/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file founder_widget.h Types related to the Founder Mode start and setup windows. */

#ifndef WIDGETS_FOUNDER_WIDGET_H
#define WIDGETS_FOUNDER_WIDGET_H

/** Widgets of the #FounderStartWindow class (pick your HQ town). */
enum FounderStartWidgets : WidgetID {
	WID_FS_CAPTION, ///< Caption.
	WID_FS_INTRO, ///< Explanation text.
	WID_FS_LIST, ///< Towns by population.
	WID_FS_SCROLLBAR, ///< Scrollbar of the town list.
	WID_FS_BUILD, ///< Open the HQ in the selected town.
};

/** Widgets of the #FounderSetupWindow class (found your startup). */
enum FounderSetupWidgets : WidgetID {
	WID_FSU_BG_ENGINEER, ///< Background: engineer.
	WID_FSU_BG_SELLER, ///< Background: seller.
	WID_FSU_BG_OPERATOR, ///< Background: operator.
	WID_FSU_SC_SANDBOX, ///< Scenario: sandbox.
	WID_FSU_SC_GOLIATH, ///< Scenario: David vs Goliath.
	WID_FSU_SC_TUTORIAL, ///< Scenario: tutorial.
	WID_FSU_MAP, ///< Map settings.
	WID_FSU_START, ///< Found the company.
};

#endif /* WIDGETS_FOUNDER_WIDGET_H */
