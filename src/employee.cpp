/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file employee.cpp Founder Mode employees: hiring, letting go and payroll. */

#include "stdafx.h"
#include "employee_base.h"
#include "employee_cmd.h"
#include "company_base.h"
#include "company_func.h"
#include "command_func.h"
#include "core/pool_func.hpp"
#include "core/random_func.hpp"
#include "debug.h"
#include "office_func.h"
#include "feature_base.h"
#include "market_func.h"
#include "town.h"
#include "settings_type.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

EmployeePool _employee_pool("Employee");
INSTANTIATE_POOL_METHODS(Employee)

static const std::string_view _first_names[] = {
	"Priya", "Marco", "Lena", "Sam", "Ada", "Tom", "Mei", "Jonas", "Amara", "Diego", "Noor", "Felix",
	"Sofia", "Kwame", "Ingrid", "Ravi", "Chloe", "Mateo", "Yuki", "Omar", "Hannah", "Luca", "Zara", "Ethan",
};

static const std::string_view _last_names[] = {
	"Shah", "Ruiz", "Ko", "Okafor", "Byrne", "Lind", "Chen", "Weber", "Mensah", "Silva", "Haddad", "Novak",
	"Rossi", "Boateng", "Larsen", "Iyer", "Martin", "Lopez", "Tanaka", "Farouk", "Schmidt", "Bianchi", "Khan", "Walsh",
};

static constexpr uint NUM_EMPLOYEE_NAMES = std::size(_first_names) * std::size(_last_names);

/**
 * Get the display name of this employee.
 * @return First and last name.
 */
std::string Employee::GetName() const
{
	uint first = this->name_index % std::size(_first_names);
	uint last = (this->name_index / std::size(_first_names)) % std::size(_last_names);
	return fmt::format("{} {}", _first_names[first], _last_names[last]);
}

/**
 * Typical monthly salary for a role, before skill is applied.
 * @param role The role.
 * @return Base monthly salary.
 */
static Money GetBaseSalary(EmployeeRole role)
{
	switch (role) {
		case EmployeeRole::Engineer: return 6000;
		case EmployeeRole::Designer: return 5000;
		case EmployeeRole::Sales: return 4500;
		case EmployeeRole::Operations: return 4000;
		default: NOT_REACHED();
	}
}

/**
 * Count the employees of a company.
 * @param company The company.
 * @return Number of employees.
 */
uint CountEmployees(CompanyID company)
{
	uint count = 0;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company == company) count++;
	}
	return count;
}

/**
 * Sum the monthly salaries of a company's employees.
 * @param company The company.
 * @return Total monthly payroll.
 */
Money GetMonthlyPayroll(CompanyID company)
{
	Money total = 0;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company == company) total += e->salary;
	}
	return total;
}

/** Charge every company its monthly payroll and office rent. Called from the monthly company loop. */
void PayEmployees()
{
	if (!_settings_game.game_creation.founder_mode) return;

	for (const Company *c : Company::Iterate()) {
		SubtractMoneyFromCompany(c->index, CommandCost(ExpensesType::Property, GetOfficeRent(c->office_level) + HUB_RENT * CountHubs(c->index)));

		Money payroll = GetMonthlyPayroll(c->index);
		if (payroll == 0) continue;

		SubtractMoneyFromCompany(c->index, CommandCost(ExpensesType::Other, payroll));
		Debug(Facility::Misc, Severity::Info, "Founder Mode: company {} paid payroll {}, cash now {}", c->index + 1, payroll, c->money);
		InvalidateWindowData(WindowClass::Team, c->index);
	}
}

/**
 * Move or remove employees when a company is taken over or closed.
 * @param old_owner The company that is going away.
 * @param new_owner The company taking over, or #INVALID_OWNER when the company closes.
 */
void ChangeEmployeeOwnership(CompanyID old_owner, CompanyID new_owner)
{
	for (Employee *e : Employee::Iterate()) {
		if (e->company != old_owner) continue;

		if (new_owner == INVALID_OWNER) {
			delete e;
		} else {
			e->company = new_owner;
		}
	}
	InvalidateWindowData(WindowClass::Team, old_owner);
	if (new_owner != INVALID_OWNER) InvalidateWindowData(WindowClass::Team, new_owner);
}

