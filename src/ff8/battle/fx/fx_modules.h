/****************************************************************************/
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

// Memory owned by each effect, for the verifier

#pragma once

#include <stdint.h>

namespace ff8fx::mod
{
	struct Region { uint32_t addr, size; const char *name; };

	struct Module
	{
		int effect_id;
		uint32_t data_lo, data_hi;
		uint32_t extra, extra_size;
		bool lookahead;
		const char *name;
		const Region *streams;
		int nstreams;
	};

	struct ExtSite { uint32_t addr; int arity; const char *name; };

	extern const Module modules[];
	extern const int module_count;
	extern const Region engine_regions[];
	extern const int engine_region_count;
	extern const ExtSite ext_sites[];
	extern const int ext_site_count;

	const uint32_t DATA_MAX = 0x40000;
	const uint32_t STREAMS_MAX = 0x80000;

	const Module *find_module(int effect_id);
	uint32_t streams_bytes(const Module &m);
	inline bool fits(const Module &m)
	{
		return m.data_hi >= m.data_lo && m.data_hi - m.data_lo <= DATA_MAX && streams_bytes(m) <= STREAMS_MAX
			&& (m.extra_size <= 16);
	}

	const uint32_t SSIGPU_ARENA = 0x1C48828;
	const uint32_t SSIGPU_ARENA_SIZE = 0x60000;
	const uint32_t RENDER_LIST_BASE_PTR = 0x1D8E04C;
	const uint32_t OT_SPAN_WORDS = 17 + 4386;
	const uint32_t FRAME_PACKET_CUR_PTR = 0x1D8E054;
	const uint32_t FRAME_PACKET_END_PTR = 0x1D969A8;
	const uint32_t FIELD_ALLOC_PTR = 0x1D999C4;
	const uint32_t FIELD_ALLOC_SCRATCH = 0x1000;
	const uint32_t CRT_TLS_FN = 0x560578;
	const uint32_t EFFECT_ID_PTR = 0x1D99A68;
}
