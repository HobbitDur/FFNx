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

// Effect 216: Different Beat (Zell's Duel finisher; MAG_216_*). The Duel (effect 274) hands over to it when the player enters
// the finisher's input; the battle casts it with Zell as the caster on one target.
//
// Structure (setup 0x6959E0 -> _Init 0x695A10 (not ported: the setup: TIM upload, magic buffer 0x250890C, context, caster,
// the target (the lowest entity of its chain, 0x501FF0), root queue), file loader 0x6959F0 (the module file 0x1154F28)):
//   RootTask (0x695AB0) - node 0x10 (pool 0x25088D8): every tick the packet cursors alternate between magic buffer + 0x3198 /
//     + 0x1D198 and + 0x1D198 / + 0x37198, the flash level is cleared, the view is orthonormalized into 0x2508910
//     (0x67AAE0); on counter 1 sets up the four queues (director 1 x 0x17F0 at the magic buffer, rings 8 x 0x104 at + 0x17F0,
//     rocks 0x40 x 0x24 at + 0x2010, sprites 0x20 x 0x1C at + 0x2910) and starts the director: Zell's position / angles saved,
//     the target's anchor (0x571400) with y 0, the scale (target size / 2 + 0x400), the stage captured into VRAM under the
//     target (0x69A790, the box of 0x114A6DC's model scaled), Zell's place before the target (the anchor's z + both half
//     sizes), the camera anchors 0x25088F8 / 0x2508900, his animation set (0x5022C0, com 0x1153B30) and chain 1, the stream
//     state, Zell shown, the target's model height (0x695F60) and its height word 0x2508908, the target's position saved and
//     raised by its model bottom, its chain flagged (bit 8), the afterimage block (bones 1 / 2 / 4 / 8..0x16); then runs the
//     queues and applies the flash level (0x5713E0, the flash words' bit 2 when it crosses 0x1000); when the director ends it
//     waits for the streams (0x5342C0(1)), puts the target / Zell back (positions, angles, the chain's bit 8 cleared), the
//     field of view back (0x1D977A0) and ends.
//   DirectorTask (0x6960E0, a function start the IDB lacks: it sits inside 0x695F60's range) - node 0x17F0, counters 0..0x8A
//     (the counter steps last; UPDATE AND DRAW in one call per tick): the flash level (rising 0..0xB, 0xA8C, falling 0x7F..0x8A;
//     0x1D..0x7C a second envelope); 9..0x16 a layout at Zell (0x1140C5C, Zell placed before the target at 9), 0x11..0x15 and
//     0x17..0x46 (four of every six ticks) a twisted model at his bone 0x14 (0x113C3CC through 0x69A670, the engine's prim
//     renderer 0x701DD0); 0x12, every sixth tick 0x17..0x46: a ring (0x69A550) and embers (0x69A110), the target thrown up
//     (0x12..0x16) and drawn by the module (0x68D1D0) from 0x17 while Zell rises after it; 0x47..0x66 the target swaying in the
//     air (sin / cos), 0x5D..0x66 a layout at Zell's bone 0x14 (0x11431D4), 0x67..0x78 two layouts there (+ 0x11484D0) while
//     both drop; 0x71..0x89 the impact: a white tile (0x72 / 0x73), rocks (0x69A2D0) and sparks (0x699F80) 0x71..0x74, the
//     target squashed (its y scale) and drawn by the module, a layout at Zell (0x1141444) until 0x7C, a layout at the anchor
//     (0x114A6DC, its object '9' through the module renderer 0x697D10), Zell landing behind (0x7E..0x83); 0x29..0x70 a
//     camera-facing layout over the target (0x11512C8); the afterimages of Zell 1..0xC, 0x18..0x46, 0x7C..0x82 (recorded 0..0xA,
//     0x17..0x23, 0x7B..0x80: 0x697520 / 0x697850); 0x8A the damage (0x506690) and the chain's bits 0x0C cleared; the sound at
//     0; the camera (0x697109..0x6974A4); 0x8B the end (the streams 0x168..0x16A, flash off).
//   Rings 0x69A5D0 (layout 0x115049C at a point, scaled about Y), rocks 0x69A3F0 / 0x69A530 (both 0x69A410: the model
//     0x1140AA4, DRAW THEN UPDATE), sprites 0x69A060 / 0x69A200 (flipbooks 0x113C1AC / 0x113C2F0, DRAW THEN UPDATE). The
//     prim-player callback 0x6978F0 draws one layout object with the block's kind table: '1' / '2' / '3' texture windows
//     scrolled, '9' through the module renderer 0x697D10 (eight primitive lists of its own: 0x697E40..0x699A00), others
//     through the engine's 0x701DD0.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Writes to battle entities: Zell (caster): position +0x1C..+0x22 (9: before the target; risen 0x17..0x46 (-0xA00 then -0xD0
//   a tick), 0x5D..0x78 moved / dropped, 0x71 back at the anchor, 0x7E..0x83 landing; put back by the root), facing +0x0E
//   (9: 0, 0x7F: the saved one), angles +0x0C..+0x13 (0x71 the saved ones), flags bit 4 cleared at the start, his animation
//   set / chain 1; the target (and its chain): +0x1E height (start: raised by its model bottom; 0x12..0x16, 0x17..0x46,
//   0x47..0x66 thrown / swaying, 0x71..0x89 squashed), +0x20 (0x47, 0x67..0x70), flags bit 8 (start .. 0x16, 0x17 / every
//   sixth tick, 0x5D..), bit 4 (drawn by the module 0x17..0x46, 0x47..0x66, 0x71..0x89; cleared 0x5D / 0x67), bits 0x0C
//   cleared at 0x8A, its chain transformation 4 every sixth tick 0x17..0x46, its position put back by the root. MeasureHeight
//   (0x695F60) writes the target's world translation (+0x54..+0x5C) from its position. Damage: 0x506690 at 0x8A.
// CAMERA (eye 0xB8B7F0, look-at 0xB8B7F8): set on 0 (with 0x50A730 the return armed, field of view 0x1D8E038 = 0x120, roll
//   0x1D977A2 = 0), 8, 0x10, 0x70; 0x657DF0 lerps 0x16..0x45, 0x46..0x65, 0x66..0x6F; the root puts back the field of view
//   (0x1D977A0) once the director ended (the view itself: the engine's return).
// Module globals: 0x2508870..0x2508930 (module file 0x2508870, packet cursors 0x2508874 / 0x2508878 (the capture's geometry),
//   context 0x250887C, caster 0x2508880, target 0x2508884, queues 0x2508888 sprites / 0x2508898 rocks / 0x25088A8 rings /
//   0x25088B8 director / 0x25088C8 root, root pool 0x25088D8, flash level 0x25088E8 / its last 0x25088EC, zero words 0x25088F0
//   (never written), camera anchors 0x25088F8 / 0x25088FC (Zell's +0x1C / +0x20 dwords at the start), 0x2508900 / 0x2508904
//   (Zell's place before the target: the camera's reference), the target's height word 0x2508908, magic buffer 0x250890C, the
//   orthonormalized view 0x2508910).

#include "mag_common.h"

namespace ff8fx
{
namespace beat216
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2508874); }
	inline uint32_t &CaptureCursor() { return var<uint32_t>(0x2508878); }    // the stage capture's geometry packets
	inline CastContext *&Ctx() { return var<CastContext *>(0x250887C); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2508880); }
	inline uint8_t *&Target() { return var<uint8_t *>(0x2508884); }          // the lowest entity of the target's chain
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2508888; }          // sprites (0x1C)
	inline TaskQueue *QRocks() { return (TaskQueue *)0x2508898; }            // rocks (0x24)
	inline TaskQueue *QRings() { return (TaskQueue *)0x25088A8; }            // rings (0x104)
	inline TaskQueue *QDirector() { return (TaskQueue *)0x25088B8; }         // the director (1 x 0x17F0)
	inline TaskQueue *QRoot() { return (TaskQueue *)0x25088C8; }
	inline int32_t &Flash() { return var<int32_t>(0x25088E8); }               // the screen flash level of the tick
	inline int32_t &FlashLast() { return var<int32_t>(0x25088EC); }
	inline const int16_t *ZeroWords() { return (const int16_t *)0x25088F0; } // (never written)
	inline uint32_t *CamAnchor() { return (uint32_t *)0x25088F8; }          // Zell's +0x1C / +0x20 dwords at the start
	inline uint32_t *Anchor() { return (uint32_t *)0x2508900; }             // Zell's place before the target (x | y, z | pad): the camera's reference
	inline int32_t &Height() { return var<int32_t>(0x2508908); }              // the target's model height + 0x4E1
	inline uint32_t &TexBase() { return var<uint32_t>(0x250890C); }          // Magic_TextureOFF (magic buffer)
	inline Mat4x3 *View() { return (Mat4x3 *)0x2508910; }                   // the orthonormalized view of the tick
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }
	inline uint32_t &FramePackets() { return var<uint32_t>(0x1D8E054); }
	inline int16_t *CamEye() { return (int16_t *)0xB8B7F0; }
	inline int16_t *CamLookAt() { return (int16_t *)0xB8B7F8; }
	inline int16_t &ViewWord() { return var<int16_t>(0x1D8E038); }
	inline int16_t &CamRoll() { return var<int16_t>(0x1D977A2); }

	static const uint32_t ORIG_RootTask = 0x695AB0;
	static const uint32_t ORIG_DirectorTask = 0x6960E0;
	static const uint32_t ORIG_RingTask = 0x69A5D0;
	static const uint32_t ORIG_RockATask = 0x69A3F0;
	static const uint32_t ORIG_RockBTask = 0x69A530;
	static const uint32_t ORIG_SparkTask = 0x69A060;
	static const uint32_t ORIG_EmberTask = 0x69A200;
	static const uint32_t ORIG_Callback = 0x6978F0;

	static const uint32_t COM_Anim = 0x1153B30;
	static const uint32_t LAYOUT_Rise = 0x1140C5C, LAYOUT_Fly = 0x11431D4, LAYOUT_Drop = 0x11484D0, LAYOUT_Impact = 0x1141444,
		LAYOUT_Crater = 0x114A6DC, LAYOUT_Pillar = 0x11512C8, LAYOUT_Ring = 0x115049C;
	static const uint32_t KIND_Rise = 0xFE0FC8, KIND_Fly = 0x1154F40, KIND_Impact = 0x1154F3C, KIND_Crater = 0x1154F34,
		KIND_Ring = 0x1154F44;
	static const uint32_t MODEL_Twist = 0x113C3CC, MODEL_Rock = 0x1140AA4;
	static const uint32_t SEQ_Spark = 0x113C1AC, SEQ_Ember = 0x113C2F0;
	static const void *const SOUND_Start = (const void *)0x113C1A8;
	static const uint32_t MORPH = 0x2CD8;   // magic buffer: twisted / blended vertices

	// ---- engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline uint32_t BindExternalAnimSet(uint8_t *entity, void *out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, void *, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationX(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701220)(a, out); }   // MAG_063_RotationXMatrix
	inline void RotationY(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(a, out); }   // Effect_RotationYMatrix
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3x3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }
	inline void Apply3DTransform(const Mat4x3 *m, const int16_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int32_t *)>(0x56C3D0)(m, in, out); }
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); } // MAG_063_RenderPrimSet
	inline uint32_t DrawShadow(uint8_t *e, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(e, ot, a, cursor); } // BattleEntity_DrawShadow
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, ctx, ot, a, cursor); }
	inline void CamLerp(const int16_t *a, const int16_t *b, int32_t t) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t)>(0x657DF0)(a, b, t); } // MAG_325_CameraLerp
	inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }   // BS_Camera_ArmReturnAfterEffect
	inline void OrthonormalizeCamera(const Mat4x3 *in, Mat4x3 *out) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x67AAE0)(in, out); } // MAG_069_OrthonormalizeCameraMatrix
	inline void TransformVectorBy3x3(const Mat4x3 *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int32_t *, int32_t *)>(0x56C600)(m, in, out); }
	inline uint32_t SetDrawEnv(void *prim, uint32_t env) { return fn<uint32_t (__cdecl *)(void *, uint32_t)>(0x45C0F0)(prim, env); } // SSIGPU_SetDrawEnv
	inline void SetDrawAreaPrim(void *prim, const int16_t *rect) { fn<void (__cdecl *)(void *, const int16_t *)>(0x45C940)(prim, rect); }
	inline void SetDrawOffsetPrim(void *prim, const int16_t *xy) { fn<void (__cdecl *)(void *, const int16_t *)>(0x45C9B0)(prim, xy); }
	inline void SetDrawStp(void *prim, int32_t on) { fn<void (__cdecl *)(void *, int32_t)>(0x45C9F0)(prim, on); }
	inline void GetScreenOffset(int32_t *x, int32_t *y) { fn<void (__cdecl *)(int32_t *, int32_t *)>(0x56CD10)(x, y); }
	inline void SetScreenOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x56CCE0)(x, y); }  // Call_Bs_ParseCamera
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
	inline uint32_t FullScreenTile(int32_t r, int32_t g, int32_t b, int32_t level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x7040B0)(r, g, b, level, cursor); } // Effect_FullScreenTile
	inline void StreamStateInit(void *dst) { fn<void (__cdecl *)(void *)>(0x534110)(dst); }
	inline void StreamQueueLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamPreload(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); } // au_re_BattleFile_preLoad_0
	// GTE (software PSX GTE) helpers not in fx_port.h
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }      // IR = MAC = R * V0
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }      // IR = R * IR
	inline void GteLoadIRFromColumn(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E180)(p); }
	inline void GteStoreIRToColumn(void *p) { fn<void (__cdecl *)(void *)>(0x45E470)(p); }
	inline void GteLoadV0FromDwords(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E060)(p); }
	inline void GteLoadIR123FromBytes(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(p); }
	inline void GteStoreIR123LowBytes(void *p) { fn<void (__cdecl *)(void *)>(0x45E410)(p); }
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *p) { fn<void (__cdecl *)(void *)>(0x45E3C0)(p); }
	inline void GteSetSZ123(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45E1D0)(a, b, c, d); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *p) { fn<void (__cdecl *)(void *)>(0x45E3D0)(p); }      // GTE_ReadOTZ (a dword)
	inline void GteReadOTZdiv4(void *p) { fn<void (__cdecl *)(void *)>(0x45E250)(p); }     // GTE_ReadOTZ_SZ3div4
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); }    // GTE_LoadV0_u16
	inline void GteReadIR2(void *dst) { fn<void (__cdecl *)(void *)>(0x703F90)(dst); }        // GTE_ReadIR2Word
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); } // GTE_ReadSXY012_Split
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // GTE_SetFarColor
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }
	inline void GteSetRotMatrixRaw(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE00)(m); }    // GTE_SetRotMatrix
	inline void GteSetTransVectorRaw(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DEF0)(m); } // GTE_SetTransVector

	inline int16_t &W16(void *p, int off) { return *(int16_t *)((uint8_t *)p + off); }
	inline uint16_t &U16(void *p, int off) { return *(uint16_t *)((uint8_t *)p + off); }
	inline int32_t &W32(void *p, int off) { return *(int32_t *)((uint8_t *)p + off); }
	inline uint32_t &U32(void *p, int off) { return *(uint32_t *)((uint8_t *)p + off); }
	inline uint8_t &B8(void *p, int off) { return *((uint8_t *)p + off); }
	// cdq; and edx, m; add; sar n (C signed division by 2^n)
	inline int32_t sdiv_pow2(int32_t v, int n) { return (int32_t)((uint32_t)v + ((uint32_t)(v >> 31) & ((1u << n) - 1))) >> n; }

	// layout sites (the director's layout draws)
	enum : int { SITE_RISE = 1, SITE_FLY, SITE_DROP_A, SITE_DROP_B, SITE_IMPACT, SITE_CRATER, SITE_PILLAR };

