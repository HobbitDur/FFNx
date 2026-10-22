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

// Effect 215: My Final Heaven (Zell's Duel finisher; MAG_215_*). The Duel (effect 274) hands over to it when the player enters
// the finisher's input; the battle casts it with Zell as the caster.
//
// Structure (setup 0x69AB60 -> _Init 0x69AB90 (not ported: the setup: TIM upload, magic buffer 0x25089A8, caster, root queue),
// file loader 0x69AB70 (the module file 0x1172B1C)):
//   RootTask (0x69AC00) - node 0x10 (pool 0x2508978): the packet cursor alternates between magic buffer + 0x10518 / + 0x22518
//     (a second cursor 0x250898C + 0x22518 / + 0x34518, never used); on counter 1 sets up the director (1 x 0x1400 at the magic
//     buffer) and the ring queue (0x24 x 0x294 at + 0x1400) and starts the director: the targets' records (entity, position,
//     angles, zero angles), the box of their anchors, the centre x moved clear of the targets (0x69D500), Zell placed there
//     0x1000 before the far z, facing 0, his position / angles saved, the camera anchors 0x2508998..0x25089A4, the debris pool
//     (0x69B040), his animation set (0x5022C0, com 0x116D1AC) and chain 1, the stream state, the party hidden, Zell shown; then
//     runs the director and the rings; when the director ends it waits for the streams (0x5342C0(2)), puts the targets / Zell
//     back (chain 2), clears Zell's bit 8, puts the saved view back and ends.
//   DirectorTask (0x69B070) - node 0x1400, counters 0..0x119 (the counter steps last; UPDATE AND DRAW in one call per tick):
//     the screen flash (rising 0..0xB, 0x8FC, falling 0x10E..0x119); 1..0x3D a layout at Zell's anchor (0x11550A8, camera-
//     aligned: 0x69C6B0); 0x3E..0x4B a layout at Zell walking back 0x100 a tick (0x1168A5C), rings at bones 0x14 / 0x13; 0x4C..
//     0x73 a layout at Zell running (0x1160BB0; Zell placed 0x1800 behind the centre, then 0x4CC a tick; the targets turn to
//     him from 0x60); 0x74..0xA3 a screen layout (0x11625E0; the targets' angles put back); 0xA4..0xB5 a layout far away
//     (0x1169844); 0xB6..0xDD the jump: Zell drawn by the module (0x69D100) and moved, debris (0x69CC10) and rings each tick,
//     rings on the targets at 0xD0, white flash 0xC0..0xD0, layouts 0x116703C at Zell and 0x115DCCC at his bone 0x16;
//     0xDE..0xFD the crater layout (0x116AD0C); 0xDE..0x118 the targets thrown up and spun / put down (0x69D200, heights
//     0x69D360); 0x18..0x73 a layout at his bone 0x16 (0x115A910, camera-aligned); the debris pool drawn (0x69CC90) and moved
//     (0x69D080) every tick; sounds at 5 / 0xA2; the damage at 0x108; 0xA3 the targets shown and moved by the centre; the camera
//     (0x69BEBB..0x69C18E); 0x11A the end (the targets' bits 0x0C and the party's bit 4 cleared, flash off).
//   RingTask (0x69CB10) - node 0x294: a layout (0x115C4C8 / 0x115CA08) at its point, turned / scaled about Y (0x6ED1E0), until
//     the layout ends.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Writes to battle entities: Zell (caster): position +0x1C / +0x20 (start; 0x3E..0x4B, 0x4C..0x55, 0xB6..0xD2), world
//   translation +0x54..+0x5C (every director tick: his position; +0x5C at 0x4C), facing +0x0E (start: 0), flags bit 4
//   (0xB6..0xD3 hidden, drawn by the module), bit 8 (0x56..0xBF; cleared by the root at the end), his position / angles put back
//   by the root, his animation set / chains 1, 2; the targets: facing +0x0E (0x60..0x73 turning to him), angles +0x0C..+0x13
//   (put back at 0x74), flags bit 4 (0x74..0xA2, 0xDE..0x118), bit 8 (0xDE..0x105), +0x1E height (0xDE..0x118), +0x20 moved by
//   -centre z at 0xA3, bits 0x0C cleared at the end; positions / angles put back by the root; the party (slots 0..2) bit 4 (set
//   at the start, cleared at the end); the screen flash words' bit 2 (0x74 cleared, 0xC0 set). Damage: 0x506BA0 at 0x108.
// CAMERA (eye 0xB8B7F0, look-at 0xB8B7F8): 0x50A730 armed, field of view 0x1D8E038 = 0x120, roll 0x1D977A2 = 0 on 0; 0x657DF0
//   lerps 0..0x17 (with the roll), set on 0x3D (roll ramp 0x3D..0x4A), set on 0x4B, set on 0xA3 (roll 0x200), lerps 0xDD..0xE0
//   (roll - 0x80 a tick); the root puts back the saved view (0xB8B800) and the field of view (0x1D977A0) at its end.
// Module globals: 0x2508938..0x25089AC (module file 0x2508938, context 0x250893C, caster 0x2508940, queues 0x2508948 rings /
//   0x2508958 director / 0x2508968 root, root pool 0x2508978, packet cursors 0x2508988 / 0x250898C, zero angles 0x2508990 (never
//   written), camera anchors 0x2508998 / 0x250899C (Zell's +0x1C / +0x20 dwords at the start), 0x25089A0 / 0x25089A4 (the centre
//   dwords), magic buffer 0x25089A8).

#include "mag_common.h"

namespace ff8fx
{
namespace heaven215
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x250893C); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2508940); }
	inline TaskQueue *QRings() { return (TaskQueue *)0x2508948; }            // rings (0x294)
	inline TaskQueue *QDirector() { return (TaskQueue *)0x2508958; }         // the director (1 x 0x1400)
	inline TaskQueue *QRoot() { return (TaskQueue *)0x2508968; }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2508988); }
	inline uint32_t &PacketCursor2() { return var<uint32_t>(0x250898C); }    // (set by the root, never used)
	inline const int16_t *ZeroAngles() { return (const int16_t *)0x2508990; }
	inline uint32_t *CamAnchor() { return (uint32_t *)0x2508998; }          // Zell's +0x1C / +0x20 dwords at the start
	inline uint32_t *CentreWords() { return (uint32_t *)0x25089A0; }        // the centre's +0x180 / +0x184 dwords
	inline uint32_t &TexBase() { return var<uint32_t>(0x25089A8); }          // Magic_TextureOFF (magic buffer)
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }
	inline uint32_t &FramePackets() { return var<uint32_t>(0x1D8E054); }
	inline int16_t *CamEye() { return (int16_t *)0xB8B7F0; }
	inline int16_t *CamLookAt() { return (int16_t *)0xB8B7F8; }
	inline int16_t &CamRoll() { return var<int16_t>(0x1D977A2); }
	inline int16_t &ViewWord() { return var<int16_t>(0x1D8E038); }

	static const uint32_t ORIG_RootTask = 0x69AC00;
	static const uint32_t ORIG_DirectorTask = 0x69B070;
	static const uint32_t ORIG_RingTask = 0x69CB10;
	static const uint32_t ORIG_CallbackA = 0x69C220;
	static const uint32_t ORIG_CallbackB = 0x69C6B0;

	static const uint32_t COM_Anim = 0x116D1AC;
	static const uint32_t LAYOUT_Anchor = 0x11550A8, LAYOUT_Back = 0x1168A5C, LAYOUT_Run = 0x1160BB0, LAYOUT_Screen = 0x11625E0;
	static const uint32_t LAYOUT_Far = 0x1169844, LAYOUT_Jump = 0x116703C, LAYOUT_JumpBone = 0x115DCCC, LAYOUT_Crater = 0x116AD0C;
	static const uint32_t LAYOUT_Bone = 0x115A910, LAYOUT_Ring = 0x115C4C8, LAYOUT_TargetRing = 0x115CA08;
	static const uint32_t KIND_Anchor = 0x1172B34, KIND_Walk = 0xFE0FC8, KIND_Screen = 0x1172B2C, KIND_Plain = 0x1154F44;
	static const uint32_t KIND_Jump = 0x1172B28, KIND_Crater = 0x1154F40, KIND_Ring = 0xFE0FB4;
	static const uint32_t MODEL_Debris = 0x1154F58;           // the debris model (vertices; its triangle list at [0x1154F50] + 0x1154F60)
	static const int16_t *const SINCOS = (const int16_t *)0x116EB1C; // 4096 pairs (sin, cos)
	static const void *const SOUND_A = (const void *)0x1154F48;
	static const void *const SOUND_B = (const void *)0x1154F4C;

	// ---- engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline uint32_t BindExternalAnimSet(uint8_t *entity, void *out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, void *, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationY(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(a, out); }   // Effect_RotationYMatrix
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3x3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }
	inline void Matrix3x3MultiplyPSX(const Mat4x3 *a, const Mat4x3 *b, Mat4x3 *out) { fn<void (__cdecl *)(const Mat4x3 *, const Mat4x3 *, Mat4x3 *)>(0x56C090)(a, b, out); } // (out pad not written)
	inline void Apply3DTransform(const Mat4x3 *m, const int16_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int32_t *)>(0x56C3D0)(m, in, out); }
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); } // MAG_063_RenderPrimSet
	inline uint32_t DrawShadow(uint8_t *e, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(e, ot, a, cursor); } // BattleEntity_DrawShadow
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, ctx, ot, a, cursor); }
	inline int32_t SqrtScaled(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BED0)(v); }    // (int)(sqrt(v * k1) * k2)
	inline void CamLerp(const int16_t *a, const int16_t *b, int32_t t) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t)>(0x657DF0)(a, b, t); } // MAG_325_CameraLerp
	inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }   // BS_Camera_ArmReturnAfterEffect
	inline void StreamQueueLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamPreload(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); } // au_re_BattleFile_preLoad_0
	// GTE (software PSX GTE) helpers not in fx_port.h
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }      // IR = MAC = R * V0
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }      // IR = R * IR
	inline void GteLoadIRFromColumn(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E180)(p); }
	inline void GteStoreIRToColumn(void *p) { fn<void (__cdecl *)(void *)>(0x45E470)(p); }
	inline void GteLoadV0FromDwords(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E060)(p); }
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *p) { fn<void (__cdecl *)(void *)>(0x45E210)(p); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteReadMAC0(void *p) { fn<void (__cdecl *)(void *)>(0x45E3C0)(p); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteReadOTZ32(void *p) { fn<void (__cdecl *)(void *)>(0x45E3D0)(p); }      // GTE_ReadOTZ (a dword)
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); }    // GTE_LoadV0_u16
	inline void GteReadIR2Word(void *p) { fn<void (__cdecl *)(void *)>(0x703F90)(p); }    // GTE_ReadIR2Word

	// layout sites (director node offsets of the decoded layouts, with the phase that plays them)
	enum : int { SITE_ANCHOR = 1, SITE_BACK, SITE_RUN, SITE_SCREEN, SITE_FAR, SITE_JUMP, SITE_JUMPBONE, SITE_CRATER, SITE_BONE };

