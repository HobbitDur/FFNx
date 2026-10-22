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

// Effect 212: Burning Rave (Zell's Duel finisher; MAG_212_BURNING_RAVE_*). The Duel (effect 274) hands over to it when the
// player enters the finisher's input; the battle casts it with Zell as the caster.
//
// Structure (setup 0x6A0D20 -> _Init 0x6A0D50 (not ported: the setup: TIM upload, magic buffer 0x2510278, caster, root queue),
// file loader 0x6A0D30 (the module file 0x11AC250)):
//   RootTask (0x6A0DC0) - node 0x10 (pool 0x2510248): every tick the packet cursor / the stage-capture cursor alternate between
//     magic buffer + 0xAAE0 / + 0x1AAE0 and + 0x1AAE0 / + 0x2AAE0; on counter 1 sets up the three queues (director 1 x 0xCD0
//     at the magic buffer, rocks 0x40 x 0x24 at + 0xCD0, sprites 0x40 x 0x18 at + 0x15D0) and the party / enemy flag copy
//     (0x6A1280), and starts the director: the targets' records (entity, position, zero angles), the box of their anchors,
//     the stage captured into VRAM (0x6A12B0), Zell placed at the box centre (facing 0), his position / angles saved, the
//     camera anchors 0x2510260..0x251026C, the afterimage block (bones 0xE / 0x12 / 0x16), his animation set (0x5022C0,
//     com 0x1181078) and chain 1, the stream state (0x534110); then runs the queues; when the director ends it waits for the
//     sprites and the streams, puts the camera back (saved view 0xB8B800, field of view 0x1D977A0), then clears the party's
//     flag bit 4 and ends.
//   DirectorTask (0x6A1660) - node 0xCD0, counters 1..0xB6 (the counter steps first; UPDATE AND DRAW in one call per tick):
//     the screen flash (rising 1..0xB, 0x8FC, falling 0xAA..0xB5); 2..0x39 the ground layout (0x11883AC, its texture
//     scrolling 0x20 a tick) at his anchor, rocks (0x6A3400) 2..0x29; 0x11..0x2C sparks from his fists (0x6A3960, bones
//     0xD / 8 and 0xE / 0xA, the fist point 0x6A3A30); 0x51..0x68 a layout at his bone 0x16; 0x51..0x5C a glow at his bone
//     18 (0x701DD0); 0x54..0x6D a layout before him; 0x5E..0xA3 the burning layout (0x118BA10, at the box, again at its far
//     end from 0x6D with the stage captured anew, the debris pieces 0x6A3510 from its objects); 0x5E..0x6C Zell drawn by the
//     module (0x6A2F60) and hidden; 0x6D..0x85 the targets drawn by the module, hidden; 0x86..0xAD the targets thrown up and
//     spun (their +0x1E, drawn by the module), rocks / sprites 0x86..0x8F, sprites from their bones from 0x96; the
//     afterimages of Zell 0x40..0x50 (record 0x40..0x4A: 274's 0x68ADF0); sounds at 5 / 0x43; the camera every tick
//     (0x6A2349..0x6A2758); 0xA4 the targets shown; 0xAE the damage (0x506BA0), the targets / Zell put back, the streams
//     0x168 / 0x169; 0xB6 the end.
//   Rocks 0x6A30F0 (bounce) and 0x6A3280 / 0x6A33C0 / 0x6A33E0 (three models through 0x6A32A0), pieces 0x6A3630: prim models
//     (DRAW THEN UPDATE); sprites 0x6A3750 / 0x6A3800 / 0x6A38B0 (DRAW THEN UPDATE). The prim-player callback 0x6A2B80 draws
//     one layout object (0x701DD0) with the block's kind tables (0x31 / 0x32: texture windows, the fade, debris pieces).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Writes to battle entities: Zell (caster): position +0x1C (start: the box centre), facing +0x0E (start: 0), flags bit 4
//   (0x5E..0x6C via 0x6A30C0, cleared at 2), his position / angles +0x1C..+0x23 / +0x0C..+0x13 put back at 0xAE, his animation
//   set / chain; the targets: flags bit 4 (2, 0x6D..0xA4 hidden), bit 8 (0x86), +0x1E height (0x86..0xAD), position put back
//   and bits 0x0C cleared at 0xAE; the party (slots 0..2) bit 4 (set at 2, cleared at the end), the enemies (0x6FBDE0 at 2,
//   bit 4 put back from the copy at 0xA4 / 0xAE). Damage: 0x506BA0.
// CAMERA (eye 0xB8B7F0, look-at 0xB8B7F8): set on 1 (with 0x50A730 the return armed, field of view 0x1D8E038 = 0x120, roll
//   0x1D977A2 = 0), 0x657DF0 lerps 0x12..0x29, 0x2B..0x50, 0x51..0x6C, 0x6D..0x85, set on 0x86; the shake y 0x1D97712 on
//   0x51..0x56; the root puts back the saved view (0xB8B800) and the field of view (0x1D977A0) once the director ended.
// Module globals: 0x25101F8..0x25102C0 (module file 0x25101F8, context 0x25101FC, caster 0x2510200, queues 0x2510208 sprites /
//   0x2510218 rocks / 0x2510228 director / 0x2510238 root, root pool 0x2510248, packet cursors 0x2510258 / 0x251025C, camera
//   anchors 0x2510260..0x251026C, zero angles 0x2510270 (never written), magic buffer 0x2510278, zero matrix 0x2510280 (never
//   written), entity flag copy 0x25102A0 (7 x 4)).

#include "mag_common.h"

