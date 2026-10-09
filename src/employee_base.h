/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file employee_base.h Founder Mode employee data. */

#ifndef EMPLOYEE_BASE_H
#define EMPLOYEE_BASE_H

#include "company_type.h"
#include "economy_type.h"
#include "employee_type.h"
#include "feature_type.h"
#include "town_type.h"
#include "core/pool_type.hpp"

using EmployeePool = Pool<Employee, EmployeeID, 64>;
extern EmployeePool _employee_pool;

/** A person working for a company in Founder Mode. */
struct Employee : EmployeePool::PoolItem<&_employee_pool> {
	CompanyID company = CompanyID::Invalid(); ///< Company the employee works for.
	EmployeeRole role = EmployeeRole::Engineer; ///< What the employee does.
	uint16_t name_index = 0; ///< Index into the name tables, see #GetEmployeeName.
	uint8_t skill = 50; ///< Skill from 1 to 100.
	uint8_t morale = 75; ///< Morale from 0 to 100.
	Money salary = 0; ///< Monthly salary.
	TownID town = TownID::Invalid(); ///< Town a sales rep works, invalid when unassigned.
	EmployeeLevel level = EmployeeLevel::Mid; ///< Seniority.
	FeatureID feature = FeatureID::Invalid(); ///< Work item this person works on, invalid when free.
	CompanyID poach_by = CompanyID::Invalid(); ///< Startup that made this person an offer, invalid when none.
	Money poach_salary = 0; ///< Salary offered by #poach_by.
	uint8_t poach_months = 0; ///< Months before the offer is taken if not matched.

	Employee(EmployeeID index, CompanyID company = CompanyID::Invalid(), EmployeeRole role = EmployeeRole::Engineer) :
		EmployeePool::PoolItem<&_employee_pool>(index), company(company), role(role) {}

	~Employee() { }

	std::string GetName() const;
};

uint CountEmployees(CompanyID company);
Money GetMonthlyPayroll(CompanyID company);
void PayEmployees();
void ChangeEmployeeOwnership(CompanyID old_owner, CompanyID new_owner);
void ApplyFounderBackground(CompanyID company, uint8_t background);
void ApplyIncumbentHeadStart(struct Company *c);
Money GetLevelSalary(EmployeeRole role, EmployeeLevel level);
uint GetLevelSpeedPercent(EmployeeLevel level);
void SetEmployeeWork(Employee *e, FeatureID feature);
void ReleaseFeatureStaff(FeatureID feature);
bool IsEmployeeFree(const Employee *e);
uint CountFieldReps(CompanyID company);
Money GetFieldSalesCosts(CompanyID company);
void AfterLoadEmployeeLevels();

static constexpr Money REP_FIELD_COST = 600; ///< Monthly travel and tools for each rep working a town.
static constexpr uint POACH_RAISE_PERCENT = 20; ///< A poaching offer pays this much more than the current salary.
Money GetPoachSalary(const Employee *e);
bool HasOpenPoachOffer(CompanyID company);

#endif /* EMPLOYEE_BASE_H */
