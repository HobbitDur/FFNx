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

// 30 fps part of mag004_double.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag004_double.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Double task updates THEN draws. A real tick records every draw in order: the node as
// drawn, the GTE registers and the Field_Alloc scratch the draw started with (draw headers keep
// what the scratch held). A held frame replays the log in the same order, from the same GTE
// state and scratch, each node moved half way to the state its own next update computes (the
// task's phase / motion functions run on a copy; the emitter's handshake word is predicted:
// 1 -> 2 -> 0). Random values (sprite angle and frame), flipbook frames, colours set by an event
// (flashes, pulses) and everything of a node the next tick ends or hides hold:
//   Rune      - spin, fade and shrink half way (its place on the circle is fixed).
//   Column    - spin and x / y / z growth half way; the fade half way while the flash model's
//               presence stays the same (a flash start / end holds the fade); place on the circle
//               at the in-between orbit angle.
//   Glow      - grey ramp half way (steps of 4; the 0x40 pulse start holds).
//   Mote      - exact point of the spiral at the in-between angle and radius; grey half way.
//   Big rune  - fade half way.
//   Morphing ring - blend and fade half way: its vertices are blended again at the in-between
//               blend (the exe-data vertex block is put back after the held frame).
//   Sprite    - grey ramp half way; its random angle and frame hold.
//   Flash     - height half way along its jump.
// No Double task has a draw-only mode; Double does not write the battle camera.

namespace ff8fx
{
namespace double004
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t MORPH_VERTS = MODEL_Morph + 8, MORPH_VERTS_SIZE = 0x4C * 8; // 76 vertices

	union AnyNode
	{
		ModelNode model;
		SpriteNode sprite;
		FlashNode flash;
	};

	struct Rec
	{
		uint32_t kind;
		AnyNode node;          // as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0; // module packet cursor at the start of the tick (arena base)
	static Rec g_recs[512];        // pools: 200 models + 200 sprites + 4 flashes
	static uint8_t g_held_packets[0x10000]; // module arena (0x6000 per tick)
	static uint8_t g_morph_save[MORPH_VERTS_SIZE];

	static void held_note_root(const RootNode *)
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = PacketCursor();
	}

	// a task is about to draw (hidden nodes draw nothing)
	static void held_note(uint32_t kind, const void *node)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		const FxNode *h = (const FxNode *)node;
		if (kind != REC_FLASH && (h->flags & 4)) return;
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		const size_t size = kind == REC_FLASH ? sizeof(FlashNode) : (kind == REC_GLOW || kind == REC_MOTE || kind == REC_SPRITE) ? sizeof(SpriteNode) : sizeof(ModelNode);
		memcpy(&r.node, node, size);
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

	// 12-bit angles that turn the short way (spins and orbits step by less than half a turn)
	static int16_t Angle12(int16_t a, int16_t b, int num, int den)
	{
		int32_t d = (int32_t)(b - a) & 0xFFF;
		if (d >= 0x800) d -= 0x1000;
		return (int16_t)((a + d * num / den) & 0xFFF);
	}
	static uint8_t Lerp8(uint8_t a, uint8_t b, int num, int den) { return (uint8_t)(a + ((int32_t)b - (int32_t)a) * num / den); }

	// the handshake word as the next tick's tasks see it (the emitter queue runs first)
	static FxNode *PredictedEmitter(const FxNode *live, EmitterNode *fake)
	{
		const int16_t s = ((const EmitterNode *)live)->signal;
		fake->signal = (int16_t)(s == 1 ? 2 : s == 2 ? 0 : s);
		return (FxNode *)fake;
	}

	static void BigHeld(const ModelNode &a, int num, int den)
	{
		ModelNode m = a;
		ModelNode b = a;
		BigRunePhase(&b);
		if (!(a.h.flags & 1) && !(b.h.flags & 4)) m.fade = lerp16(a.fade, b.fade, num, den);
		DrawModel(&m);
	}

	static void MorphHeld(const ModelNode &a, int num, int den)
	{
		ModelNode m = a;
		if (!(a.h.flags & 1))
		{
			ModelNode b = a;
			MorphPhase(&b);
			m.morph = lerp16(a.morph, b.morph, num, den);
			m.fade = lerp16(a.fade, b.fade, num, den);
		}
		MorphVertices((const uint8_t *)MORPH_A, (const uint8_t *)MORPH_B, (uint8_t *)MODEL_Morph, m.morph);
		DrawModel(&m);
	}

	static void SpriteHeld(const SpriteNode &a, int num, int den)
	{
		SpriteNode m = a;
		if (!(a.h.flags & 1))
		{
			SpriteNode b = a;
			SpritePhase(&b);
			for (int k = 0; k < 3; k++) m.rgb[k] = Lerp8(a.rgb[k], b.rgb[k], num, den);
		}
		DrawSpriteTurned(&m);
	}

