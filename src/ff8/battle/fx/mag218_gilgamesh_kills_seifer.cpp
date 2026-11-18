/****************************************************************************/
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

// Effect 218: Gilgamesh kills Seifer (enemy attack 330, Seifer c0m128; MAG_218_*, module code
// 0x6326D0..0x637500). A timeline-B module built from Gilgamesh 327's template (master, two
// sub-queues, the model helpers are byte copies of 327's), with its own events and parts.
//
// Structure:
//   setup 0x6326D0 (MAG_218_UNK11, not ported: it runs once before any task) - texture buffer
//     0x24F3550 (Magic_TextureOFF = 0x20DFAB8), exe cells 0xE257E0 = buf + 0x2D000 (pool B,
//     200 x 0x18), 0xE257E4 = buf + 0x2E800 (pool A, 150 x 0x20), 0xE257E8 = buf + 0x2F800 (vertex
//     scratch of the model draws); context 0x24F336C, caster 0x24F3370, the first target's slot
//     0x24F2368; root queue 0x24F2418 (pool 0x24F23D8, 2 x 0x10) with the master, Q1 0x24F2518
//     (pool 0x24F2528, 100 x 0x24) with the timeline, Q2 0x24F23B8 (pool 0x24F2370, 2 x 0x24);
//     the exe TIM 0xF1E920 queued; root matrix 0x24F2348 = yaw of the target + 0x800, its
//     translation 0x24F235C/60/64 = the targets' centre + the target's matrix x (0, 0, -4500).
//     The loader 0x575550 is a bare ret (no file): the files come through BattleFile_CharacterLoad.
//   SequenceTick (master, 0x6374D0) - runs Q1 then Q2, ++counter, ends when both are empty.
//   TimelineTask (0x6328A0) - 580 ticks (counter 0..0x243): camera script 0xE25970 (shared
//     MAG_066 task), streams, file loads 0x2C1..0x2C5 + TIM uploads, sounds, the battle text lines
//     (printMonsterRelatedText), party hide / show, spawns: fade tile 0, spinners 11, flare row 24,
//     ring 32, Gilgamesh + his sword 52, tile 72, the cut 383 (QueueChainTransformation 0x37 on the
//     target, ApplyActionResultToTarget), the slash 401, texture restore + chain 1 at the end.
//   Models (battle-model blocks, 327's layout):
//     E1 = 0x24F2428  the sword, model data in the exe (0xF1DD54), drawn with E2's flags / matrix
//     E2 = 0x24F3558  Gilgamesh (file 0x2C2 at the texture buffer), matrix 0x24F3598
//   GilTask (0x633C30) - 526 ticks: the descent, the anims 0..5, the dust (0x634300) and rings
//     (0x6345E0), the shards (0x634C00: the screen grabbed into VRAM 0x240,0x100 by 0x634E40 and cut
//     into 24 shards 0x635110 drawn by the module's prim renderer 0x635280), fade to black.
// Every task tests battle_to_update_flags_dword_1D96A9C & 0x201 (draw-only when set), except the
// master and the end task 0x633BF0; the sword task 0x633AC0 ends on flag bit 1 before that test.
// The timeline returns at once on bit 0, and with bit 0x200 only while a file load is pending.
// The renderer's six lists that this effect's data never reaches (F3 0x6353C0, F4 0x635650, FT4
// 0x635C40, G3 0x635FB0, G4 0x636270, GT4 0x6368E0) are called at their original addresses; FT3
// 0x635970 and GT3 0x6365E0 are ported.

#include "fx_port.h"
#include <stddef.h>

namespace ff8fx
{
namespace g218
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
		inline void VectorNormalS16(const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const int16_t *, int16_t *)>(0x56BDE0)(in, out); }
		inline uint32_t MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { return fn<uint32_t (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
		inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b = a * b (rotation)
		inline void TransformVectorBy3x3Matrix(const Mat4x3 *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int32_t *, int32_t *)>(0x56C600)(m, in, out); }
		inline int32_t GetRotationBetweenVectors(uint32_t ref, const int32_t *v, int32_t *axis) { return fn<int32_t (__cdecl *)(uint32_t, const int32_t *, int32_t *)>(0x571480)(ref, v, axis); }
		inline void BuildAxisAngleRotationMatrix(int32_t angle, Mat4x3 *out, const int32_t *axis) { fn<void (__cdecl *)(int32_t, Mat4x3 *, const int32_t *)>(0x5714F0)(angle, out, axis); }
		inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
		inline void GetDefaultEffectPosition(void *entity, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x571400)(entity, out); }
		inline void GteReadOffset(int32_t *ofx, int32_t *ofy) { fn<void (__cdecl *)(int32_t *, int32_t *)>(0x56CD10)(ofx, ofy); }     // sub_56CD10: OFX, OFY
		inline void GteSetOffset(int32_t ofx, int32_t ofy) { fn<void (__cdecl *)(int32_t, int32_t)>(0x56CCE0)(ofx, ofy); }       // Call_Bs_ParseCamera
		// battle model
		inline int32_t PreReadAnimation(void *header, void *cmd, int32_t id) { return fn<int32_t (__cdecl *)(void *, void *, int32_t)>(0x509440)(header, cmd, id); }
		inline int32_t ReadAnimation(void *header, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(0x508F90)(header, cmd); }
		inline uint32_t RenderGeometry(void *geom, void *hdr, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(geom, hdr, ot, shift, cursor); }
		// sound / streams / files / camera script / battle
		inline void BdPlaySE(uint32_t se, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(0x501330)(se, a, b); }
		inline void BdPlaySummonStream(int32_t a, int32_t b, int32_t c) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x5018C0)(a, b, c); }
		inline void BdTransSummonStream(uint32_t src, uint32_t state) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x501860)(src, state); }
		inline int32_t VoiceSlotBusy(uint32_t slot) { return fn<int32_t (__cdecl *)(uint32_t)>(0x4A2900)(slot); }
		inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
		inline void CameraScriptStart(uint32_t script, const void *frame, void *entity, TaskQueue *q, int32_t n) { fn<void (__cdecl *)(uint32_t, const void *, void *, TaskQueue *, int32_t)>(0x63E960)(script, frame, entity, q, n); } // MAG066_CamScript_Start
		inline void ApplyActionResultToTarget(uint32_t rec) { fn<void (__cdecl *)(uint32_t)>(0x506690)(rec); }
		inline void QueueChainTransformation(void *entity, int32_t chain) { fn<void (__cdecl *)(void *, int32_t)>(0x505C00)(entity, chain); }
		inline void PrintMonsterText(uint32_t slot, int32_t line, int32_t len) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(0x4876F0)(slot, line, len); } // printMonsterRelatedText
		inline void WaitAnimSeq(uint32_t buffer, void *flag) { fn<void (__cdecl *)(uint32_t, void *)>(0x508630)(buffer, flag); }                   // Battle_RestoreDefaultEffectTexture
		inline int32_t LoadState() { return fn<int32_t (__cdecl *)()>(0x508500)(); }                                                        // sub_508500: < 0 = file load busy
		inline void CharacterLoad(int32_t id, uint32_t dst) { fn<void (__cdecl *)(int32_t, uint32_t)>(0x508480)(id, dst); }                // BattleFile_CharacterLoad
		inline void QueueTIMUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(0x505E30)(tim); }                                        // Battle_QueueTIMUpload_GetEOF
		inline uint32_t SummonData() { return fn<uint32_t (__cdecl *)()>(0x571B70)(); }                                                    // sub_571B70: 0x209FAB8
		// GPU packets (PsyQ-style)
		inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); } // SSIGPU_InsertPrimAltViewport
		inline void SetDrawEnv(uint32_t packet, uint32_t env) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C0F0)(packet, env); }           // SSIGPU_SetDrawEnv
		inline void SetDrawArea(uint32_t packet, const int16_t *rect) { fn<void (__cdecl *)(uint32_t, const int16_t *)>(0x45C940)(packet, rect); } // sub_45C940
		inline void SetDrawOffset(uint32_t packet, uint32_t xy) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C9B0)(packet, xy); }          // sub_45C9B0
		inline void SetDrawStp(uint32_t packet, int32_t on) { fn<void (__cdecl *)(uint32_t, int32_t)>(0x45C9F0)(packet, on); }
		inline uint32_t RenderScrollPrimModel(void *header, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x650720)(header, ot, mode, cursor); } // MAG_066_RenderScrollPrimModel
		// software GTE
		inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
		inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // MAG_148_sub_64DDA0: BK = m.t
		inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }
		inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }                 // MAG_163_sub_64DE00: R = s * I
		inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); } // MAG_163_sub_64DD70: TR = v
		inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }                                 // MAG_164_sub_64DE40: TR = MAC123
		inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
		inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
		inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
		inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
		inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
		inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
		inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
		inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }                   // GTE_ReadOTZ
		inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
		inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
		inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
		inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
		inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
		inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }
		inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); }
		// the renderer lists this effect's models never reach (pure functions of the header; original addresses)
		typedef uint32_t (__cdecl *ListFn)(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor);
		static const uint32_t LIST_F3 = 0x6353C0, LIST_F4 = 0x635650, LIST_FT4 = 0x635C40, LIST_G3 = 0x635FB0, LIST_G4 = 0x636270, LIST_GT4 = 0x6368E0;
	}
	using namespace x;

	// --- module globals ---
	inline bool Paused() { return (var<uint32_t>(0x1D96A9C) & 0x201) != 0; } // battle_to_update_flags_dword_1D96A9C
	inline uint32_t &VoiceSlot() { return var<uint32_t>(0x24F2340); }
	static const uint32_t STREAM_STATE = 0x24F2344;
	inline Mat4x3 &RootMatrix() { return var<Mat4x3>(0x24F2348); }  // effect frame (setup), translation 0x24F235C/60/64
	inline uint32_t TargetSlot() { return var<uint32_t>(0x24F2368); } // the first target's slot (setup)
	inline TaskQueue &Q2() { return var<TaskQueue>(0x24F23B8); }     // the model draw task
	inline Mat4x3 &Frame() { return var<Mat4x3>(0x24F23F8); }       // Camera o RootMatrix, rebuilt by the timeline every tick
	inline TaskQueue &Q1() { return var<TaskQueue>(0x24F2518); }     // timeline + effects
	inline uint8_t *CastCtx() { return var<uint8_t *>(0x24F336C); }
	inline uint32_t &Flags() { return var<uint32_t>(0x24F3544); }    // bit0 end, bit1 sword task ends, bit2 sword hidden
	inline uint32_t Tex() { return var<uint32_t>(0x24F3550); }       // texture / file buffer (0x20DFAB8)
	inline uint32_t &Cursor() { return var<uint32_t>(0x1D8E054); }   // battle_texture_data_ptr_1D8E054
	inline uint32_t RenderList() { return var<uint32_t>(0x1D8E04C); }
	inline uint32_t OT() { return RenderList() + 0x44; }
	inline uint8_t *PoolB() { return var<uint8_t *>(0xE257E0); }     // buf + 0x2D000, 200 x 0x18
	inline uint8_t *PoolA() { return var<uint8_t *>(0xE257E4); }     // buf + 0x2E800, 150 x 0x20
	inline uint32_t VertexScratch() { return var<uint32_t>(0xE257E8); } // buf + 0x2F800 (RenderGeometry scratch)
	inline uint8_t *Entity(uint32_t slot) { return (uint8_t *)(0x1D972C0 + 156 * slot); }
	static uint8_t *const E1 = (uint8_t *)0x24F2428;                 // the sword (exe model 0xF1DD54)
	static uint8_t *const E2 = (uint8_t *)0x24F3558;                 // Gilgamesh
	static uint8_t *const SHARDS = (uint8_t *)0x24F3378;             // 24 x 0x10: position, pad, velocity
	inline int16_t *Centroid16(int k) { return (int16_t *)(0x24F235C + 4 * k); } // RootMatrix.t, read as words

	static const uint32_t ORIG_SequenceTick = 0x6374D0;
	static const uint32_t ORIG_TimelineTask = 0x6328A0;
	static const uint32_t ORIG_FadeTileTask = 0x632EB0;
	static const uint32_t ORIG_SpinnerTask = 0x633090;
	static const uint32_t ORIG_FlareTask = 0x633200;
	static const uint32_t ORIG_RingTask = 0x633560;
	static const uint32_t ORIG_TileTask = 0x633680;
	static const uint32_t ORIG_GroundWaveTask = 0x633720;
	static const uint32_t ORIG_SlashTask = 0x6338B0;
	static const uint32_t ORIG_SwordTask = 0x633AC0;
	static const uint32_t ORIG_EndTask = 0x633BF0;
	static const uint32_t ORIG_GilTask = 0x633C30;
	static const uint32_t ORIG_GlowTask = 0x6340A0;
	static const uint32_t ORIG_DustTask = 0x634300;
	static const uint32_t ORIG_ShockRingTask = 0x634750;
	static const uint32_t ORIG_RisingRingTask = 0x634890;
	static const uint32_t ORIG_WideRingTask = 0x634A10;
	static const uint32_t ORIG_ShakeTask = 0x634B90;
	static const uint32_t ORIG_ShardsTask = 0x634C00;
	static const uint32_t ORIG_ShardTask = 0x635110;
	static const uint32_t ORIG_ShardRenderer = 0x635280;
	static const uint32_t ORIG_ListFT3 = 0x635970;
	static const uint32_t ORIG_ListGT3 = 0x6365E0;
	static const uint32_t ORIG_ShardDustTask = 0x636CA0;
	static const uint32_t ORIG_ShardChipTask = 0x636EB0;
	static const uint32_t ORIG_FlashTask = 0x637200;
	static const uint32_t ORIG_ModelDrawTask = 0x637350;

