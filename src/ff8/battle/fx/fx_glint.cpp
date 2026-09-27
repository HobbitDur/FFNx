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

// Shared effect-library task Effect_Glint_Tick (0x8DCC20), outside every spell module's code: the
// heal / support spells built on the effect library (0x8DC530..: Cure 1, Life, ...) spawn it with
// Effect_AddTaskAndInitFromCtx (0x8DC540) and fill its parameters; it is ported here once and
// registered by this file only.
//
// A glint is a streak of light on the spawner's line anchor: a gouraud triangle (centre colour A,
// tips colour B) and a gouraud quad (colour B fading to black), additive, turned by three random
// angles (the Y angle spins by a fixed step per tick). Every tick it first moves to the midpoint of
// its line anchor (Effect_CopyMidpointFromSource: the line's emitter +0x38) at the height of the
// target's bounds centre (Effect_GetTargetBoundsCenterWorldY), then runs its phase:
//   0 - random angles (rand() y, x, z), size and colours zero;
//   1 - size and colours grow by their per-tick steps for `steps` ticks;
//   2 - size and colours shrink for `steps` ticks, then the glint is done and hidden.
// Then the Y angle turns and the glint draws (unless hidden) into the packet cursor whose address
// the spawner stored in 0x2792E74 (Cure: its module cursor 0x277AEC4), on the render list + 0x44.
// It does not test the draw-only flags (battle_to_update_flags 0x201).

#include "mag_common.h"

namespace ff8fx
{
namespace glint
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
		int16_t pos[4];        // +0x1C x, y, z
		int16_t counter;       // +0x24
		uint16_t flags;        // +0x26 bit0 done, bit2 hidden
		uint8_t children;      // +0x28
		int8_t phase;          // +0x29
		int8_t action;         // +0x2A
		int8_t target;         // +0x2B
		uint8_t attacker;      // +0x2C
		uint8_t slot;          // +0x2D target entity slot
		uint8_t pad2E[2];
	};
	struct GlintNode // 0x5C bytes
	{
		FxNode h;
		int16_t rot[3];             // +0x30 x, y, z angles (y spins by `spin` per tick)
		int16_t pad36;
		int16_t w, hgt;             // +0x38 size
		int16_t grow_w, grow_h;     // +0x3C per tick in phase 1
		int16_t shrink_w, shrink_h; // +0x40 per tick in phase 2
		uint8_t rgb_a[3];           // +0x44 centre colour
		uint8_t pad47;
		uint8_t rgb_b[3];           // +0x48 tip colour
		uint8_t pad4B;
		uint8_t d_a[3];             // +0x4C colour steps (bytes, wrapping)
		uint8_t pad4F;
		uint8_t d_b[3];             // +0x50
		uint8_t pad53;
		int16_t left;               // +0x54 ticks left in the phase
		int16_t steps;              // +0x56 ticks per phase
		int16_t spin;               // +0x58 Y angle step
		int16_t pad5A;
	};
