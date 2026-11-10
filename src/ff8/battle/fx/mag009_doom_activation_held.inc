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

// 30 fps part of mag009_doom_activation.cpp: the held-frame drawing (included at the end of mag009_doom_activation.cpp)
//
// Every drawing task of Doom Activation updates THEN draws in the same tick. The real tick notes each draw in execution order
// (held_note_draw: the node as drawn, the GTE registers, the module scratch stack and the Field_Alloc scratch it started from);
// the held frame redraws them in the same order, each at tick + num/den, from copies moved half way to the next tick's state
// (the task's own update predicted on a copy; no spawn, no sound, no random number, no entity write):
//   Sinking target - the target's model drawn by 0x8C3520 / 0x8C3620 (render context 0x274EFF0): the height offset (+0x54) and
//                    the grey level (+0x56) half way to the next tick's (the sinking task's next state: 0x8C46F0 sinks by +0x6E,
//                    0x8C4720 sinks and lowers the level by 0x10), the target (and its weapon, a party member's second model
//                    +0x78) at its in-between pose (pose_midpoint). When the next tick no longer draws it (the level reaches
//                    0) it holds below 1 and is not drawn at 1.
//   Particles      - the sprites 0x8C3E00 (drawn by 0x8C3F00): position half way to the next step (the sequence step 0x8C4620,
//                    the turn +0x42 += 0x100 and the place around the sinking anchor at its next height, on a copy); the
//                    sequence frame switches on the real frame; the sequence's end hides it at 1.
//   Prim plays     - the doom model 0x8C0BC0 (fixed at the target) and the sinking model 0x8C3C30 (following the anchor):
//                    prim::play_held (objects half way to their next keyframe records) with the placement half way to the next
//                    tick's (the anchor at its next height); released on the next tick: held below 1, not drawn at 1.
//   Screen tints   - the engine task 0x8DDC30 of the last queue (as act_engine_held.inc's held_draw_tints): the colour half way
//                    to the next step of its colour script; a tint the next tick does not draw holds, and is not drawn at 1.
// The sinking anchor's next height: the sinking task's next state (2..4 move it by +0x6E; it runs before the particles and the
// prim models in the master's queue order).
// The stage darkening (the stage group words 0x1D98992 + k * 0x2C, read by the stage renderer, not drawn here) holds, as in
// Stare 45. Spawns, sounds, random numbers and the target's entity word stay on the real ticks.
// Camera: the module writes no camera word (the camera is the engine's camera animation of the setup): every part is placed
// through the frame's camera (0x1D97778, the frame gate's: the next tick's view at 1).
// The packets go to private copies of the three packet arenas (module cursors 0x274EFEC / 0x274F14C, frame arena 0x1D8E054)
// that start with the real tick's packets; the module globals, the module scratch stack + camera copy, the Field_Alloc scratch,
// the shadow / RenderGeometry temporaries, the CRT rand seed and the GTE registers are put back; the target's bone matrices are
// rebuilt from its real pose.

namespace ff8fx
{
namespace act
{
namespace doomact
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x274EFE8, BSS_HI = 0x27584F8;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t CUR_A = PACKET_CURSOR, CUR_B = 0x274F14C, CUR_FA = FRAME_CURSOR; // the packet cursors the draws advance
	static const uint32_t NODE_MAX = 0x3E4; // the largest node a draw reads (the prim-model tasks)
	static const uint32_t SINK_NODE = 0x7C, SPARK_NODE = 0x5C, CTX_BYTES = 0x5C;

	// one draw of the real tick (execution order)
	struct Rec
	{
		int kind;
		uint32_t node;              // the task node
		uint8_t snap[NODE_MAX];     // the node as drawn
		uint8_t ctx[CTX_BYTES];     // HK_BODY: the render context as drawn
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
	};
	static const int MAX_RECS = 64;
	static Rec g_rec[MAX_RECS];
	static int g_nrec = 0;
	static uint32_t g_tick = 0xFFFFFFFF, g_end_tick = 0xFFFFFFFF;
	static uint32_t g_start[3], g_end[3];                 // the cursors at the tick's first draw / after the master's queues
	static const uint32_t CURSORS[3] = { CUR_A, CUR_B, CUR_FA };
	static uint8_t g_priv[3][0x40000];
	static bool g_in_held = false;                         // a held frame runs (its draws note nothing)
	static bool g_full = false;                            // this real tick's draws did not fit the notes

