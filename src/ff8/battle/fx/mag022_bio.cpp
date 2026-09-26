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

// Effect 22: Bio (spell, MAG_022_*, file loaded by 0x89A690).
//
// Structure (setup MAG_022_BIO 0x89A6B0, module code 0x89A690..0x89DED0). Every node starts with
// the shared effect-library header (Effect_AddTaskAndInitFromCtx 0x8DC540: context, root, emitter,
// parent, position, counter, flags, child count, phase, action / target, caster / target slot);
// every task runs its current phase function (node +0x29) from a small table, and ends when it is
// flagged done (+0x26 bit0) and has no live children.
//   Root (0x89A7B0) - alternates the packet arena (magic buffer + 0 / + 0xC000), follows the caster's
//     bones, spawns one emitter per action (the next one when the previous target's acid overlay is
//     over), runs the five queues, ends when they are empty.
//   Emitter (0x89A8D0), one per action, on its first target: model bounds -> acid centre, size and
//     scale; 0: stage wobble + first acid pool, 2: bubble spawner + pool, 4..10: pools 2..5,
//     30: palette cycler + boiling blob, 32: two mist spawners, 34: target overlay; damage once
//     the blob has burst. Sound at its first tick.
//   Pool (0x89AAE0) - an acid puddle mesh morphed between key shapes (GTE interpolation of the vertex
//     arrays) at an offset rotating around the target; it then collapses into a flattened splash.
//   Blob (0x89C270) - a boiling acid mesh (random key shapes) on the target with a wobbling scale;
//     after 5 cycles it bursts into droplets (from its vertices) and a full-screen flash.
//   Stage wobble (0x89B8A0) - ramps the battle stage's sine wobble amplitude (0x1D98990 table) up,
//     holds it until the blob burst, then back down.
//   Palette (0x89CA70) - blends the acid texture's CLUT between three palettes and uploads it.
//   Bubble spawner (0x89BA80) - bubbles (0x89BF20: flipbook that spawns a splash when it pops) and
//     splashes (0x89BD20: flipbook circling upwards) around the target.
//   Mist spawners (0x89CD60 / 0x89D020) - mist puffs inside (0x89CE60) and flying off the surface of
//     a sphere around the target (0x89D120).
//   Droplet (0x89C930) - flipbook thrown out of the blob, falling.
//   Flash (0x89C650) - full-screen additive quad, colour script 0x162693C.
//   Overlay (0x89D3D0) - hides the target model (entity flag 4) and draws it itself: shadow, then
//     every object with its vertices displaced by a sine wobble whose amplitude rises, holds and
//     falls; the target reappears when it is over.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2704568..0x2714F24 (file pointer, packet cursor 0x270456C, node pools, queues,
// overlay vertex buffer 0x270E560, arena bases 0x2714BA0 / 0x2714BA4, overlay context 0x2714E30);
// the mesh vertex buffers and the palette TIM live in the exe's data (0x16146E8..0x1623784).

#include "mag_common.h"

namespace ff8fx
{
namespace bio022
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
		FxNode *emitter;       // +0x14 first node under the root on this node's line (itself for one)
		FxNode *parent;        // +0x18 (its +0x28 counts the live children)
		int16_t pos[4];        // +0x1C x, y, z (copied from the parent)
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
	struct EmitterNode // pool of 4 nodes of 0x8C bytes
	{
		FxNode h;
		uint8_t anchor[0x18];  // +0x30 target bone anchor (MAG_001_CURE_Emitter_UpdatePos)
		int16_t bmin[4];       // +0x48 model bounds (MAG_001_CURE_Emitter_ComputeModelBounds)
		int16_t bmax[4];       // +0x50
		Mat4x3 spin_m;         // +0x58 rotation about Y by `spin`
		int16_t center[4];     // +0x78 acid centre
		int16_t acid_done;     // +0x80 the blob has burst
		int16_t spin;          // +0x82 turns by -0x40 per tick
		int16_t radius;        // +0x84 half height, at most 0xA00
		int16_t scale;         // +0x86
		int16_t big;           // +0x88 tall target: the blob is drawn as a puddle
		int16_t pad8A;
	};
	struct WobbleNode // pool of 10 nodes of 0x38 bytes (every 0x2714BA8 task)
	{
		FxNode h;              // stage wobble: h.pos[0] = amplitude, h.pos[1] = hold ticks
		int16_t pad30;
		int16_t from;          // +0x32 palette cycler: palette blended from
		int16_t to;            // +0x34 ... to
		int16_t t;             // +0x36 ... blend 0..0x1000
	};
	struct Particle // pool of 300 nodes of 0x70 bytes
	{
		FxNode h;
		int32_t scale[3];      // +0x30
		int32_t pad3C;
		int16_t rot[3];        // +0x40 x, y, z (bubble)
		int16_t pad46;
		uint8_t pad48[4];
		uint32_t seq;          // +0x4C sprite sequence
		int16_t frame;         // +0x50
		int16_t last;          // +0x52
		int16_t shade;         // +0x54 TransformCameraByShadowRotation offset
		int16_t pad56;
		int16_t vel[3];        // +0x58
		int16_t pad5E;
		int16_t acc[3];        // +0x60
		int16_t pad66;
		int16_t variant;       // +0x68
		int16_t angle;         // +0x6A splash: circling angle
		int16_t speed;         // +0x6C
		int16_t pad6E;
	};
	struct MeshNode // pool of 20 nodes of 0x98 bytes (pool and blob)
	{
		FxNode h;
		int32_t scale[3];      // +0x30
		uint8_t pad3C[8];
		int16_t rot[3];        // +0x44
		int16_t pad4A;
		uint8_t *mesh;         // +0x4C output mesh (morphed vertices + polygons)
		int16_t frame;         // +0x50
		int16_t pad52;
		int16_t offs[3];       // +0x54 pool: offset from the centre at the end of the collapse
		int16_t pad5A;
		int16_t offs_to[3];    // +0x5C pool: offset at first
		int16_t pad62;
		int16_t wob[3];        // +0x64 blob: scale wobble angles
		int16_t pad6A;
		const uint8_t *src;    // +0x6C morph key shapes
		const uint8_t *dst;    // +0x70
		int16_t key_a;         // +0x74
		int16_t key_b;         // +0x76
		int16_t spin;          // +0x78 pool: rotation of the offset about Y
		uint8_t pad7A[8];
		int16_t spin_d;        // +0x82 pool: spin left to turn during the collapse
		int16_t t;             // +0x84 morph 0..0x1000
		uint8_t pad86[4];
		int16_t cycles;        // +0x8A blob: key shapes reached
		int16_t variant;       // +0x8C pool 0..5
		int16_t step;          // +0x8E
		int16_t steps;         // +0x90
		int16_t frac;          // +0x92 step * 0x1000 / steps
		int16_t lit;           // +0x94 pool: drawn as a puddle
		int16_t scale_d;       // +0x96
	};
	struct FlashNode // pool of 2 nodes of 0x3C bytes
	{
		FxNode h;
		const uint32_t *script; // +0x30 colour words; hi byte 0xFF = end, 0xFE = wait (byte 2 = until tick)
		uint32_t colour;        // +0x34
		int16_t idx;            // +0x38
		int16_t pad3A;
	};
	// overlay context 0x2714E30 (0xF4 bytes, set up by 0x89DC30)
	struct OverlayCtx
	{
		uint32_t a00;           // +0x00 0x2705160 (not read by the draw)
		uint8_t *sprite;        // +0x04 0x2714DE8: +0x04 = vertex buffer 0x270E560
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
	static_assert(sizeof(FxNode) == 0x30 && sizeof(RootNode) == 0x64 && sizeof(EmitterNode) == 0x8C, "Bio nodes");
	static_assert(sizeof(WobbleNode) == 0x38 && sizeof(Particle) == 0x70 && sizeof(MeshNode) == 0x98, "Bio nodes");
	static_assert(sizeof(FlashNode) == 0x3C && sizeof(OverlayCtx) == 0xF4, "Bio nodes");

