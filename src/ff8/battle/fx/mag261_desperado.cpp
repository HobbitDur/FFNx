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

// Effect 261: Desperado (Laguna's limit break while he is a party member, the dream sequences of
// disc 1; kernel.bin section 18 "temporary character limit breaks" entry 2, the only kernel entry
// using effect 261), cinematic-engine module MAG_261 0x9598A0..0x9649D0, script data
// magic/mag260_b.00/.01 (loaded by MAG_261_UNKNOWN_FL 0x9598A0; setup MAG_261_UNKNOWN 0x9598D0 =
// effect table index 260).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp), a sibling of Kiros'
// Blood Pain (262, mag262_blood_pain.cpp) and Ward's Massive Anchor (263,
// mag263_massive_anchor.cpp). Instruction-level comparison against the ported clones
// (module-relative addresses normalised, table regions symbolic, callee trees hashed):
//   - SetupSummon 0x959930, BindContext 0x959980, ReleaseContext 0x959A10, the section pointer
//     set-up 0x959A20, InitSequenceState 0x963FE0, AnimChannels / AnimIntegrator and SequenceTick
//     0x959F40 (the unlit BuildMatricesAndDraw, = Water 0xA9B870) are byte clones of the engine's;
//     0x9649B0 sets the initial frame limit (ctx+0xA0 = 0x2C8); lit_dispatcher false;
//   - every table handler is a byte clone of the generic (Ifrit) one or a `ret` stub, except
//       VM 0x006 LoadBattleFile 0x960140: same code, own load callback 0x960270 (c.load_cb),
//       VM 0x042 0x963E30 EntityChainAnim (= Breath of Death 0xA658D0 / Bahamut 0xB24BB0) and
//         prim 16 0x95D100 TintedF4 (= Cerberus 0xB105B0 / Eden / Fire Breath): apply_shared_misc,
//       draw 38 0x95ACB0 AttachBattleEntity (= Brothers 0xAF59D0 / Water 0xA9C340; only that slot of
//         apply_brothers is taken, as Water does),
//       VM 0x0AD 0x963A90 PosFromPosedEntityJoint (= Mad Cow Special 0x96E450 / Final Form Death
//         0xB3DA20; static in mag258_mad_cow_special.cpp: same code below),
//     and the module's own handlers (Ifrit has `ret` stubs in these slots), ported here:
//       VM 0x0AB 0x95ABB0 BindLimitAnimSet: the slot entity (SceneHeader+0x60 + 4 * bone[0x1B], the
//         caster) is bound to the module's animation set (com data 0x187C20C) through a 0x20-byte
//         arena block (BS_BindEntityModelFromComData 0x5022C0), then QueueChainTransformation
//         0x505C00 (entity, s16 w1). Length 4;
//       VM 0x0B4 0x963B80 PosFromRandomJoint: the slot entity's bone matrices rebuilt from its pose
//         (0x508C90), joint = rand(0..bone count) (blob 0xB657A0, clamped to count - 1) of that
//         model (blob 0xB66B80, composed with the entity matrix +0x40, origin -> ws+0xF0..0xF8):
//         accumPos = that point << 16, then the bone handler. Length 2;
//       draw 71 0x95B400 JointTrail (+ 0x95B4A0 TrailSample / 0x95B950 JointFrame): a gradient
//         ribbon behind a joint of the slot entity. bone+0xB8 -> { u16 joint, u16 ring size N };
//         first call: a 0x10-byte arena block at bone+0xBC (u16 counter = 0) and the ring
//         0x2796440 (N entries of 0x14 bytes: u32 alive, base SVECTOR, tip SVECTOR) cleared.
//         Every call with outPos.y == 0: entity world bone matrices (0x5095B0 with the entity
//         matrix +0x40), the joint's position and its -z axis (0x56C600 + 0x56BC50), tip =
//         position + outPos.x * axis >> 12, stored at ring[counter % N]; the newest
//         min(N, 100 - counter) alive entries (newest first) splined (Effect_SplineSetup /
//         Effect_SplineEval 0x571620 / 0x571690, 32 points each, base -> 0x27961F0, tip ->
//         0x27962F0); then the entity's bone matrices rebuilt from its pose (0x508C90). 30
//         segments, each two POLY_G4 (0x24 B, code 0x3A: semi-transparent): base edge -> tip edge
//         in the bone colour (+0xCC, depth-cued per segment: IR0 = k * 0x1000 / 30, DPCS) and the
//         same quad's tip edge -> black, into the OT at OTZ (AVSZ4) of the base quad; the
//         segment's previous colour is the near edge's. counter += 1 every call.
// The effect's queue (0x2796158, pool 0x27963F8) holds one task, the engine tick 0x959F40.
//
// Writes outside the engine state: battle entities (the slot entity of the bone, SceneHeader+0x60 +
// 4 * bone[0x1B]): the caster's animation set / chain (VM 0x0AB) and skeleton (VM 0x0AD / 0x0B4 and
// the generic VM 0x05B: pose rebuild); the TARGET's rotation (+0x0C/+0x0E/+0x10), POSITION
// (+0x1C/+0x1E/+0x20) and flags +0/+1 (draw 38 AttachBattleEntity: the script hangs the first
// target on a node from tick ~160 on - it is carried off); a target's chain (VM 0x042); module data
// 0x2796168..0x2796454 (draw 71, not used by this module's script: spline points, curves, the ring,
// two unused direction vectors 0x2796408 / 0x2796420).
// Camera: VM 0x039 (engine). Battle results: VM 0x022 (generic, ApplyActionResultToTargets).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace desperado261
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag261_desperado_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
	void apply_brothers(Clone &c);  // mag_brothers.cpp