	static uint32_t node_size(int kind)
	{
		if (kind == HK_BODY) return SINK_NODE;
		if (kind == HK_SPARK) return SPARK_NODE;
		return NODE_MAX;
	}

	static void begin_tick()
	{
		if (g_tick == g_real_tick) return;
		g_tick = g_real_tick;
		g_nrec = 0;
		g_full = false;
		for (int i = 0; i < 3; i++) g_start[i] = g_end[i] = MEM<uint32_t>(CURSORS[i]);
	}

	static void held_note_draw(int kind, uint32_t node)
	{
		if (g_in_held) return;
		begin_tick();
		if (g_nrec >= MAX_RECS) { g_full = true; return; }
		Rec &r = g_rec[g_nrec++];
		r.kind = kind;
		r.node = node;
		memcpy(r.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(r.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(r.scratch, (const void *)SCRATCH_LO, sizeof(r.scratch));
		memcpy(r.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(r.falloc));
		memcpy(r.snap, (const void *)node, node_size(kind));
		if (kind == HK_BODY) memcpy(r.ctx, (const void *)BODY_CTX, CTX_BYTES);
	}

	static void held_note_end()
	{
		if (g_in_held) return;
		begin_tick();
		for (int i = 0; i < 3; i++) g_end[i] = MEM<uint32_t>(CURSORS[i]);
		g_end_tick = g_real_tick;
	}

	// the task ran on this real tick after the note (its frame counter stepped) and runs on the next one
	static bool still_runs(uint32_t snap, uint32_t now)
	{
		return U32(now, 8) == U32(snap, 8) && S16(now, 0x24) == (int16_t)(S16(snap, 0x24) + 1) && !((U8(now, 0x26) & 1) && U8(now, 0x28) == 0);
	}

	// the sinking task's next state (as the real tick left it): its step of the anchor's height and of the model's offset /
	// level; false when it no longer runs
	struct SinkNext { bool runs; int8_t state; int16_t speed; };
	static SinkNext sink_next()
	{
		SinkNext s = { false, 0, 0 };
		for (uint32_t node = MEM<uint32_t>(Q_PHASE); node; node = U32(node, 4))
		{
			if (U32(node, 8) != ORIG_Sink) continue;
			if ((U8(node, 0x26) & 1) && U8(node, 0x28) == 0) break; // released by its task tail
			s.runs = true;
			s.state = S8(node, 0x29);
			s.speed = S16(node, 0x6E);
			break;
		}
		return s;
	}
	// the anchor's y step of the next tick (0x8C46F0 / 0x8C4720 / 0x8C47A0)
	static int16_t anchor_dy_next()
	{
		const SinkNext s = sink_next();
		return s.runs && s.state >= 2 && s.state <= 4 ? s.speed : (int16_t)0;
	}

	static void lerp16(uint32_t mid, uint32_t a, uint32_t b, uint32_t o, int num, int den) { S16(mid, o) = (int16_t)lerp_i(S16(a, o), S16(b, o), num, den); }

	static uint8_t g_nx[NODE_MAX], g_mid[NODE_MAX];

	// ---------------------------------------------------------------------------------------------------------------
	// the sinking target (0x8C3520 without its first bone rebuild: the bone matrices are the in-between pose's)
	// ---------------------------------------------------------------------------------------------------------------
	static void body_draw(uint32_t ctx)
	{
		const uint32_t ent = U32(ctx, 0x48);
		const int32_t dy = S16(ctx, 0x54);
		memcpy((void *)ctx, (const void *)(ent + 0x40), 0x20);
		S32(ctx, 0x18) = add32(S32(ctx, 0x18), dy);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(ctx);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x20);
		x::GTE_LoadIRFromMatrixColumn(ctx + 2);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x22);
		x::GTE_LoadIRFromMatrixColumn(ctx + 4);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x24);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(ctx + 0x14);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(ctx + 0x34);
		const uint32_t cur = U32(ctx, 0x4C);
		U32(cur, 0) = x::sub_5088A0(ent, MEM<uint32_t>(0x1D8E04C) + 0x4040, 0x10, U32(cur, 0));
		da_8C3620(U32(ent, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		const uint32_t part2 = U32(ent, 0x78);
		if (part2 != 0)
			da_8C3620(U32(part2, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
	}

	static void BodyHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), live = r.node;
		const uint32_t ent = U32(P(r.ctx), 0x48);
		const int16_t y0 = S16(P(r.ctx), 0x54), c0 = S16(P(r.ctx), 0x56);
		int16_t y1 = y0, c1 = c0;
		bool draws = still_runs(cur, live);
		if (draws)
		{
			const int8_t st = S8(live, 0x29);
			const int16_t v = S16(live, 0x6E);
			if (st == 2) y1 = (int16_t)(y0 + v);
			else if (st == 3)
			{
				y1 = (int16_t)(y0 + v);
				c1 = (int16_t)(c0 - 0x10);
				draws = c1 > 0; // at 0 the model is no longer drawn
			}
			else draws = false;
		}
		if (num >= den && !draws) return;
		memcpy((void *)BODY_CTX, r.ctx, CTX_BYTES);
		const uint32_t anim = ent + 0x60;
		const uint32_t second = U32(ent, 0x78);
		if (draws)
		{
			S16(BODY_CTX, 0x54) = (int16_t)lerp_i(y0, y1, num, den);
			S16(BODY_CTX, 0x56) = (int16_t)lerp_i(c0, c1, num, den);
			// the weapon (a party member's second model, entity +0x78) animates with the body: its in-between pose too
			pose_midpoint((void *)anim, (void *)(ent + 0x6C), num, den);
			if (second != 0) pose_midpoint((void *)second, (void *)(second + 0xC), num, den);
		}
		else
			x::BattleModel_BuildBoneMatricesFromPose(anim); // this tick's picture (the next tick draws nothing)
		body_draw(BODY_CTX);
		x::BattleModel_BuildBoneMatricesFromPose(anim);
		if (draws && second != 0) x::BattleModel_BuildBoneMatricesFromPose(second);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// the particles (0x8C3E00: state, turn, place, draw 0x8C3F00)
	// ---------------------------------------------------------------------------------------------------------------
	static void spark_next(uint32_t n, int16_t dy)
	{
		if (S8(n, 0x29) == 1) da_8C4620(n);
		U16(n, 0x42) = (uint16_t)(U16(n, 0x42) + 0x100);
		const uint16_t ay = MEM<uint16_t>(ANCHOR + 2);
		MEM<uint16_t>(ANCHOR + 2) = (uint16_t)(ay + dy);
		da_particle_place(n);
		MEM<uint16_t>(ANCHOR + 2) = ay;
	}

	static void SparkHeld(const Rec &r, int num, int den, int16_t dy)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, SPARK_NODE);
		memcpy(g_mid, r.snap, SPARK_NODE);
		bool draws = still_runs(cur, nx);
		if (draws) { spark_next(nx, dy); draws = (U8(nx, 0x26) & 4) == 0; }
		if (num >= den) { if (draws) da_8C3F00(nx); return; }
		if (draws) for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
		da_8C3F00(mid);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// the prim plays (0x8C0C90: the parameter block from the node's placement)
	// ---------------------------------------------------------------------------------------------------------------
	static void PlayHeld(uint32_t live, uint32_t place, int num, int den)
	{
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		x::MAG_022_sub_8DD770(L);
		int16_t ang = S16(place, 0x62);
		if (ang != 0) x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)ang);
		ang = S16(place, 0x60);
		if (ang != 0) x::sub_8DD7E0(L, (uint32_t)(int32_t)ang);
		ang = S16(place, 0x64);
		if (ang != 0) x::sub_8DD960(L, (uint32_t)(int32_t)ang);
		x::scale3DMatrix(L, place + 0x50);
		S32(L, 0x14) = S16(place, 0x1C);
		S32(L, 0x18) = S16(place, 0x1E);
		S32(L, 0x1C) = S16(place, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(place, 0x86);
		U32(L, 0x44) = U32(place, 0x7C);
		U32(L, 0x38) = U32(place, 0x70);
		U16(L, 0x4C) = U16(place, 0x8C);
		U16(L, 0x52) = U16(place, 0x92);
		U16(L, 0x4E) = U16(place, 0x8E);
		U16(L, 0x50) = U16(place, 0x90);
		U32(L, 0x48) = BLEND_BUF;
		U16(L, 0x56) = U16(place, 0x80);
		U16(L, 0x58) = U16(place, 0x88);
		prim::play_held((prim::Layout *)(live + 0x94), (prim::Callback)(void *)da_8C0DC0, (int)L, num, den);
	}

