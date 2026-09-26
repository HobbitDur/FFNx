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

// 30 fps part of mag124_flare.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag124_flare.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Flare task draws THEN updates. A real tick records every draw in order with the GTE
// registers and the Field_Alloc scratch it started from (draw headers and the silhouette's work
// block keep the scratch bytes where the code does not write). A held frame replays the log in
// the same order, with the effect camera recomputed from the current camera:
//   Burst / ring - the two prim-model layouts through prim::play_held: every object's exact
//            in-between record (position, rotation, scale, colour, fade, vertex-frame blend);
//            blended vertex frames go to a private buffer.
//   Silhouette - the target at its in-between pose (pose_midpoint), the anchor projected with the
//            current camera, ripple phase and amplitude half way to the next tick's (the weight
//            row, a 20-step table, and the radius of the last draw hold), in the colour it was
//            drawn in (a younger target task on the same enemy may reset the entity's colour
//            after it); its shadow is drawn again. Vertices go to a private copy of the bone workspace, packets to a private
//            copy of the frame arena; the real pose's bone matrices are put back.
//   Particle - direction angles half way along the angular velocity, radius half way to the next
//            tick's (collapse / burst steps), brightness half way to the next step (to 0 when it
//            fades out); a switch of sprite mode (coloured / plain) holds the brightness.
// Held without interpolation: the screen flash level and the target's colour (15 Hz steps; the
// colour is the entity's own, also used by the engine). No Flare task has a draw-only mode.

namespace ff8fx
{
namespace flare124
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_PLAY = 1, REC_SILHOUETTE = 2, REC_PARTICLE = 3 };

	struct Rec
	{
		uint32_t kind;
		prim::Layout *layout;     // play: the live layout (prim::play_held knows what it drew)
		PrimArg arg;
		uint8_t *entity;          // silhouette
		int16_t pos[4];
		int32_t radius, step;
		uint32_t colour;         // the target's colour the silhouette was drawn in
		ParticleNode part;        // particle, as drawn
		int mode;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0; // frame arena cursor at the first silhouette
	static Rec g_recs[192];
	static uint8_t g_held_packets[0x20000]; // module arena (0x18000 per tick)
	static uint8_t g_held_frame[0x20000];   // shadow + silhouette packets
	static uint8_t g_held_verts[0x10000];   // silhouette vertices (indices are 12-bit: 0x8000)
	static uint8_t g_held_morph[0x3F0];     // blended vertex frames (magic buffer + 0x2CCC .. + 0x30BC)

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_frame_packets = 0;
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

	static void held_note_play(const TargetNode *t, uint8_t *layout, const PrimArg *arg)
	{
		(void)t;
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = (prim::Layout *)layout;
		r->arg = *arg;
	}

	static void held_note_silhouette(const TargetNode *t, int32_t e)
	{
		Rec *r = NewRec(REC_SILHOUETTE);
		if (!r) return;
		if (!g_frame_packets) g_frame_packets = FrameCursor();
		r->entity = t->entity;
		memcpy(r->pos, t->pos, sizeof(r->pos));
		r->radius = t->radius;
		r->colour = *(const uint32_t *)(t->entity + 0x28);
		r->step = e;
	}

	static void held_note_particle(const ParticleNode *p, int mode)
	{
		Rec *r = NewRec(REC_PARTICLE);
		if (!r) return;
		r->part = *p;
		r->mode = mode;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// a held frame's other outputs: frame arena packets (a copy of the real tick's, so bytes the
	// draw leaves alone are the real ones), the silhouette's vertex buffer, the effect camera;
	// all put back
	struct FrameScope
	{
		uint32_t cursor, bones;
		Mat4x3 camera;
		FrameScope()
		{
			camera = EffectCamera();
			bones = BoneWorkspace();
			cursor = FrameCursor();
			if (g_frame_packets && cursor > g_frame_packets)
			{
				uint32_t used = cursor - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = (uint32_t)g_held_frame;
			BoneWorkspace() = (uint32_t)g_held_verts;
		}
		~FrameScope()
		{
			BoneWorkspace() = bones;
			FrameCursor() = cursor;
			EffectCamera() = camera;
		}
	};

	// the silhouette part of the target task at tick + num / den
	static void SilhouetteHeld(const Rec &r, int num, int den)
	{
		const int32_t e = r.step;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(r.pos);
		GteRTPS();
		int16_t sxy[2];
		uint32_t otz;
		GteReadSXY2(sxy);
		GteReadOTZ(&otz);
		sxy[0] = (int16_t)(sxy[0] >> 3);
		sxy[1] = (int16_t)(sxy[1] >> 3);
		if ((uint16_t)otz == 0) return;
		uint32_t step = (uint32_t)(e * 20) / 20u;
		if ((int32_t)step >= 0x14) step = 0x27 - step;
		// amplitude angle (e << 11) / 40 and phase e << 9, at e + num / den
		const uint32_t e2 = (uint32_t)(e * den + num);
		const int32_t amplitude = ComputeSin((int32_t)((e2 << 11) / (uint32_t)(40 * den)));
		const int32_t phase = (e << 9) + 0x200 * num / den;
		uint8_t *entity = r.entity;
		// the colour it was drawn in (a later target task on the same enemy may have changed it)
		uint32_t &colour = *(uint32_t *)(entity + 0x28);
		const uint32_t now = colour;
		colour = r.colour;
		pose_midpoint(entity + 0x60, entity + 0x6C, num, den);
		DrawSilhouette(entity, sxy[0], sxy[1], (int16_t)r.radius, (int32_t)step, amplitude, phase);
		colour = now;
	}

	static void ParticleHeld(const Rec &r, int num, int den)
	{
		const ParticleNode &a = r.part;
		// the next tick on a copy: its move, then its size / brightness steps
		ParticleNode b = a;
		ParticleMove(&b);
		const int next = ParticlePrepare(&b);
		ParticleNode m = a;
		for (int k = 0; k < 3; k++) m.ang[k] = lerp16(a.ang[k], b.ang[k], num, den);
		m.size = lerp16(a.size, b.size < 0 ? (int16_t)0 : b.size, num, den);
		if (r.mode == 4)
		{
			if (next == 4) m.alpha = lerp16(a.alpha, b.alpha, num, den);
			else if (next == -2) m.alpha = lerp16(a.alpha, 0, num, den);
		}
		ParticleDraw(&m, r.mode);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame;
		EffectCameraMatrix(&Camera(), &EffectCamera());
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Replay(r);
			switch (r.kind)
			{
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				memcpy(g_held_morph, (const void *)r.arg.morph, sizeof(g_held_morph));
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held(r.layout, PrimPart, (int)&arg, num, den);
				break;
			}
			case REC_SILHOUETTE: SilhouetteHeld(r, num, den); break;
			case REC_PARTICLE: ParticleHeld(r, num, den); break;
			}
		}
	}
}

	static void register_mag124_held()
	{
		mark_held(flare124::ORIG_TargetTask);
		mark_held(flare124::ORIG_ParticleTask);
		register_module_held(124, flare124::HeldReady, flare124::HeldFrame);
	}
}
