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

// 30 fps part of gfc_engine.cpp: the held frames of the GF cinematic engine (state of the real
// tick, prediction of the next tick, in-between draw) and the functions called by the
// FX_HELD(...) statements of gfc_engine.cpp (included at its end, declarations in
// gfc_engine_held.h)

namespace ff8fx
{
namespace gfc
{
	// ============================================================================================
	// held frames: state of the real tick, prediction of the next one, in-between draw
	// ============================================================================================
	bool g_predict = false;
	Held g_held = { false, 0, 1, 0 };

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	// Workspace + RuntimeSlot of the last real tick, copied at its end: held frames and the
	// prediction run on this private copy (the live scratch-stack block may be reused by other
	// battle code between two ticks; vanilla's next tick gets it back at the same address)
	alignas(16) static uint8_t g_scratch_copy[0x180];
	static uint8_t *g_scratch = nullptr;

	// --- journal of out-of-snapshot writes while predicting ---
	struct JournalEntry { uint8_t *p; int n; uint32_t off; };
	static JournalEntry g_jr[4096];
	static uint8_t g_jr_bytes[0x40000];
	static int g_njr = 0;
	static uint32_t g_jr_used = 0;
	static bool g_jr_overflow = false;
	static bool g_cam_written = false;

	void guard_record(const void *p, int n)
	{
		if ((uint32_t)p < 0xB8B800 && (uint32_t)p + n > 0xB8B7F0) g_cam_written = true;
		if (g_njr >= 4096 || g_jr_used + n > sizeof(g_jr_bytes) || n <= 0) { g_jr_overflow = true; return; }
		g_jr[g_njr] = { (uint8_t *)p, n, g_jr_used };
		memcpy(g_jr_bytes + g_jr_used, p, n);
		g_jr_used += n;
		g_njr++;
	}

	static void journal_restore()
	{
		for (int i = g_njr - 1; i >= 0; i--) memcpy(g_jr[i].p, g_jr_bytes + g_jr[i].off, g_jr[i].n);
		g_njr = 0;
		g_jr_used = 0;
	}

	// --- saved regions ---
	struct Region { uint8_t *p; uint32_t n; uint8_t *copy; uint32_t cap; };
	static void region_save(Region &r, void *p, uint32_t n)
	{
		if (n > r.cap)
		{
			delete[] r.copy;
			r.cap = (n + 0xFFF) & ~0xFFFu;
			r.copy = new uint8_t[r.cap];
		}
		r.p = (uint8_t *)p;
		r.n = n;
		if (n) memcpy(r.copy, p, n);
	}
	static void region_restore(const Region &r) { if (r.n) memcpy(r.p, r.copy, r.n); }

	static Region g_r_state, g_r_bones, g_r_arena, g_r_scratch, g_r_gte, g_r_free;

	// engine renderers called by the draw (BuildBoneMatricesFromPose, RenderGeometry...) take
	// temporaries from the battle scratch stack (Field_Alloc 0x5082B0, pointer 0x1D999C4):
	// the free area above the pointer is saved and restored around held draws / predictions
	static const uint32_t FREE_SCRATCH = 0x8000;
	static void SaveFreeScratch() { region_save(g_r_free, (void *)MEM<uint32_t>(0x1D999C4), FREE_SCRATCH); }

	static uint32_t *CrtSeed() { return (uint32_t *)(x::f<uint8_t *(__cdecl *)()>(0x560578)() + 0x14); }

	// bones that may be live: SceneHeader+0x38 is only the initial allocation count, SpawnBone
	// (VM 0x032 ...) takes the first bone whose stream word is 0 without bound, so bones past it
	// are used by the clone scripts: the highest index on the order / draw lists, plus room for the
	// bones a (predicted) tick can spawn
	static uint32_t ActiveBones(const uint8_t *scene)
	{
		uint32_t n = U16(scene, 0x38);
		uint32_t hi = 0;
		for (int k = 0; k < 256 && ORDER()[k] != 0xFF; k++) if ((ORDER()[k] & 0x7Fu) + 1 > hi) hi = (ORDER()[k] & 0x7Fu) + 1;
		for (int k = 0; k < 0xD0 && DRAWORDER()[k] != 0xFF; k++) if (DRAWORDER()[k] + 1u > hi) hi = DRAWORDER()[k] + 1u;
		hi += 8;
		if (hi > n) n = hi;
		return n > 128 ? 128 : n;
	}
	static uint32_t BoneCount() { return ActiveBones(SCENE()); }

