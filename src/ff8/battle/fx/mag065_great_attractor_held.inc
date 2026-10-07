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

// 30 fps part of mag065_great_attractor.cpp: the draw log of the real tick, the held-frame drawing and
// the held-frame camera (included at the end of mag065_great_attractor.cpp; shared pieces in
// mag_common_held.h / fx_held.h).
//
// A real tick records every draw group in order (with the GTE registers and the Field_Alloc scratch it
// started from); a held frame replays the log in the same order with the module's own draw code, on the
// state half way to the next tick's ("exact vanilla shapes"):
//   backdrop  - the billboards' scale, uv scroll and the dot limit half way to the next tick's (counters
//               1..0x1F; the frame-arena backdrop of 0x20..0x21D is constant); the dots' twinkle levels
//               half way from the drawn ones to the ones the tick's twinkle left (a level 0 draws the
//               plain colour instead of the blend: a step to or from 0 holds);
//   layouts   - prim::play_held (every object's exact in-between record): the mirrored pair at the
//               caster's bones, the caster layouts (uv scroll half way while the phase goes on), the
//               overlay (its scale 0x2544F64 half way), the layout tasks (their own scale);
//   beams     - bend amplitudes half way; rays - progress / fade and the tables' spin half way;
//   rings     - radius and spin half way (the ring positions the ribbons read follow);
//   tiles     - the fade level half way;
//   particles - drawn THEN updated: ribbons (every segment that lives on moves / thins half way, the
//               chains' fade too; segments the update adds or frees wait for the real tick), particle
//               models (position, rotation half way along their steps), quads (position), sprite
//               particles (frame by age: hold).
// Anchors at the caster's bones use its in-between pose (pose_midpoint); the caster entity's world
// matrix holds. Blended / bent vertex buffers: a private morph buffer for the layouts, the module's
// own (magic buffer + 0x6CB8) put back for the beams and rays. Everything a held draw writes in the
// module's state (dot levels, particle tables, ring records, scale 0x2544F64) is put back.
// The last draw of a phase (the next tick no longer draws it) holds. No task of the module has a
// draw-only mode; the module's debug pause (0x2544F68) is assumed off for the next tick.
// Camera: HeldCamera runs the master's camera code for the next counter on saved globals (pure math
// on the GTE: registers put back) and returns the in-between look-at / eye, holding on cuts (direct
// sets, segment changes, the return to the saved view at 0x21E) and while a stream wait holds the tick.

namespace ff8fx
{
namespace ga065
{
	using namespace magc;

	static const uint32_t SCRATCH = 0x800;
	static const uint32_t ARENA = 0x10000;        // module packet arena per tick
	static const uint32_t FRAME = 0x40000;        // frame arena (backdrop, tiles, overlay, layout tasks, particle models)
	static const uint32_t MORPH = 0x990;          // magic buffer + 0x6CB8 (bent / twisted / blended vertices)
	static const uint32_t NSTARS = 0x400;

	enum RecKind : uint8_t { REC_BACK_A, REC_BACK_B, REC_MIRROR, REC_CLAYOUT, REC_BEAMS, REC_RAYS, REC_RINGS, REC_TILE, REC_OVERLAY, REC_PARTICLES, REC_LAYOUT };
	struct Rec
	{
		RecKind kind;
		bool flag;                  // mirror: RotY; caster layout: uv scroll; rays / rings: paused
		int32_t step, a, b, c, d;   // per kind (step = phase counter)
		int16_t pos[3];
		int16_t spin[4];            // rays / rings: the tables' spins as drawn
		MasterNode *m;
		LayoutNode *layout;
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct ParticleMemo
	{
		uint8_t chains[0x20 * 0x38];
		uint8_t segs[0x100 * 0x18];   // (the particle models share the area)
		uint8_t sprites[0x80 * 8];
		uint8_t quads[0x40 * 0x20];
	};

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static uint32_t g_packets = 0, g_frame = 0;  // module / frame packet cursors at the tick's first draw
	static Rec g_recs[48];
	static uint32_t g_lev_tick = 0xFFFFFFFF;
	static uint8_t g_levels[NSTARS];              // the dots' twinkle levels as drawn
	static ParticleMemo g_pm;
	static uint32_t g_pm_tick = 0xFFFFFFFF;
	static uint8_t g_held_arena[ARENA];
	static uint8_t g_held_frame[FRAME];
	static uint8_t g_held_morph[0x10000];         // the layouts' blended vertex frames
	// put back after a held frame
	static uint8_t g_save_morph[MORPH], g_save_levels[NSTARS], g_save_rings[0x90], g_save_tabs[0x50];
	static uint8_t g_save_chains[0x20 * 0x38], g_save_segs[0x100 * 0x18], g_save_sprites[0x80 * 8], g_save_quads[0x40 * 0x20];
	static uint8_t g_bones_save[256 * 0x30];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_packets = 0;
		g_frame = 0;
	}

