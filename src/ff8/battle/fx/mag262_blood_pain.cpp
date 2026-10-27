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

// Effect 262: Blood Pain (Kiros' limit break while he is a party member, the dream sequences of
// disc 1; kernel.bin section 18 "temporary character limit breaks" entry 3, the only kernel entry
// using effect 262), cinematic-engine module MAG_262 0x94F9C0..0x9598A0, script data
// magic/mag261_b.00/.01 (loaded by MAG_262_UNKNOWN_FL 0x94F9C0; setup MAG_262_UNKNOWN 0x94F9F0 =
// effect table index 261).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp), a sibling of
// Laguna's Desperado (mag261_desperado.cpp: same layout, same draw 71) and Ward's Massive Anchor
// (263). Instruction-level comparison against the ported clones (module-relative addresses
// normalised, table regions symbolic, callee trees hashed):
//   - SetupSummon 0x94FA50, BindContext 0x94FAA0, ReleaseContext 0x94FB30, the section pointer
//     set-up 0x94FB40, InitSequenceState 0x958EB0, AnimChannels / AnimIntegrator and SequenceTick
//     0x950060 (the unlit BuildMatricesAndDraw, = Water 0xA9B870) are byte clones of the engine's;
//     0x959880 sets the initial frame limit; lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub (VM 0x03F /
//     0x041 / 0x089 / 0x093, prim 7 / 9 / 12 / 17, draw 3 / 21 are stubs here), except
//       VM 0x006 LoadBattleFile 0x955220: same code, own load callback 0x955350 (c.load_cb),
//     and the module's own handlers (Ifrit has `ret` stubs in these slots), ported here:
//       VM 0x0AB 0x9509B0 BindLimitAnimSet: as Desperado's 0x95ABB0 with the module's animation
//         set (com data 0x187A2A4). Length 4;
//       VM 0x12E 0x954DA0 ApplyNextTargetResult: list = ctx+0xCC (count = s8 +0x10, entries of
//         0x18 bytes at *(+8)); when SceneHeader+0x3A < count: ApplyActionResultToTarget 0x506690
//         (entry SceneHeader+0x3A), SceneHeader+0x3A += 1. Length 2;
//       particle op 22 0x9544B0 VelocityToOrigin: the particle (ws+0x44) velocity +0x2A/+0x2C/+0x2E
//         = -(position +0x22/+0x24/+0x26) / w1 (C division). Length 4;
//       draw 71 0x950B50 JointTrail (+ 0x950BF0 TrailSample / 0x9510A0 JointFrame = Desperado's
//         0x95B400 / 0x95B4A0 / 0x95B950 with the module's own data: ring 0x27960C8, points
//         0x2795DF8 / 0x2795E38, curves 0x2795E78 / 0x2795F78, unused vectors 0x2796078..0x27960B4):
//         a gradient ribbon (30 segments x two semi-transparent POLY_G4) behind a joint of the slot
//         entity, see mag261_desperado.cpp. The script sets it on two bones (Kiros' two blades).
// The effect's queue (0x2795DE0, pool 0x2796080) holds one task, the engine tick 0x950060.
//
// Writes outside the engine state: battle entity (the caster): its animation set / chain (VM
// 0x0AB), its skeleton (draw 71: world matrices then the pose rebuild); battle results (VM 0x12E,
// VM 0x022 generic); module data 0x2795DF8..0x27960DC (draw 71). No battle-entity position write.
// Camera: VM 0x039 (engine).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace bloodpain262
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag262_blood_pain_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace bloodpain262
{
	static void describe(Clone &c)
	{
		c.name = "262 Blood Pain";
		c.effect_id = 262;
		c.vm_table = 0x18586A0;     // MAG_262 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1858310;   // MAG_262 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1858358;   // bone_table + 0x48
		c.prim_size = 0x1858448;    // bone_table + 0x138
		c.spr_table = 0x185845C;    // bone_table + 0x14C
		c.ptype_table = 0x1858488;  // bone_table + 0x178
		c.pop_table = 0x185849C;    // bone_table + 0x18C
		c.draw_table = 0x18584FC;   // MAG_262 DrawHandlerTable
		c.attr_16A = 0x1858666;
		c.attr_184 = 0x1858680;
		c.attr_186 = 0x1858682;
		c.attr_194 = 0x1858690;
		c.attr_19C = 0x1858698;
		c.desc = 0x18581A8;         // draw_table - 0x354
		c.ptr_block = 0x18582E8;    // draw_table - 0x214 (read by 0x94FAA0)
		c.queue = 0x2795DE0;
		c.file00 = 0x2795DF0;
		c.file01 = 0x2795DD8;
		c.tick = 0x950060;
		c.load_cb = 0x955350;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// module data / engine calls not wrapped by gfc_engine.h
	// ------------------------------------------------------------------------------------
	static const uint32_t COMDATA = 0x187A2A4;   // the module's animation set (VM 0x0AB)
	static const uint32_t RING = 0x27960C8;      // draw 71 ring: N x { u32 alive, SVECTOR base, SVECTOR tip }
	static const uint32_t PTS_BASE = 0x2795DF8;  // gathered base points (8 B each)
	static const uint32_t PTS_TIP = 0x2795E38;   // gathered tip points
	static const uint32_t SPL_BASE = 0x2795E78;  // 32 curve points (8 B each)
	static const uint32_t SPL_TIP = 0x2795F78;
	static const uint32_t TAIL_BASE = 0x2796078; // copy of SPL_TIP[0] (8 B)
	static const uint32_t DIR_TIP = 0x2796090;   // normalized SPL_TIP[0] - SPL_TIP[1] (3 x s32)
	static const uint32_t HEAD_BASE = 0x27960A0; // copy of SPL_BASE[0] (8 B)
	static const uint32_t DIR_BASE = 0x27960A8;  // normalized TAIL_BASE - SPL_BASE[0] (3 x s32)

	template<typename F> static inline F fx(uint32_t a) { return (F)a; }
	static inline uint8_t *A(uint32_t a) { return (uint8_t *)a; }
	// battle entity side effects (30 fps layer: see gfc_engine_held.h)
	static inline void xl_BindEntityModelFromComData(void *ent, void *out, uint32_t data) { FX_HELD(if (held_predicting()) return;) fx<void (__cdecl *)(void *, void *, uint32_t)>(0x5022C0)(ent, out, data); }
	static inline void xl_QueueChainTransformation(void *ent, int32_t id) { FX_HELD(if (held_predicting()) return;) fx<void (__cdecl *)(void *, int32_t)>(0x505C00)(ent, id); }
	// 0xB66B80 (blob): matrix of joint `joint` of the model whose BattleAnimHeader is `hdr`,
	// composed with m; joint origin -> ws+0xF0..0xF8. Returns ws+0xD0. Writes only ws + GTE.
	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m) { return fx<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(0xB66B80)(hdr, joint, scale, m); }
	// curves (exe): Effect_SplineSetup(count, points, work) / Effect_SplineEval(count, work, out, t 0..0x1000)
	static inline void xl_SplineSetup(int32_t n, const void *pts, void *work) { fx<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, pts, work); }
	static inline void xl_SplineEval(int32_t n, const void *work, void *out, int32_t t) { fx<void (__cdecl *)(int32_t, const void *, void *, int32_t)>(0x571690)(n, work, out, t); }
	static inline void xl_Normalize(const void *in, void *out) { fx<void (__cdecl *)(const void *, void *)>(0x56BC50)(in, out); }          // NormalizeVectorToFixedPoint
	static inline void xl_MulVec3x3(const void *m, const void *in, void *out) { fx<void (__cdecl *)(const void *, const void *, void *)>(0x56C600)(m, in, out); } // TransformVectorBy3x3Matrix
	// software GTE
	static inline void xl_GteReadFLAG(void *dst) { fx<void (__cdecl *)(void *)>(0x45E210)(dst); }
	static inline void xl_GteAVSZ4() { fx<void (__cdecl *)()>(0x45E610)(); }
	static inline void xl_GteSetIR0(int32_t v) { fx<void (__cdecl *)(int32_t)>(0x45E150)(v); }
	static inline void xl_GteLoadRGBC(const void *c) { fx<void (__cdecl *)(const void *)>(0x45E110)(c); }
	static inline void xl_GteDPCS() { fx<void (__cdecl *)()>(0x45F270)(); }
	static inline void xl_GteStoreRGB2(void *dst) { fx<void (__cdecl *)(void *)>(0x45E360)(dst); }
	// 0x45C7A0 SSIGPU_InsertPrimAutoDepth(ot, prim) (+ the depth-key cursor 0x1CA8828)
	static inline void xl_InsertPrimAutoDepth(uint32_t ot, void *prim) { FX_HELD(if (held_predicting()) return;) fx<void (__cdecl *)(uint32_t, void *)>(0x45C7A0)(ot, prim); }

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	// ------------------------------------------------------------------------------------
	// 0x9509B0 (VM 0x0AB BindLimitAnimSet): see the module comment
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AB_BindLimitAnimSet()
	{
		uint8_t *ent = SlotEntity();                                          // esi
		uint8_t *blk = blob::ArenaAlloc(0x20);
		xl_BindEntityModelFromComData(ent, blk, COMDATA);
		xl_QueueChainTransformation(ent, S16(STREAM(), 2));
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// 0x954DA0 (VM 0x12E ApplyNextTargetResult): see the module comment
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
	// 0x9544B0 (particle op 22 VelocityToOrigin): see the module comment
	// ------------------------------------------------------------------------------------
	static void __cdecl pop_22_VelocityToOrigin()
	{
		uint8_t *p = PTR(WS(), 0x44);                                         // ecx
		const int32_t w = S16(STREAM(), 2);                                   // esi
		S16(p, 0x2A) = (int16_t)-(S16(p, 0x22) / w);
		S16(p, 0x2C) = (int16_t)-(S16(p, 0x24) / w);
		S16(p, 0x2E) = (int16_t)-(S16(p, 0x26) / w);
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// draw 71 JointTrail
	// ------------------------------------------------------------------------------------
	// Field_Alloc(0x40) work block of 0x950BF0
	struct TrailWork
	{
		int32_t pad0[2];   // +0x00 (not used)
		int32_t otz;       // +0x08 OTZ of the segment
		int32_t flag;      // +0x0C GTE FLAG of the RTPT
		uint32_t c_prev;   // +0x10 near-edge colour
		uint32_t c_cur;    // +0x14 far-edge colour (DPCS result)
		uint32_t pad18;    // +0x18 (not used)
		uint32_t c_base;   // +0x1C colour code | bone colour (RGBC of the DPCS)
		int16_t base[4];   // +0x20 joint position (+0x26 never written)
		int16_t tip[4];    // +0x28 tip (+0x2E never written)
		int32_t dir[4];    // +0x30 joint -z axis (+0x3C not used)
	};
	static_assert(sizeof(TrailWork) == 0x40, "TrailWork");

	// 0x9510A0 JointFrame(hdr, joint, dir, pos): joint >= 0xF0 -> skeleton byte table [joint -
	// 0xEC]; dir = normalize(R * (0, 0, -0x1000)) of that bone's matrix, pos = its translation
	static void JointFrame(uint8_t *hdr, int32_t joint, int32_t *dir, int16_t *pos)
	{
		uint8_t *sk = PTR(PTR(hdr, 4), 0);                                    // ecx
		uint8_t *bones = sk + 0x10;                                           // edi
		if (joint >= 0xF0) joint = U8(sk, joint - 0xEC);
		uint8_t *b = bones + 0x30 * joint;                                    // esi
		dir[1] = 0;
		dir[0] = 0;
		dir[2] = (int32_t)0xFFFFF000;
		xl_MulVec3x3(b + 0x10, dir, dir);
		xl_Normalize(dir, dir);
		pos[0] = S16(b, 0x24);
		pos[1] = S16(b, 0x28);
		pos[2] = S16(b, 0x2C);
	}

	// tip = base + len * dir >> 12 (16-bit adds)
	static void TipFrom(TrailWork *w, int32_t len)
	{
		for (int k = 0; k < 3; k++) w->tip[k] = (int16_t)((mul32(len, w->dir[k]) >> 12) + w->base[k]);
	}

	// the drawing part of 0x950BF0 (from the GTE set-up on): 30 segments of the two 32-point
	// curves sa (base) / sb (tip), colour c (bone+0xCC), packets at ctx+0x7C
	static void TrailDraw(const uint8_t *sa, const uint8_t *sb, uint32_t c, TrailWork *w)
	{
		x::SetRotMatrix(NODEMAT());
		x::SetTransVector(NODEMAT());
		x::GteSetBackColor3(0, 0, 0);
		uint8_t *p = PTR(CTX(), 0x7C);                                        // edi
		const uint32_t col = (c & 0xFFFFFF) | 0x3A000000;
		uint8_t *q = p + 0x24;                                                // esi
		w->c_cur = col;
		w->c_base = col;
		int32_t t = 0;                                                        // [ebp-4]
		for (int32_t off = 0; off < 0xF0; off += 8, t += 0x1000)              // [ebp+8]
		{
			w->c_prev = w->c_cur;
			U32(p, 0) = 0x8000000;
			x::GteLoadV012(sa + off, sa + off + 8, sb + off);
			x::GteRTPT();
			xl_GteReadFLAG(&w->flag);
			if (w->flag & 0x60000) continue;
			x::GteReadSXY012(p + 8, p + 0x10, p + 0x18);
			x::GteLoadV0(sb + off + 8);
			x::GteRTPS();
			x::GteReadSXY2(p + 0x20);
			xl_GteAVSZ4();
			x::GteReadOTZ(&w->otz);
			xl_GteSetIR0(t / 30);
			xl_GteLoadRGBC(&w->c_base);
			xl_GteDPCS();
			xl_GteStoreRGB2(&w->c_cur);
			U32(p, 0x14) = w->c_prev;
			U32(p, 4) = w->c_prev;
			U32(p, 0x1C) = w->c_cur;
			U32(p, 0xC) = w->c_cur;
			xl_InsertPrimAutoDepth(FRAMERL() + 4 * (uint32_t)(w->otz >> 2) + 0x44, p);
			U32(q, 0) = 0x8000000;
			U32(q, 0xC) = 0x3A000000;
			U32(q, 4) = 0x3A000000;
			U32(q, 0x14) = w->c_prev;
			U32(q, 0x1C) = w->c_cur;
			U32(q, 8) = U32(p, 8);
			U32(q, 0x10) = U32(p, 0x10);
			U32(q, 0x18) = U32(p, 0x18);
			U32(q, 0x20) = U32(p, 0x20);
			xl_InsertPrimAutoDepth(FRAMERL() + 4 * (uint32_t)(w->otz >> 2) + 0x44, q);
			p += 0x48;
			q += 0x48;
		}
		PTR(CTX(), 0x7C) = q;
	}

	// newest-first gather of min(size, 100 - counter) alive ring entries from slot s: base points
	// -> pa, tip points -> pb (8 B each); returns the count (0x950CCC..0x950D44)
	static int32_t TrailGather(const uint8_t *ring, int32_t size, int32_t counter, int32_t s, uint8_t *pa, uint8_t *pb)
	{
		int32_t n = size;                                                     // eax
		const int32_t lim = 0x64 - size;                                      // ecx
		if (counter >= lim) n = n - counter + lim;
		int32_t i = 0;                                                        // esi
		for (; i < n; i++)
		{
			const uint8_t *e = ring + 0x14 * s;
			if (U32(e, 0) == 0) break;
			s--;
			U32(pa, 8 * i) = U32(e, 4);
			U32(pa, 8 * i + 4) = U32(e, 8);
			U32(pb, 8 * i) = U32(e, 0xC);
			U32(pb, 8 * i + 4) = U32(e, 0x10);
			if (s < 0) s = size - 1;
		}
		return i;
	}

	// 32-point curve of n points (Field_Alloc(0x190) work block)
	static void TrailCurve(int32_t n, const uint8_t *pts, uint8_t *work, uint8_t *out)
	{
		xl_SplineSetup(n, pts, work);
		int32_t k = 0;
		for (uint8_t *o = out; o < out + 0x100; o += 8, k += 0x1000)
			xl_SplineEval(n, work, o, k / 31);
	}

	// 0x950BF0 TrailSample(data, blk)
	static void TrailSample(uint8_t *data, uint8_t *blk)
	{
		TrailWork *w = (TrailWork *)x::FieldAlloc(0x40);                      // ebx
		JointFrame(SlotEntity() + 0x60, U16(data, 0), w->dir, w->base);
		TipFrom(w, S16(CUR(), 0x94));
		const int32_t size = U16(data, 2);                                    // edi
		const int32_t slot = (int32_t)((uint32_t)U16(blk, 0) % (uint32_t)size); // edx
		uint8_t *e = A(RING) + 0x14 * slot;
		U32(e, 0) = 1;
		U32(e, 4) = U32(w->base, 0);
		U32(e, 8) = U32(w->base, 4);
		U32(e, 0xC) = U32(w->tip, 0);
		U32(e, 0x10) = U32(w->tip, 4);
		const int32_t n = TrailGather(A(RING), size, U16(blk, 0), slot, A(PTS_BASE), A(PTS_TIP));
		if (n > 1)
		{
			uint8_t *work = (uint8_t *)x::FieldAlloc(0x190);                  // [ebp-4]
			TrailCurve(n, A(PTS_BASE), work, A(SPL_BASE));
			TrailCurve(n, A(PTS_TIP), work, A(SPL_TIP));
			x::FieldFree(0x190);
			// two direction vectors (written, never read by the module)
			U32(A(TAIL_BASE), 0) = U32(A(SPL_TIP), 0);
			S32(A(DIR_TIP), 0) = S16(A(SPL_TIP), 0) - S16(A(SPL_TIP), 8);
			U32(A(TAIL_BASE), 4) = U32(A(SPL_TIP), 4);
			S32(A(DIR_TIP), 4) = S16(A(SPL_TIP), 2) - S16(A(SPL_TIP), 0xA);
			S32(A(DIR_TIP), 8) = S16(A(SPL_TIP), 4) - S16(A(SPL_TIP), 0xC);
			xl_Normalize(A(DIR_TIP), A(DIR_TIP));
			U32(A(HEAD_BASE), 0) = U32(A(SPL_BASE), 0);
			U32(A(HEAD_BASE), 4) = U32(A(SPL_BASE), 4);
			S32(A(DIR_BASE), 0) = S16(A(TAIL_BASE), 0) - S16(A(SPL_BASE), 0);
			S32(A(DIR_BASE), 4) = S16(A(TAIL_BASE), 2) - S16(A(SPL_BASE), 2);
			S32(A(DIR_BASE), 8) = S16(A(TAIL_BASE), 4) - S16(A(SPL_BASE), 4);
			xl_Normalize(A(DIR_BASE), A(DIR_BASE));
			TrailDraw(A(SPL_BASE), A(SPL_TIP), U32(CUR(), 0xCC), w);
		}
		x::FieldFree(0x40);
		U16(blk, 0) = (uint16_t)(U16(blk, 0) + 1);
	}

	// 0x950B50 (draw 71 JointTrail): see the module comment
	static void __cdecl dh_71_JointTrail()
	{
		// 30 fps layer: see mag262_blood_pain_held.inc
		FX_HELD(if (held_active()) { held_dh71(); return; })
		uint8_t *b = CUR();
		if (PTR(b, 0xBC) == nullptr)
		{
			uint8_t *data = PTR(b, 0xB8);                                     // esi
			uint8_t *blk = blob::ArenaAlloc(0x10);
			PTR(CUR(), 0xBC) = blk;
			U16(blk, 0) = 0;
			for (int32_t i = 0; i < (int32_t)U16(data, 2); i++) U32(A(RING), 0x14 * i) = 0;
			b = CUR();
		}
		uint8_t *data = PTR(b, 0xB8);                                         // edi
		uint8_t *blk = PTR(b, 0xBC);                                          // ebx
		uint8_t *ent = PTR(SCENE(), 0x60 + 4 * (int32_t)U8(b, 0x1B));         // ecx
		// 30 fps layer: see mag262_blood_pain_held.inc
		FX_HELD(held_note_dh71(b, ent, false);)
		if (S16(b, 0x96) != 0) return;
		x::ComputeBonesWorldMatrices(ent + 0x60, ent + 0x40);
		TrailSample(data, blk);
		x::BuildBoneMatricesFromPose(ent + 0x60);
		// 30 fps layer: see mag262_blood_pain_held.inc
		FX_HELD(held_note_dh71(b, ent, true);)
	}

	// ------------------------------------------------------------------------------------
	// the module's own handlers (no shared / other-clone handler is live in this module)
	// ------------------------------------------------------------------------------------
	static void apply_slots(Clone &c)
	{
		c.vm[0x0AB] = op_0AB_BindLimitAnimSet;          // 0x9509B0
		c.vm[0x12E] = op_12E_ApplyNextTargetResult;     // 0x954DA0
		c.pop[22] = pop_22_VelocityToOrigin;            // 0x9544B0
		c.draw[71] = dh_71_JointTrail;                  // 0x950B50
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		// 30 fps layer: see mag262_blood_pain_held.inc
		FX_HELD(held_tick262();)
		return SequenceTick(g_c);
	}
}
}

	void register_mag262_blood_pain()
	{
		using namespace gfc;
		bloodpain262::describe(bloodpain262::g_c);
		init_clone(bloodpain262::g_c, nullptr, 0);
		bloodpain262::apply_slots(bloodpain262::g_c);
		register_port(0x950060, (void *)bloodpain262::SequenceTask, "MAG262 Blood Pain SequenceTick", 262);
		// 30 fps layer: see mag262_blood_pain_held.inc
		FX_HELD(register_mag262_blood_pain_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag262_blood_pain_held.inc"
#endif
