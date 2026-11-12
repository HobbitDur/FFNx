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

// Effect 287: Mega Pulse Cannon (enemy attack 292 of kernel.bin; Trauma c0m109; MAG_287_*).
//
// Structure (setup 0x6F2E80 -> _Init 0x6F2E90, no file loader: _Init queues the TIMs 0x140016C / 0x142938C, keeps the
// context (0x25562F8) and the CASTER's entity (0x25562FC), seeds 0x2556378 with rand() (unused), sets up the master queue
// 0x2556350 (pool 0x2556360, 1 x 0x14) and starts camera animation 0x13FABFC):
//   MasterTask (0x6F2F20) - alternates the packet arena (magic buffer + 0x1FF0 / + 0x11FF0), zeroes the flash level
//     0x255637C; on counter 1 (once) sets up the pools (main 1 x 0x9A8 at + 0x1070, arcs 0x40 x 0x40 at + 0x21FF4, spots
//     3 x 0x1D0 at + 0xB00, rocks 0x10 x 0x20 at + 0x900, sparks 0x40 x 0x24 at + 0), starts the main task (layouts
//     0x13EF924 / 0x13F87E8, action 0's targets), the arc profile (8 x sin(k x 0x800 / 7) at + 0x1F98) and the stream
//     state (+ 0x1FA8). Then: the caster's world bone matrices (0x5095B0), the main task, the arcs, the caster's bone
//     matrices from its pose (0x508C90), spots, rocks, sparks; the screen flash (0x5713E0) of the level; once the main
//     task has ended it waits for the stream (0x5342C0) and ends (counter held at 2 meanwhile).
//   MainTask (0x6F31A0) - counters 0..0x4A. Every tick the caster's word +0 loses bit 3. The muzzle point P (bone point
//     0x13FAEA0) until counter 0x23, then the one of 0x23 (node +0x10). Counters 0..0x1D: an arc a tick from triangle row
//     c % 13 of 0x13FAEB0 to P, the caster's word +0 gets bit 3; 0x1E: the arc queue is emptied (its header zeroed);
//     0x20..0x25: 6 sparks a tick at the caster's bones 0x1B..0x16 (0x6F3AD0), falling; even counters 0x1E..0x3A: a
//     rock (0x13EF72C) around the caster (CRT rand), bouncing; flash level ramps 0x21..0x28 / 0x3B..0x4A; counters
//     0..7: 4 sparks around P drawn in toward it; every tick the cannon layout (0x13EF924) at P in RotY(caster facing)
//     and the beam layout (0x13F87E8) at P with the caster's height, texture scrolls (c & 3) / (-c & 3) x 0x20;
//     counters 0x28 / 0x2C / 0x30: a spot (0x13F8F5C) at target 0x13FAEA8[k] if it exists; sound 0x13FAE98 at 0,
//     stream load 0x168 into + 0x21FF0 at 0x37, the stream pumped every tick, damage at 0x41.
//   ArcTask (0x6F3DF0, = Mega Spark's 0x680520 on this module's profile) - an arc from a triangle centre along its normal
//     to the muzzle point: 6 interior points of a 3-point curve, shared LCG jitter, drawn by 0x6F41C0 (= Mega Spark's
//     0x6808F0) with the edge routine 0x6F4350 (= 0x680A80); from counter 10 it fades and ends.
//   SpotTask (0x6F3D00) - a layout at a target until it finishes. RockTask (0x6F3C20) - a prim model, 10 ticks.
//   SparkTask (0x6F3B80, = Mini Pulse Cannon's 0x67A7C0) - a sequence through the shadow-rotation camera, moving.
//   PrimCallback (0x6F3760) draws every layout object (0x701DD0, header 0x6C, texture window for objects 0 / 1).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the tick it updates; main, arcs, spots, rocks, sparks.
// ENTITY WRITES: the caster - word +0 bit 3, its world bone matrices (0x5095B0) and bone matrices from its pose (0x508C90)
// every tick; the targets' action results (engine). No position word is written.
// CAMERA: the setup's camera animation 0x13FABFC (engine); no camera word is written by the tasks.
// Module globals: 0x25562F8..0x2556384 (context 0x25562F8, caster 0x25562FC, the queues 0x2556300 sparks / 0x2556310
// rocks / 0x2556320 spots / 0x2556330 arcs / 0x2556340 main / 0x2556350 master, master pool 0x2556360, packet cursor
// 0x2556374, seed 0x2556378, flash level 0x255637C, magic buffer base 0x2556380).
// SCREEN-SPACE RIBBONS: the arc edge 0x6F4350 takes the half width 0x2080 / (z + 0x400) in screen units; ported as the PC
// runs it.
// NEVER-WRITTEN STACK WORDS:
//   - the arc draw's joint direction (entry esp - 8) is read before it is written when the first two points project
//     onto one pixel: its last writer is the arc task's own push of its control point's address (ArcResidue).
//   - the 4th dword of the muzzle point (main entry esp - 0x4C) is stored with it at node +0x1C (MainPointPad; never read).
//   - the 4th words of the arc's end points land in the Field_Alloc block's +6 / +0x76 (never read).
// Fujin's described Mega Pulse Cannon (effect_287.json) runs this code: its memory through own::R() (fx_port.h), its
// spawns through own::Fn(), the setup ported below (it has no file: its TIMs are exe data); its parameters
// (MegaPulseParams: the sound, the main task's ticks, the muzzle point's follow, the arcs (as Mega Spark's 279), the
// falling sparks, the rocks, the screen flash, the charge sparks (as Mini Pulse Cannon's 286), the cannon and beam, the
// spots; its data: the three layouts, the rock mesh, the two flipbooks, the muzzle point, the spots' targets, the
// arcs' triangle rows, the two TIMs and the camera track). FX_PART tags its parts in the lab.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>
#include <intrin.h>

