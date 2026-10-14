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

// 30 fps part of mag276_xatm092_death.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag276_xatm092_death.cpp; shared pieces in mag_common_held.h / fx_held.h).
//
// Every drawing task draws THEN updates. A real tick records every draw in order with the GTE
// registers and the Field_Alloc scratch it started from. A held frame replays the log in the same
// order, each part at its in-between state (tick + num / den); discrete things are those of tick N
// up to half way and those of tick N+1 after (fraction 1/2 keeps tick N's); a part the next tick no
// longer draws (its task ended / the piece died) is drawn up to half way only:
//   Arcs       - the tick's jittered points (the jitter stays 15 Hz), each point's brightness half
//                way to the next tick's (the task's fade after its draw).
//   Prim model - prim::play_held with the tick's parameter block.
//   Sparks     - fixed position; flipbook frame discrete.
//   Falling sparks / smoke - position half way along the velocity step; flipbook frame discrete.
//   Debris     - position half way along the velocity step; the vertex offsets turned by the same
//                fraction of the piece's per-tick spin matrix (axis-angle of its rotation part,
//                scale ^ fraction): forward from tick N up to half way, back from tick N+1 after,
//                so fraction 0 / 1 are the ticks' exact vertices; at 1/2 the half spin matrix
//                (4.12) through the GTE exactly as the director's update applies its spin.
//   White screen - level half way to the next tick's.
//   Caster     - its in-between pose (pose_midpoint) without the bones hidden on tick N.
// Packets: the module arena (it alternates with the tick's parity; past half way the other one's
// bytes lie under the draws, the next tick's) and the frame arena (arcs, caster) go to private
// copies, the transformed-vertex workspace too; the prim callback's morph scratch in the magic
// buffer is put back. No task has a draw-only mode.

#include <math.h>

namespace ff8fx
{
namespace xatm276
{
	static const uint32_t SCRATCH = 0x400;
	static const uint32_t ARENA_SIZE = 0x10000;
	static const uint32_t MORPH_SIZE = BUF_PROFILE - BUF_MORPH;
	static const int MAX_RECS = 320, MAX_ARCS = 64;
	enum : uint8_t { REC_ARC = 0x10, REC_PRIM = 0x11, REC_DIRECTOR = 0x12 }; // + SPR_SPARK / SPR_FALL / SPR_SMOKE

	struct Rec
	{
		uint8_t kind;
		uint32_t node;
		uint8_t copy[0x20];       // the node as drawn (sprites)
		uint32_t aux;             // arc buffer index
		PrimArg arg;              // prim parameter block
		uint8_t gte[GTE_DATA_SIZE + GTE_CTRL_SIZE];
		uint8_t scratch[SCRATCH];
	};
	struct ArcMemo { uint8_t points[0x80]; uint32_t residue; };
	struct DirectorMemo { int16_t counter; uint8_t list[0x37]; uint8_t debris[0x80 * 0x48]; };

	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static uint32_t g_log_tick = 0xFFFFFFFF;
	static int g_nrec = 0, g_narc = 0;
	static uint32_t g_packets = 0;      // module packet cursor at the tick's start (arena start)
	static uint32_t g_frame_start = 0;  // frame arena cursor at the tick's first frame-arena draw
	static Rec g_recs[MAX_RECS];
	static ArcMemo g_arcs[MAX_ARCS];
	static DirectorMemo g_dir;
	static uint8_t g_held_packets[ARENA_SIZE];
	static uint8_t g_held_frame[0x40000];
	static uint8_t g_held_verts[0x10000];
	static uint8_t g_director_copy[0x54 + 0x80 * 0x48];

	inline uint32_t &BoneWorkspace() { return var<uint32_t>(0x1D98B3C); }

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_log_tick = g_real_tick;
		g_nrec = 0;
		g_narc = 0;
		g_packets = PacketCursor();
		g_frame_start = 0;
	}

	static Rec *NewRec(uint8_t kind, uint32_t n)
	{
		if (g_log_tick != g_real_tick || g_nrec >= MAX_RECS) return nullptr;
		Rec &r = g_recs[g_nrec++];
		r.kind = kind;
		r.node = n;
		memcpy(r.copy, (const void *)n, sizeof(r.copy));
		memcpy(r.gte, (const void *)GTE_DATA, GTE_DATA_SIZE);
		memcpy(r.gte + GTE_DATA_SIZE, (const void *)GTE_CTRL, GTE_CTRL_SIZE);
		memcpy(r.scratch, var<uint8_t *>(0x1D999C4), SCRATCH);
		return &r;
	}

