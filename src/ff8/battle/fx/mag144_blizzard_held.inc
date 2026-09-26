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

// 30 fps part of mag144_blizzard.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag144_blizzard.cpp; the shared machinery is in mag_common_held.h).
//
// Held frame = every draw of the last real tick replayed in order (see mag_common_held.h):
//   Crystal    - spin and height half way; glint scroll half way; fade (+0x0C) half way inside
//                the fade-out ramp. Its shards: the real draw moves every flying shard one
//                step (angles, radius) and draws it with the new angles at the old radius; the
//                held draw runs the same render on a copy of the shard buffer as it was before
//                the real draw, each flying shard first moved half a step, so it draws half way
//                to its next pose. A shard that settles on the next tick and the shard set
//                (appearance delays, brightness steps) hold.
//   Shockwave  - size half way; fade half way inside the fade-out ramp.
//   Debris     - position half way along its sliding velocity; flipbook frame holds.
//   FallingIce - height half way along its falling velocity; flipbook frame holds.
//   MistPuff   - fixed place and size; colour half way inside its fade-out ramp; frame holds.
// The root and the directors draw nothing. In the draw-only mode everything holds.

namespace ff8fx
{
namespace blizzard144
{
	// real tick on which the ported root task last ran
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static DrawLog<512> g_draws;
	static uint8_t g_held_packets[0x20000];

	// the shard buffer of each crystal drawn on the real tick, as it was before its draw
	struct ShardSnap { int rec; uint8_t shards[0xA00]; };
	static ShardSnap g_shard_snap[4];
	static int g_nsnap = 0;
	// private shard buffer of a held crystal draw
	static uint8_t g_held_shards[0xA00];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_draws.begin();
		g_nsnap = 0;
	}

	static void held_note_draw(uint32_t fn, const void *node) { g_draws.add(fn, node, PacketCursor()); }

	static void held_note_crystal(const CrystalNode *p)
	{
		int n = g_draws.n;
		g_draws.add(ORIG_CrystalTask, p, PacketCursor());
		g_hdr_slot = nullptr; // the crystal header is written completely by its draw
		if (g_draws.n == n + 1 && g_nsnap < 4)
		{
			ShardSnap &s = g_shard_snap[g_nsnap++];
			s.rec = n;
			memcpy(s.shards, ShardBuffer(p->action), sizeof(s.shards));
		}
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_draws.current(); }

