/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file office_gui.cpp Founder Mode office window: the offices side by side, size against cost. */

#include "stdafx.h"
#include "office_gui.h"
#include "office_func.h"
#include "office_cmd.h"
#include "employee_base.h"
#include "team_gui.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "gfx_func.h"
#include "palette_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "widgets/office_widget.h"

#include "table/strings.h"

#include "founder_gui.h"

#include "safeguards.h"

/** Window showing a company's office. */
struct OfficeWindow : public Window {
	OfficeWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->owner = static_cast<Owner>(this->window_number);
		this->OnInvalidateData(0);
		this->LowerWidget(WID_OFFICE_TAB_OFFICE);
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

	CompanyID GetCompany() const { return static_cast<CompanyID>(this->window_number); }

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_OFFICE_CAPTION: return GetString(STR_OFFICE_CAPTION, this->window_number);
			case WID_OFFICE_UPGRADE: {
				uint8_t level = GetOfficeLevel(this->GetCompany());
				if (level >= MAX_OFFICE_LEVEL) return GetString(STR_OFFICE_UPGRADE_MAX);
				return GetString(STR_OFFICE_UPGRADE, STR_OFFICE_LEVEL_GARAGE + level + 1, GetOfficeUpgradeCost(level));
			}
			default: return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_OFFICE_VIEW:
				size.width = std::max<uint>(size.width, ScaleGUITrad(420));
				size.height = std::max<uint>(size.height, ScaleGUITrad(240));
				resize.width = resize.height = 1;
				break;

			case WID_OFFICE_SUMMARY:
				size.height = GetCharacterHeight(FontSize::Normal) + WidgetDimensions::scaled.framerect.Vertical();
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_OFFICE_VIEW: this->DrawOffice(r); break;

