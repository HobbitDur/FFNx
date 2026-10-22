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

// 30 fps part of mag260_shockwave_pulsar_quistis.cpp (Quistis' Shockwave Pulsar, effect 260): the draw log of the real
// tick, the held-frame drawing and the held-frame camera (included at the end of mag260_shockwave_pulsar_quistis.cpp;
// shared pieces in mag_common_held.h / fx_held.h). Exact vanilla shapes: the logic stays at 15 Hz, a held frame draws
// the state num / den of the way from the last real tick to the next one.
//
// A real tick records every draw in execution order (master, then the sub-task queue): its kind and parameters as
// drawn, the GTE registers and the Field_Alloc scratch it started from. A held frame replays the log in the same order
// with the module's own draw code, each part num / den of the way to the draw the next tick makes; nothing random is
// drawn, nothing is written back:
//   layouts  - prim::play_held (every object's exact in-between record): the centre layout (fixed anchor), the
//              shockwave layout (its anchor z and depth half way on the 0xD5 -> 0xD6 switch), the Lift / Rise layouts
//              (anchor height half way to the one the task set for the next tick: entity +0x1E), the Ring layout.
//   models   - the targets redrawn by 0x693170 (the master's on 0x50..0xD5, the Rise task's every tick): angles half way
//              to the next draw's (the master's: the angles the Lift task left; the Rise task's: the turn its next
//              update makes), the scale half way (the Rise task's ease), the pose half way (pose_midpoint, body and
//              weapon: the next-pose approach; the targets' bone matrices put back), the entity's world matrix (+0x40)
//              half way from the one the real draw used to the one the frame holds (the engine rebuilds +0x40 from
//              the entity's position every frame, before the effect: the next draw's; element-wise, equal ends =
//              exactly as drawn).
//   sparks   - a spark the tick drew and moved (same flipbook, life - 1, same velocity): position half way, flipbook
//              frame of the real tick; one that dies holds.
//   shards   - the same with their two scales (they sit at their target's position, which does not move then).
//   tile     - the white tile's level half way to the next step.
// A draw the next tick does not make (the centre layout after 0x4F, the models after 0xD5, the shockwave after 0x105,
// the shards from 0xD4, Lift's last tick, the Rise / Ring tasks when the master resets the pool at 0x50, a finished
// Ring layout, the tile's last step, a dying spark / shard) holds below 1 and is not drawn at 1. Sparks born after the
// master's draw (the Lift bursts) are not drawn before the tick that draws them (random growth stays at 15 Hz).
// The effect camera 0x25086C0 (flag-0x1000 objects) is the one of the frame's camera (0x67AAE0 on 0x1D97778), as every
// real tick builds it before drawing; put back. No part follows Quistis' bones (her animation set and chain only).
// The held frame draws into private packet buffers (module arena, frame arena, vertex workspace) and puts back what the
// draws write outside them: GTE, Field_Alloc scratch, shadow-rotation scratch (HeldScope), the targets' bone matrices
// and world matrices, RenderGeometry's 0x1D99C18, the effect camera.
// Camera: HeldCamera runs the master's camera code for the next counter on saved words (pure math: CamLerp 0x657DF0 and
// the cosine; GTE, words, field of view, roll and the Field_Alloc scratch put back) and returns the in-between look-at /
// eye, holding on cuts (direct sets 0x3B / 0xD5, the segment start 0x4F, the return to the saved view at 0x106, any
// jump over 1500).

namespace ff8fx
{
namespace pulsar260
{
	static const uint32_t SCRATCH = 0x200;      // Field_Alloc scratch recorded per draw
	static const int MAX_RECS = 96;
	static const int MAX_ENTS = 8;

	struct Rec
	{
		int kind;
		prim::Layout *layout;                   // layouts: the live layout (prim::play_held knows what it drew)
		const void *node;                       // layouts: the drawing node; models: the Rise node (nullptr: the master)
		PrimArg arg;                            // layouts: the parameter block as drawn
		Target *rec;                            // Lift / Rise / models: the target
		uint8_t *entity;                        // models: the entity drawn
		int16_t ang[3], vec[3];                 // models: angles as drawn, offset
		int32_t scale, counter, len;            // models: scale; the drawing task's counter; Rise: lift length; tile: step
		uint8_t mat[0x20];                      // models: the entity's world matrix as drawn
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];               // Field_Alloc scratch under the draw's allocations
	};

	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0;
	static const MasterNode *g_master = nullptr;
	static int32_t g_master_counter = -1;       // the master's counter on this tick
	static uint32_t g_packets = 0, g_frame = 0; // module / frame arena cursors at the tick's first draw
	static Rec g_recs[MAX_RECS];
	static Spark g_sparks[0x80];                // the pools as drawn
	static Shard g_shards[0x40];
	static uint8_t g_held_packets[0x10000];
	static uint8_t g_held_frame[0x40000];
	static uint8_t g_held_morph[0x2000];
	static uint8_t g_ws_save[0x10000];
	static uint8_t g_bones_save[2][0x40 + 0x30 * 256];

