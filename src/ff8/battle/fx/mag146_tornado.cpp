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

// Effect 146: Tornado (spell, MAG_146_*).
//
// Structure (setup MAG_146_TORNADO 0x614D70, file loader 0x614D50 = mag145.tim; code 0x614D50..0x6185B0).
// The setup builds the root queue 0x24936E8 (pool of 2 x 0x10) with the root task and the effect queue
// 0x2493750 (pool 0x2493760, 0x64 nodes of 0x24 bytes) with the director of action 0, starts the camera
// animation 0xDD05F8 and uploads the TIM.
//   Root (0x618560) - alternates the packet arena (0x249A624 / 0x24A2624, parity 0x24AA624 also picks the
//     funnel packet copy), runs the effect queue, ends when it is empty.
//   Director (0x614E10), one per action: tick 0 centres the effect on the targets' average position
//     (ground level), captures the stage ground group top-down into VRAM (0x616260), indexes the crater
//     mesh 0xDD5564 in a 9 x 8 cell grid (0x615160) and resets the funnel (0x6152E0: packet templates
//     and 17 rings); tick 1 spawns the funnel, the spark pool, the target lift and plays the wind sound;
//     ticks 2..70 every 12: a spinning cloud prim model (morph); tick 18: 15 debris pieces + the ground
//     crater; tick 78 (last action): the texture restore task; 82 damage; 83 next action's director;
//     ends after 88.
//   Piece (0x6155B0) - one of 15 stone / debris models (tables 0xDD6644..): lies on the ground (lit, stage
//     OT) until the vortex lifts it (ticks 4..55), spinning and orbiting; ends above -5000 or at 60.
//   Ground (0x6164E0) - hides the stage ground group (0x1D989BD bit1) and redraws it through the module:
//     every stage vertex inside the crater area is lowered to the crater mesh height (triangle lookup in
//     the grid), depth-cued by its depth; fades back to the flat stage at ticks 64..68.
//   Funnel (0x6171F0) - 17 rings (0x2491CE8, 0x14 bytes each) that follow each other with random jitter,
//     radius / height / sway from four counter phases (0x617720), drawn as 16 x 16 textured quads
//     (0x617460, static packets 0x2495230, one copy per parity); fades in / out.
//   Sparks (0x617990) - pool of 60 sprites 0x249A230 (0x10 bytes: frame, size, position, fall speed)
//     thrown up around the funnel base (ticks 1..50, 3 / 2 / 1 per tick).
//   Morph cloud (0x617C50) - prim model 0xDD0B2C whose 120 vertices move from 0xDD0B34 towards
//     0xDD181C (deltas 0x24946E0 = difference / 12, frame d: base + delta * d, into 0x2494AA8 + 0x3C0 * e)
//     for 12 ticks, then the target shape; fade = 341 * d.
//   Lift (0x617DF0) - saves the targets' positions / flags (0x2494570, 7 x 0x34), orbits them up into
//     the funnel (entity positions and spin written directly), drops them at tick 72, lands at 80
//     (debris burst + sound), shakes the camera (0x1D97712), restores at 84.
//   Debris burst (0x618280) - 20 sprites per target (life 0x24946E0 + 8 i, position / speed
//     0x2494AA8 + 16 i: the morph buffers are free by then) sliding outwards for 16 ticks.
//   Texture restore (0x618480) - sub_508630 on the TIM, ends when it reports done.
// Every task tests battle_to_update_flags (0x1D96A9C) & 0x201: draw-only ticks draw and do not move
// (the director and the lift do nothing then).
// Module globals: 0x2491BC0..0x24AA62C (crater grid / links, rings, packets, pools, queues, cast
// context 0x24946DC, packet arenas and cursor 0x24AA628); exe data written: the funnel matrix
// translation 0xDD674C..0xDD6757 and the OT link of the capture's static packet 0xDD6780.

#include "mag_common.h"

namespace ff8fx
{
namespace tornado146
{
	using namespace eng;
	using namespace magc;

	// ------------------------------------------------------------------
	// raw memory access (node / record layouts follow the listings offset by offset)
	// ------------------------------------------------------------------
	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }
	inline void Copy8(uint32_t dst, uint32_t src) { U32(dst, 0) = U32(src, 0); U32(dst, 4) = U32(src, 4); }

	// --- module globals ---
	inline uint32_t &TimFile() { return var<uint32_t>(0x2495228); }       // mag145.tim (texture restore)
	inline CastContext *&Ctx() { return var<CastContext *>(0x24946DC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x24AA628); }
	inline uint32_t &Parity() { return var<uint32_t>(0x24AA624); }       // root counter & 1
	static const uint32_t Q_ROOT = 0x24936E8, Q_FX = 0x2493750;
	static const uint32_t ARENA_EVEN = 0x249A624, ARENA_ODD = 0x24A2624;
	// crater mesh grid: 9 x 8 cells of 0x200 (u32 head: 1 = empty), links of 12 bytes (next, triangle, 0)
	static const uint32_t GRID = 0x2491BC0, GRID_LinkCount = 0x2491E4C, GRID_Links = 0x2491E50, GRID_LinksEnd = 0x24936C8;
	// funnel: 17 rings of 0x14 bytes: +0 x, +2 y, +4 z (s16), +8 / +A / +C tilt angles, +0x10 radius (s32)
	static const uint32_t RINGS = 0x2491CE8, RING_SIZE = 0x14;
	static const uint32_t WANDER_Angle = 0x2491CE0, WANDER_Offset = 0x2493708; // 3 x s16 each
	static const uint32_t FUNNEL_Packets = 0x2495230;                          // 512 x 0x28 (POLY_FT4)
	static const uint32_t SPARKS = 0x249A230, SPARKS_End = 0x249A5F0;         // 60 x 0x10
	static const uint32_t MORPH_Delta = 0x24946E0, MORPH_Verts = 0x2494AA8;   // 120 x 8, 2 x 0x3C0
	static const uint32_t DEBRIS_Life = 0x24946E0, DEBRIS_Pos = 0x2494AA8;    // 8 / 16 bytes per sprite
	static const uint32_t LIFT = 0x2494570, LIFT_End = 0x24946DC, LIFT_SIZE = 0x34; // 7 x 0x34
	static const uint32_t TARGET_Count = 0x249A604, TARGET_Slots = 0x249A608;
	// the capture's packets (draw environment, mask bit, draw area, draw offset + its xy)
	static const uint32_t PKT_Env = 0x2493710, PKT_Mask = 0x249A5F8, PKT_Area = 0x2491E40, PKT_Offset = 0x24936F8, OFFSET_XY = 0x249A5F0;

	// exe data
	static const uint32_t MESH_Crater = 0xDD5564;     // +0 offset of the triangles, +8 vertices; triangles: count, 12-byte records
	static const uint32_t PIECE_Model = 0xDD6644, PIECE_Ground = 0xDD6680, PIECE_OffX = 0xDD66BC, PIECE_OffZ = 0xDD66F8; // 15 each
	static const uint32_t FUNNEL_Matrix = 0xDD6738;   // identity; its translation 0xDD674C = the effect position
	static const uint32_t PKT_Black = 0xDD6780;       // POLY_F4 of the capture (alternate viewport)
	static const uint32_t SEQ_Spark = 0xDD0980;
	static const uint32_t MORPH_Model = 0xDD0B2C, MORPH_Base = 0xDD0B34, MORPH_Target = 0xDD181C;
	static const void *const SOUND_Wind = (const void *)0xDD6638;
	static const void *const SOUND_Land = (const void *)0xDD663C;

