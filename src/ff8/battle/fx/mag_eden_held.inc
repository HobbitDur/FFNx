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

// 30 fps part of mag_eden.cpp: held frames of the clone and its held-frame notes (included at the end
// of mag_eden.cpp)
//
// Held-frame behaviour of the Eden handlers (the engine draws the held frames), per handler
// below; the others:
//   Draw 57 MeshRotScaledOrder3, Draw 61 MeshRadialLightWave, Draw 62 MeshRadialDarken, Draw 63
//     MorphWireframeLines: idempotent (no boneSkipFlag gate in vanilla, nothing advanced, no
//     external side effect; they read the in-between pose and write only ws/GTE/arena scratch
//     and packets at the held cursor) -> run unchanged on held frames.
//   Draw 58 RequestScreenFeedback, Draw 64 PlaceModelLerpBulge, Draw 65
//     BattleModelSyncTransform: nothing on held frames (external side effect / E is a battle
//     entity outside the saved set; it keeps the real tick's transform).
//   Prim 6: pure draw code.
// Prediction: VM 0x0B6 / 0x0B8 blit queue (xl_QueueBlitCommand) and VM 0x0B7
// (xl_SdStreamingVolumeTranslation) are skipped; the blit ring 0x27978E8 / index 0x2798BF4 are in
// the engine block (restored after a prediction).

#include <vector>

namespace ff8fx
{
namespace gfc
{
namespace part_eden_a
{
	// ------------------------------------------------------------------------------------
	// 0xAE5AD0 Draw 59 flat grid, held frame: steps 1 and 2 are skipped (the counters are the
	// ones the real tick drew; the arena block is not modified), active counters below 16 are
	// drawn at F + num/den. Draw 59 rolled mode is a pure function of the in-between pose.
	// ------------------------------------------------------------------------------------
	// colour of a fading line at the exact in-between of the real tick's counter F and the next
	// tick's min(F + 1, 16): the 0xB66A20 computation with IR0 = 16 * t * (F + num/den). Only
	// used on held frames (never on a real tick).
	static uint32_t HeldColourScale(uint32_t col, int32_t ir0)
	{
		x::GteWriteData(ir0, 8);
		x::GteWriteData((int32_t)(col & 0xFF), 9);
		x::GteWriteData((int32_t)((col >> 8) & 0xFF), 10);
		x::GteWriteData((int32_t)((col >> 16) & 0xFF), 11);
		eng::GteGPF();
		return (MEM<uint32_t>(0x1CA8A34) & 0xFF) | ((uint32_t)MEM<uint8_t>(0x1CA8A38) << 8)
			| ((uint32_t)MEM<uint8_t>(0x1CA8A3C) << 16) | (col & 0xFF000000u);
	}

	// FadeColour: frac (the factor really is t * F of a fade counter): on a held frame an active
	// counter below 16 is drawn at F + num/den (returns true, colour in *out)
	static bool held_fade_colour(uint32_t col, int32_t t, int32_t fv, bool frac, uint32_t *out)
	{
		if (!(frac && g_held.active && g_held.den > 0 && fv >= 0 && fv < 0x10)) return false;
		*out = HeldColourScale(col, (int32_t)((int64_t)16 * t * ((int64_t)fv * g_held.den + g_held.num) / g_held.den));
		return true;
	}

	// ------------------------------------------------------------------------------------
	// 0xAE6CA0 (Draw 60 PlaceModelOnCurlPath), held: runs normally (the bone outPos/accumPos it
	// writes live in the bone array, saved by HeldFrame), but the battle-model instance E
	// (outside the snapshot) is put back to its real-tick values before returning.
	// ------------------------------------------------------------------------------------
	static uint8_t *g_e60 = nullptr;
	static uint8_t g_sv1 = 0, g_sv94[6] = {}, g_svC[2] = {}, g_sv1C[6] = {};

	static void held_entity60_save(uint8_t *E)
	{
		g_e60 = nullptr;
		if (!g_held.active) return;
		g_e60 = E;
		g_sv1 = U8(E, 1);
		memcpy(g_sv94, E + 0x94, 6);
		memcpy(g_svC, E + 0xC, 2);
		memcpy(g_sv1C, E + 0x1C, 6);
	}

