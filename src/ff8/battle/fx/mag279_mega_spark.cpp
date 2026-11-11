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

// Effect 279: Mega Spark (enemy attack 106 of kernel.bin; Tri-Point c0m030; MAG_279_*).
//
// Structure (setup 0x67EFD0 -> _Init 0x67F000, file loader 0x67EFE0 = mag278.tim (name at 0x10B6F88) into 0x2508070; _Init
// keeps the context (0x2508074) and the CASTER's entity (0x2508078), seeds the module LCG 0x2508108 with rand(), sets up the
// master queue 0x25080E0 (pool 0x25080F0, 1 x 0x14), starts camera animation 0x10B1A2C and queues the TIM):
//   MasterTask (0x67F090) - alternates the packet arena (magic buffer + 0x7C7C / + 0x17C7C); on counter 1 (once) sets up the
//     pools (main 1 x 0xB34 at + 0x3334, arcs 0x40 x 0x40 at + 0x2334, bolts 0x30 x 0x38 at + 0x18B4, prims A 3 x 0x13C at
//     + 0x140, prims B 0x10 x 0x13C at + 0x4F4, sparks 0x10 x 0x14 at + 0), the arc profile (8 x sin(k x 0x800 / 7) at
//     + 0x7C6C), the bolt point pool (0x180 x 0x28 at + 0x3E68, all free, cache + 0x7A68 empty) and starts the main task
//     (action 0's targets, their mean +0x20 word, the caster's +0x1C / +0x20 words). Then: the bolts, the caster's world bone
//     matrices (0x5095B0), the main task, the arcs, the caster's bone matrices from its pose (0x508C90), prims A, prims B,
//     sparks; it ends when the main queue and the arc queue are both empty (the other queues are dropped as they are).
//   MainTask (0x67F340) - counters 0..0x54. Every tick the caster's entity byte +1 gets bit 4 and its +0x0C word 0x80;
//     counters 0..0x1F move the caster's +0x20 word toward the targets' mean - 5000 (x c / 32) and set +0x0C = 4c; counters
//     0x0F..0x1E draw three layouts 0x10AEC64 (decoded on 0x0F) at the caster's bones 0x10B6D38 / 30 / 28 turned by RotX(+0x0C);
//     every 4th counter below 0x20: three arcs (table rows 0x10B6D40 / 0x10B6E00 / 0x10B6EC0 + (c / 4 & 7) x 0x18 to the
//     anchors 0x10B6D28 / 30 / 38); counters 0x18..0x3B: layout 0x10AB23C (decoded on 0x18) at the caster's bone point 0x10B6F80; every 8th
//     counter of 0x14..0x3B: three arcs from the anchors 0x10B6D28 / 30 / 38 to the point 0x10B6F80; counters 0x28..0x36: one
//     bolt a tick from that point to target (c - 0x28) % n (its default effect position on 0x2C..n + 0x2C, else a random bone of
//     it), CRT rand bend. Screen fades at 0x28 / 0x30 / 0x32, sound 0x10B1B9C at 0, damage at 0x3A; counters 0x46..0x54 move
//     the caster back; at the end its words are restored and the bit cleared.
//   BoltTask (0x67FC30, no IDB start) - 8 steps a tick toward the destination (module LCG jitter), a point per step (the
//     pool's linked list), branches (at most 2 levels) while t < 0xC00, at the end a prim (layout 0x10AFEE0 / 0x10B0C08) or a
//     spark; the points fade (+0x20 / +0x22 / +0xE), are projected and drawn as a screen-space ribbon (0x6801D0 / 0x680330),
//     drift by their velocity, the faded head points are freed; it ends when fewer than 2 points are left.
//   ArcTask (0x680520) - an arc from a triangle centre along its normal (flag 0) or from an anchor point (flag 1) to a bone
//     point: 6 interior points of a 3-point curve, shared LCG jitter, drawn by 0x6808F0 (= 276's 0x686DB0) with the edge
//     routine 0x680A80; from counter 10 it fades and ends.
//   PrimTask (0x680170) - a layout at its node's frame (camera composed in) until it finishes.
//   SparkTask (0x6800F0) - sequence 0x10AB138 through the shadow-rotation camera (0x800, -0x200), 8 frames.
//   PrimCallback (0x67F7E0, = Brrawghh's 0x6780F0 on this module's cursor) draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the tick it updates; bolts, main, arcs, prims A, prims B, sparks.
// ENTITY WRITES: the caster - +0x0C (RotX angle) and +0x20 (position word) on counters 0..0x1F and 0x46..0x55, +0x0C = 0x80
// in between, byte +1 bit 4 (word +0 bit 12, cleared at the end), its world bone matrices (0x5095B0) and bone matrices from
// its pose (0x508C90) every tick; the targets' action results (engine).
// CAMERA: the setup's camera animation 0x10B1A2C (engine); no camera word is written by the tasks.
// Module globals: 0x2508070..0x2508110 (texture file 0x2508070, context 0x2508074, caster 0x2508078, the queues 0x2508080
// sparks / 0x2508090 prims A / 0x25080A0 prims B / 0x25080B0 bolts / 0x25080C0 arcs / 0x25080D0 main / 0x25080E0 master,
// master pool 0x25080F0, packet cursor 0x2508104, module LCG 0x2508108, magic buffer base 0x250810C).
// SCREEN-SPACE RIBBONS: the bolt edge 0x680330 and the arc edge 0x680A80 take the half widths 0x1F40 / (z + 0x200) and
// 0x2080 / (z + 0x400) in screen units; ported as the PC runs them.
// NEVER-WRITTEN STACK WORDS:
//   - the ribbon / arc draws' joint direction (entry esp - 8) is read before it is written when the first two points
//     project onto one pixel: the bolt ribbon's last writer is GTE_RTPS 0x45FAA0's stored double (high dword 0x43300000,
//     RibbonResidue), the arc draw's is the arc task's own push of its control point's address (ArcResidue).
//   - the 4th word of the bolt's start point (main: entry esp - 0x4A, bolt: entry esp - 0x22 when it interpolates) lands in
//     the point's +6 word (the pool's free mark, -1 = free): MainSrcPad / BoltPtPad. The bolt's word is the destination's
//     4th word (its dt) once a bolt of the tick has reached its end (0x67FE37), carried from bolt to bolt (the executor
//     calls them at one esp); before that a tick-start residue of other code (engine residue in game, never -1): modelled
//     as 0, as the main's.
//   - the 4th words of the arc's end points (arc entry esp - 0x2A / - 0x1A) land in the Field_Alloc block's +6 / +0x76
//     (never read).

