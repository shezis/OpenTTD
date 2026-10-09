/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_founder.cpp Implementation of ScriptFounder. */

#include "../../stdafx.h"
#include "script_founder.hpp"
#include "script_error.hpp"
#include "script_town.hpp"
#include "../../company_base.h"
#include "../../employee_base.h"
#include "../../employee_cmd.h"
#include "../../feature_base.h"
#include "../../feature_cmd.h"
#include "../../funding_func.h"
#include "../../market_func.h"
#include "../../office_cmd.h"
#include "../../office_func.h"
#include "../../settings_type.h"
#include "../../town.h"

#include "../../safeguards.h"

/** Resolve a script company to a valid company, or nullptr. */
static const Company *GetFounderCompany(ScriptCompany::CompanyID company)
{
	company = ScriptCompany::ResolveCompanyID(company);
	if (company == ScriptCompany::COMPANY_INVALID) return nullptr;
	return ::Company::GetIfValid(ScriptCompany::FromScriptCompanyID(company));
}

/** The roadmap entry of a catalog item, or nullptr when it is not planned. */
static const Feature *FindWorkItem(::CompanyID company, SQInteger item)
{
	for (const Feature *f : Feature::Iterate()) {
		if (f->company == company && f->spec == item) return f;
	}
	return nullptr;
}

/* static */ bool ScriptFounder::IsFounderMode()
{
	return _settings_game.game_creation.founder_mode;
}

/* static */ bool ScriptFounder::IsStartup(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c != nullptr && !::IsFounderOperator(c->index);
}

/* static */ SQInteger ScriptFounder::GetStaffCount(ScriptCompany::CompanyID company, StaffRole role)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr) return -1;
	SQInteger n = 0;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company == c->index && to_underlying(e->role) == role) n++;
	}
	return n;
}

/* static */ SQInteger ScriptFounder::GetFreeStaff(ScriptCompany::CompanyID company, WorkTrack track)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr || track < TRACK_ENGINEERING || track > TRACK_SALES) return -1;
	::WorkTrack t = static_cast<::WorkTrack>(track);
	uint staff = ::CountTrackStaff(c->index, t);
	uint assigned = ::CountAssignedStaff(c->index, t);
	return staff > assigned ? staff - assigned : 0;
}

/* static */ SQInteger ScriptFounder::GetFreeReps(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr) return -1;
	SQInteger n = 0;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company == c->index && e->role == EmployeeRole::Sales && !::Town::IsValidID(e->town)) n++;
	}
	return n;
}

/* static */ SQInteger ScriptFounder::GetDeskCount(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c == nullptr ? -1 : ::GetOfficeDesks(c->office_level);
}

/* static */ SQInteger ScriptFounder::GetOfficeLevel(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c == nullptr ? -1 : c->office_level;
}

/* static */ Money ScriptFounder::GetOfficeUpgradeCost(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr || c->office_level >= MAX_OFFICE_LEVEL) return -1;
	return ::GetOfficeUpgradeCost(c->office_level);
}

/* static */ Money ScriptFounder::GetMonthlyCosts(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c == nullptr ? Money(-1) : ::GetCompanyMonthlyCosts(c->index);
}

/* static */ Money ScriptFounder::GetMRR(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c == nullptr ? Money(-1) : ::GetCompanyMRR(c->index);
}

/* static */ SQInteger ScriptFounder::GetCustomers(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c == nullptr ? -1 : ::GetCompanyUsers(c->index);
}

/* static */ Money ScriptFounder::GetValuation(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c == nullptr ? Money(-1) : Money(c->founder_valuation);
}

/* static */ SQInteger ScriptFounder::GetFundingStage(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c == nullptr ? -1 : c->founder_stage;
}

/* static */ bool ScriptFounder::IsPublic(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c != nullptr && c->founder_ipo;
}

/* static */ SQInteger ScriptFounder::GetFounderEquity(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c == nullptr ? -1 : c->founder_equity;
}

/* static */ Money ScriptFounder::GetOfferAmount(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr) return -1;
	return c->founder_offer_stage == 0 ? Money(0) : Money(c->founder_offer_amount);
}

/* static */ SQInteger ScriptFounder::GetOfferEquity(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr) return -1;
	return c->founder_offer_stage == 0 ? 0 : c->founder_offer_equity;
}

/** The next funding round of a company, or nullptr when there is none. */
static const FundingRoundSpec *GetNextRound(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr || c->founder_ipo || c->founder_stage >= MAX_FUNDING_STAGE) return nullptr;
	return &::GetFundingRoundSpec(c->founder_stage);
}

/* static */ SQInteger ScriptFounder::GetNextRoundWorkItem(ScriptCompany::CompanyID company)
{
	const FundingRoundSpec *spec = GetNextRound(company);
	return spec == nullptr || spec->required_work == INVALID_WORK_ITEM ? -1 : spec->required_work;
}

