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

// 30 fps part of mag008_doom.cpp: the held-frame drawing (included at the end of mag008_doom.cpp)
//
// Every drawing task of Doom updates THEN draws in the same tick. The real tick notes each draw in
// queue order (reaper, actor system, dust sparkles, smoke puffs, prim-model plays, clock rings,
// screen tints) with the GTE state, the module scratch stack and the Field_Alloc scratch it started
// from; the held frame replays them in the same order, each at tick + num/den:
//   Reaper        - the engine's creature draw a_746C10 at the in-between pose (pose_midpoint; the
//                   fade-out level half way); when the next tick starts a new animation the pose
//                   holds until fraction 1; at fraction 1 the next tick's creature: its state step
//                   run on a copy of the node (with the director's next phase). While it rises, the
//                   dark renderer 0x8CC7B0 on the in-between pose: every face's step is taken as the
//                   real tick drew it (the step bytes from before its draw, the tick's rising height)
//                   and as the next tick will (the bytes after it, the next height and spiral
//                   angle); the face is drawn at its spiral offset half way between the two with its
//                   grey half way; faces the next tick starts drawing appear at fraction 1 only; the
//                   switch to the creature draw happens at fraction 1.
//   Actor system  - as Magma Breath / Counter Laser-Eye: each drawn actor from a copy whose view
//                   positions / draw matrix are half way to the next tick's (the next motion step on
//                   a second copy; the whole next actor at fraction 1).
//   Dust sparkles - 4 sprites each: positions half way along the next tick's motion (velocity loses
//                   1/16, position += velocity / 16); the flipbook frame steps at fraction 1.
//   Smoke puffs   - spin and fade half way to the next tick's values (its state step).
//   Prim plays    - the scythe, clock and strike prim models: prim::play_held with the node's
//                   placement (the clock hand's position half way to its next step's; when the hand
//                   jumps back to its start the play holds until fraction 1).
//   Clock rings   - discrete ring steps: the tick's step, the next tick's at fraction 1.
//   Screen tints  - the engine task 0x8DDC30 of the last queue (noted by the master before that
//                   queue runs): the colour half way to the next step of its colour script.
// Draws that end on the next tick: sprite sequences, prim-player objects, smoke puffs and dust
// sparkles hold their last draw; the reaper, the actor models and the tints are not drawn at
// fraction 1.
// The packets go to private buffers (module cursor 0x2758504 and frame arena 0x1D8E054) that start
// with the real tick's packets; the module globals, the reaper / actor data rewritten in exe data,
// the module scratch stack, the Field_Alloc scratch, the shadow temporaries, the RenderGeometry
// temporaries and the GTE registers are put back.

namespace ff8fx
{
namespace act
{
namespace doom
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x2758500, BSS_HI = 0x275FB80;
	// exe data the effect rewrites in place (model containers / actor data / prim models)
	static const uint32_t XDATA[3][2] = { { 0x1636000, 0x1638000 }, { 0x1818000, 0x1826000 }, { 0x19EB000, 0x19F1000 } };
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const int MAX_ACTORS = 160;
	static const uint32_t NOTE_BYTES = 0x280;
	static const uint32_t FACE_LO = 0x5D, FACE_N = 0x278 - 0x5D; // the dark draw's step bytes (context +0x5D..)
	static const uint32_t FA_CURSOR = 0x1D8E054;

	// the morph destination vertex blocks of an actor system (state +0x22C table, 16 entries)
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
				const uint32_t bytes_n = U32(blk, 4) * 8;
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
	static const int MAX_SYSTEMS = 4;
	static VertexBlocks g_sys_verts[MAX_SYSTEMS];
	static int g_nsys_verts = 0;

	enum { K_REAPER, K_SYSTEM, K_SPARKLE, K_PUFF, K_PLAY, K_RINGS, K_TINT };
	struct Note
	{
		int kind;
		uint32_t addr;        // node / actor system state
		uint8_t node[NOTE_BYTES];
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		int verts;
		int16_t ring_step;
	};
	static Note g_notes[96];
	static int g_nnotes = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0, g_fa_start = 0;
	static uint8_t g_held_packets[0x60000], g_held_fa[0x60000];