	static void held_entity60_restore()
	{
		uint8_t *E = g_e60;
		if (!E) return;
		U8(E, 1) = g_sv1;
		memcpy(E + 0x94, g_sv94, 6);
		memcpy(E + 0xC, g_svC, 2);
		memcpy(E + 0x1C, g_sv1C, 6);
		g_e60 = nullptr;
	}
}

namespace part_eden_c
{
	// held frames: a range of the unallocated scratch above the arena top, saved and restored
	struct ScratchSave
	{
		uint8_t *p = nullptr;
		std::vector<uint8_t> b;
		void save(uint8_t *q, uint32_t n) { p = q; b.assign(q, q + n); }
		void restore() { if (p && !b.empty()) memcpy(p, b.data(), b.size()); p = nullptr; }
	};
	static ScratchSave g_ss;

	static void held_scratch_restore() { g_ss.restore(); }

	// ------------------------------------------------------------------------------------
	// 0xAED040 (Draw 53 PixelParticleEmitter), held frames: no allocation, no spawn (no rand),
	// the pool is not written. Every particle the real tick drew is drawn, in the same (slot)
	// order:
	//  * alive for the next tick (life bit7 clear): at pos + ((s16)(vel + acc) << 4) * num/den
	//    (the exact in-between of the deterministic integration), colour = the in-between of
	//    the real tick's table entry (life + 1) and the next tick's (life);
	//  * died on the real tick (drawn with life 0, now 0xFF): its last picture (position and
	//    colour of the real tick) - it disappears on the next real tick. The pool cannot tell
	//    "died on this tick" from "died earlier / never used" (both 0xFF): a per-pool memo of
	//    the lives after the previous tick (taken by the held frames of that tick) does (life 0
	//    then = drawn with life 0 on this tick); a pool re-allocated at the same address by a
	//    new emitter (bone or write index not continuing) starts a fresh memo.
	// The next tick's spawns first appear on that tick (discrete, rand-driven).
	// ------------------------------------------------------------------------------------
	struct Alive53 { const uint8_t *pool; const uint8_t *bone; uint16_t widx; uint32_t tick; std::vector<uint8_t> prev, cur; };
	static Alive53 g_alive53[8];

	// memo of pool: prev = lives after tick g_real_tick - 1 (or empty), cur = after this tick
	static Alive53 *alive53_memo(const uint8_t *pool, uint8_t *parts, int32_t n)
	{
		Alive53 *m = nullptr;
		for (Alive53 &x : g_alive53) if (x.pool == pool) { m = &x; break; }
		if (!m)
		{
			for (Alive53 &x : g_alive53) if (!x.pool || x.tick + 2 < g_real_tick) { m = &x; break; }
			if (!m) return nullptr;
			m->pool = pool; m->tick = 0xFFFFFFFF; m->prev.clear(); m->cur.clear();
		}
		if (m->tick != g_real_tick)
		{
			uint16_t widx = U16(pool, 2);
			bool same = m->tick + 1 == g_real_tick && m->bone == CUR() && (uint16_t)(widx - m->widx) < 0x8000u && m->cur.size() == (size_t)n;
			if (same) m->prev.swap(m->cur);
			else m->prev.clear();
			m->cur.resize((size_t)n);
			for (int32_t i = 0; i < n; i++) m->cur[i] = U8(parts + 0x1C * i, 0x18);
			m->tick = g_real_tick;
			m->bone = CUR();
			m->widx = widx;
		}
		return m;
	}

	static uint32_t lerp_rgb(uint32_t c0, uint32_t c1, int num, int den)
	{
		uint32_t c = c0 & 0xFF000000u;
		for (int s = 0; s < 24; s += 8)
		{
			int32_t x0 = (c0 >> s) & 0xFF, x1 = (c1 >> s) & 0xFF;
			c |= (uint32_t)((x0 + (x1 - x0) * num / den) & 0xFF) << s;
		}
		return c;
	}

