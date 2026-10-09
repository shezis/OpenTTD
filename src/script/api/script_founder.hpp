/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_founder.hpp Founder Mode: run a startup through the same commands as the player. */

#ifndef SCRIPT_FOUNDER_HPP
#define SCRIPT_FOUNDER_HPP

#include "script_company.hpp"
#include "../../employee_type.h"
#include "../../feature_type.h"

/**
 * Class that handles Founder Mode: staff, office, work items, sales coverage and funding.
 * Every action goes through the same commands a human player uses, with the same costs and limits.
 * @api ai game
 */
class ScriptFounder : public ScriptObject {
public:
	/** Staff roles. */
	enum StaffRole {
		ROLE_ENGINEER = ::to_underlying(::EmployeeRole::Engineer), ///< Builds engineering items.
		ROLE_DESIGNER = ::to_underlying(::EmployeeRole::Designer), ///< Improves quality.
		ROLE_SALES = ::to_underlying(::EmployeeRole::Sales), ///< Works a town as a sales rep; staffs the sales track.
		ROLE_OPERATIONS = ::to_underlying(::EmployeeRole::Operations), ///< Staffs the business track.
	};

	/** Seniority levels: salary, speed and quality rise with level. */
	enum StaffLevel {
		LEVEL_JUNIOR = ::to_underlying(::EmployeeLevel::Junior), ///< Cheap, slower, more bugs.
		LEVEL_MID = ::to_underlying(::EmployeeLevel::Mid), ///< The baseline.
		LEVEL_SENIOR = ::to_underlying(::EmployeeLevel::Senior), ///< Expensive, faster, better quality.
	};

	/** Work tracks of the catalog. */
	enum WorkTrack {
		TRACK_ENGINEERING = ::to_underlying(::WorkTrack::Engineering), ///< Features and deployments.
		TRACK_BUSINESS = ::to_underlying(::WorkTrack::Business), ///< Company setup, legal and fundraising.
		TRACK_SALES = ::to_underlying(::WorkTrack::Sales), ///< Go-to-market.
	};

	/** Where a catalog item stands for a company. */
	enum WorkStatus {
		WORK_LOCKED, ///< Prerequisites not shipped yet.
		WORK_AVAILABLE, ///< Can be planned.
		WORK_BACKLOG, ///< Planned, nobody working on it.
		WORK_IN_PROGRESS, ///< Being worked on.
		WORK_SHIPPED, ///< Done.
	};

	/**
	 * Is this game a Founder Mode game?
	 * @return True in Founder Mode.
	 */
	static bool IsFounderMode();

	/**
	 * Is the company a startup (player or rival), rather than a transit operator?
	 * @param company The company.
	 * @return True for a startup.
	 */
	static bool IsStartup(ScriptCompany::CompanyID company);

	/**
	 * Number of staff in a role.
	 * @param company The company.
	 * @param role The role.
	 * @return Staff count, or -1 for an invalid company.
	 */
	static SQInteger GetStaffCount(ScriptCompany::CompanyID company, StaffRole role);

	/**
	 * Staff on a track who are free: not on any work item and not working a town.
	 * @param company The company.
	 * @param track The track.
	 * @return Free staff, or -1 for an invalid company.
	 */
	static SQInteger GetFreeStaff(ScriptCompany::CompanyID company, WorkTrack track);

	/**
	 * Sales people who are free: not working a town and not on sales work.
	 * @param company The company.
	 * @return Free reps, or -1 for an invalid company.
	 */
	static SQInteger GetFreeReps(ScriptCompany::CompanyID company);

	/**
	 * Desks in the company's office; staff beyond this cannot be hired.
	 * @param company The company.
	 * @return Desks, or -1 for an invalid company.
	 */
	static SQInteger GetDeskCount(ScriptCompany::CompanyID company);

	/**
	 * Office level: 0 garage, 1 loft, 2 office floor.
	 * @param company The company.
	 * @return The level, or -1 for an invalid company.
	 */
	static SQInteger GetOfficeLevel(ScriptCompany::CompanyID company);

	/**
	 * Cost of moving to the next office level.
	 * @param company The company.
	 * @return The cost, or -1 when already at the top level or for an invalid company.
	 */
	static Money GetOfficeUpgradeCost(ScriptCompany::CompanyID company);

	/**
	 * Monthly costs: payroll, rent, hub rent and sponsorship.
	 * @param company The company.
	 * @return Costs, or -1 for an invalid company.
	 */
	static Money GetMonthlyCosts(ScriptCompany::CompanyID company);

