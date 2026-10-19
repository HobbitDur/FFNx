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

// 30 fps part of mag001_cure.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag001_cure.cpp; shared pieces in mag_common_held.h / fx_held.h, the
// glints in fx_glint_held.h).
//
// Every Cure task updates THEN draws. A real tick records every draw in order: the node as drawn,
// the GTE registers and the Field_Alloc scratch the draw started with (sprite headers keep what
// the scratch held). A held frame replays the log in the same order, from the same GTE state and
// scratch, each part at its exact in-between state:
//   Ring    - its position is a closed-form function of the sub-step (centre + step * climb,
//             radius * (sin, cos)(step * 0x80)): drawn at step 4 * counter + 3 + 4 * num / den, the
//             exact point of the helix the next tick's sub-steps pass through; the ground glow's
//             grey ramp (0x3C - 3 * counter) half way to the next tick's. The overlay (the target
//             redrawn with the projected light) at the target's in-between pose (pose_midpoint of
//             its body and of its weapon, which animates with it), projected from the in-between
//             ring position; its shadow (sub_5088A0, a pure draw) is
//             drawn again. When the next tick ends the ring (hidden), its position and glow hold.
//   Mote / sparkle - position half way along its constant velocity; the last drawn frame (the
//             next tick hides it) holds. Flipbook frames hold.
//   Glints  - glint::held_draw (fx_glint_held.inc) at the place of the glint queue: size, colours
//             and spin half way; their position half way to the next tick's: the line anchor and
//             bounds height the emitter takes from the target's bones, computed by the effect
//             library's own functions on a copy of the emitter with the target at its next pose.
// The overlay's outputs (frame arena packets, vertex buffer, context) go to private copies and are
// put back; the target's (body and weapon) bone matrices are rebuilt from its real pose afterwards. No Cure
// task has a draw-only mode; Cure does not write the battle camera.

namespace ff8fx
{
namespace cure001
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t VERTS_SIZE = 0x6400; // overlay vertex buffer 0x2783E28..0x278A228
	enum : uint32_t { REC_RING = 1, REC_PARTICLE = 2 };

	struct Rec
	{
		uint32_t kind;
		const Particle *live;         // the node itself (the ring's glow colour byte is its address)
		Particle node;                // as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the start of the tick (arena base)
	static uint32_t g_frame_packets = 0; // frame arena cursor at the first overlay draw
	static const void *g_root = nullptr; // root node (the glints' +0x10)
	static Rec g_recs[1024];
	static uint8_t g_held_packets[0x10000]; // module arena (0x10000 per tick)
	static uint8_t g_held_frame[0x20000];   // overlay packets (frame arena)
	static uint8_t g_held_verts[VERTS_SIZE];

	static void held_note_root(const RootNode *r)
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = PacketCursor();
		g_frame_packets = 0;
		g_root = r;
	}

	static Rec *NewRec(uint32_t kind, const Particle *p)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.live = p;
		r.node = *p;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_ring(const Particle *r)
	{
		if (!NewRec(REC_RING, r)) return;
		if ((r->h.flags & 8) && !g_frame_packets) g_frame_packets = var<uint32_t>(0x1D8E054);
	}

