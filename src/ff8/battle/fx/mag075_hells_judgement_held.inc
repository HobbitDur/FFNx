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

// 30 fps part of mag075_hells_judgement.cpp: Hell's Judgement's held frames are the cinematic
// engine's (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real
// ticks, every draw handler re-run in its frozen (boneSkipFlag) form; draw 61 (Eden's
// MeshRadialLightWave) only reads node values. Camera: HeldCamera interpolates the battle camera
// words the VM writes (op 0x039).
// Draw 66 ShatterMesh advances its pieces on every draw (not gated by the freeze flag), so the
// held frame has its own draw: the real tick notes the piece states the draw used (after the
// launches, held75_note); the held draw launches nothing (no rand: a piece still whole on the
// real tick stays whole, launches stay 15 Hz) and draws every piece the real tick drew:
//   whole  -> at its projected vertices (the parent matrix node is the engine's in-between),
//   flying -> at the exact in-between of the drawn state N and the next draw's state N + 1
//             (that draw is one ShatterStep: velocity + acceleration, position + velocity << 4,
//             spin + spin speed; positions are linear in the step, the spin angles take the
//             shortest way), colour = table[state] (the real tick's; table[state - 1] at the
//             full step, the next tick's),
//   flying with state 1 (gone on the next tick) -> held at the real tick's state (ended part).
// The pieces are not written.

#include <vector>

namespace ff8fx
{
namespace gfc
{
namespace hells_judgement
{
	// piece states the real tick's draw used, per draw-66 block
	struct Memo { uint8_t *blk; uint32_t tick; std::vector<int8_t> st; };
	static Memo g_memo[4];

	static void held75_note(uint8_t *blk)
	{
		Memo *m = nullptr;
		for (Memo &x : g_memo) if (x.blk == blk) { m = &x; break; }
		if (!m) for (Memo &x : g_memo) if (x.blk == nullptr || x.tick != g_real_tick) { m = &x; break; }
		if (!m) return;
		m->blk = blk;
		m->tick = g_real_tick;
		const int32_t n = U16(blk, 0xC);
		m->st.resize((size_t)n);
		const uint8_t *p = PTR(blk, 4);
		for (int32_t i = 0; i < n; i++) m->st[(size_t)i] = S8(p + 0x30 * i, 0x2C);
	}

	static const Memo *held75_memo(const uint8_t *blk)
	{
		for (const Memo &x : g_memo) if (x.blk == blk && x.tick == g_real_tick) return &x;
		return nullptr;
	}

	// a flying piece at num/den between its drawn state N (the piece as the real tick left it)
	// and the next draw's state; s = its state on the real tick (1: gone on the next tick, held)
	static void ShatterStepHeld(const uint8_t *p, int8_t s)
	{
		const int num = g_held.num, den = g_held.den;
		const bool move = s > 1 && num > 0;
		x::SetRotMatrix(PTR(WS(), 0x60));
		x::SetTransVector(PTR(WS(), 0x60));
		int32_t pos[3];
		int16_t ang[3];
		for (int k = 0; k < 3; k++)
		{
			const int16_t vel = (int16_t)(U16(p, 0xC + 4 * k) + U16(p, 0xE + 4 * k));   // velocity of the next step
			const int32_t step = shl32(vel, 4);
			pos[k] = move ? add32(S32(p, 4 * k), (int32_t)((int64_t)step * num / den)) : S32(p, 4 * k);
			const int16_t a0 = (int16_t)(U16(p, 0x18 + 2 * k) << 4);
			const int16_t a1 = (int16_t)((uint16_t)(U16(p, 0x18 + 2 * k) + U16(p, 0x1E + 2 * k)) << 4);
			const int32_t d = (int16_t)(a1 - a0);
			ang[k] = move ? (int16_t)(a0 + d * num / den) : a0;
		}
		ShatterXformPos(pos[0], pos[1], pos[2]);
		ShatterXformRot(ang[0], ang[1], ang[2]);
	}

	// colour table index of a flying piece on the held frame
	static inline int32_t held75_colour_index(int8_t s) { return s > 1 && g_held.num >= g_held.den ? s - 1 : s; }

	// local vertices of a flying piece: mesh vertex - centroid (ws+0xC0 + 8 * k)
	static void held75_local(const uint8_t *p, const uint8_t *lst, int o0, int nv)
	{
		for (int k = 0; k < nv; k++)
		{
			const uint8_t *v = PTR(WS(), 0xEC) + U16(lst, o0 + 2 * k);
			U16(WS(), 0xC0 + 8 * k) = (uint16_t)(U16(v, 0) - U16(p, 0x24));
			U16(WS(), 0xC2 + 8 * k) = (uint16_t)(U16(v, 2) - U16(p, 0x26));
			U16(WS(), 0xC4 + 8 * k) = (uint16_t)(U16(v, 4) - U16(p, 0x28));
		}
	}