	static Rec *held_add(RecKind kind)
	{
		if (g_log_tick != g_real_tick || g_nrec >= (int)(sizeof(g_recs) / sizeof(g_recs[0]))) return nullptr;
		if (g_nrec == 0)
		{
			g_packets = PacketCursor();
			g_frame = FrameArena();
		}
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.flag = false;
		r.step = r.a = r.b = r.c = r.d = 0;
		r.m = nullptr;
		r.layout = nullptr;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	inline uint8_t *StarLevel(int i) { return (uint8_t *)(TexBase() + 0xDB8 + i * 0x10 + 0xB); }

	static void held_note_backdrop(int kind, int32_t step, int32_t size)
	{
		Rec *r = held_add(kind == 0 ? REC_BACK_A : REC_BACK_B);
		if (!r) return;
		r->step = step;
		r->a = size;
		if (g_lev_tick != g_real_tick)
		{
			g_lev_tick = g_real_tick;
			for (int i = 0; i < (int)NSTARS; i++) g_levels[i] = *StarLevel(i);
		}
	}

	static void held_note_mirrored(MasterNode *m, bool roty)
	{
		Rec *r = held_add(REC_MIRROR);
		if (!r) return;
		r->m = m;
		r->flag = roty;
	}

	static void held_note_caster_layout(MasterNode *m, int32_t step, bool scroll, int32_t len)
	{
		Rec *r = held_add(REC_CLAYOUT);
		if (!r) return;
		r->m = m;
		r->step = step;
		r->flag = scroll;
		r->a = len;
	}

	static void held_note_beams(int32_t step, int32_t a, int32_t b)
	{
		Rec *r = held_add(REC_BEAMS);
		if (!r) return;
		r->step = step;
		r->a = a;
		r->b = b;
	}

	static const uint32_t RAY_TABS[4] = { 0x133AC40, 0x133AC54, 0x133AC68, 0x133AC7C };
	static const uint32_t RING_RECS[4] = { 0x133AC90, 0x133ACB4, 0x133ACD8, 0x133ACFC };

	static void held_note_rays(int32_t step, int32_t t, int32_t u)
	{
		Rec *r = held_add(REC_RAYS);
		if (!r) return;
		r->step = step;
		r->a = t;
		r->b = u;
		r->flag = Pause() != 0;
		// the spins as the four draws will see them (each ray adds its step first while u is past 1/2)
		const bool turn = 0x1000 - u < 0x800 && Pause() == 0;
		for (int k = 0; k < 4; k++)
			r->spin[k] = (int16_t)(*(const int16_t *)(RAY_TABS[k] + 0x12) + (turn ? *(const int16_t *)(RAY_TABS[k] + 0x10) : 0));
	}

	static void held_note_rings(int32_t step, const int16_t *centre, int32_t size)
	{
		Rec *r = held_add(REC_RINGS);
		if (!r) return;
		r->step = step;
		r->a = size;
		r->flag = Pause() != 0;
		for (int k = 0; k < 3; k++) r->pos[k] = centre[k];
		for (int k = 0; k < 4; k++)
			r->spin[k] = (int16_t)(*(const int16_t *)(RING_RECS[k] + 0xC) + (Pause() == 0 ? *(const int16_t *)(RING_RECS[k] + 0xE) : 0));
	}

	static void held_note_tile(int32_t t, int32_t in, int32_t hold, int32_t out, int32_t len)
	{
		Rec *r = held_add(REC_TILE);
		if (!r) return;
		r->step = t;
		r->a = in;
		r->b = hold;
		r->c = out;
		r->d = len;
	}

	static void held_note_overlay(MasterNode *m, int32_t step)
	{
		Rec *r = held_add(REC_OVERLAY);
		if (!r) return;
		r->m = m;
		r->step = step;
	}

	// which: 0 ribbons, 1 sprite particles, 2 particle models, 3 quads (the tables as drawn)
	static void held_note_particles(int which)
	{
		Rec *r = held_add(REC_PARTICLES);
		if (!r) return;
		r->step = which;
		g_pm_tick = g_real_tick;
		switch (which)
		{
		case 0:
			memcpy(g_pm.chains, Chains(), sizeof(g_pm.chains));
			memcpy(g_pm.segs, Segs(), sizeof(g_pm.segs));
			break;
		case 1: memcpy(g_pm.sprites, Sprites(), sizeof(g_pm.sprites)); break;
		case 2: memcpy(g_pm.segs, PModels(), 0x80 * 0x16); break;
		default: memcpy(g_pm.quads, Quads(), sizeof(g_pm.quads)); break;
		}
	}

	static void held_note_layout(LayoutNode *L)
	{
		Rec *r = held_add(REC_LAYOUT);
		if (!r) return;
		r->layout = L;
	}

	// ------------------------------------------------------------------
	// held-frame pieces
	// ------------------------------------------------------------------
	// the master's counter for the next tick (it ran this tick: it is incremented)
	static int32_t NextCounter()
	{
		const MasterNode *m = (const MasterNode *)QMaster()->head;
		return m ? m->counter : -1;
	}

	// a prim-model layout still plays on the next tick (its frame, counted by this tick's play, below its frame count)
	static bool LayoutPlaysOn(const uint8_t *layout)
	{
		const prim::Layout *l = (const prim::Layout *)layout;
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	// particle system `which` drawn by the next tick (flags the next tick sets before its draws)
	static bool ParticlesNext(int which)
	{
		const int32_t c = NextCounter();
		uint8_t f[4] = { Flags()[0], Flags()[1], Flags()[2], Flags()[3] };
		if (c == 0x1D8) { f[0] = f[1] = 0; f[2] = f[3] = 1; }
		if (c == 0x21E) f[2] = f[3] = 0;
		if (which >= 2 && f[1]) return false; // (the sprite particles are drawn instead of the models / quads)
		return f[which] != 0;
	}
	static int32_t lerp_next(int32_t a, bool next, int32_t b, int num, int den) { return next ? lerp_i(a, b, num, den) : a; }

	// the caster's bones at the in-between pose (the skeleton's matrices are put back by BonesRestore)
	static uint32_t BonesHeld(int num, int den)
	{
		uint8_t *skel = **(uint8_t ***)(Caster() + 0x64);
		const uint32_t size = (uint32_t)skel[0] * 0x30;
		memcpy(g_bones_save, skel + 0x10, size < sizeof(g_bones_save) ? size : sizeof(g_bones_save));
		pose_midpoint(Caster() + 0x60, Caster() + 0x6C, num, den);
		return size;
	}

	static void BonesRestore(uint32_t size)
	{
		uint8_t *skel = **(uint8_t ***)(Caster() + 0x64);
		memcpy(skel + 0x10, g_bones_save, size < sizeof(g_bones_save) ? size : sizeof(g_bones_save));
	}

	// BonePoint32 / BonePoint16 on the bones as they are (no rebuild from the pose)
	static void AnchorHeld32(const int16_t *v, int32_t *out)
	{
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(Caster() + 0x40), BoneMatrix(v[3]), &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(out);
	}

	static void AnchorHeld16(const int16_t *v, int16_t *out)
	{
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(Caster() + 0x40), BoneMatrix(v[3]), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	static void MirrorHeld(const Rec &r, int num, int den)
	{
		MasterNode *m = r.m;
		// (the counter is the next tick's: the pair plays on in its phase, else the last draw holds)
		if (r.flag ? (uint32_t)(m->counter - 0x37) >= 0xC : (uint32_t)(m->counter - 0x2A) >= 0xD) num = 0;
		const uint32_t bones = BonesHeld(num, den);
		PrimArg arg;
		if (r.flag) RotY(0x320, &arg.m);
		else RotYScaled(-0x320, 0x2000, &arg.m);
		AnchorHeld32((const int16_t *)BONE_L, arg.m.t);
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.morph = (uint32_t)g_held_morph;
		arg.cursor = PacketCursor();
		arg.objs = OBJS0;
		arg.depth = 0;
		arg.scroll = 0;
		prim::play_held((prim::Layout *)m->layout_a, PartCallback, (int)&arg, num, den);
		PacketCursor() = arg.cursor;
		if (r.flag) RotY(0x320, &arg.m);
		else RotYScaled(-0x320, 0x2000, &arg.m);
		arg.m.m[0][0] = (int16_t)-arg.m.m[0][0];
		arg.m.m[0][1] = (int16_t)-arg.m.m[0][1];
		arg.m.m[0][2] = (int16_t)-arg.m.m[0][2];
		AnchorHeld32((const int16_t *)BONE_R, arg.m.t);
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.cursor = PacketCursor();
		prim::play_held((prim::Layout *)m->layout_b, PartCallback, (int)&arg, num, den);
		PacketCursor() = arg.cursor;
		BonesRestore(bones);
	}

	static void CasterLayoutHeld(const Rec &r, int num, int den)
	{
		PrimArg arg;
		RotY(0, &arg.m);
		arg.m.t[0] = CasterPos()[0];
		arg.m.t[1] = 0;
		arg.m.t[2] = CasterPos()[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		const bool next = r.step + 1 < r.a && LayoutPlaysOn(r.m->layout_a);
		if (!next) num = 0; // the last draw of the phase (or of the layout) holds
		arg.scroll = r.flag ? (int16_t)lerp_next(shl32(r.step, 5), next, shl32(r.step + 1, 5), num, den) : 0;
		arg.morph = (uint32_t)g_held_morph;
		arg.cursor = PacketCursor();
		arg.objs = r.flag ? OBJS23 : OBJS0;
		arg.depth = 0;
		prim::play_held((prim::Layout *)r.m->layout_a, PartCallback, (int)&arg, num, den);
		PacketCursor() = arg.cursor;
	}

	static void BeamsHeld(const Rec &r, int num, int den)
	{
		int32_t a = r.a, b = r.b;
		if ((uint32_t)(r.step + 1) < 0x47)
		{
			int32_t a1, b1;
			BeamBends((uint32_t)(r.step + 1), &a1, &b1);
			a = lerp_i(a, a1, num, den);
			b = lerp_i(b, b1, num, den);
		}
		const uint32_t bones = BonesHeld(num, den);
		int16_t r0[3], l0[3], p[3];
		AnchorHeld16((const int16_t *)BONE_R, r0);
		const int16_t *cp = CasterPos();
		p[0] = (int16_t)(cp[0] + 0x2710); p[1] = (int16_t)(cp[1] - 0x2710); p[2] = (int16_t)(cp[2] - 0x7D0);
		Beam(r0, p, a, b);
		p[0] = (int16_t)(cp[0] + 0x2710); p[1] = (int16_t)(cp[1] - 0x4E20); p[2] = (int16_t)(cp[2] - 0x1388);
		Beam(r0, p, a, b);
		AnchorHeld16((const int16_t *)BONE_L, l0);
		a = -a;
		p[0] = (int16_t)(cp[0] - 0x2710); p[1] = (int16_t)(cp[1] - 0x2710); p[2] = (int16_t)(cp[2] - 0x7D0);
		Beam(l0, p, a, b);
		p[0] = (int16_t)(cp[0] - 0x2710); p[1] = (int16_t)(cp[1] - 0x4E20); p[2] = (int16_t)(cp[2] - 0x1388);
		Beam(l0, p, a, b);
		BonesRestore(bones);
	}

	static void RaysHeld(const Rec &r, int num, int den)
	{
		const bool next = (uint32_t)(r.step + 1) < 0x3C;
		int32_t t = r.a, u = r.b, t1 = t, u1 = u;
		if (next) RayParams((uint32_t)(r.step + 1), &t1, &u1);
		const bool turn = next && 0x1000 - u1 < 0x800 && !r.flag;
		int16_t spin[4];
		for (int k = 0; k < 4; k++)
			spin[k] = (int16_t)lerp_next(r.spin[k], turn, (int16_t)(r.spin[k] + *(const int16_t *)(RAY_TABS[k] + 0x10)), num, den);
		t = lerp_next(t, next, t1, num, den);
		u = lerp_next(u, next, u1, num, den);
		MasterNode *m = (MasterNode *)QMaster()->head;
		const int16_t c[3] = { m->centre[0], m->centre[1], (int16_t)(m->centre[2] + 0x7D0) };
		const uint32_t bones = BonesHeld(num, den);
		int16_t r0[3], l0[3];
		AnchorHeld16((const int16_t *)BONE_R, r0);
		RayDraw(r0, c, (const uint8_t *)RAY_TABS[0], t, u, -1, TexBase() + 0x36620, spin[0]);
		RayDraw(r0, c, (const uint8_t *)RAY_TABS[1], t, u, 1, TexBase() + 0x393C8, spin[1]);
		AnchorHeld16((const int16_t *)BONE_L, l0);
		RayDraw(l0, c, (const uint8_t *)RAY_TABS[2], t, u, 1, TexBase() + 0x37558, spin[2]);
		RayDraw(l0, c, (const uint8_t *)RAY_TABS[3], t, u, -1, TexBase() + 0x38490, spin[3]);
		BonesRestore(bones);
	}

	static void RingsHeld(const Rec &r, int num, int den)
	{
		const bool next = (uint32_t)(r.step + 1) < 0x44;
		const int32_t size = lerp_next(r.a, next, next ? RingSize((uint32_t)(r.step + 1)) : r.a, num, den);
		static const uint32_t MODELS[4] = { 0x36620, 0x393C8, 0x38490, 0x37558 };
		for (int k = 0; k < 4; k++)
		{
			uint8_t *rec = (uint8_t *)RING_RECS[k];
			const int16_t spin = (int16_t)lerp_next(r.spin[k], next && !r.flag, (int16_t)(r.spin[k] + *(const int16_t *)(rec + 0xE)), num, den);
			RingDraw(rec, r.pos, size, TexBase() + MODELS[k], spin);
		}
	}

	static void OverlayHeld(const Rec &r, int num, int den)
	{
		const bool next = (uint32_t)(r.step + 1) < 0x46 && LayoutPlaysOn(r.m->layout_a);
		if (!next) num = 0; // the last draw (of the phase or of the layout) holds
		const int32_t s0 = OverlayScale((uint32_t)r.step);
		Scale() = lerp_next(s0, next, next ? OverlayScale((uint32_t)(r.step + 1)) : s0, num, den);
		PrimArg arg;
		RotY(0, &arg.m);
		arg.m.t[0] = 0;
		arg.m.t[1] = 0;
		arg.m.t[2] = 0x1000;
		arg.morph = (uint32_t)g_held_morph;
		arg.objs = OBJS0;
		arg.depth = -0x400;
		arg.scroll = 0;
		arg.cursor = FrameArena();
		prim::play_held((prim::Layout *)r.m->layout_a, ScaledPartCallback, (int)&arg, num, den);
		FrameArena() = arg.cursor;
	}

	static void LayoutHeld(const Rec &r, int num, int den)
	{
		LayoutNode *L = r.layout;
		if (NextCounter() == 0x1D8) num = 0; // the master drops the layout tasks on the next tick: the last draw holds
		PrimArg arg;
		memset(&arg.m, 0, sizeof(arg.m.m));
		arg.m.m[0][0] = arg.m.m[1][1] = arg.m.m[2][2] = 0x1000;
		arg.m.pad = 0;
		arg.m.t[0] = L->pos[0];
		arg.m.t[1] = L->pos[1];
		arg.m.t[2] = L->pos[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		Scale() = L->scale;
		arg.depth = L->depth;
		arg.morph = (uint32_t)g_held_morph;
		arg.objs = OBJS0;
		arg.cursor = FrameArena();
		arg.scroll = 0;
		prim::play_held((prim::Layout *)L->layout, ScaledPartCallback, (int)&arg, num, den);
		FrameArena() = arg.cursor;
	}

	// ribbons: the tables as drawn, every segment the update kept moved half way (its own step), the
	// chains' fade half way
	static void RibbonsHeld(int num, int den)
	{
		uint8_t *chains = Chains(), *segs = Segs();
		memcpy(g_save_chains, chains, sizeof(g_save_chains));
		memcpy(g_save_segs, segs, sizeof(g_save_segs));
		memcpy(chains, g_pm.chains, sizeof(g_pm.chains));
		memcpy(segs, g_pm.segs, sizeof(g_pm.segs));
		for (int i = 0; i < 0x20; i++)
		{
			uint8_t *c = chains + i * 0x38;
			if (F<uint32_t>(c, 0) == 0) continue;
			const uint8_t *now = g_save_chains + i * 0x38;
			if (*(const uint32_t *)now != 0) F<int16_t>(c, 0x36) = (int16_t)lerp_i(F<int16_t>(c, 0x36), *(const int16_t *)(now + 0x36), num, den);
			for (uint8_t *s = F<uint8_t *>(c, 0); s; s = F<uint8_t *>(s, 0x14))
			{
				const uint8_t *n = g_save_segs + (s - segs);
				int16_t nw = (int16_t)(F<int16_t>(s, 0x10) + F<int16_t>(s, 0x12));
				if (nw < 0) nw = 0;
				bool same = *(const int16_t *)(n + 6) != -1 && *(const int16_t *)(n + 0xE) == F<int16_t>(s, 0xE) && *(const int16_t *)(n + 0x10) == nw;
				for (int k = 0; k < 3; k++)
					if (*(const int16_t *)(n + 2 * k) != (int16_t)(F<int16_t>(s, 2 * k) + F<int16_t>(s, 8 + 2 * k))) same = false;
				if (!same) continue;
				for (int k = 0; k < 3; k++) F<int16_t>(s, 2 * k) = lerp16(F<int16_t>(s, 2 * k), *(const int16_t *)(n + 2 * k), num, den);
				F<int16_t>(s, 0x10) = lerp16(F<int16_t>(s, 0x10), nw, num, den);
			}
		}
		RibbonsDraw();
		memcpy(chains, g_save_chains, sizeof(g_save_chains));
		memcpy(segs, g_save_segs, sizeof(g_save_segs));
	}

	static void ParticlesHeld(int which, int num, int den)
	{
		if (g_pm_tick != g_real_tick) return;
		if (!ParticlesNext(which)) num = 0; // a system the next tick no longer draws holds its last draw
		if (which == 0) RibbonsHeld(num, den);
		else if (which == 1)
		{
			// sprite particles: frame by age (holds), position fixed
			memcpy(g_save_sprites, Sprites(), sizeof(g_save_sprites));
			memcpy(Sprites(), g_pm.sprites, sizeof(g_pm.sprites));
			SpritesDraw();
			memcpy(Sprites(), g_save_sprites, sizeof(g_save_sprites));
		}
		else if (which == 2)
		{
			uint8_t *p = PModels();
			memcpy(g_save_segs, p, 0x80 * 0x16);
			memcpy(p, g_pm.segs, 0x80 * 0x16);
			for (int i = 0; i < 0x80; i++)
			{
				PModel *q = (PModel *)(p + i * 0x16);
				if (q->life <= 1) continue; // (life 1: freed by the update, its last draw holds)
				for (int k = 0; k < 3; k++) q->pos[k] = lerp16(q->pos[k], (int16_t)(q->pos[k] + q->vel[k]), num, den);
				for (int k = 0; k < 2; k++) q->rot[k] = lerp16(q->rot[k], (int16_t)(q->rot[k] + (int16_t)q->drot[k]), num, den);
			}
			PModelsDraw();
			memcpy(p, g_save_segs, 0x80 * 0x16);
		}
		else
		{
			uint8_t *p = Quads();
			memcpy(g_save_quads, p, sizeof(g_save_quads));
			memcpy(p, g_pm.quads, sizeof(g_pm.quads));
			for (int i = 0; i < 0x40; i++)
			{
				QuadP *q = (QuadP *)(p + i * 0x20);
				if (q->life <= 1) continue; // (life 1: freed by the update, its last draw holds)
				for (int k = 0; k < 3; k++) q->pos[k] = lerp16(q->pos[k], (int16_t)(q->pos[k] + q->vel[k]), num, den);
			}
			QuadsDraw();
			memcpy(p, g_save_quads, sizeof(g_save_quads));
		}
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_arena, ARENA, g_packets);
		const uint32_t frame = FrameArena();
		if (g_frame && frame > g_frame)
		{
			const uint32_t used = frame - g_frame;
			memcpy(g_held_frame, (const void *)g_frame, used < FRAME ? used : FRAME);
		}
		FrameArena() = (uint32_t)g_held_frame;
		const int32_t scale = Scale();
		uint8_t *morph = (uint8_t *)(TexBase() + 0x6CB8);
		memcpy(g_save_morph, morph, MORPH);
		memcpy(g_save_rings, (const void *)0x133AC90, sizeof(g_save_rings));
		memcpy(g_save_tabs, (const void *)0x133AC40, sizeof(g_save_tabs));
		const bool levels = g_lev_tick == g_real_tick;
		if (levels)
		{
			for (int i = 0; i < (int)NSTARS; i++)
			{
				uint8_t *l = StarLevel(i);
				g_save_levels[i] = *l;
				// level 0 draws the plain colour (no blend): a step to or from it is discrete (holds)
				if (g_levels[i] == 0 || *l == 0) *l = num < den ? g_levels[i] : *l;
				else *l = (uint8_t)lerp_i(g_levels[i], *l, num, den);
			}
		}
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_BACK_A:
			{
				const bool next = (uint32_t)(r.step + 1) < 0x1F;
				const int32_t s1 = next ? BackdropSize((uint32_t)(r.step + 1)) : r.a;
				const int32_t size = lerp_next(r.a, next, s1, num, den);
				const int32_t scroll = lerp_next(shl32(r.step, 5), next, shl32(r.step + 1, 5), num, den);
				const int32_t limit = lerp_next(sdiv_pow2(r.a, 2), next, sdiv_pow2(s1, 2), num, den);
				PacketCursor() = Billboard(CasterPos()[0], -1000, CasterPos()[2] - 1000, 0, size, 0, 0x800C, 0x800, 0x1334FFC, PacketCursor());
				PacketCursor() = Billboard(CasterPos()[0], -1000, CasterPos()[2] - 1000, 0, size, scroll, 0x8030, -0x200, 0x1336924, PacketCursor());
				PacketCursor() = Stars(0, CasterPos()[0], 0, CasterPos()[2], limit, PacketCursor());
				break;
			}
			case REC_BACK_B:
				FrameArena() = Billboard(CasterPos()[0], 0, CasterPos()[2], 0, 0x1000, 0, 0xC, 0, 0x1334FFC, FrameArena());
				FrameArena() = Stars(0, CasterPos()[0], 0, CasterPos()[2], 0x10000, FrameArena());
				break;
			case REC_MIRROR: MirrorHeld(r, num, den); break;
			case REC_CLAYOUT: CasterLayoutHeld(r, num, den); break;
			case REC_BEAMS: BeamsHeld(r, num, den); break;
			case REC_RAYS: RaysHeld(r, num, den); break;
			case REC_RINGS: RingsHeld(r, num, den); break;
			case REC_TILE:
			{
				int32_t level = FadeLevel(r.step, r.a, r.b, r.c);
				if (r.step + 1 < r.d) level = lerp_i(level, FadeLevel(r.step + 1, r.a, r.b, r.c), num, den);
				FrameArena() = Tile(0xFF, 0xFF, 0xFF, level, FrameArena());
				break;
			}
			case REC_OVERLAY: OverlayHeld(r, num, den); break;
			case REC_PARTICLES: ParticlesHeld(r.step, num, den); break;
			case REC_LAYOUT: LayoutHeld(r, num, den); break;
			}
		}
		if (levels)
			for (int i = 0; i < (int)NSTARS; i++) *StarLevel(i) = g_save_levels[i];
		memcpy((void *)0x133AC40, g_save_tabs, sizeof(g_save_tabs));
		memcpy((void *)0x133AC90, g_save_rings, sizeof(g_save_rings));
		memcpy(morph, g_save_morph, MORPH);
		Scale() = scale;
		FrameArena() = frame;
	}

	// ------------------------------------------------------------------
	// held-frame camera
	// ------------------------------------------------------------------
	// counters on which the master writes the camera words
	static bool CameraWritten(int32_t c)
	{
		return (uint32_t)c <= 0x1F || c == 0x36 || (uint32_t)(c - 0x42) < 0x65 || (uint32_t)(c - 0xC5) < 0x29 || c == 0x107 || c == 0x121
			|| c == 0x13B || (uint32_t)(c - 0x155) < 0x80 || c == 0x1D7 || c == 0x21E;
	}

	// direct sets (and the return to the saved view): the camera jumps
	static bool CameraCut(int32_t c)
	{
		return c == 0x1F || c == 0x36 || c == 0xA6 || c == 0xED || c == 0x107 || c == 0x121 || c == 0x13B || c == 0x1D7 || c == 0x21E;
	}

	inline int32_t AbsI(int32_t v) { return v < 0 ? -v : v; }

	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (g_ported_tick != g_real_tick) return false;
		const MasterNode *m = (const MasterNode *)QMaster()->head;
		if (!m) return false;
		const int32_t c = m->counter;
		if (c == 0 || !CameraWritten(c)) return false;
		if (c == 0x1F || c == 0xED)
		{
			// a stream wait (0x534270 not idle) holds the tick: nothing moves
			const uint8_t *st = var<const uint8_t *>(0x1DCD6EC);
			if (!(st[3] == st[4] && st[0] == 0 && st[2] != 0 && st[1] == 0)) return false;
		}
		int16_t *eye = (int16_t *)0xB8B7F0, *at = (int16_t *)0xB8B7F8;
		int16_t e0[3], a0[3];
		for (int k = 0; k < 3; k++) { e0[k] = eye[k]; a0[k] = at[k]; }
		bool cut = CameraCut(c);
		int16_t e1[3], a1[3];
		if (!cut)
		{
			static uint8_t cam_save[0x10];
			static uint8_t gte_save[GTE_DATA_SIZE + GTE_CTRL_SIZE];
			memcpy(cam_save, (const void *)0xB8B7F0, sizeof(cam_save));
			const int16_t zoom = CameraZoom();
			memcpy(gte_save, (const void *)GTE_DATA, GTE_DATA_SIZE);
			memcpy(gte_save + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
			MasterCamera(c);
			for (int k = 0; k < 3; k++) { e1[k] = eye[k]; a1[k] = at[k]; }
			memcpy((void *)GTE_DATA, gte_save, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, gte_save + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			CameraZoom() = zoom;
			memcpy((void *)0xB8B7F0, cam_save, sizeof(cam_save));
			// a keyed segment that starts away from the current camera is a cut too
			for (int k = 0; k < 3; k++)
				if (AbsI(e1[k] - e0[k]) > 1500 || AbsI(a1[k] - a0[k]) > 1500) cut = true;
		}
		for (int k = 0; k < 3; k++)
		{
			world[k] = cut ? e0[k] : lerp16(e0[k], e1[k], num, den);
			lookat[k] = cut ? a0[k] : lerp16(a0[k], a1[k], num, den);
		}
		return true;
	}
}

	static void register_mag065_held()
	{
		mark_held(ga065::ORIG_MasterTask);
		mark_held(ga065::ORIG_LayoutTask);
		register_module_held(65, ga065::HeldReady, ga065::HeldFrame);
		register_module_camera(65, ga065::HeldCamera);
	}
}
