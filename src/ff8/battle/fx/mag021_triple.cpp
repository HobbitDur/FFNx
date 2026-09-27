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

// Effect 21: Triple (spell, MAG_021_TRIPLE_*), the bigger sibling of Double (mag004_double.cpp).
//
// Structure (module code 0x89DED0..0x89FBA0: file loader MAG_021_TRIPLE_FL 0x89DED0 = the texture
// file (pointer 0x2714F28); setup MAG_021_TRIPLE 0x89DEF0 sets up the queues and the root node,
// starts camera animation 0x1629288, queues the TIM and takes the two packet arenas: texture
// buffer + 0 (0x2720A1C) / + 0x8000 (0x2720A18)). Every node starts with the shared effect-library
// header (Effect_AddTaskAndInitFromCtx 0x8DC540); every task runs its current phase function
// (node +0x29) from a small table and ends when it is flagged done (+0x26 bit0) and has no live
// children. Every task updates THEN draws; none tests the draw-only flags (battle_to_update_flags
// 0x201).
//   Root (0x89DFF0) - packet arena by tick parity (cursor 0x2714F2C), follows the caster's bones;
//     phases: 0 advance, 1 spawns the emitter of the current action (marks the root busy), 2 waits
//     while busy, then the next action (back to 1) or on, 3 waits until the five queues are empty.
//     Queues run: emitters, sequencers, sprites, models, flashes.
//   Emitter (0x89E110), one per action: target anchor and model bounds (effect library 0x8DC610 /
//     0x8DC870); phase 0 spawns the rune sequencer, phase 1 at tick 24 releases the root and spawns
//     the rising flash, phase 2 applies the action result. Sound at its first tick (after reading
//     the middle anchor into an unused local). Its word +0x58 is a handshake with the last rune:
//     set to 1 by the rune's column, the emitter then starts a screen fade (au_re_BdLinkTask_6) and
//     the big rune and sets 2 (every column and glow reacts in that tick), then 0.
//   Sequencer (0x89EA70) - two opposite runes every 3 ticks: angles 0 / 0x800, 0x200 / 0xA00,
//     0x400 / 0xC00, 0x600 / 0xE00 (the last one flagged), then done.
//   Rune (0x89EBD0, prim model 0x1626CF0) - on a circle of radius 0x400 around the target,
//     spinning; fades in (0x1000 -> 0 in 8 ticks), then shrinks while fading out and ends hidden.
//     While spawning a column (tick 3) and a light mote per tick (ticks 0..6).
//   Column (0x89EDB0, prim model 0x1626F00, flash model 0x1627160) - grows in x/z, then in y, then
//     settles; the flagged rune's column starts the handshake; flashes once when it settles and once
//     on the emitter's signal, then fades out (hidden at the end) sending up rising sparks while its
//     fade is <= 0xA00.
//   Spark (0x89F5E0, flipbook 0x1626C18) - from the column's place, pushed out by a random turn
//     (x, 2 y, z of a random length < 0x80), rises at a random speed until its flipbook ends.
//   Glow (0x89F180, sprite 0x1626CC8 lying on the ground under the rune) - grey ramp up to 0x20,
//     a 0x40 pulse on the emitter's signal, then out.
//   Mote (0x89F410, flipbook 0x16269DC) - spirals in (radius -0x60, angle +0x60 per tick) and
//     brightens up to 0x80, until its radius is gone.
//   Big rune (0x89E250, prim models 0x1627380 and 0x1628218) - the first fades in, then both follow
//     the fade tables 0x1629C2C / 0x1629C14 (11 ticks); while the first fades in, eight sprites
//     (0x89E7D0: four around the rune, four between them) per tick for ticks 2..4 and a morphing ring
//     (0x89E670) at tick 0.
//   Morphing ring (0x89E670, prim model 0x1627970) - its vertices are blended between the vertex
//     sets 0x16276E8 and 0x1629000 by the fade table 0x1629C2C (GTE GPF / GPL, written into the model
//     in the exe data) before each draw.
//   Sprite (0x89E7D0, flipbook 0x1626B3C) - random flipbook frame and X angle every tick, a grey
//     ramp 0x18 / 0x30 / 0x40 / 0x40 / 0x30 / 0x18, then done.
//   Flash (0x89F950) - a screen-space textured quad (tpage 0x240/0x120, CLUT 0x140/0xF4, 0x28 x 0x10
//     texels) on the target's top anchor that jumps up and falls back, drawn straight into the
//     render list (no +0x44), for 15 ticks. Not spawned when the target record's +3 bit 2 is set.
// Module globals: 0x2714F28..0x2721580 (texture file pointer 0x2714F28, packet cursor 0x2714F2C,
// pools / queues: models 0x2714F30 / 0x271B010 (200 x 0x7C), root 0x271B028 / 0x271B0F0 (2 x 0x64),
// sprites 0x271B100 / 0x2720880 (200 x 0x70), emitters 0x27208A8 / 0x2720898 (4 x 0x5C), arenas
// 0x2720A18 / 0x2720A1C, sequencers 0x2720A30 / 0x2720A20 (40 x 0x40), flashes 0x2721430 /
// 0x2721570 (4 x 0x50)); the morphing ring's vertex block in the exe data 0x1627978..0x1627BF8;
// the TransformCameraByShadowRotation scratch 0x21DFED0.