	// the globals a context bind sets, without allocating (the scratch block of the real tick)
	static void BindHeld()
	{
		uint8_t *a = g_scratch;
		uint8_t *rt = a + 0x100;
		PTR(rt, 0) = a;
		PTR(rt, 4) = rt;
		const uint32_t *pb = (const uint32_t *)C().ptr_block;
		WS() = a;
		RT() = rt;
		CTX() = (uint8_t *)pb[0];
		PTR3() = (uint8_t *)pb[1];
		SCENE() = (uint8_t *)pb[2];
		RCTX() = (uint8_t *)pb[3];
		SEQ() = (uint8_t *)pb[4];
	}

	static void SaveEngine(bool arena)
	{
		region_save(g_r_state, (void *)STATE_LO, STATE_HI - STATE_LO);
		region_save(g_r_scratch, g_scratch, 0x180);
		const uint32_t *pb = (const uint32_t *)C().ptr_block;
		uint8_t *ctx = (uint8_t *)pb[0], *scene = (uint8_t *)pb[2];
		// all 128 bone slots (32 KB): scripts initialise / spawn / end bones past the live range
		(void)scene;
		region_save(g_r_bones, PTR(ctx, 0x90), 128 * 0x100);
		if (arena)
		{
			uint32_t lo = U32(ctx, 0x70), hi = U32(ctx, 0x74);
			region_save(g_r_arena, (void *)lo, hi > lo && hi - lo < 0x400000 ? hi - lo : 0);
		}
	}
	static void RestoreEngine(bool arena)
	{
		if (arena) region_restore(g_r_arena);
		region_restore(g_r_bones);
		region_restore(g_r_scratch);
		region_restore(g_r_state);
	}

	// --- the predicted next tick ---
	struct PredBone { uint16_t id16; uint8_t alive, draw; int16_t ang[3], pos[3]; uint32_t colour; };
	struct Pred
	{
		uint32_t tick;
		bool ok;
		bool paused;
		uint32_t nb;
		PredBone b[128];
		Mat4x3 node[64];
		uint16_t keys[64];
		uint8_t lights[5 * 0x50];
		Mat4x3 bb[4];
		bool cam;
		int16_t eye[3], at[3];
	};
	static Pred g_pred = { 0xFFFFFFFF, false };

	// embedded-model draw handlers of a clone besides 3 (held_model_draws)
	struct ModelDraws { const Clone *c; uint8_t d[4]; };
	static ModelDraws g_model_draws[16];
	static int g_nmodel_draws = 0;

