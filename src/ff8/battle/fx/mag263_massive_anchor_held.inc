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

// 30 fps part of mag263_massive_anchor.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form on the saved engine state. Camera:
// HeldCamera interpolates the battle camera words the VM writes (op 0x039).
//
// VM 0x0AB runs only on real ticks and in the prediction (the animation set / chain binding is
// skipped while predicting by its wrappers). Draw 70 (the anchor's attach offset in the weapon
// block, read by the battle when it draws the caster) writes nothing on a held frame: the battle
// draws the caster and his weapon outside the effect (the frame gate interpolates them), and a
// held write would reach the next real tick's battle code before the real draw 70 rewrites it.
// VM 0x05B (generic AttachToModelJoint) on a battle entity: see op_05B_AttachToModelJointHeld.

namespace ff8fx
{
namespace gfc
{
namespace anchor263
{
	// VM 0x05B AttachToModelJoint (generic) on a battle entity (op bit 10) while predicting: the
	// engine skips the entity's pose rebuild (its skeleton is outside the snapshot), so the
	// prediction composed the joint of the skeleton as the battle left it (the real tick's pose):
	// the bones on the caster's joints held tick N on the held frames. The entity's NEXT pose
	// (pose_midpoint at 1: the frame the next tick's own pose rebuild 0x508C90 reads) is put in
	// first, the skeleton journaled (guard) and put back when the prediction ends; then the generic
	// handler runs.
	static VoidFn g_op05B = nullptr;
	static void __cdecl op_05B_AttachToModelJointHeld()
	{
		if (held_predicting() && (U8(RT(), 0x4B) & 4))
		{
			uint8_t *hdr = SlotEntity() + 0x60;
			uint8_t *sk = PTR(PTR(hdr, 4), 0);
			guard(sk, 0x10 + 0x30 * (int)U8(sk, 0));
			pose_midpoint(hdr, hdr + 0xC, 1, 1);
		}
		g_op05B();
	}

	// held-frame entity (fx_held.h HeldEntity, drawn by the frame gate): VM 0x048 OverrideEntityTransform puts Ward's
	// entity at a bone's outPos (+0x1C..+0x20) / outAngle (+0x0C..+0x10) on the real ticks of his leap. On a real tick
	// the op's entity is noted with its components before the first and after the last write; a step larger than the
	// engine's own cut (outPos 0x2000 as gfc_engine_held.inc CUT_POS, an angle step beyond 0x400) is a placement: the
	// whole entity holds. Listed: the entities the op wrote on the tick just run (only they changed).
	struct EntTick { uint8_t *ent; int16_t pre[6], post[6]; };
	static EntTick g_et[4];
	static int g_net = 0;
	static uint32_t g_et_tick = 0xFFFFFFFFu;
	static VoidFn g_op048 = nullptr;
	static void __cdecl op_048_OverrideEntityTransformHeld()
	{
		if (held_predicting()) { g_op048(); return; }
		uint8_t *e = SlotEntity();
		if (g_et_tick != g_real_tick) { g_et_tick = g_real_tick; g_net = 0; }
		EntTick *r = nullptr;
		for (int i = 0; i < g_net; i++) if (g_et[i].ent == e) r = &g_et[i];
		if (!r && g_net < 4)
		{
			r = &g_et[g_net++];
			r->ent = e;
			for (int c = 0; c < 6; c++) r->pre[c] = *held_entity_field(e, c);
		}
		g_op048();
		if (r) for (int c = 0; c < 6; c++) r->post[c] = *held_entity_field(e, c);
	}

	static int HeldEntities(HeldEntity *out, int max)
	{
		if (!HeldReady(g_c) || g_et_tick != g_real_tick) return 0;
		int n = 0;
		for (int i = 0; i < g_net && n < max; i++)
		{
			HeldEntity &h = out[n++];
			held_entity_init(h, g_et[i].ent);
			bool cut = false;
			for (int c = 0; c < 6; c++)
			{
				int32_t d = (int32_t)g_et[i].post[c] - g_et[i].pre[c];
				if (c >= 3) d = ((d + 0x800) & 0xFFF) - 0x800;
				if (d > (c < 3 ? 0x2000 : 0x400) || d < -(c < 3 ? 0x2000 : 0x400)) cut = true;
			}
			if (cut) for (int c = 0; c < 6; c++) held_entity_jump(h, c);
		}
		return n;
	}

	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag263_massive_anchor_held()
	{
		using namespace gfc;
		anchor263::g_op05B = anchor263::g_c.vm[0x05B];
		anchor263::g_c.vm[0x05B] = anchor263::op_05B_AttachToModelJointHeld;
		anchor263::g_op048 = anchor263::g_c.vm[0x048];
		anchor263::g_c.vm[0x048] = anchor263::op_048_OverrideEntityTransformHeld;
		mark_held(0x945E80);
		register_module_held(263, anchor263::Ready, anchor263::Frame);
		register_module_camera(263, anchor263::Camera);
		register_module_entity(263, anchor263::HeldEntities);
	}
}
