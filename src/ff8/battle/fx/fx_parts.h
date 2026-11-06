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

// The pieces effect modules are built from: the effect library's node header and calls, the inputs
// a value is measured against, the behaviours a part follows and the ways a part is drawn. Each
// takes plain values (positions, flipbook state, parameters, the packet cursor to draw into), never
// a module's nodes or memory, so one module's code can serve every copy of it (Cure and Potion:
// mag_heal_ring_code.h) and a part can be reused by another effect. Every piece is bit-exact with
// the original code it comes from (named at each), in the same order of random draws and calls.

#pragma once

#include "mag_common.h"
#include "fx_params.h"

namespace ff8fx
{
namespace parts
{
	using namespace eng;
	using namespace magc;

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
#pragma pack(pop)
	static_assert(sizeof(FxNode) == 0x30, "effect-library node header");

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }
	template<typename T> static inline const T &F(const uint8_t *p, uint32_t off) { return *(const T *)(p + off); }

	// x / 4096 as compiled (cdq; and edx, 0xFFF; add; sar 12)
	inline int32_t Div4096(int32_t x) { return x / 4096; }

	// g_Battle_FrameRenderListBase + off: an ordering table bucket
	inline uint32_t RenderOTAt(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	// ------------------------------------------------------------------
	// Nodes (the effect library 0x8DC530..)
	// ------------------------------------------------------------------
	// Effect_AddTaskAndInitFromCtx (0x8DC540): new node of `size` bytes in `q`, zeroed, header
	// inherited from `parent` (the parent's child count goes up)
	inline FxNode *AddEffectTask(TaskQueue *q, uint32_t task, int32_t size, void *parent) { return fn<FxNode *(__cdecl *)(TaskQueue *, uint32_t, int32_t, void *)>(0x8DC540)(q, task, size, parent); }
	inline void ReleaseLinkedTask(void *n) { fn<void (__cdecl *)(void *)>(0x8DC530)(n); }                 // the parent's child count goes down
	// a task's end: flagged done and no live child left -> released, TASK_END
	inline uint32_t EndWhenDone(FxNode *n)
	{
		if ((n->flags & 1) && n->children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Inputs: what a value is measured against (the node's line: its emitter, its target)
	// ------------------------------------------------------------------
	inline void Emitter_UpdatePos(void *n) { fn<void (__cdecl *)(void *)>(0x8DC610)(n); }                  // the target's anchor (MAG_001_CURE_Emitter_UpdatePos)
	inline void Emitter_ComputeModelBounds(void *n) { fn<void (__cdecl *)(void *)>(0x8DC870)(n); }         // the target's model bounds (MAG_001_CURE_Emitter_ComputeModelBounds)
	inline void UpdateTargetPosFromBones(void *n) { fn<void (__cdecl *)(void *)>(0x8DC740)(n); }          // the caster's bones (ctx +0)
	inline void CopyAnchorXZFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC6E0)(n, out); } // the target's anchor x / z (emitter +0x30)
	inline int32_t BoundsMaxWorldY(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCAC0)(n); }      // the target's bounds bottom: its feet, y down (Effect_GetTargetBoundsMaxWorldY, ax)
	inline int32_t BoundsRadius(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCA70)(n); }         // the target's bounds radius (Effect_GetTargetBoundsRadius)
	inline int32_t BoundsHeight(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCAA0)(n); }         // the target's bounds height (Effect_GetTargetBoundsHeight, ax)

	// ------------------------------------------------------------------
	// Engine calls the parts use (original addresses)
	// ------------------------------------------------------------------
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
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }                     // GTE_MVMVA_RotIR
	inline void GteLoadIRFromMatrixColumn(const void *m) { fn<void (__cdecl *)(const void *)>(0x45E180)(m); }   // GTE_LoadIRFromMatrixColumn
	inline void GteStoreIRToMatrixColumn(void *m) { fn<void (__cdecl *)(void *)>(0x45E470)(m); }               // GTE_StoreIRToMatrixColumn
	// drawing: SSIGPU_InsertPrimDepthKeys (0x45C870), the OT insertion keyed by the GTE SZ depths
	inline void InsertPrimDepthKeys(uint32_t ot, void *prim, uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, void *, uint32_t, uint32_t, uint32_t, uint32_t)>(0x45C870)(ot, prim, a, b, c, d); }
	// BattleEntity_DrawShadow (0x5088A0): an entity's ground shadow (returns the packet cursor)
	inline uint32_t BattleEntity_DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	// battle models
	inline int32_t BattleReadAnimation(void *hdr, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(0x508F90)(hdr, cmd); }
	inline void PreBattleReadAnimation(void *hdr, void *cmd, uint32_t anim) { fn<void (__cdecl *)(void *, void *, uint32_t)>(0x509440)(hdr, cmd, anim); }

	// ------------------------------------------------------------------
	// Behaviours
	// ------------------------------------------------------------------
	// a flipbook one frame further; past `last` the node is hidden (flags bit2) and the frame stays
	// last: true (Effect_LifetimeTick_Expired 0x8D8040 and its copies)
	inline bool FlipbookExpired(int16_t &frame, int16_t last, uint16_t &flags)
	{
		frame++;
		if (frame > last)
		{
			*(uint8_t *)&flags |= 4;
			frame = last;
			return true;
		}
		return false;
	}

	// position += velocity, per axis (16-bit, wrapping)
	inline void Drift(int16_t *pos, const int16_t *vel)
	{
		pos[0] = (int16_t)(pos[0] + vel[0]);
		pos[1] = (int16_t)(pos[1] + vel[1]);
		pos[2] = (int16_t)(pos[2] + vel[2]);
	}

	// a velocity rolled once per axis, x then z then y (the random draws' order: Cure's sparkles)
	inline void RollVelocity(int16_t *vel, const fxp::Range3 &speed)
	{
		vel[0] = (int16_t)speed.x.roll(CrtRand());
		vel[2] = (int16_t)speed.z.roll(CrtRand());
		vel[1] = (int16_t)speed.y.roll(CrtRand());
	}

	// a point on a circle: x += radius x sin(angle), z += radius x cos(angle) (4096 = a full turn)
	inline void OnCircle(int16_t *pos, int16_t radius, int32_t angle)
	{
		pos[0] = (int16_t)(pos[0] + Div4096(mul32(ComputeSin((int16_t)angle), radius)));
		pos[2] = (int16_t)(pos[2] + Div4096(mul32(ComputeCos((int16_t)angle), radius)));
	}

	// a helix (Cure's ring): at step n the centre, + n x climb in y, radius x (sin, cos)(n x angle)
	// in x / z; `angle_out` gets n x angle (12 bits)
	inline void HelixPlace(int16_t *pos, int16_t &angle_out, const int16_t *centre, int16_t climb, int16_t radius, uint32_t step, int16_t angle_step)
	{
		memcpy(pos, centre, 8);
		pos[1] = (int16_t)(pos[1] + (int16_t)(climb * (int16_t)step));
		const int32_t angle = (int32_t)((step * (uint32_t)angle_step) & 0xFFF);
		angle_out = (int16_t)angle;
		pos[0] = (int16_t)(pos[0] + Div4096(mul32(ComputeSin((int16_t)angle), radius)));
		pos[2] = (int16_t)(pos[2] + Div4096(mul32(ComputeCos(angle_out), radius)));
	}

	// an additive grey fading by `fade` a tick from `start`, one byte (below 0 it wraps), in the
	// three colour bytes; the 4th byte is `high`'s
	inline uint32_t FadingGrey(uint8_t start, uint8_t fade, int16_t tick, uint32_t high)
	{
		const uint32_t grey = (uint8_t)(start - (uint8_t)(tick * fade));
		return grey | grey << 8 | grey << 16 | high;
	}

	// ------------------------------------------------------------------
	// Renderers
	// ------------------------------------------------------------------
	// a flipbook frame facing the camera at `pos` (MAG_001_CURE_DrawSprite 0x8D6F20 and its copies):
	// `size` the camera scale it is drawn with (TransformCameraByShadowRotation 0x571BC0: 4096 = its
	// own size), `shade` moves it along the line of sight, `colour` 128 = as drawn; packets at `cursor`
	inline void DrawSprite(const int16_t *pos, uint32_t seq, int16_t frame, int16_t shade, bool hidden,
	                       const fxp::Colour &colour, int16_t size, uint32_t &cursor)
	{
		if (hidden) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(pos, size, shade);
		F<uint32_t>(h, 0) = seq;
		F<int16_t>(h, 4) = frame;
		F<int16_t>(h, 0x24) = 0;
		if (!colour.unchanged())                 // the game's: the drawer's own 0x808080
		{
			F<uint32_t>(h, 0x1C) = colour.word();
			F<int16_t>(h, 0x24) = 4;
		}
		cursor = InitEffectSequenceFromData(h, RenderOTAt(0x44), 2, cursor);
		FieldFree(0xB4);
	}

	// a flipbook frame lying on the ground under `pos` (MAG_001_CURE_DrawAdditiveGlow 0x8D6F90):
	// turned by `tilt` about X (0x400: flat), `height` over the ground (y up), frame `frame` of `seq`
	// (at most its last), in `colour`; packets at `cursor`
	inline void DrawGroundFlipbook(const int16_t *pos, bool hidden, uint32_t seq, int16_t tilt, int16_t height,
	                               int16_t frame, uint32_t colour, uint32_t &cursor)
	{
		if (hidden) return;
		Mat4x3 m;
		IdentityMatrix(&m);
		RotateX(&m, tilt);
		m.t[0] = pos[0];
		m.t[1] = -height;                        // y down: the game's 0 on the ground
		m.t[2] = pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint32_t>(h, 0) = seq;
		F<int16_t>(h, 4) = fxp::last_frame(seq, frame);
		F<uint32_t>(h, 0x1C) = colour;
		F<int16_t>(h, 0x24) = 4;
		cursor = InitEffectSequenceFromData(h, RenderOTAt(0x44), 2, cursor);
		FieldFree(0xB4);
	}

	// a glint of the effect library's glint task (Effect_Glint_Tick 0x8DCC20, fx_glint.cpp: it stays
	// where it is born, grows, shrinks, spins), drawing into the packet cursor at `cursor_address`:
	// its style, its spin side at random (MAG_001_CURE_Ring_SpawnGlints 0x8D7D50's loop body)
	static const uint32_t ORIG_Glint = 0x8DCC20;
	static const uint32_t GLINT_CURSOR = 0x2792E74;       // the effect library's glint packet cursor address
	inline void SpawnGlint(TaskQueue *q, void *parent, uint32_t cursor_address, const fxp::GlintStyle &style)
	{
		var<uint32_t>(GLINT_CURSOR) = cursor_address;
		uint8_t *g = (uint8_t *)AddEffectTask(q, ORIG_Glint, 0x5C, parent);
		F<int16_t>(g, 0x56) = style.ticks;
		g[0x4C] = style.centre_step[0];
		g[0x4D] = style.centre_step[1];
		g[0x4E] = style.centre_step[2];
		g[0x50] = style.tip_step[0];
		g[0x51] = style.tip_step[1];
		g[0x52] = style.tip_step[2];
		F<int16_t>(g, 0x3C) = style.grow.w;
		F<int16_t>(g, 0x3E) = style.grow.h;
		F<int16_t>(g, 0x40) = style.shrink.w;
		F<int16_t>(g, 0x42) = style.shrink.h;
		if (CrtRand() & 1) F<int16_t>(g, 0x58) = style.spin;
		else F<int16_t>(g, 0x58) = (int16_t)-style.spin;
	}
}
}