namespace ff8fx
{
namespace rave212
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x25101FC); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2510200); }
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2510208; }          // sprites (0x18)
	inline TaskQueue *QRocks() { return (TaskQueue *)0x2510218; }            // rocks / pieces (0x24)
	inline TaskQueue *QDirector() { return (TaskQueue *)0x2510228; }         // the director (1 x 0xCD0)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2510258); }
	inline uint32_t &CaptureCursor() { return var<uint32_t>(0x251025C); }    // the stage capture's geometry packets
	inline uint32_t *CamAnchor() { return (uint32_t *)0x2510260; }          // Zell's +0x1C / +0x20 dwords at the start
	inline int16_t *Centre() { return (int16_t *)0x2510268; }               // the box centre x, 0, far z
	inline const int16_t *ZeroAngles() { return (const int16_t *)0x2510270; }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2510278); }          // Magic_TextureOFF (magic buffer)
	inline const Mat4x3 *ZeroMatrix() { return (const Mat4x3 *)0x2510280; }
	inline uint32_t *SavedFlags() { return (uint32_t *)0x25102A0; }          // the 7 entities' flag words
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }
	inline uint32_t &FramePackets() { return var<uint32_t>(0x1D8E054); }
	inline int16_t *CamEye() { return (int16_t *)0xB8B7F0; }
	inline int16_t *CamLookAt() { return (int16_t *)0xB8B7F8; }

	static const uint32_t ORIG_RootTask = 0x6A0DC0;
	static const uint32_t ORIG_DirectorTask = 0x6A1660;
	static const uint32_t ORIG_RockTask = 0x6A30F0;
	static const uint32_t ORIG_RockATask = 0x6A3280;
	static const uint32_t ORIG_RockBTask = 0x6A33C0;
	static const uint32_t ORIG_RockCTask = 0x6A33E0;
	static const uint32_t ORIG_PieceTask = 0x6A3630;
	static const uint32_t ORIG_SparkTask = 0x6A3750;
	static const uint32_t ORIG_EmberTask = 0x6A3800;
	static const uint32_t ORIG_FlameTask = 0x6A38B0;
	static const uint32_t ORIG_GhostRecord274 = 0x68ADF0; // (274's range: Burning Rave calls Duel's copy)

	static const uint32_t COM_Anim = 0x1181078;
	static const uint32_t MODEL_Rock = 0x1183FA4, MODEL_RockA = 0x1184174, MODEL_RockB = 0x11843DC, MODEL_RockC = 0x1184694;
	static const uint32_t MODEL_Glow = 0x11826E0;
	static const uint32_t SEQ_Ember = 0x1182534, SEQ_Flame = 0x11823C8;
	static const uint32_t LAYOUT_Ground = 0x11883AC, LAYOUT_Bone = 0x1187F2C, LAYOUT_Front = 0x1184ACC, LAYOUT_Burn = 0x118BA10;
	static const uint32_t KIND_Ground = 0x1154F40, KIND_Bone = 0xFE0FB4, KIND_Front = 0xFE0FC0, KIND_Burn = 0x11AC25C, KIND_BurnB = 0x11AC268;
	static const uint8_t *const PIECE_Table = (const uint8_t *)0x11AC200;  // 32 vertex indices per object
	static const int16_t *const FIST_A = (const int16_t *)0x11AC240, *const FIST_B = (const int16_t *)0x11AC248; // offset + bone
	static const void *const SOUND_A = (const void *)0x1181070;
	static const void *const SOUND_B = (const void *)0x1181074;

	// layout sites (director node offsets of the decoded layouts)
	enum : int { SITE_GROUND = 1, SITE_BONE, SITE_FRONT, SITE_BURN };

	// ---- engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t SharedRand() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }  // Effect_SharedLcg_Rand
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline uint32_t BindExternalAnimSet(uint8_t *entity, void *out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, void *, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationX(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701220)(a, out); }   // MAG_063_RotationXMatrix
	inline void RotationY(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(a, out); }   // Effect_RotationYMatrix
	inline void RotationZ(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x7012C0)(a, out); }   // MAG_065_RotationZMatrix
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3x3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }
	inline void Apply3DTransform(const Mat4x3 *m, const int16_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int32_t *)>(0x56C3D0)(m, in, out); }
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); } // MAG_063_RenderPrimSet
	inline uint32_t DrawShadow(uint8_t *e, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(e, ot, a, cursor); } // BattleEntity_DrawShadow
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, ctx, ot, a, cursor); }
	inline void MeasureModelHeight(uint8_t *e, int16_t *out) { fn<void (__cdecl *)(uint8_t *, int16_t *)>(0x6DFD90)(e, out); } // MAG_070_ShockwavePulsar_MeasureModelHeight
	inline void SetEnemyFlag4() { fn<void (__cdecl *)()>(0x6FBDE0)(); }      // MAG_065_SetEnemyEntitiesFlag4
	inline void CamLerp(const int16_t *a, const int16_t *b, int32_t t) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t)>(0x657DF0)(a, b, t); } // MAG_325_CameraLerp
	inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }   // BS_Camera_ArmReturnAfterEffect
	inline void VecLerp(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CB90)(a, b, wa, wb, out); }
	inline void TransformVectorBy3x3(const Mat4x3 *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int32_t *, int32_t *)>(0x56C600)(m, in, out); }
	inline uint32_t SetDrawEnv(void *prim, uint32_t env) { return fn<uint32_t (__cdecl *)(void *, uint32_t)>(0x45C0F0)(prim, env); } // SSIGPU_SetDrawEnv
	inline void SetDrawAreaPrim(void *prim, const int16_t *rect) { fn<void (__cdecl *)(void *, const int16_t *)>(0x45C940)(prim, rect); }
	inline void SetDrawOffsetPrim(void *prim, const int16_t *xy) { fn<void (__cdecl *)(void *, const int16_t *)>(0x45C9B0)(prim, xy); }
	inline void GetScreenOffset(int32_t *x, int32_t *y) { fn<void (__cdecl *)(int32_t *, int32_t *)>(0x56CD10)(x, y); }
	inline void SetScreenOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x56CCE0)(x, y); }  // Call_Bs_ParseCamera
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
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
	inline void GteReadMAC0(int32_t *p) { fn<void (__cdecl *)(int32_t *)>(0x45E3C0)(p); }
	inline void GteSetSZ123(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45E1D0)(a, b, c, d); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(int32_t *p) { fn<void (__cdecl *)(int32_t *)>(0x45E3D0)(p); }   // GTE_ReadOTZ (a dword)
	inline void GteReadOTZdiv4(void *p) { fn<void (__cdecl *)(void *)>(0x45E250)(p); }      // GTE_ReadOTZ_SZ3div4
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); }    // GTE_LoadV0_u16

#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes at 0x2510248
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t state;     // +0x0E 0 = not set up, 1 = running, 2 = the camera put back
		uint8_t parity;    // +0x0F packet arena parity
	};
	struct TargetRec // 0x18 bytes from the director +0x40
	{
		uint8_t *e;        // +0x00
		uint32_t pos01;    // +0x04 the entity's +0x1C / +0x1E at the start
		uint32_t pos23;    // +0x08 +0x20 / +0x22
		int16_t ang[4];    // +0x0C its spin (0x86..0xAD); from the zero angles 0x2510270 at the start
		int16_t height[2]; // +0x14 its model top / bottom (0x6DFD90 at 0x86)
	};
	struct DirectorNode // pool of 1 node of 0xCD0 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 1..0xB6 (steps first)
		int16_t pad0E;
		uint8_t bind[0x20];// +0x10 Zell's bind block
		uint32_t saved_pos[2]; // +0x30 Zell's +0x1C..+0x23
		uint32_t saved_rot[2]; // +0x38 Zell's +0x0C..+0x13
		TargetRec recs[10];// +0x40
		int16_t centre[3]; // +0x130 the box centre x, y, its far z
		int16_t pad136;
		int16_t anchor[4]; // +0x138 Zell's default effect position (2)
		int16_t box[8];    // +0x140 the burning layout's frames: +0x140 / +0x144 (near), +0x148 / +0x14C (far); the stage
		                   //        capture reads 6-word boxes at +0x140 and +0x148 (the latter's last words: ghosts +0..+3)
		uint8_t ghosts[0x488]; // +0x150 the afterimage block
		uint8_t burn_l[0x484]; // +0x5D8 layout 0x11883AC (2..0x39) / 0x118BA10 (0x5E..0xA3)
		uint8_t bone_l[0x44];  // +0xA5C layout 0x1187F2C
		uint8_t front_l[0x230];// +0xAA0 layout 0x1184ACC
	};
	// the prim-player callback's parameter block (part of the director's stack frame)
	struct PlayArg
	{
		Mat4x3 m;          // +0x00 the layout's frame (the view composed)
		const uint8_t *kind;   // +0x20 per-object kind (0x31 / 0x32: texture window, scrolled)
		const uint8_t *fade;   // +0x24 per-object (0x31: faded) or null
		const uint8_t *pieces; // +0x28 per-object (0x31: debris pieces) or null
		int16_t depth;     // +0x2C header +0x18
		int16_t depth2;    // +0x2E header +0x18 with a fade table
		uint8_t scroll;    // +0x30 texture window v (x 0x7F)
		uint8_t pad31[3];
		uint32_t morph;    // +0x34 blended vertex frames
	};
	struct RockNode // 0x24 (rocks / pieces)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t vel[3];    // +0x12
		int8_t acc[3];     // +0x18
		uint8_t life;      // +0x1B
		uint8_t rx, ry;    // +0x1C x / y angles (x 16)
		int8_t spin[2];    // +0x1E
		int16_t scale;     // +0x20
		int16_t pad22;
	};
	struct SpriteNode // 0x18
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t size;      // +0x12
		int8_t vel[3];     // +0x14 (x 4)
		uint8_t life;      // +0x17 the frame = the sequence's frames - life
	};
	// the afterimage draw context (0x6A2760 / 274's 0x68A7B0: the Field_Alloc block of 0x74 bytes from +0x20)
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
	static_assert(sizeof(RootNode) == 0x10 && sizeof(DirectorNode) == 0xCD0 && sizeof(TargetRec) == 0x18, "Burning Rave nodes");
	static_assert(sizeof(PlayArg) == 0x38 && sizeof(RockNode) == 0x24 && sizeof(SpriteNode) == 0x18 && sizeof(GhostCtx) == 0x54, "Burning Rave blocks");
}
}

#ifdef FF8_FX_HELD
#include "mag212_burning_rave_held.h"
#endif

namespace ff8fx
{
namespace rave212
{
	using namespace eng;
	using namespace magc;

	inline int16_t &W16(void *p, int off) { return *(int16_t *)((uint8_t *)p + off); }
	inline uint16_t &U16(void *p, int off) { return *(uint16_t *)((uint8_t *)p + off); }
	inline int32_t &W32(void *p, int off) { return *(int32_t *)((uint8_t *)p + off); }
	inline uint32_t &U32(void *p, int off) { return *(uint32_t *)((uint8_t *)p + off); }
	inline uint8_t &B8(void *p, int off) { return *((uint8_t *)p + off); }

	// action 0's target records / count (24 bytes each, +0 = the slot)
	inline uint8_t *Targets() { return Ctx()->actions[0].targets; }
	inline int32_t TargetCount() { return Ctx()->actions[0].target_count; }