	static void PrimHeld(const Rec &r, int num, int den, int16_t dy)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		memcpy(g_nx, (const void *)r.node, NODE_MAX);
		memcpy(g_mid, r.snap, NODE_MAX);
		const bool draws = still_runs(cur, nx) && S8(nx, 0x29) == 1;
		if (num >= den && !draws) return;
		if (draws && U32(nx, 8) == ORIG_SinkModel)
		{
			// 0x8C3CF0: the next tick's placement = the anchor at its next height
			U32(nx, 0x1C) = MEM<uint32_t>(ANCHOR);
			U32(nx, 0x20) = MEM<uint32_t>(ANCHOR + 4);
			U16(nx, 0x1E) = (uint16_t)(U16(nx, 0x1E) + dy);
			for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
		}
		PlayHeld(r.node, mid, draws ? num : 0, den);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// screen tints (act_engine_held.inc's held_draw_tints, the queue 0x2757440): state 1 (0x8DDCD0) steps the colour script
	// (+0x30 = the colour it draws) then draws four semi-transparent flat quads over the screen (OT base + 0x1C). The held
	// frame draws them with the colour half way to the next script step's (the step run on a copy); a tint the next tick does
	// not draw (script end) holds, and is not drawn at fraction 1. It drew on the real tick when its state 1 ran then (the
	// state noted at the previous held frame; without one: a colour index past the first entry).
	// ---------------------------------------------------------------------------------------------------------------
	struct TintNote { uint32_t node, tick; int8_t state, prev_state; bool prev; };
	static TintNote g_tints[16];
	static TintNote *tint_note(uint32_t node)
	{
		TintNote *free_slot = nullptr;
		for (TintNote &w : g_tints)
		{
			if (w.node == node && w.tick + 2 >= g_real_tick) return &w;
			if (!free_slot && (w.node == 0 || w.tick + 2 < g_real_tick)) free_slot = &w;
		}
		if (free_slot) { memset(free_slot, 0, sizeof(*free_slot)); free_slot->node = node; free_slot->tick = 0xFFFFFFF0u; }
		return free_slot;
	}
	// the colour step of 0x8DDCD0 on a copy (no draw); false = the script ended (nothing drawn)
	static bool tint_step(uint32_t a)
	{
		const uint16_t idx = U16(a, 0x3A);
		const uint32_t script = U32(a, 0x34);
		const uint32_t ent = U32(script, (int32_t)(int16_t)idx * 4);
		const uint8_t op = (uint8_t)(ent >> 24), b2 = (uint8_t)(ent >> 16);
		if (op == 0xFF) return false;
		if (op == 0xFE)
		{
			if (S16(a, 0x24) >= (int16_t)(uint16_t)b2 && MEM<uint32_t>(0x2795914) == 0)
			{
				const uint16_t ni = (uint16_t)(idx + 1);
				U16(a, 0x3A) = ni;
				U32(a, 0x30) = U32(script, (int32_t)(int16_t)ni * 4);
			}
		}
		else
		{
			U32(a, 0x30) = ent;
			if (MEM<uint32_t>(0x2795914) == 0) U16(a, 0x3A) = (uint16_t)(idx + 1);
		}
		return true;
	}
	static void tint_draw(uint32_t colour)
	{
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x1C;
		static const uint32_t xy[4][4] = {
			{ 0, 0xA0, 0x780000, 0x7800A0 }, { 0xA0, 0x140, 0x7800A0, 0x780140 },
			{ 0x780000, 0x7800A0, 0xF00000, 0xF000A0 }, { 0x7800A0, 0x780140, 0xF000A0, 0xF00140 } };
		uint32_t pkt = MEM<uint32_t>(0x1D8E054);
		for (int q = 0; q < 4; q++)
		{
			U32(pkt, 4) = colour;
			U32(pkt, 0) = 0x5000000;
			U8(pkt, 7) = 0x2A;
			for (int v = 0; v < 4; v++) U32(pkt, 8 + v * 4) = xy[q][v];
			x::SSIGPU_InsertPrimAltViewport(ot, pkt);
			pkt += 0x18;
		}
		const uint32_t tpage = x::sub_45C690(0, 1, 0x280, 0) & 0xFFFF;
		x::sub_45BFC0(pkt, 0, 0, tpage, 0);
		x::SSIGPU_InsertPrimAltViewport(ot, pkt);
		MEM<uint32_t>(0x1D8E054) = pkt + 0xC;
	}
	static void TintsHeld(int num, int den)
	{
		for (uint32_t node = MEM<uint32_t>(Q_TINT); node; node = U32(node, 4))
		{
			if (U32(node, 8) != 0x8DDC30 || U16(node, 0x24) == 0) continue;
			TintNote *w = tint_note(node);
			if (!w) continue;
			if (w->tick != g_real_tick)
			{
				w->prev = w->tick + 1 == g_real_tick;
				w->prev_state = w->state;
				w->tick = g_real_tick;
				w->state = S8(node, 0x29);
			}
			const bool finished = (U8(node, 0x26) & 1) != 0;
			const bool drew = !finished && S8(node, 0x29) == 1 && (w->prev ? w->prev_state == 1 : U16(node, 0x3A) > 0);
			alignas(4) static uint8_t b[0x40];
			memcpy(b, (const void *)node, sizeof(b));
			const bool drew1 = !finished && S8(node, 0x29) == 1 && tint_step(P(b));
			if (num >= den)
			{
				if (drew1) tint_draw(U32(P(b), 0x30));
				continue;
			}
			if (!drew) continue;
			uint32_t col = U32(node, 0x30);
			if (drew1)
			{
				const uint32_t next = U32(P(b), 0x30);
				uint32_t c = col & 0xFF000000;
				for (int k = 0; k < 3; k++)
					c |= (uint32_t)(lerp_i((int32_t)((col >> (8 * k)) & 0xFF), (int32_t)((next >> (8 * k)) & 0xFF), num, den) & 0xFF) << (8 * k);
				col = c;
			}
			tint_draw(col);
		}
	}