	static void ColumnHeld(const ModelNode &a, int num, int den)
	{
		ModelNode m = a;
		if (!(a.h.flags & 1))
		{
			EmitterNode fake;
			ModelNode b = a;
			b.h.emitter = PredictedEmitter(a.h.emitter, &fake);
			ColumnPhase(&b);
			ColumnFlash(&b);
			ColumnMotion(&b);
			for (int k = 0; k < 3; k++) m.scale[k] = lerp_i(a.scale[k], b.scale[k], num, den);
			m.angle[1] = Angle12(a.angle[1], b.angle[1], num, den);
			if (a.flash_on == b.flash_on) m.fade = lerp16(a.fade, b.fade, num, den);
			// the place on the circle at the in-between orbit angle (ColumnMotion's formula)
			m.orbit = Angle12(a.orbit, b.orbit, num, den);
			memcpy(m.h.pos, m.base, 8);
			m.h.pos[0] = (int16_t)(m.h.pos[0] + mul32(ComputeSin(m.orbit), m.radius) / 0x4000);
			m.h.pos[2] = (int16_t)(m.h.pos[2] + mul32(ComputeCos(m.orbit), m.radius) / 0x4000);
		}
		ColumnDraw(&m);
	}

	static void GlowHeld(const SpriteNode &a, int num, int den)
	{
		SpriteNode m = a;
		if (!(a.h.flags & 1))
		{
			EmitterNode fake;
			SpriteNode b = a;
			b.h.emitter = PredictedEmitter(a.h.emitter, &fake);
			GlowPhase(&b);
			GlowPulse(&b);
			GlowPlace(&b);
			bool ramp = !(b.h.flags & 4);
			for (int k = 0; k < 3; k++)
			{
				const int32_t d = (int32_t)b.rgb[k] - (int32_t)a.rgb[k];
				if (d > 4 || d < -4) ramp = false;
			}
			if (ramp)
			{
				for (int k = 0; k < 3; k++) m.rgb[k] = Lerp8(a.rgb[k], b.rgb[k], num, den);
				for (int k = 0; k < 3; k++) m.h.pos[k] = lerp16(a.h.pos[k], b.h.pos[k], num, den);
			}
		}
		DrawGroundGlow(&m, SEQ_Glow, (const uint32_t *)m.rgb);
	}

	static void MoteHeld(const SpriteNode &a, int num, int den)
	{
		SpriteNode m = a;
		if (!(a.h.flags & 1))
		{
			SpriteNode b = a;
			MoteUpdate(&b);
			m.ay = Angle12(a.ay, b.ay, num, den);
			m.radius = lerp16(a.radius, b.radius, num, den);
			for (int k = 0; k < 3; k++) m.rgb[k] = Lerp8(a.rgb[k], b.rgb[k], num, den);
			MotePlace(&m);
		}
		DrawSpriteFacing(&m);
	}

	static void RuneHeld(const ModelNode &a, int num, int den)
	{
		ModelNode m = a;
		ModelNode b = a;
		RunePhase(&b);
		RuneMotion(&b);
		if (!(a.h.flags & 1) && !(b.h.flags & 4))
		{
			m.fade = lerp16(a.fade, b.fade, num, den);
			for (int k = 0; k < 3; k++) m.scale[k] = lerp_i(a.scale[k], b.scale[k], num, den);
			m.angle[1] = Angle12(a.angle[1], b.angle[1], num, den);
			for (int k = 0; k < 3; k++) m.h.pos[k] = lerp16(a.h.pos[k], b.h.pos[k], num, den);
		}
		DrawModel(&m);
	}

	static void FlashHeld(const FlashNode &a, int num, int den)
	{
		FlashNode m = a;
		if (!(a.h.flags & 1))
		{
			FlashNode b = a;
			FlashPhase(&b);
			m.h.pos[1] = lerp16(a.h.pos[1], b.h.pos[1], num, den);
		}
		FlashDraw(&m);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		memcpy(g_morph_save, (const void *)MORPH_VERTS, sizeof(g_morph_save));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Replay(r);
			switch (r.kind)
			{
			case REC_BIG: BigHeld(r.node.model, num, den); break;
			case REC_MORPH: MorphHeld(r.node.model, num, den); break;
			case REC_SPRITE: SpriteHeld(r.node.sprite, num, den); break;
			case REC_COLUMN: ColumnHeld(r.node.model, num, den); break;
			case REC_GLOW: GlowHeld(r.node.sprite, num, den); break;
			case REC_MOTE: MoteHeld(r.node.sprite, num, den); break;
			case REC_RUNE: RuneHeld(r.node.model, num, den); break;
			case REC_FLASH: FlashHeld(r.node.flash, num, den); break;
			default: break;
			}
		}
		memcpy((void *)MORPH_VERTS, g_morph_save, sizeof(g_morph_save));
	}
}

	static void register_mag004_held()
	{
		mark_held(double004::ORIG_Rune);
		mark_held(double004::ORIG_Column);
		mark_held(double004::ORIG_Glow);
		mark_held(double004::ORIG_Mote);
		mark_held(double004::ORIG_BigRune);
		mark_held(double004::ORIG_Morph);
		mark_held(double004::ORIG_Sprite);
		mark_held(double004::ORIG_Flash);
		register_module_held(4, double004::HeldReady, double004::HeldFrame);
	}
}