	void held_model_draws(Clone &c, uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
	{
		if (g_nmodel_draws < 16) g_model_draws[g_nmodel_draws++] = { &c, { d0, d1, d2, d3 } };
	}

	static const uint8_t *model_draws_of(const Clone &c)
	{
		static const uint8_t none[4] = { 0, 0, 0, 0 };
		for (int i = 0; i < g_nmodel_draws; i++)
			if (g_model_draws[i].c == &c) return g_model_draws[i].d;
		return none;
	}

	static void CaptureBones(PredBone *out, uint32_t nb)
	{
		uint8_t *bones = PTR(CTX(), 0x90);
		for (uint32_t i = 0; i < nb; i++)
		{
			uint8_t *b = bones + 0x100 * i;
			PredBone &p = out[i];
			p.id16 = U16(b, 0x12);
			p.alive = 0;
			p.draw = U8(b, 0x1C);
			for (int k = 0; k < 3; k++) { p.ang[k] = S16(b, 0x8C + 2 * k); p.pos[k] = S16(b, 0x94 + 2 * k); }
			p.colour = U32(b, 0xCC);
		}
		for (int k = 0; k < 256 && ORDER()[k] != 0xFF; k++)
		{
			uint32_t i = ORDER()[k] & 0x7F;
			if (i < nb) out[i].alive = 1;
		}
	}

	// runs the VM part of the next tick (preamble + Neg/Integrator/Pos) on the real state with
	// every out-of-state side effect disabled, captures the result and puts everything back
	static void Predict(Clone &c)
	{
		if (g_pred.tick == g_real_tick) return;
		g_pred.tick = g_real_tick;
		g_pred.ok = false;
		if (!g_scratch) return;
		region_save(g_r_gte, (void *)0x1CA8A00, 0xA00);
		SaveFreeScratch();
		uint32_t seed = *CrtSeed();
		uint8_t *saved_globals[8] = { RT(), WS(), CTX(), SCENE(), RCTX(), SEQ(), CUR(), STREAM() };
		uint8_t *saved_ptr3 = PTR3();
		int16_t cam0[8];
		memcpy(cam0, (void *)0xB8B7F0, 16);
		int16_t projh = PROJH();
		BindHeld();
		SaveEngine(true);
		g_predict = true;
		g_njr = 0; g_jr_used = 0; g_jr_overflow = false; g_cam_written = false;
		guard((void *)0xB8B7F0, 0x20);  // camera words + saved copy (op 0x39 sub-op 4)
		guard((void *)0x1D8E038, 2);
		guard((void *)0x1D9771C, 2);
		guard((void *)0x1D977A0, 4);
		g_cam_written = false;

		// the next tick's preamble (0xB25DF0), without the debug pad / OT clear
		S16(CTX(), 0x32) = (int16_t)(S16(CTX(), 0x32) + 1);
		U8(RT(), 0x41) = U8(CTX(), 0x32) & 1;
		x::Rand();
		if (U16(CTX(), 0) & 0x400)
		{
			U32(RCTX(), 0x2C) = FRAMERL() + 0xC;
			U16(RCTX(), 0x24) = 0;
		}
		U8(CTX(), 0x35) = (UPDFLAGS() & 1) ? 0xFF : 0;
		U8(RT(), 0x40) = U8(RT(), 0x41);
		U32(RT(), 0x38) = FRAMERL() + 0x44;
		{
			const uint32_t *v = (const uint32_t *)0x1D97778;
			uint32_t *m0 = (uint32_t *)NODEMAT(), *ma = (uint32_t *)CAMALT();
			for (int k = 0; k < 8; k++) { m0[k] = v[k]; ma[k] = v[k]; }
			m0[4] &= 0xFFFF;
			ma[4] &= 0xFFFF;
		}
		U8(CTX(), 0xD2) = 0;
		g_pred.paused = U8(CTX(), 0x35) != 0;
		if (!g_pred.paused)
		{
			AnimChannelsNeg();
			AnimIntegrator();
			AnimChannelsPos();
		}
		g_pred.nb = BoneCount();
		CaptureBones(g_pred.b, g_pred.nb);
		memcpy(g_pred.node, NODEMAT(), sizeof(g_pred.node));
		memcpy(g_pred.keys, NODEKEYS(), sizeof(g_pred.keys));
		memcpy(g_pred.lights, LIGHTS(), sizeof(g_pred.lights));
		memcpy(g_pred.bb, BBMAT(), sizeof(g_pred.bb));
		g_pred.cam = g_cam_written;
		memcpy(g_pred.eye, (void *)0xB8B7F0, 6);
		memcpy(g_pred.at, (void *)0xB8B7F8, 6);
		g_pred.ok = !g_jr_overflow;

		g_predict = false;
		journal_restore();
		RestoreEngine(true);
		memcpy((void *)0xB8B7F0, cam0, 16);
		PROJH() = projh;
		RT() = saved_globals[0]; WS() = saved_globals[1]; CTX() = saved_globals[2]; SCENE() = saved_globals[3];
		RCTX() = saved_globals[4]; SEQ() = saved_globals[5]; CUR() = saved_globals[6]; STREAM() = saved_globals[7];
		PTR3() = saved_ptr3;
		*CrtSeed() = seed;
		region_restore(g_r_free);
		region_restore(g_r_gte);
	}

	// in-between value, holding on large jumps (teleports, re-spawns)
	static inline int16_t mix16(int16_t a, int16_t b, int num, int den, int32_t cut)
	{
		int32_t d = (int16_t)(b - a);
		if (d > cut || d < -cut) return a;
		return (int16_t)(a + d * num / den);
	}
	static inline int32_t mix32(int32_t a, int32_t b, int num, int den, int32_t cut)
	{
		int64_t d = (int64_t)b - a;
		if (d > cut || d < -cut) return a;
		return (int32_t)(a + d * num / den);
	}

	static const int32_t CUT_POS = 0x2000;   // outPos / translation jump treated as a cut
	static const int32_t CUT_ANGLE = 0x7FFF; // outAngle (int16 wrap is the shortest way)
	static const int32_t CUT_MAT = 0x7FFF;

	static void MixMatrix(Mat4x3 &m, const Mat4x3 &n, int num, int den)
	{
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++) m.m[r][c] = mix16(m.m[r][c], n.m[r][c], num, den, CUT_MAT);
		m.pad = mix16(m.pad, n.pad, num, den, 0x7FFF);
		for (int k = 0; k < 3; k++) m.t[k] = mix32(m.t[k], n.t[k], num, den, 0x100000);
	}

