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

// Effects 327-330: Gilgamesh - Zantetsuken / Masamune / Excalibur / Excalipoor (timeline-B GF
// family, MAG_327_*). One module serves the four ids: the four setups MAG_327..330_UNKNOWN
// (0x58D760 / 0x58D930 / 0x58DB10 / 0x58DCF0, not ported: they run once) differ only in the
// variant cell 0x21FFA70 (0..3) and the TIM they upload; every task below is shared and reads
// the variant where the four attacks differ.
//
// Structure (gf_study/gf_inventory_timeline.md section 3.2):
//   setup - model buffer mb = 0x20DFAB8 (dword 0x2200FB8); sprite pools 0xCD0398 = mb+0x2E000
//     (pool B, 200 x 0x18), 0xCD039C = mb+0x2F800 (pool A, 200 x 0x20), vertex scratch 0xCD03A0 =
//     mb+0x31800; root queue 0x21FF4A8 (pool 0x21FF468, 2 x 0x10) with the master, queue Q1
//     0x21FFD58 (pool 0x21FFD68, 100 x 0x24) with the timeline, queue Q2 0x21FF458 (pool
//     0x21FF3C8, 4 x 0x24) with the model tasks.
//   SequenceTick (master, 0x596B70) - runs Q1 then Q2, ++counter, ends when both are empty.
//   TimelineTask (0x592300) - 328 ticks: camera script, streams, loads, sounds, screen flash, party
//     hide/show, spawns (fade tile 0, rings 8, flare row 18, risers 44, swords + Gilgamesh 76,
//     sparks 101, puffs 113, spinners 121, dots 122, rising rings 123, horse 181, final pose 223,
//     trail 263, the variant's sword attack 273, end 279).
//   Models (battle-model blocks E, standard reader 0x509440 / 0x508F90):
//     E0..E3 = 0x21FF4F0 + 0x9C*i  the four swords (per variant, exe .data 0xE808D0..0xE91520)
//     E4     = 0x21FF760           Gilgamesh, first pose (mb)
//     E5     = 0x2200F00           Gilgamesh on the horse (mb + 0x22000)
//     B0..B3 = 0x21FF800 + 0x9C*j  its four arms' swords (mb + 0x2379C + j*0x1860)
//     E6     = 0x2200FC0           Gilgamesh, final pose (mb)
//   The timeline also queues the shared camera-script task MAG_066_sub_63E9C0 (Doomtrain file)
//   into Q1; it is not part of this module and is not ported here. The module never writes the
//   battle camera itself (no held-frame camera).
// Every task tests battle_to_update_flags_dword_1D96A9C & 0x201 (draw-only when set), except the
// master and the end task 0x596B30. The timeline returns at once on bit 0, and with bit 0x200
// only while a file load is pending (then it runs its events).
// Module-private renderers (pure functions of a scratch header: clipped target model 0x58E880,
// clipped party 0x58FB50, clipped sword 0x595250, materialising model 0x5960A0) and the bone
// midpoint helper 0x58E260 are called through their original addresses.

#include "fx_port.h"

namespace ff8fx
{
namespace g327
{
	using namespace eng;

	// --- engine functions used by this module (original addresses) ---
	namespace x
	{
		inline int32_t CrtRand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); }
		inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
		inline void ComposeZYXRotationMatrix(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x56CE30)(angles, out); }
		inline void Scale3DMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); }
		inline int32_t NormalizeVectorToFixedPoint(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); }
		inline uint32_t MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { return fn<uint32_t (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
		inline int32_t GetRotationBetweenVectors(const void *ref, const int32_t *v, int32_t *axis) { return fn<int32_t (__cdecl *)(const void *, const int32_t *, int32_t *)>(0x571480)(ref, v, axis); }
		inline void BuildAxisAngleRotationMatrix(int32_t angle, Mat4x3 *out, const int32_t *axis) { fn<void (__cdecl *)(int32_t, Mat4x3 *, const int32_t *)>(0x5714F0)(angle, out, axis); }
		inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
		inline void GetDefaultEffectPosition(void *entity, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x571400)(entity, out); }
		inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); }       // sub_571620
		inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); } // sub_571690
		inline void SetProjection(int32_t h) { fn<void (__cdecl *)(int32_t)>(0x56CD00)(h); }                                             // Call_Bs_parseCamera2 (GTE H)
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
		inline void ApplyActionResultToTarget(void *rec) { fn<void (__cdecl *)(void *)>(0x506690)(rec); }
		inline void WaitAnimSeq(uint32_t buffer, void *flag) { fn<void (__cdecl *)(uint32_t, void *)>(0x508630)(buffer, flag); }                   // sub_508630
		inline int32_t LoadState() { return fn<int32_t (__cdecl *)()>(0x508500)(); }                                                        // sub_508500: < 0 = file load busy
		inline void CharacterLoad(int32_t id, uint32_t dst) { fn<void (__cdecl *)(int32_t, uint32_t)>(0x508480)(id, dst); }                // BattleFile_CharacterLoad
		inline void QueueTIMUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(0x505E30)(tim); }                                        // Battle_QueueTIMUpload_GetEOF
		inline uint32_t SummonData() { return fn<uint32_t (__cdecl *)()>(0x571B70)(); }                                                    // sub_571B70: 0x209FAB8
		inline void AnimSeqText() { fn<void (__cdecl *)()>(0x50A750)(); }                                                                  // AnimSeqBB_handleText
		inline void RequestScreenFeedback(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t, int32_t)>(0x47CF50)(a, b, c, d, e); } // Battle_RequestScreenFeedback
		// software GTE
		inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
		inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // MAG_148_sub_64DDA0: BK = m.t
		inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }
		inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }                 // MAG_163_sub_64DE00: R = s * I
		inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); } // MAG_163_sub_64DD70: TR = v
		inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }                                 // MAG_164_sub_64DE40: TR = MAC123
		inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
		inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
		inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
		inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
		inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
		inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }                   // GTE_ReadOTZ
		inline void GteReadSZ3(void *dst) { fn<void (__cdecl *)(void *)>(0x45E220)(dst); }                     // sub_45E220: *dst = SZ3
		inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }            // set_unk_1CA8A28
		inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                                         // sub_45F270
		inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }                   // set_param_with_dword_1CA8A68
		inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
		inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DDA0)(r, g, b); } // pre_someCameraWork_45DD60
		// module-private renderers / helpers called through their original addresses
		inline void BoneMid(void *entity, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x58E260)(entity, out); } // midpoint of bones 0xF0/0xF1, [3] = entity+0x24
		inline uint32_t SliceRender(void *geom, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x58E880)(geom, h, ot, mode, cursor); }
		inline uint32_t PartyRender(void *anim, void *h, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x58FB50)(anim, h, ot, shift, cursor); }
		inline uint32_t ClipRender(void *geom, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x595250)(geom, h, ot, mode, cursor); }
		inline uint32_t MaterialRender(void *geom, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x5960A0)(geom, h, ot, mode, cursor); }
	}
	using namespace x;

	// --- module globals ---
	inline bool Paused() { return (var<uint32_t>(0x1D96A9C) & 0x201) != 0; } // battle_to_update_flags_dword_1D96A9C
	inline Mat4x3 &RootMatrix() { return var<Mat4x3>(0x21FF3A0); }  // effect frame (setup: rotation, centroid 0x21FF3B4/B8/BC)
	inline Mat4x3 &Frame() { return var<Mat4x3>(0x21FF488); }       // Camera o RootMatrix, rebuilt by the timeline every tick
	inline TaskQueue &Q1() { return var<TaskQueue>(0x21FFD58); }     // timeline + effects
	inline TaskQueue &Q2() { return var<TaskQueue>(0x21FF458); }     // model tasks
	inline uint32_t &VoiceSlot() { return var<uint32_t>(0x21FF2A8); }
	inline int32_t Variant() { return var<int32_t>(0x21FFA70); }     // 0 Zantetsuken, 1 Masamune, 2 Excalibur, 3 Excalipoor
	inline uint8_t *CastCtx() { return var<uint8_t *>(0x2200DF0); }
	inline uint32_t MB() { return var<uint32_t>(0x2200FB8); }        // model buffer (0x20DFAB8)
	inline uint32_t &Cursor() { return var<uint32_t>(0x1D8E054); }   // battle_texture_data_ptr_1D8E054
	inline uint32_t RenderList() { return var<uint32_t>(0x1D8E04C); }
	inline uint32_t OT() { return RenderList() + 0x44; }
	inline uint8_t *PoolB() { return var<uint8_t *>(0xCD0398); }     // mb + 0x2E000, 200 x 0x18
	inline uint8_t *PoolA() { return var<uint8_t *>(0xCD039C); }     // mb + 0x2F800, 200 x 0x20
	inline uint32_t VertexScratch() { return var<uint32_t>(0xCD03A0); } // mb + 0x31800 (RenderGeometry scratch)
	inline uint8_t *Targets() { return *(uint8_t **)(CastCtx() + 4); } // +0x08 -> 0x18-byte records (byte 0 = slot, byte 3 bit 2 = skip), +0x10 u8 count
	inline uint32_t TargetCount() { return Targets()[0x10]; }
	inline uint8_t *TargetRecs() { return *(uint8_t **)(Targets() + 8); }
	inline uint8_t *Entity(uint32_t slot) { return (uint8_t *)(0x1D972C0 + 156 * slot); }
	inline uint8_t *Sword(int i) { return (uint8_t *)(0x21FF4F0 + 0x9C * i); }
	inline uint8_t *Arm(int j) { return (uint8_t *)(0x21FF800 + 0x9C * j); }
	static uint8_t *const GIL1 = (uint8_t *)0x21FF760;
	static uint8_t *const GIL2 = (uint8_t *)0x2200F00;
	static uint8_t *const GIL3 = (uint8_t *)0x2200FC0;
	static const uint32_t STREAM_STATE = 0x21FF398;

	static const uint32_t ORIG_SequenceTick = 0x596B70;
	static const uint32_t ORIG_TimelineTask = 0x592300;
	static const uint32_t ORIG_FeedbackTask = 0x592A00;
	static const uint32_t ORIG_Feedback2Task = 0x592A50;
	static const uint32_t ORIG_FlashInTask = 0x592AE0;
	static const uint32_t ORIG_FlashOutTask = 0x592B60;
	static const uint32_t ORIG_FadeTileTask = 0x592BC0;
	static const uint32_t ORIG_RingTask = 0x592E90;
	static const uint32_t ORIG_RingSpinTask = 0x592F90;
	static const uint32_t ORIG_RingSpin2Task = 0x5930C0;
	static const uint32_t ORIG_FlareTask = 0x593250;
	static const uint32_t ORIG_RiserTask = 0x593600;
	static const uint32_t ORIG_PuffTask = 0x593770;
	static const uint32_t ORIG_SparkTask = 0x5939C0;
	static const uint32_t ORIG_RisingRingSpawnerTask = 0x593C70;
	static const uint32_t ORIG_RisingRingTask = 0x593D20;
	static const uint32_t ORIG_DotTask = 0x593E80;
	static const uint32_t ORIG_SpinnerTask = 0x594210;
	static const uint32_t ORIG_TrailTask = 0x5943D0;
	static const uint32_t ORIG_SwordShardTask = 0x5948A0;
	static const uint32_t ORIG_SwordPuffTask = 0x594CA0;
	static const uint32_t ORIG_SwordClipTask = 0x594FD0;
	static const uint32_t ORIG_SwordDrawTask = 0x595D50;
	static const uint32_t ORIG_Gil1Task = 0x595EE0;
	static const uint32_t ORIG_Gil1DrawTask = 0x5966B0;
	static const uint32_t ORIG_Gil2Task = 0x596710;
	static const uint32_t ORIG_Gil2DrawTask = 0x596800;
	static const uint32_t ORIG_Gil3Task = 0x596970;
	static const uint32_t ORIG_Gil3DrawTask = 0x596A60;
	static const uint32_t ORIG_EndTask = 0x596B30;
	// variant 0 (Zantetsuken)
	static const uint32_t ORIG_Zan_Task = 0x58DED0;
	static const uint32_t ORIG_Zan_ScreenPrimTask = 0x58E120;
	static const uint32_t ORIG_Zan_CutTask = 0x58E3E0;
	static const uint32_t ORIG_Zan_SliceTask = 0x58E570;
	static const uint32_t ORIG_Zan_PartyClipTask = 0x58F940;
	// variant 1 (Masamune)
	static const uint32_t ORIG_Masa_Task = 0x590510;
	static const uint32_t ORIG_Masa_ScreenPrimTask = 0x590790;
	static const uint32_t ORIG_Masa_PrimATask = 0x5908D0;
	static const uint32_t ORIG_Masa_PrimBTask = 0x590A10;
	static const uint32_t ORIG_Masa_SparkTask = 0x590B60;
	static const uint32_t ORIG_Masa_DustTask = 0x590F10;
	// variants 2 and 3 (Excalibur / Excalipoor: one code, node +0x0E selects the Excalipoor data)
	static const uint32_t ORIG_Exca_Task = 0x591250;
	static const uint32_t ORIG_Poor_Task = 0x5920E0;
	static const uint32_t ORIG_Exca_ScreenPrimTask = 0x591470;
	static const uint32_t ORIG_Exca_BurstTask = 0x5915C0;
	static const uint32_t ORIG_Exca_SmokeTask = 0x591850;
	static const uint32_t ORIG_Exca_StreakTask = 0x591B80;
	static const uint32_t ORIG_Exca_PillarTask = 0x591F50;

	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static bool g_tick_paused = false; // the 0x201 flags were set on that tick (nothing advanced)

#pragma pack(push, 1)
	// every Q1/Q2 node is 0x24 bytes; field use differs per task
	struct Node24
	{
		TaskNode hdr;
		int16_t c;                  // +0x0C tick counter
		int16_t e;                  // +0x0E delay / length / variant / target slot / Excalipoor flag / done flag
		int16_t p10, p12, p14, p16; // +0x10 position (p16: pad, copied as part of dwords; delay of the cut tasks)
		int16_t a18, a1A;           // +0x18 angle, spin (flash: level; slice: target index)
		int16_t s1C, s1E;           // +0x1C scale, its speed (sword particles: owner mask, sword index)
		int16_t s20, s22;           // +0x20 second scale / height, its speed / fade value
	};
	// sprite record of pool A (0xCD039C, 0x20 bytes)
	struct RecA
	{
		uint32_t mask;              // +0x00 owner bits
		int16_t age, size;          // +0x04 flipbook frame / age, size
		int16_t x, y, z, w0E;       // +0x08 position (w0E: initial size of the streaks / shards)
		int16_t vx, vy, vz, w16;    // +0x10 velocity
		int16_t w18, w1A, w1C, w1E; // +0x18 angles and spins / direction (streaks)
	};
	// sprite record of pool B (0xCD0398, 0x18 bytes)
	struct RecB
	{
		uint32_t mask;
		int16_t age, size;
		int16_t x, y, z, w0E;
		int16_t vx, vy, vz, w16;
	};
