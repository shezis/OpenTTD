/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file feature_cmd.h Command definitions related to Founder Mode product features. */

#ifndef FEATURE_CMD_H
#define FEATURE_CMD_H

#include "command_type.h"
#include "feature_type.h"

CommandCost CmdCreateFeature(DoCommandFlags flags, FeatureCategory category);
CommandCost CmdAssignFeature(DoCommandFlags flags, FeatureID feature, uint8_t engineers);
CommandCost CmdShipFeature(DoCommandFlags flags, FeatureID feature);

DEF_CMD_TRAIT(Commands::CreateFeature, CmdCreateFeature, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::AssignFeature, CmdAssignFeature, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::ShipFeature, CmdShipFeature, {}, CommandType::OtherManagement)

#endif /* FEATURE_CMD_H */
