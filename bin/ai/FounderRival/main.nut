/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * A rival startup. Every decision goes through AIFounder, so it pays the same costs
 * and meets the same limits as the player. The state lives in the game, not here,
 * so saving and loading needs nothing extra.
 */

const BOOTSTRAPPER = 0;
const BLITZSCALER = 1;
const COPYCAT = 2;
const INCUMBENT = 3;

class FounderRival extends AIController {
	personality = 0;
	/** Months of cash to keep before hiring more. */
	target_runway = 6;
	/** Hiring mix: engineers, sales, operations, designers. */
	mix = null;
	/** Open items to keep per track. */
	open_items = 2;

	constructor()
	{
		this.mix = [3, 2, 1, 0];
	}

	function Start();
	function Save() { return {}; }
	function Load(version, data) {}
}

function FounderRival::Start()
{
	if (!AIFounder.IsFounderMode()) {
		AILog.Warning("FounderRival only plays Founder Mode games.");
		while (true) this.Sleep(10000);
	}

	this.personality = AIController.GetSetting("personality");
	switch (this.personality) {
		case BOOTSTRAPPER: this.target_runway = 12; this.mix = [3, 2, 1, 0]; this.open_items = 1; break;
		case BLITZSCALER:  this.target_runway = 4;  this.mix = [2, 3, 1, 0]; this.open_items = 2; break;
		case COPYCAT:      this.target_runway = 8;  this.mix = [2, 3, 1, 0]; this.open_items = 2; break;
		case INCUMBENT:    this.target_runway = 6;  this.mix = [2, 3, 1, 1]; this.open_items = 1; break;
	}

	this.PickName();
	while (AICompany.GetCompanyHQ(AICompany.COMPANY_SELF) == AIMap.TILE_INVALID) {
		if (!this.BuildHQ()) this.Sleep(740);
	}
	AILog.Info("Founded, personality " + this.personality);

	local tick = 0;
	while (true) {
		this.Funding();
		this.Work();
		this.Staffing();
		this.Sales();
		if (tick % 4 == 0) this.Expansion();
		tick++;
		/* About a week. */
		this.Sleep(518);
	}
}

/** A startup name nobody has taken yet. */
function FounderRival::PickName()
{
	local first = ["Nimbus", "Quarry", "Lumen", "Parcel", "Tandem", "Orbit", "Kestrel", "Juniper", "Basalt", "Fathom"];
	local last = ["Labs", "Systems", "HQ", "Works", "Cloud", "Software"];
	local start = AIBase.RandRange(first.len());
	for (local i = 0; i < first.len(); i++) {
		local name = first[(start + i) % first.len()] + " " + last[(start + i) % last.len()];
		if (AICompany.SetName(name)) return;
	}
}

/** The towns to consider for the HQ, best first for this personality. */
function FounderRival::HQCandidates()
{
	local towns = AITownList();
	towns.Valuate(AITown.GetPopulation);
	towns.Sort(AIList.SORT_BY_VALUE, false);
	/* Bootstrappers start away from the biggest towns. */
	if (this.personality == BOOTSTRAPPER && towns.Count() > 2) towns.RemoveTop(towns.Count() / 3);
	return towns;
}

/** Try to place the HQ near the centre of a good town. */
function FounderRival::BuildHQ()
{
	local towns = this.HQCandidates();
	local tried = 0;
	foreach (town, pop in towns) {
		if (tried++ >= 5) break;
		local centre = AITown.GetLocation(town);
		local tiles = AITileList();
		local x = AIMap.GetTileX(centre);
		local y = AIMap.GetTileY(centre);
		tiles.AddRectangle(AIMap.GetTileIndex(max(1, x - 10), max(1, y - 10)), AIMap.GetTileIndex(min(AIMap.GetMapSizeX() - 2, x + 10), min(AIMap.GetMapSizeY() - 2, y + 10)));
		tiles.Valuate(AIMap.DistanceManhattan, centre);
		tiles.Sort(AIList.SORT_BY_VALUE, true);
		foreach (tile, d in tiles) {
			if (AICompany.BuildCompanyHQ(tile)) return true;
		}
	}
	return false;
}

/** Months of cash at the current net burn; 999 when profitable. */
function FounderRival::Runway()
{
	local net = AIFounder.GetMonthlyCosts(AICompany.COMPANY_SELF) - AIFounder.GetMRR(AICompany.COMPANY_SELF);
	if (net <= 0) return 999;
	local cash = AICompany.GetBankBalance(AICompany.COMPANY_SELF);
	return cash <= 0 ? 0 : cash / net;
}

