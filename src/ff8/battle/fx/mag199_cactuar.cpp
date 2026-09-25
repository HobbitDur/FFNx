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

// Effect 199: Cactuar - 1000 Needles (timeline-B GF family, MAG_199_*).
//
// Structure (see gf_study/gf_inventory_timeline.md, section 3.2):
//   setup MAG_199_CACTUAR_SUMMON_1000_NEEDLES (0x5A8750, not ported: runs once) - root queue
//     0x22599F0 (pool 0x22599D0, 2 x 0x10) holding the master, sub-queue 0x2259A08 (pool
//     0x2259A18, 100 x 0x24) holding the timeline; every other task goes into the sub-queue.
//   SequenceTick (master, 0x5AA3A0) - runs the sub-queue, ++counter, ends when it is empty.
//   TimelineTask (0x5A8940) - 151 ticks: camera script, sounds, streams, spawns the creature
//     (tick 10), the screen flash in (53) and out (145).
//   CreatureTask (0x5A8C20) - 120 ticks: the Cactuar model (state = global 0x225A838), rises
//     out of the ground (ticks 43..63), spawns the ground prims, the dust and the launcher.
//   particles: rising prim 0x5A9140, dust 0x5A9290, shrinking billboard 0x5A9570, spinning
//     billboard 0x5A9730, needle launcher 0x5A9880, needles 0x5A9C10, per-target damage
//     0x5AA2A0, end-of-effect 0x5AA360, screen flash in 0x5A8B40 / out 0x5A8BC0.
//   The timeline also adds the shared camera-script task MAG_066_sub_63E9C0 (Doomtrain file)
//   to the sub-queue; it is not part of this module and is not ported here.
// Every task tests battle_to_update_flags_dword_1D96A9C & 0x201 (draw-only when set).
// Module globals: 0x2259950..0x225A8E4, cells 0xCF3564/88/8C/90, table-rand seed 0xCF3A68
// (gf_study/gf_global_ranges.md).

#include "fx_port.h"

namespace ff8fx
{
namespace c199
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
		inline void Matrix3x3MultiplyPSX(const void *a, const void *b, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x56C090)(a, b, out); }
		inline int32_t NormalizeVectorToFixedPoint(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); }
		inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
		inline int32_t GetRotationBetweenVectors(const void *ref, const int32_t *v, int32_t *axis) { return fn<int32_t (__cdecl *)(const void *, const int32_t *, int32_t *)>(0x571480)(ref, v, axis); }
		inline void BuildAxisAngleRotationMatrix(int32_t angle, Mat4x3 *out, const int32_t *axis) { fn<void (__cdecl *)(int32_t, Mat4x3 *, const int32_t *)>(0x5714F0)(angle, out, axis); }
		inline uint32_t MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { return fn<uint32_t (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
		inline void CalculateCenterPosition(uint32_t mask, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x5020A0)(mask, out); }
		inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
		// battle model
		inline int32_t PreReadAnimation(void *header, void *cmd, int32_t id) { return fn<int32_t (__cdecl *)(void *, void *, int32_t)>(0x509440)(header, cmd, id); }
		inline int32_t ReadAnimation(void *header, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(0x508F90)(header, cmd); }
		inline uint32_t RenderGeometry(void *geom, void *hdr, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(geom, hdr, ot, shift, cursor); }
		// sound / streams / camera script / battle
		inline void BdPlaySE(const void *se, int32_t a, int32_t b) { fn<void (__cdecl *)(const void *, int32_t, int32_t)>(0x501330)(se, a, b); }
		inline void BdPlaySummonStream(int32_t a, int32_t b, int32_t c) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x5018C0)(a, b, c); }
		inline void BdTransSummonStream(const void *a, void *b) { fn<void (__cdecl *)(const void *, void *)>(0x501860)(a, b); }
		inline int32_t VoiceSlotBusy(uint32_t slot) { return fn<int32_t (__cdecl *)(uint32_t)>(0x4A2900)(slot); }   // sub_4A2900
		inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }        // sub_4A2940
		inline void CameraScriptStart(const void *script, const void *frame, void *entity, TaskQueue *q, int32_t n) { fn<void (__cdecl *)(const void *, const void *, void *, TaskQueue *, int32_t)>(0x63E960)(script, frame, entity, q, n); } // MAG_066_sub_63E960
		inline void CameraShake(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x5712C0)(a, b, c, d); } // au_re_BdLinkTask_6
		inline void ApplyActionResultToTarget(void *target) { fn<void (__cdecl *)(void *)>(0x506690)(target); }
		inline void QueueChainTransformation(void *entity, int32_t k) { fn<void (__cdecl *)(void *, int32_t)>(0x505C00)(entity, k); }
		inline void WaitAnimSeq(void *buffer, void *flag) { fn<void (__cdecl *)(void *, void *)>(0x508630)(buffer, flag); } // sub_508630
		// software GTE
		inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
		inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // MAG_148_sub_64DDA0: BK = m.t
		inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }
		inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }    // MAG_163_sub_64DE00: R = s * I
		inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }                    // MAG_164_sub_64DE40: TR = MAC123
		inline void GteLoadIRFromMatrixColumn(const void *m) { fn<void (__cdecl *)(const void *)>(0x45E180)(m); }
		inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }
		inline void GteStoreIRToMatrixColumn(void *m) { fn<void (__cdecl *)(void *)>(0x45E470)(m); }
		inline void GteLoadV0FromDwords(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x45E060)(v); }
		inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
		inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
		inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
		inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
		inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
		inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }
	}
	using namespace x;

	// --- module globals ---
	inline bool Paused() { return (var<uint32_t>(0x1D96A9C) & 0x201) != 0; } // battle_to_update_flags_dword_1D96A9C
	inline Mat4x3 &RootMatrix() { return var<Mat4x3>(0x2259950); }   // effect frame (setup: rotation, centroid 0x2259964/68/6C)
	inline Mat4x3 &Frame() { return var<Mat4x3>(0x2259978); }        // Camera o RootMatrix, rebuilt by the timeline every tick
	inline TaskQueue &SubQueue() { return var<TaskQueue>(0x2259A08); }
	inline uint32_t &RootQueueFlag() { return var<uint32_t>(0x2259A00); } // root queue pair .second.head, used as a done flag
	inline uint8_t *CastCtx() { return var<uint8_t *>(0x225A828); }
	inline uint32_t &VoiceSlot() { return var<uint32_t>(0x225A834); }
	inline uint8_t *Creature() { return (uint8_t *)0x225A838; }        // BattleAnimHeader at +0x60, BattleAnimCmd at +0x6C
	inline Mat4x3 &CreatureMat() { return var<Mat4x3>(0x225A878); }  // creature +0x40
	inline int32_t &CreatureY() { return var<int32_t>(0x225A890); }  // CreatureMat.t[1]
	inline uint8_t *ModelBuffer() { return var<uint8_t *>(0x225A8E0); }
	inline uint8_t *DustPool() { return var<uint8_t *>(0xCF3588); }   // modelBuffer + 0x2A000, 100 x 0x1C
	inline uint8_t *NeedlePool() { return var<uint8_t *>(0xCF358C); } // modelBuffer + 0x2B000, 800 x 0x3C
	inline uint8_t *AimTable() { return var<uint8_t *>(0xCF3590); }   // modelBuffer + 0x20000, 512 x 0x30
	inline uint32_t &Cursor() { return var<uint32_t>(0x1D8E054); }   // battle_texture_data_ptr_1D8E054
	inline uint32_t OT() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	// target list of the cast: +0x08 -> 24-byte records (byte 0 = battle slot), +0x10 u8 count
	inline uint8_t *Targets() { return *(uint8_t **)(CastCtx() + 4); }
	inline uint8_t *Entity(uint32_t slot) { return (uint8_t *)(0x1D972C0 + 156 * slot); }

	static const uint32_t ORIG_SequenceTick = 0x5AA3A0;
	static const uint32_t ORIG_TimelineTask = 0x5A8940;
	static const uint32_t ORIG_CreatureTask = 0x5A8C20;
	static const uint32_t ORIG_FlashInTask = 0x5A8B40;
	static const uint32_t ORIG_FlashOutTask = 0x5A8BC0;
	static const uint32_t ORIG_RisePrimTask = 0x5A9140;
	static const uint32_t ORIG_DustTask = 0x5A9290;
	static const uint32_t ORIG_ShrinkPrimTask = 0x5A9570;
	static const uint32_t ORIG_SpinPrimTask = 0x5A9730;
	static const uint32_t ORIG_LauncherTask = 0x5A9880;
	static const uint32_t ORIG_NeedleTask = 0x5A9C10;
	static const uint32_t ORIG_DamageTask = 0x5AA2A0;
	static const uint32_t ORIG_EndTask = 0x5AA360;

	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;

	// every sub-queue node is 0x24 bytes (pool 0x2259A18); fields by task
