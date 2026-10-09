/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file office_func.h Founder Mode office sizes, desks and costs. */

#ifndef OFFICE_FUNC_H
#define OFFICE_FUNC_H

#include "company_type.h"
#include "economy_type.h"

static constexpr uint8_t MAX_OFFICE_LEVEL = 2; ///< Garage (0), loft (1), office floor (2).

uint GetOfficeDesks(uint8_t level);
Money GetOfficeRent(uint8_t level);
Money GetOfficeUpgradeCost(uint8_t level);
uint8_t GetOfficeLevel(CompanyID company);

#endif /* OFFICE_FUNC_H */
