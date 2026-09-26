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

// What one tick of a battle effect (magic / GF module) can touch: shared by the differential
// verifier of the native effect code (fx_verify.cpp) and by the 30 fps look-ahead of the
// True30FPS branch, so both snapshot exactly the same state.
//
//   modules[]         per-effect table: the module's own globals (FF8 was compiled per source
//                     file, so they are contiguous: data_lo..data_hi), one extra cell the module
//                     steps outside that range, and "streams" = .data buffers its streamed files
//                     are loaded into plus other cells outside the range it writes
//   engine_regions[]  engine state any effect tick may modify
//   ext_sites[]       engine functions with external side effects (sound, file loads, damage,
//                     task spawns into engine queues, off-screen renders...). The verifier runs
//                     them once (original run, recorded) and replays their result and the bytes
//                     they wrote for the port run.

#pragma once

#include <stdint.h>

namespace ff8fx::mod
{
	struct Region { uint32_t addr, size; const char *name; };

	struct Module
	{
		int effect_id;              // *(int *)0x1D99A68 + 1 while the effect runs
		uint32_t data_lo, data_hi;  // module globals
		uint32_t extra, extra_size; // one more cell outside the range (0 = none)
		bool lookahead;             // True30FPS: held frames use the look-ahead
		const char *name;
		const Region *streams;
		int nstreams;
	};

	// arity 0 = arguments not verified yet: only the call itself is compared
	struct ExtSite { uint32_t addr; int arity; const char *name; };

	extern const Module modules[];
	extern const int module_count;
	extern const Region engine_regions[];
	extern const int engine_region_count;
	extern const ExtSite ext_sites[];
	extern const int ext_site_count;

	// snapshot limits (Siren's module globals are 0x10454 bytes; Gilgamesh's streams are the
	// sword files 0x10C50 + summon data 0x40000 + cells): a module above them is refused
	const uint32_t DATA_MAX = 0x20000;
	const uint32_t STREAMS_MAX = 0x80000;

	const Module *find_module(int effect_id); // nullptr when the effect has no entry
	uint32_t streams_bytes(const Module &m);
	inline bool fits(const Module &m)
	{
		return m.data_hi >= m.data_lo && m.data_hi - m.data_lo <= DATA_MAX && streams_bytes(m) <= STREAMS_MAX
			&& (m.extra_size <= 16);
	}

	// engine memory the verifier also snapshots and compares (addresses of FF8_EN 1.2 US)
	const uint32_t SSIGPU_ARENA = 0x1C48828;         // ssigpu_execution_start: 24-byte OT nodes
	const uint32_t SSIGPU_ARENA_SIZE = 0x60000;      // up to its cursor cell 0x1CA8828
	const uint32_t RENDER_LIST_BASE_PTR = 0x1D8E04C; // g_Battle_FrameRenderListBase (OT span base)
	const uint32_t OT_SPAN_WORDS = 17 + 4386;        // 17 header buckets (some draws use them) + the OT
	const uint32_t FRAME_PACKET_CUR_PTR = 0x1D8E054; // battle_texture_data_ptr: frame packet arena cursor
	const uint32_t FRAME_PACKET_END_PTR = 0x1D969A8; // its limit (set with it per display parity)
	const uint32_t FIELD_ALLOC_PTR = 0x1D999C4;      // Field_Alloc bump pointer (GLOBAL_MEMORY_POOL)
	const uint32_t FIELD_ALLOC_SCRATCH = 0x1000;     // scratch above it a tick may use
	const uint32_t CRT_TLS_FN = 0x560578;            // returns the CRT per-thread data: +0x14 = rand seed
	const uint32_t EFFECT_ID_PTR = 0x1D99A68;        // running effect id - 1
}
