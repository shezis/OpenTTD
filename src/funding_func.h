/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file funding_func.h Founder Mode funding: valuation, rounds and the cap table. */

#ifndef FUNDING_FUNC_H
#define FUNDING_FUNC_H

#include "company_type.h"
#include "town_type.h"
#include "economy_type.h"
#include "economy_type.h"
#include "strings_type.h"

static constexpr uint8_t MAX_FUNDING_STAGE = 4; ///< Seed, A, B, C.
static constexpr int64_t IPO_VALUATION = 1000000000; ///< Valuation at which a startup goes public.
static constexpr uint CORPORATION_TAX_PERCENT = 19; ///< Tax on monthly profit.
static constexpr uint BUYBACK_STEP_PERMILLE = 10; ///< Equity bought back per click: 1%.
static constexpr uint PERMIT_POPULATION = 3000; ///< Towns this big need an operating permit to sell there.
static constexpr uint8_t WORK_ITEM_OPERATING_PERMIT = 37; ///< City work: permit to sell in a big town.

/** Regulations that can apply to a startup. */
enum class Regulation : uint8_t {
	DataProtection, ///< Applies from 500 customers; needs the data protection programme.
	PaymentsLicence, ///< Applies once you run your own payments; needs the payments licence.
	End, ///< End marker.
};
static constexpr uint8_t REGULATION_GRACE_MONTHS = 6; ///< Months to comply before fines start.

/** One funding round's requirements and terms. */
struct FundingRoundSpec {
	StringID name; ///< Round name.
	int64_t amount; ///< Cash raised.
	int64_t floor_pre_money; ///< Lowest pre-money valuation investors accept.
	int64_t min_mrr; ///< MRR needed before investors talk.
	uint min_users; ///< Customers needed.
	uint8_t required_work; ///< Catalog item that must have shipped, or 0xFF.
};

const FundingRoundSpec &GetFundingRoundSpec(uint8_t stage);
StringID GetInvestorName(uint8_t investor);
uint16_t GetInvestorEquity(CompanyID company, uint8_t round);
void UpdateFunding(CompanyID company, int64_t mrr);
Money GetVentureDebtCapacity(const struct Company *c);
Money GetBuybackCost(CompanyID company);
bool IsPermitRequired(CompanyID company, TownID town);
bool IsRegulationMet(CompanyID company, Regulation reg);
Money GetRegulationFine(CompanyID company);
Money GetMonthlyTax(CompanyID company);
void UpdateRegulation(CompanyID company);
void ShowBoardWindow(CompanyID company);

#endif /* FUNDING_FUNC_H */
