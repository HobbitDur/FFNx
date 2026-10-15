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

// Effect 311: Gravija (enemy attack 257 of kernel.bin, Diablos c0m066 - the Magical Lamp boss;
// MAG_311_*). No other kernel.bin entry uses effect 311 (Omega / Ultima Weapon's Gravija, attack
// 309, is effect 214: a near-copy of this module at 0x69D5B0).
//
// Structure (setup 0x6652C0 -> _Init 0x6652F0, file loader 0x6652D0 = the texture file named at
// 0x10231C8; the _Init copies the module's data block 0x101B0E8 (0x1DD0 bytes: the warp mesh -
// size, 241 vertices, 24 triangles, 216 quads) into 0x25055EC, keeps the caster's entity
// (0x25055E0, not read again) and the cast context (0x25073BC), starts camera animation 0x1020D20
// and queues the TIM):
//   MasterTask (0x665380) - alternates the packet arena (magic buffer + 0x1994 / + 0x11994); on
//     counter 1 prepares the mesh copy (0x666320: every vertex's packed word = its length and
//     direction (0x666380, Sonic Wave's 0x6A9C80 on this module's tables), then every triangle
//     (Sonic Wave's 0x6A9DF0) and every quad (0x6A60D0 of module 207) turned to start at its
//     leftmost vertex, indexed for 12-byte vertices), sets up the pools (particles 0x40 x 0x1C at
//     + 0, main 1 x 0x348 at + 0x700) and starts the main task (layout 0x101CEB8, the targets of
//     action 0 and their average default effect position). Then the main and particle queues; ends
//     when the main queue is empty.
//   MainTask (0x665550) - every tick the screen flash (0x5713E0, colour 0x808080): counter x 375
//     up to 8, 3000, (0x4A - counter) x 375 from 0x43. Counters 0..0x49: the layout 0x101CEB8 at
//     the targets' average point, depth -0x80. Counters 0x12..0x49 (b = counter - 0x12): the
//     SCREEN WARP - the screen box of every target's head and feet (0x6661D0, with the box helper
//     0x6662C0), centred on the box, scale = the box (at least 0x18000 / OTZ of the average point;
//     times b / 8 below b = 8), into the module's warp frame 0x2507408; the mesh drawn by 0x665D50 at
//     OT (the nearest OTZ - 0x80) / 4 + 0x11 with the fade 0xC00 (from b = 0x30: 0xC40 + 0x80 per
//     tick). Sound at counter 0. Counter 0x12: 0x40 particle tasks (CRT rand: life 0x32 - (r & 7),
//     angle, 3 rotation angles, reach 0x5DC + (r & 0x3FF)). The particle amplitude (+0x0E):
//     sin(b x 0x40) up to b = 0x10, 0x1000 to 0x22, cos((b - 0x22) x 0x40) to 0x31. Counter 0x43
//     (b = 0x31): the action's results applied to its targets (0x506BA0). Counter 0x4A: restores the
//     battle textures (0x508630, arena A); ends when that is done (screen flash 0 every tick).
//   Warp (0x665D50) - every vertex (x, y) through the warp frame (GTE MVMVA, the vertex's z = the
//     VZ0 the GTE holds), into magic buffer + 0xE48 (12 bytes: the PREVIOUS vertex's IR1 / IR2 (the
//     first one: the IRs before the loop) and its out-of-screen flags, OR-ed into the word when
//     outside; this vertex's IR1 / IR2 >> 3 clamped to the screen); a draw mode packet, the
//     triangles (0x665F50, Sonic Wave's 0x6A97C0) and the quads (0x666070) as semi-transparent
//     Gouraud polygons at those positions, every vertex colour depth-cued (GTE DPCS, IR0 = the fade,
//     far colour 0), a draw mode packet. The warp's 5th / 6th arguments (2 sin(t / 2), t = b x
//     4096 / 56) are not read.
//   ParticleTask (0x665C30, Demi's 0x6D2990 shape) - the sprite sequence 0x101AF28 (frame 1) at
//     the point (sin, 0, cos)(angle) x (amplitude x reach >> 12) turned by its rotation, around the
//     average point, through the shadow camera (0x571BC0, 0x800 / -0x200); angle + 8 per tick;
//     ends after `life` draws.
//   PrimCallback (0x665970, Petrify Stare's 0x6BED30 + the 0x40000 rotation 0x7015B0, on this
//     module's cursor) draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws THEN updates (the counters after the draws, a layout in its play).
// ENTITY / CAMERA: no task writes an entity (position +0x1C..+0x20, flags) or the camera words
// (the camera animation is the engine's); the screen flash level is the engine's (0x5713E0).
// Module globals: 0x25055E0..0x2507428 (caster entity 0x25055E0, texture file 0x25055E4, magic
// buffer base 0x25055E8, the data copy 0x25055EC..0x25073BC, context 0x25073BC, master pool
// 0x25073C0, packet cursor 0x25073D4, the queues 0x25073D8 particles / 0x25073E8 main / 0x25073F8
// master, the warp frame 0x2507408: only m[0][0], m[1][1], TX, TY written).
// NEVER-WRITTEN STACK WORDS (the game's executor 0x508420 calls every task of a queue at one esp):
//   - The head / feet points' words +6 (0x6661D0), the particle's offset word +6 and the
//     callback's record offset word +6 only reach the GTE VZ0 high half (0x1CA8A16) or the shadow
//     camera's pad; the frames' pad words only the GTE R33 high half. Nothing else is read before
//     it is written: no residue is modelled.

