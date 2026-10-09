/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file feature.cpp Founder Mode product features: planning, building and shipping. */

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

/** Example feature names per category, indexed by #FeatureCategory. */
static const std::array<std::array<std::string_view, 4>, to_underlying(FeatureCategory::End)> _feature_names = {{
	{ "Onboarding v2", "Search", "Dark mode", "Team workspaces" },
	{ "iOS app", "Android app", "Offline mode", "Push notifications" },
	{ "Subscriptions", "Invoices", "Multi-currency", "Usage billing" },
	{ "Dashboards", "CSV export", "Cohort reports", "Forecasting" },
	{ "Slack integration", "Public API", "Webhooks", "Calendar sync" },
	{ "Single sign-on", "Audit log", "Two-factor login", "Data encryption" },
}};

/** Base effort in points per category, indexed by #FeatureCategory. */
static const uint16_t _feature_base_effort[] = { 24, 40, 32, 20, 28, 36 };
static_assert(std::size(_feature_base_effort) == to_underlying(FeatureCategory::End));

std::string Feature::GetName() const
{
	return std::string(_feature_names[to_underlying(this->category)][this->name_index % 4]);
}

/** Average skill and morale of a company's engineers. */
struct EngineerStats {
	uint count = 0; ///< Number of engineers.
	uint skill = 0; ///< Average skill.
	uint morale = 0; ///< Average morale.
	uint designers = 0; ///< Number of designers.
};

static EngineerStats GetEngineerStats(CompanyID company)
{
	EngineerStats s;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company != company) continue;
		if (e->role == EmployeeRole::Designer) s.designers++;
		if (e->role != EmployeeRole::Engineer) continue;
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

/**
 * Count engineers assigned to unshipped features.
 * @param company The company.
 * @return Assigned engineers.
 */
uint CountAssignedEngineers(CompanyID company)
{
	uint n = 0;
	for (const Feature *f : Feature::Iterate()) {
		if (f->company == company && f->state != FeatureState::Shipped) n += f->assigned;
	}
	return n;
}

/**
 * Daily progress, in hundredths of a point, that one engineer of average skill and morale adds.
 * Skill 50 at morale 75 gives half a point per day.
 */
static uint GetProgressPerEngineer(const EngineerStats &s)
{
	return s.skill * s.morale / 75;
}

/**
 * Total daily progress of a company, in hundredths of a point.
 * @param company The company.
 * @return Daily velocity.
 */
uint GetDailyVelocity(CompanyID company)
{
	return CountAssignedEngineers(company) * GetProgressPerEngineer(GetEngineerStats(company));
}

/** Ship a feature: set quality and bugs from how complete it is and who built it. */
static void ShipFeature(Feature *f, const EngineerStats &s)
{
	uint pct = f->GetProgressPercent();
	uint base = 40 + std::min<uint>(s.designers, 3) * 10 + s.skill * 3 / 10;
	f->quality = static_cast<uint8_t>(Clamp<uint>(base * pct / 100, 1, 100));
	f->bugs = static_cast<uint8_t>((100 - pct) / 8 + RandomRange(3));
	f->state = FeatureState::Shipped;
	f->assigned = 0;
	Debug(Facility::Misc, Severity::Info, "Founder Mode: company {} shipped '{}' at {}% (quality {}, bugs {})", f->company + 1, f->GetName(), pct, f->quality, f->bugs);
	InvalidateWindowData(WindowClass::Roadmap, f->company);
}

/** Every economy day, engineers make progress; finished features ship. */
static const IntervalTimer<TimerGameEconomy> _economy_features_daily({TimerGameEconomy::Trigger::Day, TimerGameEconomy::Priority::Founder}, [](auto)
{
	if (!_settings_game.game_creation.founder_mode) return;

	for (const Company *c : Company::Iterate()) {
		EngineerStats s = GetEngineerStats(c->index);
		uint per_engineer = GetProgressPerEngineer(s);
		bool changed = false;
		for (Feature *f : Feature::Iterate()) {
			if (f->company != c->index || f->state != FeatureState::InProgress || f->assigned == 0) continue;
			f->progress += f->assigned * per_engineer;
			changed = true;
			if (f->progress >= static_cast<uint32_t>(f->effort) * 100) ShipFeature(f, s);
		}
		if (changed) InvalidateWindowData(WindowClass::Roadmap, c->index);
	}
});

