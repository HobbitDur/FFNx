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

// Effect 214: Gravija of Omega Weapon / Ultima Weapon (enemy attack 309 of kernel.bin; MAG_214_*). A copy of
// Gravija 311 (mag311_gravija.cpp, Diablos' attack 257) at 0x69D5B0 with its own globals, without the grey screen
// flash, and with a real SCREEN WARP: the battle scene of the frame after the master's counter 1 is captured into
// VRAM (0x240, 0x100) and the warp mesh draws it back, each vertex pushed along its direction by a travelling sine.
//
// Structure (setup 0x69D5B0 -> _Init 0x69D5E0, file loader 0x69D5C0 = the texture file named at 0x10231C8 (311's
// name); the _Init copies 0x1DD0 DWORDS (0x7740 bytes, 4 times the 311 copy) from 0x1172D00 - the same warp mesh as
// 311's (0x1DC8 bytes: size, 241 vertices, 24 triangles, 216 quads) and whatever data follows it - into 0x25089BC,
// keeps the caster's entity (0x25089B0, not read again) and the cast context (0x25100FC), starts camera animation
// 0x1178938 and queues the TIM):
//   MasterTask (0x69D670) - alternates the packet arena (magic buffer + 0x1970 / + 0x11970); on counter 1 clears
//     VRAM (0x240, 0x100) 0x140 x 0x100 to black (0x45BEE0) and asks the next frame's drawing to land there
//     (0x45B5E0: draw offset (0x240, 0x100) for one OT pass: the screen copy), then (first time) prepares the mesh copy
//     (0x69E940 = 311's 0x666320 on this module's tables), sets up the pools (particles 0x40 x 0x1C at + 0, main
//     1 x 0x324 at + 0x700) and starts the main task (layout 0x1174AD0, the targets of action 0 and their average
//     default effect position). Then the main and particle queues; ends when the main queue is empty.
//   MainTask (0x69D870) - 311's main task without the screen flash: counters 0..0x49 the layout 0x1174AD0 at the
//     targets' average point, depth -0x80. Counters 0x12..0x49 (b = counter - 0x12): the warp frame of the targets'
//     heads and feet (0x69E860 = 311's 0x6661D0 without its OTZ-0 guard, with 311's box helper 0x6662C0), scale ramp
//     b / 8 below 8, the fade 0xC00 (from b = 0x30: 0xC40 + 0x80 per tick), and the warp's wave: phase t = b x 4096
//     / 56, amplitude 2 sin(t / 2). Sound at counter 0; counter 0x12: the 0x40 particles (CRT rand); amplitude
//     sin / 0x1000 / cos; counter 0x43 (b = 0x31): the action's results; counter 0x4A: battle textures restored
//     (0x508630, arena A).
//   Warp (0x69E020) - every vertex (x, y) moved by (dx, dy) x (sin table[(length x 2 & ~3) + t] x amplitude >> 12)
//     >> 12 (its packed direction / length), through the warp frame: magic buffer + 0xE24 (12 bytes: the moved
//     vertex's IR1 / IR2, the out-of-screen flags of the moved vertex (OR-ed in when outside), the unmoved vertex's
//     IR1 / IR2 clamped to the screen); a draw-mode packet E1000220, the Gouraud triangles / quads of 311 at the
//     unmoved positions (depth-cued by the fade), a draw-mode packet E1000240, then the TEXTURED triangles (0x69E2A0)
//     and quads (0x69E420): the moved positions, texture = the screen copy at the unmoved positions (tpage of the
//     first (leftmost) vertex, colour 0x808080), skipped when every vertex is outside on one common side.
//   ParticleTask (0x69DF00) - 311's (sequence 0x1172B40).
//   PrimCallback (0x69DC40) - 311's 0x665970 on this module's cursor.
// No task tests the draw-only flags: everything updates always. Every task draws THEN updates.
// ENTITY / CAMERA: no task writes an entity or the camera words. Engine words outside the module: the screen-copy
// request 0x1CA8850 bit 0 / 0x1CA89AC / 0x1CA89B4 (the OT executor clears them after one pass) and the VRAM copy.
// Module globals: 0x25089B0..0x2510168 (caster entity 0x25089B0, texture file 0x25089B4, magic buffer base 0x25089B8,
// the data copy 0x25089BC..0x25100FC, context 0x25100FC, master pool 0x2510100, packet cursor 0x2510114, the queues
// 0x2510118 particles / 0x2510128 main / 0x2510138 master, the warp frame 0x2510148: only m[0][0], m[1][1], TX, TY).
// NEVER-WRITTEN STACK WORDS: as 311 (only GTE pads). The textured draws' tpage sum takes the upper half of a register
// holding the third vertex's never-written word +0xA; only its bits 6..9 are kept: no residue is read.
// UNWRITTEN PACKET BYTES: the textured polygons' CLUT (+0xE) and pad (+0x1E, quads +0x26) are never written (the
// arena's own bytes), as in the game.