#pragma pack(pop)
	static_assert(sizeof(FxNode) == 0x30 && sizeof(GlintNode) == 0x5C, "glint node");

	static const uint32_t ORIG_Glint = 0x8DCC20;
	// address of the spawner's packet cursor (a uint32 cursor)
	inline uint32_t *&CursorPtr() { return var<uint32_t *>(0x2792E74); }
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	// effect library (original addresses)
	inline void ReleaseLinkedTask(void *n) { fn<void (__cdecl *)(void *)>(0x8DC530)(n); }
	inline void CopyMidpointFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC700)(n, out); }
	inline int32_t BoundsCenterY(void *n) { return fn<int32_t (__cdecl *)(void *)>(0x8DCB80)(n); }  // Effect_GetTargetBoundsCenterWorldY
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	// software GTE / drawing
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); } // GTE_ReadSXY012_Split
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }       // GTE_ReadOTZ (dword)
	inline uint32_t GteSZ(int k) { return var<uint32_t>(0x1CA8A50 + 4 * k); }                   // SZ0..SZ3 data registers
	inline void InsertPrimDepthKeys(uint32_t ot, void *prim, uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, void *, uint32_t, uint32_t, uint32_t, uint32_t)>(0x45C870)(ot, prim, a, b, c, d); }
	inline uint32_t GetTPage(int32_t tp, int32_t abr, int32_t x, int32_t y) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x45C690)(tp, abr, x, y); }
	inline void SetDrawMode(void *p, int32_t dfe, int32_t dtd, uint32_t tpage, const void *tw) { fn<void (__cdecl *)(void *, int32_t, int32_t, uint32_t, const void *)>(0x45BFC0)(p, dfe, dtd, tpage, tw); }

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }

	// phases 1 and 2: one tick of growth / fade (pure: also the held frame's next-tick prediction)
	static void Step(GlintNode *g)
	{
		if (g->h.phase == 1)
		{
			g->w = (int16_t)(g->w + g->grow_w);
			g->hgt = (int16_t)(g->hgt + g->grow_h);
			for (int k = 0; k < 3; k++) g->rgb_a[k] = (uint8_t)(g->rgb_a[k] + g->d_a[k]);
			for (int k = 0; k < 3; k++) g->rgb_b[k] = (uint8_t)(g->rgb_b[k] + g->d_b[k]);
			g->left--;
			if (g->left <= 0)
			{
				g->left = g->steps;
				g->h.phase++;
			}
		}
		else if (g->h.phase == 2)
		{
			g->w = (int16_t)(g->w - g->shrink_w);
			g->hgt = (int16_t)(g->hgt - g->shrink_h);
			for (int k = 0; k < 3; k++) g->rgb_a[k] = (uint8_t)(g->rgb_a[k] - g->d_a[k]);
			for (int k = 0; k < 3; k++) g->rgb_b[k] = (uint8_t)(g->rgb_b[k] - g->d_b[k]);
			g->left--;
			if (g->left <= 0)
			{
				*(uint8_t *)&g->h.flags |= 5;
				g->h.phase++;
			}
		}
	}

	// the streak: v0 = centre, v1 / v2 = (w/2, +-h/2), v3 / v4 = (w, +-h) in the glint's plane,
	// turned by its angles, at its position, through the camera
	static void Draw(const GlintNode *g, uint32_t ot, uint32_t *cursor)
	{
		uint8_t *p = (uint8_t *)*cursor;
		Mat4x3 m;
		ComposeZYXRotationMatrix(g->rot, &m);
		const int16_t w = g->w, h = g->hgt;
		const int16_t hw = (int16_t)(w / 2), hh = (int16_t)(h / 2);
		// SVECTORs (4th words never written: only loaded into the GTE VZ pads)
		int16_t in[5][4];
		in[0][0] = 0;
		in[0][1] = 0;
		in[0][2] = 0;
		in[1][0] = hw;
		in[1][1] = hh;
		in[1][2] = 0;
		in[2][0] = hw;
		in[2][1] = (int16_t)-hh;
		in[2][2] = 0;
		in[3][0] = w;
		in[3][1] = h;
		in[3][2] = 0;
		in[4][0] = w;
		in[4][1] = (int16_t)-h;
		in[4][2] = 0;
		int16_t out[5][4];
		for (int k = 0; k < 5; k++)
		{
			MatrixMulVector(&m, in[k], out[k]);
			out[k][0] = (int16_t)(out[k][0] + g->h.pos[0]);
			out[k][1] = (int16_t)(out[k][1] + g->h.pos[1]);
			out[k][2] = (int16_t)(out[k][2] + g->h.pos[2]);
		}
		GteSetTransVectorCtrl(&Camera());
		GteSetRotMatrixCtrl(&Camera());
		uint32_t sxy[5];
		GteLoadV012(out[0], out[1], out[2]);
		GteRTPT();
		GteReadSXY012(&sxy[0], &sxy[1], &sxy[2]);
		const uint32_t z0 = GteSZ(1), z1 = GteSZ(2);
		GteAVSZ3();
		int32_t otz;
		GteReadOTZWord(&otz);
		otz >>= 2;
		GteLoadV012(out[2], out[3], out[4]);
		GteRTPT();
		GteReadSXY012(&sxy[2], &sxy[3], &sxy[4]);
		const uint32_t z2 = GteSZ(1), z3 = GteSZ(2), z4 = GteSZ(3);
		// gouraud triangle: centre colour A, v1 / v2 colour B
		p[4] = g->rgb_a[0];
		p[5] = g->rgb_a[1];
		p[6] = g->rgb_a[2];
		F<uint32_t>(p, 0) = 0x6000000;
		p[7] = 0x32;
		p[0xC] = g->rgb_b[0];
		p[0xD] = g->rgb_b[1];
		p[0xE] = g->rgb_b[2];
		p[0x14] = g->rgb_b[0];
		p[0x15] = g->rgb_b[1];
		p[0x16] = g->rgb_b[2];
		F<uint32_t>(p, 8) = sxy[0];
		F<uint32_t>(p, 0x10) = sxy[1];
		F<uint32_t>(p, 0x18) = sxy[2];
		const uint32_t bucket = ot + (uint32_t)otz * 4;
		InsertPrimDepthKeys(bucket, p, z0, z1, z2, 0);
		p += 0x1C;
		// gouraud quad: v1 / v2 colour B, v3 / v4 black
		F<uint32_t>(p, 0) = 0x8000000;
		p[7] = 0x3A;
		p[4] = g->rgb_b[0];
		p[5] = g->rgb_b[1];
		p[6] = g->rgb_b[2];
		p[0xC] = g->rgb_b[0];
		p[0xD] = g->rgb_b[1];
		p[0xE] = g->rgb_b[2];
		p[0x1E] = 0;
		p[0x1D] = 0;
		p[0x1C] = 0;
		p[0x16] = 0;
		p[0x15] = 0;
		p[0x14] = 0;
		F<uint32_t>(p, 8) = sxy[1];
		F<uint32_t>(p, 0x10) = sxy[2];
		F<uint32_t>(p, 0x18) = sxy[3];
		F<uint32_t>(p, 0x20) = sxy[4];
		InsertPrimDepthKeys(bucket, p, z1, z2, z3, z4);
		p += 0x24;
		// additive draw mode (GetTPage(0, 1, 0x280, 0), SetDrawMode(dfe 0, dtd 0, no window))
		const uint32_t tpage = GetTPage(0, 1, 0x280, 0) & 0xFFFF;
		SetDrawMode(p, 0, 0, tpage, nullptr);
		InsertPrimAutoDepth(bucket, p);
		p += 0xC;
		*cursor = (uint32_t)p;
	}
}
}

