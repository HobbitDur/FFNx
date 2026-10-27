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

// 30 fps part of mag261_desperado.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form on the saved engine state. Camera:
// HeldCamera interpolates the battle camera words the VM writes (op 0x039).
//
// The handlers taken from other clones carry their own held handling: draw 38 AttachBattleEntity
// (mag_brothers_held.inc: runs on the in-between node, the attached entity's bytes 0..0x23 and
// skeleton saved before and put back after, so the entity keeps the real tick's state - the frame
// gate interpolates the entity; here its skeleton is put at the entity's in-between pose for the
// draw, so the pivot joint 0xF1 is the in-between one, see dh_38_AttachBattleEntityHeld), VM 0x042 /
// prim 16 (engine). VM 0x0AB / 0x0AD / 0x0B4 run only on real ticks and in the prediction (the
// animation set / chain binding is skipped while predicting by the wrappers); VM 0x0AD / 0x0B4 and
// the generic VM 0x05B on a battle entity read the entity's joints: while predicting they read the
// entity at its NEXT pose (held_pose_next: pose_midpoint at 1, the skeleton journaled and put back
// when the prediction ends), as the next tick's own pose rebuild does.
//
// Draw 71 JointTrail (the vanilla draw would add a ring entry and step the counter): a real tick
// records the curves it drew (0x27961F0 / 0x27962F0) and the caster's matrix it sampled with. A
// held frame draws the two curves half way (each point, the 4th words the real tick's) between
// the real tick's and the NEXT tick's: the next tick's ring entry from the joint of the caster at
// its NEXT pose (pose_midpoint at 1 of the caster's animation, through the caster's matrix of the
// next tick - the frame gate moves a moving caster to its in-between position: tick N's
// translation + (now - tick N's) * den / num - the skeleton and matrix put back afterwards), the
// tip length = the bone's next outPos.x (held_bone_ends), the newest min(N, 100 - counter)
// entries of a private ring copy splined by the exe's spline (0x571620 / 0x571690) on private
// buffers. Several bones on draw 71 share the module's ring (each writes ring[own counter % N]):
// the ring copy is taken at the frame's first trail bone and every bone's next entry goes into it
// in draw order, as the next tick writes them. Colour: the bone colour of the held state (the
// engine's). Holds the real tick's curves when the next tick draws no trail (outPos.y != 0,
// fewer than 2 points) and when the next tick is paused (battle update flags & 0x201); draws
// nothing when the real tick drew nothing. Nothing persistent is written: the ring, the counter,
// the module curves and the caster stay as the real tick left them.

#include <string.h>

namespace ff8fx
{
namespace gfc
{
namespace desperado261
{
	static uint32_t g_tick = 0;               // real ticks of the module (SequenceTask)
	struct Rec71
	{
		uint8_t *bone;
		bool drew;                            // the real draw drew a trail (curves valid)
		uint32_t cur0;                        // ctx+0x7C before the draw
		Mat4x3 mat;                           // the caster's matrix +0x40 the real draw sampled with
		uint8_t sa[0x100], sb[0x100];         // the real tick's curves
	};
	static Rec71 g_r71[4];
	static int g_n71 = 0;
	static uint32_t g_r71_tick = 0xFFFFFFFFu;

	static void held_tick261() { g_tick++; }

	static void held_note_dh71(uint8_t *bone, uint8_t *ent, bool after)
	{
		if (g_r71_tick != g_tick) { g_r71_tick = g_tick; g_n71 = 0; }
		Rec71 *r = nullptr;
		for (int i = 0; i < g_n71; i++) if (g_r71[i].bone == bone) r = &g_r71[i];
		if (!after)
		{
			if (r == nullptr)
			{
				if (g_n71 >= 4) return;
				r = &g_r71[g_n71++];
			}
			r->bone = bone;
			r->drew = false;
			r->cur0 = U32(CTX(), 0x7C);
			r->mat = *(const Mat4x3 *)(ent + 0x40);
			return;
		}
		if (r == nullptr) return;
		r->drew = U32(CTX(), 0x7C) != r->cur0;   // TrailDraw ran (it always moves the cursor)
		if (r->drew)
		{
			memcpy(r->sa, A(SPL_BASE), sizeof(r->sa));
			memcpy(r->sb, A(SPL_TIP), sizeof(r->sb));
		}
	}

