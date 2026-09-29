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

#include <math.h>
#include <vector>
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
	// Draw 37 ribbons (held_ribbon_v0 / held_ribbon_restore): real tick: VXY0 at the end of the
	// last ribbon draw; held frame: the value the last ribbon drawn will leave there on the next
	// tick (ok = known)
	static uint32_t g_rib_last_v0 = 0, g_rib_next_v0 = 0;
	static bool g_rib_last_ok = false, g_rib_next_ok = false;
	// battle camera shake offsets (0x1D97710, 3 x s16) as the last real tick left them
	static int16_t g_shake[3] = {};
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

	// the draw's scratch memory outside the saved engine state: transformed vertices above the
	// arena top (ctx+0x74: DrawMeshObject, RenderGeometry, particles) and the per-vertex depth
	// array dword_2798C18 (past STATE_HI). Vanilla rewrites both before reading them; a held draw
	// must still leave them as the real tick left them. (The arena lives in the magic buffer in
	// FF8_EN.exe's .data 0xB6D000..0x279F000; before the script sets it, ctx+0x74 still holds the
	// PSX address of the .01 file and nothing uses it.)
	static const uint32_t ARENA_SCRATCH = 0x8000, DEPTH_SCRATCH = 0x4000;
	static const uint32_t DATA_LO = 0xB6D000, DATA_HI = 0x279F000;
	static Region g_r_vscr, g_r_depth;
	static void SaveDrawScratch()
	{
		uint32_t top = U32(CTX(), 0x74);
		uint32_t n = top >= DATA_LO && top < DATA_HI ? DATA_HI - top : 0;
		region_save(g_r_vscr, (void *)top, n < ARENA_SCRATCH ? n : ARENA_SCRATCH);
		region_save(g_r_depth, DEPTHARR(), DEPTH_SCRATCH);
	}
	static void RestoreDrawScratch()
	{
		region_restore(g_r_depth);
		region_restore(g_r_vscr);
	}

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
	// sprite: frame list cursor (bone+0xD0) of a draw 5 (animated sprite) bone. Only the draw
	// steps it; the VM writes it when it (re)starts the sprite animation (SetSpriteAnim), i.e. on
	// a bone spawned again in the same slot. SpawnBoneWithId (0x135) gives every such bone the
	// same id (Cerberus: 0x3E7), so id16 does not tell a re-spawn: a new cursor does.
	struct PredBone { uint16_t id16; uint8_t alive, draw; int16_t ang[3], pos[3]; uint32_t colour, sprite; };
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
		int16_t shake[3];         // shake offsets the next tick's VM leaves (0 = not written)
		unsigned shake_written;   // bit k = component k written by the VM (op 0x047)
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

	// embedded battle models (draw 3 / held_model_draws) as the next tick's VM finds them.
	// Vanilla's VM rebuilds a model's bone matrices before it reads them: op 0x089
	// StepEmbeddedModelAnim (block flag 2 set: the VM, not the draw, steps the animation) reads
	// the next frame (ReadAnimation, restart on completion), op 0x05B AttachToModelJoint rebuilds
	// the pose matrices and composes a joint into a bone's node. Both engine calls are skipped
	// while predicting (they write the model's skeleton, outside the snapshot), so the prediction
	// found the matrices the real tick's draw left, already composed with the parent
	// (ComputeBonesWorldMatrices works in place): the attached bones' predicted nodes were
	// composed twice (Bahamut: the bones on the Mega Flare model's joints, far off screen).
	// Here, before the predicted VM, with the skeleton and the animation command journaled, the
	// model's local bone matrices are rebuilt from its pose; a model the next tick's VM steps
	// (op 0x089) is stepped where the VM steps it (PredOp089: the pose step + the local matrices
	// of the new pose, as op 0x05B rebuilds them), so the bones attached to it see the pose the
	// real tick gives them at their place in the VM order: the next pose after the model's bone
	// (Bahamut), the current one before it (Brothers: bone 0x3A reads the draw-3 model's joint
	// before bone 0x0D steps it - stepped up front, the attached bone ran one frame ahead).
	// "Steps" = the predicted VM advanced the block's frame counter. The held draw shows such a
	// model at the in-between pose (held_model_pose).
	struct ModelStep { uint8_t *blk; uint16_t cnt; bool pre, stepped; };
	// (the up-front step, pre, is no longer used: ModelStepsCollect leaves it false)
	static ModelStep g_msteps[8];
	static int g_nmsteps = -1;
	static uint8_t *ModelSkeleton(uint8_t *hdr) { uint8_t *com = PTR(hdr, 4); return com ? PTR(com, 0) : nullptr; }
	static void ModelStepsCollect(const Clone &c)
	{
		g_nmsteps = 0;
		const uint8_t *model_draw = model_draws_of(c);
		for (int k = 0; DRAWORDER()[k] != 0xFF && g_nmsteps < 8; k++)
		{
			uint8_t *b = PTR(CTX(), 0x90) + 0x100 * (uint32_t)DRAWORDER()[k];
			uint8_t d = U8(b, 0x1C);
			bool model = d == 3;
			for (int m = 0; m < 4; m++) model |= model_draw[m] != 0 && model_draw[m] == d;
			uint8_t *blk = model ? PTR(b, 0xBC) : nullptr;
			if (!blk || !ModelSkeleton(blk + 0x10)) continue;
			uint16_t fl = U16(blk, 2);
			(void)fl;
			g_msteps[g_nmsteps++] = { blk, U16(blk, 0), false, false };
		}
	}
	// (predicting, engine state saved) skeletons as the next tick's VM finds them
	static void ModelsPrepare()
	{
		for (int i = 0; i < g_nmsteps; i++)
		{
			uint8_t *blk = g_msteps[i].blk, *hdr = blk + 0x10, *cmd = blk + 0x20;
			uint8_t *sk = ModelSkeleton(hdr);
			guard(sk, 0x10 + 0x30 * (uint32_t)U8(sk, 0));
			guard(cmd, 8);
			g_predict = false;
			if (g_msteps[i].pre)
			{
				// 0xB26AD0, not frozen, flag 2 cleared by op 0x089
				if (x::ReadAnimation(hdr, cmd) == 1) x::PreReadAnimation(hdr, cmd, U8(blk, 9));
			}
			else
				x::BuildBoneMatricesFromPose(hdr);
			g_predict = true;
		}
	}
	// (predicting) the clone's op 0x089 StepEmbeddedModelAnim, swapped in by PredictPass: for a
	// collected (journaled) model the step h_B26AD0 makes on it (block flag 2 cleared, not frozen:
	// ReadAnimation / PreReadAnimation) with the engine calls enabled, then the local bone
	// matrices of the new pose; then the op itself (engine calls skipped: parent xform, flags,
	// frame counter)
	static VoidFn g_pred_op089 = nullptr;
	static void __cdecl PredOp089()
	{
		uint8_t *blk = PTR(CUR(), 0xBC);
		for (int i = 0; blk && i < g_nmsteps; i++)
		{
			if (g_msteps[i].blk != blk) continue;
			if (U8(RT(), 0x45) == 0 && !(U16(blk, 2) & 1) && U16(blk, 0) != 0)
			{
				uint8_t *hdr = blk + 0x10, *cmd = blk + 0x20;
				g_predict = false;
				if (x::ReadAnimation(hdr, cmd) == 1) x::PreReadAnimation(hdr, cmd, U8(blk, 9));
				x::BuildBoneMatricesFromPose(hdr);
				g_predict = true;
			}
			break;
		}
		g_pred_op089();
	}
	static void ModelStepsEnd()
	{
		for (int i = 0; i < g_nmsteps; i++) g_msteps[i].stepped = U16(g_msteps[i].blk, 0) != g_msteps[i].cnt;
	}
	static bool ModelStepped(const uint8_t *blk)
	{
		if (!g_pred.ok || g_pred.paused) return false;
		for (int i = 0; i < g_nmsteps; i++) if (g_msteps[i].blk == blk) return g_msteps[i].stepped;
		return false;
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
			p.sprite = p.draw == 5 ? U32(b, 0xD0) : 0;
		}
		for (int k = 0; k < 256 && ORDER()[k] != 0xFF; k++)
		{
			uint32_t i = ORDER()[k] & 0x7F;
			if (i < nb) out[i].alive = 1;
		}
	}

	// runs the VM part of the next tick (preamble + Neg/Integrator/Pos) on the real state with
	// every out-of-state side effect disabled, captures the result and puts everything back
	static void PredictPass(Clone &c)
	{
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

		// the view the next tick copies into the node matrices: the battle camera module builds it
		// (BS_Camera_BuildViewMatrixApplyShake 0x5033E0) from the camera this tick wrote and the
		// shake offsets 0x1D97710 this tick's VM wrote (op 0x047; the build adds and clears them),
		// before that tick; here it is built early from the same inputs (the shake as the real
		// tick left it, held_note_scratch: a held frame's own view build may have consumed it; the
		// camera block 0x1D97700..+0xC0 is put back below; the build's scratch / GTE use are inside
		// the saved regions). Predicting with the view still in 0x1D97778 (the one this tick drew
		// with, or the held frame's) bakes the wrong camera into every node slot: the held
		// geometry then lags the camera step.
		static uint8_t camblk[0xC0];
		memcpy(camblk, (void *)0x1D97700, sizeof(camblk));
		memcpy((void *)0x1D97710, g_shake, sizeof(g_shake));
		((void (__cdecl *)())0x5033E0)();
		// shake offsets the next tick's VM writes (op 0x047): a marker that no s16 outPos
		// word takes, read back after the pass (held_shake_next)
		static const uint16_t SHAKE_MARK = 0x8000;
		for (int k = 0; k < 3; k++) ((uint16_t *)0x1D97710)[k] = SHAKE_MARK;
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
		if (g_nmsteps < 0) ModelStepsCollect(c);
		if (!g_pred.paused) ModelsPrepare();
		if (!g_pred.paused)
		{
			g_pred_op089 = c.vm[0x089];
			c.vm[0x089] = PredOp089;
			AnimChannelsNeg();
			AnimIntegrator();
			AnimChannelsPos();
			c.vm[0x089] = g_pred_op089;
		}
		ModelStepsEnd();
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
		{
			const uint16_t *sh = (const uint16_t *)0x1D97710;
			g_pred.shake_written = 0;
			for (int k = 0; k < 3; k++)
			{
				g_pred.shake[k] = sh[k] == SHAKE_MARK ? 0 : (int16_t)sh[k];
				if (sh[k] != SHAKE_MARK) g_pred.shake_written |= 1u << k;
			}
		}

		g_predict = false;
		journal_restore();
		RestoreEngine(true);
		memcpy((void *)0xB8B7F0, cam0, 16);
		memcpy((void *)0x1D97700, camblk, sizeof(camblk));
		PROJH() = projh;
		RT() = saved_globals[0]; WS() = saved_globals[1]; CTX() = saved_globals[2]; SCENE() = saved_globals[3];
		RCTX() = saved_globals[4]; SEQ() = saved_globals[5]; CUR() = saved_globals[6]; STREAM() = saved_globals[7];
		PTR3() = saved_ptr3;
		*CrtSeed() = seed;
		region_restore(g_r_free);
		region_restore(g_r_gte);
	}

	static void Predict(Clone &c)
	{
		if (g_pred.tick == g_real_tick) return;
		g_pred.tick = g_real_tick;
		g_pred.ok = false;
		if (!g_scratch) return;
		g_nmsteps = -1;
		PredictPass(c);
		// a model stepped up front that the VM did not step: predict again without that step
		bool again = false;
		for (int i = 0; i < g_nmsteps; i++)
			if (g_msteps[i].pre && !g_msteps[i].stepped) { g_msteps[i].pre = false; again = true; }
		if (again) PredictPass(c);
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

	// rotation part of a matrix node at the in-between: the node matrices are rotations with a
	// per-axis scale (column scale, M = R * S, or row scale, M = S * R); blending the 3x3
	// elements shrinks the object on a large rotation step (Eden's spinning fans turn 180 degrees
	// per tick: the element-wise in-between is a flat line). The rotation is turned by
	// num/den of the step's angle about its axis (shortest way; a half turn keeps the axis the
	// data gives) and the scales are blended. False (element-wise blend) when either matrix is
	// not a scaled rotation (shear / mirror / degenerate).
	static bool MixRotation(Mat4x3 &m, const Mat4x3 &n, int num, int den)
	{
		for (int form = 0; form < 2; form++)   // 0: column scale, 1: row scale
		{
			double A[3][3], B[3][3], sa[3], sb[3];
			bool ok = true;
			for (int i = 0; i < 3 && ok; i++)
			{
				double qa = 0, qb = 0;
				for (int j = 0; j < 3; j++)
				{
					double a = (form ? m.m[i][j] : m.m[j][i]) / 4096.0, b = (form ? n.m[i][j] : n.m[j][i]) / 4096.0;
					qa += a * a; qb += b * b;
				}
				sa[i] = sqrt(qa); sb[i] = sqrt(qb);
				if (sa[i] < 1.0 / 64 || sb[i] < 1.0 / 64) ok = false;
			}
			if (!ok) return false;
			for (int r = 0; r < 3; r++)
				for (int c = 0; c < 3; c++)
				{
					int k = form ? r : c;
					A[r][c] = m.m[r][c] / 4096.0 / sa[k];
					B[r][c] = n.m[r][c] / 4096.0 / sb[k];
				}
			// orthonormal, proper rotations (tolerance for the 4.12 rounding)
			auto ortho = [](const double R[3][3]) {
				for (int i = 0; i < 3; i++)
					for (int j = 0; j < 3; j++)
					{
						double d = R[0][i] * R[0][j] + R[1][i] * R[1][j] + R[2][i] * R[2][j];
						if (fabs(d - (i == j ? 1.0 : 0.0)) > 0.02) return false;
					}
				double det = R[0][0] * (R[1][1] * R[2][2] - R[1][2] * R[2][1]) - R[0][1] * (R[1][0] * R[2][2] - R[1][2] * R[2][0])
					+ R[0][2] * (R[1][0] * R[2][1] - R[1][1] * R[2][0]);
				return det > 0;
			};
			if (!ortho(A) || !ortho(B)) continue;
			// step Q = B * A^T, angle / axis
			double Q[3][3];
			for (int r = 0; r < 3; r++)
				for (int c = 0; c < 3; c++) Q[r][c] = B[r][0] * A[c][0] + B[r][1] * A[c][1] + B[r][2] * A[c][2];
			double cs = (Q[0][0] + Q[1][1] + Q[2][2] - 1) / 2;
			if (cs > 1) cs = 1;
			if (cs < -1) cs = -1;
			double th = acos(cs), ax[3];
			if (th < 1e-6) return false;    // no rotation: the element-wise blend is exact
			double sn = sin(th);
			if (sn > 1e-3)
			{
				ax[0] = (Q[2][1] - Q[1][2]) / (2 * sn);
				ax[1] = (Q[0][2] - Q[2][0]) / (2 * sn);
				ax[2] = (Q[1][0] - Q[0][1]) / (2 * sn);
			}
			else
			{
				// half turn: Q = 2 a a^T - I; the largest diagonal gives the axis
				int k = 0;
				for (int i = 1; i < 3; i++) if (Q[i][i] > Q[k][k]) k = i;
				double ak = sqrt((Q[k][k] + 1) / 2);
				for (int i = 0; i < 3; i++) ax[i] = i == k ? ak : Q[k][i] / (2 * ak);
			}
			double al = sqrt(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
			if (al < 1e-9) return false;
			for (int i = 0; i < 3; i++) ax[i] /= al;
			double ph = th * num / den, c = cos(ph), s = sin(ph), t = 1 - c;
			double P[3][3] = {
				{ t * ax[0] * ax[0] + c, t * ax[0] * ax[1] - s * ax[2], t * ax[0] * ax[2] + s * ax[1] },
				{ t * ax[0] * ax[1] + s * ax[2], t * ax[1] * ax[1] + c, t * ax[1] * ax[2] - s * ax[0] },
				{ t * ax[0] * ax[2] - s * ax[1], t * ax[1] * ax[2] + s * ax[0], t * ax[2] * ax[2] + c } };
			for (int r = 0; r < 3; r++)
				for (int cc = 0; cc < 3; cc++)
				{
					double v = P[r][0] * A[0][cc] + P[r][1] * A[1][cc] + P[r][2] * A[2][cc];
					int k = form ? r : cc;
					v *= sa[k] + (sb[k] - sa[k]) * num / den;
					double q = floor(v * 4096 + 0.5);
					m.m[r][cc] = (int16_t)(q > 32767 ? 32767 : q < -32768 ? -32768 : q);
				}
			return true;
		}
		return false;
	}

	static void MixMatrix(Mat4x3 &m, const Mat4x3 &n, int num, int den)
	{
		if (num >= den) { m = n; return; }
		if (num > 0 && !MixRotation(m, n, num, den))
			for (int r = 0; r < 3; r++)
				for (int c = 0; c < 3; c++) m.m[r][c] = mix16(m.m[r][c], n.m[r][c], num, den, CUT_MAT);
		m.pad = mix16(m.pad, n.pad, num, den, 0x7FFF);
		for (int k = 0; k < 3; k++) m.t[k] = mix32(m.t[k], n.t[k], num, den, 0x100000);
	}

	// bones interpolated by the current held draw (held_bone_ends)
	static uint8_t g_mixed[128];

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
			// a sprite bone spawned again in this slot on the next tick: another sprite (the
			// real tick's one ends there), not a move
			if (p.draw == 5 && p.sprite != U32(b, 0xD0)) continue;
			g_mixed[i] = 1;
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

	bool held_bone_ends(const uint8_t *b, int16_t ang0[3], int16_t pos0[3], int16_t ang1[3], int16_t pos1[3])
	{
		if (!g_held.active || g_r_bones.p == nullptr) return false;
		uint32_t off = (uint32_t)(b - g_r_bones.p);
		if (b < g_r_bones.p || off >= g_r_bones.n || (off & 0xFF) != 0) return false;
		uint32_t i = off >> 8;
		if (i >= 128 || !g_mixed[i]) return false;
		const uint8_t *r = g_r_bones.copy + off;
		for (int k = 0; k < 3; k++)
		{
			ang0[k] = AT<int16_t>(r, 0x8C + 2 * k); pos0[k] = AT<int16_t>(r, 0x94 + 2 * k);
			ang1[k] = g_pred.b[i].ang[k]; pos1[k] = g_pred.b[i].pos[k];
		}
		return true;
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
		// the frame's camera shake: in-between of this tick's and the next tick's offsets
		if (g_pred.ok) ff8fx::held_shake_next(g_pred.shake, g_pred.shake_written);
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
		SaveDrawScratch();
		memset(g_mixed, 0, sizeof(g_mixed));
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
		g_rib_next_ok = false;
		U8(RT(), 0x45) = 0xFF;
		BuildMatricesAndDraw();
		x::ParseCamera2(PROJH());
		g_held.active = false;

		for (int i = 0; i < npools; i++) region_restore(pools[i]);
		for (int i = nskels - 1; i >= 0; i--) region_restore(skels[i]);
		region_restore(g_r_free);
		RestoreDrawScratch();
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
	void held_note_tick() { g_ported_tick = g_real_tick; g_rib_last_ok = false; }

	// Workspace + RuntimeSlot of the tick, copied at its end (held frames and the prediction
	// run on this copy)
	void held_note_scratch()
	{
		memcpy(g_scratch_copy, WS(), sizeof(g_scratch_copy));
		g_scratch = g_scratch_copy;
		memcpy(g_shake, (void *)0x1D97710, sizeof(g_shake));
	}

	// ------------------------------------------------------------------------------------
	// 0xB26AD0 StepEmbeddedModelAnim: held frames (rt+0x45 forced to 0xFF): when the animation
	// advances between the real tick and the next one, the bone matrices come from the exact
	// in-between pose (pose_midpoint) instead of BuildBoneMatricesFromPose. Block flag 2 clear:
	// the draw steps it (the real tick was not frozen); flag 2 set: the VM steps it (op 0x089,
	// the draw only rebuilds the pose), when the prediction saw the next tick step it
	// (ModelStepped; Bahamut's Mega Flare model: held still before).
	// ------------------------------------------------------------------------------------
	bool held_model_pose(uint8_t *blk, uint8_t *hdr, uint8_t *cmd)
	{
		uint16_t fl = U16(blk, 2);
		if (!g_held.active || (fl & 1) || U16(blk, 0) == 0) return false;
		// flag 2: the draw only rebuilds the pose; the VM (op 0x089) steps it: in-between when
		// the next tick's VM steps it (the prediction saw its frame counter advance)
		if ((fl & 2) ? !ModelStepped(blk) : g_held.real_skip != 0) return false;
		pose_midpoint(hdr, cmd, g_held.num, g_held.den);
		return true;
	}

	// ------------------------------------------------------------------------------------
	// 0xB29F20 (Draw 6) particle system, held frames (g_held, frozen path forced): the particle
	// is drawn at pos + the next integration step's position delta * num/den and with the
	// in-between of its colour and the next tick's colour fade step (+0x3C deltas, clamped
	// 0..255), on a local copy (the pool is not written). When the particle's script runs on
	// the next tick (wait counter +2 at 0) it may set a new colour / fade / velocity (ops 11,
	// 12, 4...) before the integration and the fade step: that script step is run on a second
	// copy (held_particle_script) and, when it is deterministic (no rand draw) and the particle
	// lives on, the next tick's colour and (when the script left the position alone) velocity
	// come from it; otherwise the colour stays the real tick's (it changes on that tick) and a
	// scripted position jump happens on the real frame. The frame (flipbook) stays the real
	// tick's. Called for a live particle whose frame (+0x30) is set, in place of the frozen
	// path's position type + draw.
	// ------------------------------------------------------------------------------------
	// held: the next tick's script step of particle p on the copy nx (vanilla loop of 0xB29F20:
	// ops until one yields); everything else it writes (stream cursor, the tick's Workspace /
	// RuntimeSlot, the owner bone's +0xB6, the CRT seed) is put back. True when the step drew no
	// random number and did not end the particle.
	static bool held_particle_script(uint8_t *p, uint8_t *nx)
	{
		memcpy(nx, p, 0x50);
		uint8_t *scr = PTR(p, 4);
		if (scr == nullptr) return false;
		static uint8_t ws_save[0x180];
		memcpy(ws_save, WS(), 0x180);             // Workspace + RuntimeSlot (contiguous copy)
		uint8_t *stream = STREAM();
		uint16_t b6 = U16(CUR(), 0xB6);
		uint32_t seed = *CrtSeed();
		PTR(WS(), 0x44) = nx;
		STREAM() = scr;
		int32_t op = S16(scr, 0);
		S16(RT(), 0x4A) = (int16_t)op;
		C().pop[op & 0xFF]();
		for (int n = 0; U16(RT(), 0x3E) == 0 && n < 256; n++)
		{
			op = S16(STREAM(), 0);
			S16(RT(), 0x4A) = (int16_t)op;
			C().pop[op & 0xFF]();
		}
		bool ok = U16(RT(), 0x3E) != 0 && *CrtSeed() == seed && STREAM() != nullptr && U16(nx, 0) != 0;
		*CrtSeed() = seed;
		U16(CUR(), 0xB6) = b6;
		STREAM() = stream;
		memcpy(WS(), ws_save, 0x180);
		return ok;
	}

	void held_particle_draw(uint8_t *p)
	{
		const bool held_adv = g_held.real_skip == 0 && g_held.den > 0;
		uint8_t *q = p;     // record the draw reads (a local copy when advanced)
		uint8_t loc[0x50];
		if (held_adv)
		{
			// the next tick's script step, when it runs (wait counter at 0)
			uint8_t nx[0x50];
			bool script = U16(p, 2) == 0;
			bool sim = script && held_particle_script(p, nx);
			// the record the integration starts from: nx when the script only changed the
			// velocity / acceleration / colour (a scripted position is a jump on the real frame)
			const uint8_t *src = sim && memcmp(nx + 0x10, p + 0x10, 0x10) == 0 ? nx : p;
			// position = pos + next tick's integration delta ((vel + acc) << 4, the
			// deterministic part) * num / den
			memcpy(loc, p, 0x50);
			for (int k = 0; k < 4; k++)
			{
				int32_t s = S16(src, 0x28 + 2 * k) + S16(src, 0x20 + 2 * k);
				int32_t d = (int32_t)((int64_t)shl32(s, 4) * g_held.num / g_held.den);
				S32(loc, 0x10 + 4 * k) = add32(S32(p, 0x10 + 4 * k), d);
			}
			const uint8_t *cs = script ? (sim ? nx : nullptr) : p;
			for (int k = 0; k < 3 && cs != nullptr; k++)
			{
				int32_t c0 = U8(p, 0x38 + k), c1 = S8(cs, 0x3C + k) + U8(cs, 0x38 + k);
				if (c1 >= 0x100) c1 = 0xFF;
				else if (c1 <= 0) c1 = 0;
				U8(loc, 0x38 + k) = (uint8_t)(c0 + (c1 - c0) * g_held.num / g_held.den);
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
	// the real tick's history (colour bone and parent matrix are the in-between values).
	// When the real tick would have grown the ribbon (not frozen on the real tick, outAngleX >=
	// 0), the held draw does the next tick's growth step on the saved ring (vanilla advance
	// code, held_ribbon_advance: head + 1, new cross-section, homing node position) and puts the
	// new head section num/den of its way from the previous head (the head segment grows from
	// nothing), and every segment's colour at the in-between of its ramp entry and the next
	// one's (the ramp is indexed by the age of the segment, which grows by one per tick). A full
	// ring loses its oldest (faded-out) segment with the growth. Tail retire (outAngleX < 0):
	// the oldest segment goes on the real frame (discrete), while the colours, which slide one
	// ramp entry per retire step, are the in-between of the real tick's entry and the next
	// tick's (held_ribbon_blend_ramp). Held-only additions besides:
	//   * VXY0 before SetupParentXform is set to the value the real tick had there (vanilla
	//     leaves V0.xy of the previous GTE user in the translation; in a held draw that is
	//     another handler's frozen-path value, which would shift the whole ribbon);
	//   * chained ribbons: when that previous GTE user was the ribbon drawn just before (its last
	//     loaded point: point 8 of its oldest drawn section), the translation moves with that
	//     ribbon's tail every tick it retires (Cerberus: three ribbons chained, each one moving by
	//     its predecessor's retiring tail); the held draw then takes VXY0 at the in-between of
	//     the real tick's value and the one the next tick will have (the predecessor's last point
	//     with its next tick's ring counters, held_ribbon_restore);
	//   * the arena scratch above ctx+0x74 (outside the saved arena) written by the draw is
	//     restored before returning.
	// The first-draw path cannot run on a held frame (a node's first draw is on the tick that
	// created it); if it did, everything it writes (arena, ctx+0x74, bone) is in the saved set.
	// ------------------------------------------------------------------------------------
	// software GTE data register 0 (VXY0, packed)
	static inline uint32_t &GTE_VXY0() { return MEM<uint32_t>(0x1CA8A10); }
	// VXY0 the real tick had when it reached SetupParentXform (see B1ADD0), per node; chain: it
	// was the last point loaded by the ribbon drawn just before
	struct V0Memo { uint32_t v0; bool chain; };
	static NodeMemo<V0Memo, 64> g_v0memo;
	// (g_rib_last_v0 / g_rib_next_v0: top of the file)
	// the arena scratch above ctx+0x74 this handler writes (projections + colour table)
	static uint8_t *g_hsave = nullptr;
	static uint32_t g_hsave_cap = 0;
	static uint8_t *g_hs_p = nullptr;
	static uint32_t g_hs_n = 0;

	// the held draw grew the ribbon (held_ribbon_v0 then blends the colour ramp)
	static bool g_rib_adv = false;

	// held: the next tick's growth step (0xB1A46E..0xB1A6E8 of the vanilla handler) on the saved
	// ring, then the new head section at num/den of its way from the previous head
	static void held_ribbon_advance(uint8_t *ring)
	{
		using namespace part_shared_ribbon;
		if (g_held.real_skip != 0 || g_held.den <= 0 || g_held.num <= 0) return;
		if (S16(CUR(), 0x8C) < 0) return;
		U16(ring, 0) = (uint16_t)(U16(ring, 0) + 1);
		int32_t len = (int32_t)U16(ring, 4) + 1;
		if (len < (int32_t)U16(ring, 2)) U16(ring, 4) = (uint16_t)len;
		int32_t head = U16(ring, 0);
		int32_t cap = U16(ring, 2);
		S32(WS(), 0x80) = head;
		PTR(WS(), 0x6C) = RingEntry(ring, head % cap);
		PTR(WS(), 0x68) = RingEntry(ring, sub32(head, 1) % cap);
		S32(WS(), 0x78) = U16(ring, 4);
		uint8_t *tgt = PTR(WS(), 0x98);
		uint8_t *prev = PTR(WS(), 0x68);
		uint8_t *nw = PTR(WS(), 0x6C);
		if (U8(ring, 0x10) & 1)
			S32(WS(), 0x94) = Ribbon_B1AFE0_FollowDelta(CUR() + 0x94, prev);
		else
			Ribbon_B1AE60_SteerToward(tgt + 0x94, prev, nw);
		int32_t flip = xl_LookAt_B66E50(ZEROVEC(), WS() + 0xE0, WS() + 0xA0);
		x::SetRotMatrix(WS() + 0xA0);
		x::GteWriteCtrl(S16(prev, 0), 5);
		x::GteWriteCtrl(S16(prev, 2), 6);
		x::GteWriteCtrl(S16(prev, 4), 7);
		if (U8(ring, 0x10) & 1)
		{
			Ribbon_B1B080_BuildCrossSection(S16(CUR(), 0x90), S32(WS(), 0x94), nw, flip);
			uint8_t *b = CUR();
			int32_t dx = sub32(S16(b, 0x94), S16(nw, 0));
			int32_t dy = sub32(S16(b, 0x96), S16(nw, 2));
			int32_t dz = sub32(S16(b, 0x98), S16(nw, 4));
			for (int n = 0; n < 9; n++)
			{
				U16(nw, 8 * n) = (uint16_t)(U16(nw, 8 * n) + (uint16_t)dx);
				U16(nw, 8 * n + 2) = (uint16_t)(U16(nw, 8 * n + 2) + (uint16_t)dy);
				U16(nw, 8 * n + 4) = (uint16_t)(U16(nw, 8 * n + 4) + (uint16_t)dz);
			}
		}
		else
			Ribbon_B1B080_BuildCrossSection(S16(CUR(), 0x90), S16(CUR(), 0x8C), nw, flip);
		// in-between head section (the pad words +6 / +0xE keep the new section's yaw / pitch)
		for (int n = 0; n < 9; n++)
			for (int k = 0; k < 3; k++)
			{
				int32_t a = S16(prev, 8 * n + 2 * k), d = S16(nw, 8 * n + 2 * k) - a;
				S16(nw, 8 * n + 2 * k) = (int16_t)(a + d * g_held.num / g_held.den);
			}
		if (!(U8(ring, 0x10) & 1))
		{
			// homing: the node follows its head (vanilla writes outPos / accumPos from it)
			for (int k = 0; k < 3; k++)
			{
				U16(CUR(), 0x94 + 2 * k) = U16(nw, 2 * k);
				S32(CUR(), 0x5C + 4 * k) = shl32(S16(nw, 2 * k), 16);
			}
		}
		// ws+0xF0.. (colour ramp lengths, set by the draw before this hook) were used by the
		// cross-section as its vector
		S32(WS(), 0xF0) = U16(ring, 0xA);
		S32(WS(), 0xF4) = U16(ring, 0xC);
		S32(WS(), 0xF8) = U16(ring, 2);
		g_rib_adv = true;
	}

	// held: save the scratch above the arena top this draw writes (projection buffers
	// [top, top+0xC6) and the colour table at top+0x100)
	void held_ribbon_save(uint8_t *edi)
	{
		g_hs_p = nullptr;
		g_hs_n = 0;
		g_rib_adv = false;
		if (!g_held.active) return;
		held_ribbon_advance(edi);
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

	// held: the next tick's tail step of the ribbon of CUR() (vanilla retire branch: outAngleX < 0
	// retires one tail section per tick, -2 holds the tail): 1 = one more section retired, 0 =
	// tail held, -1 = no retire step known (grows, frozen real tick, the retire ends the draw)
	static int held_ribbon_next_retire(const uint8_t *ring)
	{
		if (ring == nullptr || g_held.real_skip != 0) return -1;
		int16_t a0[3], p0[3], a1[3], p1[3];
		int32_t ax = held_bone_ends(CUR(), a0, p0, a1, p1) ? a1[0] : S16(CUR(), 0x8C);
		if (ax >= 0) return -1;
		int step = ax != -2 ? 1 : 0;
		if ((int32_t)U16(ring, 0xE) + step >= (int32_t)U16(ring, 2)) return -1;
		return step;
	}

	// held: the colour ramp ws+0x84 (built by the draw from the in-between colour bone) replaced
	// by the in-between of the real tick's ramp entry j + a and the next tick's entry j + b, each
	// built from that tick's colour bone values: (a, b) = (-1, 0) for a grown ribbon (the ramp is
	// indexed by the age of the section), (0, 1) for a retiring one (the ramp offset is the tail)
	static void held_ribbon_blend_ramp(int dir)
	{
		uint8_t *ramp = PTR(WS(), 0x84);
		uint32_t f0 = (uint32_t)S32(WS(), 0xF0), f4 = (uint32_t)S32(WS(), 0xF4), f8 = (uint32_t)S32(WS(), 0xF8);
		int32_t ents = (int32_t)(f0 + (f4 > f0 ? f4 - f0 : 0) + (f8 > f4 ? f8 - f4 : 0) + f8);
		if (ents <= 0) return;
		uint8_t *cb = PTR(WS(), 0x9C);
		int16_t a0[3], p0[3], a1[3], p1[3];
		if (!held_bone_ends(cb, a0, p0, a1, p1))
			for (int k = 0; k < 3; k++) { a0[k] = a1[k] = S16(cb, 0x8C + 2 * k); p0[k] = p1[k] = S16(cb, 0x94 + 2 * k); }
		auto colw = [](const int16_t *v) {
			uint32_t c = (uint32_t)(int32_t)v[2] | 0x200u;
			c = (c << 8) | (uint32_t)(int32_t)v[1];
			return (c << 8) | (uint32_t)(int32_t)v[0];
		};
		static std::vector<uint8_t> r0, r1;
		r0.assign((size_t)ents * 8 + 8, 0);
		r1.assign((size_t)ents * 8 + 8, 0);
		part_shared_ribbon::Ribbon_B1B220_BuildColourRamp(colw(p0), colw(a0), r0.data(), 0);
		part_shared_ribbon::Ribbon_B1B220_BuildColourRamp(colw(p1), colw(a1), r1.data(), 0);
		const int32_t da = dir < 0 ? -1 : 0, db = dir < 0 ? 0 : 1;
		for (int32_t j = ents - 1; j >= 0; j--)
		{
			int32_t j0 = j + da, j1 = j + db;
			if (j0 < 0) j0 = 0;
			if (j1 > ents - 1) j1 = ents - 1;
			for (int w = 0; w < 2; w++)
			{
				uint32_t c0 = U32(r0.data(), 8 * j0 + 4 * w), c1 = U32(r1.data(), 8 * j1 + 4 * w), c = U32(ramp, 8 * j + 4 * w) & 0xFF000000u;
				for (int s = 0; s < 24; s += 8)
				{
					int32_t x0 = (c0 >> s) & 0xFF, x1 = (c1 >> s) & 0xFF;
					c |= (uint32_t)((x0 + (x1 - x0) * g_held.num / g_held.den) & 0xFF) << s;
				}
				U32(ramp, 8 * j + 4 * w) = c;
			}
		}
	}

	// real tick: memo of VXY0 before SetupParentXform; held: VXY0 of the real tick put back
	void held_ribbon_v0()
	{
		if (!g_held.active)
		{
			V0Memo *m = g_v0memo.put(CUR());
			if (m) *m = { GTE_VXY0(), g_rib_last_ok && GTE_VXY0() == g_rib_last_v0 };
		}
		else if (!g_rib_adv)
		{
			const V0Memo *m = g_v0memo.get(CUR());
			if (m)
			{
				uint32_t v = m->v0;
				// chained to the ribbon drawn before: in-between of the real tick's value and the
				// one that ribbon leaves on the next tick
				if (m->chain && g_rib_next_ok)
				{
					int16_t x0 = (int16_t)(v & 0xFFFF), y0 = (int16_t)(v >> 16);
					int16_t x1 = (int16_t)(g_rib_next_v0 & 0xFFFF), y1 = (int16_t)(g_rib_next_v0 >> 16);
					int16_t x = (int16_t)lerp_i(x0, x1, g_held.num, g_held.den), y = (int16_t)lerp_i(y0, y1, g_held.num, g_held.den);
					v = (uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16);
				}
				GTE_VXY0() = v;
			}
		}
		else
		{
			// grown ribbon: VXY0 (it ends up in the parent translation) at the in-between of
			// the real tick's value and what the growth step left (as on the next tick). Section
			// colours: the ramp is indexed by the age of the section (it grows by one per tick)
			// and built from the colour bone: section j (age j on the next tick, age j - 1 on the
			// real tick) is the in-between of the real tick's ramp entry j - 1 and the next tick's
			// entry j, each ramp built from that tick's colour bone values
			if (const V0Memo *m = g_v0memo.get(CUR()))
			{
				uint32_t v1 = GTE_VXY0();
				int16_t x0 = (int16_t)(m->v0 & 0xFFFF), y0 = (int16_t)(m->v0 >> 16), x1 = (int16_t)(v1 & 0xFFFF), y1 = (int16_t)(v1 >> 16);
				int16_t x = (int16_t)lerp_i(x0, x1, g_held.num, g_held.den), y = (int16_t)lerp_i(y0, y1, g_held.num, g_held.den);
				GTE_VXY0() = (uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16);
			}
			held_ribbon_blend_ramp(-1);
		}
		// retiring on the next tick: the ramp offset (bone+0xBC +0xE, tail) grows by one, so
		// entry j on the real tick is entry j + 1 on the next one (the colours slide along the
		// ribbon while its tail retires)
		if (g_held.active && !g_rib_adv && held_ribbon_next_retire(PTR(CUR(), 0xBC)) == 1) held_ribbon_blend_ramp(1);
	}

	void held_ribbon_restore()
	{
		if (g_hs_n) memcpy(g_hs_p, g_hsave, g_hs_n);
		if (!g_held.active)
		{
			// the last point this draw loaded (the next ribbon's VXY0 when it is chained)
			g_rib_last_v0 = GTE_VXY0();
			g_rib_last_ok = true;
			return;
		}
		// held: the last point the next tick's draw of this ribbon will load: point 8 of its
		// oldest drawn section (the head section when none is drawn), with the next tick's ring
		// counters: a grown held ribbon already has them (its draw loaded that point); a retiring
		// one (next outAngleX < 0) loses one more tail section (-2: holds the tail); otherwise
		// (it grows on the next tick only, frozen real tick, end of the retire) unknown
		g_rib_next_ok = false;
		if (g_rib_adv)
		{
			g_rib_next_v0 = GTE_VXY0();
			g_rib_next_ok = true;
			return;
		}
		uint8_t *ring = PTR(CUR(), 0xBC);
		int step = held_ribbon_next_retire(ring);
		if (step < 0) return;
		int32_t cap = U16(ring, 2), tail = (int32_t)U16(ring, 0xE) + step;
		int32_t k = (int32_t)U16(ring, 4) - tail;
		int32_t e = (k >= 1 ? sub32((int32_t)U16(ring, 0), k) : (int32_t)U16(ring, 0)) % cap;
		const uint8_t *en = part_shared_ribbon::RingEntry(ring, e);
		g_rib_next_v0 = (uint32_t)U16(en, 0x40) | ((uint32_t)U16(en, 0x42) << 16);
		g_rib_next_ok = true;
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
