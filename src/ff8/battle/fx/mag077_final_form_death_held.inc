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

// 30 fps part of mag077_final_form_death.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks,
// every draw handler re-run in its frozen (boneSkipFlag) form. Camera: HeldCamera interpolates
// the battle camera words the VM writes (op 0x039).
//
// Draw 22 (EntityDisintegrate) has its own held draw (no launch, no rand, nothing written):
//   model  - the polygons still on the model on the real tick, at the in-between parent
//            transform and the entity's in-between pose (pose_midpoint of the entity's
//            animation; the entity record and its skeleton are saved before and put back
//            after the draw),
//   pieces - every piece the real tick drew at the exact in-between of its position N and the
//            next step's position (velocity + acceleration, position + velocity << 4 on the
//            axes the mode moves), colour of the real tick; held at N when the real tick was
//            frozen (the step is gated by the freeze) or when the piece ends on the next tick
//            (life <= 0: freed by the next step); colour = table[life] of the real tick (table[life
//            - 1], the next tick's, at the full step),
//   dots   - stepped on every draw: the in-between of the drawn state N and the next step
//            (mode 6: radius / angle in between, then the polar transform), colour of the real
//            tick (the next tick's at the full step); a dot whose life runs out on the next step
//            is held at N.
// Launches (new pieces, dots, particles) stay 15 Hz: they appear on the real tick.

namespace ff8fx
{
namespace gfc
{
namespace final_form_death
{
	static const uint32_t ENTITY_SIZE = 0x9C;   // battle entity record (0x1D972C0 + slot * 0x9C)
	static uint8_t g_ent_save[ENTITY_SIZE];
	static uint8_t g_skel_save[0x10 + 0x30 * 256];

	// in-between position of one axis: pos + ((velocity + acceleration) << 4) * num / den
	static inline int32_t held_axis(const uint8_t *p, int vel, int acc, int pos, bool move)
	{
		if (!move) return S32(p, pos);
		const int16_t v = (int16_t)(U16(p, vel) + U16(p, acc));
		return add32(S32(p, pos), (int32_t)((int64_t)shl32(v, 4) * g_held.num / g_held.den));
	}

	// 0xB34950 on a held frame: the pieces at their in-between positions (nothing written)
	static void DrawPiecesHeld()
	{
		uint8_t *st = PTR(WS(), 0x84);
		uint8_t *pk = PTR(CTX(), 0x7C);
		uint8_t *p = st + 0x20;
		h_B27130();
		const uint32_t mode = U16(PTR(WS(), 0x90), 8);
		const bool step = g_held.real_skip == 0 && mode <= 7 && mode != 4 && g_held.num > 0;
		const bool xz = mode == 1 || mode == 2 || mode == 3;
		for (int32_t n = (int32_t)U16(st, 2) + 1; n > 0; n--, p += 0x44)
		{
			if (!PTR(p, 0)) continue;
			const bool move = step && S8(p, 0x2E) > 0;
			const int8_t life = (int8_t)(S8(p, 0x2E) - (move && g_held.num >= g_held.den ? 1 : 0));   // the next tick's colour at the full step
			DrawPiece(pk, p, held_axis(p, 0x3C, 0x3E, 4, move && xz), held_axis(p, 0x16, 0x1E, 8, move), held_axis(p, 0x40, 0x42, 0xC, move && xz), life);
		}
		PTR(CTX(), 0x7C) = pk;
	}

	// 0xB341D0 on a held frame: the dots at the in-between of N and the next step (nothing written)
	static void DotsHeld()
	{
		uint8_t *d = PTR(WS(), 0x8C);
		uint8_t *desc = PTR(WS(), 0x90);
		uint8_t *pk = PTR(CTX(), 0xD8);
		const uint32_t mode = U16(desc, 8);
		for (int32_t n = U16(desc, 0x2E); n > 0; n--, d += 0x1C)
		{
			if (!U8(d, 0x18)) continue;
			const bool move = mode <= 7 && mode != 4 && (int8_t)(S8(d, 0x19) - 1) >= 0 && g_held.num > 0;
			const int8_t life = (int8_t)(S8(d, 0x19) - (move && g_held.num >= g_held.den ? 1 : 0));   // the next tick's colour at the full step
			DrawDot(pk, desc, life, held_axis(d, 0xC, 0xE, 0, move), held_axis(d, 0x10, 0x12, 4, move), held_axis(d, 0x14, 0x16, 8, move));
			pk += 0xC;
		}
		PTR(CTX(), 0xD8) = pk;
	}

	// 0xB322E0 on a held frame: the frozen path (no update), the entity at its in-between pose
	static void dh_22_EntityDisintegrateHeld()
	{
		if (U32(CUR(), 0xBC) == 0) return;   // first draw: on a real tick only
		uint8_t *bone = CUR();
		PTR(WS(), 0x90) = PTR(bone, 0xB8);
		PTR(WS(), 0x80) = PTR(CUR(), 0xBC);
		PTR(WS(), 0x84) = PTR(CUR(), 0xC0);
		uint8_t *st = PTR(WS(), 0x84);
		PTR(WS(), 0x88) = PTR(CUR(), 0xC4);
		PTR(WS(), 0x8C) = PTR(CUR(), 0xA0);
		U32(WS(), 0x48) = U32(st, 8);
		U32(WS(), 0x4C) = U32(st, 0xC);
		uint8_t *blk = PTR(WS(), 0x80);
		uint8_t *ent = PTR(blk, 0xC);
		memcpy(g_ent_save, ent, ENTITY_SIZE);
		uint8_t *m = PTR(ent, 0x64);
		uint8_t *sk = m ? PTR(m, 0) : nullptr;
		const uint32_t sk_n = sk ? 0x10 + 0x30 * (uint32_t)U8(sk, 0) : 0;
		if (sk) memcpy(g_skel_save, sk, sk_n);
		SyncEntityModelCopy_B31ED0(blk);
		pose_midpoint(blk + 0x10, ent + 0x6C, g_held.num, g_held.den);
		h_B27130();
		x::ComputeBonesWorldMatrices(blk + 0x10, WS() + 0xE0);
		U32(blk, 0x44) = U32(CTX(), 0x74);
		DrawModel_B34470();
		Dots_B341D0();
		if (sk) memcpy(sk, g_skel_save, sk_n);
		memcpy(ent, g_ent_save, ENTITY_SIZE);
	}

	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag077_final_form_death_held()
	{
		using namespace gfc;
		mark_held(0xB30990);
		register_module_held(77, final_form_death::Ready, final_form_death::Frame);
		register_module_camera(77, final_form_death::Camera);
	}
}