/* static */ SQInteger ScriptFounder::GetNextRoundCustomers(ScriptCompany::CompanyID company)
{
	const FundingRoundSpec *spec = GetNextRound(company);
	return spec == nullptr ? -1 : spec->min_users;
}

/* static */ Money ScriptFounder::GetNextRoundMRR(ScriptCompany::CompanyID company)
{
	const FundingRoundSpec *spec = GetNextRound(company);
	return spec == nullptr ? Money(-1) : Money(spec->min_mrr);
}

/* static */ TownID ScriptFounder::GetHQTown(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c == nullptr ? TownID::Invalid() : ::GetCompanyHQTown(c->index);
}

/* static */ SQInteger ScriptFounder::GetTownCustomers(TownID town, ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr || !ScriptTown::IsValidTown(town)) return -1;
	return ::Town::Get(town)->founder_users[c->index];
}

/* static */ SQInteger ScriptFounder::GetTownMarketSize(TownID town)
{
	if (!ScriptTown::IsValidTown(town)) return -1;
	return ::GetTownMarketSize(town);
}

/* static */ SQInteger ScriptFounder::GetTownWant(TownID town, SQInteger index)
{
	if (!ScriptTown::IsValidTown(town) || index < 0 || index > 1) return -1;
	FeatureCategory want = ::GetTownWants(town)[index];
	return want == FeatureCategory::End ? -1 : to_underlying(want);
}

/* static */ SQInteger ScriptFounder::GetTownFit(TownID town, ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr || !ScriptTown::IsValidTown(town)) return -1;
	return ::GetCompanyFit(c->index, town);
}

/* static */ SQInteger ScriptFounder::GetTownStrength(TownID town, ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr || !ScriptTown::IsValidTown(town)) return -1;
	return ::GetCompanyStrength(c->index, town);
}

/* static */ SQInteger ScriptFounder::GetTownReps(TownID town, ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr || !ScriptTown::IsValidTown(town)) return -1;
	return ::CountRepsInTown(c->index, town);
}

/* static */ bool ScriptFounder::IsTownInReach(TownID town, ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c != nullptr && ScriptTown::IsValidTown(town) && ::IsTownInRepRange(c->index, town);
}

/* static */ bool ScriptFounder::HasHub(TownID town, ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c != nullptr && ScriptTown::IsValidTown(town) && ::HasHubInTown(c->index, town);
}

/* static */ SQInteger ScriptFounder::GetHubCount(ScriptCompany::CompanyID company)
{
	const Company *c = GetFounderCompany(company);
	return c == nullptr ? -1 : ::CountHubs(c->index);
}

/* static */ ScriptCompany::CompanyID ScriptFounder::GetTownLeader(TownID town)
{
	if (!ScriptTown::IsValidTown(town)) return ScriptCompany::COMPANY_INVALID;
	::CompanyID leader = ::GetTownMarketLeader(town);
	return ::Company::IsValidID(leader) ? ScriptCompany::ToScriptCompanyID(leader) : ScriptCompany::COMPANY_INVALID;
}

/* static */ SQInteger ScriptFounder::GetWorkItemCount()
{
	return ::GetWorkItemCount();
}

/* static */ std::optional<std::string> ScriptFounder::GetWorkItemName(SQInteger item)
{
	if (item < 0 || item >= ::GetWorkItemCount()) return std::nullopt;
	return std::string(::GetWorkItemSpec(item).name);
}

/* static */ SQInteger ScriptFounder::GetWorkItemTrack(SQInteger item)
{
	if (item < 0 || item >= ::GetWorkItemCount()) return -1;
	return to_underlying(::GetWorkItemSpec(item).track);
}

/* static */ SQInteger ScriptFounder::GetWorkItemCategory(SQInteger item)
{
	if (item < 0 || item >= ::GetWorkItemCount()) return -1;
	FeatureCategory cat = ::GetWorkItemSpec(item).category;
	return cat == FeatureCategory::End ? -1 : to_underlying(cat);
}

/* static */ ScriptFounder::WorkStatus ScriptFounder::GetWorkItemStatus(ScriptCompany::CompanyID company, SQInteger item)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr || item < 0 || item >= ::GetWorkItemCount()) return WORK_LOCKED;
	if (::HasShippedWorkItem(c->index, item)) return WORK_SHIPPED;
	const Feature *f = FindWorkItem(c->index, item);
	if (f != nullptr) return f->state == FeatureState::InProgress ? WORK_IN_PROGRESS : WORK_BACKLOG;
	return ::GetWorkItemAvailability(c->index, item) == WorkItemAvailability::Available ? WORK_AVAILABLE : WORK_LOCKED;
}

