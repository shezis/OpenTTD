/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file office_widget.h Types related to the Founder Mode office window. */

#ifndef WIDGETS_OFFICE_WIDGET_H
#define WIDGETS_OFFICE_WIDGET_H

/** Widgets of the #OfficeWindow class. */
enum OfficeWidgets : WidgetID {
	WID_OFFICE_CAPTION, ///< Caption of the window.
	WID_OFFICE_VIEW, ///< Isometric drawing of the office.
	WID_OFFICE_SUMMARY, ///< Office size, desks and rent.
	WID_OFFICE_UPGRADE, ///< Move to a bigger office.
	WID_OFFICE_TEAM, ///< Open the team window.
};

#endif /* WIDGETS_OFFICE_WIDGET_H */