	// writes the in-between state into the (saved) engine state
	static void ApplyInBetween(int num, int den)
	{
		uint32_t nb = BoneCount();
		if (nb > g_pred.nb) nb = g_pred.nb;
		uint8_t *bones = PTR(CTX(), 0x90);
		static uint8_t alive[128];
		memset(alive, 0, sizeof(alive));
		for (int k = 0; k < 256 && ORDER()[k] != 0xFF; k++) alive[ORDER()[k] & 0x7F] = 1;
		for (uint32_t i = 0; i < nb; i++)
		{
			uint8_t *b = bones + 0x100 * i;
			const PredBone &p = g_pred.b[i];
			if (!alive[i] || !p.alive || p.id16 != U16(b, 0x12) || p.draw != U8(b, 0x1C)) continue;
			for (int k = 0; k < 3; k++)
			{
				S16(b, 0x8C + 2 * k) = mix16(S16(b, 0x8C + 2 * k), p.ang[k], num, den, CUT_ANGLE);
				S16(b, 0x94 + 2 * k) = mix16(S16(b, 0x94 + 2 * k), p.pos[k], num, den, CUT_POS);
			}
			uint32_t c0 = U32(b, 0xCC), c1 = p.colour;
			if ((c0 & 0xFF000000) == (c1 & 0xFF000000))
			{
				uint32_t c = c0 & 0xFF000000;
				for (int s = 0; s < 24; s += 8)
				{
					int32_t a = (c0 >> s) & 0xFF, bb = (c1 >> s) & 0xFF;
					c |= (uint32_t)((a + (bb - a) * num / den) & 0xFF) << s;
				}
				U32(b, 0xCC) = c;
			}
		}
		// matrix nodes: same key on both ticks (slot 0 = camera matrix, always)
		for (int k = 0; k < 64; k++)
			if (k == 0 || (NODEKEYS()[k] != 0 && NODEKEYS()[k] == g_pred.keys[k]))
				MixMatrix(NODEMAT()[k], g_pred.node[k], num, den);
		// light sets (5 slots fit before the billboard matrices) and billboard matrices
		for (int s = 0; s < 5; s++)
		{
			uint8_t *l = LIGHTS() + 0x50 * s;
			const uint8_t *n = g_pred.lights + 0x50 * s;
			for (int o = 0; o < 0x40; o += 2) S16(l, o) = mix16(S16(l, o), AT<int16_t>(n, o), num, den, 0x7FFF);
			for (int o = 0x40; o < 0x4C; o += 4) S32(l, o) = mix32(S32(l, o), AT<int32_t>(n, o), num, den, 0x7FFFFFFF);
		}
		for (int k = 0; k < 4; k++) MixMatrix(BBMAT()[k], g_pred.bb[k], num, den);
	}

	bool HeldReady(Clone &c)
	{
		(void)c;
		return g_ported_tick == g_real_tick && g_scratch != nullptr;
	}

	// private packet memory of the held draw: main cursor ctx+0x7C, alternate pool ctx+0xD8 and
	// the battle frame arena cursor (for engine renderers that use it directly)
	static uint8_t g_held_packets[0x100000];

