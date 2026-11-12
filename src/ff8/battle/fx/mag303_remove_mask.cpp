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

// Effect 303: Remove mask (Sphinxaur c0m011 -> Sphinxara; MAG_303_*): the mask flies off in four pieces that burst into
// spinning shards, the unmasked form shines in (an environment-mapped redraw), the masked caster leaves the battle.
//
// Structure (setup MAG_303 0x66A820 -> _Init 0x66A850 (not ported), file loader 0x66A830 = the texture file named at
// 0x1034260 ("mag302.tim") -> [0x25075D0]; the setup queues the TIM, keeps the context (0x25075D4), the CASTER's entity
// (0x25075D8) and action 0's first target's entity (the unmasked form, 0x25075DC: moved onto the caster's position,
// QueueChainTransformation, animation 0xD), starts camera animation 0x10340AC and the root in its 1-node pool 0x2507630):
//   RootTask (0x66A920) - alternates the packet arena (magic buffer + 0x5688 / + 0x15688); on counter 1 sets up the
//     pools once (main 1 x 0x27C0 at + 0, layouts 4 x 0x138 at + 0x27C0, sparks 0x40 x 0x24 at + 0x2CA0, sprites
//     8 x 0x14 at + 0x35A0), the main task (voice slot 0x103411C, layout 0x1033114) and the stream state (+ 0x3640).
//     Then the main, layout, spark and sprite queues; once the main has ended it pumps the stream and, when that is
//     done, removes the caster's slot... (setMonsterPresence of the unmasked form's slot), clears its flag 4 and ends.
//   MainTask (0x66AAF0) - counters 0..0x28 (at 0x29 the stream load of file 0x168 into the packet cursor, end):
//     counter 0: the unmasked form's flag 4 off, the caster's on; sound 0x10340A8; four mask pieces (layout tasks:
//       velocities 0x1034230.., models 0x1031694.., scales 0x1388 / 0x1000 / 0x1000 / 0xBB8, at the caster);
//     every tick: the shards drawn (0x66B120) and moved / spun / aged (0x66B3A0, after the draws);
//     counters 0..0x15: the point P = the caster's point 0x1034250 (counter 0), layout 0x1033114 at P (depth -0x80);
//       counters 0 / 1: 0x20 sparks at P (CRT rand velocity +-0x100, acceleration velocity / -12);
//     counter 6: 8 sprites around the caster's point 0x1034258 (CRT rand);
//     counters 0x18..0x27: the unmasked form redrawn environment-mapped (0x66B490: its uv from the GTE IR1 / IR2 of
//       its vertices, scrolled by 2 c / 4 c, grey sin((c - 0x18) * 0x80) / 64, page (0x2C0, 0x100), clut 0x3DD4) with
//       its flag 4 set; counters 0x28..0x40 flag 4 off; the voice slot released at 0x25.
//   LayoutTask (0x66C220) - a mask piece: its prim model through the camera at its position (falling: velocity y + 10
//     a tick) for `life` ticks; then the piece shatters (0x66C3F0: its vertices through the frame into the packet
//     cursor area, 64 random spin matrices after them, one shard per polygon, module LCG 0x2507648) and its layout
//     0x10325F4 plays at the last position, scaled (until it ends).
//   SparkTask (0x66C100) - sprite sequence 0x1031440 (shadow-rotation camera 0xC00, -0x300), 10 frames, moving.
//   SpriteTask (0x66C080) - sprite sequence 0x1031550, 12 frames, still.
//   PrimCallback (0x66AE20) - one layout object (header 0x58, 0x7043B0): an optional scale of the block (offset and
//     matrix), flag 0x200: the block's point through the camera plus the offset, else (point + offset) through the
//     camera with the object composed with it.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update; the shards drawn first, moved last).
// ENTITY WRITES: the caster's flag byte (+0, bit 2) at counter 0, the unmasked form's flag word (bit 2) at counters 0,
// 0x18..0x27 (set), 0x28..0x40 and the end (cleared); the unmasked form's bone matrices are rebuilt from its pose by the
// redraw (0x508C90, as the engine has them). setMonsterPresence (engine) at the end.
// Module globals: 0x25075D0..0x2507660 (texture file 0x25075D0, context 0x25075D4, caster 0x25075D8, unmasked form
// 0x25075DC, the queues 0x25075E0 root / 0x25075F0 main / 0x2507600 layouts / 0x2507610 sparks / 0x2507620 sprites, root
// pool 0x2507630, packet cursor 0x2507644, LCG 0x2507648, magic buffer base 0x2507658).
// NEVER-WRITTEN STACK WORDS: the 4th word of the vectors loaded into GTE V0 (the callback's offset) reaches VZ0's high
// half only; the callback block's scale words are read only when its flag is set (layout tasks, which write them).

#include "mag_common.h"
#include <stddef.h>

