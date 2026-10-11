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

// 30 fps part of mag079_ultimecia_final_death.cpp: the held-frame drawing (included at the end of
// mag079_ultimecia_final_death.cpp)
//
// Every drawing task of the effect updates THEN draws in the same tick. The real tick notes each draw
// in queue order (actor systems, caster model copy, sparks, debris, flakes, prim-model plays) with the
// GTE state, the module scratch stack and the Field_Alloc scratch it started from; the held frame
// replays them in the same order, each at tick + num/den:
//   Actor systems - as Doom (mag008_doom_held.inc): each drawn actor from a copy whose view positions /
//                   draw matrix are half way to the next tick's (the next motion step on a second copy;
//                   the whole next actor at fraction 1); an emitter not initialised yet (+0x62 == 0:
//                   its init may draw rand()) is not stepped.
//   Model copy    - the caster's model morphing into a ball (0x7BD020 / 0x7BD110): the caster at its
//                   in-between pose (pose_midpoint); the block's pivot, angles, morph weight, radii and
//                   the caster's matrix translation and colour half way to the next tick's (the next
//                   state step of the caster-model task run on copies: motion 0x7BDBE0, pivot 0x7BD9B0 at
//                   the next pose, morph / radii / colour steps; its spawns, sounds and target writes
//                   are not run); a copy that ends on the next tick holds its last draw and is not drawn
//                   at fraction 1.
//   Sparks / debris - 2 sprites per task: positions half way along the next tick's motion (sparks:
//                   position += velocity / 16; debris: vy += 0x100, velocity -= velocity / 8, position
//                   += velocity / 16); the flipbook frame steps at fraction 1; a part the next tick
//                   hides holds its last draw.
//   Flakes        - static sprites: the flipbook frame steps at fraction 1.
//   Prim plays    - prim::play_held with the node's placement; the plays that follow the model copy's
//                   anchor (tasks 0x7B80B0 / 0x7BDAD0) move half way to the next tick's anchor.
// The packets go to private buffers (module sprite cursor 0x25DF33C and frame arena 0x1D8E054) that
// start with the real tick's packets; the module globals, the exe data the effect rewrites (actor data,
// sequence files), the caster's skeleton, the module scratch stack, the Field_Alloc scratch, the shadow
// / RenderGeometry temporaries and the GTE registers are put back. The master, the director and the
// light tasks (0x7BCD50 / 0x7BCE40) draw nothing.

namespace ff8fx
{
namespace act
{
namespace udeath
{
	static const uint32_t SCRATCH_LO = 0x2793B58, SCRATCH_HI = 0x2793E78; // module scratch stack + camera copy
	static const uint32_t BSS_LO = 0x25DF338, BSS_HI = 0x25EB530;
	// exe data the effect rewrites in place (load script, actor data, prim-model sequence files)
	static const uint32_t XDATA[3][2] = { { 0x1581F00, 0x1582000 }, { 0x1762000, 0x176F000 }, { 0x19F1000, 0x19F7000 } };
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const uint32_t ACTOR_BYTES = 0x2A0;
	static const uint32_t EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 160;
	static const uint32_t NODE_BYTES = 0xB4;
	static const uint32_t BLK_BYTES = 0xD4, ENT_BYTES = 0x9C;
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

	enum { K_SYSTEM, K_PLAY, K_SPARK, K_DEBRIS, K_FLAKE, K_MODEL };
	struct Note
	{
		int kind;
		uint32_t addr;        // node / actor system state
		uint8_t node[NODE_BYTES];
		uint8_t gte[GTE_D_N + GTE_C_N];
		uint8_t scratch[SCRATCH_HI - SCRATCH_LO];
		uint8_t falloc[0x400];
		int verts;
	};
	static const int MAX_NOTES = 160;
	static Note g_notes[MAX_NOTES];
	static int g_nnotes = 0;
	static uint32_t g_note_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0, g_fa_start = 0;
	static uint8_t g_held_packets[0x40000], g_held_fa[0x60000];