	// ------------------------------------------------------------------
	// Afterimages of Zell (274's code: Burning Rave's 0x6A2760 / 0x6A2940 are exact copies of Duel's 0x68A7B0 / 0x68A990)
	// ------------------------------------------------------------------
	// 0x6A1230: afterimage block init: no ghost yet, `n` recorded bones (the 128-entry bone -> record map, 0xFF = not recorded)
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

	// 0x68ADF0 (Duel's range: Burning Rave calls it): the recorded bones' matrices of Zell's model into the next ghost slot
	static void GhostRecord(uint8_t *entity, uint8_t *g)
	{
		g[0] = 1;
		const uint8_t s = g[2];
		g[3] = s;
		uint8_t *dst = g + 0x88 + (uint32_t)s * 0x200;
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

	// 0x6A2A90: a vertex group of an object through the current GTE matrix into the screen vertex buffer (sxy, sz / 4, clip
	// flags: 1 / 2 left / right, 4 / 8 top / bottom of the context's rectangle x 8; behind the eye: flags 0x10 only)
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

	// 0x68AAE0 (Duel's range: Burning Rave calls it): the object's flat textured triangles / quads (semi-transparent, the
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
								GteReadOTZ32(&c->otz);
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
									GteReadOTZ32(&c->otz);
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

	// 0x6A2940: the model's visible objects (entity +0x7C mask) with their recorded bones' (ghost) matrices: vertex groups of a
	// bone not recorded are marked behind the eye; then the polygons
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
					const Mat4x3 *m = (const Mat4x3 *)(c->slot + (uint32_t)(k + 8) * 0x20);
					GteSetRotMatrixCtrl(m);
					GteSetTransVectorCtrl(m);
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
			FramePackets() = GhostPolys(c, ot, shift, FramePackets());
		}
	}

