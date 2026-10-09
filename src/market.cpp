/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file market.cpp Founder Mode market: customers per town, organic growth, churn and revenue. */

#include "stdafx.h"
#include "market_func.h"
#include "company_base.h"
#include "company_func.h"
#include "debug.h"
#include "employee_base.h"
#include "feature_base.h"
#include "map_func.h"
#include "settings_type.h"
#include "timer/timer.h"
#include "timer/timer_game_economy.h"
#include "town.h"
#include "window_func.h"
#include "strings_func.h"
#include "viewport_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr uint8_t WORK_ITEM_MVP = 0; ///< "MVP prototype": no customers before it ships.
static constexpr uint8_t WORK_ITEM_PRICING_PAGE = 21; ///< "Pricing page": raises the price per user.
static constexpr uint8_t WORK_ITEM_SELF_SERVE = 25; ///< "Self-serve checkout": raises the price per user.
static constexpr uint MARKET_BASELINE = 60; ///< Demand nobody captures, so no company reaches 100%.
static constexpr uint ORGANIC_RANGE_TILES = 40; ///< Word of mouth reaches towns this close.

/**
 * Two feature categories a town wants, fixed per town so they need no saving.
 * @param town The town.
 * @return Wanted categories (never infrastructure).
 */
std::array<FeatureCategory, 2> GetTownWants(TownID town)
{
	const uint n = to_underlying(FeatureCategory::Infrastructure); // Product categories only.
	uint a = town.base() % n;
	uint b = (town.base() * 7 + 3) % n;
	if (b == a) b = (a + 1) % n;
	return { static_cast<FeatureCategory>(a), static_cast<FeatureCategory>(b) };
}

/**
 * How well a company's shipped product fits a town, 100 = basic product.
 * @param company The company.
 * @param town The town.
 * @return Fit, 0 before the MVP has shipped.
 */
uint GetCompanyFit(CompanyID company, TownID town)
{
	if (!HasShippedWorkItem(company, WORK_ITEM_MVP)) return 0;
	auto wants = GetTownWants(town);
	uint fit = 100;
	for (const Feature *f : Feature::Iterate()) {
		if (f->company != company || f->state != FeatureState::Shipped || f->GetTrack() != WorkTrack::Engineering) continue;
		FeatureCategory cat = GetWorkItemSpec(f->spec).category;
		if (cat == FeatureCategory::Infrastructure || cat == FeatureCategory::End) continue;
		fit += (cat == wants[0] || cat == wants[1]) ? 50 * f->quality / 80 : 10;
	}
	return fit;
}

/** Town nearest to the company's HQ, or invalid without an HQ. */
TownID GetCompanyHQTown(CompanyID company)
{
	const Company *c = Company::GetIfValid(company);
	if (c == nullptr || c->location_of_HQ == INVALID_TILE) return TownID::Invalid();
	const Town *t = ClosestTownFromTile(c->location_of_HQ, UINT_MAX);
	return t == nullptr ? TownID::Invalid() : t->index;
}

/**
 * Whether a company's reps can work a town: within range of its HQ.
 * @param company The company.
 * @param town The town.
 * @return True when reps may be assigned there.
 */
bool IsTownInRepRange(CompanyID company, TownID town)
{
	const Company *c = Company::GetIfValid(company);
	const Town *t = Town::GetIfValid(town);
	if (c == nullptr || t == nullptr || c->location_of_HQ == INVALID_TILE) return false;
	return DistanceManhattan(c->location_of_HQ, t->xy) <= REP_RANGE_TILES;
}

/** Number of a company's sales reps working a town. */
uint CountRepsInTown(CompanyID company, TownID town)
{
	uint n = 0;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company == company && e->role == EmployeeRole::Sales && e->town == town) n++;
	}
	return n;
}

/**
 * Opportunity: word of mouth has brought customers here, but nobody from the company works the town.
 * @param company The company.
 * @param town The town.
 * @return True when the town is worth a rep or a hub.
 */
bool IsTownOpportunity(CompanyID company, TownID town)
{
	const Town *t = Town::GetIfValid(town);
	if (t == nullptr || !Company::IsValidID(company)) return false;
	return t->founder_users[company] > 0 && CountRepsInTown(company, town) == 0 && GetCompanyHQTown(company) != town;
}

/** Company with the most customers in a town, or invalid when nobody has any. */
CompanyID GetTownMarketLeader(TownID town)
{
	const Town *t = Town::GetIfValid(town);
	CompanyID leader = CompanyID::Invalid();
	uint best = 0;
	if (t == nullptr) return leader;
	for (const Company *c : Company::Iterate()) {
		if (t->founder_users[c->index] > best) {
			best = t->founder_users[c->index];
			leader = c->index;
		}
	}
	return leader;
}