#pragma pack(pop)
	static_assert(sizeof(Node24) == 0x24, "queue node is 0x24 bytes");
	static_assert(sizeof(RecA) == 0x20, "pool A record is 0x20 bytes");
	static_assert(sizeof(RecB) == 0x18, "pool B record is 0x18 bytes");

	static const int POOL_N = 200;
	inline RecA *RA(int i) { return (RecA *)(PoolA() + 0x20 * i); }
	inline RecB *RB(int i) { return (RecB *)(PoolB() + 0x18 * i); }

	// first free record (dword 0) of a pool, -1 when none
	static int FreeRecord(uint8_t *pool, int stride)
	{
		for (int i = 0; i < POOL_N; i++)
			if (*(uint32_t *)(pool + stride * i) == 0) return i;
		return -1;
	}

	static int16_t Lerp16(int16_t a, int16_t b, int num, int den) { return (int16_t)lerp_i(a, b, num, den); }

	// au_re_BdLinkTask_17 0x592970 (Q1) / _18 0x592990 (Q2) (no null check, as the original)
	static Node24 *LinkQ1(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), task_fn);
		n->c = 0;
		n->e = 0;
		return n;
	}
	static Node24 *LinkQ2(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q2(), task_fn);
		n->c = 0;
		n->e = 0;
		return n;
	}

	// MAG_327_sub_5904A0: rotation part = identity (translation and pad kept)
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

	// MAG_327_sub_5904D0 / 5904F0: stru_1D9898C[0..3].currentBsId bit 1 (hide / show the party)
	static void HideParty()
	{
		for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) |= 2;
	}
	static void ShowParty()
	{
		for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) &= 0xFD;
	}

	// MAG_327_sub_58E790: plane distance, n[3] = -(p . n) >> 12
	static void PlaneDistance(const int16_t *p, int16_t *nrm)
	{
		int32_t d = mul32(p[2], nrm[2]) + mul32(p[1], nrm[1]);
		d = d + mul32(p[0], nrm[0]);
		nrm[3] = (int16_t)-(d >> 12);
	}

	// ---- held-frame memos ----
	static NodeMemo<Node24, 256> g_node_memo;
	static void MemoNode(const Node24 *n) { if (Node24 *m = g_node_memo.put(n)) *m = *n; }
	static bool NodeMoved(const Node24 *m, const Node24 *n) { return !g_tick_paused && n->c == (int16_t)(m->c + 1); }

	// pool records as drawn on the current real tick (before their update)
	struct RecMemoA { uint32_t tick; const void *owner; RecA r; };
	struct RecMemoB { uint32_t tick; const void *owner; RecB r; };
	static RecMemoA g_memo_a[POOL_N];
	static RecMemoB g_memo_b[POOL_N];
	static void MemoA(int i, const void *owner) { g_memo_a[i].tick = g_real_tick; g_memo_a[i].owner = owner; g_memo_a[i].r = *RA(i); }
	static void MemoB(int i, const void *owner) { g_memo_b[i].tick = g_real_tick; g_memo_b[i].owner = owner; g_memo_b[i].r = *RB(i); }
	// the record moved on on the last tick (alive, same owner bits, next frame)
	static bool AdvancedA(const RecMemoA &m, const RecA *r) { return !g_tick_paused && r->mask == m.r.mask && r->age != m.r.age; }
	static bool AdvancedB(const RecMemoB &m, const RecB *r) { return !g_tick_paused && r->mask == m.r.mask && r->age != m.r.age; }

	// held draws give the model renderers a private vertex scratch
	static uint8_t g_vertex_scratch[0x20000];
	static bool g_held = false;
	static uint32_t ModelScratch() { return g_held ? (uint32_t)g_vertex_scratch : VertexScratch(); }

	// ------------------------------------------------------------------
	// Generic prim model draw (Effect_RenderPrimModel 0x572200, header on the scratch stack): the
	// shape shared by every node-driven prim task of the module. angles -> ZYX rotation, t, scale,
	// optionally composed with the camera / the frame; screen = GTE H forced to 0x120 around the
	// render (the screen-space sword prims).
	// ------------------------------------------------------------------
	struct PrimArgs
	{
		int16_t ang[3];
		int32_t t[3];
		int32_t s[3];
		const Mat4x3 *compose;
		uint32_t model;
		int32_t mode;
		uint32_t h1C;
		bool setC;
		int32_t hC;
		bool screen;
	};

	static void PrimDraw(const PrimArgs &a)
	{
		int16_t ang[4] = { a.ang[0], a.ang[1], a.ang[2], 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = a.t[0];
		m.t[1] = a.t[1];
		m.t[2] = a.t[2];
		int32_t s[3] = { a.s[0], a.s[1], a.s[2] };
		Scale3DMatrix(&m, s);
		if (a.compose) ComposeAffineTransform(a.compose, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = a.model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = a.h1C;
		if (a.setC) *(int32_t *)(h + 0xC) = a.hC;
		if (a.screen) SetProjection(0x120);
		Cursor() = RenderPrimModel(h, OT(), a.mode, Cursor());
		if (a.screen) SetProjection(var<int16_t>(0x1D8E038));
		FieldFree(0x58);
	}

	static PrimArgs Prim(int16_t a0, int16_t a1, int16_t a2, int32_t tx, int32_t ty, int32_t tz, int32_t sx, int32_t sy, int32_t sz,
		const Mat4x3 *compose, uint32_t model)
	{
		PrimArgs a;
		a.ang[0] = a0; a.ang[1] = a1; a.ang[2] = a2;
		a.t[0] = tx; a.t[1] = ty; a.t[2] = tz;
		a.s[0] = sx; a.s[1] = sy; a.s[2] = sz;
		a.compose = compose;
		a.model = model;
		a.mode = 2;
		a.h1C = 0x33;
		a.setC = false;
		a.hC = 0;
		a.screen = false;
		return a;
	}

	static void SetFade(PrimArgs &a, bool fade, int32_t v)
	{
		if (!fade) return;
		a.h1C = 0xF3;
		a.setC = true;
		a.hC = v;
	}

	// in-between value of a fade that is linear in the counter, when both ticks fade
	static int32_t FadeLerp(bool (*f)(int32_t, int32_t *), int32_t c, bool moved, int num, int den, bool *fade)
	{
		int32_t v0 = 0, v1 = 0;
		*fade = f(c, &v0);
		if (moved && *fade && f(c + 1, &v1)) return lerp_i(v0, v1, num, den);
		return v0;
	}

	// ------------------------------------------------------------------
	// Master (0x596B70): node from pool 0x21FF468, +0x0C counter (increments even when paused)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SequenceTick(TaskNode *tn)
	{
		g_ported_tick = g_real_tick;
		g_tick_paused = Paused();
		int a = ExecuteTaskQueue(&Q1());
		int b = ExecuteTaskQueue(&Q2());
		((Node24 *)tn)->c++;
		return (a | b) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Spawners called by the timeline
	// ------------------------------------------------------------------
	// MAG_327_sub_5929B0: screen feedback request task for len ticks (flag: skip its first tick)
	static void LinkFeedback(int32_t flag, int16_t len)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), flag ? ORIG_Feedback2Task : ORIG_FeedbackTask);
		n->c = 0;
		n->e = len;
	}

	// au_re_BdLinkTask_19 0x592AB0 / _20 0x592B30: screen flash in / out, 16 ticks, level 0x800
	static void LinkFlash(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), task_fn);
		n->c = 0;
		n->e = 0x10;
		n->a18 = 0x800;
	}

	// MAG_327_sub_592D00: the ground rings (a still one, two spinning pairs)
	static void SpawnRings()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_RingTask);
		n->c = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xE4A8;
		n->p14 = 0;
		n->s1C = 0x2400;
		n->s20 = 0x3000;
		n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_RingSpinTask);
		n->c = 0;
		n->e = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xE4A8;
		n->p14 = 0;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->a1A = 4;
		n->s1C = 0x2000;
		n->s20 = 0x3000;
		n->s22 = 0;
		n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_RingSpinTask);
		n->c = 0;
		n->e = 1;
		n->p10 = 0;
		n->p12 = (int16_t)0xE4A8;
		n->p14 = 0;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->a1A = 8;
		n->s1C = 0x2000;
		n->s20 = 0x3000;
		n->s22 = 0x600;
		n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_RingSpin2Task);
		n->c = 0;
		n->e = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xCD38;
		n->p14 = 0;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->a1A = 4;
		n->s1C = 0x3400;
		n->s22 = 0x800;
		n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_RingSpin2Task);
		n->c = 0;
		n->e = 1;
		n->p10 = 0;
		n->p12 = (int16_t)0xC950;
		n->p14 = 0;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->a1A = -4;
		n->s1C = 0x3000;
		n->s22 = 0x800;
	}

	// MAG_327_sub_5931E0: the flare row, 0x36B0 above the effect origin (world position)
	static void SpawnFlareRow()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_FlareTask);
		n->c = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xC950;
		n->p14 = 0;
		MatrixMultiplyVector(&RootMatrix(), &n->p10, &n->p10);
		n->p10 = (int16_t)(n->p10 + var<int16_t>(0x21FF3B4));
		n->p12 = (int16_t)(n->p12 + var<int16_t>(0x21FF3B8));
		n->p14 = (int16_t)(n->p14 + var<int16_t>(0x21FF3BC));
		n->s22 = 0x320;
		n->s20 = 0x320;
	}

	// MAG_327_sub_593550: four rising prims on a circle of radius 1000, random phase
	static void SpawnRisers()
	{
		int32_t base = CrtRand() % 0x1000;
		for (int32_t k = 0; k < 0x4000; k += 0x1000)
		{
			int32_t a = k / 4 + base;
			Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_RiserTask);
			n->c = 0;
			int32_t s = ComputeSin(a);
			n->p12 = (int16_t)0xC568;
			n->p10 = (int16_t)(mul32(s, 1000) >> 12);
			int32_t co = ComputeCos(a);
			n->a18 = (int16_t)a;
			n->s1C = 0x400;
			n->s22 = 0x5A;
			n->p14 = (int16_t)(mul32(co, 1000) >> 12);
		}
	}

	// MAG_327_sub_5947F0: the four swords (clipped draw in Q1, normal draw in Q2) and their
	// particles (puffs + shards per sword, owner mask 1 << k, sword index table 0xCD0480)
	static void SpawnSwords()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_SwordClipTask);
		n->c = 0;
		n->e = var<int16_t>(0x21FFA70);
		n = (Node24 *)AddTaskToQueue(&Q2(), ORIG_SwordDrawTask);
		n->c = 0;
		n->e = 0x69;
		for (int k = 0; k < 4; k++)
		{
			int16_t sword = *(const int16_t *)(0xCD0480 + 4 * k);
			Node24 *a = (Node24 *)AddTaskToQueue(&Q1(), ORIG_SwordPuffTask);
			a->c = 0;
			a->e = (int16_t)k;
			a->s1E = sword;
			a->s1C = (int16_t)(1 << k);
			Node24 *b = (Node24 *)AddTaskToQueue(&Q1(), ORIG_SwordShardTask);
			b->c = 0;
			b->s1C = (int16_t)(1 << k);
			b->s1E = *(const int16_t *)(0xCD0480 + 4 * k);
		}
	}

	// MAG_327_sub_594160: three spinning prims from the table 0xCD0468 (delay, height, scale, height)
	static void SpawnSpinners()
	{
		for (uint32_t t = 0xCD046A; t < 0xCD0482; t += 8)
		{
			Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_SpinnerTask);
			n->c = 0;
			n->e = *(const int16_t *)(t - 2);
			n->p10 = 0;
			n->p12 = *(const int16_t *)t;
			n->p14 = 0;
			n->a18 = (int16_t)(CrtRand() % 0x1000);
			int32_t r = CrtRand() % 60;
			n->a1A = (int16_t)(r + 0x14);
			int32_t w = (int32_t)*(const int16_t *)(t + 2) / 8;
			n->s1E = (int16_t)w;
			n->s1C = (int16_t)(w + w);
			int32_t h = CrtRand() % 0x600;
			n->s20 = (int16_t)(h + *(const int16_t *)(t + 4));
		}
	}

	// au_re_BdLinkTask_21 0x5943A0: the sword trail, its 10-entry history cleared
	static void LinkTrail()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_TrailTask);
		n->c = 0;
		for (uint32_t a = 0x2200DF8; a < 0x2200EC0; a += 0x14) var<uint32_t>(a) = 0;
	}

	// ------------------------------------------------------------------
	// Timeline (0x592300), 328 ticks. Load waits (sub_508500 < 0) repeat the tick.
	// ------------------------------------------------------------------
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
			CameraScriptStart(*(const uint32_t *)(0xCD0960 + 4 * Variant()), &RootMatrix(), GIL3, &Q1(), 0);
			static const uint32_t streams[4] = { 0xE9151C, 0xE8D208, 0xE88EF4, 0xE84BE0 };
			uint32_t v = (uint32_t)Variant();
			if (v <= 3) BdTransSummonStream(streams[v], STREAM_STATE);
			VoiceSlot() = ClaimVoiceSlot(*(const void **)(0xCD04A0 + 4 * Variant()), 1, 0x80);
		}
		int32_t c = n->c;
		int32_t e;
		if (c < 0x10)
		{
			if (c == 1) BdPlaySE(0xCD0378, 0, 0x80);
			else if (c == 2) CharacterLoad(0x2E9, MB() + 0xE000);
			else if (c == 0) LinkQ1(ORIG_FadeTileTask);
			else if (c == 8)
			{
				SpawnRings();
				ShowParty();
			}
		}
		else if ((e = c - 0x10) < 0x3C)
		{
			if (e == 0)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0xE000);
				CharacterLoad(0x2EA, MB());
			}
			else if (e == 8)
			{
				if (LoadState() < 0) return 0;
				CharacterLoad(0x2EB, MB() + 0x8000);
			}
			else if (e == 2) SpawnFlareRow();
			else if (e == 0x1C) SpawnRisers();
		}
		else if ((e -= 0x3C) < 0x2D) // 76
		{
			if (e == 3) BdPlaySummonStream(0x80, 0, 0x60);
			else if (e == 0)
			{
				HideParty();
				SpawnSwords();
				LinkQ2(ORIG_Gil1DrawTask);
				LinkQ1(ORIG_Gil1Task);
				LinkFlash(ORIG_FlashInTask);
			}
			else if (e == 0x18)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0x8000);
			}
			else if (e == 0x19) LinkQ1(ORIG_SparkTask);
			else if (e == 0x25) LinkQ1(ORIG_PuffTask);
		}
		else if ((e -= 0x2D) < 0x3C) // 121
		{
			if (e == 0)
			{
				BdPlaySE(0xCD037C, 0, 0x80);
				SpawnSpinners();
			}
			else if (e == 1)
			{
				CharacterLoad(0x2EC, MB() + 0x8000);
				LinkQ1(ORIG_DotTask);
			}
			else if (e == 0xF)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0x8000);
				CharacterLoad(0x2ED, MB() + 0x22000);
			}
			else if (e == 0x1E)
			{
				if (LoadState() < 0) return 0;
				CharacterLoad(0x2EE, SummonData());
			}
			else if (e == 0x37)
			{
				if (LoadState() < 0) return 0;
				BdTransSummonStream(SummonData(), STREAM_STATE);
			}
			else if (e == 2) LinkQ1(ORIG_RisingRingSpawnerTask);
		}
		else if ((e -= 0x3C) < 0x2A) // 181
		{
			if (e == 0)
			{
				BdPlaySE(0xCD0380, 0, 0x80);
				BdPlaySummonStream(0x80, 0, 0x60);
				CharacterLoad(0x2EF, MB());
				LinkQ2(ORIG_Gil2DrawTask);
				LinkQ1(ORIG_Gil2Task);
				LinkFeedback(1, 0x5C);
			}
			else if (e == 0xA)
			{
				if (LoadState() < 0) return 0;
				CharacterLoad(0x2F0, SummonData());
			}
			else if (e == 0x25)
			{
				if (LoadState() < 0) return 0;
				BdTransSummonStream(SummonData(), STREAM_STATE);
			}
		}
		else if ((e -= 0x2A) < 0x32) // 223
		{
			if (e == 0x14) BdPlaySummonStream(0x80, 0, 0x60);
			else if (e == 0)
			{
				CharacterLoad(0x2F3, MB() + 0xE000);
				LinkQ2(ORIG_Gil3DrawTask);
				LinkQ1(ORIG_Gil3Task);
			}
			else if (e == 0xE)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0xE000);
			}
			else if (e == 0xF)
			{
				int32_t id = *(const int32_t *)(0xCD09A0 + 4 * Variant());
				if (id) CharacterLoad(id, MB() + 0xE000);
			}
			else if (e == 0x1E)
			{
				if (*(const int32_t *)(0xCD09A0 + 4 * Variant()) != 0 && LoadState() < 0) return 0;
				CharacterLoad(0x2F1, SummonData());
			}
			else if (e == 0x2D)
			{
				if (LoadState() < 0) return 0;
				BdTransSummonStream(SummonData(), STREAM_STATE);
			}
			else if (e == 0x28) LinkTrail();
			else if (e == 0xA) AnimSeqText();
		}
		else if ((e -= 0x32) < 0x32) // 273
		{
			if (e == 0)
			{
				BdPlaySE(*(const uint32_t *)(0xCD04B0 + 4 * Variant()), 0, 0x80);
				BdPlaySummonStream(0x80, 0, 0x60);
				QueueTIMUpload(MB() + 0xE000);
				LinkQ1(*(const uint32_t *)(0xCD0990 + 4 * Variant())); // the variant's sword attack
			}
			else if (e == 0x26) LinkFlash(ORIG_FlashOutTask);
			else if (e == 6)
			{
				CameraShake(2, 2, 0xC, 0xFF);
				LinkQ1(ORIG_EndTask);
			}
			else if (e == 7) LinkFeedback(1, 0x2B);
		}
		else if ((e -= 0x32) < 4) // 323
		{
			if (e == 1 && VoiceSlotBusy(VoiceSlot())) ReleaseVoiceSlot(VoiceSlot());
		}
		n->c++;
		if (n->c <= 0x147) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Small screen tasks
	// ------------------------------------------------------------------
	// 0x592A00 / 0x592A50: Battle_RequestScreenFeedback every tick (0x592A50: from its second
	// tick) for +0x0E ticks. The request is issued before the pause test: a per-frame draw-side
	// request, re-issued on held frames.
	static uint32_t __cdecl FeedbackTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RequestScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F);
		if (Paused()) return 0;
		n->c++;
		return n->c >= n->e ? TASK_END : 0;
	}

	static uint32_t __cdecl Feedback2Task(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c > 0) RequestScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F);
		if (Paused()) return 0;
		n->c++;
		return n->c >= n->e ? TASK_END : 0;
	}

	// 0x592AE0: screen flash up to +0x18 over +0x0E ticks
	static uint32_t __cdecl FlashInTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		SetScreenFlash((uint32_t)mul32((int32_t)n->a18 / (int32_t)n->e, n->c), 0);
		if (Paused()) return 0;
		n->c++;
		return n->c >= n->e ? TASK_END : 0;
	}

	// 0x592B60: screen flash down from +0x18, cleared at the end
	static uint32_t __cdecl FlashOutTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t level = n->a18;
		SetScreenFlash((uint32_t)(level - mul32(level / (int32_t)n->e, n->c)), 0);
		if (Paused()) return 0;
		n->c++;
		if (n->c < n->e) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// MAG_327_sub_592C60: full-screen semi-transparent tile (GP0 0x62) + draw mode, OT bucket otz
	static void Tile(int32_t r, int32_t g, int32_t b, int32_t otz)
	{
		uint32_t *p = (uint32_t *)Cursor();
		uint32_t col = ((((((uint32_t)b & 0xFF) | 0x6200u) << 8) | ((uint32_t)g & 0xFF)) << 8) | ((uint32_t)r & 0xFF);
		p[1] = col;
		((uint16_t *)p)[4] = 0;
		((uint16_t *)p)[5] = 0;
		p[0] = 0x3000000;
		((uint16_t *)p)[6] = 0xA00;
		((uint16_t *)p)[7] = 0x6C0;
		InsertPrimAutoDepth(RenderList() + 4 * (uint32_t)otz + 0x44, p);
		p += 4;
		p[0] = 0x1000000;
		p[1] = 0xE1000220;
		InsertPrimAutoDepth(RenderList() + 4 * (uint32_t)otz + 0x44, p);
		Cursor() = (uint32_t)(p + 2);
	}

	// 0x592BC0: grey fade tile, in over 4 ticks, held to 8, out over 16 ticks, 24 ticks
	static int32_t FadeTileLevel(int32_t c)
	{
		if (c < 4) return (mul32(c, 255) / 4) & 0xFF;
		if (c >= 8) return 0xFF - ((mul32(c - 8, 255) / 16) & 0xFF);
		return 0xFF;
	}

	static uint32_t __cdecl FadeTileTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t v = FadeTileLevel(n->c);
		Tile(v, v, v, 0);
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x18 ? TASK_END : 0;
	}

	// 0x596B30: end of effect, waits for the anim-seq task queued at tick 1 to clear +0x0E
	static uint32_t __cdecl EndTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 1) WaitAnimSeq(MB() + 0xE000, &n->e);
		uint16_t done = (uint16_t)n->e;
		n->c++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Lit sprite helpers (flipbooks through InitEffectSequenceFromData 0x571C80, header 0xB4 bytes
	// on the scratch stack, positions lit/transformed through the GTE light matrix)
	// ------------------------------------------------------------------
	// scratch s (0x48 bytes at least): s+0 zero angles, s+8 frame matrix (identity rotation at the
	// position, composed with comp), set as GTE light matrix / back colour
	static void LitSetupAt(uint8_t *s, int32_t x, int32_t y, int32_t z, const Mat4x3 *comp)
	{
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = x;
		*(int32_t *)(s + 0x20) = y;
		*(int32_t *)(s + 0x24) = z;
		ComposeAffineTransform(comp, (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	// s+8 = copy of the frame matrix (the ground puffs / sparks)
	static void LitSetupFrame(uint8_t *s)
	{
		memcpy(s + 8, &Frame(), sizeof(Mat4x3));
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	// one sprite pushed towards the camera by size >> shift (view-space MAC, normalised)
	static void LitSpriteMAC(uint8_t *h, uint8_t *s, const int16_t *pos, int16_t size, int16_t age, int shift, bool set8, int16_t w8)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int16_t *)(h + 4) = age;
		if (set8) *(int32_t *)(h + 8) = w8;
		int32_t *mac = (int32_t *)(s + 0x38), *u = (int32_t *)(s + 0x28);
		GteReadMAC123(mac);
		NormalizeVectorToFixedPoint(mac, u);
		int32_t k = -((int32_t)size >> shift);
		mac[0] = mac[0] + (mul32(u[0], k) >> 12);
		mac[1] = mac[1] + (mul32(u[1], k) >> 12);
		mac[2] = mac[2] + (mul32(u[2], k) >> 12);
		GteSetTransFromVec32(mac);
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	// one sprite at the transformed position itself
	static void LitSprite(uint8_t *h, const int16_t *pos, int16_t size, int16_t age)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int16_t *)(h + 4) = age;
		GteTransFromMAC();
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	// ------------------------------------------------------------------
	// Screen-space sword prims (0x58E120 Zantetsuken / 0x590790 Masamune / 0x591470 Excalibur),
	// 8 ticks: GTE H = 0x120, t.z = 0x120, scale (+0x1C, +0x20, +0x20) growing for 6 ticks
	// ------------------------------------------------------------------
	static uint32_t ScreenPrimModel(const Node24 *n, uint32_t orig)
	{
		if (orig == ORIG_Zan_ScreenPrimTask) return 0xCCDE04;
		if (orig == ORIG_Masa_ScreenPrimTask) return 0xCCF2DC;
		return n->e ? 0xCCEC5C : 0xCCE5DC;
	}

	static void ScreenPrimDraw(const Node24 *n, uint32_t model, int16_t s1C)
	{
		PrimArgs a = Prim(0, 0, n->a18, n->p10, n->p12, 0x120, s1C, n->s20, n->s20, nullptr, model);
		a.screen = true;
		PrimDraw(a);
	}

	static uint32_t ScreenPrimTick(Node24 *n, uint32_t orig)
	{
		ScreenPrimDraw(n, ScreenPrimModel(n, orig), n->s1C);
		MemoNode(n);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c < 6)
		{
			int16_t v = n->s1E;
			n->s1C = (int16_t)(n->s1C + v);
			n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 16));
		}
		n->c = (int16_t)(c + 1);
		return n->c >= 8 ? TASK_END : 0;
	}

	static uint32_t __cdecl ZanScreenPrimTask(TaskNode *tn) { return ScreenPrimTick((Node24 *)tn, ORIG_Zan_ScreenPrimTask); }
	static uint32_t __cdecl MasaScreenPrimTask(TaskNode *tn) { return ScreenPrimTick((Node24 *)tn, ORIG_Masa_ScreenPrimTask); }
	static uint32_t __cdecl ExcaScreenPrimTask(TaskNode *tn) { return ScreenPrimTick((Node24 *)tn, ORIG_Exca_ScreenPrimTask); }

	// ------------------------------------------------------------------
	// Zantetsuken (variant 0): 0x58DED0, 50 ticks. Tick 0 the screen prim; tick 16 per target:
	// skipped targets (record byte 3 bit 2) get their action result at once, the others a cut
	// task (0x58E3E0) and a slice task (0x58E570), delayed by the target index; with no target
	// cut, the party clip task (0x58F940).
	// ------------------------------------------------------------------
	static uint32_t __cdecl ZanTask(TaskNode *tn)
	{
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			Node24 *p = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Zan_ScreenPrimTask);
			p->c = 0;
			p->p10 = (int16_t)0xFF56;
			p->p12 = (int16_t)0xFFE2;
			p->a18 = 0x860;
			p->s1E = 0x1C0;
			p->s1C = 0x1C0;
			p->s20 = 0x600;
		}
		if (n->c == 0x10)
		{
			int32_t spawned = 0;
			bool party = true;
			if (TargetCount() != 0)
			{
				uint32_t off = 0;
				int32_t idx = 0;
				do
				{
					uint8_t *rec = TargetRecs() + off;
					if (rec[3] & 4) ApplyActionResultToTarget(rec);
					else
					{
						uint32_t slot = rec[0];
						Node24 *cut = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Zan_CutTask);
						uint8_t *ent = Entity(slot);
						cut->c = 0;
						cut->e = (int16_t)slot;
						BoneMid(ent, &cut->p10);
						int32_t a = CrtRand() % 0x500 + 0xB00;
						cut->a18 = (int16_t)a;
						if (a & 1) cut->a18 = (int16_t)-a;
						int32_t r = CrtRand() % 0x12C;
						cut->s22 = (int16_t)(-150 - r);
						cut->s20 = (int16_t)(-150 - r);
						Node24 *sl = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Zan_SliceTask);
						uint32_t p0 = *(const uint32_t *)&cut->p10, p1 = *(const uint32_t *)&cut->p14;
						sl->e = (int16_t)slot;
						*(uint32_t *)&sl->p10 = p0;
						int32_t size = *(const int16_t *)(ent + 0x26);
						sl->c = 0;
						*(uint32_t *)&sl->p14 = p1;
						int32_t j = CrtRand() % 256;
						j = mul32(j + 0x80, size) >> 12;
						int32_t j2 = j + j;
						int32_t r1 = CrtRand() % j2;
						sl->p10 = (int16_t)(sl->p10 + (r1 - j));
						int32_t r2 = CrtRand() % j2;
						sl->p12 = (int16_t)(sl->p12 + (r2 - j));
						int32_t r3 = CrtRand() % j2;
						sl->a18 = 0x80;
						sl->a1A = (int16_t)idx;
						sl->p16 = (int16_t)idx;
						cut->p16 = (int16_t)idx;
						sl->p14 = (int16_t)(sl->p14 + (r3 - j));
						spawned++;
					}
					idx++;
					off += 0x18;
				} while ((uint32_t)idx < TargetCount());
				party = spawned == 0;
			}
			if (party)
			{
				Node24 *pc = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Zan_PartyClipTask);
				pc->c = 0;
				pc->e = 0x14;
				pc->s20 = 0;
				pc->s22 = 0x258;
			}
		}
		n->c++;
		return n->c >= 0x32 ? TASK_END : 0;
	}

	// 0x58E3E0: the cut of one target, 40 ticks after its delay (+0x16): per-slot angles
	// 0x2200B7C + 60*slot (x = c*a/40, y = (a/2)*c/40) and matrix 0x2200B94 + 60*slot = entity
	// matrix o (rotation about the bone midpoint, lifted by +0x20, speed +0x22 decaying by 1/8).
	static void CutMatrix(const Node24 *n, int16_t a0, int16_t a1, int16_t a2, int32_t lift, Mat4x3 *out, int16_t *A)
	{
		A[0] = a0;
		A[1] = a1;
		A[2] = a2;
		Mat4x3 m;
		ComposeZYXRotationMatrix(A, &m);
		int16_t v[4] = { 0, 0, 0, 0 };
		MatrixMultiplyVector(&m, &n->p10, v);
		m.t[0] = (int32_t)n->p10 - v[0];
		m.t[1] = lift + n->p12 - v[1];
		m.t[2] = (int32_t)n->p14 - v[2];
		ComposeAffineTransform((const Mat4x3 *)(Entity((uint32_t)(int32_t)n->e) + 0x40), &m, out);
	}

	static int16_t CutAngle0(int32_t c, int16_t a) { return (int16_t)(mul32(c, a) / 40); }
	static int16_t CutAngle1(int32_t c, int16_t a) { return (int16_t)(mul32((int32_t)a / 2, c) / 40); }

	static uint32_t __cdecl ZanCutTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->p16;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->p16 = (int16_t)(d - 1);
			return 0;
		}
		int16_t c = n->c;
		int32_t slot = n->e;
		int16_t *A = (int16_t *)(0x2200B7C + 60 * slot);
		if (c == 0)
		{
			A[2] = 0;
			A[1] = 0;
			A[0] = 0;
		}
		if (c <= 0x28)
		{
			MemoNode(n);
			CutMatrix(n, CutAngle0(c, n->a18), CutAngle1(c, n->a18), A[2], n->s20, (Mat4x3 *)(0x2200B94 + 60 * slot), A);
		}
		if (Paused()) return 0;
		int16_t v = n->s22;
		n->s20 = (int16_t)(n->s20 + v);
		n->s22 = (int16_t)(v - (int16_t)(v >> 3));
		n->c++;
		return n->c >= 0x28 ? TASK_END : 0;
	}

	// MAG_327_sub_58E7D0: target shadow, both root matrices, the clipped model, the bone matrices
	static void TargetDraw(uint8_t *h, uint8_t *ent, const Mat4x3 *moved)
	{
		if (!(ent[0] & 0x20))
			Cursor() = DrawShadow(ent, RenderList() + 0x4064, 0x10, Cursor());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(ent + 0x40), (Mat4x3 *)(h + 0x198));
		ComposeAffineTransform(&Camera(), moved, (Mat4x3 *)(h + 0x1B8));
		*(uint32_t *)(h + 4) = g_held ? (uint32_t)g_vertex_scratch : var<uint32_t>(0x1D98B3C);
		*(uint32_t *)(h + 0x20) = *(uint32_t *)(ent + 0x28);
		*(uint32_t *)(h + 0x10) = *(uint32_t *)(ent + 0x7C);
		Cursor() = SliceRender(*(void **)(ent + 0x64), h, OT(), 2, Cursor());
		BuildBoneMatricesFromPose(ent + 0x60);
	}

	// 0x58E570: the target cut in two by the plane through the bone midpoint (normal = the node
	// angle), one half at the entity matrix, the other at the cut matrix; the target is flagged
	// (+0 |= 0xC) and from tick 24 fades to the background colour (GTE DPCS). 41 ticks after its
	// delay, then the target's action result.
	static void SliceDraw(uint8_t *h, const Node24 *n, uint8_t *ent, const Mat4x3 *moved, int32_t c, int32_t w_override)
	{
		int16_t ang[4] = { 0, 0, n->a18, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		*(uint32_t *)(h + 0x48) = *(const uint32_t *)&n->p10;
		*(uint32_t *)(h + 0x4C) = *(const uint32_t *)&n->p14;
		int16_t *nrm = (int16_t *)(h + 0xA8);
		nrm[0] = 0;
		nrm[1] = (int16_t)0xF000;
		nrm[2] = 0;
		MatrixMultiplyVector(&m, nrm, nrm);
		PlaneDistance((const int16_t *)(h + 0x48), nrm);
		if (c >= 0x18)
		{
			int32_t t = ComputeSin(shl32(c - 0x18, 10) / 16);
			int32_t w = 0x1000 - t;
			if (w_override >= 0) w = w_override;
			uint32_t k = (uint32_t)(shl32(w, 7) >> 12);
			uint32_t col = ((((k | 0x3200u) << 8) | k) << 8) | k;
			*(uint32_t *)(ent + 0x2C) = col;
			GteSetFarColor(var<uint8_t>(0xB8B7D8), var<uint8_t>(0xB8B7D9), var<uint8_t>(0xB8B7DA));
			// the original zeroes the dword of its own argument slot ([esp + 0x4C]) and passes its
			// address: a zero RGBC
			uint32_t zero = 0;
			GteLoadRGBC(&zero);
			GteSetIR0(w);
			GteDPCS();
			GteStoreRGB2(ent + 0x28);
			ent[0x2B] = 2;
			ent[7] = 0;
		}
		TargetDraw(h, ent, moved);
	}

	static uint32_t __cdecl ZanSliceTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->p16;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->p16 = (int16_t)(d - 1);
			return 0;
		}
		if (n->c < 0x28)
		{
			uint8_t *ent = Entity((uint32_t)(int32_t)n->e);
			ent[0] |= 0xC;
			uint8_t *h = (uint8_t *)FieldAlloc(0x1D8);
			SliceDraw(h, n, ent, (const Mat4x3 *)(0x2200B94 + 60 * (int32_t)n->e), n->c, -1);
			FieldFree(0x1D8);
			MemoNode(n);
		}
		if (Paused()) return 0;
		n->c++;
		if (n->c <= 0x28) return 0;
		ApplyActionResultToTarget(TargetRecs() + 24 * (int32_t)n->a1A);
		return TASK_END;
	}

	// MAG_327_sub_58FAA0: the four party entities (stru_1D9898C) through the clipped renderer
	// 0x58FB50 (OT offset / shift per member from the table 0xCD0970); their bone matrices are
	// rebuilt first
	static void PartyDraw(uint8_t *h, const Mat4x3 *m)
	{
		memcpy(h + 0x198, &Camera(), sizeof(Mat4x3));
		ComposeAffineTransform(&Camera(), m, (Mat4x3 *)(h + 0x1B8));
		*(uint32_t *)(h + 4) = g_held ? (uint32_t)g_vertex_scratch : var<uint32_t>(0x1D98B3C);
		*(uint32_t *)(h + 0x10) = 0xFFFFFFFF;
		for (int i = 0; i < 4; i++)
		{
			uint8_t *e = (uint8_t *)(0x1D98991 + 0x2C * i);
			const int32_t *tb = (const int32_t *)(0xCD0970 + 8 * i);
			BuildBoneMatricesFromPose(e + 0x13);
			if (!(e[0] & 1)) continue;
			*(uint32_t *)(h + 0x14) = *(const uint32_t *)(e + 0x27);
			*(uint16_t *)(h + 0x18) = *(const uint16_t *)(e + 1);
			Cursor() = PartyRender(e + 3, h, RenderList() + 4 * (uint32_t)tb[0], tb[1], Cursor());
		}
	}

	// 0x58F940 (no target cut): 65 ticks after a 20-tick delay, the party drawn clipped by the plane
	// y = -0xBB8 tilted by -96 about z, the model matrix sinking by +0x20 (speed +0x22 decaying by
	// 1/8); the party is shown each drawn tick and hidden at the end
	static void PartyClipDraw(uint8_t *h, int32_t z)
	{
		int16_t ang[4] = { 0, 0, (int16_t)0xFFA0, 0 };
		int16_t *p = (int16_t *)(h + 0x48), *nrm = (int16_t *)(h + 0xA8);
		p[0] = 0;
		p[1] = (int16_t)0xF448;
		p[2] = 0;
		nrm[0] = 0;
		nrm[1] = (int16_t)0xF000;
		nrm[2] = 0;
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		nrm[0] = 0;
		nrm[1] = (int16_t)0xF000;
		nrm[2] = 0;
		MatrixMultiplyVector(&m, nrm, nrm);
		PlaneDistance(p, nrm);
		ResetRotation(&m);
		m.m[2][2] = 0x1000;
		m.m[1][1] = 0x1000;
		m.m[0][0] = 0x1000;
		m.t[1] = 0;
		m.t[0] = 0;
		m.t[2] = z;
		PartyDraw(h, &m);
	}

	static uint32_t __cdecl ZanPartyClipTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->e;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->e = (int16_t)(d - 1);
			return 0;
		}
		if (n->c < 0x41)
		{
			ShowParty();
			uint8_t *h = (uint8_t *)FieldAlloc(0x1D8);
			PartyClipDraw(h, n->s20);
			FieldFree(0x1D8);
			MemoNode(n);
		}
		if (Paused()) return 0;
		int16_t v = n->s22;
		n->s20 = (int16_t)(n->s20 - v);
		n->s22 = (int16_t)(v - (int16_t)(v >> 3));
		n->c++;
		if (n->c <= 0x41) return 0;
		HideParty();
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Masamune (variant 1): 0x590510, 50 ticks. Tick 0 a screen prim per target (at the target's
	// screen position); tick 16 per target two prims (0x5908D0 / 0x590A10), sparks (0x590B60) and
	// dust (0x590F10, 3-tick delay), owner mask 1 << k; tick 17 the action result.
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasaTask(TaskNode *tn)
	{
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0 && TargetCount() != 0)
		{
			uint32_t off = 0;
			for (uint32_t k = 0; k < TargetCount(); k++, off += 0x18)
			{
				uint32_t slot = TargetRecs()[off];
				Node24 *p = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Masa_ScreenPrimTask);
				p->c = 0;
				GetDefaultEffectPosition(Entity(slot), &p->p10);
				GteSetRotMatrix(&Camera());
				GteSetTransVector(&Camera());
				GteLoadV0(&p->p10);
				GteRTPS();
				int16_t sxy[2];
				GteReadSXY2(sxy);
				p->p10 = (int16_t)(sxy[0] - 0x168);
				p->p12 = (int16_t)(sxy[1] - 0x134);
				p->a18 = 0xA00;
				p->s1E = 0x1C0;
				p->s1C = 0x1C0;
				p->s20 = 0x300;
			}
		}
		if (n->c == 0x10 && TargetCount() != 0)
		{
			uint32_t off = 0;
			for (uint32_t k = 0; k < TargetCount(); k++, off += 0x18)
			{
				uint32_t slot = TargetRecs()[off];
				Node24 *a = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Masa_PrimATask);
				a->c = 0;
				GetDefaultEffectPosition(Entity(slot), &a->p10);
				a->a18 = 0x200;
				int32_t r = CrtRand() % 0xA00 + 0xA00;
				int16_t v = (int16_t)(r / 3);
				a->s1E = v;
				a->s1C = v;
				Node24 *b = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Masa_PrimBTask);
				int16_t vb = (int16_t)(a->s1E + 0x600);
				uint32_t p0 = *(const uint32_t *)&a->p10, p1 = *(const uint32_t *)&a->p14;
				*(uint32_t *)&b->p10 = p0;
				b->c = 0;
				*(uint32_t *)&b->p14 = p1;
				b->a18 = 0x200;
				b->s1E = vb;
				b->s1C = vb;
				Node24 *s = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Masa_SparkTask);
				*(uint32_t *)&s->p10 = *(const uint32_t *)&a->p10;
				int16_t mask = (int16_t)(1 << k);
				*(uint32_t *)&s->p14 = *(const uint32_t *)&a->p14;
				s->c = 0;
				s->p16 = mask;
				Node24 *d = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Masa_DustTask);
				*(uint32_t *)&d->p10 = *(const uint32_t *)&a->p10;
				*(uint32_t *)&d->p14 = *(const uint32_t *)&a->p14;
				d->c = 0;
				d->e = 3;
				d->p16 = mask;
			}
		}
		if (n->c == 0x11) ApplyActionResultToTargets((uint32_t)TargetRecs(), TargetCount());
		n->c++;
		return n->c >= 0x32 ? TASK_END : 0;
	}

	// 0x5908D0 / 0x590A10: prim at the target, rotated +0x18 about z, uniform scale +0x1C growing
	// (speed +0x1E decaying by 1/4), camera-relative, fading out (from 8: (c-8)*512, from 4:
	// (c-4)*341), 16 ticks
	static bool MasaFadeA(int32_t c, int32_t *v) { if (c < 8) return false; *v = shl32(c - 8, 9); return true; }
	static bool MasaFadeB(int32_t c, int32_t *v) { if (c < 4) return false; *v = mul32(c - 4, 341); return true; }

	static void MasaPrimDraw(const Node24 *n, bool b, int16_t s1C, bool fade, int32_t f)
	{
		PrimArgs a = Prim(0, 0, n->a18, n->p10, n->p12, n->p14, s1C, s1C, s1C, &Camera(), b ? 0xCC877C : 0xCC72FC);
		SetFade(a, fade, f);
		PrimDraw(a);
	}

	static uint32_t MasaPrimTick(Node24 *n, bool b)
	{
		int32_t f = 0;
		bool fade = b ? MasaFadeB(n->c, &f) : MasaFadeA(n->c, &f);
		MasaPrimDraw(n, b, n->s1C, fade, f);
		MemoNode(n);
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->c++;
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 4));
		return n->c >= 0x10 ? TASK_END : 0;
	}

	static uint32_t __cdecl MasaPrimATask(TaskNode *tn) { return MasaPrimTick((Node24 *)tn, false); }
	static uint32_t __cdecl MasaPrimBTask(TaskNode *tn) { return MasaPrimTick((Node24 *)tn, true); }

	// 0x590B60: sparks in pool A (owner mask +0x16), 8 per tick for 3 ticks: random direction,
	// start 300..699 out, speed 120..469 damped by 1/8, spinning (+0x1C angle, +0x1E spin);
	// flipbook 0xCC7120 (h+0x24 = 9), pushed towards the camera by size/8; ends when none is left
	static void MasaSparkSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = 0xCC7120;
		*(uint16_t *)(h + 0x24) = 9;
		LitSetupAt(s, n->p10, n->p12, n->p14, &Camera());
	}

	static uint32_t __cdecl MasaSparkTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 2)
		{
			for (int k = 0; k < 8; k++)
			{
				int i = FreeRecord(pool, 0x20);
				if (i < 0) break;
				RecA *r = (RecA *)(pool + 0x20 * i);
				r->mask = (uint32_t)(int32_t)n->p16;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x800 + 0x600);
				int32_t *v = (int32_t *)(s + 0x28);
				v[0] = CrtRand() % 0x2000 - 0x1000;
				v[1] = CrtRand() % 0x2000 - 0x1000;
				v[2] = CrtRand() % 0x2000 - 0x1000;
				NormalizeVectorToFixedPoint(v, v);
				int32_t d = CrtRand() % 400 + 300;
				r->x = (int16_t)(mul32(v[0], d) >> 12);
				r->y = (int16_t)(mul32(v[1], d) >> 12);
				r->z = (int16_t)(mul32(v[2], d) >> 12);
				int32_t sp = CrtRand() % 350 + 120;
				r->vx = (int16_t)(mul32(v[0], sp) >> 12);
				r->vy = (int16_t)(mul32(v[1], sp) >> 12);
				r->vz = (int16_t)(mul32(v[2], sp) >> 12);
				r->w1C = (int16_t)(CrtRand() % 0x800);
				int32_t spin = CrtRand() % 90 + 30;
				r->w1E = (int16_t)spin;
				if (spin & 1) r->w1E = (int16_t)-spin;
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		MasaSparkSetup(h, s, n);
		int32_t mask = n->p16;
		for (int i = 0; i < POOL_N; i++)
		{
			RecA *r = (RecA *)(pool + 0x20 * i);
			if (!(mask & r->mask)) continue;
			MemoA(i, n);
			LitSpriteMAC(h, s, &r->x, r->size, r->age, 3, true, r->w1C);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->age++;
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			r->w1C = (int16_t)(r->w1C + r->w1E);
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// 0x590F10: dust in pool B (owner mask +0x16) after a 3-tick delay, 8 per tick for 3 ticks, on
	// a ring of 400..699 round the target, speed 90..289 outwards damped by 1/8 (x/z); flipbook
	// 0xCC6DC8 (h+0x24 = 8) at the target's ground point, pushed by size/8
	static void MasaDustSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = 0xCC6DC8;
		*(uint16_t *)(h + 0x24) = 8;
		LitSetupAt(s, n->p10, 0, n->p14, &Camera());
	}

	static uint32_t __cdecl MasaDustTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->e;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->e = (int16_t)(d - 1);
			return 0;
		}
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 2)
		{
			for (int k = 0; k < 8; k++)
			{
				int i = FreeRecord(pool, 0x18);
				if (i < 0) break;
				RecB *r = (RecB *)(pool + 0x18 * i);
				r->mask = (uint32_t)(int32_t)n->p16;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x1000 + 0x500);
				int32_t a = CrtRand() % 0x1000;
				*(int32_t *)(s + 0x28) = ComputeSin(a);
				*(int32_t *)(s + 0x2C) = 0;
				*(int32_t *)(s + 0x30) = ComputeCos(a);
				int32_t rr = CrtRand() % 300 + 400;
				r->x = (int16_t)(mul32(rr, *(int32_t *)(s + 0x28)) >> 12);
				int32_t ry = CrtRand() % 50;
				r->y = (int16_t)-ry;
				r->z = (int16_t)(mul32(rr, *(int32_t *)(s + 0x30)) >> 12);
				int32_t sp = CrtRand() % 200 + 90;
				r->vx = (int16_t)(mul32(sp, *(int32_t *)(s + 0x28)) >> 12);
				r->vz = (int16_t)(mul32(sp, *(int32_t *)(s + 0x30)) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		MasaDustSetup(h, s, n);
		int32_t mask = n->p16;
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *r = (RecB *)(pool + 0x18 * i);
			if (!(mask & r->mask)) continue;
			MemoB(i, n);
			LitSpriteMAC(h, s, &r->x, r->size, r->age, 3, false, 0);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->age++;
			r->x = (int16_t)(r->x + r->vx);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Excalibur / Excalipoor (variants 2 / 3): 0x591250 / 0x5920E0, 50 ticks. Tick 0 the screen prim
	// (+0x0E = Excalipoor); tick 18 per target a burst (0x5915C0), smoke (0x591850, mask 0x100 << k),
	// streaks (0x591B80) and a pillar (0x591F50, 1-tick delay); tick 19 the action result.
	// ------------------------------------------------------------------
	static uint32_t ExcaTick(Node24 *n, int16_t poor)
	{
		if (Paused()) return 0;
		if (n->c == 0)
		{
			Node24 *p = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Exca_ScreenPrimTask);
			p->c = 0;
			p->e = poor;
			p->p10 = (int16_t)0xFF56;
			p->p12 = (int16_t)0xFF9C;
			p->a18 = 0x968;
			p->s1E = 0x555;
			p->s1C = 0x555;
			p->s20 = 0x1000;
		}
		if (n->c == 0x12 && TargetCount() != 0)
		{
			uint32_t off = 0;
			for (uint32_t k = 0; k < TargetCount(); k++, off += 0x18)
			{
				uint32_t slot = TargetRecs()[off];
				Node24 *b = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Exca_BurstTask);
				b->c = 0;
				b->e = poor;
				GetDefaultEffectPosition(Entity(slot), &b->p10);
				int16_t bit = (int16_t)(1 << k);
				b->p16 = bit;
				Node24 *s = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Exca_SmokeTask);
				*(uint32_t *)&s->p10 = *(const uint32_t *)&b->p10;
				*(uint32_t *)&s->p14 = *(const uint32_t *)&b->p14;
				s->c = 0;
				s->e = poor;
				s->p16 = (int16_t)(0x100 << k);
				Node24 *t = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Exca_StreakTask);
				*(uint32_t *)&t->p10 = *(const uint32_t *)&b->p10;
				*(uint32_t *)&t->p14 = *(const uint32_t *)&b->p14;
				t->c = 0;
				t->p16 = bit;
				Node24 *q = (Node24 *)AddTaskToQueue(&Q1(), ORIG_Exca_PillarTask);
				*(uint32_t *)&q->p10 = *(const uint32_t *)&b->p10;
				q->c = 0;
				q->e = 1;
				*(uint32_t *)&q->p14 = *(const uint32_t *)&b->p14;
				q->a18 = 0x800;
				int32_t r1 = CrtRand() % 0x200 + 0x300;
				q->s1E = (int16_t)(r1 / 3);
				q->s1C = (int16_t)(r1 / 3);
				int32_t r2 = CrtRand() % 0x300 + 0x300;
				q->s22 = (int16_t)(r2 / 3);
				q->s20 = (int16_t)(r2 / 3);
			}
		}
		if (n->c == 0x13) ApplyActionResultToTargets((uint32_t)TargetRecs(), TargetCount());
		n->c++;
		return n->c >= 0x32 ? TASK_END : 0;
	}

	static uint32_t __cdecl ExcaTask(TaskNode *tn) { return ExcaTick((Node24 *)tn, 0); }
	static uint32_t __cdecl PoorTask(TaskNode *tn) { return ExcaTick((Node24 *)tn, 1); }

	// 0x5915C0: burst in pool B, 2 per tick for 2 ticks at a random offset (+-600), standing,
	// flipbook 0xCC6730 (Excalipoor 0xCC6954), pushed by size/8; ends when none is left
	static void ExcaBurstSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = n->e ? 0xCC6954 : 0xCC6730;
		*(uint16_t *)(h + 0x24) = 8;
		LitSetupAt(s, n->p10, n->p12, n->p14, &Camera());
	}

	static uint32_t __cdecl ExcaBurstTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 1)
		{
			for (int k = 0; k < 2; k++)
			{
				int i = FreeRecord(pool, 0x18);
				if (i < 0) break;
				RecB *r = (RecB *)(pool + 0x18 * i);
				r->mask = (uint32_t)(int32_t)n->p16;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x2000 + 0x600);
				r->x = (int16_t)(CrtRand() % 0x4B0 - 0x258);
				r->y = (int16_t)(CrtRand() % 0x4B0 - 0x258);
				r->z = (int16_t)(CrtRand() % 0x4B0 - 0x258);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		ExcaBurstSetup(h, s, n);
		int32_t mask = n->p16;
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *r = (RecB *)(pool + 0x18 * i);
			if (!(mask & r->mask)) continue;
			MemoB(i, n);
			LitSpriteMAC(h, s, &r->x, r->size, r->age, 3, false, 0);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->age++;
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// 0x591850: smoke in pool B (mask 0x100 << k), 6 per tick for 3 ticks, rising (y dir
	// -0x800..-0xFFF), start 200..699 out, speed 100..399 damped by 1/8; flipbook 0xCC6F74 (h+0x24 =
	// 0xC), colour 0x808080 (Excalipoor 0x404080)
	static void ExcaSmokeSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = 0xCC6F74;
		*(uint16_t *)(h + 0x24) = 0xC;
		*(uint32_t *)(h + 0x1C) = n->e ? 0x404080u : 0x808080u;
		LitSetupAt(s, n->p10, n->p12, n->p14, &Camera());
	}

	static uint32_t __cdecl ExcaSmokeTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 2)
		{
			for (int k = 0; k < 6; k++)
			{
				int i = FreeRecord(pool, 0x18);
				if (i < 0) break;
				RecB *r = (RecB *)(pool + 0x18 * i);
				r->mask = (uint32_t)(int32_t)n->p16;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x600 + 0x300);
				int32_t *v = (int32_t *)(s + 0x28);
				v[0] = CrtRand() % 0x2000 - 0x1000;
				v[1] = CrtRand() % 0x2000 - 0x1000;
				v[2] = -0x800 - CrtRand() % 0x800;
				NormalizeVectorToFixedPoint(v, v);
				int32_t d = CrtRand() % 500 + 200;
				r->x = (int16_t)(mul32(v[0], d) >> 12);
				r->y = (int16_t)(mul32(v[1], d) >> 12);
				r->z = (int16_t)(mul32(v[2], d) >> 12);
				int32_t sp = CrtRand() % 300 + 100;
				r->vx = (int16_t)(mul32(v[0], sp) >> 12);
				r->vy = (int16_t)(mul32(v[1], sp) >> 12);
				r->vz = (int16_t)(mul32(v[2], sp) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		ExcaSmokeSetup(h, s, n);
		int32_t mask = n->p16;
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *r = (RecB *)(pool + 0x18 * i);
			if (!(r->mask & mask)) continue;
			MemoB(i, n);
			LitSprite(h, &r->x, r->size, r->age);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->age++;
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// 0x591B80: streaks in pool A (mask 1 << k), 4 per tick for 3 ticks: random direction (kept as
	// s16 at +0x18), start 400..799 out, speed 140..519 damped by 1/8; each drawn as flipbook
	// 0xCC6C0C frame 0 oriented along its direction (rotation (0,-0x1000,0) -> dir), scale (0xC00,
	// size, 0); from age 6 the length shrinks ((0x1FFC - 682*age) * initial >> 12), 13 ticks
	static void ExcaStreakSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = 0xCC6C0C;
		*(uint16_t *)(h + 0x24) = 8;
		*(uint16_t *)(h + 4) = 0;
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = n->p10;
		*(int32_t *)(s + 0x20) = n->p12;
		*(int32_t *)(s + 0x24) = n->p14;
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x48) = 0;
		*(int32_t *)(s + 0x4C) = -0x1000;
		*(int32_t *)(s + 0x50) = 0;
		*(int32_t *)(s + 0x78) = 0xC00;
		*(int32_t *)(s + 0x80) = 0;
	}

	static void ExcaStreakOne(uint8_t *h, uint8_t *s, const int16_t *dir, const int16_t *pos, int16_t size)
	{
		int32_t *v = (int32_t *)(s + 0x58);
		v[0] = dir[0];
		v[1] = dir[1];
		v[2] = dir[2];
		int32_t angle = GetRotationBetweenVectors(s + 0x48, v, (int32_t *)(s + 0x68));
		Mat4x3 *m = (Mat4x3 *)(s + 0x28);
		BuildAxisAngleRotationMatrix(angle, m, (const int32_t *)(s + 0x68));
		m->t[0] = pos[0];
		m->t[1] = pos[1];
		m->t[2] = pos[2];
		*(int32_t *)(s + 0x7C) = size;
		Scale3DMatrix(m, (const int32_t *)(s + 0x78));
		ComposeAffineTransform((const Mat4x3 *)(s + 8), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
	}

	static int16_t StreakSize(int16_t age, int16_t size0) { return (int16_t)(mul32(0x1FFC - shl32(mul32(age, 341), 1), size0) >> 12); }

	static uint32_t __cdecl ExcaStreakTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x88);
		if (!Paused() && n->c <= 2)
		{
			for (int k = 0; k < 4; k++)
			{
				int i = FreeRecord(pool, 0x20);
				if (i < 0) break;
				RecA *r = (RecA *)(pool + 0x20 * i);
				r->mask = (uint32_t)(int32_t)n->p16;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x1000 + 0x600);
				int32_t *v = (int32_t *)(s + 0x58);
				v[0] = CrtRand() % 0x2000 - 0x1000;
				v[1] = CrtRand() % 0x2000 - 0x1000;
				v[2] = CrtRand() % 0x2000 - 0x1000;
				NormalizeVectorToFixedPoint(v, v);
				int32_t d = CrtRand() % 400 + 400;
				r->x = (int16_t)(mul32(v[0], d) >> 12);
				r->y = (int16_t)(mul32(v[1], d) >> 12);
				r->z = (int16_t)(mul32(v[2], d) >> 12);
				r->w0E = r->size;
				int32_t sp = CrtRand() % 380 + 140;
				r->vx = (int16_t)(mul32(v[0], sp) >> 12);
				r->vy = (int16_t)(mul32(v[1], sp) >> 12);
				r->vz = (int16_t)(mul32(v[2], sp) >> 12);
				r->w18 = (int16_t)v[0];
				r->w1A = (int16_t)v[1];
				r->w1C = (int16_t)v[2];
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		ExcaStreakSetup(h, s, n);
		int32_t mask = n->p16;
		for (int i = 0; i < POOL_N; i++)
		{
			RecA *r = (RecA *)(pool + 0x20 * i);
			if (!(mask & r->mask)) continue;
			MemoA(i, n);
			ExcaStreakOne(h, s, &r->w18, &r->x, r->size);
			if (r->age >= 6) r->size = StreakSize(r->age, r->w0E);
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
			if (Paused()) continue;
			if (r->age >= 0xC)
			{
				r->mask = 0;
				continue;
			}
			r->age++;
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x88);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// 0x591F50: pillar at the target's ground point after a 1-tick delay, rotated +0x18 about y,
	// scale (+0x1C, +0x20, +0x1C) growing (speeds decaying by 1/4), camera-relative, fading from 4,
	// 16 ticks
	static void ExcaPillarDraw(const Node24 *n, int16_t a18, int16_t s1C, int16_t s20, bool fade, int32_t f)
	{
		PrimArgs a = Prim(0, a18, 0, n->p10, 0, n->p14, s1C, s20, s1C, &Camera(), 0xCCFAB4);
		SetFade(a, fade, f);
		PrimDraw(a);
	}

	static uint32_t __cdecl ExcaPillarTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->e;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->e = (int16_t)(d - 1);
			return 0;
		}
		int32_t f = 0;
		bool fade = MasaFadeB(n->c, &f);
		ExcaPillarDraw(n, n->a18, n->s1C, n->s20, fade, f);
		MemoNode(n);
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 4));
		int16_t w = n->s22;
		n->s20 = (int16_t)(n->s20 + w);
		n->c++;
		n->s22 = (int16_t)(w - (int16_t)((int32_t)w / 4));
		return n->c >= 0x10 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ground rings (tick 8), 68 ticks, frame-relative: 0x592E90 still (prim 0xCC8F5C, mode 1),
	// 0x592F90 / 0x5930C0 spinning (+0x18 += +0x1A), fade value +0x22 (h+0x1C = 0xC3)
	// ------------------------------------------------------------------
	static void RingDraw(const Node24 *n)
	{
		PrimArgs a = Prim(0, 0, 0, n->p10, n->p12, n->p14, n->s1C, n->s20, n->s1C, &Frame(), 0xCC8F5C);
		a.mode = 1;
		a.h1C = 0;
		PrimDraw(a);
	}

	static uint32_t __cdecl RingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RingDraw(n);
		MemoNode(n);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x44 ? TASK_END : 0;
	}

	static void RingSpinDraw(const Node24 *n, bool b, int16_t a18)
	{
		uint32_t model = b ? (n->e ? 0xCCAD3C : 0xCCA14C) : (n->e ? 0xCCC434 : 0xCCB92C);
		PrimArgs a = b ? Prim(0, a18, 0, n->p10, n->p12, n->p14, n->s1C, n->s1C, n->s1C, &Frame(), model)
			: Prim(0, a18, 0, n->p10, n->p12, n->p14, n->s1C, n->s20, n->s1C, &Frame(), model);
		a.h1C = 0xC3;
		a.setC = true;
		a.hC = n->s22;
		PrimDraw(a);
	}

	static uint32_t RingSpinTick(Node24 *n, bool b)
	{
		RingSpinDraw(n, b, n->a18);
		MemoNode(n);
		if (Paused()) return 0;
		n->a18 = (int16_t)(n->a18 + n->a1A);
		n->c++;
		return n->c >= 0x44 ? TASK_END : 0;
	}

	static uint32_t __cdecl RingSpinTask(TaskNode *tn) { return RingSpinTick((Node24 *)tn, false); }
	static uint32_t __cdecl RingSpin2Task(TaskNode *tn) { return RingSpinTick((Node24 *)tn, true); }

	// ------------------------------------------------------------------
	// Flare row (0x593250, tick 18), 22 ticks: 8 flipbook sprites 0xCC6B64 (table 0xCD03A8: frame,
	// size, offset) along the axis from the flare origin towards the camera eye (0xB8B7F0), each
	// projected and drawn in screen space (MAG_327_sub_593490: GTE matrix 0x2201060 = size * I at
	// the screen position / 8 - (160, 108), z = H); colour 0x00vvvv00 in over 2 ticks, out from 10,
	// halved on odd ticks; the spread +0x20 grows (speed +0x22 decaying by 1/16)
	// ------------------------------------------------------------------
	static void ScreenSpriteMatrix(const int16_t *v, int16_t size, int32_t zoff)
	{
		var<int16_t>(0x2201070) = size;
		var<int16_t>(0x2201068) = size;
		var<int16_t>(0x2201060) = size;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(v);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy);
		int16_t sx = (int16_t)((int32_t)sxy[0] / 8);
		int16_t sy = (int16_t)((int32_t)sxy[1] / 8);
		var<int32_t>(0x2201074) = (int32_t)sx - 0xA0;
		var<int32_t>(0x2201078) = (int32_t)sy - 0x6C;
		var<int32_t>(0x220107C) = (int32_t)var<int16_t>(0x1D8E038) + zoff;
		GteSetRotMatrix((const Mat4x3 *)0x2201060);
		GteSetTransVector((const Mat4x3 *)0x2201060);
	}

	static uint32_t FlareColour(int32_t c)
	{
		int32_t v;
		if (c < 2) v = shl32(c, 6);
		else if (c >= 10) v = 0xE4 - c * 10;
		else v = 0x80;
		if (c & 1) v >>= 1;
		return (uint32_t)shl32(shl32(v, 8) | v, 8);
	}

	static void FlareDraw(const Node24 *n, int32_t spread, int32_t depth, uint32_t colour)
	{
		int32_t ref[3] = { 0, 0, -0x1000 };
		int32_t d[3];
		d[0] = (int32_t)var<int16_t>(0xB8B7F0) - n->p10;
		d[1] = (int32_t)var<int16_t>(0xB8B7F2) - n->p12;
		d[2] = (int32_t)var<int16_t>(0xB8B7F4) - n->p14;
		NormalizeVectorToFixedPoint(d, d);
		int32_t axis[4];
		int32_t angle = GetRotationBetweenVectors(ref, d, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = n->p10;
		m.t[1] = n->p12;
		m.t[2] = n->p14;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = 0xCC6B64;
		*(uint16_t *)(h + 0x24) = 4;
		*(uint32_t *)(h + 0x1C) = colour;
		int32_t a = spread + 0x200;
		int32_t b = spread / 2 - 0x100;
		for (uint32_t t = 0xCD03AA; t < 0xCD03EA; t += 8)
		{
			int32_t k = mul32(*(const int16_t *)(t + 2), 2200) >> 12;
			int16_t v[4];
			v[0] = (int16_t)(mul32(k, a) >> 12);
			v[2] = (int16_t)(mul32(k, depth) >> 12);
			v[1] = (int16_t)(mul32(k, b) >> 12);
			v[3] = 0;
			MatrixMultiplyVector(&m, v, v);
			v[0] = (int16_t)(v[0] + (int16_t)m.t[0]);
			v[1] = (int16_t)(v[1] + (int16_t)m.t[1]);
			v[2] = (int16_t)(v[2] + (int16_t)m.t[2]);
			ScreenSpriteMatrix(v, *(const int16_t *)t, 0);
			*(int16_t *)(h + 4) = *(const int16_t *)(t - 2);
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
		}
		FieldFree(0xB4);
	}

	static int32_t FlareDepth(int32_t c) { return mul32(c, 45) - 0xE00; }

	static uint32_t __cdecl FlareTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		FlareDraw(n, n->s20, FlareDepth(c), FlareColour(c));
		MemoNode(n);
		if (Paused()) return 0;
		int16_t v = n->s22;
		n->s20 = (int16_t)(n->s20 + v);
		n->c++;
		n->s22 = (int16_t)(v - (int16_t)(v >> 4));
		return n->c >= 0x16 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Risers (0x593600, tick 44, four on a circle), 30 ticks: prim 0xCC78EC, frame-relative,
	// rotated +0x18 about y, uniform scale +0x1C growing by 1/64 per tick, rising by +0x22
	// (accelerating by 1/6), fading in over 6 ticks
	// ------------------------------------------------------------------
	static bool RiserFade(int32_t c, int32_t *v) { if (c >= 6) return false; *v = 0x1000 - shl32(mul32(c, 341), 1); return true; }

	static void RiserDraw(const Node24 *n, int16_t y, int16_t s1C, bool fade, int32_t f)
	{
		PrimArgs a = Prim(0, n->a18, 0, n->p10, y, n->p14, s1C, s1C, s1C, &Frame(), 0xCC78EC);
		SetFade(a, fade, f);
		PrimDraw(a);
	}

	static uint32_t __cdecl RiserTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t f = 0;
		bool fade = RiserFade(n->c, &f);
		RiserDraw(n, n->p12, n->s1C, fade, f);
		MemoNode(n);
		if (Paused()) return 0;
		int16_t s = n->s1C, v = n->s22;
		n->p12 = (int16_t)(n->p12 + v);
		n->s1C = (int16_t)((int16_t)(s >> 6) + s);
		n->c++;
		n->s22 = (int16_t)((int32_t)v / 6 + v);
		return n->c >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ground puffs (0x593770, tick 113): pool B mask 1, 8 per tick while the counter <= 62 on a
	// ring shrinking from 2700 (27 per tick), standing; flipbook 0xCC6DC8 frame-relative. 68 ticks,
	// then every mask-1 record is freed.
	// ------------------------------------------------------------------
	static void PuffSetup(uint8_t *h, uint8_t *s)
	{
		*(uint32_t *)h = 0xCC6DC8;
		memcpy(s + 8, &Frame(), sizeof(Mat4x3));
		*(uint16_t *)(h + 0x24) = 0;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static uint32_t __cdecl PuffTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 0x3E)
		{
			int32_t r = mul32(100 - n->c, 27);
			for (int k = 0; k < 8; k++)
			{
				int i = FreeRecord(pool, 0x18);
				if (i < 0) break;
				RecB *p = (RecB *)(pool + 0x18 * i);
				p->mask = 1;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0x800 + 0x600);
				int32_t a = CrtRand() % 0x1000;
				p->x = (int16_t)(mul32(ComputeSin(a), r) >> 12);
				int32_t ry = CrtRand() % 50;
				p->y = (int16_t)-ry;
				p->z = (int16_t)(mul32(ComputeCos(a), r) >> 12);
				int32_t jx = CrtRand() % 40;
				p->x = (int16_t)(p->x + (jx - 0x14));
				int32_t jz = CrtRand() % 40;
				p->z = (int16_t)(p->z + (jz - 0x14));
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		PuffSetup(h, s);
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *p = (RecB *)(pool + 0x18 * i);
			if (!(p->mask & 1)) continue;
			MemoB(i, n);
			LitSprite(h, &p->x, p->size, p->age);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0) p->mask = 0;
			else p->age++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		if (n->c < 0x44) return 0;
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *p = (RecB *)(pool + 0x18 * i);
			if (p->mask & 1) p->mask = 0;
		}
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Ground sparks (0x5939C0, tick 101): pool A mask 0x10, 12 per tick for 9 ticks on a ring
	// growing from 200 (145 per tick), speed 150..249 outwards damped by 1/8 (x/z); flipbook
	// 0xCC6DC8 frame-relative. Ends when none is left.
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 8)
		{
			int32_t r = mul32(n->c, 0x91) + 0xC8;
			for (int k = 0; k < 12; k++)
			{
				int i = FreeRecord(pool, 0x20);
				if (i < 0) break;
				RecA *p = (RecA *)(pool + 0x20 * i);
				p->mask = 0x10;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0x800 + 0x600);
				int32_t a = CrtRand() % 0x1000;
				int32_t sn = ComputeSin(a);
				*(int32_t *)(s + 0x28) = sn;
				int32_t cs = ComputeCos(a);
				*(int32_t *)(s + 0x30) = cs;
				p->x = (int16_t)(mul32(sn, r) >> 12);
				int32_t ry = CrtRand() % 50;
				p->y = (int16_t)-ry;
				p->z = (int16_t)(mul32(cs, r) >> 12);
				int32_t jx = CrtRand() % 60;
				p->x = (int16_t)(p->x + (jx - 0x1E));
				int32_t jz = CrtRand() % 60;
				p->z = (int16_t)(p->z + (jz - 0x1E));
				int32_t sp = CrtRand() % 100 + 0x96;
				p->vx = (int16_t)(mul32(sn, sp) >> 12);
				p->vz = (int16_t)(mul32(cs, sp) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		PuffSetup(h, s);
		for (int i = 0; i < POOL_N; i++)
		{
			RecA *p = (RecA *)(pool + 0x20 * i);
			if (!(p->mask & 0x10)) continue;
			MemoA(i, n);
			LitSprite(h, &p->x, p->size, p->age);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				p->mask = 0;
				continue;
			}
			p->age++;
			p->x = (int16_t)(p->x + p->vx);
			p->z = (int16_t)(p->z + p->vz);
			p->vx = (int16_t)(p->vx - (int16_t)(p->vx >> 3));
			p->vz = (int16_t)(p->vz - (int16_t)(p->vz >> 3));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Rising rings: spawner 0x593C70 (tick 123, 30 ticks, one ring on ticks 1, 9, 17) and ring
	// 0x593D20 (30 ticks): prim 0xCCCB1C frame-relative, spinning (+0x1A = 0x32), height scale +0x20
	// growing (speed +0x22 accelerating by 1/6), fading out from 18
	// ------------------------------------------------------------------
	static uint32_t __cdecl RisingRingSpawnerTask(TaskNode *tn)
	{
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		int16_t c = n->c;
		if (c < 0x14 && (int32_t)c % 8 == 1)
		{
			Node24 *r = (Node24 *)AddTaskToQueue(&Q1(), ORIG_RisingRingTask);
			r->c = 0;
			r->p10 = 0;
			r->p12 = 0;
			r->p14 = 0;
			r->a18 = (int16_t)(CrtRand() % 0x1000);
			r->a1A = 0x32;
			r->s1C = 0x2400;
			r->s22 = 0xBA;
			r->s20 = 0xBA;
		}
		n->c++;
		return n->c >= 0x1E ? TASK_END : 0;
	}

	static bool RisingRingFade(int32_t c, int32_t *v) { if (c < 0x12) return false; *v = mul32(c - 0x12, 341); return true; }

	static void RisingRingDraw(const Node24 *n, int16_t a18, int16_t s20, bool fade, int32_t f)
	{
		PrimArgs a = Prim(0, a18, 0, n->p10, n->p12, n->p14, n->s1C, s20, n->s1C, &Frame(), 0xCCCB1C);
		SetFade(a, fade, f);
		PrimDraw(a);
	}

	static uint32_t __cdecl RisingRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t f = 0;
		bool fade = RisingRingFade(n->c, &f);
		RisingRingDraw(n, n->a18, n->s20, fade, f);
		MemoNode(n);
		if (Paused()) return 0;
		int16_t v = n->s22;
		n->a18 = (int16_t)(n->a18 + n->a1A);
		n->s20 = (int16_t)(n->s20 + v);
		n->c++;
		n->s22 = (int16_t)((int32_t)v / 6 + v);
		return n->c >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Dots (0x593E80, tick 122): pool A mask 1, 10 per tick while the counter <= 40 on a ring
	// shrinking from 1900 (35 per tick), thrown out (90..329, damped by 1/8) and up (-10..-59,
	// accelerating by 1/16); one GP0 0x6A dot each (colour table 0xCD0428 by age, OT = SZ3 >> 4),
	// 16 ticks per dot. Ends when no dot moved.
	// ------------------------------------------------------------------
	static uint32_t DotEmit(uint8_t *s, uint32_t pk)
	{
		GteReadFLAG(s + 0x50);
		if (*(uint32_t *)(s + 0x50) & 0x60000) return pk;
		GteReadSXY2((void *)(pk + 8));
		GteReadSZ3(s + 0x54);
		uint32_t bucket = *(uint32_t *)(s + 0x48) + 4 * (uint32_t)(*(int32_t *)(s + 0x54) >> (*(uint32_t *)(s + 0x4C) & 31));
		InsertPrimAutoDepth(bucket, (void *)pk);
		return pk + 0xC;
	}

	static uint32_t __cdecl DotTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x58);
		if (!Paused() && n->c <= 0x28)
		{
			int32_t r = 0x76C - mul32(n->c, 35);
			for (int k = 0; k < 10; k++)
			{
				int i = FreeRecord(pool, 0x20);
				if (i < 0) break;
				RecA *p = (RecA *)(pool + 0x20 * i);
				p->mask = 1;
				p->age = 0;
				int32_t a = CrtRand() % 0x1000;
				int32_t sn = ComputeSin(a);
				*(int32_t *)(s + 0x28) = sn;
				int32_t cs = ComputeCos(a);
				*(int32_t *)(s + 0x30) = cs;
				p->x = (int16_t)(mul32(sn, r) >> 12);
				int32_t ry = CrtRand() % 90;
				p->y = (int16_t)-ry;
				p->z = (int16_t)(mul32(cs, r) >> 12);
				int32_t sp = CrtRand() % 240 + 0x5A;
				p->vx = (int16_t)(mul32(sn, sp) >> 12);
				int32_t up = CrtRand() % 50;
				p->vy = (int16_t)(-10 - up);
				p->vz = (int16_t)(mul32(cs, sp) >> 12);
			}
		}
		GteSetRotMatrix(&Frame());
		GteSetTransVector(&Frame());
		uint32_t pk = Cursor();
		int32_t count = 0;
		*(uint32_t *)(s + 0x48) = OT();
		*(uint32_t *)(s + 0x4C) = 4;
		for (int i = 0; i < POOL_N; i++)
		{
			RecA *p = (RecA *)(pool + 0x20 * i);
			if (!(p->mask & 1)) continue;
			MemoA(i, n);
			GteLoadV0(&p->x);
			GteRTPS();
			int16_t age = p->age;
			*(uint32_t *)pk = 0x2000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)(0xCD0428 + 4 * age);
			if (!Paused())
			{
				age = (int16_t)(age + 1);
				p->age = age;
				if (age >= 0x10) p->mask = 0;
				else
				{
					p->x = (int16_t)(p->x + p->vx);
					p->y = (int16_t)(p->y + p->vy);
					p->z = (int16_t)(p->z + p->vz);
					p->vx = (int16_t)(p->vx - (int16_t)(p->vx >> 3));
					p->vy = (int16_t)((int16_t)(p->vy >> 4) + p->vy);
					p->vz = (int16_t)(p->vz - (int16_t)(p->vz >> 3));
					count++;
				}
			}
			pk = DotEmit(s, pk);
		}
		Cursor() = pk;
		FieldFree(0x58);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Spinners (0x594210, tick 121, three from the table 0xCD0468) after their delay, 14 ticks:
	// prim 0xCC7A94 frame-relative, spinning (+0x1A), scale (+0x1C, +0x20, +0x1C) with +0x1C
	// growing (speed decaying by 1/6), fading in over 4 ticks and out from 6
	// ------------------------------------------------------------------
	static bool SpinnerFade(int32_t c, int32_t *v)
	{
		if (c < 4) { *v = shl32(4 - c, 10); return true; }
		if (c >= 6) { *v = shl32(c - 6, 9); return true; }
		return false;
	}

	static void SpinnerDraw(const Node24 *n, int16_t a18, int16_t s1C, bool fade, int32_t f)
	{
		PrimArgs a = Prim(0, a18, 0, n->p10, n->p12, n->p14, s1C, n->s20, s1C, &Frame(), 0xCC7A94);
		SetFade(a, fade, f);
		PrimDraw(a);
	}

	static uint32_t __cdecl SpinnerTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->e;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->e = (int16_t)(d - 1);
			return 0;
		}
		int32_t f = 0;
		bool fade = SpinnerFade(n->c, &f);
		SpinnerDraw(n, n->a18, n->s1C, fade, f);
		MemoNode(n);
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->a18 = (int16_t)(n->a18 + n->a1A);
		n->s1C = (int16_t)(n->s1C + v);
		n->c++;
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 6));
		return n->c >= 0xE ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Sword trail (0x5943D0, tick 263), 10 ticks: the variant's sword E[variant] base (bone 1) and
	// a point 1400 along the blade go into a 10-entry history 0x2200DF8 (slot c % 10); the entries
	// (newest first, up to 10) are splined into 32 samples per rail (0x21FFA78 / 0x21FFB78,
	// sub_571620 / sub_571690, t = k * 0x1000 / 31) and drawn as 30 segments of two gouraud quads
	// (GP0 0x3A): the rail colour (table 0xCD0490) fading to the far colour (0) along the trail
	// (GTE DPCS, IR0 = k * 0x1000 / 30), and a black edge quad.
	// ------------------------------------------------------------------
	static const uint32_t TRAIL_RING = 0x2200DF8;  // 10 x 0x14: alive, base (s16 x4), tip (s16 x4)
	static const uint32_t TRAIL_PTS_A = 0x21FF2E8; // gathered points (8 bytes each)
	static const uint32_t TRAIL_PTS_B = 0x21FF340;
	static const uint32_t TRAIL_SPL_A = 0x21FFA78; // 32 samples each (8 bytes)
	static const uint32_t TRAIL_SPL_B = 0x21FFB78;

	// base and tip (1400 along the blade) of the variant's sword
	static void TrailPoints(uint8_t *s)
	{
		int16_t *p0 = (int16_t *)(s + 0x20), *p1 = (int16_t *)(s + 0x28);
		GetEffectSpawnPosition(Sword(Variant()), 1, 0, p0);
		GetEffectSpawnPosition(Sword(Variant()), 1, 0x1000, p1);
		int32_t *d = (int32_t *)(s + 0x30);
		d[0] = (int32_t)p1[0] - p0[0];
		d[1] = (int32_t)p1[1] - p0[1];
		d[2] = (int32_t)p1[2] - p0[2];
		NormalizeVectorToFixedPoint(d, d);
		p1[0] = (int16_t)((int16_t)(mul32(d[0], 1400) >> 12) + p0[0]);
		p1[1] = (int16_t)((int16_t)(mul32(d[1], 1400) >> 12) + p0[1]);
		p1[2] = (int16_t)((int16_t)(mul32(d[2], 1400) >> 12) + p0[2]);
	}

	// newest-first gather of the history into the two point lists; returns the count
	static int32_t TrailGather(const uint8_t *ring, int32_t idx, uint8_t *pa, uint8_t *pb)
	{
		int32_t cnt = 0;
		do
		{
			const uint8_t *r = ring + 0x14 * idx;
			if (!*(const uint32_t *)r) break;
			idx--;
			*(uint32_t *)(pa + 8 * cnt) = *(const uint32_t *)(r + 4);
			*(uint32_t *)(pa + 8 * cnt + 4) = *(const uint32_t *)(r + 8);
			*(uint32_t *)(pb + 8 * cnt) = *(const uint32_t *)(r + 0xC);
			*(uint32_t *)(pb + 8 * cnt + 4) = *(const uint32_t *)(r + 0x10);
			if (idx < 0) idx = 9;
			cnt++;
		} while (cnt < 10);
		return cnt;
	}

	static void TrailSplines(int32_t cnt, const uint8_t *pa, const uint8_t *pb, uint8_t *sa, uint8_t *sb)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x190);
		SplineSetup(cnt, pa, w);
		for (int32_t k = 0, t = 0; k < 32; k++, t += 0x1000) SplineEval(cnt, w, sa + 8 * k, t / 31);
		SplineSetup(cnt, pb, w);
		for (int32_t k = 0, t = 0; k < 32; k++, t += 0x1000) SplineEval(cnt, w, sb + 8 * k, t / 31);
		FieldFree(0x190);
	}

	static void TrailDraw(const uint8_t *sa, const uint8_t *sb, uint8_t *s)
	{
		GteSetRotMatrix(&Frame());
		GteSetTransVector(&Frame());
		GteSetFarColor2(0, 0, 0);
		uint32_t colour = *(const uint32_t *)(0xCD0490 + 4 * Variant());
		*(uint32_t *)(s + 0x14) = colour;
		*(uint32_t *)(s + 0x1C) = colour;
		uint32_t *pk = (uint32_t *)Cursor();
		uint32_t *pk2 = pk + 9;
		int32_t tacc = 0;
		for (int32_t off = 0; off < 0xF0; off += 8, tacc += 0x1000)
		{
			*(uint32_t *)(s + 0x10) = *(uint32_t *)(s + 0x14);
			pk[0] = 0x8000000;
			GteLoadV012(sa + off, sa + off + 8, sb + off);
			GteRTPT();
			GteReadFLAG(s + 0xC);
			if (*(uint32_t *)(s + 0xC) & 0x60000) continue;
			GteReadSXY012Split(pk + 2, pk + 4, pk + 6);
			GteLoadV0(sb + off + 8);
			GteRTPS();
			GteReadSXY2(pk + 8);
			GteAVSZ4();
			GteReadOTZ32(s + 8);
			GteSetIR0(tacc / 30);
			GteLoadRGBC(s + 0x1C);
			GteDPCS();
			GteStoreRGB2(s + 0x14);
			pk[5] = pk[1] = *(uint32_t *)(s + 0x10);
			pk[7] = pk[3] = *(uint32_t *)(s + 0x14);
			InsertPrimAutoDepth(RenderList() + 4 * (uint32_t)(*(int32_t *)(s + 8) >> 2) + 0x44, pk);
			pk2[5] = *(uint32_t *)(s + 0x10);
			pk2[3] = pk2[1] = 0x3A000000;
			pk2[7] = *(uint32_t *)(s + 0x14);
			pk2[4] = pk[4];
			pk2[2] = pk[2];
			pk2[6] = pk[6];
			pk2[8] = pk[8];
			pk2[0] = 0x8000000;
			InsertPrimAutoDepth(RenderList() + 4 * (uint32_t)(*(int32_t *)(s + 8) >> 2) + 0x44, pk2);
			pk += 0x12;
			pk2 += 0x12;
		}
		Cursor() = (uint32_t)pk2;
	}

	struct TrailMemo { uint32_t tick; bool drawn; uint8_t sa[0x100], sb[0x100]; };
	static TrailMemo g_trail = { 0xFFFFFFFF };

	static uint32_t __cdecl TrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		TrailPoints(s);
		int32_t idx = (int32_t)n->c % 10;
		uint8_t *e = (uint8_t *)(TRAIL_RING + 0x14 * idx);
		*(uint32_t *)e = 1;
		*(uint32_t *)(e + 4) = *(const uint32_t *)(s + 0x20);
		*(uint32_t *)(e + 8) = *(const uint32_t *)(s + 0x24);
		*(uint32_t *)(e + 0xC) = *(const uint32_t *)(s + 0x28);
		*(uint32_t *)(e + 0x10) = *(const uint32_t *)(s + 0x2C);
		int32_t cnt = TrailGather((const uint8_t *)TRAIL_RING, idx, (uint8_t *)TRAIL_PTS_A, (uint8_t *)TRAIL_PTS_B);
		g_trail.tick = g_real_tick;
		g_trail.drawn = cnt > 1;
		if (cnt > 1)
		{
			TrailSplines(cnt, (const uint8_t *)TRAIL_PTS_A, (const uint8_t *)TRAIL_PTS_B, (uint8_t *)TRAIL_SPL_A, (uint8_t *)TRAIL_SPL_B);
			memcpy(g_trail.sa, (const void *)TRAIL_SPL_A, 0x100);
			memcpy(g_trail.sb, (const void *)TRAIL_SPL_B, 0x100);
			TrailDraw((const uint8_t *)TRAIL_SPL_A, (const uint8_t *)TRAIL_SPL_B, s);
		}
		FieldFree(0x40);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0xA ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Sword particles (tick 76, per sword k: owner mask 1 << k, sword index +0x1E from 0xCD0480),
	// both relative to the sword tip on the ground (GetEffectSpawnPosition bone 1, 0x1000, y = 0):
	// 0x5948A0 shards in pool A, 6 per tick for 2 ticks, tumbling prims 0xCC8E64 thrown out and up,
	//   shrinking from age 8 ((16 - age) * initial / 8), 17 ticks each;
	// 0x594CA0 puffs in pool B after a k-tick delay, 4 per tick for 2 ticks, flipbook 0xCC6C34
	//   (h+0x24 = 8), pushed by size/4, drifting out slowly.
	// Both end when none is left.
	// ------------------------------------------------------------------
	struct TipMemo { int16_t x, z; };
	static NodeMemo<TipMemo, 64> g_tip_memo;

	static void SwordTipFrame(uint8_t *s, const Node24 *n, int16_t *tip_x, int16_t *tip_z, bool use_tip)
	{
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		if (!use_tip) GetEffectSpawnPosition(Sword(n->s1E), 1, 0x1000, (int16_t *)s);
		else
		{
			*(int16_t *)(s + 0) = *tip_x;
			*(int16_t *)(s + 4) = *tip_z;
		}
		*tip_x = *(int16_t *)(s + 0);
		*tip_z = *(int16_t *)(s + 4);
		*(int32_t *)(s + 0x1C) = *(const int16_t *)(s + 0);
		*(int32_t *)(s + 0x20) = 0;
		*(int32_t *)(s + 0x24) = *(const int16_t *)(s + 4);
		ComposeAffineTransform(&Frame(), (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
	}

	static void ShardDrawOne(uint8_t *h, uint8_t *s, int16_t a0, int16_t a2, const int16_t *pos, int16_t size)
	{
		*(int16_t *)(s + 0) = a0;
		*(int16_t *)(s + 4) = a2;
		Mat4x3 *m = (Mat4x3 *)(s + 0x28);
		ComposeZYXRotationMatrix((const int16_t *)s, m);
		m->t[0] = pos[0];
		m->t[2] = pos[2];
		m->t[1] = pos[1];
		int32_t *sc = (int32_t *)(s + 0x48);
		sc[2] = size;
		sc[1] = size;
		sc[0] = size;
		Scale3DMatrix(m, sc);
		ComposeAffineTransform((const Mat4x3 *)(s + 8), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
	}

	static uint32_t __cdecl SwordShardTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x68);
		if (!Paused() && n->c <= 1)
		{
			for (int k = 0; k < 6; k++)
			{
				int i = FreeRecord(pool, 0x20);
				if (i < 0) break;
				RecA *p = (RecA *)(pool + 0x20 * i);
				p->mask = (uint32_t)(int32_t)n->s1C;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0x100 + 0x40);
				int32_t a = CrtRand() % 0x1000;
				int32_t sn = ComputeSin(a);
				*(int32_t *)(s + 0x48) = sn;
				int32_t cs = ComputeCos(a);
				*(int32_t *)(s + 0x50) = cs;
				int32_t r = CrtRand() % 80;
				p->x = (int16_t)(mul32(sn, r) >> 12);
				int32_t ry = CrtRand() % 40;
				p->y = (int16_t)-ry;
				p->z = (int16_t)(mul32(r, cs) >> 12);
				p->w0E = p->size;
				int32_t sp = (CrtRand() % 140 + 0x3C) >> 1;
				p->vx = (int16_t)(mul32(sn, sp) >> 12);
				int32_t up = CrtRand() % 140;
				p->vy = (int16_t)(-60 - up);
				p->vz = (int16_t)(mul32(sp, cs) >> 12);
				p->w18 = (int16_t)(CrtRand() % 0x1000);
				int32_t s1 = CrtRand() % 40 + 0x14;
				p->w1A = (int16_t)s1;
				if (s1 & 1) p->w1A = (int16_t)-s1;
				p->w1C = (int16_t)(CrtRand() % 0x800);
				int32_t s2 = CrtRand() % 40 + 0x14;
				p->w1E = (int16_t)s2;
				if (s2 & 1) p->w1E = (int16_t)-s2;
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		int32_t count = 0;
		*(uint32_t *)h = 0xCC8E64;
		TipMemo tip = { 0, 0 };
		SwordTipFrame(s, n, &tip.x, &tip.z, false);
		*(int16_t *)(s + 2) = 0;
		if (TipMemo *m = g_tip_memo.put(n)) *m = tip;
		int32_t mask = n->s1C;
		for (int i = 0; i < POOL_N; i++)
		{
			RecA *p = (RecA *)(pool + 0x20 * i);
			if (!(mask & p->mask)) continue;
			MemoA(i, n);
			ShardDrawOne(h, s, p->w18, p->w1C, &p->x, p->size);
			if (p->age >= 8) p->size = (int16_t)(shl32(mul32(0x10 - p->age, p->w0E), 9) >> 12);
			Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
			if (Paused()) continue;
			if (p->age >= 0x10)
			{
				p->mask = 0;
				continue;
			}
			p->age++;
			p->x = (int16_t)(p->x + p->vx);
			p->y = (int16_t)(p->y + p->vy);
			p->z = (int16_t)(p->z + p->vz);
			p->vx = (int16_t)(p->vx - (int16_t)(p->vx >> 3));
			p->vy = (int16_t)(p->vy - (int16_t)(p->vy >> 3));
			p->vz = (int16_t)(p->vz - (int16_t)(p->vz >> 3));
			p->w18 = (int16_t)(p->w18 + p->w1A);
			p->w1C = (int16_t)(p->w1C + p->w1E);
			count++;
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	static void SwordPuffSetup(uint8_t *h, uint8_t *s, const Node24 *n, TipMemo *tip, bool use_tip)
	{
		*(uint32_t *)h = 0xCC6C34;
		*(uint16_t *)(h + 0x24) = 8;
		SwordTipFrame(s, n, &tip->x, &tip->z, use_tip);
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static uint32_t __cdecl SwordPuffTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->e;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->e = (int16_t)(d - 1);
			return 0;
		}
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 1)
		{
			for (int k = 0; k < 4; k++)
			{
				int i = FreeRecord(pool, 0x18);
				if (i < 0) break;
				RecB *p = (RecB *)(pool + 0x18 * i);
				p->mask = (uint32_t)(int32_t)n->s1C;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0x400 + 0x400);
				int32_t a = CrtRand() % 0x1000;
				int32_t sn = ComputeSin(a);
				*(int32_t *)(s + 0x28) = sn;
				int32_t cs = ComputeCos(a);
				*(int32_t *)(s + 0x30) = cs;
				int32_t r = CrtRand() % 100;
				p->x = (int16_t)(mul32(r, sn) >> 12);
				int32_t ry = CrtRand() % 40;
				p->y = (int16_t)-ry;
				p->z = (int16_t)(mul32(cs, r) >> 12);
				int32_t sp = CrtRand() % 10 + 10;
				p->vx = (int16_t)(mul32(sp, sn) >> 12);
				p->vz = (int16_t)(mul32(cs, sp) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		TipMemo tip = { 0, 0 };
		SwordPuffSetup(h, s, n, &tip, false);
		if (TipMemo *m = g_tip_memo.put(n)) *m = tip;
		int32_t mask = n->s1C;
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *p = (RecB *)(pool + 0x18 * i);
			if (!(mask & p->mask)) continue;
			MemoB(i, n);
			LitSpriteMAC(h, s, &p->x, p->size, p->age, 2, false, 0);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				p->mask = 0;
				continue;
			}
			p->age++;
			p->x = (int16_t)(p->x + p->vx);
			p->z = (int16_t)(p->z + p->vz);
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Models. Block E (0x9C bytes, battle-entity head layout): +0x00 u16 flags (bit0 no anim loop,
	// bit2 hidden from the normal draw, bit3 not advanced), +0x07, +0x28 colour, +0x40 model matrix,
	// +0x60 BattleAnimHeader, +0x64 comFileData, +0x6C BattleAnimCmd, +0x7C, +0x84 model data.
	// Draws come before the advance (the tick shows the pose read on the previous tick).
	// ------------------------------------------------------------------
	// MAG_327_sub_595150: bind the model data (model file offsets table) to the block
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

	// au_re_Battle_ReadAnimation_0 0x5951C0
	static void SetAnim(uint8_t *E, int32_t id) { PreReadAnimation(E + 0x60, E + 0x6C, id); }

	// model matrix = identity rotation (MAG_327_sub_5904A0 + diagonal) at t
	static void ModelMatrixInit(uint8_t *M, int32_t tx, int32_t ty, int32_t tz)
	{
		ResetRotation(M);
		*(uint16_t *)(M + 0x10) = 0x1000;
		*(uint16_t *)(M + 8) = 0x1000;
		*(uint16_t *)(M + 0) = 0x1000;
		*(int32_t *)(M + 0x1C) = tz;
		*(int32_t *)(M + 0x18) = ty;
		*(int32_t *)(M + 0x14) = tx;
	}

	// ---- held-frame memo of every model block: the head, skeleton and reader command as drawn ----
	enum { DK_NORMAL4, DK_NORMAL3, DK_CLIP, DK_MATERIAL };
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct ModelMemo
	{
		uint32_t tick;
		const void *owner;    // task node that drew it
		bool midpoint;        // advanced after the draw: the next tick draws the pose just read
		int kind;
		int32_t threshold;    // materialise pass
		uint32_t skel_size;
		uint8_t head[0x9C];
		uint8_t skel[SKEL_MAX];
	};
	static ModelMemo g_model[11];

	static int ModelSlot(const uint8_t *E)
	{
		uint32_t a = (uint32_t)E;
		if (a >= 0x21FF4F0 && a < 0x21FF760 && (a - 0x21FF4F0) % 0x9C == 0) return (int)((a - 0x21FF4F0) / 0x9C);
		if (E == GIL1) return 4;
		if (E == GIL2) return 5;
		if (a >= 0x21FF800 && a < 0x21FFA70 && (a - 0x21FF800) % 0x9C == 0) return 6 + (int)((a - 0x21FF800) / 0x9C);
		if (E == GIL3) return 10;
		return -1;
	}

	static uint8_t *Skeleton(uint8_t *E)
	{
		uint8_t *com = *(uint8_t **)(E + 0x64); // BattleAnimHeader.comFileData
		return com ? *(uint8_t **)com : nullptr;
	}

	static void ModelMemoTake(uint8_t *E, const void *owner, int kind, int32_t threshold)
	{
		if (g_held) return;
		int k = ModelSlot(E);
		if (k < 0) return;
		ModelMemo &m = g_model[k];
		m.tick = 0xFFFFFFFF;
		uint8_t *sk = Skeleton(E);
		if (!sk) return;
		uint32_t size = 16 + 48 * (uint32_t)sk[0];
		if (size > SKEL_MAX) return;
		m.skel_size = size;
		memcpy(m.skel, sk, size);
		memcpy(m.head, E, sizeof(m.head));
		m.owner = owner;
		m.kind = kind;
		m.threshold = threshold;
		m.midpoint = false;
		m.tick = g_real_tick;
	}

	// GF_327Gilgamesh_AdvanceModelAnimLoop 0x595E00
	static void AdvanceModel(uint8_t *E)
	{
		if (ReadAnimation(E + 0x60, E + 0x6C) == 1 && !(E[0] & 1))
			PreReadAnimation(E + 0x60, E + 0x6C, E[0x6C]);
		int k = ModelSlot(E);
		if (k >= 0 && g_model[k].tick == g_real_tick) g_model[k].midpoint = true;
	}

	// MAG_327_sub_595E40 (shift 4) / MAG_327_sub_5968D0 (shift 3)
	static void DrawModel(uint8_t *E, const Mat4x3 *frame, int32_t shift)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		ComposeAffineTransform(frame, (const Mat4x3 *)(E + 0x40), (Mat4x3 *)h);
		ComputeBonesWorldMatrices(E + 0x60, h);
		*(uint32_t *)(h + 0x30) = var<uint32_t>(0x1D969A8);
		*(uint32_t *)(h + 0x24) = ModelScratch();
		*(uint16_t *)(h + 0x34) = 0;
		*(uint16_t *)(h + 0x36) = 0;
		*(uint32_t *)(h + 0x3C) = *(uint32_t *)(E + 0x28);
		*(uint32_t *)(h + 0x48) = 0;
		*(uint16_t *)(h + 0x44) = 0;
		*(uint16_t *)(h + 0x38) = 0x140;
		*(uint16_t *)(h + 0x3A) = 0xD8;
		*(uint32_t *)(h + 0x40) = *(uint32_t *)(E + 0x7C);
		Cursor() = RenderGeometry(*(void **)(E + 0x64), h + 0x20, OT(), shift, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
		FieldFree(0x4C);
	}

	// 0x594FD0 clip header: the ground plane y = 0 (normal (0, -0x1000, 0)), the vertex scratch
	static void ClipSetup(uint8_t *h)
	{
		int16_t *p = (int16_t *)(h + 0x58), *nrm = (int16_t *)(h + 0xB8);
		p[0] = 0;
		p[1] = 0;
		p[2] = 0;
		nrm[0] = 0;
		nrm[1] = (int16_t)0xF000;
		nrm[2] = 0;
		PlaneDistance(p, nrm);
		*(uint32_t *)(h + 4) = ModelScratch();
	}

	// MAG_327_sub_5951E0: the model clipped by the header's plane (root = its own matrix; the frame
	// pointer goes to 0x21FF3C0 for the renderer 0x595250)
	static void ClipDraw(uint8_t *h, uint8_t *E, const Mat4x3 *frame)
	{
		ComputeBonesWorldMatrices(E + 0x60, E + 0x40);
		*(uint32_t *)(h + 0x10) = *(uint32_t *)(E + 0x28);
		*(uint32_t *)(h + 0x14) = *(uint32_t *)(E + 0x7C);
		var<uint32_t>(0x21FF3C0) = (uint32_t)frame;
		*(uint16_t *)(h + 0x18) = 0;
		Cursor() = ClipRender(*(void **)(E + 0x64), h, OT(), 2, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
	}

	// MAG_327_sub_596010: materialising draw, faces whose threshold (table h+0x2C, built by the
	// renderer on the pass with h+0x44 = 0) is <= h+0x44
	static void MaterialDraw(uint8_t *h, uint8_t *E)
	{
		ComputeBonesWorldMatrices(E + 0x60, E + 0x40);
		*(uint32_t *)(h + 4) = ModelScratch();
		*(uint16_t *)(h + 0x14) = 0;
		*(uint16_t *)(h + 0x16) = 0;
		*(uint32_t *)(h + 0x1C) = *(uint32_t *)(E + 0x28);
		*(uint32_t *)(h + 0x20) = *(uint32_t *)(E + 0x7C);
		*(uint32_t *)(h + 0x10) = var<uint32_t>(0x1D969A8);
		h[0x2A] = E[7];
		h[0x29] = E[7];
		h[0x28] = E[7];
		*(uint16_t *)(h + 0x18) = 0x140;
		*(uint16_t *)(h + 0x1A) = 0xD8;
		Cursor() = MaterialRender(*(void **)(E + 0x64), h, OT(), 2, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
	}

	// 0x594FD0 (Q1, tick 76), 105 ticks: binds the four swords (variant's models, anim = variant) and
	// draws them clipped at the ground every tick (sets E0 bit2: the normal draw skips them); E0
	// bit3 (no advance) from 10 to 44
	static uint32_t __cdecl SwordClipTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			for (int i = 0; i < 4; i++)
			{
				uint8_t *E = Sword(i);
				ModelMatrixInit(E + 0x40, 0, 0, 0);
				ModelInit(E, (uint8_t *)(0x2200D20 + 0x34 * i), *(uint8_t **)(0xCD03E8 + 4 * (i + 4 * Variant())), 0x10);
				SetAnim(E, n->e);
			}
		}
		var<uint8_t>(0x21FF4F0) |= 4;
		uint8_t *h = (uint8_t *)FieldAlloc(0x1A8);
		ClipSetup(h);
		for (int i = 0; i < 4; i++)
		{
			ClipDraw(h, Sword(i), &Frame());
			ModelMemoTake(Sword(i), n, DK_CLIP, 0);
		}
		FieldFree(0x1A8);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 10)
		{
			int32_t e = c - 10;
			if (e < 15)
			{
				if (e == 0) var<uint8_t>(0x21FF4F0) |= 8;
			}
			else if ((e -= 15) >= 20)
			{
				e -= 20;
				if (e < 60 && e == 0) var<uint16_t>(0x21FF4F0) &= 0xFFF7;
			}
		}
		n->c = (int16_t)(c + 1);
		return n->c >= 0x69 ? TASK_END : 0;
	}

	// 0x595D50 (Q2, tick 76, +0x0E = 105 ticks): the four swords drawn normally unless E0 bit2 (E1-E3
	// take E0's matrix and colour), advanced unless E0 bit3
	static uint32_t __cdecl SwordDrawTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (!(var<uint8_t>(0x21FF4F0) & 4))
		{
			for (int i = 0; i < 4; i++)
			{
				uint8_t *E = Sword(i);
				if (i != 0)
				{
					memcpy(E + 0x40, (const void *)0x21FF530, sizeof(Mat4x3));
					*(uint32_t *)(E + 0x28) = var<uint32_t>(0x21FF518);
				}
				DrawModel(E, &Frame(), 4);
				ModelMemoTake(E, n, DK_NORMAL4, 0);
			}
		}
		if (Paused()) return 0;
		if (!(var<uint8_t>(0x21FF4F0) & 8))
			for (int i = 0; i < 4; i++) AdvanceModel(Sword(i));
		n->c++;
		return n->c >= n->e ? TASK_END : 0;
	}

	// 0x595EE0 (Q1, tick 76), 60 ticks: binds Gilgamesh E4 (anim 0, hidden, frozen from 10), draws
	// it materialising from 25 to 44 (threshold c - 25, table mb+0x22000 reset at 25), shows it at 45
	static uint32_t __cdecl Gil1Task(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			ModelMatrixInit(GIL1 + 0x40, 0, 0, 0);
			ModelInit(GIL1, (uint8_t *)0x21FF4B8, (uint8_t *)MB(), 0x20);
		}
		int32_t c = n->c;
		if (c < 10)
		{
			if (c == 0)
			{
				GIL1[0] |= 4;
				SetAnim(GIL1, 0);
			}
		}
		else if (c - 10 < 15)
		{
			if (c == 10) GIL1[0] |= 8;
		}
		else if (c - 25 < 20)
		{
			uint32_t buf = MB() + 0x22000;
			if (c == 25)
				for (int k = 0; k < 0x200; k++) ((uint32_t *)buf)[k] = 0x40004000;
			uint8_t *h = (uint8_t *)FieldAlloc(0xB0);
			*(uint32_t *)(h + 0x2C) = buf;
			*(int32_t *)(h + 0x44) = c - 25;
			MaterialDraw(h, GIL1);
			FieldFree(0xB0);
			ModelMemoTake(GIL1, n, DK_MATERIAL, c - 25);
		}
		else if (c - 45 < 60)
		{
			if (c == 45) *(uint16_t *)GIL1 &= 0xFFF3;
		}
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x3C ? TASK_END : 0;
	}

	// 0x5966B0 (Q2, tick 76), 105 ticks: E4 drawn unless bit2, advanced unless bit3
	static uint32_t __cdecl Gil1DrawTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (!(GIL1[0] & 4))
		{
			DrawModel(GIL1, &Frame(), 4);
			ModelMemoTake(GIL1, n, DK_NORMAL4, 0);
		}
		if (Paused()) return 0;
		if (!(GIL1[0] & 8)) AdvanceModel(GIL1);
		n->c++;
		return n->c >= 0x69 ? TASK_END : 0;
	}

	// 0x596710 (Q1, tick 181), 42 ticks: binds Gilgamesh on the horse E5 (t.y = -1000, anim 0) and
	// its four arms' swords B0-B3 (anim j)
	static uint32_t __cdecl Gil2Task(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			ModelMatrixInit(GIL2 + 0x40, 0, -1000, 0);
			ModelInit(GIL2, (uint8_t *)0x21FF2B0, (uint8_t *)(MB() + 0x22000), 0x80);
			SetAnim(GIL2, 0);
			uint8_t *buf = (uint8_t *)(MB() + 0x2379C);
			for (int j = 0; j < 4; j++, buf += 0x1860)
			{
				ModelInit(Arm(j), (uint8_t *)(0x21FFC78 + 0x34 * j), buf, 0x40);
				SetAnim(Arm(j), j);
			}
		}
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x2A ? TASK_END : 0;
	}

	// 0x596800 (Q2, tick 181), 42 ticks: E5 (shift 3) and B0-B3 (E5's flags, matrix pushed 0x50 in z,
	// colour), advanced unless E5 bit3
	static uint32_t __cdecl Gil2DrawTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (!(GIL2[0] & 4))
		{
			DrawModel(GIL2, &Frame(), 3);
			ModelMemoTake(GIL2, n, DK_NORMAL3, 0);
			for (int j = 0; j < 4; j++)
			{
				uint8_t *A = Arm(j);
				*(uint16_t *)A = *(uint16_t *)GIL2;
				memcpy(A + 0x40, (const void *)0x2200F40, sizeof(Mat4x3));
				*(int32_t *)(A + 0x5C) += 0x50;
				*(uint32_t *)(A + 0x28) = var<uint32_t>(0x2200F28);
				DrawModel(A, &Frame(), 4);
				ModelMemoTake(A, n, DK_NORMAL4, 0);
			}
		}
		if (Paused()) return 0;
		if (!(GIL2[0] & 8))
		{
			AdvanceModel(GIL2);
			for (int j = 0; j < 4; j++) AdvanceModel(Arm(j));
		}
		n->c++;
		return n->c >= 0x2A ? TASK_END : 0;
	}

	// 0x596970 (Q1, tick 223), 50 ticks: binds the final pose E6 (anim 0) and rebinds the four swords
	// (anim variant + 4)
	static uint32_t __cdecl Gil3Task(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			ModelMatrixInit(GIL3 + 0x40, 0, 0, 0);
			ModelInit(GIL3, (uint8_t *)0x2200EC8, (uint8_t *)MB(), 0xF);
			SetAnim(GIL3, 0);
			for (int i = 0; i < 4; i++)
			{
				ModelInit(Sword(i), (uint8_t *)(0x2200D20 + 0x34 * i), *(uint8_t **)(0xCD03E8 + 4 * (i + 4 * Variant())), 0x10);
				SetAnim(Sword(i), Variant() + 4);
			}
		}
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x32 ? TASK_END : 0;
	}

	// 0x596A60 (Q2, tick 223), 50 ticks: E6 and the swords (E6's flags, matrix, colour), advanced
	// unless E5 (sic: 0x2200F00, not E6) bit3
	static uint32_t __cdecl Gil3DrawTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (!(GIL3[0] & 4))
		{
			DrawModel(GIL3, &Frame(), 4);
			ModelMemoTake(GIL3, n, DK_NORMAL4, 0);
			for (int i = 0; i < 4; i++)
			{
				uint8_t *E = Sword(i);
				*(uint16_t *)E = *(uint16_t *)GIL3;
				memcpy(E + 0x40, (const void *)0x2201000, sizeof(Mat4x3));
				*(uint32_t *)(E + 0x28) = var<uint32_t>(0x2200FE8);
				DrawModel(E, &Frame(), 4);
				ModelMemoTake(E, n, DK_NORMAL4, 0);
			}
		}
		if (Paused()) return 0;
		if (!(GIL2[0] & 8))
		{
			AdvanceModel(GIL3);
			for (int i = 0; i < 4; i++) AdvanceModel(Sword(i));
		}
		n->c++;
		return n->c >= 0x32 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Everything is drawn half way between the state drawn on the last real
	// tick (memos) and the state the next tick will draw; flipbook frames, spawns, the flare's
	// flicker and the materialise threshold keep their 15 Hz steps. The frame matrix 0x21FF488 is
	// rebuilt from the held-frame camera while the timeline (its owner) is alive and put back after
	// (the original renderers read it by address), like the screen-sprite matrix 0x2201060 and the
	// clip renderer's frame pointer 0x21FF3C0. Model and entity skeletons touched by a draw are put
	// back; model renderers get a private vertex scratch.
	// ------------------------------------------------------------------
	static const Node24 *HeldMemo(const Node24 *n, bool *mv)
	{
		const Node24 *m = g_node_memo.get(n);
		if (m) *mv = NodeMoved(m, n);
		return m;
	}

	static void FadeTileHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		int32_t v = FadeTileLevel(m->c);
		if (mv) v = lerp_i(v, FadeTileLevel(m->c + 1), num, den);
		Tile(v, v, v, 0);
	}

	static void ScreenPrimHeld(const Node24 *n, uint32_t orig, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		int16_t s = (mv && m->c < 6) ? Lerp16(m->s1C, n->s1C, num, den) : m->s1C;
		ScreenPrimDraw(m, ScreenPrimModel(m, orig), s);
	}

	static int32_t SliceFadeW(int32_t c) { return 0x1000 - ComputeSin(shl32(c - 0x18, 10) / 16); }

	static uint8_t g_ent_head[0x30];
	static uint8_t g_skel_save[4][SKEL_MAX];

	static uint8_t *EntitySkeleton(uint8_t *anim_header, uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(anim_header + 4);
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= SKEL_MAX ? sk : nullptr;
	}

	static void SliceHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		int32_t slot = m->e;
		uint8_t *ent = Entity((uint32_t)slot);
		// the cut half's matrix half way (its cut task's memo), else the one drawn
		Mat4x3 moved = *(const Mat4x3 *)(0x2200B94 + 60 * slot);
		for (TaskNode *t = Q1().head; t; t = t->next)
		{
			const Node24 *cut = (const Node24 *)t;
			if ((uint32_t)t->func != ORIG_Zan_CutTask || cut->e != m->e) continue;
			bool cmv = false;
			const Node24 *cm = HeldMemo(cut, &cmv);
			if (cm && cmv && cm->c < 0x28)
			{
				int16_t A[4];
				int16_t a0 = Lerp16(CutAngle0(cm->c, cm->a18), CutAngle0(cm->c + 1, cm->a18), num, den);
				int16_t a1 = Lerp16(CutAngle1(cm->c, cm->a18), CutAngle1(cm->c + 1, cm->a18), num, den);
				CutMatrix(cm, a0, a1, *(const int16_t *)(0x2200B80 + 60 * slot), lerp_i(cm->s20, cut->s20, num, den), &moved, A);
			}
			break;
		}
		int32_t w = -1;
		if (m->c >= 0x18)
		{
			w = SliceFadeW(m->c);
			if (mv && m->c + 1 < 0x28) w = lerp_i(w, SliceFadeW(m->c + 1), num, den);
		}
		// the draw recolours the target and rebuilds its local bone matrices: put both back
		uint32_t size = 0;
		uint8_t *sk = EntitySkeleton(ent + 0x60, &size);
		if (!sk) return;
		memcpy(g_ent_head, ent, sizeof(g_ent_head));
		memcpy(g_skel_save[0], sk, size);
		uint8_t *h = (uint8_t *)FieldAlloc(0x1D8);
		SliceDraw(h, m, ent, &moved, m->c, w);
		FieldFree(0x1D8);
		memcpy(sk, g_skel_save[0], size);
		memcpy(ent, g_ent_head, sizeof(g_ent_head));
	}

	static void PartyClipHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		int32_t z = mv ? lerp_i(m->s20, n->s20, num, den) : m->s20;
		uint8_t *sk[4];
		uint32_t size[4];
		for (int i = 0; i < 4; i++)
		{
			sk[i] = EntitySkeleton((uint8_t *)(0x1D98991 + 0x2C * i + 0x13), &size[i]);
			if (sk[i]) memcpy(g_skel_save[i], sk[i], size[i]);
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0x1D8);
		PartyClipDraw(h, z);
		FieldFree(0x1D8);
		for (int i = 0; i < 4; i++)
			if (sk[i]) memcpy(sk[i], g_skel_save[i], size[i]);
	}

	static void MasaPrimHeld(const Node24 *n, bool b, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		bool fade = false;
		int32_t f = FadeLerp(b ? MasaFadeB : MasaFadeA, m->c, mv, num, den, &fade);
		MasaPrimDraw(m, b, mv ? Lerp16(m->s1C, n->s1C, num, den) : m->s1C, fade, f);
	}

	static void ExcaPillarHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		bool fade = false;
		int32_t f = FadeLerp(MasaFadeB, m->c, mv, num, den, &fade);
		int16_t s1C = mv ? Lerp16(m->s1C, n->s1C, num, den) : m->s1C;
		int16_t s20 = mv ? Lerp16(m->s20, n->s20, num, den) : m->s20;
		ExcaPillarDraw(m, m->a18, s1C, s20, fade, f);
	}

	static void RingHeld(const Node24 *n)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (m) RingDraw(m);
	}

	static void RingSpinHeld(const Node24 *n, bool b, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		RingSpinDraw(m, b, mv ? lerp_angle(m->a18, n->a18, num, den) : m->a18);
	}

	static void FlareHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		int32_t spread = m->s20, depth = FlareDepth(m->c);
		if (mv)
		{
			spread = lerp_i(m->s20, n->s20, num, den);
			depth = lerp_i(depth, FlareDepth(m->c + 1), num, den);
		}
		FlareDraw(m, spread, depth, FlareColour(m->c));
	}

	static void RiserHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		bool fade = false;
		int32_t f = FadeLerp(RiserFade, m->c, mv, num, den, &fade);
		RiserDraw(m, mv ? Lerp16(m->p12, n->p12, num, den) : m->p12, mv ? Lerp16(m->s1C, n->s1C, num, den) : m->s1C, fade, f);
	}

	static void RisingRingHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		bool fade = false;
		int32_t f = FadeLerp(RisingRingFade, m->c, mv, num, den, &fade);
		RisingRingDraw(m, mv ? lerp_angle(m->a18, n->a18, num, den) : m->a18, mv ? Lerp16(m->s20, n->s20, num, den) : m->s20, fade, f);
	}

	static void SpinnerHeld(const Node24 *n, int num, int den)
	{
		bool mv = false;
		const Node24 *m = HeldMemo(n, &mv);
		if (!m) return;
		bool fade = false;
		int32_t f = FadeLerp(SpinnerFade, m->c, mv, num, den, &fade);
		SpinnerDraw(m, mv ? lerp_angle(m->a18, n->a18, num, den) : m->a18, mv ? Lerp16(m->s1C, n->s1C, num, den) : m->s1C, fade, f);
	}

	// ---- pool records: the memo record half way to the record now (when it moved on), frame kept ----
	static RecA HeldRecA(const RecMemoA &mm, int i, int num, int den)
	{
		RecA t = mm.r;
		const RecA *cur = RA(i);
		if (AdvancedA(mm, cur))
		{
			t.x = Lerp16(mm.r.x, cur->x, num, den);
			t.y = Lerp16(mm.r.y, cur->y, num, den);
			t.z = Lerp16(mm.r.z, cur->z, num, den);
			t.size = Lerp16(mm.r.size, cur->size, num, den);
			t.w18 = lerp_angle(mm.r.w18, cur->w18, num, den);
			t.w1C = lerp_angle(mm.r.w1C, cur->w1C, num, den);
		}
		return t;
	}

	static RecB HeldRecB(const RecMemoB &mm, int i, int num, int den)
	{
		RecB t = mm.r;
		const RecB *cur = RB(i);
		if (AdvancedB(mm, cur))
		{
			t.x = Lerp16(mm.r.x, cur->x, num, den);
			t.y = Lerp16(mm.r.y, cur->y, num, den);
			t.z = Lerp16(mm.r.z, cur->z, num, den);
			t.size = Lerp16(mm.r.size, cur->size, num, den);
		}
		return t;
	}

	enum { LIT_MAC3, LIT_MAC3_ANGLE, LIT_MAC2, LIT_PLAIN };

	static void LitPoolHeldA(const void *owner, uint8_t *h, uint8_t *s, int kind, int num, int den)
	{
		for (int i = 0; i < POOL_N; i++)
		{
			const RecMemoA &mm = g_memo_a[i];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			RecA t = HeldRecA(mm, i, num, den);
			if (kind == LIT_PLAIN) LitSprite(h, &t.x, t.size, t.age);
			else LitSpriteMAC(h, s, &t.x, t.size, t.age, kind == LIT_MAC2 ? 2 : 3, kind == LIT_MAC3_ANGLE, t.w1C);
		}
	}

	static void LitPoolHeldB(const void *owner, uint8_t *h, uint8_t *s, int kind, int num, int den)
	{
		for (int i = 0; i < POOL_N; i++)
		{
			const RecMemoB &mm = g_memo_b[i];
			if (mm.tick != g_real_tick || mm.owner != owner) continue;
			RecB t = HeldRecB(mm, i, num, den);
			if (kind == LIT_PLAIN) LitSprite(h, &t.x, t.size, t.age);
			else LitSpriteMAC(h, s, &t.x, t.size, t.age, kind == LIT_MAC2 ? 2 : 3, false, 0);
		}
	}

	static void MasaSparkHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		MasaSparkSetup(h, s, n);
		LitPoolHeldA(n, h, s, LIT_MAC3_ANGLE, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static void MasaDustHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		MasaDustSetup(h, s, n);
		LitPoolHeldB(n, h, s, LIT_MAC3, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static void ExcaBurstHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		ExcaBurstSetup(h, s, n);
		LitPoolHeldB(n, h, s, LIT_MAC3, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static void ExcaSmokeHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		ExcaSmokeSetup(h, s, n);
		LitPoolHeldB(n, h, s, LIT_PLAIN, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static void ExcaStreakHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x88);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		ExcaStreakSetup(h, s, n);
		for (int i = 0; i < POOL_N; i++)
		{
			const RecMemoA &mm = g_memo_a[i];
			if (mm.tick != g_real_tick || mm.owner != n) continue;
			RecA t = HeldRecA(mm, i, num, den);
			ExcaStreakOne(h, s, &mm.r.w18, &t.x, t.size);
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
		}
		FieldFree(0xB4);
		FieldFree(0x88);
	}

	static void PuffHeld(const Node24 *n, bool a, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		PuffSetup(h, s);
		if (a) LitPoolHeldA(n, h, s, LIT_PLAIN, num, den);
		else LitPoolHeldB(n, h, s, LIT_PLAIN, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static void DotHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x58);
		GteSetRotMatrix(&Frame());
		GteSetTransVector(&Frame());
		uint32_t pk = Cursor();
		*(uint32_t *)(s + 0x48) = OT();
		*(uint32_t *)(s + 0x4C) = 4;
		for (int i = 0; i < POOL_N; i++)
		{
			const RecMemoA &mm = g_memo_a[i];
			if (mm.tick != g_real_tick || mm.owner != n) continue;
			RecA t = HeldRecA(mm, i, num, den);
			GteLoadV0(&t.x);
			GteRTPS();
			*(uint32_t *)pk = 0x2000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)(0xCD0428 + 4 * mm.r.age);
			pk = DotEmit(s, pk);
		}
		Cursor() = pk;
		FieldFree(0x58);
	}

	static void SwordShardHeld(const Node24 *n, int num, int den)
	{
		const TipMemo *tm = g_tip_memo.get(n);
		if (!tm) return;
		uint8_t *s = (uint8_t *)FieldAlloc(0x68);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint32_t *)h = 0xCC8E64;
		TipMemo tip = *tm;
		SwordTipFrame(s, n, &tip.x, &tip.z, true);
		*(int16_t *)(s + 2) = 0;
		for (int i = 0; i < POOL_N; i++)
		{
			const RecMemoA &mm = g_memo_a[i];
			if (mm.tick != g_real_tick || mm.owner != n) continue;
			RecA t = HeldRecA(mm, i, num, den);
			ShardDrawOne(h, s, t.w18, t.w1C, &t.x, t.size);
			Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
		}
		FieldFree(0x58);
		FieldFree(0x68);
	}

	static void SwordPuffHeld(const Node24 *n, int num, int den)
	{
		const TipMemo *tm = g_tip_memo.get(n);
		if (!tm) return;
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TipMemo tip = *tm;
		SwordPuffSetup(h, s, n, &tip, true);
		LitPoolHeldB(n, h, s, LIT_MAC2, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	// ---- trail: the next tick's history is predicted exactly (the sword's base and tip now go into
	// slot c % 10), both splined on private buffers, the samples drawn half way ----
	static uint8_t g_trail_ring[0xC8], g_trail_pa[0x50], g_trail_pb[0x50];
	static uint8_t g_trail_na[0x100], g_trail_nb[0x100], g_trail_ma[0x100], g_trail_mb[0x100];

	static void TrailHeld(const Node24 *n, int num, int den)
	{
		if (g_trail.tick != g_real_tick || !g_trail.drawn) return;
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		const uint8_t *sa = g_trail.sa, *sb = g_trail.sb;
		if (!g_tick_paused)
		{
			memcpy(g_trail_ring, (const void *)TRAIL_RING, sizeof(g_trail_ring));
			TrailPoints(s);
			int32_t idx = (int32_t)n->c % 10;
			uint8_t *e = g_trail_ring + 0x14 * idx;
			*(uint32_t *)e = 1;
			memcpy(e + 4, s + 0x20, 8);
			memcpy(e + 0xC, s + 0x28, 8);
			int32_t cnt = TrailGather(g_trail_ring, idx, g_trail_pa, g_trail_pb);
			if (cnt > 1)
			{
				TrailSplines(cnt, g_trail_pa, g_trail_pb, g_trail_na, g_trail_nb);
				const int16_t *a0 = (const int16_t *)g_trail.sa, *a1 = (const int16_t *)g_trail_na;
				const int16_t *b0 = (const int16_t *)g_trail.sb, *b1 = (const int16_t *)g_trail_nb;
				int16_t *ma = (int16_t *)g_trail_ma, *mb = (int16_t *)g_trail_mb;
				for (int k = 0; k < 0x80; k++)
				{
					ma[k] = Lerp16(a0[k], a1[k], num, den);
					mb[k] = Lerp16(b0[k], b1[k], num, den);
				}
				sa = g_trail_ma;
				sb = g_trail_mb;
			}
		}
		TrailDraw(sa, sb, s);
		FieldFree(0x40);
	}

	// ---- models: the drawn head, skeleton and command put back, the midpoint pose, the same draw ----
	static uint8_t g_head_cur[0x9C];
	static uint8_t g_skel_cur[SKEL_MAX];

	static void ModelHeld(uint8_t *E, const void *owner, int num, int den)
	{
		int k = ModelSlot(E);
		if (k < 0) return;
		ModelMemo &m = g_model[k];
		if (m.tick != g_real_tick || m.owner != owner) return;
		uint8_t *sk = Skeleton(E);
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		memcpy(g_head_cur, E, sizeof(g_head_cur));
		memcpy(g_skel_cur, sk, m.skel_size);
		memcpy(E, m.head, sizeof(m.head));
		memcpy(sk, m.skel, m.skel_size);
		if (m.midpoint && !g_tick_paused) pose_midpoint(E + 0x60, E + 0x6C, num, den);
		switch (m.kind)
		{
		case DK_NORMAL4: DrawModel(E, &Frame(), 4); break;
		case DK_NORMAL3: DrawModel(E, &Frame(), 3); break;
		case DK_CLIP:
		{
			uint8_t *h = (uint8_t *)FieldAlloc(0x1A8);
			ClipSetup(h);
			ClipDraw(h, E, &Frame());
			FieldFree(0x1A8);
			break;
		}
		case DK_MATERIAL:
			if (m.threshold > 0)
			{
				uint8_t *h = (uint8_t *)FieldAlloc(0xB0);
				*(uint32_t *)(h + 0x2C) = MB() + 0x22000;
				*(int32_t *)(h + 0x44) = m.threshold;
				MaterialDraw(h, E);
				FieldFree(0xB0);
			}
			break;
		}
		memcpy(E, g_head_cur, sizeof(g_head_cur));
		memcpy(sk, g_skel_cur, m.skel_size);
	}

	static void HeldTask(const Node24 *n, uint32_t f, int num, int den)
	{
		switch (f)
		{
		case ORIG_FeedbackTask: RequestScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F); break;
		case ORIG_Feedback2Task:
		{
			int32_t c = g_tick_paused ? n->c : n->c - 1; // the counter the last tick issued with
			if (c > 0) RequestScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F);
			break;
		}
		case ORIG_FadeTileTask: FadeTileHeld(n, num, den); break;
		case ORIG_Zan_ScreenPrimTask:
		case ORIG_Masa_ScreenPrimTask:
		case ORIG_Exca_ScreenPrimTask: ScreenPrimHeld(n, f, num, den); break;
		case ORIG_Zan_SliceTask: SliceHeld(n, num, den); break;
		case ORIG_Zan_PartyClipTask: PartyClipHeld(n, num, den); break;
		case ORIG_Masa_PrimATask: MasaPrimHeld(n, false, num, den); break;
		case ORIG_Masa_PrimBTask: MasaPrimHeld(n, true, num, den); break;
		case ORIG_Masa_SparkTask: MasaSparkHeld(n, num, den); break;
		case ORIG_Masa_DustTask: MasaDustHeld(n, num, den); break;
		case ORIG_Exca_BurstTask: ExcaBurstHeld(n, num, den); break;
		case ORIG_Exca_SmokeTask: ExcaSmokeHeld(n, num, den); break;
		case ORIG_Exca_StreakTask: ExcaStreakHeld(n, num, den); break;
		case ORIG_Exca_PillarTask: ExcaPillarHeld(n, num, den); break;
		case ORIG_RingTask: RingHeld(n); break;
		case ORIG_RingSpinTask: RingSpinHeld(n, false, num, den); break;
		case ORIG_RingSpin2Task: RingSpinHeld(n, true, num, den); break;
		case ORIG_FlareTask: FlareHeld(n, num, den); break;
		case ORIG_RiserTask: RiserHeld(n, num, den); break;
		case ORIG_PuffTask: PuffHeld(n, false, num, den); break;
		case ORIG_SparkTask: PuffHeld(n, true, num, den); break;
		case ORIG_RisingRingTask: RisingRingHeld(n, num, den); break;
		case ORIG_DotTask: DotHeld(n, num, den); break;
		case ORIG_SpinnerTask: SpinnerHeld(n, num, den); break;
		case ORIG_TrailTask: TrailHeld(n, num, den); break;
		case ORIG_SwordShardTask: SwordShardHeld(n, num, den); break;
		case ORIG_SwordPuffTask: SwordPuffHeld(n, num, den); break;
		case ORIG_SwordClipTask:
		case ORIG_SwordDrawTask:
			for (int i = 0; i < 4; i++) ModelHeld(Sword(i), n, num, den);
			break;
		case ORIG_Gil1Task:
		case ORIG_Gil1DrawTask: ModelHeld(GIL1, n, num, den); break;
		case ORIG_Gil2DrawTask:
			ModelHeld(GIL2, n, num, den);
			for (int j = 0; j < 4; j++) ModelHeld(Arm(j), n, num, den);
			break;
		case ORIG_Gil3DrawTask:
			ModelHeld(GIL3, n, num, den);
			for (int i = 0; i < 4; i++) ModelHeld(Sword(i), n, num, den);
			break;
		default: break;
		}
	}

	static uint8_t g_held_packets[0x100000];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the master's queue order (Q1 then Q2); packets go to a private buffer (the module draws
	// through the engine cursor battle_texture_data_ptr_1D8E054, redirected and put back)
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = Cursor();
		Cursor() = (uint32_t)g_held_packets;
		g_held = true;
		Mat4x3 frame = Frame();
		uint8_t screen_mat[0x20];
		memcpy(screen_mat, (const void *)0x2201060, sizeof(screen_mat));
		uint32_t frame_ptr = var<uint32_t>(0x21FF3C0);
		bool timeline = false;
		for (TaskNode *t = Q1().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TimelineTask) timeline = true;
		if (timeline) ComposeAffineTransform(&Camera(), &RootMatrix(), &Frame());
		for (TaskNode *t = Q1().head; t; t = t->next) HeldTask((const Node24 *)t, (uint32_t)t->func, num, den);
		for (TaskNode *t = Q2().head; t; t = t->next) HeldTask((const Node24 *)t, (uint32_t)t->func, num, den);
		var<uint32_t>(0x21FF3C0) = frame_ptr;
		memcpy((void *)0x2201060, screen_mat, sizeof(screen_mat));
		Frame() = frame;
		g_held = false;
		Cursor() = cursor;
	}
}

	// One set of ports serves the four ids 327-330 (the tasks are shared, the variant is data):
	// the ports are registered under 327, the held-frame functions under each id.
	void register_mag327_gilgamesh()
	{
		using namespace g327;
		register_port(ORIG_SequenceTick, (void *)SequenceTick, "G327 SequenceTick", 327);
		register_port(ORIG_TimelineTask, (void *)TimelineTask, "G327 TimelineTask", 327);
		register_port(ORIG_FeedbackTask, (void *)FeedbackTask, "G327 FeedbackTask", 327);
		register_port(ORIG_Feedback2Task, (void *)Feedback2Task, "G327 Feedback2Task", 327);
		register_port(ORIG_FlashInTask, (void *)FlashInTask, "G327 FlashInTask", 327);
		register_port(ORIG_FlashOutTask, (void *)FlashOutTask, "G327 FlashOutTask", 327);
		register_port(ORIG_FadeTileTask, (void *)FadeTileTask, "G327 FadeTileTask", 327, true);
		register_port(ORIG_RingTask, (void *)RingTask, "G327 RingTask", 327, true);
		register_port(ORIG_RingSpinTask, (void *)RingSpinTask, "G327 RingSpinTask", 327, true);
		register_port(ORIG_RingSpin2Task, (void *)RingSpin2Task, "G327 RingSpin2Task", 327, true);
		register_port(ORIG_FlareTask, (void *)FlareTask, "G327 FlareTask", 327, true);
		register_port(ORIG_RiserTask, (void *)RiserTask, "G327 RiserTask", 327, true);
		register_port(ORIG_PuffTask, (void *)PuffTask, "G327 PuffTask", 327, true);
		register_port(ORIG_SparkTask, (void *)SparkTask, "G327 SparkTask", 327, true);
		register_port(ORIG_RisingRingSpawnerTask, (void *)RisingRingSpawnerTask, "G327 RisingRingSpawnerTask", 327);
		register_port(ORIG_RisingRingTask, (void *)RisingRingTask, "G327 RisingRingTask", 327, true);
		register_port(ORIG_DotTask, (void *)DotTask, "G327 DotTask", 327, true);
		register_port(ORIG_SpinnerTask, (void *)SpinnerTask, "G327 SpinnerTask", 327, true);
		register_port(ORIG_TrailTask, (void *)TrailTask, "G327 TrailTask", 327, true);
		register_port(ORIG_SwordShardTask, (void *)SwordShardTask, "G327 SwordShardTask", 327, true);
		register_port(ORIG_SwordPuffTask, (void *)SwordPuffTask, "G327 SwordPuffTask", 327, true);
		register_port(ORIG_SwordClipTask, (void *)SwordClipTask, "G327 SwordClipTask", 327, true);
		register_port(ORIG_SwordDrawTask, (void *)SwordDrawTask, "G327 SwordDrawTask", 327, true);
		register_port(ORIG_Gil1Task, (void *)Gil1Task, "G327 Gil1Task", 327, true);
		register_port(ORIG_Gil1DrawTask, (void *)Gil1DrawTask, "G327 Gil1DrawTask", 327, true);
		register_port(ORIG_Gil2Task, (void *)Gil2Task, "G327 Gil2Task", 327);
		register_port(ORIG_Gil2DrawTask, (void *)Gil2DrawTask, "G327 Gil2DrawTask", 327, true);
		register_port(ORIG_Gil3Task, (void *)Gil3Task, "G327 Gil3Task", 327);
		register_port(ORIG_Gil3DrawTask, (void *)Gil3DrawTask, "G327 Gil3DrawTask", 327, true);
		register_port(ORIG_EndTask, (void *)EndTask, "G327 EndTask", 327);
		register_port(ORIG_Zan_Task, (void *)ZanTask, "G327 ZanTask", 327);
		register_port(ORIG_Zan_ScreenPrimTask, (void *)ZanScreenPrimTask, "G327 ZanScreenPrimTask", 327, true);
		register_port(ORIG_Zan_CutTask, (void *)ZanCutTask, "G327 ZanCutTask", 327);
		register_port(ORIG_Zan_SliceTask, (void *)ZanSliceTask, "G327 ZanSliceTask", 327, true);
		register_port(ORIG_Zan_PartyClipTask, (void *)ZanPartyClipTask, "G327 ZanPartyClipTask", 327, true);
		register_port(ORIG_Masa_Task, (void *)MasaTask, "G327 MasaTask", 327);
		register_port(ORIG_Masa_ScreenPrimTask, (void *)MasaScreenPrimTask, "G327 MasaScreenPrimTask", 327, true);
		register_port(ORIG_Masa_PrimATask, (void *)MasaPrimATask, "G327 MasaPrimATask", 327, true);
		register_port(ORIG_Masa_PrimBTask, (void *)MasaPrimBTask, "G327 MasaPrimBTask", 327, true);
		register_port(ORIG_Masa_SparkTask, (void *)MasaSparkTask, "G327 MasaSparkTask", 327, true);
		register_port(ORIG_Masa_DustTask, (void *)MasaDustTask, "G327 MasaDustTask", 327, true);
		register_port(ORIG_Exca_Task, (void *)ExcaTask, "G327 ExcaTask", 327);
		register_port(ORIG_Poor_Task, (void *)PoorTask, "G327 PoorTask", 327);
		register_port(ORIG_Exca_ScreenPrimTask, (void *)ExcaScreenPrimTask, "G327 ExcaScreenPrimTask", 327, true);
		register_port(ORIG_Exca_BurstTask, (void *)ExcaBurstTask, "G327 ExcaBurstTask", 327, true);
		register_port(ORIG_Exca_SmokeTask, (void *)ExcaSmokeTask, "G327 ExcaSmokeTask", 327, true);
		register_port(ORIG_Exca_StreakTask, (void *)ExcaStreakTask, "G327 ExcaStreakTask", 327, true);
		register_port(ORIG_Exca_PillarTask, (void *)ExcaPillarTask, "G327 ExcaPillarTask", 327, true);
		for (int id = 327; id <= 330; id++) register_module_held(id, HeldReady, HeldFrame);
	}
}
