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

// 30 fps part of act_engine.cpp: the held-frame core of the actor engine family - notes of the
// real tick (master ran, camera stepper ran, prim-model plays), the held-frame camera (the next
// camera step predicted on saved state), the held-frame redraw of a module (prim plays + creature
// actors at their midpoint pose) and the held-task registration
// (included at the end of act_engine.cpp)

namespace ff8fx
{
namespace act
{
	// ------------------------------------------------------------------------------------
	// held frames (30 fps)
	// ------------------------------------------------------------------------------------
	// real tick on which a ported master last ran (held frames need their memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;

	// master tasks (a_739F40, m_731F70, t_7624D0, b_729BD0): this real tick ran the ported effect
	void held_note_master() { g_ported_tick = g_real_tick; }

	// camera: the engine's two camera steppers (a_73AE10 interpolated move, a_73B4B0 keyframe
	// tracks) both end in a_73B160, which writes the battle camera words; they note here that
	// they ran on this real tick
	static uint32_t g_cam_tick = 0xFFFFFFFF;
	static int g_cam_kind = 0; // 1 = a_73AE10, 2 = a_73B4B0

	void held_note_camera(int kind)
	{
		g_cam_tick = g_real_tick; // held-frame camera: this stepper ran on this tick
		g_cam_kind = kind;
	}

	// saves / restores everything a camera step writes: the module's camera block (0xEC bytes
	// at *G_15339D0), the camera words, projection distance, 0x1D977A2 and the GTE registers
	struct CamSave
	{
		uint8_t block[0xEC];
		uint8_t words[0x10];
		uint16_t proj_h, w977a2;
		uint8_t gte[0x1CA9300 - 0x1CA8A10];
		uint32_t fa;
		uint8_t scratch[0x1000];
	};
	static void cam_save(CamSave &s, uint32_t blk)
	{
		memcpy(s.block, (const void *)blk, sizeof(s.block));
		memcpy(s.words, (const void *)0xB8B7F0, sizeof(s.words));
		s.proj_h = MEM<uint16_t>(0x1D8E038);
		s.w977a2 = MEM<uint16_t>(0x1D977A2);
		memcpy(s.gte, (const void *)0x1CA8A10, sizeof(s.gte));
		s.fa = MEM<uint32_t>(0x1D999C4); // the matrix helpers use the Field_Alloc scratch
		memcpy(s.scratch, (const void *)s.fa, sizeof(s.scratch));
	}
	static void cam_restore(const CamSave &s, uint32_t blk)
	{
		memcpy((void *)blk, s.block, sizeof(s.block));
		memcpy((void *)0xB8B7F0, s.words, sizeof(s.words));
		MEM<uint16_t>(0x1D8E038) = s.proj_h;
		MEM<uint16_t>(0x1D977A2) = s.w977a2;
		memcpy((void *)0x1CA8A10, s.gte, sizeof(s.gte));
		memcpy((void *)s.fa, s.scratch, sizeof(s.scratch));
		MEM<uint32_t>(0x1D999C4) = s.fa;
	}

	bool held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (g_cam_tick != g_real_tick || g_ported_tick != g_real_tick) return false;
		uint32_t blk = MEM<uint32_t>(G(G_15339D0));
		if (!blk) return false;
		const int16_t *eye = (const int16_t *)0xB8B7F0, *at = (const int16_t *)0xB8B7F8;
		int16_t e0[3] = { eye[0], eye[1], eye[2] }, a0[3] = { at[0], at[1], at[2] };
		// predict: the same stepper runs on the next tick (pure memory + GTE, restored below)
		static CamSave s;
		cam_save(s, blk);
		uint32_t tick = g_cam_tick;
		int kind = g_cam_kind;
		if (kind == 1) a_73AE10();
		else a_73B4B0();
		int16_t e1[3] = { eye[0], eye[1], eye[2] }, a1[3] = { at[0], at[1], at[2] };
		cam_restore(s, blk);
		g_cam_tick = tick;
		g_cam_kind = kind;
		// a jump larger than a normal step is a cut: hold
		for (int i = 0; i < 3; i++)
		{
			int32_t de = e1[i] - e0[i], da = a1[i] - a0[i];
			if (de > 4000 || de < -4000 || da > 4000 || da < -4000)
			{
				for (int k = 0; k < 3; k++) { world[k] = e0[k]; lookat[k] = a0[k]; }
				return true;
			}
		}
		for (int i = 0; i < 3; i++)
		{
			world[i] = (int16_t)lerp_i(e0[i], e1[i], num, den);
			lookat[i] = (int16_t)lerp_i(a0[i], a1[i], num, den);
		}
		return true;
	}

