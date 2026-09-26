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

// 30 fps part of mag104_blizzaga.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag104_blizzaga.cpp; the shared machinery is in mag_common_held.h).
//
// Blizzaga never tests the draw-only flag: every tick moves, so every held frame interpolates.
// Held frame = every draw of the last real tick replayed in order (see mag_common_held.h):
//   Frozen target  - the target model redrawn with the freeze band half way to the next
//                    tick's band (its pose is the engine's); the band range it measures goes to
//                    a scratch copy; the shadow draw (0x5088A0) is not repeated; the frame packet
//                    cursor and the engine's projected-vertex buffer (0x1D98B3C) are private.
//   Block growing  - vertices from the morph step half way to the next tick's (28..29);
//                    the fixed shape (30..32) holds.
//   Block shards   - as Blizzara: the next tick's shard update on copies, shards live on both
//                    ticks posed half way, colours half way; shards breaking loose hold.
//   Ring           - size and depth cue half way, UV scroll half a step further.
//   Flash model    - x / z size and fade half way.
//   Ribbon, Spiral - their trails (a history of positions shifted each tick) hold: the draw is
//                    replayed on the node as it was before the real draw.
//   Sparkle, Mist, Flake - position half way; flipbook frame holds.
// The last tick of a task (it ends after its draw) holds.

namespace ff8fx
{
namespace blizzaga104
{
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static DrawLog<1024> g_draws;
	static uint8_t g_held_packets[0x20000];
	static uint8_t g_held_frame_packets[0x40000];
	static uint8_t g_held_work[0x8000];
	static bool g_held_drawing = false;

	static bool held_drawing() { return g_held_drawing; }

	static const uint32_t HELD_Frozen = ORIG_FreezeTask + 3, HELD_Block = ORIG_FreezeTask + 4;
	static const uint32_t HELD_Ring = ORIG_FreezeTask + 1, HELD_Flash = ORIG_FreezeTask + 2;

	struct FreezeRec { int rec; const FreezeNode *f; int16_t level, hi; int kind; Mat4x3 m; };
	static FreezeRec g_freeze[16];
	static int g_nfreeze = 0;
	struct TrailRec { int rec; uint8_t node[sizeof(RibbonNode)]; };
	static TrailRec g_trails[16];
	static int g_ntrails = 0;

	// private copies of a block for its held draws
	static uint32_t g_model_mid[0x8CA], g_model_next[0x8CA];
	static int16_t g_verts_mid[SHARDS * 3][4], g_verts_next[SHARDS * 3][4];
	static Shard g_shards_next[SHARDS];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_draws.begin();
		g_nfreeze = 0;
		g_ntrails = 0;
	}

	// whole nodes of the logged draws (the draw log keeps 0x24 bytes, these nodes are larger)
	static uint8_t g_nodes[1024][0x30];

	static void held_note_draw(uint32_t fn, const void *node)
	{
		int n = g_draws.n;
		g_draws.add(fn, node, PacketCursor());
		if (g_draws.n == n + 1) memcpy(g_nodes[n], node, sizeof(g_nodes[n]));
	}

	static FreezeRec *NoteFreeze(uint32_t fn, const FreezeNode *f)
	{
		int n = g_draws.n;
		g_draws.add(fn, f, PacketCursor());
		g_hdr_slot = nullptr; // these headers are written completely by their draws
		if (g_draws.n != n + 1 || g_nfreeze >= 16) return nullptr;
		FreezeRec &r = g_freeze[g_nfreeze++];
		r.rec = n;
		r.f = f;
		return &r;
	}

	static void held_note_frozen(const FreezeNode *f, int16_t level)
	{
		FreezeRec *r = NoteFreeze(HELD_Frozen, f);
		if (!r) return;
		r->level = level;
		r->hi = f->band_hi;
	}

	static void held_note_block(const FreezeNode *f, int kind, const Mat4x3 *m)
	{
		FreezeRec *r = NoteFreeze(HELD_Block, f);
		if (!r) return;
		r->kind = kind;
		r->m = *m;
	}

	// a part drawn with a scratch header (ring, flash model): the header scratch is recorded
	static void held_note_part(uint32_t fn, const FreezeNode *f)
	{
		int n = g_draws.n;
		g_draws.add(fn, f, PacketCursor());
		if (g_draws.n != n + 1 || g_nfreeze >= 16) return;
		FreezeRec &r = g_freeze[g_nfreeze++];
		r.rec = n;
		r.f = f;
	}

	static void held_note_trail(uint32_t fn, const void *node)
	{
		int n = g_draws.n;
		g_draws.add(fn, node, PacketCursor());
		g_hdr_slot = nullptr;
		if (g_draws.n != n + 1 || g_ntrails >= 16) return;
		TrailRec &t = g_trails[g_ntrails++];
		t.rec = n;
		memcpy(t.node, node, fn == ORIG_RibbonTask ? sizeof(RibbonNode) : sizeof(SpiralNode));
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_draws.current(); }