	/**
	 * Monthly recurring revenue.
	 * @param company The company.
	 * @return MRR, or -1 for an invalid company.
	 */
	static Money GetMRR(ScriptCompany::CompanyID company);

	/**
	 * Customers across all towns.
	 * @param company The company.
	 * @return Customers, or -1 for an invalid company.
	 */
	static SQInteger GetCustomers(ScriptCompany::CompanyID company);

	/**
	 * Current valuation.
	 * @param company The company.
	 * @return Valuation, or -1 for an invalid company.
	 */
	static Money GetValuation(ScriptCompany::CompanyID company);

	/**
	 * Funding rounds closed so far: 0 none, 1 seed, 2 series A, 3 series B, 4 series C.
	 * @param company The company.
	 * @return Rounds closed, or -1 for an invalid company.
	 */
	static SQInteger GetFundingStage(ScriptCompany::CompanyID company);

	/**
	 * Has the company gone public?
	 * @param company The company.
	 * @return True after the IPO.
	 */
	static bool IsPublic(ScriptCompany::CompanyID company);

	/**
	 * Founders' share of the company, in tenths of a percent.
	 * @param company The company.
	 * @return Equity in permille, or -1 for an invalid company.
	 */
	static SQInteger GetFounderEquity(ScriptCompany::CompanyID company);

	/**
	 * Cash offered by the pending investor offer.
	 * @param company The company.
	 * @return The amount, 0 when there is no offer, or -1 for an invalid company.
	 */
	static Money GetOfferAmount(ScriptCompany::CompanyID company);

	/**
	 * Equity asked by the pending investor offer, in tenths of a percent.
	 * @param company The company.
	 * @return Equity in permille, 0 when there is no offer, or -1 for an invalid company.
	 */
	static SQInteger GetOfferEquity(ScriptCompany::CompanyID company);

	/**
	 * Catalog item the company's next funding round needs shipped.
	 * @param company The company.
	 * @return The item, or -1 when the round needs none, there is no next round, or for an invalid company.
	 */
	static SQInteger GetNextRoundWorkItem(ScriptCompany::CompanyID company);

	/**
	 * Customers the company's next funding round needs.
	 * @param company The company.
	 * @return Customers, or -1 when there is no next round or for an invalid company.
	 */
	static SQInteger GetNextRoundCustomers(ScriptCompany::CompanyID company);

	/**
	 * MRR the company's next funding round needs.
	 * @param company The company.
	 * @return MRR, or -1 when there is no next round or for an invalid company.
	 */
	static Money GetNextRoundMRR(ScriptCompany::CompanyID company);

	/**
	 * Town the company's HQ is in.
	 * @param company The company.
	 * @return The town, or an invalid town without an HQ.
	 */
	static TownID GetHQTown(ScriptCompany::CompanyID company);

	/**
	 * Customers a company has in a town.
	 * @param town The town.
	 * @param company The company.
	 * @return Customers, or -1 for an invalid town or company.
	 */
	static SQInteger GetTownCustomers(TownID town, ScriptCompany::CompanyID company);

	/**
	 * Potential customers in a town.
	 * @param town The town.
	 * @return Market size, or -1 for an invalid town.
	 */
	static SQInteger GetTownMarketSize(TownID town);

	/**
	 * A feature category a town wants.
	 * @param town The town.
	 * @param index 0 for the main want, 1 for the second.
	 * @return The category (see GetWorkItemCategory), or -1 for an invalid town or index.
	 */
	static SQInteger GetTownWant(TownID town, SQInteger index);

	/**
	 * Product fit of a company in a town, 0 to 100.
	 * @param town The town.
	 * @param company The company.
	 * @return Fit, or -1 for an invalid town or company.
	 */
	static SQInteger GetTownFit(TownID town, ScriptCompany::CompanyID company);

	/**
	 * Market strength of a company in a town; share moves toward strength over total strength.
	 * @param town The town.
	 * @param company The company.
	 * @return Strength, or -1 for an invalid town or company.
	 */
	static SQInteger GetTownStrength(TownID town, ScriptCompany::CompanyID company);

	/**
	 * Sales reps a company has working a town.
	 * @param town The town.
	 * @param company The company.
	 * @return Reps, or -1 for an invalid town or company.
	 */
	static SQInteger GetTownReps(TownID town, ScriptCompany::CompanyID company);

	/**
	 * Can the company's reps reach the town from its HQ or a hub?
	 * @param town The town.
	 * @param company The company.
	 * @return True when in range.
	 */
	static bool IsTownInReach(TownID town, ScriptCompany::CompanyID company);

