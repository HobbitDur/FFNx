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

// Effect 191: Doomtrain - Runaway Train (timeline-B GF family, MAG_191_*), plus the shared
// camera-script interpreter MAG_066_sub_63E9C0 that lives in the same code file.
//
// Structure (gf_study/gf_inventory_timeline.md section 3.2; code 0x63E730-0x6472E0):
//   setup MAG_191_DOOMTRAIN_SUMMON_RUNAWAY_TRAIN (0x63E730, not ported: runs once) - model
//     buffer mb (dword 0x24FD3A0); record pools (0x18-byte records): A 0xE3C8C0 = mb+0x2E000
//     (170), B 0xE3C8C4 = mb+0x2F000 (170), C 0xE3C8C8 = mb+0x30000 (150), D 0xE3C8CC =
//     mb+0x31000 (120), E 0xE3C8D0 = mb+0x32000 (130), F 0xE3C8D4 = mb+0x33000 (590);
//     0xE3C8D8 = mb+0x37000 = vertex scratch (morph targets, model draw). Root queue 0x24FBF80
//     (pool 0x24FBF60) with the master, sub-queue 0x24FC330 (pool 0x24FC340, 100 x 0x24) with
//     the timeline; every other task goes into the sub-queue. Root matrix 0x24FBE88.
//   SequenceTick (master, 0x6472C0) - runs the sub-queue, ++counter, ends when it is empty.
//   TimelineTask (0x63F2D0) - 424 ticks: camera script, streams/loads, sounds, spawns, the
//     action result at 420; rebuilds Frame 0x24FBF90 = Camera o Root every tick.
//   CreatureTask (0x641EA0, spawned at 124) - 279 ticks, the train (state 0x24FC290), drawn
//     with a PRIVATE copy of the engine anim reader (0x642490 / 0x642520 / 0x642830, called
//     through their original addresses); writes the chimney bone matrix 0x24FBE58 each tick.
//   Every other task: see the ORIG_* list. Pause: the timeline tests 0x201 (with the load
//   state), every other Doomtrain task tests only bit 0 of battle_to_update_flags_dword_1D96A9C
//   (kept exactly), the master and the end task test nothing.
// Module-private renderers / readers called through their original addresses (pure functions
// of a scratch header or of the model state): the sky renderer 0x640830 (+ its primitive
// renderers 0x640910/0x640B70/0x640E20/0x641140), the private anim reader 0x642490 (SetAnim),
// 0x642520 (ReadAnimation), 0x642830 (BuildBoneMatricesFromPose), 0x642D80 (bones o model o frame).
//
// Camera-script interpreter (MAG_066): CameraScriptStart 0x63E960 queues task 0x63E9C0 into the
// caller's queue (Doomtrain 191, Cactuar 199, Shiva 185, Gilgamesh 327, Odin 187/326, MAG_218,
// MAG_200, MAG_164/165/076/066). Its state is global (tracks 0x24FBFB0 look-at / 0x24FD3A8 eye,
// roll sign 0x24FBE78, track pointers 0x24FD2D8 / 0x24FBDA4), so one instance runs at a time.
// The port is registered once (register_port keys on the original address only, so it serves
// every ported module that queues it); camscript_held_camera() predicts its next camera.

#include "fx_port.h"

namespace ff8fx
{
namespace d191
{
	using namespace eng;

	// --- engine functions used by this module (original addresses) ---
	namespace x
	{
		inline int32_t CrtRand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); }
		inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
		inline void ComposeZYXRotationMatrix(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x56CE30)(angles, out); }
		inline void RotMatrixXYZ(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x56CF50)(angles, out); }   // sub_56CF50: X, Y, Z rotations
		inline void Scale3DMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); }
		inline void UnpackRotationMatrix(const Mat4x3 *src, Mat4x3 *dst) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C040)(src, dst); } // dst.rot = transpose(src.rot)
		inline uint32_t MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { return fn<uint32_t (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector (rotation only)
		inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
		inline int32_t NormalizeQ12(const int32_t *in, int16_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int16_t *)>(0x56BD20)(in, out); } // Math_NormalizeVec3_Q12
		inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
		inline int32_t GetRotationBetweenVectors(const void *ref, const int32_t *v, int32_t *axis) { return fn<int32_t (__cdecl *)(const void *, const int32_t *, int32_t *)>(0x571480)(ref, v, axis); }
		inline void BuildAxisAngleRotationMatrix(int32_t angle, Mat4x3 *out, const int32_t *axis) { fn<void (__cdecl *)(int32_t, Mat4x3 *, const int32_t *)>(0x5714F0)(angle, out, axis); }
		inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); } // b.rot = a.rot * b.rot
		inline void CurveCoeffs(int32_t n, const void *pts, void *buf) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, pts, buf); }        // sub_571620: curve weights of n 8-byte points
		inline void CurveEval(int32_t n, void *buf, void *dst, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, buf, dst, t); } // sub_571690: point at t (0..0x1000), s16 x3
		inline void CalculateCenterPosition(uint32_t mask, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x5020A0)(mask, out); }
		inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
		inline uint32_t RenderGeometry(void *geom, void *hdr, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(geom, hdr, ot, shift, cursor); }
		inline void ProjectionH(int32_t h) { fn<void (__cdecl *)(int32_t)>(0x56CD00)(h); } // Call_Bs_parseCamera2
		// sound / streams / files / battle
		inline void BdPlaySE(uint32_t se, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(0x501330)(se, a, b); }
		inline void BdPlaySummonStream(int32_t a, int32_t b, int32_t c) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x5018C0)(a, b, c); }
		inline void BdTransSummonStream(uint32_t src, uint32_t state) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x501860)(src, state); }
		inline int32_t VoiceSlotBusy(uint32_t slot) { return fn<int32_t (__cdecl *)(uint32_t)>(0x4A2900)(slot); }
		inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
		inline void CameraShake(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x5712C0)(a, b, c, d); } // au_re_BdLinkTask_6
		inline void ApplyActionResultToTargets(uint32_t list, uint32_t count) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x506BA0)(list, count); }
		inline void WaitAnimSeq(void *buffer, void *flag) { fn<void (__cdecl *)(void *, void *)>(0x508630)(buffer, flag); } // sub_508630
		inline int32_t LoadState() { return fn<int32_t (__cdecl *)()>(0x508500)(); }                                       // sub_508500: < 0 = file load busy
		inline void CharacterLoad(int32_t id, uint32_t dst) { fn<void (__cdecl *)(int32_t, uint32_t)>(0x508480)(id, dst); } // BattleFile_CharacterLoad
		inline void QueueTIMUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(0x505E30)(tim); }                       // Battle_QueueTIMUpload_GetEOF
		inline uint32_t SummonData() { return fn<uint32_t (__cdecl *)()>(0x571B70)(); }                                   // sub_571B70
		// software GTE
		inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
		inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // MAG_148_sub_64DDA0
		inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }
		inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }        // MAG_163_sub_64DE00: R = s * I
		inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }                        // MAG_164_sub_64DE40: TR = MAC123
		inline void GteTransFromIR() { fn<void (__cdecl *)()>(0x64DE90)(); }                         // MAG_148_sub_64DE90: TR = IR123 (data regs 9-11)
		inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); } // MAG_163_sub_64DD70: TR = v
		inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
		inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); } // set_unk_1CA8A28
		inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                               // sub_45F270: depth cue towards the far colour by IR0
		inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }             // set_param_with_dword_1CA8A68
		inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
		inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
		inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
		inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
		inline void GteReadSZ3(void *dst) { fn<void (__cdecl *)(void *)>(0x45E220)(dst); }          // sub_45E220: dword 0x1CA8A5C
		// module-private functions called through their original addresses
		inline void ReaderSetAnim(void *hdr, void *cmd, int32_t id) { fn<void (__cdecl *)(void *, void *, int32_t)>(0x642490)(hdr, cmd, id); } // GF_191Doomtrain_SetAnim_PrivateReader
		inline int32_t ReaderRead(void *hdr, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(0x642520)(hdr, cmd); }              // private Battle_ReadAnimation (1 = finished)
		inline void ReaderBuildBones(void *hdr) { fn<void (__cdecl *)(void *)>(0x642830)(hdr); }                                              // private BuildBoneMatricesFromPose
		inline uint32_t ComposeBones(void *hdr, const Mat4x3 *frame, const Mat4x3 *model) { return fn<uint32_t (__cdecl *)(void *, const Mat4x3 *, const Mat4x3 *)>(0x642D80)(hdr, frame, model); } // bone = frame o (model o bone), z clamp mask
		inline uint32_t SkyRender(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x640830)(h, ot, mode, cursor); }
	}
	using namespace x;

	// --- module globals ---
	inline uint32_t Flags() { return var<uint32_t>(0x1D96A9C); }                 // battle_to_update_flags_dword_1D96A9C
	inline bool P1() { return (var<uint8_t>(0x1D96A9C) & 1) != 0; }            // the bit every Doomtrain task tests
	inline Mat4x3 &Root() { return var<Mat4x3>(0x24FBE88); }                    // effect frame (setup)
	inline Mat4x3 &Frame() { return var<Mat4x3>(0x24FBF90); }                   // Camera o Root, rebuilt by the timeline every tick
	inline Mat4x3 &BoneWorld() { return var<Mat4x3>(0x24FBE58); }               // creature model o bone 1 (chimney), written by the creature every tick
	inline TaskQueue &SubQueue() { return var<TaskQueue>(0x24FC330); }
	inline uint8_t *CastCtx() { return var<uint8_t *>(0x24FD258); }
	inline uint32_t MB() { return var<uint32_t>(0x24FD3A0); }                  // model buffer
	inline uint8_t *Creature() { return (uint8_t *)0x24FC290; }                 // +0x40 model matrix 0x24FC2D0, +0x60 BattleAnimHeader, +0x6C BattleAnimCmd
	inline Mat4x3 &CreatureMat() { return var<Mat4x3>(0x24FC2D0); }
	inline int32_t &CreatureZ() { return var<int32_t>(0x24FC2EC); }            // CreatureMat.t[2]
	inline uint32_t &StateFlags() { return var<uint32_t>(0x24FBE80); }         // bit0 creature done, bit1 wheels left (debris end); bit0 also = rain OT shift 0xE
	inline uint32_t &VoiceSlot() { return var<uint32_t>(0x24FC28C); }
	inline uint32_t &Cursor() { return var<uint32_t>(0x1D8E054); }            // battle_texture_data_ptr_1D8E054
	inline uint32_t OT() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline uint8_t *Pool(uint32_t cell) { return var<uint8_t *>(cell); }
	static const uint32_t POOL_A = 0xE3C8C0, POOL_B = 0xE3C8C4, POOL_C = 0xE3C8C8, POOL_D = 0xE3C8CC, POOL_E = 0xE3C8D0, POOL_F = 0xE3C8D4;
	inline uint8_t *Entity(uint32_t slot) { return (uint8_t *)(0x1D972C0 + 156 * slot); }
	inline int32_t *TargetList() { return (int32_t *)0x24FD298; }            // battle slots of the targets (+ linked parts)
	inline int32_t &TargetCount() { return var<int32_t>(0x24FD350); }
	inline Mat4x3 *RigW(int16_t e) { return (Mat4x3 *)(e ? 0x24FD418 : 0x24FD380); } // rig frame (Frame o local), per side
	inline Mat4x3 *RigM(int16_t e) { return (Mat4x3 *)(e ? 0x24FD360 : 0x24FD2B8); } // rig local matrix

	static const uint32_t ORIG_SequenceTick = 0x6472C0;
	static const uint32_t ORIG_TimelineTask = 0x63F2D0;
	static const uint32_t ORIG_CamScriptTask = 0x63E9C0;
	static const uint32_t ORIG_FlashInTask = 0x63F7D0;
	static const uint32_t ORIG_FlashOutTask = 0x63F880;
	static const uint32_t ORIG_RigTask = 0x63FB40;
	static const uint32_t ORIG_RigPrimATask = 0x63FDB0;
	static const uint32_t ORIG_RigPrimBTask = 0x63FEC0;
	static const uint32_t ORIG_RigRockTask = 0x63FFD0;
	static const uint32_t ORIG_RigFlashTask = 0x640140;
	static const uint32_t ORIG_RigSpriteTask = 0x640280;
	static const uint32_t ORIG_SkyTask = 0x640580;
	static const uint32_t ORIG_SteamPuffTask = 0x641580;
	static const uint32_t ORIG_SteamPairTask = 0x6418E0;
	static const uint32_t ORIG_RainTask = 0x641AF0;
	static const uint32_t ORIG_EndTask = 0x641E60;
	static const uint32_t ORIG_CreatureTask = 0x641EA0;
	static const uint32_t ORIG_WheelTrailTask = 0x642EF0;
	static const uint32_t ORIG_PuffLineTask = 0x643340;
	static const uint32_t ORIG_PuffWallTask = 0x6435B0;
	static const uint32_t ORIG_FlareTask = 0x643880;
	static const uint32_t ORIG_GlowTask = 0x643A00;
	static const uint32_t ORIG_ChimneyTask = 0x643D10;
	static const uint32_t ORIG_SteamTrailTask = 0x644030;
	static const uint32_t ORIG_SteamStillTask = 0x644530;
	static const uint32_t ORIG_SteamBurstTask = 0x644940;
	static const uint32_t ORIG_WheelSteamTask = 0x644E70;
	static const uint32_t ORIG_WheelStillTask = 0x645540;
	static const uint32_t ORIG_WheelBurstTask = 0x645AE0;
	static const uint32_t ORIG_DebrisSpawnTask = 0x646180;
	static const uint32_t ORIG_DebrisTask = 0x6462D0;
	static const uint32_t ORIG_ExplosionTask = 0x646640;
	static const uint32_t ORIG_KnockTask = 0x646D30;
	static const uint32_t ORIG_LandDustTask = 0x646F70;

	// real tick on which the ported master last ran (held frames need its memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;

#pragma pack(push, 1)
	// every sub-queue node is 0x24 bytes (pool 0x24FC340); field use differs per task
	struct Node24
	{
		TaskNode hdr;
		int16_t c;                  // +0x0C tick counter
		int16_t e;                  // +0x0E length / side / pool mask / battle slot / delay
		int16_t f10, f12, f14, f16; // +0x10 position (f16: pad, copied with the dwords)
		int16_t f18, f1A;           // +0x18
		int16_t f1C, f1E;           // +0x1C
		int16_t f20, f22;           // +0x20
	};
	// pool record (pools A..F)
	struct Rec
	{
		uint32_t mask;   // +0x00 owner bits (0 = free)
		int16_t age;     // +0x04 flipbook frame
		int16_t size;    // +0x06
		int16_t pos[4];  // +0x08 x, y, z, (pad, copied with the dwords)
		int16_t vel[3];  // +0x10
		int16_t w16;     // +0x16
	};
	// two-point history of the steam sources (0x24FC020 / 0x24FBEA8, 0x14 stride)
	struct Hist { uint32_t valid; uint32_t a0, a1, b0, b1; };
#pragma pack(pop)
	static_assert(sizeof(Node24) == 0x24, "sub-queue node is 0x24 bytes");
	static_assert(sizeof(Rec) == 0x18, "pool record is 0x18 bytes");
	static_assert(sizeof(Hist) == 0x14, "history entry is 0x14 bytes");

	inline Rec *R(uint32_t cell, int i) { return (Rec *)(Pool(cell) + 0x18 * i); }
	inline Hist *HistA(int k) { return (Hist *)(0x24FC020 + 0x14 * k); }
	inline Hist *HistB(int k) { return (Hist *)(0x24FBEA8 + 0x14 * k); }
	inline int16_t W16(uint32_t a) { return var<int16_t>(a); }

	// first free record (dword 0) of a pool, -1 when none
	static int FreeRec(uint32_t cell, int count)
	{
		uint8_t *p = Pool(cell);
		for (int i = 0; i < count; i++, p += 0x18)
			if (*(uint32_t *)p == 0) return i;
		return -1;
	}

	static int16_t L16(int16_t a, int16_t b, int num, int den) { return (int16_t)lerp_i(a, b, num, den); }

	// ======================================================================
	// Camera-script interpreter (MAG_066): CameraScriptStart 0x63E960, task 0x63E9C0
	// ======================================================================
#pragma pack(push, 1)
	struct CamTrack // 0x24FBFB0 (look-at) / 0x24FD3A8 (eye)
	{
		int16_t op;        // +0x00 current opcode (with the high byte of the script word)
		int16_t dur;       // +0x02 ticks left
		int32_t acc;       // +0x04 progress / distance / value range
		int32_t step;      // +0x08
		int32_t pad0C;
		int16_t pos[4];    // +0x10 current point (effect frame)
		int16_t start[4];  // +0x18
		int16_t delta[4];  // +0x20
		int16_t ease;      // +0x28 easing / start value
		int16_t npts;      // +0x2A curve points after the start point
		int16_t pts[16][4];// +0x2C curve points (pts[0] = start)
	};
	struct CamNode // node of the caller's queue (0x24 bytes in every user)
	{
		TaskNode hdr;
		int16_t c, e;              // +0x0C counter, +0x0E (0)
		uint32_t f10;
		const int16_t *script;     // +0x14
		const Mat4x3 *frame;       // +0x18 effect root matrix
		void *entity;              // +0x1C
	};
