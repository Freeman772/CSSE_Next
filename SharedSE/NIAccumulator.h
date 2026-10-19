#pragma once

#include "NIDefines.h"

#include "NIObject.h"

namespace NI {
	struct Accumulator_vTable : Object_vTable {
		bool (__thiscall* startAccumulating)(Accumulator*, Camera*);
		void (__thiscall* finishAccumulating)(Accumulator*);
		bool (__thiscall* registerObject)(Accumulator*, Geometry*);
	};
	static_assert(sizeof(Accumulator_vTable) == 0x38, "NI::Accumulator's vtable failed size validation");

	struct Accumulator : Object {
		Pointer<Camera> camera;
	};
	static_assert(sizeof(Accumulator) == 0xC, "NI::Accumulator failed size validation");
}