	static void dh_53_PixelParticleEmitterHeld()
	{
		if (U32(CUR(), 0xBC) == 0) return;
		uint8_t *desc = PTR(CUR(), 0xB8);                                       // edi
		uint8_t *pool = PTR(CUR(), 0xBC);                                       // [ebp-8]
		uint8_t *ctrl = blob::GetBone(S16(desc, 6));                             // ecx
		int32_t ax = S16(ctrl, 0x8C);                                            // ebx
		int32_t az = S16(ctrl, 0x90);                                            // [ebp-4]
		int32_t ay = S16(ctrl, 0x8E);                                            // esi
		U16(WS(), 0xA0) = (uint16_t)div256(mul32(S16(desc, 0x0A), ax));
		U16(WS(), 0xA2) = (uint16_t)div256(mul32(S16(desc, 0x12), ay));
		U16(WS(), 0xA4) = (uint16_t)div256(mul32(S16(desc, 0x1A), az));
		U16(WS(), 0xA8) = (uint16_t)div256(mul32(S16(desc, 0x0C), ax));
		U16(WS(), 0xAA) = (uint16_t)div256(mul32(S16(desc, 0x14), ay));
		U16(WS(), 0xAC) = (uint16_t)div256(mul32(S16(desc, 0x1C), az));
		U16(WS(), 0xB0) = (uint16_t)div256(mul32(S16(desc, 0x0E), ax));
		U16(WS(), 0xB2) = (uint16_t)div256(mul32(S16(desc, 0x16), ay));
		U16(WS(), 0xB4) = (uint16_t)div256(mul32(S16(desc, 0x1E), az));
		U16(WS(), 0xB8) = (uint16_t)div256(mul32(S16(desc, 0x10), ax));
		U16(WS(), 0xBA) = (uint16_t)div256(mul32(S16(desc, 0x18), ay));
		U16(WS(), 0xBC) = (uint16_t)div256(mul32(S16(desc, 0x20), az));
		PTR(WS(), 0xE0) = pool + 0x10;
		// OT
		int32_t bias = S16(CUR(), 0xC0);
		if (bias == 0)
			S32(WS(), 0x64) = 0;
		else
			U32(WS(), 0x64) = ((uint32_t)bias & 0x3FFC) + U32(RT(), 0x4C);
		Mat4x3 *m = blob::GetParentMatrix(U16(CUR(), 0x9C));                     // ebx
		x::SetRotMatrix(m);
		x::SetTransVector(m);
		U32(WS(), 0xE4) = U32(pool, 4);
		uint8_t *p = PTR(WS(), 0xE0);                                            // esi
		int32_t n = U16(desc, 4);                                                // [ebp-0x10]
		uint8_t *pkt = PTR(CTX(), 0x7C);                                         // ebx / [ebp-0xc]
		const Alive53 *memo = n > 0 ? alive53_memo(pool, p, n) : nullptr;
		if (n > 0)
		{
			int32_t slot = 0;
			do
			{
				const bool alive = !(U8(p, 0x18) & 0x80);
				const bool dying = !alive && U8(p, 0x18) == 0xFF && memo && (size_t)slot < memo->prev.size() && memo->prev[slot] == 0;
				if (alive || dying)
				{
					int32_t px, py, pz;       // position used for the draw
					uint32_t col;             // colour
					if (alive)
					{
						// in-between of the next tick's integration, on locals
						int32_t d[3];
						for (int k = 0; k < 3; k++)
						{
							int16_t v = (int16_t)(U16(p, 0x0C + 4 * k) + U16(p, 0x0E + 4 * k));
							d[k] = g_held.den > 0 ? (int32_t)((int64_t)shl32(v, 4) * g_held.num / g_held.den) : 0;
						}
						px = add32(S32(p, 0), d[0]); py = add32(S32(p, 4), d[1]); pz = add32(S32(p, 8), d[2]);
						int32_t c0 = (int8_t)(uint8_t)(U8(p, 0x18) + 1), c1 = S8(p, 0x18);
						col = g_held.den > 0 ? lerp_rgb(U32(PTR(WS(), 0xE4), c0 * 4), U32(PTR(WS(), 0xE4), c1 * 4), g_held.num, g_held.den)
							: U32(PTR(WS(), 0xE4), c0 * 4);
					}
					else
					{
						// died on the real tick: drawn as the real tick drew it (life 0)
						px = S32(p, 0); py = S32(p, 4); pz = S32(p, 8);
						col = U32(PTR(WS(), 0xE4), 0);
					}
					uint8_t *v = WS() + 0xF0;                                   // [ebp-4]
					if (U16(desc, 2) == 0)
					{
						U16(v, 0) = (uint16_t)(px >> 8);
						U16(v, 2) = (uint16_t)(py >> 8);
						U16(v, 4) = (uint16_t)(pz >> 8);
					}
					if (U16(desc, 2) == 1)
					{
						int32_t rad = px >> 8;                                   // [ebp-8]
						U16(v, 2) = (uint16_t)(py >> 8);
						int32_t ang = (pz >> 4) & (int32_t)0xFFFFFFF0;           // ebx
						int32_t s = x::Sin(ang);
						U16(v, 0) = (uint16_t)((uint16_t)(mul32(s, rad) >> 12) + U16(CUR(), 0x94));
						int32_t c = x::Cos(ang);
						U16(v, 4) = (uint16_t)((uint16_t)(mul32(c, rad) >> 12) + U16(CUR(), 0x98));
					}
					// (other modes: ws+0xF0 is not written, the stale scratch is projected - vanilla)
					U8(pkt, 3) = 2;
					U32(pkt, 4) = col | 0x6A000000;
					int32_t z = xl_TransformToScreen(WS() + 0xF0, pkt + 8, WS() + 0xFC, WS() + 0xFC);
					uint32_t ot = U32(WS(), 0x64);
					if (ot == 0) ot = U32(RT(), 0x4C) + ((uint32_t)z & ~3u);    // `and al, 0xfc`
					x::InsertPrimAltViewport(ot, pkt);
					pkt += 0xC;
				}
				p += 0x1C;
				slot++;
			} while (--n != 0);
		}
		PTR(CTX(), 0x7C) = pkt;
	}

