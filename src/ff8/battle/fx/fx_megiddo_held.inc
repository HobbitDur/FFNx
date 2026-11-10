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

// 30 fps layer of fx_megiddo.h (Megiddo Flame, effects 166 / 312), included by the modules' held parts.
//
// Every drawing task draws THEN updates. A real tick records its draws in order (the root's seam starts the log): the
// node as drawn, the GTE state the draw started with and the Field_Alloc scratch under its draw header; the pools as
// drawn (sparks after the tick's spawns, debris before its update); the sheet's gathered history. A held frame replays
// the log into a private packet buffer (module cursor and frame arena), each draw at its exact in-between state (logic
// stays 15 Hz), with the LIVE camera:
//   Effect point / frame - the director's point at the caster's bone with the caster at its in-between pose
//     (pose_midpoint at num / den through the caster's live matrix: fraction 1 = the next tick's pose), the frame
//     rebuilt from it (private matrix); the real pose is rebuilt afterwards. On a draw-only tick: the tick's point.
//   Glow / ring / ring 2 / column / column flash / pillar - scales half way to the values the update left; spins' angle
//     half way; fades half way (a side drawn without the fade mode counts as fade 0: a fade-in ending / a fade-out
//     starting on the next tick goes half way too). A part whose task ends on the next tick is not drawn at fraction 1.
//   Arc - its target half way between spline points +0x18 of the two ticks (the arc's path), from the held point.
//   Sheet - each history age moved half way to the next tick's entry of that age (the newest toward the next tick's
//     (point, spline point), the older ones toward the entry one age younger), its colour half way to the next entry of
//     the 12-colour cycle (channel by channel), splines and quads rebuilt on a private copy of the record; a tick where
//     the number of entries changes holds its picture below 1 (at 1: the next tick's entries).
//   Ribbon - the points drawn grow half way (count + (next count - count) * num / den: exact spline samples), its fade
//     level half way while both ticks fade; the ribbon table is put back.
//   Flames - v scroll and fade half way. Sparks - static flipbooks (the pool as drawn; frames switch on the real
//     tick; at fraction 1 the pool the update left). Debris - each entry that lives on: y, scale and angles half way;
//     one that dies on the tick holds below 1 (not drawn at 1).
// The director's screen flashes and fades are engine state (the gate), the stage capture is VRAM kept from the real
// frame; the bolt directors, knock-downs and throws draw nothing (the entities they move are drawn by the engine).
// Nothing here writes game state: the scope puts back the cursors, the GTE registers and the Field_Alloc scratch, the
// ribbon table and the caster's bone matrices.

#pragma once

namespace ff8fx
{
namespace megiddo
{
	template<class D>
	struct MegiddoHeld
	{
		using M = Megiddo<D>;
		static constexpr uint32_t G(uint32_t a) { return M::G(a); }

		enum RecKind { REC_NODE, REC_SHEET, REC_POOL };
		static const uint32_t GTE_BYTES = magc::GTE_DATA_SIZE + magc::GTE_CTRL_SIZE;
		struct HRec
		{
			int kind;
			uint32_t fn;
			const Node24 *node;
			Node24 copy;               // the node as drawn
			uint8_t gte[GTE_BYTES];    // GTE state the draw started with
			uint8_t hdr[0xB4];         // scratch under its draw header when it was allocated
			int32_t k;                 // sheet: entries gathered
			uint8_t hist[0x40];        // sheet: the gathered entries (+0x1F4: points, +0x214: spline points)
			uint32_t snap;             // pools: offset of the pool as drawn in g_snap
		};
		static const int REC_MAX = 160;
		static inline uint32_t g_ported_tick = 0xFFFFFFFFu;
		static inline bool g_tick_paused = false;
		static inline uint32_t g_pkt_start = 0, g_fa_start = 0;
		static inline uint8_t g_held_packets[0x30000], g_held_fa[0x8000];
		static inline HRec g_rec[REC_MAX];
		static inline int g_nrec = 0;
		static inline uint8_t g_snap[0x3000];
		static inline uint32_t g_nsnap = 0;
		static inline bool g_dir_noted = false;
		static inline int16_t g_point[4];       // the effect point the tick drew with

		static bool HeldReady() { return g_ported_tick == g_real_tick; }

