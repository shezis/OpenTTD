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
#include "economy_func.h"
#include "textbuf_gui.h"
#include "strings_func.h"
#include "news_func.h"

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
 * Monthly salary of a mid-level person in a role.
 * @param role The role.
 * @return Base monthly salary.
 */
static Money GetBaseSalary(EmployeeRole role)
{
	switch (role) {
		case EmployeeRole::Engineer: return 5000;
		case EmployeeRole::Designer: return 4200;
		case EmployeeRole::Sales: return 3800;
		case EmployeeRole::Operations: return 3400;
		default: NOT_REACHED();
	}
}

/** Per level: salary in percent of the base, lowest skill, skill range. */
struct LevelSpec {
	uint salary_percent;
	uint8_t skill_min;
	uint8_t skill_range;
};
static constexpr LevelSpec _level_specs[] = {
	{  55, 25, 20 }, // Junior: skill 25-44.
	{ 100, 45, 25 }, // Mid: skill 45-69.
	{ 170, 70, 25 }, // Senior: skill 70-94.
};
static_assert(std::size(_level_specs) == to_underlying(EmployeeLevel::End));

/**
 * Monthly salary for a role at a level. The recruiting fee is one month of it.
 * @param role The role.
 * @param level The level.
 * @return Monthly salary.
 */
Money GetLevelSalary(EmployeeRole role, EmployeeLevel level)
{
	return GetBaseSalary(role) * _level_specs[to_underlying(level)].salary_percent / 100;
}

/**
 * Typical speed of a level, relative to a mid-level person.
 * @param level The level.
 * @return Speed in percent.
 */
uint GetLevelSpeedPercent(EmployeeLevel level)
{
	const LevelSpec &mid = _level_specs[to_underlying(EmployeeLevel::Mid)];
	const LevelSpec &l = _level_specs[to_underlying(level)];
	return (l.skill_min * 2 + l.skill_range) * 100 / (mid.skill_min * 2 + mid.skill_range);
}

/**
 * Is this person free: not on a work item and, for sales, not working a town?
 * @param e The employee.
 * @return True when free.
 */
bool IsEmployeeFree(const Employee *e)
{
	return e->feature == FeatureID::Invalid() && !Town::IsValidID(e->town);
}

/**
 * Put a person on a work item or take them off, keeping the item's head count.
 * @param e The employee.
 * @param feature The item, or FeatureID::Invalid() to free them.
 */
void SetEmployeeWork(Employee *e, FeatureID feature)
{
	if (Feature *old = Feature::GetIfValid(e->feature); old != nullptr && old->assigned > 0) old->assigned--;
	e->feature = feature;
	if (Feature *f = Feature::GetIfValid(feature); f != nullptr) {
		f->assigned++;
		if (f->state == FeatureState::Backlog) f->state = FeatureState::InProgress;
	}
	InvalidateWindowData(WindowClass::Team, e->company);
	InvalidateWindowData(WindowClass::Roadmap, e->company);
}

/**
 * Free everyone working on an item, e.g. when it ships.
 * @param feature The item.
 */
void ReleaseFeatureStaff(FeatureID feature)
{
	for (Employee *e : Employee::Iterate()) {
		if (e->feature == feature) SetEmployeeWork(e, FeatureID::Invalid());
	}
}

/**
 * Sales reps working a town.
 * @param company The company.
 * @return Reps in the field.
 */
uint CountFieldReps(CompanyID company)
{
	uint n = 0;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company == company && e->role == EmployeeRole::Sales && Town::IsValidID(e->town)) n++;
	}
	return n;
}

/**
 * Monthly travel and tools for the reps working towns.
 * @param company The company.
 * @return Field sales costs.
 */
Money GetFieldSalesCosts(CompanyID company)
{
	return REP_FIELD_COST * CountFieldReps(company);
}

/**
 * Salary a poaching offer pays: the current salary plus a raise, rounded up to whole hundreds.
 * @param e The person.
 * @return Offered salary.
 */
Money GetPoachSalary(const Employee *e)
{
	return CeilDiv(static_cast<int64_t>(e->salary) * (100 + POACH_RAISE_PERCENT) / 100, 100) * 100;
}

/**
 * Does a company already have an offer out to someone? One at a time.
 * @param company The company.
 * @return True with an open offer.
 */
bool HasOpenPoachOffer(CompanyID company)
{
	for (const Employee *e : Employee::Iterate()) {
		if (e->poach_by == company) return true;
	}
	return false;
}

/**
 * The person takes the job: they move to the poacher at the offered salary, if it still has a desk.
 * The poacher pays one month of the new salary as a signing fee.
 * @param e The person.
 */
