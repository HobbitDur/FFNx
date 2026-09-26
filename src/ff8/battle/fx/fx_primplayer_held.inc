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
	static inline int16_t lerp_ang(int16_t a, int16_t b, int num, int den)
	{
		int32_t d = (((int32_t)b - a + 2048) & 4095) - 2048; // 12-bit angles, the short way round
		return (int16_t)(a + d * num / den);
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
			if (moving)
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
				r.b = lerp16(r.b, n.b, num, den);
				r.b0 = lerp16(r.b0, n.b0, num, den);
				r.b1 = lerp16(r.b1, n.b1, num, den);
			}
			cb(l, &r, arg);
		}
	}

}