/**
 * Map label of a town in Founder Mode: name, optionally population, your share and opportunity.
 * @param town The town.
 * @param with_population Include the population, as the normal label setting does.
 * @return Label text.
 */
std::string GetFounderTownLabel(TownID town, bool with_population)
{
	const Town *t = Town::Get(town);
	uint size = GetTownMarketSize(town);
	uint share = 0;
	if (Company::IsValidID(_local_company) && size > 0) share = t->founder_users[_local_company] * 100 / size;
	StringID str = IsTownOpportunity(_local_company, town)
			? (with_population ? STR_VIEWPORT_TOWN_FOUNDER_POP_OPPORTUNITY : STR_VIEWPORT_TOWN_FOUNDER_OPPORTUNITY)
			: (with_population ? STR_VIEWPORT_TOWN_FOUNDER_POP : STR_VIEWPORT_TOWN_FOUNDER);
	return GetString(str, town, t->cache.population, share);
}

/** Potential customers in a town. */
uint GetTownMarketSize(TownID town)
{
	const Town *t = Town::GetIfValid(town);
	return t == nullptr ? 0 : t->cache.population / 2;
}

/**
 * How hard a company pushes in a town: sales reps, HQ presence and word of mouth, times product fit.
 * @param company The company.
 * @param town The town.
 * @return Strength, 0 when the company has nothing to sell.
 */
uint GetCompanyStrength(CompanyID company, TownID town)
{
	uint fit = GetCompanyFit(company, town);
	if (fit == 0) return 0;
	const Town *t = Town::Get(town);

	uint reach = 0;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company == company && e->role == EmployeeRole::Sales && e->town == town) reach += e->skill * e->morale / 75;
	}
	if (GetCompanyHQTown(company) == town) reach += 40;

	/* Word of mouth: existing customers here and in nearby towns. */
	reach += t->founder_users[company] / 50;
	for (const Town *o : Town::Iterate()) {
		if (o != t && DistanceManhattan(o->xy, t->xy) <= ORGANIC_RANGE_TILES) reach += o->founder_users[company] / 200;
	}

	return reach * fit / 100;
}

/** Monthly churn in permille: base plus shipped bugs. */
static uint GetChurnPermille(CompanyID company)
{
	uint bugs = 0;
	for (const Feature *f : Feature::Iterate()) {
		if (f->company == company && f->state == FeatureState::Shipped) bugs += f->bugs;
	}
	return std::min<uint>(20 + 3 * bugs, 300);
}

uint GetCompanyUsers(CompanyID company)
{
	uint users = 0;
	for (const Town *t : Town::Iterate()) users += t->founder_users[company];
	return users;
}

Money GetCompanyPricePerUser(CompanyID company)
{
	Money price = 10;
	if (HasShippedWorkItem(company, WORK_ITEM_PRICING_PAGE)) price += 3;
	if (HasShippedWorkItem(company, WORK_ITEM_SELF_SERVE)) price += 3;
	return price;
}

Money GetCompanyMRR(CompanyID company)
{
	return GetCompanyUsers(company) * GetCompanyPricePerUser(company);
}

/** Every economy month: customers move toward each company's share of demand, then pay. No Random() here. */
static const IntervalTimer<TimerGameEconomy> _economy_market_monthly({TimerGameEconomy::Trigger::Month, TimerGameEconomy::Priority::None}, [](auto)
{
	if (!_settings_game.game_creation.founder_mode) return;

	for (Town *t : Town::Iterate()) {
		std::array<uint, MAX_COMPANIES> strength{};
		uint total = MARKET_BASELINE;
		for (const Company *c : Company::Iterate()) {
			strength[c->index.base()] = GetCompanyStrength(c->index, t->index);
			total += strength[c->index.base()];
		}
		uint market = GetTownMarketSize(t->index);
		for (const Company *c : Company::Iterate()) {
			uint32_t &users = t->founder_users[c->index];
			int64_t target = static_cast<int64_t>(market) * strength[c->index.base()] / total;
			int64_t next = users + (target - static_cast<int64_t>(users)) * 3 / 10;
			next -= next * GetChurnPermille(c->index) / 1000;
			users = static_cast<uint32_t>(std::max<int64_t>(0, next));
		}
	}

	for (const Company *c : Company::Iterate()) {
		Money mrr = GetCompanyMRR(c->index);
		if (mrr > 0) SubtractMoneyFromCompany(c->index, CommandCost(ExpensesType::Other, -mrr));
		Debug(Facility::Misc, Severity::Info, "Founder Mode: company {} has {} users, MRR {}", c->index + 1, GetCompanyUsers(c->index), mrr);
		InvalidateWindowData(WindowClass::Market, c->index);
	}

	/* Shares changed: refresh town labels on the map. */
	UpdateAllTownVirtCoords();
	MarkWholeScreenDirty();
});
