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

// 30 fps part of mag154_ultra_waves.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag154_ultra_waves.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Both drawing tasks draw THEN update. A real tick records every draw in order: the node as
// drawn, the GTE registers the draw started with and the Field_Alloc scratch under its headers
// (fields a draw does not set keep what the scratch held); the Emitter also keeps a copy of the
// particle pool as it drew it. A held frame replays the log in the same order from the same GTE
// state and scratch, each part at its in-between state (fraction num / den; nothing random is
// drawn, nothing is written back):
//   Emitter    - every particle of its mask, in pool order, half way between the pool as drawn and
//                the pool as the tick left it (= what the next tick draws): position, spin angle,
//                size, radius, colour channels (colour cycle) and the effective depth-cue fade (the
//                loop keeps the last life-6+ particle's fade for the following particles: both loops
//                are simulated, the cue is on in between when either loop has it on; the cued colour
//                is interpolated, the colour sent to the cue is solved from it).
//                A particle freed by the tick (life 12) holds its last state below fraction 1; one
//                spawned by the tick appears at fraction 1 only (exactly the next tick's draw).
//   ScreenWarp - the stage capture is replayed as drawn (private copies of its packets); the grid
//                depth of every vertex half way to the next tick's wave (counter + 1), the quad
//                colour channels half way to the next entry; its last draw (counter 34) holds
//                below fraction 1 and is not drawn at 1 (the next tick draws no warp). The warp the
//                next tick spawns (Controller counter 8) is drawn at fraction 1 only, at counter 0.
// Draw-only ticks (battle_to_update_flags & 0x201) do not move: everything holds.
// The held frame draws into private packet buffers (module arena, frame arena, grid quads,
// capture packets) and puts back what the draws write outside them: the grid (depth and screen
// positions, rewritten in place for the projection), the stage vertex workspace (0x1D98B3C), the
// stage groups' bone matrices, RenderGeometry's 0x1D99C18, the Field_Alloc scratch, the shadow
// matrix scratch and the GTE.

namespace ff8fx
{
namespace ultra154
{
	static const uint32_t SCRATCH = 0x200;   // Field_Alloc scratch recorded per draw
	static const int MAX_RECS = 16;

	struct Rec
	{
		uint32_t fn;                           // task function (kind)
		uint8_t node[0x24];                    // node as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];              // Field_Alloc scratch under the draw's headers
		Particle parts[PART_COUNT];            // Emitter: the particle pool as drawn
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0, g_arena = 0; // module / frame arena cursors at the tick's first draw
	static const ControllerNode *g_controller = nullptr; // the Controller, when it ran on the tick
	static Rec g_recs[MAX_RECS];

	static uint8_t g_held_packets[0xC000];     // one module packet arena
	static uint8_t g_held_arena[0x40000];
	static uint8_t g_quads[QUADS_BYTES];
	static uint8_t g_grid_save[GRID_BYTES];
	static uint8_t g_cap_packets[0x30], g_cap_xy[4];
	static uint8_t g_workspace_save[0x10000];
	static uint8_t g_bones_save[4][0x40 + 0x30 * 256];

	static void SaveGte(uint8_t *g)
	{
		memcpy(g, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(g + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
	}

	static void LoadGte(const uint8_t *g)
	{
		memcpy((void *)GTE_DATA, g, GTE_DATA_SIZE);
		memcpy((void *)GTE_CTRL, g + GTE_DATA_SIZE, GTE_CTRL_SIZE);
	}

	static void *FieldScratch() { return var<void *>(0x1D999C4); }

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_arena = 0;
		g_controller = nullptr;
	}

	static void held_note_controller(const TaskNode *node) { g_controller = (const ControllerNode *)node; }

	static Rec *held_add(uint32_t fn, const void *node)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0) g_packets = PacketCursor();
		Rec &r = g_recs[g_nrec++];
		r.fn = fn;
		memcpy(r.node, node, sizeof(r.node));
		SaveGte(r.gte);
		memcpy(r.scratch, FieldScratch(), SCRATCH);
		return &r;
	}

	static void held_note_emitter(const EmitterNode *node)
	{
		Rec *r = held_add(ORIG_Emitter, node);
		if (r) memcpy(r->parts, Parts(), sizeof(r->parts));
	}

	static void held_note_warp(const WarpNode *node)
	{
		if (held_add(ORIG_ScreenWarp, node) && !g_arena) g_arena = FrameArena();
	}

	// the Controller spawns the ScreenWarp on the next tick (counter 8), which draws it at once
	static bool WarpBornNext() { return g_controller && g_controller->counter == 8; }

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// colour channels (bytes 0..2) half way, the code byte of a
	static uint32_t LerpColour(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t c = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			c |= (uint32_t)(lerp_i((int32_t)((a >> k) & 0xFF), (int32_t)((b >> k) & 0xFF), num, den) & 0xFF) << k;
		return c;
	}

