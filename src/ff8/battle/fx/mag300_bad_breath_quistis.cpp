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

// Effect 300: Bad Breath (Quistis' Blue Magic, kernel.bin section 19 "Bad Breath", magic +4 = 300;
// MAG_300_BAD_BREATH_QUISTIS*), cast by a party member (Quistis) on the enemies.
//
// The module 0x66EF00..0x66FCC0 is a hand-written relative of enemy Bad Breath 131 (mag131_bad_breath.cpp):
// setup / loader / _Init (0x66EF00 / 0x66EF10 / 0x66EF30), the mesh draw 0x66F710 and the sprite task 0x66FB20
// are 131's text (library copies); the swirl 0x66FBB0 is 131's 0x6B4140 on this module's buffer; the mesh
// quad lists are drawn by 131's own 0x6B3370 / 0x6B37F0 and the meshes normalised by Breath 129's 0x6B7CB0
// (both called across modules: ported here as MeshQuads / MeshGouraudQuads / NormalizeDepth); the master, the
// breath (emitter), the hit task, its prim callback, the party reaction and the mouth point are new.
//
// Structure (setup 0x66EF00 -> _Init 0x66EF30, file loader 0x66EF10 = the texture named at 0x1047110 into
// 0x25077A0; _Init keeps the context and the CASTER's entity (0x2507778), sets up the master queue
// 0x2507800 (pool 0x25077B0, 1 x 0x14), starts camera animation 0x1042F80 and queues the TIM):
//   MasterTask (0x66EFB0) - alternates the packet arena (magic buffer + 0xD84 / + 0x18D84); on counter 1
//     sets up the pools (smoke 0x20 x 0x18 at + 0, hits 4 x 0x140 at + 0x300, breath 1 x 0x3C at + 0x800),
//     starts the breath: the targets of action 0 sorted by entity x (LARGEST first), the sweep half width
//     max(0x700, |x| of every target's default effect position) + 0x100, the targets' mean effect y / z;
//     normalises the two breath meshes 0x103FA04 / 0x1040E84 (Breath 129's 0x6B7CB0: w of every vertex,
//     exe tables rewritten on every cast); binds the caster's animation set (com data 0x103DCB0, out block
//     magic buffer + 0x83C) and starts its chain transformation 1. Then the breath, hit and smoke queues;
//     ends when all three are empty.
//   BreathTask (0x66F270) - every tick the mouth = point (0x50C, 0, 0x223) of the caster's bone 9 (0x66FC40:
//     her bone matrices REBUILT from her pose, through her world matrix): it FOLLOWS Quistis' animation.
//     Counters 8..0x39 (t = counter - 8): a yellow full-screen fade tile (0x700150: in 14, hold 22, out 14),
//     the two meshes (each swirled by 0x66FBB0 into magic buffer + 0x85C, rotated about z by +angle / -angle,
//     swirl phase / phase + 0x400) stretched from the mouth to the sweep point (x = cos(t * 2048 / 50) * the
//     half width, the targets' mean y / z; the length grows in over 16 ticks, sin(t * 64)), coloured from
//     the table 0x10430F8 (3 steps of 4 ticks) faded in (t < 8) / out (t > 0x22) from / to 0x25077C8 (never
//     written: black), U scrolled by t * 10, drawn by 0x66F710. A hit task each time the sweep x passes a
//     target's x + 300. At t = 0 every other active party member gets chain transformation 2, at t = 0x31
//     chain transformation 1 (0x66F750). Sound at counter 10. Angle -= 0x32, phase += 0xC8 per tick; ends
//     after counter 0x3A.
//   HitTask (0x66F790) - 0x25 ticks: the layout 0x1042304 at the target's default effect position (y = entity
//     +0x1E + entity +0x92 / 2, taken at the spawn) through PrimCallback, scaled by entity +0x26 / 4 + 0x200
//     when the parameter block's word +0x14 is not 0 (see STACK WORDS); counters 0..8 a smoke task on a
//     random bone of the target (CRT rand: bone, size); the damage at counter 0x1E.
//   SmokeTask (0x66FB20) - sprite sequence 0x103F824 through the shadow-rotation camera, rising 16 per tick,
//     0x12 frames (131's sprite task).
//   PrimCallback (0x66F8A0) - one layout object: vertex frame / blend, rotation from the record's angles; with
//     the block's word +0x14 set, the record offset and the rotation scaled by the block's scale; offset + the
//     block's position through the camera's rotation, composed with the camera, record scale, drawn by
//     0x572200.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws THEN updates (no draw-only pass).
// ENTITY WRITES: caster - animation set + chain transformation 1 (master, counter 1, engine), her bone
// matrices rebuilt from her pose every breath tick (0x508C90); every other active party member (slots 0..2,
// entity flag 2, not flags +8 & 3) - chain transformation 2 at breath t = 0 and 1 at t = 0x31 (engine); the
// targets - their action results (hit counter 0x1E, engine). No entity position / colour word is written.
// CAMERA: the setup's camera animation 0x1042F80 (engine); no camera word is written by the tasks.
// Module globals: 0x2507758..0x2507810 (mesh frame 0x2507758, caster 0x2507778, texture file 0x25077A0, magic
// buffer base 0x25077A4, context 0x25077A8, master pool 0x25077B0, packet cursor 0x25077C4, fade colour
// 0x25077C8, the queues 0x25077D0 smoke / 0x25077E0 hits / 0x25077F0 breath / 0x2507800 master).
// STACK WORDS: the hit task's parameter block word +0x14 (entry esp - 8 of 0x66F790) is never written by it:
// 0x66F8A0 tests it (scale the layout or not). The master runs its three queues with the executor 4 bytes
// deeper each time, so that word is entry esp - 0xC of the breath task (its local [esp + 0x64]: the mouth x,
// sign-extended, written on every tick of counters 8..0x39 before the hit queue runs) and entry esp - 4 of
// the smoke task (its saved esi = the executor's current node: the smoke node). Modelled by HitFlagWord():
// the last of those writes (mouth x / smoke node), read as "not 0".

