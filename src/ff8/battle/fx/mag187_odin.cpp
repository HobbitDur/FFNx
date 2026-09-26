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

// Effect 187: Odin - Zantetsuken (timeline-B GF family, MAG_187_*).
//
// Structure (gf_study/gf_inventory_timeline.md section 3.2):
//   setup MAG_187_ODIN_SUMMON_ZANTETSUKEN (0x6472E0, not ported: runs once) - model buffer
//     mb = 0x20DFAB8 (dword 0x24FE8E0); sprite pools 0xE41E50 = mb+0x30000 (A), 0xE41E54 =
//     mb+0x31000 (B), 0xE41E58 = mb+0x32000 (C), 150 x 0x18 each; 0xE41E98 = mb+0x38000 (vertex
//     scratch of the model draw and of the morph flash); root queue 0x24FD6C0 (pool 0x24FD6A0,
//     2 x 0x10) with the master, sub-queue 0x24FD990 (pool 0x24FD9A0, 100 x 0x24) with the
//     timeline. Every other task goes into the sub-queue through au_re_BdLinkTask_98 0x647900 or
//     a spawner.
//   SequenceTick (master, 0x64DD50) - runs the sub-queue, ++counter, ends when it is empty.
//   TimelineTask (0x6474C0) - 289 ticks: camera script, streams, loads, sounds, screen flash,
//     party hide/show, spawns (grids 6, debris 10, intro 40, creature 52, target slices 208,
//     end 252), the action result at 287.
//   CreatureTask (0x649D00, spawned at 52) - 198 ticks, state = global 0x24FD8D8; spawns the
//     blade glow, the smoke/rings/dust (3), the mist (21), the fade-in (52), the sword sparks
//     (82, 106), the slash trails/strokes (121..155).
//   The timeline also queues the shared camera-script task MAG_066_sub_63E9C0 (Doomtrain file);
//   it is not part of this module and is not ported here. The module never writes the battle
//   camera itself.
// Every task tests battle_to_update_flags_dword_1D96A9C & 0x201 (draw-only when set), except the
// master, the end task 0x649CC0 and the blade's done test.
// Module-private renderers (pure functions of a scratch header: grid 0x647B10, clipped target
// model 0x648ED0, slash model 0x64C9A0, screen tile 0x64B280) and the target spawn-position helper
// 0x648970 are called through their original addresses.

#include "fx_port.h"

namespace ff8fx
{
namespace o187
{
	using namespace eng;

	// --- engine functions used by this module (original addresses) ---
	namespace x
	{
		inline int32_t CrtRand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); }
		inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
		inline void ComposeZYXRotationMatrix(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x56CE30)(angles, out); }
		inline void Scale3DMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); }
		inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); } // b.rot = a.rot * b.rot
		inline int32_t NormalizeVectorToFixedPoint(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); }
		inline void NormalizeVector16(const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const int16_t *, int16_t *)>(0x56BDE0)(in, out); } // sub_56BDE0 (float)
		inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
		inline uint32_t MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { return fn<uint32_t (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
		inline void TransformVectorBy3x3Matrix(const Mat4x3 *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int32_t *, int32_t *)>(0x56C600)(m, in, out); }
		inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
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
		inline void WaitAnimSeq(uint32_t buffer, void *flag) { fn<void (__cdecl *)(uint32_t, void *)>(0x508630)(buffer, flag); }                   // sub_508630
		inline int32_t LoadState() { return fn<int32_t (__cdecl *)()>(0x508500)(); }                                                        // sub_508500: < 0 = file load busy
		inline void CharacterLoad(int32_t id, uint32_t dst) { fn<void (__cdecl *)(int32_t, uint32_t)>(0x508480)(id, dst); }                // BattleFile_CharacterLoad
		inline void QueueTIMUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(0x505E30)(tim); }                                        // Battle_QueueTIMUpload_GetEOF
		inline void QueueVramUpload(uint32_t rect, const void *data) { fn<void (__cdecl *)(uint32_t, const void *)>(0x505DF0)(rect, data); } // Battle_QueueVramUpload_Type0_RectData
		inline uint32_t SummonData() { return fn<uint32_t (__cdecl *)()>(0x571B70)(); }                                                    // sub_571B70: 0x209FAB8
		inline uint32_t Mag151Draw(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x64E080)(h, ot, mode, cursor); } // MAG_151_sub_64E080
		// software GTE
		inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
		inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // MAG_148_sub_64DDA0: BK = m.t
		inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }
		inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }                 // MAG_163_sub_64DE00: R = s * I
		inline void GteTransFromIR() { fn<void (__cdecl *)()>(0x64DE90)(); }                                  // MAG_148_sub_64DE90: TR = IR1..3
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
		inline void GridRender(void *h, uint32_t ot, int32_t shift) { fn<void (__cdecl *)(void *, uint32_t, int32_t)>(0x647B10)(h, ot, shift); } // writes the cursor itself
		inline uint32_t SliceRender(void *geom, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x648ED0)(geom, h, ot, mode, cursor); }
		inline uint32_t SlashRender(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x64C9A0)(h, ot, mode, cursor); }
		inline void ScreenTile(int32_t r, int32_t g, int32_t b, int32_t otz) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x64B280)(r, g, b, otz); }
		inline void SpawnPosMid(void *entity, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x648970)(entity, out); } // midpoint of bones 0xF0/0xF1, [3] = entity+0x24
	}
	using namespace x;

	// --- module globals ---
	inline bool Paused() { return (var<uint32_t>(0x1D96A9C) & 0x201) != 0; } // battle_to_update_flags_dword_1D96A9C
	inline Mat4x3 &RootMatrix() { return var<Mat4x3>(0x24FE880); }  // effect frame (setup: rotation, centroid 0x24FE894/98/9C)
	inline Mat4x3 &Frame() { return var<Mat4x3>(0x24FE8A0); }       // Camera o RootMatrix, rebuilt by the timeline every tick
	inline TaskQueue &SubQueue() { return var<TaskQueue>(0x24FD990); }
	inline uint8_t *CastCtx() { return var<uint8_t *>(0x24FE7B0); }
	inline uint32_t &VoiceSlot() { return var<uint32_t>(0x24FD6D4); }
	inline uint8_t *Creature() { return (uint8_t *)0x24FD8D8; }       // +0x28 colour 0x24FD900, +0x40 model matrix 0x24FD918, +0x60 BattleAnimHeader, +0x6C BattleAnimCmd
	inline Mat4x3 &CreatureMat() { return var<Mat4x3>(0x24FD918); }
	inline Mat4x3 &BoneCopy() { return var<Mat4x3>(0x24FE8C0); }     // creature bone matrix (skeleton + 0xB30) copied by the blade task
	inline int16_t *BladeP0() { return (int16_t *)0x24FD980; }       // blade end points (effect frame), written by the blade task
	inline int16_t *BladeP1() { return (int16_t *)0x24FD988; }
	inline uint32_t &DoneFlags() { return var<uint32_t>(0x24FD50C); } // bit0 creature done
	inline uint32_t MB() { return var<uint32_t>(0x24FE8E0); }        // model buffer (0x20DFAB8)
	inline uint32_t &Cursor() { return var<uint32_t>(0x1D8E054); }   // battle_texture_data_ptr_1D8E054
	inline uint32_t OT() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline uint8_t *PoolA() { return var<uint8_t *>(0xE41E50); }     // mb + 0x30000
	inline uint8_t *PoolB() { return var<uint8_t *>(0xE41E54); }     // mb + 0x31000
	inline uint8_t *PoolC() { return var<uint8_t *>(0xE41E58); }     // mb + 0x32000
	inline uint8_t *Targets() { return *(uint8_t **)(CastCtx() + 4); } // +0x08 -> 0x18-byte records (byte 0 = slot, byte 3 bit 2 = skip), +0x10 u8 count
	inline uint8_t *Entity(uint32_t slot) { return (uint8_t *)(0x1D972C0 + 156 * slot); }
	static const uint32_t HIST_A = 0x24FD458;  // spline input points (8 bytes each)
	static const uint32_t HIST_B = 0x24FD4B0;  // spline input directions / second rail
	static const uint32_t SPLINE_A = 0x24FD6D8; // spline outputs (8 bytes each)
	static const uint32_t SPLINE_B = 0x24FD7D8;
	static const uint32_t TRAIL_RECS = 0x24FE7B8; // 10 x 0x14 blade trail history

	static const uint32_t ORIG_SequenceTick = 0x64DD50;
	static const uint32_t ORIG_TimelineTask = 0x6474C0;
	static const uint32_t ORIG_GridATask = 0x6479D0;
	static const uint32_t ORIG_GridBTask = 0x647E40;
	static const uint32_t ORIG_DebrisTask = 0x647F80;
	static const uint32_t ORIG_WarpTask = 0x648490;
	static const uint32_t ORIG_GlowTask = 0x6485C0;
	static const uint32_t ORIG_StreakTask = 0x6486F0;
	static const uint32_t ORIG_SliceTask = 0x648AF0;
	static const uint32_t ORIG_EndTask = 0x649CC0;
	static const uint32_t ORIG_CreatureTask = 0x649D00;
	static const uint32_t ORIG_SmokeTask = 0x64A370;
	static const uint32_t ORIG_RingATask = 0x64A740;
	static const uint32_t ORIG_RingBTask = 0x64A890;
	static const uint32_t ORIG_DustTask = 0x64A9D0;
	static const uint32_t ORIG_MistTask = 0x64ACB0;
	static const uint32_t ORIG_FadeInTask = 0x64B0A0;
	static const uint32_t ORIG_FlashTask = 0x64B130;
	static const uint32_t ORIG_MorphTask = 0x64B300;
	static const uint32_t ORIG_SparkTask = 0x64B640;
	static const uint32_t ORIG_Spark2Task = 0x64BB90;
	static const uint32_t ORIG_BladeTask = 0x64C060;
	static const uint32_t ORIG_TrailTask = 0x64C330;
	static const uint32_t ORIG_SlashATask = 0x64C7F0;
	static const uint32_t ORIG_SlashBTask = 0x64D6A0;
	static const uint32_t ORIG_SlashCTask = 0x64D930;
	static const uint32_t ORIG_SlashDTask = 0x64DAD0;
	static const uint32_t ORIG_HideTask = 0x64DCF0;

	// every sub-queue node is 0x24 bytes (pool 0x24FD9A0); field use differs per task
