/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file funding.cpp Founder Mode funding: valuation, investor offers, rounds and dilution. */

#include "stdafx.h"
#include "funding_func.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "debug.h"
#include "employee_cmd.h"
#include "feature_base.h"
#include "market_func.h"
#include "settings_type.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Rounds by stage: seed, A, B, C. */
static const FundingRoundSpec _funding_rounds[MAX_FUNDING_STAGE] = {
	{ STR_FUNDING_ROUND_SEED,       250000,     1000000,      0,  300, 17 }, // Needs the Pitch deck.
	{ STR_FUNDING_ROUND_A,         2000000,     6000000,  20000,    0, 18 }, // Needs the Investor data room.
	{ STR_FUNDING_ROUND_B,         8000000,    30000000,  80000,    0, 0xFF },
	{ STR_FUNDING_ROUND_C,        25000000,   120000000, 250000,    0, 0xFF },
};

static const StringID _investor_names[] = {
	STR_FUNDING_INVESTOR_0, STR_FUNDING_INVESTOR_1, STR_FUNDING_INVESTOR_2,
	STR_FUNDING_INVESTOR_3, STR_FUNDING_INVESTOR_4, STR_FUNDING_INVESTOR_5,
};

const FundingRoundSpec &GetFundingRoundSpec(uint8_t stage) { return _funding_rounds[std::min<uint8_t>(stage, MAX_FUNDING_STAGE - 1)]; }
StringID GetInvestorName(uint8_t investor) { return _investor_names[investor % std::size(_investor_names)]; }

/**
 * Current ownership of the investor from a round, after dilution by later rounds.
 * @param company The company.
 * @param round Round index.
 * @return Permille of the company.
 */
uint16_t GetInvestorEquity(CompanyID company, uint8_t round)
{
	const Company *c = Company::Get(company);
	uint64_t equity = c->founder_round_equity[round];
	for (uint8_t later = round + 1; later < c->founder_stage; later++) equity = equity * (1000 - c->founder_round_equity[later]) / 1000;
	return static_cast<uint16_t>(equity);
}

/**
 * Monthly: valuation from MRR and growth, offer expiry, and a new offer once the next round's milestones are met.
 * Deterministic (no Random()); called from the market's monthly update.
 * @param company The startup.
 * @param mrr This month's MRR.
 */
void UpdateFunding(CompanyID company, int64_t mrr)
{
	Company *c = Company::Get(company);
	int64_t growth = c->founder_last_mrr > 0 ? (mrr - c->founder_last_mrr) * 100 / c->founder_last_mrr : 0;
	int64_t multiple = 8 + std::clamp<int64_t>(growth, 0, 30) / 3;
	c->founder_valuation = std::max<int64_t>(c->founder_valuation, mrr * 12 * multiple);
	c->founder_last_mrr = mrr;

	if (c->founder_offer_stage != 0) {
		if (--c->founder_offer_months == 0) {
			c->founder_offer_stage = 0;
			InvalidateWindowData(WindowClass::Board, company);
		}
		return;
	}
	if (c->founder_stage >= MAX_FUNDING_STAGE) return;

	const FundingRoundSpec &spec = _funding_rounds[c->founder_stage];
	if (mrr < spec.min_mrr || GetCompanyUsers(company) < spec.min_users) return;
	if (!HasShippedWorkItem(company, 0)) return;
	if (spec.required_work != 0xFF && !HasShippedWorkItem(company, spec.required_work)) return;

	int64_t pre = std::max(c->founder_valuation, spec.floor_pre_money);
	int64_t post = pre + spec.amount;
	c->founder_offer_stage = c->founder_stage + 1;
	c->founder_offer_months = 3;
	c->founder_offer_investor = static_cast<uint8_t>((c->founder_stage * 2 + company.base()) % std::size(_investor_names));
	c->founder_offer_amount = spec.amount;
	c->founder_offer_equity = static_cast<uint16_t>(std::clamp<int64_t>(spec.amount * 1000 / post, 80, 300));
	Debug(Facility::Misc, Severity::Info, "Founder Mode: company {} offered round {} of {} for {} permille", company + 1, c->founder_offer_stage, spec.amount, c->founder_offer_equity);

	InvalidateWindowData(WindowClass::Board, company);
	if (company == _local_company) ShowBoardWindow(company);
}

/**
 * Accept or decline the pending investor offer.
 * @param flags Type of operation.
 * @param accept True to take the money.
 * @return The cost of this operation or an error.
 */
CommandCost CmdRespondFundingOffer(DoCommandFlags flags, bool accept)
{
	Company *c = Company::GetIfValid(_current_company);
	if (c == nullptr || c->founder_offer_stage == 0) return CommandCost(STR_ERROR_NO_FUNDING_OFFER);

	if (flags.Test(DoCommandFlag::Execute)) {
		if (accept) {
			uint8_t round = c->founder_stage;
			c->founder_round_amount[round] = c->founder_offer_amount;
			c->founder_round_equity[round] = c->founder_offer_equity;
			c->founder_round_investor[round] = c->founder_offer_investor;
			c->founder_equity = static_cast<uint16_t>(static_cast<uint32_t>(c->founder_equity) * (1000 - c->founder_offer_equity) / 1000);
			c->founder_valuation = c->founder_offer_amount * 1000 / std::max<uint16_t>(c->founder_offer_equity, 1);
			c->founder_stage++;
			SubtractMoneyFromCompany(c->index, CommandCost(ExpensesType::Other, -c->founder_offer_amount));
		}
		c->founder_offer_stage = 0;
		InvalidateWindowData(WindowClass::Board, c->index);
	}
	return CommandCost();
}