namespace ff8fx
{
namespace mask303
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x25075D4); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x25075D8); }        // caster entity (setup)
	inline uint8_t *&Unmasked() { return var<uint8_t *>(0x25075DC); }      // action 0's first target's entity (setup)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507644); }
	inline uint32_t &Lcg() { return var<uint32_t>(0x2507648); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507658); }        // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QMain() { return (TaskQueue *)0x25075F0; }           // pool: magic buffer, 1 x 0x27C0
	inline TaskQueue *QLayouts() { return (TaskQueue *)0x2507600; }        // pool: + 0x27C0, 4 x 0x138
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2507610; }         // pool: + 0x2CA0, 0x40 x 0x24
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2507620; }        // pool: + 0x35A0, 8 x 0x14
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }          // g_Battle_FrameRenderListBase
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }    // the engine's frame packet cursor

	static const uint32_t ORIG_RootTask = 0x66A920;
	static const uint32_t ORIG_MainTask = 0x66AAF0;
	static const uint32_t ORIG_LayoutTask = 0x66C220;
	static const uint32_t ORIG_SparkTask = 0x66C100;
	static const uint32_t ORIG_SpriteTask = 0x66C080;
	static const uint32_t POOL_Layouts = 0x27C0, POOL_Sparks = 0x2CA0, POOL_Sprites = 0x35A0;
	static const uint32_t STREAM_STATE = 0x3640;
	static const uint32_t MORPH = 0x3688;                // magic buffer: blended vertex frames / the redraw's vertices
	static const uint32_t ARENA_A = 0x5688, ARENA_B = 0x15688;
	static const uint32_t MODEL_Main = 0x1033114;        // prim-model layouts
	static const uint32_t MODEL_Piece = 0x10325F4;
	static const uint32_t SEQ_Spark = 0x1031440, SEQ_Sprite = 0x1031550;
	static const void *const SOUND_Voice = (const void *)0x103411C;
	static const void *const SOUND_Start = (const void *)0x10340A8;
	static const int16_t *const POINT_Main = (const int16_t *)0x1034250;    // caster points (x, y, z, bone)
	static const int16_t *const POINT_Sprites = (const int16_t *)0x1034258;
	static const struct { uint32_t vel, model; int16_t scale; } PIECES[4] = {
		{ 0x1034230, 0x1031694, 0x1388 }, { 0x1034238, 0x1031E5C, 0x1000 }, { 0x1034240, 0x103218C, 0x1000 }, { 0x1034248, 0x10324BC, 0xBB8 } };
	static const int32_t REDRAW_Clut = 0x3DD4;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }   // Effect_RotationYMatrix
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }  // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, shift, cursor); } // MAG_070_RenderPrimModel
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); } // Stream_QueueLoad
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamDone(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); }
	inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
	inline void SetMonsterPresence(int32_t slot) { fn<void (__cdecl *)(int32_t)>(0x487670)(slot); }
	inline uint32_t DrawShadow(uint8_t *ent, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(ent, ot, a, cursor); } // BattleEntity_DrawShadow
	// software GTE
	inline void GteLoadV1(const void *v) { fn<void (__cdecl *)(const void *)>(0x45DFA0)(v); }
	inline void GteLoadV2(const void *v) { fn<void (__cdecl *)(const void *)>(0x45DFC0)(v); }
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); }   // GTE_LoadV0_u16 (zero-extended z)
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteReadSXY2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E260)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteSetSZ123(int32_t a, int32_t b, int32_t c) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x45E1D0)(a, b, c, d); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2507630)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct Shard // 0x80 in the main node, 0x4C bytes
	{
		int16_t pos[3];      // +0x00
		int16_t kind;        // +0x06 0 free, 1 triangle, 2 quad
		int16_t vel[3];      // +0x08 (y falls by 0x1E a tick, bounces at the floor)
		int16_t life;        // +0x0E
		uint32_t code;       // +0x10 colour + polygon code
		uint8_t spin[0x12];  // +0x14 3x3 rotation applied to the corners every tick
		uint8_t pad26[2];
		int16_t corner[4][3]; // +0x28 corners around pos
		uint32_t uv0;        // +0x40 uv 0 + clut
		uint32_t uv1;        // +0x44 uv 1 + page
		uint16_t uv2, uv3;   // +0x48
	};
	struct MainNode // pool of 1 node of 0x27C0 bytes (the magic buffer's start)
	{
		TaskNode hdr;
		int16_t counter;     // +0x0C
		int16_t pad0E;
		uint32_t voice;      // +0x10
		int16_t pt[4];       // +0x14 P: the caster's point (counter 0), 4th word 0
		Shard *cursor;       // +0x1C next shard to try
		Shard shards[0x80];  // +0x20
		uint8_t layout[0x1A0]; // +0x2620
	};
	struct LayoutNode // pool of 4 nodes of 0x138 bytes: a mask piece
	{
		TaskNode hdr;
		uint32_t model;      // +0x0C prim model (0x572200)
		int16_t pos[3];      // +0x10
		int16_t scale;       // +0x16 the layout's scale (all axes)
		int16_t vel[3];      // +0x18
		int16_t life;        // +0x1E ticks before it shatters
		uint8_t layout[0x118]; // +0x20
	};
	struct SparkNode // pool of 0x40 nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t pos[3];      // +0x0C
		int16_t frame;       // +0x12
		int16_t vel[3];      // +0x14
		int16_t pad1A;
		int16_t acc[3];      // +0x1C
		int16_t pad22;
	};
	struct SpriteNode // pool of 8 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t pos[3];      // +0x0C
		int16_t frame;       // +0x12
	};
	// the prim-player callback's parameter block (a stack block of the drawing task)
	struct PrimArg
	{
		int16_t pos[4];      // +0x00 the point (4th word: the GTE V0 pad)
		int32_t scale[3];    // +0x08 read when `scaled`
		int32_t scaled;      // +0x14
		int32_t depth;       // +0x18 the prim header's +0x18
		uint32_t morph;      // +0x1C blended vertex frames
	};
	// the shatter header (0x66C3F0, Field_Alloc 0x58)
	struct ShatterHdr
	{
		uint32_t model;      // +0x00 prim model
		MainNode *main;      // +0x04 the shard pool's owner
		int32_t shift;       // +0x08 velocity = (centre - model centre) >> shift
		uint8_t *verts;      // +0x0C transformed vertices (the packet cursor area), 8 bytes each
		uint8_t *list;       // +0x10 polygon list cursor
		int16_t v[4][4];     // +0x14 a polygon's vertices
		int16_t centre[4];   // +0x34 the model's box centre
		uint8_t *spins;      // +0x3C 64 spin matrices (0x20 each) after the vertices
		int16_t angles[4];   // +0x40
		int32_t scale[3];    // +0x48
		uint8_t pad54[4];
	};
	// the redraw's header (0x66B740 / 0x66BEC0: Field_Alloc 0xBC + 0x20)
	struct RedrawHdr
	{
		uint8_t *faces;      // +0x00
		uint8_t *vb;         // +0x04 projected vertices, 0xC bytes each
		uint16_t ntri, nquad; // +0x08
		int16_t clip[4];     // +0x0C x0, y0, x1, y1 (pixels)
		uint32_t colour;     // +0x14 the entity's colour word (+0x28)
		uint32_t shown;      // +0x18 the entity's object mask (+0x7C)
		uint8_t e[4][0xC];   // +0x1C vertex entries: sxy, otz (word), clip byte, u, v
		int32_t mac0;        // +0x4C
		uint8_t pad50[4];
		int32_t otz;         // +0x54
		uint8_t pad58[4];
		uint32_t rgb;        // +0x5C
		uint16_t clut, page; // +0x60
		int32_t voff, uoff;  // +0x64 / +0x68
		int32_t uwrap, vwrap; // +0x6C / +0x70
		uint32_t win_a[3];   // +0x74 draw-mode packet (texture window off)
		uint32_t win_b[3];   // +0x80
		uint8_t *list;       // +0x8C object data cursor
		uint8_t *vcur;       // +0x90 vertex buffer cursor
		uint32_t frame_cursor; // +0x94 the engine's frame packet cursor
		uint32_t cursor;     // +0x98 the module packet cursor
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(Shard) == 0x4C && sizeof(MainNode) == 0x27C0, "Remove mask nodes");
	static_assert(sizeof(LayoutNode) == 0x138 && sizeof(SparkNode) == 0x24 && sizeof(SpriteNode) == 0x14 && sizeof(PrimArg) == 0x20, "Remove mask nodes");
	static_assert(sizeof(ShatterHdr) == 0x58 && sizeof(RedrawHdr) == 0x9C && offsetof(MainNode, layout) == 0x2620, "Remove mask headers");
	static_assert(offsetof(RedrawHdr, mac0) == 0x4C && offsetof(RedrawHdr, rgb) == 0x5C && offsetof(RedrawHdr, win_a) == 0x74 && offsetof(RedrawHdr, cursor) == 0x98, "Remove mask redraw header");

	// Module LCG (0x66C7B0): seed = (seed * 125 + 14) & 0x7FFF
	static uint32_t Rand15()
	{
		Lcg() = (Lcg() * 125 + 0xE) & 0x7FFF;
		return Lcg();
	}

	// the spark acceleration (0x66AC11..0x66AC63): v * 0xD5555555 >> 33 rounded toward zero (v / -12)
	static int16_t AccelOf(int16_t v)
	{
		int32_t q = (int32_t)(((int64_t)v * (int32_t)0xD5555555) >> 32);
		q >>= 1;
		q += (int32_t)((uint32_t)q >> 31);
		return (int16_t)q;
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x66AE20): one object of a layout
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
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset, scaled by the block when it says so (4th word never written: the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (arg->scaled)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
			Scale3DMatrix(&m, arg->scale);
		}
		if (r->flags & 0x200)
		{
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t s[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, s);
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
		PacketCursor() = RenderPrim2(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
	}

	// 0x66C000: a point (x, y, z, bone) through the caster's bone (the bone matrices rebuilt from its pose first)
	static void BonePoint(const int16_t *v, int16_t *out)
	{
		uint8_t *c = Caster();
		const uint32_t bones = **(const uint32_t *const *)(c + 0x64) + 0x10;
		BuildBoneMatricesFromPose(c + 0x60);
		Mat4x3 m = {};
		ComposeAffineTransform((const Mat4x3 *)(Caster() + 0x40), (const Mat4x3 *)(bones + (uint32_t)(int32_t)v[3] * 0x30 + 0x10), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// ------------------------------------------------------------------
	// Shards
	// ------------------------------------------------------------------
	static inline bool Off16(int16_t v, uint16_t lim) { return (uint16_t)v > lim; }

	// 0x66B120: every live shard's corners around its position through the camera: a flat textured triangle (kind 1,
	// 0x20 bytes) or quad (kind 2, 0x28 bytes) on the frame OT
	static void ShardsDraw(Shard *shards)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		int16_t *v = (int16_t *)FieldAlloc(8);
		for (int k = 0; k < 0x80; k++)
		{
			const Shard *s = &shards[k];
			if (s->kind == 0) continue;
			uint8_t *P = (uint8_t *)PacketCursor();
			for (int j = 0; j < 3; j++) v[j] = (int16_t)(s->corner[0][j] + s->pos[j]);
			GteLoadV0(v);
			for (int j = 0; j < 3; j++) v[j] = (int16_t)(s->corner[1][j] + s->pos[j]);
			GteLoadV1(v);
			for (int j = 0; j < 3; j++) v[j] = (int16_t)(s->corner[2][j] + s->pos[j]);
			GteLoadV2(v);
			GteRTPT();
			*(uint32_t *)(P + 0xC) = s->uv0;
			*(uint32_t *)(P + 0x14) = s->uv1;
			*(uint16_t *)(P + 0x1C) = s->uv2;
			GteReadSXY012Split(P + 8, P + 0x10, P + 0x18);
			uint32_t clip = 0;
			if (Off16(*(int16_t *)(P + 8), 0xA00)) clip = 1;
			if (Off16(*(int16_t *)(P + 0x10), 0xA00)) clip |= 2;
			if (Off16(*(int16_t *)(P + 0x18), 0xA00)) clip |= 4;
			if (Off16(*(int16_t *)(P + 0xA), 0x6C0)) clip |= 0x10;
			if (Off16(*(int16_t *)(P + 0x12), 0x6C0)) clip |= 0x20;
			if (Off16(*(int16_t *)(P + 0x1A), 0x6C0)) clip |= 0x40;
			uint32_t next;
			if (s->kind == 2)
			{
				for (int j = 0; j < 3; j++) v[j] = (int16_t)(s->corner[3][j] + s->pos[j]);
				GteLoadV0(v);
				GteRTPS();
				*(uint16_t *)(P + 0x24) = s->uv3;
				GteReadSXY2(P + 0x20);
				GteAVSZ4();
				if (Off16(*(int16_t *)(P + 0x20), 0xA00)) clip |= 8;
				if (Off16(*(int16_t *)(P + 0x22), 0x6C0)) clip |= 0x80;
				if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
				GteReadOTZ32(v);
				if (v[0] <= 0) continue;
				*(uint32_t *)P = 0x9000000;
				*(uint32_t *)(P + 4) = s->code;
				next = PacketCursor() + 0x28;
			}
			else
			{
				GteAVSZ3();
				if ((clip & 7) == 7 || (clip & 0x70) == 0x70) continue;
				*(uint32_t *)P = 0x7000000;
				*(uint32_t *)(P + 4) = s->code;
				GteReadOTZ32(v);
				if (v[0] <= 0) continue;
				next = PacketCursor() + 0x20;
			}
			PacketCursor() = next;
			InsertPrimAutoDepth(OTBase() + (uint32_t)((int32_t)v[0] >> 2) * 4, P);
		}
		FieldFree(8);
	}

	// one shard's update (0x66B3A0's loop body): age (dead at 0: free, the cursor to it); corners spun by its matrix
	// (corners 1 / 3 loaded zero-extended, as the original); dead when corner 0 spun to nothing; moves, falls, bounces.
	// Returns false when it died.
	static bool ShardMove(Shard *s, Shard **cursor)
	{
		s->life--;
		if (s->life != 0)
		{
			GteSetRotMatrixCtrl((const Mat4x3 *)s->spin);
			GteLoadV0(s->corner[0]);
			GteMVMVA_RotV0();
			GteStoreIR123(s->corner[0]);
			GteLoadV0u(s->corner[1]);
			GteMVMVA_RotV0();
			if (!(*(const uint32_t *)s->corner[0] == 0 && s->corner[0][2] == 0))
			{
				GteStoreIR123(s->corner[1]);
				GteLoadV0(s->corner[2]);
				GteMVMVA_RotV0();
				GteStoreIR123(s->corner[2]);
				GteLoadV0u(s->corner[3]);
				GteMVMVA_RotV0();
				GteStoreIR123(s->corner[3]);
				s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
				s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
				s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
				s->vel[1] = (int16_t)(s->vel[1] + 0x1E);
				if (s->pos[1] >= 0) s->vel[1] = (int16_t)(-(int32_t)s->vel[1] >> 1);
				return true;
			}
		}
		s->kind = 0;
		*cursor = s;
		return false;
	}

	// 0x66B3A0: every live shard's update
	static void ShardsMove(MainNode *m)
	{
		for (int k = 0; k < 0x80; k++)
			if (m->shards[k].kind != 0) ShardMove(&m->shards[k], &m->cursor);
	}

	// 0x66C7D0: a free shard (the cursor's, else the first free one, slot 0 first), the cursor to the next slot; its
	// life 0xC + LCG & 7
	static Shard *AllocShard(MainNode *m)
	{
		Shard *s = m->cursor;
		if (s->kind != 0)
		{
			s = &m->shards[0];
			if (s->kind != 0)
			{
				int k = 1;
				for (; k < 0x80; k++)
					if (m->shards[k].kind == 0) break;
				if (k == 0x80) return nullptr;
				s = &m->shards[k];
			}
		}
		m->cursor = (uint8_t *)s >= (uint8_t *)&m->shards[0x7F] ? &m->shards[0] : s + 1;
		s->life = (int16_t)((Rand15() & 7) + 0xC);
		return s;
	}

	// a shard from the polygon's n vertices (0x66C5C0 / 0x66C830's loop body)
	static void MakeShard(ShatterHdr *h, const uint8_t *f, int n)
	{
		Shard *s = AllocShard(h->main);
		if (!s) return;
		s->code = *(const uint32_t *)f;
		for (int i = 0; i < n; i++) memcpy(h->v[i], h->verts + (uint32_t)*(const uint16_t *)(f + 4 + 2 * i) * 4, 8);
		int16_t c[3];
		for (int j = 0; j < 3; j++)
		{
			int32_t sum = 0;
			for (int i = 0; i < n; i++) sum += h->v[i][j];
			c[j] = (int16_t)(n == 3 ? sum / 3 : sum / 4);
		}
		s->pos[0] = c[0];
		s->pos[1] = c[1];
		s->pos[2] = c[2];
		s->kind = (int16_t)(n == 3 ? 1 : 2);
		for (int i = 0; i < n; i++)
			for (int j = 0; j < 3; j++) s->corner[i][j] = (int16_t)(h->v[i][j] - c[j]);
		for (int j = 0; j < 3; j++) s->vel[j] = (int16_t)(((int32_t)c[j] - h->centre[j]) >> (h->shift & 0x1F));
		s->uv0 = *(const uint32_t *)(f + 0xC);
		s->uv1 = *(const uint32_t *)(f + 0x10);
		if (n == 3) s->uv2 = *(const uint16_t *)(f + 0xA);
		else *(uint32_t *)&s->uv2 = *(const uint32_t *)(f + 0x14);
		memcpy(s->spin, h->spins + (Rand15() & 0x3F) * 0x20, 0x12);
	}

	// 0x66C5C0 (triangles, 0x14 bytes) / 0x66C830 (quads, 0x18 bytes): one shard per polygon of the list
	static void ShardList(ShatterHdr *h, int n, uint32_t stride)
	{
		int32_t count = *(const int32_t *)h->list;
		uint8_t *f = h->list + 4;
		for (; count > 0; count--, f += stride) MakeShard(h, f, n);
		h->list = f;
	}

	// 0x66C3F0: the model's vertices through the GTE frame (rotation + translation) into the packet cursor area, their box
	// centre, 64 random spin matrices after them (CRT rand: angles +-0x100, scale 0xF81 - 0..0xFF), its triangles and quads
	// as shards
	static void Shatter(ShatterHdr *h)
	{
		uint8_t *out = (uint8_t *)PacketCursor();
		int16_t mx = -0x7FFF, my = -0x7FFF, mz = -0x7FFF, nx = 0x7FFF, ny = 0x7FFF, nz = 0x7FFF;
		h->verts = out;
		const uint8_t *v = (const uint8_t *)h->model + 8;
		int32_t n = *(const int32_t *)(h->model + 4);
		do
		{
			GteLoadV0(v);
			GteMVMVA_RotV0Tr();
			v += 8;
			GteStoreIR123(h->v[0]);
			const int16_t x = h->v[0][0], y = h->v[0][1], z = h->v[0][2];
			if (x < nx) nx = x;
			if (x > mx) mx = x;
			if (y < ny) ny = y;
			if (y > my) my = y;
			if (z < nz) nz = z;
			if (z > mz) mz = z;
			memcpy(out, h->v[0], 8);
			out += 8;
		} while (--n);
		h->spins = out;
		h->centre[0] = (int16_t)(((int32_t)mx + nx) / 2);
		h->centre[1] = (int16_t)(((int32_t)my + ny) / 2);
		h->centre[2] = (int16_t)(((int32_t)mz + nz) / 2);
		for (int k = 0x40; k != 0; k--)
		{
			h->angles[0] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
			h->angles[1] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
			h->angles[2] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
			BuildRotationMatrixFromAngles(h->angles, (Mat4x3 *)out);
			const int32_t s = 0xF81 - (CrtRand() & 0xFF);
			h->scale[2] = s;
			h->scale[1] = s;
			h->scale[0] = s;
			Scale3DMatrix((Mat4x3 *)out, h->scale);
			out += 0x20;
		}
		h->list = (uint8_t *)h->model + *(const uint32_t *)h->model + 8;
		if (*(const uint32_t *)h->list != 0) ShardList(h, 3, 0x14);
		else h->list += 4;
		if (*(const uint32_t *)h->list != 0) ShardList(h, 4, 0x18);
	}

	// ------------------------------------------------------------------
	// The environment-mapped redraw of the unmasked form (0x66B490 -> 0x66B630 -> 0x66BEC0 / 0x66B740)
	// ------------------------------------------------------------------
	// 0x66BEC0: one bone group's vertices projected (zero-extended z) into 0xC-byte entries: sxy, OTZ (32 bits), screen
	// clip bits, u = IR1 / 8, v = IR2 / 8 (the rest of the entry: the header's bytes); a vertex at OTZ <= 0 only gets clip
	// byte 0x10 (the rest of its entry stays as the buffer held it)
	static void ProjectGroup(RedrawHdr *hh)
	{
		uint8_t *p = hh->list;
		uint8_t *vb = hh->vcur;
		int32_t n = *(const int16_t *)p;
		p += 2;
		for (; n != 0; n--)
		{
			GteLoadV0u(p);
			GteRTPS();
			p += 6;
			uint8_t *e = hh->e[0];
			GteReadSXY2(e);
			GteReadOTZ(e + 4); // GTE_ReadOTZ_SZ3div4 (32 bits)
			if (*(const int16_t *)(e + 4) <= 0)
			{
				vb[6] = 0x10;
			}
			else
			{
				const int32_t sx = *(const int16_t *)e, sy = *(const int16_t *)(e + 2);
				if (sx < shl32(hh->clip[0], 3)) e[6] |= 1;
				else if (sx >= shl32(hh->clip[2], 3)) e[6] |= 2;
				if (sy < shl32(hh->clip[1], 3)) e[6] |= 4;
				else if (sy >= shl32(hh->clip[3], 3)) e[6] |= 8;
				int32_t ir1, ir2;
				GteReadDataRegTo(&ir1, 9);
				GteReadDataRegTo(&ir2, 10);
				e[8] = (uint8_t)(ir2 >> 3);
				e[7] = (uint8_t)(ir1 >> 3);
				memcpy(vb, e, 0xC);
			}
			vb += 0xC;
		}
		hh->list = p;
		hh->vcur = vb;
	}

	// u or v of n entries (byte o of each): pulled down by 0x80 when they span more than 0x80, moved by off, less the wrap
	// byte when any passes 0xFF; written to the packet bytes q + 8 i
	static void EnvUV(const RedrawHdr *hh, int n, int o, int32_t off, uint8_t wrap, uint8_t *q)
	{
		int32_t c[4];
		for (int i = 0; i < n; i++) c[i] = hh->e[i][o];
		int32_t mn = c[0];
		for (int i = 1; i < n; i++)
			if (mn > c[i]) mn = c[i];
		int32_t span = 0;
		for (int i = 0; i < n; i++) span |= c[i] - mn;
		if (span > 0x80)
			for (int i = 0; i < n; i++) c[i] = (c[i] - 0x80) & 0xFF;
		int32_t any = 0;
		for (int i = 0; i < n; i++) any |= (c[i] = (int32_t)((uint32_t)c[i] + (uint32_t)off));
		for (int i = 0; i < n; i++) q[8 * i] = any > 0xFF ? (uint8_t)((uint8_t)c[i] - wrap) : (uint8_t)c[i];
	}

	// 0x66B740: the object's triangles (0x10 bytes) and quads (0x14 bytes): a polygon whose vertices are all in front and
	// not all outside one screen side, facing the camera: [texture window off] the environment-mapped copy (frame packet
	// cursor; semi-transparent, the header's grey, page and clut) [window back] the polygon with its own texture and the
	// entity's colour (module cursor)
	static void EnvPolys(RedrawHdr *hh, uint32_t ot, int32_t shift)
	{
		uint8_t *Q = (uint8_t *)hh->frame_cursor;
		const uint8_t *vb = hh->vb;
		uint8_t *f = hh->faces;
		for (uint32_t i = 0; i < hh->ntri; i++, f += 0x10)
		{
			for (int j = 0; j < 3; j++) memcpy(hh->e[j], vb + (uint32_t)(*(const uint16_t *)(f + 2 * j) & 0xFFF) * 0xC, 0xC);
			if ((uint8_t)(hh->e[0][6] | hh->e[1][6] | hh->e[2][6]) >= 0x10) continue;
			if (hh->e[2][6] & (uint8_t)(hh->e[0][6] & hh->e[1][6])) continue;
			GteSetSXY012(*(const uint32_t *)hh->e[0], *(const uint32_t *)hh->e[1], *(const uint32_t *)hh->e[2]);
			GteNCLIP();
			GteReadMAC0(&hh->mac0);
			if (hh->mac0 < 0) continue;
			GteSetSZ123(*(const int16_t *)(hh->e[0] + 4), *(const int16_t *)(hh->e[1] + 4), *(const int16_t *)(hh->e[2] + 4));
			GteAVSZ3();
			uint8_t *P1 = (uint8_t *)hh->cursor;
			hh->cursor += 0xC;
			memcpy(P1, hh->win_a, 0xC);
			GteReadOTZ32(&hh->otz);
			const uint32_t bucket = ot + (uint32_t)(hh->otz >> (shift & 0x1F)) * 4;
			InsertPrimAutoDepth(bucket, P1);
			*(uint32_t *)Q = 0x7000000;
			*(uint32_t *)(Q + 4) = hh->rgb | 0x26000000;
			*(uint32_t *)(Q + 8) = *(const uint32_t *)hh->e[0];
			*(uint32_t *)(Q + 0x10) = *(const uint32_t *)hh->e[1];
			*(uint32_t *)(Q + 0x18) = *(const uint32_t *)hh->e[2];
			EnvUV(hh, 3, 7, hh->uoff, (uint8_t)hh->uwrap, Q + 0xC);
			EnvUV(hh, 3, 8, hh->voff, (uint8_t)hh->vwrap, Q + 0xD);
			*(uint16_t *)(Q + 0xE) = hh->clut;
			*(uint16_t *)(Q + 0x16) = hh->page;
			InsertPrimAutoDepth(bucket, Q);
			uint8_t *P2 = (uint8_t *)hh->cursor;
			hh->cursor += 0xC;
			memcpy(P2, hh->win_b, 0xC);
			Q += 0x20;
			InsertPrimAutoDepth(bucket, P2);
			uint8_t *P3 = (uint8_t *)hh->cursor;
			hh->cursor += 0x20;
			*(uint32_t *)P3 = 0x7000000;
			*(uint32_t *)(P3 + 4) = hh->colour | 0x24000000;
			*(uint32_t *)(P3 + 8) = *(const uint32_t *)hh->e[0];
			*(uint32_t *)(P3 + 0x10) = *(const uint32_t *)hh->e[1];
			*(uint32_t *)(P3 + 0x18) = *(const uint32_t *)hh->e[2];
			*(uint32_t *)(P3 + 0xC) = *(const uint32_t *)(f + 8);
			*(uint32_t *)(P3 + 0x14) = *(const uint32_t *)(f + 0xC);
			*(uint16_t *)(P3 + 0x1C) = *(const uint16_t *)(f + 6);
			InsertPrimAutoDepth(bucket, P3);
		}
		for (uint32_t i = 0; i < hh->nquad; i++, f += 0x14)
		{
			for (int j = 0; j < 4; j++) memcpy(hh->e[j], vb + (uint32_t)(*(const uint16_t *)(f + 2 * j) & 0xFFF) * 0xC, 0xC);
			if ((uint8_t)(hh->e[0][6] | hh->e[1][6] | hh->e[2][6] | hh->e[3][6]) >= 0x10) continue;
			if (hh->e[3][6] & (uint8_t)(hh->e[0][6] & hh->e[1][6] & hh->e[2][6])) continue;
			GteSetSXY012(*(const uint32_t *)hh->e[0], *(const uint32_t *)hh->e[1], *(const uint32_t *)hh->e[2]);
			GteNCLIP();
			GteReadMAC0(&hh->mac0);
			if (hh->mac0 < 0) continue;
			GteSetSZ0123(*(const int16_t *)(hh->e[0] + 4), *(const int16_t *)(hh->e[1] + 4), *(const int16_t *)(hh->e[2] + 4), *(const int16_t *)(hh->e[3] + 4));
			GteAVSZ4();
			uint8_t *P1 = (uint8_t *)hh->cursor;
			memcpy(P1, hh->win_a, 0xC);
			GteReadOTZ32(&hh->otz);
			const uint32_t bucket = ot + (uint32_t)(hh->otz >> (shift & 0x1F)) * 4;
			InsertPrimAutoDepth(bucket, P1);
			hh->cursor += 0xC;
			*(uint32_t *)Q = 0x9000000;
			*(uint32_t *)(Q + 4) = hh->rgb | 0x2E000000;
			*(uint32_t *)(Q + 8) = *(const uint32_t *)hh->e[0];
			*(uint32_t *)(Q + 0x10) = *(const uint32_t *)hh->e[1];
			*(uint32_t *)(Q + 0x18) = *(const uint32_t *)hh->e[2];
			*(uint32_t *)(Q + 0x20) = *(const uint32_t *)hh->e[3];
			EnvUV(hh, 4, 7, hh->uoff, (uint8_t)hh->uwrap, Q + 0xC);
			EnvUV(hh, 4, 8, hh->voff, (uint8_t)hh->vwrap, Q + 0xD);
			*(uint16_t *)(Q + 0xE) = hh->clut;
			*(uint16_t *)(Q + 0x16) = hh->page;
			InsertPrimAutoDepth(bucket, Q);
			uint8_t *P2 = (uint8_t *)hh->cursor;
			memcpy(P2, hh->win_b, 0xC);
			Q += 0x28;
			InsertPrimAutoDepth(bucket, P2);
			uint8_t *P3 = (uint8_t *)hh->cursor + 0xC;
			hh->cursor = (uint32_t)P3 + 0x28;
			*(uint32_t *)P3 = 0x9000000;
			*(uint32_t *)(P3 + 4) = hh->colour | 0x2C000000;
			*(uint32_t *)(P3 + 8) = *(const uint32_t *)hh->e[0];
			*(uint32_t *)(P3 + 0x10) = *(const uint32_t *)hh->e[1];
			*(uint32_t *)(P3 + 0x18) = *(const uint32_t *)hh->e[2];
			*(uint32_t *)(P3 + 0x20) = *(const uint32_t *)hh->e[3];
			*(uint32_t *)(P3 + 0xC) = *(const uint32_t *)(f + 8);
			*(uint32_t *)(P3 + 0x14) = *(const uint32_t *)(f + 0xC);
			*(uint16_t *)(P3 + 0x1C) = *(const uint16_t *)(f + 0x10);
			*(uint16_t *)(P3 + 0x24) = *(const uint16_t *)(f + 0x12);
			InsertPrimAutoDepth(bucket, P3);
		}
		hh->frame_cursor = (uint32_t)Q;
	}

	// 0x66B630: every shown object of the model: its bone groups projected, then its polygons
	static void EnvModel(const uint32_t *skel, RedrawHdr *hh, uint32_t ot, int32_t shift)
	{
		const uint8_t *bones = (const uint8_t *)skel[0] + 0x10;
		const uint32_t *objs = (const uint32_t *)skel[1];
		const int32_t n = (int32_t)*objs++;
		for (int32_t i = 0; i < n; i++)
		{
			uint8_t *o = (uint8_t *)(*objs++ + skel[1]);
			hh->list = o;
			if (!(hh->shown & shl32(1, i & 0x1F))) continue;
			hh->vcur = hh->vb;
			int32_t groups = *(const int16_t *)hh->list;
			hh->list += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = *(const int16_t *)hh->list;
				hh->list += 2;
				const Mat4x3 *m = (const Mat4x3 *)(bones + (uint32_t)bone * 0x30 + 0x10);
				GteSetRotMatrixCtrl(m);
				GteSetTransVectorCtrl(m);
				ProjectGroup(hh);
			}
			uint8_t *p = (uint8_t *)(((uint32_t)hh->list + 3) & ~3u);
			hh->list = p;
			hh->ntri = *(const uint16_t *)p;
			p += 2;
			hh->list = p;
			hh->nquad = *(const uint16_t *)p;
			p += 0xA;
			hh->list = p;
			hh->faces = p;
			EnvPolys(hh, ot, shift);
		}
	}

	// 0x66B490: the entity redrawn environment-mapped: its shadow, its bones from its matrix through the camera, the
	// header (clip rect 320 x 216, the entity's colour and object mask, page from `rect`'s x / y, clut, u / v offsets
	// (low 7 bits), wraps 0x80, the grey), the model, the bone matrices rebuilt from the pose
	static void Redraw(uint8_t *ent, const int16_t *rect, int32_t clut, int32_t uoff, int32_t voff, const uint8_t *rgb, uint32_t vb)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xBC);
		FrameCursor() = DrawShadow(ent, OTBase() + 0x4040, 0x10, FrameCursor());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(ent + 0x40), (Mat4x3 *)h);
		ComputeBonesWorldMatrices(ent + 0x60, h);
		RedrawHdr *hh = (RedrawHdr *)(h + 0x20);
		hh->vb = (uint8_t *)vb;
		hh->clip[0] = 0;
		hh->clip[1] = 0;
		hh->clip[2] = 0x140;
		hh->clip[3] = 0xD8;
		hh->colour = *(const uint32_t *)(ent + 0x28);
		hh->shown = *(const uint32_t *)(ent + 0x7C);
		hh->win_a[0] = 0x2000000;
		hh->win_a[1] = 0xE2000000;
		hh->win_b[0] = 0x2000000;
		hh->win_b[1] = 0xE2000210;
		hh->uoff = uoff & 0x7F;
		hh->voff = voff & 0x7F;
		hh->win_a[2] = 0;
		hh->win_b[2] = 0;
		hh->vwrap = 0x80;
		hh->uwrap = 0x80;
		const int32_t y = rect[1];
		uint16_t x = (uint16_t)((rect[0] & 0x3C0) | 0x2800);
		hh->page = (uint16_t)((uint16_t)((int16_t)x >> 6) | ((y >> 4) & 0x10) | ((y & 0x200) << 2));
		hh->clut = (uint16_t)clut;
		hh->rgb = *(const uint32_t *)rgb;
		hh->frame_cursor = FrameCursor();
		hh->cursor = PacketCursor();
		EnvModel(*(const uint32_t *const *)(ent + 0x64), hh, OTBase() + 0x44, 0);
		FrameCursor() = hh->frame_cursor;
		PacketCursor() = hh->cursor;
		BuildBoneMatricesFromPose(ent + 0x60);
		FieldFree(0xBC);
	}

	// the main's redraw of counter c (0x66AC7F..0x66ACE3): rect (0x2C0, 0x100, 0x80, 0x80), grey sin((c - 0x18) * 0x80) / 64
	static void MainRedraw(int32_t c, uint32_t vb)
	{
		const int16_t rect[4] = { 0x2C0, 0x100, 0x80, 0x80 };
		const int32_t g = ComputeSin(shl32((c - 0x18) & 0x1FFFFF, 7)) >> 6;
		const uint8_t rgb[4] = { (uint8_t)g, (uint8_t)g, (uint8_t)g, 0 };
		Redraw(Unmasked(), rect, REDRAW_Clut, c * 2, c * 4, rgb, vb);
	}

	// the sprite tasks' draw (0x66C080 / 0x66C100)
	static void SpriteDraw(const int16_t *pos, int16_t frame, uint32_t seq)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(pos, 0xC00, -0x300);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0xB4);
	}

	// a mask piece's frame: no rotation but the caster's turn (-its +0x0E), at pos
	static void PieceFrame(const int16_t *pos, Mat4x3 *m)
	{
		RotY(-*(const int16_t *)(Caster() + 0xE), m);
		m->t[0] = pos[0];
		m->t[1] = pos[1];
		m->t[2] = pos[2];
	}

	// a mask piece's prim model (0x66C234..0x66C2C0): its frame composed with the camera
	static void PieceDraw(uint32_t model, const int16_t *pos)
	{
		uint8_t *h = AllocHeader(0x58);
		Mat4x3 m = {};
		PieceFrame(pos, &m);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint32_t *)h = model;
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
	}

	// a mask piece's layout parameter block (0x66C37F..0x66C3C3): at its position, scaled
	static void PieceArg(const LayoutNode *L, PrimArg *a)
	{
		memcpy(a->pos, L->pos, 8);
		a->scale[0] = a->scale[1] = a->scale[2] = L->scale;
		a->scaled = 1;
		a->depth = 0;
		a->morph = TexBase() + MORPH;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag303_remove_mask_held.h"
#endif

namespace ff8fx
{
namespace mask303
{
	// ------------------------------------------------------------------
	// Sprite (0x66C080) / spark (0x66C100)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SpriteTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag303_remove_mask_held.inc
		FX_HELD(held_note(SPRITE, n);)
		SpriteDraw(s->pos, s->frame, SEQ_Sprite);
		s->frame++;
		return s->frame < 0xC ? 0 : TASK_END;
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		FX_HELD(held_note(SPARK, n);)
		SpriteDraw(s->pos, s->frame, SEQ_Spark);
		s->frame++;
		if (s->frame >= 0xA) return TASK_END;
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->vel[0] = (int16_t)(s->acc[0] + s->vel[0]);
		s->vel[1] = (int16_t)(s->acc[1] + s->vel[1]);
		s->vel[2] = (int16_t)(s->acc[2] + s->vel[2]);
		return 0;
	}

	// ------------------------------------------------------------------
	// Mask piece (0x66C220)
	// ------------------------------------------------------------------
	static uint32_t __cdecl LayoutTask(TaskNode *n)
	{
		LayoutNode *L = (LayoutNode *)n;
		if (L->life != 0)
		{
			FX_HELD(held_note(PIECE, n);)
			PieceDraw(L->model, L->pos);
			L->pos[1] = (int16_t)(L->pos[1] + L->vel[1]);
			L->pos[0] = (int16_t)(L->pos[0] + L->vel[0]);
			L->pos[2] = (int16_t)(L->pos[2] + L->vel[2]);
			L->vel[1] = (int16_t)(L->vel[1] + 0xA);
			L->life--;
			if (L->life > 0) return 0;
			Mat4x3 m = {};
			PieceFrame(L->pos, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			ShatterHdr *h = (ShatterHdr *)FieldAlloc(0x58);
			h->model = L->model;
			h->main = (MainNode *)TexBase();
			h->shift = 2;
			Shatter(h);
			L->pos[0] = h->centre[0];
			L->pos[1] = h->centre[1];
			L->pos[2] = h->centre[2];
			FieldFree(0x58);
		}
		PrimArg a;
		PieceArg(L, &a);
		FX_HELD(held_note(PIECE_PLAY, n);)
		return prim::play((prim::Layout *)L->layout, PrimCallback, (int)&a, 0) != 0 ? 0 : TASK_END;
	}

	// 0x66C1C0: a mask piece at the caster's position: velocity, life (+6 word of vel), model, scale
	static void SpawnPiece(const int16_t *vel, uint32_t model, int16_t scale)
	{
		LayoutNode *L = (LayoutNode *)AddTaskToQueue(QLayouts(), ORIG_LayoutTask);
		DecodeModelPrimLayout(MODEL_Piece, L->layout, 0x118);
		L->model = model;
		memcpy(L->pos, Caster() + 0x1C, 8);
		L->scale = scale;
		memcpy(L->vel, vel, 8);
	}

	// ------------------------------------------------------------------
	// Main (0x66AAF0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		if (m->counter >= 0x29)
		{
			StreamLoad(0x168, PacketCursor(), 1);
			return TASK_END;
		}
		if (m->counter == 0)
		{
			*(uint16_t *)Unmasked() &= 0xFFFB;
			*Caster() |= 4;
		}
		// 30 fps layer: see mag303_remove_mask_held.inc
		FX_HELD(held_note(SHARDS, n);)
		ShardsDraw(m->shards);
		const int32_t c = m->counter;
		if ((uint32_t)c < 0x16)
		{
			if (c == 0)
			{
				BonePoint(POINT_Main, m->pt);
				m->pt[3] = 0;
			}
			PrimArg a;
			memcpy(a.pos, m->pt, 8);
			a.scaled = 0;
			a.depth = -0x80;
			a.morph = TexBase() + MORPH;
			FX_HELD(held_note(MAIN_PLAY, n);)
			prim::play((prim::Layout *)m->layout, PrimCallback, (int)&a, 0);
			if ((uint32_t)c < 2)
			{
				for (int k = 0x20; k != 0; k--)
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
					if (!s) continue;
					memcpy(s->pos, m->pt, 8); // pos + frame (the point's 4th word, 0)
					s->vel[0] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
					s->vel[1] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
					s->vel[2] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
					s->acc[0] = AccelOf(s->vel[0]);
					s->acc[1] = AccelOf(s->vel[1]);
					s->acc[2] = AccelOf(s->vel[2]);
				}
			}
		}
		const uint32_t e = (uint32_t)(m->counter - 0x18);
		if (e < 0x10)
		{
			FX_HELD(held_note(REDRAW, n);)
			MainRedraw(m->counter, TexBase() + MORPH);
			*Unmasked() |= 4;
		}
		else if (e < 0x29) *(uint16_t *)Unmasked() &= 0xFFFB;
		ShardsMove(m);
		if (m->counter == 0) BdPlaySE(SOUND_Start, 0, 0x80);
		else if (m->counter == 0x25) ReleaseVoiceSlot(m->voice);
		if (m->counter == 6)
		{
			int16_t p[4];
			BonePoint(POINT_Sprites, p);
			for (int k = 8; k != 0; k--)
			{
				SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
				if (!s) continue;
				s->pos[0] = (int16_t)((CrtRand() & 0x1FF) + p[0] - 0x100);
				s->pos[1] = (int16_t)(0 - (CrtRand() & 0x1FF));
				const int32_t rz = CrtRand() & 0x1FF;
				s->frame = 0;
				s->pos[2] = (int16_t)(rz + p[2] - 0x100);
			}
		}
		if (m->counter == 0)
			for (const auto &pc : PIECES) SpawnPiece((const int16_t *)pc.vel, pc.model, pc.scale);
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root (0x66A920)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		if (r->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->arena = 1;
		}
		// 30 fps layer: see mag303_remove_mask_held.inc
		FX_HELD(held_note_root();)
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			InitTaskQueuePool(QMain(), (void *)TexBase(), 0x27C0, 1);
			InitTaskQueuePool(QLayouts(), (void *)(TexBase() + POOL_Layouts), 0x138, 4);
			InitTaskQueuePool(QSparks(), (void *)(TexBase() + POOL_Sparks), 0x24, 0x40);
			InitTaskQueuePool(QSprites(), (void *)(TexBase() + POOL_Sprites), 0x14, 8);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&m->counter, 0, 0x9ED);
			m->voice = ClaimVoiceSlot(SOUND_Voice, 1, 0x80);
			m->cursor = m->shards;
			DecodeModelPrimLayout(MODEL_Main, m->layout, 0x1A0);
			StreamStateInit((void *)(TexBase() + STREAM_STATE));
		}
		if (r->started)
		{
			const int a0 = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QLayouts());
			ExecuteTaskQueue(QSparks());
			ExecuteTaskQueue(QSprites());
			if (a0 == 0)
			{
				StreamPump();
				if (StreamDone(a0) == 0) return 0;
				SetMonsterPresence((int32_t)(Unmasked() - Entity(0)) / 0x9C);
				*(uint16_t *)Unmasked() &= 0xFFFB;
				return TASK_END;
			}
		}
		r->counter++;
		return 0;
	}
}

	void register_mag303_remove_mask()
	{
		register_port(mask303::ORIG_RootTask, (void *)mask303::RootTask, "303 RootTask", 303);
		register_port(mask303::ORIG_MainTask, (void *)mask303::MainTask, "303 MainTask", 303);
		register_port(mask303::ORIG_LayoutTask, (void *)mask303::LayoutTask, "303 LayoutTask", 303);
		register_port(mask303::ORIG_SparkTask, (void *)mask303::SparkTask, "303 SparkTask", 303);
		register_port(mask303::ORIG_SpriteTask, (void *)mask303::SpriteTask, "303 SpriteTask", 303);
		// 30 fps layer: see mag303_remove_mask_held.inc
		FX_HELD(register_mag303_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag303_remove_mask_held.inc"
#endif