	// the caster-model task's draw of this real tick (one task)
	struct ModelNote
	{
		uint32_t node;
		uint8_t blk[BLK_BYTES];   // the block as drawn
		uint8_t ent[ENT_BYTES];   // the caster entity as drawn (matrix translation +0x54.., colour +0x28)
	};
	static ModelNote g_model;
	static uint32_t g_model_tick = 0xFFFFFFFF;

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
		if (g_nnotes >= MAX_NOTES) return nullptr;
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
	static void held_note_system(uint32_t sys)
	{
		Note *n = held_note(K_SYSTEM, sys, 0);
		if (n && g_nsys_verts < MAX_SYSTEMS)
		{
			n->verts = g_nsys_verts;
			g_sys_verts[g_nsys_verts++].save(sys);
		}
	}
	static void held_note_play(uint32_t node) { held_note(K_PLAY, node, 0x94); }
	static void held_note_spark(uint32_t node) { held_note(K_SPARK, node, NODE_BYTES); }
	static void held_note_debris(uint32_t node) { held_note(K_DEBRIS, node, NODE_BYTES); }
	static void held_note_flake(uint32_t node) { held_note(K_FLAKE, node, NODE_BYTES); }
	static void held_note_model(uint32_t node)
	{
		if (!held_note(K_MODEL, node, NODE_BYTES)) return;
		g_model.node = node;
		memcpy(g_model.blk, (const void *)MODEL_BLOCK, BLK_BYTES);
		memcpy(g_model.ent, (const void *)(0x1D972C0 + (uint32_t)U8(node, 0x2C) * 0x9C), ENT_BYTES);
		g_model_tick = g_real_tick;
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
	// actor systems (as mag008_doom_held.inc: Doom's actor-system code is this module's)
	// ====================================================================================
	static uint32_t DefOf(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(ACTOR_STATE), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { P_HOLD, P_NEXT, P_ENDS };
	// the actor's copy `cur` half way to the next tick's `nxt` (the next tick's motion step of
	// 0x7B9200 run on it): view positions, draw matrix, sub-positions; at fraction 1 the whole next
	// actor. P_ENDS: the actor ends on the next tick (its sprite holds its last draw, its model is not
	// drawn at fraction 1); P_HOLD: not in its motion phase (held)
	static int Predict(uint32_t act, uint32_t cur, uint32_t nxt, int num, int den)
	{
		memcpy((void *)cur, (const void *)act, ACTOR_BYTES);
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = DefOf(act);
		if (S16(act, 0x1CC) != 0) return P_HOLD;
		a_741050(nxt, def);
		// end test of 0x7BA340 (its follow-up spawn 0x7BA3C0: not called)
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
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (0x7B9200)
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
		// the next tick's update pass (0x7B91C0) in list order: the emitters' placement / motion
		// (0x7BA490 without its emission and life count, put back after: their particles read them)
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
					continue; // (its init 0x7BB680 may draw rand(): not stepped)
				const int32_t mode = S8(act, 0x68);
				if (mode == 3)
					a_7424C0(act);
				else if (mode == 4)
					ud_7BAF90(act);
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
					ud_7BA230(P(nxts[i]), DefOf(list[i]));
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
		// sprite sequences (0x7B9080: one draw, or one per sub-position)
		for (int i = 0; i < nl; i++)
		{
			const uint32_t c = P(copies[i]);
			if (U8(DefOf(list[i]), 0x16) > 2) continue;
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				ud_7B9140(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					ud_7B9140(c);
				}
			}
		}
		// models (0x7B8EC0)
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = DefOf(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(pr[i] == P_ENDS && num >= den))
				ud_7B8F20(P(copies[i]), def);
		}
	}