#include "mag_common.h"
#include "mag_ice_common.h"

namespace ff8fx
{
namespace badbreath300
{
	using namespace eng;
	using namespace magc;
	using namespace icec;

	// --- module globals ---
	inline Mat4x3 &MeshFrame() { return var<Mat4x3>(0x2507758); }          // camera x mesh frame (breath)
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2507778); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x25077A4); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25077A8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25077C4); }
	static const void *const FADE_COLOUR = (const void *)0x25077C8;       // never written (0)
	inline TaskQueue *QSmoke() { return (TaskQueue *)0x25077D0; }          // pool: magic buffer, 0x20 x 0x18
	inline TaskQueue *QHits() { return (TaskQueue *)0x25077E0; }           // pool: + 0x300, 4 x 0x140
	inline TaskQueue *QBreath() { return (TaskQueue *)0x25077F0; }         // pool: + 0x800, 1 x 0x3C
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_MasterTask = 0x66EFB0;
	static const uint32_t ORIG_BreathTask = 0x66F270;
	static const uint32_t ORIG_HitTask = 0x66F790;
	static const uint32_t ORIG_SmokeTask = 0x66FB20;
	static const uint32_t ORIG_PrimCallback = 0x66F8A0;    // not a task: the hit layout's callback
	static uint8_t *const MESH1 = (uint8_t *)0x103FA04;   // breath meshes: size, count, vertices (w rewritten), lists
	static uint8_t *const MESH2 = (uint8_t *)0x1040E84;
	static const uint32_t MODEL_Hit = 0x1042304;          // prim-model layout
	static const uint32_t SEQ_Smoke = 0x103F824;          // sprite sequence
	static const uint32_t CAMERA_Anim = 0x1042F80;        // (the setup's)
	static const uint32_t COM_Anim = 0x103DCB0;           // the caster's animation set (com data)
	static const uint32_t BIND_Block = 0x83C;             // magic buffer: BS_BindEntityModelFromComData's out block
	static const uint8_t *const COLOURS = (const uint8_t *)0x10430F8;     // 4 x RGBx
	static const int16_t *const SWIRL_TABLE = (const int16_t *)0x1043110; // 0x1000 x (s16, s16)
	static const int16_t *const MOUTH = (const int16_t *)0x1043108;       // (0x50C, 0, 0x223), bone 9
	static const void *const SOUND_Breath = (const void *)0x10430F4;
	static const uint32_t VERTS = 0x85C;                  // magic buffer: swirled mesh vertices / morph output
	static const uint32_t ARENA_A = 0xD84, ARENA_B = 0x18D84; // magic buffer: packet arenas
	static const uint32_t POOL_Hits = 0x300, POOL_Breath = 0x800;

	// engine functions not in fx_port.h / mag_common.h / mag_ice_common.h
	inline uint32_t FadeTile(int32_t t, int32_t r, int32_t g, int32_t b, int32_t in, int32_t hold, int32_t out, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x700150)(t, r, g, b, in, hold, out, cursor); } // Effect_FadeTileInHoldOut
	inline uint32_t Tile(int32_t r, int32_t g, int32_t b, int32_t level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x7040B0)(r, g, b, level, cursor); } // MAG_063_sub_7040B0 (held frames)
	inline void RotZ(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x7012C0)(angle, out); }                // MAG_065_RotationZMatrix
	inline void GteZeroTrans() { fn<void (__cdecl *)()>(0x704070)(); }      // GTE_ZeroTranslation
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // MatrixMultiply3x3: a = a x b (3x3)
	inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); } // ColourLerp
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void GteLoadRGBBytes(const void *rgb) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(rgb); } // IR1..3 = 3 x u8
	inline void GteStoreRGBBytes(void *dst) { fn<void (__cdecl *)(void *)>(0x45E410)(dst); }      // IR1..3 low bytes -> 3 x u8
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b.rot = a.rot x b.rot
	inline void BindEntityModelFromComData(uint8_t *entity, uint32_t out, uint32_t data) { fn<void (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData
	inline void QueueChainTransformation(uint8_t *entity, int32_t id) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, id); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x25077B0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct BreathNode // pool of 1 node of 0x3C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t count;     // +0x0E targets of action 0
		uint8_t *ents[4];  // +0x10 target entities (sorted by x, largest first)
		uint8_t idx[4];    // +0x20 their target record indexes
		int16_t width;     // +0x24 sweep half width
		int16_t z;         // +0x26 the targets' mean effect z
		int16_t y;         // +0x28 the targets' mean effect y
		int16_t angle;     // +0x2A mesh rotation about z
		int16_t phase;     // +0x2C swirl phase
		int16_t next;      // +0x2E next target the sweep reaches
		int32_t length;    // +0x30 mouth -> sweep point distance + 0x200
		int16_t mouth[3];  // +0x34 the mouth point (0x66FC40)
		int16_t pad3A;
	};
	struct HitNode // pool of 4 nodes of 0x140 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t idx;       // +0x0E target record index
		uint8_t slot;      // +0x0F target entity slot
		int16_t pos[4];    // +0x10 default effect position at the spawn (y = entity +0x1E + entity +0x92 / 2)
		uint8_t layout[0x128]; // +0x18 prim-model layout
	};
	struct SmokeNode // pool of 0x20 nodes of 0x18 bytes
	{
		TaskNode hdr;
		uint16_t counter;  // +0x0C sequence frame
		uint16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t size;      // +0x16
	};
	// the prim-player callback's parameter block (a stack block of the hit task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 the node's position (+0x10..+0x17)
		int32_t scale[3];  // +0x08 entity +0x26 / 4 + 0x200
		uint32_t flag;     // +0x14 never written by the hit task (see STACK WORDS): scale when not 0
		uint32_t morph;    // +0x18 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(BreathNode) == 0x3C && sizeof(HitNode) == 0x140 && sizeof(SmokeNode) == 0x18, "Bad Breath (Quistis) nodes");
	static_assert(sizeof(PrimArg) == 0x1C, "Bad Breath (Quistis) prim block");

	// the stack word the hit task's block +0x14 reads (see STACK WORDS): its last writer's value
	static uint32_t g_hit_flag_word = 0;
	inline uint32_t &HitFlagWord() { return g_hit_flag_word; }

	// ------------------------------------------------------------------
	// 0x6B7CB0 (Breath 129's MAG_129_Breath_NormalizeVertexDepth, called across modules): w of every vertex =
	// z * 4096 / (z range of the vertices 2..count, one past the end)
	// ------------------------------------------------------------------
	static void NormalizeDepth(uint8_t *model)
	{
		const int32_t count = *(const int32_t *)(model + 4);
		int32_t mn = *(const int16_t *)(model + 0x1C), mx = mn;
		const uint8_t *z = model + 0x1C;
		for (int32_t i = count - 1; i; i--, z += 8)
		{
			const int32_t v = *(const int16_t *)z;
			if (mn > v) mn = v;
			else if (mx < v) mx = v;
		}
		const int32_t range = mx - mn;
		uint8_t *p = model + 0xE;
		for (int32_t i = count; i; i--, p += 8)
			*(int16_t *)p = (int16_t)(shl32(*(const int16_t *)(p - 2), 12) / range);
	}

	// ------------------------------------------------------------------
	// 0x66FBB0: the mesh vertices through the GTE (rotation, translation) into magic buffer + 0x85C, each
	// swirled in x / y by the table at (phase + 2 w) with amplitude table[w & 0xFFE] >> 4 (out's 4th words
	// untouched)
	// ------------------------------------------------------------------
	static void SwirlVertices(const uint8_t *model, int32_t phase, int16_t *out)
	{
		int32_t count = *(const int32_t *)(model + 4);
		const int16_t *v = (const int16_t *)(model + 8);
		do
		{
			GteLoadV0(v);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(out);
			const int32_t w = v[3];
			const int32_t a = (phase + w * 2) & 0xFFF;
			const int32_t amp = (int32_t)SWIRL_TABLE[w & 0xFFE] >> 4;
			out[1] = (int16_t)(out[1] - (int16_t)(mul32(SWIRL_TABLE[a * 2], amp) >> 12));
			out[0] = (int16_t)(out[0] + (int16_t)(mul32(SWIRL_TABLE[a * 2 + 1], amp) >> 12));
			out += 4;
			v += 4;
		} while (--count);
	}

	// ------------------------------------------------------------------
	// 0x66FC40: the point v (word +6 = bone) of the caster's bone, her bone matrices rebuilt from her pose
	// (0x508C90), through her world matrix (+0x40) -> out (3 x s16)
	// ------------------------------------------------------------------
	static void BonePoint(const int16_t *v, int16_t *out)
	{
		uint8_t *caster = Caster();
		uint8_t *bones = *(uint8_t **)*(uint8_t **)(caster + 0x64) + 0x10;
		BuildBoneMatricesFromPose(caster + 0x60);
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(caster + 0x40), (const Mat4x3 *)(bones + v[3] * 0x30 + 0x10), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// ------------------------------------------------------------------
	// Mesh draw (0x66F710 -> 131's 0x6B3370 / 0x6B37F0). Header (0x5C, Field_Alloc): +0x00 mesh, +0x04 RGB
	// (scaled per vertex by the list's red byte / 128 through the GTE), +0x0C U scroll (wrapping by the byte
	// +0x1C), +0x10 / +0x18 texture windows, +0x20 vertices, +0x24 list cursor, +0x30 OTZ, +0x34 FLAG. Every
	// quad: texture window 1, draw mode 0xE1000220, the quad, texture window 2.
	// ------------------------------------------------------------------
	static bool OffX(int16_t x) { return x < 0 || x > 0xA00; }
	static bool OffY(int16_t y) { return y < 0 || y > 0x6C0; }

	static void ScrollU(uint8_t *p, const int *off, int n, uint16_t add, uint8_t wrap)
	{
		uint32_t v[4], o = 0;
		for (int k = 0; k < n; k++)
		{
			v[k] = p[off[k]] + (uint32_t)add;
			o |= v[k];
		}
		for (int k = 0; k < n; k++) p[off[k]] = (uint8_t)(o > 0xFF ? v[k] - wrap : v[k]);
	}

	static void Shade(const uint8_t *h, uint8_t red, void *dst)
	{
		GteSetIR0((int32_t)red << 5);
		GteLoadRGBBytes(h + 4);
		GteGPF();
		GteStoreRGBBytes(dst);
	}

	static uint8_t *Insert3(const uint8_t *h, uint32_t otp, uint8_t *main, uint8_t *p)
	{
		uint8_t *t1 = p;
		*(uint32_t *)t1 = 0x02000000;
		*(uint32_t *)(t1 + 4) = TextureWindow(h + 0x10);
		*(uint32_t *)(t1 + 8) = 0;
		InsertPrimAutoDepth(otp, t1);
		uint8_t *md = p + 0xC;
		*(uint32_t *)md = 0x01000000;
		*(uint32_t *)(md + 4) = 0xE1000220;
		InsertPrimAutoDepth(otp, md);
		InsertPrimAutoDepth(otp, main);
		uint8_t *t2 = p + 0x14;
		*(uint32_t *)t2 = 0x02000000;
		*(uint32_t *)(t2 + 4) = TextureWindow(h + 0x18);
		*(uint32_t *)(t2 + 8) = 0;
		InsertPrimAutoDepth(otp, t2);
		return p + 0x20;
	}

	// 0x6B3370 (MAG_131_BadBreath_DrawMeshTexQuads): textured quads
	static uint32_t MeshQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *verts = Cursor(h, 0x20);
		uint8_t *prim = Cursor(h, 0x24);
		int32_t count = *(int32_t *)prim;
		prim += 4;
		Cursor(h, 0x24) = prim;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, prim += 0x18)
		{
			GteLoadV012(verts + 4 * *(uint16_t *)(prim + 4), verts + 4 * *(uint16_t *)(prim + 6), verts + 4 * *(uint16_t *)(prim + 8));
			GteRTPT();
			*(uint32_t *)p = 0x09000000;
			*(uint32_t *)(p + 4) = *(uint32_t *)prim;
			*(uint32_t *)(p + 0xC) = *(uint32_t *)(prim + 0xC);
			*(uint32_t *)(p + 0x14) = *(uint32_t *)(prim + 0x10);
			const uint32_t uv = *(uint32_t *)(prim + 0x14);
			*(uint32_t *)(p + 0x1C) = uv;
			*(uint32_t *)(p + 0x24) = uv >> 16;
			GteReadFLAG(h + 0x34);
			if (*(uint32_t *)(h + 0x34) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x10, p + 0x18);
			GteLoadV0(verts + 4 * *(uint16_t *)(prim + 0xA));
			GteRTPS();
			uint32_t clip = 0;
			if (OffX(*(int16_t *)(p + 8))) clip = 1;
			if (OffX(*(int16_t *)(p + 0x10))) clip |= 2;
			if (OffX(*(int16_t *)(p + 0x18))) clip |= 4;
			if (OffY(*(int16_t *)(p + 0xA))) clip |= 0x10;
			if (OffY(*(int16_t *)(p + 0x12))) clip |= 0x20;
			if (OffY(*(int16_t *)(p + 0x1A))) clip |= 0x40;
			GteReadSXY2(p + 0x20);
			GteAVSZ4();
			if (OffX(*(int16_t *)(p + 0x20))) clip |= 8;
			if (OffY(*(int16_t *)(p + 0x22))) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteReadOTZReg(h + 0x30);
			Shade(h, prim[0], p + 4);
			static const int us[4] = { 0xC, 0x14, 0x1C, 0x24 };
			ScrollU(p, us, 4, *(uint16_t *)(h + 0xC), h[0x1C]);
			const uint32_t otp = ot + 4 * (*(int32_t *)(h + 0x30) >> shift);
			p = Insert3(h, otp, p, p + 0x28);
		}
		Cursor(h, 0x24) = prim;
		return (uint32_t)p;
	}

	// 0x6B37F0 (MAG_131_BadBreath_DrawMeshGouraudTexQuads): Gouraud textured quads
	static uint32_t MeshGouraudQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *verts = Cursor(h, 0x20);
		uint8_t *prim = Cursor(h, 0x24);
		int32_t count = *(int32_t *)prim;
		prim += 4;
		Cursor(h, 0x24) = prim;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, prim += 0x24)
		{
			GteLoadV012(verts + 4 * *(uint16_t *)(prim + 4), verts + 4 * *(uint16_t *)(prim + 6), verts + 4 * *(uint16_t *)(prim + 8));
			GteRTPT();
			*(uint32_t *)p = 0x0C000000;
			*(uint32_t *)(p + 4) = *(uint32_t *)prim;
			*(uint32_t *)(p + 0xC) = *(uint32_t *)(prim + 0xC);
			*(uint32_t *)(p + 0x18) = *(uint32_t *)(prim + 0x10);
			const uint32_t uv = *(uint32_t *)(prim + 0x14);
			*(uint32_t *)(p + 0x24) = uv;
			*(uint32_t *)(p + 0x30) = uv >> 16;
			GteReadFLAG(h + 0x34);
			if (*(uint32_t *)(h + 0x34) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x14, p + 0x20);
			GteLoadV0(verts + 4 * *(uint16_t *)(prim + 0xA));
			GteRTPS();
			uint32_t clip = 0;
			if (OffX(*(int16_t *)(p + 8))) clip = 1;
			if (OffX(*(int16_t *)(p + 0x14))) clip |= 2;
			if (OffX(*(int16_t *)(p + 0x20))) clip |= 4;
			if (OffY(*(int16_t *)(p + 0xA))) clip |= 0x10;
			if (OffY(*(int16_t *)(p + 0x16))) clip |= 0x20;
			if (OffY(*(int16_t *)(p + 0x22))) clip |= 0x40;
			GteReadSXY2(p + 0x2C);
			GteAVSZ4();
			if (OffX(*(int16_t *)(p + 0x2C))) clip |= 8;
			if (OffY(*(int16_t *)(p + 0x2E))) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteReadOTZReg(h + 0x30);
			Shade(h, prim[0], p + 4);
			Shade(h, prim[0x18], p + 0x10);
			Shade(h, prim[0x1C], p + 0x1C);
			Shade(h, prim[0x20], p + 0x28);
			static const int us[4] = { 0xC, 0x18, 0x24, 0x30 };
			ScrollU(p, us, 4, *(uint16_t *)(h + 0xC), h[0x1C]);
			const uint32_t otp = ot + 4 * (*(int32_t *)(h + 0x30) >> shift);
			p = Insert3(h, otp, p, p + 0x34);
		}
		Cursor(h, 0x24) = prim;
		return (uint32_t)p;
	}

	// 0x66F710: both lists of the header's mesh
	static uint32_t MeshDraw(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *m = Cursor(h, 0);
		Cursor(h, 0x24) = m + *(uint32_t *)m + 0xC;
		const uint32_t r = MeshQuads(h, ot, shift, cursor);
		Cursor(h, 0x24) = Cursor(h, 0x24) + 0xC;
		return MeshGouraudQuads(h, ot, shift, r);
	}

	// ------------------------------------------------------------------
	// The breath's time functions (t = counter - 8)
	// ------------------------------------------------------------------
	static int16_t SweepX(int32_t t, int16_t width) { return (int16_t)(mul32(ComputeCos((int32_t)((uint32_t)shl32(t, 11) / 50)), width) >> 12); }

	// length scale: sin(t * 64) over the first 16 ticks, then the full length
	static int32_t Growth(int32_t t) { return (uint32_t)t < 0x10 ? ComputeSin((t & 0x3FFFFF) << 6) : 0x1000; }

	// the mesh colour at t (the header's +0x04..+0x06; +0x07 is not written)
	static void MeshColour(int32_t t, uint8_t *h4)
	{
		const int32_t v = shl32(t, 10);
		const int32_t i = (v >> 12) % 3, f = v & 0xFFF;
		BlendColour(COLOURS + i * 4, COLOURS + i * 4 + 4, 0x1000 - f, f, h4);
		if ((uint32_t)t < 8)
		{
			const int32_t s = ComputeSin((t & 0x3FFFFF) << 7);
			BlendColour(FADE_COLOUR, h4, 0x1000 - s, s, h4);
		}
		else if ((uint32_t)t > 0x22)
		{
			const int32_t s = ComputeSin((shl32(t, 6) - 0x841) & 0xFFFFFC0);
			BlendColour(FADE_COLOUR, h4, s, 0x1000 - s, h4);
		}
	}
}
}

