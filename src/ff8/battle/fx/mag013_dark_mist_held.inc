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

// 30 fps part of mag013_dark_mist.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag013_dark_mist.cpp; shared pieces in mag_common_held.h).
//
// Every Dark Mist task updates THEN draws. A real tick records every draw in order: the task's
// node as drawn, the GTE registers and the Field_Alloc scratch the draw started with, and for
// the overlay its wobble. A held frame replays the log in the same order, from the same GTE state
// and scratch, each part at its exact in-between state; the next tick's state comes from the
// task's own (pure) update run on a copy of the node (counter + 1 first, as Finish does):
//   Mist A / B, stream - position (and mist B's fade colour, the sprites' shade offset) half way
//            along the next update; the flipbook frame holds (mist B toggles a big bright and a
//            small dim frame every tick: the held frame's size and drawn colour are half way).
//   Puffs C / D - flipbooks at a fixed place: hold.
//   Swirl  - z spin and fade half way; the ring vertices half way between this tick's and the
//            next tick's (sub_8BE7C0 for the next ring turn); the texture v scroll holds. The
//            model lives in the exe's data: the held draw writes there and puts the bytes back.
//   Cloud  - scale, fade and position half way; the next position is GetEffectSpawnPosition of
//            the next anchor length (the bone-length word 0x1D9770C it writes is put back).
//   Overlay - the target at its in-between pose (pose_midpoint), wobble phase + 0xA0 / 2 and
//            amplitude half way to the next tick's; drawn into private copies of the frame arena
//            and the overlay vertex buffer, the overlay context put back (as Bio's).
// At fraction 1 every part draws exactly the next tick's state (discrete fields included); a
// sprite or prim model the next tick no longer draws (released / hidden) holds its last frame,
// the overlay is left out when the target is shown again.

namespace ff8fx
{
namespace mist013
{
	static const uint32_t SCRATCH = 0x200;
	static const uint32_t VERTS_SIZE = 0x2580; // overlay vertex buffer 0x274C3E0..0x274E960
	static const uint32_t BONE_LENGTH = 0x1D9770C; // written by GetEffectSpawnPosition

	struct Rec
	{
		uint32_t fn;
		uint8_t node[sizeof(Particle)];
		int16_t amp, phase;           // overlay: wobble of the draw
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0; // frame arena cursor at the overlay draw
	static Rec g_recs[512];
	static uint8_t g_held_packets[0x8000]; // module arena (0x4000 per tick)
	static uint8_t g_held_frame[0x20000];  // overlay packets
	static uint8_t g_held_verts[VERTS_SIZE];
	static uint8_t g_model_save[0x4000];   // swirl model bytes put back after the held draw

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
		case ORIG_Swirl:
		case ORIG_Cloud: return sizeof(ModelNode);
		case ORIG_Overlay: return sizeof(FxNode);
		default: return sizeof(Particle);
		}
	}

	static void held_note_draw(uint32_t fn, const void *node)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		memset(r.node, 0, sizeof(r.node));
		memcpy(r.node, node, NodeSize(fn));
		if (fn == ORIG_Overlay)
		{
			r.amp = Overlay().wobble_amp;
			r.phase = Overlay().wobble_phase;
			if (!g_frame_packets) g_frame_packets = var<uint32_t>(0x1D8E054);
		}
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// the overlay's outputs of a held frame (as Bio's): frame arena packets (a copy of the real
	// tick's), vertex buffer and context, all put back (the GTE control window, light matrix
	// included, is put back by HeldScope)
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

	// angle half way the short way round (12-bit angles, result in 0..0xFFF)
	static int16_t HalfAngle(int16_t a, int16_t b, int num, int den)
	{
		int32_t d = (b - a) & 0xFFF;
		if (d >= 0x800) d -= 0x1000;
		return (int16_t)((a + d * num / den) & 0xFFF);
	}

	static void LerpPos(Particle &m, const Particle &a, const Particle &b, int num, int den)
	{
		for (int k = 0; k < 3; k++) m.h.pos[k] = lerp16(a.h.pos[k], b.h.pos[k], num, den);
		m.shade = lerp16(a.shade, b.shade, num, den);
	}

