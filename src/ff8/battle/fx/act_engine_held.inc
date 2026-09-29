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

// 30 fps part of act_engine.cpp: the held-frame core of the actor engine family - notes of the
// real tick (master ran, camera stepper ran, prim-model plays), the held-frame camera (the next
// camera step predicted on saved state), the held-frame redraw of a module (prim plays, creature
// actors at their midpoint pose, actor systems, node tasks, screen tints, the module's own draws)
// and the held-task registration
// (included at the end of act_engine.cpp)

namespace ff8fx
{
namespace act
{
	// ------------------------------------------------------------------------------------
	// held frames (30 fps)
	// ------------------------------------------------------------------------------------
	// real tick on which a ported master last ran (held frames need their memos)
	static uint32_t g_ported_tick = 0xFFFFFFFF;

	// master tasks (a_739F40, m_731F70, t_7624D0, b_729BD0): this real tick ran the ported effect
	void held_note_master() { g_ported_tick = g_real_tick; }

	// camera: the engine's two camera steppers (a_73AE10 interpolated move, a_73B4B0 keyframe
	// tracks) both end in a_73B160, which writes the battle camera words; they note here that
	// they ran on this real tick
	static uint32_t g_cam_tick = 0xFFFFFFFF;
	static int g_cam_kind = 0; // 1 = a_73AE10, 2 = a_73B4B0

	// the camera roll (0x1D977A2) before the tick's first camera step: the sequence sprites of the
	// actor systems (0x8DD260 reads the roll) drew with it when they ran before the step
	static int16_t g_roll_pre = 0;

	void held_note_camera(int kind)
	{
		if (g_cam_tick != g_real_tick) g_roll_pre = MEM<int16_t>(0x1D977A2);
		g_cam_tick = g_real_tick; // held-frame camera: this stepper ran on this tick
		g_cam_kind = kind;
	}

	// saves / restores everything a camera step writes: the module's camera block (0xEC bytes
	// at *G_15339D0), the camera words, projection distance, 0x1D977A2 and the GTE registers
	struct CamSave
	{
		uint8_t block[0xEC];
		uint8_t words[0x10];
		uint16_t proj_h, w977a2;
		uint8_t gte[0x1CA9300 - 0x1CA8A10];
		uint32_t fa;
		uint8_t scratch[0x1000];
	};
	static void cam_save(CamSave &s, uint32_t blk)
	{
		memcpy(s.block, (const void *)blk, sizeof(s.block));
		memcpy(s.words, (const void *)0xB8B7F0, sizeof(s.words));
		s.proj_h = MEM<uint16_t>(0x1D8E038);
		s.w977a2 = MEM<uint16_t>(0x1D977A2);
		memcpy(s.gte, (const void *)0x1CA8A10, sizeof(s.gte));
		s.fa = MEM<uint32_t>(0x1D999C4); // the matrix helpers use the Field_Alloc scratch
		memcpy(s.scratch, (const void *)s.fa, sizeof(s.scratch));
	}
	static void cam_restore(const CamSave &s, uint32_t blk)
	{
		memcpy((void *)blk, s.block, sizeof(s.block));
		memcpy((void *)0xB8B7F0, s.words, sizeof(s.words));
		MEM<uint16_t>(0x1D8E038) = s.proj_h;
		MEM<uint16_t>(0x1D977A2) = s.w977a2;
		memcpy((void *)0x1CA8A10, s.gte, sizeof(s.gte));
		memcpy((void *)s.fa, s.scratch, sizeof(s.scratch));
		MEM<uint32_t>(0x1D999C4) = s.fa;
	}

	bool held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (g_cam_tick != g_real_tick || g_ported_tick != g_real_tick) return false;
		uint32_t blk = MEM<uint32_t>(G(G_15339D0));
		if (!blk) return false;
		const int16_t *eye = (const int16_t *)0xB8B7F0, *at = (const int16_t *)0xB8B7F8;
		int16_t e0[3] = { eye[0], eye[1], eye[2] }, a0[3] = { at[0], at[1], at[2] };
		// predict: the same stepper runs on the next tick (pure memory + GTE, restored below)
		static CamSave s;
		cam_save(s, blk);
		uint32_t tick = g_cam_tick;
		int kind = g_cam_kind;
		if (kind == 1) a_73AE10();
		else a_73B4B0();
		int16_t e1[3] = { eye[0], eye[1], eye[2] }, a1[3] = { at[0], at[1], at[2] };
		cam_restore(s, blk);
		g_cam_tick = tick;
		g_cam_kind = kind;
		// a jump larger than a normal step is a cut: hold
		for (int i = 0; i < 3; i++)
		{
			int32_t de = e1[i] - e0[i], da = a1[i] - a0[i];
			if (de > 4000 || de < -4000 || da > 4000 || da < -4000)
			{
				for (int k = 0; k < 3; k++) { world[k] = e0[k]; lookat[k] = a0[k]; }
				return true;
			}
		}
		for (int i = 0; i < 3; i++)
		{
			world[i] = (int16_t)lerp_i(e0[i], e1[i], num, den);
			lookat[i] = (int16_t)lerp_i(a0[i], a1[i], num, den);
		}
		return true;
	}

