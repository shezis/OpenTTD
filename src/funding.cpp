/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file funding.cpp Founder Mode funding: valuation, investor offers, rounds and dilution. */

#include "stdafx.h"
#include "funding_func.h"
#include "economy_func.h"
#include "town.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "debug.h"
#include "employee_cmd.h"
#include "feature_base.h"
#include "market_func.h"
#include "settings_type.h"
#include "window_func.h"
#include "news_func.h"
#include "strings_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Rounds by stage: seed, A, B, C. */
static const FundingRoundSpec _funding_rounds[MAX_FUNDING_STAGE] = {
	{ STR_FUNDING_ROUND_SEED,       250000,     1000000,      0,  300, 17 }, // Needs the Pitch deck.
	{ STR_FUNDING_ROUND_A,         2000000,     6000000,  12000,    0, 18 }, // Needs the Investor data room.
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
/** An investor's stake from its round, diluted by later rounds, before any buyback. */
static uint64_t GetRawInvestorEquity(const Company *c, uint8_t round)
{
	uint64_t equity = c->founder_round_equity[round];
	for (uint8_t later = round + 1; later < c->founder_stage; later++) equity = equity * (1000 - c->founder_round_equity[later]) / 1000;
	return equity;
}

uint16_t GetInvestorEquity(CompanyID company, uint8_t round)
{
	const Company *c = Company::Get(company);
	/* Buybacks shrink every investor's stake in proportion, so investors always hold what founders do not. */
	uint64_t raw_total = 0;
	for (uint8_t r = 0; r < c->founder_stage; r++) raw_total += GetRawInvestorEquity(c, r);
	if (raw_total == 0) return 0;
	return static_cast<uint16_t>(GetRawInvestorEquity(c, round) * (1000 - c->founder_equity) / raw_total);
}

/**
 * Venture debt a startup can carry: half the value of the stake investors hold.
 * Giving up more equity lets you borrow more; buying it back lowers the limit.
 * @param c The startup.
 * @return Debt limit, in whole loan steps.
 */
Money GetVentureDebtCapacity(const Company *c)
{
	int64_t investor_value = c->founder_valuation / 1000 * (1000 - c->founder_equity);
	return investor_value / 2 / LOAN_INTERVAL * LOAN_INTERVAL;
}

/**
 * Price of buying back one step of equity from investors, at the current valuation.
 * @param company The startup.
 * @return The cost.
 */
Money GetBuybackCost(CompanyID company)
{
	return Company::Get(company)->founder_valuation / 1000 * BUYBACK_STEP_PERMILLE;
}

/**
 * Big towns need an operating permit before your reps and hubs win customers there. Your HQ town never does.
 * @param company The startup.
 * @param town The town.
 * @return True when a permit is needed and not yet granted.
 */
bool IsPermitRequired(CompanyID company, TownID town)
{
	const Town *t = Town::Get(town);
	if (t->cache.population < PERMIT_POPULATION || GetCompanyHQTown(company) == town) return false;
	return !HasShippedWorkItem(company, WORK_ITEM_OPERATING_PERMIT, town);
}

/** Catalog item that satisfies each regulation. */
static constexpr uint8_t _regulation_items[] = { 38, 39 };
static_assert(std::size(_regulation_items) == to_underlying(Regulation::End));

bool IsRegulationMet(CompanyID company, Regulation reg)
{
	return HasShippedWorkItem(company, _regulation_items[to_underlying(reg)]);
}

/**
 * This month's fines for regulations past their deadline.
 * @param company The startup.
 * @return Total fines.
 */
Money GetRegulationFine(CompanyID company)
{
	const Company *c = Company::Get(company);
	Money fine = 0;
	for (uint r = 0; r < to_underlying(Regulation::End); r++) {
		if (c->founder_reg_months[r] == 1 && !IsRegulationMet(company, static_cast<Regulation>(r))) fine += std::max<Money>(2000, GetCompanyMRR(company) / 10);
	}
	return fine;
}

/**
 * Corporation tax on this month's operating profit.
 * @param company The startup.
 * @return Tax, 0 while loss-making.
 */
Money GetMonthlyTax(CompanyID company)
{
	Money profit = GetCompanyMRR(company) - GetCompanyMonthlyCosts(company);
	return profit > 0 ? profit * CORPORATION_TAX_PERCENT / 100 : Money(0);
}

/**
 * Monthly: start regulation clocks when rules begin to apply, count down, and warn the founder.
 * @param company The startup.
 */
void UpdateRegulation(CompanyID company)
{
	Company *c = Company::Get(company);
	const bool applies[] = {
		GetCompanyUsers(company) >= 500,
		HasShippedWorkItem(company, 5), // Own payments and billing; a provider handles licensing for you.
	};
	static_assert(std::size(applies) == to_underlying(Regulation::End));
	for (uint r = 0; r < to_underlying(Regulation::End); r++) {
		Regulation reg = static_cast<Regulation>(r);
		uint8_t &months = c->founder_reg_months[r];
		if (months == 0 && applies[r] && !IsRegulationMet(company, reg)) {
			months = REGULATION_GRACE_MONTHS + 1;
			if (company == _local_company) {
				AddNewsItem(GetEncodedString(STR_NEWS_FOUNDER_REGULATION, STR_REGULATION_DATA_PROTECTION + r, GetWorkItemSpec(_regulation_items[r]).name, REGULATION_GRACE_MONTHS), NewsType::CompanyInfo, NewsStyle::Normal, {});
			}
		} else if (months > 1 && !IsRegulationMet(company, reg)) {
			months--;
			if (months == 1 && company == _local_company) {
				AddNewsItem(GetEncodedString(STR_NEWS_FOUNDER_FINES, STR_REGULATION_DATA_PROTECTION + r), NewsType::CompanyInfo, NewsStyle::Small, {});
			}
		}
	}
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

	/* Going public: the win condition. */
	if (!c->founder_ipo && c->founder_valuation >= IPO_VALUATION) {
		/* David vs Goliath: the first startup to go public wins the race. */
		bool first = true;
		for (const Company *o : Company::Iterate()) {
			if (o != c && o->founder_ipo) first = false;
		}
		c->founder_ipo = true;
		c->founder_offer_stage = 0;
		if (_settings_game.game_creation.founder_scenario == 1 && first && Company::IsValidID(_local_company)) {
			AddNewsItem(GetEncodedString(company == _local_company ? STR_NEWS_FOUNDER_GOLIATH_WON : STR_NEWS_FOUNDER_GOLIATH_LOST, company), NewsType::CompanyInfo, NewsStyle::Normal, {});
		}
		Debug(Facility::Misc, Severity::Info, "Founder Mode: company {} went public at {}", company + 1, c->founder_valuation);
		AddNewsItem(GetEncodedString(STR_NEWS_FOUNDER_IPO, company, c->founder_valuation), NewsType::CompanyInfo, NewsStyle::Normal, {});
		InvalidateWindowData(WindowClass::Board, company);
		if (company == _local_company) ShowBoardWindow(company);
	}

	/* Runway warnings, once per level; reset when runway recovers. */
	int64_t net = static_cast<int64_t>(GetCompanyMonthlyCosts(company)) - mrr;
	uint8_t level = 0;
	if (net > 0) {
		int64_t months = std::max<int64_t>(0, static_cast<int64_t>(c->money)) / net;
		level = months < 3 ? 2 : (months < 6 ? 1 : 0);
		if (level > c->founder_runway_warning && company == _local_company) {
			Debug(Facility::Misc, Severity::Info, "Founder Mode: company {} runway warning level {} ({} months)", company + 1, level, months);
			EncodedString msg = months == 0 ? GetEncodedString(STR_NEWS_FOUNDER_OUT_OF_CASH) : GetEncodedString(level == 2 ? STR_NEWS_FOUNDER_RUNWAY_CRITICAL : STR_NEWS_FOUNDER_RUNWAY_LOW, months);
			AddNewsItem(std::move(msg), NewsType::CompanyInfo, NewsStyle::Small, {});
		}
	}
	c->founder_runway_warning = level;

	if (c->founder_offer_stage != 0) {
		if (--c->founder_offer_months == 0) {
			c->founder_offer_stage = 0;
			InvalidateWindowData(WindowClass::Board, company);
		}
		return;
	}
	if (c->founder_ipo || c->founder_stage >= MAX_FUNDING_STAGE) return;

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
			SubtractMoneyFromCompany(c->index, CommandCost(ExpensesType::RoadVehRevenue, -c->founder_offer_amount));
		}
		c->founder_offer_stage = 0;
		InvalidateWindowData(WindowClass::Board, c->index);
	}
	return CommandCost();
}

/**
 * Buy back one step of equity from investors at the current valuation.
 * @param flags Type of operation.
 * @param permille Equity to buy back; must be one step.
 * @return The cost of this operation or an error.
 */
CommandCost CmdBuyBackEquity(DoCommandFlags flags, uint16_t permille)
{
	Company *c = Company::GetIfValid(_current_company);
	if (c == nullptr || IsFounderOperator(c->index) || permille != BUYBACK_STEP_PERMILLE) return CMD_ERROR;
	if (c->founder_stage == 0 || 1000 - c->founder_equity < permille) return CommandCost(STR_ERROR_NO_INVESTOR_EQUITY);

	CommandCost cost(ExpensesType::RoadVehRevenue, GetBuybackCost(c->index));
	if (flags.Test(DoCommandFlag::Execute)) {
		c->founder_equity += permille;
		InvalidateWindowData(WindowClass::Board, c->index);
	}
	return cost;
}