	void HeldFrame(Clone &c, int num, int den)
	{
		if (!HeldReady(c) || den <= 0) return;
		g_clone = &c;
		Predict(c);
		uint8_t *saved_globals[8] = { RT(), WS(), CTX(), SCENE(), RCTX(), SEQ(), CUR(), STREAM() };
		uint8_t *saved_ptr3 = PTR3();
		uint32_t pktcur = PKTCUR();
		// the mesh renderer leaves its depth scale here and a later mesh may read it: keep the
		// real tick's value for the next real tick
		float depth_scale = FLT_1877DA8();
		BindHeld();
		// the arena holds the state blocks of the clone-specific draw handlers (ribbons, starfield,
		// melt columns, ...): saved with the engine so that nothing a held draw writes persists
		SaveEngine(true);
		uint32_t seed = *CrtSeed();
		// particle pools of draw handler 6 (live in the scene data, written by type 4 even when frozen)
		static Region pools[32];
		int npools = 0;
		for (int k = 0; DRAWORDER()[k] != 0xFF && npools < 32; k++)
		{
			uint8_t *b = PTR(CTX(), 0x90) + 0x100 * (uint32_t)DRAWORDER()[k];
			if (U8(b, 0x1C) == 6 && PTR(b, 0xB8))
				region_save(pools[npools++], PTR(b, 0xB8), 16 + 80 * (uint32_t)U16(PTR(b, 0xB8), 0));
		}
		// skeletons of the embedded battle models (draw handler 3 and the clone's other model
		// handlers, held_model_draws): the held draw rebuilds their bone matrices at the
		// in-between pose
		static Region skels[8];
		int nskels = 0;
		const uint8_t *model_draw = model_draws_of(c);
		for (int k = 0; DRAWORDER()[k] != 0xFF && nskels < 8; k++)
		{
			uint8_t *b = PTR(CTX(), 0x90) + 0x100 * (uint32_t)DRAWORDER()[k];
			uint8_t d = U8(b, 0x1C);
			bool model = d == 3;
			for (int m = 0; m < 4; m++) model |= model_draw[m] != 0 && model_draw[m] == d;
			uint8_t *blk = model ? PTR(b, 0xBC) : nullptr;
			uint8_t *sk = blk && PTR(blk, 0x14) ? PTR(PTR(blk, 0x14), 0) : nullptr;
			if (sk) region_save(skels[nskels++], sk, 0x10 + 0x30 * (uint32_t)U8(sk, 0));
		}
		SaveFreeScratch();
		if (g_pred.ok && !g_pred.paused) ApplyInBetween(num, den);

		// the tick's draw preamble, packets into a private buffer
		if (U16(CTX(), 0) & 0x400)
		{
			U32(RCTX(), 0x2C) = FRAMERL() + 0xC;
			U16(RCTX(), 0x24) = 0;
		}
		U32(RT(), 0x38) = FRAMERL() + 0x44;
		U32(CTX(), 0x7C) = (uint32_t)g_held_packets;
		U32(CTX(), 0xD8) = (uint32_t)g_held_packets + 0xA0000;
		PKTCUR() = (uint32_t)g_held_packets + 0xE0000;
		U16(RCTX(), 4) = 0;
		U16(RT(), 0x46) = 0;
		g_held = { true, num, den, U8(RT(), 0x45) };
		U8(RT(), 0x45) = 0xFF;
		BuildMatricesAndDraw();
		x::ParseCamera2(PROJH());
		g_held.active = false;

		for (int i = 0; i < npools; i++) region_restore(pools[i]);
		for (int i = nskels - 1; i >= 0; i--) region_restore(skels[i]);
		region_restore(g_r_free);
		RestoreEngine(true);
		*CrtSeed() = seed;
		FLT_1877DA8() = depth_scale;
		PKTCUR() = pktcur;
		RT() = saved_globals[0]; WS() = saved_globals[1]; CTX() = saved_globals[2]; SCENE() = saved_globals[3];
		RCTX() = saved_globals[4]; SEQ() = saved_globals[5]; CUR() = saved_globals[6]; STREAM() = saved_globals[7];
		PTR3() = saved_ptr3;
	}

	bool HeldCamera(Clone &c, int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (!HeldReady(c) || den <= 0) return false;
		g_clone = &c;
		Predict(c);
		if (!g_pred.ok || g_pred.paused || !g_pred.cam) return false;
		const int16_t *e0 = CAMEYE(), *a0 = CAMAT();
		bool cut = false;
		for (int k = 0; k < 3; k++)
		{
			int32_t de = (int32_t)g_pred.eye[k] - e0[k], da = (int32_t)g_pred.at[k] - a0[k];
			if (de > 1500 || de < -1500 || da > 1500 || da < -1500) cut = true;
		}
		for (int k = 0; k < 3; k++)
		{
			world[k] = cut ? e0[k] : (int16_t)lerp_i(e0[k], g_pred.eye[k], num, den);
			lookat[k] = cut ? a0[k] : (int16_t)lerp_i(a0[k], g_pred.at[k], num, den);
		}
		return true;
	}

