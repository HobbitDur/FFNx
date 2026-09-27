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

// 30 fps part of mag040_scan.cpp: the draw log of the real tick, the held-frame drawing and the
// held-frame camera (included at the end of mag040_scan.cpp; shared pieces in mag_common_held.h /
// fx_held.h).
//
// Every Scan draw comes after its task's update. A real tick records every draw in order: the node as
// drawn, the GTE registers and the Field_Alloc scratch the draw started with. A held frame replays
// the log in the same order, from the same GTE state and scratch, each node moved half way to the
// state its own next update computes (the task's phase functions run on a copy; the reticle's next
// point comes from its path table). Everything driven by the pad (text pages, the target turned /
// zoomed by the viewer), counts of lines, flipbook frames and anything the next tick ends or hides
// hold; the screen dimming of the other entities is engine state and holds too:
//   Reticle   - its point half way along the path table; the closing circle's radius half way.
//   Brackets  - following the reticle, then sliding out and growing, half way.
//   Ring      - its growing fan adds whole lines per tick: holds.
//   Scan bars - brightness half way (their 1-pixel scroll per tick holds).
//   Model pair / morph model / spinners / stat bars / backdrop - position, scale, angles (the short
//               way round), fades, the stat bars' growth and pulse, the morph blend half way (the
//               morph model's vertices are blended again at the in-between blend and the exe-data
//               vertex blocks put back after the held frame); a model switch holds the blend.
//   Sprite    - flipbook: holds.
//   Camera    - HeldCamera: while the camera task blends (or the short sequence circles the target)
//               the next tick's eye / look-at are computed on a copy of the camera state and the
//               held frame gets the camera half way; a return / cut holds.
// No Scan task has a draw-only mode.

namespace ff8fx
{
namespace scan040
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t MORPH1 = 0x15CDDF4, MORPH1_SIZE = 0x30 * 8, MORPH2 = 0x15CE1DC, MORPH2_SIZE = 0xA0 * 8;

	union AnyNode
	{
		CtlNode ctl;
		ModelNode model;
		SpriteNode sprite;
	};

	struct Rec
	{
		uint32_t kind;
		AnyNode node;          // as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;
	static Rec g_recs[256];                  // pools: 20 controllers + 20 models + 100 sprites
	static uint8_t g_held_packets[0x10000];  // module arena (0x4000 per tick)
	static uint8_t g_morph_save1[MORPH1_SIZE];
	static uint8_t g_morph_save2[MORPH2_SIZE];
	// camera: the task that moved it on this tick (camera blend or the short sequence's circling)
	static uint32_t g_cam_tick = 0xFFFFFFFF;
	static CtlNode g_cam_node;

	static void held_note_root(const RootNode *)
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = PacketCursor();
	}