static void TransferPoachedEmployee(Employee *e)
{
	CompanyID from = e->company;
	CompanyID to = e->poach_by;
	Money salary = e->poach_salary;
	e->poach_by = CompanyID::Invalid();
	e->poach_months = 0;
	if (!Company::IsValidID(to) || CountEmployees(to) >= GetOfficeDesks(GetOfficeLevel(to))) return;

	SetEmployeeWork(e, FeatureID::Invalid());
	e->town = TownID::Invalid();
	e->company = to;
	e->salary = salary;
	e->morale = static_cast<uint8_t>(std::min(100, e->morale + 10));
	SubtractMoneyFromCompany(to, CommandCost(ExpensesType::Construction, salary));
	Debug(Facility::Misc, Severity::Info, "Founder Mode: company {} poached {} from company {}", to + 1, e->GetName(), from + 1);

	if (from == _local_company) AddNewsItem(GetEncodedString(STR_NEWS_FOUNDER_POACHED_FROM_YOU, e->GetName(), to), NewsType::CompanyInfo, NewsStyle::Small, {});
	if (to == _local_company) AddNewsItem(GetEncodedString(STR_NEWS_FOUNDER_POACHED_BY_YOU, e->GetName(), from), NewsType::CompanyInfo, NewsStyle::Small, {});
	for (CompanyID c : {from, to}) {
		InvalidateWindowData(WindowClass::Team, c);
		InvalidateWindowData(WindowClass::Office, c);
		InvalidateWindowData(WindowClass::Market, c);
	}
}

/** Savegames before staff levels: give everyone a level from their skill and put people on the items that had head counts. */
void AfterLoadEmployeeLevels()
{
	for (Employee *e : Employee::Iterate()) {
		e->level = e->skill < 45 ? EmployeeLevel::Junior : (e->skill < 70 ? EmployeeLevel::Mid : EmployeeLevel::Senior);
		e->feature = FeatureID::Invalid();
	}
	for (Feature *f : Feature::Iterate()) {
		uint wanted = f->state == FeatureState::Shipped ? 0 : f->assigned;
		f->assigned = 0;
		for (Employee *e : Employee::Iterate()) {
			if (f->assigned >= wanted) break;
			if (e->company == f->company && e->role == GetTrackRole(f->GetTrack()) && IsEmployeeFree(e)) {
				e->feature = f->index;
				f->assigned++;
			}
		}
	}
}

