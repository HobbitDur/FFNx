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

// 30 fps part of mag117_break.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag117_break.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every Break task draws THEN updates. A real tick records every draw in order with the GTE
// registers and the Field_Alloc scratch it started from (draw headers, the stone draw's work block
// and the cut model's clip lists keep the scratch bytes where the code does not write). A held
// frame replays the log in the same order:
//   Shell  - the prim-model layout through prim::play_held: every object's exact in-between
//            record (position, rotation, scale, colour, fade, vertex-frame blend); the cut height
//            follows the in-between offset; the "before frame 50" test uses the frame the real
//            tick drew (play_held hands over the layout already stepped). Blended vertex frames
//            and the projected vertices go to private buffers.
//   Stone target - the target at its in-between pose (pose_midpoint), the stone level half way to
//            the next tick's (a counter ramp: more polygons turn to stone in between), in the
//            colour it was drawn in; its shadow is drawn again. Vertices go to a private copy of
//            the bone workspace, packets to a private copy of the frame arena.
//   Debris - position (gravity, bounce on the ground) and rotation half way to the next tick's.
//   Spark  - position half way to the next tick's (it slows down); the sequence frame steps at
//            15 Hz.
// The last draw of a task (it ends after its draw) holds. No Break task has a draw-only mode.

namespace ff8fx
{
namespace break117
{
	static const uint32_t SCRATCH = 0x800;
	enum : uint32_t { REC_NODE = 1, REC_PLAY = 2, REC_STONE = 3 };

	struct Rec
	{
		uint32_t kind;
		uint32_t fn;              // node: its task
		uint8_t node[0x30];       // node: as drawn
		uint8_t *entity;          // stone: the target
		uint32_t step;            // stone: counter - 24
		uint32_t colour;          // stone: the target's colour
		prim::Layout *layout;     // play
		int32_t frame;            // play: the layout frame it drew
		PrimArg arg;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};

	// the held callback's block: the task's block, then what the held draw needs
	struct HeldArg
	{
		PrimArg arg;
		int32_t frame;
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0;       // module packet cursor at the tick's first draw
	static uint32_t g_frame_packets = 0; // frame arena cursor at the first stone draw
	static Rec g_recs[160];
	static uint8_t g_held_packets[0x30000]; // module arena (0x18000 per tick)
	static uint8_t g_held_frame[0x40000];   // shadow + stone target packets
	static uint8_t g_held_verts[0x10000];   // stone target vertices (indices are 12-bit: 0x8000)
	// blended vertex frames (magic buffer + 0x4AF0) and the cut model's projected vertices right
	// after them (+ 0x4CF0), contiguous as in the magic buffer
	static uint8_t g_held_work[0x2000];
	static uint8_t *const g_held_morph = g_held_work;
	static uint8_t *const g_held_proj = g_held_work + (BUF_Proj - BUF_Morph);

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_frame_packets = 0;
	}

	static Rec *NewRec(uint32_t kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_node(uint32_t fn, const void *node)
	{
		Rec *r = NewRec(REC_NODE);
		if (!r) return;
		r->fn = fn;
		memcpy(r->node, node, fn == ORIG_DebrisTask ? sizeof(DebrisNode) : sizeof(SparkNode));
	}

	static void held_note_play(const TargetNode *t, const PrimArg *arg)
	{
		Rec *r = NewRec(REC_PLAY);
		if (!r) return;
		r->layout = (prim::Layout *)t->layout;
		r->frame = r->layout->frame;
		r->arg = *arg;
	}

	static void held_note_stone(const TargetNode *t, uint32_t step)
	{
		Rec *r = NewRec(REC_STONE);
		if (!r) return;
		if (!g_frame_packets) g_frame_packets = FrameCursor();
		r->entity = t->entity;
		r->step = step;
		r->colour = *(const uint32_t *)(t->entity + 0x28);
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a replayed draw starts from the GTE state and scratch of the real one
	static void Replay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	// the held callback: the object's draw at the frame the real tick drew, into the private buffers
	static void __cdecl PartHeld(prim::Layout *l, prim::Record *r, int arg_)
	{
		const HeldArg *a = (const HeldArg *)arg_;
		PartDraw(l, r, &a->arg, a->frame, g_held_proj);
	}

	// a held frame's other outputs: frame arena packets (a copy of the real tick's, so bytes the
	// draw leaves alone are the real ones) and the stone target's vertex buffer; put back
	struct FrameScope
	{
		uint32_t cursor, bones;
		FrameScope()
		{
			bones = BoneWorkspace();
			cursor = FrameCursor();
			if (g_frame_packets && cursor > g_frame_packets)
			{
				uint32_t used = cursor - g_frame_packets;
				memcpy(g_held_frame, (const void *)g_frame_packets, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
			}
			FrameCursor() = (uint32_t)g_held_frame;
			BoneWorkspace() = (uint32_t)g_held_verts;
		}
		~FrameScope()
		{
			BoneWorkspace() = bones;
			FrameCursor() = cursor;
		}
	};

	static void StoneHeld(const Rec &r, int num, int den)
	{
		uint8_t *entity = r.entity;
		// the colour it was drawn in
		uint32_t &colour = *(uint32_t *)(entity + 0x28);
		const uint32_t now = colour;
		colour = r.colour;
		int32_t level = (int32_t)StoneLevel(r.step);
		if (r.step + 1 < 0x3C) level = lerp_i(level, (int32_t)StoneLevel(r.step + 1), num, den);
		pose_midpoint(entity + 0x60, entity + 0x6C, num, den);
		const uint32_t unused[2] = { 0, 0x01000100 };
		DrawStoneTarget(entity, level, unused, STONE_TPage, STONE_Clut);
		colour = now;
	}

	static void NodeHeld(const Rec &r, int num, int den)
	{
		switch (r.fn)
		{
		case ORIG_DebrisTask:
		{
			const DebrisNode &a = *(const DebrisNode *)r.node;
			DebrisNode m = a;
			if (a.counter + 1 < 0x1E)
			{
				DebrisNode b = a;
				DebrisMove(&b);
				for (int c = 0; c < 3; c++)
				{
					m.pos[c] = lerp16(a.pos[c], b.pos[c], num, den);
					m.rot[c] = lerp16(a.rot[c], b.rot[c], num, den);
				}
			}
			DebrisDraw(&m);
			break;
		}
		case ORIG_SparkTask:
		{
			const SparkNode &a = *(const SparkNode *)r.node;
			SparkNode m = a;
			if (a.frame + 1 < 0xE)
			{
				SparkNode b = a;
				SparkMove(&b);
				for (int c = 0; c < 3; c++) m.pos[c] = lerp16(a.pos[c], b.pos[c], num, den);
			}
			SparkDraw(&m);
			break;
		}
		}
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		FrameScope frame;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			Replay(r);
			switch (r.kind)
			{
			case REC_NODE: NodeHeld(r, num, den); break;
			case REC_STONE: StoneHeld(r, num, den); break;
			case REC_PLAY:
			{
				HeldArg a;
				a.arg = r.arg;
				a.frame = r.frame;
				memcpy(g_held_work, (const void *)(TexBase() + BUF_Morph), BUF_Dither - BUF_Morph);
				a.arg.morph = (uint32_t)g_held_morph;
				prim::play_held(r.layout, PartHeld, (int)&a, num, den);
				break;
			}
			}
		}
	}
}

	static void register_mag117_held()
	{
		mark_held(break117::ORIG_TargetTask);
		mark_held(break117::ORIG_DebrisTask);
		mark_held(break117::ORIG_SparkTask);
		register_module_held(117, break117::HeldReady, break117::HeldFrame);
	}
}
