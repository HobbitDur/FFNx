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

// Effect 13: Dark Mist / Poison Mist (enemy attacks 10 and 21 of kernel.bin, Anacondaur;
// MAG_013_DARK_MIST_POISON_MIST).
//
// Structure (setup 0x8BE1F0, file loader 0x8BE1D0 = mag012.tim, module code 0x8BE1D0..0x8C0360).
// Every node starts with the shared effect-library header (Effect_AddTaskAndInitFromCtx 0x8DC540,
// see mag022_bio.cpp); every task runs its current phase function (node +0x29) from a small
// table, and ends when it is flagged done (+0x26 bit0) and has no live children.
//   Root (0x8BE2D0) - alternates the packet arena (magic buffer + 0 / + 0x4000), follows the
//     caster's bones, spawns one emitter per action (the next one when the previous target's
//     overlay is over), runs the four queues, ends when they are empty.
//   Emitter (0x8BE3E0), one per action: the swirl at once, the mist controller at 25, damage at
//     70. Sound at its first tick.
//   Swirl (0x8BE4A0) - prim model 0x163257C at the caster's mouth (anchor 0xF0), drawn twice
//     (scale 1 and 2 x 2 x 0.75) with two fades; its vertices are rebuilt every tick from the
//     table 0x1630ED4 (wavy ring turning 0x80 per tick) and its texture v scrolls 4 per tick
//     (both written into the model in exe data); spins about z.
//   Mist controller (0x8BE960): cloud (0x8BEAD0) at 0, two mist puffs A (0x8BEC80) and two mist
//     puffs B (0x8BEF30) per tick until 12, stream spawners (0x8BF640) at 16 and 23, the target
//     spawners C (0x8BF260) at 16 and D (0x8BF440) at 30, the target overlay (0x8BF880) at 16.
//   Cloud (0x8BEAD0) - prim model 0x1633C24 growing and fading in along the caster's anchor.
//   Mist A / B - flipbooks thrown from the caster toward the target (1/12 of the distance per
//     tick) with a sine bob; B fades out.
//   Stream (0x8BF700) - flipbooks flying from the caster along 10 table vectors (0x1634B20).
//   C (0x8BF300) / D (0x8BF4F0) - flipbooks at random places around the target.
//   Overlay (0x8BF880) - hides the target model (entity flag 4) and draws it itself with a sine
//     wobble (a copy of Bio's overlay, context 0x274EEF0); the target reappears when it is over.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x27428A8..0x274EFE4 (file pointer, packet cursor 0x27428AC, node pools,
// queues, overlay vertex buffer 0x274C3E0, arena bases 0x274EAD0 / 0x274EAD4, overlay sprite
// 0x274EEA8 and context 0x274EEF0); the swirl model lives in the exe's data (0x163257C).

#include "mag_common.h"

namespace ff8fx
{
namespace mist013
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
		FxNode *emitter;       // +0x14
		FxNode *parent;        // +0x18
		int16_t pos[4];        // +0x1C x, y, z
		int16_t counter;       // +0x24
		uint16_t flags;        // +0x26 bit0 done, bit2 hidden, bit3 overlay drawing
		uint8_t children;      // +0x28
		int8_t phase;          // +0x29
		int8_t action;         // +0x2A
		int8_t target;         // +0x2B
		uint8_t attacker;      // +0x2C
		uint8_t slot;          // +0x2D target entity slot
		uint8_t pad2E[2];
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
		uint8_t busy;          // +0x63 an action's overlay is running
	};
	struct EmitterNode // pool of 4 nodes of 0x58 bytes
	{
		FxNode h;
		uint8_t anchor[0x18];  // +0x30 target bone anchor (MAG_001_CURE_Emitter_UpdatePos)
		int16_t bmin[4];       // +0x48 model bounds (MAG_001_CURE_Emitter_ComputeModelBounds)
		int16_t bmax[4];       // +0x50
	};
	struct ModelNode // pool of 40 nodes of 0x60 bytes (swirl and cloud)
	{
		FxNode h;
		int32_t scale[3];      // +0x30
		int32_t pad3C;
		uint8_t rgb[4];        // +0x40 prim header +8 (fade colour)
		int16_t rot[3];        // +0x44 x, y, z
		int16_t pad4A;
		uint32_t model;        // +0x4C prim model
		int16_t fade;          // +0x50 prim header +0xC
		int16_t pad52;
		int16_t fade_a;        // +0x54 swirl: fade of the small draw
		int16_t fade_b;        // +0x56 swirl: fade of the big draw
		int16_t angle;         // +0x58 swirl: ring turn
		int16_t length;        // +0x5A cloud: anchor length (GetEffectSpawnPosition)
		int16_t grow;          // +0x5C cloud: scale step
		int16_t fade_step;     // +0x5E cloud
	};
	struct Particle // pool of 150 nodes of 0x68 bytes
	{
		FxNode h;
		uint8_t pad30[0x18];
		uint8_t rgb[4];        // +0x48 mist B colour (sprite header +0x1C)
		uint32_t seq;          // +0x4C sprite sequence
		int16_t frame;         // +0x50
		int16_t last;          // +0x52
		int16_t shade;         // +0x54 TransformCameraByShadowRotation offset
		int16_t pad56;
		int16_t vel[3];        // +0x58
		int16_t pad5E;
		int16_t life;          // +0x60 stream
		int16_t variant;       // +0x62 spawn index
		int16_t base_y;        // +0x64 mist: y without the bob
		int16_t bob;           // +0x66 mist: bob angle
	};
	// overlay context 0x274EEF0 (0xF4 bytes, set up by 0x8C00E0; same layout as Bio's)
	struct OverlayCtx
	{
		uint32_t a00;           // +0x00 0x27437C0 (not read by the draw)
		uint8_t *sprite;        // +0x04 0x274EEA8: +0x04 = vertex buffer 0x274C3E0
		uint8_t *entity;        // +0x08 target entity
		uint32_t *frame_cursor; // +0x0C &battle_texture_data_ptr (0x1D8E054)
		uint32_t *packets;      // +0x10 &module packet cursor
		Mat4x3 world;           // +0x14 entity matrix
		Mat4x3 view;            // +0x34 camera * world
		Mat4x3 world2;          // +0x54 entity matrix moved by `offset`
		Mat4x3 light;           // +0x74
		uint8_t pad94[0x10];
		int16_t a4[3];          // +0xA4
		int16_t padAA;
		int16_t ac[3];          // +0xAC
		int16_t padB2;
		int16_t offset[3];      // +0xB4
		int16_t padBA;
		int16_t bc[3];          // +0xBC
		int16_t padC2;
		uint8_t rgb[3];         // +0xC4
		uint8_t padC7;
		int32_t light_dist;     // +0xC8
		int16_t wobble_phase;   // +0xCC advances 0xA0 per tick
		int16_t wobble_amp;     // +0xCE
		int16_t padD0;
		int16_t mode;           // +0xD2 0: offset in model space, 1: in world space
		uint8_t padD4[4];
		int16_t anim;           // +0xD8
		int16_t slot;           // +0xDA 0xFF: the overlay advances the animation itself
		int16_t light_pos[3];   // +0xDC
		uint8_t padE2[0x10];
		int16_t glow;           // +0xF2
	};