#pragma pack(push, 1)
	struct Node24
	{
		TaskNode hdr;
		int16_t c;        // +0x0C tick counter
		int16_t e;        // +0x0E flash: length / damage: delay / end: done flag (byte)
		int16_t p[3];     // +0x10 position (prims, launcher)
		int16_t pad16;
		int16_t a;        // +0x18 prims: angle / flash: level / damage: target index
		int16_t b;        // +0x1A spin / damage: battle slot
		int16_t s;        // +0x1C scale
		int16_t ds;       // +0x1E scale speed
		int16_t pad20;
		int16_t v;        // +0x22 creature: rise speed
	};
#pragma pack(pop)
	static_assert(sizeof(Node24) == 0x24, "sub-queue node is 0x24 bytes");

	// au_re__rand 0x5A8AE0: reseed the table generator from the CRT rand()
	static void SeedTableRand() { var<int32_t>(0xCF3A68) = CrtRand(); }

	// MAG_199_sub_5A9BF0: table generator, 256 dwords at 0xCF3668
	static int32_t TableRand()
	{
		uint32_t &seed = var<uint32_t>(0xCF3A68);
		uint32_t s = seed;
		seed = s + 1;
		return ((const int32_t *)0xCF3668)[s & 0xFF];
	}

	// au_re_BdLinkTask_24 0x5A8AF0 (no null check, as the original)
	static Node24 *LinkTask(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), task_fn);
		n->c = 0;
		n->e = 0;
		return n;
	}

	// MAG_199_sub_5A90B0: rotation part = identity (translation and pad kept)
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

	// ------------------------------------------------------------------
	// Master (0x5AA3A0): node from pool 0x22599D0, +0x0C counter
	// ------------------------------------------------------------------
	static uint32_t __cdecl SequenceTick(TaskNode *n)
	{
		g_ported_tick = g_real_tick;
		int left = ExecuteTaskQueue(&SubQueue());
		((Node24 *)n)->c++;
		return left ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Timeline (0x5A8940), 151 ticks
	// ------------------------------------------------------------------
	static void LinkFlash(uint32_t task_fn, int16_t length) // au_re_BdLinkTask_25 0x5A8B10 / _26 0x5A8B90
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), task_fn);
		n->c = 0;
		n->e = length;
		n->a = 0x800;
	}

	static uint32_t __cdecl TimelineTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ComposeAffineTransform(&Camera(), &RootMatrix(), &Frame());
		if (Paused()) return 0;
		if (n->c == 0)
		{
			CameraScriptStart((const void *)0xCF35BC, &RootMatrix(), Creature(), &SubQueue(), 0);
			BdTransSummonStream((const void *)0xE94910, (void *)0x225A8D4);
			SeedTableRand();
			VoiceSlot() = ClaimVoiceSlot((const void *)0xCF3318, 1, 0x80);
		}
		switch (n->c)
		{
		case 10:
			LinkTask(ORIG_CreatureTask);
			break;
		case 15:
			BdPlaySE((const void *)0xCF3558, 0, 0x80);
			BdPlaySummonStream(0x80, 0, 0x60);
			break;
		case 53:
			BdPlaySE((const void *)0xCF355C, 0, 0x80);
			BdTransSummonStream((const void *)0xE94914, (void *)0x225A8D4);
			LinkFlash(ORIG_FlashInTask, 0xC);
			break;
		case 76:
			BdPlaySummonStream(0x80, 0, 0x60);
			break;
		case 145:
			LinkFlash(ORIG_FlashOutTask, 8);
			break;
		case 148:
			if (VoiceSlotBusy(VoiceSlot())) ReleaseVoiceSlot(VoiceSlot());
			break;
		}
		n->c++;
		return n->c > 0x97 ? TASK_END : 0;
	}

	// 0x5A8B40: screen flash up to level over length ticks
	static uint32_t __cdecl FlashInTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		SetScreenFlash((uint32_t)mul32((int32_t)n->a / (int32_t)n->e, n->c), 0);
		if (Paused()) return 0;
		n->c++;
		return n->c >= n->e ? TASK_END : 0;
	}

	// 0x5A8BC0: screen flash down from level, cleared at the end
	static uint32_t __cdecl FlashOutTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t level = n->a;
		SetScreenFlash((uint32_t)(level - mul32(level / (int32_t)n->e, n->c)), 0);
		if (Paused()) return 0;
		n->c++;
		if (n->c < n->e) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Creature (0x5A8C20), 120 ticks. State E = 0x225A838:
	//   +0x00 u16 flags (2), +0x26 s16 scale (0x1800), +0x28 colour, +0x40 model matrix
	//   (0x225A878, t[1] = height 0x225A890), +0x60 BattleAnimHeader, +0x64 comFileData,
	//   +0x6C BattleAnimCmd, +0x7C, +0x84 model data 0x2259998.
	// Node: +0x0C counter, +0x22 rise speed.
	// ------------------------------------------------------------------

	// MAG_199_sub_5A8F40: bind the model data (model buffer offsets table) to the state
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

	// au_re_Battle_ReadAnimation_1 0x5A8FB0
	static void SetAnim(int32_t id) { PreReadAnimation(Creature() + 0x60, Creature() + 0x6C, id); }

	// GF_199Cactuar_AdvanceModelAnimLoop 0x5A8FD0
	static void AdvanceModelAnimLoop(uint8_t *E)
	{
		if (ReadAnimation(E + 0x60, E + 0x6C) == 1 && !(E[0] & 1))
			PreReadAnimation(E + 0x60, E + 0x6C, E[0x6C]);
	}

	// GF_199Cactuar_DrawModel 0x5A9010
	static void DrawModel(uint8_t *E, const Mat4x3 *frame)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		ComposeAffineTransform(frame, (const Mat4x3 *)(E + 0x40), (Mat4x3 *)h);
		ComputeBonesWorldMatrices(E + 0x60, h);
		*(uint32_t *)(h + 0x30) = var<uint32_t>(0x1D969A8);
		*(uint32_t *)(h + 0x24) = var<uint32_t>(0xCF3564);
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

	// 0x5A8E27..0x5A8ECB: model matrix = scale x rotation facing the saved camera direction
	// (Doomtrain-file snapshot 0x24FD250/254 look-at, 0x24FD358/35C world, written by the
	// camera script), then the model
	static void CreatureDraw(const Mat4x3 *frame)
	{
		ResetRotation(&CreatureMat());
		int32_t s = var<int16_t>(0x225A85E);
		int32_t vec[3] = { s, s, s };
		Scale3DMatrix(&CreatureMat(), vec);
		int32_t d[3];
		d[0] = (int32_t)var<int16_t>(0x24FD358) - (int32_t)var<int16_t>(0x24FD250);
		d[1] = 0;
		d[2] = (int32_t)var<int16_t>(0x24FD35C) - (int32_t)var<int16_t>(0x24FD254);
		NormalizeVectorToFixedPoint(d, d);
		int32_t axis[4];
		int32_t angle = GetRotationBetweenVectors((const void *)0xCF3568, d, axis);
		Mat4x3 r;
		BuildAxisAngleRotationMatrix(angle, &r, axis);
		GteMatrixMultiply(&r, &CreatureMat());
		DrawModel(Creature(), frame);
	}

	// 0x5A90E0: rising prim under the creature
	static void SpawnRisePrim()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_RisePrimTask);
		n->c = 0;
		n->p[0] = (int16_t)var<int32_t>(0x225A88C);
		n->p[1] = 0;
		n->p[2] = (int16_t)var<int32_t>(0x225A894);
		n->a = (int16_t)(CrtRand() % 0x1000);
		n->s = 0x1000;
	}

	// au_re_BdLinkTask_27 0x5A9520: shrinking billboard
	static void SpawnShrinkPrim()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_ShrinkPrimTask);
		n->p[0] = (int16_t)var<int32_t>(0x225A88C);
		n->c = 0;
		n->p[1] = (int16_t)(CreatureY() - 0x17C);
		n->p[2] = (int16_t)(var<int32_t>(0x225A894) + 0x64);
		n->s = 0x3000;
		n->ds = 0x266;
	}

	// MAG_199_sub_5A96C0: spinning billboard
	static void SpawnSpinPrim()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_SpinPrimTask);
		n->c = 0;
		n->p[0] = (int16_t)var<int32_t>(0x225A88C);
		n->p[1] = (int16_t)(CreatureY() - 0x17C);
		n->p[2] = (int16_t)(var<int32_t>(0x225A894) + 0x64);
		n->a = (int16_t)(CrtRand() % 0x1000);
		n->b = 0x1E;
		n->s = 0x2000;
	}

	// held-frame memo of the creature (one instance): the pose it drew, its height
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct CreatureMemo
	{
		uint32_t tick;
		bool midpoint;         // pose advanced after the draw and the next tick shows the next frame
		int32_t y_drawn, y_next;
		uint32_t skel_size;
		uint8_t cmd[8];
		uint8_t skel[SKEL_MAX];
	};
	static CreatureMemo g_creature = { 0xFFFFFFFF };

	static uint8_t *Skeleton()
	{
		uint8_t *com = *(uint8_t **)(Creature() + 0x64); // BattleAnimHeader.comFileData
		return com ? *(uint8_t **)com : nullptr;
	}

	// height the next tick will draw (the rise of ticks 43..63), from its counter and speed
	static int32_t NextCreatureY(int32_t y, int32_t c, int16_t v)
	{
		if (c >= 43 && c < 59)
		{
			if (c == 43) v = -600;
			return y + v;
		}
		if (c >= 59 && c < 64)
		{
			if (c == 59) v = -60;
			return y + v;
		}
		return y;
	}

	static uint32_t __cdecl CreatureTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *E = Creature();
		int advance = 1;
		if (n->c == 0)
		{
			ResetRotation(&CreatureMat());
			var<uint16_t>(0x225A888) = 0x1800;
			var<uint16_t>(0x225A880) = 0x1800;
			var<uint16_t>(0x225A878) = 0x1800;
			var<uint16_t>(0x225A85E) = 0x1800;
			var<int32_t>(0x225A890) = 0;
			var<int32_t>(0x225A88C) = 0;
			var<int32_t>(0x225A894) = 0;
			ModelInit(E, (uint8_t *)0x2259998, ModelBuffer(), 1);
		}
		if (!Paused())
		{
			int32_t c = n->c;
			if (c < 5)
			{
				if (c == 0) SetAnim(0);
			}
			else if (c < 10)
			{
				if (c == 5) SetAnim(1);
			}
			else if (c < 20) advance = 0;
			else if (c < 43)
			{
				if (c < 40) advance = 0;
				if (c == 40) SetAnim(3);
			}
			else if (c < 59) // rise, speed -600 decaying by 1/8
			{
				advance = 0;
				if (c == 43) n->v = -600;
				int16_t v = n->v;
				CreatureY() += v;
				n->v = (int16_t)(v - v / 8);
				if (c == 43)
				{
					SetAnim(2);
					SpawnRisePrim();
					LinkTask(ORIG_DustTask);
				}
			}
			else if (c < 116)
			{
				int32_t k = c - 59;
				if (k < 28) advance = 0;
				if (k == 0) n->v = -60;
				if (k < 5) // settle, speed -60 decaying by 1/3
				{
					int16_t v = n->v;
					CreatureY() += v;
					n->v = (int16_t)(v - v / 3);
				}
				if (k == 28)
				{
					SetAnim(4);
					LinkTask(ORIG_LauncherTask);
				}
				else if (k == 4) SpawnShrinkPrim();
				else if (k == 5) SpawnSpinPrim();
				else if (k == 20) CameraShake(2, 1, 8, 0xFF);
			}
			else if (c < 120) advance = 0;
		}
		CreatureDraw(&Frame());

		// memo of what was drawn (pose values + matrices of the drawn pose, reader command)
		CreatureMemo &m = g_creature;
		uint8_t *sk = Skeleton();
		m.tick = 0xFFFFFFFF;
		if (sk)
		{
			m.skel_size = 16 + 48 * (uint32_t)sk[0];
			memcpy(m.skel, sk, m.skel_size);
			memcpy(m.cmd, E + 0x6C, 8);
			m.y_drawn = CreatureY();
			m.y_next = m.y_drawn;
			m.midpoint = false;
			m.tick = g_real_tick;
		}

		if (Paused()) return 0;
		if (advance) AdvanceModelAnimLoop(E);
		n->c++;
		if (m.tick == g_real_tick)
		{
			int32_t c = n->c;
			m.y_next = NextCreatureY(m.y_drawn, c, n->v);
			// the next tick draws the frame just read unless it starts another animation
			m.midpoint = advance && c != 0 && c != 5 && c != 40 && c != 43 && c != 87;
		}
		if (n->c < 0x78) return 0;
		RootQueueFlag() |= 1;
		LinkTask(ORIG_EndTask);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Prim model tasks (Effect_RenderPrimModel, header on the scratch stack)
	// ------------------------------------------------------------------
	// frame o (rotation, position, scale); billboard: GTE rotation from the local matrix only
	// (0x5A9570 / 0x5A9730), else from the composed one (0x5A9140, composed in place)
	static void PrimDraw(const Mat4x3 *frame, const int16_t *angles, const int16_t *pos, const int32_t *scale,
		bool billboard, uint32_t model, bool fade, int32_t fade_value)
	{
		Mat4x3 m, w;
		ComposeZYXRotationMatrix(angles, &m);
		m.t[0] = pos[0];
		m.t[1] = pos[1];
		m.t[2] = pos[2];
		Scale3DMatrix(&m, scale);
		if (billboard)
		{
			ComposeAffineTransform(frame, &m, &w);
			GteSetRotMatrix(&m);
			GteSetTransVector(&w);
		}
		else
		{
			ComposeAffineTransform(frame, &m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
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

	struct PrimMemo { int16_t c, v; };
	static NodeMemo<PrimMemo, 64> g_prim_memo;

	// 0x5A9140: prim 0xCF26F8 at the creature's feet, stretched vertically by the creature's
	// height (y scale = -height * 4096 / 1590), fading out over its last 6 of 12 ticks
	static void RisePrimDraw(const Mat4x3 *frame, const Node24 *n, int32_t y, bool fade, int32_t fade_value)
	{
		int16_t angles[4] = { 0, n->a, 0, 0 };
		int32_t scale[3];
		scale[0] = n->s;
		scale[2] = n->s;
		scale[1] = shl32((int32_t)(0u - (uint32_t)y), 12) / 1590;
		PrimDraw(frame, angles, n->p, scale, false, 0xCF26F8, fade, fade_value);
	}

	static uint32_t __cdecl RisePrimTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t c = n->c;
		RisePrimDraw(&Frame(), n, CreatureY(), c >= 6, 682 * (c - 6));
		if (PrimMemo *m = g_prim_memo.put(tn)) { m->c = c; m->v = 0; }
		if (Paused()) return 0;
		n->c++;
		return n->c >= 12 ? TASK_END : 0;
	}

	// 0x5A9570: billboard 0xCF08EC shrinking (scale -= speed, speed -= speed/20 for 16
	// ticks), fading in over 4 ticks, 60 ticks
	static void ShrinkPrimDraw(const Mat4x3 *frame, const Node24 *n, int32_t s, bool fade, int32_t fade_value)
	{
		int16_t angles[4] = { 0, 0, 0, 0 };
		int32_t scale[3] = { s, s, s };
		PrimDraw(frame, angles, n->p, scale, true, 0xCF08EC, fade, fade_value);
	}

	static uint32_t __cdecl ShrinkPrimTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t c = n->c;
		ShrinkPrimDraw(&Frame(), n, n->s, c < 4, shl32(4 - c, 10));
		if (PrimMemo *m = g_prim_memo.put(tn)) { m->c = c; m->v = n->s; }
		if (Paused()) return 0;
		c = n->c;
		if (c < 16)
		{
			int16_t ds = n->ds;
			n->s = (int16_t)(n->s - ds);
			n->ds = (int16_t)(ds - ds / 20);
		}
		n->c = (int16_t)(c + 1);
		return n->c >= 0x3C ? TASK_END : 0;
	}

	// 0x5A9730: billboard 0xCF11BC spinning around the view axis, fading in (8 ticks) and
	// out (from tick 14), 24 ticks
	static void SpinPrimDraw(const Mat4x3 *frame, const Node24 *n, int16_t angle, bool fade, int32_t fade_value)
	{
		int16_t angles[4] = { 0, 0, angle, 0 };
		int32_t scale[3] = { n->s, n->s, n->s };
		PrimDraw(frame, angles, n->p, scale, true, 0xCF11BC, fade, fade_value);
	}

	static int32_t SpinFade(int32_t c) { return c < 8 ? shl32(8 - c, 9) : 409 * (c - 14); }

	static uint32_t __cdecl SpinPrimTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t c = n->c;
		SpinPrimDraw(&Frame(), n, n->a, c < 8 || c >= 14, SpinFade(c));
		if (PrimMemo *m = g_prim_memo.put(tn)) { m->c = c; m->v = n->a; }
		if (Paused()) return 0;
		n->a = (int16_t)(n->a + n->b);
		n->c++;
		return n->c >= 0x18 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Dust (0x5A9290): 6 puffs per tick while the counter <= 4, pool of 100 x 0x1C at
	// modelBuffer + 0x2A000. A puff is a flipbook (0xCF0740) lit/transformed through the GTE
	// light matrix; it drifts on x/z with a velocity damped by 1/8 per tick and ends with its
	// sequence. The task ends on the first tick where no puff is left.
	// The spawn position is the node's +0x10..+0x17, which BdLinkTask_24 never writes (stale
	// pool bytes, vanilla).
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct DustPuff
	{
		uint32_t alive;   // +0x00
		int16_t age;      // +0x04 flipbook frame
		int16_t size;     // +0x06 GTE rotation scale 0x800..0xDFF
		uint32_t pad08;
		int16_t pos[4];   // +0x0C x, y, z, (copied pad)
		int16_t vel_x;    // +0x14
		int16_t pad16;
		int16_t vel_z;    // +0x18
		int16_t pad1A;
	};
#pragma pack(pop)
	static_assert(sizeof(DustPuff) == 0x1C, "dust puff is 0x1C bytes");

	struct DustMemo { uint32_t tick; int16_t pos[4]; int16_t age; };
	static DustMemo g_dust_memo[100];

	static void DustDrawOne(uint8_t *h, const int16_t *pos, int16_t size, int16_t frame)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int16_t *)(h + 4) = frame;
		GteTransFromMAC();
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static uint32_t __cdecl DustTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = DustPool();
		uint8_t *scratch = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 4)
		{
			for (int k = 0; k < 6; k++)
			{
				int i = 0;
				while (i < 100 && *(uint32_t *)(pool + 0x1C * i) != 0) i++;
				if (i >= 100) break;
				DustPuff *p = (DustPuff *)(pool + 0x1C * i);
				p->alive = 1;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0x600 + 0x800);
				int32_t angle = CrtRand() % 0x1000;
				int32_t r = CrtRand() % 200 + 300;
				memcpy(p->pos, &n->p[0], 8);
				p->pos[0] = (int16_t)(p->pos[0] + (mul32(ComputeSin(angle), r) >> 12));
				int32_t up = CrtRand() % 80;
				p->pos[1] = (int16_t)(p->pos[1] + (-80 - up));
				p->pos[2] = (int16_t)(p->pos[2] + (mul32(ComputeCos(angle), r) >> 12));
				int32_t sp = CrtRand() % 120 + 90;
				p->vel_x = (int16_t)(mul32(ComputeSin(angle), sp) >> 12);
				p->vel_z = (int16_t)(mul32(ComputeCos(angle), sp) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		memcpy(scratch + 8, &Frame(), sizeof(Mat4x3));
		*(uint32_t *)h = 0xCF0740;
		*(uint16_t *)(h + 0x24) = 0;
		int32_t count = 0;
		GteSetLightMatrix(scratch + 8);
		GteSetBackColorFromTrans(scratch + 8);
		for (int i = 0; i < 100; i++)
		{
			DustPuff *p = (DustPuff *)(pool + 0x1C * i);
			if (!(p->alive & 1)) continue;
			DustMemo &m = g_dust_memo[i];
			m.tick = g_real_tick;
			memcpy(m.pos, p->pos, 8);
			m.age = p->age;
			DustDrawOne(h, p->pos, p->size, p->age);
			if (Paused()) continue;
			p->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				p->alive = 0;
				continue;
			}
			p->pos[0] = (int16_t)(p->pos[0] + p->vel_x);
			p->pos[2] = (int16_t)(p->pos[2] + p->vel_z);
			p->vel_x = (int16_t)(p->vel_x - (int16_t)(p->vel_x >> 3));
			p->vel_z = (int16_t)(p->vel_z - (int16_t)(p->vel_z >> 3));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Needle launcher (0x5A9880), 4 ticks, draws nothing. Tick 0: launch point = creature
	// spawn position (bone 0xF0) raised by 600 / pushed 900, in the effect frame; builds the
	// aim table. Tick 1: the needle task and one damage task per target.
	// ------------------------------------------------------------------

	// MAG_199_sub_5A99D0: 512 aim records of 0x30 bytes at 0xCF3590 (modelBuffer + 0x20000):
	//   +0x00 s16 x3 origin, +0x06 s16 distance, +0x08 s16 x3 aim point, +0x0E s16 start
	//   offset 1000..1799, +0x24 s32 x3 unit direction (+0x10..+0x23 untouched).
	// Half of them aim at a random point of a random target bone (+-250), half at a random
	// point of the 8000 x 10000 floor area around the targets' centre (y = 0).
	static void BuildAimTable(const int16_t *origin, const int16_t *center)
	{
		uint8_t *pts = ModelBuffer() + 0x1C000; // s16 x4 per bone of every target
		int32_t npts = 0;
		for (uint32_t i = 0; i < Targets()[0x10]; i++)
		{
			uint32_t slot = (*(uint8_t **)(Targets() + 8))[24 * i];
			uint8_t *com = *(uint8_t **)(0x1D97324 + 156 * slot);
			int32_t nb = **(uint8_t **)com;
			if (nb <= 0) continue;
			int16_t *out = (int16_t *)(pts + 8 * npts);
			npts += nb;
			for (int32_t b = 0; b < nb; b++)
			{
				int32_t r = TableRand() % 0x1000;
				GetEffectSpawnPosition(Entity(slot), b, r, out);
				out += 4;
			}
		}
		uint8_t *rec = AimTable();
		for (int k = 0x200; k; k--, rec += 0x30)
		{
			int32_t r = TableRand();
			if (r % 2 != 0)
			{
				int32_t rx = TableRand() % 8000;
				*(int16_t *)(rec + 0xA) = 0;
				*(int16_t *)(rec + 8) = (int16_t)(rx + center[0] - 0xFA0);
				int32_t rz = TableRand() % 10000;
				*(int16_t *)(rec + 0xC) = (int16_t)(center[2] - rz + 0xFA0);
			}
			else
			{
				const int16_t *pt = (const int16_t *)(pts + 8 * (r % npts));
				int32_t rx = TableRand() % 500;
				*(int16_t *)(rec + 8) = (int16_t)(rx + pt[0] - 0xFA);
				int32_t ry = TableRand() % 500;
				*(int16_t *)(rec + 0xA) = (int16_t)(ry + pt[1] - 0xFA);
				int32_t rz = TableRand() % 500;
				*(int16_t *)(rec + 0xC) = (int16_t)(rz + pt[2] - 0xFA);
			}
			int32_t d[3];
			d[0] = (int32_t)*(int16_t *)(rec + 8) - origin[0];
			d[1] = (int32_t)*(int16_t *)(rec + 0xA) - origin[1];
			d[2] = (int32_t)*(int16_t *)(rec + 0xC) - origin[2];
			int32_t len2 = NormalizeVectorToFixedPoint(d, (int32_t *)(rec + 0x24));
			int32_t off = TableRand() % 800;
			*(int16_t *)(rec + 0xE) = (int16_t)(off + 1000);
			memcpy(rec, origin, 8);
			*(int16_t *)(rec + 6) = (int16_t)Sqrt(len2);
		}
	}

	static uint32_t __cdecl LauncherTask(TaskNode *tn)
	{
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			GetEffectSpawnPosition(Creature(), 0xF0, 0xC00, n->p);
			n->p[1] = (int16_t)(n->p[1] - 600);
			n->p[2] = (int16_t)(n->p[2] + 900);
			int16_t origin[4] = { 0, 0, 0, 0 }, center[4] = { 0, 0, 0, 0 };
			uint32_t r = MatrixMultiplyVector(&RootMatrix(), n->p, origin);
			origin[1] = (int16_t)(origin[1] + (int16_t)var<int32_t>(0x2259968));
			origin[0] = (int16_t)(origin[0] + (int16_t)var<int32_t>(0x2259964));
			origin[2] = (int16_t)(origin[2] + (int16_t)var<int32_t>(0x225996C));
			// the original passes eax with only its low word loaded (high word = the return
			// value of matrixMultiplyVector); CalculateCenterPosition masks it to 16 bits
			CalculateCenterPosition((r & 0xFFFF0000) | *(uint16_t *)(CastCtx() + 2), center);
			BuildAimTable(origin, center);
		}
		if (n->c == 1)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_NeedleTask);
			t->c = 0;
			for (uint32_t i = 0; i < Targets()[0x10]; i++)
			{
				uint8_t slot = (*(uint8_t **)(Targets() + 8))[24 * i];
				Node24 *d = (Node24 *)AddTaskToQueue(&SubQueue(), ORIG_DamageTask);
				d->c = 0;
				d->e = (int16_t)(CrtRand() % 8);
				d->a = (int16_t)i;
				d->b = slot;
			}
		}
		n->c++;
		return n->c >= 4 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Damage (0x5AA2A0): after a 0..7 tick delay, every third tick of 50 a hit reaction on the
	// target's entity (QueueChainTransformation 4), the action result on the last one; 55 ticks
	// ------------------------------------------------------------------
	static uint32_t __cdecl DamageTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (Paused()) return 0;
			n->e--;
			return 0;
		}
		if (Paused()) return 0;
		int32_t c = n->c;
		if (c < 50 && c % 3 == 1)
		{
			if (c + 3 >= 50)
				ApplyActionResultToTarget(*(uint8_t **)(Targets() + 8) + 24 * (int32_t)n->a);
			else
				QueueChainTransformation(Entity((uint32_t)(int32_t)n->b), 4);
		}
		n->c++;
		return n->c >= 0x37 ? TASK_END : 0;
	}

	// 0x5AA360: end of effect, waits for the anim-seq task queued at tick 1 to clear +0x0E
	static uint32_t __cdecl EndTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 1) WaitAnimSeq(ModelBuffer() + 0x4000, &n->e);
		uint16_t done = (uint16_t)n->e;
		n->c++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Needles (0x5A9C10): 50 per tick while the counter < 45, pool of 800 x 0x3C at
	// modelBuffer + 0x2B000. A needle starts at a random aim record's origin (+ its start
	// offset along the direction), flies in 5..10 steps to the aim point (+-800 jitter),
	// oriented along its direction; fade-in colours 0xCF3594[age < 4], then on arrival six
	// fade-out colours 0xCF35A4. Drawn as two crossed textured quads (GP0 0x2E) of the
	// template below; the task ends on the first tick where no needle was drawn.
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct Needle
	{
		uint32_t alive;   // +0x00
		int16_t age;      // +0x04
		int16_t length;   // +0x06 z scale 0xA00..0x1DFF
		int16_t fade;     // +0x08 fade-out step
		int16_t aim;      // +0x0A aim record index
		int16_t pos[4];   // +0x0C x, y, z, (record distance)
		int16_t vel[3];   // +0x14
		int16_t steps;    // +0x1A flight ticks + 1
		int16_t rot[9];   // +0x1C 3x3 orientation (4.12)
		int16_t pad2E;
		int32_t pos32[3]; // +0x30 pos as dwords (GTE V0), written by the draw
	};