	// ------------------------------------------------------------------------------------
	// SequenceTick (0xB25DF0)
	// ------------------------------------------------------------------------------------
	// the ports ran on this real tick
	void held_note_tick() { g_ported_tick = g_real_tick; }

	// Workspace + RuntimeSlot of the tick, copied at its end (held frames and the prediction
	// run on this copy)
	void held_note_scratch()
	{
		memcpy(g_scratch_copy, WS(), sizeof(g_scratch_copy));
		g_scratch = g_scratch_copy;
	}

	// ------------------------------------------------------------------------------------
	// 0xB26AD0 StepEmbeddedModelAnim: held frames (rt+0x45 forced to 0xFF): when the real tick
	// would have advanced the animation, the bone matrices come from the exact in-between pose
	// (pose_midpoint) instead of BuildBoneMatricesFromPose.
	// ------------------------------------------------------------------------------------
	bool held_model_pose(uint8_t *blk, uint8_t *hdr, uint8_t *cmd)
	{
		uint16_t fl = U16(blk, 2);
		if (!(g_held.active && g_held.real_skip == 0 && !(fl & 1) && U16(blk, 0) != 0 && !(fl & 2)))
			return false;
		pose_midpoint(hdr, cmd, g_held.num, g_held.den);
		return true;
	}

	// ------------------------------------------------------------------------------------
	// 0xB29F20 (Draw 6) particle system, held frames (g_held, frozen path forced): the particle
	// is drawn at pos + the next integration step's position delta * num/den, on a local copy
	// (the pool is not written). Called for a live particle whose frame (+0x30) is set, in place
	// of the frozen path's position type + draw.
	// ------------------------------------------------------------------------------------
	void held_particle_draw(uint8_t *p)
	{
		const bool held_adv = g_held.real_skip == 0 && g_held.den > 0;
		uint8_t *q = p;     // record the draw reads (a local copy when advanced)
		uint8_t loc[0x50];
		if (held_adv)
		{
			// position = pos + next tick's integration delta ((vel + acc) << 4, the
			// deterministic part) * num / den
			memcpy(loc, p, 0x50);
			for (int k = 0; k < 4; k++)
			{
				int32_t s = S16(p, 0x28 + 2 * k) + S16(p, 0x20 + 2 * k);
				int32_t d = (int32_t)((int64_t)shl32(s, 4) * g_held.num / g_held.den);
				S32(loc, 0x10 + 4 * k) = add32(S32(p, 0x10 + 4 * k), d);
			}
			q = loc;
			PTR(WS(), 0x44) = loc;
			C().ptype[S8(q, 0x37)]();
			PTR(WS(), 0x44) = p;
		}
		else
			C().ptype[S8(p, 0x37)]();
		if (U32(q, 0x30) == 0) return;
		// --- draw (as dh_06_ParticleSystem, from q) ---
		{
			uint8_t *m = PTR(WS(), 0x30);
			x::SetRotMatrix(m);
			x::SetTransVector(m);
			x::GteLoadV0(WS() + 0xA0);
			x::GteMVMVA_RotV0Tr();
			x::Gte_45E580();
			int32_t sc = S32(q, 0x10) >> 4;
			S32(WS(), 0xA8) = sc;
			S32(WS(), 0xA0) = sc;
			S32(WS(), 0xB0) = 0;
			S32(WS(), 0xAC) = 0;
			S32(WS(), 0xA4) = 0;
			x::SetRotMatrix(WS() + 0xA0);
			if (S32(WS(), 0x40) != 0) x::Matrix_56C2C0(BBMAT() + 1);
			U32(WS(), 0x90) = U32(q, 0x30);
			U32(WS(), 0x94) = U16(q, 0x40);
			U32(WS(), 0x98) = U32(q, 0x38);
			S32(WS(), 0x9C) = S16(q, 0x48);
			part_draw_sprite::EmitSpriteQuads();
		}
	}

