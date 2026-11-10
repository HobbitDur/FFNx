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

// 30 fps part of mag084_angel_wing.cpp: the held-frame drawing (included at the end of mag084_angel_wing.cpp)
//
// Every drawing task of Angel Wing updates THEN draws in the same tick. The real tick notes each draw in execution order
// (held_note_draw / held_note_system: the node as drawn, the GTE registers, the module scratch stack and the Field_Alloc scratch
// it started from); the held frame redraws them in the same order, each at tick + num/den, from copies of the nodes moved half
// way to the next tick's state (the task's own state step run on a copy with the director's next cut: no draw, no spawn, no
// sound). The director's cut test of the next tick (0x73B7E0 on +0x40) is its requested cut now (+0x42: the director runs first
// in the master's queues and makes it current at the start of its next run).
//   Prim plays    - the three plays (0x78ECD0 layout 0x171F2B0, 0x791260 layout 0x17217A8, 0x796290 layout 0x1720570), all
//                   drawn by the play step 0x78EDB0 (= 0x73C280, object callback 0x78EEE0): prim::play_held (objects half way to
//                   their next keyframe records) with the parameter block built from the node's placement and the frame's
//                   camera. A play that ends on the next tick (cut 3 / cut 4) holds below 1 and is not drawn at 1.
//   Model         - Rinoa's angel model (the model container 0x7961E0, draw 0x796390): REVEAL phase (+0x26 bit 3) - the reveal
//                   renderer 0x796720 on the in-between pose (pose_midpoint) with the reveal radius 0x25C23B4 half way to the next
//                   tick's (+0x20 per tick from the container's frame 0x15, up to 0x500) and the reveal height of the in-between
//                   pose; the switch to the creature draw (cut 2) happens at 1. Then the creature draw a_746C10 at the in-between
//                   pose; its fade-out (state 4: +0x13A) half way; hidden at the end of the fade: not drawn at 1.
//   Glow A        - the glow model 0x15720B0 at Rinoa's point 0xF1 (0x796FD0, draw 0x797080): the fade +0x50 half way, the
//                   point half way to the one the next tick takes (GetEffectSpawnPosition on her next pose, her real bone
//                   matrices rebuilt after).
//   Glow B        - the glow model 0x1572810 (0x7972B0, draw 0x797330): the fade +0x50 half way; hidden by the next tick (cut 3):
//                   held below 1, not drawn at 1.
//   Feathers      - the actor systems 0x7913C0 (descriptor 0x171DE1C, step 0x791F30): the act-library actor template (as The End
//                   / Angelo: every drawn actor from a copy half way to its next motion step, the view of the next tick from the
//                   live camera; flipbook frame, model, fade and colours hold).
// A part the next tick does not draw holds this tick's picture below 1 and is not drawn at 1; a part the next tick starts is not
// drawn. At 1 every part is the next tick's draw.
// Camera: the module's camera script (camera task 0x78DE40 through the library stepper a_73AE10 on the camera block [0x1572BAC]):
// HeldCamera = act::held_camera (the stepper run once more on the saved camera block / words / GTE / Field_Alloc scratch, eye and
// look-at half way; a cut holds). Every part is placed through the frame's camera (0x1D97778, the frame gate's); the actor systems
// through the module's camera copy set to the next tick's view (NextView). Not drawn by the module and held at 15 Hz by the engine
// (as the real ticks set them): the other battle entities' hidden bit (0x78E8F0 / 0x78EA70), Rinoa's model swap
// (BS_BindEntityModelFromComData / QueueChainTransformation), the sound.
// The packets go to private copies of the two packet arenas (module cursor 0x25C2354, frame arena 0x1D8E054) that start with the
// real tick's packets; the module globals, the module scratch stack + camera copy, the Field_Alloc scratch, the shadow /
// RenderGeometry temporaries, the camera word 0x1D9770C (GetEffectSpawnPosition), the actor data's morph vertex blocks, the CRT
// rand seed and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace angelwing
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25C2350, BSS_HI = 0x25C9F70;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t CAM_COPY = 0x2793E58;
	static const uint32_t CUR_A = 0x25C2354, CUR_FA = 0x1D8E054; // the packet cursors the draws advance
	static const uint32_t ORIG_PlayA = 0x78ECD0, ORIG_PlayB = 0x791260, ORIG_PlayC = 0x796290, ORIG_ActorSystem = 0x7913C0;
	static const uint32_t ORIG_Model = 0x7961E0, ORIG_GlowA = 0x796FD0, ORIG_GlowB = 0x7972B0, ORIG_Camera = 0x78DE40;
	static const uint32_t REVEAL_RADIUS = 0x25C23B4, REVEAL_HEIGHT = 0x25C23B6, MODEL_DRAW_ARG = 0x25C9238;
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const int MAX_ACTORS = 160;
	static const uint32_t NODE_MAX = 0x160; // the largest node a draw reads (model container 0x154; plays: placement 0x94)

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries): a kind-5 model's step
	// rewrites them in the actor data, which every actor system of the cast shares
	struct VertexBlocks
	{
		uint32_t n;
		uint32_t addr[16], len[16];
		uint8_t bytes[0x4000];
		void save(uint32_t sys)
		{
			n = 0;
			const uint32_t tab = U32(sys, 0x22C);
			if (!tab) return;
			uint32_t used = 0;
			for (int k = 0; k < 16; k++)
			{
				const uint32_t blk = U32(tab, k * 4);
				if (!blk) continue;
				const uint32_t bytes_n = U32(blk, 4) * 8; // vertex count +4, 8-byte vertices from +8
				if (bytes_n == 0 || used + bytes_n > sizeof(bytes)) continue;
				addr[n] = blk + 8;
				len[n] = bytes_n;
				memcpy(bytes + used, (const void *)(blk + 8), bytes_n);
				used += bytes_n;
				n++;
			}
		}
		void restore() const
		{
			uint32_t used = 0;
			for (uint32_t i = 0; i < n; i++)
			{
				memcpy((void *)addr[i], bytes + used, len[i]);
				used += len[i];
			}
		}
	};

	// one draw of the real tick (execution order)
	struct Rec
	{
		int kind;
		uint32_t node;              // the task node (HK_SYSTEM: the actor state block)
		uint8_t snap[NODE_MAX];     // the node as drawn
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		int sysi;                   // HK_SYSTEM: its vertex-block note
		int16_t radius;             // HK_MODEL: the reveal radius it drew with
	};
	static const int MAX_RECS = 64, MAX_SYSTEMS = 8;
	static Rec g_rec[MAX_RECS];
	static int g_nrec = 0;
	static VertexBlocks g_sys_verts[MAX_SYSTEMS];
	static int g_nsys = 0;
	static uint32_t g_tick = 0xFFFFFFFF, g_end_tick = 0xFFFFFFFF;
	static uint32_t g_start[2], g_end[2];                 // the cursors at the tick's first draw / after the master's queues
	static const uint32_t CURSORS[2] = { CUR_A, CUR_FA };
	static uint8_t g_priv[2][0x40000];
	static uint8_t g_cam_tick[0x20];                       // the module's camera copy this real tick drew with
	static bool g_in_held = false;                         // a held frame runs (its draws note nothing)
	static bool g_full = false;                            // this real tick's draws did not fit the notes

	static uint32_t node_size(uint32_t node)
	{
		const uint32_t f = U32(node, 8);
		if (f == ORIG_Model) return 0x154;
		if (f == ORIG_GlowA || f == ORIG_GlowB) return 0x58;
		return 0x94; // plays: the placement (the layout +0x94 is read live)
	}

	static void begin_tick()
	{
		if (g_tick == g_real_tick) return;
		g_tick = g_real_tick;
		g_nrec = g_nsys = 0;
		g_full = false;
		for (int i = 0; i < 2; i++) g_start[i] = g_end[i] = MEM<uint32_t>(CURSORS[i]);
		memcpy(g_cam_tick, (const void *)CAM_COPY, sizeof(g_cam_tick));
	}

	static Rec *new_rec(int kind, uint32_t node)
	{
		if (g_in_held) return nullptr;
		begin_tick();
		if (g_nrec >= MAX_RECS) { g_full = true; return nullptr; }
		Rec &r = g_rec[g_nrec++];
		r.kind = kind;
		r.node = node;
		r.sysi = -1;
		r.radius = MEM<int16_t>(REVEAL_RADIUS);
		memcpy(r.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(r.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(r.scratch, (const void *)SCRATCH_LO, sizeof(r.scratch));
		memcpy(r.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(r.falloc));
		return &r;
	}

	static void held_note_draw(int kind, uint32_t node)
	{
		Rec *r = new_rec(kind, node);
		if (r) memcpy(r->snap, (const void *)node, node_size(node));
	}

	static void held_note_system(uint32_t sys)
	{
		Rec *r = new_rec(HK_SYSTEM, sys);
		if (!r || g_nsys >= MAX_SYSTEMS) { if (r) { g_nrec--; g_full = true; } return; }
		r->sysi = g_nsys;
		g_sys_verts[g_nsys++].save(sys);
	}

	static void held_note_end()
	{
		if (g_in_held) return;
		begin_tick();
		for (int i = 0; i < 2; i++) g_end[i] = MEM<uint32_t>(CURSORS[i]);
		g_end_tick = g_real_tick;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// the next tick's state of a node (its task's state step on a copy: no draw, no spawn, no sound)
	// ---------------------------------------------------------------------------------------------------------------
	// the director's cut of the next tick (0x73B7E0 reads +0x40): the requested cut +0x42 becomes current at the start of its
	// next run (0x78DDB0, the first task of the master's queues)
	static bool cut_next(int32_t c) { return S16(MEM<uint32_t>(DIR_PTR), 0x42) >= c; }

	// a glow task's state step (0x796FD0 / 0x7972B0 states) on the copy a1, run with the next tick's cut
	static void glow_next(uint32_t a1)
	{
		static const uint32_t A[7] = { 0x797190, 0x7971B0, 0x7971F0, 0x797220, 0x797250, 0x797270, 0x7972A0 };
		static const uint32_t B[7] = { 0x797410, 0x797430, 0x797490, 0x7974C0, 0x7974E0, 0x797500, 0x797530 };
		const int8_t st = S8(a1, 0x29);
		if (st < 0 || st > 6) return;
		const uint32_t d = MEM<uint32_t>(DIR_PTR);
		const uint16_t cut = U16(d, 0x40);
		U16(d, 0x40) = U16(d, 0x42);
		callp((U32(a1, 8) == ORIG_GlowA ? A : B)[st], a1);
		U16(d, 0x40) = cut;
	}

	// a prim-play task of the next tick: does it play (draw)?
	static bool play_next(uint32_t a1)
	{
		const uint32_t f = U32(a1, 8);
		const int32_t st = S8(a1, 0x29);
		if (f == ORIG_PlayA) return st == 0 ? S16(a1, 0x24) >= 0x18 : st == 1 && !cut_next(3); // 0x78ED30 / 0x791210
		if (f == ORIG_PlayB) return st == 0 || (st == 1 && !cut_next(4));                       // 0x7912C0 / 0x791350
		if (f == ORIG_PlayC) return st == 0 || st == 1;                                         // 0x7962F0 / 0x796360
		return false;
	}

	// the task of a node drawn on the real tick (`snap`: as drawn, before its frame counter +0x24 counted the tick) runs on the
	// next tick: its node still holds it (same task, counter one on) and it was not released after the tick (finished with no
	// child alive: the task tail returns 2)
	static bool still_runs(uint32_t snap, uint32_t now)
	{
		return U32(now, 8) == U32(snap, 8) && S16(now, 0x24) == (int16_t)(S16(snap, 0x24) + 1) && !((U8(now, 0x26) & 1) && U8(now, 0x28) == 0);
	}

	static void lerp16(uint32_t mid, uint32_t a, uint32_t b, uint32_t o, int num, int den) { S16(mid, o) = (int16_t)lerp_i(S16(a, o), S16(b, o), num, den); }

	static uint8_t g_nx[NODE_MAX], g_mid[NODE_MAX];

	// ---------------------------------------------------------------------------------------------------------------
	// held draws
	// ---------------------------------------------------------------------------------------------------------------
	// the play step 0x78EDB0 (= 0x73C280) of the node `live` with the placement of `place`, in between
	static void PlayHeld(uint32_t live, uint32_t place, int num, int den)
	{
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		x::MAG_022_sub_8DD770(L);
		uint16_t ax = U16(place, 0x62);
		if (ax != 0) x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)(int16_t)ax);
		ax = U16(place, 0x60);
		if (ax != 0) x::sub_8DD7E0(L, (uint32_t)(int32_t)(int16_t)ax);
		ax = U16(place, 0x64);
		if (ax != 0) x::sub_8DD960(L, (uint32_t)(int32_t)(int16_t)ax);
		x::scale3DMatrix(L, place + 0x50);
		S32(L, 0x14) = S16(place, 0x1C);
		S32(L, 0x18) = S16(place, 0x1E);
		S32(L, 0x1C) = S16(place, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(place, 0x86);
		U32(L, 0x48) = MEM<uint32_t>(0x25C9EFC);
		U32(L, 0x44) = U32(place, 0x7C);
		U16(L, 0x4C) = U16(place, 0x8C);
		U32(L, 0x38) = U32(place, 0x70);
		U16(L, 0x52) = U16(place, 0x92);
		U16(L, 0x4E) = U16(place, 0x8E);
		U16(L, 0x50) = U16(place, 0x90);
		U16(L, 0x56) = U16(place, 0x80);
		U16(L, 0x58) = U16(place, 0x88);
		prim::play_held((prim::Layout *)(live + 0x94), (prim::Callback)(void *)te_78EEE0, (int)L, num, den);
	}

	// Rinoa's point 0xF1 the next tick's glow A takes (GetEffectSpawnPosition on her next pose); her real bone matrices rebuilt
	// after, the camera word it writes (0x1D9770C) put back
	static bool g_np_done = false;
	static int16_t g_np[3];
	static const int16_t *NextPoint(uint32_t ent)
	{
		if (g_np_done) return g_np;
		g_np_done = true;
		const uint16_t w = MEM<uint16_t>(0x1D9770C);
		alignas(4) uint8_t pt[8] = {};
		pose_midpoint((void *)(ent + 0x60), (void *)(ent + 0x6C), 1, 1);
		x::GetEffectSpawnPosition(ent, 0xF1, 0, P(pt));
		x::BattleModel_BuildBoneMatricesFromPose(ent + 0x60);
		MEM<uint16_t>(0x1D9770C) = w;
		for (int k = 0; k < 3; k++) g_np[k] = S16(P(pt), 2 * k);
		return g_np;
	}

	// the reveal draw (0x796390 reveal branch + 0x796620) of the model container `node` on the bone matrices built now (the
	// in-between pose: 0x796620's rebuilds from the pose are not run), radius `radius`; the real bone matrices are rebuilt after
	static void RevealDraw(uint32_t node, int16_t radius)
	{
		alignas(4) uint8_t pt[8] = {};
		U32(MEM<uint32_t>(DIR_PTR), 0x18) = node;
		const uint32_t mdl = node + 0x30;
		x::ComposeZYXRotationMatrix(mdl + 0xC, mdl + 0x40);
		S32(mdl, 0x54) = S16(mdl, 0x1C);
		S32(mdl, 0x58) = S16(mdl, 0x1E);
		S32(mdl, 0x5C) = S16(mdl, 0x20);
		x::scale3DMatrix(mdl + 0x40, node + 0x114);
		x::GetEffectSpawnPosition(mdl, 0xF1, 0, P(pt));
		MEM<uint16_t>(REVEAL_HEIGHT) = U16(P(pt), 2);
		MEM<int16_t>(REVEAL_RADIUS) = radius;
		const uint32_t ctx = REAPER_CTX;
		const uint32_t blk = U32(ctx, 0x48);
		memcpy((void *)ctx, (const void *)(blk + 0x40), 32);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(ctx);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x20);
		x::GTE_LoadIRFromMatrixColumn(ctx + 2);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x22);
		x::GTE_LoadIRFromMatrixColumn(ctx + 4);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(ctx + 0x24);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(ctx + 0x14);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(ctx + 0x34);
		MEM<uint32_t>(U32(ctx, 0x4C)) = x::sub_5088A0(blk, MEM<uint32_t>(0x1D8E04C) + 0x4040, 0x10, MEM<uint32_t>(U32(ctx, 0x4C)));
		te_796720(U32(blk, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		const uint32_t second = U32(blk, 0x78);
		if (second != 0)
			te_796720(U32(second, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, ctx);
		x::BattleModel_BuildBoneMatricesFromPose(blk + 0x60);
	}

	// the skeleton of a battle model's animation header (BattleAnimHeader +4 -> com data -> skeleton: bone count +0, pose)
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	static uint8_t *Skel(uint32_t header, uint32_t *size)
	{
		const uint32_t com = U32(header, 4);
		uint8_t *sk = com ? (uint8_t *)U32(com, 0) : nullptr;
		if (!sk || sk[0] == 0) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= SKEL_MAX ? sk : nullptr;
	}

	// the model container (0x7961E0 states 1..4 on the node as the real tick left it; the sound of state 1 is not played)
	static void ModelHeld(const Rec &r, int num, int den)
	{
		// (a container is hidden on the tick its fade-out ends, finished on the same tick: it is not noted then, so a noted
		// node always runs on the next tick)
		const uint32_t node = r.node, cur = P(r.snap);
		const int8_t st = S8(node, 0x29);
		const bool reveal0 = (U8(cur, 0x26) & 8) != 0;
		bool reveal1 = (U8(node, 0x26) & 8) != 0, shown1 = (U8(node, 0x26) & 4) == 0 && still_runs(cur, node);
		int16_t r1 = r.radius, f1 = S16(node, 0x13A);
		if (st == 1)
		{
			if (S16(node, 0x136) >= 0x15)
			{
				r1 = (int16_t)(r1 + 0x20);
				if (r1 >= 0x500) r1 = 0x500;
			}
			if (cut_next(2)) reveal1 = false;
		}
		else if (st == 4)
		{
			f1 = (int16_t)(f1 + 0x100);
			if (f1 >= 0x1000) { f1 = 0x1000; shown1 = false; }
		}
		if (num >= den && !shown1) return;
		if (reveal0 && (reveal1 || num < den))
		{
			pose_midpoint((void *)(node + 0x90), (void *)(node + 0x9C), reveal1 ? num : 0, den);
			RevealDraw(node, (int16_t)lerp_i(r.radius, reveal1 ? r1 : r.radius, num, den));
			return;
		}
		// the creature draw: the fade-out level half way (state 4 faded on the real tick already: +0x13A > 0; on the tick that
		// entered it the model keeps its colours below 1: fade 0 is the base colour mode, a_733950's tint mode a switch)
		if (st == 4 && (S16(node, 0x13A) > 0 || num >= den))
		{
			const int16_t f = (int16_t)lerp_i(S16(node, 0x13A), f1, num, den);
			S16(node, 0x13A) = f;
			S16(node, 0x13C) = f;
			te_796E30(node, 3);
			te_796F50(node);
		}
		// state 2 (a_739B40) of the next tick: a completed animation (command frame +6 >= count +7) starts animation 1
		// (au_re_Battle_ReadAnimation_7 reads its first frame): the pose holds below 1, at 1 that first frame
		uint32_t size = 0;
		uint8_t *sk = Skel(node + 0x90, &size);
		if (st == 2 && num >= den && sk && U8(node + 0x9C, 6) >= U8(node + 0x9C, 7))
		{
			// (the node keeps what the start writes, as the next tick draws it: the held frame puts the module globals back)
			static uint8_t save_sk[SKEL_MAX], next_sk[SKEL_MAX];
			memcpy(save_sk, sk, size);
			x::au_re_Battle_ReadAnimation_7(node, 1);
			memcpy(next_sk, sk, size);
			memcpy(sk, save_sk, size);
			pose_midpoint_to((void *)(node + 0x90), next_sk, num, den);
		}
		else
			pose_midpoint((void *)(node + 0x90), (void *)(node + 0x9C), num, den);
		MEM<uint32_t>(CUR_A) = a_746C10(node, MODEL_DRAW_ARG, MEM<uint32_t>(CUR_A));
	}

	// a noted node draw (play / glow) at num / den
	static void DrawHeld(const Rec &r, int num, int den)
	{
		const uint32_t cur = P(r.snap), nx = P(g_nx), mid = P(g_mid);
		const uint32_t n = node_size(cur);
		memcpy(g_nx, (const void *)r.node, n); // the node as the real tick left it
		memcpy(g_mid, r.snap, n);
		const bool alive = still_runs(cur, nx);
		switch (r.kind)
		{
		case HK_PLAY:
		{
			const bool draws = alive && play_next(nx);
			if (num >= den && !draws) return;
			PlayHeld(r.node, mid, draws ? num : 0, den);
			return;
		}
		case HK_GLOW:
		case HK_MOON:
		{
			if (alive) glow_next(nx);
			const bool draws = alive && (U8(nx, 0x26) & 4) == 0;
			if (r.kind == HK_GLOW && draws)
			{
				const int16_t *p = NextPoint(0x1D972C0 + (uint32_t)U8(nx, 0x2C) * 0x9C);
				for (int k = 0; k < 3; k++) S16(nx, 0x1C + 2 * k) = p[k];
			}
			if (num >= den) { if (draws) (r.kind == HK_GLOW ? te_797080 : te_797330)(nx); return; }
			if (draws)
			{
				for (uint32_t o = 0x1C; o <= 0x20; o += 2) lerp16(mid, cur, nx, o, num, den);
				lerp16(mid, cur, nx, 0x50, num, den);
			}
			(r.kind == HK_GLOW ? te_797080 : te_797330)(mid);
			return;
		}
		default:
			return;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// actor systems: the act-library actor template (The End / Angelo family; module actor state ACTOR_STATE)
	// ---------------------------------------------------------------------------------------------------------------
	// the battle camera words a view is built from (0x5033E0): eye 0xB8B7F0, look-at 0xB8B7F8, roll 0x1D977A2, shake 0x1D97710
	struct CamWords
	{
		int16_t w[10];
		void io(bool set)
		{
			static const uint32_t a[10] = { 0xB8B7F0, 0xB8B7F2, 0xB8B7F4, 0xB8B7F8, 0xB8B7FA, 0xB8B7FC, 0x1D977A2, 0x1D97710, 0x1D97712, 0x1D97714 };
			for (int i = 0; i < 10; i++)
			{
				if (set) MEM<int16_t>(a[i]) = w[i];
				else w[i] = MEM<int16_t>(a[i]);
			}
		}
	};
	// the words after each real tick (noted at its held frames: HeldReady): the view of the next tick is built from them
	static CamWords g_cw_held, g_cw_prev;
	static uint32_t g_cw_held_tick = 0xFFFFFFFF, g_cw_prev_tick = 0xFFFFFFFF;
	static void note_cam_words()
	{
		if (g_cw_held_tick == g_real_tick) return;
		g_cw_prev = g_cw_held;
		g_cw_prev_tick = g_cw_held_tick;
		g_cw_held.io(false);
		g_cw_held_tick = g_real_tick;
	}
	// the view the next tick draws with: the battle builds a tick's view (0x5033E0) from the camera words the tick before
	// left, so with a live camera (this tick's view = the one built from the words after the previous tick) the next view is
	// the one built from the words now; otherwise (a fixed view) it is this tick's. Camera block, words, GTE registers and
	// the Field_Alloc scratch are put back.
	static bool NextView(uint8_t out[0x20])
	{
		if (g_cw_prev_tick + 1 != g_real_tick) return false;
		static uint8_t cam[0xB0], words[0x20], gte[GTE_D_N + GTE_C_N], falloc[0x400];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(cam, (const void *)0x1D97700, sizeof(cam));
		memcpy(words, (const void *)0xB8B7F0, sizeof(words));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		if (fa) memcpy(falloc, (const void *)fa, sizeof(falloc));
		CamWords now;
		now.io(false);
		g_cw_prev.io(true);
		fn<void (__cdecl *)()>(0x5033E0)(); // BS_Camera_BuildViewMatrixApplyShake
		const bool live = memcmp((const void *)0x1D97778, g_cam_tick, sizeof(g_cam_tick)) == 0;
		if (live)
		{
			now.io(true);
			fn<void (__cdecl *)()>(0x5033E0)();
			memcpy(out, (const void *)0x1D97778, 0x20);
		}
		memcpy((void *)0x1D97700, cam, sizeof(cam));
		memcpy((void *)0xB8B7F0, words, sizeof(words));
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		MEM<uint32_t>(0x1D999C4) = fa;
		if (fa) memcpy((void *)fa, falloc, sizeof(falloc));
		return live;
	}

	// definition of an actor (state +0x224 table, index +0x1D6)
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	// the next tick's update pass (0x793780: list order; particles = type 0, actors = type 1) run on copies: a particle
	// moves by its placement kind (+0x68: 3 = rides its owner actor +0x5C, 0x7424C0; 4 = key-position path 0x7955A0; its
	// emission 0x794AD0 and life count: not run), an actor by its motion step (0x741050, the end test without its follow-up
	// spawn, 0x740910, 0x741120). A particle's owner and an actor's parent particle (+0x1BC) are their next-tick copies when
	// the pass updates them first, as on the next tick. A morphing model's step rewrites the shared vertex blocks:
	// SystemHeld puts the drawn ones back after the predictions.
	enum { P_HOLD, P_NEXT, P_ENDS };
	static const uint32_t PART_BYTES = 0x6C;
	static const int MAX_PARTS = 128;
	struct NextPass
	{
		uint32_t part[MAX_PARTS], act[MAX_ACTORS];
		int np, na, cls[MAX_ACTORS];
		alignas(4) uint8_t pc[MAX_PARTS][PART_BYTES];
		alignas(4) uint8_t ac[MAX_ACTORS][ACTOR_BYTES];
		uint32_t part_copy(uint32_t real) const { for (int i = 0; i < np; i++) if (part[i] == real) return P(pc[i]); return 0; }
		int act_index(uint32_t real) const { for (int i = 0; i < na; i++) if (act[i] == real) return i; return -1; }
	};
	static NextPass g_next;

	static int StepActor(uint32_t act, uint32_t nxt)
	{
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		const uint32_t parent = U32(nxt, 0x1BC);
		const uint32_t pc = parent ? g_next.part_copy(parent) : 0;
		if (pc) U32(nxt, 0x1BC) = pc;
		int r = P_NEXT;
		a_741050(nxt, def);
		const int32_t kind = S8(def, 0x1E);
		bool ends = false;
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			ends = S16(nxt, 0x1C8) < 0;
		}
		if (!ends && (kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) ends = true;
		if (ends)
			r = P_ENDS;
		else
		{
			a_740910(nxt, def);
			a_741120(nxt, def);
			U16(nxt, 0x1CA) = (uint16_t)(U16(nxt, 0x1CA) + 1); // frame count of the step
		}
		U32(nxt, 0x1BC) = parent;
		return r;
	}

	static void StepParticle(uint32_t part, uint32_t nxt)
	{
		memcpy((void *)nxt, (const void *)part, PART_BYTES);
		if (U16(part, 0x62) == 0) return; // (not initialised yet: none after a real tick)
		const int32_t mode = S8(nxt, 0x68);
		if (mode == 3)
		{
			const uint32_t owner = U32(nxt, 0x5C);
			const int oi = owner ? g_next.act_index(owner) : -1;
			if (oi >= 0) U32(nxt, 0x5C) = P(g_next.ac[oi]);
			a_7424C0(nxt);
			U32(nxt, 0x5C) = owner;
		}
		else if (mode == 4)
			te_7955A0(nxt);
		U16(nxt, 0x60) = (uint16_t)(U16(nxt, 0x60) + 1);
	}

	static void NextTick(uint32_t sys)
	{
		g_next.np = g_next.na = 0;
		for (uint32_t node = U32(sys, 0x2C); node; node = U32(node, 4))
		{
			const int32_t type = S16(node, 8);
			if (type == 0 && g_next.np < MAX_PARTS)
			{
				const int i = g_next.np++;
				g_next.part[i] = node;
				StepParticle(node, P(g_next.pc[i]));
			}
			else if (type == 1 && g_next.na < MAX_ACTORS)
			{
				const int i = g_next.na++;
				g_next.act[i] = node;
				g_next.cls[i] = StepActor(node, P(g_next.ac[i]));
			}
		}
	}

	// the drawn actor's copy `cur` half way to its next-tick copy: draw matrix, view positions, sub-positions; at fraction 1
	// the whole next actor. Holds: not in its motion phase, ends on the next tick, sequence change before fraction 1
	static int Predict(uint32_t act, uint32_t cur, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		const int ai = g_next.act_index(act);
		if (ai < 0) return P_HOLD;
		if (g_next.cls[ai] != P_NEXT) return g_next.cls[ai];
		const uint32_t nxt = P(g_next.ac[ai]);
		if (U32(nxt, 0x170) != U32(act, 0x170) && num < den) return P_HOLD;
		for (int k = 0; k < 9; k++)
			S16(cur, 0xAC + 2 * k) = (int16_t)lerp_i(S16(cur, 0xAC + 2 * k), S16(nxt, 0xAC + 2 * k), num, den);
		for (int k = 0; k < 3; k++)
			S32(cur, 0xC0 + 4 * k) = lerp_i(S32(cur, 0xC0 + 4 * k), S32(nxt, 0xC0 + 4 * k), num, den);
		const int32_t copies = S8(cur, 0x1D8);
		for (int32_t i = 0; i < copies && i < 16; i++)
			for (int k = 0; k < 3; k++)
			{
				const int32_t o = 0x19C + 8 * i + 2 * k;
				S16(cur, o) = (int16_t)lerp_i(S16(cur, o), S16(nxt, o), num, den);
			}
		// an actor whose motion step clears its sequence / model (+0x170: its last frame) is not drawn on the next tick
		if (num >= den && U32(nxt, 0x170) == 0) return P_ENDS;
		if (num >= den && S8(nxt, 0x1D8) == S8(cur, 0x1D8) && U32(nxt, 0x170) != 0)
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
		return P_NEXT;
	}

	static void SystemHeld(const Rec &r, const uint8_t *next_view, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		const uint32_t sys = r.node;
		MEM<uint32_t>(ACTOR_STATE) = sys;
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		if (next_view) memcpy((void *)CAM_COPY, next_view, 0x20); // (the scratch, camera copy included, is put back below)
		NextTick(sys);
		int cls[MAX_ACTORS];
		for (int i = 0; i < nl; i++)
			cls[i] = Predict(list[i], P(copies[i]), num, den);
		// the draws start from the real tick's GTE state, scratch, Field_Alloc scratch and morph vertex blocks
		memcpy((void *)GTE_D, r.gte, GTE_D_N);
		memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, r.scratch, sizeof(r.scratch));
		memcpy((void *)MEM<uint32_t>(0x1D999C4), r.falloc, sizeof(r.falloc));
		g_sys_verts[r.sysi].restore();
		// the sprite sequences, as 0x793640 draws them
		for (int i = 0; i < nl; i++)
		{
			if (U8(DefOf(list[i]), 0x16) > 2 || (cls[i] == P_ENDS && num >= den)) continue;
			const uint32_t c = P(copies[i]);
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				te_793700(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					te_793700(c);
				}
			}
		}
		// the models, as 0x791FC0 draws them
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(cls[i] == P_ENDS && num >= den))
				te_792020(P(copies[i]), def);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// the real tick's draws fit the notes and the private arenas (a quarter of each kept as margin: the in-between draws may
	// write a little more than the real ones)
	static bool fits()
	{
		if (g_full) return false;
		for (int i = 0; i < 2; i++)
			if (g_end[i] >= g_start[i] && g_end[i] - g_start[i] > sizeof(g_priv[i]) / 4 * 3) return false;
		return true;
	}

	static bool HeldReady()
	{
		note_cam_words(); // (every held frame, drawn or not)
		return held_ready() && g_end_tick == g_real_tick && g_tick == g_real_tick && fits();
	}

	static uint32_t *crt_seed() { return (uint32_t *)(fn<uint8_t *(__cdecl *)()>(0x560578)() + 0x14); }

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_084;
		g_in_held = true;
		g_np_done = false;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x1000], shadow[0x80];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		const uint16_t cam70c = MEM<uint16_t>(0x1D9770C);
		uint8_t sprite_cam[12]; // written by the sprite draws (0x571BC0)
		memcpy(sprite_cam, (const void *)0x21DFEE4, sizeof(sprite_cam));
		const uint32_t seed = *crt_seed();
		static VertexBlocks verts_now[MAX_SYSTEMS]; // the blocks as the real tick left them
		for (int i = 0; i < g_nrec; i++)
			if (g_rec[i].kind == HK_SYSTEM) verts_now[g_rec[i].sysi].save(g_rec[i].node);
		// the private arenas start with the real tick's packets: the redraws write the same packets at the same offsets
		// over them (the bytes a draw does not write are the real tick's)
		uint32_t cur[2];
		for (int i = 0; i < 2; i++)
		{
			cur[i] = MEM<uint32_t>(CURSORS[i]);
			const uint32_t used = g_end[i] - g_start[i];
			if (g_start[i] && g_end[i] > g_start[i] && used <= sizeof(g_priv[i])) memcpy(g_priv[i], (const void *)g_start[i], used);
			MEM<uint32_t>(CURSORS[i]) = P(g_priv[i]);
		}
		uint8_t view[0x20];
		const bool next_view = g_nsys > 0 && NextView(view);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_rec[i];
			if (r.kind == HK_SYSTEM) { SystemHeld(r, next_view ? view : nullptr, num, den); continue; }
			// each draw starts from the real tick's GTE state, scratch and Field_Alloc scratch
			memcpy((void *)GTE_D, r.gte, GTE_D_N);
			memcpy((void *)GTE_C, r.gte + GTE_D_N, GTE_C_N);
			memcpy((void *)SCRATCH_LO, r.scratch, sizeof(r.scratch));
			memcpy((void *)MEM<uint32_t>(0x1D999C4), r.falloc, sizeof(r.falloc));
			if (r.kind == HK_MODEL) ModelHeld(r, num, den);
			else DrawHeld(r, num, den);
		}
		for (int i = 0; i < 2; i++) MEM<uint32_t>(CURSORS[i]) = cur[i];
		for (int i = g_nrec - 1; i >= 0; i--)
			if (g_rec[i].kind == HK_SYSTEM) verts_now[g_rec[i].sysi].restore();
		*crt_seed() = seed;
		memcpy((void *)0x21DFEE4, sprite_cam, sizeof(sprite_cam));
		MEM<uint16_t>(0x1D9770C) = cam70c;
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		memcpy((void *)fa, falloc, sizeof(falloc));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_in_held = false;
		g_mod = saved_mod;
	}

	// camera: the module's camera script through the library stepper (act::held_camera)
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		const Mod *m = g_mod;
		g_mod = &MOD_084;
		const bool r = held_camera(num, den, world, lookat);
		g_mod = m;
		return r;
	}
}
}
	static void register_mag084_held()
	{
		static const uint32_t HELD[] = { act::angelwing::ORIG_PlayA, act::angelwing::ORIG_PlayB, act::angelwing::ORIG_PlayC,
			act::angelwing::ORIG_ActorSystem, act::angelwing::ORIG_Model, act::angelwing::ORIG_GlowA, act::angelwing::ORIG_GlowB, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(84, act::angelwing::HeldReady, act::angelwing::HeldFrame);
		register_module_camera(84, act::angelwing::HeldCamera);
	}
}
