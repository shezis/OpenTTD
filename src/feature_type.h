/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file feature_type.h Basic types related to Founder Mode product features. */

#ifndef FEATURE_TYPE_H
#define FEATURE_TYPE_H

#include "core/pool_type.hpp"

using FeatureID = PoolID<uint16_t, struct FeatureIDTag, 64000, 0xFFFF>;

/** Kind of feature; towns will want different categories (Phase 4). */
enum class FeatureCategory : uint8_t {
	Core, ///< Core product.
	Mobile, ///< Mobile apps.
	Payments, ///< Billing and payments.
	Analytics, ///< Reports and insights.
	Integrations, ///< Connections to other tools.
	Security, ///< Security and compliance.
	End, ///< End marker.
};

/** Where a feature is on the roadmap. */
enum class FeatureState : uint8_t {
	Backlog, ///< Planned, nobody working on it.
	InProgress, ///< Being built.
	Shipped, ///< Released to customers.
};

struct Feature;

static constexpr uint MAX_OPEN_FEATURES_PER_COMPANY = 30; ///< Backlog plus in-progress limit.
static constexpr uint SHIP_EARLY_MIN_PERCENT = 50; ///< Minimum progress before shipping early.

#endif /* FEATURE_TYPE_H */