	// ------------------------------------------------------------------------------------
	// 0xAEA420 (Draw 54 DrawEnvClipWindow), held frames: the packets are built in the private
	// packet memory (ctx+0x7C, advanced by 0x80) instead of the arena buffer (its contents must
	// not change on a held frame, and restoring them after the held draw would re-link the held
	// OT into the real tick's), the patched env is a local copy instead of the scratch above the
	// arena top; no allocation.
	// ------------------------------------------------------------------------------------
	alignas(4) static uint8_t g_local_env[0x1C];

	static uint8_t *held_local_env() { return g_local_env; }

	static uint8_t *held_packet_block()
	{
		uint8_t *pkt = PTR(CTX(), 0x7C);
		PTR(CTX(), 0x7C) = pkt + 0x80;
		return pkt;
	}

	// ------------------------------------------------------------------------------------
	// 0xAE4C70 (Draw 55 ScanlineJitterScreen), held frames: nothing allocated, the phases are
	// not written: the bars are drawn at phase + (s16)speed * num/den (the exact in-between); S is
	// saved and restored around the call (outside the saved set, and vanilla reads it stale).
	// ------------------------------------------------------------------------------------
	static void held_scratch55_save()
	{
		if (!g_held.active) return;
		int32_t v = S16(CUR(), 0x96);
		int32_t n = 0x6C - (mul32(v, 108) >> 8);
		int32_t lo = 0, hi = 0x3D8;
		if (n > 0)
		{
			if (0x2D8 - n < lo) lo = 0x2D8 - n;
			if (0x200 + n > hi) hi = 0x200 + n;
		}
		g_ss.save(PTR(CTX(), 0x74) + lo, (uint32_t)(hi - lo));
	}

	static uint16_t held_phase55(uint8_t *pool, uint8_t *desc, int k)
	{
		int32_t sp = (int16_t)U16(desc, 6 + 2 * k);
		int32_t d = g_held.den > 0 ? sp * g_held.num / g_held.den : 0;
		return (uint16_t)(U16(pool, 2 * k) + (uint16_t)d);
	}

