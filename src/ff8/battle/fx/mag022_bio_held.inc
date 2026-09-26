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

// 30 fps part of mag022_bio.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag022_bio.cpp; shared pieces in mag_common_held.h).
//
// Every Bio task updates THEN draws. A real tick records every draw in order: the task's node as
// drawn, the GTE registers and the Field_Alloc scratch the draw started with (sprite headers keep
// what the scratch held), and for the overlay its wobble. A held frame replays the log in the same
// order, from the same GTE state and scratch, each part moved to its exact in-between state; the
// next tick's state comes from the task's own (pure) update run on a copy:
//   Splash / droplet / mist B - position half way along the next update (circling rise, fall
//            with gravity, flight with deceleration); the flipbook frame holds.
//   Bubble / mist A - flipbooks at a fixed place: hold.
//   Pool   - the next tick of the pool (its phase function and position update, with the emitter's
//            next spin / counter) on a copy: spin, scale and collapse step half way, the position
//            recomputed from them (arc around the target); the morph half way along the same key
//            shapes (when the next tick moves on to the next key pair, along the continuous
//            key path); a switch of mesh / puddle mode holds the morph. The vertices are morphed
//            into a private copy of the mesh (the real mesh lives in the exe data).
//   Blob   - morph 0x400 per tick: half way along its current key pair (a key or mesh change of
//            the next tick happens at 0x1000, the end of this pair); scale / spin half way to the
//            wobble the tick left for the next draw; the tick it burst (no draw next) holds.
//   Flash  - colour half way to the script's next colour; toward black when the script ends
//            (additive quad: black = nothing drawn).
//   Overlay - the target at its in-between pose (pose_midpoint), wobble phase + 0xA0 / 2 and
//            amplitude half way to the next tick's; its shadow (sub_5088A0: a pure draw whose
//            globals are private scratch of the shadow draw) is drawn again. Vertices go to a
//            private copy of the overlay vertex buffer, packets to a private copy of the frame
//            arena; the overlay context and the real pose's bone matrices are put back.
// Held without interpolation: the stage wobble amplitude (0x1D98990, the stage renderer's) and
// the acid palette (a VRAM upload) keep their 15 Hz steps. No Bio task has a draw-only mode.

namespace ff8fx
{
namespace bio022
{
	static const uint32_t SCRATCH = 0x200;
	static const uint32_t VERTS_SIZE = 0x6400; // overlay vertex buffer 0x270E560..0x2714960
	// the GTE control registers' float mirrors start with the light matrix L at 0x1CA9210, below
	// the window of mag_common_held.h (0x1CA9230): Bio's puddle / overlay draws set L
	static const uint32_t GTE_LIGHT = 0x1CA9210, GTE_LIGHT_SIZE = GTE_CTRL - GTE_LIGHT;

	struct Rec
	{
		uint32_t fn;
		const void *live;             // the node itself (its state after the tick)
		uint8_t node[sizeof(MeshNode)];
		int16_t amp, phase;           // overlay: wobble of the draw
		uint8_t gte[GTE_DATA_SIZE + GTE_LIGHT_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	// real tick on which the ported root last ran, and its draw log
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0; // frame arena cursor at the first overlay draw
	static Rec g_recs[512];
	static uint8_t g_held_packets[0x10000]; // module arena (0xC000 per tick)
	static uint8_t g_held_frame[0x20000];   // overlay packets
	static uint8_t g_held_verts[VERTS_SIZE];
	static uint8_t g_held_mesh[0x6000];     // morphed mesh (largest 0x5024 bytes)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_frame_packets = 0;
	}

	static uint32_t NodeSize(uint32_t fn)
	{
		switch (fn)
		{
		case ORIG_Pool:
		case ORIG_Blob: return sizeof(MeshNode);
		case ORIG_Flash: return sizeof(FlashNode);
		case ORIG_Overlay: return sizeof(WobbleNode);
		default: return sizeof(Particle);
		}
	}