	// The loop's depth cue persists: once a particle of life 6+ is drawn, the header keeps flags 0xC0
	// and that particle's fade for the following particles of the loop (0x608530). Effective fade of
	// each slot in one loop (0 = no depth cue: DPCS at level 0 leaves the colour as it is).
	struct LoopCue { bool on[PART_COUNT]; int32_t fade[PART_COUNT]; };
	static void EmitterCue(const Particle *p, uint16_t mask, LoopCue &q)
	{
		bool on = false;
		int32_t fade = 0;
		for (int i = 0; i < PART_COUNT; i++)
		{
			if ((mask & p[i].mask) && p[i].life >= 6)
			{
				on = true;
				fade = ParticleFade(p[i].life);
			}
			q.on[i] = on;
			q.fade[i] = on ? fade : 0;
		}
	}

	// the colour that the depth cue (DPCS toward black at level `fade`) turns into the channels half
	// way between the two drawn (cued) colours: the cued output is interpolated, not colour and fade
	// separately (their product bulges beyond both ends)
	static uint32_t CueColour(uint32_t ca, int32_t fa, uint32_t cb, int32_t fb, int32_t fade, int num, int den)
	{
		uint32_t c = ca & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
		{
			const int32_t oa = (int32_t)((ca >> k) & 0xFF) * (0x1000 - fa) / 0x1000;
			const int32_t ob = (int32_t)((cb >> k) & 0xFF) * (0x1000 - fb) / 0x1000;
			int32_t v = lerp_i(oa, ob, num, den) * 0x1000 / (0x1000 - fade);
			if (v > 0xFF) v = 0xFF;
			c |= (uint32_t)v << k;
		}
		return c;
	}

	// ---- Emitter: a = the pool as drawn, b = the pool the tick left (the next tick's draw)
	static void HeldEmitter(const Rec &r, int num, int den)
	{
		const EmitterNode *e = (const EmitterNode *)r.node;
		const Particle *a = r.parts;
		const Particle *b = Parts();
		static LoopCue qa, qb;
		EmitterCue(a, e->mask, qa);
		EmitterCue(b, e->mask, qb);
		RenderHeader *h = (RenderHeader *)FieldAlloc(0x6C);
		EmitterWork *w = (EmitterWork *)FieldAlloc(0x88);
		EmitterBegin(h, w, e);
		const bool end = num >= den;
		for (int i = 0; i < PART_COUNT; i++)
		{
			const bool mine_a = (e->mask & a[i].mask) != 0, mine_b = (e->mask & b[i].mask) != 0;
			// depth cue of this draw: the drawn loop's at 0, the next loop's at 1, in between on when
			// either is (both are monotonic along the loop), at the effective fades half way
			const bool cue = num == 0 ? qa.on[i] : end ? qb.on[i] : (qa.on[i] || qb.on[i]);
			// the same particle, one update (or none: draw-only tick) later
			const bool same = mine_a && mine_b && (b[i].life == a[i].life || b[i].life == (int16_t)(a[i].life + 1));
			Particle d = end ? b[i] : a[i];
			d.life = cue ? 6 : 0; // ParticleDraw sets the header's fade / flags 0xC0 from life 6
			if (same)
			{
				for (int k = 0; k < 3; k++) d.pos[k] = lerp16(a[i].pos[k], b[i].pos[k], num, den);
				d.angle = lerp16(a[i].angle, b[i].angle, num, den);
				d.scale = lerp16(a[i].scale, b[i].scale, num, den);
				d.radius = lerp16(a[i].radius, b[i].radius, num, den);
				const int32_t fade = lerp_i(qa.fade[i], qb.fade[i], num, den);
				uint32_t colour = LerpColour(RingColours[a[i].colour], RingColours[b[i].colour], num, den);
				if (cue && num > 0 && !end) colour = CueColour(RingColours[a[i].colour], qa.fade[i], RingColours[b[i].colour], qb.fade[i], fade, num, den);
				ParticleDraw(h, w, d, colour, fade);
				continue;
			}
			// freed by the tick: holds; spawned by the tick: from the next tick on
			if (mine_a && !end) ParticleDraw(h, w, d, RingColours[a[i].colour], qa.fade[i]);
			if (mine_b && end) ParticleDraw(h, w, d, RingColours[b[i].colour], qb.fade[i]);
		}
		FieldFree(0x88);
		FieldFree(0x6C);
	}

	// ---- ScreenWarp: the grid depth half way to counter + 1
	static void HeldWarp(int32_t c, bool moving, int num, int den)
	{
		const bool next = moving && c + 1 < WARP_TICKS;
		if (moving && !next && num >= den) return; // the next tick draws no warp
		memcpy(g_cap_packets, (const void *)PKT_Capture, sizeof(g_cap_packets));
		memcpy(g_cap_xy, (const void *)OFFSET_XY, sizeof(g_cap_xy));
		memcpy(g_quads, (const void *)QUADS, sizeof(g_quads));
		Capture((uint32_t)g_cap_packets, (uint32_t)g_cap_xy);
		// the grid holds the drawn tick's depths: the next tick's in place, then half way
		uint32_t colour = WarpColours[c];
		if (next)
		{
			GridWave(c + 1, WARP_TICKS);
			for (uint32_t o = 0; o < GRID_BYTES; o += 0xC)
				At<int16_t>(GRID + o, 8) = lerp16(*(const int16_t *)(g_grid_save + o + 8), At<int16_t>(GRID + o, 8), num, den);
			colour = LerpColour(WarpColours[c], WarpColours[c + 1], num, den);
		}
		else GridWave(c, WARP_TICKS); // the drawn depths (a warp born on the next tick: its first)
		GridDraw((uint32_t)g_quads, DisplayBuffer(), colour);
	}

