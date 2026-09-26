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

// 30 fps part of mag149_ultima.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag149_ultima.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Ultima task draws first and updates afterwards. A real tick records each drawing task in
// order: its node, the particle array it draws from (arrays A / B), the GTE registers and the
// Field_Alloc scratch it started from (headers and work blocks keep the scratch bytes where the
// code does not write). A held frame replays the log in the same order, each part moved num / den
// of the way to the state its own update computes:
//   Orbit   - orbit angle along its step (the position is recomputed on the orbit); flipbook holds.
//   Streaks - position along the velocity, scale toward the next shrink step, velocity toward the
//             next damping step (orientation); fade = frame-parametric ramp at frame + num / den.
//             The last frame (the update kills it without moving it) holds.
//   Shards  - scale toward the next growth step, spin angle along its step; fade ramps at
//             frame + num / den (in the fading ranges); the last frame holds.
//   Energy  - spin along its step, scale toward the next (accelerating, clamped) step; fade ramp.
//   Ring    - scale toward the next step (and so the ring scale global the core reads), dome UV
//             scroll and dome fade at frame + num / den; the ring's fade table entry holds.
//   Core    - spin along its (decaying) step; the morph weight sin((frame + num / den) * 1024 / 40)
//             and the fade ramp at the fractional frame; scaled by the ring's in-between scale.
//   Dust    - position along its velocity; flipbook holds.
//   Sparks  - position along its velocity (the last frame holds); flipbook holds.
// Side effects of the held draw are undone: the ring scale global and the core vertex buffer are
// put back, the packets go to a private buffer (HeldScope). The director, the side fades (model
// colour words, 15 Hz) and the root draw nothing.
// In the draw-only mode (battle_to_update_flags & 0x201) nothing moved: everything holds.

namespace ff8fx
{
namespace ultima149
{
	static const uint32_t GTE_DATA = 0x1CA8A10, GTE_DATA_SIZE = 0x70, GTE_CTRL = 0x1CA9230, GTE_CTRL_SIZE = 0xD0;
	static const uint32_t SCRATCH = 0x400;

	struct Rec
	{
		uint32_t fn;
		PartNode node;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
		Particle parts[ARRAY_COUNT]; // the array the task draws from (particle tasks)
	};

	// real tick on which the ported root task last ran, and its draw log
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0; // packet cursor at the tick's first draw
	static Rec g_recs[64];
	static uint8_t g_held_packets[0x20000];
	static uint8_t g_core_save[4][0x3C0];
	// held frame being drawn: frame + num / den (den 0 = real tick)
	static int g_hnum = 0, g_hden = 0;

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static void held_note_draw(uint32_t fn, const PartNode *node)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.node = *node;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		if (fn == ORIG_Sparks) memcpy(r.parts, ArrayA(), sizeof(r.parts));
		else if (fn == ORIG_Orbit || fn == ORIG_Streaks || fn == ORIG_Shards || fn == ORIG_Dust) memcpy(r.parts, ArrayB(), sizeof(r.parts));
	}

	// a value linear in the frame (offset + frame * slope) at the held frame's fractional frame
	static int32_t held_ramp(int32_t v, int32_t frame, int32_t slope, int32_t offset)
	{
		if (!g_hden) return v;
		return offset + (frame * g_hden + g_hnum) * slope / g_hden;
	}

	// a sin() parameter (frame << shift) / duration at the held frame's fractional frame
	static int32_t held_frame_arg(int32_t t, int32_t frame, int shift, int32_t duration)
	{
		if (!g_hden) return t;
		return shl32(frame * g_hden + g_hnum, shift) / (duration * g_hden);
	}