	static const FreezeRec *FreezeOf(int rec)
	{
		for (int i = 0; i < g_nfreeze; i++) if (g_freeze[i].rec == rec) return &g_freeze[i];
		return nullptr;
	}

	static void FrozenHeld(const FreezeRec &r, int16_t c, int num, int den)
	{
		const FreezeNode *f = r.f;
		int16_t level = r.level, hi = r.hi;
		int32_t t8 = c + 1 - 8;
		if ((uint32_t)t8 < 0x3D)
		{
			level = (int16_t)lerp_i(level, FreezeLevel(f, t8), num, den);
			hi = (int16_t)lerp_i(hi, f->band_hi, num, den);
		}
		int16_t uv[4] = { 0, 0, 0x40, 0x100 };
		int16_t lo_out, hi_out;
		uint32_t work = var<uint32_t>(0x1D98B3C);
		uint32_t frame = FrameCursor();
		var<uint32_t>(0x1D98B3C) = (uint32_t)g_held_work;
		FrameCursor() = (uint32_t)g_held_frame_packets;
		g_held_drawing = true;
		DrawFrozenEntity(f->entity, level, hi, &lo_out, &hi_out, uv);
		g_held_drawing = false;
		FrameCursor() = frame;
		var<uint32_t>(0x1D98B3C) = work;
	}

	static void BlockHeld(const DrawRec &dr, const FreezeRec &r, int16_t c, int num, int den)
	{
		const FreezeNode *f = r.f;
		uint8_t *h = (uint8_t *)FieldAlloc(0x64);
		BlockHeader(h, f, c);
		if (r.kind == 1)
		{
			// growth: the morph half way to the next tick's (the last morph step holds)
			uint32_t t = c - 0x1C, v = MorphStep(t);
			if (t + 1 < 3) v = (uint32_t)lerp_i(v, MorphStep(t + 1), num, den);
			*(void **)(h + 8) = g_verts_mid;
			MorphVertices(h, (const uint8_t *)MORPH_TABLE[v >> 12], (const uint8_t *)MORPH_TABLE[(v >> 12) + 1], v & 0xFFF);
		}
		else if (r.kind == 2 && c < 0x61)
		{
			CopyDwords(g_model_mid, f->model, 0x8CA);
			CopyDwords(g_model_next, f->model, 0x8CA);
			memcpy(g_verts_mid, f->verts, sizeof(g_verts_mid));
			memcpy(g_verts_next, f->verts, sizeof(g_verts_next));
			memcpy(g_shards_next, f->shards, sizeof(g_shards_next));
			uint8_t *pn = ModelList(6, (uint8_t *)g_model_next, nullptr);
			UpdateShards(g_shards_next, g_verts_next, pn);
			const uint8_t *prim = ModelList(6, f->model, nullptr);
			uint8_t *pm = ModelList(6, (uint8_t *)g_model_mid, nullptr);
			static const uint8_t colour[9] = { 0, 1, 2, 0x14, 0x15, 0x16, 0x18, 0x19, 0x1A };
			for (int i = 0; i < SHARDS; i++, prim += 0x1C, pm += 0x1C, pn += 0x1C)
			{
				for (int k = 0; k < 9; k++) pm[colour[k]] = (uint8_t)lerp_i(prim[colour[k]], pn[colour[k]], num, den);
				const Shard &s = f->shards[i], &x = g_shards_next[i];
				if (!s.active || !s.live || !x.live) continue;
				Shard m = s;
				for (int k = 0; k < 3; k++)
				{
					m.pos[k] = lerp16(s.pos[k], x.pos[k], num, den);
					m.angle[k] = lerp16(s.angle[k], x.angle[k], num, den);
				}
				PoseShard(&m, g_verts_mid, pm);
			}
			BeginReplay(dr); // the draw starts from the GTE state of the real draw
			g_hdr_slot = nullptr;
			Cursor(h, 0) = (uint8_t *)g_model_mid;
			*(void **)(h + 8) = g_verts_mid;
		}
		GteSetRotMatrix(&r.m);
		GteSetTransVector(&r.m);
		PacketCursor() = RenderBlock(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x64);
	}

	static void RingHeld(const FreezeRec &fr, int16_t c, int num, int den)
	{
		uint8_t *h = AllocHeader(0x5C);
		RingHeader(h, c);
		int16_t s = RingScale(c);
		if ((uint32_t)(c + 1 - 0x2E) < 0x19)
		{
			uint8_t next[0x5C];
			memcpy(next, h, sizeof(next));
			RingHeader(next, (int16_t)(c + 1));
			int32_t fa = *(int32_t *)(h + 8), fb = *(int32_t *)(next + 8);
			if ((fa != 0) == (fb != 0)) *(int32_t *)(h + 8) = lerp_i(fa, fb, num, den);
			*(uint16_t *)(h + 0xC) = (uint16_t)(*(uint16_t *)(h + 0xC) + lerp_i(0, 8, num, den));
			s = (int16_t)lerp_i(s, RingScale((int16_t)(c + 1)), num, den);
		}
		RingDraw(fr.f, h, s);
		FieldFree(0x5C);
	}

