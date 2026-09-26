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
// GF cinematic engine (Ifrit 201, Leviathan 006, Bahamut 202, Cerberus 203, Alexander 204,
// Brothers 205, Eden 206): native port, bit-exact against the Ifrit clone (see gfc_engine.h).
// Parts, in order: core (tick, context, channel schedulers, integrator + bone handlers, draw
// dispatcher, InitBones, held frames: prediction and in-between draw, clone tables), VM opcodes
// (vm_a .. vm_d), mesh draw handlers + DrawMeshObject (draw_mesh), primitive renderers
// (draw_prim), sprites + particle system (draw_sprite). Every port names its original Ifrit
// address; listings: gf_study/tools/ff8dis.py.

#include "gfc_engine.h"

// ============================================================================================
// part core
// ============================================================================================
// ============================================================================================
// core: context binding, the task tick, the VM channel schedulers, the integrator and its bone
// handlers, the draw dispatcher, InitBones; held frames (prediction + in-between draw).
// ============================================================================================

namespace ff8fx
{
namespace gfc
{
	bool g_predict = false;
#ifdef GFC_DEBUG_HOOK
	void (*g_dbg_hook)(int kind, uint32_t id) = nullptr;
	void (*g_dbg_taint)(void *dst, const void *src, int n) = nullptr;
#endif
	Held g_held = { false, 0, 1, 0 };
	Clone *g_clone = nullptr;
	Generic g_generic;

namespace part_core
{
	// ----------------------------------------------------------------------------------------
	// 0xB26010 (Ifrit GF_201Ifrit_DebugPadRootControl): developer free camera, only with
	// ctx->flags & 0x8000; ctx+0x4C = debug mode bits, ctx+0x40 = pad bits.
	// ----------------------------------------------------------------------------------------
	static int32_t DebugPadRootControl()
	{
		uint8_t *ctx = CTX();
		if (U8(ctx, 0x4C) & 0x10)
		{
			uint32_t pad = U32(ctx, 0x40);
			if (pad & 0x8000) MEM<int16_t>(0x2797782) = (int16_t)(MEM<int16_t>(0x2797782) + 0x20);
			if (pad & 0x2000) MEM<int16_t>(0x2797782) = (int16_t)(MEM<int16_t>(0x2797782) - 0x20);
			if (pad & 0x1000) MEM<int16_t>(0x2797780) = (int16_t)(MEM<int16_t>(0x2797780) - 0x20);
			if (pad & 0x4000) MEM<int16_t>(0x2797780) = (int16_t)(MEM<int16_t>(0x2797780) + 0x20);
			if (pad & 4) MEM<int16_t>(0x2797784) = (int16_t)(MEM<int16_t>(0x2797784) - 0x20);
			if (pad & 8) MEM<int16_t>(0x2797784) = (int16_t)(MEM<int16_t>(0x2797784) + 0x20);
			if (pad & 1) MEM<int32_t>(0x2797778) = sub32(MEM<int32_t>(0x2797778), 0x20);
			if (pad & 2) MEM<int32_t>(0x2797778) = add32(MEM<int32_t>(0x2797778), 0x20);
		}
		if (U8(ctx, 0x4C) & 0x40)
		{
			uint32_t pad = U32(ctx, 0x40);
			if (pad & 0x8000) MEM<int32_t>(0x2797770) = sub32(MEM<int32_t>(0x2797770), 0x100);
			if (pad & 0x2000) MEM<int32_t>(0x2797770) = add32(MEM<int32_t>(0x2797770), 0x100);
			if (pad & 0x1000) MEM<int32_t>(0x2797774) = sub32(MEM<int32_t>(0x2797774), 0x100);
			if (pad & 0x4000) MEM<int32_t>(0x2797774) = add32(MEM<int32_t>(0x2797774), 0x100);
			if (pad & 1) MEM<int32_t>(0x2797778) = sub32(MEM<int32_t>(0x2797778), 0x100);
			if (pad & 2) MEM<int32_t>(0x2797778) = add32(MEM<int32_t>(0x2797778), 0x100);
			if (pad & 4) MEM<int32_t>(0x2797778) = sub32(MEM<int32_t>(0x2797778), 0x10);
			if (pad & 8) MEM<int32_t>(0x2797778) = add32(MEM<int32_t>(0x2797778), 0x10);
		}
		return 0;
	}

	// ----------------------------------------------------------------------------------------
	// Bone handlers (BoneHandlerTable[bone+0x18], called by the integrator after the
	// accumulators were integrated): they produce outPos (+0x94/96/98).
	// ----------------------------------------------------------------------------------------
	// 0xB262E0 (bone 0, 1): outPos = HIWORD(accumPos)
	static void __cdecl bh_00_AccumPos()
	{
		S16(CUR(), 0x94) = (int16_t)(S32(CUR(), 0x5C) >> 16);
		S16(CUR(), 0x96) = (int16_t)(S32(CUR(), 0x60) >> 16);
		S16(CUR(), 0x98) = (int16_t)(S32(CUR(), 0x64) >> 16);
	}

	// 0xB26320 (bone 5): outPos = outPos of the bone referenced by *(s16 *)(bone+0xA0)
	static void __cdecl bh_05_CopyBone()
	{
		uint8_t *o = blob::GetBone(S16(PTR(CUR(), 0xA0), 0));
		S16(CUR(), 0x94) = S16(o, 0x94);
		S16(CUR(), 0x96) = S16(o, 0x96);
		S16(CUR(), 0x98) = S16(o, 0x98);
	}

	// 0xB26380 (bone 7): outPos = referenced bone's outPos + HIWORD(accumPos) (no-op without
	// a reference pointer)
	static void __cdecl bh_07_BonePlusAccum()
	{
		uint8_t *ref = PTR(CUR(), 0xA0);
		if (!ref) return;
		uint8_t *o = blob::GetBone(S16(ref, 0));
		S16(CUR(), 0x94) = (int16_t)((S32(CUR(), 0x5C) >> 16) + S16(o, 0x94));
		S16(CUR(), 0x96) = (int16_t)((S32(CUR(), 0x60) >> 16) + S16(o, 0x96));
		S16(CUR(), 0x98) = (int16_t)((S32(CUR(), 0x64) >> 16) + S16(o, 0x98));
	}

	// 0xB263F0 (bone 3): polar around the bone S16(bone+0xB0): radius = accumPosX >> 8,
	// angle = HIWORD(accumPosZ), height = HIWORD(accumPosY)
	static void __cdecl bh_03_Polar()
	{
		uint8_t *o = blob::GetBone(S16(CUR(), 0xB0));
		int32_t ang = S32(CUR(), 0x64) >> 16;
		int32_t c = x::Cos(ang);
		S16(CUR(), 0x98) = (int16_t)((mul32(S32(CUR(), 0x5C) >> 8, c >> 4) >> 16) + S16(o, 0x98));
		int32_t s = x::Sin(ang);
		S16(CUR(), 0x94) = (int16_t)((mul32(S32(CUR(), 0x5C) >> 8, s >> 4) >> 16) + S16(o, 0x94));
		S16(CUR(), 0x96) = (int16_t)((S32(CUR(), 0x60) >> 16) + S16(o, 0x96));
	}

	// 0xB264B0 (bone 6): outPos = lerp(boneA.outPos, boneB.outPos, HIWORD(accumRot)/256) +
	// HIWORD(accumPos); A/B = *(s16 *)(bone+0xA0) [0] / [1]
	static void __cdecl bh_06_LerpBones()
	{
		const uint8_t *refs = PTR(CUR(), 0xA0);
		uint8_t *a = blob::GetBone(S16(refs, 0));
		uint8_t *b = blob::GetBone(S16(refs, 2));
		for (int k = 0; k < 3; k++)
		{
			uint8_t *bone = CUR();
			int16_t av = S16(a, 0x94 + 2 * k);
			int32_t d = mul32((int32_t)S16(b, 0x94 + 2 * k) - av, S32(bone, 0x50 + 4 * k) >> 16) / 256;
			S16(bone, 0x94 + 2 * k) = (int16_t)((S32(bone, 0x5C + 4 * k) >> 16) + av + d);
		}
	}

	// 0xB265B0 (bone 9): follows a joint of a battle entity: entity = SceneHeader+0x60 +
	// 4 * bone[0x1B]; per axis (mask bits 0x1000/0x800/0x400 of *(s16 *)bone+0xA0) outPos =
	// HIWORD(accumPos) + joint[axis] * scale / 256 (joint words at +8/+A/+C, scale at +2 of
	// the first record of entity+0x64), else HIWORD(accumPos)
	static void __cdecl bh_09_EntityJoint()
	{
		uint8_t *bone = CUR();
		uint8_t *ent = PTR(SCENE(), 0x60 + 4 * U8(bone, 0x1B));
		int32_t px = S32(bone, 0x5C) >> 16;
		uint8_t *rec = PTR(PTR(ent, 0x64), 0);
		int32_t mask = S16(PTR(bone, 0xA0), 0);
		int32_t scale = S16(rec, 2);
		if (mask & 0x1000) S16(bone, 0x94) = (int16_t)(mul32(S16(rec, 8), scale) / 256 + px);
		else S16(bone, 0x94) = (int16_t)px;
		int32_t py = S32(bone, 0x60) >> 16;
		if (mask & 0x800) S16(bone, 0x96) = (int16_t)(mul32(S16(rec, 0xA), scale) / 256 + py);
		else S16(bone, 0x96) = (int16_t)py;
		int32_t pz = S32(bone, 0x64) >> 16;
		if (mask & 0x400) S16(bone, 0x98) = (int16_t)(mul32(S16(rec, 0xC), scale) / 256 + pz);
		else S16(bone, 0x98) = (int16_t)pz;
	}

	// ----------------------------------------------------------------------------------------
	// VM channel scheduler (0xB2FCF0 Neg / 0xB30110 Pos): for every bone of the order list
	// (frozen bones skipped while rt->boneSkipFlag, unless marked 0x80), for each of its three
	// channels whose stream pointer is < 0 (Neg) / > 0 (Pos): count its wait down by the bone's
	// wait speed, or run opcodes until one requests a wait (rt->vmWaitRequest), which is added
	// to the channel wait minus the speed; the stream pointer is stored back.
	// ----------------------------------------------------------------------------------------
	template<bool NEG>
	static void AnimChannels()
	{
		if (NEG)
		{
			U16(RT(), 0x3E) = 0;
			U32(RT(), 0x4C) = U32(RT(), 0x38);
		}
		else U16(RT(), 0x3E) = 0;
		U8(RT(), 0x44) = 0;
		uint32_t e = ORDER()[U8(RT(), 0x44)];
		if (e == 0xFF) return;
		for (;;)
		{
			uint8_t *rt = RT();
			if (U8(rt, 0x45) == 0 || (e & 0x80))
			{
				U8(rt, 0x42) = (uint8_t)(e & 0x7F);
				CUR() = PTR(CTX(), 0x90) + ((uint32_t)U8(RT(), 0x42) << 8);
				U8(RT(), 0x43) = 0;
				do
				{
					uint8_t *bone = CUR();
					int32_t sp = S32(bone, 4 * U8(RT(), 0x43));
					STREAM() = (uint8_t *)sp;
					if (NEG ? sp < 0 : sp > 0)
					{
						int16_t *wait = &S16(bone, 0xC + 2 * U8(RT(), 0x43));
						if (*wait > 0) *wait = (int16_t)(*wait - S16(bone, 0xC8));
						else
						{
							int16_t op = S16((uint8_t *)sp, 0);
							S16(RT(), 0x4A) = op;
							call_vm((uint32_t)(int32_t)op);
							while (S16(RT(), 0x3E) == 0)
							{
								op = S16(STREAM(), 0);
								S16(RT(), 0x4A) = op;
								call_vm((uint32_t)(int32_t)op);
							}
							uint8_t ch = U8(RT(), 0x43);
							int16_t req = S16(RT(), 0x3E);
							uint8_t *cb = CUR();
							req = (int16_t)(req - S16(cb, 0xC8));
							S16(cb, 0xC + 2 * ch) = (int16_t)(S16(cb, 0xC + 2 * ch) + req);
							U16(RT(), 0x3E) = 0;
							S32(CUR(), 4 * U8(RT(), 0x43)) = (int32_t)STREAM();
						}
					}
					U8(RT(), 0x43) = (uint8_t)(U8(RT(), 0x43) + 1);
				} while (U8(RT(), 0x43) < 3);
			}
			rt = RT();
			U8(rt, 0x44) = (uint8_t)(U8(rt, 0x44) + 1);
			e = ORDER()[U8(RT(), 0x44)];
			if (e == 0xFF) break;
		}
	}

	// ----------------------------------------------------------------------------------------
	// 0xB26110 AnimIntegrator: per bone (freeze rules as above): vel += accel << 12 (rotation
	// if flags & 1, position if flags & 8), accum += vel, outAngle = HIWORD(accumRot), then
	// BoneHandlerTable[bone+0x18] builds outPos.
	// ----------------------------------------------------------------------------------------
	static void Integrator()
	{
		U8(RT(), 0x44) = 0;
		uint32_t e = ORDER()[U8(RT(), 0x44)];
		if (e == 0xFF) return;
		for (;;)
		{
			uint8_t *rt = RT();
			if (U8(rt, 0x45) == 0 || (e & 0x80))
			{
				U8(rt, 0x42) = (uint8_t)(e & 0x7F);
				uint8_t *b = PTR(CTX(), 0x90) + ((uint32_t)U8(RT(), 0x42) << 8);
				CUR() = b;
				uint8_t fl = U8(b, 0x1A);
				if (fl & 1)
				{
					S32(CUR(), 0x68) = add32(S32(CUR(), 0x68), shl32(S16(CUR(), 0x80), 12));
					S32(CUR(), 0x6C) = add32(S32(CUR(), 0x6C), shl32(S16(CUR(), 0x82), 12));
					S32(CUR(), 0x70) = add32(S32(CUR(), 0x70), shl32(S16(CUR(), 0x84), 12));
				}
				if (fl & 8)
				{
					S32(CUR(), 0x74) = add32(S32(CUR(), 0x74), shl32(S16(CUR(), 0x86), 12));
					S32(CUR(), 0x78) = add32(S32(CUR(), 0x78), shl32(S16(CUR(), 0x88), 12));
					S32(CUR(), 0x7C) = add32(S32(CUR(), 0x7C), shl32(S16(CUR(), 0x8A), 12));
				}
				for (int k = 0; k < 6; k++)
					S32(CUR(), 0x50 + 4 * k) = add32(S32(CUR(), 0x50 + 4 * k), S32(CUR(), 0x68 + 4 * k));
				S16(CUR(), 0x8C) = (int16_t)(S32(CUR(), 0x50) >> 16);
				S16(CUR(), 0x8E) = (int16_t)(S32(CUR(), 0x54) >> 16);
				S16(CUR(), 0x90) = (int16_t)(S32(CUR(), 0x58) >> 16);
				C().bone[U8(CUR(), 0x18)]();
#ifdef GFC_DEBUG_HOOK
				if (g_dbg_hook) g_dbg_hook(2, U8(CUR(), 0x18));
#endif
			}
			rt = RT();
			U8(rt, 0x44) = (uint8_t)(U8(rt, 0x44) + 1);
			e = ORDER()[U8(RT(), 0x44)];
			if (e == 0xFF) break;
		}
	}

	// ----------------------------------------------------------------------------------------
	// 0xB2ABE0 BuildMatricesAndDraw (Ifrit/Leviathan "lit" variant; the other five clones are the
	// same without the light block): for every bone of the draw list: render-list cursor from
	// bone+0x48, light set g_GfCinematic_BoneMatrixTable[bone+0xE1 & 0xF] into the GTE (colour
	// matrix, back colour, light matrix [x parent node if bit 7] scaled by the bone colour
	// bytes x 32), alternate packet pool when bone+0x4C & 1, then DrawHandlerTable[bone+0x1C].
	// ----------------------------------------------------------------------------------------
	static void Dispatch()
	{
		U32(WS(), 0) = 0;
		U32(WS(), 4) = U32(CTX(), 0x7C);
		uint8_t id = DRAWORDER()[U32(WS(), 0)];
		if (id == 0xFF) return;
		for (;;)
		{
			U8(RT(), 0x42) = id;
			uint8_t *rt = RT();
			uint8_t *b = PTR(CTX(), 0x90) + ((uint32_t)U8(rt, 0x42) << 8);
			CUR() = b;
			U32(rt, 0x4C) = U32(rt, 0x38) + 4 * (U16(b, 0x48) & 0xFFF);
			if (C().lit_dispatcher)
			{
				int32_t slot = S8(CUR(), 0xE1);
				if (slot != 0)
				{
					uint8_t *lt = LIGHTS() + 0x50 * (slot & 0xF);
					x::SetColorMatrix(lt + 0x20);
					x::SetBackColor(S32(lt, 0x40), S32(lt, 0x44), S32(lt, 0x48));
					S32(WS(), 0x80) = (S32(CUR(), 0xCC) & 0xFF) << 5;
					S32(WS(), 0x84) = (S32(CUR(), 0xCC) >> 3) & 0x1FE0;
					S32(WS(), 0x88) = (S32(CUR(), 0xCC) >> 11) & 0x1FE0;
					uint8_t *m = WS() + 0x20;
					if (slot & 0x80)
					{
						PTR(WS(), 0xFC) = (uint8_t *)blob::GetParentMatrix(U16(CUR(), 0x9C));
						x::MulMatrix3(lt, PTR(WS(), 0xFC), m);
						x::ScaleMatrix(m, WS() + 0x80);
						x::SetLightMatrix(m);
					}
					else
					{
						memcpy(m, lt, 20);
						x::ScaleMatrix(m, WS() + 0x80);
						x::SetLightMatrix(m);
					}
				}
			}
			if (U8(CUR(), 0x4C) & 1)
			{
				U32(WS(), 4) = U32(CTX(), 0x7C);
				U32(CTX(), 0x7C) = U32(CTX(), 0xD8);
				C().draw[U8(CUR(), 0x1C)]();
#ifdef GFC_DEBUG_HOOK
				if (g_dbg_hook) g_dbg_hook(1, U8(CUR(), 0x1C));
#endif
				U32(CTX(), 0xD8) = U32(CTX(), 0x7C);
				U32(CTX(), 0x7C) = U32(WS(), 4);
			}
			else C().draw[U8(CUR(), 0x1C)]();
#ifdef GFC_DEBUG_HOOK
			if (g_dbg_hook) g_dbg_hook(1, U8(CUR(), 0x1C));
#endif
			U32(WS(), 0) = U32(WS(), 0) + 1;
			id = DRAWORDER()[U32(WS(), 0)];
			if (id == 0xFF) break;
		}
	}
}

	// ============================================================================================
	// exported engine functions
	// ============================================================================================

	// 0xB25830 (au_re_bs_modulo_53): Workspace (0x100) + RuntimeSlot (0x80) on the battle scratch
	// stack, pointers reloaded from the module's static pointer block
	void BindContext()
	{
		uint8_t *a = (uint8_t *)x::FieldAlloc(0x180);
		uint8_t *rt = a + 0x100;
		const uint32_t *pb = (const uint32_t *)C().ptr_block;
		PTR(rt, 0) = a;
		WS() = a;
		PTR(rt, 4) = rt;
		RT() = rt;
		U32(rt, 8) = pb[0];
		CTX() = (uint8_t *)pb[0];
		U32(rt, 0xC) = pb[1];
		PTR3() = (uint8_t *)pb[1];
		U32(rt, 0x10) = pb[2];
		SCENE() = (uint8_t *)pb[2];
		U32(rt, 0x14) = pb[3];
		RCTX() = (uint8_t *)pb[3];
		U32(rt, 0x18) = pb[4];
		SEQ() = (uint8_t *)pb[4];
	}

	// 0xB258C0 (GetSequenceStatus)
	void ReleaseContext() { x::FieldFree(0x180); }

	void AnimChannelsNeg() { part_core::AnimChannels<true>(); }
	void AnimChannelsPos() { part_core::AnimChannels<false>(); }
	void AnimIntegrator() { part_core::Integrator(); }
	void BuildMatricesAndDraw() { part_core::Dispatch(); }

	// 0xB2F8F0 GF_Ifrit_InitBones: bone array from the .01 file, order/draw lists cleared,
	// scene defaults, root program on bone 0, battle entity tables copied from 0x1D972C0..
	int32_t h_B2F8F0()
	{
		U32(SEQ(), 8) = 0x80000000;
		U32(CTX(), 0x74) = U32(CTX(), 0x70);
		uint8_t *f1 = RESFILE()[1];
		PTR(CTX(), 0x90) = f1 + U32(f1, 0x1C);
		uint8_t *bones = PTR(CTX(), 0x90);
		memset(ORDER(), 0xFF, 0x80);
		CUR() = bones;
		memset(DRAWORDER(), 0xFF, 0x80);
		ORDER()[0] = 0;
		U16(SCENE(), 2) = 0;
		U16(SCENE(), 0x18) = 1;
		U16(SCENE(), 0x1A) = 0;
		U16(SCENE(), 0) = 0x8000;
		U32(SCENE(), 0x40) = 0;
		U32(SEQ(), 0xC) = 0x4000080;
		U32(SCENE(), 0x14) = 0;
		U16(SCENE(), 0x38) = 8;
		U8(SCENE(), 0x3A) = 0;
		MEM<uint8_t>(0x2798219) = 0;
		MEM<uint8_t>(0x2798218) = 0xFF;
		U8(RT(), 0x45) = 0;
		U8(CTX(), 0xA2) = 0;
		U32(SCENE(), 0x170) = 0;
		U32(SCENE(), 0x16C) = 0;
		U32(SCENE(), 0x168) = 0;
		uint8_t *sc = SCENE();
		memset(NODEKEYS(), 0, 0x80);
		uint8_t *b = CUR();
		if (U16(sc, 0x38) != 0)
		{
			uint32_t i = 0;
			do
			{
				U32(b, 0) = 0;
				b += 0x100;
				i++;
			} while (i < U16(SCENE(), 0x38));
		}
		blob::InitBone(0);
		U16(CUR(), 0x12) = 0;
		h_B2FC20();
		uint8_t *s0 = SCENE();
		for (int k = 0; k < 7; k++)
		{
			uint32_t e = 0x1D97358 + 0x9C * k;
			uint8_t *s = SCENE();
			U32(s, 0x60 + 4 * k) = e - 0x98;
			U32(s0, 0x84 + 8 * k) = MEM<uint32_t>(e - 0x7C);
			U32(s0, 0x88 + 8 * k) = MEM<uint32_t>(e - 0x78);
			U32(s0, 0x120 + 8 * k) = MEM<uint32_t>(e - 4);
			U32(s0, 0x124 + 8 * k) = MEM<uint32_t>(e);
			U32(s0, 0xD8 + 8 * k) = MEM<uint32_t>(e - 0x8C);
			U32(s0, 0xDC + 8 * k) = MEM<uint32_t>(e - 0x88);
		}
		U32(CUR(), 0) = U32(CTX(), 0x94);
		return 0;
	}

	// 0xB2FC20: cast context -> scene: caster (SceneHeader+0x44), target list (SceneHeader+0x48..,
	// bit set dedup, SceneHeader+0xCC[target] = slot), entity words 0x1D972C0 step 0x9C ->
	// SceneHeader+0x4E.., bone+0x1B = first target, SceneHeader+0x41 = target count
	int32_t h_B2FC20()
	{
		uint8_t *ctx = CTX();
		uint8_t *cast = PTR(ctx, 0xC0);
		uint8_t *list = PTR(cast, 4);
		PTR(ctx, 0xCC) = list + 20 * (uint32_t)U8(ctx, 0xD0);
		uint8_t *act = PTR(CTX(), 0xCC);
		U8(SCENE(), 0x44) = U8(cast, 0);
		U8(SCENE(), 0x48) = U8(PTR(PTR(cast, 4), 8), 0);
		uint8_t *dst = SCENE() + 0x4E;
		for (int k = 0; k < 7; k++) U16(dst, 2 * k) = MEM<uint16_t>(0x1D972C0 + 0x9C * k);
		const uint8_t *t = PTR(act, 8);
		int32_t n = U8(act, 0x10);
		uint32_t mask = 0;
		uint32_t cnt = 0;
		if (n > 0)
		{
			for (int32_t i = n; i != 0; i--)
			{
				uint8_t id = t[0];
				uint32_t bit = 1u << (id & 31);
				if (!(bit & mask))
				{
					mask |= bit;
					U8(SCENE(), 0x48 + cnt) = id;
					U8(SCENE(), 0xCC + id) = (uint8_t)cnt;
					cnt++;
				}
				t += 0x18;
			}
		}
		U8(CUR(), 0x1B) = U8(SCENE(), 0x48);
		U8(SCENE(), 0x41) = (uint8_t)cnt;
		return 0;
	}

	// ============================================================================================
	// held frames: state of the real tick, prediction of the next one, in-between draw
	// ============================================================================================
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	// Workspace + RuntimeSlot of the last real tick, copied at its end: held frames and the
	// prediction run on this private copy (the live scratch-stack block may be reused by other
	// battle code between two ticks; vanilla's next tick gets it back at the same address)
	alignas(16) static uint8_t g_scratch_copy[0x180];
	static uint8_t *g_scratch = nullptr;

	// --- journal of out-of-snapshot writes while predicting ---
	struct JournalEntry { uint8_t *p; int n; uint32_t off; };
	static JournalEntry g_jr[4096];
	static uint8_t g_jr_bytes[0x40000];
	static int g_njr = 0;
	static uint32_t g_jr_used = 0;
	static bool g_jr_overflow = false;
	static bool g_cam_written = false;

	void guard_record(const void *p, int n)
	{
		if ((uint32_t)p < 0xB8B800 && (uint32_t)p + n > 0xB8B7F0) g_cam_written = true;
		if (g_njr >= 4096 || g_jr_used + n > sizeof(g_jr_bytes) || n <= 0) { g_jr_overflow = true; return; }
		g_jr[g_njr] = { (uint8_t *)p, n, g_jr_used };
		memcpy(g_jr_bytes + g_jr_used, p, n);
		g_jr_used += n;
		g_njr++;
	}

	static void journal_restore()
	{
		for (int i = g_njr - 1; i >= 0; i--) memcpy(g_jr[i].p, g_jr_bytes + g_jr[i].off, g_jr[i].n);
		g_njr = 0;
		g_jr_used = 0;
	}

	// --- saved regions ---
	struct Region { uint8_t *p; uint32_t n; uint8_t *copy; uint32_t cap; };
	static void region_save(Region &r, void *p, uint32_t n)
	{
		if (n > r.cap)
		{
			delete[] r.copy;
			r.cap = (n + 0xFFF) & ~0xFFFu;
			r.copy = new uint8_t[r.cap];
		}
		r.p = (uint8_t *)p;
		r.n = n;
		if (n) memcpy(r.copy, p, n);
	}
	static void region_restore(const Region &r) { if (r.n) memcpy(r.p, r.copy, r.n); }

	static Region g_r_state, g_r_bones, g_r_arena, g_r_scratch, g_r_gte, g_r_free;

	// engine renderers called by the draw (BuildBoneMatricesFromPose, RenderGeometry...) take
	// temporaries from the battle scratch stack (Field_Alloc 0x5082B0, pointer 0x1D999C4):
	// the free area above the pointer is saved and restored around held draws / predictions
	static const uint32_t FREE_SCRATCH = 0x8000;
	static void SaveFreeScratch() { region_save(g_r_free, (void *)MEM<uint32_t>(0x1D999C4), FREE_SCRATCH); }

	static uint32_t *CrtSeed() { return (uint32_t *)(x::f<uint8_t *(__cdecl *)()>(0x560578)() + 0x14); }

	// bones that may be live: SceneHeader+0x38 is only the initial allocation count, SpawnBone
	// (VM 0x032 ...) takes the first bone whose stream word is 0 without bound, so bones past it
	// are used by the clone scripts: the highest index on the order / draw lists, plus room for the
	// bones a (predicted) tick can spawn
	static uint32_t ActiveBones(const uint8_t *scene)
	{
		uint32_t n = U16(scene, 0x38);
		uint32_t hi = 0;
		for (int k = 0; k < 256 && ORDER()[k] != 0xFF; k++) if ((ORDER()[k] & 0x7Fu) + 1 > hi) hi = (ORDER()[k] & 0x7Fu) + 1;
		for (int k = 0; k < 0xD0 && DRAWORDER()[k] != 0xFF; k++) if (DRAWORDER()[k] + 1u > hi) hi = DRAWORDER()[k] + 1u;
		hi += 8;
		if (hi > n) n = hi;
		return n > 128 ? 128 : n;
	}
	static uint32_t BoneCount() { return ActiveBones(SCENE()); }

	// the globals a context bind sets, without allocating (the scratch block of the real tick)
	static void BindHeld()
	{
		uint8_t *a = g_scratch;
		uint8_t *rt = a + 0x100;
		PTR(rt, 0) = a;
		PTR(rt, 4) = rt;
		const uint32_t *pb = (const uint32_t *)C().ptr_block;
		WS() = a;
		RT() = rt;
		CTX() = (uint8_t *)pb[0];
		PTR3() = (uint8_t *)pb[1];
		SCENE() = (uint8_t *)pb[2];
		RCTX() = (uint8_t *)pb[3];
		SEQ() = (uint8_t *)pb[4];
	}

	static void SaveEngine(bool arena)
	{
		region_save(g_r_state, (void *)STATE_LO, STATE_HI - STATE_LO);
		region_save(g_r_scratch, g_scratch, 0x180);
		const uint32_t *pb = (const uint32_t *)C().ptr_block;
		uint8_t *ctx = (uint8_t *)pb[0], *scene = (uint8_t *)pb[2];
		// all 128 bone slots (32 KB): scripts initialise / spawn / end bones past the live range
		(void)scene;
		region_save(g_r_bones, PTR(ctx, 0x90), 128 * 0x100);
		if (arena)
		{
			uint32_t lo = U32(ctx, 0x70), hi = U32(ctx, 0x74);
			region_save(g_r_arena, (void *)lo, hi > lo && hi - lo < 0x400000 ? hi - lo : 0);
		}
	}
	static void RestoreEngine(bool arena)
	{
		if (arena) region_restore(g_r_arena);
		region_restore(g_r_bones);
		region_restore(g_r_scratch);
		region_restore(g_r_state);
	}

	// --- the predicted next tick ---
	struct PredBone { uint16_t id16; uint8_t alive, draw; int16_t ang[3], pos[3]; uint32_t colour; };
	struct Pred
	{
		uint32_t tick;
		bool ok;
		bool paused;
		uint32_t nb;
		PredBone b[128];
		Mat4x3 node[64];
		uint16_t keys[64];
		uint8_t lights[5 * 0x50];
		Mat4x3 bb[4];
		bool cam;
		int16_t eye[3], at[3];
	};
	static Pred g_pred = { 0xFFFFFFFF, false };

	static void CaptureBones(PredBone *out, uint32_t nb)
	{
		uint8_t *bones = PTR(CTX(), 0x90);
		for (uint32_t i = 0; i < nb; i++)
		{
			uint8_t *b = bones + 0x100 * i;
			PredBone &p = out[i];
			p.id16 = U16(b, 0x12);
			p.alive = 0;
			p.draw = U8(b, 0x1C);
			for (int k = 0; k < 3; k++) { p.ang[k] = S16(b, 0x8C + 2 * k); p.pos[k] = S16(b, 0x94 + 2 * k); }
			p.colour = U32(b, 0xCC);
		}
		for (int k = 0; k < 256 && ORDER()[k] != 0xFF; k++)
		{
			uint32_t i = ORDER()[k] & 0x7F;
			if (i < nb) out[i].alive = 1;
		}
	}

	// runs the VM part of the next tick (preamble + Neg/Integrator/Pos) on the real state with
	// every out-of-state side effect disabled, captures the result and puts everything back
	static void Predict(Clone &c)
	{
		if (g_pred.tick == g_real_tick) return;
		g_pred.tick = g_real_tick;
		g_pred.ok = false;
		if (!g_scratch) return;
		region_save(g_r_gte, (void *)0x1CA8A00, 0xA00);
		SaveFreeScratch();
		uint32_t seed = *CrtSeed();
		uint8_t *saved_globals[8] = { RT(), WS(), CTX(), SCENE(), RCTX(), SEQ(), CUR(), STREAM() };
		uint8_t *saved_ptr3 = PTR3();
		int16_t cam0[8];
		memcpy(cam0, (void *)0xB8B7F0, 16);
		int16_t projh = PROJH();
		BindHeld();
		SaveEngine(true);
		g_predict = true;
		g_njr = 0; g_jr_used = 0; g_jr_overflow = false; g_cam_written = false;
		guard((void *)0xB8B7F0, 0x20);  // camera words + saved copy (op 0x39 sub-op 4)
		guard((void *)0x1D8E038, 2);
		guard((void *)0x1D9771C, 2);
		guard((void *)0x1D977A0, 4);
		g_cam_written = false;

		// the next tick's preamble (0xB25DF0), without the debug pad / OT clear
		S16(CTX(), 0x32) = (int16_t)(S16(CTX(), 0x32) + 1);
		U8(RT(), 0x41) = U8(CTX(), 0x32) & 1;
		x::Rand();
		if (U16(CTX(), 0) & 0x400)
		{
			U32(RCTX(), 0x2C) = FRAMERL() + 0xC;
			U16(RCTX(), 0x24) = 0;
		}
		U8(CTX(), 0x35) = (UPDFLAGS() & 1) ? 0xFF : 0;
		U8(RT(), 0x40) = U8(RT(), 0x41);
		U32(RT(), 0x38) = FRAMERL() + 0x44;
		{
			const uint32_t *v = (const uint32_t *)0x1D97778;
			uint32_t *m0 = (uint32_t *)NODEMAT(), *ma = (uint32_t *)CAMALT();
			for (int k = 0; k < 8; k++) { m0[k] = v[k]; ma[k] = v[k]; }
			m0[4] &= 0xFFFF;
			ma[4] &= 0xFFFF;
		}
		U8(CTX(), 0xD2) = 0;
		g_pred.paused = U8(CTX(), 0x35) != 0;
		if (!g_pred.paused)
		{
			AnimChannelsNeg();
			AnimIntegrator();
			AnimChannelsPos();
		}
		g_pred.nb = BoneCount();
		CaptureBones(g_pred.b, g_pred.nb);
		memcpy(g_pred.node, NODEMAT(), sizeof(g_pred.node));
		memcpy(g_pred.keys, NODEKEYS(), sizeof(g_pred.keys));
		memcpy(g_pred.lights, LIGHTS(), sizeof(g_pred.lights));
		memcpy(g_pred.bb, BBMAT(), sizeof(g_pred.bb));
		g_pred.cam = g_cam_written;
		memcpy(g_pred.eye, (void *)0xB8B7F0, 6);
		memcpy(g_pred.at, (void *)0xB8B7F8, 6);
		g_pred.ok = !g_jr_overflow;

		g_predict = false;
		journal_restore();
		RestoreEngine(true);
		memcpy((void *)0xB8B7F0, cam0, 16);
		PROJH() = projh;
		RT() = saved_globals[0]; WS() = saved_globals[1]; CTX() = saved_globals[2]; SCENE() = saved_globals[3];
		RCTX() = saved_globals[4]; SEQ() = saved_globals[5]; CUR() = saved_globals[6]; STREAM() = saved_globals[7];
		PTR3() = saved_ptr3;
		*CrtSeed() = seed;
		region_restore(g_r_free);
		region_restore(g_r_gte);
	}

	// in-between value, holding on large jumps (teleports, re-spawns)
	static inline int16_t mix16(int16_t a, int16_t b, int num, int den, int32_t cut)
	{
		int32_t d = (int16_t)(b - a);
		if (d > cut || d < -cut) return a;
		return (int16_t)(a + d * num / den);
	}
	static inline int32_t mix32(int32_t a, int32_t b, int num, int den, int32_t cut)
	{
		int64_t d = (int64_t)b - a;
		if (d > cut || d < -cut) return a;
		return (int32_t)(a + d * num / den);
	}

	static const int32_t CUT_POS = 0x2000;   // outPos / translation jump treated as a cut
	static const int32_t CUT_ANGLE = 0x7FFF; // outAngle (int16 wrap is the shortest way)
	static const int32_t CUT_MAT = 0x7FFF;

	static void MixMatrix(Mat4x3 &m, const Mat4x3 &n, int num, int den)
	{
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++) m.m[r][c] = mix16(m.m[r][c], n.m[r][c], num, den, CUT_MAT);
		m.pad = mix16(m.pad, n.pad, num, den, 0x7FFF);
		for (int k = 0; k < 3; k++) m.t[k] = mix32(m.t[k], n.t[k], num, den, 0x100000);
	}

	// writes the in-between state into the (saved) engine state
	static void ApplyInBetween(int num, int den)
	{
		uint32_t nb = BoneCount();
		if (nb > g_pred.nb) nb = g_pred.nb;
		uint8_t *bones = PTR(CTX(), 0x90);
		static uint8_t alive[128];
		memset(alive, 0, sizeof(alive));
		for (int k = 0; k < 256 && ORDER()[k] != 0xFF; k++) alive[ORDER()[k] & 0x7F] = 1;
		for (uint32_t i = 0; i < nb; i++)
		{
			uint8_t *b = bones + 0x100 * i;
			const PredBone &p = g_pred.b[i];
			if (!alive[i] || !p.alive || p.id16 != U16(b, 0x12) || p.draw != U8(b, 0x1C)) continue;
			for (int k = 0; k < 3; k++)
			{
				S16(b, 0x8C + 2 * k) = mix16(S16(b, 0x8C + 2 * k), p.ang[k], num, den, CUT_ANGLE);
				S16(b, 0x94 + 2 * k) = mix16(S16(b, 0x94 + 2 * k), p.pos[k], num, den, CUT_POS);
			}
			uint32_t c0 = U32(b, 0xCC), c1 = p.colour;
			if ((c0 & 0xFF000000) == (c1 & 0xFF000000))
			{
				uint32_t c = c0 & 0xFF000000;
				for (int s = 0; s < 24; s += 8)
				{
					int32_t a = (c0 >> s) & 0xFF, bb = (c1 >> s) & 0xFF;
					c |= (uint32_t)((a + (bb - a) * num / den) & 0xFF) << s;
				}
				U32(b, 0xCC) = c;
			}
		}
		// matrix nodes: same key on both ticks (slot 0 = camera matrix, always)
		for (int k = 0; k < 64; k++)
			if (k == 0 || (NODEKEYS()[k] != 0 && NODEKEYS()[k] == g_pred.keys[k]))
				MixMatrix(NODEMAT()[k], g_pred.node[k], num, den);
		// light sets (5 slots fit before the billboard matrices) and billboard matrices
		for (int s = 0; s < 5; s++)
		{
			uint8_t *l = LIGHTS() + 0x50 * s;
			const uint8_t *n = g_pred.lights + 0x50 * s;
			for (int o = 0; o < 0x40; o += 2) S16(l, o) = mix16(S16(l, o), AT<int16_t>(n, o), num, den, 0x7FFF);
			for (int o = 0x40; o < 0x4C; o += 4) S32(l, o) = mix32(S32(l, o), AT<int32_t>(n, o), num, den, 0x7FFFFFFF);
		}
		for (int k = 0; k < 4; k++) MixMatrix(BBMAT()[k], g_pred.bb[k], num, den);
	}

	bool HeldReady(Clone &c)
	{
		(void)c;
		return g_ported_tick == g_real_tick && g_scratch != nullptr;
	}

	// private packet memory of the held draw: main cursor ctx+0x7C, alternate pool ctx+0xD8 and
	// the battle frame arena cursor (for engine renderers that use it directly)
	static uint8_t g_held_packets[0x100000];

	void HeldFrame(Clone &c, int num, int den)
	{
		if (!HeldReady(c) || den <= 0) return;
		g_clone = &c;
		Predict(c);
		uint8_t *saved_globals[8] = { RT(), WS(), CTX(), SCENE(), RCTX(), SEQ(), CUR(), STREAM() };
		uint8_t *saved_ptr3 = PTR3();
		uint32_t pktcur = PKTCUR();
		// the mesh renderer leaves its depth scale here and a later mesh may read it: keep the
		// real tick's value for the next real tick
		float depth_scale = FLT_1877DA8();
		BindHeld();
		// the arena holds the state blocks of the clone-specific draw handlers (ribbons, starfield,
		// melt columns, ...): saved with the engine so that nothing a held draw writes persists
		SaveEngine(true);
		uint32_t seed = *CrtSeed();
		// particle pools of draw handler 6 (live in the scene data, written by type 4 even when frozen)
		static Region pools[32];
		int npools = 0;
		for (int k = 0; DRAWORDER()[k] != 0xFF && npools < 32; k++)
		{
			uint8_t *b = PTR(CTX(), 0x90) + 0x100 * (uint32_t)DRAWORDER()[k];
			if (U8(b, 0x1C) == 6 && PTR(b, 0xB8))
				region_save(pools[npools++], PTR(b, 0xB8), 16 + 80 * (uint32_t)U16(PTR(b, 0xB8), 0));
		}
		// skeletons of the embedded battle models (draw handler 3 and the clone's other model
		// handlers, Clone::model_draw): the held draw rebuilds their bone matrices at the
		// in-between pose
		static Region skels[8];
		int nskels = 0;
		for (int k = 0; DRAWORDER()[k] != 0xFF && nskels < 8; k++)
		{
			uint8_t *b = PTR(CTX(), 0x90) + 0x100 * (uint32_t)DRAWORDER()[k];
			uint8_t d = U8(b, 0x1C);
			bool model = d == 3;
			for (int m = 0; m < 4; m++) model |= c.model_draw[m] != 0 && c.model_draw[m] == d;
			uint8_t *blk = model ? PTR(b, 0xBC) : nullptr;
			uint8_t *sk = blk && PTR(blk, 0x14) ? PTR(PTR(blk, 0x14), 0) : nullptr;
			if (sk) region_save(skels[nskels++], sk, 0x10 + 0x30 * (uint32_t)U8(sk, 0));
		}
		SaveFreeScratch();
		if (g_pred.ok && !g_pred.paused) ApplyInBetween(num, den);

		// the tick's draw preamble, packets into a private buffer
		if (U16(CTX(), 0) & 0x400)
		{
			U32(RCTX(), 0x2C) = FRAMERL() + 0xC;
			U16(RCTX(), 0x24) = 0;
		}
		U32(RT(), 0x38) = FRAMERL() + 0x44;
		U32(CTX(), 0x7C) = (uint32_t)g_held_packets;
		U32(CTX(), 0xD8) = (uint32_t)g_held_packets + 0xA0000;
		PKTCUR() = (uint32_t)g_held_packets + 0xE0000;
		U16(RCTX(), 4) = 0;
		U16(RT(), 0x46) = 0;
		g_held = { true, num, den, U8(RT(), 0x45) };
		U8(RT(), 0x45) = 0xFF;
		BuildMatricesAndDraw();
		x::ParseCamera2(PROJH());
		g_held.active = false;

		for (int i = 0; i < npools; i++) region_restore(pools[i]);
		for (int i = nskels - 1; i >= 0; i--) region_restore(skels[i]);
		region_restore(g_r_free);
		RestoreEngine(true);
		*CrtSeed() = seed;
		FLT_1877DA8() = depth_scale;
		PKTCUR() = pktcur;
		RT() = saved_globals[0]; WS() = saved_globals[1]; CTX() = saved_globals[2]; SCENE() = saved_globals[3];
		RCTX() = saved_globals[4]; SEQ() = saved_globals[5]; CUR() = saved_globals[6]; STREAM() = saved_globals[7];
		PTR3() = saved_ptr3;
	}

	bool HeldCamera(Clone &c, int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (!HeldReady(c) || den <= 0) return false;
		g_clone = &c;
		Predict(c);
		if (!g_pred.ok || g_pred.paused || !g_pred.cam) return false;
		const int16_t *e0 = CAMEYE(), *a0 = CAMAT();
		bool cut = false;
		for (int k = 0; k < 3; k++)
		{
			int32_t de = (int32_t)g_pred.eye[k] - e0[k], da = (int32_t)g_pred.at[k] - a0[k];
			if (de > 1500 || de < -1500 || da > 1500 || da < -1500) cut = true;
		}
		for (int k = 0; k < 3; k++)
		{
			world[k] = cut ? e0[k] : (int16_t)lerp_i(e0[k], g_pred.eye[k], num, den);
			lookat[k] = cut ? a0[k] : (int16_t)lerp_i(a0[k], g_pred.at[k], num, den);
		}
		return true;
	}

	// ----------------------------------------------------------------------------------------
	// 0xB25DF0 GF_<x>_SequenceTick (Ifrit "GF_Ifrit_seqBDlink"): the only task of the effect
	// queue. Returns 2 (end) once SequenceState+0xA bit 15 is clear.
	// ----------------------------------------------------------------------------------------
	uint32_t SequenceTick(Clone &c)
	{
		g_clone = &c;
		g_ported_tick = g_real_tick;
		BindContext();
		S16(CTX(), 0x32) = (int16_t)(S16(CTX(), 0x32) + 1);
		U8(RT(), 0x41) = U8(CTX(), 0x32) & 1;
		x::Rand();
		if (U16(CTX(), 0) & 0x400)
		{
			U32(RCTX(), 0x2C) = FRAMERL() + 0xC;
			U16(RCTX(), 0x24) = 0;
		}
		U8(CTX(), 0x35) = (UPDFLAGS() & 1) ? 0xFF : 0;
		U8(RT(), 0x40) = U8(RT(), 0x41);
		U32(RT(), 0x38) = FRAMERL() + 0x44;
		{
			const uint32_t *v = (const uint32_t *)0x1D97778;
			uint32_t v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3], v4 = v[4] & 0xFFFF, v5 = v[5], v6 = v[6], v7 = v[7];
			uint32_t *m0 = (uint32_t *)NODEMAT(), *ma = (uint32_t *)CAMALT();
			m0[5] = v5; m0[0] = v0; ma[0] = v0; m0[1] = v1; m0[4] = v4; m0[6] = v6; ma[1] = v1; ma[4] = v4; ma[5] = v5;
			m0[2] = v2; m0[3] = v3; m0[7] = v7; ma[2] = v2; ma[3] = v3; ma[6] = v6; ma[7] = v7;
			U32(CTX(), 0x7C) = PKTCUR();
		}
		U32(CTX(), 0xD8) = U32(CTX(), 0xD4);
		U32(CTX(), 0x88) = add32(mul32(S8(RT(), 0x40), S32(CTX(), 0x8C)), S32(CTX(), 0x84));
		if (U8(CTX(), 1) & 0x80) x::ClearOT(U32(RT(), 0x38), 0x1000);
		if (U8(CTX(), 1) & 0x80) part_core::DebugPadRootControl();
		U8(CTX(), 0xD2) = 0;
		if (U8(CTX(), 0x35) == 0)
		{
			AnimChannelsNeg();
			AnimIntegrator();
			AnimChannelsPos();
		}
		U16(RCTX(), 4) = 0;
		U16(RT(), 0x46) = 0;
		BuildMatricesAndDraw();
		x::ParseCamera2(PROJH());
		PKTCUR() = U32(CTX(), 0x7C);
		memcpy(g_scratch_copy, WS(), sizeof(g_scratch_copy));
		g_scratch = g_scratch_copy;
		ReleaseContext();
		return (~(uint32_t)U16(SEQ(), 0xA) >> 14) & 2;
	}

	// ============================================================================================
	// port tables
	// ============================================================================================
	static void __cdecl noop() {}

	void fill_core(Generic &g)
	{
		g.bone[0] = part_core::bh_00_AccumPos;
		g.bone[1] = part_core::bh_00_AccumPos;
		g.bone[3] = part_core::bh_03_Polar;
		g.bone[5] = part_core::bh_05_CopyBone;
		g.bone[6] = part_core::bh_06_LerpBones;
		g.bone[7] = part_core::bh_07_BonePlusAccum;
		g.bone[9] = part_core::bh_09_EntityJoint;
	}

	void fill_generic()
	{
		static bool done = false;
		if (done) return;
		done = true;
		memset(&g_generic, 0, sizeof(g_generic));
		fill_core(g_generic);
		fill_vm_a(g_generic);
		fill_vm_b(g_generic);
		fill_vm_c(g_generic);
		fill_vm_d(g_generic);
		fill_draw_mesh(g_generic);
		fill_draw_prim(g_generic);
		fill_draw_sprite(g_generic);
	}

	// port of one original table entry: stub (C3) -> no-op, clone-specific (exception) or not
	// ported -> original address, else the generic port; non-code values stay raw (vanilla
	// would call them)
	static VoidFn port_of(uint32_t orig, VoidFn gen, int tid, int index, const Exception *ex, int nex)
	{
		if (orig == 0) return nullptr;
		if (orig < 0x401000 || orig >= 0xC00000) return (VoidFn)orig;
		if (*(const uint8_t *)orig == 0xC3) return noop;
		for (int k = 0; k < nex; k++)
			if (ex[k].table == tid && ex[k].index == index) return (VoidFn)orig;
		return gen ? gen : (VoidFn)orig;
	}

	static Clone *g_clones[16];
	static int g_nclones = 0;

	Clone *find_clone(int effect_id)
	{
		for (int i = 0; i < g_nclones; i++)
			if (g_clones[i]->effect_id == effect_id) return g_clones[i];
		return nullptr;
	}

	void init_clone(Clone &c, const Exception *ex, int nex)
	{
		fill_generic();
		if (!find_clone(c.effect_id) && g_nclones < 16) g_clones[g_nclones++] = &c;
		// opcodes past the table's 0x147 entries read whatever follows it, as vanilla does
		for (int i = 0; i < 0x200; i++)
			c.vm[i] = port_of(*(const uint32_t *)(c.vm_table + 4 * i), i < 0x147 ? g_generic.vm[i] : nullptr, 0, i, ex, nex);
		// the handler block bone_table .. : bone handlers, prim renderers, (prim sizes), sprite
		// orientations, particle types, particle opcodes, then the draw table
		struct Sub { uint32_t base; int count; int tid; VoidFn const *gen; } subs[] = {
			{ c.bone_table, c.bone_count, 1, g_generic.bone },
			{ c.prim_table, 60, 2, g_generic.prim },
			{ c.spr_table, 11, 3, g_generic.spr },
			{ c.ptype_table, 5, 4, g_generic.ptype },
			{ c.pop_table, 24, 5, g_generic.pop },
			{ c.draw_table, 73, 6, g_generic.draw },
		};
		for (int j = 0; j < 256; j++)
		{
			uint32_t a = c.bone_table + 4 * j;
			VoidFn gen = nullptr;
			int tid = -1, index = 0;
			for (const Sub &sb : subs)
				if (a >= sb.base && a < sb.base + 4 * (uint32_t)sb.count)
				{
					index = (int)(a - sb.base) / 4;
					tid = sb.tid;
					gen = (tid == 1 && index >= 32) ? nullptr : sb.gen[index];
				}
			c.bigtab[j] = port_of(*(const uint32_t *)a, gen, tid, index, ex, nex);
		}
		c.bone = c.bigtab;
		c.prim = c.bigtab + (c.prim_table - c.bone_table) / 4;
		c.spr = c.bigtab + (c.spr_table - c.bone_table) / 4;
		c.ptype = c.bigtab + (c.ptype_table - c.bone_table) / 4;
		c.pop = c.bigtab + (c.pop_table - c.bone_table) / 4;
		for (int i = 0; i < 256; i++)
			c.draw[i] = port_of(*(const uint32_t *)(c.draw_table + 4 * i), i < 73 ? g_generic.draw[i] : nullptr, 6, i, ex, nex);
	}
}
}

// ============================================================================================
// part vm_a
// ============================================================================================
// part vm_a: flow control, sound/stream/music, file loads, embedded model, particle pool,
// battle camera / battle entity opcodes of the GF cinematic VM (Ifrit 201 reference addresses).


namespace ff8fx
{
namespace gfc
{
namespace part_vm_a
{
	// ------------------------------------------------------------------------------------
	// globals used by this part
	// ------------------------------------------------------------------------------------
	// engine block (inside the snapshot)
	inline uint8_t &LoadBusy() { return MEM<uint8_t>(0x2798219); }   // file load in flight (0xFF), cleared by the load callback
	inline uint8_t &LoadDone() { return MEM<uint8_t>(0x2798218); }   // a8def.tim loader idle / load-completed flag
	// battle globals (OUTSIDE the snapshot: every write is guard()ed)
	inline uint8_t &Byte1D96DC4() { return MEM<uint8_t>(0x1D96DC4); }
	inline uint32_t &Ptr1D99A88() { return MEM<uint32_t>(0x1D99A88); } // MAGIC_TEXTURE_BUFFER_PTR (read only here)
	inline uint32_t &CamWord32(int i) { return MEM<uint32_t>(0xB8B7F0 + 4 * i); } // battle camera block 0xB8B7F0..0xB8B80F as dwords
	inline uint16_t &Word1D977A0() { return MEM<uint16_t>(0x1D977A0); }
	inline uint16_t &Word1D977A2() { return MEM<uint16_t>(0x1D977A2); }
	inline uint16_t &Word1D9771C() { return MEM<uint16_t>(0x1D9771C); }
	inline uint16_t *CamShake() { return (uint16_t *)0x1D97710; }      // BATTLE_CAMERA_SHAKE_OFFSET_X/Y/Z
	inline uint8_t *Entity(int32_t i) { return (uint8_t *)(0x1D972C0 + i * 0x9C); } // battle entity records (0x9C bytes)
	inline uint8_t *SlotData(uint32_t slot) { return (uint8_t *)(0x1D27B1C + slot * 0xD0); } // BATTLE_SLOT_DATA[slot] status dword

	// Module-relative: load-completion callback handed to pre_LoadBattleFile by op 0x006 = C().load_cb.
	// The loader stores it and calls it later from outside the VM, so the ORIGINAL address is passed
	// (every clone has its own copy).

	// guard only when the address lies outside the engine state block
	static inline void guard_ext(const void *p, int n)
	{
		uint32_t a = (uint32_t)p;
		if (a < STATE_LO || a + (uint32_t)n > STATE_HI) guard(p, n);
	}

	// ------------------------------------------------------------------------------------
	// helpers
	// ------------------------------------------------------------------------------------

	// 0xB2BB40 (load callback of op 0x006): LoadBusy = 0. Never called by the port itself
	// (the loader calls the original address), kept for completeness.
	[[maybe_unused]] static void __cdecl cb_B2BB40_LoadDone()
	{
		LoadBusy() = 0;
	}

	// 0xB25DB0: stream source for op 0x031. slot -> ctx+0xB0 byte table: 0xFF = ctx+0xB8 buffer,
	// else resource file f = RESFILE[b]: f + U32(f,0x18) = offset table, entry [slot].
	static uint8_t *StreamSource_B25DB0(int32_t slot)
	{
		uint8_t *ctx = CTX();
		uint32_t b = U8(PTR(ctx, 0xB0), slot);
		if (b == 0xFF) return PTR(ctx, 0xB8);
		uint8_t *f = RESFILE()[b];
		uint8_t *tab = f + U32(f, 0x18);
		return tab + U32(tab, slot * 4);
	}

	// 0xB269B0: parent xform (scaled), then one StepEmbeddedModelAnim with block bit1 cleared
	// (bit1 = "skip anim step" flag), bit1 set again afterwards. Returns 0.
	static int32_t StepModelFromVm_B269B0(uint8_t *blk)
	{
		h_B269E0();
		U16(blk, 2) &= 0xFFFD;
		h_B26AD0(blk);
		U8(blk, 2) |= 2;
		return 0;
	}

	// 0xB2B700: restore battle entities [start, start+count): clear bit 2 (hidden) of the
	// entity word +0 unless SceneHeader+0x4E+2*i has bit 2 (entity hidden before the summon).
	static void EntityRestore_B2B700(int32_t start, int32_t count)
	{
		uint8_t *keep = SCENE() + start * 2 + 0x4E;
		uint8_t *ent = Entity(start);
		if (count <= 0) return;
		do
		{
			if (!(U8(keep, 0) & 4))
			{
				guard(ent, 2);
				U16(ent, 0) &= 0xFFFB;
			}
			keep += 2;
			ent += 0x9C;
		} while (--count);
	}

	// OR 4 (hide) into the entity word +0 of [start, start+count) (the inlined loops of 0xB2B5D0)
	static void EntityHide(int32_t start, int32_t count)
	{
		uint8_t *ent = Entity(start);
		if (count <= 0) return;
		do
		{
			guard(ent, 2);
			U16(ent, 0) |= 4;
			ent += 0x9C;
		} while (--count);
	}

	// ------------------------------------------------------------------------------------
	// flow control
	// ------------------------------------------------------------------------------------

	// 0xB2AE30 (VM 0x002 Jump): cursor += s16 w0
	static void __cdecl op_002_Jump()
	{
		uint8_t *s = STREAM();
		STREAM() = s + S16(s, 2);
	}

	// 0xB2AE50 (VM 0x003 Call): per-channel return stack bone+0x24 [chan*2 + depth] = opcode+4,
	// depth byte bone+0x44+chan ++, cursor += s16 w0. chan = rt+0x43.
	static void __cdecl op_003_Call()
	{
		uint32_t ch = U8(RT(), 0x43);
		uint8_t *sp = CUR() + ch + 0x44;
		uint8_t d8 = *sp;
		int32_t depth = (int8_t)d8;
		*sp = (uint8_t)(d8 + 1);
		uint8_t *s = STREAM();
		uint32_t ch2 = U8(RT(), 0x43);
		uint8_t *bone = CUR();
		int32_t off = S16(s, 2);
		int32_t idx = depth + (int32_t)ch2 * 2;
		PTR(bone, idx * 4 + 0x24) = s + 4;
		STREAM() = STREAM() + off;
	}

	// 0xB2AEB0 (VM 0x004 Return): --depth byte bone+0x44+chan; cursor = bone+0x24 [chan*2 + depth]
	static void __cdecl op_004_Return()
	{
		uint32_t ch = U8(RT(), 0x43);
		uint8_t *sp = CUR() + ch + 0x44;
		*sp = (uint8_t)(*sp - 1);
		ch = U8(RT(), 0x43);
		uint8_t *bone = CUR();
		int32_t depth = S8(bone, ch + 0x44);
		STREAM() = PTR(bone, (depth + (int32_t)ch * 2) * 4 + 0x24);
	}

	// 0xB2AF00 (VM 0x01E SetLoopCounter): bone byte +0x20 + (op>>14) = low byte of w0
	static void __cdecl op_01E_SetLoopCounter()
	{
		uint8_t v = U8(STREAM(), 2);
		uint32_t k = (uint32_t)U16(RT(), 0x4A) >> 14;
		U8(CUR(), k + 0x20) = v;
		STREAM() += 4;
	}

	// 0xB2AF80 (VM 0x01F LoopDecJump): --bone byte +0x20+(op>>14); nonzero -> cursor += w0, else +4
	static void __cdecl op_01F_LoopDecJump()
	{
		uint32_t k = (uint32_t)U16(RT(), 0x4A) >> 14;
		uint8_t *b = CUR();
		U8(b, k + 0x20) = (uint8_t)(U8(b, k + 0x20) - 1);
		uint8_t c = U8(CUR(), k + 0x20);
		uint8_t *s = STREAM();
		if (c == 0)
			STREAM() = s + 4;
		else
			STREAM() = s + S16(s, 2);
	}

	// 0xB2AF50 (VM 0x0A7 SetCounter): SceneHeader byte [0x14 + w0] = low byte of w1
	static void __cdecl op_0A7_SetCounter()
	{
		uint8_t *s = STREAM();
		int32_t i = S16(s, 2);
		uint8_t v = U8(s, 4);
		U8(SCENE(), i + 0x14) = v;
		STREAM() += 6;
	}

	// 0xB2AFD0 (VM 0x0A8 DecCounterJumpNZ): --SceneHeader byte [0x14 + w0]; nonzero -> cursor += w1, else +6
	static void __cdecl op_0A8_DecCounterJumpNZ()
	{
		int32_t i = S16(STREAM(), 2);
		uint8_t *sc = SCENE();
		U8(sc, i + 0x14) = (uint8_t)(U8(sc, i + 0x14) - 1);
		uint8_t c = U8(SCENE(), i + 0x14);
		uint8_t *s = STREAM();
		if (c == 0)
			STREAM() = s + 6;
		else
			STREAM() = s + S16(s, 4);
	}

	// 0xB2B080 (VM 0x0A1 RandomJump): r = |Rand73(0x100)|; r <= s16 w0 -> cursor += w1, else +6
	static void __cdecl op_0A1_RandomJump()
	{
		int32_t r = blob::Rand73(0x100);
		if (r < 0) r = (int32_t)(0u - (uint32_t)r); // not; inc
		uint8_t *s = STREAM();
		if (r <= (int32_t)S16(s, 2))
			STREAM() = s + S16(s, 4);
		else
			STREAM() = s + 6;
	}

	// 0xB2B290 (VM 0x046 JumpIfTargetResult): target record = entry of the action target list
	// (ctx+0xCC: +8 records of 0x18 bytes, +0x10 u8 count) whose byte 0 == bone+0x1B slot (one past
	// the list when absent, read anyway - vanilla). op>>9: 1 rec[3]&4, 2 rec[2]&8, 3 rec[3]&2,
	// 4 BATTLE_SLOT_DATA[slot] dword & 0x2000, other !(rec[3]&4). Condition -> cursor += w0, else +4.
	static void __cdecl op_046_JumpIfTargetResult()
	{
		uint8_t *act = PTR(CTX(), 0xCC);
		uint32_t slot = U8(CUR(), 0x1B);
		uint8_t *rec = PTR(act, 8);
		int32_t n = U8(act, 0x10);
		while (n > 0)
		{
			if (U8(rec, 0) == slot) break;
			rec += 0x18;
			n--;
		}
		uint32_t sel = U16(RT(), 0x4A);
		uint8_t b3 = U8(rec, 3);
		bool jump;
		switch ((sel >> 9) - 1) // jump table 0xB2B370
		{
		case 0: jump = (b3 & 4) != 0; break;                              // 0xB2B335
		case 1: jump = (U8(rec, 2) & 8) != 0; break;                      // 0xB2B320
		case 2: jump = (b3 & 2) != 0; break;                              // 0xB2B30D
		case 3: jump = (U32(SlotData(slot), 0) & 0x2000) != 0; break;      // 0xB2B2EA
		default: jump = (b3 & 4) == 0; break;                             // 0xB2B348
		}
		uint8_t *s = STREAM();
		if (jump)
			STREAM() = s + S16(s, 2);
		else
			STREAM() = s + 4;
	}

	// 0xB2B380 (VM 0x055 JumpIfSummonState): rec = *(ctx+0xC0). op>>9: 1 ctx+0xD0 != ctx+0xD1,
	// 2 (rec[1]&3) && !(rec[1]&2), 3 (rec[1]&3) && (rec[1]&2), other ctx+0xD0 >= 1.
	// Condition -> cursor += w0, else +4.
	static void __cdecl op_055_JumpIfSummonState()
	{
		uint8_t *ctx = CTX();
		uint8_t *rec = PTR(ctx, 0xC0);
		uint32_t sel = (uint32_t)U16(RT(), 0x4A) >> 9;
		bool jump;
		if (sel == 1)
			jump = U8(ctx, 0xD0) != U8(ctx, 0xD1);
		else if (sel == 2)
		{
			uint8_t c = U8(rec, 1);
			jump = (c & 3) && !(c & 2);
		}
		else if (sel == 3)
		{
			uint8_t c = U8(rec, 1);
			jump = (c & 3) && (c & 2);
		}
		else
			jump = U8(ctx, 0xD0) >= 1;
		uint8_t *s = STREAM();
		if (jump)
			STREAM() = s + S16(s, 2);
		else
			STREAM() = s + 4;
	}

	// 0xB2AE20 (VM 0x057 Nop2): cursor += 2
	static void __cdecl op_057_Nop2() { STREAM() += 2; }
	// 0xB2AF30 (VM 0x0A5 Nop4): cursor += 4
	static void __cdecl op_0A5_Nop4() { STREAM() += 4; }
	// 0xB2AF40 (VM 0x0A6 Nop4): cursor += 4
	static void __cdecl op_0A6_Nop4() { STREAM() += 4; }
	// 0xB26BF0 (VM 0x0B8 Nop8): cursor += 8
	static void __cdecl op_0B8_Nop8() { STREAM() += 8; }

	// ------------------------------------------------------------------------------------
	// sound / stream / music (external, skipped while predicting by the x:: wrappers)
	// ------------------------------------------------------------------------------------

	// 0xB25BB0 (VM 0x02B PlaySE): BdPlaySE(ctx+0xC4 table [op>>9], s16 w0, 0x80)
	static void __cdecl op_02B_PlaySE()
	{
		uint8_t *s = STREAM();
		uint8_t *tab = PTR(CTX(), 0xC4);
		int32_t attr = S16(s, 2);
		uint32_t idx = (uint32_t)U16(RT(), 0x4A) >> 9;
		x::PlaySE(tab + U32(tab, idx * 4), attr, 0x80);
		STREAM() += 4;
	}

	// 0xB25C00 (VM 0x031 TransSummonStream): load busy -> wait (rt+0x3E = bone+0xC8, retry);
	// else BdTransSummonStream(B25DB0((op>>9)&15), ctx+0xA3 ready flag); bit15 -> byte 0x1D96DC4 = 0
	static void __cdecl op_031_TransSummonStream()
	{
		if (LoadBusy() != 0)
		{
			U16(RT(), 0x3E) = U16(CUR(), 0xC8);
			return;
		}
		uint8_t *src = StreamSource_B25DB0(((uint32_t)U16(RT(), 0x4A) >> 9) & 0xF);
		uint8_t *ready = CTX() + 0xA3;
		if (U8(RT(), 0x4B) & 0x80)
		{
			guard(&Byte1D96DC4(), 1);
			Byte1D96DC4() = 0;
		}
		x::TransSummonStream(src, ready);
		STREAM() += 2;
	}

	// 0xB25C70 (VM 0x034 WaitStreamReady): ctx+0xA3 == 0 -> wait (rt+0x3E = bone+0xC8), else +2
	static void __cdecl op_034_WaitStreamReady()
	{
		if (U8(CTX(), 0xA3) == 0)
		{
			U16(RT(), 0x3E) = U16(CUR(), 0xC8);
			return;
		}
		STREAM() += 2;
	}

	// 0xB25CA0 (VM 0x030 PlaySummonStream): BdPlaySummonStream(0x80, 1, op>>9)
	static void __cdecl op_030_PlaySummonStream()
	{
		uint32_t a = (uint32_t)U16(RT(), 0x4A) >> 9;
		x::PlaySummonStream(0x80, 1, (int32_t)a);
		STREAM() += 2;
	}

	// 0xB25CD0 (VM 0x02C StopSoundChannels): sub_46B450(0, (op>>9)&15)
	static void __cdecl op_02C_StopSoundChannels()
	{
		uint32_t m = ((uint32_t)U16(RT(), 0x4A) >> 9) & 0xF;
		x::Snd_46B450(0, (int32_t)m);
		STREAM() += 2;
	}

	// 0xB25D10 (VM 0x053 MusicVolume): bit15 -> Music_SetVolumeTrans(0, s16 w0, 0), else sub_47E3C0(s16 w0)
	static void __cdecl op_053_MusicVolume()
	{
		uint8_t *rt = RT();
		int32_t v = S16(STREAM(), 2);
		if (U8(rt, 0x4B) & 0x80)
			x::MusicSetVolumeTrans(0, v, 0);
		else
			x::Music_47E3C0(v);
		STREAM() += 4;
	}

	// 0xB25D60 (VM 0x0C3 PauseResumeSounds): bit15 -> sub_46B3E0(2), (3); else sub_46B3A0(2), (3)
	static void __cdecl op_0C3_PauseResumeSounds()
	{
		if (U8(RT(), 0x4B) & 0x80)
		{
			x::Snd_46B3E0(2);
			x::Snd_46B3E0(3);
		}
		else
		{
			x::Snd_46B3A0(2);
			x::Snd_46B3A0(3);
		}
		STREAM() += 2;
	}

	// ------------------------------------------------------------------------------------
	// file loads
	// ------------------------------------------------------------------------------------

	// 0xB2BA10 (VM 0x006 LoadFile): load busy -> wait (rt+0x3E = bone+0xC8, retry). Else busy = 0xFF,
	// destination/slot from the op word and w0 (bit15 clear) or ctx+0xB8 (bit15 set),
	// RESFILE[slot + ctx+0xA2] = dst (skipped for bit15 + slot bit5: file 0x16B + (slot&31)),
	// ctx+0xB4 = dst, pre_LoadBattleFile((slot + ctx+0xA2 + s16 ctx+0xA0) & 0xFFFF, dst, 0, 0xB2BB40).
	// cursor += 4, or += 2 when the s16 op word <= 0.
	static void __cdecl op_006_LoadFile()
	{
		if (LoadBusy() != 0)
		{
			U16(RT(), 0x3E) = U16(CUR(), 0xC8);
			return;
		}
		uint8_t *rt = RT();
		LoadBusy() = 0xFF;
		uint8_t *ctx = CTX();
		uint32_t w = U16(rt, 0x4A);
		uint32_t dst;
		uint32_t id;
		bool store = true;
		if (w & 0x8000) // 0xB2BAFA
		{
			dst = U32(ctx, 0xB8);
			id = (w >> 9) & 0x3F;
			if (id & 0x20)
			{
				id = (id & 0x1F) + 0x16B;
				store = false; // jumps to 0xB2BA85
			}
		}
		else
		{
			uint32_t src = U16(STREAM(), 2);
			uint32_t lo = src & 0xFF;
			uint32_t hi = src >> 8;
			id = w >> 9;
			if (lo == 0xFF)
				dst = U32(ctx, 0xB8);
			else if (lo & 0x80) // 0xB2BABD
				dst = (uint32_t)RESFILE()[lo & 0x7F] + (hi << 12);
			else // 0xB2BAD3
			{
				dst = (uint32_t)RESFILE()[lo];
				if ((hi & 0xFF) != 0xFF)
				{
					uint32_t d = C().desc;                 // Ifrit 0x1874894
					uint32_t t = d + U32((void *)d, 0x1C);  // dword_18748B0 + 0x1874894
					t += U32((void *)t, lo * 4);
					dst += U32((void *)t, hi * 4);
				}
			}
		}
		if (store) // 0xB2BA6B
		{
			id += U8(ctx, 0xA2);
			guard_ext(&RESFILE()[id], 4);
			RESFILE()[id] = (uint8_t *)dst;
			id += (uint32_t)(int32_t)S16(ctx, 0xA0);
		}
		// 0xB2BA85
		U32(ctx, 0xB4) = dst;
		x::PreLoadBattleFile((int32_t)(id & 0xFFFF), (void *)dst, 0, C().load_cb);
		if (S16(RT(), 0x4A) <= 0)
			STREAM() += 2;
		else
			STREAM() += 4;
	}

	// 0xB2BBD0 (VM 0x008 WaitLoadDone): !busy && done -> +2, else wait (rt+0x3E = bone+0xC8)
	static void __cdecl op_008_WaitLoadDone()
	{
		if (LoadBusy() == 0 && LoadDone() != 0)
		{
			STREAM() += 2;
			return;
		}
		U16(RT(), 0x3E) = U16(CUR(), 0xC8);
	}

	// 0xB2BB80 (VM 0x07B WaitThenReloadA8defTim): done flag set -> sub_508630(ctx+0xB8, &flag), +2;
	// else wait (rt+0x3E = bone+0xC8)
	static void __cdecl op_07B_WaitThenReloadA8defTim()
	{
		if (LoadDone() != 0)
		{
			x::Battle_508630(PTR(CTX(), 0xB8), &LoadDone());
			STREAM() += 2;
			return;
		}
		U16(RT(), 0x3E) = U16(CUR(), 0xC8);
	}

	// 0xB2BB50 (VM 0x0B2 SetCtxByteA2): ctx+0xA2 = low byte of w0 (RESFILE slot base of op 0x006)
	static void __cdecl op_0B2_SetCtxByteA2()
	{
		U8(CTX(), 0xA2) = U8(STREAM(), 2);
		STREAM() += 4;
	}

	// 0xB2B450 (VM 0x021 SetDataPointer): off = u32 (w0 | w1 << 16); base = bit15 ? .01 file +
	// U32(.01, 0x1C) : MAGIC_TEXTURE_BUFFER_PTR (0x1D99A88). (op>>12)&7: 1 ctx+0xB8 = base+off;
	// 2 ctx+0xD4 = base+off, ctx+0xDC = (s16 w2 << 8) / 2, ctx+0xD8 = ctx+0xD4 + s8 rt+0x41 * ctx+0xDC;
	// other: ctx+0x74 = ctx+0x70 = base+off (arena). cursor += 8.
	static void __cdecl op_021_SetDataPointer()
	{
		uint8_t *s = STREAM();
		uint32_t off = ((uint32_t)U16(s, 4) << 16) | U16(s, 2);
		uint32_t w = U16(RT(), 0x4A);
		uint32_t base;
		if (w & 0x8000)
		{
			uint8_t *f = RESFILE()[1];
			base = (uint32_t)f + U32(f, 0x1C);
		}
		else
			base = Ptr1D99A88();
		switch (((int32_t)w >> 12) & 7)
		{
		case 1: // 0xB2B513
			U32(CTX(), 0xB8) = base + off;
			break;
		case 2: // 0xB2B4BC
		{
			U32(CTX(), 0xD4) = base + off;
			int32_t t = shl32(S16(STREAM(), 6), 8);
			int32_t half = t / 2; // cdq; sub; sar 1
			S32(CTX(), 0xDC) = half;
			int32_t ping = S8(RT(), 0x41);
			S32(CTX(), 0xD8) = add32(mul32(ping, half), S32(CTX(), 0xD4));
			break;
		}
		default:
			U32(CTX(), 0x74) = base + off;
			U32(CTX(), 0x70) = U32(CTX(), 0x74);
			break;
		}
		STREAM() += 8;
	}

	// 0xB2B420 (VM 0x120 SetArenaTop): ctx+0x74 = ctx+0x70 + (s16 w0 << 16)
	static void __cdecl op_120_SetArenaTop()
	{
		int32_t v = S16(STREAM(), 2);
		uint8_t *ctx = CTX();
		S32(ctx, 0x74) = add32(shl32(v, 16), S32(ctx, 0x70));
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// bone setup
	// ------------------------------------------------------------------------------------

	// 0xB2B020 (VM 0x033 SetBoneId): bone+0x12 = w0
	static void __cdecl op_033_SetBoneId()
	{
		U16(CUR(), 0x12) = U16(STREAM(), 2);
		STREAM() += 4;
	}

	// 0xB2B050 (VM 0x0CE SetBoneIdFromSlot): bone+0x12 = bone+0x1B + w0 (16-bit)
	static void __cdecl op_0CE_SetBoneIdFromSlot()
	{
		uint8_t *b = CUR();
		U16(b, 0x12) = (uint16_t)(U8(b, 0x1B) + U16(STREAM(), 2));
		STREAM() += 4;
	}

	// 0xB2B0E0 (VM 0x064 SetBoneSlot): bone+0x1B = bit15 ? *(ctx+0xC0)[0] : bit12 ? low byte w0 (+4)
	// : SceneHeader+0x48; cursor += 2 (bit12 path += 4)
	static void __cdecl op_064_SetBoneSlot()
	{
		uint32_t w = U16(RT(), 0x4A);
		uint8_t v;
		if (w & 0x8000)
			v = U8(PTR(CTX(), 0xC0), 0);
		else if (w & 0x1000)
		{
			U8(CUR(), 0x1B) = U8(STREAM(), 2);
			STREAM() += 4;
			return;
		}
		else
			v = U8(SCENE(), 0x48);
		U8(CUR(), 0x1B) = v;
		STREAM() += 2;
	}

	// 0xB2B150 (VM 0x0B1 SetSeqStateByte6): SequenceState+6 = low byte of w0
	static void __cdecl op_0B1_SetSeqStateByte6()
	{
		U8(SEQ(), 6) = U8(STREAM(), 2);
		STREAM() += 4;
	}

	// 0xB2B170 (VM 0x0CD SetRenderListOffset): bone+0x48 = w0
	static void __cdecl op_0CD_SetRenderListOffset()
	{
		U16(CUR(), 0x48) = U16(STREAM(), 2);
		STREAM() += 4;
	}

	// 0xB2B1B0 (VM 0x044 SetDrawHandlerInlineDataReset): SetDrawHandler(w0), bone+0xB8 = &inline
	// block (w1 words at +6), bone+0xBC = +0xC0 = +0xC4 = 0, cursor skips the block
	static void __cdecl op_044_SetDrawHandlerInlineDataReset()
	{
		blob::SetDrawHandler(S16(STREAM(), 2));
		uint8_t *s = STREAM();
		int32_t n = S16(s, 4);
		PTR(CUR(), 0xB8) = s + 6;
		U32(CUR(), 0xBC) = 0;
		U32(CUR(), 0xC0) = 0;
		U32(CUR(), 0xC4) = 0;
		STREAM() = STREAM() + shl32(n, 1) + 6;
	}

	// 0xB2B220 (VM 0x043 SetDrawHandlerInlineData): SetDrawHandler(w0), bone+0xB8 = &inline block,
	// bone+0xBC = 0, cursor skips the block
	static void __cdecl op_043_SetDrawHandlerInlineData()
	{
		blob::SetDrawHandler(S16(STREAM(), 2));
		uint8_t *s = STREAM();
		int32_t n = S16(s, 4);
		PTR(CUR(), 0xB8) = s + 6;
		U32(CUR(), 0xBC) = 0;
		STREAM() = STREAM() + shl32(n, 1) + 6;
	}

	// 0xB2B950 (VM 0x04E SetDescriptorInlineData): bone+0xB8 [w0] = &inline block (w1 words), skip it
	static void __cdecl op_04E_SetDescriptorInlineData()
	{
		uint8_t *s = STREAM();
		int32_t k = S16(s, 2);
		PTR(CUR(), k * 4 + 0xB8) = s + 6;
		s = STREAM();
		STREAM() = s + S16(s, 4) * 2 + 6;
	}

	// 0xB2B980 (VM 0x03B SequenceFlags): w0 bit15 ? SequenceState+8 &= ~(w0 & 0x7FFF) : |= w0 & 0x7FFF
	static void __cdecl op_03B_SequenceFlags()
	{
		uint8_t *seq = SEQ();
		int32_t v = S16(STREAM(), 2);
		if (v & 0x8000)
			U16(seq, 8) &= (uint16_t)~(v & 0x7FFF);
		else
			U16(seq, 8) |= (uint16_t)(v & 0x7FFF);
		STREAM() += 4;
	}

	// 0xB2BC10 (VM 0x007 SetFreezeOthers): bit15 -> rt+0x45 boneSkipFlag = 0xFF, bit14 -> = 0;
	// order list entry of the current bone (rt+0x44) |= 0x80 (keeps running while frozen)
	static void __cdecl op_007_SetFreezeOthers()
	{
		uint8_t *rt = RT();
		uint32_t w = U16(rt, 0x4A);
		if (w & 0x8000)
		{
			U8(rt, 0x45) = 0xFF;
			rt = RT();
		}
		if (w & 0x4000)
		{
			U8(rt, 0x45) = 0;
			rt = RT();
		}
		uint32_t i = U8(rt, 0x44);
		ORDER()[i] |= 0x80;
		STREAM() += 2;
	}

	// 0xB26DF0 (VM 0x04D SetupScreenTint): bone handler +0x18 = 0, +0x48 = 0, SetDrawHandler(4)
	static void __cdecl op_04D_SetupScreenTint()
	{
		U8(CUR(), 0x18) = 0;
		U16(CUR(), 0x48) = 0;
		blob::SetDrawHandler(4);
		STREAM() += 2;
	}

	// ------------------------------------------------------------------------------------
	// embedded battle model (draw handler 3)
	// ------------------------------------------------------------------------------------

	// 0xB26820 (VM 0x03F CreateEmbeddedModel): bone+0xBC = B26860(s16 w0), SetDrawHandler(3)
	static void __cdecl op_03F_CreateEmbeddedModel()
	{
		uint8_t *blk = h_B26860(S16(STREAM(), 2));
		PTR(CUR(), 0xBC) = blk;
		blob::SetDrawHandler(3);
		STREAM() += 4;
	}

	// 0xB26900 (VM 0x041 EmbeddedModelFlags): block = bone+0xBC, sub = op>>12: 0 block+2 &= ~w0 (+4);
	// 1 block byte+2 |= 4, block+0x7C = &w0 (+6); 8 block+2 |= w0 (+4); other: nothing (no advance)
	static void __cdecl op_041_EmbeddedModelFlags()
	{
		uint8_t *s = STREAM();
		uint8_t *blk = PTR(CUR(), 0xBC);
		uint8_t *rt = RT();
		uint8_t *arg = s + 2;
		uint32_t sub = (uint32_t)U16(rt, 0x4A) >> 12;
		int32_t v = S16(arg, 0);
		switch (sub)
		{
		case 0: // 0xB26960
			U16(blk, 2) &= (uint16_t)~v;
			STREAM() += 4;
			break;
		case 1: // 0xB26949
			U8(blk, 2) |= 4;
			PTR(blk, 0x7C) = arg;
			STREAM() += 6;
			break;
		case 8: // 0xB26935
			U16(blk, 2) |= (uint16_t)v;
			STREAM() += 4;
			break;
		default:
			break; // vanilla: cursor not advanced
		}
	}

	// 0xB26980 (VM 0x089 StepEmbeddedModelAnim): B269B0(bone+0xBC)
	static void __cdecl op_089_StepEmbeddedModelAnim()
	{
		StepModelFromVm_B269B0(PTR(CUR(), 0xBC));
		STREAM() += 2;
	}

	// ------------------------------------------------------------------------------------
	// particle pool (draw handler 6)
	// ------------------------------------------------------------------------------------

	// 0xB29D70 (VM 0x058 CreateParticlePool): pool = arena(0x50*n + 0x10), bone+0xB8 = pool,
	// header {u16 n, 0, 0, 0}, per particle +0 u16, +8, +0x30, +4 = 0; SetDrawHandler(6)
	static void __cdecl op_058_CreateParticlePool()
	{
		int32_t n = S16(STREAM(), 2);
		uint8_t *pool = blob::ArenaAlloc(n * 0x50 + 0x10);
		PTR(CUR(), 0xB8) = pool;
		uint8_t *p = pool + 0x10;
		U16(pool, 0) = (uint16_t)n;
		U16(pool, 2) = 0;
		U16(pool, 4) = 0;
		U16(pool, 6) = 0;
		for (int32_t i = n; i > 0; i--)
		{
			U16(p, 0) = 0;
			U32(p, 8) = 0;
			U32(p, 0x30) = 0;
			U32(p, 4) = 0;
			p += 0x50;
		}
		blob::SetDrawHandler(6);
		STREAM() += 4;
	}

	// 0xB29DE0 (VM 0x059 EmitParticle): bit12 -> pool of bone ref w0 (the root bone: skip, +4; else
	// +2 first), else own pool. Slot = pool+2 counter++ % count; particle: state 1, script ip =
	// cursor + w, pos = CURRENT bone outPos << 8, tpage/clut/colour from the pool bone; ws+0x38/0x3C = 0.
	static void __cdecl op_059_EmitParticle()
	{
		uint8_t *bone;
		if (U8(RT(), 0x4B) & 0x10)
		{
			bone = blob::GetBone(S16(STREAM(), 2));
			if (bone == PTR(CTX(), 0x90))
			{
				STREAM() += 4; // 0xB29F0B
				return;
			}
			STREAM() += 2;
		}
		else
			bone = CUR();
		uint8_t *pool = PTR(bone, 0xB8);
		uint32_t idx = U16(pool, 2);
		int32_t cnt = U16(pool, 0);
		U16(pool, 2) = (uint16_t)(idx + 1);
		int32_t slot = (int32_t)idx % cnt; // idiv 0xB29E47 (count 0 = divide fault, as vanilla)
		U16(pool, 8) = U8(bone, 0x18);
		uint8_t *p = pool + slot * 0x50 + 0x10;
		U16(p, 0) = 1;
		U16(p, 2) = 0;
		uint8_t *s = STREAM();
		PTR(p, 4) = s + S16(s, 2);
		U32(WS(), 0x38) = 0;
		U32(WS(), 0x3C) = 0;
		U32(p, 0x20) = 0;
		U32(p, 0x24) = 0;
		U32(p, 0x28) = 0;
		U32(p, 0x2C) = 0;
		S32(p, 0x14) = shl32(S16(CUR(), 0x94), 8);
		S32(p, 0x18) = shl32(S16(CUR(), 0x96), 8);
		S32(p, 0x1C) = shl32(S16(CUR(), 0x98), 8);
		U16(p, 0x40) = U16(bone, 0x92);
		U16(p, 0x48) = U16(bone, 0x9A);
		uint32_t col = U32(bone, 0xCC) & 0x2FFFFFF;
		U32(p, 0x3C) = 0;
		U32(p, 0x38) = col;
		U8(p, 0x37) = 1;
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// battle: damage, entities, camera
	// ------------------------------------------------------------------------------------

	// 0xB2B530 (VM 0x022 ApplyActionResult): (op & 0xF000) == 0x8000 -> ApplyActionResultToTargets(
	// list, s8 count); else ApplyActionResultToTarget(entry whose slot == bone+0x1B) if present. +2
	static void __cdecl op_022_ApplyActionResult()
	{
		uint32_t w = U16(RT(), 0x4A);
		uint8_t *act = PTR(CTX(), 0xCC);
		uint8_t *rec = PTR(act, 8);
		int32_t n = S8(act, 0x10);
		if ((w & 0xF000) == 0x8000)
		{
			x::ApplyActionResultToTargets((int32_t)rec, n);
			STREAM() += 2;
			return;
		}
		uint32_t slot = U8(CUR(), 0x1B);
		if (n > 0)
		{
			do
			{
				if (U8(rec, 0) == slot)
				{
					x::ApplyActionResultToTarget((int32_t)rec);
					STREAM() += 2;
					return;
				}
				rec += 0x18;
				n--;
			} while (n > 0);
		}
		STREAM() += 2;
	}

	// 0xB2B5D0 (VM 0x13D BattleEntityVisibility): side = root bone slot (ctx+0x90 bone +0x1B) < 3
	// ? -1 : 0. w0 (jump table 0xB2B6EC): 1 hide caster side, 2 restore opposite side, 3 hide
	// opposite side, 4 restore all 7, 5 hide all 7, other restore caster side
	// (hide = entity word |= 4; restore = 0xB2B700). cursor += 4.
	static void __cdecl op_13D_BattleEntityVisibility()
	{
		int32_t side = 0;
		if (U8(PTR(CTX(), 0x90), 0x1B) < 3) side = -1;
		int32_t mode = (int32_t)S16(STREAM(), 2) - 1;
		switch ((uint32_t)mode)
		{
		case 0: // 0xB2B680
			if (side != 0) EntityHide(0, 3); else EntityHide(3, 4);
			break;
		case 1: // 0xB2B675
			if (side != 0) EntityRestore_B2B700(3, 4); else EntityRestore_B2B700(0, 3);
			break;
		case 2: // 0xB2B633
			if (side == 0) EntityHide(0, 3); else EntityHide(3, 4);
			break;
		case 3: // 0xB2B62A
			EntityRestore_B2B700(0, 7);
			break;
		case 4: // 0xB2B607
			EntityHide(0, 7);
			break;
		default: // 0xB2B6C0
			if (side == 0) EntityRestore_B2B700(3, 4); else EntityRestore_B2B700(0, 3);
			break;
		}
		STREAM() += 4;
	}

	// 0xB2B9D0 (VM 0x047 CameraShakeFromOutPos): battle camera shake offsets 0x1D97710/12/14 = bone outPos
	static void __cdecl op_047_CameraShakeFromOutPos()
	{
		uint8_t *b = CUR();
		guard(&CamShake()[0], 2);
		CamShake()[0] = U16(b, 0x94);
		guard(&CamShake()[1], 2);
		CamShake()[1] = U16(b, 0x96);
		guard(&CamShake()[2], 2);
		CamShake()[2] = U16(b, 0x98);
		STREAM() += 2;
	}

	// 0xB2B750 (VM 0x039 CameraAndSound): sub = op>>12 (jump table 0xB2B930):
	// 1 Battle_PlayCameraAnimation(.00 + U32(.00,0x20)); 2 SceneHeader+0x168[lo w0] =
	// ClaimVoiceSlot(.00 + U32(.00,0x24), hi w0 + 1, 0x80); 3 sub_4A2940 on the 3 voice slots;
	// 4 save camera (0xB8B7F0..FF -> 0xB8B800..0F, H 0x1D8E038 -> 0x1D977A0, 0x1D977A2 -> 0x1D9771C);
	// 5, 6 sub_504270(1); 7 sub_504270(0); 8 BS_Camera_ArmReturnAfterEffect; other (0, 9..15):
	// unless SequenceState+9 & 0x20, eye = bone outPos, H = bone+0x8C, 0x1D977A2 = bone+0x8E,
	// look-at = outPos of bone ref w0. cursor += 4.
	static void __cdecl op_039_CameraAndSound()
	{
		uint32_t sub = (uint32_t)U16(RT(), 0x4A) >> 12;
		switch (sub - 1)
		{
		case 0: // 0xB2B785
		{
			uint8_t *f = RESFILE()[0];
			x::PlayCameraAnimation(f + U32(f, 0x20));
			break;
		}
		case 1: // 0xB2B7A7
		{
			uint8_t *f = RESFILE()[0];
			uint8_t *s = STREAM();
			uint32_t slot = U8(s, 2);
			uint8_t *snd = f + U32(f, 0x24);
			int32_t n = (int32_t)U8(s, 3) + 1;
			uint32_t v = x::ClaimVoiceSlot(snd, n, 0x80);
			U32(SCENE(), slot * 4 + 0x168) = v;
			break;
		}
		case 2: // 0xB2B7F1
			for (int32_t off = 0x168; off < 0x174; off += 4)
			{
				uint32_t v = U32(SCENE(), off);
				if (v) x::Snd_4A2940((int32_t)v);
			}
			break;
		case 3: // 0xB2B826
		{
			uint32_t e0 = CamWord32(0), e1 = CamWord32(1), a0 = CamWord32(2);
			guard(&CamWord32(4), 4);
			CamWord32(4) = e0;             // 0xB8B800
			uint32_t a1 = CamWord32(3);
			guard(&CamWord32(5), 4);
			CamWord32(5) = e1;             // 0xB8B804
			uint16_t h = (uint16_t)PROJH();
			guard(&CamWord32(7), 4);
			CamWord32(7) = a1;             // 0xB8B80C
			guard(&CamWord32(6), 4);
			CamWord32(6) = a0;             // 0xB8B808
			uint16_t w = Word1D977A2();
			guard(&Word1D977A0(), 2);
			Word1D977A0() = h;
			guard(&Word1D9771C(), 2);
			Word1D9771C() = w;
			break;
		}
		case 4: // 0xB2B87D
		case 5:
			x::Camera_504270(1);
			break;
		case 6: // 0xB2B896
			x::Camera_504270(0);
			break;
		case 7: // 0xB2B771
			x::CameraArmReturn();
			break;
		default: // 0xB2B8AF
			if (!(U8(SEQ(), 9) & 0x20))
			{
				uint8_t *b = CUR();
				guard(&CamWord32(0), 4);
				CamWord32(0) = U32(b, 0x94);   // eye x, y
				guard(CAMEYE() + 2, 2);
				CAMEYE()[2] = S16(b, 0x98);    // eye z
				guard(&PROJH(), 2);
				PROJH() = S16(b, 0x8C);
				guard(&Word1D977A2(), 2);
				Word1D977A2() = U16(b, 0x8E);
				uint8_t *t = blob::GetBone(S16(STREAM(), 2));
				guard(&CamWord32(2), 4);
				CamWord32(2) = U32(t, 0x94);   // look-at x, y
				guard(CAMAT() + 2, 2);
				CAMAT()[2] = S16(t, 0x98);     // look-at z
			}
			break;
		}
		STREAM() += 4;
	}
}

	// 0xB26860: create the embedded battle-model block (0x80 B, arena) for object id & 0xFF:
	// +8 u16 id (+9 = animation = id >> 8), +0x10 BattleAnimHeader (+0x14 -> +0x30), +0x20
	// BattleAnimCmd, +0x30/34/38 = object sections 1..3 (skeleton, anim, geometry), defaults,
	// then pre_Battle_ReadAnimation(+0x10, +0x20, anim). Returns the block.
	uint8_t *h_B26860(int32_t id)
	{
		uint8_t *obj = blob::ObjectPtr(id & 0xFF);
		uint8_t *b = blob::ArenaAlloc(0x80);
		uint8_t *hdr = b + 0x10;
		uint8_t *sec = b + 0x30;
		uint8_t *cmd = b + 0x20;
		U16(b, 8) = (uint16_t)id;
		PTR(hdr, 4) = sec;
		PTR(sec, 0) = obj + U32(obj, 4);
		PTR(sec, 4) = obj + U32(obj, 8);
		PTR(sec, 8) = obj + U32(obj, 0xC);
		U32(b, 0x54) = 0;
		U32(b, 0x58) = 0xD00140;
		U32(b, 0x5C) = 0x808080;
		U32(b, 0x60) = 0xFFFFFFFF;
		U16(b, 0x64) = 0;
		U32(b, 0x68) = 0;
		U32(b, 0x50) = 0xFFFFFFFF;
		U16(b, 0) = 0;
		U16(b, 2) = 0;
		U8(cmd, 1) = 0;
		U32(b, 0x78) = 0x808080;
		// pre_Battle_ReadAnimation (+ Battle_ReadAnimation) write the skeleton pose, which lives in
		// the .00 file data (outside the snapshot): header byte +1, root +8..+0xC, bone i
		// (0x30 B at +0x10) words +4..+0xE. Journal the whole skeleton while predicting.
		{
			uint8_t *skel = PTR(sec, 0);
			guard(skel, 0x10 + U8(skel, 0) * 0x30);
		}
		x::PreReadAnimation(hdr, cmd, U8(b, 9));
		return b;
	}

	void fill_vm_a(Generic &g)
	{
		g.vm[0x002] = part_vm_a::op_002_Jump;
		g.vm[0x003] = part_vm_a::op_003_Call;
		g.vm[0x004] = part_vm_a::op_004_Return;
		g.vm[0x006] = part_vm_a::op_006_LoadFile;
		g.vm[0x007] = part_vm_a::op_007_SetFreezeOthers;
		g.vm[0x008] = part_vm_a::op_008_WaitLoadDone;
		g.vm[0x01E] = part_vm_a::op_01E_SetLoopCounter;
		g.vm[0x01F] = part_vm_a::op_01F_LoopDecJump;
		g.vm[0x021] = part_vm_a::op_021_SetDataPointer;
		g.vm[0x022] = part_vm_a::op_022_ApplyActionResult;
		g.vm[0x02B] = part_vm_a::op_02B_PlaySE;
		g.vm[0x02C] = part_vm_a::op_02C_StopSoundChannels;
		g.vm[0x030] = part_vm_a::op_030_PlaySummonStream;
		g.vm[0x031] = part_vm_a::op_031_TransSummonStream;
		g.vm[0x033] = part_vm_a::op_033_SetBoneId;
		g.vm[0x034] = part_vm_a::op_034_WaitStreamReady;
		g.vm[0x039] = part_vm_a::op_039_CameraAndSound;
		g.vm[0x03B] = part_vm_a::op_03B_SequenceFlags;
		g.vm[0x03F] = part_vm_a::op_03F_CreateEmbeddedModel;
		g.vm[0x041] = part_vm_a::op_041_EmbeddedModelFlags;
		g.vm[0x043] = part_vm_a::op_043_SetDrawHandlerInlineData;
		g.vm[0x044] = part_vm_a::op_044_SetDrawHandlerInlineDataReset;
		g.vm[0x046] = part_vm_a::op_046_JumpIfTargetResult;
		g.vm[0x047] = part_vm_a::op_047_CameraShakeFromOutPos;
		g.vm[0x04D] = part_vm_a::op_04D_SetupScreenTint;
		g.vm[0x04E] = part_vm_a::op_04E_SetDescriptorInlineData;
		g.vm[0x053] = part_vm_a::op_053_MusicVolume;
		g.vm[0x055] = part_vm_a::op_055_JumpIfSummonState;
		g.vm[0x057] = part_vm_a::op_057_Nop2;
		g.vm[0x058] = part_vm_a::op_058_CreateParticlePool;
		g.vm[0x059] = part_vm_a::op_059_EmitParticle;
		g.vm[0x064] = part_vm_a::op_064_SetBoneSlot;
		g.vm[0x07B] = part_vm_a::op_07B_WaitThenReloadA8defTim;
		g.vm[0x089] = part_vm_a::op_089_StepEmbeddedModelAnim;
		g.vm[0x0A1] = part_vm_a::op_0A1_RandomJump;
		g.vm[0x0A5] = part_vm_a::op_0A5_Nop4;
		g.vm[0x0A6] = part_vm_a::op_0A6_Nop4;
		g.vm[0x0A7] = part_vm_a::op_0A7_SetCounter;
		g.vm[0x0A8] = part_vm_a::op_0A8_DecCounterJumpNZ;
		g.vm[0x0B1] = part_vm_a::op_0B1_SetSeqStateByte6;
		g.vm[0x0B2] = part_vm_a::op_0B2_SetCtxByteA2;
		g.vm[0x0B8] = part_vm_a::op_0B8_Nop8;
		g.vm[0x0C3] = part_vm_a::op_0C3_PauseResumeSounds;
		g.vm[0x0CD] = part_vm_a::op_0CD_SetRenderListOffset;
		g.vm[0x0CE] = part_vm_a::op_0CE_SetBoneIdFromSlot;
		g.vm[0x120] = part_vm_a::op_120_SetArenaTop;
		g.vm[0x13D] = part_vm_a::op_13D_BattleEntityVisibility;
	}
}
}

// ============================================================================================
// part vm_b
// ============================================================================================
// GF cinematic engine, part vm_b: VM opcodes 0xB2BC60..0xB2D2B0 of the Ifrit (201) copy
// (flag ops, waits, conditional jumps, the generic accumulator/velocity write opcode 0x0A..0x17,
// bone integrator setters, battle-entity links) + helpers h_B2C440 / h_B2D460.


namespace ff8fx
{
namespace gfc
{
namespace part_vm_b
{
	// ------------------------------------------------------------------------------------
	// local helpers
	// ------------------------------------------------------------------------------------
	// x86 `shl r, cl` / `sar r, cl`: the count is masked to 5 bits
	static inline int32_t shl_cl(int32_t v, int32_t c) { return (int32_t)((uint32_t)v << (c & 31)); }
	static inline int32_t sar_cl(int32_t v, int32_t c) { return v >> (c & 31); }
	// 16-bit wrapping add (`add word ptr [..], r16`)
	static inline int16_t add16(int16_t a, int32_t b) { return (int16_t)(uint16_t)((uint16_t)a + (uint16_t)b); }
	// two's-complement negation (`not; inc`)
	static inline int32_t neg32(int32_t v) { return (int32_t)(0u - (uint32_t)v); }

	// battle-model instance of the current bone's slot: *(SceneHeader + 0x60 + 4 * bone+0x1B)
	// (FF8BattleEntitySlotData; outside the engine snapshot -> every write needs guard())
	static inline uint8_t *Entity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	// bone handler of the current bone: call [BoneHandlerTable + 4 * bone+0x18]
	// (index is a raw u8: indices >= 18 read past the table into the prim table, exactly as the
	// Clone struct lays out bone[18] followed by prim[60])
	static inline void CallBoneHandler() { (&C().bone[0])[U8(CUR(), 0x18)](); }

	// VM yield: rt->vmWaitRequest (rt+0x3E) = bone+0xC8 (channel wait speed), cursor unchanged
	static inline void Yield() { U16(RT(), 0x3E) = U16(CUR(), 0xC8); }

	// a write at bone+off (off from script data) may leave the bone; announce it then
	static inline void guard_bone(uint8_t *bone, int32_t off, int n)
	{
		if (off < 0 || off + n > 0x100) guard(bone + off, n);
	}

	// ------------------------------------------------------------------------------------
	// 0xB2BC60 (VM 0x02D SetCameraFlag): byte 0x1D97705 |= 0x80. No operand.
	static void __cdecl op_02D_SetCameraFlag()
	{
		uint8_t *s = STREAM();
		guard((void *)0x1D97705, 1);
		MEM<uint8_t>(0x1D97705) |= 0x80;
		STREAM() = s + 2;
	}

	// 0xB2BC80 (VM 0x045 EntityFlagBits): u16 flags word at +0 of the slot entity.
	// op>>12: 0 set w0; 1 clear w0 if (SceneHeader+0x4E[slot] & w0) == 0; 8 clear w0; else nothing.
	static void __cdecl op_045_EntityFlagBits()
	{
		int32_t slot = U8(CUR(), 0x1B);
		uint8_t *scene = SCENE();
		uint8_t *e = PTR(scene, 0x60 + 4 * slot);
		int32_t sw = S16(scene, 0x4E + 2 * slot);
		int32_t w = S16(STREAM(), 2);
		uint32_t sub = (uint32_t)U16(RT(), 0x4A) >> 12;
		if (sub == 0)
		{
			guard(e, 2);
			U16(e, 0) |= (uint16_t)w;
		}
		else if (sub == 1)
		{
			if ((w & sw) == 0)
			{
				guard(e, 2);
				U16(e, 0) &= (uint16_t)~w;
			}
		}
		else if (sub == 8)
		{
			guard(e, 2);
			U16(e, 0) &= (uint16_t)~w;
		}
		STREAM() += 4;
	}

	// 0xB2BD10 (VM 0x049 JumpIfEntitySize): s16 w0 value, rel16 w1 target. e = slot entity.
	// op & 0xFE00: e+0x26 < w0 -> jump else +6; otherwise e+0x26 <= w0 -> +6 else jump.
	static void __cdecl op_049_JumpIfEntitySize()
	{
		uint8_t *e = Entity();
		uint8_t *s = STREAM();
		uint32_t op = U16(RT(), 0x4A);
		int32_t w = S16(s, 2);
		int32_t v = S16(e, 0x26);
		bool jump;
		if (op & 0xFFFFFE00) jump = v < w;
		else jump = !(v <= w);
		if (jump) STREAM() = s + S16(s, 4);
		else STREAM() = s + 6;
	}

	// 0xB2BD70 (VM 0x06D PartyRecordFlag): 4 records of 0x2C B at 0x1D9898C, byte +5:
	// bit15 of op -> &= ~2, else |= 2.
	static void __cdecl op_06D_PartyRecordFlag()
	{
		bool clr = (U8(RT(), 0x4B) & 0x80) != 0;
		uint8_t *p = (uint8_t *)0x1D98991;
		for (int i = 0; i < 4; i++, p += 0x2C)
		{
			guard(p, 1);
			if (clr) *p &= 0xFD;
			else *p |= 2;
		}
		STREAM() += 2;
	}

	// 0xB2BDC0 (VM 0x01C PokeBoneField): *(bone + s16 w0) = w1; (op>>9) == 1 -> word, else byte.
	static void __cdecl op_01C_PokeBoneField()
	{
		uint8_t *s = STREAM();
		uint8_t *bone = CUR();
		int32_t off = S16(s, 2);
		uint32_t k = (uint32_t)U16(RT(), 0x4A) >> 9;
		if (k == 1)
		{
			guard_bone(bone, off, 2);
			U16(bone, off) = U16(s, 4);
		}
		else
		{
			guard_bone(bone, off, 1);
			U8(bone, off) = U8(s, 4);
		}
		STREAM() += 6;
	}

	// 0xB2BE10 (VM 0x01B Nop4): skip one operand word.
	static void __cdecl op_01B_Nop4() { STREAM() += 4; }

	// 0xB2BE20 (VM 0x077 Nop6): skip two operand words.
	static void __cdecl op_077_Nop6() { STREAM() += 6; }

	// 0xB2BE30 (VM 0x079 CompareFieldJump): s16 w0 value, rel16 w1 target. f = (op>>12)&7,
	// v = hi16 of dword bone+0x50+4f. bit15: v <= w0 -> jump else +6; else v < w0 -> +6 else jump.
	static void __cdecl op_079_CompareFieldJump()
	{
		uint32_t op = U16(RT(), 0x4A);
		uint8_t *s = STREAM();
		int32_t off = ((int32_t)op >> 10) & 0x1C;
		int32_t v = S32(CUR(), off + 0x50) >> 16;
		int32_t w = S16(s, 2);
		bool jump;
		if (op & 0x8000) jump = v <= w;
		else jump = !(v < w);
		if (jump) STREAM() = s + S16(s, 4);
		else STREAM() = s + 6;
	}

	// 0xB2BE90 (VM 0x07D SetBoneCount): s16 w0 count. Grow only: clears dword +0 of bones
	// [SceneHeader+0x38 .. count) and sets SceneHeader+0x38 = count.
	static void __cdecl op_07D_SetBoneCount()
	{
		int32_t n = S16(STREAM(), 2);
		uint32_t cur = U16(SCENE(), 0x38);
		int32_t d = sub32(n, (int32_t)cur);
		if (d > 0)
		{
			uint8_t *b = PTR(CTX(), 0x90) + (cur << 8);
			do
			{
				U32(b, 0) = 0;
				b += 0x100;
			} while (--d != 0);
			S16(SCENE(), 0x38) = (int16_t)n;
		}
		STREAM() += 4;
	}

	// 0xB2BEF0 (VM 0x08A BattleScreenFx): op>>12: 1 sub_501F30(w0) +4; 2 sub_501E40(w0) +4;
	// 3 sub_4A8480(0) +2; else ctx+0xC0 struct: +0xB = w0+1, +9 = w1 (bytes), +6.
	static void __cdecl op_08A_BattleScreenFx()
	{
		uint32_t sub = (uint32_t)U16(RT(), 0x4A) >> 12;
		if (sub == 1)
		{
			x::Battle_501F30(S16(STREAM(), 2));
			STREAM() += 4;
		}
		else if (sub == 2)
		{
			x::Battle_501E40(S16(STREAM(), 2));
			STREAM() += 4;
		}
		else if (sub == 3)
		{
			x::Battle_4A8480(0);
			STREAM() += 2;
		}
		else
		{
			uint8_t *p = PTR(CTX(), 0xC0);
			guard(p + 0xB, 1);
			U8(p, 0xB) = (uint8_t)(U8(STREAM(), 2) + 1);
			guard(p + 9, 1);
			U8(p, 9) = U8(STREAM(), 4);
			STREAM() += 6;
		}
	}

	// 0xB2BFA0 (VM 0x08D ModelFlagsSetClear): s16 w0 (sign-extended) on dword +0x7C of the slot
	// entity: bit15 of op -> &= ~w0, else |= w0.
	static void __cdecl op_08D_ModelFlagsSetClear()
	{
		uint8_t *e = Entity();
		int32_t w = S16(STREAM(), 2);
		guard(e + 0x7C, 4);
		if (U8(RT(), 0x4B) & 0x80) U32(e, 0x7C) &= ~(uint32_t)w;
		else U32(e, 0x7C) |= (uint32_t)w;
		STREAM() += 4;
	}

	// 0xB2C030 (VM 0x005 FlagOp): shared flag word *(rt+0x10 = SceneHeader)+2, mask s16 w0.
	// op>>12: 0/6+ set; 1 clear; 2 jump w1 if (f&m)!=0; 3 jump w1 if ==0; 4 wait while !=0;
	// 5 wait while ==0.
	static void __cdecl op_005_FlagOp()
	{
		uint8_t *s = STREAM();
		uint8_t *rt = RT();
		int32_t m = S16(s, 2);
		uint8_t *fw = PTR(rt, 0x10);
		uint32_t sub = (uint32_t)U16(rt, 0x4A) >> 12;
		int32_t f = S16(fw, 2);
		if (sub == 0)
		{
			U16(fw, 2) = (uint16_t)(m | f);
			STREAM() = s + 4;
		}
		else if (sub == 1)
		{
			U16(fw, 2) = (uint16_t)(~m & f);
			STREAM() = s + 4;
		}
		else if (sub == 2)
		{
			if ((m & f) == 0) STREAM() = s + 6;
			else STREAM() = s + S16(s, 4);
		}
		else if (sub == 3)
		{
			if ((m & f) != 0) STREAM() = s + 6;
			else STREAM() = s + S16(s, 4);
		}
		else if (sub == 4)
		{
			if ((m & f) == 0) STREAM() = s + 4;
			else
			{
				U16(rt, 0x3E) = U16(CUR(), 0xC8);
				STREAM() = s;
			}
		}
		else if (sub == 5)
		{
			if ((m & f) != 0) STREAM() = s + 4;
			else
			{
				U16(rt, 0x3E) = U16(CUR(), 0xC8);
				STREAM() = s;
			}
		}
		else
		{
			U16(fw, 2) = (uint16_t)(m | f);
			STREAM() = s + 4;
		}
	}

	// 0xB2C100 (VM 0x10C BoneFlagOps): same as 0x05 on the current bone's flag word +0x4A
	// (jump table 0xB2C1B0: sub 1 clear, 2 jump if !=0, 3 jump if ==0, 4 wait while !=0,
	// 5 wait while ==0; 0 / 6+ set). Waits do not touch the cursor.
	static void __cdecl op_10C_BoneFlagOps()
	{
		uint8_t *rt = RT();
		uint8_t *bone = CUR();
		uint8_t *s = STREAM();
		uint32_t fl = U16(bone, 0x4A);
		int32_t m = S16(s, 2);
		uint32_t sub = (uint32_t)U16(rt, 0x4A) >> 12;
		switch (sub)
		{
		case 1: // 0xB2C190
			U16(bone, 0x4A) = (uint16_t)(~(uint32_t)m & fl);
			STREAM() += 4;
			return;
		case 2: // 0xB2C16F
			if ((fl & (uint32_t)m) == 0) STREAM() = s + 6;
			else STREAM() = s + S16(s, 4);
			return;
		case 3: // 0xB2C15B
			if ((fl & (uint32_t)m) != 0) STREAM() = s + 6;
			else STREAM() = s + S16(s, 4);
			return;
		case 4: // 0xB2C148
			if ((fl & (uint32_t)m) == 0) { STREAM() += 4; return; }
			U16(rt, 0x3E) = U16(bone, 0xC8);
			return;
		case 5: // 0xB2C135
			if ((fl & (uint32_t)m) != 0) { STREAM() += 4; return; }
			U16(rt, 0x3E) = U16(bone, 0xC8);
			return;
		default: // 0xB2C196
			U16(bone, 0x4A) = (uint16_t)((uint32_t)m | fl);
			STREAM() += 4;
			return;
		}
	}

	// 0xB2C1D0 (VM 0x10D WaitOtherBoneFlags): bone ref w0, mask w1: m = w1 & other+0x4A & 0x3FFF.
	// op bit12: wait while m != 0; else wait while m == 0; satisfied -> +6.
	static void __cdecl op_10D_WaitOtherBoneFlags()
	{
		uint8_t *o = blob::GetBone(S16(STREAM(), 2));
		uint32_t fl = U16(o, 0x4A);
		uint8_t *rt = RT();
		uint32_t m = ((uint32_t)U16(STREAM(), 4) & fl) & 0x3FFF;
		bool wait;
		if (U8(rt, 0x4B) & 0x10) wait = m != 0;
		else wait = m == 0;
		if (wait) U16(rt, 0x3E) = U16(CUR(), 0xC8);
		else STREAM() += 6;
	}

	// 0xB2C240 (VM 0x0D7 Nop2): skip one operand word.
	static void __cdecl op_0D7_Nop4() { STREAM() += 4; }

	// 0xB2C250 (VM 0x0E7 WaitUntilFieldGreater): s16 w0 offset from bone+0x8C, s16 w1 value:
	// field > value -> +6, else yield.
	static void __cdecl op_0E7_WaitUntilFieldGreater()
	{
		uint8_t *s = STREAM();
		uint8_t *bone = CUR();
		int32_t v = S16(bone, S16(s, 2) + 0x8C);
		if (v > (int32_t)S16(s, 4)) STREAM() = s + 6;
		else U16(RT(), 0x3E) = U16(bone, 0xC8);
	}

	// 0xB2C290 (VM 0x0FB WaitUntilFieldLess): field < value -> +6, else yield.
	static void __cdecl op_0FB_WaitUntilFieldLess()
	{
		uint8_t *s = STREAM();
		uint8_t *bone = CUR();
		int32_t v = S16(bone, S16(s, 2) + 0x8C);
		if (v < (int32_t)S16(s, 4)) STREAM() = s + 6;
		else U16(RT(), 0x3E) = U16(bone, 0xC8);
	}

	// 0xB2C2D0 (VM 0x106 WaitUntilFieldLessThanBone): s16 w0 offset, bone ref w1:
	// cur.field < other.field (s16) -> +6, else yield.
	static void __cdecl op_106_WaitUntilFieldLessThanBone()
	{
		int32_t off = S16(STREAM(), 2);
		uint8_t *o = blob::GetBone(S16(STREAM(), 4));
		uint8_t *bone = CUR();
		if (S16(bone, off + 0x8C) < S16(o, off + 0x8C)) STREAM() += 6;
		else U16(RT(), 0x3E) = U16(bone, 0xC8);
	}

	// 0xB2C320 (VM 0x0FD JumpIfFieldInRange): w0 offset, w1 min, w2 max, rel16 w3:
	// min <= field <= max -> jump, else +10.
	static void __cdecl op_0FD_JumpIfFieldInRange()
	{
		uint8_t *s = STREAM();
		int32_t v = S16(CUR(), S16(s, 2) + 0x8C);
		if (v < (int32_t)S16(s, 4) || v > (int32_t)S16(s, 6)) STREAM() = s + 10;
		else STREAM() = s + S16(s, 8);
	}

	// 0xB2C360 (VM 0x074 DrawFlagBits): bone+0x4C |= w0; bit15 of op: flags &= ~w0 then
	// flags |= ~w0 (vanilla bug 0xB2C37B: the 'clear' falls through into the OR with the
	// inverted mask -> flags = ~w0 | ...).
	static void __cdecl op_074_DrawFlagBits()
	{
		bool clr = (U8(RT(), 0x4B) & 0x80) != 0;
		int32_t w = S16(STREAM(), 2);
		if (clr)
		{
			w = ~w;
			U16(CUR(), 0x4C) &= (uint16_t)w;
		}
		U16(CUR(), 0x4C) |= (uint16_t)w;
		STREAM() += 4;
	}

	// 0xB2C3B0 (VM 0x0C1 Nop): skip the opcode word.
	static void __cdecl op_0C1_Nop() { STREAM() += 2; }

	// 0xB2C3D0 (VM 0x04C SetBoneHandler): bone+0x18 = (u8)w0; runs BoneHandlerTable[w0].
	static void __cdecl op_04C_SetBoneHandler()
	{
		U8(CUR(), 0x18) = U8(STREAM(), 2);
		CallBoneHandler();
		STREAM() += 4;
	}

	// 0xB2C400 (VM 0x037 ResetAccum): zero accumRot/accumPos (+0x50..+0x67), bone handler,
	// outAngle = hi16(accumRot).
	static void __cdecl op_037_ResetAccum()
	{
		uint8_t *bone = CUR();
		for (int i = 0; i < 6; i++) U32(bone, 0x50 + 4 * i) = 0;
		CallBoneHandler();
		h_B2C440();
		STREAM() += 2;
	}

	// 0xB2C480 (VM 0x00A..0x017 generic write): attribute byte a = attr_16A[op & 0x1FF]
	// (ws+0xE4); field descriptor d = attr_184[4*(a & 15)] = {u8 bone offset, s8 shift, u8 width,
	// u8 post-action (ws+0xE0: 1 = bone handler + outAngle, 2 = integrator flags)}.
	// Mode a>>4: 0/6+ set per component (0x7654 = skip), 1 set one value, 2 add per component
	// (0x7654 = skip), 3 add rand73(w) per component (0 = skip), 4 add one rand73(w),
	// 5 add (+/-base + rand73(range)) per component. Component mask = op bits 15..10.
	static void __cdecl op_00A_GenericWrite()
	{
		uint32_t mask = U16(RT(), 0x4A); // [ebp-4]: bit 15 tested, shifted left per component
		uint32_t a = U8((const void *)C().attr_16A, (int32_t)(mask & 0x1FF));
		U32(WS(), 0xE4) = a;
		int32_t idx = (int32_t)(a & 0xF);
		int32_t off = U8((const void *)C().attr_184, idx * 4);
		int32_t width = U8((const void *)C().attr_186, idx * 4);
		uint8_t *field = off + CUR();
		int32_t shift = S8((const void *)C().attr_184, idx * 4 + 1);
		U32(WS(), 0xE0) = U8((const void *)C().attr_184, idx * 4 + 3);
		uint8_t *arg = STREAM() + 2;
		int32_t count = width * 6;
		int32_t mode = (int32_t)U32(WS(), 0xE4) >> 4;
		if (mode == 1)
		{
			int32_t v = S16(arg, 0);
			if (width == 2) v = sar_cl(v, shift);
			else v = shl_cl(v, shift);
			for (int32_t o = 0; o < count; o += width)
			{
				if ((int16_t)mask < 0)
				{
					if (width != 2) S32(field, o) = v;
					else S16(field, o) = (int16_t)v;
				}
				mask <<= 1;
			}
			arg += 2;
		}
		else if (mode == 2)
		{
			for (int32_t o = 0; o < count; o += width)
			{
				if ((int16_t)mask < 0)
				{
					int32_t v = S16(arg, 0);
					if (v != 0x7654)
					{
						if (width != 2) S32(field, o) = add32(S32(field, o), shl_cl(v, shift));
						else S16(field, o) = add16(S16(field, o), shl_cl(v, shift));
					}
					arg += 2;
				}
				mask <<= 1;
			}
		}
		else if (mode == 3)
		{
			for (int32_t o = 0; o < count; o += width)
			{
				if ((int16_t)mask < 0)
				{
					int32_t v = S16(arg, 0);
					if (v != 0)
					{
						int32_t r = blob::Rand73(v);
						if (width != 2) S32(field, o) = add32(S32(field, o), shl_cl(r, shift));
						else S16(field, o) = add16(S16(field, o), shl_cl(r, shift));
					}
					arg += 2;
				}
				mask <<= 1;
			}
		}
		else if (mode == 4)
		{
			int32_t r = blob::Rand73(S16(arg, 0));
			if (width == 2) r = sar_cl(r, shift);
			else r = shl_cl(r, shift);
			for (int32_t o = 0; o < count; o += width)
			{
				if ((int16_t)mask < 0)
				{
					if (width != 2) S32(field, o) = add32(S32(field, o), r);
					else S16(field, o) = add16(S16(field, o), r);
				}
				mask <<= 1;
			}
			arg += 2;
		}
		else if (mode == 5)
		{
			for (int32_t o = 0; o < count; o += width)
			{
				if ((int16_t)mask < 0)
				{
					int32_t base = S16(arg, 0);
					// vanilla quirk (0xB2C68F/0xB2C69A): rand73(base) is drawn and its result
					// discarded (eax is reloaded with the range word right after the call)
					blob::Rand73(base);
					int32_t range = S16(arg, 2);
					if (range < 0) base = neg32(base);
					int32_t r = blob::Rand73(range);
					base = add32(base, r);
					if (width != 2) S32(field, o) = add32(S32(field, o), shl_cl(base, shift));
					else S16(field, o) = add16(S16(field, o), shl_cl(base, shift));
					arg += 4;
				}
				mask <<= 1;
			}
		}
		else // 0xB2C6E7: mode 0 and 6..15
		{
			for (int32_t o = 0; o < count; o += width)
			{
				if ((int16_t)mask < 0)
				{
					int32_t v = S16(arg, 0);
					if (v != 0x7654)
					{
						if (width != 2) S32(field, o) = shl_cl(v, shift);
						else S16(field, o) = (int16_t)sar_cl(v, shift);
					}
					arg += 2;
				}
				mask <<= 1;
			}
		}
		STREAM() = arg;
		uint32_t post = U32(WS(), 0xE0);
		if (post == 1)
		{
			CallBoneHandler();
			h_B2C440();
		}
		else if (post == 2)
		{
			h_B2D460();
		}
	}

	// 0xB2C780 (VM 0x05F PosFromEntityOffset): accumPos = (entity+0x1C xyz + w0..w2) << 16
	// (component skipped when w == 0x7654); boneHandlerId = 1, runs BoneHandler[1].
	static void __cdecl op_05F_PosFromEntityOffset()
	{
		uint8_t *bone = CUR();
		uint8_t *p = Entity() + 0x1C;
		int32_t w = S16(STREAM(), 2);
		if (w != 0x7654)
		{
			S32(bone, 0x5C) = shl32(add32(S16(p, 0), w), 16);
			bone = CUR();
		}
		w = S16(STREAM(), 4);
		if (w != 0x7654)
		{
			S32(bone, 0x60) = shl32(add32(S16(p, 2), w), 16);
			bone = CUR();
		}
		w = S16(STREAM(), 6);
		if (w != 0x7654)
		{
			S32(bone, 0x64) = shl32(add32(S16(p, 4), w), 16);
			bone = CUR();
		}
		U8(bone, 0x18) = 1;
		C().bone[1]();
		STREAM() += 8;
	}

	// 0xB2C820 (VM 0x072 RotFromEntity): accumRot = entity+0x0C xyz << 16; outAngle refresh.
	static void __cdecl op_072_RotFromEntity()
	{
		uint8_t *bone = CUR();
		uint8_t *p = Entity() + 0xC;
		S32(bone, 0x50) = shl32(S16(p, 0), 16);
		S32(CUR(), 0x54) = shl32(S16(p, 2), 16);
		S32(CUR(), 0x58) = shl32(S16(p, 4), 16);
		h_B2C440();
		STREAM() += 2;
	}

	// 0xB2C870 (VM 0x05E PosFromSlotHome): accumPos = s16 xyz at SceneHeader+0x84+8*slot << 16;
	// bone handler, outAngle refresh.
	static void __cdecl op_05E_PosFromSlotHome()
	{
		uint8_t *bone = CUR();
		uint8_t *p = SCENE() + 8 * (int32_t)U8(bone, 0x1B) + 0x84;
		S32(bone, 0x5C) = shl32(S16(p, 0), 16);
		S32(CUR(), 0x60) = shl32(S16(p, 2), 16);
		S32(CUR(), 0x64) = shl32(S16(p, 4), 16);
		CallBoneHandler();
		h_B2C440();
		STREAM() += 2;
	}

	// 0xB2C8D0 (VM 0x02F StopMotion): zero velRot/velPos/accel (+0x68..+0x8B), integrator flags.
	static void __cdecl op_02F_StopMotion()
	{
		uint8_t *bone = CUR();
		for (int i = 0; i < 9; i++) U32(bone, 0x68 + 4 * i) = 0;
		h_B2D460();
		STREAM() += 2;
	}

	// 0xB2C900 (VM 0x082 VelocityTowardBone): bone ref w0, s16 w1 frames:
	// velPos = (other.accumPos - cur.accumPos) / frames.
	static void __cdecl op_082_VelocityTowardBone()
	{
		uint8_t *o = blob::GetBone(S16(STREAM(), 2));
		int32_t n = S16(STREAM(), 4);
		uint8_t *b = CUR();
		S32(b, 0x74) = sub32(S32(o, 0x5C), S32(b, 0x5C)) / n;
		b = CUR();
		S32(b, 0x78) = sub32(S32(o, 0x60), S32(b, 0x60)) / n;
		int32_t t = S32(o, 0x64);
		b = CUR();
		S32(b, 0x7C) = sub32(t, S32(b, 0x64)) / n;
		STREAM() += 6;
	}

	// 0xB2C970 (VM 0x0C0 VelPosTowardSpawner): target = bone ref (u16 bone+0x14), s16 w0 frames:
	// velPos = (target.accumPos - cur.accumPos) / frames.
	static void __cdecl op_0C0_VelPosTowardSpawner()
	{
		uint8_t *o = blob::GetBone((int32_t)U16(CUR(), 0x14));
		int32_t n = S16(STREAM(), 2);
		uint8_t *b = CUR();
		S32(b, 0x74) = sub32(S32(o, 0x5C), S32(b, 0x5C)) / n;
		b = CUR();
		S32(b, 0x78) = sub32(S32(o, 0x60), S32(b, 0x60)) / n;
		int32_t t = S32(o, 0x64);
		b = CUR();
		S32(b, 0x7C) = sub32(t, S32(b, 0x64)) / n;
		STREAM() += 4;
	}

	// 0xB2C9E0 (VM 0x048 OverrideEntityTransform): entity+1 |= 0x10; (op>>9)&15: 0 entity
	// +0x1C..+0x20 = outPos (+0x24 = outPosY if op bit15); 1 entity +0x0C..+0x10 = outAngle.
	static void __cdecl op_048_OverrideEntityTransform()
	{
		uint8_t *e = Entity();
		guard(e + 1, 1);
		U8(e, 1) |= 0x10;
		uint32_t k = ((uint32_t)U16(RT(), 0x4A) >> 9) & 0xF;
		if (k == 0)
		{
			uint8_t *p = CUR() + 0x94;
			guard(e + 0x1C, 6);
			U32(e, 0x1C) = U32(p, 0);
			U16(e, 0x20) = U16(p, 4);
			if (U8(RT(), 0x4B) & 0x80)
			{
				guard(e + 0x24, 2);
				U16(e, 0x24) = U16(p, 2);
			}
		}
		else if (k == 1)
		{
			uint8_t *p = CUR() + 0x8C;
			guard(e + 0xC, 6);
			U32(e, 0xC) = U32(p, 0);
			U16(e, 0x10) = U16(p, 4);
		}
		STREAM() += 2;
	}

	// 0xB2CA80 (VM 0x0B9 NegateField): s16 w0 i; if i > 0: dword bone+0x50+4i = -itself;
	// i < 3 -> outAngle refresh, 3..5 -> bone handler, >= 6 nothing more.
	static void __cdecl op_0B9_NegateField()
	{
		int32_t i = S16(STREAM(), 2);
		uint8_t *bone = CUR();
		if (i > 0)
		{
			guard_bone(bone, 0x50 + 4 * i, 4);
			S32(bone, 0x50 + 4 * i) = neg32(S32(bone, 0x50 + 4 * i));
			if (i < 6)
			{
				if (i >= 3)
				{
					CallBoneHandler();
					STREAM() += 4;
					return;
				}
				h_B2C440();
			}
		}
		STREAM() += 4;
	}

	// 0xB2CAD0 (VM 0x0BA NegateFieldNeg): same as 0xB9 but only for i < 0 (so always the
	// outAngle refresh path; writes BEFORE bone+0x50, vanilla sibling bug).
	static void __cdecl op_0BA_NegateFieldNeg()
	{
		int32_t i = S16(STREAM(), 2);
		uint8_t *bone = CUR();
		if (i < 0)
		{
			guard_bone(bone, 0x50 + 4 * i, 4);
			S32(bone, 0x50 + 4 * i) = neg32(S32(bone, 0x50 + 4 * i));
			if (i < 6)
			{
				if (i >= 3)
				{
					CallBoneHandler();
					STREAM() += 4;
					return;
				}
				h_B2C440();
			}
		}
		STREAM() += 4;
	}

	// 0xB2CB20 (VM 0x0D2 NegateMotionComponent): s16 w0 i: dword bone+0x50+4i = -itself;
	// bone handler; outAngle refresh.
	static void __cdecl op_0D2_NegateMotionComponent()
	{
		int32_t i = S16(STREAM(), 2);
		uint8_t *bone = CUR();
		guard_bone(bone, 0x50 + 4 * i, 4);
		S32(bone, 0x50 + 4 * i) = neg32(S32(bone, 0x50 + 4 * i));
		CallBoneHandler();
		h_B2C440();
		STREAM() += 4;
	}

	// 0xB2CB70 (VM 0x0CC SetBoneHandler7): bone+0xB0 = w0; bone+0x18 = 7; runs BoneHandler[7].
	static void __cdecl op_0CC_SetBoneHandler7()
	{
		U16(CUR(), 0xB0) = U16(STREAM(), 2);
		U8(CUR(), 0x18) = 7;
		CallBoneHandler();
		STREAM() += 4;
	}

	// 0xB2CBD0 (VM 0x0F2 BakeOutPosToAccum): accumPos = outPos << 16; bone+0x18 = 0.
	static void __cdecl op_0F2_BakeOutPosToAccum()
	{
		uint8_t *b = CUR();
		S32(b, 0x5C) = shl32(S16(b, 0x94), 16);
		b = CUR();
		S32(b, 0x60) = shl32(S16(b, 0x96), 16);
		b = CUR();
		S32(b, 0x64) = shl32(S16(b, 0x98), 16);
		U8(CUR(), 0x18) = 0;
		STREAM() += 2;
	}

	// 0xB2CC20 (VM 0x110 PlaceAtMidpoint): bone refs w0 = A, w1 = B: cur+0x18 = A+0x18;
	// cur.accumPos = A.accumPos + (B.accumPos - A.accumPos) / 2; cursor += 6, then bone handler.
	static void __cdecl op_110_PlaceAtMidpoint()
	{
		uint8_t *a = blob::GetBone(S16(STREAM(), 2));
		uint8_t *b = blob::GetBone(S16(STREAM(), 4));
		U8(CUR(), 0x18) = U8(a, 0x18);
		int32_t ax = S32(a, 0x5C);
		int32_t bx = S32(b, 0x5C);
		int32_t ay = S32(a, 0x60);
		int32_t az = S32(a, 0x64);
		S32(CUR(), 0x5C) = add32(sub32(bx, ax) / 2, ax);
		int32_t by = S32(b, 0x60);
		S32(CUR(), 0x60) = add32(sub32(by, ay) / 2, ay);
		int32_t bz = S32(b, 0x64);
		S32(CUR(), 0x64) = add32(sub32(bz, az) / 2, az);
		STREAM() += 6;
		CallBoneHandler();
	}

	// 0xB2CCC0 (VM 0x061 CopyAccumFromBone / 0x062 CopyVelFromBone): u16 mask w0 (bits 0..5),
	// bone ref w1; op word == 0x0061 exactly -> accum (+0x50), else vel (+0x68). Then outAngle
	// refresh, bone handler.
	static void __cdecl op_061_CopyAccumOrVelFromBone()
	{
		uint8_t *src = blob::GetBone(S16(STREAM(), 4));
		uint8_t *dst;
		if (U16(RT(), 0x4A) == 0x61)
		{
			dst = CUR() + 0x50;
			src += 0x50;
		}
		else
		{
			src += 0x68;
			dst = CUR() + 0x68;
		}
		int32_t m = S16(STREAM(), 2);
		int32_t bit = 1;
		for (int i = 0; i < 6; i++)
		{
			if (m & bit) U32(dst, 0) = U32(src, 0);
			bit <<= 1;
			dst += 4;
			src += 4;
		}
		h_B2C440();
		CallBoneHandler();
		STREAM() += 6;
	}

	// 0xB2CD50 (VM 0x091 AccumFromBoneOutputs): s16 mask w0 (bits 0..5), bone ref w1:
	// cur.accum[i] = src.out[i] << 16 (outAngle +0x8C/8E/90, outPos +0x94/96/98); outAngle
	// refresh, bone handler.
	static void __cdecl op_091_AccumFromBoneOutputs()
	{
		uint8_t *src = blob::GetBone(S16(STREAM(), 4)) + 0x8C;
		uint8_t *dst = CUR() + 0x50;
		int32_t m = S16(STREAM(), 2);
		int32_t bit = 1;
		for (int outer = 2; outer != 0; outer--)
		{
			for (int i = 0; i < 3; i++)
			{
				if (m & bit) S32(dst, 0) = shl32(S16(src, 0), 16);
				bit <<= 1;
				dst += 4;
				src += 2;
			}
			src += 2;
		}
		h_B2C440();
		CallBoneHandler();
		STREAM() += 6;
	}

	// 0xB2CE00 (VM 0x060 PosFromEntityJoint): u16 w0 joint of the slot entity -> world position
	// (blob B66B80 -> ws+0xF0/F4/F8). op bit15: orbit, s16 w1 radius mul, s16 w2 angle/16:
	// Z = F8 + ((cos(16*w2)*e26 >> 4) * w1) / 65536, X = F0 + (sin...), Y = F4; cursor 8.
	// Else accumPos = F0/F4/F8 << 16, cursor 4. Then bone handler.
	static void __cdecl op_060_PosFromEntityJoint()
	{
		uint8_t *e = Entity();
		int32_t joint = S16(STREAM(), 2);
		blob::Blob_B66B80(e + 0x60, joint, 0x1000, e + 0x40);
		if (U8(RT(), 0x4B) & 0x80)
		{
			uint8_t *s = STREAM();
			int32_t sz = S16(e, 0x26);
			int32_t ang = shl32(S16(s, 6), 4);
			int32_t mul = S16(s, 4);
			int32_t t = x::Cos(ang);
			t = mul32(t, sz) >> 4;
			t = mul32(t, mul) / 65536;
			S32(CUR(), 0x64) = shl32(add32(S32(WS(), 0xF8), t), 16);
			t = x::Sin(ang);
			t = mul32(t, sz) >> 4;
			t = mul32(t, mul) / 65536;
			S32(CUR(), 0x5C) = shl32(add32(S32(WS(), 0xF0), t), 16);
			S32(CUR(), 0x60) = shl32(S32(WS(), 0xF4), 16);
			STREAM() += 8;
			CallBoneHandler();
			return;
		}
		S32(CUR(), 0x5C) = shl32(S32(WS(), 0xF0), 16);
		S32(CUR(), 0x60) = shl32(S32(WS(), 0xF4), 16);
		S32(CUR(), 0x64) = shl32(S32(WS(), 0xF8), 16);
		STREAM() += 4;
		CallBoneHandler();
	}

	// 0xB2CF70 (VM 0x06E PosFromTargetsJointAvg): s16 w0 joint; sums the joint world position
	// over the s8 SceneHeader+0x41 targets (entity index s8 SceneHeader+0x48+i); op bits 12/13/14
	// -> accumPos X/Y/Z = (sum / count) << 16 (division by zero when no target, vanilla).
	// Bone handler, cursor 4.
	static void __cdecl op_06E_PosFromTargetsJointAvg()
	{
		int32_t joint = S16(STREAM(), 2);
		uint8_t *scene = SCENE();
		int32_t sy = 0, sx = 0, sz = 0;
		int32_t i = 0;
		if (S8(scene, 0x41) > 0)
		{
			do
			{
				uint8_t *e = PTR(scene, 0x60 + 4 * (int32_t)S8(scene, 0x48 + i));
				blob::Blob_B66B80(e + 0x60, joint, 0x1000, e + 0x40);
				uint8_t *ws = WS();
				int32_t fx = S32(ws, 0xF0), fy = S32(ws, 0xF4), fz = S32(ws, 0xF8);
				sx = add32(sx, fx);
				sy = add32(sy, fy);
				sz = add32(sz, fz);
				scene = SCENE();
				i++;
			} while (i < (int32_t)S8(scene, 0x41));
		}
		int32_t n = S8(scene, 0x41);
		uint32_t k = (uint32_t)U16(RT(), 0x4A) >> 9;
		if (k & 8) S32(CUR(), 0x5C) = shl32(sx / n, 16);
		if (k & 0x10) S32(CUR(), 0x60) = shl32(sy / n, 16);
		if (k & 0x20) S32(CUR(), 0x64) = shl32(sz / n, 16);
		CallBoneHandler();
		STREAM() += 4;
	}

	// 0xB2D070 (VM 0x01A SetBoneHandlerParams): w0 = handler<<8 | N: bone+0x18 = (u8)(w0 >> 8),
	// bone+0xA0 = pointer to the N inline parameter words; cursor 4 + 2N.
	static void __cdecl op_01A_SetBoneHandlerParams()
	{
		int32_t w = S16(STREAM(), 2);
		int32_t n = w & 0xF;
		U8(CUR(), 0x18) = (uint8_t)(w >> 8);
		PTR(CUR(), 0xA0) = STREAM() + 4;
		STREAM() = STREAM() + n * 2 + 4;
	}

	// 0xB2D0B0 (VM 0x075 AccumFromEntitySize): s16 w0 mul. size = s16 entity+0x26; op bit9:
	// size = max(size, max(F4 - e36, e3C - F4)) with F4 = joint 0 height (blob B66B80).
	// Masked (op bits 15..10) accum dwords = (w0 * size) << 8; bone handler; outAngle refresh.
	static void __cdecl op_075_AccumFromEntitySize()
	{
		int32_t ops = S16(RT(), 0x4A);
		uint8_t *e = PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B));
		int32_t size = S16(e, 0x26);
		if (ops & 0x200)
		{
			blob::Blob_B66B80(e + 0x60, 0, 0x1000, e + 0x40);
			int32_t f4 = S32(WS(), 0xF4);
			int32_t a = sub32(f4, S16(e, 0x36));
			int32_t b = sub32(S16(e, 0x3C), f4);
			if (a < b) a = b;
			if (size < a) size = a;
		}
		int32_t v = shl32(mul32(S16(STREAM(), 2), size), 8);
		uint8_t *acc = CUR() + 0x50;
		uint32_t m = (uint32_t)ops;
		for (int32_t o = 0; o < 0x18; o += 4)
		{
			if (m & 0x8000) S32(acc, o) = v;
			m <<= 1;
		}
		CallBoneHandler();
		h_B2C440();
		STREAM() += 4;
	}

	// 0xB2D170 (VM 0x076 AccumAddRandom): bone ref w0 = range bone. For each component i (op
	// bits 15..10): accum(attr_194[i]) += rand73(s16 range.out(attr_19C[i])) << 16. Bone
	// handler, outAngle refresh.
	static void __cdecl op_076_AccumAddRandom()
	{
		uint32_t m = (uint32_t)(int32_t)S16(RT(), 0x4A);
		uint8_t *src = blob::GetBone(S16(STREAM(), 2));
		uint8_t *acc = CUR() + 0x50;
		for (int32_t i = 0; i < 6; i++)
		{
			if (m & 0x8000)
			{
				int32_t so = U8((const void *)C().attr_19C, i);
				int32_t r = blob::Rand73(S16(src, so + 0x8C));
				int32_t d = U8((const void *)C().attr_194, i);
				S32(acc, d) = add32(S32(acc, d), shl32(r, 16));
			}
			m <<= 1;
		}
		CallBoneHandler();
		h_B2C440();
		STREAM() += 4;
	}

	// 0xB2D210 (VM 0x07C RandomOffsetXZ): a = rand73(4096);
	// accumPosZ += (cos(a) * (w0 + rand73(w1))) << 4; accumPosX += (sin(a) * (w2 + rand73(w3))) << 4.
	static void __cdecl op_07C_RandomOffsetXZ()
	{
		int32_t a = blob::Rand73(0x1000);
		int32_t r = blob::Rand73(S16(STREAM(), 4));
		int32_t rad = add32(r, S16(STREAM(), 2));
		uint8_t *p = CUR() + 0x64;
		int32_t t = shl32(mul32(x::Cos(a), rad), 4);
		S32(p, 0) = add32(S32(p, 0), t);
		r = blob::Rand73(S16(STREAM(), 8));
		rad = add32(r, S16(STREAM(), 6));
		p = CUR() + 0x5C;
		t = shl32(mul32(x::Sin(a), rad), 4);
		S32(p, 0) = add32(S32(p, 0), t);
		STREAM() += 10;
	}

	// 0xB2D2B0 (VM 0x087 VelocityScaleAccum): s16 w0 scale256, s16 w1 frames. For each
	// component (op bits 15..10): vel = (((accum >> 16) * (w0 - 256)) << 8) / w1.
	static void __cdecl op_087_VelocityScaleAccum()
	{
		uint32_t m = (uint32_t)U16(RT(), 0x4A) << 16;
		uint8_t *vel = CUR() + 0x68;
		int32_t sc = S16(STREAM(), 2);
		int32_t fr = S16(STREAM(), 4);
		for (int i = 6; i != 0; i--)
		{
			if ((int32_t)m < 0)
			{
				int32_t a = S32(vel, -0x18) >> 16;
				int32_t v = shl32(mul32(add32(sc, -0x100), a), 8);
				S32(vel, 0) = v / fr;
			}
			vel += 4;
			m <<= 1;
		}
		STREAM() += 6;
	}
}

	// 0xB2C440 (helper): outAngle (bone+0x8C/8E/90) = hi16 of accumRot (bone+0x50/54/58).
	void h_B2C440()
	{
		S16(CUR(), 0x8C) = (int16_t)(S32(CUR(), 0x50) >> 16);
		S16(CUR(), 0x8E) = (int16_t)(S32(CUR(), 0x54) >> 16);
		S16(CUR(), 0x90) = (int16_t)(S32(CUR(), 0x58) >> 16);
	}

	// 0xB2D460 (helper): integrator flags bone+0x1A = (any velRot/accelRot word +0x80/82/84
	// != 0 ? 1 : 0) | (any +0x86/88/8A != 0 ? 8 : 0). Returns 0.
	int32_t h_B2D460()
	{
		uint8_t *b = CUR();
		uint8_t f = 0;
		if (U16(b, 0x80) != 0) f = 1;
		if (U16(b, 0x82) != 0) f |= 1;
		if (U16(b, 0x84) != 0) f |= 1;
		if (U16(b, 0x86) != 0) f |= 8;
		if (U16(b, 0x88) != 0) f |= 8;
		if (U16(b, 0x8A) != 0) f |= 8;
		U8(b, 0x1A) = f;
		return 0;
	}

	void fill_vm_b(Generic &g)
	{
		using namespace part_vm_b;
		g.vm[0x02D] = op_02D_SetCameraFlag;             // 0xB2BC60
		g.vm[0x045] = op_045_EntityFlagBits;            // 0xB2BC80
		g.vm[0x049] = op_049_JumpIfEntitySize;          // 0xB2BD10
		g.vm[0x06D] = op_06D_PartyRecordFlag;           // 0xB2BD70
		g.vm[0x01C] = op_01C_PokeBoneField;             // 0xB2BDC0
		g.vm[0x01B] = op_01B_Nop4;                      // 0xB2BE10
		g.vm[0x077] = op_077_Nop6;                      // 0xB2BE20
		g.vm[0x079] = op_079_CompareFieldJump;          // 0xB2BE30
		g.vm[0x07D] = op_07D_SetBoneCount;              // 0xB2BE90
		g.vm[0x08A] = op_08A_BattleScreenFx;            // 0xB2BEF0
		g.vm[0x08D] = op_08D_ModelFlagsSetClear;        // 0xB2BFA0
		g.vm[0x005] = op_005_FlagOp;                    // 0xB2C030
		g.vm[0x10C] = op_10C_BoneFlagOps;               // 0xB2C100
		g.vm[0x10D] = op_10D_WaitOtherBoneFlags;        // 0xB2C1D0
		g.vm[0x0D7] = op_0D7_Nop4;                      // 0xB2C240
		g.vm[0x0E7] = op_0E7_WaitUntilFieldGreater;     // 0xB2C250
		g.vm[0x0FB] = op_0FB_WaitUntilFieldLess;        // 0xB2C290
		g.vm[0x106] = op_106_WaitUntilFieldLessThanBone;// 0xB2C2D0
		g.vm[0x0FD] = op_0FD_JumpIfFieldInRange;        // 0xB2C320
		g.vm[0x074] = op_074_DrawFlagBits;              // 0xB2C360
		g.vm[0x0C1] = op_0C1_Nop;                       // 0xB2C3B0
		g.vm[0x04C] = op_04C_SetBoneHandler;            // 0xB2C3D0
		g.vm[0x037] = op_037_ResetAccum;                // 0xB2C400
		for (int i = 0x0A; i <= 0x17; i++)
			g.vm[i] = op_00A_GenericWrite;              // 0xB2C480
		g.vm[0x05F] = op_05F_PosFromEntityOffset;       // 0xB2C780
		g.vm[0x072] = op_072_RotFromEntity;             // 0xB2C820
		g.vm[0x05E] = op_05E_PosFromSlotHome;           // 0xB2C870
		g.vm[0x02F] = op_02F_StopMotion;                // 0xB2C8D0
		g.vm[0x082] = op_082_VelocityTowardBone;        // 0xB2C900
		g.vm[0x0C0] = op_0C0_VelPosTowardSpawner;       // 0xB2C970
		g.vm[0x048] = op_048_OverrideEntityTransform;   // 0xB2C9E0
		g.vm[0x0B9] = op_0B9_NegateField;               // 0xB2CA80
		g.vm[0x0BA] = op_0BA_NegateFieldNeg;            // 0xB2CAD0
		g.vm[0x0D2] = op_0D2_NegateMotionComponent;     // 0xB2CB20
		g.vm[0x0CC] = op_0CC_SetBoneHandler7;           // 0xB2CB70
		g.vm[0x0F2] = op_0F2_BakeOutPosToAccum;         // 0xB2CBD0
		g.vm[0x110] = op_110_PlaceAtMidpoint;           // 0xB2CC20
		g.vm[0x061] = op_061_CopyAccumOrVelFromBone;    // 0xB2CCC0
		g.vm[0x062] = op_061_CopyAccumOrVelFromBone;    // 0xB2CCC0
		g.vm[0x091] = op_091_AccumFromBoneOutputs;      // 0xB2CD50
		g.vm[0x060] = op_060_PosFromEntityJoint;        // 0xB2CE00
		g.vm[0x06E] = op_06E_PosFromTargetsJointAvg;    // 0xB2CF70
		g.vm[0x01A] = op_01A_SetBoneHandlerParams;      // 0xB2D070
		g.vm[0x075] = op_075_AccumFromEntitySize;       // 0xB2D0B0
		g.vm[0x076] = op_076_AccumAddRandom;            // 0xB2D170
		g.vm[0x07C] = op_07C_RandomOffsetXZ;            // 0xB2D210
		g.vm[0x087] = op_087_VelocityScaleAccum;        // 0xB2D2B0
	}
}
}

// ============================================================================================
// part vm_c
// ============================================================================================
// GF cinematic engine, part vm_c: VM opcodes of Ifrit 0xB2D340..0xB2E7CE (channel control,
// bone spawning, bone handler setup, sprite setup, draw options, colours, textures, VRAM uploads).


namespace ff8fx
{
namespace gfc
{
namespace part_vm_c
{
	// --- engine-block globals used here (shared by all clones, inside STATE_LO..STATE_HI) ---
	inline uint8_t &LOADBUSY() { return MEM<uint8_t>(0x2798219); }   // byte_2798219: file load busy (set 0xFF by 0xB2BA10)
	static const uint32_t CLUTBUF = 0x279822C;                         // engine CLUT/palette buffer (u16 entries)
	// --- battle globals (outside the snapshot) ---
	inline uint32_t &MODELPALETTE() { return MEM<uint32_t>(0xB8B7D8); } // CURRENT_BS_MODEL_PALETTE base colour (read only)
	static const uint32_t FADELAYERS = 0x1D9898C;                      // stru_1D9898C: 4 colour/fade layers of 0x2C bytes

	// sub_5088A0 renders a battle entity model into the render list: OT insertion + packets,
	// outside the snapshot -> skipped while predicting (returns the cursor unchanged).
	static uint32_t xl_Render_5088A0(void *a, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		if (g_predict) return cursor;
		return x::Render_5088A0(a, ot, mode, cursor);
	}

	// ------------------------------------------------------------------------------------
	// helpers
	// ------------------------------------------------------------------------------------

	// 0xB2D8A0 (helper): reset channel ws+0xF0 of the current bone: wait counter (+0xC + 2ch) = 0,
	// return-stack depth (+0x44 + ch) = 0. Returns 0.
	static int32_t ResetChannel_B2D8A0()
	{
		int32_t ch = S32(WS(), 0xF0);
		U16(CUR(), ch * 2 + 0xC) = 0;
		U8(CUR(), ch + 0x44) = 0;
		return 0;
	}

	// 0xB2D610 (VM 0x032 SpawnBone, also called by 0x135/0x05D/0x09C): if the bone order list has
	// room (ORDER[scene+0x38 - 1] == 0xFF), take the first free bone (chan0 == 0), append it to the
	// order list, chan0 = opcode + w0, ++scene+0x18, InitBone, copy accumPos/accumRot/handler/...
	// from the current bone, bone id = scene+0 (post-incremented), run its bone handler and
	// B2C440 with it as the current bone; ws+0xFC = new bone. STREAM += 4.
	// caller_ecx: vanilla quirk (0xB2D613 `push ecx` / 0xB2D79A): when the order list is full,
	// ws+0xFC = the never-written local [ebp-4] = the caller's ecx at entry.
	static void SpawnBone_B2D610(uint32_t caller_ecx)
	{
		uint32_t n = U16(SCENE(), 0x38);
		if (MEM<uint8_t>(0x2797453 + n) != 0xFF) // ORDER()[n - 1]
		{
			U32(WS(), 0xFC) = caller_ecx;
			STREAM() += 4;
			return;
		}
		// end of the order list
		uint32_t pos = 0;
		if (ORDER()[0] != 0xFF)
		{
			uint8_t v;
			do
			{
				v = ORDER()[pos + 1];
				pos++;
			} while (v != 0xFF);
		}
		// first free bone
		uint8_t *bone = PTR(CTX(), 0x90);
		int32_t id = 0;
		if (U32(bone, 0) != 0)
		{
			uint32_t next;
			do
			{
				next = U32(bone, 0x100);
				bone += 0x100;
				id++;
			} while (next != 0);
		}
		ORDER()[pos] = (uint8_t)id; // overwrites the terminator (no new 0xFF is written)
		uint8_t *st = STREAM();
		PTR(bone, 0) = st + S16(st, 2);
		U16(SCENE(), 0x18) = (uint16_t)(U16(SCENE(), 0x18) + 1);
		blob::InitBone(id);
		U32(bone, 0x5C) = U32(CUR(), 0x5C);
		U32(bone, 0x60) = U32(CUR(), 0x60);
		U32(bone, 0x64) = U32(CUR(), 0x64);
		U32(bone, 0x50) = U32(CUR(), 0x50);
		U32(bone, 0x54) = U32(CUR(), 0x54);
		U32(bone, 0x58) = U32(CUR(), 0x58);
		U8(bone, 0x18) = U8(CUR(), 0x18);
		U16(bone, 0x48) = U16(CUR(), 0x48);
		U8(bone, 0x1B) = U8(CUR(), 0x1B);
		{
			uint8_t *src = CUR() + 0xB0;
			uint8_t *dst = bone + 0xB0;
			GFC_TAINT(dst, src, 8);
			for (int i = 2; i != 0; i--)
			{
				U32(dst, 0) = U32(src, 0);
				src += 4;
				dst += 4;
			}
		}
		U16(bone, 0x14) = U16(CUR(), 0x12);
		U16(bone, 0x16) = U16(CUR(), 0x14);
		U16(bone, 0x12) = U16(SCENE(), 0);
		U16(SCENE(), 0) = (uint16_t)(U16(SCENE(), 0) + 1);
		uint8_t *saved = CUR();
		CUR() = bone;
		C().bone[U8(bone, 0x18)]();
		h_B2C440();
		CUR() = saved;
		PTR(WS(), 0xFC) = bone;
		STREAM() += 4;
	}

	// 0xB2E700 (helper, isGF_Ifrit_AltTextureLoader; also the tail of VM 0x029): read alternative
	// texture w0 (ws+0xF0 rect / ws+0xFC data) and upload it (budgeted). Success: STREAM += 4,
	// returns 0. Over budget: rt+0x3E = bone+0xC8 (wait), returns the non-zero result.
	static int32_t AltTextureLoader_B2E700()
	{
		blob::ReadAltTexture(S16(STREAM(), 2));
		int32_t r = blob::AltTextureUpload(PTR(WS(), 0xF0), PTR(WS(), 0xFC));
		if (r == 0)
		{
			STREAM() += 4;
			return r;
		}
		U16(RT(), 0x3E) = U16(CUR(), 0xC8);
		return r;
	}

	// 0xB2E750 (helper, body of VM 0x028 UploadImage): if a load is busy -> wait (rt+0x3E =
	// bone+0xC8); else resolve image w0 (B66560: ws+0xF0 rect, ws+0xFC data) and upload it
	// (budgeted): success STREAM += 4 and 0, over budget -> wait and the non-zero result.
	// caller_ecx: vanilla quirk (0xB2E753 `push ecx` / 0xB2E7C8): the busy path returns the
	// never-written local [ebp-4] = the caller's ecx at entry.
	static int32_t UploadImage_B2E750(uint32_t caller_ecx)
	{
		if (LOADBUSY() == 0)
		{
			blob::ReadClut_B66560(S16(STREAM(), 2));
			int32_t r = blob::AltTextureUpload(PTR(WS(), 0xF0), PTR(WS(), 0xFC));
			if (r == 0)
			{
				STREAM() += 4;
				return r;
			}
			U16(RT(), 0x3E) = U16(CUR(), 0xC8);
			return r;
		}
		U16(RT(), 0x3E) = U16(CUR(), 0xC8);
		return (int32_t)caller_ecx;
	}

	// 0xB2DA00 (VM 0x036 SetSprite, helper of 0x0C7/0x073/0x081/0x025): sprite setup w0 (B66270),
	// orientation bone+0x1E = 1. STREAM += 4.
	static void __cdecl op_036_SetSprite()
	{
		blob::SetSpriteAnim(S16(STREAM(), 2));
		U8(CUR(), 0x1E) = 1;
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// VM opcodes
	// ------------------------------------------------------------------------------------

	// 0xB2D340 (VM 0x0AC AccelerateToTarget): frames n = w0; for each of the 6 components selected by
	// op bits 15..10 (rotX,rotY,rotZ,posX,posY,posZ order of bone+0x50..): acceleration bone+0x80+2i =
	// (target - pos - vel*n) / (n(n+1)) (fixed-point). Bit 9: targets are immediate words (<<16)
	// after w0, else the targets are the accum values (+0x50+4i) of bone ref w1. Then B2D460.
	static void __cdecl op_0AC_AccelerateToTarget()
	{
		uint8_t *st = STREAM();
		int32_t imm = U16(RT(), 0x4A) & 0x200;
		int32_t n = S16(st, 2);
		int32_t div = shl32(mul32(n + 1, n), 1) / 2;
		uint8_t *tgt = nullptr;
		if (imm != 0)
		{
			STREAM() = st + 4;
		}
		else
		{
			tgt = blob::GetBone(S16(st, 4));
			STREAM() = STREAM() + 6;
		}
		uint8_t *cur = CUR();
		uint8_t *acc = cur + 0x80;
		uint8_t *vel = cur + 0x68;
		if (imm == 0) tgt += 0x50; // imm: vanilla keeps the (never dereferenced) value 0x250 here
		uint32_t mask = U16(RT(), 0x4A);
		for (int k = 6; k != 0; k--)
		{
			if (mask & 0x8000)
			{
				int32_t v;
				if (imm != 0)
				{
					int32_t e = mul32(S32(vel, 0), n);
					int32_t a = sub32(shl32(S16(STREAM(), 0), 16), e);
					a = a / div;
					STREAM() += 2;
					v = a >> 7;
				}
				else
				{
					int32_t e = S32(vel, 0) >> 4;
					int32_t a = S32(tgt, 0) >> 4;
					e = mul32(e, n);
					a = sub32(a, e);
					e = S32(vel, -0x18) >> 4; // accum (+0x50+4i)
					a = sub32(a, e);
					a = a / div;
					v = a >> 3;
				}
				v = v >> 4;
				U16(acc, 0) = (uint16_t)v;
			}
			acc += 2;
			if (imm == 0) tgt += 4;
			vel += 4;
			mask <<= 1;
		}
		h_B2D460();
	}

	// 0xB2D4C0 (VM 0x026 StartChannel): channel = w0, or if 0 the first free of chan[1..3]
	// (none free -> STREAM += 6, nothing); chan[ch] = opcode + w1 (negative ptr phase),
	// B2D8A0 reset. STREAM += 6. No bound check on w0 (vanilla).
	static void __cdecl op_026_StartChannel()
	{
		uint8_t *st = STREAM();
		uint8_t *cur = CUR();
		int32_t ch = S16(st, 2);
		if (ch == 0)
		{
			ch = 1;
			uint8_t *p = cur + 4;
			while (U32(p, 0) != 0)
			{
				ch++;
				p += 4;
				if (ch >= 4)
				{
					STREAM() = st + 6;
					return;
				}
			}
		}
		PTR(cur, ch * 4) = st + S16(st, 4);
		S32(WS(), 0xF0) = ch;
		ResetChannel_B2D8A0();
		STREAM() += 6;
	}

	// 0xB2D520 (VM 0x08B StartChannelPos): as 0x026 but chan[ch] = (opcode + w1) & 0x7FFFFFFF.
	static void __cdecl op_08B_StartChannelPos()
	{
		uint8_t *st = STREAM();
		uint8_t *cur = CUR();
		int32_t ch = S16(st, 2);
		if (ch == 0)
		{
			ch = 1;
			uint8_t *p = cur + 4;
			while (U32(p, 0) != 0)
			{
				ch++;
				p += 4;
				if (ch >= 4)
				{
					STREAM() = st + 6;
					return;
				}
			}
		}
		U32(cur, ch * 4) = ((uint32_t)st + (uint32_t)(int32_t)S16(st, 4)) & 0x7FFFFFFF;
		S32(WS(), 0xF0) = ch;
		ResetChannel_B2D8A0();
		STREAM() += 6;
	}

	// 0xB2D590 (VM 0x02A StopChannels): op >> 14 = channel (1..3): chan[ch] = 0; 0: chan[2] =
	// chan[1] = 0. STREAM += 2.
	static void __cdecl op_02A_StopChannels()
	{
		uint32_t ch = (uint32_t)U16(RT(), 0x4A) >> 14;
		if (ch != 0)
		{
			U32(CUR(), ch * 4) = 0;
			STREAM() += 2;
			return;
		}
		U32(CUR(), 8) = 0;
		U32(CUR(), 4) = 0;
		STREAM() += 2;
	}

	// 0xB2D5E0 (VM 0x02E SetBoneIdShort): bone id +0x12 = w0; STREAM += 2 only (vanilla: w0 is then
	// executed as the next opcode).
	static void __cdecl op_02E_SetBoneIdShort()
	{
		U16(CUR(), 0x12) = U16(STREAM(), 2);
		STREAM() += 2;
	}

	// 0xB2D610 (VM 0x032 SpawnBone): see SpawnBone_B2D610. The VM dispatchers (0xB2FDAC/0xB2FDD0,
	// 0xB301BF/0xB301E3) call it with ecx = RT.
	static void __cdecl op_032_SpawnBone()
	{
		SpawnBone_B2D610((uint32_t)RT());
	}

	// 0xB2D7C0 (VM 0x135 SpawnBoneWithId): SpawnBone, then new bone id (+0x12) = w1.
	// STREAM += 4 + 2.
	static void __cdecl op_135_SpawnBoneWithId()
	{
		SpawnBone_B2D610((uint32_t)RT()); // ecx at entry = dispatcher's RT
		uint8_t *st = STREAM();
		U16(PTR(WS(), 0xFC), 0x12) = U16(st, 0);
		STREAM() += 2;
	}

	// 0xB2D7F0 (VM 0x05D SpawnPerTarget): for each target i < scene+0x41 (s8): SpawnBone (same
	// program word, STREAM rewound by 4 after each), clone slot id +0x1B = scene+0x48+i.
	// STREAM += 4.
	static void __cdecl op_05D_SpawnPerTarget()
	{
		int32_t i = 0;
		int32_t n = S8(SCENE(), 0x41);
		if (n <= 0)
		{
			STREAM() += 4;
			return;
		}
		// vanilla ecx at each SpawnBone entry: RT (dispatcher) the first time, then
		// (WS & 0xFFFFFF00) | slot byte of the previous iteration (0xB2D80C/0xB2D81D)
		uint32_t ecx = (uint32_t)RT();
		uint8_t *st;
		do
		{
			SpawnBone_B2D610(ecx);
			uint8_t *ws = WS();
			uint8_t *scene = SCENE();
			uint8_t slot = U8(scene, i + 0x48);
			ecx = ((uint32_t)ws & 0xFFFFFF00) | slot;
			U8(PTR(ws, 0xFC), 0x1B) = slot;
			st = STREAM() - 4;
			i++;
			n--;
			STREAM() = st;
		} while (n != 0);
		STREAM() = st + 4;
	}

	// 0xB2D860 (VM 0x09C SpawnBoneChained): SpawnBone, then new+0x14 = (new-0x100)+0x12,
	// new+0x16 = (new-0x100)+0x14 (the previous bone SLOT's id / parent id).
	static void __cdecl op_09C_SpawnBoneChained()
	{
		SpawnBone_B2D610((uint32_t)RT()); // ecx at entry = dispatcher's RT
		uint8_t *b = PTR(WS(), 0xFC);
		U16(b, 0x14) = U16(b, -0xEE);
		U16(b, 0x16) = U16(b, -0xEC);
	}

	// 0xB2D8D0 (VM 0x05A SetBoneHandlerParam): handler +0x18 = (u8)w0, +0xB0 = w1, run the bone
	// handler. STREAM += 6.
	static void __cdecl op_05A_SetBoneHandlerParam()
	{
		U8(CUR(), 0x18) = U8(STREAM(), 2);
		U16(CUR(), 0xB0) = U16(STREAM(), 4);
		C().bone[U8(CUR(), 0x18)]();
		STREAM() += 6;
	}

	// 0xB2D920 (VM 0x0A4 SetBoneHandler): handler +0x18 = (u8)w0, +0xB0 = ref bone w1; handler 3:
	// polar around the ref bone: +0x64 = angle(B66B30) << 16, +0x5C = XZ distance << 16,
	// +0x60 = (cur.outY - ref.outY) << 16. Runs the bone handler. STREAM += 6.
	static void __cdecl op_0A4_SetBoneHandler()
	{
		int32_t h = S16(STREAM(), 2);
		U8(CUR(), 0x18) = (uint8_t)h;
		int32_t ref = S16(STREAM(), 4);
		U16(CUR(), 0xB0) = (uint16_t)ref;
		uint8_t *rb = blob::GetBone(ref);
		if (h == 3)
		{
			uint8_t *cur = CUR();
			int32_t r = blob::Blob_B66B30(S16(rb, 0x94), S16(rb, 0x98), S16(cur, 0x94), S16(cur, 0x98));
			S32(CUR(), 0x64) = shl32(r, 16);
			S32(CUR(), 0x5C) = shl32(S32(WS(), 0xFC), 16);
			cur = CUR();
			S32(cur, 0x60) = shl32(sub32(S16(cur, 0x96), S16(rb, 0x96)), 16);
		}
		C().bone[U8(CUR(), 0x18)]();
		STREAM() += 6;
	}

	// 0xB2DA30 (VM 0x0C7 InitSpriteAnimH22): SetSprite, then draw handler +0x1C = 0x16.
	static void __cdecl op_0C7_InitSpriteAnimH22()
	{
		op_036_SetSprite();
		U8(CUR(), 0x1C) = 0x16;
	}

	// 0xB2DA40 (VM 0x073 SetSpriteOrient2): SetSprite, orientation +0x1E = 2, +0xB8 = (s32)w1.
	// STREAM += 4 + 2.
	static void __cdecl op_073_SetSpriteOrient2()
	{
		op_036_SetSprite();
		U8(CUR(), 0x1E) = 2;
		S32(CUR(), 0xB8) = S16(STREAM(), 0);
		STREAM() += 2;
	}

	// 0xB2DA80 (VM 0x081 SetSpriteAnimOriented): SetSprite, orientation +0x1E = op >> 12.
	static void __cdecl op_081_SetSpriteAnimOriented()
	{
		op_036_SetSprite();
		U8(CUR(), 0x1E) = (uint8_t)(U16(RT(), 0x4A) >> 12);
	}

	// 0xB2DAA0 (VM 0x025 SetSpriteOriented): SetSprite, orientation +0x1E = 9.
	static void __cdecl op_025_SetSpriteOriented()
	{
		op_036_SetSprite();
		U8(CUR(), 0x1E) = 9;
	}

	// 0xB2DAB0 (VM 0x09B ClearOptionBits): +0xDE &= ~((op >> 9) & 0x7F). STREAM += 2.
	static void __cdecl op_09B_ClearOptionBits()
	{
		uint32_t w = U16(RT(), 0x4A);
		uint8_t *cur = CUR();
		uint8_t m = (uint8_t)((w >> 9) & 0x7F);
		U8(cur, 0xDE) = (uint8_t)(U8(cur, 0xDE) & (uint8_t)~m);
		STREAM() += 2;
	}

	// 0xB2DAF0 (VM 0x024 SetOptionBits): v = op >> 9; (v & 3) ? +0xDE = v : +0xDE |= v. STREAM += 2.
	static void __cdecl op_024_SetOptionBits()
	{
		uint32_t v = (uint32_t)U16(RT(), 0x4A) >> 9;
		if (v & 3)
		{
			U8(CUR(), 0xDE) = (uint8_t)v;
			STREAM() += 2;
			return;
		}
		uint8_t *cur = CUR();
		U8(cur, 0xDE) = (uint8_t)(U8(cur, 0xDE) | (uint8_t)v);
		STREAM() += 2;
	}

	// 0xB2DB40 (VM 0x051 SetOptionBitsAndC0): +0xDE |= op >> 9; +0xC0 = w0. STREAM += 4.
	static void __cdecl op_051_SetOptionBitsAndC0()
	{
		uint16_t w = U16(RT(), 0x4A);
		uint8_t *cur = CUR();
		U8(cur, 0xDE) = (uint8_t)(U8(cur, 0xDE) | (uint8_t)(uint16_t)(w >> 9));
		U16(CUR(), 0xC0) = U16(STREAM(), 2);
		STREAM() += 4;
	}

	// 0xB2DB90 (VM 0x06B SetFlagCA): +0xCA |= 0x8000. STREAM += 2.
	static void __cdecl op_06B_SetFlagCA()
	{
		U16(CUR(), 0xCA) |= 0x8000;
		STREAM() += 2;
	}

	// 0xB2DBB0 (VM 0x06C ClearFlagCA): +0xCA &= 0x7FFF. STREAM += 2.
	static void __cdecl op_06C_ClearFlagCA()
	{
		U16(CUR(), 0xCA) &= 0x7FFF;
		STREAM() += 2;
	}

	// 0xB2DBD0 (VM 0x08E RemoveFromDrawList): DrawListRemove(rt+0x42 = current bone). STREAM += 2.
	static void __cdecl op_08E_RemoveFromDrawList()
	{
		blob::DrawListRemove(U8(RT(), 0x42));
		STREAM() += 2;
	}

	// 0xB2DC00 (VM 0x0D0 RemoveFromDrawList, byte clone of 0x08E).
	static void __cdecl op_0D0_RemoveFromDrawList()
	{
		blob::DrawListRemove(U8(RT(), 0x42));
		STREAM() += 2;
	}

	// 0xB2DC30 (VM 0x0EB AddToDrawList): DrawListAddId(rt+0x42). STREAM += 2.
	static void __cdecl op_0EB_AddToDrawList()
	{
		blob::DrawListAddId(U8(RT(), 0x42));
		STREAM() += 2;
	}

	// 0xB2DC60 (VM 0x0BC SetSpriteFrameSpeed): +0xCA = w0. STREAM += 4.
	static void __cdecl op_0BC_SetSpriteFrameSpeed()
	{
		U16(CUR(), 0xCA) = U16(STREAM(), 2);
		STREAM() += 4;
	}

	// 0xB2DCC0 (VM 0x071 DrawEntityInCinematic): entity model block e = scene+0x60[bone+0x1B];
	// op bit 15: e flags &= ~0x20; else e flags |= 0x20 and draw it (sub_5088A0(e, rt+0x4C, 4,
	// ctx+0x7C) -> ctx+0x7C). STREAM += 2.
	static void __cdecl op_071_DrawEntityInCinematic()
	{
		uint8_t *cur = CUR();
		uint8_t *scene = SCENE();
		uint32_t slot = U8(cur, 0x1B);
		bool clear = (U8(RT(), 0x4B) & 0x80) != 0;
		uint8_t *e = PTR(scene, slot * 4 + 0x60);
		if (clear)
		{
			guard(e, 2);
			U16(e, 0) &= 0xFFDF;
			STREAM() += 2;
			return;
		}
		guard(e, 1);
		U8(e, 0) |= 0x20;
		uint32_t r = xl_Render_5088A0(e, U32(RT(), 0x4C), 4, U32(CTX(), 0x7C));
		U32(CTX(), 0x7C) = r;
		STREAM() += 2;
	}

	// 0xB2DD30 (VM 0x040 ColourFromBoneOutPos): colour +0xCC low 24 bits = clamp(src.outPos X/Y/Z,
	// 0..255) of bone ref w0; top byte kept. STREAM += 4.
	static void __cdecl op_040_ColourFromBoneOutPos()
	{
		uint8_t *b = blob::GetBone(S16(STREAM(), 2));
		int32_t r = S16(b, 0x94);
		if (r > 0xFF) r = 0xFF; else if (r < 0) r = 0;
		int32_t g = S16(b, 0x96);
		if (g > 0xFF) g = 0xFF; else if (g < 0) g = 0;
		int32_t bl = S16(b, 0x98);
		if (bl > 0xFF) bl = 0xFF; else if (bl < 0) bl = 0;
		uint32_t c = (uint32_t)r | (((uint32_t)bl << 8 | (uint32_t)g) << 8);
		uint8_t *cur = CUR();
		U32(cur, 0xCC) = (U32(cur, 0xCC) & 0xFF000000) | c;
		STREAM() += 4;
	}

	// 0xB2DDD0 (VM 0x0D4 SetColor): op bit 15: grey w0 (4 bytes) else w0,w1,w2 (8 bytes);
	// +0xCC = (+0xCC & 0xFF000000) | (r | g << 8 | b << 16) with the words sign-extended (vanilla:
	// negative / >255 words spill into the higher bytes).
	static void __cdecl op_0D4_SetColor()
	{
		bool grey = (U8(RT(), 0x4B) & 0x80) != 0;
		uint8_t *st = STREAM();
		uint32_t r = (uint32_t)(int32_t)S16(st, 2);
		uint32_t g, b;
		if (grey)
		{
			g = (uint32_t)shl32((int32_t)r, 8);
			b = (uint32_t)shl32((int32_t)r, 16);
			st += 4;
		}
		else
		{
			g = (uint32_t)shl32(S16(st, 4), 8);
			b = (uint32_t)shl32(S16(st, 6), 16);
			st += 8;
		}
		b |= g;
		STREAM() = st;
		r |= b;
		uint8_t *cur = CUR();
		U32(cur, 0xCC) = (U32(cur, 0xCC) & 0xFF000000) | r;
	}

	// 0xB2DE30 (VM 0x0D3 ColorFromBoneOutPos): colour RGB = clamp(src.outPos XYZ * src.outAngleX
	// / 256, 0..255) of bone ref w0; top byte kept. STREAM += 4.
	static void __cdecl op_0D3_ColorFromBoneOutPos()
	{
		uint8_t *s = blob::GetBone(S16(STREAM(), 2));
		int32_t k = S16(s, 0x8C);
		int32_t r = (S16(s, 0x94) * k) / 256;
		int32_t g = (S16(s, 0x96) * k) / 256;
		int32_t b = (S16(s, 0x98) * k) / 256;
		if (r > 0xFF) r = 0xFF; else if (r < 0) r = 0;
		if (g > 0xFF) g = 0xFF; else if (g < 0) g = 0;
		if (b > 0xFF) b = 0xFF; else if (b < 0) b = 0;
		uint32_t c = (uint32_t)r | (((uint32_t)b << 8 | (uint32_t)g) << 8);
		uint8_t *cur = CUR();
		U32(cur, 0xCC) = (U32(cur, 0xCC) & 0xFF000000) | c;
		STREAM() += 4;
	}

	// 0xB2DF10 (VM 0x083 ColorFromOutAngle): colour RGB = clamp(outAngle X/Y/Z, 0..255); top byte
	// kept. STREAM += 2.
	static void __cdecl op_083_ColorFromOutAngle()
	{
		uint8_t *cur = CUR();
		int32_t r = S16(cur, 0x8C);
		if (r > 0xFF) r = 0xFF; else if (r < 0) r = 0;
		int32_t g = S16(cur, 0x8E);
		if (g > 0xFF) g = 0xFF; else if (g < 0) g = 0;
		int32_t b = S16(cur, 0x90);
		if (b > 0xFF) b = 0xFF; else if (b < 0) b = 0;
		uint32_t c = (uint32_t)r | (((uint32_t)b << 8 | (uint32_t)g) << 8);
		U32(cur, 0xCC) = (U32(cur, 0xCC) & 0xFF000000) | c;
		STREAM() += 2;
	}

	// 0xB2DFA0 (VM 0x023 SetBlendMode): v = op >> 9; v <= 3: +0xCC |= 0x02000000, +0x92 =
	// (+0x92 & 0x19F) | v << 5; else +0xCC &= ~0x02000000, +0x92 = (+0x92 & ~0x40) | 0x20.
	// STREAM += 2.
	static void __cdecl op_023_SetBlendMode()
	{
		int32_t v = (int32_t)((uint32_t)U16(RT(), 0x4A) >> 9);
		if (v <= 3)
		{
			U32(CUR(), 0xCC) |= 0x2000000;
			uint8_t *cur = CUR();
			U16(cur, 0x92) = (uint16_t)((U16(cur, 0x92) & 0x19F) | (v << 5));
			STREAM() += 2;
			return;
		}
		U32(CUR(), 0xCC) &= 0xFDFFFFFF;
		uint8_t *cur = CUR();
		U16(cur, 0x92) = (uint16_t)((U16(cur, 0x92) & 0xFFBF) | 0x20);
		STREAM() += 2;
	}

	// 0xB2E040 (VM 0x078 SetTexPage): +0x9A = scene+0x1C + ((w0 & 0x1F0) << 2) + (w0 & 0xF).
	// STREAM += 4.
	static void __cdecl op_078_SetTexPage()
	{
		int32_t w = S16(STREAM(), 2);
		uint16_t v = (uint16_t)(((w & 0x1F0) << 2) + U16(SCENE(), 0x1C));
		v = (uint16_t)(v + (w & 0xF));
		U16(CUR(), 0x9A) = v;
		STREAM() += 4;
	}

	// 0xB2E080 (VM 0x04B TintPaletteUpload): w0 lo = CLUT res (B66560 -> ws+0xFC src), hi = dest
	// bank: ws+0xF0 = src, ws+0xF8 = CLUTBUF + (w0 >> 8) * 0x200; ws+0xFC = first colour w1:
	// ws+0xF0/0xF8 += (w1 & 0xFF) * 2. op >> 12: 0 = 256 colours tinted by own outAngle (6 bytes),
	// 2 = w2 colours by own outAngle (8 bytes), 1 = w2 colours by outAngle of bone ref w3 (10 bytes).
	// B666F0 tints + uploads (skipped while predicting).
	static void __cdecl op_04B_TintPaletteUpload()
	{
		int32_t w0 = S16(STREAM(), 2);
		blob::ReadClut_B66560(w0 & 0xFF);
		U32(WS(), 0xF0) = U32(WS(), 0xFC);
		U32(WS(), 0xF8) = (uint32_t)shl32(w0 >> 8, 9) + CLUTBUF;
		S32(WS(), 0xFC) = S16(STREAM(), 4);
		uint8_t *ws = WS();
		uint32_t d = (U32(ws, 0xFC) & 0xFF) << 1;
		U32(ws, 0xF0) = U32(ws, 0xF0) + d;
		ws = WS();
		U32(ws, 0xF8) = U32(ws, 0xF8) + d;
		uint32_t mode = (uint32_t)U16(RT(), 0x4A) >> 12;
		if (mode == 1)
		{
			S32(WS(), 0xF4) = S16(STREAM(), 6);
			uint8_t *b = blob::GetBone(S16(STREAM(), 8));
			blob::ClutTint_B666F0(S16(b, 0x8C), S16(b, 0x8E), S16(b, 0x90));
			STREAM() += 0xA;
			return;
		}
		if (mode == 2)
		{
			S32(WS(), 0xF4) = S16(STREAM(), 6);
			uint8_t *cur = CUR();
			blob::ClutTint_B666F0(S16(cur, 0x8C), S16(cur, 0x8E), S16(cur, 0x90));
			STREAM() += 8;
			return;
		}
		S32(WS(), 0xF4) = 0x100;
		uint8_t *cur = CUR();
		blob::ClutTint_B666F0(S16(cur, 0x8C), S16(cur, 0x8E), S16(cur, 0x90));
		STREAM() += 6;
	}

	// 0xB2E230 (VM 0x038 SetTargetModelColor): src = (op & 0xFE00) ? outAngle (+0x8C) : outPos
	// (+0x94); ws+0xF0/F4/F8 = palette byte 0/1/2 (0xB8B7D8) + src XYZ - 0x80; B66A90 clamps and
	// packs -> ws+0xFC; entity model block (scene+0x60[bone+0x1B]) +0x28 = ws+0xFC. STREAM += 2.
	static void __cdecl op_038_SetTargetModelColor()
	{
		uint32_t pal = MODELPALETTE();
		uint8_t *src;
		if (U16(RT(), 0x4A) & 0xFFFFFE00)
			src = CUR() + 0x8C;
		else
			src = CUR() + 0x94;
		S32(WS(), 0xF0) = add32((int32_t)(pal & 0xFF), S16(src, 0) - 0x80);
		S32(WS(), 0xF4) = add32((int32_t)((pal >> 8) & 0xFF), S16(src, 2) - 0x80);
		S32(WS(), 0xF8) = add32((int32_t)((pal >> 16) & 0xFF), S16(src, 4) - 0x80);
		blob::Blob_B66A90();
		uint32_t slot = U8(CUR(), 0x1B);
		uint8_t *e = PTR(SCENE(), slot * 4 + 0x60);
		uint32_t c = U32(WS(), 0xFC);
		guard(e + 0x28, 4);
		U32(e, 0x28) = c;
		STREAM() += 2;
	}

	// 0xB2E2F0 (VM 0x020 SetFadeLayers): ws+0xF0/F4/F8 = outPos XYZ, B66A90 -> packed colour
	// ws+0xFC; intensity = clamp(outAngleX << 4, 0..0x1000); the 4 fade layers (0x1D9898C, 0x2C
	// each): word +6 = intensity, dword +0x2C = colour. STREAM += 2.
	static void __cdecl op_020_SetFadeLayers()
	{
		S32(WS(), 0xF0) = S16(CUR(), 0x94);
		S32(WS(), 0xF4) = S16(CUR(), 0x96);
		S32(WS(), 0xF8) = S16(CUR(), 0x98);
		blob::Blob_B66A90();
		int32_t k = shl32(S16(CUR(), 0x8C), 4);
		uint32_t c = U32(WS(), 0xFC);
		if (k < 0) k = 0;
		else if (k > 0x1000) k = 0x1000;
		uint32_t p = FADELAYERS + 0x2C; // 0x1D989B8
		for (int i = 4; i != 0; i--)
		{
			guard((void *)(p - 0x26), 2);
			MEM<uint16_t>(p - 0x26) = (uint16_t)k;
			guard((void *)p, 4);
			MEM<uint32_t>(p) = c;
			p += 0x2C;
		}
		STREAM() += 2;
	}

	// 0xB2E3A0 (VM 0x0DF SetupDrawHandler29): +0xB8 = 2*w0, +0xBC = 2*w1, +0xC0 = CLUTBUF + 2*w2,
	// +0xC4 = w3; draw handler 0x1D (+ draw list). STREAM += 10.
	static void __cdecl op_0DF_SetupDrawHandler29()
	{
		S32(CUR(), 0xB8) = shl32(S16(STREAM(), 2), 1);
		S32(CUR(), 0xBC) = shl32(S16(STREAM(), 4), 1);
		int32_t w2 = S16(STREAM(), 6);
		U32(CUR(), 0xC0) = (uint32_t)shl32(w2, 1) + CLUTBUF;
		S32(CUR(), 0xC4) = S16(STREAM(), 8);
		blob::SetDrawHandler(0x1D);
		STREAM() += 0xA;
	}

	// 0xB2E430 (VM 0x127 Nop6): STREAM += 6.
	static void __cdecl op_127_Nop6()
	{
		STREAM() += 6;
	}

	// 0xB2E440 (VM 0x029 VramUpload): load busy (0x2798219) -> wait (rt+0x3E = bone+0xC8).
	// op bit 15: rect ctx+0x98[w0], data ctx+0xB4 (4 bytes); bit 14: rect ctx+0x98[w0], data =
	// resfile[w1 & 0x7F] + w2 * 0x1000 (8 bytes); bit 13: inline rect w0..w3, data ctx+0xB4
	// (10 bytes); else tail-jumps to the alternative texture loader B2E700.
	static void __cdecl op_029_VramUpload()
	{
		if (LOADBUSY() != 0)
		{
			U16(RT(), 0x3E) = U16(CUR(), 0xC8);
			return;
		}
		uint16_t w = U16(RT(), 0x4A);
		if (w & 0x8000)
		{
			uint8_t *ctx = CTX();
			uint8_t *st = STREAM();
			uint32_t data = U32(ctx, 0xB4);
			uint32_t rect = U32(ctx, 0x98) + (uint32_t)shl32(S16(st, 2), 3);
			x::QueueVramUpload((const void *)rect, (const void *)data);
			STREAM() += 4;
			return;
		}
		if (w & 0x4000)
		{
			uint8_t *st = STREAM();
			uint32_t f = U8(st, 4) & 0x7F;
			uint32_t data = (uint32_t)RESFILE()[f] + (uint32_t)shl32(S16(st, 6), 12);
			uint32_t rect = U32(CTX(), 0x98) + (uint32_t)shl32(S16(st, 2), 3);
			x::QueueVramUpload((const void *)rect, (const void *)data);
			STREAM() += 8;
			return;
		}
		if (w & 0x2000)
		{
			uint8_t *ctx = CTX();
			uint8_t *rect = STREAM() + 2;
			x::QueueVramUpload(rect, PTR(ctx, 0xB4));
			STREAM() += 0xA;
			return;
		}
		AltTextureLoader_B2E700();
	}

	// 0xB2E530 (VM 0x028 UploadImage): jmp 0xB2E750 (dispatcher ecx = RT; result discarded).
	static void __cdecl op_028_UploadImage()
	{
		UploadImage_B2E750((uint32_t)RT());
	}

	// 0xB2E540 (VM 0x027 UploadAltTexAndImage): load busy -> wait; else B2E700 (alt texture w0,
	// STREAM += 4 on success), rewind STREAM by 4, B2E750 (image w0); any failure -> wait
	// (rt+0x3E = bone+0xC8).
	static void __cdecl op_027_UploadAltTexAndImage()
	{
		if (LOADBUSY() == 0)
		{
			if (AltTextureLoader_B2E700() == 0)
			{
				STREAM() -= 4;
				// the busy flag is 0 here, so B2E750's busy path (which returns the caller's ecx,
				// here whatever B2E700's callee left in ecx) is unreachable
				if (UploadImage_B2E750(0) == 0) return;
			}
		}
		U16(RT(), 0x3E) = U16(CUR(), 0xC8);
	}

	// 0xB2E580 (VM 0x04F Nop8): STREAM += 8.
	static void __cdecl op_04F_Nop8()
	{
		STREAM() += 8;
	}

	// 0xB2E590 (VM 0x06F Nop6): STREAM += 6.
	static void __cdecl op_06F_Nop6()
	{
		STREAM() += 6;
	}

	// 0xB2E5A0 (VM 0x090 Nop10): STREAM += 10.
	static void __cdecl op_090_Nop10()
	{
		STREAM() += 0xA;
	}

	// 0xB2E5C0 (VM 0x052 SetTexture): w0 == -1: tpage +0x92 = uv +0x9A = clut +0x9E = 0; else
	// B665C0(w0) -> +0x92 = ws+0xFC, +0x9E = ws+0xF8, +0x9A = B66640(w0). STREAM += 4.
	static void __cdecl op_052_SetTexture()
	{
		int32_t t = S16(STREAM(), 2);
		if (t == -1)
		{
			U16(CUR(), 0x92) = 0;
			U16(CUR(), 0x9A) = 0;
			U16(CUR(), 0x9E) = 0;
			STREAM() += 4;
			return;
		}
		blob::TexInfo_B665C0(t);
		U16(CUR(), 0x92) = U16(WS(), 0xFC);
		U16(CUR(), 0x9E) = U16(WS(), 0xF8);
		int32_t r = blob::ClutWord_B66640(t);
		U16(CUR(), 0x9A) = (uint16_t)r;
		STREAM() += 4;
	}

	// 0xB2E670 (VM 0x092 SetField92And9A): +0x92 = w0, +0x9A = w1. STREAM += 6.
	static void __cdecl op_092_SetField92And9A()
	{
		U16(CUR(), 0x92) = U16(STREAM(), 2);
		U16(CUR(), 0x9A) = U16(STREAM(), 4);
		STREAM() += 6;
	}
}

	void fill_vm_c(Generic &g)
	{
		g.vm[0x0AC] = part_vm_c::op_0AC_AccelerateToTarget;   // 0xB2D340
		g.vm[0x026] = part_vm_c::op_026_StartChannel;         // 0xB2D4C0
		g.vm[0x08B] = part_vm_c::op_08B_StartChannelPos;      // 0xB2D520
		g.vm[0x02A] = part_vm_c::op_02A_StopChannels;         // 0xB2D590
		g.vm[0x02E] = part_vm_c::op_02E_SetBoneIdShort;       // 0xB2D5E0
		g.vm[0x032] = part_vm_c::op_032_SpawnBone;            // 0xB2D610
		g.vm[0x135] = part_vm_c::op_135_SpawnBoneWithId;      // 0xB2D7C0
		g.vm[0x05D] = part_vm_c::op_05D_SpawnPerTarget;       // 0xB2D7F0
		g.vm[0x09C] = part_vm_c::op_09C_SpawnBoneChained;     // 0xB2D860
		g.vm[0x05A] = part_vm_c::op_05A_SetBoneHandlerParam;  // 0xB2D8D0
		g.vm[0x0A4] = part_vm_c::op_0A4_SetBoneHandler;       // 0xB2D920
		g.vm[0x036] = part_vm_c::op_036_SetSprite;            // 0xB2DA00
		g.vm[0x0C7] = part_vm_c::op_0C7_InitSpriteAnimH22;    // 0xB2DA30
		g.vm[0x073] = part_vm_c::op_073_SetSpriteOrient2;     // 0xB2DA40
		g.vm[0x081] = part_vm_c::op_081_SetSpriteAnimOriented;// 0xB2DA80
		g.vm[0x025] = part_vm_c::op_025_SetSpriteOriented;    // 0xB2DAA0
		g.vm[0x09B] = part_vm_c::op_09B_ClearOptionBits;      // 0xB2DAB0
		g.vm[0x024] = part_vm_c::op_024_SetOptionBits;        // 0xB2DAF0
		g.vm[0x051] = part_vm_c::op_051_SetOptionBitsAndC0;   // 0xB2DB40
		g.vm[0x06B] = part_vm_c::op_06B_SetFlagCA;            // 0xB2DB90
		g.vm[0x06C] = part_vm_c::op_06C_ClearFlagCA;          // 0xB2DBB0
		g.vm[0x08E] = part_vm_c::op_08E_RemoveFromDrawList;   // 0xB2DBD0
		g.vm[0x0D0] = part_vm_c::op_0D0_RemoveFromDrawList;   // 0xB2DC00
		g.vm[0x0EB] = part_vm_c::op_0EB_AddToDrawList;        // 0xB2DC30
		g.vm[0x0BC] = part_vm_c::op_0BC_SetSpriteFrameSpeed;  // 0xB2DC60
		g.vm[0x071] = part_vm_c::op_071_DrawEntityInCinematic;// 0xB2DCC0
		g.vm[0x040] = part_vm_c::op_040_ColourFromBoneOutPos; // 0xB2DD30
		g.vm[0x0D4] = part_vm_c::op_0D4_SetColor;             // 0xB2DDD0
		g.vm[0x0D3] = part_vm_c::op_0D3_ColorFromBoneOutPos;  // 0xB2DE30
		g.vm[0x083] = part_vm_c::op_083_ColorFromOutAngle;    // 0xB2DF10
		g.vm[0x023] = part_vm_c::op_023_SetBlendMode;         // 0xB2DFA0
		g.vm[0x078] = part_vm_c::op_078_SetTexPage;           // 0xB2E040
		g.vm[0x04B] = part_vm_c::op_04B_TintPaletteUpload;    // 0xB2E080
		g.vm[0x038] = part_vm_c::op_038_SetTargetModelColor;  // 0xB2E230
		g.vm[0x020] = part_vm_c::op_020_SetFadeLayers;        // 0xB2E2F0
		g.vm[0x0DF] = part_vm_c::op_0DF_SetupDrawHandler29;   // 0xB2E3A0
		g.vm[0x127] = part_vm_c::op_127_Nop6;                 // 0xB2E430
		g.vm[0x029] = part_vm_c::op_029_VramUpload;           // 0xB2E440
		g.vm[0x028] = part_vm_c::op_028_UploadImage;          // 0xB2E530
		g.vm[0x027] = part_vm_c::op_027_UploadAltTexAndImage; // 0xB2E540
		g.vm[0x04F] = part_vm_c::op_04F_Nop8;                 // 0xB2E580
		g.vm[0x06F] = part_vm_c::op_06F_Nop6;                 // 0xB2E590
		g.vm[0x090] = part_vm_c::op_090_Nop10;                // 0xB2E5A0
		g.vm[0x052] = part_vm_c::op_052_SetTexture;           // 0xB2E5C0
		g.vm[0x092] = part_vm_c::op_092_SetField92And9A;      // 0xB2E670
	}
}
}

// ============================================================================================
// part vm_d
// ============================================================================================
// GF cinematic engine, part vm_d: node/matrix builders (camera nodes, aux billboard slots,
// light sets, model-joint attachment), mesh setters, flow/wait opcodes and the channel end /
// sequence loop opcodes. Ported from the Ifrit (201) copy, 0xB2E7D0..0xB30103.


namespace ff8fx
{
namespace gfc
{
namespace part_vm_d
{
	// --------------------------------------------------------------------------------------
	// shared engine block globals used only here (identical addresses in all clones)
	// --------------------------------------------------------------------------------------
	// g_GfCinematic_RootAngleXYZ: s16[3] at 0x2797780 (op 0x117)
	inline int16_t *ROOTANG() { return (int16_t *)0x2797780; }
	// aux / billboard-slot matrix table (32-byte FF8_PSX_Matrix4x3 each) at 0x2798B68 (ops 0x104/0x105)
	inline uint8_t *AUXMAT(int32_t slot) { return (uint8_t *)(0x2798B68u + (uint32_t)shl32(slot, 5)); }

	// guard a write that should land inside the engine block but whose index comes from data
	// (light slot, aux slot): only an out-of-range index reaches memory outside the snapshot
	static inline void guard_outside_block(const void *p, int n)
	{
		uint32_t a = (uint32_t)p;
		if (a < STATE_LO || a + (uint32_t)n > STATE_HI) guard(p, n);
	}

	// --------------------------------------------------------------------------------------
	// local wrappers
	// --------------------------------------------------------------------------------------
	// 0xB66B80 (blob): matrix of joint `joint` of the model whose BattleAnimHeader is `hdr`,
	// composed with m (ws+0xD0 = m o joint), joint length * scale >> 12 -> ws+0xC4, and the
	// joint origin transformed -> ws+0xF0..0xF8 (flag ws+0xFC). Returns ws+0xD0.
	// (blob::Blob_B66B80 discards the return value the caller needs.) Writes only ws + GTE.
	inline uint8_t *xl_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(0xB66B80)(hdr, joint, scale, m);
	}
	// GTE far colour (control regs 21..23) and DQA/DQB (27, 28) + the fog near/far globals
	// written by 0x45DDA0 / 0x56CCC0 / 0x56CCA0: persistent render state outside the snapshot
	static void guard_fog_state()
	{
		if (!g_predict) return;
		guard((void *)0x1CA92D0, 12);   // GTE ctrl RFC/GFC/BFC (0x1CA927C + 21*4)
		guard((void *)0x1CA92E8, 8);    // GTE ctrl DQA/DQB (0x1CA927C + 27*4)
		guard((void *)0x209AB64, 4);    // fog near (sub_56CCC0)
		guard((void *)0xC78BF0, 4);     // fog far (sub_56CCA0)
	}

	static void __cdecl op_000_EndChannel();

	// ======================================================================================
	// node builders (matrix node slot = blob::NodeForCurBone(), keyed by bone+0x12)
	// ======================================================================================

	// 0xB2E7D0 (VM 0x065 NodeCamRotFromPos): node.rot = Cam.rot * Rot(outPos read as angles),
	// node.trans = Cam.trans. Operands: w0 unused. Length 4.
	// VANILLA QUIRK 0xB2E80E: the "projection word = 0" store goes to node+0x48 (= m[1][1] of the
	// node two slots further, or engine block bytes after slot 63), not to node+0x12: node+0x12
	// keeps the pad word copied by 0x56C270 (x::MulMatrixGte) from the uninitialised stack temp of
	// Matrix3x3_MultiplyPSX (0x56C090 never writes temp+0x12) - not reproducible bit-exact.
	static void __cdecl op_065_NodeCamRotFromPos()
	{
		uint8_t *node = (uint8_t *)blob::NodeForCurBone();
		x::RotMatrixFromAngles(CUR() + 0x94, node);
		S32(node, 0x14) = MEM<int32_t>(0x27979FC);
		S32(node, 0x18) = MEM<int32_t>(0x2797A00);
		S32(node, 0x1C) = MEM<int32_t>(0x2797A04);
		x::MulMatrixGte(NODEMAT(), node);
		pad_after_rot_mul(node, CUR() + 0x94); // node H (+0x12): vanilla stack word = outPosY < 0 ? -1 : 0
		U16(node, 0x48) = 0;
		STREAM() += 4;
	}

	// 0xB2E830 (VM 0x066 NodeCamTranslate): node.rot = Cam.rot (+pad), node.trans = Cam * outPos,
	// node.H = ctx+0x02. Operands: w0 unused. Length 4.
	static void __cdecl op_066_NodeCamTranslate()
	{
		uint8_t *node = (uint8_t *)blob::NodeForCurBone();
		x::SetRotMatrix(NODEMAT());
		x::SetTransVector(NODEMAT());
		x::TransformToCamera(CUR() + 0x94, node + 0x14, WS() + 0xF0);
		uint8_t *cam = (uint8_t *)NODEMAT();
		U32(node, 0x00) = U32(cam, 0x00);
		U32(node, 0x04) = U32(cam, 0x04);
		U32(node, 0x08) = U32(cam, 0x08);
		U32(node, 0x0C) = U32(cam, 0x0C);
		U32(node, 0x10) = U32(cam, 0x10);
		U16(node, 0x12) = U16(CTX(), 2);
		STREAM() += 4;
	}

	// 0xB2E8C0 (VM 0x06A NodeCamRotLocalPos): node.rot = Cam.rot * RotOrder(outAngle, op>>12),
	// node.trans = raw outPos, node.H = ctx+0x02. Length 4.
	static void __cdecl op_06A_NodeCamRotLocalPos()
	{
		uint8_t *node = (uint8_t *)blob::NodeForCurBone();
		Mat4x3 *r = blob::RotMatrixOrder(CUR() + 0x8C, (int32_t)((uint32_t)U16(RT(), 0x4A) >> 12));
		x::MulMatrix3(NODEMAT(), r, node);
		S32(node, 0x14) = S16(CUR(), 0x94);
		S32(node, 0x18) = S16(CUR(), 0x96);
		S32(node, 0x1C) = S16(CUR(), 0x98);
		U16(node, 0x12) = U16(CTX(), 2);
		STREAM() += 4;
	}

	// 0xB2E950 (VM 0x067 NodeCamRotTranslate): node.trans = Cam * outPos, node.rot = Cam.rot *
	// RotOrder(outAngle, op>>12), node.H = 0. Length 4.
	static void __cdecl op_067_NodeCamRotTranslate()
	{
		uint8_t *node = (uint8_t *)blob::NodeForCurBone();
		x::SetRotMatrix(NODEMAT());
		x::SetTransVector(NODEMAT());
		x::TransformToCamera(CUR() + 0x94, node + 0x14, WS() + 0xF0);
		Mat4x3 *r = blob::RotMatrixOrder(CUR() + 0x8C, (int32_t)((uint32_t)U16(RT(), 0x4A) >> 12));
		x::MulMatrix3(NODEMAT(), r, node);
		U16(node, 0x12) = 0;
		STREAM() += 4;
	}

	// 0xB2E9E0 (VM 0x069 NodeChildOfParent): node.rot = (Rot(outAngle) * Cam.rot) * parent.rot
	// with parent = GetParentMatrix(w1) (also sets GTE H), node.trans = raw outPos, H = 0. Length 6.
	static void __cdecl op_069_NodeChildOfParent()
	{
		uint8_t *node = (uint8_t *)blob::NodeForCurBone();
		Mat4x3 *parent = blob::GetParentMatrix(S16(STREAM(), 4));
		x::RotMatrixFromAngles(CUR() + 0x8C, node);
		x::MulMatrixInPlace(node, NODEMAT());
		x::MulMatrixInPlace(node, parent);
		S32(node, 0x14) = S16(CUR(), 0x94);
		S32(node, 0x18) = S16(CUR(), 0x96);
		S32(node, 0x1C) = S16(CUR(), 0x98);
		U16(node, 0x12) = 0;
		STREAM() += 6;
	}

	// 0xB2EA70 (VM 0x0C4 NodeLocalNoCamera): node.rot(+pad) = RotOrder(outAngle, op>>12),
	// node.trans = raw outPos, node.H = ctx+0x02. Length 4.
	static void __cdecl op_0C4_NodeLocalNoCamera()
	{
		uint8_t *node = (uint8_t *)blob::NodeForCurBone();
		uint8_t *r = (uint8_t *)blob::RotMatrixOrder(CUR() + 0x8C, (int32_t)((uint32_t)U16(RT(), 0x4A) >> 12));
		U32(node, 0x00) = U32(r, 0x00);
		U32(node, 0x04) = U32(r, 0x04);
		U32(node, 0x08) = U32(r, 0x08);
		U32(node, 0x0C) = U32(r, 0x0C);
		U32(node, 0x10) = U32(r, 0x10);
		S32(node, 0x14) = S16(CUR(), 0x94);
		S32(node, 0x18) = S16(CUR(), 0x96);
		S32(node, 0x1C) = S16(CUR(), 0x98);
		U16(node, 0x12) = U16(CTX(), 2);
		STREAM() += 4;
	}

	// 0xB2EB10 (VM 0x116 NodeCamPosLocalRot): node.trans = Cam * outPos, node.rot = Rot(outAngle)
	// (not multiplied by the camera), node.H = 0. Length 4.
	static void __cdecl op_116_NodeCamPosLocalRot()
	{
		uint8_t *node = (uint8_t *)blob::NodeForCurBone();
		x::SetRotMatrix(NODEMAT());
		x::SetTransVector(NODEMAT());
		x::TransformToCamera(CUR() + 0x94, node + 0x14, WS() + 0xF0);
		x::RotMatrixFromAngles(CUR() + 0x8C, node);
		U16(node, 0x12) = 0;
		STREAM() += 4;
	}

	// 0xB2EB80 (VM 0x095 Nop4)
	static void __cdecl op_095_Nop4()
	{
		STREAM() += 4;
	}

	// 0xB2EB90 (VM 0x07A NodeFromParentAndMatrix): node = ComposeAffine(parent, M) with
	// parent = node slot of bone w0 (find-or-ALLOCATE; w0 == 0 -> Cam node 0), M = own node
	// (w1 == 0) or the 32 bytes at curBone + w1. node.H = (op bit 15) ? ctx+0x02 : 0. Length 6.
	static void __cdecl op_07A_NodeFromParentAndMatrix()
	{
		int32_t ref = S16(STREAM(), 2);
		uint8_t *other = blob::GetBone(ref);
		uint8_t *node = (uint8_t *)blob::NodeForCurBone();
		uint8_t *saved = CUR();
		CUR() = other;
		uint8_t *parent = (uint8_t *)NODEMAT();
		if (ref != 0) parent = (uint8_t *)blob::NodeForCurBone();
		CUR() = saved;
		int32_t ofs = S16(STREAM(), 4);
		uint8_t *m = (ofs == 0) ? node : saved + ofs;
		x::ComposeAffine(parent, m, node);
		if (U8(RT(), 0x4B) & 0x80)
			U16(node, 0x12) = U16(CTX(), 2);
		else
			U16(node, 0x12) = 0;
		STREAM() += 6;
	}

	// 0xB2EC40 (VM 0x084 CopyMatrix): 32 bytes from (bone w0's node slot if w1 == 0 [find-or-
	// allocate], else bone w0 + w1) to (own node slot if w2 == 0, else curBone + w2). Length 8.
	static void __cdecl op_084_CopyMatrix()
	{
		uint8_t *other = blob::GetBone(S16(STREAM(), 2));
		uint8_t *s = STREAM();
		uint8_t *saved = CUR();
		CUR() = other;
		int32_t ofs = S16(s, 4);
		uint8_t *src = (ofs == 0) ? (uint8_t *)blob::NodeForCurBone() : other + ofs;
		CUR() = saved;
		int32_t dofs = S16(STREAM(), 6);
		uint8_t *dst = (dofs == 0) ? (uint8_t *)blob::NodeForCurBone() : saved + dofs;
		GFC_TAINT(dst, src, 0x20);
		blob::CopyMatrix(dst, src);
		STREAM() += 8;
	}

	// 0xB2ECB0 (VM 0x068 SetParentNode): bone+0x9C = w0. Length 4.
	static void __cdecl op_068_SetParentNode()
	{
		U16(CUR(), 0x9C) = U16(STREAM(), 2);
		STREAM() += 4;
	}

	// 0xB2ECE0 (VM 0x0CB NodeLookAt): eye = bone w0 (0 = self), target = bone w1 (0 = self);
	// node.rot = look-at(eye.outPos -> target.outPos) (blob 0xB66E50), node.trans = outPos of
	// target (op bit 12) or eye, then node = Cam o node (ComposeAffine), H = 0. Length 6.
	static void __cdecl op_0CB_NodeLookAt()
	{
		int32_t a = S16(STREAM(), 2);
		uint8_t *eye = (a == 0) ? CUR() : blob::GetBone(a);
		int32_t b = S16(STREAM(), 4);
		uint8_t *tgt = (b == 0) ? CUR() : blob::GetBone(b);
		uint8_t *node = (uint8_t *)blob::NodeForCurBone();
		blob::LookAt_B66E50(eye + 0x94, tgt + 0x94, node);
		uint8_t *src = (U8(RT(), 0x4B) & 0x10) ? tgt : eye;
		S32(node, 0x14) = S16(src, 0x94);
		S32(node, 0x18) = S16(src, 0x96);
		S32(node, 0x1C) = S16(src, 0x98);
		x::ComposeAffine(NODEMAT(), node, node);
		U16(node, 0x12) = 0;
		STREAM() += 6;
	}

	// 0xB2EDB0 (VM 0x104 BuildBillboardSlot): aux slot w0 (0x2798B68 + 32*w0): rot = CamAlt.rot *
	// Rot(outAngle) (Rot built in ws+0x20), trans = CamAlt * outPos, H = 0. Length 4.
	static void __cdecl op_104_BuildBillboardSlot()
	{
		uint8_t *aux = AUXMAT(S16(STREAM(), 2));
		uint8_t *r = WS() + 0x20;
		guard_outside_block(aux, 32);
		x::RotMatrixFromAngles(CUR() + 0x8C, r);
		x::MulMatrix3(CAMALT(), r, aux);
		x::SetRotMatrix(CAMALT());
		x::SetTransVector(CAMALT());
		x::TransformToCamera(CUR() + 0x94, aux + 0x14, WS() + 0xF0);
		U16(aux, 0x12) = 0;
		STREAM() += 4;
	}

	// 0xB2EE40 (VM 0x105 NodeFromBillboardSlot): node.trans = aux[w0] * outPos, node.rot =
	// aux[w0].rot * RotOrder(outAngle, op>>12), H = 0. Length 6.
	static void __cdecl op_105_NodeFromBillboardSlot()
	{
		uint8_t *node = (uint8_t *)blob::NodeForCurBone();
		uint8_t *aux = AUXMAT(S16(STREAM(), 2));
		x::SetRotMatrix(aux);
		x::SetTransVector(aux);
		x::TransformToCamera(CUR() + 0x94, node + 0x14, WS() + 0xF0);
		Mat4x3 *r = blob::RotMatrixOrder(CUR() + 0x8C, (int32_t)((uint32_t)U16(RT(), 0x4A) >> 12));
		x::MulMatrix3(aux, r, node);
		U16(node, 0x12) = 0;
		STREAM() += 6;
	}

	// 0xB2EF00 (VM 0x117 SetRotRelativeToRoot): accumRot(+0x50/54/58) = (w_i - rootAngle_i) << 16,
	// then outAngle = HIWORD(accumRot) (0xB2C440). Length 8.
	static void __cdecl op_117_SetRotRelativeToRoot()
	{
		uint8_t *s = STREAM();
		int32_t rx = ROOTANG()[0];
		int32_t y = S16(s, 4);
		int32_t z = S16(s, 6);
		int32_t x0 = S16(s, 2);
		x0 = sub32(x0, rx);
		int32_t ry = ROOTANG()[1];
		int32_t rz = ROOTANG()[2];
		S32(CUR(), 0x50) = shl32(x0, 16);
		y = sub32(y, ry);
		z = sub32(z, rz);
		S32(CUR(), 0x54) = shl32(y, 16);
		S32(CUR(), 0x58) = shl32(z, 16);
		h_B2C440();
		STREAM() += 8;
	}

	// 0xB2EF70 (VM 0x063 PosFromBoneViaParent): src = bone w0, par = bone(src+0x9C);
	// accumPos(+0x5C/60/64) = (RotOrder(par.outAngle, w1) * src.outPos + par.outPos) << 16
	// (GTE MVMVA with TR = par.outPos), then the bone handler of the current bone. Length 6.
	static void __cdecl op_063_PosFromBoneViaParent()
	{
		uint8_t *saved = CUR();
		uint8_t *src = blob::GetBone(S16(STREAM(), 2));
		CUR() = src;
		uint8_t *par = blob::GetBone((int32_t)U16(src, 0x9C));
		Mat4x3 *r = blob::RotMatrixOrder(par + 0x8C, S16(STREAM(), 4));
		x::SetRotMatrix(r);
		int32_t px = S16(par, 0x94);
		int32_t pz = S16(par, 0x98);
		int32_t py = S16(par, 0x96);
		x::GteWriteCtrl(px, 5);
		x::GteWriteCtrl(py, 6);
		x::GteWriteCtrl(pz, 7);
		x::TransformToCamera(CUR() + 0x94, WS() + 0xF0, WS() + 0xFC);   // CUR() = src here
		CUR() = saved;
		S32(saved, 0x5C) = shl32(S32(WS(), 0xF0), 16);
		S32(CUR(), 0x60) = shl32(S32(WS(), 0xF4), 16);
		S32(CUR(), 0x64) = shl32(S32(WS(), 0xF8), 16);
		C().bone[U8(CUR(), 0x18)]();
		STREAM() += 6;
	}

	// 0xB2F090 (VM 0x05B AttachToModelJoint): attaches the current bone to a joint of a battle
	// model. op bit 10: model = battle entity E = SceneHeader[0x60 + 4*bone+0x1B] (anim header
	// E+0x60, base matrix E+0x40); else model = embedded model block *(bone w0 +0xBC) (header
	// block+0x10; none -> skip the opcode) with base matrix ws+0x60 = BBMAT[0].rot (+pad),
	// trans = own outPos, scaled by 16*outAngle of bone w0. op bit 11: joint = w1 & 0xFF,
	// node offset = (w1 >> 8) & 0xFF, scale = 16*w2, length 8; else joint = own outAngleX,
	// scale 0x1000, offset 0, length 4. Rebuilds the model's bone matrices, J = base o joint
	// (ws+0xD0), joint origin -> ws+0xF0. op>>12: 1 = own node = Cam o (J.rot, ws+0xF0), H = 0;
	// 2 = (own node or curBone+offset) = (J.rot, ws+0xF0); else accumPos = ws+0xF0 << 16 and
	// the bone handler runs. (Reads - does not write - the billboard matrices 0x2797968.)
	static void __cdecl op_05B_AttachToModelJoint()
	{
		uint8_t *hdr;              // [ebp-4]
		uint8_t *base;             // esi
		uint8_t *mbone = nullptr;  // eax (only read when base == 0)
		int32_t nodeofs;           // [ebp-8]
		int32_t joint, scale;      // edi, ebx
		if (U8(RT(), 0x4B) & 4)
		{
			uint8_t *e = PTR(SCENE(), 0x60 + U8(CUR(), 0x1B) * 4);
			// 0xB2F0B7 loads the uninitialised [ebp-8] into eax: dead value (eax is only read on
			// the base == 0 path, which this branch never takes)
			hdr = e + 0x60;
			base = e + 0x40;
		}
		else
		{
			mbone = blob::GetBone(S16(STREAM(), 2));
			uint8_t *blk = PTR(mbone, 0xBC);
			if (blk == nullptr)
			{
				if (U8(RT(), 0x4B) & 8)
					STREAM() += 8;
				else
					STREAM() += 4;
				return;
			}
			hdr = blk + 0x10;
			base = nullptr;
		}
		if (U8(RT(), 0x4B) & 8)
		{
			uint8_t *s = STREAM();
			int32_t w1 = S16(s, 4);
			scale = S16(s, 6);
			nodeofs = (int32_t)(((uint32_t)w1 & 0xFFFF) >> 8);
			joint = w1 & 0xFF;
			scale = shl32(scale, 4);
			STREAM() = s + 8;
		}
		else
		{
			uint8_t *c = CUR();
			uint8_t *s = STREAM();
			nodeofs = 0;
			joint = S16(c, 0x8C);
			scale = 0x1000;
			STREAM() = s + 4;
		}
		if (base == nullptr)
		{
			uint8_t *w = WS() + 0x60;
			U32(w, 0x00) = MEM<uint32_t>(0x2797968);
			U32(w, 0x04) = MEM<uint32_t>(0x279796C);
			U32(w, 0x08) = MEM<uint32_t>(0x2797970);
			U32(w, 0x0C) = MEM<uint32_t>(0x2797974);
			U32(w, 0x10) = MEM<uint32_t>(0x2797978);
			S32(w, 0x14) = S16(CUR(), 0x94);
			S32(w, 0x18) = S16(CUR(), 0x96);
			S32(w, 0x1C) = S16(CUR(), 0x98);
			S32(w, 0x20) = shl32(S16(mbone, 0x8C), 4);
			S32(w, 0x24) = shl32(S16(mbone, 0x8E), 4);
			S32(w, 0x28) = shl32(S16(mbone, 0x90), 4);
			x::ScaleMatrix(w, w + 0x20);
			base = w;
		}
		// writes the model skeleton (outside the snapshot): the header wrapper skips it while
		// predicting, B66B80 then reads the matrices of the last real build
		x::BuildBoneMatricesFromPose(hdr);
		uint8_t *jm = xl_B66B80(hdr, joint, scale, base);
		switch ((uint32_t)U16(RT(), 0x4A) >> 12)
		{
		case 1:
		{
			uint8_t *node = (uint8_t *)blob::NodeForCurBone();
			blob::CopyMatrix(node, jm);
			S32(node, 0x14) = S32(WS(), 0xF0);
			S32(node, 0x18) = S32(WS(), 0xF4);
			S32(node, 0x1C) = S32(WS(), 0xF8);
			x::ComposeAffine(NODEMAT(), node, node);
			U16(node, 0x12) = 0;
			return;
		}
		case 2:
		{
			uint8_t *node = (nodeofs == 0) ? (uint8_t *)blob::NodeForCurBone() : CUR() + nodeofs;
			blob::CopyMatrix(node, jm);
			// node+0x12 = pad of B66B80's result: the uninitialised stack temporary of
			// ComposeAffineTransform 0x56C2F0 (vanilla stack garbage, depends on the caller chain)
			GFC_TAINT(node + 0x12, nullptr, 2);
			S32(node, 0x14) = S32(WS(), 0xF0);
			S32(node, 0x18) = S32(WS(), 0xF4);
			S32(node, 0x1C) = S32(WS(), 0xF8);
			return;
		}
		default:
			S32(CUR(), 0x5C) = shl32(S32(WS(), 0xF0), 16);
			S32(CUR(), 0x60) = shl32(S32(WS(), 0xF4), 16);
			S32(CUR(), 0x64) = shl32(S32(WS(), 0xF8), 16);
			C().bone[U8(CUR(), 0x18)]();
			return;
		}
	}

	// ======================================================================================
	// mesh / draw-handler setters
	// ======================================================================================

	// 0xB2F380 (VM 0x054 SetMesh, also called by 0x03C / 0x056): colour(+0xCC) &= 0x2FFFFFF,
	// +0xBC = 0, mesh ptr(+0xD8) = ObjectPtr(w0), SetDrawHandler(1). Length 4.
	static void __cdecl op_054_SetMesh()
	{
		U32(CUR(), 0xCC) = U32(CUR(), 0xCC) & 0x2FFFFFF;
		U32(CUR(), 0xBC) = 0;
		uint8_t *obj = blob::ObjectPtr(S16(STREAM(), 2));
		PTR(CUR(), 0xD8) = obj;
		blob::SetDrawHandler(1);
		STREAM() += 4;
	}

	// 0xB2F3E0 (VM 0x03C SetMeshScaled): SetMesh, then drawHandlerId(+0x1C) = 2. Length 4.
	static void __cdecl op_03C_SetMeshScaled()
	{
		op_054_SetMesh();
		U8(CUR(), 0x1C) = 2;
	}

	// 0xB2F3F0 (VM 0x056 SetClippedMesh): SetMesh, drawHandlerId = 9, +0xC2 = w1. Length 6.
	static void __cdecl op_056_SetClippedMesh()
	{
		op_054_SetMesh();
		U8(CUR(), 0x1C) = 9;
		U16(CUR(), 0xC2) = U16(STREAM(), 0);   // STREAM already advanced by 4: this is w1
		STREAM() += 2;
	}

	// 0xB2F430 (VM 0x03D SetMeshTexParam): colour = (colour & 0x2000000) | 0x808080, mesh ptr =
	// ObjectPtr(w1), +0xB8 = 16*w2, +0xBC = 0, SetDrawHandler(w0). Length 8.
	static void __cdecl op_03D_SetMeshTexParam()
	{
		U32(CUR(), 0xCC) = (U32(CUR(), 0xCC) & 0x2000000) | 0x808080;
		uint8_t *obj = blob::ObjectPtr(S16(STREAM(), 4));
		PTR(CUR(), 0xD8) = obj;
		S32(CUR(), 0xB8) = shl32(S16(STREAM(), 6), 4);
		U32(CUR(), 0xBC) = 0;
		blob::SetDrawHandler(S16(STREAM(), 2));
		STREAM() += 8;
	}

	// 0xB2F4C0 (VM 0x04A SetMeshDrawHandler): colour &= 0x2FFFFFF, +0xBC = 0, mesh ptr =
	// ObjectPtr(w0), SetDrawHandler(op >> 9). Length 4.
	static void __cdecl op_04A_SetMeshDrawHandler()
	{
		U32(CUR(), 0xCC) = U32(CUR(), 0xCC) & 0x2FFFFFF;
		U32(CUR(), 0xBC) = 0;
		uint8_t *obj = blob::ObjectPtr(S16(STREAM(), 2));
		PTR(CUR(), 0xD8) = obj;
		blob::SetDrawHandler((int32_t)((uint32_t)U16(RT(), 0x4A) >> 9));
		STREAM() += 4;
	}

	// 0xB2F530 (VM 0x03E SetInlineDescriptor3): bone+0xBC = &w0 (3 inline words). Length 8.
	static void __cdecl op_03E_SetInlineDescriptor3()
	{
		PTR(CUR(), 0xBC) = STREAM() + 2;
		STREAM() += 8;
	}

	// ======================================================================================
	// lights / fog
	// ======================================================================================

	// 0xB2F590 (VM 0x093 BuildLightMatrices, IDA GF_Ifrit_BuildPoseMatrices is a misnomer):
	// light set L = 0x27977A4 + 0x50*w0. For i = 0..2 with bone B_i = w(i+1) (-1 = none):
	// row i of M (ws+0x20) = 16*B_i.outPos (zero row if none); column i of the colour matrix
	// L+0x20 (words +0x20+2i, +0x26+2i, +0x2C+2i) = 16*B_i.outAngle (left UNCHANGED if none).
	// L+0x00 rot(+pad) = M * Rot(16 * low byte of curBone.outPos X/Y/Z) (angles in ws+0x50);
	// L+0x40..0x48 (s32) = 16*curBone.outAngle (back colour). Length 10.
	static void __cdecl op_093_BuildLightMatrices()
	{
		uint8_t *s = STREAM();
		int32_t slot = S16(s, 2);
		uint8_t *L = (uint8_t *)(0x27977A4u + (uint32_t)mul32(slot, 0x50));
		uint8_t *m = WS() + 0x20;
		guard_outside_block(L, 0x4C);
		int32_t id = S16(s, 4);
		if (id != -1)
		{
			uint8_t *b = blob::GetBone(id);
			U16(m, 0x0) = (uint16_t)(U16(b, 0x94) << 4);
			U16(m, 0x2) = (uint16_t)(U16(b, 0x96) << 4);
			U16(m, 0x4) = (uint16_t)(U16(b, 0x98) << 4);
			U16(L, 0x20) = (uint16_t)(U16(b, 0x8C) << 4);
			U16(L, 0x26) = (uint16_t)(U16(b, 0x8E) << 4);
			U16(L, 0x2C) = (uint16_t)(U16(b, 0x90) << 4);
		}
		else
		{
			U16(m, 0x4) = 0;
			U16(m, 0x2) = 0;
			U16(m, 0x0) = 0;
		}
		id = S16(STREAM(), 6);
		if (id != -1)
		{
			uint8_t *b = blob::GetBone(id);
			U16(m, 0x6) = (uint16_t)(U16(b, 0x94) << 4);
			U16(m, 0x8) = (uint16_t)(U16(b, 0x96) << 4);
			U16(m, 0xA) = (uint16_t)(U16(b, 0x98) << 4);
			U16(L, 0x22) = (uint16_t)(U16(b, 0x8C) << 4);
			U16(L, 0x28) = (uint16_t)(U16(b, 0x8E) << 4);
			U16(L, 0x2E) = (uint16_t)(U16(b, 0x90) << 4);
		}
		else
		{
			U16(m, 0xA) = 0;
			U16(m, 0x8) = 0;
			U16(m, 0x6) = 0;
		}
		id = S16(STREAM(), 8);
		if (id != -1)
		{
			uint8_t *b = blob::GetBone(id);
			U16(m, 0xC) = (uint16_t)(U16(b, 0x94) << 4);
			U16(m, 0xE) = (uint16_t)(U16(b, 0x96) << 4);
			U16(m, 0x10) = (uint16_t)(U16(b, 0x98) << 4);
			U16(L, 0x24) = (uint16_t)(U16(b, 0x8C) << 4);
			U16(L, 0x2A) = (uint16_t)(U16(b, 0x8E) << 4);
			U16(L, 0x30) = (uint16_t)(U16(b, 0x90) << 4);
		}
		else
		{
			U16(m, 0x10) = 0;
			U16(m, 0xE) = 0;
			U16(m, 0xC) = 0;
		}
		uint8_t *ang = m + 0x30;
		U16(ang, 0) = (uint16_t)((uint32_t)U8(CUR(), 0x94) << 4);
		U16(m, 0x32) = (uint16_t)((uint32_t)U8(CUR(), 0x96) << 4);
		U16(m, 0x34) = (uint16_t)((uint32_t)U8(CUR(), 0x98) << 4);
		x::RotMatrixFromAngles(ang, L);
		x::MulMatrixGte(m, L);
		pad_after_rot_mul(L, ang); // L+0x12: vanilla stack word (always 0 here)
		S32(L, 0x40) = shl32(S16(CUR(), 0x8C), 4);
		S32(L, 0x44) = shl32(S16(CUR(), 0x8E), 4);
		S32(L, 0x48) = shl32(S16(CUR(), 0x90), 4);
		STREAM() += 10;
	}

	// 0xB2F7D0 (VM 0x0AA SetLightSlot): bone+0xE1 = (w0 == -1) ? 0 : (u8)w0. Length 4.
	static void __cdecl op_0AA_SetLightSlot()
	{
		int32_t v = S16(STREAM(), 2);
		if (v == -1)
		{
			U8(CUR(), 0xE1) = 0;
			STREAM() += 4;
			return;
		}
		U8(CUR(), 0xE1) = (uint8_t)v;
		STREAM() += 4;
	}

	// 0xB2F820 (VM 0x0C6 SetFogColorAndNear): GTE far colour = 16*outAngle (0x45DDA0), fog near =
	// outPosX, DQA/DQB recomputed with outPosY (0x56CCC0). Length 2.
	static void __cdecl op_0C6_SetFogColorAndNear()
	{
		guard_fog_state();
		uint8_t *c = CUR();
		int32_t b = S16(c, 0x90), g = S16(c, 0x8E), r = S16(c, 0x8C);
		x::GteSetBackColor3(r, g, b);
		c = CUR();
		int32_t y = S16(c, 0x96), xx = S16(c, 0x94);
		x::Fog_56CCC0(xx, y);
		STREAM() += 2;
	}

	// 0xB2F870 (VM 0x11B SetFogColorAndFar): as 0x0C6 but fog FAR = outPosX (0x56CCA0). Length 2.
	static void __cdecl op_11B_SetFogColorAndFar()
	{
		guard_fog_state();
		uint8_t *c = CUR();
		int32_t b = S16(c, 0x90), g = S16(c, 0x8E), r = S16(c, 0x8C);
		x::GteSetBackColor3(r, g, b);
		c = CUR();
		int32_t y = S16(c, 0x96), xx = S16(c, 0x94);
		x::Fog_56CCA0(xx, y);
		STREAM() += 2;
	}

	// 0xB2F8C0 (VM 0x0F1 Nop2)
	static void __cdecl op_0F1_Nop2()
	{
		STREAM() += 4;
	}

	// ======================================================================================
	// flow / sequence
	// ======================================================================================

	// 0xB2FB20 (VM 0x001 LoopSequenceOrFinish): if loop count ctx+0xD0 < ctx+0xD1: ++count,
	// InitBones, cursor = opcode + w0, rt+0x44 (boneCursor) = 0, vmWait = 0. Else finish: order
	// list[0..127] = 0xFF, SequenceState+8 = 0, channel pointers 0/4/8 = 0, vmWait = 0xFFFF,
	// SceneHeader+0x18 = 0, cursor = 0, SceneHeader+0x1A = 4.
	static void __cdecl op_001_LoopSequenceOrFinish()
	{
		uint8_t *ctx = CTX();
		uint8_t cnt = U8(ctx, 0xD0);
		if (cnt >= U8(ctx, 0xD1))
		{
			memset(ORDER(), 0xFF, 0x80);
			U32(SEQ(), 8) = 0;
			U32(CUR(), 8) = 0;
			U32(CUR(), 4) = 0;
			U32(CUR(), 0) = 0;
			U16(RT(), 0x3E) = 0xFFFF;
			U16(SCENE(), 0x18) = 0;
			STREAM() = nullptr;
			U16(SCENE(), 0x1A) = 4;
			return;
		}
		uint8_t *saved = CUR();
		uint8_t *s = STREAM();
		U8(ctx, 0xD0) = (uint8_t)(cnt + 1);
		h_B2F8F0();
		STREAM() = s;
		CUR() = saved;
		STREAM() = s + S16(s, 2);
		U8(RT(), 0x44) = 0;
		U16(RT(), 0x3E) = 0;
	}

	// 0xB2FBE0 (VM 0x0AE NextTargetOrJump): if ctx+0xD0 < ctx+0xD1: ++index, target setup
	// (0xB2FC20), cursor += 4; else cursor += w0.
	static void __cdecl op_0AE_NextTargetOrJump()
	{
		uint8_t *ctx = CTX();
		uint8_t cnt = U8(ctx, 0xD0);
		if (cnt >= U8(ctx, 0xD1))
		{
			uint8_t *s = STREAM();
			STREAM() = s + S16(s, 2);
			return;
		}
		U8(ctx, 0xD0) = (uint8_t)(cnt + 1);
		h_B2FC20();
		STREAM() += 4;
	}

	// 0xB2FE80 (VM 0x009 Wait): vmWaitRequest(rt+0x3E) = (op >> 9) << 7. Length 2.
	static void __cdecl op_009_Wait()
	{
		uint8_t *rt = RT();
		uint16_t v = (uint16_t)(U16(rt, 0x4A) >> 9);
		U16(rt, 0x3E) = (uint16_t)((uint32_t)v << 7);
		STREAM() += 2;
	}

	// 0xB2FEB0 (VM 0x000 EndChannel): channel != 0 (rt+0x43): cursor = 0, vmWait = 0x8000.
	// Channel 0: KILL the bone: channel pointers 0/4/8 and +0xC = 0, remove it from the order
	// list (frees its node key) and the draw list, --SceneHeader+0x18, vmWait = bone+0xC8,
	// cursor = 0, --rt+0x44, and clear the node key equal to bone+0x12 (again).
	static void __cdecl op_000_EndChannel()
	{
		uint8_t *rt = RT();
		if (U8(rt, 0x43) != 0)
		{
			STREAM() = nullptr;
			U16(rt, 0x3E) = 0x8000;
			return;
		}
		U32(CUR(), 0x0) = 0;
		U32(CUR(), 0x4) = 0;
		U32(CUR(), 0x8) = 0;
		U32(CUR(), 0xC) = 0;
		int32_t id = U8(RT(), 0x42);
		blob::OrderListRemove(id);
		blob::DrawListRemove(id);
		U16(SCENE(), 0x18) = (uint16_t)(U16(SCENE(), 0x18) - 1);
		U16(RT(), 0x3E) = U16(CUR(), 0xC8);
		STREAM() = nullptr;
		U8(RT(), 0x44) = (uint8_t)(U8(RT(), 0x44) - 1);
		uint32_t key = U16(CUR(), 0x12);
		uint16_t *k = NODEKEYS();
		for (int i = 0; i < 0x40; i++, k++)
		{
			if (key == (uint32_t)*k)
			{
				*k = 0;
				return;
			}
		}
	}

	// 0xB2FF70 (VM 0x01D WaitBoneGoneThenEnd): if bone w0 no longer exists (GetBone returns the
	// bone array base ctx+0x90) -> EndChannel (tail jump); else wait one tick (vmWait = 0x80)
	// and retry the same opcode (cursor not advanced).
	static void __cdecl op_01D_WaitBoneGoneThenEnd()
	{
		uint8_t *b = blob::GetBone(S16(STREAM(), 2));
		if (b == PTR(CTX(), 0x90))
		{
			op_000_EndChannel();
			return;
		}
		U16(RT(), 0x3E) = 0x80;
	}

	// 0xB2FFC0 (VM 0x09E JumpIfCtxFlag15Clear): (ctx+0x01 & 0x80) ? cursor += 4 : cursor += w0.
	static void __cdecl op_09E_JumpIfCtxFlag15Clear()
	{
		uint8_t f = U8(CTX(), 1);
		uint8_t *s = STREAM();
		if (f & 0x80)
			STREAM() = s + 4;
		else
			STREAM() = s + S16(s, 2);
	}

	// 0xB2FFF0 (VM 0x09F JumpIfCtxFlag15Set): (ctx+0x01 & 0x80) ? cursor += w0 : cursor += 4.
	static void __cdecl op_09F_JumpIfCtxFlag15Set()
	{
		uint8_t f = U8(CTX(), 1);
		uint8_t *s = STREAM();
		if (f & 0x80)
			STREAM() = s + S16(s, 2);
		else
			STREAM() = s + 4;
	}

	// 0xB30020 (VM 0x0A2 Nop4)
	static void __cdecl op_0A2_Nop4()
	{
		STREAM() += 4;
	}

	// 0xB30030 (VM 0x0A0 JumpIfCtxFlag13Clear): (ctx u16 +0 & 0x2000) ? cursor += 4 : cursor += w0.
	static void __cdecl op_0A0_JumpIfCtxFlag13Clear()
	{
		uint16_t f = (uint16_t)(U16(CTX(), 0) & 0x2000);
		uint8_t *s = STREAM();
		if (f != 0)
			STREAM() = s + 4;
		else
			STREAM() = s + S16(s, 2);
	}

	// 0xB30060 (VM 0x0B0 RandomWait): r = Rand74(w0) (1 CRT rand()), cursor += 4, v = r *
	// bone+0xC8 (s16, imul 32); if v != 0: vmWait = (u16)v.
	static void __cdecl op_0B0_RandomWait()
	{
		int32_t r = blob::Rand74(S16(STREAM(), 2));
		uint8_t *s = STREAM();
		int32_t v = mul32(S16(CUR(), 0xC8), r);
		STREAM() = s + 4;
		if (v != 0) U16(RT(), 0x3E) = (uint16_t)v;
	}

	// 0xB300A0 (VM 0x140 WaitScaledBySlot): vmWait = (u16)((u8 SceneHeader[0xCC + bone+0x1B] *
	// w0) << 7) (16-bit multiply). Length 4.
	static void __cdecl op_140_WaitScaledBySlot()
	{
		uint32_t idx = U8(CUR(), 0x1B);
		uint16_t f = U8(SCENE(), 0xCC + idx);
		uint16_t p = (uint16_t)(f * U16(STREAM(), 2));
		U16(RT(), 0x3E) = (uint16_t)((uint32_t)p << 7);
		STREAM() += 4;
	}

	// 0xB300E0 (VM 0x050 FreeArenaBlock): ws+0xF0 = bone+0xC4 (size), arena top ctx+0x74 -= size
	// (4-aligned, blob 0xB65710). Length 2.
	static void __cdecl op_050_FreeArenaBlock()
	{
		U32(WS(), 0xF0) = U32(CUR(), 0xC4);
		blob::ArenaFree_B65710();
		STREAM() += 2;
	}
}

	void fill_vm_d(Generic &g)
	{
		g.vm[0x065] = part_vm_d::op_065_NodeCamRotFromPos;       // 0xB2E7D0
		g.vm[0x066] = part_vm_d::op_066_NodeCamTranslate;        // 0xB2E830
		g.vm[0x06A] = part_vm_d::op_06A_NodeCamRotLocalPos;      // 0xB2E8C0
		g.vm[0x067] = part_vm_d::op_067_NodeCamRotTranslate;     // 0xB2E950
		g.vm[0x069] = part_vm_d::op_069_NodeChildOfParent;       // 0xB2E9E0
		g.vm[0x0C4] = part_vm_d::op_0C4_NodeLocalNoCamera;       // 0xB2EA70
		g.vm[0x116] = part_vm_d::op_116_NodeCamPosLocalRot;      // 0xB2EB10
		g.vm[0x095] = part_vm_d::op_095_Nop4;                    // 0xB2EB80
		g.vm[0x07A] = part_vm_d::op_07A_NodeFromParentAndMatrix; // 0xB2EB90
		g.vm[0x084] = part_vm_d::op_084_CopyMatrix;              // 0xB2EC40
		g.vm[0x068] = part_vm_d::op_068_SetParentNode;           // 0xB2ECB0
		g.vm[0x0CB] = part_vm_d::op_0CB_NodeLookAt;              // 0xB2ECE0
		g.vm[0x104] = part_vm_d::op_104_BuildBillboardSlot;      // 0xB2EDB0
		g.vm[0x105] = part_vm_d::op_105_NodeFromBillboardSlot;   // 0xB2EE40
		g.vm[0x117] = part_vm_d::op_117_SetRotRelativeToRoot;    // 0xB2EF00
		g.vm[0x063] = part_vm_d::op_063_PosFromBoneViaParent;    // 0xB2EF70
		g.vm[0x05B] = part_vm_d::op_05B_AttachToModelJoint;      // 0xB2F090
		g.vm[0x054] = part_vm_d::op_054_SetMesh;                 // 0xB2F380
		g.vm[0x03C] = part_vm_d::op_03C_SetMeshScaled;           // 0xB2F3E0
		g.vm[0x056] = part_vm_d::op_056_SetClippedMesh;          // 0xB2F3F0
		g.vm[0x03D] = part_vm_d::op_03D_SetMeshTexParam;         // 0xB2F430
		g.vm[0x04A] = part_vm_d::op_04A_SetMeshDrawHandler;      // 0xB2F4C0
		g.vm[0x03E] = part_vm_d::op_03E_SetInlineDescriptor3;    // 0xB2F530
		g.vm[0x093] = part_vm_d::op_093_BuildLightMatrices;      // 0xB2F590
		g.vm[0x0AA] = part_vm_d::op_0AA_SetLightSlot;            // 0xB2F7D0
		g.vm[0x0C6] = part_vm_d::op_0C6_SetFogColorAndNear;      // 0xB2F820
		g.vm[0x11B] = part_vm_d::op_11B_SetFogColorAndFar;       // 0xB2F870
		g.vm[0x0F1] = part_vm_d::op_0F1_Nop2;                    // 0xB2F8C0
		g.vm[0x001] = part_vm_d::op_001_LoopSequenceOrFinish;    // 0xB2FB20
		g.vm[0x0AE] = part_vm_d::op_0AE_NextTargetOrJump;        // 0xB2FBE0
		g.vm[0x009] = part_vm_d::op_009_Wait;                    // 0xB2FE80
		g.vm[0x000] = part_vm_d::op_000_EndChannel;              // 0xB2FEB0
		g.vm[0x01D] = part_vm_d::op_01D_WaitBoneGoneThenEnd;     // 0xB2FF70
		g.vm[0x09E] = part_vm_d::op_09E_JumpIfCtxFlag15Clear;    // 0xB2FFC0
		g.vm[0x09F] = part_vm_d::op_09F_JumpIfCtxFlag15Set;      // 0xB2FFF0
		g.vm[0x0A2] = part_vm_d::op_0A2_Nop4;                    // 0xB30020
		g.vm[0x0A0] = part_vm_d::op_0A0_JumpIfCtxFlag13Clear;    // 0xB30030
		g.vm[0x0B0] = part_vm_d::op_0B0_RandomWait;              // 0xB30060
		g.vm[0x140] = part_vm_d::op_140_WaitScaledBySlot;        // 0xB300A0
		g.vm[0x050] = part_vm_d::op_050_FreeArenaBlock;          // 0xB300E0
	}
}
}

// ============================================================================================
// part draw_mesh
// ============================================================================================
// GF cinematic engine, part "draw_mesh": the mesh / embedded-model / screen-tint / texture-scroll
// draw handlers and the generic mesh renderer DrawMeshObject (Ifrit 0xB266C0..0xB27E1C).


namespace ff8fx
{
namespace gfc
{
namespace part_draw_mesh
{
	// ------------------------------------------------------------------------------------
	// local wrappers / constants
	// ------------------------------------------------------------------------------------
	// 0x56C270 GTE_MatrixMultiply: TWO arguments, b.rot = a.rot * b.rot (in place, translation of
	// b untouched), returns b. (x::MulMatrixGte declares a third argument that is ignored.)
	static inline void *xl_MatrixMultiplyGte(const void *a, void *b) { return x::f<void *(__cdecl *)(const void *, void *)>(0x56C270)(a, b); }
	// 0x56C820 TransformCoordinateToCameraSpace: GTE_LoadV0(v); MVMVA(R*V0+TR); MAC1..3 -> mac[3];
	// FLAG -> *flag. (Same address as x::TransformToCamera, argument roles made explicit.)
	static inline void xl_TransformV0ToMac(const void *v, void *mac, void *flag) { x::f<void (__cdecl *)(const void *, void *, void *)>(0x56C820)(v, mac, flag); }

	// engine block: 8 zero dwords at 0x27971E4 (cleared by SetupSectionPtrs 0xB258D0), used as a
	// null V0 by SetupParentXformScaledAtOrigin
	static inline uint8_t *ZEROVEC() { return (uint8_t *)0x27971E4; }
	// software GTE control register 31 (FLAG) of the emulated GTE (ctrl file 0x1CA927C + 31*4)
	static inline uint32_t &GTEFLAG() { return MEM<uint32_t>(0x1CA92F8); }
	// the rdata float at Ifrit 0xB69540 is 4096.0f (the value is used directly, not its address)
	static const double DEPTH_SCALE_NUM = 4096.0;

	// ------------------------------------------------------------------------------------
	// 0xB266C0 (Draw 21 ScrollTextureUpload): scrolls a texture vertically inside its VRAM
	// rectangle: bone+0xB8 -> {s16 alt texture id, s16 vram x, s16 vram y}; bone+0x96 = scroll
	// offset (masked by height-1). Two VRAM uploads (the wrapped halves). Draws no primitive.
	// Held frames: returns at once (no VRAM upload).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_21_ScrollTextureUpload()
	{
		if (g_held.active) return;
		uint8_t *arg = PTR(CUR(), 0xB8);                  // esi
		blob::ReadAltTexture(S16(arg, 0));                // sets ws+0xF0..0xFC
		U32(WS(), 0x64) = U32(WS(), 0xFC);                // texel data
		uint8_t *w = WS();
		int32_t vx = S16(arg, 2);                         // edi
		uint8_t *tex = PTR(w, 0xF0);
		int32_t vy = S16(arg, 4);                         // esi
		S32(w, 0x78) = S16(tex, 4);                       // width (in VRAM halfwords)
		S32(WS(), 0x7C) = S16(tex, 6);                    // height
		S32(WS(), 0x70) = S16(CUR(), 0x96);               // scroll
		w = WS();
		S32(w, 0x70) = (int32_t)((uint32_t)S32(w, 0x70) & (uint32_t)sub32(S32(w, 0x7C), 1));
		uint8_t *rect = blob::VramRectRing();
		S16(rect, 0) = (int16_t)vx;
		S16(rect, 2) = (int16_t)vy;
		U16(rect, 4) = U16(WS(), 0x78);
		w = WS();
		U16(rect, 6) = (uint16_t)(U16(w, 0x7C) - U16(w, 0x70));
		w = WS();
		x::QueueVramUpload(rect, (const void *)(U32(w, 0x64) + (uint32_t)mul32(S32(w, 0x78), S32(w, 0x70)) * 2u));
		if (S32(WS(), 0x70) != 0)
		{
			rect = blob::VramRectRing();
			S16(rect, 0) = (int16_t)vx;
			w = WS();
			U16(rect, 2) = (uint16_t)((uint16_t)(U16(w, 0x7C) - U16(w, 0x70)) + (uint16_t)vy);
			U16(rect, 4) = U16(WS(), 0x78);
			U16(rect, 6) = U16(WS(), 0x70);
			x::QueueVramUpload(rect, (const void *)U32(WS(), 0x64));
		}
	}

	// ------------------------------------------------------------------------------------
	// 0xB27000 SetupParentXform: GTE R/TR = parent matrix (bone+0x9C), TR = parent * bone
	// position (bone+0x94). Returns 0 (unused).
	// ------------------------------------------------------------------------------------
	static void SetupParentXform()
	{
		Mat4x3 *m = blob::GetParentMatrix(U16(CUR(), 0x9C));
		x::GteSetRotMatrix(m);
		x::GteSetTransVector(m);
		x::GteLoadV0(CUR() + 0x94);
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
	}

	// ------------------------------------------------------------------------------------
	// 0xB27130 SetupParentXformScaledAtOrigin: like h_B269E0 but V0 = the zero vector
	// 0x27971E4 (TR = parent translation); GTE R = parent rotation scaled by bone+0x8C.. << 4
	// (ws+0xE0 = scaled copy, ws+0xD4 = scale). Returns 0 (unused).
	// ------------------------------------------------------------------------------------
	static void SetupParentXformScaledAtOrigin()
	{
		Mat4x3 *m = blob::GetParentMatrix(U16(CUR(), 0x9C));
		x::GteSetRotMatrix(m);
		x::GteSetTransVector(m);
		x::GteLoadV0(ZEROVEC());
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
		S32(WS(), 0xD4) = shl32(S16(CUR(), 0x8C), 4);
		S32(WS(), 0xD8) = shl32(S16(CUR(), 0x8E), 4);
		S32(WS(), 0xDC) = shl32(S16(CUR(), 0x90), 4);
		uint8_t *d = WS() + 0xE0;
		for (int i = 0; i < 8; i++) U32(d, i * 4) = U32(m, i * 4);
		x::ScaleMatrix(d, WS() + 0xD4);
		x::GteSetRotMatrix(d);
	}

	// ------------------------------------------------------------------------------------
	// 0xB27360 SetupBillboardXform(pos, angles, scale, order): ws+0xE0 = rotation from angles
	// (RotMatrixOrder), GTE R/TR = parent (bone+0x9C); ws+0xF4 = parent * pos (MAC, i.e. the
	// translation of ws+0xE0), ws+0xC0 = FLAG; ws+0xE0.rot = parent.rot * ws+0xE0.rot; scaled by
	// (scale, scale, scale) at ws+0xC0; GTE R/TR = ws+0xE0. Returns 0 (unused).
	// ------------------------------------------------------------------------------------
	static void SetupBillboardXform(const void *pos, const void *angles, int32_t scale, int32_t order)
	{
		blob::RotMatrixOrder(angles, order);
		Mat4x3 *m = blob::GetParentMatrix(U16(CUR(), 0x9C));   // esi
		x::SetRotMatrix(m);
		x::SetTransVector(m);
		uint8_t *w = WS();
		xl_TransformV0ToMac(pos, w + 0xF4, w + 0xC0);
		xl_MatrixMultiplyGte(m, WS() + 0xE0);
		S32(WS(), 0xC8) = scale;
		S32(WS(), 0xC4) = scale;
		S32(WS(), 0xC0) = scale;
		w = WS();
		x::ScaleMatrix(w + 0xE0, w + 0xC0);
		x::SetRotMatrix(WS() + 0xE0);
		x::SetTransVector(WS() + 0xE0);
	}

	// ------------------------------------------------------------------------------------
	// 0xB27440 DrawMeshObject(clip): the generic mesh renderer of the current bone.
	//   bone+0xD8 = mesh {+8 prim stream, +0x14 vertices, +0x18 vertex count, +0x1C normals,
	//   +0x20 normal count}; ws+0x4C = optional morph descriptor {s16 weight bone, s16 object A,
	//   s16 object B}; ws+0x40/44/48 = optional per-axis scale (4.12); clip = optional
	//   {s16 x, s16 y, s16 z, s16 flags}: flags&8 clamps vertex Y to <= y*8, else flags&4 to
	//   >= y*8. Transforms the vertices (SXY/depth into the arena copy, IR0 into ws+0x88, SZ3
	//   into 0x2798C18), optionally lights the normals (bone+0xE1), then runs the prim stream
	//   through C().prim (type + 20 * (bone+0x4C & 2) + 40 * (bone+0x4C & 4)).
	// ------------------------------------------------------------------------------------
	static void DrawMeshObject(const int16_t *clipArg)
	{
		int16_t local[4];                  // [ebp-0x34]
		const int16_t *clip = clipArg;     // [ebp-0x18]
		if (clipArg != nullptr)
		{
			local[0] = (int16_t)(uint16_t)((uint32_t)(uint16_t)clipArg[0] << 3);
			local[1] = (int16_t)(uint16_t)((uint32_t)(uint16_t)clipArg[1] << 3);
			local[2] = clipArg[2];
			local[3] = clipArg[3];
			clip = local;
		}
		uint32_t pkt = U32(CTX(), 0x7C);
		U32(WS(), 0x60) = pkt;                                                    // packet cursor
		U32(WS(), 0x50) = (uint32_t)(int32_t)S16(CUR(), 0x92) | 0xE1000200u;      // draw mode word
		S32(WS(), 0x54) = S16(CUR(), 0x9A);
		U32(WS(), 0x84) = U32(CUR(), 0xCC) & 0x2FFFFFFu;                          // colour / code
		U32(WS(), 0x8C) = U32(WS(), 0x84) & 0x2000000u;                           // semi-transparency bit
		U32(WS(), 0x5C) = U32(RT(), 0x4C);                                        // OT
		U32(WS(), 0x90) = U8(CUR(), 0xDE);                                        // depth mode (& 0x60)
		S32(WS(), 0x94) = S16(CUR(), 0xC0);                                       // depth bias / fixed depth
		U32(WS(), 0x98) = U16(CUR(), 0x9E);
		U32(WS(), 0x68) = U32(CUR(), 0xD8);                                       // mesh
		{
			uint8_t *mesh = PTR(WS(), 0x68);                                      // eax
			uint8_t *w = WS();
			U32(w, 0x6C) = U32(mesh, 0x08) + (uint32_t)mesh;                      // prim stream
			U32(WS(), 0x74) = U32(mesh, 0x14) + (uint32_t)mesh;                   // vertices
			U32(WS(), 0x78) = U32(mesh, 0x1C) + (uint32_t)mesh;                   // normals
			U32(WS(), 0x7C) = U32(CTX(), 0x74);                                   // arena: transformed vertices
			w = WS();
			U32(w, 0x80) = U32(w, 0x7C) + U32(mesh, 0x18) * 8u;                  // lit colours
			w = WS();
			U32(w, 0x88) = U32(w, 0x80) + U32(mesh, 0x20) * 8u;                  // IR0 per vertex
		}

		// --- morph (ws+0x4C) ---
		uint8_t *w = WS();                                                        // ecx
		uint8_t *morph = PTR(w, 0x4C);
		if (morph != nullptr)
		{
			uint8_t *wbone = blob::GetBone(S16(morph, 0));                        // ebx
			uint8_t *objA = blob::ObjectPtr(S16(morph, 2));                       // edi / [ebp-0x20]
			uint8_t *objB = blob::ObjectPtr(S16(morph, 4));                       // eax / [ebp-0x2c]
			S32(WS(), 0xE0) = S16(wbone, 0x94);                                   // weights (x/256)
			S32(WS(), 0xE4) = S16(wbone, 0x96);
			S32(WS(), 0xE8) = S16(wbone, 0x98);
			uint32_t fl = U32(objA, 0) & U32(objB, 0);                            // [ebp-0x28]
			if (fl & 2)
			{
				// vertex morph into the arena (do-while: runs once even with count <= 0, 0xB276B7)
				w = WS();
				U32(w, 0x74) = U32(w, 0x7C);
				U32(WS(), 0xF0) = U32(objA, 0x14) + (uint32_t)objA;
				U32(WS(), 0xF4) = U32(objB, 0x14) + (uint32_t)objB;
				S32(WS(), 0xF8) = S32(objA, 0x18);
				w = WS();                                                         // ecx kept in the loop
				int32_t n = S32(w, 0xF8);
				uint8_t *a = PTR(w, 0xF0), *b = PTR(w, 0xF4), *d = PTR(w, 0x7C);
				do
				{
					int32_t a0 = S16(a, 0);
					int32_t d0 = sub32(S16(b, 0), a0);
					int32_t p0 = mul32(S16(w, 0xE0), d0);
					int32_t k1 = S16(w, 0xE4);
					int32_t a1 = S16(a, 2);
					int32_t d1 = sub32(S16(b, 2), a1);
					S16(d, 0) = (int16_t)add32(p0 >> 8, a0);
					int32_t p1 = mul32(k1, d1);
					int32_t a2 = S16(a, 4);
					int32_t d2 = sub32(S16(b, 4), a2);
					int32_t k2 = S16(w, 0xE8);
					S16(d, 2) = (int16_t)add32(p1 >> 8, a1);
					int32_t p2 = mul32(k2, d2);
					n--;
					a += 8;
					b += 8;
					S16(d, 4) = (int16_t)add32(p2 >> 8, a2);
					d += 8;
				} while (n > 0);
			}
			if (fl & 4)
			{
				// normal morph into ws+0x80 (x/256 as C division here: cdq/and 0xFF/add/sar 8)
				w = WS();
				U32(w, 0x78) = U32(w, 0x80);
				uint8_t *a = objA + S32(objA, 0x1C);
				uint8_t *b = objB + S32(objB, 0x1C);
				uint8_t *d = PTR(WS(), 0x78);
				int32_t n = S32(objA, 0x20);
				if (n > 0)
				{
					do
					{
						w = WS();
						int16_t a0 = S16(a, 0);
						int32_t t = mul32(sub32(S16(b, 0), a0), S32(w, 0xE0));
						S16(d, 0) = (int16_t)add32(t / 256, a0);
						int16_t a1 = S16(a, 2);
						t = mul32(sub32(S16(b, 2), a1), S32(WS(), 0xE4));
						S16(d, 2) = (int16_t)add32(t / 256, a1);
						int16_t a2 = S16(a, 4);
						t = mul32(sub32(S16(b, 4), a2), S32(WS(), 0xE8));
						S16(d, 4) = (int16_t)add32(t / 256, a2);
						a += 8;
						b += 8;
						d += 8;
					} while (--n != 0);
				}
			}
			w = WS();
		}

		// --- Y clip (0xB27872) ---
		if (clip != nullptr)
		{
			int32_t cf = clip[3];
			if (cf & 0xC)
			{
				uint8_t *mesh = PTR(w, 0x68);
				int32_t lim = clip[1];
				int32_t n = S32(mesh, 0x18);
				uint8_t *d = PTR(w, 0x7C);
				uint8_t *s = PTR(w, 0x74);
				U32(w, 0x74) = U32(w, 0x7C);
				if (cf & 8)
				{
					while (n > 0)
					{
						int16_t y = S16(s, 2);
						if ((int32_t)y > lim) S16(d, 2) = (int16_t)lim;
						else S16(d, 2) = y;
						U16(d, 0) = U16(s, 0);
						s += 8;
						U16(d, 4) = U16(s, -4);
						d += 8;
						n--;
					}
				}
				else
				{
					while (n > 0)
					{
						int16_t y = S16(s, 2);
						if ((int32_t)y < lim) S16(d, 2) = (int16_t)lim;
						else S16(d, 2) = y;
						U16(d, 0) = U16(s, 0);
						s += 8;
						U16(d, 4) = U16(s, -4);
						d += 8;
						n--;
					}
				}
				w = WS();
			}
		}

		// --- vertex transform ---
		if ((U32(w, 0x48) | U32(w, 0x44) | U32(w, 0x40)) != 0)
		{
			// scaled (0xB2791D). fild/fdivr [4096.0f]/fstp float: a correctly rounded single for
			// any x87 precision control (double rounding is innocuous for a division 53 >= 2*24+2)
			guard(&FLT_1877DA8(), 4);
			FLT_1877DA8() = (float)(DEPTH_SCALE_NUM / (double)S32(w, 0xC0));
			uint32_t *depth = DEPTHARR();                     // ebx
			uint8_t *mesh = PTR(w, 0x68);
			uint8_t *dst = PTR(w, 0x7C);                      // edi
			uint8_t *v = PTR(w, 0x74);                        // esi
			int32_t n = S32(mesh, 0x18);                      // [ebp-8]
			int32_t sx = S32(w, 0x40);                        // [ebp-4] (reused as the FLAG local, see below)
			int32_t sy = S32(w, 0x44);                        // [ebp-0x14]
			int32_t sz = S32(w, 0x48);                        // [ebp-0x24]
			uint8_t *ir0 = PTR(w, 0x88);                      // [ebp-0xc]
			int32_t bias = S32(w, 0x94);                      // [ebp-0x18]
			int32_t mode = (int32_t)(U32(w, 0x90) & 0x60);    // [ebp-0x20]
			guard(depth, (n > 0 ? n : 1) * 4);
			do
			{
				int32_t vx = S16(v, 0);
				int32_t vy = S16(v, 2);
				int32_t lo = (mul32(vx, sx) >> 12) & 0xFFFF;
				int32_t hi = shl32(mul32(vy, sy) >> 12, 16);
				int32_t xy = lo | hi;
				x::GteWriteData(xy, 0);
				int32_t vz = S16(v, 4);
				int32_t z = mul32(vz, sz) >> 12;
				x::GteWriteData(z, 1);
				x::GteRTPS();
				n--;
				v += 8;
				x::GteReadData2(0xE, dst);                        // SXY2
				int32_t loc8;
				x::GteReadData(&loc8, 0x13);                      // SZ3
				*depth++ = (uint32_t)loc8;
				x::GteReadData2(8, ir0);                          // IR0
				// vanilla quirk (0xB27A2D): FLAG is read into [ebp-4], the stack slot holding the
				// X scale, and shifted there (0xB27A66): every vertex after the first is scaled in
				// X by (previous vertex's FLAG << 4) instead of ws+0x40.
				x::GteReadCtrl(&sx, 0x1F);
				if (mode == 0) loc8 = loc8 >> 2;
				else
				{
					int32_t q = loc8 >> 2;
					if (mode & 0x20) loc8 = bias;
					else loc8 = add32(q, bias);
				}
				sx = shl32(sx, 4);
				S32(dst, 4) = sx;
				U16(dst, 4) = (uint16_t)loc8;
				ir0 += 8;
				dst += 8;
			} while (n > 0);
		}
		else if (U8(CUR(), 0x4C) & 2)
		{
			// with screen clip codes (0xB27AA0); FLT_1877DA8 is NOT written on this path (the
			// prim renderers read the value left by the previous mesh)
			uint8_t *ir0 = PTR(w, 0x88);                      // [ebp-0xc]
			uint8_t *mesh = PTR(w, 0x68);
			uint8_t *dst = PTR(w, 0x7C);                      // edi
			uint8_t *v = PTR(w, 0x74);                        // ebx
			int32_t n = S32(mesh, 0x18);                      // [ebp-8]
			int32_t bias = S32(w, 0x94);                      // [ebp-0x18]
			int32_t mode = (int32_t)(U32(w, 0x90) & 0x60);    // [ebp-0x20]
			uint32_t *depth = DEPTHARR();                     // [ebp-0x1c]
			guard(depth, (n > 0 ? n : 1) * 4);
			do
			{
				int32_t loc8 = S32(v, 0);
				int32_t vz = U16(v, 4);
				x::GteWriteData(loc8, 0);
				x::GteWriteData(vz, 1);
				x::GteRTPS();
				n--;
				v += 8;
				x::GteReadData(&loc8, 0xE);                       // SXY2
				x::GteReadData2(8, ir0);                          // IR0
				S32(dst, 0) = loc8;
				uint32_t code = 0;                                // [ebp-4] (dead store in vanilla)
				if (GTEFLAG() & 0x20000) code = 0x10;
				int32_t scx = (int16_t)loc8;
				if (scx < 0) code |= 8;
				else if (scx >= 0xA00) code |= 2;
				int32_t scy = loc8 >> 16;
				loc8 = scy;
				if (scy < 0) code |= 4;
				else if (scy >= 0x700) code |= 1;
				U16(dst, 6) = (uint16_t)code;
				x::GteReadData(&loc8, 0x13);                      // SZ3
				*depth++ = (uint32_t)loc8;
				int32_t dz;
				if (mode == 0) dz = loc8 >> 2;
				else
				{
					dz = loc8 >> 2;
					if (mode & 0x20) dz = bias;
					else dz = add32(dz, bias);
				}
				U16(dst, 4) = (uint16_t)dz;
				dst += 8;
				ir0 += 8;
			} while (n > 0);
		}
		else
		{
			// plain (0xB27BCD)
			guard(&FLT_1877DA8(), 4);
			U32(&FLT_1877DA8(), 0) = 0x3F800000u;            // 1.0f
			uint8_t *ir0 = PTR(w, 0x88);                      // [ebp-0xc]
			uint8_t *mesh = PTR(w, 0x68);
			uint8_t *dst = PTR(w, 0x7C);                      // ebx
			uint8_t *v = PTR(w, 0x74);                        // edi
			int32_t n = S32(mesh, 0x18);                      // [ebp-8]
			int32_t bias = S32(w, 0x94);                      // [ebp-0x18]
			int32_t mode = (int32_t)(U32(w, 0x90) & 0x60);    // [ebp-0x20]
			uint32_t *depth = DEPTHARR();                     // [ebp-0x1c]
			guard(depth, (n > 0 ? n : 1) * 4);
			do
			{
				int32_t loc8 = S32(v, 0);
				int32_t vz = U16(v, 4);
				x::GteWriteData(loc8, 0);
				x::GteWriteData(vz, 1);
				x::GteRTPS();
				n--;
				v += 8;
				x::GteReadData2(0xE, dst);                        // SXY2
				x::GteReadData(&loc8, 0x13);                      // SZ3
				*depth++ = (uint32_t)loc8;
				x::GteReadData2(8, ir0);                          // IR0
				int32_t flag;                                     // [ebp-4]
				x::GteReadCtrl(&flag, 0x1F);                      // FLAG
				if (mode == 0) loc8 = loc8 >> 2;
				else
				{
					int32_t q = loc8 >> 2;
					if (mode & 0x20) loc8 = bias;
					else loc8 = add32(q, bias);
				}
				dst += 8;
				flag = shl32(flag, 4);
				S32(dst, -4) = flag;
				U16(dst, -4) = (uint16_t)loc8;
				ir0 += 8;
			} while (n > 0);
		}

		// --- normal lighting (0xB27CC3) ---
		if (U8(CUR(), 0xE1) != 0)
		{
			w = WS();
			U32(w, 0xF0) = (U32(w, 0x84) & 0xFF808080u) | 0x808080u;
			uint8_t *wb = WS();                                   // ebx
			uint8_t *out = PTR(wb, 0x80);                         // edi
			x::GteWriteData(S32(wb, 0xF0), 6);                    // RGBC
			uint8_t *nrm = PTR(wb, 0x78);                         // esi
			int32_t n = S32(PTR(wb, 0x68), 0x20);                 // ebx
			// the first normal is lit unconditionally (count <= 0 still runs once, 0xB27D14)
			do
			{
				x::GteWriteData2(0, S32(nrm, 0));
				x::GteWriteData2(1, S32(nrm, 4));
				nrm += 8;
				x::Gte_4601B0();
				x::GteReadData2(0x16, out);                       // RGB2
				n--;
				out += 8;
			} while (n > 0);
		}

		// --- prim stream (0xB27D75): {s16 type, s16 count} + count records of prim_size[type] ---
		w = WS();                                                 // edx
		uint8_t *hdr = PTR(w, 0x6C);
		int32_t cnt = S16(hdr, 2);
		while (cnt != -1)
		{
			S32(w, 0x70) = cnt;
			U32(WS(), 0x6C) = U32(WS(), 0x6C) + 4;
			int32_t type = S16(hdr, 0);
			uint8_t *w2 = WS();
			uint32_t size = MEM<uint8_t>(C().prim_size + (uint32_t)type);
			U32(w2, 0x58) = (uint32_t)mul32((int32_t)size, S32(w2, 0x70)) + U32(w2, 0x6C);
			uint16_t df = U16(CUR(), 0x4C);
			int32_t idx = type;
			if (df & 2) idx += 0x14;
			if (df & 4) idx += 0x28;
			C().prim[idx]();
			U32(WS(), 0x6C) = U32(WS(), 0x58);
			w = WS();
			hdr = PTR(w, 0x6C);
			cnt = S16(hdr, 2);
		}
		U32(CTX(), 0x7C) = U32(w, 0x60);
	}

	// ------------------------------------------------------------------------------------
	// 0xB26B80 (Draw 3 SkinnedBattleModel): embedded battle model (block bone+0xBC, created by
	// h_B26860): parent xform scaled (h_B269E0), step/pose the model (h_B26AD0), RenderGeometry
	// (mode 4) into the packet cursor ctx+0x7C; block+0x44 = arena scratch (ctx+0x74).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_03_SkinnedBattleModel()
	{
		h_B269E0();
		h_B26AD0(PTR(CUR(), 0xBC));
		uint8_t *blk = PTR(CUR(), 0xBC);
		U32(blk, 0x44) = U32(CTX(), 0x74);
		uint32_t cursor = U32(CTX(), 0x7C);
		uint32_t ot = U32(RT(), 0x4C);
		U32(CTX(), 0x7C) = x::RenderGeometry(blk + 0x30, blk + 0x40, ot, 4, cursor);
	}

	// ------------------------------------------------------------------------------------
	// 0xB26E20 (Draw 4 ScreenTintQuad): full-screen (320x256) flat quad of colour bone+0x94/96/98
	// (|v| clamped to 255; any negative component -> subtractive tpage 0x40 instead of additive
	// 0x20), OT slot bone+0xC0 when bone+0xDE & 0x60 else 0, then a draw-mode packet.
	// Colour 0: only byte +3 of the packet is written, nothing is inserted.
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_04_ScreenTintQuad()
	{
		int32_t slot;                                         // edi
		if (U8(CUR(), 0xDE) & 0x60) slot = S32(CUR(), 0xC0);
		else slot = 0;
		U32(WS(), 0xF0) = 0x20;
		int32_t r = S16(CUR(), 0x94);
		int32_t g = S16(CUR(), 0x96);
		int32_t b = S16(CUR(), 0x98);
		if (r < 0) { r = -r; U32(WS(), 0xF0) = 0x40; }
		if (r >= 0x100) r = 0xFF;
		if (g < 0) { g = -g; U32(WS(), 0xF0) = 0x40; }
		if (g >= 0x100) g = 0xFF;
		if (b < 0) { b = -b; U32(WS(), 0xF0) = 0x40; }
		if (b >= 0x100) b = 0xFF;
		uint8_t *p = PTR(CTX(), 0x7C);
		uint32_t col = (((uint32_t)b << 8 | (uint32_t)g) << 8) | (uint32_t)r;
		U8(p, 3) = 5;
		if (col == 0) return;
		U32(p, 8) = 0;
		U32(p, 4) = col | 0x2A000000u;
		U32(p, 0xC) = 0x140;
		U32(p, 0x10) = 0x1000000;
		U32(p, 0x14) = 0x1000140;
		x::InsertPrimAltViewport(U32(RT(), 0x4C) + (uint32_t)slot * 4u, p);
		p += 0x18;
		x::SetDrawMode(p, 0, 0, S32(WS(), 0xF0), nullptr);
		x::InsertPrimAltViewport(U32(RT(), 0x4C), p);
		U32(CTX(), 0x7C) = (uint32_t)(p + 0xC);
	}

	// ------------------------------------------------------------------------------------
	// 0xB26FB0 (Draw 1 MeshAtBonePos): mesh bone+0xD8 at the bone position, parent rotation,
	// no scale, morph descriptor bone+0xBC.
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_01_MeshAtBonePos()
	{
		S32(WS(), 0x48) = 0;
		S32(WS(), 0x44) = 0;
		S32(WS(), 0x40) = 0;
		U32(WS(), 0x4C) = U32(CUR(), 0xBC);
		SetupParentXform();
		DrawMeshObject(nullptr);
	}

	// ------------------------------------------------------------------------------------
	// 0xB27050 (Draw 2 MeshScaled): mesh with the parent rotation scaled by bone+0x8C.. (h_B269E0).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_02_MeshScaled()
	{
		U32(WS(), 0x4C) = U32(CUR(), 0xBC);
		S32(WS(), 0x48) = 0;
		S32(WS(), 0x44) = 0;
		S32(WS(), 0x40) = 0;
		h_B269E0();
		DrawMeshObject(nullptr);
	}

	// ------------------------------------------------------------------------------------
	// 0xB270A0 (Draw 9 MeshYClipped): scaled mesh at the parent origin, vertices clamped in Y by
	// the clip record ws+0x20 = {bone+0x94, +0x96, +0x98, flags bone+0xC2}.
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_09_MeshYClipped()
	{
		U32(WS(), 0x4C) = U32(CUR(), 0xBC);
		S32(WS(), 0x48) = 0;
		S32(WS(), 0x44) = 0;
		S32(WS(), 0x40) = 0;
		SetupParentXformScaledAtOrigin();
		uint8_t *c = WS() + 0x20;
		U16(c, 0) = U16(CUR(), 0x94);
		U16(c, 2) = U16(CUR(), 0x96);
		U16(c, 4) = U16(CUR(), 0x98);
		U16(c, 6) = U16(CUR(), 0xC2);
		DrawMeshObject((const int16_t *)c);
	}

	// ------------------------------------------------------------------------------------
	// 0xB27220 (Draw 7 MeshCameraSpace): camera matrix (node 0) * bone angles (bone+0x8C) into
	// ws+0x20, TR = camera * bone position; per-axis scale ws+0x40/44/48 = bone+0xB8 (scaled
	// vertex path).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_07_MeshCameraSpace()
	{
		x::SetRotMatrix(NODEMAT());
		x::SetTransVector(NODEMAT());
		x::GteLoadV0(CUR() + 0x94);
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
		uint8_t *m = WS() + 0x20;                          // esi
		x::RotMatrixFromAngles(CUR() + 0x8C, m);
		xl_MatrixMultiplyGte(NODEMAT(), m);
		pad_after_rot_mul(m, CUR() + 0x8C); // ws+0x32: vanilla stack word
		x::SetRotMatrix(m);
		U32(WS(), 0x48) = U32(CUR(), 0xB8);
		U32(WS(), 0x44) = U32(WS(), 0x48);
		U32(WS(), 0x40) = U32(WS(), 0x44);
		U32(WS(), 0x4C) = U32(CUR(), 0xBC);
		DrawMeshObject(nullptr);
	}

	// ------------------------------------------------------------------------------------
	// 0xB272D0 (Draw 27 MeshBillboardScaled): billboard xform, angles ws+0x90 = {bone+0x8C,
	// bone+0x8E, 0}, uniform scale bone+0x90 << 4, order 0.
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_27_MeshBillboardScaled()
	{
		uint8_t *ang = WS() + 0x90;
		uint8_t *pos = CUR() + 0x94;
		U16(ang, 0) = U16(CUR(), 0x8C);
		uint16_t ay = U16(CUR(), 0x8E);
		U16(ang, 4) = 0;
		U16(ang, 2) = ay;
		int32_t scale = shl32(S16(CUR(), 0x90), 4);
		SetupBillboardXform(pos, ang, scale, 0);
		S32(WS(), 0x48) = 0;
		S32(WS(), 0x44) = 0;
		S32(WS(), 0x40) = 0;
		U32(WS(), 0x4C) = U32(CUR(), 0xBC);
		DrawMeshObject(nullptr);
	}
}

	// ------------------------------------------------------------------------------------
	// 0xB269E0 SetupParentXformScaled: GTE R/TR = parent matrix (bone+0x9C), TR = parent * bone
	// position (bone+0x94); ws+0xD4.. = scale bone+0x8C/8E/90 << 4; ws+0xE0 = parent scaled;
	// GTE R = ws+0xE0. (Original returns 0, unused.)
	// ------------------------------------------------------------------------------------
	void h_B269E0()
	{
		Mat4x3 *m = blob::GetParentMatrix(U16(CUR(), 0x9C));   // esi
		x::GteSetRotMatrix(m);
		x::GteSetTransVector(m);
		x::GteLoadV0(CUR() + 0x94);
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
		S32(WS(), 0xD4) = shl32(S16(CUR(), 0x8C), 4);
		S32(WS(), 0xD8) = shl32(S16(CUR(), 0x8E), 4);
		S32(WS(), 0xDC) = shl32(S16(CUR(), 0x90), 4);
		uint8_t *d = WS() + 0xE0;                                 // edi
		for (int i = 0; i < 8; i++) U32(d, i * 4) = U32(m, i * 4);
		x::ScaleMatrix(d, WS() + 0xD4);
		x::GteSetRotMatrix(d);
	}

	// ------------------------------------------------------------------------------------
	// 0xB26AD0 StepEmbeddedModelAnim(blk): embedded battle model block {+0 u16 frame counter,
	// +2 u16 flags (1 = frozen, 2 = hold pose), +9 u8 anim id, +0x10 BattleAnimHeader,
	// +0x20 BattleAnimCmd}. Not frozen (rt+0x45 == 0 and !(flags & 1)): counter != 0 ->
	// (flags & 2 ? rebuild pose : ReadAnimation, restart the anim when it returns 1, counter++);
	// then the bones' world matrices from ws+0xE0. Frozen: rebuild the current pose + world
	// matrices. Counter 0 -> 1 at the end. Returns 0.
	// Held frames (rt+0x45 forced to 0xFF): when the real tick would have advanced the
	// animation, the bone matrices come from the exact in-between pose (pose_midpoint).
	// ------------------------------------------------------------------------------------
	int32_t h_B26AD0(uint8_t *blk)
	{
		uint8_t *hdr = blk + 0x10;   // edi
		uint8_t *cmd = blk + 0x20;   // ebx
		bool frozen = true;
		if (U8(RT(), 0x45) == 0)
		{
			uint16_t fl = U16(blk, 2);
			if (!(fl & 1))
			{
				frozen = false;
				if (U16(blk, 0) != 0)
				{
					if (fl & 2)
						x::BuildBoneMatricesFromPose(hdr);
					else
					{
						// 0xB26B0C `movzx cx, byte [esi+9]`: the upper half of ecx is whatever
						// ReadAnimation left; PreReadAnimation reads only the low word (0x509445).
						if (x::ReadAnimation(hdr, cmd) == 1) x::PreReadAnimation(hdr, cmd, U8(blk, 9));
						U16(blk, 0) = (uint16_t)(U16(blk, 0) + 1);
					}
				}
				x::ComputeBonesWorldMatrices(hdr, WS() + 0xE0);
			}
		}
		if (frozen)
		{
			uint16_t fl = U16(blk, 2);
			if (g_held.active && g_held.real_skip == 0 && !(fl & 1) && U16(blk, 0) != 0 && !(fl & 2))
				pose_midpoint(hdr, cmd, g_held.num, g_held.den);
			else
				x::BuildBoneMatricesFromPose(hdr);
			x::ComputeBonesWorldMatrices(hdr, WS() + 0xE0);
		}
		if (U16(blk, 0) == 0) U16(blk, 0) = 1;
		return 0;
	}

	// exported for the clone-specific handlers (byte clones of these helpers exist in the clones)
	void h_B27000() { part_draw_mesh::SetupParentXform(); }
	void h_B27130() { part_draw_mesh::SetupParentXformScaledAtOrigin(); }
	void h_B27360(const void *pos, const void *angles, int32_t scale, int32_t order) { part_draw_mesh::SetupBillboardXform(pos, angles, scale, order); }
	void h_B27440(const int16_t *clipArg) { part_draw_mesh::DrawMeshObject(clipArg); }

	void fill_draw_mesh(Generic &g)
	{
		g.draw[1] = part_draw_mesh::dh_01_MeshAtBonePos;
		g.draw[2] = part_draw_mesh::dh_02_MeshScaled;
		g.draw[3] = part_draw_mesh::dh_03_SkinnedBattleModel;
		g.draw[4] = part_draw_mesh::dh_04_ScreenTintQuad;
		g.draw[7] = part_draw_mesh::dh_07_MeshCameraSpace;
		g.draw[9] = part_draw_mesh::dh_09_MeshYClipped;
		g.draw[21] = part_draw_mesh::dh_21_ScrollTextureUpload;
		g.draw[27] = part_draw_mesh::dh_27_MeshBillboardScaled;
	}
}
}

// ============================================================================================
// part draw_prim
// ============================================================================================
// Part "draw_prim": the primitive renderers of the generic mesh renderer (DrawMeshObject,
// Ifrit 0xB27440). DrawMeshObject walks the mesh's primitive blocks
//   block = { s16 type, s16 count, record[count] }   (0xFFFF type ends the list)
// and calls C().prim[type (+20 if bone+0x4C bit1) (+40 if bit2)] for each block with the
// workspace (WS()) prepared as follows (set in DrawMeshObject 0xB27440):
//   ws+0x50  u32  0xE1000200 | (s16)bone+0x92   draw-mode (tpage) word; its low 16 bits are ORed
//                                                 into the tpage half-word of textured prims
//   ws+0x54  s32  (s16)bone+0x9A                 CLUT offset added to the record CLUT word
//   ws+0x58  ptr  end of the current block (record base + prim_size[type] * count); becomes the
//                 next block cursor after the call (set at 0xB27DB7, read back at 0xB27DE0)
//   ws+0x5C  u32  rt+0x4C                        OT base (byte address); OT slot = base + key
//   ws+0x60  ptr  ctx+0x7C                       packet cursor (prims advance it; written back
//                                                 to ctx+0x7C at the end of DrawMeshObject)
//   ws+0x68  ptr  bone+0xD8                      mesh header
//   ws+0x6C  ptr  first record of the current block
//   ws+0x70  s32  (s16)block count
//   ws+0x74  ptr  vertex positions (mesh +0x14)  ws+0x78 ptr  normals (mesh +0x1C), 8 B each
//   ws+0x7C  ptr  ctx+0x74 (arena)               projected vertices, 8 B each (indexed by the
//                                                 record's BYTE offset): +0 u32 SXY (GTE SXY2),
//                                                 +4 u16 depth (SZ3/4 or bias), +6 u16 clip bits
//                                                 (bits 0x46 of the high half = off-screen/behind)
//   ws+0x80  ptr  ws+0x7C + nverts*8             per-normal lit colours (GTE RGB2, when bone+0xE1)
//   ws+0x84  u32  bone+0xCC & 0x2FFFFFF          material colour (RGB) + bit 25 semi-transparency
//   ws+0x88  ptr  ws+0x80 + nnormals*8           per-vertex GTE IR0 scratch
//   ws+0x8C  u32  ws+0x84 & 0x2000000            semi-transparency bit ORed into the GP0 code
//   ws+0x90  u32  (u8)bone+0xDE                  flags: 0x10 = no back-face culling (NCLIP skipped),
//                                                 0x60 = depth bias mode (used by DrawMeshObject)
//   ws+0x94  s32  (s16)bone+0xC0                 depth bias
//   ws+0x98  u32  (u16)bone+0x9E                 UV offset added to the record UV words (FT3, FT4, GT4;
//                                                 NOT GT3 - vanilla)
//   ws+0xC0  s32  depth scale divisor: FLT_1877DA8 = const(0xB69540) / ws+0xC0 (or 1.0)
//   DEPTHARR() u32[] GTE SZ3 of every projected vertex (index = vertex byte offset / 8)
//   ws+0xE0..0xF3  scratch for the light/colour matrix and V0 of the colour setup 0xB29450.
//
// Every renderer: for each record, reads the 3/4 projected vertices, optional NCLIP back-face
// test, rejects the prim if any vertex has clip bits 0x46, computes the OT key (triangles:
// ((d0+d1+d2) & 0xFFFF) / 3 & 0x3FFC, quads: (d0+d1+d2+d3) >> 2 & 0x3FFC), lights the record
// colours through the GTE (NCCS 0x4601B0, RGBC in data reg 6, result RGB2 reg 22) and inserts
// the packet with SSIGPU_InsertPrimDepthKeys(ot + key, packet, z0, z1, z2, z3 or 0), where
// zN = (int)(u16 SZ3 of vertex N * FLT_1877DA8).
// No held-frame specific behaviour (pure draw code).


namespace ff8fx
{
namespace gfc
{
namespace part_draw_prim
{
	// fild qword [u16 zero-extended DEPTHARR[off >> 3]]; fmul dword [0x1877DA8]; call __ftol
	// (u16 x float = at most 40 significant bits: exact in a double; __ftol truncates to int64
	// and the callers keep the low dword)
	static inline int32_t DepthKey(uint32_t off)
	{
		uint32_t v = DEPTHARR()[off >> 3] & 0xFFFF;
		return (int32_t)(int64_t)((double)v * (double)FLT_1877DA8());
	}

	// mov [t], 0x55555556; imul dword [t] (edx:eax = v * 0x55555556); ...; sar v, 0x1F; sub edx, v
	static inline int32_t Div3(int32_t v)
	{
		int32_t hi = (int32_t)(((int64_t)v * (int64_t)0x55555556) >> 32);
		return sub32(hi, v >> 31);
	}

	// 0xB29450 (helper of the prim renderers): colour/light setup for the "tinted" prims.
	// Light matrix = three lights along +X (rows (0x1000,0,0)), colour matrix = the material
	// colour c (r, g, b) * 32 on light 1, back colour 0, V0 = (0x1000, 0x1000, 0x1000); so NCCS
	// gives record colour * material colour. Uses ws+0xE0..0xF3 as the matrix scratch.
	static void LightColourSetup(int32_t c)
	{
		U32(WS(), 0xE0) = 0x1000;
		U32(WS(), 0xE4) = 0x10000000;
		U32(WS(), 0xE8) = 0;
		U32(WS(), 0xEC) = 0x1000;
		U32(WS(), 0xF0) = 0;
		x::SetLightMatrix(WS() + 0xE0);
		U32(WS(), 0xE0) = (uint32_t)(c & 0xFF) << 5;
		U32(WS(), 0xE4) = (uint32_t)(c & 0xFF00) << 13;
		U32(WS(), 0xEC) = (uint32_t)((c >> 11) & 0x1FE0);     // sar eax, 0xB
		x::SetColorMatrix(WS() + 0xE0);
		x::SetBackColor(0, 0, 0);
		U32(WS(), 0xE0) = 0x10001000;
		U32(WS(), 0xE4) = 0x1000;
		x::GteLoadV0(WS() + 0xE0);
	}

	// NCLIP of three screen vertices (GTE SXY0..2 = data regs 12..14), MAC0 (reg 24)
	static inline int32_t Nclip(uint32_t s0, uint32_t s1, uint32_t s2)
	{
		int32_t mac0;
		x::GteWriteData((int32_t)s0, 12);
		x::GteWriteData((int32_t)s1, 13);
		x::GteWriteData((int32_t)s2, 14);
		x::GteNCLIP();
		x::GteReadData(&mac0, 24);
		return mac0;
	}

	// 0xB27E90 (Prim[8], C().prim[7]): Gouraud triangle with a draw-mode word, colours tinted by the
	// material colour. Record 0x14: +0/+4/+8 u32 colour 0..2, +0xC/+0xE/+0x10 u16 vertex offsets.
	// Packet 0x24 (len 8): +4 ws+0x50 draw mode, +8 0, +0xC/+0x14/+0x1C RGB (code 0x30 | ws+0x8C),
	// +0x10/+0x18/+0x20 SXY.
	static void __cdecl pr_07_TintedG3()
	{
		LightColourSetup((int32_t)U32(WS(), 0x84));
		uint8_t *ws = WS();
		int32_t count = S32(ws, 0x70);
		uint8_t *pkt = PTR(ws, 0x60);
		uint8_t *rec = PTR(ws, 0x6C);
		uint32_t ot = U32(ws, 0x5C);
		uint32_t nocull = U32(ws, 0x90) & 0x10;
		U32(ws, 0xF0) = 0;
		do
		{
			uint8_t *vb = PTR(ws, 0x7C);
			uint32_t o0 = U16(rec, 0xC);
			uint32_t o1 = U16(rec, 0xE);
			uint32_t o2 = U16(rec, 0x10);
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			uint8_t *p1 = vb + o1;
			uint8_t *p0 = vb + o0;
			uint8_t *p2 = vb + o2;
			uint32_t s1 = U32(p1, 0);
			uint32_t s0 = U32(p0, 0);
			uint32_t s2 = U32(p2, 0);
			U32(pkt, 0x18) = s1;
			U32(pkt, 0x10) = s0;
			U32(pkt, 0x20) = s2;
			if (nocull == 0 && Nclip(s0, s1, s2) < 0) goto next;
			{
				uint32_t a0 = U32(p0, 4), a1 = U32(p1, 4), a2 = U32(p2, 4);
				if (((a2 | a1 | a0) >> 16) & 0x46) goto next;
				int32_t sum = (int32_t)((a2 + a1 + a0) & 0xFFFF);
				U8(pkt, 3) = 8;
				U32(pkt, 8) = 0;
				x::GteWriteData((int32_t)(U32(ws, 0x8C) | U32(rec, 0) | 0x30000000), 6);
				uint32_t mode = U32(ws, 0x50);
				x::Gte_4601B0();
				U32(pkt, 4) = mode;
				x::GteReadData2(0x16, pkt + 0xC);
				x::GteWriteData2(6, (int32_t)U32(rec, 4));
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 0x14);
				x::GteWriteData2(6, (int32_t)U32(rec, 8));
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 0x1C);
				uint32_t key = (uint32_t)Div3(sum) & 0x3FFC;
				x::InsertPrimDepthKeys(ot + key, pkt, z0, z1, z2, 0);
				pkt += 0x24;
			}
		next:
			rec += 0x14;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// 0xB28110 (Prim[9], C().prim[8]): flat textured triangle (FT3). Record 0x14: +0 u32 colour,
	// +4/+6/+8 u16 UV 0..2, +0xA/+0xC/+0xE u16 vertex offsets, +0x10 u16 CLUT, +0x12 u16 tpage.
	// Packet 0x20 (len 7): +4 RGB (code 0x24 | ws+0x8C), +8/+0x10/+0x18 SXY, +0xC/+0x14/+0x1C
	// UV + ws+0x98, +0xE CLUT + ws+0x54, +0x16 tpage | ws+0x50.
	static void __cdecl pr_08_TintedFT3()
	{
		LightColourSetup((int32_t)U32(WS(), 0x84));
		uint8_t *ws = WS();
		int32_t count = S32(ws, 0x70);
		uint32_t ot = U32(ws, 0x5C);
		uint8_t *pkt = PTR(ws, 0x60);
		uint8_t *rec = PTR(ws, 0x6C);
		uint32_t nocull = U32(ws, 0x90) & 0x10;
		U32(ws, 0xF0) = 0;
		do
		{
			uint8_t *vb = PTR(ws, 0x7C);
			uint32_t o1 = U16(rec, 0xC);
			uint32_t o0 = U16(rec, 0xA);
			uint32_t o2 = U16(rec, 0xE);
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			uint8_t *p1 = vb + o1;
			uint8_t *p0 = vb + o0;
			uint8_t *p2 = vb + o2;
			uint32_t s0 = U32(p0, 0);
			uint32_t s2 = U32(p2, 0);
			uint32_t s1 = U32(p1, 0);
			U32(pkt, 0x10) = s1;
			U32(pkt, 8) = s0;
			U32(pkt, 0x18) = s2;
			if (nocull == 0 && Nclip(s0, s1, s2) < 0) goto next;
			{
				uint32_t a0 = U32(p0, 4), a1 = U32(p1, 4), a2 = U32(p2, 4);
				if (((a2 | a1 | a0) >> 16) & 0x46) goto next;
				int32_t sum = (int32_t)((a2 + a1 + a0) & 0xFFFF);
				U8(pkt, 3) = 7;
				uint32_t tpage = U32(ws, 0x50) | U16(rec, 0x12);
				int32_t clut_add = S32(ws, 0x54);
				U16(pkt, 0x16) = (uint16_t)tpage;
				U16(pkt, 0xE) = (uint16_t)add32(U16(rec, 0x10), clut_add);
				uint32_t uvofs = U16(ws, 0x98);
				uint32_t uv1 = U16(rec, 6) + uvofs;
				uint32_t uv0 = U16(rec, 4) + uvofs;
				uint32_t uv2 = U16(rec, 8) + uvofs;
				U16(pkt, 0xC) = (uint16_t)uv0;
				U16(pkt, 0x14) = (uint16_t)uv1;
				U16(pkt, 0x1C) = (uint16_t)uv2;
				x::GteWriteData((int32_t)(U32(ws, 0x8C) | U32(rec, 0) | 0x24000000), 6);
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 4);
				uint32_t key = (uint32_t)Div3(sum) & 0x3FFC;
				x::InsertPrimDepthKeys(ot + key, pkt, z0, z1, z2, 0);
				pkt += 0x20;
			}
		next:
			rec += 0x14;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// 0xB283B0 (Prim[10], C().prim[9]): Gouraud textured triangle (GT3). Record 0x1C: +0/+4/+8 u32
	// colour 0..2, +0xC/+0xE/+0x10 u16 UV 0..2, +0x12/+0x14/+0x16 u16 vertex offsets, +0x18 u16
	// CLUT, +0x1A u16 tpage. Packet 0x28 (len 9): +4/+0x10/+0x1C RGB (code 0x34 | ws+0x8C),
	// +8/+0x14/+0x20 SXY, +0xC/+0x18/+0x24 UV (ws+0x98 NOT added - vanilla), +0xE CLUT + ws+0x54,
	// +0x1A tpage | ws+0x50.
	static void __cdecl pr_09_TintedGT3()
	{
		LightColourSetup((int32_t)U32(WS(), 0x84));
		uint8_t *ws = WS();
		int32_t count = S32(ws, 0x70);
		uint32_t ot = U32(ws, 0x5C);
		uint8_t *pkt = PTR(ws, 0x60);
		uint8_t *rec = PTR(ws, 0x6C);
		uint32_t nocull = U32(ws, 0x90) & 0x10;
		U32(ws, 0xF0) = 0;
		do
		{
			uint8_t *vb = PTR(ws, 0x7C);
			uint32_t o1 = U16(rec, 0x14);
			uint32_t o0 = U16(rec, 0x12);
			uint32_t o2 = U16(rec, 0x16);
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			uint8_t *p1 = vb + o1;
			uint8_t *p0 = vb + o0;
			uint8_t *p2 = vb + o2;
			uint32_t s0 = U32(p0, 0);
			uint32_t s2 = U32(p2, 0);
			uint32_t s1 = U32(p1, 0);
			U32(pkt, 0x14) = s1;
			U32(pkt, 8) = s0;
			U32(pkt, 0x20) = s2;
			if (nocull == 0 && Nclip(s0, s1, s2) < 0) goto next;
			{
				uint32_t a0 = U32(p0, 4), a1 = U32(p1, 4), a2 = U32(p2, 4);
				if (((a2 | a1 | a0) >> 16) & 0x46) goto next;
				int32_t sum = (int32_t)((a2 + a1 + a0) & 0xFFFF);
				U8(pkt, 3) = 9;
				uint32_t tpage = U32(ws, 0x50) | U16(rec, 0x1A);
				int32_t clut_add = S32(ws, 0x54);
				U16(pkt, 0x1A) = (uint16_t)tpage;
				U16(pkt, 0xE) = (uint16_t)add32(U16(rec, 0x18), clut_add);
				U16(pkt, 0xC) = U16(rec, 0xC);
				U16(pkt, 0x18) = U16(rec, 0xE);
				U16(pkt, 0x24) = U16(rec, 0x10);
				x::GteWriteData((int32_t)(U32(ws, 0x8C) | U32(rec, 0) | 0x34000000), 6);
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 4);
				x::GteWriteData2(6, (int32_t)U32(rec, 4));
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 0x10);
				x::GteWriteData2(6, (int32_t)U32(rec, 8));
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 0x1C);
				uint32_t key = (uint32_t)Div3(sum) & 0x3FFC;
				x::InsertPrimDepthKeys(ot + key, pkt, z0, z1, z2, 0);
				pkt += 0x28;
			}
		next:
			rec += 0x1C;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// 0xB28690 (Prim[13], C().prim[12]): semi-transparent Gouraud quad lit by one face normal with
	// the current light setup (no 0xB29450), followed by a draw-mode packet in the same OT slot.
	// Record 0x1C: +0/+4/+8/+0xC u32 colour 0..3, +0x10 u16 normal byte offset (into ws+0x78),
	// +0x12 unused, +0x14/+0x16/+0x18/+0x1A u16 vertex offsets.
	// Packets 0x30: G4 0x24 (len 8, code 0x3A, RGB at +4/+0xC/+0x14/+0x1C, SXY at +8/+0x10/+0x18/
	// +0x20) then draw mode 0xC (tag 0x02000000, +4 ws+0x50, +8 0).
	// Vanilla: overwrites ws+0x84 (material colour) with 0x3A000000 for the rest of the mesh draw.
	static void __cdecl pr_12_NormalLitG4()
	{
		U32(WS(), 0x84) = 0x3A000000;
		uint8_t *ws = WS();
		int32_t count = S32(ws, 0x70);
		uint8_t *pkt = PTR(ws, 0x60);
		uint32_t ot = U32(ws, 0x5C);
		uint32_t nocull = U32(ws, 0x90) & 0x10;
		uint8_t *rec = PTR(ws, 0x6C);
		uint8_t *c0 = pkt + 4;      // [ebp-0x24]
		uint8_t *c3 = pkt + 0x1C;   // [ebp-0x20]
		uint8_t *c2 = pkt + 0x14;   // [ebp-0x1C]
		U32(ws, 0xF0) = 0;
		uint8_t *c1 = pkt + 0xC;    // [ebp-0x18]
		do
		{
			uint8_t *vb = PTR(ws, 0x7C);
			uint32_t o0 = U16(rec, 0x14);
			uint32_t o1 = U16(rec, 0x16);
			uint32_t o2 = U16(rec, 0x18);
			uint32_t o3 = U16(rec, 0x1A);
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			int32_t z3 = DepthKey(o3);
			uint8_t *p2 = vb + o2;
			uint8_t *p1 = vb + o1;
			uint8_t *p3 = vb + o3;
			uint32_t s1 = U32(p1, 0);
			uint8_t *p0 = vb + o0;
			uint32_t s2 = U32(p2, 0);
			uint32_t s0 = U32(p0, 0);
			uint32_t s3 = U32(p3, 0);
			U32(pkt, 0x10) = s1;
			U32(pkt, 0x20) = s3;
			U32(pkt, 8) = s0;
			U32(pkt, 0x18) = s2;
			if (nocull == 0 && Nclip(s0, s1, s2) < 0) goto next;
			{
				uint32_t a0 = U32(p0, 4), a1 = U32(p1, 4), a2 = U32(p2, 4), a3 = U32(p3, 4);
				if (((a3 | a2 | a1 | a0) >> 16) & 0x46) goto next;
				U8(pkt, 3) = 8;
				uint32_t slot = ot + (((a3 + a2 + a1 + a0) >> 2) & 0x3FFC);
				uint32_t mat = U32(ws, 0x84);
				uint8_t *normals = PTR(ws, 0x78);
				uint8_t *n;
				x::GteWriteData((int32_t)(U32(rec, 0) | mat), 6);
				n = normals + U16(rec, 0x10);
				x::GteWriteData2(0, (int32_t)U32(n, 0));
				x::GteWriteData2(1, (int32_t)U32(n, 4));
				x::Gte_4601B0();
				x::GteReadData2(0x16, c0);
				x::GteWriteData((int32_t)(mat | U32(rec, 4)), 6);
				n = normals + U16(rec, 0x10);
				x::GteWriteData2(0, (int32_t)U32(n, 0));
				x::GteWriteData2(1, (int32_t)U32(n, 4));
				uint32_t col2 = U32(rec, 8);
				x::Gte_4601B0();
				col2 |= mat;
				x::GteReadData2(0x16, c1);
				x::GteWriteData((int32_t)col2, 6);
				n = normals + U16(rec, 0x10);
				x::GteWriteData2(0, (int32_t)U32(n, 0));
				x::GteWriteData2(1, (int32_t)U32(n, 4));
				uint32_t col3 = U32(rec, 0xC);
				x::Gte_4601B0();
				x::GteReadData2(0x16, c2);
				x::GteWriteData((int32_t)(mat | col3), 6);
				n = normals + U16(rec, 0x10);
				x::GteWriteData2(0, (int32_t)U32(n, 0));
				x::GteWriteData2(1, (int32_t)U32(n, 4));
				uint32_t mode = U32(ws, 0x50);
				x::Gte_4601B0();
				x::GteReadData2(0x16, c3);
				x::InsertPrimDepthKeys(slot, pkt, z0, z1, z2, z3);
				c1 += 0x24;
				c3 += 0x24;
				pkt += 0x24;
				c2 += 0x24;
				c0 += 0x24;
				U32(pkt, 0) = 0x2000000;
				U32(c0, 0) = mode;              // = pkt + 4
				U32(pkt, 8) = 0;
				x::InsertPrimDepthKeys(slot, pkt, z0, z1, z2, z3);
				c1 += 0xC;
				pkt += 0xC;
				c2 += 0xC;
				c3 += 0xC;
				c0 += 0xC;
			}
		next:
			rec += 0x1C;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// 0xB28B00 (Prim[18], C().prim[17]): Gouraud quad with a draw-mode word, colours tinted by the
	// material colour. Record 0x18: +0/+4/+8/+0xC u32 colour 0..3, +0x10/+0x12/+0x14/+0x16 u16
	// vertex offsets. Packet 0x2C (len 0xA): +4 ws+0x50, +8 0, +0xC/+0x14/+0x1C/+0x24 RGB (code
	// 0x38 | ws+0x8C), +0x10/+0x18/+0x20/+0x28 SXY.
	static void __cdecl pr_17_TintedG4()
	{
		LightColourSetup((int32_t)U32(WS(), 0x84));
		uint8_t *ws = WS();
		int32_t count = S32(ws, 0x70);
		uint8_t *pkt = PTR(ws, 0x60);
		uint8_t *rec = PTR(ws, 0x6C);
		uint32_t ot = U32(ws, 0x5C);
		uint32_t nocull = U32(ws, 0x90) & 0x10;
		U32(ws, 0xF0) = 0;
		do
		{
			uint8_t *vb = PTR(ws, 0x7C);
			uint32_t o0 = U16(rec, 0x10);
			uint32_t o1 = U16(rec, 0x12);
			uint32_t o2 = U16(rec, 0x14);
			uint32_t o3 = U16(rec, 0x16);
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			int32_t z3 = DepthKey(o3);
			uint8_t *p2 = vb + o2;
			uint8_t *p1 = vb + o1;
			uint8_t *p3 = vb + o3;
			uint32_t s1 = U32(p1, 0);
			uint8_t *p0 = vb + o0;
			uint32_t s2 = U32(p2, 0);
			uint32_t s0 = U32(p0, 0);
			uint32_t s3 = U32(p3, 0);
			U32(pkt, 0x18) = s1;
			U32(pkt, 0x28) = s3;
			U32(pkt, 0x10) = s0;
			U32(pkt, 0x20) = s2;
			if (nocull == 0 && Nclip(s0, s1, s2) < 0) goto next;
			{
				uint32_t a0 = U32(p0, 4), a1 = U32(p1, 4), a2 = U32(p2, 4), a3 = U32(p3, 4);
				if (((a3 | a2 | a1 | a0) >> 16) & 0x46) goto next;
				U8(pkt, 3) = 0xA;
				U32(pkt, 8) = 0;
				uint32_t slot = ot + (((a3 + a2 + a1 + a0) >> 2) & 0x3FFC);
				x::GteWriteData((int32_t)(U32(ws, 0x8C) | U32(rec, 0) | 0x38000000), 6);
				uint32_t mode = U32(ws, 0x50);
				x::Gte_4601B0();
				U32(pkt, 4) = mode;
				x::GteReadData2(0x16, pkt + 0xC);
				x::GteWriteData2(6, (int32_t)U32(rec, 4));
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 0x14);
				x::GteWriteData2(6, (int32_t)U32(rec, 8));
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 0x1C);
				x::GteWriteData2(6, (int32_t)U32(rec, 0xC));
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 0x24);
				x::InsertPrimDepthKeys(slot, pkt, z0, z1, z2, z3);
				pkt += 0x2C;
			}
		next:
			rec += 0x18;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// 0xB28DD0 (Prim[19], C().prim[18]): flat textured quad (FT4). Record 0x18: +0 u32 colour,
	// +4/+6/+8/+0xA u16 UV 0..3, +0xC/+0xE/+0x10/+0x12 u16 vertex offsets, +0x14 u16 CLUT,
	// +0x16 u16 tpage. Packet 0x28 (len 9): +4 RGB (code 0x2C | ws+0x8C), +8/+0x10/+0x18/+0x20 SXY,
	// +0xC/+0x14/+0x1C/+0x24 UV + ws+0x98, +0xE CLUT + ws+0x54, +0x16 tpage | ws+0x50.
	static void __cdecl pr_18_TintedFT4()
	{
		LightColourSetup((int32_t)U32(WS(), 0x84));
		uint8_t *ws = WS();
		int32_t count = S32(ws, 0x70);
		uint8_t *pkt = PTR(ws, 0x60);
		uint8_t *rec = PTR(ws, 0x6C);
		uint32_t ot = U32(ws, 0x5C);
		uint32_t nocull = U32(ws, 0x90) & 0x10;
		U32(ws, 0xF0) = 0;
		do
		{
			uint8_t *vb = PTR(ws, 0x7C);
			uint32_t o0 = U16(rec, 0xC);
			uint32_t o1 = U16(rec, 0xE);
			uint32_t o2 = U16(rec, 0x10);
			uint32_t o3 = U16(rec, 0x12);
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			int32_t z3 = DepthKey(o3);
			uint8_t *p2 = vb + o2;
			uint8_t *p1 = vb + o1;
			uint8_t *p3 = vb + o3;
			uint32_t s1 = U32(p1, 0);
			uint8_t *p0 = vb + o0;
			uint32_t s2 = U32(p2, 0);
			uint32_t s0 = U32(p0, 0);
			uint32_t s3 = U32(p3, 0);
			U32(pkt, 0x10) = s1;
			U32(pkt, 0x20) = s3;
			U32(pkt, 8) = s0;
			U32(pkt, 0x18) = s2;
			if (nocull == 0 && Nclip(s0, s1, s2) < 0) goto next;
			{
				uint32_t a0 = U32(p0, 4), a1 = U32(p1, 4), a2 = U32(p2, 4), a3 = U32(p3, 4);
				if (((a3 | a2 | a1 | a0) >> 16) & 0x46) goto next;
				U8(pkt, 3) = 9;
				uint32_t slot = ot + (((a3 + a2 + a1 + a0) >> 2) & 0x3FFC);
				uint32_t tpage = U32(ws, 0x50) | U16(rec, 0x16);
				int32_t clut_add = S32(ws, 0x54);
				U16(pkt, 0x16) = (uint16_t)tpage;
				U16(pkt, 0xE) = (uint16_t)add32(U16(rec, 0x14), clut_add);
				uint32_t uvofs = U16(ws, 0x98);
				uint32_t uv1 = U16(rec, 6) + uvofs;
				uint32_t uv0 = U16(rec, 4) + uvofs;
				uint32_t uv2 = U16(rec, 8) + uvofs;
				uint32_t uv3 = U16(rec, 0xA) + uvofs;
				U16(pkt, 0xC) = (uint16_t)uv0;
				U16(pkt, 0x24) = (uint16_t)uv3;
				U16(pkt, 0x14) = (uint16_t)uv1;
				U16(pkt, 0x1C) = (uint16_t)uv2;
				x::GteWriteData((int32_t)(U32(ws, 0x8C) | U32(rec, 0) | 0x2C000000), 6);
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 4);
				x::InsertPrimDepthKeys(slot, pkt, z0, z1, z2, z3);
				pkt += 0x28;
			}
		next:
			rec += 0x18;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// 0xB290B0 (Prim[20], C().prim[19]): Gouraud textured quad (GT4). Record 0x24: +0/+4/+8/+0xC
	// u32 colour 0..3, +0x10/+0x12/+0x14/+0x16 u16 UV 0..3, +0x18/+0x1A/+0x1C/+0x1E u16 vertex
	// offsets, +0x20 u16 CLUT, +0x22 u16 tpage. Packet 0x34 (len 0xC): +4/+0x10/+0x1C/+0x28 RGB
	// (code 0x3C | ws+0x8C), +8/+0x14/+0x20/+0x2C SXY, +0xC/+0x18/+0x24/+0x30 UV + ws+0x98,
	// +0xE CLUT + ws+0x54, +0x1A tpage | ws+0x50.
	static void __cdecl pr_19_TintedGT4()
	{
		LightColourSetup((int32_t)U32(WS(), 0x84));
		uint8_t *ws = WS();
		int32_t count = S32(ws, 0x70);
		uint8_t *pkt = PTR(ws, 0x60);
		uint8_t *rec = PTR(ws, 0x6C);
		uint32_t ot = U32(ws, 0x5C);
		uint32_t nocull = U32(ws, 0x90) & 0x10;
		U32(ws, 0xF0) = 0;
		do
		{
			uint8_t *vb = PTR(ws, 0x7C);
			uint32_t o0 = U16(rec, 0x18);
			uint32_t o1 = U16(rec, 0x1A);
			uint32_t o2 = U16(rec, 0x1C);
			uint32_t o3 = U16(rec, 0x1E);
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			int32_t z3 = DepthKey(o3);
			uint8_t *p2 = vb + o2;
			uint8_t *p1 = vb + o1;
			uint8_t *p3 = vb + o3;
			uint32_t s1 = U32(p1, 0);
			uint8_t *p0 = vb + o0;
			uint32_t s2 = U32(p2, 0);
			uint32_t s0 = U32(p0, 0);
			uint32_t s3 = U32(p3, 0);
			U32(pkt, 0x14) = s1;
			U32(pkt, 0x2C) = s3;
			U32(pkt, 8) = s0;
			U32(pkt, 0x20) = s2;
			if (nocull == 0 && Nclip(s0, s1, s2) < 0) goto next;
			{
				uint32_t a0 = U32(p0, 4), a1 = U32(p1, 4), a2 = U32(p2, 4), a3 = U32(p3, 4);
				if (((a3 | a2 | a1 | a0) >> 16) & 0x46) goto next;
				U8(pkt, 3) = 0xC;
				uint32_t slot = ot + (((a3 + a2 + a1 + a0) >> 2) & 0x3FFC);
				uint32_t tpage = U32(ws, 0x50) | U16(rec, 0x22);
				int32_t clut_add = S32(ws, 0x54);
				U16(pkt, 0x1A) = (uint16_t)tpage;
				U16(pkt, 0xE) = (uint16_t)add32(U16(rec, 0x20), clut_add);
				uint32_t uvofs = U16(ws, 0x98);
				uint32_t uv1 = U16(rec, 0x12) + uvofs;
				uint32_t uv0 = U16(rec, 0x10) + uvofs;
				uint32_t uv2 = U16(rec, 0x14) + uvofs;
				uint32_t uv3 = U16(rec, 0x16) + uvofs;
				U16(pkt, 0xC) = (uint16_t)uv0;
				U16(pkt, 0x30) = (uint16_t)uv3;
				U16(pkt, 0x18) = (uint16_t)uv1;
				U16(pkt, 0x24) = (uint16_t)uv2;
				x::GteWriteData((int32_t)(U32(ws, 0x8C) | U32(rec, 0) | 0x3C000000), 6);
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 4);
				x::GteWriteData2(6, (int32_t)U32(rec, 4));
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 0x10);
				x::GteWriteData2(6, (int32_t)U32(rec, 8));
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 0x1C);
				x::GteWriteData2(6, (int32_t)U32(rec, 0xC));
				x::Gte_4601B0();
				x::GteReadData2(0x16, pkt + 0x28);
				x::InsertPrimDepthKeys(slot, pkt, z0, z1, z2, z3);
				pkt += 0x34;
			}
		next:
			rec += 0x24;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}
}

	void h_B29450(int32_t c) { part_draw_prim::LightColourSetup(c); }

	void fill_draw_prim(Generic &g)
	{
		g.prim[7] = part_draw_prim::pr_07_TintedG3;     // 0xB27E90
		g.prim[8] = part_draw_prim::pr_08_TintedFT3;    // 0xB28110
		g.prim[9] = part_draw_prim::pr_09_TintedGT3;    // 0xB283B0
		g.prim[12] = part_draw_prim::pr_12_NormalLitG4; // 0xB28690
		g.prim[17] = part_draw_prim::pr_17_TintedG4;    // 0xB28B00
		g.prim[18] = part_draw_prim::pr_18_TintedFT4;   // 0xB28DD0
		g.prim[19] = part_draw_prim::pr_19_TintedGT4;   // 0xB290B0
	}
}
}

// ============================================================================================
// part draw_sprite
// ============================================================================================
// GF cinematic engine, part "draw_sprite": animated sprites (draw handler 5), the particle
// system (draw handler 6) with its particle VM, particle position types and the sprite
// orientation handlers. Original addresses are the Ifrit (201) clone.
//
// Particle record (0x50 bytes, pool = bone+0xB8: u16 count @+0, u16 @+8 (ws+0x34, unused
// here), records from +0x10):
//   +0x00 u16  cleared by particle op 0          +0x02 u16  script wait timer
//   +0x04 u8*  script cursor (0 = dead)          +0x08 u8*  frame list cursor (8-byte frames)
//   +0x0C u8*  frame list start                  +0x10 s32  scale (matrix diag = >>4)
//   +0x14 s32  X / radius                        +0x18 s32  Y / elevation angle
//   +0x1C s32  Z / azimuth angle                 +0x20 s16[4] velocity (of +0x10..+0x1C)
//   +0x28 s16[4] acceleration                    +0x30 u8*  current sprite quad list (0 = hidden)
//   +0x34 s16  frame timer                       +0x37 s8   position type (C().ptype index)
//   +0x38 u8[4] colour r,g,b,x (-> ws+0x98)      +0x3C s8[3] colour delta per tick
//   +0x40 u16  tpage                             +0x42 s16[3] origin x,y,z
//   +0x48 s16  clut word (-> ws+0x9C)            +0x4A u8   frame list kind (op 7 -> 0, op 8 -> 1)
//   +0x4C u32  sprite data base (frame offsets are relative to it)


namespace ff8fx
{
namespace gfc
{
namespace part_draw_sprite
{
	// 0x56C270 GTE_MatrixMultiply: TWO arguments, b(rotation) = a * b (the header's
	// x::MulMatrixGte declares a third, unused, argument)
	inline void *xl_MatMulGte(const void *a, void *b) { return x::f<void *(__cdecl *)(const void *, void *)>(0x56C270)(a, b); }

	// software GTE data registers SZ0..SZ3 (0x1CA8A10 data register file, regs 16..19)
	inline int32_t GteSZ(int i) { return MEM<int32_t>(0x1CA8A50 + 4 * i); }

	inline Mat4x3 *BillboardOfCurBone() { return BBMAT() + (U8(CUR(), 0xDE) & 3); }

	// ------------------------------------------------------------------------------------
	// sprite orientation handlers (C().spr, index = bone+0x1E); they leave the rotation of
	// the sprite quads in the GTE rotation matrix
	// ------------------------------------------------------------------------------------

	// 0xB29630 (SprOri 1): rotation = billboard matrix BBMAT[bone+0xDE & 3]
	static void __cdecl so_01_Billboard()
	{
		x::SetRotMatrix(BillboardOfCurBone());
	}

	// 0xB29650 (SprOri 2): billboard matrix scaled uniformly by bone+0xB8 << 4
	static void __cdecl so_02_BillboardScaled()
	{
		uint8_t *bb = (uint8_t *)BillboardOfCurBone();
		uint8_t *m = WS() + 0xD0;
		for (int i = 0; i < 20; i += 4) U32(m, i) = U32(bb, i);
		int32_t s = shl32(S32(CUR(), 0xB8), 4);
		S32(WS(), 0xF8) = s;
		S32(WS(), 0xF4) = s;
		S32(WS(), 0xF0) = s;
		x::ScaleMatrix(m, WS() + 0xF0);
		x::SetRotMatrix(m);
	}

	// 0xB296E0 (SprOri 7): parent matrix * rot(0x400, 0, bone+0x90), scaled by
	// (bone+0x8C << 4, bone+0x8E << 4, 0x1000)
	static void __cdecl so_07_ParentTiltedScaled()
	{
		uint8_t *a = WS() + 0xC0;
		Mat4x3 *par = blob::GetParentMatrix(U16(CUR(), 0x9C));
		uint8_t *sc = a + 0x10;
		S32(sc, 0) = shl32(S16(CUR(), 0x8C), 4);
		S32(a, 0x14) = shl32(S16(CUR(), 0x8E), 4);
		S32(a, 0x18) = 0x1000;
		U16(a, 0) = 0x400;
		U16(a, 2) = 0;
		U16(a, 4) = U16(CUR(), 0x90);
		uint8_t *m = WS() + 0xE0;
		x::RotMatrixFromAngles(a, m);
		xl_MatMulGte(par, m);
		pad_after_rot_mul(m, a); // ws+0xF2: vanilla stack word
		x::ScaleMatrix(m, sc);
		x::SetRotMatrix(m);
	}

	// 0xB29790 (SprOri 8): parent matrix * rot order 1 (0, bone+0x8E, bone+0x90), scaled by
	// (bone+0x8C << 4, bone+0x8C << 4, 0x1000)
	static void __cdecl so_08_ParentRotatedScaled()
	{
		uint8_t *a = WS() + 0x20;
		Mat4x3 *par = blob::GetParentMatrix(U16(CUR(), 0x9C));
		uint8_t *sc = a + 0x10;
		S32(sc, 0) = shl32(S16(CUR(), 0x8C), 4);
		S32(a, 0x14) = shl32(S16(CUR(), 0x8C), 4);
		S32(a, 0x18) = 0x1000;
		U16(a, 0) = 0;
		U16(a, 2) = U16(CUR(), 0x8E);
		U16(a, 4) = U16(CUR(), 0x90);
		Mat4x3 *m = blob::RotMatrixOrder(a, 1);
		xl_MatMulGte(par, m);
		x::ScaleMatrix(m, sc);
		x::SetRotMatrix(m);
	}

	// 0xB29830 (SprOri 9): billboard matrix * rot(0, 0, bone+0x90), scaled by
	// (bone+0x8C << 4, bone+0x8E << 4, 0x1000)
	static void __cdecl so_09_BillboardRolledScaled()
	{
		uint8_t *a = WS() + 0xC0;
		uint8_t *sc = a + 0x10;
		S32(sc, 0) = shl32(S16(CUR(), 0x8C), 4);
		S32(a, 0x14) = shl32(S16(CUR(), 0x8E), 4);
		S32(a, 0x18) = 0x1000;
		U16(a, 0) = 0;
		U16(a, 2) = 0;
		U16(a, 4) = U16(CUR(), 0x90);
		Mat4x3 *bb = BillboardOfCurBone();
		uint8_t *m = WS() + 0xE0;
		x::RotMatrixFromAngles(a, m);
		xl_MatMulGte(bb, m);
		pad_after_rot_mul(m, a); // ws+0xF2: vanilla stack word
		x::ScaleMatrix(m, sc);
		x::SetRotMatrix(m);
	}

	// ------------------------------------------------------------------------------------
	// 0xB29A40 EmitSpriteQuads: emits one POLY_FT4 (+ optional draw-mode word) per quad of the
	// sprite quad list ws+0x90 ({u32 count, then 20-byte quads}); the GTE rotation/translation
	// are already set. ws+0x94 = tpage override (bone+0xDE bit 2), ws+0x98 = flat colour.
	// Packet cursor = ctx+0x7C. Quad: s16 x,y, s16 scaleW, scaleH (4.12), +8 unused, u16 uv,
	// u8 w,h, u16 clut, u8 brightness (0x80 = use ws+0x98), +0x11 unused, u16 tpage.
	// Returns 0.
	// ------------------------------------------------------------------------------------
	static int32_t EmitSpriteQuads()
	{
		uint8_t *pkt = PTR(CTX(), 0x7C);
		uint8_t *v = WS() + 0xE0;                       // 4 SVECTORs (kept in esi)
		uint8_t *list = PTR(WS(), 0x90);
		uint32_t count = U32(list, 0);
		uint8_t *rec = list + 4;
		U16(v, 0xC) = 0;
		U16(v, 4) = 0;
		U16(v, 0x1C) = 0;
		U16(v, 0x14) = 0;
		if (count == 0)
		{
			PTR(CTX(), 0x7C) = pkt;
			return 0;
		}
		do
		{
			uint32_t bright = U8(rec, 0x10);
			if (bright == 0x80)
				U32(pkt, 4) = U32(WS(), 0x98) | 0x2C000000;
			else if (U8(CUR(), 0xDE) & 8)
				U32(pkt, 4) = U32(WS(), 0x98) | 0x2C000000;
			else
			{
				uint32_t abr = U32(CUR(), 0xCC) & 0x2000000;
				uint32_t c = bright | 0x2C00;
				c <<= 8;
				c |= bright;
				c <<= 8;
				c |= abr;
				c |= bright;
				U32(pkt, 4) = c;
			}
			U16(pkt, 0xE) = U16(rec, 0xE);
			uint32_t wh = U16(rec, 0xC);
			uint32_t uv = U16(rec, 0xA);
			uint32_t w = wh & 0xFF;
			uint32_t h = wh & 0xFF00;
			U16(pkt, 0xC) = (uint16_t)uv;
			U16(pkt, 0x14) = (uint16_t)(w + uv);
			U16(pkt, 0x1C) = (uint16_t)(h + uv);
			U16(pkt, 0x24) = (uint16_t)(h + w + uv);
			// width
			int32_t w16 = (int32_t)(w << 4);
			int32_t W = mul32(S16(rec, 4), w16) / 4096;
			int32_t x0 = sub32(w16 / 2, W / 2);
			x0 = add32(x0, S16(rec, 0));
			S16(v, 0x10) = (int16_t)x0;
			S16(v, 0) = (int16_t)x0;
			int32_t x1 = add32(x0, W);
			S16(v, 0x18) = (int16_t)x1;
			S16(v, 8) = (int16_t)x1;
			// height
			int32_t h16 = (int32_t)h >> 4;
			h16 &= (int32_t)0xFFFFFFF0;
			int32_t H = mul32(S16(rec, 6), h16) / 4096;
			int32_t y0 = sub32(S16(rec, 2), H / 2);
			y0 = add32(y0, h16 / 2);
			int32_t y1 = add32(H, y0);
			U16(v, 0x1C) = 0;
			U16(v, 0x14) = 0;
			U16(v, 0xC) = 0;
			U16(v, 4) = 0;
			S16(v, 0xA) = (int16_t)y0;
			S16(v, 2) = (int16_t)y0;
			S16(v, 0x1A) = (int16_t)y1;
			S16(v, 0x12) = (int16_t)y1;
			x::GteLoadV012(v, v + 8, v + 0x10);
			x::GteRTPT();
			x::GteReadSXY012(pkt + 8, pkt + 0x10, pkt + 0x18);
			x::GteAVSZ3();
			x::GteReadOTZ(WS() + 0xE0);                 // OTZ -> ws+0xE0 (overwrites v0.xy)
			x::GteLoadV0(v + 0x18);
			x::GteRTPS();
			x::GteReadSXY2(pkt + 0x20);
			x::GteReadOTZdiv4(WS() + 0xE4);             // SZ3 >> 2 -> ws+0xE4 (overwrites v0.z/pad)
			int32_t sz3 = GteSZ(3), sz2 = GteSZ(2), sz1 = GteSZ(1), sz0 = GteSZ(0);
			int32_t z = add32(S32(WS(), 0xE4), S32(WS(), 0xE0)) / 2;
			uint32_t ot = U32(RT(), 0x4C) + ((uint32_t)(z >> 2) << 2);
			x::InsertPrimDepthKeys(ot, pkt, sz0, sz1, sz2, sz3);
			uint16_t tp;
			if (U8(CUR(), 0xDE) & 4) tp = U16(WS(), 0x94);
			else tp = U16(rec, 0x12);
			U16(pkt, 0x16) = tp;
			uint32_t size;
			if ((tp & 0x60) == 0x20)
			{
				U8(pkt, 3) = 9;
				size = 0x28;
			}
			else
			{
				U8(pkt, 3) = 0xA;
				U32(pkt, 0x28) = 0xE1000220;
				size = 0x2C;
			}
			pkt += size;
			rec += 0x14;
		} while (--count != 0);
		PTR(CTX(), 0x7C) = pkt;
		return 0;
	}

	// ------------------------------------------------------------------------------------
	// 0xB298D0 (Draw 5) animated sprite: steps the frame list (bone+0xD0, 8-byte frames
	// {u32 quad list offset from bone+0xE4, u16 duration, s8 ctl: 0 next, 1 end (remove from
	// draw list, ws[0]--), 2 loop to bone+0xD4}) by bone+0xCA per tick unless frozen
	// (rt+0x45), then draws the current quad list at bone+0x94 in the parent frame
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_05_AnimatedSprite()
	{
		uint8_t skip = U8(RT(), 0x45);
		uint8_t *b = CUR();
		if (skip == 0)
		{
			int32_t step = S16(b, 0xCA);
			if (step >= 0)
			{
				S16(b, 0xDC) = (int16_t)(S16(b, 0xDC) - step);
				b = CUR();
				if (S16(b, 0xDC) <= 0)
				{
					uint8_t *fr = PTR(b, 0xD0);
					U32(b, 0xD8) = U32(b, 0xE4) + U32(fr, 0);
					b = CUR();
					S16(b, 0xDC) = (int16_t)(S16(b, 0xDC) + U16(fr, 4));
					uint8_t *cb = CUR();
					int32_t ctl = S8(fr, 6);
					uint32_t next = (uint32_t)fr;
					if (ctl != 0)
					{
						if (ctl == 1)
						{
							U8(cb, 0x1C) = 0;
							blob::DrawListRemove(U8(RT(), 0x42));
							S32(WS(), 0) = sub32(S32(WS(), 0), 1);
							return;
						}
						if (ctl == 2) next = U32(cb, 0xD4) - 8;
					}
					U32(cb, 0xD0) = next + 8;
					b = CUR();
				}
			}
		}
		Mat4x3 *m = blob::GetParentMatrix(U16(b, 0x9C));
		x::SetRotMatrix(m);
		x::SetTransVector(m);
		x::GteLoadV0(CUR() + 0x94);
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
		C().spr[U8(CUR(), 0x1E)]();
		U32(WS(), 0x90) = U32(CUR(), 0xD8);
		S32(WS(), 0x94) = S16(CUR(), 0x92);
		U32(WS(), 0x98) = U32(CUR(), 0xCC);
		S32(WS(), 0x9C) = S16(CUR(), 0x9A);
		EmitSpriteQuads();
	}

	// ------------------------------------------------------------------------------------
	// particle position types (C().ptype, index = particle+0x37): particle (ws+0x44) ->
	// local position SVECTOR ws+0xA0 (ws+0x38 / ws+0x3C = x / z bias, always 0 here)
	// ------------------------------------------------------------------------------------

	// 0xB2A2E0 (PartType 1) cartesian: (X >> 8 - bias x, Y >> 8, Z >> 8 - bias z)
	static void __cdecl pt_1_Cartesian()
	{
		uint8_t *w = WS();
		uint8_t *p = PTR(w, 0x44);
		S16(w, 0xA0) = (int16_t)((S32(p, 0x14) >> 8) - U16(w, 0x38));
		S16(WS(), 0xA2) = (int16_t)(S32(p, 0x18) >> 8);
		w = WS();
		S16(w, 0xA4) = (int16_t)((S32(p, 0x1C) >> 8) - U16(w, 0x3C));
	}

	// 0xB2A330 (PartType 2) cylindrical around the origin +0x42: radius +0x14 >> 4, angle
	// +0x1C >> 4, height +0x18 >> 8
	static void __cdecl pt_2_Cylindrical()
	{
		uint8_t *p = PTR(WS(), 0x44);
		int32_t ang = (S32(p, 0x1C) >> 4) & 0xFFF;
		int32_t r = S32(p, 0x14) >> 4;
		int32_t c = x::Cos(ang);
		c = mul32(c, r) >> 16;
		int32_t s = x::Sin(ang);
		s = mul32(s, r) >> 16;
		uint8_t *w = WS();
		S16(w, 0xA0) = (int16_t)((uint16_t)(U16(p, 0x42) - U16(w, 0x38)) + s);
		S16(WS(), 0xA2) = (int16_t)((S32(p, 0x18) >> 8) + U16(p, 0x44));
		w = WS();
		S16(w, 0xA4) = (int16_t)((uint16_t)(U16(p, 0x46) - U16(w, 0x3C)) + c);
	}

	// 0xB2A3C0 (PartType 3) on the ellipsoid of bone (cur bone+0xB6): extents bone+0x94/96/98,
	// elevation +0x18 >> 4, azimuth +0x1C >> 4, around the origin +0x42
	static void __cdecl pt_3_Ellipsoid()
	{
		uint8_t *p = PTR(WS(), 0x44);
		uint8_t *bn = blob::GetBone(S16(CUR(), 0xB6));
		int32_t a1 = (S32(p, 0x18) >> 4) & 0xFFF;
		int32_t t = x::Sin(a1);
		t = mul32(t, S16(bn, 0x98)) >> 12;
		S16(WS(), 0xA4) = (int16_t)(U16(p, 0x46) + t);
		int32_t R = S16(bn, 0x98);
		int32_t cr = x::Cos(a1);
		cr = mul32(cr, R);
		int32_t A = mul32(S16(bn, 0x94), cr) / R;   // idiv 0xB2A442 (R = 0 faults, as vanilla)
		A >>= 4;
		int32_t a2 = (S32(p, 0x1C) >> 4) & 0xFFF;
		int32_t xx = x::Cos(a2);
		xx = mul32(xx, A) >> 20;
		uint8_t *w = WS();
		S16(w, 0xA0) = (int16_t)((uint16_t)(U16(p, 0x42) - U16(w, 0x38)) + xx);
		int32_t B = mul32(S16(bn, 0x96), cr) / S16(bn, 0x98);   // idiv 0xB2A48B
		B >>= 4;
		int32_t yy = x::Sin(a2);
		yy = mul32(yy, B) >> 20;
		w = WS();
		S16(w, 0xA2) = (int16_t)((uint16_t)(U16(p, 0x44) - U16(w, 0x3C)) + yy);
	}

	// ------------------------------------------------------------------------------------
	// particle VM (C().pop, index = op word & 0xFF; op word in rt+0x4A, its high byte = rt+0x4B;
	// STREAM = particle script cursor; an op ends the particle's tick by setting rt+0x3E = wait)
	// ------------------------------------------------------------------------------------

	// 0xB2A4C0 (PartOp 0) End: kill the particle (+0 = 0, script = 0, sprite = 0, frames = 0)
	static void __cdecl po_00_End()
	{
		uint8_t *p = PTR(WS(), 0x44);
		U16(p, 0) = 0;
		U16(RT(), 0x3E) = U16(WS(), 0x24);
		STREAM() = nullptr;
		U32(p, 0x30) = 0;
		U32(p, 8) = 0;
	}

	// 0xB2A4F0 (PartOp 1) Wait: rt+0x3E = hi << 7
	static void __cdecl po_01_Wait()
	{
		uint8_t *rt = RT();
		U16(rt, 0x3E) = (uint16_t)((uint32_t)U8(rt, 0x4B) << 7);
		STREAM() += 2;
	}

	// 0xB2A510 (PartOp 2) Jump: STREAM += s16 [+2]
	static void __cdecl po_02_Jump()
	{
		uint8_t *s = STREAM();
		STREAM() = s + S16(s, 2);
	}

	// 0xB2A530 (PartOp 3) AddRandomWait: wait timer += Rand73(hi) << 7 (no yield)
	static void __cdecl po_03_AddRandomWait()
	{
		uint8_t *p = PTR(WS(), 0x44);
		int32_t r = blob::Rand73(U16(RT(), 0x4A) >> 8);
		U16(p, 2) = (uint16_t)(U16(p, 2) + (uint16_t)shl32(r, 7));
		STREAM() += 2;
	}

	// op bits 8-9 select the vector: 0 = position (4 x s32, value << 8), 0x100 = velocity,
	// else acceleration (4 x s16); bits 15..12 = component mask (one s16 operand per set bit)
	static inline void SelVector(uint32_t op, uint8_t *p, uint8_t *&dst, int &esz, int &sh)
	{
		uint32_t sel = op & 0x300;
		if (sel == 0) { dst = p + 0x10; esz = 4; sh = 8; }
		else { dst = sel == 0x100 ? p + 0x20 : p + 0x28; esz = 2; sh = 0; }
	}

	// 0xB2A570 (PartOp 4) SetVector: masked components = operand (0x7654 = keep)
	static void __cdecl po_04_SetVector()
	{
		uint32_t mask = U16(RT(), 0x4A);
		uint8_t *p = PTR(WS(), 0x44);
		uint8_t *dst; int esz, sh;
		SelVector(mask, p, dst, esz, sh);
		uint8_t *s = STREAM() + 2;
		for (int off = 0; off < esz * 4; off += esz)
		{
			if ((int16_t)mask < 0)
			{
				int32_t v = S16(s, 0);
				if (v != 0x7654)
				{
					if (esz == 2) S16(dst, off) = (int16_t)shl32(v, sh);
					else S32(dst, off) = shl32(v, sh);
				}
				s += 2;
			}
			mask <<= 1;
		}
		STREAM() = s;
	}

	// 0xB2A630 (PartOp 5) AddRandomVector: masked components += Rand73(operand) (0 = none)
	static void __cdecl po_05_AddRandomVector()
	{
		uint32_t mask = U16(RT(), 0x4A);
		uint8_t *p = PTR(WS(), 0x44);
		uint8_t *dst; int esz, sh;
		SelVector(mask, p, dst, esz, sh);
		uint8_t *s = STREAM() + 2;
		for (int off = 0; off < esz * 4; off += esz)
		{
			if ((int16_t)mask < 0)
			{
				int32_t v = S16(s, 0);
				if (v != 0)
				{
					int32_t r = blob::Rand73(v);
					if (esz == 2) S16(dst, off) = (int16_t)(S16(dst, off) + (int16_t)shl32(r, sh));
					else S32(dst, off) = add32(S32(dst, off), shl32(r, sh));
				}
				s += 2;
			}
			mask <<= 1;
		}
		STREAM() = s;
	}

	// 0xB2A6F0 (PartOp 6) AddPosition: masked position components += operand << 8 (0x7654 = none)
	static void __cdecl po_06_AddPosition()
	{
		uint8_t *d = PTR(WS(), 0x44) + 0x10;
		uint16_t mask = U16(RT(), 0x4A);
		uint8_t *s = STREAM() + 2;
		for (int off = 0; off < 0x10; off += 4)
		{
			if ((int16_t)mask < 0)
			{
				int32_t v = S16(s, 0);
				if (v != 0x7654) S32(d, off) = add32(S32(d, off), shl32(v, 8));
				s += 2;
			}
			mask = (uint16_t)(mask << 1);
		}
		STREAM() = s;
	}

	// 0xB2A820 (PartOp 7, 8) SetFrameList: frame list = object hi (ObjectPtrWs), kind = op - 7,
	// sprite = first frame, frame timer = cur bone+0xCA
	static void __cdecl po_07_SetFrameList()
	{
		uint8_t *p = PTR(WS(), 0x44);
		uint32_t op = U16(RT(), 0x4A);
		U8(p, 0x4A) = (uint8_t)((uint8_t)op - 7);
		uint8_t *fl = blob::ObjectPtrWs((int32_t)op >> 8);
		uint32_t base = U32(WS(), 0xFC);
		PTR(p, 8) = fl;
		U32(p, 0x4C) = base;
		PTR(p, 0xC) = fl;
		U32(p, 0x30) = U32(WS(), 0xFC) + U32(fl, 0);
		U16(p, 0x34) = U16(CUR(), 0xCA);
		STREAM() += 2;
	}

	// 0xB2A750 (PartOp 9) SetOriginHere: origin = current type position (+ bias)
	static void __cdecl po_09_SetOriginHere()
	{
		uint8_t *p = PTR(WS(), 0x44);
		int32_t t = S8(p, 0x37);
		U16(p, 0x42) = 0;
		U16(p, 0x44) = 0;
		U16(p, 0x46) = 0;
		C().ptype[t]();
		uint8_t *w = WS();
		U16(p, 0x42) = (uint16_t)(U16(w, 0xA0) + U16(w, 0x38));
		U16(p, 0x44) = U16(WS(), 0xA2);
		w = WS();
		U16(p, 0x46) = (uint16_t)(U16(w, 0xA4) + U16(w, 0x3C));
		STREAM() += 2;
	}

	// 0xB2A7C0 (PartOp 10) TypeCylindrical: type = 2
	static void __cdecl po_10_TypeCylindrical()
	{
		U8(PTR(WS(), 0x44), 0x37) = 2;
		STREAM() += 2;
	}

	// 0xB2A890 (PartOp 11) SetColor: r = hi, g = [+2] low, b = [+2] high
	static void __cdecl po_11_SetColor()
	{
		uint8_t *p = PTR(WS(), 0x44);
		U8(p, 0x38) = U8(RT(), 0x4B);
		uint32_t v = U16(STREAM(), 2);
		U8(p, 0x39) = (uint8_t)v;
		U8(p, 0x3A) = (uint8_t)(v >> 8);
		STREAM() += 4;
	}

	// 0xB2A8D0 (PartOp 12) SetColorDelta: dr = hi, dg = [+2] low, db = [+2] high
	static void __cdecl po_12_SetColorDelta()
	{
		uint8_t *p = PTR(WS(), 0x44);
		U8(p, 0x3C) = U8(RT(), 0x4B);
		uint32_t v = U16(STREAM(), 2);
		U8(p, 0x3D) = (uint8_t)v;
		U8(p, 0x3E) = (uint8_t)(v >> 8);
		STREAM() += 4;
	}

	// 0xB2A7E0 (PartOp 13) TypeEllipsoid: type = 3, cur bone+0xB6 = bone ref [+2]
	static void __cdecl po_13_TypeEllipsoid()
	{
		U8(PTR(WS(), 0x44), 0x37) = 3;
		U16(CUR(), 0xB6) = U16(STREAM(), 2);
		STREAM() += 4;
	}

	// 0xB2A910 (PartOp 14) AimAtBone: azimuth (+0x1C) = angle from (X, Z) to bone [+2]
	// (bone+0x94, bone+0x98) << 4
	static void __cdecl po_14_AimAtBone()
	{
		uint8_t *p = PTR(WS(), 0x44);
		uint8_t *bn = blob::GetBone(U16(STREAM(), 2));
		int32_t tz = shl32(S16(bn, 0x98), 8);
		int32_t tx = shl32(S16(bn, 0x94), 8);
		int32_t r = blob::Blob_B66B30(S32(p, 0x14), S32(p, 0x1C), tx, tz);
		S32(p, 0x1C) = shl32(r, 4);
		STREAM() += 4;
	}

	// 0xB2A970 (PartOp 15) RandomBranch: |Rand73(0x100)| <= s16 [+2] ? STREAM += s16 [+4] : 6
	static void __cdecl po_15_RandomBranch()
	{
		int32_t r = blob::Rand73(0x100);
		if (r < 0) r = (int32_t)(0u - (uint32_t)r);
		uint8_t *s = STREAM();
		if (r > S16(s, 2)) STREAM() = s + 6;
		else STREAM() = s + S16(s, 4);
	}

	// 0xB2A9B0 (PartOp 16) SetFrame: frame index = hi (bit 7: Rand74(hi & 0x7F))
	static void __cdecl po_16_SetFrame()
	{
		uint8_t *p = PTR(WS(), 0x44);
		uint32_t v = (uint32_t)U16(RT(), 0x4A) >> 8;
		if (v & 0x80) v = (uint32_t)blob::Rand74((int32_t)(v & 0x7F));
		uint32_t f = U32(p, 0xC) + v * 8;
		U32(p, 8) = f;
		U32(p, 0x30) = U32(p, 0x4C) + U32((void *)f, 0);
		U16(p, 0x34) = U16((void *)f, 4);
		STREAM() += 2;
	}

	// 0xB2AA00 (PartOp 17) HoldFrame: frame timer = 0x7F80
	static void __cdecl po_17_HoldFrame()
	{
		U16(PTR(WS(), 0x44), 0x34) = 0x7F80;
		STREAM() += 2;
	}

	// 0xB2AA20 (PartOp 18) RandomizeInBone: X/Y/Z (op bits 14..12) += Rand73(bone [+2]
	// extent +0x94/+0x96/+0x98) << 8
	static void __cdecl po_18_RandomizeInBone()
	{
		uint8_t *bn = blob::GetBone(S16(STREAM(), 2));
		uint8_t *ext = bn + 0x94;
		uint16_t mask = (uint16_t)(U16(RT(), 0x4A) << 1);
		uint8_t *q = PTR(WS(), 0x44) + 0x14;
		for (int k = 3; k != 0; k--)
		{
			if ((int16_t)mask < 0)
			{
				int32_t v = S16(ext, 0);
				if (v != 0)
				{
					int32_t r = blob::Rand73(v);
					S32(q, 0) = add32(S32(q, 0), shl32(r, 8));
				}
			}
			q += 4;
			ext += 2;
			mask = (uint16_t)(mask << 1);
		}
		STREAM() += 4;
	}

	// 0xB2AAB0 (PartOp 19) SetTexture: tpage (+0x40) and clut (+0x48) of texture hi
	static void __cdecl po_19_SetTexture()
	{
		uint32_t id = (uint32_t)U16(RT(), 0x4A) >> 8;
		uint8_t *p = PTR(WS(), 0x44);
		blob::TexInfo_B665C0((int32_t)id);
		U16(p, 0x40) = U16(WS(), 0xFC);
		U16(p, 0x48) = (uint16_t)blob::ClutWord_B66640((int32_t)id);
		STREAM() += 2;
	}

	// 0xB2AB00 (PartOp 20) CopyFromBone: bit 15 set: scale/X/Y/Z (bits 15..12) = bone [+2]
	// +0x8C.. << 8; else X/Y/Z/+0x20 (bits 14..11) = bone +0x94.. << 8
	static void __cdecl po_20_CopyFromBone()
	{
		uint8_t *p = PTR(WS(), 0x44);
		uint8_t *bn = blob::GetBone(S16(STREAM(), 2));
		uint16_t mask = U16(RT(), 0x4A);
		uint8_t *dst, *src;
		if (mask & 0x8000) { dst = p + 0x10; src = bn + 0x8C; }
		else { dst = p + 0x14; src = bn + 0x94; mask = (uint16_t)(mask << 1); }
		for (int off = 0; off < 0x10; off += 4)
		{
			// else-branch bit 11 writes the dword +0x20 (velocity x AND y), as vanilla
			if ((int16_t)mask < 0) S32(dst, off) = shl32(S16(src, 0), 8);
			src += 2;
			mask = (uint16_t)(mask << 1);
		}
		STREAM() += 4;
	}

	// 0xB2AB70 (PartOp 21) CopyVelFromBone: velocity +0x22/+0x24/+0x26 (bits 14..12) =
	// bone [+2] +0x94/+0x96/+0x98
	static void __cdecl po_21_CopyVelFromBone()
	{
		uint8_t *q = PTR(WS(), 0x44) + 0x22;
		uint8_t *bn = blob::GetBone(S16(STREAM(), 2));
		uint8_t *src = bn + 0x94;
		uint16_t mask = (uint16_t)(U16(RT(), 0x4A) << 1);
		for (int k = 3; k != 0; k--)
		{
			if ((int16_t)mask < 0) U16(q, 0) = U16(src, 0);
			mask = (uint16_t)(mask << 1);
			q += 2;
			src += 2;
		}
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// 0xB29F20 (Draw 6) particle system: for every live particle (script != 0) of the pool
	// bone+0xB8: run its script (unless waiting), integrate vel += acc, pos += (vel) << 4,
	// position type, colour fade, frame list; then draw its sprite at the type position in
	// the parent frame, scaled by +0x10 >> 4. Frozen (rt+0x45): position type + draw only.
	// Held frames (g_held, frozen path forced): the particle is drawn at pos + the next
	// integration step's position delta * num/den, on a local copy (the pool is not written).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_06_ParticleSystem()
	{
		U16(RT(), 0x3E) = 0;
		S32(WS(), 0x38) = 0;
		S32(WS(), 0x3C) = 0;
		S32(WS(), 0x24) = S16(CUR(), 0xC8);
		S32(WS(), 0x28) = S16(CUR(), 0xCA);
		Mat4x3 *par = blob::GetParentMatrix(U16(CUR(), 0x9C));
		PTR(WS(), 0x30) = (uint8_t *)par;
		uint8_t *pool = PTR(CUR(), 0xB8);
		uint8_t *p = pool + 0x10;
		PTR(WS(), 0x44) = p;
		S32(WS(), 0x20) = U16(pool, 0);
		S32(WS(), 0x34) = U16(pool, 8);
		if (S32(WS(), 0x20) <= 0) return;
		const bool held_adv = g_held.active && g_held.real_skip == 0 && g_held.den > 0;
		do
		{
			S32(WS(), 0x40) = 0;
			uint8_t *scr = PTR(p, 4);
			if (scr == nullptr) goto next;
			{
				uint8_t *q = p;     // record the draw reads (a local copy on held frames)
				uint8_t loc[0x50];
				STREAM() = scr;
				if (U8(RT(), 0x45) == 0)
				{
					// --- script ---
					uint32_t wait = U16(p, 2);
					if ((int32_t)wait > 0)
					{
						U16(p, 2) = (uint16_t)(wait - (uint32_t)S32(WS(), 0x24));
					}
					else
					{
						uint8_t *rt = RT();
						int32_t op = S16(scr, 0);
						S16(rt, 0x4A) = (int16_t)op;
						C().pop[op & 0xFF]();
						while (U16(RT(), 0x3E) == 0)
						{
							rt = RT();
							op = S16(STREAM(), 0);
							S16(rt, 0x4A) = (int16_t)op;
							C().pop[op & 0xFF]();
						}
						rt = RT();
						uint16_t d = (uint16_t)(U16(rt, 0x3E) - (uint16_t)S32(WS(), 0x24));
						U16(p, 2) = (uint16_t)(U16(p, 2) + d);
						U16(RT(), 0x3E) = 0;
						PTR(p, 4) = STREAM();
					}
					// --- integration (the 32-bit sum, not the stored s16, is shifted) ---
					for (int k = 0; k < 4; k++)
					{
						int32_t s = S16(p, 0x28 + 2 * k) + S16(p, 0x20 + 2 * k);
						S16(p, 0x20 + 2 * k) = (int16_t)s;
						S32(p, 0x10 + 4 * k) = add32(S32(p, 0x10 + 4 * k), shl32(s, 4));
					}
					C().ptype[S8(p, 0x37)]();
					// --- colour fade, clamped 0..255 ---
					for (int k = 0; k < 3; k++)
					{
						int32_t c = S8(p, 0x3C + k) + U8(p, 0x38 + k);
						if (c >= 0x100) c = 0xFF;
						else if (c <= 0) c = 0;
						U8(p, 0x38 + k) = (uint8_t)c;
					}
					// --- frame list ---
					if (U32(p, 8) != 0)
					{
						int32_t t = S16(p, 0x34);
						bool step = true;
						if (t > 0)
						{
							t = sub32(t, S32(WS(), 0x28));
							S16(p, 0x34) = (int16_t)t;
							if (t > 0) step = false;
						}
						if (step)
						{
							uint32_t f = U32(p, 8);
							U32(p, 0x30) = U32(p, 0x4C) + U32((void *)f, 0);
							U16(p, 0x34) = (uint16_t)(U16(p, 0x34) + U16((void *)f, 4));
							int32_t ctl = S8((void *)f, 6);
							if (ctl == 1)
							{
								U32(p, 4) = 0;
								U32(p, 0x30) = 0;
								U32(p, 8) = f;
							}
							else
							{
								if (ctl == 2) f = U32(p, 0xC) - 8;
								U32(p, 8) = f + 8;
							}
						}
					}
				}
				else
				{
					if (U32(p, 0x30) == 0) goto next;
					if (held_adv)
					{
						// held refinement: position = pos + next tick's integration delta
						// ((vel + acc) << 4, the deterministic part) * num / den
						memcpy(loc, p, 0x50);
						for (int k = 0; k < 4; k++)
						{
							int32_t s = S16(p, 0x28 + 2 * k) + S16(p, 0x20 + 2 * k);
							int32_t d = (int32_t)((int64_t)shl32(s, 4) * g_held.num / g_held.den);
							S32(loc, 0x10 + 4 * k) = add32(S32(p, 0x10 + 4 * k), d);
						}
						q = loc;
						PTR(WS(), 0x44) = loc;
						C().ptype[S8(q, 0x37)]();
						PTR(WS(), 0x44) = p;
					}
					else
						C().ptype[S8(p, 0x37)]();
				}
				if (U32(q, 0x30) == 0) goto next;
				// --- draw ---
				{
					uint8_t *m = PTR(WS(), 0x30);
					x::SetRotMatrix(m);
					x::SetTransVector(m);
					x::GteLoadV0(WS() + 0xA0);
					x::GteMVMVA_RotV0Tr();
					x::Gte_45E580();
					int32_t sc = S32(q, 0x10) >> 4;
					S32(WS(), 0xA8) = sc;
					S32(WS(), 0xA0) = sc;
					S32(WS(), 0xB0) = 0;
					S32(WS(), 0xAC) = 0;
					S32(WS(), 0xA4) = 0;
					x::SetRotMatrix(WS() + 0xA0);
					if (S32(WS(), 0x40) != 0) x::Matrix_56C2C0(BBMAT() + 1);
					U32(WS(), 0x90) = U32(q, 0x30);
					U32(WS(), 0x94) = U16(q, 0x40);
					U32(WS(), 0x98) = U32(q, 0x38);
					S32(WS(), 0x9C) = S16(q, 0x48);
					EmitSpriteQuads();
				}
			}
		next:
			p += 0x50;
			PTR(WS(), 0x44) = p;
			S32(WS(), 0x20) = sub32(S32(WS(), 0x20), 1);
		} while (S32(WS(), 0x20) > 0);
	}
}

	void fill_draw_sprite(Generic &g)
	{
		g.spr[1] = part_draw_sprite::so_01_Billboard;
		g.spr[2] = part_draw_sprite::so_02_BillboardScaled;
		g.spr[7] = part_draw_sprite::so_07_ParentTiltedScaled;
		g.spr[8] = part_draw_sprite::so_08_ParentRotatedScaled;
		g.spr[9] = part_draw_sprite::so_09_BillboardRolledScaled;
		g.draw[5] = part_draw_sprite::dh_05_AnimatedSprite;
		g.draw[6] = part_draw_sprite::dh_06_ParticleSystem;
		g.ptype[1] = part_draw_sprite::pt_1_Cartesian;
		g.ptype[2] = part_draw_sprite::pt_2_Cylindrical;
		g.ptype[3] = part_draw_sprite::pt_3_Ellipsoid;
		g.pop[0] = part_draw_sprite::po_00_End;
		g.pop[1] = part_draw_sprite::po_01_Wait;
		g.pop[2] = part_draw_sprite::po_02_Jump;
		g.pop[3] = part_draw_sprite::po_03_AddRandomWait;
		g.pop[4] = part_draw_sprite::po_04_SetVector;
		g.pop[5] = part_draw_sprite::po_05_AddRandomVector;
		g.pop[6] = part_draw_sprite::po_06_AddPosition;
		g.pop[7] = part_draw_sprite::po_07_SetFrameList;
		g.pop[8] = part_draw_sprite::po_07_SetFrameList;
		g.pop[9] = part_draw_sprite::po_09_SetOriginHere;
		g.pop[10] = part_draw_sprite::po_10_TypeCylindrical;
		g.pop[11] = part_draw_sprite::po_11_SetColor;
		g.pop[12] = part_draw_sprite::po_12_SetColorDelta;
		g.pop[13] = part_draw_sprite::po_13_TypeEllipsoid;
		g.pop[14] = part_draw_sprite::po_14_AimAtBone;
		g.pop[15] = part_draw_sprite::po_15_RandomBranch;
		g.pop[16] = part_draw_sprite::po_16_SetFrame;
		g.pop[17] = part_draw_sprite::po_17_HoldFrame;
		g.pop[18] = part_draw_sprite::po_18_RandomizeInBone;
		g.pop[19] = part_draw_sprite::po_19_SetTexture;
		g.pop[20] = part_draw_sprite::po_20_CopyFromBone;
		g.pop[21] = part_draw_sprite::po_21_CopyVelFromBone;
		// pop[22] = 0xB2ABD0 is a bare `ret` (init_clone -> no-op), pop[23] = 0 (never dispatched)
	}
}
}

// ==== BEGIN clone-shared handler parts (generated by integrate.py) ====
// ============================================================================================
// part shared_ribbon
// ============================================================================================
namespace ff8fx
{
namespace gfc
{
namespace part_shared_ribbon
{
	// ------------------------------------------------------------------------------------
	// engine functions not wrapped by gfc_engine.h (pure GTE / math, no side effect outside
	// the GTE register file and the given buffers)
	// ------------------------------------------------------------------------------------
	// 0x56BEC0 integer square root
	static inline int32_t xl_ISqrt(int32_t v) { return x::f<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	// 0x56C850 RotTrans: LoadV0(v); MVMVA(R*V0+TR); IR1..3 -> out (3 x s16); FLAG -> *flag
	static inline void xl_RotTrans(const void *v, void *out, void *flag) { x::f<void (__cdecl *)(const void *, void *, void *)>(0x56C850)(v, out, flag); }
	// 0x56C880 TransformWorldCoordinateToProjectedSpace (RotTransPers): LoadV0(v); RTPS;
	// SXY2 -> *sxy; *p = interpolation value; *flag = FLAG; returns OTZ (SZ3 / 4)
	static inline int32_t xl_RotTransPers(const void *v, void *sxy, void *p, void *flag) { return x::f<int32_t (__cdecl *)(const void *, void *, void *, void *)>(0x56C880)(v, sxy, p, flag); }
	// 0x45E150 set_dword_1CA8A30: GTE IR0 = v
	static inline void xl_GteSetIR0(int32_t v) { x::f<void (__cdecl *)(int32_t)>(0x45E150)(v); }
	// 0x45E0E0: IR1..3 = 3 x u8 (zero-extended) from src
	static inline void xl_GteLoadRGB(const void *src) { x::f<void (__cdecl *)(const void *)>(0x45E0E0)(src); }
	// 0x45E9D0 GPF: MAC/IR = IR0 * IR
	static inline void xl_GteGPF() { x::f<void (__cdecl *)()>(0x45E9D0)(); }
	// 0x45E410: IR1..3 low bytes -> 3 x u8 at dst
	static inline void xl_GteStoreRGB(void *dst) { x::f<void (__cdecl *)(void *)>(0x45E410)(dst); }
	// blob 0xB66E50 look-at matrix (a -> b) into out; RETURNS 0 or -1 (-1 when the second
	// angle lies in (0x400, 0xC00), i.e. the direction points "backwards"); the header wrapper
	// blob::LookAt_B66E50 drops the return value, which this handler uses.
	static inline int32_t xl_LookAt_B66E50(const void *a, const void *b, void *out) { return x::f<int32_t (__cdecl *)(const void *, const void *, void *)>(0xB66E50)(a, b, out); }

	// engine block: 8 zero dwords at 0x27971E4 (the origin passed to the look-at)
	static inline uint8_t *ZEROVEC() { return (uint8_t *)0x27971E4; }
	// software GTE data register 19 (SZ3) of the emulated GTE (data file 0x1CA8A10 + 19*4)
	static inline uint16_t &GTE_SZ3_W() { return MEM<uint16_t>(0x1CA8A5C); }
	// software GTE data register 0 (VXY0, packed) - read / restored only for held frames
	static inline uint32_t &GTE_VXY0() { return MEM<uint32_t>(0x1CA8A10); }

	// ------------------------------------------------------------------------------------
	// 0xB1AF60 TurnToward(cur, target, rate): 12-bit angle `cur` turned toward `target` by at
	// most `rate` the short way round; returns the new angle & 0xFFF (snaps onto target when
	// the step would overshoot).
	// ------------------------------------------------------------------------------------
	static int32_t Ribbon_B1AF60_TurnToward(int32_t a, int32_t t, int32_t rate)
	{
		int32_t eax = a & 0xFFF;
		int32_t ecx = t & 0xFFF;
		int32_t edx = ecx;
		if (eax > ecx) edx = ecx + 0x1000;
		edx = edx - eax;
		if (edx < 0x800)
		{
			eax = add32(eax, rate) & 0xFFF;
			if (eax > ecx) ecx += 0x1000;
			edx = ecx - eax;
			if (edx < 0x800) return eax & 0xFFF;
			eax = ecx;
			return eax & 0xFFF;
		}
		eax = sub32(eax, rate) & 0xFFF;
		if (eax > ecx) ecx += 0x1000;
		edx = ecx - eax;
		if (edx < 0x800) eax = ecx;
		return eax & 0xFFF;
	}

	// ------------------------------------------------------------------------------------
	// 0xB1AE60 SteerToward(target, prev, new) (homing mode): new.yaw (+6) = prev.yaw turned
	// toward the XZ angle prev->target by the node's turn rate (bone+0x8E), new.pitch (+0xE)
	// likewise with the YZ angle; ws+0xE0 = direction (sin yaw, sin pitch, cos yaw) (s16),
	// ws+0xF0 = prev + direction. Returns the last sign-extended sine (unused by the caller).
	// ------------------------------------------------------------------------------------
	static int32_t Ribbon_B1AE60_SteerToward(const uint8_t *target, const uint8_t *prev, uint8_t *nw)
	{
		int32_t py = S16(prev, 2);                        // [ebp+0xC]
		int32_t pz = S16(prev, 4);                        // edi
		int32_t ty = S16(target, 2);                      // [ebp+8]
		int32_t tx = S16(target, 0);                      // edx
		int32_t px = S16(prev, 0);                        // ecx, [ebp-4]
		int32_t tz = S16(target, 4);                      // ebx
		int32_t yaw = blob::Blob_B66B30(px, pz, tx, tz);
		U16(nw, 6) = (uint16_t)Ribbon_B1AF60_TurnToward(S16(prev, 6), yaw, S16(CUR(), 0x8E));
		int32_t pitch = blob::Blob_B66B30(py, pz, ty, tz);
		int32_t np = Ribbon_B1AF60_TurnToward(S16(prev, 0xE), pitch, S16(CUR(), 0x8E));
		U16(nw, 0xE) = (uint16_t)np;
		uint8_t *w = WS();
		uint8_t *o = w + 0xF0;                            // esi
		uint8_t *d = w + 0xE0;                            // ebx
		int32_t s = x::Sin((int16_t)np);                  // movsx eax, ax
		s = shl32(s, 12) >> 12;
		U16(d, 2) = (uint16_t)s;
		U16(o, 2) = (uint16_t)add32(py, s);
		int32_t ny = S16(nw, 6);                          // [ebp+0x10]
		int32_t c = x::Cos(ny);
		c = shl32(c, 12) >> 12;
		pz = add32(pz, c);
		U16(d, 4) = (uint16_t)c;
		U16(o, 4) = (uint16_t)pz;
		s = x::Sin(ny);
		s = shl32(s, 12) >> 12;
		U16(d, 0) = (uint16_t)s;
		U16(o, 0) = (uint16_t)add32(px, s);
		return s;
	}

	// ------------------------------------------------------------------------------------
	// 0xB1AFE0 FollowDelta(pos, prev, new) (follow mode): ws+0xF0 = pos (3 x s16), ws+0xE0 =
	// pos - prev (s16); returns isqrt(|pos - prev|^2). The third argument is not read.
	// ------------------------------------------------------------------------------------
	static int32_t Ribbon_B1AFE0_FollowDelta(const uint8_t *pos, const uint8_t *prev)
	{
		int32_t pz = S16(prev, 4);                        // [ebp-4]
		int32_t py = S16(prev, 2);                        // [ebp+0xC]
		int32_t px = S16(prev, 0);                        // ebx, [ebp-8]
		uint8_t *w = WS();
		uint8_t *o = w + 0xF0;
		uint8_t *d = w + 0xE0;
		int32_t ax = S16(pos, 0), cx = S16(pos, 2), dx = S16(pos, 4);
		U16(o, 0) = (uint16_t)ax;
		U16(o, 2) = (uint16_t)cx;
		U16(o, 4) = (uint16_t)dx;
		U16(d, 0) = (uint16_t)sub32(ax, px);
		U16(d, 2) = (uint16_t)sub32(cx, py);
		U16(d, 4) = (uint16_t)sub32(dx, pz);
		int32_t ez = sub32(dx, pz), ey = sub32(cx, py), ex = sub32(ax, px);
		int32_t sq = add32(add32(mul32(ez, ez), mul32(ey, ey)), mul32(ex, ex));
		return xl_ISqrt(sq);
	}

	// ------------------------------------------------------------------------------------
	// 0xB1B080 BuildCrossSection(w, depth, pts, flip): the 9 points of a ring entry, each
	// RotTrans'ed (current GTE R/TR) from a local offset held in ws+0xF0: (0,0,-d), (+-w1,0,-d),
	// (+-3*w1/8,0,-d), (0,+-w1,-d), (0,+-3*w1/8,-d), with w1 = flip ? -w : w. The entry's pad
	// words +6 / +0xE (yaw / pitch) are saved and put back. GTE FLAG -> ws+0xFC.
	// ------------------------------------------------------------------------------------
	static void Ribbon_B1B080_BuildCrossSection(int32_t w, int32_t dep, uint8_t *pts, int32_t flip)
	{
		uint8_t *v = WS() + 0xF0;                         // esi
		int32_t ebx = w;
		uint16_t save6 = U16(pts, 6);                     // [ebp-4]
		uint16_t saveE = U16(pts, 0xE);                   // [ebp-8]
		if (flip != 0) ebx = sub32(0, ebx);               // not / inc
		int32_t wfl = ebx;                                // [ebp+8]
		int32_t nd = sub32(0, dep);                       // [ebp+0x10]
		U16(v, 0) = 0;
		U16(v, 2) = 0;
		U16(v, 4) = (uint16_t)nd;
		xl_RotTrans(v, pts, WS() + 0xFC);
		U16(v, 0) = (uint16_t)ebx;
		U16(v, 2) = 0;
		U16(v, 4) = (uint16_t)nd;
		xl_RotTrans(v, pts + 8, WS() + 0xFC);
		int32_t nw = sub32(0, ebx);                       // [ebp+0x14]
		U16(v, 0) = (uint16_t)nw;
		xl_RotTrans(v, pts + 0x10, WS() + 0xFC);
		int32_t t = shl32(mul32(ebx, 3), 9);
		t = add32(t, (t >> 31) & 0xFFF);                  // cdq; and edx, 0xFFF; add
		ebx = t >> 12;                                    // 3*w1/8 (toward zero)
		U16(v, 0) = (uint16_t)ebx;
		xl_RotTrans(v, pts + 0x18, WS() + 0xFC);
		int32_t n38 = sub32(0, ebx);                      // [ebp+0xC]
		U16(v, 0) = (uint16_t)n38;
		xl_RotTrans(v, pts + 0x20, WS() + 0xFC);
		U16(v, 0) = 0;
		U16(v, 2) = (uint16_t)wfl;
		U16(v, 4) = (uint16_t)nd;
		xl_RotTrans(v, pts + 0x28, WS() + 0xFC);
		U16(v, 2) = (uint16_t)nw;
		xl_RotTrans(v, pts + 0x30, WS() + 0xFC);
		U16(v, 2) = (uint16_t)ebx;
		xl_RotTrans(v, pts + 0x38, WS() + 0xFC);
		U16(v, 2) = (uint16_t)n38;
		xl_RotTrans(v, pts + 0x40, WS() + 0xFC);
		U16(pts, 6) = save6;
		U16(pts, 0xE) = saveE;
	}

	// ------------------------------------------------------------------------------------
	// 0xB1B220 BuildColourRamp(a, b, out, unused): per ribbon segment two colour words
	// {inner, outer}: ws+0xE0 = (a & 0x2FFFFFF) | 0x38000000, ws+0xE4 = same of b;
	// fade-in ws+0xF0 entries (GPF, IR0 = 0 .. step 0x1000/f0), full colour up to ws+0xF4,
	// fade-out up to ws+0xF8 (IR0 0x1000 down), then ws+0xF8 entries of 0x3A000000 (black).
	// Code byte 0x3A on every scaled word. GTE RGBC (data reg 6) = 0x3A000000.
	// ------------------------------------------------------------------------------------
	static void Ribbon_B1B220_BuildColourRamp(uint32_t a, uint32_t b, uint8_t *out, int32_t unused)
	{
		(void)unused;
		x::GteWriteData((int32_t)0x3A000000, 6);
		uint8_t *pa = WS() + 0xE0;                        // [ebp-4]
		U32(pa, 0) = (a & 0x2FFFFFFu) | 0x38000000u;
		uint8_t *pb = WS() + 0xE4;                        // [ebp-8]
		U32(pb, 0) = (b & 0x2FFFFFFu) | 0x38000000u;
		uint8_t *esi = out;
		uint8_t *w = WS();                                // edi
		int32_t n = S32(w, 0xF0);
		if (n != 0)
		{
			int32_t step = 0x1000 / n;                    // [ebp+0x10]
			int32_t ir0 = 0;                              // ebx
			if (n > 0)
			{
				uint8_t *edi = esi + 4;
				for (int32_t cnt = n; cnt != 0; cnt--)
				{
					xl_GteSetIR0(ir0);
					xl_GteLoadRGB(pa);
					xl_GteGPF();
					xl_GteStoreRGB(esi);
					U8(edi, -1) = 0x3A;
					xl_GteSetIR0(ir0);
					xl_GteLoadRGB(pb);
					xl_GteGPF();
					xl_GteStoreRGB(edi);
					U8(edi, 3) = 0x3A;
					esi += 8;
					edi += 8;
					ir0 = add32(ir0, step);
				}
				w = WS();
			}
		}
		{
			int32_t m = sub32(S32(w, 0xF4), S32(w, 0xF0));
			uint32_t e4 = U32(w, 0xE4);
			uint32_t e0 = U32(w, 0xE0);
			if (m > 0)
			{
				for (; m != 0; m--)
				{
					U32(esi, 0) = e0;
					U32(esi, 4) = e4;
					esi += 8;
				}
				w = WS();
			}
		}
		n = sub32(S32(w, 0xF8), S32(w, 0xF4));
		if (n != 0)
		{
			int32_t step = 0x1000 / n;
			int32_t ir0 = 0x1000;
			if (n > 0)
			{
				uint8_t *edi = esi + 4;
				for (int32_t cnt = n; cnt != 0; cnt--)
				{
					xl_GteSetIR0(ir0);
					xl_GteLoadRGB(pa);
					xl_GteGPF();
					xl_GteStoreRGB(esi);
					U8(edi, -1) = 0x3A;
					xl_GteSetIR0(ir0);
					xl_GteLoadRGB(pb);
					xl_GteGPF();
					xl_GteStoreRGB(edi);
					U8(edi, 3) = 0x3A;
					esi += 8;
					edi += 8;
					ir0 = sub32(ir0, step);
				}
				w = WS();
			}
		}
		int32_t z = S32(w, 0xF8);
		for (int32_t cnt = z; cnt > 0; cnt--)
		{
			U32(esi, 0) = 0x3A000000u;
			U32(esi, 4) = 0x3A000000u;
			esi += 8;
		}
	}

	// ------------------------------------------------------------------------------------
	// 0xB1ADD0 SetupParentXformKeepV0: GTE R/TR = parent matrix (bone+0x9C); VZ0 = 0 (written
	// twice), VXY0 is NOT written (vanilla quirk: it keeps whatever the last GTE user loaded;
	// on an advancing tick the last cross-section point of B1B080 = (0, -3*w1/8)); TR =
	// R * V0 + TR. Returns 0. (Not a byte clone of Ifrit's SetupParentXform 0xB27000, which
	// loads V0 = bone outPos.)
	// ------------------------------------------------------------------------------------
	static int32_t Ribbon_B1ADD0_SetupParentXform()
	{
		Mat4x3 *m = blob::GetParentMatrix(U16(CUR(), 0x9C));
		x::GteSetRotMatrix(m);
		x::GteSetTransVector(m);
		x::GteWriteData(0, 1);
		x::GteWriteData(0, 1);
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
		return 0;
	}

	// ------------------------------------------------------------------------------------
	// 0xB1AE20 InsertQuad(depthSum, prim): OT slot = (((sum / 4) >> 2) & ~3) + rt+0x4C
	// (sum = four u16 depths), SSIGPU_InsertPrimDepthKeys(slot, prim, 0, 0, 0, 0).
	// ------------------------------------------------------------------------------------
	static void Ribbon_B1AE20_InsertQuad(int32_t sum, void *prim)
	{
		int32_t e = add32(sum, (sum >> 31) & 3) >> 2;
		e = e >> 2;
		e = (int32_t)((uint32_t)e & 0xFFFFFFFCu);
		e = add32(e, S32(RT(), 0x4C));
		x::InsertPrimDepthKeys((uint32_t)e, prim, 0, 0, 0, 0);
	}

	// one ribbon quad (0x2C-byte gouraud packet with a tpage word): outer band (form A: black
	// inner edge, colour-table outer words) between point pairs (a, b) of the previous (pp)
	// and current (cc) entry projections; listing order of the writes kept
	static uint8_t *QuadOuter(uint8_t *pkt, const uint8_t *col, const uint8_t *pp, const uint8_t *cc, int a, int b)
	{
		U8(pkt, 3) = 0xA;
		U32(pkt, 8) = 0;
		U32(pkt, 4) = U32(WS(), 0x64);
		U32(pkt, 0xC) = U32(WS(), 0x60);
		U32(pkt, 0x1C) = 0;
		U32(pkt, 0x14) = U32(col, 4);
		U32(pkt, 0x24) = U32(col, 0xC);
		U32(pkt, 0x10) = U32(pp, a);
		U32(pkt, 0x18) = U32(pp, b);
		U32(pkt, 0x20) = U32(cc, a);
		U32(pkt, 0x28) = U32(cc, b);
		int32_t sum = (int32_t)U16(pp, a + 4) + (int32_t)U16(cc, a + 4) + (int32_t)U16(pp, b + 4) + (int32_t)U16(cc, b + 4);
		Ribbon_B1AE20_InsertQuad(sum, pkt);
		return pkt + 0x2C;
	}
	// inner band (form B): point a to the centre point 0, colour-table words on all four
	static uint8_t *QuadInner(uint8_t *pkt, const uint8_t *col, const uint8_t *pp, const uint8_t *cc, int a)
	{
		U8(pkt, 3) = 0xA;
		U32(pkt, 8) = 0;
		U32(pkt, 4) = U32(WS(), 0x64);
		U32(pkt, 0xC) = U32(col, 4);
		U32(pkt, 0x14) = U32(col, 0);
		U32(pkt, 0x1C) = U32(col, 0xC);
		U32(pkt, 0x24) = U32(col, 8);
		U32(pkt, 0x10) = U32(pp, a);
		U32(pkt, 0x18) = U32(pp, 0);
		U32(pkt, 0x20) = U32(cc, a);
		U32(pkt, 0x28) = U32(cc, 0);
		int32_t sum = (int32_t)U16(pp, a + 4) + (int32_t)U16(pp, 4) + (int32_t)U16(cc, 4) + (int32_t)U16(cc, a + 4);
		Ribbon_B1AE20_InsertQuad(sum, pkt);
		return pkt + 0x2C;
	}

	// ring entry idx (signed: vanilla idiv, a negative index reads before the ring)
	static inline uint8_t *RingEntry(uint8_t *ring, int32_t idx) { return ring + mul32(idx, 72) + 0x20; }

	// held frames: VXY0 the real tick had when it reached SetupParentXform (see B1ADD0), per node
	static NodeMemo<uint32_t, 64> g_v0memo;
	// held frames: the arena scratch above ctx+0x74 this handler writes (projections + colour table)
	static uint8_t *g_hsave = nullptr;
	static uint32_t g_hsave_cap = 0;

	// ------------------------------------------------------------------------------------
	// 0xB1A130 (Draw 37 RibbonTrail) - Bahamut 0xB1A130 / Cerberus 0xB0D690 / Alexander
	// 0xB01290 / Eden 0xAE8E20.
	// First call (bone+0xBC == 0): allocate the ring (72*n + 32 bytes, arena) and seed entries
	// 0 and 1 at the node position aimed at the target (not gated by boneSkipFlag).
	// Later calls: bind target (ws+0x98) / colour (ws+0x9C) bones; unless frozen (rt+0x45):
	//   outAngleX (bone+0x8C) < 0: tail retire (++T[7]; -2 = hold); returns without drawing once
	//     the tail reaches the capacity;
	//   else ++head, ++length (capped at n-1), new cross-section at the head: follow mode (flags
	//     bit0) = section around the previous point moved to the node position; homing mode =
	//     steered toward the target, the node's outPos (+0x94..) and accumPos (+0x5C..) are set
	//     to it and bone+0x4A bit0 = (distance to target <= target bone+0x8E).
	// Then (always) the draw: colour ramp, head projection, 8 gouraud quads per segment.
	//
	// Held frames (g_held, rt+0x45 forced to 0xFF): the frozen path runs, so the ring, the node
	// position and bone+0x4A never advance: the ribbon drawn is the real tick's history
	// (colour bone and parent matrix are the in-between values). Two held-only additions:
	//   * VXY0 before SetupParentXform is set to the value the real tick had there (vanilla
	//     leaves V0.xy of the previous GTE user in the translation; in a held draw that is
	//     another handler's frozen-path value, which would shift the whole ribbon);
	//   * the arena scratch above ctx+0x74 (outside the saved arena) written by the draw is
	//     restored before returning.
	// The first-draw path cannot run on a held frame (a node's first draw is on the tick that
	// created it); if it did, everything it writes (arena, ctx+0x74, bone) is in the saved set.
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_37_RibbonTrail()
	{
		uint8_t *cur = CUR();
		uint8_t *ring = PTR(cur, 0xBC);                   // esi
		uint8_t *edi;
		if (ring == nullptr)
		{
			// ---- first draw: allocate + seed ----
			uint8_t *prm = PTR(cur, 0xB8);                // ebx
			uint32_t cap = U16(prm, 8);                   // si (esi was 0)
			uint8_t *blk = blob::ArenaAlloc((int32_t)(cap * 9 * 8 + 0x20));
			PTR(CUR(), 0xBC) = blk;
			edi = PTR(CUR(), 0xBC);
			U16(edi, 2) = (uint16_t)cap;
			U16(edi, 0) = 1;
			U16(edi, 4) = 1;
			U16(edi, 8) = U16(prm, 0xE);
			U16(edi, 0xA) = U16(prm, 0xA);
			U16(edi, 0xC) = U16(prm, 0xC);
			U16(edi, 0xE) = 0;
			U16(edi, 0x10) = U16(prm, 0x10);
			uint16_t tref = U16(prm, 0);
			U16(edi, 6) = tref;
			PTR(WS(), 0x98) = blob::GetBone(tref);
			uint8_t *e0 = edi + 0x20;                     // esi
			uint8_t *c = CUR();
			int32_t x0 = S16(c, 0x94);                    // eax
			int32_t y0 = S16(c, 0x96);                    // ecx, [ebp-8]
			int32_t z0 = S16(c, 0x98);                    // edx, [ebp-4]
			U16(e0, 0) = (uint16_t)x0;
			U16(e0, 2) = (uint16_t)y0;
			U16(e0, 4) = (uint16_t)z0;
			int32_t ty, tz;                               // eax / ecx at 0xB1A282
			if (U8(edi, 0x10) & 1)
			{
				uint8_t *b = CUR();
				int32_t dx = S32(b, 0x74) >> 16;
				int32_t dz = S32(b, 0x7C) >> 16;
				int32_t yaw = blob::Blob_B66B30(x0, z0, add32(dx, x0), add32(dz, z0)) & 0xFFF;
				U16(e0, 0x4E) = (uint16_t)yaw;
				U16(e0, 6) = (uint16_t)yaw;
				b = CUR();
				ty = add32(S32(b, 0x78) >> 16, y0);
				tz = S32(b, 0x7C) >> 16;
			}
			else
			{
				int32_t yaw = blob::Blob_B66B30(x0, z0, add32(S16(prm, 2), x0), add32(S16(prm, 6), z0)) & 0xFFF;
				U16(e0, 0x4E) = (uint16_t)yaw;
				U16(e0, 6) = (uint16_t)yaw;
				ty = add32(S16(prm, 4), y0);
				tz = S16(prm, 6);
			}
			tz = add32(tz, z0);
			int32_t pr = blob::Blob_B66B30(y0, z0, ty, tz);
			U16(e0, 0x56) = (uint16_t)(pr & 0xFFF);
			U16(e0, 0xE) = (uint16_t)(pr & 0xFFF);
			int32_t r = x::Sin(pr);
			U16(WS(), 0xD2) = (uint16_t)(shl32(r, 12) >> 12);
			int32_t yw = S16(e0, 6);                      // ebx
			r = x::Cos(yw);
			U16(WS(), 0xD4) = (uint16_t)(shl32(r, 12) >> 12);
			r = x::Sin(yw);
			U16(WS(), 0xD0) = (uint16_t)(shl32(r, 12) >> 12);
			uint8_t *w = WS();
			int32_t flip = xl_LookAt_B66E50(ZEROVEC(), w + 0xD0, w + 0xA0);   // ebx
			x::SetRotMatrix(WS() + 0xA0);
			int32_t sx = S16(e0, 0), sy = S16(e0, 2), sz = S16(e0, 4);
			x::GteWriteCtrl(sx, 5);
			x::GteWriteCtrl(sy, 6);
			x::GteWriteCtrl(sz, 7);
			Ribbon_B1B080_BuildCrossSection(S16(CUR(), 0x90), 0, e0, flip);
			int32_t dep = (U8(edi, 0x10) & 1) ? 0 : S16(CUR(), 0x8C);
			uint8_t *e1 = e0 + 0x48;                      // [ebp-8]
			Ribbon_B1B080_BuildCrossSection(S16(CUR(), 0x90), dep, e1, flip);
			PTR(WS(), 0x68) = e0;
			PTR(WS(), 0x6C) = e1;
			S32(WS(), 0x80) = U16(edi, 0);
			S32(WS(), 0x78) = U16(edi, 4);
			PTR(WS(), 0x9C) = blob::GetBone(U16(edi, 8));
		}
		else
		{
			PTR(WS(), 0x98) = blob::GetBone(U16(ring, 6));
			PTR(WS(), 0x9C) = blob::GetBone(U16(ring, 8));
			if (U8(RT(), 0x45) == 0)
			{
				int16_t ax = S16(CUR(), 0x8C);
				if (ax < 0)
				{
					// ---- tail retire (-2 = hold the tail) ----
					if (ax != -2)
					{
						U16(ring, 0xE) = (uint16_t)(U16(ring, 0xE) + 1);
						if (U16(ring, 0xE) >= U16(ring, 2)) return;   // 0xB1A450 -> 0xB1ADBC
					}
					int32_t rr = (int32_t)U16(ring, 0) % (int32_t)U16(ring, 2);
					PTR(WS(), 0x6C) = RingEntry(PTR(CUR(), 0xBC), rr);
				}
				else
				{
					// ---- advance: new head cross-section ----
					U16(ring, 0) = (uint16_t)(U16(ring, 0) + 1);
					int32_t len = (int32_t)U16(ring, 4) + 1;
					if (len < (int32_t)U16(ring, 2)) U16(ring, 4) = (uint16_t)len;
					edi = PTR(CUR(), 0xBC);
					int32_t head = U16(edi, 0);               // ecx
					S32(WS(), 0x80) = head;
					int32_t cap = U16(edi, 2);                // esi
					PTR(WS(), 0x6C) = RingEntry(edi, head % cap);
					PTR(WS(), 0x68) = RingEntry(edi, sub32(head, 1) % cap);   // head 0 (u16 wrap) -> entry -1
					S32(WS(), 0x78) = U16(edi, 4);
					uint8_t *w = WS();
					uint8_t *tgt = PTR(w, 0x98);              // ebx
					uint8_t *prev = PTR(w, 0x68);             // ecx, [ebp-4]
					uint8_t *nw = PTR(w, 0x6C);               // esi
					if (U8(edi, 0x10) & 1)
						S32(WS(), 0x94) = Ribbon_B1AFE0_FollowDelta(CUR() + 0x94, prev);
					else
						Ribbon_B1AE60_SteerToward(tgt + 0x94, prev, nw);
					w = WS();
					int32_t flip = xl_LookAt_B66E50(ZEROVEC(), w + 0xE0, w + 0xA0);   // [ebp-8]
					x::SetRotMatrix(WS() + 0xA0);
					int32_t py = S16(prev, 2), pz = S16(prev, 4), px = S16(prev, 0);
					x::GteWriteCtrl(px, 5);
					x::GteWriteCtrl(py, 6);
					x::GteWriteCtrl(pz, 7);
					if (U8(edi, 0x10) & 1)
					{
						// follow mode: section around the previous point, moved to the node
						Ribbon_B1B080_BuildCrossSection(S16(CUR(), 0x90), S32(WS(), 0x94), nw, flip);
						uint8_t *b = CUR();
						int32_t dx = sub32(S16(b, 0x94), S16(nw, 0));
						int32_t dy = sub32(S16(b, 0x96), S16(nw, 2));
						int32_t dz = sub32(S16(b, 0x98), S16(nw, 4));
						uint8_t *p = nw + 4;
						for (int n = 9; n != 0; n--)
						{
							U16(p, -4) = (uint16_t)(U16(p, -4) + (uint16_t)dx);
							U16(p, -2) = (uint16_t)(U16(p, -2) + (uint16_t)dy);
							U16(p, 0) = (uint16_t)(U16(p, 0) + (uint16_t)dz);
							p += 8;
						}
					}
					else
					{
						// homing mode: the node moves with the head; arrived flag
						Ribbon_B1B080_BuildCrossSection(S16(CUR(), 0x90), S16(CUR(), 0x8C), nw, flip);
						int32_t nx = S16(nw, 0);
						U16(CUR(), 0x94) = (uint16_t)nx;
						S32(CUR(), 0x5C) = shl32(nx, 16);
						int32_t ny = S16(nw, 2);
						U16(CUR(), 0x96) = (uint16_t)ny;
						S32(CUR(), 0x60) = shl32(ny, 16);
						int32_t nz = S16(nw, 4);
						U16(CUR(), 0x98) = (uint16_t)nz;
						S32(CUR(), 0x64) = shl32(nz, 16);
						int32_t ex = sub32(S16(tgt, 0x94), nx);
						int32_t ey = sub32(S16(tgt, 0x96), ny);
						int32_t ez = sub32(S16(tgt, 0x98), nz);
						int32_t sq = add32(add32(mul32(ez, ez), mul32(ex, ex)), mul32(ey, ey));
						int32_t dist = xl_ISqrt(sq);
						int32_t rad = S16(tgt, 0x8E);
						if (dist > rad) U16(CUR(), 0x4A) = (uint16_t)(U16(CUR(), 0x4A) & 0xFFFE);
						else U8(CUR(), 0x4A) = (uint8_t)(U8(CUR(), 0x4A) | 1);
					}
				}
			}
		}

		// ---- 0xB1A6ED: draw (reads only the ring) ----
		uint8_t *c = CUR();
		edi = PTR(c, 0xBC);                                // [ebp-0x10]
		uint8_t *const rng = edi;
		int32_t v90;
		if (U8(edi, 0x10) & 1) v90 = shl32(S16(c, 0x8E), 4);
		else v90 = shl32(S16(PTR(WS(), 0x98), 0x8C), 4);
		S32(WS(), 0x90) = v90;
		uint8_t *w = WS();
		int32_t arg4 = S32(w, 0x90);                      // [ebp-0xC]
		uint8_t *cb = PTR(w, 0x9C);
		uint32_t colA = (uint32_t)(int32_t)S16(cb, 0x98) | 0x200u;
		colA = (colA << 8) | (uint32_t)(int32_t)S16(cb, 0x96);
		colA = (colA << 8) | (uint32_t)(int32_t)S16(cb, 0x94);
		uint32_t colB = (uint32_t)(int32_t)S16(cb, 0x90) | 0x200u;
		colB = (colB << 8) | (uint32_t)(int32_t)S16(cb, 0x8E);
		colB = (colB << 8) | (uint32_t)(int32_t)S16(cb, 0x8C);
		PTR(w, 0x84) = PTR(CTX(), 0x74) + 0x100;
		S32(WS(), 0xF0) = U16(edi, 0xA);
		S32(WS(), 0xF4) = U16(edi, 0xC);
		S32(WS(), 0xF8) = U16(edi, 2);

		// held: save the scratch above the arena top this draw writes (projection buffers
		// [top, top+0xC6) and the colour table at top+0x100)
		uint8_t *hs_p = nullptr;
		uint32_t hs_n = 0;
		if (g_held.active)
		{
			uint32_t f0 = U16(edi, 0xA), f4 = U16(edi, 0xC), f8 = U16(edi, 2);
			uint32_t ents = f0 + (f4 > f0 ? f4 - f0 : 0) + (f8 > f4 ? f8 - f4 : 0) + f8;
			hs_p = PTR(CTX(), 0x74);
			hs_n = 0x100 + ents * 8;
			if (hs_n > g_hsave_cap)
			{
				delete[] g_hsave;
				g_hsave_cap = (hs_n + 0xFFF) & ~0xFFFu;
				g_hsave = new uint8_t[g_hsave_cap];
			}
			memcpy(g_hsave, hs_p, hs_n);
		}

		Ribbon_B1B220_BuildColourRamp(colA, colB, PTR(WS(), 0x84), arg4);
		if (!g_held.active)
		{
			uint32_t *m = g_v0memo.put(CUR());
			if (m) *m = GTE_VXY0();
		}
		else
		{
			const uint32_t *m = g_v0memo.get(CUR());
			if (m) GTE_VXY0() = *m;
		}
		Ribbon_B1ADD0_SetupParentXform();

		// head entry projection into the arena scratch at ctx+0x74 (sxy, OTZ dword)
		uint8_t *pkt = PTR(CTX(), 0x7C);                  // esi
		{
			int32_t rr = (int32_t)U16(edi, 0) % (int32_t)U16(edi, 2);
			PTR(WS(), 0x6C) = RingEntry(PTR(CUR(), 0xBC), rr);
			uint8_t *pt = PTR(WS(), 0x6C);                // [ebp-4]
			uint8_t *out = PTR(CTX(), 0x74);              // ebx
			for (int n = 9; n != 0; n--)
			{
				uint8_t *fl = WS() + 0xFC;
				S32(out, 4) = xl_RotTransPers(pt, out, fl, fl);
				out += 8;
				pt += 8;
			}
		}
		S32(WS(), 0x7C) = 0;
		S32(WS(), 0x60) = (int32_t)((U32(CUR(), 0xCC) & 0x2000000u) | 0x38000000u);
		S32(WS(), 0x64) = (int32_t)((uint32_t)(int32_t)S16(CUR(), 0x92) | 0xE1000000u);
		S32(WS(), 0x88) = U16(edi, 4);
		uint8_t *col = PTR(WS(), 0x84);                   // [ebp-4]
		{
			uint32_t tail = U16(edi, 0xE);
			if (tail >= 1)
			{
				S32(WS(), 0x88) = sub32(S32(WS(), 0x88), (int32_t)tail);
				col = col + tail * 8;
			}
		}
		S32(WS(), 0x7C) = add32(S32(WS(), 0x7C), 1);
		while (S32(WS(), 0x7C) <= S32(WS(), 0x88))
		{
			int32_t k = S32(WS(), 0x7C);
			int32_t rr = sub32((int32_t)U16(rng, 0), k) % (int32_t)U16(rng, 2);
			PTR(WS(), 0x68) = RingEntry(PTR(CUR(), 0xBC), rr);
			uint8_t *ws = WS();
			int32_t odd = S32(ws, 0x7C) & 1;
			uint8_t *src = PTR(ws, 0x68);                 // ebx
			uint8_t *base = PTR(CTX(), 0x74);
			uint8_t *dst = base + (odd << 7);             // edi
			if (odd == 0)
			{
				PTR(ws, 0x70) = base;
				PTR(WS(), 0x74) = PTR(WS(), 0x70) + 0x80;
			}
			else
			{
				PTR(ws, 0x74) = base;
				PTR(WS(), 0x70) = PTR(WS(), 0x74) + 0x80;
			}
			src += 2;
			for (int n = 9; n != 0; n--)
			{
				uint32_t z = U16(src, 2);
				uint32_t xx = U16(src, -2);
				uint32_t y = U16(src, 0);
				x::GteWriteData((int32_t)((y << 16) | xx), 0);
				x::GteWriteData((int32_t)z, 1);
				x::GteRTPS();
				x::GteReadSXY2(dst);
				U16(dst, 4) = GTE_SZ3_W();
				dst += 8;
				src += 8;
			}
			ws = WS();
			const uint8_t *cc = PTR(ws, 0x70);            // current entry (edi)
			const uint8_t *pp = PTR(ws, 0x74);            // previous entry (ebx)
			pkt = QuadOuter(pkt, col, pp, cc, 0x08, 0x18);
			pkt = QuadOuter(pkt, col, pp, cc, 0x10, 0x20);
			pkt = QuadInner(pkt, col, pp, cc, 0x18);
			pkt = QuadInner(pkt, col, pp, cc, 0x20);
			pkt = QuadOuter(pkt, col, pp, cc, 0x28, 0x38);
			pkt = QuadOuter(pkt, col, pp, cc, 0x30, 0x40);
			pkt = QuadInner(pkt, col, pp, cc, 0x38);
			pkt = QuadInner(pkt, col, pp, cc, 0x40);
			col += 8;
			S32(WS(), 0x7C) = add32(S32(WS(), 0x7C), 1);
		}
		PTR(CTX(), 0x7C) = pkt;

		if (hs_n) memcpy(hs_p, g_hsave, hs_n);
	}
}
	// installs the ports into a clone's tables (called after init_clone)
	void apply_shared_ribbon(Clone &c)
	{
		c.draw[37] = part_shared_ribbon::dh_37_RibbonTrail;
	}
}
}

// ============================================================================================
// part shared_misc
// ============================================================================================
namespace ff8fx
{
namespace gfc
{
namespace part_shared_misc
{
	// ------------------------------------------------------------------------------------
	// local wrappers / constants
	// ------------------------------------------------------------------------------------
	// 0x45C7A0 SSIGPU_InsertPrimAutoDepth(ot, prim): links a packet (and bumps the depth-key
	// cursor 0x1CA8828). Packet/OT side effect: nothing while predicting.
	static inline void xl_InsertPrimAutoDepth(uint32_t ot, void *prim) { if (!g_predict) x::f<void (__cdecl *)(uint32_t, void *)>(0x45C7A0)(ot, prim); }
	// 0x45C9F0 SetDrawStp(p, pbw): GP0 0xE6 mask-bit packet (1 word + tag). Nothing while predicting.
	static inline void xl_SetDrawStp(void *p, int32_t pbw) { if (!g_predict) x::f<void (__cdecl *)(void *, int32_t)>(0x45C9F0)(p, pbw); }
	// 0x45C060 SetDrawMove(p, rect, x, y): GP0 0x80 VRAM-to-VRAM copy of rect to (x, y) (5 words +
	// tag; an empty rect only clears the tag length). Nothing while predicting.
	static inline void xl_SetDrawMove(void *p, const void *rect, int32_t xx, int32_t yy) { if (!g_predict) x::f<void (__cdecl *)(void *, const void *, int32_t, int32_t)>(0x45C060)(p, rect, xx, yy); }
	// 0x505C00 QueueChainTransformation(entity, id): spawns a battle anim-seq task on the entity
	// chain (battle entity + task state). Nothing while predicting.
	static inline void xl_QueueChainTransformation(void *entity, int32_t id) { if (!g_predict) x::f<void (__cdecl *)(void *, int32_t)>(0x505C00)(entity, id); }
	// 0x501FF0 compare_com_127: lowest-address entity of the circular chain entity+0x8C (pure read)
	static inline uint8_t *xl_ChainMinEntity(void *entity) { return x::f<uint8_t *(__cdecl *)(void *)>(0x501FF0)(entity); }
	// 0xB65D10 (shared blob) OffscreenStageRender(desc, pos, angles): renders the battle stage
	// model into the desc's private OT/packet block and EXECUTES that OT immediately (GPU work,
	// battle model pose rebuild 0x1D989D0). Nothing while predicting; returns 0.
	static inline int32_t xl_OffscreenStageRender(void *desc, const void *pos, const void *angles)
	{
		if (g_predict) return 0;
		return x::f<int32_t (__cdecl *)(void *, const void *, const void *)>(0xB65D10)(desc, pos, angles);
	}

	// battle display globals (shared engine data): u8 display parity, display/draw env array
	// (92 bytes per buffer)
	static const uint32_t DISP_PARITY_1D96A80 = 0x1D96A80;
	static const uint32_t DISP_ENV_1D969C8 = 0x1D969C8;
	// battle entity array base (0x9C bytes per entity), used by VM 0x0CF to turn an entity
	// pointer into a slot
	static const uint32_t BATTLE_ENTITIES_1D972C0 = 0x1D972C0;

	// fild qword [u16 zero-extended DEPTHARR[off >> 3]]; fmul dword [0x1877DA8]; call __ftol
	static inline int32_t DepthKey(uint32_t off)
	{
		uint32_t v = DEPTHARR()[off >> 3] & 0xFFFF;
		return (int32_t)(int64_t)((double)v * (double)FLT_1877DA8());
	}

	// NCLIP of three screen vertices (GTE SXY0..2 = data regs 12..14), MAC0 (reg 24)
	static inline int32_t Nclip(uint32_t s0, uint32_t s1, uint32_t s2)
	{
		int32_t mac0;
		x::GteWriteData((int32_t)s0, 12);
		x::GteWriteData((int32_t)s1, 13);
		x::GteWriteData((int32_t)s2, 14);
		x::GteNCLIP();
		x::GteReadData(&mac0, 24);
		return mac0;
	}

	// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n  (signed division rounding toward zero)
	static inline int32_t SDivPow2(int32_t v, int n)
	{
		return add32(v, (int32_t)((uint32_t)(v >> 31) & ((1u << n) - 1))) >> n;
	}

	// ------------------------------------------------------------------------------------
	// 0xB58FA0 (Draw 16 ScrollTextureU, Leviathan; = Bahamut 0xB198C0): horizontal scroll of a
	// texture inside its VRAM rectangle. bone+0xB8 -> {s16 alt texture id, s16 vram x, s16 vram
	// y}; the row buffer (w*h*2 bytes) is allocated once in the arena (bone+0xBC); every row is
	// rotated right by outPosX & (w-1) texels, then ONE upload of the whole rect (rect from the
	// SceneHeader+0x43 ring). Draws no primitive.
	// Held frames: returns at once (no VRAM upload, no rect ring slot, no arena allocation).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_16_ScrollTextureU()
	{
		if (g_held.active) return;
		uint8_t *arg = PTR(CUR(), 0xB8);                         // edi
		blob::ReadAltTexture(S16(arg, 0));                       // sets ws+0xF0..0xFC
		U32(WS(), 0x64) = U32(WS(), 0xFC);                       // texel data
		uint8_t *tex = PTR(WS(), 0xF0);                          // esi
		if (U32(CUR(), 0xBC) == 0)
		{
			int32_t n = shl32(mul32(S16(tex, 6), S16(tex, 4)), 1);
			uint8_t *b = blob::ArenaAlloc(n);
			PTR(CUR(), 0xBC) = b;
		}
		U32(WS(), 0x60) = U32(CUR(), 0xBC);
		{
			uint8_t *w = WS();
			U32(w, 0x80) = U32(w, 0x60);                         // upload source
		}
		PTR(WS(), 0x68) = blob::VramRectRing();
		uint8_t *rect = PTR(WS(), 0x68);                         // eax
		U16(rect, 0) = U16(arg, 2);
		U16(rect, 2) = U16(arg, 4);
		uint16_t wv = U16(tex, 4);
		U16(rect, 4) = wv;
		S32(WS(), 0x78) = (int16_t)wv;                           // width
		int32_t wd = S32(WS(), 0x78);                            // ecx
		uint16_t hv = U16(tex, 6);
		U16(rect, 6) = hv;
		S32(WS(), 0x7C) = (int16_t)hv;                           // rows left
		int32_t mask = sub32(wd, 1);                             // eax
		int32_t stride = add32(wd, wd);                          // ecx
		S32(WS(), 0x6C) = mask;
		S32(WS(), 0x70) = (int32_t)S16(CUR(), 0x94) & mask;      // scroll
		S32(WS(), 0x84) = stride;
		uint8_t *w = WS();
		if (S32(w, 0x7C) > 0)
		{
			do
			{
				int32_t sc = S32(w, 0x70);                       // edi
				uint8_t *src = PTR(w, 0x64);                     // ecx
				uint8_t *dst = (uint8_t *)(U32(w, 0x60) + (uint32_t)sc * 2u);
				int32_t k = sub32(S32(w, 0x78), sc);
				if (k > 0)
				{
					do
					{
						U16(dst, 0) = U16(src, 0);
						src += 2;
						dst += 2;
					} while (--k != 0);
					w = WS();
				}
				k = S32(w, 0x70);
				dst = PTR(w, 0x60);
				if (k > 0)
				{
					do
					{
						U16(dst, 0) = U16(src, 0);
						src += 2;
						dst += 2;
					} while (--k != 0);
					w = WS();
				}
				int32_t st = S32(w, 0x84);
				U32(w, 0x60) = U32(w, 0x60) + (uint32_t)st;
				w = WS();
				U32(w, 0x64) = U32(w, 0x64) + (uint32_t)st;
				w = WS();
				S32(w, 0x7C) = sub32(S32(w, 0x7C), 1);
				w = WS();
			} while (S32(w, 0x7C) > 0);
		}
		x::QueueVramUpload(PTR(w, 0x68), PTR(w, 0x80));
	}

	// ------------------------------------------------------------------------------------
	// 0xB0EB60 (helper of Draw 26 and VM 0x096, Cerberus; = Eden): screen capture. Links into
	// the OT bucket rt+0x4C+4: SetDrawStp(0), five DR_MOVE packets copying the displayed
	// framebuffer (display env 0x1D969C8 + 92 * parity, rect {x, y, 0x40, 0xE0} at ws+0xF8,
	// x += 0x40 per strip as a DWORD add) to VRAM (ws+0xF0 + 0x40 * i, ws+0xF4), SetDrawStp(1).
	// Packets from ctx+0x7C. Returns 0.
	// Predict (VM 0x096): the packet/OT calls are skipped by their wrappers (the cursor and the
	// ws fields it writes are restored anyway).
	// ------------------------------------------------------------------------------------
	static int32_t CaptureScreenStrips()
	{
		uint32_t par = MEM<uint8_t>(DISP_PARITY_1D96A80);
		uint8_t *pkt = PTR(CTX(), 0x7C);                                     // esi
		uint32_t idx = par * 3u;
		idx <<= 3;
		idx -= par;                                                          // par * 23
		uint8_t *env = (uint8_t *)(DISP_ENV_1D969C8 + idx * 4u);             // edi
		xl_SetDrawStp(pkt, 0);
		xl_InsertPrimAutoDepth(U32(RT(), 0x4C) + 4, pkt);
		uint8_t *w = WS();
		U16(w, 0xF8) = U16(env, 0);
		pkt += 0xC;
		U16(w, 0xFA) = U16(env, 2);
		U32(WS(), 0xFC) = 0xE00040;                                          // w 0x40, h 0xE0
		w = WS();
		int32_t yy = S32(w, 0xF4);                                           // [ebp-4]
		int32_t xx = S32(w, 0xF0);                                           // edi
		for (int k = 5; k != 0; k--)
		{
			xl_SetDrawMove(pkt, WS() + 0xF8, xx, yy);
			xl_InsertPrimAutoDepth(U32(RT(), 0x4C) + 4, pkt);
			w = WS();
			xx = add32(xx, 0x40);
			pkt += 0x18;
			U32(w, 0xF8) = U32(w, 0xF8) + 0x40;                              // dword add (x, carry into y)
		}
		xl_SetDrawStp(pkt, 1);
		xl_InsertPrimAutoDepth(U32(RT(), 0x4C) + 4, pkt);
		pkt += 0xC;
		PTR(CTX(), 0x7C) = pkt;
		return 0;
	}

	// the bone whose draw 26 did the capture on the last real draw (its first call): the held
	// frames that follow draw nothing for it (vanilla drew no strips on that tick)
	static const void *g_cap_bone = nullptr;
	static uint32_t g_cap_tick = 0xFFFFFFFF;

	// ------------------------------------------------------------------------------------
	// 0xB0E930 (Draw 26 ScreenCaptureStrips, Cerberus; = Eden 0xAEA0C0): bone+0xB8 -> {s16 vram x,
	// s16 vram y} -> ws+0x90/0x94. First call (bone+0xC4 == 0): bone+0xC4 = -1, ws+0xF0/F4 =
	// vram x/y, CaptureScreenStrips (5 DR_MOVE framebuffer -> VRAM). Later calls, only when the
	// colour bone+0xCC has R == G == B: five semi-transparent flat quads (POLY_F4, code 0x2A |
	// colour) 64 x 224 at (outPosX + 64 * i, outPosY) into rt+0x4C+8 (alt viewport), then a
	// draw-mode packet (tpage 0, window {0, 0, 256, 256}); ws+0xF0/F4 = vram x/y. The tpage word
	// computed into ws+0x80 (0xB0EA0B) is a dead store: SetDrawMode gets tpage 0.
	// Held frames: never the one-shot capture (bone+0xC4 == 0 -> nothing), and nothing on the
	// held frames that follow the real tick whose draw was the capture (vanilla drew no strips
	// on that tick); otherwise the strips are redrawn (pure packets, in-between outPos/colour).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_26_ScreenCaptureStrips()
	{
		uint8_t *arg = PTR(CUR(), 0xB8);
		S32(WS(), 0x90) = S16(arg, 0);
		S32(WS(), 0x94) = S16(arg, 2);
		uint8_t *b = CUR();                                                  // ecx
		if (U32(b, 0xC4) == 0)
		{
			if (g_held.active) return;
			U32(b, 0xC4) = 0xFFFFFFFF;
			U32(WS(), 0xF0) = U32(WS(), 0x90);
			U32(WS(), 0xF4) = U32(WS(), 0x94);
			CaptureScreenStrips();
			g_cap_bone = b;
			g_cap_tick = g_real_tick;
			return;
		}
		if (g_held.active)
		{
			if (g_cap_bone == b && g_cap_tick == g_real_tick) return;
		}
		else if (g_cap_bone == b)
			g_cap_bone = nullptr;
		uint32_t c = U32(b, 0xCC);                                           // esi
		uint32_t t = (uint32_t)((int32_t)c >> 8);
		if (((t ^ c) & 0xFF) != 0) return;                                   // R != G
		if (((t ^ c) & 0xFF00) != 0) return;                                 // G != B
		int32_t tp = S16(b, 0x92);                                           // ecx
		uint8_t *pkt = PTR(CTX(), 0x7C);                                     // esi
		uint8_t *w = WS();
		int32_t di = 0;
		int32_t v = (S32(w, 0x90) >> 2) & 0xF0;
		v |= S32(w, 0x94) & 0x100;
		v >>= 4;
		v |= tp;
		v |= 0x100;                                                          // or dh, 1
		S32(w, 0x80) = v;                                                    // dead store
		U32(WS(), 0x84) = U32(CUR(), 0xCC) | 0x2A000000;
		S32(WS(), 0x60) = S16(CUR(), 0x94);
		S32(WS(), 0x64) = S16(CUR(), 0x96);
		for (int k = 5; k != 0; k--)
		{
			U8(pkt, 3) = 5;
			U32(pkt, 4) = U32(WS(), 0x84);
			uint16_t x0 = (uint16_t)(U16(WS(), 0x60) + (uint16_t)di);
			U16(pkt, 0x10) = x0;
			U16(pkt, 8) = x0;
			uint16_t x1 = (uint16_t)((uint16_t)(U16(WS(), 0x60) + (uint16_t)di) + 0x40);
			U16(pkt, 0x14) = x1;
			U16(pkt, 0xC) = x1;
			uint16_t y0 = U16(WS(), 0x64);
			U16(pkt, 0xE) = y0;
			U16(pkt, 0xA) = y0;
			uint16_t y1 = (uint16_t)(U16(WS(), 0x64) + 0xE0);
			U16(pkt, 0x16) = y1;
			U16(pkt, 0x12) = y1;
			x::InsertPrimAltViewport(U32(RT(), 0x4C) + 8, pkt);
			di += 0x40;
			pkt += 0x18;
		}
		int16_t rect[4];
		rect[1] = 0;          // [ebp-6]
		rect[0] = 0;          // [ebp-8]
		rect[3] = 0x100;      // [ebp-2]
		rect[2] = 0x100;      // [ebp-4]
		x::SetDrawMode(pkt, 0, 0, 0, rect);
		x::InsertPrimAltViewport(U32(RT(), 0x4C) + 8, pkt);
		PTR(CTX(), 0x7C) = pkt + 0xC;
		U32(WS(), 0xF0) = U32(WS(), 0x90);
		U32(WS(), 0xF4) = U32(WS(), 0x94);
	}

	// per-block memo of the stars drawn by the last real draw of Draw 39 (held frames redraw
	// exactly those)
	struct StarMemo { const uint8_t *blk; uint32_t tick; uint8_t bits[0x1000]; };
	static StarMemo g_star[4];
	static uint8_t g_held_scr[0x8000 * 8];   // held frames: screen coordinates (vanilla: ctx+0x74)

	static StarMemo *star_memo_put(const uint8_t *blk)
	{
		for (StarMemo &m : g_star)
			if (m.blk == blk) { m.tick = g_real_tick; memset(m.bits, 0, sizeof(m.bits)); return &m; }
		for (StarMemo &m : g_star)
			if (m.tick != g_real_tick || m.blk == nullptr) { m.blk = blk; m.tick = g_real_tick; memset(m.bits, 0, sizeof(m.bits)); return &m; }
		return nullptr;
	}
	static const StarMemo *star_memo_get(const uint8_t *blk)
	{
		for (const StarMemo &m : g_star)
			if (m.blk == blk && m.tick == g_real_tick) return &m;
		return nullptr;
	}

	// ------------------------------------------------------------------------------------
	// 0xB1E3F0 (Draw 39 Starfield, Bahamut; = Eden 0xAECD20). bone+0xB8 -> {s16 n, s16 range x,
	// y, z, s16 colour jitter, s16 twinkle probability (/256)}. First call (bone+0xBC == 0):
	// arena block {u16 counter, u16 n, n x 16-byte stars {s16 x, y, z, u16 flags (bit0 = twinkles),
	// u32 packet colour word (0x68 | colour + jitter), u8 on timer, u8 off timer, pad}}, seeded with
	// CRT rand (blob rand wrappers, 8 or 9 draws per star). Every call (NOT gated by boneSkipFlag):
	// ++counter, stars projected through SetupParentXform (screen xy + OTZ/4 into the free arena at
	// ctx+0x74), then one TILE_1-style dot packet (len 2: colour, xy) per visible star into
	// bone+0xC0 ? rt+0x4C + (bone+0xC0 & 0x3FFC) : rt+0x4C + (OTZ & ~3); twinkling stars count
	// their on/off timers down and redraw a timer with rand(0..0x40) at expiry (a star whose off
	// timer expires turns on but is not drawn that tick).
	// Held frames: bone+0xBC == 0 -> nothing (no allocation, no rand); else no ++counter, no timer,
	// no rand: the stars the real tick drew (memo) are re-projected with the in-between parent
	// matrix / outPos (screen coordinates in a private buffer) and redrawn.
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_39_Starfield()
	{
		const bool held = g_held.active;
		if (U32(CUR(), 0xBC) == 0)
		{
			if (held) return;
			uint8_t *prm = PTR(CUR(), 0xB8);                                 // edi
			int32_t n = S16(prm, 0);                                         // ebx, [ebp-8]
			uint8_t *blk = blob::ArenaAlloc(add32(shl32(n, 4), 4));
			PTR(CUR(), 0xBC) = blk;
			uint8_t *s = PTR(CUR(), 0xBC);                                   // esi
			U16(s, 2) = (uint16_t)n;
			int32_t jit = S16(prm, 8);                                       // ebx
			uint32_t col = U32(CUR(), 0xCC);
			s += 4;
			int32_t c0 = (int32_t)(col & 0xFF);                              // [ebp-0x14]
			int32_t c2 = (int32_t)((col >> 16) & 0xFF);                      // [ebp-0x1C]
			int32_t c1 = (int32_t)((col >> 8) & 0xFF);                       // [ebp-0x18]
			int32_t rx = S16(prm, 2);                                        // [ebp-4]
			int32_t ry = S16(prm, 4);                                        // [ebp-0xC]
			int32_t rz = S16(prm, 6);                                        // [ebp-0x10]
			S32(WS(), 0xF0) = S16(prm, 0xA);
			if (n > 0)
			{
				int32_t k = n;
				do
				{
					U16(s, 6) = 0;
					U16(s, 0xC) = 0;
					U16(s, 0) = (uint16_t)blob::Rand73(rx);
					U16(s, 2) = (uint16_t)blob::Rand73(ry);
					U16(s, 4) = (uint16_t)blob::Rand73(rz);
					int32_t a = add32(blob::Rand73(jit), c0);                // edi
					int32_t g = shl32(add32(blob::Rand73(jit), c1), 8);
					a |= g;
					int32_t bl = shl32(add32(blob::Rand73(jit), c2), 16);
					U32(s, 8) = (uint32_t)(bl | a | 0x68000000);
					int32_t r = blob::Rand74(0x100);
					if (r < S32(WS(), 0xF0))
					{
						U8(s, 6) |= 1;
						int32_t tt = blob::Rand73(0x40);
						if (tt > 0) U8(s, 0xC) = (uint8_t)tt;
						else U8(s, 0xD) = (uint8_t)~(uint8_t)tt;
					}
					s += 0x10;
				} while (--k != 0);
			}
		}
		uint8_t *blk = PTR(CUR(), 0xBC);                                     // ebx, [ebp-0x1C]
		uint8_t *stars = blk + 4;                                            // edi
		PTR(WS(), 0x60) = stars;
		if (!held) U16(blk, 0) = (uint16_t)(U16(blk, 0) + 1);               // not gated in vanilla
		uint32_t otw = U16(CUR(), 0xC0);
		if (otw == 0)
			U32(WS(), 0x64) = 0;
		else
			U32(WS(), 0x64) = (otw & 0x3FFC) + U32(RT(), 0x4C);
		h_B27000();
		int32_t n = S16(blk, 2);
		uint8_t *scr0 = held ? g_held_scr : PTR(CTX(), 0x74);               // esi
		if (held && n > 0x8000) n = 0x8000;
		if (n > 0)
		{
			uint8_t *s = scr0;
			uint8_t *v = stars;
			int32_t k = n;
			do
			{
				x::GteLoadV0(v);
				x::GteRTPS();
				x::GteReadSXY2(s);
				x::GteReadOTZdiv4(s + 4);
				v += 0x10;
				s += 8;
			} while (--k != 0);
		}
		uint8_t *sc = held ? g_held_scr : PTR(CTX(), 0x74);                 // [ebp-4]
		uint8_t *pkt = PTR(CTX(), 0x7C);                                     // edi
		uint8_t *e = PTR(WS(), 0x60);                                        // esi
		n = S16(blk, 2);
		if (held && n > 0x8000) n = 0x8000;
		StarMemo *mp = held ? nullptr : star_memo_put(blk);
		const StarMemo *mg = held ? star_memo_get(blk) : nullptr;
		if (n > 0)
		{
			e += 0xC;
			for (int32_t i = 0; i < n; i++)
			{
				bool draw;
				if (held)
				{
					// the real tick's decision (fallback without memo: the timer state)
					if (mg) draw = ((mg->bits[i >> 3] >> (i & 7)) & 1) != 0;
					else draw = !(U8(e, -6) & 1) || U8(e, 0) != 0;
				}
				else if (U8(e, -6) & 1)
				{
					uint8_t al = U8(e, 0);
					if (al == 0)
					{
						al = (uint8_t)(U8(e, 1) - 1);
						U8(e, 1) = al;
						if (al == 0)
						{
							int32_t r = blob::Rand74(0x40);
							U8(e, 1) = 0;
							U8(e, 0) = (uint8_t)r;
						}
						draw = false;
					}
					else
					{
						al--;
						U8(e, 0) = al;
						if (al == 0)
						{
							int32_t r = blob::Rand74(0x40);
							U8(e, 0) = 0;
							U8(e, 1) = (uint8_t)r;
						}
						draw = true;
					}
				}
				else
					draw = true;
				if (draw)
				{
					U8(pkt, 3) = 2;
					U32(pkt, 4) = U32(e, -4);
					U32(pkt, 8) = U32(sc, 0);
					uint32_t ot = U32(WS(), 0x64);
					if (ot == 0) ot = (uint32_t)((int32_t)S16(sc, 4) & ~3) + U32(RT(), 0x4C);
					xl_InsertPrimAutoDepth(ot, pkt);
					pkt += 0xC;
					if (mp && i < 0x8000) mp->bits[i >> 3] |= (uint8_t)(1u << (i & 7));
				}
				sc += 8;
				e += 0x10;
			}
		}
		PTR(CTX(), 0x7C) = pkt;
	}

	// ------------------------------------------------------------------------------------
	// 0xB105B0 (prim renderer 16, Cerberus; = Eden 0xAEC030): flat quad with a draw-mode word,
	// colour tinted by the material colour (LightColourSetup(ws+0x84) + NCCS). Record 0xC: +0 u32
	// colour, +4/+6/+8/+0xA u16 vertex offsets 0..3. Packet 0x20 (len 7): +4 ws+0x50 draw mode,
	// +8 0, +0xC RGB (code 0x28 | ws+0x8C), +0x10/+0x14/+0x18/+0x1C SXY 0..3. Culled by NCLIP
	// (v0, v1, v2) unless ws+0x90 & 0x10, skipped when any vertex has a clip bit (0x46 << 16).
	// ------------------------------------------------------------------------------------
	static void __cdecl pr_16_TintedF4()
	{
		h_B29450((int32_t)U32(WS(), 0x84));
		uint8_t *ws = WS();                                  // [ebp-0x14]
		int32_t count = S32(ws, 0x70);                       // [ebp-0x1C]
		uint8_t *rec = PTR(ws, 0x6C);                        // [ebp-0x10]
		uint8_t *pkt = PTR(ws, 0x60);                        // esi
		uint32_t ot = U32(ws, 0x5C);                         // [ebp-0x28]
		uint32_t nocull = U32(ws, 0x90) & 0x10;              // [ebp-0x20]
		U32(ws, 0xF0) = 0;
		do
		{
			uint8_t *vb = PTR(ws, 0x7C);
			uint32_t o0 = U16(rec, 4);
			uint32_t o2 = U16(rec, 8);
			uint32_t o1 = U16(rec, 6);
			uint32_t o3 = U16(rec, 0xA);
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			int32_t z3 = DepthKey(o3);
			uint8_t *p2 = vb + o2;
			uint8_t *p3 = vb + o3;
			uint32_t s2 = U32(p2, 0);
			uint32_t s1 = U32(vb + o1, 0);
			uint8_t *p0 = vb + o0;
			uint8_t *p1 = vb + o1;
			uint32_t s3 = U32(p3, 0);
			uint32_t s0 = U32(p0, 0);
			U32(pkt, 0x1C) = s3;
			U32(pkt, 0x14) = s1;
			U32(pkt, 0x10) = s0;
			U32(pkt, 0x18) = s2;
			if (nocull == 0 && Nclip(s0, s1, s2) < 0) goto next;
			{
				uint32_t a0 = U32(p0, 4), a1 = U32(p1, 4), a2 = U32(p2, 4), a3 = U32(p3, 4);
				if (((a3 | a2 | a1 | a0) >> 16) & 0x46) goto next;
				uint32_t slot = (((a3 + a2 + a1 + a0) >> 2) & 0x3FFC) + ot;
				U8(pkt, 3) = 7;
				U32(pkt, 8) = 0;
				x::GteWriteData((int32_t)(U32(ws, 0x8C) | U32(rec, 0) | 0x28000000), 6);
				x::Gte_4601B0();
				uint32_t mode = U32(ws, 0x50);
				x::GteReadData2(0x16, pkt + 0xC);
				U32(pkt, 4) = mode;
				x::InsertPrimDepthKeys(slot, pkt, z0, z1, z2, z3);
				pkt += 0x20;
			}
		next:
			rec += 0xC;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// ------------------------------------------------------------------------------------
	// 0xB11340 (prim renderer 29, Cerberus; = Alexander 0xB053E0): Gouraud textured triangle
	// with the record's raw colours (no GTE lighting). Record 0x1C: +0/+4/+8 u32 colour 0..2,
	// +0xC/+0xE/+0x10 u16 UV 0..2, +0x12/+0x14/+0x16 u16 vertex offsets, +0x18 u16 CLUT, +0x1A u16
	// tpage. Packet 0x28 (len 9): +4/+0x10/+0x1C colour (code 0x34 on colour 0), +8/+0x14/+0x20
	// SXY, +0xC/+0x18/+0x24 UV (no ws+0x98 offset), +0xE CLUT + ws+0x54, +0x1A tpage | ws+0x50
	// (+0x26 not written). Skipped when the three vertices share a clip bit (u16 +6 AND), culled
	// by NCLIP always; OT slot = ot + ((z0 + z1 + z2) / 3 & 0x3FFC) from the vertex +4 words.
	// ------------------------------------------------------------------------------------
	static void __cdecl pr_29_GT3()
	{
		uint8_t *ws = WS();                                  // [ebp-0x10]
		int32_t count = S32(ws, 0x70);                       // [ebp-0x18]
		uint8_t *pkt = PTR(ws, 0x60);                        // esi
		uint32_t ot = U32(ws, 0x5C);                         // [ebp-0x30]
		uint8_t *rec = PTR(ws, 0x6C);                        // edi
		do
		{
			uint8_t *vb = PTR(ws, 0x7C);
			uint32_t o0 = U16(rec, 0x12);
			uint32_t o1 = U16(rec, 0x14);
			uint32_t o2 = U16(rec, 0x16);
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			uint8_t *p2 = vb + o2;
			uint8_t *p0 = vb + o0;
			uint8_t *p1 = vb + o1;
			if (U16(p0, 6) & (uint16_t)(U16(p2, 6) & U16(p1, 6))) goto next;
			{
				uint32_t s0 = U32(p0, 0);
				uint32_t s1 = U32(p1, 0);
				uint32_t s2 = U32(p2, 0);
				U32(pkt, 8) = s0;
				U32(pkt, 0x14) = s1;
				U32(pkt, 0x20) = s2;
				if (Nclip(s0, s1, s2) < 0) goto next;
				int32_t sum = (int32_t)(U16(p2, 4) + U16(p1, 4) + U16(p0, 4));
				int32_t hi = (int32_t)(((int64_t)sum * (int64_t)0x55555556) >> 32);
				U8(pkt, 3) = 9;
				int32_t sgn = sum >> 31;
				uint32_t tp = U32(ws, 0x50) | U16(rec, 0x1A);
				int32_t clut_add = S32(ws, 0x54);
				U16(pkt, 0x1A) = (uint16_t)tp;
				U16(pkt, 0xE) = (uint16_t)add32(U16(rec, 0x18), clut_add);
				U16(pkt, 0xC) = U16(rec, 0xC);
				U16(pkt, 0x18) = U16(rec, 0xE);
				U16(pkt, 0x24) = U16(rec, 0x10);
				U32(pkt, 0x10) = U32(rec, 4);
				U32(pkt, 0x1C) = U32(rec, 8);
				uint32_t key = (uint32_t)sub32(hi, sgn) & 0x3FFC;
				U32(pkt, 4) = U32(rec, 0) | 0x34000000;
				x::InsertPrimDepthKeys(ot + key, pkt, z0, z1, z2, 0);
				pkt += 0x28;
			}
		next:
			rec += 0x1C;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// ------------------------------------------------------------------------------------
	// 0xAFA6E0 (VM 0x022 ApplyActionResultList, Brothers; = Eden 0xAEF450). Action = ctx+0xCC
	// {+8 result records (0x18 bytes, +0 u8 slot), +0x10 s8 count}. sub = op >> 12:
	//   1: SceneHeader+0x17A s8 m != 0 -> for i = m..1: ApplyActionResultToTarget(first record
	//      whose slot == SceneHeader+0x173+i) (m < 0: nothing); m == 0 -> as "other";
	//   8: ApplyActionResultToTargets(records, count);
	//   other: ApplyActionResultToTarget(first record whose slot == bone+0x1B), if any.
	// cursor += 2. (Ifrit's 0x022 tests (op & 0xF000) == 0x8000 only.)
	// Predict: the battle calls are no-ops (x:: wrappers).
	// ------------------------------------------------------------------------------------
	static void __cdecl op_022_ApplyActionResultList()
	{
		uint8_t *act = PTR(CTX(), 0xCC);                     // ebx
		uint32_t sub = (uint32_t)U16(RT(), 0x4A) >> 12;
		uint8_t *rec = PTR(act, 8);                          // ecx
		int32_t n = S8(act, 0x10);                           // esi
		if (sub == 1)
		{
			uint8_t *sc = SCENE();
			int32_t m = S8(sc, 0x17A);                       // edi
			if (m != 0)
			{
				if (m > 0)
				{
					uint8_t *list = sc + 0x173;              // [ebp-4]
					do
					{
						uint32_t slot = U8(list, m);         // [ebp-8]
						uint8_t *r = PTR(act, 8);
						int32_t k = n;
						if (k > 0)
						{
							do
							{
								if (slot == U8(r, 0))
								{
									x::ApplyActionResultToTarget((int32_t)r);
									break;
								}
								r += 0x18;
								k--;
							} while (k > 0);
						}
						m--;
					} while (m > 0);
				}
				STREAM() += 2;
				return;
			}
		}
		else if (sub == 8)
		{
			x::ApplyActionResultToTargets((int32_t)rec, n);
			STREAM() += 2;
			return;
		}
		uint32_t slot = U8(CUR(), 0x1B);
		if (n > 0)
		{
			do
			{
				if (slot == U8(rec, 0))
				{
					x::ApplyActionResultToTarget((int32_t)rec);
					STREAM() += 2;
					return;
				}
				rec += 0x18;
				n--;
			} while (n > 0);
		}
		STREAM() += 2;
	}

	// ------------------------------------------------------------------------------------
	// 0xB24BB0 (VM 0x042 EntityChainAnim, Bahamut; = Brothers 0xAFEF60). Entity = SceneHeader+
	// 0x60[bone+0x1B]. sub = op >> 9: 0 -> QueueChainTransformation(entity, s16 w1), cursor += 4;
	// 2 -> entity+0x72 != entity+0x73 ? cursor += 4 : cursor += s16 w1 (relative jump); other ->
	// nothing (cursor unchanged, vanilla).
	// Predict: QueueChainTransformation is a no-op.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_042_EntityChainAnim()
	{
		uint32_t sub = (uint32_t)U16(RT(), 0x4A) >> 9;
		if (sub == 0)
		{
			int32_t id = S16(STREAM(), 2);
			uint32_t slot = U8(CUR(), 0x1B);
			uint8_t *e = PTR(SCENE(), 0x60 + (int32_t)slot * 4);
			xl_QueueChainTransformation(e, id);
			STREAM() += 4;
		}
		else if (sub == 2)
		{
			uint32_t slot = U8(CUR(), 0x1B);
			uint8_t *e = PTR(SCENE(), 0x60 + (int32_t)slot * 4);
			uint8_t a = U8(e, 0x72);
			uint8_t b = U8(e, 0x73);
			uint8_t *s = STREAM();
			if (a != b)
				STREAM() = s + 4;
			else
				STREAM() = s + S16(s, 2);
		}
	}

	// ------------------------------------------------------------------------------------
	// 0xB64090 (VM 0x05C AccumFromMeshVertex, Leviathan; = Brothers 0xAFEBF0 = Eden 0xAF3660):
	// target bone b = bone ref w1 (CUR is switched to it, the caller's bone kept in ws+0x60).
	//   op bit 15 set: M = RotMatrixOrder(p->outAngle, w3) (p = bone of b's parent id bone+0x9C)
	//     scaled by b->outAngle << 4, translation = b->outPos through (M, p->outPos); v = vertex
	//     w2 of b's mesh (bone+0xD8, vertices at +U32(+0x14), 8 bytes) through M -> ws+0xF0 (MAC),
	//     ws+0xFC (FLAG); accum = ws+0xF0..0xF8 << 16; cursor += 8.
	//   else: morph block bone+0xBC {s16 weight bone, s16 object A, s16 object B}: vertex w2 of A
	//     and B, lerp by weight bone outPos / 256 per axis, scaled by b->outAngle (4.12, << 4),
	//     plus parent(b)->outPos; accum = that << 16; cursor += 6.
	// Then CUR = the caller's bone, its accumPos (+0x5C/60/64) = accum, and its bone handler
	// (bone+0x18) runs.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_05C_AccumFromMeshVertex()
	{
		PTR(WS(), 0x60) = CUR();
		uint8_t *b = blob::GetBone(S16(STREAM(), 2));
		CUR() = b;
		int32_t ax, ay, az;                                  // ecx, edx, eax
		uint8_t *next;                                       // edi
		if (U8(RT(), 0x4B) & 0x80)
		{
			uint8_t *p = blob::GetBone(U16(b, 0x9C));        // edi
			uint8_t *m = (uint8_t *)blob::RotMatrixOrder(p + 0x8C, S16(STREAM(), 6));   // esi
			S32(WS(), 0x80) = shl32(S16(CUR(), 0x8C), 4);
			S32(WS(), 0x84) = shl32(S16(CUR(), 0x8E), 4);
			S32(WS(), 0x88) = shl32(S16(CUR(), 0x90), 4);
			x::ScaleMatrix(m, WS() + 0x80);
			uint8_t *t = m + 0x14;                           // ebx
			S32(t, 0) = S16(p, 0x94);
			S32(m, 0x18) = S16(p, 0x96);
			S32(m, 0x1C) = S16(p, 0x98);
			x::SetRotMatrix(m);
			x::SetTransVector(m);
			x::TransformToCamera(CUR() + 0x94, t, WS() + 0x80);
			x::SetTransVector(m);
			uint8_t *mesh = PTR(CUR(), 0xD8);
			uint8_t *w = WS();
			int32_t vi = S16(STREAM(), 4);
			uint8_t *v = (uint8_t *)(uint32_t)add32(add32((int32_t)U32(mesh, 0x14), shl32(vi, 3)), (int32_t)(uint32_t)mesh);
			x::TransformToCamera(v, w + 0xF0, w + 0xFC);
			w = WS();
			next = STREAM();
			ax = shl32(S32(w, 0xF0), 16);
			ay = shl32(S32(w, 0xF4), 16);
			az = shl32(S32(w, 0xF8), 16);
			next += 8;
		}
		else
		{
			uint8_t *blk = PTR(b, 0xBC);                     // esi
			uint8_t *wb = blob::GetBone(S16(blk, 0));        // edi
			uint8_t *oa = blob::ObjectPtr(S16(blk, 2));      // ebx
			uint8_t *ob = blob::ObjectPtr(S16(blk, 4));      // eax
			uint8_t *vb = ob + U32(ob, 0x14);                // esi
			int32_t off = shl32(S16(STREAM(), 4), 3);        // edx
			uint8_t *va = oa + U32(oa, 0x14);                // ecx
			vb += off;
			int32_t a = S16(va + off, 0);                    // ebx
			int32_t bv = S16(vb, 0);
			va += off;
			S32(WS(), 0xE0) = add32(SDivPow2(mul32(bv - a, S16(wb, 0x94)), 8), a);
			a = S16(va, 2);
			bv = S16(vb, 2);
			S32(WS(), 0xE4) = add32(SDivPow2(mul32(bv - a, S16(wb, 0x96)), 8), a);
			a = S16(va, 4);
			bv = S16(vb, 4);
			S32(WS(), 0xE8) = add32(SDivPow2(mul32(bv - a, S16(wb, 0x98)), 8), a);
			{
				uint8_t *w = WS();
				S32(w, 0xE0) = SDivPow2(shl32(mul32(S16(CUR(), 0x8C), S32(w, 0xE0)), 4), 12);
			}
			{
				uint8_t *w = WS();
				S32(w, 0xE4) = SDivPow2(shl32(mul32(S16(CUR(), 0x8E), S32(w, 0xE4)), 4), 12);
			}
			{
				uint8_t *w = WS();
				S32(w, 0xE8) = SDivPow2(shl32(mul32(S16(CUR(), 0x90), S32(w, 0xE8)), 4), 12);
			}
			uint8_t *par = blob::GetBone(U16(CUR(), 0x9C));
			uint8_t *w = WS();
			ax = add32(S16(par, 0x94), S32(w, 0xE0));
			ay = add32(S16(par, 0x96), S32(w, 0xE4));
			az = add32(S16(par, 0x98), S32(w, 0xE8));
			next = STREAM();
			ax = shl32(ax, 16);
			ay = shl32(ay, 16);
			az = shl32(az, 16);
			next += 6;
		}
		STREAM() = next;
		uint8_t *sv = PTR(WS(), 0x60);
		CUR() = sv;
		S32(sv, 0x5C) = ax;
		S32(CUR(), 0x60) = ay;
		S32(CUR(), 0x64) = az;
		C().bone[U8(CUR(), 0x18)]();
	}

	// ------------------------------------------------------------------------------------
	// 0xB23AF0 (VM 0x090 OffscreenStageRender, Bahamut; = Brothers 0xAFDA00). Unless RenderCtx+0x30
	// (u16) != 0: ws+0xD0..0xD6 = w1..w4 (off-screen rect), render descriptor at the arena top
	// {+0 OT = top + 0xEC, +4 0, +8 packets = top + 0x10EC, +0xC size 0x5BEC, +0x14 arena top after
	// the allocation}, bone+0xC4 = 0x5BEC, ArenaAlloc(0x5BEC), then blob 0xB65D10(desc, outPos,
	// outAngle) renders the battle stage into it and executes that OT. cursor += 10.
	// (Ifrit's 0x090 is `cursor += 10`.)
	// Predict: the descriptor words written above the arena top are guarded (ArenaAlloc journals
	// the block only when it is called, after these writes); the render is a no-op.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_090_OffscreenStageRender()
	{
		if (U16(RCTX(), 0x30) != 0)
		{
			STREAM() += 0xA;
			return;
		}
		U16(WS(), 0xD0) = U16(STREAM(), 2);
		U16(WS(), 0xD2) = U16(STREAM(), 4);
		U16(WS(), 0xD4) = U16(STREAM(), 6);
		U16(WS(), 0xD6) = U16(STREAM(), 8);
		uint8_t *d = PTR(CTX(), 0x74);                       // esi
		guard(d, 0x18);
		U32(d, 0) = 0;
		U32(d, 4) = 0;
		U32(d, 0) = U32(CTX(), 0x74) + 0xEC;
		uint32_t sz = 0x5BEC;
		uint32_t pk = U32(CTX(), 0x74);
		U32(d, 0xC) = sz;
		U32(d, 8) = pk + 0x10EC;
		U32(CUR(), 0xC4) = sz;
		blob::ArenaAlloc(S32(d, 0xC));
		U32(d, 0x14) = U32(CTX(), 0x74);
		uint8_t *b = CUR();
		xl_OffscreenStageRender(d, b + 0x94, b + 0x8C);
		STREAM() += 0xA;
	}

	// ------------------------------------------------------------------------------------
	// 0xB16F60 (VM 0x096 CaptureScreenStrips, Cerberus; = Eden 0xAF2760): ws+0xF0/F4 = s16 w1/w2
	// (VRAM destination), CaptureScreenStrips (0xB0EB60). cursor += 6.
	// Predict: the capture's packet/OT calls are no-ops (wrappers).
	// ------------------------------------------------------------------------------------
	static void __cdecl op_096_CaptureScreenStrips()
	{
		S32(WS(), 0xF0) = S16(STREAM(), 2);
		S32(WS(), 0xF4) = S16(STREAM(), 4);
		CaptureScreenStrips();
		STREAM() += 6;
	}

	// ------------------------------------------------------------------------------------
	// 0xAFCBE0 (VM 0x0CF TargetListFromChain, Brothers; = Eden 0xAF1830). Entity e =
	// SceneHeader+0x60[bone+0x1B]; if e+0x8C (chain link) == 0: SceneHeader+0x17A = 0, cursor += 2.
	// sub = op >> 12: 0 -> SceneHeader+0x174 = bone slot, +0x17A = 1; 1 -> +0x17A = +0x41 (s8
	// count m), +0x173+i = +0x47+i for i = m..1; other -> nothing (cursor unchanged, vanilla).
	// Then the bone's slot = SceneHeader+0x48 = (lowest entity of e's chain - 0x1D972C0) / 0x9C
	// (magic 0xA41A41A5 sequence), SceneHeader+0x41 = 1, cursor += 2.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0CF_TargetListFromChain()
	{
		uint32_t sub = (uint32_t)U16(RT(), 0x4A) >> 12;
		uint8_t *e;
		if (sub == 0)
		{
			uint8_t slot = U8(CUR(), 0x1B);
			uint8_t *sc = SCENE();                           // edx
			e = PTR(sc, 0x60 + (int32_t)slot * 4);           // ecx
			if (U32(e, 0x8C) == 0)
			{
				U8(sc, 0x17A) = 0;
				STREAM() += 2;
				return;
			}
			U8(sc, 0x174) = slot;
			U8(SCENE(), 0x17A) = 1;
		}
		else if (sub == 1)
		{
			uint32_t slot = U8(CUR(), 0x1B);
			uint8_t *sc = SCENE();                           // eax
			e = PTR(sc, 0x60 + (int32_t)slot * 4);           // esi
			if (U32(e, 0x8C) == 0)
			{
				U8(sc, 0x17A) = 0;
				STREAM() += 2;
				return;
			}
			U8(sc, 0x17A) = U8(sc, 0x41);
			sc = SCENE();
			int32_t k = S8(sc, 0x17A);
			if (k > 0)
			{
				for (;;)
				{
					U8(sc, k + 0x173) = U8(sc, k + 0x47);
					k--;
					if (k <= 0) break;
					sc = SCENE();
				}
			}
		}
		else
			return;
		uint8_t *r = xl_ChainMinEntity(e);
		uint32_t d = (uint32_t)r - BATTLE_ENTITIES_1D972C0;                 // ecx
		uint32_t hi = (uint32_t)(((uint64_t)d * 0xA41A41A5ull) >> 32);      // edx
		uint32_t q = (((d - hi) >> 1) + hi) >> 7;
		U8(CUR(), 0x1B) = (uint8_t)q;
		U8(SCENE(), 0x48) = (uint8_t)q;
		U8(SCENE(), 0x41) = 1;
		STREAM() += 2;
	}
}

	// installs the ports into a clone's tables (called after init_clone): only the slots whose
	// original code is real (not a `ret` stub); VM 0x022 / 0x090 also exist in Ifrit's form in
	// every clone, so they are replaced only in the clones that carry this variant
	void apply_shared_misc(Clone &c)
	{
		auto live = [](uint32_t slot) -> bool
		{
			uint32_t orig = *(const uint32_t *)slot;
			return orig >= 0x401000 && orig < 0xC00000 && *(const uint8_t *)orig != 0xC3;
		};
		if (live(c.draw_table + 4 * 16)) c.draw[16] = part_shared_misc::dh_16_ScrollTextureU;         // Leviathan 0xB58FA0, Bahamut 0xB198C0
		if (live(c.draw_table + 4 * 26)) c.draw[26] = part_shared_misc::dh_26_ScreenCaptureStrips;    // Cerberus 0xB0E930, Eden 0xAEA0C0
		if (live(c.draw_table + 4 * 39)) c.draw[39] = part_shared_misc::dh_39_Starfield;              // Bahamut 0xB1E3F0, Eden 0xAECD20
		if (live(c.prim_table + 4 * 16)) c.prim[16] = part_shared_misc::pr_16_TintedF4;               // Cerberus 0xB105B0, Eden 0xAEC030
		if (live(c.prim_table + 4 * 29)) c.prim[29] = part_shared_misc::pr_29_GT3;                    // Cerberus 0xB11340, Alexander 0xB053E0
		if (live(c.vm_table + 4 * 0x042)) c.vm[0x042] = part_shared_misc::op_042_EntityChainAnim;     // Bahamut 0xB24BB0, Brothers 0xAFEF60
		if (live(c.vm_table + 4 * 0x05C)) c.vm[0x05C] = part_shared_misc::op_05C_AccumFromMeshVertex; // Leviathan 0xB64090, Brothers 0xAFEBF0, Eden 0xAF3660
		if (live(c.vm_table + 4 * 0x096)) c.vm[0x096] = part_shared_misc::op_096_CaptureScreenStrips; // Cerberus 0xB16F60, Eden 0xAF2760
		if (live(c.vm_table + 4 * 0x0CF)) c.vm[0x0CF] = part_shared_misc::op_0CF_TargetListFromChain; // Brothers 0xAFCBE0, Eden 0xAF1830
		if ((c.effect_id == 205 || c.effect_id == 206) && live(c.vm_table + 4 * 0x022))
			c.vm[0x022] = part_shared_misc::op_022_ApplyActionResultList;                           // Brothers 0xAFA6E0, Eden 0xAEF450
		if ((c.effect_id == 202 || c.effect_id == 205) && live(c.vm_table + 4 * 0x090))
			c.vm[0x090] = part_shared_misc::op_090_OffscreenStageRender;                            // Bahamut 0xB23AF0, Brothers 0xAFDA00
	}
}
}
// ==== END clone-shared handler parts ====
