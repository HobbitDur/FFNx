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

// 30 fps part of mag145_sleep.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag145_sleep.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Sleep task draws first and updates afterwards. A real tick records each drawing task in
// order: its node, the trail it draws (as drawn, and where it lives), the GTE registers and the
// Field_Alloc scratch it started from (the draw headers keep the scratch bytes the code does not
// write: the bubble fade before tick 0xE, the model header). A held frame replays the log in the
// same order, each part moved num / den of the way to the state its own update computes:
//   Main visual  - radius and height toward the next step (until tick 0x12), spin and orbit angle
//                  along their steps: the bubble is recomputed on its circle; the fade-in at
//                  tick + num / den (ramp); the ground copy follows (its fade from the height);
//                  every trail ghost from its entry toward the next newer one (the newest toward
//                  the bubble's position as drawn), spin angle included. The tinted target model
//                  at its in-between pose (pose_midpoint), around the in-between bubble, depth
//                  cued by the in-between fade; its shadow is redrawn as it is.
//   Bubbles      - position along the velocity (the bounce clamp to the ground included; the
//                  random new speed / turn of a bounce are the next tick's), fade-out ramp at
//                  tick + num / den; trail ghosts toward the next newer entry, their angle holds.
//   Particles    - position along the velocity, size toward the next shrink step; the flipbook
//                  frame holds.
//   Zzz sprites  - fixed place, flipbook: hold.
// The model draw's side effects are undone: the frame packet cursor goes to a private copy of the
// frame arena, the vertex buffer (0x1D98B3C) to a private copy of it, the shadow / geometry
// temporaries are put back, and the bone matrices are rebuilt from the real pose; the module
// packets go to a private buffer (HeldScope). The director draws nothing.
// In the draw-only mode (battle_to_update_flags & 0x201) nothing moved: everything holds.

namespace ff8fx
{
namespace sleep145
{
	static const uint32_t SCRATCH = 0x400;

	struct Rec
	{
		uint32_t fn;
		uint8_t node[0x24];
		Trail trail[3];            // the trail as drawn
		const Trail *live;         // the trail itself (after the tick: the next draw's)
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	// real tick on which the ported root last ran, and its draw log
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static bool g_overflow = false;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static uint32_t g_frame_tick = 0xFFFFFFFF;
	static uint32_t g_frame_packets = 0; // frame packet cursor at the tick's first model draw
	static Rec g_recs[512];
	static Zzz g_zzz[40];                // the Zzz sprites as drawn
	static uint8_t g_held_packets[0x10000]; // module arena (0x8000 per tick)
	static uint8_t g_held_frame[0x40000];
	static uint8_t g_held_verts[0x10000];
	// held frame being drawn (g_held), at tick + num / den (den 0: nothing moves)
	static bool g_held = false;
	static int g_hnum = 0, g_hden = 0;

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_overflow = false;
		g_packets = 0;
	}

