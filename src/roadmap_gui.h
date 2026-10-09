/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file roadmap_gui.h Founder Mode roadmap window. */

#ifndef ROADMAP_GUI_H
#define ROADMAP_GUI_H

#include "company_type.h"
#include "town_type.h"

void ShowRoadmapWindow(CompanyID company);
std::string GetWorkImpactText(uint8_t spec, TownID town);
std::string GetWorkItemDetails(uint8_t spec, TownID town);

#endif /* ROADMAP_GUI_H */