		static void note_root()
		{
			g_ported_tick = g_real_tick;
			g_tick_paused = DrawOnly();
			g_pkt_start = M::PacketCursor();
			g_fa_start = var<uint32_t>(0x1D8E054);
			g_nrec = 0;
			g_nsnap = 0;
			g_dir_noted = false;
		}

		static void note_director(const Node24 *)
		{
			if (g_ported_tick != g_real_tick) return;
			memcpy(g_point, M::Point(), sizeof(g_point));
			g_dir_noted = true;
		}

		static HRec *NoteRec(int kind, uint32_t fn, const Node24 *n, bool header)
		{
			magc::g_hdr_slot = nullptr;
			if (g_ported_tick != g_real_tick || g_nrec >= REC_MAX) return nullptr;
			HRec &r = g_rec[g_nrec++];
			r.kind = kind;
			r.fn = fn;
			r.node = n;
			memcpy(&r.copy, n, sizeof(Node24));
			memcpy(r.gte, (const void *)magc::GTE_DATA, magc::GTE_DATA_SIZE);
			memcpy(r.gte + magc::GTE_DATA_SIZE, (const void *)magc::GTE_CTRL, magc::GTE_CTRL_SIZE);
			r.k = 0;
			if (header) magc::g_hdr_slot = r.hdr;
			return &r;
		}

		static void note_node(uint32_t fn, const Node24 *n) { NoteRec(REC_NODE, fn, n, fn != M::ORIG_Ribbon); }

		static void note_sheet(const Node24 *n, uint32_t rec, int32_t k)
		{
			HRec *r = NoteRec(REC_SHEET, M::ORIG_Sheet, n, false);
			if (!r) return;
			r->k = k;
			memcpy(r->hist, (const void *)(rec + 0x1F4), 0x40);
		}

		static void note_pool(uint32_t fn, const Node24 *n)
		{
			const uint32_t pool = fn == M::ORIG_Sparks ? G(M::POOL_A) : G(M::POOL_B);
			const uint32_t end = fn == M::ORIG_Sparks ? G(M::POOL_A_END) : G(M::POOL_B_END);
			if (g_nsnap + (end - pool) > sizeof(g_snap)) { magc::g_hdr_slot = nullptr; return; }
			HRec *r = NoteRec(REC_POOL, fn, n, true);
			if (!r) return;
			r->snap = g_nsnap;
			memcpy(g_snap + g_nsnap, (const void *)pool, end - pool);
			g_nsnap += end - pool;
		}

		// a held draw starts like the real one: its GTE state and header scratch
		static void Replay(const HRec &r, bool header)
		{
			memcpy((void *)magc::GTE_DATA, r.gte, magc::GTE_DATA_SIZE);
			memcpy((void *)magc::GTE_CTRL, r.gte + magc::GTE_DATA_SIZE, magc::GTE_CTRL_SIZE);
			magc::g_hdr_slot = header ? (uint8_t *)r.hdr : nullptr;
		}

		// the node still runs the same task on the next tick and advanced
		static bool Alive(const HRec &r)
		{
			return (r.node->hdr.flags & 1) != 0 && (uint32_t)r.node->hdr.func == M::C(r.fn) && r.node->c == (int16_t)(r.copy.c + 1);
		}

		static inline int16_t L16(int16_t a, int16_t b, int num, int den) { return magc::lerp16(a, b, num, den); }

		// the fade half way to the next tick's: a side drawn without the fade mode (flags without 0xC0) is fade 0 (the
		// renderer's colours unchanged), so a fade-in ending / a fade-out starting on the next tick goes half way too;
		// fraction 1 = the next tick's exactly; no fading side (or another mode change): tick N's
		static typename M::PF PFLerp(const typename M::PF &a, const typename M::PF &b, bool next, int num, int den)
		{
			if (!next) return a;
			if (num >= den) return b;
			const bool ca = (a.flags & 0xC0) != 0, cb = (b.flags & 0xC0) != 0;
			if (!ca && !cb) return a;
			if (ca && cb && a.flags != b.flags) return a;
			typename M::PF r = ca ? a : b;
			r.wf = true;
			r.fade = lerp_i(ca ? a.fade : 0, cb ? b.fade : 0, num, den);
			return r;
		}

		// the node's task ends on the next tick (not paused): a held frame at 1 draws nothing for it, as the next tick
		static bool EndsAtOne(const HRec &r, int num, int den) { return num >= den && !g_tick_paused && !Alive(r); }