	// held frame of a module: in-between redraw of its prim-model plays (prim::play_held) and of
	// its creature actor(s) (pose_midpoint + DrawModel a_746C10), into a private packet buffer.
	// Everything these draws write besides packets is saved and put back: the module's globals
	// (the prim callbacks refresh module tables, e.g. Siren's orbit key tables), the module scratch
	// stack, the Field_Alloc scratch, the shadow temporaries of 0x5088A0, the packet cursor, the
	// creature's model matrix / bbox / bone matrices (rebuilt from the pose by DrawModel itself)
	static uint8_t g_held_packets[0x100000];
	static uint8_t g_held_bss[0x20000];
	void held_frame(const HeldDesc &d, int num, int den)
	{
		if (g_ported_tick != g_real_tick) return;
		const Mod *saved_mod = g_mod;
		g_mod = d.mod;
		uint32_t bss_n = d.bss_hi - d.bss_lo;
		if (bss_n > sizeof(g_held_bss)) bss_n = sizeof(g_held_bss);
		memcpy(g_held_bss, (const void *)d.bss_lo, bss_n);
		static uint8_t camcopy[0x20], scratch[0x8000], shadow[0x80], gte[0x1CA9300 - 0x1CA8A10];
		memcpy(gte, (const void *)0x1CA8A10, sizeof(gte)); // GTE register files (the gate saves them too)
		uint32_t parsepoly[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) }; // RenderGeometry (ParsePolygons) temporaries
		memcpy(camcopy, (const void *)0x2793E58, sizeof(camcopy));
		uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(scratch, (const void *)fa, sizeof(scratch));
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		MEM<uint32_t>(0x1D8E054) = P(g_held_packets);

		held_draw_prim_plays(num, den);

		auto draw_creatures = [&](uint32_t queue, uint32_t task, uint32_t draw_arg)
		{
			for (uint32_t node = MEM<uint32_t>(queue); node; node = U32(node, 4))
			{
				if (U32(node, 8) != task) continue;
				if (U8(node, 0x26) & 4) continue; // hidden: the real tick did not draw it either
				static uint8_t mdl[0x40], place[0x30];
				memcpy(mdl, (const void *)(node + 0x60), sizeof(mdl)); // bbox +0x64..0x6E, matrix +0x70..0x8F
				memcpy(place, (const void *)(node + 0x30), sizeof(place)); // placement (position +0x4C..0x50)
				pose_midpoint((void *)(node + 0x90), (void *)(node + 0x9C), num, den);
				if (d.adjust) d.adjust(node, num, den);
				MEM<uint32_t>(0x1D8E054) = a_746C10(node, draw_arg, MEM<uint32_t>(0x1D8E054));
				memcpy((void *)(node + 0x30), place, sizeof(place));
				memcpy((void *)(node + 0x60), mdl, sizeof(mdl));
			}
		};
		draw_creatures(d.creature_queue, d.creature_task, d.draw_arg);
		for (const HeldCreature *c = d.more; c && c->queue; c++)
			draw_creatures(c->queue, c->task, c->draw_arg);

		MEM<uint32_t>(0x1D8E054) = cursor;
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		memcpy((void *)fa, scratch, sizeof(scratch));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)0x2793E58, camcopy, sizeof(camcopy));
		memcpy((void *)d.bss_lo, g_held_bss, bss_n);
		memcpy((void *)0x1CA8A10, gte, sizeof(gte));
		MEM<uint32_t>(0x1D99C18) = parsepoly[0];
		MEM<uint32_t>(0x1D99C1C) = parsepoly[1];
		g_mod = saved_mod;
	}

	bool held_ready() { return g_ported_tick == g_real_tick; }

	// ------------------------------------------------------------------------------------
	// prim-model player (prim_play): every play of the real tick is remembered (layout, ported
	// callback, the 0x5C-byte parameter block) so the held frame can redraw it in between
	// (prim::play_held)
	// ------------------------------------------------------------------------------------
	struct PrimPlay { uint32_t layout, cb; const Mod *mod; uint8_t arg[0x5C]; };
	static PrimPlay g_prim_plays[256];
	static int g_nprim_plays = 0;
	static uint32_t g_prim_tick = 0xFFFFFFFF;

	// prim_play, after the play: f = the callback it ran (port or original)
	void held_note_prim_play(uint32_t layout, uint32_t f, uint32_t arg)
	{
		if (g_prim_tick != g_real_tick) { g_prim_tick = g_real_tick; g_nprim_plays = 0; }
		if (g_nprim_plays < 256)
		{
			PrimPlay &m = g_prim_plays[g_nprim_plays++];
			m.layout = layout;
			m.cb = f;
			m.mod = g_mod;
			memcpy(m.arg, (const void *)arg, sizeof(m.arg));
		}
	}

	void held_draw_prim_plays(int num, int den)
	{
		if (g_prim_tick != g_real_tick) return;
		for (int i = 0; i < g_nprim_plays; i++)
		{
			const PrimPlay &m = g_prim_plays[i];
			if (m.mod != g_mod) continue;
			alignas(4) uint8_t arg[0x5C];
			memcpy(arg, m.arg, sizeof(arg));
			ff8fx::prim::play_held((ff8fx::prim::Layout *)m.layout, (ff8fx::prim::Callback)m.cb, (int)P(arg), num, den);
		}
	}

	// ------------------------------------------------------------------------------------
	// registration
	// ------------------------------------------------------------------------------------
	// held tasks of a module among its engine ports (register_module registers them): marked held
	// = the module's held frame redraws their drawing (the generic packet replay skips them)
	void register_module_held_tasks(int effect_id, const uint32_t *held_tasks)
	{
		for (const AddrPort *p = engine_ports(effect_id); p && p->addr; p++)
		{
			bool held = false;
			for (const uint32_t *h = held_tasks; h && *h; h++) held |= *h == p->addr;
			if (held) mark_held(p->addr);
		}
	}
}
}
