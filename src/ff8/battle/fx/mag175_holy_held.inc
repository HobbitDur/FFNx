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

// 30 fps part of mag175_holy.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag175_holy.cpp).
//
// Every Holy task draws THEN updates, so the state it will draw on the next tick is its live
// state after the real tick. A real tick records every draw in order: the task node (or pool
// entry) as drawn, the GTE registers and the Field_Alloc scratch the draw started with (fields a
// draw does not set keep what the scratch held). A held frame replays the log in the same order
// from the same GTE state and scratch, each part at its exact in-between state: fields that move
// are lerped from the recorded state to the live one, frame-driven values (fades, brightness,
// texture scroll, the beam's sweep) are computed at frame + num / den (held_ramp). A part moves
// only when its task advanced on the real tick (node still in use, counter + 1): the last frame
// of a task and every draw-only tick hold.
//   Shatter  - frames 0..9: the stage capture is replayed (the stage groups' bone matrices are
//              rebuilt for it and put back afterwards, as RenderGeometry's 0x1D99C18); grid:
//              triangles in place hold; moving ones at the displacement half way from the one
//              drawn to the live one, scale half way to the next shrink step (min 0x180);
//              brightness at the fractional frame. The packets go to a private copy of the
//              triangle packets (the real ones stay as the real tick left them), the triangles'
//              state is not touched.
//   Orbit controllers - their matrices are rebuilt from the angles half way to the live ones
//              (private matrices, the module's 0x23745B0 matrices stay).
//   Orbiting sparks - angle and radius half way, on the in-between controller matrix;
//              the flipbook frame holds.
//   Sparkles - radius / height / angle half way; flipbook frame holds.
//   Motes    - height half way, brightness at the fractional age; flipbook frame holds.
//   Ring / pillars / disks - spin angle half way (disks: size half way); fades at the
//              fractional frame. Expanding rings: scale half way, fade. Flash: size half way,
//              fade. Waves: texture scroll and fade at the fractional frame, texture holds.
//   Beam     - brightness and sweep direction at the fractional frame, facing the current camera.
// Not drawn by the module (hold): the entities' fade colours (entity state the engine's model draw
// reads), the director's screen flash level (stage group state).
// The held frame draws into private packet buffers (module arena and frame arena) and puts back
// everything the draws write: sprite matrix 0x23941E8, draw offset 0x23941B8, the grid's projected
// positions, the shadow matrix scratch, the Field_Alloc scratch, the GTE.

namespace ff8fx
{
namespace holy175
{
	static const uint32_t GTE_DATA = 0x1CA8A10, GTE_DATA_SIZE = 0x70, GTE_CTRL = 0x1CA9230, GTE_CTRL_SIZE = 0xD0;
	static const uint32_t NODE_MAX = 0x24, SCRATCH = 0x300;

	struct Rec
	{
		uint32_t fn;
		const void *live;           // the node / pool entry (its state after the real tick)
		uint8_t node[NODE_MAX];     // as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];   // Field_Alloc scratch from the task's first allocation
	};

	// real tick on which the ported root task last ran, and its draw log
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0, g_arena = 0; // module / frame packet cursors at the tick's first draw
	static Rec g_recs[512];
	static Triangle g_tri_drawn[GRID_TriangleCount]; // triangles before the shatter draw
	static bool g_have_shatter = false;
	static uint8_t g_held_packets[0x10000];
	static uint8_t g_held_arena[0x40000];
	static uint8_t g_held_grid[GRID_TriangleCount * 0x40];
	static uint8_t g_env_copy[0x18];
	// held frame being drawn: frame + num / den; ramps only for parts that advance
	static bool g_in_held = false, g_ramp = false;
	static int g_hnum = 0, g_hden = 1;
	// in-between orbit controller matrices of the held frame
	static Mat4x3 g_orbit_m[2];
	static bool g_orbit_ok[2];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_arena = 0;
		g_have_shatter = false;
	}

