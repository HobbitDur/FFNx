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

// 30 fps part of mag307_ray_bomb_quistis.cpp (Quistis' Ray-Bomb: Ray-Bomb 10's held part relocated, the muzzle in between, the
// 307 particle rules and end tests): the draw log of the real tick and the held-frame drawing
// (included at the end of mag307_ray_bomb_quistis.cpp; the shared machinery is in mag_common_held.h).
//
// Every Ray-Bomb task draws THEN updates. A real tick records every draw in order (the node as
// drawn, then the node after its update: no random number reaches a drawn field) and the dust
// particle array as the particle pass drew it. A held frame replays the log in the same order
// (GTE state and header scratch of the real draw) on the in-between node:
//   continuous fields half way (debris height and tumble, ring height, beam spin, spark height,
//   flash / charge size, beam length, dust positions); discrete fields (flipbook frames, counters) hold and
//   switch to the next tick's value at fraction 1;
//   fades half way between the two ticks' draw headers when both fade the same way (prim model
//   fade +0x0C, sprite colour +0x1C..+0x1E);
//   dust particles: position and size half way to the particle's next ones (the live array after the
//   tick); a particle freed on the tick holds; one at frame 0 (not drawn) appears at fraction 1.
// The beam faces the camera eye words 0xB8B7F0, which the camera animation writes for the NEXT view during the tick: a held
// frame extrapolates them num / den of a tick on from the last two real ticks' (exact for a camera moving at a constant rate).
// The muzzle follows the caster's animating hand (the director recomputes it from the caster's bone 0xF0 every tick):
// a held frame draws the flash, the charge glows and the beam from the muzzle of the caster's in-between pose
// (pose_midpoint, the director's own muzzle computation; the bone matrices and the muzzle are put back), when the director
// moved on the tick (not draw-only) and runs on the next one; else the muzzle holds.
// In the draw-only mode (battle_to_update_flags & 0x201) nothing moved: everything holds.
// Not drawn by the module: the bounce task's entity positions (engine-drawn models) and the
// screen flash level stay 15 Hz.

namespace ff8fx
{
namespace raybomb307
{
	static const int LOG_SIZE = 128;
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static DrawLog<LOG_SIZE> g_draws;
	struct After { const void *node; uint8_t b[NODE_BYTES]; };
	static After g_after[LOG_SIZE];          // per record: the node after its update
	static Particle g_dust[PARTICLES];       // the particle array as the pass drew it
	static uint8_t g_held_packets[0x10000];  // one packet arena is 0x8000 bytes

	// header of the next tick's draw of the node being replayed (fade half way)
	static bool g_fix = false, g_fix_prim = false;
	static int g_fix_num = 0, g_fix_den = 1;
	static uint8_t g_fix_next[0xB4];

	// the camera eye words 0xB8B7F0 of the last two real ticks (as the effect queue ran: noted by the root)
	static uint32_t g_eye_tick[2] = { 0xFFFFFFFF, 0xFFFFFFFF };
	static int16_t g_eye[2][3];
	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		if (g_eye_tick[0] != g_real_tick)
		{
			g_eye_tick[1] = g_eye_tick[0];
			memcpy(g_eye[1], g_eye[0], sizeof(g_eye[0]));
			g_eye_tick[0] = g_real_tick;
			memcpy(g_eye[0], (const void *)0xB8B7F0, sizeof(g_eye[0]));
		}
		g_draws.begin();
	}

	// the director ran on this tick and runs on the next tick too (counter < 0x87; a draw-only tick does not count: <= 0x87).
	// The caster animates on draw-only ticks too: the muzzle follows her in-between pose either way (as the Cure weapon pose)
	static uint32_t g_dir_tick = 0xFFFFFFFF;
	static bool g_dir_moving = false;
	static void held_note_director(const DirectorNode *d)
	{
		g_dir_tick = g_real_tick;
		g_dir_moving = d->counter < (DrawOnly() ? 0x88 : 0x87);
	}

	// the muzzle of the caster's in-between pose: its bone matrices at the pose num / den of the way to the next one
	// (pose_midpoint), the director's muzzle computation, the bone matrices put back
	static uint8_t g_bones_save[0x40 + 0x30 * 256];
	static void HeldMuzzle(int num, int den)
	{
		uint8_t *caster = Entity(CasterSlot());
		uint8_t *skel = **(uint8_t ***)(caster + 0x64);
		uint32_t bytes = 0x40 + 0x30 * (uint32_t)skel[0];
		if (bytes > sizeof(g_bones_save)) bytes = sizeof(g_bones_save);
		memcpy(g_bones_save, skel, bytes);
		pose_midpoint((void *)(caster + 0x60), (void *)(caster + 0x6C), num, den);
		ComputeMuzzle();
		memcpy(skel, g_bones_save, bytes);
	}

