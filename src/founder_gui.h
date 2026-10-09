/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file founder_gui.h Shared look and docking for the Founder Mode company panel. */

#ifndef FOUNDER_GUI_H
#define FOUNDER_GUI_H

#include "company_type.h"
#include "gfx_type.h"
#include "window_type.h"

/** Panel colour of Founder Mode windows: OpenTTD widgets, recoloured from the classic brown. */
static constexpr Colours FOUNDER_COLOUR = Colours::Grey;

/** Tabs of the docked company panel. */
enum class FounderTab : uint8_t {
	Team, ///< People.
	Office, ///< Office view.
	Work, ///< Roadmap tree and list.
};

Point GetFounderPanelPosition(int width);
Dimension GetFounderPanelSize();
void ShowFounderTab(FounderTab tab, CompanyID company);
void CloseOtherFounderTabs(WindowClass keep, CompanyID company);

#endif /* FOUNDER_GUI_H */