#pragma pack(push, 1)
	struct Node24
	{
		TaskNode hdr;
		int16_t c;                  // +0x0C tick counter
		int16_t e;                  // +0x0E delay / target slot / bone / model index / end task done flag
		int16_t p10, p12, p14, p16; // +0x10 position (p16: pad or base fade)
		int16_t a18;                // +0x18 angle / phase / history table / battle slot
		int16_t s1A;                // +0x1A saved entity flag
		int16_t s1C, s1E;           // +0x1C scale, its speed
		int16_t s20, s22;           // +0x20 second scale / length, its speed / delay
	};
	// sprite record of the pools A/B/C
	struct Rec
	{
		uint32_t alive;       // +0x00 owner bits (A: 1 debris, 2 smoke, 4 mist; B: 1 splash, 2 dust; C: 1 sparks, 2 sparks 2)
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

}
}

#ifdef FF8_FX_HELD
#include "mag187_odin_held.h"
#endif

namespace ff8fx
{
namespace o187
{
	inline Rec *RecOf(uint8_t *pool, int i) { return (Rec *)(pool + 0x18 * i); }

	// first free record (dword 0) of a pool, -1 when none
	static int FreeRecord(uint8_t *pool)
	{
		for (int i = 0; i < 150; i++)
			if (*(uint32_t *)(pool + 0x18 * i) == 0) return i;
		return -1;
	}

	// au_re_BdLinkTask_98 0x647900 (no null check, as the original)
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

	// ------------------------------------------------------------------
	// Master (0x64DD50): node from pool 0x24FD6A0, +0x0C counter (increments even when paused)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SequenceTick(TaskNode *tn)
	{
		// 30 fps layer: see mag187_odin_held.inc
		FX_HELD(held_note_master();)
		int left = ExecuteTaskQueue(&SubQueue());
		((Node24 *)tn)->c++;
		return left ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Timeline (0x6474C0), 289 ticks. Load waits (sub_508500 < 0) repeat the tick.
	// ------------------------------------------------------------------
	// MAG_187_sub_647920 / 647940: stru_1D9898C[0..3].currentBsId bit 1 (hide / show the party)
	static void HideParty() { for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) *(uint8_t *)a |= 2; }
	static void ShowParty() { for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) *(uint8_t *)a &= 0xFD; }

	// MAG_187_sub_647960 (tick 6): the two ground grids
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

	// MAG_187_sub_6483B0 (tick 40): texture warp, glow, two screen streaks
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

