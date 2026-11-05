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

	// the data blocks a data parameter may point into besides the exe image: a new effect's own copies
	// of the exe's data (its flipbooks, sounds, layouts), each registered by the host of the program
	// that holds it (FF8UltimateEditor's Tools/fxhost/fxprogram.h) and dropped with that program
	struct DataBlock { const void *owner; uint32_t begin, end; };
	inline DataBlock g_data_blocks[1024];
	inline int g_data_block_count = 0;
	inline void add_data(const void *owner, uint32_t begin, uint32_t size)
	{
		if (g_data_block_count < 1024) g_data_blocks[g_data_block_count++] = { owner, begin, begin + size };
	}
	inline void drop_data(const void *owner)
	{
		int n = 0;
		for (int k = 0; k < g_data_block_count; k++)
			if (g_data_blocks[k].owner != owner) g_data_blocks[n++] = g_data_blocks[k];
		g_data_block_count = n;
	}
	// the end of what [address, + size) lies in: the exe image or a data block (0: neither)
	inline uint32_t data_end(uint32_t address, uint32_t size)
	{
		constexpr uint32_t image_begin = 0x400000, image_end = 0x282B000;    // FF8_EN.exe in memory
		if (address + size < address) return 0;
		if (address >= image_begin && address + size <= image_end) return image_end;
		for (int k = 0; k < g_data_block_count; k++)
			if (address >= g_data_blocks[k].begin && address + size <= g_data_blocks[k].end) return g_data_blocks[k].end;
		return 0;
	}
#define FXP_DATA_BLOCKS 1   // (hosts test it: older sources have no data blocks)

	// a sprite sequence parameter (the flipbooks of FF8_EN.exe's data, InitEffectSequenceFromData
	// 0x571C80): its address when it is one - in the exe image or a data block, the signature
	// 24 05 01 00, its frame table in it and at least the game's frames (its code plays a fixed count) -
	// else the game's, so a wrong value draws the game's sprite instead of reading anywhere
	inline uint32_t sequence(uint32_t address, uint32_t game)
	{
		if (address == game || address & 3 || !data_end(address, 12)) return game;
		if (*(const uint32_t *)address != 0x00010524) return game;
		const uint32_t frames = *(const uint16_t *)(address + 8);
		if (frames < *(const uint16_t *)(game + 8) || !data_end(address, 10 + 2 * frames)) return game;
		return address;
	}

	// a sound parameter (the battle's sound blocks, BdPlaySE 0x501330: it reads their leading world
	// sound id only): its address when it can be one - in the exe image or a data block, aligned -
	// else the game's
	inline uint32_t sound(uint32_t address, uint32_t game)
	{
		return address == game || address & 3 || !data_end(address, 4) ? game : address;
	}

	// a prim-model layout parameter (the effect library's prim player's: Effect_DecodeModelPrimLayout
	// 0x7016B0 reads it, FF8UltimateEditor's FF8GameData/psxgpu/primlayout.py its format): its address
	// when it reads as one - in the exe image or a data block, aligned, its model count, its header
	// (object count, length) and objects inside what it lies in, the player state it needs within
	// `capacity` (the caller's block, the size it decodes it with) - else the game's
	inline uint32_t layout(uint32_t address, uint32_t game, uint32_t capacity)
	{
		if (address == game || address & 3) return game;
		const uint32_t end = data_end(address, 8);
		if (!end) return game;
		const uint32_t models = *(const uint32_t *)address;
		const int32_t at = *(const int32_t *)(address + 4);
		if (models > 256 || at < 8 || (uint32_t)at + 8 > end - address) return game;
		const uint32_t header = address + at;
		const int32_t objects = *(const int32_t *)header, frames = *(const int32_t *)(header + 4);
		if (objects < 0 || objects > 256 || frames <= 0 || header + 8 + 4 * (uint32_t)objects > end) return game;
		// the decoder's state: the data pointer and the frame, then each object's integrator slots by its track flags
		uint32_t words = 2;
		for (int32_t k = 0; k < objects; k++)
		{
			const uint32_t object = header + *(const int32_t *)(header + 8 + 4 * k);
			if (object < header || object + 8 > end) return game;
			const uint32_t tf = *(const uint32_t *)(object + 4);
			words += tf & 4 ? 9 : tf & 2 ? 6 : tf & 1 ? 3 : 0;            // position
			words += tf & 0x20 ? 9 : tf & 0x10 ? 6 : tf & 8 ? 3 : 0;      // rotation
			words += tf & 0x100 ? 9 : tf & 0x80 ? 6 : tf & 0x40 ? 3 : 0;  // scale
			words += tf & 0x800 ? 6 : tf & 0x400 ? 4 : tf & 0x200 ? 2 : 0; // colour
			words += (tf & 0x1000 ? 3 : 0) + (tf & 0x2000 ? 4 : 0);        // channels A, B
		}
		return words * 4 <= capacity ? address : game;
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
	// block's address (32 bits); Layout: a prim-model layout's address (32 bits)
	enum FieldType : uint8_t { I16 = 0, U8 = 1, I32 = 2, Seq = 3, Sound = 4, File = 5, Layout = 6 };
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

	// every module's parameters, registered by its register function (room for every effect)
	inline Module g_modules[512];
	inline int g_module_count = 0;
	inline void register_params(const Module &m)
	{
		for (int k = 0; k < g_module_count; k++)
			if (g_modules[k].effect_id == m.effect_id) return;
		if (g_module_count < 512) g_modules[g_module_count++] = m;
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
	inline RegisterFn g_lab_modules[512];   // (every lab module: past 64 the last ones were dropped)
	inline int g_lab_module_count = 0;
	struct LabModule
	{
		LabModule(RegisterFn f) { if (g_lab_module_count < 512) g_lab_modules[g_lab_module_count++] = f; }
	};
}
}
#define FX_LAB_MODULE(fn) static ff8fx::fxp::LabModule fx_lab_module_##fn(fn)
#else
#define FX_PART(id) ((void)0)
#define FX_NOTE(name, value) ((void)0)
#define FX_LAB_MODULE(fn) static_assert(true, "")
#endif