	// ------------------------------------------------------------------
	// Module globals
	// ------------------------------------------------------------------
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x270456C); }
	inline uint32_t &ArenaA() { return var<uint32_t>(0x2714BA4); } // magic buffer (Magic_TextureOFF)
	inline uint32_t &ArenaB() { return var<uint32_t>(0x2714BA0); } // magic buffer + 0xC000
	inline TaskQueue *QEmitters() { return (TaskQueue *)0x2714960; }  // 4 x 0x8C
	inline TaskQueue *QControl() { return (TaskQueue *)0x2714BA8; }   // 10 x 0x38
	inline TaskQueue *QParticles() { return (TaskQueue *)0x270E540; } // 300 x 0x70
	inline TaskQueue *QMeshes() { return (TaskQueue *)0x2705150; }    // 20 x 0x98
	inline TaskQueue *QFlash() { return (TaskQueue *)0x27044E0; }     // 2 x 0x3C
	inline OverlayCtx &Overlay() { return var<OverlayCtx>(0x2714E30); }
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_Root = 0x89A7B0;
	static const uint32_t ORIG_Emitter = 0x89A8D0;
	static const uint32_t ORIG_Pool = 0x89AAE0;
	static const uint32_t ORIG_Wobble = 0x89B8A0;
	static const uint32_t ORIG_BubbleSpawner = 0x89BA80;
	static const uint32_t ORIG_Splash = 0x89BD20;
	static const uint32_t ORIG_Bubble = 0x89BF20;
	static const uint32_t ORIG_Blob = 0x89C270;
	static const uint32_t ORIG_Flash = 0x89C650;
	static const uint32_t ORIG_Droplet = 0x89C930;
	static const uint32_t ORIG_Palette = 0x89CA70;
	static const uint32_t ORIG_MistSpawnerA = 0x89CD60;
	static const uint32_t ORIG_MistA = 0x89CE60;
	static const uint32_t ORIG_MistSpawnerB = 0x89D020;
	static const uint32_t ORIG_MistB = 0x89D120;
	static const uint32_t ORIG_Overlay = 0x89D3D0;

	static const void *const SOUND_Bio = (const void *)0x16146E4;
	static const uint32_t STAGE_WOBBLE = 0x1D98990;       // 4 stage groups of 0x2C bytes: +2 amplitude
	static const uint32_t PALETTE_TIM = 0x16146E8;        // acid CLUT (uploaded every tick)
	static const uint32_t PALETTES = 0x1614F68;           // 3 source palette TIMs
	static const uint32_t POOL_KEYS = 0x16189CC;          // pool key shapes 0..6
	static const uint32_t BLOB_KEYS = 0x1626000;          // blob key shapes 0..4
	static const uint32_t MESH_Pool = 0x1615CE4, MESH_Splat = 0x1617E3C, MESH_Blob = 0x1619888, MESH_BlobBoil = 0x161F750;
	static const uint32_t KEY_BlobA = 0x16189E8, KEY_BlobB = 0x161E8B0;
	static const uint32_t SEQ_SplashA = 0x1614F74, SEQ_SplashB = 0x1615120;
	static const uint32_t SEQ_BubbleA = 0x16152CC, SEQ_BubbleB = 0x1615440;
	static const uint32_t SEQ_Droplet = 0x16156F8, SEQ_MistA = 0x1615858, SEQ_MistB = 0x16155B4;
	static const uint32_t FLASH_Script = 0x162693C;
	static const uint32_t COUNT_Bubbles = 0x162694C, COUNT_Splashes = 0x162696C, COUNT_MistA = 0x162698C, COUNT_MistB = 0x16269AC;
	static const uint32_t OVERLAY_A00 = 0x2705160, OVERLAY_SPRITE = 0x2714DE8, OVERLAY_VERTS = 0x270E560;


	// ------------------------------------------------------------------
	// Engine / effect-library functions (original addresses)
	// ------------------------------------------------------------------
	// Effect_AddTaskAndInitFromCtx (0x8DC540): new node of `size` bytes in `q`, zeroed, header
	// inherited from `parent` (the parent's child count goes up)
	inline FxNode *AddEffectTask(TaskQueue *q, uint32_t task, int32_t size, void *parent) { return fn<FxNode *(__cdecl *)(TaskQueue *, uint32_t, int32_t, void *)>(0x8DC540)(q, task, size, parent); }
	// Effect_ReleaseLinkedTask (0x8DC530): the parent's child count goes down
	inline void ReleaseLinkedTask(void *n) { fn<void (__cdecl *)(void *)>(0x8DC530)(n); }
	// the node's target / caster anchor from their bones 0xF0 / 0xF1 (+0x30..+0x45)
	inline void EmitterUpdatePos(void *n) { fn<void (__cdecl *)(void *)>(0x8DC610)(n); }        // MAG_001_CURE_Emitter_UpdatePos
	inline void UpdateTargetPosFromBones(void *n) { fn<void (__cdecl *)(void *)>(0x8DC740)(n); } // (caster, ctx +0)
	inline void ComputeModelBounds(void *n) { fn<void (__cdecl *)(void *)>(0x8DC870)(n); }      // MAG_001_CURE_Emitter_ComputeModelBounds (+0x48..+0x57)
	inline void CopyMidpointFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC700)(n, out); }
	inline int32_t BoundsCenterY(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCB80)(n); }  // Effect_GetTargetBoundsCenterWorldY
	inline int32_t BoundsHalfHeight(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCA50)(n); } // MAG_022_sub_8DCA50
	inline int32_t BoundsBaseY(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCAF0)(n); }      // sub_8DCAF0: entity +0x1E + bounds min y
	// rotation matrices (MATRIX, 0x20 bytes): identity, then m = R(angle) * m
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
	// software GTE
	inline void GteLoadV012Block(const void *v) { fn<void (__cdecl *)(const void *)>(0x45E020)(v); } // V0..V2 from 3 consecutive SVECTORs
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteLoadV0Dwords(const void *v) { fn<void (__cdecl *)(const void *)>(0x45E060)(v); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }       // GTE_ReadOTZ (dword)
	inline void GteStoreSXY012_GT3(void *p) { fn<void (__cdecl *)(void *)>(0x45E300)(p); }       // p +8, +0x14, +0x20
	inline void GteStoreSXY012_FT3(void *p) { fn<void (__cdecl *)(void *)>(0x45E2E0)(p); }       // p +8, +0x10, +0x18
	inline void GteStoreSXY012_FT4(void *p) { fn<void (__cdecl *)(void *)>(0x45E320)(p); }       // (same layout)
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteSetBackground(int32_t x, int32_t y, int32_t z) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DCF0)(x, y, z); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }                    // IR = L * V0 + BK
	inline void GteMVMVA_RotV0Tr2() { fn<void (__cdecl *)()>(0x460830)(); }                     // GTE_MVMVA_RotV0_Tr_2
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }
	inline void GteLoadIRFromColumn(const void *m) { fn<void (__cdecl *)(const void *)>(0x45E180)(m); }
	inline void GteStoreIRToColumn(void *m) { fn<void (__cdecl *)(void *)>(0x45E470)(m); }
	inline void GteLoadIRBytes(const void *rgb) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(rgb); }
	inline void GteStoreIRBytes(void *rgb) { fn<void (__cdecl *)(void *)>(0x45E410)(rgb); }
	// drawing
	inline void InsertPrimAltViewport(uint32_t bucket, void *p) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, p); }
	inline uint32_t GetTPage(int32_t tp, int32_t abr, int32_t x, int32_t y) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x45C690)(tp, abr, x, y); }
	inline void SetDrawMode(void *p, int32_t dfe, int32_t dtd, uint32_t tpage, const void *tw) { fn<void (__cdecl *)(void *, int32_t, int32_t, uint32_t, const void *)>(0x45BFC0)(p, dfe, dtd, tpage, tw); }
	inline void QueueTIMUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(0x505E30)(tim); } // Battle_QueueTIMUpload_GetEOF
	// battle models
	inline int32_t BattleReadAnimation(void *hdr, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(0x508F90)(hdr, cmd); }
	inline void PreBattleReadAnimation(void *hdr, void *cmd, int32_t anim) { fn<void (__cdecl *)(void *, void *, int32_t)>(0x509440)(hdr, cmd, anim); }
	// sub_5088A0: an entity's ground shadow (returns the packet cursor)
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }

	// x / 4096 / 10 as compiled (0x66666667, >> 2)
	static int16_t Div40960(int32_t x)
	{
		int32_t q = x / 4096;
		int32_t hi = (int32_t)(((int64_t)q * 0x66666667) >> 32) >> 2;
		hi += (int32_t)((uint32_t)hi >> 31);
		return (int16_t)hi;
	}

	// x / -12 as compiled (0xD5555555, >> 1)
	static int16_t DivNeg12(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * (int32_t)0xD5555555) >> 32) >> 1;
		hi += (int32_t)((uint32_t)hi >> 31);
		return (int16_t)hi;
	}

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

	// sub_89BEE0: next flipbook frame; past the last one the node is hidden (frame stays last)
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
}
}

#ifdef FF8_FX_HELD
#include "mag022_bio_held.h"
#endif

namespace ff8fx
{
namespace bio022
{
	// ------------------------------------------------------------------
	// Meshes (pool and blob)
	// ------------------------------------------------------------------
	// sub_89B0D0: out vertices = src * (1 - t) + dst * t (GTE interpolation, 4.12)
	static void Morph(const uint8_t *src, const uint8_t *dst, uint8_t *out, int32_t t)
	{
		const uint8_t *a = src + 8;
		int32_t count = *(const int32_t *)(src + 4);
		const uint8_t *b = dst + 8;
		uint8_t *o = out + 8;
		int32_t *s = (int32_t *)FieldAlloc(8);
		int32_t t16 = (int16_t)t;
		s[1] = t16;
		s[0] = 0x1000 - t16;
		for (int32_t k = count; k > 0; k--)
		{
			GteSetIR0(s[0]);
			GteLoadIR123(a);
			GteGPF();
			a += 8;
			GteSetIR0(s[1]);
			GteLoadIR123(b);
			GteGPL();
			b += 8;
			GteStoreIR123(o);
			o += 8;
		}
		FieldFree(8);
	}

	// mesh layout: +0 vertex block size, +4 vertex count, +8 vertices (8 bytes), then the polygon
	// count and the polygons (0x1C bytes: +0 colour/code, +4..+8 vertex indices * 2, +0xA uv2,
	// +0xC uv0/clut, +0x10 uv1/tpage, +0x14 / +0x18 colours)
	static const uint8_t *MeshPolys(const uint8_t *mesh, int32_t *count)
	{
		int32_t vb = *(const int32_t *)mesh / 4;
		*count = *(const int32_t *)(mesh + vb * 4 + 0x18);
		return mesh + vb * 4 + 0x1C;
	}

