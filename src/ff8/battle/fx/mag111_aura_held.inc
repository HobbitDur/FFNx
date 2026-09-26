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

// 30 fps part of mag111_aura.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag111_aura.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Aura task draws THEN updates. A real tick records every draw in order with the GTE
// registers and the Field_Alloc scratch it started from (draw headers keep the scratch bytes where
// the code does not write). A held frame replays the log in the same order:
//   Aura  - the prim-model layout through prim::play_held: every object's exact in-between record
//           (position, rotation, scale, colour, fade, vertex-frame blend); the block matrix is
//           composed again with the current camera; blended vertex frames go to a private buffer.
//   Glow  - drawn exactly as on the real tick: fixed position, its sequence frame steps at 15 Hz.
//   Flame - angle around the target half way along its speed, fade half way to the next tick's;
//           its last draw (tick 20) holds.
// No Aura task has a draw-only mode.

namespace ff8fx
{
namespace aura111
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_PLAY = 1, REC_GLOW = 2, REC_FLAME = 3 };

	struct Rec
	{
		uint32_t kind;
		prim::Layout *layout;     // play: the live layout (prim::play_held knows what it drew)
		PrimArg arg;              // play: its block before the camera (+ flags, morph)
		int16_t pos[4];           // glow
		int32_t step;
		FlameNode flame;          // flame, as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[64];
	static uint8_t g_held_packets[0x20000]; // module arena (0x18000 per tick)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames (magic buffer + 0xE20)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint32_t kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_play(const TargetNode *t, const Mat4x3 *local)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = (prim::Layout *)t->aura;
		r->arg.m = *local;
		r->arg.flags = PART_Flags;
		r->arg.morph = TexBase() + 0xE20;
	}

	static void held_note_glow(const int16_t *pos, int32_t e)
	{
		Rec *r = NewRec(REC_GLOW);
		if (!r) return;
		memcpy(r->pos, pos, sizeof(r->pos));
		r->step = e;
	}

	static void held_note_flame(const FlameNode *f)
	{
		Rec *r = NewRec(REC_FLAME);
		if (!r) return;
		r->flame = *f;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void FlameHeld(const Rec &r, int num, int den)
	{
		const FlameNode &a = r.flame;
		FlameNode m = a;
		uint8_t *h = AllocHeader(0x58);
		FlameHeader(h, &a);
		if (a.counter < 0x14)
		{
			// the next tick's draw: angle + speed, counter + 1
			FlameNode b = a;
			b.angle = (int16_t)(b.angle + b.speed);
			b.counter++;
			m.angle = lerp16(a.angle, b.angle, num, den);
			*(int32_t *)(h + 0xC) = lerp_i(FlameFade(&a), FlameFade(&b), num, den);
		}
		FlameDraw(&m, h);
		FieldFree(0x58);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Replay(r);
			switch (r.kind)
			{
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
				memcpy(g_held_morph, (const void *)r.arg.morph, sizeof(g_held_morph));
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held(r.layout, AuraPart, (int)&arg, num, den);
				break;
			}
			case REC_GLOW: GlowDraw(r.pos, r.step); break;
			case REC_FLAME: FlameHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag111_held()
	{
		mark_held(aura111::ORIG_TargetTask);
		mark_held(aura111::ORIG_FlameTask);
		register_module_held(111, aura111::HeldReady, aura111::HeldFrame);
	}
}
