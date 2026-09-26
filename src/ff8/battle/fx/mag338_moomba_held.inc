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

// 30 fps part of mag338_moomba.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag338_moomba.cpp)

namespace ff8fx
{
namespace m338
{
	static uint32_t g_ported_tick = 0xFFFFFFFF;          // real tick on which the ported master last ran

	// ------------------------------------------------------------------
	// Held-frame bookkeeping of the real tick
	// ------------------------------------------------------------------
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct PoseMemo { uint32_t tick; bool ok; uint32_t size; uint8_t pose[SKEL_MAX]; };

	struct CreatureMemo
	{
		uint32_t tick;
		CreatureNode *node;
		int16_t c;                  // counter the tick ran with
		bool ring; int32_t ring_x, ring_y, ring_z, ring_angle, ring_fade;
		bool model, restart;        // model drawn; its animation restarted before the draw
		int32_t root_y, root_z;     // root it was drawn with
		bool s0_ok; uint32_t s0_size; uint8_t s0[SKEL_MAX]; // skeleton at the start of the tick (drawn matrices)
		bool sparks; Spark pool[0x40];                      // the sparks as drawn
	};
	static CreatureMemo g_cm = { 0xFFFFFFFF };
	static PoseMemo g_built[2] = { { 0xFFFFFFFF }, { 0xFFFFFFFF } }; // pose the matrices were built from after the draw, per tick parity

	struct PrimRun { PrimNode *node; };
	static uint32_t g_prim_tick = 0xFFFFFFFF;
	static int g_prim_n = 0;
	static PrimRun g_prim_run[16];

	struct DebrisRun { DebrisNode *node; bool ended; int16_t pos[3]; uint8_t rot[2]; int16_t size; };
	static uint32_t g_debris_tick = 0xFFFFFFFF;
	static int g_debris_n = 0;
	static DebrisRun g_debris_run[0x40];
	static DebrisRun *g_debris_cur = nullptr; // memo of the debris task running now (nullptr: memo full)

	static uint8_t *Skeleton(CreatureNode *cn, uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(cn->model + 0x64); // BattleAnimHeader.comFileData
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= SKEL_MAX ? sk : nullptr;
	}

	// master: its ports ran on this real tick
	static void held_note_master()
	{
		g_ported_tick = g_real_tick;
	}

	// prim-model task: redrawn in between by PrimsHeld
	static void held_note_prim(PrimNode *p)
	{
		if (g_prim_tick != g_real_tick) { g_prim_tick = g_real_tick; g_prim_n = 0; }
		if (g_prim_n < 16) g_prim_run[g_prim_n++].node = p;
	}

	// debris chunk: the state it is drawn with (before its update)
	static void held_note_debris(DebrisNode *d)
	{
		if (g_debris_tick != g_real_tick) { g_debris_tick = g_real_tick; g_debris_n = 0; }
		DebrisRun *run = g_debris_n < 0x40 ? &g_debris_run[g_debris_n++] : nullptr;
		g_debris_cur = run;
		if (run)
		{
			run->node = d;
			run->ended = false;
			memcpy(run->pos, d->pos, sizeof(run->pos));
			run->rot[0] = d->rot[0];
			run->rot[1] = d->rot[1];
			run->size = d->size;
		}
	}

	static void held_note_debris_ended()
	{
		if (g_debris_cur) g_debris_cur->ended = true;
	}

	// creature: start of the tick (counter, skeleton the draw uses)
	static void held_note_creature(CreatureNode *cn)
	{
		CreatureMemo &M = g_cm;
		M.tick = g_real_tick;
		M.node = cn;
		M.c = cn->counter;
		M.ring = M.model = M.restart = M.sparks = false;
		{
			uint32_t size = 0;
			uint8_t *sk = Skeleton(cn, &size);
			M.s0_ok = sk != nullptr;
			if (sk) { memcpy(M.s0, sk, size); M.s0_size = size; }
		}
	}

