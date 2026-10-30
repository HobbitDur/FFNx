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

// Effect 133: Hypnotize (enemy attack 76 of kernel.bin, Gayla c0m033; MAG_133_*). No other
// kernel.bin entry uses effect 133.
//
// Structure (setup MAG_133_HYPNOTIZE 0x6B0FB0 -> _Init 0x6B0FE0, file loader 0x6B0FC0 = the
// texture file named at 0x1212B60; the setup keeps the CASTER's entity and the cast context, starts
// camera animation 0x1212A30 and queues the TIM; setup / loader / _Init are the copies of 126..132):
//   MasterTask (0x6B1060) - alternates the packet arena (magic buffer + 0xE0C / + 0x10E0C); on
//     counter 1 sets up the pools (main 1 x 0xC4 at + 0xD40, rings 8 x 0xB0 at + 0x7C0, target
//     layouts 0x10 x 0x54 at + 0x280, target sprites 0x20 x 0x14 at + 0) and starts the main task
//     with the targets of action 0 and the layout 0x1207958. Then the main, ring, target-layout and
//     sprite queues; ends when all four are empty.
//   MainTask (0x6B1230) - counters 0..0x1D: the layout 0x1207958 in VIEW space (not composed with
//     the camera): the caster's default effect position projected (RTPS), the basis (0x50CBA0) of
//     ((0xA0 - sx) << 4, (0x78 - sy) << 4, 12 H) as its rotation, translation (-(0xA0 - sx) << 4,
//     -(0x78 - sy) << 4, 16 H) (H = the projection distance 0x1D8E038); its return value is not
//     tested. Sound at counter 0. Even counters 0..0xE: a ring (table 0x1212AE0 / layouts
//     0x1212B20). Even counters 2..0x16: a target layout at a random bone of a random target (CRT
//     rand; layouts 0x1212B40). Even counters 0x12..0x28: a sprite at a random target's bone 0xF0
//     (CRT rand). Damage of every target of action 0 at 0x22; ends at 0x2A.
//   RingTask (0x6B17C0) - its table point (x, y, z, bone) through the caster's bone (0x6B18A0 =
//     Dissolving Acid's 0x6B2B70) and the camera: the layout with the camera's rotation at that
//     point, depth -0x80; ends with the layout.
//   TargetLayoutTask (0x6B1900) - the layout at its spawn position with the camera's rotation
//     scaled by its word +0x12 (GTE IR0 interpolation of the camera rows), depth -0x80; ends with
//     the layout.
//   SpriteTask (0x6B1740) - sprite sequence 0x12077B8 at its position through the shadow-rotation
//     camera (0x400, -0x100), flipbook frame = its counter; 14 ticks.
//   PrimCallback (0x6B1520, Heartbreak's 0x872700 on this module's cursor and parameter block)
//     draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update: the layouts advance
// in the prim player's play, the sprite's counter after its draw).
// ENTITY / CAMERA: no task writes a battle entity (position +0x1C..+0x20 only read) or the camera
// words (the camera animation is the engine's).
// Module globals: 0x2520EE8..0x2520F60 (caster entity 0x2520EE8, texture file 0x2520EEC, magic
// buffer base 0x2520EF0, context 0x2520EF4, master pool 0x2520EF8, packet cursor 0x2520F0C, the
// queues 0x2520F10 sprites / 0x2520F20 target layouts / 0x2520F30 rings / 0x2520F40 main /
// 0x2520F50 master).
// The parameter block's blended vertex frames (magic buffer + 0xE04) lie 8 bytes before packet
// arena A (+ 0xE0C): on arena-A ticks a blended object overwrites the tick's first packets (as in
// the game).
// NEVER-WRITTEN STACK WORDS (DR0 write watches under the game executor): only the 4th words of the
// vectors loaded into GTE V0 (the ring's bone point, entry esp - 0x2A, and the callback's record
// offset): they reach the GTE's VZ0 high half (0x1CA8A16) only, which nothing reads. The main
// task's anchor carries the caster's height there (GetDefaultEffectPosition writes it); the target
// layout's its scale word. The matrices' pad words (block + 0x12) are never read.
// PC SCREEN UNITS: the PC's software GTE gives screen coordinates in eighths of a pixel
// (GTE_RTPS 0x45FAA0: 8 x H x / z + 8 OFX); the main layout's frame takes them as the PSX's
// pixels, so on PC its translation lands 8 times the caster's screen offset out (off screen) and
// its basis leans out as much. HypnotizeParams main.pixels (off: the PC as it is) takes them back
// to pixels first, as the PSX had them.
// Parameters (HypnotizeParams, fx_params.h): the numbers below, one struct per part, defaults =
// the game's; the parts are tagged (FX_PART) for the effect lab.