			case WID_OFFICE_SUMMARY: {
				CompanyID company = this->GetCompany();
				uint8_t level = GetOfficeLevel(company);
				DrawString(r.Shrink(WidgetDimensions::scaled.framerect), GetString(STR_OFFICE_SUMMARY, STR_OFFICE_LEVEL_GARAGE + level, CountEmployees(company), GetOfficeDesks(level), GetOfficeRent(level)));
				break;
			}
		}
	}

	/** Size versus cost of each office: desks, rent, cost to move and rent per desk, with the current one marked. */
	void DrawOffice(const Rect &r) const
	{
		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		const int line = GetCharacterHeight(FontSize::Normal);
		const CompanyID company = this->GetCompany();
		const uint8_t current = GetOfficeLevel(company);
		const uint staff = CountEmployees(company);

		const int w = ir.Width();
		const int x_desks = w * 26 / 100, x_rent = w * 38 / 100, x_move = w * 60 / 100, x_per = w * 82 / 100;
		auto row = [&](int y, StringID name, std::string desks, std::string rent, std::string move, std::string per, TextColour tc) {
			DrawString(ir.left, ir.left + x_desks - 4, y, name, tc);
			DrawString(ir.left + x_desks, ir.left + x_rent - 4, y, desks, tc);
			DrawString(ir.left + x_rent, ir.left + x_move - 4, y, rent, tc);
			DrawString(ir.left + x_move, ir.left + x_per - 4, y, move, tc);
			DrawString(ir.left + x_per, ir.right, y, per, tc);
		};

		int y = ir.top;
		row(y, STR_OFFICE_COL_OFFICE, GetString(STR_OFFICE_COL_DESKS), GetString(STR_OFFICE_COL_RENT), GetString(STR_OFFICE_COL_MOVE), GetString(STR_OFFICE_COL_PER_DESK), TextColour::Black);
		y += line + ScaleGUITrad(2);
		GfxDrawLine(ir.left, y - ScaleGUITrad(1), ir.right, y - ScaleGUITrad(1), PC_DARK_GREY);

		for (uint8_t level = 0; level <= MAX_OFFICE_LEVEL; level++) {
			uint desks = GetOfficeDesks(level);
			Money rent = GetOfficeRent(level);
			bool here = level == current;
			if (here) GfxFillRect(ir.left, y, ir.right, y + 2 * line, GetColourGradient(Colours::LightBlue, Shade::Dark));
			TextColour tc = here ? TextColour::White : (level < current ? TextColour::Silver : TextColour::Black);
			std::string move = level <= current ? GetString(level == current ? STR_OFFICE_HERE : STR_OFFICE_OUTGROWN) : GetString(STR_JUST_CURRENCY_LONG, GetOfficeUpgradeCost(level - 1));
			row(y, STR_OFFICE_LEVEL_GARAGE + level, GetString(STR_JUST_COMMA, desks), GetString(STR_JUST_CURRENCY_LONG, rent), move, GetString(STR_JUST_CURRENCY_LONG, rent / std::max(desks, 1U)), tc);

			/* Desk bar: filled desks of your team in this office. */
			int by = y + line + ScaleGUITrad(3);
			int bx1 = ir.left + x_rent - ScaleGUITrad(8);
			GfxFillRect(ir.left, by, bx1, by + ScaleGUITrad(4), PC_BLACK);
			int filled = ir.left + (bx1 - ir.left) * static_cast<int>(std::min(staff, desks)) / static_cast<int>(desks);
			if (filled > ir.left) GfxFillRect(ir.left, by, filled, by + ScaleGUITrad(4), staff > desks ? PC_RED : (here ? PC_GREEN : PC_GREY));
			DrawString(ir.left + x_rent, ir.right, y + line, GetString(staff <= desks ? STR_OFFICE_FITS : STR_OFFICE_TOO_SMALL, std::min(staff, desks), desks), tc, AlignmentH::Start, false, FontSize::Small);
			y += 2 * line + ScaleGUITrad(6);
		}

		y += line / 2;
		DrawStringMultiLine(ir.left, ir.right, y, ir.bottom, STR_OFFICE_TABLE_NOTE, TextColour::Black);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_OFFICE_TAB_TEAM: ShowFounderTab(FounderTab::Team, static_cast<CompanyID>(this->window_number)); break;
			case WID_OFFICE_TAB_OFFICE: break; // Already showing this tab.
			case WID_OFFICE_TAB_WORK: ShowFounderTab(FounderTab::Work, static_cast<CompanyID>(this->window_number)); break;

			case WID_OFFICE_UPGRADE:
				Command<Commands::UpgradeOffice>::Post(STR_ERROR_CAN_T_UPGRADE_OFFICE, static_cast<uint8_t>(GetOfficeLevel(this->GetCompany()) + 1));
				break;

			case WID_OFFICE_TEAM:
				ShowTeamWindow(this->GetCompany());
				break;
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		bool own = this->window_number == _local_company;
		this->SetWidgetDisabledState(WID_OFFICE_UPGRADE, !own || GetOfficeLevel(this->GetCompany()) >= MAX_OFFICE_LEVEL);
		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_office_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, FOUNDER_COLOUR),
		NWidget(WWT_CAPTION, FOUNDER_COLOUR, WID_OFFICE_CAPTION),
		NWidget(WWT_SHADEBOX, FOUNDER_COLOUR),
		NWidget(WWT_DEFSIZEBOX, FOUNDER_COLOUR),
		NWidget(WWT_STICKYBOX, FOUNDER_COLOUR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_OFFICE_TAB_TEAM), SetStringTip(STR_FOUNDER_TAB_TEAM, STR_FOUNDER_TAB_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_OFFICE_TAB_OFFICE), SetStringTip(STR_FOUNDER_TAB_OFFICE, STR_FOUNDER_TAB_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_OFFICE_TAB_WORK), SetStringTip(STR_FOUNDER_TAB_WORK, STR_FOUNDER_TAB_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_OFFICE_VIEW), SetResize(1, 1), EndContainer(),
	NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_OFFICE_SUMMARY), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_OFFICE_UPGRADE), SetToolTip(STR_OFFICE_UPGRADE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_OFFICE_TEAM), SetStringTip(STR_OFFICE_TEAM, STR_OFFICE_TEAM_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, FOUNDER_COLOUR),
	EndContainer(),
};

static WindowDesc _office_desc(
	WindowPosition::Automatic, "founder_office", 460, 320,
	WindowClass::Office, WindowClass::None,
	{},
	_nested_office_widgets
);

/**
 * Open the office window of a company.
 * @param company The company whose office to show.
 */
void ShowOfficeWindow(CompanyID company)
{
	if (!Company::IsValidID(company)) return;
	CloseOtherFounderTabs(WindowClass::Office, company);
	AllocateWindowDescFront<OfficeWindow>(_office_desc, company);
}
