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

// Effect 70: Shockwave Pulsar (enemy attack 362 of kernel.bin; Ultimecia's final form c0m122; no
// other kernel.bin entry uses effect 70; MAG_070_SHOCKWAVE_PULSAR).
//
// Structure (setup 0x6DD8D0 -> 0x6DD900, file loader 0x6DD8E0 = texture 0x112D1A4; the setup queues
// the TIM, keeps the cast context 0x2544E8C and the caster's entity 0x2544E90 and starts the root task
// in the root queue 0x2544EB8, one 0x10-byte node at 0x2544EC8):
//   RootTask (0x6DD970) - alternates the module packet arena (magic buffer + 0xAE9C / + 0x1AE9C,
//     cursor 0x2544E84); the screen flash level 0x2544F10 is reset every tick; on counter 1 saves the
//     seven entities' flag words (0x6DDCC0), clears the two particle pools (0x6DDCF0 / 0x6DDD20),
//     resets the stream loader state (magic buffer + 0x678), sets up the master pool (1 x 0x115C at
//     magic buffer + 0x6C0) and the sub-task pool (6 x 0x114 at the magic buffer), starts the master,
//     claims a voice slot (0x1324DAC), keeps every target with its position, the targets' centre
//     (0x2544F08, effect anchors) and the side (0x2544F18: +1 = the targets are the party); then each
//     tick: the effect camera (0x67AAE0 -> 0x2544F20), the master queue, the sub-task queue, the
//     screen flash (0x5713E0) and entity-array bit 1 while the flash is full; ends with the master.
//   Master (0x6DDD50) - node 0x115C: counter, target count, voice slot, the targets (entity, saved
//     position, rotation angles, model top / bottom), a prim-model layout (+0x5C).  A 0x106-tick
//     timeline (then waits for the stream loader):
//       0..0xB, 0xFA..0x105   screen flash ramps (0x8FC in between)
//       1, 9, 0x11            one RiseTask per target (0x6E0100), model height measured (0x6DFD90)
//       0x32..0x4F            a layout (0x1332540) at the targets' centre, 0xC000 high
//       0x50                  sub-task pool reset, flag 4 on the shown entities (0x6DF940), the
//                             shockwave layout (0x1327D28) and one LiftTask (0x6DFF00) per target
//       0x50..0xD5            the targets' models redrawn rotated and scaled (0x693170), the shockwave
//                             layout behind the targets; sparks rise (pool A) on 0x5D..0x78 and
//                             model shards (pool B) fly off the targets on 0xC3..0xD2; a white tile
//                             (0x7040B0) on 0xCE..0xD5
//       0xD6                  positions and flags restored; then the flash fades out over 0x20 ticks,
//                             the shockwave layout drawn deeper
//       0, 0x14..0x3A, 0x3B, 0x4F..0x76, 0x77..0xB2, 0xD5   battle camera (direct sets and 0x657DF0)
//       0xD5                  stream loads 0x168 / 0x169 into magic buffer + 0x269C
//       0xFC                  damage (0x506BA0); 0x102 voice slot released; sounds at 0, 0x50, 0xAA
//     Every tick it draws the sparks (0x6DF9A0, flipbook 0x6FC9E0) and the shards (0x6DFB80: model
//     0x1324ED8 through the module renderer 0x6DECA0), then moves them (0x6DFB30 / 0x6DFD60).
//   RiseTask (0x6E0100) - node 0x114: a layout (0x1325A88) on the target; from tick 0xD it lifts the
//     target (entity +0x1E) and spins its redrawn model (0x693170), starting a RingTask (0x6E02E0,
//     layout 0x1324FF0 at the target's feet); after the lift the target drops back.
//   LiftTask (0x6DFF00) - node 0x114: a layout (0x1326C68) on the target for 0x1E ticks, sets the
//     target's height (entity +0x1E) and spins its angles; a burst of 16 sparks on tick 5.
// Prim-player callback 0x6DE860: one object (flags 0x40000 / 0x80000: other rotation orders, 0x1000:
// offset through the effect camera, 0x200: screen-axes offset, 0x100: 3x3 scale, 0x4000: blend mode);
// objects whose type byte is '1' go through the module renderer 0x6DECA0 (the engine's 0x7043B0 with
// three primitive lists of its own: 0x6DEDD0 flat quads, 0x6DF140 gouraud quads, 0x6DF560 gouraud
// textured quads, each between two draw-mode primitives; the other five lists are effect 260's
// 0x691C70 / 0x691F30 / 0x6921C0 / 0x6924F0 / 0x692830), the others through 0x7043B0.
// No pause flag is tested (the debug pause / draw-only flags are ignored by the original).
// The effect WRITES ENTITY POSITIONS: the targets' +0x1E (height) every tick of the Lift / Rise tasks,
// +0x1C..+0x20 restored on 0xD6; entity flag words (bits 2, 3) and entity-array bit 1. It WRITES THE
// BATTLE CAMERA: eye 0xB8B7F0 / look-at 0xB8B7F8 on 0, 0x14..0x3A, 0x3B, 0x4F..0xB2, 0xD5; 0x1D8E038 on
// 0, 0x4F..0x76, 0xD5 and at the end; 0x1D977A2 on 0; camera return armed at 0 (0x50A730).
// Module globals: 0x2544E78..0x2544F5C. Pools, arenas, particle tables in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace sp070
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&SparkCursor() { return var<uint8_t *>(0x2544E7C); }     // pool A alloc cursor
	inline uint8_t *&ShardCursor() { return var<uint8_t *>(0x2544E80); }     // pool B alloc cursor
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2544E84); }
	inline uint32_t &ArenaEnd() { return var<uint32_t>(0x2544E88); }         // (written, never read)
	inline uint8_t *&Ctx() { return var<uint8_t *>(0x2544E8C); }             // cast context
	inline TaskQueue *QSub() { return (TaskQueue *)0x2544E98; }              // pool: magic buffer, 6 x 0x114
	inline TaskQueue *QMaster() { return (TaskQueue *)0x2544EA8; }           // pool: magic buffer + 0x6C0, 1 x 0x115C
	inline uint32_t &InitAngles0() { return var<uint32_t>(0x2544EF8); }      // copied into every target's angles
	inline uint32_t &InitAngles1() { return var<uint32_t>(0x2544EFC); }
	inline uint32_t &CentreX() { return var<uint32_t>(0x2544F08); }          // targets' centre (x, y words; z)
	inline int16_t &CentreY() { return var<int16_t>(0x2544F0A); }
	inline uint32_t &CentreZ() { return var<uint32_t>(0x2544F0C); }
	inline int32_t &Flash() { return var<int32_t>(0x2544F10); }              // screen flash level this tick
	inline int32_t &FlashPrev() { return var<int32_t>(0x2544F14); }
	inline int32_t &Side() { return var<int32_t>(0x2544F18); }               // +1: targets are the party, -1: enemies
	inline uint32_t &TexBase() { return var<uint32_t>(0x2544F1C); }          // Magic_TextureOFF (magic buffer)
	inline Mat4x3 &EffCam() { return var<Mat4x3>(0x2544F20); }               // effect camera matrix (0x67AAE0)
	inline uint32_t *SavedFlags() { return (uint32_t *)0x2544F40; }          // 7 entities' flag words
	inline uint32_t OT() { return var<uint32_t>(0x1D8E04C); }                // g_Battle_FrameRenderListBase
	inline int16_t &ViewWord() { return var<int16_t>(0x1D8E038); }
	inline int16_t &CameraZoom() { return var<int16_t>(0x1D977A2); }
	inline int16_t *CamEye() { return (int16_t *)0xB8B7F0; }
	inline int16_t *CamLookAt() { return (int16_t *)0xB8B7F8; }

	static const uint32_t ORIG_RootTask = 0x6DD970;
	static const uint32_t ORIG_MasterTask = 0x6DDD50;
	static const uint32_t ORIG_LiftTask = 0x6DFF00;
	static const uint32_t ORIG_RiseTask = 0x6E0100;
	static const uint32_t ORIG_RingTask = 0x6E02E0;
	static const uint32_t ORIG_Callback = 0x6DE860;
	static const uint32_t LAYOUT_CENTRE = 0x1332540;   // 0x218
	static const uint32_t LAYOUT_WAVE = 0x1327D28;     // 0x1100
	static const uint32_t LAYOUT_LIFT = 0x1326C68;     // 0xBC
	static const uint32_t LAYOUT_RISE = 0x1325A88;     // 0x100
	static const uint32_t LAYOUT_RING = 0x1324FF0;     // 0xD8
	static const uint32_t OBJS_A = 0x1334D5C;          // object type strings ('1' = module renderer)
	static const uint32_t OBJS_WAVE = 0x112D1B4;
	static const uint32_t SPARK_SEQ = 0x1324B98;       // pool A flipbooks
	static const uint32_t BURST_SEQ = 0x13249EC;
	static const uint32_t SHARD_MODEL = 0x1324ED8;
	static const void *const SOUND_Voice = (const void *)0x1324DAC;
	static const void *const SOUND_0 = (const void *)0x1324ECC;
	static const void *const SOUND_50 = (const void *)0x1324ED0;
	static const void *const SOUND_AA = (const void *)0x1324ED4;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	inline void RotationFromAnglesC(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701630)(angles, out); }  // MAG_070_sub_701630 (X, Y, Z)
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // sub_56C220: a = a * b (3x3)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline uint32_t Tile(int32_t r, int32_t g, int32_t b, int32_t level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x7040B0)(r, g, b, level, cursor); } // MAG_063_sub_7040B0
	inline uint32_t Flipbook(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x6FC9E0)(h, ot, mode, cursor); } // MAG_065_sub_6FC9E0
	// MAG_070_sub_657DF0 (Diablos' module): look-at = a[0..2] -> b[0..2], eye = a[3..5] -> b[3..5] at t
	inline void CamLerp(const int16_t *a, const int16_t *b, int32_t t) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t)>(0x657DF0)(a, b, t); }
	// effect 260's helpers the module calls directly
	inline void DrawTargetModel(uint8_t *e, const int16_t *angles, const int16_t *offset, int32_t scale) { fn<void (__cdecl *)(uint8_t *, const int16_t *, const int16_t *, int32_t)>(0x693170)(e, angles, offset, scale); } // MAG_070_sub_693170
	inline void ShardMatrix(int32_t angle, int32_t sy, int32_t sxz, void *out) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, void *)>(0x692F00)(angle, sy, sxz, out); } // MAG_070_sub_692F00
	typedef uint32_t (__cdecl *PrimList)(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor);
	static const uint32_t LIST_FLAT_TRI = 0x691C70, LIST_1 = 0x691F30, LIST_2 = 0x6921C0, LIST_3 = 0x6924F0, LIST_4 = 0x692830;
	inline uint32_t CallList(uint32_t addr, uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<PrimList>(addr)(h, ot, shift, cursor); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }    // BS_Camera_ArmReturnAfterEffect
	inline void ReleaseVoice(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }          // au_re_BattleFile_preLoad
	inline int32_t StreamPending(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); } // au_re_BattleFile_preLoad_0
	inline void TransformVec3(const void *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const void *, const int32_t *, int32_t *)>(0x56C600)(m, in, out); } // TransformVectorBy3x3Matrix
	// software GTE
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // MAG_069_sub_703FB0: V0 = 3 x u16
	inline void GteReadIR2(void *dst) { fn<void (__cdecl *)(void *)>(0x703F90)(dst); }        // MAG_070_sub_703F90: IR2 (word)
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); } // GTE_ReadSXY012_Split
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }    // GTE_ReadOTZ (dword)
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }        // set_unk_1CA8A28
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }                   // set_param_with_dword_1CA8A68
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }

	inline uint32_t mulhi_u(uint32_t a, uint32_t b) { return (uint32_t)(((uint64_t)a * b) >> 32); }
	inline int32_t mulhi_s(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 32); }
	// cdq; and edx, m; add; sar n (C signed division by 2^n)
	inline int32_t sdiv_pow2(int32_t v, int n) { return (int32_t)((uint32_t)v + ((uint32_t)(v >> 31) & ((1u << n) - 1))) >> n; }
	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }
	inline int16_t &E16(uint8_t *e, uint32_t off) { return *(int16_t *)(e + off); }
	// battle entity slot of an entity pointer (the compiler's division by 0x9C)
	inline int32_t SlotOf(const uint8_t *e)
	{
		const int32_t d = (int32_t)((uint32_t)e - 0x1D972C0);
		int32_t q = (mulhi_s(d, (int32_t)0xD20D20D3) + d) >> 7;
		return q + (int32_t)((uint32_t)q >> 31);
	}

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes (0x2544EC8)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t parity;    // +0x0E packet arena parity
		uint8_t started;   // +0x0F master started
	};
	struct Target // 0x18 bytes
	{
		uint8_t *entity;
		uint32_t pos_xy;   // entity +0x1C at the start (+6 = the height word)
		uint32_t pos_z;    // entity +0x20 at the start
		int16_t ang[3];    // +0x0C rotation of the redrawn model (from 0x2544EF8)
		int16_t pad12;
		int16_t y_max;     // +0x14 model bottom (largest bone-space y, 0x6DFD90)
		int16_t y_min;     // +0x16 model top
	};
	struct MasterNode // pool of 1 node of 0x115C bytes (magic buffer + 0x6C0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t ntargets;  // +0x0E
		uint32_t voice;    // +0x10
		Target targets[3]; // +0x14
		uint8_t layout[0x1100]; // +0x5C
	};
	struct SubNode // pool of 6 nodes of 0x114 bytes (magic buffer): Lift / Rise / Ring tasks
	{
		TaskNode hdr;
		union
		{
			struct { Target *rec; int16_t counter; int16_t delay; } t; // Lift (delay unused), Rise
			int16_t pos[4];                                              // Ring: x, y, z
		};
		uint8_t layout[0x100]; // +0x14
	};
	struct Spark // pool A: magic buffer + 0x181C, 0x80 x 0x10
	{
		const uint8_t *seq;
		int16_t pos[3];    // +4
		int16_t scale;     // +0xA
		int8_t vel[3];     // +0xC (x 4 per tick)
		uint8_t life;      // +0xF
	};
	struct Shard // pool B: magic buffer + 0x201C, 0x40 x 0x10
	{
		int16_t life;
		int16_t bone;      // +2
		int16_t sy, sxz;   // +4 / +6 scale of the shard model
		int16_t dsy, dsxz; // +8 / +0xA
		Target *rec;       // +0xC
	};
	// the prim-player callback's parameter block (a stack block of the caller)
	struct PrimArg
	{
		int16_t pos[3];
		int16_t pad06;     // never written (VZ0 high half)
		int32_t scale[3];  // +0x08
		int32_t scaled;    // +0x14
		uint32_t objs;     // +0x18 object type string
		int16_t depth;     // +0x1C prim header +0x18
		int16_t pad1E;
		uint32_t morph;    // +0x20 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(MasterNode) == 0x115C && sizeof(SubNode) == 0x114, "Pulsar 070 nodes");
	static_assert(sizeof(Target) == 0x18 && sizeof(Spark) == 0x10 && sizeof(Shard) == 0x10 && sizeof(PrimArg) == 0x24, "Pulsar 070 blocks");
}
}