static void TransferPoachedEmployee(Employee *e);

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

	/* Poaching offers nobody matched: the person leaves. */
	for (Employee *e : Employee::Iterate()) {
		if (e->poach_by == CompanyID::Invalid() || --e->poach_months > 0) continue;
		TransferPoachedEmployee(e);
	}

	for (const Company *c : Company::Iterate()) {
		if (IsFounderOperator(c->index)) continue;
		SubtractMoneyFromCompany(c->index, CommandCost(ExpensesType::Property, GetOfficeRent(c->office_level) + HUB_RENT * CountHubs(c->index)));
		Money field = GetFieldSalesCosts(c->index);
		if (field > 0) SubtractMoneyFromCompany(c->index, CommandCost(ExpensesType::RoadVehRun, field));
		Money running = GetWorkRunCosts(c->index);
		if (running > 0) SubtractMoneyFromCompany(c->index, CommandCost(ExpensesType::AircraftRun, running));

		Money payroll = GetMonthlyPayroll(c->index);
		if (payroll == 0) continue;

		SubtractMoneyFromCompany(c->index, CommandCost(ExpensesType::TrainRun, payroll));
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
			e->feature = FeatureID::Invalid();
		}
	}
	/* Offers made by the company that is going away lapse. */
	for (Employee *e : Employee::Iterate()) {
		if (e->poach_by == old_owner) e->poach_by = CompanyID::Invalid();
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
static Employee *CreateEmployee(CompanyID company, EmployeeRole role, EmployeeLevel level = EmployeeLevel::Mid)
{
	if (!Employee::CanAllocateItem()) return nullptr;
	const LevelSpec &spec = _level_specs[to_underlying(level)];
	Employee *e = Employee::Create(company, role);
	e->level = level;
	e->name_index = RandomRange(NUM_EMPLOYEE_NAMES);
	e->skill = spec.skill_min + RandomRange(spec.skill_range);
	e->morale = 60 + RandomRange(31);
	e->salary = GetLevelSalary(role, level);
	return e;
}

/**
 * Give a new startup its starting team. Rival startups pick a background like the player does.
 * @param company The new company.
 * @param background 0 engineer, 1 seller, 2 operator.
 */
void ApplyFounderBackground(CompanyID company, uint8_t background)
{
	switch (background) {
		default:
		case 0: // Engineer.
			CreateEmployee(company, EmployeeRole::Engineer);
			CreateEmployee(company, EmployeeRole::Engineer, EmployeeLevel::Junior);
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
CommandCost CmdHireEmployee(DoCommandFlags flags, EmployeeRole role, EmployeeLevel level)
{
	if (!_settings_game.game_creation.founder_mode) return CommandCost(STR_ERROR_FOUNDER_MODE_ONLY);
	if (role >= EmployeeRole::End || level >= EmployeeLevel::End) return CMD_ERROR;
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;
	if (!Employee::CanAllocateItem() || CountEmployees(_current_company) >= MAX_EMPLOYEES_PER_COMPANY) return CommandCost(STR_ERROR_TEAM_FULL);
	if (CountEmployees(_current_company) >= GetOfficeDesks(GetOfficeLevel(_current_company))) return CommandCost(STR_ERROR_NO_FREE_DESK);

	Money fee = GetLevelSalary(role, level);

	if (flags.Test(DoCommandFlag::Execute)) {
		CreateEmployee(_current_company, role, level);
		InvalidateWindowData(WindowClass::Team, _current_company);
		InvalidateWindowData(WindowClass::Office, _current_company);
	}

	return CommandCost(ExpensesType::Construction, fee);
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

	CommandCost cost(ExpensesType::Construction, e->salary);

	if (flags.Test(DoCommandFlag::Execute)) {
		CompanyID company = e->company;
		SetEmployeeWork(e, FeatureID::Invalid());
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
		/* Working a town and working an item are either-or. */
		if (town != TownID::Invalid()) SetEmployeeWork(e, FeatureID::Invalid());
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

/**
 * Put a person on a work item of their track, or take them off.
 * A salesperson working a town leaves it to take on sales work.
 * @param flags Type of operation.
 * @param employee The person.
 * @param feature The item, or FeatureID::Invalid() to free them.
 * @return The cost of this operation or an error.
 */
CommandCost CmdAssignWork(DoCommandFlags flags, EmployeeID employee, FeatureID feature)
{
	Employee *e = Employee::GetIfValid(employee);
	if (e == nullptr || e->company != _current_company) return CMD_ERROR;
	if (feature != FeatureID::Invalid()) {
		const Feature *f = Feature::GetIfValid(feature);
		if (f == nullptr || f->company != _current_company) return CMD_ERROR;
		if (f->state == FeatureState::Shipped) return CommandCost(STR_ERROR_FEATURE_SHIPPED);
		if (GetTrackRole(f->GetTrack()) != e->role) return CommandCost(STR_ERROR_WRONG_ROLE_FOR_WORK);
		if (e->feature == feature) return CommandCost();
		if (f->assigned >= GetWorkItemSlots(f->spec)) return CommandCost(STR_ERROR_WORK_ITEM_FULL);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		if (feature != FeatureID::Invalid()) e->town = TownID::Invalid();
		SetEmployeeWork(e, feature);
		InvalidateWindowData(WindowClass::Market, e->company);
	}
	return CommandCost();
}

/** Employee a poaching prompt is about, for the query callback. */
static EmployeeID _poach_prompt;

/** The founder answered the poaching prompt. */
static void PoachPromptCallback(Window *, bool match)
{
	if (Employee::IsValidID(_poach_prompt)) Command<Commands::RespondPoachOffer>::Post(STR_ERROR_CAN_T_RESPOND_POACH, _poach_prompt, match);
}

/**
 * Offer another startup's employee a job at a raise. The employer can match it within a month.
 * @param flags Type of operation.
 * @param employee The person.
 * @return The cost of this operation or an error.
 */
CommandCost CmdPoachEmployee(DoCommandFlags flags, EmployeeID employee)
{
	if (!_settings_game.game_creation.founder_mode) return CommandCost(STR_ERROR_FOUNDER_MODE_ONLY);
	Employee *e = Employee::GetIfValid(employee);
	if (e == nullptr || !Company::IsValidID(_current_company) || e->company == _current_company) return CMD_ERROR;
	if (IsFounderOperator(_current_company) || IsFounderOperator(e->company)) return CMD_ERROR;
	if (e->poach_by != CompanyID::Invalid()) return CommandCost(STR_ERROR_ALREADY_HAS_OFFER);
	if (HasOpenPoachOffer(_current_company)) return CommandCost(STR_ERROR_ONE_OFFER_AT_A_TIME);
	if (CountEmployees(_current_company) >= GetOfficeDesks(GetOfficeLevel(_current_company))) return CommandCost(STR_ERROR_NO_FREE_DESK);
	Money salary = GetPoachSalary(e);
	if (Company::Get(_current_company)->money < salary) return CommandCost(STR_ERROR_POACH_NO_CASH);

	if (flags.Test(DoCommandFlag::Execute)) {
		e->poach_by = _current_company;
		e->poach_salary = salary;
		e->poach_months = 2;
		InvalidateWindowData(WindowClass::Team, e->company);
		if (e->company == _local_company) {
			_poach_prompt = e->index;
			ShowQuery(GetEncodedString(STR_POACH_QUERY_CAPTION), GetEncodedString(STR_POACH_QUERY, _current_company, e->GetName(), STR_TEAM_LEVEL_JUNIOR + to_underlying(e->level), STR_TEAM_ROLE_ENGINEER + to_underlying(e->role), salary, e->salary), nullptr, PoachPromptCallback);
		}
	}
	return CommandCost();
}

/**
 * Answer a poaching offer for one of your people: match the salary to keep them, or let them go now.
 * @param flags Type of operation.
 * @param employee The person.
 * @param match True to match the offer.
 * @return The cost of this operation or an error.
 */
CommandCost CmdRespondPoachOffer(DoCommandFlags flags, EmployeeID employee, bool match)
{
	Employee *e = Employee::GetIfValid(employee);
	if (e == nullptr || e->company != _current_company || e->poach_by == CompanyID::Invalid()) return CommandCost(STR_ERROR_NO_POACH_OFFER);

	if (flags.Test(DoCommandFlag::Execute)) {
		if (match) {
			e->salary = e->poach_salary;
			e->morale = static_cast<uint8_t>(std::min(100, e->morale + 5));
			e->poach_by = CompanyID::Invalid();
			e->poach_months = 0;
			InvalidateWindowData(WindowClass::Team, e->company);
		} else {
			TransferPoachedEmployee(e);
		}
	}
	return CommandCost();
}

/**
 * David vs Goliath: the incumbent starts as an established company.
 * An office floor of mostly mid and senior staff, a mature product, seed and series A raised, and cash in the bank.
 * @param c The incumbent.
 */
void ApplyIncumbentHeadStart(Company *c)
{
	c->office_level = 2;
	static constexpr std::pair<EmployeeRole, EmployeeLevel> team[] = {
		{EmployeeRole::Engineer, EmployeeLevel::Senior}, {EmployeeRole::Engineer, EmployeeLevel::Senior}, {EmployeeRole::Engineer, EmployeeLevel::Senior},
		{EmployeeRole::Engineer, EmployeeLevel::Mid}, {EmployeeRole::Engineer, EmployeeLevel::Mid}, {EmployeeRole::Engineer, EmployeeLevel::Mid},
		{EmployeeRole::Engineer, EmployeeLevel::Mid}, {EmployeeRole::Engineer, EmployeeLevel::Junior}, {EmployeeRole::Engineer, EmployeeLevel::Junior},
		{EmployeeRole::Sales, EmployeeLevel::Senior}, {EmployeeRole::Sales, EmployeeLevel::Senior}, {EmployeeRole::Sales, EmployeeLevel::Mid},
		{EmployeeRole::Sales, EmployeeLevel::Mid}, {EmployeeRole::Sales, EmployeeLevel::Mid}, {EmployeeRole::Sales, EmployeeLevel::Mid},
		{EmployeeRole::Sales, EmployeeLevel::Mid}, {EmployeeRole::Sales, EmployeeLevel::Mid}, {EmployeeRole::Sales, EmployeeLevel::Junior},
		{EmployeeRole::Sales, EmployeeLevel::Junior}, {EmployeeRole::Operations, EmployeeLevel::Senior}, {EmployeeRole::Operations, EmployeeLevel::Mid},
		{EmployeeRole::Operations, EmployeeLevel::Mid}, {EmployeeRole::Designer, EmployeeLevel::Mid}, {EmployeeRole::Designer, EmployeeLevel::Mid},
	};
	for (const auto &[role, level] : team) CreateEmployee(c->index, role, level);

	/* A mature product, built the slow, sales-led way. */
	static constexpr uint8_t shipped[] = { 0, 1, 2, 3, 4, 5, 6, 9, 13, 14, 15, 16, 17, 18, 21, 22, 23, 24, 30, 38, 39 };
	for (uint8_t item : shipped) GrantShippedWorkItem(c->index, item);

	/* Seed and series A taken. */
	c->founder_stage = 2;
	c->founder_round_amount = {250000, 2000000, 0, 0};
	c->founder_round_equity = {200, 250, 0, 0};
	c->founder_round_investor = {1, 3, 0, 0};
	c->founder_equity = 600;
	c->founder_valuation = 40000000;
	c->money = 3000000;
}