	// vertex workspace bytes RenderGeometry writes for a stage group: its largest object
	static uint32_t WorkspaceBytes(uint32_t grp)
	{
		if (!At<uint32_t>(grp, 4)) return 0;
		uint32_t ob = At<uint32_t>(grp, 4);
		int32_t count = At<int32_t>(ob, 0);
		uint32_t most = 0;
		for (int32_t o = 0; o < count; o++)
		{
			uint32_t d = ob + At<uint32_t>(ob, 4 + 4 * (uint32_t)o);
			int32_t groups = At<int16_t>(d, 0);
			d += 2;
			uint32_t verts = 0;
			for (; groups > 0; groups--)
			{
				int32_t nv = At<int16_t>(d, 2);
				d += 4;
				if (nv > 0)
				{
					verts += (uint32_t)nv;
					d += 6 * (uint32_t)nv;
				}
			}
			if (verts > most) most = verts;
		}
		return most * 8;
	}

	static void HeldFrame(int num, int den)
	{
		if (g_nrec == 0) return;
		// a held frame larger than the private buffers is not drawn (never seen)
		if (PacketCursor() - g_packets > sizeof(g_held_packets)) return;
		if (g_arena && FrameArena() - g_arena > sizeof(g_held_arena)) return;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const uint32_t arena = FrameArena();
		// the private arena starts as the real one from where the held draws start (the recorded
		// warp's position, else the cursor: a warp born on the next tick draws there first), so the
		// bytes a packet does not write (pad halves) are the real ones
		const uint32_t base = g_arena ? g_arena : arena;
		const uint32_t limit = var<uint32_t>(0x1D969A8); // end of the frame arena
		uint32_t fill = limit > base ? limit - base : 0;
		if (fill > sizeof(g_held_arena)) fill = sizeof(g_held_arena);
		memcpy(g_held_arena, (const void *)base, fill);
		FrameArena() = (uint32_t)g_held_arena;
		// what the warp's capture writes outside its packets
		memcpy(g_grid_save, (const void *)GRID, GRID_BYTES);
		const uint32_t ws = var<uint32_t>(0x1D98B3C);
		uint32_t ws_bytes = 0;
		uint8_t *sk[4] = {};
		uint32_t sk_bytes[4] = {};
		for (int k = 0; k < 4; k++)
		{
			const uint32_t anim = STAGE_Anim0 + 0x2C * (uint32_t)k;
			if (ws)
			{
				const uint32_t n = WorkspaceBytes(anim - 0x10);
				if (n > ws_bytes) ws_bytes = n;
			}
			uint8_t **pp = *(uint8_t ***)(anim + 4);
			if (pp && *pp)
			{
				sk[k] = *pp;
				sk_bytes[k] = 0x40 + 0x30 * (uint32_t)sk[k][0];
				if (sk_bytes[k] > sizeof(g_bones_save[k])) sk_bytes[k] = sizeof(g_bones_save[k]);
				memcpy(g_bones_save[k], sk[k], sk_bytes[k]);
			}
		}
		if (ws_bytes > sizeof(g_workspace_save)) ws_bytes = sizeof(g_workspace_save);
		if (ws_bytes) memcpy(g_workspace_save, (const void *)ws, ws_bytes);
		const uint32_t last_packet = var<uint32_t>(0x1D99C18); // RenderGeometry: last packet it wrote
		const bool moving = !DrawOnly();
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			LoadGte(r.gte);
			memcpy(FieldScratch(), r.scratch, SCRATCH);
			if (r.fn == ORIG_Emitter) HeldEmitter(r, num, den);
			else HeldWarp(((const WarpNode *)r.node)->counter, moving, num, den);
		}
		// spawned by the next tick (drawn from its first tick): at fraction 1 only, at counter 0, after
		// the replayed draws (the next tick's queue order), from the GTE state / scratch they leave
		if (num >= den && WarpBornNext())
		{
			HeldWarp(0, false, num, den);
		}
		var<uint32_t>(0x1D99C18) = last_packet;
		if (ws_bytes) memcpy((void *)ws, g_workspace_save, ws_bytes);
		for (int k = 3; k >= 0; k--)
			if (sk[k]) memcpy(sk[k], g_bones_save[k], sk_bytes[k]);
		memcpy((void *)GRID, g_grid_save, GRID_BYTES);
		FrameArena() = arena;
	}
}

	static void register_mag154_held()
	{
		mark_held(ultra154::ORIG_Emitter);
		mark_held(ultra154::ORIG_ScreenWarp);
		register_module_held(154, ultra154::HeldReady, ultra154::HeldFrame);
	}
}