	// ------------------------------------------------------------------------------------
	// 0xB1A130 (Draw 37 RibbonTrail), held frames (g_held, rt+0x45 forced to 0xFF): the frozen
	// path runs, so the ring, the node position and bone+0x4A never advance: the ribbon drawn is
	// the real tick's history (colour bone and parent matrix are the in-between values). Two
	// held-only additions:
	//   * VXY0 before SetupParentXform is set to the value the real tick had there (vanilla
	//     leaves V0.xy of the previous GTE user in the translation; in a held draw that is
	//     another handler's frozen-path value, which would shift the whole ribbon);
	//   * the arena scratch above ctx+0x74 (outside the saved arena) written by the draw is
	//     restored before returning.
	// The first-draw path cannot run on a held frame (a node's first draw is on the tick that
	// created it); if it did, everything it writes (arena, ctx+0x74, bone) is in the saved set.
	// ------------------------------------------------------------------------------------
	// software GTE data register 0 (VXY0, packed)
	static inline uint32_t &GTE_VXY0() { return MEM<uint32_t>(0x1CA8A10); }
	// VXY0 the real tick had when it reached SetupParentXform (see B1ADD0), per node
	static NodeMemo<uint32_t, 64> g_v0memo;
	// the arena scratch above ctx+0x74 this handler writes (projections + colour table)
	static uint8_t *g_hsave = nullptr;
	static uint32_t g_hsave_cap = 0;
	static uint8_t *g_hs_p = nullptr;
	static uint32_t g_hs_n = 0;

	// held: save the scratch above the arena top this draw writes (projection buffers
	// [top, top+0xC6) and the colour table at top+0x100)
	void held_ribbon_save(uint8_t *edi)
	{
		g_hs_p = nullptr;
		g_hs_n = 0;
		if (!g_held.active) return;
		uint32_t f0 = U16(edi, 0xA), f4 = U16(edi, 0xC), f8 = U16(edi, 2);
		uint32_t ents = f0 + (f4 > f0 ? f4 - f0 : 0) + (f8 > f4 ? f8 - f4 : 0) + f8;
		g_hs_p = PTR(CTX(), 0x74);
		g_hs_n = 0x100 + ents * 8;
		if (g_hs_n > g_hsave_cap)
		{
			delete[] g_hsave;
			g_hsave_cap = (g_hs_n + 0xFFF) & ~0xFFFu;
			g_hsave = new uint8_t[g_hsave_cap];
		}
		memcpy(g_hsave, g_hs_p, g_hs_n);
	}

	// real tick: memo of VXY0 before SetupParentXform; held: VXY0 of the real tick put back
	void held_ribbon_v0()
	{
		if (!g_held.active)
		{
			uint32_t *m = g_v0memo.put(CUR());
			if (m) *m = GTE_VXY0();
		}
		else
		{
			const uint32_t *m = g_v0memo.get(CUR());
			if (m) GTE_VXY0() = *m;
		}
	}

	void held_ribbon_restore()
	{
		if (g_hs_n) memcpy(g_hs_p, g_hsave, g_hs_n);
	}

	// ------------------------------------------------------------------------------------
	// 0xB0E930 (Draw 26 ScreenCaptureStrips), held frames: never the one-shot capture
	// (bone+0xC4 == 0 -> nothing), and nothing on the held frames that follow the real tick
	// whose draw was the capture (vanilla drew no strips on that tick); otherwise the strips are
	// redrawn (pure packets, in-between outPos/colour).
	// ------------------------------------------------------------------------------------
	// the bone whose draw 26 did the capture on the last real draw (its first call): the held
	// frames that follow draw nothing for it (vanilla drew no strips on that tick)
	static const void *g_cap_bone = nullptr;
	static uint32_t g_cap_tick = 0xFFFFFFFF;

	void held_note_capture(uint8_t *b)
	{
		g_cap_bone = b;
		g_cap_tick = g_real_tick;
	}

	// held: true = draw nothing; real tick: forget the capture once the bone draws strips
	bool held_capture_skip(uint8_t *b)
	{
		if (g_held.active)
			return U32(b, 0xC4) == 0 || (g_cap_bone == b && g_cap_tick == g_real_tick);
		if (U32(b, 0xC4) != 0 && g_cap_bone == b)
			g_cap_bone = nullptr;
		return false;
	}

