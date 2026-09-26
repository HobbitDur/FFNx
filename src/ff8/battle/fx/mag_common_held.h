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

// 30 fps part of mag_common.h, shared by the magic (spell) modules (included by mag_common.h).
//
// Spell tasks draw THEN update. A real tick records every draw in order (DrawLog): the task's
// node as it was drawn, and the Field_Alloc scratch bytes its draw header was allocated on
// (the draw code leaves some header fields unset: the engine reads what the scratch held).
// A held frame replays the log in the same order: each draw on a copy of its node moved half
// way to the state the task's own update computes (linear motion), starting from the GTE state
// and with the scratch bytes of the real draw under its header, so everything the draw does not
// interpolate is exactly what the real tick drew (flipbook frames, tables, flags hold). At
// fraction 0 a held frame inserts exactly the primitives of the real tick (fxh heldzero check).

#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace magc
{
// internal linkage: every module file has its own copy (its own log, slot and replay state)
namespace
{
	// header scratch of the draw being recorded (real tick) or replayed (held frame)
	uint8_t *g_hdr_slot = nullptr;
	bool g_hdr_replay = false;

	void held_header(uint8_t *h, int size)
	{
		if (!g_hdr_slot) return;
		if (g_hdr_replay) memcpy(h, g_hdr_slot, size);
		else memcpy(g_hdr_slot, h, size);
		g_hdr_slot = nullptr;
	}

	const int NODE_BYTES = 0x24;  // spell task nodes (pools of 0x24-byte nodes)
	const int HEADER_BYTES = 0xB4; // largest draw header (sprite sequence)

	// GTE register files (data 0x1CA8A10, control 0x1CA9230 with the float mirrors): a replayed
	// draw starts from the GTE state its real draw started from (registers a draw does not set,
	// e.g. older FIFO entries, stay the real tick's)
	const uint32_t GTE_DATA = 0x1CA8A10, GTE_DATA_SIZE = 0x70, GTE_CTRL = 0x1CA9230, GTE_CTRL_SIZE = 0xD0;

	struct DrawRec
	{
		uint32_t fn;                // original task function
		uint8_t node[NODE_BYTES];   // the node as drawn
		uint8_t hdr[HEADER_BYTES];  // scratch under its draw header when it was allocated
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE]; // GTE state the draw started with
	};

	// a held draw starts like the real one: its GTE state and header scratch
	void BeginReplay(const DrawRec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		g_hdr_slot = (uint8_t *)r.hdr;
	}

	template<int N>
	struct DrawLog
	{
		uint32_t tick = 0xFFFFFFFF;
		int n = 0;
		uint32_t packets = 0; // the module's packet cursor at the tick's first draw
		DrawRec r[N];
		void begin() { tick = g_real_tick; n = 0; packets = 0; }
		bool current() const { return tick == g_real_tick; }
		// a task is about to draw: keep its node, its header scratch goes to the record
		void add(uint32_t fn, const void *node, uint32_t packet_cursor)
		{
			g_hdr_slot = nullptr;
			if (tick != g_real_tick || n >= N) return;
			if (n == 0) packets = packet_cursor;
			DrawRec &d = r[n++];
			d.fn = fn;
			memcpy(d.node, node, NODE_BYTES);
			memcpy(d.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
			memcpy(d.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
			g_hdr_slot = d.hdr;
		}
	};

	// int16 value half way (num/den) from a to b, b = a + delta as the game computes it (wraps)
	int16_t lerp16(int16_t a, int16_t b, int num, int den) { return (int16_t)(a + (int16_t)(b - a) * num / den); }

	// A held frame's surroundings: the module's packet cursor goes to a private buffer, the
	// Field_Alloc scratch, the TransformCameraByShadowRotation scratch (0x21DFED0..0x21DFEF0) and
	// the GTE registers the draws change are put back afterwards; the draw log is replayed into
	// the headers.
	struct HeldScope
	{
		uint32_t &cursor;
		uint32_t saved_cursor;
		uint8_t *scratch;
		uint8_t scratch_save[0x1000];
		uint8_t shadow_save[0x20];
		uint8_t gte_save[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		// buffer gets a copy of the packets the real tick wrote (from `packets` to the cursor): the
		// replayed draws write the same packets at the same offsets over them, so the bytes a draw
		// does not write (pad halves) are the real tick's too
		HeldScope(uint32_t &packet_cursor, uint8_t *buffer, uint32_t buffer_size, uint32_t packets) : cursor(packet_cursor)
		{
			saved_cursor = cursor;
			if (packets && saved_cursor > packets)
			{
				uint32_t used = saved_cursor - packets;
				memcpy(buffer, (const void *)packets, used < buffer_size ? used : buffer_size);
			}
			cursor = (uint32_t)buffer;
			scratch = var<uint8_t *>(0x1D999C4);
			if (scratch) memcpy(scratch_save, scratch, sizeof(scratch_save));
			memcpy(shadow_save, (void *)0x21DFED0, sizeof(shadow_save));
			memcpy(gte_save, (const void *)GTE_DATA, GTE_DATA_SIZE);
			memcpy(gte_save + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
			g_hdr_replay = true;
		}
		~HeldScope()
		{
			g_hdr_replay = false;
			g_hdr_slot = nullptr;
			memcpy((void *)0x21DFED0, shadow_save, sizeof(shadow_save));
			memcpy((void *)GTE_DATA, gte_save, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, gte_save + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			if (scratch) memcpy(scratch, scratch_save, sizeof(scratch_save));
			cursor = saved_cursor;
		}
	};

	// prim-model draw header: +0x0C = fade (GTE IR0 toward the +8 colour), +0x1C = flags. The
	// fade is a counter ramp: half way when the next tick's header uses the same flags.
	void LerpPrimFade(uint8_t *h, const uint8_t *next, int num, int den)
	{
		if (*(const uint32_t *)(h + 0x1C) != *(const uint32_t *)(next + 0x1C)) return;
		*(int32_t *)(h + 0xC) = lerp_i(*(const int32_t *)(h + 0xC), *(const int32_t *)(next + 0xC), num, den);
	}
}
}
}