	static Rec *held_add(uint32_t fn)
	{
		if (g_held || g_log_tick != g_real_tick) return nullptr;
		if (g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0])))
		{
			g_overflow = true;
			return nullptr;
		}
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.live = nullptr;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_draw(uint32_t fn, const void *node, const Trail *trail)
	{
		Rec *r = held_add(fn);
		if (!r) return;
		memcpy(r->node, node, sizeof(r->node));
		if (trail)
		{
			memcpy(r->trail, trail, sizeof(r->trail));
			r->live = trail;
		}
	}

	static void held_note_zzz()
	{
		if (held_add(ORIG_Root)) memcpy(g_zzz, (const void *)ZZZ, sizeof(g_zzz));
	}

	// the target model: at its in-between pose on a held frame; the real tick notes where the
	// frame arena was at its first model draw
	static void held_model_pose(uint32_t entity)
	{
		if (g_held)
		{
			if (g_hden) pose_midpoint((void *)(entity + 0x60), (void *)(entity + 0x6C), g_hnum, g_hden);
			return;
		}
		if (g_frame_tick != g_real_tick)
		{
			g_frame_tick = g_real_tick;
			g_frame_packets = FrameCursor();
		}
	}

	// a value linear in the tick (offset + tick * slope) at the held frame's fractional tick
	static int32_t held_ramp(int32_t v, int32_t frame, int32_t slope, int32_t offset)
	{
		if (!g_hden) return v;
		return offset + (frame * g_hden + g_hnum) * slope / g_hden;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick && !g_overflow; }

	// largest vertex count of one drawn object of a model (the vertex buffer each object is built in)
	static uint32_t MaxObjectVertices(uint32_t model, uint32_t visible)
	{
		uint32_t best = 0;
		const uint32_t table = U32(model, 4);
		const int32_t count = S32(table, 0);
		for (int32_t i = 0; i < count && i < 64; i++)
		{
			if (!(visible & (1u << (i & 31)))) continue;
			uint32_t obj = table + U32(table, 4 + 4 * i);
			int32_t groups = S16(obj, 0);
			obj += 2;
			uint32_t n = 0;
			for (; groups > 0; groups--)
			{
				const int32_t nv = S16(obj, 2);
				obj += 4 + (nv > 0 ? nv : 0) * 6;
				if (nv > 0) n += (uint32_t)nv;
			}
			if (n > best) best = n;
		}
		return best;
	}

	// the engine state a model draw changes, private or put back for the held frame
	struct ModelScope
	{
		bool on;
		uint32_t frame, verts, geom[2], shadow_words[2];
		uint8_t shadow[0x80];
		ModelScope(bool model_drawn) : on(model_drawn)
		{
			if (!on) return;
			frame = FrameCursor();
			if (g_frame_packets && frame > g_frame_packets)
			{
				uint32_t used = frame - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = P(g_held_frame);
			verts = var<uint32_t>(0x1D98B3C);
			geom[0] = var<uint32_t>(0x1D99C18);
			geom[1] = var<uint32_t>(0x1D99C1C);
			shadow_words[0] = var<uint32_t>(0xB8B9E4);
			shadow_words[1] = var<uint32_t>(0xB8B9EC);
			memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
			// the model's faces read the vertex buffer's pad words (never written by the draw): the
			// private buffer starts as a copy of the real one
			uint32_t n = 0;
			for (int i = 0; i < g_nrec; i++)
			{
				if (g_recs[i].fn != ORIG_Main) continue;
				const MainNode *m = (const MainNode *)g_recs[i].node;
				if (m->counter >= 0x1A) continue;
				const uint32_t e = P(Entity(Ctx()->actions[m->action].targets[0]));
				uint32_t k = MaxObjectVertices(U32(e + 0x60, 4), U32(e, 0x7C));
				if (k > n) n = k;
				const uint32_t weapon = U32(e, 0x78);
				if (weapon && !(U8(e, 1) & 2))
				{
					k = MaxObjectVertices(U32(weapon, 4), 0xFFFFFFFF);
					if (k > n) n = k;
				}
			}
			n *= 8;
			if (verts) memcpy(g_held_verts, (const void *)verts, n < sizeof(g_held_verts) ? n : sizeof(g_held_verts));
			var<uint32_t>(0x1D98B3C) = P(g_held_verts);
		}
		~ModelScope()
		{
			if (!on) return;
			var<uint32_t>(0x1D98B3C) = verts;
			memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
			var<uint32_t>(0xB8B9EC) = shadow_words[1];
			var<uint32_t>(0xB8B9E4) = shadow_words[0];
			var<uint32_t>(0x1D99C1C) = geom[1];
			var<uint32_t>(0x1D99C18) = geom[0];
			FrameCursor() = frame;
		}
	};

	// the trail at the held frame: each ghost drawn (slot s) moved toward the next newer entry,
	// the one the next tick's ghost of the same rank draws (slot s + 1 of the updated trail)
	static void HeldTrail(const Rec &r, int32_t c, bool angles, Trail *t)
	{
		memcpy(t, r.trail, sizeof(r.trail));
		if (!g_hden || c < 1 || !r.live) return;
		for (int s = 0; s < 3; s++)
		{
			const Trail &b = r.live[(s + 1) % 3];
			if (!t[s].used || !b.used) continue;
			for (int k = 0; k < 3; k++) t[s].pos[k] = lerp16(t[s].pos[k], b.pos[k], g_hnum, g_hden);
			if (angles) t[s].angle = lerp16(t[s].angle, b.angle, g_hnum, g_hden);
		}
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		ModelScope model(g_frame_tick == g_real_tick);
		const bool moving = !DrawOnly();
		g_held = true;
		g_hnum = moving ? num : 0;
		g_hden = moving ? den : 0;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.fn)
			{
			case ORIG_Particle:
			{
				ParticleNode a;
				memcpy(&a, r.node, sizeof(a));
				ParticleNode m = a;
				if (g_hden)
				{
					ParticleNode b = a;
					ParticleUpdate(&b);
					for (int k = 0; k < 3; k++) m.pos[k] = lerp16(a.pos[k], b.pos[k], num, den);
					m.scale = lerp16(a.scale, b.scale, num, den);
				}
				ParticleDraw(&m);
				break;
			}
			case ORIG_Bubble:
			{
				BubbleNode a;
				memcpy(&a, r.node, sizeof(a));
				BubbleNode m = a;
				if (g_hden)
				{
					int16_t b[3];
					b[0] = (int16_t)(a.pos[0] + a.vx);
					b[1] = (int16_t)(a.pos[1] + a.vy);
					if (b[1] > 0) b[1] = 0; // the bounce clamps it to the ground
					b[2] = (int16_t)(a.pos[2] + a.vz);
					for (int k = 0; k < 3; k++) m.pos[k] = lerp16(a.pos[k], b[k], num, den);
				}
				Trail t[3];
				HeldTrail(r, a.counter, false, t);
				int16_t drawn[3];
				BubbleDraw(&m, t, drawn);
				break;
			}
			case ORIG_Main:
			{
				MainNode a;
				memcpy(&a, r.node, sizeof(a));
				MainNode m = a;
				if (g_hden)
				{
					if (a.counter < 0x12)
					{
						m.pos[1] = lerp16(a.pos[1], (int16_t)(a.pos[1] + a.vy), num, den);
						m.radius = lerp16(a.radius, (int16_t)(a.radius - a.shrink), num, den);
					}
					m.spin = lerp16(a.spin, (int16_t)(a.spin + a.spin_v), num, den);
					m.orbit = lerp16(a.orbit, (int16_t)(a.orbit + a.orbit_v), num, den);
				}
				Trail t[3];
				HeldTrail(r, a.counter, true, t);
				MainFrame f;
				MainDraw(&m, t, &f);
				break;
			}
			case ORIG_Root:
			{
				uint8_t *h = ZzzBegin();
				for (int k = 0; k < 40; k++)
					if (g_zzz[k].frame >= 0) ZzzDraw(h, &g_zzz[k]);
				FieldFree(0xB4);
				break;
			}
			}
		}
		g_hden = 0;
		g_hnum = 0;
		g_held = false;
	}
}

	static void register_mag145_held()
	{
		mark_held(sleep145::ORIG_Root); // the root draws the Zzz sprites
		mark_held(sleep145::ORIG_Main);
		mark_held(sleep145::ORIG_Bubble);
		mark_held(sleep145::ORIG_Particle);
		register_module_held(145, sleep145::HeldReady, sleep145::HeldFrame);
	}
}