	static void held_note(uint32_t kind, const void *node)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return;
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		const size_t size = (kind == REC_PAIR || kind == REC_MORPH || kind == REC_SPINNER || kind == REC_STATBAR || kind == REC_STATIC
			|| kind == REC_BACKDROP || kind == REC_ALTSPINNER) ? sizeof(ModelNode) : kind == REC_SPRITE ? sizeof(SpriteNode) : sizeof(CtlNode);
		memcpy(&r.node, node, size);
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
	}

	static void held_note_camera(const CtlNode *n)
	{
		g_cam_tick = g_real_tick;
		g_cam_node = *n;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// 12-bit angles that turn the short way
	static int16_t Angle12(int16_t a, int16_t b, int num, int den)
	{
		int32_t d = (int32_t)(b - a) & 0xFFF;
		if (d >= 0x800) d -= 0x1000;
		return (int16_t)((a + d * num / den) & 0xFFF);
	}

	// model nodes: position, scale, angles, fade half way to b
	static void LerpModel(ModelNode &m, const ModelNode &a, const ModelNode &b, int num, int den)
	{
		for (int k = 0; k < 3; k++)
		{
			m.h.pos[k] = lerp16(a.h.pos[k], b.h.pos[k], num, den);
			m.scale[k] = lerp_i(a.scale[k], b.scale[k], num, den);
			m.angle[k] = Angle12(a.angle[k], b.angle[k], num, den);
		}
		m.fade = lerp16(a.fade, b.fade, num, den);
	}

	// the node's next update as its task runs it (phase functions on a copy; the task's counter goes
	// up after its draw)
	static bool NextModel(uint32_t kind, const ModelNode &a, ModelNode &b)
	{
		if (a.h.flags & 1) return false; // ends (or keeps only its children)
		b = a;
		b.h.counter++;
		switch (kind)
		{
		case REC_PAIR: PairPhase(&b); break;
		case REC_MORPH: MorphPhase(&b); break;
		case REC_SPINNER: SpinnerPhase(&b); SpinnerTurn(&b); break;
		case REC_STATBAR: if (b.h.phase == 0) return false; StatBarPhase(&b); StatBarPulse(&b); break;
		case REC_BACKDROP: BackdropPhase(&b); break;
		case REC_ALTSPINNER: AltSpinnerPhase(&b); break;
		default: return false;
		}
		return !(b.h.flags & 4) && !(a.h.flags & 4);
	}

	static void ModelHeld(const Rec &r, int num, int den)
	{
		const ModelNode &a = r.node.model;
		ModelNode m = a;
		ModelNode b;
		if (NextModel(r.kind, a, b))
		{
			LerpModel(m, a, b, num, den);
			if (r.kind == REC_PAIR)
			{
				m.fade1 = lerp16(a.fade1, b.fade1, num, den);
				m.fade2 = lerp16(a.fade2, b.fade2, num, den);
			}
			if (r.kind == REC_MORPH && a.model == b.model) m.blend = lerp16(a.blend, b.blend, num, den);
		}
		switch (r.kind)
		{
		case REC_PAIR: PairDraw(&m); break;
		case REC_MORPH:
			MorphVertices((const uint8_t *)m.morph_a, (const uint8_t *)m.morph_b, (uint8_t *)m.model, m.blend);
			DrawHudModel(&m, false);
			break;
		case REC_ALTSPINNER: DrawHudModel(&m, true); break;
		default: DrawHudModel(&m, false); break;
		}
	}

	// the reticle's point at path index `counter` (0x7FFF = it stops)
	static bool ReticleNext(int16_t counter, int16_t *p)
	{
		const int16_t *path = (const int16_t *)RETICLE_PATH + 2 * counter;
		if (path[0] == 0x7FFF) return false;
		p[0] = path[0];
		p[1] = path[1];
		return true;
	}

	static void CtlHeld(const Rec &r, int num, int den)
	{
		const CtlNode &a = r.node.ctl;
		CtlNode m = a;
		int16_t out[2];
		switch (r.kind)
		{
		case REC_BRACKET:
			// sliding out: the next tick moves it by its drift and grows it
			if (!(a.h.flags & 1) && a.h.phase == 2 && (uint32_t)a.h.hdr.func == ORIG_Bracket)
			{
				m.h.pos[0] = lerp16(a.h.pos[0], (int16_t)(a.h.pos[0] + a.dx), num, den);
				m.h.pos[1] = lerp16(a.h.pos[1], (int16_t)(a.h.pos[1] + a.dy), num, den);
				m.size = lerp16(a.size, (int16_t)(a.size + 0x400), num, den);
			}
			BracketLines(&m);
			break;
		case REC_BRACKET_FOLLOW:
		{
			// on the reticle (the next tick: its next point), or the first slide
			int16_t p[2];
			if (a.timer - 1 < 0)
			{
				p[0] = (int16_t)(a.h.pos[0] + a.dx);
				p[1] = (int16_t)(a.h.pos[1] + a.dy);
				m.size = lerp16(a.size, (int16_t)(a.size + 0x400), num, den);
				m.h.pos[0] = lerp16(a.h.pos[0], p[0], num, den);
				m.h.pos[1] = lerp16(a.h.pos[1], p[1], num, den);
			}
			else
			{
				const CtlNode *parent = (const CtlNode *)a.h.parent;
				if (parent->h.phase == 1 && ReticleNext(parent->h.counter, p))
				{
					m.h.pos[0] = lerp16(a.h.pos[0], p[0], num, den);
					m.h.pos[1] = lerp16(a.h.pos[1], p[1], num, den);
				}
			}
			BracketLines(&m);
			break;
		}
		case REC_RING1:
			Fan(&m.h, 0x30, 0x600, m.timer, out);
			break;
		case REC_RING2:
		case REC_RING3:
		{
			Fan(&m.h, 0x30, 0x600, m.timer, out);
			const int16_t p[2] = { (int16_t)(out[0] + 0x18), (int16_t)(out[1] + 0x18) };
			Line(out, p);
			if (r.kind == REC_RING3)
			{
				const int16_t q[2] = { (int16_t)(p[0] + 0x58), p[1] };
				Line(p, q);
			}
			break;
		}
		case REC_RETICLE:
		{
			int16_t radius = 8;
			int16_t p[2];
			if (a.h.phase == 1 && ReticleNext((int16_t)(a.h.counter + 1), p))
			{
				m.h.pos[0] = lerp16(a.h.pos[0], p[0], num, den);
				m.h.pos[1] = lerp16(a.h.pos[1], p[1], num, den);
			}
			else if (a.h.phase == 2) radius = lerp16(8, 6, num, den); // the next tick: radius 8 - 2
			Fan(&m.h, radius, 0, 0x20, out);
			CornerLines(&m.h);
			break;
		}
		case REC_RETICLE_CLOSE:
		{
			int16_t radius = a.timer;
			if (a.timer - 2 > 0) radius = lerp16(a.timer, (int16_t)(a.timer - 2), num, den);
			Fan(&m.h, radius, 0, 0x20, out);
			CornerLines(&m.h);
			break;
		}
		case REC_BARS:
		{
			int16_t f = a.fade;
			if (!(a.h.flags & 1))
			{
				if (a.h.phase == 1) f = (int16_t)(a.fade + 8 >= 0x80 ? 0x80 : a.fade + 8);
				else if (a.h.phase == 3) f = (int16_t)(a.fade - 8 <= 0 ? 0 : a.fade - 8);
			}
			ScanBars(a.h.counter, (uint8_t)lerp16(a.fade, f, num, den));
			break;
		}
		default: break;
		}
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		memcpy(g_morph_save1, (const void *)MORPH1, sizeof(g_morph_save1));
		memcpy(g_morph_save2, (const void *)MORPH2, sizeof(g_morph_save2));
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Replay(r);
			switch (r.kind)
			{
			case REC_PAIR: case REC_MORPH: case REC_SPINNER: case REC_STATBAR: case REC_STATIC: case REC_BACKDROP: case REC_ALTSPINNER:
				ModelHeld(r, num, den);
				break;
			case REC_SPRITE: DrawHudSprite(&r.node.sprite, 0x400); break;
			default: CtlHeld(r, num, den); break;
			}
		}
		memcpy((void *)MORPH1, g_morph_save1, sizeof(g_morph_save1));
		memcpy((void *)MORPH2, g_morph_save2, sizeof(g_morph_save2));
	}

	// the camera of the next tick on a copy of the camera state (pure math), then half way
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (g_cam_tick != g_real_tick) return false;
		const CtlNode &n = g_cam_node;
		CamState c = Cam();
		// the matrix helpers work on the Field_Alloc scratch: put it back afterwards
		static uint8_t scratch_save[SCRATCH];
		uint8_t *scratch = var<uint8_t *>(0x1D999C4);
		memcpy(scratch_save, scratch, SCRATCH);
		if ((uint32_t)n.h.hdr.func == ORIG_AltScanner)
		{
			if (n.h.phase != 3) return false; // the text was closed: the camera stops
			const uint8_t *info = (const uint8_t *)(INFO_TABLE + (int32_t)EntityClass((int16_t)(uint16_t)n.h.slot) * 16);
			if (info[0xE] == 1) c.yaw = (int16_t)((c.yaw + 0x40) & 0xFFF);
			if (F<int16_t>(info, 4) < (int16_t)0xFE80)
			{
				c.h = (int16_t)(c.h - c.h1 / 32);
				if (c.h > 0) c.h = 0;
			}
		}
		else
		{
			if (n.h.flags & 1) return false; // arrived
			CameraBlend(c);
		}
		CameraEye(c);
		memcpy(scratch, scratch_save, SCRATCH);
		for (int k = 0; k < 3; k++)
		{
			world[k] = lerp16(var<int16_t>(0xB8B7F0 + 2 * k), c.eye[k], num, den);
			lookat[k] = lerp16(var<int16_t>(0xB8B7F8 + 2 * k), c.look[k], num, den);
		}
		return true;
	}
}

	static void register_mag040_held()
	{
		using namespace scan040;
		mark_held(ORIG_Reticle);
		mark_held(ORIG_Pair);
		mark_held(ORIG_Morph);
		mark_held(ORIG_Corner);
		mark_held(ORIG_Bracket);
		mark_held(ORIG_Ring);
		mark_held(ORIG_Sprite);
		mark_held(ORIG_Bars);
		mark_held(ORIG_Spinner);
		mark_held(ORIG_Board);
		mark_held(ORIG_StatBar);
		mark_held(ORIG_Backdrop);
		mark_held(ORIG_AltSpinner);
		mark_held(ORIG_AltBoard);
		register_module_held(40, HeldReady, HeldFrame);
		register_module_camera(40, HeldCamera);
	}
}
