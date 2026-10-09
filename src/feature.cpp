/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file feature.cpp Founder Mode work: a sequential catalog of engineering, business and sales items. */

#include "stdafx.h"
#include "feature_base.h"
#include "feature_cmd.h"
#include "employee_base.h"
#include "company_base.h"
#include "command_func.h"
#include "core/pool_func.hpp"
#include "core/random_func.hpp"
#include "debug.h"
#include "settings_type.h"
#include "timer/timer.h"
#include "timer/timer_game_economy.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

FeaturePool _feature_pool("Feature");
INSTANTIATE_POOL_METHODS(Feature)

static constexpr uint8_t NONE = INVALID_WORK_ITEM;

/**
 * The work catalog. Order matters: indices are saved in savegames and used as prerequisites,
 * so only ever append new items.
 */
static const WorkItemSpec _work_items[] = {
	/* Engineering: features and deployments. */
	/*  0 */ { WorkTrack::Engineering, FeatureCategory::Core,           20, "MVP prototype",             { NONE, NONE, NONE } },
	/*  1 */ { WorkTrack::Engineering, FeatureCategory::Core,           16, "User accounts",             {    0, NONE, NONE } },
	/*  2 */ { WorkTrack::Engineering, FeatureCategory::Infrastructure, 12, "First cloud deployment",    {    0, NONE, NONE } },
	/*  3 */ { WorkTrack::Engineering, FeatureCategory::Infrastructure, 14, "CI/CD pipeline",            {    2, NONE, NONE } },
	/*  4 */ { WorkTrack::Engineering, FeatureCategory::Core,           18, "Onboarding flow",           {    1, NONE, NONE } },
	/*  5 */ { WorkTrack::Engineering, FeatureCategory::Payments,       30, "Payments and billing",      {    1, NONE, NONE } },
	/*  6 */ { WorkTrack::Engineering, FeatureCategory::Infrastructure, 16, "Monitoring and alerts",     {    3, NONE, NONE } },
	/*  7 */ { WorkTrack::Engineering, FeatureCategory::Mobile,         40, "Mobile app",                {    4, NONE, NONE } },
	/*  8 */ { WorkTrack::Engineering, FeatureCategory::Integrations,   28, "Public API",                {    1,    3, NONE } },
	/*  9 */ { WorkTrack::Engineering, FeatureCategory::Analytics,      24, "Analytics dashboards",      {    5, NONE, NONE } },
	/* 10 */ { WorkTrack::Engineering, FeatureCategory::Security,       26, "Single sign-on",            {    1,    6, NONE } },
	/* 11 */ { WorkTrack::Engineering, FeatureCategory::Infrastructure, 36, "Multi-region deployment",   {    6, NONE, NONE } },
	/* 12 */ { WorkTrack::Engineering, FeatureCategory::Security,       30, "Audit log and compliance",  {   10, NONE, NONE } },
	/* Business: company setup, legal, fundraising. */
	/* 13 */ { WorkTrack::Business,    FeatureCategory::End,             8, "Incorporate the company",   { NONE, NONE, NONE } },
	/* 14 */ { WorkTrack::Business,    FeatureCategory::End,             6, "Bank account and bookkeeping", { 13, NONE, NONE } },
	/* 15 */ { WorkTrack::Business,    FeatureCategory::End,             8, "Terms of service and privacy", { 13, NONE, NONE } },
	/* 16 */ { WorkTrack::Business,    FeatureCategory::End,            10, "Payroll and HR setup",      {   14, NONE, NONE } },
	/* 17 */ { WorkTrack::Business,    FeatureCategory::End,            10, "Pitch deck",                {   13, NONE, NONE } },
	/* 18 */ { WorkTrack::Business,    FeatureCategory::End,            14, "Investor data room",        {   17,   14, NONE } },
	/* 19 */ { WorkTrack::Business,    FeatureCategory::End,            12, "Hiring pipeline",           {   16, NONE, NONE } },
	/* 20 */ { WorkTrack::Business,    FeatureCategory::End,            30, "SOC 2 readiness",           {   15,    6, NONE } },
	/* Sales: go-to-market. */
	/* 21 */ { WorkTrack::Sales,       FeatureCategory::End,             8, "Pricing page",              {    0, NONE, NONE } },
	/* 22 */ { WorkTrack::Sales,       FeatureCategory::End,            10, "Founder-led sales",         {   21, NONE, NONE } },
	/* 23 */ { WorkTrack::Sales,       FeatureCategory::End,             8, "CRM setup",                 {   22, NONE, NONE } },
	/* 24 */ { WorkTrack::Sales,       FeatureCategory::End,            14, "Sales playbook",            {   23, NONE, NONE } },
	/* 25 */ { WorkTrack::Sales,       FeatureCategory::End,            16, "Self-serve checkout",       {   21,    5, NONE } },
	/* 26 */ { WorkTrack::Sales,       FeatureCategory::End,            16, "Customer success team",     {   24, NONE, NONE } },
	/* 27 */ { WorkTrack::Sales,       FeatureCategory::End,            18, "Partner program",           {   24,    8, NONE } },
	/* 28 */ { WorkTrack::Sales,       FeatureCategory::End,            24, "Enterprise contracts",      {   24,   15,   10 } },
};