	// ring drawn at the root with this angle / fade
	static void held_note_ring(const Mat4x3 &root, int32_t angle, int32_t fade)
	{
		CreatureMemo &M = g_cm;
		M.ring = true;
		M.ring_x = root.t[0]; M.ring_y = root.t[1]; M.ring_z = root.t[2];
		M.ring_angle = angle; M.ring_fade = fade;
	}

	// Moomba drawn (after the animation read)
	static void held_note_model(CreatureNode *cn)
	{
		CreatureMemo &M = g_cm;
		uint8_t *e = cn->model;
		M.restart = e[0x6C + 6] == 0; // completed: restarted, pose zeroed
		M.model = true;
		M.root_y = Root(cn).t[1];
		M.root_z = Root(cn).t[2];
	}

	// pose the draw built the matrices from
	static void held_note_built(CreatureNode *cn)
	{
		PoseMemo &b = g_built[g_real_tick & 1];
		uint32_t size = 0;
		uint8_t *sk = Skeleton(cn, &size);
		b.tick = g_real_tick;
		b.ok = sk != nullptr;
		if (sk) { memcpy(b.pose, sk, size); b.size = size; }
	}

	// the sparks as drawn (before they move)
	static void held_note_sparks()
	{
		CreatureMemo &M = g_cm;
		M.sparks = true;
		memcpy(M.pool, Sparks(ModelBuffer()), sizeof(M.pool));
	}

	// ring fade / angle of tick c (0..7 and 56..67), pure (held-frame prediction)
	static bool RingAt(int32_t c, int32_t &angle, int32_t &fade)
	{
		if ((uint32_t)c < 8)
		{
			uint32_t q7 = ((uint32_t)c << 10) / 7;
			int32_t co = x::Cos((int32_t)q7);
			fade = 0x1000 - x::Cos((0x1000 - co) >> 2);
			angle = (int32_t)((uint32_t)c << 8);
			return true;
		}
		uint32_t e = (uint32_t)(c - 0x38);
		if (e < 0xC)
		{
			uint32_t q = ((e << 12) / 12) >> 2;
			int32_t s = ComputeSin((int32_t)q) >> 2;
			fade = 0x1000 - ComputeSin(s);
			angle = (int32_t)(e << 8);
			return true;
		}
		return false;
	}

