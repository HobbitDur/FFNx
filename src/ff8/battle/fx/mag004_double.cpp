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

// Effect 4: Double (spell, MAG_004_DOUBLE_*).
//
// Structure (module code 0x8D5060..0x8D69E0: file loader MAG_004_DOUBLE_FL 0x8D5060 = the texture
// file named at 0x16417B8 (pointer 0x276FB00); setup MAG_004_DOUBLE_Init 0x8D5080 sets up the queues
// and the root node, starts camera animation 0x1641188, queues the TIM and takes the two packet
// arenas: texture buffer + 0 (0x277A344) / + 0x6000 (0x277A340)). Every node starts with the shared
// effect-library header (Effect_AddTaskAndInitFromCtx 0x8DC540, as in Cure); every task runs its
// current phase function (node +0x29) from a small table and ends when it is flagged done (+0x26
// bit0) and has no live children. Every task updates THEN draws; none tests the draw-only flags
// (battle_to_update_flags 0x201).
//   Root (0x8D5180) - packet arena by tick parity (cursor 0x276FB04), follows the caster's bones;
//     phases: 0 advance, 1 spawns the emitter of the current action (marks the root busy), 2 waits
//     while busy, then the next action (back to 1) or on, 3 waits until the five queues are empty.
//     Queues run: emitters, sequencers, sprites, models, flashes.
//   Emitter (0x8D52A0), one per action: target anchor and model bounds (effect library 0x8DC610 /
//     0x8DC870); phase 0 spawns the rune sequencer, phase 1 at tick 19 releases the root and spawns
//     the rising flash, phase 2 applies the action result. Sound at its first tick. Its word +0x58
//     is a handshake with the last rune: set to 1 by the rune, the emitter then starts a screen fade
//     (au_re_BdLinkTask_6) and the big rune and sets 2 (every rune and glow reacts in that tick),
//     then 0.
//   Sequencer (0x8D5B40) - one rune per tick at angles 0, 0x333, 0x666, 0x999, 0xCCC (the last
//     one flagged), then done.
//   Rune (0x8D5C80, prim model 0x163F014) - on a circle of radius 0x400 around the target, spinning;
//     fades in (0x1000 -> 0 in 4 ticks), then shrinks while fading out and ends hidden. While
//     spawning (its first ticks) a column (tick 3) and a light mote per tick (ticks 0..6).
//   Column (0x8D5E60, prim model 0x163EB94, flash model 0x163EDF4) - grows in x/z, then in y, then
//     settles; the flagged rune's column starts the handshake; flashes (the flash model drawn
//     over it) once when it settles and once on the emitter's signal, then fades out.
//   Glow (0x8D6200, sprite 0x163EB6C lying on the ground under the rune) - grey ramp up to 0x20,
//     a 0x40 pulse on the emitter's signal, then out.
//   Mote (0x8D6490, flipbook 0x163E930) - spirals in (radius -0x60, angle +0x60 per tick) and
//     brightens, until its radius is gone.
//   Big rune (0x8D53C0, prim model 0x163F224) - fades in then out; while it fades in, five sprites
//     (0x8D58A0) per tick for ticks 2..4 and a morphing ring (0x8D5750) at ticks 0 and 2.
//   Morphing ring (0x8D5750, prim model 0x163FFD0) - its vertices are blended between the vertex
//     sets 0x163FB00 and 0x163FD68 by its counter ramp (GTE GPF / GPL, written into the model in
//     the exe data) before each draw; fades while it opens.
//   Sprite (0x8D58A0, flipbook 0x163EA90) - random flipbook frame and X angle every tick, a grey
//     ramp 0x18 / 0x30 / 0x40 / 0x40 / 0x30 / 0x18, then done.
//   Flash (0x8D6790) - a screen-space textured quad (tpage 0x240/0x120, CLUT 0x140/0xF4, 0x28 x 0x10
//     texels) on the target's top anchor that jumps up and falls back, drawn straight into the
//     render list (no +0x44), for 15 ticks. Not spawned when the target record's +3 bit 2 is set.
// Module globals: 0x276FB00..0x277AEA8 (texture file pointer 0x276FB00, packet cursor 0x276FB04,
// pools / queues: models 0x276FB08 / 0x2774F68 (200 x 0x6C), root 0x2774F90 / 0x2775058 (2 x 0x64),
// sprites 0x2775068 / 0x277A1A8 (200 x 0x68), emitters 0x277A1D0 / 0x277A1C0 (4 x 0x5C), arenas
// 0x277A340 / 0x277A344, sequencers 0x277A358 / 0x277A348 (40 x 0x40), flashes 0x277AD58 /
// 0x277AE98 (4 x 0x50)); the morphing ring's vertex block in the exe data 0x163FFD8..0x1640238;
// the TransformCameraByShadowRotation scratch 0x21DFED0.

#include "mag_common.h"