#include "mag_common.h"

namespace ff8fx
{
namespace gravija311
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x25055E8); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25073BC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25073D4); }
	inline TaskQueue *QParticles() { return (TaskQueue *)0x25073D8; }      // pool: magic buffer, 0x40 x 0x1C
	inline TaskQueue *QMain() { return (TaskQueue *)0x25073E8; }           // pool: + 0x700, 1 x 0x348
	inline Mat4x3 &WarpFrame() { return var<Mat4x3>(0x2507408); }          // m[0][0], m[1][1], TX, TY written
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }

	static const uint32_t ORIG_MasterTask = 0x665380;
	static const uint32_t ORIG_MainTask = 0x665550;
	static const uint32_t ORIG_ParticleTask = 0x665C30;
	static const uint32_t MAIN = 0x700;                  // magic buffer: the main node
	static const uint32_t MORPH = 0xA48;                 // magic buffer: layout morph buffer
	static const uint32_t VBUF = 0xE48;                  // magic buffer: warp vertices (241 x 12)
	static const uint32_t VBUF_BYTES = 0x1994 - VBUF;
	static const uint32_t MORPH_BYTES = VBUF - MORPH;
	static const uint32_t ARENA_A = 0x1994;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11994;
	static uint8_t *const MESH = (uint8_t *)0x25055EC;   // the warp mesh (module copy): size, count, vertices, triangles, quads
	static const uint16_t *const SQRT_TABLE = (const uint16_t *)0x1020FC8;
	static const int16_t *const LEN_TABLE = (const int16_t *)0x10211C8;  // 0x400 x s16
	static const uint32_t MODEL_Main = 0x101CEB8;        // prim-model layout
	static const uint32_t SEQ_Particle = 0x101AF28;      // sprite sequence
	static const void *const SOUND_Gravija = (const void *)0x1021044;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesC(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701430)(angles, out); }  // MAG_117_sub_701430
	inline void RotationFromAngles2(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_sub_7043B0: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteWriteData2(int reg, uint32_t value) { fn<void (__cdecl *)(int, uint32_t)>(0x45DB90)(reg, value); }   // sub_45DB90
	inline void GteSetRGBC(const void *rgbc) { fn<void (__cdecl *)(const void *)>(0x45E110)(rgbc); }                     // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                                                       // sub_45F270
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }                                // set_param_with_dword_1CA8A68
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void InsertPrimAltViewport(uint32_t ot, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(ot, packet); }       // SSIGPU_InsertPrimAltViewport
	// sub_45B640: copies the current draw environment (0x5C bytes at 0x1CA8948)
	inline void GetDrawEnv(void *env) { fn<void (__cdecl *)(void *)>(0x45B640)(env); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x25073C0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x348 bytes (magic buffer + 0x700)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t amp;       // +0x0E particle amplitude (4.12)
		int16_t count;     // +0x10 targets of action 0
		uint8_t restored;  // +0x12 texture restore done (0x508630)
		uint8_t pad13;
		uint8_t *ents[14]; // +0x14 target entities
		int16_t avg[4];    // +0x4C the targets' average default effect position (+0x52: 0)
		uint8_t layout[0x2F4]; // +0x54 prim-model layout 0x101CEB8
	};
	struct ParticleNode // pool of 0x40 nodes of 0x1C bytes (magic buffer + 0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t life;      // +0x0E draws
		int16_t angle;     // +0x10 + 8 per tick
		int16_t reach;     // +0x12
		int16_t rot[3];    // +0x14 orientation (ZYX)
		int16_t pad1A;
	};
	// the prim-player callback's parameter block (a stack block of the main task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 anchor x, y, z (+6: the node's +0x52, 0)
		int32_t depth;     // +0x08 prim header +0x18
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x348 && sizeof(ParticleNode) == 0x1C, "Gravija nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Gravija prim block");

	enum DrawKind { K_PLAY = 1, K_WARP = 2, K_PARTICLE = 3 };
}
}