/**
 * Create an employee with random name, skill and morale. Must run in a synced context (command or game loop).
 * @param company Employer.
 * @param role Role of the new employee.
 * @return The new employee, or nullptr when the pool is full.
 */
static Employee *CreateEmployee(CompanyID company, EmployeeRole role)
{
	if (!Employee::CanAllocateItem()) return nullptr;
	Employee *e = Employee::Create(company, role);
	e->name_index = RandomRange(NUM_EMPLOYEE_NAMES);
	e->skill = 30 + RandomRange(61);
	e->morale = 60 + RandomRange(31);
	e->salary = GetBaseSalary(role) * (50 + e->skill) / 100;
	return e;
}

/**
 * Give a new company its starting team from the founder background setting.
 * Applies to every company, AI rivals included, so everyone plays by the same rules.
 * @param company The new company.
 */
void ApplyFounderBackground(CompanyID company)
{
	switch (_settings_game.game_creation.founder_background) {
		default:
		case 0: // Engineer.
			CreateEmployee(company, EmployeeRole::Engineer);
			CreateEmployee(company, EmployeeRole::Engineer);
			break;
		case 1: // Seller.
			CreateEmployee(company, EmployeeRole::Sales);
			CreateEmployee(company, EmployeeRole::Engineer);
			break;
		case 2: // Operator.
			CreateEmployee(company, EmployeeRole::Operations);
			GrantShippedWorkItem(company, FOUNDER_OPERATOR_HEAD_START);
			break;
	}
}

/**
 * Hire a new employee. The recruiting fee is one month of the base salary.
 * @param flags Type of operation.
 * @param role Role of the new employee.
 * @return The cost of this operation or an error.
 */
CommandCost CmdHireEmployee(DoCommandFlags flags, EmployeeRole role)
{
	if (!_settings_game.game_creation.founder_mode) return CommandCost(STR_ERROR_FOUNDER_MODE_ONLY);
	if (role >= EmployeeRole::End) return CMD_ERROR;
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;
	if (!Employee::CanAllocateItem() || CountEmployees(_current_company) >= MAX_EMPLOYEES_PER_COMPANY) return CommandCost(STR_ERROR_TEAM_FULL);
	if (CountEmployees(_current_company) >= GetOfficeDesks(GetOfficeLevel(_current_company))) return CommandCost(STR_ERROR_NO_FREE_DESK);

	Money base = GetBaseSalary(role);

	if (flags.Test(DoCommandFlag::Execute)) {
		CreateEmployee(_current_company, role);
		InvalidateWindowData(WindowClass::Team, _current_company);
		InvalidateWindowData(WindowClass::Office, _current_company);
	}

	return CommandCost(ExpensesType::Other, base);
}

/**
 * Let an employee go. Severance is one month of their salary.
 * @param flags Type of operation.
 * @param employee The employee to let go.
 * @return The cost of this operation or an error.
 */
CommandCost CmdFireEmployee(DoCommandFlags flags, EmployeeID employee)
{
	Employee *e = Employee::GetIfValid(employee);
	if (e == nullptr || e->company != _current_company) return CMD_ERROR;

	CommandCost cost(ExpensesType::Other, e->salary);

	if (flags.Test(DoCommandFlag::Execute)) {
		CompanyID company = e->company;
		delete e;
		InvalidateWindowData(WindowClass::Team, company);
		InvalidateWindowData(WindowClass::Office, company);
	}

	return cost;
}

/**
 * Assign a sales rep to a town, or take them off it.
 * @param flags Type of operation.
 * @param employee The sales rep.
 * @param town Town to work, or TownID::Invalid() to unassign.
 * @return The cost of this operation or an error.
 */