	// battle / stage
	static const uint32_t STAGE_Ground = 0x1D989C0;    // stage group 1 (skeleton +0, objects +4)
	static const uint32_t STAGE_GroundAnim = 0x1D989D0;
	inline uint8_t &StageGroundFlags() { return var<uint8_t>(0x1D989BD); } // bit0 visible, bit1 drawn by the stage task
	static const uint32_t DRAWENV = 0x1D969C8;         // 2 x 0x5C
	inline uint8_t DisplayBuffer() { return var<uint8_t>(0x1D96A80); }
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }
	inline uint32_t &FrameArena() { return var<uint32_t>(0x1D8E054); }
	inline int16_t &CameraShake() { return var<int16_t>(0x1D97712); }
	static const uint32_t ENTITY0 = 0x1D972C0, ENTITY_SIZE = 0x9C;
	static const uint32_t CAMERA = 0x1D97778;

	// --- task functions ---
	static const uint32_t ORIG_Root = 0x618560;
	static const uint32_t ORIG_Director = 0x614E10;
	static const uint32_t ORIG_Piece = 0x6155B0;
	static const uint32_t ORIG_Ground = 0x6164E0;
	static const uint32_t ORIG_Funnel = 0x6171F0;
	static const uint32_t ORIG_Sparks = 0x617990;
	static const uint32_t ORIG_Morph = 0x617C50;
	static const uint32_t ORIG_Lift = 0x617DF0;
	static const uint32_t ORIG_Debris = 0x618280;
	static const uint32_t ORIG_TexRestore = 0x618480;

	// --- engine functions (original addresses) ---
	inline uint32_t Add(uint32_t q, uint32_t task) { return (uint32_t)AddTaskToQueue((TaskQueue *)q, task); }
	inline int32_t Sin(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D130)(a); }   // computeSin
	inline int32_t Cos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }   // computeCosine
	inline void BuildRot(uint32_t angles, uint32_t m) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x56CD50)(angles, m); }
	inline void ComposeZYX(uint32_t angles, uint32_t m) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x56CE30)(angles, m); }
	inline void Scale3D(uint32_t m, uint32_t v) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x56BEF0)(m, v); }
	inline void Compose(uint32_t a, uint32_t b, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56C2F0)(a, b, out); }
	inline void MatVec(uint32_t m, uint32_t in, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void SetRotW(uint32_t m) { fn<void (__cdecl *)(uint32_t)>(0x56BAE0)(m); }        // GTE_SetRotMatrix_W
	inline void SetTransW(uint32_t m) { fn<void (__cdecl *)(uint32_t)>(0x56BB30)(m); }      // GTE_SetTransVector_W
	inline void SetRot(uint32_t m) { fn<void (__cdecl *)(uint32_t)>(0x45DE00)(m); }         // GTE_SetRotMatrix
	inline void SetTrans(uint32_t m) { fn<void (__cdecl *)(uint32_t)>(0x45DEF0)(m); }       // GTE_SetTransVector
	inline void WriteCtrl(uint32_t value, int32_t reg) { fn<void (__cdecl *)(uint32_t, int32_t)>(0x45D7F0)(value, reg); } // GTE_WriteControlReg
	inline void FarColour(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void LoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void LoadV0(uint32_t a) { fn<void (__cdecl *)(uint32_t)>(0x45DF80)(a); }
	inline void RTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void RTPS() { fn<void (__cdecl *)()>(0x45FAA0)(); }
	inline void NCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void AVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void AVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void ReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void ReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void ReadOTZ(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
	inline void ReadSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); } // GTE_ReadSXY012_Split
	inline void ReadSXY2(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E260)(dst); }
	inline void ReadData(int32_t reg, uint32_t dst) { fn<void (__cdecl *)(int32_t, uint32_t)>(0x45DBA0)(reg, dst); }
	inline void LightV0() { fn<void (__cdecl *)()>(0x4607E0)(); }   // GTE_MVMVA_LightV0_Bk
	inline void LightV1() { fn<void (__cdecl *)()>(0x4607F0)(); }
	inline void LightV2() { fn<void (__cdecl *)()>(0x460800)(); }
	inline void RotV0Tr() { fn<void (__cdecl *)()>(0x460830)(); }   // GTE_MVMVA_RotV0_Tr_2
	inline void SetRGBC(uint32_t src) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(src); }                  // set_unk_1CA8A28
	inline void SetRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E120)(a, b, c); } // sub_45E120
	inline void SetIR0(int32_t v) { fn<void (__cdecl *)(int32_t)>(0x45E150)(v); }                         // set_dword_1CA8A30
	inline void DPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                                            // sub_45F270
	inline void DPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }                                            // sub_45F4C0
	inline void StoreRGB2(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(dst); }                // set_param_with_dword_1CA8A68
	inline void StoreRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E370)(a, b, c); } // sub_45E370
	inline void LoadV0Words(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x64DEE0)(v); }                  // MAG_146_sub_64DEE0: V0 from 3 x s16
	inline void StoreIR(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x64DF10)(dst); }                  // MAG_146_sub_64DF10: IR1..3 -> 3 x s16 + 0
	inline void SetIR0Word(uint32_t src) { fn<void (__cdecl *)(uint32_t)>(0x64DF80)(src); }               // sub_64DF80: IR0 = *(s16 *)src
	inline void Insert(uint32_t slot, uint32_t packet) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C7A0)(slot, packet); }
	inline void InsertAlt(uint32_t slot, uint32_t packet) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C8E0)(slot, packet); } // SSIGPU_InsertPrimAltViewport
	inline void BuildDrawEnvPacket(uint32_t packet, uint32_t env) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C0F0)(packet, env); }
	inline void BuildMaskBitPacket(uint32_t packet, int32_t set) { fn<void (__cdecl *)(uint32_t, int32_t)>(0x45C9F0)(packet, set); }
	inline void BuildDrawAreaPacket(uint32_t packet, uint32_t rect) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C940)(packet, rect); }
	inline void BuildDrawOffsetPacket(uint32_t packet, uint32_t xy) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C9B0)(packet, xy); }
	inline void GetScreenOffset(uint32_t x, uint32_t y) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x56CD10)(x, y); }
	inline void SetScreenOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x56CCE0)(x, y); } // Call_Bs_ParseCamera
	inline void BuildBones(uint32_t anim) { fn<void (__cdecl *)(uint32_t)>(0x508C90)(anim); }             // BattleModel_BuildBoneMatricesFromPose
	inline void ComputeBones(uint32_t anim, uint32_t root) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x5095B0)(anim, root); }
	inline uint32_t RenderGeometry(uint32_t geom, uint32_t hdr, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, int32_t, uint32_t)>(0x5099D0)(geom, hdr, ot, mode, cursor); }
	inline uint32_t PrimModel(uint32_t hdr, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, int32_t, uint32_t)>(0x572200)(hdr, ot, mode, cursor); }
	inline uint32_t SpriteSeq(uint32_t hdr, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, int32_t, uint32_t)>(0x571C80)(hdr, ot, mode, cursor); }
	inline void ShadowRotation(uint32_t pos, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(0x571BC0)(pos, a, b); } // TransformCameraByShadowRotation
	inline void SpawnPosition(uint32_t entity, int32_t bone, int32_t frac, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, uint32_t)>(0x502170)(entity, bone, frac, out); } // GetEffectSpawnPosition
	inline void TexRestore(uint32_t tex, uint32_t done) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x508630)(tex, done); }

	inline uint32_t Alloc(int size) { return (uint32_t)FieldAlloc(size); }
	inline uint32_t OT44() { return OTBase() + 0x44; }
	inline uint32_t Entity(int slot) { return ENTITY0 + ENTITY_SIZE * slot; }

	// x / 3, x / 17 with the compiler's magic numbers (truncating)
	inline int32_t Div3(int32_t x) { int32_t h = (int32_t)(((int64_t)x * 0x55555556) >> 32); return h + (int32_t)((uint32_t)h >> 31); }
	// x / 512 and x / 16 as cdq; and; add; sar (truncating)
	inline int32_t Div512(int32_t x) { return (x + ((x >> 31) & 0x1FF)) >> 9; }
	inline int32_t Div16(int32_t x) { return (x + ((x >> 31) & 0xF)) >> 4; }

	// screen-space rejection of a projected triangle / quad (x in 0..0xA00, y in 0..0x6C0)
	inline uint32_t OffX(int16_t x, uint32_t bit) { return (x < 0 || x > 0xA00) ? bit : 0; }
	inline uint32_t OffY(int16_t y, uint32_t bit) { return (y < 0 || y > 0x6C0) ? bit : 0; }

	// the capture's packets (static in the module)
	struct CapturePackets { uint32_t env, black, mask, area, offset, offset_xy; };

	// the funnel parameters of a counter value (0x6171F0, before 0x617720)
	struct FunnelParams { int32_t p[6]; };
}
}

#ifdef FF8_FX_HELD
#include "mag146_tornado_held.h"
#endif

namespace ff8fx
{
namespace tornado146
{
	// ------------------------------------------------------------------
	// Root task (0x618560): node +0x0C counter
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		uint32_t p = (uint32_t)n;
		// 30 fps layer: see mag146_tornado_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = ARENA_ODD;
		Parity() = U8(p, 0xC) & 1;
		if (Parity() == 0) PacketCursor() = ARENA_EVEN;
		int alive = ExecuteTaskQueue((TaskQueue *)Q_FX);
		U16(p, 0xC) = (uint16_t)(U16(p, 0xC) + 1);
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// 0x6184C0: average effect anchor of the targets (16-bit sums / count); +6 = the last target's
	// height word
	// ------------------------------------------------------------------
	static void AverageTargetPos(uint32_t targets, int32_t count, uint32_t out)
	{
		int16_t sx = 0, sy = 0, sz = 0;
		int16_t pos[4] = {};
		for (int32_t k = 0; k < count; k++)
		{
			GetDefaultEffectPosition((uint8_t *)Entity(U8(targets, k * TARGET_STRIDE)), pos);
			sx = (int16_t)(sx + pos[0]);
			sy = (int16_t)(sy + pos[1]);
			sz = (int16_t)(sz + pos[2]);
		}
		pos[0] = (int16_t)(sx / count);
		pos[1] = (int16_t)(sy / count);
		pos[2] = (int16_t)(sz / count);
		memcpy((void *)out, pos, 8);
	}

	// ------------------------------------------------------------------
	// 0x615160 / 0x6151C0: crater mesh grid. Every triangle goes to the cell of its centroid
	// (x + 0x8D8, z + 0x80E) / 0x200 at the head of the cell's list.
	// ------------------------------------------------------------------
	static void GridInsert(uint32_t h, uint32_t verts)
	{
		uint32_t tri = U32(h, 0x38);
		int32_t count = S32(tri, 0);
		tri += 4;
		U32(h, 0x38) = tri;
		if (count <= 0) return;
		int32_t first = S32(GRID_LinkCount, 0);
		S32(GRID_LinkCount, 0) = first + count;
		uint32_t link = GRID_Links + 12 * first;
		for (; count > 0; count--)
		{
			Copy8(h + 0x68, verts + U16(tri, 4) * 4);
			Copy8(h + 0x70, verts + U16(tri, 6) * 4);
			Copy8(h + 0x78, verts + U16(tri, 8) * 4);
			int32_t cx = Div512(Div3(S16(h, 0x78) + S16(h, 0x68) + S16(h, 0x70)) + 0x8D8);
			int32_t cz = Div512(Div3(S16(h, 0x7C) + S16(h, 0x74) + S16(h, 0x6C)) + 0x80E);
			uint32_t cell = GRID + (uint32_t)(cz * 9 + cx) * 4;
			uint32_t head = U32(cell, 0);
			if (head != 1) U32(link, 0) = head;
			U32(cell, 0) = link;
			U32(link, 4) = tri;
			U32(link, 8) = 0;
			link += 12;
			tri += 12;
		}
		U32(h, 0x38) = tri;
	}

	static void GridBuild(uint32_t h)
	{
		for (int k = 0; k < 0x48; k++) U32(GRID, 4 * k) = 1;
		U32(GRID_LinkCount, 0) = 0;
		for (uint32_t l = GRID_Links; l < GRID_LinksEnd; l += 12) U32(l, 0) = 1;
		uint32_t mesh = U32(h, 0x30);
		uint32_t tri = mesh + U32(mesh, 0);
		U32(h, 0x34) = mesh + 8;
		U32(h, 0x38) = tri;
		GridInsert(h, mesh + 8);
	}

	// ------------------------------------------------------------------
	// 0x6152E0: funnel packet templates (8 rows of each texture page x 16 quads x 2 parity copies,
	// v from the row, u mirrored every other pair) and the 17 rings (x 0, y 5 k, z 0, radius 1500)
	// ------------------------------------------------------------------
	static void FunnelTemplateRow(uint32_t q, uint8_t ua, uint8_t ub, uint8_t va, uint8_t vb, uint16_t clut, uint16_t tpage)
	{
		for (int c = 0; c < 2; c++, q += 0x28)
		{
			U32(q, 0) = 0x9000000;
			U32(q, 4) = 0x2E808080;
			U8(q, 0x1C) = ub;
			U8(q, 0x0C) = ub;
			U8(q, 0x24) = ua;
			U8(q, 0x14) = ua;
			U8(q, 0x15) = vb;
			U8(q, 0x0D) = vb;
			U8(q, 0x25) = va;
			U8(q, 0x1D) = va;
			U16(q, 0x0E) = clut;
			U16(q, 0x16) = tpage;
		}
	}

	static void BuildFunnelMesh()
	{
		for (int set = 0; set < 2; set++)
		{
			uint32_t base = FUNNEL_Packets + 0x2800 * set;
			uint16_t clut = set ? 0x3FD4 : 0x3F94, tpage = set ? 0xB6 : 0xB7;
			for (int k = 0; k < 8; k++)
			{
				uint8_t v0 = (uint8_t)(k * 0x1F);
				uint8_t v1 = (uint8_t)(v0 + 0x1F);
				if (k == 7) v1 = 0xFF;
				uint8_t va = (uint8_t)(0xFF - v0), vb = (uint8_t)(0xFF - v1);
				uint8_t ua = 0, ub = 0x3F;
				for (int j = 0; j < 16; j += 2)
				{
					ub ^= 0x3F;
					ua ^= 0x3F;
					uint32_t q = base + (k * 16 + j) * 0x50;
					FunnelTemplateRow(q, ua, ub, va, vb, clut, tpage);
					FunnelTemplateRow(q + 0x50, (uint8_t)(ua + 0x40), (uint8_t)(ub + 0x40), va, vb, clut, tpage);
				}
			}
		}
		int32_t acc = 0;
		for (uint32_t r = RINGS; r < RINGS + 17 * RING_SIZE; r += RING_SIZE, acc += 0x50)
		{
			S16(r, 0) = 0;
			S16(r, 2) = (int16_t)Div16(acc);
			S16(r, 4) = 0;
			S32(r, 0x10) = 0x5DC;
		}
		S16(WANDER_Offset, 4) = 0;
		S16(WANDER_Offset, 2) = 0;
		S16(WANDER_Offset, 0) = 0;
		S16(WANDER_Angle, 4) = 0;
		S16(WANDER_Angle, 2) = 0;
		S16(WANDER_Angle, 0) = 0;
	}

