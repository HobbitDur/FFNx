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

// 30 fps part of mag044_junk.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag044_junk.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every drawing task draws THEN updates. A real tick records every draw in order (smoke sprite,
// item model, debris layout play) with the node as drawn, the GTE registers and the Field_Alloc
// scratch it started from (draw headers keep the scratch bytes where the code does not write, e.g.
// the prim-model depth word). A held frame replays the log in the same order into a private copy of
// the packet arena (its start taken at the tick's first draw: the root picks the arena by tick
// parity):
//   smoke   moved half way along its velocity (linear drift); the flipbook frame holds; the last
//           tick of a smoke (life 1) holds.
//   item    position and rotation half way to the node's next state (the node after its update:
//           fall, bounce, drift); the fade-out over the last 3 ticks half way; the model (animated
//           kind 5: one model per tick) holds; the last tick (life 1) holds.
//   debris  the layout's exact in-between records (prim::play_held) under the Y rotation half way
//           to the next tick's (angle + speed); blended vertex frames go to a private buffer.
// No Junk task has a draw-only mode; the module does not move the camera.

namespace ff8fx
{
namespace junk044
{
	static const uint32_t SCRATCH = 0x800;
	static const uint32_t ARENA = 0x10000;   // module packet arena per tick
	static const int MAX_RECS = 0x50;        // smoke 32 + items 16 + debris 32

	enum { K_SMOKE, K_ITEM, K_DEBRIS };

	struct Rec
	{
		int kind;
		const uint8_t *node;       // the live node
		uint8_t copy[0x58];        // the node as drawn
		PrimArg arg;               // debris: the parameter block of the play
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_morph[0x8000]; // blended vertex frames of held debris draws

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *held_note(int kind, const void *node, int size)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = (const uint8_t *)node;
		memcpy(r.copy, node, size);
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_smoke(const SmokeNode *s) { held_note(K_SMOKE, s, sizeof(SmokeNode)); }
	static void held_note_item(const ItemNode *it) { held_note(K_ITEM, it, sizeof(ItemNode)); }
	static void held_note_debris(const DebrisNode *d, const PrimArg *arg)
	{
		Rec *r = held_note(K_DEBRIS, d, sizeof(DebrisNode));
		if (r) r->arg = *arg;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// DrawItem with the fade value given (the in-between fade of the last ticks)
	static void HeldDrawItem(const ItemNode *it, int32_t fade)
	{
		const uint8_t *kind = it->kind;
		uint8_t *b = (uint8_t *)FieldAlloc(0x78);
		Mat4x3 *m = (Mat4x3 *)b;
		ComposeZYXRotationMatrix(it->rot, m);
		m->t[0] = it->x;
		m->t[1] = it->y;
		m->t[2] = it->z;
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		uint8_t *h = b + 0x20;
		if (it->life < 4)
		{
			*(uint32_t *)(h + 0x1C) = 0xC3;
			*(uint32_t *)(h + 8) = 0;
			*(int32_t *)(h + 0xC) = fade;
		}
		else *(uint32_t *)(h + 0x1C) = 0;
		const int32_t k = (int32_t)it->life % (int32_t)*(const int16_t *)(kind + 0x10);
		*(uint32_t *)h = *(const uint32_t *)(kind + k * 4);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x78);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == K_SMOKE)
			{
				SmokeNode s;
				memcpy(&s, r.copy, sizeof(s));
				if (s.life > 1)
					for (int k = 0; k < 3; k++) s.pos[k] = lerp16(s.pos[k], (int16_t)(s.pos[k] + s.vel[k]), num, den);
				DrawSmoke(&s);
			}
			else if (r.kind == K_ITEM)
			{
				ItemNode it;
				memcpy(&it, r.copy, sizeof(it));
				int32_t fade = (4 - (int32_t)it.life) << 10;
				if (it.life > 1)
				{
					// the node after its update (an item node is only reused on a later tick)
					const ItemNode *next = (const ItemNode *)r.node;
					it.x = lerp16(it.x, next->x, num, den);
					it.y = lerp16(it.y, next->y, num, den);
					it.z = lerp16(it.z, next->z, num, den);
					for (int k = 0; k < 3; k++) it.rot[k] = lerp_angle(it.rot[k], next->rot[k], num, den);
					if (it.life < 4) fade = lerp_i(fade, (5 - (int32_t)it.life) << 10, num, den);
				}
				HeldDrawItem(&it, fade);
			}
			else
			{
				DebrisNode d;
				memcpy(&d, r.copy, sizeof(d));
				PrimArg arg;
				DebrisMatrix(&d, lerp_angle(d.angle, (int16_t)(d.angle + d.speed), num, den), &arg);
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held((prim::Layout *)((const DebrisNode *)r.node)->layout, PartCallback, (int)&arg, num, den);
			}
		}
	}
}

	static void register_mag044_held()
	{
		mark_held(junk044::ORIG_SmokeTask);
		mark_held(junk044::ORIG_ItemTask);
		mark_held(junk044::ORIG_DebrisTask);
		register_module_held(44, junk044::HeldReady, junk044::HeldFrame);
	}
}