	// ------------------------------------------------------------------------------------
	// 0xAE52C0 (Draw 56 LineRingTunnel), held frames: the draw of the vanilla handler after its
	// ws+0x70/0x74 setup (dh_56_Held, called from held_skip56, which then ends the vanilla
	// handler); no allocation; the work area above the arena top is saved and restored; an
	// in-between outAngleY of 0 (the divisor; the real tick's is non-zero) skips the draw.
	// Geometry, per-row fade and scroll come from the in-between bone. The colour table index
	// (row + column - outPosY / outAngleY + outPosX) turns the colour pattern round the ring by
	// one column per outPosX step: every line end is drawn with the in-between of its colour at
	// the real tick's outPosX and at the next tick's (held_bone_ends), i.e. the pattern half a
	// column round. The reveal (hidden where row + column > outPosX) stays discrete (the
	// in-between outPosX, i.e. the real tick's set until the next tick adds its column).
	// ------------------------------------------------------------------------------------
	// colour of table entry idx, blended toward entry idx + dl (the next tick's) by num/den
	static uint32_t col56(const uint8_t *ctab, int32_t idx, int32_t dl)
	{
		uint32_t c0 = U32(ctab, 4 * idx);
		if (dl == 0 || g_held.den <= 0) return c0;
		uint32_t c1 = U32(ctab, 4 * ((idx + dl) & (S32(WS(), 0xF0))));
		uint32_t c = c0 & 0xFF000000u;
		for (int s = 0; s < 24; s += 8)
		{
			int32_t x0 = (c0 >> s) & 0xFF, x1 = (c1 >> s) & 0xFF;
			c |= (uint32_t)((x0 + (x1 - x0) * g_held.num / g_held.den) & 0xFF) << s;
		}
		return c;
	}

	static void held_scratch56_save(uint8_t *desc);