	static void *FieldScratch() { return var<void *>(0x1D999C4); }

	static void held_note_master(const MasterNode *m)
	{
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_master = m;
		g_master_counter = m->counter;
		g_packets = 0;
		g_frame = 0;
	}

	static Rec *held_add(int kind, int32_t counter)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		if (g_nrec == 0)
		{
			g_packets = PacketCursor();
			g_frame = FrameArena();
		}
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.counter = counter;
		r.layout = nullptr;
		r.node = nullptr;
		r.rec = nullptr;
		r.entity = nullptr;
		r.len = 0;
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, FieldScratch(), SCRATCH);
		return &r;
	}

	static void held_note_play(int kind, const void *node, const PrimArg *arg, int32_t counter)
	{
		Rec *r = held_add(kind, counter);
		if (!r) return;
		r->node = node;
		if (kind == K_CENTRE || kind == K_WAVE) r->layout = (prim::Layout *)((const MasterNode *)node)->layout;
		else
		{
			r->layout = (prim::Layout *)((const SubNode *)node)->layout;
			if (kind != K_RING) r->rec = ((const SubNode *)node)->t.rec;
		}
		r->arg = *arg;
	}

	static void held_note_tile(int32_t w)
	{
		held_add(K_TILE, w);
	}

	static void held_note_model(const SubNode *rise, const Target *rec, uint8_t *e, const int16_t *vec)
	{
		Rec *r = held_add(K_MODEL, rise ? rise->t.counter : g_master_counter);
		if (!r) return;
		r->node = rise;
		r->rec = (Target *)rec;
		r->entity = e;
		memcpy(r->ang, rec->ang, sizeof(r->ang));
		memcpy(r->vec, vec, sizeof(r->vec));
		r->scale = rec->scale;
		r->len = rise ? rise->t.len : 0;
		memcpy(r->mat, e + 0x40, sizeof(r->mat));
	}

	static void held_note_sparks()
	{
		if (held_add(K_SPARKS, g_master_counter)) memcpy(g_sparks, (const void *)(TexBase() + SPARKS), sizeof(g_sparks));
	}

	static void held_note_shards()
	{
		if (held_add(K_SHARDS, g_master_counter)) memcpy(g_shards, (const void *)(TexBase() + SHARDS), sizeof(g_shards));
	}

	static bool HeldReady() { return g_log_tick == g_real_tick && g_nrec > 0; }

	// the master is still queued (it ends on its counter 0x106 once the streams are loaded)
	static bool MasterLive() { return g_master && QMaster()->head == (TaskNode *)g_master; }
	// the sub-tasks run on the next tick: the master is live and does not reset the pool there (its counter 0x50)
	static bool SubsNext() { return MasterLive() && g_master->counter != 0x50 && g_master_counter < 0x106; }

	// ------------------------------------------------------------------
	// layouts
	// ------------------------------------------------------------------
	// the layout has a frame left to play (prim::play: frames 0..total - 1, a finished layout draws nothing)
	static bool LayoutMore(const prim::Layout *l)
	{
		const uint8_t *hdr = l->data + *(const int32_t *)(l->data + 4);
		return l->frame < *(const int32_t *)(hdr + 4);
	}

	static void HeldPlay(const Rec &r, int num, int den)
	{
		PrimArg a = r.arg;
		bool next = false;
		switch (r.kind)
		{
		case K_CENTRE: next = MasterLive() && r.counter < 0x4F; break;
		case K_WAVE:
			next = MasterLive() && r.counter < 0x105;
			if (next && r.counter == 0xD5)
			{
				// the next tick draws the layout at the centre, deeper (the models are no longer redrawn)
				a.pos[2] = lerp16(a.pos[2], (int16_t)CentreZ(), num, den);
				a.depth = lerp16(a.depth, (int16_t)0xF000, num, den);
			}
			break;
		case K_LIFT:
		case K_RISE:
		{
			next = SubsNext() && (r.kind == K_RISE || r.counter < 0x1D);
			// the anchor height the task set for the next tick
			PrimArg n;
			TargetArg(n, r.rec, r.rec->entity);
			if (next) a.pos[1] = lerp16(a.pos[1], n.pos[1], num, den);
			break;
		}
		default: next = SubsNext(); break; // a Ring task ends with its layout
		}
		next = next && LayoutMore(r.layout);
		if (!next && num >= den) return; // the last draw: the next tick draws none
		a.morph = (uint32_t)g_held_morph;
		prim::play_held(r.layout, PrimPart, (int)&a, next ? num : 0, den);
	}

	// ------------------------------------------------------------------
	// the white tile (master 0xCE..0xD5: steps 0..7)
	// ------------------------------------------------------------------
	static void HeldTile(const Rec &r, int num, int den)
	{
		const int32_t w = r.counter;
		const bool next = MasterLive() && w + 1 < 8;
		if (!next && num >= den) return;
		const int32_t l0 = FadeLevel(w, 7, 1, 0);
		const int32_t level = next ? lerp_i(l0, FadeLevel(w + 1, 7, 1, 0), num, den) : l0;
		PacketCursor() = Tile(0xFF, 0xFF, 0xFF, level, PacketCursor());
	}

	// ------------------------------------------------------------------
	// the targets' redrawn models (0x693170)
	// ------------------------------------------------------------------
	static uint32_t WorkspaceBytes(uint32_t model)
	{
		if (!model || !*(const uint32_t *)(model + 4)) return 0;
		const uint32_t ob = *(const uint32_t *)(model + 4);
		const int32_t count = *(const int32_t *)ob;
		uint32_t most = 0;
		for (int32_t o = 0; o < count; o++)
		{
			uint32_t d = ob + *(const uint32_t *)(ob + 4 + 4 * (uint32_t)o);
			int32_t groups = *(const int16_t *)d;
			d += 2;
			uint32_t verts = 0;
			for (; groups > 0; groups--)
			{
				const int32_t nv = *(const int16_t *)(d + 2);
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

	// skeleton of an anim header (BattleAnimHeader: +4 -> pointer -> bone count, matrices)
	static uint8_t *Skeleton(uint32_t anim, uint32_t *bytes)
	{
		if (!anim) return nullptr;
		uint8_t **pp = *(uint8_t ***)(anim + 4);
		if (!pp || !*pp) return nullptr;
		uint8_t *sk = *pp;
		uint32_t n = 0x40 + 0x30 * (uint32_t)sk[0];
		if (n > sizeof(g_bones_save[0])) n = sizeof(g_bones_save[0]);
		*bytes = n;
		return sk;
	}

	// the next draw of a model record: angles / scale (false: the next tick does not draw it)
	static bool ModelNext(const Rec &r, int16_t ang[3], int32_t &scale)
	{
		const Target *rec = r.rec;
		if (!r.node)
		{
			// the master (0x50..0xD5): the angles the Lift task left, the scale as is
			if (!MasterLive() || r.counter >= 0xD5) return false;
			memcpy(ang, rec->ang, 6);
			scale = rec->scale;
			return true;
		}
		// the Rise task: its next update eases the scale, turns the target (lift steps), then draws
		if (!SubsNext()) return false;
		const int32_t c = r.counter + 1;
		scale = RiseScale(rec, c);
		memcpy(ang, r.ang, 6);
		const uint32_t i = (uint32_t)(c - 0xD), len = (uint32_t)r.len;
		if (i < len)
		{
			const int32_t t = (int32_t)((i << 12) / len);
			const int16_t d = (int16_t)sdiv_pow2(t, 3);
			const int32_t sgn = (SlotOf(rec->entity) & 1) ? 1 : -1;
			ang[0] = (int16_t)(ang[0] + sgn * d);
			ang[1] = (int16_t)(ang[1] + sgn * d);
		}
		return true;
	}

	static void HeldModel(const Rec &r, int num, int den)
	{
		uint8_t *e = r.entity;
		int16_t next_ang[3];
		int32_t next_scale = r.scale;
		const bool next = ModelNext(r, next_ang, next_scale);
		if (!next && num >= den) return;
		const int pnum = next ? num : 0;
		int16_t ang[3];
		for (int k = 0; k < 3; k++) ang[k] = next ? lerp16(r.ang[k], next_ang[k], num, den) : r.ang[k];
		const int32_t scale = next ? lerp_i((int16_t)r.scale, (int16_t)next_scale, num, den) : r.scale;
		// the world matrix: from the one drawn to the one the frame holds (the next draw's)
		uint8_t mat[0x20];
		memcpy(mat, e + 0x40, sizeof(mat));
		if (memcmp(mat, r.mat, sizeof(mat)))
		{
			Mat4x3 &m = *(Mat4x3 *)(e + 0x40);
			const Mat4x3 &a = *(const Mat4x3 *)r.mat, &b = *(const Mat4x3 *)mat;
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 3; j++) m.m[i][j] = lerp16(a.m[i][j], b.m[i][j], pnum, den);
				m.t[i] = lerp_i(a.t[i], b.t[i], pnum, den);
			}
		}
		uint32_t b0 = 0, b1 = 0;
		uint8_t *sk0 = Skeleton((uint32_t)(e + 0x60), &b0);
		const uint32_t w = *(const uint32_t *)(e + 0x78);
		uint8_t *sk1 = Skeleton(w, &b1);
		if (sk0) memcpy(g_bones_save[0], sk0, b0);
		if (sk1) memcpy(g_bones_save[1], sk1, b1);
		if (pnum)
		{
			pose_midpoint(e + 0x60, e + 0x6C, pnum, den);
			if (w) pose_midpoint((void *)w, (void *)(w + 0xC), pnum, den);
		}
		DrawEntityModel(e, ang, r.vec, scale);
		if (sk0) memcpy(sk0, g_bones_save[0], b0);
		if (sk1) memcpy(sk1, g_bones_save[1], b1);
		memcpy(e + 0x40, mat, sizeof(mat));
	}

	// ------------------------------------------------------------------
	// pools: an entry the tick drew and updated moves half way
	// ------------------------------------------------------------------
	static void HeldSparks(int num, int den)
	{
		Spark tmp[0x80];
		memcpy(tmp, g_sparks, sizeof(tmp));
		const Spark *cur = (const Spark *)(TexBase() + SPARKS);
		const bool live = MasterLive();
		for (int k = 0; k < 0x80; k++)
		{
			Spark &s = tmp[k];
			const Spark &c = cur[k];
			if (s.life == 0) continue;
			const bool moved = live && c.life != 0 && c.life == (uint8_t)(s.life - 1) && c.seq == s.seq && c.vel[0] == s.vel[0] && c.vel[1] == s.vel[1] && c.vel[2] == s.vel[2];
			if (!moved)
			{
				if (num >= den) s.life = 0; // not drawn by the next tick
				continue;
			}
			for (int a = 0; a < 3; a++) s.pos[a] = lerp16(s.pos[a], c.pos[a], num, den);
			if (num >= den) s.life = c.life; // the next tick's flipbook frame
		}
		SparksDraw(tmp);
	}

	static void HeldShards(int num, int den)
	{
		Shard tmp[0x40];
		memcpy(tmp, g_shards, sizeof(tmp));
		const Shard *cur = (const Shard *)(TexBase() + SHARDS);
		const bool live = MasterLive() && g_master_counter < 0xD4; // the master draws the shards before 0xD5
		for (int k = 0; k < 0x40; k++)
		{
			Shard &s = tmp[k];
			const Shard &c = cur[k];
			if (s.life == 0) continue;
			const bool moved = live && c.life != 0 && c.life == (int16_t)(s.life - 1) && c.rec == s.rec && c.off == s.off;
			if (!moved)
			{
				if (num >= den) s.life = 0;
				continue;
			}
			s.sy = lerp16(s.sy, c.sy, num, den);
			s.sxz = lerp16(s.sxz, c.sxz, num, den);
		}
		ShardsDraw(tmp);
	}

	// ------------------------------------------------------------------
	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		// a held frame larger than the private buffers is not drawn (never seen)
		if (g_packets && PacketCursor() - g_packets > sizeof(g_held_packets)) return;
		if (g_frame && FrameArena() - g_frame > sizeof(g_held_frame)) return;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		const uint32_t arena = FrameArena();
		// the private frame arena starts as the real one from where the tick's draws started, so the bytes a packet
		// does not write (pad halves) are the real ones
		const uint32_t base = g_frame ? g_frame : arena;
		const uint32_t limit = var<uint32_t>(0x1D969A8);
		uint32_t fill = limit > base ? limit - base : 0;
		if (fill > sizeof(g_held_frame)) fill = sizeof(g_held_frame);
		memcpy(g_held_frame, (const void *)base, fill);
		FrameArena() = (uint32_t)g_held_frame;
		// the effect camera of the frame's camera (as every real tick builds it before drawing)
		const Mat4x3 effcam = EffCam();
		EffectCameraMatrix(&Camera(), &EffCam());
		// GTE data registers 28..31 are not in GTE_DATA
		uint8_t gte_hi[0x10];
		memcpy(gte_hi, (const void *)(GTE_DATA + GTE_DATA_SIZE), sizeof(gte_hi));
		// what the model draws write outside the buffers
		const uint32_t last_packet = var<uint32_t>(0x1D99C18);
		const uint32_t ws = var<uint32_t>(0x1D98B3C);
		uint32_t ws_bytes = 0;
		if (ws)
		{
			for (int i = 0; i < g_nrec; i++)
			{
				if (g_recs[i].kind != K_MODEL) continue;
				const uint8_t *e = g_recs[i].entity;
				uint32_t b = WorkspaceBytes(*(const uint32_t *)(e + 0x64));
				const uint32_t w2 = *(const uint32_t *)(e + 0x78);
				if (w2)
				{
					const uint32_t b2 = WorkspaceBytes(*(const uint32_t *)(w2 + 4));
					if (b2 > b) b = b2;
				}
				if (b > ws_bytes) ws_bytes = b;
			}
			if (ws_bytes > sizeof(g_ws_save)) ws_bytes = sizeof(g_ws_save);
			if (ws_bytes) memcpy(g_ws_save, (const void *)ws, ws_bytes);
		}
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			// a replayed draw starts from the GTE state and the Field_Alloc scratch of the real one
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(FieldScratch(), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case K_TILE: HeldTile(r, num, den); break;
			case K_MODEL: HeldModel(r, num, den); break;
			case K_SPARKS: HeldSparks(num, den); break;
			case K_SHARDS: HeldShards(num, den); break;
			default: HeldPlay(r, num, den); break;
			}
		}
		if (ws_bytes) memcpy((void *)ws, g_ws_save, ws_bytes);
		var<uint32_t>(0x1D99C18) = last_packet;
		memcpy((void *)(GTE_DATA + GTE_DATA_SIZE), gte_hi, sizeof(gte_hi));
		EffCam() = effcam;
		FrameArena() = arena;
	}

	// ------------------------------------------------------------------
	// held-frame camera
	// ------------------------------------------------------------------
	// counters on which the master writes the camera words
	static bool CameraWritten(int32_t c)
	{
		return c == 0 || (uint32_t)(c - 0x14) < 0x27 || c == 0x3B || (uint32_t)(c - 0x4F) < 0x28 || (uint32_t)(c - 0x77) < 0x3C || c == 0xD5 || c >= 0x106;
	}

	// direct sets, the second segment's start and the return to the saved view: the camera jumps
	static bool CameraCut(int32_t c)
	{
		return c == 0 || c == 0x3B || c == 0x4F || c == 0xD5 || c >= 0x106;
	}

	inline int32_t AbsI(int32_t v) { return v < 0 ? -v : v; }

	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (!HeldReady() || !MasterLive()) return false;
		const int32_t c = g_master->counter; // the counter the next tick runs
		if (c == g_master_counter || !CameraWritten(c)) return false;
		int16_t *eye = CamEye(), *at = CamLookAt();
		int16_t e0[3], a0[3];
		for (int k = 0; k < 3; k++) { e0[k] = eye[k]; a0[k] = at[k]; }
		bool cut = CameraCut(c);
		int16_t e1[3], a1[3];
		if (!cut)
		{
			// the master's camera code for the next counter on the saved words (the scratch above the Field_Alloc
			// pointer, the GTE, the words, field of view and roll put back)
			static uint8_t cam_save[0x10], fa_save[0x1000];
			static uint8_t gte_save[GTE_DATA_SIZE + GTE_CTRL_SIZE];
			uint8_t *fa = var<uint8_t *>(0x1D999C4);
			if (fa) memcpy(fa_save, fa, sizeof(fa_save));
			memcpy(cam_save, (const void *)0xB8B7F0, sizeof(cam_save));
			const int16_t view = ViewWord(), zoom = CameraZoom();
			memcpy(gte_save, (const void *)GTE_DATA, GTE_DATA_SIZE);
			memcpy(gte_save + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
			MasterCamera(c, false);
			for (int k = 0; k < 3; k++) { e1[k] = eye[k]; a1[k] = at[k]; }
			memcpy((void *)GTE_DATA, gte_save, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, gte_save + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			CameraZoom() = zoom;
			ViewWord() = view;
			memcpy((void *)0xB8B7F0, cam_save, sizeof(cam_save));
			var<uint8_t *>(0x1D999C4) = fa;
			if (fa) memcpy(fa, fa_save, sizeof(fa_save));
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

	static void register_mag260_held()
	{
		using namespace pulsar260;
		mark_held(ORIG_MasterTask);
		mark_held(ORIG_LiftTask);
		mark_held(ORIG_RiseTask);
		mark_held(ORIG_RingTask);
		register_module_held(260, HeldReady, HeldFrame);
		register_module_camera(260, HeldCamera);
	}
}