	static Note *held_note(int kind, uint32_t addr, uint32_t bytes)
	{
		if (g_note_tick != g_real_tick)
		{
			g_note_tick = g_real_tick;
			g_nnotes = 0;
			g_nsys_verts = 0;
			g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
			g_fa_start = MEM<uint32_t>(FA_CURSOR);
		}
		if (g_nnotes >= 96) return nullptr;
		Note &n = g_notes[g_nnotes++];
		n.kind = kind;
		n.addr = addr;
		n.verts = -1;
		if (bytes) memcpy(n.node, (const void *)addr, bytes);
		memcpy(n.gte, (const void *)GTE_D, GTE_D_N);
		memcpy(n.gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(n.scratch, (const void *)SCRATCH_LO, sizeof(n.scratch));
		memcpy(n.falloc, (const void *)MEM<uint32_t>(0x1D999C4), sizeof(n.falloc));
		return &n;
	}

	// ---- the FX_HELD statements of the real tick
	static void held_note_reaper(uint32_t node)
	{
		Note *n = held_note(K_REAPER, node, 0);
		if (n) memcpy(n->node, (const void *)(REAPER_CTX + FACE_LO), FACE_N); // step bytes before the draw
	}
	static void held_note_system(uint32_t sys)
	{
		Note *n = held_note(K_SYSTEM, sys, 0);
		if (n && g_nsys_verts < MAX_SYSTEMS)
		{
			n->verts = g_nsys_verts;
			g_sys_verts[g_nsys_verts++].save(sys);
		}
	}
	static void held_note_sparkle(uint32_t node) { held_note(K_SPARKLE, node, 0xB0); }
	static void held_note_puff(uint32_t node) { held_note(K_PUFF, node, 0); }
	static void held_note_play(uint32_t node) { held_note(K_PLAY, node, 0x94); }
	static void held_note_rings(uint32_t node)
	{
		Note *n = held_note(K_RINGS, node, 0x30);
		if (n) n->ring_step = S16(MEM<uint32_t>(CLOCK_BLOCK), 0x54);
	}

	// the screen tints (engine task 0x8DDC30 in the last queue 0x275E488): their nodes before the
	// queue runs
	static const int MAX_TINTS = 10;
	static uint8_t g_tint[MAX_TINTS][0x40];
	static uint32_t g_tint_addr[MAX_TINTS];
	static int g_ntint = 0;
	static void held_note_tints()
	{
		g_ntint = 0;
		if (!held_note(K_TINT, 0, 0)) return;
		for (uint32_t node = MEM<uint32_t>(0x275E488); node && g_ntint < MAX_TINTS; node = U32(node, 4))
		{
			if (U32(node, 8) != 0x8DDC30) continue;
			memcpy(g_tint[g_ntint], (const void *)node, 0x40);
			g_tint_addr[g_ntint++] = node;
		}
	}

	static bool HeldReady() { return held_ready() && g_note_tick == g_real_tick; }

	// the draw starts from the real tick's GTE state and scratch
	static void Restore(const Note &n)
	{
		memcpy((void *)GTE_D, n.gte, GTE_D_N);
		memcpy((void *)GTE_C, n.gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, n.scratch, sizeof(n.scratch));
		memcpy((void *)MEM<uint32_t>(0x1D999C4), n.falloc, sizeof(n.falloc));
	}

	// ====================================================================================
	// reaper
	// ====================================================================================
	// the dark renderer's walk (0x8CC7B0) over the visible objects of model a1: the object's
	// vertices through the current bone matrices into the vertex buffer, then f(quad, rec, v[4])
	// for every face in draw order (v = the face's vertex words, 8 bytes each)
	template<typename F> static void DarkWalk(uint32_t model, uint32_t ctx, F f)
	{
		const uint32_t blk = U32(ctx, 0x48);
		const uint32_t vbuf = U32(U32(ctx, 0x44), 4);
		uint32_t table = U32(model, 4);
		const uint32_t bones = U32(model, 0) + 0x10;
		const int32_t count = S32(table, 0);
		table += 4;
		const uint32_t mask = U32(blk, 0x7C);
		alignas(4) static uint8_t scr[0x10];
		const uint32_t s = P(scr);
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t v = vbuf;
			const uint32_t off = U32(table, 0);
			table += 4;
			uint32_t obj = U32(model, 4) + off;
			if (!((mask >> (i & 31)) & 1))
				continue;
			int32_t groups = S16(obj, 0);
			obj += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = S16(obj, 0);
				obj += 2;
				const uint32_t m = bones + (uint32_t)(bone * 0x30) + 0x10;
				GTE_SetLightMatrix(m);
				GTE_SetBackgroundVector(U32(m, 0x14), U32(m, 0x18), U32(m, 0x1C));
				int32_t nv = S16(obj, 0);
				obj += 2;
				for (; nv > 0; nv--)
				{
					U16(s, 0) = U16(obj, 0);
					U16(s, 2) = U16(obj, 2);
					U16(s, 4) = U16(obj, 4);
					obj += 6;
					x::GTE_LoadV0(s);
					GTE_MVMVA_LightV0_Bk();
					x::GTE_StoreIR123(s);
					U32(v, 0) = U32(s, 0);
					U32(v, 4) = U32(s, 4);
					v += 8;
				}
			}
			obj = (obj + 3) & ~3u;
			const int32_t n3 = S16(obj, 0);
			const int32_t n4 = S16(obj, 2);
			uint32_t rec = obj + 0xC;
			for (int32_t k = n3; k > 0; k--, rec += 0x10)
			{
				const uint32_t vv[4] = { vbuf + (U16(rec, 0) & 0xFFF) * 8, vbuf + (U16(rec, 2) & 0xFFF) * 8, vbuf + (U16(rec, 4) & 0xFFF) * 8, 0 };
				f(false, rec, vv);
			}
			if (n4 > 0)
			{
				rec += 4;
				for (int32_t k = n4; k > 0; k--, rec += 0x14)
				{
					const uint32_t vv[4] = { vbuf + (U16(rec, -4) & 0xFFF) * 8, vbuf + (U16(rec, -2) & 0xFFF) * 8,
						vbuf + (U16(rec, 0) & 0xFFF) * 8, vbuf + (U16(rec, 2) & 0xFFF) * 8 };
					f(true, rec, vv);
				}
			}
		}
	}

	// a face's step after the draw's increment (face mean height at or below the rising height h)
	static uint8_t DarkStep(bool quad, const uint32_t *vv, uint8_t s, int16_t h)
	{
		int32_t mean;
		if (quad)
			mean = (S16(vv[3], 2) + S16(vv[0], 2) + S16(vv[1], 2) + S16(vv[2], 2)) / 4;
		else
			mean = (S16(vv[0], 2) + S16(vv[1], 2) + S16(vv[2], 2)) / 3;
		if ((int16_t)mean >= h && s < 0x13)
			s++;
		return s;
	}