	static void held_note_arc(uint32_t n, uint32_t points, uint32_t residue)
	{
		if (g_narc >= MAX_ARCS) return;
		Rec *r = NewRec(REC_ARC, n);
		if (!r) return;
		r->aux = (uint32_t)g_narc;
		ArcMemo &m = g_arcs[g_narc++];
		memcpy(m.points, (const void *)points, sizeof(m.points));
		m.residue = residue;
		if (!g_frame_start) g_frame_start = FrameCursor();
	}

	static void held_note_prim(uint32_t n, const PrimArg *a)
	{
		Rec *r = NewRec(REC_PRIM, n);
		if (r) r->arg = *a;
	}

	static void held_note_sprite(uint8_t kind, uint32_t n) { NewRec(kind, n); }

	static void held_note_director(uint32_t n)
	{
		if (!NewRec(REC_DIRECTOR, n)) return;
		g_dir.counter = S16(n, 0xC);
		memcpy(g_dir.list, (const void *)(n + 0xE), sizeof(g_dir.list));
		memcpy(g_dir.debris, (const void *)(n + 0x54), sizeof(g_dir.debris));
		if (g_dir.counter <= 0x6C && !g_frame_start) g_frame_start = FrameCursor();
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_log_tick == g_real_tick; }

	// a node still running after the tick (its task did not end)
	static inline bool Alive(uint32_t n, uint32_t fn) { return (U16(n, 0) & 1) != 0 && U32(n, 8) == fn; }

