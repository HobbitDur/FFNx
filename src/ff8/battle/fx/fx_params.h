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
#include <string.h>

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

	// a random value on each axis
	struct Range3
	{
		Range x, y, z;
	};

	// value * num / den, at most max (integer, as the game computes it: the product's low 16 bits,
	// then the cap) - a size from the target's, as Cure's ring radius
	struct Scaled
	{
		int16_t num, den, max;
		int16_t of(int32_t value) const
		{
			const int16_t v = (int16_t)(value * num / den);
			return v > max ? max : v;
		}
	};

	// a random share: a numerator at random in [min, max], over den
	struct RandomFraction
	{
		int16_t min, max, den;
		int32_t roll(int32_t random) const { return min + random % (max - min + 1); }
	};

	struct Size
	{
		int16_t w, h;
	};

	// a colour multiplier (128 = unchanged), as the sprite drawer takes it (its workspace +0x1C,
	// flag 4): a piece's colour = its texel x this x its intensity >> 14
	struct Colour
	{
		uint8_t r, g, b, pad;
		bool unchanged() const { return r == 0x80 && g == 0x80 && b == 0x80; }
		uint32_t word() const { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16; }
	};

	// the shared effect library's glint (Effect_Glint_Tick 0x8DCC20, fx_glint.cpp): a streak that
	// grows for `ticks`, then shrinks for as many, its colours stepping, spinning about Y
	struct GlintStyle
	{
		int16_t ticks;                      // ticks per phase
		Size grow;                          // size added per tick while growing
		Size shrink;                        // size taken per tick while shrinking
		uint8_t centre_step[3];             // centre colour added / taken per tick (wrapping bytes)
		uint8_t tip_step[3];                // tip colour
		int16_t spin;                       // Y angle per tick, either way at random
	};

	// a sprite sequence parameter (the flipbooks of FF8_EN.exe's data, InitEffectSequenceFromData
	// 0x571C80): its address when it is one - in the exe image, the signature 24 05 01 00 - else the
	// game's, so a wrong value draws the game's sprite instead of reading anywhere
	inline uint32_t sequence(uint32_t address, uint32_t game)
	{
		constexpr uint32_t image_begin = 0x400000, image_end = 0x282B000;    // FF8_EN.exe in memory
		if (address == game || address < image_begin || address + 12 > image_end || address & 3) return game;
		return *(const uint32_t *)address == 0x00010524 ? address : game;
	}

	// a sound parameter (the battle's sound blocks, BdPlaySE 0x501330: it reads their leading world
	// sound id only): its address when it can be one - in the exe image, aligned - else the game's
	inline uint32_t sound(uint32_t address, uint32_t game)
	{
		constexpr uint32_t image_begin = 0x400000, image_end = 0x282B000;    // FF8_EN.exe in memory
		return address == game || address < image_begin || address + 4 > image_end || address & 3 ? game : address;
	}

	// a file name parameter (the files an effect's loader reads from the magic folder,
	// IO_GetFile_MAGIC 0x571B80): empty = the game's own file
	struct FileName { char name[16]; };
	inline const char *file(const FileName &f, const char *game)
	{
		return f.name[0] && memchr(f.name, 0, sizeof(f.name)) ? f.name : game;
	}

	// a flipbook's last frame played, within its sequence (frame count at +8): past it the game would
	// read the frame list's end as a frame
	inline int16_t last_frame(uint32_t sequence, int16_t last)
	{
		const int16_t count = (int16_t)*(const uint16_t *)(sequence + 8);
		return last < 0 ? 0 : last >= count ? (int16_t)(count - 1) : last;
	}

	// ---- field table (what an editor shows) ----
	// Seq: a sprite sequence's address (32 bits): tools offer the exe's sequences; Sound: a sound
	// block's address (32 bits)
	enum FieldType : uint8_t { I16 = 0, U8 = 1, I32 = 2, Seq = 3, Sound = 4, File = 5 };
	// how an editor shows the value (the raw value beside it): Angle 4096 = a full turn (degrees),
	// Scale 4096 = its own size (x 1)
	enum Unit : uint8_t { Plain = 0, Angle = 1, Scale = 2 };
	struct Field
	{
		const char *part;    // the part it belongs to (PartDesc::key)
		const char *key;     // unique name, e.g. "ring.radius.num"
		const char *label;   // shown name
		const char *doc;     // what it does
		uint32_t offset;     // in the module's parameter struct
		FieldType type;
		int32_t min, max;    // editor bounds
		Unit unit = Plain;
	};
	struct Part
	{
		int id;              // FX_PART tag (0: draws nothing itself)
		const char *key;
		const char *title;
		const char *doc;
	};
	// values edited together: the fields of one sub-structure (a field belongs to the group whose
	// key starts its own, followed by '.' or '['), e.g. "ring.radius": num, den, max
	struct Group
	{
		const char *key;
		const char *label;
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
		const Group *groups = nullptr;
		int group_count = 0;
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
// FX_NOTE(name, value): a value the port measures (the target's size, the radius it takes...), noted by
// the lab with its tick for the tools to show (Fujin's geometry view); nothing in the game
namespace ff8fx { void fx_note(const char *name, int32_t value); }
#define FX_NOTE(name, value) ff8fx::fx_note(name, (int32_t)(value))
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
#define FX_NOTE(name, value) ((void)0)
#define FX_LAB_MODULE(fn) static_assert(true, "")
#endif
