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

// 30 fps part of mag288_rapture.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag288_rapture.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// The target, feather and sparkle tasks draw THEN advance. A real tick records every call of those tasks in execution order (the
// target queue, the feather queue, the sparkle queue) with the state it drew: the target task's counter and its target's world
// matrix (+0x40), facing (+0xE), +0x24, size (+0x26) and flags; a feather's / sparkle's node; plus the GTE registers and the
// Field_Alloc scratch the call started from (headers keep the scratch bytes the code does not write) and the module packet cursor
// of the tick's first draw. A held frame replays the calls in the same order, each part as its task draws it:
// - the two target layouts (prim::play_held: every object's exact in-between record; objects appearing / disappearing switch on
//   the real frame; mesh numbers never blended). Their frames follow the target's effect points 0xF1 / 0xF0 every tick: the
//   NEXT-POSE APPROACH - the target at the in-between pose (pose_midpoint: the engine reader one frame ahead, bones put back) and
//   its world matrix +0x40 half way from the one the real draw used to the one the frame holds (the engine's per-entity task
//   rebuilds it from the position before the effect: the next draw's; the lift writes the height +0x1E), facing / +0x24 / size
//   half way; then the effect points (GetEffectSpawnPosition, after the default effect position as the task calls it: the same
//   scratch use), the facing turn and the frame's camera exactly as the task builds the block. At 1 that is the next tick's block.
// - the feathers: angles + spin, y + fall speed, the scale (shrinking after 10 ticks) num/den of the way to the next tick's.
// - the sparkles: y num/den of the 0x1E rise, flipbook frame N below 1, the next tick's frame N + 1 at 1.
// A part the next tick does not draw (a layout at its last frame / out of its counter range / the target's flags off, a
// feather that ends, a sparkle's last frame 11, a target task's last draw) holds this tick's picture and is not drawn at 1;
// parts the next tick starts are not drawn (fraction 1 = a subsequence of the next tick). Every part is placed through the
// frame's camera. Not drawn by the module (held by the engine at 15 Hz, as the real ticks set them): the targets' height +0x1E and
// flags (their models are the engine's), the screen flash level / colour (Effect_SetScreenFlash). Blended vertex frames go to a
// private buffer; the packets go to a private copy of the module arena (magc::HeldScope); the target's world matrix, bones,
// +0xE / +0x24 / +0x26, the camera-state word 0x1D9770C (GetEffectSpawnPosition) and the GTE registers 28..31 are put back.
// No task of the module has a draw-only mode.

namespace ff8fx
{
namespace rapture288
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA = 0x18000;  // module packet arena per tick
	static const int MAX_RECS = 6 + 0x30 + 0x30;

	struct Rec
	{
		const void *node;         // the live node
		int kind;                 // 0 target, 1 feather, 2 sparkle
		uint8_t copy[0x24];       // the feather / sparkle node as drawn
		int16_t counter;          // target: the counter it drew with
		uint8_t world[0x20];      // target: its entity's world matrix at the draw
		int16_t facing, h24, size; uint16_t flags;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static uint8_t g_held_packets[ARENA];
	static uint8_t g_held_morph[0x1000]; // blended vertex frames (magic buffer + 0x2340)
	static uint8_t g_bones_save[0x40 + 0x30 * 256];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *held_note(const void *node, int kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.node = node;
		r.kind = kind;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}
	static void held_note_target(const TargetNode *t)
	{
		Rec *r = held_note(t, 0);
		if (!r) return;
		uint8_t *e = t->entity;
		r->counter = t->counter;
		memcpy(r->world, e + 0x40, sizeof(r->world));
		r->facing = S16(e, 0xE);
		r->h24 = S16(e, 0x24);
		r->size = S16(e, 0x26);
		r->flags = *(const uint16_t *)e;
	}
	static void held_note_part(const void *node, int kind)
	{
		Rec *r = held_note(node, kind);
		if (r) memcpy(r->copy, node, kind == 1 ? sizeof(FeatherNode) : sizeof(SparkleNode));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the layout plays again on the next tick (it has frames left)
	static bool LayoutGoesOn(const void *layout)
	{
		const prim::Layout *l = (const prim::Layout *)layout;
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	// skeleton of an anim header (BattleAnimHeader: +4 -> pointer -> bone count, matrices)
	static uint8_t *Skeleton(uint8_t *anim, uint32_t *bytes)
	{
		uint8_t **pp = *(uint8_t ***)(anim + 4);
		if (!pp || !*pp) return nullptr;
		uint8_t *sk = *pp;
		uint32_t n = 0x40 + 0x30 * (uint32_t)sk[0];
		if (n > sizeof(g_bones_save)) n = sizeof(g_bones_save);
		*bytes = n;
		return sk;
	}

	// the target at tick + pnum / den for the duration of a block computation (everything put back by Restore)
	struct TargetAt
	{
		uint8_t *e; uint8_t world[0x20]; int16_t facing, h24, size, cam70C; uint8_t *sk = nullptr; uint32_t b = 0;
		TargetAt(uint8_t *e_, const Rec &r, int pnum, int den) : e(e_)
		{
			memcpy(world, e + 0x40, sizeof(world));
			facing = S16(e, 0xE); h24 = S16(e, 0x24); size = S16(e, 0x26);
			cam70C = var<int16_t>(0x1D9770C);
			sk = Skeleton(e + 0x60, &b);
			if (sk) memcpy(g_bones_save, sk, b);
			// the world matrix: from the one drawn to the one the frame holds (the next draw's)
			Mat4x3 &m = *(Mat4x3 *)(e + 0x40);
			const Mat4x3 &a = *(const Mat4x3 *)r.world, &c = *(const Mat4x3 *)world;
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 3; j++) m.m[i][j] = lerp_angle(a.m[i][j], c.m[i][j], pnum, den);
				m.t[i] = lerp_i(a.t[i], c.t[i], pnum, den);
			}
			S16(e, 0xE) = lerp_angle(r.facing, facing, pnum, den);
			S16(e, 0x24) = (int16_t)lerp_i(r.h24, h24, pnum, den);
			S16(e, 0x26) = (int16_t)lerp_i(r.size, size, pnum, den);
			if (pnum) pose_midpoint(e + 0x60, e + 0x6C, pnum, den);
		}
		~TargetAt()
		{
			if (sk) memcpy(sk, g_bones_save, b);
			memcpy(e + 0x40, world, sizeof(world));
			S16(e, 0xE) = facing; S16(e, 0x24) = h24; S16(e, 0x26) = size;
			var<int16_t>(0x1D9770C) = cam70C;
		}
	};