#pragma pack(push, 1)
	// every Q1/Q2 node is 0x24 bytes (327's layout); field use differs per task
	struct Node24
	{
		TaskNode hdr;
		int16_t c;                  // +0x0C tick counter
		int16_t e;                  // +0x0E variant / shake amplitude / shard index / done flag
		int16_t p10, p12, p14, p16; // +0x10 position (p16: shard delay)
		int16_t a18, a1A;           // +0x18 angle, spin
		int16_t s1C, s1E;           // +0x1C scale, its speed
		int16_t s20, s22;           // +0x20 second scale / height, its speed
	};
	// sprite record of pool B (0xE257E0, 0x18 bytes)
	struct RecB
	{
		uint32_t mask;
		int16_t age, size;
		int16_t x, y, z, w0E;
		int16_t vx, vy, vz, w16;
	};
	// prim record of pool A (0xE257E4, 0x20 bytes)
	struct RecA
	{
		uint32_t mask;
		int16_t age, size;
		int16_t x, y, z, w0E;
		int16_t vx, vy, vz, w16;
		int16_t ax, sx, az, sz;     // angles x / z and their spins
	};
	// shard (0x24F3378 + 0x10 * k)
	struct Shard
	{
		int16_t x, y, z, w6;
		int16_t vx, vy, vz, wE;
	};
#pragma pack(pop)
	static_assert(sizeof(Node24) == 0x24, "queue node is 0x24 bytes");
	static_assert(sizeof(RecB) == 0x18, "pool B record is 0x18 bytes");
	static_assert(sizeof(RecA) == 0x20, "pool A record is 0x20 bytes");
	static_assert(sizeof(Shard) == 0x10, "shard is 0x10 bytes");
}
}

#ifdef FF8_FX_HELD
#include "mag218_gilgamesh_kills_seifer_held.h"
#endif

namespace ff8fx
{
namespace g218
{
	static const int POOLB_N = 200, POOLA_N = 150;
	inline Shard *SH(int k) { return (Shard *)(SHARDS + 0x10 * k); }
	static void SpawnRings();

	// au_re_BdLinkTask_72 0x632E30 (Q1) / _73 0x632E50 (Q2) (no null check, as the original)
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

