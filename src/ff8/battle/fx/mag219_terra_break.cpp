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

// Effect 219: Terra Break (enemy attack 311 of kernel.bin, Omega Weapon c0m114), cinematic-engine module MAG_219
// 0xABD3F0..0xACA0F0, script data magic/mag218_b.00/.01 (loaded by MAG_219_TERRA_BREAK_FL 0xABD3F0: IO_GetFile_MAGIC
// of the names 0x186F198 / 0x186F1A4; setup MAG_219_TERRA_BREAK 0xABD420 = effect table index 218).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x186EAD4; its descriptor block 0x186E6D0 sits 0x404 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcX\tools\cmpall.py / best.py / callees.py / fdiff.py):
//   - SetupSummon 0xABD480, BindContext 0xABD4D0, the section pointers 0xABD570, InitSequenceState
//     0xAC9700 and SequenceTick 0xABDA90 (the unlit BuildMatricesAndDraw) are byte clones of Sand
//     Shake 256's; the initial frame limit 0xACA0D0 sets 0x2C4 (original setup code, not ported);
//     lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (as Beam
//     Cannon 250, plus the generic prim 8), except
//       VM 0x006 LoadBattleFile 0xAC5820: same code, own load callback 0xAC5950 (c.load_cb),
//       prim 6 0xAC0CE0 TintedF3 = Eden 0xAEB5B0 (apply_eden_d), draw 16 0xABE340 = Bahamut 0xB198C0
//         ScrollTextureU (apply_shared_misc), draw 37 0xABE840 = Bahamut 0xB1A130 RibbonTrail
//         (apply_shared_ribbon), draw 58 0xABFB20 = Eden 0xAEA550 RequestScreenFeedback (apply_eden_c)
//         (only those slots taken),
//       VM 0x0AD 0xAC92E0 PosFromPosedEntityJoint = Ice Strike 228's 0xA8EA50 / Homing Laser 229's
//         0xA84780 (copied from mag228_ice_strike.cpp),
//       VM 0x12E 0xAC53A0 ApplyNextTargetResult = Blood Pain 262's 0x954DA0 (copied),
//       VM 0x0B3 0xAC7800 SteerTowardBone, draw 49 0xAC26B0 StageShatter (+ 0xAC2890 / 0xAC2EA0 /
//         0xAC3460), draw 67 0xAC2230 FoggedGridSheet: Terra Break's own handlers, ported here (their
//         module helpers 0xABFF90 / 0xAC20C0 = the generic 0xB27130 / 0xB29450: h_B27130 / h_B29450).
// The effect's queue (0x2796C50) holds one task, the engine tick 0xABDA90.
// Battle writes: draw 49 hides stage groups 0..2 (stage group slots 0x1D9898C + 0x2C k, +5 bit 1)
// on its first draw (group 3 +5 bit 1 set) and rebuilds their skeletons on every draw; VM 0x0AD rebuilds the slot entity's
// bone matrices; VM 0x12E applies the action result to the next target; the battle camera words
// through the engine's camera ops (VM 0x039).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
	void apply_eden_c(Clone &c);  // mag_eden.cpp
	void apply_eden_d(Clone &c);  // mag_eden.cpp

