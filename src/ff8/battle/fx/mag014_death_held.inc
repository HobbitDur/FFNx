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

// 30 fps part of mag014_death.cpp: the held-frame drawing (included at the end of mag014_death.cpp)
//
// Every drawing task of Death updates THEN draws in the same tick. The real tick notes each draw
// (in queue order: reaper, actor systems, sparks) with the GTE state, the module scratch stack and
// the Field_Alloc scratch it started from; the held frame replays them in the same order:
//   Reaper        - its pose at the midpoint of the animation step (pose_midpoint), drawn like the
//                   real tick (dark renderer 0x8B7290 while it fades in, else the engine's creature
//                   draw a_746C10); placement, fade and colours hold.
//   Actor systems - as Drain: each drawn actor from a copy whose view positions are half way to the
//                   next tick's (the next motion step on a second copy); ending / morphing actors
//                   hold. The morph (0x8BBA50) writes its destination vertex blocks in the loaded
//                   actor data, which every actor system of the cast shares: each system's replay
//                   puts back the blocks it drew with, the held frame then the current ones.
//   Sparks        - positions half way along the next tick's motion (velocity loses a quarter,
//                   position += velocity / 16), scale and fade half way to the next tick's table
//                   values; a spark group on its last tick holds.
// The packets go to a private buffer (module cursor 0x273AE84) that starts with the real tick's
// packets; the module globals (pools, nodes, render context, vertex buffer), the module scratch
// stack, the Field_Alloc scratch, the shadow temporaries, the RenderGeometry temporaries and the GTE
// registers are put back.

namespace ff8fx
{
namespace act
{
namespace death
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x273AE80, BSS_HI = 0x27428A0;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t ACTOR_BYTES = 0x2A0, SPARK_BYTES = 0x118;
	static const int MAX_ACTORS = 160;

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a
	// kind-5 model's step (0x8BBA50) rewrites them in the actor data, which every actor system of
	// the cast shares, so a later system's update overwrites what an earlier one drew with
	struct VertexBlocks
	{
		uint32_t n;
		uint32_t addr[16], len[16];
		uint8_t bytes[0x4000];
		void save(uint32_t sys)
		{
			n = 0;
			const uint32_t tab = U32(sys, 0x22C);
			if (!tab) return;
			uint32_t used = 0;
			for (int k = 0; k < 16; k++)
			{
				const uint32_t blk = U32(tab, k * 4);
				if (!blk) continue;
				const uint32_t bytes_n = U32(blk, 4) * 8; // vertex count +4, 8-byte vertices from +8
				if (bytes_n == 0 || used + bytes_n > sizeof(bytes)) continue;
				addr[n] = blk + 8;
				len[n] = bytes_n;
				memcpy(bytes + used, (const void *)(blk + 8), bytes_n);
				used += bytes_n;
				n++;
			}
		}
		void restore() const
		{
			uint32_t used = 0;
			for (uint32_t i = 0; i < n; i++)
			{
				memcpy((void *)addr[i], bytes + used, len[i]);
				used += len[i];
			}
		}
	};
	static const int MAX_SYSTEMS = 8;
	static VertexBlocks g_sys_verts[MAX_SYSTEMS]; // per noted actor system: the blocks it drew with
	static int g_nsys_verts = 0;

	struct Note
	{
		int kind;             // 0 reaper, 1 actor system, 2 spark group
		uint32_t addr;        // reaper node / actor system state / spark node
		uint8_t node[SPARK_BYTES];
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		int verts;            // actor system: its g_sys_verts entry (-1 none)
	};
	static Note g_notes[96];
	static int g_nnotes = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0;
	static uint8_t g_held_packets[0x60000];

