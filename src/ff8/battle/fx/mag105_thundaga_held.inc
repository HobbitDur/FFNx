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

// 30 fps part of mag105_thundaga.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag105_thundaga.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// A real tick records every draw in order with the GTE registers and the Field_Alloc scratch it
// started from (headers and work blocks keep the scratch bytes where the code does not write).
// A held frame replays the log in the same order:
//   Debris  - drawn by the root after its update (update then draw): each debris drawn on the
//             real tick is drawn num / den of the way to its next state: position along its
//             velocity, size toward the next shrink step, corners rotated by the same fraction
//             of the per-tick spin (axis-angle of its rotation matrix); debris spawned after the
//             draw appear on the next real tick.
//   Bolt    - the lightning model through prim::play_held: every object's exact in-between record
//             (position, rotation, scale, colour, fade, morph weight); the UV scroll step of the
//             scrolled parts holds (the frame the real tick drew). Morph vertices go to a private
//             buffer.
//   Capture - the frame-28 render-to-texture is replayed as drawn (private prim copies and arena).
//   Shard   - position along its velocity, angles along the spin.
//   Spark   - position along its velocity; sequence frame holds.
//   Flash   - sequence frame at a fixed place: holds.
// No Thundaga task has a draw-only mode: everything moves on every tick.

#include <math.h>

namespace ff8fx
{
namespace thundaga105
{
	static const uint32_t GTE_DATA = 0x1CA8A10, GTE_DATA_SIZE = 0x70, GTE_CTRL = 0x1CA9230, GTE_CTRL_SIZE = 0xD0;
	static const uint32_t NODE_MAX = 0x3C, SCRATCH = 0x400;
	enum : uint32_t { REC_DEBRIS = 1, REC_BOLT = 2, REC_CAPTURE = 3 };

	struct Rec
	{
		uint32_t fn;                 // original task function, or REC_*
		uint8_t node[NODE_MAX];
		const void *live;            // the bolt node (its prim layout is played live)
		BoltArg arg;
		int32_t frame;               // prim layout frame the bolt drew
		int16_t anchor[4], box[6];   // capture
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0; // packet cursor at the tick's first draw
	static Rec g_recs[256];
	static Debris g_debris[DEBRIS_COUNT], g_debris_mid[DEBRIS_COUNT];
	static uint8_t g_held_packets[0x40000];
	static uint8_t g_morph[0x7D0];      // magic buffer + 0x2254 .. + 0x2A24
	static uint8_t g_arena[0x18000];    // second arena
	static uint8_t g_env[0x40], g_area[0x10], g_offset[0xC], g_offset_xy[8];
	static bool g_part_held = false;
	static uint16_t g_part_frame = 0;

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
	}

	static Rec *NewRec(uint32_t fn)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_debris()
	{
		if (NewRec(REC_DEBRIS)) memcpy(g_debris, DebrisSlots(), sizeof(g_debris));
	}

	static void held_note_bolt(const BoltNode *p, const BoltArg *arg)
	{
		Rec *r = NewRec(REC_BOLT);
		if (!r) return;
		r->live = p;
		r->arg = *arg;
		r->frame = ((const prim::Layout *)p->layout)->frame;
	}

	static void held_note_capture(const int16_t *anchor, const int16_t *box)
	{
		Rec *r = NewRec(REC_CAPTURE);
		if (!r) return;
		memcpy(r->anchor, anchor, 6);
		memcpy(r->box, box, 12);
	}

	static void held_note_draw(uint32_t fn, const void *node, uint32_t size)
	{
		Rec *r = NewRec(fn);
		if (!r) return;
		memset(r->node, 0, sizeof(r->node));
		memcpy(r->node, node, size < NODE_MAX ? size : NODE_MAX);
	}

	// the scrolled parts of a held bolt keep the UV step of the frame the real tick drew
	static uint16_t held_part_frame(uint16_t frame) { return g_part_held ? g_part_frame : frame; }