	static void dh_56_Held()
	{
		uint8_t *desc = PTR(WS(), 0x80);                                            // edi, [ebp-0x14]
		uint8_t *pool = PTR(WS(), 0x84);                                            // [ebp-0x18]
		PTR(WS(), 0x64) = PTR(CTX(), 0x74);
		PTR(WS(), 0x60) = PTR(WS(), 0x64);
		held_scratch56_save(desc);
		int32_t mask = (int32_t)U16(desc, 8);                                      // ebx
		int32_t lx = S16(CUR(), 0x94);                                              // [ebp-4]
		// colour ends: table index with the real tick's outPosX (lx0) and the next tick's (lx1)
		int16_t a0[3], p0[3], a1[3], p1[3];
		int32_t lx0 = lx, dl = 0;
		if (held_bone_ends(CUR(), a0, p0, a1, p1)) { lx0 = p0[0]; dl = p1[0] - p0[0]; }
		int32_t q = S32(WS(), 0x74) / S32(WS(), 0x70);                               // [ebp-8]
		uint8_t *s = PTR(WS(), 0x60);                                               // esi
		mask = sub32(mask, 1);
		// colour index bytes, (rows + 1) x 17
		{
			int32_t r = 0;                                                         // [ebp-0xc]
			do
			{
				int32_t c = r;                                                     // ecx
				for (int k = 0; k < 17; k++)
				{
					if (c > lx)
						*s = 0x80;
					else
						*s = (uint8_t)(((uint8_t)c - (uint8_t)q + (uint8_t)lx0) & (uint8_t)mask);
					s++;
					c++;
				}
				r++;
			} while (r <= (int32_t)U16(desc, 6));
		}
		s = (uint8_t *)(((uint32_t)s + 4) & ~3u);
		PTR(WS(), 0x68) = s;
		int32_t step, accel;                                                        // [ebp-4], [ebp-8]
		{
			uint8_t *w = WS();
			int32_t sp = S32(w, 0x70);                                             // ebx
			int32_t rem = S32(w, 0x74) % sp;
			int32_t half = mul32((int32_t)U16(desc, 6), sp) / 2;
			step = rem;
			U16(w, 0xD2) = (uint16_t)sub32(rem, half);
			accel = shl32(rem, 12) / S32(WS(), 0x70);
		}
		if (U16(desc, 2) == 0)
		{
			int32_t r0 = shl32(S16(CUR(), 0x8C), 12);
			S32(WS(), 0xF8) = r0;
			int32_t d = sub32(shl32(S16(CUR(), 0x90), 12), r0) / (int32_t)U16(desc, 6);
			int32_t f = mul32(accel, d);
			accel = 0;
			step = d;
			S32(WS(), 0xF8) = add32(S32(WS(), 0xF8), f >> 12);
		}
		if (U16(desc, 2) == 1)
		{
			S32(WS(), 0xF8) = shl32(S16(CUR(), 0x8C), 12);
			step = shl32(S16(CUR(), 0x90), 4);
			accel = shl32(S16(CUR(), 0x98), 4);
		}
		Mat4x3 *m = blob::GetParentMatrix(U16(CUR(), 0x9C));                         // ebx
		x::GteSetRotMatrix(m);
		x::GteSetTransVector(m);
		x::GteLoadV0(ZEROVEC());
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
		S32(WS(), 0xC4) = 0x1000;
		// project the rows
		{
			int32_t r = 0;                                                         // [ebp-0xc]
			do
			{
				for (int k = 0; k < 5; k++) U32(WS(), 0xE0 + 4 * k) = U32(m, 4 * k);
				S32(WS(), 0xC8) = S32(WS(), 0xF8) >> 8;
				S32(WS(), 0xC0) = S32(WS(), 0xC8);
				x::ScaleMatrix(WS() + 0xE0, WS() + 0xC0);
				x::SetRotMatrix(WS() + 0xE0);
				for (int c = 0; c < 17; c++)
				{
					U16(WS(), 0xD0) = U16(pool, 8 + 2 * c);
					U16(WS(), 0xD4) = U16(pool, 0x2A + 2 * c);
					int32_t z = xl_TransformToScreen(WS() + 0xD0, s, WS() + 0xFC, WS() + 0xFC);
					U16(s, 4) = (uint16_t)z;
					s += 8;
				}
				step = add32(step, accel);
				S32(WS(), 0xF8) = add32(S32(WS(), 0xF8), step);
				U16(WS(), 0xD2) = (uint16_t)(U16(WS(), 0xD2) + U16(WS(), 0x70));
				r++;
			} while (r <= (int32_t)U16(desc, 6));
		}
		// per-row fade
		PTR(WS(), 0x6C) = s;
		{
			int32_t rows = U16(desc, 6);                                           // [ebp-0x1c]
			int32_t sp = S32(WS(), 0x70);
			int32_t rem = S32(WS(), 0x74) % sp;                                    // ebx
			int32_t inv = 0x10000 / sp;
			int32_t a = sub32(shl32(rows, 6), mul32(rem, inv) >> 10);
			if (rows >= 0)
			{
				int32_t cnt = rows + 1;
				do
				{
					if (a < 0) a = 0;
					S32(s, 0) = a;
					s += 4;
					a = sub32(a, 0x40);
				} while (--cnt != 0);
			}
		}
		// lines
		uint8_t *pkt = PTR(CTX(), 0x7C);                                           // esi
		S32(WS(), 0xF0) = sub32((int32_t)U16(desc, 8), 1);
		uint8_t *ci = PTR(WS(), 0x64);                                              // [ebp-8]
		uint8_t *pv = PTR(WS(), 0x68);                                              // edi
		uint8_t *ctab = PTR(pool, 0);                                               // [ebp-0x10]
		if (U16(desc, 6) != 0)
		{
			// horizontal (along each ring), rows 0..rows-1
			int32_t r = 0;
			do
			{
				int32_t a = S32(PTR(WS(), 0x6C), 4 * r);                           // [ebp-4]
				for (int c = 0; c < 16; c++)
				{
					int32_t idx = S8(ci, 0);                                       // ebx
					if (idx >= 0)
					{
						uint32_t col = col56(ctab, idx, dl);
						if (a < 0x100) col = xl_ScaleColour_B66A20(col, a);
						idx++;
						U32(pkt, 0xC) = col | 0x52000000;
						col = col56(ctab, S32(WS(), 0xF0) & idx, dl);
						if (a < 0x100) col = xl_ScaleColour_B66A20(col, a);
						U32(pkt, 0x14) = col;
						U32(pkt, 4) = 0xE1000220;
						U8(pkt, 3) = 6;
						U32(pkt, 8) = 0;
						U32(pkt, 0x10) = U32(pv, 0);
						U32(pkt, 0x18) = U32(pv, 8);
						int32_t d1 = U16(pv, 0xC), d0 = U16(pv, 4);
						uint32_t ot = ((uint32_t)(add32(d1, d0) / 2) & ~3u) + U32(RT(), 0x4C);
						x::InsertPrimDepthKeys(ot, pkt, d0, d1, 0, 0);
						pkt += 0x1C;
					}
					pv += 8;
					ci++;
				}
				pv += 8;
				ci++;
				r++;
			} while (r < (int32_t)U16(desc, 6));
		}
		S32(WS(), 0xF4) = 0x88;
		ci = PTR(WS(), 0x64);
		pv = PTR(WS(), 0x68);
		ctab = PTR(pool, 0);
		if (U16(desc, 6) != 0)
		{
			// vertical (between rows r and r+1), rows 0..rows-1
			int32_t r = 0;
			do
			{
				uint8_t *ar = PTR(WS(), 0x6C) + 4 * r;                             // [ebp-0x1c]
				for (int c = 0; c < 16; c++)
				{
					int32_t idx = S8(ci, 0);                                       // ebx
					if (idx >= 0)
					{
						uint32_t col = col56(ctab, idx, dl);
						int32_t a = S32(ar, 0);
						if (a < 0x100) col = xl_ScaleColour_B66A20(col, a);
						U32(pkt, 0xC) = col | 0x52000000;
						idx++;
						col = col56(ctab, S32(WS(), 0xF0) & idx, dl);
						a = S32(ar, 4);
						if (a < 0x100) col = xl_ScaleColour_B66A20(col, a);
						U32(pkt, 0x14) = col;
						U32(pkt, 4) = 0xE1000220;
						U8(pkt, 3) = 6;
						U32(pkt, 8) = 0;
						U32(pkt, 0x10) = U32(pv, 0);
						U32(pkt, 0x18) = U32(pv, S32(WS(), 0xF4));
						int32_t d1 = U16(pv, 0xC), d0 = U16(pv, 4);           // 0xAE595D: same-row keys (vanilla)
						uint32_t ot = ((uint32_t)(add32(d1, d0) / 2) & ~3u) + U32(RT(), 0x4C);
						x::InsertPrimDepthKeys(ot, pkt, d0, d1, 0, 0);
						pkt += 0x1C;
					}
					ci++;
					pv += 8;
				}
				pv += 8;
				ci++;
				r++;
			} while (r < (int32_t)U16(desc, 6));
		}
		PTR(CTX(), 0x7C) = pkt;
		held_scratch_restore();
	}