CommandCost CmdAssignRep(DoCommandFlags flags, EmployeeID employee, TownID town)
{
	Employee *e = Employee::GetIfValid(employee);
	if (e == nullptr || e->company != _current_company) return CMD_ERROR;
	if (e->role != EmployeeRole::Sales) return CommandCost(STR_ERROR_NOT_A_SALES_REP);
	if (town != TownID::Invalid()) {
		if (!Town::IsValidID(town)) return CMD_ERROR;
		if (!IsTownInRepRange(_current_company, town)) return CommandCost(STR_ERROR_TOWN_OUT_OF_REP_RANGE);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		e->town = town;
		InvalidateWindowData(WindowClass::Team, e->company);
		InvalidateWindowData(WindowClass::Market, e->company);
	}

	return CommandCost();
}

/**
 * Open or close a sales hub in a town. Closing sends the town's reps home if it leaves them out of range.
 * @param flags Type of operation.
 * @param town The town.
 * @param open True to open, false to close.
 * @return The cost of this operation or an error.
 */
CommandCost CmdSetHub(DoCommandFlags flags, TownID town, bool open)
{
	if (!_settings_game.game_creation.founder_mode) return CommandCost(STR_ERROR_FOUNDER_MODE_ONLY);
	Town *t = Town::GetIfValid(town);
	const Company *c = Company::GetIfValid(_current_company);
	if (t == nullptr || c == nullptr) return CMD_ERROR;

	if (open) {
		if (c->location_of_HQ == INVALID_TILE) return CommandCost(STR_ERROR_HUB_NEEDS_HQ);
		if (t->founder_hubs.Test(_current_company)) return CommandCost(STR_ERROR_HUB_ALREADY_OPEN);
		if (GetCompanyHQTown(_current_company) == town) return CommandCost(STR_ERROR_HUB_IN_HQ_TOWN);
		if (CountHubs(_current_company) >= MAX_HUBS_PER_COMPANY) return CommandCost(STR_ERROR_TOO_MANY_HUBS);
		if (flags.Test(DoCommandFlag::Execute)) t->founder_hubs.Set(_current_company);
	} else {
		if (!t->founder_hubs.Test(_current_company)) return CMD_ERROR;
		if (flags.Test(DoCommandFlag::Execute)) {
			t->founder_hubs.Reset(_current_company);
			for (Employee *e : Employee::Iterate()) {
				if (e->company == _current_company && Town::IsValidID(e->town) && !IsTownInRepRange(_current_company, e->town)) e->town = TownID::Invalid();
			}
		}
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		InvalidateWindowData(WindowClass::Market, _current_company);
		InvalidateWindowData(WindowClass::Team, _current_company);
	}
	return CommandCost(ExpensesType::Construction, open ? HUB_OPEN_COST : Money(0));
}

/**
 * Sponsor a transit operator, change the amount, or end the sponsorship (amount 0).
 * Each operator has at most one sponsor; a startup sponsors at most one operator.
 * @param flags Type of operation.
 * @param op The transit operator.
 * @param monthly Monthly amount, one of #SPONSOR_TIERS, or 0 to end.
 * @return The cost of this operation or an error.
 */
CommandCost CmdSponsorOperator(DoCommandFlags flags, CompanyID op, Money monthly)
{
	if (!_settings_game.game_creation.founder_mode) return CommandCost(STR_ERROR_FOUNDER_MODE_ONLY);
	Company *c = Company::GetIfValid(_current_company);
	if (c == nullptr || IsFounderOperator(c->index)) return CMD_ERROR;

	if (monthly == 0) {
		if (!Company::IsValidID(c->founder_sponsoring)) return CMD_ERROR;
	} else {
		if (std::ranges::find(SPONSOR_TIERS, monthly) == SPONSOR_TIERS.end()) return CMD_ERROR;
		if (!IsFounderOperator(op)) return CMD_ERROR;
		CompanyID current = GetOperatorSponsor(op);
		if (current != CompanyID::Invalid() && current != c->index) return CommandCost(STR_ERROR_OPERATOR_TAKEN);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		CompanyID previous = c->founder_sponsoring;
		c->founder_sponsoring = monthly == 0 ? CompanyID::Invalid() : op;
		c->founder_sponsor_monthly = monthly;
		if (Company::IsValidID(previous)) ApplyOperatorLivery(previous);
		if (monthly != 0) ApplyOperatorLivery(op);
		InvalidateWindowData(WindowClass::Market, c->index);
	}
	return CommandCost();
}