		// ---- held surroundings: module cursor (magc::HeldScope: + scratch, shadow scratch, GTE) and frame arena
		struct Scope
		{
			magc::HeldScope base;
			uint32_t fa_saved;
			Scope() : base(M::PacketCursor(), g_held_packets, sizeof(g_held_packets), g_pkt_start)
			{
				fa_saved = var<uint32_t>(0x1D8E054);
				if (g_fa_start && fa_saved > g_fa_start)
				{
					const uint32_t used = fa_saved - g_fa_start;
					memcpy(g_held_fa, (const void *)g_fa_start, used < sizeof(g_held_fa) ? used : sizeof(g_held_fa));
				}
				var<uint32_t>(0x1D8E054) = (uint32_t)g_held_fa;
			}
			~Scope() { var<uint32_t>(0x1D8E054) = fa_saved; }
		};

		// ---- the held effect point and frame
		static inline int16_t g_hpoint[4];
		static inline Mat4x3 g_hframe;
		static void HeldPoint(int num, int den)
		{
			memcpy(g_hpoint, g_dir_noted ? g_point : M::Point(), sizeof(g_hpoint));
			if (g_dir_noted && !g_tick_paused && num != 0)
			{
				uint8_t *ent = Entity(M::CasterSlot());
				if (*(const uint32_t *)(ent + 0x64) != 0)
				{
					pose_midpoint((void *)(ent + 0x60), (void *)(ent + 0x6C), num, den);
					M::EffectPoint(g_hpoint);
					BuildBoneMatricesFromPose((void *)(ent + 0x60));
				}
			}
			M::FrameAt(g_hpoint, &g_hframe);
		}

		// ---- single models
		static void NodeHeld(const HRec &r, int num, int den)
		{
			const Node24 &d = r.copy;
			const Node24 *now = r.node;
			const bool next = num != 0 && !g_tick_paused && Alive(r);
			if (EndsAtOne(r, num, den)) return;
			Node24 h = d;
			auto L = [&](int16_t a, int16_t b) { return next ? L16(a, b, num, den) : a; };
			const int32_t c = d.c;
			Replay(r, r.fn != M::ORIG_Ribbon);
			switch (r.fn)
			{
			case M::ORIG_Glow:
				h.w1C = L(d.w1C, now->w1C);
				M::GlowDraw(&h, &g_hframe, PFLerp(M::GlowPF(c), M::GlowPF(c + 1), next, num, den));
				break;
			case M::ORIG_Spin:
				h.w18 = L(d.w18, now->w18);
				M::SpinDraw(&h, &g_hframe, PFLerp(M::SpinPF(c), M::SpinPF(c + 1), next, num, den));
				break;
			case M::ORIG_Ring:
				h.w1C = L(d.w1C, now->w1C);
				M::RingDraw(&h, &g_hframe, PFLerp(M::RingPF(c), M::RingPF(c + 1), next, num, den));
				break;
			case M::ORIG_Arc:
			{
				const uint32_t rec = M::Bolt(d.e);
				const int16_t *a = (const int16_t *)(rec + 0x38 + 8 * d.w18);
				int16_t t[4] = { a[0], a[1], a[2], a[3] };
				if (next)
				{
					const int16_t *b = (const int16_t *)(rec + 0x38 + 8 * now->w18);
					for (int q = 0; q < 3; q++) t[q] = L16(a[q], b[q], num, den);
				}
				M::ArcDraw(&h, g_hpoint, t, PFLerp(M::ArcPF(c), M::ArcPF(c + 1), next, num, den));
				break;
			}
			case M::ORIG_Ribbon:
			{
				const uint32_t rec = M::Bolt(d.e);
				int32_t cnt = d.w18 + 1;
				if (next) cnt += (now->w18 - d.w18) * num / den;
				const bool fade = c >= 0x18;
				int32_t ir0 = M::RibbonFade(c);
				if (next && fade) ir0 = lerp_i(ir0, M::RibbonFade(c + 1), num, den);
				uint8_t table[0x30 * 0x18];
				memcpy(table, (const void *)G(M::RIBBON_TABLE), sizeof(table));
				uint8_t *blk = (uint8_t *)FieldAlloc(0x90);
				M::RibbonTable(rec, blk, cnt);
				M::RibbonDraw(fade, ir0, blk, cnt);
				FieldFree(0x90);
				memcpy((void *)G(M::RIBBON_TABLE), table, sizeof(table));
				break;
			}
			case M::ORIG_Column:
				M::ColumnDraw(&h, L(d.w1C, now->w1C));
				break;
			case M::ORIG_ColumnFlash:
				M::ColumnFlashDraw(&h, L(d.w1C, now->w1C), PFLerp(M::ColumnFlashPF(c), M::ColumnFlashPF(c + 1), next, num, den));
				break;
			case M::ORIG_Pillar:
				M::PillarDraw(&h, d.w1C, L(d.w20, now->w20), PFLerp(M::PillarPF(c), M::PillarPF(c + 1), next, num, den));
				break;
			case M::ORIG_Flame:
			{
				int32_t scroll = mul32(d.w22, c);
				if (next) scroll = lerp_i(scroll, mul32(now->w22, c + 1), num, den);
				M::FlameDraw(&h, scroll, PFLerp(M::FlamePF(c), M::FlamePF(c + 1), next, num, den));
				break;
			}
			case M::ORIG_Ring2:
				M::Ring2Draw(&h, L(d.w1C, now->w1C), PFLerp(M::Ring2PF(c), M::Ring2PF(c + 1), next, num, den));
				break;
			}
		}