	static Note *held_note(int kind, uint32_t addr)
	{
		if (g_note_tick != g_real_tick)
		{
			g_note_tick = g_real_tick;
			g_nnotes = 0;
			g_nsys_verts = 0;
			g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
		}
		if (g_nnotes >= 96) return nullptr;
		Note &n = g_notes[g_nnotes++];
		n.kind = kind;
		n.addr = addr;
		n.verts = -1;
		memcpy(n.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(n.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		memcpy(n.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(n.falloc));
		return &n;
	}

	static void held_note_reaper(uint32_t node) { held_note(0, node); }
	static void held_note_system(uint32_t sys)
	{
		Note *n = held_note(1, sys);
		if (n && g_nsys_verts < MAX_SYSTEMS)
		{
			n->verts = g_nsys_verts;
			g_sys_verts[g_nsys_verts++].save(sys);
		}
	}
	static void held_note_spark(uint32_t node)
	{
		Note *n = held_note(2, node);
		if (n) memcpy(n->node, (const void *)node, SPARK_BYTES);
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	// the draw starts from the real tick's GTE state and scratch
	static void Restore(const Note &n)
	{
		memcpy((void *)GTE_D, n.gte, GTE_D_N);
		memcpy((void *)GTE_C, n.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, n.scratch, sizeof(n.scratch));
		memcpy((void *)MEM<uint32_t>(0x1D999C4), n.falloc, sizeof(n.falloc));
	}

	// ---- reaper
	static void ReaperHeld(uint32_t node, int num, int den)
	{
		if (U16(node, 0x26) & 8)
		{
			// 0x8B7030 dark branch and 0x8B7290 with the midpoint pose
			x::ComposeZYXRotationMatrix(node + 0x3C, node + 0x70);
			S32(node, 0x84) = S16(node, 0x4C);
			S32(node, 0x88) = S16(node, 0x4E);
			S32(node, 0x8C) = S16(node, 0x50);
			x::scale3DMatrix(node + 0x70, node + 0x114);
			const uint32_t ctx = REAPER_CTX;
			const uint32_t blk = U32(ctx, 0x48);
			memcpy((void *)ctx, (const void *)(blk + 0x40), 32);
			const uint32_t anim = blk + 0x60;
			pose_midpoint((void *)anim, (void *)(anim + 0xC), num, den);
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
			MEM<uint32_t>(U32(ctx, 0x4C)) = x::sub_5088A0(blk, MEM<uint32_t>(0x1D8E04C) + 0x4040, 0x10, MEM<uint32_t>(U32(ctx, 0x4C)));
			d_8B7390(U32(blk, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
			const uint32_t second = U32(blk, 0x78);
			if (second != 0)
				d_8B7390(U32(second, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
			x::BattleModel_BuildBoneMatricesFromPose(anim);
			return;
		}
		pose_midpoint((void *)(node + 0x90), (void *)(node + 0x9C), num, den);
		MEM<uint32_t>(PACKET_CURSOR) = a_746C10(node, 0x27419F0, MEM<uint32_t>(PACKET_CURSOR));
	}

	// ---- actor systems (as mag039_drain_held.inc)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	static void Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0 || U8(def, 0x16) == 5) return;
		a_741050(nxt, def);
		// end test of 0x8BBB60 (its end spawns: not called)
		const int32_t kind = S8(def, 0x1E);
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			if (S16(nxt, 0x1C8) < 0) return;
		}
		if ((kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) return;
		a_740910(nxt, def);
		a_741120(nxt, def);
		for (int k = 0; k < 3; k++)
			S32(cur, 0xC0 + 4 * k) = lerp_i(S32(cur, 0xC0 + 4 * k), S32(nxt, 0xC0 + 4 * k), num, den);
		const int32_t copies = S8(cur, 0x1D8);
		for (int32_t i = 0; i < copies && i < 16; i++)
			for (int k = 0; k < 3; k++)
			{
				const int32_t o = 0x19C + 8 * i + 2 * k;
				S16(cur, o) = (int16_t)lerp_i(S16(cur, o), S16(nxt, o), num, den);
			}
	}

	static void SystemHeld(const Note &n, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxt[ACTOR_BYTES];
		MEM<uint32_t>(ACTOR_STATE) = n.addr;
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.addr, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		for (int i = 0; i < nl; i++)
			Predict(list[i], P(copies[i]), P(nxt), num, den);
		Restore(n);
		if (n.verts >= 0) g_sys_verts[n.verts].restore();
		for (int i = 0; i < nl; i++)
		{
			const uint32_t c = P(copies[i]);
			if (U8(DefOf(list[i]), 0x16) > 2) continue;
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				d_8BA960(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					d_8BA960(c);
				}
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if (U8(def, 0x16) == 4 || U8(def, 0x16) == 5)
				d_8B8520(P(copies[i]), def);
		}
	}

	// ---- sparks
	static void SparkHeld(const Note &n, int num, int den)
	{
		alignas(4) static uint8_t a[SPARK_BYTES], b[SPARK_BYTES];
		memcpy(a, n.node, SPARK_BYTES);
		const uint32_t node = P(a);
		const int32_t e = S16(node, 0x24) * 2;
		int32_t sx = MEM<int16_t>(0x1630928 + e), sy = MEM<int16_t>(0x1630948 + e), sz = MEM<int16_t>(0x1630968 + e);
		int16_t fade = MEM<int16_t>(0x1630988 + e);
		if (!(U8(node, 0x26) & 1))
		{
			memcpy(b, a, SPARK_BYTES);
			SparkMove(P(b));
			for (int k = 0; k < 4; k++)
				for (int j = 0; j < 3; j++)
				{
					const int32_t o = 0xD4 + 8 * k + 2 * j;
					S16(node, o) = (int16_t)lerp_i(S16(node, o), S16(P(b), o), num, den);
				}
			sx = lerp_i(sx, MEM<int16_t>(0x1630928 + e + 2), num, den);
			sy = lerp_i(sy, MEM<int16_t>(0x1630948 + e + 2), num, den);
			sz = lerp_i(sz, MEM<int16_t>(0x1630968 + e + 2), num, den);
			fade = (int16_t)lerp_i(fade, MEM<int16_t>(0x1630988 + e + 2), num, den);
		}
		S32(node, 0x30) = sx;
		S32(node, 0x34) = sy;
		S32(node, 0x38) = sz;
		S16(node, 0x50) = fade;
		Restore(n);
		uint32_t pos = node + 0xD4;
		uint32_t mat = node + 0x54;
		for (int k = 4; k != 0; k--, pos += 8, mat += 0x20)
		{
			U32(node, 0x1C) = U32(pos, 0);
			U32(node, 0x20) = U32(pos, 4);
			d_8BDD20(node, mat);
		}
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_014;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x400], shadow[0x80];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		static VertexBlocks verts_now[MAX_SYSTEMS]; // the blocks as the real tick left them
		for (int i = 0; i < g_nnotes; i++)
			if (g_notes[i].verts >= 0) verts_now[g_notes[i].verts].save(g_notes[i].addr);
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		const uint32_t used = MEM<uint32_t>(PACKET_CURSOR) - g_pkt_start;
		if (g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		for (int i = 0; i < g_nnotes; i++)
		{
			const Note &n = g_notes[i];
			if (n.kind == 0)
			{
				Restore(n);
				ReaperHeld(n.addr, num, den);
			}
			else if (n.kind == 1)
				SystemHeld(n, num, den);
			else
				SparkHeld(n, num, den);
		}
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		for (int i = g_nnotes - 1; i >= 0; i--)
			if (g_notes[i].verts >= 0) verts_now[g_notes[i].verts].restore();
		memcpy((void *)fa, falloc, sizeof(falloc));
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag014_held()
	{
		mark_held(act::death::ORIG_Reaper);
		mark_held(act::death::ORIG_ActorSystem);
		mark_held(act::death::ORIG_Spark);
		register_module_held(14, act::death::HeldReady, act::death::HeldFrame);
	}
}
