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
#include "employee_base.h"
#include "employee_cmd.h"
#include "town.h"
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
#include "core/backup_type.hpp"

#include "widgets/roadmap_widget.h"

#include "table/strings.h"

#include "founder_gui.h"

#include "safeguards.h"

/** Short level names for crew lists, indexed by #EmployeeLevel. */
static const StringID _level_short_names[] = {
	STR_TEAM_LEVEL_JUNIOR,
	STR_TEAM_LEVEL_MID,
	STR_TEAM_LEVEL_SENIOR,
};

/**
 * What shipping an item does, in a few words.
 * @param spec Catalog item.
 * @param town Town of a city work item.
 * @return The impact text, empty for setup work.
 */
std::string GetWorkImpactText(uint8_t spec, TownID town)
{
	const WorkItemSpec &ws = GetWorkItemSpec(spec);
	switch (ws.impact) {
		case WorkImpact::Fit: return GetString(STR_WORK_IMPACT_FIT, STR_FEATURE_CATEGORY_CORE + to_underlying(ws.category));
		case WorkImpact::Price: return GetString(STR_WORK_IMPACT_PRICE, ws.impact_value);
		case WorkImpact::Churn: return GetString(STR_WORK_IMPACT_CHURN, ws.impact_value);
		case WorkImpact::Reach: return GetString(STR_WORK_IMPACT_REACH, ws.impact_value);
		case WorkImpact::LocalFit: return Town::IsValidID(town) ? GetString(STR_WORK_IMPACT_LOCAL_FIT, ws.impact_value, town) : GetString(STR_WORK_IMPACT_LOCAL_FIT_ANY, ws.impact_value);
		case WorkImpact::LocalReach: return Town::IsValidID(town) ? GetString(STR_WORK_IMPACT_LOCAL_REACH, ws.impact_value, town) : GetString(STR_WORK_IMPACT_LOCAL_REACH_ANY, ws.impact_value);
		default: return GetString(STR_WORK_IMPACT_NONE);
	}
}

/**
 * Size, impact and costs of an item, for planning lists.
 * @param spec Catalog item.
 * @param town Town of a city work item.
 * @return The details.
 */
std::string GetWorkItemDetails(uint8_t spec, TownID town)
{
	const WorkItemSpec &ws = GetWorkItemSpec(spec);
	return GetString(STR_WORK_DETAILS, ws.effort, GetWorkItemSlots(spec), GetWorkImpactText(spec, town), ws.test_cost, ws.run_cost);
}

/** Name of each track, indexed by #WorkTrack. */
static const StringID _work_track_names[] = {
	STR_WORK_TRACK_ENGINEERING,
	STR_WORK_TRACK_BUSINESS,
	STR_WORK_TRACK_SALES,
};
static_assert(std::size(_work_track_names) == to_underlying(WorkTrack::End));

/** Column of a catalog item in the tree: one more than its deepest prerequisite. */
static uint GetWorkItemDepth(uint8_t spec)
{
	uint depth = 0;
	for (uint8_t p : GetWorkItemSpec(spec).prereqs) {
		if (p != INVALID_WORK_ITEM) depth = std::max(depth, GetWorkItemDepth(p) + 1);
	}
	return depth;
}

/** Number of tree columns. */
static uint GetWorkTreeColumns()
{
	uint cols = 0;
	for (uint i = 0; i < GetWorkItemCount(); i++) {
		if (!GetWorkItemSpec(i).city) cols = std::max(cols, GetWorkItemDepth(i) + 1);
	}
	return cols;
}

/** Accent colour of each track, indexed by #WorkTrack. */
static const Colours _work_track_colours[] = { Colours::Blue, Colours::Purple, Colours::Orange };
static_assert(std::size(_work_track_colours) == to_underlying(WorkTrack::End));