#ifdef FF8_FX_HELD
#include "mag300_bad_breath_quistis_held.h"
#endif

namespace ff8fx
{
namespace badbreath300
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x66F8A0): one object of the hit layout. Vertex frame (or blend into the block's
	// morph buffer), rotation from the record's angles; with the block's word +0x14 set, the record's offset
	// and the rotation scaled by the block's scale; offset + the block's position through the camera's
	// rotation (the GTE), composed with the camera, scaled by the record (scale3DMatrix), drawn by 0x572200.
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
		RotationFromAngles(r->rot, &m);
		// the offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (arg->flag)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
			Scale3DMatrix(&m, arg->scale);
		}
		off[0] = (int16_t)(off[0] + arg->pos[0]);
		off[1] = (int16_t)(off[1] + arg->pos[1]);
		off[2] = (int16_t)(off[2] + arg->pos[2]);
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(off);
		GteMVMVA_RotV0();
		GteReadMAC123(m.t);
		MatrixMultiply(&Camera(), &m);
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
		// +0x18 (depth offset) is not written: the scratch word under the header
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Smoke (0x66FB20, 131's sprite task): sequence 0x103F824 through the shadow-rotation camera, 0x12
	// frames, rising
	// ------------------------------------------------------------------
	static void SmokeDraw(const SmokeNode *p)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(p->pos, p->size, -((int32_t)p->size >> 2));
		*(uint32_t *)h = SEQ_Smoke;
		*(uint16_t *)(h + 4) = p->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SmokeTask(TaskNode *n)
	{
		SmokeNode *p = (SmokeNode *)n;
		HitFlagWord() = (uint32_t)n; // (its saved esi: the executor's node; see STACK WORDS)
		// 30 fps layer: see mag300_bad_breath_quistis_held.inc
		FX_HELD(held_note_smoke(p);)
		SmokeDraw(p);
		p->pos[1] = (int16_t)(p->pos[1] - 0x10);
		p->counter++;
		return (int16_t)p->counter < 0x12 ? 0 : TASK_END;
	}

	// the hit layout's parameter block (0x66F790's stack block; +0x14 = the modelled stack word)
	static void HitArg(const HitNode *t, PrimArg *arg)
	{
		memcpy(arg->pos, t->pos, sizeof(arg->pos));
		const int32_t s = ((int32_t)*(const int16_t *)(Entity(t->slot) + 0x26) >> 2) + 0x200;
		arg->scale[0] = s;
		arg->scale[1] = s;
		arg->scale[2] = s;
		arg->flag = HitFlagWord();
		arg->morph = TexBase() + VERTS;
	}

	// ------------------------------------------------------------------
	// Hit (0x66F790)
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *t = (HitNode *)n;
		uint8_t *entity = Entity(t->slot);
		PrimArg arg;
		HitArg(t, &arg);
		// 30 fps layer: see mag300_bad_breath_quistis_held.inc
		FX_HELD(held_note_hit(t, &arg);)
		prim::play((prim::Layout *)t->layout, PrimCallback, (int)&arg, 0);
		if (t->counter <= 8)
		{
			SmokeNode *s = (SmokeNode *)AddTaskToQueue(QSmoke(), ORIG_SmokeTask);
			if (s)
			{
				const int32_t bone = mul32(CrtRand(), ***(const uint8_t *const *const *)(entity + 0x64)) >> 15;
				GetEffectSpawnPosition(entity, bone, 0, s->pos);
				s->counter = 0;
				s->size = (int16_t)((CrtRand() & 0x1FF) + 0x400);
			}
		}
		if (t->counter == 0x1E) ApplyActionResultToTarget(Ctx()->actions[0].targets + t->idx * TARGET_STRIDE);
		t->counter++;
		return t->counter < 0x25 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// 0x66F750: chain transformation id of every other active party member (slots 0..2: flag 2, not +8 & 3)
	// ------------------------------------------------------------------
	static void PartyReact(int32_t id)
	{
		for (uint8_t *e = (uint8_t *)0x1D972C0; e < (uint8_t *)0x1D97494; e += 0x9C)
			if ((e[0] & 2) && !(e[8] & 3) && e != Caster()) QueueChainTransformation(e, id);
	}

	// ------------------------------------------------------------------
	// Breath (0x66F270)
	// ------------------------------------------------------------------
	static uint32_t __cdecl BreathTask(TaskNode *n)
	{
		BreathNode *e = (BreathNode *)n;
		BonePoint(MOUTH, e->mouth);
		const int32_t t = e->counter - 8;
		if ((uint32_t)t < 0x32)
		{
			// 30 fps layer: see mag300_bad_breath_quistis_held.inc
			FX_HELD(held_note_breath(e);)
			uint8_t *h = AllocHeader(0x5C);
			PacketCursor() = FadeTile(t, 0x40, 0x40, 0, 0xE, 0x16, 0xE, PacketCursor());
			Mat4x3 rot;
			RotZ(e->angle, &rot);
			GteSetRotMatrix(&rot);
			GteZeroTrans();
			const uint32_t verts = TexBase() + VERTS;
			SwirlVertices(MESH1, e->phase, (int16_t *)verts);
			const int16_t x = SweepX(t, e->width);
			const int16_t y = e->y, z = e->z;
			if (e->next < e->count)
			{
				uint8_t *target = e->ents[e->next];
				if ((int32_t)x - 0x12C <= (int32_t)*(const int16_t *)(target + 0x1C))
				{
					HitNode *p = (HitNode *)AddTaskToQueue(QHits(), ORIG_HitTask);
					if (p)
					{
						p->counter = 0;
						p->idx = e->idx[e->next];
						p->slot = (uint8_t)((int32_t)((uint32_t)target - 0x1D972C0u) / 0x9C);
						GetDefaultEffectPosition(target, p->pos);
						p->pos[1] = (int16_t)((int32_t)*(const int16_t *)(target + 0x92) / 2 + *(const int16_t *)(target + 0x1E));
						DecodeModelPrimLayout(MODEL_Hit, p->layout, 0x128);
					}
					e->next++;
				}
			}
			const int32_t dx = (int32_t)x - e->mouth[0];
			const int32_t dy = (int32_t)y - e->mouth[1];
			const int32_t dz = (int32_t)z - e->mouth[2];
			e->length = Sqrt(mul32(dz, dz) + mul32(dy, dy) + mul32(dx, dx)) + 0x200;
			int16_t scale[9] = { 0x1000, 0, 0, 0, 0x1000, 0, 0, 0, (int16_t)e->length };
			MeshColour(t, h + 4);
			if ((uint32_t)t < 0x10) scale[8] = (int16_t)(mul32(scale[8], Growth(t)) >> 12);
			const int32_t v[3] = { dx, dy, -dz };
			Mat4x3 basis;
			BuildOrthonormalBasis(v, &basis);
			MulMatrix3(&basis, scale);
			basis.t[0] = e->mouth[0];
			basis.t[1] = e->mouth[1];
			basis.t[2] = e->mouth[2];
			HitFlagWord() = (uint32_t)(int32_t)e->mouth[0]; // (its local [esp + 0x64]; see STACK WORDS)
			ComposeAffineTransform(&Camera(), &basis, &MeshFrame());
			GteSetRotMatrix(&MeshFrame());
			GteSetTransVector(&MeshFrame());
			*(uint16_t *)(h + 0x16) = 0x100;
			*(uint16_t *)(h + 0x14) = 0x100;
			*(uint16_t *)(h + 0x1E) = 0x100;
			*(uint16_t *)(h + 0x12) = 0;
			*(uint16_t *)(h + 0x10) = 0;
			*(uint16_t *)(h + 0x18) = 0;
			*(uint16_t *)(h + 0x1A) = 0;
			*(uint16_t *)(h + 0x1C) = 0x80;
			*(uint16_t *)(h + 0xC) = (uint16_t)((uint8_t)t * 10 & 0x7F);
			*(uint8_t **)h = MESH1;
			*(uint32_t *)(h + 0x20) = verts;
			PacketCursor() = MeshDraw(h, RenderOT44(), 2, PacketCursor());
			RotZ((int16_t)-e->angle, &rot);
			GteSetRotMatrix(&rot);
			GteZeroTrans();
			SwirlVertices(MESH2, (int32_t)e->phase + 0x400, (int16_t *)verts);
			GteSetRotMatrix(&MeshFrame());
			GteSetTransVector(&MeshFrame());
			*(uint8_t **)h = MESH2;
			PacketCursor() = MeshDraw(h, RenderOT44(), 2, PacketCursor());
			FieldFree(0x5C);
			if (t == 0) PartyReact(2);
			else if (t == 0x31) PartyReact(1);
		}
		if (e->counter == 0xA) BdPlaySE(SOUND_Breath, 0, 0x80);
		e->angle = (int16_t)(e->angle - 0x32);
		e->phase = (int16_t)(e->phase + 0xC8);
		if (e->counter >= 0x3A) return TASK_END;
		e->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x66EFB0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag300_bad_breath_quistis_held.inc
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
			m->started = 1;
			InitTaskQueuePool(QBreath(), (void *)(TexBase() + POOL_Breath), 0x3C, 1);
			InitTaskQueuePool(QHits(), (void *)(TexBase() + POOL_Hits), 0x140, 4);
			InitTaskQueuePool(QSmoke(), (void *)TexBase(), 0x18, 0x20);
			BreathNode *e = (BreathNode *)AddTaskToQueue(QBreath(), ORIG_BreathTask);
			Memset32(&e->counter, 0, 0xC);
			uint8_t *const b = (uint8_t *)e;
			e->count = (int16_t)Ctx()->actions[0].target_count;
			e->width = 0x700;
			e->z = 0;
			e->y = 0;
			// target record indexes (+0x20 + i) and entities (+0x10 + 4 i); the half width = the largest |x| of
			// the targets' default effect positions (at least 0x700), their y / z summed
			for (int32_t i = 0; i < e->count; i++)
			{
				b[0x20 + i] = (uint8_t)i;
				uint8_t *ent = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
				*(uint8_t **)(b + 0x10 + 4 * i) = ent;
				int16_t pos[4];
				GetDefaultEffectPosition(ent, pos);
				int16_t ax = pos[0];
				if (ax < 0) ax = (int16_t)(0u - (uint32_t)*(const int32_t *)pos);
				if (e->width < ax) e->width = ax;
				e->z = (int16_t)(e->z + pos[2]);
				e->y = (int16_t)(e->y + pos[1]);
			}
			e->z = (int16_t)((int32_t)e->z / (int32_t)e->count);
			e->width = (int16_t)(e->width + 0x100);
			e->y = (int16_t)((int32_t)e->y / (int32_t)e->count);
			// sorted by entity x, largest first (the record indexes follow)
			for (int32_t i = 0; i < e->count - 1; i++)
			{
				for (int32_t j = i + 1; j < e->count; j++)
				{
					uint8_t *a = *(uint8_t **)(b + 0x10 + 4 * i);
					uint8_t *c = *(uint8_t **)(b + 0x10 + 4 * j);
					if (*(const int16_t *)(a + 0x1C) < *(const int16_t *)(c + 0x1C))
					{
						*(uint8_t **)(b + 0x10 + 4 * i) = c;
						*(uint8_t **)(b + 0x10 + 4 * j) = a;
						const uint8_t x = b[0x20 + j];
						const uint8_t y = b[0x20 + i];
						b[0x20 + i] = x;
						b[0x20 + j] = y;
					}
				}
			}
			NormalizeDepth(MESH1);
			NormalizeDepth(MESH2);
			BindEntityModelFromComData(Caster(), TexBase() + BIND_Block, COM_Anim);
			QueueChainTransformation(Caster(), 1);
		}
		int a0, a1, a2;
		if (m->started)
		{
			a0 = ExecuteTaskQueue(QBreath());
			a1 = ExecuteTaskQueue(QHits());
			a2 = ExecuteTaskQueue(QSmoke());
		}
		else a0 = a1 = a2 = (int)n;
		if (m->started && a0 == 0 && a1 == 0 && a2 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag300_bad_breath_quistis()
	{
		register_port(badbreath300::ORIG_MasterTask, (void *)badbreath300::MasterTask, "300 MasterTask", 300);
		register_port(badbreath300::ORIG_BreathTask, (void *)badbreath300::BreathTask, "300 BreathTask", 300);
		register_port(badbreath300::ORIG_HitTask, (void *)badbreath300::HitTask, "300 HitTask", 300);
		register_port(badbreath300::ORIG_SmokeTask, (void *)badbreath300::SmokeTask, "300 SmokeTask", 300);
		// 30 fps layer: see mag300_bad_breath_quistis_held.inc
		FX_HELD(register_mag300_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag300_bad_breath_quistis_held.inc"
#endif