	// the spiral offset of step s (angle a, step da; 0x8CC7B0's +0x74 / +0xC4 tables), mirrored on
	// odd faces
	static void DarkOffset(uint8_t s, uint16_t a, uint16_t da, bool odd, int16_t o[3])
	{
		o[0] = o[1] = o[2] = 0;
		if (s == 0x13 || s >= 0x10)
			return;
		uint32_t ang = a;
		for (int32_t i = 0; i < (int32_t)s; i++)
			ang = (uint32_t)(uint16_t)(da + (uint16_t)ang) & 0xFFF;
		const int32_t radius = (int16_t)((0xF - (int32_t)s) * 0x30);
		o[0] = (int16_t)(mul32((int32_t)x::computeSin((uint32_t)(int32_t)(int16_t)ang), radius) / 4096);
		o[2] = (int16_t)(mul32((int32_t)x::computeCosine((uint32_t)(int32_t)(int16_t)ang), radius) / 4096);
		if (odd)
		{
			o[0] = (int16_t)-o[0];
			o[2] = (int16_t)-o[2];
		}
	}
	static uint32_t DarkGrey(uint8_t s) { const uint32_t g = s * 8u > 0x80 ? 0x80u : s * 8u; return g; }

	static const int MAX_FACES = 2048;
	static uint8_t g_sn[MAX_FACES], g_sn1[MAX_FACES];

	// held dark draw of the reaper (0x8CC450 dark branch, 0x8CC6B0, 0x8CC7B0 for both models)
	static void DarkHeld(uint32_t node, const Note &n, int16_t h0, int16_t h1, uint16_t a0, uint16_t a1, int num, int den)
	{
		x::ComposeZYXRotationMatrix(node + 0x3C, node + 0x70);
		S32(node, 0x84) = S16(node, 0x4C);
		S32(node, 0x88) = S16(node, 0x4E);
		S32(node, 0x8C) = S16(node, 0x50);
		x::scale3DMatrix(node + 0x70, node + 0x114);
		const uint32_t ctx = REAPER_CTX;
		const uint32_t blk = U32(ctx, 0x48);
		memcpy((void *)ctx, (const void *)(blk + 0x40), 32);
		const uint32_t anim = blk + 0x60;
		const uint16_t da = U16(ctx, 0x5A);
		const uint32_t second = U32(blk, 0x78);
		const uint32_t models[2] = { U32(blk, 0x64), second ? U32(second, 4) : 0 };
		// the steps the real tick drew with (from the bytes before its draw, pose N, height h0) and
		// the next tick's (from the bytes after it, pose N+1, height h1); the face byte pointer
		// restarts at context +0x5C for each model (the models share the bytes)
		alignas(4) static uint8_t bytes[FACE_N + 8];
		int nf = 0;
		x::BattleModel_BuildBoneMatricesFromPose(anim);
		memcpy(bytes, n.node, FACE_N);
		for (int m = 0; m < 2 && models[m]; m++)
		{
			int fi = 0;
			DarkWalk(models[m], ctx, [&](bool quad, uint32_t, const uint32_t *vv) {
				const uint8_t s = DarkStep(quad, vv, bytes[fi], h0);
				bytes[fi] = s;
				if (nf < MAX_FACES) g_sn[nf] = s;
				fi++;
				nf++;
			});
		}
		pose_midpoint((void *)anim, (void *)(anim + 0xC), 1, 1);
		memcpy(bytes, (const void *)(ctx + FACE_LO), FACE_N);
		nf = 0;
		for (int m = 0; m < 2 && models[m]; m++)
		{
			int fi = 0;
			DarkWalk(models[m], ctx, [&](bool quad, uint32_t, const uint32_t *vv) {
				const uint8_t s = DarkStep(quad, vv, bytes[fi], h1);
				bytes[fi] = s;
				if (nf < MAX_FACES) g_sn1[nf] = s;
				fi++;
				nf++;
			});
		}
		// the in-between pose: the context matrix, the shadow and the faces as 0x8CC6B0 / 0x8CC7B0
		pose_midpoint((void *)anim, (void *)(anim + 0xC), num, den);
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
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		const bool last = num == den;
		alignas(4) static uint8_t fv[0x20];
		alignas(4) static uint8_t w[0x10];
		const uint32_t F = P(fv), W = P(w);
		nf = 0;
		for (int m = 0; m < 2 && models[m]; m++)
		{
			uint32_t pkt = MEM<uint32_t>(U32(ctx, 0x4C));
			x::GTE_SetRotMatrix(ctx + 0x20);
			x::GTE_SetTransVector(ctx + 0x20);
			int fi = 0;
			DarkWalk(models[m], ctx, [&](bool quad, uint32_t rec, const uint32_t *vv) {
				const int k = nf < MAX_FACES ? nf : MAX_FACES - 1;
				const uint8_t s0 = g_sn[k], s1 = g_sn1[k];
				fi++;
				nf++;
				const bool odd = (fi & 1) != 0;
				if (s0 == 0 && !(last && s1 != 0))
					return;
				int16_t o0[3], o1[3];
				DarkOffset(s0, a0, da, odd, o0);
				DarkOffset(s1, a1, da, odd, o1);
				const int nv = quad ? 4 : 3;
				for (int j = 0; j < nv; j++)
				{
					U32(F, j * 8) = U32(vv[j], 0);
					U32(F, j * 8 + 4) = U32(vv[j], 4);
					for (int c = 0; c < 3; c++)
						U16(F, j * 8 + c * 2) = (uint16_t)(U16(F, j * 8 + c * 2) + (uint16_t)lerp_i(o0[c], o1[c], num, den));
				}
				const uint32_t g = (uint32_t)lerp_i((int32_t)DarkGrey(s0), (int32_t)DarkGrey(s1), num, den);
				const uint8_t sc = last ? s1 : s0;
				const uint32_t grey = ((g << 8) + g) << 8 | g;
				GTE_LoadV012_Packed(F);
				x::GTE_RTPT();
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(W);
				if (S32(W, 0) <= 0)
					return;
				if (!quad)
				{
					x::GTE_AVSZ3();
					x::GTE_ReadOTZ(W + 4);
					const uint32_t z = U32(W, 4) >> 2;
					GTE_StoreSXY012_PolyFT3(pkt);
					U32(pkt, 0) = 0x7000000;
					U32(pkt, 0x14) = U32(rec, 0xC);
					U32(pkt, 0xC) = U32(rec, 8);
					U16(pkt, 0x1C) = U16(rec, 6);
					U32(pkt, 4) = grey | (sc == 0x13 ? 0x24000000u : 0x26000000u);
					x::SSIGPU_InsertPrimAutoDepth(ot + z * 4, pkt);
					pkt += 0x20;
				}
				else
				{
					GTE_StoreSXY012_PolyFT3_2(pkt);
					x::GTE_LoadV0(F + 0x18);
					x::GTE_RTPS();
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					x::GTE_ReadOTZ(W + 4);
					const uint32_t z = U32(W, 4) >> 2;
					U32(pkt, 0xC) = U32(rec, 4);
					U32(pkt, 0x14) = U32(rec, 8);
					U16(pkt, 0x1C) = U16(rec, 0xC);
					U16(pkt, 0x24) = U16(rec, 0xE);
					U32(pkt, 0) = 0x9000000;
					U32(pkt, 4) = grey | (sc == 0x13 ? 0x2C000000u : 0x2E000000u);
					x::SSIGPU_InsertPrimAutoDepth(ot + z * 4, pkt);
					pkt += 0x28;
				}
			});
			MEM<uint32_t>(U32(ctx, 0x4C)) = pkt;
		}
		x::BattleModel_BuildBoneMatricesFromPose(anim);
	}