	// gouraud textured triangle from the three projected vertices of a polygon
	static uint8_t *MeshTriangle(const uint8_t *poly, uint8_t *pkt, uint32_t ot, uint8_t *otz)
	{
		GteStoreSXY012_GT3(pkt);
		*(uint32_t *)(pkt + 4) = *(const uint32_t *)poly;
		*(uint32_t *)(pkt + 0x10) = *(const uint32_t *)(poly + 0x14);
		*(uint32_t *)(pkt + 0x1C) = *(const uint32_t *)(poly + 0x18);
		pkt[7] = poly[3];
		*(uint32_t *)pkt = 0x9000000;
		*(uint32_t *)(pkt + 0xC) = *(const uint32_t *)(poly + 0xC);
		*(uint32_t *)(pkt + 0x18) = *(const uint32_t *)(poly + 0x10);
		*(uint16_t *)(pkt + 0x24) = *(const uint16_t *)(poly + 0xA);
		GteAVSZ3();
		GteReadOTZWord(otz);
		int32_t z = *(int32_t *)otz >> 2;
		*(int32_t *)otz = z;
		InsertPrimAutoDepth(ot + z * 4, pkt);
		return pkt + 0x28;
	}

	// sub_89AB90: the mesh at pos, rotated (rot), scaled (scale), seen by the camera
	static void DrawMesh(MeshNode *m)
	{
		uint32_t ot = RenderOT(0x44);
		uint8_t *pkt = (uint8_t *)PacketCursor();
		if (m->h.flags & 4) return;
		uint8_t *s = (uint8_t *)FieldAlloc(0x3C);
		Mat4x3 *mat = (Mat4x3 *)s;
		BuildRotationMatrixFromAngles(m->rot, mat);
		mat->t[0] = m->h.pos[0];
		mat->t[1] = m->h.pos[1];
		mat->t[2] = m->h.pos[2];
		Scale3DMatrix(mat, m->scale);
		ComposeAffineTransform(&Camera(), mat, mat);
		GteSetRotMatrix(mat);
		GteSetTransVector(mat);
		const uint8_t *mesh = m->mesh;
		const uint8_t *verts = mesh + 8;
		int32_t count;
		const uint8_t *poly = MeshPolys(mesh, &count);
		int32_t otz;
		for (int32_t k = count; k > 0; k--)
		{
			memcpy(s + 0x20, verts + (*(const uint16_t *)(poly + 4) >> 1) * 8, 8);
			memcpy(s + 0x28, verts + (*(const uint16_t *)(poly + 6) >> 1) * 8, 8);
			memcpy(s + 0x30, verts + (*(const uint16_t *)(poly + 8) >> 1) * 8, 8);
			GteLoadV012Block(s + 0x20);
			GteRTPT();
			GteNCLIP();
			GteReadMAC0(s + 0x38);
			if (*(int32_t *)(s + 0x38) > 0) pkt = MeshTriangle(poly, pkt, ot, (uint8_t *)&otz);
			poly += 0x1C;
		}
		PacketCursor() = (uint32_t)pkt;
		FieldFree(0x3C);
	}

	// a vertex already in world space sags: above y = -0x100 it spreads away from the centre
	// (by (y + 0x100) / 0x200, y at most 0x200) and is flattened onto the ground (y at most 0)
	static void Sag(int16_t *v, const MeshNode *m)
	{
		int16_t y = v[1];
		if (y <= -0x100) return;
		if (y > 0x200) v[1] = 0x200;
		int32_t f = v[1] + 0x100;
		int16_t x = v[0];
		int32_t d = mul32((int32_t)x - m->h.pos[0], f);
		v[0] = (int16_t)(((d + ((d >> 31) & 0x1FF)) >> 9) + x);
		int16_t z = v[2];
		d = mul32((int32_t)z - m->h.pos[2], f);
		v[2] = (int16_t)(((d + ((d >> 31) & 0x1FF)) >> 9) + z);
		if (v[1] > 0) v[1] = 0;
	}

	// sub_89AD30: the mesh as a puddle: vertices to world space (light matrix = the model matrix,
	// background = its translation), sagged onto the ground, then seen by the camera
	static void DrawMeshPuddle(MeshNode *m)
	{
		uint8_t *pkt = (uint8_t *)PacketCursor();
		uint32_t ot = RenderOT(0x44);
		if (m->h.flags & 4) return;
		uint8_t *s = (uint8_t *)FieldAlloc(0x3C);
		Mat4x3 *mat = (Mat4x3 *)s;
		BuildRotationMatrixFromAngles(m->rot, mat);
		mat->t[0] = m->h.pos[0];
		mat->t[1] = m->h.pos[1];
		mat->t[2] = m->h.pos[2];
		Scale3DMatrix(mat, m->scale);
		const uint8_t *mesh = m->mesh;
		const uint8_t *verts = mesh + 8;
		int32_t count;
		const uint8_t *poly = MeshPolys(mesh, &count);
		GteSetLightMatrix(mat);
		GteSetBackground(mat->t[0], mat->t[1], mat->t[2]);
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		int32_t otz;
		for (int32_t k = count; k > 0; k--)
		{
			memcpy(s + 0x20, verts + (*(const uint16_t *)(poly + 4) >> 1) * 8, 8);
			memcpy(s + 0x28, verts + (*(const uint16_t *)(poly + 6) >> 1) * 8, 8);
			memcpy(s + 0x30, verts + (*(const uint16_t *)(poly + 8) >> 1) * 8, 8);
			for (int i = 0; i < 3; i++)
			{
				GteLoadV0(s + 0x20 + 8 * i);
				GteMVMVA_LightV0Bk();
				GteStoreIR123(s + 0x20 + 8 * i);
			}
			for (int i = 0; i < 3; i++) Sag((int16_t *)(s + 0x20 + 8 * i), m);
			GteLoadV012(s + 0x20, s + 0x28, s + 0x30);
			GteRTPT();
			GteNCLIP();
			GteReadMAC0(s + 0x38);
			if (*(int32_t *)(s + 0x38) > 0) pkt = MeshTriangle(poly, pkt, ot, (uint8_t *)&otz);
			poly += 0x1C;
		}
		PacketCursor() = (uint32_t)pkt;
		FieldFree(0x3C);
	}

	// ------------------------------------------------------------------
	// Pool (0x89AAE0)
	// ------------------------------------------------------------------
	// sub_89B170: pos = acid centre + Ry(spin) * (offs_to + (offs - offs_to) * frac)
	static void PoolUpdatePos(MeshNode *m)
	{
		int16_t v[4];
		memcpy(v, m->offs_to, 8);
		int32_t frac = m->frac;
		FxNode *e = m->h.emitter;
		v[0] = (int16_t)(v[0] + (int16_t)(mul32((int32_t)m->offs[0] - m->offs_to[0], frac) / 4096));
		v[1] = (int16_t)(v[1] + (int16_t)(mul32((int32_t)m->offs[1] - m->offs_to[1], frac) / 4096));
		v[2] = (int16_t)(v[2] + (int16_t)(mul32((int32_t)m->offs[2] - m->offs_to[2], frac) / 4096));
		Mat4x3 r;
		int16_t out[4];
		IdentityMatrix(&r);
		RotateY(&r, m->spin);
		MatrixMulVector(&r, v, out);
		memcpy(m->h.pos, ((EmitterNode *)e)->center, 8);
		m->h.pos[0] = (int16_t)(m->h.pos[0] + out[0]);
		m->h.pos[1] = (int16_t)(m->h.pos[1] + out[1]);
		m->h.pos[2] = (int16_t)(m->h.pos[2] + out[2]);
	}

	// sub_89B250 (phase 0): size, key shapes 0 / 1 and the pool's place around the target
	static void PoolInit(MeshNode *m)
	{
		EmitterNode *e = (EmitterNode *)m->h.emitter;
		int32_t s = e->scale;
		int32_t half = s / 2;
		m->scale[2] = half;
		m->scale[1] = half;
		m->scale[0] = half;
		switch ((uint32_t)(int32_t)m->variant)
		{
		case 0:
			m->rot[1] = 0x800;
			m->offs[0] = Div40960(mul32(s, -4240));
			m->offs[1] = Div40960(mul32(s, 2720));
			m->offs[2] = Div40960(mul32(s, -1200));
			break;
		case 1:
			m->rot[1] = 0x800;
			m->offs[0] = Div40960(mul32(s, -240));
			m->offs[1] = Div40960(mul32(s, 4400));
			m->offs[2] = Div40960(mul32(s, -80));
			break;
		case 2:
			m->rot[1] = 0xA00;
			m->offs[0] = Div40960(mul32(s, 1760));
			m->offs[1] = Div40960(mul32(s, -5200));
			m->offs[2] = Div40960(mul32(s, 80));
			break;
		case 3:
			m->rot[1] = 0x100;
			m->offs[0] = Div40960(mul32(s, -400));
			m->offs[1] = Div40960(mul32(s, 80));
			m->offs[2] = Div40960(mul32(s, -5200));
			break;
		case 4:
			m->offs[0] = Div40960(mul32(s, -1760));
			m->offs[1] = Div40960(mul32(s, -160));
			m->rot[1] = 0xC00;
			m->offs[2] = Div40960(mul32(s, 4400));
			break;
		case 5:
			m->offs[0] = Div40960(mul32(s, 5200));
			m->offs[1] = Div40960(mul32(s, 1440));
			m->rot[1] = 0xC00;
			m->offs[2] = Div40960(mul32(s, 640));
			break;
		default:
			break;
		}
		m->offs_to[0] = (int16_t)(m->offs[0] * 2);
		int32_t lift = mul32(s, 640) / 2 / 4096;
		m->key_b = 1;
		m->mesh = (uint8_t *)MESH_Pool;
		m->offs_to[1] = (int16_t)((int16_t)-e->center[1] - lift);
		m->spin = e->spin;
		m->key_a = 0;
		m->frame = 0;
		m->offs_to[2] = (int16_t)(m->offs[2] * 2);
		m->src = *(const uint8_t *const *)POOL_KEYS;
		m->dst = *(const uint8_t *const *)(POOL_KEYS + 4);
		m->h.phase++;
	}

