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

// 30 fps part of mag297_white_wind_quistis.cpp (Quistis' White Wind: White Wind 226's held part, play blocks without the size
// scale, a target layout whose task ends on the tick holds its picture): the draw log of the real tick and the held-frame drawing
// (included at the end of mag297_white_wind_quistis.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The scene and target tasks draw THEN update. A real tick records every layout play in order (the
// caster layout on counters 0..0x15, the sweep layout on counters 7..0x1C, then the target layouts)
// with its anchor, the GTE registers and the Field_Alloc scratch it started from
// (draw headers keep the scratch bytes where the code does not write). A held frame replays the log
// in the same order through prim::play_held: every object's exact in-between record (position,
// rotation, scale, colour, fade, vertex-frame blend). The anchors are fixed (taken when a layout
// starts); the play blocks and the module camera matrix (flag 0x1000 objects) are
// rebuilt from the frame's camera (the frame gate's) exactly as the tasks build them. A layout the
// next tick no longer plays (the caster layout after counter 0x15, the sweep layout on the scene's last
// counter 0x1C, a target layout whose task ends on the tick: its play returned 0xA) holds its picture. Blended vertex frames go to a private buffer. The module camera matrix and the
// GTE registers 28..31 are put back. No White Wind task has a draw-only mode.

namespace ff8fx
{
namespace whitewind297
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x10000;  // module packet arena per tick

	struct Rec
	{
		prim::Layout *layout;     // the live layout (prim::play_held knows what it drew)
		int kind;
		int16_t anchor[3];
		bool next_plays;          // the next tick plays the layout again (else its picture holds)
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[8];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x400];  // blended vertex frames (magic buffer + 0xAEC: 0x3E0 bytes)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_play(prim::Layout *layout, int kind, const int16_t *anchor)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.layout = layout;
		r.kind = kind;
		memcpy(r.anchor, anchor, sizeof(r.anchor));
		// the scene's counter as drawn (counted up after its draws): the caster layout plays on counters
		// 0..0x15, the sweep layout on 7..0x1C (the scene ends after 0x1C); a target layout plays until its task ends
		// (held_note_play_end: its play returned 0xA)
		r.next_plays = true;
		if (kind == PLAY_CASTER)
			r.next_plays = (uint32_t)((int32_t)((const SceneNode *)((const uint8_t *)layout - 0x18))->counter + 1) < 0x16;
		else if (kind == PLAY_SWEEP)
			r.next_plays = (uint32_t)((int32_t)((const SceneNode *)((const uint8_t *)layout - 0x1A8))->counter + 1 - 7) < 0x16;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	// real tick: the target task whose play was logged last ends on this tick (its play returned 0xA): the next tick no longer
	// plays its layout
	static void held_note_play_end()
	{
		if (g_log_tick == g_real_tick && g_nrec > 0) g_recs[g_nrec - 1].next_plays = false;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the play block of a logged play at the frame's camera (as the scene / target tasks build it)
	static void HeldBlock(const Rec &r, PrimArg &arg)
	{
		memset(&arg, 0, sizeof(arg));
		arg.m.t[0] = r.anchor[0];
		arg.m.t[1] = r.anchor[1];
		arg.m.t[2] = r.anchor[2];
		if (r.kind == PLAY_SWEEP)
		{
			arg.m.m[0][0] = 0x1000;
			arg.m.m[1][1] = 0x800;
			arg.m.m[2][2] = 0x800;
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		}
		else
			Billboard(&arg.m);
		arg.depth = -0x80;
		arg.morph = (uint32_t)g_held_morph;
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		const Mat4x3 mod = ModMatrix();
		// the root task's module camera matrix, at the frame's camera
		EffectCameraMatrix(&Camera(), &ModMatrix());
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			PrimArg arg;
			HeldBlock(r, arg);
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			prim::play_held(r.layout, PrimCallback, (int)&arg, r.next_plays ? num : 0, den);
		}
		ModMatrix() = mod;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag297_held()
	{
		mark_held(whitewind297::ORIG_SceneTask);
		mark_held(whitewind297::ORIG_TargetTask);
		register_module_held(297, whitewind297::HeldReady, whitewind297::HeldFrame);
	}
}