#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes at 0x2508978
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t state;     // +0x0E 0 = not set up, 1 = running
		uint8_t parity;    // +0x0F packet arena parity
	};
	struct TargetRec // 0x20 bytes from the director +0x40
	{
		uint8_t *e;        // +0x00
		uint32_t pos01;    // +0x04 the entity's +0x1C / +0x1E at the start
		uint32_t pos23;    // +0x08 +0x20 / +0x22
		uint32_t rot01;    // +0x0C +0x0C / +0x0E
		uint32_t rot23;    // +0x10 +0x10 / +0x12
		int16_t ang[4];    // +0x14 its spin (0xDE..0x105; from the zero angles 0x2508990 at the start)
		int16_t height[2]; // +0x1C its model top / bottom (0x69D360 at 0xDE and 0x106..0x118)
	};
	struct Debris // 0x16 bytes, 0x80 of them at the director +0x8FC
	{
		int16_t pos[3];    // +0x00
		int16_t life;      // +0x06 0 = free
		int16_t vel[3];    // +0x08 (gravity 0xA a tick, bounce on y >= 0)
		int16_t scale;     // +0x0E
		int16_t rx, ry;    // +0x10 angles (spin x 4 a tick)
		int8_t spin[2];    // +0x14
	};
	struct DirectorNode // pool of 1 node of 0x1400 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..0x119 (steps last)
		int16_t pad0E;
		uint8_t bind[0x20];// +0x10 Zell's bind block
		uint32_t saved_pos[2]; // +0x30 Zell's +0x1C..+0x23
		uint32_t saved_rot[2]; // +0x38 Zell's +0x0C..+0x13
		TargetRec recs[10];// +0x40
		int16_t centre[3]; // +0x180 the centre x (moved clear of the targets), 0, the far z
		int16_t pad186;
		uint8_t layout_b[0x188]; // +0x188 the walk / run / screen / far / jump layouts
		uint8_t layout_c[0x27C]; // +0x310 the bone 0x16 layouts (0x115A910 / 0x115DCCC)
		uint8_t layout_a[0x370]; // +0x58C the anchor / crater layouts (0x11550A8 / 0x116AD0C)
		Debris debris[0x80];     // +0x8FC
		Debris *cursor;          // +0x13FC the next free debris to try
	};
	// the prim-player callback 0x69C220's parameter block (part of the director's / ring task's stack frame)
	struct ArgA
	{
		Mat4x3 m;          // +0x00 the layout's frame (the view composed)
		const uint8_t *kind; // +0x20 per object: 0x31 / 0x32 / 0x33 texture windows (scrolled, -scrolled), 0x39 the wave (0x69C5A0)
		int16_t depth;     // +0x24 header +0x18
		int16_t scroll;    // +0x26 texture v (x 0x7F) / the wave's growth
		int16_t fade;      // +0x28 non-zero: faded to black by it
		int16_t pad2A;
		uint32_t morph;    // +0x2C blended vertex frames
	};
	// the camera-aligned prim-player callback 0x69C6B0's parameter block
	struct ArgB
	{
		int16_t pos[3];    // +0x00 the world anchor
		int16_t pad06;
		const uint8_t *kind; // +0x08
		int16_t depth;     // +0x0C
		int16_t scroll;    // +0x0E
		int16_t fade;      // +0x10
		int16_t pad12;
		uint32_t morph;    // +0x14
	};
	struct RingNode // 0x294 (pool of 0x24)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t scale;     // +0x12 its Y scale (0x6ED1E0)
		int16_t angle;     // +0x14 its turn about Y
		int16_t pad16;
		uint8_t layout[0x27C]; // +0x18
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(DirectorNode) == 0x1400 && sizeof(TargetRec) == 0x20 && sizeof(Debris) == 0x16, "My Final Heaven nodes");
	static_assert(sizeof(ArgA) == 0x30 && sizeof(ArgB) == 0x18 && sizeof(RingNode) == 0x294, "My Final Heaven blocks");
}
}

#ifdef FF8_FX_HELD
#include "mag215_my_final_heaven_held.h"
#endif

namespace ff8fx
{
namespace heaven215
{
	using namespace eng;
	using namespace magc;

	inline int16_t &W16(void *p, int off) { return *(int16_t *)((uint8_t *)p + off); }
	inline uint16_t &U16(void *p, int off) { return *(uint16_t *)((uint8_t *)p + off); }
	inline int32_t &W32(void *p, int off) { return *(int32_t *)((uint8_t *)p + off); }
	inline uint32_t &U32(void *p, int off) { return *(uint32_t *)((uint8_t *)p + off); }
	inline uint8_t &B8(void *p, int off) { return *((uint8_t *)p + off); }
	// signed division by 4096 rounding toward zero (cdq / and 0xFFF / add / sar 12)
	inline int32_t sdiv4096(int32_t v) { return (int32_t)((uint32_t)v + ((uint32_t)(v >> 31) & 0xFFFu)) >> 12; }

	// action 0's target records / count (24 bytes each, +0 = the slot)
	inline uint8_t *Targets() { return Ctx()->actions[0].targets; }
	inline int32_t TargetCount() { return Ctx()->actions[0].target_count; }

	// ------------------------------------------------------------------
	// Matrices
	// ------------------------------------------------------------------
	// 0x6ED1E0 (Moomba's range: My Final Heaven calls it): a rotation about Y by -angle with the Y scale `scale` (rows x / z scaled)
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

	// 0x69CFC0: a rotation about X by `angle` scaled by `scale` (the sin / cos table 0x116EB1C)
	static void ScaledRotX(int32_t angle, int32_t scale, Mat4x3 *out)
	{
		memset(out, 0, 0x14);
		const int16_t *sc = SINCOS + (angle & 0xFFF) * 2;
		const int32_t s = (int16_t)scale;
		const int32_t a = mul32(sc[0], s) >> 12;
		const int32_t b = mul32(sc[1], s) >> 12;
		out->m[0][0] = (int16_t)scale;
		out->m[2][1] = (int16_t)-a;
		out->m[1][1] = (int16_t)b;
		out->m[1][2] = (int16_t)a;
		out->m[2][2] = (int16_t)b;
	}

