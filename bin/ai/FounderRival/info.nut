/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** Founder Mode rival startup. Plays through AIFounder with the same rules as the player. */
class FounderRival extends AIInfo {
	function GetAuthor()        { return "Founder Mode"; }
	function GetName()          { return "FounderRival"; }
	function GetDescription()   { return "A rival startup for Founder Mode. Hires, ships, sells and raises money by the player's rules."; }
	function GetVersion()       { return 1; }
	function MinVersionToLoad() { return 1; }
	function GetDate()          { return "2026-10-09"; }
	function CreateInstance()   { return "FounderRival"; }
	function GetShortName()     { return "FMRV"; }
	function GetAPIVersion()    { return "16"; }
	function UseAsRandomAI()    { return false; }

	function GetSettings() {
		AddSetting({
			name = "personality",
			description = "How this rival plays",
			min_value = 0,
			max_value = 3,
			default_value = 0,
			flags = CONFIG_NONE
		});
		AddLabels("personality", {
			_0 = "Bootstrapper: grows slowly near break-even",
			_1 = "Blitzscaler: raises early and expands fast",
			_2 = "Copycat: opens hubs in the leader's best towns",
			_3 = "Incumbent: starts big, ships slowly"
		});
	}
}

RegisterAI(FounderRival());