	// ------------------------------------------------------------------------------------
	// actor systems (held frames): the tasks of HeldDesc::actor_sys step an actor system (node
	// +0x34, a_7353B0 / Siren s_740470: update pass, sprite draws a_740700, model draws) on every
	// real tick of their phase 1, so the actors a real tick drew are the list as it left them.
	// Each drawn actor is redrawn from a copy half way to the next tick's (the next tick's step
	// of a_740880 run on a second copy, in list order, with the emitters' motion applied to the
	// emitters and put back): draw matrix (rotation / scale), view position(s); a morphing model's
	// vertex blocks half way to the next tick's morph (F_7418B0); flipbook frame, model, fade and
	// colours hold (the next tick's at fraction 1); an actor ending on the next tick holds (a model:
	// not drawn at fraction 1); an actor not in its motion phase holds. Actors born on the next
	// tick are not drawn. (The design of the enemy attacks' actor systems, mag030 / mag048.)
	// ------------------------------------------------------------------------------------
	static const uint32_t ACTOR_BYTES = 0x2A0, EMIT_BYTES = 0x6C;
	static const int MAX_ACTORS = 160;

	// the model records of an actor system (state +0x22C table, 16 entries: vertex count +4,
	// 8-byte vertices from +8); a kind-5 model's step (F_7418B0) rewrites their vertices
	struct ActVertexBlocks
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

	static uint32_t act_def(uint32_t act)
	{
		return U32(U32(MEM<uint32_t>(G(G_258FB78)), 0x224), S8(act, 0x1D6) * 4);
	}

	enum { AP_HOLD, AP_NEXT, AP_ENDS };
	// the next tick's state of actor `act` on the copy `nxt` (a_740880's phase-0 step: frame step,
	// end test of a_7419C0 without its follow-up spawn, motion a_740910, transform a_741120)
	static int act_predict(uint32_t act, uint32_t nxt)
	{
		memcpy((void *)nxt, (const void *)act, ACTOR_BYTES);
		const uint32_t def = act_def(act);
		if (S16(act, 0x1CC) != 0) return AP_HOLD;
		a_741050(nxt, def);
		const int32_t kind = S8(def, 0x1E);
		if (kind == 1 || kind == 2)
		{
			U16(nxt, 0x1C8) = (uint16_t)(U16(nxt, 0x1C8) - 1);
			if (S16(nxt, 0x1C8) < 0) return AP_ENDS;
		}
		if ((kind == 0 || kind == 2) && U8(nxt, 0x1D2) == 1) return AP_ENDS;
		a_740910(nxt, def);
		a_741120(nxt, def); // (a morphing model's step rewrites its vertex blocks: put back by the caller)
		return AP_NEXT;
	}