#ifdef FF8_FX_HELD
#include "mag070_shockwave_pulsar_held.h"
#endif

namespace ff8fx
{
namespace sp070
{
	// screen clip of a projected vertex (x 0..0xA00, y 0..0x6C0: the PC's 4x subpixel screen)
	static inline bool OutX(const uint8_t *xy) { const int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(const uint8_t *xy) { const int16_t v = *(const int16_t *)(xy + 2); return v < 0 || v > 0x6C0; }

	// ------------------------------------------------------------------
	// sub_6DEDD0: flat quads (0xC-byte records: code + rgb, 4 vertex indices) -> POLY_F4 packets,
	// each between two draw-mode primitives (same bucket); header flags 1 / 4 semi-transparency on /
	// off, 0x10 double-sided, 0x40 depth-cued colour
	// ------------------------------------------------------------------
	static uint32_t __cdecl FlatQuads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *list = F<uint8_t *>(h, 0x20);
		const int32_t count = *(const int32_t *)list;
		const uint8_t *rec = list + 4;
		F<const uint8_t *>(h, 0x20) = rec;
		const uint8_t *verts = F<uint8_t *>(h, 4);
		if (count <= 0)
		{
			F<const uint8_t *>(h, 0x20) = rec;
			return (uint32_t)pk;
		}
		for (int32_t k = count; k != 0; k--, rec += 0xC)
		{
			GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			GteRTPT();
			F<uint32_t>(pk, 0) = 0x5000000;
			const uint32_t code = *(const uint32_t *)rec;
			F<uint32_t>(pk, 4) = code;
			if (F<uint8_t>(h, 0x1C) & 1) F<uint32_t>(pk, 4) = code | 0x2000000;
			if (F<uint8_t>(h, 0x1C) & 4) F<uint32_t>(pk, 4) &= 0xFDFFFFFF;
			GteReadFLAG(h + 0x30);
			if (F<uint32_t>(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0(h + 0x24);
			if (F<int32_t>(h, 0x24) < 0 && !(F<uint8_t>(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0xC, pk + 0x10);
			GteLoadV0(verts + (uint32_t)*(const uint16_t *)(rec + 0xA) * 4);
			GteRTPS();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0xC)) clip |= 2;
			if (OutX(pk + 0x10)) clip |= 4;
			if (OutY(pk + 8)) clip |= 0x10;
			if (OutY(pk + 0xC)) clip |= 0x20;
			if (OutY(pk + 0x10)) clip |= 0x40;
			GteReadSXY2(pk + 0x14);
			GteAVSZ4();
			if (OutX(pk + 0x14)) clip |= 8;
			if (OutY(pk + 0x14)) clip |= 0x80;
			if ((clip & 0xF) == 0xF) continue;
			if ((clip & 0xF0) == 0xF0) continue;
			GteReadOTZWord(h + 0x2C);
			if (F<uint8_t>(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(F<int32_t>(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			int32_t z = (int32_t)((uint32_t)F<int32_t>(h, 0x2C) + (uint32_t)F<int32_t>(h, 0x18));
			F<int32_t>(h, 0x2C) = z;
			if (z < 0) F<int32_t>(h, 0x2C) = 0;
			const uint32_t bucket = ot + (uint32_t)(F<int32_t>(h, 0x2C) >> shift) * 4;
			uint8_t *poly = pk;
			uint8_t *mode1 = pk + 0x18;
			uint8_t *mode2 = pk + 0x20;
			pk += 0x28;
			F<uint32_t>(mode1, 0) = 0x1000000;
			F<uint32_t>(mode1, 4) = 0xE1000220;
			InsertPrimAutoDepth(bucket, mode1);
			InsertPrimAutoDepth(bucket, poly);
			F<uint32_t>(mode2, 0) = 0x1000000;
			F<uint32_t>(mode2, 4) = 0xE1000240;
			InsertPrimAutoDepth(bucket, mode2);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// sub_6DF140: gouraud quads (0x18-byte records: code + rgb0, 4 vertex indices, rgb1..3) ->
	// POLY_G4 packets between two draw-mode primitives; flags 2 / 8 semi-transparency, 0x20
	// double-sided, 0x80 depth cue. The cursor advances 0x58 bytes per quad (0x24 unused).
	// ------------------------------------------------------------------
	static uint32_t __cdecl GouraudQuads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *list = F<uint8_t *>(h, 0x20);
		const int32_t count = *(const int32_t *)list;
		const uint8_t *rec = list + 4;
		const uint8_t *verts = F<uint8_t *>(h, 4);
		F<const uint8_t *>(h, 0x20) = rec;
		if (count <= 0)
		{
			F<const uint8_t *>(h, 0x20) = rec;
			return (uint32_t)pk;
		}
		for (int32_t k = count; k != 0; k--, rec += 0x18)
		{
			GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			GteRTPT();
			F<uint32_t>(pk, 0) = 0x8000000;
			const uint32_t code = *(const uint32_t *)rec;
			F<uint32_t>(pk, 4) = code;
			if (F<uint8_t>(h, 0x1C) & 2) F<uint32_t>(pk, 4) = code | 0x2000000;
			if (F<uint8_t>(h, 0x1C) & 8) F<uint32_t>(pk, 4) &= 0xFDFFFFFF;
			GteReadFLAG(h + 0x30);
			if (F<uint32_t>(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0(h + 0x24);
			if (F<int32_t>(h, 0x24) < 0 && !(F<uint8_t>(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0(verts + (uint32_t)*(const uint16_t *)(rec + 0xA) * 4);
			GteRTPS();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x10)) clip |= 2;
			if (OutX(pk + 0x18)) clip |= 4;
			if (OutY(pk + 8)) clip |= 0x10;
			if (OutY(pk + 0x10)) clip |= 0x20;
			if (OutY(pk + 0x18)) clip |= 0x40;
			GteReadSXY2(pk + 0x20);
			GteAVSZ4();
			if (OutX(pk + 0x20)) clip |= 8;
			if (OutY(pk + 0x20)) clip |= 0x80;
			if ((clip & 0xF) == 0xF) continue;
			if ((clip & 0xF0) == 0xF0) continue;
			GteReadOTZWord(h + 0x2C);
			if (F<uint8_t>(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0xC, rec + 0x10, rec + 0x14);
				GteSetIR0(F<int32_t>(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0xC, pk + 0x14, pk + 0x1C);
				GteLoadRGBC(pk + 4);
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			else
			{
				F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
				F<uint32_t>(pk, 0x14) = *(const uint32_t *)(rec + 0x10);
				F<uint32_t>(pk, 0x1C) = *(const uint32_t *)(rec + 0x14);
			}
			int32_t z = (int32_t)((uint32_t)F<int32_t>(h, 0x2C) + (uint32_t)F<int32_t>(h, 0x18));
			F<int32_t>(h, 0x2C) = z;
			if (z < 0) F<int32_t>(h, 0x2C) = 0;
			const uint32_t bucket = ot + (uint32_t)(F<int32_t>(h, 0x2C) >> shift) * 4;
			uint8_t *poly = pk;
			uint8_t *mode1 = pk + 0x24;
			uint8_t *mode2 = pk + 0x2C;
			pk += 0x58;
			F<uint32_t>(mode1, 0) = 0x1000000;
			F<uint32_t>(mode1, 4) = 0xE1000220;
			InsertPrimAutoDepth(bucket, mode1);
			InsertPrimAutoDepth(bucket, poly);
			F<uint32_t>(mode2, 0) = 0x1000000;
			F<uint32_t>(mode2, 4) = 0xE1000240;
			InsertPrimAutoDepth(bucket, mode2);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// sub_6DF560: gouraud textured quads (0x24-byte records: code + rgb0, 4 vertex indices, uv0 +
	// clut, uv1 + tpage, uv2 | uv3 << 16, rgb1..3) -> POLY_GT4 packets after a draw-mode primitive
	// ------------------------------------------------------------------
	static uint32_t __cdecl GouraudTexQuads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *list = F<uint8_t *>(h, 0x20);
		const int32_t count = *(const int32_t *)list;
		const uint8_t *rec = list + 4;
		const uint8_t *verts = F<uint8_t *>(h, 4);
		F<const uint8_t *>(h, 0x20) = rec;
		if (count <= 0)
		{
			F<const uint8_t *>(h, 0x20) = rec;
			return (uint32_t)pk;
		}
		for (int32_t k = count; k != 0; k--, rec += 0x24)
		{
			GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			GteRTPT();
			F<uint32_t>(pk, 0) = 0xC000000;
			const uint32_t code = *(const uint32_t *)rec;
			F<uint32_t>(pk, 4) = code;
			if (F<uint8_t>(h, 0x1C) & 2) F<uint32_t>(pk, 4) = code | 0x2000000;
			if (F<uint8_t>(h, 0x1C) & 8) F<uint32_t>(pk, 4) &= 0xFDFFFFFF;
			F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
			F<uint32_t>(pk, 0x18) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			F<uint32_t>(pk, 0x24) = uv;
			F<uint32_t>(pk, 0x30) = uv >> 16;
			GteReadFLAG(h + 0x30);
			if (F<uint32_t>(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0(h + 0x24);
			if (F<int32_t>(h, 0x24) < 0 && !(F<uint8_t>(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			GteLoadV0(verts + (uint32_t)*(const uint16_t *)(rec + 0xA) * 4);
			GteRTPS();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x14)) clip |= 2;
			if (OutX(pk + 0x20)) clip |= 4;
			if (OutY(pk + 8)) clip |= 0x10;
			if (OutY(pk + 0x14)) clip |= 0x20;
			if (OutY(pk + 0x20)) clip |= 0x40;
			GteReadSXY2(pk + 0x2C);
			GteAVSZ4();
			if (OutX(pk + 0x2C)) clip |= 8;
			if (OutY(pk + 0x2C)) clip |= 0x80;
			if ((clip & 0xF) == 0xF) continue;
			if ((clip & 0xF0) == 0xF0) continue;
			GteReadOTZWord(h + 0x2C);
			if (F<uint8_t>(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0x18, rec + 0x1C, rec + 0x20);
				GteSetIR0(F<int32_t>(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 0x28);
				GteLoadRGBC(pk + 4);
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			else
			{
				F<uint32_t>(pk, 0x10) = *(const uint32_t *)(rec + 0x18);
				F<uint32_t>(pk, 0x1C) = *(const uint32_t *)(rec + 0x1C);
				F<uint32_t>(pk, 0x28) = *(const uint32_t *)(rec + 0x20);
			}
			int32_t z = (int32_t)((uint32_t)F<int32_t>(h, 0x2C) + (uint32_t)F<int32_t>(h, 0x18));
			F<int32_t>(h, 0x2C) = z;
			if (z < 0) F<int32_t>(h, 0x2C) = 0;
			const uint32_t bucket = ot + (uint32_t)(F<int32_t>(h, 0x2C) >> shift) * 4;
			uint8_t *poly = pk;
			uint8_t *mode = pk + 0x34;
			pk = mode + 8;
			F<uint32_t>(mode, 0) = 0x1000000;
			F<uint32_t>(mode, 4) = 0xE1000220;
			InsertPrimAutoDepth(bucket, mode);
			InsertPrimAutoDepth(bucket, poly);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// MAG_070_sub_6DECA0: the module's prim-model renderer (the engine's 0x7043B0 with the
	// primitive lists 2, 6 and 8 of its own): vertex base = model + 8 unless header flag 0x2000,
	// far colour from the header colour bytes, then the 8 lists (an empty list is skipped)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RenderModel(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		if (!(F<uint32_t>(h, 0x1C) & 0x2000))
			F<uint32_t>(h, 4) = F<uint32_t>(h, 0) + 8;
		{
			const uint8_t *model = F<uint8_t *>(h, 0);
			const uint32_t b = F<uint8_t>(h, 0xA);
			F<const uint8_t *>(h, 0x20) = model + *(const uint32_t *)model;
			const uint32_t g = F<uint8_t>(h, 9);
			const uint32_t r = F<uint8_t>(h, 8);
			GteSetFarColor(r, g, b);
		}
		uint32_t c = cursor;
		for (int i = 0; i < 8; i++)
		{
			const uint8_t *list = F<uint8_t *>(h, 0x20);
			if (*(const uint32_t *)list == 0)
			{
				F<const uint8_t *>(h, 0x20) = list + 4;
				continue;
			}
			switch (i)
			{
				case 0: c = CallList(LIST_FLAT_TRI, h, ot, shift, c); break;
				case 1: c = FlatQuads(h, ot, shift, c); break;
				case 2: c = CallList(LIST_1, h, ot, shift, c); break;
				case 3: c = CallList(LIST_2, h, ot, shift, c); break;
				case 4: c = CallList(LIST_3, h, ot, shift, c); break;
				case 5: c = GouraudQuads(h, ot, shift, c); break;
				case 6: c = CallList(LIST_4, h, ot, shift, c); break;
				default: c = GouraudTexQuads(h, ot, shift, c); break;
			}
		}
		return c;
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6DE860): one object of a layout
	// ------------------------------------------------------------------
	static void __cdecl PrimPart(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
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
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else if (r->flags & 0x80000) RotationFromAnglesC(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset, scaled by the block's scale
		int16_t off[4] = {};
		const int32_t scaled = arg->scaled;
		if (scaled)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
		}
		else
		{
			off[0] = r->pos[0];
			off[1] = r->pos[1];
			off[2] = r->pos[2];
		}
		if (scaled) Scale3DMatrix(&m, arg->scale);
		const uint32_t flags = r->flags;
		if (flags & 0x1000)
		{
			// the block position through the camera, the offset through the effect camera
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			GteSetRotMatrixCtrl(&EffCam());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t mac[3];
			GteReadMAC123(mac);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)mac[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)mac[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)mac[2]);
			MatrixMultiply(&EffCam(), &m);
		}
		else if (flags & 0x200)
		{
			// the block position through the camera, the offset added unrotated, rotation kept
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			// block position + offset through the camera, rotation composed with the camera
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (flags & 0x100)
			{
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MatrixMultiply3(&m, &d);
			}
			else
			{
				int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = (r->flags & 0x4000) ? 0x2000 : 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) |= 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		if (((const char *)arg->objs)[(int16_t)r->flags_lo] == '1')
			PacketCursor() = RenderModel(h, OT() + 0x44, 2, PacketCursor());
		else
			PacketCursor() = RenderPrimModel2(h, OT() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// small helpers
	// ------------------------------------------------------------------
	// MAG_070_sub_6DDCC0: the seven entities' flag words
	static void SaveEntityFlags()
	{
		for (int k = 0; k < 7; k++) SavedFlags()[k] = *(const uint16_t *)Entity(k);
	}
	// MAG_070_sub_6DF960: bit 2 of the shown entities' flag words back to the saved value
	static void RestoreEntityFlags()
	{
		for (int k = 0; k < 7; k++)
		{
			uint8_t *e = Entity(k);
			const uint16_t w = *(const uint16_t *)e;
			if (w & 2) *(uint16_t *)e = (uint16_t)((((uint32_t)(uint8_t)SavedFlags()[k] ^ w) & 4) ^ w);
		}
	}
	// MAG_065_sub_6DF940: flag 4 on the shown entities (flag 2)
	static void EntitiesFlag4On()
	{
		for (int k = 0; k < 7; k++)
		{
			uint8_t *e = Entity(k);
			const uint16_t w = *(const uint16_t *)e;
			if (w & 2) *(uint16_t *)e = (uint16_t)(w | 4);
		}
	}
	// MAG_070_sub_6DDCF0 / sub_6DDD20: empty pools
	static void ClearSparks()
	{
		uint8_t *p = (uint8_t *)TexBase() + 0x182B;
		for (int k = 0x80; k != 0; k--, p += 0x10) *p = 0;
		SparkCursor() = (uint8_t *)TexBase() + 0x181C;
	}
	static void ClearShards()
	{
		uint8_t *p = (uint8_t *)TexBase() + 0x201C;
		for (int k = 0x40; k != 0; k--, p += 0x10) *(int16_t *)p = 0;
		ShardCursor() = (uint8_t *)TexBase() + 0x201C;
	}
	// MAG_070_sub_6DFAD0: a free spark (life 0) from the cursor on, else from the start
	static Spark *SparkAlloc()
	{
		uint8_t *p = SparkCursor();
		const uint32_t tex = TexBase();
		if (p[0xF] != 0)
		{
			p = (uint8_t *)tex + 0x181C;
			if (p[0xF] != 0)
			{
				int32_t n = 0x80;
				do
				{
					p += 0x10;
					if (--n == 0) return nullptr;
				} while (p[0xF] != 0);
			}
		}
		if ((uint32_t)p >= tex + 0x200C) SparkCursor() = (uint8_t *)tex + 0x181C;
		else SparkCursor() = p + 0x10;
		return (Spark *)p;
	}
	// MAG_070_sub_6DFD00: a free shard (life 0)
	static Shard *ShardAlloc()
	{
		uint8_t *p = ShardCursor();
		const uint32_t tex = TexBase();
		if (*(const int16_t *)p != 0)
		{
			p = (uint8_t *)tex + 0x201C;
			if (*(const int16_t *)p != 0)
			{
				int32_t n = 0x40;
				do
				{
					p += 0x10;
					if (--n == 0) return nullptr;
				} while (*(const int16_t *)p != 0);
			}
		}
		if ((uint32_t)p >= tex + 0x240C) ShardCursor() = (uint8_t *)tex + 0x201C;
		else ShardCursor() = p + 0x10;
		return (Shard *)p;
	}
	// MAG_070_sub_6DFB30: sparks: life--, position += 4 x velocity
	static void SparksUpdate()
	{
		Spark *s = (Spark *)(TexBase() + 0x181C);
		for (int k = 0x80; k != 0; k--, s++)
		{
			if (s->life == 0) continue;
			s->life--;
			s->pos[0] = (int16_t)(s->pos[0] + (int16_t)((int16_t)s->vel[0] << 2));
			s->pos[1] = (int16_t)(s->pos[1] + (int16_t)((int16_t)s->vel[1] << 2));
			s->pos[2] = (int16_t)(s->pos[2] + (int16_t)((int16_t)s->vel[2] << 2));
		}
	}
	// MAG_070_sub_6DFD60: shards: life--, scales += steps
	static void ShardsUpdate()
	{
		Shard *s = (Shard *)(TexBase() + 0x201C);
		for (int k = 0x40; k != 0; k--, s++)
		{
			if (s->life == 0) continue;
			const int16_t d = s->dsy;
			s->life--;
			s->sy = (int16_t)(s->sy + d);
			s->sxz = (int16_t)(s->sxz + s->dsxz);
		}
	}

	// ------------------------------------------------------------------
	// MAG_070_sub_6DF9A0: sparks, one flipbook frame each (frame = sequence counted down by life)
	// ------------------------------------------------------------------
	static void SparksDraw(const Spark *s)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint16_t>(h, 0x60) = 0;
		F<uint16_t>(h, 0x58) = 0;
		F<uint16_t>(h, 0x50) = 0;
		F<uint16_t>(h, 0x48) = 0;
		for (int n = 0x80; n != 0; n--, s++)
		{
			if (s->life == 0) continue;
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			GteLoadV0(s->pos);
			GteMVMVA_RotV0Tr();
			GteReadMAC123((int32_t *)(h + 0x98));
			memset(h + 0x84, 0, 0x14);
			F<uint16_t>(h, 0x94) = (uint16_t)s->scale;
			F<uint16_t>(h, 0x8C) = (uint16_t)s->scale;
			F<uint16_t>(h, 0x84) = (uint16_t)s->scale;
			const uint8_t *seq = s->seq;
			*(const uint8_t **)h = seq;
			const int32_t frames = *(const int16_t *)(seq + 8);
			const int32_t k = frames - ((int32_t)(int8_t)s->life - 1) % frames;
			const uint8_t *frame = seq + *(const int16_t *)(seq + 8 + 2 * k);
			*(const uint8_t **)(h + 0x2C) = frame;
			const int32_t word = *(const int32_t *)frame;
			F<int32_t>(h, 0x30) = word;
			if (word < 0)
			{
				h[0x25] |= 1; // sticky in the scratch block, as the original
				F<uint32_t>(h, 0x30) = (uint32_t)word & 0x7FFFFFFF;
			}
			*(const uint8_t **)(h + 0x2C) = frame + 4;
			PacketCursor() = Flipbook(h, OT() + 0x44, 2, PacketCursor());
		}
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// MAG_070_sub_6DFC70: a bone's position (entity matrix, half scale, + entity translation);
	// bone ids >= 0xF0 go through the skeleton's id table; 1 = the bone is not there
	// ------------------------------------------------------------------
	static int32_t BonePosition(uint8_t *e, int32_t bone, int32_t *out)
	{
		const uint8_t *skel = **(uint8_t *const *const *)(e + 0x64);
		if (bone >= 0xF0) bone = skel[bone - 0xEC];
		const uint8_t *b = skel + 0x10 + (uint32_t)bone * 0x30;
		if (*(const int16_t *)b < 0) return 1;
		TransformVec3(e + 0x40, (const int32_t *)(b + 0x24), out);
		out[0] = (int32_t)((uint32_t)((out[0] - (out[0] >> 31)) >> 1) + (uint32_t)F<int32_t>(e, 0x54));
		out[1] = (int32_t)((uint32_t)((out[1] - (out[1] >> 31)) >> 1) + (uint32_t)F<int32_t>(e, 0x58));
		out[2] = (int32_t)((uint32_t)((out[2] - (out[2] >> 31)) >> 1) + (uint32_t)F<int32_t>(e, 0x5C));
		return 0;
	}

	// ------------------------------------------------------------------
	// MAG_070_sub_6DFB80: shards, the model 0x1324ED8 at a bone of their target, squashed by the
	// shard's scales and turned by the side (module renderer, depth -0x100, flag 0x10)
	// ------------------------------------------------------------------
	static void ShardsDraw(const Shard *s)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x78);
		const int32_t angle = (int32_t)(((Side() <= 0 ? 0u : 0xFFFFFFFFu) & 0xFFFFF800u) + 0x800u);
		F<uint32_t>(h, 0x38) = 0xFFFFFF00;
		F<uint32_t>(h, 0x3C) = 0x10;
		for (int n = 0x40; n != 0; n--, s++)
		{
			if (s->life == 0) continue;
			ShardMatrix(angle, s->sy, s->sxz, h);
			BonePosition(s->rec->entity, s->bone, (int32_t *)(h + 0x14));
			F<int32_t>(h, 0x18) = (int32_t)((uint32_t)F<int32_t>(h, 0x18) + (uint32_t)sdiv_pow2(s->rec->y_min + s->rec->y_max, 2));
			ComposeAffineTransform(&Camera(), (Mat4x3 *)h, (Mat4x3 *)h);
			GteSetRotMatrix((Mat4x3 *)h);
			GteSetTransVector((Mat4x3 *)h);
			F<uint32_t>(h, 0x20) = SHARD_MODEL;
			PacketCursor() = RenderModel(h + 0x20, OT() + 0x44, 2, PacketCursor());
		}
		FieldFree(0x78);
	}

	// ------------------------------------------------------------------
	// MAG_070_sub_6DFD90: a model's vertical extent in its bone space (every shown object's
	// vertices through their bones, IR2 min / max) -> out[0] = max, out[1] = min; returns min
	// ------------------------------------------------------------------
	static int32_t MeasureHeight(uint8_t *e, int16_t *out)
	{
		ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
		uint8_t *h = (uint8_t *)FieldAlloc(0x18);
		F<uint32_t>(h, 4) = 0xFFFF0000;
		F<uint32_t>(h, 0) = 0x10000;
		F<uint32_t>(h, 0x14) = F<uint32_t>(e, 0x7C);
		const uint8_t *model = F<uint8_t *>(e, 0x64);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		F<uint32_t>(h, 8) = 0;
		F<uint16_t>(h, 0xC) = 0x1000;
		const uint8_t *objs = *(const uint8_t *const *)(model + 4);
		const int32_t count = *(const int32_t *)objs;
		const uint32_t *offs = (const uint32_t *)(objs + 4);
		for (int32_t i = 0; i < count; i++)
		{
			const uint8_t *p = *(const uint8_t *const *)(model + 4) + offs[i];
			if (!(F<uint32_t>(h, 0x14) & (1u << (i & 31)))) continue;
			int32_t ng = *(const int16_t *)p;
			p += 2;
			if (ng <= 0) continue;
			do
			{
				const int32_t bone = *(const int16_t *)p;
				p += 2;
				const Mat4x3 *bm = (const Mat4x3 *)(bones + (uint32_t)bone * 0x30 + 0x10);
				GteSetRotMatrixCtrl(bm);
				GteSetTransVectorCtrl(bm);
				int32_t nv = *(const int16_t *)p;
				p += 2;
				for (; nv != 0; nv--)
				{
					GteLoadV0u(p);
					GteRTPS();
					p += 6;
					GteReadIR2(h + 0x10);
					const int32_t v = F<int16_t>(h, 0x10);
					if (F<int32_t>(h, 0) > v) F<int32_t>(h, 0) = v;
					if (F<int32_t>(h, 4) < v) F<int32_t>(h, 4) = v;
				}
			} while (--ng != 0);
		}
		out[0] = F<int16_t>(h, 4);
		out[1] = F<int16_t>(h, 0);
		FieldFree(0x18);
		BuildBoneMatricesFromPose(e + 0x60);
		return out[1];
	}

	// the layout parameter block of the Lift / Rise tasks: on the target, scaled by its height
	static void TargetArg(PrimArg &arg, const Target *rec, uint8_t *e)
	{
		arg.pos[0] = E16(e, 0x1C);
		arg.pos[2] = E16(e, 0x20);
		const int32_t v = rec->y_max + rec->y_min;
		arg.pos[1] = (int16_t)((int16_t)((v - (v >> 31)) >> 1) + E16(e, 0x1E));
		arg.morph = TexBase() + 0x241C;
		const int32_t d = rec->y_min - rec->y_max;
		arg.scaled = 1;
		arg.scale[2] = d;
		arg.scale[1] = d;
		arg.scale[0] = d;
		arg.objs = OBJS_A;
		arg.depth = (int16_t)0xFF00;
	}

	// ------------------------------------------------------------------
	// LiftTask (0x6DFF00): node +0x0C target, +0x10 counter, +0x14 layout 0x1326C68
	// ------------------------------------------------------------------
	static uint32_t __cdecl LiftTask(TaskNode *n)
	{
		SubNode *s = (SubNode *)n;
		const int32_t n12 = shl32(s->t.counter, 12);
		int32_t q = (mulhi_s(n12, (int32_t)0x8D3DCB09) + n12) >> 4;
		q += (int32_t)((uint32_t)q >> 31);                    // counter x 0x1000 / 29
		Target *rec = s->t.rec;
		uint8_t *e = rec->entity;
		const int32_t slot = SlotOf(e);
		PrimArg arg;
		TargetArg(arg, rec, e);
		// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
		FX_HELD(held_note_play(K_LIFT, s, &arg, s->t.counter);)
		prim::play((prim::Layout *)s->layout, PrimPart, (int)&arg, 0);
		if (s->t.counter == 5)
		{
			// a ring of 16 sparks from the target's position
			Spark *p = SparkAlloc();
			if (p)
			{
				int32_t ang = 0x1000;
				do
				{
					const int32_t r = CrtRand();
					p->pos[0] = E16(e, 0x1C);
					p->pos[1] = E16(e, 0x1E);
					p->pos[2] = E16(e, 0x20);
					const int32_t speed = (r & 0xF) + 0x10;
					p->scale = (int16_t)((CrtRand() & 0x800) + 0xC00);
					p->vel[0] = (int8_t)(mul32(ComputeSin(ang), speed) >> 12);
					p->vel[2] = (int8_t)(mul32(ComputeCos(ang), speed) >> 12);
					p->vel[1] = 0;
					p->seq = (const uint8_t *)BURST_SEQ;
					p->life = 0x10;
					ang -= 0x100;
					if (ang == 0) break;
					p = SparkAlloc();
				} while (p);
			}
		}
		const int32_t rest = 0x1000 - q;
		if (slot & 1)
		{
			const int16_t d = (int16_t)sdiv_pow2(rest, 3);
			rec->ang[0] = (int16_t)(rec->ang[0] + d);
			rec->ang[1] = (int16_t)(rec->ang[1] + d);
		}
		else
		{
			const int16_t d = (int16_t)sdiv_pow2(rest, 3);
			rec->ang[0] = (int16_t)(rec->ang[0] - d);
			rec->ang[1] = (int16_t)(rec->ang[1] - d);
		}
		E16(e, 0x1E) = (int16_t)sdiv_pow2(shl32(mul32(rest, rest) >> 12, 12), 12);
		s->t.counter++;
		if (s->t.counter >= 0x1E)
		{
			*(uint16_t *)e &= 0xFFF7;
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// RingTask (0x6E02E0): node +0x0C position, +0x14 layout 0x1324FF0; ends with the layout
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		SubNode *s = (SubNode *)n;
		PrimArg arg;
		arg.scaled = 0;
		arg.objs = OBJS_A;
		arg.pos[0] = s->pos[0];
		arg.pos[2] = s->pos[2];
		arg.pos[1] = s->pos[1];
		arg.morph = TexBase() + 0x241C;
		arg.depth = (int16_t)0xFF80;
		// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
		FX_HELD(held_note_play(K_RING, s, &arg, 0);)
		const int left = prim::play((prim::Layout *)s->layout, PrimPart, (int)&arg, 0);
		return left != 0 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// RiseTask (0x6E0100): node +0x0C target, +0x10 counter, +0x12 lift length, +0x14 layout
	// 0x1325A88. Never ends by itself (the master empties the queue).
	// ------------------------------------------------------------------
	static uint32_t __cdecl RiseTask(TaskNode *n)
	{
		SubNode *s = (SubNode *)n;
		Target *rec = s->t.rec;
		uint8_t *e = rec->entity;
		PrimArg arg;
		TargetArg(arg, rec, e);
		// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
		FX_HELD(held_note_play(K_RISE, s, &arg, s->t.counter);)
		prim::play((prim::Layout *)s->layout, PrimPart, (int)&arg, 0);
		const uint32_t i = (uint32_t)((int32_t)s->t.counter - 0xD);
		const uint32_t len = (uint32_t)(int32_t)s->t.delay;
		if (i < len)
		{
			const int32_t t = (int32_t)((i << 12) / len);
			const int32_t slot = SlotOf(e);
			if (i == 0)
			{
				SubNode *ring = (SubNode *)AddTaskToQueue(QSub(), ORIG_RingTask);
				ring->pos[0] = E16(e, 0x1C);
				ring->pos[2] = E16(e, 0x20);
				ring->pos[1] = 0;
				DecodeModelPrimLayout(LAYOUT_RING, ring->layout, 0xD8);
			}
			const int16_t d = (int16_t)sdiv_pow2(t, 3);
			if (slot & 1)
			{
				rec->ang[0] = (int16_t)(rec->ang[0] + d);
				rec->ang[1] = (int16_t)(rec->ang[1] + d);
			}
			else
			{
				rec->ang[0] = (int16_t)(rec->ang[0] - d);
				rec->ang[1] = (int16_t)(rec->ang[1] - d);
			}
			int32_t v = shl32(mul32(t, t) >> 12, 14);
			v = (v - (v >> 31)) >> 1;
			v = sdiv_pow2(v, 12);
			E16(e, 0x1E) = (int16_t)(*(const int16_t *)((const uint8_t *)rec + 6) - (int16_t)v);
			int16_t vec[3];
			vec[2] = 0;
			vec[0] = 0;
			vec[1] = (int16_t)((rec->y_min + rec->y_max) >> 1);
			// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
			FX_HELD(held_note_model(K_RMODEL, rec, vec, 0x1000, s->t.counter, (int32_t)len);)
			DrawTargetModel(e, rec->ang, vec, 0x1000);
			*e |= 4;
			s->t.counter++;
			return 0;
		}
		if (i < 0xC8)
		{
			if (i == len) E16(e, 0x1E) = (int16_t)0xD000;
			E16(e, 0x1E) = (int16_t)(E16(e, 0x1E) + (int16_t)0xFF00);
		}
		s->t.counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x6DDD50)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// screen flash ramps
		if (m->counter < 0xC)
		{
			const int32_t v = (int32_t)m->counter * 2300;
			int32_t q = mulhi_s(v, 0x2AAAAAAB) >> 1;
			Flash() = q + (int32_t)((uint32_t)q >> 31);
		}
		else if (m->counter >= 0xFA)
		{
			const int32_t v = (0x106 - (int32_t)m->counter) * 2300;
			int32_t q = mulhi_s(v, 0x2AAAAAAB) >> 1;
			Flash() = q + (int32_t)((uint32_t)q >> 31);
		}
		else
			Flash() = 0x8FC;

		// 0x32..0x4F: the centre layout
		if ((uint32_t)((int32_t)m->counter - 0x32) < 0x1E)
		{
			if (m->counter == 0x32) DecodeModelPrimLayout(LAYOUT_CENTRE, m->layout, 0x218);
			PrimArg arg;
			arg.pos[0] = (int16_t)CentreX();
			arg.pos[2] = (int16_t)CentreZ();
			arg.pos[1] = (int16_t)0xC000;
			arg.morph = TexBase() + 0x241C;
			arg.scaled = 0;
			arg.objs = OBJS_A;
			arg.depth = (int16_t)0xFF80;
			// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
			FX_HELD(held_note_play(K_CENTRE, m, &arg, m->counter);)
			prim::play((prim::Layout *)m->layout, PrimPart, (int)&arg, 0);
		}

		const int32_t s = (int32_t)m->counter - 0x50;
		if ((uint32_t)s < 0xB6)
		{
			// 0xCE..0xD5: a white tile fading in
			const uint32_t w = (uint32_t)(s - 0x7E);
			if (w < 8)
			{
				int32_t level;
				if ((int32_t)w < 7)
				{
					const int32_t v = shl32((int32_t)w + 1, 12);
					int32_t q = (mulhi_s(v, (int32_t)0x92492493) + v) >> 2;
					level = q + (int32_t)((uint32_t)q >> 31);
				}
				else if ((int32_t)w < 8) level = 0x1000;
				else level = shl32(8 - (int32_t)w, 12);
				// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
				FX_HELD(held_note_tile(level, m->counter);)
				PacketCursor() = Tile(0xFF, 0xFF, 0xFF, level, PacketCursor());
			}
			bool behind = true;      // the targets redrawn, the layout behind them
			if (s == 0)
			{
				InitTaskQueuePool(QSub(), (void *)TexBase(), 0x114, 6);
				EntitiesFlag4On();
				DecodeModelPrimLayout(LAYOUT_WAVE, m->layout, 0x1100);
				for (int32_t k = 0; k < m->ntargets; k++)
				{
					SubNode *t = (SubNode *)AddTaskToQueue(QSub(), ORIG_LiftTask);
					Target *rec = &m->targets[k];
					*(int16_t *)(rec->entity + 0x1E) = 0x1000;
					if (SlotOf(rec->entity) & 1)
					{
						rec->ang[1] = (int16_t)0xE20B;
						rec->ang[0] = (int16_t)0xE20B;
					}
					else
					{
						rec->ang[1] = 0x1DF5;
						rec->ang[0] = 0x1DF5;
					}
					rec->ang[2] = 0;
					t->t.counter = 0;
					DecodeModelPrimLayout(LAYOUT_LIFT, t->layout, 0xBC);
					t->t.rec = rec;
				}
			}
			else
			{
				if ((uint32_t)s >= 0xD && (uint32_t)s <= 0x28)
				{
					// 0x5D..0x78: four sparks a tick around the centre, alternately rising / falling
					int32_t k = 4;
					Spark *p = SparkAlloc();
					while (p)
					{
						CrtRand();
						if (k & 1)
						{
							p->pos[1] = 0x1000;
							p->vel[1] = (int8_t)0xF4;
						}
						else
						{
							p->pos[1] = (int16_t)0xF000;
							p->vel[1] = 0xC;
						}
						const int32_t a = CrtRand();
						p->pos[0] = (int16_t)((uint32_t)(ComputeSin(a) >> 3) + CentreX());
						p->pos[2] = (int16_t)((uint32_t)((ComputeCos(a) >> 3) - shl32(Side(), 11)) + CentreZ());
						p->scale = 0x200;
						CrtRand();
						p->vel[2] = 0;
						p->vel[0] = 0;
						p->seq = (const uint8_t *)SPARK_SEQ;
						p->life = 0x50;
						if (--k == 0) break;
						p = SparkAlloc();
					}
				}
				else if ((uint32_t)s >= 0x73 && (uint32_t)s < 0x83)
				{
					// 0xC3..0xD2: two shards a tick off every target
					for (int32_t k = 0; k < m->ntargets; k++)
					{
						Target *rec = &m->targets[k];
						const int32_t height = rec->y_max - rec->y_min;
						uint8_t *e = rec->entity;
						int32_t j = 2;
						Shard *p = ShardAlloc();
						while (p)
						{
							p->life = 6;
							const int32_t nb = ***(const uint8_t *const *const *)(e + 0x64); // skeleton bone count
							p->bone = (int16_t)(mul32(CrtRand(), nb) >> 15);
							const int32_t r = CrtRand();
							p->rec = rec;
							const int32_t v = sdiv_pow2(mul32(r, height), 16) + height;
							p->sxz = (int16_t)v;
							const int16_t a = (int16_t)((int16_t)v >> 1);
							p->sy = a;
							p->dsy = (int16_t)(a >> 4);
							p->dsxz = (int16_t)(a >> 2);
							if (--j == 0) break;
							p = ShardAlloc();
						}
					}
				}
				if ((uint32_t)s >= 0x86)
				{
					const int32_t u = s - 0x86;
					if (u == 0)
					{
						for (int32_t k = 0; k < m->ntargets; k++)
						{
							Target *rec = &m->targets[k];
							*(uint16_t *)rec->entity &= 0xFFF3;
							*(uint32_t *)(rec->entity + 0x1C) = rec->pos_xy;
							*(uint32_t *)(rec->entity + 0x20) = rec->pos_z;
						}
						RestoreEntityFlags();
					}
					if (Flash() >= 0x8FC && u < 0x20) Flash() = 0x1000 - sdiv_pow2(u * 1796, 5);
					behind = false;
				}
			}
			int32_t zoff = 0;
			int16_t depth = 0;
			if (behind)
			{
				// the targets' models redrawn rotated, half size
				zoff = shl32(-Side(), 11);
				Flash() = 0x1000;
				int16_t vec[3];
				vec[2] = 0;
				vec[0] = 0;
				for (int32_t k = 0; k < m->ntargets; k++)
				{
					Target *rec = &m->targets[k];
					vec[1] = (int16_t)((rec->y_max + rec->y_min) >> 1);
					// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
					FX_HELD(held_note_model(K_MODEL, rec, vec, 0x800, m->counter, 0);)
					DrawTargetModel(rec->entity, rec->ang, vec, 0x800);
					*rec->entity |= 4;
				}
			}
			else
				depth = (int16_t)0xFC00;
			PrimArg arg;
			arg.pos[0] = (int16_t)CentreX();
			arg.morph = TexBase() + 0x241C;
			arg.pos[1] = 0;
			arg.scaled = 0;
			arg.pos[2] = (int16_t)(zoff + (int32_t)CentreZ());
			arg.objs = OBJS_WAVE;
			arg.depth = depth;
			// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
			FX_HELD(held_note_play(K_WAVE, m, &arg, m->counter);)
			prim::play((prim::Layout *)m->layout, PrimPart, (int)&arg, 0);
		}

		// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
		FX_HELD(held_note_sparks(m);)
		SparksDraw((const Spark *)(TexBase() + 0x181C));
		// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
		FX_HELD(held_note_shards(m);)
		ShardsDraw((const Shard *)(TexBase() + 0x201C));
		SparksUpdate();
		ShardsUpdate();

		if (m->counter == 0) BdPlaySE(SOUND_0, 0, 0x80);
		else if (m->counter == 0x50) BdPlaySE(SOUND_50, 0, 0x80);
		else if (m->counter == 0xAA) BdPlaySE(SOUND_AA, 0, 0x80);
		if (m->counter == 0x102) ReleaseVoice(m->voice);

		// 1, 9, 0x11: a RiseTask on target (counter - 1) / 8
		{
			const uint32_t c1 = (uint32_t)((int32_t)m->counter - 1);
			if (c1 < 0x18 && (c1 & 7) == 0 && (int32_t)(c1 >> 3) < m->ntargets)
			{
				Target *rec = &m->targets[c1 >> 3];
				SubNode *t = (SubNode *)AddTaskToQueue(QSub(), ORIG_RiseTask);
				*rec->entity |= 8;
				MeasureHeight(rec->entity, &rec->y_max);
				t->t.counter = 0;
				t->t.delay = (int16_t)(0x32 - (int32_t)c1);
				DecodeModelPrimLayout(LAYOUT_RISE, t->layout, 0x100);
				t->t.rec = rec;
			}
		}

		// battle camera
		const int32_t X = (int32_t)CentreX(), Z = (int32_t)CentreZ(), sd = Side();
		if (m->counter == 0)
		{
			CameraArmReturn();
			ViewWord() = 0x120;
			CameraZoom() = 0;
			CamLookAt()[1] = (int16_t)0xFCD9;
			CamLookAt()[0] = (int16_t)(X + 8);
			CamEye()[1] = (int16_t)0xF491;
			CamLookAt()[2] = (int16_t)(Z + sd * 340);
			CamEye()[0] = (int16_t)(X + 0x5A2);
			CamEye()[2] = (int16_t)(Z + sd * 3431);
		}
		if ((uint32_t)((int32_t)m->counter - 0x14) < 0x27)
		{
			const uint32_t a = (uint32_t)((int32_t)m->counter - 0x14) << 12;
			int16_t k0[6], k1[6];
			k1[0] = (int16_t)(X + 8);
			k1[1] = (int16_t)0xFCD9;
			k1[2] = (int16_t)(Z + sd * 340);
			k1[3] = (int16_t)(X + 0x5A2);
			k1[4] = (int16_t)0xF491;
			k1[5] = (int16_t)(Z + sd * 3431);
			k0[0] = (int16_t)(X - 0x66);
			k0[1] = (int16_t)0xDF3F;
			k0[2] = (int16_t)(Z + sd * 2438);
			k0[3] = (int16_t)(X + 0x1A2);
			k0[4] = 0x6CD;
			k0[5] = (int16_t)(Z + sd * 10573);
			const uint32_t hi = mulhi_u(a, 0xAF286BCB);
			const uint32_t angle = ((((a - hi) >> 1) + hi) >> 5) >> 2;
			CamLerp(k1, k0, 0x1000 - ComputeCos((int32_t)angle));
		}
		if (m->counter == 0x3B)
		{
			CamLookAt()[1] = (int16_t)0xBFC3;
			CamLookAt()[0] = (int16_t)(X - 0x16D);
			CamEye()[1] = (int16_t)0xCAB7;
			CamLookAt()[2] = (int16_t)(Z + sd * 538);
			CamEye()[0] = (int16_t)(X - 0x169);
			CamEye()[2] = (int16_t)(Z + sd * 8304);
		}
		if ((uint32_t)((int32_t)m->counter - 0x4F) < 0x28)
		{
			const uint32_t a = (uint32_t)((int32_t)m->counter - 0x4F) << 12;
			int16_t k0[6], k1[6];
			ViewWord() = 0xC8;
			k0[0] = (int16_t)(X - 0x1D);
			k0[1] = 0xBB7;
			k0[2] = (int16_t)(Z - sd * 291);
			k0[3] = (int16_t)(X - 0x18D5);
			k0[4] = (int16_t)0xE9DA;
			k0[5] = (int16_t)(Z + sd * 4421);
			k1[0] = (int16_t)(X - 0xCC);
			k1[1] = (int16_t)0xFFF3;
			k1[2] = (int16_t)(Z - sd * 2255);
			k1[3] = (int16_t)(X + 0x1B);
			k1[4] = (int16_t)0xFBCF;
			k1[5] = (int16_t)(Z + sd * 4566);
			const uint32_t hi = mulhi_u(a, 0xA41A41A5);
			const uint32_t t = (((a - hi) >> 1) + hi) >> 5;
			CamLerp(k0, k1, (int32_t)t);
		}
		if ((uint32_t)((int32_t)m->counter - 0x77) < 0x3C)
		{
			const uint32_t a = (uint32_t)((int32_t)m->counter - 0x77) << 12;
			int16_t k0[6], k1[6];
			k0[0] = (int16_t)(X - 0xCC);
			k0[1] = (int16_t)0xFFF3;
			k0[2] = (int16_t)(Z - sd * 2255);
			k0[3] = (int16_t)(X + 0x1B);
			k0[4] = (int16_t)0xFBCF;
			k0[5] = (int16_t)(Z + sd * 4566);
			k1[0] = (int16_t)(X - 0xCD);
			k1[1] = (int16_t)0xFFF2;
			k1[2] = (int16_t)(Z - sd * 2256);
			k1[3] = (int16_t)(X + 0x22BE);
			k1[4] = (int16_t)0xFB3E;
			k1[5] = (int16_t)(Z - sd * 2368);
			const uint32_t t = mulhi_u(a, 0x22B63CBF) >> 3;
			CamLerp(k0, k1, (int32_t)t);
		}
		if (m->counter == 0xD5)
		{
			ViewWord() = var<int16_t>(0x1D977A0);
			CamLookAt()[0] = (int16_t)(X + 0x2BE);
			CamEye()[0] = (int16_t)(X + 0x1AA2);
			CamLookAt()[1] = 0x63;
			CamEye()[1] = (int16_t)0xE1DE;
			CamLookAt()[2] = (int16_t)(Z + sd * 459);
			CamEye()[2] = (int16_t)(Z + sd * 445);
			StreamLoad(0x168, TexBase() + 0x269C, 1);
			StreamLoad(0x169, TexBase() + 0x269C, 0);
		}
		StreamPump();
		if (m->counter == 0xFC)
		{
			const ActionData *act = ((const CastContext *)Ctx())->actions;
			ApplyActionResultToTargets(act->targets, act->target_count);
		}
		if (m->counter >= 0x106)
		{
			RestoreEntityFlags();
			Flash() = 0;
			ViewWord() = var<int16_t>(0x1D977A0);
			QSub()->tail = nullptr;
			QSub()->head = nullptr;
			return StreamPending(1) != 0 ? TASK_END : 0;
		}
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6DD970)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
		FX_HELD(held_note_root();)
		if (r->parity)
		{
			PacketCursor() = TexBase() + 0xAE9C;
			ArenaEnd() = TexBase() + 0x1AE9C;
			r->parity = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x1AE9C;
			ArenaEnd() = TexBase() + 0x2AE9C;
			r->parity = 1;
		}
		Flash() = 0;
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			SaveEntityFlags();
			ClearSparks();
			ClearShards();
			StreamStateInit((void *)(TexBase() + 0x678));
			InitTaskQueuePool(QMaster(), (void *)(TexBase() + 0x6C0), 0x115C, 1);
			InitTaskQueuePool(QSub(), (void *)TexBase(), 0x114, 6);
			MasterNode *m = (MasterNode *)AddTaskToQueue(QMaster(), ORIG_MasterTask);
			Memset32(&m->counter, 0, 0x454);
			m->voice = ClaimVoiceSlot(SOUND_Voice, 1, 0x80);
			const ActionData *act = ((const CastContext *)Ctx())->actions;
			int16_t minx = 0x7FFF, miny = 0x7FFF, minz = 0x7FFF;
			int16_t maxx = (int16_t)0x8001, maxy = (int16_t)0x8001, maxz = (int16_t)0x8001;
			m->ntargets = act->target_count;
			for (int32_t i = 0; i < m->ntargets; i++)
			{
				Target *rec = &m->targets[i];
				uint8_t *e = Entity(((const CastContext *)Ctx())->actions->targets[i * TARGET_STRIDE]);
				rec->entity = e;
				rec->pos_xy = *(const uint32_t *)(e + 0x1C);
				rec->pos_z = *(const uint32_t *)(e + 0x20);
				*(uint32_t *)rec->ang = InitAngles0();
				*(uint32_t *)&rec->ang[2] = InitAngles1();
				int16_t p[4];
				GetDefaultEffectPosition(e, p);
				if (p[0] < minx) minx = p[0];
				if (p[0] > maxx) maxx = p[0];
				if (p[1] < miny) miny = p[1];
				if (p[1] > maxy) maxy = p[1];
				if (p[2] < minz) minz = p[2];
				if (p[2] > maxz) maxz = p[2];
				Side() = SlotOf(e) >= 3 ? -1 : 1;
			}
			int32_t v = maxx + minx;
			*(int16_t *)&CentreX() = (int16_t)((v - (v >> 31)) >> 1);
			v = maxy + miny;
			CentreY() = (int16_t)((v - (v >> 31)) >> 1);
			v = maxz + minz;
			*(int16_t *)&CentreZ() = (int16_t)((v - (v >> 31)) >> 1);
		}
		int a = (int)n;
		if (r->started)
		{
			EffectCameraMatrix(&Camera(), &EffCam());
			a = ExecuteTaskQueue(QMaster());
			ExecuteTaskQueue(QSub());
		}
		if (Flash() != FlashPrev())
		{
			SetScreenFlash((uint32_t)Flash(), 0);
			const int32_t f = Flash();
			if (f >= 0x1000)
			{
				for (uint32_t b = 0x1D98991; b < 0x1D98A41; b += 0x2C) *(uint8_t *)b &= 0xFD;
			}
			else if (FlashPrev() >= 0x1000)
			{
				for (uint32_t b = 0x1D98991; b < 0x1D98A41; b += 0x2C) *(uint8_t *)b |= 2;
			}
			FlashPrev() = f;
		}
		if (r->started && a == 0)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		r->counter++;
		return 0;
	}
}

	void register_mag070_shockwave_pulsar()
	{
		register_port(sp070::ORIG_RootTask, (void *)sp070::RootTask, "P070 RootTask", 70);
		register_port(sp070::ORIG_MasterTask, (void *)sp070::MasterTask, "P070 MasterTask", 70);
		register_port(sp070::ORIG_LiftTask, (void *)sp070::LiftTask, "P070 LiftTask", 70);
		register_port(sp070::ORIG_RiseTask, (void *)sp070::RiseTask, "P070 RiseTask", 70);
		register_port(sp070::ORIG_RingTask, (void *)sp070::RingTask, "P070 RingTask", 70);
		// // 30 fps layer: see mag070_shockwave_pulsar_held.inc
		FX_HELD(register_mag070_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag070_shockwave_pulsar_held.inc"
#endif