#ifdef FF8_FX_HELD
#include "fx_glint_held.h"
#endif

namespace ff8fx
{
namespace glint
{
	// ------------------------------------------------------------------
	// Effect_Glint_Tick (0x8DCC20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl GlintTask(TaskNode *n)
	{
		GlintNode *g = (GlintNode *)n;
		const uint32_t ot = RenderOT(0x44);
		CopyMidpointFromSource(g, g->h.pos);
		g->h.pos[1] = (int16_t)BoundsCenterY(g);
		if (g->h.phase == 0)
		{
			g->left = g->steps;
			g->w = 0;
			g->hgt = 0;
			g->rot[1] = (int16_t)(CrtRand() & 0xFFF);
			g->rot[0] = (int16_t)(CrtRand() & 0xFFF);
			g->rot[2] = (int16_t)(CrtRand() & 0xFFF);
			g->rgb_b[2] = 0;
			g->rgb_b[1] = 0;
			g->rgb_b[0] = 0;
			g->rgb_a[2] = 0;
			g->rgb_a[1] = 0;
			g->rgb_a[0] = 0;
			g->h.phase++;
		}
		else Step(g);
		g->rot[1] = (int16_t)((uint16_t)(g->spin + g->rot[1]) & 0xFFF);
		if (!(g->h.flags & 4))
		{
			// 30 fps layer: see fx_glint_held.inc
			FX_HELD(held_note_draw(g, CursorPtr());)
			Draw(g, ot, CursorPtr());
		}
		g->h.counter++;
		if ((g->h.flags & 1) && g->h.children == 0)
		{
			ReleaseLinkedTask(g);
			return TASK_END;
		}
		return 0;
	}
}

	void register_fx_glint()
	{
		register_port(glint::ORIG_Glint, (void *)glint::GlintTask, "shared 8DCC20 Effect_Glint_Tick", 1);
		// 30 fps layer: see fx_glint_held.inc
		FX_HELD(register_fx_glint_held();)
	}
}

#ifdef FF8_FX_HELD
#include "fx_glint_held.inc"
#endif
