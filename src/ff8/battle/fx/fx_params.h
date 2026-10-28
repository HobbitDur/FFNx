/****************************************************************************/
//    Copyright (C) 2026 HobbitDur                                          //
//                                                                          //
//    This file is part of FFNx                                             //
//                                                                          //
//    FFNx is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU General Public License as published by  //
//    the Free Software Foundation, either version 3 of the License         //
//                                                                          //
//    FFNx is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//    GNU General Public License for more details.                          //
/****************************************************************************/

// Effect parameters: the numbers a ported effect is made of, out of its code, in generic blocks
// every module can reuse (a tick window, a count per tick, a random range, a fraction, the shared
// effect library's glint style). Each module keeps its parameters in one struct whose defaults
// are the game's own values (so the port stays exact), describes them in a field table, and
// names its parts (what it draws: a ring, its sparkles...). Tools read the table to edit them
// (FF8UltimateEditor's Fujin: Choreography, one tab per part) and run the effect with other values.
//
// Part tags: FX_PART(id) says which part draws now. Only the effect lab build (FF8_FX_LAB,
// FF8UltimateEditor's effect recorder) records it with each primitive; in the game it is nothing.
// FX_LAB_MODULE(register function): a module with parameters registers itself in the lab build
// (the recorder builds every source file naming it); in the game it is nothing (fx_port.cpp).

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ff8fx
{
namespace fxp
{
	// [first, last] ticks of a node's counter (both included)
	struct Window
	{
		int16_t first, last;
		bool has(int32_t tick) const { return tick >= first && tick <= last; }
	};

	// how many per tick: from[k] the tick step k starts at (increasing), count[k] how many from
	// there; the last step reached counts
	struct Schedule
	{
		int16_t from[4];
		int16_t count[4];
		int32_t at(int32_t tick) const
		{
			int32_t n = 0;
			for (int k = 0; k < 4; k++)
				if (tick >= from[k]) n = count[k];
			return n;
		}
	};

	// a random integer in [min, max]: min + rand() % (max - min + 1) - the game's `rand() & mask`
	// for its power-of-two spans (rand() is never negative)
	struct Range
	{
		int16_t min, max;
		int32_t roll(int32_t random) const { return min + random % (max - min + 1); }
	};

	// value * num / den (integer, as the game computes it)
	struct Fraction
	{
		int16_t num, den;
		int32_t of(int32_t value) const { return value * num / den; }
	};

	// the shared effect library's glint (Effect_Glint_Tick 0x8DCC20, fx_glint.cpp): a streak that
	// grows for `ticks`, then shrinks for as many, its colours stepping, spinning about Y
	struct GlintStyle
	{
		int16_t ticks;                      // ticks per phase
		int16_t grow_w, grow_h;             // size added per tick while growing
		int16_t shrink_w, shrink_h;         // size taken per tick while shrinking
		uint8_t centre_step[3];             // centre colour added / taken per tick (wrapping bytes)
		uint8_t tip_step[3];                // tip colour
		int16_t spin;                       // Y angle per tick, either way at random
	};

	// ---- field table (what an editor shows) ----
	enum FieldType : uint8_t { I16 = 0, U8 = 1, I32 = 2 };
	struct Field
	{
		const char *part;    // the part it belongs to (PartDesc::key)
		const char *key;     // unique name, e.g. "ring.radius.num"
		const char *label;   // shown name
		const char *doc;     // what it does
		uint32_t offset;     // in the module's parameter struct
		FieldType type;
		int32_t min, max;    // editor bounds
	};
	struct Part
	{
		int id;              // FX_PART tag (0: draws nothing itself)
		const char *key;
		const char *title;
		const char *doc;
	};
	struct Module
	{
		int effect_id;
		const char *name;
		void *params;        // the live parameters the module reads
		const void *defaults;
		uint32_t size;
		const Part *parts;
		int part_count;
		const Field *fields;
		int field_count;
	};

	// every module's parameters, registered by its register function
	inline Module g_modules[64];
	inline int g_module_count = 0;
	inline void register_params(const Module &m)
	{
		for (int k = 0; k < g_module_count; k++)
			if (g_modules[k].effect_id == m.effect_id) return;
		if (g_module_count < 64) g_modules[g_module_count++] = m;
	}
	inline const Module *find_params(int effect_id)
	{
		for (int k = 0; k < g_module_count; k++)
			if (g_modules[k].effect_id == effect_id) return &g_modules[k];
		return nullptr;
	}
}
}

#ifdef FF8_FX_LAB
namespace ff8fx { void fx_part(int id); }   // the lab records where each part starts drawing
#define FX_PART(id) ff8fx::fx_part(id)
namespace ff8fx
{
namespace fxp
{
	using RegisterFn = void (*)();
	inline RegisterFn g_lab_modules[64];
	inline int g_lab_module_count = 0;
	struct LabModule
	{
		LabModule(RegisterFn f) { if (g_lab_module_count < 64) g_lab_modules[g_lab_module_count++] = f; }
	};
}
}
#define FX_LAB_MODULE(fn) static ff8fx::fxp::LabModule fx_lab_module_##fn(fn)
#else
#define FX_PART(id) ((void)0)
#define FX_LAB_MODULE(fn) static_assert(true, "")
#endif
