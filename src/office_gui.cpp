/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file office_gui.cpp Founder Mode office window: an isometric view of the team at their desks. */

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

#include "safeguards.h"

/** Maps office floor coordinates (in tiles) to screen pixels, 2:1 isometric like the game map. */
struct OfficeIso {
	int ox; ///< Screen x of the floor's back corner.
	int oy; ///< Screen y of the floor's back corner.
	int tw; ///< Width of one floor tile in pixels.

	Point operator()(float i, float j) const
	{
		return { this->ox + static_cast<int>((i - j) * this->tw / 2), this->oy + static_cast<int>((i + j) * this->tw / 4) };
	}
};

/** Draw a box standing on the office floor. */
static void DrawOfficeBox(const OfficeIso &iso, float i, float j, float w, float d, int h, Colours colour)
{
	Point a = iso(i, j), b = iso(i + w, j), c = iso(i + w, j + d), e = iso(i, j + d);
	auto up = [h](Point p) { return Point{p.x, p.y - h}; };

	const std::array<Point, 4> right = {b, c, up(c), up(b)};
	const std::array<Point, 4> left = {e, c, up(c), up(e)};
	const std::array<Point, 4> top = {up(a), up(b), up(c), up(e)};
	GfxFillPolygon(right, GetColourGradient(colour, Shade::Dark));
	GfxFillPolygon(left, GetColourGradient(colour, Shade::Normal));
	GfxFillPolygon(top, GetColourGradient(colour, Shade::Lighter));
}

/** Draw a person standing on the floor. */
static void DrawOfficePerson(const OfficeIso &iso, float i, float j, Colours colour)
{
	Point p = iso(i, j);
	int bw = std::max(2, iso.tw / 10), bh = std::max(4, iso.tw / 4), hd = std::max(2, iso.tw / 10);
	GfxFillRect(p.x - bw, p.y - bh, p.x + bw, p.y, GetColourGradient(colour, Shade::Normal));
	GfxFillRect(p.x - hd, p.y - bh - 2 * hd - 1, p.x + hd, p.y - bh - 1, GetColourGradient(Colours::Cream, Shade::Lighter));
}

/** Shirt colour of each role, indexed by #EmployeeRole. */
static const Colours _role_colours[] = { Colours::Blue, Colours::Purple, Colours::Orange, Colours::Green };
static_assert(std::size(_role_colours) == to_underlying(EmployeeRole::End));

/** Window showing a company's office. */
struct OfficeWindow : public Window {
	OfficeWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->owner = static_cast<Owner>(this->window_number);
		this->OnInvalidateData(0);
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

	void DrawOffice(const Rect &r) const
	{
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), GetColourGradient(Colours::Grey, Shade::Darker));

		const CompanyID company = this->GetCompany();
		const uint8_t level = GetOfficeLevel(company);
		const int n = level == 0 ? 4 : (level == 1 ? 6 : 9); // Floor size in tiles.
		const int wall = ScaleGUITrad(level == 0 ? 22 : 30);
		const int margin = ScaleGUITrad(10);

		int tw = std::min((r.Width() - 2 * margin) / n, (r.Height() - wall - 2 * margin) * 2 / n);
		tw = std::max(8, tw & ~3);
		OfficeIso iso{r.left + r.Width() / 2, r.top + margin + wall + (r.Height() - 2 * margin - wall - n * tw / 2) / 2, tw};

		/* Floor: concrete in the garage, wood in the loft, carpet on the office floor. */
		const Colours floor = level == 0 ? Colours::Grey : (level == 1 ? Colours::Brown : Colours::DarkBlue);
		for (int i = 0; i < n; i++) {
			for (int j = 0; j < n; j++) {
				const std::array<Point, 4> tile = {iso(i, j), iso(i + 1, j), iso(i + 1, j + 1), iso(i, j + 1)};
				GfxFillPolygon(tile, GetColourGradient(floor, (i + j) % 2 == 0 ? Shade::Light : Shade::Normal));
			}
		}

		/* Back walls. */
		DrawOfficeBox(iso, 0, 0, n, 0.15f, wall, Colours::Cream);
		DrawOfficeBox(iso, 0, 0.15f, 0.15f, n - 0.15f, wall, Colours::Cream);

		/* Board room in the front corner of the office floor, in company colour. */
		const Company *c = Company::GetIfValid(company);
		const bool board_room = level == MAX_OFFICE_LEVEL;
		const float br = 3.0f; // Board room size.

		/* Lay out desks in rows; each desk seats two. */
		struct Desk { float i, j; };
		std::vector<Desk> desks;
		const uint seats = GetOfficeDesks(level);
		for (float j = 0.9f; j + 1.1f <= n && desks.size() * 2 < seats; j += 1.5f) {
			for (float i = 0.6f; i + 1.6f <= n && desks.size() * 2 < seats; i += 2.0f) {
				if (board_room && i + 1.4f > n - br && j + 0.6f > n - br) continue;
				desks.push_back({i, j});
			}
		}

		/* Seat employees in pool order. */
		struct Seat { float i, j; Colours colour; };
		std::vector<Seat> people;
		uint k = 0;
		for (const Employee *e : Employee::Iterate()) {
			if (e->company != company) continue;
			const Desk &d = desks[std::min<size_t>(k / 2, desks.size() - 1)];
			people.push_back({d.i + (k % 2 == 0 ? 0.35f : 1.05f), d.j + 0.95f, _role_colours[to_underlying(e->role)]});
			k++;
		}

		/* Draw back to front so nearer things cover farther ones. */
		struct Drawable { float depth; std::function<void()> draw; };
		std::vector<Drawable> list;
		const int desk_h = std::max(3, tw / 6);
		for (const Desk &d : desks) list.push_back({d.i + d.j + 1.0f, [&iso, d, desk_h] { DrawOfficeBox(iso, d.i, d.j, 1.4f, 0.6f, desk_h, Colours::Brown); }});
		for (const Seat &s : people) list.push_back({s.i + s.j, [&iso, s] { DrawOfficePerson(iso, s.i, s.j, s.colour); }});
		if (board_room && c != nullptr) {
			const Colours cc = c->colour;
			list.push_back({2.0f * n, [&iso, n, br, wall, cc] { DrawOfficeBox(iso, n - br + 0.2f, n - br + 0.2f, br - 0.4f, br - 0.4f, wall * 2 / 3, cc); }});
		}
		std::ranges::sort(list, {}, &Drawable::depth);
		for (const Drawable &dr : list) dr.draw();

		if (people.empty()) {
			DrawString(r.left, r.right, r.bottom - margin - GetCharacterHeight(FontSize::Normal), STR_OFFICE_EMPTY, TextColour::White, AlignmentH::Centre);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
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
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_OFFICE_CAPTION),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_DEFSIZEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_OFFICE_VIEW), SetResize(1, 1), EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_OFFICE_SUMMARY), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_OFFICE_UPGRADE), SetToolTip(STR_OFFICE_UPGRADE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_OFFICE_TEAM), SetStringTip(STR_OFFICE_TEAM, STR_OFFICE_TEAM_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
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
	AllocateWindowDescFront<OfficeWindow>(_office_desc, company);
}