#include "mag_common.h"

namespace ff8fx
{
namespace triple021
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
		int16_t signal;        // +0x58 handshake with the last rune's column (1 -> 2 -> 0)
		int16_t pad5A;
	};
	struct SeqNode // pool of 40 nodes of 0x40 bytes
	{
		FxNode h;
		uint8_t pad30[0x10];
	};
	struct ModelNode // rune / column / big rune / morphing ring: pool of 200 nodes of 0x7C bytes
	{
		FxNode h;
		int32_t scale[3];      // +0x30
		uint32_t pad3C;
		uint8_t rgb[4];        // +0x40 prim-model colour of the draw (dword)
		int16_t angle[3];      // +0x44 rotation (y spins)
		int16_t pad4A;
		uint32_t model;        // +0x4C prim model of the draw
		int16_t fade;          // +0x50 prim-model fade of the draw
		int16_t pad52;
		int16_t base[4];       // +0x54 rune / column: circle centre
		uint8_t rgb1[4];       // +0x5C big rune: first model's colour
		uint8_t rgb2[4];       // +0x60 big rune: second model's colour
		int16_t fade1;         // +0x64 big rune: first model's fade
		int16_t fade2;         // +0x66 big rune: second model's fade
		int16_t orbit;         // +0x68 rune / column: angle on the circle
		int16_t flagged;       // +0x6A the last rune (column: copied)
		int16_t flash;         // +0x6C column: flash state (1 / 3 start, 2 / 4 fade)
		int16_t radius;        // +0x6E column: circle radius (4.12 of 0x400)
		int16_t flash_on;      // +0x70 column: flash model drawn
		int16_t orbit_step;    // +0x72 column: angle step (never written: 0)
		int16_t morph;         // +0x74 morphing ring: blend
		int16_t arg;           // +0x76 rune spawn argument (0)
		int16_t step;          // +0x78 big rune / morphing ring: fade table index
		int16_t pad7A;
	};
	struct SpriteNode // sprite / glow / mote / spark: pool of 200 nodes of 0x70 bytes
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
		int16_t pad60;
		int16_t vel;           // +0x62 spark: y velocity
		uint32_t pad64;
		int16_t radius;        // +0x68 mote: spiral radius
		int16_t angle;         // +0x6A glow: angle on the circle
		int16_t pulse;         // +0x6C glow: pulse state (1 / 3 start, 2 / 4 fade)
		int16_t pad6E;
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
	static_assert(sizeof(FxNode) == 0x30 && sizeof(RootNode) == 0x64 && sizeof(EmitterNode) == 0x5C, "Triple nodes");
	static_assert(sizeof(SeqNode) == 0x40 && sizeof(ModelNode) == 0x7C && sizeof(SpriteNode) == 0x70, "Triple nodes");
	static_assert(sizeof(FlashNode) == 0x50, "Triple nodes");

	// ------------------------------------------------------------------
	// Module globals
	// ------------------------------------------------------------------
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2714F2C); }
	inline TaskQueue *QEmitters() { return (TaskQueue *)0x2720898; }  // 4 x 0x5C
	inline TaskQueue *QSequencers() { return (TaskQueue *)0x2720A20; }// 40 x 0x40
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2720880; }   // 200 x 0x70
	inline TaskQueue *QModels() { return (TaskQueue *)0x271B010; }    // 200 x 0x7C
	inline TaskQueue *QFlashes() { return (TaskQueue *)0x2721570; }   // 4 x 0x50

	static const uint32_t ORIG_Root = 0x89DFF0;
	static const uint32_t ORIG_Emitter = 0x89E110;
	static const uint32_t ORIG_BigRune = 0x89E250;
	static const uint32_t ORIG_Morph = 0x89E670;
	static const uint32_t ORIG_Sprite = 0x89E7D0;
	static const uint32_t ORIG_Sequencer = 0x89EA70;
	static const uint32_t ORIG_Rune = 0x89EBD0;
	static const uint32_t ORIG_Column = 0x89EDB0;
	static const uint32_t ORIG_Glow = 0x89F180;
	static const uint32_t ORIG_Mote = 0x89F410;
	static const uint32_t ORIG_Spark = 0x89F5E0;
	static const uint32_t ORIG_Flash = 0x89F950;

	static const void *const SOUND_Triple = (const void *)0x16269D8;
	static const uint32_t MODEL_BigRune1 = 0x1627380, MODEL_BigRune2 = 0x1628218, MODEL_Rune = 0x1626CF0;
	static const uint32_t MODEL_Column = 0x1626F00, MODEL_ColumnFlash = 0x1627160, MODEL_Morph = 0x1627970;
	static const uint32_t MORPH_A = 0x16276E8, MORPH_B = 0x1629000;
	static const uint32_t SEQ_Sprite = 0x1626B3C, SEQ_Glow = 0x1626CC8, SEQ_Mote = 0x16269DC, SEQ_Spark = 0x1626C18;
	static const uint32_t FADE_TABLE_A = 0x1629C2C, FADE_TABLE_B = 0x1629C14; // 12 words each (0..0x1000)
	static const uint32_t ARENA_A = 0x2720A1C, ARENA_B = 0x2720A18;

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
	inline void CopyAnchorXZFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC6E0)(n, out); }   // emitter +0x30
	inline void CopyMidpointFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC700)(n, out); }   // emitter +0x38
	inline void CopyAnchorTopFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC720)(n, out); }  // emitter +0x40
	inline void IdentityMatrix(Mat4x3 *m) { fn<void (__cdecl *)(Mat4x3 *)>(0x8DD770)(m); }
	inline void RotateX(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD7E0)(m, a); }
	inline void RotateY(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD8A0)(m, a); }
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void InsertPrimAltViewport(uint32_t ot, void *prim) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(ot, prim); }

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }

	// sub_89E620 / sub_89E5E0: fade table entry (index clamped to 0..11)
	static int16_t FadeTable(uint32_t table, int16_t k)
	{
		if (k > 0xB) k = 0xB;
		else if (k < 0) k = 0;
		return ((const int16_t *)table)[k];
	}

	// ------------------------------------------------------------------
	// Drawing helpers
	// ------------------------------------------------------------------
	// sub_89E300 (= Double 0x8D5440): the node's prim model at its position, rotation (+0x44) and
	// scale (+0x30), in its colour (+0x40) and fade (+0x50)
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

	// sub_89E880 (= Double 0x8D5950): a sprite's flipbook frame turned by its Y then X angle, scaled
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

	// sub_89F2B0 (= MAG_001_CURE_DrawAdditiveGlow): flipbook `seq` frame 0 lying on the ground under
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

	// sub_89F500 (= MAG_001_CURE_DrawSprite): the node's flipbook frame at its position (camera-facing)
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

	// sub_89E6E0 (= Double 0x8D57C0): vertex blend out = a * (1 - t) + b * t (4.12) of the vertex
	// blocks (+4 = count, 8 bytes per vertex from +8), through the GTE (GPF then GPL)
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
	// Big rune (0x89E250) and what it spawns
	// ------------------------------------------------------------------
	// sub_89E3C0: while fading in (ticks 2..4) four sprites around the rune, random x in
	// -0x200..0x1FF, 0x2C0 back: mode 0 at angles k * 0x400, mode 1 at k * 0x400 + 0x200
	static void SpawnSprites(ModelNode *n, int16_t mode)
	{
		if (n->h.counter < 2 || n->h.counter > 4) return;
		for (int32_t i = 0; i < 4; i++)
		{
			const int32_t k = (i + (int32_t)n->h.counter * 4) % 4;
			SpriteNode *p = (SpriteNode *)AddEffectTask(QSprites(), ORIG_Sprite, 0x70, n);
			p->scale[0] = 0x1000;
			p->scale[1] = 0x1000;
			p->scale[2] = 0x1000;
			p->seq = SEQ_Sprite;
			p->last = 7;
			int16_t v[4];
			if (mode == 0)
			{
				const int32_t r = CrtRand() % 0x400;
				v[1] = 0;
				v[0] = (int16_t)(r - 0x200);
				v[2] = (int16_t)-0x2C0;
				p->ay = (int16_t)(k << 10);
			}
			else if (mode == 1)
			{
				const int32_t r = CrtRand() % 0x400;
				v[1] = 0;
				v[0] = (int16_t)(r - 0x200);
				v[2] = (int16_t)-0x2C0;
				p->ay = (int16_t)((k << 10) + 0x200);
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

	// sub_89E500: a morphing ring at tick 0
	static void SpawnMorph(ModelNode *n)
	{
		if (n->h.counter != 0) return;
		ModelNode *m = (ModelNode *)AddEffectTask(QModels(), ORIG_Morph, 0x7C, n);
		CopyAnchorXZFromSource(m, m->h.pos);
		m->h.pos[1] = (int16_t)(m->h.pos[1] - 0x280);
		m->angle[1] = 0;
		m->model = MODEL_Morph;
		m->fade = 0;
		m->scale[0] = 0x1000;
		m->scale[1] = 0x1000;
		m->scale[2] = 0x1000;
		m->rgb[0] = 0;
		m->rgb[1] = 0;
		m->rgb[2] = 0;
	}

	// MAG_021_sub_89E1E0 (the emitter's signal): the big rune on the target
	static void SpawnBigRune(EmitterNode *e)
	{
		ModelNode *n = (ModelNode *)AddEffectTask(QModels(), ORIG_BigRune, 0x7C, e);
		CopyAnchorXZFromSource(n, n->h.pos);
		n->h.pos[1] = (int16_t)(n->h.pos[1] - 0x280);
		n->scale[0] = 0x1000;
		n->scale[1] = 0x1000;
		n->scale[2] = 0x1000;
		n->angle[1] = 0;
		n->model = MODEL_BigRune1;
		n->fade1 = 0x800;
		n->fade2 = 0;
		n->rgb1[0] = 0xFF;
		n->rgb1[1] = 0xFF;
		n->rgb1[2] = 0xFF;
		n->rgb2[0] = 0;
		n->rgb2[1] = 0;
		n->rgb2[2] = 0;
	}

	// phases 0x89E560 (first model fades in, spawning), 0x89E590 (both models follow the fade
	// tables, then hidden and done), 0x89E660
	static void BigRunePhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0:
			n->fade1 = (int16_t)(n->fade1 - 0x200);
			*(uint8_t *)&n->h.flags |= 2;
			if (n->fade1 <= 0)
			{
				n->fade1 = 0;
				n->rgb1[0] = 0;
				n->rgb1[1] = 0;
				n->rgb1[2] = 0;
				n->h.phase++;
			}
			break;
		case 1:
		{
			n->step++;
			const int16_t a = FadeTable(FADE_TABLE_A, n->step);
			n->fade = a;
			n->fade1 = a;
			const int16_t b = FadeTable(FADE_TABLE_B, n->step);
			n->fade = b;
			n->fade2 = b;
			if (n->step >= 0xB)
			{
				n->h.flags = (uint16_t)((n->h.flags & 0xFFFD) | 5);
				n->h.phase++;
			}
			break;
		}
		default: break;
		}
	}

	// the big rune's two draws: the first model in its colour / fade, then the second
	static void BigRuneDraw(ModelNode *n)
	{
		memcpy(n->rgb, n->rgb1, 4);
		n->fade = n->fade1;
		n->model = MODEL_BigRune1;
		DrawModel(n);
		memcpy(n->rgb, n->rgb2, 4);
		n->fade = n->fade2;
		n->model = MODEL_BigRune2;
		DrawModel(n);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag021_triple_held.h"
#endif

namespace ff8fx
{
namespace triple021
{
	static uint32_t __cdecl BigRuneTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		BigRunePhase(n);
		// 30 fps layer: see mag021_triple_held.inc
		FX_HELD(held_note(REC_BIG, n);)
		BigRuneDraw(n);
		if (n->h.flags & 2)
		{
			SpawnSprites(n, 0);
			SpawnSprites(n, 1);
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
	// Morphing ring (0x89E670)
	// ------------------------------------------------------------------
	// phases 0x89E780 (blend and fade from the fade table, done at its end), 0x89E7C0
	static void MorphPhase(ModelNode *n)
	{
		if (n->h.phase != 0) return;
		n->step++;
		const int16_t a = FadeTable(FADE_TABLE_A, n->step);
		n->fade = a;
		n->morph = a;
		if (n->step >= 0xB)
		{
			*(uint8_t *)&n->h.flags |= 1;
			n->h.phase++;
		}
	}

	static uint32_t __cdecl MorphTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		MorphPhase(n);
		// 30 fps layer: see mag021_triple_held.inc
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
	// Sprite (0x89E7D0)
	// ------------------------------------------------------------------
	// phases 0x89E960.. 0x89EA10: grey 0x18, 0x30, 0x40, (hold), 0x30, 0x18, then 0 and done
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
		// 30 fps layer: see mag021_triple_held.inc
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
	// Spark (0x89F5E0)
	// ------------------------------------------------------------------
	// sub_89F6A0: pushed out by a random turn (yaw, pitch) along z by rand() % n (n = 0 counts
	// as 1): x, 2 * y, z of the turned vector
	static void Scatter(SpriteNode *p, int16_t n)
	{
		int32_t d = n;
		if (n == 0) d = 1;
		const int32_t yaw = CrtRand() & 0xFFF;
		const int32_t pitch = CrtRand() & 0xFFF;
		Mat4x3 m;
		IdentityMatrix(&m);
		RotateY(&m, (int16_t)yaw);
		RotateX(&m, (int16_t)pitch);
		int16_t v[4];
		int16_t out[4];
		v[0] = 0;
		v[1] = 0;
		v[2] = (int16_t)(CrtRand() % (int16_t)d);
		MatrixMulVector(&m, v, out);
		p->h.pos[0] = (int16_t)(p->h.pos[0] + out[0]);
		p->h.pos[1] = (int16_t)(p->h.pos[1] + (int16_t)(out[1] * 2));
		p->h.pos[2] = (int16_t)(p->h.pos[2] + out[2]);
	}

	// sub_89F790: next flipbook frame; past the last one the node is hidden (frame stays last), 1
	static int32_t LifetimeExpired(SpriteNode *p)
	{
		p->frame++;
		if (p->frame > p->last)
		{
			*(uint8_t *)&p->h.flags |= 4;
			p->frame = p->last;
			return 1;
		}
		return 0;
	}

	// phase 1 (0x89F760): rises by its velocity, done at the last frame
	static void SparkMove(SpriteNode *p)
	{
		p->h.pos[1] = (int16_t)(p->h.pos[1] + p->vel);
		if (LifetimeExpired(p))
		{
			*(uint8_t *)&p->h.flags |= 1;
			p->h.phase++;
		}
	}

	static uint32_t __cdecl SparkTask(TaskNode *t)
	{
		SpriteNode *p = (SpriteNode *)t;
		switch (p->h.phase)
		{
		case 0: // 0x89F640: at the column's place, scattered; white, 5 frames, rising speed 8..0x27
		{
			const FxNode *parent = p->h.parent;
			memcpy(p->h.pos, parent->pos, 8);
			Scatter(p, 0x80);
			p->last = 4;
			p->rgb[0] = 0x80;
			p->rgb[1] = 0x80;
			p->rgb[2] = 0x80;
			p->vel = (int16_t)(-8 - (CrtRand() & 0x1F));
			p->h.phase++;
			break;
		}
		case 1: SparkMove(p); break;
		default: break; // 0x89F7C0
		}
		// 30 fps layer: see mag021_triple_held.inc
		FX_HELD(held_note(REC_SPARK, p);)
		DrawSpriteFacing(p);
		p->h.counter++;
		if ((p->h.flags & 1) && p->h.children == 0)
		{
			ReleaseLinkedTask(p);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Column (0x89EDB0)
	// ------------------------------------------------------------------
	// phases 0x89EF50 (fade in, x / z grow), 0x89EF90 / 0x89EFB0 (y grows to 0x1800), 0x89EFE0
	// (settles at 0x1000: flash, or the handshake for the flagged rune's column), 0x89F050 (the
	// emitter's signal: second flash), 0x89F070 (fade out with sparks, then hidden and done),
	// 0x89F0D0. `spawn` = false: the sparks are not spawned (state only)
	static void ColumnPhase(ModelNode *n, bool spawn)
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
				if (n->fade <= 0xA00 && spawn)
				{
					SpriteNode *p = (SpriteNode *)AddEffectTask(QSprites(), ORIG_Spark, 0x70, n);
					p->seq = SEQ_Spark;
				}
				n->fade = (int16_t)(n->fade + 0x200);
				if (n->fade >= 0x1000)
				{
					*(uint8_t *)&n->h.flags |= 5;
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
		ColumnPhase(n, true);
		ColumnFlash(n);
		ColumnMotion(n);
		// 30 fps layer: see mag021_triple_held.inc
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
	// Glow (0x89F180)
	// ------------------------------------------------------------------
	static void AddGrey(SpriteNode *p, uint8_t d)
	{
		p->rgb[1] = (uint8_t)(p->rgb[1] + d);
		p->rgb[0] = (uint8_t)(p->rgb[0] + d);
		p->rgb[2] = (uint8_t)(p->rgb[2] + d);
	}

	// phases 0x89F370 (grey up by 4 to 0x20), 0x89F3A0 (the emitter's signal: pulse), 0x89F3C0
	// (grey down by 4 once the pulse is over, then hidden and done), 0x89F400
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
		// 30 fps layer: see mag021_triple_held.inc
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
	// Mote (0x89F410)
	// ------------------------------------------------------------------
	// sub_89F4B0: + r * (sin, cos)(angle) / 4096 in x / z
	static void Orbit(SpriteNode *p, int16_t r, int16_t angle)
	{
		p->h.pos[0] = (int16_t)(p->h.pos[0] + mul32(ComputeSin(angle), r) / 4096);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + mul32(ComputeCos(angle), r) / 4096);
	}

	// phase 0x89F590 (grey +0x20 up to 0x80), 0x89F5D0; radius -0x60 (done when gone), angle +0x60
	static void MoteUpdate(SpriteNode *p)
	{
		if (p->h.phase == 0)
		{
			p->rgb[1] = (uint8_t)(p->rgb[1] + 0x20);
			p->rgb[2] = (uint8_t)(p->rgb[2] + 0x20);
			p->rgb[0] = (uint8_t)(p->rgb[0] + 0x20);
			if (p->rgb[0] >= 0x80)
			{
				p->rgb[0] = 0x80;
				p->rgb[1] = 0x80;
				p->rgb[2] = 0x80;
				p->h.phase++;
			}
		}
		p->radius = (int16_t)(p->radius - 0x60);
		if (p->radius <= 0) *(uint8_t *)&p->h.flags |= 1;
		p->ay = (int16_t)((p->ay + 0x60) & 0xFFF);
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
		// 30 fps layer: see mag021_triple_held.inc
		FX_HELD(held_note(REC_MOTE, p);)
		DrawSpriteFacing(p);
		// sub_89F570: next flipbook frame (looping)
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
	// Rune (0x89EBD0)
	// ------------------------------------------------------------------
	// sub_89ED20: the rune's column
	static void SpawnColumn(ModelNode *r)
	{
		ModelNode *c = (ModelNode *)AddEffectTask(QModels(), ORIG_Column, 0x7C, r);
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

	// sub_89ECA0: a mote per tick for ticks 0..6
	static void SpawnMote(ModelNode *r)
	{
		if (r->h.counter < 0 || r->h.counter > 6) return;
		SpriteNode *p = (SpriteNode *)AddEffectTask(QSprites(), ORIG_Mote, 0x70, r);
		memcpy(p->base, p->h.pos, 8);
		p->seq = SEQ_Mote;
		p->radius = (int16_t)((CrtRand() & 0x1FF) + 0x200);
		p->last = 0xC;
		p->ay = (int16_t)(CrtRand() & 0xFFF);
		p->scale[0] = 0x1000;
		p->scale[1] = 0x1000;
		p->scale[2] = 0x1000;
	}

	// phases 0x89F0E0 (fade in, spawning), 0x89F110 (shrink and fade out, then hidden and done), 0x89F170
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
		// 30 fps layer: see mag021_triple_held.inc
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
	// Sequencer (0x89EA70)
	// ------------------------------------------------------------------
	// sub_89EB20: a rune at `angle` on the circle around the target (0x280 up) and its glow
	static void SpawnRune(SeqNode *s, int32_t angle, int16_t arg, int16_t flagged)
	{
		ModelNode *r = (ModelNode *)AddEffectTask(QModels(), ORIG_Rune, 0x7C, s);
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
		SpriteNode *g = (SpriteNode *)AddEffectTask(QSprites(), ORIG_Glow, 0x70, s);
		CopyAnchorXZFromSource(g, g->h.pos);
		memcpy(g->base, g->h.pos, 8);
		g->angle = a;
	}

	static uint32_t __cdecl SequencerTask(TaskNode *t)
	{
		SeqNode *s = (SeqNode *)t;
		switch (s->h.phase)
		{
		case 0: // 0x89EAE0
			if (s->h.counter >= 0)
			{
				SpawnRune(s, 0, 0, 0);
				SpawnRune(s, 0x800, 0, 0);
				s->h.phase++;
			}
			break;
		case 1: // 0x89F7D0
			if (s->h.counter >= 3)
			{
				SpawnRune(s, 0x200, 0, 0);
				SpawnRune(s, 0xA00, 0, 0);
				s->h.phase++;
			}
			break;
		case 2: // 0x89F810
			if (s->h.counter >= 6)
			{
				SpawnRune(s, 0x400, 0, 0);
				SpawnRune(s, 0xC00, 0, 0);
				s->h.phase++;
			}
			break;
		case 3: // 0x89F850: the flagged last rune, then done
			if (s->h.counter >= 9)
			{
				SpawnRune(s, 0x600, 0, 0);
				SpawnRune(s, 0xE00, 0, 1);
				*(uint8_t *)&s->h.flags |= 1;
				s->h.phase++;
			}
			break;
		default: break; // 0x89F890
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
	// Flash (0x89F950, = Double 0x8D6790)
	// ------------------------------------------------------------------
	// MAG_021_sub_89F8D0: on the target's top anchor, unless the target record's +3 bit 2 is set
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
		// 30 fps layer: see mag021_triple_held.inc
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
	// Emitter (0x89E110)
	// ------------------------------------------------------------------
	static uint32_t __cdecl EmitterTask(TaskNode *t)
	{
		EmitterNode *e = (EmitterNode *)t;
		EmitterUpdatePos(e);
		ComputeModelBounds(e);
		switch (e->h.phase)
		{
		case 0: // 0x89EA40: the rune sequencer
			AddEffectTask(QSequencers(), ORIG_Sequencer, 0x40, e);
			e->h.phase++;
			break;
		case 1: // 0x89F8A0: at 24 the root goes on, the flash
			if (e->h.counter >= 0x18)
			{
				((RootNode *)e->h.root)->busy = 0;
				SpawnFlash(e);
				e->h.phase++;
			}
			break;
		case 2: // 0x89FB00: the action result, done
			ApplyActionResultToTarget(e->h.ctx->actions[e->h.action].targets);
			*(uint8_t *)&e->h.flags |= 1;
			e->h.phase++;
			break;
		default: break; // 0x89FB30
		}
		if (e->h.counter == 0)
		{
			int16_t mid[4]; // read and not used
			CopyMidpointFromSource(e, mid);
			BdPlaySE(SOUND_Triple, 0, 0x80);
		}
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
	// Root (0x89DFF0)
	// ------------------------------------------------------------------
	static void RootPhase(RootNode *r)
	{
		switch (r->h.phase)
		{
		case 0: // 0x89E0D0
			r->h.phase++;
			break;
		case 1: // 0x89E0E0: the emitter of the current action
			r->busy = 1;
			AddEffectTask(QEmitters(), ORIG_Emitter, 0x5C, r);
			r->h.phase++;
			break;
		case 2: // 0x89FB40: when the action is over, the next one (back to 1) or on
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
		case 3: // 0x89FB70: every queue empty, done
			if (r->alive == 0)
			{
				*(uint8_t *)&r->h.flags |= 1;
				r->h.phase++;
			}
			break;
		default: break; // 0x89FB90
		}
	}

	static uint32_t __cdecl RootTask(TaskNode *t)
	{
		RootNode *r = (RootNode *)t;
		if (r->tick & 1) PacketCursor() = var<uint32_t>(ARENA_A);
		else PacketCursor() = var<uint32_t>(ARENA_B);
		// 30 fps layer: see mag021_triple_held.inc
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

	void register_mag021_triple()
	{
		register_port(triple021::ORIG_Root, (void *)triple021::RootTask, "021 Root", 21);
		register_port(triple021::ORIG_Emitter, (void *)triple021::EmitterTask, "021 Emitter", 21);
		register_port(triple021::ORIG_Sequencer, (void *)triple021::SequencerTask, "021 Sequencer", 21);
		register_port(triple021::ORIG_Rune, (void *)triple021::RuneTask, "021 Rune", 21);
		register_port(triple021::ORIG_Column, (void *)triple021::ColumnTask, "021 Column", 21);
		register_port(triple021::ORIG_Spark, (void *)triple021::SparkTask, "021 Spark", 21);
		register_port(triple021::ORIG_Glow, (void *)triple021::GlowTask, "021 Glow", 21);
		register_port(triple021::ORIG_Mote, (void *)triple021::MoteTask, "021 Mote", 21);
		register_port(triple021::ORIG_BigRune, (void *)triple021::BigRuneTask, "021 BigRune", 21);
		register_port(triple021::ORIG_Morph, (void *)triple021::MorphTask, "021 Morph", 21);
		register_port(triple021::ORIG_Sprite, (void *)triple021::SpriteTask, "021 Sprite", 21);
		register_port(triple021::ORIG_Flash, (void *)triple021::FlashTask, "021 Flash", 21);
		// 30 fps layer: see mag021_triple_held.inc
		FX_HELD(register_mag021_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag021_triple_held.inc"
#endif
