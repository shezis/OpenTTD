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
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "error.h"
#include "genworld.h"
#include "gui.h"
#include "object_cmd.h"
#include "object_type.h"
#include "settings_type.h"
#include "strings_func.h"
#include "tilearea_spiral.h"
#include "town.h"
#include "viewport_func.h"
#include "palette_func.h"

#include "core/backup_type.hpp"
#include "timer/timer.h"
#include "timer/timer_window.h"

#include "widgets/founder_widget.h"

#include "table/strings.h"

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

/* --- Getting started: pick the HQ town --- */

/**
 * Build the local company's HQ on the nearest free spot to a town centre, then show the team.
 * @param town Town to open the HQ in.
 * @return Whether the HQ was built.
 */
bool OpenFounderHQ(TownID town)
{
	const Town *t = Town::GetIfValid(town);
	if (t == nullptr || !Company::IsValidID(_local_company)) return false;
	for (TileIndex tile : SpiralTileSequence(t->xy, 20)) {
		/* Test as the local company, so tiles it may not build on are skipped. */
		bool ok;
		{
			AutoRestoreBackup cur_company(_current_company, _local_company);
			ok = Command<Commands::BuildObject>::Do(DoCommandFlags{DoCommandFlag::Auto, DoCommandFlag::NoWater}, tile, OBJECT_HQ, 0).Succeeded();
		}
		if (!ok) continue;
		if (!Command<Commands::BuildObject>::Post(STR_ERROR_CAN_T_BUILD_COMPANY_HEADQUARTERS, tile, OBJECT_HQ, 0)) return false;
		ScrollMainWindowToTile(tile);
		CloseWindowById(WindowClass::FounderStart, 0);
		ShowFounderTab(FounderTab::Team, _local_company);
		return true;
	}
	ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_BUILD_COMPANY_HEADQUARTERS), GetEncodedString(STR_FOUNDER_START_NO_SPACE), WarningLevel::Info);
	return false;
}

/**
 * Towns ordered biggest first, as listed in the HQ picker.
 * @return Towns by population, descending.
 */
std::vector<const Town *> GetTownsBySize()
{
	std::vector<const Town *> towns;
	for (const Town *t : Town::Iterate()) towns.push_back(t);
	std::ranges::sort(towns, std::greater{}, [](const Town *t) { return t->cache.population; });
	return towns;
}

/** Docked panel shown at the start of a Founder game until the HQ exists. */
struct FounderStartWindow : public Window {
	Scrollbar *vscroll = nullptr; ///< Scrollbar of the town list.
	TownID selected = TownID::Invalid(); ///< Selected town.

	FounderStartWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_FS_SCROLLBAR);
		this->FinishInitNested(window_number);
		this->OnInvalidateData(0);
	}

	Point OnInitialPosition([[maybe_unused]] int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		return GetFounderPanelPosition(GetFounderPanelSize().width);
	}

	void FindWindowPlacementAndResize(int, int, bool allow_resize) override
	{
		Dimension d = GetFounderPanelSize();
		Window::FindWindowPlacementAndResize(d.width, d.height, allow_resize);
	}

	static std::vector<const Town *> GetTowns() { return GetTownsBySize(); }

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_FS_INTRO:
				size.height = 4 * GetCharacterHeight(FontSize::Normal) + WidgetDimensions::scaled.framerect.Vertical();
				break;
			case WID_FS_LIST:
				resize.width = 1;
				fill.height = resize.height = GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(4);
				size.height = 6 * resize.height;
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_FS_INTRO:
				DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.framerect), STR_FOUNDER_START_INTRO, TextColour::Black);
				break;

			case WID_FS_LIST: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
				const int row_h = this->resize.step_height;
				int pos = -this->vscroll->GetPosition();
				for (const Town *t : GetTowns()) {
					if (pos >= 0 && pos < this->vscroll->GetCapacity()) {
						Rect row = ir.WithHeight(row_h);
						bool sel = t->index == this->selected;
						if (sel) GfxFillRect(row.left, row.top, row.right, row.bottom - 1, PC_DARK_GREY);
						int ty = row.top + (row_h - GetCharacterHeight(FontSize::Normal)) / 2;
						DrawString(row.left, row.right, ty, GetString(STR_FOUNDER_START_TOWN, t->index, t->cache.population), sel ? TextColour::White : TextColour::Black);
						ir.top += row_h;
					}
					pos++;
				}
				break;
			}
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FS_BUILD) {
			if (Town::IsValidID(this->selected)) return GetString(STR_FOUNDER_START_BUILD, this->selected);
			return GetString(STR_FOUNDER_START_PICK);
		}
		return this->Window::GetWidgetString(widget, stringid);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_FS_LIST: {
				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_FS_LIST, WidgetDimensions::scaled.framerect.top);
				auto towns = GetTowns();
				if (row < 0 || row >= static_cast<int>(towns.size())) break;
				this->selected = towns[row]->index;
				ScrollMainWindowToTile(towns[row]->xy);
				this->OnInvalidateData(0);
				break;
			}

			case WID_FS_BUILD:
				if (Town::IsValidID(this->selected)) OpenFounderHQ(this->selected);
				break;
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_FS_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	/** Towns grow while the player decides; keep the list and order current. */
	const IntervalTimer<TimerWindow> refresh_interval = {std::chrono::seconds(3), [this](auto) {
		this->SetWidgetDirty(WID_FS_LIST);
	}};

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->vscroll->SetCount(Town::GetNumItems());
		this->SetWidgetDisabledState(WID_FS_BUILD, !Town::IsValidID(this->selected));
		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_founder_start_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, FOUNDER_COLOUR),
		NWidget(WWT_CAPTION, FOUNDER_COLOUR, WID_FS_CAPTION), SetStringTip(STR_FOUNDER_START_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_FS_INTRO), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_FS_LIST), SetToolTip(STR_FOUNDER_START_LIST_TOOLTIP), SetScrollbar(WID_FS_SCROLLBAR), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, FOUNDER_COLOUR, WID_FS_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_FS_BUILD), SetToolTip(STR_FOUNDER_START_BUILD_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, FOUNDER_COLOUR),
	EndContainer(),
};