#ifdef FF8_FX_HELD
#include "mag311_gravija_held.h"
#endif

namespace ff8fx
{
namespace gravija311
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x665970 = Petrify Stare's 0x6BED30 with the 0x40000 rotation 0x7015B0):
	// one object of the layout. Vertex frame (or blend into the block's morph buffer), rotation
	// from the record's angles (flag 0x400: 0x701430, 0x40000: 0x7015B0, else 0x701310); flag 0x200:
	// the anchor through the camera with the offset added in screen axes, else anchor + offset
	// through the camera and the rotation composed with it; scale unless 1.0, fade, drawn by 0x7043B0.
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
		if (r->flags & 0x400) RotationFromAnglesC(r->rot, &m);
		else if (r->flags & 0x40000) RotationFromAngles2(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x200)
		{
			// the anchor through the camera, the offset added in screen axes
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
			// anchor + offset through the camera, rotation composed with the camera
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
		PacketCursor() = RenderPrimModel2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// 0x666380 (Sonic Wave's 0x6A9C80 on this module's tables, no zero-length guard): every vertex's
	// packed word = its length (bits 20..31, even) and its direction scaled by the length table
	// (bits 0..9 x, 10..19 y)
	static void NormaliseMesh(uint8_t *mesh)
	{
		uint32_t n = *(const uint32_t *)(mesh + 4);
		uint8_t *v = mesh + 8;
		do
		{
			const int32_t x = *(const int16_t *)v, y = *(const int16_t *)(v + 2);
			int32_t sq = (int32_t)((uint32_t)mul32(x, x) + (uint32_t)mul32(y, y));
			if (sq == 0) *(uint32_t *)(v + 4) = 0;
			else
			{
				GteSetLZCS(sq);
				int32_t lz;
				GteReadLZCR(&lz);
				const int32_t e = lz & ~1;
				const int32_t sh = (0x1F - e) >> 1;
				const int32_t c = e - 0x18;
				uint32_t idx;
				if (c > 0) idx = (uint32_t)sq << (c & 31);
				else idx = (uint32_t)(sq >> ((0x18 - e) & 31));
				const int32_t len = (int32_t)((uint32_t)SQRT_TABLE[idx] << (sh & 31)) >> 12;
				*(uint32_t *)(v + 4) = ((uint32_t)(len & ~1) << 19) | (*(const uint32_t *)(v + 4) & 0xFFFFF);
				int32_t a = 0x400 - (len >> 2);
				if (a < 0) a = 0;
				else if (a > 0x3FF) a = 0x3FF;
				const int32_t s = LEN_TABLE[a] >> 2;
				// (a vertex of length 0 would divide by zero, as in the game: the mesh has none)
				const int32_t dx = mul32(x, s) / len;
				const int32_t dy = mul32(y, s) / len;
				const uint32_t p = *(const uint32_t *)(v + 4);
				*(uint32_t *)(v + 4) = (((p & ~0x3FFu) | ((uint32_t)dx & 0x3FF)) & 0xFFF003FFu) | (((uint32_t)dy & 0x3FF) << 10);
			}
			v += 8;
		} while (--n);
	}

	// 0x666320 / 0x666350: the mesh normalised, then every triangle (Sonic Wave's 0x6A9DF0) and every
	// quad (0x6A60D0 of module 207) turned to start at its leftmost vertex (indices and colours) and
	// its indices made 12-byte vertex indices; the walk state lives in a Field_Alloc header (0x58)
	static void PrepareMesh(uint8_t *mesh)
	{
		NormaliseMesh(mesh);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint8_t **)h = mesh;
		*(uint8_t **)(h + 4) = mesh + 8;
		*(uint8_t **)(h + 0x20) = mesh + *(const uint32_t *)mesh + 0x10;
		// 0x6A9DF0: triangles (0x14 bytes: colour 0, indices, colours 1, 2)
		uint8_t *p = *(uint8_t **)(h + 0x20);
		uint32_t n = *(const uint32_t *)p;
		p += 4;
		*(uint8_t **)(h + 0x20) = p;
		if (n)
		{
			do
			{
				const uint32_t *verts = *(const uint32_t *const *)(h + 4);
				const uint16_t i0 = *(const uint16_t *)(p + 4), i1 = *(const uint16_t *)(p + 6), i2 = *(const uint16_t *)(p + 8);
				*(uint32_t *)(h + 0x34) = verts[i0];
				*(uint32_t *)(h + 0x38) = verts[i0 + 1];
				*(uint32_t *)(h + 0x3C) = verts[i1];
				*(uint32_t *)(h + 0x40) = verts[i1 + 1];
				*(uint32_t *)(h + 0x44) = verts[i2];
				*(uint32_t *)(h + 0x48) = verts[i2 + 1];
				int32_t lo = *(const int16_t *)(h + 0x34);
				bool second = false;
				if ((int32_t)*(const int16_t *)(h + 0x3C) < lo)
				{
					lo = *(const int16_t *)(h + 0x3C);
					second = true;
				}
				uint16_t *w = (uint16_t *)(p + 4);
				uint32_t *c = (uint32_t *)p;
				if ((int32_t)*(const int16_t *)(h + 0x44) < lo)
				{
					const uint16_t a = w[0], b = w[1], d = w[2];
					w[0] = d; w[1] = a; w[2] = b;
					const uint32_t c0 = c[0], c1 = c[3], c2 = c[4];
					c[0] = c2; c[4] = c1; c[3] = c0;
				}
				else if (second)
				{
					const uint16_t a = w[0], b = w[1], d = w[2];
					w[0] = b; w[2] = a; w[1] = d;
					const uint32_t c0 = c[0], c1 = c[3], c2 = c[4];
					c[0] = c1; c[3] = c2; c[4] = c0;
				}
				for (int k = 0; k < 3; k++) w[k] = (uint16_t)(w[k] + (uint16_t)(w[k] >> 1));
				p += 0x14;
			} while (--n);
			*(uint8_t **)(h + 0x20) = p;
		}
		// 0x6A60D0: quads (0x18 bytes: colour 0, indices 0..3, colours 1..3; vertex 3 opposite 0)
		p = *(uint8_t **)(h + 0x20);
		n = *(const uint32_t *)p;
		p += 4;
		*(uint8_t **)(h + 0x20) = p;
		if (n)
		{
			do
			{
				const uint32_t *verts = *(const uint32_t *const *)(h + 4);
				uint16_t *w = (uint16_t *)(p + 4);
				uint32_t *c = (uint32_t *)p; // c[0] colour 0, c[3..5] colours 1..3
				*(uint32_t *)(h + 0x34) = verts[w[0]];
				*(uint32_t *)(h + 0x38) = verts[w[0] + 1];
				*(uint32_t *)(h + 0x3C) = verts[w[1]];
				*(uint32_t *)(h + 0x40) = verts[w[1] + 1];
				*(uint32_t *)(h + 0x44) = verts[w[2]];
				*(uint32_t *)(h + 0x48) = verts[w[2] + 1];
				*(uint32_t *)(h + 0x4C) = verts[w[3]];
				*(uint32_t *)(h + 0x50) = verts[w[3] + 1];
				int32_t lo = *(const int16_t *)(h + 0x34);
				int which = 0;
				if ((int32_t)*(const int16_t *)(h + 0x3C) < lo) { lo = *(const int16_t *)(h + 0x3C); which = 1; }
				if ((int32_t)*(const int16_t *)(h + 0x44) < lo) { lo = *(const int16_t *)(h + 0x44); which = 2; }
				if ((int32_t)*(const int16_t *)(h + 0x4C) < lo)
				{
					// vertex 3 first: 3, 2, 1, 0
					const uint16_t a = w[0], b = w[1], d = w[2], e = w[3];
					w[0] = e; w[3] = a; w[1] = d; w[2] = b;
					const uint32_t c0 = c[0], c1 = c[3], c2 = c[4], c3 = c[5];
					c[0] = c3; c[5] = c0; c[3] = c2; c[4] = c1;
				}
				else if (which == 1)
				{
					// vertex 1 first: 1, 3, 0, 2
					const uint16_t a = w[0], b = w[1], d = w[2], e = w[3];
					w[0] = b; w[1] = e; w[3] = d; w[2] = a;
					const uint32_t c0 = c[0], c1 = c[3], c2 = c[4], c3 = c[5];
					c[0] = c1; c[3] = c3; c[5] = c2; c[4] = c0;
				}
				else if (which == 2)
				{
					// vertex 2 first: 2, 0, 3, 1
					const uint16_t a = w[0], b = w[1], d = w[2], e = w[3];
					w[0] = d; w[2] = e; w[3] = b; w[1] = a;
					const uint32_t c0 = c[0], c1 = c[3], c2 = c[4], c3 = c[5];
					c[0] = c2; c[4] = c3; c[5] = c1; c[3] = c0;
				}
				for (int k = 0; k < 4; k++) w[k] = (uint16_t)(w[k] + (uint16_t)(w[k] >> 1));
				p += 0x18;
			} while (--n);
			*(uint8_t **)(h + 0x20) = p;
		}
		FieldFree(0x58);
	}

