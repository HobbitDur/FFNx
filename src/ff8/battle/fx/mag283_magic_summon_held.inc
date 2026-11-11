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

// 30 fps part of mag283_magic_summon.cpp (Magic Summon, effect 283). Exact vanilla shapes: the logic stays at 15 Hz, a held
// frame draws the state num/den of the way from the last real tick to the next one.
//
// The main task draws THEN updates (a layout advances in its play). A real tick records its draws in order with the GTE state
// and the Field_Alloc scratch each one started from; a held frame replays them in the same order on private packets, with the
// current camera:
//   Layouts A / B - prim::play_held (every object's exact in-between record); the block as the real play read it (anchor,
//             scale, depth, the texture scroll of counter N); blended vertex frames go to a private buffer.
//   Warp    - the target at its in-between pose (pose_midpoint), the position projected with the current camera, its colour
//             faded by the in-between fade (sin of the fractional angle). The push (weight row and radius switch per tick) is
//             projected twice per bone group: with the real draw's step / radius and with the next tick's (step - 1, the radius
//             the real draw returned); the vertices go half way between the two (clip flags from the in-between position),
//             then the polygons are culled and drawn from them; its shadow is drawn again. Vertices go to a private copy of
//             the bone workspace, packets to a private copy of the frame arena; the real pose's bone matrices are put back.
// A part the next tick no longer draws (the main task's last counter 59, the last warp at counter 43) holds its picture
// (state N). Held without interpolation: the screen flash and the target's colour +0x2C (15 Hz steps, engine state). No
// task has a draw-only mode. The module packet arena alternates per tick: the held packets start where the tick's first draw
// started.

namespace ff8fx
{
namespace magicsummon283
{
	static const uint32_t SCRATCH = 0x400;

	struct Rec
	{
		int kind;                 // DrawKind
		const MainNode *node;
		int16_t counter;          // the main counter as drawn
		int16_t radius;           // warp: the radius it was drawn with
		int32_t e;                // warp: counter - 24
		uint32_t colour;          // warp: the target's colour +0x28 when drawn
		PrimArg arg;              // plays: the block as the play read it
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;          // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0;    // frame arena cursor at the warp
	static Rec g_recs[8];
	static uint8_t g_held_packets[0x10000]; // module arena (0x10000 per tick)
	static uint8_t g_held_frame[0x40000];   // shadow + warp packets
	static uint8_t g_held_verts[0x10000];   // warp vertices (indices are 12-bit: 0x8000)
	static uint8_t g_verts_next[0x10000];   // the next tick's push of a bone group's vertices
	static uint8_t g_held_morph[0x2000];    // blended vertex frames (magic buffer + 0x648)

	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_frame_packets = 0;
	}

	static Rec *NewRec(int kind, const MainNode *m)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = m;
		r.counter = m->counter;
		r.radius = m->radius;
		r.e = 0;
		r.colour = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_play(const MainNode *m, int kind, const PrimArg *arg)
	{
		Rec *r = NewRec(kind, m);
		if (!r) return;
		r->arg = *arg;
	}