	static Rec *held_add(uint32_t fn, const void *live, uint32_t size, const void *scratch)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0)
		{
			g_packets = PacketCursor();
			g_arena = FrameArena();
		}
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		r.live = live;
		memset(r.node, 0, sizeof(r.node));
		memcpy(r.node, live, size < NODE_MAX ? size : NODE_MAX);
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, scratch, SCRATCH);
		return &r;
	}

	static void held_note_draw(uint32_t fn, const void *node, uint32_t size) { held_add(fn, node, size, var<uint8_t *>(0x1D999C4)); }
	static void held_note_item(uint32_t fn, const void *entry, const void *scratch) { held_add(fn, entry, 0x18, scratch); }

	static void held_note_shatter(const void *node)
	{
		if (!held_add(ORIG_ShatterTask, node, sizeof(ShatterNode), var<uint8_t *>(0x1D999C4))) return;
		memcpy(g_tri_drawn, (const void *)GRID_Triangles, sizeof(g_tri_drawn));
		g_have_shatter = true;
	}

	// the capture's draw environment restore packet: a private copy on held frames (inserting
	// links the packet into the OT, the static one in the exe's data stays as the real tick left it)
	static uint8_t *held_env_packet(uint8_t *env)
	{
		if (!g_in_held) return env;
		memcpy(g_env_copy, env, sizeof(g_env_copy));
		return g_env_copy;
	}

	// a frame-driven value v(frame) = v + slope per frame, at frame + num / den
	static int32_t held_ramp(int32_t value, int32_t slope)
	{
		if (!g_in_held || !g_ramp) return value;
		return value + slope * g_hnum / g_hden;
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// the task advanced on the real tick: its node is still in use, same task, counter + 1
	static bool NodeAdvanced(const Rec &r)
	{
		const TaskNode *live = (const TaskNode *)r.live;
		const TaskNode *was = (const TaskNode *)r.node;
		return (live->flags & 1) && live->func == was->func && *(const int16_t *)((const uint8_t *)live + 0xC) == (int16_t)(*(const int16_t *)(r.node + 0xC) + 1);
	}

	// ---- the stage capture of the held frame: bone matrices rebuilt for it, then put back (and
	// the packet pointer RenderGeometry leaves in 0x1D99C18)
	static uint8_t g_bones_save[4][0x40 + 0x30 * 256];
	static void HeldStageCapture()
	{
		uint8_t *sk[4] = {};
		uint32_t size[4] = {};
		for (int k = 0; k < 4; k++)
		{
			uint8_t *anim = (uint8_t *)(STAGE_AnimHeaders + 0x2C * k);
			uint8_t **pp = *(uint8_t ***)(anim + 4);
			if (!pp || !*pp) continue;
			sk[k] = *pp;
			size[k] = 0x40 + 0x30 * (uint32_t)sk[k][0];
			memcpy(g_bones_save[k], sk[k], size[k]);
		}
		uint32_t last_packet = var<uint32_t>(0x1D99C18); // RenderGeometry: last packet it wrote
		StageCapture();
		var<uint32_t>(0x1D99C18) = last_packet;
		for (int k = 0; k < 4; k++)
			if (sk[k]) memcpy(sk[k], g_bones_save[k], size[k]);
	}

	// ---- the grid of the held frame (MAG_175_sub_5D2BC0 on the recorded / live triangles)
	static void HeldGridDraw(int32_t counter, bool advanced, int num, int den)
	{
		int32_t level = counter * 12 + 0x80;
		if (advanced) level += 12 * num / den;
		if (level > 0xFF) level = 0xFF;
		uint32_t colour = (uint32_t)level | ((uint32_t)level << 8) | ((uint32_t)level << 16);
		memcpy(g_held_grid, (const void *)GRID_Packets, sizeof(g_held_grid));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		ProjectGrid(h);
		*(int32_t *)(h + 0x30) = ProjH();
		int32_t buf = DisplayBuffer();
		const Triangle *live = (const Triangle *)GRID_Triangles;
		uint8_t *pk = g_held_grid;
		for (int i = 0; i < GRID_TriangleCount; i++, pk += 0x40)
		{
			const Triangle &a = g_tri_drawn[i];
			const Triangle &b = live[i];
			if (a.delay < 0) continue;
			if (a.delay >= counter)
			{
				DrawStaticTriangle(&a, pk + buf * 0x20, colour);
				continue;
			}
			int32_t x = a.offx + a.cx;
			int32_t y = a.offy + a.cy;
			*(int32_t *)(h + 0x78) = x;
			*(int32_t *)(h + 0x7C) = y;
			if (x > 0xA8 || y < -0x74) continue;
			// drawn at the displacement before the update and the scale after it
			Triangle m = b;
			int16_t next = b.scale;
			if (advanced)
			{
				next = (int16_t)(b.scale - b.dscale);
				if (next < 0x180) next = 0x180;
			}
			m.scale = lerp16(b.scale, next, num, den);
			int16_t ox = lerp16(a.offx, b.offx, num, den);
			int16_t oy = lerp16(a.offy, b.offy, num, den);
			*(int32_t *)(h + 0x78) = ox + a.cx + 8;
			*(int32_t *)(h + 0x80) = *(int32_t *)(h + 0x30);
			*(int32_t *)(h + 0x7C) = oy + a.cy + 4;
			ProjectTriangle(h, &m);
			if (GridTextured()) EmitMovingTriangle(&b, pk + buf * 0x20, colour);
			else if (b.scale == 0x180)
			{
				FlatTriangle((uint8_t *)PacketCursor());
				PacketCursor() += 0x20;
			}
		}
		FieldFree(0xB4);
	}

	// A held frame's surroundings: the packet cursors go to private buffers (holding copies of
	// what the real tick wrote there, so bytes a draw does not write are the real tick's), and
	// everything the draws write outside them is put back afterwards
	struct HeldScope
	{
		uint32_t cursor, arena, arena_end;
		uint8_t *scratch;
		uint8_t scratch_save[0x1000];
		uint8_t shadow_save[0x20];
		uint8_t sprite_save[0x20];
		uint8_t offset_save[4];
		uint8_t gte_save[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		bool grid;
		uint8_t vertices_save[GRID_VerticesEnd - GRID_Vertices];
		uint8_t centres_save[GRID_CentresEnd - GRID_Centres];

		static void copy_real(uint8_t *dst, uint32_t size, uint32_t from, uint32_t to)
		{
			if (from && to > from) memcpy(dst, (const void *)from, to - from < size ? to - from : size);
		}

		HeldScope(bool with_grid) : grid(with_grid)
		{
			cursor = PacketCursor();
			arena = FrameArena();
			arena_end = var<uint32_t>(0x1D969A8);
			copy_real(g_held_packets, sizeof(g_held_packets), g_packets, cursor);
			copy_real(g_held_arena, sizeof(g_held_arena), g_arena, arena);
			PacketCursor() = (uint32_t)g_held_packets;
			FrameArena() = (uint32_t)g_held_arena;
			var<uint32_t>(0x1D969A8) = (uint32_t)g_held_arena + sizeof(g_held_arena);
			scratch = var<uint8_t *>(0x1D999C4);
			if (scratch) memcpy(scratch_save, scratch, sizeof(scratch_save));
			memcpy(shadow_save, (const void *)0x21DFED0, sizeof(shadow_save));
			memcpy(sprite_save, SpriteMatrix(), sizeof(sprite_save));
			memcpy(offset_save, (const void *)0x23941B8, sizeof(offset_save));
			memcpy(gte_save, (const void *)GTE_DATA, GTE_DATA_SIZE);
			memcpy(gte_save + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
			if (grid)
			{
				memcpy(vertices_save, (const void *)GRID_Vertices, sizeof(vertices_save));
				memcpy(centres_save, (const void *)GRID_Centres, sizeof(centres_save));
			}
		}
		~HeldScope()
		{
			if (grid)
			{
				memcpy((void *)GRID_Vertices, vertices_save, sizeof(vertices_save));
				memcpy((void *)GRID_Centres, centres_save, sizeof(centres_save));
			}
			memcpy((void *)GTE_DATA, gte_save, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, gte_save + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy((void *)0x23941B8, offset_save, sizeof(offset_save));
			memcpy(SpriteMatrix(), sprite_save, sizeof(sprite_save));
			memcpy((void *)0x21DFED0, shadow_save, sizeof(shadow_save));
			if (scratch) memcpy(scratch, scratch_save, sizeof(scratch_save));
			var<uint32_t>(0x1D969A8) = arena_end;
			FrameArena() = arena;
			PacketCursor() = cursor;
		}
	};

	static void BeginReplay(const Rec &r)
	{
		memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
		memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
	}

	static void HeldFrame(int num, int den)
	{
		// a held frame larger than the private buffers is not drawn (never seen: the stage capture
		// and the motes use under 16 KB of the frame arena, the module arena is 48 KB)
		if (FrameArena() - g_arena > sizeof(g_held_arena) / 2 || PacketCursor() - g_packets > sizeof(g_held_packets) / 2) return;
		HeldScope scope(g_have_shatter);
		g_in_held = true;
		g_hnum = num;
		g_hden = den;
		g_orbit_ok[0] = g_orbit_ok[1] = false;
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			BeginReplay(r);
			switch (r.fn)
			{
			case ORIG_ShatterTask:
			{
				const ShatterNode *p = (const ShatterNode *)r.node;
				bool advanced = NodeAdvanced(r);
				g_ramp = false;
				if (p->counter < 10) HeldStageCapture();
				HeldGridDraw(p->counter, advanced, num, den);
				break;
			}
			case ORIG_OrbitCtrlTask:
			{
				OrbitCtrlNode m;
				memcpy(&m, r.node, sizeof(m));
				if (NodeAdvanced(r))
				{
					const OrbitCtrlNode *b = (const OrbitCtrlNode *)r.live;
					m.ax = lerp16(m.ax, b->ax, num, den);
					m.az = lerp16(m.az, b->az, num, den);
				}
				if (m.index >= 0 && m.index < 2)
				{
					g_orbit_m[m.index] = Mat4x3{};
					OrbitCtrlMatrix(&m, &g_orbit_m[m.index]);
					g_orbit_ok[m.index] = true;
				}
				break;
			}
			case ORIG_OrbitTask:
			{
				OrbitNode m;
				memcpy(&m, r.node, sizeof(m));
				if (NodeAdvanced(r))
				{
					const OrbitNode *b = (const OrbitNode *)r.live;
					m.angle = lerp16(m.angle, b->angle, num, den);
					m.radius = lerp16(m.radius, b->radius, num, den);
				}
				bool own = m.index >= 0 && m.index < 2 && g_orbit_ok[m.index];
				OrbitDraw(&m, own ? &g_orbit_m[m.index] : OrbitMatrix(m.index));
				break;
			}
			case ORIG_SparkleTask:
			{
				Sparkle m;
				memcpy(&m, r.node, sizeof(m));
				const Sparkle *b = (const Sparkle *)r.live;
				if ((b->active & 1) && b->frame == (uint16_t)(m.frame + 1))
				{
					m.radius = lerp16(m.radius, b->radius, num, den);
					m.y = lerp16(m.y, b->y, num, den);
					m.angle = lerp16(m.angle, b->angle, num, den);
				}
				uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
				int16_t *s = (int16_t *)FieldAlloc(0x10);
				SparkleDraw(h, s, &m);
				FieldFree(0x10);
				FieldFree(0xB4);
				break;
			}
			case ORIG_MoteTask:
			{
				Mote m;
				memcpy(&m, r.node, sizeof(m));
				const Mote *b = (const Mote *)r.live;
				g_ramp = (b->active & 1) && b->age == (int16_t)(m.age + 1);
				if (g_ramp) m.y = lerp16(m.y, b->y, num, den);
				uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
				uint8_t *mm = (uint8_t *)FieldAlloc(0x48);
				MoteDraw(h, mm, &m);
				FieldFree(0x48);
				FieldFree(0xB4);
				break;
			}
			case ORIG_RingTask:
			case ORIG_WaveTask:
			case ORIG_ExpandTask:
			case ORIG_PillarTask:
			case ORIG_BeamTask:
			case ORIG_DiskTask:
			case ORIG_FlashTask:
			{
				RingNode m;
				memcpy(&m, r.node, sizeof(m));
				g_ramp = NodeAdvanced(r);
				if (g_ramp)
				{
					const RingNode *b = (const RingNode *)r.live;
					m.angle = lerp16(m.angle, b->angle, num, den);
					m.scale = lerp16(m.scale, b->scale, num, den);
				}
				switch (r.fn)
				{
				case ORIG_RingTask: RingDraw(&m); break;
				case ORIG_WaveTask: WaveDraw(&m); break;
				case ORIG_ExpandTask: ExpandDraw(&m); break;
				case ORIG_PillarTask: PillarDraw(&m); break;
				case ORIG_BeamTask: BeamDraw(&m); break;
				case ORIG_DiskTask: DiskDraw(&m); break;
				case ORIG_FlashTask: FlashDraw(&m); break;
				}
				break;
			}
			}
			g_ramp = false;
		}
		g_in_held = false;
		g_hnum = 0;
		g_hden = 1;
	}
}

	static void register_mag175_held()
	{
		mark_held(holy175::ORIG_ShatterTask);
		mark_held(holy175::ORIG_SparkleTask);
		mark_held(holy175::ORIG_OrbitTask);
		mark_held(holy175::ORIG_MoteTask);
		mark_held(holy175::ORIG_RingTask);
		mark_held(holy175::ORIG_WaveTask);
		mark_held(holy175::ORIG_ExpandTask);
		mark_held(holy175::ORIG_PillarTask);
		mark_held(holy175::ORIG_BeamTask);
		mark_held(holy175::ORIG_DiskTask);
		mark_held(holy175::ORIG_FlashTask);
		register_module_held(175, holy175::HeldReady, holy175::HeldFrame);
	}
}
