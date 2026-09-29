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

// 30 fps part of fx_primplayer.cpp: exact in-between records of what play() drew

namespace ff8fx::prim
{
	// what each play drew on this real tick (the held frame shows the same objects in between)
	struct Drawn { int32_t frame; bool paused; };
	static NodeMemo<Drawn, 256> g_drawn;

	static void held_note_drawn(Layout *l, bool paused)
	{
		if (Drawn *d = g_drawn.put(l)) { d->frame = l->frame; d->paused = paused; }
	}

	static uint8_t g_copy[0x8000];
	static Record g_cur[256], g_next[256];

	static int EvalAll(const Layout *l, int32_t frame, bool paused, Record *out)
	{
		const uint8_t *hdr = Header(l);
		int32_t count = rd32(hdr);
		uint32_t size = StateSize(hdr);
		if (count > 256 || size > sizeof(g_copy)) return -1;
		memcpy(g_copy, l->state, size);
		uint8_t *st = g_copy;
		for (int32_t i = 0; i < count; i++)
			st = EvalObject((uint16_t)i, hdr + rd32(hdr + 8 + 4 * i), st, frame, paused, out[i]);
		return count;
	}

	static inline int16_t lerp16(int16_t a, int16_t b, int num, int den) { return (int16_t)(a + ((int32_t)b - a) * num / den); }
	// rotation channels are integrators (value += velocity each frame) of unwrapped angles: the
	// step is the 16-bit difference, whatever its size (a spin of half a turn or more per frame
	// taken "the short way round" would turn the wrong way)
	static inline int16_t lerp_ang(int16_t a, int16_t b, int num, int den)
	{
		int32_t d = (int16_t)(uint16_t)((uint16_t)b - (uint16_t)a);
		return (int16_t)(a + d * num / den);
	}

	// the callbacks draw nothing for an object with a zero scale (all three components)
	static inline bool hidden(const Record &r) { return (r.scale[0] | r.scale[1] | r.scale[2]) == 0; }

	// channel B is the vertex morph of the object's prim model in every callback: b0 / b1 = the
	// two keyframe meshes, b = weight 0..0x1000 (b0 == b1, b == 0: mesh b0; b == 0x1000: mesh b1).
	// Mesh numbers are never blended: the in-between is taken along the morph. A new segment
	// starts where the last one ended ((k, k+1, w) then (k+1, k+2, 0) = mesh k+1): the held frame
	// finishes the current segment ((k, k+1, (w + 0x1000) / 2)). Consecutive segments ((k, k+1, w)
	// then (k+1, k+2, w') with w' > 0) are one morph running through mesh k+1: the in-between is
	// taken along that path (up to mesh k+1 on the first segment, then on the next one).
	static bool single_mesh(const Record &r, int16_t &mesh)
	{
		if (r.b0 == r.b1 || r.b == 0) { mesh = r.b0; return true; }
		if (r.b == 0x1000) { mesh = r.b1; return true; }
		return false;
	}

	static void lerp_morph(Record &r, const Record &n, int num, int den)
	{
		int32_t w0 = r.b, w1;
		int16_t f;
		if (n.b0 == r.b0 && n.b1 == r.b1) w1 = n.b;
		else if (r.b0 != r.b1 && single_mesh(n, f) && (f == r.b1 || f == r.b0)) w1 = f == r.b1 ? 0x1000 : 0;
		else if (n.b0 != n.b1 && single_mesh(r, f) && (f == n.b0 || f == n.b1))
		{
			// the next segment starts from the mesh drawn now
			w0 = f == n.b0 ? 0 : 0x1000;
			w1 = n.b;
			r.b0 = n.b0;
			r.b1 = n.b1;
		}
		else if (r.b0 != r.b1 && n.b0 != n.b1 && n.b0 == r.b1)
		{
			int32_t u = w0 + (0x1000 + n.b - w0) * num / den; // along the path, 0..0x2000
			if (u > 0x1000)
			{
				r.b0 = n.b0;
				r.b1 = n.b1;
				u -= 0x1000;
			}
			r.b = (int16_t)u;
			return;
		}
		else return; // unrelated meshes: the switch happens on the real frame
		r.b = (int16_t)(w0 + (w1 - w0) * num / den);
	}

	void play_held(Layout *l, Callback cb, int arg, int num, int den)
	{
		const Drawn *d = g_drawn.get(l);
		if (!d) return; // this play drew nothing on the real tick
		const uint8_t *hdr = Header(l);
		int32_t total = rd32(hdr + 4);
		// the state after the real tick, evaluated paused at the drawn frame, is exactly the record drawn
		int count = EvalAll(l, d->frame, true, g_cur);
		if (count <= 0) return;
		bool moving = !d->paused && d->frame + 1 < total && l->frame == d->frame + 1;
		if (moving && EvalAll(l, d->frame + 1, false, g_next) != count) moving = false;
		for (int i = 0; i < count; i++)
		{
			Record r = g_cur[i];
			// an object that appears or disappears on the next tick does so on the real frame
			// (blending from / to its hidden record would draw it half-scaled half way to the origin)
			if (moving && !hidden(r) && !hidden(g_next[i]))
			{
				const Record &n = g_next[i];
				for (int c = 0; c < 3; c++)
				{
					r.pos[c] = lerp16(r.pos[c], n.pos[c], num, den);
					r.rot[c] = lerp_ang(r.rot[c], n.rot[c], num, den);
					r.scale[c] = lerp16(r.scale[c], n.scale[c], num, den);
					r.rgb[c] = (uint8_t)(r.rgb[c] + ((int32_t)n.rgb[c] - r.rgb[c]) * num / den);
				}
				r.a = lerp16(r.a, n.a, num, den);
				lerp_morph(r, n, num, den);
			}
			cb(l, &r, arg);
		}
	}

}