#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes at 0x25088D8
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t state;     // +0x0E 0 = not set up, 1 = running
		uint8_t parity;    // +0x0F packet arena parity
	};
	struct DirectorNode // pool of 1 node of 0x17F0 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..0x8A (steps last)
		int16_t pad0E;
		uint8_t bind[0x20];// +0x10 Zell's bind block
		int16_t place[4];  // +0x30 Zell's place before the target (its anchor, y 0, z + both half sizes)
		int16_t anchor[4]; // +0x38 the target's anchor
		uint32_t saved_pos[2]; // +0x40 Zell's +0x1C..+0x23
		uint32_t saved_rot[2]; // +0x48 Zell's +0x0C..+0x13
		int16_t scale;     // +0x50 the target's size / 2 + 0x400
		int16_t lift;      // +0x52 the pillar's height
		int16_t sway_y;    // +0x54 the target's height in the air (0x47..0x66)
		int16_t pad56;
		uint8_t layout_a[0x484];  // +0x58 0x1140C5C (9..0x16) / 0x11431D4 (0x5D..0x78)
		uint8_t layout_b[0x160];  // +0x4DC 0x11484D0 (0x67..0x78)
		uint8_t layout_c[0xA8];   // +0x63C 0x1141444 (0x71..0x7C)
		uint8_t layout_d[0x684];  // +0x6E4 0x114A6DC (0x71..0x89)
		uint8_t layout_e[0xEC];   // +0xD68 0x11512C8 (0x29..0x70)
		uint8_t ghosts[0x988];    // +0xE54 the afterimage block (18 bones)
		int16_t height[2]; // +0x17DC the target's model bottom / top (0x695F60)
		int16_t angles[3]; // +0x17E0 the target's sway angles (x)
		uint8_t pad17E6[2];
		uint32_t target_pos[2]; // +0x17E8 the target's +0x1C..+0x23 at the start
	};
	// the prim-player callback's parameter block (part of the director's / ring task's stack frame)
	struct PlayArg
	{
		Mat4x3 m;          // +0x00 the layout's frame (the view composed)
		const uint8_t *kind;   // +0x20 per-object kind ('1' / '2' / '3': texture windows, '9': the module renderer)
		int16_t depth;     // +0x24 header +0x18
		int16_t scroll;    // +0x26 texture window v (x 0x7F)
		int16_t fade;      // +0x28 header +0xC (0: none)
		int16_t pad2A;
		uint32_t morph;    // +0x2C blended vertex frames
	};
	struct RingNode // 0x104 (8 at the magic buffer + 0x17F0)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t scale;     // +0x12
		int32_t angle;     // +0x14 (0)
		uint8_t layout[0xEC]; // +0x18 0x115049C
	};
	struct RockNode // 0x24 (0x40 at + 0x2010)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t vel[3];    // +0x12
		int8_t acc[3];     // +0x18 (0, 0x14, 0)
		uint8_t life;      // +0x1B
		uint8_t rx, ry;    // +0x1C x / y angles (x 16)
		int8_t spin[2];    // +0x1E
		int16_t scale;     // +0x20
		int16_t pad22;
	};
	struct SpriteNode // 0x1C (0x20 at + 0x2910)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t size;      // +0x12
		int8_t vel[3];     // +0x14 (x 4)
		uint8_t frame;     // +0x17
		int8_t acc[3];     // +0x18 (embers)
		uint8_t pad1B;
	};
	// the afterimage draw context (0x697520: the Field_Alloc block of 0x74 bytes from +0x20)
	struct GhostCtx
	{
		uint32_t polys;    // +0x00 polygon cursor
		uint32_t verts;    // +0x04 screen vertex buffer ([0x1D98B3C])
		int16_t ntri, nquad; // +0x08
		int16_t clip[4];   // +0x0C x0, y0, x1, y1 (x 8)
		uint32_t colour;   // +0x14
		uint32_t visible;  // +0x18 object mask (entity +0x7C)
		uint32_t v[8];     // +0x1C .. +0x3B vertex words (sxy, sz | flags) x 4
		int32_t mac0;      // +0x3C
		uint32_t pad40;
		int32_t otz;       // +0x44
		uint8_t pad48[4];
		uint8_t *ghosts;   // +0x4C afterimage block
		uint32_t slot;     // +0x50 the ghost's recorded matrices
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(DirectorNode) == 0x17F0, "Different Beat nodes");
	static_assert(sizeof(PlayArg) == 0x30 && sizeof(RingNode) == 0x104 && sizeof(RockNode) == 0x24 && sizeof(SpriteNode) == 0x1C && sizeof(GhostCtx) == 0x54, "Different Beat blocks");
}
}
#ifdef FF8_FX_HELD
#include "mag216_different_beat_held.h"
#endif

namespace ff8fx
{
namespace beat216
{
	using namespace eng;
	using namespace magc;