	static uint8_t g_ring[64 * 0x14], g_pa[64 * 8], g_pb[64 * 8];
	static uint8_t g_na[0x100], g_nb[0x100], g_ma[0x100], g_mb[0x100];
	static uint8_t g_skel[0x10 + 0x30 * 256];

	// the bone's next ring entry into the frame's ring copy (as the next tick writes it) and, when
	// the real tick drew a trail, the next tick's curves into g_na / g_nb (false: the next tick
	// draws no trail, or it cannot be told: the real tick's curves hold)
	static bool TrailNext(const Rec71 &r, int num, int den)
	{
		if (UPDFLAGS() & 0x201) return false;
		uint8_t *b = CUR();
		int32_t len = S16(b, 0x94), y = S16(b, 0x96);
		int16_t a0[3], p0[3], a1[3], p1[3];
		if (held_bone_ends(b, a0, p0, a1, p1)) { len = p1[0]; y = p1[1]; }
		if (y != 0) return false;
		uint8_t *data = PTR(b, 0xB8), *blk = PTR(b, 0xBC);
		const int32_t size = U16(data, 2);
		if (blk == nullptr || size <= 0 || size > 64) return false;
		uint8_t *ent = SlotEntity();
		uint8_t *hdr = ent + 0x60;
		uint8_t *sk = PTR(PTR(hdr, 4), 0);
		const uint32_t skn = 0x10 + 0x30 * (uint32_t)U8(sk, 0);
		memcpy(g_skel, sk, skn);
		Mat4x3 *em = (Mat4x3 *)(ent + 0x40);
		const Mat4x3 now = *em;
		for (int k = 0; k < 3; k++) em->t[k] = r.mat.t[k] + (int32_t)((int64_t)(now.t[k] - r.mat.t[k]) * den / num);
		pose_midpoint(hdr, ent + 0x6C, 1, 1);
		x::ComputeBonesWorldMatrices(hdr, ent + 0x40);
		TrailWork *w = (TrailWork *)x::FieldAlloc(0x40);
		JointFrame(hdr, U16(data, 0), w->dir, w->base);
		TipFrom(w, len);
		memcpy(sk, g_skel, skn);
		*em = now;
		const int32_t c = U16(blk, 0);
		const int32_t slot = (int32_t)((uint32_t)c % (uint32_t)size);
		uint8_t *e = g_ring + 0x14 * slot;
		U32(e, 0) = 1;
		U32(e, 4) = U32(w->base, 0);
		U32(e, 8) = U32(w->base, 4);
		U32(e, 0xC) = U32(w->tip, 0);
		U32(e, 0x10) = U32(w->tip, 4);
		const int32_t n = TrailGather(g_ring, size, c, slot, g_pa, g_pb);
		if (n > 1 && r.drew)
		{
			memcpy(g_na, r.sa, sizeof(g_na));
			memcpy(g_nb, r.sb, sizeof(g_nb));
			uint8_t *work = (uint8_t *)x::FieldAlloc(0x190);
			TrailCurve(n, g_pa, work, g_na);
			TrailCurve(n, g_pb, work, g_nb);
			x::FieldFree(0x190);
		}
		x::FieldFree(0x40);
		return n > 1 && r.drew;
	}

