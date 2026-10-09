/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file board_widget.h Types related to the Founder Mode board window. */

#ifndef WIDGETS_BOARD_WIDGET_H
#define WIDGETS_BOARD_WIDGET_H

/** Widgets of the #BoardWindow class. */
enum BoardWidgets : WidgetID {
	WID_BD_CAPTION, ///< Caption.
	WID_BD_REPORT, ///< This month's numbers.
	WID_BD_CAPTABLE, ///< Ownership.
	WID_BD_OFFER, ///< Pending offer or next milestone.
	WID_BD_ACCEPT, ///< Accept the offer.
	WID_BD_DECLINE, ///< Decline the offer.
	WID_BD_FINANCES, ///< Open the finances window.
	WID_BD_BUYBACK, ///< Buy back equity from investors.
};

#endif /* WIDGETS_BOARD_WIDGET_H */