	// ------------------------------------------------------------------
	// The module's prim-model renderer 0x697D10 (the engine's 0x7043B0 with eight primitive lists of its own, used by the
	// layout objects of kind '9'): header (Field_Alloc 0x6C of the prim-player callback) +0 model, +4 vertices, +8..+0xA far
	// colour, +0xC depth-cue level, +0x18 depth, +0x1C flags, +0x20 list cursor, +0x24 MAC0, +0x2C OTZ, +0x30 GTE flag, +0x34
	// a saved screen vertex. Flags: 1 / 4 (2 / 8 gouraud) semi-transparency on / off, 0x10 (0x20) double-sided, 0x40 (0x80)
	// depth cue: the polygon semi-transparent, then a grey overlay polygon (0x80 - level / 32) between draw-mode packets
	// 0xE1000220 / 0xE1000240 (the textured lists insert only the second)
	// ------------------------------------------------------------------
	static inline bool OutX(const uint8_t *xy) { const int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(const uint8_t *xy) { const int16_t v = *(const int16_t *)xy; return v < 0 || v > 0x6C0; }
	// OTZ + the header's depth (not below 0) -> bucket
	static inline uint32_t Bucket(uint8_t *h, uint32_t ot, int32_t shift)
	{
		GteReadOTZWord(h + 0x2C);
		W32(h, 0x2C) = (int32_t)((uint32_t)W32(h, 0x2C) + (uint32_t)W32(h, 0x18));
		if (W32(h, 0x2C) < 0) W32(h, 0x2C) = 0;
		return ot + (uint32_t)(W32(h, 0x2C) >> shift) * 4;
	}
	static inline void ModePrim(uint8_t *p, uint32_t word)
	{
		U32(p, 0) = 0x1000000;
		U32(p, 4) = word;
	}
	// the code word with the semi-transparency switches of the header (on / off bits)
	static inline void Code(uint8_t *h, uint8_t *pk, uint32_t code, uint8_t on, uint8_t off)
	{
		U32(pk, 4) = code;
		if (B8(h, 0x1C) & on) U32(pk, 4) = code | 0x2000000;
		if (B8(h, 0x1C) & off) U32(pk, 4) &= 0xFDFFFFFF;
	}
	// the grey overlay polygon of the depth cue: tag, grey 0x80 - level / 32, code byte
	static inline void Overlay(uint8_t *h, uint8_t *p, uint32_t tag, uint8_t code)
	{
		const uint8_t g = (uint8_t)(0x80 - (uint8_t)(W32(h, 0xC) >> 5));
		U32(p, 0) = tag;
		B8(p, 7) = code;
		B8(p, 6) = g;
		B8(p, 5) = g;
		B8(p, 4) = g;
	}
	static inline const uint8_t *Vert(const uint8_t *verts, const uint8_t *rec, int off) { return verts + (uint32_t)*(const uint16_t *)(rec + off) * 4; }
	// the triangle clip of the three screen vertices at a / b / c (x then y): rejected when all three are off one side
	static inline bool Clip3(const uint8_t *a, const uint8_t *b, const uint8_t *c)
	{
		uint32_t clip = OutX(a) ? 1 : 0;
		if (OutX(b)) clip |= 2;
		if (OutX(c)) clip |= 4;
		if (OutY(a + 2)) clip |= 0x10;
		if (OutY(b + 2)) clip |= 0x20;
		if (OutY(c + 2)) clip |= 0x40;
		return (clip & 7) == 7 || (clip & 0x70) == 0x70;
	}
	static inline uint32_t Clip3Bits(const uint8_t *a, const uint8_t *b, const uint8_t *c)
	{
		uint32_t clip = OutX(a) ? 1 : 0;
		if (OutX(b)) clip |= 2;
		if (OutX(c)) clip |= 4;
		if (OutY(a + 2)) clip |= 0x10;
		if (OutY(b + 2)) clip |= 0x20;
		if (OutY(c + 2)) clip |= 0x40;
		return clip;
	}
	// the list's head: count, the cursor past it (stored), the vertices
	struct ListHead { int32_t count; const uint8_t *rec; const uint8_t *verts; };
	static inline ListHead Head(uint8_t *h)
	{
		const uint8_t *list = *(const uint8_t **)(h + 0x20);
		ListHead l = { *(const int32_t *)list, list + 4, *(const uint8_t **)(h + 4) };
		*(const uint8_t **)(h + 0x20) = l.rec;
		return l;
	}

	// 0x697E40: flat triangles (0xC: code + rgb, 3 vertex indices) -> POLY_F3 (0x14)
	static uint32_t FlatTris(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		ListHead l = Head(h);
		uint8_t *pk = (uint8_t *)cursor;
		if (l.count <= 0) return (uint32_t)pk;
		const uint8_t *rec = l.rec;
		for (int32_t k = l.count; k != 0; k--, rec += 0xC)
		{
			GteLoadV012(Vert(l.verts, rec, 4), Vert(l.verts, rec, 6), Vert(l.verts, rec, 8));
			GteRTPT();
			U32(pk, 0) = 0x4000000;
			Code(h, pk, *(const uint32_t *)rec, 1, 4);
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x24);
			if (W32(h, 0x24) < 0 && !(B8(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0xC, pk + 0x10);
			GteAVSZ3();
			if (Clip3(pk + 8, pk + 0xC, pk + 0x10)) continue;
			const uint32_t bucket = Bucket(h, ot, shift);
			if (B8(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(W32(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
				B8(pk, 7) |= 2;
				InsertPrimAutoDepth(bucket, pk);
				uint8_t *m1 = pk + 0x14, *p2 = pk + 0x1C, *m2 = pk + 0x30;
				pk += 0x38;
				ModePrim(m1, 0xE1000220);
				InsertPrimAutoDepth(bucket, m1);
				GteReadSXY012(p2 + 8, p2 + 0xC, p2 + 0x10);
				Overlay(h, p2, 0x4000000, 0x22);
				InsertPrimAutoDepth(bucket, p2);
				ModePrim(m2, 0xE1000240);
				InsertPrimAutoDepth(bucket, m2);
			}
			else
			{
				InsertPrimAutoDepth(bucket, pk);
				pk += 0x14;
			}
		}
		*(const uint8_t **)(h + 0x20) = rec;
		return (uint32_t)pk;
	}

	// 0x6981A0: flat quads (0xC: code + rgb, 4 vertex indices) -> POLY_F4 (0x18); the overlay's third vertex is the saved
	// screen vertex 2 (+0x34), its others the GTE's screen FIFO after the fourth vertex
	static uint32_t FlatQuads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		ListHead l = Head(h);
		uint8_t *pk = (uint8_t *)cursor;
		if (l.count <= 0) return (uint32_t)pk;
		const uint8_t *rec = l.rec;
		for (int32_t k = l.count; k != 0; k--, rec += 0xC)
		{
			GteLoadV012(Vert(l.verts, rec, 4), Vert(l.verts, rec, 6), Vert(l.verts, rec, 8));
			GteRTPT();
			U32(pk, 0) = 0x5000000;
			Code(h, pk, *(const uint32_t *)rec, 1, 4);
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x24);
			if (W32(h, 0x24) < 0 && !(B8(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0xC, pk + 0x10);
			GteLoadV0(Vert(l.verts, rec, 0xA));
			GteRTPS();
			uint32_t clip = Clip3Bits(pk + 8, pk + 0xC, pk + 0x10);
			GteReadSXY2(pk + 0x14);
			GteAVSZ4();
			if (OutX(pk + 0x14)) clip |= 8;
			if (OutY(pk + 0x16)) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			const uint32_t bucket = Bucket(h, ot, shift);
			if (B8(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(W32(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
				U32(h, 0x34) = U32(pk, 0x10);
				B8(pk, 7) |= 2;
				InsertPrimAutoDepth(bucket, pk);
				uint8_t *m1 = pk + 0x18, *p2 = pk + 0x20, *m2 = pk + 0x38;
				pk += 0x40;
				ModePrim(m1, 0xE1000220);
				InsertPrimAutoDepth(bucket, m1);
				U32(p2, 0x10) = U32(h, 0x34);
				GteReadSXY012(p2 + 8, p2 + 0xC, p2 + 0x14);
				Overlay(h, p2, 0x5000000, 0x2A);
				InsertPrimAutoDepth(bucket, p2);
				ModePrim(m2, 0xE1000240);
				InsertPrimAutoDepth(bucket, m2);
			}
			else
			{
				InsertPrimAutoDepth(bucket, pk);
				pk += 0x18;
			}
		}
		*(const uint8_t **)(h + 0x20) = rec;
		return (uint32_t)pk;
	}

	// 0x6985C0: textured triangles (0x14: code + rgb, 3 vertex indices + uv2, uv0 + clut, uv1 + tpage) -> POLY_FT3 (0x20)
	static uint32_t TexTris(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		ListHead l = Head(h);
		uint8_t *pk = (uint8_t *)cursor;
		if (l.count <= 0) return (uint32_t)pk;
		const uint8_t *rec = l.rec;
		for (int32_t k = l.count; k != 0; k--, rec += 0x14)
		{
			GteLoadV012(Vert(l.verts, rec, 4), Vert(l.verts, rec, 6), Vert(l.verts, rec, 8));
			GteRTPT();
			U32(pk, 0) = 0x7000000;
			Code(h, pk, *(const uint32_t *)rec, 1, 4);
			U32(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
			U32(pk, 0x14) = *(const uint32_t *)(rec + 0x10);
			U32(pk, 0x1C) = *(const uint32_t *)(rec + 8) >> 16;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x24);
			if (W32(h, 0x24) < 0 && !(B8(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3();
			if (Clip3(pk + 8, pk + 0x10, pk + 0x18)) continue;
			const uint32_t bucket = Bucket(h, ot, shift);
			if (B8(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(W32(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
				B8(pk, 7) |= 2;
				InsertPrimAutoDepth(bucket, pk);
				uint8_t *p2 = pk + 0x20, *m2 = pk + 0x34;
				pk += 0x3C;
				GteReadSXY012(p2 + 8, p2 + 0xC, p2 + 0x10);
				Overlay(h, p2, 0x4000000, 0x22);
				InsertPrimAutoDepth(bucket, p2);
				ModePrim(m2, 0xE1000240);
				InsertPrimAutoDepth(bucket, m2);
			}
			else
			{
				InsertPrimAutoDepth(bucket, pk);
				pk += 0x20;
			}
		}
		*(const uint8_t **)(h + 0x20) = rec;
		return (uint32_t)pk;
	}

	// 0x698900: textured quads (0x18: code + rgb, 4 vertex indices, uv0 + clut, uv1 + tpage, uv2 | uv3) -> POLY_FT4 (0x28)
	static uint32_t TexQuads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		ListHead l = Head(h);
		uint8_t *pk = (uint8_t *)cursor;
		if (l.count <= 0) return (uint32_t)pk;
		const uint8_t *rec = l.rec;
		for (int32_t k = l.count; k != 0; k--, rec += 0x18)
		{
			GteLoadV012(Vert(l.verts, rec, 4), Vert(l.verts, rec, 6), Vert(l.verts, rec, 8));
			GteRTPT();
			U32(pk, 0) = 0x9000000;
			Code(h, pk, *(const uint32_t *)rec, 1, 4);
			U32(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
			U32(pk, 0x14) = *(const uint32_t *)(rec + 0x10);
			U32(pk, 0x1C) = *(const uint32_t *)(rec + 0x14);
			U32(pk, 0x24) = *(const uint32_t *)(rec + 0x14) >> 16;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x24);
			if (W32(h, 0x24) < 0 && !(B8(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0(Vert(l.verts, rec, 0xA));
			GteRTPS();
			uint32_t clip = Clip3Bits(pk + 8, pk + 0x10, pk + 0x18);
			GteReadSXY2(pk + 0x20);
			GteAVSZ4();
			if (OutX(pk + 0x20)) clip |= 8;
			if (OutY(pk + 0x22)) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			const uint32_t bucket = Bucket(h, ot, shift);
			if (B8(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(W32(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
				U32(h, 0x34) = U32(pk, 0x18);
				B8(pk, 7) |= 2;
				InsertPrimAutoDepth(bucket, pk);
				uint8_t *p2 = pk + 0x28, *m2 = pk + 0x40;
				pk += 0x48;
				U32(p2, 0x10) = U32(h, 0x34);
				GteReadSXY012(p2 + 8, p2 + 0xC, p2 + 0x14);
				Overlay(h, p2, 0x5000000, 0x2A);
				InsertPrimAutoDepth(bucket, p2);
				ModePrim(m2, 0xE1000240);
				InsertPrimAutoDepth(bucket, m2);
			}
			else
			{
				InsertPrimAutoDepth(bucket, pk);
				pk += 0x28;
			}
		}
		*(const uint8_t **)(h + 0x20) = rec;
		return (uint32_t)pk;
	}

	// 0x698D00: gouraud triangles (0x14: code + rgb0, 3 vertex indices, rgb1, rgb2) -> POLY_G3 (0x1C)
	static uint32_t GouraudTris(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		ListHead l = Head(h);
		uint8_t *pk = (uint8_t *)cursor;
		if (l.count <= 0) return (uint32_t)pk;
		const uint8_t *rec = l.rec;
		for (int32_t k = l.count; k != 0; k--, rec += 0x14)
		{
			GteLoadV012(Vert(l.verts, rec, 4), Vert(l.verts, rec, 6), Vert(l.verts, rec, 8));
			GteRTPT();
			U32(pk, 0) = 0x6000000;
			Code(h, pk, *(const uint32_t *)rec, 2, 8);
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x24);
			if (W32(h, 0x24) < 0 && !(B8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3();
			if (Clip3(pk + 8, pk + 0x10, pk + 0x18)) continue;
			const uint32_t bucket = Bucket(h, ot, shift);
			if (B8(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0xC, rec + 0x10, pk + 4);
				GteSetIR0(W32(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0xC, pk + 0x14, pk + 4);
				B8(pk, 7) |= 2;
				InsertPrimAutoDepth(bucket, pk);
				uint8_t *m1 = pk + 0x1C, *p2 = pk + 0x24, *m2 = pk + 0x38;
				pk += 0x40;
				ModePrim(m1, 0xE1000220);
				InsertPrimAutoDepth(bucket, m1);
				GteReadSXY012(p2 + 8, p2 + 0xC, p2 + 0x10);
				Overlay(h, p2, 0x4000000, 0x22);
				InsertPrimAutoDepth(bucket, p2);
				ModePrim(m2, 0xE1000240);
				InsertPrimAutoDepth(bucket, m2);
			}
			else
			{
				U32(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
				U32(pk, 0x14) = *(const uint32_t *)(rec + 0x10);
				InsertPrimAutoDepth(bucket, pk);
				pk += 0x1C;
			}
		}
		*(const uint8_t **)(h + 0x20) = rec;
		return (uint32_t)pk;
	}

	// 0x699110: gouraud quads (0x18: code + rgb0, 4 vertex indices, rgb1, rgb2, rgb3) -> POLY_G4 (0x24)
	static uint32_t GouraudQuads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		ListHead l = Head(h);
		uint8_t *pk = (uint8_t *)cursor;
		if (l.count <= 0) return (uint32_t)pk;
		const uint8_t *rec = l.rec;
		for (int32_t k = l.count; k != 0; k--, rec += 0x18)
		{
			GteLoadV012(Vert(l.verts, rec, 4), Vert(l.verts, rec, 6), Vert(l.verts, rec, 8));
			GteRTPT();
			U32(pk, 0) = 0x8000000;
			Code(h, pk, *(const uint32_t *)rec, 2, 8);
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x24);
			if (W32(h, 0x24) < 0 && !(B8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0(Vert(l.verts, rec, 0xA));
			GteRTPS();
			uint32_t clip = Clip3Bits(pk + 8, pk + 0x10, pk + 0x18);
			GteReadSXY2(pk + 0x20);
			GteAVSZ4();
			if (OutX(pk + 0x20)) clip |= 8;
			if (OutY(pk + 0x22)) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			const uint32_t bucket = Bucket(h, ot, shift);
			if (B8(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0xC, rec + 0x10, rec + 0x14);
				GteSetIR0(W32(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0xC, pk + 0x14, pk + 0x1C);
				GteLoadRGBC(pk + 4);
				GteDPCS();
				GteStoreRGB2(pk + 4);
				U32(h, 0x34) = U32(pk, 0x18);
				B8(pk, 7) |= 2;
				InsertPrimAutoDepth(bucket, pk);
				uint8_t *m1 = pk + 0x24, *p2 = pk + 0x2C, *m2 = pk + 0x44;
				pk += 0x4C;
				ModePrim(m1, 0xE1000220);
				InsertPrimAutoDepth(bucket, m1);
				U32(p2, 0x10) = U32(h, 0x34);
				GteReadSXY012(p2 + 8, p2 + 0xC, p2 + 0x14);
				Overlay(h, p2, 0x5000000, 0x2A);
				InsertPrimAutoDepth(bucket, p2);
				ModePrim(m2, 0xE1000240);
				InsertPrimAutoDepth(bucket, m2);
			}
			else
			{
				U32(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
				U32(pk, 0x14) = *(const uint32_t *)(rec + 0x10);
				U32(pk, 0x1C) = *(const uint32_t *)(rec + 0x14);
				InsertPrimAutoDepth(bucket, pk);
				pk += 0x24;
			}
		}
		*(const uint8_t **)(h + 0x20) = rec;
		return (uint32_t)pk;
	}

	// 0x699630: gouraud textured triangles (0x1C: code + rgb0, 3 vertex indices + uv2, uv0 + clut, uv1 + tpage, rgb1, rgb2)
	// -> POLY_GT3 (0x28)
	static uint32_t GouraudTexTris(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		ListHead l = Head(h);
		uint8_t *pk = (uint8_t *)cursor;
		if (l.count <= 0) return (uint32_t)pk;
		const uint8_t *rec = l.rec;
		for (int32_t k = l.count; k != 0; k--, rec += 0x1C)
		{
			GteLoadV012(Vert(l.verts, rec, 4), Vert(l.verts, rec, 6), Vert(l.verts, rec, 8));
			GteRTPT();
			U32(pk, 0) = 0x9000000;
			Code(h, pk, *(const uint32_t *)rec, 2, 8);
			U32(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
			U32(pk, 0x18) = *(const uint32_t *)(rec + 0x10);
			U32(pk, 0x24) = *(const uint32_t *)(rec + 8) >> 16;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x24);
			if (W32(h, 0x24) < 0 && !(B8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			GteAVSZ3();
			if (Clip3(pk + 8, pk + 0x14, pk + 0x20)) continue;
			const uint32_t bucket = Bucket(h, ot, shift);
			if (B8(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0x14, rec + 0x18, pk + 4);
				GteSetIR0(W32(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 4);
				B8(pk, 7) |= 2;
				InsertPrimAutoDepth(bucket, pk);
				uint8_t *p2 = pk + 0x28, *m2 = pk + 0x3C;
				pk += 0x44;
				GteReadSXY012(p2 + 8, p2 + 0xC, p2 + 0x10);
				Overlay(h, p2, 0x4000000, 0x22);
				InsertPrimAutoDepth(bucket, p2);
				ModePrim(m2, 0xE1000240);
				InsertPrimAutoDepth(bucket, m2);
			}
			else
			{
				U32(pk, 0x10) = *(const uint32_t *)(rec + 0x14);
				U32(pk, 0x1C) = *(const uint32_t *)(rec + 0x18);
				InsertPrimAutoDepth(bucket, pk);
				pk += 0x28;
			}
		}
		*(const uint8_t **)(h + 0x20) = rec;
		return (uint32_t)pk;
	}

	// 0x699A00: gouraud textured quads (0x24: code + rgb0, 4 vertex indices, uv0 + clut, uv1 + tpage, uv2 | uv3, rgb1, rgb2,
	// rgb3) -> POLY_GT4 (0x34)
	static uint32_t GouraudTexQuads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		ListHead l = Head(h);
		uint8_t *pk = (uint8_t *)cursor;
		if (l.count <= 0) return (uint32_t)pk;
		const uint8_t *rec = l.rec;
		for (int32_t k = l.count; k != 0; k--, rec += 0x24)
		{
			GteLoadV012(Vert(l.verts, rec, 4), Vert(l.verts, rec, 6), Vert(l.verts, rec, 8));
			GteRTPT();
			U32(pk, 0) = 0xC000000;
			Code(h, pk, *(const uint32_t *)rec, 2, 8);
			U32(pk, 0xC) = *(const uint32_t *)(rec + 0xC);
			U32(pk, 0x18) = *(const uint32_t *)(rec + 0x10);
			U32(pk, 0x24) = *(const uint32_t *)(rec + 0x14);
			U32(pk, 0x30) = *(const uint32_t *)(rec + 0x14) >> 16;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x24);
			if (W32(h, 0x24) < 0 && !(B8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			GteLoadV0(Vert(l.verts, rec, 0xA));
			GteRTPS();
			uint32_t clip = Clip3Bits(pk + 8, pk + 0x14, pk + 0x20);
			GteReadSXY2(pk + 0x2C);
			GteAVSZ4();
			if (OutX(pk + 0x2C)) clip |= 8;
			if (OutY(pk + 0x2E)) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			const uint32_t bucket = Bucket(h, ot, shift);
			if (B8(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0x18, rec + 0x1C, rec + 0x20);
				GteSetIR0(W32(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 0x28);
				GteLoadRGBC(pk + 4);
				GteDPCS();
				GteStoreRGB2(pk + 4);
				U32(h, 0x34) = U32(pk, 0x20);
				B8(pk, 7) |= 2;
				InsertPrimAutoDepth(bucket, pk);
				uint8_t *p2 = pk + 0x34, *m2 = pk + 0x4C;
				pk += 0x54;
				U32(p2, 0x10) = U32(h, 0x34);
				GteReadSXY012(p2 + 8, p2 + 0xC, p2 + 0x14);
				Overlay(h, p2, 0x5000000, 0x2A);
				InsertPrimAutoDepth(bucket, p2);
				ModePrim(m2, 0xE1000240);
				InsertPrimAutoDepth(bucket, m2);
			}
			else
			{
				U32(pk, 0x10) = *(const uint32_t *)(rec + 0x18);
				U32(pk, 0x1C) = *(const uint32_t *)(rec + 0x1C);
				U32(pk, 0x28) = *(const uint32_t *)(rec + 0x20);
				InsertPrimAutoDepth(bucket, pk);
				pk += 0x34;
			}
		}
		*(const uint8_t **)(h + 0x20) = rec;
		return (uint32_t)pk;
	}

	// 0x697D10: vertex base = model + 8 unless header flag 0x2000, far colour from the header's colour bytes, then the eight
	// lists (an empty list is skipped)
	static uint32_t RenderModel(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		if (!(U32(h, 0x1C) & 0x2000)) U32(h, 4) = U32(h, 0) + 8;
		const uint8_t *model = *(const uint8_t **)h;
		*(const uint8_t **)(h + 0x20) = model + *(const uint32_t *)model;
		GteSetFarColor(B8(h, 8), B8(h, 9), B8(h, 0xA));
		uint32_t c = cursor;
		for (int i = 0; i < 8; i++)
		{
			const uint8_t *list = *(const uint8_t **)(h + 0x20);
			if (*(const uint32_t *)list == 0)
			{
				*(const uint8_t **)(h + 0x20) = list + 4;
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

	// action 0's target records (24 bytes each, +0 = the slot)
	inline uint8_t *Targets() { return Ctx()->actions[0].targets; }

	// the record's offset is loaded into the GTE V0 with its 4th word never written (the stack's residue: the port loads 0)
	static int16_t g_off_pad = 0;

	// 0x6978F0: one layout object: its model (frame / blended frames), its matrix (the record's angles; the offset through
	// the block's frame, or the orthonormalized view 0x2508910 (flag 0x1000), or as is (0x200); the rotation composed unless
	// 0x8000; the record's scale), drawn by the engine's 0x701DD0 with the block's kind: '1' / '2' / '3' texture windows
	// scrolled (+0x26), the fade (+0x28); '9' through the module renderer 0x697D10 (depth + 0x100, no fade)
	static void __cdecl Callback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PlayArg *arg = (const PlayArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
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
		Mat4x3 m;
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		const int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], g_off_pad };
		if (r->flags & 0x1000)
		{
			GteSetRotMatrixRaw(View());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t t[3];
			GteReadMAC123(t);
			m.t[0] = t[0];
			m.t[1] = t[1];
			m.t[2] = t[2];
			if (!(r->flags & 0x8000)) MatrixMultiply(View(), &m);
		}
		else if (r->flags & 0x200)
		{
			m.t[0] = off[0];
			m.t[1] = off[1];
			m.t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixRaw(&arg->m);
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
				// 3x3 scale matrix (diagonal x, y, z)
				Mat4x3 s;
				s.m[0][0] = r->scale[0];
				s.m[1][1] = r->scale[1];
				s.m[2][2] = r->scale[2];
				s.m[1][2] = 0;
				s.m[0][2] = 0;
				s.m[2][1] = 0;
				s.m[0][1] = 0;
				s.m[2][0] = 0;
				s.m[1][0] = 0;
				MatrixMultiply3x3(&m, &s);
			}
			else
			{
				const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		U32(h, 0x1C) = (r->flags & 0x4000) ? 0x2000 : 0x2030;
		if (r->flags & 0x2000) U32(h, 0x1C) |= 0xC;
		W32(h, 0xC) = r->a;
		if (r->a != 0)
		{
			U32(h, 0x1C) |= 0xC0;
			U32(h, 8) = *(const uint32_t *)r->rgb;
		}
		W32(h, 0x18) = arg->depth;
		U16(h, 0x22) = 0;
		U16(h, 0x20) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		switch ((uint32_t)arg->kind[object] - 0x31)
		{
		case 0:
			U16(h, 0x2C) = 0;
			U16(h, 0x2E) = 0;
			U16(h, 0x32) = 0x80;
			U16(h, 0x30) = 0x100;
			U16(h, 0x22) = (uint16_t)(*(const uint8_t *)&arg->scroll & 0x7F);
			break;
		case 1:
			U16(h, 0x2C) = 0;
			U16(h, 0x2E) = 0;
			U16(h, 0x32) = 0x80;
			U16(h, 0x30) = 0x100;
			U16(h, 0x22) = (uint16_t)((uint8_t)-*(const uint8_t *)&arg->scroll & 0x7F);
			break;
		case 2:
			U16(h, 0x2C) = 0;
			U16(h, 0x2E) = 0x80;
			U16(h, 0x32) = 0x80;
			U16(h, 0x30) = 0x100;
			U16(h, 0x22) = (uint16_t)(*(const uint8_t *)&arg->scroll & 0x7F);
			break;
		case 8:
			if (W32(h, 0xC) != 0) B8(h, 0x1C) &= 0xF3;
			W32(h, 0x18) += 0x100;
			PacketCursor() = RenderModel(h, OTBase() + 0x44, 2, PacketCursor());
			FieldFree(0x6C);
			return;
		}
		if (arg->fade != 0)
		{
			U32(h, 8) = 0;
			W32(h, 0xC) = arg->fade;
			U32(h, 0x1C) |= 0xC0;
		}
		PacketCursor() = RenderPrimSet(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x6C);
	}

	// ------------------------------------------------------------------
	// Afterimages of Zell (the Duel family: 212's 0x6A1230 / 0x6A2A90, 274's 0x68AAE0, its own 0x697520 / 0x697700 /
	// 0x697850 with 18 recorded bones)
	// ------------------------------------------------------------------
	// 0x6A1230 (212's range: Different Beat calls it): afterimage block init: no ghost yet, `n` recorded bones (the
	// 128-entry bone -> record map, 0xFF = not recorded)
	static void GhostInit(uint8_t *g, int32_t n, const int32_t *bones)
	{
		g[3] = 0;
		g[2] = 0;
		g[1] = 0;
		g[0] = 0;
		g[4] = (uint8_t)n;
		memset(g + 5, 0xFF, 0x80);
		for (int32_t i = 0; i < n; i++) g[5 + bones[i]] = (uint8_t)i;
	}

	// 0x697850: the recorded bones' matrices of Zell's model into the next ghost slot (0x480 bytes: 18 x 0x20, then their
	// view matrices)
	static void GhostRecord(uint8_t *entity, uint8_t *g)
	{
		g[0] = 1;
		const uint8_t s = g[2];
		g[3] = s;
		uint8_t *dst = g + 0x88 + (uint32_t)s * 0x480;
		g[2] = (uint8_t)(s + 1);
		if ((uint8_t)(s + 1) >= 2) g[2] = 0;
		g[1] = (uint8_t)(g[1] + 1);
		if (g[1] > 2) g[1] = 2;
		uint8_t *skel = **(uint8_t ***)(entity + 0x64);
		for (uint32_t b = 0; b < skel[0]; b++)
		{
			const int32_t k = (int8_t)g[5 + b];
			if (k >= 0) memcpy(dst + k * 0x20, skel + 0x20 + b * 0x30, 0x20);
		}
	}

	// 0x6A2A90 (212's range: Different Beat calls it): a vertex group of an object through the current GTE matrix into the
	// screen vertex buffer (sxy, sz / 4, clip flags: 1 / 2 left / right, 4 / 8 top / bottom of the context's rectangle x 8;
	// behind the eye: flags 0x10 only)
	static void GhostVertices(uint8_t **cursor, uint8_t **out, GhostCtx *c)
	{
		uint8_t *src = *cursor;
		int32_t count = *(const int16_t *)src;
		uint8_t *o = *out;
		src += 2;
		for (; count != 0; count--)
		{
			GteLoadV0u(src);
			GteRTPS();
			GteReadSXY2(&c->v[0]);
			GteReadOTZdiv4(&c->v[1]);
			src += 6;
			if (U16(c, 0x20) == 0)
			{
				U16(o, 6) = 0x10;
			}
			else
			{
				U16(c, 0x22) = 0;
				const int32_t x = W16(c, 0x1C);
				if (x < (int32_t)c->clip[0] * 8) U16(c, 0x22) = 1;
				else if (x >= (int32_t)c->clip[2] * 8) U16(c, 0x22) = 2;
				const int32_t y = W16(c, 0x1E);
				if (y < (int32_t)c->clip[1] * 8) B8(c, 0x22) |= 4;
				else if (y >= (int32_t)c->clip[3] * 8) B8(c, 0x22) |= 8;
				U32(o, 0) = c->v[0];
				U32(o, 4) = c->v[1];
			}
			o += 8;
		}
		*cursor = src;
		*out = o;
	}

	// 0x68AAE0 (Duel's range: Different Beat calls it): the object's flat textured triangles / quads (semi-transparent, the
	// context's colour) from the screen vertex buffer; a polygon with a vertex behind the eye (flags >= 0x10), outside on one
	// side or back-facing is dropped
	static uint32_t GhostPolys(GhostCtx *c, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint8_t *vb = (uint8_t *)c->verts;
		uint8_t *p = (uint8_t *)c->polys;
		int16_t n = c->ntri;
		c->ntri = (int16_t)(n - 1);
		if (n != 0)
		{
			do
			{
				c->v[0] = U32(vb, (U16(p, 0) & 0xFFF) * 8);
				c->v[1] = U32(vb, (U16(p, 0) & 0xFFF) * 8 + 4);
				if (U16(c, 0x22) < 0x10)
				{
					c->v[2] = U32(vb, (U16(p, 2) & 0xFFF) * 8);
					c->v[3] = U32(vb, (U16(p, 2) & 0xFFF) * 8 + 4);
					if (U16(c, 0x2A) < 0x10)
					{
						c->v[4] = U32(vb, (U16(p, 4) & 0xFFF) * 8);
						c->v[5] = U32(vb, (U16(p, 4) & 0xFFF) * 8 + 4);
						if (U16(c, 0x32) < 0x10 && (B8(c, 0x22) & (uint8_t)(B8(c, 0x2A) & B8(c, 0x32))) == 0)
						{
							GteSetSXY012(c->v[0], c->v[2], c->v[4]);
							GteNCLIP();
							GteReadMAC0(&c->mac0);
							if (c->mac0 >= 0)
							{
								GteSetSZ123(U16(c, 0x20), U16(c, 0x28), U16(c, 0x30));
								GteAVSZ3();
								U32((void *)pk, 0) = 0x7000000;
								U32((void *)pk, 4) = c->colour | 0x26000000;
								U32((void *)pk, 8) = c->v[0];
								U32((void *)pk, 0x10) = c->v[2];
								U32((void *)pk, 0x18) = c->v[4];
								U32((void *)pk, 0xC) = U32(p, 8);
								U32((void *)pk, 0x14) = U32(p, 0xC);
								U16((void *)pk, 0x16) &= 0xFDFF;
								U16((void *)pk, 0x1C) = U16(p, 6);
								GteReadOTZWord(&c->otz);
								InsertPrimAutoDepth(ot + (uint32_t)(c->otz >> shift) * 4, (void *)pk);
								pk += 0x20;
							}
						}
					}
				}
				n = c->ntri;
				p += 0x10;
				c->ntri = (int16_t)(n - 1);
			} while (n != 0);
		}
		n = c->nquad;
		c->nquad = (int16_t)(n - 1);
		if (n != 0)
		{
			p += 4; // (once: the loop's cursor is 4 bytes into the quad)
			do
			{
				c->v[0] = U32(vb, (U16(p, -4) & 0xFFF) * 8);
				c->v[1] = U32(vb, (U16(p, -4) & 0xFFF) * 8 + 4);
				if (U16(c, 0x22) < 0x10)
				{
					c->v[2] = U32(vb, (U16(p, -2) & 0xFFF) * 8);
					c->v[3] = U32(vb, (U16(p, -2) & 0xFFF) * 8 + 4);
					if (U16(c, 0x2A) < 0x10)
					{
						c->v[4] = U32(vb, (U16(p, 0) & 0xFFF) * 8);
						c->v[5] = U32(vb, (U16(p, 0) & 0xFFF) * 8 + 4);
						if (U16(c, 0x32) < 0x10)
						{
							c->v[6] = U32(vb, (U16(p, 2) & 0xFFF) * 8);
							c->v[7] = U32(vb, (U16(p, 2) & 0xFFF) * 8 + 4);
							if (U16(c, 0x3A) < 0x10 && (B8(c, 0x3A) & (uint8_t)(B8(c, 0x22) & (uint8_t)(B8(c, 0x2A) & B8(c, 0x32)))) == 0)
							{
								GteSetSXY012(c->v[0], c->v[2], c->v[4]);
								GteNCLIP();
								GteReadMAC0(&c->mac0);
								if (c->mac0 >= 0)
								{
									GteSetSZ0123(U16(c, 0x20), U16(c, 0x28), U16(c, 0x30), U16(c, 0x38));
									GteAVSZ4();
									U32((void *)pk, 0) = 0x9000000;
									U32((void *)pk, 4) = c->colour | 0x2E000000;
									U32((void *)pk, 8) = c->v[0];
									U32((void *)pk, 0x10) = c->v[2];
									U32((void *)pk, 0x18) = c->v[4];
									U32((void *)pk, 0x20) = c->v[6];
									U32((void *)pk, 0xC) = U32(p, 4);
									U32((void *)pk, 0x14) = U32(p, 8);
									U16((void *)pk, 0x1C) = U16(p, 0xC);
									U16((void *)pk, 0x16) &= 0xFDFF;
									U16((void *)pk, 0x24) = U16(p, 0xE);
									GteReadOTZWord(&c->otz);
									InsertPrimAutoDepth(ot + (uint32_t)(c->otz >> shift) * 4, (void *)pk);
									pk += 0x28;
								}
							}
						}
					}
				}
				n = c->nquad;
				p += 0x14;
				c->nquad = (int16_t)(n - 1);
			} while (n != 0);
		}
		return pk;
	}

	// 0x697700: the model's visible objects (entity +0x7C mask) with their recorded bones' (ghost) view matrices (the slot's
	// second half, +0x240): vertex groups of a bone not recorded are marked behind the eye; then the polygons
	static void GhostModel(uint8_t *model, GhostCtx *c, uint32_t ot, int32_t shift)
	{
		const uint8_t *geom = *(uint8_t **)(model + 4);
		const int32_t nobj = *(const int32_t *)geom;
		const uint32_t *offs = (const uint32_t *)(geom + 4);
		for (int32_t i = 0; i < nobj; i++)
		{
			uint8_t *obj = (uint8_t *)(offs[i] + *(uint32_t *)(model + 4));
			if (!(c->visible & (1u << i))) continue;
			uint8_t *out = (uint8_t *)c->verts;
			uint8_t *cur = obj + 2;
			int32_t groups = *(const int16_t *)obj;
			for (; groups > 0; groups--)
			{
				const int32_t bone = *(const int16_t *)cur;
				cur += 2;
				const int32_t k = (int8_t)c->ghosts[bone + 5];
				if (k >= 0)
				{
					const Mat4x3 *m = (const Mat4x3 *)(c->slot + (uint32_t)(k + 0x12) * 0x20);
					GteSetRotMatrixRaw(m);
					GteSetTransVectorRaw(m);
					GhostVertices(&cur, &out, c);
				}
				else
				{
					const int32_t nv = *(const int16_t *)cur;
					cur += nv * 6 + 2;
					for (int32_t v = nv; v != 0; v--)
					{
						U16(out, 6) = 0x10;
						out += 8;
					}
				}
			}
			cur = (uint8_t *)(((uint32_t)cur + 3) & ~3u);
			c->ntri = *(const int16_t *)cur;
			cur += 2;
			c->nquad = *(const int16_t *)cur;
			cur += 0xA;
			c->polys = (uint32_t)cur;
			PacketCursor() = GhostPolys(c, ot, shift, PacketCursor());
		}
	}

	// 0x697520: the recorded ghosts (up to 2, the newer one first, brighter: his colour (+0x28) x 0x800 * 1.5, then x 0x400 *
	// 1.5) of Zell's model: every recorded bone matrix through the view x his world matrix (into the slot's second half), the
	// model drawn with them (0x697700); the count drops by one on a tick without a recording
	static void GhostDraw(uint8_t *entity, uint8_t *g)
	{
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_ghosts(entity, g);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x74);
		GhostCtx *c = (GhostCtx *)(h + 0x20);
		c->verts = var<uint32_t>(0x1D98B3C);
		c->clip[0] = 0;
		c->clip[1] = 0;
		c->clip[2] = 0x140;
		c->clip[3] = 0xD8;
		c->visible = *(const uint32_t *)(entity + 0x7C);
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)h);
		c->ghosts = g;
		int32_t w = 0x800;
		for (int32_t i = 0; i < g[1]; i++)
		{
			GteSetIR0((w >> 1) + w);
			GteLoadIR123FromBytes(entity + 0x28);
			GteGPF();
			GteStoreIR123LowBytes(&c->colour);
			B8(c, 0x17) = 0;
			c->slot = (uint32_t)g + 0x88 + (((uint32_t)g[3] - (uint32_t)i) & 1) * 0x480;
			for (int32_t b = 0; b < g[4]; b++)
			{
				const uint32_t r = c->slot + (uint32_t)b * 0x20;
				GteSetRotMatrixRaw(h);
				GteLoadIRFromColumn((const void *)(r + 0));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x240));
				GteLoadIRFromColumn((const void *)(r + 2));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x242));
				GteLoadIRFromColumn((const void *)(r + 4));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x244));
				GteSetTransVectorRaw(h);
				GteLoadV0FromDwords((const void *)(r + 0x14));
				GteMVMVA_RotV0Tr();
				GteReadMAC123((int32_t *)(r + 0x254));
			}
			GhostModel(*(uint8_t **)(entity + 0x64), c, OTBase() + 0x44, 0);
			w -= 0x400;
		}
		FieldFree(0x74);
		if (g[0] == 0 && g[1] != 0) g[1]--;
		g[0] = 0;
	}

	// ------------------------------------------------------------------
	// Small helpers
	// ------------------------------------------------------------------
	// 0x69A700 (MAG_210's name, Different Beat's range): the box (min x, y, z, max x, y, z) of a prim model's vertices (+0xC,
	// 8 bytes)
	static void ModelBox(uint32_t model, int16_t *out)
	{
		const uint8_t *m = (const uint8_t *)model + 4;
		int32_t n = *(const int32_t *)m;
		const int16_t *v = (const int16_t *)(m + 8);
		out[3] = out[0] = v[0];
		out[4] = out[1] = v[1];
		out[5] = out[2] = v[2];
		n--;
		do
		{
			v += 4;
			if (out[0] > v[0]) out[0] = v[0];
			else if (out[3] < v[0]) out[3] = v[0];
			if (out[1] > v[1]) out[1] = v[1];
			else if (out[4] < v[1]) out[4] = v[1];
			if (out[2] > v[2]) out[2] = v[2];
			else if (out[5] < v[2]) out[5] = v[2];
		} while (--n != 0);
	}

	// 0x6ED1E0 (Moomba's range: Different Beat calls it): a yaw rotation (by -angle) scaled by `scale`: x / z rows (cos, -sin
	// / sin, cos) x scale, m11 = scale
	static void ScaledRotY(int32_t angle, int32_t scale, Mat4x3 *out)
	{
		memset(out, 0, 0x14);
		const int32_t a = -(int32_t)(int16_t)angle;
		const int32_t r = (int16_t)scale;
		const int32_t s = mul32(ComputeSin(a), r) >> 12;
		const int32_t c = mul32(ComputeCos(a), r) >> 12;
		out->m[2][0] = (int16_t)s;
		out->m[1][1] = (int16_t)scale;
		out->m[0][0] = (int16_t)c;
		out->m[0][2] = (int16_t)-s;
		out->m[2][2] = (int16_t)c;
	}

	// 0x69A670: the model's vertices (+8, 8 bytes) twisted about Z by an angle growing with the height: angle = y / 4 + `angle`,
	// y scaled by `scale` first; z kept (the out vertices' 4th word untouched)
	static void TwistVertices(uint32_t model, uint8_t *out, int32_t scale, int32_t angle)
	{
		const int16_t *v = (const int16_t *)(model + 8);
		int32_t n = *(const int32_t *)(model + 4);
		do
		{
			const int32_t a = (v[1] >> 2) + angle;
			const int32_t s = ComputeSin(a);
			const int32_t c = ComputeCos(a);
			const int32_t y = mul32(v[1], scale) >> 12;
			W16(out, 0) = (int16_t)((mul32(v[0], c) - mul32(y, s)) >> 12);
			W16(out, 2) = (int16_t)((mul32(v[0], s) + mul32(y, c)) >> 12);
			W16(out, 4) = v[2];
			out += 8;
			v += 4;
		} while (--n != 0);
	}

	// 0x699EE0: a full-screen tile (0x7040B0) at frame `f` of a fade in over `in` ticks, held `hold`, out over `out`
	static uint32_t FadeTile(int32_t f, int32_t r, int32_t g, int32_t b, int32_t in, int32_t hold, int32_t out, uint32_t cursor)
	{
		const int32_t din = in ? in : 1;
		const int32_t dout = out ? out : 1;
		if (f < in) return FullScreenTile(r, g, b, shl32(f + 1, 12) / din, cursor);
		if (f < in + hold) return FullScreenTile(r, g, b, 0x1000, cursor);
		return FullScreenTile(r, g, b, (shl32(in - f + hold, 12) - 0x1000) / dout + 0x1000, cursor);
	}

	// 0x695F60: the entity's model height under its pose: its world translation from its position, its bones' world matrices,
	// every vertex of its visible objects through its bone (y through the GTE, IR2): out[0] = the highest y (max), out[1] = the
	// lowest (min); its pose matrices rebuilt. Returns out[1].
	static int32_t MeasureHeight(uint8_t *e, int16_t *out)
	{
		W32(e, 0x58) = W16(e, 0x1E);
		W32(e, 0x54) = W16(e, 0x1C);
		W32(e, 0x5C) = W16(e, 0x20);
		ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
		uint8_t *h = (uint8_t *)FieldAlloc(0x18);
		U32(h, 4) = 0xFFFF0000;
		U32(h, 0) = 0x10000;
		U32(h, 0x14) = U32(e, 0x7C);
		U32(h, 8) = 0;
		U16(h, 0xC) = 0x1000;
		uint8_t *bones = **(uint8_t ***)(e + 0x64) + 0x10;
		uint8_t *model = *(uint8_t **)(e + 0x64);
		const uint8_t *geom = *(uint8_t **)(model + 4);
		const int32_t nobj = *(const int32_t *)geom;
		for (int32_t i = 0; i < nobj; i++)
		{
			const uint8_t *src = *(uint8_t **)(model + 4) + ((const uint32_t *)(geom + 4))[i];
			if (!(U32(h, 0x14) & (1u << i))) continue;
			int32_t groups = *(const int16_t *)src;
			src += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = *(const int16_t *)src;
				src += 2;
				const Mat4x3 *m = (const Mat4x3 *)(bones + 0x10 + bone * 0x30);
				GteSetRotMatrixRaw(m);
				GteSetTransVectorRaw(m);
				int32_t nv = *(const int16_t *)src;
				src += 2;
				for (; nv != 0; nv--)
				{
					GteLoadV0u(src);
					GteRTPS();
					GteReadIR2(h + 0x10);
					src += 6;
					const int32_t y = W16(h, 0x10);
					if (W32(h, 0) > y) W32(h, 0) = y;
					if (W32(h, 4) < y) W32(h, 4) = y;
				}
			}
		}
		out[0] = W16(h, 4);
		out[1] = W16(h, 0);
		FieldFree(0x18);
		BuildBoneMatricesFromPose(e + 0x60);
		return out[1];
	}

	// 0x68D1D0 (Duel's range: Different Beat calls it): an entity drawn by the module: its shadow, its bones' world matrices
	// from its world matrix x (rotation `angles`, translation `off` minus the rotated `off`, then `scale`) under the view,
	// RenderGeometry, its pose matrices rebuilt; the same for its weapon (+0x78)
	static void DrawEntity(uint8_t *e, const int16_t *angles, const int16_t *off, const int32_t *scale)
	{
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_entity(e, angles, off, scale);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x5C);
		Mat4x3 *m = (Mat4x3 *)h;
		FramePackets() = DrawShadow(e, OTBase() + 0x4040, 0x10, FramePackets());
		ComposeZYXRotationMatrix(angles, m);
		m->t[0] = off[0];
		m->t[1] = off[1];
		m->t[2] = off[2];
		ComposeAffineTransform((const Mat4x3 *)(e + 0x40), m, m);
		Scale3DMatrix(m, scale);
		ComposeAffineTransform(&Camera(), m, m);
		Apply3DTransform(m, off, (int32_t *)(h + 0x20));
		m->t[1] = (int32_t)((uint32_t)m->t[1] - U32(h, 0x24));
		m->t[0] = (int32_t)((uint32_t)m->t[0] - U32(h, 0x20));
		m->t[2] = (int32_t)((uint32_t)m->t[2] - U32(h, 0x28));
		ComputeBonesWorldMatrices(e + 0x60, m);
		U32(h, 0x34) = var<uint32_t>(0x1D98B3C);
		U16(h, 0x44) = 0;
		U16(h, 0x46) = 0;
		U16(h, 0x48) = 0x140;
		U16(h, 0x4A) = 0xD8;
		U32(h, 0x4C) = U32(e, 0x28);
		U32(h, 0x58) = 0;
		U32(h, 0x50) = U32(e, 0x7C);
		U32(h, 0x40) = var<uint32_t>(0x1D969A8);
		FramePackets() = RenderGeometry(U32(e, 0x64), h + 0x30, OTBase() + 0x44, 4, FramePackets());
		BuildBoneMatricesFromPose(e + 0x60);
		uint8_t *w = *(uint8_t **)(e + 0x78);
		if (w)
		{
			U32(h, 0x50) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(w, m);
			FramePackets() = RenderGeometry(U32(w, 4), h + 0x30, OTBase() + 0x44, 4, FramePackets());
			BuildBoneMatricesFromPose(w);
		}
		FieldFree(0x5C);
	}

	// every entity of the target's chain (+0x8C, from the target, until it comes back or ends)
	template<typename F> static void ForChain(F f)
	{
		uint8_t *e = Target();
		do
		{
			f(e);
			e = *(uint8_t **)(e + 0x8C);
		} while (e != Target() && e != nullptr);
	}

	// 0x69A790: the stage captured into VRAM (0x280, 0x180, 0x80 x 0x80): the screen flash off; a top view of the stage at
	// `pos` (x, z) over the box `b` (x0 = b[0], x1 = b[3], z0 = b[2], z1 = b[5]) scaled into the rectangle (the draw environment
	// switched to it, the stage geometry 0x1D989C0 when present, a black quad, the draw mask), the screen offset put back,
	// then the drawing area / offset packets of the rectangle (OT + 0x44)
	static void CaptureStage(const int16_t *pos, const int16_t *b)
	{
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_capture(pos, b);)
		const int32_t b0 = -(int32_t)b[0];
		const int32_t b3 = b[3];
		const int32_t w = b3 + b0;
		const int32_t nb2 = -(int32_t)b[2];
		const int32_t b5 = b[5];
		const int32_t d = b5 + nb2;
		SetScreenFlash(0, 0);
		int16_t rect[4] = { 0x280, 0x180, 0x80, 0x80 };
		// (the round-up tests the quotient's remainder, not the extent's)
		int32_t sx = sdiv_pow2(w, 7);
		if (sx % 0x80 != 0) sx++;
		int32_t sz = sdiv_pow2(d, 7);
		if (sz % 0x80 != 0) sz++;
		Mat4x3 m, m2;
		m.t[1] = 0;
		m.t[0] = -((int32_t)pos[0] / sx);
		m.t[2] = (int32_t)pos[2] / sz;
		RotationY(0, &m);
		TransformVectorBy3x3(&m, m.t, m.t);
		RotationX(-0x400, &m2);
		MatrixMultiply3x3(&m2, &m);
		int32_t s3[3];
		s3[1] = 0;
		m2.t[0] = (b0 - b3) / 2 / sx + m.t[0];
		m2.t[2] = (int32_t)var<int16_t>(0x1D8E038);
		m2.t[1] = m.t[2] - ((nb2 - b5) / 2 / sz);
		s3[0] = 0x1000000 / (shl32(w, 12) / rect[2]);
		s3[2] = 0x1000000 / (shl32(d, 12) / rect[3]);
		Scale3DMatrix(&m2, s3);
		uint8_t *env = (uint8_t *)PacketCursor();
		PacketCursor() += 0x40;
		SetDrawEnv(env, 0x1D969C8 + (uint32_t)(var<uint8_t>(0x1D96A80) ^ 1) * 0x5C);
		InsertPrimAutoDepth(OTBase() + 0x44, env);
		int32_t ox, oy;
		GetScreenOffset(&ox, &oy);
		SetScreenOffset(rect[2] / 2, rect[3] / 2);
		GteSetRotMatrix(&m2);
		GteSetTransVector(&m2);
		if (var<uint8_t>(0x1D989BD) & 1)
		{
			uint8_t *h = (uint8_t *)FieldAlloc(0x2C);
			U16(h, 0x14) = 0;
			U16(h, 0x16) = 0;
			U16(h, 0x18) = 0x140;
			U16(h, 0x1A) = 0xD8;
			U16(h, 0x24) = var<uint16_t>(0x1D989BE);
			U32(h, 0x1C) = var<uint32_t>(0x1D989E4);
			U32(h, 4) = var<uint32_t>(0x1D98B3C);
			U32(h, 0x20) = 0xFFFFFFFF;
			U32(h, 0x28) = 0;
			U32(h, 0x10) = CaptureCursor();
			BuildBoneMatricesFromPose((void *)0x1D989D0);
			ComputeBonesWorldMatrices((void *)0x1D989D0, &m2);
			PacketCursor() = RenderGeometry(0x1D989C0, h, OTBase() + 0x44, 0x10, PacketCursor());
			FieldFree(0x2C);
		}
		uint8_t *q = (uint8_t *)PacketCursor();
		PacketCursor() += 0x18;
		U16(q, 8) = 0;
		U16(q, 0xA) = 0;
		U16(q, 0xC) = 0x80;
		U16(q, 0xE) = 0;
		U16(q, 0x10) = 0;
		U16(q, 0x12) = 0x80;
		U16(q, 0x14) = 0x80;
		U16(q, 0x16) = 0x80;
		q[4] = 0;
		q[5] = 0;
		q[6] = 0;
		U32(q, 0) = 0x05000000;
		q[7] = 0x28;
		InsertPrimAltViewport(OTBase() + 0x44, q);
		uint8_t *stp = (uint8_t *)PacketCursor();
		PacketCursor() += 0xC;
		SetDrawStp(stp, 1);
		InsertPrimAutoDepth(OTBase() + 0x44, stp);
		SetScreenOffset(ox, oy);
		uint8_t *area = (uint8_t *)PacketCursor();
		PacketCursor() += 0xC;
		SetDrawAreaPrim(area, rect);
		InsertPrimAutoDepth(OTBase() + 0x44, area);
		int16_t xy[2] = { rect[0], rect[1] };
		uint8_t *offp = (uint8_t *)PacketCursor();
		PacketCursor() += 0xC;
		SetDrawOffsetPrim(offp, xy);
		InsertPrimAutoDepth(OTBase() + 0x44, offp);
	}

	// ------------------------------------------------------------------
	// Rings, rocks, sprites (DRAW THEN UPDATE)
	// ------------------------------------------------------------------
	// RingTask (0x69A5D0): node 0x104, the layout 0x115049C at its point turned / scaled about Y; ends with its layout
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *k = (RingNode *)n;
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_ring(k);)
		PlayArg arg;
		ScaledRotY(k->angle, k->scale, &arg.m);
		arg.m.t[0] = k->pos[0];
		arg.m.t[1] = k->pos[1];
		arg.m.t[2] = k->pos[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.morph = TexBase() + MORPH;
		arg.depth = -0x80;
		arg.fade = 0;
		arg.kind = (const uint8_t *)KIND_Ring;
		arg.scroll = 0; // (never written by the original: its kind table has no texture window)
		arg.pad2A = 0;
		return prim::play((prim::Layout *)k->layout, Callback, (int)&arg, 0) != 0 ? 0 : 2;
	}

	// 0x69A550: a ring at Zell's position + (x, y, z), Y scale `scale`
	static void SpawnRing(int32_t x, int32_t y, int32_t z, int32_t scale)
	{
		RingNode *k = (RingNode *)AddTaskToQueue(QRings(), ORIG_RingTask);
		if (!k) return;
		k->scale = (int16_t)scale;
		k->pos[0] = (int16_t)(W16(Caster(), 0x1C) + x);
		k->pos[1] = (int16_t)(W16(Caster(), 0x1E) + y);
		k->angle = 0;
		k->pos[2] = (int16_t)(W16(Caster(), 0x20) + z);
		DecodeModelPrimLayout(LAYOUT_Ring, k->layout, 0xEC);
	}

	// 0x69A410: a rock (the model 0x1140AA4) at its position, angles x / y (x 16), scale; life--: ends at 0, else it moves
	// (angles by its spin, position by its velocity, velocity by its acceleration)
	static void RockDrawAt(const int16_t *pos, int32_t ax, int32_t ay, int16_t scale)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x98);
		Mat4x3 *mb = (Mat4x3 *)(h + 0x78), *ma = (Mat4x3 *)(h + 0x58);
		ScaledRotY(ay, scale, mb);
		RotationX(ax, ma);
		MatrixMultiply3x3(ma, mb);
		ma->t[0] = pos[0];
		ma->t[1] = pos[1];
		ma->t[2] = pos[2];
		ComposeAffineTransform(&Camera(), ma, ma);
		GteSetRotMatrix(ma);
		GteSetTransVector(ma);
		U32(h, 0x1C) = 0;
		U32(h, 0) = MODEL_Rock;
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x98);
	}
	static void RockDraw(const RockNode *k) { RockDrawAt(k->pos, (int32_t)(int8_t)k->rx << 4, (int32_t)(int8_t)k->ry << 4, k->scale); }
	static void RockMove(RockNode *k)
	{
		k->rx = (uint8_t)(k->rx + k->spin[0]);
		k->pos[0] = (int16_t)(k->pos[0] + k->vel[0]);
		const int16_t vx = (int16_t)(k->acc[0] + k->vel[0]);
		k->ry = (uint8_t)(k->ry + k->spin[1]);
		k->pos[1] = (int16_t)(k->pos[1] + k->vel[1]);
		k->pos[2] = (int16_t)(k->pos[2] + k->vel[2]);
		const int16_t vy = (int16_t)(k->acc[1] + k->vel[1]);
		k->vel[0] = vx;
		const int16_t vz = (int16_t)(k->acc[2] + k->vel[2]);
		k->vel[1] = vy;
		k->vel[2] = vz;
	}
	static uint32_t RockStep(RockNode *k)
	{
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_rock(k);)
		RockDraw(k);
		if (--k->life == 0) return 2;
		RockMove(k);
		return 0;
	}
	// 0x69A3F0 / 0x69A530: the two rock tasks (the same 0x69A410 with the model 0x1140AA4)
	static uint32_t __cdecl RockATask(TaskNode *n)
	{
		return RockStep((RockNode *)n);
	}
	static uint32_t __cdecl RockBTask(TaskNode *n)
	{
		return RockStep((RockNode *)n);
	}

	// 0x69A2D0: n + 1 rocks (0x69A3F0 or 0x69A530 at random) around (x, y, z): distance rand x d0 / 0x8000 + d1 at a random
	// angle, life rand x l0 / 0x8000 + l1, scale rand x s0 / 0x8000 + s1, velocity: out by distance x v / 0x4000, up by
	// distance / 4; random angles / spins; gravity 0x14
	static void SpawnRocks(int32_t n, int32_t x, int32_t y, int32_t z, int32_t l0, int32_t l1, int32_t d0, int32_t d1, int32_t s0, int32_t s1)
	{
		for (;;)
		{
			const uint32_t task = (CrtRand() & 1) ? ORIG_RockATask : ORIG_RockBTask;
			RockNode *k = (RockNode *)AddTaskToQueue(QRocks(), task);
			if (!k) return;
			U32(k, 0x18) = 0;
			k->acc[1] = 0x14;
			k->scale = (int16_t)((mul32(CrtRand(), s0) >> 15) + s1);
			int32_t dist = (mul32(CrtRand(), d0) >> 15) + d1;
			const int32_t a = CrtRand();
			int32_t sx = mul32(ComputeSin(a), dist) >> 12;
			int32_t cz = mul32(ComputeCos(a), dist) >> 12;
			k->pos[0] = (int16_t)(x + sx);
			k->pos[1] = (int16_t)y;
			k->pos[2] = (int16_t)(cz + z);
			k->life = (uint8_t)((uint8_t)(mul32(CrtRand(), l0) >> 15) + (uint8_t)l1);
			sx = mul32(sx, d1) >> 14;
			cz = mul32(cz, d1) >> 14;
			dist = -dist >> 2;
			k->vel[0] = (int16_t)sx;
			k->vel[2] = (int16_t)cz;
			k->vel[1] = (int16_t)dist;
			k->rx = (uint8_t)CrtRand();
			k->ry = (uint8_t)CrtRand();
			k->spin[0] = (int8_t)CrtRand();
			k->spin[1] = (int8_t)CrtRand();
			if (n-- == 0) return;
		}
	}

	// a sprite's flipbook frame at its position (the shadow-rotation scratch, 0x571C80)
	static void SpriteDraw(const SpriteNode *s, uint32_t seq)
	{
		TransformCameraByShadowRotation(s->pos, s->size, -(s->size >> 2));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		U32(h, 0) = seq;
		W16(h, 4) = (int8_t)s->frame;
		U16(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0xB4);
	}
	// SparkTask (0x69A060): the flipbook 0x113C1AC; position += velocity x 4; 0xC frames
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_sprite(s, SEQ_Spark);)
		SpriteDraw(s, SEQ_Spark);
		s->pos[0] = (int16_t)(s->pos[0] + (int16_t)(s->vel[0] << 2));
		s->pos[1] = (int16_t)(s->pos[1] + (int16_t)(s->vel[1] << 2));
		s->pos[2] = (int16_t)(s->pos[2] + (int16_t)(s->vel[2] << 2));
		s->frame++;
		return (int8_t)s->frame < 0xC ? 0 : 2;
	}
	// EmberTask (0x69A200): the flipbook 0x113C2F0; position += velocity x 4, velocity += acceleration; 8 frames
	static uint32_t __cdecl EmberTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_sprite(s, SEQ_Ember);)
		SpriteDraw(s, SEQ_Ember);
		s->pos[0] = (int16_t)(s->pos[0] + (int16_t)(s->vel[0] << 2));
		s->pos[1] = (int16_t)(s->pos[1] + (int16_t)(s->vel[1] << 2));
		s->pos[2] = (int16_t)(s->pos[2] + (int16_t)(s->vel[2] << 2));
		s->vel[0] = (int8_t)(s->acc[0] + s->vel[0]);
		s->vel[1] = (int8_t)(s->acc[1] + s->vel[1]);
		s->vel[2] = (int8_t)(s->acc[2] + s->vel[2]);
		s->frame++;
		return (int8_t)s->frame < 8 ? 0 : 2;
	}

	// 0x699F80: n + 1 sparks at (x, y, z) + a random offset (distance rand x d0 / 0x8000 + d1 at a random angle), size rand x
	// s0 / 0x8000 + s1, flying out by the offset x d1 / 0x10000 a tick
	static void SpawnSparks(int32_t n, int32_t x, int32_t y, int32_t z, int32_t d0, int32_t d1, int32_t s0, int32_t s1)
	{
		for (;;)
		{
			SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SparkTask);
			if (!s) return;
			s->size = (int16_t)((mul32(CrtRand(), s0) >> 15) + s1);
			const int32_t dist = (mul32(CrtRand(), d0) >> 15) + d1;
			const int32_t a = CrtRand();
			const int32_t sx = mul32(ComputeSin(a), dist) >> 12;
			const int32_t cz = mul32(ComputeCos(a), dist) >> 12;
			s->pos[0] = (int16_t)(x + sx);
			s->pos[1] = (int16_t)y;
			s->frame = 0;
			s->pos[2] = (int16_t)(cz + z);
			s->vel[2] = (int8_t)(mul32(cz, d1) >> 16);
			s->vel[0] = (int8_t)(mul32(sx, d1) >> 16);
			s->vel[1] = 0;
			if (n-- == 0) return;
		}
	}

	// 0x69A110: n + 1 embers at Zell's position + (x, y, z), size 0x800, random velocity bytes (x: rand + 0x80, y: 0x80 - rand,
	// z: rand + 0x80), each slowed by an eighth a tick
	static void SpawnEmbers(int32_t n, int32_t x, int32_t y, int32_t z)
	{
		for (;;)
		{
			SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_EmberTask);
			if (!s) return;
			s->size = 0x800;
			s->pos[0] = (int16_t)(W16(Caster(), 0x1C) + x);
			s->pos[1] = (int16_t)(W16(Caster(), 0x1E) + y);
			s->frame = 0;
			s->pos[2] = (int16_t)(W16(Caster(), 0x20) + z);
			s->vel[0] = (int8_t)((uint8_t)CrtRand() + 0x80);
			s->vel[1] = (int8_t)(0x80 - (uint8_t)CrtRand());
			const uint8_t vz = (uint8_t)((uint8_t)CrtRand() + 0x80);
			s->vel[2] = (int8_t)vz;
			s->acc[0] = (int8_t)-sdiv_pow2(s->vel[0], 3);
			s->acc[1] = (int8_t)-sdiv_pow2(s->vel[1], 3);
			s->acc[2] = (int8_t)-sdiv_pow2((int8_t)vz, 3);
			if (n-- == 0) return;
		}
	}

	// ------------------------------------------------------------------
	// DirectorTask (0x6960E0): node 0x17F0, counters 0..0x8A (the counter steps last)
	// ------------------------------------------------------------------
	// one layout of the director through the prim player (0x701970) with the callback 0x6978F0 (`frame`: the block's frame
	// without the view, for the 30 fps layer)
	static void Play(int site, uint8_t *layout, PlayArg *arg, const Mat4x3 *frame)
	{
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_play(site, layout, arg, frame);)
		(void)frame;
		prim::play((prim::Layout *)layout, Callback, (int)arg, 0);
	}

	// the twisted model 0x113C3CC at Zell's bone 0x14 (yaw 0x400, scale 0x800): its vertices twisted (0x69A670) by `scale` /
	// `angle` into `morph` (the magic buffer + 0x2CD8), drawn by the engine's 0x701DD0 (the last step: flags 0x20F0, fade
	// 0x800). `point`: the bone point to use (a held frame's), else measured (0x502170)
	static void DrawTwist(int32_t scale, int32_t angle, bool last, const int16_t *point, uint32_t morph)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x94);
		Mat4x3 *m = (Mat4x3 *)(h + 0x6C);
		ScaledRotY(0x400, 0x800, m);
		int16_t p[4];
		if (point) memcpy(h + 0x8C, point, 6);
		else GetEffectSpawnPosition(Caster(), 0x14, 0, (int16_t *)(h + 0x8C));
		memcpy(p, h + 0x8C, 6);
		m->t[0] = p[0];
		m->t[1] = p[1];
		m->t[2] = p[2];
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		U32(h, 0) = MODEL_Twist;
		U32(h, 4) = morph;
		TwistVertices(MODEL_Twist, (uint8_t *)morph, scale, angle);
		U32(h, 0x1C) = 0x2030;
		if (last)
		{
			U32(h, 0x1C) = 0x20F0;
			U32(h, 0xC) = 0x800;
			U32(h, 8) = 0;
		}
		W32(h, 0x18) = -0x80;
		U16(h, 0x22) = 0;
		U16(h, 0x20) = 0;
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_twist(p, scale, angle, last);)
		PacketCursor() = RenderPrimSet(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x94);
	}

	// every entity of the target's chain present (bit 1): drawn by the module (0x68D1D0) and hidden from the engine's draw
	static void DrawTargets(const int16_t *angles, const int16_t *off, const int32_t *scale)
	{
		ForChain([&](uint8_t *e)
		{
			if (B8(e, 0) & 2)
			{
				DrawEntity(e, angles, off, scale);
				B8(e, 0) |= 4;
			}
		});
	}

	// the flash level of the tick: (c x 0xA8C) / 12 up 0..0xB, 0xA8C, ((0x8B - c) x 0xA8C) / 12 from 0x7F
	static int32_t FlashLevel(int32_t c)
	{
		if (c < 0xC) return mul32(c, 2700) / 12;
		if (c >= 0x7F) return mul32(0x8B - c, 2700) / 12;
		return 0xA8C;
	}
	// the second envelope 0x1D..0x7C: v = (h x 0x1000) / 12 + 0xAA up 0..0xB, 0x1000, down from 0x54; level = v x 1396 / 0x1000 +
	// 0xA8C
	static int32_t FlashLevel2(int32_t h)
	{
		int32_t v;
		if ((uint32_t)h < 0xC) v = (int32_t)(((uint32_t)h << 12) / 12) + 0xAA;
		else if ((uint32_t)h >= 0x54) v = (int32_t)((uint32_t)(0x5F000 - ((uint32_t)h << 12)) / 12) + 0xAA;
		else v = 0x1000;
		return (mul32(v, 1396) >> 12) + 0xA8C;
	}

	// the camera words: look-at / eye set directly
	static void CamSet(int16_t ax, int16_t ay, int16_t az, int16_t ex, int16_t ey, int16_t ez)
	{
		CamLookAt()[0] = ax;
		CamLookAt()[1] = ay;
		CamLookAt()[2] = az;
		CamEye()[0] = ex;
		CamEye()[1] = ey;
		CamEye()[2] = ez;
	}
	// the camera of counter c (sets on 0 / 8 / 0x10 / 0x70, CamLerp 0x16..0x45, 0x46..0x65, 0x66..0x6F); `engine`: the
	// return armed on 0 (a held frame's run passes false)
	static void DirectorCamera(int32_t c, bool engine)
	{
		if (c == 0)
		{
			if (engine) CameraArmReturn();
			ViewWord() = 0x120;
			CamRoll() = 0;
			const int16_t x = (int16_t)CamAnchor()[0], z = (int16_t)CamAnchor()[1];
			CamSet((int16_t)(x - 0x88), (int16_t)0xFDE2, (int16_t)(z - 0xD0), (int16_t)(x - 0x270), (int16_t)0xFD50, (int16_t)(z - 0x750));
		}
		const int16_t px = (int16_t)Anchor()[0], py = (int16_t)(Anchor()[0] >> 16), pz = (int16_t)Anchor()[1];
		const int16_t hy = (int16_t)Height();
		if (c == 8) CamSet((int16_t)(px - 0x2D), (int16_t)0xFE1D, (int16_t)(pz + 0x1CA), (int16_t)(px + 0x3EB), (int16_t)0xFDB7, (int16_t)(pz + 0x9D3));
		if (c == 0x10) CamSet((int16_t)(px - 0x62), (int16_t)0xFC86, (int16_t)(pz - 0x99), (int16_t)(px + 0x9CC), (int16_t)0xFADD, (int16_t)(pz + 0x1B1));
		int32_t s = c - 0x16;
		if ((uint32_t)s < 0x30)
		{
			const int16_t a[6] = { (int16_t)(px + 0xC1), (int16_t)(py - 0xC2F), (int16_t)(pz - 0x191), (int16_t)(px + 0x3D2), (int16_t)(py - 0x988), (int16_t)(pz - 0xF9) };
			const int16_t b[6] = { (int16_t)(px + 0x1D), (int16_t)(py - 0x2414), (int16_t)(pz - 0x292), (int16_t)(px + 0xBB3), (int16_t)(py - 0x131C), (int16_t)(pz + 0x3F) };
			CamLerp(a, b, (int32_t)(((uint32_t)s << 12) / 47));
		}
		s = c - 0x46;
		if ((uint32_t)s < 0x20)
		{
			const int16_t a[6] = { (int16_t)(px - 0x19), (int16_t)(hy - 0x3B8A), (int16_t)(pz + 0x13D4), (int16_t)(px - 0x81), (int16_t)(hy - 0x3AC6), (int16_t)(pz + 0x1BC0) };
			const int16_t b[6] = { (int16_t)(px + 0x42), (int16_t)(hy - 0x391D), (int16_t)(pz + 0x11CF), (int16_t)(px + 0x1512), (int16_t)(hy - 0x3848), (int16_t)(pz + 0x12AC) };
			CamLerp(a, b, (int32_t)(((uint32_t)s << 12) / 31));
		}
		s = c - 0x66;
		if ((uint32_t)s < 0xA)
		{
			const int16_t a[6] = { (int16_t)(px + 0x42), (int16_t)(hy - 0x391D), (int16_t)(pz + 0x11CF), (int16_t)(px + 0x1512), (int16_t)(hy - 0x3848), (int16_t)(pz + 0x12AC) };
			const int16_t b[6] = { (int16_t)(px + 0x12), (int16_t)(hy - 0xAB6), (int16_t)(pz - 0x27), (int16_t)(px + 0x381), (int16_t)(hy - 0x16C3), (int16_t)(pz + 0x1164) };
			CamLerp(a, b, (int32_t)(((uint32_t)s << 12) / 9));
		}
		if (c == 0x70) CamSet((int16_t)(px + 0x211), (int16_t)(py - 0x50C), (int16_t)(pz - 0x2CA), (int16_t)(px + 0x905), (int16_t)(py - 0x7C0), (int16_t)(pz - 0xA44));
	}

	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_director(d);)
		uint8_t *z = Caster();
		const int32_t c = d->counter;
		Flash() = FlashLevel(c);
		PlayArg arg;
		memset(&arg, 0, sizeof(arg));
		Mat4x3 frame;
		const uint32_t morph = TexBase() + MORPH;
		// 9..0x16: a layout at Zell's anchor (0x1140C5C), its texture scrolling 0x20 a tick; 9: Zell placed before the target
		int32_t s = c - 9;
		if ((uint32_t)s < 0xE)
		{
			if (s == 0)
			{
				W16(z, 0xE) = 0;
				U32(z, 0x1C) = *(const uint32_t *)&d->place[0];
				U32(z, 0x20) = *(const uint32_t *)&d->place[2];
				DecodeModelPrimLayout(LAYOUT_Rise, d->layout_a, 0x40);
				d->lift = 0xC40;
				d->ghosts[3] = 0;
				d->ghosts[2] = 0;
				d->ghosts[1] = 0;
				d->ghosts[0] = 0;
			}
			RotationY(0, &arg.m);
			int16_t p[4];
			GetDefaultEffectPosition(z, p);
			arg.m.t[0] = p[0];
			arg.m.t[1] = 0;
			arg.m.t[2] = p[2];
			frame = arg.m;
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			arg.morph = morph;
			arg.depth = 0;
			arg.fade = 0;
			arg.kind = (const uint8_t *)KIND_Rise;
			arg.scroll = (int16_t)(s << 5);
			Play(SITE_RISE, d->layout_a, &arg, &frame);
			// 0x11..0x15: the twisted model at his bone 0x14
			if ((uint32_t)s >= 8 && (uint32_t)s < 0xD)
			{
				const int32_t t = s - 8;
				DrawTwist(shl32(t, 12) / 5 / 2 + 0x800, 0x400 - shl32(t, 10), s == 0xC, nullptr, TexBase() + MORPH);
			}
			// 0x12: a ring and embers at Zell; 0x12..0x16 the target thrown up
			if (s == 9)
			{
				SpawnRing(0, -0x1F4, -0x3E8, 0x1000);
				SpawnEmbers(0x10, 0, -0x1F4, -0x3E8);
			}
			if ((uint32_t)s >= 9) W16(Target(), 0x1E) = (int16_t)(W16(Target(), 0x1E) - 0x280);
		}
		// 0x17..0x46: Zell rising after the target (the target flagged at 0x17), every sixth tick a ring / embers, the target's
		// chain unflagged and its transformation 4; four of every six ticks the twisted model; the target swinging (its x angle
		// flips every six ticks), drawn by the module
		s = c - 0x17;
		if ((uint32_t)s < 0x30)
		{
			const int32_t r = (int32_t)((uint32_t)s % 6);
			if (s == 0)
			{
				W16(z, 0x1E) = (int16_t)(W16(z, 0x1E) - 0xA00);
				ForChain([](uint8_t *e) { if (B8(e, 0) & 2) B8(e, 0) |= 8; });
			}
			W16(z, 0x1E) = (int16_t)(W16(z, 0x1E) - 0xD0);
			if (r >= 2 && r <= 4) W16(Target(), 0x1E) = (int16_t)(W16(Target(), 0x1E) - 0x1A0);
			bool twist = r <= 3;
			if (r == 0)
			{
				SpawnRing(0, -0x1F4, -0x3E8, 0x1000);
				SpawnEmbers(0x10, 0, -0x1F4, -0x3E8);
				ForChain([](uint8_t *e) { if (B8(e, 0) & 2) U16(e, 0) &= 0xFFF7; });
				QueueChainTransformation(Target(), 4);
				twist = true;
			}
			if (twist) DrawTwist(shl32(r, 12) / 3 / 2 + 0x800, 0x400 - shl32(r, 10), r == 3, nullptr, TexBase() + MORPH);
			if (((uint32_t)s / 6) & 1) d->angles[0] = (int16_t)-d->angles[0];
			const int32_t one[3] = { 0x1000, 0x1000, 0x1000 };
			DrawTargets(ZeroWords(), ZeroWords(), one);
		}
		// 0x5D..0x66: a layout at Zell's bone 0x14 (0x11431D4, tilted -0x200 about X); 0x5D: the target's chain shown to the
		// engine (bit 8, not 4), its model height measured again; 0x65 / 0x66 Zell moves
		s = c - 0x5D;
		if ((uint32_t)s < 0xA)
		{
			if (s == 0)
			{
				DecodeModelPrimLayout(LAYOUT_Fly, d->layout_a, 0x484);
				ForChain([](uint8_t *e) { if (B8(e, 0) & 2) U16(e, 0) = (uint16_t)((U16(e, 0) & 0xFFFB) | 8); });
				MeasureHeight(Target(), d->height);
				d->height[0] = (int16_t)(d->height[0] - W16(Target(), 0x1E));
				d->height[1] = (int16_t)(d->height[1] - W16(Target(), 0x1E));
			}
			RotationX(-0x200, &arg.m);
			int16_t p[4];
			GetEffectSpawnPosition(z, 0x14, 0, p);
			arg.m.t[0] = p[0];
			arg.m.t[1] = p[1];
			arg.m.t[2] = p[2];
			frame = arg.m;
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			arg.morph = morph;
			arg.kind = (const uint8_t *)KIND_Fly;
			arg.scroll = (int16_t)(s << 5);
			arg.depth = -0x80;
			arg.fade = 0;
			Play(SITE_FLY, d->layout_a, &arg, &frame);
			if ((uint32_t)s >= 8)
			{
				W16(z, 0x1E) = (int16_t)(W16(z, 0x1E) + 0x200);
				W16(z, 0x20) = (int16_t)(W16(z, 0x20) - 0x200);
			}
		}
		// 0x47..0x66: the target swaying in the air: 0x47 Zell / the target placed, the pillar height, the ring queue cleared;
		// then the pillar rises (cos), the target bobs (sin) and tilts (sin / 16), drawn by the module about its middle; 0x67:
		// the target's chain shown
		s = c - 0x47;
		if ((uint32_t)s < 0x16 && s == 0)
		{
			W16(z, 0x1E) = (int16_t)(W16(z, 0x1E) + (int16_t)(d->height[1] - d->height[0] - 0x600));
			W16(z, 0x20) = (int16_t)(W16(z, 0x20) + (int16_t)(0x1788 - (W16(z, 0x26) >> 1)));
			W16(Target(), 0x20) = (int16_t)(W16(Target(), 0x20) + 0x1388);
			d->angles[0] = 0;
			d->lift = (int16_t)(Height() - 0x1055);
			d->sway_y = (int16_t)(W16(Target(), 0x1E) + 0x600);
			QRings()->tail = nullptr;
			QRings()->head = nullptr;
		}
		if ((uint32_t)s < 0x20)
		{
			const int32_t a = (int32_t)(((uint32_t)s << 12) >> 5);
			d->lift = (int16_t)(d->lift + (int16_t)(ComputeCos(a >> 2) / 12));
			W16(Target(), 0x1E) = (int16_t)(d->sway_y - (int16_t)(ComputeSin(a >> 2) >> 1));
			d->angles[0] = (int16_t)(ComputeSin(a * 2) >> 4);
			const int16_t off[3] = { 0, (int16_t)((d->height[1] + d->height[0]) >> 1), 0 };
			const int32_t one[3] = { 0x1000, 0x1000, 0x1000 };
			DrawTargets(d->angles, off, one);
		}
		else if (s == 0x20)
		{
			ForChain([](uint8_t *e) { if (B8(e, 0) & 2) U16(e, 0) &= 0xFFFB; });
		}
		// 0x67..0x78: two layouts at Zell's bone 0x14 (0x11431D4 and 0x11484D0, tilted -0x200 about X; from 0x71 -0x400 and
		// fading); 0x67..0x70 Zell and the target drop; 0x70: the camera's anchor z follows the place
		s = c - 0x67;
		if ((uint32_t)s < 0x12)
		{
			if (s == 0) DecodeModelPrimLayout(LAYOUT_Drop, d->layout_b, 0x160);
			if ((uint32_t)s < 0xA)
			{
				RotationX(-0x200, &arg.m);
				arg.fade = 0;
			}
			else
			{
				RotationX(-0x400, &arg.m);
				arg.fade = (int16_t)((s - 0xA) << 9);
			}
			int16_t p[4];
			GetEffectSpawnPosition(z, 0x14, 0, p);
			arg.m.t[0] = p[0];
			arg.m.t[1] = p[1];
			arg.m.t[2] = p[2];
			frame = arg.m;
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			arg.morph = morph;
			arg.kind = (const uint8_t *)KIND_Fly;
			arg.scroll = (int16_t)(s << 5);
			arg.depth = -0x80;
			Play(SITE_DROP_A, d->layout_a, &arg, &frame);
			Play(SITE_DROP_B, d->layout_b, &arg, &frame);
			if ((uint32_t)s < 0xA)
			{
				W16(z, 0x1E) = (int16_t)(W16(z, 0x1E) + 0x4E6);
				W16(Target(), 0x1E) = (int16_t)(W16(Target(), 0x1E) + 0x4E6);
				W16(z, 0x20) = (int16_t)(W16(z, 0x20) - 0x1F4);
				W16(Target(), 0x20) = (int16_t)(W16(Target(), 0x20) - 0x1F4);
			}
			if (s == 9) W16((void *)Anchor(), 4) = d->anchor[2];
		}
		// 0x71..0x89: the impact: 0x71 the layouts decoded, the target back at its height, Zell back at the anchor (his angles);
		// 0x72 / 0x73 a white tile; 0x71..0x74 rocks and sparks around the anchor; the target squashed (y scale) and drawn by
		// the module; 0x71..0x7C a layout at Zell (0x1141444); a layout at the anchor (0x114A6DC, scaled); 0x7E..0x82 Zell lands
		// behind the anchor, 0x83 at his saved place; 0x8A the damage
		s = c - 0x71;
		if ((uint32_t)s < 0x19)
		{
			const int32_t e24 = (int32_t)(((uint32_t)s << 12) / 24);
			if (s == 0)
			{
				DecodeModelPrimLayout(LAYOUT_Impact, d->layout_c, 0xA8);
				DecodeModelPrimLayout(LAYOUT_Crater, d->layout_d, 0x684);
				W16(Target(), 0x1E) = (int16_t)(W16(&d->target_pos[0], 2) - d->height[0]);
				U32(z, 0xC) = d->saved_rot[0];
				U32(z, 0x10) = d->saved_rot[1];
				W16(z, 0x1E) = (int16_t)(d->height[1] - d->height[0]);
				W16(z, 0x1C) = d->anchor[0];
				W16(z, 0x20) = d->anchor[2];
			}
			else if ((uint32_t)s - 1 <= 1)
			{
				// 30 fps layer: see mag216_different_beat_held.inc
				FX_HELD(held_note_tile(s - 1);)
				PacketCursor() = FadeTile(s - 1, 0x80, 0x80, 0x80, 0, 1, 1, PacketCursor());
			}
			if ((uint32_t)s < 4)
			{
				const int32_t sc = d->scale >> 1;
				SpawnRocks(0x10, d->anchor[0], 0, d->anchor[2], 5, 0xF, sc, sc, 0x3E8, 0x3E8);
				const int32_t y = -(CrtRand() & 0x3FF);
				SpawnSparks(8, d->anchor[0], y, d->anchor[2], sc, sc, 0x7D0, 0x7D0);
			}
			int32_t v = 0x1000 - ComputeSin(e24 >> 1);
			v = mul32(v, v) >> 12;
			v = mul32(v, v) >> 12;
			v = mul32(v, v) >> 12;
			const int32_t k = (v >> 2) + (v >> 1) + 0x400;
			W16(Target(), 0x1E) = (int16_t)(W16(&d->target_pos[0], 2) - (int16_t)(mul32(d->height[0], k) / 0x1000));
			{
				const int32_t sq[3] = { 0x1000, k, 0x1000 };
				DrawTargets(ZeroWords(), ZeroWords(), sq);
			}
			if ((uint32_t)s < 0xC)
			{
				W16(z, 0x1E) = (int16_t)(mul32(d->height[1] - d->height[0], k) / 0x1000);
				RotationY(0, &arg.m);
				arg.m.t[0] = W16(z, 0x1C);
				arg.m.t[1] = W16(z, 0x1E);
				arg.m.t[2] = W16(z, 0x20) + 0x1F4;
				frame = arg.m;
				ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
				arg.morph = morph;
				arg.depth = 0;
				arg.fade = 0;
				arg.kind = (const uint8_t *)KIND_Impact;
				Play(SITE_IMPACT, d->layout_c, &arg, &frame);
			}
			memset(&arg.m, 0, sizeof(Mat4x3));
			arg.m.m[0][0] = d->scale;
			arg.m.m[1][1] = d->scale;
			arg.m.m[2][2] = d->scale;
			arg.m.t[0] = d->anchor[0];
			arg.m.t[1] = 0;
			arg.m.t[2] = d->anchor[2];
			frame = arg.m;
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			arg.morph = morph;
			arg.depth = 0;
			arg.fade = 0;
			arg.kind = (const uint8_t *)KIND_Crater;
			Play(SITE_CRATER, d->layout_d, &arg, &frame);
			if ((uint32_t)s >= 0xD && (uint32_t)s <= 0x11)
			{
				const int32_t t = 0x11 - s;
				if (s == 0xE) W16(z, 0xE) = W16(&d->saved_rot[0], 2);
				W16(z, 0x1E) = (int16_t)(W16(&d->saved_pos[0], 2) - (int16_t)(t << 8));
				const int32_t dist = (t << 8) + 0xBB8;
				const int16_t yaw = W16(&d->saved_rot[0], 2);
				W16(z, 0x1C) = (int16_t)(W16(&d->saved_pos[0], 0) - (int16_t)(mul32(ComputeSin(yaw), dist) / 0x1000));
				W16(z, 0x20) = (int16_t)(W16(&d->saved_pos[1], 0) - (int16_t)(mul32(ComputeCos(yaw), dist) / 0x1000));
			}
			else if (s == 0x12)
			{
				U32(z, 0x1C) = d->saved_pos[0];
				U32(z, 0x20) = d->saved_pos[1];
			}
		}
		else if (s == 0x19)
		{
			ForChain([](uint8_t *e) { if (B8(e, 0) & 2) U16(e, 0) &= 0xFFF3; });
			ApplyActionResultToTarget(Targets());
		}
		// 0x1D..0x7C: the second flash envelope
		s = c - 0x1D;
		if ((uint32_t)s < 0x60) Flash() = FlashLevel2(s);
		// 0x29..0x70: a camera-facing layout over the target (0x11512C8; stretched 9 / 8 in y, at the pillar's height, faded
		// in / out over 0xC ticks)
		s = c - 0x29;
		if ((uint32_t)s < 0x48)
		{
			if (s == 0) DecodeModelPrimLayout(LAYOUT_Pillar, d->layout_e, 0xEC);
			RotationY(0, &arg.m);
			arg.m.t[0] = d->place[0];
			arg.m.t[1] = d->lift - 0x3A80;
			arg.m.t[2] = d->place[2];
			frame = arg.m;
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			RotationY(0, &arg.m);
			arg.m.m[0][1] = (int16_t)(arg.m.m[0][1] + (int16_t)sdiv_pow2(arg.m.m[0][1], 3));
			arg.kind = (const uint8_t *)KIND_Impact;
			arg.depth = -0x200;
			arg.m.m[1][1] = (int16_t)(arg.m.m[1][1] + (int16_t)sdiv_pow2(arg.m.m[1][1], 3));
			arg.m.m[2][1] = (int16_t)(arg.m.m[2][1] + (int16_t)sdiv_pow2(arg.m.m[2][1], 3));
			arg.morph = morph;
			if ((uint32_t)s < 0xC) arg.fade = (int16_t)((uint32_t)(0xC000 - ((uint32_t)s << 12)) / 12 + 0xAA);
			else if ((uint32_t)s >= 0x3C) arg.fade = (int16_t)(((uint32_t)(s - 0x3C) << 12) / 12 + 0xAA);
			else arg.fade = 0;
			Play(SITE_PILLAR, d->layout_e, &arg, &frame);
		}
		// the afterimages: drawn 1..0xC, 0x7C..0x82, 0x18..0x46; recorded 0..0xA, 0x7B..0x80, 0x17..0x23
		if ((c > 0 && c <= 0xC) || (c > 0x7B && c <= 0x82) || (c > 0x17 && c < 0x47)) GhostDraw(z, d->ghosts);
		if ((c >= 0 && c <= 0xA) || (c >= 0x7B && c <= 0x80) || (c >= 0x17 && c <= 0x23)) GhostRecord(z, d->ghosts);
		if (c == 0) BdPlaySE(SOUND_Start, 0, 0x80);
		DirectorCamera(c, true);
		d->counter = (int16_t)(c + 1);
		if (d->counter < 0x8B) return 0;
		StreamQueueLoad(0x168, TexBase() + 0x3198, 1);
		StreamQueueLoad(0x169, TexBase() + 0x3198, 1);
		StreamQueueLoad(0x16A, TexBase() + 0x3198, 0);
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// RootTask (0x695AB0): node 0x10 (pool 0x25088D8)
	// ------------------------------------------------------------------
	// the director's start (root counter 1): Zell's position / angles saved, the target's anchor / scale, the stage captured
	// under it, Zell's place, the camera anchors, his animation set and chain 1, the stream state, Zell shown, the target's
	// model height, its position saved, raised by its model bottom and its chain flagged, the afterimage block
	static void StartDirector(DirectorNode *d)
	{
		Memset32(&d->counter, 0, 0x5F9);
		uint8_t *z = Caster();
		d->saved_pos[0] = U32(z, 0x1C);
		d->saved_pos[1] = U32(z, 0x20);
		d->saved_rot[0] = U32(z, 0xC);
		d->saved_rot[1] = U32(z, 0x10);
		GetDefaultEffectPosition(Target(), d->place);
		memcpy(d->anchor, d->place, 8);
		d->place[1] = 0;
		d->scale = (int16_t)((W16(Target(), 0x26) >> 1) + 0x400);
		int16_t box[6];
		ModelBox(LAYOUT_Crater + *(const uint32_t *)(LAYOUT_Crater + 0xC), box);
		box[0] = (int16_t)(mul32(box[0], d->scale) >> 12);
		box[3] = (int16_t)(mul32(box[3], d->scale) >> 12);
		box[1] = (int16_t)(mul32(box[1], d->scale) >> 12);
		box[4] = (int16_t)(mul32(box[4], d->scale) >> 12);
		box[2] = (int16_t)(mul32(box[2], d->scale) >> 12);
		box[5] = (int16_t)(mul32(box[5], d->scale) >> 12);
		CaptureStage(d->place, box);
		d->place[2] = (int16_t)(d->place[2] + (int16_t)((W16(Target(), 0x26) >> 1) + (W16(z, 0x26) >> 1)));
		CamAnchor()[0] = U32(z, 0x1C);
		CamAnchor()[1] = U32(z, 0x20);
		Anchor()[0] = *(const uint32_t *)&d->place[0];
		Anchor()[1] = *(const uint32_t *)&d->place[2];
		BindExternalAnimSet(z, d->bind, COM_Anim);
		QueueChainTransformation(z, 1);
		StreamStateInit((void *)(TexBase() + 0x2C90));
		U16(z, 0) &= 0xFFFB;
		MeasureHeight(Target(), d->height);
		Height() = d->height[1] - d->height[0] + 0x4E1;
		d->target_pos[0] = U32(Target(), 0x1C);
		d->target_pos[1] = U32(Target(), 0x20);
		W16(Target(), 0x1E) = (int16_t)(W16(Target(), 0x1E) - d->height[0]);
		ForChain([](uint8_t *e) { if (B8(e, 0) & 2) B8(e, 0) |= 8; });
		static const int32_t bones[0x12] = { 1, 2, 4, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
		GhostInit(d->ghosts, 0x12, bones);
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(held_note_root(r);)
		if (r->parity)
		{
			PacketCursor() = TexBase() + 0x3198;
			CaptureCursor() = TexBase() + 0x1D198;
			r->parity = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x1D198;
			CaptureCursor() = TexBase() + 0x37198;
			r->parity = 1;
		}
		Flash() = 0;
		if (r->counter == 1)
		{
			if (r->state == 0)
			{
				r->state = 1;
				InitTaskQueuePool(QDirector(), (void *)TexBase(), 0x17F0, 1);
				InitTaskQueuePool(QRings(), (void *)(TexBase() + 0x17F0), 0x104, 8);
				InitTaskQueuePool(QRocks(), (void *)(TexBase() + 0x2010), 0x24, 0x40);
				InitTaskQueuePool(QSprites(), (void *)(TexBase() + 0x2910), 0x1C, 0x20);
			}
			DirectorNode *d = (DirectorNode *)AddTaskToQueue(QDirector(), ORIG_DirectorTask);
			if (d) StartDirector(d);
		}
		OrthonormalizeCamera(&Camera(), View());
		int a = 1;
		if (r->state)
		{
			a = ExecuteTaskQueue(QDirector());
			ExecuteTaskQueue(QRings());
			ExecuteTaskQueue(QRocks());
			ExecuteTaskQueue(QSprites());
		}
		const int32_t f = Flash();
		if (f != FlashLast())
		{
			SetScreenFlash((uint32_t)f, 0);
			if (Flash() >= 0x1000)
			{
				for (uint32_t p = 0x1D98991; p < 0x1D98A41; p += 0x2C) var<uint8_t>(p) &= 0xFD;
			}
			else if (FlashLast() >= 0x1000)
			{
				for (uint32_t p = 0x1D98991; p < 0x1D98A41; p += 0x2C) var<uint8_t>(p) |= 2;
			}
			FlashLast() = Flash();
		}
		if (r->state && a == 0)
		{
			StreamPump();
			if (StreamPreload(1) == 0) return 0;
			// the director ended: the target / Zell put back, the target's chain's bit 8 cleared, the field of view back
			const DirectorNode *d = (const DirectorNode *)TexBase();
			U32(Target(), 0x1C) = d->target_pos[0];
			U32(Target(), 0x20) = d->target_pos[1];
			ForChain([](uint8_t *e) { if (B8(e, 0) & 2) U16(e, 0) &= 0xFFF7; });
			U32(Caster(), 0x1C) = d->saved_pos[0];
			U32(Caster(), 0x20) = d->saved_pos[1];
			U32(Caster(), 0xC) = d->saved_rot[0];
			U32(Caster(), 0x10) = d->saved_rot[1];
			ViewWord() = var<int16_t>(0x1D977A0);
			return TASK_END;
		}
		r->counter++;
		return 0;
	}
}

	void register_mag216_different_beat()
	{
		register_port(beat216::ORIG_RootTask, (void *)beat216::RootTask, "216 RootTask", 216);
		register_port(beat216::ORIG_DirectorTask, (void *)beat216::DirectorTask, "216 DirectorTask", 216);
		register_port(beat216::ORIG_RingTask, (void *)beat216::RingTask, "216 RingTask", 216);
		register_port(beat216::ORIG_RockATask, (void *)beat216::RockATask, "216 RockATask", 216);
		register_port(beat216::ORIG_RockBTask, (void *)beat216::RockBTask, "216 RockBTask", 216);
		register_port(beat216::ORIG_SparkTask, (void *)beat216::SparkTask, "216 SparkTask", 216);
		register_port(beat216::ORIG_EmberTask, (void *)beat216::EmberTask, "216 EmberTask", 216);
		// 30 fps layer: see mag216_different_beat_held.inc
		FX_HELD(register_mag216_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag216_different_beat_held.inc"
#endif