	// ====================================================================================
	// sparks (0x7BBDF0), debris (0x7BC840), flakes (0x7BCC80): sprite tasks updated then drawn
	// ====================================================================================
	// spark motion of the real tick (position += velocity / 16)
	static void SparkMove(uint32_t node)
	{
		uint32_t c = node + 0x6C;
		for (int i = 0; i < 2; i++)
		{
			const int32_t vx = S16(c, 0x20);
			c += 8;
			U16(c, -8) = (uint16_t)(U16(c, -8) + (uint16_t)(vx / 16));
			const int32_t vy = S16(c, 0x1A);
			U16(c, -6) = (uint16_t)(U16(c, -6) + (uint16_t)(vy / 16));
			const int32_t vz = S16(c, 0x1C);
			U16(c, -4) = (uint16_t)(U16(c, -4) + (uint16_t)(vz / 16));
		}
	}
	// debris motion of the real tick (vy += 0x100, velocity -= velocity / 8, position += velocity / 16)
	static void DebrisMove(uint32_t node)
	{
		uint32_t c = node + 0x8E;
		for (int i = 0; i < 2; i++)
		{
			U16(c, 0) = (uint16_t)(U16(c, 0) + 0x100);
			int16_t v = S16(c, -2);
			v = (int16_t)(v - (int16_t)((int32_t)v / 8));
			c += 8;
			U16(c, -0xA) = (uint16_t)v;
			v = S16(c, -8);
			v = (int16_t)(v - (int16_t)((int32_t)v / 8));
			U16(c, -8) = (uint16_t)v;
			v = S16(c, -6);
			v = (int16_t)(v - (int16_t)((int32_t)v / 8));
			U16(c, -6) = (uint16_t)v;
			U16(c, -0x2A) = (uint16_t)(U16(c, -0x2A) + (uint16_t)((int32_t)S16(c, -0xA) / 16));
			U16(c, -0x28) = (uint16_t)(U16(c, -0x28) + (uint16_t)((int32_t)S16(c, -8) / 16));
			U16(c, -0x26) = (uint16_t)(U16(c, -0x26) + (uint16_t)((int32_t)S16(c, -6) / 16));
		}
	}

	static void SpriteHeld(const Note &n, int num, int den)
	{
		alignas(4) static uint8_t a[NODE_BYTES], b[NODE_BYTES], c[NODE_BYTES];
		memcpy(a, n.node, NODE_BYTES);           // after the real tick's state step, before its moves
		const uint32_t A = P(a), B = P(b), C = P(c);
		static const uint32_t SPARK[3] = { 0x7BC580, 0x7BC5A0, 0x7BC5F0 };
		static const uint32_t DEBRIS[3] = { 0x7BCAA0, 0x7BCAC0, 0x7BCAE0 };
		static const uint32_t FLAKE[3] = { 0x7BCCF0, 0x7BCD10, 0x7BCD30 };
		const uint32_t *tab = n.kind == K_SPARK ? SPARK : n.kind == K_DEBRIS ? DEBRIS : FLAKE;
		if (n.kind == K_SPARK) SparkMove(A);
		else if (n.kind == K_DEBRIS) DebrisMove(A);
		memcpy(b, a, NODE_BYTES);                // the next tick: its state step, then its moves
		callp(tab[S8(B, 0x29)], B);
		if (n.kind == K_SPARK) SparkMove(B);
		else if (n.kind == K_DEBRIS) DebrisMove(B);
		// a part the next tick hides (+0x26 bit 2: its sequence ended) holds its last draw
		if (U8(B, 0x26) & 4)
		{
			memcpy(b, a, NODE_BYTES);
			num = 0;
		}
		const bool last = num == den;
		memcpy(c, last ? b : a, NODE_BYTES);
		if (n.kind == K_FLAKE)
		{
			U32(C, 0x1C) = U32(C, 0x6C);
			U32(C, 0x20) = U32(C, 0x70);
			ud_7BBEB0(C);
			return;
		}
		for (int k = 0; k < 2; k++)
		{
			const uint32_t pa = A + 0x6C + (uint32_t)k * 8, pb = B + 0x6C + (uint32_t)k * 8;
			U32(C, 0x1C) = U32(last ? pb : pa, 0);
			U32(C, 0x20) = U32(last ? pb : pa, 4);
			for (int j = 0; j < 3; j++)
				S16(C, 0x1C + j * 2) = (int16_t)lerp_i(S16(pa, j * 2), S16(pb, j * 2), num, den);
			if (n.kind == K_SPARK)
				ud_7BBEB0(C);
			else
				ud_7BC950(C, 0x2000);
		}
	}

