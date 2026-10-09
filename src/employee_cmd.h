/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file employee_cmd.h Command definitions related to Founder Mode employees. */

#ifndef EMPLOYEE_CMD_H
#define EMPLOYEE_CMD_H

#include "command_type.h"
#include "employee_type.h"
#include "town_type.h"
#include "company_type.h"
#include "economy_type.h"

CommandCost CmdHireEmployee(DoCommandFlags flags, EmployeeRole role);
CommandCost CmdFireEmployee(DoCommandFlags flags, EmployeeID employee);
CommandCost CmdAssignRep(DoCommandFlags flags, EmployeeID employee, TownID town);
CommandCost CmdSetHub(DoCommandFlags flags, TownID town, bool open);
CommandCost CmdSponsorOperator(DoCommandFlags flags, CompanyID op, Money monthly);
CommandCost CmdRespondFundingOffer(DoCommandFlags flags, bool accept);

DEF_CMD_TRAIT(Commands::HireEmployee, CmdHireEmployee, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::FireEmployee, CmdFireEmployee, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::AssignRep, CmdAssignRep, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::SetHub, CmdSetHub, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::SponsorOperator, CmdSponsorOperator, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::RespondFundingOffer, CmdRespondFundingOffer, {}, CommandType::OtherManagement)

#endif /* EMPLOYEE_CMD_H */