/** Window with the company's work: a tree of the catalog by default, or a list. */
struct RoadmapWindow : public Window {
	Scrollbar *vscroll = nullptr; ///< Scrollbar of the list.
	Scrollbar *hscroll = nullptr; ///< Column scrollbar of the tree.
	FeatureID selected = FeatureID::Invalid(); ///< Currently selected feature.
	bool tree_view = true; ///< Show the tree (default) instead of the list.
	bool log_view = false; ///< Show the work log (shipped items by date) instead of the list.
	mutable std::vector<std::pair<uint8_t, Rect>> tree_cards; ///< Card rectangles from the last tree draw, for clicks.

	static int ColumnWidth() { return ScaleGUITrad(136); }
	static int CardWidth() { return ScaleGUITrad(124); }
	static int RowHeight() { return ScaleGUITrad(28); }
	static int CardHeight() { return ScaleGUITrad(22); }

	/** Roadmap entry of this company for a catalog item, if planned. */
	const Feature *FindPlanned(uint8_t spec) const
	{
		for (const Feature *f : Feature::Iterate()) {
			if (f->company == this->window_number && f->spec == spec) return f;
		}
		return nullptr;
	}

	/** Work out card positions in window coordinates. */
	void LayoutTree(const Rect &ir) const
	{
		this->tree_cards.clear();
		const int first = this->hscroll->GetPosition();
		const int header = GetCharacterHeight(FontSize::Small) + ScaleGUITrad(4);
		int y = ir.top;
		for (uint t = 0; t < to_underlying(WorkTrack::End); t++) {
			std::vector<uint> rows(GetWorkTreeColumns(), 0);
			uint max_rows = 0;
			for (uint i = 0; i < GetWorkItemCount(); i++) {
				if (to_underlying(GetWorkItemSpec(i).track) != t || GetWorkItemSpec(i).city) continue;
				uint d = GetWorkItemDepth(i);
				int x = ir.left + ScaleGUITrad(4) + (static_cast<int>(d) - first) * ColumnWidth();
				int cy = y + header + static_cast<int>(rows[d]) * RowHeight();
				this->tree_cards.push_back({static_cast<uint8_t>(i), Rect{x, cy, x + CardWidth() - 1, cy + CardHeight() - 1}});
				max_rows = std::max(max_rows, ++rows[d]);
			}
			y += header + static_cast<int>(max_rows) * RowHeight() + ScaleGUITrad(6);
		}
	}

	const Rect *FindCard(uint8_t spec) const
	{
		for (const auto &[s, rect] : this->tree_cards) {
			if (s == spec) return &rect;
		}
		return nullptr;
	}