	static void held_note_draw(uint32_t fn, const void *node)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.live = node;
		memset(r.node, 0, sizeof(r.node));
		memcpy(r.node, node, NodeSize(fn));
		if (fn == ORIG_Overlay)
		{
			r.amp = Overlay().wobble_amp;
			r.phase = Overlay().wobble_phase;
			if (!g_frame_packets) g_frame_packets = var<uint32_t>(0x1D8E054);
		}
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_LIGHT, GTE_LIGHT_SIZE + GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_LIGHT, r.gte + GTE_DATA_SIZE, GTE_LIGHT_SIZE + GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// the overlay's outputs of a held frame: frame arena packets (a copy of the real tick's, so
	// bytes the draw leaves alone are the real ones), vertex buffer, context, and the light
	// matrix mirror (puddle and overlay draws); all put back
	struct OverlayScope
	{
		uint32_t cursor, verts;
		uint8_t ctx[sizeof(OverlayCtx)];
		uint8_t light[GTE_LIGHT_SIZE];
		OverlayScope()
		{
			memcpy(light, (const void *)GTE_LIGHT, GTE_LIGHT_SIZE);
			uint32_t &fc = var<uint32_t>(0x1D8E054);
			cursor = fc;
			if (g_frame_packets && cursor > g_frame_packets)
			{
				uint32_t used = cursor - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			fc = (uint32_t)g_held_frame;
			memcpy(ctx, &Overlay(), sizeof(ctx));
			verts = *(uint32_t *)(OVERLAY_SPRITE + 4);
			memcpy(g_held_verts, (const void *)OVERLAY_VERTS, sizeof(g_held_verts));
			*(uint32_t *)(OVERLAY_SPRITE + 4) = (uint32_t)g_held_verts;
		}
		~OverlayScope()
		{
			*(uint32_t *)(OVERLAY_SPRITE + 4) = verts;
			memcpy(&Overlay(), ctx, sizeof(ctx));
			var<uint32_t>(0x1D8E054) = cursor;
			memcpy((void *)GTE_LIGHT, light, GTE_LIGHT_SIZE);
		}
	};

	static uint32_t MeshSize(const uint8_t *mesh)
	{
		int32_t count;
		const uint8_t *p = MeshPolys(mesh, &count);
		return (uint32_t)(p - mesh) + (uint32_t)count * 0x1C;
	}

	// the mesh's vertices morphed into a private copy of it
	static void HeldMorph(MeshNode &m)
	{
		uint32_t size = MeshSize(m.mesh);
		memcpy(g_held_mesh, m.mesh, size < sizeof(g_held_mesh) ? size : sizeof(g_held_mesh));
		m.mesh = g_held_mesh;
		Morph(m.src, m.dst, m.mesh, m.t);
	}

	// angle half way the short way round (12-bit angles); `wrap` keeps the result in 0..0xFFF
	static int16_t HalfAngle(int16_t a, int16_t b, int num, int den, bool wrap)
	{
		int32_t d = (b - a) & 0xFFF;
		if (d >= 0x800) d -= 0x1000;
		int32_t v = a + d * num / den;
		return (int16_t)(wrap ? (v & 0xFFF) : v);
	}

	static void PoolHeld(const Rec &r, int num, int den)
	{
		MeshNode a;
		memcpy(&a, r.node, sizeof(a));
		// the next tick on copies: the emitter turns and counts first
		EmitterNode e2 = *(const EmitterNode *)a.h.emitter;
		e2.spin = (int16_t)((uint16_t)(e2.spin - 0x40) & 0xFFF);
		e2.h.counter++;
		MeshNode b = a;
		b.h.emitter = &e2.h;
		PoolPhase(&b);
		PoolUpdatePos(&b);
		MeshNode m = a;
		m.spin = HalfAngle(a.spin, b.spin, num, den, false);
		m.frac = lerp16(a.frac, b.frac, num, den);
		for (int k = 0; k < 3; k++) m.scale[k] = lerp_i(a.scale[k], b.scale[k], num, den);
		if (b.mesh == a.mesh && b.lit == a.lit)
		{
			if (b.src == a.src && b.dst == a.dst) m.t = lerp16(a.t, b.t, num, den);
			else if (b.key_a == a.key_a + 1 && b.key_b == a.key_b + 1)
			{
				int32_t u = lerp_i(a.t, b.t + 0x1000, num, den);
				if (u <= 0x1000) m.t = (int16_t)u;
				else
				{
					m.t = (int16_t)(u - 0x1000);
					m.key_a = b.key_a;
					m.key_b = b.key_b;
					m.src = b.src;
					m.dst = b.dst;
				}
			}
		}
		PoolUpdatePos(&m);
		Replay(r);
		HeldMorph(m);
		if (m.lit == 0) DrawMesh(&m);
		else DrawMeshPuddle(&m);
	}

	static void BlobHeld(const Rec &r, int num, int den)
	{
		MeshNode a;
		memcpy(&a, r.node, sizeof(a));
		const MeshNode *live = (const MeshNode *)r.live;
		MeshNode m = a;
		if (live->h.phase == 1 || live->h.phase == 2)
		{
			m.t = (int16_t)(a.t + 0x400 * num / den);
			for (int k = 0; k < 3; k++) m.scale[k] = lerp_i(a.scale[k], live->scale[k], num, den);
			m.rot[0] = HalfAngle(a.rot[0], live->rot[0], num, den, true);
			m.rot[1] = HalfAngle(a.rot[1], live->rot[1], num, den, true);
		}
		const EmitterNode *e = (const EmitterNode *)a.h.emitter;
		Replay(r);
		HeldMorph(m);
		if (e->big == 1) DrawMeshPuddle(&m);
		else DrawMesh(&m);
	}