namespace ff8fx
{
namespace double004
{
	using namespace eng;
	using namespace magc;

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	// shared effect-library node header (0x30 bytes, Effect_AddTaskAndInitFromCtx 0x8DC540)
	struct FxNode
	{
		TaskNode hdr;          // +0x00
		CastContext *ctx;      // +0x0C
		FxNode *root;          // +0x10
		FxNode *emitter;       // +0x14 first node under the root on this node's line
		FxNode *parent;        // +0x18 (its +0x28 counts the live children)
		int16_t pos[4];        // +0x1C x, y, z (copied from the parent)
		int16_t counter;       // +0x24
		uint16_t flags;        // +0x26 bit0 done, bit1 spawning, bit2 hidden
		uint8_t children;      // +0x28
		int8_t phase;          // +0x29
		int8_t action;         // +0x2A
		int8_t target;         // +0x2B
		uint8_t attacker;      // +0x2C
		uint8_t slot;          // +0x2D target entity slot
		uint8_t line;          // +0x2E
		uint8_t last;          // +0x2F
	};
	struct RootNode // pool of 2 nodes of 0x64 bytes
	{
		FxNode h;
		uint8_t bones[0x28];   // +0x30 caster bone anchor (Effect_UpdateTargetPosFromBones)
		int16_t last_action;   // +0x58
		int16_t target_count;  // +0x5A
		int16_t tick;          // +0x5C arena parity
		int16_t alive;         // +0x5E sum of the queue results
		uint8_t pad60[3];
		uint8_t busy;          // +0x63 an action is running
	};
	struct EmitterNode // pool of 4 nodes of 0x5C bytes
	{
		FxNode h;
		uint8_t anchor[0x18];  // +0x30 target anchors: +0x30 base, +0x38 middle, +0x40 top
		int16_t bmin[4];       // +0x48 model bounds
		int16_t bmax[4];       // +0x50
		int16_t signal;        // +0x58 handshake with the last rune (1 -> 2 -> 0)
		int16_t pad5A;
	};
	struct SeqNode // pool of 40 nodes of 0x40 bytes
	{
		FxNode h;
		uint8_t pad30[0x10];
	};
	struct ModelNode // rune / column / big rune / morphing ring: pool of 200 nodes of 0x6C bytes
	{
		FxNode h;
		int32_t scale[3];      // +0x30
		uint32_t pad3C;
		uint8_t rgb[4];        // +0x40 prim-model colour (dword)
		int16_t angle[3];      // +0x44 rotation (y spins)
		int16_t pad4A;
		uint32_t model;        // +0x4C prim model
		int16_t fade;          // +0x50 prim-model fade
		int16_t pad52;
		int16_t base[4];       // +0x54 rune / column: circle centre
		int16_t orbit;         // +0x5C rune / column: angle on the circle
		int16_t flagged;       // +0x5E the last rune (column: copied)
		int16_t flash;         // +0x60 column: flash state (1 / 3 start, 2 / 4 fade)
		int16_t radius;        // +0x62 column: circle radius (4.12 of 0x400)
		int16_t flash_on;      // +0x64 column: flash model drawn
		int16_t orbit_step;    // +0x66 column: angle step (never written: 0)
		int16_t morph;         // +0x68 morphing ring: blend
		int16_t arg;           // +0x6A rune spawn argument (0)
	};
	struct SpriteNode // sprite / glow / mote: pool of 200 nodes of 0x68 bytes
	{
		FxNode h;
		int32_t scale[3];      // +0x30
		uint32_t pad3C;
		int16_t ax;            // +0x40 sprite: X angle
		int16_t ay;            // +0x42 sprite: Y angle; mote: spiral angle
		uint32_t pad44;
		uint8_t rgb[4];        // +0x48 colour (dword)
		uint32_t seq;          // +0x4C flipbook
		int16_t frame;         // +0x50
		int16_t last;          // +0x52 last frame
		int16_t shade;         // +0x54 TransformCameraByShadowRotation offset (0)
		int16_t pad56;
		int16_t base[4];       // +0x58 glow / mote: centre
		int16_t radius;        // +0x60 mote: spiral radius
		int16_t angle;         // +0x62 glow: angle on the circle
		int16_t pulse;         // +0x64 glow: pulse state (1 / 3 start, 2 / 4 fade)
		int16_t pad66;
	};
	struct FlashNode // pool of 4 nodes of 0x50 bytes
	{
		FxNode h;
		int16_t pad30;
		int16_t vel;           // +0x32 y velocity
		uint32_t pad34;
		int16_t target[4];     // +0x38 rest position (the target's top anchor)
		int16_t tx, ty;        // +0x40 texture page (VRAM x, y)
		int16_t cx, cy;        // +0x44 CLUT (VRAM x, y)
		int16_t u, v;          // +0x48 texel origin
		int16_t tw, th;        // +0x4C size
	};
#pragma pack(pop)
	static_assert(sizeof(FxNode) == 0x30 && sizeof(RootNode) == 0x64 && sizeof(EmitterNode) == 0x5C, "Double nodes");
	static_assert(sizeof(SeqNode) == 0x40 && sizeof(ModelNode) == 0x6C && sizeof(SpriteNode) == 0x68, "Double nodes");
	static_assert(sizeof(FlashNode) == 0x50, "Double nodes");