/** Role that staffs each track, indexed by #WorkTrack. */
static const EmployeeRole _track_roles[] = { EmployeeRole::Engineer, EmployeeRole::Operations, EmployeeRole::Sales };
static_assert(std::size(_track_roles) == to_underlying(WorkTrack::End));

uint GetWorkItemCount() { return static_cast<uint>(std::size(_work_items)); }

const WorkItemSpec &GetWorkItemSpec(uint8_t spec)
{
	return _work_items[std::min<uint>(spec, GetWorkItemCount() - 1)];
}

std::string Feature::GetName() const { return std::string(GetWorkItemSpec(this->spec).name); }
WorkTrack Feature::GetTrack() const { return GetWorkItemSpec(this->spec).track; }

/** Find a company's roadmap entry for a catalog item, if any. */
static const Feature *FindWorkItem(CompanyID company, uint8_t spec)
{
	for (const Feature *f : Feature::Iterate()) {
		if (f->company == company && f->spec == spec) return f;
	}
	return nullptr;
}

bool HasShippedWorkItem(CompanyID company, uint8_t spec)
{
	const Feature *f = FindWorkItem(company, spec);
	return f != nullptr && f->state == FeatureState::Shipped;
}

WorkItemAvailability GetWorkItemAvailability(CompanyID company, uint8_t spec)
{
	if (FindWorkItem(company, spec) != nullptr) return WorkItemAvailability::Planned;
	for (uint8_t p : GetWorkItemSpec(spec).prereqs) {
		if (p != NONE && !HasShippedWorkItem(company, p)) return WorkItemAvailability::Locked;
	}
	return WorkItemAvailability::Available;
}

/**
 * Names of the prerequisites that are not shipped yet, joined by commas.
 * @param company The company.
 * @param spec Catalog item.
 * @return Missing prerequisite names, empty when none.
 */
std::string GetWorkItemPrereqText(CompanyID company, uint8_t spec)
{
	std::string out;
	for (uint8_t p : GetWorkItemSpec(spec).prereqs) {
		if (p == NONE || HasShippedWorkItem(company, p)) continue;
		if (!out.empty()) out += ", ";
		out += GetWorkItemSpec(p).name;
	}
	return out;
}

/** Average skill and morale of the people staffing a track. */
struct TrackStaffStats {
	uint count = 0; ///< People in the track's role.
	uint skill = 0; ///< Their average skill.
	uint morale = 0; ///< Their average morale.
	uint designers = 0; ///< Designers in the company (raise engineering quality).
};

