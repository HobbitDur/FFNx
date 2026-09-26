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

// Effect 326: Odin - Zantetsuken Reverse (Seifer kills Odin; timeline-B GF family, MAG_326_*).
//
// The module is a re-linked copy of Odin 187 (mag187_odin.cpp): about half of its functions are
// byte-identical to their 187 twins except for addresses (model buffer offsets +0x800, module
// globals 0x24F0BD0..0x24F2308, exe data tables 0xE10xxx..0xE19xxx). The second half (Odin cut in
// two) replaces the target slices of 187 by a sliced creature and adds new tasks.
//
// Structure:
//   setup MAG_326_ODIN_ZANTETSUKEN_REVERSE (0x62A7E0, not ported: runs once) - model buffer
//     mb = 0x20DFAB8 (dword 0x24F22DC); sprite pools 0xE19668 = mb+0x30800 (A), 0xE1966C =
//     mb+0x31800 (B), 0xE19670 = mb+0x32800 (C), 150 x 0x18 each; 0xE196B0 = mb+0x38800 (vertex
//     scratch of the model draws and of the morph flash); dword 0x24F22E4 = 0xEECC30 (exe data:
//     warp texture source, later a character-load destination); root queue 0x24F0ED8 (pool
//     0x24F0EB8, 2 x 0x10) with the master, sub-queue 0x24F1320 (pool 0x24F1330, 100 x 0x24)
//     with the timeline.
//   SequenceTick (master, 0x6326B0) - runs the sub-queue, ++counter, ends when it is empty.
//   TimelineTask (0x62A9C0) - 410 ticks: camera script, streams, loads, sounds, chain
//     transformations of the caster, screen flash, party hide/show, spawns (grids 6, debris 10,
//     intro 40, creature 52, grids 2 at 322, ghost + its sword 323, walls 352, the two fading
//     models + screen tint 382, end 410), the action result at 408.
//   CreatureTask (0x62CEE0, spawned at 52) - 256 ticks, state E = global 0x24F1268 (Odin); the
//     187 choreography up to tick 173, then Odin is cut in two (174..253) by the module's clipped
//     renderer MAG_326_sub_62D6C0 (two halves drifting / tilting apart), blood bursts at 246.
//   Two more model states: E2 = 0x24F0EE8 (ghost-trail model 323.., faded model 382..) and E3 =
//     0x24F0D50 (second faded model 382..).
//   The timeline also queues the shared camera-script task MAG_066_sub_63E9C0 (Doomtrain file);
//   it is not part of this module and is not ported here. The module never writes the battle
//   camera itself (no held-frame camera).
// Every task tests battle_to_update_flags_dword_1D96A9C & 0x201 (draw-only when set), except the
// master, the end task 0x62CEA0 and the blade's done test.
// Module-private renderers (pure functions of a scratch header: grid 0x62B1C0, clipped model
// 0x62D6C0, slash model 0x6314A0, screen tile 0x62CE20) and the bone position helper 0x631050
// (a module copy of GetEffectSpawnPosition) are called through their original addresses.

#include "fx_port.h"

namespace ff8fx
{
namespace o326
{
	using namespace eng;

	// --- engine functions used by this module (original addresses) ---
	namespace x
	{
		inline int32_t CrtRand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); }
		inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
		inline void ComposeZYXRotationMatrix(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x56CE30)(angles, out); } // writes the 3x3 only (t kept)
		inline void Scale3DMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); }
		inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); } // b.rot = a.rot * b.rot
		inline int32_t NormalizeVectorToFixedPoint(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); }
		inline void NormalizeVector16(const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const int16_t *, int16_t *)>(0x56BDE0)(in, out); } // sub_56BDE0 (float)
		inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
		inline uint32_t MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { return fn<uint32_t (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
		inline void TransformVectorBy3x3Matrix(const Mat4x3 *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int32_t *, int32_t *)>(0x56C600)(m, in, out); }
		inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
		inline int32_t GetDefaultEffectPosition(void *entity, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int16_t *)>(0x571400)(entity, out); }
		// GetRotationBetweenVectors_AxisAngle: returns the angle, writes the axis
		inline int32_t RotationBetweenVectors(uint32_t from, const int32_t *to, int32_t *axis) { return fn<int32_t (__cdecl *)(uint32_t, const int32_t *, int32_t *)>(0x571480)(from, to, axis); }
		inline void AxisAngleMatrix(int32_t angle, Mat4x3 *out, const int32_t *axis) { fn<void (__cdecl *)(int32_t, Mat4x3 *, const int32_t *)>(0x5714F0)(angle, out, axis); } // BuildAxisAngleRotationMatrix
		inline void SplineSetup(int32_t n, uint32_t points, void *work) { fn<void (__cdecl *)(int32_t, uint32_t, void *)>(0x571620)(n, points, work); }             // sub_571620
		inline void SplineEval(int32_t n, void *work, uint32_t out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, uint32_t, int32_t)>(0x571690)(n, work, out, t); } // sub_571690
		// battle model
		inline int32_t PreReadAnimation(void *header, void *cmd, int32_t id) { return fn<int32_t (__cdecl *)(void *, void *, int32_t)>(0x509440)(header, cmd, id); }
		inline int32_t ReadAnimation(void *header, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(0x508F90)(header, cmd); }
		inline uint32_t RenderGeometry(void *geom, void *hdr, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(geom, hdr, ot, shift, cursor); }
		inline uint32_t DrawShadow(void *entity, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, shift, cursor); } // sub_5088A0
		// sound / streams / files / camera script / battle
		inline void BdPlaySE(uint32_t se, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(0x501330)(se, a, b); }
		inline void BdPlaySummonStream(int32_t a, int32_t b, int32_t c) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x5018C0)(a, b, c); }
		inline void BdTransSummonStream(uint32_t src, uint32_t state) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x501860)(src, state); }
		inline int32_t VoiceSlotBusy(uint32_t slot) { return fn<int32_t (__cdecl *)(uint32_t)>(0x4A2900)(slot); }
		inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
		inline void CameraScriptStart(uint32_t script, const void *frame, void *entity, TaskQueue *q, int32_t n) { fn<void (__cdecl *)(uint32_t, const void *, void *, TaskQueue *, int32_t)>(0x63E960)(script, frame, entity, q, n); } // MAG_066_sub_63E960
		inline void CameraShake(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x5712C0)(a, b, c, d); } // au_re_BdLinkTask_6
		inline void ApplyActionResultToTargets(uint32_t list, uint32_t count) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x506BA0)(list, count); }
		inline void QueueChainTransformation(void *entity, int32_t id) { fn<void (__cdecl *)(void *, int32_t)>(0x505C00)(entity, id); }
		inline void WaitAnimSeq(uint32_t buffer, void *flag) { fn<void (__cdecl *)(uint32_t, void *)>(0x508630)(buffer, flag); }                   // sub_508630
		inline int32_t LoadState() { return fn<int32_t (__cdecl *)()>(0x508500)(); }                                                        // sub_508500: < 0 = file load busy
		inline void CharacterLoad(int32_t id, uint32_t dst) { fn<void (__cdecl *)(int32_t, uint32_t)>(0x508480)(id, dst); }                // BattleFile_CharacterLoad
		inline void QueueTIMUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(0x505E30)(tim); }                                        // Battle_QueueTIMUpload_GetEOF
		inline void QueueVramUpload(uint32_t rect, const void *data) { fn<void (__cdecl *)(uint32_t, const void *)>(0x505DF0)(rect, data); } // Battle_QueueVramUpload_Type0_RectData
		inline uint32_t SummonData() { return fn<uint32_t (__cdecl *)()>(0x571B70)(); }                                                    // sub_571B70: 0x209FAB8
		inline uint32_t Mag066Draw(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x650720)(h, ot, mode, cursor); } // MAG_066_sub_650720
		// software GTE
		inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
		inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // MAG_148_sub_64DDA0: BK = m.t
		inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }
		inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }                 // MAG_163_sub_64DE00: R = s * I
		inline void GteTransFromIR() { fn<void (__cdecl *)()>(0x64DE90)(); }                                  // MAG_148_sub_64DE90: TR = IR1..3
		inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }                                 // MAG_164_sub_64DE40: TR = MAC1..3
		inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
		inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
		inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
		inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
		inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
		inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }                   // GTE_ReadOTZ
		inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }            // set_unk_1CA8A28
		inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                                         // sub_45F270
		inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }                   // set_param_with_dword_1CA8A68
		inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
		inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DDA0)(r, g, b); } // pre_someCameraWork_45DD60
		// module-private renderers / helpers called through their original addresses
		inline void GridRender(void *h, uint32_t ot, int32_t shift) { fn<void (__cdecl *)(void *, uint32_t, int32_t)>(0x62B1C0)(h, ot, shift); } // writes the cursor itself
		inline uint32_t SliceRender(void *geom, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x62D6C0)(geom, h, ot, mode, cursor); }
		inline uint32_t SlashRender(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x6314A0)(h, ot, mode, cursor); }
		inline void ScreenTile(int32_t r, int32_t g, int32_t b, int32_t otz) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x62CE20)(r, g, b, otz); } // full-screen tile, code 0x62 (semi-transparent)
		inline int32_t BonePosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x631050)(entity, bone, frac, out); }
	}
	using namespace x;

	// --- module globals ---
	inline bool Paused() { return (var<uint32_t>(0x1D96A9C) & 0x201) != 0; } // battle_to_update_flags_dword_1D96A9C
	inline Mat4x3 &RootMatrix() { return var<Mat4x3>(0x24F2248); }  // effect frame (setup: caster rotation + 0x800, centroid 0x24F225C/60/64)
	inline Mat4x3 &Frame() { return var<Mat4x3>(0x24F2268); }       // Camera o RootMatrix, rebuilt by the timeline every tick
	inline TaskQueue &SubQueue() { return var<TaskQueue>(0x24F1320); }
	inline uint8_t *CastCtx() { return var<uint8_t *>(0x24F2174); }
	inline uint32_t &VoiceSlot() { return var<uint32_t>(0x24F1060); }
	inline uint32_t CasterSlot() { return var<uint32_t>(0x24F0C80); } // setup: first byte of castCtx->+4->+8
	inline uint8_t *Creature() { return (uint8_t *)0x24F1268; }       // E: +0x28 colour 0x24F1290, +0x40 matrix 0x24F12A8, +0x60 BattleAnimHeader, +0x6C BattleAnimCmd, +0x7C object mask 0x24F12E4
	inline Mat4x3 &CreatureMat() { return var<Mat4x3>(0x24F12A8); }
	inline uint8_t *Model2() { return (uint8_t *)0x24F0EE8; }         // E2: +0x28 colour 0x24F0F10, +0x40 matrix 0x24F0F28, +0x64 0x24F0F4C; model data 0x24F2140
	inline Mat4x3 &Model2Mat() { return var<Mat4x3>(0x24F0F28); }
	inline uint8_t *Model3() { return (uint8_t *)0x24F0D50; }         // E3: +0x28 colour 0x24F0D78, +0x40 matrix 0x24F0D90; model data 0x24F22A8
	inline Mat4x3 &BoneCopy() { return var<Mat4x3>(0x24F2288); }     // bone matrix copied by the blade (creature skeleton + 0xB30) and by the sword (E2 skeleton + 0x20)
	inline int16_t *BladeP0() { return (int16_t *)0x24F1310; }       // blade end points (effect frame), written by the blade task
	inline int16_t *BladeP1() { return (int16_t *)0x24F1318; }
	inline uint32_t &DoneFlags() { return var<uint32_t>(0x24F0C84); } // bit0 creature done
	inline uint32_t MB() { return var<uint32_t>(0x24F22DC); }        // model buffer (0x20DFAB8)
	inline uint32_t &Cursor() { return var<uint32_t>(0x1D8E054); }   // battle_texture_data_ptr_1D8E054
	inline uint32_t OT() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline uint8_t *PoolA() { return var<uint8_t *>(0xE19668); }     // mb + 0x30800
	inline uint8_t *PoolB() { return var<uint8_t *>(0xE1966C); }     // mb + 0x31800
	inline uint8_t *PoolC() { return var<uint8_t *>(0xE19670); }     // mb + 0x32800
	inline uint8_t *Entity(uint32_t slot) { return (uint8_t *)(0x1D972C0 + 156 * slot); }
	static const uint32_t HIST_A = 0x24F0BD0;  // spline input points (8 bytes each)
	static const uint32_t HIST_B = 0x24F0C28;  // spline input directions / second rail
	static const uint32_t SPLINE_A = 0x24F1068; // spline outputs (8 bytes each)
	static const uint32_t SPLINE_B = 0x24F1168;
	static const uint32_t TRAIL_RECS = 0x24F2180; // 10 x 0x14 blade trail history (to 0x24F2248)
	static const uint32_t GHOST_RECS = 0x24F0F88; // 6 x 0x24 ghost matrices (dword used + matrix, to 0x24F1060)
	static const uint32_t STREAM_STATE = 0x24F0F84;

	static const uint32_t ORIG_SequenceTick = 0x6326B0;
	static const uint32_t ORIG_TimelineTask = 0x62A9C0;
	static const uint32_t ORIG_GridATask = 0x62B080;
	static const uint32_t ORIG_GridBTask = 0x62B4F0;
	static const uint32_t ORIG_DebrisTask = 0x62B630;
	static const uint32_t ORIG_WarpTask = 0x62BB40;
	static const uint32_t ORIG_GlowTask = 0x62BC70;
	static const uint32_t ORIG_StreakTask = 0x62BDA0;
	static const uint32_t ORIG_GridCTask = 0x62BF40;
	static const uint32_t ORIG_GridDTask = 0x62C070;
	static const uint32_t ORIG_WallTask = 0x62C2F0;
	static const uint32_t ORIG_ShockTask = 0x62C430;
	static const uint32_t ORIG_GhostTask = 0x62C550;
	static const uint32_t ORIG_SwordTask = 0x62C8F0;
	static const uint32_t ORIG_FadeATask = 0x62CA80;
	static const uint32_t ORIG_FadeBTask = 0x62CC00;
	static const uint32_t ORIG_TintTask = 0x62CD40;
	static const uint32_t ORIG_EndTask = 0x62CEA0;
	static const uint32_t ORIG_CreatureTask = 0x62CEE0;
	static const uint32_t ORIG_SmokeTask = 0x62E4F0;
	static const uint32_t ORIG_RingATask = 0x62E8C0;
	static const uint32_t ORIG_RingBTask = 0x62EA10;
	static const uint32_t ORIG_DustTask = 0x62EB50;
	static const uint32_t ORIG_MistTask = 0x62EE30;
	static const uint32_t ORIG_FadeInTask = 0x62F220;
	static const uint32_t ORIG_FlashTask = 0x62F2B0;
	static const uint32_t ORIG_MorphTask = 0x62F400;
	static const uint32_t ORIG_SparkTask = 0x62F740;
	static const uint32_t ORIG_Spark2Task = 0x62FC90;
	static const uint32_t ORIG_BladeTask = 0x630160;
	static const uint32_t ORIG_TrailTask = 0x630430;
	static const uint32_t ORIG_BurstATask = 0x630870;
	static const uint32_t ORIG_BurstBTask = 0x630AB0;
	static const uint32_t ORIG_BloodTask = 0x630D30;
	static const uint32_t ORIG_SlashATask = 0x6312F0;
	static const uint32_t ORIG_SlashBTask = 0x6321A0;
	static const uint32_t ORIG_SlashCTask = 0x632350;
	static const uint32_t ORIG_SlashDTask = 0x632500;

	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static bool g_tick_paused = false; // the 0x201 flags were set on that tick (nothing advanced)

	// every sub-queue node is 0x24 bytes (pool 0x24F1330); field use differs per task
#pragma pack(push, 1)
	struct Node24
	{
		TaskNode hdr;
		int16_t c;                  // +0x0C tick counter
		int16_t e;                  // +0x0E delay / model index / pool mask / end task done flag
		int16_t p10, p12, p14, p16; // +0x10 position or velocity (p16: pad or base fade)
		int16_t a18;                // +0x18 angle / phase / history table
		int16_t s1A;                // +0x1A angle speed
		int16_t s1C, s1E;           // +0x1C scale, its speed
		int16_t s20, s22;           // +0x20 second scale / length / cut angle, its speed / delay
	};
	// sprite record of the pools A/B/C
	struct Rec
	{
		uint32_t alive;       // +0x00 owner bits (A: 1 debris, 2 smoke, 4 mist; B: 1 splash, 2 dust; C: 1 sparks, 2 sparks 2, 4 / 8 blood)
		int16_t age;          // +0x04 flipbook frame
		int16_t size;         // +0x06
		int16_t x, y, z;      // +0x08
		int16_t w0E;          // +0x0E
		int16_t vx, vy, vz;   // +0x10
		int16_t w16;          // +0x16
	};