	// 0x69D030: a rotation about Y by `angle` (the table 0x116EB1C)
	static void RotYTable(int32_t angle, Mat4x3 *out)
	{
		memset(out, 0, 0x14);
		const int16_t *sc = SINCOS + (angle & 0xFFF) * 2;
		out->m[2][0] = sc[0];
		out->m[0][0] = sc[1];
		out->m[1][1] = 0x1000;
		out->m[0][2] = (int16_t)-sc[0];
		out->m[2][2] = sc[1];
	}

	// 0x69C5A0: the wave (kind 0x39): the model's vertices up to n x s / 0x1000 (the rest at 0, 0, 0x4000), the last 0x40 of them
	// drawn toward a neighbour ((i & 7) - 2 vertices back) the closer they are to the front
	static void WaveVertices(uint32_t model, uint8_t *out, int32_t s)
	{
		const int32_t n = *(const int32_t *)(model + 4);
		const int16_t *v = (const int16_t *)(model + 0xC);
		const int32_t k = mul32(n, s) >> 12;
		int16_t *o = (int16_t *)out;
		for (int32_t i = 0; i < n; i++, v += 4, o += 4)
		{
			if (i > k)
			{
				*(uint32_t *)o = 0;
				o[2] = 0x4000; // (the 4th word not written)
				continue;
			}
			if ((uint32_t)(i - k + 0x40) < 0x40)
			{
				const int32_t w = (int32_t)((uint32_t)(-0x40 * i) + (uint32_t)(0x40 * k));
				const int16_t *u = v + (2 - (i & 7)) * 4;
				o[0] = (int16_t)(sdiv4096(mul32((int32_t)v[0] - (int32_t)u[0], w)) + u[0]);
				o[1] = (int16_t)(sdiv4096(mul32((int32_t)v[1] - (int32_t)u[1], w)) + u[1]);
				o[2] = v[2]; // (the 4th word not written)
				continue;
			}
			memcpy(o, v, 8);
		}
	}

