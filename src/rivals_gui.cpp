/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rivals_gui.cpp Founder Mode startup league: every startup ranked by valuation. */

#include "stdafx.h"
#include "market_func.h"
#include "funding_func.h"
#include "company_base.h"
#include "company_func.h"
#include "company_gui.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"
#include "ai/ai_config.hpp"
#include "timer/timer.h"
#include "timer/timer_window.h"

#include "widgets/rivals_widget.h"

#include "table/strings.h"

#include "founder_gui.h"

#include "safeguards.h"

/** Docked league of startups: you and the rival startups, transit operators left out. */
struct StartupLeagueWindow : public Window {
	Scrollbar *vscroll = nullptr; ///< Scrollbar of the list.
	CompanyID selected = CompanyID::Invalid(); ///< Selected startup.

	StartupLeagueWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_FML_SCROLLBAR);
		this->FinishInitNested(window_number);
		this->selected = _local_company;
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

	/** Startups, highest valuation first. */
	static std::vector<const Company *> GetStartups()
	{
		std::vector<const Company *> list;
		for (const Company *c : Company::Iterate()) {
			if (!IsFounderOperator(c->index)) list.push_back(c);
		}
		std::ranges::stable_sort(list, std::greater{}, [](const Company *c) { return c->founder_valuation; });
		return list;
	}

	/** How the startup plays: you, or a rival's personality. */
	static StringID GetStyle(const Company *c)
	{
		if (!c->is_ai) return STR_LEAGUE_STYLE_YOU;
		if (c->ai_config == nullptr) return STR_LEAGUE_STYLE_RIVAL;
		int p = c->ai_config->GetSetting("personality");
		return p >= 0 && p <= 3 ? STR_LEAGUE_STYLE_BOOTSTRAPPER + p : STR_LEAGUE_STYLE_RIVAL;
	}

	/** Longer description of a startup's style, for the summary. */
	static StringID GetStyleDescription(const Company *c)
	{
		StringID s = GetStyle(c);
		return s >= STR_LEAGUE_STYLE_BOOTSTRAPPER && s <= STR_LEAGUE_STYLE_INCUMBENT ? STR_LEAGUE_ABOUT_BOOTSTRAPPER + (s - STR_LEAGUE_STYLE_BOOTSTRAPPER) : STR_LEAGUE_ABOUT_NONE;
	}

	/** Round reached: public, the last round closed, or none. */
	static StringID GetRound(const Company *c)
	{
		if (c->founder_ipo) return STR_BOARD_PUBLIC;
		return c->founder_stage == 0 ? STR_BOARD_NO_ROUNDS : GetFundingRoundSpec(c->founder_stage - 1).name;
	}

	int RowHeight() const
	{
		return GetCharacterHeight(FontSize::Normal) + GetCharacterHeight(FontSize::Small) + ScaleGUITrad(4);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_FML_LIST:
				resize.width = 1;
				fill.height = resize.height = this->RowHeight();
				size.height = 6 * resize.height + WidgetDimensions::scaled.framerect.Vertical();
				break;

			case WID_FML_SUMMARY:
				size.height = 3 * GetCharacterHeight(FontSize::Normal) + WidgetDimensions::scaled.framerect.Vertical();
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_FML_LIST: this->DrawList(r); break;

			case WID_FML_SUMMARY: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
				const Company *c = Company::GetIfValid(this->selected);
				if (c == nullptr) {
					DrawStringMultiLine(ir, GetString(STR_LEAGUE_SUMMARY_NONE, IPO_VALUATION));
				} else {
					uint pct = static_cast<uint>(std::min<int64_t>(100, c->founder_valuation * 100 / IPO_VALUATION));
					DrawStringMultiLine(ir, GetString(STR_LEAGUE_SUMMARY, c->index, pct, IPO_VALUATION, CountHubs(c->index), GetStyleDescription(c)));
				}
				break;
			}
		}
	}

	void DrawList(const Rect &r) const
	{
		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		const auto startups = GetStartups();
		const int row_h = this->RowHeight();
		const int line = GetCharacterHeight(FontSize::Normal);
		const int w = ir.Width();
		const int x_name = ScaleGUITrad(40), x_val = w * 55 / 100;

		int pos = -this->vscroll->GetPosition();
		const int cap = this->vscroll->GetCapacity();
		int rank = 0;
		for (const Company *c : startups) {
			rank++;
			if (pos >= 0 && pos < cap) {
				Rect row = ir.WithHeight(row_h);
				bool sel = c->index == this->selected;
				if (sel) GfxFillRect(row.left, row.top, row.right, row.bottom - 1, PC_DARK_GREY);
				TextColour tc = sel ? TextColour::White : TextColour::Black;
				bool you = c->index == _local_company;

				DrawString(row.left, row.left + x_name, row.top, GetString(STR_LEAGUE_RANK, rank), tc);
				DrawCompanyIcon(c->index, row.left + x_name - ScaleGUITrad(14), row.top + ScaleGUITrad(1));
				DrawString(row.left + x_name, row.left + x_val - 4, row.top, GetString(STR_COMPANY_NAME, c->index), you && !sel ? TextColour::DarkGreen : tc);
				DrawString(row.left + x_val, row.right, row.top, GetString(STR_LEAGUE_VALUE, c->founder_valuation, GetRound(c)), c->founder_ipo && !sel ? TextColour::DarkGreen : tc, AlignmentH::End);
				DrawString(row.left + x_name, row.right, row.top + line, GetString(STR_LEAGUE_DETAIL, GetStyle(c), GetCompanyUsers(c->index), GetCompanyMRR(c->index), c->money), sel ? TextColour::Silver : TextColour::Grey, AlignmentH::Start, false, FontSize::Small);
				ir.top += row_h;
			}
			pos++;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_FML_LIST: {
				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_FML_LIST, WidgetDimensions::scaled.framerect.top);
				const auto startups = GetStartups();
				if (row >= 0 && row < static_cast<int>(startups.size())) this->selected = startups[row]->index;
				this->OnInvalidateData(0);
				break;
			}

			case WID_FML_SHOW:
				if (const Company *c = Company::GetIfValid(this->selected); c != nullptr && c->location_of_HQ != INVALID_TILE) ScrollMainWindowToTile(c->location_of_HQ);
				break;

			case WID_FML_BOARD:
				if (Company::IsValidID(this->selected)) ShowBoardWindow(this->selected);
				break;
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_FML_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (!Company::IsValidID(this->selected)) this->selected = CompanyID::Invalid();
		this->vscroll->SetCount(GetStartups().size());
		const Company *c = Company::GetIfValid(this->selected);
		this->SetWidgetDisabledState(WID_FML_SHOW, c == nullptr || c->location_of_HQ == INVALID_TILE);
		this->SetWidgetDisabledState(WID_FML_BOARD, c == nullptr);
		this->SetDirty();
	}

	/** Numbers change monthly and AI companies come and go; refresh every few seconds. */
	const IntervalTimer<TimerWindow> refresh_interval = {std::chrono::seconds(3), [this](auto) {
		this->OnInvalidateData(0);
	}};
};

