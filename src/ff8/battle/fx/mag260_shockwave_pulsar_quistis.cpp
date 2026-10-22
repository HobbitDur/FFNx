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

// Effect 260: Shockwave Pulsar (Quistis' Blue Magic, kernel.bin section 19 "Shockwave Pulsar", magic +4 = 260;
// MAG_260_SHOCKWAVE_PULSAR_QUISTIS*), cast by a party member (Quistis) on the enemies.
//
// The module 0x690690..0x693790 is a hand-written relative of the enemy Shockwave Pulsar 70 (mag070_shockwave_pulsar.cpp):
// the two modules share their draw helpers ACROSS modules. This module's 0x693170 (target model redraw), 0x692F00
// (shard matrix) and the primitive lists 0x691C70 / 0x691F30 / 0x6921C0 / 0x6924F0 / 0x692830 are called by 70; this
// module calls 70's 0x6DF940 (flag 4 on the shown entities) and 70's lists 0x6DEDD0 / 0x6DF140 / 0x6DF560 (ported here
// as native copies: EntitiesFlag4On, FlatQuads, GouraudQuads, GouraudTexQuads). Spark / shard pools, their allocators /
// updates, the flipbook draw and the model measure are 70's text on this module's globals; the root, the master, the
// lift / rise / ring tasks, the prim callback (70's without the scaled block), the shard draw, the screen flash entries
// and the fade tile are this module's own.
//
// Structure (setup 0x690690 -> _Init 0x6906C0, file loader 0x6906A0 = the texture named at 0x112D1A4 into 0x2508610;
// _Init queues the TIM, keeps the magic buffer base 0x25086B8, the cast context 0x2508624 and the CASTER's entity
// 0x2508628, and starts the root task in the root queue 0x2508650, one 0x10-byte node at 0x2508660):
//   RootTask (0x690730) - alternates the packet arena (magic buffer + 0xB650 / + 0x1B650, cursor 0x250861C); the two
//     flash levels 0x25086A8 / 0x25086B0 are reset every tick; on counter 1 saves the seven entities' flag words, empties
//     the spark / shard pools, resets the stream loader state (magic buffer + 0), sets up the master pool (1 x 0x1298 at
//     magic buffer + 0x48) and the sub-task pool (0xC x 0x114 at + 0x2160), starts the master, BINDS THE CASTER'S
//     animation set (com data 0x111B3CC, out block = master + 0x178) and starts her chain transformation 1, keeps every
//     target of action 0 (one only in encounter 0x31C: word 0x1CFF6E0) with its position, scale 0x800000 / (size + 0x200)
//     and whether its linked entities (+0x8C ring) hold no other target ("group": the whole ring is redrawn), and the
//     targets' centre (0x25086A0..: middle of their default effect positions). Then each tick: the effect camera
//     (0x67AAE0 -> 0x25086C0), the master queue, the sub-task queue; flash A (0x25086A8) -> the screen-flash entries 0..2
//     (0x690B70; entity-array bit 1 of entries 0..2 while full), flash B (0x25086B0) -> entry 3 (0x690BA0); ends with the
//     master (screen flash off).
//   Master (0x690C20) - node 0x1298: counter, target count, the targets (0x24 each: entity, saved position, angles,
//     group flag, scale target, scale, model bottom / top), the caster's bind block (+0x178), a prim-model layout
//     (+0x198). A 0x106-tick timeline (then waits for the stream loader):
//       flash A: 0..0x31 ramp in, 0xFA..0x105 out (0x1000 between); flash B: 0..0xB in, 0xFA.. out (0x8FC between)
//       1, 9, 0x11, 0x19      one RiseTask per target (0x6934B0), model height measured (0x693000)
//       0x32..0x4F            a layout (0x112A974) at the targets' centre, 0xC000 high
//       0x50                  sub-task pool reset (the Rise / Ring tasks dropped), flag 4 on the shown entities, the
//                             shockwave layout (0x112015C) and one LiftTask (0x6932C0) per target (entity +0x1E and
//                             +0x58 = 0x1000, angles set)
//       0x50..0xD5            the targets' models (or their rings) redrawn rotated and scaled (0x693170), both flashes
//                             full, the shockwave layout 0x800 deeper; sparks (pool A) on 0x5D..0x78, model shards
//                             (pool B) off the targets on 0xC3..0xD2; a white fade tile (0x692B80) on 0xCE..0xD5
//       0xD6                  positions and flag bits 2 / 3 restored; then both flashes fade out over 0x20 ticks
//       0, 0x14..0x3A, 0x3B, 0x4F..0x76, 0x77..0xB2, 0xD5   battle camera (direct sets and 0x657DF0)
//       0xD5                  stream loads 0x168 / 0x169 into magic buffer + 0x2E50
//       0xFC                  damage (0x506BA0); sounds at 0, 0x50, 0xAA
//       0x106                 flag bits restored, the saved camera view put back (0xB8B800..), the sub queue emptied
//     Every tick it draws the sparks (0x692C40, flipbook 0x6FC9E0) and, before 0xD5, the shards (0x692E10: model 0x111D30C
//     at the target's position through the module renderer 0x691B40), then moves them (0x692DC0 / 0x692FD0).
//   RiseTask (0x6934B0) - node 0x114: its target's scale eases from 0x1000 to the scale target over 0xD ticks; a layout
//     (0x111DEBC) on the target; from tick 0xD the target lifts (entity +0x1E) and spins for len ticks (starting a
//     RingTask 0x693720, layout 0x111D424 at its feet), then drops (-0x100 a tick); the target's model (or ring) is
//     redrawn every tick (0x693170). Never ends by itself (the master resets the pool at 0x50).
//   LiftTask (0x6932C0) - node 0x114: a layout (0x111F09C) on the target for 0x1E ticks, sets the target's height
//     (entity +0x1E), spins its angles; a burst of 16 sparks on tick 5; flag 3 off at the end.
//   RingTask (0x693720) - node 0x114: its layout at a fixed point, ends with the layout.
// Prim-player callback 0x691760: one object (flags 0x40000 / 0x80000: other rotation orders, 0x1000: offset through the
// effect camera, 0x200: screen-axes offset, 0x100: 3x3 scale, 0x4000: blend mode); objects whose type byte is '1' go
// through the module renderer 0x691B40 (the engine's 0x7043B0 with all eight primitive lists of its own), the others
// through 0x7043B0.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: the master draws (centre layout, tile, models, shockwave layout, sparks, shards) THEN updates; Lift and
// Ring draw then update; Rise updates the scale, draws its layout, updates the lift, then redraws the model.
// ENTITY WRITES: caster - animation set (com data 0x111B3CC) + chain transformation 1 (root, counter 1, engine); the
// targets - height +0x1E every tick of the Lift / Rise tasks, matrix translation y +0x58 (0x1000 at 0x50; 0 then +0x1E
// around the bone measure 0x693000), +0x1C / +0x20 restored at 0xD6; flag words: bit 3 on at a Rise spawn, off at a
// Lift end, bit 2 on while redrawn, bits 2 / 3 off and the saved bit 2 back at 0xD6 / 0x106, bit 2 on every shown
// entity at 0x50; action results (0xFC, engine). Screen-flash entries 0x1D98990 + k * 0x2C (k 0..3: +2 level, +0x28
// word) and their bit 1 (+1).
// CAMERA: eye 0xB8B7F0 / look-at 0xB8B7F8 on 0, 0x14..0x3A, 0x3B, 0x4F..0xB2, 0xD5, and back to the saved view
// 0xB8B800 at 0x106; field of view 0x1D8E038 on 0, 0x4F..0x76, 0xD5, 0x106; roll 0x1D977A2 on 0; camera return armed
// at 0 (0x50A730).
// Module globals: 0x2508610..0x25086FC (texture file 0x2508610, pool cursors 0x2508614 / 0x2508618, packet cursor
// 0x250861C, arena end 0x2508620, context 0x2508624, caster 0x2508628, queues 0x2508630 sub / 0x2508640 master /
// 0x2508650 root (+ node 0x2508660), angles 0x2508690, centre 0x25086A0, flashes 0x25086A8..0x25086B4, magic buffer
// base 0x25086B8, effect camera 0x25086C0, saved flags 0x25086E0).
// STACK WORDS: the prim blocks' word +0x06 (never written: only loaded into the GTE V0 pad) and +0x0E (never read); the
// callback's offset pad (V0 pad); the model draw header's +0x30 / +0x38 / +0x3C / +0x54 and the shard header's colour
// bytes (h + 0x28..0x2A: the renderer's far colour) are Field_Alloc scratch residue (the port allocates the same way).

