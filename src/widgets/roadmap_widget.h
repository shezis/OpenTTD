/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file roadmap_widget.h Types related to the Founder Mode roadmap window. */

#ifndef WIDGETS_ROADMAP_WIDGET_H
#define WIDGETS_ROADMAP_WIDGET_H

/** Widgets of the #RoadmapWindow class. */
enum RoadmapWidgets : WidgetID {
	WID_RM_CAPTION, ///< Caption of the window.
	WID_RM_LIST, ///< List of features.
	WID_RM_SCROLLBAR, ///< Scrollbar of the list.
	WID_RM_SUMMARY, ///< Engineers and velocity line.
	WID_RM_NEW, ///< Add a feature (dropdown of categories).
	WID_RM_ADD_ENGINEER, ///< Put one more engineer on the selected feature.
	WID_RM_REMOVE_ENGINEER, ///< Take one engineer off the selected feature.
	WID_RM_SHIP, ///< Ship the selected feature now.
};

#endif /* WIDGETS_ROADMAP_WIDGET_H */