	// sub_89B660 (phase 1): morph 0x800 per tick through key shapes 0..4; then the splat mesh
	// (keys 5 -> 6, puddle) and the collapse toward the target over the emitter's ticks left to 30
	static void PoolBubble(MeshNode *m)
	{
		m->t = (int16_t)(m->t + 0x800);
		int16_t t = m->t;
		if (t <= 0x1000) return;
		t = (int16_t)(t - 0x1000);
		m->key_a++;
		m->key_b++;
		m->t = t;
		if (m->key_a >= 4)
		{
			EmitterNode *e = (EmitterNode *)m->h.emitter;
			int16_t steps = (int16_t)(0x1E - e->h.counter);
			int16_t sd = (int16_t)((int16_t)m->scale[0] - e->scale);
			m->mesh = (uint8_t *)MESH_Splat;
			m->key_a = 5;
			m->steps = steps;
			uint16_t sp = (uint16_t)((uint16_t)(m->spin - e->spin) & 0xFFF);
			m->scale_d = sd;
			m->key_b = 6;
			m->spin_d = (int16_t)sp;
			if ((int16_t)sp >= 0x800) m->spin_d = (int16_t)(sp - 0x1000);
			m->lit = 1;
			m->h.phase++;
		}
		m->src = *(const uint8_t *const *)(POOL_KEYS + 4 * m->key_a);
		m->dst = *(const uint8_t *const *)(POOL_KEYS + 4 * m->key_b);
	}

	// spin and scale toward the emitter's (rem / steps of the difference left)
	static void PoolFollow(MeshNode *m, int32_t rem, int32_t steps)
	{
		EmitterNode *e = (EmitterNode *)m->h.emitter;
		m->spin = (int16_t)((int16_t)(mul32(m->spin_d, rem) / steps) + e->spin);
		int32_t sc = mul32(m->scale_d, rem) / steps + e->scale;
		m->scale[2] = sc;
		m->scale[1] = sc;
		m->scale[0] = sc;
	}

	// sub_89B740 (phase 2): splat morph 0x400 per tick while following the emitter
	static void PoolCollapse(MeshNode *m)
	{
		int32_t steps = m->steps;
		int16_t step = m->step;
		int32_t rem = steps - step;
		m->t = (int16_t)(m->t + 0x400);
		PoolFollow(m, rem, steps);
		int16_t ns = (int16_t)(step + 1);
		m->step = ns;
		m->frac = (int16_t)(shl32(ns, 12) / steps);
		if (m->t >= 0x1000)
		{
			m->t = 0x1000;
			m->h.phase++;
		}
	}

	// sub_89B7E0 (phase 3): follows the emitter until the last step, then done
	static void PoolSettle(MeshNode *m)
	{
		int16_t steps = m->steps;
		int32_t rem = (int32_t)steps - m->step;
		PoolFollow(m, rem, steps);
		int16_t ns = (int16_t)(m->step + 1);
		m->step = ns;
		m->frac = (int16_t)(shl32(ns, 12) / steps);
		if (ns >= steps)
		{
			m->h.flags |= 1;
			m->h.phase++;
		}
	}

	// sub_89B870 (phase 4)
	static void PoolWaitEnd(MeshNode *m)
	{
		if (m->h.emitter->counter >= 0x1E)
		{
			m->h.flags |= 1;
			m->h.phase++;
		}
	}

	static void PoolPhase(MeshNode *m)
	{
		switch (m->h.phase)
		{
		case 0: PoolInit(m); break;
		case 1: PoolBubble(m); break;
		case 2: PoolCollapse(m); break;
		case 3: PoolSettle(m); break;
		case 4: PoolWaitEnd(m); break;
		default: break; // nullsub_1743
		}
	}

	static uint32_t __cdecl PoolTask(TaskNode *n)
	{
		MeshNode *m = (MeshNode *)n;
		PoolPhase(m);
		PoolUpdatePos(m);
		// 30 fps layer: see mag022_bio_held.inc
		FX_HELD(held_note_draw(ORIG_Pool, m);)
		Morph(m->src, m->dst, m->mesh, m->t);
		if (m->lit == 0) DrawMesh(m);
		else DrawMeshPuddle(m);
		return Finish(&m->h);
	}

	// ------------------------------------------------------------------
	// Blob (0x89C270)
	// ------------------------------------------------------------------
	// the blob's morph and draw (flat on a tall target)
	static void BlobShow(MeshNode *m)
	{
		EmitterNode *e = (EmitterNode *)m->h.emitter;
		// 30 fps layer: see mag022_bio_held.inc
		FX_HELD(held_note_draw(ORIG_Blob, m);)
		Morph(m->src, m->dst, m->mesh, m->t);
		if (e->big == 1) DrawMeshPuddle(m);
		else DrawMesh(m);
	}