	/**
	 * Does the company have a sales hub in the town?
	 * @param town The town.
	 * @param company The company.
	 * @return True with a hub.
	 */
	static bool HasHub(TownID town, ScriptCompany::CompanyID company);

	/**
	 * Number of sales hubs.
	 * @param company The company.
	 * @return Hubs, or -1 for an invalid company.
	 */
	static SQInteger GetHubCount(ScriptCompany::CompanyID company);

	/**
	 * Company with the most customers in a town.
	 * @param town The town.
	 * @return The leader, or COMPANY_INVALID.
	 */
	static ScriptCompany::CompanyID GetTownLeader(TownID town);

	/**
	 * Number of items in the work catalog.
	 * @return Item count.
	 */
	static SQInteger GetWorkItemCount();

	/**
	 * Name of a catalog item.
	 * @param item The item.
	 * @return The name, or null for an invalid item.
	 */
	static std::optional<std::string> GetWorkItemName(SQInteger item);

	/**
	 * Track of a catalog item.
	 * @param item The item.
	 * @return The track, or -1 for an invalid item.
	 */
	static SQInteger GetWorkItemTrack(SQInteger item);

	/**
	 * Feature category of a catalog item; towns want categories.
	 * @param item The item.
	 * @return The category, -1 for business and sales items or an invalid item.
	 */
	static SQInteger GetWorkItemCategory(SQInteger item);

	/**
	 * Where a catalog item stands for a company.
	 * @param company The company.
	 * @param item The item.
	 * @return The status, or WORK_LOCKED for an invalid company or item.
	 */
	static WorkStatus GetWorkItemStatus(ScriptCompany::CompanyID company, SQInteger item);

	/**
	 * Progress of a planned item, 0 to 100.
	 * @param company The company.
	 * @param item The item.
	 * @return Percent done, or -1 when not planned.
	 */
	static SQInteger GetWorkItemProgress(ScriptCompany::CompanyID company, SQInteger item);

	/**
	 * How many people can work on an item at once.
	 * @param item The item.
	 * @return Slots, or -1 for an invalid item.
	 */
	static SQInteger GetWorkItemSlots(SQInteger item);

	/**
	 * Staff working on a planned item.
	 * @param company The company.
	 * @param item The item.
	 * @return Staff, or -1 when not planned.
	 */
	static SQInteger GetWorkItemStaff(ScriptCompany::CompanyID company, SQInteger item);

	/**
	 * Monthly salary of a role at a level; the recruiting fee is one month of it.
	 * @param role The role.
	 * @param level The level.
	 * @return The salary.
	 */
	static Money GetSalary(StaffRole role, StaffLevel level);

	/**
	 * Hire someone.
	 * @param role The role.
	 * @param level The level.
	 * @return True when hired.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool Hire(StaffRole role, StaffLevel level);

	/**
	 * Let someone go: a free person in the role if there is one, otherwise the newest hire.
	 * @param role The role.
	 * @return True when someone left.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool Fire(StaffRole role);

	/**
	 * Send a free sales rep to work a town.
	 * @param town The town.
	 * @pre GetFreeReps(COMPANY_SELF) > 0.
	 * @return True when assigned.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool AssignRep(TownID town);

	/**
	 * Take a sales rep off a town; they become free.
	 * @param town The town.
	 * @return True when a rep was taken off.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool UnassignRep(TownID town);

	/**
	 * Open a sales hub in a town.
	 * @param town The town.
	 * @return True when opened.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool OpenHub(TownID town);

	/**
	 * Close a sales hub.
	 * @param town The town.
	 * @return True when closed.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool CloseHub(TownID town);

	/**
	 * Move to the next office level.
	 * @return True when moved.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool UpgradeOffice();

	/**
	 * Put a catalog item on the roadmap.
	 * @param item The item.
	 * @return True when planned.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool PlanWorkItem(SQInteger item);

	/**
	 * Set how many people work on a planned item; they come from the item's track.
	 * @param item The item.
	 * @param people Number of people.
	 * @return True when set.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool StaffWorkItem(SQInteger item, SQInteger people);

	/**
	 * Accept or decline the pending investor offer.
	 * @param accept True to take the money.
	 * @return True when answered.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool RespondToOffer(bool accept);

	/**
	 * Sponsor a transit operator; 0 ends the sponsorship.
	 * @param transit_operator The operator.
	 * @param monthly Monthly amount: 0, 2000, 5000 or 10000.
	 * @return True when set.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	static bool Sponsor(ScriptCompany::CompanyID transit_operator, Money monthly);
};

#endif /* SCRIPT_FOUNDER_HPP */