	// ------------------------------------------------------------------------------------
	// 0xB1E3F0 (Draw 39 Starfield), held frames: bone+0xBC == 0 -> nothing (no allocation, no
	// rand); else no ++counter, no timer, no rand: the stars the real tick drew (memo) are
	// re-projected with the in-between parent matrix / outPos (screen coordinates in a private
	// buffer) and redrawn.
	// ------------------------------------------------------------------------------------
	// per-block memo of the stars drawn by the last real draw of Draw 39 (held frames redraw
	// exactly those)
	struct StarMemo { const uint8_t *blk; uint32_t tick; uint8_t bits[0x1000]; };
	static StarMemo g_star[4];
	static uint8_t g_held_scr[0x8000 * 8];   // held frames: screen coordinates (vanilla: ctx+0x74)
	static StarMemo *g_star_cur = nullptr;   // memo of the real draw in progress

	static StarMemo *star_memo_put(const uint8_t *blk)
	{
		for (StarMemo &m : g_star)
			if (m.blk == blk) { m.tick = g_real_tick; memset(m.bits, 0, sizeof(m.bits)); return &m; }
		for (StarMemo &m : g_star)
			if (m.tick != g_real_tick || m.blk == nullptr) { m.blk = blk; m.tick = g_real_tick; memset(m.bits, 0, sizeof(m.bits)); return &m; }
		return nullptr;
	}
	static const StarMemo *star_memo_get(const uint8_t *blk)
	{
		for (const StarMemo &m : g_star)
			if (m.blk == blk && m.tick == g_real_tick) return &m;
		return nullptr;
	}

	void held_star_begin(const uint8_t *blk) { g_star_cur = star_memo_put(blk); }

	void held_star_drawn(int32_t i)
	{
		if (g_star_cur && i < 0x8000) g_star_cur->bits[i >> 3] |= (uint8_t)(1u << (i & 7));
	}

	void dh_39_StarfieldHeld()
	{
		if (U32(CUR(), 0xBC) == 0) return;
		uint8_t *blk = PTR(CUR(), 0xBC);                                     // ebx, [ebp-0x1C]
		uint8_t *stars = blk + 4;                                            // edi
		PTR(WS(), 0x60) = stars;
		uint32_t otw = U16(CUR(), 0xC0);
		if (otw == 0)
			U32(WS(), 0x64) = 0;
		else
			U32(WS(), 0x64) = (otw & 0x3FFC) + U32(RT(), 0x4C);
		h_B27000();
		int32_t n = S16(blk, 2);
		uint8_t *scr0 = g_held_scr;                                          // esi
		if (n > 0x8000) n = 0x8000;
		if (n > 0)
		{
			uint8_t *s = scr0;
			uint8_t *v = stars;
			int32_t k = n;
			do
			{
				x::GteLoadV0(v);
				x::GteRTPS();
				x::GteReadSXY2(s);
				x::GteReadOTZdiv4(s + 4);
				v += 0x10;
				s += 8;
			} while (--k != 0);
		}
		uint8_t *sc = g_held_scr;                                            // [ebp-4]
		uint8_t *pkt = PTR(CTX(), 0x7C);                                     // edi
		uint8_t *e = PTR(WS(), 0x60);                                        // esi
		n = S16(blk, 2);
		if (n > 0x8000) n = 0x8000;
		const StarMemo *mg = star_memo_get(blk);
		if (n > 0)
		{
			e += 0xC;
			for (int32_t i = 0; i < n; i++)
			{
				// the real tick's decision (fallback without memo: the timer state)
				bool draw;
				if (mg) draw = ((mg->bits[i >> 3] >> (i & 7)) & 1) != 0;
				else draw = !(U8(e, -6) & 1) || U8(e, 0) != 0;
				if (draw)
				{
					U8(pkt, 3) = 2;
					U32(pkt, 4) = U32(e, -4);
					U32(pkt, 8) = U32(sc, 0);
					uint32_t ot = U32(WS(), 0x64);
					if (ot == 0) ot = (uint32_t)((int32_t)S16(sc, 4) & ~3) + U32(RT(), 0x4C);
					part_shared_misc::xl_InsertPrimAutoDepth(ot, pkt);
					pkt += 0xC;
				}
				sc += 8;
				e += 0x10;
			}
		}
		PTR(CTX(), 0x7C) = pkt;
	}
}
}