	// 0x6662C0 (copied by Sonic Wave): grows the screen box (x0, y0, x1, y1) by the point (x, y, z)
	// when z > 0, keeping the smallest z; 1 when it did
	static int32_t ExpandBox(int16_t *box, const int16_t *p, int32_t *minz)
	{
		if (p[2] <= 0) return 0;
		if ((int32_t)p[2] < *minz) *minz = p[2];
		if (box[0] > p[0]) box[0] = p[0];
		if (box[2] < p[0]) box[2] = p[0];
		if (box[1] > p[1]) box[1] = p[1];
		if (box[3] < p[1]) box[3] = p[1];
		return 1;
	}

	// 0x6661D0 (Sonic Wave's 0x6A9AD0 + an OTZ-0 guard): the box of an entity's head (y - (size +
	// 0x100)) and feet points, each widened by its screen size 54 (size + 0x100) / OTZ (taken at the
	// head; OTZ 0 counts as 1) both ways (camera already in the GTE)
	static int32_t HeadBox(int16_t *box, const uint8_t *e, int32_t *minz)
	{
		const int16_t x = *(const int16_t *)(e + 0x1C), y = *(const int16_t *)(e + 0x1E), z = *(const int16_t *)(e + 0x20);
		const int16_t size = (int16_t)(*(const int16_t *)(e + 0x26) + 0x100);
		// [0] feet, [1] head; the 4th words are never written (GTE V0 pad only)
		int16_t pts[2][4] = { { x, y, z, 0 }, { x, (int16_t)(y - size), z, 0 } };
		int32_t any = 0, w = size;
		for (int i = 1; i >= 0; i--)
		{
			GteLoadV0(pts[i]);
			GteRTPS();
			int16_t q[4];
			GteReadSXY2(q);
			GteReadOTZ(q + 2);
			q[0] = (int16_t)(q[0] >> 3);
			q[1] = (int16_t)(q[1] >> 3);
			if (i == 1)
			{
				const int32_t d = q[2] ? (int32_t)q[2] : 1;
				w = mul32((int32_t)size, 54) / d;
			}
			q[0] = (int16_t)(q[0] - (int16_t)w);
			any |= ExpandBox(box, q, minz);
			q[0] = (int16_t)(q[0] + (int16_t)(w + w));
			any |= ExpandBox(box, q, minz);
		}
		return any;
	}