namespace terra219
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag219_terra_break_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace terra219
{
	static void describe(Clone &c)
	{
		c.name = "219 Terra Break";
		c.effect_id = 219;
		c.vm_table = 0x186EC78;      // MAG_219 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x186E8E8;    // MAG_219 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x186E930;    // bone_table + 0x48
		c.prim_size = 0x186EA20;     // bone_table + 0x138
		c.spr_table = 0x186EA34;     // bone_table + 0x14C
		c.ptype_table = 0x186EA60;   // bone_table + 0x178
		c.pop_table = 0x186EA74;     // bone_table + 0x18C
		c.draw_table = 0x186EAD4;    // MAG_219 DrawHandlerTable
		c.attr_16A = 0x186EC3E;
		c.attr_184 = 0x186EC58;
		c.attr_186 = 0x186EC5A;
		c.attr_194 = 0x186EC68;
		c.attr_19C = 0x186EC70;
		c.desc = 0x186E6D0;          // draw_table - 0x404 (read by 0xABD570)
		c.ptr_block = 0x186E8C0;     // draw_table - 0x214 (read by 0xABD4D0)
		c.queue = 0x2796C50;
		c.file00 = 0x2796C64;
		c.file01 = 0x2796C60;
		c.tick = 0xABDA90;
		c.load_cb = 0xAC5950;        // passed by VM 0x006 0xAC5820
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// engine calls
	// ------------------------------------------------------------------------------------
	// 0xB66B80 (blob): matrix of joint `joint` of the model whose BattleAnimHeader is `hdr`,
	// composed with m; joint origin -> ws+0xF0..0xF8. Returns ws+0xD0. Writes only ws + GTE.
	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(0xB66B80)(hdr, joint, scale, m);
	}
	// 0xB67A60 (shared GF helper blob, original address): steers the point {s16 x, y, z, s16 angle A,
	// s16 angle B} p toward the target point t: both angles turned toward the target by at most
	// `turn` (blob 0xB67B80), p + speed * (sin, cos) of them -> ws+0xF0..0xF5 (new point), ws+0xE0 /
	// ws+0xE4 = the new angles; returns 1 when the new point lies within `reach` of the target.
	// Writes only the workspace.
	static inline int32_t xl_Steer_B67A60(const void *p, const void *t, int32_t speed, int32_t turn, int32_t reach, int32_t a, int32_t b)
	{
		return x::f<int32_t (__cdecl *)(const void *, const void *, int32_t, int32_t, int32_t, int32_t, int32_t)>(0xB67A60)(p, t, speed, turn, reach, a, b);
	}
	// the bone's slot entity (scene entity table [bone+0x1B])
	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	// ------------------------------------------------------------------------------------
	// 0xAC92E0 (VM 0x0AD PosFromPosedEntityJoint, = Ice Strike 228's 0xA8EA50 / Homing Laser 229's
	// 0xA84780, copied from mag228_ice_strike.cpp): the slot entity's bone matrices rebuilt from its
	// pose (header entity+0x60, or *(entity+0x78) when op bit 15 is set), joint w0 of that model (blob
	// 0xB66B80, composed with the entity matrix +0x40) applied to outPos -> accumPos = the camera-space
	// point << 16. Then the bone handler. Length 4.
	// 30 fps: while the held layer predicts the next tick, the joint comes from the entity's NEXT
	// pose (see mag219_terra_break_held.inc).
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AD_PosFromPosedEntityJoint()
	{
		uint8_t *ent = SlotEntity();                                          // edi
		const bool weapon = (U8(RT(), 0x4B) & 0x80) != 0;
		uint8_t *hdr = weapon ? PTR(ent, 0x78) : ent + 0x60;                  // esi
		x::BuildBoneMatricesFromPose(hdr);
		// 30 fps layer: see mag219_terra_break_held.inc
		FX_HELD(if (held_predicting()) held_next_pose(hdr, weapon ? hdr + 0xC : ent + 0x6C);)
		uint8_t *m = xl_JointMatrix_B66B80(hdr, S16(STREAM(), 2), 0x1000, ent + 0x40);
		x::SetRotMatrix(m);
		x::SetTransVector(m);
		uint8_t *w = WS();
		x::TransformToCamera(CUR() + 0x94, w + 0xF0, w + 0xFC);
		S32(CUR(), 0x5C) = shl32(S32(WS(), 0xF0), 16);
		S32(CUR(), 0x60) = shl32(S32(WS(), 0xF4), 16);
		S32(CUR(), 0x64) = shl32(S32(WS(), 0xF8), 16);
		C().bone[U8(CUR(), 0x18)]();
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// 0xAC53A0 (VM 0x12E ApplyNextTargetResult, = Blood Pain 262's 0x954DA0, copied from
	// mag262_blood_pain.cpp): while SceneHeader+0x3A < the target count (ctx+0xCC list +0x10), the
	// action result of target [SceneHeader+0x3A] (list +8, 0x18 each) is applied and the index
	// stepped. Length 2.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_12E_ApplyNextTargetResult()
	{
		uint8_t *list = PTR(CTX(), 0xCC);                                     // ecx
		const int32_t i = U8(SCENE(), 0x3A);                                  // eax
		if (S8(list, 0x10) > i)
		{
			x::ApplyActionResultToTarget((int32_t)(uint32_t)(PTR(list, 8) + 0x18 * i));
			U8(SCENE(), 0x3A) = (uint8_t)(U8(SCENE(), 0x3A) + 1);
		}
		STREAM() += 2;
	}

	// ------------------------------------------------------------------------------------
	// 0xAC7800 (VM 0x0B3 SteerTowardBone): operand s16 bone T. The bone's steering state bone+0xA0
	// {s16 x, y, z, s16 angle A, s16 angle B}. Opcode bit 0x8000 (RT+0x4B & 0x80): start: state =
	// outPos, A = 0xB66B30(x, z, T.x, T.z), B = 0xB66B30(y, z, T.y, T.z). Else one step (blob 0xB67A60,
	// outPos = speed / turn rate / reach): state and outPos = the new point, A / B = the new angles,
	// bone+0x4A bit 0 = arrived. Length 4.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0B3_SteerTowardBone()
	{
		uint8_t *t = blob::GetBone(S16(STREAM(), 2));                       // eax
		uint8_t *cur = CUR();                                                // ecx
		uint8_t *st = cur + 0xA0;                                            // esi
		if (U8(RT(), 0x4B) & 0x80)
		{
			const int16_t x0 = S16(cur, 0x94), y0 = S16(cur, 0x96), z0 = S16(cur, 0x98);
			S16(st, 0) = x0;
			S16(st, 2) = y0;
			S16(st, 4) = z0;
			const int32_t tx = S16(t, 0x94), ty = S16(t, 0x96), tz = S16(t, 0x98);
			S16(st, 6) = (int16_t)blob::Blob_B66B30(x0, z0, tx, tz);
			S16(st, 8) = (int16_t)blob::Blob_B66B30(y0, z0, ty, tz);
			STREAM() += 4;
			return;
		}
		const int32_t r = xl_Steer_B67A60(st, t + 0x94, S16(cur, 0x94), S16(cur, 0x96), S16(cur, 0x98), S16(st, 6), S16(st, 8));
		if (r == 0) U16(CUR(), 0x4A) &= 0xFFFE;
		else U8(CUR(), 0x4A) |= 1;
		uint8_t *ws = WS();
		U16(st, 0) = U16(ws, 0xF0);
		U16(CUR(), 0x94) = U16(ws, 0xF0);
		U16(st, 2) = U16(ws, 0xF2);
		U16(CUR(), 0x96) = U16(ws, 0xF2);
		U16(st, 4) = U16(ws, 0xF4);
		U16(CUR(), 0x98) = U16(ws, 0xF4);
		U16(st, 6) = U16(WS(), 0xE0);
		U16(st, 8) = U16(WS(), 0xE4);
		STREAM() += 4;
	}

	// 0x56CC50 GTE_SetFogNearFar(near, far, h): fog globals 0x209AB64 / 0xC78BF0 + GTE DQA / DQB
	static inline void xl_SetFogNearFar_56CC50(int32_t a, int32_t b, int32_t h) { x::f<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x56CC50)(a, b, h); }
	// 0x56CAA0 GTE_RotTransPersNormalColor(v, n, colour, sxy, rgb, flag): RTPS of v -> sxy, NCDS-style lit colour -> rgb, returns OTZ
	static inline int32_t xl_RotTransPersNormalColor_56CAA0(const void *v, const void *n, const void *c, void *sxy, void *rgb, void *flag)
	{
		return x::f<int32_t (__cdecl *)(const void *, const void *, const void *, void *, void *, void *)>(0x56CAA0)(v, n, c, sxy, rgb, flag);
	}

	// ------------------------------------------------------------------------------------
	// 0xAC2230 (draw 67 FoggedGridSheet): bone+0xB8 = {s16 reference bone, s16 texture id, s16 W, s16 H,
	// s16 fog near, s16 fog far}. A (W+1) x (H+1) vertex grid 0x200 apart in the bone's XZ plane (Y =
	// outPos Y << 4; the grid offset by outPos X / Z & 0x1FF: a scrolling sheet), each vertex projected
	// and lit (0x56CAA0, colour 0x3C808080), drawn as W x H Gouraud textured quads (GT4, 0x34 bytes)
	// with the texture's tpage / CLUT (blob 0xB665C0 / 0xB66640), UVs stepping 0x20 per cell from
	// (-(W/2 << 5) - (outPos Z >> 4 & 0xE0), -(H/2 << 5) - (outPos X >> 4 & 0x1E0)), in the alternate
	// viewport (SSIGPU_InsertPrimAltViewport); depth slot bone+0xC0 when bone+0xDE bit 0x20, else the
	// cell's mean depth. Far colour = the reference bone's colour, fog near / far from the descriptor.
	// The parent transform is built with outAngle Y forced to 0x1000 (put back after).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_67_FoggedGridSheet()
	{
		const uint8_t *d = PTR(CUR(), 0xB8);                                  // esi
		S32(WS(), 0x60) = S16(d, 4);
		S32(WS(), 0x64) = S16(d, 6);
		const int32_t tex = S16(d, 2);                                        // edi
		blob::TexInfo_B665C0(tex);
		U32(WS(), 0x68) = U32(WS(), 0xFC);
		S32(WS(), 0x6C) = blob::ClutWord_B66640(tex);
		uint8_t *ws = WS();                                                   // ecx
		{
			const int32_t a = shl32(S32(ws, 0x60) / 2, 5);
			const int32_t e = ((int32_t)U16(CUR(), 0x98) >> 4) & 0xE0;
			S32(ws, 0x70) = add32(sub32((int32_t)~(uint32_t)a, e), 1);
		}
		ws = WS();
		{
			const int32_t a = shl32(S32(ws, 0x64) / 2, 5);
			const int32_t e = ((int32_t)U16(CUR(), 0x94) >> 4) & 0x1E0;
			S32(ws, 0x74) = add32(sub32((int32_t)~(uint32_t)a, e), 1);
		}
		ws = WS();
		S32(ws, 0x78) = mul32(S32(ws, 0x60) + 1, 12);
		uint8_t *ref = blob::GetBone(S16(d, 0));                              // ebx
		if (U8(CUR(), 0xDE) & 0x20) U32(WS(), 0x7C) = U32(CUR(), 0xC0);
		else U32(WS(), 0x7C) = 0xFFFFFFFFu;
		const int16_t angy = S16(CUR(), 0x8E);                                // edi
		S16(CUR(), 0x8E) = 0x1000;
		h_B27130();
		S16(CUR(), 0x8E) = angy;
		h_B29450((int32_t)U32(CUR(), 0xCC));
		xl_SetFogNearFar_56CC50(S16(d, 8), S16(d, 0xA), 0x200);
		const uint32_t rc = U32(ref, 0xCC);
		x::GteSetBackColor3((int32_t)(rc & 0xFF), (int32_t)((rc >> 8) & 0xFF), (int32_t)((rc >> 16) & 0xFF));
		uint8_t *v = PTR(CTX(), 0x74);                                        // ebx
		ws = WS();
		S16(ws, 0xC0) = (int16_t)((U16(CUR(), 0x94) & 0x1FF) - shl32(S32(ws, 0x64) / 2, 9));
		ws = WS();
		const int32_t z0 = (int32_t)(U16(CUR(), 0x98) & 0x1FF) - shl32(S32(ws, 0x60) / 2, 9);   // [ebp-8]
		S16(ws, 0xC2) = 0;
		S16(WS(), 0xC8) = (int16_t)(U16(CUR(), 0x96) << 4);
		S16(WS(), 0xCA) = 0;
		S16(WS(), 0xCC) = 0;
		U32(WS(), 0xE0) = 0x3C808080;
		const uint32_t code = (U32(CUR(), 0xCC) & 0x2000000) | 0x3C000000;   // edi
		ws = WS();
		if (S32(ws, 0x64) >= 0)
		{
			for (int32_t r = S32(ws, 0x64) + 1; r > 0; r--)
			{
				S16(ws, 0xC4) = (int16_t)z0;
				ws = WS();
				if (S32(ws, 0x60) >= 0)
				{
					for (int32_t c = S32(ws, 0x60) + 1; c > 0; c--)
					{
						U16(v, 4) = (uint16_t)xl_RotTransPersNormalColor_56CAA0(ws + 0xC0, ws + 0xC8, ws + 0xE0, v, v + 8, ws + 0xFC);
						U32(v, 8) |= code;
						v += 0xC;
						S16(WS(), 0xC4) = (int16_t)(S16(WS(), 0xC4) + 0x200);
						ws = WS();
					}
				}
				S16(ws, 0xC0) = (int16_t)(S16(ws, 0xC0) + 0x200);
				ws = WS();
			}
		}
		uint8_t *row0 = PTR(CTX(), 0x74);                                     // ebx
		uint8_t *row1 = row0 + S32(ws, 0x78);                                 // edi
		uint8_t *pk = PTR(CTX(), 0x7C);                                       // esi
		int32_t vv = S32(ws, 0x74);                                           // [ebp-4]
		for (int32_t r = S32(ws, 0x64); r > 0; r--)
		{
			int32_t u = S32(ws, 0x70);
			const int32_t w = S32(ws, 0x60);
			if (w > 0)
			{
				const int32_t v2 = vv + 0x20;                                     // [ebp-0x14]
				for (int32_t c = w; c > 0; c--)
				{
					U8(pk, 3) = 0xC;
					U32(pk, 4) = U32(row0, 8);
					U32(pk, 0x10) = U32(row0, 0x14);
					U32(pk, 0x1C) = U32(row1, 8);
					U32(pk, 0x28) = U32(row1, 0x14);
					u &= 0x60;
					U16(pk, 0x1A) = U16(WS(), 0x68);
					U16(pk, 0xE) = U16(WS(), 0x6C);
					U8(pk, 0x24) = (uint8_t)u;
					U8(pk, 0xC) = (uint8_t)u;
					u += 0x20;
					const uint8_t u2 = (u & 0x70) ? (uint8_t)u : 0x7F;
					U8(pk, 0x30) = u2;
					U8(pk, 0x18) = u2;
					U8(pk, 0x19) = (uint8_t)vv;
					U8(pk, 0xD) = (uint8_t)vv;
					const uint8_t vb = (v2 & 0xF0) ? (uint8_t)v2 : 0xFF;
					U8(pk, 0x31) = vb;
					U8(pk, 0x25) = vb;
					U32(pk, 8) = U32(row0, 0);
					U32(pk, 0x14) = U32(row0, 0xC);
					U32(pk, 0x20) = U32(row1, 0);
					U32(pk, 0x2C) = U32(row1, 0xC);
					int32_t z = S32(WS(), 0x7C);
					if (z < 0) z = (S16(row1, 0x10) + S16(row1, 4) + S16(row0, 0x10) + S16(row0, 4)) / 4;
					x::InsertPrimAltViewport(((uint32_t)z & 0xFFFFFFFCu) + U32(RT(), 0x4C), pk);
					row0 += 0xC;
					row1 += 0xC;
					pk += 0x34;
				}
				ws = WS();
			}
			vv += 0x20;
			row0 += 0xC;
			row1 += 0xC;
		}
		PTR(CTX(), 0x7C) = pk;
	}

	// 0x56CB30 (-> 0x56CB00): GTE NCLIP of three screen coordinates (SXY words)
	static inline int32_t xl_NClip_56CB30(uint32_t s0, uint32_t s1, uint32_t s2) { return x::f<int32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56CB30)(s0, s1, s2); }
	// 0x45C7A0 SSIGPU_InsertPrimAutoDepth(ot, prim)
	static inline void xl_InsertPrimAutoDepth(uint32_t ot, void *prim) { x::f<void (__cdecl *)(uint32_t, void *)>(0x45C7A0)(ot, prim); }
	// GTE data registers (0x1CA8A10 + 4 * reg)
	static inline uint32_t &TbGte(int reg) { return MEM<uint32_t>(0x1CA8A10 + 4 * reg); }
	// x / 3 as compiled (imul 0x55555556, + sign bit of the high half)
	static inline int32_t TbDiv3(int32_t v)
	{
		const int32_t hi = (int32_t)(((int64_t)v * 0x55555556LL) >> 32);
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// Draw 49 StageShatter. The battle stage geometry groups 0..2 (stage group slots 0x1D9898C + 0x2C k, from +4: e = slot + 4:
	// +1 bit 0 = present, +4 skeleton (bone matrices from +0x20, 0x30 each), +8 model (object table:
	// count, offsets; per object vertex groups {bone, count, s16 xyz}, then triangle / quad records),
	// +0x14 BattleAnimHeader, +0x28 far colour) fall apart in shards: every polygon whose centroid comes
	// within outAngleX of the bone's outPos (model space) is launched as a shard (the pool at bone+0xBC:
	// 0x400 records of 0x2C, model order) and from then on drawn as a free triangle / quad that falls
	// (velocity +9 << 4 per tick on Y) and spins (byte angles +0x12 / +0x1A / +0x22 += +0x13 / +0x1B /
	// +0x23) in the colour of the descriptor table [life +7] (semi-transparent), until its life runs
	// out; the other polygons are drawn in place (the model as posed). Descriptor bone+0xB8: {s16
	// offset of the colour table, s16 life, s16 reference bone}; the reference bone gives the launch
	// velocity (outPos Y + rand(outAngle Y)) >> 4 and the spin rates (outPos Z + rand(outAngle Z)).
	// First draw: the pool, every record's life, and stage groups 0..2 hidden from the battle stage renderer
	// (render entries 0..2 +1 bit 1 cleared, entry 3 +1 bit 1 set).

	// one shard record (0x2C): +0/+2/+4 s16 position (model space, relative to outPos), +6 u8 launched,
	// +7 s8 life, +8 u8 0, +9 s8 fall speed, +0xC / +0x14 / +0x1C / +0x24 s16 xyz vertex offsets (the
	// 4th only for quads), +0x12 / +0x1A / +0x22 s8 angles X / Y / Z (+0x13 / +0x1B / +0x23 their rates)

	// launch of the polygons near the bone (0xAC2890), on real ticks only (it draws random numbers)
	static void Launch_AC2890(uint8_t *e, uint8_t *pool)
	{
		if (!(U8(e, 1) & 1)) return;
		uint8_t *ws = WS();                                                    // edi
		x::BuildBoneMatricesFromPose(e + 0x14);
		uint8_t *mats = PTR(e, 4) + 0x10;                                      // [ebp-0x1c]
		const uint8_t *tab = PTR(e, 8);
		int32_t nobj = S32(tab, 0);
		tab += 4;
		for (; nobj > 0; nobj--)
		{
			const uint8_t *obj = PTR(e, 8) + U32(tab, 0) + 2;                 // ebx
			tab += 4;
			uint8_t *out = PTR(CTX(), 0x74);
			PTR(ws, 0x88) = out;
			for (int32_t g = S16(obj, -2); g > 0; g--)
			{
				uint8_t *m = mats + 0x30 * S16(obj, 0) + 0x10;
				x::SetRotMatrix(m);
				x::SetTransVector(m);
				int32_t nv = S16(obj, 2);
				obj += 4;
				for (; nv > 0; nv--)
				{
					x::GteWriteData((int32_t)(((uint32_t)U16(obj, 2) << 16) | U16(obj, 0)), 0);
					x::GteWriteData((int32_t)U16(obj, 4), 1);
					x::GteMVMVA_RotV0Tr();
					U16(out, 0) = (uint16_t)TbGte(25);
					U16(out, 2) = (uint16_t)TbGte(26);
					U16(out, 4) = (uint16_t)TbGte(27);
					obj += 6;
					out += 8;
				}
			}
			const uint8_t *t = (const uint8_t *)(((uintptr_t)obj + 3) & ~(uintptr_t)3);
			S16(ws, 0xD0) = S16(t, 4);
			S16(ws, 0xD2) = S16(t, 6);
			const uint8_t *p = t + 0xC;                                        // [ebp-4]
			const uint8_t *vb = PTR(ws, 0x88);
			for (; S16(ws, 0xD0) > 0; S16(ws, 0xD0) = (int16_t)(S16(ws, 0xD0) - 1), pool += 0x2C, p += 0x14)
			{
				if (U8(pool, 6) != 0) continue;
				const uint8_t *v0 = vb + 8 * (U16(p, 0) & 0xFFF), *v1 = vb + 8 * (U16(p, 2) & 0xFFF), *v2 = vb + 8 * (U16(p, 4) & 0xFFF);
				const int32_t cx = TbDiv3(S16(v1, 0) + S16(v0, 0) + S16(v2, 0)) - S32(ws, 0x60);
				const int32_t cy = TbDiv3(S16(v2, 2) + S16(v1, 2) + S16(v0, 2)) - S32(ws, 0x64);
				const int32_t cz = TbDiv3(S16(v1, 4) + S16(v2, 4) + S16(v0, 4)) - S32(ws, 0x68);
				if (add32(add32(mul32(cy, cy), mul32(cx, cx)), mul32(cz, cz)) >= S32(WS(), 0x80)) continue;
				U8(pool, 6) = 1;
				S16(pool, 0) = (int16_t)cx;
				S16(pool, 2) = (int16_t)cy;
				S16(pool, 4) = (int16_t)cz;
				for (int k = 0; k < 3; k++)
				{
					const uint8_t *v = vb + 8 * (U16(p, 2 * k) & 0xFFF);
					S16(pool, 0xC + 8 * k) = (int16_t)(S16(v, 0) - cx);
					S16(pool, 0xE + 8 * k) = (int16_t)(S16(v, 2) - cy);
					S16(pool, 0x10 + 8 * k) = (int16_t)(S16(v, 4) - cz);
				}
				U32(pool, 8) = 0;
				U16(pool, 0x22) = 0;
				U16(pool, 0x1A) = 0;
				U16(pool, 0x12) = 0;
				const int32_t base = S32(ws, 0x90), range = S32(ws, 0x94);
				U8(pool, 0x13) = (uint8_t)(blob::Rand73(range) + base);
				U8(pool, 0x1B) = (uint8_t)(blob::Rand73(range) + base);
				U8(pool, 0x23) = (uint8_t)(blob::Rand73(range) + base);
				U8(pool, 9) = (uint8_t)(add32(S32(ws, 0x70), blob::Rand73(S32(ws, 0x74))) >> 4);
			}
			if (S16(ws, 0xD2) > 0)
			{
				p += 4;
				for (; S16(ws, 0xD2) > 0; S16(ws, 0xD2) = (int16_t)(S16(ws, 0xD2) - 1), pool += 0x2C, p += 0x18)
				{
					if (U8(pool, 6) != 0) continue;
					const uint8_t *v0 = vb + 8 * (U16(p, -4) & 0xFFF), *v1 = vb + 8 * (U16(p, -2) & 0xFFF);
					const uint8_t *v2 = vb + 8 * (U16(p, 0) & 0xFFF), *v3 = vb + 8 * (U16(p, 2) & 0xFFF);
					const int32_t cx = (S16(v1, 0) + S16(v0, 0) + S16(v2, 0) + S16(v3, 0)) / 4 - S32(ws, 0x60);
					const int32_t cy = (S16(v3, 2) + S16(v2, 2) + S16(v1, 2) + S16(v0, 2)) / 4 - S32(ws, 0x64);
					const int32_t cz = (S16(v3, 4) + S16(v2, 4) + S16(v1, 4) + S16(v0, 4)) / 4 - S32(ws, 0x68);
					if (add32(add32(mul32(cy, cy), mul32(cx, cx)), mul32(cz, cz)) >= S32(WS(), 0x80)) continue;
					S16(pool, 4) = (int16_t)cz;
					U8(pool, 6) = 1;
					S16(pool, 0) = (int16_t)cx;
					S16(pool, 2) = (int16_t)cy;
					for (int k = 0; k < 4; k++)
					{
						const uint8_t *v = vb + 8 * (U16(p, 2 * k - 4) & 0xFFF);
						S16(pool, 0xC + 8 * k) = (int16_t)(S16(v, 0) - cx);
						S16(pool, 0xE + 8 * k) = (int16_t)(S16(v, 2) - cy);
						S16(pool, 0x10 + 8 * k) = (int16_t)(S16(v, 4) - cz);
					}
					U32(pool, 8) = 0;
					U16(pool, 0x22) = 0;
					U16(pool, 0x1A) = 0;
					U16(pool, 0x12) = 0;
					const int32_t base = S32(ws, 0x90), range = S32(ws, 0x94);
					U8(pool, 0x13) = (uint8_t)(blob::Rand73(range) + base);
					U8(pool, 0x1B) = (uint8_t)(blob::Rand73(range) + base);
					U8(pool, 0x23) = (uint8_t)(blob::Rand73(range) + base);
					U8(pool, 9) = (uint8_t)(add32(S32(ws, 0x70), blob::Rand73(S32(ws, 0x74))) >> 4);
				}
			}
		}
	}

	// 0xAC3460: the shard's step (not frozen: life - 1, Y += speed << 4, angles += rates) and its
	// transform (angles << 4, translation = position, composed with the parent matrix ws+0x84) into
	// the GTE. Held frames (30 fps layer): the in-between of the step instead (TbHeldShard).
	static void ShardStep_AC3460(uint8_t *s)
	{
		uint8_t *ws = WS();                                                    // ebx
		if (U8(RT(), 0x45) == 0)
		{
			FX_HELD(TbNoteStep(s);)
			U8(s, 7) = (uint8_t)(U8(s, 7) - 1);
			S16(s, 2) = (int16_t)(S16(s, 2) + (int16_t)(S8(s, 9) << 4));
			U8(s, 0x12) = (uint8_t)(U8(s, 0x12) + U8(s, 0x13));
			U8(s, 0x1A) = (uint8_t)(U8(s, 0x1A) + U8(s, 0x1B));
			U8(s, 0x22) = (uint8_t)(U8(s, 0x22) + U8(s, 0x23));
		}
		int16_t ang[3] = { (int16_t)(S8(s, 0x12) << 4), (int16_t)(S8(s, 0x1A) << 4), (int16_t)(S8(s, 0x22) << 4) };
		int16_t pos[3] = { S16(s, 0), S16(s, 2), S16(s, 4) };
		FX_HELD(if (g_held.active) TbHeldShard(s, ang, pos);)
		S16(ws, 0xC0) = ang[0];
		S16(ws, 0xC2) = ang[1];
		S16(ws, 0xC4) = ang[2];
		x::RotMatrixFromAngles(ws + 0xC0, ws + 0xE0);
		S32(ws, 0xE0 + 0x14) = pos[0];
		S32(ws, 0xE0 + 0x18) = pos[1];
		S32(ws, 0xE0 + 0x1C) = pos[2];
		x::ComposeAffine(PTR(ws, 0x84), ws + 0xE0, ws + 0xE0);
		x::SetRotMatrix(ws + 0xE0);
		x::SetTransVector(ws + 0xE0);
	}

	// 0xAC2EA0: the model of render entry e drawn (polygons not launched, in place) and its shards;
	// returns the pool cursor after the model's records (0 when the entry is not present, as vanilla)
	static uint8_t *Draw_AC2EA0(uint8_t *e, uint8_t *pool)
	{
		if (!(U8(e, 1) & 1)) return nullptr;
		uint8_t *ws = WS();                                                    // ebx
		x::BuildBoneMatricesFromPose(e + 0x14);
		Mat4x3 *par = blob::GetParentMatrix(0);
		PTR(WS(), 0x84) = (uint8_t *)par;
		x::ComputeBonesWorldMatrices(e + 0x14, par);
		x::GteSetBackColor3(U8(e, 0x28), U8(e, 0x29), U8(e, 0x2A));
		uint8_t *mats = PTR(e, 4) + 0x10;                                      // [ebp-0x38]
		const uint8_t *tab = PTR(e, 8);
		int32_t nobj = S32(tab, 0);
		tab += 4;
		for (; nobj > 0; nobj--)
		{
			const uint8_t *obj = PTR(e, 8) + U32(tab, 0) + 2;                 // esi
			tab += 4;
			uint8_t *out = PTR(CTX(), 0x74);
			PTR(ws, 0x88) = out;
			for (int32_t g = S16(obj, -2); g > 0; g--)
			{
				uint8_t *m = mats + 0x30 * S16(obj, 0) + 0x10;
				x::SetRotMatrix(m);
				x::SetTransVector(m);
				int32_t nv = S16(obj, 2);
				obj += 4;
				for (; nv > 0; nv--)
				{
					x::GteWriteData((int32_t)(((uint32_t)U16(obj, 2) << 16) | U16(obj, 0)), 0);
					x::GteWriteData((int32_t)U16(obj, 4), 1);
					x::GteRTPS();
					x::GteReadSXY2(out);
					U16(out, 4) = (uint16_t)TbGte(19);
					obj += 6;
					out += 8;
				}
			}
			const uint8_t *t = (const uint8_t *)(((uintptr_t)obj + 3) & ~(uintptr_t)3);
			const uint8_t *rec = t + 0xC;                                      // edi
			S16(ws, 0xD0) = S16(t, 4);
			S16(ws, 0xD2) = S16(t, 6);
			uint8_t *pk = PTR(CTX(), 0x7C);                                    // esi
			const uint8_t *vb = PTR(ws, 0x88);
			if (S16(ws, 0xD0) > 0)
			{
				for (; S16(ws, 0xD0) > 0; S16(ws, 0xD0) = (int16_t)(S16(ws, 0xD0) - 1), rec += 0x14, pool += 0x2C)
				{
					U8(pk, 3) = 7;
					U16(pk, 0x16) = U16(rec, 0xE);
					U16(pk, 0xE) = U16(rec, 0xA);
					U16(pk, 0xC) = U16(rec, 8);
					U16(pk, 0x14) = U16(rec, 0xC);
					U16(pk, 0x1C) = U16(rec, 6);
					if (U8(pool, 6) == 0)
					{
						U32(pk, 4) = U32(rec, 0x10);
						const uint8_t *p0 = vb + 8 * (U16(rec, 0) & 0xFFF), *p1 = vb + 8 * (U16(rec, 2) & 0xFFF), *p2 = vb + 8 * (U16(rec, 4) & 0xFFF);
						const uint32_t s0 = U32(p0, 0), s1 = U32(p1, 0), s2 = U32(p2, 0);
						if (xl_NClip_56CB30(s0, s1, s2) <= 0) continue;
						U32(pk, 8) = s0;
						U32(pk, 0x10) = s1;
						U32(pk, 0x18) = s2;
						const int32_t z = TbDiv3((int32_t)U16(p2, 4) + U16(p1, 4) + U16(p0, 4));
						xl_InsertPrimAutoDepth(((uint32_t)(z >> 2) & 0xFFFFFFFCu) + U32(RT(), 0x4C), pk);
					}
					else
					{
						int8_t life = S8(pool, 7);
						FX_HELD(if (g_held.active) life = TbHeldLife(pool);)
						if (life < 0) continue;
						U32(pk, 4) = AT<uint32_t>(PTR(WS(), 0x98), 4 * life) | 0x26000000;
						U8(pk, 0x16) |= 0x20;
						ShardStep_AC3460(pool);
						x::GteLoadV012(pool + 0xC, pool + 0x14, pool + 0x1C);
						x::GteRTPT();
						x::GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
						const uint16_t sz = (uint16_t)TbGte(19);
						U16(ws, 0xFC) = sz;
						xl_InsertPrimAutoDepth((((uint32_t)sz >> 2) & 0xFFFFFFFCu) + U32(RT(), 0x4C), pk);
					}
					pk += 0x20;
				}
			}
			if (S16(ws, 0xD2) > 0)
			{
				rec += 0xA;
				for (; S16(ws, 0xD2) > 0; S16(ws, 0xD2) = (int16_t)(S16(ws, 0xD2) - 1), rec += 0x18, pool += 0x2C)
				{
					U8(pk, 3) = 9;
					U16(pk, 0x16) = U16(rec, 4);
					U16(pk, 0xE) = U16(rec, 0);
					U16(pk, 0xC) = U16(rec, -2);
					U16(pk, 0x14) = U16(rec, 2);
					U16(pk, 0x1C) = U16(rec, 6);
					U16(pk, 0x24) = U16(rec, 8);
					if (U8(pool, 6) == 0)
					{
						U32(pk, 4) = U32(rec, 0xA);
						const uint8_t *p0 = vb + 8 * (U16(rec, -0xA) & 0xFFF), *p1 = vb + 8 * (U16(rec, -8) & 0xFFF);
						const uint8_t *p2 = vb + 8 * (U16(rec, -6) & 0xFFF), *p3 = vb + 8 * (U16(rec, -4) & 0xFFF);
						const uint32_t s0 = U32(p0, 0), s1 = U32(p1, 0), s2 = U32(p2, 0);
						if (xl_NClip_56CB30(s0, s1, s2) <= 0) continue;
						U32(pk, 8) = s0;
						U32(pk, 0x10) = s1;
						U32(pk, 0x18) = s2;
						U32(pk, 0x20) = U32(p3, 0);
						const int32_t z = ((int32_t)S16(p3, 4) + U16(p2, 4) + U16(p1, 4) + U16(p0, 4)) / 4;
						xl_InsertPrimAutoDepth(((uint32_t)(z >> 2) & 0xFFFFFFFCu) + U32(RT(), 0x4C), pk);
					}
					else
					{
						int8_t life = S8(pool, 7);
						FX_HELD(if (g_held.active) life = TbHeldLife(pool);)
						if (life < 0) continue;
						U8(pk, 0x16) |= 0x20;
						U32(pk, 4) = AT<uint32_t>(PTR(WS(), 0x98), 4 * life) | 0x2E000000;
						ShardStep_AC3460(pool);
						x::GteLoadV0(pool + 0x24);
						x::GteRTPS();
						x::GteReadSXY2(pk + 0x20);
						x::GteLoadV012(pool + 0xC, pool + 0x14, pool + 0x1C);
						x::GteRTPT();
						x::GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
						const uint16_t sz = (uint16_t)TbGte(19);
						U16(ws, 0xFC) = sz;
						xl_InsertPrimAutoDepth((((uint32_t)sz >> 2) & 0xFFFFFFFCu) + U32(RT(), 0x4C), pk);
					}
					pk += 0x28;
				}
			}
			PTR(CTX(), 0x7C) = pk;
		}
		return pool;
	}

	// 0xAC26B0 (draw 49 StageShatter): see above
	static void __cdecl dh_49_StageShatter()
	{
		if (U32(CUR(), 0xBC) == 0)
		{
			const uint8_t *d = PTR(CUR(), 0xB8);
			PTR(CUR(), 0xBC) = blob::ArenaAlloc(0xB000);
			const uint16_t life = (uint16_t)shl32(S16(d, 2), 8);
			uint8_t *p = PTR(CUR(), 0xBC) + 6;
			for (int i = 0; i < 0x400; i++, p += 0x2C)
			{
				U16(p, 0) = life;
				MEM<uint8_t>(0x1D98991) &= 0xFD;
				MEM<uint8_t>(0x1D989BD) &= 0xFD;
				MEM<uint8_t>(0x1D989E9) &= 0xFD;
				MEM<uint8_t>(0x1D98A15) |= 2;
			}
		}
		const uint8_t *d = PTR(CUR(), 0xB8);                                  // esi
		uint8_t *ref = blob::GetBone(S16(d, 4));                              // eax
		PTR(WS(), 0x98) = (uint8_t *)d + S16(d, 0);
		S32(WS(), 0x9C) = S16(d, 2);
		S32(WS(), 0x60) = S16(CUR(), 0x94);
		S32(WS(), 0x64) = S16(CUR(), 0x96);
		S32(WS(), 0x68) = S16(CUR(), 0x98);
		S32(WS(), 0x6C) = S16(CUR(), 0x8C);
		S32(WS(), 0x80) = mul32(S32(WS(), 0x6C), S32(WS(), 0x6C));
		S32(WS(), 0x70) = S16(ref, 0x96);
		S32(WS(), 0x74) = S16(ref, 0x8E);
		S32(WS(), 0x90) = S16(ref, 0x98);
		S32(WS(), 0x94) = S16(ref, 0x90);
		uint8_t *pool = PTR(CUR(), 0xBC);
		FX_HELD(if (!g_held.active) TbNoteBegin(pool);)
		static uint8_t *const ENT[3] = { (uint8_t *)0x1D98990, (uint8_t *)0x1D989BC, (uint8_t *)0x1D989E8 };
		for (int k = 0; k < 3; k++)
		{
			// 30 fps layer: launches on real ticks only; the group skeleton the draw rebuilds is put back
			FX_HELD(if (g_held.active) { pool = TbHeldDraw(ENT[k], pool); continue; })
			Launch_AC2890(ENT[k], pool);
			pool = Draw_AC2EA0(ENT[k], pool);
		}
	}

	// ------------------------------------------------------------------------------------
	// handler set: the shared handlers this module carries (only these slots taken from the shared
	// sets), then Terra Break's own
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		const VoidFn d16 = c.draw[16];        // 0xABE340 = Bahamut 0xB198C0 ScrollTextureU
		apply_eden_c(c);
		const VoidFn d58 = c.draw[58];        // 0xABFB20 = Eden 0xAEA550 RequestScreenFeedback
		apply_eden_d(c);
		const VoidFn p6 = c.prim[6];          // 0xAC0CE0 = Eden 0xAEB5B0 TintedF3
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.draw[16] = d16;
		c.draw[58] = d58;
		c.prim[6] = p6;
		apply_shared_ribbon(c);               // draw 37 0xABE840
		c.vm[0x0AD] = op_0AD_PosFromPosedEntityJoint;   // 0xAC92E0
		c.vm[0x0B3] = op_0B3_SteerTowardBone;           // 0xAC7800
		c.vm[0x12E] = op_12E_ApplyNextTargetResult;     // 0xAC53A0
		c.draw[49] = dh_49_StageShatter;                // 0xAC26B0
		c.draw[67] = dh_67_FoggedGridSheet;             // 0xAC2230
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag219_terra_break()
	{
		using namespace gfc;
		terra219::describe(terra219::g_c);
		init_clone(terra219::g_c, nullptr, 0);
		terra219::apply_slots(terra219::g_c);
		register_port(0xABDA90, (void *)terra219::SequenceTask, "MAG219 Terra Break SequenceTick", 219);
		// 30 fps layer: see mag219_terra_break_held.inc
		FX_HELD(register_mag219_terra_break_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag219_terra_break_held.inc"
#endif