namespace ff8fx
{
namespace megapulse287
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x25562F8)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x25562FC)); }        // caster entity (setup)
	inline TaskQueue *QSparks() { return (TaskQueue *)own::R(0x2556300); }         // pool: magic buffer, 0x40 x 0x24
	inline TaskQueue *QRocks() { return (TaskQueue *)own::R(0x2556310); }          // pool: + 0x900, 0x10 x 0x20
	inline TaskQueue *QSpots() { return (TaskQueue *)own::R(0x2556320); }          // pool: + 0xB00, 3 x 0x1D0
	inline TaskQueue *QArcs() { return (TaskQueue *)own::R(0x2556330); }           // pool: + 0x21FF4, 0x40 x 0x40
	inline TaskQueue *QMain() { return (TaskQueue *)own::R(0x2556340); }           // pool: + 0x1070, 1 x 0x9A8
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x2556374)); }
	inline uint32_t &Flash() { return var<uint32_t>(own::R(0x255637C)); }          // screen flash level
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2556380)); }        // Magic_TextureOFF (magic buffer)
	inline uint32_t &Seed() { return var<uint32_t>(own::R(0x2556378)); }           // rand() at the setup (never read)
	inline TaskQueue *QMaster() { return (TaskQueue *)own::R(0x2556350); }         // pool 0x2556360, 1 x 0x14
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }    // the frame's packet arena

	static const uint32_t ORIG_MasterTask = 0x6F2F20;
	static const uint32_t ORIG_MainTask = 0x6F31A0;
	static const uint32_t ORIG_SparkTask = 0x6F3B80;
	static const uint32_t ORIG_RockTask = 0x6F3C20;
	static const uint32_t ORIG_SpotTask = 0x6F3D00;
	static const uint32_t ORIG_ArcTask = 0x6F3DF0;
	static const uint32_t ORIG_PrimCallback = 0x6F3760;  // not a task: the layouts' callback
	static const uint32_t POOL_Rocks = 0x900, POOL_Spots = 0xB00, POOL_Main = 0x1070, POOL_Arcs = 0x21FF4;
	static const uint32_t BUF_Morph = 0x1A18;             // blended vertex frames
	static const uint32_t BUF_Profile = 0x1F98, BUF_ProfileEnd = 0x1FA8; // arc profile, 8 words
	static const uint32_t BUF_Stream = 0x1FA8;            // stream state
	static const uint32_t BUF_StreamDst = 0x21FF0;        // the streamed file (over the arc pool, emptied on counter 0x1E)
	static const uint32_t ARENA_A = 0x1FF0, ARENA_B = 0x11FF0;          // packet arenas
	static const uint32_t MODEL_Cannon = 0x13EF924;       // prim-model layouts
	static const uint32_t MODEL_Beam = 0x13F87E8;
	static const uint32_t MODEL_Spot = 0x13F8F5C;
	static const uint32_t MODEL_Rock = 0x13EF72C;         // prim model (0x572200)
	static const uint32_t SEQ_Charge = 0x13EF448;         // sprite sequences
	static const uint32_t SEQ_Spark = 0x13EF5E8;
	static const uint32_t POINT_Muzzle = 0x13FAEA0;       // the muzzle's bone offset (word +3 = bone)
	static const uint32_t TBL_SpotTargets = 0x13FAEA8;    // 3 bytes: the target of the spots of counters 0x28 / 0x2C / 0x30
	static const uint32_t TRI_Rows = 0x13FAEB0;           // 13 triangle rows (0x18 each)
	static const uint32_t TBL_Distance = 0x13FAFE8;       // 128 x 128 bytes: screen distance of (|dx|, |dy|)
	static const uint32_t TBL_Lengths = 0x13FEF68;        // u16 square-root table
	static const int32_t STREAM_File = 0x168;
	static const void *const SOUND_Cannon = (const void *)0x13FAE98;
	static const uint32_t QUAD_UV0 = 0x3E14C048, QUAD_UV1 = 0xB9C078; // ribbon quads: uv0 + clut, uv1 + tpage, uv2, uv3
	static const uint16_t QUAD_UV2 = 0xC048, QUAD_UV3 = 0xC078;
	static const uint32_t ORIG_Setup = 0x6F2E80;
	static const uint32_t TIM_A = 0x140016C, TIM_B = 0x142938C, CAMERA_ANIM = 0x13FABFC;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h); the arcs and their ribbon read as Mega Spark's (279), the charge
	// sparks as Mini Pulse Cannon's (286)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_CANNON = 1, PART_BEAM = 2, PART_ARCS = 3, PART_CHARGE = 4, PART_SPARKS = 5, PART_ROCKS = 6, PART_SPOTS = 7 };
	struct MegaPulseParams
	{
		uint32_t sound;                                   // at the main task's sound tick
		struct { int16_t sound, stream, damage, end; } director;   // the main task's ticks
		struct { int16_t from, ramp, step, out, out_ticks, out_step, level; } flash;   // the screen flash's level
		struct { int16_t follow, depth, scroll; } cannon;
		struct { int16_t until, rows, clear, amp_shift, bright, fade_from, fade_base, fade_shift; int32_t width, width_depth; } arcs;
		struct { int16_t clut, tpage; uint8_t u_left, u_right, v_from, v_to; } ribbon;   // the arcs' quads
		struct { int16_t from, until, count, bone, rise, rise_rand, life; fxp::Range shadow; } sparks;
		struct { int16_t from, until, every, radius_shift, ahead, rise, rise_rand, gravity, speed, life; fxp::Range angle, spin; } rocks;
		struct { int16_t ticks, count, life, shadow, angle_start, angle_tick, angle_step; fxp::Range angle, radius, depth; } charge;
		struct { int16_t from, every, count, back, depth; } spots;
		struct { uint32_t charge, spark; } seqs;          // the sprite sequences drawn (flipbooks)
		struct { uint32_t cannon, beam, spot; } layouts;  // the prim-model layouts played
		struct { uint32_t rock; } model_data;             // the prim models drawn (meshes)
		struct { uint32_t muzzle, spot_targets, triangles; } tables;   // its small tables (data)
		struct { uint32_t tim_a, tim_b, camera; } res;    // its resources (data): the TIMs, the camera track
	};
	static const MegaPulseParams GAME = {
		0x13FAE98,
		{ 0, 0x37, 0x41, 0x4B },
		{ 0x21, 8, 0x100, 0x3B, 0x10, 0x80, 0x800 },
		{ 0x24, -0x80, 0x20 },
		{ 0x1D, 0xD, 0x1E, 0xC, 0x80, 0xA, 0x1400, 7, 0x2080, 0x400 },
		{ 0x3E14, 0xB9, 0x48, 0x78, 0xC0, 0xC0 },
		{ 0x20, 0x25, 6, 0x16, 0x14, 0x10, 0xC, { 0x1000, 0x13FF } },
		{ 0x1E, 0x3B, 2, 1, 0x1388, 0x20, 0x20, 0xA, 0x190, 0xA, { 0, 0x7FF }, { 0x200, 0x3FF } },
		{ 8, 4, 0xE, 0x7D0, 0x1000, 0x80, -0x400, { -0x20, 0x1F }, { 0x400, 0xBFF }, { -0x80, 0x7F } },
		{ 0x28, 4, 3, 0x1F4, -0x80 },
		{ SEQ_Charge, SEQ_Spark },
		{ MODEL_Cannon, MODEL_Beam, MODEL_Spot },
		{ MODEL_Rock },
		{ POINT_Muzzle, TBL_SpotTargets, TRI_Rows },
		{ TIM_A, TIM_B, CAMERA_ANIM } };
	static MegaPulseParams live = GAME;
	static const MegaPulseParams &P() { return live; }
	// a random in a range (the game's `rand() & mask` + min for its power-of-two spans); an empty range: its min
	inline int32_t Roll(const fxp::Range &r, int32_t random)
	{
		const int32_t span = (int32_t)r.max - r.min + 1;
		return span > 0 ? r.min + random % span : r.min;
	}
	inline int32_t Below(int32_t n, int32_t random) { return n > 0 ? random % n : 0; }
	inline uint32_t MuzzlePoint() { return fxp::data(P().tables.muzzle, POINT_Muzzle, 8); }
	inline uint32_t SpotTargets() { return fxp::data(P().tables.spot_targets, TBL_SpotTargets, 3, 1); }
	inline uint32_t Triangles() { return fxp::data(P().tables.triangles, TRI_Rows, 0x138); }

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // MatrixMultiply3x3: a = a x b (3x3)
	inline void MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); } // MAG_063_RenderPrimSet
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }   // Effect_RotationYMatrix
	inline void TriangleCentreNormal(uint32_t bones, uint32_t tri, int16_t *centre, int16_t *normal) { fn<void (__cdecl *)(uint32_t, uint32_t, int16_t *, int16_t *)>(0x704130)(bones, tri, centre, normal); } // MAG_TriangleCentreNormal
	inline void TransformCoordinateToCameraSpace(const void *v, int32_t *out, void *flag) { fn<void (__cdecl *)(const void *, int32_t *, void *)>(0x56C820)(v, out, flag); }
	inline int32_t SharedRand() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }   // Effect_SharedLcg_Rand
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void StreamQueueLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamPreload(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); } // au_re_BattleFile_preLoad_0

	template<typename T> inline T &AT(uint32_t a, int32_t off = 0) { return *(T *)(a + (uint32_t)off); }
	inline int16_t &S16(uint32_t a, int32_t off) { return AT<int16_t>(a, off); }
	inline uint16_t &U16(uint32_t a, int32_t off) { return AT<uint16_t>(a, off); }
	inline uint32_t &U32(uint32_t a, int32_t off) { return AT<uint32_t>(a, off); }
	inline uint8_t &U8(uint32_t a, int32_t off) { return AT<uint8_t>(a, off); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2556360)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x9A8 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t ntargets;  // +0x0E action 0's target count
		int32_t point[4];  // +0x10 the muzzle point (counters 0..0x23; [3] never written: a stack dword)
		uint8_t *targets[3]; // +0x20 their entities
		uint8_t cannon[0x8C4]; // +0x2C layout 0x13EF924
		uint8_t beam[0xB8];    // +0x8F0 layout 0x13F87E8
	};
	struct SparkNode // pool of 0x40 nodes of 0x24 bytes
	{
		TaskNode hdr;
		uint32_t seq;      // +0x0C sprite sequence
		int32_t shadow;    // +0x10 shadow-rotation camera distance
		int16_t pos[3];    // +0x14
		int16_t frame;     // +0x1A flipbook frame
		int16_t vel[3];    // +0x1C
		int16_t life;      // +0x22
	};
	struct RockNode // pool of 0x10 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t vy;        // +0x16
		int16_t angles[3]; // +0x18
		int16_t spin;      // +0x1E x angle step
	};
	struct SpotNode // pool of 3 nodes of 0x1D0 bytes
	{
		TaskNode hdr;
		int16_t pos[4];    // +0x0C the target's default effect position (y - 0x1F4)
		uint8_t layout[0x1BC]; // +0x14 layout 0x13F8F5C
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
		int16_t depth;     // +0x20 the prim header's +0x18
		uint8_t second;    // +0x22 object 0 of the beam layout: the other texture half
		uint8_t scroll;    // +0x23 texture scroll of objects 0 / 1 (0: none)
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x9A8 && sizeof(SparkNode) == 0x24, "Mega Pulse Cannon nodes");
	static_assert(sizeof(RockNode) == 0x20 && sizeof(SpotNode) == 0x1D0 && sizeof(ArcNode) == 0x40, "Mega Pulse Cannon nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Mega Pulse Cannon prim block");

	// ------------------------------------------------------------------
	// Never-written stack words (see the module comment)
	// ------------------------------------------------------------------
	static uint32_t g_master_esp = 0;   // MasterTask's entry esp (its return address slot)
	// the arc task runs at the master's entry esp - 0x38; its last write at the draw's joint direction is the push of its
	// control point's address (arc entry esp - 0x28)
	static inline uint32_t ArcResidue() { return g_master_esp - 0x38 - 0x28; }
	static int32_t MainPointPad() { return 0; }

	// 0x6F3A80: the offset v (word +3 = bone) through the caster's bone matrix -> out (3 int32)
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

	// 0x6F3AD0: the point (0, 0, the parent bone's length) of bone idx (idx >= 0xF0: the skeleton's bone table) of the
	// entity -> out (3 int16)
	static void BoneTipPoint(const uint8_t *ent, int32_t idx, int16_t *out)
	{
		const uint32_t blk = (uint32_t)FieldAlloc(0x3C);
		U32(blk, 0x34) = 0;
		const uint32_t skel = **(const uint32_t *const *)(ent + 0x64);
		const uint32_t bones = skel + 0x10;
		if (idx >= 0xF0) idx = U8(skel, idx - 0xEC);
		const uint32_t bone = bones + (uint32_t)idx * 0x30;
		const int16_t parent = S16(bone, 0);
		if (parent >= 0) S16(blk, 0x38) = S16(bones + (uint32_t)(int32_t)parent * 0x30, 2);
		else S16(blk, 0x38) = 0;
		GteSetRotMatrix((const Mat4x3 *)(bone + 0x10));
		GteSetTransVector((const Mat4x3 *)(bone + 0x10));
		TransformCoordinateToCameraSpace((const void *)(blk + 0x34), (int32_t *)(blk + 0x20), (void *)(blk + 0x30));
		out[0] = S16(blk, 0x20);
		out[1] = S16(blk, 0x24);
		out[2] = S16(blk, 0x28);
		FieldFree(0x3C);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag287_mega_pulse_cannon_held.h"
#endif

namespace ff8fx
{
namespace megapulse287
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x6F3760): one object of a layout. Vertex frame (or blend into the parameter block's morph
	// buffer), rotation from the record's angles, the record's offset (flag 0x800: turned by that rotation first) through
	// the parameter matrix (flag 0x200: as is), the matrix composed with it (unless flag 0x200 or 0x8000), scale (flag
	// 0x100: 3x3 product, else scale3DMatrix), fade (flag 0x4000: no blend bits), the texture scroll of objects 0 / 1,
	// drawn by 0x701DD0 (header 0x6C; the fields it does not set keep the Field_Alloc scratch)
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x6C);
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
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x800) MatrixMultiplyVector(&m, off, off);
		if (r->flags & 0x200)
		{
			m.t[0] = off[0];
			m.t[1] = off[1];
			m.t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&arg->m, &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
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
		*(uint32_t *)(h + 0x1C) = (r->flags & 0x4000) ? 0x2000 : 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint8_t *)(h + 0x1C) |= 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		if (arg->scroll != 0 && (int16_t)r->flags_lo <= 1)
		{
			*(uint16_t *)(h + 0x26) = 0;
			*(uint16_t *)(h + 0x24) = 0;
			*(uint16_t *)(h + 0x2A) = 0x100;
			*(uint16_t *)(h + 0x28) = 0x100;
			*(uint16_t *)(h + 0x30) = 0x100;
			*(uint16_t *)(h + 0x2C) = 0x100;
			if ((int16_t)r->flags_lo == 0 && arg->second == 0)
			{
				*(uint16_t *)(h + 0x2E) = 0;
				*(uint16_t *)(h + 0x32) = 0x80;
			}
			else
			{
				*(uint16_t *)(h + 0x2E) = 0x80;
				*(uint16_t *)(h + 0x32) = 0x80;
			}
			*(uint16_t *)(h + 0x22) = arg->scroll;
		}
		else *(uint16_t *)(h + 0x22) = 0;
		*(int32_t *)(h + 0x18) = arg->depth;
		*(uint16_t *)(h + 0x20) = 0;
		PacketCursor() = RenderPrimSet(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x6C);
	}

	// ------------------------------------------------------------------
	// Screen-space ribbons
	// ------------------------------------------------------------------
	// 0x6F4350 (= Mega Spark's 0x680A80): a point's ribbon edge. The screen point (x, y at +sxy) moves to its -half-width side, the +side
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
			const int32_t dx = (int32_t)S16(next, sxy) - S16(p, sxy);
			int32_t ax = dx < 0 ? -dx : dx;
			if (ax >= 0x80) ax = 0x7F;
			const int32_t dy = (int32_t)S16(p, sxy + 2) - S16(next, sxy + 2);
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
			oy = mul32(w, ny) >> 12;
			ox = mul32(w, nx) >> 12;
		}
		const int16_t x = S16(p, sxy), y = S16(p, sxy + 2);
		S16(p, sxy) = (int16_t)(x - ox);
		S16(p, sxy + 8) = (int16_t)(x + ox);
		S16(p, sxy + 0xA) = (int16_t)(y + oy);
		S16(p, sxy + 2) = (int16_t)(y - oy);
	}
	// 0x6F4350: arc point edge (screen point at +0, z at +4)
	static void ArcEdge(uint32_t p, uint32_t next, const int16_t *prev, int16_t *out) { RibbonEdge(p, next, prev, out, 0, P().arcs.width, P().arcs.width_depth); }

	// one gouraud textured quad (0x34 bytes) of a ribbon: xy0 / xy1 of a point and of the next one, grey levels c0 / c1
	static void RibbonQuad(uint32_t pk, uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1, int32_t c0, int32_t c1, uint32_t bucket)
	{
		U32(pk, 8) = a0;
		U32(pk, 0x14) = a1;
		const auto &q = P().ribbon;
		U32(pk, 0xC) = (uint32_t)(uint16_t)q.clut << 16 | (uint32_t)q.v_from << 8 | q.u_left;      // QUAD_UV0
		U32(pk, 0x18) = (uint32_t)(uint16_t)q.tpage << 16 | (uint32_t)q.v_from << 8 | q.u_right;   // QUAD_UV1
		U16(pk, 0x24) = (uint16_t)(q.v_to << 8 | q.u_left);                                       // QUAD_UV2
		U16(pk, 0x30) = (uint16_t)(q.v_to << 8 | q.u_right);                                      // QUAD_UV3
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

	// 0x6F41C0 (= Mega Spark's 0x6808F0): the 8 points (16 bytes: x, y, z / screen xy, z, edge xy, brightness +0xE) projected,
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
	// Spark (0x6F3B80)
	// ------------------------------------------------------------------
	static void SparkDraw(const SparkNode *s)
	{
		TransformCameraByShadowRotation(s->pos, (int16_t)s->shadow, -(s->shadow >> 2));
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = s->seq;
		*(int16_t *)(h + 4) = fxp::last_frame(s->seq, s->frame);
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag287_mega_pulse_cannon_held.inc
		FX_HELD(held_note_spark(s);)
		FX_PART(s->seq == fxp::sequence(P().seqs.spark, SEQ_Spark) ? PART_SPARKS : PART_CHARGE);
		SparkDraw(s);
		FX_PART(0);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->frame++;
		return s->frame < s->life ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Rock (0x6F3C20): a prim model at its position and angles, falling and bouncing on y = 0, 10 ticks
	// ------------------------------------------------------------------
	static void RockDraw(const RockNode *k)
	{
		uint8_t *h = AllocHeader(0x58);
		Mat4x3 m;
		BuildRotationMatrixFromAngles(k->angles, &m);
		m.t[0] = k->pos[0];
		m.t[1] = k->pos[1];
		m.t[2] = k->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = fxp::model(P().model_data.rock, MODEL_Rock);
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl RockTask(TaskNode *n)
	{
		RockNode *k = (RockNode *)n;
		// 30 fps layer: see mag287_mega_pulse_cannon_held.inc
		FX_HELD(held_note_rock(k);)
		FX_PART(PART_ROCKS);
		RockDraw(k);
		FX_PART(0);
		k->vy = (int16_t)(k->vy + P().rocks.gravity);
		k->angles[0] = (int16_t)(k->angles[0] + k->spin);
		k->pos[2] = (int16_t)(k->pos[2] + P().rocks.speed);
		k->pos[1] = (int16_t)(k->pos[1] + k->vy);
		if (k->pos[1] > 0) k->vy = (int16_t)-k->vy;
		k->counter++;
		return k->counter < P().rocks.life ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Spot (0x6F3D00): its layout at its position until the layout ends
	// ------------------------------------------------------------------
	static uint32_t __cdecl SpotTask(TaskNode *n)
	{
		SpotNode *p = (SpotNode *)n;
		PrimArg arg;
		arg.second = 0;   // never written (read only with a scroll)
		RotY(0, &arg.m);
		arg.m.t[0] = p->pos[0];
		arg.m.t[1] = p->pos[1];
		arg.m.t[2] = p->pos[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.morph = TexBase() + BUF_Morph;
		arg.depth = P().spots.depth;
		arg.scroll = 0;
		// 30 fps layer: see mag287_mega_pulse_cannon_held.inc
		FX_HELD(held_note_spot(p, &arg);)
		FX_PART(PART_SPOTS);
		const int playing = prim::play((prim::Layout *)p->layout, PrimCallback, (int)&arg, 0);
		FX_PART(0);
		return playing ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Arc (0x6F3DF0, = Mega Spark's 0x680520)
	// ------------------------------------------------------------------
	// 0x6F3D80: an arc of the caster's skeleton from the triangle row / anchor `from` to the bone offset `to`
	static void SpawnArc(uint32_t model, uint32_t from, uint32_t to, uint8_t anchor, uint8_t amp_shift, uint8_t fade_shift)
	{
		ArcNode *a = (ArcNode *)AddTaskToQueue(QArcs(), own::Fn(ORIG_ArcTask));
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
			a->bright[k][0] = P().arcs.bright;
			a->bright[k][1] = 0;
		}
	}

	// the arc's 8 points (the caster's world bone matrices, the shared LCG jitter through rnd) into a Field_Alloc block
	template<typename R> static uint32_t ArcPoints(const ArcNode *a, R rnd)
	{
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
			S16(q, 0) = (int16_t)(S16(q, 0) + (mul32(rnd() - 0x4000, e) >> 18));
			S16(q, 2) = (int16_t)(S16(q, 2) + (mul32(rnd() - 0x4000, e) >> 18));
			S16(q, 4) = (int16_t)(S16(q, 4) + (mul32(rnd() - 0x4000, e) >> 18));
		}
		return b;
	}

	static uint32_t __cdecl ArcTask(TaskNode *n)
	{
		ArcNode *a = (ArcNode *)n;
		const uint32_t b = ArcPoints(a, SharedRand);
		// 30 fps layer: see mag287_mega_pulse_cannon_held.inc
		FX_HELD(held_note_arc(a, b, ArcResidue());)
		FX_PART(PART_ARCS);
		ArcDraw(b, ArcResidue());
		FX_PART(0);
		FieldFree(0x80);
		if (a->counter >= P().arcs.fade_from)
		{
			int32_t lit = 0;
			for (int k = 0; k < 8; k++)
			{
				const int32_t dec = (P().arcs.fade_base - S16(TexBase(), BUF_Profile + 2 * k)) >> a->fade_shift;
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
	// Main (0x6F31A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		*(uint16_t *)Caster() &= 0xFFF7;
		const int32_t c = m->counter;
		if ((uint32_t)c < (uint32_t)P().director.end)
		{
			int32_t pt[4];
			if ((uint32_t)c < (uint32_t)P().cannon.follow)
			{
				BonePoint((const int16_t *)MuzzlePoint(), pt);
				pt[3] = MainPointPad();
				for (int k = 0; k < 4; k++) m->point[k] = pt[k];
			}
			else
				for (int k = 0; k < 4; k++) pt[k] = m->point[k];
			if (c <= P().arcs.until)
			{
				const int32_t rows = P().arcs.rows < 1 ? 1 : P().arcs.rows > 13 ? 13 : P().arcs.rows;   // (13 in its table)
				SpawnArc(U32((uint32_t)Caster(), 0x64), Triangles() + ((uint32_t)c % (uint32_t)rows) * 0x18, MuzzlePoint(), 0,
					(uint8_t)P().arcs.amp_shift, (uint8_t)P().arcs.fade_shift);
				*Caster() |= 8;
			}
			const auto &S = P().sparks;
			if (c >= S.from && c <= S.until)
				for (int32_t i = S.count - 1; i >= 0; i--)
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), own::Fn(ORIG_SparkTask));
					if (!s) continue;
					s->seq = fxp::sequence(P().seqs.spark, SEQ_Spark);
					BoneTipPoint(Caster(), i + S.bone, s->pos);
					s->vel[2] = 0;
					s->vel[0] = 0;
					s->vel[1] = (int16_t)(-S.rise - Below(S.rise_rand, CrtRand()));
					s->life = S.life;
					s->frame = 0;
					s->shadow = Roll(S.shadow, CrtRand());
				}
			if (c == P().arcs.clear)
			{
				// the arc queue is emptied: its running arcs are dropped as they are
				U32((uint32_t)QArcs(), 4) = 0;
				U32((uint32_t)QArcs(), 0) = 0;
			}
			const auto &K = P().rocks;
			if (K.every > 0 && c >= K.from && c <= K.until && (c - K.from) % K.every == 0)
			{
				RockNode *k = (RockNode *)AddTaskToQueue(QRocks(), own::Fn(ORIG_RockTask));
				if (k)
				{
					const int32_t a = Roll(K.angle, CrtRand());
					const int32_t shift = K.radius_shift & 31;
					k->pos[0] = (int16_t)((ComputeCos(a) >> shift) + *(const int16_t *)(Caster() + 0x1C));
					const int32_t s = ComputeSin(a);
					k->pos[1] = 0;
					k->pos[2] = (int16_t)(*(const int16_t *)(Caster() + 0x20) - (int16_t)(s >> shift) + K.ahead);
					k->vy = (int16_t)(-K.rise - Below(K.rise_rand, CrtRand()));
					k->angles[0] = (int16_t)CrtRand();
					k->angles[1] = (int16_t)CrtRand();
					k->angles[2] = (int16_t)CrtRand();
					k->spin = (int16_t)Roll(K.spin, CrtRand());
				}
			}
			const auto &F = P().flash;
			if ((uint32_t)c >= (uint32_t)F.from)
			{
				if ((uint32_t)(c - F.from) < (uint32_t)F.ramp) Flash() = (uint32_t)mul32(c - F.from, F.step);
				else if ((uint32_t)(c - F.out) < (uint32_t)F.out_ticks) Flash() = (uint32_t)mul32(F.out_ticks - (c - F.out), F.out_step);
				else Flash() = (uint32_t)(int32_t)F.level;
			}
			Mat4x3 M;
			M.t[0] = pt[0];
			M.t[1] = pt[1];
			M.t[2] = pt[2];
			RotY(*(const int16_t *)(Caster() + 0xE), &M);
			const auto &C = P().charge;
			if ((uint32_t)c < (uint32_t)C.ticks)
			{
				GteSetRotMatrix(&M);
				if (C.count > 0)
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), own::Fn(ORIG_SparkTask));
					int32_t base = mul32(c, C.angle_tick) + C.angle_start;
					const int32_t life = C.life > 0 ? C.life : 1;
					for (int32_t left = C.count; s; )
					{
						s->seq = fxp::sequence(P().seqs.charge, SEQ_Charge);
						const int32_t a = Roll(C.angle, CrtRand()) + base;
						const int32_t r = Roll(C.radius, CrtRand());
						s->pos[0] = (int16_t)(mul32(ComputeSin(a), r) >> 12);
						s->pos[1] = (int16_t)(mul32(ComputeCos(a), r) >> 12);
						s->pos[2] = (int16_t)Roll(C.depth, CrtRand());
						GteLoadV0(s->pos);
						GteMVMVA_RotV0();
						GteStoreIR123(s->pos);
						const int16_t x = s->pos[0], y = s->pos[1], z = s->pos[2];
						s->life = (int16_t)life;
						s->vel[0] = (int16_t)((int32_t)x / -life);
						s->vel[1] = (int16_t)((int32_t)y / -life);
						s->vel[2] = (int16_t)((int32_t)z / -life);
						s->pos[0] = (int16_t)(x + M.t[0]);
						s->pos[1] = (int16_t)(y + M.t[1]);
						s->pos[2] = (int16_t)(z + M.t[2]);
						s->frame = 0;
						s->shadow = C.shadow;
						base += C.angle_step;
						if (--left == 0) break;
						s = (SparkNode *)AddTaskToQueue(QSparks(), own::Fn(ORIG_SparkTask));
					}
				}
			}
			PrimArg arg;
			ComposeAffineTransform(&Camera(), &M, &arg.m);
			arg.morph = TexBase() + BUF_Morph;
			arg.depth = P().cannon.depth;
			arg.second = 0;
			arg.scroll = (uint8_t)mul32(c & 3, P().cannon.scroll);
			// 30 fps layer: see mag287_mega_pulse_cannon_held.inc
			FX_HELD(held_note_main(m, REC_CANNON, &arg);)
			FX_PART(PART_CANNON);
			prim::play((prim::Layout *)m->cannon, PrimCallback, (int)&arg, 0);
			FX_PART(0);
			M.t[1] = *(const int16_t *)(Caster() + 0x24);
			ComposeAffineTransform(&Camera(), &M, &arg.m);
			arg.second = 1;
			arg.scroll = (uint8_t)mul32(-c & 3, P().cannon.scroll);
			// 30 fps layer: see mag287_mega_pulse_cannon_held.inc
			FX_HELD(held_note_main(m, REC_BEAM, &arg);)
			FX_PART(PART_BEAM);
			prim::play((prim::Layout *)m->beam, PrimCallback, (int)&arg, 0);
			FX_PART(0);
		}
		const auto &T = P().spots;
		const int32_t spots = T.count < 0 ? 0 : T.count > 3 ? 3 : T.count;   // (3 in its table and its pool)
		const int32_t e = m->counter - T.from;
		if (T.every > 0 && (uint32_t)e < (uint32_t)(spots * T.every) && e % T.every == 0)
		{
			const int32_t idx = U8(SpotTargets(), e / T.every);
			if (idx < m->ntargets)
			{
				SpotNode *p = (SpotNode *)AddTaskToQueue(QSpots(), own::Fn(ORIG_SpotTask));
				if (p)
				{
					GetDefaultEffectPosition(m->targets[idx], p->pos);
					p->pos[2] = (int16_t)(p->pos[2] - T.back);
					DecodeModelPrimLayout(fxp::layout(P().layouts.spot, MODEL_Spot, 0x1BC), p->layout, 0x1BC);
				}
			}
		}
		if (m->counter == P().director.sound) BdPlaySE((const void *)fxp::sound(P().sound, (uint32_t)SOUND_Cannon), 0, 0x80);
		if (m->counter == P().director.stream) StreamQueueLoad(STREAM_File, TexBase() + BUF_StreamDst, 0);
		StreamPump();
		if (m->counter == P().director.damage) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		m->counter++;
		if (m->counter < P().director.end) return 0;
		Flash() = 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Setup (0x6F2E80 -> _Init 0x6F2E90; no file loader: its TIMs are exe data): the game runs its own; Fujin's programs
	// this (its memory may be a copy's): the two TIMs, the magic buffer, the seed, the cast and its caster's entity, the
	// master (it sets up the rest), the camera
	// ------------------------------------------------------------------
	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(fxp::tim(P().res.tim_a, TIM_A));
		QueueTimUpload(fxp::tim(P().res.tim_b, TIM_B));
		TexBase() = (uint32_t)layouts::MagicBuffer();
		Seed() = (uint32_t)CrtRand();
		Ctx() = ctx;
		Caster() = (uint8_t *)(0x1D972C0 + 0x9C * ctx->attacker);
		InitTaskQueuePool(QMaster(), (void *)own::R(0x2556360), 0x14, 1);
		Memset32((uint8_t *)AddTaskToQueue(QMaster(), own::Fn(ORIG_MasterTask)) + 0xC, 0, 2);
		PlayCameraAnimation((const void *)fxp::camera(P().res.camera, CAMERA_ANIM));
		return QMaster();
	}

	// ------------------------------------------------------------------
	// Master (0x6F2F20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		g_master_esp = (uint32_t)_AddressOfReturnAddress();
		// 30 fps layer: see mag287_mega_pulse_cannon_held.inc
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
		Flash() = 0;
		if (mn->counter == 1 && !mn->started)
		{
			mn->started = 1;
			InitTaskQueuePool(QMain(), (void *)(TexBase() + POOL_Main), 0x9A8, 1);
			InitTaskQueuePool(QArcs(), (void *)(TexBase() + POOL_Arcs), 0x40, 0x40);
			InitTaskQueuePool(QSpots(), (void *)(TexBase() + POOL_Spots), 0x1D0, 3);
			InitTaskQueuePool(QRocks(), (void *)(TexBase() + POOL_Rocks), 0x20, 0x10);
			InitTaskQueuePool(QSparks(), (void *)TexBase(), 0x24, 0x40);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), own::Fn(ORIG_MainTask));
			Memset32(&m->counter, 0, 0x267);
			DecodeModelPrimLayout(fxp::layout(P().layouts.cannon, MODEL_Cannon, 0x8C4), m->cannon, 0x8C4);
			DecodeModelPrimLayout(fxp::layout(P().layouts.beam, MODEL_Beam, 0xB8), m->beam, 0xB8);
			for (int32_t k = 0; k < 8; k++)
				S16(TexBase(), BUF_Profile + 2 * k) = (int16_t)ComputeSin(shl32(k, 11) / 7);
			m->ntargets = Ctx()->actions->target_count;
			for (int32_t i = 0; i < m->ntargets; i++)
				m->targets[i] = Entity(Ctx()->actions->targets[i * TARGET_STRIDE]);
			StreamStateInit((void *)(TexBase() + BUF_Stream));
		}
		int a0;
		if (mn->started)
		{
			uint8_t *caster = Caster();
			ComputeBonesWorldMatrices(caster + 0x60, caster + 0x40);
			a0 = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QArcs());
			BuildBoneMatricesFromPose(Caster() + 0x60);
			ExecuteTaskQueue(QSpots());
			ExecuteTaskQueue(QRocks());
			ExecuteTaskQueue(QSparks());
		}
		else a0 = (int)n;
		SetScreenFlash(Flash(), 0);
		if (mn->started && !a0)
		{
			if (StreamPreload(0)) return TASK_END;
			mn->counter = 2;
			return 0;
		}
		mn->counter++;
		return 0;
	}
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "director", "Director", "The main task's timeline: the sound, the screen flash's level, the stream load, the damage (every target of action 0), the end" },
			{ PART_CANNON, "cannon", "Cannon", "The cannon layout at the muzzle point (an offset on a bone of the caster, held still after a while), turned with the caster, its texture scrolling" },
			{ PART_BEAM, "beam", "Beam", "The beam layout at the muzzle point at the caster's height, its texture scrolling the other way" },
			{ PART_ARCS, "arcs", "Arcs", "Electric arcs (screen-space ribbons, jittered) over the caster's body from triangles of its skeleton to the muzzle point, an arc a tick; each fades out after a while, the last ones dropped" },
			{ PART_CHARGE, "charge", "Charge sparks", "Sparks round the muzzle point on the first ticks, flying back into it" },
			{ PART_SPARKS, "sparks", "Falling sparks", "Sparks at the tips of a run of the caster's bones, falling" },
			{ PART_ROCKS, "rocks", "Rocks", "Rocks thrown up round the caster, bouncing on the ground" },
			{ PART_SPOTS, "spots", "Spots", "A layout at each of up to three targets in turn" },
		};
		static const fxp::Group GROUPS[] = {
			{ "charge.angle", "Angle random", "A random angle added to each spark's" },
			{ "charge.radius", "Radius", "Its distance from the muzzle point, at random" },
			{ "charge.depth", "Depth", "Its offset along the muzzle's axis, at random" },
			{ "sparks.shadow", "Distance", "A falling spark's camera distance, at random" },
			{ "rocks.angle", "Angle", "Where round the caster, at random" },
			{ "rocks.spin", "Spin", "A rock's spin, at random" },
		};
		static const Field FIELDS[] = {
			{ "director", "sound", "Sound", "At the main task's sound tick", (uint32_t)offsetof(MegaPulseParams, sound), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "director.sound", "Sound tick", "The sound at this tick of the main task", (uint32_t)offsetof(MegaPulseParams, director.sound), fxp::I16, -1, 300 },
			{ "director", "director.stream", "Stream tick", "The stream load (battle file 0x168, into the magic buffer) at this tick; the effect ends once it is done", (uint32_t)offsetof(MegaPulseParams, director.stream), fxp::I16, -1, 300 },
			{ "director", "director.damage", "Damage tick", "The damage (every target of action 0) at this tick", (uint32_t)offsetof(MegaPulseParams, director.damage), fxp::I16, -1, 300 },
			{ "director", "director.end", "End tick", "The main task's last tick + 1 (the effect ends once its parts have and the stream is done)", (uint32_t)offsetof(MegaPulseParams, director.end), fxp::I16, 1, 300 },
			{ "director", "flash.from", "Flash from", "The screen flash brightens from this tick of the main task", (uint32_t)offsetof(MegaPulseParams, flash.from), fxp::I16, 0, 300 },
			{ "director", "flash.ramp", "Flash ramp", "For this many ticks, by Flash step a tick", (uint32_t)offsetof(MegaPulseParams, flash.ramp), fxp::I16, 0, 300 },
			{ "director", "flash.step", "Flash step", "(its level a tick)", (uint32_t)offsetof(MegaPulseParams, flash.step), fxp::I16, -0x8000, 0x7FFF },
			{ "director", "flash.level", "Flash level", "Then this level (4096 = white)", (uint32_t)offsetof(MegaPulseParams, flash.level), fxp::I16, -0x8000, 0x7FFF },
			{ "director", "flash.out", "Flash out", "It fades from this tick", (uint32_t)offsetof(MegaPulseParams, flash.out), fxp::I16, 0, 300 },
			{ "director", "flash.out_ticks", "Flash out ticks", "Over this many ticks", (uint32_t)offsetof(MegaPulseParams, flash.out_ticks), fxp::I16, 0, 300 },
			{ "director", "flash.out_step", "Flash out step", "(its level a tick left)", (uint32_t)offsetof(MegaPulseParams, flash.out_step), fxp::I16, -0x8000, 0x7FFF },
			{ "director", "res.tim_a", "TIM image a", "The TIM image it uploads as it is (a palette when the game's has one)", (uint32_t)offsetof(MegaPulseParams, res.tim_a), fxp::Tim, 0, 0x7FFFFFFF },
			{ "director", "res.tim_b", "TIM image b", "The TIM image it uploads as it is (a palette when the game's has one)", (uint32_t)offsetof(MegaPulseParams, res.tim_b), fxp::Tim, 0, 0x7FFFFFFF },
			{ "director", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(MegaPulseParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "cannon", "layouts.cannon", "Cannon", "The prim-model layout cannon at the muzzle point, turned with the caster (its models' animation)", (uint32_t)offsetof(MegaPulseParams, layouts.cannon), fxp::Layout, 0, 0x7FFFFFFF },
			{ "cannon", "tables.muzzle", "Muzzle point", "The muzzle point: an offset (x, y, z) on a bone of the caster (8 bytes: x, y, z, bone; an edited one keeps its length)", (uint32_t)offsetof(MegaPulseParams, tables.muzzle), fxp::Data, 0, 0x7FFFFFFF },
			{ "cannon", "cannon.follow", "Follow until", "The muzzle point follows the caster's bone until this tick of the main task, then stays", (uint32_t)offsetof(MegaPulseParams, cannon.follow), fxp::I16, 0, 300 },
			{ "cannon", "cannon.depth", "Depth", "The cannon's and the beam's depth offset (their prim header's +0x18)", (uint32_t)offsetof(MegaPulseParams, cannon.depth), fxp::I16, -0x8000, 0x7FFF },
			{ "cannon", "cannon.scroll", "Texture scroll", "Their objects 0 / 1 scroll their texture by this x (tick & 3), the beam the other way", (uint32_t)offsetof(MegaPulseParams, cannon.scroll), fxp::I16, 0, 255 },
			{ "beam", "layouts.beam", "Beam", "The prim-model layout beam at the muzzle point at the caster's height (its models' animation)", (uint32_t)offsetof(MegaPulseParams, layouts.beam), fxp::Layout, 0, 0x7FFFFFFF },
			{ "arcs", "arcs.until", "Until", "An arc a tick over the caster's body to the muzzle point (from the next triangle row) up to this tick of the main task", (uint32_t)offsetof(MegaPulseParams, arcs.until), fxp::I16, -1, 300 },
			{ "arcs", "arcs.rows", "Rows", "The triangle rows taken in turn (13 at most: its table's)", (uint32_t)offsetof(MegaPulseParams, arcs.rows), fxp::I16, 1, 13 },
			{ "arcs", "arcs.clear", "Clear tick", "At this tick the arcs still running are dropped", (uint32_t)offsetof(MegaPulseParams, arcs.clear), fxp::I16, -1, 300 },
			{ "arcs", "arcs.amp_shift", "Bend", "How far an arc bows out: its length >> this", (uint32_t)offsetof(MegaPulseParams, arcs.amp_shift), fxp::I16, 0, 31 },
			{ "arcs", "arcs.bright", "Brightness", "An arc's brightness when it starts (128 = the texture's)", (uint32_t)offsetof(MegaPulseParams, arcs.bright), fxp::I16, 0, 255 },
			{ "arcs", "arcs.fade_from", "Fade from", "From this tick of its own an arc fades (its ends faster) and ends when one point is left lit", (uint32_t)offsetof(MegaPulseParams, arcs.fade_from), fxp::I16, 0, 255 },
			{ "arcs", "arcs.fade_base", "Fade", "A point's fade a tick: (this - its profile) >> Fade shift", (uint32_t)offsetof(MegaPulseParams, arcs.fade_base), fxp::I16, -0x8000, 0x7FFF },
			{ "arcs", "arcs.fade_shift", "Fade shift", "(the game's 7)", (uint32_t)offsetof(MegaPulseParams, arcs.fade_shift), fxp::I16, 0, 31 },
			{ "arcs", "arcs.width", "Width", "The ribbon's half width on screen: this / (depth + Width depth)", (uint32_t)offsetof(MegaPulseParams, arcs.width), fxp::I32, 0, 0x7FFFFFFF },
			{ "arcs", "arcs.width_depth", "Width depth", "(its depth offset)", (uint32_t)offsetof(MegaPulseParams, arcs.width_depth), fxp::I32, -32768, 0x7FFFFFFF },
			{ "arcs", "ribbon.clut", "Ribbon palette", "The arcs' ribbon quads: their palette (CLUT word)", (uint32_t)offsetof(MegaPulseParams, ribbon.clut), fxp::I16, 0, 0x7FFF },
			{ "arcs", "ribbon.tpage", "Ribbon page", "Their texture page word", (uint32_t)offsetof(MegaPulseParams, ribbon.tpage), fxp::I16, 0, 0x7FFF },
			{ "arcs", "ribbon.u_left", "Ribbon u left", "The texture's u across the ribbon: on its one side", (uint32_t)offsetof(MegaPulseParams, ribbon.u_left), fxp::U8, 0, 255 },
			{ "arcs", "ribbon.u_right", "Ribbon u right", "On its other side", (uint32_t)offsetof(MegaPulseParams, ribbon.u_right), fxp::U8, 0, 255 },
			{ "arcs", "ribbon.v_from", "Ribbon v from", "The texture's v at a quad's first point", (uint32_t)offsetof(MegaPulseParams, ribbon.v_from), fxp::U8, 0, 255 },
			{ "arcs", "ribbon.v_to", "Ribbon v to", "At its next point", (uint32_t)offsetof(MegaPulseParams, ribbon.v_to), fxp::U8, 0, 255 },
			{ "arcs", "tables.triangles", "Triangle rows", "The arcs' starts: 13 triangles on the caster's bones, each three offsets (x, y, z, bone) - an arc leaves its centre along its normal (312 bytes; an edited one keeps its length)", (uint32_t)offsetof(MegaPulseParams, tables.triangles), fxp::Data, 0, 0x7FFFFFFF },
			{ "charge", "seqs.charge", "Spark", "The sprite sequence of a charge spark (a flipbook of 14 frames: an edited one keeps as many; a spark holds its last past it)", (uint32_t)offsetof(MegaPulseParams, seqs.charge), fxp::Seq, 0, 0x7FFFFFFF },
			{ "charge", "charge.ticks", "Ticks", "Charge sparks on the main task's first ticks, up to this one", (uint32_t)offsetof(MegaPulseParams, charge.ticks), fxp::I16, 0, 300 },
			{ "charge", "charge.count", "A tick", "How many a tick (64 sparks at most at once)", (uint32_t)offsetof(MegaPulseParams, charge.count), fxp::I16, 0, 64 },
			{ "charge", "charge.life", "Life", "A spark's ticks: it flies back to the muzzle point in as many", (uint32_t)offsetof(MegaPulseParams, charge.life), fxp::I16, 1, 100 },
			{ "charge", "charge.shadow", "Distance", "Its sprite's camera distance (the shadow-rotation camera: this and -this / 4)", (uint32_t)offsetof(MegaPulseParams, charge.shadow), fxp::I16, -0x8000, 0x7FFF },
			{ "charge", "charge.angle_start", "Angle", "The first spark's angle round the muzzle at tick 0 (4096 = a full turn)", (uint32_t)offsetof(MegaPulseParams, charge.angle_start), fxp::I16, -0x8000, 0x7FFF, fxp::Angle },
			{ "charge", "charge.angle_tick", "Angle a tick", "Added each tick", (uint32_t)offsetof(MegaPulseParams, charge.angle_tick), fxp::I16, -0x8000, 0x7FFF, fxp::Angle },
			{ "charge", "charge.angle_step", "Angle step", "Added for each next spark of the tick", (uint32_t)offsetof(MegaPulseParams, charge.angle_step), fxp::I16, -0x8000, 0x7FFF, fxp::Angle },
			{ "charge", "charge.angle.min", "Angle random", "A random angle added: from", (uint32_t)offsetof(MegaPulseParams, charge.angle.min), fxp::I16, -0x8000, 0x7FFF, fxp::Angle },
			{ "charge", "charge.angle.max", "Angle random to", "(to)", (uint32_t)offsetof(MegaPulseParams, charge.angle.max), fxp::I16, -0x8000, 0x7FFF, fxp::Angle },
			{ "charge", "charge.radius.min", "Radius", "Its distance from the muzzle point: from", (uint32_t)offsetof(MegaPulseParams, charge.radius.min), fxp::I16, 0, 0x7FFF },
			{ "charge", "charge.radius.max", "Radius to", "(to)", (uint32_t)offsetof(MegaPulseParams, charge.radius.max), fxp::I16, 0, 0x7FFF },
			{ "charge", "charge.depth.min", "Depth", "Its offset along the muzzle's axis: from", (uint32_t)offsetof(MegaPulseParams, charge.depth.min), fxp::I16, -0x8000, 0x7FFF },
			{ "charge", "charge.depth.max", "Depth to", "(to)", (uint32_t)offsetof(MegaPulseParams, charge.depth.max), fxp::I16, -0x8000, 0x7FFF },
			{ "sparks", "seqs.spark", "Spark", "The sprite sequence of a falling spark (a flipbook of 12 frames: an edited one keeps as many; a spark holds its last past it)", (uint32_t)offsetof(MegaPulseParams, seqs.spark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "sparks", "sparks.from", "From", "Falling sparks at the tips of the caster's bones from this tick of the main task", (uint32_t)offsetof(MegaPulseParams, sparks.from), fxp::I16, 0, 300 },
			{ "sparks", "sparks.until", "Until", "Until this tick", (uint32_t)offsetof(MegaPulseParams, sparks.until), fxp::I16, -1, 300 },
			{ "sparks", "sparks.count", "A tick", "How many a tick: one per bone from the first", (uint32_t)offsetof(MegaPulseParams, sparks.count), fxp::I16, 0, 64 },
			{ "sparks", "sparks.bone", "First bone", "The first bone (the others follow it)", (uint32_t)offsetof(MegaPulseParams, sparks.bone), fxp::I16, 0, 255 },
			{ "sparks", "sparks.rise", "Fall", "A spark's y velocity: -this - a random below Fall random (it does not accelerate)", (uint32_t)offsetof(MegaPulseParams, sparks.rise), fxp::I16, -0x8000, 0x7FFF },
			{ "sparks", "sparks.rise_rand", "Fall random", "(the random's span)", (uint32_t)offsetof(MegaPulseParams, sparks.rise_rand), fxp::I16, 0, 0x7FFF },
			{ "sparks", "sparks.life", "Life", "A spark's ticks (a frame a tick)", (uint32_t)offsetof(MegaPulseParams, sparks.life), fxp::I16, 1, 100 },
			{ "sparks", "sparks.shadow.min", "Distance", "Its sprite's camera distance, at random: from", (uint32_t)offsetof(MegaPulseParams, sparks.shadow.min), fxp::I16, -0x8000, 0x7FFF },
			{ "sparks", "sparks.shadow.max", "Distance to", "(to)", (uint32_t)offsetof(MegaPulseParams, sparks.shadow.max), fxp::I16, -0x8000, 0x7FFF },
			{ "rocks", "model_data.rock", "Rock", "The prim model rock (a mesh of 12 vertices: an edited one keeps its vertices)", (uint32_t)offsetof(MegaPulseParams, model_data.rock), fxp::Model, 0, 0x7FFFFFFF },
			{ "rocks", "rocks.from", "From", "A rock thrown up round the caster from this tick of the main task", (uint32_t)offsetof(MegaPulseParams, rocks.from), fxp::I16, 0, 300 },
			{ "rocks", "rocks.until", "Until", "Until this tick", (uint32_t)offsetof(MegaPulseParams, rocks.until), fxp::I16, -1, 300 },
			{ "rocks", "rocks.every", "Every", "Every this many ticks", (uint32_t)offsetof(MegaPulseParams, rocks.every), fxp::I16, 0, 300 },
			{ "rocks", "rocks.angle.min", "Angle", "Where round the caster, at random: from (4096 = a full turn)", (uint32_t)offsetof(MegaPulseParams, rocks.angle.min), fxp::I16, -0x8000, 0x7FFF, fxp::Angle },
			{ "rocks", "rocks.angle.max", "Angle to", "(to)", (uint32_t)offsetof(MegaPulseParams, rocks.angle.max), fxp::I16, -0x8000, 0x7FFF, fxp::Angle },
			{ "rocks", "rocks.radius_shift", "Radius shift", "Its distance from the caster: 4096 >> this", (uint32_t)offsetof(MegaPulseParams, rocks.radius_shift), fxp::I16, 0, 31 },
			{ "rocks", "rocks.ahead", "Ahead", "Moved this far along z", (uint32_t)offsetof(MegaPulseParams, rocks.ahead), fxp::I16, -0x8000, 0x7FFF },
			{ "rocks", "rocks.rise", "Rise", "Its y velocity: -this - a random below Rise random", (uint32_t)offsetof(MegaPulseParams, rocks.rise), fxp::I16, -0x8000, 0x7FFF },
			{ "rocks", "rocks.rise_rand", "Rise random", "(the random's span)", (uint32_t)offsetof(MegaPulseParams, rocks.rise_rand), fxp::I16, 0, 0x7FFF },
			{ "rocks", "rocks.gravity", "Gravity", "Added to its y velocity a tick (it bounces off the ground)", (uint32_t)offsetof(MegaPulseParams, rocks.gravity), fxp::I16, -0x8000, 0x7FFF },
			{ "rocks", "rocks.speed", "Speed", "Its z step a tick", (uint32_t)offsetof(MegaPulseParams, rocks.speed), fxp::I16, -0x8000, 0x7FFF },
			{ "rocks", "rocks.spin.min", "Spin", "Its x spin a tick, at random: from", (uint32_t)offsetof(MegaPulseParams, rocks.spin.min), fxp::I16, -0x8000, 0x7FFF, fxp::Angle },
			{ "rocks", "rocks.spin.max", "Spin to", "(to)", (uint32_t)offsetof(MegaPulseParams, rocks.spin.max), fxp::I16, -0x8000, 0x7FFF, fxp::Angle },
			{ "rocks", "rocks.life", "Life", "A rock's ticks", (uint32_t)offsetof(MegaPulseParams, rocks.life), fxp::I16, 1, 300 },
			{ "spots", "layouts.spot", "Spot", "The prim-model layout spot at a target (its models' animation)", (uint32_t)offsetof(MegaPulseParams, layouts.spot), fxp::Layout, 0, 0x7FFFFFFF },
			{ "spots", "spots.from", "From", "A spot at a target from this tick of the main task", (uint32_t)offsetof(MegaPulseParams, spots.from), fxp::I16, 0, 300 },
			{ "spots", "spots.every", "Every", "Every this many ticks", (uint32_t)offsetof(MegaPulseParams, spots.every), fxp::I16, 1, 300 },
			{ "spots", "spots.count", "Count", "This many (3 at most: its table's and its pool's)", (uint32_t)offsetof(MegaPulseParams, spots.count), fxp::I16, 0, 3 },
			{ "spots", "spots.back", "Back", "Moved this far back along z from the target's effect point", (uint32_t)offsetof(MegaPulseParams, spots.back), fxp::I16, -0x8000, 0x7FFF },
			{ "spots", "spots.depth", "Depth", "Its depth offset", (uint32_t)offsetof(MegaPulseParams, spots.depth), fxp::I16, -0x8000, 0x7FFF },
			{ "spots", "tables.spot_targets", "Spot targets", "Which target of the action each spot is on (3 bytes; a spot whose target the action does not have is left out)", (uint32_t)offsetof(MegaPulseParams, tables.spot_targets), fxp::Data, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag287_mega_pulse_cannon()
	{
		fxp::register_params({ 287, "Mega Pulse Cannon", &megapulse287::live, &megapulse287::GAME, sizeof(megapulse287::MegaPulseParams),
		                       megapulse287::table::PARTS, (int)(sizeof(megapulse287::table::PARTS) / sizeof(megapulse287::table::PARTS[0])),
		                       megapulse287::table::FIELDS, (int)(sizeof(megapulse287::table::FIELDS) / sizeof(megapulse287::table::FIELDS[0])),
		                       megapulse287::table::GROUPS, (int)(sizeof(megapulse287::table::GROUPS) / sizeof(megapulse287::table::GROUPS[0])) });
		own::note(megapulse287::ORIG_Setup, (void *)megapulse287::Setup);
		own::note(megapulse287::ORIG_MasterTask, (void *)megapulse287::MasterTask);
		register_port(megapulse287::ORIG_MasterTask, (void *)megapulse287::MasterTask, "287 MasterTask", 287);
		own::note(megapulse287::ORIG_MainTask, (void *)megapulse287::MainTask);
		register_port(megapulse287::ORIG_MainTask, (void *)megapulse287::MainTask, "287 MainTask", 287);
		own::note(megapulse287::ORIG_ArcTask, (void *)megapulse287::ArcTask);
		register_port(megapulse287::ORIG_ArcTask, (void *)megapulse287::ArcTask, "287 ArcTask", 287);
		own::note(megapulse287::ORIG_SpotTask, (void *)megapulse287::SpotTask);
		register_port(megapulse287::ORIG_SpotTask, (void *)megapulse287::SpotTask, "287 SpotTask", 287);
		own::note(megapulse287::ORIG_RockTask, (void *)megapulse287::RockTask);
		register_port(megapulse287::ORIG_RockTask, (void *)megapulse287::RockTask, "287 RockTask", 287);
		own::note(megapulse287::ORIG_SparkTask, (void *)megapulse287::SparkTask);
		register_port(megapulse287::ORIG_SparkTask, (void *)megapulse287::SparkTask, "287 SparkTask", 287);
		// 30 fps layer: see mag287_mega_pulse_cannon_held.inc
		FX_HELD(megapulse287::register_mag287_held();)
	}
	FX_LAB_MODULE(register_mag287_mega_pulse_cannon);
}

#ifdef FF8_FX_HELD
#include "mag287_mega_pulse_cannon_held.inc"
#endif
