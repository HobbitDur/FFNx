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

// Effect 1: Cure (spell, MAG_001_CURE_*).
//
// Structure (module code 0x8D69E0..0x8D8510: file loader MAG_001_CURE_FL 0x8D69E0 = the texture
// file named at 0x164252C; setup MAG_001_CURE_Init 0x8D6A00 sets up the queues, the root node and
// the packet arenas, starts camera animation 0x16417C8 and queues the TIM). Every node starts with
// the shared effect-library header (Effect_AddTaskAndInitFromCtx 0x8DC540: context, root, emitter,
// parent, position, counter, flags, child count, phase, action / target, caster / target slot);
// every task runs its current phase function (node +0x29) from a small table, and ends when it is
// flagged done (+0x26 bit0) and has no live children.
//   Root (0x8D6B30) - copies the camera into the shared snapshot 0x2793E58, alternates the packet
//     arena (texture file + 0 / + 0x10000), follows the caster's bones; phases: 0..2 wait for the
//     children, 3 spawns the emitter of the current action (marks the root busy), 4 waits while
//     busy, then moves to the next action (back to 3) until the last one, 6 waits until the three
//     queues (emitters, particles, glints) are empty, 9 done. Runs the three queues every tick.
//   Emitter (0x8D6CC0) - on the action's target: target anchor and model bounds every tick
//     (Effect library 0x8DC610 / 0x8DC870); spawns the ring at its first tick, sound at 0, heal
//     (ApplyActionResultToTarget 0x506690) from tick 15, then done.
//   Ring (0x8D6D80) - a light rising in a helix around the target: its position is recomputed
//     every tick from the counter in 4 sub-steps (step = 4 * counter + s: centre + step * climb in y,
//     radius * (sin, cos)(step * 0x80) in x / z); while spawning (flag 2) a sparkle per sub-step
//     (counter 1..16), 1..3 motes and 2..4 glints per tick (counter ranges); while flag 8 it redraws
//     the target model with a projected light texture (the overlay, below: the target is hidden,
//     entity flag 4); draws its flipbook and, for 20 ticks, an additive ground glow fading out.
//     Phase 1 switches its flipbook at 20, phase 2 runs the flipbook to its end, then unhides the
//     target, clears the root's busy flag and ends.
//   Mote (0x8D7F60) / Sparkle (0x8D80B0) - flipbooks moving at a random constant velocity (motes
//     rise, sparkles fly out and up) until their last frame.
//   Glints - the shared effect-library task Effect_Glint_Tick 0x8DCC20 (ported in fx_glint.cpp).
// Overlay (0x8D7110, context 0x278C7E8): the target's shadow, then every object of its body (+0x64)
// and weapon (+0x78) models: vertices through the bones into the overlay vertex buffer 0x2783E28,
// each front-facing polygon drawn twice - flat in the entity's colour with its own texture (frame
// arena, battle_texture_data_ptr 0x1D8E054), and, when it faces the projector and falls inside the
// projected window, gouraud with the Cure texture page mapped by a projection from the ring's
// position (module arena).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x277AEC0..0x2792E78 (texture file 0x277AEC0, packet cursor 0x277AEC4, arena
// limit 0x277AEEC, camera snapshot pointers 0x277AEF0 / 0x278C7DC, root pool 0x277AEF8 / queue
// 0x277AFC0, particle queue 0x277BF70 (pool 300 x 0x6C), arena pointers 0x2783E18 / 0x2783E1C /
// 0x278C798 / 0x278C79C / 0x278C7E4, overlay vertex buffer 0x2783E28, emitter queue 0x278A228
// (pool 0x278C628, 4 x 0x58), glint pool 0x278A238 (100 x 0x5C) / queue 0x278C788, overlay sprite
// block 0x278C7A0, overlay context 0x278C7E8), the effect library's glint cursor address 0x2792E74
// and camera snapshot 0x2793E58.

#include "mag_common.h"

namespace ff8fx
{
namespace cure001
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
		uint16_t flags;        // +0x26 bit0 done, bit1 spawning, bit2 hidden, bit3 overlay drawing
		uint8_t children;      // +0x28
		int8_t phase;          // +0x29
		int8_t action;         // +0x2A
		int8_t target;         // +0x2B
		uint8_t attacker;      // +0x2C
		uint8_t slot;          // +0x2D target entity slot
		uint8_t line;          // +0x2E incremented with the action (copied to the children)
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
		uint8_t busy;          // +0x63 a ring is running
	};
	struct EmitterNode // pool of 4 nodes of 0x58 bytes
	{
		FxNode h;
		uint8_t anchor[0x18];  // +0x30 target bone anchor (MAG_001_CURE_Emitter_UpdatePos)
		int16_t bmin[4];       // +0x48 model bounds (MAG_001_CURE_Emitter_ComputeModelBounds)
		int16_t bmax[4];       // +0x50
	};
	struct Particle // ring / mote / sparkle: pool of 300 nodes of 0x6C bytes
	{
		FxNode h;
		uint8_t pad30[0x12];
		int16_t angle;         // +0x42 ring: angle of the last sub-step
		uint8_t pad44[8];
		uint32_t seq;          // +0x4C flipbook
		int16_t frame;         // +0x50
		int16_t last;          // +0x52 last frame
		int16_t shade;         // +0x54 TransformCameraByShadowRotation offset (0)
		int16_t pad56;
		int16_t vel[3];        // +0x58 mote / sparkle velocity; ring: vel[1] = climb per sub-step
		int16_t pad5E;
		int16_t center[4];     // +0x60 ring: centre (x, top of the target's bounds, z)
		int16_t radius;        // +0x68 ring radius
		int16_t pad6A;
	};
	// overlay context 0x278C7E8 (set up by 0x8D8280; the render also reads the word at +0xF4)
	struct OverlayCtx
	{
		uint32_t a00;           // +0x00 0x277AEC8 (not read by the draw)
		uint8_t *sprite;        // +0x04 0x278C7A0: +0x04 = vertex buffer 0x2783E28
		uint8_t *entity;        // +0x08 target entity
		uint32_t *frame_cursor; // +0x0C &battle_texture_data_ptr (0x1D8E054)
		uint32_t *packets;      // +0x10 &module packet cursor 0x277AEC4
		Mat4x3 world;           // +0x14 entity matrix
		Mat4x3 view;            // +0x34 camera * world
		Mat4x3 world2;          // +0x54 entity matrix moved by `offset`
		Mat4x3 light;           // +0x74 projector matrix (the entity seen from light_pos)
		uint8_t pad94[0x10];
		int16_t a4[3];          // +0xA4
		int16_t padAA;
		int16_t ac[3];          // +0xAC
		int16_t padB2;
		int16_t offset[3];      // +0xB4 (y = the ring's height)
		int16_t padBA;
		int16_t bc[3];          // +0xBC
		int16_t padC2;
		uint8_t rgb[3];         // +0xC4 projected colour
		uint8_t padC7;
		int32_t light_dist;     // +0xC8
		int16_t cc;             // +0xCC
		int16_t ce;             // +0xCE
		int16_t padD0;
		int16_t mode;           // +0xD2 0: offset in model space, 1: in world space
		uint8_t padD4[4];
		int16_t anim;           // +0xD8
		int16_t slot;           // +0xDA 0xFF: the overlay advances the animation itself
		int16_t light_pos[3];   // +0xDC projector position (the ring)
		int16_t padE2;
		int16_t proj_w;         // +0xE4 projected window size
		int16_t proj_h;         // +0xE6
		int16_t u_off;          // +0xE8 texture offset
		int16_t v_off;          // +0xEA
		uint16_t tpage;         // +0xEC
		uint16_t clut;          // +0xEE
		int16_t padF0;
		int16_t glow;           // +0xF2
		int16_t cull;           // +0xF4 (0x278C8DC, never written by Cure) 1: projector back faces skipped
		int16_t padF6;
	};
