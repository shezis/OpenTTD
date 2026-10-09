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

	Employee(EmployeeID index, CompanyID company = CompanyID::Invalid(), EmployeeRole role = EmployeeRole::Engineer) :
		EmployeePool::PoolItem<&_employee_pool>(index), company(company), role(role) {}

	~Employee() { }

	std::string GetName() const;
};

uint CountEmployees(CompanyID company);
Money GetMonthlyPayroll(CompanyID company);
void PayEmployees();
void ChangeEmployeeOwnership(CompanyID old_owner, CompanyID new_owner);
void ApplyFounderBackground(CompanyID company);

#endif /* EMPLOYEE_BASE_H */
