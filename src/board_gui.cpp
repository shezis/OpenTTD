/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file board_gui.cpp Founder Mode board window: valuation, cap table and investor offers. */

#include "stdafx.h"
#include "funding_func.h"
#include "founder_gui.h"
#include "market_func.h"
#include "office_func.h"
#include "employee_base.h"
#include "employee_cmd.h"
#include "feature_base.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "company_gui.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "widgets/board_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/** Docked board window. */
struct BoardWindow : public Window {
	BoardWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->owner = static_cast<Owner>(this->window_number);
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

	const Company *GetCompany() const { return Company::Get(static_cast<CompanyID>(this->window_number)); }

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_BD_CAPTION) return GetString(STR_BOARD_CAPTION, this->window_number);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		const int line = GetCharacterHeight(FontSize::Normal);
		switch (widget) {
			case WID_BD_REPORT: size.height = 6 * line + WidgetDimensions::scaled.framerect.Vertical(); break;
			case WID_BD_CAPTABLE: size.height = 6 * line + WidgetDimensions::scaled.framerect.Vertical(); resize.height = 1; fill.height = 1; break;
			case WID_BD_OFFER: size.height = 4 * line + WidgetDimensions::scaled.framerect.Vertical(); break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const Company *c = this->GetCompany();
		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		const int line = GetCharacterHeight(FontSize::Normal);
		switch (widget) {
			case WID_BD_REPORT: {
				StringID stage = c->founder_ipo ? STR_BOARD_PUBLIC : (c->founder_stage == 0 ? STR_BOARD_NO_ROUNDS : GetFundingRoundSpec(c->founder_stage - 1).name);
				DrawString(ir, GetString(STR_BOARD_VALUATION, c->founder_valuation, stage), c->founder_ipo ? TextColour::DarkGreen : TextColour::Black);
				ir.top += line;
				DrawString(ir, GetString(STR_BOARD_REVENUE, GetCompanyMRR(c->index), GetCompanyUsers(c->index)), TextColour::Black);
				ir.top += line;
				Money burn = GetCompanyMonthlyCosts(c->index);
				Money net = burn - GetCompanyMRR(c->index);
				DrawString(ir, GetString(STR_BOARD_BURN, burn, net > 0 ? net : Money(0)), TextColour::Black);
				ir.top += line;
				/* Where the money goes: every running cost, hubs and field sales included. */
				uint hubs = CountHubs(c->index);
				uint reps = CountFieldReps(c->index);
				ir.top = DrawStringMultiLine(ir.left, ir.right, ir.top, ir.top + 2 * line, GetString(STR_BOARD_COSTS_BREAKDOWN,
						GetMonthlyPayroll(c->index), GetOfficeRent(c->office_level), HUB_RENT * hubs, hubs, GetFieldSalesCosts(c->index), reps, c->founder_sponsor_monthly), TextColour::Grey);
				if (c->money < 0) {
					DrawString(ir, GetString(STR_BOARD_INSOLVENT, std::max<uint>(c->months_of_bankruptcy, 1)), TextColour::Red);
				} else if (net <= 0) {
					DrawString(ir, STR_BOARD_RUNWAY_PROFITABLE, TextColour::DarkGreen);
				} else {
					int64_t tenths = std::max<int64_t>(0, static_cast<int64_t>(c->money) * 10 / static_cast<int64_t>(net));
					DrawString(ir, GetString(STR_BOARD_RUNWAY, tenths / 10, tenths % 10), tenths < 30 ? TextColour::Red : (tenths < 60 ? TextColour::Yellow : TextColour::Black));
				}
				break;
			}

			case WID_BD_CAPTABLE: {
				DrawString(ir, STR_BOARD_CAPTABLE_TITLE, TextColour::Black);
				ir.top += line;
				DrawString(ir, GetString(STR_BOARD_CAPTABLE_FOUNDERS, c->founder_equity / 10, c->founder_equity % 10), TextColour::Black);
				ir.top += line;
				for (uint8_t round = 0; round < c->founder_stage; round++) {
					uint16_t eq = GetInvestorEquity(c->index, round);
					DrawString(ir, GetString(STR_BOARD_CAPTABLE_ROUND, GetFundingRoundSpec(round).name, GetInvestorName(c->founder_round_investor[round]), c->founder_round_amount[round], eq / 10, eq % 10), TextColour::Black);
					ir.top += line;
				}
				break;
			}

			case WID_BD_OFFER: {
				if (c->founder_ipo) {
					Money raised = 0;
					for (uint8_t round = 0; round < c->founder_stage; round++) raised += c->founder_round_amount[round];
					Money stake = c->founder_valuation / 1000 * c->founder_equity;
					DrawStringMultiLine(ir, GetString(STR_BOARD_IPO_REPORT, c->founder_equity / 10, c->founder_equity % 10, stake, raised, c->founder_stage, GetCompanyUsers(c->index)), TextColour::DarkGreen);
				} else if (c->founder_offer_stage != 0) {
					int64_t post = c->founder_offer_amount * 1000 / std::max<uint16_t>(c->founder_offer_equity, 1);
					DrawStringMultiLine(ir, GetString(STR_BOARD_OFFER, GetInvestorName(c->founder_offer_investor), GetFundingRoundSpec(c->founder_offer_stage - 1).name,
							c->founder_offer_amount, c->founder_offer_equity / 10, c->founder_offer_equity % 10, post, c->founder_offer_months), TextColour::Black);
				} else if (c->founder_stage < MAX_FUNDING_STAGE) {
					const FundingRoundSpec &spec = GetFundingRoundSpec(c->founder_stage);
					std::string needs = spec.required_work != 0xFF ? std::string(GetWorkItemSpec(spec.required_work).name) : std::string();
					if (spec.min_users == 0) {
						DrawStringMultiLine(ir, GetString(needs.empty() ? STR_BOARD_NEXT_MRR : STR_BOARD_NEXT_MRR_WITH_WORK, spec.name, spec.min_mrr, needs), TextColour::Black);
					} else {
						DrawStringMultiLine(ir, GetString(needs.empty() ? STR_BOARD_NEXT : STR_BOARD_NEXT_WITH_WORK, spec.name, spec.min_mrr, spec.min_users, needs), TextColour::Black);
					}
				} else {
					DrawStringMultiLine(ir, STR_BOARD_ALL_ROUNDS, TextColour::Black);
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BD_ACCEPT: Command<Commands::RespondFundingOffer>::Post(STR_ERROR_CAN_T_RESPOND_OFFER, true); break;
			case WID_BD_DECLINE: Command<Commands::RespondFundingOffer>::Post(STR_ERROR_CAN_T_RESPOND_OFFER, false); break;
			case WID_BD_FINANCES: ShowCompanyFinances(static_cast<CompanyID>(this->window_number)); break;
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		bool own = this->window_number == _local_company;
		bool offer = this->GetCompany()->founder_offer_stage != 0;
		this->SetWidgetsDisabledState(!own || !offer, WID_BD_ACCEPT, WID_BD_DECLINE);
		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_board_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, FOUNDER_COLOUR),
		NWidget(WWT_CAPTION, FOUNDER_COLOUR, WID_BD_CAPTION),
		NWidget(WWT_SHADEBOX, FOUNDER_COLOUR),
		NWidget(WWT_STICKYBOX, FOUNDER_COLOUR),
	EndContainer(),
	NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_BD_REPORT), SetResize(1, 0), EndContainer(),
	NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_BD_CAPTABLE), SetResize(1, 1), EndContainer(),
	NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_BD_OFFER), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, WID_BD_ACCEPT), SetStringTip(STR_BOARD_ACCEPT, STR_BOARD_ACCEPT_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_BD_DECLINE), SetStringTip(STR_BOARD_DECLINE, STR_BOARD_DECLINE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_BD_FINANCES), SetStringTip(STR_BOARD_FINANCES, STR_BOARD_FINANCES_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, FOUNDER_COLOUR),
	EndContainer(),
};

static WindowDesc _board_desc(
	WindowPosition::Manual, "founder_board", 0, 0,
	WindowClass::Board, WindowClass::None,
	{},
	_nested_board_widgets
);

/**
 * Open the board window of a company, in the docked panel spot.
 * @param company The company.
 */
void ShowBoardWindow(CompanyID company)
{
	if (!Company::IsValidID(company)) return;
	CloseOtherFounderTabs(WindowClass::Board, company);
	AllocateWindowDescFront<BoardWindow>(_board_desc, company);
}