#pragma pack(pop)
	static_assert(sizeof(FxNode) == 0x30 && sizeof(RootNode) == 0x64 && sizeof(EmitterNode) == 0x58, "Cure nodes");
	static_assert(sizeof(Particle) == 0x6C && sizeof(OverlayCtx) == 0xF8, "Cure nodes");

	// ------------------------------------------------------------------
	// Module globals
	// ------------------------------------------------------------------
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x277AEC4); }
	inline uint32_t &PacketLimit() { return var<uint32_t>(0x277AEEC); }
	inline TaskQueue *QEmitters() { return (TaskQueue *)0x278A228; }  // 4 x 0x58
	inline TaskQueue *QParticles() { return (TaskQueue *)0x277BF70; } // 300 x 0x6C
	inline TaskQueue *QGlints() { return (TaskQueue *)0x278C788; }    // 100 x 0x5C
	inline OverlayCtx &Overlay() { return var<OverlayCtx>(0x278C7E8); }
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_Root = 0x8D6B30;
	static const uint32_t ORIG_Emitter = 0x8D6CC0;
	static const uint32_t ORIG_Ring = 0x8D6D80;
	static const uint32_t ORIG_Mote = 0x8D7F60;
	static const uint32_t ORIG_Sparkle = 0x8D80B0;
	static const uint32_t ORIG_Glint = 0x8DCC20;         // Effect_Glint_Tick (fx_glint.cpp)

	static const void *const SOUND_Cure = (const void *)0x16417C4;
	static const uint32_t SEQ_Ring = 0x1641F78, SEQ_RingEnd = 0x1641FCC, SEQ_Mote = 0x164205C, SEQ_Sparkle = 0x1642238;
	static const uint32_t SEQ_Glow = 0x1642504;
	static const uint32_t CAM_SNAPSHOT = 0x2793E58;       // Effect_SharedCamMatrixSnapshot
	static const uint32_t ARENA_A = 0x278C79C, LIMIT_A = 0x2783E1C, ARENA_B = 0x278C798, LIMIT_B = 0x2783E18;
	static const uint32_t OVERLAY_A00 = 0x277AEC8, OVERLAY_SPRITE = 0x278C7A0, OVERLAY_VERTS = 0x2783E28;
	static const uint32_t GLINT_CURSOR = 0x2792E74;       // the effect library's glint packet cursor address

	// ------------------------------------------------------------------
	// Engine / effect-library functions (original addresses)
	// ------------------------------------------------------------------
	// Effect_AddTaskAndInitFromCtx (0x8DC540): new node of `size` bytes in `q`, zeroed, header
	// inherited from `parent` (the parent's child count goes up)
	inline FxNode *AddEffectTask(TaskQueue *q, uint32_t task, int32_t size, void *parent) { return fn<FxNode *(__cdecl *)(TaskQueue *, uint32_t, int32_t, void *)>(0x8DC540)(q, task, size, parent); }
	inline void ReleaseLinkedTask(void *n) { fn<void (__cdecl *)(void *)>(0x8DC530)(n); }                 // the parent's child count goes down
	inline void EmitterUpdatePos(void *n) { fn<void (__cdecl *)(void *)>(0x8DC610)(n); }                  // MAG_001_CURE_Emitter_UpdatePos
	inline void ComputeModelBounds(void *n) { fn<void (__cdecl *)(void *)>(0x8DC870)(n); }                // MAG_001_CURE_Emitter_ComputeModelBounds
	inline void UpdateTargetPosFromBones(void *n) { fn<void (__cdecl *)(void *)>(0x8DC740)(n); }          // (caster, ctx +0)
	inline void CopyAnchorXZFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC6E0)(n, out); } // emitter +0x30
	inline int32_t BoundsMaxWorldY(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCAC0)(n); }      // Effect_GetTargetBoundsMaxWorldY (ax)
	inline int32_t BoundsRadius(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCA70)(n); }         // Effect_GetTargetBoundsRadius
	inline int32_t BoundsHeight(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCAA0)(n); }         // Effect_GetTargetBoundsHeight (ax)
	inline void IdentityMatrix(Mat4x3 *m) { fn<void (__cdecl *)(Mat4x3 *)>(0x8DD770)(m); }
	inline void RotateX(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD7E0)(m, a); }
	inline void BuildMatrixFromDirAndUp(void *out, const void *dir, const void *up) { fn<void (__cdecl *)(void *, const void *, const void *)>(0x8DDA50)(out, dir, up); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline void NormalizeVec3(const int32_t *in, int16_t *out) { fn<void (__cdecl *)(const int32_t *, int16_t *)>(0x56BD20)(in, out); }
	inline void UnpackRotationMatrix(const void *in, void *out) { fn<void (__cdecl *)(const void *, void *)>(0x56C040)(in, out); }
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	// software GTE
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteLoadV0Dwords(const void *v) { fn<void (__cdecl *)(const void *)>(0x45E060)(v); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }       // GTE_ReadOTZ (dword)
	inline void GteReadSXY012(void *dst) { fn<void (__cdecl *)(void *)>(0x45E2A0)(dst); }        // 3 consecutive dwords
	inline uint32_t GteSZ(int k) { return var<uint32_t>(0x1CA8A50 + 4 * k); }                   // SZ0..SZ3 data registers
	inline void GteStoreSXY012_GT3(void *p) { fn<void (__cdecl *)(void *)>(0x45E300)(p); }       // p +8, +0x14, +0x20
	inline void GteStoreSXY012_FT3(void *p) { fn<void (__cdecl *)(void *)>(0x45E2E0)(p); }       // p +8, +0x10, +0x18
	inline void GteStoreSXY012_GT4(void *p) { fn<void (__cdecl *)(void *)>(0x45E340)(p); }       // GTE_StoreSXY012_PolyGT3_2
	inline void GteStoreSXY012_FT4(void *p) { fn<void (__cdecl *)(void *)>(0x45E320)(p); }       // GTE_StoreSXY012_PolyFT3_2
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteSetBackground(int32_t x, int32_t y, int32_t z) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DCF0)(x, y, z); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }                    // IR = L * V0 + BK
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }
	inline void GteLoadIRFromColumn(const void *m) { fn<void (__cdecl *)(const void *)>(0x45E180)(m); }
	inline void GteStoreIRToColumn(void *m) { fn<void (__cdecl *)(void *)>(0x45E470)(m); }
	// drawing
	inline void InsertPrimDepthKeys(uint32_t ot, void *prim, uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, void *, uint32_t, uint32_t, uint32_t, uint32_t)>(0x45C870)(ot, prim, a, b, c, d); }
	// sub_5088A0: an entity's ground shadow (returns the packet cursor)
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	// battle models
	inline int32_t BattleReadAnimation(void *hdr, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(0x508F90)(hdr, cmd); }
	inline void PreBattleReadAnimation(void *hdr, void *cmd, uint32_t anim) { fn<void (__cdecl *)(void *, void *, uint32_t)>(0x509440)(hdr, cmd, anim); }

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }
	template<typename T> static inline const T &F(const uint8_t *p, uint32_t off) { return *(const T *)(p + off); }

	// x / 4096 as compiled (cdq; and edx, 0xFFF; add; sar 12)
	static inline int32_t Div4096(int32_t x) { return x / 4096; }

	// ------------------------------------------------------------------
	// Drawing helpers
	// ------------------------------------------------------------------
	// MAG_001_CURE_DrawSprite (0x8D6F20): the node's flipbook frame at its position (camera-facing)
	static void DrawSprite(const Particle *p)
	{
		if (p->h.flags & 4) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(p->h.pos, 0x1000, p->shade);
		F<uint32_t>(h, 0) = p->seq;
		F<int16_t>(h, 4) = p->frame;
		F<int16_t>(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// MAG_001_CURE_DrawAdditiveGlow (0x8D6F90): flipbook `seq` frame 0 lying on the ground under the
	// node (rotated 90 degrees about X, y = 0), in `colour`
	static void DrawAdditiveGlow(const Particle *p, uint32_t seq, const uint32_t *colour)
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
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// MAG_001_CURE_SetOverlayTexturePage (0x8D7050): projector position, the texture page / CLUT of
	// the projected image (x, y / clut x, y as GetTPage / GetClut), its window size and texel offset
	static void SetOverlayTexturePage(OverlayCtx *c, const int16_t *pos, int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t w, int32_t h)
	{
		c->light_pos[0] = pos[0];
		c->light_pos[1] = pos[1];
		c->light_pos[2] = pos[2];
		const int32_t yy = (int16_t)y;
		uint32_t t = ((uint32_t)x & 0x3C0) | 0x2800;
		t = (uint32_t)(uint16_t)((int16_t)t >> 6);
		t |= (uint32_t)(yy >> 4) & 0x10;
		t |= ((uint32_t)yy & 0x200) << 2;
		c->tpage = (uint16_t)t;
		c->clut = (uint16_t)((uint32_t)((cx >> 4) & 0x3F) | shl32(cy, 6));
		c->proj_w = (int16_t)w;
		c->u_off = (int16_t)(((uint32_t)x & 0x3F) << 1);
		c->v_off = (int16_t)(y & 0xFF);
		c->proj_h = (int16_t)h;
	}

	// MAG_001_CURE_Overlay_UpdateMatrices (0x8D7200): the entity's matrix, and a copy moved by `offset`
	static void UpdateMatrices(OverlayCtx *c, const uint8_t *entity)
	{
		memcpy(&c->world, entity + 0x40, 0x20);
		memcpy(&c->world2, entity + 0x40, 0x20);
		if (c->mode == 0)
		{
			int16_t out[4];
			MatrixMulVector(&c->world2, c->offset, out);
			c->world2.t[0] = (int32_t)((uint32_t)c->world2.t[0] + (uint32_t)(int32_t)out[0]);
			c->world2.t[1] = (int32_t)((uint32_t)c->world2.t[1] + (uint32_t)(int32_t)out[1]);
			c->world2.t[2] = (int32_t)((uint32_t)c->world2.t[2] + (uint32_t)(int32_t)out[2]);
		}
		else if (c->mode == 1)
		{
			c->world2.t[0] = (int32_t)((uint32_t)c->world2.t[0] + (uint32_t)(int32_t)c->offset[0]);
			c->world2.t[1] = (int32_t)((uint32_t)c->world2.t[1] + (uint32_t)(int32_t)c->offset[1]);
			c->world2.t[2] = (int32_t)((uint32_t)c->world2.t[2] + (uint32_t)(int32_t)c->offset[2]);
		}
	}

	// MAG_001_CURE_Overlay_BuildProjectorMatrix (0x8D72C0): `light` = the entity's rotation seen from
	// light_pos (looking at the moved entity origin, at light_dist)
	static void BuildProjectorMatrix(OverlayCtx *c)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x70);
		const int16_t x = (int16_t)c->world2.t[0];
		const int16_t y = (int16_t)c->world2.t[1];
		const int16_t z = (int16_t)c->world2.t[2];
		F<int16_t>(s, 0x68) = x;
		const int32_t dx = (int32_t)x - c->light_pos[0];
		F<int16_t>(s, 0x6A) = y;
		const int32_t dy = (int32_t)y - c->light_pos[1];
		F<int16_t>(s, 0x6C) = z;
		const int32_t dz = (int32_t)z - c->light_pos[2];
		F<int32_t>(s, 4) = dy;
		F<int32_t>(s, 8) = dz;
		F<int32_t>(s, 0) = dx;
		const int32_t d2 = (int32_t)((uint32_t)mul32(dz, dz) + (uint32_t)mul32(dy, dy) + (uint32_t)mul32(dx, dx));
		F<int32_t>(s, 0x60) = d2;
		F<int32_t>(s, 0x64) = Sqrt(d2);
		F<int16_t>(s, 0x12) = 0x1000;
		F<int16_t>(s, 0x10) = 0;
		F<int16_t>(s, 0x14) = 0;
		NormalizeVec3((const int32_t *)s, (int16_t *)(s + 0x18));
		BuildMatrixFromDirAndUp(s + 0x20, s + 0x18, s + 0x10);
		UnpackRotationMatrix(s + 0x20, s + 0x40);
		F<int32_t>(s, 0x5C) = c->light_dist;
		memcpy(s + 0x20, &c->world, 0x20);
		F<int32_t>(s, 0x54) = 0;
		F<int32_t>(s, 0x58) = 0;
		F<int32_t>(s, 0x38) = (int32_t)((uint32_t)c->world.t[1] - (uint32_t)c->world2.t[1]);
		F<int32_t>(s, 0x34) = (int32_t)((uint32_t)c->world.t[0] - (uint32_t)c->world2.t[0]);
		F<int32_t>(s, 0x3C) = (int32_t)((uint32_t)c->world.t[2] - (uint32_t)c->world2.t[2]);
		GteSetRotMatrixCtrl((const Mat4x3 *)(s + 0x40));
		uint8_t *light = (uint8_t *)&c->light;
		for (int k = 0; k < 3; k++)
		{
			GteLoadIRFromColumn(s + 0x20 + 2 * k);
			GteMVMVA_RotIR();
			GteStoreIRToColumn(light + 2 * k);
		}
		GteSetTransVectorCtrl((const Mat4x3 *)(s + 0x40));
		GteLoadV0Dwords(s + 0x34);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(c->light.t);
		FieldFree(0x70);
	}

	// the projected vertices inside the window (x0..x1, y0..y1) move by the texel offset; false = out
	static bool ProjectedUV(uint8_t *s, int count)
	{
		for (int v = 0; v < count; v++)
		{
			const int16_t x = F<int16_t>(s, 0x40 + 4 * v);
			if (x < F<int16_t>(s, 0xC0) || x > F<int16_t>(s, 0xC2)) return false;
			const int16_t y = F<int16_t>(s, 0x42 + 4 * v);
			if (y < F<int16_t>(s, 0xC4) || y > F<int16_t>(s, 0xC6)) return false;
			F<int16_t>(s, 0x40 + 4 * v) = (int16_t)(F<int16_t>(s, 0xC8) + x);
			F<int16_t>(s, 0x42 + 4 * v) = (int16_t)(F<int16_t>(s, 0x42 + 4 * v) + F<int16_t>(s, 0xCA));
		}
		return true;
	}

	// the three / four vertices again through the projector matrix: skipped (+0xCC = 1) when the
	// back-face test is on and they face away from it
	static void ProjectorPass(uint8_t *s, const uint8_t *verts)
	{
		GteSetRotMatrixCtrl((const Mat4x3 *)(s + 0x20));
		GteSetTransVectorCtrl((const Mat4x3 *)(s + 0x20));
		GteLoadV012(verts + F<uint32_t>(s, 0x94) * 16 + 4, verts + F<uint32_t>(s, 0x98) * 16 + 4, verts + F<uint32_t>(s, 0x9C) * 16 + 4);
		GteRTPT();
		F<int16_t>(s, 0xCC) = 0;
		GteNCLIP();
		GteReadMAC0(s + 0x8C);
		if (F<int16_t>(s, 0xCE) == 1 && F<int32_t>(s, 0x8C) <= 0) F<int16_t>(s, 0xCC) = 1;
	}

	// MAG_001_CURE_Overlay_RenderModel (0x8D7430): every object of `model` the entity's mask shows.
	// Work block (Field_Alloc 0xD0): +0x00 view matrix, +0x20 projector matrix, +0x40 projected
	// xy (4), +0x70 vertex, +0x8C NCLIP, +0x90 OTZ, +0x94..+0xA0 vertex indices, +0xA8 / +0xAC
	// projected colour codes (GT3 / GT4), +0xB0 / +0xB4 entity colour codes (FT3 / FT4), +0xB8
	// object mask, +0xBC tpage, +0xBE clut, +0xC0..+0xC6 window, +0xC8 / +0xCA texel offset,
	// +0xCC skip, +0xCE back-face test.
	static void RenderModel(const uint8_t *model, uint32_t ot, int32_t mode, OverlayCtx *c)
	{
		(void)mode;
		uint32_t fcur = *c->frame_cursor;
		uint32_t pcur = *c->packets;
		uint8_t *verts = *(uint8_t **)(c->sprite + 4);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const uint8_t *table = *(const uint8_t *const *)(model + 4);
		const int32_t count = *(const int32_t *)table;
		const uint8_t *entity = c->entity;
		uint8_t *s = (uint8_t *)FieldAlloc(0xD0);
		F<int16_t>(s, 0xCE) = c->cull;
		F<uint32_t>(s, 0xB8) = *(const uint32_t *)(entity + 0x7C);
		F<uint16_t>(s, 0xBC) = c->tpage;
		F<uint16_t>(s, 0xBE) = c->clut;
		const int32_t hw = (int32_t)c->proj_w / 2;
		const int32_t hh = (int32_t)c->proj_h / 2;
		F<int16_t>(s, 0xC2) = (int16_t)(hw + 0x9F);
		F<int16_t>(s, 0xC0) = (int16_t)(0xA0 - hw);
		F<int16_t>(s, 0xC6) = (int16_t)(hh + 0x77);
		F<int16_t>(s, 0xC4) = (int16_t)(0x78 - hh);
		F<int16_t>(s, 0xCA) = (int16_t)(c->v_off + (int16_t)hh - 0x78);
		F<int16_t>(s, 0xC8) = (int16_t)(c->u_off + (int16_t)hw - 0xA0);
		const uint32_t rgb = *(const uint32_t *)c->rgb & 0xFFFFFF;
		F<uint32_t>(s, 0xAC) = rgb | 0x3E000000;
		F<uint32_t>(s, 0xA8) = rgb | 0x36000000;
		const uint32_t col = *(const uint32_t *)(entity + 0x28) & 0xFFFFFF;
		F<uint32_t>(s, 0xB0) = col | 0x24000000;
		F<uint32_t>(s, 0xB4) = col | 0x2C000000;
		memcpy(s, &c->view, 0x20);
		memcpy(s + 0x20, &c->light, 0x20);
		F<int16_t>(s, 0xA4) = c->ce;
		F<int16_t>(s, 0xA6) = c->cc;
		GteSetRotMatrixCtrl((const Mat4x3 *)s);
		GteSetTransVectorCtrl((const Mat4x3 *)s);
		const uint32_t *offs = (const uint32_t *)(table + 4);
		for (int32_t k = 0; k < count; k++)
		{
			uint8_t *vcur = verts;
			const uint8_t *p = table + offs[k];
			if (!((F<uint32_t>(s, 0xB8) >> (k & 31)) & 1)) continue;
			// vertices, grouped by bone: L = bone matrix, BK = its translation
			const int32_t groups = *(const int16_t *)p;
			p += 2;
			for (int32_t g = groups; g > 0; g--)
			{
				const int32_t bone = *(const int16_t *)p;
				p += 2;
				const uint8_t *bm = bones + (uint32_t)mul32(bone, 0x30) + 0x10;
				GteSetLightMatrix(bm);
				GteSetBackground(F<int32_t>(bm, 0x14), F<int32_t>(bm, 0x18), F<int32_t>(bm, 0x1C));
				const int32_t nv = *(const int16_t *)p;
				p += 2;
				if (nv <= 0) continue;
				uint8_t *out = vcur + 4;
				vcur += (uint32_t)nv * 16;
				for (int32_t v = nv; v > 0; v--)
				{
					F<int16_t>(s, 0x70) = *(const int16_t *)p;
					F<int16_t>(s, 0x72) = *(const int16_t *)(p + 2);
					F<int16_t>(s, 0x74) = *(const int16_t *)(p + 4);
					p += 6;
					GteLoadV0(s + 0x70);
					GteMVMVA_LightV0Bk();
					GteStoreIR123(out);
					out += 0x10;
				}
			}
			// polygons: 12-byte header (triangle count, quad count), 16-byte triangles, 20-byte quads
			p = (const uint8_t *)(((uint32_t)p + 3) & ~3u);
			const int32_t tris = *(const int16_t *)p;
			const int32_t quads = *(const int16_t *)(p + 2);
			const uint8_t *poly = p + 0xC;
			uint8_t *gp = (uint8_t *)pcur;
			uint8_t *fp = (uint8_t *)fcur;
			for (int32_t t = tris; t > 0; t--, poly += 0x10)
			{
				const uint32_t i2 = *(const uint16_t *)(poly + 4) & 0xFFF;
				const uint32_t i1 = *(const uint16_t *)(poly + 2) & 0xFFF;
				const uint32_t i0 = *(const uint16_t *)poly & 0xFFF;
				F<uint32_t>(s, 0x9C) = i2;
				F<uint32_t>(s, 0x98) = i1;
				F<uint32_t>(s, 0x94) = i0;
				GteLoadV012(verts + i0 * 16 + 4, verts + i1 * 16 + 4, verts + i2 * 16 + 4);
				GteRTPT();
				const uint32_t z1 = GteSZ(1), z2 = GteSZ(2), z3 = GteSZ(3);
				GteNCLIP();
				GteReadMAC0(s + 0x8C);
				if (F<int32_t>(s, 0x8C) <= 0) continue;
				GteAVSZ3();
				GteReadOTZWord(s + 0x90);
				F<uint32_t>(s, 0x90) >>= 2;
				GteStoreSXY012_GT3(gp);
				GteStoreSXY012_FT3(fp);
				ProjectorPass(s, verts);
				if (F<int16_t>(s, 0xCC) == 0)
				{
					GteReadSXY012(s + 0x40);
					if (!ProjectedUV(s, 3)) F<int16_t>(s, 0xCC) = 1;
					if (F<int16_t>(s, 0xCC) == 0)
					{
						// the triangle in the projected light: gouraud, Cure texture page
						gp[0xC] = s[0x40];
						gp[0xD] = s[0x42];
						gp[0x18] = s[0x44];
						gp[0x19] = s[0x46];
						gp[0x24] = s[0x48];
						gp[0x25] = s[0x4A];
						F<uint16_t>(gp, 0x1A) = F<uint16_t>(s, 0xBC);
						F<uint16_t>(gp, 0xE) = F<uint16_t>(s, 0xBE);
						F<uint32_t>(gp, 0x1C) = F<uint32_t>(s, 0xA8);
						F<uint32_t>(gp, 0x10) = F<uint32_t>(s, 0xA8);
						F<uint32_t>(gp, 4) = F<uint32_t>(s, 0xA8);
						F<uint32_t>(gp, 0) = 0x9000000;
						InsertPrimAutoDepth(ot + F<uint32_t>(s, 0x90) * 4, gp);
						gp += 0x28;
					}
				}
				// the triangle itself: flat in the entity's colour, its own texture
				F<uint32_t>(fp, 0xC) = *(const uint32_t *)(poly + 8);
				F<uint32_t>(fp, 0x14) = *(const uint32_t *)(poly + 0xC);
				F<uint16_t>(fp, 0x1C) = *(const uint16_t *)(poly + 6);
				F<uint32_t>(fp, 4) = F<uint32_t>(s, 0xB0);
				F<uint32_t>(fp, 0) = 0x7000000;
				if (poly[0xF] & 2) fp[7] |= 2;
				InsertPrimDepthKeys(ot + F<uint32_t>(s, 0x90) * 4, fp, z1, z2, z3, 0);
				fp += 0x20;
				GteSetRotMatrixCtrl((const Mat4x3 *)s);
				GteSetTransVectorCtrl((const Mat4x3 *)s);
			}
			for (int32_t q = quads; q > 0; q--, poly += 0x14)
			{
				const uint32_t i2 = *(const uint16_t *)(poly + 4) & 0xFFF;
				const uint32_t i1 = *(const uint16_t *)(poly + 2) & 0xFFF;
				const uint32_t i0 = *(const uint16_t *)poly & 0xFFF;
				F<uint32_t>(s, 0x9C) = i2;
				F<uint32_t>(s, 0x98) = i1;
				F<uint32_t>(s, 0x94) = i0;
				GteLoadV012(verts + i0 * 16 + 4, verts + i1 * 16 + 4, verts + i2 * 16 + 4);
				GteRTPT();
				const uint32_t z1 = GteSZ(1), z2 = GteSZ(2), z3 = GteSZ(3);
				GteNCLIP();
				GteReadMAC0(s + 0x8C);
				if (F<int32_t>(s, 0x8C) <= 0) continue;
				GteStoreSXY012_GT4(gp);
				GteStoreSXY012_FT4(fp);
				const uint32_t i3 = *(const uint16_t *)(poly + 6) & 0xFFF;
				F<uint32_t>(s, 0xA0) = i3;
				GteLoadV0(verts + i3 * 16 + 4);
				GteRTPS();
				const uint32_t z4 = GteSZ(3);
				GteReadSXY2(gp + 0x2C);
				GteReadSXY2(fp + 0x20);
				GteAVSZ4();
				GteReadOTZWord(s + 0x90);
				F<uint32_t>(s, 0x90) >>= 2;
				ProjectorPass(s, verts);
				if (F<int16_t>(s, 0xCC) == 0)
				{
					GteReadSXY012(s + 0x40);
					GteLoadV0(verts + F<uint32_t>(s, 0xA0) * 16 + 4);
					GteRTPS();
					GteReadSXY2(s + 0x4C);
					if (!ProjectedUV(s, 4)) F<int16_t>(s, 0xCC) = 1;
					if (F<int16_t>(s, 0xCC) == 0)
					{
						gp[0xD] = s[0x42];
						gp[0xC] = s[0x40];
						gp[0x18] = s[0x44];
						gp[0x24] = s[0x48];
						gp[0x19] = s[0x46];
						gp[0x25] = s[0x4A];
						gp[0x31] = s[0x4E];
						gp[0x30] = s[0x4C];
						F<uint16_t>(gp, 0x1A) = F<uint16_t>(s, 0xBC);
						F<uint32_t>(gp, 0x28) = F<uint32_t>(s, 0xAC);
						F<uint32_t>(gp, 0x1C) = F<uint32_t>(s, 0xAC);
						F<uint32_t>(gp, 0x10) = F<uint32_t>(s, 0xAC);
						F<uint32_t>(gp, 4) = F<uint32_t>(s, 0xAC);
						F<uint16_t>(gp, 0xE) = F<uint16_t>(s, 0xBE);
						F<uint32_t>(gp, 0) = 0xC000000;
						InsertPrimAutoDepth(ot + F<uint32_t>(s, 0x90) * 4, gp);
						gp += 0x34;
					}
				}
				F<uint32_t>(fp, 0xC) = *(const uint32_t *)(poly + 8);
				F<uint32_t>(fp, 0x14) = *(const uint32_t *)(poly + 0xC);
				F<uint16_t>(fp, 0x1C) = *(const uint16_t *)(poly + 0x10);
				F<uint16_t>(fp, 0x24) = *(const uint16_t *)(poly + 0x12);
				F<uint32_t>(fp, 4) = F<uint32_t>(s, 0xB4);
				F<uint32_t>(fp, 0) = 0x9000000;
				if (poly[0xF] & 2) fp[7] |= 2;
				InsertPrimDepthKeys(ot + F<uint32_t>(s, 0x90) * 4, fp, z1, z2, z3, z4);
				fp += 0x28;
				GteSetRotMatrixCtrl((const Mat4x3 *)s);
				GteSetTransVectorCtrl((const Mat4x3 *)s);
			}
			pcur = (uint32_t)gp;
			fcur = (uint32_t)fp;
		}
		*c->frame_cursor = fcur;
		*c->packets = pcur;
		FieldFree(0xD0);
	}

	// MAG_001_CURE_DrawTargetModelWithOverlay (0x8D7110): shadow, body (+0x64) and weapon (+0x78)
	// of the target with the projected light
	static void DrawTargetModelWithOverlay(OverlayCtx *c)
	{
		uint8_t *entity = c->entity;
		UpdateMatrices(c, entity);
		if (c->slot != 0xFF) BuildBoneMatricesFromPose(entity + 0x60);
		else
		{
			const int32_t more = BattleReadAnimation(entity + 0x60, entity + 0x6C);
			// the animation number goes in the low word of the returned register
			if (more) PreBattleReadAnimation(entity + 0x60, entity + 0x6C, ((uint32_t)more & 0xFFFF0000u) | (uint16_t)c->anim);
		}
		ComposeAffineTransform(&Camera(), &c->world, &c->view);
		if (!(entity[0] & 0x20)) *c->frame_cursor = DrawShadow(entity, RenderOT(0x4040), 0x10, *c->frame_cursor);
		BuildProjectorMatrix(c);
		RenderModel(*(const uint8_t *const *)(entity + 0x64), RenderOT(0x44), 4, c);
		const uint8_t *weapon = *(const uint8_t *const *)(entity + 0x78);
		if (weapon) RenderModel(*(const uint8_t *const *)(weapon + 4), RenderOT(0x44), 4, c);
		if (c->slot != 0xFF) BuildBoneMatricesFromPose(entity + 0x60);
	}

	// MAG_001_CURE_SetupTargetOverlay (0x8D8280)
	static void SetupTargetOverlay(OverlayCtx *c, uint32_t a00, uint8_t *sprite, uint32_t verts, uint8_t *entity, int32_t slot)
	{
		c->entity = entity;
		c->a00 = a00;
		c->slot = (int16_t)slot;
		c->sprite = sprite;
		c->frame_cursor = (uint32_t *)0x1D8E054;
		c->packets = (uint32_t *)0x277AEC4;
		c->offset[0] = 0;
		c->offset[1] = 0;
		c->offset[2] = 0;
		c->rgb[0] = 0x80;
		c->rgb[1] = 0x80;
		c->rgb[2] = 0x80;
		c->light_dist = 0x2000;
		c->mode = 1;
		c->ce = 0;
		c->glow = 0;
		c->a4[0] = 0;
		c->a4[1] = 0;
		c->a4[2] = 0;
		c->ac[0] = 0;
		c->ac[1] = 0;
		c->ac[2] = 0;
		c->bc[0] = 0;
		c->bc[1] = 0;
		c->bc[2] = 0;
		sprite[0x1C] = 0x80;
		sprite[0x1D] = 0x80;
		sprite[0x1E] = 0x80;
		F<uint32_t>(sprite, 4) = verts;
		F<uint16_t>(sprite, 0x14) = 0;
		F<uint16_t>(sprite, 0x16) = 0;
		F<uint16_t>(sprite, 0x18) = 0x140;
		F<uint16_t>(sprite, 0x1A) = 0;
		F<uint32_t>(sprite, 0x20) = 0xFFFFFFFF;
		F<uint16_t>(sprite, 0x24) = 0;
	}

	// ------------------------------------------------------------------
	// Particles
	// ------------------------------------------------------------------
	// Effect_LifetimeTick_Expired (0x8D8040): next flipbook frame; past the last one the node is
	// hidden (frame stays last) and 1 is returned
	static int32_t LifetimeExpired(Particle *p)
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

	// mote / sparkle phase 1 (0x8D8000 / 0x8D8160): position += velocity, done at the last frame
	static void Integrate(Particle *p)
	{
		p->h.pos[0] = (int16_t)(p->h.pos[0] + p->vel[0]);
		p->h.pos[1] = (int16_t)(p->h.pos[1] + p->vel[1]);
		p->h.pos[2] = (int16_t)(p->h.pos[2] + p->vel[2]);
		if (LifetimeExpired(p))
		{
			*(uint8_t *)&p->h.flags |= 1;
			p->h.phase++;
		}
	}

	// the ring's place at sub-step `step` (4 * counter + s): centre, + step * climb in y, radius *
	// (sin, cos)(step * 0x80) in x / z
	static void RingPlace(Particle *r, uint32_t step)
	{
		memcpy(r->h.pos, r->center, 8);
		r->h.pos[1] = (int16_t)(r->h.pos[1] + (int16_t)(r->vel[1] * (int16_t)step));
		const int32_t angle = (int32_t)((step << 7) & 0xFFF);
		r->angle = (int16_t)angle;
		r->h.pos[0] = (int16_t)(r->h.pos[0] + Div4096(mul32(ComputeSin((int16_t)angle), r->radius)));
		r->h.pos[2] = (int16_t)(r->h.pos[2] + Div4096(mul32(ComputeCos(r->angle), r->radius)));
	}

	// MAG_001_CURE_Ring_SpawnSparkle (0x8D8080): counter 1..16
	static void SpawnSparkle(Particle *r)
	{
		if (r->h.counter > 0 && r->h.counter < 0x11) AddEffectTask(QParticles(), ORIG_Sparkle, 0x6C, r);
	}

	// MAG_001_CURE_Ring_SpawnMotes (0x8D7E20): counter 2..: 1 (2..6), 2 (7, 8), 3 (9, 10), 2 (11..)
	// motes on a circle of 1.5 x the target's radius (at most 0x400) at a random angle, at the ring's
	// height lowered by a random part of the ring's height above the target's top
	static void SpawnMotes(Particle *r)
	{
		const int16_t c = r->h.counter;
		if (c <= 1 || c >= 0x7FFF) return;
		int32_t n;
		if (c > 10) n = 2;
		else if (c > 8) n = 3;
		else n = (c > 6) + 1;
		for (; n; n--)
		{
			const int32_t a = CrtRand() & 0xFFF;
			Particle *m = (Particle *)AddEffectTask(QParticles(), ORIG_Mote, 0x6C, r);
			CopyAnchorXZFromSource(m, m->h.pos);
			m->h.pos[1] = r->h.pos[1];
			int32_t d = (int32_t)r->h.pos[1] - (int32_t)(int16_t)BoundsMaxWorldY(r);
			d = mul32(d, CrtRand() & 0x1F);
			// d / -48 (0xD5555555, >> 3)
			int32_t q = (int32_t)(((int64_t)d * (int32_t)0xD5555555) >> 32) >> 3;
			q += (int32_t)((uint32_t)q >> 31);
			m->h.pos[1] = (int16_t)(m->h.pos[1] + q);
			const int32_t rad = (int32_t)(int16_t)BoundsRadius(r) * 3 / 2;
			m->radius = (int16_t)rad;
			if ((int16_t)rad > 0x400) m->radius = 0x400;
			m->h.pos[0] = (int16_t)(m->h.pos[0] + Div4096(mul32(ComputeSin((int16_t)a), m->radius)));
			m->h.pos[2] = (int16_t)(m->h.pos[2] + Div4096(mul32(ComputeCos((int16_t)a), m->radius)));
			memcpy(m->vel, r->vel, 8);
		}
	}

	// MAG_001_CURE_Ring_SpawnGlints (0x8D7D50): counter 4..10: 2 (4..6), 3 (7, 8), 4 (9, 10) glints
	// (Effect_Glint_Tick) drawing into the module cursor: 8 ticks growing, 8 fading, spinning either way
	static void SpawnGlints(Particle *r)
	{
		const int16_t c = r->h.counter;
		if (c <= 3 || c >= 0xB) return;
		int32_t n;
		if (c > 10) n = 2;
		else if (c > 8) n = 4;
		else n = (c > 6) + 2;
		for (; n; n--)
		{
			var<uint32_t>(GLINT_CURSOR) = 0x277AEC4;
			uint8_t *g = (uint8_t *)AddEffectTask(QGlints(), ORIG_Glint, 0x5C, r);
			F<int16_t>(g, 0x56) = 8;
			g[0x4C] = 0x10;
			g[0x4D] = 0x10;
			g[0x4E] = 0x10;
			g[0x50] = 0;
			g[0x51] = 0;
			g[0x52] = 0x10;
			F<int16_t>(g, 0x3C) = 0x100;
			F<int16_t>(g, 0x3E) = 0x10;
			F<int16_t>(g, 0x40) = 0x40;
			F<int16_t>(g, 0x42) = 0x10;
			if (CrtRand() & 1) F<int16_t>(g, 0x58) = 0x40;
			else F<int16_t>(g, 0x58) = (int16_t)0xFFC0;
		}
	}

	// ring phase 0 (MAG_001_CURE_Ring_State0_Init 0x8D81B0): spawning on; centre on the line's
	// anchor at the top of the target's bounds; radius 2 x the bounds radius (at most 0x400); climb
	// (height + 0x100) / -20 / 4 per sub-step; the overlay on the target, which the normal renderer
	// then skips (entity flag 4)
	static void RingInit(Particle *r)
	{
		*(uint8_t *)&r->h.flags |= 2;
		r->seq = SEQ_Ring;
		uint8_t *entity = Entity(r->h.slot);
		CopyAnchorXZFromSource(r, r->center);
		r->center[1] = (int16_t)BoundsMaxWorldY(r);
		const int32_t rad = shl32(BoundsRadius(r), 1);
		r->radius = (int16_t)rad;
		if ((int16_t)rad > 0x400) r->radius = 0x400;
		const int32_t hv = (int32_t)(int16_t)BoundsHeight(r) + 0x100;
		// hv / -20 (0x99999999, >> 3)
		int32_t q = (int32_t)(((int64_t)hv * (int32_t)0x99999999) >> 32) >> 3;
		q += (int32_t)((uint32_t)q >> 31);
		r->vel[1] = (int16_t)((int32_t)(int16_t)q / 4);
		SetupTargetOverlay(&Overlay(), OVERLAY_A00, (uint8_t *)OVERLAY_SPRITE, OVERLAY_VERTS, entity, r->h.slot);
		*(uint8_t *)&r->h.flags |= 8;
		entity[0] |= 4;
		r->h.phase++;
	}

	// ring phase 2 (0x8D83A0): at the flipbook's end: the root is free again, the target shown,
	// spawning and overlay off, done
	static void RingEnd(Particle *r)
	{
		if (!LifetimeExpired(r)) return;
		((RootNode *)r->h.root)->busy = 0;
		*(uint16_t *)Entity(r->h.slot) &= 0xFFFB;
		uint16_t f = r->h.flags;
		f = (uint16_t)((f & 0xFF00) | (uint8_t)(((uint8_t)f & 0xF5) | 1));
		r->h.flags = f;
		r->h.phase++;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag001_cure_held.h"
#endif

namespace ff8fx
{
namespace cure001
{
	// ------------------------------------------------------------------
	// Sparkle (0x8D80B0) / Mote (0x8D7F60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkleTask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: // 0x8D8110: random velocity, flying out and up
			p->seq = SEQ_Sparkle;
			p->last = 8;
			p->vel[0] = (int16_t)((CrtRand() & 0x1F) - 0x10);
			p->vel[2] = (int16_t)((CrtRand() & 0x1F) - 0x10);
			p->vel[1] = (int16_t)((CrtRand() & 0xF) + 0x10);
			p->h.phase++;
			break;
		case 1: Integrate(p); break;
		default: break;
		}
		// 30 fps layer: see mag001_cure_held.inc
		FX_HELD(held_note_particle(p);)
		DrawSprite(p);
		p->h.counter++;
		if ((p->h.flags & 1) && p->h.children == 0)
		{
			ReleaseLinkedTask(p);
			return TASK_END;
		}
		return 0;
	}

	static uint32_t __cdecl MoteTask(TaskNode *n)
	{
		Particle *p = (Particle *)n;
		switch (p->h.phase)
		{
		case 0: // 0x8D7FC0: rising at a random speed
			p->seq = SEQ_Mote;
			p->last = 0x10;
			p->vel[1] = (int16_t)(-8 - (CrtRand() & 0xF));
			p->vel[0] = 0;
			p->vel[2] = 0;
			p->h.phase++;
			break;
		case 1: Integrate(p); break;
		default: break;
		}
		// 30 fps layer: see mag001_cure_held.inc
		FX_HELD(held_note_particle(p);)
		DrawSprite(p);
		p->h.counter++;
		if ((p->h.flags & 1) && p->h.children == 0)
		{
			ReleaseLinkedTask(p);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Ring (0x8D6D80)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		Particle *r = (Particle *)n;
		switch (r->h.phase)
		{
		case 0: RingInit(r); break;
		case 1: // 0x8D8370: at 20 the ending flipbook
			if (r->h.counter >= 0x14)
			{
				r->seq = SEQ_RingEnd;
				r->last = 4;
				r->h.phase++;
			}
			break;
		case 2: RingEnd(r); break;
		default: break;
		}
		for (uint32_t s = 0; s < 4; s++)
		{
			RingPlace(r, (uint32_t)(uint16_t)(r->h.counter << 2) + s);
			if (r->h.flags & 2) SpawnSparkle(r);
		}
		if (r->h.flags & 2)
		{
			SpawnMotes(r);
			SpawnGlints(r);
		}
		// 30 fps layer: see mag001_cure_held.inc
		FX_HELD(held_note_ring(r);)
		if (r->h.flags & 8)
		{
			Overlay().offset[1] = r->h.pos[1];
			SetOverlayTexturePage(&Overlay(), r->h.pos, 0x200, 0x180, 0x140, 0xF1, 0x80, 0x80);
			DrawTargetModelWithOverlay(&Overlay());
		}
		if (r->h.counter < 0x14)
		{
			// grey 0x3C - 3 * counter; the 4th byte is the high byte of the task argument (the node
			// address) the colour is written over on the original's stack
			const uint32_t grey = (uint8_t)(0x3C - (uint8_t)(r->h.counter * 3));
			const uint32_t colour = grey | grey << 8 | grey << 16 | ((uint32_t)r & 0xFF000000u);
			DrawSprite(r);
			DrawAdditiveGlow(r, SEQ_Glow, &colour);
		}
		else DrawSprite(r);
		r->h.counter++;
		if ((r->h.flags & 1) && r->h.children == 0)
		{
			ReleaseLinkedTask(r);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Emitter (0x8D6CC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl EmitterTask(TaskNode *n)
	{
		EmitterNode *e = (EmitterNode *)n;
		EmitterUpdatePos(e);
		ComputeModelBounds(e);
		switch (e->h.phase)
		{
		case 0: // 0x8D6D50: the ring
			AddEffectTask(QParticles(), ORIG_Ring, 0x6C, e);
			e->h.phase++;
			break;
		case 1: e->h.phase++; break;
		case 2: // 0x8D8410: heal from 15
			if (e->h.counter >= 0xF)
			{
				ApplyActionResultToTarget(e->h.ctx->actions[e->h.action].targets + e->h.target * TARGET_STRIDE);
				*(uint8_t *)&e->h.flags |= 1;
				e->h.phase++;
			}
			break;
		default: break;
		}
		if (e->h.counter == 0) BdPlaySE(SOUND_Cure, 0, 0x80);
		e->h.counter++;
		if ((e->h.flags & 1) && e->h.children == 0)
		{
			ReleaseLinkedTask(e);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root (0x8D6B30)
	// ------------------------------------------------------------------
	static void RootPhase(RootNode *r)
	{
		switch (r->h.phase)
		{
		case 0: // 0x8D6C60
		case 1: // 0x8D6C70
			r->h.phase++;
			break;
		case 2: // 0x8D6C80: children gone
			if (r->h.children == 0) r->h.phase++;
			break;
		case 3: // 0x8D6C90: the emitter of the current action
			r->busy = 1;
			AddEffectTask(QEmitters(), ORIG_Emitter, 0x58, r);
			r->h.phase++;
			break;
		case 4: // 0x8D8460: when the ring is over, the next action (back to 3) or on
			if (r->busy == 0)
			{
				const int8_t a = r->h.action;
				if ((int16_t)a < r->last_action)
				{
					r->h.action = (int8_t)(a + 1);
					r->h.line++;
					r->h.phase--;
				}
				else r->h.phase++;
			}
			break;
		case 5: // 0x8D84A0
			r->h.phase++;
			break;
		case 6: // 0x8D84B0: every queue empty
			if (r->alive == 0) r->h.phase++;
			break;
		case 7: // 0x8D84C0
		case 8: // 0x8D84D0
			r->h.phase++;
			break;
		case 9: // 0x8D84E0: done
			*(uint8_t *)&r->h.flags |= 1;
			r->h.phase++;
			break;
		default: break; // 0x8D8500
		}
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		memcpy((void *)CAM_SNAPSHOT, &Camera(), 0x20);
		var<uint32_t>(0x278C7DC) = CAM_SNAPSHOT;
		var<uint32_t>(0x277AEF0) = CAM_SNAPSHOT;
		if (r->tick & 1)
		{
			PacketCursor() = var<uint32_t>(ARENA_A);
			PacketLimit() = var<uint32_t>(LIMIT_A);
		}
		else
		{
			PacketCursor() = var<uint32_t>(ARENA_B);
			PacketLimit() = var<uint32_t>(LIMIT_B);
		}
		// 30 fps layer: see mag001_cure_held.inc
		FX_HELD(held_note_root(r);)
		UpdateTargetPosFromBones(r);
		RootPhase(r);
		r->alive = 0;
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QEmitters()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QParticles()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QGlints()));
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

	void register_mag001_cure()
	{
		register_port(cure001::ORIG_Root, (void *)cure001::RootTask, "001 Root", 1);
		register_port(cure001::ORIG_Emitter, (void *)cure001::EmitterTask, "001 Emitter", 1);
		register_port(cure001::ORIG_Ring, (void *)cure001::RingTask, "001 Ring", 1);
		register_port(cure001::ORIG_Mote, (void *)cure001::MoteTask, "001 Mote", 1);
		register_port(cure001::ORIG_Sparkle, (void *)cure001::SparkleTask, "001 Sparkle", 1);
		// 30 fps layer: see mag001_cure_held.inc
		FX_HELD(register_mag001_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag001_cure_held.inc"
#endif