static TrackStaffStats GetTrackStaffStats(CompanyID company, WorkTrack track)
{
	const EmployeeRole role = _track_roles[to_underlying(track)];
	TrackStaffStats s;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company != company) continue;
		if (e->role == EmployeeRole::Designer) s.designers++;
		if (e->role != role) continue;
		s.count++;
		s.skill += e->skill;
		s.morale += e->morale;
	}
	if (s.count > 0) {
		s.skill /= s.count;
		s.morale /= s.count;
	}
	return s;
}

uint CountTrackStaff(CompanyID company, WorkTrack track)
{
	return GetTrackStaffStats(company, track).count;
}

uint CountAssignedStaff(CompanyID company, WorkTrack track)
{
	uint n = 0;
	for (const Feature *f : Feature::Iterate()) {
		if (f->company == company && f->state != FeatureState::Shipped && f->GetTrack() == track) n += f->assigned;
	}
	return n;
}

/**
 * Daily progress, in hundredths of a point, of one person of average skill and morale.
 * Skill 50 at morale 75 gives half a point per day.
 */
static uint GetProgressPerPerson(const TrackStaffStats &s)
{
	return s.skill * s.morale / 75;
}

uint GetDailyVelocity(CompanyID company, WorkTrack track)
{
	return CountAssignedStaff(company, track) * GetProgressPerPerson(GetTrackStaffStats(company, track));
}

/** Ship a work item: quality and bugs depend on how complete it is and who built it. */
static void ShipFeature(Feature *f, const TrackStaffStats &s)
{
	uint pct = f->GetProgressPercent();
	uint design_bonus = f->GetTrack() == WorkTrack::Engineering ? std::min<uint>(s.designers, 3) * 10 : 20;
	uint base = 40 + design_bonus + s.skill * 3 / 10;
	f->quality = static_cast<uint8_t>(Clamp<uint>(base * pct / 100, 1, 100));
	f->bugs = static_cast<uint8_t>((100 - pct) / 8 + RandomRange(3));
	f->state = FeatureState::Shipped;
	f->assigned = 0;
	Debug(Facility::Misc, Severity::Info, "Founder Mode: company {} shipped '{}' at {}% (quality {}, bugs {})", f->company + 1, f->GetName(), pct, f->quality, f->bugs);
	InvalidateWindowData(WindowClass::Roadmap, f->company);
}

/** Every economy day, staff make progress on their track's work; finished items ship. */
static const IntervalTimer<TimerGameEconomy> _economy_features_daily({TimerGameEconomy::Trigger::Day, TimerGameEconomy::Priority::Founder}, [](auto)
{
	if (!_settings_game.game_creation.founder_mode) return;

	for (const Company *c : Company::Iterate()) {
		std::array<TrackStaffStats, to_underlying(WorkTrack::End)> stats;
		for (uint t = 0; t < stats.size(); t++) stats[t] = GetTrackStaffStats(c->index, static_cast<WorkTrack>(t));

		bool changed = false;
		for (Feature *f : Feature::Iterate()) {
			if (f->company != c->index || f->state != FeatureState::InProgress || f->assigned == 0) continue;
			const TrackStaffStats &s = stats[to_underlying(f->GetTrack())];
			f->progress += f->assigned * GetProgressPerPerson(s);
			changed = true;
			if (f->progress >= static_cast<uint32_t>(f->effort) * 100) ShipFeature(f, s);
		}
		if (changed) InvalidateWindowData(WindowClass::Roadmap, c->index);
	}
});

/**
 * Move or remove work items when a company is taken over or closed.
 * @param old_owner The company that is going away.
 * @param new_owner The company taking over, or #INVALID_OWNER when the company closes.
 */