	static int16_t Lerp16(int16_t a, int16_t b) { return lerp16(a, b, g_hnum, g_hden); }

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		if (DrawOnly()) num = 0; // the real tick only drew: nothing moves
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		int32_t ring_scale = RingScale();
		int ncore = 0;
		uint8_t *core_buf[4];
		g_hnum = num;
		g_hden = den;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			PartNode a = r.node;
			switch (r.fn)
			{
			case ORIG_Orbit:
			{
				int16_t *p;
				uint8_t *h = OrbitBegin(&a, &p);
				for (int k = 0; k < ARRAY_COUNT; k++)
				{
					Particle e = r.parts[k];
					if (!(*(uint8_t *)&e.type & 1)) continue;
					e.vel[0] = Lerp16(e.vel[0], (int16_t)(e.vel[0] + e.vel[2]));
					OrbitDraw(h, p, &e);
				}
				FieldFree(0x10);
				FieldFree(0xB4);
				break;
			}
			case ORIG_Streaks:
			{
				StreakWork *w;
				uint8_t *h = StreakBegin(&w);
				for (int k = 0; k < ARRAY_COUNT; k++)
				{
					Particle e = r.parts[k];
					if (!(*(uint8_t *)&e.type & 2)) continue;
					if (e.frame + 1 < 8)
					{
						Particle b = e;
						StreakMove(&b);
						for (int c = 0; c < 3; c++)
						{
							e.pos[c] = Lerp16(e.pos[c], b.pos[c]);
							e.vel[c] = Lerp16(e.vel[c], b.vel[c]);
						}
						e.scale = Lerp16(e.scale, b.scale);
					}
					StreakDraw(h, w, &e);
				}
				FieldFree(0x88);
				FieldFree(0x58);
				break;
			}
			case ORIG_Shards:
			{
				ShardWork *w;
				uint8_t *h = ShardBegin(&a, &w);
				for (int k = 0; k < ARRAY_COUNT; k++)
				{
					Particle e = r.parts[k];
					if (!(*(uint8_t *)&e.type & 4)) continue;
					if (e.frame + 1 < 0x10)
					{
						e.scale = (int16_t)lerp_i(e.scale, (int16_t)(e.scale + Div6(e.scale)), num, den);
						e.angle = Lerp16(e.angle, (int16_t)(e.angle + e.spin));
					}
					ShardDraw(h, w, &e);
				}
				FieldFree(0xA0);
				FieldFree(0x58);
				break;
			}
			case ORIG_Energy:
			{
				PartNode b = a;
				EnergyMove(&b);
				a.rot = Lerp16(a.rot, b.rot);
				a.scale = (int16_t)lerp_i(a.scale, b.scale, num, den);
				EnergyDraw(&a);
				break;
			}
			case ORIG_Ring:
				a.scale = (int16_t)lerp_i(a.scale, (int16_t)(a.scale + a.dscale), num, den);
				RingDraw(&a);
				break;
			case ORIG_Core:
				if (ncore < 4)
				{
					core_buf[ncore] = CoreVertices() + a.index * 0x3C0;
					memcpy(g_core_save[ncore], core_buf[ncore], 0x3C0);
					ncore++;
				}
				a.rot = Lerp16(a.rot, (int16_t)(a.rot + a.rotvel));
				CoreDraw(&a);
				break;
			case ORIG_Dust:
			{
				uint8_t *h = DustBegin();
				for (int k = 0; k < ARRAY_COUNT; k++)
				{
					Particle e = r.parts[k];
					if (!(*(uint8_t *)&e.type & 8)) continue;
					for (int c = 0; c < 3; c++) e.pos[c] = Lerp16(e.pos[c], (int16_t)(e.pos[c] + e.vel[c]));
					DustDraw(h, &e);
				}
				FieldFree(0xB4);
				break;
			}
			case ORIG_Sparks:
			{
				uint8_t *h = SparkBegin();
				for (int k = 0; k < ARRAY_COUNT; k++)
				{
					Particle e = r.parts[k];
					if (!(*(uint8_t *)&e.type & 1)) continue;
					if (e.frame + 1 < 0x10)
						for (int c = 0; c < 3; c++) e.pos[c] = Lerp16(e.pos[c], (int16_t)(e.pos[c] + e.vel[c]));
					SparkDraw(h, &e);
				}
				FieldFree(0xB4);
				break;
			}
			}
		}
		g_hden = 0;
		g_hnum = 0;
		while (ncore > 0)
		{
			ncore--;
			memcpy(core_buf[ncore], g_core_save[ncore], 0x3C0);
		}
		RingScale() = ring_scale;
	}
}

	static void register_mag149_held()
	{
		mark_held(ultima149::ORIG_Orbit);
		mark_held(ultima149::ORIG_Streaks);
		mark_held(ultima149::ORIG_Shards);
		mark_held(ultima149::ORIG_Energy);
		mark_held(ultima149::ORIG_Ring);
		mark_held(ultima149::ORIG_Core);
		mark_held(ultima149::ORIG_Dust);
		mark_held(ultima149::ORIG_Sparks);
		register_module_held(149, ultima149::HeldReady, ultima149::HeldFrame);
	}
}
