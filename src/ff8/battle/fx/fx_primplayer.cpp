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

// Shared effect prim-model player (MAG_011_sub_701970 = PrimPlayer_Play, ~300 callers in the
// effect modules) - native twin, held-frame midpoints and a live self-check.
//
// Layout (the caller's player block):
//   +0 data  (u8 *)   animation data; header = data + *(int32 *)(data + 4)
//   +4 frame (int32)  frame counter (incremented after each unpaused play)
//   +8 state          per-object integrator state, objects back to back
// Header: +0 object count, +4 total frames, +8 int32 object offsets (from the header).
// Object: +0 flags (u32; low byte copied into the record), +4 track flags tf (u32),
//   +8 int32 key-table offsets (from the object), one per keyed channel, in channel order.
// Channels, in order (state slots only for the channels the flags name):
//   position  tf bits 0-2   value/vel/acc  3 x int32 each (16.16), key table 16-byte {key, v[3]}
//   rotation  tf bits 3-5   same
//   scale     tf bits 6-8   same (record default 0x1000)
//   colour    tf bits 9-11  value/vel/acc  3 x int16 + pad each, key table 12-byte {key, w[3]}
//   channel A tf bit 12     value/vel/acc  int32 each, clamped 0..0x10000000, 16-byte keys
//   channel B tf bit 13     u16 a, u16 b, int32 c, int32 vel, int32 acc; 20-byte keys
// A bit 0/1/2 set means "value/velocity/acceleration keyed"; any bit gives the value slot,
// bits 1|2 the velocity slot, bit 2 the acceleration slot. Unpaused plays integrate
// vel += acc, value += vel before the keys of the current frame overwrite slots.
// For every object the player fills a 44-byte record and calls cb(layout, record, arg).

#include "fx_port.h"
#include "../../../patch.h"
#include "../../../log.h"

namespace ff8fx::prim
{
	static_assert(sizeof(Record) == 44, "prim record");

	static inline int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
	static inline int32_t rd32(const uint8_t *p) { return *(const int32_t *)p; }

	// keyframe search shared by 0x7017E0 (stride 16), 0x701790 (stride 12) and the inline
	// lookups of channels A (16) and B (20): first key >= frame, the list ends with key -1
	template<int STRIDE>
	static const uint8_t *FindKey(const uint8_t *p, int32_t frame)
	{
		int32_t k = rd32(p);
		if (k < frame)
		{
			for (;;)
			{
				if (k == -1) return nullptr;
				p += STRIDE;
				k = rd32(p);
				if (k >= frame) break;
			}
		}
		return k == frame ? p : nullptr;
	}

	// 0x701820: one 3-component integrator channel; returns the state cursor after it
	static uint8_t *TrackEval(uint32_t f, const uint8_t *obj, const uint8_t *&keys, uint8_t *st,
		int32_t frame, int16_t *out, bool paused)
	{
		int32_t *a = (int32_t *)st;
		bool write = false;
		if (paused) write = (f & 7) != 0;
		else if (f & 4)
		{
			a[3] = add32(a[3], a[6]); a[4] = add32(a[4], a[7]); a[5] = add32(a[5], a[8]);
			a[0] = add32(a[0], a[3]); a[1] = add32(a[1], a[4]); a[2] = add32(a[2], a[5]);
			write = true;
		}
		else if (f & 2)
		{
			a[0] = add32(a[0], a[3]); a[1] = add32(a[1], a[4]); a[2] = add32(a[2], a[5]);
			write = true;
		}
		else write = (f & 1) != 0;
		if (write) { out[0] = (int16_t)(a[0] >> 16); out[1] = (int16_t)(a[1] >> 16); out[2] = (int16_t)(a[2] >> 16); }

		if (f & 1)
		{
			const uint8_t *k = FindKey<16>(obj + rd32(keys), frame);
			keys += 4;
			if (k) memcpy(st, k + 4, 12);
			out[0] = (int16_t)(a[0] >> 16); out[1] = (int16_t)(a[1] >> 16); out[2] = (int16_t)(a[2] >> 16);
			st += 12;
		}
		else if (f & 6) st += 12;
		if (f & 2)
		{
			const uint8_t *k = FindKey<16>(obj + rd32(keys), frame);
			keys += 4;
			if (k) memcpy(st, k + 4, 12);
			st += 12;
		}
		else if (f & 4) st += 12;
		if (f & 4)
		{
			const uint8_t *k = FindKey<16>(obj + rd32(keys), frame);
			keys += 4;
			if (k) memcpy(st, k + 4, 12);
			st += 12;
		}
		return st;
	}

	static inline uint16_t clamp16(int32_t v) { return v < 0 ? 0 : (v > 0xFFFF ? 0xFFFF : (uint16_t)v); }