/** Answer investor offers. Bootstrappers only take money when they need it. */
function FounderRival::Funding()
{
	local amount = AIFounder.GetOfferAmount(AICompany.COMPANY_SELF);
	if (amount <= 0) return;
	local accept = this.personality != BOOTSTRAPPER || this.Runway() < 6;
	AIFounder.RespondToOffer(accept);
	AILog.Info((accept ? "Accepted " : "Declined ") + "an offer of " + amount);
}

/** The next funding round's item comes first, then engineering items the HQ town wants. */
function FounderRival::ItemScore(item)
{
	if (item == AIFounder.GetNextRoundWorkItem(AICompany.COMPANY_SELF)) return 10;
	local cat = AIFounder.GetWorkItemCategory(item);
	local town = AIFounder.GetHQTown(AICompany.COMPANY_SELF);
	if (cat >= 0 && AITown.IsValidTown(town)) {
		if (cat == AIFounder.GetTownWant(town, 0)) return 2;
		if (cat == AIFounder.GetTownWant(town, 1)) return 1;
	}
	return 0;
}

/** Keep each track's roadmap filled and its free staff working. */
function FounderRival::Work()
{
	local count = AIFounder.GetWorkItemCount();
	foreach (track in [AIFounder.TRACK_ENGINEERING, AIFounder.TRACK_BUSINESS, AIFounder.TRACK_SALES]) {
		local open = [];
		local best = -1;
		local best_score = -1;
		for (local item = 0; item < count; item++) {
			if (AIFounder.GetWorkItemTrack(item) != track) continue;
			local status = AIFounder.GetWorkItemStatus(AICompany.COMPANY_SELF, item);
			if (status == AIFounder.WORK_BACKLOG || status == AIFounder.WORK_IN_PROGRESS) open.append(item);
			if (status == AIFounder.WORK_AVAILABLE) {
				local score = this.ItemScore(item);
				if (score > best_score) {
					best = item;
					best_score = score;
				}
			}
		}
		if (open.len() < this.open_items && best >= 0 && AIFounder.PlanWorkItem(best)) open.append(best);
		if (open.len() == 0) continue;

		/* Staff the most important open item. */
		local item = open[0];
		foreach (o in open) {
			if (this.ItemScore(o) > this.ItemScore(item)) item = o;
		}
		local staff = AIFounder.GetWorkItemStaff(AICompany.COMPANY_SELF, item);
		local free = AIFounder.GetFreeStaff(AICompany.COMPANY_SELF, track);
		/* Sales people mostly work towns; lend at most one to sales-track work. */
		if (track == AIFounder.TRACK_SALES) free = min(free, 1 - staff);
		if (free > 0) AIFounder.StaffWorkItem(item, staff + free);
	}
}

/** Is anything planned or plannable on this track? */
function FounderRival::HasOpenWork(track)
{
	local count = AIFounder.GetWorkItemCount();
	for (local item = 0; item < count; item++) {
		if (AIFounder.GetWorkItemTrack(item) != track) continue;
		local status = AIFounder.GetWorkItemStatus(AICompany.COMPANY_SELF, item);
		if (status == AIFounder.WORK_AVAILABLE || status == AIFounder.WORK_BACKLOG || status == AIFounder.WORK_IN_PROGRESS) return true;
	}
	return false;
}