/* static */ SQInteger ScriptFounder::GetWorkItemProgress(ScriptCompany::CompanyID company, SQInteger item)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr || item < 0 || item >= ::GetWorkItemCount()) return -1;
	if (::HasShippedWorkItem(c->index, item)) return 100;
	const Feature *f = FindWorkItem(c->index, item);
	return f == nullptr ? -1 : f->GetProgressPercent();
}

/* static */ SQInteger ScriptFounder::GetWorkItemStaff(ScriptCompany::CompanyID company, SQInteger item)
{
	const Company *c = GetFounderCompany(company);
	if (c == nullptr) return -1;
	const Feature *f = FindWorkItem(c->index, item);
	return f == nullptr ? -1 : f->assigned;
}

/* static */ bool ScriptFounder::Hire(StaffRole role)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, role >= ROLE_ENGINEER && role <= ROLE_OPERATIONS);
	return ScriptObject::Command<Commands::HireEmployee>::Do(static_cast<EmployeeRole>(role));
}

/* static */ bool ScriptFounder::Fire(StaffRole role)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, role >= ROLE_ENGINEER && role <= ROLE_OPERATIONS);
	::CompanyID self = ScriptObject::GetCompany();
	const Employee *pick = nullptr;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company != self || to_underlying(e->role) != role) continue;
		/* Keep a rep without a town once found; otherwise end on the newest hire. */
		if (pick != nullptr && pick->role == EmployeeRole::Sales && !::Town::IsValidID(pick->town)) continue;
		pick = e;
	}
	EnforcePrecondition(false, pick != nullptr);
	return ScriptObject::Command<Commands::FireEmployee>::Do(pick->index);
}

/* static */ bool ScriptFounder::AssignRep(TownID town)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ScriptTown::IsValidTown(town));
	::CompanyID self = ScriptObject::GetCompany();
	const Employee *rep = nullptr;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company == self && e->role == EmployeeRole::Sales && !::Town::IsValidID(e->town)) {
			rep = e;
			break;
		}
	}
	EnforcePrecondition(false, rep != nullptr);
	return ScriptObject::Command<Commands::AssignRep>::Do(rep->index, town);
}

/* static */ bool ScriptFounder::UnassignRep(TownID town)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ScriptTown::IsValidTown(town));
	::CompanyID self = ScriptObject::GetCompany();
	const Employee *rep = nullptr;
	for (const Employee *e : Employee::Iterate()) {
		if (e->company == self && e->role == EmployeeRole::Sales && e->town == town) {
			rep = e;
			break;
		}
	}
	EnforcePrecondition(false, rep != nullptr);
	return ScriptObject::Command<Commands::AssignRep>::Do(rep->index, TownID::Invalid());
}

/* static */ bool ScriptFounder::OpenHub(TownID town)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ScriptTown::IsValidTown(town));
	return ScriptObject::Command<Commands::SetHub>::Do(town, true);
}

/* static */ bool ScriptFounder::CloseHub(TownID town)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, ScriptTown::IsValidTown(town));
	return ScriptObject::Command<Commands::SetHub>::Do(town, false);
}

/* static */ bool ScriptFounder::UpgradeOffice()
{
	EnforceCompanyModeValid(false);
	const Company *c = ::Company::Get(ScriptObject::GetCompany());
	EnforcePrecondition(false, c->office_level < MAX_OFFICE_LEVEL);
	return ScriptObject::Command<Commands::UpgradeOffice>::Do(c->office_level + 1);
}

/* static */ bool ScriptFounder::PlanWorkItem(SQInteger item)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, item >= 0 && item < ::GetWorkItemCount());
	return ScriptObject::Command<Commands::CreateFeature>::Do(static_cast<uint8_t>(item));
}

/* static */ bool ScriptFounder::StaffWorkItem(SQInteger item, SQInteger people)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, people >= 0 && people <= UINT8_MAX);
	const Feature *f = FindWorkItem(ScriptObject::GetCompany(), item);
	EnforcePrecondition(false, f != nullptr);
	return ScriptObject::Command<Commands::AssignFeature>::Do(f->index, static_cast<uint8_t>(people));
}

/* static */ bool ScriptFounder::RespondToOffer(bool accept)
{
	EnforceCompanyModeValid(false);
	return ScriptObject::Command<Commands::RespondFundingOffer>::Do(accept);
}

/* static */ bool ScriptFounder::Sponsor(ScriptCompany::CompanyID transit_operator, Money monthly)
{
	EnforceCompanyModeValid(false);
	transit_operator = ScriptCompany::ResolveCompanyID(transit_operator);
	EnforcePrecondition(false, transit_operator != ScriptCompany::COMPANY_INVALID);
	return ScriptObject::Command<Commands::SponsorOperator>::Do(ScriptCompany::FromScriptCompanyID(transit_operator), monthly);
}