#pragma pack(pop)
	static_assert(sizeof(Node24) == 0x24, "sub-queue node is 0x24 bytes");
	static_assert(sizeof(Rec) == 0x18, "sprite record is 0x18 bytes");

	inline Rec *RecOf(uint8_t *pool, int i) { return (Rec *)(pool + 0x18 * i); }

	// first free record (dword 0) of a pool, -1 when none
	static int FreeRecord(uint8_t *pool)
	{
		for (int i = 0; i < 150; i++)
			if (*(uint32_t *)(pool + 0x18 * i) == 0) return i;
		return -1;
	}

	// au_re_BdLinkTask_69 0x62AFB0 (no null check, as the original)
	static Node24 *LinkTask(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), task_fn);
		n->c = 0;
		n->e = 0;
		return n;
	}

	static uint32_t Rgb(int32_t v)
	{
		uint32_t c = (uint32_t)v;
		c = (c << 8) | (uint32_t)v;
		return (c << 8) | (uint32_t)v;
	}

	// ---- held-frame memos ----
	static NodeMemo<Node24, 128> g_node_memo;
	static void MemoNode(const Node24 *n) { if (Node24 *m = g_node_memo.put(n)) *m = *n; }
	static bool NodeMoved(const Node24 *m, const Node24 *n) { return n->c == (int16_t)(m->c + 1); }

	// sprite records as drawn (per pool, record, draw ordinal: pool C mask 2 is drawn twice a tick)
	struct PoolMemo { uint32_t tick; const void *owner; Rec r; bool died; };
	static PoolMemo g_pool_memo[3][150][2];
	static PoolMemo *MemoRecord(int pool, int i, const void *owner, const Rec *r)
	{
		PoolMemo *m = g_pool_memo[pool][i];
		int k = 0;
		if (m[0].tick == g_real_tick)
		{
			if (m[1].tick == g_real_tick) return nullptr;
			k = 1;
		}
		m[k].tick = g_real_tick;
		m[k].owner = owner;
		m[k].r = *r;
		m[k].died = false;
		return &m[k];
	}

	// ------------------------------------------------------------------
	// Master (0x6326B0): node from pool 0x24F0EB8, +0x0C counter (increments even when paused)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SequenceTick(TaskNode *tn)
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = Paused();
		int left = ExecuteTaskQueue(&SubQueue());
		((Node24 *)tn)->c++;
		return left ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Timeline (0x62A9C0), 410 ticks. Load waits (sub_508500 < 0) repeat the tick (at 218 the
	// chain transformation and at 252 the sound are issued again on every repeat, as vanilla).
	// ------------------------------------------------------------------
	// MAG_326_sub_62AFD0 / 62AFF0: stru_1D9898C[0..3].currentBsId bit 1 (hide / show the party)
	static void HideParty() { for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) *(uint8_t *)a |= 2; }
	static void ShowParty() { for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) *(uint8_t *)a &= 0xFD; }

	// MAG_326_sub_62B010 (tick 6): the two ground grids
	static void SpawnGrids()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_GridATask);
		n->c = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xE4A8;
		n->p14 = (int16_t)0xEE6C;
		n->a18 = 0x400;
		n->s1C = 0x3000;
		n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_GridBTask);
		n->c = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xE570;
		n->p14 = (int16_t)0xEE6C;
		n->a18 = 0x400;
		n->s1C = 0x3000;
	}

	// MAG_326_sub_62BED0 (tick 322): two more grids, higher up, unrotated
	static void SpawnGrids2()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_GridCTask);
		n->c = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xCD38;
		n->p14 = (int16_t)0xEE6C;
		n->a18 = 0;
		n->s1C = 0x3000;
		n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_GridDTask);
		n->c = 0;
		n->p10 = 0;
		n->a18 = 0;
		n->p12 = (int16_t)0xD120;
		n->p14 = (int16_t)0xEE6C;
		n->s1C = 0x3000;
	}

	// MAG_326_sub_62BA60 (tick 40): texture warp, glow, two screen streaks
	static void SpawnIntro()
	{
		Node24 *w = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_WarpTask);
		w->c = 0;
		int32_t r = CrtRand() % 0x1000;
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_GlowTask);
		w->a18 = (int16_t)r;
		n->c = 0;
		n->p10 = 0;
		n->p12 = 0;
		n->p14 = 0;
		n->s1C = 0xB00;
		n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_StreakTask);
		n->c = 0;
		n->e = 1;
		n->p10 = -50;
		n->p12 = 0x3C;
		n->a18 = 0;
		n->s1C = 0xC00;
		n->s1E = -4;
		n->s20 = 0x500;
		n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_StreakTask);
		n->e = 1;
		n->c = 0;
		n->p10 = 0x5A;
		n->p12 = 0xA;
		n->a18 = 0x800;
		n->s1C = 0x1200;
		n->s1E = -6;
		n->s20 = 0x700;
	}

	// MAG_326_sub_62C180 (tick 352): four walls (two models by +0x0E, two heights, two depths)
	// sliding apart in z, and one widening shock plane. The shock plane's +0x0E / +0x16 / +0x22
	// are not written (pool leftovers, never read by its task).
	static void SpawnWalls()
	{
		static const int16_t y[4] = { (int16_t)0xD120, (int16_t)0xD120, (int16_t)0xCF5E, (int16_t)0xCF5E };
		static const int16_t z[4] = { (int16_t)0xEE6C, (int16_t)0xFBB4, (int16_t)0xEE6C, (int16_t)0xFBB4 };
		for (int i = 0; i < 4; i++)
		{
			Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_WallTask);
			bool odd = (i & 1) != 0;
			n->c = 0;
			n->e = odd ? 1 : 0;
			n->p10 = 0;
			n->p12 = y[i];
			n->p14 = z[i];
			n->p16 = i < 2 ? 0 : 0x800;
			n->a18 = 0;
			n->s1C = 0x3000;
			n->s22 = odd ? (int16_t)-40 : (int16_t)0x32;
			n->s1E = odd ? (int16_t)0x21 : (int16_t)-33;
			n->s20 = n->s1E;
		}
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_ShockTask);
		n->c = 0;
		n->p10 = 0;
		n->a18 = 0;
		n->p12 = (int16_t)0xCF2C;
		n->p14 = (int16_t)0xEE6C;
		n->s1E = 0x120;
		n->s1C = 0x120;
		n->s20 = 0x4800;
	}

	static void Stream() { BdPlaySummonStream(0x80, 0, 0x60); }

	static uint32_t __cdecl TimelineTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ComposeAffineTransform(&Camera(), &RootMatrix(), &Frame());
		uint32_t flags = var<uint32_t>(0x1D96A9C);
		if (flags & 0x201)
		{
			if (flags & 1) return 0;
			if (LoadState() < 0) return 0;
		}
		if (n->c == 0)
		{
			CameraScriptStart(0xE197D0, &RootMatrix(), Creature(), &SubQueue(), 0);
			BdTransSummonStream(0xEECC2C, STREAM_STATE);
			VoiceSlot() = ClaimVoiceSlot((const void *)0xE194E4, 1, 0x80);
		}
		int32_t c = n->c;
		if (c < 12)
		{
			if (c == 1)
			{
				BdPlaySE(0xE1964C, 0, 0x80);
				CharacterLoad(0x2E1, MB() + 0x14800);
			}
			else if (c == 6) SpawnGrids();
			else if (c == 10) LinkTask(ORIG_DebrisTask);
		}
		else if ((c -= 12) < 0x1C) // 12..39
		{
			if (c == 1)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0x14800);
				CharacterLoad(0x2E2, MB());
			}
			else if (c == 10)
			{
				if (LoadState() < 0) return 0;
				CharacterLoad(0x2E3, 0xEECC2C);
			}
		}
		else if ((c -= 0x1C) < 0x17) // 40..62
		{
			if (c == 0x10) Stream();
			else if (c == 0) SpawnIntro();
			else if (c == 0xC) LinkTask(ORIG_CreatureTask);
		}
		else if ((c -= 0x17) >= 0x1C) // >= 91
		{
			c -= 0x1C;
			if (c < 0x19) // 91..115
			{
				if (c == 0x14) BdPlaySE(0xE19650, 0, 0x80);
			}
			else if ((c -= 0x19) < 0x23) // 116..150
			{
				if (c == 0x11) BdTransSummonStream(0xEECC2C, STREAM_STATE);
				else if (c == 0x16) Stream();
				else if (c == 1)
				{
					if (LoadState() < 0) return 0;
					CharacterLoad(0x2E4, MB() + 0x14800);
				}
				else if (c == 0xB)
				{
					if (LoadState() < 0) return 0;
					QueueTIMUpload(MB() + 0x14800);
				}
				else if (c == 0x19) CharacterLoad(0x2E5, SummonData());
			}
			else if ((c -= 0x23) < 0x14) // 151..170
			{
				if (c == 0x13) QueueChainTransformation(Entity(CasterSlot()), 0x34);
				else if (c == 0xC)
				{
					if (LoadState() < 0) return 0;
					BdTransSummonStream(SummonData(), STREAM_STATE);
				}
			}
			else if ((c -= 0x14) < 0xC) // 171..182
			{
				if (c == 0) BdPlaySE(0xE19654, 0, 0x80);
			}
			else if ((c -= 0xC) < 0xC) {} // 183..194
			else if ((c -= 0xC) < 0x17) // 195..217
			{
				if (c == 3) Stream();
				else if (c == 0) CharacterLoad(0x2E6, MB() + 0x14800);
			}
			else if ((c -= 0x17) < 0x22) // 218..251
			{
				if (c == 0)
				{
					QueueChainTransformation(Entity(CasterSlot()), 0x35);
					if (LoadState() < 0) return 0;
					CharacterLoad(0x2E7, SummonData());
				}
			}
			else if ((c -= 0x22) < 0x46) // 252..321
			{
				if (c == 0)
				{
					BdPlaySE(0xE19658, 0, 0x80);
					if (LoadState() < 0) return 0;
					BdTransSummonStream(SummonData(), STREAM_STATE);
					CharacterLoad(0x2E8, var<uint32_t>(0x24F22E4));
				}
			}
			else if ((c -= 0x46) < 0x3C) // 322..381
			{
				if (c == 0)
				{
					BdPlaySE(0xE1965C, 0, 0x80);
					QueueTIMUpload(MB() + 0x14800);
					QueueChainTransformation(Entity(CasterSlot()), 1);
					SpawnGrids2();
				}
				else if (c == 0x1E)
				{
					Stream();
					SpawnWalls();
				}
				else if (c == 1)
				{
					LinkTask(ORIG_GhostTask);
					LinkTask(ORIG_SwordTask);
				}
				else if (c == 0x1B) CameraShake(2, 1, 2, 0xFF);
			}
			else if ((c -= 0x3C) < 0x14) // 382..401
			{
				if (c == 0)
				{
					BdPlaySE(0xE19660, 0, 0x80);
					LinkTask(ORIG_FadeATask);
					LinkTask(ORIG_FadeBTask);
					LinkTask(ORIG_TintTask);
					ShowParty();
				}
			}
			else if ((c -= 0x14) < 8) // 402..409
			{
				if (c == 5)
				{
					if (VoiceSlotBusy(VoiceSlot())) ReleaseVoiceSlot(VoiceSlot());
				}
			}
		}
		// screen flash in over 0..5, party shown at 6, hidden with a flash at 40, flash out 354..369
		int16_t cc = n->c;
		if (cc < 6) SetScreenFlash((uint32_t)(682 * (int32_t)cc), 0);
		else if (cc == 6) ShowParty();
		else if (cc == 0x28)
		{
			HideParty();
			SetScreenFlash(0xA00, 0);
		}
		else if (cc >= 0x162 && cc < 0x172) SetScreenFlash((uint32_t)((0x172 - (int32_t)cc) * 160), 0);
		if (n->c == 0x198)
		{
			uint8_t *t = *(uint8_t **)(CastCtx() + 4);
			ApplyActionResultToTargets(*(uint32_t *)(t + 8), t[0x10]);
		}
		n->c++;
		if (n->c < 0x19A) return 0;
		LinkTask(ORIG_EndTask);
		HideParty();
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// End of effect (0x62CEA0, timeline end): starts the anim-seq wait at tick 1, ends when
	// +0x0E is set. No pause test (as the original).
	// ------------------------------------------------------------------
	static uint32_t __cdecl EndTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 1) WaitAnimSeq(MB() + 0x14800, &n->e);
		uint16_t done = (uint16_t)n->e;
		n->c++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ground grids (0x62B080 / 0x62B4F0, timeline tick 6), 54 ticks, and (0x62BF40 / 0x62C070,
	// timeline tick 322), 60 / 30 ticks: a plane of the grid renderer MAG_326_sub_62B1C0 in world
	// space (camera only), scrolling, fading in and out.
	// ------------------------------------------------------------------
	static int32_t GridAFade(int32_t c) { return c < 8 ? c * 8 : (c >= 0x2E ? (0x36 - c) * 8 : 0x40); }
	static int32_t GridBFade(int32_t c) { return c < 8 ? c * 8 : (c >= 0x26 ? (0x36 - c) * 4 : 0x40); }
	// 0x62BF40: scroll (c, -c) up to 30, then (30, -30) and the grey level drops to 0x20
	static int32_t GridCU(int32_t c) { return c < 0x1E ? c : 0x1E; }
	static int32_t GridCFade(int32_t c) { return c < 0x1E ? 0x40 : 0x20; }

	static void GridDraw(const Node24 *n, int32_t w, int32_t count, int32_t u, int32_t v, int32_t fade)
	{
		int16_t ang[4] = { 0, n->a18, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->p10;
		m.t[2] = n->p14;
		m.t[1] = n->p12;
		int32_t sc[3] = { n->s1C, n->s1C, n->s1C };
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x78);
		int32_t *hd = (int32_t *)h;
		hd[1] = 0x20;
		hd[2] = count;
		hd[0] = w;
		hd[3] = u;
		hd[4] = v;
		*(uint32_t *)(h + 0x74) = Rgb(fade);
		GridRender(h, OT(), 2);
		FieldFree(0x78);
	}

	static uint32_t __cdecl GridATask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		GridDraw(n, 0x100, 0x20, c, c * 2, GridAFade(c));
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x36 ? TASK_END : 0;
	}

	static uint32_t __cdecl GridBTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		GridDraw(n, 0x200, 0x10, c * 2, c * 2, GridBFade(c));
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x36 ? TASK_END : 0;
	}

	static uint32_t __cdecl GridCTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		int32_t u = GridCU(c);
		GridDraw(n, 0x100, 0x20, u, -u, GridCFade(c));
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x3C ? TASK_END : 0;
	}

	static uint32_t __cdecl GridDTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		GridDraw(n, 0x200, 0x10, c * 2, shl32(-c, 1), 0x40);
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Lit flipbook sprites (pools A/B/C): the record position goes through the GTE light matrix
	// (+BK = the matrix translation), the result is the sprite's translation (from IR, or from
	// MAC for the blood), the rotation a uniform scale.
	// ------------------------------------------------------------------
	static void LitSprite(uint8_t *h, const int16_t *pos, int16_t size, int16_t frame, bool mac = false)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int16_t *)(h + 4) = frame;
		if (mac) GteTransFromMAC();
		else GteTransFromIR();
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	// update rules of the records (deterministic part, used to predict the next drawn state)
	enum Integrator { INT_NONE, INT_FALL, INT_DRAG4, INT_GROW5_DRAG8, INT_XZ_DRAG8, INT_GROW6_DRAG8, INT_BLOOD };
	static void Integrate(int kind, Rec *r)
	{
		switch (kind)
		{
		case INT_FALL:
			r->y = (int16_t)(r->y + r->vy);
			r->vy = (int16_t)(r->vy + (int16_t)(r->vy >> 5));
			break;
		case INT_DRAG4:
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 2));
			r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 2));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 2));
			break;
		case INT_GROW5_DRAG8:
		case INT_GROW6_DRAG8:
			r->size = (int16_t)(r->size + (int16_t)(r->size >> (kind == INT_GROW5_DRAG8 ? 5 : 6)));
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			break;
		case INT_XZ_DRAG8:
			r->x = (int16_t)(r->x + r->vx);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			break;
		case INT_BLOOD: // 0x630D30: grows 1/32, x/z damped 1/8, falling speed grows 1/64
			r->size = (int16_t)(r->size + (int16_t)(r->size >> 5));
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vy = (int16_t)(r->vy + (int16_t)(r->vy >> 6));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			break;
		default:
			break;
		}
	}

	// ------------------------------------------------------------------
	// Debris (0x62B630, timeline tick 10): pool A (mask 1) falling rocks drawn in world space (the
	// camera as light matrix, flipbook 0xE10C80 frame 0, sequence scale 2 * size + 0x1200); on
	// landing (y >= 0) a rock becomes a pool B (mask 1) splash (flipbook 0xE10CA8 on a tilted
	// plane at the landing point). Spawns 1..30 rocks per tick while the counter <= 150; ends on
	// the first tick (from 4) where no record was left.
	// ------------------------------------------------------------------
	static void DebrisDrawOne(uint8_t *h, const int16_t *pos, int16_t size)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int32_t *)(h + 0x10) = (int32_t)size * 2 + 0x1200;
		GteTransFromIR();
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static void SplashDrawOne(uint8_t *h, uint8_t *s, const Rec *r)
	{
		memcpy(s + 0x28, s + 8, 0x20);
		*(int32_t *)(s + 0x3C) = r->x;
		*(int32_t *)(s + 0x40) = r->y;
		*(int32_t *)(s + 0x44) = r->z;
		*(int32_t *)(s + 0x50) = r->size;
		*(int32_t *)(s + 0x4C) = r->size;
		*(int32_t *)(s + 0x48) = r->size;
		Scale3DMatrix((Mat4x3 *)(s + 0x28), (const int32_t *)(s + 0x48));
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(s + 0x28), (Mat4x3 *)(s + 0x28));
		GteSetRotMatrixCtrl((const Mat4x3 *)(s + 0x28));
		GteSetTransVectorCtrl((const Mat4x3 *)(s + 0x28));
		*(int16_t *)(h + 4) = r->age;
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static void DebrisSetupA(uint8_t *h, uint8_t *s)
	{
		*(uint32_t *)(h + 0x14) = 0x1000;
		*(uint32_t *)(h + 0xC) = 0x1000;
		memcpy(s + 8, &Camera(), 0x20);
		*(uint32_t *)h = 0xE10C80;
		*(int16_t *)(h + 4) = 0;
		*(int16_t *)(h + 0x24) = 2;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static void DebrisSetupB(uint8_t *h, uint8_t *s)
	{
		*(uint32_t *)h = 0xE10CA8;
		*(int16_t *)(h + 0x24) = 0x200;
		*(int16_t *)(s + 0) = 0x400;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
	}

	static uint32_t __cdecl DebrisTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x68);
		int32_t count = 0;
		DebrisSetupA(h, s);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecOf(PoolA(), i);
			if (!(r->alive & 1)) continue;
			DebrisDrawOne(h, &r->x, r->size);
			PoolMemo *pm = MemoRecord(0, i, n, r);
			if (Paused()) continue;
			r->y = (int16_t)(r->y + r->vy);
			r->vy = (int16_t)(r->vy + (int16_t)(r->vy >> 5));
			if (r->y < 0)
			{
				count++;
				continue;
			}
			r->alive = 0;
			if (pm) pm->died = true;
			int j = FreeRecord(PoolB());
			if (j < 0) continue;
			Rec *b = RecOf(PoolB(), j);
			b->alive = 1;
			b->age = 0;
			int32_t rr = CrtRand() % 0x400;
			b->size = (int16_t)(rr + r->size + 0x400);
			b->x = r->x;
			b->y = 0;
			b->z = r->z;
		}
		DebrisSetupB(h, s);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecOf(PoolB(), i);
			if (!(r->alive & 1)) continue;
			SplashDrawOne(h, s, r);
			PoolMemo *pm = MemoRecord(1, i, n, r);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				if (pm) pm->died = true;
			}
			else count++;
		}
		FieldFree(0x68);
		FieldFree(0xB4);
		if (Paused()) return 0;
		FieldAlloc(0x68); // allocated and never used (vanilla)
		int16_t c = n->c;
		if (c >= 0 && c <= 0x96)
		{
			int32_t k = CrtRand() % 6;
			k = k + (int32_t)c / 4 + 1;
			if (k > 0x1E) k = 0x1E;
			for (int m = 0; m < k; m++)
			{
				int i = FreeRecord(PoolA());
				if (i < 0) break;
				Rec *r = RecOf(PoolA(), i);
				r->alive = 1;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x480 + 0x300);
				r->x = (int16_t)(CrtRand() % 0x1770 - 0xBB8);
				r->y = (int16_t)(-4000 - CrtRand() % 1000);
				r->z = (int16_t)(CrtRand() % 10000 - 5000);
				r->vy = (int16_t)(CrtRand() % 0x226 + 0xB4);
			}
		}
		FieldFree(0x68);
		n->c++;
		if (n->c < 4 || count != 0) return 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Texture warp (0x62BB40, timeline tick 40), 27 ticks, draws nothing: rebuilds the 128 x 128
	// 8-bit texture at *(0x24F22E4) + 0x4000 (0xEF0C30, exe data) from the one at 0xEECC30 with a
	// sine ripple and queues its VRAM upload (rect 0xE196B8) - every tick, paused or not.
	// ------------------------------------------------------------------
	static uint32_t __cdecl WarpTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *src = var<uint8_t *>(0x24F22E4);
		uint8_t *dst = src + 0x4000;
		int32_t a1 = n->a18, a2 = n->a18;
		uint8_t *row = dst;
		int32_t y = 0;
		for (int32_t acc = 0; acc < 0x40000; acc += 0x800, y++, row += 0x80)
		{
			int32_t s1 = ComputeSin(a1);
			int32_t amp = shl32(-mul32(s1, 3), 2) >> 12;
			int32_t s2 = ComputeSin(acc / 128);
			a1 += 0x20;
			int32_t off = mul32(s2, amp) >> 12;
			int32_t s3 = ComputeSin(a2);
			int32_t sy = (y - (shl32(s3, 1) >> 12)) & 0x7F;
			a2 += 0x70;
			int32_t base = (0x7F - sy) << 7;
			for (int32_t x = 0; x < 0x80; x++) row[x] = src[((x - off) & 0x7F) + base];
		}
		QueueVramUpload(0xE196B8, dst);
		if (Paused()) return 0;
		n->a18 = (int16_t)(n->a18 + 0x20);
		n->c++;
		return n->c > 0x1A ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Glow (0x62BC70, timeline tick 40), 26 ticks: MAG_066_sub_650720 model 0xE163F8 (colour
	// 0x808080) at the effect origin, scale 0xB00, fade value 0x400 (mode 0xF3), texture scroll
	// +-counter.
	// ------------------------------------------------------------------
	static void GlowDraw(const Mat4x3 *frame, const Node24 *n, int32_t c)
	{
		int16_t ang[4] = { 0, 0, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[1] = n->p12;
		m.t[0] = n->p10;
		int32_t sc[3] = { n->s1C, n->s1C, n->s1C };
		m.t[2] = n->p14;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x90);
		*(uint32_t *)h = 0xE163F8;
		*(uint16_t *)(h + 0x1C) = 0x80;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(int32_t *)(h + 0x14) = c;
		*(int32_t *)(h + 0xC) = 0x400;
		*(int32_t *)(h + 0x10) = -c;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x20) = 0xF3;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint32_t *)(h + 0x24) = 0x808080;
		Cursor() = Mag066Draw(h, OT(), 2, Cursor());
		FieldFree(0x90);
	}

	static uint32_t __cdecl GlowTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		GlowDraw(&Frame(), n, n->c);
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x1A ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Screen streaks (0x62BDA0 x 2, timeline tick 40): after a 1-tick delay, prim 0xE15D10 in
	// screen space (z = the projection distance word_1D8E038), scale (s1C, s20, s20), moving
	// by s1E per tick in x, 23 ticks.
	// ------------------------------------------------------------------
	static void StreakDraw(const Node24 *n, int32_t x)
	{
		int16_t ang[4] = { 0, n->a18, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = x;
		m.t[1] = n->p12;
		int32_t sc[3];
		sc[0] = n->s1C;
		m.t[2] = var<int16_t>(0x1D8E038);
		sc[2] = n->s20;
		sc[1] = n->s20;
		Scale3DMatrix(&m, sc);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = 0xE15D10;
		*(uint32_t *)(h + 0xC) = 0x800;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		Cursor() = RenderPrimModel(h, OT(), 0xE, Cursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl StreakTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (Paused()) return 0;
			n->e--;
			return 0;
		}
		StreakDraw(n, n->p10);
		MemoNode(n);
		if (Paused()) return 0;
		n->p10 = (int16_t)(n->p10 + n->s1E);
		n->c++;
		return n->c >= 0x17 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Walls (0x62C2F0 x 4, timeline tick 352), 30 ticks: prim 0xE17450 (+0x0E != 0) or 0xE16CF0 in
	// world space (camera only) at (p10 + s20, p12, p14), rotation y a18, scale s1C, fade value
	// p16 (mode 0xF3); every tick p14 += s22 and s20 += s1E / 4.
	// ------------------------------------------------------------------
	static void WallDraw(const Node24 *n, int32_t s20, int32_t z)
	{
		int16_t ang[4] = { 0, n->a18, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[1] = n->p12;
		m.t[0] = (int32_t)n->p10 + s20;
		m.t[2] = z;
		int32_t sc[3] = { n->s1C, n->s1C, n->s1C };
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = n->e != 0 ? 0xE17450 : 0xE16CF0;
		*(int32_t *)(h + 0xC) = n->p16;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl WallTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		WallDraw(n, n->s20, n->p14);
		MemoNode(n);
		if (Paused()) return 0;
		n->p14 = (int16_t)(n->p14 + n->s22);
		n->s20 = (int16_t)(n->s20 + (int16_t)((int32_t)n->s1E / 4));
		n->c++;
		return n->c >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Shock plane (0x62C430, timeline tick 352), 30 ticks: prim 0xE17BB0 in world space at the
	// node position, scale (s1C, s20, s20), mode 0x33; s1C grows by s1E, s1E by 1/32.
	// ------------------------------------------------------------------
	static void ShockDraw(const Node24 *n, int32_t sx)
	{
		int16_t ang[4] = { 0, 0, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[1] = n->p12;
		m.t[0] = n->p10;
		int32_t sc[3];
		sc[2] = n->s20;
		sc[1] = n->s20;
		m.t[2] = n->p14;
		sc[0] = sx;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = 0xE17BB0;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl ShockTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ShockDraw(n, n->s1C);
		MemoNode(n);
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->c++;
		n->s1E = (int16_t)(v + (int16_t)(v >> 5));
		return n->c >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Battle-model states (0x9C bytes, the BattleAnimHeader layout of 187's creature):
	//   +0x00 u16 flags (bit0 = no anim loop), +0x07, +0x28 colour, +0x40 model matrix, +0x60
	//   BattleAnimHeader, +0x64 comFileData, +0x6C BattleAnimCmd, +0x78, +0x7C object mask,
	//   +0x84 model data, +0x8C.
	// ------------------------------------------------------------------

	// MAG_326_sub_62C8C0: rotation part = identity (translation and pad kept)
	static void ResetRotation(void *m)
	{
		uint32_t *d = (uint32_t *)m;
		d[3] = 0;
		d[2] = 0;
		d[1] = 0;
		d[0] = 0;
		*(uint16_t *)((uint8_t *)m + 0x10) = 0x1000;
		*(uint16_t *)((uint8_t *)m + 8) = 0x1000;
		*(uint16_t *)((uint8_t *)m + 0) = 0x1000;
	}

	// MAG_326_sub_62C790: bind the model data (model buffer offsets table) to the state
	static void ModelInit(uint8_t *E, uint8_t *data, uint8_t *buf, int32_t id)
	{
		*(uint16_t *)(E + 0) = 2;
		E[7] = 0;
		*(uint32_t *)(E + 0x28) = 0x808080;
		E[0x61] = 0;
		E[0x60] = 0;
		*(uint8_t **)(E + 0x68) = data + 0xC;
		*(uint8_t **)(E + 0x64) = data + 0xC;
		*(uint32_t *)(E + 0x78) = 0;
		*(uint32_t *)(E + 0x7C) = 0xFFFFFFFF;
		*(uint8_t **)(E + 0x84) = data;
		*(uint32_t *)(E + 0x8C) = 0;
		*(uint8_t **)(data + 0xC) = buf + *(uint32_t *)(buf + 4);
		*(uint8_t **)(data + 0x10) = buf + *(uint32_t *)(buf + 8);
		*(uint8_t **)(data + 0x14) = buf + *(uint32_t *)(buf + 0xC);
		*(uint16_t *)(data + 2) = (uint16_t)id;
		*(uint8_t **)(data + 0x30) = buf + *(uint32_t *)(buf + 0x10);
	}

	// au_re_Battle_ReadAnimation_3 0x62C800
	static void SetAnim(uint8_t *E, int32_t id) { PreReadAnimation(E + 0x60, E + 0x6C, id); }

	// MAG_326_sub_62CBC0 (one frame, re-armed on completion unless flags bit0)
	static void AdvanceModelAnimLoop(uint8_t *E)
	{
		if (ReadAnimation(E + 0x60, E + 0x6C) == 1 && !(E[0] & 1))
			PreReadAnimation(E + 0x60, E + 0x6C, E[0x6C]);
	}

	// MAG_326_sub_62C820 (scratch = RenderGeometry's vertex scratch, dword 0xE196B0)
	static void DrawModel(uint8_t *E, const Mat4x3 *frame, uint32_t scratch)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		ComposeAffineTransform(frame, (const Mat4x3 *)(E + 0x40), (Mat4x3 *)h);
		ComputeBonesWorldMatrices(E + 0x60, h);
		*(uint32_t *)(h + 0x30) = var<uint32_t>(0x1D969A8);
		*(uint32_t *)(h + 0x24) = scratch;
		*(uint16_t *)(h + 0x34) = 0;
		*(uint16_t *)(h + 0x36) = 0;
		*(uint32_t *)(h + 0x3C) = *(uint32_t *)(E + 0x28);
		*(uint32_t *)(h + 0x48) = 0;
		*(uint16_t *)(h + 0x44) = 0;
		*(uint16_t *)(h + 0x38) = 0x140;
		*(uint16_t *)(h + 0x3A) = 0xD8;
		*(uint32_t *)(h + 0x40) = *(uint32_t *)(E + 0x7C);
		Cursor() = RenderGeometry(*(void **)(E + 0x64), h + 0x20, OT(), 4, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
		FieldFree(0x4C);
	}

	static uint32_t Scratch() { return var<uint32_t>(0xE196B0); }

	// held-frame memo of a model: the drawn skeleton (pose values + local matrices), the reader
	// command, the model matrix and the colour it was drawn with
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct ModelMemo
	{
		uint32_t tick;
		bool midpoint;   // the next draw shows the pose one frame ahead of this one
		bool hold_mat;   // the next tick cuts the model matrix
		int32_t next_dz; // the next tick moves t.z by this before drawing (creature 144..152)
		Mat4x3 mat;
		uint32_t color;
		uint32_t skel_size;
		uint8_t cmd[8];
		uint8_t skel[SKEL_MAX];
	};
	static ModelMemo g_creature = { 0xFFFFFFFF };
	static ModelMemo g_fade_a = { 0xFFFFFFFF };
	static ModelMemo g_fade_b = { 0xFFFFFFFF };

	static uint8_t *Skeleton(uint8_t *E)
	{
		uint8_t *com = *(uint8_t **)(E + 0x64); // BattleAnimHeader.comFileData
		return com ? *(uint8_t **)com : nullptr;
	}

	static void MemoTake(ModelMemo &m, uint8_t *E)
	{
		uint8_t *sk = Skeleton(E);
		m.tick = 0xFFFFFFFF;
		if (!sk) return;
		uint32_t size = 16 + 48 * (uint32_t)sk[0];
		if (size > SKEL_MAX) return;
		m.skel_size = size;
		memcpy(m.skel, sk, size);
		memcpy(m.cmd, E + 0x6C, 8);
		m.mat = *(const Mat4x3 *)(E + 0x40);
		m.color = *(uint32_t *)(E + 0x28);
		m.midpoint = false;
		m.hold_mat = true;
		m.next_dz = 0;
		m.tick = g_real_tick;
	}

	// ------------------------------------------------------------------
	// Spawners called by the creature
	// ------------------------------------------------------------------
	// MAG_326_sub_62E810 (creature tick 3): two expanding rings (prim 0xE18150)
	static void SpawnRings()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_RingATask);
		n->c = 0;
		n->p10 = 0;
		n->p12 = 0;
		n->p14 = 0x28;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->s1C = 0xE00;
		n->s22 = 0x555;
		n->s20 = 0x555;
		n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_RingBTask);
		n->c = 0;
		n->p10 = 0;
		n->p12 = 0;
		n->p14 = 0x28;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->s1E = 0x500;
		n->s1C = 0x500;
		n->s20 = 0x200;
	}

	// MAG_326_sub_62F1C0 (creature tick 52): creature fade-in, screen flash, morph flash
	static void SpawnFadeIn()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_FadeInTask);
		n->c = 0;
		n->e = 0;
		n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_FlashTask);
		n->c = 0;
		n->e = 0xA10;
		n->s1C = 0xFF;
		n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_MorphTask);
		n->c = 0;
		n->s1C = 0x400;
		n->s20 = 0x400;
	}

	// au_re_BdLinkTask_70 0x62F710 (creature tick 82): sword sparks, history 0x24F0C88 cleared
	static void SpawnSparks()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SparkTask);
		n->c = 0;
		for (uint32_t a = 0x24F0C88; a < 0x24F0CC4; a += 0x14) *(uint32_t *)a = 0;
	}

	// MAG_326_sub_62FC20 (creature tick 106): two spark tasks on bones 0x1E / 0x34, histories cleared
	static void SpawnSparks2()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_Spark2Task);
		n->c = 0;
		n->e = 0x1E;
		n->a18 = 1;
		n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_Spark2Task);
		n->c = 0;
		n->e = 0x34;
		n->a18 = 2;
		for (uint32_t a = TRAIL_RECS; a < 0x24F2248; a += 0x14) *(uint32_t *)a = 0;
		for (uint32_t k = 0; k < 0x3C; k += 0x14)
		{
			*(uint32_t *)(0x24F0C88 + k) = 0;
			*(uint32_t *)(0x24F0DF0 + k) = 0;
		}
	}

	// au_re_BdLinkTask_71 0x630400 (creature tick 146): the blade trail, its history cleared
	static void SpawnTrail()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_TrailTask);
		n->c = 0;
		for (uint32_t a = TRAIL_RECS; a < 0x24F2248; a += 0x14) *(uint32_t *)a = 0;
	}

	// MAG_326_sub_630810 (creature tick 108): impact burst at the target's default effect position
	static void SpawnBurstA()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_BurstATask);
		n->c = 0;
		GetDefaultEffectPosition(Entity(CasterSlot()), &n->p10);
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->s1C = 0x1000;
		n->s20 = 0x1E00;
	}

	// MAG_326_sub_630A70 (creature tick 132): burst at the camera-script look point
	static void SpawnBurstB()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_BurstBTask);
		n->c = 0;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->s1C = 0x600;
		n->s20 = 0xA00;
	}

	// MAG_326_sub_630CB0 (creature tick 246): two blood bursts (pool C masks 4 / 8) at the
	// creature's bone 0xF1, pushed 0x320 to each side, y 0
	static void SpawnBlood()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_BloodTask);
		n->c = 0;
		n->e = 4;
		GetEffectSpawnPosition(Creature(), 0xF1, 0, &n->p10);
		n->p10 = 0x320;
		n->p12 = 0;
		Node24 *m = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_BloodTask);
		int16_t x = n->p10, z = n->p14;
		m->c = 0;
		m->p12 = 0;
		m->e = 8;
		m->p10 = (int16_t)-x;
		m->p14 = z;
	}

	// MAG_326_sub_631150 (creature tick 198): three cut lines (0x6312F0, table 0xE19710 entries
	// 0-2: x, y, delay, model) with two echoes each (0x6321A0), one long cut (0x632350, entry 3)
	// with four echoes (0x632500)
	static void SpawnSlashes()
	{
		for (uint32_t e = 0xE19710; e < 0xE19728; e += 8)
		{
			const int16_t *t = (const int16_t *)e;
			Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SlashATask);
			n->e = t[3];
			int16_t z = t[2];
			n->p10 = t[0];
			n->c = 0;
			n->p12 = t[1];
			n->p14 = z;
			n->p16 = 0;
			n->s1C = 0x4000;
			n->s1E = 0x2000;
			n->s20 = (int16_t)(0x28 - z);
			n->s22 = z;
			for (int i = 0; i < 2; i++)
			{
				Node24 *u = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SlashBTask);
				u->e = n->e;
				*(uint32_t *)&u->p10 = *(const uint32_t *)&n->p10;
				*(uint32_t *)&u->p14 = *(const uint32_t *)&n->p14;
				u->s1C = n->s1C;
				u->s1E = n->s1E;
				int16_t cc = (int16_t)(u->p14 + i);
				u->c = 0;
				u->p14 = cc;
				u->p16 = 0x800;
				u->s20 = (int16_t)(0x28 - cc);
				u->s22 = cc;
			}
		}
		const int16_t *t = (const int16_t *)0xE19728;
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SlashCTask);
		n->e = t[3];
		int16_t z = t[2];
		n->p10 = t[0];
		n->c = 0;
		n->p12 = t[1];
		n->p14 = z;
		n->p16 = 0;
		n->s1C = 0x2DC0;
		n->s1E = 0xF40;
		n->s20 = (int16_t)(0x28 - z);
		n->s22 = z;
		for (int i = 0; i < 4; i++)
		{
			Node24 *u = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SlashDTask);
			u->e = n->e;
			*(uint32_t *)&u->p10 = *(const uint32_t *)&n->p10;
			*(uint32_t *)&u->p14 = *(const uint32_t *)&n->p14;
			u->s1C = n->s1C;
			u->s1E = n->s1E;
			int16_t cc = (int16_t)(u->p14 + i);
			u->c = 0;
			u->p14 = cc;
			u->p16 = 0x800;
			u->s20 = (int16_t)(0x28 - cc);
			u->s22 = cc;
		}
	}

	// entity flag bit 3 of the caster slot and the slot after it (hidden from the normal pass)
	static void CasterFlags(bool hide)
	{
		uint16_t *a = (uint16_t *)Entity(CasterSlot());
		uint16_t *b = (uint16_t *)(Entity(CasterSlot()) + 0x9C);
		if (hide)
		{
			*a = (uint16_t)(*a | 8);
			*b = (uint16_t)(*b | 8);
		}
		else
		{
			*a = (uint16_t)(*a & 0xFFF7);
			*b = (uint16_t)(*b & 0xFFF7);
		}
	}

	// ------------------------------------------------------------------
	// Sliced creature draw (174..253)
	// ------------------------------------------------------------------
	// MAG_326_sub_62D5D0: plane distance, n[3] = -(p . n) >> 12
	static void PlaneDistance(const int16_t *p, int16_t *nrm)
	{
		int32_t d = mul32(p[2], nrm[2]) + mul32(p[1], nrm[1]);
		d = d + mul32(p[0], nrm[0]);
		nrm[3] = (int16_t)-(d >> 12);
	}

	// the cut plane: normal (-1, 0, 0) through bone 0x21 (half way along it) moved 13 in x
	static void SliceSetup(uint8_t *h, uint8_t *E)
	{
		*(int16_t *)(h + 0xA2) = 0;
		*(int16_t *)(h + 0xA4) = 0;
		*(uint16_t *)(h + 0xA0) = 0xF000;
		BonePosition(E, 0x21, 0x800, (int16_t *)(h + 0x40));
		*(int16_t *)(h + 0x40) = (int16_t)(*(int16_t *)(h + 0x40) + 0xD);
		PlaneDistance((const int16_t *)(h + 0x40), (int16_t *)(h + 0xA0));
	}

	// MAG_326_sub_62D610: shadow, both root matrices, the clipped model, the bone matrices
	static void CreatureSliceDraw(uint8_t *h, uint8_t *E, const Mat4x3 *m1, const Mat4x3 *m2, const Mat4x3 *frame, uint32_t scratch)
	{
		if (!(E[0] & 0x20))
			Cursor() = DrawShadow(E, var<uint32_t>(0x1D8E04C) + 0x4064, 0x10, Cursor());
		ComposeAffineTransform(frame, m1, (Mat4x3 *)(h + 0x190));
		ComposeAffineTransform(frame, m2, (Mat4x3 *)(h + 0x1B0));
		*(uint32_t *)(h + 4) = scratch;
		*(uint32_t *)(h + 0x18) = *(uint32_t *)(E + 0x28);
		*(uint32_t *)(h + 0x10) = *(uint32_t *)(E + 0x7C);
		Cursor() = SliceRender(*(void **)(E + 0x64), h, OT(), 2, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
	}

	// phase A (174..199): the halves drift apart in x by 8c' - 8 (c' = c - 174)
	static void SliceADraw(uint8_t *E, int32_t d, const Mat4x3 *frame, uint32_t scratch)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x1D0);
		SliceSetup(h, E);
		int32_t t0 = CreatureMat().t[0];
		Mat4x3 ma = CreatureMat(), mb = CreatureMat();
		ma.t[0] = (int32_t)((uint32_t)t0 - (uint32_t)d);
		mb.t[0] = (int32_t)((uint32_t)t0 + (uint32_t)d);
		CreatureSliceDraw(h, E, &mb, &ma, frame, scratch);
		FieldFree(0x1D0);
	}

	// dim to black by w (GTE depth cue of a zero colour towards the far colour (r, g, b))
	static void DepthCue(int32_t w, int32_t r, int32_t g, int32_t b, void *out)
	{
		GteSetFarColor(r, g, b);
		// the original zeroes a dword of its own argument area and passes its address: a zero RGBC
		uint32_t zero = 0;
		GteLoadRGBC(&zero);
		GteSetIR0(w);
		GteDPCS();
		GteStoreRGB2(out);
	}

	static int32_t SliceBFadeW(int32_t c) { return 0x1000 - ComputeSin(shl32(c - 0x2E, 10) / 6); }

	// phase B (200..253): the halves at +-0x320, tilted by +-a about z; from c' = 46 the colour
	// fades (colour written to E+0x28 unless out is given)
	static void SliceBDraw(uint8_t *E, int32_t c, int16_t a, const Mat4x3 *frame, uint32_t scratch, bool real, uint32_t held_color)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x1D0);
		SliceSetup(h, E);
		Mat4x3 ma = CreatureMat(), mb = CreatureMat();
		ma.t[0] = (int32_t)0xFFFFFCE0;
		mb.t[0] = 0x320;
		int16_t ang[4] = { 0, 0, a, 0 };
		Mat4x3 mc;
		ComposeZYXRotationMatrix(ang, &mc);
		GteMatrixMultiply(&mc, &mb);
		ang[2] = (int16_t)-a;
		ComposeZYXRotationMatrix(ang, &mc);
		GteMatrixMultiply(&mc, &ma);
		if (real)
		{
			if (c >= 0x2E)
			{
				int32_t w = SliceBFadeW(c);
				DepthCue(w, 0x80, 0x80, 0x80, E + 0x28);
				E[0x2B] = 2;
			}
		}
		else *(uint32_t *)(E + 0x28) = held_color;
		CreatureSliceDraw(h, E, &mb, &ma, frame, scratch);
		FieldFree(0x1D0);
	}

	// ------------------------------------------------------------------
	// Creature (0x62CEE0, timeline tick 52), 256 ticks. State E = 0x24F1268 (0x9C bytes).
	//   0-11    anim 0 (scale 2), draw then advance; smoke, rings, dust at 3
	//   12-39   anim 1 (scale 1), advance then draw; mist at 21
	//   40-52   anim 2 (read twice at 40), advance then draw; fade-in/flash/morph at 52
	//   53-64   draw only
	//   65-107  anim 3, draw then advance; sparks 82, sparks 2 at 106
	//   108-119 anim 4 + impact burst at 108, draw then advance
	//   120-131 anim 4 again at 120, draw only
	//   132-142 burst 2 at 132; draw, advance, spawn position (bone 0) -> node +0x10
	//   143-173 anim 5 at 143 (t.x = 0xD2, t.z = node z + 0x4B0, camera shake); t.z -= 0x1C2
	//           before the draw at 143..152, trail at 146, object mask 3 at 153 (the blade ends);
	//           draw then advance
	//   174     draw only
	//   175-199 cut, phase A (draw only); cut lines at 198
	//   200-253 cut, phase B: caster slot entities hidden (flag 8) until 229, shown and the cut
	//           angle +0x20 += 0x12 from 230, camera shake 222, blood 246, fade 246..
	//   254-255 nothing drawn
	// Node: +0x10 spawn position (132-142), +0x20 cut angle.
	// ------------------------------------------------------------------
	struct SliceMemo { uint32_t tick; bool phase_b; int32_t cc; int16_t angle; uint32_t color; };
	static SliceMemo g_slice = { 0xFFFFFFFF };

	// reader steps between the draw at tick c and the one at c + 1 (see the table above)
	static bool AdvanceAfterDraw(int32_t c) { return (c >= 0 && c < 12) || (c >= 65 && c < 120) || (c >= 132 && c < 174); }
	static bool AdvanceBeforeDraw(int32_t c) { return c >= 12 && c < 53; }
	static bool SetAnimBeforeDraw(int32_t c) { return c == 0 || c == 12 || c == 40 || c == 65 || c == 108 || c == 120 || c == 143; }

	static void Draw(uint8_t *E)
	{
		DrawModel(E, &Frame(), Scratch());
		MemoTake(g_creature, E);
	}

	static uint32_t __cdecl CreatureTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *E = Creature();
		if (n->c == 0)
		{
			ResetRotation((void *)0x24F12A8);
			var<uint16_t>(0x24F12B8) = 0x2000;
			var<uint16_t>(0x24F12B0) = 0x2000;
			var<uint16_t>(0x24F12A8) = 0x2000;
			var<int32_t>(0x24F12C0) = 0;
			var<int32_t>(0x24F12BC) = 0;
			var<int32_t>(0x24F12C4) = 0x708;
			ModelInit(E, (uint8_t *)0x24F22E8, (uint8_t *)MB(), 0xF);
			E[0] |= 1;
			LinkTask(ORIG_BladeTask);
			var<uint32_t>(0x24F12E4) = 7;
		}
		g_slice.tick = 0xFFFFFFFF;
		int32_t c = n->c;
		if (c < 12)
		{
			if (Paused()) goto draw_advance;
			if (c == 0) SetAnim(E, 0);
			else if (c == 3)
			{
				LinkTask(ORIG_SmokeTask);
				SpawnRings();
				LinkTask(ORIG_DustTask);
			}
			goto draw_advance;
		}
		if ((c -= 12) < 0x1C) // 12..39
		{
			var<uint16_t>(0x24F12B8) = 0x1000;
			var<uint16_t>(0x24F12B0) = 0x1000;
			var<uint16_t>(0x24F12A8) = 0x1000;
			if (Paused()) goto draw;
			if (c == 0) SetAnim(E, 1);
			else if (c == 9) LinkTask(ORIG_MistTask);
			goto advance_draw;
		}
		if ((c -= 0x1C) < 0xD) // 40..52
		{
			if (Paused()) goto draw;
			if (c == 0)
			{
				SetAnim(E, 2);
				AdvanceModelAnimLoop(E);
			}
			else if (c == 0xC) SpawnFadeIn();
			goto advance_draw;
		}
		if ((c -= 0xD) < 0xC) goto draw; // 53..64
		if ((c -= 0xC) < 0x2B)            // 65..107
		{
			if (Paused()) goto draw_advance;
			if (c == 0) SetAnim(E, 3);
			else if (c == 0x11) SpawnSparks();
			else if (c == 0x29) SpawnSparks2();
			goto draw_advance;
		}
		if ((c -= 0x2B) < 0xC) // 108..119
		{
			if (Paused()) goto draw_advance;
			if (c == 0)
			{
				SetAnim(E, 4);
				SpawnBurstA();
			}
			goto draw_advance;
		}
		if ((c -= 0xC) < 0xC) // 120..131
		{
			if (Paused()) goto draw;
			if (c == 0) SetAnim(E, 4);
			goto draw;
		}
		if ((c -= 0xC) < 0xB) // 132..142
		{
			if (!Paused() && c == 0) SpawnBurstB();
			Draw(E);
			if (Paused()) goto end;
			AdvanceModelAnimLoop(E);
			GetEffectSpawnPosition(E, 0, 0, &n->p10);
			goto end;
		}
		if ((c -= 0xB) < 0x1F) // 143..173
		{
			if (Paused()) goto draw_advance;
			if (c == 0)
			{
				var<int32_t>(0x24F12BC) = 0xD2;
				var<int32_t>(0x24F12C4) = (int32_t)n->p14 + 0x4B0;
				SetAnim(E, 5);
				CameraShake(4, 6, 0xC, 0xFF);
				var<int32_t>(0x24F12C4) = (int32_t)((uint32_t)var<int32_t>(0x24F12C4) - 0x1C2u);
			}
			else if (c == 0xA) var<uint32_t>(0x24F12E4) = 3;
			else if (c < 0xA)
			{
				var<int32_t>(0x24F12C4) = (int32_t)((uint32_t)var<int32_t>(0x24F12C4) - 0x1C2u);
				if (c == 3) SpawnTrail();
			}
			goto draw_advance;
		}
		if ((c -= 0x1F) < 0x1A) // 174..199
		{
			if (!Paused() && c == 0x18) SpawnSlashes();
			else if (c < 1) goto draw;
			SliceADraw(E, c * 8 - 8, &Frame(), Scratch());
			n->s20 = 0x80;
			g_slice.tick = g_real_tick;
			g_slice.phase_b = false;
			g_slice.cc = c;
			g_slice.angle = 0x80;
			g_slice.color = *(uint32_t *)(E + 0x28);
			goto end;
		}
		if ((c -= 0x1A) < 0x36) // 200..253
		{
			if (!Paused())
			{
				if (c == 0x16)
				{
					CameraShake(2, 1, 4, 0xFF);
					CasterFlags(true);
				}
				else if (c == 0x2E || c >= 0x1E)
				{
					if (c == 0x2E) SpawnBlood();
					CasterFlags(false);
					n->s20 = (int16_t)(n->s20 + 0x12);
				}
				else CasterFlags(true);
			}
			SliceBDraw(E, c, n->s20, &Frame(), Scratch(), true, 0);
			g_slice.tick = g_real_tick;
			g_slice.phase_b = true;
			g_slice.cc = c;
			g_slice.angle = n->s20;
			g_slice.color = *(uint32_t *)(E + 0x28);
		}
		goto end;

	advance_draw:
		AdvanceModelAnimLoop(E);
	draw:
		Draw(E);
		goto end;

	draw_advance:
		Draw(E);
		if (!Paused()) AdvanceModelAnimLoop(E);

	end:
		if (Paused()) return 0;
		n->c++;
		if (g_creature.tick == g_real_tick)
		{
			int32_t drawn = n->c - 1, nc = n->c;
			int steps = (AdvanceAfterDraw(drawn) ? 1 : 0) + (AdvanceBeforeDraw(nc) ? 1 : 0);
			g_creature.midpoint = steps == 1 && !SetAnimBeforeDraw(nc) && nc <= 174;
			// the model matrix the next draw uses: the current one moved by next_dz, except on its cuts
			g_creature.hold_mat = nc == 12 || nc == 143 || nc >= 174;
			g_creature.next_dz = nc >= 144 && nc <= 152 ? -0x1C2 : 0;
		}
		if (n->c < 0x100) return 0;
		DoneFlags() |= 1;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Creature fade-in (0x62F220, creature tick 52), draws nothing: creature colour = 12c - 12
	// grey, 0x808080 at the end (11 ticks).
	// ------------------------------------------------------------------
	static uint32_t __cdecl FadeInTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (Paused()) return 0;
			n->e--;
			return 0;
		}
		int16_t c = n->c;
		int32_t v = c >= 1 ? (int32_t)c * 12 - 12 : 0;
		var<uint32_t>(0x24F1290) = Rgb(v);
		if (Paused()) return 0;
		n->c = (int16_t)(c + 1);
		if (n->c < 0xB) return 0;
		var<uint32_t>(0x24F1290) = 0x808080;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Screen flash (0x62F2B0, creature tick 52): a full-screen tile (MAG_326_sub_62CE20) at the
	// depth of a point 200 in front of the look-at point; +0x0E packs the in / hold / out lengths
	// (nibbles 0 / 1 / 2), +0x1C the level. Ends when the level runs out.
	// ------------------------------------------------------------------
	static int32_t FlashLevel(int32_t c, int32_t e, int32_t level)
	{
		int32_t a = e & 0xF;
		if (c < a) return (level / a) * c;
		c -= a;
		int32_t b = (e >> 4) & 0xF;
		if (c < b) return level;
		int32_t o = (e >> 8) & 0xF;
		c -= b;
		if (c < o) return level - (level / o) * c;
		return -1;
	}

	static int32_t FlashDepth()
	{
		int32_t d[3];
		d[0] = (int32_t)var<int16_t>(0xB8B7F8) - (int32_t)var<int16_t>(0xB8B7F0);
		d[1] = (int32_t)var<int16_t>(0xB8B7FA) - (int32_t)var<int16_t>(0xB8B7F2);
		d[2] = (int32_t)var<int16_t>(0xB8B7FC) - (int32_t)var<int16_t>(0xB8B7F4);
		NormalizeVectorToFixedPoint(d, d);
		int16_t p[4];
		memcpy(p, (const void *)0xB8B7F8, 8);
		p[0] = (int16_t)(p[0] + (int16_t)(mul32(d[0], 200) >> 12));
		p[1] = (int16_t)(p[1] + (int16_t)(mul32(d[1], 200) >> 12));
		p[2] = (int16_t)(p[2] + (int16_t)(mul32(d[2], 200) >> 12));
		GteLoadV0(p);
		GteRTPS();
		int32_t otz;
		GteReadOTZ32(&otz);
		return otz;
	}

	struct FlashMemo { int32_t level, otz; };
	static NodeMemo<FlashMemo, 8> g_flash_memo;

	static uint32_t __cdecl FlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t otz = FlashDepth();
		int32_t level = FlashLevel(n->c, n->e, n->s1C);
		if (level >= 0) ScreenTile(level, level, level, otz);
		if (FlashMemo *m = g_flash_memo.put(n)) { m->level = level; m->otz = otz; }
		if (Paused()) return 0;
		n->c++;
		return level < 0 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Morph flash (0x62F400, creature tick 52), 12 ticks: prim 0xE148E8 billboarded 100 in front
	// of the look-at point (camera space), its 126 vertices morphed from the model's own (0xE148F0)
	// to the set 0xE15668 by sin(c * 1024 / 12) into the scratch dword 0xE196B0, drawn twice;
	// fading out from tick 6 (mode 0x20F3).
	// ------------------------------------------------------------------
	static void MorphDraw(const Node24 *n, int32_t sin_arg, bool fade, int32_t fade_value, uint32_t vbuf)
	{
		int16_t ang[4] = { 0, 0, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		int32_t sc[3];
		sc[2] = n->s20;
		sc[1] = n->s20;
		sc[0] = n->s1C;
		Scale3DMatrix(&m, sc);
		int32_t d[3];
		d[0] = (int32_t)var<int16_t>(0xB8B7F8) - (int32_t)var<int16_t>(0xB8B7F0);
		d[1] = (int32_t)var<int16_t>(0xB8B7FA) - (int32_t)var<int16_t>(0xB8B7F2);
		d[2] = (int32_t)var<int16_t>(0xB8B7FC) - (int32_t)var<int16_t>(0xB8B7F4);
		NormalizeVectorToFixedPoint(d, d);
		int16_t p[4];
		memcpy(p, (const void *)0xB8B7F8, 8);
		p[0] = (int16_t)(p[0] + (int16_t)(mul32(d[0], 200) >> 13));
		p[1] = (int16_t)(p[1] + (int16_t)(mul32(d[1], 200) >> 13));
		p[2] = (int16_t)(p[2] + (int16_t)(mul32(d[2], 200) >> 13));
		MatrixMultiplyVector(&Camera(), p, p);
		m.t[0] = (int32_t)p[0] + Camera().t[0];
		m.t[1] = (int32_t)p[1] + Camera().t[1];
		m.t[2] = (int32_t)p[2] + Camera().t[2];
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = 0xE148E8;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x2033;
		if (fade)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F3;
			*(int32_t *)(h + 0xC) = fade_value;
		}
		int32_t w = ComputeSin(sin_arg);
		const int16_t *a = (const int16_t *)0xE148F0, *b = (const int16_t *)0xE15668;
		int16_t *o = (int16_t *)vbuf;
		for (int j = 0; j < 0x7E; j++, a += 4, b += 4, o += 4)
		{
			o[0] = (int16_t)(a[0] + (mul32((int32_t)b[0] - (int32_t)a[0], w) >> 12));
			o[1] = (int16_t)(a[1] + (mul32((int32_t)b[1] - (int32_t)a[1], w) >> 12));
			o[2] = (int16_t)(a[2] + (mul32((int32_t)b[2] - (int32_t)a[2], w) >> 12));
		}
		*(uint32_t *)(h + 4) = vbuf;
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
		FieldFree(0x58);
	}

	static int32_t MorphArg(int32_t c) { return shl32(c, 10) / 12; }

	static uint32_t __cdecl MorphTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		MorphDraw(n, MorphArg(c), c >= 6, mul32(c - 6, 682), Scratch());
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0xC ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Smoke (0x62E4F0, creature tick 3): pool A (mask 2), 10 puffs per tick for 4 ticks in a ring
	// around the effect origin, flipbook 0xE10FB0, drifting with a velocity damped by 1/4.
	// ------------------------------------------------------------------
	static void LitSetup(const Mat4x3 *frame, uint8_t *s, int32_t tx, int32_t ty, int32_t tz)
	{
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = tx;
		*(int32_t *)(s + 0x20) = ty;
		*(int32_t *)(s + 0x24) = tz;
		ComposeAffineTransform(frame, (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static uint32_t __cdecl SmokeTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE10FB0;
		*(int16_t *)(h + 0x24) = 0;
		int32_t count = 0;
		LitSetup(&Frame(), s, 0, 0, 0x28);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecOf(PoolA(), i);
			if (!(r->alive & 2)) continue;
			LitSprite(h, &r->x, r->size, r->age);
			PoolMemo *pm = MemoRecord(0, i, n, r);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				if (pm) pm->died = true;
				continue;
			}
			Integrate(INT_DRAG4, r);
			count++;
		}
		FieldFree(0x48);
		FieldFree(0xB4);
		if (Paused()) return 0;
		FieldAlloc(0x48); // allocated and never used (vanilla)
		int16_t c = n->c;
		if (c >= 0 && c <= 3)
		{
			for (int m = 0; m < 10; m++)
			{
				int i = FreeRecord(PoolA());
				if (i < 0) break;
				Rec *r = RecOf(PoolA(), i);
				r->alive = 2;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x160 + 0x80);
				int32_t a = CrtRand() % 0x1000;
				int32_t rad = CrtRand() % 0x96 + 0xC8;
				r->x = (int16_t)(mul32(ComputeCos(a), rad) >> 12);
				r->y = (int16_t)-(CrtRand() % 100);
				r->z = (int16_t)(mul32(ComputeSin(a), rad) >> 12);
				int32_t sp = CrtRand() % 0x37 + 0x23;
				r->vx = (int16_t)(mul32(ComputeCos(a), sp) >> 12);
				r->vy = (int16_t)(-50 - CrtRand() % 0x7D);
				r->vz = (int16_t)(mul32(ComputeSin(a), sp) >> 12);
			}
		}
		FieldFree(0x48);
		n->c++;
		if (n->c < 4 || count != 0) return 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Rings (0x62E8C0 / 0x62EA10, creature tick 3), 12 ticks: prim 0xE18150 around the effect
	// origin, scale (s1C, s20, s1C); A grows in height (s20 += s22, s22 -= s22/4) and fades from
	// tick 6, B grows in width (s1C += s1E, s1E -= s1E/4) and fades from tick 4.
	// ------------------------------------------------------------------
	static void RingDraw(const Mat4x3 *frame, const Node24 *n, int32_t sx, int32_t sy, bool fade, int32_t fade_value)
	{
		int16_t ang[4] = { 0, n->a18, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->p10;
		m.t[1] = n->p12;
		int32_t sc[3];
		sc[2] = sx;
		sc[0] = sx;
		m.t[2] = n->p14;
		sc[1] = sy;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = 0xE18150;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (fade)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade_value;
		}
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
		FieldFree(0x58);
	}

	static int32_t RingAFade(int32_t c) { return mul32(c - 6, 682); }
	static int32_t RingBFade(int32_t c) { return shl32(c - 4, 9); }

	static uint32_t __cdecl RingATask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t c = n->c;
		RingDraw(&Frame(), n, n->s1C, n->s20, c >= 6, RingAFade(c));
		MemoNode(n);
		if (Paused()) return 0;
		int16_t v = n->s22;
		n->s20 = (int16_t)(n->s20 + v);
		n->c++;
		n->s22 = (int16_t)(v - (int16_t)((int32_t)v / 4));
		return n->c >= 0xC ? TASK_END : 0;
	}

	static uint32_t __cdecl RingBTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t c = n->c;
		RingDraw(&Frame(), n, n->s1C, n->s20, c >= 4, RingBFade(c));
		MemoNode(n);
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->c++;
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 4));
		return n->c >= 0xC ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Dust (0x62EB50, creature tick 3): pool B (mask 2), 12 puffs per tick for 2 ticks on a ring
	// on the ground (y 0, frame raised by 40), flipbook 0xE108E8, sliding outwards (x/z velocity
	// damped by 1/8).
	// ------------------------------------------------------------------
	static uint32_t __cdecl DustTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE108E8;
		*(int16_t *)(h + 0x24) = 0;
		int32_t count = 0;
		LitSetup(&Frame(), s, 0, -40, 0x28);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecOf(PoolB(), i);
			if (!(r->alive & 2)) continue;
			LitSprite(h, &r->x, r->size, r->age);
			PoolMemo *pm = MemoRecord(1, i, n, r);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				if (pm) pm->died = true;
				continue;
			}
			Integrate(INT_XZ_DRAG8, r);
			count++;
		}
		FieldFree(0x48);
		FieldFree(0xB4);
		if (Paused()) return 0;
		FieldAlloc(0x48); // allocated and never used (vanilla)
		int16_t c = n->c;
		if (c >= 0 && c <= 1)
		{
			for (int m = 0; m < 12; m++)
			{
				int i = FreeRecord(PoolB());
				if (i < 0) break;
				Rec *r = RecOf(PoolB(), i);
				r->alive = 2;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x280 + 0x600);
				int32_t a = CrtRand() % 0x1000;
				int32_t rad = CrtRand() % 0x3C + 0xAA;
				r->x = (int16_t)(mul32(ComputeCos(a), rad) >> 12);
				r->y = 0;
				r->z = (int16_t)(mul32(ComputeSin(a), rad) >> 12);
				int32_t sp = CrtRand() % 0x14 + 0xF;
				r->vx = (int16_t)(mul32(ComputeCos(a), sp) >> 12);
				r->vz = (int16_t)(mul32(ComputeSin(a), sp) >> 12);
			}
		}
		FieldFree(0x48);
		n->c++;
		if (n->c < 4 || count != 0) return 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Mist (0x62EE30, creature tick 21): pool A (mask 4), 2 puffs per tick for 8 ticks at the
	// creature's bone 0xF0 (frame raised 20, pushed -50), flipbook 0xE108E8 (colour 0x404040),
	// thrown up and back, growing by 1/32 and damped by 1/8.
	// ------------------------------------------------------------------
	static uint32_t __cdecl MistTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE108E8;
		*(uint32_t *)(h + 0x1C) = 0x404040;
		*(int16_t *)(h + 0x24) = 4;
		int32_t count = 0;
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		GetEffectSpawnPosition(Creature(), 0xF0, 0, (int16_t *)s);
		*(int32_t *)(s + 0x1C) = *(int16_t *)(s + 0);
		*(int32_t *)(s + 0x20) = *(int16_t *)(s + 2) + 0x14;
		*(int32_t *)(s + 0x24) = *(int16_t *)(s + 4) - 0x32;
		ComposeAffineTransform(&Frame(), (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
		// held frames draw around the translation of this tick
		if (Node24 *m = g_node_memo.put(n))
		{
			*m = *n;
			m->p10 = (int16_t)*(int32_t *)(s + 0x1C);
			m->p12 = (int16_t)*(int32_t *)(s + 0x20);
			m->p14 = (int16_t)*(int32_t *)(s + 0x24);
		}
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecOf(PoolA(), i);
			if (!(r->alive & 4)) continue;
			LitSprite(h, &r->x, r->size, r->age);
			PoolMemo *pm = MemoRecord(0, i, n, r);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				if (pm) pm->died = true;
				continue;
			}
			Integrate(INT_GROW5_DRAG8, r);
			count++;
		}
		FieldFree(0x48);
		FieldFree(0xB4);
		if (Paused()) return 0;
		uint8_t *t = (uint8_t *)FieldAlloc(0x48);
		int16_t c = n->c;
		if (c >= 0 && c <= 7)
		{
			for (int m = 0; m < 2; m++)
			{
				int i = FreeRecord(PoolA());
				if (i < 0) break;
				Rec *r = RecOf(PoolA(), i);
				r->alive = 4;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x80 + 0x200);
				r->x = (int16_t)(CrtRand() % 10 - 5);
				r->y = (int16_t)(CrtRand() % 10 - 5);
				r->z = (int16_t)(CrtRand() % 10 - 5);
				int32_t sp = CrtRand() % 8 + 0xF;
				int32_t *v = (int32_t *)(t + 0x28);
				v[0] = CrtRand() % 0x200 - 0x100;
				*(int32_t *)(t + 0x2C) = CrtRand() % 0x400 - 0x180;
				int32_t z = CrtRand() % 0x200;
				*(int32_t *)(t + 0x30) = -0xE00 - z;
				NormalizeVectorToFixedPoint(v, v);
				r->vx = (int16_t)(mul32(v[0], sp) >> 12);
				r->vy = (int16_t)(mul32(*(int32_t *)(t + 0x2C), sp) >> 12);
				r->vz = (int16_t)(mul32(*(int32_t *)(t + 0x30), sp) >> 12);
			}
		}
		FieldFree(0x48);
		n->c++;
		if (n->c < 4 || count != 0) return 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Sword sparks (0x62F740, creature tick 82, history 0x24F0C88; 0x62FC90 x 2, creature tick
	// 106, bones 0x1E / 0x34, histories *(0xE196A8 / 0xE196AC)): every tick the bone's position
	// and direction (effect frame) go into a 3-entry ring history; while the counter <= 11 and at
	// least 2 entries exist, a spline through them (sub_571620 / sub_571690) seeds sparks along
	// the blade: 62F740 5 of 6 points (pool C mask 1, flipbook 0xE108E8 colour 0x404040, growing
	// 1/32), 62FC90 3 of 4 points (mask 2, flipbook 0xE10E04 colour 0x202020, growing 1/64). Both
	// 62FC90 tasks draw and update every mask-2 record.
	// ------------------------------------------------------------------
	static int32_t BoneHistory(uint32_t table, int32_t bone, int32_t c, uint8_t *s)
	{
		GetEffectSpawnPosition(Creature(), bone, 0, (int16_t *)s);
		GetEffectSpawnPosition(Creature(), bone, 0x1000, (int16_t *)(s + 8));
		int16_t *a = (int16_t *)s, *b = (int16_t *)(s + 8);
		int16_t d0 = (int16_t)(b[0] - a[0]), d1 = (int16_t)(b[1] - a[1]), d2 = (int16_t)(b[2] - a[2]);
		a[0] = d0;
		a[1] = d1;
		a[2] = d2;
		NormalizeVector16(a, a);
		MatrixMultiplyVector(&RootMatrix(), a, a);
		int32_t idx = c % 3;
		uint32_t *t = (uint32_t *)(table + 0x14 * idx);
		t[0] = 1;
		t[1] = *(uint32_t *)(s + 8);
		t[2] = *(uint32_t *)(s + 0xC);
		t[3] = *(uint32_t *)(s + 0);
		t[4] = *(uint32_t *)(s + 4);
		int32_t n = 0;
		for (int32_t j = idx; n < 3; n++)
		{
			const uint32_t *e = (const uint32_t *)(table + 0x14 * j);
			if (!e[0]) break;
			var<uint32_t>(HIST_A + 8 * n) = e[1];
			var<uint32_t>(HIST_A + 8 * n + 4) = e[2];
			var<uint32_t>(HIST_B + 8 * n) = e[3];
			var<uint32_t>(HIST_B + 8 * n + 4) = e[4];
			if (--j < 0) j = 2;
		}
		return n;
	}

	static void SparkSetup(const Mat4x3 *frame, uint8_t *h, uint8_t *s, uint32_t seq, uint32_t color)
	{
		memcpy(s + 0x10, frame, 0x20);
		*(uint32_t *)h = seq;
		*(uint32_t *)(h + 0x1C) = color;
		*(int16_t *)(h + 0x24) = 0xC;
		GteSetLightMatrix(s + 0x10);
		GteSetBackColorFromTrans(s + 0x10);
	}

	static int32_t SparkDrawPool(const void *owner, uint8_t *h, uint32_t mask, int kind)
	{
		int32_t count = 0;
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecOf(PoolC(), i);
			if (!(r->alive & mask)) continue;
			LitSprite(h, &r->x, r->size, r->age);
			PoolMemo *pm = MemoRecord(2, i, owner, r);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				if (pm) pm->died = true;
				continue;
			}
			Integrate(kind, r);
			count++;
		}
		return count;
	}

	static void SplineRails(int32_t n, int32_t points, int32_t div)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x190);
		SplineSetup(n, HIST_A, w);
		for (int32_t i = 0, t = 0; i < points; i++, t += 0x1000) SplineEval(n, w, SPLINE_A + 8 * i, t / div);
		SplineSetup(n, HIST_B, w);
		for (int32_t i = 0, t = 0; i < points; i++, t += 0x1000) SplineEval(n, w, SPLINE_B + 8 * i, t / div);
		FieldFree(0x190);
	}

	static uint32_t __cdecl SparkTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		int32_t hist = BoneHistory(0x24F0C88, 0x28, n->c, s);
		SparkSetup(&Frame(), h, s, 0xE108E8, 0x404040);
		int32_t count = SparkDrawPool(n, h, 1, INT_GROW5_DRAG8);
		FieldFree(0x50);
		FieldFree(0xB4);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 0 && c <= 0xB && hist > 1)
		{
			SplineRails(hist, 6, 5);
			for (int j = 0; j < 5; j++)
			{
				int i = FreeRecord(PoolC());
				if (i < 0) break;
				Rec *r = RecOf(PoolC(), i);
				r->alive = 1;
				r->age = 0;
				int32_t sz = CrtRand() % 0x480;
				uint32_t pa = var<uint32_t>(SPLINE_A + 8 * j), pb = var<uint32_t>(SPLINE_A + 8 * j + 4);
				r->size = (int16_t)(sz + 0x380);
				*(uint32_t *)&r->x = pa;
				*(uint32_t *)&r->z = pb;
				r->x = (int16_t)(r->x + (int16_t)(CrtRand() % 10 - 5));
				r->y = (int16_t)(r->y + (int16_t)(CrtRand() % 10 - 5));
				r->z = (int16_t)(r->z + (int16_t)(CrtRand() % 10 - 5));
				int32_t sp = CrtRand() % 0xF + 0x19;
				const int16_t *d = (const int16_t *)(SPLINE_B + 8 * j);
				int32_t k = CrtRand() % 10;
				r->vx = (int16_t)(k + (mul32(d[0], sp) >> 12) - 5);
				k = CrtRand() % 10;
				r->vy = (int16_t)(k + (mul32(d[1], sp) >> 12) - 5);
				k = CrtRand() % 10;
				r->vz = (int16_t)(k + (mul32(d[2], sp) >> 12) - 5);
			}
		}
		n->c++;
		if (n->c < 4 || count != 0) return 0;
		return TASK_END;
	}

	static uint32_t __cdecl Spark2Task(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		uint32_t table = var<uint32_t>(0xE196A4 + 4 * (int32_t)n->a18);
		int32_t hist = BoneHistory(table, n->e, n->c, s);
		SparkSetup(&Frame(), h, s, 0xE10E04, 0x202020);
		int32_t count = SparkDrawPool(n, h, 2, INT_GROW6_DRAG8);
		FieldFree(0x50);
		FieldFree(0xB4);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 0 && c <= 0xB && hist > 1)
		{
			SplineRails(hist, 4, 3);
			for (int j = 0; j < 3; j++)
			{
				int i = FreeRecord(PoolC());
				if (i < 0) break;
				Rec *r = RecOf(PoolC(), i);
				r->alive = 2;
				r->age = 0;
				int32_t sz = CrtRand() % 0x400;
				uint32_t pa = var<uint32_t>(SPLINE_A + 8 * j), pb = var<uint32_t>(SPLINE_A + 8 * j + 4);
				r->size = (int16_t)(sz + 0x100);
				*(uint32_t *)&r->x = pa;
				*(uint32_t *)&r->z = pb;
				r->x = (int16_t)(r->x + (int16_t)(CrtRand() % 20 - 10));
				r->y = (int16_t)(r->y + (int16_t)(CrtRand() % 20 - 10));
				r->z = (int16_t)(r->z + (int16_t)(CrtRand() % 20 - 10));
				int32_t sp = CrtRand() % 0xF + 0x19;
				const int16_t *d = (const int16_t *)(SPLINE_B + 8 * j);
				int32_t k = CrtRand() % 20;
				r->vx = (int16_t)(k + (mul32(d[0], sp) >> 12) - 10);
				k = CrtRand() % 20;
				r->vy = (int16_t)(k + (mul32(d[1], sp) >> 12) - 10);
				k = CrtRand() % 20;
				r->vz = (int16_t)(k + (mul32(d[2], sp) >> 12) - 10);
			}
		}
		n->c++;
		if (n->c < 4 || count != 0) return 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Blade glow (0x630160, creature tick 0), until the creature's object mask +0x7C loses bit 2
	// (creature tick 153): prim 0xE18B38 on the sword, matrix = creature matrix o the bone matrix
	// copied (one tick late) from skeleton + 0xB30 o R(0x400, 0, 0x800) x 0x1A00; also computes
	// the blade end points 0x24F1310 / 0x24F1318 (effect frame) used by the trail. Drawn from
	// tick 1, at the nearer end's depth minus 8 (mode 0xE).
	// ------------------------------------------------------------------
	static void BladeDraw(const Mat4x3 *frame, const Mat4x3 *emat, const Mat4x3 *bone, int16_t *p0, int16_t *p1, bool draw)
	{
		int16_t v[4] = { 0x400, 0, 0x800, 0 };
		Mat4x3 m1, m2;
		ComposeZYXRotationMatrix(v, &m1);
		int32_t sc[3] = { 0x1A00, 0x1A00, 0x1A00 };
		m1.t[0] = 0;
		m1.t[1] = 0;
		m1.t[2] = 0;
		Scale3DMatrix(&m1, sc);
		ComposeAffineTransform(emat, bone, &m2);
		GteMatrixMultiply(&m2, &m1);
		v[0] = 0;
		v[1] = 0;
		v[2] = 0;
		TransformVectorBy3x3Matrix(&m2, m1.t, m1.t);
		int32_t t0 = (int32_t)((uint32_t)m1.t[0] + (uint32_t)m2.t[0]);
		int32_t t1 = (int32_t)((uint32_t)m1.t[1] + (uint32_t)m2.t[1]);
		int32_t t2 = (int32_t)((uint32_t)m1.t[2] + (uint32_t)m2.t[2]);
		m1.t[2] = t2;
		m1.t[0] = t0;
		m1.t[1] = t1;
		v[0] = 0;
		v[1] = 1000;
		v[2] = -1000;
		MatrixMultiplyVector(&m1, v, p0);
		p0[0] = (int16_t)(p0[0] + (int16_t)t0);
		p0[1] = (int16_t)(p0[1] + (int16_t)t1);
		p0[2] = (int16_t)(p0[2] + (int16_t)t2);
		v[0] = 0;
		v[1] = 0x1838;
		v[2] = 0x258;
		MatrixMultiplyVector(&m1, v, p1);
		p1[0] = (int16_t)(p1[0] + (int16_t)t0);
		p1[1] = (int16_t)(p1[1] + (int16_t)t1);
		p1[2] = (int16_t)(p1[2] + (int16_t)t2);
		ComposeAffineTransform(frame, &m1, &m1);
		GteSetRotMatrix(&m1);
		GteSetTransVector(&m1);
		int32_t o0, o1;
		GteLoadV0(p0);
		GteRTPS();
		GteReadSXY2(v);
		GteReadOTZ32(&o0);
		GteLoadV0(p1);
		GteRTPS();
		GteReadSXY2(v);
		GteReadOTZ32(&o1);
		int32_t o = o0 < o1 ? o0 : o1;
		o = o / 4 - 8;
		if (o < 0) o = 0;
		if (!draw) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = 0xE18B38;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		Cursor() = RenderPrimModel(h, var<uint32_t>(0x1D8E04C) + 4 * (uint32_t)o, 0xE, Cursor());
		FieldFree(0x58);
	}

	struct BladeMemo { uint32_t tick; int16_t c; Mat4x3 e, bone; };
	static BladeMemo g_blade = { 0xFFFFFFFF };

	static uint32_t __cdecl BladeTask(TaskNode *tn)
	{
		if (!(var<uint8_t>(0x24F12E4) & 4)) return TASK_END;
		Node24 *n = (Node24 *)tn;
		g_blade.tick = g_real_tick;
		g_blade.c = n->c;
		g_blade.e = CreatureMat();
		g_blade.bone = BoneCopy();
		BladeDraw(&Frame(), &CreatureMat(), &BoneCopy(), BladeP0(), BladeP1(), n->c > 0);
		if (Paused()) return 0;
		uint8_t *sk = **(uint8_t ***)(0x24F12CC);
		n->c++;
		memcpy(&BoneCopy(), sk + 0xB30, 0x20);
		return 0;
	}

	// ------------------------------------------------------------------
	// Blade trail (0x630430, creature tick 146), 6 ticks: every tick the blade base 0x24F1310 and
	// a point along the blade (length factor 0xE19674[c]) go into the 10-entry ring 0x24F2180; a
	// 32-point spline through the history (base rail and tip rail, the newest tip = 0x24F1318) is
	// drawn as 30 Gouraud quad pairs (semi-transparent, colour 0x903030 fading to the far colour
	// 0 along the trail through GTE DPCS).
	// ------------------------------------------------------------------
	static void TrailDraw(const Mat4x3 *frame, uint32_t rail_a, uint32_t rail_b, uint8_t *s)
	{
		GteSetRotMatrix(frame);
		GteSetTransVector(frame);
		GteSetFarColor2(0, 0, 0);
		uint32_t pk = Cursor();
		*(uint32_t *)(s + 0x14) = 0x3A903030;
		*(uint32_t *)(s + 0x1C) = 0x3A903030;
		uint32_t pk2 = pk + 0x24;
		int32_t acc = 0;
		for (uint32_t j = 0; j < 0xF0; j += 8, acc += 0x1000)
		{
			*(uint32_t *)(s + 0x10) = *(uint32_t *)(s + 0x14);
			uint32_t *a = (uint32_t *)pk, *b = (uint32_t *)pk2;
			a[0] = 0x8000000;
			GteLoadV012((const void *)(rail_a + j), (const void *)(rail_a + j + 8), (const void *)(rail_b + j));
			GteRTPT();
			GteReadFLAG(s + 0xC);
			if (*(uint32_t *)(s + 0xC) & 0x60000) continue;
			GteReadSXY012Split(a + 2, a + 4, a + 6);
			GteLoadV0((const void *)(rail_b + j + 8));
			GteRTPS();
			GteReadSXY2(a + 8);
			GteAVSZ4();
			GteReadOTZ32(s + 8);
			GteSetIR0(acc / 30);
			GteLoadRGBC(s + 0x1C);
			GteDPCS();
			GteStoreRGB2(s + 0x14);
			uint32_t c0 = *(uint32_t *)(s + 0x10), c1 = *(uint32_t *)(s + 0x14);
			a[5] = c0;
			a[1] = c0;
			a[7] = c1;
			a[3] = c1;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + 4 * (uint32_t)(*(int32_t *)(s + 8) >> 2) + 0x44, a);
			b[5] = *(uint32_t *)(s + 0x10);
			b[3] = 0x3A000000;
			b[1] = 0x3A000000;
			b[2] = a[2];
			b[7] = *(uint32_t *)(s + 0x14);
			b[4] = a[4];
			b[6] = a[6];
			b[8] = a[8];
			b[0] = 0x8000000;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + 4 * (uint32_t)(*(int32_t *)(s + 8) >> 2) + 0x44, b);
			pk += 0x48;
			pk2 += 0x48;
		}
		Cursor() = pk2; // (vanilla: the cursor ends 0x24 bytes past the last packet)
	}

	struct TrailMemo { bool drawn; uint8_t a[0x100], b[0x100]; };
	static NodeMemo<TrailMemo, 8> g_trail_memo;

	static uint32_t __cdecl TrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		const int16_t *p0 = BladeP0(), *p1 = BladeP1();
		int32_t k = var<int32_t>(0xE19674 + 4 * (int32_t)n->c);
		int32_t *d = (int32_t *)(s + 0x30);
		d[0] = (int32_t)p1[0] - (int32_t)p0[0];
		d[1] = (int32_t)p1[1] - (int32_t)p0[1];
		d[2] = (int32_t)p1[2] - (int32_t)p0[2];
		int32_t len = Sqrt(NormalizeVectorToFixedPoint(d, d));
		len = mul32(len, k) >> 12;
		uint32_t w0 = var<uint32_t>(0x24F1310), w1 = var<uint32_t>(0x24F1314);
		*(int16_t *)(s + 0x28) = (int16_t)((mul32(d[0], len) >> 12) + (int32_t)w0);
		*(int16_t *)(s + 0x2A) = (int16_t)((int16_t)(mul32(len, d[1]) >> 12) + p0[1]);
		*(int16_t *)(s + 0x2C) = (int16_t)((mul32(d[2], len) >> 12) + (int32_t)w1);
		int32_t idx = (int32_t)n->c % 10;
		uint32_t *rec = (uint32_t *)(TRAIL_RECS + 0x14 * idx);
		rec[0] = 1;
		rec[1] = w0;
		rec[2] = w1;
		rec[3] = *(uint32_t *)(s + 0x28);
		rec[4] = *(uint32_t *)(s + 0x2C);
		int32_t cnt = 0;
		for (int32_t j = idx; cnt < 10; cnt++)
		{
			const uint32_t *e = (const uint32_t *)(TRAIL_RECS + 0x14 * j);
			if (!e[0]) break;
			var<uint32_t>(HIST_A + 8 * cnt) = e[1];
			var<uint32_t>(HIST_A + 8 * cnt + 4) = e[2];
			const uint32_t *src = cnt == 0 ? (const uint32_t *)0x24F1318 : e + 3;
			var<uint32_t>(HIST_B + 8 * cnt) = src[0];
			var<uint32_t>(HIST_B + 8 * cnt + 4) = src[1];
			if (--j < 0) j = 9;
		}
		TrailMemo *m = g_trail_memo.put(n);
		if (m) m->drawn = false;
		if (cnt > 1)
		{
			SplineRails(cnt, 32, 31);
			TrailDraw(&Frame(), SPLINE_A, SPLINE_B, s);
			if (m)
			{
				m->drawn = true;
				memcpy(m->a, (const void *)SPLINE_A, 0x100);
				memcpy(m->b, (const void *)SPLINE_B, 0x100);
			}
		}
		FieldFree(0x40);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 6 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ghost trail (0x62C550, timeline tick 323), 30 ticks, model E2 (data 0x24F2140, buffer
	// 0xE18CF8, anim 1 armed, never advanced): moves by the node velocity (+0x10) and turns about x
	// (+0x18 += +0x1A) - update, then draw; then the matrix goes into the 6-entry ring 0x24F0F88
	// and every recorded matrix is drawn again, newest first, colour 0x2606060 darkening by
	// 0x0C0C0C; matrix and colour (0x808080) put back.
	// ------------------------------------------------------------------
	static void GhostPose(Mat4x3 *m, int16_t a)
	{
		int16_t ang[4] = { a, 0, 0, 0 };
		ComposeZYXRotationMatrix(ang, m);
		int32_t sc[3] = { 0x1000, 0x1000, 0x1000 };
		Scale3DMatrix(m, sc);
	}

	static void GhostEchoes(uint8_t *E, int32_t k, const Mat4x3 *frame, uint32_t scratch)
	{
		uint32_t col = 0x2606060;
		for (int32_t i = 0, j = k; i < 6; i++)
		{
			const uint32_t *r = (const uint32_t *)(GHOST_RECS + 0x24 * j);
			if (!r[0]) break;
			memcpy(&Model2Mat(), r + 1, 0x20);
			var<uint32_t>(0x24F0F10) = col;
			DrawModel(E, frame, scratch);
			col -= 0xC0C0C;
			if (--j < 0) j = 5;
		}
	}

	struct GhostMemo { uint32_t tick; Node24 n; int32_t k; Mat4x3 mat; };
	static GhostMemo g_ghost = { 0xFFFFFFFF };

	static uint32_t __cdecl GhostTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *E = Model2();
		if (n->c == 0)
		{
			ResetRotation((void *)0x24F0F28);
			var<uint16_t>(0x24F0F38) = 0x1000;
			var<uint16_t>(0x24F0F30) = 0x1000;
			var<uint16_t>(0x24F0F28) = 0x1000;
			var<int32_t>(0x24F0F3C) = 0xEA6;
			var<int32_t>(0x24F0F40) = (int32_t)0xFFFFEC78;
			var<int32_t>(0x24F0F44) = 0x1518;
			ModelInit(E, (uint8_t *)0x24F2140, (uint8_t *)0xE18CF8, 0x10);
			SetAnim(E, 1);
			n->p10 = (int16_t)0xFE89;
			n->p12 = (int16_t)0xFD3C;
			n->p14 = (int16_t)0xFD1B;
			n->a18 = (int16_t)(CrtRand() % 0x1000);
			n->s1A = 0x300;
			for (uint32_t a = GHOST_RECS; a < 0x24F1060; a += 0x24) *(uint32_t *)a = 0;
		}
		if (!Paused())
		{
			Mat4x3 &m = Model2Mat();
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)n->p10);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)n->p14);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)n->p12);
			n->a18 = (int16_t)(n->a18 + n->s1A);
			n->c++;
		}
		GhostPose(&Model2Mat(), n->a18);
		DrawModel(E, &Frame(), Scratch());
		int32_t k = (int32_t)n->c % 6;
		uint32_t *rec = (uint32_t *)(GHOST_RECS + 0x24 * k);
		rec[0] = 1;
		memcpy(rec + 1, &Model2Mat(), 0x20);
		Mat4x3 save = Model2Mat();
		g_ghost.tick = g_real_tick;
		g_ghost.n = *n;
		g_ghost.k = k;
		g_ghost.mat = save;
		GhostEchoes(E, k, &Frame(), Scratch());
		Model2Mat() = save;
		var<uint32_t>(0x24F0F10) = 0x808080;
		return n->c >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ghost's sword (0x62C8F0, timeline tick 323), 30 ticks: prim 0xE18B38 (mode 3) scaled 0x5400
	// on E2's bone matrix (skeleton + 0x20, copied to 0x24F2288 every tick, paused or not) under
	// E2's matrix; drawn from tick 1.
	// ------------------------------------------------------------------
	static void SwordDraw(const Mat4x3 *frame, const Mat4x3 *emat, bool draw)
	{
		int16_t v[4] = { 0, 0, 0, 0 };
		Mat4x3 m1, m2;
		ComposeZYXRotationMatrix(v, &m1);
		int32_t sc[3] = { 0x5400, 0x5400, 0x5400 };
		m1.t[0] = 0;
		m1.t[1] = 0;
		m1.t[2] = 0;
		Scale3DMatrix(&m1, sc);
		uint8_t *sk = **(uint8_t ***)(0x24F0F4C); // E2 comFileData -> skeleton
		memcpy(&BoneCopy(), sk + 0x20, 0x20);
		ComposeAffineTransform(emat, &BoneCopy(), &m2);
		GteMatrixMultiply(&m2, &m1);
		TransformVectorBy3x3Matrix(&m2, m1.t, m1.t);
		int32_t t1 = (int32_t)((uint32_t)m1.t[1] + (uint32_t)m2.t[1]);
		int32_t t2 = (int32_t)((uint32_t)m1.t[2] + (uint32_t)m2.t[2]);
		int32_t t0 = (int32_t)((uint32_t)m1.t[0] + (uint32_t)m2.t[0]);
		m1.t[1] = t1;
		m1.t[2] = t2;
		m1.t[0] = t0;
		ComposeAffineTransform(frame, &m1, &m1);
		GteSetRotMatrix(&m1);
		GteSetTransVector(&m1);
		if (!draw) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = 0xE18B38;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		Cursor() = RenderPrimModel(h, OT(), 3, Cursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl SwordTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		SwordDraw(&Frame(), &Model2Mat(), n->c > 0);
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Fading models (0x62CA80: E2, data 0x24F2140, buffer 0xE18CF8; 0x62CC00: E3, data 0x24F22A8,
	// buffer *(0x24F22E4) = character load 0x2E8; timeline tick 382), 27 ticks: anim 0 (no loop)
	// at (-250, -0x708, 0x258); advance then draw; from tick 19 the colour is depth-cued from
	// 0x808080 to black by 0x1000 - sin((c - 19) * 1024 / 8).
	// ------------------------------------------------------------------
	static int32_t FadeW(int32_t c) { return 0x1000 - ComputeSin(shl32(c - 0x13, 10) / 8); }

	static uint32_t FadeTask(Node24 *n, uint8_t *E, uint32_t mat, uint32_t data, uint32_t buf_ptr, ModelMemo &memo)
	{
		if (n->c == 0)
		{
			ResetRotation((void *)mat);
			uint8_t *buf = buf_ptr ? var<uint8_t *>(buf_ptr) : (uint8_t *)0xE18CF8;
			var<uint16_t>(mat + 0x10) = 0x1000;
			var<uint16_t>(mat + 8) = 0x1000;
			var<uint16_t>(mat + 0) = 0x1000;
			var<int32_t>(mat + 0x14) = (int32_t)0xFFFFFF06;
			var<int32_t>(mat + 0x18) = (int32_t)0xFFFFF8F8;
			var<int32_t>(mat + 0x1C) = 0x258;
			ModelInit(E, (uint8_t *)data, buf, 0x10);
			E[0] |= 1;
			SetAnim(E, 0);
		}
		else if (!Paused()) AdvanceModelAnimLoop(E);
		int16_t c = n->c;
		if (c >= 0x13)
		{
			int32_t w = FadeW(c);
			DepthCue(w, 0x80, 0x80, 0x80, E + 0x28);
		}
		DrawModel(E, &Frame(), Scratch());
		MemoTake(memo, E);
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		memo.midpoint = c >= 1; // the next tick advances one frame (the pose right after SetAnim is held)
		memo.hold_mat = true;   // the matrix never moves
		return n->c >= 0x1B ? TASK_END : 0;
	}

	static uint32_t __cdecl FadeATask(TaskNode *tn) { return FadeTask((Node24 *)tn, Model2(), 0x24F0F28, 0x24F2140, 0, g_fade_a); }
	static uint32_t __cdecl FadeBTask(TaskNode *tn) { return FadeTask((Node24 *)tn, Model3(), 0x24F0D90, 0x24F22A8, 0x24F22E4, g_fade_b); }

	// ------------------------------------------------------------------
	// Screen tint (0x62CD40, timeline tick 382), 27 ticks: full-screen tile (0x70, 0x80, 0xF0) at
	// depth 0x1000, from tick 19 depth-cued to black like the fading models.
	// ------------------------------------------------------------------
	static void TintColor(int32_t c, uint8_t *L)
	{
		L[0] = 0x70;
		L[1] = 0x80;
		L[2] = 0xF0;
		if (c >= 0x13)
		{
			int32_t w = FadeW(c);
			// RGB2 is stored over the three colour bytes (and the byte after them)
			uint8_t out[4];
			DepthCue(w, L[0], L[1], L[2], out);
			memcpy(L, out, 4);
		}
	}

	struct TintMemo { uint8_t rgb[4]; };
	static NodeMemo<TintMemo, 8> g_tint_memo;

	static uint32_t __cdecl TintTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t L[4];
		TintColor(n->c, L);
		ScreenTile(L[0], L[1], L[2], 0x1000);
		if (TintMemo *m = g_tint_memo.put(n)) memcpy(m->rgb, L, 4);
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x1B ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Bursts (MAG_066_sub_650720 models, header 0x90, texture scroll c * 16, per-frame value from
	// an 8-entry table, mode 0x33 then fading (0xF3)), rotated to face the camera-script snapshot
	// point 0x24FD358 (Doomtrain-file globals written by MAG_066_sub_63E9C0) and spun about z:
	//   0x630870 (creature 108): model 0xE11798 at the node position, table 0xE19790, fades from
	//            tick 6 by 682 / tick, 12 ticks;
	//   0x630AB0 (creature 132): model 0xE13040 at the snapshot point 0x24FD250, facing along
	//            0x24FD250 -> 0x24FD358, table 0xE197B0, fades from tick 7 by 1024 / tick, 11 ticks.
	// Scale (s1C, s1C, s20).
	// ------------------------------------------------------------------
	static void BurstDraw(const Node24 *n, int32_t c, bool b, int32_t scroll, int32_t fade)
	{
		int32_t d[3];
		int32_t px, py, pz;
		if (b)
		{
			px = var<int16_t>(0x24FD250);
			py = var<int16_t>(0x24FD252);
			pz = var<int16_t>(0x24FD254);
		}
		else
		{
			px = n->p10;
			py = n->p12;
			pz = n->p14;
		}
		d[0] = (int32_t)var<int16_t>(0x24FD358) - px;
		d[1] = (int32_t)var<int16_t>(0x24FD35A) - py;
		d[2] = (int32_t)var<int16_t>(0x24FD35C) - pz;
		NormalizeVectorToFixedPoint(d, d);
		int32_t axis[4];
		int32_t angle = RotationBetweenVectors(0xE19740, d, axis);
		Mat4x3 m1, m2;
		AxisAngleMatrix(angle, &m1, axis);
		int16_t ang[4] = { 0, 0, n->a18, 0 };
		ComposeZYXRotationMatrix(ang, &m2);
		GteMatrixMultiply(&m1, &m2);
		if (b)
		{
			m2.t[1] = var<int16_t>(0x24FD252);
			m2.t[0] = var<int16_t>(0x24FD250);
			m2.t[2] = var<int16_t>(0x24FD254);
		}
		else
		{
			m2.t[1] = n->p12;
			m2.t[0] = n->p10;
			m2.t[2] = n->p14;
		}
		int32_t sc[3];
		sc[1] = n->s1C;
		sc[0] = n->s1C;
		sc[2] = n->s20;
		Scale3DMatrix(&m2, sc);
		ComposeAffineTransform(&Camera(), &m2, &m2);
		GteSetRotMatrix(&m2);
		GteSetTransVector(&m2);
		uint8_t *h = (uint8_t *)FieldAlloc(0x90);
		*(uint32_t *)h = b ? 0xE13040 : 0xE11798;
		*(uint32_t *)(h + 8) = 0;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint32_t *)(h + 0x10) = 0;
		*(uint32_t *)(h + 0x20) = 0x33;
		*(int32_t *)(h + 0x14) = scroll;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint32_t *)(h + 0x24) = var<uint32_t>((b ? 0xE197B0 : 0xE19790) + 4 * (c % 8));
		if (c >= (b ? 7 : 6))
		{
			*(uint32_t *)(h + 0x20) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		Cursor() = Mag066Draw(h, OT(), 2, Cursor());
		FieldFree(0x90);
	}

	static int32_t BurstFade(bool b, int32_t c) { return b ? shl32(c - 7, 10) : mul32(c - 6, 682); }

	static uint32_t __cdecl BurstATask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		BurstDraw(n, c, false, shl32(c, 4), BurstFade(false, c));
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0xC ? TASK_END : 0;
	}

	static uint32_t __cdecl BurstBTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		BurstDraw(n, c, true, shl32(c, 4), BurstFade(true, c));
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0xB ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Blood (0x630D30 x 2, creature tick 246): pool C (mask = +0x0E: 4 / 8); while the counter
	// <= 6, 6 drops per tick at the node position, thrown sideways (x sign of the node position,
	// random y-less direction, radius 300..699, speed 90..189, up 15..39); then every drop is
	// drawn (flipbook 0xE10E04, translation from MAC) and integrated (grows 1/32, x/z damped 1/8,
	// falling speed grows 1/64). Ends on the first tick with no drop left.
	// ------------------------------------------------------------------
	static void BloodSetup(const Mat4x3 *frame, uint8_t *h, uint8_t *s)
	{
		*(int16_t *)(h + 0x24) = 8;
		memcpy(s + 8, frame, 0x20);
		*(uint32_t *)h = 0xE10E04;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static uint32_t __cdecl BloodTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolC();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 6)
		{
			*(uint32_t *)(s + 0) = *(const uint32_t *)&n->p10;
			*(uint32_t *)(s + 4) = *(const uint32_t *)&n->p14;
			int32_t sign = *(int16_t *)s < 0 ? -1 : 1;
			for (int m = 0; m < 6; m++)
			{
				int i = FreeRecord(pool);
				if (i < 0) break;
				Rec *r = RecOf(pool, i);
				r->alive = (uint32_t)(int32_t)n->e;
				r->age = 0;
				int32_t sz = CrtRand() % 0x600;
				r->size = (int16_t)(sz + 0x200);
				*(uint32_t *)&r->x = *(uint32_t *)(s + 0);
				*(uint32_t *)&r->z = *(uint32_t *)(s + 4);
				int32_t *v = (int32_t *)(s + 0x28);
				v[0] = mul32(CrtRand() % 0x800, sign);
				v[1] = 0;
				v[2] = CrtRand() % 0x2000 - 0x1000;
				NormalizeVectorToFixedPoint(v, v);
				int32_t rad = CrtRand() % 0x190 + 0x12C;
				r->x = (int16_t)(r->x + (int16_t)(mul32(v[0], rad) >> 12));
				r->y = (int16_t)-(CrtRand() % 100);
				r->z = (int16_t)(r->z + (int16_t)(mul32(v[2], rad) >> 12));
				int32_t sp = CrtRand() % 100 + 0x5A;
				r->vx = (int16_t)(mul32(v[0], sp) >> 12);
				int32_t up = CrtRand() % 0x19;
				int32_t vz = mul32(v[2], sp) >> 12;
				r->vy = (int16_t)(-15 - up);
				r->vz = (int16_t)vz;
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		BloodSetup(&Frame(), h, s);
		int32_t count = 0;
		uint32_t mask = (uint32_t)(int32_t)n->e;
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecOf(pool, i);
			if (!(mask & r->alive)) continue;
			LitSprite(h, &r->x, r->size, r->age, true);
			PoolMemo *pm = MemoRecord(2, i, n, r);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				if (pm) pm->died = true;
				continue;
			}
			r->age++;
			Integrate(INT_BLOOD, r);
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count != 0 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Cut lines, screen space (z = word_1D8E038), model *(0xE196C0)[+0x0E] with colour
	// *(0xE19730)[+0x0E] through the renderer MAG_326_sub_6314A0 (mode 3), scale s1C x 3 shrinking
	// (s1C -= s1E, s1E -= s1E / k) over the first ticks, fading in over 8 and out over the last 8
	// (+0x16 = base fade), after a delay +0x22:
	//   0x6312F0 (creature 198, x3) shrink 4 ticks (k 2), ends at +0x20; 0x6321A0 (x2 each) ends at 7;
	//   0x632350 (creature 198) shrink 6 ticks (k 3), ends at +0x20; 0x632500 (x4) ends at 7.
	// ------------------------------------------------------------------
	static int32_t SlashFade(int32_t c, int32_t s20, int32_t base)
	{
		int32_t span = 0x1000 - base;
		if (c < 8) return 0x1000 - mul32(span / 8, c);
		int32_t out = s20 - 8;
		if (c >= out) return mul32(c - out, span / 8) + base;
		return base;
	}

	static void SlashDraw(const Node24 *n, int32_t scale, int32_t fade_value)
	{
		int16_t ang[4] = { 0, 0, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[1] = n->p12;
		m.t[0] = n->p10;
		int32_t sc[3] = { scale, scale, scale };
		m.t[2] = var<int16_t>(0x1D8E038);
		Scale3DMatrix(&m, sc);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
		int32_t e = n->e;
		*(uint32_t *)h = var<uint32_t>(0xE196C0 + 4 * e);
		*(uint32_t *)(h + 8) = 0;
		*(int32_t *)(h + 0xC) = fade_value;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		*(uint32_t *)(h + 0x20) = var<uint32_t>(0xE19730 + 4 * e);
		Cursor() = SlashRender(h, OT(), 3, Cursor());
		FieldFree(0x6C);
	}

	static uint32_t SlashTask(Node24 *n, int32_t shrink, int32_t div, int32_t end)
	{
		if (n->s22 > 0)
		{
			if (Paused()) return 0;
			n->s22--;
			return 0;
		}
		SlashDraw(n, n->s1C, SlashFade(n->c, n->s20, n->p16));
		MemoNode(n);
		if (Paused()) return 0;
		int16_t cc = n->c;
		if (cc < shrink)
		{
			int16_t v = n->s1E;
			n->s1C = (int16_t)(n->s1C - v);
			n->s1E = (int16_t)(v - (int16_t)((int32_t)v / div));
		}
		n->c = (int16_t)(cc + 1);
		return n->c >= (end < 0 ? n->s20 : end) ? TASK_END : 0;
	}

	static uint32_t __cdecl SlashATask(TaskNode *tn) { return SlashTask((Node24 *)tn, 4, 2, -1); }
	static uint32_t __cdecl SlashBTask(TaskNode *tn) { return SlashTask((Node24 *)tn, 4, 2, 7); }
	static uint32_t __cdecl SlashCTask(TaskNode *tn) { return SlashTask((Node24 *)tn, 6, 3, -1); }
	static uint32_t __cdecl SlashDTask(TaskNode *tn) { return SlashTask((Node24 *)tn, 6, 3, 7); }

	// ------------------------------------------------------------------
	// Held frames (30 fps). Everything is drawn half way between the state drawn on the last
	// real tick and the state the next tick will draw; spawns, flipbook frames, per-frame table
	// values and random choices keep their 15 Hz batches. The frame matrix is rebuilt from the
	// held-frame camera while the timeline (its owner) is alive, into a local (never into
	// 0x24F2268). Module globals written by the draws (blade end points, bone copy, model
	// matrices / colours / skeletons, the morph vertices) are either computed into private
	// copies or put back; model draws use a private vertex scratch.
	// ------------------------------------------------------------------
	static uint8_t g_scratch[0x20000];   // RenderGeometry / clipped renderer vertex scratch
	static uint8_t g_morph_verts[0x400]; // 126 x 8
	static uint8_t g_skel_cur[SKEL_MAX];

	static int16_t Lerp16(int32_t a, int32_t b, int num, int den) { return (int16_t)lerp_i(a, b, num, den); }

	static uint32_t LerpColor(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t r = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			r |= (uint32_t)(lerp_i((a >> k) & 0xFF, (b >> k) & 0xFF, num, den) & 0xFF) << k;
		return r;
	}

	static void LerpMatrix(Mat4x3 *out, const Mat4x3 &a, const Mat4x3 &b, int num, int den)
	{
		*out = a;
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++) out->m[i][j] = Lerp16(a.m[i][j], b.m[i][j], num, den);
			out->t[i] = lerp_i(a.t[i], b.t[i], num, den);
		}
	}

	// the creature matrix the next real tick draws with (t.z moves before the draw at 144..152)
	static Mat4x3 CreatureMatNext()
	{
		Mat4x3 m = CreatureMat();
		if (g_creature.tick == g_real_tick) m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)g_creature.next_dz);
		return m;
	}

	// draws model E with the memo's skeleton (at the midpoint pose if asked), the given matrix and
	// colour and the private scratch; puts the skeleton, the reader command, matrix and colour back
	static void ModelHeld(const ModelMemo &m, uint8_t *E, const Mat4x3 *frame, const Mat4x3 &mat, uint32_t color, bool midpoint, int num, int den)
	{
		if (m.tick != g_real_tick) return;
		uint8_t *sk = Skeleton(E);
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		uint8_t cmd_cur[8];
		Mat4x3 mat_cur = *(Mat4x3 *)(E + 0x40);
		uint32_t color_cur = *(uint32_t *)(E + 0x28);
		memcpy(g_skel_cur, sk, m.skel_size);
		memcpy(cmd_cur, E + 0x6C, 8);
		// the drawn pose (its values and its local matrices) and the command that read it
		memcpy(sk, m.skel, m.skel_size);
		memcpy(E + 0x6C, m.cmd, 8);
		if (midpoint) pose_midpoint(E + 0x60, E + 0x6C, num, den);
		*(Mat4x3 *)(E + 0x40) = mat;
		*(uint32_t *)(E + 0x28) = color;
		DrawModel(E, frame, (uint32_t)g_scratch);
		*(uint32_t *)(E + 0x28) = color_cur;
		*(Mat4x3 *)(E + 0x40) = mat_cur;
		memcpy(E + 0x6C, cmd_cur, 8);
		memcpy(sk, g_skel_cur, m.skel_size);
	}

	// ---- grids ----
	static void GridHeld(const Node24 *n, int kind, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t c = m->c;
		bool mv = NodeMoved(m, n) && !g_tick_paused;
		switch (kind)
		{
		case 0: // A
		case 1: // B
		{
			bool b = kind == 1;
			int32_t f = b ? GridBFade(c) : GridAFade(c);
			int32_t u = b ? c * 2 : c, v = c * 2;
			if (mv)
			{
				f = lerp_i(f, b ? GridBFade(c + 1) : GridAFade(c + 1), num, den);
				u = lerp_i(u, b ? (c + 1) * 2 : c + 1, num, den);
				v = lerp_i(v, (c + 1) * 2, num, den);
			}
			GridDraw(m, b ? 0x200 : 0x100, b ? 0x10 : 0x20, u, v, f);
			break;
		}
		case 2: // C: the grey level steps (15 Hz), the scroll moves
		{
			int32_t u = GridCU(c);
			if (mv) u = lerp_i(u, GridCU(c + 1), num, den);
			GridDraw(m, 0x100, 0x20, u, -u, GridCFade(c));
			break;
		}
		default: // D
		{
			int32_t u = c * 2;
			if (mv) u = lerp_i(u, (c + 1) * 2, num, den);
			GridDraw(m, 0x200, 0x10, u, -u, 0x40);
			break;
		}
		}
	}

	// ---- lit sprite pools: memo record integrated to the next draw, half way; frame kept ----
	enum PoolDraw { DRAW_DEBRIS, DRAW_LIT, DRAW_LIT_MAC };
	static void PoolHeld(int pool, const void *owner, uint8_t *h, int kind, int steps, int how, int num, int den)
	{
		for (int i = 0; i < 150; i++)
		{
			for (int k = 0; k < 2; k++)
			{
				const PoolMemo &mm = g_pool_memo[pool][i][k];
				if (mm.tick != g_real_tick || mm.owner != owner) continue;
				Rec t = mm.r;
				if (!mm.died && !g_tick_paused)
					for (int s = 0; s < steps; s++) Integrate(kind, &t);
				int16_t pos[4] = { Lerp16(mm.r.x, t.x, num, den), Lerp16(mm.r.y, t.y, num, den), Lerp16(mm.r.z, t.z, num, den), mm.r.w0E };
				int16_t size = Lerp16(mm.r.size, t.size, num, den);
				if (how == DRAW_DEBRIS) DebrisDrawOne(h, pos, size);
				else LitSprite(h, pos, size, mm.r.age, how == DRAW_LIT_MAC);
			}
		}
	}

	static void DebrisHeld(const void *owner, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x68);
		DebrisSetupA(h, s);
		PoolHeld(0, owner, h, INT_FALL, 1, DRAW_DEBRIS, num, den);
		DebrisSetupB(h, s);
		for (int i = 0; i < 150; i++)
		{
			const PoolMemo &mm = g_pool_memo[1][i][0];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			SplashDrawOne(h, s, &mm.r); // standing splashes: the frame drawn
		}
		FieldFree(0x68);
		FieldFree(0xB4);
	}

	static void LitPoolHeld(const Mat4x3 *frame, const Node24 *n, int pool, uint32_t seq, int32_t ty, int kind, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 0x24) = 0;
		LitSetup(frame, s, 0, ty, 0x28);
		PoolHeld(pool, n, h, kind, 1, DRAW_LIT, num, den);
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void MistHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE108E8;
		*(uint32_t *)(h + 0x1C) = 0x404040;
		*(int16_t *)(h + 0x24) = 4;
		LitSetup(frame, s, m->p10, m->p12, m->p14); // the bone position of the last tick
		PoolHeld(0, n, h, INT_GROW5_DRAG8, 1, DRAW_LIT, num, den);
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void SparkHeld(const Mat4x3 *frame, const Node24 *n, bool two, int drivers, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		if (two) SparkSetup(frame, h, s, 0xE10E04, 0x202020);
		else SparkSetup(frame, h, s, 0xE108E8, 0x404040);
		PoolHeld(2, n, h, two ? INT_GROW6_DRAG8 : INT_GROW5_DRAG8, two ? drivers : 1, DRAW_LIT, num, den);
		FieldFree(0x50);
		FieldFree(0xB4);
	}

	static void BloodHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		BloodSetup(frame, h, s);
		PoolHeld(2, n, h, INT_BLOOD, 1, DRAW_LIT_MAC, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	// ---- node-driven models ----
	static void GlowHeld(const Mat4x3 *frame, const Node24 *n)
	{
		const Node24 *m = g_node_memo.get(n);
		if (m) GlowDraw(frame, m, m->c); // the scroll steps whole units: the one drawn
	}

	static void StreakHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t x = m->p10;
		if (NodeMoved(m, n) && !g_tick_paused) x = lerp_i(m->p10, n->p10, num, den);
		StreakDraw(m, x);
	}

	static void WallHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t s20 = m->s20, z = m->p14;
		if (NodeMoved(m, n) && !g_tick_paused)
		{
			s20 = lerp_i(m->s20, n->s20, num, den);
			z = lerp_i(m->p14, n->p14, num, den);
		}
		WallDraw(m, s20, z);
	}

	static void ShockHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t sx = m->s1C;
		if (NodeMoved(m, n) && !g_tick_paused) sx = lerp_i(m->s1C, n->s1C, num, den);
		ShockDraw(m, sx);
	}

	static void RingHeld(const Mat4x3 *frame, const Node24 *n, bool b, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n) && !g_tick_paused;
		int32_t c = m->c;
		int32_t sx = m->s1C, sy = m->s20;
		int32_t start = b ? 4 : 6;
		int32_t f = b ? RingBFade(c) : RingAFade(c);
		if (mv)
		{
			sx = lerp_i(m->s1C, n->s1C, num, den);
			sy = lerp_i(m->s20, n->s20, num, den);
			if (c >= start) f = lerp_i(f, b ? RingBFade(c + 1) : RingAFade(c + 1), num, den);
		}
		RingDraw(frame, m, sx, sy, c >= start, f);
	}

	static void FlashHeld(const Node24 *n, int num, int den)
	{
		const FlashMemo *m = g_flash_memo.get(n);
		if (!m || m->level < 0) return;
		int32_t level = m->level;
		if (!g_tick_paused)
		{
			int32_t next = FlashLevel(n->c, n->e, n->s1C);
			if (next >= 0) level = lerp_i(m->level, next, num, den);
		}
		ScreenTile(level, level, level, m->otz);
	}

	static void MorphHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t c = m->c;
		int32_t arg = MorphArg(c), f = mul32(c - 6, 682);
		if (NodeMoved(m, n) && !g_tick_paused)
		{
			arg = lerp_i(arg, MorphArg(c + 1), num, den);
			if (c >= 6) f = lerp_i(f, mul32(c - 5, 682), num, den);
		}
		MorphDraw(m, arg, c >= 6, f, (uint32_t)g_morph_verts);
	}

	static void TintHeld(const Node24 *n, int num, int den)
	{
		const TintMemo *m = g_tint_memo.get(n);
		const Node24 *mn = g_node_memo.get(n);
		if (!m || !mn) return;
		uint8_t rgb[4];
		memcpy(rgb, m->rgb, 4);
		if (NodeMoved(mn, n) && !g_tick_paused)
		{
			uint8_t next[4];
			TintColor(mn->c + 1, next);
			for (int k = 0; k < 3; k++) rgb[k] = (uint8_t)lerp_i(m->rgb[k], next[k], num, den);
		}
		ScreenTile(rgb[0], rgb[1], rgb[2], 0x1000);
	}

	static void BurstHeld(const Node24 *n, bool b, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t c = m->c;
		int32_t scroll = shl32(c, 4), fade = BurstFade(b, c);
		if (NodeMoved(m, n) && !g_tick_paused)
		{
			scroll = lerp_i(scroll, shl32(c + 1, 4), num, den);
			if (c >= (b ? 7 : 6)) fade = lerp_i(fade, BurstFade(b, c + 1), num, den);
		}
		BurstDraw(m, c, b, scroll, fade);
	}

	static void SlashHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		int32_t c = m->c;
		int32_t scale = m->s1C, f = SlashFade(c, m->s20, m->p16);
		if (NodeMoved(m, n) && !g_tick_paused)
		{
			scale = lerp_i(m->s1C, n->s1C, num, den);
			f = lerp_i(f, SlashFade(c + 1, m->s20, m->p16), num, den);
		}
		SlashDraw(m, scale, f);
	}

	// ---- the creature ----
	static void SliceHeld(const Mat4x3 *frame, int num, int den)
	{
		const SliceMemo &s = g_slice;
		uint8_t *E = Creature();
		uint8_t *sk = Skeleton(E);
		uint32_t size = sk ? 16 + 48 * (uint32_t)sk[0] : 0;
		if (!sk || size > SKEL_MAX) return;
		bool mv = !g_tick_paused;
		uint32_t color_cur = *(uint32_t *)(E + 0x28);
		memcpy(g_skel_cur, sk, size);
		if (!s.phase_b)
		{
			// halves drifting apart: 8c' - 8 -> 8c' (the switch to phase B at 200 is a cut)
			int32_t d = s.cc * 8 - 8;
			if (mv && s.cc + 1 < 0x1A) d = lerp_i(d, s.cc * 8, num, den);
			*(uint32_t *)(E + 0x28) = s.color;
			SliceADraw(E, d, frame, (uint32_t)g_scratch);
		}
		else
		{
			int16_t a = s.angle;
			uint32_t col = s.color;
			int32_t nc = s.cc + 1;
			if (mv && nc < 0x36)
			{
				if (nc >= 0x1E) a = lerp_angle(a, (int16_t)(a + 0x12), num, den);
				if (nc >= 0x2E)
				{
					uint32_t next = 0;
					DepthCue(SliceBFadeW(nc), 0x80, 0x80, 0x80, &next);
					col = LerpColor(col, next, num, den);
				}
			}
			SliceBDraw(E, s.cc, a, frame, (uint32_t)g_scratch, false, col);
		}
		*(uint32_t *)(E + 0x28) = color_cur;
		memcpy(sk, g_skel_cur, size);
	}

	static void CreatureHeld(const Mat4x3 *frame, int num, int den)
	{
		if (g_slice.tick == g_real_tick)
		{
			SliceHeld(frame, num, den);
			return;
		}
		const ModelMemo &m = g_creature;
		if (m.tick != g_real_tick) return;
		uint8_t *E = Creature();
		bool mv = !g_tick_paused;
		Mat4x3 mat = m.mat;
		if (mv && !m.hold_mat)
		{
			Mat4x3 next = CreatureMatNext();
			for (int k = 0; k < 3; k++) mat.t[k] = lerp_i(m.mat.t[k], next.t[k], num, den);
		}
		// the colour the next draw uses is already in place (the fade-in task runs after the creature)
		uint32_t color = mv ? LerpColor(m.color, *(uint32_t *)(E + 0x28), num, den) : m.color;
		ModelHeld(m, E, frame, mat, color, m.midpoint && mv, num, den);
	}

	static void BladeHeld(const Mat4x3 *frame, int num, int den)
	{
		if (g_blade.tick != g_real_tick) return;
		Mat4x3 e = g_blade.e, bone = g_blade.bone;
		if (!g_tick_paused)
		{
			LerpMatrix(&e, g_blade.e, CreatureMatNext(), num, den);
			LerpMatrix(&bone, g_blade.bone, BoneCopy(), num, den);
		}
		int16_t p0[4], p1[4];
		memcpy(p0, BladeP0(), 8);
		memcpy(p1, BladeP1(), 8);
		BladeDraw(frame, &e, &bone, p0, p1, g_blade.c > 0);
	}

	static void TrailHeld(const Mat4x3 *frame, const Node24 *n)
	{
		const TrailMemo *m = g_trail_memo.get(n);
		if (!m || !m->drawn) return; // the trail grows in 15 Hz steps: the one drawn, from the held camera
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		TrailDraw(frame, (uint32_t)m->a, (uint32_t)m->b, s);
		FieldFree(0x40);
	}

	// ---- model 2: ghost trail and its sword ----
	static bool g_m2_valid = false;
	static Mat4x3 g_m2_held;

	// the ghost's in-between matrix (turned half way, moved half the velocity); the echoes stay
	// where they were recorded
	static void GhostPrepare(int num, int den)
	{
		g_m2_valid = false;
		if (g_ghost.tick != g_real_tick) return;
		const Node24 &n = g_ghost.n;
		Mat4x3 m = g_ghost.mat;
		if (!g_tick_paused)
		{
			GhostPose(&m, lerp_angle(n.a18, (int16_t)(n.a18 + n.s1A), num, den));
			m.t[0] = (int32_t)((uint32_t)g_ghost.mat.t[0] + (uint32_t)lerp_i(0, n.p10, num, den));
			m.t[1] = (int32_t)((uint32_t)g_ghost.mat.t[1] + (uint32_t)lerp_i(0, n.p12, num, den));
			m.t[2] = (int32_t)((uint32_t)g_ghost.mat.t[2] + (uint32_t)lerp_i(0, n.p14, num, den));
		}
		g_m2_held = m;
		g_m2_valid = true;
	}

	static void GhostHeld(const Mat4x3 *frame)
	{
		if (!g_m2_valid) return;
		uint8_t *E = Model2();
		uint8_t *sk = Skeleton(E);
		uint32_t size = sk ? 16 + 48 * (uint32_t)sk[0] : 0;
		if (!sk || size > SKEL_MAX) return;
		Mat4x3 mat_cur = Model2Mat();
		uint32_t color_cur = var<uint32_t>(0x24F0F10);
		memcpy(g_skel_cur, sk, size);
		Model2Mat() = g_m2_held;
		DrawModel(E, frame, (uint32_t)g_scratch);
		GhostEchoes(E, g_ghost.k, frame, (uint32_t)g_scratch);
		var<uint32_t>(0x24F0F10) = color_cur;
		Model2Mat() = mat_cur;
		memcpy(sk, g_skel_cur, size);
	}

	static void SwordHeld(const Mat4x3 *frame, const Node24 *n)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		Mat4x3 mat_cur = Model2Mat(), bone_cur = BoneCopy();
		if (g_m2_valid) Model2Mat() = g_m2_held;
		SwordDraw(frame, &Model2Mat(), m->c > 0);
		BoneCopy() = bone_cur;
		Model2Mat() = mat_cur;
	}

	// ---- the fading models ----
	static void FadeHeld(const Mat4x3 *frame, const Node24 *n, uint8_t *E, const ModelMemo &memo, int num, int den)
	{
		const Node24 *mn = g_node_memo.get(n);
		if (!mn || memo.tick != g_real_tick) return;
		bool mv = NodeMoved(mn, n) && !g_tick_paused;
		uint32_t color = memo.color;
		if (mv && mn->c + 1 >= 0x13)
		{
			uint32_t next = 0;
			DepthCue(FadeW(mn->c + 1), 0x80, 0x80, 0x80, &next);
			color = LerpColor(memo.color, next, num, den);
		}
		ModelHeld(memo, E, frame, memo.mat, color, memo.midpoint && mv, num, den);
	}

	static uint8_t g_held_packets[0x80000];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the master's queue order; packets go to a private buffer (the module draws through the
	// engine cursor battle_texture_data_ptr_1D8E054, redirected and put back)
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = Cursor();
		Cursor() = (uint32_t)g_held_packets;
		bool timeline = false;
		int drivers = 0;
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			if ((uint32_t)t->func == ORIG_TimelineTask) timeline = true;
			if ((uint32_t)t->func == ORIG_Spark2Task) drivers++;
		}
		if (drivers < 1) drivers = 1;
		Mat4x3 frame;
		if (timeline) ComposeAffineTransform(&Camera(), &RootMatrix(), &frame);
		else frame = Frame();
		GhostPrepare(num, den);
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			const Node24 *n = (const Node24 *)t;
			switch ((uint32_t)t->func)
			{
			case ORIG_GridATask: GridHeld(n, 0, num, den); break;
			case ORIG_GridBTask: GridHeld(n, 1, num, den); break;
			case ORIG_GridCTask: GridHeld(n, 2, num, den); break;
			case ORIG_GridDTask: GridHeld(n, 3, num, den); break;
			case ORIG_DebrisTask: DebrisHeld(n, num, den); break;
			case ORIG_GlowTask: GlowHeld(&frame, n); break;
			case ORIG_StreakTask: StreakHeld(n, num, den); break;
			case ORIG_WallTask: WallHeld(n, num, den); break;
			case ORIG_ShockTask: ShockHeld(n, num, den); break;
			case ORIG_GhostTask: GhostHeld(&frame); break;
			case ORIG_SwordTask: SwordHeld(&frame, n); break;
			case ORIG_FadeATask: FadeHeld(&frame, n, Model2(), g_fade_a, num, den); break;
			case ORIG_FadeBTask: FadeHeld(&frame, n, Model3(), g_fade_b, num, den); break;
			case ORIG_TintTask: TintHeld(n, num, den); break;
			case ORIG_CreatureTask: CreatureHeld(&frame, num, den); break;
			case ORIG_SmokeTask: LitPoolHeld(&frame, n, 0, 0xE10FB0, 0, INT_DRAG4, num, den); break;
			case ORIG_RingATask: RingHeld(&frame, n, false, num, den); break;
			case ORIG_RingBTask: RingHeld(&frame, n, true, num, den); break;
			case ORIG_DustTask: LitPoolHeld(&frame, n, 1, 0xE108E8, -40, INT_XZ_DRAG8, num, den); break;
			case ORIG_MistTask: MistHeld(&frame, n, num, den); break;
			case ORIG_FlashTask: FlashHeld(n, num, den); break;
			case ORIG_MorphTask: MorphHeld(n, num, den); break;
			case ORIG_SparkTask: SparkHeld(&frame, n, false, 1, num, den); break;
			case ORIG_Spark2Task: SparkHeld(&frame, n, true, drivers, num, den); break;
			case ORIG_BladeTask: BladeHeld(&frame, num, den); break;
			case ORIG_TrailTask: TrailHeld(&frame, n); break;
			case ORIG_BurstATask: BurstHeld(n, false, num, den); break;
			case ORIG_BurstBTask: BurstHeld(n, true, num, den); break;
			case ORIG_BloodTask: BloodHeld(&frame, n, num, den); break;
			case ORIG_SlashATask:
			case ORIG_SlashBTask:
			case ORIG_SlashCTask:
			case ORIG_SlashDTask: SlashHeld(n, num, den); break;
			default: break;
			}
		}
		Cursor() = cursor;
	}
}

	void register_mag326_odin_reverse()
	{
		register_port(o326::ORIG_SequenceTick, (void *)o326::SequenceTick, "O326 SequenceTick", 326);
		register_port(o326::ORIG_TimelineTask, (void *)o326::TimelineTask, "O326 TimelineTask", 326);
		register_port(o326::ORIG_GridATask, (void *)o326::GridATask, "O326 GridATask", 326, true);
		register_port(o326::ORIG_GridBTask, (void *)o326::GridBTask, "O326 GridBTask", 326, true);
		register_port(o326::ORIG_DebrisTask, (void *)o326::DebrisTask, "O326 DebrisTask", 326, true);
		register_port(o326::ORIG_WarpTask, (void *)o326::WarpTask, "O326 WarpTask", 326);
		register_port(o326::ORIG_GlowTask, (void *)o326::GlowTask, "O326 GlowTask", 326, true);
		register_port(o326::ORIG_StreakTask, (void *)o326::StreakTask, "O326 StreakTask", 326, true);
		register_port(o326::ORIG_GridCTask, (void *)o326::GridCTask, "O326 GridCTask", 326, true);
		register_port(o326::ORIG_GridDTask, (void *)o326::GridDTask, "O326 GridDTask", 326, true);
		register_port(o326::ORIG_WallTask, (void *)o326::WallTask, "O326 WallTask", 326, true);
		register_port(o326::ORIG_ShockTask, (void *)o326::ShockTask, "O326 ShockTask", 326, true);
		register_port(o326::ORIG_GhostTask, (void *)o326::GhostTask, "O326 GhostTask", 326, true);
		register_port(o326::ORIG_SwordTask, (void *)o326::SwordTask, "O326 SwordTask", 326, true);
		register_port(o326::ORIG_FadeATask, (void *)o326::FadeATask, "O326 FadeATask", 326, true);
		register_port(o326::ORIG_FadeBTask, (void *)o326::FadeBTask, "O326 FadeBTask", 326, true);
		register_port(o326::ORIG_TintTask, (void *)o326::TintTask, "O326 TintTask", 326, true);
		register_port(o326::ORIG_EndTask, (void *)o326::EndTask, "O326 EndTask", 326);
		register_port(o326::ORIG_CreatureTask, (void *)o326::CreatureTask, "O326 CreatureTask", 326, true);
		register_port(o326::ORIG_SmokeTask, (void *)o326::SmokeTask, "O326 SmokeTask", 326, true);
		register_port(o326::ORIG_RingATask, (void *)o326::RingATask, "O326 RingATask", 326, true);
		register_port(o326::ORIG_RingBTask, (void *)o326::RingBTask, "O326 RingBTask", 326, true);
		register_port(o326::ORIG_DustTask, (void *)o326::DustTask, "O326 DustTask", 326, true);
		register_port(o326::ORIG_MistTask, (void *)o326::MistTask, "O326 MistTask", 326, true);
		register_port(o326::ORIG_FadeInTask, (void *)o326::FadeInTask, "O326 FadeInTask", 326);
		register_port(o326::ORIG_FlashTask, (void *)o326::FlashTask, "O326 FlashTask", 326, true);
		register_port(o326::ORIG_MorphTask, (void *)o326::MorphTask, "O326 MorphTask", 326, true);
		register_port(o326::ORIG_SparkTask, (void *)o326::SparkTask, "O326 SparkTask", 326, true);
		register_port(o326::ORIG_Spark2Task, (void *)o326::Spark2Task, "O326 Spark2Task", 326, true);
		register_port(o326::ORIG_BladeTask, (void *)o326::BladeTask, "O326 BladeTask", 326, true);
		register_port(o326::ORIG_TrailTask, (void *)o326::TrailTask, "O326 TrailTask", 326, true);
		register_port(o326::ORIG_BurstATask, (void *)o326::BurstATask, "O326 BurstATask", 326, true);
		register_port(o326::ORIG_BurstBTask, (void *)o326::BurstBTask, "O326 BurstBTask", 326, true);
		register_port(o326::ORIG_BloodTask, (void *)o326::BloodTask, "O326 BloodTask", 326, true);
		register_port(o326::ORIG_SlashATask, (void *)o326::SlashATask, "O326 SlashATask", 326, true);
		register_port(o326::ORIG_SlashBTask, (void *)o326::SlashBTask, "O326 SlashBTask", 326, true);
		register_port(o326::ORIG_SlashCTask, (void *)o326::SlashCTask, "O326 SlashCTask", 326, true);
		register_port(o326::ORIG_SlashDTask, (void *)o326::SlashDTask, "O326 SlashDTask", 326, true);
		register_module_held(326, o326::HeldReady, o326::HeldFrame);
	}
}