#include "mag_common.h"
#include "fx_params.h"

namespace ff8fx
{
namespace gravija214
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x25089B8); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25100FC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2510114); }
	inline TaskQueue *QParticles() { return (TaskQueue *)0x2510118; }      // pool: magic buffer, 0x40 x 0x1C
	inline TaskQueue *QMain() { return (TaskQueue *)0x2510128; }           // pool: + 0x700, 1 x 0x324
	inline Mat4x3 &WarpFrame() { return var<Mat4x3>(0x2510148); }          // m[0][0], m[1][1], TX, TY written
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }

	static const uint32_t ORIG_MasterTask = 0x69D670;
	static const uint32_t ORIG_MainTask = 0x69D870;
	static const uint32_t ORIG_ParticleTask = 0x69DF00;
	static const uint32_t MAIN = 0x700;                  // magic buffer: the main node
	static const uint32_t MORPH = 0xA24;                 // magic buffer: layout morph buffer
	static const uint32_t VBUF = 0xE24;                  // magic buffer: warp vertices (241 x 12)
	static const uint32_t VBUF_BYTES = 0x1970 - VBUF;
	static const uint32_t MORPH_BYTES = VBUF - MORPH;
	static const uint32_t ARENA_A = 0x1970;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11970;
	static uint8_t *const MESH = (uint8_t *)0x25089BC;   // the warp mesh (module copy): size, count, vertices, triangles, quads
	static const uint16_t *const SQRT_TABLE = (const uint16_t *)0x1178A50;
	static const int16_t *const SIN_TABLE = (const int16_t *)0x1178C50;  // 0x1000 x s16 (sin, 4.12); the length table is its first quarter
	static const uint32_t MODEL_Main = 0x1174AD0;        // prim-model layout
	static const uint32_t SEQ_Particle = 0x1172B40;      // sprite sequence
	static const void *const SOUND_Gravija = (const void *)0x1172B3C;
	static const uint32_t COPY_X = 0x240, COPY_Y = 0x100; // VRAM screen copy

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
	// 0x45BEE0 (isDravingOverlayImage): fills a VRAM rectangle (x, y, w, h) with an RGB colour
	inline void ClearImage(const int16_t *rect, int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(const int16_t *, int32_t, int32_t, int32_t)>(0x45BEE0)(rect, r, g, b); }
	// 0x45B5E0: the next OT pass draws at the offset (x, y) (0x1CA8850 |= 1, 0x1CA89B4 = x, 0x1CA89AC = y; the OT executor
	// 0x45D2EA clears them after the pass): the screen copy into VRAM
	inline void RequestDrawOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x45B5E0)(x, y); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2510100)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x324 bytes (magic buffer + 0x700)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t amp;       // +0x0E particle amplitude (4.12)
		int16_t count;     // +0x10 targets of action 0
		uint8_t restored;  // +0x12 texture restore done (0x508630)
		uint8_t pad13;
		uint8_t *ents[5];  // +0x14 target entities
		int16_t avg[4];    // +0x28 the targets' average default effect position (+0x2E: 0)
		uint8_t layout[0x2F4]; // +0x30 prim-model layout 0x1174AD0
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
		int16_t pos[4];    // +0x00 anchor x, y, z (+6: the node's +0x2E, 0)
		int32_t depth;     // +0x08 prim header +0x18
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x324 && sizeof(ParticleNode) == 0x1C, "Gravija 214 nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Gravija 214 prim block");

	enum DrawKind { K_PLAY = 1, K_WARP = 2, K_PARTICLE = 3 };
}
}

#ifdef FF8_FX_HELD
#include "mag214_omega_gravija_held.h"
#endif