#pragma pack(pop)
	static_assert(sizeof(CamTrack) == 0xAC, "camera track is 0xAC bytes");
	static_assert(sizeof(CamNode) == 0x20, "camera-script node fields end at +0x20");

	inline CamTrack *TrackLook() { return (CamTrack *)0x24FBFB0; }
	inline CamTrack *TrackEye() { return (CamTrack *)0x24FD3A8; }
	inline CamTrack *&TrackCur() { return var<CamTrack *>(0x24FD2D8); }
	inline CamTrack *&TrackOther() { return var<CamTrack *>(0x24FBDA4); }
	inline int16_t &CamH() { return var<int16_t>(0x1D8E038); }      // projection distance (restored by the sky)
	inline int16_t &CamRoll() { return var<int16_t>(0x1D977A2); }   // g_BattleCam_Roll
	static const uint32_t CAM_EYE = 0xB8B7F0, CAM_AT = 0xB8B7F8;    // x, y, z, pad (s16)

	// MAG_066_sub_63F1E0: script argument, negative = variable -v-1 of the table 0x24FBF48
	static int32_t CsArg(const int16_t **pp)
	{
		const int16_t *p = *pp;
		int32_t v = p[0];
		*pp = p + 1;
		if (v < 0) v = *(const int16_t *)(uint32_t)(0x24FBF48 - (uint32_t)(2 * v + 2));
		return v;
	}

	// MAG_066_sub_63F210: script point, x mirrored by the roll sign
	static void CsVec(const int16_t **pp, int16_t *out)
	{
		const int16_t *p = *pp;
		out[0] = (int16_t)(p[0] * var<int16_t>(0x24FBE78));
		out[1] = p[1];
		out[2] = p[2];
		*pp = p + 3;
	}

	// MAG_066_sub_63F240: easing of t (0..0x1000); p > 0 ease-in (p >> 8 sine passes), p < 0
	// ease-out, blended with the linear t by (|p| & 0xFF) + 1 / 256
	static int32_t CsEase(int32_t t, int32_t p)
	{
		int32_t ecx = t, w = p;
		if (p > 0)
		{
			for (int32_t n = p >> 8; n > 0; n--) ecx = ComputeSin(ecx / 4);
		}
		else if (p < 0)
		{
			w = -p;
			int32_t eax = 0x1000 - ecx;
			for (int32_t n = w >> 8; n > 0; n--) eax = ComputeSin(eax / 4);
			ecx = 0x1000 - eax;
		}
		else return t;
		w = (w & 0xFF) + 1;
		return (mul32(0x100 - w, t) + mul32(w, ecx)) >> 8;
	}

	// MAG_066_sub_63E960 (native: the Doomtrain timeline calls it; the Cactuar / Shiva ports call
	// the original, both do the same)
	static void CameraScriptStart(uint32_t script, const Mat4x3 *frame, void *entity, TaskQueue *q, int32_t n)
	{
		CamNode *m = (CamNode *)AddTaskToQueue(q, ORIG_CamScriptTask);
		m->script = (const int16_t *)script;
		m->frame = frame;
		m->c = 0;
		m->e = 0;
		m->entity = entity;
		if (n != 0)
		{
			int32_t r = CrtRand() % n;
			var<int32_t>(0x24FBE78) = r != 1 ? 1 : -1;
		}
		else var<int32_t>(0x24FBE78) = 1;
	}

	// one tick of task 0x63E9C0 (the port and the held-frame predictor run this same code);
	// cut: set when a track jumps (opcodes 1 and 10)
	static uint32_t CsRun(CamNode *n, bool *cut)
	{
		uint8_t *S = (uint8_t *)FieldAlloc(0x1B0);
		int16_t *s16 = (int16_t *)S;
		CamTrack *T0 = TrackLook(), *T1 = TrackEye();
		if (!(var<uint8_t>(0x1D96A9C) & 1))
		{
			if (n->c == 0)
			{
				T0->dur = 0;
				T1->dur = 0;
			}
			// parse opcodes until both tracks have a duration
			while (T1->dur == 0 || T0->dur == 0)
			{
				const int16_t *p = n->script;
				uint16_t word = (uint16_t)p[0];
				n->script = p + 1;
				bool eye = (word & 0x40) != 0;
				CamTrack *cur = eye ? T1 : T0, *oth = eye ? T0 : T1;
				TrackCur() = cur;
				TrackOther() = oth;
				word = (uint16_t)(word & 0xFF3F);
				cur->op = (int16_t)word;
				int32_t op = (int16_t)word;
				if ((uint32_t)op > 10)
				{
					FieldFree(0x1B0);
					return TASK_END;
				}
				switch (op)
				{
				case 0: // wait
					cur->dur = (int16_t)CsArg(&n->script);
					break;
				case 1: // set point
					CsVec(&n->script, cur->pos);
					if (cut) *cut = true;
					break;
				case 2: // linear move
					cur->dur = (int16_t)CsArg(&n->script);
					CsVec(&n->script, s16);
					for (int i = 0; i < 3; i++) cur->delta[i] = (int16_t)(((int32_t)s16[i] - cur->pos[i]) / (int32_t)cur->dur);
					break;
				case 3: // eased move
				{
					cur->dur = (int16_t)CsArg(&n->script);
					const int16_t *q = n->script;
					cur->ease = q[0];
					n->script = q + 1;
					cur->acc = 0;
					cur->step = 0x1000 / (int32_t)cur->dur;
					for (int i = 0; i < 3; i++) cur->start[i] = cur->pos[i];
					CsVec(&n->script, s16);
					for (int i = 0; i < 3; i++) cur->delta[i] = (int16_t)(s16[i] - cur->pos[i]);
					break;
				}
				case 4: // eased curve through npts points
				{
					cur->dur = (int16_t)CsArg(&n->script);
					const int16_t *q = n->script;
					int32_t w = q[0];
					n->script = q + 1;
					cur->ease = (int16_t)(w & 0xFFFFFFF0);
					cur->npts = (int16_t)(w & 0xF);
					cur->acc = 0;
					cur->step = 0x1000 / (int32_t)cur->dur;
					memcpy(cur->pts[0], cur->pos, 8);
					for (int i = 1; i <= cur->npts; i++) CsVec(&n->script, cur->pts[i]);
					break;
				}
				case 5: // move keeping a changing distance from the other track
				{
					for (int i = 0; i < 3; i++) s16[i] = (int16_t)(oth->pos[i] - cur->pos[i]);
					uint32_t d2 = (uint32_t)mul32(s16[0], s16[0]) + (uint32_t)mul32(s16[1], s16[1]) + (uint32_t)mul32(s16[2], s16[2]);
					cur->acc = Sqrt((int32_t)d2);
					memcpy(cur->start, cur->pos, 8);
					cur->dur = (int16_t)CsArg(&n->script);
					CsVec(&n->script, s16);
					for (int i = 0; i < 3; i++) cur->delta[i] = (int16_t)(((int32_t)s16[i] - cur->pos[i]) / (int32_t)cur->dur);
					CamTrack *o = TrackOther();
					switch (o->op - 2)
					{
					case 0: // the other's end point: linear
					{
						int16_t k = cur->dur;
						for (int i = 0; i < 3; i++) s16[4 + i] = (int16_t)((int16_t)(o->delta[i] * k) + o->pos[i]);
						break;
					}
					case 1: // eased
						for (int i = 0; i < 3; i++) s16[4 + i] = (int16_t)(o->delta[i] + o->start[i]);
						break;
					case 2: // curve: vanilla reads THIS track's point count and points (0x63ECE4)
						memcpy(S + 8, cur->pts[cur->npts], 8);
						break;
					default:
						memcpy(S + 8, o->pos, 8);
						break;
					}
					for (int i = 0; i < 3; i++) s16[i] = (int16_t)(s16[i] - s16[4 + i]);
					d2 = (uint32_t)mul32(s16[0], s16[0]) + (uint32_t)mul32(s16[1], s16[1]) + (uint32_t)mul32(s16[2], s16[2]);
					int32_t len = Sqrt((int32_t)d2);
					cur->step = (len - cur->acc) / (int32_t)cur->dur;
					break;
				}
				case 6: // projection distance
				{
					const int16_t *q = n->script;
					CamH() = q[0];
					n->script = q + 1;
					break;
				}
				case 7: // projection distance ramp
				{
					cur->dur = (int16_t)CsArg(&n->script);
					int16_t h = CamH();
					const int16_t *q = n->script;
					cur->acc = (int32_t)q[0] - (int32_t)h;
					n->script = q + 1;
					cur->step = (int32_t)cur->dur;
					cur->ease = h;
					break;
				}
				case 8: // roll
				{
					const int16_t *q = n->script;
					CamRoll() = (int16_t)(var<int16_t>(0x24FBE78) * q[0]);
					n->script = q + 1;
					break;
				}
				case 9: // roll ramp
				{
					cur->dur = (int16_t)CsArg(&n->script);
					const int16_t *q = n->script;
					int16_t r = CamRoll();
					cur->acc = mul32(q[0], var<int32_t>(0x24FBE78)) - (int32_t)r;
					n->script = q + 1;
					cur->step = (int32_t)cur->dur;
					cur->ease = r;
					break;
				}
				case 10: // take the current battle camera point back into the effect frame
				{
					memcpy(cur->pos, (const void *)(eye ? CAM_EYE : CAM_AT), 8);
					Mat4x3 inv;
					UnpackRotationMatrix(n->frame, &inv);
					CamTrack *c2 = TrackCur();
					const Mat4x3 *f = n->frame;
					c2->pos[0] = (int16_t)(c2->pos[0] - (int16_t)f->t[0]);
					c2->pos[1] = (int16_t)(c2->pos[1] - (int16_t)f->t[1]);
					c2->pos[2] = (int16_t)(c2->pos[2] - (int16_t)f->t[2]);
					MatVec(&inv, c2->pos, c2->pos);
					if (cut) *cut = true;
					break;
				}
				}
			}
			// run both tracks one step: eye first, then look-at
			TrackCur() = T1;
			TrackOther() = T0;
			for (int it = 2; it; it--)
			{
				CamTrack *cur = TrackCur();
				switch (cur->op - 2)
				{
				case 0:
					for (int i = 0; i < 3; i++) cur->pos[i] = (int16_t)(cur->pos[i] + cur->delta[i]);
					break;
				case 1:
				{
					cur->acc += cur->step;
					int32_t e = CsEase(cur->acc, cur->ease);
					for (int i = 0; i < 3; i++) cur->pos[i] = (int16_t)((mul32(cur->delta[i], e) >> 12) + cur->start[i]);
					break;
				}
				case 2:
				{
					cur->acc += cur->step;
					int32_t e = CsEase(cur->acc, cur->ease);
					CurveCoeffs(cur->npts + 1, cur->pts, S + 0x20);
					CurveEval(cur->npts + 1, S + 0x20, cur->pos, e);
					break;
				}
				case 3:
				{
					CamTrack *o = TrackOther();
					for (int i = 0; i < 3; i++) cur->start[i] = (int16_t)(cur->start[i] + cur->delta[i]);
					cur->acc += cur->step;
					int32_t *v = (int32_t *)(S + 0x10);
					for (int i = 0; i < 3; i++) v[i] = (int32_t)o->pos[i] - (int32_t)cur->start[i];
					NormalizeQ12(v, s16);
					for (int i = 0; i < 3; i++) cur->pos[i] = (int16_t)(o->pos[i] - (int16_t)(mul32(s16[i], cur->acc) >> 12));
					break;
				}
				case 5:
					CamH() = (int16_t)(mul32(cur->step - cur->dur + 1, cur->acc) / cur->step + cur->ease);
					break;
				case 7:
					CamRoll() = (int16_t)(mul32(cur->step - cur->dur + 1, cur->acc) / cur->step + cur->ease);
					break;
				default:
					break;
				}
				cur->dur--;
				TrackCur() = T0;
				TrackOther() = T1;
			}
		}
		// the battle camera: saved (read by Cactuar, Odin-reverse, MAG_218), then the two track
		// points in world space
		var<uint32_t>(0x24FD35C) = var<uint32_t>(0xB8B7F4);
		var<uint32_t>(0x24FD250) = var<uint32_t>(0xB8B7F8);
		var<uint32_t>(0x24FD358) = var<uint32_t>(0xB8B7F0);
		var<uint32_t>(0x24FD254) = var<uint32_t>(0xB8B7FC);
		MatVec(n->frame, TrackEye()->pos, s16);
		for (int i = 0; i < 3; i++) s16[i] = (int16_t)(s16[i] + (int16_t)n->frame->t[i]);
		MatVec(n->frame, TrackLook()->pos, s16 + 4);
		for (int i = 0; i < 3; i++) s16[4 + i] = (int16_t)(s16[4 + i] + (int16_t)n->frame->t[i]);
		var<uint32_t>(0xB8B7F0) = *(uint32_t *)(S + 0);
		var<uint32_t>(0xB8B7F4) = *(uint32_t *)(S + 4);
		var<uint32_t>(0xB8B7F8) = *(uint32_t *)(S + 8);
		var<uint32_t>(0xB8B7FC) = *(uint32_t *)(S + 0xC);
		FieldFree(0x1B0);
		if (!(var<uint8_t>(0x1D96A9C) & 1)) n->c++;
		return 0;
	}

	// the node that ran on the last real tick (for the held-frame camera)
	static CamNode *g_cs_node = nullptr;
	static uint32_t g_cs_tick = 0xFFFFFFFF;

	static uint32_t __cdecl CamScriptTask(TaskNode *tn)
	{
		CamNode *n = (CamNode *)tn;
		uint32_t r = CsRun(n, nullptr);
		if (r == 0)
		{
			g_cs_node = n;
			g_cs_tick = g_real_tick;
		}
		else if (g_cs_node == n) g_cs_tick = 0xFFFFFFFF;
		return r;
	}

	// held-frame camera: the next tick of the script, run on the real globals and put back
	// (pure maths only: the parse / curve / easing helpers have no side effect)
	static bool CsHeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		CamNode *n = g_cs_node;
		if (!n || g_cs_tick != g_real_tick) return false;
		if (!(n->hdr.flags & 1) || (uint32_t)n->hdr.func != ORIG_CamScriptTask) return false;
		static uint8_t t0[sizeof(CamTrack)], t1[sizeof(CamTrack)], node[sizeof(CamNode)], cam[16], saved[16];
		memcpy(t0, TrackLook(), sizeof(t0));
		memcpy(t1, TrackEye(), sizeof(t1));
		memcpy(node, n, sizeof(node));
		memcpy(cam, (const void *)CAM_EYE, 16);
		memcpy(saved, (const void *)0x24FD250, 8);
		memcpy(saved + 8, (const void *)0x24FD358, 8);
		CamTrack *pc = TrackCur(), *po = TrackOther();
		int16_t h = CamH(), roll = CamRoll();
		bool cut = false;
		uint32_t r = CsRun(n, &cut);
		int16_t e1[3], a1[3];
		memcpy(e1, (const void *)CAM_EYE, 6);
		memcpy(a1, (const void *)CAM_AT, 6);
		memcpy(TrackLook(), t0, sizeof(t0));
		memcpy(TrackEye(), t1, sizeof(t1));
		memcpy(n, node, sizeof(node));
		memcpy((void *)CAM_EYE, cam, 16);
		memcpy((void *)0x24FD250, saved, 8);
		memcpy((void *)0x24FD358, saved + 8, 8);
		TrackCur() = pc;
		TrackOther() = po;
		CamH() = h;
		CamRoll() = roll;
		const int16_t *e0 = (const int16_t *)cam, *a0 = (const int16_t *)(cam + 8);
		bool move = r == 0 && !cut;
		for (int i = 0; i < 3 && move; i++)
		{
			int32_t de = (int32_t)e1[i] - e0[i], da = (int32_t)a1[i] - a0[i];
			if (de > 1500 || de < -1500 || da > 1500 || da < -1500) move = false; // a jump of its own: a cut
		}
		for (int i = 0; i < 3; i++)
		{
			world[i] = move ? (int16_t)lerp_i(e0[i], e1[i], num, den) : e0[i];
			lookat[i] = move ? (int16_t)lerp_i(a0[i], a1[i], num, den) : a0[i];
		}
		return true;
	}

	// ======================================================================
	// Held-frame memos
	// ======================================================================
	// node a task drew with (whole node, before its update)
	static NodeMemo<Node24, 256> g_node_memo;
	static void MemoNode(const Node24 *n) { if (Node24 *m = g_node_memo.put(n)) *m = *n; }
	// the node moved on by one tick since it was drawn (not a different node reusing the slot)
	static bool NodeMoved(const Node24 *m, const Node24 *n) { return n->c == (int16_t)(m->c + 1); }

	// pool records as drawn on the real tick (two owners per record: a few tasks draw the same
	// records of pool A, e.g. the wheel trail draws the puffs' records too)
	struct RecMemo { uint32_t tick; const void *owner; Rec r; };
	template<int N> struct PoolMemo
	{
		RecMemo m[N][2];
		void put(int i, const void *owner, const Rec *r)
		{
			RecMemo *x = &m[i][0];
			if (x->tick == g_real_tick && x->owner != owner) x = &m[i][1];
			x->tick = g_real_tick;
			x->owner = owner;
			x->r = *r;
		}
		const RecMemo *get(int i, int k, const void *owner) const
		{
			const RecMemo &x = m[i][k];
			return x.tick == g_real_tick && x.owner == owner ? &x : nullptr;
		}
	};
	static PoolMemo<170> g_memo_a, g_memo_b;
	static PoolMemo<150> g_memo_c;
	static PoolMemo<120> g_memo_d;
	static PoolMemo<130> g_memo_e;
	static PoolMemo<590> g_memo_f;
	// the record moved on to its next frame on this tick (not freed, not respawned)
	static bool Advanced(const Rec &m, const Rec *cur) { return cur->mask == m.mask && cur->age == (int16_t)(m.age + 1); }

	// ======================================================================
	// Master (0x6472C0): node from pool 0x24FBF60, +0x0C counter
	// ======================================================================
	static uint32_t __cdecl SequenceTick(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		g_ported_tick = g_real_tick;
		int left = ExecuteTaskQueue(&SubQueue());
		n->c++;
		return left ? 0 : TASK_END;
	}

	// ======================================================================
	// Small helpers (target list, battle-state flags, task links)
	// ======================================================================
	// au_re_BdLinkTask_78 0x63F760 (no null check, as the original)
	static Node24 *LinkTask(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), task_fn);
		n->c = 0;
		n->e = 0;
		return n;
	}
	static Node24 *AddTask(uint32_t task_fn) { return (Node24 *)AddTaskToQueue(&SubQueue(), task_fn); }

	// MAG_191_sub_63F780 / _63F830: stru_1D9898C[0..3].currentBsId |= 2 / &= ~2
	static void BsFlagSet() { for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) |= 2; }
	static void BsFlagClear() { for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) &= 0xFD; }

	// MAG_191_sub_63F9D0: target battle slots (castCtx list, no duplicates) + every linked part of a
	// target (entity ring at +0x8C, parts flagged 2), count at 0x24FD350
	static void BuildTargetList()
	{
		int32_t *list = TargetList();
		int32_t cnt = 0;
		uint8_t *t = *(uint8_t **)(CastCtx() + 4);
		int32_t n = t[0x10];
		if (n > 0)
		{
			uint8_t *rec = *(uint8_t **)(t + 8);
			for (int32_t k = n; k; k--, rec += 0x18)
			{
				uint32_t slot = rec[0];
				if (cnt > 0)
				{
					int32_t i = 0;
					while (i < cnt && (uint32_t)list[i] != slot) i++;
					if (i < cnt) continue;
				}
				list[cnt] = (int32_t)slot;
				cnt++;
				uint8_t *link = *(uint8_t **)(0x1D9734C + 156 * slot);
				uint8_t *self = Entity(slot);
				if (!link || link == self) continue;
				int32_t *out = &list[cnt];
				do
				{
					if (link[0] & 2)
					{
						int32_t j = 0;
						uint8_t *e = (uint8_t *)0x1D972C0;
						while (e != link)
						{
							e += 0x9C;
							j++;
							if (e >= (uint8_t *)0x1D97704) break;
						}
						if (e == link && e < (uint8_t *)0x1D97704 && j < 7)
						{
							*out++ = j;
							cnt++;
						}
					}
					link = *(uint8_t **)(link + 0x8C);
				} while (link != self);
			}
		}
		TargetCount() = cnt;
	}

	// MAG_191_sub_63F970: save the targets' positions (+0x1C) and scales (+0x94)
	static void SaveTargets()
	{
		for (int32_t i = 0; i < TargetCount(); i++)
		{
			int32_t slot = TargetList()[i];
			memcpy((void *)(0x24FD260 + 8 * slot), Entity(slot) + 0x1C, 8);
			memcpy((void *)(0x24FC0C8 + 8 * slot), Entity(slot) + 0x94, 8);
		}
	}
	// MAG_191_sub_642E40: put them back
	static void RestoreTargets()
	{
		for (int32_t i = 0; i < TargetCount(); i++)
		{
			int32_t slot = TargetList()[i];
			memcpy(Entity(slot) + 0x1C, (const void *)(0x24FD260 + 8 * slot), 8);
			memcpy(Entity(slot) + 0x94, (const void *)(0x24FC0C8 + 8 * slot), 8);
		}
	}
	// MAG_191_sub_642EA0: save the positions again and push the targets out of the train's way
	// (+0x20 low word += 0xA240)
	static void SaveTargetsAndMove()
	{
		for (int32_t i = 0; i < TargetCount(); i++)
		{
			int32_t slot = TargetList()[i];
			uint8_t *E = Entity(slot);
			*(uint32_t *)(0x24FD260 + 8 * slot) = *(uint32_t *)(E + 0x1C);
			uint32_t z = *(uint32_t *)(E + 0x20);
			*(int16_t *)(E + 0x20) = (int16_t)(*(int16_t *)(E + 0x20) + (int16_t)0xA240);
			*(uint32_t *)(0x24FD264 + 8 * slot) = z;
		}
	}
	// MAG_191_sub_63F930 / _63F8F0: entity flag 4 (hidden) set / cleared
	static void HideTargets() { for (int32_t i = 0; i < TargetCount(); i++) *(uint8_t *)Entity(TargetList()[i]) |= 4; }
	static void ShowTargets() { for (int32_t i = 0; i < TargetCount(); i++) *(uint16_t *)Entity(TargetList()[i]) &= 0xFFFB; }

	// ======================================================================
	// Timeline (0x63F2D0), 424 ticks. Load waits (sub_508500 < 0) repeat the tick.
	// ======================================================================
	static void SpawnRigs();
	static void SpawnSky();
	static void LinkFlash(uint32_t task_fn);
	static void LinkSteamPuff();
	static void LinkSteamPair();

	static uint32_t __cdecl TimelineTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		memcpy((void *)0x24FBDA8, &Camera(), sizeof(Mat4x3));
		ComposeAffineTransform(&Camera(), &Root(), &Frame());
		uint32_t fl = Flags();
		if (fl & 0x201)
		{
			if (fl & 1) return 0;
			if (LoadState() < 0) return 0;
		}
		if (n->c == 0)
		{
			CameraScriptStart(0xE3CA88, &Root(), Creature(), &SubQueue(), 0);
			BuildTargetList();
			SaveTargets();
			BdTransSummonStream(0xF78380, 0x24FC280);
			VoiceSlot() = ClaimVoiceSlot((const void *)0xE3C774, 1, 0x80);
		}
		switch (n->c)
		{
		case 1: SpawnRigs(); break;
		case 4: BdPlaySE(0xE3C8A8, 0, 0x80); break;
		case 5: CharacterLoad(0x22B, MB() + 0x10000); break;
		case 15:
			if (LoadState() < 0) return 0;
			QueueTIMUpload(MB() + 0x10000);
			CharacterLoad(0x22C, MB());
			LinkFlash(ORIG_FlashInTask);
			break;
		case 20:
			SpawnSky();
			LinkTask(ORIG_RainTask);
			break;
		case 30:
			if (LoadState() < 0) return 0;
			CharacterLoad(0x22D, MB() + 0x10000);
			break;
		case 85:
			if (LoadState() < 0) return 0;
			CharacterLoad(0x22E, SummonData());
			LinkSteamPuff();
			break;
		case 95: case 178: case 313: case 355:
			if (LoadState() < 0) return 0;
			BdTransSummonStream(SummonData(), 0x24FC280);
			break;
		case 103:
			HideTargets();
			LinkSteamPair();
			break;
		case 124:
			BdPlaySummonStream(0x80, 0, 0x60);
			LinkTask(ORIG_CreatureTask);
			break;
		case 160: BdPlaySE(0xE3C8AC, 0, 0x80); break;
		case 168: CharacterLoad(0x22F, SummonData()); break;
		case 293: CharacterLoad(0x230, SummonData()); break;
		case 294:
			BdPlaySE(0xE3C8B0, 0, 0x80);
			BdPlaySummonStream(0x80, 0, 0x60);
			break;
		case 335:
			BdPlaySummonStream(0x80, 0, 0x60);
			CharacterLoad(0x231, SummonData());
			break;
		case 336: BdPlaySE(0xE3C8B4, 0, 0x80); break;
		case 366: BdPlaySummonStream(0x80, 0, 0x60); break;
		case 405:
			LinkFlash(ORIG_FlashOutTask);
			QueueTIMUpload(MB() + 0x10000);
			break;
		case 406: LinkTask(ORIG_EndTask); break;
		case 417: BdPlaySE(0xE3C8B8, 0, 0x80); break;
		case 420:
			if (VoiceSlotBusy(VoiceSlot())) ReleaseVoiceSlot(VoiceSlot());
			break;
		}
		if (n->c == 0x1A4)
		{
			uint8_t *t = *(uint8_t **)(CastCtx() + 4);
			ApplyActionResultToTargets(*(uint32_t *)(t + 8), t[0x10]);
		}
		n->c++;
		if (n->c <= 0x1A7) return 0;
		BsFlagSet();
		ShowTargets();
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ======================================================================
	// Screen flash in (0x63F7D0) / out (0x63F880): au_re_BdLinkTask_79 / _80, level 0x1000
	// over 16 ticks; +0x1A: also toggle the battle-state flag 2 (in: clear at the end, out: set
	// on its first tick)
	// ======================================================================
	static void LinkFlash(uint32_t task_fn)
	{
		Node24 *n = AddTask(task_fn);
		n->c = 0;
		n->e = 0x10;
		n->f18 = 0x1000;
		n->f1A = 1;
	}

	static uint32_t __cdecl FlashInTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		SetScreenFlash((uint32_t)mul32((int32_t)n->f18 / (int32_t)n->e, n->c), 0);
		if (P1()) return 0;
		n->c++;
		if (n->c < n->e) return 0;
		if (n->f1A) BsFlagClear();
		return TASK_END;
	}

	static uint32_t __cdecl FlashOutTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t level = n->f18;
		SetScreenFlash((uint32_t)(level - mul32(level / (int32_t)n->e, n->c)), 0);
		if (n->c == 0 && n->f1A) BsFlagSet();
		if (P1()) return 0;
		n->c++;
		if (n->c < n->e) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ======================================================================
	// End (0x641E60): waits for the anim-seq task queued at tick 1 to clear +0x0E
	// ======================================================================
	static uint32_t __cdecl EndTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 1) WaitAnimSeq((void *)(MB() + 0x10000), &n->e);
		uint16_t done = (uint16_t)n->e;
		n->c++;
		return done ? TASK_END : 0;
	}

	// ======================================================================
	// Rails / rig (timeline tick 1): two RigTask nodes (e = side 0 / 1), each builds its local
	// matrix M (0x24FD2B8 / 0x24FD360) on its first tick, spawns four prim tasks that draw in its
	// frame W = Frame o M (0x24FD380 / 0x24FD418, rebuilt every tick), and on side 0 a flash
	// every 7 ticks. Children: +0x0E side, +0x10 position, +0x16 length, +0x1C (+0x20) scale.
	// ======================================================================
	// MAG_191_sub_63FAA0
	static void SpawnRigs()
	{
		Node24 *a = AddTask(ORIG_RigTask);
		a->c = 0;
		a->e = 0;
		a->f10 = 0xC80;
		a->f12 = 0;
		a->f14 = (int16_t)0xF380;
		a->f16 = 0x67;
		a->f18 = (int16_t)0xFC00;
		a->f1A = 1;
		a->f1C = 0x1000;
		a->f20 = 1;
		Node24 *b = AddTask(ORIG_RigTask);
		b->e = 1;
		b->f1A = 1;
		b->c = 0;
		b->f12 = 0;
		b->f20 = 0;
		b->f10 = (int16_t)0xF380;
		b->f14 = 0xC80;
		b->f16 = 0x67;
		b->f18 = 0x400;
		b->f1C = 0x1000;
	}

	static uint32_t __cdecl RigTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		Mat4x3 *M = RigM(n->e), *W = RigW(n->e);
		if (!P1())
		{
			if (n->c == 0)
			{
				int16_t ang[3] = { 0, n->f18, 0 };
				ComposeZYXRotationMatrix(ang, M);
				M->t[0] = n->f10;
				M->t[1] = n->f12;
				M->t[2] = n->f14;
				int32_t s = n->f1C;
				int32_t v[3] = { s, s, s };
				Scale3DMatrix(M, v);
				Node24 *a = AddTask(ORIG_RigPrimATask);
				a->c = 0; a->e = n->e; a->f10 = 0; a->f12 = 0; a->f14 = 0; a->f16 = n->f16; a->f1C = 0x800;
				if (n->f20)
				{
					Node24 *b = AddTask(ORIG_RigPrimBTask);
					b->c = 0; b->e = n->e; b->f10 = 0; b->f12 = 0; b->f14 = 0; b->f16 = n->f16; b->f1C = 0x800;
				}
				Node24 *r = AddTask(ORIG_RigRockTask);
				r->c = 0; r->e = n->e; r->f10 = 0x208; r->f12 = (int16_t)0xFDDA; r->f14 = 0x1CC; r->f16 = n->f16; r->f1C = 0x600; r->f20 = 0x600;
				if (n->f1A) { r->f18 = (int16_t)0xFC00; r->f1A = (int16_t)0xFFCD; }
				else { r->f18 = 0; r->f1A = 0; }
				if (n->f20)
				{
					Node24 *s2 = AddTask(ORIG_RigSpriteTask);
					s2->c = 0; s2->e = n->e; s2->f10 = 0; s2->f12 = (int16_t)0xFD30; s2->f14 = (int16_t)0xFF4C; s2->f16 = n->f16; s2->f1C = 0x600;
				}
			}
			int32_t c = n->c;
			if (c % 7 == 0 && n->f20 && n->c < 0x50)
			{
				int16_t x = ((c / 7) & 1) ? 0xB4 : -0xB4;
				Node24 *f = AddTask(ORIG_RigFlashTask);
				f->c = 0; f->e = n->e; f->f10 = x; f->f12 = (int16_t)0xFB64; f->f14 = (int16_t)0xFF42; f->f1C = 0x780;
				BdPlaySummonStream(0x80, 0, 0x60);
			}
			n->c++;
		}
		ComposeAffineTransform(&Frame(), M, W);
		return n->c > n->f16 ? TASK_END : 0;
	}

	// local matrix of a rig child: rotation, position, scale, in W; GTE rotation + translation
	static void RigMatrix(const Mat4x3 *W, const int16_t *ang, const Node24 *n, int32_t sx, int32_t sy, int32_t sz)
	{
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->f10;
		m.t[1] = n->f12;
		m.t[2] = n->f14;
		int32_t v[3] = { sx, sy, sz };
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(W, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	// Effect_RenderPrimModel with the 0x58 header (fields as vanilla writes them)
	static void PrimModel(uint32_t model, int32_t mode, uint32_t h1C)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = h1C;
		Cursor() = RenderPrimModel(h, OT(), mode, Cursor());
		FieldFree(0x58);
	}

	// 0x63FDB0: prim 0xE375BC (mode 2, 0x30), c < length
	static void RigPrimADraw(const Mat4x3 *W, const Node24 *n)
	{
		int16_t ang[3] = { 0, 0, 0 };
		RigMatrix(W, ang, n, n->f1C, n->f1C, n->f1C);
		PrimModel(0xE375BC, 2, 0x30);
	}
	static uint32_t __cdecl RigPrimATask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RigPrimADraw(RigW(n->e), n);
		MemoNode(n);
		if (P1()) return 0;
		n->c++;
		return n->c >= n->f16 ? TASK_END : 0;
	}

	// 0x63FEC0: prim 0xE38E78 (mode 3, 0)
	static void RigPrimBDraw(const Mat4x3 *W, const Node24 *n)
	{
		int16_t ang[3] = { 0, 0, 0 };
		RigMatrix(W, ang, n, n->f1C, n->f1C, n->f1C);
		PrimModel(0xE38E78, 3, 0);
	}
	static uint32_t __cdecl RigPrimBTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RigPrimBDraw(RigW(n->e), n);
		MemoNode(n);
		if (P1()) return 0;
		n->c++;
		return n->c >= n->f16 ? TASK_END : 0;
	}

	// 0x63FFD0: prim 0xE396B0 rolled by +0x18 (tilts by -+0x1A over 25..44, then rocks back with
	// a damped sine 45..50, then level)
	static void RigRockDraw(const Mat4x3 *W, const Node24 *n, int16_t roll)
	{
		int16_t ang[3] = { 0, 0, roll };
		RigMatrix(W, ang, n, n->f1C, n->f20, n->f20);
		PrimModel(0xE396B0, 2, 0);
	}
	static uint32_t __cdecl RigRockTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RigRockDraw(RigW(n->e), n, n->f18);
		MemoNode(n);
		if (P1()) return 0;
		int16_t c = n->c;
		int32_t k = c;
		if (k >= 0x19)
		{
			k -= 0x19;
			if (k < 0x14) n->f18 = (int16_t)(n->f18 - n->f1A);
			else if ((k -= 0x14) < 6)
			{
				int32_t s = ComputeSin(shl32(k, 10) / 6 + 0x400);
				int32_t v = mul32(s, 20) >> 12;
				if (!(c & 1)) v = -v;
				n->f18 = (int16_t)(n->f18 + v);
			}
			else n->f18 = 0;
		}
		n->c = (int16_t)(c + 1);
		return n->c >= n->f16 ? TASK_END : 0;
	}

	// 0x640140: flash prim 0xE3B83C (mode 4), faded out from tick 2 (682 per tick), 8 ticks
	static int32_t RigFlashFade(int32_t c) { return mul32(c - 2, 682); }
	static void RigFlashDraw(const Mat4x3 *W, const Node24 *n, int16_t c, bool held, int32_t fade)
	{
		int16_t ang[3] = { 0, 0, 0 };
		RigMatrix(W, ang, n, n->f1C, n->f1C, n->f1C);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = 0xE3B83C;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 3;
		if (c == 0)
		{
			*(uint32_t *)(h + 0xC) = 0x800;
			*(uint32_t *)(h + 0x1C) = 0xC3;
		}
		else if (c >= 2)
		{
			*(int32_t *)(h + 0xC) = held ? fade : RigFlashFade(c);
			*(uint32_t *)(h + 0x1C) = 0xC3;
		}
		Cursor() = RenderPrimModel(h, OT(), 4, Cursor());
		FieldFree(0x58);
	}
	static uint32_t __cdecl RigFlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RigFlashDraw(RigW(n->e), n, n->c, false, 0);
		MemoNode(n);
		if (P1()) return 0;
		n->c++;
		return n->c >= 8 ? TASK_END : 0;
	}

	// 0x640280: flipbook 0xE346B0 (mode 3), frame = counter & 7
	static void RigSpriteDraw(const Mat4x3 *W, const Node24 *n, int16_t c)
	{
		int16_t ang[3] = { 0, 0, 0 };
		RigMatrix(W, ang, n, n->f1C, n->f1C, n->f1C);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint16_t *)(h + 4) = (uint16_t)(c & 7);
		*(uint32_t *)h = 0xE346B0;
		*(uint16_t *)(h + 0x24) = 8;
		Cursor() = InitEffectSequenceFromData(h, OT(), 3, Cursor());
		FieldFree(0xB4);
	}
	static uint32_t __cdecl RigSpriteTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RigSpriteDraw(RigW(n->e), n, n->c);
		MemoNode(n);
		if (P1()) return 0;
		n->c++;
		return n->c >= n->f16 ? TASK_END : 0;
	}

	// ======================================================================
	// Sky (timeline tick 20, MAG_191_sub_640390 spawns two): a morphing camera-space dome
	// (vertices 0xE34974 -> 0xE3702C blended by the phase +0x20 into mb+0x37000), rotation
	// +0x18/+0x10/+0x14 (x and z spin by +0x12/+0x16), colour table 0xE3C8DC[+0x1E & 31]
	// depth-cued by the phase, fade in 0..31 / out from 372, drawn by the private renderer
	// 0x640830 into the OT at +0x4078 with projection 0x120; 388 ticks.
	// ======================================================================
	static int16_t RandMod(int32_t m) { return (int16_t)(CrtRand() % m); }

	static void SpawnSky()
	{
		Node24 *n = AddTask(ORIG_SkyTask);
		n->c = 0;
		n->f10 = RandMod(0x1000);
		n->f12 = (int16_t)(CrtRand() % 8 + 4);
		if (n->f10 & 1) n->f12 = (int16_t)-n->f12;
		n->f14 = RandMod(0x1000);
		n->f16 = (int16_t)(CrtRand() % 4 + 2);
		if (n->f14 & 1) n->f16 = (int16_t)-n->f16;
		n->f18 = RandMod(0x1000);
		n->f1A = 1;
		n->f1C = 0xB80;
		n->f1E = RandMod(0x20);
		n->f20 = RandMod(0x1000);
		n->f22 = (int16_t)(CrtRand() % 0x18 + 0x24);
		n = AddTask(ORIG_SkyTask);
		n->c = 0;
		n->f10 = RandMod(0x1000);
		n->f12 = (int16_t)(CrtRand() % 8 + 4);
		if (n->f10 & 1) n->f12 = (int16_t)-n->f12;
		n->f14 = RandMod(0x1000);
		n->f16 = (int16_t)(CrtRand() % 4 + 2);
		if (n->f14 & 1) n->f16 = (int16_t)-n->f16;
		n->f18 = RandMod(0x1000);
		n->f1A = 1;
		n->f1C = 0xC00;
		n->f1E = RandMod(0x20);
		n->f20 = RandMod(0x800);
		n->f22 = (int16_t)(CrtRand() % 0x28 + 0x3C);
	}

	static int32_t SkyFade(int32_t c)
	{
		if (c < 0x20) return shl32(0x20 - c, 7);
		if (c >= 0x174) return shl32(c - 0x174, 8);
		return 0;
	}

	static void SkyDraw(int16_t ay, int16_t ax, int16_t az, int16_t f1A, int16_t scale, int16_t idx, int16_t phase, int32_t fade)
	{
		int16_t ang[3] = { ay, ax, az };
		Mat4x3 m;
		RotMatrixXYZ(ang, &m);
		int32_t v[3] = { scale, scale, scale };
		m.t[0] = 0;
		m.t[1] = 0;
		m.t[2] = 0x120;
		Scale3DMatrix(&m, v);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
		uint32_t mode = f1A ? 3 : 0xC;
		*(uint32_t *)h = 0xE3496C;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = mode | 0x20C0;
		*(uint32_t *)(h + 0x20) = ((const uint32_t *)0xE3C8DC)[idx & 0x1F];
		*(int32_t *)(h + 0xC) = (ComputeSin(phase) + 0x1000) / 2;
		GteSetFarColor(0, 0, 0);
		GteLoadRGBC(h + 0x20);
		GteSetIR0(*(int32_t *)(h + 0xC));
		GteDPCS();
		GteStoreRGB2(h + 0x20);
		*(int32_t *)(h + 0xC) = fade;
		int32_t t = (ComputeSin((int32_t)phase >> 2) + 0x1000) / 2;
		int32_t count = var<int32_t>(0xE34970);
		int16_t *out = (int16_t *)(MB() + 0x37000);
		const int16_t *a = (const int16_t *)0xE34974, *b = (const int16_t *)0xE3702C;
		for (int32_t i = 0; i < count; i++, a += 4, b += 4, out += 4)
		{
			out[0] = (int16_t)((mul32(b[0] - a[0], t) >> 12) + a[0]);
			out[1] = (int16_t)((mul32(b[1] - a[1], t) >> 12) + a[1]);
			out[2] = (int16_t)((mul32(b[2] - a[2], t) >> 12) + a[2]);
		}
		*(uint32_t *)(h + 4) = MB() + 0x37000;
		ProjectionH(0x120);
		Cursor() = SkyRender(h, var<uint32_t>(0x1D8E04C) + 0x4078, 6, Cursor());
		ProjectionH(CamH());
		FieldFree(0x6C);
	}

	static uint32_t __cdecl SkyTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		SkyDraw(n->f18, n->f10, n->f14, n->f1A, n->f1C, n->f1E, n->f20, SkyFade(n->c));
		MemoNode(n);
		if (P1()) return 0;
		n->f10 = (int16_t)(n->f10 + n->f12);
		n->f14 = (int16_t)(n->f14 + n->f16);
		n->f1E++;
		n->f20 = (int16_t)(n->f20 + n->f22);
		n->c++;
		return n->c >= 0x184 ? TASK_END : 0;
	}

	// ======================================================================
	// Lit sprites of the record pools. Two draw forms:
	//   lit:    V0 = record, IR = LightMatrix * V0 + BK (the effect frame), R = size * I,
	//           TR = MAC (or IR), flipbook header at +4 = frame
	//   offset: the same, then TR = MAC pushed towards the eye by size >> shift along the
	//           normalised MAC (so big puffs do not cut into the geometry)
	// ======================================================================
	// light matrix = frame o (zero rotation, translation t) in scratch s + 8 (0x48 bytes)
	static void LitFrame(uint8_t *s, const Mat4x3 *pre, const Mat4x3 *frame, int32_t tx, int32_t ty, int32_t tz)
	{
		*(int16_t *)s = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = tx;
		*(int32_t *)(s + 0x20) = ty;
		*(int32_t *)(s + 0x24) = tz;
		if (pre) ComposeAffineTransform(pre, (Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		ComposeAffineTransform(frame, (Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static void LitSprite(uint8_t *h, const int16_t *pos, int16_t size, int16_t frame, bool ir)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int16_t *)(h + 4) = frame;
		if (ir) GteTransFromIR();
		else GteTransFromMAC();
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static void OffsetSprite(uint8_t *h, int32_t *mac, int32_t *nv, const int16_t *pos, int16_t size, int16_t frame, int shift)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int16_t *)(h + 4) = frame;
		GteReadMAC123(mac);
		Normalize(mac, nv);
		int32_t k = -((int32_t)size >> shift);
		mac[0] += mul32(nv[0], k) >> 12;
		mac[1] += mul32(nv[1], k) >> 12;
		mac[2] += mul32(nv[2], k) >> 12;
		GteSetTransFromVec32(mac);
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	// ======================================================================
	// Steam puffs (timeline 85, au_re_BdLinkTask_81) and pairs (timeline 103, _82): flipbook
	// 0xE33E40 puffs of pool A tagged with the node's +0x0E (1 / 2), looping flipbooks.
	// Puffs: one pair per tick left and right of a point that drifts along a sine (+0x18), 18
	// ticks, radius +0x1C growing by +0x20 (-1 per tick). Pairs: two pairs per tick 900 either
	// side, +0x14 advancing 1500, 21 spawn ticks, 90 ticks. Both clear their records at the end.
	// ======================================================================
	static void LinkSteamPuff()
	{
		Node24 *n = AddTask(ORIG_SteamPuffTask);
		n->c = 0;
		n->e = 1;
		n->f10 = 0;
		n->f12 = 0;
		n->f14 = (int16_t)0xF830;
		n->f18 = 0;
		n->f1C = 0x7D0;
		n->f1E = 0x1000;
		n->f20 = (int16_t)0xFFA6;
	}
	static void LinkSteamPair()
	{
		Node24 *n = AddTask(ORIG_SteamPairTask);
		n->c = 0;
		n->e = 2;
		n->f10 = 0;
		n->f12 = (int16_t)0xFF38;
		n->f14 = (int16_t)0x8AD0;
		n->f1E = 0x800;
	}

	// draw loop of the puff tasks: records with (mask & node +0x0E), looping flipbook
	static void PuffLoop(const Node24 *n, bool ir)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE33E40;
		*(uint16_t *)(h + 0x24) = 8;
		LitFrame(s, nullptr, &Frame(), 0, 0, 0);
		for (int i = 0; i < 170; i++)
		{
			Rec *r = R(POOL_A, i);
			if (!(r->mask & (uint32_t)(int32_t)n->e)) continue;
			LitSprite(h, r->pos, r->size, r->age, ir);
			g_memo_a.put(i, n, r);
			if (P1()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0) r->age = 0;
		}
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void ClearMask(uint32_t cell, int count, uint32_t mask, bool dword_test)
	{
		uint8_t *p = Pool(cell);
		for (int i = 0; i < count; i++, p += 0x18)
			if (dword_test ? (*(uint32_t *)p & mask) != 0 : (p[0] & mask) != 0) *(uint32_t *)p = 0;
	}

	static uint32_t __cdecl SteamPuffTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		PuffLoop(n, true);
		if (P1()) return 0;
		FieldAlloc(0x48);
		if (n->c <= 0x12)
		{
			for (int k = 0; k < 1; k++)
			{
				int i = FreeRec(POOL_A, 170);
				if (i < 0) break;
				int32_t s0 = ComputeSin(n->f18);
				int32_t ang = mul32(s0, 180) >> 12;
				uint32_t e = (uint32_t)(int32_t)n->e;
				Rec *a = R(POOL_A, i);
				a->mask = e;
				a->age = 0;
				a->size = n->f1E;
				memcpy(a->pos, &n->f10, 8);
				int32_t t1 = ang - 0x400;
				int32_t sa = ComputeSin(t1);
				a->pos[0] = (int16_t)(a->pos[0] + (mul32(sa, n->f1C) >> 12));
				int32_t ca = ComputeCos(t1);
				a->pos[2] = (int16_t)(a->pos[2] + (mul32(ca, n->f1C) >> 12));
				Rec *b = R(POOL_A, i + 1); // the next record, used or not (vanilla)
				b->mask = e;
				b->age = 0;
				b->size = n->f1E;
				memcpy(b->pos, &n->f10, 8);
				int32_t t2 = ang + 0x400;
				int32_t sb = ComputeSin(t2);
				b->pos[0] = (int16_t)(b->pos[0] + (mul32(sb, n->f1C) >> 12));
				int32_t cb = ComputeCos(t2);
				b->pos[2] = (int16_t)(b->pos[2] + (mul32(cb, n->f1C) >> 12));
				int32_t sd = ComputeSin(ang);
				n->f10 = (int16_t)(n->f10 + (mul32(sd, 2000) >> 12));
				n->f12 = (int16_t)(n->f12 - 0x14);
				int32_t cd = ComputeCos(ang);
				n->f18 = (int16_t)(n->f18 + 0xA0);
				n->f1E = (int16_t)(n->f1E - 0x58);
				n->f14 = (int16_t)(n->f14 + (mul32(cd, 2000) >> 12));
				int16_t g = n->f20;
				n->f1C = (int16_t)(n->f1C + g);
				n->f20 = (int16_t)(g - 1);
			}
		}
		FieldFree(0x48);
		n->c++;
		if (n->c < 0x12) return 0;
		ClearMask(POOL_A, 170, (uint32_t)(int32_t)n->e, true);
		return TASK_END;
	}

	static uint32_t __cdecl SteamPairTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		PuffLoop(n, false);
		if (P1()) return 0;
		FieldAlloc(0x48);
		if (n->c <= 0x14)
		{
			for (int k = 0; k < 2; k++)
			{
				int i = FreeRec(POOL_A, 170);
				if (i < 0) break;
				uint32_t e = (uint32_t)(int32_t)n->e;
				int16_t sz = n->f1E;
				Rec *a = R(POOL_A, i);
				a->mask = e;
				a->age = 0;
				a->size = sz;
				memcpy(a->pos, &n->f10, 8);
				a->pos[0] = (int16_t)(a->pos[0] - 900);
				Rec *b = R(POOL_A, i + 1);
				b->mask = e;
				b->age = 0;
				b->size = sz;
				memcpy(b->pos, &n->f10, 4);
				memcpy(&b->pos[2], &n->f14, 4);
				n->f14 = (int16_t)(n->f14 + 0x5DC);
				b->pos[0] = (int16_t)(b->pos[0] + 900);
				n->f1E = (int16_t)(sz - 0x10);
			}
		}
		FieldFree(0x48);
		n->c++;
		if (n->c < 0x5A) return 0;
		ClearMask(POOL_A, 170, (uint32_t)(int32_t)n->e, true);
		return TASK_END;
	}

	// ======================================================================
	// Rain / cinders (timeline 20, until 308 and the last one is gone): pool F, 28 new per tick
	// in a 40000 x 20000 x 40000 box, drifting by a random velocity for 64 ticks; one textured
	// quad each (packet 9 words, colour table 0xE3C988[frame]) through the camera matrix;
	// OT shift 0xE once the creature is done (0x24FBE80 bit 0), else 4.
	// ======================================================================
	static void RainTemplate(uint8_t *T)
	{
		T[0x3C] = 0xA0;
		T[0x34] = 0xA0;
		T[0x40] = 0xC0;
		T[0x38] = 0xC0;
		T[0x39] = 0xC0;
		T[0x35] = 0xC0;
		T[0x41] = 0xE0;
		T[0x3D] = 0xE0;
		*(int16_t *)(T + 0x10) = (int16_t)0xFF00;
		*(int16_t *)(T + 0x12) = (int16_t)0xFF00;
		*(int16_t *)(T + 0x1A) = (int16_t)0xFF00;
		*(int16_t *)(T + 0x20) = (int16_t)0xFF00;
		*(uint32_t *)(T + 0x30) = 0x2E808080;
		*(uint16_t *)(T + 0x36) = 0x3EE4;
		*(uint16_t *)(T + 0x3A) = 0x36;
		*(int16_t *)(T + 0x18) = 0x100;
		*(int16_t *)(T + 0x22) = 0x100;
		*(int16_t *)(T + 0x24) = 0;
		*(int16_t *)(T + 0x1C) = 0;
		*(int16_t *)(T + 0x14) = 0;
	}

	// one quad; true when inserted
	static bool RainQuad(uint8_t *T, uint32_t &cursor, uint32_t color, int16_t size)
	{
		uint32_t *pk = (uint32_t *)cursor;
		pk[1] = color;
		GteSetRotScale(size);
		GteTransFromMAC();
		GteLoadV012(T + 0x10, T + 0x18, T + 0x20);
		GteRTPT();
		pk[3] = *(uint32_t *)(T + 0x34);
		pk[0] = 0x09000000;
		pk[5] = *(uint32_t *)(T + 0x38);
		pk[7] = *(uint32_t *)(T + 0x3C);
		pk[9] = *(uint32_t *)(T + 0x40);
		GteReadFLAG(T + 8);
		if (*(uint32_t *)(T + 8) & 0x60000) return false;
		GteReadSXY012Split(pk + 2, pk + 4, pk + 6);
		*(int16_t *)((uint8_t *)pk + 0x20) = *(int16_t *)((uint8_t *)pk + 0x10);
		*(int16_t *)((uint8_t *)pk + 0x22) = *(int16_t *)((uint8_t *)pk + 0x1A);
		GteReadSZ3(T + 0xC);
		int32_t z = *(int32_t *)(T + 0xC);
		InsertPrimAutoDepth(*(uint32_t *)T + 4 * (uint32_t)(z >> (*(uint32_t *)(T + 4) & 31)), pk);
		cursor += 0x28;
		return true;
	}

	static const uint32_t *const RAIN_COLOR = (const uint32_t *)0xE3C988;

	static uint32_t __cdecl RainTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x50) + 0x10;
		uint8_t *T = (uint8_t *)FieldAlloc(0x44);
		RainTemplate(T);
		memcpy(s, &Camera(), sizeof(Mat4x3));
		int32_t count = 0;
		GteSetLightMatrix(s);
		GteSetBackColorFromTrans(s);
		*(uint32_t *)T = OT();
		*(uint32_t *)(T + 4) = (var<uint8_t>(0x24FBE80) & 1) ? 0xE : 4;
		uint32_t cursor = Cursor();
		for (int i = 0; i < 590; i++)
		{
			Rec *r = R(POOL_F, i);
			if (!(*(uint8_t *)r & 1)) continue;
			int32_t fr = r->age;
			GteLoadV0(r->pos);
			GteMVMVA_LightV0Bk();
			g_memo_f.put(i, n, r);
			uint32_t color = RAIN_COLOR[fr];
			if (!P1())
			{
				fr++;
				r->age = (int16_t)fr;
				if (fr >= 0x40) r->mask = 0;
				else
				{
					r->pos[0] = (int16_t)(r->pos[0] + r->vel[0]);
					r->pos[1] = (int16_t)(r->pos[1] + r->vel[1]);
					r->pos[2] = (int16_t)(r->pos[2] + r->vel[2]);
				}
			}
			if (RainQuad(T, cursor, color, r->size)) count++;
		}
		Cursor() = cursor;
		FieldFree(0x44);
		FieldFree(0x50);
		if (P1()) return 0;
		FieldAlloc(0x50);
		if (n->c < 0x134)
		{
			for (int k = 0; k < 0x1C; k++)
			{
				int i = FreeRec(POOL_F, 590);
				if (i < 0) break;
				Rec *r = R(POOL_F, i);
				r->mask = 1;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x600 + 0x600);
				r->pos[0] = (int16_t)(2 * (CrtRand() % 20000) - 20000);
				r->pos[1] = (int16_t)(CrtRand() % 20000 - 10000);
				r->pos[2] = (int16_t)(2 * (CrtRand() % 20000) - 20000);
				r->vel[0] = (int16_t)(CrtRand() % 0x30 - 0x18);
				r->vel[1] = (int16_t)(CrtRand() % 0x30 - 0x18);
				r->vel[2] = (int16_t)(CrtRand() % 0x30 - 0x18);
			}
		}
		FieldFree(0x50);
		n->c++;
		if (n->c >= 0x134 && count == 0) return TASK_END;
		return 0;
	}

	// ======================================================================
	// Creature (0x641EA0, timeline 124), 279 ticks. State E = 0x24FC290 (model data 0x24FBD70,
	// model id 15 from the model buffer): +0x40 model matrix 0x24FC2D0 (pure scale diagonal
	// 0x24FC2D0/D8/E0, z translation 0x24FC2EC = distance of the train), +0x60 header, +0x6C
	// reader command. Skeleton word +2 (pointer 0x24FC0C0, original value kept in +0x1C) = model
	// scale: /4, /5 from 69. Phases: 0..34 far (z 25000), 35..68 approach, 69..168 anim 1 at
	// z 0 (the wheel trail), 169..188 anim 0 rushing past (targets moved away), 189..210, 211..241
	// (targets restored), 242..281 (explosions, knock-back, camera shake), 282..285 still drawn.
	// After every tick: 0x24FBE58 = model matrix o bone 1 (chimney) for the smoke / light tasks.
	// ======================================================================
	// MAG_191_sub_642E10: rotation part = identity (translation kept)
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

	// MAG_191_sub_642400: bind the model data (model buffer offsets table) to the state
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
		*(uint8_t **)(data + 0xC) = buf + *(uint32_t *)(buf + 4);
		*(uint8_t **)(data + 0x10) = buf + *(uint32_t *)(buf + 8);
		*(uint8_t **)(data + 0x14) = buf + *(uint32_t *)(buf + 0xC);
		*(uint16_t *)(data + 2) = (uint16_t)id;
		*(uint8_t **)(data + 0x30) = buf + *(uint32_t *)(buf + 0x10);
	}

	// MAG_191_sub_642470
	static void SetAnim(uint8_t *E, int32_t id) { ReaderSetAnim(E + 0x60, E + 0x6C, id); }

	// MAG_191_sub_642CA0: next frame, restart a looping animation that finished
	static void AdvanceLoop(uint8_t *E)
	{
		if (ReaderRead(E + 0x60, E + 0x6C) == 1 && !(E[0] & 1))
			ReaderSetAnim(E + 0x60, E + 0x6C, (int32_t)E[0x6C]);
	}

	// MAG_191_sub_642CE0
	static void DrawModel(uint8_t *E, const Mat4x3 *frame)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		uint32_t mask = ComposeBones(E + 0x60, frame, (const Mat4x3 *)(E + 0x40));
		*(uint32_t *)(h + 0x30) = var<uint32_t>(0x1D969A8);
		*(uint32_t *)(h + 0x24) = var<uint32_t>(0xE3C8D8);
		*(uint16_t *)(h + 0x34) = 0;
		*(uint16_t *)(h + 0x36) = 0;
		*(uint32_t *)(h + 0x48) = 0;
		*(uint16_t *)(h + 0x44) = 0;
		*(uint32_t *)(h + 0x3C) = *(uint32_t *)(E + 0x28);
		*(uint16_t *)(h + 0x38) = 0x140;
		*(uint16_t *)(h + 0x3A) = 0xD8;
		*(uint32_t *)(h + 0x40) = *(uint32_t *)(E + 0x7C) ^ mask;
		Cursor() = RenderGeometry(*(void **)(E + 0x64), h + 0x20, OT(), 4, Cursor());
		ReaderBuildBones(E + 0x60);
		FieldFree(0x4C);
	}

	static uint8_t *Skeleton()
	{
		uint8_t *com = *(uint8_t **)(Creature() + 0x64); // BattleAnimHeader.comFileData
		return com ? *(uint8_t **)com : nullptr;
	}

	static void SetDiag(uint16_t d)
	{
		var<uint16_t>(0x24FC2E0) = d;
		var<uint16_t>(0x24FC2D8) = d;
		var<uint16_t>(0x24FC2D0) = d;
	}

	// held-frame memo of the creature: the skeleton as drawn (pose values + local matrices) and
	// the reader command after that frame, the model matrix drawn with
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct CreatureMemo
	{
		uint32_t tick;
		Mat4x3 model;
		uint32_t skel_size;
		uint8_t cmd[8];
		uint8_t skel[SKEL_MAX];
	};
	static CreatureMemo g_creature = { 0xFFFFFFFF };

	static void CreatureMemoTake()
	{
		CreatureMemo &m = g_creature;
		uint8_t *sk = Skeleton();
		m.tick = 0xFFFFFFFF;
		if (!sk) return;
		uint32_t size = 16 + 48 * (uint32_t)sk[0];
		if (size > SKEL_MAX) return;
		m.skel_size = size;
		memcpy(m.skel, sk, size);
		memcpy(m.cmd, Creature() + 0x6C, 8);
		m.model = CreatureMat();
		m.tick = g_real_tick;
	}

	static void SpawnFlare();
	static void LinkGlow(int16_t e, int16_t y, int16_t s1c, int16_t s1e);
	static void LinkSteam(uint32_t task_fn, int16_t e, int16_t f12, int16_t f1C, int16_t f1E, int16_t f20, int16_t f22, bool set_e, bool set_f12, bool hist_b);
	static void SpawnPuffLine(bool second);
	static void KnockTargets();
	static void SpawnExplosions();

	static uint32_t __cdecl CreatureTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *E = Creature();
		int16_t *sp;
		int32_t z;
		if (n->c == 0)
		{
			ResetRotation(&CreatureMat());
			var<int32_t>(0x24FC2E8) = 0;
			var<int32_t>(0x24FC2E4) = 0;
			ModelInit(E, (uint8_t *)0x24FBD70, (uint8_t *)MB(), 0xF);
			sp = (int16_t *)(var<uint32_t>(0x24FBD7C) + 2);
			var<int16_t *>(0x24FC0C0) = sp;
			n->f1C = *sp;
		}
		else sp = var<int16_t *>(0x24FC0C0);
		int32_t k = n->c;
		if (k < 0x23)
		{
			*sp = (int16_t)(n->f1C / 4);
			CreatureZ() = 0x61A8;
			SetDiag(0x2000);
			if (P1()) goto draw;
			if (k == 0) { SetAnim(E, 0); goto draw; }
			if (k == 1) { LinkTask(ORIG_ChimneyTask); goto draw; }
			if (k == 0xE) SpawnFlare();
			goto draw;
		}
		k -= 0x23;
		if (k < 9) // 35..43
		{
			if (P1()) goto after_advance;
			if (k == 1) LinkGlow(0x21, (int16_t)0xFB82, 0xF00, 0xF00);
			else if (k == 0)
			{
				LinkSteam(ORIG_SteamTrailTask, 0x22, 0xF, 0x580, 0x380, 0x32, 0x78, true, true, false);
				LinkSteam(ORIG_WheelSteamTask, 0x22, 0xA, 0x480, 0x440, 0x32, 0x32, true, true, true);
			}
			z = CreatureZ() - 0x190;
			goto store;
		}
		k -= 9;
		if (k < 0x19) // 44..68
		{
			if (P1()) goto draw;
			CreatureZ() -= 0x258;
			AdvanceLoop(E);
			goto draw;
		}
		k -= 0x19;
		if (k < 0x64) // 69..168
		{
			CreatureZ() = 0;
			SetDiag(0x1000);
			*sp = (int16_t)(n->f1C / 5);
			if (P1()) goto after_advance;
			if (k == 0)
			{
				SetAnim(E, 1);
				LinkTask(ORIG_WheelTrailTask);
				goto compose;
			}
			AdvanceLoop(E);
			if (k == 1)
			{
				LinkGlow(0x63, (int16_t)0xFB82, 0x1C00, 0x1C00);
				LinkSteam(ORIG_SteamStillTask, 0x63, 0, 0x198, 0x168, 0xA, 0xF, true, false, false);
				LinkSteam(ORIG_WheelStillTask, 0x63, 0, 0x168, 0x148, 8, 8, true, false, true);
				goto draw;
			}
			goto after_advance;
		}
		k -= 0x64;
		if (k < 0x14) // 169..188
		{
			*sp = (int16_t)(n->f1C / 4);
			SetDiag(0x2000);
			if (!P1())
			{
				if (k == 0)
				{
					CreatureZ() = 0x7148;
					SetAnim(E, 0);
					SaveTargetsAndMove();
					LinkSteam(ORIG_SteamTrailTask, 0x14, 0, 0x680, 0x480, 0x32, 0x78, true, true, false);
					LinkSteam(ORIG_WheelSteamTask, 0x14, 0, 0x480, 0x440, 0x32, 0x32, true, true, true);
				}
				else if (k == 1)
				{
					ShowTargets();
					SpawnPuffLine(false);
					LinkGlow(0x12, (int16_t)0xFB82, 0xF00, 0xF00);
				}
				else if (k == 0x13) HideTargets();
				CreatureZ() -= 0x3E8;
				AdvanceLoop(E);
			}
			if (k == 0) goto compose;
			DrawModel(E, &Frame());
			CreatureMemoTake();
			{
				// vanilla halves the battle camera matrix here (it is rebuilt every frame)
				int32_t v[3] = { 0x800, 0x800, 0x800 };
				Scale3DMatrix(&Camera(), v);
			}
			goto compose;
		}
		k -= 0x14;
		if (k < 0x16) // 189..210
		{
			*sp = (int16_t)(n->f1C / 4);
			SetDiag(0x4000);
			if (P1()) goto after_advance;
			if (k == 0)
			{
				CreatureZ() = 0x6D60;
				AdvanceLoop(E);
				SpawnPuffLine(true);
				LinkSteam(ORIG_SteamBurstTask, 0, 0, 0x1200, 0xB80, 0x96, 0xFA, false, false, false);
				LinkSteam(ORIG_WheelBurstTask, 0, 0, 0xC00, 0x580, 0x96, 0x96, false, false, true);
			}
			else if (k == 1) LinkGlow(0x34, (int16_t)0xFB6E, 0x1300, 0x1000);
			else if (k == 4) LinkTask(ORIG_DebrisSpawnTask);
			z = CreatureZ() - 0x320;
			goto store;
		}
		k -= 0x16;
		if (k < 0x1F) // 211..241
		{
			*sp = (int16_t)(n->f1C / 4);
			SetDiag(0x4000);
			if (P1()) goto after_advance;
			if (k == 0)
			{
				CreatureZ() = 0x7D00;
				ShowTargets();
				RestoreTargets();
				StateFlags() |= 2;
			}
			else if (k == 0x14) LinkTask(ORIG_PuffWallTask);
			else if (k > 0x14) CreatureZ() -= 0x708;
			goto advance;
		}
		k -= 0x1F;
		if (k < 0x28) // 242..281
		{
			*sp = (int16_t)(n->f1C / 4);
			SetDiag(0x4000);
			if (P1()) goto after_advance;
			if (k == 0)
			{
				CreatureZ() = 0x6D60;
				LinkGlow(0xA, (int16_t)0xFB6E, 0x1300, 0x1000);
			}
			else if (k == 3)
			{
				KnockTargets();
				CameraShake(0, 1, 4, 0x80);
			}
			else if (k == 2) SpawnExplosions();
			z = CreatureZ() - 0x12C0;
			goto store;
		}
		k -= 0x28;
		if (k < 4) goto draw; // 282..285
		goto compose;
	store:
		CreatureZ() = z;
	advance:
		AdvanceLoop(E);
	after_advance:
		if (k == 0) goto compose;
	draw:
		DrawModel(E, &Frame());
		CreatureMemoTake();
	compose:
		ComposeAffineTransform(&CreatureMat(), (const Mat4x3 *)(*(uint8_t **)*(uint8_t **)(E + 0x64) + 0x50), &BoneWorld());
		if (P1()) return 0;
		n->c++;
		if (n->c < 0x117) return 0;
		StateFlags() |= 1;
		return TASK_END;
	}

	// ======================================================================
	// Wheel trail (creature 69, au_re_BdLinkTask_78): on its first tick it steps anim 1 through
	// up to 119 frames, dropping a record pair (pools A and B, same index) at the two wheel
	// points of bone 1 for each; then draws every pair (offset form, 0xE33E40) with a looping
	// flipbook; 100 ticks, then clears both pools' bit-0 records.
	// ======================================================================
	static void WheelTrailPair(uint8_t *h, int32_t *mac, int32_t *nv, const Rec *a, const Rec *b, int16_t fa, int16_t fb)
	{
		OffsetSprite(h, mac, nv, a->pos, a->size, fa, 4);
		OffsetSprite(h, mac, nv, b->pos, b->size, fb, 4);
	}

	static uint32_t __cdecl WheelTrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		*(uint32_t *)h = 0xE33E40;
		*(uint16_t *)(h + 0x24) = 8;
		*(int16_t *)s = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 0x10));
		*(int32_t *)(s + 0x24) = 0;
		*(int32_t *)(s + 0x28) = 0;
		*(int32_t *)(s + 0x2C) = 0;
		ComposeAffineTransform(&Frame(), (Mat4x3 *)(s + 0x10), (Mat4x3 *)(s + 0x10));
		GteSetLightMatrix(s + 0x10);
		GteSetBackColorFromTrans(s + 0x10);
		for (int i = 0; i < 170; i++)
		{
			Rec *a = R(POOL_A, i), *b = R(POOL_B, i);
			if (!(*(uint8_t *)a & 1)) continue;
			g_memo_a.put(i, n, a);
			g_memo_b.put(i, n, b);
			WheelTrailPair(h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), a, b, a->age, b->age);
			if (P1()) continue;
			a->age++;
			b->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				a->age = 0;
				b->age = 0;
			}
		}
		FieldFree(0x50);
		FieldFree(0xB4);
		if (P1()) return 0;
		s = (uint8_t *)FieldAlloc(0x50);
		if (n->c == 0)
		{
			uint8_t *E = Creature();
			uint8_t *bone = *(uint8_t **)*(uint8_t **)(E + 0x64) + 0x40;
			SetAnim(E, 1);
			for (int k = 0; k < 0x77; k++)
			{
				int i = FreeRec(POOL_A, 170);
				if (i < 0) break;
				Mat4x3 *m = (Mat4x3 *)(s + 0x10);
				int16_t *v = (int16_t *)(s + 8);
				ComposeAffineTransform(&CreatureMat(), (const Mat4x3 *)(bone + 0x10), m);
				v[1] = 0x2BC;
				v[0] = (int16_t)0xFB50;
				v[2] = 0;
				MatVec(m, v, v);
				v[1] = (int16_t)(v[1] + *(int16_t *)(s + 0x28));
				v[2] = (int16_t)(v[2] + *(int16_t *)(s + 0x2C));
				v[0] = (int16_t)(v[0] + *(int16_t *)(s + 0x24));
				Rec *a = R(POOL_A, i);
				a->mask = 1;
				a->age = 0;
				a->size = 0x180;
				memcpy(a->pos, v, 8); // v[3] = s + 0xE: scratch word, copied as vanilla does
				v[0] = 0x4B0;
				v[1] = 0x2BC;
				v[2] = 0;
				MatVec(m, v, v);
				v[0] = (int16_t)(v[0] + *(int16_t *)(s + 0x24));
				v[1] = (int16_t)(v[1] + *(int16_t *)(s + 0x28));
				v[2] = (int16_t)(v[2] + *(int16_t *)(s + 0x2C));
				Rec *b = R(POOL_B, i);
				b->mask = 1;
				b->age = 0;
				b->size = 0x180;
				memcpy(b->pos, v, 8);
				AdvanceLoop(E);
			}
			SetAnim(E, 1);
		}
		FieldFree(0x50);
		n->c++;
		if (n->c < 0x64) return 0;
		for (int i = 0; i < 170; i++)
		{
			Rec *a = R(POOL_A, i), *b = R(POOL_B, i);
			if (*(uint8_t *)a & 1) a->mask = 0;
			if (*(uint8_t *)b & 1) b->mask = 0;
		}
		return TASK_END;
	}

	// ======================================================================
	// Puff line (creature 170 = 0x6432F0 / 189 = au_re_BdLinkTask_84): on its first tick +0x16
	// record pairs (pool A bit 0) either side (+-0x1C) of a point marching by +0x22 along z;
	// draws every bit-0 record of pool A (lit, 0xE33E40), looping flipbook, drifting by +0x1E
	// in z; +0x0E ticks, then clears pool A's bit-0 records.
	// ======================================================================
	static void SpawnPuffLine(bool second)
	{
		Node24 *n = AddTask(ORIG_PuffLineTask);
		n->c = 0;
		if (second)
		{
			n->e = 0x2A;
			n->f10 = 0;
			n->f12 = (int16_t)0xFE70;
			n->f14 = 0x7530;
			n->f16 = 0x14;
			n->f1C = 0x7D0;
			n->f1E = 0x320;
			n->f20 = 0x1400;
			n->f22 = (int16_t)0xF448;
		}
		else
		{
			n->e = 0x13;
			n->f10 = 0;
			n->f12 = (int16_t)0xFF38;
			n->f14 = 0x7530;
			n->f16 = 0x1E;
			n->f1C = 0x384;
			n->f1E = 0;
			n->f20 = 0x800;
			n->f22 = (int16_t)0xF830;
		}
	}

	static uint32_t __cdecl PuffLineTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE33E40;
		*(uint16_t *)(h + 0x24) = 8;
		LitFrame(s, nullptr, &Frame(), 0, 0, 0);
		for (int i = 0; i < 170; i++)
		{
			Rec *r = R(POOL_A, i);
			if (!(*(uint8_t *)r & 1)) continue;
			LitSprite(h, r->pos, r->size, r->age, false);
			g_memo_a.put(i, n, r);
			if (P1()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0) r->age = 0;
			else r->pos[2] = (int16_t)(r->pos[2] + n->f1E);
		}
		FieldFree(0x48);
		FieldFree(0xB4);
		if (P1()) return 0;
		FieldAlloc(0x48);
		if (n->c == 0)
		{
			for (int k = 0; k < (int32_t)n->f16; k++)
			{
				int i = FreeRec(POOL_A, 170);
				if (i < 0) break;
				int16_t sz = n->f20, dx = n->f1C;
				Rec *a = R(POOL_A, i);
				a->mask = 1;
				a->age = 0;
				a->size = sz;
				memcpy(a->pos, &n->f10, 8);
				a->pos[0] = (int16_t)(a->pos[0] - dx);
				Rec *b = R(POOL_A, i + 1);
				b->mask = 1;
				b->age = 0;
				b->size = sz;
				memcpy(b->pos, &n->f10, 8);
				b->pos[0] = (int16_t)(b->pos[0] + dx);
				n->f14 = (int16_t)(n->f14 + n->f22);
			}
		}
		FieldFree(0x48);
		n->c++;
		if (n->c < n->e) return 0;
		ClearMask(POOL_A, 170, 1, false);
		return TASK_END;
	}

	// ======================================================================
	// Puff wall (creature 231): on its first tick 20 record pairs (pool A bit 0, size 0x1400)
	// 2000 either side of a point marching -3000 along z from (0, -400, 30000); draws every
	// bit-0 record (lit, 0xE33E40, header +0x24 0x208 before tick 11), fading out from 41
	// (grey level (49 - c) << 4); 49 ticks, then clears pool A's bit-0 records.
	// ======================================================================
	static uint32_t WallGrey(int32_t c)
	{
		uint32_t v = (uint32_t)shl32(0x31 - c, 4);
		return (((v << 8) | v) << 8) | v;
	}

	static void PuffWallSetup(uint8_t *h, uint8_t *s, const Mat4x3 *frame, int16_t c, bool held, uint32_t grey)
	{
		*(uint32_t *)h = 0xE33E40;
		*(uint16_t *)(h + 0x24) = 8;
		if (c < 0xB) *(uint16_t *)(h + 0x24) = 0x208;
		*(int16_t *)s = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = 0;
		*(int32_t *)(s + 0x20) = 0;
		*(int32_t *)(s + 0x24) = 0;
		ComposeAffineTransform(frame, (Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		if (c >= 0x29)
		{
			h[0x24] |= 4;
			*(uint32_t *)(h + 0x1C) = held ? grey : WallGrey(c);
		}
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static uint32_t __cdecl PuffWallTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		PuffWallSetup(h, s, &Frame(), n->c, false, 0);
		MemoNode(n);
		for (int i = 0; i < 170; i++)
		{
			Rec *r = R(POOL_A, i);
			if (!(*(uint8_t *)r & 1)) continue;
			LitSprite(h, r->pos, r->size, r->age, false);
			g_memo_a.put(i, n, r);
			if (P1()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0) r->age = 0;
		}
		FieldFree(0x48);
		FieldFree(0xB4);
		if (P1()) return 0;
		FieldAlloc(0x48);
		if (n->c == 0)
		{
			n->f10 = 0;
			n->f12 = (int16_t)0xFE70;
			n->f14 = 0x7530;
			for (int k = 0; k < 0x14; k++)
			{
				int i = FreeRec(POOL_A, 170);
				if (i < 0) break;
				Rec *a = R(POOL_A, i);
				a->mask = 1;
				a->age = 0;
				a->size = 0x1400;
				memcpy(a->pos, &n->f10, 8);
				a->pos[0] = (int16_t)(a->pos[0] - 2000);
				Rec *b = R(POOL_A, i + 1);
				b->mask = 1;
				b->age = 0;
				b->size = 0x1400;
				memcpy(b->pos, &n->f10, 4);
				memcpy(&b->pos[2], &n->f14, 4);
				n->f14 = (int16_t)(n->f14 - 3000);
				b->pos[0] = (int16_t)(b->pos[0] + 2000);
			}
		}
		FieldFree(0x48);
		n->c++;
		if (n->c < 0x31) return 0;
		ClearMask(POOL_A, 170, 1, false);
		return TASK_END;
	}

	// ======================================================================
	// Headlight flare (creature 14, MAG_191_sub_6437F0): prim 0xE3A744 at the lamp (bone point
	// (0, -1150, -9750) in 0x24FBE58, taken once), x/y scale +0x1C * sin(1760 c / 19) over 19
	// ticks, 22 ticks. The spawner also seeds the glow flicker 0x24FC284 (rand) / 0x24FC288.
	// ======================================================================
	static void SpawnFlare()
	{
		Node24 *n = AddTask(ORIG_FlareTask);
		int16_t *v = &n->f10;
		n->c = 0;
		v[0] = 0;
		n->f12 = (int16_t)0xFB82;
		n->f14 = (int16_t)0xD9EA;
		MatVec(&BoneWorld(), v, v);
		v[0] = (int16_t)(v[0] + W16(0x24FBE6C));
		n->f12 = (int16_t)(n->f12 + W16(0x24FBE70));
		n->f14 = (int16_t)(n->f14 + W16(0x24FBE74));
		n->f1C = 0xB00;
		var<int32_t>(0x24FC284) = CrtRand() % 0x1000;
		var<int32_t>(0x24FC288) = 0x110;
	}

	static int32_t FlareScale(int32_t c, int32_t s)
	{
		if (c >= 0x13) c = 0x13;
		return mul32(ComputeSin(1760 * c / 19), s) >> 12;
	}

	static void FlareDraw(const Mat4x3 *frame, const Node24 *n, int32_t v)
	{
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[1] = n->f12;
		m.t[0] = n->f10;
		m.t[2] = n->f14;
		int32_t sc[3] = { v, v, n->f1C };
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimModel(0xE3A744, 2, 0x33);
	}

	static uint32_t __cdecl FlareTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		int32_t v;
		{
			int16_t ang[3] = { 0, 0, 0 };
			Mat4x3 m;
			ComposeZYXRotationMatrix(ang, &m);
			m.t[1] = n->f12;
			m.t[0] = n->f10;
			m.t[2] = n->f14;
			v = FlareScale(c, n->f1C);
			int32_t sc[3] = { v, v, n->f1C };
			Scale3DMatrix(&m, sc);
			ComposeAffineTransform(&Frame(), &m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			PrimModel(0xE3A744, 2, 0x33);
		}
		MemoNode(n);
		if (P1()) return 0;
		n->c++;
		return n->c >= 0x16 ? TASK_END : 0;
	}

	// ======================================================================
	// Headlight glow (au_re_BdLinkTask_85..89, creature 36/70/170/190/242): at the bone point
	// (0, +0x12, -9700) of 0x24FBE58: prim 0xE3A744 scaled +0x1C with a flickering fade
	// (0x24FC284 phase, 0x24FC288 speed + rand 0..63 per tick; phase % 13 == 1 = flash), then
	// prim 0xE3AD34 scaled +0x1E; +0x0E ticks.
	// ======================================================================
	static void LinkGlow(int16_t e, int16_t y, int16_t s1c, int16_t s1e)
	{
		Node24 *n = AddTask(ORIG_GlowTask);
		n->c = 0;
		n->f10 = 0;
		n->e = e;
		n->f12 = y;
		n->f14 = (int16_t)0xDA1C;
		n->f1C = s1c;
		n->f1E = s1e;
	}

	static int32_t GlowFade()
	{
		int32_t f;
		if (var<int32_t>(0x24FC288) % 13 == 1) f = 0x100;
		else
		{
			int32_t x = (ComputeSin(var<int32_t>(0x24FC284)) + 0x1000) / 2;
			f = shl32(x * 9, 8) >> 12;
		}
		return f;
	}

	// fade < 0: computed (the real tick), else the held value
	static int32_t GlowDraw(const Mat4x3 *frame, const Mat4x3 *bone, const Node24 *n, int32_t held_fade)
	{
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m1, m2;
		ComposeZYXRotationMatrix(ang, &m1);
		m1.t[0] = n->f10;
		m1.t[1] = n->f12;
		m1.t[2] = n->f14;
		ComposeAffineTransform(bone, &m1, &m1);
		m2 = m1;
		int32_t v[3] = { n->f1C, n->f1C, n->f1C };
		Scale3DMatrix(&m2, v);
		ComposeAffineTransform(frame, &m2, &m2);
		GteSetRotMatrix(&m2);
		GteSetTransVector(&m2);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = 0xE3A744;
		*(int32_t *)(h + 0xC) = held_fade >= 0 ? held_fade : GlowFade();
		if (*(int32_t *)(h + 0xC) < 0) *(int32_t *)(h + 0xC) = 0;
		int32_t fade = *(int32_t *)(h + 0xC);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		Cursor() = RenderPrimModel(h, OT(), 3, Cursor());
		int32_t v2[3] = { n->f1E, n->f1E, n->f1E };
		Scale3DMatrix(&m1, v2);
		ComposeAffineTransform(frame, &m1, &m1);
		GteSetRotMatrix(&m1);
		GteSetTransVector(&m1);
		*(uint32_t *)h = 0xE3AD34;
		*(uint32_t *)(h + 0xC) = 0;
		Cursor() = RenderPrimModel(h, OT(), 3, Cursor());
		FieldFree(0x58);
		return fade;
	}

	struct GlowMemo { int32_t fade; };
	static NodeMemo<GlowMemo, 64> g_glow_memo;

	static uint32_t __cdecl GlowTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t fade = GlowDraw(&Frame(), &BoneWorld(), n, -1);
		if (GlowMemo *m = g_glow_memo.put(n)) m->fade = fade;
		if (P1()) return 0;
		var<int32_t>(0x24FC284) += var<int32_t>(0x24FC288);
		int32_t r = CrtRand() % 0x40;
		var<int32_t>(0x24FC288) += r;
		n->c++;
		return n->c >= n->e ? TASK_END : 0;
	}

	// ======================================================================
	// Chimney smoke (creature 1): pool A bit 2, 7 puffs per tick for 12 ticks at the chimney
	// (bone 1 point (-0xB4, -0x456, -0x2454)), rising with a speed damped by 1/11, growing by
	// 1/8 (lit 0xE342A4); ends on the first tick after 12 with no puff left.
	// ======================================================================
	static void ChimneyFrame(uint8_t *s, const Mat4x3 *bone, const Mat4x3 *frame)
	{
		LitFrame(s, bone, frame, -0xB4, -0x456, -0x2454);
	}

	static uint32_t __cdecl ChimneyTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = 0xE342A4;
		*(uint16_t *)(h + 0x24) = 8;
		int32_t count = 0;
		ChimneyFrame(s, &BoneWorld(), &Frame());
		for (int i = 0; i < 170; i++)
		{
			Rec *r = R(POOL_A, i);
			if (!(*(uint8_t *)r & 4)) continue;
			LitSprite(h, r->pos, r->size, r->age, false);
			g_memo_a.put(i, n, r);
			if (P1()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			int16_t sz = r->size;
			r->size = (int16_t)(sz + (int16_t)(sz >> 3));
			r->pos[1] = (int16_t)(r->pos[1] + r->vel[1]);
			int16_t vy = r->vel[1];
			r->vel[1] = (int16_t)(vy - vy / 11);
			count++;
		}
		FieldFree(0x48);
		FieldFree(0xB4);
		if (P1()) return 0;
		FieldAlloc(0x48);
		if (n->c < 0xC)
		{
			for (int k = 0; k < 7; k++)
			{
				int i = FreeRec(POOL_A, 170);
				if (i < 0) break;
				Rec *r = R(POOL_A, i);
				r->mask = 4;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x90 + 0x90);
				r->pos[0] = (int16_t)(CrtRand() % 0x1E - 0xF);
				r->pos[1] = (int16_t)-(CrtRand() % 0x28);
				r->pos[2] = (int16_t)(CrtRand() % 0x1E - 0xF);
				r->vel[1] = (int16_t)(-0x73 - CrtRand() % 0xF0);
			}
		}
		FieldFree(0x48);
		n->c++;
		if (n->c < 0xC) return 0;
		if (count) return 0;
		ClearMask(POOL_A, 170, 4, false);
		return TASK_END;
	}

	// ======================================================================
	// Steam from the chimney (pool C bit 0, offset form 0xE342A4 tinted 0x586880): each tick the
	// chimney point (0, -1780, -9300) of 0x24FBE58 goes into a two-entry history (0x24FC020);
	// the newest two points make a curve, sampled into 0x24FC100.. and seeded with puffs.
	//   SteamTrail (0x644030, au_re_BdLinkTask_90/_91): draw, then spawn 1 (7 from +0x12) per
	//     tick while c < +0x0E; puffs rise (damped 1/2) and grow by 1/32; +0x0E ticks.
	//   SteamStill (0x644530, _92): spawn 4 static puffs per tick, then draw; +0x0E ticks.
	//   SteamBurst (0x644940, _93): big puffs (5, 2 in 22..41), rising, pushed along z (+500 for
	//     42 ticks, growing by 1/4), point clamped to z >= -25000; until 59 and no puff left.
	// ======================================================================
	static void LinkSteam(uint32_t task_fn, int16_t e, int16_t f12, int16_t f1C, int16_t f1E, int16_t f20, int16_t f22, bool set_e, bool set_f12, bool hist_b)
	{
		Node24 *n = AddTask(task_fn);
		n->c = 0;
		if (set_e) n->e = e;
		if (set_f12) n->f12 = f12;
		n->f1C = f1C;
		n->f1E = f1E;
		n->f20 = f20;
		n->f22 = f22;
		for (int k = 0; k < 2; k++) (hist_b ? HistB(k) : HistA(k))->valid = 0;
	}

	// chimney point into history A (s = scratch 0x50: point at s + 8, its pad word s + 0xE is
	// scratch, copied as vanilla does); clamp: z >= -25000
	static void SteamHistory(uint8_t *s, int32_t c, bool clamp)
	{
		int16_t *v = (int16_t *)(s + 8);
		v[1] = (int16_t)0xF90C;
		v[0] = 0;
		v[2] = (int16_t)0xDBAC;
		MatVec(&BoneWorld(), v, v);
		v[0] = (int16_t)(v[0] + W16(0x24FBE6C));
		v[1] = (int16_t)(v[1] + W16(0x24FBE70));
		v[2] = (int16_t)(v[2] + W16(0x24FBE74));
		if (clamp && v[2] < (int16_t)0x9E58) v[2] = (int16_t)0x9E58;
		Hist *h = HistA(c % 2);
		h->valid = 1;
		h->a0 = *(uint32_t *)(s + 8);
		h->a1 = *(uint32_t *)(s + 0xC);
	}

	// the valid history points, newest first, into 0x24FBDC8 (and B points into 0x24FBE10)
	static int32_t SteamCollect(int32_t c, bool two)
	{
		int32_t k = c % 2, n = 0;
		for (; n < 2; n++)
		{
			Hist *h = two ? HistB(k) : HistA(k);
			if (!h->valid) break;
			var<uint32_t>(0x24FBDC8 + 8 * n) = h->a0;
			var<uint32_t>(0x24FBDCC + 8 * n) = h->a1;
			if (two)
			{
				var<uint32_t>(0x24FBE10 + 8 * n) = h->b0;
				var<uint32_t>(0x24FBE14 + 8 * n) = h->b1;
			}
			k--;
			if (k < 0) k = 1;
		}
		return n;
	}

	// curve points: count samples at i * 0x1000 / div into dst
	static void SteamCurve(int32_t n, uint32_t pts, uint32_t dst, int32_t count, int32_t div, uint8_t *buf, bool coeffs)
	{
		if (coeffs) CurveCoeffs(n, (const void *)pts, buf);
		for (int32_t i = 0, t = 0; i < count; i++, t += 0x1000) CurveEval(n, buf, (void *)(dst + 8 * i), t / div);
	}

	// draw scratch: header (0xB4) + light scratch (0x50: frame at +0x10, MAC at +0x40, normal
	// at +0x30), the effect frame as the light matrix
	static void SteamSetup(uint8_t *h, uint8_t *s, uint32_t seq, bool tint, int16_t w24)
	{
		*(uint32_t *)h = seq;
		if (tint) *(uint32_t *)(h + 0x1C) = 0x586880;
		*(uint16_t *)(h + 0x24) = (uint16_t)w24;
		memcpy(s + 0x10, &Frame(), sizeof(Mat4x3));
		GteSetLightMatrix(s + 0x10);
		GteSetBackColorFromTrans(s + 0x10);
	}

	// one spawned steam puff at a curve point
	static Rec *SteamPuff(uint32_t cell, int i, const Node24 *n, uint32_t pt, bool y_signed)
	{
		Rec *r = R(cell, i);
		r->mask = 1;
		r->age = 0;
		r->size = (int16_t)(CrtRand() % n->f1E + n->f1C);
		memcpy(r->pos, (const void *)pt, 8);
		int32_t f20 = n->f20;
		r->pos[0] = (int16_t)(r->pos[0] + (int16_t)(CrtRand() % (2 * f20) - f20));
		if (y_signed)
		{
			int32_t f22 = n->f22;
			r->pos[1] = (int16_t)(r->pos[1] + (int16_t)(CrtRand() % (2 * f22) - f22));
		}
		else r->pos[1] = (int16_t)(r->pos[1] - (int16_t)(CrtRand() % n->f22));
		r->pos[2] = (int16_t)(r->pos[2] + (int16_t)(CrtRand() % (2 * f20) - f20));
		return r;
	}

	static uint32_t __cdecl SteamTrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		SteamHistory(s, n->c, false);
		int32_t np = SteamCollect(n->c, false);
		SteamSetup(h, s, 0xE342A4, true, 0xC);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = R(POOL_C, i);
			if (!(*(uint8_t *)r & 1)) continue;
			OffsetSprite(h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), r->pos, r->size, r->age, 4);
			g_memo_c.put(i, n, r);
			if (P1()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			int16_t sz = r->size;
			r->size = (int16_t)(sz + (int16_t)(sz >> 5));
			r->pos[1] = (int16_t)(r->pos[1] + r->vel[1]);
			int16_t vy = r->vel[1];
			r->vel[1] = (int16_t)(vy - (int16_t)(vy >> 1));
		}
		FieldFree(0x50);
		FieldFree(0xB4);
		if (P1()) return 0;
		if (n->c < n->e && np > 1)
		{
			uint8_t *buf = (uint8_t *)FieldAlloc(0x190);
			SteamCurve(np, 0x24FBDC8, 0x24FC100, 8, 7, buf, true);
			FieldFree(0x190);
			int32_t num = n->c >= n->f12 ? 7 : 1;
			uint32_t pt = 0x24FC100;
			for (int32_t j = 0; j < num; j++, pt += 8)
			{
				int i = FreeRec(POOL_C, 150);
				if (i < 0) break;
				Rec *r = SteamPuff(POOL_C, i, n, pt, false);
				r->vel[1] = (int16_t)(-0x41 - CrtRand() % 0x82);
			}
		}
		n->c++;
		if (n->c < n->e) return 0;
		ClearMask(POOL_C, 150, 1, false);
		return TASK_END;
	}

	static uint32_t __cdecl SteamStillTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		SteamHistory(s, n->c, false);
		FieldFree(0x50);
		int32_t np = SteamCollect(n->c, false);
		if (!P1() && n->c < n->e && np > 1)
		{
			uint8_t *buf = (uint8_t *)FieldAlloc(0x190);
			SteamCurve(np, 0x24FBDC8, 0x24FC100, 5, 4, buf, true);
			FieldFree(0x190);
			for (uint32_t pt = 0x24FC100; pt < 0x24FC120; pt += 8)
			{
				int i = FreeRec(POOL_C, 150);
				if (i < 0) break;
				SteamPuff(POOL_C, i, n, pt, false);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		s = (uint8_t *)FieldAlloc(0x50);
		SteamSetup(h, s, 0xE342A4, true, 0xC);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = R(POOL_C, i);
			if (!(*(uint8_t *)r & 1)) continue;
			OffsetSprite(h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), r->pos, r->size, r->age, 2);
			g_memo_c.put(i, n, r);
			if (P1()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0) r->mask = 0;
		}
		FieldFree(0x50);
		FieldFree(0xB4);
		if (P1()) return 0;
		n->c++;
		if (n->c < n->e) return 0;
		ClearMask(POOL_C, 150, 1, false);
		return TASK_END;
	}

	static uint32_t __cdecl SteamBurstTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		SteamHistory(s, n->c, true);
		FieldFree(0x50);
		int32_t np = SteamCollect(n->c, false);
		if (!P1() && n->c < 0x3B && np > 1)
		{
			uint8_t *buf = (uint8_t *)FieldAlloc(0x190);
			SteamCurve(np, 0x24FBDC8, 0x24FC100, 6, 5, buf, true);
			FieldFree(0x190);
			int16_t c = n->c;
			int32_t num = c < 0x16 ? 5 : (c < 0x2A ? 2 : 5);
			uint32_t pt = 0x24FC100;
			for (int32_t j = 0; j < num; j++, pt += 8)
			{
				int i = FreeRec(POOL_C, 150);
				if (i < 0) break;
				Rec *r = SteamPuff(POOL_C, i, n, pt, false);
				r->vel[1] = (int16_t)(-0xA0 - CrtRand() % 0x12C);
				r->vel[2] = n->c < 0x2A ? 0x1F4 : 0;
				if (num == 2) r->vel[1] = (int16_t)(r->vel[1] << 1);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		s = (uint8_t *)FieldAlloc(0x50);
		SteamSetup(h, s, 0xE342A4, true, 0xC);
		int32_t count = 0;
		for (int i = 0; i < 150; i++)
		{
			Rec *r = R(POOL_C, i);
			if (!(*(uint8_t *)r & 1)) continue;
			OffsetSprite(h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), r->pos, r->size, r->age, 4);
			g_memo_c.put(i, n, r);
			if (P1()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			int16_t sz = r->size;
			r->size = (int16_t)(sz + (int16_t)(sz >> 5));
			r->pos[1] = (int16_t)(r->pos[1] + r->vel[1]);
			r->pos[2] = (int16_t)(r->pos[2] + r->vel[2]);
			int16_t vy = r->vel[1], vz = r->vel[2];
			r->vel[1] = (int16_t)(vy - (int16_t)(vy >> 1));
			r->vel[2] = (int16_t)(vz + (int16_t)(vz >> 2));
			count++;
		}
		FieldFree(0x50);
		FieldFree(0xB4);
		if (P1()) return 0;
		n->c++;
		if (n->c < 0x3B) return 0;
		if (count) return 0;
		ClearMask(POOL_C, 150, 1, false);
		return TASK_END;
	}

	// ======================================================================
	// Steam from the wheels (pool D bit 0, offset form 0xE342A4, flipbook frame = 2 x age):
	// the two wheel points (+-1200, 530, -8500) of 0x24FBE58 go into history B (0x24FBEA8), two
	// curves (0x24FC100.. / 0x24FC1C0..) are seeded in pairs (left puffs drift right, right
	// puffs left).
	//   WheelSteam (0x644E70, _94/_95): draw, then 2 (7 from +0x12) pairs per tick while
	//     c < +0x0E; puffs slide (damped 1/2); +0x0E ticks.
	//   WheelStill (0x645540 inside au_re_BdLinkTask_96, _96): 4 static pairs per tick (no pause
	//     test before the spawn, vanilla), then draw; +0x0E ticks.
	//   WheelBurst (0x645AE0, _97): 5 (2 in 22..41) fast pairs per tick, z push +500 for 42
	//     ticks; points clamped to z >= -25000; until 59 and no puff left.
	// ======================================================================
	// both wheel points into history B (s + 0: world point, s + 8: local point)
	static void WheelHistory(uint8_t *s, int32_t c, bool clamp)
	{
		int16_t *w = (int16_t *)s, *v = (int16_t *)(s + 8);
		Hist *h = HistB(c % 2);
		h->valid = 1;
		v[0] = 0x4B0;
		v[1] = 0x212;
		v[2] = (int16_t)0xDECC;
		MatVec(&BoneWorld(), v, w);
		w[0] = (int16_t)(w[0] + W16(0x24FBE6C));
		w[1] = (int16_t)(w[1] + W16(0x24FBE70));
		w[2] = (int16_t)(w[2] + W16(0x24FBE74));
		if (clamp && w[2] < (int16_t)0x9E58) w[2] = (int16_t)0x9E58;
		h->a0 = *(uint32_t *)s;
		h->a1 = *(uint32_t *)(s + 4);
		v[0] = (int16_t)-v[0];
		MatVec(&BoneWorld(), v, w);
		w[0] = (int16_t)(w[0] + W16(0x24FBE6C));
		w[1] = (int16_t)(w[1] + W16(0x24FBE70));
		w[2] = (int16_t)(w[2] + W16(0x24FBE74));
		if (clamp && w[2] < (int16_t)0x9E58) w[2] = (int16_t)0x9E58;
		h->b0 = *(uint32_t *)s;
		h->b1 = *(uint32_t *)(s + 4);
	}

	static void WheelDrawLoop(const Node24 *n, uint8_t *h, uint8_t *s, int shift, bool move, bool burst, int32_t *count)
	{
		for (int i = 0; i < 120; i++)
		{
			Rec *r = R(POOL_D, i);
			if (!(*(uint8_t *)r & 1)) continue;
			OffsetSprite(h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), r->pos, r->size, (int16_t)(r->age << 1), shift);
			g_memo_d.put(i, n, r);
			if (P1()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			if (!move) continue;
			r->pos[0] = (int16_t)(r->pos[0] + r->vel[0]);
			r->pos[1] = (int16_t)(r->pos[1] + r->vel[1]);
			if (burst) r->pos[2] = (int16_t)(r->pos[2] + r->vel[2]);
			int16_t vx = r->vel[0], vy = r->vel[1];
			r->vel[0] = (int16_t)(vx - (int16_t)(vx >> 1));
			r->vel[1] = (int16_t)(vy - (int16_t)(vy >> 1));
			if (burst)
			{
				int16_t vz = r->vel[2];
				r->vel[2] = (int16_t)(vz + (int16_t)(vz >> 2));
				(*count)++;
			}
		}
	}

	static uint32_t __cdecl WheelSteamTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		WheelHistory(s, n->c, false);
		int32_t np = SteamCollect(n->c, true);
		SteamSetup(h, s, 0xE342A4, false, 8);
		WheelDrawLoop(n, h, s, 3, true, false, nullptr);
		FieldFree(0x50);
		FieldFree(0xB4);
		if (P1()) return 0;
		if (n->c < n->e && np > 1)
		{
			uint8_t *buf = (uint8_t *)FieldAlloc(0x190);
			SteamCurve(np, 0x24FBDC8, 0x24FC100, 8, 7, buf, true);
			SteamCurve(np, 0x24FBE10, 0x24FC1C0, 8, 7, buf, true);
			FieldFree(0x190);
			int32_t num = n->c >= n->f12 ? 7 : 2;
			for (int32_t j = 0; j < num; j++)
			{
				int i = FreeRec(POOL_D, 120);
				if (i < 0) break;
				Rec *a = SteamPuff(POOL_D, i, n, 0x24FC100 + 8 * j, true);
				a->vel[0] = (int16_t)(CrtRand() % 0x37 + 0x46);
				a->vel[1] = (int16_t)(CrtRand() % 0x37 + 0x46);
				Rec *b = SteamPuff(POOL_D, i + 1, n, 0x24FC1C0 + 8 * j, true);
				b->vel[0] = (int16_t)(-0x46 - CrtRand() % 0x37);
				b->vel[1] = (int16_t)(CrtRand() % 0x37 + 0x46);
			}
		}
		n->c++;
		if (n->c < n->e) return 0;
		ClearMask(POOL_D, 120, 1, false);
		return TASK_END;
	}

	static uint32_t __cdecl WheelStillTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		WheelHistory(s, n->c, false);
		FieldFree(0x50);
		int32_t np = SteamCollect(n->c, true);
		if (n->c < n->e && np > 1)
		{
			uint8_t *buf = (uint8_t *)FieldAlloc(0x190);
			SteamCurve(np, 0x24FBDC8, 0x24FC100, 5, 4, buf, true);
			SteamCurve(np, 0x24FBE10, 0x24FC1C0, 5, 4, buf, true);
			FieldFree(0x190);
			for (uint32_t o = 0; o < 0x20; o += 8)
			{
				int i = FreeRec(POOL_D, 120);
				if (i < 0) break;
				SteamPuff(POOL_D, i, n, 0x24FC100 + o, true);
				SteamPuff(POOL_D, i + 1, n, 0x24FC1C0 + o, true);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		s = (uint8_t *)FieldAlloc(0x50);
		SteamSetup(h, s, 0xE342A4, false, 8);
		WheelDrawLoop(n, h, s, 2, false, false, nullptr);
		FieldFree(0x50);
		FieldFree(0xB4);
		if (P1()) return 0;
		n->c++;
		if (n->c < n->e) return 0;
		ClearMask(POOL_D, 120, 1, false);
		return TASK_END;
	}

	static uint32_t __cdecl WheelBurstTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		WheelHistory(s, n->c, true);
		FieldFree(0x50);
		int32_t np = SteamCollect(n->c, true);
		if (!P1() && n->c < 0x3B && np > 1)
		{
			uint8_t *buf = (uint8_t *)FieldAlloc(0x190);
			SteamCurve(np, 0x24FBDC8, 0x24FC100, 6, 5, buf, true);
			SteamCurve(np, 0x24FBE10, 0x24FC1C0, 6, 5, buf, true);
			FieldFree(0x190);
			int16_t c = n->c;
			int32_t num = c < 0x16 ? 5 : (c < 0x2A ? 2 : 5);
			for (int32_t j = 0; j < num; j++)
			{
				int i = FreeRec(POOL_D, 120);
				if (i < 0) break;
				Rec *a = SteamPuff(POOL_D, i, n, 0x24FC100 + 8 * j, true);
				a->vel[0] = (int16_t)(CrtRand() % 0xEB + 0xA0);
				a->vel[1] = (int16_t)(CrtRand() % 0xEB + 0xA0);
				a->vel[2] = n->c < 0x2A ? 0x1F4 : 0;
				Rec *b = SteamPuff(POOL_D, i + 1, n, 0x24FC1C0 + 8 * j, true);
				b->vel[0] = (int16_t)(-0xA0 - CrtRand() % 0xEB);
				b->vel[1] = (int16_t)(CrtRand() % 0xEB + 0xA0);
				b->vel[2] = n->c < 0x2A ? 0x1F4 : 0;
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		s = (uint8_t *)FieldAlloc(0x50);
		SteamSetup(h, s, 0xE342A4, false, 8);
		int32_t count = 0;
		WheelDrawLoop(n, h, s, 2, true, true, &count);
		FieldFree(0x50);
		FieldFree(0xB4);
		if (P1()) return 0;
		n->c++;
		if (n->c < 0x3B) return 0;
		if (count) return 0;
		ClearMask(POOL_D, 120, 1, false);
		return TASK_END;
	}

	// ======================================================================
	// Debris (creature 193, spawner 0x646180): every third tick of 12, 1-2 pieces (0x6462D0)
	// after a 0..3+i tick delay: prim 0xE3BD64 morphing (0xE3C37C -> 0xE3C57C by c / 12 into
	// mb+0x37000) at +0x10 in the chimney frame 0x24FBE58, xz scale +0x1C shrinking by +0x1E,
	// y scale +0x20 growing by +0x22 (damped 1/8, max 0x7000), fading its last 8 ticks; +0x16
	// ticks, or until the wheels leave (0x24FBE80 bit 1).
	// ======================================================================
	static uint32_t __cdecl DebrisSpawnTask(TaskNode *tn)
	{
		if (P1()) return 0;
		Node24 *n = (Node24 *)tn;
		int16_t c = n->c;
		if (c >= 0 && c < 0xC && c % 3 == 1)
		{
			int32_t k = CrtRand() % 2 + 1;
			for (int32_t i = 0; i < k; i++)
			{
				Node24 *m = AddTask(ORIG_DebrisTask);
				m->c = 0;
				m->e = (int16_t)(CrtRand() % 4 + i);
				m->f10 = (int16_t)(CrtRand() % 0x190 + 0x3E8);
				m->f12 = (int16_t)(CrtRand() % 0x7D0 - 0x514);
				int32_t r = CrtRand() % 0xBB8;
				m->f16 = 0xC;
				m->f14 = (int16_t)(-0x1770 - r);
				int16_t s = (int16_t)(CrtRand() % 0xC00 + 0x480);
				m->f1C = s;
				m->f1E = (int16_t)(s / 6);
				int32_t v = (CrtRand() % 0x1800 + 0x680) / 2;
				m->f22 = (int16_t)v;
				m->f20 = (int16_t)v;
			}
		}
		n->c++;
		return n->c > 0xC ? TASK_END : 0;
	}

	static void DebrisDraw(const Mat4x3 *frame, const Mat4x3 *bone, const Node24 *n, int16_t sxz, int16_t sy, bool fade, int32_t fadev, int32_t t)
	{
		int16_t ang[3] = { 0, (int16_t)(n->f10 < 0 ? -100 : 100), 0x400 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[1] = n->f12;
		m.t[0] = n->f10;
		m.t[2] = n->f14;
		int32_t v[3] = { sxz, sxz, sy };
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(bone, &m, &m);
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = 0xE3BD64;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x2033;
		if (fade)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F3;
			*(int32_t *)(h + 0xC) = fadev;
		}
		int32_t count = var<int32_t>(0xE3BD68);
		int16_t *out = (int16_t *)(MB() + 0x37000);
		const int16_t *a = (const int16_t *)0xE3C37C, *b = (const int16_t *)0xE3C57C;
		for (int32_t i = 0; i < count; i++, a += 4, b += 4, out += 4)
		{
			out[0] = (int16_t)((mul32(b[0] - a[0], t) >> 12) + a[0]);
			out[1] = (int16_t)((mul32(b[1] - a[1], t) >> 12) + a[1]);
			out[2] = (int16_t)((mul32(b[2] - a[2], t) >> 12) + a[2]);
		}
		*(uint32_t *)(h + 4) = MB() + 0x37000;
		Cursor() = RenderPrimModel(h, OT(), 3, Cursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl DebrisTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t e = n->e;
		if (e > 0)
		{
			if (P1()) return 0;
			n->e = (int16_t)(e - 1);
			return 0;
		}
		int32_t c = n->c, d = n->f16 - 8;
		DebrisDraw(&Frame(), &BoneWorld(), n, n->f1C, n->f20, c >= d, shl32(c - d, 9), shl32(c, 12) / 12);
		MemoNode(n);
		if (P1()) return 0;
		n->f1C = (int16_t)(n->f1C - n->f1E);
		int16_t v = n->f22;
		n->f20 = (int16_t)(n->f20 + v);
		n->f22 = (int16_t)(v - v / 8);
		if (n->f20 > 0x7000) n->f20 = 0x7000;
		n->c++;
		if (n->c >= n->f16 || (var<uint8_t>(0x24FBE80) & 2)) return TASK_END;
		return 0;
	}

	// ======================================================================
	// Explosions (creature 244, au_re__rand_0 0x646570): 5..8 bursts (0x646640), +0x0E = pool E
	// tag 1 << (i+1), after a +0x16 tick delay: a flash flipbook 0xE3460C at +0x10 for 5 ticks,
	// and 6 shards per tick for 5 ticks (0xE34548, oriented along their velocity, speed damped
	// 1/2); ends on the first tick after 4 with no shard left.
	// ======================================================================
	static void SpawnExplosions()
	{
		int32_t cnt = CrtRand() % 4 + 5;
		for (int32_t i = 0; i < cnt; i++)
		{
			Node24 *m = AddTask(ORIG_ExplosionTask);
			m->c = 0;
			m->e = (int16_t)(1 << (i + 1));
			m->f10 = (int16_t)(CrtRand() % 0x3E8 + 0x3E8);
			m->f12 = (int16_t)(CrtRand() % 0x4B0 - 0x320);
			m->f14 = (int16_t)(CrtRand() % 0x2328 - 0x10CC);
			m->f16 = (int16_t)(CrtRand() % 0xC + 2 * i);
			m->f1C = (int16_t)(CrtRand() % 0x1000 + 0x1600);
		}
	}

	// one shard record (scratch s 0x98: frame at +8, model matrix +0x28, rotation +0x48, scale
	// +0x68, direction +0x78, axis +0x88)
	static void ShardDraw(uint8_t *h, uint8_t *s, const int16_t *pos, int16_t size, const int16_t *vel, int16_t age)
	{
		Mat4x3 *mm = (Mat4x3 *)(s + 0x28);
		ResetRotation(mm);
		*(int32_t *)(s + 0x3C) = pos[0];
		*(int32_t *)(s + 0x40) = pos[1];
		*(int32_t *)(s + 0x44) = pos[2];
		*(int32_t *)(s + 0x6C) = size;
		Scale3DMatrix(mm, (const int32_t *)(s + 0x68));
		int32_t *dir = (int32_t *)(s + 0x78);
		dir[0] = vel[0];
		dir[1] = vel[1];
		dir[2] = vel[2];
		Normalize(dir, dir);
		int32_t angle = GetRotationBetweenVectors((const void *)0xE3C978, dir, (int32_t *)(s + 0x88));
		BuildAxisAngleRotationMatrix(angle, (Mat4x3 *)(s + 0x48), (const int32_t *)(s + 0x88));
		GteMatrixMultiply((const Mat4x3 *)(s + 0x48), mm);
		ComposeAffineTransform((const Mat4x3 *)(s + 8), mm, mm);
		GteSetRotMatrix(mm);
		GteSetTransVector(mm);
		*(int16_t *)(h + 4) = age;
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static void ExplosionFrame(uint8_t *s, const Node24 *n, const Mat4x3 *frame)
	{
		*(int16_t *)s = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = n->f10;
		*(int32_t *)(s + 0x20) = n->f12;
		*(int32_t *)(s + 0x24) = n->f14;
		ComposeAffineTransform(frame, (Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetRotMatrix((Mat4x3 *)(s + 8));
		GteSetTransVector((Mat4x3 *)(s + 8));
	}

	static uint32_t __cdecl ExplosionTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->f16;
		if (d > 0)
		{
			if (!P1()) n->f16 = (int16_t)(d - 1);
			return 0;
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x98);
		*(uint32_t *)h = 0xE3460C;
		*(uint16_t *)(h + 0x24) = 0x208;
		int32_t count = 0;
		MemoNode(n);
		if (n->c < 5)
		{
			// vanilla passes eax with only ax loaded (high word = high word of the 0x98 scratch
			// pointer); the callee reads the low word only
			int16_t sz = n->f1C;
			TransformCameraByShadowRotation(&n->f10, (int32_t)(((uint32_t)s & 0xFFFF0000u) | (uint16_t)sz), -((int32_t)sz >> 3));
			*(int16_t *)(h + 4) = n->c;
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
		}
		*(uint32_t *)h = 0xE34548;
		ExplosionFrame(s, n, &Frame());
		*(int32_t *)(s + 0x70) = 0x1000;
		*(int32_t *)(s + 0x68) = 0x1000;
		for (int i = 0; i < 130; i++)
		{
			Rec *r = R(POOL_E, i);
			if (r->mask != (uint32_t)(int32_t)n->e) continue;
			ShardDraw(h, s, r->pos, r->size, r->vel, r->age);
			g_memo_e.put(i, n, r);
			if (P1()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			for (int k = 0; k < 3; k++) r->pos[k] = (int16_t)(r->pos[k] + r->vel[k]);
			for (int k = 0; k < 3; k++)
			{
				int16_t v = r->vel[k];
				r->vel[k] = (int16_t)(v - (int16_t)(v >> 1));
			}
			count++;
		}
		FieldFree(0x98);
		FieldFree(0xB4);
		if (P1()) return 0;
		s = (uint8_t *)FieldAlloc(0x98);
		int16_t c = n->c;
		if (c >= 0 && c <= 4)
		{
			for (int k = 0; k < 6; k++)
			{
				int i = FreeRec(POOL_E, 130);
				if (i < 0) break;
				Rec *r = R(POOL_E, i);
				r->mask = (uint32_t)(int32_t)n->e;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x1000 + 0x1400);
				int32_t *dv = (int32_t *)(s + 0x68);
				dv[0] = CrtRand() % 0x1000 - 0x800;
				dv[1] = CrtRand() % 0x1000 - 0x800;
				dv[2] = CrtRand() % 0x1000 - 0x800;
				Normalize(dv, dv);
				int32_t d1 = CrtRand() % 0x190 + 0x12C;
				r->pos[0] = (int16_t)(mul32(dv[0], d1) >> 12);
				r->pos[1] = (int16_t)(mul32(d1, dv[1]) >> 12);
				r->pos[2] = (int16_t)(mul32(dv[2], d1) >> 12);
				int32_t d2 = CrtRand() % 0x104 + 0xDC;
				r->vel[0] = (int16_t)(mul32(dv[0], d2) >> 12);
				r->vel[1] = (int16_t)(mul32(d2, dv[1]) >> 12);
				r->vel[2] = (int16_t)(mul32(dv[2], d2) >> 12);
			}
		}
		FieldFree(0x98);
		n->c++;
		if (n->c < 4) return 0;
		if (count) return 0;
		return TASK_END;
	}

	// ======================================================================
	// Knock-back of the targets (creature 245, MAG_191_sub_646AA0): per target a 0x646D30 task
	// (+0x0E battle slot, +0x10 spawn point = bone 0xF1 of the target, +0x18 spin 0x1000 or
	// 0x2000, +0x1A tag 1 << (i+1)); per-slot table 0x24FD150 (0x24 each: +0 flags & 0x1020,
	// +4 rotation, +0xC position saved, +0x14 offset, +0x1C its speed = (random point 15000
	// away from the targets' centre - spawn) / 16). The task tumbles and throws the entity for
	// 16 ticks, then drops it (+0x20 from -9000, +0x22 from 42), restores it at 50 and queues
	// the landing dust. Draws nothing itself (the engine draws the entity).
	// ======================================================================
	inline uint8_t *KnockSlot(int32_t slot) { return (uint8_t *)(0x24FD150 + 36 * slot); }

	static void KnockTargets()
	{
		int16_t center[4];
		CalculateCenterPosition((uint32_t)*(uint16_t *)(CastCtx() + 2), center);
		for (uint32_t i = 0; i < (*(uint8_t **)(CastCtx() + 4))[0x10]; i++)
		{
			uint8_t *t = *(uint8_t **)(CastCtx() + 4);
			uint32_t slot = (*(uint8_t **)(t + 8))[0x18 * i];
			Node24 *m = AddTask(ORIG_KnockTask);
			m->c = 0;
			m->e = (int16_t)slot;
			int32_t r = CrtRand() % 2 + 1;
			m->f18 = (int16_t)shl32(r, 12);
			m->f1A = (int16_t)(1 << (i + 1));
			uint8_t *E = Entity(slot);
			GetEffectSpawnPosition(E, 0xF1, 0, &m->f10);
			uint8_t *T = KnockSlot((int32_t)slot);
			*(uint16_t *)T = (uint16_t)(*(uint16_t *)E & 0x1020);
			*(uint32_t *)(T + 0xC) = *(uint32_t *)(E + 0x1C);
			*(uint32_t *)(T + 0x10) = *(uint32_t *)(E + 0x20);
			*(uint32_t *)(T + 4) = *(uint32_t *)(E + 0xC);
			*(uint32_t *)(T + 8) = *(uint32_t *)(E + 0x10);
			int32_t d[3];
			d[0] = (int32_t)m->f10 - center[0];
			d[1] = (int32_t)m->f12 - center[1];
			d[2] = (int32_t)m->f14 - center[2];
			if (d[2] < 0) d[2] = -d[2];
			Normalize(d, d);
			*(int16_t *)(T + 0x18) = 0;
			*(int16_t *)(T + 0x16) = 0;
			*(int16_t *)(T + 0x14) = 0;
			for (int k = 0; k < 3; k++)
			{
				int32_t rr = CrtRand() % 0x1770;
				*(int16_t *)(T + 0x1C + 2 * k) = (int16_t)((mul32(d[k], 15000) >> 12) + center[k] + rr - 0xBB8);
			}
			*(int16_t *)(T + 0x1C) = (int16_t)(((int32_t)*(int16_t *)(T + 0x1C) - m->f10) / 16);
			*(int16_t *)(T + 0x1E) = (int16_t)(((int32_t)*(int16_t *)(T + 0x1E) - m->f12) / 16);
			*(int16_t *)(T + 0x20) = (int16_t)(((int32_t)*(int16_t *)(T + 0x20) - m->f14) / 16);
		}
	}

	static uint32_t __cdecl KnockTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		int32_t w = mul32(n->f18, c) / 16;
		int32_t slot = n->e;
		uint8_t *E = Entity((uint32_t)slot);
		uint8_t *T = KnockSlot(slot);
		*(uint16_t *)E |= 0x1020;
		int16_t *rot = (int16_t *)(E + 0xC);
		rot[0] = (int16_t)w;
		rot[1] = (int16_t)(shl32(c, 12) / 16);
		int16_t d[4];
		d[0] = (int16_t)(n->f10 - *(int16_t *)(T + 0xC));
		d[1] = (int16_t)(n->f12 - *(int16_t *)(T + 0xE));
		d[2] = (int16_t)(n->f14 - *(int16_t *)(T + 0x10));
		Mat4x3 m;
		ComposeZYXRotationMatrix(rot, &m);
		MatVec(&m, d, d);
		int16_t *p = (int16_t *)(E + 0x1C);
		p[0] = (int16_t)(n->f10 - d[0]);
		p[1] = (int16_t)(n->f12 - d[1]);
		p[2] = (int16_t)(n->f14 - d[2]);
		int16_t cc = n->c;
		if (cc < 0x10)
		{
			p[0] = (int16_t)(p[0] + *(int16_t *)(T + 0x14));
			p[1] = (int16_t)(p[1] + *(int16_t *)(T + 0x16));
			p[2] = (int16_t)(p[2] + *(int16_t *)(T + 0x18));
		}
		else if (cc < 0x32)
		{
			if (cc == 0x10)
			{
				n->f20 = (int16_t)0xDCD8;
				n->f22 = 0x465;
			}
			p[1] = (int16_t)(p[1] + n->f20);
		}
		if (P1()) return 0;
		if (cc < 0x10)
		{
			*(int16_t *)(T + 0x14) = (int16_t)(*(int16_t *)(T + 0x14) + *(int16_t *)(T + 0x1C));
			*(int16_t *)(T + 0x16) = (int16_t)(*(int16_t *)(T + 0x16) + *(int16_t *)(T + 0x1E));
			*(int16_t *)(T + 0x18) = (int16_t)(*(int16_t *)(T + 0x18) + *(int16_t *)(T + 0x20));
		}
		else if (cc >= 0x2A) n->f20 = (int16_t)(n->f20 + n->f22);
		cc = (int16_t)(cc + 1);
		n->c = cc;
		if (cc < 0x32) return 0;
		*(uint16_t *)E = (uint16_t)((*(uint16_t *)E & 0xEFDF) | *(uint16_t *)T);
		*(uint32_t *)(E + 0x1C) = *(uint32_t *)(T + 0xC);
		*(uint32_t *)(E + 0x20) = *(uint32_t *)(T + 0x10);
		Node24 *m2 = AddTask(ORIG_LandDustTask);
		m2->e = n->f1A;
		*(uint32_t *)&m2->f10 = *(uint32_t *)(T + 0xC);
		*(uint32_t *)&m2->f14 = *(uint32_t *)(T + 0x10);
		*(uint32_t *)(E + 0xC) = *(uint32_t *)(T + 4);
		*(uint32_t *)(E + 0x10) = *(uint32_t *)(T + 8);
		m2->c = 0;
		m2->f1C = (int16_t)(mul32(*(int16_t *)(E + 0x26), 2000) >> 12);
		return TASK_END;
	}

	// ======================================================================
	// Landing dust (0x646F70, queued by the knock-back): pool A tagged with the node's +0x0E,
	// 8 puffs per tick for 2 ticks on a ring (radius from +0x1C) around +0x10, sliding outwards
	// (damped 1/4); lit through the battle camera (not the effect frame), 0xE34404 tinted;
	// ends on the first tick with no puff left.
	// ======================================================================
	static uint32_t __cdecl LandDustTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!P1() && n->c <= 1)
		{
			for (int k = 0; k < 8; k++)
			{
				int i = FreeRec(POOL_A, 170);
				if (i < 0) break;
				Rec *r = R(POOL_A, i);
				r->mask = (uint32_t)(int32_t)n->e;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0xC00 + 0x600);
				int32_t a = CrtRand() % 0x1000;
				int32_t sc = n->f1C;
				int32_t rr = CrtRand() % ((sc >> 1) & ~1);
				int32_t rad = rr - (sc >> 2) + sc;
				memcpy(r->pos, &n->f10, 8);
				r->pos[0] = (int16_t)(r->pos[0] + (mul32(ComputeSin(a), rad) >> 12));
				r->pos[1] = (int16_t)-(CrtRand() % 0xC8);
				r->pos[2] = (int16_t)(r->pos[2] + (mul32(ComputeCos(a), rad) >> 12));
				int32_t sp = CrtRand() % 0xBE + 0x78;
				r->vel[0] = (int16_t)(mul32(ComputeSin(a), sp) >> 12);
				r->vel[2] = (int16_t)(mul32(ComputeCos(a), sp) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		memcpy(s + 8, &Camera(), sizeof(Mat4x3));
		*(uint32_t *)h = 0xE34404;
		*(uint32_t *)(h + 0x1C) = 0x586880;
		*(uint16_t *)(h + 0x24) = 0xC;
		int32_t count = 0;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
		for (int i = 0; i < 170; i++)
		{
			Rec *r = R(POOL_A, i);
			if (!(r->mask & (uint32_t)(int32_t)n->e)) continue;
			OffsetSprite(h, (int32_t *)(s + 0x38), (int32_t *)(s + 0x28), r->pos, r->size, r->age, 3);
			g_memo_a.put(i, n, r);
			if (P1()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->pos[0] = (int16_t)(r->pos[0] + r->vel[0]);
			r->pos[2] = (int16_t)(r->pos[2] + r->vel[2]);
			int16_t vx = r->vel[0], vz = r->vel[2];
			r->vel[0] = (int16_t)(vx - (int16_t)(vx >> 2));
			r->vel[2] = (int16_t)(vz - (int16_t)(vz >> 2));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (P1()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	// ======================================================================
	// Held frames (30 fps). Everything is drawn half way between the state drawn on the last
	// real tick and the state the next tick draws; spawns, flipbook frames, colour-table steps
	// and the glow flicker keep their 15 Hz batches. The effect frame is rebuilt from the
	// held-frame camera while the timeline (its owner) is alive, into a local (never into
	// 0x24FBF90); the rig frames and the chimney bone matrix likewise (locals). Packets go to a
	// private buffer; the morph / model scratch mb+0x37000 is saved and put back.
	// ======================================================================
	static uint8_t g_held_packets[0x80000];
	static const uint32_t MORPH_SAVE = 0x10000;
	static uint8_t g_morph_save[MORPH_SAVE];
	static uint8_t g_skel_cur[SKEL_MAX], g_skel_next[SKEL_MAX];
	static Mat4x3 g_held_bone;
	static Mat4x3 g_held_w[2];

	static uint32_t LerpColor(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t r = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			r |= (uint32_t)(lerp_i((a >> k) & 0xFF, (b >> k) & 0xFF, num, den) & 0xFF) << k;
		return r;
	}

	// what the creature's next tick does with the pose: 1 = reads one frame and draws, the
	// distance changing by dz; 0 = no step (static pose, a cut, or no draw)
	static int CreatureStep(int32_t c, int32_t *dz)
	{
		*dz = 0;
		if (c >= 36 && c <= 43) { *dz = -0x190; return 1; }
		if (c >= 44 && c <= 68) { *dz = -0x258; return 1; }
		if (c >= 70 && c <= 168) return 1;
		if (c >= 170 && c <= 188) { *dz = -0x3E8; return 1; }
		if (c >= 190 && c <= 210) { *dz = -0x320; return 1; }
		if (c >= 212 && c <= 231) return 1;
		if (c >= 232 && c <= 241) { *dz = -0x708; return 1; }
		if (c >= 243 && c <= 281) { *dz = -0x12C0; return 1; }
		return 0;
	}

	// pose values half way (root offset, bone angles the short way round, bone scales)
	static void PoseBlend(uint8_t *sk, const uint8_t *next, int num, int den)
	{
		int16_t *r = (int16_t *)(sk + 8);
		const int16_t *rn = (const int16_t *)(next + 8);
		for (int k = 0; k < 3; k++) r[k] = L16(r[k], rn[k], num, den);
		for (uint32_t i = 0; i < sk[0]; i++)
		{
			int16_t *a = (int16_t *)(sk + 0x10 + 48 * i + 4);
			const int16_t *b = (const int16_t *)(next + 0x10 + 48 * i + 4);
			for (int k = 0; k < 3; k++) a[k] = lerp_angle(a[k], b[k], num, den);
			for (int k = 3; k < 6; k++) a[k] = L16(a[k], b[k], num, den);
		}
	}

	// the train: the drawn pose, advanced half a frame through the private reader (run on a
	// copy of the command; a finished animation or a phase change holds the pose), its distance
	// half way; the held chimney matrix from the same pose
	static void CreatureHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		CreatureMemo &m = g_creature;
		if (m.tick != g_real_tick) return;
		uint8_t *E = Creature();
		uint8_t *sk = Skeleton();
		if (!sk || 16 + 48 * (uint32_t)sk[0] != m.skel_size) return;
		uint32_t size = m.skel_size;
		Mat4x3 model_cur = CreatureMat();
		uint8_t cmd_cur[8];
		memcpy(cmd_cur, E + 0x6C, 8);
		memcpy(g_skel_cur, sk, size);
		memcpy(sk, m.skel, size);
		Mat4x3 model = m.model;
		int32_t dz;
		if (!P1() && CreatureStep(n->c, &dz))
		{
			uint8_t cmd[8];
			memcpy(cmd, m.cmd, 8);
			if (ReaderRead(E + 0x60, cmd) != 1)
			{
				memcpy(g_skel_next, sk, size);
				memcpy(sk, m.skel, size);
				PoseBlend(sk, g_skel_next, num, den);
				ReaderBuildBones(E + 0x60);
				model.t[2] = lerp_i(m.model.t[2], m.model.t[2] + dz, num, den);
			}
			else memcpy(sk, m.skel, size);
		}
		CreatureMat() = model;
		DrawModel(E, frame);
		ComposeAffineTransform(&model, (const Mat4x3 *)(sk + 0x50), &g_held_bone);
		CreatureMat() = model_cur;
		memcpy(E + 0x6C, cmd_cur, 8);
		memcpy(sk, g_skel_cur, size);
	}

	// lit records of an owner as drawn, moved half way when they moved on
	template<int N> static void HeldLit(const PoolMemo<N> &pm, uint32_t cell, const void *owner, uint8_t *h, bool ir, bool lerp, int num, int den)
	{
		for (int i = 0; i < N; i++)
			for (int k = 0; k < 2; k++)
			{
				const RecMemo *m = pm.get(i, k, owner);
				if (!m) continue;
				const Rec *cur = R(cell, i);
				int16_t pos[4];
				memcpy(pos, m->r.pos, 8);
				int16_t size = m->r.size;
				if (lerp && Advanced(m->r, cur))
				{
					for (int j = 0; j < 3; j++) pos[j] = L16(m->r.pos[j], cur->pos[j], num, den);
					size = L16(m->r.size, cur->size, num, den);
				}
				LitSprite(h, pos, size, m->r.age, ir);
			}
	}

	template<int N> static void HeldOffset(const PoolMemo<N> &pm, uint32_t cell, const void *owner, uint8_t *h, int32_t *mac, int32_t *nv, int shift, bool age2, bool lerp, int num, int den)
	{
		for (int i = 0; i < N; i++)
			for (int k = 0; k < 2; k++)
			{
				const RecMemo *m = pm.get(i, k, owner);
				if (!m) continue;
				const Rec *cur = R(cell, i);
				int16_t pos[4];
				memcpy(pos, m->r.pos, 8);
				int16_t size = m->r.size;
				if (lerp && Advanced(m->r, cur))
				{
					for (int j = 0; j < 3; j++) pos[j] = L16(m->r.pos[j], cur->pos[j], num, den);
					size = L16(m->r.size, cur->size, num, den);
				}
				OffsetSprite(h, mac, nv, pos, size, age2 ? (int16_t)(m->r.age << 1) : m->r.age, shift);
			}
	}

	static void PuffHeld(const Mat4x3 *frame, const Node24 *n, uint32_t seq, bool ir, int16_t w24, bool has_tint, uint32_t tint, const Mat4x3 *pre, int32_t tx, int32_t ty, int32_t tz, bool lerp, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = seq;
		*(uint16_t *)(h + 0x24) = (uint16_t)w24;
		if (has_tint) *(uint32_t *)(h + 0x1C) = tint;
		LitFrame(s, pre, frame, tx, ty, tz);
		HeldLit(g_memo_a, POOL_A, n, h, ir, lerp, num, den);
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void SteamHeld(const Mat4x3 *light, const Node24 *n, uint32_t cell, bool pool_c, bool tint, int16_t w24, int shift, bool lerp, uint32_t seq, int num, int den)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		*(uint32_t *)h = seq;
		if (tint) *(uint32_t *)(h + 0x1C) = 0x586880;
		*(uint16_t *)(h + 0x24) = (uint16_t)w24;
		memcpy(s + 0x10, light, sizeof(Mat4x3));
		GteSetLightMatrix(s + 0x10);
		GteSetBackColorFromTrans(s + 0x10);
		if (pool_c) HeldOffset(g_memo_c, cell, n, h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), shift, false, lerp, num, den);
		else HeldOffset(g_memo_d, cell, n, h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), shift, true, lerp, num, den);
		FieldFree(0x50);
		FieldFree(0xB4);
	}

	static void WheelTrailHeld(const Mat4x3 *frame, const Node24 *n)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x50);
		*(uint32_t *)h = 0xE33E40;
		*(uint16_t *)(h + 0x24) = 8;
		memcpy(s + 0x10, frame, sizeof(Mat4x3));
		GteSetLightMatrix(s + 0x10);
		GteSetBackColorFromTrans(s + 0x10);
		for (int i = 0; i < 170; i++)
			for (int k = 0; k < 2; k++)
			{
				const RecMemo *a = g_memo_a.get(i, k, n);
				if (!a) continue;
				const RecMemo *b = g_memo_b.get(i, 0, n);
				if (!b) b = g_memo_b.get(i, 1, n);
				if (!b) continue;
				WheelTrailPair(h, (int32_t *)(s + 0x40), (int32_t *)(s + 0x30), &a->r, &b->r, a->r.age, b->r.age);
			}
		FieldFree(0x50);
		FieldFree(0xB4);
	}

	static void RainHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x50) + 0x10;
		uint8_t *T = (uint8_t *)FieldAlloc(0x44);
		RainTemplate(T);
		memcpy(s, &Camera(), sizeof(Mat4x3));
		GteSetLightMatrix(s);
		GteSetBackColorFromTrans(s);
		*(uint32_t *)T = OT();
		*(uint32_t *)(T + 4) = (var<uint8_t>(0x24FBE80) & 1) ? 0xE : 4;
		uint32_t cursor = Cursor();
		for (int i = 0; i < 590; i++)
			for (int k = 0; k < 2; k++)
			{
				const RecMemo *m = g_memo_f.get(i, k, n);
				if (!m) continue;
				const Rec *cur = R(POOL_F, i);
				int16_t pos[4];
				memcpy(pos, m->r.pos, 8);
				uint32_t color = RAIN_COLOR[m->r.age & 0x3F];
				if (Advanced(m->r, cur) && cur->age < 0x40)
				{
					for (int j = 0; j < 3; j++) pos[j] = L16(m->r.pos[j], cur->pos[j], num, den);
					color = LerpColor(color, RAIN_COLOR[cur->age], num, den);
				}
				GteLoadV0(pos);
				GteMVMVA_LightV0Bk();
				RainQuad(T, cursor, color, m->r.size);
			}
		Cursor() = cursor;
		FieldFree(0x44);
		FieldFree(0x50);
	}

	static void RigHeld(const Node24 *n, uint32_t fnaddr, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		const Mat4x3 *W = &g_held_w[m->e ? 1 : 0];
		bool mv = NodeMoved(m, n);
		switch (fnaddr)
		{
		case ORIG_RigPrimATask: RigPrimADraw(W, m); break;
		case ORIG_RigPrimBTask: RigPrimBDraw(W, m); break;
		case ORIG_RigRockTask: RigRockDraw(W, m, mv ? lerp_angle(m->f18, n->f18, num, den) : m->f18); break;
		case ORIG_RigFlashTask:
		{
			int32_t f = RigFlashFade(m->c);
			if (mv && m->c >= 2) f = lerp_i(f, RigFlashFade(n->c), num, den);
			RigFlashDraw(W, m, m->c, true, f);
			break;
		}
		case ORIG_RigSpriteTask: RigSpriteDraw(W, m, m->c); break;
		}
	}

	static void SkyHeld(const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		int16_t ax = mv ? lerp_angle(m->f10, n->f10, num, den) : m->f10;
		int16_t az = mv ? lerp_angle(m->f14, n->f14, num, den) : m->f14;
		int16_t ph = mv ? lerp_angle(m->f20, n->f20, num, den) : m->f20;
		int32_t f = SkyFade(m->c);
		if (mv) f = lerp_i(f, SkyFade(n->c), num, den);
		SkyDraw(m->f18, ax, az, m->f1A, m->f1C, m->f1E, ph, f);
	}

	static void PuffWallHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint32_t grey = WallGrey(m->c);
		if (m->c >= 0x29 && NodeMoved(m, n))
		{
			uint32_t v = (uint32_t)lerp_i(shl32(0x31 - m->c, 4), shl32(0x31 - n->c, 4), num, den);
			grey = (((v << 8) | v) << 8) | v;
		}
		PuffWallSetup(h, s, frame, m->c, true, grey);
		HeldLit(g_memo_a, POOL_A, n, h, false, false, num, den);
		FieldFree(0x48);
		FieldFree(0xB4);
	}

	static void DebrisHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		bool mv = NodeMoved(m, n);
		int32_t c = m->c, d = m->f16 - 8;
		int16_t sxz = mv ? L16(m->f1C, n->f1C, num, den) : m->f1C;
		int16_t sy = mv ? L16(m->f20, n->f20, num, den) : m->f20;
		int32_t t = shl32(c, 12) / 12, f = shl32(c - d, 9);
		if (mv)
		{
			t = lerp_i(t, shl32(n->c, 12) / 12, num, den);
			if (c >= d) f = lerp_i(f, shl32(n->c - d, 9), num, den);
		}
		DebrisDraw(frame, &g_held_bone, m, sxz, sy, c >= d, f, t);
	}

	static void ExplosionHeld(const Mat4x3 *frame, const Node24 *n, int num, int den)
	{
		const Node24 *m = g_node_memo.get(n);
		if (!m) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x98);
		*(uint32_t *)h = 0xE3460C;
		*(uint16_t *)(h + 0x24) = 0x208;
		if (m->c < 5)
		{
			int16_t sz = m->f1C;
			TransformCameraByShadowRotation(&m->f10, sz, -((int32_t)sz >> 3));
			*(int16_t *)(h + 4) = m->c;
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
		}
		*(uint32_t *)h = 0xE34548;
		ExplosionFrame(s, m, frame);
		*(int32_t *)(s + 0x70) = 0x1000;
		*(int32_t *)(s + 0x68) = 0x1000;
		for (int i = 0; i < 130; i++)
			for (int k = 0; k < 2; k++)
			{
				const RecMemo *r = g_memo_e.get(i, k, n);
				if (!r) continue;
				const Rec *cur = R(POOL_E, i);
				int16_t pos[4];
				memcpy(pos, r->r.pos, 8);
				if (Advanced(r->r, cur))
					for (int j = 0; j < 3; j++) pos[j] = L16(r->r.pos[j], cur->pos[j], num, den);
				ShardDraw(h, s, pos, r->r.size, r->r.vel, r->r.age);
			}
		FieldFree(0x98);
		FieldFree(0xB4);
	}

	static void LandDustHeld(const Node24 *n, int num, int den)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		memcpy(s + 8, &Camera(), sizeof(Mat4x3));
		*(uint32_t *)h = 0xE34404;
		*(uint32_t *)(h + 0x1C) = 0x586880;
		*(uint16_t *)(h + 0x24) = 0xC;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
		HeldOffset(g_memo_a, POOL_A, n, h, (int32_t *)(s + 0x38), (int32_t *)(s + 0x28), 3, false, true, num, den);
		FieldFree(0xB4);
		FieldFree(0x48);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// the master's queue order
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = Cursor();
		Cursor() = (uint32_t)g_held_packets;
		uint8_t *morph = (uint8_t *)(MB() + 0x37000);
		memcpy(g_morph_save, morph, MORPH_SAVE);
		bool timeline = false;
		for (TaskNode *t = SubQueue().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TimelineTask) timeline = true;
		Mat4x3 frame;
		if (timeline) ComposeAffineTransform(&Camera(), &Root(), &frame);
		else frame = Frame();
		g_held_w[0] = *RigW(0);
		g_held_w[1] = *RigW(1);
		g_held_bone = BoneWorld();
		for (TaskNode *t = SubQueue().head; t; t = t->next)
		{
			const Node24 *n = (const Node24 *)t;
			uint32_t f = (uint32_t)t->func;
			switch (f)
			{
			case ORIG_RigTask:
				ComposeAffineTransform(&frame, RigM(n->e), &g_held_w[n->e ? 1 : 0]);
				break;
			case ORIG_RigPrimATask: case ORIG_RigPrimBTask: case ORIG_RigRockTask: case ORIG_RigFlashTask: case ORIG_RigSpriteTask:
				RigHeld(n, f, num, den);
				break;
			case ORIG_SkyTask: SkyHeld(n, num, den); break;
			case ORIG_SteamPuffTask: PuffHeld(&frame, n, 0xE33E40, true, 8, false, 0, nullptr, 0, 0, 0, false, num, den); break;
			case ORIG_SteamPairTask: PuffHeld(&frame, n, 0xE33E40, false, 8, false, 0, nullptr, 0, 0, 0, false, num, den); break;
			case ORIG_RainTask: RainHeld(n, num, den); break;
			case ORIG_CreatureTask: CreatureHeld(&frame, n, num, den); break;
			case ORIG_WheelTrailTask: WheelTrailHeld(&frame, n); break;
			case ORIG_PuffLineTask: PuffHeld(&frame, n, 0xE33E40, false, 8, false, 0, nullptr, 0, 0, 0, true, num, den); break;
			case ORIG_PuffWallTask: PuffWallHeld(&frame, n, num, den); break;
			case ORIG_FlareTask:
			{
				const Node24 *m = g_node_memo.get(n);
				if (!m) break;
				int32_t v = FlareScale(m->c, m->f1C);
				if (NodeMoved(m, n)) v = lerp_i(v, FlareScale(n->c, n->f1C), num, den);
				FlareDraw(&frame, m, v);
				break;
			}
			case ORIG_GlowTask:
				if (const GlowMemo *g = g_glow_memo.get(n)) GlowDraw(&frame, &g_held_bone, n, g->fade);
				break;
			case ORIG_ChimneyTask: PuffHeld(&frame, n, 0xE342A4, false, 8, false, 0, &g_held_bone, -0xB4, -0x456, -0x2454, true, num, den); break;
			case ORIG_SteamTrailTask: SteamHeld(&frame, n, POOL_C, true, true, 0xC, 4, true, 0xE342A4, num, den); break;
			case ORIG_SteamStillTask: SteamHeld(&frame, n, POOL_C, true, true, 0xC, 2, false, 0xE342A4, num, den); break;
			case ORIG_SteamBurstTask: SteamHeld(&frame, n, POOL_C, true, true, 0xC, 4, true, 0xE342A4, num, den); break;
			case ORIG_WheelSteamTask: SteamHeld(&frame, n, POOL_D, false, false, 8, 3, true, 0xE342A4, num, den); break;
			case ORIG_WheelStillTask: SteamHeld(&frame, n, POOL_D, false, false, 8, 2, false, 0xE342A4, num, den); break;
			case ORIG_WheelBurstTask: SteamHeld(&frame, n, POOL_D, false, false, 8, 2, true, 0xE342A4, num, den); break;
			case ORIG_DebrisTask: if (n->e <= 0) DebrisHeld(&frame, n, num, den); break;
			case ORIG_ExplosionTask: if (n->f16 <= 0) ExplosionHeld(&frame, n, num, den); break;
			case ORIG_LandDustTask: LandDustHeld(n, num, den); break;
			default: break;
			}
		}
		memcpy(morph, g_morph_save, MORPH_SAVE);
		Cursor() = cursor;
	}
}

	// held-frame camera of the shared camera-script task 0x63E9C0 (effects 185, 191, 199 and
	// any other ported module that queues it)
	bool camscript_held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		return d191::CsHeldCamera(num, den, world, lookat);
	}

	void register_mag191_doomtrain()
	{
		using namespace d191;
		// the shared camera-script interpreter: registered once (lookup keys on the address, so
		// it also serves Cactuar 199, Shiva 185 and any other ported module that queues it)
		register_port(ORIG_CamScriptTask, (void *)CamScriptTask, "D191 CameraScriptTask (MAG_066, shared)", 191);
		register_port(ORIG_SequenceTick, (void *)SequenceTick, "D191 SequenceTick", 191);
		register_port(ORIG_TimelineTask, (void *)TimelineTask, "D191 TimelineTask", 191);
		register_port(ORIG_FlashInTask, (void *)FlashInTask, "D191 FlashInTask", 191);
		register_port(ORIG_FlashOutTask, (void *)FlashOutTask, "D191 FlashOutTask", 191);
		register_port(ORIG_RigTask, (void *)RigTask, "D191 RigTask", 191);
		register_port(ORIG_RigPrimATask, (void *)RigPrimATask, "D191 RigPrimATask", 191, true);
		register_port(ORIG_RigPrimBTask, (void *)RigPrimBTask, "D191 RigPrimBTask", 191, true);
		register_port(ORIG_RigRockTask, (void *)RigRockTask, "D191 RigRockTask", 191, true);
		register_port(ORIG_RigFlashTask, (void *)RigFlashTask, "D191 RigFlashTask", 191, true);
		register_port(ORIG_RigSpriteTask, (void *)RigSpriteTask, "D191 RigSpriteTask", 191, true);
		register_port(ORIG_SkyTask, (void *)SkyTask, "D191 SkyTask", 191, true);
		register_port(ORIG_SteamPuffTask, (void *)SteamPuffTask, "D191 SteamPuffTask", 191, true);
		register_port(ORIG_SteamPairTask, (void *)SteamPairTask, "D191 SteamPairTask", 191, true);
		register_port(ORIG_RainTask, (void *)RainTask, "D191 RainTask", 191, true);
		register_port(ORIG_EndTask, (void *)EndTask, "D191 EndTask", 191);
		register_port(ORIG_CreatureTask, (void *)CreatureTask, "D191 CreatureTask", 191, true);
		register_port(ORIG_WheelTrailTask, (void *)WheelTrailTask, "D191 WheelTrailTask", 191, true);
		register_port(ORIG_PuffLineTask, (void *)PuffLineTask, "D191 PuffLineTask", 191, true);
		register_port(ORIG_PuffWallTask, (void *)PuffWallTask, "D191 PuffWallTask", 191, true);
		register_port(ORIG_FlareTask, (void *)FlareTask, "D191 FlareTask", 191, true);
		register_port(ORIG_GlowTask, (void *)GlowTask, "D191 GlowTask", 191, true);
		register_port(ORIG_ChimneyTask, (void *)ChimneyTask, "D191 ChimneyTask", 191, true);
		register_port(ORIG_SteamTrailTask, (void *)SteamTrailTask, "D191 SteamTrailTask", 191, true);
		register_port(ORIG_SteamStillTask, (void *)SteamStillTask, "D191 SteamStillTask", 191, true);
		register_port(ORIG_SteamBurstTask, (void *)SteamBurstTask, "D191 SteamBurstTask", 191, true);
		register_port(ORIG_WheelSteamTask, (void *)WheelSteamTask, "D191 WheelSteamTask", 191, true);
		register_port(ORIG_WheelStillTask, (void *)WheelStillTask, "D191 WheelStillTask", 191, true);
		register_port(ORIG_WheelBurstTask, (void *)WheelBurstTask, "D191 WheelBurstTask", 191, true);
		register_port(ORIG_DebrisSpawnTask, (void *)DebrisSpawnTask, "D191 DebrisSpawnTask", 191);
		register_port(ORIG_DebrisTask, (void *)DebrisTask, "D191 DebrisTask", 191, true);
		register_port(ORIG_ExplosionTask, (void *)ExplosionTask, "D191 ExplosionTask", 191, true);
		register_port(ORIG_KnockTask, (void *)KnockTask, "D191 KnockTask", 191);
		register_port(ORIG_LandDustTask, (void *)LandDustTask, "D191 LandDustTask", 191, true);
		register_module_held(191, HeldReady, HeldFrame);
		register_module_camera(191, camscript_held_camera);
	}
}