	// ------------------------------------------------------------------
	// 0x616260: top-down capture of the stage ground group into VRAM (0x280, 0x100, 256 x 256):
	// draw environment of the other buffer, flat matrix (y scale 0) centred on the effect position,
	// screen offset at the centre
	// ------------------------------------------------------------------
	static void Capture(uint32_t pos, const CapturePackets &pk)
	{
		int16_t rect[4] = { 0x280, 0x100, 0x100, 0x100 };
		int16_t angles[4] = { 0x400, 0, 0, 0 };
		Mat4x3 m;
		int32_t x = S16(pos, 0);
		int32_t q = (int32_t)(((int64_t)x * 0x78787879) >> 32) >> 3;
		m.t[0] = 7 - (q + (int32_t)((uint32_t)q >> 31));   // 7 - x / 17
		m.t[1] = Div16(S16(pos, 4)) - 1;                    // z / 16 - 1
		m.t[2] = var<int16_t>(0x1D8E038);
		BuildRot((uint32_t)angles, (uint32_t)&m);
		int32_t scale[3];
		scale[1] = 0;
		scale[0] = 0x1000000 / (0x10C0000 / rect[2]);
		scale[2] = 0x1000000 / (0xFED000 / rect[3]);
		Scale3D((uint32_t)&m, (uint32_t)scale);
		BuildDrawEnvPacket(pk.env, DRAWENV + 0x5C * (uint32_t)(DisplayBuffer() ^ 1));
		Insert(OT44(), pk.env);
		int32_t ofx, ofy;
		GetScreenOffset((uint32_t)&ofx, (uint32_t)&ofy);
		SetScreenOffset(rect[2] / 2, rect[3] / 2);
		SetRotW((uint32_t)&m);
		SetTransW((uint32_t)&m);
		uint32_t h = Alloc(0x2C);
		U32(h, 0x1C) = var<uint32_t>(0x1D989E4);
		U32(h, 0x10) = 0xFFFFFFFF;
		U32(h, 0x20) = 0xFFFFFFFF;
		S16(h, 0x14) = 0;
		S16(h, 0x16) = 0;
		S16(h, 0x18) = 0x140;
		S16(h, 0x1A) = 0xD8;
		U16(h, 0x24) = var<uint16_t>(0x1D989BE);
		U32(h, 4) = var<uint32_t>(0x1D98B3C);
		if (StageGroundFlags() & 1)
		{
			BuildBones(STAGE_GroundAnim);
			ComputeBones(STAGE_GroundAnim, (uint32_t)&m);
			PacketCursor() = RenderGeometry(STAGE_Ground, h, OT44(), 0x10, PacketCursor());
		}
		FieldFree(0x2C);
		SetScreenOffset(ofx, ofy);
		InsertAlt(OT44(), pk.black);
		BuildMaskBitPacket(pk.mask, 0);
		Insert(OT44(), pk.mask);
		BuildDrawAreaPacket(pk.area, (uint32_t)rect);
		Insert(OT44(), pk.area);
		S16(pk.offset_xy, 0) = rect[0];
		S16(pk.offset_xy, 2) = rect[1];
		BuildDrawOffsetPacket(pk.offset, pk.offset_xy);
		Insert(OT44(), pk.offset);
	}

	static const CapturePackets CAPTURE_PACKETS = { PKT_Env, PKT_Black, PKT_Mask, PKT_Area, PKT_Offset, OFFSET_XY };

	// ------------------------------------------------------------------
	// 0x617BD0: morph deltas (target - base) / 12 of the 120 cloud vertices
	// ------------------------------------------------------------------
	static void MorphDeltas(uint32_t base, uint32_t target)
	{
		for (int i = 0; i < 120; i++)
			for (int c = 0; c < 3; c++)
				S16(MORPH_Delta + 8 * i, 2 * c) = (int16_t)((S16(target + 8 * i, 2 * c) - S16(base + 8 * i, 2 * c)) / 12);
	}

	// ------------------------------------------------------------------
	// Director (0x614E10): node +0x0C counter, +0x0E action, +0x10 effect position (x, y, z, h),
	// +0x22 morph buffer toggle
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		uint32_t d = (uint32_t)n;
		if (DrawOnly()) return 0;
		if (S16(d, 0xC) == 0)
		{
			ActionData *ad = &Ctx()->actions[S16(d, 0xE)];
			AverageTargetPos((uint32_t)ad->targets, ad->target_count, d + 0x10);
			S16(d, 0x12) = 0;
			S16(d, 0x22) = 0;
			// 30 fps layer: see mag146_tornado_held.inc
			FX_HELD(held_note_capture(d);)
			Capture(d + 0x10, CAPTURE_PACKETS);
			uint32_t h = Alloc(0xC8);
			U32(h, 0x30) = MESH_Crater;
			GridBuild(h);
			FieldFree(0xC8);
			BuildFunnelMesh();
		}
		if (S16(d, 0xC) == 0x12)
		{
			for (int k = 0; k < 15; k++)
			{
				uint32_t p = Add(Q_FX, ORIG_Piece);
				U32(p, 0x10) = U32(d, 0x10);
				S16(p, 0xC) = 0;
				S16(p, 0xE) = (int16_t)k;
				U32(p, 0x14) = U32(d, 0x14);
				int32_t r = CrtRand();
				S16(p, 0x18) = 0;
				S16(p, 0x16) = (int16_t)(-50 - r % 140 - k);
				r = CrtRand();
				S16(p, 0x1C) = 0;
				S16(p, 0x1A) = (int16_t)(r % 10 + 5);
				r = CrtRand();
				S16(p, 0x20) = 0;
				S16(p, 0x1E) = (int16_t)(r % 40 + 20);
				r = CrtRand();
				int32_t spin = r % 60 + 30;
				S16(p, 0x22) = (int16_t)spin;
				if (U8(p, 0x1A) & 1) S16(p, 0x22) = (int16_t)-spin;
			}
			uint32_t g = Add(Q_FX, ORIG_Ground);
			U32(g, 0x10) = U32(d, 0x10);
			S16(g, 0xC) = 0;
			U32(g, 0x14) = U32(d, 0x14);
		}
		if (S16(d, 0xC) == 1)
		{
			uint32_t f = Add(Q_FX, ORIG_Funnel);
			U32(f, 0x10) = U32(d, 0x10);
			S16(f, 0xC) = 0;
			U32(f, 0x14) = U32(d, 0x14);
			MorphDeltas(MORPH_Base, MORPH_Target);
			if (S16(d, 0xC) == 1)
			{
				uint32_t s = Add(Q_FX, ORIG_Sparks);
				U32(s, 0x10) = U32(d, 0x10);
				S16(s, 0xC) = 0;
				U32(s, 0x14) = U32(d, 0x14);
				for (uint32_t e = SPARKS; e < SPARKS_End; e += 0x10) S32(e, 0) = -1;
			}
		}
		int16_t c = S16(d, 0xC);
		if (c >= 2 && c <= 0x46 && c % 12 == 2)
		{
			U16(d, 0x22) ^= 1;
			uint32_t m = Add(Q_FX, ORIG_Morph);
			U16(m, 0xE) = U16(d, 0x22);
			U32(m, 0x10) = U32(d, 0x10);
			S16(m, 0xC) = 0;
			U32(m, 0x14) = U32(d, 0x14);
			S16(m, 0x1C) = 0x3000;
		}
		if (S16(d, 0xC) == 1)
		{
			uint32_t l = Add(Q_FX, ORIG_Lift);
			U16(l, 0xE) = U16(d, 0xE);
			U32(l, 0x10) = U32(d, 0x10);
			S16(l, 0xC) = 0;
			U32(l, 0x14) = U32(d, 0x14);
			S16(l, 0x18) = 0;
		}
		if (U16(d, 0xE) == (uint16_t)Ctx()->actions[0].last_action && S16(d, 0xC) == 0x4E)
		{
			uint32_t t = Add(Q_ROOT, ORIG_TexRestore);
			S16(t, 0xC) = 0;
		}
		if (S16(d, 0xC) == 0x52)
		{
			ActionData *ad = &Ctx()->actions[S16(d, 0xE)];
			ApplyActionResultToTargets(ad->targets, ad->target_count);
		}
		if (S16(d, 0xC) == 0x53)
		{
			int32_t next = S16(d, 0xE) + 1;
			if (next <= Ctx()->actions[0].last_action)
			{
				uint32_t t = Add(Q_FX, ORIG_Director);
				S16(t, 0xC) = 0;
				S16(t, 0xE) = (int16_t)next;
			}
		}
		if (S16(d, 0xC) == 1)
		{
			ActionData *ad = &Ctx()->actions[S16(d, 0xE)];
			int16_t pos[4];
			AverageTargetPos((uint32_t)ad->targets, ad->target_count, (uint32_t)pos);
			BdPlaySE3D(SOUND_Wind, 0, pos);
		}
		S16(d, 0xC) = (int16_t)(S16(d, 0xC) + 1);
		return S16(d, 0xC) > 0x58 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Piece models. Header (0x138 bytes, Field_Alloc): +0 model, +4..6 far colour (never set: scratch),
	// +8 depth cue level, +0x18 flags, +0x1C vertices, +0x20 primitive cursor, +0x38 / +0x40 second
	// OT and shift, +0x48 MAC0, +0x50 OTZ, +0x54 FLAG, +0x58 / +0x6C / +0x80 vertex copies, +0xA0 /
	// +0xB4 / +0xC8 light results, +0xD0 / +0xD4 model offset x / z, +0xE4 spin angles, +0xE6 lift,
	// +0xF8 piece matrix, +0x118 polygon matrix.
	// ------------------------------------------------------------------