	// 0x665F50 (Sonic Wave's 0x6A97C0): the triangles as semi-transparent Gouraud triangles at the
	// vertices' screen positions, every vertex colour depth-cued (GTE DPCS with the warp's IR0 / far
	// colour 0)
	static void DrawTriangles(uint8_t *h)
	{
		uint8_t *t = *(uint8_t **)(h + 0x10);
		uint32_t n = *(const uint32_t *)t;
		t += 4;
		*(uint8_t **)(h + 0x10) = t;
		uint8_t *p = (uint8_t *)PacketCursor();
		if (n)
		{
			do
			{
				const uint8_t *v = *(const uint8_t **)(h + 4);
				memcpy(h + 0x18, v + *(const uint16_t *)(t + 4) * 4, 0xC);
				memcpy(h + 0x24, v + *(const uint16_t *)(t + 6) * 4, 0xC);
				memcpy(h + 0x30, v + *(const uint16_t *)(t + 8) * 4, 0xC);
				*(uint32_t *)(p + 8) = *(const uint32_t *)(h + 0x1C);
				*(uint32_t *)(p + 0x10) = *(const uint32_t *)(h + 0x28);
				*(uint32_t *)(p + 0x18) = *(const uint32_t *)(h + 0x34);
				GteSetRGBC(t);
				GteDPCS();
				GteStoreRGB2(p + 4);
				GteSetRGBC(t + 0xC);
				GteDPCS();
				GteStoreRGB2(p + 0xC);
				GteSetRGBC(t + 0x10);
				GteDPCS();
				GteStoreRGB2(p + 0x14);
				*(uint32_t *)p = 0x6000000;
				p[7] = 0x32;
				InsertPrimAltViewport(*(const uint32_t *)(h + 0x14), p);
				p += 0x1C;
				t += 0x14;
			} while (--n);
		}
		*(uint8_t **)(h + 0x10) = t;
		PacketCursor() = (uint32_t)p;
	}