	static void held_note_draw(uint32_t fn, const void *node)
	{
		int n = g_draws.n;
		g_draws.add(fn, node, PacketCursor());
		if (g_draws.n == n) return;
		After &a = g_after[n];
		a.node = node;
		memcpy(a.b, node, NODE_BYTES);
	}

	static void held_note_after(const void *node)
	{
		if (!g_draws.current() || g_draws.n == 0) return;
		After &a = g_after[g_draws.n - 1];
		if (a.node == node) memcpy(a.b, node, NODE_BYTES);
	}

	static void held_note_particles()
	{
		memcpy(g_dust, Particles(), sizeof(g_dust));
		held_note_draw(ORIG_Particles, Particles());
	}

	static void held_fix_header(uint8_t *h)
	{
		if (!g_fix) return;
		const uint8_t *nx = g_fix_next;
		if (g_fix_prim)
		{
			// prim model: fade +0x0C when both headers fade with the same flags
			uint32_t f = *(const uint32_t *)(h + 0x1C);
			if (f == *(const uint32_t *)(nx + 0x1C) && (f & 0xC0) == 0xC0)
				*(int32_t *)(h + 0xC) = lerp_i(*(const int32_t *)(h + 0xC), *(const int32_t *)(nx + 0xC), g_fix_num, g_fix_den);
		}
		else
		{
			// sprite sequence: colour when both headers set it (mode 1 = colour not set)
			uint16_t mode = *(const uint16_t *)(h + 0x24);
			if (mode == *(const uint16_t *)(nx + 0x24) && mode != 1)
				for (int k = 0x1C; k <= 0x1E; k++) h[k] = (uint8_t)lerp_i(h[k], nx[k], g_fix_num, g_fix_den);
		}
	}

	// the tasks' end tests on the node after its update
	static bool Ended(uint32_t fn, const uint8_t *b)
	{
		switch (fn)
		{
		case ORIG_Debris: return ((const DebrisNode *)b)->counter >= ((const DebrisNode *)b)->life;
		case ORIG_Ring: return ((const RingNode *)b)->counter >= 0x18;
		case ORIG_Puff: return ((const PuffNode *)b)->counter >= ((const PuffNode *)b)->life;
		case ORIG_Beam: return ((const BeamNode *)b)->counter > 9;
		case ORIG_Spark: return ((const SparkNode *)b)->frame >= ((const SparkNode *)b)->life;
		case ORIG_Flash: return ((const FlashNode *)b)->counter >= 0x1C;
		case ORIG_Charge: return ((const ChargeNode *)b)->counter >= 0x16;
		default: return false;
		}
	}

	// beam length: 0, half, full at counters 0, 1, 2..: half way between the two ticks' lengths
	static bool g_fix_beam = false;
	static int16_t g_beam_c0 = 0, g_beam_c1 = 0;
	static void held_fix_stretch(int32_t *stretch, int32_t full)
	{
		if (!g_fix_beam) return;
		const int32_t s0 = g_beam_c0 < 2 ? (full / 2) * g_beam_c0 : full;
		const int32_t s1 = g_beam_c1 < 2 ? (full / 2) * g_beam_c1 : full;
		*stretch = lerp_i(s0, s1, g_fix_num, g_fix_den);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_draws.current(); }

	// the in-between node: discrete fields step at fraction 1, the listed int16 fields half way
	template<typename T>
	static T Between(const uint8_t *a, const uint8_t *b, int num, int den, std::initializer_list<int> lerped)
	{
		T m;
		memcpy(&m, num >= den ? b : a, sizeof(T));
		for (int off : lerped)
			*(int16_t *)((uint8_t *)&m + off) = lerp16(*(const int16_t *)(a + off), *(const int16_t *)(b + off), num, den);
		return m;
	}