	// one object of 0x701970: fills the record, advances the state cursor
	static uint8_t *EvalObject(uint16_t index, const uint8_t *obj, uint8_t *st, int32_t frame, bool paused, Record &r)
	{
		memset(&r, 0, sizeof(r));
		r.index = index;
		r.scale[0] = r.scale[1] = r.scale[2] = 0x1000;
		uint32_t flags = *(const uint32_t *)obj;
		r.flags_lo = (uint16_t)(flags & 0xFF);
		r.flags = flags;
		uint32_t tf = *(const uint32_t *)(obj + 4);
		const uint8_t *keys = obj + 8;
		st = TrackEval(tf & 0xFF, obj, keys, st, frame, r.pos, paused);
		st = TrackEval((tf >> 3) & 0xFF, obj, keys, st, frame, r.rot, paused);
		st = TrackEval((tf >> 6) & 0xFF, obj, keys, st, frame, r.scale, paused);

		// colour: value (u16 x3, pad) / velocity (s16 x3, pad) / acceleration
		uint16_t *cv = (uint16_t *)st;
		int16_t *cd = (int16_t *)(st + 8), *cdd = (int16_t *)(st + 16);
		bool write = false;
		if (paused) write = (tf & 0xE00) != 0;
		else
		{
			bool integrate = false;
			if (tf & 0x800)
			{
				cd[0] = (int16_t)(cd[0] + cdd[0]); cd[1] = (int16_t)(cd[1] + cdd[1]); cd[2] = (int16_t)(cd[2] + cdd[2]);
				integrate = true;
			}
			else if (tf & 0x400) integrate = true;
			if (integrate)
			{
				for (int c = 0; c < 3; c++) cv[c] = clamp16((int32_t)cv[c] + cd[c]);
				write = true;
			}
			else write = (tf & 0x200) != 0;
		}
		if (write) { r.rgb[0] = (uint8_t)(cv[0] >> 8); r.rgb[1] = (uint8_t)(cv[1] >> 8); r.rgb[2] = (uint8_t)(cv[2] >> 8); r.rgb[3] = 0; }
		if (tf & 0x200)
		{
			const uint8_t *k = FindKey<12>(obj + rd32(keys), frame);
			keys += 4;
			if (k) memcpy(st, k + 4, 6);
			r.rgb[0] = (uint8_t)(cv[0] >> 8); r.rgb[1] = (uint8_t)(cv[1] >> 8); r.rgb[2] = (uint8_t)(cv[2] >> 8); r.rgb[3] = 0;
			st += 8;
		}
		else if (tf & 0xC00) st += 8;
		if (tf & 0x400)
		{
			const uint8_t *k = FindKey<12>(obj + rd32(keys), frame);
			keys += 4;
			if (k) memcpy(st, k + 4, 6);
			st += 8;
		}
		else if (tf & 0x800) st += 8;
		if (tf & 0x800)
		{
			const uint8_t *k = FindKey<12>(obj + rd32(keys), frame);
			keys += 4;
			if (k) memcpy(st, k + 4, 6);
			st += 8;
		}

		if (tf & 0x1000) // channel A: int32 value / vel / acc, value clamped 0..0x10000000
		{
			int32_t *a = (int32_t *)st;
			if (!paused)
			{
				a[1] = add32(a[1], a[2]);
				int32_t s = add32(a[0], a[1]);
				a[0] = s < 0 ? 0 : (s > 0x10000000 ? 0x10000000 : s);
			}
			const uint8_t *k = FindKey<16>(obj + rd32(keys), frame);
			keys += 4;
			if (k) memcpy(st, k + 4, 12);
			r.a = (int16_t)(a[0] >> 16);
			st += 12;
		}
		if (tf & 0x2000) // channel B: u16 x2, int32 value / vel / acc (value clamped)
		{
			int32_t *c = (int32_t *)(st + 4);
			if (!paused)
			{
				c[1] = add32(c[1], c[2]);
				int32_t s = add32(c[0], c[1]);
				c[0] = s < 0 ? 0 : (s > 0x10000000 ? 0x10000000 : s);
			}
			const uint8_t *k = FindKey<20>(obj + rd32(keys), frame);
			keys += 4;
			if (k) memcpy(st, k + 4, 16);
			r.b0 = *(int16_t *)st;
			r.b1 = *(int16_t *)(st + 2);
			r.b = (int16_t)(c[0] >> 16);
			st += 16;
		}
		return st;
	}

	static inline const uint8_t *Header(const Layout *l) { return l->data + rd32(l->data + 4); }