	static void held_note_warp(const MainNode *m, int32_t e)
	{
		Rec *r = NewRec(WARP, m);
		if (!r) return;
		if (!g_frame_packets) g_frame_packets = FrameCursor();
		r->e = e;
		r->colour = *(const uint32_t *)(m->ent + 0x28);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the next tick draws the part again
	static bool NextDraws(const Rec &r)
	{
		if (r.kind == WARP) return r.e + 1 < 0x14;
		return r.counter + 1 < 0x3C;
	}

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// a held frame's other outputs: frame arena packets (a copy of the real tick's, so bytes the draw leaves alone are the
	// real ones), the warp's vertex buffer; both put back
	struct FrameScope
	{
		uint32_t cursor, bones;
		FrameScope()
		{
			bones = BoneWorkspace();
			cursor = FrameCursor();
			if (g_frame_packets && cursor > g_frame_packets)
			{
				uint32_t used = cursor - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = (uint32_t)g_held_frame;
			BoneWorkspace() = (uint32_t)g_held_verts;
		}
		~FrameScope()
		{
			BoneWorkspace() = bones;
			FrameCursor() = cursor;
		}
	};

	// ---- warp ----
	struct Push { int32_t step; int16_t radius; };

	static void SetPush(uint8_t *s, const Push &p)
	{
		F<uint32_t>(s, 0x4C) = WARP_Weights + (uint32_t)shl32(p.step, 8);
		F<int16_t>(s, 0x50) = p.radius;
	}

	// WarpModel with every vertex half way between the tick's push (a) and the next tick's (b, nullptr: none): screen
	// position and depth in between, clip flags from the in-between position; a vertex behind the camera in either pass keeps
	// the tick's
	static uint32_t WarpModelHeld(const uint8_t *model, uint8_t *s, uint32_t ot, uint32_t cursor, const Push &a, const Push *b, int num, int den)
	{
		const uint8_t *table = *(uint8_t *const *)(model + 4);
		const uint8_t *bones = *(uint8_t *const *)model + 0x10;
		const int32_t count = *(const int32_t *)table;
		const uint32_t *offs = (const uint32_t *)(table + 4);
		for (int32_t i = 0; i < count; i++)
		{
			uint8_t *obj = (uint8_t *)(*(uint8_t *const *)(model + 4) + offs[i]);
			if (!(F<uint32_t>(s, 0x18) & (1u << (i & 31)))) continue;
			uint8_t *out = F<uint8_t *>(s, 4);
			if (b)
			{
				uint8_t *obj_b = obj;
				SetPush(s, *b);
				WarpGroups(bones, &obj_b, g_verts_next, s);
			}
			SetPush(s, a);
			uint8_t *end = WarpGroups(bones, &obj, out, s);
			if (b && num)
			{
				uint32_t n = (uint32_t)(end - out) / 8;
				if (n > sizeof(g_verts_next) / 8) n = sizeof(g_verts_next) / 8;
				for (uint32_t k = 0; k < n; k++)
				{
					uint8_t *va = out + k * 8;
					const uint8_t *vb = g_verts_next + k * 8;
					if ((va[6] | vb[6]) & 0x10) continue;
					int16_t *pa = (int16_t *)va;
					const int16_t *pb = (const int16_t *)vb;
					pa[0] = (int16_t)lerp_i(pa[0], pb[0], num, den);
					pa[1] = (int16_t)lerp_i(pa[1], pb[1], num, den);
					*(uint16_t *)(va + 4) = (uint16_t)lerp_i(*(const uint16_t *)(va + 4), *(const uint16_t *)(vb + 4), num, den);
					uint8_t f = 0;
					if (pa[0] < shl32(F<int16_t>(s, 0xC), 3)) f |= 1;
					else if (pa[0] >= shl32(F<int16_t>(s, 0x10), 3)) f |= 2;
					if (pa[1] < shl32(F<int16_t>(s, 0xE), 3)) f |= 4;
					else if (pa[1] >= shl32(F<int16_t>(s, 0x12), 3)) f |= 8;
					va[6] = f;
				}
			}
			WarpCounts(obj, s);
			cursor = WarpPolygons(s, ot, 0, cursor);
		}
		return cursor;
	}

	// DrawWarped with WarpModelHeld
	static void DrawWarpedHeld(uint8_t *entity, int16_t sx, int16_t sy, int32_t fade, const Push &a, const Push *b, int num, int den)
	{
		uint8_t *c = WarpBegin(entity, sx, sy, a.radius, a.step, fade);
		uint8_t *s = c + 0x20;
		FrameCursor() = WarpModelHeld(*(const uint8_t *const *)(entity + 0x64), s, RenderOT(0x44), FrameCursor(), a, b, num, den);
		BuildBoneMatricesFromPose(entity + 0x60);
		uint8_t *weapon = *(uint8_t *const *)(entity + 0x78);
		if (weapon)
		{
			F<uint32_t>(c, 0x38) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(weapon, c);
			FrameCursor() = WarpModelHeld(*(const uint8_t *const *)(weapon + 4), s, RenderOT(0x44), FrameCursor(), a, b, num, den);
			BuildBoneMatricesFromPose(weapon);
		}
		FieldFree(0x7C);
	}

	static void WarpHeld(const Rec &r, int k, int den)
	{
		const MainNode *m = r.node;
		int16_t sxy[2];
		if (!WarpCentre(m->pos, sxy)) return;
		uint8_t *entity = m->ent;
		// the colour it was drawn in
		uint32_t &colour = *(uint32_t *)(entity + 0x28);
		const uint32_t now = colour;
		colour = r.colour;
		if (k) pose_midpoint(entity + 0x60, entity + 0x6C, k, den);
		// the fade at the fractional counter e + k / den: sin((20 - e) x 0x400 / 20)
		const int32_t fade = ComputeSin(shl32(0x14 * den - (r.e * den + k), 10) / (0x14 * den));
		const int32_t step = 0x14 - r.e;
		const Push a = { step, r.radius };
		// the next tick's warp: one step weaker, the radius the real draw returned
		const Push b = { step - 1, m->radius };
		DrawWarpedHeld(entity, sxy[0], sxy[1], fade, a, k ? &b : nullptr, k, den);
		colour = now;
	}

	static void HeldDraw(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			const int k = NextDraws(r) ? num : 0; // the last picture holds
			Replay(r);
			if (r.kind == WARP) WarpHeld(r, k, den);
			else
			{
				PrimArg arg = r.arg;
				memcpy(g_held_morph, (const void *)r.arg.morph, sizeof(g_held_morph));
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held((prim::Layout *)(r.kind == PLAY_A ? r.node->layoutA : r.node->layoutB), PrimCallback, (int)&arg, k, den);
			}
		}
	}
}

	static void register_mag283_held()
	{
		mark_held(magicsummon283::ORIG_MainTask);
		register_module_held(283, magicsummon283::HeldReady, magicsummon283::HeldDraw);
	}
}