	static void ShockwaveHeld(const DrawRec &r, int num, int den, bool moving)
	{
		ShockwaveNode a, b;
		memcpy(&a, r.node, sizeof(a));
		b = a;
		if (moving) ShockwaveUpdate(&b);
		ShockwaveNode m = a;
		m.scale = lerp16(a.scale, b.scale, num, den);
		ShockwaveMatrix(&m);
		uint8_t *h = AllocHeader(0x58);
		ShockwaveHeader(h, a.counter);
		if (moving)
		{
			uint8_t next[0x58];
			memcpy(next, r.hdr, sizeof(next));
			ShockwaveHeader(next, b.counter);
			LerpPrimFade(h, next, num, den);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void MistHeld(const DrawRec &r, int num, int den, bool moving)
	{
		MistNode a;
		memcpy(&a, r.node, sizeof(a));
		TransformCameraByShadowRotation(a.pos, a.scale, -(a.scale >> 5));
		uint8_t *h = AllocHeader(0xB4);
		MistHeader(h, a.frame);
		if (moving && a.frame + 1 < 8)
		{
			uint8_t next[0xB4];
			memcpy(next, r.hdr, sizeof(next));
			MistHeader(next, (int16_t)(a.frame + 1));
			if (*(uint16_t *)(h + 0x24) == 0xC && *(uint16_t *)(next + 0x24) == 0xC)
				for (int k = 0x1C; k <= 0x1E; k++) h[k] = (uint8_t)lerp_i(h[k], next[k], num, den);
		}
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void CrystalHeld(int rec, const DrawRec &r, int num, int den, bool moving)
	{
		const ShardSnap *snap = nullptr;
		for (int i = 0; i < g_nsnap; i++) if (g_shard_snap[i].rec == rec) snap = &g_shard_snap[i];
		if (!snap) return;
		CrystalNode a, b;
		memcpy(&a, r.node, sizeof(a));
		b = a;
		if (moving) CrystalUpdate(&b);
		CrystalNode m = a;
		m.yaw = lerp16(a.yaw, b.yaw, num, den);
		m.pos[1] = lerp16(a.pos[1], b.pos[1], num, den);
		// every flying shard half a step ahead (the render then moves it one more step)
		memcpy(g_held_shards, snap->shards, sizeof(g_held_shards));
		if (moving)
		{
			Shard *s = (Shard *)g_held_shards;
			for (int i = 0; i < (int)(sizeof(g_held_shards) / sizeof(Shard)); i++, s++)
			{
				if (s->delay > a.counter || s->step + 1 >= s->steps) continue;
				for (int k = 0; k < 3; k++) s->angle[k] = lerp16(s->angle[k], (int16_t)(s->angle[k] - s->angle_vel[k]), num, den);
				s->radius = lerp16(s->radius, (int16_t)(s->radius - s->radius_vel), num, den);
			}
		}
		Mat4x3 mat = {};
		CrystalMatrix(&m, &mat);
		CrystalHeader *h = (CrystalHeader *)FieldAlloc(0xD0);
		CrystalHeaderInit(h, &a, &mat);
		h->shards = g_held_shards;
		if (moving)
		{
			h->scroll = lerp_i(a.counter * 8, (a.counter + 1) * 8, num, den);
			if (a.counter >= 0x28) h->ir0 = lerp_i((a.counter - 0x28) << 9, (a.counter + 1 - 0x28) << 9, num, den);
		}
		PacketCursor() = CrystalRender(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xD0);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_draws.packets);
		bool moving = !DrawOnly();
		for (int i = 0; i < g_draws.n; i++)
		{
			const DrawRec &r = g_draws.r[i];
			BeginReplay(r);
			switch (r.fn)
			{
			case ORIG_CrystalTask:
				g_hdr_slot = nullptr;
				CrystalHeld(i, r, num, den, moving);
				break;
			case ORIG_ShockwaveTask: ShockwaveHeld(r, num, den, moving); break;
			case ORIG_DebrisTask:
			{
				DebrisNode a, b;
				memcpy(&a, r.node, sizeof(a));
				b = a;
				if (moving) DebrisUpdate(&b);
				DebrisNode m = a;
				m.pos[0] = lerp16(a.pos[0], b.pos[0], num, den);
				m.pos[2] = lerp16(a.pos[2], b.pos[2], num, den);
				DebrisDraw(&m);
				break;
			}
			case ORIG_FallingIceTask:
			{
				FallingIceNode a, b;
				memcpy(&a, r.node, sizeof(a));
				b = a;
				if (moving) FallingIceUpdate(&b);
				FallingIceNode m = a;
				m.pos[1] = lerp16(a.pos[1], b.pos[1], num, den);
				FallingIceDraw(&m);
				break;
			}
			case ORIG_MistPuffTask: MistHeld(r, num, den, moving); break;
			default:
				g_hdr_slot = nullptr;
				break;
			}
		}
	}
}

	static void register_mag144_held()
	{
		mark_held(blizzard144::ORIG_CrystalTask);
		mark_held(blizzard144::ORIG_ShockwaveTask);
		mark_held(blizzard144::ORIG_DebrisTask);
		mark_held(blizzard144::ORIG_FallingIceTask);
		mark_held(blizzard144::ORIG_MistPuffTask);
		register_module_held(144, blizzard144::HeldReady, blizzard144::HeldFrame);
	}
}
