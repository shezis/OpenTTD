/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rivals_widget.h Types related to the Founder Mode startup league window. */

#ifndef WIDGETS_RIVALS_WIDGET_H
#define WIDGETS_RIVALS_WIDGET_H

/** Widgets of the #StartupLeagueWindow class. */
enum StartupLeagueWidgets : WidgetID {
	WID_FML_CAPTION, ///< Caption.
	WID_FML_LIST, ///< Startups ranked by valuation.
	WID_FML_SCROLLBAR, ///< Scrollbar of the list.
	WID_FML_SUMMARY, ///< Selected startup's position and the IPO target.
	WID_FML_SHOW, ///< Scroll the map to the selected startup's HQ.
	WID_FML_BOARD, ///< Open the selected startup's board.
};

#endif /* WIDGETS_RIVALS_WIDGET_H */
