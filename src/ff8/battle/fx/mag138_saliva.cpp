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

// Effect 138: Saliva (enemy attack 175 of kernel.bin, Abyss Worm c0m044; MAG_138_*). No other
// kernel.bin entry uses effect 138.
//
// Structure (setup MAG_138_SALIVA 0x6A9F10 -> _Init 0x6A9F40, file loader 0x6A9F20 = the texture
// file named at 0x11E9194; the setup keeps the CASTER's entity (0x2520C48), starts camera animation
// 0x11E3024 and queues the TIM; setup / loader / _Init are the copies of effects 126..137):
//   MasterTask (0x6A9FC0) - alternates the packet arena (magic buffer + 0x1510 / + 0x11510); on
//     counter 1 sets up the pools (splats 8 x 0x14 at + 0, sprays 8 x 0x20 at + 0xA0, strings 3 x 0x30
//     at + 0x1A0, main 1 x 0x380 at + 0x230), starts the main task (the first target of action 0, its
//     effect point 0xF1 (0x502170), layouts 0x11E1034 / 0x11E0434 / 0x11E21CC), normalises the mesh
//     0x11DF764 (Oil Shot's 0x6AD430: w of every vertex from its z; this static exe table is
//     rewritten on EVERY cast, with the same values) and frees the 0x60 string points (0x20 bytes at
//     + 0x5B0, word +6 = -1). Then the main, string, spray and splat queues; ends when the main AND
//     the splat queues are empty (strings and sprays are abandoned).
//   MainTask (0x6AA1A0) - counters 0..0x37. Counters 0x17..0x24: M = the caster's bone 13 point
//     0x11E3188 (0x6AA7B0, the bone matrices as they are), RotY(-facing); counters 0x17..0x1D: two
//     spray sprites per tick (CRT rand: angle, spin, scale, the direction (Oil Shot's 0x6ADFA0, z
//     the other way) through RotY, its length scaled by IR0 = rand & 0x7F + 0x80); layouts +0x2C and
//     +0xD4 at M (RotY composed with the camera, depth -0x80); counters 0x17..0x19: a string (CRT
//     rand: amplitude). Counters 0x1D..0x36: layout +0x268 at the target point through the camera
//     (depth 0); odd counters 0x21..0x27: a splat sprite (CRT rand offset). Sound at counter 0x19,
//     damage at counter 0x28.
//   StringTask (0x6AA940) - counter 0: its start = M (then its previous point). Counters 1..9, two
//     sub-steps j: t = (2 counter + j) * 2048 / 10, s = sin(t / 2); the mesh 0x11DF764 through
//     RotZ(counter << 8) into magic buffer + 0x11B0, each y lowered by table[w / 2] x (amp x s >> 13)
//     >> 16 (0x6AAC80); P = the start blended to the target by t, y - (amp x s >> 14); the basis of
//     P - prev (y and z the other way), scaled in z by s + 0x800 (word 0x11E3148 of the exe matrix
//     0x11E3138, rewritten every sub-step) at P; on j = 1 composed with the camera and drawn by
//     0x6AACE0 (flat textured quads 0x6AAD20 then Gouraud textured quads 0x6AB110, U scrolled by
//     (counter & 7) << 4); prev = P; a chain point at P (0x6AB520: width 0xC8, fade 0x80, its index).
//     Counters >= 10: every point drips (y + table 0x11E3158[index]). Then (2 points or more) every
//     point fades by 4; the head pair is dropped while both are at 0x20 or less (the task ends below
//     2 points); 3 points or more: projected through the camera (0x704090: depth = SZ3 / 4) and drawn
//     as a ribbon (0x6AB570 / 0x6AB6C0, Gastric Juice's 0x6B90B0 / 0x6B9210 on this module's points).
//   SprayTask (0x6AA890, Oil Shot's 0x6AE300) - sprite sequence 0x11DF4E0 (rotation, scale) through
//     the shadow-rotation camera, 10 frames; position += velocity, rotation += spin.
//   SplatTask (0x6AA810, Oil Shot's 0x6AE190) - sprite sequence 0x11DF604 (scale 0xC00), 13 frames.
//   PrimCallback (0x6AA510, Carbuncle's 0x681FD0 / Eerie Sound Wave's 0x6B46C0 / Breath's 0x6B57B0 on
//     this module's cursor) draws every layout object through 0x7043B0.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws THEN updates (the main counter after its draws, a layout in its
// play, a sprite after its draw, the string's points before their projection and ribbon).
// ENTITY / CAMERA: no task writes an entity position (+0x1C..+0x20 only read) or the camera words
// (the camera animation is the engine's).
// EXE DATA WRITES: the mesh 0x11DF764 (vertex w words, every cast) and the word 0x11E3148 (string
// counters 1..9).
// Module globals: 0x2520C48..0x2520CC0 (caster entity 0x2520C48, texture file 0x2520C4C, magic
// buffer base 0x2520C50, context 0x2520C54, master pool 0x2520C58, packet cursor 0x2520C6C, the
// queues 0x2520C70 splats / 0x2520C80 sprays / 0x2520C90 strings / 0x2520CA0 main / 0x2520CB0 master).
// NEVER-WRITTEN STACK WORDS (the game's executor 0x508420 calls every task of a queue at one esp; the
// master pushes its four queue arguments without popping: main entry esp E, strings E - 4, sprays
// E - 8, splats E - 0xC):
//   - StringTask's P word +6 (string entry esp - 0x32; 0x56CB90 writes 3 words) is copied into the
//     node's previous-point word +0x22 (counters 1..9; never read) and the point's width word (then
//     overwritten). Its last writer is the main task, which runs right before the string queue on
//     those ticks: its `push ebp` (0x6AA1A4) saved the executor's ebp there = the main queue
//     0x2520CA0, high half 0x0252 (StringResidue).
//   - The ribbon's joint direction (0x6AB570 local, string entry esp - 0x5C) is never initialised:
//     the first edge call leaves it unwritten when the head point is hidden or has no screen length,
//     and the next one reads it. Its last writer is 0x704090's local (the SZ3 dword of the last point
//     projected right before): RIBBON residue = GTE SZ3 after the projection loop.
//   - The main task's M word +6 (copied into a spray's scale word, then overwritten), the block
//     matrices' pad words (GTE R33 pad 0x1CA928E only) and the callback's record offset word +6 (GTE
//     VZ0 high half 0x1CA8A16 only) reach no output.