	static void ReaperHeld(const Note &n, int num, int den)
	{
		const uint32_t node = n.addr;
		const int8_t st = S8(node, 0x29);   // the state the next tick runs
		const bool dark = (U16(node, 0x26) & 8) != 0;
		const int16_t h0 = MEM<int16_t>(0x2758564);
		const uint16_t a0 = MEM<uint16_t>(0x2758568), da = MEM<uint16_t>(0x275856A);
		int16_t h1 = h0;
		uint16_t a1 = a0;
		bool dark1 = dark;
		if (st == 1)
		{
			h1 = (int16_t)(h0 - 0x48);
			a1 = (uint16_t)((a0 + da) & 0xFFF);
		}
		else if (st == 2)
		{
			a1 = (uint16_t)((a0 + da) & 0xFFF);
			if ((int16_t)(U16(node, 0x134) - 1) <= 0)
				dark1 = false;
		}
		if (dark && (dark1 || num < den))
		{
			DarkHeld(node, n, h0, h1, a0, a1, num, den);
			return;
		}
		// the next tick's creature: its state step run on a copy of the node (animation switches,
		// fade, hide; the skeleton / model data it rewrites come back with the held frame's restore)
		static const uint32_t states[11] = { 0x8CCF80, 0x8CD090, 0x8CD0E0, 0x8CD120, 0x8CD1A0, 0x8CD1D0,
			0x8CD200, 0x8CD250, 0x8CD290, 0x8CD2B0, 0x8CD4A0 };
		alignas(4) static uint8_t cp[0x140];
		// the director's phase the next tick's reaper sees (0x8C5410 runs first: a requested phase
		// +0x42 becomes current +0x40)
		const uint32_t d = MEM<uint32_t>(PHASE_DATA);
		const uint16_t phase = U16(d, 0x40), gate = U16(d, 0x44);
		static uint8_t cam[0x100];
		memcpy(cam, (const void *)0x1D97700, sizeof(cam));
		if (num == den && st >= 1 && st <= 10)
		{
			memcpy(cp, (const void *)node, sizeof(cp));
			U16(d, 0x40) = U16(d, 0x42);
			callp(states[st], P(cp));
			U16(d, 0x40) = phase;
			U16(d, 0x44) = gate;
			memcpy((void *)0x1D97700, cam, sizeof(cam));
			if (U8(P(cp), 0x26) & 4)
				return;                          // hidden (the fade-out ended): released
			MEM<uint32_t>(PACKET_CURSOR) = a_746C10(P(cp), 0x275E838, MEM<uint32_t>(PACKET_CURSOR));
			return;
		}
		// an animation switch on the next tick (states 4..7 start a new animation when they
		// advance): the pose holds until then
		bool hold = false;
		if (st >= 4 && st <= 7)
		{
			memcpy(cp, (const void *)node, sizeof(cp));
			static uint8_t xd0[0x2000];
			memcpy(xd0, (const void *)XDATA[0][0], sizeof(xd0));
			U16(d, 0x40) = U16(d, 0x42);
			callp(states[st], P(cp));
			U16(d, 0x40) = phase;
			U16(d, 0x44) = gate;
			memcpy((void *)0x1D97700, cam, sizeof(cam));
			memcpy((void *)XDATA[0][0], xd0, sizeof(xd0));
			hold = S8(P(cp), 0x29) != st;
		}
		// fade-out state (0x8CD2B0): the fade level half way to the next tick's (when the real tick
		// faded already; on the tick that entered the state the model keeps its colours until then)
		if (st == 9 && S16(node, 0x13A) > 0)
		{
			const int16_t f0 = S16(node, 0x13A);
			int16_t f1 = (int16_t)(f0 + 0x100);
			if (f1 >= 0x1000)
				f1 = 0x1000;
			const int16_t f = (int16_t)lerp_i(f0, f1, num, den);
			S16(node, 0x13A) = f;
			S16(node, 0x13C) = f;
			dm_8CD310(node, 3);
			dm_8CD430(node);
		}
		if (hold)
			x::BattleModel_BuildBoneMatricesFromPose(node + 0x90);
		else
			pose_midpoint((void *)(node + 0x90), (void *)(node + 0x9C), num, den);
		MEM<uint32_t>(PACKET_CURSOR) = a_746C10(node, 0x275E838, MEM<uint32_t>(PACKET_CURSOR));
	}