	static void FlashHeld(const FreezeRec &fr, int16_t c, int num, int den)
	{
		uint8_t *h = AllocHeader(0x58);
		int16_t s = FlashScale(c);
		int32_t fade = FlashFade(c);
		if ((uint32_t)(c + 1 - 0x1F) < 0xA)
		{
			s = (int16_t)lerp_i(s, FlashScale((int16_t)(c + 1)), num, den);
			fade = lerp_i(fade, FlashFade((int16_t)(c + 1)), num, den);
		}
		FlashDraw(fr.f, h, s, fade);
		FieldFree(0x58);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_draws.packets);
		uint8_t sparkle_t[12];
		memcpy(sparkle_t, (const void *)(SPARKLE_MATRIX + 0x14), sizeof(sparkle_t));
		for (int i = 0; i < g_draws.n; i++)
		{
			const DrawRec &r = g_draws.r[i];
			BeginReplay(r);
			int16_t c = ((const FreezeNode *)r.node)->counter;
			switch (r.fn)
			{
			case HELD_Frozen:
			{
				g_hdr_slot = nullptr;
				const FreezeRec *fr = FreezeOf(i);
				if (fr) FrozenHeld(*fr, c, num, den);
				break;
			}
			case HELD_Block:
			{
				g_hdr_slot = nullptr;
				const FreezeRec *fr = FreezeOf(i);
				if (fr) BlockHeld(r, *fr, c, num, den);
				break;
			}
			case HELD_Ring:
			case HELD_Flash:
			{
				const FreezeRec *fr = FreezeOf(i);
				if (!fr) { g_hdr_slot = nullptr; break; }
				if (r.fn == HELD_Ring) RingHeld(*fr, c, num, den);
				else FlashHeld(*fr, c, num, den);
				break;
			}
			case ORIG_RibbonTask:
			case ORIG_SpiralTask:
			{
				g_hdr_slot = nullptr;
				for (int k = 0; k < g_ntrails; k++)
				{
					if (g_trails[k].rec != i) continue;
					static uint8_t copy[sizeof(RibbonNode)];
					memcpy(copy, g_trails[k].node, sizeof(copy));
					if (r.fn == ORIG_RibbonTask) RibbonDraw((RibbonNode *)copy);
					else SpiralDraw((SpiralNode *)copy);
				}
				break;
			}
			case ORIG_SparkleTask:
			{
				SparkleNode a, b;
				memcpy(&a, g_nodes[&r - g_draws.r], sizeof(a));
				b = a;
				SparkleUpdate(&b);
				if (b.counter >= 0xA) b = a;
				SparkleNode m = a;
				for (int k = 0; k < 3; k++) m.pos[k] = lerp16(a.pos[k], b.pos[k], num, den);
				SparkleDraw(&m);
				break;
			}
			case ORIG_MistTask:
			{
				MistNode a, b;
				memcpy(&a, g_nodes[&r - g_draws.r], sizeof(a));
				b = a;
				b.counter++;
				if (b.counter < 0xA) MistUpdate(&b);
				else b = a;
				MistNode m = a;
				for (int k = 0; k < 3; k++) m.pos[k] = lerp16(a.pos[k], b.pos[k], num, den);
				MistDraw(&m);
				break;
			}
			case ORIG_FlakeTask:
			{
				FlakeNode a, b;
				memcpy(&a, g_nodes[&r - g_draws.r], sizeof(a));
				b = a;
				b.counter++;
				if (b.counter < 0xE) FlakeUpdate(&b);
				else b = a;
				FlakeNode m = a;
				for (int k = 0; k < 3; k++) m.pos[k] = lerp16(a.pos[k], b.pos[k], num, den);
				FlakeDraw(&m);
				break;
			}
			default:
				g_hdr_slot = nullptr;
				break;
			}
		}
		memcpy((void *)(SPARKLE_MATRIX + 0x14), sparkle_t, sizeof(sparkle_t));
	}
}

	static void register_mag104_held()
	{
		mark_held(blizzaga104::ORIG_FreezeTask);
		mark_held(blizzaga104::ORIG_RibbonTask);
		mark_held(blizzaga104::ORIG_SpiralTask);
		mark_held(blizzaga104::ORIG_SparkleTask);
		mark_held(blizzaga104::ORIG_MistTask);
		mark_held(blizzaga104::ORIG_FlakeTask);
		register_module_held(104, blizzaga104::HeldReady, blizzaga104::HeldFrame);
	}
}