	static bool fits()
	{
		if (g_full) return false;
		for (int i = 0; i < 3; i++)
			if (g_end[i] >= g_start[i] && g_end[i] - g_start[i] > sizeof(g_priv[i]) / 4 * 3) return false;
		return true;
	}

	static bool HeldReady()
	{
		return held_ready() && g_end_tick == g_real_tick && g_tick == g_real_tick && fits();
	}

	static uint32_t *crt_seed() { return (uint32_t *)(fn<uint8_t *(__cdecl *)()>(0x560578)() + 0x14); }

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_009;
		g_in_held = true;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x1000], shadow[0x80], cam[0xB0];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		memcpy(cam, (const void *)0x1D97700, sizeof(cam));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		uint8_t sprite_cam[12]; // written by the sprite draws (0x571BC0)
		memcpy(sprite_cam, (const void *)0x21DFEE4, sizeof(sprite_cam));
		const uint32_t seed = *crt_seed();
		uint32_t cur[3];
		for (int i = 0; i < 3; i++)
		{
			cur[i] = MEM<uint32_t>(CURSORS[i]);
			const uint32_t used = g_end[i] - g_start[i];
			if (g_start[i] && g_end[i] > g_start[i] && used <= sizeof(g_priv[i])) memcpy(g_priv[i], (const void *)g_start[i], used);
			MEM<uint32_t>(CURSORS[i]) = P(g_priv[i]);
		}
		const int16_t dy = anchor_dy_next();
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_rec[i];
			memcpy((void *)GTE_D, r.gte, GTE_D_N);
			memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
			memcpy((void *)SCRATCH_LO, r.scratch, sizeof(r.scratch));
			memcpy((void *)MEM<uint32_t>(0x1D999C4), r.falloc, sizeof(r.falloc));
			switch (r.kind)
			{
			case HK_BODY: BodyHeld(r, num, den); break;
			case HK_SPARK: SparkHeld(r, num, den, dy); break;
			case HK_PLAY: PrimHeld(r, num, den, dy); break;
			default: break;
			}
		}
		TintsHeld(num, den);
		for (int i = 0; i < 3; i++) MEM<uint32_t>(CURSORS[i]) = cur[i];
		*crt_seed() = seed;
		memcpy((void *)0x21DFEE4, sprite_cam, sizeof(sprite_cam));
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		memcpy((void *)0x1D97700, cam, sizeof(cam));
		memcpy((void *)fa, falloc, sizeof(falloc));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_in_held = false;
		g_mod = saved_mod;
	}
}
}
	static void register_mag009_held()
	{
		static const uint32_t HELD[] = { act::doomact::ORIG_Sink, act::doomact::ORIG_Particle, act::doomact::ORIG_DoomModel, act::doomact::ORIG_SinkModel, 0x8DDC30, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(9, act::doomact::HeldReady, act::doomact::HeldFrame);
	}
}
