/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file employee_type.h Basic types related to Founder Mode employees. */

#ifndef EMPLOYEE_TYPE_H
#define EMPLOYEE_TYPE_H

#include "core/pool_type.hpp"

using EmployeeID = PoolID<uint16_t, struct EmployeeIDTag, 64000, 0xFFFF>;

/** What an employee does at the startup. */
enum class EmployeeRole : uint8_t {
	Engineer, ///< Builds features.
	Designer, ///< Improves feature quality.
	Sales, ///< Wins customers.
	Operations, ///< Keeps the company running.
	End, ///< End marker.
};

struct Employee;

static constexpr uint MAX_EMPLOYEES_PER_COMPANY = 100; ///< Hard cap until offices add desk limits.

#endif /* EMPLOYEE_TYPE_H */
