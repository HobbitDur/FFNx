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

// 30 fps part of fx_glint.cpp: the glints' draw log of the real tick and their held-frame drawing
// (included at the end of fx_glint.cpp; API in fx_glint_held.h).
//
// The glint task updates THEN draws. A real tick records every glint draw in order: the node as
// drawn, the GTE registers its draw started with and the packet cursor it drew into. A held
// frame (glint::held_draw, called by the module's held frame at the place of its glint queue)
// draws each again from the same GTE state at its in-between state: the next tick's growth / fade
// step (the task's own pure Step on a copy) half way - size, colours (per-tick byte steps) and the
// Y angle's spin; the position (the line anchor's midpoint and the target's bounds centre height,
// engine-driven) and the other angles hold. A glint whose next tick hides it (end of its fade)
// holds its last state. At fraction 0 the draws are exactly the real tick's.

namespace ff8fx
{
namespace glint
{
	struct Rec
	{
		uint8_t node[sizeof(GlintNode)];
		uint32_t *cursor;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
	};
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static Rec g_recs[256];

	void held_note_draw(const void *node, uint32_t *cursor)
	{
		if (g_log_tick != g_real_tick)
		{
			g_log_tick = g_real_tick;
			g_nrec = 0;
		}
		if (g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		Rec &r = g_recs[g_nrec++];
		memcpy(r.node, node, sizeof(GlintNode));
		r.cursor = cursor;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
	}

	// byte colour half way along its per-tick step (the step wraps as a byte)
	static inline uint8_t LerpByte(uint8_t a, uint8_t b, int num, int den) { return (uint8_t)(a + (int8_t)(uint8_t)(b - a) * num / den); }

	void held_draw(const void *root, int num, int den)
	{
		if (g_log_tick != g_real_tick) return;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			GlintNode a;
			memcpy(&a, r.node, sizeof(a));
			if (root && a.h.root != root) continue;
			GlintNode m = a;
			GlintNode b = a;
			Step(&b);
			if (!(b.h.flags & 4) && (a.h.phase == 1 || a.h.phase == 2))
			{
				m.w = lerp16(a.w, b.w, num, den);
				m.hgt = lerp16(a.hgt, b.hgt, num, den);
				for (int k = 0; k < 3; k++)
				{
					m.rgb_a[k] = LerpByte(a.rgb_a[k], b.rgb_a[k], num, den);
					m.rgb_b[k] = LerpByte(a.rgb_b[k], b.rgb_b[k], num, den);
				}
				m.rot[1] = (int16_t)((uint16_t)(a.rot[1] + a.spin * num / den) & 0xFFF);
			}
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			Draw(&m, RenderOT(0x44), r.cursor);
		}
	}
}

	void register_fx_glint_held()
	{
		mark_held(glint::ORIG_Glint);
	}
}
