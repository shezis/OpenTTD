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
#include "economy_type.h"
#include "strings_type.h"

static constexpr uint8_t MAX_FUNDING_STAGE = 4; ///< Seed, A, B, C.
static constexpr int64_t IPO_VALUATION = 1000000000; ///< Valuation at which a startup goes public.

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
void ShowBoardWindow(CompanyID company);

#endif /* FUNDING_FUNC_H */
