/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file employee_sl.cpp Code handling saving and loading of Founder Mode employees. */

#include "../stdafx.h"

#include "saveload.h"

#include "../employee_base.h"

#include "../safeguards.h"

static const SaveLoad _employees_desc[] = {
	SaveLoad::Variable<VarFileType::U8>("company", SLE_OBJECT_ADDRESS(Employee, company)),
	SaveLoad::Variable<VarFileType::U8>("role", SLE_OBJECT_ADDRESS(Employee, role)),
	SaveLoad::Variable<VarFileType::U16>("name_index", SLE_OBJECT_ADDRESS(Employee, name_index)),
	SaveLoad::Variable<VarFileType::U8>("skill", SLE_OBJECT_ADDRESS(Employee, skill)),
	SaveLoad::Variable<VarFileType::U8>("morale", SLE_OBJECT_ADDRESS(Employee, morale)),
	SaveLoad::Variable<VarFileType::I64>("salary", SLE_OBJECT_ADDRESS(Employee, salary)),
};

struct EMPLChunkHandler : ChunkHandler {
	EMPLChunkHandler() : ChunkHandler("EMPL", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_employees_desc);

		for (Employee *e : Employee::Iterate()) {
			SlSetArrayIndex(e->index);
			SlObject(e, _employees_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlTableHeader(_employees_desc);

		int index;
		while ((index = SlIterateArray()) != -1) {
			Employee *e = Employee::CreateAtIndex(EmployeeID(index));
			SlObject(e, slt);
		}
	}
};

static const EMPLChunkHandler EMPL;
static const ChunkHandlerRef employee_chunk_handlers[] = {
	EMPL,
};

extern const ChunkHandlerTable _employee_chunk_handlers(employee_chunk_handlers);