	// ====================================================================================
	// actor system (as mag048_magma_breath_held.inc / mag030_counter_laser_eye_held.inc)
	// ====================================================================================
	static const uint32_t EMIT_BYTES = 0x6C;
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's motion step of
	// 0x8C9810 run on it): view positions, draw matrix, sub-positions; at fraction 1 the whole
	// next actor. P_ENDS: the actor ends on the next tick (its sprite holds its last draw, its model
	// is not drawn at fraction 1); P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x8CA950 (its end spawns: not called)
		const int32_t kind = S8(def, 0x1E);
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			if (S16(nxt, 0x1C8) < 0) return P_ENDS;
		}
		if ((kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) return P_ENDS;
		a_740910(nxt, def);
		a_741120(nxt, def); // (a morphing model's step rewrites its vertex block: put back by the caller)
		if (U32(nxt, 0x170) != U32(act, 0x170) && num < den) return P_HOLD;
		for (int k = 0; k < 9; k++) // rotation / scale of the draw matrix +0xAC
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
		if (num >= den && S8(nxt, 0x1D8) == S8(cur, 0x1D8) && U32(nxt, 0x170) != 0)
		{
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x8C9810)
		}
		return P_NEXT;
	}

	static void SystemHeld(const Note &n, int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxts[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t verts_next[sizeof(g_sys_verts[0].bytes)];
		MEM<uint32_t>(ACTOR_STATE) = n.addr;
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(n.addr, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(DefOf(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		// the next tick's update pass (0x8C97D0) in list order: the emitters' placement / motion
		// (0x8CAAA0 without its emission and life count, put back after: their particles read them)
		// and the drawn particles' steps (Predict)
		static const int MAX_EMIT = 64;
		static uint8_t esave[MAX_EMIT][EMIT_BYTES];
		uint32_t eaddr[MAX_EMIT];
		int ne = 0, pi = 0;
		int pr[MAX_ACTORS];
		bool morph = false;
		for (uint32_t act = U32(n.addr, 0x2C); act; act = U32(act, 4))
		{
			if (S16(act, 8) == 0)
			{
				if (ne >= MAX_EMIT) continue;
				eaddr[ne] = act;
				memcpy(esave[ne++], (const void *)act, EMIT_BYTES);
				if (U16(act, 0x62) == 0)
				{
					a_742D00(act);
					U16(act, 0x62) = (uint16_t)(U16(act, 0x62) + 1);
				}
				const int32_t mode = S8(act, 0x68);
				if (mode == 3)
					a_7424C0(act);
				else if (mode == 4)
					dm_8CB5A0(act);
			}
			else if (pi < nl && act == list[pi])
			{
				pr[pi] = Predict(list[pi], P(copies[pi]), P(nxts[pi]), num, den);
				if (pr[pi] == P_NEXT && U8(DefOf(list[pi]), 0x16) == 5) morph = true;
				pi++;
			}
		}
		for (int e = ne - 1; e >= 0; e--)
			memcpy((void *)eaddr[e], esave[e], EMIT_BYTES);
		const VertexBlocks *vb = n.verts >= 0 ? &g_sys_verts[n.verts] : nullptr;
		if (morph && vb)
		{
			for (int i = 0; i < nl; i++)
				if (pr[i] == P_NEXT && U8(DefOf(list[i]), 0x16) == 5)
					dm_8CA840(P(nxts[i]), DefOf(list[i]));
			uint32_t used = 0;
			for (uint32_t b = 0; b < vb->n; b++)
			{
				memcpy(verts_next + used, (const void *)vb->addr[b], vb->len[b]);
				used += vb->len[b];
			}
			vb->restore();
			used = 0;
			for (uint32_t b = 0; b < vb->n; b++)
			{
				for (uint32_t o = 0; o < vb->len[b]; o += 2)
					S16(vb->addr[b], o) = (int16_t)lerp_i(S16(vb->addr[b], o), S16(P(verts_next) + used, o), num, den);
				used += vb->len[b];
			}
		}
		else if (vb)
			vb->restore();
		Restore(n);
		for (int i = 0; i < nl; i++)
		{
			const uint32_t c = P(copies[i]);
			if (U8(DefOf(list[i]), 0x16) > 2) continue;
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				dm_8C9750(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					dm_8C9750(c);
				}
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			// (a model that ends on the next tick is not drawn at fraction 1: its draws are
			// untagged, the sprite sequences above hold their last frame)
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(pr[i] == P_ENDS && num >= den))
				dm_8C9530(P(copies[i]), def);
		}
	}

	// ====================================================================================
	// dust sparkles (0x8CD670: 4 sprites; velocity -= velocity / 16, position += velocity / 16)
	// ====================================================================================
	static void SparkleMove(uint32_t node, int k)
	{
		const uint32_t v = node + 0x8E + (uint32_t)k * 8;
		for (int c = -2; c <= 2; c += 2)
		{
			const int16_t x_ = S16(v, c);
			U16(v, c) = (uint16_t)((uint16_t)x_ - (int32_t)x_ / 16);
		}
		U16(v, -0x22) = (uint16_t)(U16(v, -0x22) + (int32_t)S16(v, -2) / 16);
		U16(v, -0x20) = (uint16_t)(U16(v, -0x20) + (int32_t)S16(v, 0) / 16);
		U16(v, -0x1E) = (uint16_t)(U16(v, -0x1E) + (int32_t)S16(v, 2) / 16);
	}

	static void SparkleHeld(const Note &n, int num, int den)
	{
		alignas(4) static uint8_t a[0xB0], b[0xB0], c[0xB0];
		memcpy(a, n.node, 0xB0);                 // before the real tick's moves (after its state step)
		const uint32_t A = P(a), B = P(b), C = P(c);
		for (int k = 0; k < 4; k++)
			SparkleMove(A, k);
		memcpy(b, a, 0xB0);                      // the next tick: its state step, then its moves
		const uint32_t tab[3] = { 0x8CDE40, 0x8CDE60, 0x8CDEB0 };
		callp(tab[S8(B, 0x29)], B);
		for (int k = 0; k < 4; k++)
			SparkleMove(B, k);
		// a sparkle that ends on the next tick (hidden, released) holds its last frame
		if (U8(B, 0x26) & 4)
		{
			memcpy(b, a, 0xB0);
			num = 0;
		}
		const bool last = num == den;
		memcpy(c, last ? b : a, 0xB0);
		for (int k = 0; k < 4; k++)
		{
			const uint32_t pa = A + 0x6C + (uint32_t)k * 8, pb = B + 0x6C + (uint32_t)k * 8;
			U32(C, 0x1C) = U32(last ? pb : pa, 0);
			U32(C, 0x20) = U32(last ? pb : pa, 4);
			for (int j = 0; j < 3; j++)
				S16(C, 0x1C + j * 2) = (int16_t)lerp_i(S16(pa, j * 2), S16(pb, j * 2), num, den);
			dm_8CD770(C);
		}
	}

	// ====================================================================================
	// smoke puffs (0x8CDFB0: state, spin +0x46 += 0x80, draw 0x8CE040)
	// ====================================================================================
	static void PuffHeld(const Note &n, int num, int den)
	{
		alignas(4) static uint8_t b[0x58], c[0x58];
		const uint32_t node = n.addr, B = P(b), C = P(c);
		memcpy(b, (const void *)node, 0x58);
		const uint32_t states[5] = { 0x8CE130, 0x8CE1B0, 0x8CE1E0, 0x8CE200, 0x8CE230 };
		const int8_t st = S8(B, 0x29);
		if (st >= 1 && st <= 4)
			callp(states[st], B);
		U16(B, 0x46) = (uint16_t)((U16(B, 0x46) + 0x80) & 0xFFF);
		// a puff that ends on the next tick holds its last draw
		if ((U8(B, 0x26) & 1) && U8(B, 0x28) == 0)
			memcpy(b, (const void *)node, 0x58);
		memcpy(c, (const void *)node, 0x58);
		S16(C, 0x46) = lerp_angle(S16(node, 0x46), S16(B, 0x46), num, den);
		S16(C, 0x50) = (int16_t)lerp_i(S16(node, 0x50), S16(B, 0x50), num, den);
		dm_8CE040(C);
	}

	// ====================================================================================
	// prim-model plays (0x8C59E0 = a_73C280 on the node) and the clock rings
	// ====================================================================================
	static void PlayHeld(const Note &n, int num, int den)
	{
		const uint32_t node = n.addr;
		alignas(4) static uint8_t cp[0x94];
		memcpy(cp, n.node, 0x94);                // the placement the real tick played with
		const uint32_t N = P(cp);
		// the doom clock's hand (0x8C8050 state 1 = 0x8C8400) moves to its next step's position; at the
		// end of its step table it jumps back to its start (a cut: the play holds until fraction 1)
		bool cut = false;
		if (U32(node, 8) == 0x8C8050 && S8(node, 0x29) == 1)
		{
			alignas(4) static uint8_t nx[8];
			const int16_t step = MEM<int16_t>(0x1636C34 + (uint32_t)((int32_t)(int16_t)(U16(node, 0x456) + 1) * 2));
			if (step >= 0x7F)
			{
				memcpy(nx, (const void *)(node + 0x44C), 8);
				dm_8C8350(P(nx), 0x11);
				cut = true;
			}
			else
			{
				memcpy(nx, (const void *)(node + 0x1C), 8);
				dm_8C8350(P(nx), (uint32_t)(uint16_t)step);
			}
			if (!cut || num == den)
				for (int j = 0; j < 3; j++)
					S16(N, 0x1C + j * 2) = (int16_t)lerp_i(S16(N, 0x1C + j * 2), S16(P(nx), j * 2), cut ? 1 : num, cut ? 1 : den);
		}
		// the parameter block of a_73C280
		alignas(4) static uint8_t fr[0x5C];
		memset(fr, 0, sizeof(fr));
		const uint32_t L = P(fr);
		x::MAG_022_sub_8DD770(L);
		if (U16(N, 0x62) != 0) x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)S16(N, 0x62));
		if (U16(N, 0x60) != 0) x::sub_8DD7E0(L, (uint32_t)(int32_t)S16(N, 0x60));
		if (U16(N, 0x64) != 0) x::sub_8DD960(L, (uint32_t)(int32_t)S16(N, 0x64));
		x::scale3DMatrix(L, N + 0x50);
		S32(L, 0x14) = S16(N, 0x1C);
		S32(L, 0x18) = S16(N, 0x1E);
		S32(L, 0x1C) = S16(N, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(N, 0x86);
		U32(L, 0x44) = U32(N, 0x7C);
		U16(L, 0x4C) = U16(N, 0x8C);
		U16(L, 0x4E) = U16(N, 0x8E);
		U32(L, 0x38) = U32(N, 0x70);
		U16(L, 0x52) = U16(N, 0x92);
		U16(L, 0x56) = U16(N, 0x80);
		U16(L, 0x50) = U16(N, 0x90);
		U32(L, 0x48) = G(G_258EF98);
		U16(L, 0x58) = U16(N, 0x88);
		void *p = port_of(0x8C5B10);
		prim::play_held((prim::Layout *)(node + 0x94), (prim::Callback)(p ? P(p) : 0x8C5B10), (int)L, cut && num < den ? 0 : num, den);
	}

	static void RingsHeld(const Note &n, int num, int den)
	{
		const uint32_t node = n.addr;
		const uint32_t b = MEM<uint32_t>(CLOCK_BLOCK);
		int16_t step = n.ring_step;
		if (num == den)
		{
			// the next tick draws its next step when the clock task still runs a ring state
			const int8_t st = S8(node, 0x29);
			if (st < 1 || st > 3)
				return;
			step = S16(b, 0x54);
		}
		uint8_t save[8];
		memcpy(save, (const void *)(b + 0x50), 8);
		S16(b, 0x54) = step;
		dm_8C84A0();
		memcpy((void *)(b + 0x50), save, 8);
	}

	// ====================================================================================
	// screen tints (0x8DDC30 state 1 = 0x8DDCD0: colour script step, four semi-transparent flat
	// quads over the screen + a draw mode into OT base + 0x1C)
	// ====================================================================================
	// the colour step of 0x8DDCD0 on node a (no draw); false = the script ended (nothing drawn)
	static bool TintStep(uint32_t a)
	{
		const uint16_t idx = U16(a, 0x3A);
		const uint32_t script = U32(a, 0x34);
		const uint32_t ent = U32(script, (int32_t)(int16_t)idx * 4);
		const uint8_t op = (uint8_t)(ent >> 24), b2 = (uint8_t)(ent >> 16);
		if (op == 0xFF)
		{
			U8(a, 0x26) |= 1;
			return false;
		}
		if (op == 0xFE)
		{
			if (S16(a, 0x24) >= (int16_t)(uint16_t)b2 && MEM<uint32_t>(0x2795914) == 0)
			{
				const uint16_t ni = (uint16_t)(idx + 1);
				U16(a, 0x3A) = ni;
				U32(a, 0x30) = U32(script, (int32_t)(int16_t)ni * 4);
			}
		}
		else
		{
			U32(a, 0x30) = ent;
			if (MEM<uint32_t>(0x2795914) == 0)
				U16(a, 0x3A) = (uint16_t)(idx + 1);
		}
		return true;
	}
	static void TintDraw(uint32_t colour)
	{
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x1C;
		static const uint32_t xy[4][4] = {
			{ 0, 0xA0, 0x780000, 0x7800A0 }, { 0xA0, 0x140, 0x7800A0, 0x780140 },
			{ 0x780000, 0x7800A0, 0xF00000, 0xF000A0 }, { 0x7800A0, 0x780140, 0xF000A0, 0xF00140 } };
		uint32_t pkt = MEM<uint32_t>(PACKET_CURSOR);
		for (int q = 0; q < 4; q++)
		{
			U32(pkt, 4) = colour;
			U32(pkt, 0) = 0x5000000;
			U8(pkt, 7) = 0x2A;
			for (int v = 0; v < 4; v++)
				U32(pkt, 8 + v * 4) = xy[q][v];
			x::SSIGPU_InsertPrimAltViewport(ot, pkt);
			pkt += 0x18;
		}
		const uint32_t tpage = x::sub_45C690(0, 1, 0x280, 0) & 0xFFFF;
		x::sub_45BFC0(pkt, 0, 0, tpage, 0);
		x::SSIGPU_InsertPrimAltViewport(ot, pkt);
		MEM<uint32_t>(PACKET_CURSOR) = pkt + 0xC;
	}
	static void TintHeld(int num, int den)
	{
		alignas(4) static uint8_t a[0x40], b[0x40];
		const uint32_t A = P(a), B = P(b);
		for (int i = 0; i < g_ntint; i++)
		{
			// the real tick: state 1 drew (unless its script ended)
			memcpy(a, g_tint[i], 0x40);
			const bool drew = S8(A, 0x29) == 1 && TintStep(A);
			// the next tick: the node as the real tick left it (released when it finished)
			bool drew1 = false;
			if (!(U8(A, 0x26) & 1) && S8(A, 0x29) == 1)
			{
				memcpy(b, (const void *)g_tint_addr[i], 0x40);
				drew1 = S8(B, 0x29) == 1 && TintStep(B);
			}
			else if (!(U8(A, 0x26) & 1) && S8(A, 0x29) == 0)
			{
				// its delay state ran on the real tick: it may start drawing on the next
				memcpy(b, (const void *)g_tint_addr[i], 0x40);
				drew1 = S8(B, 0x29) == 1 && TintStep(B);
			}
			if (num == den)
			{
				if (drew1) TintDraw(U32(B, 0x30));
				continue;
			}
			if (!drew) continue;
			uint32_t col = U32(A, 0x30);
			if (drew1)
			{
				const uint32_t next = U32(B, 0x30);
				uint32_t c = col & 0xFF000000;
				for (int k = 0; k < 3; k++)
				{
					const int32_t x0 = (int32_t)((col >> (8 * k)) & 0xFF), x1 = (int32_t)((next >> (8 * k)) & 0xFF);
					c |= (uint32_t)(lerp_i(x0, x1, num, den) & 0xFF) << (8 * k);
				}
				col = c;
			}
			TintDraw(col);
		}
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_008;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[SCRATCH_HI - SCRATCH_LO], gte[GTE_D_N + GTE_C_N], falloc[0x1000], shadow[0x80];
		static uint8_t xdata[3][0xE000];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		for (int i = 0; i < 3; i++)
			memcpy(xdata[i], (const void *)XDATA[i][0], XDATA[i][1] - XDATA[i][0]);
		memcpy(scratch, (const void *)SCRATCH_LO, sizeof(scratch));
		memcpy(gte, (const void *)GTE_D, GTE_D_N);
		memcpy(gte + GTE_D_N, (const void *)GTE_C, GTE_C_N);
		memcpy(falloc, (const void *)fa, sizeof(falloc));
		static VertexBlocks verts_now[MAX_SYSTEMS];
		for (int i = 0; i < g_nnotes; i++)
			if (g_notes[i].verts >= 0) verts_now[g_notes[i].verts].save(g_notes[i].addr);
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		const uint32_t shadow_words[2] = { MEM<uint32_t>(0xB8B9E4), MEM<uint32_t>(0xB8B9EC) };
		const uint32_t geom[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) };
		const uint32_t pc = MEM<uint32_t>(PACKET_CURSOR), fc = MEM<uint32_t>(FA_CURSOR);
		{
			const uint32_t used = pc - g_pkt_start;
			if (g_pkt_start && pc > g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
			const uint32_t used2 = fc - g_fa_start;
			if (g_fa_start && fc > g_fa_start && used2 < sizeof(g_held_fa)) memcpy(g_held_fa, (const void *)g_fa_start, used2);
		}
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		MEM<uint32_t>(FA_CURSOR) = P(g_held_fa);
		for (int i = 0; i < g_nnotes; i++)
		{
			const Note &n = g_notes[i];
			Restore(n);
			switch (n.kind)
			{
			case K_REAPER: ReaperHeld(n, num, den); break;
			case K_SYSTEM: SystemHeld(n, num, den); break;
			case K_SPARKLE: SparkleHeld(n, num, den); break;
			case K_PUFF: PuffHeld(n, num, den); break;
			case K_PLAY: PlayHeld(n, num, den); break;
			case K_RINGS: RingsHeld(n, num, den); break;
			case K_TINT: TintHeld(num, den); break;
			}
		}
		MEM<uint32_t>(PACKET_CURSOR) = pc;
		MEM<uint32_t>(FA_CURSOR) = fc;
		MEM<uint32_t>(0x1D99C18) = geom[0];
		MEM<uint32_t>(0x1D99C1C) = geom[1];
		MEM<uint32_t>(0xB8B9E4) = shadow_words[0];
		MEM<uint32_t>(0xB8B9EC) = shadow_words[1];
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		for (int i = g_nnotes - 1; i >= 0; i--)
			if (g_notes[i].verts >= 0) verts_now[g_notes[i].verts].restore();
		memcpy((void *)fa, falloc, sizeof(falloc));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)GTE_D, gte, GTE_D_N);
		memcpy((void *)GTE_C, gte + GTE_D_N, GTE_C_N);
		memcpy((void *)SCRATCH_LO, scratch, sizeof(scratch));
		for (int i = 0; i < 3; i++)
			memcpy((void *)XDATA[i][0], xdata[i], XDATA[i][1] - XDATA[i][0]);
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag008_held()
	{
		// the drawing tasks (the engine task bodies 0x8C58B0 / 0x8C8050 / 0x8C8790 / 0x8C88D0 run
		// this module's states)
		static const uint32_t HELD[] = { act::doom::ORIG_Reaper, 0x8C88D0, 0x8CD670, 0x8CDFB0, 0x8C58B0, 0x8C8050, 0x8C8790, 0x8DDC30, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(8, act::doom::HeldReady, act::doom::HeldFrame);
	}
}
