/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file founder_gui.cpp Docking for the Founder Mode company panel: Team, Office and Work share one spot on the right. */

#include "stdafx.h"
#include "founder_gui.h"
#include "gfx_func.h"
#include "office_gui.h"
#include "roadmap_gui.h"
#include "team_gui.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "safeguards.h"

/**
 * Size of the docked panel: a fixed width on the right, the full height between toolbar and status bar.
 * @return Panel size in pixels.
 */
Dimension GetFounderPanelSize()
{
	uint width = std::min<uint>(ScaleGUITrad(420), _screen.width * 2 / 5);
	uint height = std::max(0, GetMainViewBottom() - GetMainViewTop());
	return { width, height };
}

/**
 * Top-left corner of the docked panel.
 * @param width Actual width of the window being placed.
 * @return Position against the right edge, just below the toolbar.
 */
Point GetFounderPanelPosition(int width)
{
	return { _screen.width - width, GetMainViewTop() };
}

/**
 * Close the panel tabs other than the one about to be shown, so tabs replace each other in place.
 * @param keep Window class of the tab being shown.
 * @param company Company whose panel it is.
 */
void CloseOtherFounderTabs(WindowClass keep, CompanyID company)
{
	for (WindowClass wc : { WindowClass::Team, WindowClass::Office, WindowClass::Roadmap }) {
		if (wc != keep) CloseWindowById(wc, company);
	}
}

/**
 * Show a tab of the company panel.
 * @param tab Tab to show.
 * @param company Company whose panel it is.
 */
void ShowFounderTab(FounderTab tab, CompanyID company)
{
	switch (tab) {
		case FounderTab::Team: ShowTeamWindow(company); break;
		case FounderTab::Office: ShowOfficeWindow(company); break;
		case FounderTab::Work: ShowRoadmapWindow(company); break;
	}
}