		// ---- the sheet: history ages half way to the next tick's
		static inline uint8_t g_rec_copy[0x334];
		static void SheetHeld(const HRec &r, int num, int den)
		{
			const Node24 &d = r.copy;
			const Node24 *now = r.node;
			const uint32_t rec = M::Bolt(d.e);
			memcpy(g_rec_copy, (const void *)rec, sizeof(g_rec_copy));
			const uint32_t pc = (uint32_t)g_rec_copy;
			memcpy(g_rec_copy + 0x1F4, r.hist, 0x40);
			const int32_t k = r.k;
			if (EndsAtOne(r, num, den)) return;
			bool next = num != 0 && !g_tick_paused && Alive(r);
			int32_t kk = k; // entries drawn
			if (next)
			{
				// the next tick's entry count: its limit, the valid entries (one more, at most 3)
				const int32_t c1 = d.c + 1;
				const int32_t lim = c1 >= 0x15 ? 0x18 - c1 : 3;
				int32_t valid = 1;
				for (int q = 0; q < 3; q++)
					if (q != c1 % 3 && At<uint32_t>(rec, 0x1B8 + 0x14 * q) != 0) valid++;
				const int32_t k1 = valid < lim ? valid : lim;
				// the count changes: held below 1; at 1 the next tick's entries (none drawn below 2)
				if (k1 != k)
				{
					if (num < den) next = false;
					else if (k1 <= 1) return;
					else kk = k1;
				}
			}
			if (next)
			{
				int16_t newest[8];
				memcpy(newest, g_hpoint, 8);
				const int16_t *sa = (const int16_t *)(rec + 0x38 + 8 * d.w18), *sb = (const int16_t *)(rec + 0x38 + 8 * now->w18);
				for (int q = 0; q < 4; q++) newest[4 + q] = sa[q];
				for (int q = 0; q < 3; q++) newest[4 + q] = L16(sa[q], sb[q], num, den);
				// ages from the oldest: each toward the entry one age younger (N's), the newest toward the next tick's
				for (int32_t a = kk - 1; a >= 0; a--)
				{
					int16_t *pt = (int16_t *)(pc + 0x1F4 + 8 * a), *sp = (int16_t *)(pc + 0x214 + 8 * a);
					if (a == 0)
					{
						for (int q = 0; q < 3; q++) { pt[q] = newest[q]; sp[q] = newest[4 + q]; }
						continue;
					}
					const int16_t *pt1 = (const int16_t *)(r.hist + 8 * (a - 1)), *sp1 = (const int16_t *)(r.hist + 0x20 + 8 * (a - 1));
					for (int q = 0; q < 3; q++)
					{
						pt[q] = L16(pt[q], pt1[q], num, den);
						sp[q] = L16(sp[q], sp1[q], num, den);
					}
				}
			}
			Replay(r, false);
			M::SheetCurves(pc, kk);
			uint8_t *blk = (uint8_t *)FieldAlloc(0x40);
			// the colour cycle (12 colours, one step a tick) half way to the next tick's, channel by channel
			uint32_t col = M::SheetColour(d.c);
			if (next)
			{
				const uint32_t c1 = M::SheetColour(d.c + 1);
				uint32_t m = 0;
				for (int sh = 0; sh < 24; sh += 8) m |= (uint32_t)lerp_i((int32_t)(col >> sh & 0xFF), (int32_t)(c1 >> sh & 0xFF), num, den) << sh;
				col = m;
			}
			M::SheetDraw(col, pc, blk);
			FieldFree(0x40);
		}

