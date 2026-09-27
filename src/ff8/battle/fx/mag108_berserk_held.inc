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

// 30 fps part of mag108_berserk.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag108_berserk.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// A real tick records every Berserk draw in order with the GTE registers and the Field_Alloc
// scratch it started from (draw headers keep the scratch bytes where the code does not write).
// A held frame replays the log in the same order, each draw at its in-between state:
//   Flash    - the flash sequence drawn exactly as on the real tick (fixed place, 15 Hz frames).
//   Glow     - scale and fade half way to the next tick's (a counter ramp over 17..31; the last
//              draw holds).
//   Burst    - the prim-model layout through prim::play_held: every object's exact in-between
//              record (position, rotation, scale, colour, fade, vertex-frame blend); blended
//              vertex frames go to a private buffer.
//   Rays     - a copy of the ray table as drawn: every ray that is still alive after its update
//              is drawn with its spin, length, width and colours half way to the next tick's
//              (rays that die in this update hold); the copy is drawn with the real draw code.
//   Swirl    - a copy of the ribbon as the draw found it: the brightness step of the real tick
//              is applied, then every position's brightness goes half way to the next tick's and
//              its angle half way along its wave step; the effect frame is rebuilt from the
//              current camera; the positions the next tick pushes are not shown (15 Hz batches).
//   Particle - an ember rises half way to its next height (the task continues); sparks and the
//              last ember draw hold; sequence frames step at 15 Hz.
// No Berserk task has a draw-only mode; the module does not drive the camera.