/** Hire while runway allows, move office when desks run out, cut when cash runs low. */
function FounderRival::Staffing()
{
	local roles = [AIFounder.ROLE_ENGINEER, AIFounder.ROLE_SALES, AIFounder.ROLE_OPERATIONS, AIFounder.ROLE_DESIGNER];
	local counts = [];
	local total = 0;
	foreach (role in roles) {
		local n = AIFounder.GetStaffCount(AICompany.COMPANY_SELF, role);
		counts.append(n);
		total += n;
	}
	local runway = this.Runway();

	if (runway < 2 && total > 1) {
		/* Let go of the role furthest over its share of the mix, keeping one of each. */
		local worst = -1;
		local worst_ratio = -1;
		for (local i = 0; i < roles.len(); i++) {
			if (counts[i] <= 1) continue;
			local ratio = this.mix[i] == 0 ? 1000 : counts[i] * 100 / this.mix[i];
			if (ratio > worst_ratio) {
				worst = i;
				worst_ratio = ratio;
			}
		}
		if (worst >= 0) AIFounder.Fire(roles[worst]);
		return;
	}
	/* A track with waiting work and nobody to do it gets its first person while cash lasts. */
	if (runway >= 4) {
		local first = [[AIFounder.TRACK_BUSINESS, 2], [AIFounder.TRACK_SALES, 1], [AIFounder.TRACK_ENGINEERING, 0]];
		foreach (pair in first) {
			if (counts[pair[1]] == 0 && this.HasOpenWork(pair[0])) {
				AIFounder.Hire(roles[pair[1]]);
				return;
			}
		}
	}
	if (runway <= this.target_runway) return;
	/* Would one more person, at the average cost per head, still leave the target runway? */
	local costs = AIFounder.GetMonthlyCosts(AICompany.COMPANY_SELF);
	local net = costs + costs / max(1, total) - AIFounder.GetMRR(AICompany.COMPANY_SELF);
	if (net > 0 && AICompany.GetBankBalance(AICompany.COMPANY_SELF) / net <= this.target_runway) return;

	if (total >= AIFounder.GetDeskCount(AICompany.COMPANY_SELF)) {
		local cost = AIFounder.GetOfficeUpgradeCost(AICompany.COMPANY_SELF);
		if (cost > 0 && AICompany.GetBankBalance(AICompany.COMPANY_SELF) > cost * 3) AIFounder.UpgradeOffice();
		return;
	}

	/* Hire the role furthest under its share of the mix. */
	local pick = 0;
	local pick_ratio = 1 << 30;
	for (local i = 0; i < roles.len(); i++) {
		if (this.mix[i] == 0) continue;
		local ratio = counts[i] * 100 / this.mix[i];
		if (ratio < pick_ratio) {
			pick = i;
			pick_ratio = ratio;
		}
	}
	AIFounder.Hire(roles[pick]);
}

/** Send free reps to the reachable town with the most customers left to win per rep. */
function FounderRival::Sales()
{
	while (AIFounder.GetFreeReps(AICompany.COMPANY_SELF) > 0) {
		local best = -1;
		local best_score = -1;
		foreach (town, _ in AITownList()) {
			if (!AIFounder.IsTownInReach(town, AICompany.COMPANY_SELF)) continue;
			local open = AIFounder.GetTownMarketSize(town) - AIFounder.GetTownCustomers(town, AICompany.COMPANY_SELF);
			local score = open / (AIFounder.GetTownReps(town, AICompany.COMPANY_SELF) + 1);
			if (score > best_score) {
				best = town;
				best_score = score;
			}
		}
		if (best < 0 || !AIFounder.AssignRep(best)) return;
	}
}

/** Open a sales hub when there is cash to spare, in a town chosen by personality. */
function FounderRival::Expansion()
{
	local self = AICompany.ResolveCompanyID(AICompany.COMPANY_SELF);
	local cash = AICompany.GetBankBalance(self);
	local hubs = AIFounder.GetHubCount(self);
	local reps = AIFounder.GetStaffCount(self, AIFounder.ROLE_SALES);
	local want = 0;
	switch (this.personality) {
		case BOOTSTRAPPER: want = this.Runway() > 18 ? reps / 6 : 0; break;
		case BLITZSCALER:  want = reps / 2; break;
		case COPYCAT:      want = reps / 3; break;
		case INCUMBENT:    want = reps / 3; break;
	}
	if (hubs >= want || cash < 60000) return;

	local hq = AIFounder.GetHQTown(self);
	local best = -1;
	local best_score = -1;
	foreach (town, _ in AITownList()) {
		if (town == hq || AIFounder.HasHub(town, self)) continue;
		local score = AIFounder.GetTownMarketSize(town);
		if (this.personality == COPYCAT) {
			/* Follow the leader: towns another startup leads count most. */
			local leader = AIFounder.GetTownLeader(town);
			if (leader != AICompany.COMPANY_INVALID && leader != self && AIFounder.IsStartup(leader)) {
				score = AIFounder.GetTownCustomers(town, leader) * 3;
			}
		} else if (AIFounder.IsTownInReach(town, self)) {
			/* Already reachable: a hub adds less. */
			score = score / 2;
		}
		if (score > best_score) {
			best = town;
			best_score = score;
		}
	}
	if (best >= 0 && AIFounder.OpenHub(best)) AILog.Info("Opened a hub in " + AITown.GetName(best));
}