	// root y / z the tick with counter c sets before its draws (from y / z), pure
	static void RootAt(int32_t c, int32_t &y, int32_t &z)
	{
		uint32_t e8 = (uint32_t)(c - 8);
		if (e8 < 0x30 && e8 - 0x18 < 5)
		{
			int32_t s = ComputeSin(0x400 - (int32_t)(((e8 - 0x18) << 10) >> 2));
			int16_t out[4];
			x::Blend((const void *)0x2556240, (const void *)0x2556248, 0x1000 - s, s, out);
			y = out[1];
			z = out[2];
		}
		if ((uint32_t)c < 8)
		{
			int32_t a = 0x400 - (int32_t)(((uint32_t)c << 10) / 7);
			int32_t s = ComputeSin(a);
			y = (int32_t)W(5) - mul32(s, 2000) / 4096;
			z = mul32(a, 4000) / 1024 + W(6);
		}
		uint32_t e = (uint32_t)(c - 0x38);
		if (e < 4)
		{
			int32_t b = (int32_t)((e << 10) / 3);
			int32_t s = ComputeSin(b);
			y = (int32_t)W(1) - mul32(s, 2000) / 4096;
			z = mul32(b, 1000) / 1024 + W(2);
		}
		e = (uint32_t)(c - 0x3C);
		if (e < 8)
		{
			int32_t a = (int32_t)(((e + 1) << 10) >> 3);
			int32_t s = ComputeSin(a);
			y = (int32_t)W(1) - mul32(s, 4000) / 4096 - 0x7D0;
			z = mul32(a, 4000) / 1024 + W(2) + 0x3E8;
		}
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Exact vanilla shapes: everything that moves is drawn half way
	// between what the real tick drew and what the next tick draws; flipbook frames, spawns and
	// the screen flash keep their 15 Hz steps. Nothing here changes game state: no RNG, no
	// spawn, no sound; the model skeleton and root, the model-buffer scratch and the packet
	// cursors are put back.
	// ------------------------------------------------------------------
	static void PoseBlend(uint8_t *sk, const uint8_t *from, int num, int den)
	{
		// sk holds the pose the next tick draws, from the pose drawn this tick: write the
		// midpoint (angles the short way round) into sk's pose fields
		int nb = sk[0];
		bool scaled = (sk[1] & 1) != 0;
		for (int a = 0; a < 3; a++)
		{
			int16_t *p = (int16_t *)(sk + 8) + a;
			int16_t f = ((const int16_t *)(from + 8))[a];
			*p = (int16_t)(f + ((int32_t)*p - f) * num / den);
		}
		for (int b = 0; b < nb; b++)
		{
			int16_t *p = (int16_t *)(sk + 16 + 48 * b + 4);
			const int16_t *f = (const int16_t *)(from + 16 + 48 * b + 4);
			for (int a = 0; a < 3; a++)
			{
				int32_t d = (((int32_t)p[a] - f[a] + 2048) & 4095) - 2048;
				p[a] = (int16_t)(f[a] + d * num / den);
			}
			if (scaled)
				for (int a = 3; a < 6; a++) p[a] = (int16_t)(f[a] + ((int32_t)p[a] - f[a]) * num / den);
		}
	}

	inline int16_t Lerp16(int16_t a, int16_t b, int num, int den) { return (int16_t)(a + (int16_t)(b - a) * num / den); }

	static uint8_t g_skel_save[SKEL_MAX];
	static Spark g_held_pool[0x40];

	static void CreatureHeld(int num, int den)
	{
		const CreatureMemo &M = g_cm;
		if (M.tick != g_real_tick || !M.node) return;
		CreatureNode *cn = M.node; // (an ended node's memory is intact until the next tick)
		int32_t c0 = M.c, c1 = cn->counter;
		bool step = c1 == c0 + 1 && c1 < 0x4C;

		// next tick's root (the drops / slides / hops are computed before the draws)
		int32_t ny = Root(cn).t[1], nz = Root(cn).t[2];
		if (step) RootAt(c1, ny, nz);

		if (M.ring)
		{
			int32_t y = M.ring_y, z = M.ring_z, angle = M.ring_angle, fade = M.ring_fade;
			int32_t na, nf;
			if (step && RingAt(c1, na, nf))
			{
				y = lerp_i(y, ny, num, den);
				z = lerp_i(z, nz, num, den);
				angle = lerp_i(angle, na, num, den);
				fade = lerp_i(fade, nf, num, den);
			}
			DrawRing(M.ring_x, y, z, angle, fade, MODEL_Ring);
		}

		if (M.model)
		{
			uint32_t size = 0;
			uint8_t *sk = Skeleton(cn, &size);
			if (sk)
			{
				Mat4x3 root_save = Root(cn);
				memcpy(g_skel_save, sk, size);
				bool next = step && c1 < 0x44;
				const PoseMemo &drawn = g_built[(g_real_tick - 1) & 1], &built = g_built[g_real_tick & 1];
				bool blend = next && !M.restart && drawn.ok && drawn.tick == g_real_tick - 1 && drawn.size == size
					&& built.ok && built.tick == g_real_tick && built.size == size;
				Root(cn).t[1] = next ? lerp_i(M.root_y, ny, num, den) : M.root_y;
				Root(cn).t[2] = next ? lerp_i(M.root_z, nz, num, den) : M.root_z;
				if (blend)
				{
					memcpy(sk, built.pose, size);
					PoseBlend(sk, drawn.pose, num, den);
					BuildBoneMatricesFromPose(cn->model + 0x60);
				}
				else if (M.s0_ok && M.s0_size == size)
					memcpy(sk, M.s0, size); // the matrices drawn this tick, as they were
				PacketCursor() = x::DrawModel(cn->model, ModelBuffer() + MB_SCRATCH, PacketCursor(), PacketCursor2());
				memcpy(sk, g_skel_save, size);
				Root(cn) = root_save;
			}
		}

		if (M.sparks)
		{
			// as drawn (flipbook frame of the drawn life), positions towards the moved ones
			const Spark *cur = Sparks(ModelBuffer());
			memcpy(g_held_pool, M.pool, sizeof(g_held_pool));
			for (int i = 0; i < 0x40; i++)
			{
				Spark &s = g_held_pool[i];
				if (s.life == 0) continue;
				for (int k = 0; k < 3; k++) s.pos[k] = Lerp16(s.pos[k], cur[i].pos[k], num, den);
			}
			SparksDraw(g_held_pool);
		}
	}

	static void PrimsHeld(int num, int den)
	{
		if (g_prim_tick != g_real_tick) return;
		for (int i = 0; i < g_prim_n; i++)
		{
			PrimNode *p = g_prim_run[i].node;
			uint8_t ctx[0x28];
			PrimCtx(p, ctx);
			// in between the records drawn and the next ones; a finished player holds its last
			prim::play_held((prim::Layout *)p->layout, (prim::Callback)CB_PrimObject, (int)ctx, num, den);
		}
	}

	static void DebrisHeld(int num, int den)
	{
		if (g_debris_tick != g_real_tick) return;
		for (int i = 0; i < g_debris_n; i++)
		{
			const DebrisRun &r = g_debris_run[i];
			int16_t pos[3] = { r.pos[0], r.pos[1], r.pos[2] };
			int32_t rx = DebrisAngle(r.rot[0]), ry = DebrisAngle(r.rot[1]);
			if (!r.ended) // the last tick's chunk holds (vanilla shows it until the next tick)
			{
				const DebrisNode *d = r.node;
				for (int k = 0; k < 3; k++) pos[k] = Lerp16(r.pos[k], d->pos[k], num, den);
				rx += (int8_t)(d->rot[0] - r.rot[0]) * 16 * num / den;
				ry += (int8_t)(d->rot[1] - r.rot[1]) * 16 * num / den;
			}
			DebrisDraw(pos, rx, ry, r.size);
		}
	}

	static uint8_t g_held_packets[0x60000]; // cursor arena 0x40000, then the "next arena" argument
	static uint8_t g_scratch_save[MB_SCRATCH_SIZE];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// mirrors the master's queue order (creature: ring, Moomba, sparks; prims; debris); packets go
	// to a private buffer (both module cursors redirected and put back); the model-buffer scratch
	// the model draw and the prim morphs write is saved and put back
	static void HeldFrame(int num, int den)
	{
		uint8_t *MB = ModelBuffer();
		if (!MB) return;
		uint32_t cursor = PacketCursor(), cursor2 = PacketCursor2();
		memcpy(g_scratch_save, MB + MB_SCRATCH, sizeof(g_scratch_save));
		PacketCursor() = (uint32_t)g_held_packets;
		PacketCursor2() = (uint32_t)g_held_packets + 0x40000;

		CreatureHeld(num, den);
		PrimsHeld(num, den);
		DebrisHeld(num, den);

		memcpy(MB + MB_SCRATCH, g_scratch_save, sizeof(g_scratch_save));
		PacketCursor() = cursor;
		PacketCursor2() = cursor2;
	}
}

	static void register_mag338_held()
	{
		mark_held(m338::ORIG_CreatureTask);
		mark_held(m338::ORIG_PrimTask);
		mark_held(m338::ORIG_DebrisTask);
		register_module_held(338, m338::HeldReady, m338::HeldFrame);
	}
}
