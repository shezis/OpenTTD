/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file roadmap_gui.cpp Founder Mode roadmap window: plan, staff and ship features. */

#include "stdafx.h"
#include "roadmap_gui.h"
#include "feature_base.h"
#include "feature_cmd.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "gfx_func.h"
#include "palette_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "widgets/roadmap_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/** Name of each track, indexed by #WorkTrack. */
static const StringID _work_track_names[] = {
	STR_WORK_TRACK_ENGINEERING,
	STR_WORK_TRACK_BUSINESS,
	STR_WORK_TRACK_SALES,
};
static_assert(std::size(_work_track_names) == to_underlying(WorkTrack::End));

/** Window listing a company's features: in progress first, then backlog, then shipped. */
struct RoadmapWindow : public Window {
	Scrollbar *vscroll = nullptr; ///< Scrollbar of the list.
	FeatureID selected = FeatureID::Invalid(); ///< Currently selected feature.

	RoadmapWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_RM_SCROLLBAR);
		this->FinishInitNested(window_number);
		this->owner = static_cast<Owner>(this->window_number);
		this->OnInvalidateData(0);
	}

	CompanyID GetCompany() const { return static_cast<CompanyID>(this->window_number); }

	std::vector<const Feature *> GetFeatures() const
	{
		std::vector<const Feature *> list;
		for (const Feature *f : Feature::Iterate()) {
			if (f->company == this->window_number) list.push_back(f);
		}
		auto order = [](FeatureState s) { return s == FeatureState::InProgress ? 0 : (s == FeatureState::Backlog ? 1 : 2); };
		std::ranges::stable_sort(list, {}, [&order](const Feature *f) { return order(f->state); });
		return list;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_RM_CAPTION) return GetString(STR_ROADMAP_CAPTION, this->window_number);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_RM_LIST:
				resize.width = 1;
				fill.height = resize.height = GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(4);
				size.height = 8 * resize.height + WidgetDimensions::scaled.framerect.Vertical();
				size.width = std::max<uint>(size.width, ScaleGUITrad(520));
				break;

			case WID_RM_SUMMARY:
				size.height = GetCharacterHeight(FontSize::Normal) + WidgetDimensions::scaled.framerect.Vertical();
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_RM_LIST: this->DrawList(r); break;

			case WID_RM_SUMMARY: {
				CompanyID company = this->GetCompany();
				DrawString(r.Shrink(WidgetDimensions::scaled.framerect), GetString(STR_ROADMAP_SUMMARY,
						CountAssignedStaff(company, WorkTrack::Engineering), CountTrackStaff(company, WorkTrack::Engineering),
						CountAssignedStaff(company, WorkTrack::Business), CountTrackStaff(company, WorkTrack::Business),
						CountAssignedStaff(company, WorkTrack::Sales), CountTrackStaff(company, WorkTrack::Sales)));
				break;
			}
		}
	}

	void DrawList(const Rect &r) const
	{
		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		const int row_h = this->resize.step_height;
		const int text_h = GetCharacterHeight(FontSize::Normal);
		const auto features = this->GetFeatures();

		if (features.empty()) {
			DrawString(ir, STR_ROADMAP_NONE);
			return;
		}

		const int w = ir.Width();
		const int x_cat = w * 34 / 100, x_bar = w * 52 / 100, x_status = w * 70 / 100;

		int pos = -this->vscroll->GetPosition();
		const int cap = this->vscroll->GetCapacity();
		for (const Feature *f : features) {
			if (pos >= 0 && pos < cap) {
				Rect row = ir.WithHeight(row_h);
				bool sel = f->index == this->selected;
				if (sel) GfxFillRect(row.left, row.top, row.right, row.bottom - 1, PC_DARK_GREY);
				TextColour tc = sel ? TextColour::White : TextColour::Black;
				int ty = row.top + (row_h - text_h) / 2;

				DrawString(row.left, row.left + x_cat - 4, ty, f->GetName(), tc);
				DrawString(row.left + x_cat, row.left + x_bar - 4, ty, _work_track_names[to_underlying(f->GetTrack())], tc);

				/* Progress bar. */
				int bx0 = row.left + x_bar, bx1 = row.left + x_status - ScaleGUITrad(8);
				int by0 = row.top + row_h / 2 - ScaleGUITrad(3), by1 = by0 + ScaleGUITrad(6);
				GfxFillRect(bx0, by0, bx1, by1, PC_BLACK);
				int filled = bx0 + (bx1 - bx0) * static_cast<int>(f->GetProgressPercent()) / 100;
				PixelColour bar = f->state == FeatureState::Shipped ? PC_GREEN : (f->assigned > 0 ? PC_LIGHT_BLUE : PC_GREY);
				if (filled > bx0) GfxFillRect(bx0, by0, filled, by1, bar);

				std::string status;
				switch (f->state) {
					case FeatureState::Backlog: status = GetString(STR_ROADMAP_STATUS_BACKLOG, f->effort); break;
					case FeatureState::InProgress: status = GetString(STR_ROADMAP_STATUS_PROGRESS, f->GetProgressPercent(), f->assigned); break;
					case FeatureState::Shipped: status = GetString(STR_ROADMAP_STATUS_SHIPPED, f->quality, f->bugs); break;
				}
				TextColour sc = tc;
				if (!sel && f->state == FeatureState::Shipped) sc = f->bugs > 3 ? TextColour::Red : TextColour::DarkGreen;
				DrawString(row.left + x_status, row.right, ty, status, sc);
				ir.top += row_h;
			}
			pos++;
		}
	}

	const Feature *GetSelected() const
	{
		const Feature *f = Feature::GetIfValid(this->selected);
		return (f != nullptr && f->company == this->window_number) ? f : nullptr;
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_RM_LIST: {
				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_RM_LIST, WidgetDimensions::scaled.framerect.top);
				const auto features = this->GetFeatures();
				this->selected = (row >= 0 && row < static_cast<int>(features.size())) ? features[row]->index : FeatureID::Invalid();
				this->OnInvalidateData(0);
				break;
			}

			case WID_RM_NEW: {
				CompanyID company = this->GetCompany();
				DropDownList list;
				for (uint i = 0; i < GetWorkItemCount(); i++) {
					const WorkItemSpec &spec = GetWorkItemSpec(i);
					StringID track = _work_track_names[to_underlying(spec.track)];
					switch (GetWorkItemAvailability(company, i)) {
						case WorkItemAvailability::Planned: break;
						case WorkItemAvailability::Available:
							list.push_back(MakeDropDownListStringItem(GetString(STR_ROADMAP_NEW_ITEM, track, spec.name, spec.effort), i));
							break;
						case WorkItemAvailability::Locked:
							list.push_back(MakeDropDownListStringItem(GetString(STR_ROADMAP_NEW_ITEM_LOCKED, track, spec.name, GetWorkItemPrereqText(company, i)), i, true));
							break;
					}
				}
				if (list.empty()) list.push_back(MakeDropDownListStringItem(STR_ROADMAP_NEW_ALL_PLANNED, -1, true));
				ShowDropDownList(this, std::move(list), -1, WID_RM_NEW);
				break;
			}

			case WID_RM_ADD_ENGINEER:
			case WID_RM_REMOVE_ENGINEER: {
				const Feature *f = this->GetSelected();
				if (f == nullptr) break;
				int n = f->assigned + (widget == WID_RM_ADD_ENGINEER ? 1 : -1);
				if (n < 0 || n > 255) break;
				Command<Commands::AssignFeature>::Post(STR_ERROR_CAN_T_ASSIGN_FEATURE, f->index, static_cast<uint8_t>(n));
				break;
			}

			case WID_RM_SHIP: {
				const Feature *f = this->GetSelected();
				if (f != nullptr) Command<Commands::ShipFeature>::Post(STR_ERROR_CAN_T_SHIP_FEATURE, f->index);
				break;
			}
		}
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		if (widget != WID_RM_NEW || index < 0) return;
		Command<Commands::CreateFeature>::Post(STR_ERROR_CAN_T_CREATE_FEATURE, static_cast<uint8_t>(index));
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_RM_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		const Feature *f = this->GetSelected();
		if (f == nullptr) this->selected = FeatureID::Invalid();

		this->vscroll->SetCount(this->GetFeatures().size());
		bool own = this->window_number == _local_company;
		bool open = f != nullptr && f->state != FeatureState::Shipped;
		this->SetWidgetDisabledState(WID_RM_NEW, !own);
		this->SetWidgetDisabledState(WID_RM_ADD_ENGINEER, !own || !open);
		this->SetWidgetDisabledState(WID_RM_REMOVE_ENGINEER, !own || !open || f->assigned == 0);
		this->SetWidgetDisabledState(WID_RM_SHIP, !own || !open || f->GetProgressPercent() < SHIP_EARLY_MIN_PERCENT);
		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_roadmap_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_RM_CAPTION),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_DEFSIZEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Brown, WID_RM_LIST), SetToolTip(STR_ROADMAP_LIST_TOOLTIP), SetScrollbar(WID_RM_SCROLLBAR), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::Brown, WID_RM_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_RM_SUMMARY), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_DROPDOWN, Colours::Brown, WID_RM_NEW), SetStringTip(STR_ROADMAP_NEW, STR_ROADMAP_NEW_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_RM_ADD_ENGINEER), SetStringTip(STR_ROADMAP_ADD_ENGINEER, STR_ROADMAP_ADD_ENGINEER_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_RM_REMOVE_ENGINEER), SetStringTip(STR_ROADMAP_REMOVE_ENGINEER, STR_ROADMAP_REMOVE_ENGINEER_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_RM_SHIP), SetStringTip(STR_ROADMAP_SHIP, STR_ROADMAP_SHIP_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _roadmap_desc(
	WindowPosition::Automatic, "founder_roadmap", 560, 260,
	WindowClass::Roadmap, WindowClass::None,
	{},
	_nested_roadmap_widgets
);

/**
 * Open the roadmap window of a company.
 * @param company The company whose roadmap to show.
 */
void ShowRoadmapWindow(CompanyID company)
{
	if (!Company::IsValidID(company)) return;
	AllocateWindowDescFront<RoadmapWindow>(_roadmap_desc, company);
}