namespace ff8fx
{
namespace berserk108
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_FLASH = 1, REC_GLOW = 2, REC_PLAY = 3, REC_RAYS = 4, REC_SWIRL = 5, REC_PARTICLE = 6 };

	struct Rec
	{
		uint32_t kind;
		int16_t pos[4];           // flash, glow, rays, swirl: the effect anchor
		int32_t e;                // flash / glow step
		int32_t copy;             // rays / swirl: index of the table copy
		prim::Layout *layout;     // play: the live layout (prim::play_held knows what it drew)
		PrimArg arg;
		ParticleNode part;        // particle, as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct SwirlCopy
	{
		int16_t counter;
		int32_t count;
		SwirlEntry trail[32];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_nrays = 0, g_nswirls = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static Rec g_recs[160];
	static Ray g_rays[8][32];            // ray tables as drawn (one per main task)
	static SwirlCopy g_swirls[16];       // ribbons as the draw found them (one per swirl task)
	static SwirlEntry g_cur[32], g_next[32];
	static uint8_t g_held_packets[0x20000]; // module arena (0x18000 per tick)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames (magic buffer + 0x52CC)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_nrays = 0;
		g_nswirls = 0;
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

	static void held_note_flash(const MainNode *m, int32_t e)
	{
		Rec *r = NewRec(REC_FLASH);
		if (!r) return;
		memcpy(r->pos, m->pos, sizeof(r->pos));
		r->e = e;
	}

	static void held_note_glow(const MainNode *m, int32_t e)
	{
		Rec *r = NewRec(REC_GLOW);
		if (!r) return;
		memcpy(r->pos, m->pos, sizeof(r->pos));
		r->e = e;
	}

	static void held_note_play(const MainNode *m, const PrimArg *arg)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = (prim::Layout *)m->burst;
		r->arg = *arg;
	}

	static void held_note_rays(const MainNode *m)
	{
		if (g_nrays >= (int)(sizeof(g_rays) / sizeof(g_rays[0]))) return;
		Rec *r = NewRec(REC_RAYS);
		if (!r) return;
		memcpy(r->pos, m->pos, sizeof(r->pos));
		r->copy = g_nrays++;
		memcpy(g_rays[r->copy], m->rays, sizeof(m->rays));
	}

	static void held_note_swirl(const SwirlNode *s)
	{
		if (g_nswirls >= (int)(sizeof(g_swirls) / sizeof(g_swirls[0]))) return;
		Rec *r = NewRec(REC_SWIRL);
		if (!r) return;
		memcpy(r->pos, s->owner->pos, sizeof(r->pos));
		r->copy = g_nswirls++;
		SwirlCopy &c = g_swirls[r->copy];
		c.counter = s->counter;
		c.count = s->count;
		memcpy(c.trail, s->trail, sizeof(c.trail));
	}

	static void held_note_particle(const ParticleNode *p)
	{
		Rec *r = NewRec(REC_PARTICLE);
		if (!r) return;
		r->part = *p;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static inline uint8_t lerp8(uint8_t a, uint8_t b, int num, int den) { return (uint8_t)lerp_i(a, b, num, den); }

	static void GlowHeld(const Rec &r, int num, int den)
	{
		int32_t scale = GlowScale(r.e);
		int32_t fade = GlowFade(r.e);
		if (r.e + 1 < 0xF)
		{
			scale = lerp_i(scale, GlowScale(r.e + 1), num, den);
			fade = lerp_i(fade, GlowFade(r.e + 1), num, den);
		}
		GlowDraw(r.pos, scale, fade);
	}

	static void RaysHeld(const Rec &r, int num, int den)
	{
		Ray rays[32];
		memcpy(rays, g_rays[r.copy], sizeof(rays));
		for (int k = 0; k < 32; k++)
		{
			Ray &a = rays[k];
			if (!a.alive) continue;
			Ray b = a;
			RayUpdate(&b);
			if (!b.alive) continue; // not drawn on the next tick: holds
			a.spin = lerp16(a.spin, b.spin, num, den);
			a.length = lerp16(a.length, b.length, num, den);
			a.width = lerp16(a.width, b.width, num, den);
			for (int i = 0; i < 3; i++)
			{
				a.col0[i] = lerp8(a.col0[i], b.col0[i], num, den);
				a.col1[i] = lerp8(a.col1[i], b.col1[i], num, den);
				a.col2[i] = lerp8(a.col2[i], b.col2[i], num, den);
			}
		}
		int32_t alive;
		PacketCursor() = RayDraw(0x20, rays, r.pos, &alive, RenderOT(0x44), 2, PacketCursor());
	}

	static void SwirlHeld(const Rec &r, int num, int den)
	{
		const SwirlCopy &c = g_swirls[r.copy];
		int32_t n = c.count;
		memcpy(g_cur, c.trail, sizeof(g_cur));
		SwirlFade(n, g_cur, -0x28);       // brightness as drawn on the real tick
		memcpy(g_next, g_cur, sizeof(g_next));
		SwirlFade(n, g_next, -0x28);      // as the next tick draws it
		for (int32_t k = 0; k < n && k < 32; k++)
		{
			SwirlEntry &e = g_cur[k];
			e.bright = lerp16(e.bright, g_next[k].bright, num, den);
			const int16_t wave = (int16_t)(ComputeSin(e.phase) >> 6);
			e.angle = lerp16(e.angle, (int16_t)(e.angle + wave), num, den);
		}
		SwirlFrame f = {};
		SwirlAnchor(r.pos, &f);
		SwirlBasis(&f);
		PacketCursor() = SwirlStrip(&n, g_cur, shl32(c.counter, 5), RenderOT(0x44), 2, PacketCursor());
	}

	static void ParticleHeld(const Rec &r, int num, int den)
	{
		ParticleNode p = r.part;
		if (p.seq == SEQ_Ember && (int8_t)(uint8_t)(p.counter + 1) < 0xE)
		{
			const int16_t vel = (int16_t)(p.vel + p.accel);
			p.pos[1] = lerp16(p.pos[1], (int16_t)(p.pos[1] + vel), num, den);
		}
		ParticleDraw(&p);
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
			case REC_FLASH: FlashDraw(r.pos, r.e); break;
			case REC_GLOW: GlowHeld(r, num, den); break;
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				memcpy(g_held_morph, (const void *)r.arg.morph, sizeof(g_held_morph));
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held(r.layout, BurstPart, (int)&arg, num, den);
				break;
			}
			case REC_RAYS: RaysHeld(r, num, den); break;
			case REC_SWIRL: SwirlHeld(r, num, den); break;
			case REC_PARTICLE: ParticleHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag108_held()
	{
		mark_held(berserk108::ORIG_MainTask);
		mark_held(berserk108::ORIG_SwirlTask);
		mark_held(berserk108::ORIG_ParticleTask);
		register_module_held(108, berserk108::HeldReady, berserk108::HeldFrame);
	}
}