	// 0x6158C0: lit flat-textured triangles (0x14-byte records -> 0x20-byte POLY_FT3), lifted by
	// +0xE6; lights 0 / 1 / 2 on the background colour: a non-positive result goes to the second OT
	static uint32_t PieceLitTris(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint32_t verts = U32(h, 0x1C);
		uint32_t rec = U32(h, 0x20);
		int32_t count = S32(rec, 0);
		rec += 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x14)
		{
			Copy8(h + 0x58, verts + U16(rec, 4) * 4);
			S16(h, 0x5A) = (int16_t)(S16(h, 0x5A) + S16(h, 0xE6));
			Copy8(h + 0x6C, verts + U16(rec, 6) * 4);
			S16(h, 0x6E) = (int16_t)(S16(h, 0x6E) + S16(h, 0xE6));
			Copy8(h + 0x80, verts + U16(rec, 8) * 4);
			S16(h, 0x82) = (int16_t)(S16(h, 0x82) + S16(h, 0xE6));
			LoadV012(h + 0x58, h + 0x6C, h + 0x80);
			RTPT();
			U32(pk, 4) = U32(rec, 0);
			U32(pk, 0xC) = U32(rec, 0xC);
			U32(pk, 0x14) = U32(rec, 0x10);
			U32(pk, 0) = 0x7000000;
			U32(pk, 0x1C) = U32(rec, 8) >> 16;
			ReadFLAG(h + 0x54);
			if (U32(h, 0x54) & 0x60000) continue;
			NCLIP();
			ReadMAC0(h + 0x48);
			ReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			AVSZ3();
			ReadOTZ(h + 0x50);
			LightV0();
			ReadData(9, h + 0xA0);
			LightV1();
			ReadData(9, h + 0xB4);
			LightV2();
			ReadData(9, h + 0xC8);
			int32_t lit = S32(h, 0xC8) | S32(h, 0xB4) | S32(h, 0xA0);
			uint32_t z = U32(h, 0x50);
			if (lit <= 0) Insert(U32(h, 0x38) + ((int32_t)z >> S32(h, 0x40)) * 4, pk);
			else Insert(ot + ((int32_t)z >> shift) * 4, pk);
			pk += 0x20;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x615A60: the same for gouraud-textured triangles (0x1C-byte records -> 0x28-byte POLY_GT3)
	static uint32_t PieceLitGTris(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint32_t verts = U32(h, 0x1C);
		uint32_t rec = U32(h, 0x20);
		int32_t count = S32(rec, 0);
		rec += 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x1C)
		{
			Copy8(h + 0x58, verts + U16(rec, 4) * 4);
			S16(h, 0x5A) = (int16_t)(S16(h, 0x5A) + S16(h, 0xE6));
			Copy8(h + 0x6C, verts + U16(rec, 6) * 4);
			S16(h, 0x6E) = (int16_t)(S16(h, 0x6E) + S16(h, 0xE6));
			Copy8(h + 0x80, verts + U16(rec, 8) * 4);
			S16(h, 0x82) = (int16_t)(S16(h, 0x82) + S16(h, 0xE6));
			LoadV012(h + 0x58, h + 0x6C, h + 0x80);
			RTPT();
			U32(pk, 0x10) = U32(rec, 0x14);
			U32(pk, 0x1C) = U32(rec, 0x18);
			U32(pk, 0x18) = U32(rec, 0x10);
			U32(pk, 4) = U32(rec, 0);
			U32(pk, 0) = 0x9000000;
			U32(pk, 0xC) = U32(rec, 0xC);
			U32(pk, 0x24) = U32(rec, 8) >> 16;
			ReadFLAG(h + 0x54);
			if (U32(h, 0x54) & 0x60000) continue;
			NCLIP();
			ReadMAC0(h + 0x48);
			ReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			AVSZ3();
			ReadOTZ(h + 0x50);
			LightV0();
			ReadData(9, h + 0xA0);
			LightV1();
			ReadData(9, h + 0xB4);
			LightV2();
			ReadData(9, h + 0xC8);
			int32_t lit = S32(h, 0xA0) | S32(h, 0xC8) | S32(h, 0xB4);
			uint32_t z = U32(h, 0x50);
			if (lit <= 0) Insert(U32(h, 0x38) + ((int32_t)z >> S32(h, 0x40)) * 4, pk);
			else Insert(ot + ((int32_t)z >> shift) * 4, pk);
			pk += 0x28;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x6157F0: a piece lying on the ground: fixed lighting colour registers, both primitive lists
	static uint32_t PieceDrawLit(uint32_t h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t model = U32(h, 0);
		S16(h, 0xD0) = 0;
		U16(h, 0xD2) = 0xF000;
		S16(h, 0xD4) = 0;
		U32(h, 0x1C) = model + 8;
		S16(h, 0xD6) = 0x96;
		uint32_t c89 = U32(h, 0xD4);
		U32(h, 0x20) = model + U32(model, 0);
		U32(h, 0x38) = OTBase() + 0x4068;
		U32(h, 0x40) = 6;
		WriteCtrl(U32(h, 0xD0), 8);
		WriteCtrl(c89, 9);
		WriteCtrl((uint32_t)((int32_t)c89 >> 16), 0xD);
		U32(h, 0x20) += 8;
		uint32_t pk = cursor;
		if (U32(U32(h, 0x20), 0) != 0) pk = PieceLitTris(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		U32(h, 0x20) += 0xC;
		if (U32(U32(h, 0x20), 0) != 0) pk = PieceLitGTris(h, ot, shift, pk);
		return pk;
	}

	// copies the 3 vertices of a record into the header, minus the model offset (x, z)
	static void PieceVerts(uint32_t h, uint32_t verts, uint32_t rec)
	{
		int16_t ox = S16(h, 0xD0), oz = S16(h, 0xD4);
		Copy8(h + 0x58, verts + U16(rec, 4) * 4);
		S16(h, 0x58) = (int16_t)(S16(h, 0x58) - ox);
		S16(h, 0x5C) = (int16_t)(S16(h, 0x5C) - oz);
		Copy8(h + 0x6C, verts + U16(rec, 6) * 4);
		S16(h, 0x6C) = (int16_t)(S16(h, 0x6C) - ox);
		S16(h, 0x70) = (int16_t)(S16(h, 0x70) - oz);
		Copy8(h + 0x80, verts + U16(rec, 8) * 4);
		S16(h, 0x80) = (int16_t)(S16(h, 0x80) - ox);
		S32(h, 0x12C) = ox;
		S32(h, 0x130) = S16(h, 0xD2);
		S16(h, 0x84) = (int16_t)(S16(h, 0x84) - oz);
		S32(h, 0x134) = oz;
		// polygon matrix: spin angles, translated back by the offset, on the piece matrix
		BuildRot(h + 0xE4, h + 0x118);
		Compose(h + 0xF8, h + 0x118, h + 0x118);
		SetRot(h + 0x118);
		SetTrans(h + 0x118);
		LoadV012(h + 0x58, h + 0x6C, h + 0x80);
	}

	// clip flags of a projected triangle whose screen words are at xy0, xy1, xy2
	static uint32_t TriClip(uint32_t xy0, uint32_t xy1, uint32_t xy2)
	{
		uint32_t f = OffX(S16(xy0, 0), 1);
		f |= OffX(S16(xy1, 0), 2);
		f |= OffX(S16(xy2, 0), 4);
		f |= OffY(S16(xy0, 2), 0x10);
		f |= OffY(S16(xy1, 2), 0x20);
		f |= OffY(S16(xy2, 2), 0x40);
		return f;
	}

	// 0x615CA0: spinning flat-textured triangles (0x14-byte records -> POLY_FT3)
	static uint32_t PieceSpinTris(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint32_t verts = U32(h, 0x1C);
		uint32_t rec = U32(h, 0x20);
		int32_t count = S32(rec, 0);
		rec += 4;
		U32(h, 0x20) = rec;
		if (count <= 0) return pk;
		for (; count > 0; count--, rec += 0x14)
		{
			PieceVerts(h, verts, rec);
			RTPT();
			U32(pk, 0) = 0x7000000;
			uint32_t code = U32(rec, 0);
			U32(pk, 4) = code;
			if (U8(h, 0x18) & 1) U32(pk, 4) = code | 0x2000000;
			U32(pk, 0xC) = U32(rec, 0xC);
			U32(pk, 0x14) = U32(rec, 0x10);
			U32(pk, 0x1C) = U32(rec, 8) >> 16;
			ReadFLAG(h + 0x54);
			if (U32(h, 0x54) & 0x60000) continue;
			NCLIP();
			ReadMAC0(h + 0x48);
			ReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			AVSZ3();
			uint32_t f = TriClip(pk + 8, pk + 0x10, pk + 0x18);
			if ((f & 7) == 7 || (f & 0x70) == 0x70) continue;
			ReadOTZ(h + 0x50);
			if (U8(h, 0x18) & 0x40)
			{
				SetRGBC(pk + 4);
				SetIR0(S32(h, 8));
				DPCS();
				StoreRGB2(pk + 4);
			}
			Insert(ot + ((int32_t)U32(h, 0x50) >> shift) * 4, pk);
			pk += 0x20;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x615F70: spinning gouraud-textured triangles (0x1C-byte records -> POLY_GT3)
	static uint32_t PieceSpinGTris(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint32_t verts = U32(h, 0x1C);
		uint32_t rec = U32(h, 0x20);
		int32_t count = S32(rec, 0);
		rec += 4;
		U32(h, 0x20) = rec;
		if (count <= 0) return pk;
		for (; count > 0; count--, rec += 0x1C)
		{
			PieceVerts(h, verts, rec);
			RTPT();
			U32(pk, 0) = 0x9000000;
			uint32_t code = U32(rec, 0);
			U32(pk, 4) = code;
			if (U8(h, 0x18) & 2) U32(pk, 4) = code | 0x2000000;
			U32(pk, 0xC) = U32(rec, 0xC);
			U32(pk, 0x18) = U32(rec, 0x10);
			U32(pk, 0x24) = U32(rec, 8) >> 16;
			ReadFLAG(h + 0x54);
			if (U32(h, 0x54) & 0x60000) continue;
			NCLIP();
			ReadMAC0(h + 0x48);
			ReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			AVSZ3();
			uint32_t f = TriClip(pk + 8, pk + 0x14, pk + 0x20);
			if ((f & 7) == 7 || (f & 0x70) == 0x70) continue;
			ReadOTZ(h + 0x50);
			if (U8(h, 0x18) & 0x80)
			{
				SetRGB012(rec + 0x14, rec + 0x18, pk + 4);
				SetIR0(S32(h, 8));
				DPCT();
				StoreRGB012(pk + 0x10, pk + 0x1C, pk + 4);
			}
			else
			{
				U32(pk, 0x10) = U32(rec, 0x14);
				U32(pk, 0x1C) = U32(rec, 0x18);
			}
			Insert(ot + ((int32_t)U32(h, 0x50) >> shift) * 4, pk);
			pk += 0x28;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x615C10: a piece in the air: far colour from the header, both primitive lists
	static uint32_t PieceDrawSpin(uint32_t h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t model = U32(h, 0);
		U32(h, 0x1C) = model + 8;
		U32(h, 0x20) = model + U32(model, 0);
		FarColour(U8(h, 4), U8(h, 5), U8(h, 6));
		U32(h, 0x20) += 8;
		uint32_t pk = cursor;
		if (U32(U32(h, 0x20), 0) != 0) pk = PieceSpinTris(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		U32(h, 0x20) += 0xC;
		if (U32(U32(h, 0x20), 0) != 0) return PieceSpinGTris(h, ot, shift, pk);
		U32(h, 0x20) += 4;
		return pk;
	}

	// ------------------------------------------------------------------
	// Piece (0x6155B0): node +0x0C counter, +0x0E piece type (0..14), +0x10 x, +0x12 y, +0x14 z,
	// +0x16 rise speed, +0x18 yaw, +0x1A (random 5..14: odd = spin backwards), +0x1C / +0x20 spin
	// angles, +0x1E / +0x22 their speeds
	// ------------------------------------------------------------------
	static void PieceDraw(uint32_t p)
	{
		int16_t angles[4] = { 0, S16(p, 0x18), 0, 0 };
		Mat4x3 m;
		BuildRot((uint32_t)angles, (uint32_t)&m);
		m.t[0] = S16(p, 0x10);
		int32_t unit[3] = { 0x1000, 0x1000, 0x1000 };
		m.t[1] = S16(p, 0x12);
		m.t[2] = S16(p, 0x14);
		Scale3D((uint32_t)&m, (uint32_t)unit);
		uint32_t h = Alloc(0x138);
		int32_t type = S16(p, 0xE);
		if (S16(p, 0x12) < S32(PIECE_Ground, 4 * type))
		{
			U32(h, 0) = U32(PIECE_Model, 4 * type);
			U16(h, 0xD4) = U16(PIECE_OffZ, 4 * type);
			U16(h, 0xE4) = U16(p, 0x1C);
			U16(h, 0xD0) = U16(PIECE_OffX, 4 * type);
			U32(h, 0x18) = 0;
			S16(h, 0xD2) = 0;
			S16(h, 0xE6) = 0;
			U16(h, 0xE8) = U16(p, 0x20);
			Compose(CAMERA, (uint32_t)&m, h + 0xF8);
			PacketCursor() = PieceDrawSpin(h, OT44(), 2, PacketCursor());
		}
		else
		{
			m.t[1] = 0;
			Compose(CAMERA, (uint32_t)&m, (uint32_t)&m);
			SetRotW((uint32_t)&m);
			SetTransW((uint32_t)&m);
			U32(h, 0x18) = 0;
			U32(h, 0) = U32(PIECE_Model, 4 * type);
			U16(h, 0xE6) = U16(p, 0x12);
			PacketCursor() = PieceDrawLit(h, OT44(), 2, PacketCursor());
		}
		FieldFree(0x138);
	}

	// spin while in the air; ticks 4, 5 rise at the start speed, 6..55 rise faster by 1/16 per tick
	// (halved at 6) and turn; returns TASK_END above -5000 (counter not advanced)
	static uint32_t PieceUpdate(uint32_t p)
	{
		int32_t type = S16(p, 0xE);
		int16_t y = S16(p, 0x12);
		if (y < S32(PIECE_Ground, 4 * type))
		{
			S16(p, 0x1C) = (int16_t)(S16(p, 0x1C) + S16(p, 0x1E));
			S16(p, 0x20) = (int16_t)(S16(p, 0x20) + S16(p, 0x22));
		}
		int16_t c = S16(p, 0xC);
		int32_t t = c;
		if (t >= 4)
		{
			t -= 4;
			if (t < 2) S16(p, 0x12) = (int16_t)(S16(p, 0x16) + y);
			else
			{
				t -= 2;
				if (t < 0x32)
				{
					if (t == 0) S16(p, 0x16) = (int16_t)(S16(p, 0x16) >> 1);
					int16_t v = S16(p, 0x16);
					S16(p, 0x12) = (int16_t)(y + v);
					int16_t nv = (int16_t)(Div16(v) + v);
					S16(p, 0x16) = nv;
					if (S16(p, 0x12) < -5000) return TASK_END;
					S16(p, 0x18) = (int16_t)(S16(p, 0x18) - (int16_t)(nv >> 2));
				}
			}
		}
		S16(p, 0xC) = (int16_t)(c + 1);
		return S16(p, 0xC) >= 0x3C ? TASK_END : 0;
	}

	static uint32_t __cdecl PieceTask(TaskNode *n)
	{
		uint32_t p = (uint32_t)n;
		// 30 fps layer: see mag146_tornado_held.inc
		FX_HELD(held_note_draw(ORIG_Piece, p);)
		PieceDraw(p);
		if (DrawOnly()) return 0;
		return PieceUpdate(p);
	}

	// ------------------------------------------------------------------
	// Ground crater. Header (0xC8 bytes): +0 polygon cursor, +4 vertex workspace (0x1D98B3C),
	// +8..E primitive counts, +0x2C second OT, +0x30 crater mesh, +0x3C depth factor (0x1000 = full
	// crater), +0x44 average depth cue, +0x48 OTZ, +0x4C FLAG, +0x50..5C crater area (x / z bounds),
	// +0x60 lowered, +0x68.. vertex copies, +0x94 / +0xA4 / +0xB4 edge tests, +0xB8 / +0xBC centre
	// ------------------------------------------------------------------

	// 0x6167E0: a stage vertex (workspace, camera-free coordinates) inside the crater area: the crater
	// triangle under it (grid cells around it) gives its height * factor; +6 = depth cue (y * 4096 / 1465)
	static void GroundLower(uint32_t h, uint32_t v)
	{
		int32_t dx = S16(v, 0) - S16(h, 0xB8);
		int32_t dz = S16(v, 4) - S16(h, 0xBC);
		uint32_t verts = U32(h, 0x30) + 8;
		int32_t col = Div512(dx + 0x8D8);
		int32_t row = Div512(dz + 0x80E);
		int32_t c0 = col - 1, c1 = col + 2, r0 = row - 1, r1 = row + 2;
		if (c0 < 0) c0 = 0; else if (c0 > 8) c0 = 8;
		if (c1 < 0) c1 = 0; else if (c1 > 8) c1 = 8;
		if (r0 < 0) r0 = 0; else if (r0 > 7) r0 = 7;
		if (r1 < 0) r1 = 0; else if (r1 > 7) r1 = 7;
		S32(h, 0x60) = 0;
		for (int32_t r = r0; r < r1; r++)
		{
			uint32_t cell = GRID + (uint32_t)(r * 9 + c0) * 4;
			for (int32_t c = c0; c < c1; c++, cell += 4)
			{
				uint32_t link = U32(cell, 0);
				if (link == 1) continue;
				for (;;)
				{
					uint32_t tri = U32(link, 4);
					Copy8(h + 0x78, verts + U16(tri, 4) * 4);
					Copy8(h + 0x70, verts + U16(tri, 6) * 4);
					Copy8(h + 0x68, verts + U16(tri, 8) * 4);
					int32_t x0 = S16(h, 0x78), z0 = S16(h, 0x7C);
					int32_t x1 = S16(h, 0x70), z1 = S16(h, 0x74);
					int32_t x2 = S16(h, 0x68), z2 = S16(h, 0x6C);
					int32_t e0 = mul32(dz - z2, x1 - x2) - mul32(dx - x2, z1 - z2);
					S32(h, 0x94) = e0;
					int32_t e1 = mul32(dz - z1, x0 - x1) - mul32(dx - x1, z0 - z1);
					S32(h, 0xA4) = e1;
					int32_t e2 = mul32(dz - z0, x2 - x0) - mul32(dx - x0, z2 - z0);
					S32(h, 0xB4) = e2;
					if (e0 == 0) S32(h, 0x94) = -1;
					if (e1 == 0) S32(h, 0xA4) = -1;
					if (e2 == 0) S32(h, 0xB4) = -1;
					if ((S32(h, 0x94) & S32(h, 0xB4) & S32(h, 0xA4)) < 0)
					{
						int32_t y = Div3(S16(h, 0x7A) + S16(h, 0x72) + S16(h, 0x6A));
						S32(h, 0x60) = 1;
						y = mul32(y, S32(h, 0x3C)) >> 12;
						S16(v, 2) = (int16_t)y;
						int32_t q = (int32_t)(((int64_t)shl32((int16_t)y, 12) * 0x59780C95) >> 32) >> 9;
						q += (int32_t)((uint32_t)q >> 31);
						S16(v, 6) = (int16_t)q;
						return;
					}
					uint32_t next = U32(link, 0);
					if (next == 1) break;
					link = next;
				}
			}
		}
	}

	// 0x616AC0: the stage ground polygons from the workspace: depth-cued gouraud primitives (colour
	// towards the far colour 0 by each vertex's +6) where any vertex is lowered, the stage's own
	// flat-textured ones (clipped, second OT) elsewhere
	static uint32_t GroundPolys(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint32_t ws = U32(h, 4);
		uint32_t pd = U32(h, 0);
		for (uint32_t i = 0; i < U16(h, 0xC); i++, pd += 0x14)
		{
			Copy8(h + 0x68, ws + (U16(pd, 0) & 0xFFF) * 8);
			Copy8(h + 0x70, ws + (U16(pd, 2) & 0xFFF) * 8);
			Copy8(h + 0x78, ws + (U16(pd, 4) & 0xFFF) * 8);
			LoadV012(h + 0x68, h + 0x70, h + 0x78);
			RTPT();
			S32(h, 0x44) = Div3(S16(h, 0x6E) + S16(h, 0x7E) + S16(h, 0x76));
			if (S32(h, 0x44) != 0)
			{
				U32(pk, 0) = 0x9000000;
				U32(pk, 4) = U32(pd, 0x10);
				U32(pk, 0xC) = U32(pd, 8);
				U32(pk, 0x18) = U32(pd, 0xC);
				U8(pk, 7) = 0x34;
				ReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
				AVSZ3();
				U32(pk, 0x24) = (uint32_t)(S32(pd, 4) >> 16);
				ReadOTZ(h + 0x48);
				SetRGBC(pk + 4);
				SetIR0Word(h + 0x6E);
				DPCS();
				StoreRGB2(pk + 4);
				SetIR0Word(h + 0x76);
				DPCS();
				StoreRGB2(pk + 0x10);
				SetIR0Word(h + 0x7E);
				DPCS();
				StoreRGB2(pk + 0x1C);
				Insert(ot + (S32(h, 0x48) >> shift) * 4, pk);
				pk += 0x28;
			}
			else
			{
				U32(pk, 0) = 0x7000000;
				U32(pk, 4) = U32(pd, 0x10);
				U32(pk, 0xC) = U32(pd, 8);
				U32(pk, 0x14) = U32(pd, 0xC);
				ReadFLAG(h + 0x4C);
				if (U32(h, 0x4C) & 0x60000) continue;
				ReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
				AVSZ3();
				uint32_t f = TriClip(pk + 8, pk + 0x10, pk + 0x18);
				if ((f & 7) == 7 || (f & 0x70) == 0x70) continue;
				U32(pk, 0x1C) = (uint32_t)(S32(pd, 4) >> 16);
				ReadOTZ(h + 0x48);
				Insert(U32(h, 0x2C) + (S32(h, 0x48) >> 12) * 4, pk);
				pk += 0x20;
			}
		}
		for (uint32_t i = 0; i < U16(h, 0xE); i++, pd += 0x18)
		{
			Copy8(h + 0x68, ws + (U16(pd, 0) & 0xFFF) * 8);
			Copy8(h + 0x70, ws + (U16(pd, 2) & 0xFFF) * 8);
			Copy8(h + 0x78, ws + (U16(pd, 4) & 0xFFF) * 8);
			Copy8(h + 0x80, ws + (U16(pd, 6) & 0xFFF) * 8);
			LoadV012(h + 0x68, h + 0x70, h + 0x78);
			RTPT();
			int32_t sum = S16(h, 0x6E) + S16(h, 0x86) + S16(h, 0x7E) + S16(h, 0x76);
			S32(h, 0x44) = (sum + ((sum >> 31) & 3)) >> 2;
			if (S32(h, 0x44) != 0)
			{
				U32(pk, 0) = 0xC000000;
				U32(pk, 4) = U32(pd, 0x14);
				U32(pk, 0xC) = U32(pd, 8);
				U32(pk, 0x18) = U32(pd, 0xC);
				U8(pk, 7) = 0x3C;
				ReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
				LoadV0(h + 0x80);
				RTPS();
				U32(pk, 0x24) = U32(pd, 0x10);
				U32(pk, 0x30) = (uint32_t)(S32(pd, 0x10) >> 16);
				ReadSXY2(pk + 0x2C);
				AVSZ4();
				ReadOTZ(h + 0x48);
				SetRGBC(pk + 4);
				SetIR0Word(h + 0x6E);
				DPCS();
				StoreRGB2(pk + 4);
				SetIR0Word(h + 0x76);
				DPCS();
				StoreRGB2(pk + 0x10);
				SetIR0Word(h + 0x7E);
				DPCS();
				StoreRGB2(pk + 0x1C);
				SetIR0Word(h + 0x86);
				DPCS();
				StoreRGB2(pk + 0x28);
				Insert(ot + (S32(h, 0x48) >> shift) * 4, pk);
				pk += 0x34;
			}
			else
			{
				U32(pk, 0) = 0x9000000;
				U32(pk, 4) = U32(pd, 0x14);
				U32(pk, 0xC) = U32(pd, 8);
				U32(pk, 0x14) = U32(pd, 0xC);
				ReadFLAG(h + 0x4C);
				if (U32(h, 0x4C) & 0x60000) continue;
				ReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
				LoadV0(h + 0x80);
				RTPS();
				U32(pk, 0x1C) = U32(pd, 0x10);
				U32(pk, 0x24) = (uint32_t)(S32(pd, 0x10) >> 16);
				uint32_t f = TriClip(pk + 8, pk + 0x10, pk + 0x18);
				ReadSXY2(pk + 0x20);
				AVSZ4();
				f |= OffX(S16(pk, 0x20), 8);
				f |= OffY(S16(pk, 0x22), 0x80);
				if ((f & 0xF) == 0xF || (f & 0xF0) == 0xF0) continue;
				ReadOTZ(h + 0x48);
				Insert(U32(h, 0x2C) + (S32(h, 0x48) >> 12) * 4, pk);
				pk += 0x28;
			}
		}
		return pk;
	}

	// 0x616610: the stage ground group's objects: every vertex through its bone into the workspace
	// (lowered by the crater when inside its area), then the object's polygons with the camera
	static uint32_t GroundObjects(uint32_t grp, uint32_t h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t bones = U32(grp, 0) + 0x10;
		U32(h, 0x2C) = OTBase() + 0x4068;
		uint32_t ob = U32(grp, 4);
		int32_t count = S32(ob, 0);
		ob += 4;
		int32_t x = S16(h, 0xB8);
		S32(h, 0x50) = x - 0x8D8;
		S32(h, 0x54) = x + 0x7E8;
		int32_t z = S16(h, 0xBC);
		S32(h, 0x58) = z - 0x80E;
		S32(h, 0x5C) = z + 0x7DF;
		S32(h, 0x60) = 0;
		S32(h, 0x64) = 0;
		// (a debug print, nullsub_958, is called here with the centre)
		FarColour(0, 0, 0);
		for (; count > 0; count--)
		{
			uint32_t d = U32(grp, 4) + U32(ob, 0);
			uint32_t ws = U32(h, 4);
			ob += 4;
			int32_t groups = S16(d, 0);
			d += 2;
			for (; groups > 0; groups--)
			{
				uint32_t m = bones + S16(d, 0) * 0x30 + 0x10;
				d += 2;
				SetRot(m);
				SetTrans(m);
				int32_t nv = S16(d, 0);
				d += 2;
				for (; nv > 0; nv--, ws += 8)
				{
					LoadV0Words(d);
					d += 6;
					RotV0Tr();
					StoreIR(ws);
					int32_t vx = S16(ws, 0);
					if (vx < S32(h, 0x50) || vx > S32(h, 0x54)) continue;
					int32_t vz = S16(ws, 4);
					if (vz < S32(h, 0x58) || vz > S32(h, 0x5C)) continue;
					GroundLower(h, ws);
				}
			}
			uint32_t a = (d + 3) & ~3u;
			U16(h, 8) = U16(a, 0);
			U16(h, 0xA) = U16(a, 2);
			U16(h, 0xC) = U16(a, 4);
			U16(h, 0xE) = U16(a, 6);
			U32(h, 0) = a + 0xC;
			SetRot(CAMERA);
			SetTrans(CAMERA);
			cursor = GroundPolys(h, ot, shift, cursor);
		}
		return cursor;
	}

	// 0x616590
	static void GroundDraw(uint32_t h)
	{
		S16(h, 0x14) = 0;
		S16(h, 0x16) = 0;
		S16(h, 0x18) = 0x140;
		U16(h, 0x24) = var<uint16_t>(0x1D989BE);
		S16(h, 0x1A) = 0xD8;
		U32(h, 0x1C) = var<uint32_t>(0x1D989E4);
		U32(h, 4) = var<uint32_t>(0x1D98B3C);
		U32(h, 0x10) = var<uint32_t>(0x1D969A8);
		U32(h, 0x20) = 0xFFFFFFFF;
		BuildBones(STAGE_GroundAnim);
		FrameArena() = GroundObjects(STAGE_Ground, h, OTBase() + 0x4068, 6, FrameArena());
	}

	// crater depth factor of a counter value: full, then fading at 64..67
	inline int32_t GroundFactor(int32_t c) { return c >= 0x40 ? (0x44 - c) << 10 : 0x1000; }

	static void GroundTaskDraw(uint32_t g, int32_t factor)
	{
		uint32_t h = Alloc(0xC8);
		U32(h, 0xB8) = U32(g, 0x10);
		U32(h, 0xBC) = U32(g, 0x14);
		U32(h, 0x30) = MESH_Crater;
		S32(h, 0x3C) = factor;
		GroundDraw(h);
		FieldFree(0xC8);
	}

	// ------------------------------------------------------------------
	// Ground (0x6164E0): node +0x0C counter, +0x10 centre
	// ------------------------------------------------------------------
	static uint32_t __cdecl GroundTask(TaskNode *n)
	{
		uint32_t g = (uint32_t)n;
		StageGroundFlags() &= 0xFD;
		if (S16(g, 0xC) < 0x44)
		{
			// 30 fps layer: see mag146_tornado_held.inc
			FX_HELD(held_note_draw(ORIG_Ground, g);)
			GroundTaskDraw(g, GroundFactor(S16(g, 0xC)));
		}
		if (DrawOnly()) return 0;
		S16(g, 0xC) = (int16_t)(S16(g, 0xC) + 1);
		if (S16(g, 0xC) <= 0x44) return 0;
		StageGroundFlags() |= 2;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Funnel
	// ------------------------------------------------------------------

	// 0x617720 (second half): ring k from its tilt angles: tilted radius vector (0x400 + 192 k) * c
	// gives x / z and a sway of y, height -(256 k * a) >> 12, radius b * (1 - (1 - d) * sin(sin(256 k
	// * e + f) / 4)). h = 0x28 bytes of scratch (angles, matrix).
	static void RingPlace(uint32_t ring, int32_t k, const FunnelParams &P, uint32_t h)
	{
		uint32_t a = ring + 8;
		int32_t e = shl32(k, 12);
		int32_t kk = (e + ((e >> 31) & 0xF)) >> 4;
		S16(h, 0) = (int16_t)(mul32(Sin(S16(a, 0)), 288) >> 12);
		S16(h, 2) = (int16_t)(mul32(Sin(S16(a, 2)), 288) >> 12);
		S16(h, 4) = (int16_t)(mul32(Sin(S16(a, 4)), 288) >> 12);
		ComposeZYX(h, h + 8);
		int32_t r = (shl32(kk * 3, 10) >> 12) + 0x400;
		S16(h, 0) = (int16_t)(mul32(r, P.p[2]) >> 12);
		S16(h, 2) = 0;
		S16(h, 4) = 0;
		MatVec(h + 8, h, h);
		int32_t hy = mul32(kk, P.p[0]) >> 12;
		S16(ring, 0) = S16(h, 0);
		S16(ring, 2) = (int16_t)((int16_t)(S16(h, 2) >> 4) - hy);
		S16(ring, 4) = S16(h, 4);
		int32_t ph = (mul32(kk, P.p[4]) >> 12) + P.p[5];
		int32_t s = Sin(Sin(ph) >> 2);
		int32_t w = 0x1000 - (mul32(0x1000 - P.p[3], s) >> 12);
		S32(ring, 0x10) = mul32(w, P.p[1]) >> 12;
	}

	// 0x617720: rings 16..1 take the previous ring's tilt with a random jitter (-64..63); ring 0 takes
	// the wander offset, which moves along random-walk angles
	static void FunnelUpdate(const FunnelParams &P)
	{
		uint32_t h = Alloc(0x28);
		int32_t k = 16;
		for (uint32_t ring = RINGS + 16 * RING_SIZE; ring >= RINGS; ring -= RING_SIZE, k--)
		{
			uint32_t a = ring + 8;
			if (ring > RINGS)
			{
				for (int m = 0; m < 3; m++)
				{
					int32_t r = CrtRand() % 128;
					S16(a, 2 * m) = (int16_t)(r + S16(a, 2 * m - 0x14) - 0x40);
				}
			}
			else
			{
				S16(a, 0) = S16(WANDER_Offset, 0);
				S16(a, 2) = S16(WANDER_Offset, 2);
				S16(a, 4) = S16(WANDER_Offset, 4);
				for (int m = 0; m < 3; m++)
				{
					int32_t r = CrtRand() % 512;
					S16(WANDER_Angle, 2 * m) = (int16_t)(S16(WANDER_Angle, 2 * m) + (int16_t)(r - 0x100));
				}
				for (int m = 0; m < 3; m++)
					S16(WANDER_Offset, 2 * m) = (int16_t)(S16(WANDER_Offset, 2 * m) + (int16_t)(shl32(Sin(S16(WANDER_Angle, 2 * m)), 9) >> 12));
			}
			RingPlace(ring, k, P, h);
		}
		FieldFree(0x28);
	}

	// 0x6171F0: parameters of the four phases (grow 0..39, hold 40..57, shrink 58..75)
	static FunnelParams FunnelPhase(int32_t c, uint32_t node)
	{
		FunnelParams P;
		if (c < 0x28)
		{
			int32_t t = shl32(c, 12) / 40;
			P.p[0] = (mul32(t, 4920) >> 12) + 0x50;
			P.p[1] = (mul32(t, 1500) >> 12) + 0x5DC;
			P.p[2] = (mul32(t, 900) >> 12) + 0x64;
			P.p[3] = (mul32(t, -640) >> 12) + 0x900;
			P.p[4] = 0x800;
			P.p[5] = (mul32(t, -3328) >> 12) + 0xD00;
		}
		else if (c - 0x28 < 0x12)
		{
			P.p[0] = 0x1388;
			P.p[1] = 0xBB8;
			P.p[2] = 0x3E8;
			P.p[3] = 0x680;
			P.p[4] = 0x800;
			P.p[5] = 0;
		}
		else if (c - 0x3A < 0x12)
		{
			int32_t t = shl32(c - 0x3A, 12) / 18;
			P.p[0] = (mul32(t, 4000) >> 12) + 0x1388;
			P.p[1] = (mul32(t, -2600) >> 12) + 0xBB8;
			P.p[2] = (mul32(t, -200) >> 12) + 0x3E8;
			P.p[3] = (mul32(t, -1664) >> 12) + 0x680;
			P.p[4] = (mul32(t, -768) >> 12) + 0x800;
			P.p[5] = mul32(t, 768) >> 12;
		}
		else
		{
			// never reached (the task ends at 76): the original passes its node pointer six times
			for (int k = 0; k < 6; k++) P.p[k] = (int32_t)node;
		}
		return P;
	}

	// 0x617460: the funnel from the 17 rings: 16 vertices per ring at angle + 256 j (+ 0x40 per ring),
	// quad j of ring r between rings r and r - 1, into packet 2 * (16 (r - 1) + j) + parity
	static void FunnelDraw(uint32_t rings, uint32_t packets, int32_t angle, int32_t alpha)
	{
		uint32_t h = Alloc(0x120);
		uint32_t colour = (uint32_t)alpha | 0x2E00;
		colour = (colour << 8) | (uint32_t)alpha;
		colour = (colour << 8) | (uint32_t)alpha;
		int32_t radius = S32(rings, 0x10);
		U32(h, 0) = U32(rings, 0);
		U32(h, 4) = U32(rings, 4);
		for (int32_t j = 0; j < 16; j++)
		{
			int32_t a = j * 0x100 + angle;
			uint32_t v = h + 8 + 8 * j;
			S16(v, 0) = (int16_t)((int16_t)(mul32(Sin(a), radius) >> 12) + S16(h, 0));
			S16(v, 2) = S16(h, 2);
			S16(v, 4) = (int16_t)((int16_t)(mul32(Cos(a), radius) >> 12) + S16(h, 4));
		}
		U32(h, 0x88) = U32(h, 8);
		U32(h, 0x8C) = U32(h, 0xC);
		int32_t idx = 0;
		uint32_t par = 0;
		for (uint32_t ring = rings + RING_SIZE; ring < rings + 17 * RING_SIZE; ring += RING_SIZE)
		{
			U32(h, 0) = U32(ring, 0);
			radius = S32(ring, 0x10);
			angle += 0x40;
			par ^= 1;
			U32(h, 4) = U32(ring, 4);
			uint32_t cur = h + par * 0x88 + 8;
			for (int32_t j = 0; j < 16; j++)
			{
				int32_t a = j * 0x100 + angle;
				uint32_t v = cur + 8 * j;
				S16(v, 0) = (int16_t)((int16_t)(mul32(Sin(a), radius) >> 12) + S16(h, 0));
				S16(v, 2) = S16(h, 2);
				S16(v, 4) = (int16_t)((int16_t)(mul32(Cos(a), radius) >> 12) + S16(h, 4));
			}
			Copy8(cur + 0x80, cur);
			uint32_t prev = h + (par ^ 1) * 0x88 + 8;
			for (int32_t j = 0; j < 16; j++, idx += 2)
			{
				uint32_t pk = packets + (uint32_t)(idx + Parity()) * 0x28;
				LoadV012(cur + 8 * j, cur + 8 * j + 8, prev + 8 * j);
				RTPT();
				U32(pk, 4) = colour;
				ReadFLAG(h + 0x11C);
				if (U32(h, 0x11C) & 0x60000) continue;
				ReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
				LoadV0(prev + 8 * j + 8);
				RTPS();
				ReadSXY2(pk + 0x20);
				AVSZ4();
				ReadOTZ(h + 0x118);
				Insert(OTBase() + (S32(h, 0x118) >> 2) * 4 + 0x44, pk);
			}
		}
		FieldFree(0x120);
	}

	// fade: in over 16 ticks, out over the last 8
	inline int32_t FunnelAlpha(int32_t c) { return c < 0x10 ? c * 8 : (c >= 0x44 ? (0x4C - c) << 4 : 0x80); }

	// ------------------------------------------------------------------
	// Funnel (0x6171F0): node +0x0C counter, +0x10 position
	// ------------------------------------------------------------------
	static uint32_t __cdecl FunnelTask(TaskNode *n)
	{
		uint32_t f = (uint32_t)n;
		if (!DrawOnly()) FunnelUpdate(FunnelPhase(S16(f, 0xC), f));
		S32(FUNNEL_Matrix, 0x14) = S16(f, 0x10);
		S32(FUNNEL_Matrix, 0x18) = S16(f, 0x12);
		S32(FUNNEL_Matrix, 0x1C) = S16(f, 0x14);
		Mat4x3 m;
		Compose(CAMERA, FUNNEL_Matrix, (uint32_t)&m);
		SetRotW((uint32_t)&m);
		SetTransW((uint32_t)&m);
		int32_t c = S16(f, 0xC);
		// 30 fps layer: see mag146_tornado_held.inc
		FX_HELD(held_note_draw(ORIG_Funnel, f);)
		FunnelDraw(RINGS, FUNNEL_Packets, c * 100, FunnelAlpha(c));
		if (DrawOnly()) return 0;
		S16(f, 0xC) = (int16_t)(S16(f, 0xC) + 1);
		return S16(f, 0xC) >= 0x4C ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Sparks (0x617990): pool entries +0 frame (s32, -1 = free), +4 size (s32), +8 x, +A y, +C z,
	// +E fall speed (s16)
	// ------------------------------------------------------------------
	static void SparkDraw(uint32_t h, uint32_t e)
	{
		int32_t size = S32(e, 4);
		U16(h, 4) = (uint16_t)S32(e, 0);
		ShadowRotation(e + 8, (int16_t)size, -(size >> 3));
		PacketCursor() = SpriteSeq(h, OT44(), 2, PacketCursor());
	}

	// shrink by 0x20 and fall faster by 1/8 per tick; gone after 16 frames
	static void SparkUpdate(uint32_t e)
	{
		S32(e, 0) = S32(e, 0) + 1;
		if (S32(e, 0) >= 0x10)
		{
			S32(e, 0) = -1;
			return;
		}
		S32(e, 4) -= 0x20;
		S16(e, 0xA) = (int16_t)(S16(e, 0xA) + S16(e, 0xE));
		int16_t v = S16(e, 0xE);
		S16(e, 0xE) = (int16_t)(v + (int16_t)(v >> 3));
	}

	static uint32_t __cdecl SparksTask(TaskNode *n)
	{
		uint32_t s = (uint32_t)n;
		uint32_t h = Alloc(0xB4);
		U32(h, 0) = SEQ_Spark;
		S16(h, 0x24) = 0;
		for (uint32_t e = SPARKS; e < SPARKS_End; e += 0x10)
		{
			if (S32(e, 0) < 0) continue;
			// 30 fps layer: see mag146_tornado_held.inc
			FX_HELD(held_note_item(ORIG_Sparks, e, h);)
			SparkDraw(h, e);
			if (DrawOnly()) continue;
			SparkUpdate(e);
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		int16_t c = S16(s, 0xC);
		if (c >= 1 && c <= 0x32)
		{
			int32_t count = c < 0x19 ? 3 : (c < 0x26 ? 2 : 1);
			for (int32_t i = 0; i < count; i++)
			{
				int32_t k = 0;
				uint32_t e = SPARKS;
				while (S32(e, 0) >= 0)
				{
					e += 0x10;
					k++;
					if (e >= SPARKS_End) goto done;
				}
				if (k >= 0x3C) break;
				S32(e, 0) = 0;
				S32(e, 4) = CrtRand() % 0x1800 + 0x900;
				int32_t a = CrtRand() % 4096;
				int32_t r = CrtRand() % 0x44C + 0x76C;
				S16(e, 8) = (int16_t)((int16_t)(mul32(Cos(a), r) >> 12) + S16(s, 0x10));
				S16(e, 0xA) = (int16_t)(-40 - CrtRand() % 140);
				S16(e, 0xC) = (int16_t)((int16_t)(mul32(Sin(a), r) >> 12) + S16(s, 0x14));
				S16(e, 0xE) = (int16_t)(-20 - CrtRand() % 50);
			}
		}
	done:
		S16(s, 0xC) = (int16_t)(S16(s, 0xC) + 1);
		return S16(s, 0xC) >= 0x46 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Morph cloud (0x617C50): node +0x0C frame d, +0x0E buffer, +0x10 position, +0x1C scale
	// ------------------------------------------------------------------
	static uint32_t __cdecl MorphTask(TaskNode *n)
	{
		uint32_t p = (uint32_t)n;
		int16_t angles[4] = { 0, 0, 0, 0 };
		Mat4x3 m;
		BuildRot((uint32_t)angles, (uint32_t)&m);
		m.t[1] = S16(p, 0x12);
		m.t[0] = S16(p, 0x10);
		int32_t sc = S16(p, 0x1C);
		int32_t scale[3] = { sc, sc, sc };
		m.t[2] = S16(p, 0x14);
		Scale3D((uint32_t)&m, (uint32_t)scale);
		Compose(CAMERA, (uint32_t)&m, (uint32_t)&m);
		SetRotW((uint32_t)&m);
		SetTransW((uint32_t)&m);
		uint32_t h = Alloc(0x58);
		int16_t d = S16(p, 0xC);
		U32(h, 0) = MORPH_Model;
		U8(h, 0xA) = 0;
		U8(h, 9) = 0;
		U8(h, 8) = 0;
		S32(h, 0xC) = d * 341;
		if (d < 12)
		{
			uint32_t buf = MORPH_Verts + S16(p, 0xE) * 0x3C0;
			for (int i = 0; i < 120; i++)
				for (int c = 0; c < 3; c++)
					S16(buf + 8 * i, 2 * c) = (int16_t)((int16_t)(S16(MORPH_Delta + 8 * i, 2 * c) * d) + S16(MORPH_Base + 8 * i, 2 * c));
			U32(h, 0x1C) = 0x20C0;
			U32(h, 4) = buf;
		}
		else
		{
			U32(h, 4) = MORPH_Target;
			U32(h, 0x1C) = 0x20C0;
		}
		// 30 fps layer: see mag146_tornado_held.inc
		FX_HELD(held_note_item(ORIG_Morph, p, h);)
		PacketCursor() = PrimModel(h, OT44(), 2, PacketCursor());
		FieldFree(0x58);
		if (DrawOnly()) return 0;
		S16(p, 0xC) = (int16_t)(S16(p, 0xC) + 1);
		return S16(p, 0xC) >= 0xD ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Lift (0x617DF0): node +0x0C counter, +0x0E action, +0x10 centre, +0x18 next debris sprite.
	// Records (7 x 0x34, by entity slot): +0 1 = not lifted, +4 entity flags & 0x1020, +8 / +C / +10 /
	// +14 entity +0x1C / +0x20 / +0x0C / +0x10 saved, +18 x - centre x, +1A y, +1C z - centre z,
	// +1E rise speed, +22 spin, +26 spin speed, +28 orbit angle (s32), +2C orbit speed (s32),
	// +30 landing height (effect spawn position y)
	// ------------------------------------------------------------------
	static uint32_t __cdecl LiftTask(TaskNode *n)
	{
		uint32_t l = (uint32_t)n;
		if (S16(l, 0xC) == 0)
		{
			for (uint32_t r = LIFT; r < LIFT_End; r += LIFT_SIZE) U32(r, 0) = 1;
			int32_t count = Ctx()->actions[S16(l, 0xE)].target_count;
			S32(TARGET_Count, 0) = count;
			for (int32_t k = 0; k < S32(TARGET_Count, 0); k++)
			{
				int32_t slot = Ctx()->actions[S16(l, 0xE)].targets[k * TARGET_STRIDE];
				S32(TARGET_Slots, 4 * k) = slot;
				uint32_t r = LIFT + LIFT_SIZE * slot;
				uint32_t e = Entity(slot);
				uint32_t e1C = U32(e, 0x1C), e20 = U32(e, 0x20), e0C = U32(e, 0xC), e10 = U32(e, 0x10);
				U32(r, 0) = 0;
				U16(r, 4) = (uint16_t)(U16(e, 0) & 0x1020);
				U32(r, 0x18) = e1C;
				U32(r, 0x1C) = e20;
				U32(r, 8) = e1C;
				U32(r, 0xC) = e20;
				U32(r, 0x20) = e0C;
				U32(r, 0x24) = e10;
				U32(r, 0x10) = e0C;
				U32(r, 0x14) = e10;
				S16(r, 0x18) = (int16_t)(S16(r, 0x18) - S16(l, 0x10));
				S16(r, 0x1C) = (int16_t)(S16(r, 0x1C) - S16(l, 0x14));
				S16(r, 0x1E) = (int16_t)(-60 - CrtRand() % 110);
				S16(r, 0x26) = (int16_t)(CrtRand() % 40 + 0xF);
				S32(r, 0x28) = 0;
				S32(r, 0x2C) = CrtRand() % 60 + 0x14;
				int16_t pos[4];
				SpawnPosition(e, 0xF0, 0x1000, (uint32_t)pos);
				U8(e, 1) |= 0x10;
				S32(r, 0x30) = pos[1];
			}
		}
		for (int slot = 0; slot < 7; slot++)
		{
			uint32_t r = LIFT + LIFT_SIZE * slot;
			uint32_t e = Entity(slot);
			if (U32(r, 0) != 0) continue;
			int16_t c = S16(l, 0xC);
			if (c < 0x48)
			{
				// orbit: the offset from the centre turned by the orbit angle
				int16_t angles[4] = { 0, S16(r, 0x28), 0, 0 };
				Mat4x3 m;
				BuildRot((uint32_t)angles, (uint32_t)&m);
				int16_t v[4];
				MatVec((uint32_t)&m, r + 0x18, (uint32_t)v);
				v[0] = (int16_t)(v[0] + S16(l, 0x10));
				v[2] = (int16_t)(v[2] + S16(l, 0x14));
				v[1] = S16(r, 0x1A);
				v[3] = S16(r, 0xE);
				U32(e, 0x1C) = *(uint32_t *)&v[0];
				U32(e, 0x20) = *(uint32_t *)&v[2];
				S16(e, 0xE) = S16(r, 0x22);
			}
			else if (c < 0x52)
			{
				// dropped: back to its place, at its height, turned half way round
				S16(e, 0x1C) = S16(r, 8);
				S16(e, 0x1E) = S16(r, 0x1A);
				S16(e, 0x20) = S16(r, 0xC);
				S16(e, 0x1E) = (int16_t)(S16(e, 0x1E) + (int16_t)S32(r, 0x30));
				S16(e, 0xC) = S16(r, 0x10);
				S16(e, 0xE) = S16(r, 0x12);
				S16(e, 0x10) = (int16_t)(S16(r, 0x14) + 0x800);
			}
			else
			{
				U32(e, 0x1C) = U32(r, 8);
				U32(e, 0x20) = U32(r, 0xC);
				U32(e, 0xC) = U32(r, 0x10);
				U32(e, 0x10) = U32(r, 0x14);
			}
		}
		if (DrawOnly()) return 0;
		for (int slot = 0; slot < 7; slot++)
		{
			uint32_t r = LIFT + LIFT_SIZE * slot;
			uint32_t e = Entity(slot);
			if (U32(r, 0) != 0) continue;
			int16_t c = S16(l, 0xC);
			if (c < 0x48)
			{
				if (c >= 0x1A && S16(r, 0x1A) > -12000)
				{
					S16(r, 0x1A) = (int16_t)(S16(r, 0x1A) + S16(r, 0x1E));
					int16_t v = S16(r, 0x1E);
					S16(r, 0x1E) = (int16_t)(v + (int16_t)(v >> 5));
				}
				S16(r, 0x22) = (int16_t)(S16(r, 0x22) + S16(r, 0x26));
				int16_t w = S16(r, 0x26);
				S16(r, 0x26) = (int16_t)(w + (int16_t)(w >> 6));
				S32(r, 0x28) += S32(r, 0x2C);
				S32(r, 0x2C) += S32(r, 0x2C) >> 5;
				if (S16(r, 0x1A) < -200) U8(e, 0) |= 0x20;
			}
			else if (c == 0x48)
			{
				int32_t y = S16(r, 0x1A);
				S16(r, 0x1E) = (int16_t)-((y + ((y >> 31) & 7)) >> 3);
				S16(r, 0x18) = S16(r, 8);
				S16(r, 0x1A) = (int16_t)(S16(r, 0x1A) + S16(r, 0x1E));
				S16(r, 0x1C) = S16(r, 0xC);
			}
			else if (c < 0x50)
				S16(r, 0x1A) = (int16_t)(S16(r, 0x1A) + S16(r, 0x1E));
			else if (c == 0x50)
			{
				uint32_t b = Add(Q_FX, ORIG_Debris);
				U32(b, 0x10) = U32(r, 0x18);
				int16_t first = S16(l, 0x18);
				S16(b, 0xE) = first;
				U32(b, 0x14) = U32(r, 0x1C);
				S16(b, 0xC) = 0;
				int16_t end = (int16_t)(first + 0x14);
				S16(l, 0x18) = end;
				for (int32_t i = S16(b, 0xE); i < end; i++) U16(DEBRIS_Life + 8 * i, 6) = 0xFFFF;
				BdPlaySE3D(SOUND_Land, 0x8000, (const int16_t *)(r + 0x18));
			}
			else
				CameraShake() = (int16_t)(CameraShake() + ((c & 1) ? -12 : 12));
		}
		S16(l, 0xC) = (int16_t)(S16(l, 0xC) + 1);
		if (S16(l, 0xC) <= 0x53) return 0;
		for (int slot = 0; slot < 7; slot++)
		{
			uint32_t r = LIFT + LIFT_SIZE * slot;
			uint32_t e = Entity(slot);
			if (U32(r, 0) == 0) U16(e, 0) = (uint16_t)((U16(e, 0) & 0xEFDF) | U16(r, 4));
		}
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Debris burst (0x618280): node +0x0C counter, +0x0E first sprite, +0x10 landing point.
	// Sprite i: life table +0 size, +6 frame (-1 = none); position +0 x, +2 y, +4 z, +8 / +C speed x / z
	// ------------------------------------------------------------------
	static void DebrisDraw(uint32_t h, uint32_t life, uint32_t pos)
	{
		U16(h, 4) = U16(life, 6);
		int16_t size = S16(life, 0);
		ShadowRotation(pos, size, -(size >> 3));
		PacketCursor() = SpriteSeq(h, OT44(), 2, PacketCursor());
	}

	// slide outwards, slowing by 1/16 per tick; gone after 16 frames
	static void DebrisUpdate(uint32_t life, uint32_t pos)
	{
		U16(life, 6) = (uint16_t)(U16(life, 6) + 1);
		if (S16(life, 6) >= 0x10)
		{
			U16(life, 6) = 0xFFFF;
			return;
		}
		S16(pos, 0) = (int16_t)(S16(pos, 0) + S16(pos, 8));
		S16(pos, 4) = (int16_t)(S16(pos, 4) + S16(pos, 0xC));
		int16_t v = S16(pos, 8);
		S16(pos, 8) = (int16_t)(v - (int16_t)(v >> 4));
		v = S16(pos, 0xC);
		S16(pos, 0xC) = (int16_t)(v - (int16_t)(v >> 4));
	}

	static uint32_t __cdecl DebrisTask(TaskNode *n)
	{
		uint32_t b = (uint32_t)n;
		uint32_t h = Alloc(0xB4);
		int32_t first = S16(b, 0xE);
		uint32_t pos0 = DEBRIS_Pos + 16 * first, life0 = DEBRIS_Life + 8 * first;
		U32(h, 0) = SEQ_Spark;
		S16(h, 0x24) = 0;
		for (int i = 0; i < 0x14; i++)
		{
			uint32_t life = life0 + 8 * i, pos = pos0 + 16 * i;
			if (S16(life, 6) < 0) continue;
			// 30 fps layer: see mag146_tornado_held.inc
			FX_HELD(held_note_debris(life, pos, h);)
			DebrisDraw(h, life, pos);
			if (DrawOnly()) continue;
			DebrisUpdate(life, pos);
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (S16(b, 0xC) == 0)
		{
			for (int i = 0; i < 0x14; i++)
			{
				uint32_t life = life0 + 8 * i, pos = pos0 + 16 * i;
				S16(life, 6) = 0;
				S16(life, 0) = (int16_t)(CrtRand() % 0x1800 + 0x900);
				S16(pos, 0) = S16(b, 0x10);
				S16(pos, 2) = 0;
				S16(pos, 4) = S16(b, 0x14);
				int32_t a = CrtRand() % 4096;
				int32_t r = CrtRand() % 0xA0 + 0x5A;
				S16(pos, 8) = (int16_t)(mul32(Cos(a), r) >> 12);
				S16(pos, 0xC) = (int16_t)(mul32(Sin(a), r) >> 12);
			}
		}
		S16(b, 0xC) = (int16_t)(S16(b, 0xC) + 1);
		return S16(b, 0xC) >= 0x11 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Texture restore (0x618480): node +0x0C counter, +0x0E done flag
	// ------------------------------------------------------------------
	static uint32_t __cdecl TexRestoreTask(TaskNode *n)
	{
		uint32_t t = (uint32_t)n;
		if (S16(t, 0xC) == 0)
		{
			S16(t, 0xE) = 0;
			TexRestore(TimFile(), t + 0xE);
		}
		int16_t done = S16(t, 0xE);
		S16(t, 0xC) = (int16_t)(S16(t, 0xC) + 1);
		return done ? TASK_END : 0;
	}
}

	void register_mag146_tornado()
	{
		register_port(tornado146::ORIG_Root, (void *)tornado146::RootTask, "146 RootTask", 146);
		register_port(tornado146::ORIG_Director, (void *)tornado146::DirectorTask, "146 DirectorTask", 146);
		register_port(tornado146::ORIG_Piece, (void *)tornado146::PieceTask, "146 PieceTask", 146);
		register_port(tornado146::ORIG_Ground, (void *)tornado146::GroundTask, "146 GroundTask", 146);
		register_port(tornado146::ORIG_Funnel, (void *)tornado146::FunnelTask, "146 FunnelTask", 146);
		register_port(tornado146::ORIG_Sparks, (void *)tornado146::SparksTask, "146 SparksTask", 146);
		register_port(tornado146::ORIG_Morph, (void *)tornado146::MorphTask, "146 MorphTask", 146);
		register_port(tornado146::ORIG_Lift, (void *)tornado146::LiftTask, "146 LiftTask", 146);
		register_port(tornado146::ORIG_Debris, (void *)tornado146::DebrisTask, "146 DebrisTask", 146);
		register_port(tornado146::ORIG_TexRestore, (void *)tornado146::TexRestoreTask, "146 TexRestoreTask", 146);
		// 30 fps layer: see mag146_tornado_held.inc
		FX_HELD(register_mag146_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag146_tornado_held.inc"
#endif
