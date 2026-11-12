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

// Effect 207: Maelstrom (Edea's attack; MAG_207_*). The Gravija family (311 / 214: mag214_omega_gravija.cpp): the same
// master, the same screen copy into VRAM (0x240, 0x100) and the same warp drawing, with its own mesh (0x11AC624), a
// lens-like warp (each vertex's position scaled by a row of the table 0x11B3364 picked by its length), a prim-model
// layout with its own callback, and particles drawn into the vortex.
//
// Structure (setup 0x6A3AB0 -> _Init 0x6A3AE0, file loader 0x6A3AC0 = the texture file named at 0x11B5364
// ("mag213-310.tim"); the _Init copies 0x1DD0 DWORDS (0x7740 bytes) from 0x11AC624 - the warp mesh (0x790 bytes of
// vertices: 241, then 24 triangles and 216 quads) and what follows it - into 0x25102CC, keeps the caster's entity
// (0x25102C0, not read again) and the cast context (0x2517A0C), starts camera animation 0x11AC278, queues the TIM):
//   MasterTask (0x6A3B70) - 214's master: packet arena magic buffer + 0x1A2C / + 0x11A2C; on counter 1 the screen copy
//     (VRAM (0x240, 0x100) 320 x 256 cleared by 0x45BEE0, the next OT pass drawn there by 0x45B5E0), then (first time) the
//     mesh prepared (0x6A6070: every vertex's word +6 = its length / 32 (0x6A6280); triangles by Sonic Wave's 0x6A9DF0,
//     quads by 0x6A60D0), the pools (particles 0x40 x 0x20 at + 0, main 1 x 0x4E8 at + 0x800) and the main task (layout
//     0x11AE3F4, the targets of action 0, their average point: default effect position x / z, entity word +0x36 - 0x200
//     as y). Every tick once set up: the module matrix 0x2517A80 = the camera orthonormalised (0x67AAE0), then the
//     main and particle queues; ends when the main queue is empty.
//   MainTask (0x6A3D90) - counters 0..0x4F: the layout at the average point (callback 0x6A41E0, block: point, the
//     object kinds string 0x11B5370, depth -0x80, texture scroll counter x 32, morph buffer + 0xCE8). Counters
//     0x20..0x4F (b = counter - 0x20): the warp frame of the targets' heads and feet (0x6A5F10), scale ramp b / 8 below 8,
//     fade 0x1000 - sin(b x 2048 / 48), the warp row (16 - 16b/12 below 12, 16(b - 12)/12 below 24, then 16 + 16(b - 24)/12
//     folded at 0x20, at most 0x1F). Sound at counter 0. Counters 0x30..0x3F: 4 particles a tick (CRT rand: a direction
//     (0x6A6020), a distance 0x800 + (r & 0x7FF); they start that far from the point and fall back to it). Counter 0x50:
//     the action's results applied, battle textures restored (0x508630, arena A); ends when that is done.
//   PrimCallback (0x6A41E0, Full-cure 280's 0x67EB50 with the module matrix 0x2517A80): one layout object; the object's
//     kind (its char in the string): '9' drawn by the module's own prim-set renderer 0x6A45E0 (0x701DD0's copy calling the
//     screen-copy renderers 0x6A4710 / 0x6A4BA0 / 0x6A5120 and the renderers 0x65C880 / 0x657160 / 0x65CB40 / 0x65CE80 /
//     0x65D2A0 of modules 320 / 325: NOT PORTED, called in the exe: pure draws of the header it is given), '1' the
//     engine's 0x701DD0 with the texture scroll (low 7 bits of the block's scroll), else 0x701DD0.
//   ParticleTask (0x6A5650, Energy Bomber 217's 0x695930 shape) - sequence 0x11AC414 frame = counter at the point
//     through the shadow camera (0x571BC0, 0x1000 / -0x400), then velocity += acceleration (s8), point += velocity;
//     0x10 draws.
//   Warp (0x6A5700) - 214's warp with the lens: the moved vertex = (x, y) x row[length / 32] >> 12; the Gouraud polygons
//     (0x6A5C90 / 0x6A5DB0 = 311's) at the unmoved positions, then the screen-copy polygons (0x6A5950 / 0x6A5AD0 = 214's)
//     at the moved ones; vertices at magic buffer + 0xEE0.
// No task tests the draw-only flags. Every task draws THEN updates.
// ENTITY / CAMERA: no task writes an entity or the camera words; engine words outside the module: the screen-copy request
// (0x1CA8850 bit 0, 0x1CA89AC, 0x1CA89B4) and the VRAM copy.
// Module globals: 0x25102C0..0x2517AA0 (caster entity 0x25102C0, texture file 0x25102C4, magic buffer base 0x25102C8, the
// data copy 0x25102CC..0x2517A0C, context 0x2517A0C, master pool 0x2517A10, packet cursor 0x2517A24, the queues 0x2517A28
// particles / 0x2517A38 main / 0x2517A48 master, the warp frame 0x2517A58 (m[0][0], m[1][1], TX, TY), the module matrix
// 0x2517A80).
// NEVER-WRITTEN WORDS: as 214 (GTE pads; the screen-copy polygons' CLUT / pad halves keep the arena's bytes).