	// ------------------------------------------------------------------
	// Module globals
	// ------------------------------------------------------------------
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x276FB04); }
	inline TaskQueue *QEmitters() { return (TaskQueue *)0x277A1C0; }  // 4 x 0x5C
	inline TaskQueue *QSequencers() { return (TaskQueue *)0x277A348; }// 40 x 0x40
	inline TaskQueue *QSprites() { return (TaskQueue *)0x277A1A8; }   // 200 x 0x68
	inline TaskQueue *QModels() { return (TaskQueue *)0x2774F68; }    // 200 x 0x6C
	inline TaskQueue *QFlashes() { return (TaskQueue *)0x277AE98; }   // 4 x 0x50

	static const uint32_t ORIG_Root = 0x8D5180;
	static const uint32_t ORIG_Emitter = 0x8D52A0;
	static const uint32_t ORIG_BigRune = 0x8D53C0;
	static const uint32_t ORIG_Morph = 0x8D5750;
	static const uint32_t ORIG_Sprite = 0x8D58A0;
	static const uint32_t ORIG_Sequencer = 0x8D5B40;
	static const uint32_t ORIG_Rune = 0x8D5C80;
	static const uint32_t ORIG_Column = 0x8D5E60;
	static const uint32_t ORIG_Glow = 0x8D6200;
	static const uint32_t ORIG_Mote = 0x8D6490;
	static const uint32_t ORIG_Flash = 0x8D6790;

	static const void *const SOUND_Double = (const void *)0x163E92C;
	static const uint32_t MODEL_BigRune = 0x163F224, MODEL_Rune = 0x163F014, MODEL_Column = 0x163EB94;
	static const uint32_t MODEL_ColumnFlash = 0x163EDF4, MODEL_Morph = 0x163FFD0;
	static const uint32_t MORPH_A = 0x163FB00, MORPH_B = 0x163FD68;
	static const uint32_t SEQ_Sprite = 0x163EA90, SEQ_Glow = 0x163EB6C, SEQ_Mote = 0x163E930;
	static const uint32_t ARENA_A = 0x277A344, ARENA_B = 0x277A340;

	// ------------------------------------------------------------------
	// Engine / effect-library functions (original addresses)
	// ------------------------------------------------------------------
	// Effect_AddTaskAndInitFromCtx (0x8DC540): new node of `size` bytes in `q`, zeroed, header
	// inherited from `parent` (the parent's child count goes up)
	inline FxNode *AddEffectTask(TaskQueue *q, uint32_t task, int32_t size, void *parent) { return fn<FxNode *(__cdecl *)(TaskQueue *, uint32_t, int32_t, void *)>(0x8DC540)(q, task, size, parent); }
	inline void ReleaseLinkedTask(void *n) { fn<void (__cdecl *)(void *)>(0x8DC530)(n); }
	inline void EmitterUpdatePos(void *n) { fn<void (__cdecl *)(void *)>(0x8DC610)(n); }                  // MAG_001_CURE_Emitter_UpdatePos
	inline void ComputeModelBounds(void *n) { fn<void (__cdecl *)(void *)>(0x8DC870)(n); }                // MAG_001_CURE_Emitter_ComputeModelBounds
	inline void UpdateTargetPosFromBones(void *n) { fn<void (__cdecl *)(void *)>(0x8DC740)(n); }
	inline void CopyAnchorXZFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC6E0)(n, out); } // emitter +0x30
	inline void CopyAnchorTopFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC720)(n, out); } // emitter +0x40
	inline void IdentityMatrix(Mat4x3 *m) { fn<void (__cdecl *)(Mat4x3 *)>(0x8DD770)(m); }
	inline void RotateX(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD7E0)(m, a); }
	inline void RotateY(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD8A0)(m, a); }
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void InsertPrimAltViewport(uint32_t ot, void *prim) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(ot, prim); }

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }

	// ------------------------------------------------------------------
	// Drawing helpers
	// ------------------------------------------------------------------
	// sub_8D5440: the node's prim model at its position, rotation (+0x44) and scale (+0x30), in its
	// colour (+0x40) and fade (+0x50)
	static void DrawModel(const ModelNode *n)
	{
		if (n->h.flags & 4) return;
		Mat4x3 m;
		BuildRotationMatrixFromAngles(n->angle, &m);
		m.t[0] = n->h.pos[0];
		m.t[1] = n->h.pos[1];
		m.t[2] = n->h.pos[2];
		Scale3DMatrix(&m, n->scale);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		F<uint32_t>(h, 0) = n->model;
		F<uint32_t>(h, 8) = *(const uint32_t *)n->rgb;
		F<int32_t>(h, 0xC) = n->fade;
		F<uint32_t>(h, 0x1C) = 0xF0;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// sub_8D5950: a sprite's flipbook frame turned by its Y then X angle, scaled, in its colour
	static void DrawSpriteTurned(const SpriteNode *p)
	{
		if (p->h.flags & 4) return;
		Mat4x3 m;
		IdentityMatrix(&m);
		RotateY(&m, p->ay);
		RotateX(&m, p->ax);
		m.t[0] = p->h.pos[0];
		m.t[1] = p->h.pos[1];
		m.t[2] = p->h.pos[2];
		Scale3DMatrix(&m, p->scale);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint32_t>(h, 0) = p->seq;
		F<int16_t>(h, 4) = p->frame;
		F<uint32_t>(h, 0x1C) = *(const uint32_t *)p->rgb;
		F<int16_t>(h, 0x24) = 4;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// sub_8D6330 (= MAG_001_CURE_DrawAdditiveGlow): flipbook `seq` frame 0 lying on the ground under
	// the node (rotated 90 degrees about X, y = 0), in `colour`
	static void DrawGroundGlow(const SpriteNode *p, uint32_t seq, const uint32_t *colour)
	{
		if (p->h.flags & 4) return;
		Mat4x3 m;
		IdentityMatrix(&m);
		RotateX(&m, 0x400);
		m.t[0] = p->h.pos[0];
		m.t[1] = 0;
		m.t[2] = p->h.pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint32_t>(h, 0) = seq;
		F<int16_t>(h, 4) = 0;
		F<uint32_t>(h, 0x1C) = *colour;
		F<int16_t>(h, 0x24) = 4;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// sub_8D6580 (= MAG_001_CURE_DrawSprite): the node's flipbook frame at its position (camera-facing)
	static void DrawSpriteFacing(const SpriteNode *p)
	{
		if (p->h.flags & 4) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(p->h.pos, 0x1000, p->shade);
		F<uint32_t>(h, 0) = p->seq;
		F<int16_t>(h, 4) = p->frame;
		F<int16_t>(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// sub_8D57C0: vertex blend out = a * (1 - t) + b * t (4.12) of the vertex blocks (+4 = count,
	// 8 bytes per vertex from +8), through the GTE (GPF then GPL, 3 words stored per vertex)
	static void MorphVertices(const uint8_t *a, const uint8_t *b, uint8_t *out, int16_t t)
	{
		const uint8_t *pa = a + 8;
		const uint8_t *pb = b + 8;
		uint8_t *po = out + 8;
		int32_t count = *(const int32_t *)(a + 4);
		int32_t *w = (int32_t *)FieldAlloc(8);
		w[1] = t;
		w[0] = 0x1000 - t;
		for (; count > 0; count--)
		{
			GteSetIR0(w[0]);
			GteLoadIR123(pa);
			GteGPF();
			GteSetIR0(w[1]);
			GteLoadIR123(pb);
			GteGPL();
			GteStoreIR123(po);
			pa += 8;
			pb += 8;
			po += 8;
		}
		FieldFree(8);
	}

	// ------------------------------------------------------------------
	// Big rune (0x8D53C0) and what it spawns
	// ------------------------------------------------------------------
	// sub_8D5570: while fading in (ticks 2..4) five sprites: mode 0 = around the rune at angles
	// k * 0x333, random x in -0x180..0x17F, 0x2C0 back; mode 1 (not used) = random 0x160..0x29F out
	// at angles k * 0x333 + 0x266
	static void SpawnSprites(ModelNode *n, int16_t mode)
	{
		if (n->h.counter < 2 || n->h.counter > 4) return;
		for (int32_t i = 0; i < 5; i++)
		{
			const int32_t k = ((int32_t)n->h.counter * 5 + i) % 5;
			SpriteNode *p = (SpriteNode *)AddEffectTask(QSprites(), ORIG_Sprite, 0x68, n);
			p->seq = SEQ_Sprite;
			p->last = 7;
			p->scale[0] = 0x1000;
			p->scale[1] = 0x1000;
			p->scale[2] = 0x1000;
			int16_t v[4];
			if (mode == 0)
			{
				const int32_t r = CrtRand() % 0x300;
				v[1] = 0;
				v[2] = (int16_t)-0x2C0;
				v[0] = (int16_t)(r - 0x180);
				p->ay = (int16_t)(k * 0x333);
			}
			else if (mode == 1)
			{
				const int32_t r = CrtRand() % 0x140;
				v[1] = 0;
				v[2] = 0;
				v[0] = (int16_t)(r + 0x160);
				p->ay = (int16_t)(k * 0x333 + 0x266);
			}
			// (another mode would turn the never-written vector: not reached)
			Mat4x3 m;
			IdentityMatrix(&m);
			RotateY(&m, p->ay);
			MatrixMulVector(&m, v, v);
			p->h.pos[0] = (int16_t)(p->h.pos[0] + v[0]);
			p->h.pos[1] = (int16_t)(p->h.pos[1] + v[1]);
			p->h.pos[2] = (int16_t)(p->h.pos[2] + v[2]);
		}
	}

	// sub_8D5500: a morphing ring at ticks 0 and 2
	static void SpawnMorph(ModelNode *n)
	{
		if (n->h.counter != 0 && n->h.counter != 2) return;
		ModelNode *m = (ModelNode *)AddEffectTask(QModels(), ORIG_Morph, 0x6C, n);
		CopyAnchorXZFromSource(m, m->h.pos);
		m->h.pos[1] = (int16_t)(m->h.pos[1] - 0x280);
		m->angle[1] = 0x199;
		m->model = MODEL_Morph;
		m->fade = 0;
		m->scale[0] = 0x1000;
		m->scale[1] = 0x1000;
		m->scale[2] = 0x1000;
		m->rgb[0] = 0;
		m->rgb[1] = 0;
		m->rgb[2] = 0;
	}

	// MAG_004_sub_8D5360 (the emitter's signal): the big rune on the target
	static void SpawnBigRune(EmitterNode *e)
	{
		ModelNode *n = (ModelNode *)AddEffectTask(QModels(), ORIG_BigRune, 0x6C, e);
		CopyAnchorXZFromSource(n, n->h.pos);
		n->h.pos[1] = (int16_t)(n->h.pos[1] - 0x280);
		n->scale[0] = 0x1000;
		n->scale[1] = 0x1000;
		n->scale[2] = 0x1000;
		n->angle[1] = 0x199;
		n->model = MODEL_BigRune;
		n->fade = 0x800;
		n->rgb[0] = 0xFF;
		n->rgb[1] = 0xFF;
		n->rgb[2] = 0xFF;
	}

	// phases 0x8D56D0 (fade in, spawning), 0x8D5700 (fade out, then hidden and done), 0x8D5740
	static void BigRunePhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0:
			n->fade = (int16_t)(n->fade - 0x200);
			*(uint8_t *)&n->h.flags |= 2;
			if (n->fade <= 0)
			{
				n->fade = 0;
				n->rgb[0] = 0;
				n->rgb[1] = 0;
				n->rgb[2] = 0;
				n->h.phase++;
			}
			break;
		case 1:
			n->fade = (int16_t)(n->fade + 0x300);
			if (n->fade >= 0x1000)
			{
				n->fade = 0x1000;
				n->h.flags = (uint16_t)((n->h.flags & 0xFFFD) | 5);
				n->h.phase++;
			}
			break;
		default: break;
		}
	}
}
}

#ifdef FF8_FX_HELD
#include "mag004_double_held.h"
#endif

namespace ff8fx
{
namespace double004
{
	static uint32_t __cdecl BigRuneTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		BigRunePhase(n);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_BIG, n);)
		DrawModel(n);
		if (n->h.flags & 2)
		{
			SpawnSprites(n, 0);
			SpawnMorph(n);
		}
		n->h.counter++;
		if ((n->h.flags & 1) && n->h.children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Morphing ring (0x8D5750)
	// ------------------------------------------------------------------
	// phases 0x8D5860 (blend and fade +0x200 per tick, done at 0x1000), 0x8D5890
	static void MorphPhase(ModelNode *n)
	{
		if (n->h.phase != 0) return;
		n->morph = (int16_t)(n->morph + 0x200);
		n->fade = (int16_t)(n->fade + 0x200);
		if (n->fade >= 0x1000)
		{
			*(uint8_t *)&n->h.flags |= 1;
			n->fade = 0x1000;
			n->h.phase++;
		}
	}

	static uint32_t __cdecl MorphTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		MorphPhase(n);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_MORPH, n);)
		MorphVertices((const uint8_t *)MORPH_A, (const uint8_t *)MORPH_B, (uint8_t *)MODEL_Morph, n->morph);
		DrawModel(n);
		n->h.counter++;
		if ((n->h.flags & 1) && n->h.children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Sprite (0x8D58A0)
	// ------------------------------------------------------------------
	// phases 0x8D5A30.. 0x8D5AE0: grey 0x18, 0x30, 0x40, (hold), 0x30, 0x18, then 0 and done
	static void SpritePhase(SpriteNode *p)
	{
		uint8_t g;
		switch (p->h.phase)
		{
		case 0: g = 0x18; break;
		case 1: g = 0x30; break;
		case 2: g = 0x40; break;
		case 3: p->h.phase++; return;
		case 4: g = 0x30; break;
		case 5: g = 0x18; break;
		case 6:
			*(uint8_t *)&p->h.flags |= 1;
			p->rgb[0] = 0;
			p->rgb[1] = 0;
			p->rgb[2] = 0;
			return;
		default: return;
		}
		p->rgb[0] = g;
		p->rgb[1] = g;
		p->rgb[2] = g;
		p->h.phase++;
	}

	static uint32_t __cdecl SpriteTask(TaskNode *t)
	{
		SpriteNode *p = (SpriteNode *)t;
		SpritePhase(p);
		p->ax = (int16_t)(CrtRand() & 0xFFF);
		const int32_t r = CrtRand() % 7;
		p->frame = (int16_t)((uint8_t)(r + (uint8_t)p->frame + 1) & 7);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_SPRITE, p);)
		DrawSpriteTurned(p);
		p->h.counter++;
		if ((p->h.flags & 1) && p->h.children == 0)
		{
			ReleaseLinkedTask(p);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Column (0x8D5E60)
	// ------------------------------------------------------------------
	// phases 0x8D6000 (fade in, x / z grow), 0x8D6040 / 0x8D6060 (y grows to 0x1800), 0x8D6090
	// (settles at 0x1000: flash, or the handshake for the flagged rune's column), 0x8D6100 (the
	// emitter's signal: second flash), 0x8D6120 (fade out, done), 0x8D6150
	static void ColumnPhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0:
			n->fade = (int16_t)(n->fade - 0x400);
			n->scale[0] += 0x200;
			n->scale[2] += 0x200;
			if (n->fade <= 0)
			{
				n->fade = 0;
				n->scale[0] = 0x800;
				n->scale[2] = 0x800;
				n->h.phase++;
			}
			break;
		case 1:
			n->scale[1] += 0x400;
			if (n->scale[1] >= 0x1000) n->h.phase++;
			break;
		case 2:
			n->scale[1] += 0x400;
			if (n->scale[1] >= 0x1800)
			{
				n->scale[1] = 0x1800;
				n->h.phase++;
			}
			break;
		case 3:
			n->scale[0] += 0x400;
			n->scale[1] -= 0x400;
			n->scale[2] += 0x400;
			if (n->scale[1] <= 0x1000)
			{
				n->scale[0] = 0x1000;
				n->scale[1] = 0x1000;
				n->scale[2] = 0x1000;
				if (n->flagged == 0) n->flash = 1;
				else ((EmitterNode *)n->h.emitter)->signal = 1;
				n->h.phase++;
			}
			break;
		case 4:
			if (((EmitterNode *)n->h.emitter)->signal == 2)
			{
				n->flash = 3;
				n->h.phase++;
			}
			break;
		case 5:
			if (n->flash == 0)
			{
				n->fade = (int16_t)(n->fade + 0x200);
				if (n->fade >= 0x1000)
				{
					*(uint8_t *)&n->h.flags |= 1;
					n->fade = 0x1000;
					n->h.phase++;
				}
			}
			break;
		default: break;
		}
	}

	// the flash state: 1 / 3 start the flash model at fade 0x400 / 0x800 (white), 2 / 4 fade it by
	// 0x100 / 0x200 per tick, then off
	static void ColumnFlash(ModelNode *n)
	{
		const int16_t s = n->flash;
		switch (s)
		{
		case 1:
		case 3:
			n->flash_on = 1;
			n->fade = (int16_t)(s == 1 ? 0x400 : 0x800);
			n->rgb[0] = 0xFF;
			n->rgb[1] = 0xFF;
			n->rgb[2] = 0xFF;
			n->flash = (int16_t)(s + 1);
			return;
		case 2:
			n->fade = (int16_t)(n->fade - 0x100);
			if (n->fade > 0) return;
			break;
		case 4:
			n->fade = (int16_t)(n->fade - 0x200);
			if (n->fade > 0) return;
			break;
		default: return;
		}
		n->fade = 0;
		n->rgb[0] = 0;
		n->rgb[1] = 0;
		n->rgb[2] = 0;
		n->flash_on = 0;
		n->flash = 0;
	}

	// spin, and the place on the circle (radius * (sin, cos) / 0x4000 of the orbit angle)
	static void ColumnMotion(ModelNode *n)
	{
		const int16_t orbit = (int16_t)((n->orbit_step + n->orbit) & 0xFFF);
		n->angle[1] = (int16_t)((n->angle[1] + 0x140) & 0xFFF);
		n->orbit = orbit;
		const int32_t dx = mul32(ComputeSin(n->orbit), n->radius) / 0x4000;
		const int32_t dz = mul32(ComputeCos(n->orbit), n->radius) / 0x4000;
		memcpy(n->h.pos, n->base, 8);
		n->h.pos[0] = (int16_t)(n->h.pos[0] + dx);
		n->h.pos[2] = (int16_t)(n->h.pos[2] + dz);
	}

	// the column's two draws: the flash model over it while the flash is on, then the column
	static void ColumnDraw(ModelNode *n)
	{
		if (n->flash_on == 1)
		{
			n->model = MODEL_ColumnFlash;
			DrawModel(n);
			n->model = MODEL_Column;
		}
		DrawModel(n);
	}

	static uint32_t __cdecl ColumnTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		ColumnPhase(n);
		ColumnFlash(n);
		ColumnMotion(n);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_COLUMN, n);)
		ColumnDraw(n);
		n->h.counter++;
		if ((n->h.flags & 1) && n->h.children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Glow (0x8D6200)
	// ------------------------------------------------------------------
	static void AddGrey(SpriteNode *p, uint8_t d)
	{
		p->rgb[1] = (uint8_t)(p->rgb[1] + d);
		p->rgb[0] = (uint8_t)(p->rgb[0] + d);
		p->rgb[2] = (uint8_t)(p->rgb[2] + d);
	}

	// phases 0x8D63F0 (grey up by 4 to 0x20), 0x8D6420 (the emitter's signal: pulse), 0x8D6440
	// (grey down by 4 once the pulse is over, then hidden and done), 0x8D6480
	static void GlowPhase(SpriteNode *p)
	{
		switch (p->h.phase)
		{
		case 0:
			AddGrey(p, 4);
			if (p->rgb[0] >= 0x20) p->h.phase++;
			break;
		case 1:
			if (((EmitterNode *)p->h.emitter)->signal == 2)
			{
				p->pulse = 3;
				p->h.phase++;
			}
			break;
		case 2:
			if (p->pulse == 0)
			{
				AddGrey(p, 0xFC);
				if (p->rgb[0] == 0)
				{
					*(uint8_t *)&p->h.flags |= 5;
					p->h.phase++;
				}
			}
			break;
		default: break;
		}
	}

	// the pulse: 1 / 3 set 0x40, 2 / 4 fade by 4 per tick back to 0x20
	static void GlowPulse(SpriteNode *p)
	{
		switch (p->pulse)
		{
		case 1:
		case 3:
			p->rgb[0] = 0x40;
			p->rgb[1] = 0x40;
			p->rgb[2] = 0x40;
			p->pulse = (int16_t)(p->pulse + 1);
			break;
		case 2:
		case 4:
			AddGrey(p, 0xFC);
			if (p->rgb[0] <= 0x20) p->pulse = 0;
			break;
		default: break;
		}
	}

	// on the rune's circle (radius 0x400: (sin, cos) / 4), on the ground
	static void GlowPlace(SpriteNode *p)
	{
		memcpy(p->h.pos, p->base, 8);
		p->h.pos[0] = (int16_t)(p->h.pos[0] + ComputeSin(p->angle) / 4);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + ComputeCos(p->angle) / 4);
	}

	static uint32_t __cdecl GlowTask(TaskNode *t)
	{
		SpriteNode *p = (SpriteNode *)t;
		GlowPhase(p);
		GlowPulse(p);
		GlowPlace(p);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_GLOW, p);)
		DrawGroundGlow(p, SEQ_Glow, (const uint32_t *)p->rgb);
		p->h.counter++;
		if ((p->h.flags & 1) && p->h.children == 0)
		{
			ReleaseLinkedTask(p);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Mote (0x8D6490)
	// ------------------------------------------------------------------
	// sub_8D6530: + r * (sin, cos)(angle) / 4096 in x / z
	static void Orbit(SpriteNode *p, int16_t r, int16_t angle)
	{
		p->h.pos[0] = (int16_t)(p->h.pos[0] + mul32(ComputeSin(angle), r) / 4096);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + mul32(ComputeCos(angle), r) / 4096);
	}

	// spiral in: angle +0x60, grey +0x20 up to 0x80, radius -0x60 (done when gone)
	static void MoteUpdate(SpriteNode *p)
	{
		p->ay = (int16_t)((p->ay + 0x60) & 0xFFF);
		if (p->rgb[0] < 0x80)
		{
			p->rgb[0] = (uint8_t)(p->rgb[0] + 0x20);
			p->rgb[1] = (uint8_t)(p->rgb[1] + 0x20);
			p->rgb[2] = (uint8_t)(p->rgb[2] + 0x20);
		}
		p->radius = (int16_t)(p->radius - 0x60);
		if (p->radius <= 0)
		{
			*(uint8_t *)&p->h.flags |= 1;
			p->h.phase++;
		}
	}

	static void MotePlace(SpriteNode *p)
	{
		memcpy(p->h.pos, p->base, 8);
		Orbit(p, p->radius, p->ay);
	}

	static uint32_t __cdecl MoteTask(TaskNode *t)
	{
		SpriteNode *p = (SpriteNode *)t;
		MoteUpdate(p);
		MotePlace(p);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_MOTE, p);)
		DrawSpriteFacing(p);
		// sub_8D65F0: next flipbook frame (looping)
		p->frame++;
		if (p->frame > p->last) p->frame = 0;
		p->h.counter++;
		if ((p->h.flags & 1) && p->h.children == 0)
		{
			ReleaseLinkedTask(p);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Rune (0x8D5C80)
	// ------------------------------------------------------------------
	// sub_8D5D50: the rune's column
	static void SpawnColumn(ModelNode *r)
	{
		ModelNode *c = (ModelNode *)AddEffectTask(QModels(), ORIG_Column, 0x6C, r);
		c->radius = 0x1000;
		c->flash_on = 0;
		c->flagged = r->flagged;
		c->arg = r->arg;
		CopyAnchorXZFromSource(c, c->h.pos);
		c->h.pos[1] = (int16_t)(c->h.pos[1] - 0x280);
		memcpy(c->base, c->h.pos, 8);
		c->orbit = r->orbit;
		c->angle[1] = r->orbit;
		c->arg = r->arg;
		c->model = MODEL_Column;
		c->fade = 0x1000;
		c->scale[0] = 0x10;
		c->scale[1] = 0x10;
		c->scale[2] = 0x10;
	}

	// sub_8D5DE0: a mote per tick for ticks 0..6
	static void SpawnMote(ModelNode *r)
	{
		if (r->h.counter < 0 || r->h.counter > 6) return;
		SpriteNode *p = (SpriteNode *)AddEffectTask(QSprites(), ORIG_Mote, 0x68, r);
		memcpy(p->base, p->h.pos, 8);
		p->seq = SEQ_Mote;
		p->radius = (int16_t)((CrtRand() & 0x1FF) + 0x200);
		p->last = 0xC;
		p->ay = (int16_t)(CrtRand() & 0xFFF);
		p->scale[0] = 0x1000;
		p->scale[1] = 0x1000;
		p->scale[2] = 0x1000;
	}

	// phases 0x8D6160 (fade in, spawning), 0x8D6190 (shrink and fade out, then hidden and done), 0x8D61F0
	static void RunePhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0:
			n->fade = (int16_t)(n->fade - 0x200);
			*(uint8_t *)&n->h.flags |= 2;
			if (n->fade <= 0)
			{
				n->fade = 0;
				n->h.phase++;
			}
			break;
		case 1:
			n->fade = (int16_t)(n->fade + 0x200);
			n->scale[0] -= 0x200;
			n->scale[1] -= 0x200;
			n->scale[2] -= 0x200;
			if (n->fade >= 0x1000)
			{
				n->fade = 0x1000;
				n->scale[0] = 0x10;
				n->scale[1] = 0x10;
				n->scale[2] = 0x10;
				n->h.flags = (uint16_t)((n->h.flags & 0xFFFD) | 5);
				n->h.phase++;
			}
			break;
		default: break;
		}
	}

	// spin, and the place on the circle (radius 0x400: (sin, cos) / 4 of the orbit angle)
	static void RuneMotion(ModelNode *n)
	{
		n->angle[1] = (int16_t)((n->angle[1] + 0x140) & 0xFFF);
		memcpy(n->h.pos, n->base, 8);
		n->h.pos[0] = (int16_t)(n->h.pos[0] + ComputeSin(n->orbit) / 4);
		n->h.pos[2] = (int16_t)(n->h.pos[2] + ComputeCos(n->orbit) / 4);
	}

	static uint32_t __cdecl RuneTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		RunePhase(n);
		RuneMotion(n);
		if (n->h.flags & 2)
		{
			if (n->h.counter == 3) SpawnColumn(n);
			SpawnMote(n);
		}
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_RUNE, n);)
		DrawModel(n);
		n->h.counter++;
		if ((n->h.flags & 1) && n->h.children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Sequencer (0x8D5B40)
	// ------------------------------------------------------------------
	// sub_8D5BD0: a rune at `angle` on the circle around the target (0x280 up) and its glow
	static void SpawnRune(SeqNode *s, int32_t angle, int16_t arg, int16_t flagged)
	{
		ModelNode *r = (ModelNode *)AddEffectTask(QModels(), ORIG_Rune, 0x6C, s);
		CopyAnchorXZFromSource(r, r->h.pos);
		r->h.pos[1] = (int16_t)(r->h.pos[1] - 0x280);
		memcpy(r->base, r->h.pos, 8);
		const int16_t a = (int16_t)(angle & 0xFFF);
		r->arg = arg;
		r->flagged = flagged;
		r->orbit = a;
		r->model = MODEL_Rune;
		r->fade = 0x1000;
		r->scale[0] = 0x1000;
		r->scale[1] = 0x1000;
		r->scale[2] = 0x1000;
		SpriteNode *g = (SpriteNode *)AddEffectTask(QSprites(), ORIG_Glow, 0x68, s);
		CopyAnchorXZFromSource(g, g->h.pos);
		memcpy(g->base, g->h.pos, 8);
		g->angle = a;
	}

	static uint32_t __cdecl SequencerTask(TaskNode *t)
	{
		SeqNode *s = (SeqNode *)t;
		switch (s->h.phase)
		{
		case 0: SpawnRune(s, 0, 0, 0); s->h.phase++; break;       // 0x8D5BB0
		case 1: SpawnRune(s, 0x333, 0, 0); s->h.phase++; break;   // 0x8D6610
		case 2: SpawnRune(s, 0x666, 0, 0); s->h.phase++; break;   // 0x8D6640
		case 3: SpawnRune(s, 0x999, 0, 0); s->h.phase++; break;   // 0x8D6670
		case 4: // 0x8D66A0: the flagged last rune, then done
			SpawnRune(s, 0xCCC, 0, 1);
			*(uint8_t *)&s->h.flags |= 1;
			s->h.phase++;
			break;
		default: break; // 0x8D66D0
		}
		s->h.counter++;
		if ((s->h.flags & 1) && s->h.children == 0)
		{
			ReleaseLinkedTask(s);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Flash (0x8D6790)
	// ------------------------------------------------------------------
	// MAG_004_sub_8D6710: on the target's top anchor, unless the target record's +3 bit 2 is set
	static void SpawnFlash(EmitterNode *e)
	{
		const uint8_t *targets = e->h.ctx->actions[e->h.action].targets;
		if (targets[3] & 4) return;
		FlashNode *f = (FlashNode *)AddEffectTask(QFlashes(), ORIG_Flash, 0x50, e);
		CopyAnchorTopFromSource(f, f->h.pos);
		memcpy(f->target, f->h.pos, 8);
		f->tx = 0x240;
		f->ty = 0x120;
		f->cx = 0x140;
		f->cy = 0xF4;
		f->u = 0;
		f->v = 0x20;
		f->tw = 0x28;
		f->th = 0x10;
	}

	// 0: jump (y velocity -0x90); 1: falls (+0x28 per tick) back to its rest height; 2: done at 15
	static void FlashPhase(FlashNode *f)
	{
		switch (f->h.phase)
		{
		case 0:
			f->vel = (int16_t)-0x90;
			f->h.phase++;
			break;
		case 1:
			f->vel = (int16_t)(f->vel + 0x28);
			f->h.pos[1] = (int16_t)(f->h.pos[1] + f->vel);
			if (f->h.pos[1] >= f->target[1])
			{
				f->h.pos[1] = f->target[1];
				f->h.phase++;
			}
			break;
		case 2:
			if (f->h.counter >= 0xF)
			{
				*(uint8_t *)&f->h.flags |= 1;
				f->h.phase++;
			}
			break;
		default: break;
		}
	}

	// a semi-transparent textured quad (POLY_FT4, raw texture) tw x th centred on the projected
	// position (screen coordinates / 8), its bottom edge there; straight into the render list
	static void FlashDraw(const FlashNode *f)
	{
		uint8_t *p = (uint8_t *)PacketCursor();
		const int32_t ty = f->ty;
		uint32_t tp = ((uint32_t)(uint16_t)f->tx & 0x3C0) | 0x2800;
		tp = (uint32_t)(uint16_t)((int16_t)tp >> 6);
		tp |= (uint32_t)(ty >> 4) & 0x10;
		tp |= ((uint32_t)ty & 0x200) << 2;
		F<uint16_t>(p, 0x16) = (uint16_t)tp;
		const uint8_t v = (uint8_t)f->v;
		F<uint8_t>(p, 0xD) = v;
		F<uint16_t>(p, 0xE) = (uint16_t)((((uint32_t)(uint16_t)f->cx >> 4) & 0x3F) | (uint16_t)((uint16_t)f->cy << 6));
		const uint8_t u = (uint8_t)f->u;
		F<uint8_t>(p, 0xC) = u;
		F<uint8_t>(p, 0x1C) = u;
		const uint8_t u1 = (uint8_t)(u + (uint8_t)f->tw);
		F<uint8_t>(p, 0x15) = v;
		const uint8_t v1 = (uint8_t)(v + (uint8_t)f->th);
		F<uint32_t>(p, 0) = 0x9000000;
		F<uint8_t>(p, 7) = 0x2F;
		F<uint8_t>(p, 0x14) = u1;
		F<uint8_t>(p, 0x1D) = v1;
		F<uint8_t>(p, 0x24) = u1;
		F<uint8_t>(p, 0x25) = v1;
		GteSetTransVectorCtrl(&Camera());
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(f->h.pos);
		GteRTPS();
		int16_t sxy[4];
		GteReadSXY2(sxy);
		sxy[0] = (int16_t)(sxy[0] >> 3);
		sxy[1] = (int16_t)(sxy[1] >> 3);
		const int32_t half = f->tw / 2;
		const int16_t hh = f->th;
		F<int16_t>(p, 8) = (int16_t)(sxy[0] - half);
		F<int16_t>(p, 0xA) = (int16_t)(sxy[1] - hh);
		F<int16_t>(p, 0x10) = (int16_t)(sxy[0] + half);
		F<int16_t>(p, 0x12) = (int16_t)(sxy[1] - hh);
		F<int16_t>(p, 0x18) = (int16_t)(sxy[0] - half);
		F<int16_t>(p, 0x1A) = sxy[1];
		F<int16_t>(p, 0x20) = (int16_t)(sxy[0] + half);
		F<int16_t>(p, 0x22) = sxy[1];
		InsertPrimAltViewport(var<uint32_t>(0x1D8E04C), p);
		PacketCursor() = (uint32_t)(p + 0x28);
	}

	static uint32_t __cdecl FlashTask(TaskNode *t)
	{
		FlashNode *f = (FlashNode *)t;
		FlashPhase(f);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_FLASH, f);)
		FlashDraw(f);
		f->h.counter++;
		if ((f->h.flags & 1) && f->h.children == 0)
		{
			ReleaseLinkedTask(f);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Emitter (0x8D52A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl EmitterTask(TaskNode *t)
	{
		EmitterNode *e = (EmitterNode *)t;
		EmitterUpdatePos(e);
		ComputeModelBounds(e);
		switch (e->h.phase)
		{
		case 0: // 0x8D5B10: the rune sequencer
			AddEffectTask(QSequencers(), ORIG_Sequencer, 0x40, e);
			e->h.phase++;
			break;
		case 1: // 0x8D66E0: at 19 the root goes on, the flash
			if (e->h.counter >= 0x13)
			{
				((RootNode *)e->h.root)->busy = 0;
				SpawnFlash(e);
				e->h.phase++;
			}
			break;
		case 2: // 0x8D6940: the action result, done
			ApplyActionResultToTarget(e->h.ctx->actions[e->h.action].targets);
			*(uint8_t *)&e->h.flags |= 1;
			e->h.phase++;
			break;
		default: break; // 0x8D6970
		}
		if (e->h.counter == 0) BdPlaySE(SOUND_Double, 0, 0x80);
		switch (e->signal)
		{
		case 1: // the last rune's column settled: screen fade and the big rune
			ScreenFadeTask(0, 1, 2, 0x80);
			SpawnBigRune(e);
			e->signal++;
			break;
		case 2:
			e->signal = 0;
			break;
		default: break;
		}
		e->h.counter++;
		if ((e->h.flags & 1) && e->h.children == 0)
		{
			ReleaseLinkedTask(e);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root (0x8D5180)
	// ------------------------------------------------------------------
	static void RootPhase(RootNode *r)
	{
		switch (r->h.phase)
		{
		case 0: // 0x8D5260
			r->h.phase++;
			break;
		case 1: // 0x8D5270: the emitter of the current action
			r->busy = 1;
			AddEffectTask(QEmitters(), ORIG_Emitter, 0x5C, r);
			r->h.phase++;
			break;
		case 2: // 0x8D6980: when the action is over, the next one (back to 1) or on
			if (r->busy == 0)
			{
				const int8_t a = r->h.action;
				if ((int16_t)a < r->last_action)
				{
					r->h.action = (int8_t)(a + 1);
					r->h.phase--;
				}
				else r->h.phase++;
			}
			break;
		case 3: // 0x8D69B0: every queue empty, done
			if (r->alive == 0)
			{
				*(uint8_t *)&r->h.flags |= 1;
				r->h.phase++;
			}
			break;
		default: break; // 0x8D69D0
		}
	}

	static uint32_t __cdecl RootTask(TaskNode *t)
	{
		RootNode *r = (RootNode *)t;
		if (r->tick & 1) PacketCursor() = var<uint32_t>(ARENA_A);
		else PacketCursor() = var<uint32_t>(ARENA_B);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note_root(r);)
		UpdateTargetPosFromBones(r);
		RootPhase(r);
		r->alive = (int16_t)ExecuteTaskQueue(QEmitters());
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QSequencers()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QSprites()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QModels()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QFlashes()));
		r->tick++;
		r->h.counter++;
		if ((r->h.flags & 1) && r->h.children == 0)
		{
			ReleaseLinkedTask(r);
			return TASK_END;
		}
		return 0;
	}
}

	void register_mag004_double()
	{
		register_port(double004::ORIG_Root, (void *)double004::RootTask, "004 Root", 4);
		register_port(double004::ORIG_Emitter, (void *)double004::EmitterTask, "004 Emitter", 4);
		register_port(double004::ORIG_Sequencer, (void *)double004::SequencerTask, "004 Sequencer", 4);
		register_port(double004::ORIG_Rune, (void *)double004::RuneTask, "004 Rune", 4);
		register_port(double004::ORIG_Column, (void *)double004::ColumnTask, "004 Column", 4);
		register_port(double004::ORIG_Glow, (void *)double004::GlowTask, "004 Glow", 4);
		register_port(double004::ORIG_Mote, (void *)double004::MoteTask, "004 Mote", 4);
		register_port(double004::ORIG_BigRune, (void *)double004::BigRuneTask, "004 BigRune", 4);
		register_port(double004::ORIG_Morph, (void *)double004::MorphTask, "004 Morph", 4);
		register_port(double004::ORIG_Sprite, (void *)double004::SpriteTask, "004 Sprite", 4);
		register_port(double004::ORIG_Flash, (void *)double004::FlashTask, "004 Flash", 4);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(register_mag004_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag004_double_held.inc"
#endif