	static void held_dh71()
	{
		uint8_t *b = CUR();
		if (g_r71_tick != g_tick) return;
		const Rec71 *r = nullptr;
		for (int i = 0; i < g_n71; i++) if (g_r71[i].bone == b) r = &g_r71[i];
		if (r == nullptr) return;
		if (r == &g_r71[0]) memcpy(g_ring, A(RING), sizeof(g_ring)); // the frame's first trail bone: the ring as the real tick left it
		const int num = g_held.num, den = g_held.den;
		const bool next = num != 0 && den > 0 && TrailNext(*r, num, den);
		if (!r->drew) return;
		const uint8_t *sa = r->sa, *sb = r->sb;
		if (next)
		{
			const int16_t *a0 = (const int16_t *)r->sa, *a1 = (const int16_t *)g_na;
			const int16_t *b0 = (const int16_t *)r->sb, *b1 = (const int16_t *)g_nb;
			int16_t *ma = (int16_t *)g_ma, *mb = (int16_t *)g_mb;
			for (int k = 0; k < 0x80; k++)
			{
				const bool pad = (k & 3) == 3; // (the 4th words: the real tick's)
				ma[k] = pad ? a0[k] : (int16_t)lerp_i(a0[k], a1[k], num, den);
				mb[k] = pad ? b0[k] : (int16_t)lerp_i(b0[k], b1[k], num, den);
			}
			sa = g_ma;
			sb = g_mb;
		}
		TrailWork *w = (TrailWork *)x::FieldAlloc(0x40);
		TrailDraw(sa, sb, U32(b, 0xCC), w);
		x::FieldFree(0x40);
	}

	// VM 0x0AD / 0x0B4 while predicting the next tick: the entity's bone matrices at its NEXT pose
	// (pose_midpoint at 1: the frame the next tick's own pose rebuild 0x508C90 reads), the skeleton
	// journaled (guard) and put back when the prediction ends; false otherwise (the vanilla rebuild)
	static bool held_pose_next(uint8_t *hdr)
	{
		if (!held_predicting()) return false;
		uint8_t *sk = PTR(PTR(hdr, 4), 0);
		guard(sk, 0x10 + 0x30 * (int)U8(sk, 0));
		pose_midpoint(hdr, hdr + 0xC, 1, 1);
		return true;
	}

	// VM 0x05B AttachToModelJoint (generic) on a battle entity (op bit 10) while predicting: the
	// engine skips the entity's pose rebuild (its skeleton is outside the snapshot), so the
	// prediction composed the joint of the skeleton as the battle left it (the real tick's pose):
	// the bones on the caster's joints held tick N on the held frames. The entity's NEXT pose is
	// put in first (held_pose_next, journaled), then the generic handler runs.
	static VoidFn g_op05B = nullptr;
	static void __cdecl op_05B_AttachToModelJointHeld()
	{
		if (held_predicting() && (U8(RT(), 0x4B) & 4)) held_pose_next(SlotEntity() + 0x60);
		g_op05B();
	}

	// draw 38 AttachBattleEntity (Brothers' port, which saves and restores the entity around a
	// held draw) reads joint 0xF1 of the attached entity's skeleton (blob 0xB66B80): on a held frame
	// the skeleton is put at the entity's in-between pose first (pose_midpoint num / den), and put
	// back after the draw, so the pivot is the in-between one (the held frame at 1 = the next tick's)
	static VoidFn g_d38 = nullptr;
	static uint8_t g_skel38[0x10 + 0x30 * 256];
	static void __cdecl dh_38_AttachBattleEntityHeld()
	{
		if (!held_active()) { g_d38(); return; }
		uint8_t *ent = SlotEntity();
		uint8_t *sk = PTR(PTR(ent + 0x60, 4), 0);
		const uint32_t n = sk ? 0x10 + 0x30 * (uint32_t)U8(sk, 0) : 0;
		if (sk) { memcpy(g_skel38, sk, n); pose_midpoint(ent + 0x60, ent + 0x6C, g_held.num, g_held.den); }
		g_d38();
		if (sk) memcpy(sk, g_skel38, n);
	}

	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag261_desperado_held()
	{
		using namespace gfc;
		desperado261::g_op05B = desperado261::g_c.vm[0x05B];
		desperado261::g_c.vm[0x05B] = desperado261::op_05B_AttachToModelJointHeld;
		desperado261::g_d38 = desperado261::g_c.draw[38];
		desperado261::g_c.draw[38] = desperado261::dh_38_AttachBattleEntityHeld;
		mark_held(0x959F40);
		register_module_held(261, desperado261::Ready, desperado261::Frame);
		register_module_camera(261, desperado261::Camera);
	}
}
