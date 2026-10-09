/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file feature_base.h Founder Mode product feature data. */

#ifndef FEATURE_BASE_H
#define FEATURE_BASE_H

#include "company_type.h"
#include "feature_type.h"
#include "employee_type.h"
#include "town_type.h"
#include "economy_type.h"
#include "timer/timer_game_economy.h"
#include "core/pool_type.hpp"

using FeaturePool = Pool<Feature, FeatureID, 64>;
extern FeaturePool _feature_pool;

/** A product feature on a company's roadmap. */
struct Feature : FeaturePool::PoolItem<&_feature_pool> {
	CompanyID company = CompanyID::Invalid(); ///< Company building the feature.
	uint8_t spec = 0; ///< Index into the work item catalog, see #GetWorkItemSpec.
	FeatureState state = FeatureState::Backlog; ///< Roadmap state.
	uint8_t assigned = 0; ///< Engineers working on it.
	uint8_t quality = 0; ///< Quality from 1 to 100, set when shipped.
	uint8_t bugs = 0; ///< Bugs shipped with the feature.
	uint16_t effort = 0; ///< Total effort in points.
	uint32_t progress = 0; ///< Effort done, in hundredths of a point.
	TownID town = TownID::Invalid(); ///< Town of a city work item, invalid for central work.
	TimerGameEconomy::Date shipped_date{}; ///< When it shipped, for the work log.

	Feature(FeatureID index, CompanyID company = CompanyID::Invalid(), uint8_t spec = 0) :
		FeaturePool::PoolItem<&_feature_pool>(index), company(company), spec(spec) {}

	~Feature() { }

	std::string GetName() const;
	WorkTrack GetTrack() const;

	/** @return Progress as a percentage of the effort, 0 to 100. */
	uint GetProgressPercent() const { return this->effort == 0 ? 100 : std::min<uint>(100, this->progress / this->effort); }
};

/** A piece of work in the catalog. Items must be done in order: each lists its prerequisites. */
struct WorkItemSpec {
	WorkTrack track; ///< Which track does the work.
	FeatureCategory category; ///< Feature category for engineering items, #FeatureCategory::End otherwise.
	uint16_t effort; ///< Effort in points.
	std::string_view name; ///< Display name.
	std::array<uint8_t, 3> prereqs; ///< Catalog items that must ship first, #INVALID_WORK_ITEM for none.
	WorkImpact impact = WorkImpact::None; ///< What shipping it changes.
	int16_t impact_value = 0; ///< By how much; for #WorkImpact::Fit the category decides where.
	Money test_cost = 0; ///< One-off testing and QA cost, paid when it ships.
	Money run_cost = 0; ///< Monthly running cost once shipped (hosting, tools, fees).
	uint8_t fork = 0; ///< Fork group: planning one item of a group rules out the others. 0 for none.
	bool city = false; ///< City work: planned for one town, named after it.
};

uint GetWorkItemCount();
const WorkItemSpec &GetWorkItemSpec(uint8_t spec);
WorkItemAvailability GetWorkItemAvailability(CompanyID company, uint8_t spec, TownID town = TownID::Invalid());
const Feature *FindWorkItem(CompanyID company, uint8_t spec, TownID town = TownID::Invalid());
uint8_t GetWorkItemExcludedBy(CompanyID company, uint8_t spec);
int GetWorkImpact(CompanyID company, WorkImpact impact, TownID town = TownID::Invalid());
Money GetWorkRunCosts(CompanyID company);
std::string GetWorkItemPrereqText(CompanyID company, uint8_t spec);
bool HasShippedWorkItem(CompanyID company, uint8_t spec, TownID town = TownID::Invalid());
uint CountAssignedStaff(CompanyID company, WorkTrack track);
uint CountTrackStaff(CompanyID company, WorkTrack track);
uint GetDailyVelocity(CompanyID company, WorkTrack track);
uint GetFeatureDailyProgress(const Feature *f);
uint GetPersonDailyProgress(const struct Employee *e);
EmployeeRole GetTrackRole(WorkTrack track);
uint GetWorkItemSlots(uint8_t spec);
void ChangeFeatureOwnership(CompanyID old_owner, CompanyID new_owner);
void GrantShippedWorkItem(CompanyID company, uint8_t spec);

static constexpr uint8_t FOUNDER_OPERATOR_HEAD_START = 13; ///< "Incorporate the company", already done for operator founders.

#endif /* FEATURE_BASE_H */