	// 0x666070: the quads the same way (semi-transparent Gouraud quads)
	static void DrawQuads(uint8_t *h)
	{
		uint8_t *t = *(uint8_t **)(h + 0x10);
		uint32_t n = *(const uint32_t *)t;
		t += 4;
		*(uint8_t **)(h + 0x10) = t;
		uint8_t *p = (uint8_t *)PacketCursor();
		if (n)
		{
			do
			{
				const uint8_t *v = *(const uint8_t **)(h + 4);
				memcpy(h + 0x18, v + *(const uint16_t *)(t + 4) * 4, 0xC);
				memcpy(h + 0x24, v + *(const uint16_t *)(t + 6) * 4, 0xC);
				memcpy(h + 0x30, v + *(const uint16_t *)(t + 8) * 4, 0xC);
				memcpy(h + 0x3C, v + *(const uint16_t *)(t + 0xA) * 4, 0xC);
				*(uint32_t *)(p + 8) = *(const uint32_t *)(h + 0x1C);
				*(uint32_t *)(p + 0x10) = *(const uint32_t *)(h + 0x28);
				*(uint32_t *)(p + 0x18) = *(const uint32_t *)(h + 0x34);
				*(uint32_t *)(p + 0x20) = *(const uint32_t *)(h + 0x40);
				GteSetRGBC(t);
				GteDPCS();
				GteStoreRGB2(p + 4);
				GteSetRGBC(t + 0xC);
				GteDPCS();
				GteStoreRGB2(p + 0xC);
				GteSetRGBC(t + 0x10);
				GteDPCS();
				GteStoreRGB2(p + 0x14);
				GteSetRGBC(t + 0x14);
				GteDPCS();
				GteStoreRGB2(p + 0x1C);
				*(uint32_t *)p = 0x8000000;
				p[7] = 0x3A;
				InsertPrimAltViewport(*(const uint32_t *)(h + 0x14), p);
				p += 0x24;
				t += 0x18;
			} while (--n);
		}
		*(uint8_t **)(h + 0x10) = t;
		PacketCursor() = (uint32_t)p;
	}

	// 0x665D50: the warp of the mesh (frame already in the GTE) into `vbuf` (12-byte vertices), the
	// draw mode packets around the triangles and quads at OT entry `ot`, colours faded by `fade`.
	// (The original's 5th / 6th arguments - amplitude and phase - are not read.)
	static void Warp(uint8_t *mesh, const uint8_t *verts, int32_t ot, int32_t fade, uint8_t *vbuf)
	{
		uint8_t env[0x5C];
		GetDrawEnv(env); // (never read)
		uint32_t n = *(const uint32_t *)(mesh + 4);
		uint8_t *o = vbuf;
		const uint8_t *v = verts;
		do
		{
			int32_t ir1, ir2;
			GteReadDataRegTo(&ir1, 9);
			GteReadDataRegTo(&ir2, 10);
			*(int16_t *)o = (int16_t)ir1;
			*(int16_t *)(o + 2) = (int16_t)ir2;
			GteWriteData2(0, *(const uint32_t *)v);
			GteMVMVA_RotV0Tr();
			if (*(const int16_t *)o < 0) o[8] |= 1;
			else if (*(const int16_t *)o >= 0x140) o[8] |= 2;
			else *(uint16_t *)(o + 8) = 0;
			const int16_t sy = *(const int16_t *)(o + 2);
			if (sy < 0) o[8] |= 4;
			else if (sy >= 0xD8) o[8] |= 8;
			GteReadDataRegTo(&ir1, 9);
			GteReadDataRegTo(&ir2, 10);
			const int16_t u = (int16_t)((int16_t)ir1 >> 3);
			*(int16_t *)(o + 4) = u;
			*(int16_t *)(o + 6) = (int16_t)((int16_t)ir2 >> 3);
			if (u < 0) *(int16_t *)(o + 4) = 0;
			else if (*(const int16_t *)(o + 4) >= 0x140) *(int16_t *)(o + 4) = 0x13F;
			else if (*(const int16_t *)(o + 6) < 0) *(int16_t *)(o + 6) = 0;
			else if (*(const int16_t *)(o + 6) >= 0xD8) *(int16_t *)(o + 6) = 0xD7;
			o += 0xC;
			v += 8;
		} while (--n);
		uint8_t *h = (uint8_t *)FieldAlloc(0x48);
		*(uint8_t **)h = mesh;
		*(uint32_t *)(h + 8) = 0;
		*(uint8_t **)(h + 4) = vbuf;
		*(uint32_t *)(h + 0xC) = 0;
		*(uint32_t *)(h + 0x14) = OTBase() + (uint32_t)ot * 4;
		uint32_t *p = (uint32_t *)PacketCursor();
		PacketCursor() += 8;
		p[0] = 0x1000000;
		p[1] = 0xE1000220;
		InsertPrimAutoDepth(*(const uint32_t *)(h + 0x14), p);
		GteSetIR0(fade);
		GteSetFarColor(0, 0, 0);
		*(uint8_t **)(h + 0x10) = mesh + *(const uint32_t *)mesh + 0x10;
		DrawTriangles(h);
		DrawQuads(h);
		p = (uint32_t *)PacketCursor();
		PacketCursor() += 8;
		p[0] = 0x1000000;
		p[1] = 0xE1000240;
		InsertPrimAutoDepth(*(const uint32_t *)(h + 0x14), p);
		*(uint8_t **)(h + 0x10) = mesh + *(const uint32_t *)mesh + 0x10;
		FieldFree(0x48);
	}