	// the drawn state of the copy `cur` (the actor as the real tick left it): its transform rebuilt
	// (a_741120, from the frame the step built it with) for the held frame's camera, from the world
	// state half way to the next tick's `nxt` (AP_NEXT): angles +0xCC/+0xCE/+0xD0 (12-bit, the short
	// way round) and world positions +0xDC (stride 0x10); a keyframed model's scale (+0xD4..+0xD8,
	// from the frame's tables) half way too. Flipbook frame, model, fade and colours stay the drawn
	// tick's. At fraction 1 the next tick's actor (same draws: same copy count, still drawn).
	static void act_mid(uint32_t cur, uint32_t nxt, uint32_t def, int pr, int num, int den)
	{
		const uint8_t mode = U8(def, 0x16);
		if (pr == AP_NEXT && num >= den && S8(nxt, 0x1D8) == S8(cur, 0x1D8) && U32(nxt, 0x170) != 0)
		{
			memcpy((void *)cur, (const void *)nxt, ACTOR_BYTES);
			U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1); // frame count of the step (a_740880)
			return;
		}
		if (pr == AP_NEXT)
		{
			for (int k = 0; k < 3; k++)
			{
				const int32_t a = S16(cur, 0xCC + 2 * k), b = S16(nxt, 0xCC + 2 * k);
				S16(cur, 0xCC + 2 * k) = (int16_t)((a + ((((b - a) + 0x800) & 0xFFF) - 0x800) * num / den) & 0xFFF);
			}
			const int32_t n = S8(cur, 0x1D8) == S8(nxt, 0x1D8) ? S8(cur, 0x1D8) : 0;
			for (int32_t i = 0; i < n && i < 16; i++)
				for (int k = 0; k < 3; k++)
					S32(cur, 0xDC + 0x10 * i + 4 * k) = lerp_i(S32(cur, 0xDC + 0x10 * i + 4 * k), S32(nxt, 0xDC + 0x10 * i + 4 * k), num, den);
		}
		// (the step builds the transform from its frame, then counts the frame)
		U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) - 1);
		a_741120(cur, def);
		U16(cur, 0x1CA) = (uint16_t)(U16(cur, 0x1CA) + 1);
		if (pr == AP_NEXT && (mode == 4 || mode == 5))
			for (int j = 0; j < 3; j++) // columns of the draw matrix: scale3DMatrix scales column j by scale j
			{
				const int32_t sa = S16(cur, 0xD4 + 2 * j), sb = S16(nxt, 0xD4 + 2 * j);
				if (sa == sb || sa == 0) continue;
				const int32_t sm = lerp_i(sa, sb, num, den);
				for (int i = 0; i < 3; i++)
					S16(cur, 0xAC + 6 * i + 2 * j) = (int16_t)(S16(cur, 0xAC + 6 * i + 2 * j) * sm / sa);
			}
	}

	static void act_model_draw(uint32_t act, uint32_t def) { a_7354A0(act, def); }

	// one actor system (node +0x34) that drew on this real tick, redrawn half way
	static void held_draw_actor_system(uint32_t sys, void (*model_draw)(uint32_t, uint32_t), int num, int den)
	{
		alignas(4) static uint8_t copies[MAX_ACTORS][ACTOR_BYTES];
		alignas(4) static uint8_t nxts[MAX_ACTORS][ACTOR_BYTES];
		static ActVertexBlocks verts;
		alignas(4) static uint8_t verts_next[sizeof(verts.bytes)];
		MEM<uint32_t>(G(G_258FB78)) = sys;
		verts.save(sys);
		// the transforms are built for the held frame's camera: the snapshot *G_257F9B0 the real
		// tick took of the camera matrix 0x1D97778, taken again (at fraction 0 it is the same; the
		// next tick's steps only use the world state of their transforms, at fraction 1 its camera)
		memcpy((void *)MEM<uint32_t>(G(G_257F9B0)), (const void *)0x1D97778, 0x20);
		// the drawn actors: sprite sequences (definition kinds 0..2, a_740700) then models (4 / 5)
		uint32_t list[MAX_ACTORS];
		int nl = 0;
		for (uint32_t act = U32(sys, 0x2C); act && nl < MAX_ACTORS; act = U32(act, 4))
		{
			if (U16(act, 8) != 1 || U32(act, 0x170) == 0) continue;
			const uint8_t kind = U8(act_def(act), 0x16);
			if (kind > 5 || kind == 3) continue;
			list[nl++] = act;
		}
		// the next tick's update pass in list order: the emitters' placement / motion (a_737ED0
		// without its emission and life count, applied to the emitters themselves and put back
		// after: their particles read them) and the drawn actors' steps (act_predict)
		static const int MAX_EMIT = 64;
		static uint8_t esave[MAX_EMIT][EMIT_BYTES];
		uint32_t eaddr[MAX_EMIT];
		int ne = 0, pi = 0;
		int pr[MAX_ACTORS];
		bool morph = false;
		for (uint32_t act = U32(sys, 0x2C); act; act = U32(act, 4))
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
					callp(F(F_7424C0), act);
				else if (mode == 4)
					callp(F(F_742610), act);
			}
			else if (pi < nl && act == list[pi])
			{
				pr[pi] = act_predict(list[pi], P(nxts[pi]));
				if (pr[pi] == AP_NEXT && U8(act_def(list[pi]), 0x16) == 5) morph = true;
				pi++;
			}
		}
		for (int e = ne - 1; e >= 0; e--)
			memcpy((void *)eaddr[e], esave[e], EMIT_BYTES);
		// the next tick's morphs (vertex blocks), then the drawn states (their transforms rewrite the
		// drawn morphs; the blocks are put back and set half way below)
		if (morph)
		{
			uint32_t used = 0;
			for (uint32_t b = 0; b < verts.n; b++)
			{
				memcpy(verts_next + used, (const void *)verts.addr[b], verts.len[b]);
				used += verts.len[b];
			}
		}
		for (int i = 0; i < nl; i++)
		{
			memcpy(copies[i], (const void *)list[i], ACTOR_BYTES);
			if (S16(list[i], 0x1CC) == 0) act_mid(P(copies[i]), P(nxts[i]), act_def(list[i]), pr[i], num, den);
		}
		verts.restore();
		// morphing models: the vertex blocks half way between the drawn ones and the next tick's
		if (morph)
		{
			uint32_t used = 0;
			for (uint32_t b = 0; b < verts.n; b++)
			{
				for (uint32_t o = 0; o < verts.len[b]; o += 2)
					S16(verts.addr[b], o) = (int16_t)lerp_i(S16(verts.addr[b], o), S16(P(verts_next) + used, o), num, den);
				used += verts.len[b];
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t c = P(copies[i]);
			if (U8(act_def(list[i]), 0x16) > 2) continue;
			const int8_t cnt = S8(c, 0x1D8);
			if (cnt == 1)
				a_7407C0(c);
			else if (cnt > 0)
			{
				uint32_t sub = c + 0x19E;
				for (int32_t k = 0; k < S8(c, 0x1D8); k++, sub += 8)
				{
					S32(c, 0xC0) = S16(sub, -2);
					S32(c, 0xC4) = S16(sub, 0);
					S32(c, 0xC8) = S16(sub, 2);
					a_7407C0(c);
				}
			}
		}
		for (int i = 0; i < nl; i++)
		{
			const uint32_t def = act_def(list[i]);
			if ((U8(def, 0x16) == 4 || U8(def, 0x16) == 5) && !(pr[i] == AP_ENDS && num >= den))
				model_draw(P(copies[i]), def);
		}
		verts.restore();
	}

	// every actor system of the module that drew on this real tick (task state 1 = the step;
	// system phase 1 after its first step, or phase 2 reached on this tick), in queue order
	static void held_draw_actor_systems(const HeldDesc &d, int num, int den)
	{
		if (!d.actor_sys) return;
		const uint32_t cur = MEM<uint32_t>(G(G_258FB78));
		// the camera roll the sprites read: the real tick's systems drew before its camera step
		// (with the roll before it), the next tick's draw with the roll it wrote; half way
		// (12-bit angle, the short way round)
		const int16_t roll = MEM<int16_t>(0x1D977A2);
		if (g_cam_tick == g_real_tick)
			MEM<int16_t>(0x1D977A2) = (int16_t)(g_roll_pre + (((roll - g_roll_pre + 0x800) & 0xFFF) - 0x800) * num / den);
		for (uint32_t node = MEM<uint32_t>(G(G_258BA90)); node; node = U32(node, 4))
		{
			const HeldActorSys *a = d.actor_sys;
			for (; a->task; a++) if (U32(node, 8) == a->task) break;
			if (!a->task || S8(node, 0x29) != 1) continue;
			const uint32_t sys = node + 0x34;
			const int16_t phase = S16(sys, 0x20);
			if (!((phase == 1 && S16(sys, 0x18) > 0) || phase == 2)) continue;
			held_draw_actor_system(sys, a->model_draw ? a->model_draw : act_model_draw, num, den);
		}
		MEM<uint32_t>(G(G_258FB78)) = cur;
		MEM<int16_t>(0x1D977A2) = roll;
	}

	// ------------------------------------------------------------------------------------
	// node tasks (held frames): tasks of HeldDesc::node_tasks update THEN draw from their node on
	// every real tick, so the node as the real tick left it is the state the tick drew. Each one
	// that ran (+0x24 > 0) is redrawn from a copy half way to the next tick's (its update run on a
	// second copy: HeldNodeTask::step): the continuous words of HeldNodeTask::lerp (position,
	// angles the short way round, scale, particle positions) half way, drawn with the tick's
	// discrete state (sequence, frame, model, flags; A) and again, privately, with the next
	// tick's (B): A's colours go half way to B's, A's vertices too when both show the same
	// texture (a sequence frame that only changes the picture's shape); a frame change of the
	// texture switches on the real frame. At fraction 1 the next tick's node is drawn.
	// ------------------------------------------------------------------------------------
	static uint8_t g_nt_priv_pk[0x20000];
	static uint32_t g_nt_priv_ot[0x4000];

	// packets a (drawn) toward packets b (the same draw from the next tick's discrete state): see above;
	// stops at the first packet whose layout differs
	static void held_lerp_packets(uint8_t *a, uint8_t *ae, const uint8_t *b, const uint8_t *be, int num, int den)
	{
		while (a + 8 <= ae && b + 8 <= be)
		{
			const uint32_t la = a[3];
			if (!la || la != b[3] || a[7] != b[7]) return;
			const uint8_t code = a[7];
			if ((code & 0xE0) == 0x20)
			{
				const bool gouraud = (code & 0x10) != 0, tex = (code & 4) != 0;
				const uint32_t nv = (code & 8) ? 4 : 3;
				uint32_t *wa = (uint32_t *)(a + 4);
				const uint32_t *wb = (const uint32_t *)(b + 4);
				bool pos = true;
				if (tex)
					for (uint32_t v = 0, i = 0; v < nv; v++)
					{
						i += (v == 0 || gouraud ? 1 : 0) + 1;
						const uint32_t mask = v < 2 ? 0xFFFFFFFFu : 0xFFFFu; // uv (+ clut / page); the high half of the last two is unused
						if ((wa[i] ^ wb[i]) & mask) pos = false;
						i++;
					}
				for (uint32_t v = 0, i = 0; v < nv; v++)
				{
					if (v == 0 || gouraud)
					{
						uint8_t *ca = (uint8_t *)&wa[i];
						const uint8_t *cb = (const uint8_t *)&wb[i];
						for (int k = 0; k < 3; k++) ca[k] = (uint8_t)lerp_i(ca[k], cb[k], num, den);
					}
					i += v == 0 || gouraud ? 1 : 0;
					if (pos)
					{
						int16_t *pa = (int16_t *)&wa[i];
						const int16_t *pb = (const int16_t *)&wb[i];
						for (int k = 0; k < 2; k++) pa[k] = (int16_t)lerp_i(pa[k], pb[k], num, den);
					}
					i += 1 + (tex ? 1 : 0);
				}
			}
			a += 4 + 4 * la;
			b += 4 + 4 * la;
		}
	}

	// the continuous words of a node half way from `cur` to `nxt` (HeldNodeTask::lerp bits)
	static void held_node_lerp(uint32_t mid, uint32_t cur, uint32_t nxt, uint32_t bits, int num, int den)
	{
		auto s16 = [&](uint32_t o) { S16(mid, o) = (int16_t)lerp_i(S16(cur, o), S16(nxt, o), num, den); };
		auto ang = [&](uint32_t o) { S16(mid, o) = (int16_t)(S16(cur, o) + ((((S16(nxt, o) - S16(cur, o)) + 0x800) & 0xFFF) - 0x800) * num / den); };
		if (bits & NT_POS) for (int k = 0; k < 3; k++) s16(0x1C + 2 * k);
		if (bits & NT_ANG) for (int k = 0; k < 3; k++) ang(0x44 + 2 * k);
		if (bits & NT_SCALE) for (int k = 0; k < 3; k++) S32(mid, 0x30 + 4 * k) = lerp_i(S32(cur, 0x30 + 4 * k), S32(nxt, 0x30 + 4 * k), num, den);
		if (bits & NT_ANG54) ang(0x54);
		if (bits & NT_PART4) for (int i = 0; i < 4; i++) for (int k = 0; k < 3; k++) s16(0x6C + 8 * i + 2 * k);
	}

	static void held_draw_node_task(const HeldNodeTask &t, uint32_t node, int num, int den)
	{
		alignas(4) static uint8_t cur[0x300], nxt[0x300], mid[0x300];
		const uint32_t n = t.size < sizeof(cur) ? t.size : sizeof(cur);
		memcpy(cur, (const void *)node, n);
		memcpy(nxt, (const void *)node, n);
		t.step(P(nxt));
		if (num >= den)
		{
			t.draw(P(nxt));
			return;
		}
		// A: the continuous words half way, the tick's discrete state
		memcpy(mid, cur, n);
		held_node_lerp(P(mid), P(cur), P(nxt), t.lerp, num, den);
		const uint32_t cur_at = t.cursor ? t.cursor : 0x1D8E054;
		const uint32_t c0 = MEM<uint32_t>(cur_at);
		t.draw(P(mid));
		const uint32_t c1 = MEM<uint32_t>(cur_at);
		if (c1 == c0 || !num) return;
		// B (private): the same continuous words, the next tick's discrete state
		alignas(4) static uint8_t midb[0x300];
		memcpy(midb, nxt, n);
		held_node_lerp(P(midb), P(cur), P(nxt), t.lerp, num, den);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C), exec = MEM<uint32_t>(0x1CA8828);
		MEM<uint32_t>(0x1D8E04C) = P(g_nt_priv_ot);
		MEM<uint32_t>(cur_at) = P(g_nt_priv_pk);
		t.draw(P(midb));
		const uint32_t e = MEM<uint32_t>(cur_at);
		MEM<uint32_t>(cur_at) = c1;
		MEM<uint32_t>(0x1D8E04C) = ot;
		MEM<uint32_t>(0x1CA8828) = exec;
		if (e > P(g_nt_priv_pk) && e - P(g_nt_priv_pk) <= sizeof(g_nt_priv_pk))
			held_lerp_packets((uint8_t *)c0, (uint8_t *)c1, g_nt_priv_pk, (const uint8_t *)e, num, den);
	}

	// every node of the module's node tasks that ran on a real tick, in queue order (a draw into a
	// module packet pool writes into a private one: the real pool's bytes past its cursor are left
	// as they were)
	static uint8_t g_nt_pool_pk[0x40000];
	static void held_draw_node_tasks(const HeldDesc &d, int num, int den)
	{
		for (const HeldNodeTask *t = d.node_tasks; t && t->task; t++)
		{
			const uint32_t pool = t->cursor ? MEM<uint32_t>(t->cursor) : 0;
			if (t->cursor) MEM<uint32_t>(t->cursor) = P(g_nt_pool_pk);
			for (uint32_t node = t->queue() ? MEM<uint32_t>(t->queue()) : 0; node; node = U32(node, 4))
				if (U32(node, 8) == t->task && U16(node, 0x24) != 0)
					held_draw_node_task(*t, node, num, den);
			if (t->cursor) MEM<uint32_t>(t->cursor) = pool;
		}
	}

	// ------------------------------------------------------------------------------------
	// screen tints (held frames): the shared engine task 0x8DDC30 in HeldDesc::tint_queue. State 1
	// (0x8DDCD0) steps the colour script (+0x30 = the colour it draws) then draws four
	// semi-transparent flat quads over the screen into the static packets 0x2795110.. (OT base
	// + 0x1C). The held frame draws them (into the held packet buffer) with the colour half way to
	// the next script step's (the step run on a copy); a tint the next tick does not draw (script end)
	// holds, and is not drawn at fraction 1. It drew on the real tick when its state 1 ran then (the
	// state noted at the previous held frame; without one: a colour index past the first entry).
	// (The design of mag008_doom_held.inc.)
	// ------------------------------------------------------------------------------------
	struct TintNote { uint32_t node, tick; int8_t state, prev_state; bool prev; };
	static TintNote g_tints[16];
	static TintNote *tint_note(uint32_t node)
	{
		TintNote *free_slot = nullptr;
		for (TintNote &w : g_tints)
		{
			if (w.node == node && w.tick + 2 >= g_real_tick) return &w;
			if (!free_slot && (w.node == 0 || w.tick + 2 < g_real_tick)) free_slot = &w;
		}
		if (free_slot) { memset(free_slot, 0, sizeof(*free_slot)); free_slot->node = node; free_slot->tick = 0xFFFFFFF0u; }
		return free_slot;
	}
	// the colour step of 0x8DDCD0 on a copy (no draw); false = the script ended (nothing drawn)
	static bool tint_step(uint32_t a)
	{
		const uint16_t idx = U16(a, 0x3A);
		const uint32_t script = U32(a, 0x34);
		const uint32_t ent = U32(script, (int32_t)(int16_t)idx * 4);
		const uint8_t op = (uint8_t)(ent >> 24), b2 = (uint8_t)(ent >> 16);
		if (op == 0xFF) return false;
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
			if (MEM<uint32_t>(0x2795914) == 0) U16(a, 0x3A) = (uint16_t)(idx + 1);
		}
		return true;
	}
	static void tint_draw(uint32_t colour)
	{
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x1C;
		static const uint32_t xy[4][4] = {
			{ 0, 0xA0, 0x780000, 0x7800A0 }, { 0xA0, 0x140, 0x7800A0, 0x780140 },
			{ 0x780000, 0x7800A0, 0xF00000, 0xF000A0 }, { 0x7800A0, 0x780140, 0xF000A0, 0xF00140 } };
		uint32_t pkt = MEM<uint32_t>(0x1D8E054);
		for (int q = 0; q < 4; q++)
		{
			U32(pkt, 4) = colour;
			U32(pkt, 0) = 0x5000000;
			U8(pkt, 7) = 0x2A;
			for (int v = 0; v < 4; v++) U32(pkt, 8 + v * 4) = xy[q][v];
			x::SSIGPU_InsertPrimAltViewport(ot, pkt);
			pkt += 0x18;
		}
		const uint32_t tpage = x::sub_45C690(0, 1, 0x280, 0) & 0xFFFF;
		x::sub_45BFC0(pkt, 0, 0, tpage, 0);
		x::SSIGPU_InsertPrimAltViewport(ot, pkt);
		MEM<uint32_t>(0x1D8E054) = pkt + 0xC;
	}
	static void held_draw_tints(uint32_t queue, int num, int den)
	{
		for (uint32_t node = queue ? MEM<uint32_t>(queue) : 0; node; node = U32(node, 4))
		{
			if (U32(node, 8) != 0x8DDC30 || U16(node, 0x24) == 0) continue;
			TintNote *w = tint_note(node);
			if (!w) continue;
			if (w->tick != g_real_tick)
			{
				w->prev = w->tick + 1 == g_real_tick;
				w->prev_state = w->state;
				w->tick = g_real_tick;
				w->state = S8(node, 0x29);
			}
			const bool finished = (U8(node, 0x26) & 1) != 0;
			const bool drew = !finished && S8(node, 0x29) == 1 && (w->prev ? w->prev_state == 1 : U16(node, 0x3A) > 0);
			alignas(4) static uint8_t b[0x40];
			memcpy(b, (const void *)node, sizeof(b));
			const bool drew1 = !finished && S8(node, 0x29) == 1 && tint_step(P(b));
			if (num >= den)
			{
				if (drew1) tint_draw(U32(P(b), 0x30));
				continue;
			}
			if (!drew) continue;
			uint32_t col = U32(node, 0x30);
			if (drew1)
			{
				const uint32_t next = U32(P(b), 0x30);
				uint32_t c = col & 0xFF000000;
				for (int k = 0; k < 3; k++)
					c |= (uint32_t)(lerp_i((int32_t)((col >> (8 * k)) & 0xFF), (int32_t)((next >> (8 * k)) & 0xFF), num, den) & 0xFF) << (8 * k);
				col = c;
			}
			tint_draw(col);
		}
	}

	// held frame of a module: in-between redraw of its prim-model plays (prim::play_held) and of
	// its creature actor(s) (pose_midpoint + DrawModel a_746C10), into a private packet buffer.
	// Everything these draws write besides packets is saved and put back: the module's globals
	// (the prim callbacks refresh module tables, e.g. Siren's orbit key tables), the module scratch
	// stack, the Field_Alloc scratch, the shadow temporaries of 0x5088A0, the packet cursor, the
	// creature's model matrix / bbox / bone matrices (rebuilt from the pose by DrawModel itself)
	// the CRT rand seed (per-thread data of _getptd 0x560578, +0x14)
	static uint32_t *act_crt_seed() { return (uint32_t *)(fn<uint8_t *(__cdecl *)()>(0x560578)() + 0x14); }
	static uint8_t g_held_packets[0x100000];
	static uint8_t g_held_bss[0x20000];
	static uint32_t g_held_bss_lo = 0, g_held_bss_n = 0;
	const void *held_real_state(uint32_t addr)
	{
		return addr >= g_held_bss_lo && addr - g_held_bss_lo < g_held_bss_n ? (const void *)(g_held_bss + (addr - g_held_bss_lo)) : (const void *)addr;
	}
	void held_frame(const HeldDesc &d, int num, int den)
	{
		if (g_ported_tick != g_real_tick) return;
		const Mod *saved_mod = g_mod;
		g_mod = d.mod;
		uint32_t bss_n = d.bss_hi - d.bss_lo;
		if (bss_n > sizeof(g_held_bss)) bss_n = sizeof(g_held_bss);
		memcpy(g_held_bss, (const void *)d.bss_lo, bss_n);
		g_held_bss_lo = d.bss_lo;
		g_held_bss_n = bss_n;
		static uint8_t camcopy[0x1020], scratch[0x8000], shadow[0x80], gte[0x1CA9300 - 0x1CA8A10];
		memcpy(gte, (const void *)0x1CA8A10, sizeof(gte)); // GTE register files (the gate saves them too)
		uint32_t parsepoly[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) }; // RenderGeometry (ParsePolygons) temporaries
		memcpy(camcopy, (const void *)0x2792E58, sizeof(camcopy)); // module scratch stack (grows down from 0x2793E58: draw headers the next tick reads as residue) + camera snapshot
		uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(scratch, (const void *)fa, sizeof(scratch));
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		uint8_t sprite_cam[12]; // written by the sprite draws (0x571BC0)
		memcpy(sprite_cam, (const void *)0x21DFEE4, sizeof(sprite_cam));
		const uint32_t seed = *act_crt_seed(); // the actor emitters' placement may draw random numbers
		MEM<uint32_t>(0x1D8E054) = P(g_held_packets);

		held_draw_prim_plays(num, den);

		held_draw_actor_systems(d, num, den);
		held_draw_node_tasks(d, num, den);
		if (d.more_draws) d.more_draws(num, den);
		held_draw_tints(d.tint_queue, num, den);

		auto draw_creatures = [&](uint32_t queue, uint32_t task, uint32_t draw_arg)
		{
			for (uint32_t node = MEM<uint32_t>(queue); node; node = U32(node, 4))
			{
				if (U32(node, 8) != task) continue;
				if (U8(node, 0x26) & 4) continue; // hidden: the real tick did not draw it either
				static uint8_t mdl[0x40], place[0x30];
				memcpy(mdl, (const void *)(node + 0x60), sizeof(mdl)); // bbox +0x64..0x6E, matrix +0x70..0x8F
				memcpy(place, (const void *)(node + 0x30), sizeof(place)); // placement (position +0x4C..0x50)
				pose_midpoint((void *)(node + 0x90), (void *)(node + 0x9C), num, den);
				if (d.adjust) d.adjust(node, num, den);
				MEM<uint32_t>(0x1D8E054) = a_746C10(node, draw_arg, MEM<uint32_t>(0x1D8E054));
				memcpy((void *)(node + 0x30), place, sizeof(place));
				memcpy((void *)(node + 0x60), mdl, sizeof(mdl));
			}
		};
		draw_creatures(d.creature_queue, d.creature_task, d.draw_arg);
		for (const HeldCreature *c = d.more; c && c->queue; c++)
			draw_creatures(c->queue, c->task, c->draw_arg);
		MEM<uint32_t>(0x1D8E054) = cursor;
		*act_crt_seed() = seed;
		memcpy((void *)0x21DFEE4, sprite_cam, sizeof(sprite_cam));
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		memcpy((void *)fa, scratch, sizeof(scratch));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)0x2792E58, camcopy, sizeof(camcopy));
		memcpy((void *)d.bss_lo, g_held_bss, bss_n);
		g_held_bss_n = 0;
		memcpy((void *)0x1CA8A10, gte, sizeof(gte));
		MEM<uint32_t>(0x1D99C18) = parsepoly[0];
		MEM<uint32_t>(0x1D99C1C) = parsepoly[1];
		g_mod = saved_mod;
	}

	bool held_ready() { return g_ported_tick == g_real_tick; }

	// ------------------------------------------------------------------------------------
	// prim-model player (prim_play): every play of the real tick is remembered (layout, ported
	// callback, the 0x5C-byte parameter block) so the held frame can redraw it in between
	// (prim::play_held)
	// ------------------------------------------------------------------------------------
	struct PrimPlay { uint32_t layout, cb; const Mod *mod; uint8_t arg[0x5C]; };
	static PrimPlay g_prim_plays[256];
	static int g_nprim_plays = 0;
	static uint32_t g_prim_tick = 0xFFFFFFFF;

	// prim_play, after the play: f = the callback it ran (port or original)
	void held_note_prim_play(uint32_t layout, uint32_t f, uint32_t arg)
	{
		if (g_prim_tick != g_real_tick) { g_prim_tick = g_real_tick; g_nprim_plays = 0; }
		if (g_nprim_plays < 256)
		{
			PrimPlay &m = g_prim_plays[g_nprim_plays++];
			m.layout = layout;
			m.cb = f;
			m.mod = g_mod;
			memcpy(m.arg, (const void *)arg, sizeof(m.arg));
		}
	}

	void held_draw_prim_plays(int num, int den)
	{
		if (g_prim_tick != g_real_tick) return;
		for (int i = 0; i < g_nprim_plays; i++)
		{
			const PrimPlay &m = g_prim_plays[i];
			if (m.mod != g_mod) continue;
			alignas(4) uint8_t arg[0x5C];
			memcpy(arg, m.arg, sizeof(arg));
			ff8fx::prim::play_held((ff8fx::prim::Layout *)m.layout, (ff8fx::prim::Callback)m.cb, (int)P(arg), num, den);
		}
	}

	// ------------------------------------------------------------------------------------
	// registration
	// ------------------------------------------------------------------------------------
	// held tasks of a module among its engine ports (register_module registers them): marked held
	// = the module's held frame redraws their drawing (the generic packet replay skips them)
	void register_module_held_tasks(int effect_id, const uint32_t *held_tasks)
	{
		for (const AddrPort *p = engine_ports(effect_id); p && p->addr; p++)
		{
			bool held = false;
			for (const uint32_t *h = held_tasks; h && *h; h++) held |= *h == p->addr;
			if (held) mark_held(p->addr);
		}
	}
}
}
