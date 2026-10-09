/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file market_widget.h Types related to the Founder Mode market window. */

#ifndef WIDGETS_MARKET_WIDGET_H
#define WIDGETS_MARKET_WIDGET_H

/** Widgets of the #MarketWindow class. */
enum MarketWidgets : WidgetID {
	WID_MK_CAPTION, ///< Caption.
	WID_MK_LIST, ///< Towns.
	WID_MK_SCROLLBAR, ///< Scrollbar of the list.
	WID_MK_SUMMARY, ///< Customers, MRR and price.
	WID_MK_ADD_REP, ///< Send a free rep to the selected town.
	WID_MK_REMOVE_REP, ///< Take a rep off the selected town.
	WID_MK_SHOW, ///< Scroll the map to the selected town.
	WID_MK_HUB, ///< Open or close a sales hub in the selected town.
	WID_MK_SPONSOR, ///< Sponsor a transit operator (dropdown).
};

#endif /* WIDGETS_MARKET_WIDGET_H */