static WindowDesc _founder_start_desc(
	WindowPosition::Manual, {}, 0, 0,
	WindowClass::FounderStart, WindowClass::None,
	{},
	_nested_founder_start_widgets
);

/**
 * Open the "pick your HQ town" panel if the local company has no HQ yet.
 * @return Whether the panel was opened.
 */
bool ShowFounderStartIfNeeded()
{
	if (!_settings_game.game_creation.founder_mode) return false;
	const Company *c = Company::GetIfValid(_local_company);
	if (c == nullptr || c->location_of_HQ != INVALID_TILE) return false;
	CloseOtherFounderTabs(WindowClass::Invalid, _local_company);
	AllocateWindowDescFront<FounderStartWindow>(_founder_start_desc, 0);
	return true;
}

/* --- Found your startup: background and scenario before the map is made --- */

/** Setup window opened by "New Startup" on the title screen. */
struct FounderSetupWindow : public Window {
	FounderSetupWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->DisableWidget(WID_FSU_SC_GOLIATH);
		this->DisableWidget(WID_FSU_SC_TUTORIAL);
		this->MarkSelected(WID_FSU_SC_SANDBOX, true);
		this->UpdateBackground();
	}

	/** Show a choice as selected: lowered and tinted, since grey bevels alone are hard to see. */
	void MarkSelected(WidgetID widget, bool selected)
	{
		this->SetWidgetLoweredState(widget, selected);
		this->GetWidget<NWidgetCore>(widget)->colour = selected ? Colours::LightBlue : FOUNDER_COLOUR;
	}

	void UpdateBackground()
	{
		uint8_t bg = _settings_newgame.game_creation.founder_background;
		this->MarkSelected(WID_FSU_BG_ENGINEER, bg == 0);
		this->MarkSelected(WID_FSU_BG_SELLER, bg == 1);
		this->MarkSelected(WID_FSU_BG_OPERATOR, bg == 2);
		this->SetDirty();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_FSU_BG_ENGINEER:
			case WID_FSU_BG_SELLER:
			case WID_FSU_BG_OPERATOR:
				_settings_newgame.game_creation.founder_background = static_cast<uint8_t>(widget - WID_FSU_BG_ENGINEER);
				this->UpdateBackground();
				break;

			case WID_FSU_MAP:
				_settings_newgame.game_creation.founder_mode = true;
				this->Close();
				ShowGenerateLandscape();
				break;

			case WID_FSU_START:
				_settings_newgame.game_creation.founder_mode = true;
				this->Close();
				StartNewGameWithoutGUI(GENERATE_NEW_SEED);
				break;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_founder_setup_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, FOUNDER_COLOUR),
		NWidget(WWT_CAPTION, FOUNDER_COLOUR), SetStringTip(STR_FOUNDER_SETUP_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, FOUNDER_COLOUR),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			NWidget(WWT_LABEL, Colours::Invalid), SetStringTip(STR_FOUNDER_SETUP_BACKGROUND), SetAlignment({AlignmentH::Start, AlignmentV::Middle}), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_FSU_BG_ENGINEER), SetStringTip(STR_FOUNDER_SETUP_BG_ENGINEER, STR_FOUNDER_SETUP_BG_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_FSU_BG_SELLER), SetStringTip(STR_FOUNDER_SETUP_BG_SELLER, STR_FOUNDER_SETUP_BG_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_FSU_BG_OPERATOR), SetStringTip(STR_FOUNDER_SETUP_BG_OPERATOR, STR_FOUNDER_SETUP_BG_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_LABEL, Colours::Invalid), SetStringTip(STR_FOUNDER_SETUP_SCENARIO), SetAlignment({AlignmentH::Start, AlignmentV::Middle}), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_FSU_SC_GOLIATH), SetStringTip(STR_FOUNDER_SETUP_SC_GOLIATH, STR_FOUNDER_SETUP_SC_GOLIATH_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_FSU_SC_SANDBOX), SetStringTip(STR_FOUNDER_SETUP_SC_SANDBOX, STR_FOUNDER_SETUP_SC_SANDBOX_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_FSU_SC_TUTORIAL), SetStringTip(STR_FOUNDER_SETUP_SC_TUTORIAL, STR_FOUNDER_SETUP_SC_TUTORIAL_TOOLTIP), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_FSU_MAP), SetStringTip(STR_FOUNDER_SETUP_MAP, STR_FOUNDER_SETUP_MAP_TOOLTIP), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, WID_FSU_START), SetStringTip(STR_FOUNDER_SETUP_START, STR_FOUNDER_SETUP_START_TOOLTIP), SetFill(1, 0),
	EndContainer(),
};

static WindowDesc _founder_setup_desc(
	WindowPosition::Center, {}, 0, 0,
	WindowClass::FounderSetup, WindowClass::None,
	{},
	_nested_founder_setup_widgets
);

/** Open the "found your startup" setup window. */
void ShowFounderSetupWindow()
{
	CloseWindowByClass(WindowClass::FounderSetup);
	new FounderSetupWindow(_founder_setup_desc, 0);
}