		// ---- pools as drawn
		static void PoolHeld(const HRec &r, int num, int den)
		{
			const bool next = num != 0 && !g_tick_paused && (r.node->hdr.flags & 1) && (uint32_t)r.node->hdr.func == M::C(r.fn);
			if (num >= den && !g_tick_paused && !next) return; // the task ended on the tick (its pool empty): the next tick draws none
			Replay(r, true);
			if (r.fn == M::ORIG_Sparks)
			{
				// below 1 the pool as drawn (flipbook frames hold); at 1 the pool the update left (frames stepped, ended
				// sparks freed), as the next tick draws it before its spawns
				const bool one = next && num >= den;
				const uint32_t pool = one ? G(M::POOL_A) : (uint32_t)(g_snap + r.snap);
				uint8_t *blk = (uint8_t *)FieldAlloc(0x48);
				M::SparksDraw(blk, pool, pool + (G(M::POOL_A_END) - G(M::POOL_A)), false);
				FieldFree(0x48);
				return;
			}
			uint8_t *hdr = AllocHeader(0x58);
			uint8_t *blk = (uint8_t *)FieldAlloc(0x88);
			memcpy(blk + 0x28, &Camera(), 0x20);
			*(uint32_t *)(hdr + 8) = 0;
			*(uint32_t *)(hdr + 0x1C) = 0;
			*(int16_t *)(blk + 2) = 0;
			const uint32_t snap = (uint32_t)(g_snap + r.snap), live = G(M::POOL_B);
			for (uint32_t o = 0; o < G(M::POOL_B_END) - G(M::POOL_B); o += 0x1C)
			{
				const uint32_t e = snap + o, e1 = live + o;
				if (!(*(const uint8_t *)e & 2)) continue;
				int16_t ax = At<int16_t>(e, 0x16), az = At<int16_t>(e, 0x1A), s = At<int16_t>(e, 6), y = At<int16_t>(e, 0xE);
				const bool lives = At<uint32_t>(e1, 0) == At<uint32_t>(e, 0) && At<int16_t>(e1, 4) == (int16_t)(At<int16_t>(e, 4) + 1);
				if (next && num >= den && !lives) continue; // ends on the tick: the next tick draws none
				if (next && lives)
				{
					ax = L16(ax, At<int16_t>(e1, 0x16), num, den);
					az = L16(az, At<int16_t>(e1, 0x1A), num, den);
					s = L16(s, At<int16_t>(e1, 6), num, den);
					y = L16(y, At<int16_t>(e1, 0xE), num, den);
				}
				M::DebrisEntry(hdr, blk, e, ax, az, s, At<int16_t>(e, 0xC), y, At<int16_t>(e, 0x10));
			}
			FieldFree(0x88);
			FieldFree(0x58);
		}

		static void HeldFrame(int num, int den)
		{
			if (!HeldReady()) return;
			Scope scope;
			HeldPoint(num, den);
			for (int i = 0; i < g_nrec; i++)
			{
				const HRec &r = g_rec[i];
				switch (r.kind)
				{
				case REC_NODE: NodeHeld(r, num, den); break;
				case REC_SHEET: SheetHeld(r, num, den); break;
				case REC_POOL: PoolHeld(r, num, den); break;
				}
			}
		}

		static void Register()
		{
			static const uint32_t drawn[] = { M::ORIG_Root, M::ORIG_Director, M::ORIG_Glow, M::ORIG_Spin, M::ORIG_Ring, M::ORIG_Arc,
				M::ORIG_Sheet, M::ORIG_Ribbon, M::ORIG_Column, M::ORIG_ColumnFlash, M::ORIG_Pillar, M::ORIG_Flame, M::ORIG_Sparks,
				M::ORIG_Debris, M::ORIG_Ring2 };
			for (uint32_t f : drawn) mark_held(M::C(f));
			register_module_held(D::EFFECT, HeldReady, HeldFrame);
		}
	};
}
}