	// sub_89C750: droplets thrown from every 8th vertex (starting at vertex 2k) of the blob
	static void BlobBurst(MeshNode *m, int32_t k)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x30);
		memcpy(s + 8, m->h.pos, 8);
		Mat4x3 *mat = (Mat4x3 *)(s + 0x10);
		BuildRotationMatrixFromAngles(m->rot, mat);
		mat->t[0] = *(const int16_t *)(s + 8);
		mat->t[1] = *(const int16_t *)(s + 0xA);
		mat->t[2] = *(const int16_t *)(s + 0xC);
		Scale3DMatrix(mat, m->scale);
		GteSetRotMatrix(mat);
		GteSetTransVector(mat);
		const uint8_t *v = m->mesh + 8 + (int16_t)k * 16;
		int32_t n = *(const int32_t *)(m->mesh + 4) / 8 - 1;
		for (; n > 0; n--)
		{
			Particle *p = (Particle *)AddEffectTask(QParticles(), ORIG_Droplet, 0x70, m);
			GteLoadV0(v);
			GteMVMVA_RotV0Tr2();
			GteStoreIR123(p->h.pos);
			int16_t vx = (int16_t)((int16_t)(p->h.pos[0] - *(const int16_t *)(s + 8)) / 8);
			int16_t vy = (int16_t)((int16_t)(p->h.pos[1] - *(const int16_t *)(s + 0xA)) / 8);
			int16_t vz = (int16_t)((int16_t)(p->h.pos[2] - *(const int16_t *)(s + 0xC)) / 8);
			p->vel[0] = vx;
			p->vel[1] = vy;
			p->acc[0] = (int16_t)-(vx / 16);
			v += 0x40;
			p->acc[1] = (int16_t)-(vy / 16);
			p->vel[2] = vz;
			p->acc[2] = (int16_t)-(vz / 16);
		}
		FieldFree(0x30);
	}

	// sub_89C410 (phase 0): on the acid centre, first morph at 0x400
	static void BlobInit(MeshNode *m)
	{
		EmitterNode *e = (EmitterNode *)m->h.emitter;
		memcpy(m->h.pos, e->center, 8);
		m->rot[1] = e->spin;
		m->mesh = (uint8_t *)MESH_Blob;
		m->t = 0x400;
		m->src = (const uint8_t *)KEY_BlobA;
		m->dst = (const uint8_t *)KEY_BlobB;
		m->frame = 0;
		int32_t sc = e->scale;
		m->scale[2] = sc;
		m->scale[1] = sc;
		m->scale[0] = sc;
		BlobShow(m);
		m->h.phase++;
	}

	// sub_89C4C0 (phase 1): grows (0x400 per tick), then the boiling mesh (keys 4 -> 0)
	static void BlobGrow(MeshNode *m)
	{
		m->t = (int16_t)(m->t + 0x400);
		if (m->t >= 0x1000)
		{
			const uint8_t *a = *(const uint8_t *const *)(BLOB_KEYS + 0x10);
			const uint8_t *b = *(const uint8_t *const *)BLOB_KEYS;
			m->key_a = 4;
			m->key_b = 0;
			m->t = 0;
			m->mesh = (uint8_t *)MESH_BlobBoil;
			m->h.phase++;
			m->src = a;
			m->dst = b;
		}
		BlobShow(m);
	}

	// sub_89C550 (phase 2): boils between random key shapes 0..3; after 5 the blob bursts
	static void BlobBoil(MeshNode *m)
	{
		EmitterNode *e = (EmitterNode *)m->h.emitter;
		m->t = (int16_t)(m->t + 0x400);
		if (m->t >= 0x1000)
		{
			m->key_a = m->key_b;
			int32_t r = CrtRand();
			m->key_b = (int16_t)(m->key_b + (int16_t)(r % 3 + 1));
			while (m->key_b >= 4) m->key_b = (int16_t)(m->key_b - 4);
			m->src = *(const uint8_t *const *)(BLOB_KEYS + 4 * m->key_a);
			m->cycles++;
			m->dst = *(const uint8_t *const *)(BLOB_KEYS + 4 * m->key_b);
			m->t = 0;
		}
		BlobShow(m);
		if (m->cycles >= 5)
		{
			BlobBurst(m, 0);
			e->acid_done = 1;
			FlashNode *f = (FlashNode *)AddEffectTask(QFlash(), ORIG_Flash, 0x3C, m);
			f->script = (const uint32_t *)FLASH_Script;
			m->h.phase++;
		}
	}

	static void BlobPhase(MeshNode *m)
	{
		switch (m->h.phase)
		{
		case 0: BlobInit(m); break;
		case 1: BlobGrow(m); break;
		case 2: BlobBoil(m); break;
		case 3: BlobBurst(m, 1); m->h.phase++; break; // sub_89C8C0
		case 4: BlobBurst(m, 2); m->h.phase++; break; // sub_89C8E0
		case 5: m->h.flags |= 1; m->h.phase++; break; // sub_89C900
		default: break;                                // nullsub_1748
		}
	}

	static uint32_t __cdecl BlobTask(TaskNode *n)
	{
		MeshNode *m = (MeshNode *)n;
		BlobPhase(m);
		EmitterNode *e = (EmitterNode *)m->h.emitter;
		// scale wobble and spin of the next tick's draw
		int32_t r = CrtRand();
		m->wob[0] = (int16_t)(m->wob[0] + (r & 0x7F) + 0x80);
		r = CrtRand();
		m->wob[0] &= 0x7FF;
		int16_t w0 = m->wob[0];
		m->wob[1] = (int16_t)(m->wob[1] + (r & 0x3F) + 0x40);
		m->wob[1] &= 0x7FF;
		r = CrtRand();
		m->wob[2] = (int16_t)(((uint32_t)(uint16_t)((r & 0x3F) + (uint16_t)m->wob[2]) + 0x10) & 0x7FF);
		int32_t sc = e->scale;
		m->scale[2] = sc;
		m->scale[1] = sc;
		m->scale[0] = sc;
		int32_t sn = ComputeSin(w0);
		m->scale[0] += mul32(e->scale / 4, sn) / 4096;
		sn = ComputeSin(m->wob[1]);
		m->scale[1] += mul32(e->scale / 4, sn) / 4096;
		sn = ComputeSin(m->wob[2]);
		m->scale[2] += mul32(e->scale / 4, sn) / 4096;
		r = CrtRand();
		m->rot[0] = (int16_t)(m->rot[0] + (r & 0x1F) + 0x20);
		m->rot[0] &= 0xFFF;
		r = CrtRand();
		int32_t ry = (int32_t)(uint16_t)((r & 0x1F) + (uint16_t)m->rot[1]) + 0x20;
		m->h.counter++;
		m->rot[1] = (int16_t)(ry & 0xFFF);
		if ((m->h.flags & 1) && m->h.children == 0)
		{
			ReleaseLinkedTask(m);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Sprites (splash, bubble, droplet, mist)
	// ------------------------------------------------------------------
	// sub_89BD90: camera-facing flipbook `seq` frame `frame` at pos
	static void DrawSprite(Particle *p, int32_t size)
	{
		if (p->h.flags & 4) return;
		TransformCameraByShadowRotation(p->h.pos, size, p->shade);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = p->seq;
		*(int16_t *)(h + 4) = p->frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// sub_89BF80: the flipbook on the plane rotated by rot (y, z, x) and scaled
	static void DrawOrientedSprite(Particle *p)
	{
		if (p->h.flags & 4) return;
		Mat4x3 m = {};
		IdentityMatrix(&m);
		RotateY(&m, p->rot[1]);
		RotateZ(&m, p->rot[2]);
		RotateX(&m, p->rot[0]);
		Scale3DMatrix(&m, p->scale);
		m.t[0] = p->h.pos[0];
		m.t[1] = p->h.pos[1];
		m.t[2] = p->h.pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = p->seq;
		*(int16_t *)(h + 4) = p->frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// splash (0x89BD20) - sub_89BE00: flipbook A / B, random circling
	static void SplashInit(Particle *p)
	{
		if (p->variant == 0) p->seq = SEQ_SplashA;
		else p->seq = SEQ_SplashB;
		p->last = 0xF;
		p->vel[1] = -0x40;
		p->angle = (int16_t)(CrtRand() & 0xFFF);
		p->speed = (int16_t)((CrtRand() & 0x80) + 0x1C0);
		p->h.phase++;
	}

	// sub_89BE60: rises 0x40 per tick while circling (radius sin / cos >> 7)
	static void SplashMove(Particle *p)
	{
		p->angle = (int16_t)((uint16_t)(p->speed + p->angle) & 0xFFF);
		int32_t sn = ComputeSin(p->angle);
		p->vel[0] = (int16_t)(sn / 128);
		int32_t cs = ComputeCos(p->angle);
		p->h.pos[1] = (int16_t)(p->h.pos[1] + p->vel[1]);
		p->h.pos[0] = (int16_t)(p->h.pos[0] + p->vel[0]);
		int16_t vz = (int16_t)(cs / 128);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + vz);
		p->vel[2] = vz;
		if (AdvanceFrame(p))
		{
			p->h.flags |= 1;
			p->h.phase++;
		}
	}

	static uint32_t __cdecl SplashTask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: SplashInit(p); break;
		case 1: SplashMove(p); break;
		default: break; // nullsub_1745
		}
		// 30 fps layer: see mag022_bio_held.inc
		FX_HELD(held_note_draw(ORIG_Splash, p);)
		DrawSprite(p, (int16_t)p->scale[0]);
		return Finish(&p->h);
	}

	// bubble (0x89BF20) - sub_89C070: flipbook A / B lying flat
	static void BubbleInit(Particle *p)
	{
		p->rot[0] = 0x400;
		if (p->variant == 0)
		{
			p->seq = SEQ_BubbleA;
			p->last = 0xC;
		}
		else
		{
			p->seq = SEQ_BubbleB;
			p->last = 0xC;
		}
		p->h.phase++;
	}

	// sub_89C0B0: when the flipbook is over, a splash of the same kind and size
	static void BubblePop(Particle *p)
	{
		if (!AdvanceFrame(p)) return;
		Particle *q = (Particle *)AddEffectTask(QParticles(), ORIG_Splash, 0x70, p);
		q->variant = p->variant;
		p->h.flags |= 1;
		memcpy(q->scale, p->scale, 0x10);
		p->h.phase++;
	}

	static uint32_t __cdecl BubbleTask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: BubbleInit(p); break;
		case 1: BubblePop(p); break;
		default: break; // nullsub_1746
		}
		// 30 fps layer: see mag022_bio_held.inc
		FX_HELD(held_note_draw(ORIG_Bubble, p);)
		DrawOrientedSprite(p);
		return Finish(&p->h);
	}

	// droplet (0x89C930) - sub_89C9B0: size 1..8 eighths of the emitter's scale
	static void DropletInit(Particle *p)
	{
		EmitterNode *e = (EmitterNode *)p->h.emitter;
		int32_t r = CrtRand();
		p->seq = SEQ_Droplet;
		p->last = 0xC;
		int32_t sc = mul32((r & 7) + 1, e->scale) / 8;
		p->scale[2] = sc;
		p->scale[1] = sc;
		p->scale[0] = sc;
		p->h.phase++;
	}

	// sub_89CA00: velocity += acceleration (gravity 6 on y), position += velocity
	static void DropletFall(Particle *p)
	{
		int16_t ay = p->acc[1];
		p->vel[0] = (int16_t)(p->vel[0] + p->acc[0]);
		p->vel[2] = (int16_t)(p->vel[2] + p->acc[2]);
		p->h.pos[0] = (int16_t)(p->h.pos[0] + p->vel[0]);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + p->vel[2]);
		p->vel[1] = (int16_t)(p->vel[1] + (int16_t)(ay - 6));
		p->h.pos[1] = (int16_t)(p->h.pos[1] + p->vel[1]);
		if (AdvanceFrame(p))
		{
			p->h.flags |= 1;
			p->h.phase++;
		}
	}

	// droplet and mist: below the ground (y > 0) the sprite is hidden and done
	static void BelowGround(Particle *p)
	{
		if (p->h.pos[1] > 0) p->h.flags |= 5;
	}

	static uint32_t __cdecl DropletTask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: DropletInit(p); break;
		case 1: DropletFall(p); break;
		default: break; // nullsub_1749
		}
		BelowGround(p);
		// 30 fps layer: see mag022_bio_held.inc
		FX_HELD(held_note_draw(ORIG_Droplet, p);)
		DrawSprite(p, (int16_t)p->scale[0]);
		return Finish(&p->h);
	}

	// sub_89CF30: pos += a random point of the ball of radius r (random direction, random length)
	static void RandomOffsetInBall(Particle *p, int32_t r)
	{
		int32_t rr = r;
		if ((int16_t)rr == 0) rr = 1;
		int32_t a = CrtRand() & 0xFFF;
		int32_t b = CrtRand() & 0xFFF;
		Mat4x3 m = {};
		IdentityMatrix(&m);
		RotateY(&m, (int16_t)a);
		RotateX(&m, (int16_t)b);
		int16_t v[4] = {};
		int16_t out[4];
		int32_t d = CrtRand() % (int16_t)rr;
		v[2] = (int16_t)d;
		MatrixMulVector(&m, v, out);
		p->h.pos[0] = (int16_t)(p->h.pos[0] + out[0]);
		p->h.pos[1] = (int16_t)(p->h.pos[1] + out[1]);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + out[2]);
	}

	// sub_89D280: pos += a random point of the sphere of radius r
	static void RandomOffsetOnSphere(Particle *p, int32_t r)
	{
		int32_t rr = r;
		if ((int16_t)rr == 0) rr = 1;
		int32_t a = CrtRand() & 0xFFF;
		int32_t b = CrtRand() & 0xFFF;
		Mat4x3 m = {};
		IdentityMatrix(&m);
		RotateY(&m, (int16_t)a);
		RotateX(&m, (int16_t)b);
		int16_t v[4] = {};
		int16_t out[4];
		v[2] = (int16_t)rr;
		MatrixMulVector(&m, v, out);
		p->h.pos[0] = (int16_t)(p->h.pos[0] + out[0]);
		p->h.pos[1] = (int16_t)(p->h.pos[1] + out[1]);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + out[2]);
	}

	// mist inside (0x89CE60) - sub_89CEE0: somewhere in the ball of 3/16 of the scale
	static void MistAInit(Particle *p)
	{
		EmitterNode *e = (EmitterNode *)p->h.emitter;
		memcpy(p->h.pos, e->center, 8);
		RandomOffsetInBall(p, mul32(e->scale * 3, 256) / 4096);
		p->seq = SEQ_MistA;
		p->last = 0xF;
		p->h.phase++;
	}

	static uint32_t __cdecl MistATask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: MistAInit(p); break;
		case 1: // sub_89CFE0
			if (AdvanceFrame(p))
			{
				p->h.flags |= 1;
				p->h.phase++;
			}
			break;
		default: break; // nullsub_1751
		}
		BelowGround(p);
		// 30 fps layer: see mag022_bio_held.inc
		FX_HELD(held_note_draw(ORIG_MistA, p);)
		DrawSprite(p, (int16_t)p->scale[0]);
		return Finish(&p->h);
	}

	// mist flying off (0x89D120) - sub_89D1A0: on the sphere of 5/16 of the scale, flying away
	// from the centre (1/12..1/19 of the distance per tick), decelerating by -1/12 of that
	static void MistBInit(Particle *p)
	{
		EmitterNode *e = (EmitterNode *)p->h.emitter;
		memcpy(p->h.pos, e->center, 8);
		RandomOffsetOnSphere(p, mul32(e->scale * 5, 256) / 4096);
		p->seq = SEQ_MistB;
		int32_t r = CrtRand();
		p->last = 0xB;
		int32_t d = (int16_t)((r & 7) + 0xC);
		int32_t vx = ((int32_t)p->h.pos[0] - e->center[0]) / d;
		int32_t vy = ((int32_t)p->h.pos[1] - e->center[1]) / d;
		p->vel[0] = (int16_t)vx;
		int32_t vz = ((int32_t)p->h.pos[2] - e->center[2]) / d;
		p->vel[1] = (int16_t)vy;
		p->vel[2] = (int16_t)vz;
		p->acc[0] = DivNeg12((int16_t)vx);
		p->acc[1] = DivNeg12((int16_t)vy);
		p->acc[2] = DivNeg12((int16_t)vz);
		p->h.phase++;
	}

	// sub_89D330: velocity += acceleration, position += velocity
	static void MistBMove(Particle *p)
	{
		p->vel[0] = (int16_t)(p->vel[0] + p->acc[0]);
		p->vel[1] = (int16_t)(p->vel[1] + p->acc[1]);
		p->vel[2] = (int16_t)(p->vel[2] + p->acc[2]);
		p->h.pos[0] = (int16_t)(p->h.pos[0] + p->vel[0]);
		p->h.pos[1] = (int16_t)(p->h.pos[1] + p->vel[1]);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + p->vel[2]);
		if (AdvanceFrame(p))
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
		default: break; // nullsub_1753
		}
		BelowGround(p);
		// 30 fps layer: see mag022_bio_held.inc
		FX_HELD(held_note_draw(ORIG_MistB, p);)
		DrawSprite(p, (int16_t)p->scale[0]);
		return Finish(&p->h);
	}

	// ------------------------------------------------------------------
	// Spawners
	// ------------------------------------------------------------------
	// bubble spawner (0x89BA80) - sub_89BAE0: on the acid centre
	static void SpawnerAtCenter(FxNode *n)
	{
		memcpy(n->pos, ((EmitterNode *)n->emitter)->center, 8);
		n->phase++;
	}

	// sub_89BB00: ticks 0..20, table counts of bubbles on a ring (radius (0x380..0x4FF) * scale)
	// and of splashes on the same kind of ring at a random height of the target
	static void BubbleSpawn(FxNode *n)
	{
		if (n->counter > 0x14)
		{
			n->flags |= 1;
			n->phase++;
			return;
		}
		EmitterNode *e = (EmitterNode *)n->emitter;
		int32_t count = *(const uint8_t *)(COUNT_Bubbles + n->counter);
		for (int32_t k = count; k > 0; k--)
		{
			int32_t a = CrtRand() & 0xFFF;
			int32_t r = CrtRand() % 0x180;
			int32_t rad = mul32(r + 0x380, e->scale) / 4096;
			Particle *p = (Particle *)AddEffectTask(QParticles(), ORIG_Bubble, 0x70, n);
			p->variant = (int16_t)(CrtRand() & 1);
			int32_t r16 = (int16_t)rad;
			int32_t sn = ComputeSin((int16_t)a);
			p->h.pos[0] = (int16_t)(p->h.pos[0] + (int16_t)(mul32(sn, r16) / 4096));
			int32_t cs = ComputeCos((int16_t)a);
			p->h.pos[1] = 0;
			p->h.pos[2] = (int16_t)(p->h.pos[2] + (int16_t)(mul32(cs, r16) / 4096));
			int32_t sc = e->scale;
			p->scale[2] = sc;
			p->scale[1] = sc;
			p->scale[0] = sc;
		}
		count = *(const uint8_t *)(COUNT_Splashes + n->counter);
		for (int32_t k = count; k > 0; k--)
		{
			int32_t a = CrtRand() & 0xFFF;
			int32_t r = CrtRand() % 0x180;
			int32_t rad = mul32(r + 0x380, e->scale) / 4096;
			Particle *p = (Particle *)AddEffectTask(QParticles(), ORIG_Splash, 0x70, n);
			p->variant = (int16_t)(CrtRand() & 1);
			int32_t r16 = (int16_t)rad;
			int32_t sn = ComputeSin((int16_t)a);
			p->h.pos[0] = (int16_t)(p->h.pos[0] + (int16_t)(mul32(sn, r16) / 4096));
			int32_t cs = ComputeCos((int16_t)a);
			p->h.pos[2] = (int16_t)(p->h.pos[2] + (int16_t)(mul32(cs, r16) / 4096));
			int32_t base = (int16_t)BoundsBaseY(n);
			int32_t ry = CrtRand() & 0xFFF;
			p->h.pos[1] = (int16_t)(mul32(ry, base) / 4096);
			int32_t sc = e->scale;
			int32_t rs = CrtRand() & 0x7FF;
			int32_t v = sc - mul32(rs, sc) / 4096;
			p->scale[2] = v;
			p->scale[1] = v;
			p->scale[0] = v;
		}
	}

	static uint32_t __cdecl BubbleSpawnerTask(TaskNode *n)
	{
		FxNode *s = (FxNode *)n;
		switch (s->phase)
		{
		case 0: SpawnerAtCenter(s); break;
		case 1: BubbleSpawn(s); break;
		default: break; // nullsub_1747
		}
		return Finish(s);
	}

	// mist spawners: table counts per tick, size = the emitter's scale or half of it
	static void MistSpawn(FxNode *n, int16_t last_tick, uint32_t counts, uint32_t task)
	{
		if (n->counter > last_tick)
		{
			n->flags |= 1;
			n->phase++;
			return;
		}
		EmitterNode *e = (EmitterNode *)n->emitter;
		int32_t count = *(const uint8_t *)(counts + n->counter);
		for (int32_t k = count; k > 0; k--)
		{
			Particle *p = (Particle *)AddEffectTask(QParticles(), task, 0x70, n);
			int32_t r = CrtRand();
			int32_t sc = e->scale;
			if (!(r & 1)) sc /= 2;
			p->scale[2] = sc;
			p->scale[1] = sc;
			p->scale[0] = sc;
		}
	}

	// 0x89CD60: sub_89CDC0, then sub_89CDE0 (ticks 0..24)
	static uint32_t __cdecl MistSpawnerATask(TaskNode *n)
	{
		FxNode *s = (FxNode *)n;
		switch (s->phase)
		{
		case 0: SpawnerAtCenter(s); break;
		case 1: MistSpawn(s, 0x18, COUNT_MistA, ORIG_MistA); break;
		default: break; // nullsub_1750
		}
		return Finish(s);
	}

	// 0x89D020: sub_89D080, then sub_89D0A0 (ticks 0..20)
	static uint32_t __cdecl MistSpawnerBTask(TaskNode *n)
	{
		FxNode *s = (FxNode *)n;
		switch (s->phase)
		{
		case 0: SpawnerAtCenter(s); break;
		case 1: MistSpawn(s, 0x14, COUNT_MistB, ORIG_MistB); break;
		default: break; // nullsub_1754
		}
		return Finish(s);
	}

	// ------------------------------------------------------------------
	// Stage wobble (0x89B8A0): h.pos[0] = amplitude, h.pos[1] = hold ticks
	// ------------------------------------------------------------------
	static void SetStageWobble(int16_t amp)
	{
		for (int k = 0; k < 4; k++) *(int16_t *)(STAGE_WOBBLE + 2 + 0x2C * k) = amp;
	}

	static uint32_t __cdecl WobbleTask(TaskNode *n)
	{
		WobbleNode *w = (WobbleNode *)n;
		switch (w->h.phase)
		{
		case 0: // sub_89B910: amplitude and the wobble bytes of the 4 stage groups to 0
			w->h.pos[0] = 0;
			for (int k = 0; k < 4; k++)
			{
				*(int16_t *)(STAGE_WOBBLE + 2 + 0x2C * k) = 0;
				*(uint8_t *)(STAGE_WOBBLE + 0x2A + 0x2C * k) = 0;
				*(uint8_t *)(STAGE_WOBBLE + 0x29 + 0x2C * k) = 0;
				*(uint8_t *)(STAGE_WOBBLE + 0x28 + 0x2C * k) = 0;
			}
			w->h.phase++;
			break;
		case 1: // sub_89B950: up 0x80 per tick to 0x400
			w->h.pos[0] = (int16_t)(w->h.pos[0] + 0x80);
			if (w->h.pos[0] >= 0x400)
			{
				w->h.phase++;
				w->h.pos[0] = 0x400;
			}
			SetStageWobble(w->h.pos[0]);
			break;
		case 2: // sub_89B990: until the blob has burst
			if (((EmitterNode *)w->h.emitter)->acid_done != 0)
			{
				w->h.pos[1] = 0;
				w->h.phase++;
			}
			break;
		case 3: // sub_89B9B0: 4 more ticks
			w->h.pos[1]++;
			if (w->h.pos[1] >= 4) w->h.phase++;
			break;
		case 4: // sub_89B9D0: down 0x80 per tick to 0, then done
			w->h.pos[0] = (int16_t)(w->h.pos[0] - 0x80);
			if (w->h.pos[0] <= 0)
			{
				w->h.flags |= 1;
				w->h.phase++;
				w->h.pos[0] = 0;
			}
			SetStageWobble(w->h.pos[0]);
			break;
		default: break; // nullsub_1744
		}
		return Finish(&w->h);
	}

	// ------------------------------------------------------------------
	// Palette cycler (0x89CA70)
	// ------------------------------------------------------------------
	// sub_89CB90: n 15-bit colours of `out` = a * (1 - t) + b * t (TIMs, CLUT at +0x14), STP set
	static void LerpClut(uint32_t a, uint32_t b, uint32_t out, int32_t t, int32_t n)
	{
		const uint8_t *pa = (const uint8_t *)a + 0x14;
		const uint8_t *pb = (const uint8_t *)b + 0x14;
		uint8_t *po = (uint8_t *)out + 0x14;
		uint8_t *s = (uint8_t *)FieldAlloc(0x18);
		int32_t t16 = (int16_t)t;
		*(int32_t *)(s + 4) = t16;
		int32_t count = (int16_t)n;
		*(int32_t *)s = 0x1000 - t16;
		intptr_t delta = pb - pa;
		for (int32_t k = count; k > 0; k--)
		{
			uint16_t ca = (uint16_t)(*(const uint16_t *)pa & 0x7FFF);
			*(uint16_t *)(s + 0x14) = ca;
			s[8] = (uint8_t)(ca & 0x1F);
			s[9] = (uint8_t)((ca >> 5) & 0x1F);
			s[10] = (uint8_t)((ca >> 10) & 0x1F);
			uint16_t cb = (uint16_t)(*(const uint16_t *)(pa + delta) & 0x7FFF);
			*(uint16_t *)(s + 0x16) = cb;
			s[0xC] = (uint8_t)(cb & 0x1F);
			s[0xD] = (uint8_t)((cb >> 5) & 0x1F);
			s[0xE] = (uint8_t)((cb >> 10) & 0x1F);
			GteSetIR0(*(const int32_t *)s);
			GteLoadIRBytes(s + 8);
			GteGPF();
			pa += 2;
			GteSetIR0(*(const int32_t *)(s + 4));
			GteLoadIRBytes(s + 0xC);
			GteGPL();
			GteStoreIRBytes(s + 0x10);
			uint32_t c = ((uint32_t)(s[0x12] & 0x1F) << 10) + ((uint32_t)(s[0x11] & 0x1F) << 5) + (uint32_t)(s[0x10] & 0x1F);
			*(uint16_t *)po = (uint16_t)(c | 0x8000);
			po += 2;
		}
		FieldFree(0x18);
	}

	static uint32_t __cdecl PaletteTask(TaskNode *n)
	{
		WobbleNode *c = (WobbleNode *)n;
		switch (c->h.phase)
		{
		case 0: // sub_89CAD0
			c->from = 0;
			c->h.phase++;
			c->to = 1;
			break;
		case 1: // sub_89CAF0: blends to a random other palette, 2 ticks each; ends at the burst
		{
			c->t = (int16_t)(c->t + 0x800);
			EmitterNode *e = (EmitterNode *)c->h.emitter;
			if (c->t >= 0x1000)
			{
				c->t = 0;
				c->from = c->to;
				int32_t r = CrtRand();
				c->to = (int16_t)(c->to + (int16_t)((r & 1) + 1));
				while (c->to > 2) c->to = (int16_t)(c->to - 3);
			}
			LerpClut(*(const uint32_t *)(PALETTES + 4 * c->from), *(const uint32_t *)(PALETTES + 4 * c->to), PALETTE_TIM, c->t, 0x80);
			QueueTIMUpload(PALETTE_TIM);
			if (e->acid_done != 0)
			{
				c->h.flags |= 1;
				c->h.phase++;
			}
			break;
		}
		default: break; // nullsub_1750 (0x89CD10)
		}
		return Finish(&c->h);
	}

	// ------------------------------------------------------------------
	// Flash (0x89C650): full-screen additive quad
	// ------------------------------------------------------------------
	static void FlashDraw(const FlashNode *f)
	{
		uint32_t bucket = RenderOT(0x1C);
		uint8_t *p = (uint8_t *)PacketCursor();
		*(uint32_t *)(p + 4) = f->colour;
		*(uint32_t *)p = 0x5000000;
		p[7] = 0x2A;
		*(uint32_t *)(p + 8) = 0;
		*(uint32_t *)(p + 0xC) = 0x140;
		*(uint32_t *)(p + 0x10) = 0xD80000;
		*(uint32_t *)(p + 0x14) = 0xD80140;
		InsertPrimAltViewport(bucket, p);
		uint8_t *q = p + 0x18;
		uint32_t tpage = GetTPage(0, 1, 0x280, 0) & 0xFFFF;
		SetDrawMode(q, 0, 0, tpage, nullptr);
		InsertPrimAutoDepth(bucket, q);
		PacketCursor() = (uint32_t)(q + 0xC);
	}

	static uint32_t __cdecl FlashTask(TaskNode *n)
	{
		FlashNode *f = (FlashNode *)n;
		int16_t idx = f->idx;
		uint32_t w = f->script[idx];
		uint8_t op = (uint8_t)(w >> 24);
		if (op == 0xFF) f->h.flags |= 1;
		else
		{
			if (op == 0xFE)
			{
				if (f->h.counter >= (int16_t)(uint8_t)(w >> 16))
				{
					idx++;
					f->idx = idx;
					f->colour = f->script[idx];
				}
			}
			else
			{
				idx++;
				f->colour = w;
				f->idx = idx;
			}
			// 30 fps layer: see mag022_bio_held.inc
			FX_HELD(held_note_draw(ORIG_Flash, f);)
			FlashDraw(f);
		}
		return Finish(&f->h);
	}

	// ------------------------------------------------------------------
	// Target overlay (0x89D3D0)
	// ------------------------------------------------------------------
	// sub_89DC30
	static void InitOverlay(OverlayCtx *mc, uint32_t a00, uint8_t *sprite, uint32_t verts, uint8_t *entity, int32_t slot)
	{
		mc->entity = entity;
		mc->slot = (int16_t)slot;
		mc->a00 = a00;
		mc->sprite = sprite;
		mc->frame_cursor = (uint32_t *)0x1D8E054;
		mc->packets = (uint32_t *)0x270456C;
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

	// sub_89D550: the entity's matrix, and a copy moved by `offset`
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

	// sub_89D610: `light` = the entity's rotation seen from light_pos (at light_dist)
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

	// sub_89D780: one model object (bone groups of vertices, then textured triangles and quads)
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

	// sub_89D460: shadow, body (+0x64) and weapon (+0x78) of the target with the wobble
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

	// sub_89DBD0 (phase 0): hide the target, the overlay draws it
	static void OverlayInit(FxNode *o)
	{
		uint8_t slot = o->slot;
		uint8_t *entity = Entity(slot);
		InitOverlay(&Overlay(), OVERLAY_A00, (uint8_t *)OVERLAY_SPRITE, OVERLAY_VERTS, entity, slot);
		o->flags |= 8;
		entity[0] |= 4;
		o->phase++;
	}

	// glow + 0x40 per tick, at most 0x300 (sub_89DD20 / sub_89DD70)
	static void OverlayGlow(OverlayCtx &mc)
	{
		mc.glow = (int16_t)(mc.glow + 0x40);
		if (mc.glow > 0x300) mc.glow = 0x300;
	}

	static uint32_t __cdecl OverlayTask(TaskNode *n)
	{
		FxNode *o = (FxNode *)n;
		OverlayCtx &mc = Overlay();
		switch (o->phase)
		{
		case 0: OverlayInit(o); break;
		case 1: // sub_89DD20: amplitude + 0x200 per tick up to 0x1000
			OverlayGlow(mc);
			mc.wobble_amp = (int16_t)(mc.wobble_amp + 0x200);
			if (mc.wobble_amp >= 0x1000)
			{
				mc.wobble_amp = 0x1000;
				o->phase++;
			}
			break;
		case 2: // sub_89DD70: until tick 26
			OverlayGlow(mc);
			if (o->counter >= 0x1A) o->phase++;
			break;
		case 3: // sub_89DDA0: amplitude - 0x100 per tick; at 0 the target is shown again
		{
			mc.glow = (int16_t)(mc.glow - 0x40);
			if (mc.glow < 0) mc.glow = 0;
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
		default: break; // nullsub_1755
		}
		mc.wobble_phase = (int16_t)((uint16_t)(mc.wobble_phase + 0xA0) & 0xFFF);
		if (o->flags & 8)
		{
			// 30 fps layer: see mag022_bio_held.inc
			FX_HELD(held_note_draw(ORIG_Overlay, o);)
			DrawOverlay(&mc);
		}
		return Finish(o);
	}

	// ------------------------------------------------------------------
	// Emitter (0x89A8D0)
	// ------------------------------------------------------------------
	// sub_89A9D0 (phase 0): acid centre / size / scale from the target's bounds, stage wobble, pool 0
	static void EmitterInit(EmitterNode *e)
	{
		CopyMidpointFromSource(e, e->center);
		e->center[1] = (int16_t)BoundsCenterY(e);
		int16_t hh = (int16_t)BoundsHalfHeight(e);
		e->radius = hh;
		if (hh > 0xA00) e->radius = 0xA00;
		int32_t rad = e->radius;
		int32_t hi = (int32_t)(((int64_t)shl32(rad, 12) * 0x4BDA12F7) >> 32) >> 7;
		hi += (int32_t)((uint32_t)hi >> 31); // rad * 4096 / 432
		int16_t s = (int16_t)hi;
		e->scale = s;
		if (s < 0x1000) e->scale = 0x1000;
		else if (s > 0x4000) e->scale = 0x4000;
		e->scale = (int16_t)(shl32(e->scale * 11, 8) / 4096);
		int32_t cy = e->center[1];
		if (cy + rad > -0x200)
		{
			e->big = 1;
			e->center[1] = (int16_t)(shl32(cy * 3, 10) / 4096);
		}
		AddEffectTask(QControl(), ORIG_Wobble, 0x38, e);
		MeshNode *m = (MeshNode *)AddEffectTask(QMeshes(), ORIG_Pool, 0x98, e);
		m->variant = 0;
		e->h.phase++;
	}

	// a pool of `variant` once the emitter's counter reaches `tick`
	static void EmitterPool(EmitterNode *e, int16_t tick, int16_t variant)
	{
		if (e->h.counter < tick) return;
		MeshNode *m = (MeshNode *)AddEffectTask(QMeshes(), ORIG_Pool, 0x98, e);
		m->variant = variant;
		e->h.phase++;
	}

	static void EmitterPhase(EmitterNode *e)
	{
		switch (e->h.phase)
		{
		case 0: EmitterInit(e); break;
		case 1: // sub_89BA20: bubble spawner and pool 1
			if (e->h.counter >= 2)
			{
				AddEffectTask(QControl(), ORIG_BubbleSpawner, 0x38, e);
				EmitterPool(e, 2, 1);
			}
			break;
		case 2: EmitterPool(e, 4, 2); break;  // sub_89C130
		case 3: EmitterPool(e, 6, 3); break;  // sub_89C170
		case 4: EmitterPool(e, 8, 4); break;  // sub_89C1B0
		case 5: EmitterPool(e, 10, 5); break; // sub_89C1F0
		case 6: // sub_89C230: palette cycler and blob
			if (e->h.counter >= 0x1E)
			{
				AddEffectTask(QControl(), ORIG_Palette, 0x38, e);
				AddEffectTask(QMeshes(), ORIG_Blob, 0x98, e);
				e->h.phase++;
			}
			break;
		case 7: // sub_89CD20: mist spawners
			if (e->h.counter >= 0x20)
			{
				AddEffectTask(QControl(), ORIG_MistSpawnerA, 0x38, e);
				AddEffectTask(QControl(), ORIG_MistSpawnerB, 0x38, e);
				e->h.phase++;
			}
			break;
		case 8: // sub_89D3A0: target overlay
			if (e->h.counter >= 0x22)
			{
				AddEffectTask(QControl(), ORIG_Overlay, 0x38, e);
				e->h.phase++;
			}
			break;
		case 9: // sub_89DE20: damage once the blob has burst
			if (e->acid_done == 1)
			{
				ApplyActionResultToTarget(e->h.ctx->actions[e->h.action].targets);
				e->h.flags |= 1;
				e->h.phase++;
			}
			break;
		default: break; // nullsub_1756
		}
	}

	static uint32_t __cdecl EmitterTask(TaskNode *n)
	{
		EmitterNode *e = (EmitterNode *)n;
		EmitterUpdatePos(e);
		ComputeModelBounds(e);
		EmitterPhase(e);
		e->spin = (int16_t)((uint16_t)(e->spin - 0x40) & 0xFFF);
		IdentityMatrix(&e->spin_m);
		RotateY(&e->spin_m, e->spin);
		if (e->h.counter == 0) BdPlaySE(SOUND_Bio, 0, 0x80);
		return Finish(&e->h);
	}

	// ------------------------------------------------------------------
	// Root (0x89A7B0)
	// ------------------------------------------------------------------
	static void RootPhase(RootNode *r)
	{
		switch (r->h.phase)
		{
		case 0: r->h.phase++; break; // sub_89A890
		case 1: // sub_89A8A0: emitter of the current action
			r->busy = 1;
			AddEffectTask(QEmitters(), ORIG_Emitter, 0x8C, r);
			r->h.phase++;
			break;
		case 2: // sub_89DE70: when its overlay is over, the next action (back to phase 1)
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
		case 3: // sub_89DEA0: until every queue is empty
			if (r->alive == 0)
			{
				r->h.flags |= 1;
				r->h.phase++;
			}
			break;
		default: break; // nullsub_1757
		}
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag022_bio_held.inc
		FX_HELD(held_note_root();)
		if (r->tick & 1) PacketCursor() = ArenaA();
		else PacketCursor() = ArenaB();
		UpdateTargetPosFromBones(r);
		RootPhase(r);
		r->alive = (int16_t)ExecuteTaskQueue(QEmitters());
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QControl()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QParticles()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QMeshes()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QFlash()));
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

	void register_mag022_bio()
	{
		register_port(bio022::ORIG_Root, (void *)bio022::RootTask, "B022 RootTask", 22);
		register_port(bio022::ORIG_Emitter, (void *)bio022::EmitterTask, "B022 EmitterTask", 22);
		register_port(bio022::ORIG_Pool, (void *)bio022::PoolTask, "B022 PoolTask", 22);
		register_port(bio022::ORIG_Wobble, (void *)bio022::WobbleTask, "B022 WobbleTask", 22);
		register_port(bio022::ORIG_BubbleSpawner, (void *)bio022::BubbleSpawnerTask, "B022 BubbleSpawnerTask", 22);
		register_port(bio022::ORIG_Splash, (void *)bio022::SplashTask, "B022 SplashTask", 22);
		register_port(bio022::ORIG_Bubble, (void *)bio022::BubbleTask, "B022 BubbleTask", 22);
		register_port(bio022::ORIG_Blob, (void *)bio022::BlobTask, "B022 BlobTask", 22);
		register_port(bio022::ORIG_Flash, (void *)bio022::FlashTask, "B022 FlashTask", 22);
		register_port(bio022::ORIG_Droplet, (void *)bio022::DropletTask, "B022 DropletTask", 22);
		register_port(bio022::ORIG_Palette, (void *)bio022::PaletteTask, "B022 PaletteTask", 22);
		register_port(bio022::ORIG_MistSpawnerA, (void *)bio022::MistSpawnerATask, "B022 MistSpawnerATask", 22);
		register_port(bio022::ORIG_MistA, (void *)bio022::MistATask, "B022 MistATask", 22);
		register_port(bio022::ORIG_MistSpawnerB, (void *)bio022::MistSpawnerBTask, "B022 MistSpawnerBTask", 22);
		register_port(bio022::ORIG_MistB, (void *)bio022::MistBTask, "B022 MistBTask", 22);
		register_port(bio022::ORIG_Overlay, (void *)bio022::OverlayTask, "B022 OverlayTask", 22);
		// 30 fps layer: see mag022_bio_held.inc
		FX_HELD(register_mag022_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag022_bio_held.inc"
#endif