	// rotation matrix (4.12) to the power num / den: same axis, angle scaled (identity at 0)
	static void RotationPower(const int16_t *m, int num, int den, Mat4x3 *out)
	{
		memset(out, 0, sizeof(*out));
		double r[3][3];
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) r[i][j] = m[i * 3 + j] / 4096.0;
		double c = (r[0][0] + r[1][1] + r[2][2] - 1.0) * 0.5;
		if (c > 1.0) c = 1.0;
		if (c < -1.0) c = -1.0;
		double th = acos(c);
		double x = r[2][1] - r[1][2], y = r[0][2] - r[2][0], z = r[1][0] - r[0][1];
		double len = sqrt(x * x + y * y + z * z);
		if (num == 0 || len < 1e-9 || th < 1e-9)
		{
			out->m[0][0] = out->m[1][1] = out->m[2][2] = 0x1000;
			return;
		}
		x /= len;
		y /= len;
		z /= len;
		double a = th * num / den, ca = cos(a), sa = sin(a), t = 1.0 - ca;
		double q[3][3] = {
			{ t * x * x + ca, t * x * y - sa * z, t * x * z + sa * y },
			{ t * x * y + sa * z, t * y * y + ca, t * y * z - sa * x },
			{ t * x * z - sa * y, t * y * z + sa * x, t * z * z + ca },
		};
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) out->m[i][j] = (int16_t)floor(q[i][j] * 4096.0 + 0.5);
	}

	// the debris drawn on the real tick, num / den of the way to their next state
	static void DebrisHeld(int num, int den)
	{
		memcpy(g_debris_mid, g_debris, sizeof(g_debris_mid));
		if (num == 0) return;
		for (int i = 0; i < DEBRIS_COUNT; i++)
		{
			Debris &e = g_debris_mid[i];
			if (e.size == 0) continue;
			const Debris &a = g_debris[i];
			e.size = lerp16(a.size, (int16_t)(a.size + a.dsize), num, den);
			for (int k = 0; k < 3; k++) e.pos[k] = lerp16(a.pos[k], (int16_t)(a.pos[k] + Sar16(a.vel[k], 4)), num, den);
			Mat4x3 part;
			RotationPower(a.rot, num, den, &part);
			GteSetRotMatrixCtrl(&part);
			for (int k = 0; k < 3; k++)
			{
				GteLoadV0(e.v[k]);
				GteMVMVA_RotV0();
				GteStoreIR123(e.v[k]);
			}
		}
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// (the debris' in-between corners use the GTE before the draw's own state is restored)
			if (r.fn == REC_DEBRIS) DebrisHeld(num, den);
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.fn)
			{
			case REC_DEBRIS:
				PacketCursor() = DebrisDraw(g_debris_mid, RenderOT44(), 2, PacketCursor());
				break;
			case REC_BOLT:
			{
				BoltArg arg = r.arg;
				memcpy(g_morph, TexBase() + 0x2254, sizeof(g_morph));
				arg.morph = g_morph;
				g_part_held = true;
				g_part_frame = (uint16_t)r.frame;
				prim::play_held((prim::Layout *)((const BoltNode *)r.live)->layout, BoltPart, (int)&arg, num, den);
				g_part_held = false;
				break;
			}
			case REC_CAPTURE:
			{
				memcpy(g_env, PRIM_Env, sizeof(g_env));
				memcpy(g_area, PRIM_Area, sizeof(g_area));
				memcpy(g_offset, PRIM_Offset, sizeof(g_offset));
				memcpy(g_offset_xy, RECT_Offset, sizeof(g_offset_xy));
				CapturePrims c = { g_env, g_area, g_offset, (int16_t *)g_offset_xy, (uint32_t)g_arena };
				CaptureDraw(r.anchor, r.box, c);
				break;
			}
			case ORIG_ShardTask:
			{
				ShardNode a, b;
				memcpy(&a, r.node, sizeof(a));
				b = a;
				ShardUpdate(&b);
				ShardNode m = a;
				for (int k = 0; k < 3; k++)
				{
					m.pos[k] = lerp16(a.pos[k], b.pos[k], num, den);
					m.ang[k] = lerp16(a.ang[k], b.ang[k], num, den);
				}
				ShardDraw(&m);
				break;
			}
			case ORIG_SparkTask:
			{
				SpriteNode a, b;
				memcpy(&a, r.node, sizeof(a));
				b = a;
				SparkUpdate(&b);
				SpriteNode m = a;
				for (int k = 0; k < 3; k++) m.pos[k] = lerp16(a.pos[k], b.pos[k], num, den);
				SpriteDraw(&m);
				break;
			}
			case ORIG_FlashTask:
			{
				SpriteNode a;
				memcpy(&a, r.node, sizeof(a));
				SpriteDraw(&a);
				break;
			}
			}
		}
	}
}

	static void register_mag105_held()
	{
		mark_held(thundaga105::ORIG_RootTask); // the root draws the debris
		mark_held(thundaga105::ORIG_BoltTask);
		mark_held(thundaga105::ORIG_ShardTask);
		mark_held(thundaga105::ORIG_SparkTask);
		mark_held(thundaga105::ORIG_FlashTask);
		register_module_held(105, thundaga105::HeldReady, thundaga105::HeldFrame);
	}
}