	// first record of a sprite frame (sequence: +0xA frame offsets; frame: count, records)
	static const uint8_t *FrameRecord(uint32_t seq, int16_t frame)
	{
		return (const uint8_t *)seq + *(const uint16_t *)(seq + 0xA + 2 * frame) + 4;
	}

	// mist B's flipbook toggles between a big bright frame and a small dim one every tick (record
	// scale +0x10 / +0x12, record brightness byte +1: the drawn colour is header colour *
	// brightness / 0x80). A held frame draws the tick's frame scaled to the in-between size
	// (sequence header flag 2: the camera rows x / y scaled by +0xC / +0x10) with the header colour
	// giving the in-between drawn colour; the vanilla draw when the frame does not change.
	static void DrawMistBHeld(const Particle &a, Particle *p, const Particle &next, int num, int den)
	{
		if (p->h.flags & 4) return;
		const uint8_t *ra = FrameRecord(p->seq, p->frame), *rb = FrameRecord(next.seq, next.frame);
		const int32_t ax = *(const int16_t *)(ra + 0x10), ay = *(const int16_t *)(ra + 0x12), ka = ra[1];
		const int32_t bx = *(const int16_t *)(rb + 0x10), by = *(const int16_t *)(rb + 0x12), kb = rb[1];
		if (num == 0 || num >= den || (ax == bx && ay == by && ka == kb) || ax == 0 || ay == 0 || ka == 0)
		{
			DrawSpriteColoured(p);
			return;
		}
		uint8_t rgb[4] = { 0, 0, 0, p->rgb[3] };
		for (int c = 0; c < 3; c++)
		{
			const int32_t t = lerp_i((a.rgb[c] * ka) >> 7, (next.rgb[c] * kb) >> 7, num, den);
			const int32_t v = (t * 0x80 + ka - 1) / ka;
			rgb[c] = (uint8_t)(v > 0xFF ? 0xFF : v);
		}
		TransformCameraByShadowRotation(p->h.pos, 0x1000, p->shade);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = p->seq;
		*(int16_t *)(h + 4) = p->frame;
		*(int32_t *)(h + 0xC) = lerp_i(ax, bx, num, den) * 0x1000 / ax;
		*(int32_t *)(h + 0x10) = lerp_i(ay, by, num, den) * 0x1000 / ay;
		*(uint32_t *)(h + 0x1C) = *(const uint32_t *)rgb;
		*(uint16_t *)(h + 0x24) = 4 | 2;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// a sprite part: the next tick's update on a copy; at fraction 1 exactly the next tick's draw.
	// A part the next tick no longer draws (released on this tick, or hidden by the next update)
	// holds its last frame (as a prim-player layout that ended).
	static void ParticleHeld(const Rec &r, int num, int den)
	{
		Particle a;
		memcpy(&a, r.node, sizeof(a));
		Particle m = a, b = a;
		if (!(a.h.flags & 1) && a.h.phase == 1)
		{
			b.h.counter++;
			switch (r.fn)
			{
			case ORIG_MistA: MistAMove(&b); break;
			case ORIG_MistB: MistBMove(&b); break;
			case ORIG_Stream: StreamMove(&b); break;
			default: PuffPlay(&b); break;
			}
			if (b.h.flags & 4) b = a;
			else if (num >= den) m = b;
			else if (r.fn != ORIG_PuffC && r.fn != ORIG_PuffD)
			{
				LerpPos(m, a, b, num, den);
				if (r.fn == ORIG_MistB)
					for (int k = 0; k < 3; k++) m.rgb[k] = (uint8_t)lerp_i(a.rgb[k], b.rgb[k], num, den);
			}
		}
		Replay(r);
		if (r.fn == ORIG_MistB) DrawMistBHeld(a, &m, b, num, den);
		else DrawSprite(&m);
	}

	static uint32_t ModelSize(uint32_t model)
	{
		const uint8_t *m = (const uint8_t *)model;
		int32_t vb = *(const int32_t *)m / 4;
		int32_t count = *(const int32_t *)(m + vb * 4 + 0x1C);
		uint32_t size = (uint32_t)(vb * 4 + 0x20 + count * 0x24);
		uint32_t ring = 8 + (uint32_t)*(const int32_t *)(RING_Table + 4) * 8;
		return size > ring ? size : ring;
	}

	static void SwirlHeld(const Rec &r, int num, int den)
	{
		ModelNode a;
		memcpy(&a, r.node, sizeof(a));
		const bool last = (a.h.flags & 1) != 0;
		ModelNode m = a;
		const bool small = a.scale[2] == 0x1000;
		uint32_t size = ModelSize(m.model);
		if (size > sizeof(g_model_save)) size = sizeof(g_model_save);
		memcpy(g_model_save, (const void *)m.model, size);
		if (!last)
		{
			ModelNode b = a;
			b.h.counter++;
			SwirlPhase(&b);
			b.rot[2] = (int16_t)((uint16_t)(b.rot[2] - 0x20) & 0xFFF);
			b.angle = (int16_t)((uint16_t)(b.angle + 0x80) & 0xFFF);
			m.rot[2] = HalfAngle(a.rot[2], b.rot[2], num, den);
			m.fade = lerp16(a.fade, small ? b.fade_a : b.fade_b, num, den);
			// ring vertices: half way between this tick's (in the model) and the next tick's
			WarpRing(RING_Table, m.model, b.angle);
			const int32_t count = *(const int32_t *)(RING_Table + 4);
			const int16_t *va = (const int16_t *)(g_model_save + 8);
			int16_t *v = (int16_t *)(m.model + 8);
			for (int32_t k = 0; k < count && 8 + k * 8 + 4 <= (int32_t)size; k++)
			{
				v[k * 4] = lerp16(va[k * 4], v[k * 4], num, den);
				v[k * 4 + 1] = lerp16(va[k * 4 + 1], v[k * 4 + 1], num, den);
			}
			if (num >= den) ScrollV(m.model);
		}
		Replay(r);
		DrawModel(&m);
		memcpy((void *)m.model, g_model_save, size);
	}

	static void CloudHeld(const Rec &r, int num, int den)
	{
		ModelNode a;
		memcpy(&a, r.node, sizeof(a));
		const bool last = (a.h.flags & 1) != 0;
		ModelNode m = a;
		if (!last)
		{
			ModelNode b = a;
			b.h.counter++;
			if (b.h.phase == 1) CloudFade(&b);
			const int16_t length = var<int16_t>(BONE_LENGTH);
			CloudUpdate(&b, CasterEntity(&a.h));
			var<int16_t>(BONE_LENGTH) = length;
			for (int k = 0; k < 3; k++)
			{
				m.scale[k] = lerp_i(a.scale[k], b.scale[k], num, den);
				m.h.pos[k] = lerp16(a.h.pos[k], b.h.pos[k], num, den);
			}
			m.fade = lerp16(a.fade, b.fade, num, den);
		}
		Replay(r);
		DrawModel(&m);
	}

	// sub_8BF910 on a held frame: the in-between pose, then the real pose's matrices back
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
		if (o->phase == 3 && amp1 <= 0 && num >= den) return; // the target is shown again
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
			case ORIG_Swirl: SwirlHeld(r, num, den); break;
			case ORIG_Cloud: CloudHeld(r, num, den); break;
			case ORIG_Overlay: OverlayHeld(r, num, den); break;
			default: ParticleHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag013_held()
	{
		mark_held(mist013::ORIG_Swirl);
		mark_held(mist013::ORIG_Cloud);
		mark_held(mist013::ORIG_MistA);
		mark_held(mist013::ORIG_MistB);
		mark_held(mist013::ORIG_PuffC);
		mark_held(mist013::ORIG_PuffD);
		mark_held(mist013::ORIG_Stream);
		mark_held(mist013::ORIG_Overlay);
		register_module_held(13, mist013::HeldReady, mist013::HeldFrame);
	}
}