	// ====================================================================================
	// the caster's model copy (0x7BCF10 -> 0x7BD020 / 0x7BD110)
	// ====================================================================================
	struct ModelNext
	{
		bool valid, drawn;
		uint8_t blk[BLK_BYTES];
		int32_t tr[3];       // the caster's matrix translation the next tick draws with
		uint8_t col[3];      // the caster's colour the next tick draws with (+0x28)
	};
	static ModelNext g_mnext;

	static uint32_t ModelEntity(uint32_t node) { return 0x1D972C0 + (uint32_t)U8(node, 0x2C) * 0x9C; }
	// the caster's skeleton (bone records rebuilt by 0x508C90; pose_midpoint changes them)
	static uint32_t SkelOf(uint32_t ent) { const uint32_t m = U32(ent, 0x64); return m ? U32(m, 0) : 0; }
	static uint32_t SkelBytes(uint32_t sk) { return sk ? 16 + 48 * (uint32_t)U8(sk, 0) : 0; }
	// the caster's next pose exactly as the next tick has it: the battle loop's animation step (the engine reader
	// 0x508F90 on the animation command +0x6C, then the bone matrices 0x508C90); the caller puts the skeleton and the
	// entity back (pose_midpoint's look-ahead keeps the real tick's command for its build and differs from it here)
	static void EnginePoseNext(uint32_t ent)
	{
		if (U8(ent, 0x72) < U8(ent, 0x73))
			fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x508F90)(ent + 0x60, ent + 0x6C);
		x::BattleModel_BuildBoneMatricesFromPose(ent + 0x60);
	}
	static uint8_t g_skel_save[16 + 48 * 256];

	// the next tick's state step of the caster-model task on copies (its pure parts: block motion
	// 0x7BDBE0, pivot 0x7BD9B0 at the next pose, morph / radii / colour steps; no spawn, sound or
	// target write) -> g_mnext; everything it touched is put back
	static void ModelPredict()
	{
		g_mnext.valid = false;
		if (g_model_tick != g_real_tick) return;
		const uint32_t node = g_model.node;
		const uint32_t ent = ModelEntity(node);
		const uint32_t sk = SkelOf(ent);
		const uint32_t skn = SkelBytes(sk);
		if (skn > sizeof(g_skel_save)) return;
		alignas(4) static uint8_t blk_save[BLK_BYTES], ent_save[ENT_BYTES], nc[NODE_BYTES], ec[ENT_BYTES];
		memcpy(blk_save, (const void *)MODEL_BLOCK, BLK_BYTES);
		memcpy(ent_save, (const void *)ent, ENT_BYTES);
		if (sk) memcpy(g_skel_save, (const void *)sk, skn);
		const uint16_t w770c = MEM<uint16_t>(0x1D9770C);
		memcpy(nc, (const void *)node, NODE_BYTES);
		memcpy(ec, (const void *)ent, ENT_BYTES);
		const uint32_t NC = P(nc), EC = P(ec);
		// the next pose (the bones the next tick's pivot reads)
		EnginePoseNext(ent);
		switch (S8(NC, 0x29))
		{
		case 2: // 0x7BDA60
			ud_7BDBE0(NC, S16(NC, 0xB0) < 0xC ? 1u : 0u);
			ud_7BD9B0(NC, MODEL_BLOCK);
			break;
		case 3: // 0x7BDCB0
		{
			ud_7BDBE0(NC, 1);
			ud_7BD9B0(NC, MODEL_BLOCK);
			const uint16_t m = (uint16_t)(MEM<uint16_t>(0x25DF3E8) + 0x80);
			MEM<uint16_t>(0x25DF3E8) = (int16_t)m >= 0x1000 ? (uint16_t)0x1000 : m;
			break;
		}
		case 4: // 0x7BDD00
		case 5: // 0x7BDD40
			ud_7BDBE0(NC, 1);
			ud_7BD9B0(NC, MODEL_BLOCK);
			break;
		case 6: // 0x7BDD70
			ud_7BD9B0(NC, MODEL_BLOCK);
			break;
		case 8: // 0x7BDF90: the caster's colour level +0xAE, radii y / z = x, hidden at the end
		{
			U16(NC, 0xAE) = (uint16_t)(U16(NC, 0xAE) + 0x200);
			if (S16(NC, 0xAE) >= 0x1000)
				U16(NC, 0xAE) = 0x1000;
			a_733950(EC, (uint32_t)U16(NC, 0xAE), 0);
			const uint16_t sx = MEM<uint16_t>(0x25DF3EA);
			MEM<uint16_t>(0x25DF3EE) = sx;
			MEM<uint16_t>(0x25DF3EC) = sx;
			if ((int32_t)(MEM<uint32_t>(0x25DF3FC) + 0x100) >= 0x1000)
				U8(NC, 0x26) |= 4;
			break;
		}
		default:
			break;
		}
		const uint8_t fl = U8(NC, 0x26);
		g_mnext.drawn = (fl & 8) != 0 && (fl & 4) == 0;
		memcpy(g_mnext.blk, (const void *)MODEL_BLOCK, BLK_BYTES);
		// 0x7BCF10: the caster's position words = anchor - pivot; +0x54.. = the sign-extended words
		g_mnext.tr[0] = (int16_t)(MEM<uint32_t>(0x25DF3C0) - MEM<uint32_t>(0x25DF3B8));
		g_mnext.tr[1] = (int16_t)(uint16_t)(MEM<uint16_t>(0x25DF3C2) - MEM<uint16_t>(0x25DF3BA));
		g_mnext.tr[2] = (int16_t)(MEM<uint32_t>(0x25DF3C4) - MEM<uint32_t>(0x25DF3BC));
		memcpy(g_mnext.col, ec + 0x28, 3);
		g_mnext.valid = true;
		// put back
		MEM<uint16_t>(0x1D9770C) = w770c;
		if (sk) memcpy((void *)sk, g_skel_save, skn);
		memcpy((void *)ent, ent_save, ENT_BYTES);
		memcpy((void *)MODEL_BLOCK, blk_save, BLK_BYTES);
	}