#pragma pack(pop)
	static_assert(sizeof(FxNode) == 0x30 && sizeof(RootNode) == 0x64 && sizeof(EmitterNode) == 0x58, "Dark Mist nodes");
	static_assert(sizeof(ModelNode) == 0x60 && sizeof(Particle) == 0x68 && sizeof(OverlayCtx) == 0xF4, "Dark Mist nodes");

	// ------------------------------------------------------------------
	// Module globals
	// ------------------------------------------------------------------
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x27428AC); }
	inline uint32_t &ArenaA() { return var<uint32_t>(0x274EAD4); } // magic buffer (Magic_TextureOFF)
	inline uint32_t &ArenaB() { return var<uint32_t>(0x274EAD0); } // magic buffer + 0x4000
	inline TaskQueue *QEmitters() { return (TaskQueue *)0x274E960; }  // 4 x 0x58
	inline TaskQueue *QControl() { return (TaskQueue *)0x274EAD8; }   // 20 x 0x30
	inline TaskQueue *QParticles() { return (TaskQueue *)0x274C3C0; } // 150 x 0x68
	inline TaskQueue *QModels() { return (TaskQueue *)0x27437B0; }    // 40 x 0x60
	inline OverlayCtx &Overlay() { return var<OverlayCtx>(0x274EEF0); }
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_Root = 0x8BE2D0;
	static const uint32_t ORIG_Emitter = 0x8BE3E0;
	static const uint32_t ORIG_Swirl = 0x8BE4A0;
	static const uint32_t ORIG_Control = 0x8BE960;
	static const uint32_t ORIG_Cloud = 0x8BEAD0;
	static const uint32_t ORIG_MistA = 0x8BEC80;
	static const uint32_t ORIG_MistB = 0x8BEF30;
	static const uint32_t ORIG_SpawnerC = 0x8BF260;
	static const uint32_t ORIG_PuffC = 0x8BF300;
	static const uint32_t ORIG_SpawnerD = 0x8BF440;
	static const uint32_t ORIG_PuffD = 0x8BF4F0;
	static const uint32_t ORIG_SpawnerE = 0x8BF640;
	static const uint32_t ORIG_Stream = 0x8BF700;
	static const uint32_t ORIG_Overlay = 0x8BF880;

	static const void *const SOUND_Mist = (const void *)0x16309F8;
	static const uint32_t MODEL_Swirl = 0x163257C, MODEL_Cloud = 0x1633C24;
	static const uint32_t RING_Table = 0x1630ED4;         // swirl ring: count at +4, (x, y, angle, -) from +8
	static const uint32_t STREAM_Vectors = 0x1634B20;     // 10 x (x, y, z, -)
	static const uint32_t SEQ_Mist = 0x16309FC, SEQ_MistB = 0x1630E94, SEQ_PuffC = 0x1630C78, SEQ_PuffD = 0x1630D84;
	static const uint32_t OVERLAY_A00 = 0x27437C0, OVERLAY_SPRITE = 0x274EEA8, OVERLAY_VERTS = 0x274C3E0;

	// ------------------------------------------------------------------
	// Engine / effect-library functions (original addresses)
	// ------------------------------------------------------------------
	inline FxNode *AddEffectTask(TaskQueue *q, uint32_t task, int32_t size, void *parent) { return fn<FxNode *(__cdecl *)(TaskQueue *, uint32_t, int32_t, void *)>(0x8DC540)(q, task, size, parent); }
	inline void ReleaseLinkedTask(void *n) { fn<void (__cdecl *)(void *)>(0x8DC530)(n); }
	inline void EmitterUpdatePos(void *n) { fn<void (__cdecl *)(void *)>(0x8DC610)(n); }        // MAG_001_CURE_Emitter_UpdatePos
	inline void UpdateTargetPosFromBones(void *n) { fn<void (__cdecl *)(void *)>(0x8DC740)(n); }
	inline void ComputeModelBounds(void *n) { fn<void (__cdecl *)(void *)>(0x8DC870)(n); }      // MAG_001_CURE_Emitter_ComputeModelBounds
	// Effect_CopyAnchorXZFromSource (0x8DC6E0): 8 bytes of the emitter's +0x30 anchor to out
	inline void CopyAnchor(void *n, void *out) { fn<void (__cdecl *)(void *, void *)>(0x8DC6E0)(n, out); }
	// GetEffectSpawnPosition (0x502170): position of an entity's anchor `bone`, the bone length
	// scaled by `length` (4.12)
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t length, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, length, out); }
	inline void IdentityMatrix(Mat4x3 *m) { fn<void (__cdecl *)(Mat4x3 *)>(0x8DD770)(m); }
	inline void RotateX(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD7E0)(m, a); }
	inline void RotateY(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD8A0)(m, a); }
	inline void RotateZ(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD960)(m, a); }
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline void NormalizeVec3(const int32_t *in, int16_t *out) { fn<void (__cdecl *)(const int32_t *, int16_t *)>(0x56BD20)(in, out); }
	inline void BuildMatrixFromDirAndUp(void *out, const void *dir, const void *up) { fn<void (__cdecl *)(void *, const void *, const void *)>(0x8DDA50)(out, dir, up); }
	inline void UnpackRotationMatrix(const void *in, void *out) { fn<void (__cdecl *)(const void *, void *)>(0x56C040)(in, out); }
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteLoadV0Dwords(const void *v) { fn<void (__cdecl *)(const void *)>(0x45E060)(v); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }       // GTE_ReadOTZ (dword)
	inline void GteStoreSXY012_FT3(void *p) { fn<void (__cdecl *)(void *)>(0x45E2E0)(p); }       // p +8, +0x10, +0x18
	inline void GteStoreSXY012_FT4(void *p) { fn<void (__cdecl *)(void *)>(0x45E320)(p); }
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteSetBackground(int32_t x, int32_t y, int32_t z) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DCF0)(x, y, z); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }                    // IR = L * V0 + BK
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }
	inline void GteLoadIRFromColumn(const void *m) { fn<void (__cdecl *)(const void *)>(0x45E180)(m); }
	inline void GteStoreIRToColumn(void *m) { fn<void (__cdecl *)(void *)>(0x45E470)(m); }
	inline int32_t BattleReadAnimation(void *hdr, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(0x508F90)(hdr, cmd); }
	inline void PreBattleReadAnimation(void *hdr, void *cmd, int32_t anim) { fn<void (__cdecl *)(void *, void *, int32_t)>(0x509440)(hdr, cmd, anim); }
	// sub_5088A0: an entity's ground shadow (returns the packet cursor)
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }

	static uint8_t *CasterEntity(const FxNode *n) { return Entity(n->ctx->attacker); }

	// x / 2^18, rounded toward zero (cdq; and 0x3FFFF; add; sar 18)
	static int32_t Div2p18(int32_t x) { return (x + ((x >> 31) & 0x3FFFF)) >> 18; }

	// end of every task: counter + 1; done and no live children -> released
	static uint32_t Finish(FxNode *n)
	{
		n->counter++;
		if ((n->flags & 1) && n->children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// sub_8BEEF0: next flipbook frame; past the last one the node is hidden (frame stays last)
	static int AdvanceFrame(Particle *p)
	{
		p->frame++;
		if (p->frame > p->last)
		{
			p->h.flags |= 4;
			p->frame = p->last;
			return 1;
		}
		return 0;
	}

	// sub_8BF230: next flipbook frame, looping
	static void LoopFrame(Particle *p)
	{
		p->frame++;
		if (p->frame > p->last) p->frame = 0;
	}

	// ------------------------------------------------------------------
	// Prim models (swirl, cloud)
	// ------------------------------------------------------------------
	// sub_8BE6B0: every polygon's texture v (bytes 1 of its three uv words) - 4, wrapped in its
	// 0x40-high band when any of them goes below 0 (the band is 0..0x7F)
	static void ScrollV(uint32_t model)
	{
		const uint8_t *m = (const uint8_t *)model;
		int32_t vb = *(const int32_t *)m / 4;
		int32_t count = *(const int32_t *)(m + vb * 4 + 0x1C);
		if (count <= 0) return;
		uint32_t *w = (uint32_t *)(m + vb * 4 + 0x1C + 4 + 0x10);
		for (int32_t k = count; k != 0; k--)
		{
			uint32_t d0 = w[-1], d1 = w[0], d2 = w[1];
			uint32_t v0 = (d0 & 0xFF00) - 0x400;
			uint32_t v1 = (d1 & 0xFF00) - 0x400;
			uint32_t v2 = (d2 & 0xFF00) - 0x400;
			uint32_t v3 = ((d2 >> 24) << 8) - 0x400;
			if (v0 > 0xFF00 || v1 > 0xFF00 || v2 > 0xFF00 || v3 > 0xFF00)
			{
				v0 += 0x4000;
				v1 += 0x4000;
				v2 += 0x4000;
				v3 += 0x4000;
			}
			w[0] = (d1 & 0xFFFF00FF) | (v1 & 0x7F00);
			w[-1] = (d0 & 0xFFFF00FF) | (v0 & 0x7F00);
			w[1] = (d2 & 0x00FF00FF) | (v2 & 0x7F00) | ((v3 & 0x7F00) << 16);
			w += 9;
		}
	}

	// sub_8BE7C0: the model's vertices x / y = ring x / y + |x| cos / |y| sin (ring angle + angle)
	static void WarpRing(uint32_t ring, uint32_t model, int32_t angle)
	{
		int32_t count = *(const int32_t *)(ring + 4);
		const int16_t *src = (const int16_t *)(ring + 8);
		int16_t *dst = (int16_t *)(model + 8);
		for (int32_t k = count; k > 0; k--)
		{
			int32_t a = (int32_t)((uint32_t)(uint16_t)(src[2] + (int16_t)angle) & 0xFFF);
			int32_t c = ComputeCos(a);
			int32_t x = src[0];
			if (x < 0) x = -x;
			dst[0] = (int16_t)(mul32(x, c) / 4096 + src[0]);
			int32_t s = ComputeSin(a);
			int32_t y = src[1];
			if (y < 0) y = -y;
			dst[1] = (int16_t)(mul32(y, s) / 4096 + src[1]);
			src += 4;
			dst += 4;
		}
	}

	// sub_8BE5C0: the prim model at pos, rotated (y, x, z) and scaled, fade +0x50
	static void DrawModel(ModelNode *n)
	{
		if (n->h.flags & 4) return;
		Mat4x3 m = {};
		IdentityMatrix(&m);
		RotateY(&m, n->rot[1]);
		RotateX(&m, n->rot[0]);
		RotateZ(&m, n->rot[2]);
		m.t[0] = n->h.pos[0];
		m.t[1] = n->h.pos[1];
		m.t[2] = n->h.pos[2];
		Scale3DMatrix(&m, n->scale);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = n->model;
		*(uint32_t *)(h + 8) = *(const uint32_t *)n->rgb;
		*(int32_t *)(h + 0xC) = n->fade;
		*(uint32_t *)(h + 0x1C) = 0xF0;
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Sprites
	// ------------------------------------------------------------------
	// sub_8BECE0: camera-facing flipbook `seq` frame `frame` at pos
	static void DrawSprite(Particle *p)
	{
		if (p->h.flags & 4) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(p->h.pos, 0x1000, p->shade);
		*(uint32_t *)h = p->seq;
		*(int16_t *)(h + 4) = p->frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// sub_8BEF90: the same with the node's colour (header +0x1C, mode 4)
	static void DrawSpriteColoured(Particle *p)
	{
		if (p->h.flags & 4) return;
		TransformCameraByShadowRotation(p->h.pos, 0x1000, p->shade);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = p->seq;
		*(int16_t *)(h + 4) = p->frame;
		*(uint32_t *)(h + 0x1C) = *(const uint32_t *)p->rgb;
		*(uint16_t *)(h + 0x24) = 4;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag013_dark_mist_held.h"
#endif

namespace ff8fx
{
namespace mist013
{
	// ------------------------------------------------------------------
	// Mist A (0x8BEC80)
	// ------------------------------------------------------------------
	// sub_8BED50: aimed at the emitter's anchor, 1/12 of the way per tick; odd spawns start half
	// a step ahead
	static void MistAInit(Particle *p)
	{
		p->seq = SEQ_Mist;
		p->last = 0x17;
		p->shade = 0;
		CopyAnchor(p, p->vel);
		const int16_t x0 = p->h.pos[0], z0 = p->h.pos[2];
		const int32_t vx = (int16_t)(p->vel[0] - x0) / 12;
		p->vel[0] = (int16_t)vx;
		const int16_t y0 = p->h.pos[1];
		const int32_t r = CrtRand();
		const int32_t vy = (int16_t)((uint16_t)p->vel[1] - (uint32_t)(r & 0xFF) - (uint16_t)y0 - 0xC0) / 12;
		p->bob = 0;
		p->base_y = y0;
		const int32_t vz = (int16_t)(p->vel[2] - z0) / 12;
		p->vel[1] = (int16_t)vy;
		p->vel[2] = (int16_t)vz;
		if (p->variant != 0)
		{
			p->h.pos[0] = (int16_t)((int16_t)vx / 2 + x0);
			p->base_y = (int16_t)((int16_t)vy / 2 + y0);
			p->h.pos[2] = (int16_t)((int16_t)vz / 2 + z0);
			const int32_t s = ComputeSin(0);
			p->bob = (int16_t)((uint16_t)(p->bob + 0x155) & 0xFFF);
			p->h.pos[1] = (int16_t)(s / 16 + p->base_y);
		}
		p->h.phase++;
	}

	// sub_8BEE80
	static void MistAMove(Particle *p)
	{
		p->h.pos[0] = (int16_t)(p->h.pos[0] + p->vel[0]);
		p->shade = (int16_t)(p->shade - 0x20);
		p->base_y = (int16_t)(p->base_y + p->vel[1]);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + p->vel[2]);
		const int32_t s = ComputeSin(p->bob);
		p->bob = (int16_t)((uint16_t)(p->bob + 0x155) & 0xFFF);
		p->h.pos[1] = (int16_t)(s / 16 + p->base_y);
		AdvanceFrame(p);
		if (p->h.counter >= 0xC)
		{
			p->h.flags |= 1;
			p->h.phase++;
		}
	}

	static uint32_t __cdecl MistATask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: MistAInit(p); break;
		case 1: MistAMove(p); break;
		default: break; // nullsub_1810
		}
		// 30 fps layer: see mag013_dark_mist_held.inc
		FX_HELD(held_note_draw(ORIG_MistA, p);)
		DrawSprite(p);
		return Finish(&p->h);
	}

	// ------------------------------------------------------------------
	// Mist B (0x8BEF30)
	// ------------------------------------------------------------------
	// sub_8BF000: aimed at a random point around the emitter's anchor
	static void MistBInit(Particle *p)
	{
		p->seq = SEQ_MistB;
		p->rgb[0] = 0x80;
		p->rgb[1] = 0x80;
		p->rgb[2] = 0x80;
		p->last = 1;
		p->shade = 0;
		CopyAnchor(p, p->vel);
		int32_t r = CrtRand();
		p->vel[1] = (int16_t)(p->vel[1] + (int16_t)(-0xC0 - (r & 0xFF)));
		r = CrtRand();
		p->vel[0] = (int16_t)(p->vel[0] + (int16_t)(0x100 - (r & 0x1FF)));
		r = CrtRand();
		const int16_t x0 = p->h.pos[0], z0 = p->h.pos[2];
		p->vel[1] = (int16_t)(p->vel[1] + (int16_t)(0x100 - (r & 0x1FF)));
		const int32_t vx = (int16_t)(p->vel[0] - x0) / 12;
		const int16_t y0 = p->h.pos[1];
		p->vel[0] = (int16_t)vx;
		const int32_t vy = (int16_t)(p->vel[1] - y0) / 12;
		p->vel[1] = (int16_t)vy;
		r = CrtRand();
		const int32_t vz = (int16_t)((uint16_t)p->vel[2] - (uint32_t)(r & 0x1FF) - (uint16_t)z0 + 0x100) / 12;
		p->bob = 0;
		p->base_y = y0;
		p->vel[2] = (int16_t)vz;
		if (p->variant != 0)
		{
			p->h.pos[0] = (int16_t)((int16_t)vx / 2 + x0);
			p->base_y = (int16_t)((int16_t)vy / 2 + y0);
			p->h.pos[2] = (int16_t)((int16_t)vz / 2 + z0);
			const int32_t s = ComputeSin(0);
			p->bob = (int16_t)((uint16_t)(p->bob + 0x15) & 0xFFF);
			p->h.pos[1] = (int16_t)(s / 16 + p->base_y);
		}
		p->h.phase++;
	}

	// sub_8BF180: like mist A (looping flipbook), fading out from tick 4
	static void MistBMove(Particle *p)
	{
		p->h.pos[0] = (int16_t)(p->h.pos[0] + p->vel[0]);
		p->shade = (int16_t)(p->shade - 0x30);
		p->base_y = (int16_t)(p->base_y + p->vel[1]);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + p->vel[2]);
		const int32_t s = ComputeSin(p->bob);
		p->bob = (int16_t)((uint16_t)(p->bob + 0x155) & 0xFFF);
		p->h.pos[1] = (int16_t)(s / 16 + p->base_y);
		LoopFrame(p);
		if (p->h.counter >= 4)
		{
			const uint8_t r = (uint8_t)(p->rgb[0] + 0xF0);
			p->rgb[1] = (uint8_t)(p->rgb[1] + 0xF0);
			p->rgb[0] = r;
			p->rgb[2] = (uint8_t)(p->rgb[2] + 0xF0);
			if (r == 0)
			{
				p->rgb[0] = 0x10;
				p->rgb[1] = 0x10;
				p->rgb[2] = 0x10;
			}
		}
		if (p->h.counter >= 0xC)
		{
			p->h.flags |= 1;
			p->h.phase++;
		}
	}

	static uint32_t __cdecl MistBTask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: MistBInit(p); break;
		case 1: MistBMove(p); break;
		default: break; // nullsub_1811
		}
		// 30 fps layer: see mag013_dark_mist_held.inc
		FX_HELD(held_note_draw(ORIG_MistB, p);)
		DrawSpriteColoured(p);
		return Finish(&p->h);
	}

	// ------------------------------------------------------------------
	// Puffs C (0x8BF300) / D (0x8BF4F0) around the target, and their spawners
	// ------------------------------------------------------------------
	// sub_8BF360
	static void PuffCInit(Particle *p)
	{
		p->seq = SEQ_PuffC;
		p->last = 8;
		p->shade = 0;
		CopyAnchor(p, p->h.pos);
		p->h.pos[0] = (int16_t)(p->h.pos[0] + (int16_t)(CrtRand() % 0x300 - 0x180));
		p->h.pos[2] = (int16_t)(p->h.pos[2] + (int16_t)(CrtRand() % 0x300 - 0x180));
		p->h.pos[1] = (int16_t)(p->h.pos[1] + (int16_t)(CrtRand() % 0x300 - 0x380));
		p->h.phase++;
	}

	// sub_8BF550
	static void PuffDInit(Particle *p)
	{
		p->seq = SEQ_PuffD;
		p->last = 9;
		p->shade = 0;
		CopyAnchor(p, p->h.pos);
		p->h.pos[0] = (int16_t)(p->h.pos[0] + (int16_t)(CrtRand() % 0x200 - 0x100));
		p->h.pos[2] = (int16_t)(p->h.pos[2] + (int16_t)(CrtRand() % 0x200 - 0x100));
		p->h.pos[1] = (int16_t)(p->h.pos[1] + (int16_t)(CrtRand() % 0x200 - 0x300));
		p->h.phase++;
	}

	// sub_8BF3E0 / sub_8BF5E0: the flipbook once
	static void PuffPlay(Particle *p)
	{
		if (AdvanceFrame(p))
		{
			p->h.flags |= 1;
			p->h.phase++;
		}
	}

	static uint32_t __cdecl PuffCTask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: PuffCInit(p); break;
		case 1: PuffPlay(p); break;
		default: break; // nullsub_1812
		}
		// 30 fps layer: see mag013_dark_mist_held.inc
		FX_HELD(held_note_draw(ORIG_PuffC, p);)
		DrawSprite(p);
		return Finish(&p->h);
	}

	static uint32_t __cdecl PuffDTask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: PuffDInit(p); break;
		case 1: PuffPlay(p); break;
		default: break; // nullsub_1814
		}
		// 30 fps layer: see mag013_dark_mist_held.inc
		FX_HELD(held_note_draw(ORIG_PuffD, p);)
		DrawSprite(p);
		return Finish(&p->h);
	}

	// spawner C (0x8BF260): one puff per tick up to tick 16 (sub_8BF2C0), done after 16 (sub_8BF410)
	static uint32_t __cdecl SpawnerCTask(TaskNode *n)
	{
		FxNode *s = (FxNode *)n;
		switch (s->phase)
		{
		case 0:
			if (s->counter <= 0x10)
			{
				Particle *p = (Particle *)AddEffectTask(QParticles(), ORIG_PuffC, 0x68, s);
				p->variant = s->counter;
			}
			else s->phase++;
			break;
		case 1:
			if (s->counter > 0x10)
			{
				s->flags |= 1;
				s->phase++;
			}
			break;
		default: break; // nullsub_1813
		}
		return Finish(s);
	}

	// spawner D (0x8BF440): two puffs per tick up to tick 8 (sub_8BF4A0), done after 16 (sub_8BF610)
	static uint32_t __cdecl SpawnerDTask(TaskNode *n)
	{
		FxNode *s = (FxNode *)n;
		switch (s->phase)
		{
		case 0:
			if (s->counter <= 8)
			{
				AddEffectTask(QParticles(), ORIG_PuffD, 0x68, s);
				Particle *p = (Particle *)AddEffectTask(QParticles(), ORIG_PuffD, 0x68, s);
				p->variant = s->counter;
			}
			else s->phase++;
			break;
		case 1:
			if (s->counter > 0x10)
			{
				s->flags |= 1;
				s->phase++;
			}
			break;
		default: break; // nullsub_1815
		}
		return Finish(s);
	}

	// ------------------------------------------------------------------
	// Stream (0x8BF700) and its spawner (0x8BF640)
	// ------------------------------------------------------------------
	// sub_8BF760: spawn k flies vector k / (20 - k) per tick for 20 - k ticks, flipbook from 3 + k
	static void StreamInit(Particle *p)
	{
		const int16_t life = (int16_t)(0x14 - p->variant);
		p->seq = SEQ_Mist;
		p->life = life;
		p->frame = (int16_t)(0x17 - life);
		p->last = 0x17;
		p->shade = (int16_t)0xFE00;
		CopyAnchor(p, p->h.pos);
		const int16_t *v = (const int16_t *)(STREAM_Vectors + (int32_t)p->variant * 8);
		const int32_t d = p->life;
		p->vel[0] = (int16_t)(v[0] / d);
		p->vel[1] = (int16_t)(v[1] / d);
		p->vel[2] = (int16_t)(v[2] / d);
		p->h.phase++;
	}

	// sub_8BF7E0
	static void StreamMove(Particle *p)
	{
		p->h.pos[0] = (int16_t)(p->h.pos[0] + p->vel[0]);
		p->h.pos[1] = (int16_t)(p->h.pos[1] + p->vel[1]);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + p->vel[2]);
		AdvanceFrame(p);
		if (p->h.counter > p->life)
		{
			p->h.flags |= 1;
			p->h.phase++;
		}
	}

	static uint32_t __cdecl StreamTask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: StreamInit(p); break;
		case 1: StreamMove(p); break;
		default: break; // nullsub_1816
		}
		// 30 fps layer: see mag013_dark_mist_held.inc
		FX_HELD(held_note_draw(ORIG_Stream, p);)
		DrawSprite(p);
		return Finish(&p->h);
	}

	// spawner E: at the caster's anchor, one stream particle per tick up to tick 9 (sub_8BF6C0)
	static uint32_t __cdecl SpawnerETask(TaskNode *n)
	{
		FxNode *s = (FxNode *)n;
		GetEffectSpawnPosition(CasterEntity(s), 0xF0, 0x400, s->pos);
		switch (s->phase)
		{
		case 0:
			if (s->counter <= 9)
			{
				Particle *p = (Particle *)AddEffectTask(QParticles(), ORIG_Stream, 0x68, s);
				p->variant = s->counter;
			}
			else
			{
				s->flags |= 1;
				s->phase++;
			}
			break;
		default: break; // nullsub_1817
		}
		return Finish(s);
	}

	// ------------------------------------------------------------------
	// Cloud (0x8BEAD0)
	// ------------------------------------------------------------------
	// sub_8BEBA0
	static void CloudInit(ModelNode *c)
	{
		uint8_t *entity = CasterEntity(&c->h);
		c->rot[0] = 0x100;
		c->length = 0x400;
		c->fade_step = 0x2C0;
		c->grow = 0x800;
		c->model = MODEL_Cloud;
		c->rot[1] = *(const int16_t *)(entity + 0xE);
		c->fade = 0;
		c->rgb[0] = 0;
		c->rgb[1] = 0;
		c->rgb[2] = 0;
		c->scale[0] = 0;
		c->scale[1] = 0;
		c->scale[2] = 0;
		GetEffectSpawnPosition(entity, 0xF0, 0x400, c->h.pos);
		c->h.phase++;
	}

	// sub_8BEC30: fades in (step - 0x40 per tick, at least 0x40), done at full
	static void CloudFade(ModelNode *c)
	{
		const int16_t step = c->fade_step;
		c->fade = (int16_t)(c->fade + step);
		if (c->fade >= 0x1000)
		{
			c->h.flags |= 1;
			c->fade = 0x1000;
			c->h.phase++;
		}
		const int16_t s = (int16_t)(step - 0x40);
		c->fade_step = s;
		if (s < 0x40) c->fade_step = 0x40;
	}

	// the tick's growth and anchor after the phase function
	static void CloudUpdate(ModelNode *c, uint8_t *entity)
	{
		const int16_t g = c->grow;
		c->scale[0] += g;
		c->scale[1] += g;
		c->scale[2] += g;
		const int16_t g2 = (int16_t)(g - 0x80);
		c->grow = g2;
		if (g2 < 0) c->grow = 0;
		c->length = (int16_t)(c->length + 0x400);
		GetEffectSpawnPosition(entity, 0xF0, c->length, c->h.pos);
	}

	static uint32_t __cdecl CloudTask(TaskNode *n)
	{
		ModelNode *c = (ModelNode *)n;
		uint8_t *entity = CasterEntity(&c->h);
		switch (c->h.phase)
		{
		case 0: CloudInit(c); break;
		case 1: CloudFade(c); break;
		default: break; // nullsub_1809
		}
		CloudUpdate(c, entity);
		// 30 fps layer: see mag013_dark_mist_held.inc
		FX_HELD(held_note_draw(ORIG_Cloud, c);)
		DrawModel(c);
		return Finish(&c->h);
	}

	// ------------------------------------------------------------------
	// Swirl (0x8BE4A0)
	// ------------------------------------------------------------------
	static void SwirlPhase(ModelNode *w)
	{
		switch (w->h.phase)
		{
		case 0: // sub_8BE860
			w->fade_a = 0x1000;
			w->fade_b = 0x1000;
			w->scale[0] = 0x1000;
			w->scale[1] = 0x1000;
			w->scale[2] = 0x1000;
			w->rgb[0] = 0;
			w->rgb[1] = 0;
			w->rgb[2] = 0;
			w->model = MODEL_Swirl;
			w->h.phase++;
			break;
		case 1: // sub_8BE8A0: fades in
			w->fade_a = (int16_t)(w->fade_a - 0x400);
			w->fade_b = (int16_t)(w->fade_b - 0x200);
			if (w->fade_a <= 0)
			{
				w->fade_a = 0;
				w->fade_b = 0x800;
				w->h.phase++;
			}
			break;
		case 2: // sub_8BE8D0: until tick 12
			if (w->h.counter >= 0xC) w->h.phase++;
			break;
		case 3: // sub_8BE8E0: fades out
			w->fade_a = (int16_t)(w->fade_a + 0x200);
			w->fade_b = (int16_t)(w->fade_b + 0x100);
			if (w->fade_a >= 0x1000)
			{
				w->h.flags |= 1;
				w->fade_a = 0x1000;
				w->fade_b = 0x1000;
				w->h.phase++;
			}
			break;
		default: break; // nullsub_1808
		}
	}

	static uint32_t __cdecl SwirlTask(TaskNode *n)
	{
		ModelNode *w = (ModelNode *)n;
		uint8_t *entity = CasterEntity(&w->h);
		w->rot[0] = 0x100;
		w->rot[1] = *(const int16_t *)(entity + 0xE);
		GetEffectSpawnPosition(entity, 0xF0, 0x400, w->h.pos);
		SwirlPhase(w);
		w->rot[2] = (int16_t)((uint16_t)(w->rot[2] - 0x20) & 0xFFF);
		ScrollV(w->model);
		w->angle = (int16_t)((uint16_t)(w->angle + 0x80) & 0xFFF);
		WarpRing(RING_Table, w->model, w->angle);
		w->fade = w->fade_a;
		w->scale[0] = 0x1000;
		w->scale[1] = 0x1000;
		w->scale[2] = 0x1000;
		// 30 fps layer: see mag013_dark_mist_held.inc
		FX_HELD(held_note_draw(ORIG_Swirl, w);)
		DrawModel(w);
		w->fade = w->fade_b;
		w->scale[0] = 0x2000;
		w->scale[1] = 0x2000;
		w->scale[2] = 0xC00;
		// 30 fps layer: see mag013_dark_mist_held.inc
		FX_HELD(held_note_draw(ORIG_Swirl, w);)
		DrawModel(w);
		return Finish(&w->h);
	}

	// ------------------------------------------------------------------
	// Target overlay (0x8BF880): a copy of Bio's
	// ------------------------------------------------------------------
	// sub_8C00E0
	static void InitOverlay(OverlayCtx *mc, uint32_t a00, uint8_t *sprite, uint32_t verts, uint8_t *entity, int32_t slot)
	{
		mc->entity = entity;
		mc->slot = (int16_t)slot;
		mc->a00 = a00;
		mc->sprite = sprite;
		mc->frame_cursor = (uint32_t *)0x1D8E054;
		mc->packets = (uint32_t *)0x27428AC;
		mc->offset[0] = 0;
		mc->offset[1] = 0;
		mc->offset[2] = 0;
		mc->rgb[0] = 0x80;
		mc->rgb[1] = 0x80;
		mc->rgb[2] = 0x80;
		mc->light_dist = 0x2000;
		mc->mode = 1;
		mc->wobble_amp = 0;
		mc->glow = 0;
		mc->a4[0] = 0;
		mc->a4[1] = 0;
		mc->a4[2] = 0;
		mc->ac[0] = 0;
		mc->ac[1] = 0;
		mc->ac[2] = 0;
		mc->bc[0] = 0;
		mc->bc[1] = 0;
		mc->bc[2] = 0;
		sprite[0x1C] = 0x80;
		sprite[0x1D] = 0x80;
		sprite[0x1E] = 0x80;
		*(uint32_t *)(sprite + 4) = verts;
		*(uint16_t *)(sprite + 0x14) = 0;
		*(uint16_t *)(sprite + 0x16) = 0;
		*(uint16_t *)(sprite + 0x18) = 0x140;
		*(uint16_t *)(sprite + 0x1A) = 0;
		*(uint32_t *)(sprite + 0x20) = 0xFFFFFFFF;
		*(uint16_t *)(sprite + 0x24) = 0;
	}

	// sub_8BFA00: the entity's matrix, and a copy moved by `offset`
	static void CopyWorld(OverlayCtx *mc, const uint8_t *entity)
	{
		memcpy(&mc->world, entity + 0x40, 0x20);
		memcpy(&mc->world2, entity + 0x40, 0x20);
		int16_t mode = mc->mode;
		if (mode == 0)
		{
			int16_t out[4];
			MatrixMulVector(&mc->world2, mc->offset, out);
			mc->world2.t[0] += out[0];
			mc->world2.t[1] += out[1];
			mc->world2.t[2] += out[2];
		}
		else if (mode == 1)
		{
			mc->world2.t[0] += mc->offset[0];
			mc->world2.t[1] += mc->offset[1];
			mc->world2.t[2] += mc->offset[2];
		}
	}

	// sub_8BFAC0: `light` = the entity's rotation seen from light_pos (at light_dist)
	static void BuildLightMatrix(OverlayCtx *mc)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x70);
		int16_t x = (int16_t)mc->world2.t[0];
		int16_t y = (int16_t)mc->world2.t[1];
		int16_t z = (int16_t)mc->world2.t[2];
		*(int16_t *)(s + 0x68) = x;
		int32_t dx = x - mc->light_pos[0];
		*(int16_t *)(s + 0x6A) = y;
		int32_t dy = y - mc->light_pos[1];
		*(int16_t *)(s + 0x6C) = z;
		int32_t dz = z - mc->light_pos[2];
		*(int32_t *)(s + 4) = dy;
		*(int32_t *)(s + 8) = dz;
		*(int32_t *)s = dx;
		int32_t d2 = mul32(dz, dz) + mul32(dy, dy) + mul32(dx, dx);
		*(int32_t *)(s + 0x60) = d2;
		*(int32_t *)(s + 0x64) = Sqrt(d2);
		*(int16_t *)(s + 0x12) = 0x1000;
		*(int16_t *)(s + 0x10) = 0;
		*(int16_t *)(s + 0x14) = 0;
		NormalizeVec3((const int32_t *)s, (int16_t *)(s + 0x18));
		BuildMatrixFromDirAndUp(s + 0x20, s + 0x18, s + 0x10);
		UnpackRotationMatrix(s + 0x20, s + 0x40);
		*(int32_t *)(s + 0x5C) = mc->light_dist;
		memcpy(s + 0x20, &mc->world, 0x20);
		*(int32_t *)(s + 0x54) = 0;
		*(int32_t *)(s + 0x58) = 0;
		*(int32_t *)(s + 0x38) = mc->world.t[1] - mc->world2.t[1];
		*(int32_t *)(s + 0x34) = mc->world.t[0] - mc->world2.t[0];
		*(int32_t *)(s + 0x3C) = mc->world.t[2] - mc->world2.t[2];
		GteSetRotMatrixCtrl((const Mat4x3 *)(s + 0x40));
		uint8_t *light = (uint8_t *)&mc->light;
		for (int c = 0; c < 3; c++)
		{
			GteLoadIRFromColumn(s + 0x20 + 2 * c);
			GteMVMVA_RotIR();
			GteStoreIRToColumn(light + 2 * c);
		}
		GteSetTransVectorCtrl((const Mat4x3 *)(s + 0x40));
		GteLoadV0Dwords(s + 0x34);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(mc->light.t);
		FieldFree(0x70);
	}

	// sub_8BFC30: one model object (bone groups of vertices, then textured triangles and quads)
	// with every vertex moved by amp * (sin, cos)(phase + 12 * y) / 2^18 in x / z; flat colours
	// from the entity's colour (+0x28). Vertices go to the sprite's buffer, packets to the frame
	// arena (battle_texture_data_ptr).
	static void DrawOverlayObject(const uint8_t *obj, uint32_t ot, int32_t mode, OverlayCtx *mc)
	{
		(void)mode;
		uint8_t *entity = mc->entity;
		uint32_t cursor = *mc->frame_cursor;
		uint8_t *verts = *(uint8_t **)(mc->sprite + 4);
		const uint8_t *bones = *(uint8_t *const *)obj + 0x10;
		const uint8_t *table = *(uint8_t *const *)(obj + 4);
		int32_t count = *(const int32_t *)table;
		uint8_t *s = (uint8_t *)FieldAlloc(0x74);
		const uint32_t *offs = (const uint32_t *)(table + 4);
		*(uint32_t *)(s + 0x70) = *(const uint32_t *)(entity + 0x7C);
		uint32_t col = *(const uint32_t *)(entity + 0x28) & 0xFFFFFF;
		*(uint32_t *)(s + 0x68) = col | 0x24000000;
		*(uint32_t *)(s + 0x6C) = col | 0x2C000000;
		*(int16_t *)(s + 0x64) = mc->wobble_amp;
		*(int16_t *)(s + 0x66) = mc->wobble_phase;
		GteSetRotMatrixCtrl(&mc->view);
		GteSetTransVectorCtrl(&mc->view);
		for (int32_t i = 0; i < count; i++)
		{
			const uint8_t *p = table + offs[i];
			if (!((*(const uint32_t *)(s + 0x70) >> (i & 31)) & 1)) continue;
			// vertices, grouped by bone
			uint8_t *v = verts;
			int32_t groups = *(const int16_t *)p;
			p += 2;
			for (int32_t g = groups; g > 0; g--)
			{
				int32_t bone = *(const int16_t *)p;
				p += 2;
				const uint8_t *bm = bones + bone * 0x30 + 0x10;
				GteSetLightMatrix(bm);
				GteSetBackground(*(const int32_t *)(bm + 0x14), *(const int32_t *)(bm + 0x18), *(const int32_t *)(bm + 0x1C));
				int32_t nv = *(const int16_t *)p;
				p += 2;
				for (int32_t k = nv; k > 0; k--)
				{
					*(int16_t *)(s + 0x30) = *(const int16_t *)p;
					*(int16_t *)(s + 0x32) = *(const int16_t *)(p + 2);
					*(int16_t *)(s + 0x34) = *(const int16_t *)(p + 4);
					p += 6;
					GteLoadV0(s + 0x30);
					GteMVMVA_LightV0Bk();
					GteStoreIR123(s + 0x30);
					int32_t a = (int32_t)(((uint32_t)*(const uint16_t *)(s + 0x66) + 12u * *(const uint16_t *)(s + 0x32)) & 0xFFF);
					*(int32_t *)(s + 0x48) = a;
					*(int32_t *)(s + 0x40) = Div2p18(mul32(ComputeSin(a), *(const int16_t *)(s + 0x64)));
					*(int32_t *)(s + 0x44) = Div2p18(mul32(ComputeCos(*(const int32_t *)(s + 0x48)), *(const int16_t *)(s + 0x64)));
					*(int16_t *)(v + 6) = *(const int16_t *)(s + 0x32);
					*(int16_t *)(v + 4) = (int16_t)(*(const int16_t *)(s + 0x30) + (int16_t)*(const int32_t *)(s + 0x40));
					*(int16_t *)(v + 8) = (int16_t)(*(const int16_t *)(s + 0x34) + (int16_t)*(const int32_t *)(s + 0x44));
					v += 0x10;
				}
			}
			// polygons: 12-byte header (triangle count, quad count), 16-byte triangles, 20-byte quads
			p = (const uint8_t *)(((uintptr_t)p + 3) & ~(uintptr_t)3);
			int32_t tris = *(const int16_t *)p;
			int32_t quads = *(const int16_t *)(p + 2);
			p += 0xC;
			uint8_t *pk = (uint8_t *)cursor;
			for (int32_t k = tris; k > 0; k--)
			{
				uint32_t i2 = *(const uint16_t *)(p + 4) & 0xFFF;
				uint32_t i1 = *(const uint16_t *)(p + 2) & 0xFFF;
				uint32_t i0 = *(const uint16_t *)p & 0xFFF;
				*(uint32_t *)(s + 0x5C) = i2;
				*(uint32_t *)(s + 0x58) = i1;
				*(uint32_t *)(s + 0x54) = i0;
				GteLoadV012(verts + i0 * 16 + 4, verts + i1 * 16 + 4, verts + i2 * 16 + 4);
				GteRTPT();
				GteNCLIP();
				GteReadMAC0(s + 0x4C);
				if (*(const int32_t *)(s + 0x4C) > 0)
				{
					GteStoreSXY012_FT3(pk);
					GteAVSZ3();
					GteReadOTZWord(s + 0x50);
					uint32_t z = *(const uint32_t *)(s + 0x50) >> 2;
					*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 8);
					*(uint32_t *)(s + 0x50) = z;
					*(uint32_t *)pk = 0x7000000;
					*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 0xC);
					*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 6);
					*(uint32_t *)(pk + 4) = *(const uint32_t *)(s + 0x68);
					if (p[0xF] & 2) pk[7] |= 2;
					InsertPrimAutoDepth(ot + z * 4, pk);
					pk += 0x20;
				}
				p += 0x10;
			}
			for (int32_t k = quads; k > 0; k--)
			{
				uint32_t i2 = *(const uint16_t *)(p + 4) & 0xFFF;
				uint32_t i1 = *(const uint16_t *)(p + 2) & 0xFFF;
				uint32_t i0 = *(const uint16_t *)p & 0xFFF;
				*(uint32_t *)(s + 0x5C) = i2;
				*(uint32_t *)(s + 0x58) = i1;
				*(uint32_t *)(s + 0x54) = i0;
				GteLoadV012(verts + i0 * 16 + 4, verts + i1 * 16 + 4, verts + i2 * 16 + 4);
				GteRTPT();
				GteNCLIP();
				GteReadMAC0(s + 0x4C);
				if (*(const int32_t *)(s + 0x4C) > 0)
				{
					GteStoreSXY012_FT4(pk);
					uint32_t i3 = *(const uint16_t *)(p + 6) & 0xFFF;
					*(uint32_t *)(s + 0x60) = i3;
					GteLoadV0(verts + i3 * 16 + 4);
					GteRTPS();
					GteReadSXY2(pk + 0x20);
					GteAVSZ4();
					GteReadOTZWord(s + 0x50);
					uint32_t z = *(const uint32_t *)(s + 0x50) >> 2;
					*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 8);
					*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 0xC);
					*(uint32_t *)(s + 0x50) = z;
					*(uint32_t *)pk = 0x9000000;
					*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 0x10);
					*(uint16_t *)(pk + 0x24) = *(const uint16_t *)(p + 0x12);
					*(uint32_t *)(pk + 4) = *(const uint32_t *)(s + 0x6C);
					if (p[0xF] & 2) pk[7] |= 2;
					InsertPrimAutoDepth(ot + z * 4, pk);
					pk += 0x28;
				}
				p += 0x14;
			}
			cursor = (uint32_t)pk;
		}
		*mc->frame_cursor = cursor;
		FieldFree(0x74);
	}

	// sub_8BF910: shadow, body (+0x64) and weapon (+0x78) of the target with the wobble
	static void DrawOverlay(OverlayCtx *mc)
	{
		uint8_t *entity = mc->entity;
		CopyWorld(mc, entity);
		if (mc->slot != 0xFF) BuildBoneMatricesFromPose(entity + 0x60);
		else if (BattleReadAnimation(entity + 0x60, entity + 0x6C)) PreBattleReadAnimation(entity + 0x60, entity + 0x6C, (uint16_t)mc->anim);
		ComposeAffineTransform(&Camera(), &mc->world, &mc->view);
		if (!(entity[0] & 0x20)) *mc->frame_cursor = DrawShadow(entity, RenderOT(0x4040), 0x10, *mc->frame_cursor);
		BuildLightMatrix(mc);
		DrawOverlayObject(*(const uint8_t *const *)(entity + 0x64), RenderOT(0x44), 4, mc);
		const uint8_t *weapon = *(const uint8_t *const *)(entity + 0x78);
		if (weapon) DrawOverlayObject(*(const uint8_t *const *)(weapon + 4), RenderOT(0x44), 4, mc);
		if (mc->slot != 0xFF) BuildBoneMatricesFromPose(entity + 0x60);
	}

	static uint32_t __cdecl OverlayTask(TaskNode *n)
	{
		FxNode *o = (FxNode *)n;
		OverlayCtx &mc = Overlay();
		switch (o->phase)
		{
		case 0: // sub_8C0080: hide the target, the overlay draws it
		{
			uint8_t slot = o->slot;
			uint8_t *entity = Entity(slot);
			InitOverlay(&mc, OVERLAY_A00, (uint8_t *)OVERLAY_SPRITE, OVERLAY_VERTS, entity, slot);
			o->flags |= 8;
			entity[0] |= 4;
			o->phase++;
			break;
		}
		case 1: // sub_8C01D0: amplitude + 0x200 per tick up to 0x1000
			mc.wobble_amp = (int16_t)(mc.wobble_amp + 0x200);
			if (mc.wobble_amp >= 0x1000)
			{
				mc.wobble_amp = 0x1000;
				o->phase++;
			}
			break;
		case 2: // sub_8C0200: until tick 28
			if (o->counter >= 0x1C) o->phase++;
			break;
		case 3: // sub_8C0210: amplitude - 0x100 per tick; at 0 the target is shown again
		{
			int16_t a = (int16_t)(mc.wobble_amp - 0x100);
			mc.wobble_amp = a;
			if (a <= 0)
			{
				*(uint16_t *)Entity(o->slot) &= 0xFFFB;
				((RootNode *)o->root)->busy = 0;
				o->flags = (uint16_t)((o->flags & ~8u) | 1);
				o->phase++;
			}
			break;
		}
		default: break; // nullsub_1818
		}
		mc.wobble_phase = (int16_t)((uint16_t)(mc.wobble_phase + 0xA0) & 0xFFF);
		if (o->flags & 8)
		{
			// 30 fps layer: see mag013_dark_mist_held.inc
			FX_HELD(held_note_draw(ORIG_Overlay, o);)
			DrawOverlay(&mc);
		}
		return Finish(o);
	}

	// ------------------------------------------------------------------
	// Mist controller (0x8BE960)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ControlTask(TaskNode *n)
	{
		FxNode *c = (FxNode *)n;
		GetEffectSpawnPosition(CasterEntity(c), 0xF0, 0x400, c->pos);
		switch (c->phase)
		{
		case 0: c->phase++; break; // sub_8BF840
		case 1: // sub_8BF850: the overlay at 16
			if (c->counter >= 0x10)
			{
				AddEffectTask(QControl(), ORIG_Overlay, 0x30, c);
				c->phase++;
			}
			break;
		case 2: // sub_8C0280: done after 30
			if (c->counter > 0x1E)
			{
				c->flags |= 1;
				c->phase++;
			}
			break;
		default: break; // nullsub_1819
		}
		if (c->counter == 0) AddEffectTask(QModels(), ORIG_Cloud, 0x60, c);
		if (c->counter <= 0xC)
		{
			Particle *p = (Particle *)AddEffectTask(QParticles(), ORIG_MistA, 0x68, c);
			p->variant = 0;
			p = (Particle *)AddEffectTask(QParticles(), ORIG_MistA, 0x68, c);
			p->variant = 1;
			p = (Particle *)AddEffectTask(QParticles(), ORIG_MistB, 0x68, c);
			p->variant = 0;
			p = (Particle *)AddEffectTask(QParticles(), ORIG_MistB, 0x68, c);
			p->variant = 1;
		}
		if (c->counter == 0x10 || c->counter == 0x17) AddEffectTask(QControl(), ORIG_SpawnerE, 0x30, c);
		if (c->counter == 0x10) AddEffectTask(QControl(), ORIG_SpawnerC, 0x30, c);
		if (c->counter == 0x1E) AddEffectTask(QControl(), ORIG_SpawnerD, 0x30, c);
		return Finish(c);
	}

	// ------------------------------------------------------------------
	// Emitter (0x8BE3E0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl EmitterTask(TaskNode *n)
	{
		EmitterNode *e = (EmitterNode *)n;
		EmitterUpdatePos(e);
		ComputeModelBounds(e);
		switch (e->h.phase)
		{
		case 0: // sub_8BE470: the swirl
			AddEffectTask(QModels(), ORIG_Swirl, 0x60, e);
			e->h.phase++;
			break;
		case 1: // MAG_013_sub_8BE930: the mist controller at 25
			if (e->h.counter >= 0x19)
			{
				AddEffectTask(QControl(), ORIG_Control, 0x30, e);
				e->h.phase++;
			}
			break;
		case 2: // MAG_013_sub_8C02B0: damage at 70
			if (e->h.counter >= 0x46)
			{
				ApplyActionResultToTarget(e->h.ctx->actions[e->h.action].targets);
				e->h.flags |= 1;
				e->h.phase++;
			}
			break;
		default: break; // nullsub_1820
		}
		if (e->h.counter == 0) BdPlaySE(SOUND_Mist, 0, 0x80);
		return Finish(&e->h);
	}

	// ------------------------------------------------------------------
	// Root (0x8BE2D0)
	// ------------------------------------------------------------------
	static void RootPhase(RootNode *r)
	{
		switch (r->h.phase)
		{
		case 0: r->h.phase++; break; // MAG_013_sub_8BE3A0
		case 1: // MAG_013_sub_8BE3B0: emitter of the current action
			r->busy = 1;
			AddEffectTask(QEmitters(), ORIG_Emitter, 0x58, r);
			r->h.phase++;
			break;
		case 2: // MAG_013_sub_8C0300: when its overlay is over, the next action (back to phase 1)
			if (r->busy == 0)
			{
				int8_t a = r->h.action;
				if ((int16_t)a < r->last_action)
				{
					r->h.action = (int8_t)(a + 1);
					r->h.phase--;
				}
				else r->h.phase++;
			}
			break;
		case 3: // MAG_013_sub_8C0330: until every queue is empty
			if (r->alive == 0)
			{
				r->h.flags |= 1;
				r->h.phase++;
			}
			break;
		default: break; // nullsub_1821
		}
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag013_dark_mist_held.inc
		FX_HELD(held_note_root();)
		if (r->tick & 1) PacketCursor() = ArenaA();
		else PacketCursor() = ArenaB();
		UpdateTargetPosFromBones(r);
		RootPhase(r);
		r->alive = (int16_t)ExecuteTaskQueue(QEmitters());
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QControl()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QParticles()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QModels()));
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

	void register_mag013_dark_mist()
	{
		register_port(mist013::ORIG_Root, (void *)mist013::RootTask, "M013 RootTask", 13);
		register_port(mist013::ORIG_Emitter, (void *)mist013::EmitterTask, "M013 EmitterTask", 13);
		register_port(mist013::ORIG_Swirl, (void *)mist013::SwirlTask, "M013 SwirlTask", 13);
		register_port(mist013::ORIG_Control, (void *)mist013::ControlTask, "M013 ControlTask", 13);
		register_port(mist013::ORIG_Cloud, (void *)mist013::CloudTask, "M013 CloudTask", 13);
		register_port(mist013::ORIG_MistA, (void *)mist013::MistATask, "M013 MistATask", 13);
		register_port(mist013::ORIG_MistB, (void *)mist013::MistBTask, "M013 MistBTask", 13);
		register_port(mist013::ORIG_SpawnerC, (void *)mist013::SpawnerCTask, "M013 SpawnerCTask", 13);
		register_port(mist013::ORIG_PuffC, (void *)mist013::PuffCTask, "M013 PuffCTask", 13);
		register_port(mist013::ORIG_SpawnerD, (void *)mist013::SpawnerDTask, "M013 SpawnerDTask", 13);
		register_port(mist013::ORIG_PuffD, (void *)mist013::PuffDTask, "M013 PuffDTask", 13);
		register_port(mist013::ORIG_SpawnerE, (void *)mist013::SpawnerETask, "M013 SpawnerETask", 13);
		register_port(mist013::ORIG_Stream, (void *)mist013::StreamTask, "M013 StreamTask", 13);
		register_port(mist013::ORIG_Overlay, (void *)mist013::OverlayTask, "M013 OverlayTask", 13);
		// 30 fps layer: see mag013_dark_mist_held.inc
		FX_HELD(register_mag013_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag013_dark_mist_held.inc"
#endif