	static void HeldTarget(const Rec &r, int num, int den)
	{
		const TargetNode *t = (const TargetNode *)r.node;
		uint8_t *e = t->entity;
		const int32_t c = r.counter;
		const bool ends = c >= TARGET_END;  // the task's last call (nothing drawn after it)
		const bool drew_a = (uint32_t)(c - 4) < 0x1A;
		const bool drew_b = (uint32_t)(c - 8) < 0x42 && !(r.flags & 4) && (r.flags & 2);
		const uint16_t fl = *(const uint16_t *)e; // the flags the next call reads
		const bool next_a = !ends && (uint32_t)(c + 1 - 4) < 0x1A && LayoutGoesOn(t->layout_a);
		const bool next_b = !ends && (uint32_t)(c + 1 - 8) < 0x42 && !(fl & 4) && (fl & 2) && LayoutGoesOn(t->layout_b);
		PrimArg arg;
		arg.scaled = 1;
		arg.morph = (uint32_t)g_held_morph;
		if (drew_a && (next_a || num < den))
		{
			const int f = next_a ? num : 0;
			TargetAt at(e, r, f, den);
			int16_t def[4];
			GetDefaultEffectPosition(e, def); // (as the task: its Field_Alloc use)
			arg.scale[0] = arg.scale[1] = arg.scale[2] = TargetScale(S16(e, 0x26));
			BlockA(e, S16(e, 0xE), &arg);
			arg.depth = 0;
			prim::play_held((prim::Layout *)t->layout_a, PrimCallback, (int)&arg, f, den);
		}
		if (drew_b && (next_b || num < den))
		{
			const int f = next_b ? num : 0;
			TargetAt at(e, r, f, den);
			if (!drew_a)
			{
				int16_t def[4];
				GetDefaultEffectPosition(e, def); // (as the task: its Field_Alloc use)
			}
			arg.scale[0] = arg.scale[1] = arg.scale[2] = TargetScale(S16(e, 0x26));
			BlockB(e, S16(e, 0xE), &arg);
			prim::play_held((prim::Layout *)t->layout_b, PrimCallback, (int)&arg, f, den);
		}
	}

	static void HeldFeather(const Rec &r, int num, int den)
	{
		FeatherNode f;
		memcpy(&f, r.copy, sizeof(f));
		// the next tick's feather (the task's update)
		int16_t ns = f.scale;
		if (f.counter + 1 >= 10) ns = (int16_t)(ns - 0x100);
		const bool goes_on = !(f.counter + 1 >= 10 && ns < 0x100);
		if (!goes_on && num == den) return;
		const int k = goes_on ? num : 0;
		for (int a = 0; a < 3; a++) f.ang[a] = (int16_t)(f.ang[a] + (int16_t)f.spin[a] * k / den);
		f.pos[1] = (int16_t)(f.pos[1] + f.vy * k / den);
		f.scale = (int16_t)lerp_i(f.scale, ns, k, den);
		FeatherDraw(&f);
	}

	static void HeldSparkle(const Rec &r, int num, int den)
	{
		SparkleNode s;
		memcpy(&s, r.copy, sizeof(s));
		const bool goes_on = s.frame + 1 < SPARKLE_FRAMES;
		if (!goes_on && num == den) return;
		if (goes_on)
		{
			s.pos[1] = (int16_t)(s.pos[1] + 0x1E * num / den);
			if (num == den) s.frame++;
		}
		SparkleDraw(&s);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// GTE data registers 28..31 (IRGB, ORGB, LZCS, LZCR) are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed task starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			if (r.kind == 0) HeldTarget(r, num, den);
			else if (r.kind == 1) HeldFeather(r, num, den);
			else HeldSparkle(r, num, den);
		}
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
	}
}

	static void register_mag288_held()
	{
		mark_held(rapture288::ORIG_TargetTask);
		mark_held(rapture288::ORIG_FeatherTask);
		mark_held(rapture288::ORIG_SparkleTask);
		register_module_held(288, rapture288::HeldReady, rapture288::HeldFrame);
	}
}
