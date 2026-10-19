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

// Effect 51: Hi-Potion (battle items Hi-Potion and Hi-Potion+: kernel.bin battle item +4 = 51; MAG_051_*).
// The module is a relocated copy of Cura's (effect 25, mag025_cura.cpp): the same code instruction
// for instruction, byte-identical data tables (sound, camera animation, sequences, prim-model layouts,
// spawn tables; the same texture file "mag024-050.tim"), its own globals (Cura's - 0xCB9B8).
// Like Cura it is a copy of the actor/heal effect library (see act_engine.h):
// the library's master / emitter framework (node layout, Effect_* helpers 0x8DC5xx, state tables,
// task bodies 0x73A380 / 0x73F110, spawn state 0x73A170) around the module's own tasks.
//
// Setup MAG_051_HIPOTION 0x81C800 (runs once, not ported; file loader 0x81C7E0 = the texture file named
// at 0x15ACD68; camera animation 0x15AA734, TIM upload) creates the root queue 0x261A048 with the
// master and three task pools: 0x26315E8 (4 x 0x58: emitters), 0x2631760 (0x40 x 0xC8: model
// players, spawners, target overlay) and 0x262B1C8 (0x1F4 x 0x84: sprites); the packet arenas are
// the magic buffer + 0 / + 0x12000 (0x2631758 / 0x263175C).
//   Master (0x81C8E0) - packet arena by tick parity (cursor 0x2619F54), bone follow, 5-state
//     table: one emitter per action (engine spawn 0x73A170 -> 0x81C9E0), next action once the
//     emitter's overlay released the master's +0x63 hold (0x81F990), then waits for the queues
//     (live task count -> +0x5E, 0x81F9C0); runs the three queues, ends when nothing is left.
//   Emitter (0x81C9E0), one per action: bone-follow anchor + model bounds (CURE_Emitter helpers),
//     sound at its first tick; state 0 (0x81CA80) spawns the glow (0x81CB10), the aura player
//     (0x81D1E0), the ring spawner (0x81DFD0), the sparkle spawner (0x81E2B0) and the light task
//     (0x81DE70); 1 (0x81E620) the second aura player and the target overlay (0x81E670); 2 / 3 two
//     more aura players (fade bases 0x800 / 0xC00 / 0xE00); 4 the particle emitter (0x81F710,
//     after tick 11); 5 applies the heal (after tick 30) and finishes.
//   Glow (0x81CB10): a sprite sequence at the anchor lifted by 0x600 and a flat ground sprite
//     (0x81CC00), 4 frames; at its start 8 swirl sparks (0x81CD70) on an eighth-turn star.
//   Swirl spark (0x81CD70): flies out along its angle (decelerating radius), falls (y velocity
//     +10 per tick), a trail mote (0x81CF80, random offset 0x80) every other tick, drawn as a
//     looping sprite + ground sprite; after tick 16 an end sequence; gone below the ground.
//   Trail mote (0x81CF80) / particle (0x81F840): a sprite sequence moving down (mote: decelerating
//     from 0x20..0x27 to 2 per tick; particle: 4..11 per tick), gone at the end of the sequence or
//     below the ground.
//   Aura players (0x81D1E0, engine task body 0x73A380): the prim-model layout 0x15AB250 at the
//     anchor, played with the module's prim-model player (0x81D420, the shared player 0x701970
//     without its pause argument: ff8fx::prim::play) and the draw callback 0x81DA30, fading from
//     its base (node +0xC4).
//   Ring spawner (0x81DFD0) / ring players (0x81E130): per tick (table 0x15ACCB4) new players of
//     the layout 0x15AAF7C at the emitter's midpoint, spinning (random speeds), until tick 18.
//   Sparkle spawner (0x81E2B0) / sparkles (0x81E3F0): rising spawner (table 0x15ACCF0), sparkles
//     at random offsets 0x200: a sprite sequence + a spinning flat sprite (0x81E490), 16 frames.
//   Particle emitter (0x81F710): rising, spawns the particles (table 0x15ACD2C) until tick 20.
//   Light task (0x81DE70, engine task body 0x73F110): ramps the shared battle light words
//     0x1D98992 + k * 0x2C (k < 4) up to 0x600, holds until tick 20, ramps them down.
//   Target overlay (0x81E670): hides the target (entity flags |= 4) and draws its battle model
//     itself for 18 ticks (0x81E840): the model twice through the GTE - camera view and a light
//     view lifted by a rising offset - every lit triangle / quad as a flat packet in the entity
//     colour (frame arena 0x1D8E054) plus, when inside a screen window, a textured copy sampling
//     the screen (module arena); then shows the target again and releases the master.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x2619F50..0x2634AB8 (file pointer 0x2619F50, packet cursor 0x2619F54, the
// overlay's first word 0x2619F58, pools, queues, arena pointers, the overlay's lit vertices
// 0x262B1E8, light block 0x2634970, vertex blend buffer pointer 0x26349A8, overlay state
// 0x26349C0..0x2634AB8); the shared battle light words 0x1D98992 + k * 0x2C.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace hipotion
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_051 = { "hipotion", 51, 0x81C7E0, 0x81F9F0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26315E8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x81C9E0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x81D070, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x2619F54;   // module packet cursor (sprites, prim models, overlay)
	static const uint32_t FRAME_CURSOR = 0x1D8E054;    // frame arena cursor (overlay flat packets, shadow)
	static const uint32_t Q_EMITTER = 0x26315E8;       // 4 x 0x58
	static const uint32_t Q_PLAYER = 0x2631760;        // 0x40 x 0xC8
	static const uint32_t Q_SPRITE = 0x262B1C8;        // 0x1F4 x 0x84
	static const uint32_t ARENA_EVEN = 0x2631758, ARENA_ODD = 0x263175C; // packet arena pointers
	static const uint32_t BLEND_BUFFER = 0x26349A8;    // -> vertex frames blended by 0x81DDC0
	static const uint32_t OVL_STATE = 0x26349C0;       // target overlay state (0xF8 bytes)
	static const uint32_t OVL_LIGHT = 0x2634970;       // its light block (+4 -> lit vertices 0x262B1E8)
	static const uint32_t OVL_FIRST = 0x2619F58;       // its first word
	static const uint32_t LIT_VERTICES = 0x262B1E8;
	static const uint32_t LIGHT_WORDS = 0x1D98992;     // shared battle light words, 4 x stride 0x2C
	static const uint32_t TAB_RINGS = 0x15ACCB4, TAB_SPARKLES = 0x15ACCF0, TAB_PARTICLES = 0x15ACD2C; // s16 per tick
	static const uint32_t LAYOUT_Aura = 0x15AB250, LAYOUT_Ring = 0x15AAF7C;
	static const uint32_t SEQ_Glow = 0x15A9F78, SEQ_Swirl = 0x15A9FEC, SEQ_SwirlEnd = 0x15AA040, SEQ_Mote = 0x15AA104;
	static const uint32_t SEQ_SparkleA = 0x15A9DCC, SEQ_SparkleA2 = 0x15AA3DC, SEQ_SparkleB = 0x15AA230, SEQ_SparkleB2 = 0x15AA588;
	static const uint32_t SOUND_HiPotion = 0x15A9DC8;

	static const uint32_t ORIG_Master = 0x81C8E0;
	static const uint32_t ORIG_Emitter = 0x81C9E0;
	static const uint32_t ORIG_Glow = 0x81CB10;
	static const uint32_t ORIG_Swirl = 0x81CD70;
	static const uint32_t ORIG_Mote = 0x81CF80;
	static const uint32_t ORIG_Aura = 0x81D1E0;        // engine task body a_73A380
	static const uint32_t ORIG_LightTask = 0x81DE70;   // engine task body a_73F110
	static const uint32_t ORIG_RingSpawner = 0x81DFD0; // engine task body a_73A380
	static const uint32_t ORIG_Ring = 0x81E130;
	static const uint32_t ORIG_SparkleSpawner = 0x81E2B0; // engine task body a_73A380
	static const uint32_t ORIG_Sparkle = 0x81E3F0;
	static const uint32_t ORIG_Overlay = 0x81E670;
	static const uint32_t ORIG_ParticleEmitter = 0x81F710; // engine task body a_73A380
	static const uint32_t ORIG_Particle = 0x81F840;
	static const uint32_t ORIG_DrawCallback = 0x81DA30;

	// engine / effect-library functions not in act::x (original addresses)
	namespace cx
	{
		inline uint32_t CopyAnchorFromSource(uint32_t node, uint32_t out) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DC6E0)(node, out); }   // Effect_CopyAnchorXZFromSource: 8 bytes of the emitter +0x30
		inline uint32_t CopyMidpointFromSource(uint32_t node, uint32_t out) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DC700)(node, out); } // 8 bytes of the emitter +0x38
		inline uint32_t BoundsBaseY(uint32_t node) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x8DCAF0)(node); }  // sub_8DCAF0: entity +0x1E + emitter +0x4A (ax)
		inline uint32_t BuildMatrixFromDirAndUp(uint32_t out, uint32_t dir, uint32_t up) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x8DDA50)(out, dir, up); }
		inline uint32_t NormalizeVec3(uint32_t in, uint32_t out) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56BD20)(in, out); } // Math_NormalizeVec3_Q12
		inline uint32_t TransformCameraByShadowRotation(uint32_t pos, uint32_t a, uint32_t b) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x571BC0)(pos, a, b); }
		inline uint32_t InitEffectSequenceFromData(uint32_t h, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x571C80)(h, ot, mode, cursor); }
		inline uint32_t BattleReadAnimation(uint32_t hdr, uint32_t cmd) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x508F90)(hdr, cmd); }
		inline uint32_t PreBattleReadAnimation(uint32_t hdr, uint32_t cmd, uint32_t anim) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x509440)(hdr, cmd, anim); }
		// software GTE
		inline uint32_t GteSetLightMatrix(uint32_t m) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE50)(m); }
		inline uint32_t GteSetBackgroundVector(uint32_t r, uint32_t g, uint32_t b) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DCF0)(r, g, b); }
		inline uint32_t GteMVMVA_LightV0Bk() { return fn<uint32_t (__cdecl *)()>(0x4607E0)(); }
		inline uint32_t GteStoreSXY012_PolyGT3(uint32_t p) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E300)(p); }
		inline uint32_t GteStoreSXY012_PolyFT3(uint32_t p) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E2E0)(p); }
		inline uint32_t GteStoreSXY012_PolyFT3_2(uint32_t p) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E320)(p); }
		inline uint32_t GteReadSXY012(uint32_t p) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E2A0)(p); }
		inline uint32_t GteLoadIR123(uint32_t v) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(v); }
		inline uint32_t GteGPF() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		inline uint32_t GteGPL() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
	}

	// common task tail (frame counter already incremented): end (release the linked task, return 2)
	// when finished (status read before the increment) and no children are alive
	static inline uint32_t task_end(uint32_t node, uint8_t status)
	{
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	static inline uint32_t RenderOT44() { return MEM<uint32_t>(0x1D8E04C) + 0x44; }

	// the colour words 0x81CB10 / 0x81CD70 / 0x81E3F0 hand to their flat sprite: three bytes stored
	// over the task's own argument slot (the node pointer), whose high byte stays
	static inline uint32_t ArgSlotColour(uint32_t node, uint8_t c)
	{
		return (node & 0xFF000000u) | ((uint32_t)c << 16) | ((uint32_t)c << 8) | c;
	}

	// x / 18 (0x38E38E39, sar 2) and x / 20 (0x66666667, sar 3)
	static inline int32_t Div18(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * 0x38E38E39) >> 32) >> 2;
		return hi + (int32_t)((uint32_t)hi >> 31);
	}
	static inline int32_t Div20(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * 0x66666667) >> 32) >> 3;
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// stack frame of the prim-model play states (0x81D370 aura, 0x81E1E0 ring), from its lowest word
	// (esp + 8 of the original): +0x00 the object flags (the callback indexes them by the record's
	// object index: both layouts have one object), +0x04 the block handed to the callback (+0
	// matrix pointer, +4 flags pointer, +8 blend buffer, +0xC UV table - never written, read only
	// under flag 0x20000000 -, +0x10 fade base word - aura only, read only under flag 0x40000000),
	// +0x18 the anchor matrix (Mat4x3)
	static const uint32_t FR_FLAGS = 0x00, FR_BLOCK = 0x04, FR_MATRIX = 0x18, FR_SIZE = 0x38;
}
}
}