namespace ff8fx
{
namespace gravija214
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x69DC40 = 311's 0x665970): one object of the layout. Vertex frame (or blend into the
	// block's morph buffer), rotation from the record's angles (flag 0x400: 0x701430, 0x40000: 0x7015B0, else
	// 0x701310); flag 0x200: the anchor through the camera with the offset added in screen axes, else anchor + offset
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

	// 0x69E9A0 (311's 0x666380 on this module's tables): every vertex's packed word = its length (bits 20..31,
	// even) and its direction scaled by the length table (bits 0..9 x, 10..19 y)
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
				const int32_t s = SIN_TABLE[a] >> 2;
				// (a vertex of length 0 would divide by zero, as in the game: the mesh has none)
				const int32_t dx = mul32(x, s) / len;
				const int32_t dy = mul32(y, s) / len;
				const uint32_t p = *(const uint32_t *)(v + 4);
				*(uint32_t *)(v + 4) = (((p & ~0x3FFu) | ((uint32_t)dx & 0x3FF)) & 0xFFF003FFu) | (((uint32_t)dy & 0x3FF) << 10);
			}
			v += 8;
		} while (--n);
	}

	// 0x69E940 / 0x69E970 (311's 0x666320 / 0x666350): the mesh normalised, then every triangle (Sonic Wave's
	// 0x6A9DF0) and every quad (0x6A60D0 of module 207) turned to start at its leftmost vertex (indices and colours)
	// and its indices made 12-byte vertex indices; the walk state lives in a Field_Alloc header (0x58)
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

	// 0x6662C0 (module 311's function, called by this module): grows the screen box (x0, y0, x1, y1) by the point
	// (x, y, z) when z > 0, keeping the smallest z; 1 when it did
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

	// 0x69E860 (311's 0x6661D0 without its OTZ-0 guard): the box of an entity's head (y - (size + 0x100)) and feet
	// points, each widened by its screen size 54 (size + 0x100) / OTZ (taken at the head) both ways (camera already in
	// the GTE). (An OTZ of 0 at the head faults in the game - integer division by 0; the port takes 1 there, as 311.)
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

	// 0x69E5E0 (311's 0x665F50): the triangles as semi-transparent Gouraud triangles at the vertices' (unmoved) screen
	// positions, every vertex colour depth-cued (GTE DPCS with the warp's IR0 / far colour 0)
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

	// 0x69E700 (311's 0x666070): the quads the same way (semi-transparent Gouraud quads)
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

	// the screen-copy texture page of a polygon whose first (leftmost) vertex is at x (header +8 / +0xC = the copy's
	// VRAM origin): only bits 6..9 of origin x + x count
	static uint16_t CopyTPage(const uint8_t *h, int16_t x)
	{
		const uint32_t ox = *(const uint32_t *)(h + 8);
		const int32_t oy = *(const int32_t *)(h + 0xC);
		return (uint16_t)((((ox + (uint32_t)(int32_t)x) >> 6) & 0xF) | ((((uint32_t)oy & 0x200) | 0x40) << 2) | ((uint32_t)(oy >> 4) & 0x10));
	}

	// 0x69E2A0: the triangles TEXTURED by the screen copy: positions = the moved vertices (record +0), texture
	// coordinates = the unmoved ones (record +4) relative to the first vertex's 64-pixel page; skipped when the three
	// vertices share an out-of-screen side (record +8)
	static void DrawCopyTriangles(uint8_t *h)
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
				if ((h[0x20] & h[0x2C] & h[0x38]) == 0)
				{
					const int16_t x0 = *(const int16_t *)(h + 0x1C);
					*(uint16_t *)(p + 0x16) = CopyTPage(h, x0);
					const uint8_t a = (uint8_t)(x0 & 0xC0);
					p[0xC] = (uint8_t)(h[0x1C] - a);
					p[0xD] = h[0x1E];
					p[0x14] = (uint8_t)(h[0x28] - a);
					p[0x15] = h[0x2A];
					p[0x1C] = (uint8_t)(h[0x34] - a);
					p[0x1D] = h[0x36];
					*(uint32_t *)(p + 8) = *(const uint32_t *)(h + 0x18);
					*(uint32_t *)(p + 0x10) = *(const uint32_t *)(h + 0x24);
					*(uint32_t *)(p + 4) = 0x808080;
					*(uint32_t *)(p + 0x18) = *(const uint32_t *)(h + 0x30);
					*(uint32_t *)p = 0x7000000;
					p[7] = 0x26;
					InsertPrimAltViewport(*(const uint32_t *)(h + 0x14), p);
					p += 0x20;
				}
				t += 0x14;
			} while (--n);
		}
		*(uint8_t **)(h + 0x10) = t;
		PacketCursor() = (uint32_t)p;
	}

	// 0x69E420: the quads the same way
	static void DrawCopyQuads(uint8_t *h)
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
				if ((h[0x44] & h[0x2C] & h[0x20] & h[0x38]) == 0)
				{
					const int16_t x0 = *(const int16_t *)(h + 0x1C);
					*(uint16_t *)(p + 0x16) = CopyTPage(h, x0);
					const uint8_t a = (uint8_t)(x0 & 0xC0);
					p[0xC] = (uint8_t)(h[0x1C] - a);
					p[0xD] = h[0x1E];
					p[0x14] = (uint8_t)(h[0x28] - a);
					p[0x15] = h[0x2A];
					p[0x1C] = (uint8_t)(h[0x34] - a);
					p[0x1D] = h[0x36];
					p[0x24] = (uint8_t)(h[0x40] - a);
					p[0x25] = h[0x42];
					*(uint32_t *)(p + 8) = *(const uint32_t *)(h + 0x18);
					*(uint32_t *)(p + 0x10) = *(const uint32_t *)(h + 0x24);
					*(uint32_t *)(p + 0x18) = *(const uint32_t *)(h + 0x30);
					*(uint32_t *)(p + 4) = 0x808080;
					*(uint32_t *)(p + 0x20) = *(const uint32_t *)(h + 0x3C);
					*(uint32_t *)p = 0x9000000;
					p[7] = 0x2E;
					InsertPrimAltViewport(*(const uint32_t *)(h + 0x14), p);
					p += 0x28;
				}
				t += 0x18;
			} while (--n);
		}
		*(uint8_t **)(h + 0x10) = t;
		PacketCursor() = (uint32_t)p;
	}

	// 0x69E020: the warp of the mesh (frame already in the GTE) into `vbuf` (12-byte vertices), each vertex moved
	// along its packed direction by the wave (sin table[((length word >> 18) & ~3) + phase] x amp >> 12); the draw-mode
	// packets around the Gouraud triangles / quads, then the screen-copy triangles / quads, at OT entry `ot`, colours
	// faded by `fade`
	static void Warp(uint8_t *mesh, const uint8_t *verts, int32_t ot, int32_t fade, int32_t amp, int32_t phase, uint8_t *vbuf)
	{
		uint8_t env[0x5C];
		GetDrawEnv(env); // (never read)
		uint32_t n = *(const uint32_t *)(mesh + 4);
		uint8_t *o = vbuf;
		const uint8_t *v = verts;
		do
		{
			const uint32_t pk = *(const uint32_t *)(v + 4);
			const int32_t k = mul32(SIN_TABLE[(uint32_t)((((int32_t)pk >> 18) & ~3) + phase) & 0xFFF], amp) >> 12;
			const int32_t dx = (int32_t)(pk << 22) >> 22, dy = (int32_t)(pk << 12) >> 22;
			int16_t moved[2];
			moved[0] = (int16_t)((mul32(dx, k) >> 12) + *(const int16_t *)v);
			moved[1] = (int16_t)((mul32(dy, k) >> 12) + *(const int16_t *)(v + 2));
			GteWriteData2(0, *(const uint32_t *)moved);
			GteMVMVA_RotV0Tr();
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
			*(int16_t *)(o + 6) = (int16_t)ir2;
			const int16_t u = (int16_t)ir1;
			*(int16_t *)(o + 4) = u;
			if (u < 0) *(int16_t *)(o + 4) = 0;
			else if (u >= 0x140) *(int16_t *)(o + 4) = 0x13F;
			else if (*(const int16_t *)(o + 6) < 0) *(int16_t *)(o + 6) = 0;
			else if (*(const int16_t *)(o + 6) >= 0xD8) *(int16_t *)(o + 6) = 0xD7;
			o += 0xC;
			v += 8;
		} while (--n);
		uint8_t *h = (uint8_t *)FieldAlloc(0x48);
		*(uint8_t **)h = mesh;
		*(uint32_t *)(h + 8) = COPY_X;
		*(uint8_t **)(h + 4) = vbuf;
		*(uint32_t *)(h + 0xC) = COPY_Y;
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
		DrawCopyTriangles(h);
		DrawCopyQuads(h);
		FieldFree(0x48);
	}

	// the warp's frame (0x69D8D4..0x69DA2A): the box of the targets' heads and feet (camera in the GTE), its centre,
	// its size (at least 0x18000 / OTZ of the average point); false when no point is in front or the nearest OTZ is
	// 0x80 or less. *z_out = the nearest OTZ - 0x80.
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

	// the particle's draw (0x69DF00 up to its update)
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
	// Particle (0x69DF00)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		const MainNode *w = (const MainNode *)(TexBase() + MAIN);
		// 30 fps layer: see mag214_omega_gravija_held.inc
		FX_HELD(held_note_particle(n);)
		ParticleDraw(p, w->amp, w->avg);
		p->angle = (int16_t)(p->angle + 8);
		p->counter++;
		return p->counter < p->life ? 0 : TASK_END;
	}

	// the wave of the warp at b = counter - 0x12: phase t = b x 4096 / 56, amplitude 2 sin(t / 2)
	static int32_t WavePhase(int32_t b) { return (int32_t)(((uint32_t)b << 12) / 56); }
	static int32_t WaveAmp(int32_t t) { const int32_t s = ComputeSin(t >> 1); return s + s; }

	// ------------------------------------------------------------------
	// Main (0x69D870)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *w = (MainNode *)n;
		if ((uint16_t)w->counter < 0x4A)
		{
			PrimArg arg;
			memcpy(arg.pos, w->avg, 8);
			arg.depth = -0x80;
			arg.morph = TexBase() + MORPH;
			// 30 fps layer: see mag214_omega_gravija_held.inc
			FX_HELD(held_note_main(K_PLAY, n);)
			prim::play((prim::Layout *)w->layout, PrimCallback, (int)&arg, 0);
		}
		int32_t b = (int32_t)w->counter - 0x12;
		if ((uint32_t)b < 0x38)
		{
			// 30 fps layer: see mag214_omega_gravija_held.inc
			FX_HELD(held_note_main(K_WARP, n);)
			int32_t z;
			if (WarpBox(w, WarpFrame(), &z))
			{
				const int32_t t = WavePhase(b);
				const int32_t amp = WaveAmp(t);
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
				Warp(MESH, MESH + 8, (z >> 2) + 0x11, fade, amp, t, (uint8_t *)(TexBase() + VBUF));
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
		return w->restored ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Master (0x69D670)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag214_omega_gravija_held.inc
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
		if (m->counter == 1)
		{
			// the screen copy: VRAM (0x240, 0x100) 320 x 256 cleared, the next OT pass drawn there
			const int16_t rect[4] = { (int16_t)COPY_X, (int16_t)COPY_Y, 0x140, 0x100 };
			ClearImage(rect, 0, 0, 0);
			RequestDrawOffset(COPY_X, COPY_Y);
			if (!m->started)
			{
				PrepareMesh(MESH);
				m->started = 1;
				InitTaskQueuePool(QMain(), (void *)(TexBase() + MAIN), 0x324, 1);
				InitTaskQueuePool(QParticles(), (void *)TexBase(), 0x1C, 0x40);
				MainNode *w = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
				Memset32(&w->counter, 0, 0xC6);
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

	void register_mag214_omega_gravija()
	{
		register_port(gravija214::ORIG_MasterTask, (void *)gravija214::MasterTask, "214 MasterTask", 214);
		register_port(gravija214::ORIG_MainTask, (void *)gravija214::MainTask, "214 MainTask", 214);
		register_port(gravija214::ORIG_ParticleTask, (void *)gravija214::ParticleTask, "214 ParticleTask", 214);
		// 30 fps layer: see mag214_omega_gravija_held.inc
		FX_HELD(register_mag214_held();)
	}
	FX_LAB_MODULE(register_mag214_omega_gravija);
}

#ifdef FF8_FX_HELD
#include "mag214_omega_gravija_held.inc"
#endif