#include "mag_common.h"
#include "fx_params.h"

namespace ff8fx
{
namespace maelstrom207
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x25102C8); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2517A0C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2517A24); }
	inline TaskQueue *QParticles() { return (TaskQueue *)0x2517A28; }      // pool: magic buffer, 0x40 x 0x20
	inline TaskQueue *QMain() { return (TaskQueue *)0x2517A38; }           // pool: + 0x800, 1 x 0x4E8
	inline Mat4x3 &WarpFrame() { return var<Mat4x3>(0x2517A58); }          // m[0][0], m[1][1], TX, TY written
	inline Mat4x3 &ModuleMatrix() { return var<Mat4x3>(0x2517A80); }       // the camera orthonormalised, every master tick
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }

	static const uint32_t ORIG_MasterTask = 0x6A3B70;
	static const uint32_t ORIG_MainTask = 0x6A3D90;
	static const uint32_t ORIG_ParticleTask = 0x6A5650;
	static const uint32_t MAIN = 0x800;                  // magic buffer: the main node
	static const uint32_t MORPH = 0xCE8;                 // magic buffer: layout morph buffer
	static const uint32_t VBUF = 0xEE0;                  // magic buffer: warp vertices (241 x 12)
	static const uint32_t VBUF_BYTES = 0x1A2C - VBUF;
	static const uint32_t MORPH_BYTES = VBUF - MORPH;
	static const uint32_t ARENA_A = 0x1A2C;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11A2C;
	static uint8_t *const MESH = (uint8_t *)0x25102CC;   // the warp mesh (module copy): size, count, vertices, triangles, quads
	static const uint16_t *const SQRT_TABLE = (const uint16_t *)0x11B3164;
	static const int16_t *const LENS_TABLE = (const int16_t *)0x11B3364;  // 0x20 rows x 0x80 s16 (4.12 scale by length / 32)
	static const uint32_t MODEL_Main = 0x11AE3F4;        // prim-model layout
	static const uint32_t KINDS = 0x11B5370;             // "00900901": the layout objects' renderers
	static const uint32_t SEQ_Particle = 0x11AC414;      // sprite sequence
	static const void *const SOUND_Maelstrom = (const void *)0x11AC274;
	static const uint32_t COPY_X = 0x240, COPY_Y = 0x100; // VRAM screen copy

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_063_RenderPrimSet: prim-set draw of a 0x6C header (+0 model, +4 vertices, +8 fade colour, +0xC fade, +0x18 depth,
	// +0x1C mode, +0x20.. texture scroll)
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); }
	// 0x6A45E0: this module's copy of 0x701DD0 with its screen-copy renderers (not ported: called in the exe, a pure draw)
	inline uint32_t RenderPrimSetCopy(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x6A45E0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // MatrixMultiply3x3
	inline void OrthonormalizeCamera(const void *cam, Mat4x3 *out) { fn<void (__cdecl *)(const void *, Mat4x3 *)>(0x67AAE0)(cam, out); } // MAG_069_OrthonormalizeCameraMatrix
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteWriteData2(int reg, uint32_t value) { fn<void (__cdecl *)(int, uint32_t)>(0x45DB90)(reg, value); }   // sub_45DB90
	inline void GteSetRGBC(const void *rgbc) { fn<void (__cdecl *)(const void *)>(0x45E110)(rgbc); }                     // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                                                       // sub_45F270
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }                                // set_param_with_dword_1CA8A68
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void InsertPrimAltViewport(uint32_t ot, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(ot, packet); }       // SSIGPU_InsertPrimAltViewport
	inline void GetDrawEnv(void *env) { fn<void (__cdecl *)(void *)>(0x45B640)(env); }                                    // GPU_GetDrawEnv
	// 0x45BEE0 (isDravingOverlayImage): fills a VRAM rectangle (x, y, w, h) with an RGB colour
	inline void ClearImage(const int16_t *rect, int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(const int16_t *, int32_t, int32_t, int32_t)>(0x45BEE0)(rect, r, g, b); }
	// 0x45B5E0: the next OT pass draws at the offset (x, y) (the screen copy into VRAM)
	inline void RequestDrawOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x45B5E0)(x, y); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2517A10)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x4E8 bytes (magic buffer + 0x800)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t count;     // +0x0E targets of action 0
		uint8_t restored;  // +0x10 texture restore done (0x508630)
		uint8_t pad11[3];
		uint8_t *ents[5];  // +0x14 target entities
		int16_t avg[4];    // +0x28 the targets' average point (+0x2E: 0)
		uint8_t layout[0x4B8]; // +0x30 prim-model layout 0x11AE3F4
	};
	struct ParticleNode // pool of 0x40 nodes of 0x20 bytes (magic buffer + 0)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 draws (the sequence frame)
		int16_t vel[3];    // +0x14
		int8_t acc[3];     // +0x1A
		uint8_t pad1D[3];
	};
	// the prim-player callback's parameter block (a stack block of the main task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 anchor x, y, z (+6: the node's +0x2E, 0)
		uint32_t kinds;    // +0x08 the objects' renderer chars
		int16_t depth;     // +0x0C prim header +0x18
		int16_t scroll;    // +0x0E counter x 32: texture scroll (low 7 bits)
		uint32_t morph;    // +0x10 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x4E8 && sizeof(ParticleNode) == 0x20, "Maelstrom nodes");
	static_assert(sizeof(PrimArg) == 0x14, "Maelstrom prim block");

	enum DrawKind { K_PLAY = 1, K_WARP = 2, K_PARTICLE = 3 };
}
}