void ChangeFeatureOwnership(CompanyID old_owner, CompanyID new_owner)
{
	for (Feature *f : Feature::Iterate()) {
		if (f->company != old_owner) continue;
		/* The buyer keeps its own copy of an item it already has. */
		if (new_owner == INVALID_OWNER || FindWorkItem(new_owner, f->spec) != nullptr) {
			delete f;
		} else {
			f->company = new_owner;
			f->assigned = 0;
		}
	}
	InvalidateWindowData(WindowClass::Roadmap, old_owner);
	if (new_owner != INVALID_OWNER) InvalidateWindowData(WindowClass::Roadmap, new_owner);
}

/**
 * Add a catalog item to the roadmap. Its prerequisites must have shipped.
 * @param flags Type of operation.
 * @param spec Catalog item.
 * @return The cost of this operation or an error.
 */
CommandCost CmdCreateFeature(DoCommandFlags flags, uint8_t spec)
{
	if (!_settings_game.game_creation.founder_mode) return CommandCost(STR_ERROR_FOUNDER_MODE_ONLY);
	if (spec >= GetWorkItemCount()) return CMD_ERROR;
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;

	switch (GetWorkItemAvailability(_current_company, spec)) {
		case WorkItemAvailability::Planned: return CommandCost(STR_ERROR_WORK_ITEM_PLANNED);
		case WorkItemAvailability::Locked: return CommandCost(STR_ERROR_WORK_ITEM_LOCKED);
		case WorkItemAvailability::Available: break;
	}

	uint open = 0;
	for (const Feature *f : Feature::Iterate()) {
		if (f->company == _current_company && f->state != FeatureState::Shipped) open++;
	}
	if (!Feature::CanAllocateItem() || open >= MAX_OPEN_FEATURES_PER_COMPANY) return CommandCost(STR_ERROR_ROADMAP_FULL);

	if (flags.Test(DoCommandFlag::Execute)) {
		Feature *f = Feature::Create(_current_company, spec);
		f->effort = GetWorkItemSpec(spec).effort;
		InvalidateWindowData(WindowClass::Roadmap, _current_company);
	}

	return CommandCost();
}

/**
 * Set how many people work on an item. They come from the item's track role.
 * @param flags Type of operation.
 * @param feature The item.
 * @param people Number of people.
 * @return The cost of this operation or an error.
 */
CommandCost CmdAssignFeature(DoCommandFlags flags, FeatureID feature, uint8_t people)
{
	Feature *f = Feature::GetIfValid(feature);
	if (f == nullptr || f->company != _current_company) return CMD_ERROR;
	if (f->state == FeatureState::Shipped) return CommandCost(STR_ERROR_FEATURE_SHIPPED);

	WorkTrack track = f->GetTrack();
	uint others = CountAssignedStaff(_current_company, track) - f->assigned;
	if (others + people > CountTrackStaff(_current_company, track)) return CommandCost(STR_ERROR_NO_FREE_STAFF);

	if (flags.Test(DoCommandFlag::Execute)) {
		f->assigned = people;
		if (people > 0 && f->state == FeatureState::Backlog) f->state = FeatureState::InProgress;
		InvalidateWindowData(WindowClass::Roadmap, _current_company);
	}

	return CommandCost();
}

/**
 * Ship an item before it is finished. Unfinished work lowers quality and adds bugs.
 * @param flags Type of operation.
 * @param feature The item.
 * @return The cost of this operation or an error.
 */
CommandCost CmdShipFeature(DoCommandFlags flags, FeatureID feature)
{
	Feature *f = Feature::GetIfValid(feature);
	if (f == nullptr || f->company != _current_company) return CMD_ERROR;
	if (f->state == FeatureState::Shipped) return CommandCost(STR_ERROR_FEATURE_SHIPPED);
	if (f->GetProgressPercent() < SHIP_EARLY_MIN_PERCENT) return CommandCost(STR_ERROR_FEATURE_TOO_EARLY);

	if (flags.Test(DoCommandFlag::Execute)) ShipFeature(f, GetTrackStaffStats(_current_company, f->GetTrack()));

	return CommandCost();
}