#include "mag_common.h"
#include "fx_params.h"

namespace ff8fx
{
namespace hypno133
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2520EE8); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2520EF0); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2520EF4); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2520F0C); }
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2520F10; }        // pool: magic buffer, 0x20 x 0x14
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2520F20; }        // pool: + 0x280, 0x10 x 0x54
	inline TaskQueue *QRings() { return (TaskQueue *)0x2520F30; }          // pool: + 0x7C0, 8 x 0xB0
	inline TaskQueue *QMain() { return (TaskQueue *)0x2520F40; }           // pool: + 0xD40, 1 x 0xC4
	inline int16_t ProjectionH() { return var<int16_t>(0x1D8E038); }

	static const uint32_t ORIG_MasterTask = 0x6B1060;
	static const uint32_t ORIG_MainTask = 0x6B1230;
	static const uint32_t ORIG_SpriteTask = 0x6B1740;
	static const uint32_t ORIG_RingTask = 0x6B17C0;
	static const uint32_t ORIG_TargetLayoutTask = 0x6B1900;
	static const uint32_t MORPH = 0xE04;                 // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0xE0C;               // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x10E0C;
	static const uint32_t MODEL_Main = 0x1207958;        // prim-model layouts
	static const uint32_t *const MODEL_Rings = (const uint32_t *)0x1212B20;   // 8
	static const uint32_t *const MODEL_Targets = (const uint32_t *)0x1212B40; // 8
	static const uint32_t *const RING_TABLE = (const uint32_t *)0x1212AE0;    // 8 x (x, y, z, bone)
	static const uint32_t SEQ_Sprite = 0x12077B8;        // sprite sequence
	static const void *const SOUND_Hypnotize = (const void *)0x1212ADC;

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct HypnotizeParams
	{
		struct Timing
		{
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE reads its world sound id
			int16_t sound_tick;     // the sound plays (main counter)
			int16_t damage_tick;    // every target of action 0 takes the result
			int16_t end_tick;       // the main task ends
		} timing;
		struct Main
		{
			int16_t last_tick;      // drawn while the counter is at most this
			int16_t basis_h;        // its basis: the screen offset at basis_h x H
			int16_t depth_h;        // its translation at depth_h x H
			int16_t offset_shift;   // the screen offset times 2^shift
			int16_t pixels;         // 1: the screen offset in pixels (the PSX) instead of the PC's eighths
		} main;
		struct Spawned
		{
			fxp::Window window;     // main counters it is spawned at...
			int16_t every;          // ...every this many ticks
		};
		Spawned rings;
		struct Targets
		{
			Spawned spawn;
			fxp::Range spread_x;    // added to the bone point's x
			fxp::Range scale;       // its size (4096 = 1), rolled on rand() >> 4
			int16_t depth;          // the prim header's depth
		} targets;
		struct Sprites
		{
			Spawned spawn;
			int16_t bone;           // the target's bone point
			fxp::Range3 spread;     // added to the bone point
			int16_t frames;         // flipbook frames: lifetime
		} sprites;
	};
	static const HypnotizeParams DEFAULTS = {
		{ (uint32_t)SOUND_Hypnotize, 0, 0x22, 0x2A },
		{ 0x1D, 12, 16, 4, 0 },
		{ { 0, 0xF }, 2 },
		{ { { 2, 0x16 }, 2 }, { -0x400, 0x3FF }, { 0x800, 0xFFF }, -0x80 },
		{ { { 0x12, 0x29 }, 2 }, 0xF0, { { -0x100, 0xFF }, { -0x180, 0x7F }, { -0x100, 0xFF } }, 0xE },
	};
	static HypnotizeParams g_params = DEFAULTS;
	static inline const HypnotizeParams &P() { return g_params; }
	enum PartId { PART_MAIN = 1, PART_RINGS = 2, PART_TARGETS = 3, PART_SPRITES = 4 };
	// spawned at counter c: inside the window, every `every` ticks from its first
	static inline bool Spawns(const HypnotizeParams::Spawned &s, int32_t c)
	{
		return s.window.has(c) && s.every > 0 && (c - s.window.first) % s.every == 0;
	}

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2520EF8)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0xC4 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t count;     // +0x0E targets of action 0
		uint8_t *ents[3];  // +0x10 target entities
		uint8_t layout[0xA8]; // +0x1C prim-model layout
	};
	struct RingNode // pool of 8 nodes of 0xB0 bytes
	{
		TaskNode hdr;
		int16_t point[4];  // +0x0C table point x, y, z, caster bone
		uint8_t layout[0x9C]; // +0x14 prim-model layout
	};
	struct TargetNode // pool of 0x10 nodes of 0x54 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C spawn position
		int16_t scale;     // +0x12 0x800..0xFFF
		uint8_t layout[0x40]; // +0x14 prim-model layout
	};
	struct SpriteNode // pool of 0x20 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
	};
	// the prim-player callback's parameter block (a stack block of the drawing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame (main: view space; else camera x anchor)
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0xC4 && sizeof(RingNode) == 0xB0, "Hypnotize nodes");
	static_assert(sizeof(TargetNode) == 0x54 && sizeof(SpriteNode) == 0x14, "Hypnotize nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Hypnotize prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x6B1520, Heartbreak's 0x872700 on this module's cursor and block): one
	// object of a layout. Vertex frame (or blend into the parameter block's morph buffer), rotation
	// from the record's angles, the record's offset through the parameter matrix (flag 0x200: as
	// is), the matrix composed with it, scale unless 1.0, fade, drawn by 0x7043B0.
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x58);
		const int32_t object = (int16_t)r->flags_lo;
		uint8_t *model = l->data + *(const int32_t *)(l->data + object * 4 + 8);
		*(uint8_t **)h = model;
		const int16_t f0 = r->b0, f1 = r->b1;
		const int32_t nverts = *(const int32_t *)(model + 4);
		if (f0 == f1)
		{
			if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
			else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
		}
		else
		{
			const int16_t t = r->b;
			if (t == 0)
			{
				if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
			}
			else if (t == 0x1000)
			{
				if (f1 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f1) * 8 + 0xC;
			}
			else
			{
				BlendVertexFrames((uint32_t)model, f0, f1, t, arg->morph);
				*(uint32_t *)(h + 4) = arg->morph;
			}
		}
		Mat4x3 m = {};
		RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		int32_t t[3];
		if (r->flags & 0x200)
		{
			t[0] = off[0];
			t[1] = off[1];
			t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
			t[0] = m.t[0];
			t[1] = m.t[1];
			t[2] = m.t[2];
		}
		m.t[0] = (int32_t)((uint32_t)t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		PacketCursor() = RenderPrim2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// 0x6B18A0 (= Dissolving Acid's 0x6B2B70): a point through the caster's bone (word +6) composed
	// with its matrix
	static void BonePoint(const int16_t *entry, int16_t *out)
	{
		uint8_t *c = Caster();
		Mat4x3 m = {};
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(**(uint8_t ***)(c + 0x64) + 0x20 + (int32_t)entry[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(entry);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// the main layout's view-space frame (0x6B1243..0x6B130B): the anchor projected, the basis of
	// its screen offset from the centre (PSX pixels: 160, 120) at 12 H, translation at 16 H. The
	// basis direction and the translation share the block's +0x14 words (the original's stack
	// layout). On PC the projection is in eighths of a pixel (see PC SCREEN UNITS above).
	static void MainFrame(const int16_t *pos, PrimArg *arg)
	{
		const HypnotizeParams::Main &mp = P().main;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(pos);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy);
		if (mp.pixels)
		{
			sxy[0] = (int16_t)(sxy[0] >> 3);
			sxy[1] = (int16_t)(sxy[1] >> 3);
		}
		int32_t *v = arg->m.t;
		v[0] = shl32(0xA0 - (int32_t)sxy[0], mp.offset_shift);
		v[1] = shl32(0x78 - (int32_t)sxy[1], mp.offset_shift);
		const int32_t h = ProjectionH();
		v[2] = h * mp.basis_h;
		BuildOrthonormalBasis(v, &arg->m);
		v[1] = (int32_t)(0u - (uint32_t)v[1]);
		v[0] = (int32_t)(0u - (uint32_t)v[0]);
		v[2] = h * mp.depth_h;
	}

	// the camera rows scaled by s / 4096 through the GTE (IR0 interpolation, 0x6B190D..0x6B1958)
	static void ScaledCamera(int32_t s, Mat4x3 *m)
	{
		GteSetIR0(s);
		GteLoadIR123(&Camera().m[0][0]);
		GteGPF();
		GteStoreIR123(&m->m[0][0]);
		GteLoadIR123(&Camera().m[1][0]);
		GteGPF();
		GteStoreIR123(&m->m[1][0]);
		GteLoadIR123(&Camera().m[2][0]);
		GteGPF();
		GteStoreIR123(&m->m[2][0]);
	}

	// an anchor through the camera: rotation already in m, translation = camera x pos + camera t
	static void CameraAnchor(const int16_t *pos, Mat4x3 *m)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(pos);
		GteMVMVA_RotV0();
		GteReadMAC123(m->t);
		m->t[0] = (int32_t)((uint32_t)m->t[0] + (uint32_t)Camera().t[0]);
		m->t[1] = (int32_t)((uint32_t)m->t[1] + (uint32_t)Camera().t[1]);
		m->t[2] = (int32_t)((uint32_t)m->t[2] + (uint32_t)Camera().t[2]);
	}

	// the sprite (0x6B1742..0x6B17A0)
	static void SpriteDraw(const SpriteNode *s)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(s->pos, 0x400, -0x100);
		*(uint32_t *)h = SEQ_Sprite;
		*(int16_t *)(h + 4) = s->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag133_hypnotize_held.h"
#endif

namespace ff8fx
{
namespace hypno133
{
	// ------------------------------------------------------------------
	// Sprite (0x6B1740)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SpriteTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag133_hypnotize_held.inc
		FX_HELD(held_note_sprite(s);)
		FX_PART(PART_SPRITES);
		SpriteDraw(s);
		s->counter++;
		return s->counter < P().sprites.frames ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Ring (0x6B17C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *g = (RingNode *)n;
		FX_PART(PART_RINGS);
		int16_t pos[4];
		BonePoint(g->point, pos);
		PrimArg arg;
		memcpy(&arg.m.m, &Camera().m, sizeof(arg.m.m));
		CameraAnchor(pos, &arg.m);
		arg.depth = -0x80;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag133_hypnotize_held.inc
		FX_HELD(held_note_play(PLAY_RING, n, (prim::Layout *)g->layout, &arg);)
		return prim::play((prim::Layout *)g->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Target layout (0x6B1900)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetLayoutTask(TaskNode *n)
	{
		TargetNode *p = (TargetNode *)n;
		FX_PART(PART_TARGETS);
		PrimArg arg;
		ScaledCamera(p->scale, &arg.m);
		CameraAnchor(p->pos, &arg.m);
		arg.depth = P().targets.depth;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag133_hypnotize_held.inc
		FX_HELD(held_note_play(PLAY_TARGET, n, (prim::Layout *)p->layout, &arg);)
		return prim::play((prim::Layout *)p->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// a random target of action 0 (CRT rand)
	static uint8_t *RandomTarget(const MainNode *t)
	{
		const int32_t i = mul32(CrtRand(), (int32_t)t->count) >> 15;
		return t->ents[i];
	}

	// ------------------------------------------------------------------
	// Main (0x6B1230)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *t = (MainNode *)n;
		const HypnotizeParams &pp = P();
		if ((uint16_t)t->counter <= (uint16_t)pp.main.last_tick)
		{
			FX_PART(PART_MAIN);
			int16_t pos[4];
			GetDefaultEffectPosition(Caster(), pos);
			PrimArg arg;
			MainFrame(pos, &arg);
			arg.depth = 0;
			arg.morph = TexBase() + MORPH;
			// 30 fps layer: see mag133_hypnotize_held.inc
			FX_HELD(held_note_play(PLAY_MAIN, n, (prim::Layout *)t->layout, &arg);)
			prim::play((prim::Layout *)t->layout, PrimCallback, (int)&arg, 0);
		}
		if (t->counter == pp.timing.sound_tick) BdPlaySE((const void *)fxp::sound(pp.timing.sound, (uint32_t)SOUND_Hypnotize), 0, 0x80);
		const int32_t c = t->counter;
		if (Spawns(pp.rings, c))
		{
			RingNode *g = (RingNode *)AddTaskToQueue(QRings(), ORIG_RingTask);
			if (g)
			{
				const uint32_t i = (uint32_t)((c - pp.rings.window.first) / pp.rings.every) & 7;
				((uint32_t *)g->point)[0] = RING_TABLE[2 * i];
				((uint32_t *)g->point)[1] = RING_TABLE[2 * i + 1];
				DecodeModelPrimLayout(MODEL_Rings[i], g->layout, 0x9C);
			}
		}
		if (Spawns(pp.targets.spawn, c))
		{
			const uint32_t e = (uint32_t)((c - pp.targets.spawn.window.first) / pp.targets.spawn.every);
			TargetNode *p = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetLayoutTask);
			if (p)
			{
				uint8_t *ent = RandomTarget(t);
				const int32_t flag = CrtRand() >> 3;
				const int32_t bone = mul32(CrtRand(), (int32_t)***(uint8_t ***)(ent + 0x64)) >> 15;
				GetEffectSpawnPosition(ent, bone, flag, p->pos);
				p->pos[0] = (int16_t)(p->pos[0] + pp.targets.spread_x.roll(CrtRand()));
				p->scale = (int16_t)pp.targets.scale.roll(CrtRand() >> 4);
				DecodeModelPrimLayout(MODEL_Targets[e & 7], p->layout, 0x40);
			}
		}
		if (Spawns(pp.sprites.spawn, c))
		{
			SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
			if (s)
			{
				uint8_t *ent = RandomTarget(t);
				GetEffectSpawnPosition(ent, pp.sprites.bone, 0, s->pos);
				s->pos[0] = (int16_t)(s->pos[0] + pp.sprites.spread.x.roll(CrtRand()));
				s->pos[1] = (int16_t)(s->pos[1] + pp.sprites.spread.y.roll(CrtRand()));
				const int32_t rz = CrtRand();
				s->counter = 0;
				s->pos[2] = (int16_t)(s->pos[2] + pp.sprites.spread.z.roll(rz));
			}
		}
		if (t->counter == pp.timing.damage_tick) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		t->counter++;
		return t->counter < pp.timing.end_tick ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Master (0x6B1060)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag133_hypnotize_held.inc
		FX_HELD(held_note_master();)
		if (m->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			m->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			m->arena = 1;
		}
		if (m->counter == 1 && !m->started)
		{
			m->started = 1;
			InitTaskQueuePool(QMain(), (void *)(TexBase() + 0xD40), 0xC4, 1);
			InitTaskQueuePool(QRings(), (void *)(TexBase() + 0x7C0), 0xB0, 8);
			InitTaskQueuePool(QTargets(), (void *)(TexBase() + 0x280), 0x54, 0x10);
			InitTaskQueuePool(QSprites(), (void *)TexBase(), 0x14, 0x20);
			MainNode *t = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&t->counter, 0, 0x2E);
			t->count = (int16_t)Ctx()->actions[0].target_count;
			for (int32_t i = 0; i < t->count; i++)
				t->ents[i] = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
			DecodeModelPrimLayout(MODEL_Main, t->layout, 0xA8);
		}
		int a0, a1, a2, a3;
		if (m->started)
		{
			a0 = ExecuteTaskQueue(QMain());
			a1 = ExecuteTaskQueue(QRings());
			a2 = ExecuteTaskQueue(QTargets());
			a3 = ExecuteTaskQueue(QSprites());
			FX_PART(0);
		}
		else a0 = a1 = a2 = a3 = (int)n;
		if (m->started && a0 == 0 && a1 == 0 && a2 == 0 && a3 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of HypnotizeParams
	// ------------------------------------------------------------------
	namespace hypno133
	{
		using fxp::Field;
#define HYPNO_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(HypnotizeParams, member), fxp::type, lo, hi }
#define HYPNO_UNIT(part, member, label, doc, type, lo, hi, unit) \
		{ part, #member, label, doc, (uint32_t)offsetof(HypnotizeParams, member), fxp::type, lo, hi, fxp::unit }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The main task's clock: when the sound plays, when the targets take the result, when it ends (draws nothing)" },
			{ PART_MAIN, "main", "Main layout", "The layout in front of the caster, framed in view space on its screen position" },
			{ PART_RINGS, "rings", "Rings", "Eight layouts at points of the caster's bones, one every other tick" },
			{ PART_TARGETS, "targets", "Target layouts", "Layouts at random bones of random targets, at random sizes" },
			{ PART_SPRITES, "sprites", "Sprites", "A flipbook at random points around a target's bone" },
		};
		static const fxp::Group GROUPS[] = {
			{ "rings", "Spawning", "Spawned at main ticks From tick, From tick + Every, ... up to To tick; layout = ((tick - From tick) / Every) mod 8 of 8, at the caster's bone points" },
			{ "targets.spawn", "Spawning", "Spawned at main ticks From tick, From tick + Every, ... up to To tick; layout = ((tick - From tick) / Every) mod 8 of 8, at a random bone of a random target" },
			{ "targets.spread_x", "Spread x", "x += min + random % (max - min + 1)" },
			{ "targets.scale", "Size", "size = min + (random / 16) % (max - min + 1) (4096 = its own size)" },
			{ "sprites.spawn", "Spawning", "One flipbook spawned at main ticks From tick, From tick + Every, ... up to To tick" },
			{ "sprites.spread", "Spread", "Added to the bone point: x, then y, then z, each min + random % (max - min + 1) (y goes down)" },
		};
		static const Field FIELDS[] = {
			HYPNO_FIELD("timing", timing.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			HYPNO_FIELD("timing", timing.sound_tick, "Sound tick", "The sound plays when the main tick == this", I16, 0, 200),
			HYPNO_FIELD("timing", timing.damage_tick, "Damage tick", "The targets take the result when the main tick == this", I16, 0, 200),
			HYPNO_FIELD("timing", timing.end_tick, "End tick", "The main task ends when the main tick reaches this", I16, 1, 200),
			HYPNO_FIELD("main", main.last_tick, "Last tick", "Drawn while main tick <= this", I16, -1, 200),
			HYPNO_FIELD("main", main.basis_h, "Basis depth (x H)", "It faces along (screen offset, H x this), H = the projection distance", I16, 1, 64),
			HYPNO_FIELD("main", main.depth_h, "Depth (x H)", "It sits H x this in front of the camera", I16, 1, 64),
			HYPNO_FIELD("main", main.offset_shift, "Offset scale (2^n)", "screen offset = (160 - x, 120 - y) of the caster's projected point x 2^this", I16, 0, 8),
			HYPNO_FIELD("main", main.pixels, "PSX screen units", "1: the caster's screen position in pixels as on PSX (x, y / 8; on PC it is in eighths: the layout lands far off it)", I16, 0, 1),
			HYPNO_FIELD("rings", rings.window.first, "From tick", "", I16, 0, 200),
			HYPNO_FIELD("rings", rings.window.last, "To tick", "", I16, 0, 200),
			HYPNO_FIELD("rings", rings.every, "Every", "8 layouts, then they repeat", I16, 1, 50),
			HYPNO_FIELD("targets", targets.spawn.window.first, "From tick", "", I16, 0, 200),
			HYPNO_FIELD("targets", targets.spawn.window.last, "To tick", "", I16, 0, 200),
			HYPNO_FIELD("targets", targets.spawn.every, "Every", "", I16, 1, 50),
			HYPNO_FIELD("targets", targets.spread_x.min, "Min", "", I16, -0x2000, 0x2000),
			HYPNO_FIELD("targets", targets.spread_x.max, "Max", "", I16, -0x2000, 0x2000),
			HYPNO_UNIT("targets", targets.scale.min, "Min", "", I16, 0, 0x7FFF, Scale),
			HYPNO_UNIT("targets", targets.scale.max, "Max", "", I16, 0, 0x7FFF, Scale),
			HYPNO_FIELD("targets", targets.depth, "Depth", "Added to its drawing depth (sort key)", I16, -0x1000, 0x1000),
			HYPNO_FIELD("sprites", sprites.spawn.window.first, "From tick", "", I16, 0, 200),
			HYPNO_FIELD("sprites", sprites.spawn.window.last, "To tick", "", I16, 0, 200),
			HYPNO_FIELD("sprites", sprites.spawn.every, "Every", "", I16, 1, 50),
			HYPNO_FIELD("sprites", sprites.bone, "Bone", "The bone point of a random target they start from", I16, 0, 0xFF),
			HYPNO_FIELD("sprites", sprites.spread.x.min, "x min", "", I16, -0x2000, 0x2000),
			HYPNO_FIELD("sprites", sprites.spread.x.max, "x max", "", I16, -0x2000, 0x2000),
			HYPNO_FIELD("sprites", sprites.spread.y.min, "y min", "y goes down", I16, -0x2000, 0x2000),
			HYPNO_FIELD("sprites", sprites.spread.y.max, "y max", "", I16, -0x2000, 0x2000),
			HYPNO_FIELD("sprites", sprites.spread.z.min, "z min", "", I16, -0x2000, 0x2000),
			HYPNO_FIELD("sprites", sprites.spread.z.max, "z max", "", I16, -0x2000, 0x2000),
			HYPNO_FIELD("sprites", sprites.frames, "Frames", "Flipbook frames, one per tick: it lives this many ticks", I16, 1, 64),
		};
#undef HYPNO_FIELD
#undef HYPNO_UNIT
	}

	void register_mag133_hypnotize()
	{
		fxp::register_params({ 133, "Hypnotize", &hypno133::g_params, &hypno133::DEFAULTS, sizeof(hypno133::HypnotizeParams),
		                       hypno133::PARTS, (int)(sizeof(hypno133::PARTS) / sizeof(hypno133::PARTS[0])),
		                       hypno133::FIELDS, (int)(sizeof(hypno133::FIELDS) / sizeof(hypno133::FIELDS[0])),
		                       hypno133::GROUPS, (int)(sizeof(hypno133::GROUPS) / sizeof(hypno133::GROUPS[0])) });
		register_port(hypno133::ORIG_MasterTask, (void *)hypno133::MasterTask, "133 MasterTask", 133);
		register_port(hypno133::ORIG_MainTask, (void *)hypno133::MainTask, "133 MainTask", 133);
		register_port(hypno133::ORIG_RingTask, (void *)hypno133::RingTask, "133 RingTask", 133);
		register_port(hypno133::ORIG_TargetLayoutTask, (void *)hypno133::TargetLayoutTask, "133 TargetLayoutTask", 133);
		register_port(hypno133::ORIG_SpriteTask, (void *)hypno133::SpriteTask, "133 SpriteTask", 133);
		// 30 fps layer: see mag133_hypnotize_held.inc
		FX_HELD(register_mag133_held();)
	}
	FX_LAB_MODULE(register_mag133_hypnotize);
}

#ifdef FF8_FX_HELD
#include "mag133_hypnotize_held.inc"
#endif
