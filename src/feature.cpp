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

EmployeeRole GetTrackRole(WorkTrack track) { return _track_roles[to_underlying(track)]; }

/**
 * How many people can work on an item at once; bigger items take more.
 * @param spec Catalog item.
 * @return Slots.
 */
uint GetWorkItemSlots(uint8_t spec)
{
	return std::clamp<uint>(1 + GetWorkItemSpec(spec).effort / 10, 2, 6);
}

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
 * Daily progress of one person, in hundredths of a point.
 * Skill 50 at morale 75 gives half a point per day.
 * @param e The person.
 * @return Progress per day.
 */
uint GetPersonDailyProgress(const Employee *e)
{
	return e->skill * e->morale / 75;
}

/**
 * Daily progress of everyone on one item.
 * @param f The item.
 * @return Progress per day, in hundredths of a point.
 */
uint GetFeatureDailyProgress(const Feature *f)
{
	uint total = 0;
	for (const Employee *e : Employee::Iterate()) {
		if (e->feature == f->index) total += GetPersonDailyProgress(e);
	}
	return total;
}

uint GetDailyVelocity(CompanyID company, WorkTrack track)
{
	uint total = 0;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company != company || !Feature::IsValidID(e->feature)) continue;
		if (Feature::Get(e->feature)->GetTrack() == track) total += GetPersonDailyProgress(e);
	}
	return total;
}

/** Ship a work item: quality and bugs depend on how complete it is and who built it. */
static void ShipFeature(Feature *f, const TrackStaffStats &s)
{
	/* The crew's skill counts; juniors add bugs. */
	uint crew = 0;
	uint skill = 0;
	uint juniors = 0;
	for (const Employee *e : Employee::Iterate()) {
		if (e->feature != f->index) continue;
		crew++;
		skill += e->skill;
		if (e->level == EmployeeLevel::Junior) juniors++;
	}
	skill = crew > 0 ? skill / crew : s.skill;

	uint pct = f->GetProgressPercent();
	uint design_bonus = f->GetTrack() == WorkTrack::Engineering ? std::min<uint>(s.designers, 3) * 10 : 20;
	uint base = 40 + design_bonus + skill * 3 / 10;
	f->quality = static_cast<uint8_t>(Clamp<uint>(base * pct / 100, 1, 100));
	f->bugs = static_cast<uint8_t>(std::min<uint>(UINT8_MAX, (100 - pct) / 8 + RandomRange(3) + juniors));
	f->state = FeatureState::Shipped;
	ReleaseFeatureStaff(f->index);
	f->assigned = 0;
	Debug(Facility::Misc, Severity::Info, "Founder Mode: company {} shipped '{}' at {}% (quality {}, bugs {})", f->company + 1, f->GetName(), pct, f->quality, f->bugs);
	InvalidateWindowData(WindowClass::Roadmap, f->company);
}

/** Every economy day, staff make progress on their track's work; finished items ship. */
static const IntervalTimer<TimerGameEconomy> _economy_features_daily({TimerGameEconomy::Trigger::Day, TimerGameEconomy::Priority::Founder}, [](auto)
{
	if (!_settings_game.game_creation.founder_mode) return;

	/* Each person adds their own progress to the item they work on. */
	for (const Employee *e : Employee::Iterate()) {
		Feature *f = Feature::GetIfValid(e->feature);
		if (f != nullptr && f->state == FeatureState::InProgress) f->progress += GetPersonDailyProgress(e);
	}
	for (Feature *f : Feature::Iterate()) {
		if (f->state == FeatureState::InProgress && f->progress >= static_cast<uint32_t>(f->effort) * 100) {
			ShipFeature(f, GetTrackStaffStats(f->company, f->GetTrack()));
		}
	}
	for (const Company *c : Company::Iterate()) InvalidateWindowData(WindowClass::Roadmap, c->index);
});

/**
 * Mark a catalog item as already done for a company, e.g. as a starting bonus.
 * @param company The company.
 * @param spec Catalog item.
 */
void GrantShippedWorkItem(CompanyID company, uint8_t spec)
{
	if (spec >= GetWorkItemCount() || FindWorkItem(company, spec) != nullptr || !Feature::CanAllocateItem()) return;
	Feature *f = Feature::Create(company, spec);
	f->effort = GetWorkItemSpec(spec).effort;
	f->progress = static_cast<uint32_t>(f->effort) * 100;
	f->state = FeatureState::Shipped;
	f->quality = 70;
}

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
		ReleaseFeatureStaff(f->index);
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

	if (people > GetWorkItemSlots(f->spec)) return CommandCost(STR_ERROR_WORK_ITEM_FULL);

	/* Add the most skilled free people, or take off the least skilled. */
	const EmployeeRole role = GetTrackRole(f->GetTrack());
	std::vector<Employee *> pool;
	for (Employee *e : Employee::Iterate()) {
		if (e->company != _current_company || e->role != role) continue;
		if (people > f->assigned ? IsEmployeeFree(e) : e->feature == f->index) pool.push_back(e);
	}
	std::ranges::sort(pool, [&](const Employee *a, const Employee *b) {
		return people > f->assigned ? a->skill > b->skill : a->skill < b->skill;
	});
	uint change = people > f->assigned ? people - f->assigned : f->assigned - people;
	if (pool.size() < change) return CommandCost(STR_ERROR_NO_FREE_STAFF);

	if (flags.Test(DoCommandFlag::Execute)) {
		for (uint i = 0; i < change; i++) SetEmployeeWork(pool[i], people > f->assigned ? f->index : FeatureID::Invalid());
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