	static bool held_skip56()
	{
		if (!g_held.active) return false;
		if (S32(WS(), 0x70) != 0) dh_56_Held();
		return true;
	}

	static void held_scratch56_save(uint8_t *desc)
	{
		if (!g_held.active) return;
		uint32_t nr = (uint32_t)U16(desc, 6) + 1;
		uint8_t *s0 = PTR(WS(), 0x60);
		uint32_t pv0 = ((uint32_t)s0 + nr * 17 + 4) & ~3u;
		uint32_t end = pv0 + nr * 0x88 + nr * 4;
		g_ss.save(s0, end - (uint32_t)s0);
	}
}

namespace part_eden_d
{
	// ------------------------------------------------------------------------------------
	// 0xAE44E0 (Draw 72 BattleModelPivotAttach), held frames: runs on the in-between pose (so the
	// shadow follows it), but E bytes 0..0x99 (every field written: +0/+1, +0xC..+0x11,
	// +0x1C..+0x21, the anim header +0x60.., +0x94..+0x99) and E's skeleton (0x10 + 0x30 * bones,
	// rebuilt by BuildBoneMatricesFromPose / ComputeBonesWorldMatrices) are saved before and
	// restored after: no battle-entity write persists. The one-shot init writes only the bone
	// (saved by HeldFrame) and E+1 (restored). Never runs while predicting (draw handler).
	// ------------------------------------------------------------------------------------
	static void dh_72_Held()
	{
		static uint8_t saved_e[0x9A];
		static uint8_t saved_sk[0x10 + 0x30 * 256];
		uint8_t *E = PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B));
		uint8_t *sk = PTR(E, 0x64) ? PTR(PTR(E, 0x64), 0) : nullptr;
		uint32_t sk_n = sk ? 0x10 + 0x30 * (uint32_t)U8(sk, 0) : 0;
		memcpy(saved_e, E, sizeof(saved_e));
		if (sk_n) memcpy(saved_sk, sk, sk_n);
		BattleModelPivotAttach();
		if (sk_n) memcpy(sk, saved_sk, sk_n);
		memcpy(E, saved_e, sizeof(saved_e));
	}
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace eden
{
	// held frames of the clone: the engine's (gfc_engine_held.inc)
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_gfc_eden_held()
	{
		using namespace gfc;
		mark_held(0xAE3470);
		register_module_held(206, eden::Ready, eden::Frame);
		register_module_camera(206, eden::Camera);
	}
}