	static void FlashHeld(const Rec &r, int num, int den)
	{
		FlashNode m;
		memcpy(&m, r.node, sizeof(m));
		uint32_t w = m.script[m.idx], next;
		uint8_t op = (uint8_t)(w >> 24);
		if (op == 0xFF) next = 0;
		else if (op == 0xFE) next = m.h.counter + 1 >= (int16_t)(uint8_t)(w >> 16) ? m.script[m.idx + 1] : m.colour;
		else next = w;
		uint8_t *c = (uint8_t *)&m.colour;
		for (int k = 0; k < 3; k++) c[k] = (uint8_t)lerp_i(c[k], (uint8_t)(next >> (8 * k)), num, den);
		Replay(r);
		FlashDraw(&m);
	}

	// sub_89D460 on a held frame: the in-between pose, then the real pose's matrices back
	static void OverlayHeld(const Rec &r, int num, int den)
	{
		const FxNode *o = (const FxNode *)r.node;
		int16_t amp1 = r.amp;
		if (o->phase == 1)
		{
			amp1 = (int16_t)(r.amp + 0x200);
			if (amp1 >= 0x1000) amp1 = 0x1000;
		}
		else if (o->phase == 3) amp1 = (int16_t)(r.amp - 0x100);
		OverlayCtx *mc = &Overlay();
		mc->wobble_amp = lerp16(r.amp, amp1, num, den);
		mc->wobble_phase = (int16_t)((uint16_t)(r.phase + 0xA0 * num / den) & 0xFFF);
		Replay(r);
		uint8_t *entity = mc->entity;
		CopyWorld(mc, entity);
		pose_midpoint(entity + 0x60, entity + 0x6C, num, den);
		ComposeAffineTransform(&Camera(), &mc->world, &mc->view);
		if (!(entity[0] & 0x20)) *mc->frame_cursor = DrawShadow(entity, RenderOT(0x4040), 0x10, *mc->frame_cursor);
		BuildLightMatrix(mc);
		DrawOverlayObject(*(const uint8_t *const *)(entity + 0x64), RenderOT(0x44), 4, mc);
		const uint8_t *weapon = *(const uint8_t *const *)(entity + 0x78);
		if (weapon) DrawOverlayObject(*(const uint8_t *const *)(weapon + 4), RenderOT(0x44), 4, mc);
		BuildBoneMatricesFromPose(entity + 0x60);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		OverlayScope overlay;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			switch (r.fn)
			{
			case ORIG_Splash:
			case ORIG_Droplet:
			case ORIG_MistB:
			{
				Particle a;
				memcpy(&a, r.node, sizeof(a));
				Particle m = a;
				if (a.h.phase == 1)
				{
					Particle b = a;
					if (r.fn == ORIG_Splash) SplashMove(&b);
					else if (r.fn == ORIG_Droplet) DropletFall(&b);
					else MistBMove(&b);
					for (int k = 0; k < 3; k++) m.h.pos[k] = lerp16(a.h.pos[k], b.h.pos[k], num, den);
				}
				Replay(r);
				DrawSprite(&m, (int16_t)m.scale[0]);
				break;
			}
			case ORIG_MistA:
			{
				Particle a;
				memcpy(&a, r.node, sizeof(a));
				Replay(r);
				DrawSprite(&a, (int16_t)a.scale[0]);
				break;
			}
			case ORIG_Bubble:
			{
				Particle a;
				memcpy(&a, r.node, sizeof(a));
				Replay(r);
				DrawOrientedSprite(&a);
				break;
			}
			case ORIG_Pool: PoolHeld(r, num, den); break;
			case ORIG_Blob: BlobHeld(r, num, den); break;
			case ORIG_Flash: FlashHeld(r, num, den); break;
			case ORIG_Overlay: OverlayHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag022_held()
	{
		mark_held(bio022::ORIG_Pool);
		mark_held(bio022::ORIG_Blob);
		mark_held(bio022::ORIG_Splash);
		mark_held(bio022::ORIG_Bubble);
		mark_held(bio022::ORIG_Droplet);
		mark_held(bio022::ORIG_MistA);
		mark_held(bio022::ORIG_MistB);
		mark_held(bio022::ORIG_Flash);
		mark_held(bio022::ORIG_Overlay);
		register_module_held(22, bio022::HeldReady, bio022::HeldFrame);
	}
}
