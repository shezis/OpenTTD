/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file market_func.h Founder Mode market: customers per town, demand and revenue. */

#ifndef MARKET_FUNC_H
#define MARKET_FUNC_H

#include "company_type.h"
#include "economy_type.h"
#include "feature_type.h"
#include "town_type.h"

static constexpr uint REP_RANGE_TILES = 48; ///< Reps can cover towns this close to the HQ (hubs extend this later).

std::array<FeatureCategory, 2> GetTownWants(TownID town);
uint GetCompanyFit(CompanyID company, TownID town);
uint GetCompanyStrength(CompanyID company, TownID town);
uint GetTownMarketSize(TownID town);
uint GetCompanyUsers(CompanyID company);
Money GetCompanyPricePerUser(CompanyID company);
Money GetCompanyMRR(CompanyID company);
TownID GetCompanyHQTown(CompanyID company);
bool IsTownInRepRange(CompanyID company, TownID town);
bool IsTownOpportunity(CompanyID company, TownID town);
uint CountRepsInTown(CompanyID company, TownID town);
CompanyID GetTownMarketLeader(TownID town);
std::string GetFounderTownLabel(TownID town, bool with_population);
void ShowMarketWindow(CompanyID company);

#endif /* MARKET_FUNC_H */
