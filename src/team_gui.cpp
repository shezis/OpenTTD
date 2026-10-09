/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file team_gui.cpp Founder Mode team window: list, hire and let go employees. */

#include "stdafx.h"
#include "team_gui.h"
#include "employee_base.h"
#include "employee_cmd.h"
#include "office_func.h"
#include "office_gui.h"
#include "town.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "feature_base.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "widgets/team_widget.h"

#include "table/strings.h"

#include "founder_gui.h"

#include "safeguards.h"

/** Name of each role, indexed by #EmployeeRole. */
static const StringID _employee_role_names[] = {
	STR_TEAM_ROLE_ENGINEER,
	STR_TEAM_ROLE_DESIGNER,
	STR_TEAM_ROLE_SALES,
	STR_TEAM_ROLE_OPERATIONS,
};
static_assert(std::size(_employee_role_names) == to_underlying(EmployeeRole::End));

/** Name of each level, indexed by #EmployeeLevel. */
static const StringID _employee_level_names[] = {
	STR_TEAM_LEVEL_JUNIOR,
	STR_TEAM_LEVEL_MID,
	STR_TEAM_LEVEL_SENIOR,
};
static_assert(std::size(_employee_level_names) == to_underlying(EmployeeLevel::End));

/** What each level trades off, indexed by #EmployeeLevel. */
static const StringID _employee_level_traits[] = {
	STR_TEAM_TRAIT_JUNIOR,
	STR_TEAM_TRAIT_MID,
	STR_TEAM_TRAIT_SENIOR,
};

/** Window listing a company's employees. */
struct TeamWindow : public Window {
	Scrollbar *vscroll = nullptr; ///< Scrollbar of the employee list.
	EmployeeID selected = EmployeeID::Invalid(); ///< Currently selected employee.

	TeamWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_TEAM_SCROLLBAR);
		this->FinishInitNested(window_number);
		this->owner = static_cast<Owner>(this->window_number);
		this->OnInvalidateData(0);
		this->LowerWidget(WID_TEAM_TAB_TEAM);
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

	/** Collect this company's employees in pool order. */
	std::vector<const Employee *> GetEmployees() const
	{
		std::vector<const Employee *> list;
		for (const Employee *e : Employee::Iterate()) {
			if (e->company == this->window_number) list.push_back(e);
		}
		return list;
	}

	bool IsOwnCompany() const
	{
		return this->window_number == _local_company;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_TEAM_CAPTION) return GetString(STR_TEAM_CAPTION, this->window_number);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_TEAM_LIST: {
				resize.width = 1;
				fill.height = resize.height = GetCharacterHeight(FontSize::Normal);
				size.height = 8 * resize.height + WidgetDimensions::scaled.framerect.Vertical();
				size.width = std::max<uint>(size.width, ScaleGUITrad(460));
				break;
			}

			case WID_TEAM_SUMMARY:
				size.height = GetCharacterHeight(FontSize::Normal) + WidgetDimensions::scaled.framerect.Vertical();
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_TEAM_LIST: this->DrawList(r); break;

			case WID_TEAM_SUMMARY: {
				CompanyID company = static_cast<CompanyID>(this->window_number);
				DrawString(r.Shrink(WidgetDimensions::scaled.framerect), GetString(STR_TEAM_SUMMARY, CountEmployees(company), GetOfficeDesks(GetOfficeLevel(company)), GetMonthlyPayroll(company), GetFieldSalesCosts(company)));
				break;
			}
		}
	}

	void DrawList(const Rect &r) const
	{
		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		const int line = GetCharacterHeight(FontSize::Normal);
		const auto employees = this->GetEmployees();

		if (employees.empty()) {
			DrawString(ir, STR_TEAM_NONE);
			return;
		}

		/* Columns as fractions of the width: name, level and role, work, salary, morale. */
		const int w = ir.Width();
		const int x_role = w * 26 / 100, x_skill = w * 46 / 100, x_salary = w * 70 / 100, x_morale = w * 86 / 100;

		int pos = -this->vscroll->GetPosition();
		const int cap = this->vscroll->GetCapacity();
		for (const Employee *e : employees) {
			if (pos >= 0 && pos < cap) {
				Rect row = ir.WithHeight(line);
				bool sel = e->index == this->selected;
				if (sel) GfxFillRect(row.left, row.top, row.right, row.bottom, PC_DARK_GREY);
				TextColour tc = sel ? TextColour::White : TextColour::Black;

				DrawString(row.left, row.left + x_role - 4, row.top, e->GetName(), tc);
				DrawString(row.left + x_role, row.left + x_skill - 4, row.top, GetString(STR_TEAM_LEVEL_ROLE, _employee_level_names[to_underlying(e->level)], _employee_role_names[to_underlying(e->role)]), tc);
				if (Town::IsValidID(e->town)) {
					DrawString(row.left + x_skill, row.left + x_salary - 4, row.top, GetString(STR_TEAM_WORK_TOWN, e->town), tc);
				} else if (const Feature *f = Feature::GetIfValid(e->feature); f != nullptr) {
					DrawString(row.left + x_skill, row.left + x_salary - 4, row.top, f->GetName(), tc);
				} else {
					DrawString(row.left + x_skill, row.left + x_salary - 4, row.top, STR_TEAM_WORK_FREE, sel ? TextColour::White : TextColour::Orange);
				}
				DrawString(row.left + x_salary, row.left + x_morale - 4, row.top, GetString(STR_TEAM_SALARY, e->salary), tc);
				TextColour mood = e->morale >= 70 ? TextColour::Green : (e->morale >= 45 ? TextColour::Yellow : TextColour::Red);
				if (e->poach_by != CompanyID::Invalid()) {
					DrawString(row.left + x_morale, row.right, row.top, GetString(STR_TEAM_POACH_OFFER, e->poach_by), sel ? TextColour::White : TextColour::Red);
				} else {
					DrawString(row.left + x_morale, row.right, row.top, GetString(STR_TEAM_MORALE, e->morale), sel ? TextColour::White : mood);
				}
				ir.top += line;
			}
			pos++;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TEAM_TAB_TEAM: break; // Already showing this tab.
			case WID_TEAM_TAB_OFFICE: ShowFounderTab(FounderTab::Office, static_cast<CompanyID>(this->window_number)); break;
			case WID_TEAM_TAB_WORK: ShowFounderTab(FounderTab::Work, static_cast<CompanyID>(this->window_number)); break;

			case WID_TEAM_LIST: {
				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_TEAM_LIST, WidgetDimensions::scaled.framerect.top);
				const auto employees = this->GetEmployees();
				this->selected = (row >= 0 && row < static_cast<int>(employees.size())) ? employees[row]->index : EmployeeID::Invalid();
				this->OnInvalidateData(0);
				break;
			}

			case WID_TEAM_HIRE_ENGINEER:
			case WID_TEAM_HIRE_DESIGNER:
			case WID_TEAM_HIRE_SALES:
			case WID_TEAM_HIRE_OPERATIONS: {
				/* Each level with its monthly cost and what it trades off. */
				EmployeeRole role = static_cast<EmployeeRole>(widget - WID_TEAM_HIRE_ENGINEER);
				DropDownList list;
				for (uint l = 0; l < to_underlying(EmployeeLevel::End); l++) {
					EmployeeLevel level = static_cast<EmployeeLevel>(l);
					list.push_back(MakeDropDownListStringItem(GetString(STR_TEAM_HIRE_LEVEL_ITEM, _employee_level_names[to_underlying(level)],
							GetLevelSalary(role, level), GetLevelSpeedPercent(level), _employee_level_traits[to_underlying(level)]), to_underlying(level)));
				}
				ShowDropDownList(this, std::move(list), -1, widget, 0, DropDownOption::Filterable);
				break;
			}

			case WID_TEAM_OFFICE:
				ShowOfficeWindow(static_cast<CompanyID>(this->window_number));
				break;

			case WID_TEAM_FIRE:
				if (this->selected != EmployeeID::Invalid()) {
					Command<Commands::FireEmployee>::Post(STR_ERROR_CAN_T_LET_GO, this->selected);
				}
				break;
		}
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		if (widget < WID_TEAM_HIRE_ENGINEER || widget > WID_TEAM_HIRE_OPERATIONS || index < 0) return;
		EmployeeRole role = static_cast<EmployeeRole>(widget - WID_TEAM_HIRE_ENGINEER);
		Command<Commands::HireEmployee>::Post(STR_ERROR_CAN_T_HIRE, role, static_cast<EmployeeLevel>(index));
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_TEAM_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		if (!Employee::IsValidID(this->selected) || Employee::Get(this->selected)->company != this->window_number) {
			this->selected = EmployeeID::Invalid();
		}

		this->vscroll->SetCount(this->GetEmployees().size());
		bool own = this->IsOwnCompany();
		this->SetWidgetsDisabledState(!own, WID_TEAM_HIRE_ENGINEER, WID_TEAM_HIRE_DESIGNER, WID_TEAM_HIRE_SALES, WID_TEAM_HIRE_OPERATIONS);
		this->SetWidgetDisabledState(WID_TEAM_FIRE, !own || this->selected == EmployeeID::Invalid());
		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_team_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, FOUNDER_COLOUR),
		NWidget(WWT_CAPTION, FOUNDER_COLOUR, WID_TEAM_CAPTION),
		NWidget(WWT_SHADEBOX, FOUNDER_COLOUR),
		NWidget(WWT_DEFSIZEBOX, FOUNDER_COLOUR),
		NWidget(WWT_STICKYBOX, FOUNDER_COLOUR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_TEAM_TAB_TEAM), SetStringTip(STR_FOUNDER_TAB_TEAM, STR_FOUNDER_TAB_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_TEAM_TAB_OFFICE), SetStringTip(STR_FOUNDER_TAB_OFFICE, STR_FOUNDER_TAB_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_TEAM_TAB_WORK), SetStringTip(STR_FOUNDER_TAB_WORK, STR_FOUNDER_TAB_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_TEAM_LIST), SetToolTip(STR_TEAM_LIST_TOOLTIP), SetScrollbar(WID_TEAM_SCROLLBAR), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, FOUNDER_COLOUR, WID_TEAM_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_TEAM_SUMMARY), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_DROPDOWN, FOUNDER_COLOUR, WID_TEAM_HIRE_ENGINEER), SetStringTip(STR_TEAM_HIRE_ENGINEER, STR_TEAM_HIRE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_DROPDOWN, FOUNDER_COLOUR, WID_TEAM_HIRE_DESIGNER), SetStringTip(STR_TEAM_HIRE_DESIGNER, STR_TEAM_HIRE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_DROPDOWN, FOUNDER_COLOUR, WID_TEAM_HIRE_SALES), SetStringTip(STR_TEAM_HIRE_SALES, STR_TEAM_HIRE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_DROPDOWN, FOUNDER_COLOUR, WID_TEAM_HIRE_OPERATIONS), SetStringTip(STR_TEAM_HIRE_OPERATIONS, STR_TEAM_HIRE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_TEAM_FIRE), SetStringTip(STR_TEAM_LET_GO, STR_TEAM_LET_GO_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_TEAM_OFFICE), SetStringTip(STR_TEAM_OFFICE, STR_TEAM_OFFICE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, FOUNDER_COLOUR),
	EndContainer(),
};

static WindowDesc _team_desc(
	WindowPosition::Automatic, "founder_team", 500, 220,
	WindowClass::Team, WindowClass::None,
	{},
	_nested_team_widgets
);

/**
 * Open the team window of a company.
 * @param company The company whose team to show.
 */
void ShowTeamWindow(CompanyID company)
{
	if (!Company::IsValidID(company)) return;
	CloseOtherFounderTabs(WindowClass::Team, company);
	AllocateWindowDescFront<TeamWindow>(_team_desc, company);
}