	// ------------------------------------------------------------------
	// Debris spin: the vertex offset turned by `f` of the 3x3 spin matrix (4.12) R = s * Q:
	// Q's axis-angle by the fraction, s ^ f
	// ------------------------------------------------------------------
	static void SpinFraction(const int16_t *R, const int16_t *v, double f, int16_t *out)
	{
		double m[3][3];
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) m[i][j] = R[3 * i + j] / 4096.0;
		const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
		double r[3];
		if (det <= 1e-9)
		{
			for (int i = 0; i < 3; i++) r[i] = v[i] + f * ((m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2]) - v[i]);
		}
		else
		{
			const double s = cbrt(det);
			double q[3][3];
			for (int i = 0; i < 3; i++)
				for (int j = 0; j < 3; j++) q[i][j] = m[i][j] / s;
			// quaternion of q (w, x, y, z)
			double w, x, y, z;
			const double tr = q[0][0] + q[1][1] + q[2][2];
			if (tr > 0)
			{
				const double k = 0.5 / sqrt(tr + 1.0);
				w = 0.25 / k; x = (q[2][1] - q[1][2]) * k; y = (q[0][2] - q[2][0]) * k; z = (q[1][0] - q[0][1]) * k;
			}
			else if (q[0][0] > q[1][1] && q[0][0] > q[2][2])
			{
				const double k = 2.0 * sqrt(1.0 + q[0][0] - q[1][1] - q[2][2]);
				w = (q[2][1] - q[1][2]) / k; x = 0.25 * k; y = (q[0][1] + q[1][0]) / k; z = (q[0][2] + q[2][0]) / k;
			}
			else if (q[1][1] > q[2][2])
			{
				const double k = 2.0 * sqrt(1.0 + q[1][1] - q[0][0] - q[2][2]);
				w = (q[0][2] - q[2][0]) / k; x = (q[0][1] + q[1][0]) / k; y = 0.25 * k; z = (q[1][2] + q[2][1]) / k;
			}
			else
			{
				const double k = 2.0 * sqrt(1.0 + q[2][2] - q[0][0] - q[1][1]);
				w = (q[1][0] - q[0][1]) / k; x = (q[0][2] + q[2][0]) / k; y = (q[1][2] + q[2][1]) / k; z = 0.25 * k;
			}
			const double n = sqrt(w * w + x * x + y * y + z * z);
			w /= n; x /= n; y /= n; z /= n;
			if (w < 0) { w = -w; x = -x; y = -y; z = -z; } // the short way round
			const double half = acos(w < 1.0 ? w : 1.0);
			const double sh = sin(half);
			double ax = 0, ay = 0, az = 0;
			if (sh > 1e-12) { ax = x / sh; ay = y / sh; az = z / sh; }
			const double a = half * f; // half of the turned angle
			const double cw = cos(a), sw = sin(a);
			const double qw = cw, qx = ax * sw, qy = ay * sw, qz = az * sw;
			// v' = q v q*
			const double vx = v[0], vy = v[1], vz = v[2];
			const double tx = 2.0 * (qy * vz - qz * vy), ty = 2.0 * (qz * vx - qx * vz), tz = 2.0 * (qx * vy - qy * vx);
			const double sc = pow(s, f);
			r[0] = sc * (vx + qw * tx + (qy * tz - qz * ty));
			r[1] = sc * (vy + qw * ty + (qz * tx - qx * tz));
			r[2] = sc * (vz + qw * tz + (qx * ty - qy * tx));
		}
		for (int i = 0; i < 3; i++)
		{
			double t = floor(r[i] + 0.5);
			if (t > 32767) t = 32767;
			if (t < -32768) t = -32768;
			out[i] = (int16_t)t;
		}
	}

	// the half spin of a 3x3 spin matrix (4.12): the half angle of its rotation part about the same axis, times the square
	// root of its uniform scale. A held frame at 1/2 turns the vertex offsets with it as the director's own update does
	// with the full spin (0x6855F7..0x685662).
	static void HalfSpin(const int16_t *R, int16_t *H)
	{
		double m[3][3];
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) m[i][j] = R[3 * i + j] / 4096.0;
		const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
		const double s = det > 1e-9 ? cbrt(det) : 1.0;
		double q[3][3];
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) q[i][j] = m[i][j] / s;
		double w, x, y, z;
		const double tr = q[0][0] + q[1][1] + q[2][2];
		if (tr > 0) { const double k = 0.5 / sqrt(tr + 1.0); w = 0.25 / k; x = (q[2][1] - q[1][2]) * k; y = (q[0][2] - q[2][0]) * k; z = (q[1][0] - q[0][1]) * k; }
		else if (q[0][0] > q[1][1] && q[0][0] > q[2][2]) { const double k = 2.0 * sqrt(1.0 + q[0][0] - q[1][1] - q[2][2]); w = (q[2][1] - q[1][2]) / k; x = 0.25 * k; y = (q[0][1] + q[1][0]) / k; z = (q[0][2] + q[2][0]) / k; }
		else if (q[1][1] > q[2][2]) { const double k = 2.0 * sqrt(1.0 + q[1][1] - q[0][0] - q[2][2]); w = (q[0][2] - q[2][0]) / k; x = (q[0][1] + q[1][0]) / k; y = 0.25 * k; z = (q[1][2] + q[2][1]) / k; }
		else { const double k = 2.0 * sqrt(1.0 + q[2][2] - q[0][0] - q[1][1]); w = (q[1][0] - q[0][1]) / k; x = (q[0][2] + q[2][0]) / k; y = (q[1][2] + q[2][1]) / k; z = 0.25 * k; }
		double n = sqrt(w * w + x * x + y * y + z * z);
		w /= n; x /= n; y /= n; z /= n;
		if (w < 0) { w = -w; x = -x; y = -y; z = -z; }
		w += 1.0;
		n = sqrt(w * w + x * x + y * y + z * z);
		w /= n; x /= n; y /= n; z /= n;
		const double hs = sqrt(s);
		const double r[3][3] = {
			{ 1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y) },
			{ 2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x) },
			{ 2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y) } };
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
			{
				double v = floor(r[i][j] * hs * 4096.0 + 0.5);
				if (v > 32767) v = 32767;
				if (v < -32768) v = -32768;
				H[3 * i + j] = (int16_t)v;
			}
	}

	// the debris of tick N at tick + num / den (records the next tick no longer has: up to half way)
	static void DebrisHeld(uint32_t node, int num, int den, bool late)
	{
		const uint32_t c = (uint32_t)g_director_copy;
		memcpy((void *)c, (const void *)node, 0x54);
		for (int i = 0; i < 0x80; i++)
		{
			const uint32_t m = (uint32_t)g_dir.debris + 0x48 * i;
			const uint32_t cur = node + 0x54 + 0x48 * i;
			const uint32_t out = c + 0x54 + 0x48 * i;
			memcpy((void *)out, (const void *)m, 0x48);
			if (S16(m, 6) == 0) continue;
			const bool next = S16(cur, 6) != 0 && S16(cur, 0xE) == (int16_t)(S16(m, 0xE) - 1);
			if (late && !next) { S16(out, 6) = 0; continue; }
			if (num == 0) continue;
			for (int k = 0; k < 3; k++) S16(out, 2 * k) = lerp16(S16(m, 2 * k), (int16_t)(S16(m, 2 * k) + S16(m, 8 + 2 * k)), num, den);
			const int16_t *R = (const int16_t *)(m + 0x10);
			if (!late && 2 * num == den)
			{
				Mat4x3 H;
				memcpy(&H, (const void *)(m + 0x10), sizeof(H));
				HalfSpin(R, &H.m[0][0]);
				GteSetRotCtrl((uint32_t)&H);
				GteLoadV0((const void *)(m + 0x24));
				GteMVMVA_RotV0();
				GteStoreIR123((void *)(out + 0x24));
				GteLoadV0u((const void *)(m + 0x2A));
				GteMVMVA_RotV0();
				GteStoreIR123((void *)(out + 0x2A));
				GteLoadV0((const void *)(m + 0x30));
				GteMVMVA_RotV0();
				GteStoreIR123((void *)(out + 0x30));
				GteLoadV0u((const void *)(m + 0x36));
				GteMVMVA_RotV0();
				GteStoreIR123((void *)(out + 0x36));
				continue;
			}
			for (int v = 0; v < 4; v++)
			{
				int16_t *o = (int16_t *)(out + 0x24 + 6 * v);
				if (late && num == den) { memcpy(o, (const void *)(cur + 0x24 + 6 * v), 6); continue; }
				if (late) SpinFraction(R, (const int16_t *)(cur + 0x24 + 6 * v), (double)(num - den) / den, o);
				else SpinFraction(R, (const int16_t *)(m + 0x24 + 6 * v), (double)num / den, o);
			}
		}
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		DrawDebris(c);
	}

	// the white screen level of a director tick (-1 = none), as DrawWhite draws it
	static int32_t WhiteLevel(int32_t c)
	{
		if ((uint32_t)(c - 0x64) < 0x10) return (int32_t)(((uint32_t)(c - 0x64) & 0xFFFFF) << 8);
		const uint32_t e = (uint32_t)(c - 0x7C);
		if (e < 0x14) return (int32_t)(0x1000 - (uint32_t)(((uint64_t)(e << 12) * 0xCCCCCCCDu) >> 36));
		if (c >= 0x74 && c < 0x7C) return 0x1000;
		return -1;
	}

	static void DirectorHeld(uint32_t node, int num, int den, bool late)
	{
		const int32_t c = g_dir.counter, nc = c + 1;
		const bool next = nc < 0x90;
		if (late && !next) return;
		DebrisHeld(node, num, den, late);
		const int32_t l0 = WhiteLevel(c), l1 = next ? WhiteLevel(nc) : -1;
		int32_t level = -1;
		if (l0 >= 0 && l1 >= 0) level = lerp_i(l0, l1, num, den);
		else if (l0 >= 0 && !late) level = l0;
		else if (l1 >= 0 && late) level = l1;
		if (level >= 0) PacketCursor() = Tile(0xFF, 0xFF, 0xFF, level, PacketCursor());
		if (c <= 0x6C && !(late && nc > 0x6C))
		{
			const uint32_t e = Caster();
			pose_midpoint((void *)(e + 0x60), (void *)(e + 0x6C), num, den);
			DrawCaster(e, (uint32_t)g_dir.list);
		}
	}

	static void ArcHeld(const Rec &r, int num, int den, bool late)
	{
		const uint32_t n = r.node;
		if (late && !Alive(n, ORIG_ArcTask)) return;
		const ArcMemo &m = g_arcs[r.aux];
		const uint32_t b = (uint32_t)FieldAlloc(0x80);
		memcpy((void *)b, m.points, sizeof(m.points));
		for (int k = 0; k < 8; k++) S16(b, 0x10 * k + 0xE) = lerp16(S16(b, 0x10 * k + 0xE), S16(n, 0x1E + 6 * k), num, den);
		ArcDraw(b, m.residue);
		FieldFree(0x80);
	}

	static void PrimHeld(const Rec &r, int num, int den, bool late)
	{
		if (late && !Alive(r.node, ORIG_PrimTask)) return;
		PrimArg a = r.arg;
		prim::play_held((prim::Layout *)(r.node + 0x18), PrimCallback, (int)&a, num, den);
	}

	static void SpriteHeld(const Rec &r, int num, int den, bool late)
	{
		const uint32_t n = r.node;
		const uint32_t fn = r.kind == SPR_SPARK ? ORIG_SparkTask : r.kind == SPR_FALL ? ORIG_FallTask : ORIG_SmokeTask;
		const bool alive = Alive(n, fn);
		if (late && !alive) return;
		uint8_t t[0x20];
		memcpy(t, late ? (const void *)n : (const void *)r.copy, sizeof(t));
		const uint32_t p = (uint32_t)t;
		if (r.kind == SPR_SPARK)
		{
			DrawSpark(p);
			return;
		}
		// the position half way between the drawn one and the one the update left
		const uint32_t d = (uint32_t)r.copy;
		for (int k = 0; k < 3; k++) S16(p, 0xC + 2 * k) = lerp16(S16(d, 0xC + 2 * k), S16(n, 0xC + 2 * k), num, den);
		DrawMoving(p, r.kind == SPR_FALL ? SEQ_Fall : SEQ_Smoke);
	}

	static void HeldFrame(int num, int den)
	{
		const bool late = 2 * num > den;
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_packets);
		// past half way the draws are the next tick's: under them, what the next tick's arena (the
		// other one of the pair) holds
		if (late && g_packets)
		{
			const uint32_t a0 = Buffer() + BUF_ARENA0, a1 = Buffer() + BUF_ARENA1;
			memcpy(g_held_packets, (const void *)(g_packets == a0 ? a1 : a0), sizeof(g_held_packets));
		}
		// the frame arena, the vertex workspace and the morph scratch go to / come back from copies
		const uint32_t frame = FrameCursor(), verts = BoneWorkspace();
		if (g_frame_start && frame > g_frame_start)
		{
			const uint32_t used = frame - g_frame_start;
			memcpy(g_held_frame, (const void *)g_frame_start, used < sizeof(g_held_frame) ? used : sizeof(g_held_frame));
		}
		if (verts) memcpy(g_held_verts, (const void *)verts, sizeof(g_held_verts));
		FrameCursor() = (uint32_t)g_held_frame;
		BoneWorkspace() = (uint32_t)g_held_verts;
		static uint8_t morph[MORPH_SIZE];
		memcpy(morph, (const void *)(Buffer() + BUF_MORPH), MORPH_SIZE);
		for (int i = 0; i < g_nrec; i++)
		{
			const Rec &r = g_recs[i];
			memcpy((void *)GTE_DATA, r.gte, GTE_DATA_SIZE);
			memcpy((void *)GTE_CTRL, r.gte + GTE_DATA_SIZE, GTE_CTRL_SIZE);
			memcpy(var<uint8_t *>(0x1D999C4), r.scratch, SCRATCH);
			switch (r.kind)
			{
			case REC_ARC: ArcHeld(r, num, den, late); break;
			case REC_PRIM: PrimHeld(r, num, den, late); break;
			case REC_DIRECTOR: DirectorHeld(r.node, num, den, late); break;
			default: SpriteHeld(r, num, den, late); break;
			}
		}
		memcpy((void *)(Buffer() + BUF_MORPH), morph, MORPH_SIZE);
		BoneWorkspace() = verts;
		FrameCursor() = frame;
	}
}

	static void register_mag276_held()
	{
		mark_held(xatm276::ORIG_ArcTask);
		mark_held(xatm276::ORIG_PrimTask);
		mark_held(xatm276::ORIG_SparkTask);
		mark_held(xatm276::ORIG_FallTask);
		mark_held(xatm276::ORIG_SmokeTask);
		mark_held(xatm276::ORIG_Director);
		register_module_held(276, xatm276::HeldReady, xatm276::HeldFrame);
	}
}
