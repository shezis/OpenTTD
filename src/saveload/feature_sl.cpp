/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file feature_sl.cpp Code handling saving and loading of Founder Mode features. */

#include "../stdafx.h"

#include "saveload.h"

#include "../feature_base.h"

#include "../safeguards.h"

static const SaveLoad _features_desc[] = {
	SaveLoad::Variable<VarFileType::U8>("company", SLE_OBJECT_ADDRESS(Feature, company)),
	SaveLoad::Variable<VarFileType::U8>("spec", SLE_OBJECT_ADDRESS(Feature, spec), SaveLoadVersion::FounderModeWorkTracks),
	SaveLoad::Variable<VarFileType::U8>("state", SLE_OBJECT_ADDRESS(Feature, state)),
	SaveLoad::Variable<VarFileType::U8>("assigned", SLE_OBJECT_ADDRESS(Feature, assigned)),
	SaveLoad::Variable<VarFileType::U8>("quality", SLE_OBJECT_ADDRESS(Feature, quality)),
	SaveLoad::Variable<VarFileType::U8>("bugs", SLE_OBJECT_ADDRESS(Feature, bugs)),
	SaveLoad::Variable<VarFileType::U16>("effort", SLE_OBJECT_ADDRESS(Feature, effort)),
	SaveLoad::Variable<VarFileType::U32>("progress", SLE_OBJECT_ADDRESS(Feature, progress)),
};

struct FEATChunkHandler : ChunkHandler {
	FEATChunkHandler() : ChunkHandler("FEAT", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_features_desc);

		for (Feature *f : Feature::Iterate()) {
			SlSetArrayIndex(f->index);
			SlObject(f, _features_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlTableHeader(_features_desc);

		int index;
		while ((index = SlIterateArray()) != -1) {
			Feature *f = Feature::CreateAtIndex(FeatureID(index));
			SlObject(f, slt);
		}
	}
};

static const FEATChunkHandler FEAT;
static const ChunkHandlerRef feature_chunk_handlers[] = {
	FEAT,
};

extern const ChunkHandlerTable _feature_chunk_handlers(feature_chunk_handlers);
