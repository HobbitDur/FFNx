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

// 30 fps part of mag113_bad_breath.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag113_bad_breath.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Bad Breath task draws THEN updates. A real tick records every draw in order with the GTE
// registers and the Field_Alloc scratch it started from (draw headers and the overlay's work
// block keep the scratch bytes where the code does not write). A held frame replays the log in
// the same order:
//   Particle - position half way along its next step (velocity + acceleration); the flipbook
//              frame, scale and roll hold; a particle whose task ends after this draw holds.
//   Overlay  - the target at its in-between pose (pose_midpoint), sway amplitude (sin of the step)
//              and wave phase half way to the next step's (the last step holds); the entity
//              matrix, tint and shadow are the engine's (hold). Vertices go to a private vertex
//              buffer, packets to a private copy of the frame arena; the entity-shadow
//              temporaries are put back.
//   Cloud    - the prim-model layout through prim::play_held: every object's exact in-between
//              record (position, rotation, scale, colour, fade, vertex-frame blend); blended
//              vertex frames go to a private buffer. The anchor (the target's effect bone 0xF1)
//              is read at the target's in-between pose (pose_midpoint; the real pose's bone
//              matrices are rebuilt afterwards, as the overlay does); its height holds.
// Spawns (random) stay at 15 Hz. No Bad Breath task has a draw-only mode. The module packet arena
// alternates per tick: the held packets start where the tick's first draw started.

namespace ff8fx
{
namespace badbreath113
{
	static const uint32_t SCRATCH = 0x400;
	enum : uint32_t { REC_PARTICLE = 1, REC_OVERLAY = 2, REC_PLAY = 3 };

	struct Rec
	{
		uint32_t kind;
		ParticleNode node;        // particle: as drawn
		uint8_t *entity;          // overlay, play: the target
		int32_t step;             // overlay: e (0..27)
		prim::Layout *layout;     // play: the live layout (prim::play_held knows what it drew)
		PrimArg arg;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0; // frame arena cursor at the tick's first overlay
	static Rec g_recs[0x60];
	static uint8_t g_held_packets[0x10000]; // module arena (0x10000 per tick)
	static uint8_t g_held_frame[0x40000];   // shadow + overlay packets
	static uint8_t g_held_verts[0x10000];   // overlay vertices (indices are 12-bit: 0x8000)
	static uint8_t g_held_morph[0x1000];    // blended vertex frames (magic buffer + 0x11B8)

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

	static void held_note_particle(const ParticleNode *p)
	{
		Rec *r = NewRec(REC_PARTICLE);
		if (!r) return;
		r->node = *p;
	}

	static void held_note_overlay(uint8_t *entity, int32_t e)
	{
		Rec *r = NewRec(REC_OVERLAY);
		if (!r) return;
		if (!g_frame_packets) g_frame_packets = FrameCursor();
		r->entity = entity;
		r->step = e;
	}

	static void held_note_play(const TargetNode *t, const PrimArg *arg)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = (prim::Layout *)t->cloud;
		r->entity = Entity(t->slot);
		r->arg = *arg;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a held frame's overlay outputs: frame arena packets (a copy of the real tick's, so bytes the
	// draw leaves alone are the real ones), the vertex buffer, the entity-shadow temporaries; all
	// put back
	struct FrameScope
	{
		uint32_t cursor, bones, words[4];
		uint8_t shadow[0x80];
		FrameScope()
		{
			bones = BoneWorkspace();
			cursor = FrameCursor();
			words[0] = var<uint32_t>(0xB8B9E4);
			words[1] = var<uint32_t>(0xB8B9EC);
			words[2] = var<uint32_t>(0x1D99C18);
			words[3] = var<uint32_t>(0x1D99C1C);
			memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
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
			memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
			var<uint32_t>(0x1D99C1C) = words[3];
			var<uint32_t>(0x1D99C18) = words[2];
			var<uint32_t>(0xB8B9EC) = words[1];
			var<uint32_t>(0xB8B9E4) = words[0];
			BoneWorkspace() = bones;
			FrameCursor() = cursor;
		}
	};

	static void ParticleHeld(const Rec &r, int num, int den)
	{
		ParticleNode m = r.node;
		if (m.counter + 1 < m.end)
		{
			ParticleNode next = m;
			ParticleStep(&next);
			for (int c = 0; c < 3; c++) m.pos[c] = lerp16(m.pos[c], next.pos[c], num, den);
		}
		ParticleDraw(&m);
	}

	static void OverlayHeld(const Rec &r, int num, int den)
	{
		const int32_t e = r.step;
		int32_t sinv = OverlaySin(e);
		int32_t phase = e << 8;
		if (e + 1 < 0x1C)
		{
			sinv = lerp_i(sinv, OverlaySin(e + 1), num, den);
			phase = lerp_i(phase, (e + 1) << 8, num, den);
		}
		pose_midpoint(r.entity + 0x60, r.entity + 0x6C, num, den);
		DrawOverlay(r.entity, sinv, phase);
	}

	// the cloud's anchor at the target's in-between pose: effect bone 0xF1 (GetEffectSpawnPosition
	// also writes the word 0x1D9770C: put back), then the real pose's bone matrices again
	static void PlayAnchorHeld(uint8_t *entity, PrimArg *arg, int num, int den)
	{
		pose_midpoint(entity + 0x60, entity + 0x6C, num, den);
		int16_t sp[3];
		const int16_t w = var<int16_t>(0x1D9770C);
		GetEffectSpawnPosition(entity, 0xF1, 0, sp);
		var<int16_t>(0x1D9770C) = w;
		BuildBoneMatricesFromPose(entity + 0x60);
		arg->pos[0] = sp[0];
		arg->pos[2] = sp[2];
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_PARTICLE: ParticleHeld(r, num, den); break;
			case REC_OVERLAY: OverlayHeld(r, num, den); break;
			case REC_PLAY:
			{
				PrimArg arg = r.arg;
				PlayAnchorHeld(r.entity, &arg, num, den);
				memcpy(g_held_morph, (const void *)r.arg.morph, sizeof(g_held_morph));
				arg.morph = (uint32_t)g_held_morph;
				prim::play_held(r.layout, PartCallback, (int)&arg, num, den);
				break;
			}
			}
		}
	}
}

	static void register_mag113_held()
	{
		mark_held(badbreath113::ORIG_ParticleTask);
		mark_held(badbreath113::ORIG_TargetTask);
		register_module_held(113, badbreath113::HeldReady, badbreath113::HeldFrame);
	}
}