#include "mag_common.h"

namespace ff8fx
{
namespace pulsar260
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&SparkCursor() { return var<uint8_t *>(0x2508614); }     // pool A alloc cursor
	inline uint8_t *&ShardCursor() { return var<uint8_t *>(0x2508618); }     // pool B alloc cursor
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x250861C); }
	inline uint32_t &ArenaEnd() { return var<uint32_t>(0x2508620); }         // (written, never read)
	inline uint8_t *&Ctx() { return var<uint8_t *>(0x2508624); }             // cast context
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2508628); }          // caster entity (setup)
	inline TaskQueue *QSub() { return (TaskQueue *)0x2508630; }              // pool: magic buffer + 0x2160, 0xC x 0x114
	inline TaskQueue *QMaster() { return (TaskQueue *)0x2508640; }           // pool: magic buffer + 0x48, 1 x 0x1298
	inline uint32_t &InitAngles0() { return var<uint32_t>(0x2508690); }      // copied into every target's angles
	inline uint32_t &InitAngles1() { return var<uint32_t>(0x2508694); }
	inline uint32_t &CentreX() { return var<uint32_t>(0x25086A0); }          // targets' centre (x, y words; z)
	inline int16_t &CentreY() { return var<int16_t>(0x25086A2); }
	inline uint32_t &CentreZ() { return var<uint32_t>(0x25086A4); }
	inline int32_t &FlashA() { return var<int32_t>(0x25086A8); }             // screen-flash entries 0..2 this tick
	inline int32_t &FlashAPrev() { return var<int32_t>(0x25086AC); }
	inline int32_t &FlashB() { return var<int32_t>(0x25086B0); }             // screen-flash entry 3 this tick
	inline int32_t &FlashBPrev() { return var<int32_t>(0x25086B4); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x25086B8); }          // Magic_TextureOFF (magic buffer)
	inline Mat4x3 &EffCam() { return var<Mat4x3>(0x25086C0); }               // effect camera matrix (0x67AAE0)
	inline uint32_t *SavedFlags() { return (uint32_t *)0x25086E0; }          // 7 entities' flag words
	inline uint32_t OT() { return var<uint32_t>(0x1D8E04C); }                // g_Battle_FrameRenderListBase
	inline uint32_t &FrameArena() { return var<uint32_t>(0x1D8E054); }
	inline int16_t &ViewWord() { return var<int16_t>(0x1D8E038); }
	inline int16_t &CameraZoom() { return var<int16_t>(0x1D977A2); }
	inline int16_t *CamEye() { return (int16_t *)0xB8B7F0; }
	inline int16_t *CamLookAt() { return (int16_t *)0xB8B7F8; }

	static const uint32_t ORIG_RootTask = 0x690730;
	static const uint32_t ORIG_MasterTask = 0x690C20;
	static const uint32_t ORIG_LiftTask = 0x6932C0;
	static const uint32_t ORIG_RiseTask = 0x6934B0;
	static const uint32_t ORIG_RingTask = 0x693720;
	static const uint32_t ORIG_Callback = 0x691760;    // not a task: the layouts' prim callback
	static const uint32_t LAYOUT_CENTRE = 0x112A974;   // 0x218
	static const uint32_t LAYOUT_WAVE = 0x112015C;     // 0x1100
	static const uint32_t LAYOUT_LIFT = 0x111F09C;     // 0xBC
	static const uint32_t LAYOUT_RISE = 0x111DEBC;     // 0x100
	static const uint32_t LAYOUT_RING = 0x111D424;     // 0xD8
	static const uint32_t OBJS_A = 0x112D190;          // object type strings ('1' = module renderer)
	static const uint32_t OBJS_WAVE = 0x112D1B4;
	static const uint32_t SPARK_SEQ = 0x111D0F8;       // pool A flipbooks
	static const uint32_t BURST_SEQ = 0x111CF4C;
	static const uint32_t SHARD_MODEL = 0x111D30C;
	static const uint32_t COM_Anim = 0x111B3CC;        // the caster's animation set (com data)
	static const void *const SOUND_0 = (const void *)0x111CF40;
	static const void *const SOUND_50 = (const void *)0x111CF44;
	static const void *const SOUND_AA = (const void *)0x111CF48;
	static const uint32_t SPARKS = 0x12E0, SPARKS_LAST = 0x1AD0;   // magic buffer: pool A (0x80 x 0x10)
	static const uint32_t SHARDS = 0x1AE0, SHARDS_LAST = 0x1ED0;   // magic buffer: pool B (0x40 x 0x10)
	static const uint32_t MORPH = 0x1EE0;              // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0xB650, ARENA_B = 0x1B650, ARENA_C = 0x2B650;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_RotationFromAnglesYXZ
	inline void RotationFromAnglesC(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701630)(angles, out); }  // MAG_RotationFromAnglesXYZ
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // MatrixMultiply3x3: a = a * b (3x3)
	inline void Apply3D(const Mat4x3 *m, const int16_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int32_t *)>(0x56C3D0)(m, in, out); } // apply3DTransform (rotation only)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_RenderPrimModel
	inline uint32_t RenderGeometry(void *geom, void *hdr, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(geom, hdr, ot, shift, cursor); }
	inline uint32_t Tile(int32_t r, int32_t g, int32_t b, int32_t level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x7040B0)(r, g, b, level, cursor); } // Effect_FullScreenTile
	inline uint32_t Flipbook(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x6FC9E0)(h, ot, mode, cursor); } // MAG_065_RenderFlipbook
	// MAG_325_CameraLerp (Diablos' module): look-at = a[0..2] -> b[0..2], eye = a[3..5] -> b[3..5] at t
	inline void CamLerp(const int16_t *a, const int16_t *b, int32_t t) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t)>(0x657DF0)(a, b, t); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }    // BS_Camera_ArmReturnAfterEffect
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); } // Stream_QueueLoad
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }          // Stream_Pump
	inline int32_t StreamPending(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); } // au_re_BattleFile_preLoad_0
	inline void BindEntityModelFromComData(uint8_t *entity, void *out, uint32_t data) { fn<void (__cdecl *)(uint8_t *, void *, uint32_t)>(0x5022C0)(entity, out, data); } // BattleEntity_BindExternalAnimSet
	inline void QueueChainTransformation(uint8_t *entity, int32_t id) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, id); }
	inline void TransformVec3(const void *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const void *, const int32_t *, int32_t *)>(0x56C600)(m, in, out); } // TransformVectorBy3x3Matrix
	// software GTE
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // GTE_LoadV0_u16
	inline void GteReadIR2(void *dst) { fn<void (__cdecl *)(void *)>(0x703F90)(dst); }        // GTE_ReadIR2Word
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); } // GTE_ReadSXY012_Split
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }    // GTE_ReadOTZ (dword)
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // GTE_SetFarColor
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }
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
	// the next entity of a linked ring (+0x8C)
	inline uint8_t *NextLinked(const uint8_t *e) { return *(uint8_t *const *)(e + 0x8C); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes (0x2508660)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t parity;    // +0x0E packet arena parity
		uint8_t started;   // +0x0F master started
	};
	struct Target // 0x24 bytes
	{
		uint8_t *entity;
		uint32_t pos_xy;   // +0x04 entity +0x1C at the start (+6 = the height word)
		uint32_t pos_z;    // +0x08 entity +0x20 at the start
		int16_t ang[3];    // +0x0C rotation of the redrawn model (from 0x2508690)
		int16_t pad12;
		int32_t group;     // +0x14 1: the linked ring holds no other target (the whole ring is redrawn)
		int32_t scale_to;  // +0x18 0x800000 / (entity +0x26 + 0x200)
		int32_t scale;     // +0x1C model scale (the Rise task eases it to scale_to; read as a word)
		int16_t y_max;     // +0x20 model bottom (largest bone-space y, 0x693000)
		int16_t y_min;     // +0x22 model top
	};
	struct MasterNode // pool of 1 node of 0x1298 bytes (magic buffer + 0x48)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t ntargets;  // +0x0E
		Target targets[10]; // +0x10
		uint8_t bind[0x20]; // +0x178 the caster's BS_BindEntityModelFromComData block
		uint8_t layout[0x1100]; // +0x198
	};
	struct SubNode // pool of 0xC nodes of 0x114 bytes (magic buffer + 0x2160): Lift / Rise / Ring tasks
	{
		TaskNode hdr;
		union
		{
			struct { Target *rec; int16_t counter; int16_t len; } t; // Lift (len unused), Rise
			int16_t pos[4];                                          // Ring: x, y, z
		};
		uint8_t layout[0x100]; // +0x14
	};
	struct Spark // pool A: magic buffer + 0x12E0, 0x80 x 0x10
	{
		const uint8_t *seq;
		int16_t pos[3];    // +4
		int16_t scale;     // +0xA
		int8_t vel[3];     // +0xC (x 4 per tick)
		uint8_t life;      // +0xF
	};
	struct Shard // pool B: magic buffer + 0x1AE0, 0x40 x 0x10
	{
		int16_t life;
		int16_t off;       // +2 height offset from the target's middle
		int16_t sy, sxz;   // +4 / +6 scale of the shard model
		int16_t dsy, dsxz; // +8 / +0xA
		Target *rec;       // +0xC
	};
	// the prim-player callback's parameter block (a stack block of the caller)
	struct PrimArg
	{
		int16_t pos[3];
		int16_t pad06;     // never written (VZ0 high half)
		uint32_t objs;     // +0x08 object type string
		int16_t depth;     // +0x0C prim header +0x18
		int16_t pad0E;     // never read
		uint32_t morph;    // +0x10 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(MasterNode) == 0x1298 && sizeof(SubNode) == 0x114, "Pulsar 260 nodes");
	static_assert(sizeof(Target) == 0x24 && sizeof(Spark) == 0x10 && sizeof(Shard) == 0x10 && sizeof(PrimArg) == 0x14, "Pulsar 260 blocks");
}
}