#pragma pack(pop)
	static_assert(sizeof(Needle) == 0x3C, "needle is 0x3C bytes");

	static const uint32_t *const NEEDLE_FADE_IN = (const uint32_t *)0xCF3594;
	static const uint32_t *const NEEDLE_FADE_OUT = (const uint32_t *)0xCF35A4;

	// packet template, 0x68 bytes: +0 OT base, +4 OT shift (0xE), +8 FLAG, +0xC OTZ,
	// +0x10 colour+code, quad A vertices at +0x14/+0x1C/+0x24/+0x2C, quad B at
	// +0x34/+0x3C/+0x44/+0x4C, +0x54 default colour, +0x58 uv0+CLUT, +0x5C uv1+tpage,
	// +0x60 uv2, +0x64 uv3 (the high halves of +0x60/+0x64 are never written: scratch bytes,
	// copied into the packets as vanilla does)
	static void NeedleTemplate(uint8_t *T)
	{
		*(uint32_t *)(T + 0x10) = 0x2E808080;
		*(uint32_t *)(T + 0x54) = 0x2E808080;
		*(int16_t *)(T + 0x2C) = 0x40;
		T[0x64] = 0xF;
		T[0x5C] = 0xF;
		*(int16_t *)(T + 0x1C) = 0x40;
		*(int16_t *)(T + 0x30) = 0;
		*(int16_t *)(T + 0x28) = 0;
		*(int16_t *)(T + 0x2E) = 0;
		*(int16_t *)(T + 0x26) = 0;
		*(int16_t *)(T + 0x1E) = 0;
		*(int16_t *)(T + 0x16) = 0;
		*(int16_t *)(T + 0x4E) = 0x40;
		*(int16_t *)(T + 0x3E) = 0x40;
		*(int16_t *)(T + 0x50) = 0;
		*(int16_t *)(T + 0x48) = 0;
		*(int16_t *)(T + 0x4C) = 0;
		*(int16_t *)(T + 0x44) = 0;
		*(int16_t *)(T + 0x3C) = 0;
		*(int16_t *)(T + 0x34) = 0;
		T[0x65] = 0x80;
		T[0x61] = 0x80;
		*(int16_t *)(T + 0x20) = 0x200;
		*(int16_t *)(T + 0x18) = 0x200;
		*(int16_t *)(T + 0x40) = 0x200;
		*(int16_t *)(T + 0x38) = 0x200;
		*(int16_t *)(T + 0x24) = -0x40;
		*(int16_t *)(T + 0x14) = -0x40;
		*(int16_t *)(T + 0x46) = -0x40;
		*(int16_t *)(T + 0x36) = -0x40;
		*(uint32_t *)T = OT();
		*(uint16_t *)(T + 0x5A) = 0x3F54;
		*(uint16_t *)(T + 0x5E) = 0xB9;
		T[0x60] = 0;
		T[0x58] = 0;
		T[0x5D] = 0;
		T[0x59] = 0;
		*(uint32_t *)(T + 4) = 0xE;
	}

	// GTE matrices of one needle (camera at scratch + 0x10) and quad A's RTPT
	static void NeedleProject(uint8_t *scratch, const int16_t *rot, const int32_t *pos32, const uint8_t *T)
	{
		GteSetRotMatrixCtrl((const Mat4x3 *)(scratch + 0x10));
		GteLoadIRFromMatrixColumn(rot + 0);
		GteMVMVA_RotIR();
		GteStoreIRToMatrixColumn(scratch + 0x30);
		GteLoadIRFromMatrixColumn(rot + 1);
		GteMVMVA_RotIR();
		GteStoreIRToMatrixColumn(scratch + 0x32);
		GteLoadIRFromMatrixColumn(rot + 2);
		GteMVMVA_RotIR();
		GteStoreIRToMatrixColumn(scratch + 0x34);
		GteSetTransVectorCtrl((const Mat4x3 *)(scratch + 0x10));
		GteLoadV0FromDwords(pos32);
		GteMVMVA_RotV0Tr();
		GteReadMAC123((int32_t *)(scratch + 0x44));
		GteSetRotMatrixCtrl((const Mat4x3 *)(scratch + 0x30));
		GteSetTransVectorCtrl((const Mat4x3 *)(scratch + 0x30));
		GteLoadV012(T + 0x14, T + 0x1C, T + 0x24);
		GteRTPT();
	}

	static void NeedlePacketHeader(uint32_t *pk, const uint8_t *T)
	{
		pk[3] = *(const uint32_t *)(T + 0x58);
		pk[1] = *(const uint32_t *)(T + 0x10);
		pk[9] = *(const uint32_t *)(T + 0x64);
		pk[0] = 0x09000000;
		pk[5] = *(const uint32_t *)(T + 0x5C);
		pk[7] = *(const uint32_t *)(T + 0x60);
	}

	static uint32_t NeedleBucket(const uint8_t *T)
	{
		int32_t otz = *(const int32_t *)(T + 0xC);
		return *(const uint32_t *)T + 4 * (uint32_t)(otz >> (*(const uint32_t *)(T + 4) & 31));
	}

	// both quads (quad A already through RTPT); true when both were inserted
	static bool NeedleEmit(uint8_t *T, uint32_t &cursor)
	{
		uint32_t *pk = (uint32_t *)cursor;
		NeedlePacketHeader(pk, T);
		GteReadFLAG(T + 8);
		if (*(uint32_t *)(T + 8) & 0x60000) return false;
		GteReadSXY012Split(pk + 2, pk + 4, pk + 6);
		GteLoadV0(T + 0x2C);
		GteRTPS();
		GteReadSXY2(pk + 8);
		GteAVSZ4();
		GteReadOTZ32(T + 0xC);
		InsertPrimAutoDepth(NeedleBucket(T), pk);
		cursor += 0x28;
		pk = (uint32_t *)cursor;
		GteLoadV012(T + 0x34, T + 0x3C, T + 0x44);
		GteRTPT();
		NeedlePacketHeader(pk, T);
		GteReadFLAG(T + 8);
		if (*(uint32_t *)(T + 8) & 0x60000) return false;
		GteReadSXY012Split(pk + 2, pk + 4, pk + 6);
		GteLoadV0(T + 0x4C);
		GteRTPS();
		GteReadSXY2(pk + 8);
		InsertPrimAutoDepth(NeedleBucket(T), pk); // quad B reuses quad A's OTZ
		cursor += 0x28;
		return true;
	}

	struct NeedleMemo { uint32_t tick; int16_t pos[3]; int16_t age; uint32_t color; };
	static NeedleMemo g_needle_memo[800];

	static void SpawnNeedle(uint8_t *scratch, Needle *p)
	{
		p->alive = 1;
		p->age = 0;
		p->length = (int16_t)(CrtRand() % 0x1400 + 0xA00);
		int32_t j = CrtRand() % 0x200;
		memcpy(p->pos, AimTable() + 48 * j, 8);
		p->steps = (int16_t)(CrtRand() % 6 + 5);
		int32_t *v = (int32_t *)(scratch + 0x70);
		const uint8_t *rec = AimTable() + 48 * j;
		v[0] = CrtRand() % 0x640 + *(const int16_t *)(rec + 8) - p->pos[0] - 0x320;
		if (*(const int16_t *)(rec + 0xA) != 0)
			v[1] = CrtRand() % 0x640 + *(const int16_t *)(AimTable() + 48 * j + 0xA) - p->pos[1] - 0x320;
		else
			v[1] = -(int32_t)p->pos[1];
		v[2] = CrtRand() % 0x640 + *(const int16_t *)(AimTable() + 48 * j + 0xC) - p->pos[2] - 0x320;
		int32_t len = Sqrt(NormalizeVectorToFixedPoint(v, v));
		int32_t angle = GetRotationBetweenVectors((const void *)0xCF3578, v, (int32_t *)(scratch + 0x80));
		BuildAxisAngleRotationMatrix(angle, (Mat4x3 *)(scratch + 0x10), (const int32_t *)(scratch + 0x80));
		int32_t q = len / (int32_t)p->steps;
		p->vel[0] = (int16_t)(mul32(v[0], q) >> 12);
		p->vel[1] = (int16_t)(mul32(v[1], q) >> 12);
		p->vel[2] = (int16_t)(mul32(v[2], q) >> 12);
		int32_t off = *(const int16_t *)(AimTable() + 48 * j + 0xE);
		p->pos[0] = (int16_t)(p->pos[0] + (mul32(v[0], off) >> 12));
		p->pos[1] = (int16_t)(p->pos[1] + (mul32(v[1], off) >> 12));
		p->pos[2] = (int16_t)(p->pos[2] + (mul32(v[2], off) >> 12));
		p->steps++;
		p->fade = 0;
		p->aim = (int16_t)j;
		ResetRotation(scratch + 0x30);
		v[1] = 0xC00;
		v[0] = 0xC00;
		v[2] = p->length;
		Scale3DMatrix((Mat4x3 *)(scratch + 0x30), v);
		Matrix3x3MultiplyPSX(scratch + 0x10, scratch + 0x30, p->rot);
	}

	static uint32_t __cdecl NeedleTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = NeedlePool();
		uint8_t *scratch = (uint8_t *)FieldAlloc(0x90);
		if (!Paused() && n->c < 0x2D)
		{
			for (int k = 0; k < 50; k++)
			{
				int i = 0;
				while (i < 800 && *(uint32_t *)(pool + 0x3C * i) != 0) i++;
				if (i >= 800) break;
				SpawnNeedle(scratch, (Needle *)(pool + 0x3C * i));
			}
		}
		uint8_t *T = (uint8_t *)FieldAlloc(0x68);
		int32_t count = 0;
		NeedleTemplate(T);
		memcpy(scratch + 0x10, &Camera(), sizeof(Mat4x3));
		uint32_t cursor = Cursor();
		for (int i = 0; i < 800; i++)
		{
			Needle *p = (Needle *)(pool + 0x3C * i);
			if (!(p->alive & 1)) continue;
			p->pos32[0] = p->pos[0];
			p->pos32[1] = p->pos[1];
			p->pos32[2] = p->pos[2];
			NeedleProject(scratch, p->rot, p->pos32, T);
			int16_t age = p->age;
			*(uint32_t *)(T + 0x10) = age < 4 ? NEEDLE_FADE_IN[age] : *(uint32_t *)(T + 0x54);
			NeedleMemo &m = g_needle_memo[i];
			m.tick = g_real_tick;
			memcpy(m.pos, p->pos, 6);
			m.age = age;
			if (!Paused())
			{
				age = (int16_t)(age + 1);
				p->age = age;
				if (age < p->steps)
				{
					p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
					p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
					p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
				}
				else
				{
					int16_t f = p->fade;
					*(uint32_t *)(T + 0x10) = NEEDLE_FADE_OUT[f];
					f = (int16_t)(f + 1);
					p->fade = f;
					if (f >= 6) p->alive = 0;
				}
			}
			m.color = *(uint32_t *)(T + 0x10);
			if (NeedleEmit(T, cursor)) count++;
		}
		Cursor() = cursor;
		FieldFree(0x68);
		FieldFree(0x90);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Everything is drawn half way between the state drawn on the last
	// real tick and the state the next tick will draw; spawns, flipbook frames and the
	// random aim keep their 15 Hz batches. The frame matrix is rebuilt from the held-frame
	// camera while the timeline (its owner) is alive, into a local (never into 0x2259978).
	// ------------------------------------------------------------------
	static uint8_t g_skel_cur[SKEL_MAX];

	static void CreatureHeld(const Mat4x3 *frame, int32_t y, int num, int den)
	{
		CreatureMemo &m = g_creature;
		if (m.tick != g_real_tick) return;
		uint8_t *E = Creature();
		uint8_t *sk = Skeleton();
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		uint8_t cmd_cur[8];
		Mat4x3 mat_cur = CreatureMat();
		memcpy(g_skel_cur, sk, m.skel_size);
		memcpy(cmd_cur, E + 0x6C, 8);
		// the drawn pose (its values and its local matrices) and the command that read it
		memcpy(sk, m.skel, m.skel_size);
		memcpy(E + 0x6C, m.cmd, 8);
		if (m.midpoint) pose_midpoint(E + 0x60, E + 0x6C, num, den);
		CreatureY() = y;
		CreatureDraw(frame);
		CreatureMat() = mat_cur;
		memcpy(E + 0x6C, cmd_cur, 8);
		memcpy(sk, g_skel_cur, m.skel_size);
	}

	static void RisePrimHeld(const Mat4x3 *frame, const Node24 *n, int32_t y, int num, int den)
	{
		const PrimMemo *m = g_prim_memo.get(n);
		if (!m) return;
		int32_t f0 = 682 * (m->c - 6), f = f0;
		if (m->c >= 6 && n->c != m->c) f = lerp_i(f0, 682 * (n->c - 6), num, den);
		RisePrimDraw(frame, n, y, m->c >= 6, f);
	}

	static void ShrinkPrimHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const PrimMemo *m = g_prim_memo.get(n);
		if (!m) return;
		int32_t f0 = shl32(4 - m->c, 10), f = f0;
		if (m->c < 4 && n->c != m->c && n->c < 4) f = lerp_i(f0, shl32(4 - n->c, 10), num, den);
		ShrinkPrimDraw(frame, n, lerp_i(m->v, n->s, num, den), m->c < 4, f);
	}

	static void SpinPrimHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const PrimMemo *m = g_prim_memo.get(n);
		if (!m) return;
		bool fade = m->c < 8 || m->c >= 14;
		int32_t f = SpinFade(m->c);
		if (n->c != m->c && ((m->c < 8 && n->c < 8) || m->c >= 14))
			f = lerp_i(f, SpinFade(n->c), num, den);
		SpinPrimDraw(frame, n, lerp_angle(m->v, n->a, num, den), fade, f);
	}

	static void DustHeld(const Mat4x3 *frame, int num, int den)
	{
		uint8_t *pool = DustPool();
		uint8_t *scratch = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		memcpy(scratch + 8, frame, sizeof(Mat4x3));
		*(uint32_t *)h = 0xCF0740;
		*(uint16_t *)(h + 0x24) = 0;
		GteSetLightMatrix(scratch + 8);
		GteSetBackColorFromTrans(scratch + 8);
		for (int i = 0; i < 100; i++)
		{
			const DustMemo &m = g_dust_memo[i];
			if (m.tick != g_real_tick) continue;
			const DustPuff *p = (const DustPuff *)(pool + 0x1C * i);
			int16_t pos[4];
			memcpy(pos, m.pos, 8);
			if ((p->alive & 1) && p->age != m.age) // moved on the last tick: half way to where it is now
				for (int k = 0; k < 3; k++) pos[k] = (int16_t)lerp_i(m.pos[k], p->pos[k], num, den);
			DustDrawOne(h, pos, p->size, m.age);
		}
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static uint32_t LerpColor(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t r = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			r |= (uint32_t)(lerp_i((a >> k) & 0xFF, (b >> k) & 0xFF, num, den) & 0xFF) << k;
		return r;
	}

	static void NeedleHeld(int num, int den)
	{
		uint8_t *pool = NeedlePool();
		uint8_t *scratch = (uint8_t *)FieldAlloc(0x90);
		uint8_t *T = (uint8_t *)FieldAlloc(0x68);
		NeedleTemplate(T);
		memcpy(scratch + 0x10, &Camera(), sizeof(Mat4x3));
		uint32_t cursor = Cursor();
		for (int i = 0; i < 800; i++)
		{
			const NeedleMemo &m = g_needle_memo[i];
			if (m.tick != g_real_tick) continue;
			const Needle *p = (const Needle *)(pool + 0x3C * i);
			int32_t pos[3] = { m.pos[0], m.pos[1], m.pos[2] };
			uint32_t color = m.color;
			if ((p->alive & 1) && p->age != m.age)
			{
				// the colour the next tick draws: fade-in step, or the next fade-out step
				int16_t a = p->age;
				uint32_t next = a < 4 ? NEEDLE_FADE_IN[a] : *(uint32_t *)(T + 0x54);
				if ((int16_t)(a + 1) >= p->steps) next = NEEDLE_FADE_OUT[p->fade];
				color = LerpColor(m.color, next, num, den);
				for (int k = 0; k < 3; k++) pos[k] = lerp_i(m.pos[k], p->pos[k], num, den);
			}
			NeedleProject(scratch, p->rot, pos, T);
			*(uint32_t *)(T + 0x10) = color;
			NeedleEmit(T, cursor);
		}
		Cursor() = cursor;
		FieldFree(0x68);
		FieldFree(0x90);
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
		for (TaskNode *t = SubQueue().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TimelineTask) timeline = true;
		Mat4x3 frame;
		if (timeline) ComposeAffineTransform(&Camera(), &RootMatrix(), &frame);
		else frame = Frame();
		int32_t y = CreatureY();
		if (g_creature.tick == g_real_tick) y = lerp_i(g_creature.y_drawn, g_creature.y_next, num, den);
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			const Node24 *n = (const Node24 *)t;
			switch ((uint32_t)t->func)
			{
			case ORIG_CreatureTask: CreatureHeld(&frame, y, num, den); break;
			case ORIG_RisePrimTask: RisePrimHeld(&frame, n, y, num, den); break;
			case ORIG_ShrinkPrimTask: ShrinkPrimHeld(&frame, n, num, den); break;
			case ORIG_SpinPrimTask: SpinPrimHeld(&frame, n, num, den); break;
			case ORIG_DustTask: DustHeld(&frame, num, den); break;
			case ORIG_NeedleTask: NeedleHeld(num, den); break;
			}
		}
		Cursor() = cursor;
	}
}

	void register_mag199_cactuar()
	{
		register_port(c199::ORIG_SequenceTick, (void *)c199::SequenceTick, "C199 SequenceTick", 199);
		register_port(c199::ORIG_TimelineTask, (void *)c199::TimelineTask, "C199 TimelineTask", 199);
		register_port(c199::ORIG_CreatureTask, (void *)c199::CreatureTask, "C199 CreatureTask", 199, true);
		register_port(c199::ORIG_FlashInTask, (void *)c199::FlashInTask, "C199 FlashInTask", 199);
		register_port(c199::ORIG_FlashOutTask, (void *)c199::FlashOutTask, "C199 FlashOutTask", 199);
		register_port(c199::ORIG_RisePrimTask, (void *)c199::RisePrimTask, "C199 RisePrimTask", 199, true);
		register_port(c199::ORIG_DustTask, (void *)c199::DustTask, "C199 DustTask", 199, true);
		register_port(c199::ORIG_ShrinkPrimTask, (void *)c199::ShrinkPrimTask, "C199 ShrinkPrimTask", 199, true);
		register_port(c199::ORIG_SpinPrimTask, (void *)c199::SpinPrimTask, "C199 SpinPrimTask", 199, true);
		register_port(c199::ORIG_LauncherTask, (void *)c199::LauncherTask, "C199 LauncherTask", 199);
		register_port(c199::ORIG_NeedleTask, (void *)c199::NeedleTask, "C199 NeedleTask", 199, true);
		register_port(c199::ORIG_DamageTask, (void *)c199::DamageTask, "C199 DamageTask", 199);
		register_port(c199::ORIG_EndTask, (void *)c199::EndTask, "C199 EndTask", 199);
		register_module_held(199, c199::HeldReady, c199::HeldFrame);
	}
}