	// MAG_218_sub_632E70 / 632E90 (= 327's 0x5904D0 / 0x5904F0): stage groups' bit 1 (hide / show)
	static void HideStage()
	{
		for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) |= 2;
	}
	static void ShowStage()
	{
		for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) &= 0xFD;
	}

	// MAG_218_sub_634040 (= 327's 0x5904A0): rotation part = identity (translation and pad kept)
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

	// a prim-model header (Effect_RenderPrimModel 0x572200, 0x58 bytes on the scratch stack)
	static void PrimModel(uint32_t model, uint32_t flags, bool fade, int32_t level, uint32_t ot, int32_t shift)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = flags;
		if (fade) *(int32_t *)(h + 0xC) = level;
		Cursor() = RenderPrimModel(h, ot, shift, Cursor());
		FieldFree(0x58);
	}

	// a scroll prim-model header (MAG_066_RenderScrollPrimModel 0x650720, 0x90 bytes): uv scroll v,
	// 0x40 x 0x80 window, colour 0x808080 unless given
	static uint8_t *ScrollHeader(uint32_t model)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x90);
		*(uint32_t *)h = model;
		return h;
	}

	// ------------------------------------------------------------------
	// Master (0x6374D0): node from pool 0x24F23D8, +0x0C counter (increments even when paused)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SequenceTick(TaskNode *tn)
	{
		FX_HELD(held_note_master();)
		int a = ExecuteTaskQueue(&Q1());
		int b = ExecuteTaskQueue(&Q2());
		FX_HELD(held_note_tick_end();)
		((Node24 *)tn)->c++;
		return (a | b) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Screen tiles (MAG_218_sub_632F50): a 320 x 216 TILE in colour (r, g, b), semi-transparent
	// (additive draw mode 0xE1000220 after it), at OT slot otz (through the alt-viewport insert)
	// ------------------------------------------------------------------
	static void Tile(int32_t r, int32_t g, int32_t b, int32_t otz)
	{
		uint32_t *p = (uint32_t *)Cursor();
		uint32_t col = ((((((uint32_t)b & 0xFF) | 0x6200u) << 8) | ((uint32_t)g & 0xFF)) << 8) | ((uint32_t)r & 0xFF);
		p[1] = col;
		((uint16_t *)p)[4] = 0;
		((uint16_t *)p)[5] = 0;
		p[0] = 0x3000000;
		((uint16_t *)p)[6] = 0x140;
		((uint16_t *)p)[7] = 0xD8;
		InsertPrimAltViewport(RenderList() + 4 * (uint32_t)otz + 0x44, p);
		p += 4;
		p[0] = 0x1000000;
		p[1] = 0xE1000220;
		InsertPrimAltViewport(RenderList() + 4 * (uint32_t)otz + 0x44, p);
		Cursor() = (uint32_t)(p + 2);
	}

	// 0x632EB0 (tick 0), 24 ticks: white fade in over 4 ticks, out from 16 over 8
	static int32_t FadeTileLevel(int32_t c)
	{
		if (c < 4) return (mul32(c, 255) / 4) & 0xFF;
		if (c >= 16) return 0xFF - ((mul32(c - 16, 255) / 8) & 0xFF);
		return 0xFF;
	}

	static uint32_t __cdecl FadeTileTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t v = FadeTileLevel(n->c);
		Tile(v, v, v, 0);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x18 ? TASK_END : 0;
	}

	// 0x633680 (tick 72), 20 ticks: blue tile (0x70, 0x80, 0xF0) at the back of the OT (slot 0x1000)
	static uint32_t __cdecl TileTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		Tile(0x70, 0x80, 0xF0, 0x1000);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x14 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Spinners (MAG_218_sub_632FF0 at tick 11, task 0x633090), 61 ticks: two screen-space prims
	// 0xE21CC0 (x 0x400, y = +0x18 turning by +0x1A, scale +0x1C, at z = H), fade level +0x20,
	// drawn into the second OT (render list + 0x4068, shift 14); mode 0xFC for the second (+0x0E 1)
	// ------------------------------------------------------------------
	static void SpawnSpinners()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_SpinnerTask);
		n->c = 0;
		n->e = 0;
		n->a18 = (int16_t)(CrtRand() % 0x800 - 0x400);
		n->a1A = -16;
		n->s1C = 0xC80;
		n->s20 = 0x600;
		n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_SpinnerTask);
		n->c = 0;
		n->e = 1;
		n->a18 = (int16_t)(CrtRand() % 0x200);
		n->a1A = -8;
		n->s1C = 0xC00;
		n->s20 = 0xA00;
	}

	static void SpinnerDraw(const Node24 *n, int16_t a18, int16_t s1C)
	{
		int16_t ang[4] = { 0x400, a18, 0, 0 };
		Mat4x3 m;
		BuildRotationMatrixFromAngles(ang, &m);
		m.t[2] = var<int16_t>(0x1D8E038);
		m.t[0] = 0;
		m.t[1] = 0;
		int32_t s[3] = { s1C, s1C, s1C };
		Scale3DMatrix(&m, s);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = 0xE21CC0;
		*(int32_t *)(h + 0xC) = n->s20;
		*(uint32_t *)(h + 0x1C) = (n->e ? 0xCu : 3u) | 0xF0u;
		*(uint32_t *)(h + 8) = 0;
		Cursor() = RenderPrimModel(h, RenderList() + 0x4068, 0xE, Cursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl SpinnerTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		SpinnerDraw(n, n->a18, n->s1C);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->a18 = (int16_t)(n->a18 + n->a1A);
		n->c++;
		return n->c >= 0x3D ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Flare row (MAG_218_sub_6331A0 at tick 24, task 0x633200), 24 ticks: 8 flipbook sprites
	// 0xE19C14 (table 0xE257F0: frame, size, offset) along the axis from the flare origin towards
	// the camera eye (0xB8B7F0), each projected and drawn in screen space (MAG_218_sub_633440 = 327's
	// 0x593490, GTE matrix 0x24F35F8 = size * I at the screen position / 8 - (160, 108), z = H);
	// colour 0x00vvvv00 (mode 4) over the first 2 ticks and from tick 16, none in between
	// ------------------------------------------------------------------
	static void SpawnFlare()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_FlareTask);
		n->c = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xD8F0;
		n->p14 = 0;
		MatrixMultiplyVector(&RootMatrix(), &n->p10, &n->p10);
		n->p10 = (int16_t)(n->p10 + *Centroid16(0));
		n->p12 = (int16_t)(n->p12 + *Centroid16(1));
		n->p14 = (int16_t)(n->p14 + *Centroid16(2));
	}

	static void ScreenSpriteMatrix(const int16_t *v, int16_t size)
	{
		var<int16_t>(0x24F3608) = size;
		var<int16_t>(0x24F3600) = size;
		var<int16_t>(0x24F35F8) = size;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(v);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy);
		int16_t sx = (int16_t)((int32_t)sxy[0] / 8);
		int16_t sy = (int16_t)((int32_t)sxy[1] / 8);
		var<int32_t>(0x24F360C) = (int32_t)sx - 0xA0;
		var<int32_t>(0x24F3610) = (int32_t)sy - 0x6C;
		var<int32_t>(0x24F3614) = (int32_t)var<int16_t>(0x1D8E038);
		GteSetRotMatrix((const Mat4x3 *)0x24F35F8);
		GteSetTransVector((const Mat4x3 *)0x24F35F8);
	}

	// colour word of the flare at counter c; false = not written (the header keeps the scratch's word)
	static bool FlareColour(int32_t c, uint32_t *col)
	{
		int32_t v;
		if (c < 2) v = shl32(c, 6);
		else if (c >= 16) v = shl32(0x18 - c, 4);
		else return false;
		*col = (uint32_t)shl32(shl32(v, 8) | v, 8);
		return true;
	}

	static int32_t FlareA(int32_t c) { return -0x200 - mul32(c, 800); }
	static int32_t FlareB(int32_t c) { return mul32(c, 400) - 0x100; }
	static int32_t FlareDZ(int32_t c) { return -0xE00 - mul32(c, 25); }

	static void FlareDraw(const Node24 *n, int32_t c, int32_t a, int32_t b, int32_t dz)
	{
		int32_t d[3];
		d[0] = (int32_t)var<int16_t>(0xB8B7F0) - n->p10;
		d[1] = (int32_t)var<int16_t>(0xB8B7F2) - n->p12;
		d[2] = (int32_t)var<int16_t>(0xB8B7F4) - n->p14;
		int32_t ref[3] = { 0, 0, -0x1000 };
		NormalizeVectorToFixedPoint(d, d);
		int32_t axis[4];
		int32_t angle = GetRotationBetweenVectors((uint32_t)ref, d, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = n->p10;
		m.t[1] = n->p12;
		m.t[2] = n->p14;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = 0xE19C14;
		*(uint16_t *)(h + 0x24) = 0;
		uint32_t col;
		if (FlareColour(c, &col))
		{
			*(uint16_t *)(h + 0x24) = 4;
			*(uint32_t *)(h + 0x1C) = col;
		}
		for (uint32_t t = 0xE257F2; t < 0xE25832; t += 8)
		{
			int32_t k = mul32(*(const int16_t *)(t + 2), 2200) >> 12;
			int16_t v[4];
			v[0] = (int16_t)(mul32(k, a) >> 12);
			v[2] = (int16_t)(mul32(k, dz) >> 12);
			v[1] = (int16_t)(mul32(k, b) >> 12);
			MatrixMultiplyVector(&m, v, v);
			v[0] = (int16_t)(v[0] + (int16_t)m.t[0]);
			v[1] = (int16_t)(v[1] + (int16_t)m.t[1]);
			v[2] = (int16_t)(v[2] + (int16_t)m.t[2]);
			ScreenSpriteMatrix(v, *(const int16_t *)t);
			*(int16_t *)(h + 4) = *(const int16_t *)(t - 2);
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
		}
		FieldFree(0xB4);
	}

	static uint32_t __cdecl FlareTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		FlareDraw(n, n->c, FlareA(n->c), FlareB(n->c), FlareDZ(n->c));
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x18 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ring (MAG_218_sub_633500 at tick 32, task 0x633560), 16 ticks: prim 0xE1B7E0 tilted x 0x400,
	// turning (+0x18 by +0x1A), scale 2 sin(c * 0x80); rotation without the frame, translation
	// through it (camera-facing, at (0, -10000, 0) not moved by the root)
	// ------------------------------------------------------------------
	static void SpawnRing()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_RingTask);
		n->c = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xD8F0;
		n->p14 = 0;
		n->a18 = (int16_t)(CrtRand() % 0x200);
		n->a1A = (int16_t)(CrtRand() % 30 + 0x19);
	}

	static int32_t RingScale(int32_t c) { return shl32(ComputeSin(shl32(c, 11) / 16), 13) >> 12; }

	static void RingDraw(const Node24 *n, int16_t a18, int32_t v)
	{
		int16_t ang[4] = { 0x400, a18, 0, 0 };
		Mat4x3 m;
		BuildRotationMatrixFromAngles(ang, &m);
		m.t[0] = n->p10;
		m.t[2] = n->p14;
		m.t[1] = n->p12;
		int32_t s[3] = { v, v, v };
		Scale3DMatrix(&m, s);
		Mat4x3 w;
		ComposeAffineTransform(&Frame(), &m, &w);
		GteSetRotMatrix(&m);
		GteSetTransVector(&w);
		PrimModel(0xE1B7E0, 0x33, false, 0, OT(), 2);
	}

	static uint32_t __cdecl RingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RingDraw(n, n->a18, RingScale(n->c));
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->a18 = (int16_t)(n->a18 + n->a1A);
		n->c++;
		return n->c >= 0x10 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ground wave (MAG_218_sub_6336D0 at tick 383, task 0x633720), 18 ticks: scroll prim 0xE22570
	// at the target's default effect position (+400 z, y 0), turned y 0x800, scaled
	// (0xC00, 0x1E00, 0x1800), uv scroll 24 c, colour 0x808080 depth-cued towards (0x10, 0x10, 0x90)
	// by c / 18
	// ------------------------------------------------------------------
	static void SpawnGroundWave()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_GroundWaveTask);
		n->c = 0;
		GetDefaultEffectPosition(Entity(TargetSlot()), &n->p10);
		n->p14 = (int16_t)(n->p14 + 0x190);
		n->p12 = 0;
	}

	static int32_t GroundWaveIR0(int32_t c) { return shl32(c, 12) / 18; }

	static void GroundWaveDraw(const Node24 *n, int32_t uv, int32_t ir0)
	{
		int16_t ang[4] = { 0, 0x800, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		int32_t s[3] = { 0xC00, 0x1E00, 0x1800 };
		m.t[0] = n->p10;
		m.t[1] = n->p12;
		m.t[2] = n->p14;
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = ScrollHeader(0xE22570);
		*(uint16_t *)(h + 0x1A) = 0x80;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint16_t *)(h + 0x18) = 0x40;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(int32_t *)(h + 0x14) = uv;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x20) = 0x33;
		*(uint32_t *)(h + 0x10) = 0;
		GteSetFarColor(0x10, 0x10, 0x90);
		uint32_t rgbc = 0x808080;
		GteLoadRGBC(&rgbc);
		GteSetIR0(ir0);
		GteDPCS();
		GteStoreRGB2(h + 0x24);
		Cursor() = RenderScrollPrimModel(h, OT(), 2, Cursor());
		FieldFree(0x90);
	}

	static uint32_t __cdecl GroundWaveTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		GroundWaveDraw(n, mul32(n->c, 24), GroundWaveIR0(n->c));
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x12 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Slash (0x6338B0, tick 401), 22 ticks: scroll prim 0xE23660 at the target's bone 0xF0 (written
	// into the node every tick), turned from (0x24FD358 = the camera script's saved eye, Doomtrain's
	// range) towards it, placed 2000 / 4096 of the way back towards that eye, scaled (0x1800,
	// 0x1800, 0x3400), uv scroll -24 c
	// ------------------------------------------------------------------
	static void SlashDraw(const int16_t *p, const int16_t *eye, int32_t uv)
	{
		const int32_t ex = eye[0], ey = eye[1], ez = eye[2];
		int32_t d[3];
		d[0] = ex - p[0];
		d[1] = ey - p[1];
		d[2] = ez - p[2];
		NormalizeVectorToFixedPoint(d, d);
		int32_t axis[4];
		int32_t angle = GetRotationBetweenVectors(0xE25840, d, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = (mul32(-2000, d[0]) >> 12) + ex;
		m.t[1] = (mul32(-2000, d[1]) >> 12) + ey;
		m.t[2] = (mul32(-2000, d[2]) >> 12) + ez;
		int32_t s[3] = { 0x1800, 0x1800, 0x3400 };
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = ScrollHeader(0xE23660);
		*(uint16_t *)(h + 0x1A) = 0x80;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint16_t *)(h + 0x18) = 0x40;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(int32_t *)(h + 0x14) = uv;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x20) = 0x33;
		*(uint32_t *)(h + 0x10) = 0;
		*(uint32_t *)(h + 0x24) = 0x808080;
		Cursor() = RenderScrollPrimModel(h, OT(), 2, Cursor());
		FieldFree(0x90);
	}

	static uint32_t __cdecl SlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		GetEffectSpawnPosition(Entity(TargetSlot()), 0xF0, 0x1000, &n->p10);
		SlashDraw(&n->p10, (const int16_t *)0x24FD358, -mul32(n->c, 24));
		FX_HELD(MemoExtra((const int16_t *)0x24FD358, 3); MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x16 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Sword (0x633AC0, tick 52): prim 0xE254D8 on E1's bone 0 (E1 matrix 0x24F2468 o bone matrix
	// +0x50), turned x -0x400, scaled 0x5400, through the frame; drawn from counter 1 unless flag
	// bit 2; ends at once when flag bit 1 is set (before the draw-only test)
	// ------------------------------------------------------------------
	// the sword's matrix before the frame (E1 matrix o bone matrix +0x50 o turn / scale)
	static void SwordLocal(Mat4x3 *m)
	{
		int16_t ang[4] = { (int16_t)0xFC00, 0, 0, 0 };
		ComposeZYXRotationMatrix(ang, m);
		int32_t s[3] = { 0x5400, 0x5400, 0x5400 };
		m->t[0] = 0;
		m->t[1] = 0;
		m->t[2] = 0;
		Scale3DMatrix(m, s);
		Mat4x3 b;
		ComposeAffineTransform((const Mat4x3 *)(E1 + 0x40), (const Mat4x3 *)(**(uint32_t **)(E1 + 0x64) + 0x50), &b);
		ComposeAffineTransform(&b, m, m);
	}

	static uint32_t __cdecl SwordTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (Flags() & 2) return TASK_END;
		Mat4x3 m;
		SwordLocal(&m);
		FX_HELD(Mat4x3 local = m;)
		ComposeAffineTransform(&Frame(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		if (n->c > 0 && !(Flags() & 4)) PrimModel(0xE254D8, 0x33, false, 0, OT(), 3);
		FX_HELD(MemoMat(&local, n->c > 0 && !(Flags() & 4)); MemoNode(n);)
		if (!Paused()) n->c++;
		return 0;
	}

	// 0x633BF0: end of effect, restores the effect texture (anim-seq task) and waits for +0x0E
	static uint32_t __cdecl EndTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 1) WaitAnimSeq(Tex() + 0x11000, &n->e);
		uint16_t done = (uint16_t)n->e;
		n->c++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Models. Block E (0x9C bytes, battle-entity head layout, 327's): +0x00 u16 flags (bit0 no anim
	// loop, bit2 not drawn, bit3 not advanced), +0x28 colour, +0x40 model matrix, +0x60
	// BattleAnimHeader, +0x64 comFileData, +0x6C BattleAnimCmd, +0x7C, +0x84 model data.
	// ------------------------------------------------------------------
	// MAG_218_sub_633FB0 (= 327's 0x595150): bind the model data (model file offsets table) to the block
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

	// au_re_Battle_ReadAnimation_4 0x634020 (= 327's 0x5951C0)
	static void SetAnim(uint8_t *E, int32_t id) { PreReadAnimation(E + 0x60, E + 0x6C, id); }

	// MAG_218_sub_6373F0 (= 327's AdvanceModelAnimLoop 0x595E00)
	static void AdvanceModel(uint8_t *E)
	{
		if (ReadAnimation(E + 0x60, E + 0x6C) == 1 && !(E[0] & 1))
			PreReadAnimation(E + 0x60, E + 0x6C, E[0x6C]);
		FX_HELD(held_note_advanced(E);)
	}

	// vertex scratch of the model draws: the engine's (0xE257E8) unless another one is set here
	static uint32_t g_vertex_scratch_override = 0;
	static uint32_t ModelScratch() { return g_vertex_scratch_override ? g_vertex_scratch_override : VertexScratch(); }

	// MAG_218_sub_637430 (= 327's DrawModel 0x595E40, shift 4, vertex scratch 0xE257E8)
	static void DrawModel(uint8_t *E, const Mat4x3 *frame)
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
		Cursor() = RenderGeometry(*(void **)(E + 0x64), h + 0x20, OT(), 4, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
		FieldFree(0x4C);
	}

	// 0x637350 (Q2, tick 52), 526 ticks: E1 takes E2's flags, matrix and the fade colour 0x24F3580;
	// both drawn (sword first) unless bit 2, advanced unless bit 3
	static uint32_t __cdecl ModelDrawTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint16_t fl = *(uint16_t *)E2;
		*(uint16_t *)E1 = fl;
		if (!(fl & 4))
		{
			ff8fx::copy<(0x20)>(E1 + 0x40, E2 + 0x40);
			*(uint32_t *)(E1 + 0x28) = var<uint32_t>(0x24F3580);
			DrawModel(E1, &Frame());
			DrawModel(E2, &Frame());
			fl = *(uint16_t *)E2;
			FX_HELD(ModelMemoTake(n);)
		}
		if (Paused()) return 0;
		if (!(fl & 8))
		{
			AdvanceModel(E1);
			AdvanceModel(E2);
		}
		n->c++;
		return n->c >= 0x20E ? TASK_END : 0;
	}

	// au_re_BdLinkTask_74 0x634070: the sword glow
	static void LinkGlow()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_GlowTask);
		n->c = 0;
		n->s1C = 0x2400;
		n->s20 = 0x1E00;
	}

	// 0x633C30 (tick 52), 526 ticks: Gilgamesh. Binds E2 (file buffer, id 0xF) and E1 (exe model
	// 0xF1DD54, id 0x10) on counter 0 (also while paused), then: descends from y -10000 (speed +0x20
	// growing by 1/8, z by +0x22, scale +0x1C from 0x140 growing by 1/8) for 20 ticks, stands at
	// y -5000 for 20, the anims 1..5 with the dust and rings (45), the shards (342) and the flash
	// (357); hidden by flag bit 3 between 476 and 566 (E2), sword task end at 567, fade of the
	// colour 0x24F3580 towards black from 570; flag bit 0 at the end
	// ------------------------------------------------------------------
	static uint32_t __cdecl GilTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		Mat4x3 *M2 = (Mat4x3 *)(E2 + 0x40);
		if (n->c == 0)
		{
			ResetRotation(M2);
			var<uint16_t>(0x24F35A8) = 0x1000;
			var<uint16_t>(0x24F35A0) = 0x1000;
			var<uint16_t>(0x24F3598) = 0x1000;
			M2->t[1] = 0;
			M2->t[0] = 0;
			M2->t[2] = 0;
			ModelInit(E2, (uint8_t *)0x24F3500, (uint8_t *)Tex(), 0xF);
			ff8fx::copy<(0x20)>(E1 + 0x40, M2);
			ModelInit(E1, (uint8_t *)0x24F3338, (uint8_t *)0xF1DD54, 0x10);
		}
		if (Paused()) return 0;
		int32_t d = n->c;
		if (d < 0x14)
		{
			if (d == 0)
			{
				SetAnim(E1, 0);
				SetAnim(E2, 0);
				M2->t[1] = -10000;
				n->s1C = 0x140;
				n->s20 = 0x50;
				n->s22 = -8;
			}
			int16_t v = n->s20;
			M2->t[1] += v;
			n->s20 = (int16_t)((int16_t)(v >> 3) + v);
			int16_t w = n->s22;
			M2->t[2] += w;
			n->s22 = (int16_t)((int16_t)(w >> 3) + w);
			int16_t s = n->s1C;
			var<int16_t>(0x24F35A8) = s;
			var<int16_t>(0x24F35A0) = s;
			var<int16_t>(0x24F3598) = s;
			n->s1C = (int16_t)((int16_t)(s >> 3) + s);
			if (d == 0) LinkGlow();
		}
		else if ((d -= 0x14) < 0x14)
		{
			if (d == 0)
			{
				M2->t[1] = -5000;
				M2->t[2] = 0;
			}
			var<int16_t>(0x24F35A8) = 0x1000;
			var<int16_t>(0x24F35A0) = 0x1000;
			var<int16_t>(0x24F3598) = 0x1000;
		}
		else if ((d -= 0x14) < 0x1A)
		{
			if (d == 0)
			{
				SetAnim(E1, 1);
				SetAnim(E2, 1);
				M2->t[1] = 0;
			}
			else if (d == 5)
			{
				LinkQ1(ORIG_DustTask);
				SpawnRings();
			}
		}
		else if ((d -= 0x1A) < 0xF4)
		{
			if (d == 0)
			{
				SetAnim(E1, 2);
				SetAnim(E2, 2);
			}
		}
		else if ((d -= 0xF4) < 0x15)
		{
			if (d == 0)
			{
				SetAnim(E1, 3);
				SetAnim(E2, 3);
				LinkQ1(ORIG_ShardsTask);
			}
			else if (d == 0xF) LinkQ1(ORIG_FlashTask);
		}
		else if ((d -= 0x15) < 0x28)
		{
		}
		else if ((d -= 0x28) < 0x1E)
		{
			if (d == 0)
			{
				SetAnim(E1, 4);
				SetAnim(E2, 4);
			}
		}
		else if ((d -= 0x1E) < 0x7D)
		{
			if (d == 0)
			{
				SetAnim(E1, 5);
				SetAnim(E2, 5);
			}
			else if (d == 0xF) E2[0] |= 8;
			else if (d == 0x69) *(uint16_t *)E2 &= 0xFFF7;
			else if (d == 0x6A) Flags() |= 2;
			else if (d >= 0x6D)
			{
				GteSetFarColor(0, 0, 0);
				uint32_t rgbc = 0x2606060;
				GteLoadRGBC(&rgbc);
				GteSetIR0(shl32(d - 0x6D, 12) / 16);
				GteDPCS();
				GteStoreRGB2((void *)0x24F3580);
			}
		}
		FX_HELD(MemoNode(n);)
		n->c++;
		if (n->c < 0x20E) return 0;
		Flags() |= 1;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Sword glow (au_re_BdLinkTask_74 at the GilTask's counter 0, task 0x6340A0), 40 ticks: scroll
	// prim 0xE1A0F0 at E2's bone 0xF0 (Gilgamesh's hand), moved 600 / 4096 along the direction from
	// the midpoint of bones 0x2E / 0x34 (frac 0xC00) to it, turned to that direction (reference
	// 0xE25830), scaled (+0x1C, +0x20, +0x1C) x E2's scale, uv scroll -32 c, fade 0x400
	// ------------------------------------------------------------------
	// A = the midpoint of bones 0x2E / 0x34, C = bone 0xF0 (words 0..2)
	static void GlowPoints(int16_t *A, int16_t *C)
	{
		int16_t B[4];
		GetEffectSpawnPosition(E2, 0x2E, 0xC00, A);
		GetEffectSpawnPosition(E2, 0x34, 0xC00, B);
		GetEffectSpawnPosition(E2, 0xF0, 0, C);
		A[0] = (int16_t)(((int32_t)B[0] + A[0]) / 2);
		A[1] = (int16_t)(((int32_t)B[1] + A[1]) / 2);
		A[2] = (int16_t)(((int32_t)B[2] + A[2]) / 2);
	}

	static void GlowDraw(const Node24 *n, const int16_t *A, const int16_t *C, int32_t k, int32_t uv)
	{
		int32_t d[3] = { (int32_t)C[0] - A[0], (int32_t)C[1] - A[1], (int32_t)C[2] - A[2] };
		NormalizeVectorToFixedPoint(d, d);
		int32_t axis[4];
		int32_t angle = GetRotationBetweenVectors(0xE25830, d, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = (mul32(d[0], 600) >> 12) + C[0];
		m.t[1] = (mul32(d[1], 600) >> 12) + C[1];
		m.t[2] = (mul32(d[2], 600) >> 12) + C[2];
		int32_t s[3];
		s[0] = mul32(n->s1C, k) >> 12;
		s[2] = s[0];
		s[1] = mul32(n->s20, k) >> 12;
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Frame(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = ScrollHeader(0xE1A0F0);
		*(int32_t *)(h + 0x14) = uv;
		*(int32_t *)(h + 0xC) = 0x400;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x20) = 0xF3;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint32_t *)(h + 0x10) = 0;
		*(uint32_t *)(h + 0x24) = 0x808080;
		Cursor() = RenderScrollPrimModel(h, OT(), 2, Cursor());
		FieldFree(0x90);
	}

	static uint32_t __cdecl GlowTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t A[4], C[4];
		GlowPoints(A, C);
		GlowDraw(n, A, C, var<int16_t>(0x24F3598), shl32(-n->c, 5));
		FX_HELD(MemoExtra(A, 3); MemoExtra2(C, 3, var<int16_t>(0x24F3598)); MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x28 ? TASK_END : 0;
	}

	// first free record (dword 0) of a pool, -1 when none
	static int FreeRecord(uint8_t *pool, int stride, int count)
	{
		for (int i = 0; i < count; i++)
			if (*(uint32_t *)(pool + stride * i) == 0) return i;
		return -1;
	}

	// ------------------------------------------------------------------
	// Dust (0x634300, GilTask 45): pool B mask 1, 8 per tick while the counter <= 4 on a ring of
	// 400..700 around the frame origin, moving out (speed / 8 drag); flipbook 0xE19CBC (mode 8)
	// lit through the frame, pushed back towards the camera by size / 8; ends when none is left
	// ------------------------------------------------------------------
	static void SpritePush(uint8_t *s, int16_t size)
	{
		int32_t *v = (int32_t *)(s + 0x38), *u = (int32_t *)(s + 0x28);
		GteReadMAC123(v);
		NormalizeVectorToFixedPoint(v, u);
		int32_t k = -((int32_t)size >> 3);
		v[0] += mul32(u[0], k) >> 12;
		v[1] += mul32(u[1], k) >> 12;
		v[2] += mul32(u[2], k) >> 12;
		GteSetTransFromVec32(v);
	}

	static void DustSetup(uint8_t *h, uint8_t *s)
	{
		*(uint16_t *)(h + 0x24) = 8;
		ff8fx::copy<(0x20)>(s + 8, &Frame());
		*(uint32_t *)h = 0xE19CBC;
	}

	static void DustOne(uint8_t *h, uint8_t *s, const RecB *p)
	{
		GteLoadV0(&p->x);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(p->size);
		*(int16_t *)(h + 4) = p->age;
		SpritePush(s, p->size);
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static uint32_t __cdecl DustTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 4)
		{
			for (int k = 0; k < 8; k++)
			{
				int i = FreeRecord(pool, 0x18, POOLB_N);
				if (i < 0) break;
				RecB *p = (RecB *)(pool + 0x18 * i);
				p->mask = 1;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0xC00 + 0xA00);
				int32_t a = CrtRand() % 0x1000;
				int32_t r = CrtRand() % 300 + 400;
				p->x = (int16_t)(mul32(ComputeSin(a), r) >> 12);
				p->y = (int16_t)-(CrtRand() % 60);
				p->z = (int16_t)(mul32(ComputeCos(a), r) >> 12);
				int32_t r2 = CrtRand() % 120 + 40;
				p->vx = (int16_t)(mul32(ComputeSin(a), r2) >> 12);
				p->vz = (int16_t)(mul32(ComputeCos(a), r2) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		DustSetup(h, s);
		int live = 0;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
		for (int i = 0; i < POOLB_N; i++)
		{
			RecB *p = (RecB *)(pool + 0x18 * i);
			if (!(p->mask & 1)) continue;
			DustOne(h, s, p);
			FX_HELD(PoolMemoB(i, p);)
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
			live++;
		}
		FX_HELD(MemoNode(n);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return live ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Rings (MAG_218_sub_6345E0, GilTask 45): a shock ring, two rising rings, a wide ring (frame
	// origin) and the camera shake (amplitude 40)
	// ------------------------------------------------------------------
	static void SpawnRings()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_ShockRingTask);
		n->c = 0;
		n->p14 = 0;
		n->p12 = 0;
		n->p10 = 0;
		n->s1E = 0x900;
		n->s1C = 0x900;
		for (int k = 0; k < 2; k++)
		{
			n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_RisingRingTask);
			n->c = 0;
			n->p14 = 0;
			n->p12 = 0;
			n->p10 = 0;
			n->a18 = (int16_t)(CrtRand() % 0x800 - 0x400);
			int32_t r = CrtRand() % 90;
			n->s1E = 0xB80;
			n->s1C = 0xB80;
			n->a1A = (int16_t)(-30 - r);
			int32_t v = (CrtRand() % 0x1000 + 0x1600) / 10;
			n->s22 = (int16_t)v;
			n->s20 = (int16_t)v;
		}
		n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_WideRingTask);
		n->c = 0;
		n->p14 = 0;
		n->p12 = 0;
		n->p10 = 0;
		n->a18 = (int16_t)(CrtRand() % 0x400 - 0x200);
		int32_t r = CrtRand() % 120;
		n->s1E = 0x740;
		n->s1C = 0x740;
		n->s22 = 0x3AA;
		n->s20 = 0x3AA;
		n->a1A = (int16_t)(r + 0xF);
		n = (Node24 *)AddTaskToQueue(&Q1(), ORIG_ShakeTask);
		n->c = 0;
		n->e = 0x28;
	}

	// a frame-relative prim (angles (0, y, 0), translation p, scale (sx, sy, sx)), faded when asked
	static void RingPrim(const Node24 *n, uint32_t model, int16_t ay, int32_t sx, int32_t sy, bool fade, int32_t level)
	{
		int16_t ang[4] = { 0, ay, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = n->p10;
		m.t[1] = n->p12;
		m.t[2] = n->p14;
		int32_t s[3] = { sx, sy, sx };
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Frame(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimModel(model, fade ? 0xF3 : 0x33, fade, level, OT(), 2);
	}

	// 0x634750, 14 ticks: prim 0xE1EF08, scale +0x1C (speed +0x1E, drag 1/5), fade from 6 (512 per tick)
	static uint32_t __cdecl ShockRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RingPrim(n, 0xE1EF08, 0, n->s1C, n->s1C, n->c >= 6, shl32(n->c - 6, 9));
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->s1E = (int16_t)(v - (int16_t)(v / 5));
		n->c++;
		return n->c >= 0xE ? TASK_END : 0;
	}

	// 0x634890, 22 ticks: prim 0xE1C918 turning (+0x18 by +0x1A), radius +0x1C (drag 1/6), height
	// +0x20 (drag 1/18), fade from 10 (341 per tick)
	static uint32_t __cdecl RisingRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RingPrim(n, 0xE1C918, n->a18, n->s1C, n->s20, n->c >= 10, mul32(n->c - 10, 341));
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->a18 = (int16_t)(n->a18 + n->a1A);
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->s1E = (int16_t)(v - (int16_t)(v / 6));
		int16_t w = n->s22;
		n->s20 = (int16_t)(n->s20 + w);
		n->s22 = (int16_t)(w - (int16_t)(w / 18));
		n->c++;
		return n->c >= 0x16 ? TASK_END : 0;
	}

	// 0x634A10, 16 ticks: prim 0xE1E260, as the rising rings with drags 1/5 and 1/8, fade from 10 (682)
	static uint32_t __cdecl WideRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RingPrim(n, 0xE1E260, n->a18, n->s1C, n->s20, n->c >= 10, mul32(n->c - 10, 682));
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->a18 = (int16_t)(n->a18 + n->a1A);
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->s1E = (int16_t)(v - (int16_t)(v / 5));
		int16_t w = n->s22;
		n->s20 = (int16_t)(n->s20 + w);
		n->s22 = (int16_t)(w - (int16_t)(w / 8));
		n->c++;
		return n->c >= 0x10 ? TASK_END : 0;
	}

	// 0x634B90, 8 ticks: camera shake word 0x1D97712 = +-(sin(c * 0x80 + 0x400) * amplitude) (written
	// also on draw-only ticks)
	static int16_t ShakeValue(int32_t c, int32_t amp)
	{
		int32_t v = mul32(ComputeSin(shl32(c, 10) / 8 + 0x400), amp) >> 12;
		if (!(c & 1)) v = -v;
		return (int16_t)v;
	}

	static uint32_t __cdecl ShakeTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		var<int16_t>(0x1D97712) = ShakeValue(n->c, n->e);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 8 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Screen grab (MAG_218_sub_634E40, the shards' counter 0): the stage group 1 (0x1D989B8) is
	// drawn into the VRAM area (0x240, 0x100, 0x80 x 0x100) through its own draw env (0x24F24D8),
	// GTE offset (0x40, 0x80) and a matrix squeezing the view onto it; then the exe packet 0xE25AD8,
	// stp off (0x24F3538), the draw area (0x24F23C8) and offset (0x24F24C8, 0x24F34F8) of that area.
	// (OT insertions are LIFO: the env / area / offset packets inserted last run first.)
	// ------------------------------------------------------------------
	// the packets the grab inserts: the module's (real ticks) or copies (held frames)
	struct GrabPackets { uint32_t env, exe, stp, area, ofs, xy; };
	static const GrabPackets GRAB_MODULE = { 0x24F24D8, 0xE25AD8, 0x24F3538, 0x24F23C8, 0x24F24C8, 0x24F34F8 };

	static void ScreenGrab(const int16_t *pos, const GrabPackets &pk)
	{
		int16_t rect[4] = { 0x240, 0x100, 0x80, 0x100 };
		Mat4x3 m1, m2;
		m1.t[0] = 4 - (int32_t)pos[0] / 17;
		m1.t[1] = (int32_t)pos[2] / 17 - 3;
		m1.t[2] = var<int16_t>(0x1D8E038);
		int16_t ang[4] = { 0x400, 0, 0, 0 };
		BuildRotationMatrixFromAngles(ang, &m1);
		int32_t s[3];
		s[2] = 0x1000000 / (0x10A4000 / rect[3]);
		s[1] = 0;
		s[0] = 0x1000000 / (0x820000 / rect[2]);
		Scale3DMatrix(&m1, s);
		int16_t ang2[4] = { 0, 0, (int16_t)0xFC00, 0 };
		BuildRotationMatrixFromAngles(ang2, &m2);
		MatrixMultiply(&m2, &m1);
		TransformVectorBy3x3Matrix(&m2, m1.t, m1.t);
		SetDrawEnv(pk.env, 0x1D969C8 + 0x5C * (var<uint8_t>(0x1D96A80) ^ 1u));
		InsertPrimAutoDepth(OT(), (void *)pk.env);
		int32_t ofx, ofy;
		GteReadOffset(&ofx, &ofy);
		GteSetOffset(rect[2] / 2, rect[3] / 2);
		GteSetRotMatrix(&m1);
		GteSetTransVector(&m1);
		uint8_t *h = (uint8_t *)FieldAlloc(0x2C);
		*(int16_t *)(h + 0x24) = var<int16_t>(0x1D989BE);
		*(int32_t *)(h + 0x10) = -1;
		*(int32_t *)(h + 0x20) = -1;
		*(uint16_t *)(h + 0x14) = 0;
		*(uint16_t *)(h + 0x16) = 0;
		*(uint16_t *)(h + 0x18) = 0x140;
		*(uint16_t *)(h + 0x1A) = 0xD8;
		*(uint32_t *)(h + 0x1C) = var<uint32_t>(0x1D989E4);
		*(uint32_t *)(h + 4) = var<uint32_t>(0x1D98B3C);
		if (var<uint8_t>(0x1D989BD) & 1)
		{
			BuildBoneMatricesFromPose((void *)0x1D989D0);
			ComputeBonesWorldMatrices((void *)0x1D989D0, &m1);
			Cursor() = RenderGeometry((void *)0x1D989C0, h, OT(), 0x10, Cursor());
		}
		FieldFree(0x2C);
		GteSetOffset(ofx, ofy);
		InsertPrimAltViewport(OT(), (void *)pk.exe);
		SetDrawStp(pk.stp, 0);
		InsertPrimAutoDepth(OT(), (void *)pk.stp);
		SetDrawArea(pk.area, rect);
		InsertPrimAutoDepth(OT(), (void *)pk.area);
		var<int16_t>(pk.xy) = rect[0];
		var<int16_t>(pk.xy + 2) = rect[1];
		SetDrawOffset(pk.ofs, pk.xy);
		InsertPrimAutoDepth(OT(), (void *)pk.ofs);
	}

	// ------------------------------------------------------------------
	// Shards (0x634C00, GilTask 342), 21 ticks: counter 0 the screen grab at the frame point
	// (0, 0, -700); counter 15 the 24 shards (table 0xE258B0: offset, per-shard model 0xE25850),
	// thrown from the target's bone 0xF1 (+100 y) with a random up kick (250..600), delay k / 4;
	// counter 16 the shard dust and chips
	// ------------------------------------------------------------------
	static uint32_t __cdecl ShardsTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			n->p12 = 0;
			n->p10 = 0;
			n->p14 = (int16_t)0xFD44;
			MatrixMultiplyVector(&RootMatrix(), &n->p10, &n->p10);
			n->p10 = (int16_t)(n->p10 + *Centroid16(0));
			n->p12 = (int16_t)(n->p12 + *Centroid16(1));
			n->p14 = (int16_t)(n->p14 + *Centroid16(2));
			ScreenGrab(&n->p10, GRAB_MODULE);
		}
		if (n->c == 0xF)
		{
			int16_t P[4];
			GetEffectSpawnPosition(Entity(TargetSlot()), 0xF1, 0, P);
			P[1] = (int16_t)(P[1] + 0x64);
			int16_t V[4];
			V[0] = 0;
			V[1] = 0;
			V[2] = 0;
			MatrixMultiplyVector(&RootMatrix(), V, V);
			V[0] = (int16_t)(V[0] + *Centroid16(0));
			V[1] = (int16_t)(V[1] + *Centroid16(1));
			V[2] = (int16_t)(V[2] + *Centroid16(2));
			for (int k = 0; k < 24; k++)
			{
				Node24 *t = (Node24 *)AddTaskToQueue(&Q1(), ORIG_ShardTask);
				t->c = 0;
				t->p16 = (int16_t)(k / 4);
				Shard *sh = SH(k);
				*(uint32_t *)&sh->x = *(uint32_t *)&n->p10;
				*(uint32_t *)&sh->z = *(uint32_t *)&n->p14;
				const int16_t *tb = (const int16_t *)(0xE258B2 + 8 * k);
				sh->x = (int16_t)(sh->x + tb[-1]);
				sh->y = (int16_t)(sh->y + tb[0]);
				sh->z = (int16_t)(sh->z + tb[1]);
				t->e = (int16_t)k;
				t->s1C = 0x1000;
				int32_t D[3];
				D[0] = (sh->x / 3) * 2;
				int32_t r = CrtRand() % 800;
				D[1] = (r + P[1]) * 2 - 0x320;
				D[2] = (int32_t)P[2] - V[2];
				NormalizeVectorToFixedPoint(D, D);
				int32_t m = CrtRand() % 350 + 250;
				sh->vx = (int16_t)(mul32(m, D[0]) >> 12);
				sh->vy = (int16_t)(mul32(m, D[1]) >> 12);
				sh->vz = (int16_t)(mul32(m, D[2]) >> 12);
			}
		}
		if (n->c == 0x10)
		{
			((Node24 *)AddTaskToQueue(&Q1(), ORIG_ShardDustTask))->c = 0;
			((Node24 *)AddTaskToQueue(&Q1(), ORIG_ShardChipTask))->c = 0;
		}
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x15 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// The module's prim renderer (MAG_218_sub_635280: the Effect_RenderPrimModel dispatch with a
	// per-model vertex offset h+0x58 subtracted from every vertex): header 0x60, eight lists
	// F3 F4 FT3 FT4 G3 G4 GT3 GT4 (count, records). FT3 / GT3 ported; the six others called at
	// their original addresses (this effect's shard models have none of them).
	// ------------------------------------------------------------------
	static void ListVertices(uint8_t *h, const uint8_t *rec)
	{
		const uint8_t *vb = *(uint8_t **)(h + 4);
		static const int OFS[3] = { 0x34, 0x3C, 0x44 };
		const uint16_t idx[3] = { *(const uint16_t *)(rec + 4), *(const uint16_t *)(rec + 6), *(const uint16_t *)(rec + 8) };
		for (int k = 0; k < 3; k++)
		{
			int16_t *v = (int16_t *)(h + OFS[k]);
			*(uint32_t *)v = *(const uint32_t *)(vb + 4 * idx[k]);
			*(uint32_t *)(v + 2) = *(const uint32_t *)(vb + 4 * idx[k] + 4);
			v[0] = (int16_t)(v[0] - *(int16_t *)(h + 0x58));
			v[1] = (int16_t)(v[1] - *(int16_t *)(h + 0x5A));
			v[2] = (int16_t)(v[2] - *(int16_t *)(h + 0x5C));
		}
		GteLoadV012(h + 0x34, h + 0x3C, h + 0x44);
		GteRTPT();
	}

	// screen-bound flags of three SXY (0xA00 x 0x6C0 eighths): culled when all three are off one side
	static bool ListOffScreen(const uint8_t *p0, const uint8_t *p1, const uint8_t *p2)
	{
		uint32_t f = 0;
		const uint8_t *p[3] = { p0, p1, p2 };
		for (int k = 0; k < 3; k++)
		{
			int16_t x = *(const int16_t *)p[k], y = *(const int16_t *)(p[k] + 2);
			if (x < 0 || x > 0xA00) f |= 1u << k;
			if (y < 0 || y > 0x6C0) f |= 0x10u << k;
		}
		return (f & 7) == 7 || (f & 0x70) == 0x70;
	}

	// tpage / clut overrides after NCLIP (flags 0x100 / 0x400 tpage = / +=, 0x200 / 0x800 clut)
	static void ListTpageClut(const uint8_t *h, uint8_t *pkt, int tpage_ofs)
	{
		uint32_t fl = *(const uint32_t *)(h + 0x1C);
		if (fl & 0x400) *(int16_t *)(pkt + tpage_ofs) = (int16_t)(*(int16_t *)(pkt + tpage_ofs) + *(const int16_t *)(h + 0x10));
		else if (fl & 0x100) *(int16_t *)(pkt + tpage_ofs) = *(const int16_t *)(h + 0x10);
		if (fl & 0x800) *(int16_t *)(pkt + 0xE) = (int16_t)(*(int16_t *)(pkt + 0xE) + *(const int16_t *)(h + 0x14));
		else if (fl & 0x200) *(int16_t *)(pkt + 0xE) = *(const int16_t *)(h + 0x14);
	}

	// sub_635970: FT3 list (records 0x14, POLY_FT3 0x20)
	static uint32_t __cdecl ListFT3(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pkt = (uint8_t *)cursor;
		uint8_t *lp = *(uint8_t **)(h + 0x20);
		int32_t count = *(int32_t *)lp;
		uint8_t *rec = lp + 4;
		*(uint8_t **)(h + 0x20) = rec;
		for (; count > 0; count--, rec += 0x14)
		{
			ListVertices(h, rec);
			uint32_t fl = *(uint32_t *)(h + 0x1C);
			*(uint32_t *)pkt = 0x7000000;
			*(uint32_t *)(pkt + 4) = *(uint32_t *)rec;
			if (fl & 1) *(uint32_t *)(pkt + 4) |= 0x2000000;
			if (fl & 4) *(uint32_t *)(pkt + 4) &= 0xFDFFFFFF;
			const uint32_t uv = *(uint32_t *)(h + 0x18);
			*(uint32_t *)(pkt + 0xC) = *(uint32_t *)(rec + 0xC) + uv;
			*(uint32_t *)(pkt + 0x14) = *(uint32_t *)(rec + 0x10) + uv;
			*(uint32_t *)(pkt + 0x1C) = (*(uint32_t *)(rec + 8) >> 16) + uv;
			GteReadFLAG(h + 0x30);
			if (*(uint32_t *)(h + 0x30) & 0x60000) continue;
			GteNCLIP();
			ListTpageClut(h, pkt, 0x16);
			GteReadMAC0(h + 0x24);
			const int32_t mac0 = *(int32_t *)(h + 0x24);
			if (mac0 == 0) continue;
			if (mac0 < 0 && !(*(uint8_t *)(h + 0x1C) & 0x10)) continue;
			GteReadSXY012Split(pkt + 8, pkt + 0x10, pkt + 0x18);
			GteAVSZ3();
			if (ListOffScreen(pkt + 8, pkt + 0x10, pkt + 0x18)) continue;
			GteReadOTZ32(h + 0x2C);
			if (*(uint8_t *)(h + 0x1C) & 0x40)
			{
				GteLoadRGBC(pkt + 4);
				GteSetIR0(*(int32_t *)(h + 0xC));
				GteDPCS();
				GteStoreRGB2(pkt + 4);
			}
			InsertPrimAutoDepth(ot + 4 * (uint32_t)(*(int32_t *)(h + 0x2C) >> shift), pkt);
			pkt += 0x20;
		}
		*(uint8_t **)(h + 0x20) = rec;
		return (uint32_t)pkt;
	}

	// sub_6365E0: GT3 list (records 0x1C, POLY_GT3 0x28)
	static uint32_t __cdecl ListGT3(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pkt = (uint8_t *)cursor;
		uint8_t *lp = *(uint8_t **)(h + 0x20);
		int32_t count = *(int32_t *)lp;
		uint8_t *rec = lp + 4;
		*(uint8_t **)(h + 0x20) = rec;
		for (; count > 0; count--, rec += 0x1C)
		{
			ListVertices(h, rec);
			uint32_t fl = *(uint32_t *)(h + 0x1C);
			*(uint32_t *)pkt = 0x9000000;
			*(uint32_t *)(pkt + 4) = *(uint32_t *)rec;
			if (fl & 2) *(uint32_t *)(pkt + 4) |= 0x2000000;
			if (fl & 8) *(uint32_t *)(pkt + 4) &= 0xFDFFFFFF;
			const uint32_t uv = *(uint32_t *)(h + 0x18);
			*(uint32_t *)(pkt + 0xC) = *(uint32_t *)(rec + 0xC) + uv;
			*(uint32_t *)(pkt + 0x18) = *(uint32_t *)(rec + 0x10) + uv;
			*(uint32_t *)(pkt + 0x24) = (*(uint32_t *)(rec + 8) >> 16) + uv;
			GteReadFLAG(h + 0x30);
			if (*(uint32_t *)(h + 0x30) & 0x60000) continue;
			GteNCLIP();
			ListTpageClut(h, pkt, 0x1A);
			GteReadMAC0(h + 0x24);
			if (*(int32_t *)(h + 0x24) < 0 && !(*(uint8_t *)(h + 0x1C) & 0x20)) continue;
			GteReadSXY012Split(pkt + 8, pkt + 0x14, pkt + 0x20);
			GteAVSZ3();
			if (ListOffScreen(pkt + 8, pkt + 0x14, pkt + 0x20)) continue;
			GteReadOTZ32(h + 0x2C);
			if (*(uint8_t *)(h + 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0x14, rec + 0x18, pkt + 4);
				GteSetIR0(*(int32_t *)(h + 0xC));
				GteDPCT();
				GteStoreRGB012(pkt + 0x10, pkt + 0x1C, pkt + 4);
			}
			else
			{
				*(uint32_t *)(pkt + 0x10) = *(uint32_t *)(rec + 0x14);
				*(uint32_t *)(pkt + 0x1C) = *(uint32_t *)(rec + 0x18);
			}
			InsertPrimAutoDepth(ot + 4 * (uint32_t)(*(int32_t *)(h + 0x2C) >> shift), pkt);
			pkt += 0x28;
		}
		*(uint8_t **)(h + 0x20) = rec;
		return (uint32_t)pkt;
	}

	static uint32_t ShardRender(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t fl = *(uint32_t *)(h + 0x1C);
		if (!(fl & 0x2000)) *(uint32_t *)(h + 4) = *(uint32_t *)h + 8;
		const uint32_t model = *(uint32_t *)h;
		*(uint32_t *)(h + 0x20) = model + *(uint32_t *)model;
		if (!(fl & 0x1000)) *(uint32_t *)(h + 0x18) = 0;
		GteSetFarColor(h[8], h[9], h[0xA]);
		const ListFn lists[8] = { (ListFn)LIST_F3, (ListFn)LIST_F4, ListFT3, (ListFn)LIST_FT4, (ListFn)LIST_G3, (ListFn)LIST_G4, ListGT3, (ListFn)LIST_GT4 };
		for (int k = 0; k < 8; k++)
		{
			if (**(uint32_t **)(h + 0x20) != 0) cursor = lists[k](h, ot, shift, cursor);
			else *(uint32_t *)(h + 0x20) += 4;
		}
		return cursor;
	}

	// ------------------------------------------------------------------
	// Shard (0x635110), 46 ticks after its delay +0x16: the shard's own model (0xE25850 + 4 k) at
	// its position (0x24F3378 + 16 k, moving by its velocity, no drag), scale +0x1C, camera-relative,
	// vertex offset 0xE258B0 + 8 k (the shard's place in the grabbed picture)
	// ------------------------------------------------------------------
	static void ShardDraw(const Node24 *n, const Shard *sh)
	{
		int16_t ang[4] = { 0, 0, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = sh->x;
		m.t[1] = sh->y;
		int32_t s[3] = { n->s1C, n->s1C, n->s1C };
		m.t[2] = sh->z;
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x60);
		const int32_t e = n->e;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x3C;
		*(uint32_t *)h = *(const uint32_t *)(0xE25850 + 4 * e);
		*(uint32_t *)(h + 0x58) = *(const uint32_t *)(0xE258B0 + 8 * e);
		*(uint32_t *)(h + 0x5C) = *(const uint32_t *)(0xE258B4 + 8 * e);
		Cursor() = ShardRender(h, OT(), 2, Cursor());
		FieldFree(0x60);
	}

	static uint32_t __cdecl ShardTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->p16 > 0)
		{
			if (!Paused()) n->p16--;
			return 0;
		}
		Shard *sh = SH(n->e);
		ShardDraw(n, sh);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		sh->x = (int16_t)(sh->x + sh->vx);
		sh->y = (int16_t)(sh->y + sh->vy);
		sh->z = (int16_t)(sh->z + sh->vz);
		n->c++;
		return n->c >= 0x2E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Shard dust (0x636CA0, shards 16): pool B mask 1, one sprite per shard per tick while the
	// counter <= 18 (shard position +-500), standing; flipbook 0xE19E68 (frame 2 age + 1, colour
	// 0x404040, mode 12) lit through the camera; ends when none is left
	// ------------------------------------------------------------------
	static void ShardDustSetup(uint8_t *h, uint8_t *s)
	{
		ff8fx::copy<(0x20)>(s + 8, &Camera());
		*(uint32_t *)h = 0xE19E68;
		*(uint32_t *)(h + 0x1C) = 0x404040;
		*(uint16_t *)(h + 0x24) = 0xC;
	}

	static void ShardDustOne(uint8_t *h, const RecB *p)
	{
		GteLoadV0(&p->x);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(p->size);
		*(int16_t *)(h + 4) = (int16_t)(shl32(p->age, 1) + 1);
		GteTransFromMAC();
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static uint32_t __cdecl ShardDustTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 0x12)
		{
			for (int k = 0; k < 24; k++)
			{
				int i = FreeRecord(pool, 0x18, POOLB_N);
				if (i < 0) break;
				RecB *p = (RecB *)(pool + 0x18 * i);
				p->mask = 1;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0xA00 + 0x600);
				*(uint32_t *)&p->x = *(uint32_t *)&SH(k)->x;
				*(uint32_t *)&p->z = *(uint32_t *)&SH(k)->z;
				p->x = (int16_t)(p->x + (CrtRand() % 1000 - 500));
				p->y = (int16_t)(p->y + (CrtRand() % 1000 - 500));
				p->z = (int16_t)(p->z + (CrtRand() % 1000 - 500));
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		ShardDustSetup(h, s);
		int live = 0;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
		for (int i = 0; i < POOLB_N; i++)
		{
			RecB *p = (RecB *)(pool + 0x18 * i);
			if (!(p->mask & 1)) continue;
			ShardDustOne(h, p);
			FX_HELD(PoolMemoB(i, p);)
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				p->mask = 0;
				continue;
			}
			p->age++;
			live++;
		}
		FX_HELD(MemoNode(n);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return live ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Shard chips (0x636EB0, shards 16): pool A mask 1, one prim per shard per tick while the
	// counter <= 6 (shard position +-700, along the shard's direction at 100..1050, tumbling about
	// x / z); prim 0xE1F5F0 (header +0x1C 0) camera-relative, drag 1/128; 45 ticks, then every
	// mask-1 record is freed
	// ------------------------------------------------------------------
	static void ChipSetup(uint8_t *h, uint8_t *s)
	{
		ff8fx::copy<(0x20)>(s + 8, &Camera());
		*(uint32_t *)h = 0xE1F5F0;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		*(int16_t *)(s + 2) = 0;
	}

	static void ChipOne(uint8_t *h, uint8_t *s, const RecA *p)
	{
		Mat4x3 *M = (Mat4x3 *)(s + 0x28);
		*(int16_t *)(s + 0) = p->ax;
		*(int16_t *)(s + 4) = p->az;
		ComposeZYXRotationMatrix((const int16_t *)s, M);
		M->t[1] = p->y;
		M->t[2] = p->z;
		M->t[0] = p->x;
		int32_t *sc = (int32_t *)(s + 0x48);
		sc[2] = p->size;
		sc[1] = p->size;
		sc[0] = p->size;
		Scale3DMatrix(M, sc);
		ComposeAffineTransform((const Mat4x3 *)(s + 8), M, M);
		GteSetRotMatrix(M);
		GteSetTransVector(M);
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
	}

	static uint32_t __cdecl ShardChipTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x68);
		if (!Paused() && n->c <= 6)
		{
			for (int k = 0; k < 24; k++)
			{
				int i = FreeRecord(pool, 0x20, POOLA_N);
				if (i < 0) break;
				RecA *p = (RecA *)(pool + 0x20 * i);
				p->mask = 1;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0x400 + 0x80);
				*(uint32_t *)&p->x = *(uint32_t *)&SH(k)->x;
				*(uint32_t *)&p->z = *(uint32_t *)&SH(k)->z;
				p->x = (int16_t)(p->x + (CrtRand() % 1400 - 700));
				p->y = (int16_t)(p->y + (CrtRand() % 1400 - 700));
				p->z = (int16_t)(p->z + (CrtRand() % 1400 - 700));
				*(uint32_t *)s = *(uint32_t *)&SH(k)->vx;
				*(uint32_t *)(s + 4) = *(uint32_t *)&SH(k)->vz;
				VectorNormalS16((int16_t *)s, (int16_t *)s);
				int32_t m = CrtRand() % 950 + 100;
				p->vx = (int16_t)(mul32(((int16_t *)s)[0], m) >> 12);
				p->vy = (int16_t)(mul32(((int16_t *)s)[1], m) >> 12);
				p->vz = (int16_t)(mul32(((int16_t *)s)[2], m) >> 12);
				p->ax = (int16_t)(CrtRand() % 0x400);
				int32_t sp = CrtRand() % 67 + 0x1E;
				p->sx = (int16_t)((sp & 1) ? -sp : sp);
				p->az = (int16_t)(CrtRand() % 0x400 - 0x200);
				p->sz = (int16_t)(CrtRand() % 40 + 0xF);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		ChipSetup(h, s);
		for (int i = 0; i < POOLA_N; i++)
		{
			RecA *p = (RecA *)(pool + 0x20 * i);
			if (!(p->mask & 1)) continue;
			ChipOne(h, s, p);
			FX_HELD(PoolMemoA(i, p);)
			if (Paused()) continue;
			p->age++;
			p->x = (int16_t)(p->x + p->vx);
			p->y = (int16_t)(p->y + p->vy);
			p->z = (int16_t)(p->z + p->vz);
			p->vx = (int16_t)(p->vx - (int16_t)(p->vx >> 7));
			p->vy = (int16_t)(p->vy - (int16_t)(p->vy >> 7));
			p->vz = (int16_t)(p->vz - (int16_t)(p->vz >> 7));
			p->ax = (int16_t)(p->ax + p->sx);
			p->az = (int16_t)(p->az + p->sz);
		}
		FX_HELD(MemoNode(n);)
		FieldFree(0x58);
		FieldFree(0x68);
		if (Paused()) return 0;
		n->c++;
		if (n->c < 0x2D) return 0;
		for (int i = 0; i < POOLA_N; i++)
			if (*(uint8_t *)(pool + 0x20 * i) & 1) *(uint32_t *)(pool + 0x20 * i) = 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Flash (0x637200, GilTask 357), 10 ticks: screen-space scroll prim 0xE24FC8 (scale 0xE00 at
	// z = H) into OT shift 14, uv scroll -28 (c + 3), fade from 6 (1024 per tick)
	// ------------------------------------------------------------------
	static void FlashDraw(int32_t uv, bool fade, int32_t level)
	{
		int16_t ang[4] = { 0, 0, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		int32_t s[3] = { 0xE00, 0xE00, 0xE00 };
		m.t[0] = 0;
		m.t[1] = 0;
		m.t[2] = var<int16_t>(0x1D8E038);
		Scale3DMatrix(&m, s);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = ScrollHeader(0xE24FC8);
		*(uint16_t *)(h + 0x18) = 0x40;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x10) = 0;
		*(uint16_t *)(h + 0x1A) = 0x80;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint32_t *)(h + 0x20) = 0x33;
		*(uint32_t *)(h + 0x24) = 0x808080;
		*(int32_t *)(h + 0x14) = uv;
		if (fade)
		{
			*(uint32_t *)(h + 0x20) = 0xF3;
			*(int32_t *)(h + 0xC) = level;
		}
		Cursor() = RenderScrollPrimModel(h, OT(), 0xE, Cursor());
		FieldFree(0x90);
	}

	static uint32_t __cdecl FlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		FlashDraw(mul32(-3 - n->c, 28), n->c >= 6, shl32(n->c - 6, 10));
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0xA ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Timeline (0x6328A0): rebuilds the frame (camera o root) first, every tick
	// ------------------------------------------------------------------
	static void Text(int32_t line, int32_t len) { PrintMonsterText(TargetSlot(), line, len); }

	static uint32_t __cdecl TimelineTask(TaskNode *tn)
	{
		ComposeAffineTransform(&Camera(), &RootMatrix(), &Frame());
		const uint32_t f = var<uint32_t>(0x1D96A9C);
		if (f & 0x201)
		{
			if (f & 1) return 0;
			if (LoadState() < 0) return 0;
		}
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			CameraScriptStart(0xE25970, &RootMatrix(), E2, &Q1(), 0);
			BdTransSummonStream(0xF1DD50, STREAM_STATE);
			VoiceSlot() = ClaimVoiceSlot((const void *)0xE25698, 1, 0x80);
		}
		int32_t d = n->c;
		if (d < 0x34)
		{
			if (d == 2)
			{
				BdPlaySE(0xE257D0, 0, 0x80);
				CharacterLoad(0x2C1, Tex() + 0x11000);
			}
			else if (d == 0x10)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(Tex() + 0x11000);
				CharacterLoad(0x2C2, Tex());
			}
			else if (d == 0) LinkQ1(ORIG_FadeTileTask);
			else if (d == 0xB)
			{
				SpawnSpinners();
				ShowStage();
			}
			else if (d == 0x18) SpawnFlare();
			else if (d == 0x20) SpawnRing();
		}
		else if ((d -= 0x34) < 0x14)
		{
			if (d == 0)
			{
				if (LoadState() < 0) return 0;
				LinkQ2(ORIG_ModelDrawTask);
				LinkQ1(ORIG_GilTask);
				LinkQ1(ORIG_SwordTask);
			}
		}
		else if ((d -= 0x14) < 0x14)
		{
			if (d == 0) LinkQ1(ORIG_TileTask);
		}
		else if ((d -= 0x14) < 9)
		{
			if (d == 4) BdPlaySummonStream(0x80, 0, 0x60);
			else if (d == 0) HideStage();
		}
		else if ((d -= 9) < 0x33)
		{
			if (d == 0) CharacterLoad(0x2C3, Tex() + 0x11000);
			else if (d == 0xE)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(Tex() + 0x11000);
			}
			else if (d == 0xF) CharacterLoad(0x2C4, Tex() + 0x11000);
			else if (d == 0x19)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(Tex() + 0x11000);
			}
		}
		else if ((d -= 0x33) < 0x2D)
		{
			if (d == 0)
			{
				CharacterLoad(0x2C5, SummonData());
				Text(0, 0x2D);
			}
			else if (d == 0xA)
			{
				if (LoadState() < 0) return 0;
				BdTransSummonStream(SummonData(), STREAM_STATE);
			}
		}
		else if ((d -= 0x2D) < 0xA5)
		{
			if (d == 0x14)
			{
				Text(1, 0x2D);
				E2[0] |= 8;
			}
			else if (d == 0x41) *(uint16_t *)E2 &= 0xFFF7;
			else if (d == 0x55)
			{
				Text(2, 0x2D);
				E2[0] |= 8;
			}
			else if (d == 0x82) *(uint16_t *)E2 &= 0xFFF7;
			else if (d == 0x96) Text(3, 0xF);
		}
		else if ((d -= 0xA5) < 0x15)
		{
			if (d == 4)
			{
				BdPlaySE(0xE257D4, 0, 0x80);
				Flags() &= ~4u;
			}
			else if (d == 0xE)
			{
				BdPlaySummonStream(0x80, 0, 0x60);
				Flags() &= ~4u;
			}
			else if (d == 0)
			{
				*(uint16_t *)E2 &= 0xFFF7;
				Flags() &= ~4u;
			}
			else if (d >= 8 && d <= 0xB) Flags() |= 4;
			else Flags() &= ~4u;
		}
		else if ((d -= 0x15) < 0x12)
		{
			if (d == 0)
			{
				uint8_t *t = Entity(TargetSlot());
				QueueChainTransformation(t, 0x37);
				t[0] |= 0x20;
				SpawnGroundWave();
			}
		}
		else if ((d -= 0x12) < 0x16)
		{
			if (d == 0)
			{
				LinkQ1(ORIG_SlashTask);
				Text(5, 0x14);
			}
		}
		else if ((d -= 0x16) < 0x9C)
		{
			if (d == 0x88) BdPlaySE(0xE257D8, 0, 0x80);
			else if (d == 0x51) Text(6, 0x2D);
			else if (d == 0x99)
			{
				if (VoiceSlotBusy(VoiceSlot())) ReleaseVoiceSlot(VoiceSlot());
			}
		}
		if (n->c == 0x17F) ApplyActionResultToTarget(*(uint32_t *)(*(uint8_t **)(CastCtx() + 4) + 8));
		n->c++;
		if (n->c <= 0x243) return 0;
		LinkQ1(ORIG_EndTask);
		uint8_t *t = Entity(TargetSlot());
		QueueChainTransformation(t, 1);
		t[0] |= 4;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// registration
	// ------------------------------------------------------------------
	static void RegisterPorts()
	{
		register_port(ORIG_SequenceTick, (void *)SequenceTick, "G218 SequenceTick", 218);
		register_port(ORIG_TimelineTask, (void *)TimelineTask, "G218 TimelineTask", 218);
		register_port(ORIG_FadeTileTask, (void *)FadeTileTask, "G218 FadeTileTask", 218);
		register_port(ORIG_SpinnerTask, (void *)SpinnerTask, "G218 SpinnerTask", 218);
		register_port(ORIG_FlareTask, (void *)FlareTask, "G218 FlareTask", 218);
		register_port(ORIG_RingTask, (void *)RingTask, "G218 RingTask", 218);
		register_port(ORIG_TileTask, (void *)TileTask, "G218 TileTask", 218);
		register_port(ORIG_GroundWaveTask, (void *)GroundWaveTask, "G218 GroundWaveTask", 218);
		register_port(ORIG_SlashTask, (void *)SlashTask, "G218 SlashTask", 218);
		register_port(ORIG_SwordTask, (void *)SwordTask, "G218 SwordTask", 218);
		register_port(ORIG_EndTask, (void *)EndTask, "G218 EndTask", 218);
		register_port(ORIG_GilTask, (void *)GilTask, "G218 GilTask", 218);
		register_port(ORIG_GlowTask, (void *)GlowTask, "G218 GlowTask", 218);
		register_port(ORIG_DustTask, (void *)DustTask, "G218 DustTask", 218);
		register_port(ORIG_ShockRingTask, (void *)ShockRingTask, "G218 ShockRingTask", 218);
		register_port(ORIG_RisingRingTask, (void *)RisingRingTask, "G218 RisingRingTask", 218);
		register_port(ORIG_WideRingTask, (void *)WideRingTask, "G218 WideRingTask", 218);
		register_port(ORIG_ShakeTask, (void *)ShakeTask, "G218 ShakeTask", 218);
		register_port(ORIG_ShardsTask, (void *)ShardsTask, "G218 ShardsTask", 218);
		register_port(ORIG_ShardTask, (void *)ShardTask, "G218 ShardTask", 218);
		register_port(ORIG_ShardDustTask, (void *)ShardDustTask, "G218 ShardDustTask", 218);
		register_port(ORIG_ShardChipTask, (void *)ShardChipTask, "G218 ShardChipTask", 218);
		register_port(ORIG_FlashTask, (void *)FlashTask, "G218 FlashTask", 218);
		register_port(ORIG_ModelDrawTask, (void *)ModelDrawTask, "G218 ModelDrawTask", 218);
		// 30 fps layer: see mag218_gilgamesh_kills_seifer_held.inc
		FX_HELD(register_mag218_held();)
	}
}

	void register_mag218_gilgamesh_kills_seifer() { g218::RegisterPorts(); }
}

#ifdef FF8_FX_HELD
#include "mag218_gilgamesh_kills_seifer_held.inc"
#endif