	template<typename T>
	static void NextHeader(void (*fill)(uint8_t *, const T *), const T *b, bool prim, int num, int den)
	{
		g_fix_prim = prim;
		memset(g_fix_next, 0, sizeof(g_fix_next));
		fill(g_fix_next, b);
		g_fix_num = num;
		g_fix_den = den;
		g_fix = true;
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_draws.packets);
		// at fraction 1 the packets start from what the next tick writes over: the other arena (its
		// bytes the draws do not write, e.g. pad halves, are the ones of two ticks ago)
		if (num >= den && g_draws.packets)
		{
			const uint32_t other = g_draws.packets >= TexBase() + 0x8000 ? TexBase() : TexBase() + 0x8000;
			memcpy(g_held_packets, (const void *)other, 0x8000);
		}
		// the muzzle the flash / charge glows / beam draw from
		int16_t muzzle[4];
		memcpy(muzzle, Muzzle(), sizeof(muzzle));
		if (g_dir_tick == g_real_tick && g_dir_moving) HeldMuzzle(num, den);
		for (int i = 0; i < g_draws.n; i++)
		{
			const DrawRec &r = g_draws.r[i];
			const uint8_t *a = r.node, *b = g_after[i].b;
			BeginReplay(r);
			// a task that ended on the tick is not in the next tick's state (fraction 1)
			if (num >= den && Ended(r.fn, b)) { g_hdr_slot = nullptr; continue; }
			switch (r.fn)
			{
			case ORIG_Debris:
			{
				DebrisNode m = Between<DebrisNode>(a, b, num, den, { 0x12, 0x1C, 0x20 });
				NextHeader(DebrisHeader, (const DebrisNode *)b, true, num, den);
				DebrisDraw(&m);
				break;
			}
			case ORIG_Ring:
			{
				RingNode m = Between<RingNode>(a, b, num, den, { 0x20 });
				NextHeader(RingHeader, (const RingNode *)b, true, num, den);
				RingDraw(&m);
				break;
			}
			case ORIG_Puff:
			{
				PuffNode m = Between<PuffNode>(a, b, num, den, {});
				NextHeader(PuffHeader, (const PuffNode *)b, false, num, den);
				PuffDraw(&m);
				break;
			}
			case ORIG_Beam:
			{
				BeamNode m = Between<BeamNode>(a, b, num, den, { 0x18 });
				NextHeader(BeamHeader, (const BeamNode *)b, true, num, den);
				int16_t end[4];
				g_beam_c0 = ((const BeamNode *)a)->counter;
				g_beam_c1 = ((const BeamNode *)b)->counter;
				g_fix_beam = true;
				int16_t eye[3];
				memcpy(eye, (const void *)0xB8B7F0, sizeof(eye));
				if (g_eye_tick[0] == g_real_tick && g_eye_tick[1] == g_real_tick - 1)
					for (int q = 0; q < 3; q++) ((int16_t *)0xB8B7F0)[q] = (int16_t)(g_eye[0][q] + (int16_t)(g_eye[0][q] - g_eye[1][q]) * num / den);
				BeamDraw(&m, end);
				memcpy((void *)0xB8B7F0, eye, sizeof(eye));
				g_fix_beam = false;
				break;
			}
			case ORIG_Spark:
			{
				SparkNode m = Between<SparkNode>(a, b, num, den, { 0x12 });
				SparkDraw(&m);
				break;
			}
			case ORIG_Flash:
			{
				FlashNode m = Between<FlashNode>(a, b, num, den, { 0x1C });
				NextHeader(FlashHeader, (const FlashNode *)b, true, num, den);
				FlashDraw(&m);
				break;
			}
			case ORIG_Charge:
			{
				ChargeNode m = Between<ChargeNode>(a, b, num, den, { 0x1C });
				NextHeader(ChargeHeader, (const ChargeNode *)b, false, num, den);
				uint8_t *h;
				ChargeDraw(&m, &h);
				break;
			}
			case ORIG_Particles:
			{
				uint8_t *h = AllocHeader(0xB4);
				*(uint32_t *)h = SEQ_Dust;
				*(int16_t *)(h + 0x24) = 0;
				for (int k = 0; k < PARTICLES; k++)
				{
					const Particle &pa = g_dust[k];
					if (!(pa.flag & 1)) continue;
					Particle m = pa;
					const Particle &pb = Particles()[k];
					if (!(pb.flag & 1))
					{
						if (num >= den) continue; // freed on the tick: not in the next tick's state
					}
					else
					{
						// still in use: the next tick draws it at its new position / frame
						if (num >= den) m.counter = pb.counter;
						for (int c = 0; c < 3; c++) m.pos[c] = lerp16(pa.pos[c], pb.pos[c], num, den);
						m.scale = lerp16(pa.scale, pb.scale, num, den);
					}
					// frame 0 is not drawn (a particle born on the tick appears at fraction 1)
					if (m.counter == 0) continue;
					ParticleDraw(h, &m);
				}
				FieldFree(0xB4);
				break;
			}
			default:
				g_hdr_slot = nullptr;
				break;
			}
			g_fix = false;
		}
		memcpy(Muzzle(), muzzle, sizeof(muzzle));
	}
}

	static void register_mag307_held()
	{
		mark_held(raybomb307::ORIG_Debris);
		mark_held(raybomb307::ORIG_Ring);
		mark_held(raybomb307::ORIG_Puff);
		mark_held(raybomb307::ORIG_Beam);
		mark_held(raybomb307::ORIG_Spark);
		mark_held(raybomb307::ORIG_Flash);
		mark_held(raybomb307::ORIG_Charge);
		mark_held(raybomb307::ORIG_RootTask); // the dust particles are drawn by the root
		register_module_held(307, raybomb307::HeldReady, raybomb307::HeldFrame);
	}
}