#ifdef FF8_FX_HELD
#include "mag051_hipotion_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace hipotion
{
	// ------------------------------------------------------------------------------------
	// draws
	// ------------------------------------------------------------------------------------
	// 0x81CB90 (MAG_051_HIPOTION_DrawSprite): the node's sprite sequence (+0x4C, frame +0x50) at its
	// position (+0x1C) facing the camera, scale +0x54; not when hidden (+0x26 bit 2)
	static void DrawSprite(uint32_t node)
	{
		if (U8(node, 0x26) & 4)
			return;
		// 30 fps layer: see mag051_hipotion_held.inc
		FX_HELD(held_note_sprite(node);)
		const uint32_t h = x::Field_Alloc(0xB4);
		cx::TransformCameraByShadowRotation(node + 0x1C, 0x1000, (uint32_t)(int32_t)S16(node, 0x54));
		U32(h, 0) = U32(node, 0x4C);
		U16(h, 4) = U16(node, 0x50);
		U16(h, 0x24) = 0;
		MEM<uint32_t>(PACKET_CURSOR) = cx::InitEffectSequenceFromData(h, RenderOT44(), 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0xB4);
	}

	// 0x81CC00 (MAG_051_HIPOTION_DrawGroundSprite): a flat sprite on the ground under the node (x, 0, z; tilted a quarter
	// turn), sequence `seq` frame +0x50, colour word *colour (header +0x1C, mode 4)
	static void DrawGroundSprite(uint32_t node, uint32_t seq, uint32_t colour)
	{
		if (U8(node, 0x26) & 4)
			return;
		// 30 fps layer: see mag051_hipotion_held.inc
		FX_HELD(held_note_ground(node, seq, colour);)
		Mat4x3 m = {};
		x::MAG_022_sub_8DD770(P(&m));
		x::sub_8DD7E0(P(&m), 0x400);
		m.t[0] = S16(node, 0x1C);
		m.t[1] = 0;
		m.t[2] = S16(node, 0x20);
		x::ComposeAffineTransform(0x1D97778, P(&m), P(&m));
		x::GTE_SetRotMatrix_W(P(&m));
		x::GTE_SetTransVector_W(P(&m));
		const uint32_t h = x::Field_Alloc(0xB4);
		U32(h, 0) = seq;
		U16(h, 4) = U16(node, 0x50);
		U16(h, 0x24) = 4;
		U32(h, 0x1C) = U32(colour, 0);
		MEM<uint32_t>(PACKET_CURSOR) = cx::InitEffectSequenceFromData(h, RenderOT44(), 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0xB4);
	}

	// 0x81E490 (MAG_051_HIPOTION_DrawSpinSprite): a flat sprite on the ground under the node spinning about y by `angle`,
	// scaled by `scale` (4.12, all axes), sequence `seq` frame +0x50, colour word *colour
	static void DrawSpinSprite(uint32_t node, uint32_t seq, uint32_t colour, uint32_t angle, uint32_t scale)
	{
		if (U8(node, 0x26) & 4)
			return;
		// 30 fps layer: see mag051_hipotion_held.inc
		FX_HELD(held_note_spin(node, seq, colour, angle, scale);)
		const int32_t s = (int16_t)scale;
		int32_t sv[3] = { s, s, s };
		Mat4x3 m = {};
		x::MAG_022_sub_8DD770(P(&m));
		x::MAG_022_sub_8DD8A0(P(&m), (uint32_t)(int32_t)(int16_t)angle);
		x::sub_8DD7E0(P(&m), 0x400);
		x::scale3DMatrix(P(&m), P(sv));
		m.t[0] = S16(node, 0x1C);
		m.t[1] = 0;
		m.t[2] = S16(node, 0x20);
		x::ComposeAffineTransform(0x1D97778, P(&m), P(&m));
		x::GTE_SetRotMatrix_W(P(&m));
		x::GTE_SetTransVector_W(P(&m));
		const uint32_t h = x::Field_Alloc(0xB4);
		U32(h, 0) = seq;
		U16(h, 4) = U16(node, 0x50);
		U16(h, 0x24) = 4;
		U32(h, 0x1C) = U32(colour, 0);
		MEM<uint32_t>(PACKET_CURSOR) = cx::InitEffectSequenceFromData(h, RenderOT44(), 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0xB4);
	}

	// 0x81CED0 (MAG_051_HIPOTION_RandomOffset): moves the node by a random offset: the vector (0, 0, rand % n) turned by
	// two random angles (yaw 0x8DD8A0, pitch 0x8DD7E0); n = 0 counts as 1
	static void RandomOffset(uint32_t node, uint32_t n)
	{
		int32_t d = (int16_t)n;
		if ((uint16_t)n == 0)
			d = 1;
		const int32_t yaw = x::CrtRand() & 0xFFF;
		const int32_t pitch = x::CrtRand() & 0xFFF;
		Mat4x3 m = {};
		x::MAG_022_sub_8DD770(P(&m));
		x::MAG_022_sub_8DD8A0(P(&m), (uint32_t)(int32_t)(int16_t)yaw);
		x::sub_8DD7E0(P(&m), (uint32_t)(int32_t)(int16_t)pitch);
		int16_t v[4] = {};
		const int32_t r = (int32_t)x::CrtRand();
		v[2] = (int16_t)(r % d);
		int16_t out[4] = {};
		x::matrixMultiplyVector(P(&m), P(v), P(out));
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + (uint16_t)out[0]);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + (uint16_t)out[1]);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + (uint16_t)out[2]);
	}

	// ------------------------------------------------------------------------------------
	// prim-model layouts (the module's copies of the library's layout code)
	// ------------------------------------------------------------------------------------
	// 0x81D350 (MAG_051_HIPOTION_Memset32): `count` dwords of `value` at dst
	static void Memset32(uint32_t dst, uint32_t value, uint32_t count)
	{
		for (uint32_t i = 0; i < count; i++)
			U32(dst, (int32_t)(i * 4)) = value;
	}

	// 0x81D270 (MAG_051_HIPOTION_DecodePrimLayout, = Effect_DecodeModelPrimLayout 0x7016B0): clears `size` bytes of the
	// layout, +0 = data, then gives every object's scale integrator (after its position / rotation
	// integrators, by the object's channel flags) the start value 0x1000 (4.12, as 0x10000000)
	static void DecodeLayout(uint32_t data, uint32_t out, int32_t size)
	{
		Memset32(out, 0, (uint32_t)(size >> 2));
		U32(out, 0) = data;
		uint32_t c = out + 8;
		const uint32_t tab = data + U32(data, 4);
		int32_t n = S32(tab, 0);
		uint32_t p = tab + 8;
		for (; n > 0; n--)
		{
			const uint32_t off = U32(p, 0);
			p += 4;
			const uint32_t f = U32(tab + off, 4);
			if (f & 4) c += 0x24;
			else if (f & 2) c += 0x18;
			else if (f & 1) c += 0xC;
			if (f & 0x20) c += 0x24;
			else if (f & 0x10) c += 0x18;
			else if (f & 8) c += 0xC;
			if (f & 0x1C0)
			{
				U32(c, 8) = 0x10000000;
				U32(c, 4) = 0x10000000;
				U32(c, 0) = 0x10000000;
			}
			if (f & 0x100) c += 0x24;
			else if (f & 0x80) c += 0x18;
			else if (f & 0x40) c += 0xC;
			if (f & 0x800) c += 0x18;
			else if (f & 0x400) c += 0x10;
			else if (f & 0x200) c += 8;
			if (f & 0x1000) c += 0xC;
			if (f & 0x2000) c += 0x10;
		}
	}

	// 0x81DDC0 (MAG_051_HIPOTION_BlendVertexFrames, = MAG_017_sub_701390): vertex frames f0 and f1 of a model blended by t
	// (4.12) into out, through the GTE (IR0 interpolation)
	static void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out)
	{
		const int32_t w0 = 0x1000 - t;
		uint32_t a = f0 == 0 ? model + 0xC : model + (uint32_t)mul32(S32(model, 4), f0) * 8 + 0xC;
		uint32_t b = f1 == 0 ? model + 0xC : model + (uint32_t)mul32(S32(model, 4), f1) * 8 + 0xC;
		uint32_t n = U32(model, 4);
		if (n == 0)
			return;
		for (; n != 0; n--)
		{
			x::set_dword_1CA8A30((uint32_t)w0);
			cx::GteLoadIR123(a);
			cx::GteGPF();
			x::set_dword_1CA8A30((uint32_t)t);
			cx::GteLoadIR123(b);
			cx::GteGPL();
			x::GTE_StoreIR123(out);
			out += 8;
			a += 8;
			b += 8;
		}
	}

	// 0x81DCA0 (MAG_051_HIPOTION_ScrollModelV): scrolls the V coordinates of the model's textured primitives by `v`
	// (all of a primitive's V wrapped back by 0x80 when one passes 0xFF)
	static void ScrollModelV(uint32_t model, int32_t v)
	{
		const int32_t q = S32(model, 0) / 4;
		uint32_t c = model + (uint32_t)q * 4 + 0x1C;
		const uint32_t sh = (uint32_t)shl32(v, 8);
		int32_t count = S32(c, 0);
		c += 4;
		if (count <= 0)
			return;
		c += 0x10;
		for (; count != 0; count--, c += 0x24)
		{
			const uint32_t w1 = U32(c, 0), w0 = U32(c, -4), w2 = U32(c, 4);
			uint32_t e0 = (w0 & 0xFF00) + sh;             // eax
			uint32_t e1 = (w1 & 0xFF00) + sh;             // edi
			uint32_t e2 = (w2 & 0xFF00) + sh;             // [esp+0x18]
			uint32_t e3 = ((w2 >> 24) << 8) + sh;          // edx (byte 3 of w2 as V)
			if (e0 > 0xFF00 || e1 > 0xFF00 || e2 > 0xFF00 || e3 > 0xFF00)
			{
				e0 -= 0x8000;
				e1 -= 0x8000;
				e2 -= 0x8000;
				e3 -= 0x8000;
			}
			const uint32_t n0 = (w0 & 0xFFFF00FF) | (e0 & 0xFF00);
			const uint32_t n1 = (w1 & 0xFFFF00FF) | (e1 & 0xFF00);
			const uint32_t n2 = (w2 & 0x00FF00FF) | (e2 & 0xFF00) | ((e3 & 0xFFFFFF00) << 16);
			U32(c, 0) = n1;
			U32(c, -4) = n0;
			U32(c, 4) = n2;
		}
	}

	// 0x81DA30 (MAG_051_HIPOTION_PrimDrawCallback): prim-model player draw callback (layout l, record r, block b of the play
	// state) - picks the object's vertex frame (blended between two frames by 0x81DDC0), builds its
	// matrix (record rotation ZYX, record position through the block matrix: object flag 0x10000000
	// = position only, else composed), the record scale, the fade (flag 0x40000000: from the
	// block's fade base), the V scroll (flag 0x20000000), draws it into the module arena
	static void __cdecl DrawCallback(prim::Layout *lp, prim::Record *rp, int arg)
	{
		const uint32_t l = P(lp), rec = P(rp), blk = (uint32_t)arg;
		if (((uint32_t)(int32_t)S16(rec, 0x1C) | U32(rec, 0x18)) == 0)
			return; // zero scale: nothing drawn
		if (S16(rec, 0x24) >= 0x1000 && U32(rec, 0x20) == 0)
			return; // full fade to black: nothing drawn
		const uint32_t h = x::Field_Alloc(0x58);
		const int32_t object = S16(rec, 2);
		const uint32_t data = U32(l, 0);
		const uint32_t model = data + U32(data, object * 4 + 8);
		U32(h, 0) = model;
		const int16_t f1 = S16(rec, 0x2A);
		const int16_t f0 = S16(rec, 0x28);
		auto frame_ptr = [model](int16_t f) -> uint32_t {
			if (f == 0)
				return model + 0xC;
			return model + (uint32_t)mul32(S32(model, 4), (int32_t)f) * 8 + 0xC;
		};
		if (f0 == f1)
			U32(h, 4) = frame_ptr(f0);
		else
		{
			const int16_t t = S16(rec, 0x26);
			if (t == 0)
				U32(h, 4) = frame_ptr(f0);
			else if (t == 0x1000)
				U32(h, 4) = frame_ptr(f1);
			else
			{
				BlendVertexFrames(model, f0, f1, t, U32(blk, 8));
				U32(h, 4) = U32(blk, 8);
			}
		}
		Mat4x3 mo = {};
		x::ComposeZYXRotationMatrix(rec + 0x10, P(&mo));
		mo.t[0] = S16(rec, 8);
		mo.t[1] = S16(rec, 0xA);
		mo.t[2] = S16(rec, 0xC);
		const int32_t index = S16(rec, 0);
		const uint32_t m = U32(blk, 0);
		if (U32(U32(blk, 4), index * 4) & 0x10000000)
		{
			x::TransformVectorBy3x3Matrix(m, P(mo.t), P(mo.t));
			mo.t[0] = add32(mo.t[0], S32(m, 0x14));
			mo.t[1] = add32(mo.t[1], S32(m, 0x18));
			mo.t[2] = add32(mo.t[2], S32(m, 0x1C));
		}
		else
			x::ComposeAffineTransform(m, P(&mo), P(&mo));
		if (U32(rec, 0x18) != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			int32_t sv[3] = { S16(rec, 0x18), S16(rec, 0x1A), S16(rec, 0x1C) };
			x::scale3DMatrix(P(&mo), P(sv));
		}
		x::GTE_SetRotMatrix_W(P(&mo));
		x::GTE_SetTransVector_W(P(&mo));
		const uint32_t fp = U32(blk, 4) + (uint32_t)(index * 4);
		U32(h, 0x1C) = (U32(fp, 0) & 0xDFFF) | 0x2000;
		const int32_t a = S16(rec, 0x24);
		S32(h, 0xC) = a;
		if (U32(fp, 0) & 0x40000000)
		{
			const int32_t base = S16(blk, 0x10);
			S32(h, 0xC) = mul32(base, 0x1000 - a) / 4096 + a;
		}
		if (S32(h, 0xC) != 0)
		{
			U32(h, 0x1C) |= 0xC0;
			U32(h, 8) = U32(rec, 0x20);
		}
		if (U32(fp, 0) & 0x20000000)
			ScrollModelV(U32(h, 0), S16(U32(blk, 0xC), index * 2));
		MEM<uint32_t>(PACKET_CURSOR) = x::Effect_RenderPrimModel(h, RenderOT44(), 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x58);
	}

	// ------------------------------------------------------------------------------------
	// target overlay (the module's copy of Cure's overlay renderer)
	// ------------------------------------------------------------------------------------
	// 0x81F530 (MAG_051_HIPOTION_Overlay_Init): overlay state `st` for `entity`: +0 first word, +4 light block
	// (+4 lit vertex buffer, +0x1C colour 0x80, +0x18 0x140, +0x20 -1), +8 entity, +0xC / +0x10 the
	// frame arena / module packet cursors, colour +0xC4 = 0x80, +0xC8 0x2000, +0xD2 = 1 (offset
	// added as is), +0xDA animation slot (0xFF = the overlay reads the animation itself)
	static void OverlayInit(uint32_t st, uint32_t first, uint32_t light, uint32_t lit, uint32_t entity, uint32_t slot)
	{
		U32(st, 8) = entity;
		U32(st, 0) = first;
		U16(st, 0xDA) = (uint16_t)slot;
		U32(st, 4) = light;
		U32(st, 0xC) = FRAME_CURSOR;
		U32(st, 0x10) = PACKET_CURSOR;
		U16(st, 0xB4) = 0;
		U16(st, 0xB6) = 0;
		U16(st, 0xB8) = 0;
		U8(st, 0xC4) = 0x80;
		U8(st, 0xC5) = 0x80;
		U8(st, 0xC6) = 0x80;
		U32(st, 0xC8) = 0x2000;
		U16(st, 0xD2) = 1;
		U16(st, 0xCE) = 0;
		U16(st, 0xF2) = 0;
		U16(st, 0xA4) = 0;
		U16(st, 0xA6) = 0;
		U16(st, 0xA8) = 0;
		U16(st, 0xAC) = 0;
		U16(st, 0xAE) = 0;
		U16(st, 0xB0) = 0;
		U16(st, 0xBC) = 0;
		U16(st, 0xBE) = 0;
		U16(st, 0xC0) = 0;
		U8(light, 0x1C) = 0x80;
		U8(light, 0x1D) = 0x80;
		U8(light, 0x1E) = 0x80;
		U32(light, 4) = lit;
		U16(light, 0x14) = 0;
		U16(light, 0x16) = 0;
		U16(light, 0x18) = 0x140;
		U16(light, 0x1A) = 0;
		U32(light, 0x20) = 0xFFFFFFFF;
		U16(light, 0x24) = 0;
	}

	// 0x81E780 (MAG_051_HIPOTION_Overlay_SetParams): overlay parameters: light position +0xDC (3 words), the screen texture
	// (tpage +0xEC from x / y, clut +0xEE), window size +0xE4 / +0xE6, texture offset +0xE8 / +0xEA
	static void OverlayParams(uint32_t st, uint32_t pos, int32_t tx, int32_t ty, int32_t cx_, int32_t cy, int32_t w, int32_t hgt)
	{
		U16(st, 0xDC) = U16(pos, 0);
		U16(st, 0xDE) = U16(pos, 2);
		U16(st, 0xE0) = U16(pos, 4);
		const int32_t y = (int16_t)ty;
		uint32_t tp = ((uint32_t)tx & 0x3C0) | 0x2800;
		tp = (uint32_t)(uint16_t)((int16_t)(uint16_t)tp >> 6);
		tp |= (uint32_t)(y >> 4) & 0x10;
		tp |= ((uint32_t)y & 0x200) << 2;
		U16(st, 0xEC) = (uint16_t)tp;
		const uint32_t clut = (((uint32_t)(cx_ >> 4)) & 0x3F) | (uint32_t)shl32(cy, 6);
		U16(st, 0xEE) = (uint16_t)clut;
		U16(st, 0xE4) = (uint16_t)w;
		U16(st, 0xEA) = (uint16_t)(ty & 0xFF);
		U16(st, 0xE8) = (uint16_t)(((uint32_t)tx - ((uint32_t)tx & 0xFFFFFFC0u)) << 1);
		U16(st, 0xE6) = (uint16_t)hgt;
	}

	// 0x81E930 (MAG_051_HIPOTION_Overlay_SetMatrices): the entity's matrix (+0x40) into +0x14 and +0x54; +0x54 moved by the
	// offset +0xB4 (mode +0xD2: 0 = turned by the matrix, 1 = as is)
	static void OverlayMatrices(uint32_t st, uint32_t entity)
	{
		memcpy((void *)(st + 0x14), (const void *)(entity + 0x40), 0x20);
		memcpy((void *)(st + 0x54), (const void *)(entity + 0x40), 0x20);
		const int32_t mode = S16(st, 0xD2);
		if (mode == 0)
		{
			int16_t out[4] = {};
			x::matrixMultiplyVector(st + 0x54, st + 0xB4, P(out));
			S32(st, 0x68) = add32(S32(st, 0x68), out[0]);
			S32(st, 0x6C) = add32(S32(st, 0x6C), out[1]);
			S32(st, 0x70) = add32(S32(st, 0x70), out[2]);
		}
		else if (mode == 1)
		{
			S32(st, 0x68) = add32(S32(st, 0x68), S16(st, 0xB4));
			S32(st, 0x6C) = add32(S32(st, 0x6C), S16(st, 0xB6));
			S32(st, 0x70) = add32(S32(st, 0x70), S16(st, 0xB8));
		}
	}

	// 0x81E9F0 (MAG_051_HIPOTION_Overlay_LightView): the light view: rotation turning the direction from the light position
	// (+0xDC) to the lifted model (+0x68) onto z (0x8DDA50), composed with the model's rotation
	// into +0x74 (by columns), translation = light rotation * (model - lifted model) + (0, 0, +0xC8)
	// into +0x88
	static void OverlayLight(uint32_t st)
	{
		const uint32_t L = x::Field_Alloc(0x70);
		const uint16_t w68 = U16(st, 0x68), w6c = U16(st, 0x6C), w70 = U16(st, 0x70);
		U16(L, 0x68) = w68;
		U16(L, 0x6A) = w6c;
		U16(L, 0x6C) = w70;
		const int32_t dx = (int16_t)w68 - S16(st, 0xDC);
		const int32_t dy = (int16_t)w6c - S16(st, 0xDE);
		const int32_t dz = (int16_t)w70 - S16(st, 0xE0);
		S32(L, 4) = dy;
		S32(L, 8) = dz;
		S32(L, 0) = dx;
		const int32_t sq = add32(add32(mul32(dz, dz), mul32(dy, dy)), mul32(dx, dx));
		S32(L, 0x60) = sq;
		U32(L, 0x64) = x::Sqrt((uint32_t)sq);
		U16(L, 0x12) = 0x1000;
		U16(L, 0x10) = 0;
		U16(L, 0x14) = 0;
		cx::NormalizeVec3(L, L + 0x18);
		cx::BuildMatrixFromDirAndUp(L + 0x20, L + 0x18, L + 0x10);
		x::UnpackRotationMatrix(L + 0x20, L + 0x40);
		U32(L, 0x5C) = U32(st, 0xC8);
		memcpy((void *)(L + 0x20), (const void *)(st + 0x14), 0x20);
		U32(L, 0x54) = 0;
		U32(L, 0x58) = 0;
		S32(L, 0x38) = sub32(S32(st, 0x2C), S32(st, 0x6C));
		S32(L, 0x34) = sub32(S32(st, 0x28), S32(st, 0x68));
		S32(L, 0x3C) = sub32(S32(st, 0x30), S32(st, 0x70));
		x::GTE_SetRotMatrix(L + 0x40);
		x::GTE_LoadIRFromMatrixColumn(L + 0x20);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(st + 0x74);
		x::GTE_LoadIRFromMatrixColumn(L + 0x22);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(st + 0x76);
		x::GTE_LoadIRFromMatrixColumn(L + 0x24);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(st + 0x78);
		x::GTE_SetTransVector(L + 0x40);
		x::GTE_LoadV0FromDwords(L + 0x34);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(st + 0x88);
		x::Field_Free(0x70);
	}

	// 0x81EB60 (MAG_051_HIPOTION_Overlay_RenderModel): draws one model part (`part`: +0 bone matrices - 0x10, +4 object
	// table) for the overlay state `st`: per visible object (entity +0x7C mask) every vertex lit
	// through its bone (GTE light matrix + background = the bone matrix) into the lit buffer, then
	// every front-facing triangle / quad projected with the camera view (+0x34): a flat packet in
	// the entity colour (frame arena, depth keys = the three / four SZ), and projected again with
	// the light view (+0x74): when front-facing (or +0xF4 == 0) and inside the screen window, a
	// gouraud packet textured with the screen at those coordinates (+ the window offset) in the
	// overlay colour (module arena)
	static void RenderModel(uint32_t part, uint32_t ot, uint32_t st)
	{
		uint32_t frame_cur = U32(U32(st, 0xC), 0);
		uint32_t mod_cur = U32(U32(st, 0x10), 0);
		const uint32_t lit = U32(U32(st, 4), 4);
		const uint32_t bones = U32(part, 0) + 0x10;
		const uint32_t objtab = U32(part, 4);
		const int32_t nobj = S32(objtab, 0);
		const uint32_t entity = U32(st, 8);
		const uint32_t h = x::Field_Alloc(0xD0);
		U16(h, 0xCE) = U16(st, 0xF4);
		U32(h, 0xB8) = U32(entity, 0x7C);
		U16(h, 0xBC) = U16(st, 0xEC);
		U16(h, 0xBE) = U16(st, 0xEE);
		const int32_t hw = S16(st, 0xE4) / 2;
		const int32_t hh = S16(st, 0xE6) / 2;
		U16(h, 0xC2) = (uint16_t)(hw + 0x9F);
		U16(h, 0xC0) = (uint16_t)(0xA0 - hw);
		U16(h, 0xC6) = (uint16_t)(hh + 0x77);
		U16(h, 0xCA) = (uint16_t)(U16(st, 0xEA) + (uint16_t)hh - 0x78);
		U16(h, 0xC4) = (uint16_t)(0x78 - hh);
		U16(h, 0xC8) = (uint16_t)(U16(st, 0xE8) + (uint16_t)hw - 0xA0);
		const uint32_t ovl = U32(st, 0xC4) & 0xFFFFFF;
		U32(h, 0xA8) = ovl | 0x36000000;
		U32(h, 0xAC) = ovl | 0x3E000000;
		const uint32_t tint = U32(entity, 0x28) & 0xFFFFFF;
		U32(h, 0xB0) = tint | 0x24000000;
		U32(h, 0xB4) = tint | 0x2C000000;
		memcpy((void *)h, (const void *)(st + 0x34), 0x20);
		memcpy((void *)(h + 0x20), (const void *)(st + 0x74), 0x20);
		U16(h, 0xA4) = U16(st, 0xCE);
		U16(h, 0xA6) = U16(st, 0xCC);
		x::GTE_SetRotMatrix(h);
		x::GTE_SetTransVector(h);
		auto vtx = [lit](uint32_t i) -> uint32_t { return lit + 4 + (i << 4); };
		uint32_t offp = objtab + 4;
		for (int32_t k = 0; k < nobj; k++)
		{
			uint32_t litc = lit;
			uint32_t obj = U32(part, 4) + U32(offp, 0);
			offp += 4;
			if (((U32(h, 0xB8) >> (k & 31)) & 1) == 0)
				continue;
			// lighting: every vertex through its bone
			const int32_t groups = S16(obj, 0);
			obj += 2;
			for (int32_t g = groups; g > 0; g--)
			{
				const int32_t bone = S16(obj, 0);
				obj += 2;
				const uint32_t m = (uint32_t)mul32(bone, 0x30) + bones + 0x10;
				cx::GteSetLightMatrix(m);
				cx::GteSetBackgroundVector(U32(m, 0x14), U32(m, 0x18), U32(m, 0x1C));
				const int32_t nv = S16(obj, 0);
				obj += 2;
				if (nv <= 0)
					continue;
				uint32_t out = litc + 4;
				litc += (uint32_t)shl32(nv, 4);
				for (int32_t v = nv; v != 0; v--)
				{
					U16(h, 0x70) = U16(obj, 0);
					U16(h, 0x72) = U16(obj, 2);
					U16(h, 0x74) = U16(obj, 4);
					obj += 6;
					x::GTE_LoadV0(h + 0x70);
					cx::GteMVMVA_LightV0Bk();
					x::GTE_StoreIR123(out);
					out += 0x10;
				}
			}
			obj = (obj + 3) & ~3u;
			uint32_t ft = frame_cur;
			uint32_t pk = mod_cur;
			const int32_t ntri = S16(obj, 0);
			const int32_t nquad = S16(obj, 2);
			uint32_t rec = obj + 0xC;
			// triangles (16-byte records: 3 vertex indices, +6 uv2, +8 uv0 + clut, +0xC uv1 + tpage)
			for (int32_t c = ntri; c > 0; c--, rec += 0x10)
			{
				U32(h, 0x9C) = U16(rec, 4) & 0xFFF;
				U32(h, 0x98) = U16(rec, 2) & 0xFFF;
				U32(h, 0x94) = U16(rec, 0) & 0xFFF;
				x::GTE_LoadV012(vtx(U32(h, 0x94)), vtx(U32(h, 0x98)), vtx(U32(h, 0x9C)));
				x::GTE_RTPT();
				const uint32_t sz1 = MEM<uint32_t>(0x1CA8A54), sz2 = MEM<uint32_t>(0x1CA8A58), sz3 = MEM<uint32_t>(0x1CA8A5C);
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(h + 0x8C);
				if (S32(h, 0x8C) <= 0)
					continue;
				x::GTE_AVSZ3();
				x::GTE_ReadOTZ(h + 0x90);
				U32(h, 0x90) = U32(h, 0x90) >> 2;
				cx::GteStoreSXY012_PolyGT3(pk);
				cx::GteStoreSXY012_PolyFT3(ft);
				x::GTE_SetRotMatrix(h + 0x20);
				x::GTE_SetTransVector(h + 0x20);
				x::GTE_LoadV012(vtx(U32(h, 0x94)), vtx(U32(h, 0x98)), vtx(U32(h, 0x9C)));
				x::GTE_RTPT();
				U16(h, 0xCC) = 0;
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(h + 0x8C);
				if (U16(h, 0xCE) == 1 && S32(h, 0x8C) <= 0)
					U16(h, 0xCC) = 1;
				if (U16(h, 0xCC) == 0)
				{
					cx::GteReadSXY012(h + 0x40);
					for (int i = 0; i < 3; i++)
					{
						const int16_t sx = S16(h, 0x40 + 4 * i), sy = S16(h, 0x42 + 4 * i);
						if (sx < S16(h, 0xC0) || sx > S16(h, 0xC2) || sy < S16(h, 0xC4) || sy > S16(h, 0xC6))
						{
							U16(h, 0xCC) = 1;
							break;
						}
						U16(h, 0x40 + 4 * i) = (uint16_t)(U16(h, 0xC8) + (uint16_t)sx);
						U16(h, 0x42 + 4 * i) = (uint16_t)(U16(h, 0x42 + 4 * i) + U16(h, 0xCA));
					}
					if (U16(h, 0xCC) == 0)
					{
						U8(pk, 0xC) = U8(h, 0x40);
						U8(pk, 0xD) = U8(h, 0x42);
						U8(pk, 0x18) = U8(h, 0x44);
						U8(pk, 0x19) = U8(h, 0x46);
						U16(pk, 0x1A) = U16(h, 0xBC);
						U8(pk, 0x24) = U8(h, 0x48);
						U8(pk, 0x25) = U8(h, 0x4A);
						U16(pk, 0xE) = U16(h, 0xBE);
						U32(pk, 0x1C) = U32(h, 0xA8);
						U32(pk, 0x10) = U32(h, 0xA8);
						U32(pk, 4) = U32(h, 0xA8);
						U32(pk, 0) = 0x9000000;
						x::SSIGPU_InsertPrimAutoDepth(ot + U32(h, 0x90) * 4, pk);
						pk += 0x28;
					}
				}
				U32(ft, 0xC) = U32(rec, 8);
				U32(ft, 0x14) = U32(rec, 0xC);
				U32(ft, 0) = 0x7000000;
				U16(ft, 0x1C) = U16(rec, 6);
				U32(ft, 4) = U32(h, 0xB0);
				if (U8(rec, 0xF) & 2)
					U8(ft, 7) |= 2;
				x::SSIGPU_InsertPrimDepthKeys(ot + U32(h, 0x90) * 4, ft, sz1, sz2, sz3, 0);
				ft += 0x20;
				x::GTE_SetRotMatrix(h);
				x::GTE_SetTransVector(h);
			}
			// quads (20-byte records: 4 vertex indices, +8 uv0 + clut, +0xC uv1 + tpage, +0x10 uv2,
			// +0x12 uv3)
			for (int32_t c = nquad; c > 0; c--, rec += 0x14)
			{
				U32(h, 0x9C) = U16(rec, 4) & 0xFFF;
				U32(h, 0x98) = U16(rec, 2) & 0xFFF;
				U32(h, 0x94) = U16(rec, 0) & 0xFFF;
				x::GTE_LoadV012(vtx(U32(h, 0x94)), vtx(U32(h, 0x98)), vtx(U32(h, 0x9C)));
				x::GTE_RTPT();
				const uint32_t sz1 = MEM<uint32_t>(0x1CA8A54), sz2 = MEM<uint32_t>(0x1CA8A58), sz3 = MEM<uint32_t>(0x1CA8A5C);
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(h + 0x8C);
				if (S32(h, 0x8C) <= 0)
					continue;
				x::GTE_StoreSXY012_PolyGT3_2(pk);
				cx::GteStoreSXY012_PolyFT3_2(ft);
				U32(h, 0xA0) = U16(rec, 6) & 0xFFF;
				x::GTE_LoadV0(vtx(U32(h, 0xA0)));
				x::GTE_RTPS();
				const uint32_t sz4 = MEM<uint32_t>(0x1CA8A5C);
				x::GTE_ReadSXY2(pk + 0x2C);
				x::GTE_ReadSXY2(ft + 0x20);
				x::GTE_AVSZ4();
				x::GTE_ReadOTZ(h + 0x90);
				U32(h, 0x90) = U32(h, 0x90) >> 2;
				x::GTE_SetRotMatrix(h + 0x20);
				x::GTE_SetTransVector(h + 0x20);
				x::GTE_LoadV012(vtx(U32(h, 0x94)), vtx(U32(h, 0x98)), vtx(U32(h, 0x9C)));
				x::GTE_RTPT();
				U16(h, 0xCC) = 0;
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(h + 0x8C);
				if (U16(h, 0xCE) == 1 && S32(h, 0x8C) <= 0)
					U16(h, 0xCC) = 1;
				if (U16(h, 0xCC) == 0)
				{
					cx::GteReadSXY012(h + 0x40);
					x::GTE_LoadV0(vtx(U32(h, 0xA0)));
					x::GTE_RTPS();
					x::GTE_ReadSXY2(h + 0x4C);
					for (int i = 0; i < 4; i++)
					{
						const int16_t sx = S16(h, 0x40 + 4 * i), sy = S16(h, 0x42 + 4 * i);
						if (sx < S16(h, 0xC0) || sx > S16(h, 0xC2) || sy < S16(h, 0xC4) || sy > S16(h, 0xC6))
						{
							U16(h, 0xCC) = 1;
							break;
						}
						U16(h, 0x40 + 4 * i) = (uint16_t)(U16(h, 0xC8) + (uint16_t)sx);
						U16(h, 0x42 + 4 * i) = (uint16_t)(U16(h, 0x42 + 4 * i) + U16(h, 0xCA));
					}
					if (U16(h, 0xCC) == 0)
					{
						U8(pk, 0xD) = U8(h, 0x42);
						U8(pk, 0xC) = U8(h, 0x40);
						U8(pk, 0x18) = U8(h, 0x44);
						U8(pk, 0x24) = U8(h, 0x48);
						U8(pk, 0x19) = U8(h, 0x46);
						U8(pk, 0x25) = U8(h, 0x4A);
						U8(pk, 0x31) = U8(h, 0x4E);
						U8(pk, 0x30) = U8(h, 0x4C);
						U16(pk, 0x1A) = U16(h, 0xBC);
						U32(pk, 0x28) = U32(h, 0xAC);
						U32(pk, 0x1C) = U32(h, 0xAC);
						U32(pk, 0x10) = U32(h, 0xAC);
						U32(pk, 4) = U32(h, 0xAC);
						U16(pk, 0xE) = U16(h, 0xBE);
						U32(pk, 0) = 0xC000000;
						x::SSIGPU_InsertPrimAutoDepth(ot + U32(h, 0x90) * 4, pk);
						pk += 0x34;
					}
				}
				U32(ft, 0xC) = U32(rec, 8);
				U32(ft, 0x14) = U32(rec, 0xC);
				U16(ft, 0x1C) = U16(rec, 0x10);
				U32(ft, 0) = 0x9000000;
				U16(ft, 0x24) = U16(rec, 0x12);
				U32(ft, 4) = U32(h, 0xB4);
				if (U8(rec, 0xF) & 2)
					U8(ft, 7) |= 2;
				x::SSIGPU_InsertPrimDepthKeys(ot + U32(h, 0x90) * 4, ft, sz1, sz2, sz3, sz4);
				ft += 0x28;
				x::GTE_SetRotMatrix(h);
				x::GTE_SetTransVector(h);
			}
			frame_cur = ft;
			mod_cur = pk;
		}
		U32(U32(st, 0xC), 0) = frame_cur;
		U32(U32(st, 0x10), 0) = mod_cur;
		x::Field_Free(0xD0);
	}

	// 0x81E840 (MAG_051_HIPOTION_Overlay_Draw): draws the overlay: the entity's matrices (0x81E930), its bone matrices
	// from the pose (animation slot +0xDA != 0xFF, else the overlay steps the animation itself),
	// camera view +0x34, the entity's shadow (unless entity flag 0x20), the light view (0x81E9F0),
	// the body (+0x64) and the second model (+0x78) if any, then the bone matrices again
	static void DrawOverlay(uint32_t st)
	{
		const uint32_t entity = U32(st, 8);
		OverlayMatrices(st, entity);
		const uint32_t anim = entity + 0x60;
		if (U16(st, 0xDA) != 0xFF)
			x::BattleModel_BuildBoneMatricesFromPose(anim);
		else
		{
			const uint32_t cmd = entity + 0x6C;
			const uint32_t r = cx::BattleReadAnimation(anim, cmd);
			// the animation word is pushed from ax: the high half is what Battle_ReadAnimation returned
			if (r != 0)
				cx::PreBattleReadAnimation(anim, cmd, (r & 0xFFFF0000u) | U16(st, 0xD8));
		}
		x::ComposeAffineTransform(0x1D97778, st + 0x14, st + 0x34);
		if ((U8(entity, 0) & 0x20) == 0)
			U32(U32(st, 0xC), 0) = x::sub_5088A0(entity, MEM<uint32_t>(0x1D8E04C) + 0x4040, 0x10, U32(U32(st, 0xC), 0));
		OverlayLight(st);
		RenderModel(U32(entity, 0x64), RenderOT44(), st);
		const uint32_t second = U32(entity, 0x78);
		if (second != 0)
			RenderModel(U32(second, 4), RenderOT44(), st);
		if (U16(st, 0xDA) != 0xFF)
			x::BattleModel_BuildBoneMatricesFromPose(anim);
	}

	// ------------------------------------------------------------------------------------
	// master and emitter
	// ------------------------------------------------------------------------------------
	// 0x81C8E0 (MAG_051_HIPOTION_Tick): MASTER task - packet arena by tick parity, bone follow, 5-state
	// table, the three queues (live task count -> node +0x5E)
	static uint32_t __cdecl Master(uint32_t a1)
	{
		g_mod = &MOD_051;
		// 30 fps layer: see mag051_hipotion_held.inc
		FX_HELD(held_note_master();)
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x81C9A0;
		states[1] = 0x81C9B0;
		states[2] = 0x81F990;
		states[3] = 0x81F9C0;
		states[4] = 0x81F9E0; // nullsub (ret)
		if (U8(node, 0x5C) & 1)
			MEM<uint32_t>(PACKET_CURSOR) = MEM<uint32_t>(ARENA_ODD);
		else
			MEM<uint32_t>(PACKET_CURSOR) = MEM<uint32_t>(ARENA_EVEN);
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = (uint16_t)x::ExecuteTaskQueue(Q_EMITTER);
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_PLAYER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_SPRITE));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x81F990 (MAG_051_HIPOTION_State2_NextActionLoop): master state - once the emitter released the
	// hold (+0x63 == 0): the next action (state back to the spawn) or, after the last one, next state
	static uint32_t __cdecl NextAction(uint32_t a1)
	{
		if (U8(a1, 0x63) == 0)
		{
			const uint8_t action = U8(a1, 0x2A);
			if ((int16_t)(int8_t)action < S16(a1, 0x58))
			{
				U8(a1, 0x2A) = (uint8_t)(action + 1);
				U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) - 1);
			}
			else
				U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x81F9C0 (MAG_051_HIPOTION_Root_State3_WaitQueues): master state - no task left (+0x5E == 0): finished
	static uint32_t __cdecl WaitQueues(uint32_t a1)
	{
		if (U16(a1, 0x5E) == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x81C9E0 (MAG_051_HIPOTION_EmitterTask): EMITTER task (one per action) - bone-follow anchor and model
	// bounds, 7-state table, sound at its first tick
	static uint32_t __cdecl Emitter(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[7];
		states[0] = 0x81CA80;
		states[1] = 0x81E620;
		states[2] = 0x81F680;
		states[3] = 0x81F6B0;
		states[4] = 0x81F6E0;
		states[5] = 0x81F940;
		states[6] = 0x81F980; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(SOUND_HiPotion, 0, 0x80);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x81CA80 (MAG_051_HIPOTION_Emitter_State0_SpawnGlow): emitter state 0 - the glow, the first aura player (fade base
	// 0), the ring and sparkle spawners and the light task
	static uint32_t __cdecl SpawnFirst(uint32_t a1)
	{
		x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_Glow, 0x84, a1);
		const uint32_t aura = x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, ORIG_Aura, 0xC8, a1);
		U16(aura, 0xC4) = 0;
		x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, ORIG_RingSpawner, 0xC8, a1);
		x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, ORIG_SparkleSpawner, 0xC8, a1);
		x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, ORIG_LightTask, 0xC8, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x81E620 (MAG_051_HIPOTION_Emitter_State1_SpawnOverlay): emitter state 1 - aura player (fade base 0x800) and the target
	// overlay
	static uint32_t __cdecl SpawnOverlay(uint32_t a1)
	{
		const uint32_t aura = x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, ORIG_Aura, 0xC8, a1);
		U16(aura, 0xC4) = 0x800;
		x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, ORIG_Overlay, 0xC8, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x81F680 / 0x81F6B0 (MAG_051_HIPOTION_Emitter_State2_SpawnAuraC00 / _88CC10): emitter states 2 / 3 - aura players (fade
	// bases 0xC00 / 0xE00)
	static uint32_t __cdecl SpawnAuraC00(uint32_t a1)
	{
		const uint32_t aura = x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, ORIG_Aura, 0xC8, a1);
		U16(aura, 0xC4) = 0xC00;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}
	static uint32_t __cdecl SpawnAuraE00(uint32_t a1)
	{
		const uint32_t aura = x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, ORIG_Aura, 0xC8, a1);
		U16(aura, 0xC4) = 0xE00;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x81F6E0 (MAG_051_HIPOTION_Emitter_State4_SpawnParticleEmitter): emitter state 4 - after tick 11 the particle emitter
	static uint32_t __cdecl SpawnParticleEmitter(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0xB)
		{
			x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, ORIG_ParticleEmitter, 0xC8, a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x81F940 (MAG_051_HIPOTION_Emitter_State5_Heal): emitter state 5 - after tick 30 the heal of the action's
	// target(s), finished
	static uint32_t __cdecl Heal(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x1E)
		{
			const int32_t action = S8(a1, 0x2A);
			const uint32_t acts = U32(U32(a1, 0xC), 4);
			x::ApplyActionResultToTarget(U32(acts + (uint32_t)(action * 20), 8));
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// ------------------------------------------------------------------------------------
	// glow, swirl sparks, trail motes
	// ------------------------------------------------------------------------------------
	// 0x81CB10 (MAG_051_HIPOTION_Glow_Tick): GLOW task - states {0x81CCD0 start, 0x81D1B0 sequence (engine
	// a_743C00), ret}, then its sprite and the ground sprite (colour 0x20)
	static uint32_t __cdecl Glow(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x81CCD0;
		states[1] = 0x81D1B0;
		states[2] = 0x81D1D0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		DrawSprite(node);
		const uint32_t colour = ArgSlotColour(node, 0x20);
		DrawGroundSprite(node, U32(node, 0x4C), P(&colour));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x81CCD0 (MAG_051_HIPOTION_Glow_State0_SpawnSwirl): glow start - anchor lifted by 0x600, sequence 0x15A9F78 (4 frames), 8
	// swirl sparks on an eighth-turn star (random start angle, + rand & 0x7F each)
	static uint32_t __cdecl GlowStart(uint32_t a1)
	{
		const uint32_t node = a1;
		cx::CopyAnchorFromSource(node, node + 0x1C);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + 0xFA00);
		U32(node, 0x4C) = SEQ_Glow;
		U16(node, 0x52) = 3;
		uint32_t angle = x::CrtRand() & 0xFFF;
		for (int i = 8; i != 0; i--)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_Swirl, 0x84, node);
			S32(t, 0x30) = 0x1000;
			S32(t, 0x34) = 0x1000;
			S32(t, 0x38) = 0x1000;
			const uint32_t r = x::CrtRand();
			U16(t, 0x42) = (uint16_t)((r & 0x7F) + angle);
			angle = (angle + 0x200) & 0xFFF;
		}
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x81CD70 (MAG_051_HIPOTION_Swirl_Tick): SWIRL SPARK task - states {0x81D0B0 start, 0x81D120 loop, 0x81D180 end
	// sequence (engine a_743C00), ret}; then the motion: radius +0x7C += +0x7E (not below 0), x / z
	// (+0x68 / +0x6C) += sin / cos(+0x42) * radius, y velocity +0x5A += 10, y += it; a trail mote
	// every other tick (not after the loop); hidden and finished below the ground; its sprite and
	// the ground sprite (colour 0x50)
	static uint32_t __cdecl Swirl(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x81D0B0;
		states[1] = 0x81D120;
		states[2] = 0x81D180;
		states[3] = 0x81D1A0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x7C) = (uint16_t)(U16(node, 0x7C) + U16(node, 0x7E));
		if (S16(node, 0x7C) < 0)
			U16(node, 0x7C) = 0;
		const int32_t s = mul32((int32_t)x::computeSin((uint32_t)(int32_t)S16(node, 0x42)), S16(node, 0x7C));
		U16(node, 0x68) = (uint16_t)(U16(node, 0x68) + (uint16_t)(s / 4096));
		const int32_t c = mul32((int32_t)x::computeCosine((uint32_t)(int32_t)S16(node, 0x42)), S16(node, 0x7C));
		U16(node, 0x5A) = (uint16_t)(U16(node, 0x5A) + 0xA);
		const uint16_t xw = U16(node, 0x68);
		U16(node, 0x6C) = (uint16_t)(U16(node, 0x6C) + (uint16_t)(c / 4096));
		U16(node, 0x20) = U16(node, 0x6C);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(node, 0x5A));
		U16(node, 0x1C) = xw;
		if ((U8(node, 0x26) & 0x10) == 0 && (U8(node, 0x24) & 1) != 0)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_Mote, 0x84, node);
			RandomOffset(t, 0x80);
			U16(t, 0x1E) = U16(node, 0x1E);
			S32(t, 0x30) = 0x1000;
			S32(t, 0x34) = 0x1000;
			S32(t, 0x38) = 0x1000;
		}
		if (S16(node, 0x1E) > 0)
			U8(node, 0x26) |= 5;
		DrawSprite(node);
		const uint32_t colour = ArgSlotColour(node, 0x50);
		DrawGroundSprite(node, U32(node, 0x4C), P(&colour));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x81D0B0 (MAG_051_HIPOTION_Swirl_State0_Start): swirl start - sequence 0x15A9FEC (2 frames, looping), start position
	// +0x68 / +0x6C = the spawn position, y velocity -0x30..0xF, radius 0x50..0x6F shrinking by
	// radius / 16 per tick; +0x78 = a random word
	static uint32_t __cdecl SwirlStart(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t r1 = x::CrtRand();
		const uint32_t w20 = U32(node, 0x20);
		U16(node, 0x78) = (uint16_t)(r1 & 0xFFF);
		const uint32_t w1c = U32(node, 0x1C);
		U32(node, 0x4C) = SEQ_Swirl;
		U32(node, 0x68) = w1c;
		U32(node, 0x6C) = w20;
		U16(node, 0x52) = 1;
		const uint32_t r2 = x::CrtRand();
		U16(node, 0x5A) = (uint16_t)((r2 & 0x3F) - 0x30);
		const uint32_t r3 = x::CrtRand();
		const int16_t radius = (int16_t)((r3 & 0x1F) + 0x50);
		U16(node, 0x7C) = (uint16_t)radius;
		U16(node, 0x7E) = (uint16_t)(-((int32_t)radius / 16));
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x81D120 (MAG_051_HIPOTION_Swirl_State1_Loop): swirl loop - the 2-frame loop (0x81D160); from tick 16 the end sequence
	// 0x15AA040 (7 frames) and no more motes (+0x26 bit 4)
	static uint32_t __cdecl SwirlLoop(uint32_t a1)
	{
		const uint32_t node = a1;
		// 0x81D160 (MAG_051_HIPOTION_FlipbookLoop): looping frame counter
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + 1);
		if (S16(node, 0x50) > S16(node, 0x52))
			U16(node, 0x50) = 0;
		if (S16(node, 0x24) >= 0x10)
		{
			U16(node, 0x26) |= 0x10;
			U32(node, 0x4C) = SEQ_SwirlEnd;
			U16(node, 0x50) = 0;
			U16(node, 0x52) = 6;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x81CF80 (MAG_051_HIPOTION_Mote_Tick): TRAIL MOTE task - states {0x81CFF0 start, 0x81D030 fall, ret}; hidden
	// and finished below the ground; its sprite
	static uint32_t __cdecl Mote(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x81CFF0;
		states[1] = 0x81D030;
		states[2] = 0x81D0A0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		if (S16(node, 0x1E) > 0)
			U8(node, 0x26) |= 5;
		DrawSprite(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x81CFF0 (MAG_051_HIPOTION_Mote_State0_Start): mote start - sequence 0x15AA104 (11 frames), fall speed 0x20..0x27,
	// slowing by 3 per tick
	static uint32_t __cdecl MoteStart(uint32_t a1)
	{
		const uint32_t node = a1;
		U32(node, 0x4C) = SEQ_Mote;
		U16(node, 0x50) = 0;
		U16(node, 0x52) = 0xA;
		const uint32_t r = x::CrtRand();
		U16(node, 0x62) = 0xFFFD;
		U16(node, 0x5A) = (uint16_t)((r & 7) + 0x20);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x81D030 (MAG_051_HIPOTION_Mote_State1_Fall): mote fall - speed += -3 (not below 2), y += speed; next frame (engine
	// a_743C20), finished after the last
	static uint32_t __cdecl MoteFall(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x5A) = (uint16_t)(U16(node, 0x5A) + U16(node, 0x62));
		if (S16(node, 0x5A) < 2)
			U16(node, 0x5A) = 2;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(node, 0x5A));
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// ------------------------------------------------------------------------------------
	// aura and ring players (prim-model layouts)
	// ------------------------------------------------------------------------------------
	// 0x81D240 (MAG_051_HIPOTION_Aura_State0_Start): aura player state 0 - scale 1.0, layout 0x15AB250 into +0x60
	static uint32_t __cdecl AuraStart(uint32_t a1)
	{
		S32(a1, 0x30) = 0x1000;
		S32(a1, 0x34) = 0x1000;
		S32(a1, 0x38) = 0x1000;
		DecodeLayout(LAYOUT_Aura, a1 + 0x60, 0x64);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// the play frame of 0x81D370 / 0x81E1E0 (see FR_*): the block's pointers into the frame
	static void FrameLinks(uint32_t fr)
	{
		U32(fr, FR_BLOCK) = fr + FR_MATRIX;
		U32(fr, FR_BLOCK + 4) = fr + FR_FLAGS;
		U32(fr, FR_BLOCK + 8) = MEM<uint32_t>(BLEND_BUFFER);
	}

	// 0x81D370 (MAG_051_HIPOTION_Aura_State1_Play): aura player state 1 - anchor (+0x1C) from the emitter, matrix =
	// camera * (scale +0x30, translation = the anchor), plays the layout (callback 0x81DA30, object
	// flags 0x40000030: fade from the base +0xC4); finished when the layout has ended
	static uint32_t __cdecl AuraPlay(uint32_t a1)
	{
		const uint32_t node = a1;
		alignas(4) uint8_t frame[FR_SIZE] = {};
		const uint32_t fr = P(frame);
		U32(fr, FR_FLAGS) = 0x40000030;
		cx::CopyAnchorFromSource(node, node + 0x1C);
		U16(fr, FR_BLOCK + 0x10) = U16(node, 0xC4);
		const uint32_t m = fr + FR_MATRIX;
		x::MAG_022_sub_8DD770(m);
		S32(m, 0x14) = S16(node, 0x1C);
		S32(m, 0x18) = S16(node, 0x1E);
		S32(m, 0x1C) = S16(node, 0x20);
		x::scale3DMatrix(m, node + 0x30);
		x::ComposeAffineTransform(0x1D97778, m, m);
		FrameLinks(fr);
		// 30 fps layer: see mag051_hipotion_held.inc
		FX_HELD(held_note_play(node, fr);)
		const int left = prim::play((prim::Layout *)(node + 0x60), DrawCallback, (int)(fr + FR_BLOCK), 0);
		FX_HELD(held_note_played(node);)
		if (left == 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x81E050 (MAG_051_HIPOTION_RingSpawner_State1_SpawnRing): ring spawner state 1 - per tick (table 0x15ACCB4) ring players with
	// random start angles (+0x50 / +0x54) and speeds (+0x58 / +0x5C: 0x20..0x5F either way); from
	// tick 18 finished
	static uint32_t __cdecl RingSpawn(uint32_t a1)
	{
		const uint32_t node = a1;
		int32_t i = 0;
		if (S16(TAB_RINGS, S16(node, 0x24) * 2) > 0)
		{
			do
			{
				const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_PLAYER, ORIG_Ring, 0xC8, node);
				S32(t, 0x30) = 0x1000;
				S32(t, 0x34) = 0x1000;
				S32(t, 0x38) = 0x1000;
				U16(t, 0x50) = (uint16_t)(x::CrtRand() & 0xFFF);
				U16(t, 0x54) = (uint16_t)(x::CrtRand() & 0xFFF);
				U16(t, 0x58) = (uint16_t)((x::CrtRand() & 0x3F) + 0x20);
				U16(t, 0x5C) = (uint16_t)((x::CrtRand() & 0x3F) + 0x20);
				if (x::CrtRand() & 1)
					U16(t, 0x58) = (uint16_t)(0x1000 - U16(t, 0x58));
				if (x::CrtRand() & 1)
					U16(t, 0x5C) = (uint16_t)(0x1000 - U16(t, 0x5C));
				i++;
			} while (i < S16(TAB_RINGS, S16(node, 0x24) * 2));
		}
		if (S16(node, 0x24) >= 0x12)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x81E030 (MAG_051_HIPOTION_RingSpawner_State0_Start): ring spawner state 0 - position = the emitter's midpoint
	static uint32_t __cdecl RingSpawnerStart(uint32_t a1)
	{
		cx::CopyMidpointFromSource(a1, a1 + 0x1C);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x81E130 (MAG_051_HIPOTION_Ring_Tick): RING PLAYER task - states {0x81E1B0 layout, 0x81E1E0 play, ret}, then
	// the spin: +0x50 += +0x58, +0x54 += +0x5C (12 bits)
	static uint32_t __cdecl Ring(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x81E1B0;
		states[1] = 0x81E1E0;
		states[2] = 0x81E290; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint16_t a50 = (uint16_t)(U16(node, 0x58) + U16(node, 0x50));
		const uint16_t a54 = (uint16_t)(U16(node, 0x5C) + U16(node, 0x54));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		U16(node, 0x50) = (uint16_t)(a50 & 0xFFF);
		U16(node, 0x54) = (uint16_t)(a54 & 0xFFF);
		return task_end(node, status);
	}

	// 0x81E1B0 (MAG_051_HIPOTION_Ring_State0_Start): ring player state 0 - layout 0x15AAF7C into +0x60
	static uint32_t __cdecl RingStart(uint32_t a1)
	{
		DecodeLayout(LAYOUT_Ring, a1 + 0x60, 0x34);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x81E1E0 (MAG_051_HIPOTION_Ring_State1_Play): ring player state 1 - position (+0x1C) = the emitter's midpoint, matrix
	// = camera * (rotation ZYX +0x50, scale +0x30, translation), plays the layout (callback
	// 0x81DA30, object flags 0x30); finished when the layout has ended
	static uint32_t __cdecl RingPlay(uint32_t a1)
	{
		const uint32_t node = a1;
		alignas(4) uint8_t frame[FR_SIZE] = {};
		const uint32_t fr = P(frame);
		U32(fr, FR_FLAGS) = 0x30;
		cx::CopyMidpointFromSource(node, node + 0x1C);
		const uint32_t m = fr + FR_MATRIX;
		x::ComposeZYXRotationMatrix(node + 0x50, m);
		S32(m, 0x14) = S16(node, 0x1C);
		S32(m, 0x18) = S16(node, 0x1E);
		S32(m, 0x1C) = S16(node, 0x20);
		x::scale3DMatrix(m, node + 0x30);
		x::ComposeAffineTransform(0x1D97778, m, m);
		FrameLinks(fr);
		// 30 fps layer: see mag051_hipotion_held.inc
		FX_HELD(held_note_play(node, fr);)
		const int left = prim::play((prim::Layout *)(node + 0x60), DrawCallback, (int)(fr + FR_BLOCK), 0);
		FX_HELD(held_note_played(node);)
		if (left == 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// ------------------------------------------------------------------------------------
	// sparkles and particles
	// ------------------------------------------------------------------------------------
	// 0x81E310 (MAG_051_HIPOTION_SparkleSpawner_State0_Start): sparkle spawner state 0 - anchor, y = the target's bounds base,
	// rising (0x100 - y) / 18 per tick
	static uint32_t __cdecl SparkleSpawnerStart(uint32_t a1)
	{
		const uint32_t node = a1;
		cx::CopyAnchorFromSource(node, node + 0x1C);
		const int16_t y = (int16_t)cx::BoundsBaseY(node);
		U16(node, 0x1E) = (uint16_t)y;
		U16(node, 0x42) = (uint16_t)Div18(0x100 - (int32_t)y);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x81E360 (MAG_051_HIPOTION_SparkleSpawner_State1_SpawnSparkle): sparkle spawner state 1 - rises; per tick (table 0x15ACCF0) sparkles at
	// random offsets 0x200 on its height; from tick 18 finished
	static uint32_t __cdecl SparkleSpawn(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(node, 0x42));
		for (int32_t i = 0; i < S16(TAB_SPARKLES, S16(node, 0x24) * 2); i++)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_Sparkle, 0x84, node);
			RandomOffset(t, 0x200);
			U16(t, 0x1E) = U16(node, 0x1E);
			S32(t, 0x30) = 0x1000;
			S32(t, 0x34) = 0x1000;
			S32(t, 0x38) = 0x1000;
		}
		if (S16(node, 0x24) >= 0x12)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x81E3F0 (MAG_051_HIPOTION_Sparkle_Tick): SPARKLE task - states {0x81E590 start, 0x81E5E0 sequence (engine
	// a_743C00), ret}; its sprite, then its second sequence (+0x70) flat on the ground, spinning
	// (+0x7A += 0x40), scale 2.0, colour 0x30
	static uint32_t __cdecl Sparkle(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x81E590;
		states[1] = 0x81E5E0;
		states[2] = 0x81E600; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		DrawSprite(node);
		const uint32_t angle = (uint32_t)(U16(node, 0x7A) + 0x40) & 0xFFF;
		const uint32_t seq2 = U32(node, 0x70);
		const uint32_t colour = ArgSlotColour(node, 0x30);
		U16(node, 0x7A) = (uint16_t)angle;
		DrawSpinSprite(node, seq2, P(&colour), angle, 0x2000);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x81E590 (MAG_051_HIPOTION_Sparkle_State0_Start): sparkle start - one of two sequence pairs, random spin angle, 16
	// frames, sprite scale -0x200
	static uint32_t __cdecl SparkleStart(uint32_t a1)
	{
		const uint32_t node = a1;
		if (x::CrtRand() & 1)
		{
			U32(node, 0x4C) = SEQ_SparkleA;
			U32(node, 0x70) = SEQ_SparkleA2;
		}
		else
		{
			U32(node, 0x4C) = SEQ_SparkleB;
			U32(node, 0x70) = SEQ_SparkleB2;
		}
		const uint32_t r = x::CrtRand();
		U16(node, 0x52) = 0xF;
		U16(node, 0x7A) = (uint16_t)(r & 0xFFF);
		U16(node, 0x54) = 0xFE00;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x81F770 (MAG_051_HIPOTION_ParticleEmitter_State0_AnchorAndRate): particle emitter state 0 - anchor,
	// y = the target's bounds base, rising (0x100 - y) / 20 per tick
	static uint32_t __cdecl ParticleEmitterStart(uint32_t a1)
	{
		const uint32_t node = a1;
		cx::CopyAnchorFromSource(node, node + 0x1C);
		const int16_t y = (int16_t)cx::BoundsBaseY(node);
		U16(node, 0x1E) = (uint16_t)y;
		U16(node, 0x42) = (uint16_t)Div20(0x100 - (int32_t)y);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x81F7C0 (MAG_051_HIPOTION_ParticleEmitter_State1_RiseAndSpawn): particle emitter state 1 - rises; per
	// tick (table 0x15ACD2C) particles 0x180 above it at random offsets 0x200; from tick 20 finished
	static uint32_t __cdecl ParticleSpawn(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(node, 0x42));
		for (int32_t i = 0; i < S16(TAB_PARTICLES, S16(node, 0x24) * 2); i++)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_SPRITE, ORIG_Particle, 0x84, node);
			U16(t, 0x1E) = (uint16_t)(U16(node, 0x1E) - 0x180);
			RandomOffset(t, 0x200);
		}
		if (S16(node, 0x24) >= 0x14)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x81F840 (MAG_051_HIPOTION_Particle_Tick): PARTICLE task - states {0x81F8B0 start, 0x81F8F0 rise,
	// ret}; hidden and finished below the ground; its sprite
	static uint32_t __cdecl Particle(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x81F8B0;
		states[1] = 0x81F8F0;
		states[2] = 0x81F920; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		if (S16(node, 0x1E) > 0)
			U8(node, 0x26) |= 5;
		DrawSprite(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x81F8B0 (MAG_051_HIPOTION_Particle_State0_Init): particle start - sequence 0x15AA104 (11 frames),
	// scale -0x400, speed 4..11
	static uint32_t __cdecl ParticleStart(uint32_t a1)
	{
		const uint32_t node = a1;
		U32(node, 0x4C) = SEQ_Mote;
		U16(node, 0x52) = 0xA;
		U16(node, 0x54) = 0xFC00;
		const uint32_t r = x::CrtRand();
		U16(node, 0x5A) = (uint16_t)((r & 7) + 4);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x81F8F0 (MAG_051_HIPOTION_Particle_State1_Rise): particle move - y += speed; next frame (engine
	// a_743C20), finished after the last
	static uint32_t __cdecl ParticleMove(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(node, 0x5A));
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// ------------------------------------------------------------------------------------
	// light task
	// ------------------------------------------------------------------------------------
	static void SetLightWords(uint16_t v)
	{
		for (int k = 0; k < 4; k++)
			U16(LIGHT_WORDS, k * 0x2C) = v;
	}

	// 0x81DF20 (MAG_051_HIPOTION_Light_State1_Up): light state 1 - +0x1C += 0xC0 up to 0x600, into the light words
	static uint32_t __cdecl LightUp(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xC0);
		if (S16(a1, 0x1C) >= 0x600)
		{
			U16(a1, 0x1C) = 0x600;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		SetLightWords(U16(a1, 0x1C));
		return 0; // void
	}

	// 0x81DF60 (MAG_051_HIPOTION_Light_State2_WaitTick20): light state 2 - hold until tick 20
	static uint32_t __cdecl LightHold(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x14)
		{
			U16(a1, 0x1E) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x81DF80 (MAG_051_HIPOTION_Light_State3_Down): light state 3 - +0x1C -= 0xC0 down to 0 (finished), into the light words
	static uint32_t __cdecl LightDown(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xFF40);
		if (S16(a1, 0x1C) <= 0)
		{
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		SetLightWords(U16(a1, 0x1C));
		return 0; // void
	}

	// ------------------------------------------------------------------------------------
	// target overlay task
	// ------------------------------------------------------------------------------------
	// 0x81E670 (MAG_051_HIPOTION_Overlay_Tick): OVERLAY task - states {0x81F480 start, 0x81F620 rise, ret};
	// while active (+0x26 bit 3): light position = anchor + 1024 * (sin, cos) of the camera's yaw
	// (x / z) and the rising y (+0x1E), which also lifts the light-view model copy; draws the
	// overlay
	static uint32_t __cdecl Overlay(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x81F480;
		states[1] = 0x81F620;
		states[2] = 0x81F670; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		if (U8(node, 0x26) & 8)
		{
			alignas(4) uint8_t anchor[8] = {};
			cx::CopyAnchorFromSource(node, P(anchor));
			const uint32_t yaw = x::sub_8DD7B0(0x1D97778);
			U16(node, 0x52) = (uint16_t)yaw;
			const int32_t s = shl32((int32_t)x::computeSin((uint32_t)(int32_t)(int16_t)yaw), 0xA);
			U16(node, 0x1C) = (uint16_t)(s / 4096 + S32(P(anchor), 0));
			const int32_t c = shl32((int32_t)x::computeCosine((uint32_t)(int32_t)S16(node, 0x52)), 0xA);
			U16(node, 0x20) = (uint16_t)(c / 4096 + S32(P(anchor), 4));
			U16(OVL_STATE, 0xB6) = U16(node, 0x1E);
			OverlayParams(OVL_STATE, node + 0x1C, 0x280, 0x1C0, 0x140, 0xF4, 0x80, 0x40);
			// 30 fps layer: see mag051_hipotion_held.inc
			FX_HELD(held_note_overlay(node);)
			DrawOverlay(OVL_STATE);
			FX_HELD(held_note_overlay_end();)
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x81F480 (MAG_051_HIPOTION_Overlay_State0_Start): overlay start - anchor lifted by 0x600, rising (0x100 - y) / 18 per
	// tick; the overlay state for the target (0x81F530), the target hidden (entity flags |= 4),
	// overlay colour 0x40
	static uint32_t __cdecl OverlayStart(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t entity = U8(node, 0x2D) * 0x9Cu + 0x1D972C0;
		cx::CopyAnchorFromSource(node, node + 0x1C);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + 0xFA00);
		U16(node, 0x42) = (uint16_t)Div18(0x100 - (int32_t)S16(node, 0x1E));
		OverlayInit(OVL_STATE, OVL_FIRST, OVL_LIGHT, LIT_VERTICES, entity, U8(node, 0x2D));
		U8(node, 0x26) |= 8;
		U8(entity, 0) |= 4;
		U8(OVL_STATE, 0xC4) = 0x40;
		U8(OVL_STATE, 0xC5) = 0x40;
		U8(OVL_STATE, 0xC6) = 0x40;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x81F620 (MAG_051_HIPOTION_Overlay_State1_Rise): overlay rise - y += +0x42; from tick 18 the target is shown again, the
	// master released (root +0x63 = 0), inactive and finished
	static uint32_t __cdecl OverlayRise(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + U16(node, 0x42));
		if (S16(node, 0x24) >= 0x12)
		{
			const uint32_t entity = U8(node, 0x2D) * 0x9Cu + 0x1D972C0;
			U16(entity, 0) &= 0xFFFB;
			U8(U32(node, 0x10), 0x63) = 0;
			U16(node, 0x26) = (uint16_t)((U16(node, 0x26) & 0xFFF7) | 1);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// ---- generated (gendesc.py): the module's functions served by the engine's ports (identical
	// code up to module addresses) ----
	static const ModPort ENGINE_PORTS[] = {
		{ 0x81C9A0, (void *)a_73A0D0, "051 MAG_051_HIPOTION_Root_State0_Advance" },
		{ 0x81C9B0, (void *)a_73A170, "051 MAG_051_HIPOTION_Root_State1_SpawnEmitter" },
		{ 0x81D070, (void *)a_743C20, "051 MAG_051_HIPOTION_StepCounterLimit" },
		{ 0x81D0A0, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Mote_State2_Empty" },
		{ 0x81D180, (void *)a_743C00, "051 MAG_051_HIPOTION_Swirl_State2_CounterFinish" },
		{ 0x81D1A0, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Swirl_State3_Empty" },
		{ 0x81D1B0, (void *)a_743C00, "051 MAG_051_HIPOTION_Glow_State1_CounterFinish" },
		{ 0x81D1D0, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Glow_State2_Empty" },
		{ 0x81D1E0, (void *)a_73A380, "051 MAG_051_HIPOTION_AuraTask" },
		{ 0x81DE60, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Aura_State2_Empty" },
		{ 0x81DE70, (void *)a_73F110, "051 MAG_051_HIPOTION_LightTask" },
		{ 0x81DEE0, (void *)a_7335D0, "051 MAG_051_HIPOTION_Light_State0_ResetEntityRecords" },
		{ 0x81DFC0, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Light_State4_Empty" },
		{ 0x81DFD0, (void *)a_73A380, "051 MAG_051_HIPOTION_RingSpawner" },
		{ 0x81E290, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Ring_State2_Empty" },
		{ 0x81E2A0, (void *)a_73A6D0, "051 MAG_051_HIPOTION_RingSpawner_State2_Empty" },
		{ 0x81E2B0, (void *)a_73A380, "051 MAG_051_HIPOTION_SparkleSpawner" },
		{ 0x81E5E0, (void *)a_743C00, "051 MAG_051_HIPOTION_Sparkle_State1_CounterFinish" },
		{ 0x81E600, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Sparkle_State2_Empty" },
		{ 0x81E610, (void *)a_73A6D0, "051 MAG_051_HIPOTION_SparkleSpawner_State2_Empty" },
		{ 0x81F670, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Overlay_State2_Empty" },
		{ 0x81F710, (void *)a_73A380, "051 MAG_051_HIPOTION_ParticleEmitter_Tick" },
		{ 0x81F920, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Particle_State2_Empty" },
		{ 0x81F930, (void *)a_73A6D0, "051 MAG_051_HIPOTION_ParticleEmitter_State2_Empty" },
		{ 0x81F980, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Emitter_State6_Empty" },
		{ 0x81F9E0, (void *)a_73A6D0, "051 MAG_051_HIPOTION_Root_State4_Empty" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports (tasks and the state handlers the state tables hold)
	static const ModPort PORTS[] = {
		{ ORIG_Master, (void *)Master, "051 Hi-Potion Master" },
		{ 0x81F990, (void *)NextAction, "051 NextAction" },
		{ 0x81F9C0, (void *)WaitQueues, "051 WaitQueues" },
		{ ORIG_Emitter, (void *)Emitter, "051 Emitter" },
		{ 0x81CA80, (void *)SpawnFirst, "051 SpawnFirst" },
		{ 0x81E620, (void *)SpawnOverlay, "051 SpawnOverlay" },
		{ 0x81F680, (void *)SpawnAuraC00, "051 SpawnAuraC00" },
		{ 0x81F6B0, (void *)SpawnAuraE00, "051 SpawnAuraE00" },
		{ 0x81F6E0, (void *)SpawnParticleEmitter, "051 SpawnParticleEmitter" },
		{ 0x81F940, (void *)Heal, "051 Heal" },
		{ ORIG_Glow, (void *)Glow, "051 Glow" },
		{ 0x81CCD0, (void *)GlowStart, "051 GlowStart" },
		{ ORIG_Swirl, (void *)Swirl, "051 Swirl" },
		{ 0x81D0B0, (void *)SwirlStart, "051 SwirlStart" },
		{ 0x81D120, (void *)SwirlLoop, "051 SwirlLoop" },
		{ ORIG_Mote, (void *)Mote, "051 Mote" },
		{ 0x81CFF0, (void *)MoteStart, "051 MoteStart" },
		{ 0x81D030, (void *)MoteFall, "051 MoteFall" },
		{ 0x81D240, (void *)AuraStart, "051 AuraStart" },
		{ 0x81D370, (void *)AuraPlay, "051 AuraPlay" },
		{ 0x81E030, (void *)RingSpawnerStart, "051 RingSpawnerStart" },
		{ 0x81E050, (void *)RingSpawn, "051 RingSpawn" },
		{ ORIG_Ring, (void *)Ring, "051 Ring" },
		{ 0x81E1B0, (void *)RingStart, "051 RingStart" },
		{ 0x81E1E0, (void *)RingPlay, "051 RingPlay" },
		{ 0x81E310, (void *)SparkleSpawnerStart, "051 SparkleSpawnerStart" },
		{ 0x81E360, (void *)SparkleSpawn, "051 SparkleSpawn" },
		{ ORIG_Sparkle, (void *)Sparkle, "051 Sparkle" },
		{ 0x81E590, (void *)SparkleStart, "051 SparkleStart" },
		{ 0x81F770, (void *)ParticleEmitterStart, "051 ParticleEmitterStart" },
		{ 0x81F7C0, (void *)ParticleSpawn, "051 ParticleSpawn" },
		{ ORIG_Particle, (void *)Particle, "051 Particle" },
		{ 0x81F8B0, (void *)ParticleStart, "051 ParticleStart" },
		{ 0x81F8F0, (void *)ParticleMove, "051 ParticleMove" },
		{ 0x81DF20, (void *)LightUp, "051 LightUp" },
		{ 0x81DF60, (void *)LightHold, "051 LightHold" },
		{ 0x81DF80, (void *)LightDown, "051 LightDown" },
		{ ORIG_Overlay, (void *)Overlay, "051 Overlay" },
		{ 0x81F480, (void *)OverlayStart, "051 OverlayStart" },
		{ 0x81F620, (void *)OverlayRise, "051 OverlayRise" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag051_hipotion()
	{
		act::register_module(51);
		for (const act::hipotion::ModPort *p = act::hipotion::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(51, p->addr, p->port, p->name);
		for (const act::hipotion::ModPort *p = act::hipotion::PORTS; p->addr; p++)
			act::register_module_port(51, p->addr, p->port, p->name);
		// 30 fps layer: see mag051_hipotion_held.inc
		FX_HELD(register_mag051_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag051_hipotion_held.inc"
#endif