	// the warp's frame (0x6655FA..0x665742): the box of the targets' heads and feet (camera in the
	// GTE), its centre, its size (at least 0x18000 / OTZ of the average point); false when no point
	// is in front or the nearest OTZ is 0x80 or less. *z_out = the nearest OTZ - 0x80.
	static bool WarpBox(const MainNode *w, Mat4x3 &frame, int32_t *z_out)
	{
		int16_t box[4] = { 0x7FFF, 0x7FFF, (int16_t)0x8001, (int16_t)0x8001 };
		int32_t any = 0, minz = 0x10000;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		for (int32_t i = 0; i < w->count; i++) any |= HeadBox(box, w->ents[i], &minz);
		const int32_t z = minz - 0x80;
		if (!any || z <= 0) return false;
		int16_t bw = (int16_t)(box[2] - box[0]);
		const int16_t cx = (int16_t)(box[0] + (int16_t)((int32_t)bw / 2));
		int16_t bh = (int16_t)(box[3] - box[1]);
		const int16_t cy = (int16_t)(box[1] + (int16_t)((int32_t)bh / 2));
		GteLoadV0(w->avg);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy); // (not used)
		int32_t otz;
		GteReadOTZ(&otz);
		const int16_t s = (int16_t)(0x18000 / (int32_t)(int16_t)otz);
		if (bw < s) bw = s;
		if (bh < s) bh = s;
		frame.m[1][1] = bh;
		frame.m[0][0] = bw;
		frame.t[1] = cy;
		frame.t[0] = cx;
		*z_out = z;
		return true;
	}

	// the warp's fade (IR0) at b = counter - 0x12
	static int32_t WarpFade(int32_t b)
	{
		if (b < 0x30) return 0xC00;
		return (int32_t)((((uint32_t)b + 0x3FFFD0u) << 10) >> 3) + 0xC40;
	}

	// the particle amplitude the main task sets at b = counter - 0x12 (0 <= b < 0x32)
	static int16_t ParticleAmp(int32_t b)
	{
		if (b < 0x10) return (int16_t)ComputeSin(b << 6);
		if (b <= 0x22) return 0x1000;
		return (int16_t)ComputeCos((b - 0x22) << 6);
	}

	// the particle's draw (0x665C30 up to its update)
	static void ParticleDraw(const ParticleNode *p, int16_t amp, const int16_t *avg)
	{
		Mat4x3 m = {};
		ComposeZYXRotationMatrix(p->rot, &m);
		const int32_t dist = mul32(amp, p->reach) >> 12;
		// the point on its circle (x, 0, z) turned by its orientation (4th word never written:
		// GTE V0 pad / shadow camera pad only)
		int16_t v[4] = {};
		v[0] = (int16_t)(mul32(ComputeSin(p->angle), dist) >> 12);
		v[2] = (int16_t)(mul32(ComputeCos(p->angle), dist) >> 12);
		v[1] = 0;
		GteSetRotMatrixCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0();
		GteStoreIR123(v);
		v[0] = (int16_t)(v[0] + avg[0]);
		v[1] = (int16_t)(v[1] + avg[1]);
		v[2] = (int16_t)(v[2] + avg[2]);
		TransformCameraByShadowRotation(v, 0x800, (int32_t)0xFFFFFE00);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Particle;
		*(uint16_t *)(h + 4) = 1;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// Particle (0x665C30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		const MainNode *w = (const MainNode *)(TexBase() + MAIN);
		// 30 fps layer: see mag311_gravija_held.inc
		FX_HELD(held_note_particle(n);)
		ParticleDraw(p, w->amp, w->avg);
		p->angle = (int16_t)(p->angle + 8);
		p->counter++;
		return p->counter < p->life ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Main (0x665550)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *w = (MainNode *)n;
		int32_t flash = 0xBB8;
		if (w->counter < 8) flash = mul32(w->counter, 3000) >> 3;
		else if (w->counter > 0x42) flash = mul32(0x4A - (int32_t)w->counter, 3000) >> 3;
		SetScreenFlash((uint32_t)flash, 0x808080);
		if ((uint16_t)w->counter < 0x4A)
		{
			PrimArg arg;
			memcpy(arg.pos, w->avg, 8);
			arg.depth = -0x80;
			arg.morph = TexBase() + MORPH;
			// 30 fps layer: see mag311_gravija_held.inc
			FX_HELD(held_note_main(K_PLAY, n);)
			prim::play((prim::Layout *)w->layout, PrimCallback, (int)&arg, 0);
		}
		int32_t b = (int32_t)w->counter - 0x12;
		if ((uint32_t)b < 0x38)
		{
			// 30 fps layer: see mag311_gravija_held.inc
			FX_HELD(held_note_main(K_WARP, n);)
			int32_t z;
			if (WarpBox(w, WarpFrame(), &z))
			{
				const int32_t t = (int32_t)(((uint32_t)b << 12) / 56);
				const int32_t s = ComputeSin(t >> 1); // (the warp's amplitude argument, not read)
				(void)s;
				if (b < 0x19)
				{
					if (b < 8)
					{
						WarpFrame().m[0][0] = (int16_t)((uint32_t)mul32(WarpFrame().m[0][0], b) >> 3);
						WarpFrame().m[1][1] = (int16_t)((uint32_t)mul32(WarpFrame().m[1][1], b) >> 3);
					}
				}
				const int32_t fade = WarpFade(b);
				GteSetRotMatrix(&WarpFrame());
				GteSetTransVector(&WarpFrame());
				Warp(MESH, MESH + 8, (z >> 2) + 0x11, fade, (uint8_t *)(TexBase() + VBUF));
			}
		}
		if (w->counter == 0) BdPlaySE(SOUND_Gravija, 0, 0x80);
		b = (int32_t)w->counter - 0x12;
		if ((uint32_t)b < 0x32)
		{
			int16_t amp;
			if (b == 0)
			{
				for (int i = 0; i < 0x40; i++)
				{
					ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
					if (!p) continue;
					p->counter = 0;
					p->life = (int16_t)(0x32 - (CrtRand() & 7));
					p->angle = (int16_t)CrtRand();
					p->rot[0] = (int16_t)CrtRand();
					p->rot[1] = (int16_t)CrtRand();
					p->rot[2] = (int16_t)CrtRand();
					p->reach = (int16_t)((CrtRand() & 0x3FF) + 0x5DC);
				}
			}
			amp = ParticleAmp(b);
			w->amp = amp;
			if (b == 0x31) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		}
		if (w->counter < 0x4A)
		{
			w->counter++;
			return 0;
		}
		if (w->counter == 0x4A)
		{
			TextureRestoreTask((void *)(TexBase() + ARENA_A), &w->restored);
			w->counter++;
		}
		SetScreenFlash(0, 0);
		return w->restored ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Master (0x665380)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag311_gravija_held.inc
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
			PrepareMesh(MESH);
			m->started = 1;
			InitTaskQueuePool(QMain(), (void *)(TexBase() + MAIN), 0x348, 1);
			InitTaskQueuePool(QParticles(), (void *)TexBase(), 0x1C, 0x40);
			MainNode *w = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&w->counter, 0, 0xCF);
			DecodeModelPrimLayout(MODEL_Main, w->layout, 0x2F4);
			w->count = (int16_t)Ctx()->actions[0].target_count;
			w->avg[2] = 0;
			w->avg[1] = 0;
			w->avg[0] = 0;
			for (int32_t i = 0; i < w->count; i++)
			{
				w->ents[i] = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
				int16_t pos[4];
				GetDefaultEffectPosition(w->ents[i], pos);
				w->avg[0] = (int16_t)(w->avg[0] + pos[0]);
				w->avg[1] = (int16_t)(w->avg[1] + pos[1]);
				w->avg[2] = (int16_t)(w->avg[2] + pos[2]);
			}
			w->avg[0] = (int16_t)((int32_t)w->avg[0] / (int32_t)w->count);
			w->avg[1] = (int16_t)((int32_t)w->avg[1] / (int32_t)w->count);
			w->avg[2] = (int16_t)((int32_t)w->avg[2] / (int32_t)w->count);
		}
		int a0;
		if (m->started)
		{
			a0 = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QParticles());
		}
		else a0 = (int)n;
		if (m->started && a0 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag311_gravija()
	{
		register_port(gravija311::ORIG_MasterTask, (void *)gravija311::MasterTask, "311 MasterTask", 311);
		register_port(gravija311::ORIG_MainTask, (void *)gravija311::MainTask, "311 MainTask", 311);
		register_port(gravija311::ORIG_ParticleTask, (void *)gravija311::ParticleTask, "311 ParticleTask", 311);
		// 30 fps layer: see mag311_gravija_held.inc
		FX_HELD(register_mag311_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag311_gravija_held.inc"
#endif