#include "mag_common.h"
#include "mag_ice_common.h"

namespace ff8fx
{
namespace saliva138
{
	using namespace eng;
	using namespace magc;
	using namespace icec;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2520C48); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2520C50); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2520C54); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2520C6C); }
	inline TaskQueue *QSplats() { return (TaskQueue *)0x2520C70; }         // pool: magic buffer, 8 x 0x14
	inline TaskQueue *QSprays() { return (TaskQueue *)0x2520C80; }         // pool: + 0xA0, 8 x 0x20
	inline TaskQueue *QStrings() { return (TaskQueue *)0x2520C90; }        // pool: + 0x1A0, 3 x 0x30
	inline TaskQueue *QMain() { return (TaskQueue *)0x2520CA0; }           // pool: + 0x230, 1 x 0x380

	static const uint32_t ORIG_MasterTask = 0x6A9FC0;
	static const uint32_t ORIG_MainTask = 0x6AA1A0;
	static const uint32_t ORIG_StringTask = 0x6AA940;
	static const uint32_t ORIG_SplatTask = 0x6AA810;
	static const uint32_t ORIG_SprayTask = 0x6AA890;
	static const uint32_t POINTS = 0x5B0;                // magic buffer: 0x60 string points of 0x20 bytes
	static const uint32_t VERTS = 0x11B0;                // magic buffer: mesh vertices / blended vertex frames
	static const uint32_t ARENA_A = 0x1510;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11510;
	static uint8_t *const MESH = (uint8_t *)0x11DF764;   // string mesh: size, count, vertices (w rewritten), lists
	static const uint32_t MODEL_A = 0x11E1034;           // prim-model layouts
	static const uint32_t MODEL_B = 0x11E0434;
	static const uint32_t MODEL_C = 0x11E21CC;
	static const uint32_t SEQ_Spray = 0x11DF4E0;         // sprite sequences
	static const uint32_t SEQ_Splat = 0x11DF604;
	static const int16_t *const POINT = (const int16_t *)0x11E3188;   // caster bone point (x, y, z, bone 13)
	static const void *const SCALE_Z = (const void *)0x11E3138;       // exe matrix, its m[2][2] (0x11E3148) rewritten
	static int16_t &ScaleZ() { return *(int16_t *)0x11E3148; }
	static const int16_t *const DRIP = (const int16_t *)0x11E3158;    // y step by point index
	static const int32_t *const WOBBLE = (const int32_t *)0x11E3190;  // 0x801 x s32, by vertex w / 2
	static const uint8_t *const DIST_TABLE = (const uint8_t *)0x11E5194; // 128 x 128 screen lengths
	static const void *const SOUND_Spit = (const void *)0x11E3134;

	// engine functions not in fx_port.h / mag_common.h / mag_ice_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAngles2(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // sub_56C220: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }   // MAG_063_sub_701270
	inline void RotZ(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x7012C0)(angle, out); }   // MAG_065_sub_7012C0
	inline void MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
	inline void BlendVectors(const int16_t *a, const int16_t *b, int32_t wa, int32_t wb, int16_t *out) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int16_t *)>(0x56CB90)(a, b, wa, wb, out); }
	inline void GteZeroTrans() { fn<void (__cdecl *)()>(0x704070)(); }      // MAG_129_sub_704070: TRX = TRY = TRZ = 0
	inline void GteReadSZ3Quarter(void *dst) { fn<void (__cdecl *)(void *)>(0x704090)(dst); } // MAG_138_sub_704090: s16 = SZ3 >> 2
	inline uint32_t GteSZ3() { return *(const uint32_t *)0x1CA8A5C; }       // GTE data register 0x13 (SZ3)

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2520C58)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t started;   // +0x0E pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x380 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint8_t *target;   // +0x10 first target of action 0 (not read)
		uint8_t pad14[8];
		int16_t pos[4];    // +0x1C the target's effect point 0xF1
		uint8_t pad24[8];
		uint8_t layA[0xA8];   // +0x2C layout 0x11E1034
		uint8_t layB[0x194];  // +0xD4 layout 0x11E0434
		uint8_t layC[0x118];  // +0x268 layout 0x11E21CC
	};
	struct Point // 0x60 points of 0x20 bytes at magic buffer + 0x5B0
	{
		int16_t pos[3];    // +0x00
		int16_t width;     // +0x06 0xC8; -1 = free
		int16_t sx, sy;    // +0x08 screen point, then its edge vertex 1
		int16_t otz;       // +0x0C SZ3 / 4 (-1: no screen length)
		int16_t idx;       // +0x0E index in its string (drip table)
		int16_t sx2, sy2;  // +0x10 edge vertex 2
		uint8_t pad14[4];
		int16_t fade;      // +0x18 0x80 down to 0 (grey level)
		int16_t zero;      // +0x1A
		Point *next;       // +0x1C
	};
	struct StringNode // pool of 3 nodes of 0x30 bytes
	{
		TaskNode hdr;
		int16_t start[3];  // +0x0C the caster's bone point at counter 0
		int16_t counter;   // +0x12
		int16_t target[3]; // +0x14 the main task's target point
		int16_t amp;       // +0x1A wave amplitude (CRT rand)
		int16_t prev[4];   // +0x1C the last point (+0x22: a stack word, see StringResidue)
		int16_t count;     // +0x24 chain points
		int16_t pad26;
		Point *head;       // +0x28
		Point *tail;       // +0x2C
	};
	struct SplatNode // pool of 8 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t frame;     // +0x12 (also the 4th word of pos)
	};
	struct SprayNode // pool of 8 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t angle;     // +0x0C sprite rotation
		int16_t spin;      // +0x0E
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16
		int16_t vel[3];    // +0x18
		int16_t frame;     // +0x1E
	};
	// the prim-player callback's parameter block (a stack block of the playing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 rotation (pad word +0x12: R33 pad only) and anchor
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x380 && sizeof(StringNode) == 0x30, "Saliva nodes");
	static_assert(sizeof(Point) == 0x20 && sizeof(SplatNode) == 0x14 && sizeof(SprayNode) == 0x20, "Saliva nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Saliva prim block");

	static uint8_t *&Cursor(uint8_t *h, int off) { return *(uint8_t **)(h + off); }
	static bool OffX(int16_t x) { return x < 0 || x > 0xA00; }
	static bool OffY(int16_t y) { return y < 0 || y > 0x6C0; }

	// ------------------------------------------------------------------
	// Mesh renderer (0x6AACE0, Bad Breath's 0x6B3330 -> Oil Blast's list renderers on other header
	// offsets). Header (0x5C, the string task's): +0x00 mesh, +0x04 vertices, +0x10 U scroll, +0x14 /
	// +0x1C texture windows, +0x20 U wrap byte, +0x24 list cursor, +0x30 OTZ, +0x34 FLAG.
	// ------------------------------------------------------------------
	static void ScrollU(uint8_t *p, const int *off, uint16_t add, uint8_t wrap)
	{
		uint32_t v[4], o = 0;
		for (int k = 0; k < 4; k++)
		{
			v[k] = p[off[k]] + (uint32_t)add;
			o |= v[k];
		}
		for (int k = 0; k < 4; k++) p[off[k]] = (uint8_t)(o > 0xFF ? v[k] - wrap : v[k]);
	}

	// the main packet between the texture windows +0x14 / +0x1C (no draw mode)
	static uint8_t *Windowed(const uint8_t *h, uint32_t otp, uint8_t *main, uint8_t *p)
	{
		uint8_t *t1 = p;
		*(uint32_t *)t1 = 0x02000000;
		*(uint32_t *)(t1 + 4) = TextureWindow(h + 0x14);
		*(uint32_t *)(t1 + 8) = 0;
		InsertPrimAutoDepth(otp, t1);
		InsertPrimAutoDepth(otp, main);
		uint8_t *t2 = p + 0xC;
		*(uint32_t *)t2 = 0x02000000;
		*(uint32_t *)(t2 + 4) = TextureWindow(h + 0x1C);
		*(uint32_t *)(t2 + 8) = 0;
		InsertPrimAutoDepth(otp, t2);
		return p + 0x18;
	}

	// a quad's screen clip bits (x 1 / 2 / 4 / 8, y 0x10 / 0x20 / 0x40 / 0x80): drawn unless all four off
	// one side
	static uint32_t TriClip(const uint8_t *a, const uint8_t *b, const uint8_t *c)
	{
		uint32_t clip = 0;
		if (OffX(*(const int16_t *)a)) clip = 1;
		if (OffX(*(const int16_t *)b)) clip |= 2;
		if (OffX(*(const int16_t *)c)) clip |= 4;
		if (OffY(*(const int16_t *)(a + 2))) clip |= 0x10;
		if (OffY(*(const int16_t *)(b + 2))) clip |= 0x20;
		if (OffY(*(const int16_t *)(c + 2))) clip |= 0x40;
		return clip;
	}
	static bool QuadClipped(uint32_t clip3, const uint8_t *x3)
	{
		uint32_t clip = clip3;
		if (OffX(*(const int16_t *)x3)) clip |= 8;
		if (OffY(*(const int16_t *)(x3 + 2))) clip |= 0x80;
		return (clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0;
	}

	// 0x6AAD20 (Oil Blast's 0x6AC380): flat textured quads (0x18-byte records, 0x28-byte packets)
	static uint32_t FlatTexQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x24);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		uint8_t *verts = Cursor(h, 4);
		Cursor(h, 0x24) = rec;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x18)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)p = 0x09000000;
			*(uint32_t *)(p + 4) = *(const uint32_t *)rec;
			*(uint32_t *)(p + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(p + 0x14) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			*(uint32_t *)(p + 0x1C) = uv;
			*(uint32_t *)(p + 0x24) = uv >> 16;
			GteReadFLAG(h + 0x34);
			if (*(const uint32_t *)(h + 0x34) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x10, p + 0x18);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			const uint32_t clip = TriClip(p + 8, p + 0x10, p + 0x18);
			GteReadSXY2(p + 0x20);
			GteAVSZ4();
			if (QuadClipped(clip, p + 0x20)) continue;
			GteReadOTZReg(h + 0x30);
			static const int us[4] = { 0xC, 0x14, 0x1C, 0x24 };
			ScrollU(p, us, *(const uint16_t *)(h + 0x10), h[0x20]);
			const uint32_t otp = ot + 4 * (*(const int32_t *)(h + 0x30) >> shift);
			p = Windowed(h, otp, p, p + 0x28);
		}
		Cursor(h, 0x24) = rec;
		return (uint32_t)p;
	}

	// 0x6AB110 (Oil Blast's 0x6ACB30): Gouraud textured quads (0x24-byte records, 0x34-byte packets)
	static uint32_t GouraudTexQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x24);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		uint8_t *verts = Cursor(h, 4);
		Cursor(h, 0x24) = rec;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x24)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)p = 0x0C000000;
			*(uint32_t *)(p + 4) = *(const uint32_t *)rec;
			*(uint32_t *)(p + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(p + 0x18) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			*(uint32_t *)(p + 0x24) = uv;
			*(uint32_t *)(p + 0x30) = uv >> 16;
			GteReadFLAG(h + 0x34);
			if (*(const uint32_t *)(h + 0x34) & 0x60000) continue;
			GteReadSXY012(p + 8, p + 0x14, p + 0x20);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			const uint32_t clip = TriClip(p + 8, p + 0x14, p + 0x20);
			GteReadSXY2(p + 0x2C);
			GteAVSZ4();
			if (QuadClipped(clip, p + 0x2C)) continue;
			GteReadOTZReg(h + 0x30);
			*(uint32_t *)(p + 0x10) = *(const uint32_t *)(rec + 0x18);
			*(uint32_t *)(p + 0x1C) = *(const uint32_t *)(rec + 0x1C);
			*(uint32_t *)(p + 0x28) = *(const uint32_t *)(rec + 0x20);
			static const int us[4] = { 0xC, 0x18, 0x24, 0x30 };
			ScrollU(p, us, *(const uint16_t *)(h + 0x10), h[0x20]);
			const uint32_t otp = ot + 4 * (*(const int32_t *)(h + 0x30) >> shift);
			p = Windowed(h, otp, p, p + 0x34);
		}
		Cursor(h, 0x24) = rec;
		return (uint32_t)p;
	}

	// MAG_138_sub_6AACE0: the first list (flat textured quads), then the third (Gouraud textured quads)
	static uint32_t MeshDraw(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *m = Cursor(h, 0);
		Cursor(h, 0x24) = m + *(const uint32_t *)m + 0xC;
		const uint32_t r = FlatTexQuads(h, ot, shift, cursor);
		Cursor(h, 0x24) += 0xC;
		return GouraudTexQuads(h, ot, shift, r);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6AA510, Eerie Sound Wave's 0x6B46C0 on this module's cursor): one object
	// of a layout. Vertex frame (or blend into the parameter block's morph buffer), rotation from the
	// record's angles, the record's offset through the parameter matrix (flag 0x200: as is), the
	// matrix composed with it, scale (flag 0x100: 3x3 product, else scale3DMatrix), drawn by 0x7043B0.
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
		if (r->flags & 0x40000) RotationFromAngles2(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		int32_t t[3];
		if (r->flags & 0x200)
		{
			t[0] = off[0];
			t[1] = off[1];
			t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
			t[0] = m.t[0];
			t[1] = m.t[1];
			t[2] = m.t[2];
		}
		m.t[0] = (int32_t)((uint32_t)t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				int16_t s[9] = { r->scale[0], 0, 0, 0, r->scale[1], 0, 0, 0, r->scale[2] };
				MulMatrix3(&m, s);
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
		PacketCursor() = RenderPrim2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------
	// Oil Shot's 0x6AD430 (MAG_136_sub_6AD430): w of every vertex = (z - min) * 4096 / (max - min), the z
	// range over the vertices 2..count (one past the end)
	static void NormalizeMesh(uint8_t *model)
	{
		const int32_t count = *(const int32_t *)(model + 4);
		int32_t mx = *(const int16_t *)(model + 0x1C), mn = mx;
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
			*(int16_t *)p = (int16_t)(shl32(*(const int16_t *)(p - 2) - mn, 12) / range);
	}

	// Oil Shot's 0x6ADFA0 (MAG_136_sub_6ADFA0): unit vector of (a, b): (-sin a sin b, sin a cos b,
	// cos a) (z negated by the caller)
	static void AngleVector(int32_t a, int32_t b, int16_t *out)
	{
		out[2] = (int16_t)ComputeCos(a);
		const int32_t s = -ComputeSin(a);
		out[0] = (int16_t)(mul32(ComputeSin(b), s) >> 12);
		out[1] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeCos(b), s)) >> 12);
	}

	// MAG_138_sub_6AA7B0 (= Hypnotize's 0x6B18A0): a point through the caster's bone (word +6)
	// composed with its matrix (the bone matrices as they are)
	static void BonePoint(const int16_t *v, int16_t *out)
	{
		uint8_t *c = Caster();
		Mat4x3 m = {};
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(**(uint8_t ***)(c + 0x64) + 0x20 + (int32_t)v[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// MAG_138_sub_6AAC80 (Oil Shot's 0x6ADFF0 on this module's buffer / table): the mesh vertices
	// through the GTE (rotation + translation) into out, each y lowered by WOBBLE[w / 2] x amp >> 16
	static void WobbleMesh(const uint8_t *model, int32_t amp, int16_t *out)
	{
		int32_t count = *(const int32_t *)(model + 4);
		const int16_t *v = (const int16_t *)(model + 8);
		do
		{
			GteLoadV0(v);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(out);
			const int32_t w = v[3] >> 1;
			out[1] = (int16_t)(out[1] - (int16_t)(mul32(WOBBLE[w], amp) >> 0x10));
			out += 4;
			v += 4;
		} while (--count);
	}

	// the string's progress t at sub-step x (= 2 counter + j): x * 2048 / 10
	static int32_t StringT(int32_t x) { return shl32(x, 11) / 10; }

	// the string point at progress t: the start blended to the target, y - (amp x sin(t / 2) >> 14)
	static void StringPoint(const StringNode *s, int32_t t, int32_t wave, int16_t *P)
	{
		BlendVectors(s->start, s->target, 0x1000 - t, t, P);
		P[1] = (int16_t)(P[1] - (int16_t)(wave >> 14));
	}

	// the basis of P - prev (y and z the other way)
	static int32_t StringBasis(const int16_t *P, const int16_t *prev, Mat4x3 *M)
	{
		const int32_t v[3] = { (int32_t)P[0] - prev[0], (int32_t)prev[1] - P[1], (int32_t)prev[2] - P[2] };
		return BuildOrthonormalBasis(v, M);
	}

	// the mesh: the basis M (scaled, at P) composed with the camera, drawn by 0x6AACE0 (U scroll u)
	static void MeshRender(Mat4x3 *M, uint16_t u, uint32_t verts)
	{
		ComposeAffineTransform(&Camera(), M, M);
		GteSetRotMatrix(M);
		GteSetTransVector(M);
		uint8_t *h = (uint8_t *)FieldAlloc(0x5C);
		*(uint8_t **)h = MESH;
		*(uint32_t *)(h + 4) = verts;
		*(uint16_t *)(h + 0x16) = 0;
		*(uint16_t *)(h + 0x14) = 0;
		*(uint16_t *)(h + 0x1A) = 0x100;
		*(uint16_t *)(h + 0x18) = 0x100;
		*(uint16_t *)(h + 0x1E) = 0;
		*(uint16_t *)(h + 0x1C) = 0;
		*(uint16_t *)(h + 0x20) = 0x80;
		*(uint16_t *)(h + 0x22) = 0x100;
		*(uint16_t *)(h + 0x10) = u;
		PacketCursor() = MeshDraw(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x5C);
	}

	// MAG_138_sub_6AB520: a free point (word +6 == -1) at pos (8 bytes copied), width w, fade 0x80
	static Point *AllocPoint(int16_t w, const int16_t *pos)
	{
		Point *p = (Point *)(TexBase() + POINTS);
		int n = 0x60;
		while (p->width != -1)
		{
			p++;
			if (--n == 0) return nullptr;
		}
		memcpy(p->pos, pos, 8);
		p->width = w;
		p->fade = 0x80;
		p->zero = 0;
		p->next = nullptr;
		return p;
	}

	// MAG_138_sub_6AB6C0 (Gastric Juice's 0x6B9210 on this module's points): the two edge vertices of
	// point p from the screen direction towards n (averaged with the previous one: mitred joints),
	// half width = n's width * 32 / (p's depth + 0x1000); a point without n has no width
	static void RibbonEdges(Point *p, const Point *n, const int16_t *prev, int16_t *dir)
	{
		if (p->otz < 0) return;
		int32_t ox = 0, oy = 0;
		if (n)
		{
			if (n->otz < 0) return;
			const int32_t dx = n->sx - p->sx;
			const int32_t ndy = p->sy - n->sy;
			int32_t adx = dx < 0 ? -dx : dx, ady = ndy < 0 ? -ndy : ndy;
			if (adx >= 0x80) adx = 0x7F;
			if (ady >= 0x80) ady = 0x7F;
			const int32_t len = DIST_TABLE[ady * 128 + adx];
			int32_t px, py; // unit normal (4.12)
			if (prev)
			{
				if (len == 0)
				{
					dir[0] = prev[0];
					dir[1] = prev[1];
					px = prev[0];
					py = prev[1];
				}
				else
				{
					const int32_t cx = shl32(ndy, 12) / len, cy = shl32(dx, 12) / len;
					int32_t sx = cx + prev[0], sy = cy + prev[1];
					dir[0] = (int16_t)cx;
					dir[1] = (int16_t)cy;
					if (sx == 0 && sy == 0)
					{
						sx = cx * 2;
						sy = cy * 2;
					}
					const int32_t sq = (int32_t)((uint32_t)mul32(sx, sx) + (uint32_t)mul32(sy, sy)); // wraps, as the original
					const int32_t k = 0x4000 - (sq >> 13); // one Newton step of 1/|v|
					px = mul32(k, sx) >> 13;
					py = mul32(k, sy) >> 13;
				}
			}
			else
			{
				if (len == 0)
				{
					p->otz = -1;
					return;
				}
				px = shl32(ndy, 12) / len;
				py = shl32(dx, 12) / len;
				dir[0] = (int16_t)px;
				dir[1] = (int16_t)py;
			}
			const int32_t w = shl32(n->width, 5) / (p->otz + 0x1000);
			ox = mul32(w, px) >> 12;
			oy = mul32(w, py) >> 12;
		}
		const int16_t x = p->sx, y = p->sy;
		p->sx = (int16_t)(x - ox);
		p->sx2 = (int16_t)(x + ox);
		p->sy2 = (int16_t)(y + oy);
		p->sy = (int16_t)(y - oy);
	}

	// MAG_138_sub_6AB570 (Gastric Juice's 0x6B90B0): the edges of the chain, then one GP0 0x3E quad
	// (gouraud, textured, semi-transparent) per pair of visible points, grey level = fade.
	// UNINITIALISED READ: the joint direction is a stack local the function never initialises; when
	// the first point is hidden or has no screen length the second point reads it. Its slot (string
	// entry esp - 0x5C) is 0x704090's local of the last point projected right before: `residue` = that
	// SZ3 dword (low word = x, high word = y).
	static void DrawRibbon(Point *head, int32_t count, uint32_t residue)
	{
		int16_t dir[2] = { (int16_t)(residue & 0xFFFF), (int16_t)(residue >> 16) };
		RibbonEdges(head, head->next, nullptr, dir);
		Point *p = head->next;
		for (int32_t i = count - 2; i != 0; i--)
		{
			RibbonEdges(p, p->next, dir, dir);
			p = p->next;
		}
		RibbonEdges(p, nullptr, dir, nullptr);
		Point *a = head;
		for (int32_t i = count - 1; i != 0; i--)
		{
			Point *b = a->next;
			if (a->otz > 0 && b->otz > 0)
			{
				uint32_t *pk = (uint32_t *)PacketCursor();
				PacketCursor() += 0x34;
				pk[2] = *(const uint32_t *)&a->sx;
				pk[5] = *(const uint32_t *)&a->sx2;
				pk[8] = *(const uint32_t *)&b->sx;
				pk[11] = *(const uint32_t *)&b->sx2;
				uint32_t ca = (uint32_t)(int32_t)a->fade;
				ca = (((ca << 8) | ca) << 8) | ca;
				pk[4] = ca;
				pk[1] = ca;
				uint32_t cb = (uint32_t)(int32_t)b->fade;
				pk[3] = 0x3C14C008;  // uv0 + CLUT
				cb = (((cb << 8) | cb) << 8) | cb;
				pk[6] = 0x00BAC038;  // uv1 + texture page
				*(uint16_t *)((uint8_t *)pk + 0x24) = 0xC008; // uv2
				pk[10] = cb;
				pk[7] = cb;
				*(uint16_t *)((uint8_t *)pk + 0x30) = 0xC038; // uv3
				pk[0] = 0x0C000000;
				((uint8_t *)pk)[7] = 0x3E;
				InsertPrimAutoDepth(RenderOT() + 4 * ((a->otz + b->otz) >> 3), pk);
			}
			a = b;
		}
	}

	// the chain's projection (camera; depth = SZ3 / 4), then its ribbon from the projection's residue
	static void StringDraw(Point *head, int32_t count)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		Point *p = head;
		do
		{
			GteLoadV0(p);
			GteRTPS();
			GteReadSXY2(&p->sx);
			GteReadSZ3Quarter(&p->otz);
			p = p->next;
		} while (p);
		DrawRibbon(head, count, GteSZ3());
	}

	// StringTask's P word +6 (entry esp - 0x32, never written: 0x56CB90 stores 3 words), copied into
	// the node's previous-point word +0x22 on counters 1..9 (never read afterwards). The main task runs
	// right before the string queue on those ticks (main counters 0x18..0x22): its saved ebp sits
	// there, the game executor's queue pointer 0x2520CA0 (high half).
	static int16_t StringResidue()
	{
		return 0x0252;
	}

	static void Play(void *layout, const PrimArg *arg)
	{
		prim::play((prim::Layout *)layout, PrimCallback, (int)arg, 0);
	}

	// the block at a point through the camera: the camera rotation, the point through it + the
	// camera translation
	static void BlockAt(PrimArg *arg, const int16_t *p4)
	{
		memcpy(arg->m.m, Camera().m, sizeof(arg->m.m));
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(p4);
		GteMVMVA_RotV0();
		GteReadMAC123(arg->m.t);
		arg->m.t[0] = (int32_t)((uint32_t)arg->m.t[0] + (uint32_t)Camera().t[0]);
		arg->m.t[1] = (int32_t)((uint32_t)arg->m.t[1] + (uint32_t)Camera().t[1]);
		arg->m.t[2] = (int32_t)((uint32_t)arg->m.t[2] + (uint32_t)Camera().t[2]);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag138_saliva_held.h"
#endif

namespace ff8fx
{
namespace saliva138
{
	// ------------------------------------------------------------------
	// Sprites
	// ------------------------------------------------------------------
	// 0x6AA890 (Oil Shot's 0x6AE300): sprite sequence 0x11DF4E0 (rotation, scale) through the
	// shadow-rotation camera, 10 frames
	static void SprayDraw(const int16_t *pos, int16_t scale, int16_t frame, int16_t angle)
	{
		TransformCameraByShadowRotation(pos, scale, -(scale >> 2));
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Spray;
		*(int16_t *)(h + 4) = frame;
		*(int32_t *)(h + 8) = angle;
		*(int16_t *)(h + 0x24) = 1;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SprayTask(TaskNode *n)
	{
		SprayNode *s = (SprayNode *)n;
		// 30 fps layer: see mag138_saliva_held.inc
		FX_HELD(held_note_sprite(SPR_SPRAY, n);)
		SprayDraw(s->pos, s->scale, s->frame, s->angle);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->angle = (int16_t)(s->angle + s->spin);
		s->frame++;
		return s->frame < 0xA ? 0 : TASK_END;
	}

	// 0x6AA810 (Oil Shot's 0x6AE190): sprite sequence 0x11DF604 through the shadow-rotation camera,
	// 13 frames
	static void SplatDraw(const int16_t *pos, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, 0xC00, -0x300);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Splat;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SplatTask(TaskNode *n)
	{
		SplatNode *s = (SplatNode *)n;
		// 30 fps layer: see mag138_saliva_held.inc
		FX_HELD(held_note_sprite(SPR_SPLAT, n);)
		SplatDraw(s->pos, s->frame);
		s->frame++;
		return s->frame < 0xD ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// String (0x6AA940)
	// ------------------------------------------------------------------
	static uint32_t __cdecl StringTask(TaskNode *n)
	{
		StringNode *s = (StringNode *)n;
		if (s->counter == 0)
		{
			BonePoint(POINT, s->start);
			const int16_t c = s->counter;
			s->counter++;
			memcpy(s->prev, s->start, 6);
			s->prev[3] = c;
			return 0;
		}
		if (s->counter < 0xA)
		{
			for (int32_t j = 0; j < 2; j++)
			{
				const int32_t t = StringT(j + s->counter * 2);
				const int32_t sine = ComputeSin(t >> 1);
				Mat4x3 M;      // entry esp - 0x20: RotZ, then the string basis
				RotZ((int16_t)((s->counter & 0xFF) << 8), &M);
				GteSetRotMatrix(&M);
				GteZeroTrans();
				const int32_t wave = mul32(s->amp, sine);
				WobbleMesh(MESH, wave >> 13, (int16_t *)(TexBase() + VERTS));
				int16_t P[4];  // entry esp - 0x38
				P[3] = StringResidue();
				StringPoint(s, t, wave, P);
				StringBasis(P, s->prev, &M);
				ScaleZ() = (int16_t)(sine + 0x800);
				MulMatrix3(&M, SCALE_Z);
				M.t[0] = P[0];
				M.t[1] = P[1];
				M.t[2] = P[2];
				if (j == 1)
				{
					// 30 fps layer: see mag138_saliva_held.inc
					FX_HELD(held_note_mesh(n);)
					MeshRender(&M, (uint16_t)((s->counter & 7) << 4), TexBase() + VERTS);
				}
				memcpy(s->prev, P, 8);
				Point *p = AllocPoint(0xC8, P);
				if (s->count == 0)
				{
					s->tail = p;
					s->head = p;
				}
				else
				{
					s->tail->next = p;
					s->tail = p;
				}
				p->idx = s->count;
				s->count++;
			}
		}
		else
		{
			Point *p = s->head;
			do
			{
				p->pos[1] = (int16_t)(p->pos[1] + DRIP[p->idx]);
				p = p->next;
			} while (p);
		}
		if (s->count >= 2)
		{
			Point *p = s->head;
			do
			{
				p->fade = (int16_t)(p->fade - 4);
				if (p->fade < 0) p->fade = 0;
				p = p->next;
			} while (p);
			p = s->head;
			if (p->fade <= 0x20)
			{
				for (;;)
				{
					Point *q = p->next;
					if (q->fade > 0x20) break;
					p->width = -1;
					s->count--;
					s->head = q;
					if (s->count < 2) return TASK_END;
					p = q;
					if (p->fade > 0x20) break;
				}
			}
		}
		if (s->count >= 3)
		{
			// 30 fps layer: see mag138_saliva_held.inc
			FX_HELD(held_note_ribbon(n);)
			StringDraw(s->head, s->count);
		}
		s->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Main (0x6AA1A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *w = (MainNode *)n;
		int16_t M4[4];     // entry esp - 0x30: the bone point (4th word: never written, see the module comment)
		M4[3] = 0;
		PrimArg arg;       // entry esp - 0x28
		arg.m.pad = 0;
		const int32_t e = w->counter - 0x17;
		if ((uint32_t)e < 0xE)
		{
			BonePoint(POINT, M4);
			RotY((int16_t)-*(const int16_t *)(Caster() + 0xE), &arg.m);
			if ((uint32_t)e < 7)
			{
				for (int i = 2; i; i--)
				{
					SprayNode *s = (SprayNode *)AddTaskToQueue(QSprays(), ORIG_SprayTask);
					if (!s) continue;
					s->angle = (int16_t)CrtRand();
					s->spin = (int16_t)((CrtRand() & 0x1FF) - 0x100);
					memcpy(s->pos, M4, 8);
					s->scale = (int16_t)((CrtRand() & 0x3FF) + 0x400);
					const int32_t b = CrtRand();
					const int32_t a = (CrtRand() & 0x3FF) - 0x200;
					AngleVector(a, b, s->vel);
					s->vel[2] = (int16_t)-s->vel[2];
					MatrixMultiplyVector(&arg.m, s->vel, s->vel);
					GteSetIR0((CrtRand() & 0x7F) + 0x80);
					GteLoadIR123(s->vel);
					GteGPF();
					GteStoreIR123(s->vel);
					s->frame = 0;
				}
			}
			arg.m.t[0] = M4[0];
			arg.m.t[1] = M4[1];
			arg.m.t[2] = M4[2];
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			arg.morph = TexBase() + VERTS;
			arg.depth = -0x80;
			// 30 fps layer: see mag138_saliva_held.inc
			FX_HELD(held_note_play(PLAY_A, n, (prim::Layout *)w->layA, M4);)
			Play(w->layA, &arg);
			// 30 fps layer: see mag138_saliva_held.inc
			FX_HELD(held_note_play(PLAY_B, n, (prim::Layout *)w->layB, M4);)
			Play(w->layB, &arg);
			if ((uint32_t)e < 3)
			{
				StringNode *s = (StringNode *)AddTaskToQueue(QStrings(), ORIG_StringTask);
				s->counter = 0;
				memcpy(s->target, w->pos, 8);
				const int32_t r = CrtRand();
				s->count = 0;
				s->amp = (int16_t)((r & 0x7FF) + 0x800);
			}
		}
		const int32_t h = w->counter - 0x1D;
		if ((uint32_t)h < 0x1A)
		{
			BlockAt(&arg, w->pos);
			arg.morph = TexBase() + VERTS;
			arg.depth = 0;
			// 30 fps layer: see mag138_saliva_held.inc
			FX_HELD(held_note_play(PLAY_C, n, (prim::Layout *)w->layC, w->pos);)
			Play(w->layC, &arg);
			if ((uint32_t)h >= 4 && (uint32_t)h < 0xC && !(h & 1))
			{
				SplatNode *s = (SplatNode *)AddTaskToQueue(QSplats(), ORIG_SplatTask);
				if (s)
				{
					memcpy(s->pos, w->pos, 8);
					s->pos[0] = (int16_t)(s->pos[0] + ((CrtRand() & 0x1FF) - 0x100));
					s->pos[1] = (int16_t)(s->pos[1] + ((CrtRand() & 0x1FF) - 0x12C));
					const int32_t r = CrtRand();
					s->frame = 0;
					s->pos[2] = (int16_t)(s->pos[2] + ((r & 0x1FF) - 0x100));
				}
			}
		}
		if (w->counter == 0x19) BdPlaySE(SOUND_Spit, 0, 0x80);
		if (w->counter == 0x28) ApplyActionResultToTarget(Ctx()->actions->targets);
		if (w->counter >= 0x37) return TASK_END;
		w->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x6A9FC0)
	// ------------------------------------------------------------------
	static void StartMain(MasterNode *m)
	{
		uint8_t *target = Entity(Ctx()->actions->targets[0]);
		m->started = 1;
		InitTaskQueuePool(QMain(), (void *)(TexBase() + 0x230), 0x380, 1);
		InitTaskQueuePool(QStrings(), (void *)(TexBase() + 0x1A0), 0x30, 3);
		InitTaskQueuePool(QSprays(), (void *)(TexBase() + 0xA0), 0x20, 8);
		InitTaskQueuePool(QSplats(), (void *)TexBase(), 0x14, 8);
		MainNode *w = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
		Memset32(&w->counter, 0, 0xDD);
		w->target = target;
		GetEffectSpawnPosition(target, 0xF1, 0, w->pos);
		DecodeModelPrimLayout(MODEL_A, w->layA, 0xA8);
		DecodeModelPrimLayout(MODEL_B, w->layB, 0x194);
		DecodeModelPrimLayout(MODEL_C, w->layC, 0x118);
		NormalizeMesh(MESH);
		Point *p = (Point *)(TexBase() + POINTS);
		for (int i = 0x60; i; i--, p++) p->width = -1;
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag138_saliva_held.inc
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
		if (m->counter == 1) StartMain(m);
		int r_main, r_splat;
		if (m->started)
		{
			r_main = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QStrings());
			ExecuteTaskQueue(QSprays());
			r_splat = ExecuteTaskQueue(QSplats());
		}
		else r_main = r_splat = (int)n;
		if (m->started && r_main == 0 && r_splat == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag138_saliva()
	{
		register_port(saliva138::ORIG_MasterTask, (void *)saliva138::MasterTask, "138 MasterTask", 138);
		register_port(saliva138::ORIG_MainTask, (void *)saliva138::MainTask, "138 MainTask", 138);
		register_port(saliva138::ORIG_StringTask, (void *)saliva138::StringTask, "138 StringTask", 138);
		register_port(saliva138::ORIG_SprayTask, (void *)saliva138::SprayTask, "138 SprayTask", 138);
		register_port(saliva138::ORIG_SplatTask, (void *)saliva138::SplatTask, "138 SplatTask", 138);
		// 30 fps layer: see mag138_saliva_held.inc
		FX_HELD(register_mag138_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag138_saliva_held.inc"
#endif