	static void held_note_particle(const Particle *p)
	{
		if (p->h.flags & 4) return; // hidden: nothing drawn
		NewRec(REC_PARTICLE, p);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// the overlay's outputs of a held frame: frame arena packets (a copy of the real tick's, so
	// bytes the draw leaves alone are the real ones), vertex buffer, context; all put back
	struct OverlayScope
	{
		uint32_t cursor, verts;
		uint8_t ctx[sizeof(OverlayCtx)];
		OverlayScope()
		{
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
		}
	};

	// 0x8D7110 on a held frame: the target at its in-between pose, then the real pose's matrices back
	static void OverlayHeld(int num, int den)
	{
		OverlayCtx *c = &Overlay();
		uint8_t *entity = c->entity;
		UpdateMatrices(c, entity);
		const bool posed = c->slot != 0xFF; // (0xFF: the overlay advances the animation: pose held)
		uint8_t *weapon = *(uint8_t *const *)(entity + 0x78);
		// the weapon (a party member's, entity +0x78) animates with the body (AdvanceAnimationBy1 0x5094F0):
		// its in-between pose too
		if (posed) pose_midpoint(entity + 0x60, entity + 0x6C, num, den);
		if (posed && weapon) pose_midpoint(weapon, weapon + 0xC, num, den);
		ComposeAffineTransform(&Camera(), &c->world, &c->view);
		if (!(entity[0] & 0x20)) *c->frame_cursor = DrawShadow(entity, RenderOT(0x4040), 0x10, *c->frame_cursor);
		BuildProjectorMatrix(c);
		RenderModel(*(const uint8_t *const *)(entity + 0x64), RenderOT(0x44), 4, c);
		if (weapon) RenderModel(*(const uint8_t *const *)(weapon + 4), RenderOT(0x44), 4, c);
		if (posed) BuildBoneMatricesFromPose(entity + 0x60);
		if (posed && weapon) BuildBoneMatricesFromPose(weapon);
	}

	// the glints' position on the next tick (glint::held_draw's next_pos): the glint task takes it from
	// its line's emitter (Effect_CopyMidpointFromSource 0x8DC700 and Effect_GetTargetBoundsCenterWorldY
	// 0x8DCB80), which the emitter task updates first on every tick from the target's bones
	// (MAG_001_CURE_Emitter_UpdatePos 0x8DC610 / _ComputeModelBounds 0x8DC870). Here the same functions
	// run on a copy of the emitter with the target at its next pose (pose_midpoint at 1), then the real
	// pose is rebuilt and the word GetEffectSpawnPosition writes (0x1D9770C) put back; once per emitter
	// and held frame.
	inline void CopyMidpointFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC700)(n, out); } // Effect_CopyMidpointFromSource
	inline int32_t BoundsCenterY(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCB80)(n); }                  // Effect_GetTargetBoundsCenterWorldY
	struct NextAnchor { const EmitterNode *emitter; bool ok; EmitterNode next; };
	static NextAnchor g_next[8];
	static int g_nnext = 0;
	static bool NextGlintPos(const void *node, int16_t pos[3])
	{
		const EmitterNode *e = *(const EmitterNode *const *)((const uint8_t *)node + 0x14);
		if (!e) return false;
		NextAnchor *na = nullptr;
		for (int i = 0; i < g_nnext; i++) if (g_next[i].emitter == e) na = &g_next[i];
		if (!na)
		{
			if (g_nnext >= (int)(sizeof(g_next) / sizeof(g_next[0]))) return false;
			na = &g_next[g_nnext++];
			na->emitter = e;
			na->next = *e;
			uint8_t *entity = Entity(e->h.slot);
			na->ok = *(const uint32_t *)(entity + 0x64) != 0;
			if (na->ok)
			{
				const uint16_t w70c = var<uint16_t>(0x1D9770C);
				pose_midpoint(entity + 0x60, entity + 0x6C, 1, 1);
				EmitterUpdatePos(&na->next);
				ComputeModelBounds(&na->next);
				BuildBoneMatricesFromPose(entity + 0x60);
				var<uint16_t>(0x1D9770C) = w70c;
			}
		}
		if (!na->ok) return false;
		uint8_t g[0x5C];
		memcpy(g, node, sizeof(g));
		*(const EmitterNode **)(g + 0x14) = &na->next;
		int16_t mid[4];
		CopyMidpointFromSource(g, mid);
		pos[0] = mid[0];
		pos[1] = (int16_t)BoundsCenterY(g);
		pos[2] = mid[2];
		return true;
	}

	static void RingHeld(const Rec &r, int num, int den)
	{
		Particle m = r.node;
		// the next tick: its phase function first (phase 2 may end the ring: then everything holds)
		Particle b = m;
		bool ends = false;
		if (b.h.phase == 2) ends = LifetimeExpired(&b) != 0;
		if (!ends) RingPlace(&m, (uint32_t)(uint16_t)(m.h.counter << 2) + 3 + (uint32_t)(4 * num / den));
		if (m.h.flags & 8)
		{
			Overlay().offset[1] = m.h.pos[1];
			SetOverlayTexturePage(&Overlay(), m.h.pos, 0x200, 0x180, 0x140, 0xF1, 0x80, 0x80);
			OverlayHeld(num, den);
		}
		if (m.h.counter < 0x14)
		{
			const int32_t g0 = (uint8_t)(0x3C - (uint8_t)(m.h.counter * 3));
			const int32_t g1 = (uint8_t)(0x3C - (uint8_t)((m.h.counter + 1) * 3));
			const uint32_t grey = (uint8_t)(ends ? g0 : lerp_i(g0, g1, num, den));
			const uint32_t colour = grey | grey << 8 | grey << 16 | ((uint32_t)r.live & 0xFF000000u);
			DrawSprite(&m);
			DrawAdditiveGlow(&m, SEQ_Glow, &colour);
		}
		else DrawSprite(&m);
	}

	static void ParticleHeld(const Rec &r, int num, int den)
	{
		const Particle &a = r.node;
		Particle m = a;
		if (a.h.phase == 1)
		{
			Particle b = a;
			Integrate(&b);
			if (!(b.h.flags & 4))
				for (int k = 0; k < 3; k++) m.h.pos[k] = lerp16(a.h.pos[k], b.h.pos[k], num, den);
		}
		DrawSprite(&m);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		OverlayScope overlay;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Replay(r);
			if (r.kind == REC_RING) RingHeld(r, num, den);
			else ParticleHeld(r, num, den);
		}
		// the glint queue runs after the particle queue
		g_nnext = 0;
		glint::held_draw(g_root, num, den, NextGlintPos);
	}
}

	static void register_mag001_held()
	{
		mark_held(cure001::ORIG_Ring);
		mark_held(cure001::ORIG_Mote);
		mark_held(cure001::ORIG_Sparkle);
		register_module_held(1, cure001::HeldReady, cure001::HeldFrame);
	}
}