	void DrawTree(const Rect &r) const
	{
		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		this->LayoutTree(ir);
		CompanyID company = this->GetCompany();

		DrawPixelInfo tmp_dpi;
		if (!FillDrawPixelInfo(&tmp_dpi, ir)) return;
		AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
		const int ox = ir.left, oy = ir.top;

		/* Lane headers and separators. */
		const int header = GetCharacterHeight(FontSize::Small) + ScaleGUITrad(4);
		int lane_top = 0;
		for (uint t = 0; t < to_underlying(WorkTrack::End); t++) {
			int lane_bottom = lane_top;
			for (const auto &[s, rect] : this->tree_cards) {
				if (to_underlying(GetWorkItemSpec(s).track) == t) lane_bottom = std::max(lane_bottom, rect.bottom - oy);
			}
			if (t > 0) GfxDrawLine(0, lane_top - ScaleGUITrad(3), ir.Width(), lane_top - ScaleGUITrad(3), PC_DARK_GREY, 1, 2);
			DrawString(ScaleGUITrad(4), ir.Width(), lane_top, _work_track_names[t], TextColour::Black, AlignmentH::Start, false, FontSize::Small);
			lane_top = lane_bottom + ScaleGUITrad(9);
		}
		(void)header;

		/* Prerequisite lines, dashed across lanes. */
		for (const auto &[s, rect] : this->tree_cards) {
			const WorkItemSpec &spec = GetWorkItemSpec(s);
			for (uint8_t p : spec.prereqs) {
				if (p == INVALID_WORK_ITEM) continue;
				const Rect *from = this->FindCard(p);
				if (from == nullptr) continue;
				bool cross = GetWorkItemSpec(p).track != spec.track;
				PixelColour colour = HasShippedWorkItem(company, p) ? PC_GREEN : PC_DARK_GREY;
				GfxDrawLine(from->right - ox, (from->top + from->bottom) / 2 - oy, rect.left - ox, (rect.top + rect.bottom) / 2 - oy, colour, 1, cross ? 3 : 0);
			}
		}

		/* Cards. */
		for (const auto &[s, rect] : this->tree_cards) {
			const WorkItemSpec &spec = GetWorkItemSpec(s);
			const Feature *f = this->FindPlanned(s);
			WorkItemAvailability avail = GetWorkItemAvailability(company, s);
			Rect c{rect.left - ox, rect.top - oy, rect.right - ox, rect.bottom - oy};

			PixelColour fill;
			TextColour tc = TextColour::Black;
			if (f != nullptr && f->state == FeatureState::Shipped) {
				fill = GetColourGradient(Colours::Green, Shade::Lighter);
			} else if (f != nullptr && f->state == FeatureState::InProgress) {
				fill = GetColourGradient(Colours::LightBlue, Shade::Lighter);
			} else if (f != nullptr) {
				fill = GetColourGradient(Colours::Grey, Shade::Lightest);
			} else if (avail == WorkItemAvailability::Available) {
				fill = GetColourGradient(Colours::Cream, Shade::Lighter);
			} else if (avail == WorkItemAvailability::Excluded) {
				fill = GetColourGradient(Colours::Red, Shade::Darker);
				tc = TextColour::Silver;
			} else {
				fill = GetColourGradient(Colours::Grey, Shade::Dark);
				tc = TextColour::Silver;
			}
			GfxFillRect(c.left, c.top, c.right, c.bottom, fill);
			GfxFillRect(c.left, c.top, c.left + ScaleGUITrad(3), c.bottom, GetColourGradient(_work_track_colours[to_underlying(spec.track)], Shade::Normal));
			if (f != nullptr && f->state == FeatureState::InProgress) {
				int w = (c.Width() - ScaleGUITrad(6)) * static_cast<int>(f->GetProgressPercent()) / 100;
				GfxFillRect(c.left + ScaleGUITrad(4), c.bottom - ScaleGUITrad(2), c.left + ScaleGUITrad(4) + w, c.bottom, PC_DARK_BLUE);
			}
			if (f != nullptr && f->index == this->selected) {
				GfxDrawLine(c.left, c.top, c.right, c.top, PC_WHITE);
				GfxDrawLine(c.left, c.bottom, c.right, c.bottom, PC_WHITE);
				GfxDrawLine(c.left, c.top, c.left, c.bottom, PC_WHITE);
				GfxDrawLine(c.right, c.top, c.right, c.bottom, PC_WHITE);
			}
			int ty = c.top + (c.Height() - GetCharacterHeight(FontSize::Small)) / 2;
			DrawString(c.left + ScaleGUITrad(6), c.right - ScaleGUITrad(2), ty, std::string(spec.name), tc, AlignmentH::Start, false, FontSize::Small);
		}
	}

	RoadmapWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_RM_SCROLLBAR);
		this->hscroll = this->GetScrollbar(WID_RM_HSCROLLBAR);
		this->hscroll->SetCount(GetWorkTreeColumns());
		this->FinishInitNested(window_number);
		this->owner = static_cast<Owner>(this->window_number);
		this->OnInvalidateData(0);
		this->LowerWidget(WID_RM_TAB_WORK);
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

	std::vector<const Feature *> GetFeatures() const
	{
		std::vector<const Feature *> list;
		for (const Feature *f : Feature::Iterate()) {
			if (f->company == this->window_number) list.push_back(f);
		}
		if (this->log_view) {
			/* The work log: shipped items, newest first. */
			std::erase_if(list, [](const Feature *f) { return f->state != FeatureState::Shipped; });
			std::ranges::stable_sort(list, std::greater{}, [](const Feature *f) { return f->shipped_date; });
			return list;
		}
		auto order = [](FeatureState s) { return s == FeatureState::InProgress ? 0 : (s == FeatureState::Backlog ? 1 : 2); };
		std::ranges::stable_sort(list, {}, [&order](const Feature *f) { return order(f->state); });
		return list;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_RM_CAPTION) return GetString(STR_ROADMAP_CAPTION, this->window_number);
		if (widget == WID_RM_VIEW) return GetString(this->tree_view ? STR_ROADMAP_VIEW_LIST : (this->log_view ? STR_ROADMAP_VIEW_TREE : STR_ROADMAP_VIEW_LOG));
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
			case WID_RM_LIST:
				if (this->tree_view) {
					this->DrawTree(r);
				} else if (this->log_view) {
					this->DrawLog(r);
				} else {
					this->DrawList(r);
				}
				break;

			case WID_RM_SUMMARY: {
				CompanyID company = this->GetCompany();
				/* The selected item's crew, by name. */
				if (const Feature *f = this->GetSelected(); f != nullptr && f->state != FeatureState::Shipped) {
					std::string crew;
					for (const Employee *e : Employee::Iterate()) {
						if (e->feature != f->index) continue;
						if (!crew.empty()) crew += ", ";
						crew += GetString(STR_ROADMAP_CREW_PERSON, e->GetName(), _level_short_names[to_underlying(e->level)]);
					}
					uint per_day = GetFeatureDailyProgress(f);
					uint32_t left = static_cast<uint32_t>(f->effort) * 100 - std::min<uint32_t>(f->progress, static_cast<uint32_t>(f->effort) * 100);
					DrawString(r.Shrink(WidgetDimensions::scaled.framerect), crew.empty()
							? GetString(STR_ROADMAP_CREW_NONE, f->GetName(), GetWorkItemDetails(f->spec, f->town))
							: GetString(STR_ROADMAP_CREW, crew, per_day / 100, per_day % 100 / 10, CeilDiv(left, std::max(per_day, 1U)), GetWorkImpactText(f->spec, f->town)));
					break;
				}
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
					case FeatureState::InProgress: status = GetString(STR_ROADMAP_STATUS_PROGRESS, f->GetProgressPercent(), f->assigned, GetWorkItemSlots(f->spec)); break;
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

	void DrawLog(const Rect &r) const
	{
		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		const int row_h = this->resize.step_height;
		const int text_h = GetCharacterHeight(FontSize::Normal);
		const auto features = this->GetFeatures();
		if (features.empty()) {
			DrawString(ir, STR_ROADMAP_LOG_NONE);
			return;
		}

		const int w = ir.Width();
		const int x_name = w * 16 / 100, x_impact = w * 56 / 100, x_quality = w * 84 / 100;
		int pos = -this->vscroll->GetPosition();
		const int cap = this->vscroll->GetCapacity();
		for (const Feature *f : features) {
			if (pos >= 0 && pos < cap) {
				Rect row = ir.WithHeight(row_h);
				bool sel = f->index == this->selected;
				if (sel) GfxFillRect(row.left, row.top, row.right, row.bottom - 1, PC_DARK_GREY);
				TextColour tc = sel ? TextColour::White : TextColour::Black;
				int ty = row.top + (row_h - text_h) / 2;
				if (f->shipped_date != TimerGameEconomy::Date{}) DrawString(row.left, row.left + x_name - 4, ty, GetString(STR_JUST_DATE_SHORT, f->shipped_date), tc);
				DrawString(row.left + x_name, row.left + x_impact - 4, ty, f->GetName(), tc);
				DrawString(row.left + x_impact, row.left + x_quality - 4, ty, GetWorkImpactText(f->spec, f->town), sel ? TextColour::White : TextColour::DarkGreen);
				DrawString(row.left + x_quality, row.right, ty, GetString(STR_ROADMAP_LOG_QUALITY, f->quality, f->bugs), f->bugs > 3 && !sel ? TextColour::Red : tc);
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
			case WID_RM_TAB_TEAM: ShowFounderTab(FounderTab::Team, static_cast<CompanyID>(this->window_number)); break;
			case WID_RM_TAB_OFFICE: ShowFounderTab(FounderTab::Office, static_cast<CompanyID>(this->window_number)); break;
			case WID_RM_TAB_WORK: break; // Already showing this tab.

			case WID_RM_VIEW:
				/* Tree, then list, then log. */
				if (this->tree_view) {
					this->tree_view = false;
				} else if (!this->log_view) {
					this->log_view = true;
				} else {
					this->log_view = false;
					this->tree_view = true;
				}
				this->OnInvalidateData(0);
				break;

			case WID_RM_LIST: {
				if (this->tree_view) {
					for (const auto &[s, rect] : this->tree_cards) {
						if (!rect.Contains(pt)) continue;
						const Feature *f = this->FindPlanned(s);
						if (f != nullptr) {
							this->selected = f->index;
							this->OnInvalidateData(0);
						} else if (this->window_number == _local_company) {
							Command<Commands::CreateFeature>::Post(STR_ERROR_CAN_T_CREATE_FEATURE, s, TownID::Invalid());
						}
						break;
					}
					break;
				}
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
					if (spec.city) continue; // Planned per town from the market window.
					StringID track = _work_track_names[to_underlying(spec.track)];
					switch (GetWorkItemAvailability(company, i)) {
						case WorkItemAvailability::Planned: break;
						case WorkItemAvailability::Available:
							list.push_back(MakeDropDownListStringItem(GetString(spec.fork != 0 ? STR_ROADMAP_NEW_ITEM_FORK : STR_ROADMAP_NEW_ITEM, track, spec.name, GetWorkItemDetails(i, TownID::Invalid())), i));
							break;
						case WorkItemAvailability::Excluded:
							list.push_back(MakeDropDownListStringItem(GetString(STR_ROADMAP_NEW_ITEM_EXCLUDED, track, spec.name, GetWorkItemSpec(GetWorkItemExcludedBy(company, i)).name), i, true));
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
				/* Pick a person by name: free people of the item's track to add, its crew to remove. */
				const Feature *f = this->GetSelected();
				if (f == nullptr) break;
				bool add = widget == WID_RM_ADD_ENGINEER;
				EmployeeRole role = GetTrackRole(f->GetTrack());
				DropDownList list;
				for (const Employee *e : Employee::Iterate()) {
					if (e->company != f->company || e->role != role) continue;
					if (add ? e->feature == f->index : e->feature != f->index) continue;
					uint per_day = GetPersonDailyProgress(e);
					StringID str = STR_ROADMAP_PERSON_ITEM;
					std::string where;
					if (add && Town::IsValidID(e->town)) {
						str = STR_ROADMAP_PERSON_ITEM_TOWN;
						where = GetString(STR_TOWN_NAME, e->town);
					} else if (add && Feature::IsValidID(e->feature)) {
						str = STR_ROADMAP_PERSON_ITEM_BUSY;
						where = Feature::Get(e->feature)->GetName();
					}
					list.push_back(MakeDropDownListStringItem(GetString(str, e->GetName(), _level_short_names[to_underlying(e->level)], per_day / 100, per_day % 100 / 10, where), e->index.base()));
				}
				if (list.empty()) list.push_back(MakeDropDownListStringItem(add ? STR_ROADMAP_NOBODY_FREE : STR_ROADMAP_NOBODY_ON_IT, -1, true));
				ShowDropDownList(this, std::move(list), -1, widget);
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
		if (index < 0) return;
		switch (widget) {
			case WID_RM_NEW:
				Command<Commands::CreateFeature>::Post(STR_ERROR_CAN_T_CREATE_FEATURE, static_cast<uint8_t>(index), TownID::Invalid());
				break;

			case WID_RM_ADD_ENGINEER:
				if (const Feature *f = this->GetSelected(); f != nullptr) Command<Commands::AssignWork>::Post(STR_ERROR_CAN_T_ASSIGN_FEATURE, EmployeeID(index), f->index);
				break;

			case WID_RM_REMOVE_ENGINEER:
				Command<Commands::AssignWork>::Post(STR_ERROR_CAN_T_ASSIGN_FEATURE, EmployeeID(index), FeatureID::Invalid());
				break;
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_RM_LIST, WidgetDimensions::scaled.framerect.Vertical());
		const NWidgetBase *list = this->GetWidget<NWidgetBase>(WID_RM_LIST);
		this->hscroll->SetCapacity(std::max(1, static_cast<int>(list->current_x) / ColumnWidth()));
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		const Feature *f = this->GetSelected();
		if (f == nullptr) this->selected = FeatureID::Invalid();

		this->vscroll->SetCount(this->tree_view ? 0 : this->GetFeatures().size());
		this->hscroll->SetCount(GetWorkTreeColumns());
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
		NWidget(WWT_CLOSEBOX, FOUNDER_COLOUR),
		NWidget(WWT_CAPTION, FOUNDER_COLOUR, WID_RM_CAPTION),
		NWidget(WWT_SHADEBOX, FOUNDER_COLOUR),
		NWidget(WWT_DEFSIZEBOX, FOUNDER_COLOUR),
		NWidget(WWT_STICKYBOX, FOUNDER_COLOUR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_RM_TAB_TEAM), SetStringTip(STR_FOUNDER_TAB_TEAM, STR_FOUNDER_TAB_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_RM_TAB_OFFICE), SetStringTip(STR_FOUNDER_TAB_OFFICE, STR_FOUNDER_TAB_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_TEXTBTN, FOUNDER_COLOUR, WID_RM_TAB_WORK), SetStringTip(STR_FOUNDER_TAB_WORK, STR_FOUNDER_TAB_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_RM_LIST), SetToolTip(STR_ROADMAP_LIST_TOOLTIP), SetScrollbar(WID_RM_SCROLLBAR), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, FOUNDER_COLOUR, WID_RM_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HSCROLLBAR, FOUNDER_COLOUR, WID_RM_HSCROLLBAR),
	NWidget(WWT_PANEL, FOUNDER_COLOUR, WID_RM_SUMMARY), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_RM_VIEW), SetToolTip(STR_ROADMAP_VIEW_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_DROPDOWN, FOUNDER_COLOUR, WID_RM_NEW), SetStringTip(STR_ROADMAP_NEW, STR_ROADMAP_NEW_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_DROPDOWN, FOUNDER_COLOUR, WID_RM_ADD_ENGINEER), SetStringTip(STR_ROADMAP_ADD_ENGINEER, STR_ROADMAP_ADD_ENGINEER_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_DROPDOWN, FOUNDER_COLOUR, WID_RM_REMOVE_ENGINEER), SetStringTip(STR_ROADMAP_REMOVE_ENGINEER, STR_ROADMAP_REMOVE_ENGINEER_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, FOUNDER_COLOUR, WID_RM_SHIP), SetStringTip(STR_ROADMAP_SHIP, STR_ROADMAP_SHIP_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, FOUNDER_COLOUR),
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
	CloseOtherFounderTabs(WindowClass::Roadmap, company);
	AllocateWindowDescFront<RoadmapWindow>(_roadmap_desc, company);
}
