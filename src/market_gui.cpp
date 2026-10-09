/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file market_gui.cpp Founder Mode market window: towns, customers, demand and sales reps. */

#include "stdafx.h"
#include "market_func.h"
#include "founder_gui.h"
#include "employee_base.h"
#include "employee_cmd.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "error.h"
#include "gfx_func.h"
#include "palette_func.h"
#include "strings_func.h"
#include "town.h"
#include "viewport_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "widgets/market_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/** Docked window listing towns as markets. */
struct MarketWindow : public Window {
	Scrollbar *vscroll = nullptr; ///< Scrollbar of the list.
	TownID selected = TownID::Invalid(); ///< Selected town.

	MarketWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_MK_SCROLLBAR);
		this->FinishInitNested(window_number);
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

	CompanyID GetCompany() const { return static_cast<CompanyID>(this->window_number); }

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_MK_CAPTION) return GetString(STR_MARKET_CAPTION, this->window_number);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_MK_LIST:
				resize.width = 1;
				fill.height = resize.height = 2 * GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(6);
				size.height = 5 * resize.height;
				break;
			case WID_MK_SUMMARY:
				size.height = GetCharacterHeight(FontSize::Normal) + WidgetDimensions::scaled.framerect.Vertical();
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		CompanyID company = this->GetCompany();
		switch (widget) {
			case WID_MK_SUMMARY:
				DrawString(r.Shrink(WidgetDimensions::scaled.framerect), GetString(STR_MARKET_SUMMARY, GetCompanyUsers(company), GetCompanyMRR(company), GetCompanyPricePerUser(company)));
				break;

			case WID_MK_LIST: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
				const int row_h = this->resize.step_height;
				const int line = GetCharacterHeight(FontSize::Normal);
				int pos = -this->vscroll->GetPosition();
				for (const Town *t : GetTownsBySize()) {
					if (pos >= 0 && pos < this->vscroll->GetCapacity()) {
						Rect row = ir.WithHeight(row_h);
						bool sel = t->index == this->selected;
						if (sel) GfxFillRect(row.left, row.top, row.right, row.bottom - 1, PC_DARK_GREY);
						TextColour tc = sel ? TextColour::White : TextColour::Black;
						uint size = GetTownMarketSize(t->index);
						uint users = t->founder_users[company];
						uint share = size == 0 ? 0 : users * 100 / size;

						/* Line 1: name, customers, share bar, status. */
						int y1 = row.top + ScaleGUITrad(2);
						int w = row.Width();
						DrawString(row.left, row.left + w * 45 / 100, y1, GetString(STR_MARKET_TOWN, t->index, users, size), tc);
						int bx0 = row.left + w * 46 / 100, bx1 = row.left + w * 66 / 100;
						int by0 = y1 + line / 2 - ScaleGUITrad(3), by1 = by0 + ScaleGUITrad(6);
						GfxFillRect(bx0, by0, bx1, by1, PC_BLACK);
						int x = bx0;
						for (const Company *c : Company::Iterate()) {
							uint u = t->founder_users[c->index];
							if (u == 0 || size == 0) continue;
							int seg = (bx1 - bx0) * static_cast<int>(std::min(u, size)) / static_cast<int>(size);
							if (seg > 0) GfxFillRect(x, by0, std::min(bx1, x + seg), by1, GetColourGradient(c->colour, Shade::Normal));
							x += seg;
						}
						StringID status = GetCompanyHQTown(company) == t->index ? STR_MARKET_STATUS_HQ
								: IsTownOpportunity(company, t->index) ? STR_MARKET_STATUS_OPPORTUNITY
								: !IsTownInRepRange(company, t->index) ? STR_MARKET_STATUS_OUT_OF_RANGE
								: STR_MARKET_STATUS_NONE;
						DrawString(row.left + w * 68 / 100, row.right, y1, GetString(status, share), status == STR_MARKET_STATUS_OPPORTUNITY && !sel ? TextColour::DarkGreen : tc);

						/* Line 2: what the town wants and your reps there. */
						auto wants = GetTownWants(t->index);
						DrawString(row.left + ScaleGUITrad(8), row.right, y1 + line + ScaleGUITrad(1),
								GetString(STR_MARKET_WANTS, STR_FEATURE_CATEGORY_CORE + to_underlying(wants[0]), STR_FEATURE_CATEGORY_CORE + to_underlying(wants[1]), CountRepsInTown(company, t->index)),
								sel ? TextColour::White : TextColour::Grey, AlignmentH::Start, false, FontSize::Small);
						ir.top += row_h;
					}
					pos++;
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		CompanyID company = this->GetCompany();
		switch (widget) {
			case WID_MK_LIST: {
				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_MK_LIST, WidgetDimensions::scaled.framerect.top);
				auto towns = GetTownsBySize();
				if (row < 0 || row >= static_cast<int>(towns.size())) break;
				this->selected = towns[row]->index;
				if (click_count > 1) ScrollMainWindowToTile(towns[row]->xy);
				this->OnInvalidateData(0);
				break;
			}

			case WID_MK_ADD_REP: {
				/* First sales rep without a town. */
				for (const Employee *e : Employee::Iterate()) {
					if (e->company == company && e->role == EmployeeRole::Sales && !Town::IsValidID(e->town)) {
						Command<Commands::AssignRep>::Post(STR_ERROR_CAN_T_ASSIGN_REP, e->index, this->selected);
						return;
					}
				}
				ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_ASSIGN_REP), GetEncodedString(STR_MARKET_NO_FREE_REP), WarningLevel::Info);
				break;
			}

			case WID_MK_REMOVE_REP:
				for (const Employee *e : Employee::Iterate()) {
					if (e->company == company && e->role == EmployeeRole::Sales && e->town == this->selected) {
						Command<Commands::AssignRep>::Post(STR_ERROR_CAN_T_ASSIGN_REP, e->index, TownID::Invalid());
						return;
					}
				}
				break;

			case WID_MK_SHOW:
				if (Town::IsValidID(this->selected)) ScrollMainWindowToTile(Town::Get(this->selected)->xy);
				break;
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_MK_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (!Town::IsValidID(this->selected)) this->selected = TownID::Invalid();
		this->vscroll->SetCount(Town::GetNumItems());
		bool own = this->window_number == _local_company;
		bool sel = Town::IsValidID(this->selected);
		this->SetWidgetDisabledState(WID_MK_ADD_REP, !own || !sel || !IsTownInRepRange(this->GetCompany(), this->selected));
		this->SetWidgetDisabledState(WID_MK_REMOVE_REP, !own || !sel || CountRepsInTown(this->GetCompany(), this->selected) == 0);
		this->SetWidgetDisabledState(WID_MK_SHOW, !sel);
		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_market_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, FOUNDER_COLOUR),
		NWidget(WWT_CAPTION, FOUNDER_COLOUR, WID_MK_CAPTION),
		NWidget(WWT_SHADEBOX, FOUNDER_COLOUR),
		NWidget(WWT_STICKYBOX, FOUNDER_COLOUR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_MK_LIST), SetToolTip(STR_MARKET_LIST_TOOLTIP), SetScrollbar(WID_MK_SCROLLBAR), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, FOUNDER_COLOUR, WID_MK_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_MK_SUMMARY), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_MK_ADD_REP), SetStringTip(STR_MARKET_ADD_REP, STR_MARKET_ADD_REP_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_MK_REMOVE_REP), SetStringTip(STR_MARKET_REMOVE_REP, STR_MARKET_REMOVE_REP_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_MK_SHOW), SetStringTip(STR_MARKET_SHOW, STR_MARKET_SHOW_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, FOUNDER_COLOUR),
	EndContainer(),
};

static WindowDesc _market_desc(
	WindowPosition::Manual, "founder_market", 0, 0,
	WindowClass::Market, WindowClass::None,
	{},
	_nested_market_widgets
);

/**
 * Open the market window of a company, in the docked panel spot.
 * @param company The company.
 */
void ShowMarketWindow(CompanyID company)
{
	if (!Company::IsValidID(company)) return;
	CloseOtherFounderTabs(WindowClass::Market, company);
	AllocateWindowDescFront<MarketWindow>(_market_desc, company);
}
