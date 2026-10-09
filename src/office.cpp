/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file office.cpp Founder Mode office: sizes, desks, rent and upgrades. */

#include "stdafx.h"
#include "office_func.h"
#include "office_cmd.h"
#include "company_base.h"
#include "command_func.h"
#include "settings_type.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Per-level office data. */
struct OfficeSpec {
	uint desks; ///< Number of desks, the headcount limit.
	Money rent; ///< Monthly rent.
	Money upgrade_cost; ///< One-off cost of moving to this level.
};

static const OfficeSpec _office_specs[MAX_OFFICE_LEVEL + 1] = {
	{  6,   500,      0 }, // Garage
	{ 15,  2500,  25000 }, // Loft
	{ 40,  8000,  90000 }, // Office floor
};

uint GetOfficeDesks(uint8_t level) { return _office_specs[std::min(level, MAX_OFFICE_LEVEL)].desks; }
Money GetOfficeRent(uint8_t level) { return _office_specs[std::min(level, MAX_OFFICE_LEVEL)].rent; }

/**
 * Cost of upgrading from a level to the next one.
 * @param level Current level.
 * @return Cost, or 0 when already at the top level.
 */
Money GetOfficeUpgradeCost(uint8_t level)
{
	return level < MAX_OFFICE_LEVEL ? _office_specs[level + 1].upgrade_cost : Money(0);
}

/**
 * Get the office level of a company.
 * @param company The company.
 * @return Office level, 0 for an invalid company.
 */
uint8_t GetOfficeLevel(CompanyID company)
{
	const Company *c = Company::GetIfValid(company);
	return c == nullptr ? 0 : c->office_level;
}

/**
 * Move the company to the next office size.
 * @param flags Type of operation.
 * @param target_level The level to move to; must be one above the current level.
 * @return The cost of this operation or an error.
 */
CommandCost CmdUpgradeOffice(DoCommandFlags flags, uint8_t target_level)
{
	if (!_settings_game.game_creation.founder_mode) return CommandCost(STR_ERROR_FOUNDER_MODE_ONLY);
	Company *c = Company::GetIfValid(_current_company);
	if (c == nullptr) return CMD_ERROR;
	if (c->office_level >= MAX_OFFICE_LEVEL) return CommandCost(STR_ERROR_OFFICE_AT_MAX);
	if (target_level != c->office_level + 1) return CMD_ERROR;

	CommandCost cost(ExpensesType::Construction, GetOfficeUpgradeCost(c->office_level));

	if (flags.Test(DoCommandFlag::Execute)) {
		c->office_level++;
		InvalidateWindowData(WindowClass::Office, c->index);
		InvalidateWindowData(WindowClass::Team, c->index);
	}

	return cost;
}