static constexpr std::initializer_list<NWidgetPart> _nested_startup_league_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, FOUNDER_COLOUR),
		NWidget(WWT_CAPTION, FOUNDER_COLOUR, WID_FML_CAPTION), SetStringTip(STR_LEAGUE_CAPTION),
		NWidget(WWT_SHADEBOX, FOUNDER_COLOUR),
		NWidget(WWT_STICKYBOX, FOUNDER_COLOUR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_FML_LIST), SetToolTip(STR_LEAGUE_LIST_TOOLTIP), SetScrollbar(WID_FML_SCROLLBAR), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, FOUNDER_COLOUR, WID_FML_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_FML_SUMMARY), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_FML_SHOW), SetStringTip(STR_LEAGUE_SHOW, STR_LEAGUE_SHOW_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_FML_BOARD), SetStringTip(STR_LEAGUE_BOARD, STR_LEAGUE_BOARD_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, FOUNDER_COLOUR),
	EndContainer(),
};

static WindowDesc _startup_league_desc(
	WindowPosition::Manual, "founder_league", 0, 0,
	WindowClass::StartupLeague, WindowClass::None,
	{},
	_nested_startup_league_widgets
);

/** Open the startup league. */
void ShowStartupLeague()
{
	CloseOtherFounderTabs(WindowClass::StartupLeague, _local_company);
	AllocateWindowDescFront<StartupLeagueWindow>(_startup_league_desc, 0);
}