	// 0x7BD020 without its bone builds (the caller set the pose)
	static void ModelDraw(uint32_t blk)
	{
		const int32_t idx = S16(blk, 0xA8);
		const uint32_t ent = 0x1D972C0 + (uint32_t)(idx * 0x9C);
		memcpy((void *)(blk + 0x20), (const void *)(ent + 0x40), 0x20);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x20);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x40);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x22);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x42);
		x::GTE_LoadIRFromMatrixColumn(blk + 0x24);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0x44);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(blk + 0x34);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(blk + 0x54);
		ud_7BD110(U32(ent, 0x64), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, blk);
		const uint32_t part2 = U32(ent, 0x78);
		if (part2 != 0)
			ud_7BD110(U32(part2, 4), MEM<uint32_t>(0x1D8E04C) + 0x44, 4, blk);
	}

	static void ModelHeld(const Note &n, int num, int den)
	{
		if (g_model_tick != g_real_tick || !g_mnext.valid) return;
		const bool ends = !g_mnext.drawn;
		if (ends && num >= den) return; // the next tick does not draw it
		const uint32_t node = n.addr;
		const uint32_t ent = ModelEntity(node);
		const uint32_t sk = SkelOf(ent);
		const uint32_t skn = SkelBytes(sk);
		alignas(4) static uint8_t blk_save[BLK_BYTES], ent_save[ENT_BYTES];
		memcpy(blk_save, (const void *)MODEL_BLOCK, BLK_BYTES);
		memcpy(ent_save, (const void *)ent, ENT_BYTES);
		if (sk) memcpy(g_skel_save, (const void *)sk, skn);
		// the block and the caster as drawn on the real tick
		memcpy((void *)MODEL_BLOCK, g_model.blk, BLK_BYTES);
		memcpy((void *)ent, g_model.ent, ENT_BYTES);
		if (ends)
			x::BattleModel_BuildBoneMatricesFromPose(ent + 0x60); // held: the real tick's pose
		else
		{
			const uint32_t B = MODEL_BLOCK, Q = P(g_mnext.blk);
			for (int k = 0; k < 3; k++) // pivot +0x70
				S16(B, 0x70 + 2 * k) = (int16_t)lerp_i(S16(B, 0x70 + 2 * k), S16(Q, 0x70 + 2 * k), num, den);
			for (int k = 0; k < 3; k++) // angles +0x98 / +0x9A / +0x9C (the short way round)
				S16(B, 0x98 + 2 * k) = lerp_angle(S16(B, 0x98 + 2 * k), S16(Q, 0x98 + 2 * k), num, den);
			for (int k = 0; k < 4; k++) // morph weight +0xA0, radii +0xA2..+0xA6
				S16(B, 0xA0 + 2 * k) = (int16_t)lerp_i(S16(B, 0xA0 + 2 * k), S16(Q, 0xA0 + 2 * k), num, den);
			S16(B, 0xD0) = (int16_t)lerp_i(S16(B, 0xD0), S16(Q, 0xD0), num, den); // depth bias
			for (int k = 0; k < 3; k++) // the caster's matrix translation
				S32(ent, 0x54 + 4 * k) = lerp_i(S32(ent, 0x54 + 4 * k), g_mnext.tr[k], num, den);
			for (int k = 0; k < 3; k++) // its colour
				U8(ent, 0x28 + k) = (uint8_t)lerp_i(U8(ent, 0x28 + k), g_mnext.col[k], num, den);
			if (num >= den)
				EnginePoseNext(ent); // fraction 1: the next tick's pose
			else
				pose_midpoint((void *)(ent + 0x60), (void *)(ent + 0x6C), num, den);
		}
		ModelDraw(MODEL_BLOCK);
		if (sk) memcpy((void *)sk, g_skel_save, skn);
		memcpy((void *)ent, ent_save, ENT_BYTES);
		memcpy((void *)MODEL_BLOCK, blk_save, BLK_BYTES);
	}

	// ====================================================================================
	// prim-model plays (0x7B5C50 = a_73C280 on the node)
	// ====================================================================================
	static void PlayHeld(const Note &n, int num, int den)
	{
		const uint32_t node = n.addr;
		alignas(4) static uint8_t cp[0x94];
		memcpy(cp, n.node, 0x94);                // the placement the real tick played with
		const uint32_t N = P(cp);
		// the plays that follow the model copy's anchor (0x7B8110 / 0x7B8170, 0x7BDB30 / 0x7BDB90) move
		// to the next tick's anchor
		const uint32_t fn = U32(node, 8);
		if (fn == 0x7B80B0 || fn == 0x7BDAD0)
		{
			const uint32_t nb = g_mnext.valid ? P(g_mnext.blk) : MODEL_BLOCK;
			for (int j = 0; j < 3; j++)
				S16(N, 0x1C + j * 2) = (int16_t)lerp_i(S16(N, 0x1C + j * 2), S16(nb, 0x78 + j * 2), num, den);
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
		void *p = port_of(0x7B5D80);
		prim::play_held((prim::Layout *)(node + 0x94), (prim::Callback)(p ? P(p) : 0x7B5D80), (int)L, num, den);
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady()) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_079;
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
		ModelPredict();
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);
		MEM<uint32_t>(FA_CURSOR) = P(g_held_fa);
		for (int i = 0; i < g_nnotes; i++)
		{
			const Note &n = g_notes[i];
			Restore(n);
			switch (n.kind)
			{
			case K_SYSTEM: SystemHeld(n, num, den); break;
			case K_PLAY: PlayHeld(n, num, den); break;
			case K_SPARK:
			case K_DEBRIS:
			case K_FLAKE: SpriteHeld(n, num, den); break;
			case K_MODEL: ModelHeld(n, num, den); break;
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
	static void register_mag079_held()
	{
		// the drawing tasks: the module's own (sparks, debris, flakes, caster model copy) and the engine
		// task bodies running this module's states (actor systems 0x7B82C0, prim-model sequences
		// 0x7B5B80 / 0x7B80B0 / 0x7B81C0 / 0x7BDAD0)
		static const uint32_t HELD[] = { 0x7BBDF0, 0x7BC840, 0x7BCC80, 0x7BCF10, 0x7B82C0, 0x7B5B80, 0x7B80B0, 0x7B81C0, 0x7BDAD0, 0 };
		for (const uint32_t *h = HELD; *h; h++)
			mark_held(*h);
		register_module_held(79, act::udeath::HeldReady, act::udeath::HeldFrame);
	}
}