#ifdef FF8_FX_HELD
#include "mag207_maelstrom_held.h"
#endif

namespace ff8fx
{
namespace maelstrom207
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x6A41E0 = Full-cure 280's 0x67EB50 with this module's matrix, a 0x6C header, the block's
	// depth / morph / kinds): one object of the layout
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
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
		Mat4x3 m = {}; // (pad +0x12 never written in the original: GTE R33 word only)
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		const uint32_t flags = r->flags;
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 }; // (4th word never written in the original: GTE VZ0 high half only)
		if (flags & 0x1000)
		{
			// the anchor through the camera + the offset through the module matrix; the rotation through it too
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			GteSetRotMatrixCtrl(&ModuleMatrix());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t v[3];
			GteReadMAC123(v);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)v[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)v[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)v[2]);
			MatrixMultiply(&ModuleMatrix(), &m);
		}
		else if (flags & 0x200)
		{
			// the anchor through the camera + the offset
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			// the offset + the anchor through the camera, composed with it
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
			if (flags & 0x100)
			{
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MatrixMultiply3(&m, &d);
			}
			else
			{
				const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
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
		*(uint16_t *)(h + 0x22) = 0;
		*(uint16_t *)(h + 0x20) = 0;
		*(uint16_t *)(h + 0x26) = 0;
		*(uint16_t *)(h + 0x24) = 0;
		*(uint16_t *)(h + 0x2A) = 0x100;
		*(uint16_t *)(h + 0x28) = 0x100;
		const char kind = *(const char *)(arg->kinds + object);
		if (kind == '9') PacketCursor() = RenderPrimSetCopy(h, RenderOT(), 2, PacketCursor());
		else
		{
			if (kind == '1')
			{
				// texture scroll
				*(uint16_t *)(h + 0x2C) = 0;
				*(uint16_t *)(h + 0x2E) = 0;
				*(uint16_t *)(h + 0x32) = 0x80;
				*(uint16_t *)(h + 0x30) = 0x100;
				*(uint16_t *)(h + 0x22) = (uint16_t)((uint8_t)arg->scroll & 0x7F);
			}
			PacketCursor() = RenderPrimSet(h, RenderOT(), 2, PacketCursor());
		}
		FieldFree(0x6C);
	}

	// 0x6A6280: every vertex's word +6 = its length (sqrt(x^2 + y^2) by the table) / 32 (0 for a vertex at the centre:
	// the whole word +4)
	static void NormaliseMesh(uint8_t *mesh)
	{
		uint32_t n = *(const uint32_t *)(mesh + 4);
		uint8_t *v = mesh + 8;
		do
		{
			const int32_t x = *(const int16_t *)v, y = *(const int16_t *)(v + 2);
			int32_t sq = (int32_t)((uint32_t)mul32(y, y) + (uint32_t)mul32(x, x));
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
				*(int16_t *)(v + 6) = (int16_t)((int32_t)((uint32_t)SQRT_TABLE[idx] << (sh & 31)) >> 17);
			}
			v += 8;
		} while (--n);
	}

	// 0x6A6070 / 0x6A60A0: the mesh normalised, then every triangle (Sonic Wave's 0x6A9DF0) and every quad (0x6A60D0)
	// turned to start at its leftmost vertex and indexed for 12-byte vertices (the walk state in a Field_Alloc header)
	static void PrepareMesh(uint8_t *mesh)
	{
		NormaliseMesh(mesh);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint8_t **)h = mesh;
		*(uint8_t **)(h + 4) = mesh + 8;
		*(uint8_t **)(h + 0x20) = mesh + *(const uint32_t *)mesh + 0x10;
		// Sonic Wave's 0x6A9DF0 (called by 0x6A60A0): triangles (0x14 bytes: colour 0, indices, colours 1, 2)
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
					const uint16_t a = w[0], b = w[1], d = w[2], e = w[3];
					w[0] = e; w[3] = a; w[1] = d; w[2] = b;
					const uint32_t c0 = c[0], c1 = c[3], c2 = c[4], c3 = c[5];
					c[0] = c3; c[5] = c0; c[3] = c2; c[4] = c1;
				}
				else if (which == 1)
				{
					const uint16_t a = w[0], b = w[1], d = w[2], e = w[3];
					w[0] = b; w[1] = e; w[3] = d; w[2] = a;
					const uint32_t c0 = c[0], c1 = c[3], c2 = c[4], c3 = c[5];
					c[0] = c1; c[3] = c3; c[5] = c2; c[4] = c0;
				}
				else if (which == 2)
				{
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

	// 0x6662C0 (module 311's function, called by this module): grows the screen box by the point (x, y, z) when z > 0
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

	// 0x6A5F10: the box of an entity's feet (x, y, z) and head (x, word +0x36 - 0x200, z) points, screen positions / 8
	// (rounded toward 0), each widened by its screen size 54 (size + 0x100) / OTZ (taken at the head) both ways. (An OTZ
	// of 0 at the head faults in the game; the port takes 1 there.)
	static int32_t HeadBox(int16_t *box, const uint8_t *e, int32_t *minz)
	{
		const int16_t x = *(const int16_t *)(e + 0x1C), y = *(const int16_t *)(e + 0x1E), z = *(const int16_t *)(e + 0x20);
		const int16_t size = (int16_t)(*(const int16_t *)(e + 0x26) + 0x100);
		const int16_t hy = (int16_t)(*(const int16_t *)(e + 0x36) - 0x200);
		int16_t pts[2][4] = { { x, y, z, 0 }, { x, hy, z, 0 } }; // (4th words: GTE V0 pad only)
		int32_t any = 0, w = size;
		for (int i = 1; i >= 0; i--)
		{
			GteLoadV0(pts[i]);
			GteRTPS();
			int16_t q[4];
			GteReadSXY2(q);
			GteReadOTZ(q + 2);
			q[0] = (int16_t)((int32_t)q[0] / 8);
			q[1] = (int16_t)((int32_t)q[1] / 8);
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

	// 0x6A5C90 (311's 0x665F50): the triangles as semi-transparent Gouraud triangles at the unmoved positions, depth-cued
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

	// 0x6A5DB0 (311's 0x666070): the quads the same way
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

	// the screen-copy texture page of a polygon whose first (leftmost) vertex is at x (header +8 / +0xC = the copy's origin)
	static uint16_t CopyTPage(const uint8_t *h, int16_t x)
	{
		const uint32_t ox = *(const uint32_t *)(h + 8);
		const int32_t oy = *(const int32_t *)(h + 0xC);
		return (uint16_t)((((ox + (uint32_t)(int32_t)x) >> 6) & 0xF) | ((((uint32_t)oy & 0x200) | 0x40) << 2) | ((uint32_t)(oy >> 4) & 0x10));
	}

	// 0x6A5950 (214's 0x69E2A0): the triangles textured by the screen copy at the moved positions
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

	// 0x6A5AD0 (214's 0x69E420): the quads the same way
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

	// 0x6A5700: the warp of the mesh (frame already in the GTE) into `vbuf`, each vertex scaled by the lens row `row`
	// (0x80 s16 by length / 32), then the draw-mode packets around the Gouraud polygons and the screen-copy polygons at OT
	// entry `ot`, colours faded by `fade`
	static void Warp(uint8_t *mesh, const uint8_t *verts, int32_t ot, int32_t fade, const int16_t *row, uint8_t *vbuf)
	{
		uint8_t env[0x5C];
		GetDrawEnv(env); // (never read)
		uint32_t n = *(const uint32_t *)(mesh + 4);
		uint8_t *o = vbuf;
		const uint8_t *v = verts;
		do
		{
			const int32_t k = row[*(const int16_t *)(v + 6)];
			int16_t moved[2];
			moved[0] = (int16_t)(mul32(*(const int16_t *)v, k) >> 12);
			moved[1] = (int16_t)(mul32(*(const int16_t *)(v + 2), k) >> 12);
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
			const int16_t u = (int16_t)ir1;
			*(int16_t *)(o + 4) = u;
			*(int16_t *)(o + 6) = (int16_t)ir2;
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

	// the warp's frame (0x6A3E01..0x6A3F89): the box of the targets' heads and feet, its centre, its size (at least
	// 0x18000 / OTZ of the average point); false when no point is in front or the nearest OTZ is 0x80 or less
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
		GteReadSXY2(sxy); // (/ 8 into a local: not used)
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

	// the warp's fade at b = counter - 0x20: 0x1000 - sin(b x 2048 / 48)
	static int32_t WarpFade(int32_t b) { return 0x1000 - ComputeSin((int32_t)(((uint32_t)b << 11) / 48)); }

	// the warp's lens row at b
	static int32_t WarpRow(int32_t b)
	{
		int32_t r;
		if ((uint32_t)b < 0xC) r = 0x10 - (int32_t)(((uint32_t)b << 4) / 12);
		else if ((uint32_t)b < 0x18) r = (int32_t)((((uint32_t)b - 0xC) << 4) / 12);
		else
		{
			r = (int32_t)((((uint32_t)b - 0x18) << 4) / 12) + 0x10;
			if (r <= 0x1F) return r;
			r = 0x40 - r;
		}
		return r > 0x1F ? 0x1F : r;
	}

	// the particle's draw (0x6A5650 up to its update)
	static void ParticleDraw(int16_t *pos, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, 0x1000, (int32_t)0xFFFFFC00);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_Particle;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// 0x6A6020: the unit direction of pitch a / yaw b (4.12)
	static void DirectionFromPitchYaw(int32_t a, int32_t b, int16_t *out)
	{
		const int32_t c = ComputeCos(a);
		out[1] = (int16_t)-ComputeSin(a);
		out[0] = (int16_t)(mul32(ComputeSin(b), c) >> 12);
		out[2] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeCos(b), c)) >> 12);
	}

	// ------------------------------------------------------------------
	// Particle (0x6A5650)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		// 30 fps layer: see mag207_maelstrom_held.inc
		FX_HELD(held_note_particle(n);)
		ParticleDraw(p->pos, p->counter);
		p->vel[0] = (int16_t)(p->vel[0] + (int16_t)p->acc[0]);
		p->vel[1] = (int16_t)(p->vel[1] + (int16_t)p->acc[1]);
		p->vel[2] = (int16_t)(p->vel[2] + (int16_t)p->acc[2]);
		p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
		p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
		p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
		p->counter++;
		return p->counter < 0x10 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Main (0x6A3D90)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *w = (MainNode *)n;
		if ((uint32_t)(int32_t)w->counter < 0x50)
		{
			PrimArg arg;
			memcpy(arg.pos, w->avg, 8);
			arg.kinds = KINDS;
			arg.depth = -0x80;
			arg.scroll = (int16_t)(w->counter << 5);
			arg.morph = TexBase() + MORPH;
			// 30 fps layer: see mag207_maelstrom_held.inc
			FX_HELD(held_note_main(K_PLAY, n);)
			prim::play((prim::Layout *)w->layout, PrimCallback, (int)&arg, 0);
		}
		const int32_t b = (int32_t)w->counter - 0x20;
		if ((uint32_t)b < 0x30)
		{
			// 30 fps layer: see mag207_maelstrom_held.inc
			FX_HELD(held_note_main(K_WARP, n);)
			int32_t z;
			if (WarpBox(w, WarpFrame(), &z))
			{
				const int32_t fade = WarpFade(b);
				if (b < 8)
				{
					WarpFrame().m[0][0] = (int16_t)((uint32_t)mul32(WarpFrame().m[0][0], b) >> 3);
					WarpFrame().m[1][1] = (int16_t)((uint32_t)mul32(WarpFrame().m[1][1], b) >> 3);
				}
				GteSetRotMatrix(&WarpFrame());
				GteSetTransVector(&WarpFrame());
				Warp(MESH, MESH + 8, (z >> 2) + 0x11, fade, LENS_TABLE + WarpRow(b) * 0x80, (uint8_t *)(TexBase() + VBUF));
			}
		}
		if (w->counter == 0) BdPlaySE(SOUND_Maelstrom, 0, 0x80);
		if ((uint32_t)((int32_t)w->counter - 0x30) < 0x10)
		{
			for (int i = 0; i < 4; i++)
			{
				ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
				if (!p) break;
				p->counter = 0;
				const int32_t r1 = CrtRand();
				const int32_t r2 = CrtRand();
				DirectionFromPitchYaw(r2, r1, p->pos);
				const int32_t dist = (CrtRand() & 0x7FF) + 0x800;
				const int32_t dx = mul32(p->pos[0], dist) >> 12;
				const int32_t dy = mul32(p->pos[1], dist) >> 12;
				const int32_t dz = mul32(p->pos[2], dist) >> 12;
				p->vel[2] = 0;
				p->vel[1] = 0;
				p->vel[0] = 0;
				p->acc[0] = (int8_t)((int32_t)(0u - (uint32_t)dx) >> 7);
				p->acc[1] = (int8_t)((int32_t)(0u - (uint32_t)dy) >> 7);
				p->acc[2] = (int8_t)((int32_t)(0u - (uint32_t)dz) >> 7);
				p->pos[0] = (int16_t)(w->avg[0] + (int16_t)dx);
				p->pos[1] = (int16_t)((int16_t)dy + w->avg[1]);
				p->pos[2] = (int16_t)((int16_t)dz + w->avg[2]);
			}
		}
		if (w->counter < 0x50)
		{
			w->counter++;
			return 0;
		}
		if (w->counter == 0x50)
		{
			ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
			TextureRestoreTask((void *)(TexBase() + ARENA_A), &w->restored);
			w->counter++;
		}
		return w->restored ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Master (0x6A3B70)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag207_maelstrom_held.inc
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
				InitTaskQueuePool(QMain(), (void *)(TexBase() + MAIN), 0x4E8, 1);
				InitTaskQueuePool(QParticles(), (void *)TexBase(), 0x20, 0x40);
				MainNode *w = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
				Memset32(&w->counter, 0, 0x137);
				DecodeModelPrimLayout(MODEL_Main, w->layout, 0x4B8);
				w->count = (int16_t)Ctx()->actions[0].target_count;
				w->avg[2] = 0;
				w->avg[1] = 0;
				w->avg[0] = 0;
				for (int32_t i = 0; i < w->count; i++)
				{
					w->ents[i] = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
					int16_t pos[4];
					GetDefaultEffectPosition(w->ents[i], pos);
					const int16_t y = (int16_t)(*(const int16_t *)(w->ents[i] + 0x36) - 0x200);
					w->avg[0] = (int16_t)(w->avg[0] + pos[0]);
					w->avg[2] = (int16_t)(w->avg[2] + pos[2]);
					w->avg[1] = (int16_t)(w->avg[1] + y);
					pos[1] = y; // (the local's y word, not read again)
				}
				w->avg[0] = (int16_t)((int32_t)w->avg[0] / (int32_t)w->count);
				w->avg[1] = (int16_t)((int32_t)w->avg[1] / (int32_t)w->count);
				w->avg[2] = (int16_t)((int32_t)w->avg[2] / (int32_t)w->count);
			}
		}
		int a0;
		if (m->started)
		{
			OrthonormalizeCamera((const void *)0x1D97778, &ModuleMatrix());
			a0 = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QParticles());
		}
		else a0 = (int)n;
		if (m->started && a0 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag207_maelstrom()
	{
		register_port(maelstrom207::ORIG_MasterTask, (void *)maelstrom207::MasterTask, "207 MasterTask", 207);
		register_port(maelstrom207::ORIG_MainTask, (void *)maelstrom207::MainTask, "207 MainTask", 207);
		register_port(maelstrom207::ORIG_ParticleTask, (void *)maelstrom207::ParticleTask, "207 ParticleTask", 207);
		// 30 fps layer: see mag207_maelstrom_held.inc
		FX_HELD(register_mag207_held();)
	}
	FX_LAB_MODULE(register_mag207_maelstrom);
}

#ifdef FF8_FX_HELD
#include "mag207_maelstrom_held.inc"
#endif