#include "mag_common.h"
#include <intrin.h>

namespace ff8fx
{
namespace megaspark279
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2508074); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2508078); }        // caster entity (setup)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2508080; }         // pool: magic buffer, 0x10 x 0x14
	inline TaskQueue *QPrimA() { return (TaskQueue *)0x2508090; }          // pool: + 0x140, 3 x 0x13C
	inline TaskQueue *QPrimB() { return (TaskQueue *)0x25080A0; }          // pool: + 0x4F4, 0x10 x 0x13C
	inline TaskQueue *QBolts() { return (TaskQueue *)0x25080B0; }          // pool: + 0x18B4, 0x30 x 0x38
	inline TaskQueue *QArcs() { return (TaskQueue *)0x25080C0; }           // pool: + 0x2334, 0x40 x 0x40
	inline TaskQueue *QMain() { return (TaskQueue *)0x25080D0; }           // pool: + 0x3334, 1 x 0xB34
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2508104); }
	inline uint32_t &Seed() { return var<uint32_t>(0x2508108); }           // module LCG
	inline uint32_t &TexBase() { return var<uint32_t>(0x250810C); }        // Magic_TextureOFF (magic buffer)
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }    // the frame's packet arena

	static const uint32_t ORIG_MasterTask = 0x67F090;
	static const uint32_t ORIG_MainTask = 0x67F340;
	static const uint32_t ORIG_BoltTask = 0x67FC30;
	static const uint32_t ORIG_SparkTask = 0x6800F0;
	static const uint32_t ORIG_PrimTask = 0x680170;
	static const uint32_t ORIG_ArcTask = 0x680520;
	static const uint32_t ORIG_PrimCallback = 0x67F7E0;  // not a task: the layouts' callback
	static const uint32_t POOL_PrimA = 0x140, POOL_PrimB = 0x4F4, POOL_Bolts = 0x18B4, POOL_Arcs = 0x2334, POOL_Main = 0x3334;
	static const uint32_t BUF_Points = 0x3E68;            // magic buffer: bolt points, 0x180 x 0x28
	static const uint32_t BUF_PointCache = 0x7A68;        // the last freed point (0: none)
	static const uint32_t BUF_Morph = 0x7A6C;             // blended vertex frames
	static const uint32_t BUF_Profile = 0x7C6C, BUF_ProfileEnd = 0x7C7C; // arc profile, 8 words
	static const uint32_t ARENA_A = 0x7C7C, ARENA_B = 0x17C7C;          // packet arenas
	static const uint32_t MODEL_Ring = 0x10AEC64;         // prim-model layouts
	static const uint32_t MODEL_Orb = 0x10AB23C;
	static const uint32_t MODEL_Strike = 0x10AFEE0;
	static const uint32_t MODEL_Hit = 0x10B0C08;
	static const uint32_t SEQ_Spark = 0x10AB138;          // sprite sequence
	static const uint32_t ANCHOR_0 = 0x10B6D28;           // caster bone offsets (word +3 = bone): 0x10B6D28 / 30 / 38
	static const uint32_t POINT_Orb = 0x10B6F80;          // the orb's bone offset
	static const uint32_t TRI_Rows = 0x10B6D40;           // triangle rows (0x18 each) of the three arc sets: + 0 / 0xC0 / 0x180
	static const uint32_t TBL_Distance = 0x10B1BA0;       // 128 x 128 bytes: screen distance of (|dx|, |dy|)
	static const uint32_t TBL_Lengths = 0x10B5B20;        // u16 square-root table
	static const uint32_t TBL_Sine = 0x10B5D20;           // s16 sine (0x1000 entries)
	static const void *const SOUND_Spark = (const void *)0x10B1B9C;
	static const uint32_t QUAD_UV0 = 0x3D948048, QUAD_UV1 = 0xB68078; // ribbon quads: uv0 + clut, uv1 + tpage, uv2, uv3
	static const uint16_t QUAD_UV2 = 0x8048, QUAD_UV3 = 0x8078;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAngles2(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_RotationFromAnglesYXZ
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // MatrixMultiply3x3: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_RenderPrimModel
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void RotX(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701220)(angle, out); }   // MAG_063_RotationXMatrix
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }   // Effect_RotationYMatrix
	inline void GetEffectSpawnPosition(uint8_t *ent, int32_t bone, int32_t mode, int16_t *out) { fn<void (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(ent, bone, mode, out); }
	inline void TriangleCentreNormal(uint32_t bones, uint32_t tri, int16_t *centre, int16_t *normal) { fn<void (__cdecl *)(uint32_t, uint32_t, int16_t *, int16_t *)>(0x704130)(bones, tri, centre, normal); } // MAG_TriangleCentreNormal
	inline int32_t SharedRand() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }   // Effect_SharedLcg_Rand

	template<typename T> inline T &AT(uint32_t a, int32_t off = 0) { return *(T *)(a + (uint32_t)off); }
	inline int16_t &S16(uint32_t a, int32_t off) { return AT<int16_t>(a, off); }
	inline uint16_t &U16(uint32_t a, int32_t off) { return AT<uint16_t>(a, off); }
	inline uint32_t &U32(uint32_t a, int32_t off) { return AT<uint32_t>(a, off); }
	inline uint8_t &U8(uint32_t a, int32_t off) { return AT<uint8_t>(a, off); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x25080F0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0xB34 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t ntargets;  // +0x0E action 0's target count
		int16_t mean;      // +0x10 the targets' mean +0x20 word
		int16_t pad12;
		uint8_t *targets[3]; // +0x14 their entities
		uint32_t saved1C;  // +0x20 the caster's +0x1C / +0x1E words
		uint32_t saved20;  // +0x24 the caster's +0x20 / +0x22 words
		uint8_t orb[0x734];  // +0x28 layout 0x10AB23C
		uint8_t ring[3][0x148]; // +0x75C / +0x8A4 / +0x9EC layouts 0x10AEC64
	};
	struct BoltNode // pool of 0x30 nodes of 0x38 bytes
	{
		TaskNode hdr;
		int16_t count;     // +0x0C points in the list
		int16_t strike;    // +0x0E 1: a strike prim at the end
		uint32_t head;     // +0x10 oldest point
		uint32_t tail;     // +0x14 newest point
		int16_t cur[3];    // +0x18 the head of the bolt
		int16_t t;         // +0x1E progress (0x1000 = reached)
		int16_t dst[3];    // +0x20 destination
		int16_t dt;        // +0x26 progress per step
		int16_t dir[3];    // +0x28 velocity (LCG drift)
		int16_t branches;  // +0x2E branches left (-1: a branch)
		int16_t bend[3];   // +0x30 the points' drift (x sine of t)
		int16_t bend3;     // +0x36
	};
	struct PrimNode // pool of 3 (A) / 0x10 (B) nodes of 0x13C bytes
	{
		TaskNode hdr;
		Mat4x3 m;          // +0x0C frame
		uint8_t layout[0x110]; // +0x2C
	};
	struct SparkNode // pool of 0x10 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t frame;     // +0x12
	};
	struct ArcNode // pool of 0x40 nodes of 0x40 bytes
	{
		TaskNode hdr;
		uint8_t counter;   // +0x0C
		uint8_t anchor;    // +0x0D 0: from a triangle (centre, normal), 1: from an anchor point
		uint8_t amp_shift; // +0x0E
		uint8_t fade_shift; // +0x0F
		int16_t off[4];    // +0x10 the end's bone offset (+0x16 = bone)
		uint32_t tri;      // +0x18 triangle row / anchor offset
		uint32_t model;    // +0x1C the caster's skeleton (+0x64)
		int16_t bright[8][2]; // +0x20 brightness of the 8 points (second word 0)
	};
	// the prim-player callback's parameter block (a stack block of the playing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 rotation and anchor
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0xB34 && sizeof(BoltNode) == 0x38, "Mega Spark nodes");
	static_assert(sizeof(PrimNode) == 0x13C && sizeof(SparkNode) == 0x14 && sizeof(ArcNode) == 0x40, "Mega Spark nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Mega Spark prim block");

	// ------------------------------------------------------------------
	// Never-written stack words (see the module comment)
	// ------------------------------------------------------------------
	static uint32_t g_master_esp = 0;   // MasterTask's entry esp (its return address slot)
	// the arc task runs at the master's entry esp - 0x40; its last write at the draw's joint direction is the push of its
	// control point's address (arc entry esp - 0x28)
	static inline uint32_t ArcResidue() { return g_master_esp - 0x40 - 0x28; }
	// the bolt ribbon's joint direction: the last GTE_RTPS call of the projection loop stores a double there (0x45FB89)
	static const uint32_t RibbonResidue = 0x43300000;
	static uint16_t MainSrcPad() { return 0; }
	static uint16_t g_bolt_pt_pad = 0;   // the bolt task's point 4th word (entry esp - 0x22), carried from call to call
	static uint16_t BoltPtPad() { return g_bolt_pt_pad; }

	// 0x680C00: the offset v (word +3 = bone) through the caster's bone matrix -> out (3 int32)
	static void BonePoint(const int16_t *v, int32_t *out)
	{
		const uint8_t *bones = *(uint8_t *const *)*(uint8_t *const *)(Caster() + 0x64);
		const Mat4x3 *bm = (const Mat4x3 *)(bones + v[3] * 0x30 + 0x20);
		GteSetRotMatrixCtrl(bm);
		GteSetTransVectorCtrl(bm);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(out);
	}

	// 0x67FC10: module LCG
	static int32_t Lcg()
	{
		Seed() = (Seed() * 125 + 0xE) & 0x7FFF;
		return (int32_t)Seed();
	}

	// 0x67FBA0: a free point (the cache, else the first one marked free) with the start point, fade 0x200 / +0x300
	static uint32_t AllocPoint(const int16_t *src)
	{
		const uint32_t buf = TexBase();
		uint32_t p = U32(buf, BUF_PointCache);
		if (p)
			U32(buf, BUF_PointCache) = 0;
		else
		{
			p = buf + BUF_Points;
			int left = 0x180;
			while (S16(p, 6) != -1)
			{
				p += 0x28;
				if (--left == 0) return 0;
			}
		}
		U32(p, 0) = *(const uint32_t *)src;
		U32(p, 4) = *(const uint32_t *)(src + 2);
		S16(p, 0xE) = 2;
		S16(p, 0x20) = 0x200;
		S16(p, 0x22) = 0x300;
		U32(p, 0x24) = 0;
		return p;
	}

	// 0x67FA80: a bolt from src to dst (4 words each) with velocity dir, the points' drift (bend: 4 words, nullptr: module LCG)
	static uint32_t SpawnBolt(const int16_t *dir, const int16_t *src, const int16_t *dst, const int16_t *bend, int16_t bend3,
		int16_t t, int16_t dt, int16_t strike, int16_t branches)
	{
		const uint32_t p = AllocPoint(src);
		if (!p) return 0;
		BoltNode *b = (BoltNode *)AddTaskToQueue(QBolts(), ORIG_BoltTask);
		if (!b)
		{
			S16(p, 6) = -1;
			return 0;
		}
		*(uint32_t *)&b->cur[0] = *(const uint32_t *)src;
		*(uint32_t *)&b->cur[2] = *(const uint32_t *)(src + 2);
		*(uint32_t *)&b->dst[0] = *(const uint32_t *)dst;
		*(uint32_t *)&b->dst[2] = *(const uint32_t *)(dst + 2);
		*(uint32_t *)&b->dir[0] = *(const uint32_t *)dir;
		*(uint32_t *)&b->dir[2] = *(const uint32_t *)(dir + 2);
		b->t = t;
		b->dt = dt;
		b->strike = strike;
		b->branches = branches;
		b->tail = p;
		b->head = p;
		b->count = 1;
		if (!bend)
		{
			S16(p, 0xC) = 0;
			S16(p, 0xA) = 0;
			S16(p, 8) = 0;
			b->bend[0] = (int16_t)((Lcg() & 0xFF) - 0x80);
			b->bend[1] = (int16_t)((Lcg() & 0xFF) - 0x80);
			b->bend[2] = (int16_t)((Lcg() & 0xFF) - 0x80);
			b->bend3 = bend3;
			return p;
		}
		*(uint32_t *)&b->bend[0] = *(const uint32_t *)bend;
		*(uint32_t *)&b->bend[2] = *(const uint32_t *)(bend + 2);
		b->bend3 = bend3;
		return p;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag279_mega_spark_held.h"
#endif

namespace ff8fx
{
namespace megaspark279
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x67F7E0, = Brrawghh's 0x6780F0 on this module's cursor): one object of a layout. Vertex frame
	// (or blend into the parameter block's morph buffer), rotation from the record's angles, the record's offset through the
	// parameter matrix (flag 0x200: as is), the matrix composed with it, scale (flag 0x100: 3x3 product, else scale3DMatrix),
	// drawn by 0x7043B0.
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
		PacketCursor() = RenderPrim2(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Screen-space ribbons
	// ------------------------------------------------------------------
	// 0x680330 / 0x680A80: a point's ribbon edge. The screen point (x, y at +sxy) moves to its -half-width side, the +side
	// goes to +sxy + 8; the half width num / (z + den) (z at +sxy + 4) along the unit normal of the segment to the next point
	// (averaged with the previous segment's, `prev`); out = this segment's unit normal. A point whose segment has no length
	// (first point) is marked behind (z = -1).
	static void RibbonEdge(uint32_t p, uint32_t next, const int16_t *prev, int16_t *out, int32_t sxy, int32_t num, int32_t den)
	{
		if (S16(p, sxy + 4) < 0) return;
		int32_t ox, oy;
		if (next == 0)
		{
			oy = 0;
			ox = 0;
		}
		else
		{
			if (S16(next, sxy + 4) < 0) return;
			const int32_t dx = ScreenPx(S16(next, sxy)) - ScreenPx(S16(p, sxy));
			int32_t ax = dx < 0 ? -dx : dx;
			if (ax >= 0x80) ax = 0x7F;
			const int32_t dy = ScreenPx(S16(p, sxy + 2)) - ScreenPx(S16(next, sxy + 2));
			int32_t ay = dy < 0 ? -dy : dy;
			if (ay >= 0x80) ay = 0x7F;
			const int32_t dist = U8(TBL_Distance, ay * 128 + ax);
			int32_t nx, ny;
			if (prev)
			{
				if (dist == 0)
				{
					out[0] = prev[0];
					out[1] = prev[1];
					nx = out[0];
					ny = out[1];
				}
				else
				{
					const int32_t ex = shl32(dy, 12) / dist;
					const int32_t ey = shl32(dx, 12) / dist;
					int32_t sx = ex + prev[0];
					int32_t sy = ey + prev[1];
					out[0] = (int16_t)ex;
					out[1] = (int16_t)ey;
					if (sx == 0 && sy == 0)
					{
						sx = ex + ex;
						sy = ey + ey;
					}
					const int32_t k = 0x4000 - ((mul32(sx, sx) + mul32(sy, sy)) >> 13);
					ny = mul32(k, sy) >> 13;
					nx = mul32(k, sx) >> 13;
				}
			}
			else
			{
				if (dist == 0)
				{
					S16(p, sxy + 4) = -1;
					return;
				}
				nx = shl32(dy, 12) / dist;
				ny = shl32(dx, 12) / dist;
				out[0] = (int16_t)nx;
				out[1] = (int16_t)ny;
			}
			const int32_t w = num / ((int32_t)S16(p, sxy + 4) + den);
			oy = ScreenEighths(mul32(w, ny) >> 12);
			ox = ScreenEighths(mul32(w, nx) >> 12);
		}
		const int16_t x = S16(p, sxy), y = S16(p, sxy + 2);
		S16(p, sxy) = (int16_t)(x - ox);
		S16(p, sxy + 8) = (int16_t)(x + ox);
		S16(p, sxy + 0xA) = (int16_t)(y + oy);
		S16(p, sxy + 2) = (int16_t)(y - oy);
	}
	// 0x680330: bolt point edge (screen point at +0x10, z at +0x14)
	static void BoltEdge(uint32_t p, uint32_t next, const int16_t *prev, int16_t *out) { RibbonEdge(p, next, prev, out, 0x10, 0x1F40, 0x200); }
	// 0x680A80: arc point edge (screen point at +0, z at +4)
	static void ArcEdge(uint32_t p, uint32_t next, const int16_t *prev, int16_t *out) { RibbonEdge(p, next, prev, out, 0, 0x2080, 0x400); }

	// one gouraud textured quad (0x34 bytes) of a ribbon: xy0 / xy1 of a point and of the next one, grey levels c0 / c1
	static void RibbonQuad(uint32_t pk, uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1, int32_t c0, int32_t c1, uint32_t bucket)
	{
		U32(pk, 8) = a0;
		U32(pk, 0x14) = a1;
		U32(pk, 0xC) = QUAD_UV0;
		U32(pk, 0x18) = QUAD_UV1;
		U16(pk, 0x24) = QUAD_UV2;
		U16(pk, 0x30) = QUAD_UV3;
		U32(pk, 0x20) = b0;
		U32(pk, 0x2C) = b1;
		const uint32_t col0 = (uint32_t)((((c0 << 8) | c0) << 8) | c0);
		U32(pk, 0x10) = col0;
		U32(pk, 4) = col0;
		const uint32_t col1 = (uint32_t)((((c1 << 8) | c1) << 8) | c1);
		U32(pk, 0) = 0xC000000;
		U8(pk, 7) = 0x3E;
		U32(pk, 0x28) = col1;
		U32(pk, 0x1C) = col1;
		InsertPrimAutoDepth(bucket, (void *)pk);
	}

	// 0x6801D0: the bolt's points (count of them, from head) as a ribbon into the module's packet arena
	static void BoltRibbon(uint32_t head, int32_t count)
	{
		int16_t dir[2] = { (int16_t)(RibbonResidue & 0xFFFF), (int16_t)(RibbonResidue >> 16) };
		BoltEdge(head, U32(head, 0x24), nullptr, dir);
		uint32_t p = U32(head, 0x24);
		for (int32_t k = count - 2; k; k--)
		{
			BoltEdge(p, U32(p, 0x24), dir, dir);
			p = U32(p, 0x24);
		}
		BoltEdge(p, 0, dir, nullptr);
		uint32_t q = head;
		for (int32_t k = count - 1; k; k--)
		{
			const uint32_t nx = U32(q, 0x24);
			if (S16(q, 0x14) > 0 && S16(nx, 0x14) > 0)
			{
				const uint32_t pk = PacketCursor();
				PacketCursor() = pk + 0x34;
				const int32_t c0 = (int16_t)(S16(q, 0x20) >> 4), c1 = (int16_t)(S16(nx, 0x20) >> 4);
				RibbonQuad(pk, U32(q, 0x10), U32(q, 0x18), U32(nx, 0x10), U32(nx, 0x18), c0, c1,
					RenderOT44() + (uint32_t)(((int32_t)S16(nx, 0x14) + S16(q, 0x14)) >> 3) * 4);
			}
			q = nx;
		}
	}

	// 0x6808F0 (= 276's 0x686DB0): the 8 points (16 bytes: x, y, z / screen xy, z, edge xy, brightness +0xE) projected,
	// their edges, 7 quads into the frame arena
	static void ArcDraw(uint32_t pts, uint32_t residue)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		uint32_t q = pts;
		for (int i = 0; i < 8; i++, q += 0x10)
		{
			GteLoadV0((void *)q);
			GteRTPS();
			GteReadDataReg(0xE, (void *)q);
			int32_t sz;
			GteReadDataRegTo(&sz, 0x13);
			S16(q, 4) = (int16_t)(sz >> 2);
		}
		int16_t dir[2] = { (int16_t)(residue & 0xFFFF), (int16_t)(residue >> 16) };
		ArcEdge(pts, pts + 0x10, nullptr, dir);
		for (int i = 1; i < 7; i++) ArcEdge(pts + 0x10 * i, pts + 0x10 * (i + 1), dir, dir);
		ArcEdge(pts + 0x70, 0, dir, nullptr);
		q = pts;
		for (int i = 0; i < 7; i++, q += 0x10)
		{
			if (S16(q, 4) <= 0 || S16(q, 0x14) <= 0) continue;
			const uint32_t pk = FrameCursor();
			FrameCursor() = pk + 0x34;
			RibbonQuad(pk, U32(q, 0), U32(q, 8), U32(q, 0x10), U32(q, 0x18), S16(q, 0xE), S16(q, 0x1E),
				RenderOT44() + (uint32_t)(((int32_t)S16(q, 0x14) + S16(q, 4)) >> 3) * 4);
		}
	}

	// ------------------------------------------------------------------
	// Spark (0x6800F0)
	// ------------------------------------------------------------------
	static void SparkDraw(const SparkNode *s)
	{
		TransformCameraByShadowRotation(s->pos, 0x800, -0x200);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Spark;
		*(int16_t *)(h + 4) = s->frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag279_mega_spark_held.inc
		FX_HELD(held_note_spark(s);)
		SparkDraw(s);
		s->frame++;
		return s->frame < 8 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Prim (0x680170): its layout at its frame until the layout ends
	// ------------------------------------------------------------------
	static uint32_t __cdecl PrimTask(TaskNode *n)
	{
		PrimNode *p = (PrimNode *)n;
		PrimArg arg;
		ComposeAffineTransform(&Camera(), &p->m, &arg.m);
		arg.morph = TexBase() + BUF_Morph;
		arg.depth = -0x80;
		// 30 fps layer: see mag279_mega_spark_held.inc
		FX_HELD(held_note_prim(p, &arg);)
		return prim::play((prim::Layout *)p->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Bolt (0x67FC30)
	// ------------------------------------------------------------------
	// the end of a bolt: a branch (branches < 0) leaves a hit prim (destination above the ground: y >= 0) or a spark, the
	// trunk of a strike bolt a strike prim; the prim's frame faces the caster, scaled unless 0x1000
	static void BoltEnd(BoltNode *b)
	{
		PrimNode *p;
		int32_t scale = 0x1000;
		if (b->branches < 0)
		{
			if (b->dst[1] < 0)
			{
				SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
				if (!s) return;
				*(uint32_t *)&s->pos[0] = *(const uint32_t *)&b->dst[0];
				*(uint32_t *)&s->pos[2] = *(const uint32_t *)&b->dst[2];
				s->frame = 0;
				return;
			}
			p = (PrimNode *)AddTaskToQueue(QPrimB(), ORIG_PrimTask);
			if (!p) return;
			DecodeModelPrimLayout(MODEL_Hit, p->layout, 0x88);
			scale = (CrtRand() & 0xFFF) + 0x800;
		}
		else
		{
			if (b->strike == 0) return;
			p = (PrimNode *)AddTaskToQueue(QPrimA(), ORIG_PrimTask);
			if (!p) return;
			DecodeModelPrimLayout(MODEL_Strike, p->layout, 0x110);
		}
		const uint8_t *caster = Caster();
		int32_t v[3] = { (int32_t)b->dst[0] - *(const int16_t *)(caster + 0x1C), 0, (int32_t)b->dst[2] - *(const int16_t *)(caster + 0x20) };
		BuildOrthonormalBasis(v, &p->m);
		p->m.t[0] = b->dst[0];
		p->m.t[1] = b->dst[1];
		p->m.t[2] = b->dst[2];
		if ((int16_t)scale != 0x1000)
		{
			const int32_t s[3] = { (int16_t)scale, (int16_t)scale, (int16_t)scale };
			Scale3DMatrix(&p->m, s);
		}
	}

	static uint32_t __cdecl BoltTask(TaskNode *n)
	{
		BoltNode *b = (BoltNode *)n;
		for (int step = 8; step; step--)
		{
			if (b->t >= 0x1000) continue;
			b->cur[0] = (int16_t)(b->cur[0] + b->dir[0]);
			b->t = (int16_t)(b->dt + b->t);
			b->cur[1] = (int16_t)(b->cur[1] + b->dir[1]);
			b->cur[2] = (int16_t)(b->cur[2] + b->dir[2]);
			b->dir[0] = (int16_t)(b->dir[0] + ((Lcg() & 0x7F) - 0x40));
			b->dir[1] = (int16_t)(b->dir[1] + ((Lcg() & 0x7F) - 0x40));
			b->dir[2] = (int16_t)(b->dir[2] + ((Lcg() & 0x7F) - 0x40));
			if (Lcg() < 0x1000)
			{
				b->cur[0] = (int16_t)(b->cur[0] + ((Lcg() & 0x1FF) - 0x100));
				b->cur[1] = (int16_t)(b->cur[1] + ((Lcg() & 0x1FF) - 0x100));
				b->cur[2] = (int16_t)(b->cur[2] + ((Lcg() & 0x1FF) - 0x100));
			}
			int16_t pt[4];
			if (b->t >= 0x1000)
			{
				BoltEnd(b);
				*(uint32_t *)&pt[0] = *(const uint32_t *)&b->dst[0];
				*(uint32_t *)&pt[2] = *(const uint32_t *)&b->dst[2];
				b->t = 0x1000;
				g_bolt_pt_pad = (uint16_t)pt[3];
			}
			else
			{
				GteSetIR0(0x1000 - b->t);
				GteLoadIR123(b->cur);
				GteGPF();
				GteSetIR0(b->t);
				GteLoadIR123(b->dst);
				GteGPL();
				GteStoreIR123(pt);
				pt[3] = (int16_t)BoltPtPad();
			}
			const uint32_t p = AllocPoint(pt);
			if (!p)
			{
				b->t = 0x1000;
				continue;
			}
			U32(b->tail, 0x24) = p;
			b->tail = p;
			GteSetIR0(AT<int16_t>(TBL_Sine, ((b->t >> 1) & 0xFFF) * 2));
			GteLoadIR123(b->bend);
			GteGPF();
			b->count++;
			GteStoreIR123((void *)(p + 8));
			if (b->t >= 0xC00) continue;
			if (Lcg() >= shl32(b->branches, 13)) continue;
			int16_t nd[4], nv[4];
			nd[0] = (int16_t)((Lcg() & 0xFFF) + b->dst[0] - 0x800);
			nd[1] = (int16_t)((Lcg() & 0x7FF) + b->dst[1] - 0x400);
			nd[2] = (int16_t)((Lcg() & 0x1FF) + b->dst[2] + 0x100);
			nv[0] = (int16_t)((Lcg() & 0x1FF) + b->dir[0] - 0x100);
			nv[1] = (int16_t)((Lcg() & 0x1FF) + b->dir[1] - 0x100);
			nv[2] = (int16_t)((Lcg() & 0x1FF) + b->dir[2] - 0x100);
			nd[3] = 0;   // copied into the branch's dst 4th word, then replaced by its dt
			nv[3] = 0;   // copied into the branch's dir 4th word, then replaced by its branches
			const uint32_t q = SpawnBolt(nv, pt, nd, b->bend, -1, b->t, 0x200, 0, -1);
			if (q)
			{
				U32(q, 8) = U32(p, 8);
				U32(q, 0xC) = U32(p, 0xC);
			}
			b->branches--;
		}
		// the points fade: +0x20 by +0x22; +0xE ticks at full step, then the step falls by 0x140 down to -0x180
		uint32_t p = b->head;
		do
		{
			if (S16(p, 0x20) > 0)
			{
				S16(p, 0x20) = (int16_t)(S16(p, 0x20) + S16(p, 0x22));
				if (S16(p, 0x20) <= 0) S16(p, 0x20) = 0;
				else if (S16(p, 0xE) != 0 && S16(p, 0x22) <= 0) S16(p, 0xE) = (int16_t)(S16(p, 0xE) - 1);
				else if (S16(p, 0x22) > -0x180) S16(p, 0x22) = (int16_t)(S16(p, 0x22) - 0x140);
			}
			p = U32(p, 0x24);
		} while (p);
		p = b->head;
		do
		{
			GteLoadV0((void *)p);
			GteRTPS();
			GteReadSXY2((void *)(p + 0x10));
			GteReadOTZ((void *)(p + 0x14));
			p = U32(p, 0x24);
		} while (p);
		// 30 fps layer: see mag279_mega_spark_held.inc
		FX_HELD(held_note_bolt(b);)
		if (b->count >= 2) BoltRibbon(b->head, b->count);
		p = b->head;
		do
		{
			S16(p, 0) = (int16_t)(S16(p, 0) + S16(p, 8));
			S16(p, 2) = (int16_t)(S16(p, 2) + S16(p, 0xA));
			S16(p, 4) = (int16_t)(S16(p, 4) + S16(p, 0xC));
			p = U32(p, 0x24);
		} while (p);
		// the faded head points go back to the pool
		p = b->head;
		if (S16(p, 0x20) != 0) return 0;
		for (;;)
		{
			const uint32_t nx = U32(p, 0x24);
			if (S16(nx, 0x20) != 0) return 0;
			U32(TexBase(), BUF_PointCache) = p;
			S16(p, 6) = -1;
			b->count--;
			b->head = nx;
			if (b->count < 2) return TASK_END;
			p = nx;
			if (S16(p, 0x20) != 0) return 0;
		}
	}

	// ------------------------------------------------------------------
	// Arc (0x680520)
	// ------------------------------------------------------------------
	// 0x6804B0: an arc of the caster's skeleton from the triangle row / anchor `from` to the bone offset `to`
	static void SpawnArc(uint32_t model, uint32_t from, uint32_t to, uint8_t anchor, uint8_t amp_shift, uint8_t fade_shift)
	{
		ArcNode *a = (ArcNode *)AddTaskToQueue(QArcs(), ORIG_ArcTask);
		if (!a) return;
		a->amp_shift = amp_shift;
		a->fade_shift = fade_shift;
		a->anchor = anchor;
		a->counter = 0;
		*(uint32_t *)&a->off[0] = U32(to, 0);
		*(uint32_t *)&a->off[2] = U32(to, 4);
		a->tri = from;
		a->model = model;
		for (int k = 0; k < 8; k++)
		{
			a->bright[k][0] = 0x80;
			a->bright[k][1] = 0;
		}
	}

	static uint32_t __cdecl ArcTask(TaskNode *n)
	{
		ArcNode *a = (ArcNode *)n;
		const uint32_t bones = U32(a->model, 0) + 0x10;
		int16_t c[4], ctrl[4], p2[4];
		c[3] = 0;    // never written (only reaches the Field_Alloc block's +6)
		p2[3] = 0;   // never written (only reaches the Field_Alloc block's +0x76)
		int32_t amp;
		if (a->anchor == 0)
		{
			TriangleCentreNormal(bones, a->tri, c, ctrl);
			const Mat4x3 *bm = (const Mat4x3 *)(bones + a->off[3] * 0x30 + 0x10);
			GteSetRotMatrixCtrl(bm);
			GteSetTransVectorCtrl(bm);
			GteLoadV0(a->off);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(p2);
			int32_t d[3];
			d[0] = (int32_t)p2[0] - c[0];
			d[1] = (int32_t)p2[1] - c[1];
			d[2] = (int32_t)p2[2] - c[2];
			GteLoadIR123_32b(d);
			GteSQR();
			GteReadMAC123(d);
			int32_t sum = d[2] + d[1] + d[0];
			GteSetLZCS(sum);
			int32_t lz;
			GteReadLZCR(&lz);
			lz &= ~1;
			const int32_t shift = (0x1F - lz) >> 1;
			const int32_t norm = lz - 0x18;
			if (norm > 0) sum = shl32(sum, norm);
			else sum >>= (0x18 - lz);
			const int32_t len = (int32_t)((uint32_t)AT<uint16_t>(TBL_Lengths, sum * 2) << shift);
			amp = len >> 12;
			GteSetIR0(len >> a->amp_shift);
			GteLoadIR123(ctrl);
			GteGPF();
			GteStoreIR123(ctrl);
			ctrl[0] = (int16_t)(ctrl[0] + c[0]);
			ctrl[1] = (int16_t)(ctrl[1] + c[1]);
			ctrl[2] = (int16_t)(ctrl[2] + c[2]);
		}
		else
		{
			const Mat4x3 *am = (const Mat4x3 *)(bones + S16(a->tri, 6) * 0x30 + 0x10);
			GteSetRotMatrixCtrl(am);
			GteSetTransVectorCtrl(am);
			GteLoadV0((const void *)a->tri);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(c);
			*(uint32_t *)&ctrl[0] = *(const uint32_t *)&c[0];
			*(uint32_t *)&ctrl[2] = *(const uint32_t *)&c[2];
			const Mat4x3 *bm = (const Mat4x3 *)(bones + a->off[3] * 0x30 + 0x10);
			GteSetRotMatrixCtrl(bm);
			GteSetTransVectorCtrl(bm);
			GteLoadV0(a->off);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(p2);
			amp = 0x800;
		}
		const uint32_t b = (uint32_t)FieldAlloc(0x80);
		U32(b, 0) = *(const uint32_t *)&c[0];
		U32(b, 4) = *(const uint32_t *)&c[2];
		S16(b, 0xE) = a->bright[0][0];
		U32(b, 0x70) = *(const uint32_t *)&p2[0];
		U32(b, 0x74) = *(const uint32_t *)&p2[2];
		S16(b, 0x7E) = a->bright[7][0];
		for (int32_t k = 1; k < 7; k++)
		{
			const int32_t t = shl32(k, 12) / 7;
			const int32_t u = 0x1000 - t;
			int16_t q[4];
			GteSetIR0(u);
			GteLoadIR123(c);
			GteGPF();
			GteSetIR0(t);
			GteLoadIR123(ctrl);
			GteGPL();
			GteStoreIR123(q);
			GteSetIR0(u);
			GteLoadIR123(q);
			GteGPF();
			GteSetIR0(t);
			GteLoadIR123(p2);
			GteGPL();
			GteStoreIR123((void *)(b + 0x10 * k));
			S16(b, 0x10 * k + 0xE) = a->bright[k][0];
		}
		for (int32_t k = 1; k < 7; k++)
		{
			const uint32_t q = b + 0x10 * k;
			const int32_t e = mul32(S16(TexBase(), BUF_Profile + 2 * k), amp) >> 12;
			S16(q, 0) = (int16_t)(S16(q, 0) + (mul32(SharedRand() - 0x4000, e) >> 18));
			S16(q, 2) = (int16_t)(S16(q, 2) + (mul32(SharedRand() - 0x4000, e) >> 18));
			S16(q, 4) = (int16_t)(S16(q, 4) + (mul32(SharedRand() - 0x4000, e) >> 18));
		}
		// 30 fps layer: see mag279_mega_spark_held.inc
		FX_HELD(held_note_arc(a, b, ArcResidue());)
		ArcDraw(b, ArcResidue());
		FieldFree(0x80);
		if (a->counter >= 0xA)
		{
			int32_t lit = 0;
			for (int k = 0; k < 8; k++)
			{
				const int32_t dec = (0x1400 - S16(TexBase(), BUF_Profile + 2 * k)) >> a->fade_shift;
				a->bright[k][0] = (int16_t)(a->bright[k][0] - dec);
				if (a->bright[k][0] < 0) a->bright[k][0] = 0;
				else lit++;
			}
			if (lit <= 1) return TASK_END;
		}
		a->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Main (0x67F340)
	// ------------------------------------------------------------------
	static void ArcSet(uint32_t from_or_table, uint32_t to, uint8_t anchor, uint8_t amp_shift)
	{
		SpawnArc(U32((uint32_t)Caster(), 0x64), from_or_table, to, anchor, amp_shift, 7);
	}

	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		*(Caster() + 1) |= 0x10;
		*(int16_t *)(Caster() + 0xC) = 0x80;
		PrimArg arg;
		arg.depth = 0;   // (written before every play)
		arg.morph = 0;
		int32_t c = m->counter;
		if ((uint32_t)c < 0x20)
		{
			const int16_t saved = (int16_t)m->saved20;
			const uint32_t d = ((uint32_t)((int32_t)saved - (int32_t)m->mean + 0x1388) * (uint32_t)c) >> 5;
			*(int16_t *)(Caster() + 0x20) = (int16_t)(saved - (int16_t)d);
			*(int16_t *)(Caster() + 0xC) = (int16_t)(((uint32_t)c << 7) >> 5);
			if ((uint32_t)c >= 0xF && (uint32_t)c < 0x1F)
			{
				if (c == 0xF)
					for (int i = 2; i >= 0; i--) DecodeModelPrimLayout(MODEL_Ring, m->ring[i], 0x148);
				arg.depth = -0x80;
				arg.morph = TexBase() + BUF_Morph;
				for (int i = 2; i >= 0; i--)
				{
					BonePoint((const int16_t *)(ANCHOR_0 + 8 * i), arg.m.t);
					RotX(*(const int16_t *)(Caster() + 0xC), &arg.m);
					ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
					// 30 fps layer: see mag279_mega_spark_held.inc
					FX_HELD(held_note_main(m, REC_RING, i, &arg);)
					prim::play((prim::Layout *)m->ring[i], PrimCallback, (int)&arg, 0);
				}
			}
			if ((c & 3) == 0)
			{
				const uint32_t row = (((uint32_t)c >> 2) & 7) * 0x18;
				ArcSet(TRI_Rows + row, ANCHOR_0, 0, 0xC);
				ArcSet(TRI_Rows + 0xC0 + row, ANCHOR_0 + 8, 0, 0xC);
				ArcSet(TRI_Rows + 0x180 + row, ANCHOR_0 + 0x10, 0, 0xC);
			}
		}
		c = m->counter - 0x14;
		if ((uint32_t)c < 0x28)
		{
			if ((uint32_t)c >= 4)
			{
				if (c == 4) DecodeModelPrimLayout(MODEL_Orb, m->orb, 0x734);
				BonePoint((const int16_t *)POINT_Orb, arg.m.t);
				RotY(0, &arg.m);
				ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
				arg.morph = TexBase() + BUF_Morph;
				arg.depth = -0x80;
				// 30 fps layer: see mag279_mega_spark_held.inc
				FX_HELD(held_note_main(m, REC_ORB, 0, &arg);)
				prim::play((prim::Layout *)m->orb, PrimCallback, (int)&arg, 0);
			}
			if ((c & 7) == 0)
			{
				ArcSet(ANCHOR_0, POINT_Orb, 1, 0xE);
				ArcSet(ANCHOR_0 + 8, POINT_Orb, 1, 0xE);
				ArcSet(ANCHOR_0 + 0x10, POINT_Orb, 1, 0xE);
			}
		}
		c = m->counter - 0x28;
		if ((uint32_t)c < 0xF)
		{
			uint8_t *tgt = m->targets[(uint32_t)c % (uint32_t)(int32_t)m->ntargets];
			if (c == 0) ScreenFadeTask(0, 1, 2, 0x80);
			int32_t tmp[3];
			BonePoint((const int16_t *)POINT_Orb, tmp);
			int16_t src[4] = { (int16_t)tmp[0], (int16_t)tmp[1], (int16_t)tmp[2], (int16_t)MainSrcPad() };
			int16_t dst[4] = { 0, 0, 0, 0 };   // 4th word: replaced by the bolt's dt
			int16_t strike;
			if ((uint32_t)c >= 4 && (uint32_t)c <= (uint32_t)((int32_t)m->ntargets + 4))
			{
				strike = 1;
				GetDefaultEffectPosition(tgt, dst);
			}
			else
			{
				strike = 0;
				const int32_t r = CrtRand();
				GetEffectSpawnPosition(tgt, mul32(r, ***(const uint8_t *const *const *)(tgt + 0x64)) >> 15, 0, dst);
			}
			int16_t vel[4];
			vel[0] = (int16_t)((CrtRand() & 0xFF) - 0x80);
			vel[1] = (int16_t)(-0x40 - (CrtRand() & 0x7F));
			vel[2] = (int16_t)((CrtRand() & 0x3F) + 0x40);
			vel[3] = 0;   // replaced by the bolt's branches
			SpawnBolt(vel, src, dst, nullptr, 0, 0, 0x200, strike, 2);
		}
		if (m->counter == 0) BdPlaySE(SOUND_Spark, 0, 0x80);
		if (m->counter == 0x30) ScreenFadeTask(0, 1, 0, 0x80);
		else if (m->counter == 0x32) ScreenFadeTask(0, 1, 0, 0x40);
		else if (m->counter == 0x3A) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		const int32_t e = 0x55 - m->counter;
		if ((uint32_t)e < 0x10)
		{
			const int16_t saved = (int16_t)m->saved20;
			const uint32_t d = ((uint32_t)((int32_t)saved - (int32_t)m->mean + 0x1388) * (uint32_t)e) >> 4;
			*(int16_t *)(Caster() + 0x20) = (int16_t)(saved - (int16_t)d);
			*(int16_t *)(Caster() + 0xC) = (int16_t)(((uint32_t)e << 7) >> 4);
		}
		m->counter++;
		if (m->counter < 0x55) return 0;
		*(uint16_t *)Caster() &= 0xEFFF;
		*(int16_t *)(Caster() + 0xC) = 0;
		*(int16_t *)(Caster() + 0x20) = (int16_t)m->saved20;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Master (0x67F090)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		g_master_esp = (uint32_t)_AddressOfReturnAddress();
		g_bolt_pt_pad = 0;   // tick-start residue at the bolts' entry esp - 0x22 (see the module comment)
		// 30 fps layer: see mag279_mega_spark_held.inc
		FX_HELD(held_note_master();)
		MasterNode *mn = (MasterNode *)n;
		if (mn->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			mn->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			mn->arena = 1;
		}
		if (mn->counter == 1 && !mn->started)
		{
			mn->started = 1;
			const uint32_t buf = TexBase();
			InitTaskQueuePool(QMain(), (void *)(buf + POOL_Main), 0xB34, 1);
			InitTaskQueuePool(QArcs(), (void *)(TexBase() + POOL_Arcs), 0x40, 0x40);
			InitTaskQueuePool(QBolts(), (void *)(TexBase() + POOL_Bolts), 0x38, 0x30);
			InitTaskQueuePool(QPrimA(), (void *)(TexBase() + POOL_PrimA), 0x13C, 3);
			InitTaskQueuePool(QPrimB(), (void *)(TexBase() + POOL_PrimB), 0x13C, 0x10);
			InitTaskQueuePool(QSparks(), (void *)TexBase(), 0x14, 0x10);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&m->counter, 0, 0x2CA);
			for (int32_t k = 0; k < 8; k++)
				S16(TexBase(), BUF_Profile + 2 * k) = (int16_t)ComputeSin(shl32(k, 11) / 7);
			// 0x67F310: every point free, no cached point
			for (uint32_t p = TexBase() + BUF_Points + 6, i = 0; i < 0x180; i++, p += 0x28) S16(p, 0) = -1;
			U32(TexBase(), BUF_PointCache) = 0;
			const ActionData *act = Ctx()->actions;
			m->ntargets = act->target_count;
			m->mean = 0;
			for (int32_t i = 0; i < m->ntargets; i++)
			{
				uint8_t *t = Entity(Ctx()->actions->targets[i * TARGET_STRIDE]);
				m->targets[i] = t;
				m->mean = (int16_t)(m->mean + *(const int16_t *)(t + 0x20));
			}
			m->mean = (int16_t)((int32_t)m->mean / (int32_t)m->ntargets);
			m->saved1C = *(const uint32_t *)(Caster() + 0x1C);
			m->saved20 = *(const uint32_t *)(Caster() + 0x20);
		}
		int a0, a1;
		if (mn->started)
		{
			ExecuteTaskQueue(QBolts());
			uint8_t *caster = Caster();
			ComputeBonesWorldMatrices(caster + 0x60, caster + 0x40);
			a0 = ExecuteTaskQueue(QMain());
			a1 = ExecuteTaskQueue(QArcs());
			BuildBoneMatricesFromPose(Caster() + 0x60);
			ExecuteTaskQueue(QPrimA());
			ExecuteTaskQueue(QPrimB());
			ExecuteTaskQueue(QSparks());
		}
		else a0 = a1 = (int)n;
		if (mn->started && !a0 && !a1) return TASK_END;
		mn->counter++;
		return 0;
	}
}

	void register_mag279_mega_spark()
	{
		register_port(megaspark279::ORIG_MasterTask, (void *)megaspark279::MasterTask, "279 MasterTask", 279);
		register_port(megaspark279::ORIG_MainTask, (void *)megaspark279::MainTask, "279 MainTask", 279);
		register_port(megaspark279::ORIG_BoltTask, (void *)megaspark279::BoltTask, "279 BoltTask", 279);
		register_port(megaspark279::ORIG_ArcTask, (void *)megaspark279::ArcTask, "279 ArcTask", 279);
		register_port(megaspark279::ORIG_PrimTask, (void *)megaspark279::PrimTask, "279 PrimTask", 279);
		register_port(megaspark279::ORIG_SparkTask, (void *)megaspark279::SparkTask, "279 SparkTask", 279);
		// 30 fps layer: see mag279_mega_spark_held.inc
		FX_HELD(megaspark279::register_mag279_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag279_mega_spark_held.inc"
#endif