namespace desperado261
{
	static void describe(Clone &c)
	{
		c.name = "261 Desperado";
		c.effect_id = 261;
		c.vm_table = 0x185919C;     // MAG_261 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x1858E0C;   // MAG_261 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x1858E54;   // bone_table + 0x48
		c.prim_size = 0x1858F44;    // bone_table + 0x138
		c.spr_table = 0x1858F58;    // bone_table + 0x14C
		c.ptype_table = 0x1858F84;  // bone_table + 0x178
		c.pop_table = 0x1858F98;    // bone_table + 0x18C
		c.draw_table = 0x1858FF8;   // MAG_261 DrawHandlerTable
		c.attr_16A = 0x1859162;
		c.attr_184 = 0x185917C;
		c.attr_186 = 0x185917E;
		c.attr_194 = 0x185918C;
		c.attr_19C = 0x1859194;
		c.desc = 0x1858BD8;         // draw_table - 0x420
		c.ptr_block = 0x1858DE4;    // draw_table - 0x214 (read by 0x959980)
		c.queue = 0x2796158;
		c.file00 = 0x27961E8;
		c.file01 = 0x27961EC;
		c.tick = 0x959F40;
		c.load_cb = 0x960270;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// ------------------------------------------------------------------------------------
	// module data / engine calls not wrapped by gfc_engine.h
	// ------------------------------------------------------------------------------------
	static const uint32_t COMDATA = 0x187C20C;   // the module's animation set (VM 0x0AB)
	static const uint32_t RING = 0x2796440;      // draw 71 ring: N x { u32 alive, SVECTOR base, SVECTOR tip }
	static const uint32_t PTS_BASE = 0x2796168;  // gathered base points (8 B each)
	static const uint32_t PTS_TIP = 0x27961A8;   // gathered tip points
	static const uint32_t SPL_BASE = 0x27961F0;  // 32 curve points (8 B each)
	static const uint32_t SPL_TIP = 0x27962F0;
	static const uint32_t TAIL_BASE = 0x27963F0; // copy of SPL_TIP[0] (8 B)
	static const uint32_t DIR_TIP = 0x2796408;   // normalized SPL_TIP[0] - SPL_TIP[1] (3 x s32)
	static const uint32_t HEAD_BASE = 0x2796418; // copy of SPL_BASE[0] (8 B)
	static const uint32_t DIR_BASE = 0x2796420;  // normalized TAIL_BASE - SPL_BASE[0] (3 x s32)

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
	// 0x95ABB0 (VM 0x0AB BindLimitAnimSet): see the module comment
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
	// 0x963A90 (VM 0x0AD PosFromPosedEntityJoint, = Mad Cow Special 0x96E450 / Final Form Death
	// 0xB3DA20): the slot entity's bone matrices rebuilt from its pose (header entity+0x60, or
	// *(entity+0x78) when op bit 15 is set), joint w1 of that model (blob 0xB66B80, composed with
	// the entity matrix +0x40) applied to outPos -> accumPos = the camera-space point << 16. Then
	// the bone handler. Length 4.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AD_PosFromPosedEntityJoint()
	{
		uint8_t *ent = SlotEntity();                                          // edi
		uint8_t *hdr = (U8(RT(), 0x4B) & 0x80) ? PTR(ent, 0x78) : ent + 0x60; // esi
		// 30 fps layer: see mag261_desperado_held.inc
		FX_HELD(if (!held_pose_next(hdr)))
		x::BuildBoneMatricesFromPose(hdr);
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
	// 0x963B80 (VM 0x0B4 PosFromRandomJoint): see the module comment
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0B4_PosFromRandomJoint()
	{
		uint8_t *ent = SlotEntity();                                          // ebx
		uint8_t *hdr = ent + 0x60;                                            // esi
		// 30 fps layer: see mag261_desperado_held.inc
		FX_HELD(if (!held_pose_next(hdr)))
		x::BuildBoneMatricesFromPose(hdr);
		const int32_t n = U8(PTR(PTR(hdr, 4), 0), 0);                         // edi: bone count
		int32_t j = blob::Rand74(n);
		if (j >= n) j = n - 1;
		xl_JointMatrix_B66B80(hdr, j, 0x1000, ent + 0x40);
		S32(CUR(), 0x5C) = shl32(S32(WS(), 0xF0), 16);
		S32(CUR(), 0x60) = shl32(S32(WS(), 0xF4), 16);
		S32(CUR(), 0x64) = shl32(S32(WS(), 0xF8), 16);
		C().bone[U8(CUR(), 0x18)]();
		STREAM() += 2;
	}

	// ------------------------------------------------------------------------------------
	// draw 71 JointTrail
	// ------------------------------------------------------------------------------------
	// Field_Alloc(0x40) work block of 0x95B4A0
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

	// 0x95B950 JointFrame(hdr, joint, dir, pos): joint >= 0xF0 -> skeleton byte table [joint -
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

	// the drawing part of 0x95B4A0 (from the GTE set-up on): 30 segments of the two 32-point
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
	// -> pa, tip points -> pb (8 B each); returns the count (0x95B57C..0x95B5F4)
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

	// 0x95B4A0 TrailSample(data, blk)
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

	// 0x95B400 (draw 71 JointTrail): see the module comment
	static void __cdecl dh_71_JointTrail()
	{
		// 30 fps layer: see mag261_desperado_held.inc
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
		// 30 fps layer: see mag261_desperado_held.inc
		FX_HELD(held_note_dh71(b, ent, false);)
		if (S16(b, 0x96) != 0) return;
		x::ComputeBonesWorldMatrices(ent + 0x60, ent + 0x40);
		TrailSample(data, blk);
		x::BuildBoneMatricesFromPose(ent + 0x60);
		// 30 fps layer: see mag261_desperado_held.inc
		FX_HELD(held_note_dh71(b, ent, true);)
	}

	// ------------------------------------------------------------------------------------
	// handler sets: draw 38 (the only Brothers handler this module carries) and the shared
	// handlers (VM 0x042, prim 16), then the module's own handlers
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_brothers(c);
		const VoidFn d38 = c.draw[38];        // 0x95ACB0 = Brothers 0xAF59D0
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		apply_shared_misc(c);                 // VM 0x042 0x963E30, prim 16 0x95D100 (the live slots)
		c.draw[38] = d38;
		c.vm[0x0AB] = op_0AB_BindLimitAnimSet;          // 0x95ABB0
		c.vm[0x0AD] = op_0AD_PosFromPosedEntityJoint;   // 0x963A90
		c.vm[0x0B4] = op_0B4_PosFromRandomJoint;        // 0x963B80
		c.draw[71] = dh_71_JointTrail;                  // 0x95B400
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		// 30 fps layer: see mag261_desperado_held.inc
		FX_HELD(held_tick261();)
		return SequenceTick(g_c);
	}
}
}

	void register_mag261_desperado()
	{
		using namespace gfc;
		desperado261::describe(desperado261::g_c);
		init_clone(desperado261::g_c, nullptr, 0);
		desperado261::apply_slots(desperado261::g_c);
		register_port(0x959F40, (void *)desperado261::SequenceTask, "MAG261 Desperado SequenceTick", 261);
		// 30 fps layer: see mag261_desperado_held.inc
		FX_HELD(register_mag261_desperado_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag261_desperado_held.inc"
#endif