	// 0x6A2760: the recorded ghosts (up to 2, the newer one first, brighter: his colour (+0x28) x 0x800 * 1.5, then x 0x400 *
	// 1.5) of Zell's model: every recorded bone matrix through the view x his world matrix (into the slot's second half), the
	// model drawn with them (0x6A2940); the count drops by one on a tick without a recording
	static void GhostDraw(uint8_t *entity, uint8_t *g)
	{
		// 30 fps layer: see mag212_burning_rave_held.inc
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
			c->slot = (uint32_t)g + 0x88 + (((uint32_t)g[3] - (uint32_t)i) & 1) * 0x200;
			for (int32_t b = 0; b < g[4]; b++)
			{
				const uint32_t r = c->slot + (uint32_t)b * 0x20;
				GteSetRotMatrixCtrl((const Mat4x3 *)h);
				GteLoadIRFromColumn((const void *)(r + 0));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x100));
				GteLoadIRFromColumn((const void *)(r + 2));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x102));
				GteLoadIRFromColumn((const void *)(r + 4));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x104));
				GteSetTransVectorCtrl((const Mat4x3 *)h);
				GteLoadV0FromDwords((const void *)(r + 0x14));
				GteMVMVA_RotV0Tr();
				GteReadMAC123((int32_t *)(r + 0x114));
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
	// 0x6A1280: the 7 entities' flag words (zero-extended) into the copy 0x25102A0
	static void SaveFlags()
	{
		for (int i = 0; i < 7; i++) SavedFlags()[i] = *(const uint16_t *)Entity(i);
	}

	// 0x6A2F20: flag bit 4 of the enemy slots (3..6) put back from the copy where the entity is present (bit 1)
	static void RestoreEnemyFlag4()
	{
		for (int i = 3; i < 7; i++)
		{
			uint8_t *e = Entity(i);
			const uint16_t f = *(const uint16_t *)e;
			if (f & 2) *(uint16_t *)e = (uint16_t)(((B8(&SavedFlags()[i], 0) ^ f) & 4) ^ f);
		}
	}

	// 0x69A700 (MAG_210's range, Different Beat): the box (min x, y, z, max x, y, z) of a prim model's vertices (+0xC, 8 bytes)
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

	// 0x6A3220: a yaw rotation scaled by s (y scale s): m = 0, then x / z rows (cos, -sin / sin, cos) x s, m11 = s
	static void YawScaled(int32_t angle, int32_t s, Mat4x3 *m)
	{
		memset(m, 0, 0x14);
		const int32_t sn = mul32(ComputeSin(angle), s) >> 12;
		const int32_t cs = mul32(ComputeCos(angle), s) >> 12;
		m->m[2][0] = (int16_t)sn;
		m->m[0][0] = (int16_t)cs;
		m->m[1][1] = (int16_t)s;
		m->m[0][2] = (int16_t)-sn;
		m->m[2][2] = (int16_t)cs;
	}

	// 0x6A3A30: a point of Zell's model: `p` (offset, its +6 word = the bone) through his bone x world matrix (his pose rebuilt)
	static void FistPoint(const int16_t *p, int16_t *out)
	{
		uint8_t *c = Caster();
		uint8_t *skel = **(uint8_t ***)(c + 0x64);
		BuildBoneMatricesFromPose(c + 0x60);
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(skel + 0x20 + (int32_t)p[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(p);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// 0x6A2F60: an entity drawn by the module: its shadow (OT - 0xBC), its bones' world matrices from its world matrix x
	// (rotation `angles`, translation `off` minus the rotated `off`) under the view, RenderGeometry, its pose matrices rebuilt;
	// the same for its weapon (+0x78)
	static void DrawEntity(uint8_t *e, const int16_t *angles, const int16_t *off)
	{
		// 30 fps layer: see mag212_burning_rave_held.inc
		FX_HELD(held_note_entity(e, angles, off);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x5C);
		Mat4x3 *m = (Mat4x3 *)h;
		FramePackets() = DrawShadow(e, OTBase() - 0xBC, 4, FramePackets());
		ComposeZYXRotationMatrix(angles, m);
		m->t[0] = off[0];
		m->t[1] = off[1];
		m->t[2] = off[2];
		ComposeAffineTransform((const Mat4x3 *)(e + 0x40), m, m);
		ComposeAffineTransform(&Camera(), m, m);
		Apply3DTransform(m, off, (int32_t *)(h + 0x20));
		m->t[0] = (int32_t)((uint32_t)m->t[0] - U32(h, 0x20));
		m->t[1] = (int32_t)((uint32_t)m->t[1] - U32(h, 0x24));
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

	// 0x6A30C0: Zell drawn by the module (zero angles / offset) and hidden from the engine's draw (bit 4)
	static void DrawCaster()
	{
		DrawEntity(Caster(), ZeroAngles(), ZeroAngles());
		B8(Caster(), 0) |= 4;
	}

	// 0x6A12B0: the stage captured into VRAM (0x2C0, 0x100, 0x80 x 0x100): the screen flash off; a top view of the stage over
	// the box `a` (x0 = a[0], x1 = a[3], z0 = a[2], z1 = a[5]) scaled by the burning layout's extent `b` into the 0x80 x 0x100
	// rectangle (the draw environment switched to it, the stage geometry 0x1D989C0 when present, a black quad first), the screen
	// offset put back, then the drawing area / offset packets of the rectangle (OT bucket 0x11)
	static void CaptureStage(const int16_t *a, const int16_t *b)
	{
		// 30 fps layer: see mag212_burning_rave_held.inc
		FX_HELD(held_note_capture(a, b);)
		const int32_t b0 = -(int32_t)b[0];
		const int32_t b3 = b[3];
		const int32_t w = b3 + b0;
		const int32_t nb2 = -(int32_t)b[2];
		const int32_t b5 = b[5];
		const int32_t dz = b5 + nb2;
		SetScreenFlash(0, 0);
		int16_t rect[4] = { 0x2C0, 0x100, 0x80, 0x100 };
		// (the round-up tests the quotient's remainder, not the extent's)
		int32_t sx = w / 0x80;
		if (sx % 0x80 != 0) sx++;
		int32_t sz = dz / 0x100;
		if (sz % 0x100 != 0) sz++;
		Mat4x3 m, m2;
		m.t[1] = 0;
		m.t[0] = -((int32_t)a[0] / sx);
		m.t[2] = (int32_t)a[2] / sz;
		RotationY(0x800, &m);
		TransformVectorBy3x3(&m, m.t, m.t);
		RotationX(-0x400, &m2);
		MatrixMultiply3x3(&m2, &m);
		int32_t s3[3];
		s3[1] = 0;
		m2.t[0] = (b0 - b3) / 2 / sx + m.t[0];
		m2.t[2] = (int32_t)var<int16_t>(0x1D8E038);
		m2.t[1] = m.t[2] - ((nb2 - b5) / 2 / sz);
		s3[0] = 0x1000000 / (shl32(w, 12) / rect[2]);
		s3[2] = 0x1000000 / (shl32(dz, 12) / rect[3]);
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
		U16(q, 0x12) = 0x100;
		U16(q, 0x14) = 0x80;
		U16(q, 0x16) = 0x100;
		q[4] = 0;
		q[5] = 0;
		q[6] = 0;
		U32(q, 0) = 0x05000000;
		q[7] = 0x28;
		InsertPrimAltViewport(OTBase() + 0x44, q);
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
	// Rocks (0x6A30F0, 0x6A32A0 through 0x6A3280 / 0x6A33C0 / 0x6A33E0), pieces (0x6A3630), sprites (0x6A3750 / 0x6A3800 /
	// 0x6A38B0). DRAW THEN UPDATE
	// ------------------------------------------------------------------
	// a rock model at pos: yaw `ay` scaled by `scale`, then the x angle `ax`
	static void RockDrawAt(const int16_t *pos, int32_t ax, int32_t ay, int32_t scale, uint32_t model)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x98);
		Mat4x3 *m = (Mat4x3 *)(h + 0x58);
		YawScaled(ay, scale, (Mat4x3 *)(h + 0x78));
		RotationX(ax, m);
		MatrixMultiply3x3(m, (const Mat4x3 *)(h + 0x78));
		m->t[0] = pos[0];
		m->t[1] = pos[1];
		m->t[2] = pos[2];
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x98);
	}

	// the draw of 0x6A30F0 / 0x6A32A0 / 0x6A3630: yaw (x 16) scaled by the node's scale, then its x angle (x 16), at the node
	static void RockDraw(const RockNode *k, uint32_t model)
	{
		// 30 fps layer: see mag212_burning_rave_held.inc
		FX_HELD(held_note_rock(k, model);)
		RockDrawAt(k->pos, (int32_t)(int8_t)k->rx << 4, (int32_t)(int8_t)k->ry << 4, k->scale, model);
	}

	// 0x6A30F0's step: angles, motion, a bounce on the ground (y >= 0: vy = -vy / 2), then the acceleration
	static void RockMoveBounce(RockNode *k)
	{
		const int16_t vy = k->vel[1];
		k->pos[1] = (int16_t)(k->pos[1] + vy);
		k->rx = (uint8_t)(k->rx + (uint8_t)k->spin[0]);
		k->pos[2] = (int16_t)(k->pos[2] + k->vel[2]);
		k->ry = (uint8_t)(k->ry + (uint8_t)k->spin[1]);
		k->pos[0] = (int16_t)(k->pos[0] + k->vel[0]);
		if (k->pos[1] >= 0) k->vel[1] = (int16_t)(-(int32_t)vy >> 1);
		k->vel[0] = (int16_t)(k->vel[0] + k->acc[0]);
		k->vel[1] = (int16_t)(k->vel[1] + k->acc[1]);
		k->vel[2] = (int16_t)(k->vel[2] + k->acc[2]);
	}

	// 0x6A32A0's / 0x6A3630's step: angles, motion, the acceleration
	static void RockMove(RockNode *k)
	{
		k->rx = (uint8_t)(k->rx + (uint8_t)k->spin[0]);
		k->pos[0] = (int16_t)(k->pos[0] + k->vel[0]);
		k->ry = (uint8_t)(k->ry + (uint8_t)k->spin[1]);
		k->pos[1] = (int16_t)(k->pos[1] + k->vel[1]);
		k->pos[2] = (int16_t)(k->pos[2] + k->vel[2]);
		k->vel[0] = (int16_t)(k->vel[0] + k->acc[0]);
		k->vel[1] = (int16_t)(k->vel[1] + k->acc[1]);
		k->vel[2] = (int16_t)(k->vel[2] + k->acc[2]);
	}

	// 0x6A30F0
	static uint32_t __cdecl RockTask(TaskNode *n)
	{
		RockNode *k = (RockNode *)n;
		RockDraw(k, MODEL_Rock);
		if (--k->life == 0) return TASK_END;
		RockMoveBounce(k);
		return 0;
	}

	// 0x6A32A0: a falling rock of the given model
	static uint32_t RockModelTask(RockNode *k, uint32_t model)
	{
		RockDraw(k, model);
		if (--k->life == 0) return TASK_END;
		RockMove(k);
		return 0;
	}
	// 0x6A3280 / 0x6A33C0 / 0x6A33E0: the three falling rock models
	static uint32_t __cdecl RockATask(TaskNode *n)
	{
		return RockModelTask((RockNode *)n, MODEL_RockA);
	}
	static uint32_t __cdecl RockBTask(TaskNode *n)
	{
		return RockModelTask((RockNode *)n, MODEL_RockB);
	}
	static uint32_t __cdecl RockCTask(TaskNode *n)
	{
		return RockModelTask((RockNode *)n, MODEL_RockC);
	}

	// 0x6A3630: a debris piece of the burning layout
	static uint32_t __cdecl PieceTask(TaskNode *n)
	{
		RockNode *k = (RockNode *)n;
		RockDraw(k, MODEL_Rock);
		if (--k->life == 0) return TASK_END;
		RockMove(k);
		return 0;
	}

	// the draw of 0x6A3750 / 0x6A3800 / 0x6A38B0: the sequence's frame = its frames - life
	static void SpriteDraw(const SpriteNode *s, uint32_t seq, int32_t depth)
	{
		// 30 fps layer: see mag212_burning_rave_held.inc
		FX_HELD(held_note_sprite(s, seq, depth);)
		TransformCameraByShadowRotation(s->pos, s->size, depth);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = seq;
		W16(h, 0x24) = 0;
		W16(h, 4) = (int16_t)(*(const uint32_t *)(seq + 8) - (uint32_t)(int32_t)(int16_t)(int8_t)s->life);
		PacketCursor() = InitEffectSequenceFromData(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void SpriteMove(SpriteNode *s)
	{
		s->pos[1] = (int16_t)(s->pos[1] + ((int32_t)s->vel[1] << 2));
		s->pos[2] = (int16_t)(s->pos[2] + ((int32_t)s->vel[2] << 2));
		s->pos[0] = (int16_t)(s->pos[0] + ((int32_t)s->vel[0] << 2));
	}

	static uint32_t SpriteTaskStep(SpriteNode *s, uint32_t seq, int32_t depth)
	{
		SpriteDraw(s, seq, depth);
		if (--s->life == 0) return TASK_END;
		SpriteMove(s);
		return 0;
	}
	// 0x6A3750: a spark (a fixed depth -0x89)
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		return SpriteTaskStep((SpriteNode *)n, SEQ_Ember, -0x89);
	}
	// 0x6A3800 / 0x6A38B0: an ember / a flame (depth -size / 4)
	static uint32_t __cdecl EmberTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		return SpriteTaskStep(s, SEQ_Ember, -((int32_t)s->size >> 2));
	}
	static uint32_t __cdecl FlameTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		return SpriteTaskStep(s, SEQ_Flame, -((int32_t)s->size >> 2));
	}

	// 0x6A3400: count + 1 rocks of task `fn` around (x, y, z) at radius (rand * rs >> 15) + rb, scale (rand * ss >> 15) + sb,
	// life (rand * ls >> 15) + lb, gravity -20
	static void SpawnRocks(int32_t count, int32_t x, int32_t y, int32_t z, int32_t ls, int32_t lb, int32_t rs, int32_t rb, int32_t ss, int32_t sb, uint32_t fn)
	{
		RockNode *k = (RockNode *)AddTaskToQueue(QRocks(), fn);
		while (k)
		{
			*(uint32_t *)k->acc = 0;
			k->acc[1] = (int8_t)0xEC;
			k->scale = (int16_t)((mul32(CrtRand(), ss) >> 15) + sb);
			const int32_t r = (mul32(CrtRand(), rs) >> 15) + rb;
			const int32_t a = CrtRand();
			const int32_t sn = mul32(ComputeSin(a), r) >> 12;
			const int32_t cs = mul32(ComputeCos(a), r) >> 12;
			k->pos[0] = (int16_t)(sn + x);
			k->pos[1] = (int16_t)y;
			k->pos[2] = (int16_t)(cs + z);
			k->life = (uint8_t)((uint8_t)(mul32(CrtRand(), ls) >> 15) + (uint8_t)lb);
			k->vel[2] = 0;
			k->vel[1] = 0;
			k->vel[0] = 0;
			k->rx = (uint8_t)CrtRand();
			k->ry = (uint8_t)CrtRand();
			k->spin[0] = (int8_t)CrtRand();
			k->spin[1] = (int8_t)CrtRand();
			if (count-- == 0) break;
			k = (RockNode *)AddTaskToQueue(QRocks(), fn);
		}
	}

	// 0x6A3960: count + 1 sprites of task `fn` at (x, y, z) + off + (rand * spread >> 15) each, velocity (vx, vy, vz) x 4, size
	// (rand * ss >> 15) + sb, `life` frames
	static void SpawnSprites(int32_t count, int32_t life, int32_t x, int32_t y, int32_t z, int32_t spread, int32_t off, int32_t vx, int32_t vy, int32_t vz, int32_t ss, int32_t sb, uint32_t fn)
	{
		SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), fn);
		while (s)
		{
			s->pos[0] = (int16_t)((mul32(CrtRand(), spread) >> 15) + off + x);
			s->pos[1] = (int16_t)((mul32(CrtRand(), spread) >> 15) + y + off);
			s->pos[2] = (int16_t)((mul32(CrtRand(), spread) >> 15) + z + off);
			s->size = (int16_t)((mul32(CrtRand(), ss) >> 15) + sb);
			s->vel[1] = (int8_t)vy;
			s->vel[0] = (int8_t)vx;
			s->vel[2] = (int8_t)vz;
			s->life = (uint8_t)life;
			if (count-- == 0) break;
			s = (SpriteNode *)AddTaskToQueue(QSprites(), fn);
		}
	}

	// 0x6A3510: a debris piece from a burning-layout object (frames < 0x20): at its two piece vertices' blend (the morph's
	// keyframes index the object's piece table), relative to the box's far point, flung sideways (object 0 to -x), 10 ticks
	static void SpawnPiece(prim::Layout *l, prim::Record *r, uint8_t *h)
	{
		if (l->frame >= 0x20) return;
		RockNode *k = (RockNode *)AddTaskToQueue(QRocks(), ORIG_PieceTask);
		if (!k) return;
		const uint8_t *verts = *(uint8_t **)(h + 4);
		const uint8_t *tab = PIECE_Table + (int32_t)(int16_t)r->flags_lo * 0x20;
		const int32_t t = r->b;
		VecLerp(verts + tab[r->b0] * 8, verts + tab[r->b1] * 8, 0x1000 - t, t, k->pos);
		const DirectorNode *d = (const DirectorNode *)TexBase();
		k->pos[0] = (int16_t)(d->box[4] - k->pos[0]);
		k->pos[1] = 0;
		*(uint32_t *)k->acc = 0;
		k->pos[2] = (int16_t)(d->box[6] - k->pos[2]);
		k->acc[1] = 0x1E;
		k->vel[1] = 0;
		k->vel[2] = 0;
		if (r->flags_lo == 0) k->vel[0] = (int16_t)(-0x5A - (CrtRand() & 0x3F));
		else k->vel[0] = (int16_t)((CrtRand() & 0x3F) + 0x5A);
		k->life = 0xA;
		k->scale = (int16_t)((shl32(CrtRand(), 12) >> 15) + 0x800);
		k->rx = (uint8_t)CrtRand();
		k->ry = (uint8_t)CrtRand();
		k->spin[0] = (int8_t)CrtRand();
		k->spin[1] = (int8_t)CrtRand();
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6A2B80): one object of a layout (0x701DD0)
	// ------------------------------------------------------------------
	// the record's offset is loaded into the GTE V0 with its 4th word never written (the stack's residue: the port loads 0)
	static int16_t g_off_pad = 0;
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
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
			// the offset through the zero matrix 0x2510280 (never written), the rotation composed with it
			GteSetRotMatrixCtrl(ZeroMatrix());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t t[3];
			GteReadMAC123(t);
			m.t[0] = t[0];
			m.t[1] = t[1];
			m.t[2] = t[2];
			MatrixMultiply(ZeroMatrix(), &m);
		}
		else if (r->flags & 0x200)
		{
			m.t[0] = off[0];
			m.t[1] = off[1];
			m.t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
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
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		U16(h, 0x22) = 0;
		U16(h, 0x20) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		const uint8_t kind = arg->kind[object];
		if (kind == 0x31)
		{
			U16(h, 0x2C) = 0;
			U16(h, 0x2E) = 0;
			U16(h, 0x32) = 0x80;
			U16(h, 0x30) = 0x100;
			U16(h, 0x22) = (uint16_t)(arg->scroll & 0x7F);
		}
		else if (kind == 0x32)
		{
			U16(h, 0x2C) = 0;
			U16(h, 0x2E) = 0x80;
			U16(h, 0x32) = 0x80;
			U16(h, 0x30) = 0x100;
			U16(h, 0x22) = (uint16_t)(arg->scroll & 0x7F);
		}
		if (arg->fade)
		{
			*(int32_t *)(h + 0x18) = arg->depth2;
			if (arg->fade[object] == 0x31)
			{
				*(int32_t *)(h + 0xC) = 0x8FC;
				B8(h, 0x1C) |= 0xC0;
				*(uint32_t *)(h + 8) = 0;
			}
		}
		if (arg->pieces && arg->pieces[object] == 0x31) SpawnPiece(l, r, h);
		PacketCursor() = RenderPrimSet(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x6C);
	}

	// ------------------------------------------------------------------
	// DirectorTask (0x6A1660): node 0xCD0, counters 1..0xB6 (the counter steps first)
	// ------------------------------------------------------------------
	// one layout of the director through the prim player (0x701970), the callback 0x6A2B80: its frame (arg->m) under the view
	static void Play(DirectorNode *d, int site, uint8_t *layout, PlayArg *arg)
	{
		// 30 fps layer: see mag212_burning_rave_held.inc
		FX_HELD(held_note_layout(d, site, layout, arg);)
		ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
		prim::play((prim::Layout *)layout, PartCallback, (int)arg, 0);
	}

	// the screen flash level at counter c (rising 1..0xB, 0x8FC, falling 0xAA..0xB5)
	static int32_t FlashLevel(int32_t c)
	{
		if (c < 0xC) return c * 2300 / 12;
		if (c >= 0xAA) return (0xB6 - c) * 2300 / 12;
		return 0x8FC;
	}

	// the shake y (0x1D97712) the director writes on 0x51..0x56: +-cos(341 e + 0xAA) * 100 (odd e negated)
	static int16_t ShakeY(int32_t e)
	{
		const int32_t v = mul32(ComputeCos(e * 341 + 0xAA), 100) >> 12;
		return (int16_t)((e & 1) ? -v : v);
	}

	// 0x51..0x5C: a glow (0x11826E0) at Zell's bone 18, b = 0..0xB: turning b x 0x100, its z scale (0x1800 - cos(b x 0x1000 /
	// 48)) x 2, 0x1000 behind the bone; fading in over 4 ticks, out from 5 (b == 4: no fade), its texture v (b & 3) x 0x20
	struct GlowArgs
	{
		int32_t spin, cosarg; // the z turn, the cosine argument of the z scale
		uint32_t mode, fade;  // header +0x1C (0xF0: faded by +0xC toward black; 0x30: not faded), +0xC
		uint16_t v;           // texture v
	};
	static void GlowParams(int32_t b, GlowArgs *g)
	{
		g->spin = b << 8;
		g->cosarg = (int32_t)(((uint32_t)b << 12) / 48);
		g->mode = b == 4 ? 0x30 : 0xF0;
		g->fade = b < 4 ? (uint32_t)(4 - b) << 10 : (uint32_t)b << 9;
		g->v = (uint16_t)((b & 3) << 5);
	}
	// the glow with the given bone (model space) and world matrices
	static void GlowDraw(const GlowArgs &g, const Mat4x3 *bone, const Mat4x3 *world)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x8C);
		Mat4x3 *m = (Mat4x3 *)h;
		RotationZ(g.spin, m);
		const int32_t cs = ComputeCos(g.cosarg);
		m->t[0] = 0;
		m->t[1] = 0;
		W16(h, 0x10) = (int16_t)((0x1800 - cs) << 1);
		m->t[2] = -0x1000;
		ComposeAffineTransform(bone, m, m);
		ComposeAffineTransform(world, m, m);
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		uint8_t *p = h + 0x20;
		U32(p, 0) = MODEL_Glow;
		if (g.mode == 0xF0)
		{
			U32(p, 8) = 0;
			U32(p, 0xC) = g.fade;
		}
		U32(p, 0x1C) = g.mode; // (0x30: the fade / colour words are not written, not read)
		W32(p, 0x18) = -0x80;
		U16(p, 0x20) = 0;
		U16(p, 0x22) = g.v;
		U16(p, 0x26) = 0;
		U16(p, 0x24) = 0;
		U16(p, 0x2A) = 0x100;
		U16(p, 0x28) = 0x100;
		U16(p, 0x2C) = 0;
		U16(p, 0x2E) = 0x80;
		U16(p, 0x32) = 0x80;
		U16(p, 0x30) = 0x100;
		PacketCursor() = RenderPrimSet(p, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x8C);
	}
	static void DrawGlow(int32_t b)
	{
		// 30 fps layer: see mag212_burning_rave_held.inc
		FX_HELD(held_note_glow(b);)
		GlowArgs g;
		GlowParams(b, &g);
		uint8_t *skel = **(uint8_t ***)(Caster() + 0x64);
		GlowDraw(g, (const Mat4x3 *)(skel + 0x380), (const Mat4x3 *)(Caster() + 0x40));
	}

	// the director's camera at counter c (eye 0xB8B7F0, look-at 0xB8B7F8): set on 1 (`engine`: the camera return armed),
	// lerps 0x12..0x29, 0x2B..0x50, 0x51..0x6C (the shake y on 0x51..0x56 when `engine`), 0x6D..0x85, set on 0x86. X / Z = Zell's
	// position at the start (0x2510260 / 0x2510264), Cx / Cz = the box centre / far z (0x2510268 / 0x251026C)
	static void DirectorCamera(int32_t c, bool engine)
	{
		const int16_t X = (int16_t)CamAnchor()[0], Z = (int16_t)CamAnchor()[1];
		if (c == 1)
		{
			if (engine) CameraArmReturn();
			var<int16_t>(0x1D8E038) = 0x120;
			var<int16_t>(0x1D977A2) = 0;
			CamLookAt()[1] = (int16_t)0xFCFD;
			CamEye()[1] = (int16_t)0xFF1B;
			CamLookAt()[0] = (int16_t)(X - 0x7D);
			CamLookAt()[2] = (int16_t)(Z - 0x2DC);
			CamEye()[0] = (int16_t)(X - 0x441);
			CamEye()[2] = (int16_t)(Z - 0x72A);
		}
		if ((uint32_t)(c - 0x12) < 0x18)
		{
			const int16_t a[6] = { (int16_t)(X - 0x5F), (int16_t)0xFD1E, (int16_t)(Z - 0x276), (int16_t)(X - 0xC8), (int16_t)0xFE99, (int16_t)(Z - 0x635) };
			const int16_t b[6] = { (int16_t)(X - 0x65), (int16_t)0xFCAB, (int16_t)(Z - 0x2A6), (int16_t)(X - 0xA1), (int16_t)0xFD77, (int16_t)(Z - 0x495) };
			CamLerp(a, b, (int32_t)(((uint32_t)(c - 0x12) << 12) / 25));
		}
		if ((uint32_t)(c - 0x2B) < 0x26)
		{
			const int16_t a[6] = { (int16_t)(X - 0x85), (int16_t)0xFCAE, (int16_t)(Z - 0x2B1), (int16_t)(X + 0x21F), (int16_t)0xFDD3, (int16_t)(Z - 0x6B8) };
			const int16_t b[6] = { (int16_t)(X - 7), (int16_t)0xFE73, (int16_t)(Z - 0x523), (int16_t)(X + 0x1F), (int16_t)0xFB53, (int16_t)(Z - 0x93A) };
			CamLerp(a, b, (int32_t)(((uint32_t)(c - 0x2B) << 12) / 38));
		}
		if ((uint32_t)(c - 0x51) < 0x1C)
		{
			const int32_t e = c - 0x51;
			if (e < 6 && engine) var<int16_t>(0x1D97712) = ShakeY(e);
			const int16_t a[6] = { (int16_t)(X - 7), (int16_t)0xFE73, (int16_t)(Z - 0x523), (int16_t)(X + 0x1F), (int16_t)0xFB53, (int16_t)(Z - 0x93A) };
			const int16_t b[6] = { (int16_t)(X - 0x20), (int16_t)0xFDF2, (int16_t)(Z - 0x4D4), (int16_t)(X - 0x2D1), (int16_t)0xFC9D, (int16_t)(Z - 0x12E0) };
			CamLerp(a, b, (int32_t)(((uint32_t)e << 12) / 28));
		}
		if ((uint32_t)(c - 0x6D) < 0x19)
		{
			const int16_t cx = Centre()[0], cz = Centre()[2];
			const int16_t a[6] = { cx, 0, cz, cx, (int16_t)0xFAC2, (int16_t)(cz + 0x2AB8) };
			const int16_t b[6] = { cx, 0, cz, cx, (int16_t)0xFDE8, (int16_t)(cz + 0x1BFC) };
			CamLerp(a, b, (int32_t)(((uint32_t)(c - 0x6D) << 12) / 25));
			return;
		}
		if (c == 0x86)
		{
			CamLookAt()[0] = (int16_t)(X - 0x2A3);
			CamLookAt()[1] = (int16_t)0xF74E;
			CamLookAt()[2] = (int16_t)(Z - 0x122E);
			CamEye()[0] = (int16_t)(X + 0x375);
			CamEye()[1] = (int16_t)0xFF59;
			CamEye()[2] = (int16_t)(Z + 0x423);
		}
	}

	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		// 30 fps layer: see mag212_burning_rave_held.inc
		FX_HELD(held_note_director(d);)
		const int16_t c = ++d->counter;
		if (c == 0xB6) return TASK_END;
		if (c == 0xAE)
		{
			// the damage; the targets put back (position, bits 0x0C), the enemies' bit 4, Zell's position / angles, the streams
			ApplyActionResultToTargets(Targets(), (uint32_t)TargetCount());
			for (int32_t i = 0; i < TargetCount(); i++)
			{
				TargetRec &t = d->recs[i];
				U32(t.e, 0x1C) = t.pos01;
				*(uint16_t *)t.e &= 0xFFF3;
				U32(t.e, 0x20) = t.pos23;
			}
			RestoreEnemyFlag4();
			U32(Caster(), 0x1C) = d->saved_pos[0];
			U32(Caster(), 0x20) = d->saved_pos[1];
			U32(Caster(), 0xC) = d->saved_rot[0];
			U32(Caster(), 0x10) = d->saved_rot[1];
			StreamQueueLoad(0x168, TexBase() + 0x1C18, 1);
			StreamQueueLoad(0x169, TexBase() + 0x1C18, 0);
			SetScreenFlash(0, 0);
			return 0;
		}
		if (c > 0xAE) return 0;
		SetScreenFlash((uint32_t)FlashLevel(c), 0);
		// the layouts' parameter block (one per call: blocks keep the fields they do not set)
		PlayArg arg;
		memset(&arg, 0, sizeof(arg));
		const uint32_t morph = TexBase() + 0xA418;

		// 2..0x39: the ground layout at his anchor (texture scrolling), rocks 2..0x29
		const int32_t s = c - 2;
		if ((uint32_t)s < 0x38)
		{
			if (s == 0)
			{
				DecodeModelPrimLayout(LAYOUT_Ground, d->burn_l, 0x190);
				GetDefaultEffectPosition(Caster(), d->anchor);
			}
			RotationY(0, &arg.m);
			arg.m.t[0] = d->anchor[0];
			arg.m.t[1] = 0;
			arg.m.t[2] = d->anchor[2];
			arg.morph = morph;
			arg.depth = -0x80;
			arg.kind = (const uint8_t *)KIND_Ground;
			arg.pieces = nullptr;
			arg.fade = nullptr;
			*(uint16_t *)&arg.scroll = (uint16_t)(s << 5);
			Play(d, SITE_GROUND, d->burn_l, &arg);
			if ((uint32_t)s < 0x28) SpawnRocks(1, d->anchor[0], 0, d->anchor[2], 5, 0xF, 0x1F4, 0x1F4, 0x3E8, 0x3E8, ORIG_RockTask);
		}
		// 0x11..0x2C: sparks from his fists (odd: bones 0xD / 8, the fist point 0x11AC240; even: 0xE / 0xA, 0x11AC248)
		if ((uint32_t)(c - 0x11) < 0x1C)
		{
			int16_t s1[4], s2[4], s3[4];
			if ((c - 0x11) & 1)
			{
				GetEffectSpawnPosition(Caster(), 0xD, 0, s1);
				GetEffectSpawnPosition(Caster(), 8, 0, s2);
				FistPoint(FIST_A, s3);
			}
			else
			{
				GetEffectSpawnPosition(Caster(), 0xE, 0, s1);
				GetEffectSpawnPosition(Caster(), 0xA, 0, s2);
				FistPoint(FIST_B, s3);
			}
			s3[1] = (int16_t)(s3[1] - 0x64);
			// (the two bone points are extrapolated (2 s1 - s2, x and z) into the frame and never read)
			const int32_t vy = -4 - (CrtRand() & 3);
			SpawnSprites(1, 0x10, s3[0], s3[1], s3[2], 0x64, -0x32, 0, vy, 0, 0xFF, 0x320, ORIG_SparkTask);
		}
		// 0x51..0x68: a layout at his bone 0x16
		if ((uint32_t)(c - 0x51) < 0x18)
		{
			if (c == 0x51) DecodeModelPrimLayout(LAYOUT_Bone, d->bone_l, 0x44);
			int16_t p[4];
			GetEffectSpawnPosition(Caster(), 0x16, 0, p);
			RotationY(0, &arg.m);
			arg.m.t[0] = p[0];
			arg.m.t[1] = p[1];
			arg.m.t[2] = p[2];
			arg.morph = morph;
			arg.depth = -0x80;
			arg.kind = (const uint8_t *)KIND_Bone;
			arg.pieces = nullptr;
			arg.fade = nullptr;
			Play(d, SITE_BONE, d->bone_l, &arg);
		}
		// 0x51..0x5C: the glow at his bone 18
		if ((uint32_t)(c - 0x51) < 0xC) DrawGlow(c - 0x51);
		// 0x54..0x6D: a layout before him
		if ((uint32_t)(c - 0x54) < 0x1A)
		{
			if (c == 0x54) DecodeModelPrimLayout(LAYOUT_Front, d->front_l, 0x230);
			RotationY(0, &arg.m);
			arg.m.t[1] = 0;
			arg.m.t[0] = *(const int16_t *)(Caster() + 0x1C) - 0xC3;
			arg.m.t[2] = *(const int16_t *)(Caster() + 0x20) - 0x48F;
			arg.morph = morph;
			arg.depth = -0x100;
			arg.kind = (const uint8_t *)KIND_Front;
			arg.pieces = nullptr;
			arg.fade = nullptr;
			Play(d, SITE_FRONT, d->front_l, &arg);
		}
		// 0x5E..0xA3: the burning layout at the box (Zell drawn by the module and hidden until 0x6C), at its far end from 0x6D
		// (the stage captured anew, Zell hidden, the layout decoded again; debris pieces); 0xA4: the targets shown
		const int32_t f = c - 0x5E;
		if ((uint32_t)f < 0x46)
		{
			if (f == 0) DecodeModelPrimLayout(LAYOUT_Burn, d->burn_l, 0x484);
			RotationY(0x800, &arg.m);
			if ((uint32_t)f < 0xF)
			{
				DrawCaster();
				arg.m.t[0] = d->box[0];
				arg.m.t[1] = 0;
				arg.m.t[2] = d->box[2];
				arg.pieces = (const uint8_t *)KIND_Burn;
			}
			else
			{
				if (f == 0xF)
				{
					int16_t box_a[6], box_b[6];
					ModelBox(LAYOUT_Burn + *(const uint32_t *)(LAYOUT_Burn + 8), box_a);
					ModelBox(LAYOUT_Burn + *(const uint32_t *)(LAYOUT_Burn + 0xC), box_b);
					box_a[3] = box_b[3];
					CaptureStage(&d->box[4], box_a);
					B8(Caster(), 0) |= 4;
					DecodeModelPrimLayout(LAYOUT_Burn, d->burn_l, 0x484);
				}
				arg.m.t[0] = d->box[4];
				arg.m.t[1] = 0;
				arg.m.t[2] = d->box[6];
				arg.pieces = (const uint8_t *)KIND_BurnB;
			}
			arg.depth = 0;
			arg.depth2 = 0x180;
			arg.morph = morph;
			arg.kind = (const uint8_t *)KIND_Burn;
			arg.fade = (const uint8_t *)KIND_BurnB;
			Play(d, SITE_BURN, d->burn_l, &arg);
		}
		else if (f == 0x46)
		{
			for (int32_t i = 0; i < TargetCount(); i++) *(uint16_t *)d->recs[i].e &= 0xFFFB;
			RestoreEnemyFlag4();
		}
		// 0x6D..0x85: the targets drawn by the module (zero angles / offset) and hidden from the engine's draw
		if ((uint32_t)(c - 0x6D) < 0x19)
		{
			for (int32_t i = 0; i < TargetCount(); i++)
			{
				DrawEntity(d->recs[i].e, ZeroAngles(), ZeroAngles());
				B8(d->recs[i].e, 0) |= 4;
			}
		}
		// 0x86..0xAE: the targets thrown up (+0x1E) and spun (y by k, x by 2 k from Zell's angles), drawn by the module about
		// their middle; sprites from their bones from 0x96
		const int32_t t = c - 0x86;
		if ((uint32_t)t <= 0x28)
		{
			const int32_t k = (int32_t)(((uint32_t)t << 12) / 40);
			int16_t off[3];
			off[2] = 0;
			off[0] = 0;
			int32_t y;
			if (k < 0x800)
			{
				y = -ComputeSin(k >> 1);
			}
			else
			{
				y = 0x1000 - ComputeSin(k >> 1);
				y = mul32(y, y) >> 12;
				y = mul32(y, y) >> 12;
				y -= 0x1000;
			}
			if (t == 0)
			{
				// (the loop index lives in the director's argument slot)
				for (int32_t i = 0; i < TargetCount(); i++)
				{
					uint8_t *e = d->recs[i].e;
					MeasureModelHeight(e, d->recs[i].height);
					B8(e, 0) |= 8;
				}
			}
			for (int32_t i = 0; i < TargetCount(); i++)
			{
				TargetRec &r = d->recs[i];
				r.ang[0] = (int16_t)(*(const int16_t *)(Caster() + 0xC) + k);
				r.ang[1] = (int16_t)(*(const int16_t *)(Caster() + 0xE) + 2 * k);
				W16(r.e, 0x1E) = (int16_t)(y + (int16_t)(r.pos01 >> 16));
				off[1] = (int16_t)(((int32_t)r.height[1] + (int32_t)r.height[0]) >> 1);
				DrawEntity(r.e, r.ang, off);
				B8(r.e, 0) |= 4;
			}
			if ((uint32_t)t > 0xF)
			{
				for (int32_t i = 0; i < TargetCount(); i++)
				{
					uint8_t *e = d->recs[i].e;
					SpriteNode *sp = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_EmberTask);
					if (!sp) break;
					const int32_t r = SharedRand();
					const uint8_t bones = (**(uint8_t ***)(e + 0x64))[0];
					GetEffectSpawnPosition(e, mul32(r, bones) >> 15, 0, sp->pos);
					const int32_t r2 = CrtRand();
					sp->vel[0] = 0;
					sp->vel[1] = (int8_t)0xF1;
					sp->size = (int16_t)((shl32(r2, 11) >> 15) + 0x1000);
					sp->vel[2] = 0;
					sp->life = 0x10;
				}
			}
		}
		// the afterimages of Zell: drawn 0x41..0x50, recorded 0x40..0x4A (274's 0x68ADF0)
		if (c > 0x40 && c <= 0x50) GhostDraw(Caster(), d->ghosts);
		if (c >= 0x40 && c <= 0x4A) GhostRecord(Caster(), d->ghosts);
		if (c == 5) BdPlaySE(SOUND_A, 0, 0x80);
		else if (c == 0x43) BdPlaySE(SOUND_B, 0, 0x80);
		// 0x86..0x8F: rocks falling at the far end (the rock pool reset at 0x86), an ember and a flame rising there
		if ((uint32_t)t < 0xA)
		{
			if (t == 0) InitTaskQueuePool(QRocks(), (void *)(TexBase() + 0xCD0), 0x24, 0x40);
			// (the task function lives in the director's argument slot; (rand x 3) >> 15 is 0..2: always set)
			uint32_t fn = ORIG_RockATask;
			for (int32_t i = 2; i != 0; i--)
			{
				switch ((CrtRand() * 3) >> 15)
				{
				case 0: fn = ORIG_RockATask; break;
				case 1: fn = ORIG_RockBTask; break;
				case 2: fn = ORIG_RockCTask; break;
				}
				RockNode *k = (RockNode *)AddTaskToQueue(QRocks(), fn);
				if (!k) break;
				*(uint32_t *)k->acc = 0;
				k->acc[1] = 0x32;
				k->life = 0xF;
				k->scale = (int16_t)((CrtRand() & 0x7FB) + 0x400);
				const int32_t rx = CrtRand() & 0xFFF;
				k->pos[1] = 0x400;
				k->pos[0] = (int16_t)(rx + d->box[4] - 0x800);
				k->vel[2] = 0;
				k->pos[2] = (int16_t)(d->box[6] - 0x1428);
				k->vel[0] = 0;
				k->vel[1] = (int16_t)(-0x200 - (CrtRand() & 0x1FF));
				k->rx = (uint8_t)CrtRand();
				k->ry = (uint8_t)CrtRand();
				k->spin[0] = (int8_t)CrtRand();
				k->spin[1] = (int8_t)CrtRand();
			}
			for (int pass = 0; pass < 2; pass++)
			{
				SpriteNode *sp = (SpriteNode *)AddTaskToQueue(QSprites(), pass == 0 ? ORIG_EmberTask : ORIG_FlameTask);
				if (!sp) continue;
				sp->pos[0] = (int16_t)(CrtRand() * 4700 / 0x8000 + d->box[4] - 0x92E);
				sp->pos[1] = 0;
				sp->pos[2] = (int16_t)(d->box[6] - 0x1028);
				sp->size = (int16_t)((shl32(CrtRand(), 11) >> 15) + 0x1000);
				sp->vel[0] = 0;
				sp->vel[1] = (int8_t)0x81;
				sp->vel[2] = 0;
				sp->life = pass == 0 ? 0x10 : 0xC;
			}
		}
		// the camera: on 2 the targets, the enemies and the party hidden, Zell shown
		if (c == 2)
		{
			for (int32_t i = 0; i < TargetCount(); i++) *(uint16_t *)d->recs[i].e |= 4;
			SetEnemyFlag4();
			*(uint16_t *)Entity(0) |= 4;
			*(uint16_t *)Entity(1) |= 4;
			*(uint16_t *)Entity(2) |= 4;
			*(uint16_t *)Caster() &= 0xFFFB;
		}
		DirectorCamera(c, true);
		return 0;
	}

	// ------------------------------------------------------------------
	// RootTask (0x6A0DC0): node 0x10 (pool 0x2510248)
	// ------------------------------------------------------------------
	static void StartDirector(DirectorNode *d)
	{
		Memset32(&d->counter, 0, 0x331);
		int16_t mn[3] = { 0x7FFF, 0x7FFF, 0x7FFF };
		int16_t mx[3] = { (int16_t)0x8001, (int16_t)0x8001, (int16_t)0x8001 };
		for (int32_t i = 0; i < TargetCount(); i++)
		{
			TargetRec &r = d->recs[i];
			r.e = Entity(Targets()[i * 0x18]);
			r.pos01 = U32(r.e, 0x1C);
			r.pos23 = U32(r.e, 0x20);
			*(uint32_t *)&r.ang[0] = *(const uint32_t *)ZeroAngles();
			*(uint32_t *)&r.ang[2] = *(const uint32_t *)(ZeroAngles() + 2);
			int16_t p[4];
			GetDefaultEffectPosition(r.e, p);
			for (int k = 0; k < 3; k++)
			{
				if (p[k] < mn[k]) mn[k] = p[k];
				if (p[k] > mx[k]) mx[k] = p[k];
			}
		}
		d->centre[2] = mx[2];
		const int16_t cx = (int16_t)(((int32_t)mx[0] + (int32_t)mn[0]) / 2);
		d->centre[0] = cx;
		d->centre[1] = (int16_t)(((int32_t)mx[1] + (int32_t)mn[1]) / 2);
		d->box[0] = cx;
		d->box[1] = 0;
		d->box[2] = (int16_t)(*(const int16_t *)(Caster() + 0x20) - 0x1428);
		d->box[4] = cx;
		d->box[5] = 0;
		d->box[6] = (int16_t)(mx[2] + 0x10CC);
		int16_t box_a[6], box_b[6];
		ModelBox(LAYOUT_Burn + *(const uint32_t *)(LAYOUT_Burn + 8), box_a);
		ModelBox(LAYOUT_Burn + *(const uint32_t *)(LAYOUT_Burn + 0xC), box_b);
		box_a[3] = box_b[3];
		CaptureStage(d->box, box_a);
		uint8_t *z = Caster();
		d->saved_pos[0] = U32(z, 0x1C);
		d->saved_pos[1] = U32(z, 0x20);
		d->saved_rot[0] = U32(z, 0xC);
		d->saved_rot[1] = U32(z, 0x10);
		W16(z, 0x1C) = d->centre[0];
		W16(z, 0xE) = 0;
		CamAnchor()[0] = U32(z, 0x1C);
		CamAnchor()[1] = U32(z, 0x20);
		Centre()[0] = d->centre[0];
		Centre()[1] = 0;
		Centre()[2] = d->centre[2];
		static const int32_t bones[3] = { 0xE, 0x12, 0x16 };
		GhostInit(d->ghosts, 3, bones);
		BindExternalAnimSet(z, d->bind, COM_Anim);
		QueueChainTransformation(z, 1);
		StreamStateInit((void *)(TexBase() + 0x1BD0));
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag212_burning_rave_held.inc
		FX_HELD(held_note_root(r);)
		if (r->parity)
		{
			PacketCursor() = TexBase() + 0xAAE0;
			CaptureCursor() = TexBase() + 0x1AAE0;
			r->parity = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x1AAE0;
			CaptureCursor() = TexBase() + 0x2AAE0;
			r->parity = 1;
		}
		if (r->counter == 1)
		{
			if (r->state == 0)
			{
				r->state = 1;
				InitTaskQueuePool(QDirector(), (void *)TexBase(), 0xCD0, 1);
				InitTaskQueuePool(QRocks(), (void *)(TexBase() + 0xCD0), 0x24, 0x40);
				InitTaskQueuePool(QSprites(), (void *)(TexBase() + 0x15D0), 0x18, 0x40);
				SaveFlags();
			}
			DirectorNode *d = (DirectorNode *)AddTaskToQueue(QDirector(), ORIG_DirectorTask);
			if (d) StartDirector(d);
		}
		if (r->state == 0)
		{
			r->counter++;
			return 0;
		}
		const int a = ExecuteTaskQueue(QDirector());
		ExecuteTaskQueue(QRocks());
		const int s = ExecuteTaskQueue(QSprites());
		if (a != 0)
		{
			r->counter++;
			return 0;
		}
		// the director ended: wait for the sprites and the streams, put the camera back, then show the party and end
		StreamPump();
		if (s != 0) return 0;
		if (StreamPreload(1) == 0) return 0;
		if (r->state == 1)
		{
			var<int16_t>(0x1D8E038) = var<int16_t>(0x1D977A0);
			memcpy((void *)0xB8B7F0, (const void *)0xB8B800, 0x10);
			r->state = 2;
			return 0;
		}
		if (r->state == 2)
		{
			*(uint16_t *)Entity(0) &= 0xFFFB;
			*(uint16_t *)Entity(1) &= 0xFFFB;
			*(uint16_t *)Entity(2) &= 0xFFFB;
			return TASK_END;
		}
		return 0;
	}
}

	void register_mag212_burning_rave()
	{
		register_port(rave212::ORIG_RootTask, (void *)rave212::RootTask, "212 RootTask", 212);
		register_port(rave212::ORIG_DirectorTask, (void *)rave212::DirectorTask, "212 DirectorTask", 212);
		register_port(rave212::ORIG_RockTask, (void *)rave212::RockTask, "212 RockTask", 212);
		register_port(rave212::ORIG_RockATask, (void *)rave212::RockATask, "212 RockATask", 212);
		register_port(rave212::ORIG_RockBTask, (void *)rave212::RockBTask, "212 RockBTask", 212);
		register_port(rave212::ORIG_RockCTask, (void *)rave212::RockCTask, "212 RockCTask", 212);
		register_port(rave212::ORIG_PieceTask, (void *)rave212::PieceTask, "212 PieceTask", 212);
		register_port(rave212::ORIG_SparkTask, (void *)rave212::SparkTask, "212 SparkTask", 212);
		register_port(rave212::ORIG_EmberTask, (void *)rave212::EmberTask, "212 EmberTask", 212);
		register_port(rave212::ORIG_FlameTask, (void *)rave212::FlameTask, "212 FlameTask", 212);
		// 30 fps layer: see mag212_burning_rave_held.inc
		FX_HELD(register_mag212_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag212_burning_rave_held.inc"
#endif