	// MAG_187_sub_648820 (tick 208): one slice task per target not flagged (record byte 3 bit 2),
	// placed at a random point around the target's middle (range from its entity +0x26)
	static void SpawnSlices()
	{
		uint8_t *t = Targets();
		for (uint32_t i = 0; i < t[0x10]; i++, t = Targets())
		{
			uint8_t *rec = *(uint8_t **)(t + 8) + 0x18 * i;
			if (rec[3] & 4) continue;
			uint32_t slot = rec[0];
			Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SliceTask);
			n->c = 0;
			n->e = (int16_t)slot;
			SpawnPosMid(Entity(slot), &n->p10);
			int32_t range = *(int16_t *)(Entity(slot) + 0x26);
			int32_t lim = range;
			int32_t half = mul32(CrtRand() % 0x100 + 0x80, range) >> 12;
			int32_t span = half * 2;
			n->p10 = (int16_t)(n->p10 + (int16_t)(CrtRand() % span - half));
			n->p12 = (int16_t)(n->p12 + (int16_t)(CrtRand() % span - half));
			n->p14 = (int16_t)(n->p14 + (int16_t)(CrtRand() % span - half));
			int32_t a = CrtRand() % 0x500;
			n->s1C = 0;
			n->a18 = (int16_t)(a - 0x280);
			if (lim > 0xA00) lim = 0xA00;
			int32_t v = CrtRand() % 0x30 + 0x30;
			n->s1E = (int16_t)(mul32(v, lim) >> 12);
		}
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
			CameraScriptStart(0xE41F28, &RootMatrix(), Creature(), &SubQueue(), 0);
			BdTransSummonStream(0xFA54A4, 0x24FD6D0);
			VoiceSlot() = ClaimVoiceSlot((const void *)0xE41CE8, 1, 0x80);
		}
		int32_t c = n->c;
		if (c < 12)
		{
			if (c == 1)
			{
				BdPlaySE(0xE41E40, 0, 0x80);
				CharacterLoad(0x225, MB() + 0x14000);
			}
			else if (c == 6) SpawnGrids();
			else if (c == 10) LinkTask(ORIG_DebrisTask);
		}
		else if ((c -= 12) < 28) // 12..39
		{
			if (c == 1)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0x14000);
				CharacterLoad(0x226, MB());
			}
			else if (c == 10)
			{
				if (LoadState() < 0) return 0;
				CharacterLoad(0x227, SummonData());
			}
		}
		else if ((c -= 28) < 23) // 40..62
		{
			if (c == 16) Stream();
			else if (c == 0) SpawnIntro();
			else if (c == 12) LinkTask(ORIG_CreatureTask);
		}
		else if ((c -= 23) >= 28) // >= 91
		{
			c -= 28;
			if (c < 25) // 91..115
			{
				if (c == 20) BdPlaySE(0xE41E44, 0, 0x80);
			}
			else if ((c -= 25) < 35) // 116..150
			{
				if (c == 17) BdTransSummonStream(SummonData(), 0x24FD6D0);
				else if (c == 22) Stream();
				else if (c == 1)
				{
					if (LoadState() < 0) return 0;
					CharacterLoad(0x228, MB() + 0x14000);
				}
				else if (c == 11)
				{
					if (LoadState() < 0) return 0;
					QueueTIMUpload(MB() + 0x14000);
				}
				else if (c == 25) CharacterLoad(0x229, SummonData());
			}
			else if ((c -= 35) < 20) // 151..170
			{
				if (c == 12)
				{
					if (LoadState() < 0) return 0;
					BdTransSummonStream(SummonData(), 0x24FD6D0);
				}
				else if (c == 19)
				{
					if (var<uint8_t>(0x24FD6D0)) CharacterLoad(0x22A, SummonData());
				}
				else if (c == 17) BdPlaySE(0xE41E48, 0, 0x80);
			}
			else if ((c -= 20) < 12) { if (c == 2) Stream(); }      // 171..182
			else if ((c -= 12) < 12) { if (c == 2) Stream(); }      // 183..194
			else if ((c -= 12) < 13) { if (c == 2) Stream(); }      // 195..207
			else if ((c -= 13) < 43)                                 // 208..250
			{
				if (c == 0)
				{
					if (LoadState() < 0) return 0;
					BdTransSummonStream(SummonData(), 0x24FD6D0);
					SpawnSlices();
				}
				else if (c == 4) Stream();
			}
			else if ((c -= 43) < 30) { if (c == 1) LinkTask(ORIG_EndTask); } // 251..280
			else if ((c -= 30) < 8)                                           // 281..288
			{
				if (c == 5)
				{
					if (VoiceSlotBusy(VoiceSlot())) ReleaseVoiceSlot(VoiceSlot());
				}
			}
		}
		// screen flash in over 0..5, party shown at 6, hidden with a flash at 40, flash out 273..
		int16_t cc = n->c;
		if (cc < 6) SetScreenFlash((uint32_t)(682 * (int32_t)cc), 0);
		else if (cc == 6) ShowParty();
		else if (cc == 0x28)
		{
			HideParty();
			SetScreenFlash(0xA00, 0);
		}
		else if (cc >= 0x111) SetScreenFlash((uint32_t)((0x121 - (int32_t)cc) * 160), 0);
		if (n->c == 0x11F)
		{
			uint8_t *t = Targets();
			ApplyActionResultToTargets(*(uint32_t *)(t + 8), t[0x10]);
		}
		n->c++;
		if (n->c < 0x121) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// End of effect (0x649CC0, timeline tick 252): starts the anim-seq wait at tick 1, ends when
	// +0x0E is set. No pause test (as the original).
	// ------------------------------------------------------------------
	static uint32_t __cdecl EndTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 1) WaitAnimSeq(MB() + 0x14000, &n->e);
		uint16_t done = (uint16_t)n->e;
		n->c++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ground grids (0x6479D0 / 0x647E40, timeline tick 6), 54 ticks: a plane of the grid renderer
	// MAG_187_sub_647B10 in world space (camera only), scrolling, fading in and out.
	// ------------------------------------------------------------------
	static int32_t GridAFade(int32_t c) { return c < 8 ? c * 8 : (c >= 0x2E ? (0x36 - c) * 8 : 0x40); }
	static int32_t GridBFade(int32_t c) { return c < 8 ? c * 8 : (c >= 0x26 ? (0x36 - c) * 4 : 0x40); }

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
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x36 ? TASK_END : 0;
	}

	static uint32_t __cdecl GridBTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		GridDraw(n, 0x200, 0x10, c * 2, c * 2, GridBFade(c));
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x36 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Lit flipbook sprites (pools A/B/C): the record position goes through the GTE light matrix
	// (+BK = the matrix translation), the result is the sprite's translation, the rotation a
	// uniform scale.
	// ------------------------------------------------------------------
	static void LitSprite(uint8_t *h, const int16_t *pos, int16_t size, int16_t frame)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int16_t *)(h + 4) = frame;
		GteTransFromIR();
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	// update rules of the records
	enum Integrator { INT_NONE, INT_FALL, INT_DRAG4, INT_GROW5_DRAG8, INT_XZ_DRAG8, INT_GROW6_DRAG8 };
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
		default:
			break;
		}
	}

	// ------------------------------------------------------------------
	// Debris (0x647F80, timeline tick 10): pool A (mask 1) falling rocks drawn in world space (the
	// camera as light matrix, flipbook 0xE3CFD0 frame 0, sequence scale 2 * size + 0x1200); on
	// landing (y >= 0) a rock becomes a pool B (mask 1) splash (flipbook 0xE3CFF8 on a tilted
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
		*(uint32_t *)h = 0xE3CFD0;
		*(int16_t *)(h + 4) = 0;
		*(int16_t *)(h + 0x24) = 2;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static void DebrisSetupB(uint8_t *h, uint8_t *s)
	{
		*(uint32_t *)h = 0xE3CFF8;
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
			FX_HELD(PoolMemo *pm = MemoRecord(0, i, n, r);)
			if (Paused()) continue;
			r->y = (int16_t)(r->y + r->vy);
			r->vy = (int16_t)(r->vy + (int16_t)(r->vy >> 5));
			if (r->y < 0)
			{
				count++;
				continue;
			}
			r->alive = 0;
			FX_HELD(MemoDied(pm);)
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
			FX_HELD(PoolMemo *pm = MemoRecord(1, i, n, r);)
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				FX_HELD(MemoDied(pm);)
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
	// Texture warp (0x648490, timeline tick 40), 27 ticks, draws nothing: rebuilds the 128 x 128
	// 8-bit texture at 0x24FE8E8 + 0x4000 (0xFA94A8, exe data) from the one at 0xFA54A8 with a
	// sine ripple and queues its VRAM upload (rect 0xE41EA0) - every tick, paused or not.
	// ------------------------------------------------------------------
	static uint32_t __cdecl WarpTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *src = var<uint8_t *>(0x24FE8E8);
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
		QueueVramUpload(0xE41EA0, dst);
		if (Paused()) return 0;
		n->a18 = (int16_t)(n->a18 + 0x20);
		n->c++;
		return n->c > 0x1A ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Glow (0x6485C0, timeline tick 40), 26 ticks: MAG_151_sub_64E080 model 0xE40848 at the
	// effect origin, scale 0xB00, fade value 0x400 (mode 0xF3), texture scroll +-counter.
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
		uint8_t *h = (uint8_t *)FieldAlloc(0x7C);
		*(uint32_t *)h = 0xE40848;
		*(uint16_t *)(h + 0x1C) = 0x80;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(int32_t *)(h + 0x14) = c;
		*(int32_t *)(h + 0xC) = 0x400;
		*(int32_t *)(h + 0x10) = -c;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x20) = 0xF3;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1A) = 0;
		Cursor() = Mag151Draw(h, OT(), 2, Cursor());
		FieldFree(0x7C);
	}

	static uint32_t __cdecl GlowTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		GlowDraw(&Frame(), n, n->c);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x1A ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Screen streaks (0x6486F0 x 2, timeline tick 40): after a 1-tick delay, prim 0xE40160 in
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
		*(uint32_t *)h = 0xE40160;
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
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->p10 = (int16_t)(n->p10 + n->s1E);
		n->c++;
		return n->c >= 0x17 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Target slice (0x648AF0, timeline tick 208, one per target), 82 ticks. Ticks 0-29 the
	// target entity is flagged (+0 |= 8); 30-79 it is hidden from the normal pass (|= 4) and
	// drawn here in two halves by the clipped renderer MAG_187_sub_648ED0: the plane through the
	// node position (normal = the slice angle), one half at the entity matrix, the other shifted
	// along the plane by +0x1C (speed +0x1E: x3 for 4 ticks, then decaying by 1/10, >= 2); from
	// tick 65 the target colour fades to the background colour (GTE DPCS).
	// ------------------------------------------------------------------
	// MAG_187_sub_648DE0: plane distance, n[3] = -(p . n) >> 12
	static void PlaneDistance(const int16_t *p, int16_t *nrm)
	{
		int32_t d = mul32(p[2], nrm[2]) + mul32(p[1], nrm[1]);
		d = d + mul32(p[0], nrm[0]);
		nrm[3] = (int16_t)-(d >> 12);
	}

	// MAG_187_sub_648E20: shadow, both root matrices, the clipped model, the bone matrices
	static void TargetDraw(uint8_t *h, uint8_t *ent, const Mat4x3 *moved)
	{
		if (!(ent[0] & 0x20))
			Cursor() = DrawShadow(ent, var<uint32_t>(0x1D8E04C) + 0x4064, 0x10, Cursor());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(ent + 0x40), (Mat4x3 *)(h + 0x190));
		ComposeAffineTransform(&Camera(), moved, (Mat4x3 *)(h + 0x1B0));
		*(uint32_t *)(h + 4) = var<uint32_t>(0x1D98B3C);
		*(uint32_t *)(h + 0x18) = *(uint32_t *)(ent + 0x28);
		*(uint32_t *)(h + 0x10) = *(uint32_t *)(ent + 0x7C);
		Cursor() = SliceRender(*(void **)(ent + 0x64), h, OT(), 2, Cursor());
		BuildBoneMatricesFromPose(ent + 0x60);
	}

	static void SliceDraw(uint8_t *h, const Node24 *n, uint8_t *ent, int32_t offset, int32_t c, bool recolor)
	{
		int16_t v[4] = { 0, 0, n->a18, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(v, &m);
		*(uint32_t *)(h + 0x40) = *(const uint32_t *)&n->p10;
		*(uint32_t *)(h + 0x44) = *(const uint32_t *)&n->p14;
		int16_t *nrm = (int16_t *)(h + 0xA0);
		nrm[0] = 0;
		nrm[1] = (int16_t)0xF000;
		nrm[2] = 0;
		MatrixMultiplyVector(&m, nrm, nrm);
		PlaneDistance((const int16_t *)(h + 0x40), nrm);
		v[0] = 0x1000;
		v[1] = 0;
		v[2] = 0;
		GteMatrixMultiply((const Mat4x3 *)(ent + 0x40), &m);
		MatrixMultiplyVector(&m, v, v);
		int32_t d = offset;
		if (v[1] < 0) d = -d;
		Mat4x3 moved = *(const Mat4x3 *)(ent + 0x40);
		moved.t[0] = (int32_t)((uint32_t)moved.t[0] + (uint32_t)(mul32(v[0], d) >> 12));
		moved.t[1] = (int32_t)((uint32_t)moved.t[1] + (uint32_t)(mul32(v[1], d) >> 12));
		moved.t[2] = (int32_t)((uint32_t)moved.t[2] + (uint32_t)(mul32(v[2], d) >> 12));
		if (recolor && c >= 0x41) // the target colour fade (from tick 65)
		{
			int32_t t = ComputeSin(shl32(c - 0x41, 10) / 16);
			int32_t w = 0x1000 - t;
			uint32_t k = (uint32_t)(shl32(w, 7) >> 12);
			uint32_t col = ((((k | 0x3200u) << 8) | k) << 8) | k;
			*(uint32_t *)(ent + 0x2C) = col;
			GteSetFarColor(var<uint8_t>(0xB8B7D8), var<uint8_t>(0xB8B7D9), var<uint8_t>(0xB8B7DA));
			// the original zeroes the dword above its only argument ([esp + 0x4C], outside its
			// frame) and passes its address: a zero RGBC
			uint32_t zero = 0;
			GteLoadRGBC(&zero);
			GteSetIR0(w);
			GteDPCS();
			GteStoreRGB2(ent + 0x28);
			ent[0x2B] = 2;
			ent[7] = 0;
		}
		TargetDraw(h, ent, &moved);
	}

	static uint32_t __cdecl SliceTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		if (c < 30) Entity((uint32_t)(int32_t)n->e)[0] |= 8;
		else if (c - 30 < 50)
		{
			uint8_t *ent = Entity((uint32_t)(int32_t)n->e);
			ent[0] |= 4;
			uint8_t *h = (uint8_t *)FieldAlloc(0x1D0);
			SliceDraw(h, n, ent, n->s1C, c, true);
			FieldFree(0x1D0);
			FX_HELD(MemoNode(n);)
		}
		if (Paused()) return 0;
		int16_t cc = n->c;
		if (cc >= 30)
		{
			if (cc < 34) n->s1C = (int16_t)(n->s1C + (int16_t)(n->s1E * 3));
			else
			{
				int16_t v = n->s1E;
				n->s1C = (int16_t)(n->s1C + v);
				int16_t nv = (int16_t)(v - (int16_t)((int32_t)v / 10));
				n->s1E = nv < 2 ? (int16_t)2 : nv;
			}
		}
		n->c = (int16_t)(cc + 1);
		return n->c > 0x51 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Creature (0x649D00, timeline tick 52), 198 ticks. State E = 0x24FD8D8 (0x9C bytes):
	//   +0x00 u16 flags (3: bit0 = no anim loop), +0x28 colour (0x24FD900, faded in by 0x64B0A0),
	//   +0x40 model matrix 0x24FD918 (t 0x24FD92C/930/934), +0x60 BattleAnimHeader, +0x64
	//   comFileData, +0x6C BattleAnimCmd, +0x7C, +0x84 model data 0x24FE8F0.
	//   0-11    anim 0 (scale 2), draw then advance; smoke, rings, dust at 3
	//   12-39   anim 1 (scale 1), advance then draw; mist at 21
	//   40-52   anim 2 (read twice at 40), advance then draw; fade-in/flash/morph at 52
	//   53-64   draw only
	//   65-107  anim 3, draw then advance; sparks 82, sparks 2 at 106
	//   108-119 anim 4, draw, then t.z += 90 (108-111), advance, spawn position (bone 0) -> node +0x10
	//   120-154 anim 5 (x3: 120, 132, 144), strokes at +1, trail + shake at +3; 131 / 143 hide the
	//           targets (1 tick), draw only
	//   155-167 draw only (155: hide targets 2 ticks + slash lines; 157: cut, anim 6 frame 0)
	//   168-197 draw then advance
	// Node: +0x10 spawn position (108-119).
	// ------------------------------------------------------------------

	// MAG_187_sub_64A340: rotation part = identity (translation and pad kept)
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

	// MAG_187_sub_64A1D0: bind the model data (model buffer offsets table) to the state
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

	// au_re_Battle_ReadAnimation_5 0x64A240
	static void SetAnim(int32_t id) { PreReadAnimation(Creature() + 0x60, Creature() + 0x6C, id); }

	// GF_187Odin_AdvanceModelAnimLoop 0x64A260
	static void AdvanceModelAnimLoop(uint8_t *E)
	{
		if (ReadAnimation(E + 0x60, E + 0x6C) == 1 && !(E[0] & 1))
			PreReadAnimation(E + 0x60, E + 0x6C, E[0x6C]);
	}

	// GF_187Odin_DrawModel 0x64A2A0 (scratch = RenderGeometry's vertex scratch, dword 0xE41E98)
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

	// MAG_187_sub_64A690 (creature tick 3): two expanding rings (prim 0xE41140)
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

	// MAG_187_sub_64B040 (creature tick 52): creature fade-in, screen flash, morph flash
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

	// MAG_187_sub_64B610 (creature tick 82): sword sparks, history 0x24FD510 cleared
	static void SpawnSparks()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SparkTask);
		n->c = 0;
		for (uint32_t a = 0x24FD510; a < 0x24FD54C; a += 0x14) *(uint32_t *)a = 0;
	}

	// MAG_187_sub_64BB20 (creature tick 106): two spark tasks on bones 0x1E / 0x34, histories cleared
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
		for (uint32_t a = TRAIL_RECS; a < 0x24FE880; a += 0x14) *(uint32_t *)a = 0;
		for (uint32_t k = 0; k < 0x3C; k += 0x14)
		{
			*(uint32_t *)(0x24FD510 + k) = 0;
			*(uint32_t *)(0x24FD5D8 + k) = 0;
		}
	}

	// MAG_187_sub_64C300 (creature ticks 123/135/147): the blade trail, its history cleared
	static void SpawnTrail()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_TrailTask);
		n->c = 0;
		for (uint32_t a = TRAIL_RECS; a < 0x24FE880; a += 0x14) *(uint32_t *)a = 0;
	}

	// MAG_187_sub_64D850 (creature ticks 121/133/145, k = 3/4/5): one stroke (0x64D930) and four
	// shorter echoes (0x64DAD0) from the table 0xE41EF8 (x, y, delay, model)
	static void SpawnStrokes(int32_t k)
	{
		const int16_t *t = (const int16_t *)(0xE41EF8 + 8 * k);
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SlashCTask);
		n->e = t[3];
		int16_t z = t[2];
		n->p10 = t[0];
		n->c = 0;
		n->p12 = t[1];
		n->p14 = z;
		n->p16 = 0;
		n->s1C = 0x2E80;
		n->s1E = 0x1740;
		n->s20 = (int16_t)(0xE - z);
		n->s22 = z;
		for (int i = 0; i < 4; i++)
		{
			Node24 *u = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SlashDTask);
			u->e = n->e;
			*(uint32_t *)&u->p10 = *(const uint32_t *)&n->p10;
			*(uint32_t *)&u->p14 = *(const uint32_t *)&n->p14;
			u->s1C = n->s1C;
			u->s1E = n->s1E;
			int16_t cc = (int16_t)(i + u->p14);
			u->c = 0;
			u->p14 = cc;
			u->p16 = 0x800;
			u->s20 = (int16_t)(0xE - cc);
			u->s22 = cc;
		}
	}

	// MAG_187_sub_64C710 (creature tick 155): three slash lines (0x64C7F0) and two echoes each
	// (0x64D6A0) from the table 0xE41EF8 entries 0-2
	static void SpawnSlashLines()
	{
		for (uint32_t e = 0xE41EF8; e < 0xE41F10; e += 8)
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
			n->s1C = 0x2DC0;
			n->s1E = 0xF40;
			n->s20 = (int16_t)(0x1E - z);
			n->s22 = z;
			for (int i = 0; i < 2; i++)
			{
				Node24 *u = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SlashBTask);
				u->e = n->e;
				*(uint32_t *)&u->p10 = *(const uint32_t *)&n->p10;
				*(uint32_t *)&u->p14 = *(const uint32_t *)&n->p14;
				u->s1C = n->s1C;
				u->s1E = n->s1E;
				int16_t cc = (int16_t)(i + u->p14);
				u->c = 0;
				u->p14 = cc;
				u->p16 = 0x800;
				u->s20 = (int16_t)(0x1E - cc);
				u->s22 = cc;
			}
		}
	}

	// MAG_187_sub_64DC70 (creature ticks 131/143 k = 1, 155 k = 2): hide every target (entity
	// flags |= 8) for k ticks; the task 0x64DCF0 restores bit 3 as it was
	static void HideTargets(int32_t k)
	{
		for (uint32_t i = 0; i < Targets()[0x10]; i++)
		{
			uint32_t slot = (*(uint8_t **)(Targets() + 8))[0x18 * i];
			Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_HideTask);
			n->a18 = (int16_t)slot;
			n->c = 0;
			n->e = (int16_t)k;
			uint16_t *flags = (uint16_t *)Entity(slot);
			uint16_t f = *flags;
			*flags = (uint16_t)(f | 8);
			n->s1A = (int16_t)(f & 8);
		}
	}

	static void Draw(uint8_t *E)
	{
		DrawModel(E, &Frame(), var<uint32_t>(0xE41E98));
		// 30 fps layer: see mag187_odin_held.inc
		FX_HELD(CreatureMemoTake();)
	}

	static uint32_t __cdecl CreatureTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *E = Creature();
		if (n->c == 0)
		{
			ResetRotation((void *)0x24FD918);
			var<uint16_t>(0x24FD928) = 0x2000;
			var<uint16_t>(0x24FD920) = 0x2000;
			var<uint16_t>(0x24FD918) = 0x2000;
			var<int32_t>(0x24FD930) = 0;
			var<int32_t>(0x24FD92C) = 0;
			var<int32_t>(0x24FD934) = 0x708;
			ModelInit(E, (uint8_t *)0x24FE8F0, (uint8_t *)MB(), 0xF);
			E[0] |= 1;
			LinkTask(ORIG_BladeTask);
		}
		int32_t c = n->c;
		if (c < 12)
		{
			if (Paused()) goto draw_advance;
			if (c == 0) SetAnim(0);
			else if (c == 3)
			{
				LinkTask(ORIG_SmokeTask);
				SpawnRings();
				LinkTask(ORIG_DustTask);
			}
			goto draw_advance;
		}
		if ((c -= 12) < 28) // 12..39
		{
			var<uint16_t>(0x24FD928) = 0x1000;
			var<uint16_t>(0x24FD920) = 0x1000;
			var<uint16_t>(0x24FD918) = 0x1000;
			if (Paused()) goto draw;
			if (c == 0) SetAnim(1);
			else if (c == 9) LinkTask(ORIG_MistTask);
			goto advance_draw;
		}
		if ((c -= 28) < 13) // 40..52
		{
			if (Paused()) goto draw;
			if (c == 0)
			{
				SetAnim(2);
				AdvanceModelAnimLoop(E);
			}
			else if (c == 12) SpawnFadeIn();
			goto advance_draw;
		}
		if ((c -= 13) < 12) goto draw; // 53..64
		if ((c -= 12) < 43)             // 65..107
		{
			if (Paused()) goto draw_advance;
			if (c == 0) SetAnim(3);
			else if (c == 17) SpawnSparks();
			else if (c == 41) SpawnSparks2();
			goto draw_advance;
		}
		if ((c -= 43) < 12) // 108..119
		{
			if (!Paused() && c == 0) SetAnim(4);
			Draw(E);
			if (Paused()) goto end;
			if (c < 4) var<int32_t>(0x24FD934) += 0x5A;
			AdvanceModelAnimLoop(E);
			GetEffectSpawnPosition(E, 0, 0, &n->p10);
			goto end;
		}
		if ((c -= 12) < 11) // 120..130
		{
			if (Paused()) goto draw_advance;
			if (c == 0)
			{
				var<int32_t>(0x24FD92C) = 0;
				var<int32_t>(0x24FD930) = 0;
				var<int32_t>(0x24FD934) = n->p14 - 100;
				SetAnim(5);
			}
			else if (c == 3)
			{
				SpawnTrail();
				CameraShake(0, 1, 2, 0xFF);
			}
			else if (c == 1) SpawnStrokes(3);
			goto draw_advance;
		}
		c -= 11;            // c - 131
		if (c < 1) goto hide_one;
		if ((c -= 1) < 11) // 132..142
		{
			if (Paused()) goto draw_advance;
			if (c == 0) SetAnim(5);
			else if (c == 3)
			{
				SpawnTrail();
				CameraShake(0, 1, 2, 0xFF);
			}
			else if (c == 1) SpawnStrokes(4);
			goto draw_advance;
		}
		if ((c -= 11) < 1) goto hide_one; // 143
		if ((c -= 1) < 11)                // 144..154
		{
			if (Paused()) goto draw_advance;
			if (c == 0) SetAnim(5);
			else if (c == 3)
			{
				SpawnTrail();
				CameraShake(0, 1, 2, 0xFF);
			}
			else if (c == 1) SpawnStrokes(5);
			goto draw_advance;
		}
		if ((c -= 11) < 2) // 155..156
		{
			if (Paused() || c != 0) goto draw;
			HideTargets(2);
			SpawnSlashLines();
			goto draw;
		}
		if ((c -= 2) < 11) // 157..167
		{
			if (Paused() || c != 0) goto draw;
			var<int32_t>(0x24FD930) = 0x2BC;
			var<int32_t>(0x24FD934) = (int32_t)0xFFFFDD3C;
			var<int32_t>(0x24FD92C) = mul32(var<int32_t>(0x24FBE78), -1900);
			SetAnim(6);
			goto draw;
		}
		if ((c -= 11) < 20) goto draw_advance; // 168..187
		if ((c -= 20) < 10) goto draw_advance; // 188..197
		goto end;

	hide_one: // 131, 143
		if (Paused() || c != 0) goto draw;
		HideTargets(1);
		goto draw;

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
		FX_HELD(held_note_creature_next(n);)
		if (n->c < 0xC6) return 0;
		DoneFlags() |= 1;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Creature fade-in (0x64B0A0, creature tick 52), draws nothing: creature colour = 12c - 12
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
		var<uint32_t>(0x24FD900) = Rgb(v);
		if (Paused()) return 0;
		n->c = (int16_t)(c + 1);
		if (n->c < 0xB) return 0;
		var<uint32_t>(0x24FD900) = 0x808080;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Screen flash (0x64B130, creature tick 52): a full-screen additive tile (MAG_187_sub_64B280)
	// at the depth of a point 200 in front of the look-at point; +0x0E packs the in / hold / out
	// lengths (nibbles 0 / 1 / 2), +0x1C the level. Ends when the level runs out.
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

	static uint32_t __cdecl FlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t otz = FlashDepth();
		int32_t level = FlashLevel(n->c, n->e, n->s1C);
		if (level >= 0) ScreenTile(level, level, level, otz);
		FX_HELD(held_note_flash(n, level, otz);)
		if (Paused()) return 0;
		n->c++;
		return level < 0 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Morph flash (0x64B300, creature tick 52), 12 ticks: prim 0xE3EFF0 billboarded 100 in front
	// of the look-at point (camera space), its 126 vertices morphed from the model's own (0xE3EFF8)
	// to the set 0xE3FD70 by sin(c * 1024 / 12) into the scratch dword 0xE41E98, drawn twice;
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
		*(uint32_t *)h = 0xE3EFF0;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x2033;
		if (fade)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F3;
			*(int32_t *)(h + 0xC) = fade_value;
		}
		int32_t w = ComputeSin(sin_arg);
		const int16_t *a = (const int16_t *)0xE3EFF8, *b = (const int16_t *)0xE3FD70;
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
		MorphDraw(n, MorphArg(c), c >= 6, mul32(c - 6, 682), var<uint32_t>(0xE41E98));
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0xC ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Smoke (0x64A370, creature tick 3): pool A (mask 2), 10 puffs per tick for 4 ticks in a ring
	// around the effect origin, flipbook 0xE3D300, drifting with a velocity damped by 1/4.
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
		*(uint32_t *)h = 0xE3D300;
		*(int16_t *)(h + 0x24) = 0;
		int32_t count = 0;
		LitSetup(&Frame(), s, 0, 0, 0x28);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecOf(PoolA(), i);
			if (!(r->alive & 2)) continue;
			LitSprite(h, &r->x, r->size, r->age);
			FX_HELD(PoolMemo *pm = MemoRecord(0, i, n, r);)
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				FX_HELD(MemoDied(pm);)
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
	// Rings (0x64A740 / 0x64A890, creature tick 3), 12 ticks: prim 0xE41140 around the effect
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
		*(uint32_t *)h = 0xE41140;
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
		FX_HELD(MemoNode(n);)
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
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->c++;
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 4));
		return n->c >= 0xC ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Dust (0x64A9D0, creature tick 3): pool B (mask 2), 12 puffs per tick for 2 ticks on a ring
	// on the ground (y 0, frame raised by 40), flipbook 0xE3CC38, sliding outwards (x/z velocity
	// damped by 1/8).
	// ------------------------------------------------------------------
	static uint32_t __cdecl DustTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE3CC38;
		*(int16_t *)(h + 0x24) = 0;
		int32_t count = 0;
		LitSetup(&Frame(), s, 0, -40, 0x28);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecOf(PoolB(), i);
			if (!(r->alive & 2)) continue;
			LitSprite(h, &r->x, r->size, r->age);
			FX_HELD(PoolMemo *pm = MemoRecord(1, i, n, r);)
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				FX_HELD(MemoDied(pm);)
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
	// Mist (0x64ACB0, creature tick 21): pool A (mask 4), 2 puffs per tick for 8 ticks at the
	// creature's bone 0xF0 (frame raised 20, pushed -50), flipbook 0xE3CC38 (colour 0x404040),
	// thrown up and back, growing by 1/32 and damped by 1/8.
	// ------------------------------------------------------------------
	static uint32_t __cdecl MistTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE3CC38;
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
		FX_HELD(held_note_mist(n, s);)
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecOf(PoolA(), i);
			if (!(r->alive & 4)) continue;
			LitSprite(h, &r->x, r->size, r->age);
			FX_HELD(PoolMemo *pm = MemoRecord(0, i, n, r);)
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				FX_HELD(MemoDied(pm);)
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
	// Sword sparks (0x64B640, creature tick 82, history 0x24FD510; 0x64BB90 x 2, creature tick
	// 106, bones 0x1E / 0x34, histories *(0xE41E90 / 0xE41E94) = 0x24FD510 / 0x24FD5D8): every
	// tick the bone's position and direction (effect frame) go into a 3-entry ring history; while
	// the counter <= 11 and at least 2 entries exist, a spline through them (sub_571620 /
	// sub_571690) seeds sparks along the blade: 64B640 5 of 6 points (pool C mask 1, flipbook
	// 0xE3CC38 colour 0x404040, growing 1/32), 64BB90 3 of 4 points (mask 2, flipbook 0xE3D154
	// colour 0x202020, growing 1/64). Both 64BB90 tasks draw and update every mask-2 record.
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
			FX_HELD(PoolMemo *pm = MemoRecord(2, i, owner, r);)
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->alive = 0;
				FX_HELD(MemoDied(pm);)
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
		int32_t hist = BoneHistory(0x24FD510, 0x28, n->c, s);
		SparkSetup(&Frame(), h, s, 0xE3CC38, 0x404040);
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
		uint32_t table = var<uint32_t>(0xE41E8C + 4 * (int32_t)n->a18);
		int32_t hist = BoneHistory(table, n->e, n->c, s);
		SparkSetup(&Frame(), h, s, 0xE3D154, 0x202020);
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
	// Blade glow (0x64C060, creature tick 0), until the creature is done: prim 0xE41B28 on the
	// sword, matrix = creature matrix o the bone matrix copied (one tick late) from skeleton +
	// 0xB30 o R(0x400, 0, 0x800) x 0x1A00; also computes the blade end points 0x24FD980 /
	// 0x24FD988 (effect frame) used by the trail. Drawn from tick 1, at the nearer end's depth
	// minus 8 (mode 0xE).
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
		*(uint32_t *)h = 0xE41B28;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		Cursor() = RenderPrimModel(h, var<uint32_t>(0x1D8E04C) + 4 * (uint32_t)o, 0xE, Cursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl BladeTask(TaskNode *tn)
	{
		if (var<uint8_t>(0x24FD50C) & 1) return TASK_END;
		Node24 *n = (Node24 *)tn;
		FX_HELD(held_note_blade(n);)
		BladeDraw(&Frame(), &CreatureMat(), &BoneCopy(), BladeP0(), BladeP1(), n->c > 0);
		if (Paused()) return 0;
		uint8_t *sk = **(uint8_t ***)(0x24FD93C);
		n->c++;
		memcpy(&BoneCopy(), sk + 0xB30, 0x20);
		return 0;
	}

	// ------------------------------------------------------------------
	// Blade trail (0x64C330, creature ticks 123/135/147), 6 ticks: every tick the blade base
	// 0x24FD980 and a point along the blade (length factor 0xE41E5C[c]) go into the 10-entry ring
	// 0x24FE7B8; a 32-point spline through the history (base rail and tip rail, the newest tip =
	// 0x24FD988) is drawn as 30 Gouraud quad pairs (semi-transparent, colour 0x903030 fading to
	// the far colour 0 along the trail through GTE DPCS).
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

	static uint32_t __cdecl TrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		const int16_t *p0 = BladeP0(), *p1 = BladeP1();
		int32_t k = var<int32_t>(0xE41E5C + 4 * (int32_t)n->c);
		int32_t *d = (int32_t *)(s + 0x30);
		d[0] = (int32_t)p1[0] - (int32_t)p0[0];
		d[1] = (int32_t)p1[1] - (int32_t)p0[1];
		d[2] = (int32_t)p1[2] - (int32_t)p0[2];
		int32_t len = Sqrt(NormalizeVectorToFixedPoint(d, d));
		len = mul32(len, k) >> 12;
		uint32_t w0 = var<uint32_t>(0x24FD980), w1 = var<uint32_t>(0x24FD984);
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
			const uint32_t *src = cnt == 0 ? (const uint32_t *)0x24FD988 : e + 3;
			var<uint32_t>(HIST_B + 8 * cnt) = src[0];
			var<uint32_t>(HIST_B + 8 * cnt + 4) = src[1];
			if (--j < 0) j = 9;
		}
		FX_HELD(TrailMemo *m = held_trail_put(n);)
		if (cnt > 1)
		{
			SplineRails(cnt, 32, 31);
			TrailDraw(&Frame(), SPLINE_A, SPLINE_B, s);
			FX_HELD(held_trail_drawn(m);)
		}
		FieldFree(0x40);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 6 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Slash strokes, screen space (z = word_1D8E038), model *(0xE41EA8)[+0x0E], scale s1C x 3
	// shrinking (s1C -= s1E) over the first ticks, fading in and out (+0x16 = base fade), after a
	// delay +0x22:
	//   0x64C7F0 (creature 155, x3) renderer MAG_187_sub_64C9A0 (colour 0x2020A0), fade in over
	//            8, out from +0x20 - 8, shrink 6 ticks (1/3), ends at +0x20;
	//   0x64D6A0 (x2 per 0x64C7F0) the same, ends at 7;
	//   0x64D930 (creature 121/133/145) Effect_RenderPrimModel mode 3, fade in over 6, shrink 3
	//            ticks (1/2), ends at +0x20;
	//   0x64DAD0 (x4 per 0x64D930) the same, ends at 5.
	// ------------------------------------------------------------------
	static int32_t SlashFade(int32_t c, int32_t s20, int32_t base, int32_t in_len)
	{
		int32_t span = 0x1000 - base;
		if (c < in_len) return 0x1000 - mul32(span / in_len, c);
		int32_t out = s20 - 8;
		if (c >= out) return mul32(c - out, span / 8) + base;
		return base;
	}

	static void SlashDraw(const Node24 *n, int32_t scale, int32_t fade_value, bool model)
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
		uint8_t *h = (uint8_t *)FieldAlloc(model ? 0x6C : 0x58);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)h = var<uint32_t>(0xE41EA8 + 4 * (int32_t)n->e);
		*(int32_t *)(h + 0xC) = fade_value;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		if (model)
		{
			*(uint32_t *)(h + 0x20) = 0x2020A0;
			Cursor() = SlashRender(h, OT(), 3, Cursor());
			FieldFree(0x6C);
		}
		else
		{
			Cursor() = RenderPrimModel(h, OT(), 3, Cursor());
			FieldFree(0x58);
		}
	}

	static uint32_t SlashTask(Node24 *n, bool model, int32_t end, bool third)
	{
		if (n->s22 > 0)
		{
			if (Paused()) return 0;
			n->s22--;
			return 0;
		}
		int32_t c = n->c;
		SlashDraw(n, n->s1C, SlashFade(c, n->s20, n->p16, model ? 8 : 6), model);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t cc = n->c;
		if (third ? cc < 6 : cc < 3)
		{
			int16_t v = n->s1E;
			n->s1C = (int16_t)(n->s1C - v);
			n->s1E = (int16_t)(v - (int16_t)((int32_t)v / (third ? 3 : 2)));
		}
		n->c = (int16_t)(cc + 1);
		return n->c >= (end < 0 ? n->s20 : end) ? TASK_END : 0;
	}

	static uint32_t __cdecl SlashATask(TaskNode *tn) { return SlashTask((Node24 *)tn, true, -1, true); }
	static uint32_t __cdecl SlashBTask(TaskNode *tn) { return SlashTask((Node24 *)tn, true, 7, true); }
	static uint32_t __cdecl SlashCTask(TaskNode *tn) { return SlashTask((Node24 *)tn, false, -1, false); }
	static uint32_t __cdecl SlashDTask(TaskNode *tn) { return SlashTask((Node24 *)tn, false, 5, false); }

	// ------------------------------------------------------------------
	// Target hide (0x64DCF0, creature ticks 131/143/155), draws nothing: after +0x0E ticks puts
	// entity flag bit 3 of slot +0x18 back as saved in +0x1A.
	// ------------------------------------------------------------------
	static uint32_t __cdecl HideTask(TaskNode *tn)
	{
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		n->c++;
		if (n->c < n->e) return 0;
		uint16_t *flags = (uint16_t *)Entity((uint32_t)(int32_t)n->a18);
		*flags = (uint16_t)((*flags & 0xFFF7) | (uint16_t)n->s1A);
		return TASK_END;
	}
}

	void register_mag187_odin()
	{
		register_port(o187::ORIG_SequenceTick, (void *)o187::SequenceTick, "O187 SequenceTick", 187);
		register_port(o187::ORIG_TimelineTask, (void *)o187::TimelineTask, "O187 TimelineTask", 187);
		register_port(o187::ORIG_GridATask, (void *)o187::GridATask, "O187 GridATask", 187);
		register_port(o187::ORIG_GridBTask, (void *)o187::GridBTask, "O187 GridBTask", 187);
		register_port(o187::ORIG_DebrisTask, (void *)o187::DebrisTask, "O187 DebrisTask", 187);
		register_port(o187::ORIG_WarpTask, (void *)o187::WarpTask, "O187 WarpTask", 187);
		register_port(o187::ORIG_GlowTask, (void *)o187::GlowTask, "O187 GlowTask", 187);
		register_port(o187::ORIG_StreakTask, (void *)o187::StreakTask, "O187 StreakTask", 187);
		register_port(o187::ORIG_SliceTask, (void *)o187::SliceTask, "O187 SliceTask", 187);
		register_port(o187::ORIG_EndTask, (void *)o187::EndTask, "O187 EndTask", 187);
		register_port(o187::ORIG_CreatureTask, (void *)o187::CreatureTask, "O187 CreatureTask", 187);
		register_port(o187::ORIG_SmokeTask, (void *)o187::SmokeTask, "O187 SmokeTask", 187);
		register_port(o187::ORIG_RingATask, (void *)o187::RingATask, "O187 RingATask", 187);
		register_port(o187::ORIG_RingBTask, (void *)o187::RingBTask, "O187 RingBTask", 187);
		register_port(o187::ORIG_DustTask, (void *)o187::DustTask, "O187 DustTask", 187);
		register_port(o187::ORIG_MistTask, (void *)o187::MistTask, "O187 MistTask", 187);
		register_port(o187::ORIG_FadeInTask, (void *)o187::FadeInTask, "O187 FadeInTask", 187);
		register_port(o187::ORIG_FlashTask, (void *)o187::FlashTask, "O187 FlashTask", 187);
		register_port(o187::ORIG_MorphTask, (void *)o187::MorphTask, "O187 MorphTask", 187);
		register_port(o187::ORIG_SparkTask, (void *)o187::SparkTask, "O187 SparkTask", 187);
		register_port(o187::ORIG_Spark2Task, (void *)o187::Spark2Task, "O187 Spark2Task", 187);
		register_port(o187::ORIG_BladeTask, (void *)o187::BladeTask, "O187 BladeTask", 187);
		register_port(o187::ORIG_TrailTask, (void *)o187::TrailTask, "O187 TrailTask", 187);
		register_port(o187::ORIG_SlashATask, (void *)o187::SlashATask, "O187 SlashATask", 187);
		register_port(o187::ORIG_SlashBTask, (void *)o187::SlashBTask, "O187 SlashBTask", 187);
		register_port(o187::ORIG_SlashCTask, (void *)o187::SlashCTask, "O187 SlashCTask", 187);
		register_port(o187::ORIG_SlashDTask, (void *)o187::SlashDTask, "O187 SlashDTask", 187);
		register_port(o187::ORIG_HideTask, (void *)o187::HideTask, "O187 HideTask", 187);
		// 30 fps layer: see mag187_odin_held.inc
		FX_HELD(register_mag187_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag187_odin_held.inc"
#endif
