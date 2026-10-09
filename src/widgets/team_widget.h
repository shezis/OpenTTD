/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file team_widget.h Types related to the Founder Mode team window. */

#ifndef WIDGETS_TEAM_WIDGET_H
#define WIDGETS_TEAM_WIDGET_H

/** Widgets of the #TeamWindow class. */
enum TeamWidgets : WidgetID {
	WID_TEAM_CAPTION, ///< Caption of the window.
	WID_TEAM_TAB_TEAM, ///< Company panel tab: team.
	WID_TEAM_TAB_OFFICE, ///< Company panel tab: office.
	WID_TEAM_TAB_WORK, ///< Company panel tab: work.
	WID_TEAM_LIST, ///< List of employees.
	WID_TEAM_SCROLLBAR, ///< Scrollbar of the list.
	WID_TEAM_SUMMARY, ///< Headcount and payroll line.
	WID_TEAM_HIRE_ENGINEER, ///< Hire an engineer.
	WID_TEAM_HIRE_DESIGNER, ///< Hire a designer.
	WID_TEAM_HIRE_SALES, ///< Hire a salesperson.
	WID_TEAM_HIRE_OPERATIONS, ///< Hire an operations person.
	WID_TEAM_FIRE, ///< Let the selected employee go.
	WID_TEAM_OFFICE, ///< Open the office window.
};

#endif /* WIDGETS_TEAM_WIDGET_H */