	// state bytes of all objects (for the copies the held frames and the self-check work on)
	static uint32_t StateSize(const uint8_t *hdr)
	{
		int32_t count = rd32(hdr);
		uint32_t size = 0;
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t tf = *(const uint32_t *)(hdr + rd32(hdr + 8 + 4 * i) + 4);
			for (int ch = 0; ch < 3; ch++)
			{
				uint32_t f = (tf >> (3 * ch)) & 7;
				if (f) size += 12;
				if (f & 6) size += 12;
				if (f & 4) size += 12;
			}
			uint32_t c = (tf >> 9) & 7;
			if (c) size += 8;
			if (c & 6) size += 8;
			if (c & 4) size += 8;
			if (tf & 0x1000) size += 12;
			if (tf & 0x2000) size += 16;
		}
		return size;
	}

	// what each play drew on this real tick (the held frame shows the same objects in between)
	struct Drawn { int32_t frame; bool paused; };
	static NodeMemo<Drawn, 256> g_drawn;

	int play(Layout *l, Callback cb, int arg, int paused)
	{
		const uint8_t *hdr = Header(l);
		int32_t total = rd32(hdr + 4);
		if (l->frame >= total) return 0;
		int32_t count = rd32(hdr);
		if (Drawn *d = g_drawn.put(l)) { d->frame = l->frame; d->paused = paused != 0; }
		uint8_t *st = l->state;
		for (int32_t i = 0; i < count; i++)
		{
			Record r;
			st = EvalObject((uint16_t)i, hdr + rd32(hdr + 8 + 4 * i), st, l->frame, paused != 0, r);
			cb(l, &r, arg);
		}
		if (!paused) l->frame++;
		return total - l->frame;
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

	// ---- live self-check: every call of the original player in the game (all effects) runs the
	// native twin on a copy first and compares records, return value and final state ----
	static uint32_t g_verify_ri = 0;
	static uint32_t g_checked = 0, g_mismatch = 0;
	struct VerifyCtx { Callback cb; Record *rec; int count; };
	static VerifyCtx *g_vctx = nullptr;
	static Record g_rec_native[256], g_rec_orig[256];
	static uint8_t g_state_native[0x8000];

	static void __cdecl RecordCb(Layout *l, Record *r, int arg)
	{
		VerifyCtx *c = g_vctx;
		if (c->count < 256) c->rec[c->count] = *r;
		c->count++;
		c->cb(l, r, arg);
	}

	static int __cdecl VerifyHook(Layout *l, Callback cb, int arg, int paused)
	{
		const uint8_t *hdr = Header(l);
		int32_t count = rd32(hdr), total = rd32(hdr + 4);
		uint32_t size = StateSize(hdr);
		bool check = count >= 0 && count <= 256 && size <= sizeof(g_state_native);
		int32_t frame0 = l->frame;
		int nret = 0, ncount = 0;
		int32_t nframe = frame0;
		if (check)
		{
			memcpy(g_state_native, l->state, size);
			if (frame0 < total)
			{
				uint8_t *st = g_state_native;
				for (int32_t i = 0; i < count; i++)
					st = EvalObject((uint16_t)i, hdr + rd32(hdr + 8 + 4 * i), st, frame0, paused != 0, g_rec_native[ncount++]);
				if (!paused) nframe++;
				nret = total - nframe;
			}
		}
		VerifyCtx ctx{ cb, g_rec_orig, 0 };
		VerifyCtx *outer = g_vctx;
		g_vctx = &ctx;
		unreplace_function(g_verify_ri);
		int r = ((int(__cdecl *)(Layout *, Callback, int, int))0x701970)(l, RecordCb, arg, paused);
		rereplace_function(g_verify_ri);
		g_vctx = outer;
		if (!check) return r;
		g_checked++;
		const char *what = nullptr;
		if (r != nret) what = "return";
		else if (l->frame != nframe) what = "frame";
		else if (ctx.count != ncount) what = "record count";
		else if (memcmp(g_rec_orig, g_rec_native, sizeof(Record) * ncount)) what = "records";
		else if (memcmp(l->state, g_state_native, size)) what = "state";
		if (what && g_mismatch++ < 20)
			ffnx_error("30fps primplayer: MISMATCH (%s) data=%p frame=%d paused=%d objects=%d ret=%d/%d\n",
				what, l->data, frame0, paused, count, r, nret);
		if ((g_checked & 0x3FFF) == 0)
			ffnx_info("30fps primplayer: %u plays checked, %u mismatches\n", g_checked, g_mismatch);
		return r;
	}

	void install_verify()
	{
		g_verify_ri = replace_function(0x701970, (void *)VerifyHook);
		ffnx_info("30fps primplayer: live self-check of the native prim-model player installed\n");
	}
}
