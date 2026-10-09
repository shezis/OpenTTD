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

static constexpr uint REP_RANGE_TILES = 48; ///< Reps can cover towns this close to the HQ or a sales hub.
static constexpr uint MAX_HUBS_PER_COMPANY = 8; ///< Sales hub limit.
static constexpr Money HUB_OPEN_COST = 15000; ///< One-off cost of opening a hub.
static constexpr Money HUB_RENT = 1500; ///< Monthly rent per hub.

std::array<FeatureCategory, 2> GetTownWants(TownID town);
uint GetCompanyFit(CompanyID company, TownID town);
uint GetCompanyStrength(CompanyID company, TownID town);
uint GetTownMarketSize(TownID town);
uint GetCompanyUsers(CompanyID company);
Money GetCompanyPricePerUser(CompanyID company);
Money GetCompanyMRR(CompanyID company);
TownID GetCompanyHQTown(CompanyID company);
bool IsTownInRepRange(CompanyID company, TownID town);
bool HasHubInTown(CompanyID company, TownID town);
uint CountHubs(CompanyID company);
bool IsTownOpportunity(CompanyID company, TownID town);
uint CountRepsInTown(CompanyID company, TownID town);
CompanyID GetTownMarketLeader(TownID town);
std::string GetFounderTownLabel(TownID town, bool with_population);
void ShowMarketWindow(CompanyID company);
void ConfigureFounderOperators();
bool IsFounderOperator(CompanyID company);
Money GetOperatorTransitBudget(CompanyID company);

static constexpr std::string_view FOUNDER_OPERATOR_AI = "SimpleAI"; ///< Bundled AI that runs transit operators.
static constexpr Money OPERATOR_BUDGET_PER_RESIDENT = 1; ///< Monthly city transit budget per resident of a served town.
static constexpr Money OPERATOR_BUDGET_CAP = 40000; ///< Monthly city transit budget cap per operator.
static constexpr std::array<Money, 3> SPONSOR_TIERS = {2000, 5000, 10000}; ///< Monthly sponsorship amounts.

Money GetCompanyMonthlyCosts(CompanyID company);
CompanyID GetOperatorSponsor(CompanyID op);
bool OperatorServesTown(CompanyID op, TownID town);
void ApplyOperatorLivery(CompanyID op);
void ClearSponsorships(CompanyID company);

#endif /* MARKET_FUNC_H */