	// ------------------------------------------------------------------
	// Prim-player callbacks (one object of a layout, 0x701DD0)
	// ------------------------------------------------------------------
	// the model / vertex frame of a record into the header (+0 model, +4 vertices: a frame, or the blend into `morph`)
	static uint8_t *PartModel(prim::Layout *l, prim::Record *r, uint8_t *h, uint32_t morph)
	{
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
				BlendVertexFrames((uint32_t)model, f0, f1, t, morph);
				*(uint32_t *)(h + 4) = morph;
			}
		}
		return model;
	}

	// the header's fade / texture window by the object's kind (0x31 / 0x32 / 0x33 / 0x39), then the block's fade, then the draw
	static void PartHeader(prim::Record *r, uint8_t *h, int16_t depth, const uint8_t *kind, int16_t scroll, int16_t fade, uint32_t morph)
	{
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = depth;
		U16(h, 0x22) = 0;
		U16(h, 0x20) = 0;
		U16(h, 0x26) = 0;
		U16(h, 0x24) = 0;
		U16(h, 0x2A) = 0x100;
		U16(h, 0x28) = 0x100;
		switch (kind[(int16_t)r->flags_lo])
		{
		case 0x31:
			U16(h, 0x2C) = 0;
			U16(h, 0x2E) = 0;
			U16(h, 0x32) = 0x80;
			U16(h, 0x30) = 0x100;
			U16(h, 0x22) = (uint16_t)((uint8_t)scroll & 0x7F);
			break;
		case 0x32:
			U16(h, 0x2C) = 0;
			U16(h, 0x2E) = 0;
			U16(h, 0x32) = 0x80;
			U16(h, 0x30) = 0x100;
			U16(h, 0x22) = (uint16_t)((uint8_t)(-(uint8_t)scroll) & 0x7F);
			break;
		case 0x33:
			U16(h, 0x2C) = 0;
			U16(h, 0x2E) = 0x80;
			U16(h, 0x32) = 0x80;
			U16(h, 0x30) = 0x100;
			U16(h, 0x22) = (uint16_t)((uint8_t)scroll & 0x7F);
			break;
		case 0x39:
			WaveVertices(*(const uint32_t *)h, (uint8_t *)morph, scroll);
			*(uint32_t *)(h + 4) = morph;
			break;
		}
		if (fade != 0)
		{
			*(uint32_t *)(h + 8) = 0;
			*(int32_t *)(h + 0xC) = fade;
			B8(h, 0x1C) |= 0xC0;
		}
		PacketCursor() = RenderPrimSet(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x6C);
	}

	// the record's offset is loaded into the GTE V0 with its 4th word never written (the stack's residue: the port loads 0)
	static int16_t g_off_pad = 0;

	// 0x69C220: an object at the block's frame (the record's offset through it unless flag 0x200)
	static void __cdecl CallbackA(prim::Layout *l, prim::Record *r, int arg_)
	{
		const ArgA *arg = (const ArgA *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
		PartModel(l, r, h, arg->morph);
		Mat4x3 m;
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		const int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], g_off_pad };
		if (r->flags & 0x200)
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
		PartHeader(r, h, arg->depth, arg->kind, arg->scroll, arg->fade, arg->morph);
	}

	// the residue under the camera-aligned callback's matrix pad (0x56C090 / the 0x200 copy never write it): modelled as 0
	static int16_t g_m2_pad = 0;

	// 0x69C6B0: an object at the block's world anchor, its rotation (and offset unless flag 0x200) in the camera's axes
	static void __cdecl CallbackB(prim::Layout *l, prim::Record *r, int arg_)
	{
		const ArgB *arg = (const ArgB *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
		PartModel(l, r, h, arg->morph);
		Mat4x3 m;
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		const int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], g_off_pad };
		int32_t o32[3];
		if (r->flags & 0x200)
		{
			o32[0] = off[0];
			o32[1] = off[1];
			o32[2] = off[2];
		}
		else Apply3DTransform(&Camera(), off, o32);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
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
				// the rows scaled (x, y, z)
				for (int i = 0; i < 3; i++)
					for (int j = 0; j < 3; j++) m.m[i][j] = (int16_t)(mul32(m.m[i][j], r->scale[i]) >> 12);
			}
		}
		Mat4x3 m2;
		m2.pad = g_m2_pad;
		if (r->flags & 0x200)
		{
			memcpy(&m2, &m, 0x12);
			m2.pad = g_m2_pad;
		}
		else Matrix3x3MultiplyPSX(&Camera(), &m, &m2);
		Apply3DTransform(&Camera(), arg->pos, m2.t);
		m2.t[0] = (int32_t)((uint32_t)m2.t[0] + (uint32_t)Camera().t[0] + (uint32_t)o32[0]);
		m2.t[1] = (int32_t)((uint32_t)m2.t[1] + (uint32_t)Camera().t[1] + (uint32_t)o32[1]);
		m2.t[2] = (int32_t)((uint32_t)m2.t[2] + (uint32_t)Camera().t[2] + (uint32_t)o32[2]);
		GteSetRotMatrix(&m2);
		GteSetTransVector(&m2);
		PartHeader(r, h, arg->depth, arg->kind, arg->scroll, arg->fade, arg->morph);
	}

	// ------------------------------------------------------------------
	// Debris (the director's pool of 0x80 at +0x8FC): flat-shaded chunks (0x69CE00), DRAW THEN UPDATE
	// ------------------------------------------------------------------
	// 0x69B040: every debris free, the cursor at the first
	static void DebrisInit(DirectorNode *d)
	{
		for (int i = 0x7F; i >= 0; i--) d->debris[i].life = 0;
		d->cursor = &d->debris[0];
	}

	// 0x69CC10: a free debris (the cursor's, else the first, else the first free one), its angles / spin random; the cursor moves
	// past it (back to the first after the last)
	static Debris *DebrisAlloc(DirectorNode *d)
	{
		Debris *k = d->cursor;
		if (k->life != 0)
		{
			k = &d->debris[0];
			if (k->life != 0)
			{
				int n = 0x80;
				do
				{
					k++;
					if (--n == 0) return nullptr;
				} while (k->life != 0);
			}
		}
		if (k >= &d->debris[0x7F]) d->cursor = &d->debris[0];
		else d->cursor = k + 1;
		k->rx = (int16_t)CrtRand();
		k->ry = (int16_t)CrtRand();
		k->spin[0] = (int8_t)CrtRand();
		k->spin[1] = (int8_t)CrtRand();
		return k;
	}

	// 0x69CE00: gouraud triangles (POLY_G3) of a model (h[0] vertices, 4 bytes each; h[1] -> count, then 0x14-byte triangles)
	// through the GTE, dropped when off-screen / back-facing (screen 0xA00 x 0x6C0); the block's +0x14.. hold FLAG / MAC0 / OTZ
	static uint32_t TriRender(uint8_t *h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint8_t *v = *(const uint8_t **)h;
		const uint8_t *t = *(const uint8_t **)(h + 4);
		int32_t count = *(const int32_t *)t;
		t += 4;
		*(const uint8_t **)(h + 4) = t;
		if (count <= 0) return pk;
		uint8_t *p = (uint8_t *)pk;
		for (; count != 0; count--, t += 0x14)
		{
			GteLoadV012(v + (uint32_t)U16((void *)t, 4) * 4, v + (uint32_t)U16((void *)t, 6) * 4, v + (uint32_t)U16((void *)t, 8) * 4);
			GteRTPT();
			U32(p, 0) = 0x6000000;
			U32(p, 4) = *(const uint32_t *)t;
			GteReadFLAG(h + 0x14);
			if (U32(h, 0x14) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadSXY012Split(p + 8, p + 0x10, p + 0x18);
			U32(p, 0xC) = *(const uint32_t *)(t + 0xC);
			U32(p, 0x14) = *(const uint32_t *)(t + 0x10);
			GteReadMAC0(h + 8);
			if (W32(h, 8) < 0) continue;
			GteAVSZ3();
			const int16_t x0 = W16(p, 8), x1 = W16(p, 0x10), x2 = W16(p, 0x18);
			const int16_t y0 = W16(p, 0xA), y1 = W16(p, 0x12), y2 = W16(p, 0x1A);
			clip = (x0 < 0 || x0 > 0xA00) ? 1 : 0;
			if (x1 < 0 || x1 > 0xA00) clip |= 2;
			if (x2 < 0 || x2 > 0xA00) clip |= 4;
			if (y0 < 0 || y0 > 0x6C0) clip |= 0x10;
			if (y1 < 0 || y1 > 0x6C0) clip |= 0x20;
			if (y2 < 0 || y2 > 0x6C0) clip |= 0x40;
			if ((clip & 7) == 7 || (clip & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x10);
			InsertPrimAutoDepth(ot + (uint32_t)(W32(h, 0x10) >> shift) * 4, p);
			p += 0x1C;
		}
		*(const uint8_t **)(h + 4) = t;
		return (uint32_t)p;
	}

	// one debris: its X rotation (scaled) by its Y rotation, at its position, under the view; the model 0x1154F58
	static void DebrisDrawOne(const Debris *k, uint8_t *h)
	{
		// 30 fps layer: see mag215_my_final_heaven_held.inc
		FX_HELD(held_note_debris(k);)
		Mat4x3 *m = (Mat4x3 *)h;
		ScaledRotX(k->rx, k->scale, m);
		RotYTable(k->ry, (Mat4x3 *)(h + 0x20));
		GteSetRotMatrixCtrl(m);
		GteLoadIRFromColumn(h + 0x20);
		GteMVMVA_RotIR();
		GteStoreIRToColumn(h);
		GteLoadIRFromColumn(h + 0x22);
		GteMVMVA_RotIR();
		GteStoreIRToColumn(h + 2);
		GteLoadIRFromColumn(h + 0x24);
		GteMVMVA_RotIR();
		GteStoreIRToColumn(h + 4);
		m->t[0] = k->pos[0];
		m->t[1] = k->pos[1];
		m->t[2] = k->pos[2];
		GteSetRotMatrixCtrl(&Camera());
		GteLoadIRFromColumn(h);
		GteMVMVA_RotIR();
		GteStoreIRToColumn(h);
		GteLoadIRFromColumn(h + 2);
		GteMVMVA_RotIR();
		GteStoreIRToColumn(h + 2);
		GteLoadIRFromColumn(h + 4);
		GteMVMVA_RotIR();
		GteStoreIRToColumn(h + 4);
		GteSetTransVectorCtrl(&Camera());
		GteLoadV0FromDwords(m->t);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(m->t);
		GteSetRotMatrixCtrl(m);
		GteSetTransVectorCtrl(m);
		U32(h, 0x40) = MODEL_Debris;
		U32(h, 0x44) = *(const uint32_t *)0x1154F50 + 0x1154F60;
		FramePackets() = TriRender(h + 0x40, OTBase() + 0x44, 2, FramePackets());
	}

	// 0x69CC90: every live debris
	static void DebrisDraw(DirectorNode *d)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		for (int i = 0; i < 0x80; i++)
			if (d->debris[i].life != 0) DebrisDrawOne(&d->debris[i], h);
		FieldFree(0x58);
	}

	// 0x69D080's step of one debris: its angles by its spin x 4, its position by its velocity, the gravity 0xA (on the ground:
	// vy = -vy / 2)
	static void DebrisStep(Debris *k)
	{
		k->rx = (int16_t)(k->rx + ((int32_t)k->spin[0] << 2));
		k->pos[0] = (int16_t)(k->pos[0] + k->vel[0]);
		k->pos[1] = (int16_t)(k->pos[1] + k->vel[1]);
		int16_t vy = (int16_t)(k->vel[1] - 0xA);
		k->ry = (int16_t)(k->ry + ((int32_t)k->spin[1] << 2));
		k->pos[2] = (int16_t)(k->pos[2] + k->vel[2]);
		if (k->pos[1] >= 0) vy = (int16_t)-((int32_t)vy / 2);
		k->vel[1] = vy;
	}

	// 0x69D080: every live debris ages (the one that ends becomes the cursor) or moves
	static void DebrisMove(DirectorNode *d)
	{
		for (int i = 0; i < 0x80; i++)
		{
			Debris *k = &d->debris[i];
			if (k->life == 0) continue;
			if (--k->life == 0)
			{
				d->cursor = k;
				continue;
			}
			DebrisStep(k);
		}
	}

	// ------------------------------------------------------------------
	// Entities drawn by the module
	// ------------------------------------------------------------------
	// 0x69D100: an entity at its world matrix: its shadow (OT + 0x84), its bones under the view, RenderGeometry (mode 6), its pose
	// matrices rebuilt; the same for its weapon (+0x78)
	static void DrawEntityWorld(uint8_t *e)
	{
		// 30 fps layer: see mag215_my_final_heaven_held.inc
		FX_HELD(held_note_world(e);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		FramePackets() = DrawShadow(e, OTBase() + 0x84, 6, FramePackets());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(e + 0x40), (Mat4x3 *)h);
		ComputeBonesWorldMatrices(e + 0x60, h);
		U16(h, 0x38) = 0x140;
		U32(h, 0x24) = var<uint32_t>(0x1D98B3C);
		U16(h, 0x34) = 0;
		U16(h, 0x36) = 0;
		U16(h, 0x3A) = 0xD8;
		U32(h, 0x3C) = U32(e, 0x28);
		U32(h, 0x48) = 0;
		U32(h, 0x40) = U32(e, 0x7C);
		U32(h, 0x30) = var<uint32_t>(0x1D969A8);
		FramePackets() = RenderGeometry(U32(e, 0x64), h + 0x20, OTBase() + 0x44, 6, FramePackets());
		BuildBoneMatricesFromPose(e + 0x60);
		uint8_t *w = *(uint8_t **)(e + 0x78);
		if (w)
		{
			U32(h, 0x40) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(w, h);
			FramePackets() = RenderGeometry(U32(w, 4), h + 0x20, OTBase() + 0x44, 6, FramePackets());
			BuildBoneMatricesFromPose(w);
		}
		FieldFree(0x4C);
	}

	// 0x69D200: an entity drawn by the module: its shadow (OT + 0x84), its bones' world matrices from its world matrix x (rotation
	// `angles`, translation `off` minus the rotated `off`) under the view, RenderGeometry (mode 4), its pose matrices rebuilt; the
	// same for its weapon (+0x78)
	static void DrawEntity(uint8_t *e, const int16_t *angles, const int16_t *off)
	{
		// 30 fps layer: see mag215_my_final_heaven_held.inc
		FX_HELD(held_note_entity(e, angles, off);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x5C);
		Mat4x3 *m = (Mat4x3 *)h;
		FramePackets() = DrawShadow(e, OTBase() + 0x84, 4, FramePackets());
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

	// 0x69D360: an entity's model height (out[0] = the lowest projected y (max), out[1] = the highest (min)) of its visible
	// objects' vertices at its pose with its world translation taken from its position words (put back), then its pose matrices
	// rebuilt; returns out[1]
	static int32_t MeasureHeight(uint8_t *e, int16_t *out)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x24);
		U32(h, 8) = U32(e, 0x54);
		U32(h, 0xC) = U32(e, 0x58);
		U32(h, 0x10) = U32(e, 0x5C);
		W32(e, 0x54) = W16(e, 0x1C);
		W32(e, 0x58) = W16(e, 0x1E);
		W32(e, 0x5C) = W16(e, 0x20);
		ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
		U32(e, 0x54) = U32(h, 8);
		U32(e, 0x58) = U32(h, 0xC);
		U32(e, 0x5C) = U32(h, 0x10);
		W32(h, 4) = (int32_t)0xFFFF0000;
		W32(h, 0) = 0x10000;
		U32(h, 0x20) = U32(e, 0x7C);
		uint8_t *skel = **(uint8_t ***)(e + 0x64);
		U32(h, 0x14) = 0;
		U16(h, 0x18) = 0x1000;
		uint8_t *model = *(uint8_t **)(e + 0x64);
		const uint8_t *geom = *(uint8_t **)(model + 4);
		const int32_t nobj = *(const int32_t *)geom;
		const uint32_t *offs = (const uint32_t *)(geom + 4);
		for (int32_t i = 0; i < nobj; i++)
		{
			const uint8_t *obj = (const uint8_t *)(offs[i] + *(uint32_t *)(model + 4));
			if (!(U32(h, 0x20) & (1u << i))) continue;
			int32_t groups = *(const int16_t *)obj;
			obj += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = *(const int16_t *)obj;
				obj += 2;
				const Mat4x3 *bm = (const Mat4x3 *)(skel + 0x20 + bone * 0x30);
				GteSetRotMatrixCtrl(bm);
				GteSetTransVectorCtrl(bm);
				int32_t nv = *(const int16_t *)obj;
				obj += 2;
				for (; nv != 0; nv--)
				{
					GteLoadV0u(obj);
					GteRTPS();
					obj += 6;
					GteReadIR2Word(h + 0x1C);
					const int32_t y = W16(h, 0x1C);
					if (W32(h, 0) > y) W32(h, 0) = y;
					if (W32(h, 4) < y) W32(h, 4) = y;
				}
			}
		}
		out[0] = W16(h, 4);
		out[1] = W16(h, 0);
		FieldFree(0x24);
		BuildBoneMatricesFromPose(e + 0x60);
		return out[1];
	}

	// 0x69D500: x moved by 0x80 until it is clear of every target taller than `height` (their +0x34 / +0x3A half extents about
	// their x, against x +- size / 2)
	static int32_t ClearX(int32_t x, int32_t size, int32_t height)
	{
		const int32_t half = size >> 1;
		for (;;)
		{
			const int32_t hi = x + half, lo = x - half;
			bool hit = false;
			for (int32_t i = 0; i < TargetCount() && !hit; i++)
			{
				uint8_t *e = Entity(Targets()[i * 0x18]);
				if (W16(e, 0x3C) <= height) continue;
				const int32_t ex = W16(e, 0x1C);
				const int32_t a = (W16(e, 0x34) >> 1) + ex;
				if (a > lo)
				{
					if (a < hi) hit = true;
				}
				else if ((W16(e, 0x3A) >> 1) + ex > lo) hit = true;
			}
			if (!hit) return x;
			x += 0x80;
		}
	}

	// ------------------------------------------------------------------
	// RingTask (0x69CB10): node 0x294, a layout at its point turned / scaled about Y; ends with its layout
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *k = (RingNode *)n;
		// 30 fps layer: see mag215_my_final_heaven_held.inc
		FX_HELD(held_note_ring(k);)
		ArgA arg;
		ScaledRotY(k->angle, k->scale, &arg.m);
		arg.m.t[0] = k->pos[0];
		arg.m.t[1] = k->pos[1];
		arg.m.t[2] = k->pos[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.morph = TexBase() + 0xF918;
		arg.depth = -0x80;
		arg.kind = (const uint8_t *)KIND_Ring;
		arg.fade = 0;
		arg.scroll = 0; // (not written by the original; the kind table has no scrolled object)
		const int r = prim::play((prim::Layout *)k->layout, CallbackA, (int)&arg, 0);
		return r != 0 ? 0 : TASK_END;
	}

	// 0x69CBB0: a ring at Zell's bone `bone` (0x502170 with `flag`), Y scale `scale`, layout 0x115C4C8
	static void SpawnBoneRing(int32_t bone, int32_t flag, int32_t scale)
	{
		RingNode *k = (RingNode *)AddTaskToQueue(QRings(), ORIG_RingTask);
		if (!k) return;
		GetEffectSpawnPosition(Caster(), bone, flag, k->pos);
		k->pos[1] = 0;
		k->scale = (int16_t)scale;
		*(uint32_t *)&k->angle = 0;
		DecodeModelPrimLayout(LAYOUT_Ring, k->layout, 0x40);
	}

	// ------------------------------------------------------------------
	// DirectorTask (0x69B070): node 0x1400, counters 0..0x119 (the counter steps last)
	// ------------------------------------------------------------------
	// one layout of the director through the prim player (0x701970) with the callback 0x69C220: its frame (arg->m) under the view
	// unless `view` is false (the screen layout)
	static void PlayA(DirectorNode *d, int site, uint8_t *layout, ArgA *arg, bool view)
	{
		// 30 fps layer: see mag215_my_final_heaven_held.inc
		FX_HELD(held_note_play(d, site, layout, arg, nullptr);)
		if (view) ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
		prim::play((prim::Layout *)layout, CallbackA, (int)arg, 0);
	}
	// the same with the camera-aligned callback 0x69C6B0 (its world anchor arg->pos)
	static void PlayB(DirectorNode *d, int site, uint8_t *layout, ArgB *arg)
	{
		// 30 fps layer: see mag215_my_final_heaven_held.inc
		FX_HELD(held_note_play(d, site, layout, nullptr, arg);)
		prim::play((prim::Layout *)layout, CallbackB, (int)arg, 0);
	}

	// the screen flash level at counter c (rising 0..0xB, 0x8FC, falling 0x10E..0x119)
	static int32_t FlashLevel(int32_t c)
	{
		if (c < 0xC) return c * 2300 / 12;
		if (c >= 0x10E) return (0x11A - c) * 2300 / 12;
		return 0x8FC;
	}

	// the jump's white flash (0xC0..0xD0): 0x1000 - (b - 0xA) x 449 / 4
	static int32_t JumpFlash(int32_t b)
	{
		return 0x1000 - (shl32(mul32(b - 0xA, 449), 10) >> 12);
	}

	// the roll of the opening lerp: -(0x1000 - t) / 14 (the exe's reciprocal multiply)
	static int32_t OpeningRoll(int32_t t)
	{
		const int32_t v = 0x1000 - t;
		int32_t q = (int32_t)(((int64_t)v * 0x6DB6DB6D) >> 32) - v;
		q >>= 3;
		return q + (int32_t)((uint32_t)q >> 31);
	}

	// the director's camera at counter c (eye 0xB8B7F0, look-at 0xB8B7F8, roll 0x1D977A2): `engine`: the camera return armed
	// on 0. X / Z = Zell's position at the start (0x2508998 / 0x250899C), CX / CZ = the centre (0x25089A0 / 0x25089A4)
	static void DirectorCamera(int32_t c, bool engine)
	{
		if (c == 0)
		{
			if (engine) CameraArmReturn();
			ViewWord() = 0x120;
			CamRoll() = 0;
		}
		const int16_t X = (int16_t)CamAnchor()[0], Z = (int16_t)CamAnchor()[1];
		if ((uint32_t)c < 0x18)
		{
			const int32_t t = (int32_t)(((uint32_t)c << 12) / 23);
			CamRoll() = (int16_t)OpeningRoll(t);
			const int16_t a[6] = { (int16_t)(X - 0x76), (int16_t)0xFDB1, (int16_t)(Z - 0x300), (int16_t)(X + 0x18D), (int16_t)0xFDC4, (int16_t)(Z - 0x1418) };
			const int16_t b[6] = { (int16_t)(X - 0x89), (int16_t)0xFCE6, (int16_t)(Z - 0x88), (int16_t)(X - 0x79), (int16_t)0xFD01, (int16_t)(Z - 0x2F3) };
			CamLerp(a, b, t);
		}
		const int32_t s = c - 0x3D;
		if ((uint32_t)s < 0xE)
		{
			const int32_t t = (int32_t)(((uint32_t)s << 12) / 14);
			if (s == 0)
			{
				CamLookAt()[1] = (int16_t)0xFCB0;
				CamEye()[1] = (int16_t)0xFBFD;
				CamLookAt()[0] = (int16_t)(X - 0x76);
				CamEye()[0] = (int16_t)(X - 0x94);
				CamLookAt()[2] = (int16_t)(Z - 0x318);
				CamEye()[2] = (int16_t)(Z + 0x507);
			}
			CamRoll() = (int16_t)sdiv4096(mul32(t, 1365));
		}
		const int16_t CX = (int16_t)CentreWords()[0], CZ = (int16_t)CentreWords()[1];
		if (c == 0x4B)
		{
			CamRoll() = 0;
			CamLookAt()[0] = (int16_t)(CX - 0x2F);
			CamLookAt()[2] = (int16_t)(CZ - 0x2B6);
			CamEye()[0] = (int16_t)(CX + 0xB53);
			CamLookAt()[1] = (int16_t)0xFE39;
			CamEye()[1] = (int16_t)0xF4DE;
			CamEye()[2] = (int16_t)(CZ + 0x3AB);
		}
		if (c == 0xA3)
		{
			CamRoll() = 0x200;
			CamLookAt()[0] = (int16_t)(CX - 0x105);
			CamLookAt()[2] = (int16_t)(CZ + 0x59C);
			CamEye()[0] = (int16_t)(CX + 0x1E);
			CamLookAt()[1] = (int16_t)0xF955;
			CamEye()[1] = (int16_t)0xFDE1;
			CamEye()[2] = (int16_t)(CZ - 0x1265);
		}
		const int32_t e = c - 0xDD;
		if ((uint32_t)e < 4)
		{
			const int16_t a[6] = { (int16_t)(CX - 0x105), (int16_t)0xF955, (int16_t)(CZ + 0x59C), (int16_t)(CX + 0x1E), (int16_t)0xFDE1, (int16_t)(CZ - 0x1265) };
			const int16_t b[6] = { (int16_t)(CX + 0xCE), (int16_t)0xFC16, (int16_t)(CZ - 0xC49), (int16_t)(CX + 0x35B), 0x3C, (int16_t)(CZ - 0x15D5) };
			const int32_t t = (int32_t)(((uint32_t)e << 12) / 3);
			CamRoll() = (int16_t)(CamRoll() - 0x80);
			CamLerp(a, b, t);
		}
	}

	// a ring at Zell's feet (x +- 0xC8 alternately), Y scale 0x2000, layout 0x115C4C8 (the jump 0xB6..0xD2)
	static void SpawnSideRing(int32_t b)
	{
		RingNode *k = (RingNode *)AddTaskToQueue(QRings(), ORIG_RingTask);
		if (!k) return;
		const int16_t x = W16(Caster(), 0x1C);
		k->pos[0] = x;
		k->pos[0] = (int16_t)((b & 1) ? x + 0xC8 : x - 0xC8);
		k->pos[1] = 0;
		k->scale = 0x2000;
		*(uint32_t *)&k->angle = 0;
		k->pos[2] = (int16_t)(W16(Caster(), 0x20) + 0x400);
		DecodeModelPrimLayout(LAYOUT_Ring, k->layout, 0x40);
	}

	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		// 30 fps layer: see mag215_my_final_heaven_held.inc
		FX_HELD(held_note_director(d);)
		const int16_t c = d->counter;
		SetScreenFlash((uint32_t)FlashLevel(c), 0);
		// the layouts' parameter blocks (one per call: blocks keep the fields they do not set)
		ArgA arga;
		ArgB argb;
		memset(&arga, 0, sizeof(arga));
		memset(&argb, 0, sizeof(argb));
		const uint32_t morph = TexBase() + 0xF918;
		uint8_t *z = Caster();

		// 1..0x3D: a layout at Zell's anchor (on the ground), camera-aligned
		int32_t s = c - 1;
		if ((uint32_t)s < 0x3D)
		{
			if (s == 0) DecodeModelPrimLayout(LAYOUT_Anchor, d->layout_a, 0x370);
			GetDefaultEffectPosition(z, argb.pos);
			argb.pos[1] = 0;
			argb.morph = morph;
			argb.depth = -0x80;
			argb.kind = (const uint8_t *)KIND_Anchor;
			argb.scroll = (int16_t)(s << 5);
			argb.fade = 0;
			PlayB(d, SITE_ANCHOR, d->layout_a, &argb);
		}
		// 0x3E..0x4B: Zell walks back 0x100 a tick, a layout at him; rings at his bones 0x14 / 0x13
		s = c - 0x3E;
		if ((uint32_t)s < 0xE)
		{
			if (s == 0) DecodeModelPrimLayout(LAYOUT_Back, d->layout_b, 0xB0);
			W16(z, 0x20) = (int16_t)(W16(z, 0x20) - 0x100);
			RotationY(0, &arga.m);
			arga.m.t[0] = W16(z, 0x1C);
			arga.m.t[1] = 0;
			arga.m.t[2] = W16(z, 0x20);
			arga.morph = morph;
			arga.depth = -0x80;
			arga.kind = (const uint8_t *)KIND_Walk;
			arga.scroll = (int16_t)(s << 5);
			arga.fade = 0;
			PlayA(d, SITE_BACK, d->layout_b, &arga, true);
			const uint32_t r = (uint32_t)s % 6;
			if (r == 2) SpawnBoneRing(0x14, 0x1000, 0x1000);
			else if (r == 5) SpawnBoneRing(0x13, 0x1000, 0x1000);
		}
		// 0x4C..0x73: Zell runs in from 0x1800 behind the centre (0x4CC a tick to 0x55), a layout at him; the targets turn to him
		// from 0x60
		s = c - 0x4C;
		if ((uint32_t)s < 0x28)
		{
			if (s == 0)
			{
				QRings()->tail = nullptr;
				QRings()->head = nullptr;
				DecodeModelPrimLayout(LAYOUT_Run, d->layout_b, 0x188);
				W16(z, 0x20) = (int16_t)(d->centre[2] + 0x1800);
				W32(z, 0x5C) = W16(z, 0x20);
				W16(z, 0x20) = (int16_t)(W16(z, 0x20) - 0x4CC);
			}
			else if ((uint32_t)s < 0xA)
			{
				W16(z, 0x20) = (int16_t)(W16(z, 0x20) - 0x4CC);
			}
			else
			{
				B8(z, 0) |= 8;
			}
			if ((uint32_t)s >= 0x14)
			{
				for (int32_t i = 0; i < TargetCount(); i++)
				{
					uint8_t *e = d->recs[i].e;
					const int32_t dx = (W16(e, 0x1C) - W16(z, 0x1C)) >> 2;
					if (dx > -0x400 && dx < 0x400)
					{
						if (dx > 0) W16(e, 0xE) = (int16_t)(W16(e, 0xE) + dx - 0x400);
						else W16(e, 0xE) = (int16_t)(W16(e, 0xE) + dx + 0x400);
					}
				}
			}
			RotationY(0, &arga.m);
			arga.m.t[0] = W16(z, 0x1C);
			arga.m.t[1] = 0;
			arga.m.t[2] = W16(z, 0x20);
			arga.morph = morph;
			arga.depth = -0x80;
			arga.kind = (const uint8_t *)KIND_Walk;
			arga.scroll = (int16_t)(s << 6);
			arga.fade = 0;
			PlayA(d, SITE_RUN, d->layout_b, &arga, true);
		}
		// 0x74..0xA3: a screen layout (the view not composed); 0x74: the flash words' bit 2 off, the targets hidden, their angles
		// put back
		s = c - 0x74;
		if ((uint32_t)s < 0x30)
		{
			if (s == 0)
			{
				for (int k = 0; k < 4; k++) var<uint8_t>(0x1D98991 + k * 0x2C) &= 0xFD;
				for (int32_t i = 0; i < TargetCount(); i++)
				{
					TargetRec &r = d->recs[i];
					B8(r.e, 0) |= 4;
					U32(r.e, 0xC) = r.rot01;
					U32(r.e, 0x10) = r.rot23;
				}
				DecodeModelPrimLayout(LAYOUT_Screen, d->layout_b, 0x94);
			}
			RotationY(0, &arga.m);
			arga.morph = morph;
			const int32_t k = (int32_t)(((uint32_t)s << 12) / 48);
			arga.m.t[0] = 0;
			arga.m.t[1] = 0;
			arga.m.t[2] = 0xAF0;
			arga.depth = 0;
			arga.kind = (const uint8_t *)KIND_Screen;
			arga.scroll = (int16_t)(k + 0x4D);
			arga.fade = (uint32_t)s > 0x2C ? (int16_t)((s << 10) + 0x5200) : (int16_t)0;
			PlayA(d, SITE_SCREEN, d->layout_b, &arga, false);
		}
		// 0xA4..0xB5: a layout far ahead (the centre's z + 0x5DC0)
		s = c - 0xA4;
		if ((uint32_t)s < 0x12)
		{
			if (s == 0) DecodeModelPrimLayout(LAYOUT_Far, d->layout_b, 0x154);
			RotationY(0, &arga.m);
			arga.m.t[0] = W16(z, 0x1C);
			arga.m.t[1] = 0;
			arga.m.t[2] = (int32_t)d->centre[2] + 0x5DC0;
			arga.morph = morph;
			arga.depth = 0;
			arga.kind = (const uint8_t *)KIND_Plain;
			arga.fade = 0;
			PlayA(d, SITE_FAR, d->layout_b, &arga, true);
		}
		// 0xB6..0xDD: the jump
		const int32_t b = c - 0xB6;
		if ((uint32_t)b < 0x28)
		{
			arga.depth = 0;
			if ((uint32_t)b < 8) arga.fade = (int16_t)((8 - b) << 9);
			else if ((uint32_t)b > 0x16) arga.fade = (int16_t)((b - 0x16) << 9);
			else arga.fade = 0;
			if (b == 0)
			{
				W16(z, 0x20) = (int16_t)(d->centre[2] + 0x5AA0);
				DecodeModelPrimLayout(LAYOUT_Jump, d->layout_b, 0xBC);
				DecodeModelPrimLayout(LAYOUT_JumpBone, d->layout_c, 0x27C);
				InitTaskQueuePool(QRings(), (void *)(TexBase() + 0x1400), 0x294, 0x24);
			}
			if ((uint32_t)b < 0x1E)
			{
				// Zell drawn by the module and hidden; moved toward the targets
				DrawEntityWorld(z);
				B8(z, 0) |= 4;
				arga.depth = (int16_t)0xF800;
				if ((uint32_t)b < 0x14) W16(z, 0x20) = (int16_t)(W16(z, 0x20) - 0x320);
				else if ((uint32_t)b < 0x19) W16(z, 0x20) = (int16_t)(W16(z, 0x20) - 0x640);
				else if ((uint32_t)b < 0x1D)
				{
					if (b == 0x19) W16(z, 0x20) = (int16_t)(W16(z, 0x20) - 0x400);
					int32_t v = shl32(b - 0x19, 12);
					v = (int32_t)((uint32_t)v + ((uint32_t)(v >> 31) & 3u)) >> 2;
					W16(z, 0x20) = (int16_t)(W16(z, 0x20) - (mul32(0xE00 - v, 600) >> 12));
				}
				// rings (on the targets at 0xD0), debris (0xB6..0xCE)
				if (b == 0x1A)
				{
					for (int32_t i = 0; i < TargetCount(); i++)
					{
						uint8_t *e = Entity(Targets()[i * 0x18]);
						RingNode *k = (RingNode *)AddTaskToQueue(QRings(), ORIG_RingTask);
						if (!k) break;
						GetDefaultEffectPosition(e, k->pos);
						*(uint32_t *)&k->angle = 0x800;
						k->scale = (int16_t)(((int32_t)W16(e, 0x26) / 2) * 2 + 0x800);
						DecodeModelPrimLayout(LAYOUT_TargetRing, k->layout, 0x160);
					}
				}
				else if ((uint32_t)b < 0x19)
				{
					int32_t count = 4;
					Debris *k = DebrisAlloc(d);
					while (k)
					{
						k->pos[0] = W16(z, 0x1C);
						k->pos[1] = 0;
						k->life = 0xF;
						k->pos[2] = (int16_t)(W16(z, 0x20) + 0x400);
						int32_t vx = (CrtRand() & 0x3F) + 0x3F;
						k->vel[0] = (int16_t)vx;
						if (count & 1) k->vel[0] = (int16_t)-vx;
						k->vel[1] = (int16_t)(-0x50 - (CrtRand() & 0x3F));
						k->vel[2] = (int16_t)((CrtRand() & 0x3F) + 0x3F);
						k->scale = (int16_t)((CrtRand() & 0xFFF) + 0x400);
						if (--count == 0) break;
						k = DebrisAlloc(d);
					}
					if (b == 0xA) *(uint16_t *)z &= 0xFFF7;
				}
				if ((uint32_t)b < 0x1D) SpawnSideRing(b);
				if (b == 0xA)
					for (int k = 0; k < 4; k++) var<uint8_t>(0x1D98991 + k * 0x2C) |= 2;
				if ((uint32_t)b >= 0xA && (uint32_t)b <= 0x1A) SetScreenFlash((uint32_t)JumpFlash(b), 0);
			}
			else if (b == 0x1E)
			{
				*(uint16_t *)z &= 0xFFFB; // (its fade is 0x1000: no layout drawn)
			}
			else
			{
				arga.depth = (int16_t)0xF800;
			}
			if (arga.fade < 0x1000)
			{
				RotationY(0, &arga.m);
				arga.m.t[0] = W16(z, 0x1C);
				arga.m.t[1] = 0;
				arga.m.t[2] = W16(z, 0x20);
				arga.morph = morph;
				arga.kind = (const uint8_t *)KIND_Jump;
				arga.scroll = (int16_t)(b << 5);
				PlayA(d, SITE_JUMP, d->layout_b, &arga, true);
				if ((uint32_t)b >= 0xA)
				{
					int16_t p[4];
					GetEffectSpawnPosition(z, 0x16, 0, p);
					RotationY(0, &arga.m);
					arga.m.t[0] = p[0];
					arga.m.t[1] = p[1];
					arga.m.t[2] = p[2];
					arga.kind = (const uint8_t *)KIND_Plain;
					PlayA(d, SITE_JUMPBONE, d->layout_c, &arga, true);
				}
			}
		}
		// 0xDE..0xFD: the crater layout at the centre (turned 0x800); 0xFE: Zell shown
		s = c - 0xDE;
		if ((uint32_t)s < 0x20)
		{
			if (s == 0) DecodeModelPrimLayout(LAYOUT_Crater, d->layout_a, 0x154);
			RotationY(0x800, &arga.m);
			arga.m.t[0] = d->centre[0];
			arga.m.t[1] = 0;
			arga.m.t[2] = (int32_t)d->centre[2] + 0x400;
			arga.morph = morph;
			arga.depth = -0x80;
			arga.kind = (const uint8_t *)KIND_Crater;
			arga.scroll = (int16_t)(s << 5);
			arga.fade = 0;
			PlayA(d, SITE_CRATER, d->layout_a, &arga, true);
		}
		else if (s == 0x20)
		{
			*(uint16_t *)z &= 0xFFFB;
		}
		// 0xDE..0x105: the targets thrown up (+0x1E) and spun (y by +-0xCC a tick), drawn by the module about their middle;
		// 0x106..0x118: put down, drawn upright; 0x106: their bit 8 cleared, the streams 0x168 / 0x169 / 0x16A
		const int32_t t = c - 0xDE;
		if ((uint32_t)t < 0x28)
		{
			const int32_t k = (int32_t)(((uint32_t)t << 12) / 40);
			int32_t y;
			if (k < 0xC00) y = SqrtScaled(ComputeSin(k >> 1));
			else
			{
				const int32_t v = mul32(0x1000 - k, 3444);
				y = (int32_t)((uint32_t)v + ((uint32_t)(v >> 31) & 0x3FFu)) >> 10;
			}
			y = shl32(-y, 1);
			int16_t off[3];
			off[2] = 0;
			off[0] = 0;
			if (t == 0)
			{
				DecodeModelPrimLayout(LAYOUT_Crater, d->layout_a, 0x154);
				for (int32_t i = 0; i < TargetCount(); i++)
				{
					uint8_t *e = d->recs[i].e;
					MeasureHeight(e, d->recs[i].height);
					B8(e, 0) |= 8;
				}
			}
			for (int32_t i = 0; i < TargetCount(); i++)
			{
				TargetRec &r = d->recs[i];
				const int16_t ry = (int16_t)(r.pos01 >> 16);
				W16(r.e, 0x1E) = (int16_t)(ry - (int16_t)sdiv4096(mul32(r.height[0], k)) + y);
				off[1] = (int16_t)(((int32_t)r.height[1] + (int32_t)r.height[0]) >> 1);
				DrawEntity(r.e, r.ang, off);
				r.ang[0] = (int16_t)(r.ang[0] + ((i & 1) ? 0xCC : -0xCC));
				B8(r.e, 0) |= 4;
			}
		}
		else
		{
			if ((uint32_t)t < 0x3B)
			{
				for (int32_t i = 0; i < TargetCount(); i++)
				{
					TargetRec &r = d->recs[i];
					const int16_t ry = (int16_t)(r.pos01 >> 16);
					W16(r.e, 0x1E) = ry;
					MeasureHeight(r.e, r.height);
					W16(r.e, 0x1E) = (int16_t)(ry - r.height[0]);
					DrawEntity(r.e, ZeroAngles(), ZeroAngles());
					B8(r.e, 0) |= 4;
				}
			}
			if (t == 0x28)
			{
				for (int32_t i = 0; i < TargetCount(); i++) *(uint16_t *)d->recs[i].e &= 0xFFF7;
				StreamQueueLoad(0x168, TexBase() + 0x7118, 1);
				StreamQueueLoad(0x169, TexBase() + 0x7118, 1);
				StreamQueueLoad(0x16A, TexBase() + 0x7118, 0);
			}
		}
		// Zell's world translation from his position (every tick)
		W32(z, 0x54) = W16(z, 0x1C);
		W32(z, 0x58) = W16(z, 0x1E);
		W32(z, 0x5C) = W16(z, 0x20);
		// 0x18..0x73: a layout at his bone 0x16, camera-aligned
		s = c - 0x18;
		if ((uint32_t)s < 0x5C)
		{
			if (s == 0) DecodeModelPrimLayout(LAYOUT_Bone, d->layout_c, 0x1B4);
			GetEffectSpawnPosition(z, 0x16, 0, argb.pos);
			argb.morph = morph;
			argb.depth = -0x80;
			argb.kind = (const uint8_t *)KIND_Plain;
			argb.fade = 0;
			PlayB(d, SITE_BONE, d->layout_c, &argb);
		}
		DebrisDraw(d);
		DebrisMove(d);
		if (c == 5) BdPlaySE(SOUND_A, 0, 0x80);
		else if (c == 0xA2) BdPlaySE(SOUND_B, 0, 0x80);
		else if (c == 0x108) ApplyActionResultToTargets(Targets(), (uint32_t)TargetCount());
		if (c == 0xA3)
		{
			// the targets shown and moved by the centre's -z
			const int16_t mz = (int16_t)-d->centre[2];
			d->centre[0] = 0;
			for (int32_t i = 0; i < TargetCount(); i++)
			{
				uint8_t *e = d->recs[i].e;
				*(uint16_t *)e &= 0xFFFB;
				W16(e, 0x20) = (int16_t)(W16(e, 0x20) + mz);
			}
		}
		DirectorCamera(c, true);
		if (++d->counter >= 0x11A)
		{
			for (int32_t i = 0; i < TargetCount(); i++) *(uint16_t *)d->recs[i].e &= 0xFFF3;
			*(uint16_t *)Entity(0) &= 0xFFFB;
			*(uint16_t *)Entity(1) &= 0xFFFB;
			*(uint16_t *)Entity(2) &= 0xFFFB;
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// RootTask (0x69AC00): node 0x10 (pool 0x2508978)
	// ------------------------------------------------------------------
	static void StartDirector(DirectorNode *d)
	{
		Memset32(&d->counter, 0, 0x4FD);
		int16_t minx = 0x7FFF, minz = 0x7FFF, maxx = (int16_t)0x8001, maxz = (int16_t)0x8001;
		for (int32_t i = 0; i < TargetCount(); i++)
		{
			TargetRec &r = d->recs[i];
			r.e = Entity(Targets()[i * 0x18]);
			r.pos01 = U32(r.e, 0x1C);
			r.pos23 = U32(r.e, 0x20);
			r.rot01 = U32(r.e, 0xC);
			r.rot23 = U32(r.e, 0x10);
			*(uint32_t *)&r.ang[0] = *(const uint32_t *)ZeroAngles();
			*(uint32_t *)&r.ang[2] = *(const uint32_t *)(ZeroAngles() + 2);
			int16_t p[4];
			GetDefaultEffectPosition(r.e, p);
			if (p[0] < minx) minx = p[0];
			if (p[0] > maxx) maxx = p[0];
			if (p[2] < minz) minz = p[2];
			if (p[2] > maxz) maxz = p[2];
		}
		(void)minz;
		const int16_t cx = (int16_t)(((int32_t)maxx + (int32_t)minx) / 2);
		d->centre[0] = cx;
		d->centre[1] = 0;
		d->centre[2] = maxz;
		uint8_t *z = Caster();
		const int16_t x = (int16_t)ClearX(cx, W16(z, 0x26), W16(z, 0x92));
		d->centre[0] = x;
		d->saved_pos[0] = U32(z, 0x1C);
		d->saved_pos[1] = U32(z, 0x20);
		d->saved_rot[0] = U32(z, 0xC);
		d->saved_rot[1] = U32(z, 0x10);
		W16(z, 0x1C) = x;
		W16(z, 0x20) = (int16_t)(d->centre[2] + 0x1000);
		W16(z, 0xE) = 0;
		CamAnchor()[0] = U32(z, 0x1C);
		CamAnchor()[1] = U32(z, 0x20);
		CentreWords()[0] = *(const uint32_t *)&d->centre[0];
		CentreWords()[1] = *(const uint32_t *)&d->centre[2];
		DebrisInit(d);
		BindExternalAnimSet(z, d->bind, COM_Anim);
		QueueChainTransformation(z, 1);
		StreamStateInit((void *)(TexBase() + 0x70D0));
		*(uint16_t *)Entity(0) |= 4;
		*(uint16_t *)Entity(1) |= 4;
		*(uint16_t *)Entity(2) |= 4;
		*(uint16_t *)z &= 0xFFFB;
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag215_my_final_heaven_held.inc
		FX_HELD(held_note_root(r);)
		if (r->parity)
		{
			PacketCursor() = TexBase() + 0x10518;
			PacketCursor2() = TexBase() + 0x22518;
			r->parity = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x22518;
			PacketCursor2() = TexBase() + 0x34518;
			r->parity = 1;
		}
		if (r->counter == 1)
		{
			if (r->state == 0)
			{
				r->state = 1;
				InitTaskQueuePool(QDirector(), (void *)TexBase(), 0x1400, 1);
				InitTaskQueuePool(QRings(), (void *)(TexBase() + 0x1400), 0x294, 0x24);
			}
			DirectorNode *d = (DirectorNode *)AddTaskToQueue(QDirector(), ORIG_DirectorTask);
			if (d) StartDirector(d);
		}
		int a = 1;
		if (r->state)
		{
			a = ExecuteTaskQueue(QDirector());
			ExecuteTaskQueue(QRings());
		}
		if (r->state)
		{
			StreamPump();
			if (a == 0)
			{
				if (StreamPreload(2) == 0) return 0;
				// the director ended: the targets / Zell put back, his bit 8 cleared, the saved view back
				const DirectorNode *d = (const DirectorNode *)TexBase();
				for (int32_t i = 0; i < TargetCount(); i++)
				{
					const TargetRec &t = d->recs[i];
					U32(t.e, 0x1C) = t.pos01;
					U32(t.e, 0x20) = t.pos23;
					U32(t.e, 0xC) = t.rot01;
					U32(t.e, 0x10) = t.rot23;
				}
				QueueChainTransformation(Caster(), 2);
				U32(Caster(), 0x1C) = d->saved_pos[0];
				U32(Caster(), 0x20) = d->saved_pos[1];
				U32(Caster(), 0xC) = d->saved_rot[0];
				U32(Caster(), 0x10) = d->saved_rot[1];
				*(uint16_t *)Caster() &= 0xFFF7;
				memcpy((void *)0xB8B7F0, (const void *)0xB8B800, 0x10);
				ViewWord() = var<int16_t>(0x1D977A0);
				return TASK_END;
			}
		}
		const int16_t old = r->counter;
		r->counter = (int16_t)(old + 1);
		if (old > 0x100) r->counter = 0x100;
		return 0;
	}
}

	void register_mag215_my_final_heaven()
	{
		register_port(heaven215::ORIG_RootTask, (void *)heaven215::RootTask, "215 RootTask", 215);
		register_port(heaven215::ORIG_DirectorTask, (void *)heaven215::DirectorTask, "215 DirectorTask", 215);
		register_port(heaven215::ORIG_RingTask, (void *)heaven215::RingTask, "215 RingTask", 215);
		// 30 fps layer: see mag215_my_final_heaven_held.inc
		FX_HELD(register_mag215_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag215_my_final_heaven_held.inc"
#endif