	// ShatterTris (nv 3) / ShatterQuads (nv 4) of the held frame: states from the memo, pieces
	// not written
	static uint8_t *ShatterPolysHeld(uint8_t *p, const int8_t *&st, uint8_t *verts, uint8_t *mesh, int nv)
	{
		const int vo = nv == 3 ? 0xA : 0xC;                            // first vertex offset of a record
		const int rs = nv == 3 ? 0x14 : 0x18;                          // record size
		const int ps = nv == 3 ? 0x20 : 0x28;                          // packet size
		uint8_t *lst = PTR(WS(), 0xF0);
		uint8_t *pk = PTR(CTX(), 0x7C);
		S32(WS(), 0xF0) = S16(lst, 2);
		lst += 4;
		U32(WS(), 0xF4) = (U32(CUR(), 0xCC) & 0x2000000) | (nv == 3 ? 0x24000000u : 0x2C000000u);
		U32(WS(), 0xF8) = U8(CUR(), 0x92) & 0x60;
		PTR(WS(), 0xEC) = mesh + U32(mesh, 0x14);
		uint8_t *start = pk;
		if (S32(WS(), 0xF0) > 0)
		{
			pk += 0x10;
			do
			{
				const int8_t s = *st;
				if (s != 0)
				{
					int32_t z = 0;
					if (s > 0)
					{
						ShatterStepHeld(p, s);
						held75_local(p, lst, vo, nv);
						uint8_t *w = WS();
						if (nv == 3)
							z = shl32(mul32(xl_RotTransPers3(w + 0xC0, w + 0xC8, w + 0xD0, pk - 8, pk, pk + 8, w + 0xFC, w + 0xFC), 3), 2);
						else
							z = shl32(xl_RotTransPers4(w + 0xC0, w + 0xC8, w + 0xD0, w + 0xD8, pk - 8, pk, pk + 8, pk + 0x10, w + 0xFC, w + 0xFC), 4);
						U32(pk, -0xC) = U32(PTR(WS(), 0x64), held75_colour_index(s) * 4) | U32(WS(), 0xF4);
					}
					else
					{
						for (int k = 0; k < nv; k++) U32(pk, -8 + 8 * k) = U32(verts, U16(lst, vo + 2 * k));
						// SZ sum in the original order (second vertex first)
						z = (int32_t)U16(verts, U16(lst, vo + 2) + 4) + U16(verts, U16(lst, vo) + 4);
						for (int k = 2; k < nv; k++) z += U16(verts, U16(lst, vo + 2 * k) + 4);
						U32(pk, -0xC) = U32(WS(), 0xF4) | U32(lst, 0);
					}
					U8(pk, -0xD) = nv == 3 ? 7 : 9;
					U16(pk, 6) = (uint16_t)(U16(WS(), 0xF8) | U16(lst, nv == 3 ? 0x12 : 0x16));
					U16(pk, -2) = U16(lst, nv == 3 ? 0x10 : 0x14);
					U16(pk, -4) = U16(lst, 4);
					U16(pk, 4) = U16(lst, 6);
					U16(pk, 0xC) = U16(lst, 8);
					if (nv == 4) U16(pk, 0x14) = U16(lst, 0xA);
					xl_InsertPrim(nv == 3 ? BucketG3(z) : BucketG4(z), start);
					start += ps;
					pk += ps;
				}
				S32(WS(), 0xF0) = S32(WS(), 0xF0) - 1;
				lst += rs;
				p += 0x30;
				st++;
			} while (S32(WS(), 0xF0) > 0);
		}
		PTR(CTX(), 0x7C) = start;
		PTR(WS(), 0xF0) = lst;
		return p;
	}

	// draw 66 on a held frame: no launches, pieces from the memo of the real tick
	static void dh_66_ShatterMeshHeld()
	{
		uint8_t *bone = CUR();
		uint8_t *blk = PTR(bone, 0xBC);
		const Memo *m = blk ? held75_memo(blk) : nullptr;
		if (!m || m->st.size() != (size_t)U16(blk, 0xC)) return;
		uint8_t *mesh = PTR(blk, 8);
		PTR(WS(), 0x60) = (uint8_t *)blob::GetParentMatrix(U16(bone, 0x9C));
		PTR(WS(), 0x64) = PTR(blk, 0);
		h_B27130();
		uint8_t *out = PTR(CTX(), 0x74);
		const uint8_t *v = mesh + U32(mesh, 0x14);
		for (int32_t k = S32(mesh, 0x18); k > 0; k--)
		{
			x::GteWriteData((int32_t)(((uint32_t)U16(v, 2) << 16) | U16(v, 0)), 0);
			x::GteWriteData((int32_t)U16(v, 4), 1);
			x::GteRTPS();
			x::GteReadSXY2(out);
			U16(out, 4) = MEM<uint16_t>(0x1CA8A5C);
			v += 8;
			out += 8;
		}
		uint8_t *lst = mesh + U32(mesh, 8);
		uint8_t *verts = PTR(CTX(), 0x74);
		uint8_t *p = PTR(blk, 4);
		const int8_t *st = m->st.data();
		const int8_t *st_end = st + m->st.size();
		for (;;)
		{
			const int32_t type = S16(lst, 0);
			const int32_t n = S16(lst, 2);
			if (type != 8 && type != 0x12) return;
			if (n > 0 && st + n > st_end) return;   // more polygons than pieces (never: ShatterInit counted them)
			PTR(WS(), 0xF0) = lst;
			p = ShatterPolysHeld(p, st, verts, mesh, type == 8 ? 3 : 4);
			lst = PTR(WS(), 0xF0);
		}
	}

	static bool Ready() { return HeldReady(g_hj); }
	static void Frame(int num, int den) { HeldFrame(g_hj, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_hj, num, den, world, lookat); }
}
}

	static void register_mag075_hells_judgement_held()
	{
		using namespace gfc;
		mark_held(0xB3EF50);
		register_module_held(75, hells_judgement::Ready, hells_judgement::Frame);
		register_module_camera(75, hells_judgement::Camera);
	}
}