#ifdef FF8_FX_HELD
#include "mag260_shockwave_pulsar_quistis_held.h"
#endif

namespace ff8fx
{
namespace pulsar260
{
	// screen clip of a projected vertex (x 0..0xA00, y 0..0x6C0: the PC's 4x subpixel screen)
	static inline bool OutX(const uint8_t *xy) { const int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(const uint8_t *xy) { const int16_t v = *(const int16_t *)(xy + 2); return v < 0 || v > 0x6C0; }

	// the end of a primitive: OTZ + the header's depth (not below 0) -> bucket
	static inline uint32_t Bucket(uint8_t *h, uint32_t ot, int32_t shift)
	{
		int32_t z = (int32_t)((uint32_t)F<int32_t>(h, 0x2C) + (uint32_t)F<int32_t>(h, 0x18));
		F<int32_t>(h, 0x2C) = z;
		if (z < 0) F<int32_t>(h, 0x2C) = 0;
		return ot + (uint32_t)(F<int32_t>(h, 0x2C) >> shift) * 4;
	}
	static inline void ModePrim(uint8_t *p, uint32_t word)
	{
		F<uint32_t>(p, 0) = 0x1000000;
		F<uint32_t>(p, 4) = word;
	}
	// the code word with the semi-transparency switches of the header (on / off bits)
	static inline void Code(uint8_t *h, uint8_t *pk, uint32_t code, uint8_t on, uint8_t off)
	{
		F<uint32_t>(pk, 4) = code;
		if (F<uint8_t>(h, 0x1C) & on) F<uint32_t>(pk, 4) = code | 0x2000000;
		if (F<uint8_t>(h, 0x1C) & off) F<uint32_t>(pk, 4) &= 0xFDFFFFFF;
	}

	// ------------------------------------------------------------------
	// 0x691C70: flat triangles (0xC-byte records: code + rgb, 3 vertex indices) -> POLY_F3 packets between two
	// draw-mode primitives; header flags 1 / 4 semi-transparency on / off, 0x10 double-sided, 0x40 depth-cued colour
	// ------------------------------------------------------------------
	static uint32_t __cdecl FlatTris(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
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
		for (int32_t k = count; k != 0; k--, rec += 0xC)
		{
			GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			GteRTPT();
			F<uint32_t>(pk, 0) = 0x4000000;
			Code(h, pk, *(const uint32_t *)rec, 1, 4);
			GteReadFLAG(h + 0x30);
			if (F<uint32_t>(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0(h + 0x24);
			if (F<int32_t>(h, 0x24) < 0 && !(F<uint8_t>(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0xC, pk + 0x10);
			GteAVSZ3();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0xC)) clip |= 2;
			if (OutX(pk + 0x10)) clip |= 4;
			if (OutY(pk + 8)) clip |= 0x10;
			if (OutY(pk + 0xC)) clip |= 0x20;
			if (OutY(pk + 0x10)) clip |= 0x40;
			if ((clip & 7) == 7) continue;
			if ((clip & 0x70) == 0x70) continue;
			GteReadOTZWord(h + 0x2C);
			if (F<uint8_t>(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(F<int32_t>(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			const uint32_t bucket = Bucket(h, ot, shift);
			uint8_t *poly = pk;
			uint8_t *mode1 = pk + 0x14;
			uint8_t *mode2 = pk + 0x1C;
			pk += 0x24;
			ModePrim(mode1, 0xE1000220);
			InsertPrimAutoDepth(bucket, mode1);
			InsertPrimAutoDepth(bucket, poly);
			ModePrim(mode2, 0xE1000240);
			InsertPrimAutoDepth(bucket, mode2);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// 70's 0x6DEDD0: flat quads (0xC-byte records: code + rgb, 4 vertex indices) -> POLY_F4 packets between two
	// draw-mode primitives; flags 1 / 4, 0x10, 0x40 as the triangles
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
			Code(h, pk, *(const uint32_t *)rec, 1, 4);
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
			const uint32_t bucket = Bucket(h, ot, shift);
			uint8_t *poly = pk;
			uint8_t *mode1 = pk + 0x18;
			uint8_t *mode2 = pk + 0x20;
			pk += 0x28;
			ModePrim(mode1, 0xE1000220);
			InsertPrimAutoDepth(bucket, mode1);
			InsertPrimAutoDepth(bucket, poly);
			ModePrim(mode2, 0xE1000240);
			InsertPrimAutoDepth(bucket, mode2);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// 0x691F30: textured flat triangles (0x14-byte records: code + rgb, 3 vertex indices, uv2, uv0 + clut, uv1 +
	// tpage) -> POLY_FT3 packets after a draw-mode primitive; flags 1 / 4, 0x10, 0x40
	// ------------------------------------------------------------------
	static uint32_t __cdecl TexTris(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
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
		for (int32_t k = count; k != 0; k--, rec += 0x14)
		{
			GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			GteRTPT();
			F<uint32_t>(pk, 0) = 0x7000000;
			Code(h, pk, *(const uint32_t *)rec, 1, 4);
			F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
			F<uint32_t>(pk, 0x14) = *(const uint32_t *)(rec + 0x10);
			F<uint32_t>(pk, 0x1C) = *(const uint32_t *)(rec + 8) >> 16;
			GteReadFLAG(h + 0x30);
			if (F<uint32_t>(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0(h + 0x24);
			if (F<int32_t>(h, 0x24) < 0 && !(F<uint8_t>(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x10)) clip |= 2;
			if (OutX(pk + 0x18)) clip |= 4;
			if (OutY(pk + 8)) clip |= 0x10;
			if (OutY(pk + 0x10)) clip |= 0x20;
			if (OutY(pk + 0x18)) clip |= 0x40;
			if ((clip & 7) == 7) continue;
			if ((clip & 0x70) == 0x70) continue;
			GteReadOTZWord(h + 0x2C);
			if (F<uint8_t>(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(F<int32_t>(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			const uint32_t bucket = Bucket(h, ot, shift);
			uint8_t *poly = pk;
			uint8_t *mode = pk + 0x20;
			pk += 0x28;
			ModePrim(mode, 0xE1000220);
			InsertPrimAutoDepth(bucket, mode);
			InsertPrimAutoDepth(bucket, poly);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// 0x6921C0: textured flat quads (0x18-byte records: code + rgb, 4 vertex indices, uv0 + clut, uv1 + tpage,
	// uv2 | uv3 << 16) -> POLY_FT4 packets after a draw-mode primitive; flags 1 / 4, 0x10, 0x40
	// ------------------------------------------------------------------
	static uint32_t __cdecl TexQuads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
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
		for (int32_t k = count; k != 0; k--, rec += 0x18)
		{
			GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			GteRTPT();
			F<uint32_t>(pk, 0) = 0x9000000;
			Code(h, pk, *(const uint32_t *)rec, 1, 4);
			F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
			F<uint32_t>(pk, 0x14) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			F<uint32_t>(pk, 0x1C) = uv;
			F<uint32_t>(pk, 0x24) = uv >> 16;
			GteReadFLAG(h + 0x30);
			if (F<uint32_t>(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0(h + 0x24);
			if (F<int32_t>(h, 0x24) < 0 && !(F<uint8_t>(h, 0x1C) & 0x10)) continue;
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
			if (F<uint8_t>(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(F<int32_t>(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			const uint32_t bucket = Bucket(h, ot, shift);
			uint8_t *poly = pk;
			uint8_t *mode = pk + 0x28;
			pk += 0x30;
			ModePrim(mode, 0xE1000220);
			InsertPrimAutoDepth(bucket, mode);
			InsertPrimAutoDepth(bucket, poly);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// 0x6924F0: gouraud triangles (0x14-byte records: code + rgb0, 3 vertex indices, rgb1, rgb2) -> POLY_G3 packets
	// between two draw-mode primitives; flags 2 / 8 semi-transparency, 0x20 double-sided, 0x80 depth cue. The cursor
	// advances 0x48 bytes per triangle (0x24 unused).
	// ------------------------------------------------------------------
	static uint32_t __cdecl GouraudTris(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
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
		for (int32_t k = count; k != 0; k--, rec += 0x14)
		{
			GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			GteRTPT();
			F<uint32_t>(pk, 0) = 0x6000000;
			Code(h, pk, *(const uint32_t *)rec, 2, 8);
			GteReadFLAG(h + 0x30);
			if (F<uint32_t>(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0(h + 0x24);
			if (F<int32_t>(h, 0x24) < 0 && !(F<uint8_t>(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x10)) clip |= 2;
			if (OutX(pk + 0x18)) clip |= 4;
			if (OutY(pk + 8)) clip |= 0x10;
			if (OutY(pk + 0x10)) clip |= 0x20;
			if (OutY(pk + 0x18)) clip |= 0x40;
			if ((clip & 7) == 7) continue;
			if ((clip & 0x70) == 0x70) continue;
			GteReadOTZWord(h + 0x2C);
			if (F<uint8_t>(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0xC, rec + 0x10, pk + 4);
				GteSetIR0(F<int32_t>(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0xC, pk + 0x14, pk + 4);
			}
			else
			{
				F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
				F<uint32_t>(pk, 0x14) = *(const uint32_t *)(rec + 0x10);
			}
			const uint32_t bucket = Bucket(h, ot, shift);
			uint8_t *poly = pk;
			uint8_t *mode1 = pk + 0x1C;
			uint8_t *mode2 = pk + 0x24;
			pk += 0x48;
			ModePrim(mode1, 0xE1000220);
			InsertPrimAutoDepth(bucket, mode1);
			InsertPrimAutoDepth(bucket, poly);
			ModePrim(mode2, 0xE1000240);
			InsertPrimAutoDepth(bucket, mode2);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// 70's 0x6DF140: gouraud quads (0x18-byte records: code + rgb0, 4 vertex indices, rgb1..3) -> POLY_G4 packets
	// between two draw-mode primitives; flags 2 / 8, 0x20, 0x80. The cursor advances 0x58 bytes per quad (0x24 unused).
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
			Code(h, pk, *(const uint32_t *)rec, 2, 8);
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
			const uint32_t bucket = Bucket(h, ot, shift);
			uint8_t *poly = pk;
			uint8_t *mode1 = pk + 0x24;
			uint8_t *mode2 = pk + 0x2C;
			pk += 0x58;
			ModePrim(mode1, 0xE1000220);
			InsertPrimAutoDepth(bucket, mode1);
			InsertPrimAutoDepth(bucket, poly);
			ModePrim(mode2, 0xE1000240);
			InsertPrimAutoDepth(bucket, mode2);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// 0x692830: gouraud textured triangles (0x1C-byte records: code + rgb0, 3 vertex indices, uv2, uv0 + clut,
	// uv1 + tpage, rgb1, rgb2) -> POLY_GT3 packets after a draw-mode primitive; flags 2 / 8, 0x20, 0x80
	// ------------------------------------------------------------------
	static uint32_t __cdecl GouraudTexTris(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
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
		for (int32_t k = count; k != 0; k--, rec += 0x1C)
		{
			GteLoadV012(verts + (uint32_t)*(const uint16_t *)(rec + 4) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 6) * 4, verts + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			GteRTPT();
			F<uint32_t>(pk, 0) = 0x9000000;
			Code(h, pk, *(const uint32_t *)rec, 2, 8);
			F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
			F<uint32_t>(pk, 0x18) = *(const uint32_t *)(rec + 0x10);
			F<uint32_t>(pk, 0x24) = *(const uint32_t *)(rec + 8) >> 16;
			GteReadFLAG(h + 0x30);
			if (F<uint32_t>(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0(h + 0x24);
			if (F<int32_t>(h, 0x24) < 0 && !(F<uint8_t>(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			GteAVSZ3();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x14)) clip |= 2;
			if (OutX(pk + 0x20)) clip |= 4;
			if (OutY(pk + 8)) clip |= 0x10;
			if (OutY(pk + 0x14)) clip |= 0x20;
			if (OutY(pk + 0x20)) clip |= 0x40;
			if ((clip & 7) == 7) continue;
			if ((clip & 0x70) == 0x70) continue;
			GteReadOTZWord(h + 0x2C);
			if (F<uint8_t>(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0x14, rec + 0x18, pk + 4);
				GteSetIR0(F<int32_t>(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 4);
			}
			else
			{
				F<uint32_t>(pk, 0x10) = *(const uint32_t *)(rec + 0x14);
				F<uint32_t>(pk, 0x1C) = *(const uint32_t *)(rec + 0x18);
			}
			const uint32_t bucket = Bucket(h, ot, shift);
			uint8_t *poly = pk;
			uint8_t *mode = pk + 0x28;
			pk += 0x30;
			ModePrim(mode, 0xE1000220);
			InsertPrimAutoDepth(bucket, mode);
			InsertPrimAutoDepth(bucket, poly);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// 70's 0x6DF560: gouraud textured quads (0x24-byte records: code + rgb0, 4 vertex indices, uv0 + clut, uv1 + tpage,
	// uv2 | uv3 << 16, rgb1..3) -> POLY_GT4 packets after a draw-mode primitive; flags 2 / 8, 0x20, 0x80
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
			Code(h, pk, *(const uint32_t *)rec, 2, 8);
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
			const uint32_t bucket = Bucket(h, ot, shift);
			uint8_t *poly = pk;
			uint8_t *mode = pk + 0x34;
			pk = mode + 8;
			ModePrim(mode, 0xE1000220);
			InsertPrimAutoDepth(bucket, mode);
			InsertPrimAutoDepth(bucket, poly);
		}
		F<const uint8_t *>(h, 0x20) = rec;
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// 0x691B40: the module's prim-model renderer (the engine's 0x7043B0 with the eight primitive lists of its
	// own): vertex base = model + 8 unless header flag 0x2000, far colour from the header colour bytes, then the lists
	// (an empty list is skipped)
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
				case 0: c = FlatTris(h, ot, shift, c); break;
				case 1: c = FlatQuads(h, ot, shift, c); break;
				case 2: c = TexTris(h, ot, shift, c); break;
				case 3: c = TexQuads(h, ot, shift, c); break;
				case 4: c = GouraudTris(h, ot, shift, c); break;
				case 5: c = GouraudQuads(h, ot, shift, c); break;
				case 6: c = GouraudTexTris(h, ot, shift, c); break;
				default: c = GouraudTexQuads(h, ot, shift, c); break;
			}
		}
		return c;
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x691760): one object of a layout
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
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
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
	// 0x690B40: the seven entities' flag words
	static void SaveEntityFlags()
	{
		for (int k = 0; k < 7; k++) SavedFlags()[k] = *(const uint16_t *)Entity(k);
	}
	// 0x692B40: bit 2 of the shown entities' flag words back to the saved value
	static void RestoreEntityFlags()
	{
		for (int k = 0; k < 7; k++)
		{
			uint8_t *e = Entity(k);
			const uint16_t w = *(const uint16_t *)e;
			if (w & 2) *(uint16_t *)e = (uint16_t)((((uint32_t)(uint8_t)SavedFlags()[k] ^ w) & 4) ^ w);
		}
	}
	// 70's 0x6DF940 (MAG_070_ShockwavePulsar_EntitiesFlag4On): flag 4 on the shown entities (flag 2)
	static void EntitiesFlag4On()
	{
		for (int k = 0; k < 7; k++)
		{
			uint8_t *e = Entity(k);
			const uint16_t w = *(const uint16_t *)e;
			if (w & 2) *(uint16_t *)e = (uint16_t)(w | 4);
		}
	}
	// 0x690B70 / 0x690BA0: the screen-flash entries 0x1D98990 + k * 0x2C (k 0..2 / 3) in use (+1 bit 0)
	// get the level (+2) and the word +0x28
	static void FlashEntries012(int32_t level, uint32_t v)
	{
		for (uint32_t e = 0x1D989B8; e < 0x1D98A3C; e += 0x2C)
		{
			if (*(const uint8_t *)(e - 0x27) & 1)
			{
				*(uint32_t *)e = v;
				*(int16_t *)(e - 0x26) = (int16_t)level;
			}
		}
	}
	static void FlashEntry3(int32_t level, uint32_t v)
	{
		if (*(const uint8_t *)0x1D98A15 & 1)
		{
			*(uint32_t *)0x1D98A3C = v;
			*(int16_t *)0x1D98A16 = (int16_t)level;
		}
	}
	// 0x690BC0 / 0x690BF0: empty pools
	static void ClearSparks()
	{
		uint8_t *p = (uint8_t *)TexBase() + SPARKS + 0xF;
		for (int k = 0x80; k != 0; k--, p += 0x10) *p = 0;
		SparkCursor() = (uint8_t *)TexBase() + SPARKS;
	}
	static void ClearShards()
	{
		uint8_t *p = (uint8_t *)TexBase() + SHARDS;
		for (int k = 0x40; k != 0; k--, p += 0x10) *(int16_t *)p = 0;
		ShardCursor() = (uint8_t *)TexBase() + SHARDS;
	}
	// 0x692D60: a free spark (life 0) from the cursor on, else from the start
	static Spark *SparkAlloc()
	{
		uint8_t *p = SparkCursor();
		const uint32_t tex = TexBase();
		if (p[0xF] != 0)
		{
			p = (uint8_t *)tex + SPARKS;
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
		if ((uint32_t)p >= tex + SPARKS_LAST) SparkCursor() = (uint8_t *)tex + SPARKS;
		else SparkCursor() = p + 0x10;
		return (Spark *)p;
	}
	// 0x692F70: a free shard (life 0)
	static Shard *ShardAlloc()
	{
		uint8_t *p = ShardCursor();
		const uint32_t tex = TexBase();
		if (*(const int16_t *)p != 0)
		{
			p = (uint8_t *)tex + SHARDS;
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
		if ((uint32_t)p >= tex + SHARDS_LAST) ShardCursor() = (uint8_t *)tex + SHARDS;
		else ShardCursor() = p + 0x10;
		return (Shard *)p;
	}
	// 0x692DC0: sparks: life--, position += 4 x velocity
	static void SparksUpdate()
	{
		Spark *s = (Spark *)(TexBase() + SPARKS);
		for (int k = 0x80; k != 0; k--, s++)
		{
			if (s->life == 0) continue;
			s->life--;
			s->pos[0] = (int16_t)(s->pos[0] + (int16_t)((int16_t)s->vel[0] << 2));
			s->pos[1] = (int16_t)(s->pos[1] + (int16_t)((int16_t)s->vel[1] << 2));
			s->pos[2] = (int16_t)(s->pos[2] + (int16_t)((int16_t)s->vel[2] << 2));
		}
	}
	// 0x692FD0: shards: life--, scales += steps
	static void ShardsUpdate()
	{
		Shard *s = (Shard *)(TexBase() + SHARDS);
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
	// 0x692B80: a full-screen tile faded in over `in` ticks, held `hold`, out over `out` (the engine's
	// 0x700150 on the module's packet cursor)
	// ------------------------------------------------------------------
	static int32_t FadeLevel(int32_t t, int32_t in, int32_t hold, int32_t out)
	{
		if (t < in) return shl32(t + 1, 12) / in;
		if (t < in + hold) return 0x1000;
		if (out == 0) out = 1;
		return (shl32(in - t + hold, 12) - 0x1000) / out + 0x1000;
	}
	static void FadeTile(int32_t t, int32_t r, int32_t g, int32_t b, int32_t in, int32_t hold, int32_t out)
	{
		PacketCursor() = Tile(r, g, b, FadeLevel(t, in, hold, out), PacketCursor());
	}

	// ------------------------------------------------------------------
	// 0x692C40: sparks, one flipbook frame each (frame = sequence counted down by life)
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
			PacketCursor() = Flipbook(h, OT() + 0x44, 0, PacketCursor());
		}
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// MAG_260_ShardMatrix (0x692F00): rotation about y by -angle, y scaled by sy, x / z rows scaled by sxz
	// ------------------------------------------------------------------
	static void ShardMatrix(int32_t angle, int32_t sy, int32_t sxz, Mat4x3 *out)
	{
		uint32_t *w = (uint32_t *)out;
		w[0] = w[1] = w[2] = w[3] = w[4] = 0;
		const int32_t a = -(int32_t)(int16_t)angle;
		const int32_t s = ComputeSin(a);
		const int32_t c = ComputeCos(a);
		const int32_t z = (int16_t)sxz;
		out->m[0][0] = (int16_t)c;
		out->m[2][0] = (int16_t)(mul32((int16_t)s, z) >> 12);
		out->m[0][2] = (int16_t)-s;
		out->m[1][1] = (int16_t)sy;
		out->m[2][2] = (int16_t)(mul32((int16_t)c, z) >> 12);
	}

	// ------------------------------------------------------------------
	// 0x692E10: shards, the model 0x111D30C at their target's position (+ the middle of its model + the
	// shard's offset), squashed by the shard's scales (module renderer, depth -0x400, flags 0x30)
	// ------------------------------------------------------------------
	static void ShardsDraw(const Shard *s)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x78);
		F<uint32_t>(h, 0x38) = 0xFFFFFC00;
		F<uint32_t>(h, 0x3C) = 0x30;
		for (int n = 0x40; n != 0; n--, s++)
		{
			if (s->life == 0) continue;
			ShardMatrix(0x800, s->sy, s->sxz, (Mat4x3 *)h);
			uint8_t *e = s->rec->entity;
			F<int32_t>(h, 0x14) = E16(e, 0x1C);
			F<int32_t>(h, 0x18) = sdiv_pow2((int32_t)s->rec->y_min + s->rec->y_max, 1) + s->off + E16(e, 0x1E);
			F<int32_t>(h, 0x1C) = E16(e, 0x20);
			ComposeAffineTransform(&Camera(), (Mat4x3 *)h, (Mat4x3 *)h);
			GteSetRotMatrix((Mat4x3 *)h);
			GteSetTransVector((Mat4x3 *)h);
			F<uint32_t>(h, 0x20) = SHARD_MODEL;
			PacketCursor() = RenderModel(h + 0x20, OT() + 0x44, 2, PacketCursor());
		}
		FieldFree(0x78);
	}

	// ------------------------------------------------------------------
	// 0x693000: a model's vertical extent in its bone space (every shown object's vertices through their
	// bones, IR2 min / max) -> out[0] = max, out[1] = min; returns min. The entity's matrix translation y is 0 for
	// the bone build, then its height (+0x1E).
	// ------------------------------------------------------------------
	static int32_t MeasureHeight(uint8_t *e, int16_t *out)
	{
		F<int32_t>(e, 0x58) = 0;
		ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
		F<int32_t>(e, 0x58) = E16(e, 0x1E);
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

	// ------------------------------------------------------------------
	// MAG_260_DrawEntityModelTransformed (0x693170): an entity's model (and weapon) redrawn through its own bones, turned
	// by `angles` and scaled by `scale` about the point `offset` of its world matrix (+0x40), through RenderGeometry into
	// the frame arena
	// ------------------------------------------------------------------
	static void DrawEntityModel(uint8_t *e, const int16_t *angles, const int16_t *offset, int32_t scale)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x5C);
		Mat4x3 *m = (Mat4x3 *)h;
		ComposeZYXRotationMatrix(angles, m);
		const int32_t s = (int16_t)scale;
		F<int32_t>(h, 0x28) = s;
		F<int32_t>(h, 0x24) = s;
		F<int32_t>(h, 0x20) = s;
		Scale3DMatrix(m, (const int32_t *)(h + 0x20));
		F<int32_t>(h, 0x14) = offset[0];
		F<int32_t>(h, 0x18) = offset[1];
		F<int32_t>(h, 0x1C) = offset[2];
		ComposeAffineTransform((const Mat4x3 *)(e + 0x40), m, m);
		ComposeAffineTransform(&Camera(), m, m);
		Apply3D(m, offset, (int32_t *)(h + 0x20));
		F<int32_t>(h, 0x18) = (int32_t)((uint32_t)F<int32_t>(h, 0x18) - (uint32_t)F<int32_t>(h, 0x24));
		F<int32_t>(h, 0x14) = (int32_t)((uint32_t)F<int32_t>(h, 0x14) - (uint32_t)F<int32_t>(h, 0x20));
		F<int32_t>(h, 0x1C) = (int32_t)((uint32_t)F<int32_t>(h, 0x1C) - (uint32_t)F<int32_t>(h, 0x28));
		ComputeBonesWorldMatrices(e + 0x60, m);
		F<uint32_t>(h, 0x34) = var<uint32_t>(0x1D98B3C);
		F<uint16_t>(h, 0x48) = 0x140;
		F<uint16_t>(h, 0x44) = 0;
		F<uint16_t>(h, 0x46) = 0;
		F<uint16_t>(h, 0x4A) = 0xD8;
		F<uint32_t>(h, 0x4C) = F<uint32_t>(e, 0x28);
		F<uint32_t>(h, 0x58) = 0;
		F<uint32_t>(h, 0x50) = F<uint32_t>(e, 0x7C);
		F<uint32_t>(h, 0x40) = var<uint32_t>(0x1D969A8);
		FrameArena() = RenderGeometry(F<void *>(e, 0x64), h + 0x30, OT() + 0x44, 4, FrameArena());
		BuildBoneMatricesFromPose(e + 0x60);
		uint8_t *w = F<uint8_t *>(e, 0x78);
		if (w)
		{
			F<uint32_t>(h, 0x50) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(w, m);
			FrameArena() = RenderGeometry(F<void *>(w, 4), h + 0x30, OT() + 0x44, 4, FrameArena());
			BuildBoneMatricesFromPose(w);
		}
		FieldFree(0x5C);
	}

	// a target's model redrawn (by the master: rise = nullptr, or by its Rise task): the whole linked ring (entities
	// shown, flag 2) when the ring holds no other target, else the target alone; then flag 4 on what was drawn
	static void DrawTarget(Target *rec, const int16_t *vec, const SubNode *rise)
	{
		uint8_t *e0 = rec->entity;
		if (rec->group != 0)
		{
			uint8_t *e = e0;
			do
			{
				if (*(const uint16_t *)e & 2)
				{
					// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
					FX_HELD(held_note_model(rise, rec, e, vec);)
					DrawEntityModel(e, rec->ang, vec, rec->scale);
				}
				e = NextLinked(e);
			} while (e != e0 && e);
			e = e0;
			do
			{
				const uint16_t w = *(const uint16_t *)e;
				if (w & 2) *(uint16_t *)e = (uint16_t)(w | 4);
				e = NextLinked(e);
			} while (e != e0 && e);
		}
		else
		{
			// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
			FX_HELD(held_note_model(rise, rec, e0, vec);)
			DrawEntityModel(e0, rec->ang, vec, rec->scale);
			*e0 |= 4;
		}
	}

	// the layout parameter block of the Lift / Rise tasks: on the target, at the middle of its model
	static void TargetArg(PrimArg &arg, const Target *rec, uint8_t *e)
	{
		arg.pos[0] = E16(e, 0x1C);
		arg.pos[2] = E16(e, 0x20);
		arg.pos[1] = (int16_t)((int16_t)sdiv_pow2((int32_t)rec->y_max + rec->y_min, 1) + E16(e, 0x1E));
		arg.morph = TexBase() + MORPH;
		arg.objs = OBJS_A;
		arg.depth = (int16_t)0xFC00;
	}

	// ------------------------------------------------------------------
	// LiftTask (0x6932C0): node +0x0C target, +0x10 counter, +0x14 layout 0x111F09C
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
		// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
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
					p->scale = (int16_t)((CrtRand() & 0x800) + 0x1770);
					p->vel[0] = (int8_t)(mul32(ComputeSin(ang), speed) >> 11);
					p->vel[1] = 0;
					p->vel[2] = (int8_t)(mul32(ComputeCos(ang), speed) >> 11);
					p->seq = (const uint8_t *)BURST_SEQ;
					p->life = 0x10;
					ang -= 0x100;
					if (ang == 0) break;
					p = SparkAlloc();
				} while (p);
			}
		}
		const int32_t rest = 0x1000 - q;
		const int16_t d = (int16_t)sdiv_pow2(rest, 3);
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
		const int32_t h = sdiv_pow2(shl32(mul32(rest, rest) >> 12, 12), 12);
		E16(e, 0x1E) = (int16_t)(h - sdiv_pow2((int32_t)rec->y_min + rec->y_max, 1));
		s->t.counter++;
		if (s->t.counter >= 0x1E)
		{
			*(uint16_t *)e &= 0xFFF7;
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// RingTask (0x693720): node +0x0C position, +0x14 layout 0x111D424; ends with the layout
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		SubNode *s = (SubNode *)n;
		PrimArg arg;
		arg.objs = OBJS_A;
		arg.depth = (int16_t)0xFF80;
		arg.pos[0] = s->pos[0];
		arg.pos[1] = s->pos[1];
		arg.pos[2] = s->pos[2];
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
		FX_HELD(held_note_play(K_RING, s, &arg, 0);)
		const int left = prim::play((prim::Layout *)s->layout, PrimPart, (int)&arg, 0);
		return left != 0 ? 0 : TASK_END;
	}

	// the Rise task's scale at counter c (from 0x1000 to the scale target over 0xD ticks)
	static int32_t RiseScale(const Target *rec, int32_t c)
	{
		if (c < 0xD)
		{
			const int32_t v = mul32(0x1000 - rec->scale_to, c);
			int32_t q = mulhi_s(v, 0x4EC4EC4F) >> 2;
			q += (int32_t)((uint32_t)q >> 31);
			return 0x1000 - q;
		}
		return rec->scale_to;
	}
	// the Rise task's height at lift step t (0..0x1000), from the target's starting height (+6 of its record)
	static int16_t RiseHeight(const Target *rec, int32_t t)
	{
		const int32_t tt = mul32(t, t) >> 12;
		const int32_t b = sdiv_pow2(mul32((int32_t)rec->y_max + rec->y_min, tt), 13);
		int32_t a = shl32(-tt, 14);
		a = sdiv_pow2(a, 1);
		a = sdiv_pow2(a, 12);
		return (int16_t)(a - b + *(const int16_t *)((const uint8_t *)rec + 6));
	}

	// ------------------------------------------------------------------
	// RiseTask (0x6934B0): node +0x0C target, +0x10 counter, +0x12 lift length, +0x14 layout 0x111DEBC. Never ends by
	// itself (the master resets the pool).
	// ------------------------------------------------------------------
	static uint32_t __cdecl RiseTask(TaskNode *n)
	{
		SubNode *s = (SubNode *)n;
		Target *rec = s->t.rec;
		uint8_t *e = rec->entity;
		rec->scale = RiseScale(rec, s->t.counter);
		PrimArg arg;
		TargetArg(arg, rec, e);
		// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
		FX_HELD(held_note_play(K_RISE, s, &arg, s->t.counter);)
		prim::play((prim::Layout *)s->layout, PrimPart, (int)&arg, 0);
		const uint32_t i = (uint32_t)((int32_t)s->t.counter - 0xD);
		const uint32_t len = (uint32_t)(int32_t)s->t.len;
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
			E16(e, 0x1E) = RiseHeight(rec, t);
		}
		else if (i < 0xC8)
		{
			if (i == len) E16(e, 0x1E) = (int16_t)(0xFFFFD000u - (uint32_t)sdiv_pow2((int32_t)rec->y_max + rec->y_min, 1));
			E16(e, 0x1E) = (int16_t)(E16(e, 0x1E) + (int16_t)0xFF00);
		}
		int16_t vec[3];
		vec[2] = 0;
		vec[0] = 0;
		vec[1] = (int16_t)(((int32_t)rec->y_min + rec->y_max) >> 1);
		DrawTarget(rec, vec, s);
		s->t.counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// the master's battle camera at counter c (eye 0xB8B7F0, look-at 0xB8B7F8; field of view, roll, the camera return)
	// ------------------------------------------------------------------
	static void MasterCamera(int32_t c, bool engine)
	{
		const int16_t X = (int16_t)CentreX(), Z = (int16_t)CentreZ();
		if (c == 0)
		{
			if (engine) CameraArmReturn();
			ViewWord() = 0x120;
			CameraZoom() = 0;
			CamLookAt()[1] = (int16_t)0xF777;
			CamEye()[1] = (int16_t)0xEDBE;
			CamLookAt()[0] = (int16_t)(X - 0x13E);
			CamLookAt()[2] = (int16_t)(Z + 0x139);
			CamEye()[0] = (int16_t)(X + 0x99F);
			CamEye()[2] = (int16_t)(Z - 0x1641);
		}
		if ((uint32_t)(c - 0x14) < 0x27)
		{
			const uint32_t a = (uint32_t)(c - 0x14) << 12;
			int16_t k0[6], k1[6];
			k1[0] = (int16_t)(X - 0x13E);
			k1[1] = (int16_t)0xF777;
			k1[2] = (int16_t)(Z + 0x139);
			k1[3] = (int16_t)(X + 0x99F);
			k1[4] = (int16_t)0xEDBE;
			k1[5] = (int16_t)(Z - 0x1641);
			k0[0] = (int16_t)(X - 0x66);
			k0[1] = (int16_t)0xDF3F;
			k0[2] = (int16_t)(Z - 0x986);
			k0[3] = (int16_t)(X + 0x1A2);
			k0[4] = 0x6CD;
			k0[5] = (int16_t)(Z - 0x1F40);
			const uint32_t hi = mulhi_u(a, 0xAF286BCB);
			const uint32_t angle = ((((a - hi) >> 1) + hi) >> 5) >> 2;
			CamLerp(k1, k0, 0x1000 - ComputeCos((int32_t)angle));
		}
		if (c == 0x3B)
		{
			CamLookAt()[1] = (int16_t)0xBFC3;
			CamLookAt()[0] = (int16_t)(X - 0x16D);
			CamLookAt()[2] = (int16_t)(Z - 0x21A);
			CamEye()[0] = (int16_t)(X - 0x169);
			CamEye()[1] = (int16_t)0xCAB7;
			CamEye()[2] = (int16_t)(Z - 0x2070);
		}
		if ((uint32_t)(c - 0x4F) < 0x28)
		{
			const uint32_t a = (uint32_t)(c - 0x4F) << 12;
			int16_t k0[6], k1[6];
			ViewWord() = 0xC8;
			k0[0] = (int16_t)(X - 0x1D);
			k0[1] = 0xBB7;
			k0[2] = (int16_t)(Z + 0x123);
			k0[3] = (int16_t)(X - 0x18D5);
			k0[4] = (int16_t)0xE9DA;
			k0[5] = (int16_t)(Z - 0x1145);
			k1[0] = (int16_t)(X - 0xCC);
			k1[1] = (int16_t)0xFFF3;
			k1[2] = (int16_t)(Z + 0x8CF);
			k1[3] = (int16_t)(X + 0x1B);
			k1[4] = (int16_t)0xFBCF;
			k1[5] = (int16_t)(Z - 0x11D6);
			const uint32_t hi = mulhi_u(a, 0xA41A41A5);
			const uint32_t t = (((a - hi) >> 1) + hi) >> 5;
			CamLerp(k0, k1, (int32_t)t);
		}
		if ((uint32_t)(c - 0x77) < 0x3C)
		{
			const uint32_t a = (uint32_t)(c - 0x77) << 12;
			int16_t k0[6], k1[6];
			k0[0] = (int16_t)(X - 0xCC);
			k0[1] = (int16_t)0xFFF3;
			k0[2] = (int16_t)(Z + 0x8CF);
			k0[3] = (int16_t)(X + 0x1B);
			k0[4] = (int16_t)0xFBCF;
			k0[5] = (int16_t)(Z - 0x11D6);
			k1[0] = (int16_t)(X - 0xCD);
			k1[1] = (int16_t)0xFFF2;
			k1[2] = (int16_t)(Z + 0x8D0);
			k1[3] = (int16_t)(X + 0x22BE);
			k1[4] = (int16_t)0xFB3E;
			k1[5] = (int16_t)(Z + 0x940);
			const uint32_t t = mulhi_u(a, 0x22B63CBF) >> 3;
			CamLerp(k0, k1, (int32_t)t);
		}
		if (c == 0xD5)
		{
			CamEye()[0] = (int16_t)(X + 0x1167);
			CamLookAt()[0] = (int16_t)(X + 0x2BE);
			ViewWord() = 0x120;
			CamLookAt()[1] = 0x63;
			CamLookAt()[2] = (int16_t)(Z - 0x1CB);
			CamEye()[1] = (int16_t)0xE015;
			CamEye()[2] = (int16_t)(Z + 0x206D);
			if (engine)
			{
				StreamLoad(0x168, TexBase() + 0x2E50, 1);
				StreamLoad(0x169, TexBase() + 0x2E50, 0);
			}
		}
	}

	// ------------------------------------------------------------------
	// Master (0x690C20)
	// ------------------------------------------------------------------
	// flash A: the ramp in over 0x32 ticks, out from 0xFA; flash B: in over 0xC ticks, out from 0xFA
	static int32_t Ramp12(int32_t v)
	{
		int32_t q = mulhi_s(v * 2300, 0x2AAAAAAB) >> 1;
		return q + (int32_t)((uint32_t)q >> 31);
	}
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
		FX_HELD(held_note_master(m);)
		if (m->counter < 0x32)
		{
			const int32_t v = shl32(m->counter, 12);
			int32_t q = mulhi_s(v, 0x51EB851F) >> 4;
			FlashA() = q + (int32_t)((uint32_t)q >> 31);
		}
		else if (m->counter >= 0xFA) FlashA() = Ramp12(0x106 - (int32_t)m->counter);
		else FlashA() = 0x1000;
		if (m->counter < 0xC) FlashB() = Ramp12(m->counter);
		else if (m->counter >= 0xFA) FlashB() = Ramp12(0x106 - (int32_t)m->counter);
		else FlashB() = 0x8FC;

		// 0x32..0x4F: the centre layout
		if ((uint32_t)((int32_t)m->counter - 0x32) < 0x1E)
		{
			if (m->counter == 0x32) DecodeModelPrimLayout(LAYOUT_CENTRE, m->layout, 0x218);
			PrimArg arg;
			arg.pos[0] = (int16_t)CentreX();
			arg.pos[2] = (int16_t)CentreZ();
			arg.pos[1] = (int16_t)0xC000;
			arg.morph = TexBase() + MORPH;
			arg.objs = OBJS_A;
			arg.depth = (int16_t)0xFF80;
			// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
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
				// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
				FX_HELD(held_note_tile((int32_t)w);)
				FadeTile((int32_t)w, 0xFF, 0xFF, 0xFF, 7, 1, 0);
			}
			bool models = true;      // the targets redrawn, the layout behind them
			if (s == 0)
			{
				InitTaskQueuePool(QSub(), (void *)(TexBase() + 0x2160), 0x114, 0xC);
				EntitiesFlag4On();
				DecodeModelPrimLayout(LAYOUT_WAVE, m->layout, 0x1100);
				for (int32_t k = 0; k < m->ntargets; k++)
				{
					SubNode *t = (SubNode *)AddTaskToQueue(QSub(), ORIG_LiftTask);
					Target *rec = &m->targets[k];
					E16(rec->entity, 0x1E) = 0x1000;
					F<int32_t>(rec->entity, 0x58) = 0x1000;
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
						p->pos[2] = (int16_t)((uint32_t)(ComputeCos(a) >> 3) + CentreZ() + 0x800);
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
						int32_t j = 2;
						Shard *p = ShardAlloc();
						while (p)
						{
							p->life = 6;
							p->off = (int16_t)(sdiv_pow2(CrtRand(), 5) - 0x200);
							const int32_t r = CrtRand();
							p->rec = rec;
							const int16_t v = (int16_t)(sdiv_pow2(shl32(r, 11), 16) + 0x800);
							p->sxz = v;
							const int16_t a = (int16_t)(v >> 1);
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
							uint8_t *e0 = rec->entity;
							if (rec->group != 0)
							{
								uint8_t *e = e0;
								do
								{
									const uint16_t wd = *(const uint16_t *)e;
									if (wd & 2) *(uint16_t *)e = (uint16_t)(wd & 0xFFF3);
									e = NextLinked(e);
								} while (e != e0 && e);
							}
							else *(uint16_t *)e0 &= 0xFFF3;
							*(uint32_t *)(e0 + 0x1C) = rec->pos_xy;
							*(uint32_t *)(e0 + 0x20) = rec->pos_z;
						}
						RestoreEntityFlags();
					}
					if (FlashA() >= 0x8FC)
					{
						if (u < 0x20) FlashA() = 0x1000 - sdiv_pow2(u * 1796, 5);
						else FlashA() = 0x8FC;
					}
					if (FlashB() >= 0x8FC && u < 0x20) FlashB() = 0x1000 - sdiv_pow2(u * 1796, 5);
					models = false;
				}
			}
			int32_t zoff = 0;
			int16_t depth = (int16_t)0xF000;
			if (models)
			{
				// the targets' models redrawn rotated and scaled; both flashes full
				FlashB() = 0x1000;
				FlashA() = 0x1000;
				depth = 0x800;
				zoff = 0x800;
				int16_t vec[3];
				vec[2] = 0;
				vec[0] = 0;
				for (int32_t k = 0; k < m->ntargets; k++)
				{
					Target *rec = &m->targets[k];
					vec[1] = (int16_t)(((int32_t)rec->y_min + rec->y_max) >> 1);
					DrawTarget(rec, vec, nullptr);
				}
			}
			PrimArg arg;
			arg.pos[2] = (int16_t)(zoff + (int32_t)CentreZ());
			arg.pos[0] = (int16_t)CentreX();
			arg.morph = TexBase() + MORPH;
			arg.pos[1] = 0;
			arg.objs = OBJS_WAVE;
			arg.depth = depth;
			// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
			FX_HELD(held_note_play(K_WAVE, m, &arg, m->counter);)
			prim::play((prim::Layout *)m->layout, PrimPart, (int)&arg, 0);
		}

		// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
		FX_HELD(held_note_sparks();)
		SparksDraw((const Spark *)(TexBase() + SPARKS));
		if (m->counter < 0xD5)
		{
			// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
			FX_HELD(held_note_shards();)
			ShardsDraw((const Shard *)(TexBase() + SHARDS));
		}
		SparksUpdate();
		ShardsUpdate();

		if (m->counter == 0) BdPlaySE(SOUND_0, 0, 0x80);
		else if (m->counter == 0x50) BdPlaySE(SOUND_50, 0, 0x80);
		else if (m->counter == 0xAA) BdPlaySE(SOUND_AA, 0, 0x80);

		// 1, 9, 0x11, 0x19: a RiseTask on target (counter - 1) / 8
		{
			const uint32_t c1 = (uint32_t)((int32_t)m->counter - 1);
			if (c1 < 0x20 && (c1 & 7) == 0 && (int32_t)(c1 >> 3) < m->ntargets)
			{
				Target *rec = &m->targets[c1 >> 3];
				SubNode *t = (SubNode *)AddTaskToQueue(QSub(), ORIG_RiseTask);
				MeasureHeight(rec->entity, &rec->y_max);
				*rec->entity |= 8;
				t->t.counter = 0;
				t->t.len = (int16_t)(0x32 - (int32_t)c1);
				DecodeModelPrimLayout(LAYOUT_RISE, t->layout, 0x100);
				t->t.rec = rec;
			}
		}

		MasterCamera(m->counter, true);
		StreamPump();
		if (m->counter == 0xFC)
		{
			const ActionData *act = ((const CastContext *)Ctx())->actions;
			ApplyActionResultToTargets(act->targets, act->target_count);
		}
		if (m->counter >= 0x106)
		{
			RestoreEntityFlags();
			ViewWord() = var<int16_t>(0x1D977A0);
			var<uint32_t>(0xB8B7F8) = var<uint32_t>(0xB8B808);
			FlashB() = 0;
			FlashA() = 0;
			var<uint32_t>(0xB8B7FC) = var<uint32_t>(0xB8B80C);
			var<uint32_t>(0xB8B7F0) = var<uint32_t>(0xB8B800);
			var<uint32_t>(0xB8B7F4) = var<uint32_t>(0xB8B804);
			QSub()->tail = nullptr;
			QSub()->head = nullptr;
			return StreamPending(1) != 0 ? TASK_END : 0;
		}
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x690730)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		if (r->parity)
		{
			PacketCursor() = TexBase() + ARENA_A;
			ArenaEnd() = TexBase() + ARENA_B;
			r->parity = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			ArenaEnd() = TexBase() + ARENA_C;
			r->parity = 1;
		}
		FlashA() = 0;
		FlashB() = 0;
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			SaveEntityFlags();
			ClearSparks();
			ClearShards();
			StreamStateInit((void *)TexBase());
			InitTaskQueuePool(QMaster(), (void *)(TexBase() + 0x48), 0x1298, 1);
			InitTaskQueuePool(QSub(), (void *)(TexBase() + 0x2160), 0x114, 0xC);
			MasterNode *m = (MasterNode *)AddTaskToQueue(QMaster(), ORIG_MasterTask);
			Memset32(&m->counter, 0, 0x4A3);
			BindEntityModelFromComData(Caster(), m->bind, COM_Anim);
			QueueChainTransformation(Caster(), 1);
			int16_t minx = 0x7FFF, miny = 0x7FFF, minz = 0x7FFF;
			int16_t maxx = (int16_t)0x8001, maxy = (int16_t)0x8001, maxz = (int16_t)0x8001;
			m->ntargets = ((const CastContext *)Ctx())->actions->target_count;
			if (var<uint16_t>(0x1CFF6E0) == 0x31C) m->ntargets = 1; // (encounter 0x31C: the first target only)
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
				rec->scale_to = 0x800000 / ((int32_t)E16(e, 0x26) + 0x200);
			}
			// a target whose linked ring (+0x8C) holds no other shown target is redrawn with its whole ring
			for (int32_t i = 0; i < m->ntargets; i++)
			{
				Target *rec = &m->targets[i];
				uint8_t *e = rec->entity;
				rec->group = 0;
				bool other = false;
				for (uint8_t *p = NextLinked(e); p != e; p = NextLinked(p))
				{
					if (!p) break;
					if (*(const uint8_t *)p & 2)
					{
						for (int32_t j = 0; j < m->ntargets && !other; j++) other = m->targets[j].entity == p;
						if (other) break;
					}
				}
				if (!other) rec->group = 1;
			}
			int32_t v = maxx + minx;
			*(int16_t *)&CentreX() = (int16_t)sdiv_pow2(v, 1);
			v = maxy + miny;
			CentreY() = (int16_t)sdiv_pow2(v, 1);
			v = maxz + minz;
			*(int16_t *)&CentreZ() = (int16_t)sdiv_pow2(v, 1);
		}
		int a = (int)n;
		if (r->started)
		{
			EffectCameraMatrix(&Camera(), &EffCam());
			a = ExecuteTaskQueue(QMaster());
			ExecuteTaskQueue(QSub());
		}
		if (FlashA() != FlashAPrev())
		{
			FlashEntries012(FlashA(), 0);
			const int32_t f = FlashA();
			if (f >= 0x1000)
			{
				for (uint32_t b = 0x1D98991; b < 0x1D98A15; b += 0x2C) *(uint8_t *)b &= 0xFD;
			}
			else if (FlashAPrev() >= 0x1000)
			{
				for (uint32_t b = 0x1D98991; b < 0x1D98A15; b += 0x2C) *(uint8_t *)b |= 2;
			}
			FlashAPrev() = f;
		}
		if (FlashB() != FlashBPrev())
		{
			FlashEntry3(FlashB(), 0);
			const int32_t f = FlashB();
			if (f >= 0x1000) *(uint8_t *)0x1D98A15 &= 0xFD;
			else if (FlashBPrev() >= 0x1000) *(uint8_t *)0x1D98A15 |= 2;
			FlashBPrev() = f;
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

	void register_mag260_shockwave_pulsar_quistis()
	{
		register_port(pulsar260::ORIG_RootTask, (void *)pulsar260::RootTask, "260 RootTask", 260);
		register_port(pulsar260::ORIG_MasterTask, (void *)pulsar260::MasterTask, "260 MasterTask", 260);
		register_port(pulsar260::ORIG_LiftTask, (void *)pulsar260::LiftTask, "260 LiftTask", 260);
		register_port(pulsar260::ORIG_RiseTask, (void *)pulsar260::RiseTask, "260 RiseTask", 260);
		register_port(pulsar260::ORIG_RingTask, (void *)pulsar260::RingTask, "260 RingTask", 260);
		// 30 fps layer: see mag260_shockwave_pulsar_quistis_held.inc
		FX_HELD(register_mag260_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag260_shockwave_pulsar_quistis_held.inc"
#endif