/**
 * Move or remove features when a company is taken over or closed.
 * @param old_owner The company that is going away.
 * @param new_owner The company taking over, or #INVALID_OWNER when the company closes.
 */
void ChangeFeatureOwnership(CompanyID old_owner, CompanyID new_owner)
{
	for (Feature *f : Feature::Iterate()) {
		if (f->company != old_owner) continue;
		if (new_owner == INVALID_OWNER) {
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
 * Add a feature to the backlog.
 * @param flags Type of operation.
 * @param category Kind of feature.
 * @return The cost of this operation or an error.
 */
CommandCost CmdCreateFeature(DoCommandFlags flags, FeatureCategory category)
{
	if (!_settings_game.game_creation.founder_mode) return CommandCost(STR_ERROR_FOUNDER_MODE_ONLY);
	if (category >= FeatureCategory::End) return CMD_ERROR;
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;

	uint open = 0;
	for (const Feature *f : Feature::Iterate()) {
		if (f->company == _current_company && f->state != FeatureState::Shipped) open++;
	}
	if (!Feature::CanAllocateItem() || open >= MAX_OPEN_FEATURES_PER_COMPANY) return CommandCost(STR_ERROR_ROADMAP_FULL);

	if (flags.Test(DoCommandFlag::Execute)) {
		Feature *f = Feature::Create(_current_company, category);
		uint16_t base = _feature_base_effort[to_underlying(category)];
		f->name_index = RandomRange(4);
		f->effort = base + RandomRange(base / 2 + 1);
		InvalidateWindowData(WindowClass::Roadmap, _current_company);
	}

	return CommandCost();
}

/**
 * Set how many engineers work on a feature.
 * @param flags Type of operation.
 * @param feature The feature.
 * @param engineers Number of engineers.
 * @return The cost of this operation or an error.
 */
CommandCost CmdAssignFeature(DoCommandFlags flags, FeatureID feature, uint8_t engineers)
{
	Feature *f = Feature::GetIfValid(feature);
	if (f == nullptr || f->company != _current_company) return CMD_ERROR;
	if (f->state == FeatureState::Shipped) return CommandCost(STR_ERROR_FEATURE_SHIPPED);

	uint others = CountAssignedEngineers(_current_company) - f->assigned;
	if (others + engineers > GetEngineerStats(_current_company).count) return CommandCost(STR_ERROR_NO_FREE_ENGINEER);

	if (flags.Test(DoCommandFlag::Execute)) {
		f->assigned = engineers;
		if (engineers > 0 && f->state == FeatureState::Backlog) f->state = FeatureState::InProgress;
		InvalidateWindowData(WindowClass::Roadmap, _current_company);
	}

	return CommandCost();
}

/**
 * Ship a feature before it is finished. Unfinished work lowers quality and adds bugs.
 * @param flags Type of operation.
 * @param feature The feature.
 * @return The cost of this operation or an error.
 */
CommandCost CmdShipFeature(DoCommandFlags flags, FeatureID feature)
{
	Feature *f = Feature::GetIfValid(feature);
	if (f == nullptr || f->company != _current_company) return CMD_ERROR;
	if (f->state == FeatureState::Shipped) return CommandCost(STR_ERROR_FEATURE_SHIPPED);
	if (f->GetProgressPercent() < SHIP_EARLY_MIN_PERCENT) return CommandCost(STR_ERROR_FEATURE_TOO_EARLY);

	if (flags.Test(DoCommandFlag::Execute)) ShipFeature(f, GetEngineerStats(_current_company));

	return CommandCost();
}
