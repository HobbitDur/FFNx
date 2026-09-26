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

// 30 fps part of mag103_blizzara.cpp: the draw log of the real tick and the held-frame drawing
// (included at the end of mag103_blizzara.cpp; the shared machinery is in mag_common_held.h).
//
// Blizzara never tests the draw-only flag: every tick moves, so every held frame interpolates.
// Held frame = every draw of the last real tick replayed in order (see mag_common_held.h):
//   IceBlock growing (20..30) - y scale half way to the next tick's; UV scroll of the second
//                triangles half a step further.
//   IceBlock shards (30..70) - the block's draw comes after its shard update, so the drawn state
//                is still in the node; the next tick's shard update runs on copies (shards,
//                vertices, both models), every shard active on both ticks is posed from its
//                half-way centre and angles (same vertex transform as the update), every colour
//                (triangle colours, glow, second-UV colour) is half way; UV scroll half a step
//                further. Shards that break loose on the next tick (random spin) hold.
//   Pillar     - spin and size half way; fade (+0x0C) half way when the next tick draws too.
//   Burst      - size half way; fade half way when the next tick draws too.
//   Chunk      - position half way (fall, then slide); flipbook frame holds.
//   Mist       - position half way; flipbook frame holds.
// The last tick of a task (it ends after its draw) holds.

namespace ff8fx
{
namespace blizzara103
{
	static uint32_t g_ported_tick = 0xFFFFFFFF;
	static DrawLog<512> g_draws;
	static uint8_t g_held_packets[0x20000];

	// the block node of each block draw of the real tick (record index -> node)
	static const uint32_t HELD_BlockGrow = ORIG_IceBlockTask, HELD_BlockShards = ORIG_IceBlockTask + 1;
	struct BlockRec { int rec; const IceBlockNode *node; };
	static BlockRec g_blocks[8];
	static int g_nblocks = 0;

	// private copies of a block for its held draws
	static uint32_t g_model_mid[0x72F], g_model2_mid[0x465], g_model_next[0x72F], g_model2_next[0x465];
	static int16_t g_verts_mid[SHARDS * 3][4], g_verts_next[SHARDS * 3][4];
	static Shard g_shards_next[SHARDS];

	static void held_note_root()
	{
		g_ported_tick = g_real_tick;
		g_draws.begin();
		g_nblocks = 0;
	}

	// whole nodes of the logged draws (the draw log keeps 0x24 bytes, these nodes are larger)
	static uint8_t g_nodes[512][0x30];

	static void held_note_draw(uint32_t fn, const void *node)
	{
		int n = g_draws.n;
		g_draws.add(fn, node, PacketCursor());
		if (g_draws.n == n + 1) memcpy(g_nodes[n], node, sizeof(g_nodes[n]));
	}

	static void held_note_block(const IceBlockNode *b, bool shards)
	{
		int n = g_draws.n;
		g_draws.add(shards ? HELD_BlockShards : HELD_BlockGrow, b, PacketCursor());
		g_hdr_slot = nullptr; // the block header is written completely by its draw
		if (g_draws.n == n + 1 && g_nblocks < 8) g_blocks[g_nblocks++] = { n, b };
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick && g_draws.current(); }

	static const IceBlockNode *BlockOf(int rec)
	{
		for (int i = 0; i < g_nblocks; i++) if (g_blocks[i].rec == rec) return g_blocks[i].node;
		return nullptr;
	}

	static int16_t GrowScale(int16_t c)
	{
		int32_t t = c - 0x14;
		return t < 3 ? (int16_t)Div3(t << 12) : 0x1000;
	}

	static void ScrollHalf(uint8_t *h, int num, int den)
	{
		*(uint16_t *)(h + 0x1C) = (uint16_t)(*(uint16_t *)(h + 0x1C) + lerp_i(0, 2, num, den));
		*(uint16_t *)(h + 0x1E) = (uint16_t)(*(uint16_t *)(h + 0x1E) + lerp_i(0, 8, num, den));
	}

	static void BlockGrowHeld(const DrawRec &r, const IceBlockNode *b, int num, int den)
	{
		int16_t c = ((const IceBlockNode *)r.node)->counter;
		bool next = c + 1 <= 0x1E;
		int16_t sy = GrowScale(c);
		if (next) sy = (int16_t)lerp_i(sy, GrowScale((int16_t)(c + 1)), num, den);
		uint8_t *h = (uint8_t *)FieldAlloc(0x64);
		BlockMatrix(b, sy);
		BlockHeader(h, b, c, 0x6600C);
		if (next) ScrollHalf(h, num, den);
		PacketCursor() = RenderBlock(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x64);
	}

	static void BlockShardsHeld(const DrawRec &r, const IceBlockNode *b, int num, int den)
	{
		int16_t c = ((const IceBlockNode *)r.node)->counter;
		bool next = c < 0x46;
		CopyDwords(g_model_mid, b->model, 0x72F);
		CopyDwords(g_model2_mid, b->model2, 0x465);
		memcpy(g_verts_mid, b->verts, sizeof(g_verts_mid));
		if (next)
		{
			CopyDwords(g_model_next, b->model, 0x72F);
			CopyDwords(g_model2_next, b->model2, 0x465);
			memcpy(g_verts_next, b->verts, sizeof(g_verts_next));
			memcpy(g_shards_next, b->shards, sizeof(g_shards_next));
			uint8_t *prims_next = ModelList(6, (uint8_t *)g_model_next, nullptr);
			uint8_t *uvs_next = ModelList(2, (uint8_t *)g_model2_next, nullptr);
			UpdateShards(g_shards_next, g_verts_next, prims_next, uvs_next);
			const uint8_t *prim = ModelList(6, b->model, nullptr);
			const uint8_t *uv = ModelList(2, b->model2, nullptr);
			uint8_t *pm = ModelList(6, (uint8_t *)g_model_mid, nullptr);
			uint8_t *um = ModelList(2, (uint8_t *)g_model2_mid, nullptr);
			static const uint8_t colour[10] = { 0, 1, 2, 0x14, 0x15, 0x16, 0x18, 0x19, 0x1A, 0x17 };
			for (int i = 0; i < SHARDS; i++, prim += 0x1C, pm += 0x1C, prims_next += 0x1C, uv += 0x14, um += 0x14, uvs_next += 0x14)
			{
				for (int k = 0; k < 10; k++) pm[colour[k]] = (uint8_t)lerp_i(prim[colour[k]], prims_next[colour[k]], num, den);
				for (int k = 0; k < 3; k++) um[k] = (uint8_t)lerp_i(uv[k], uvs_next[k], num, den);
				const Shard &s = b->shards[i], &x = g_shards_next[i];
				if (!s.active || !x.active) continue;
				Shard m = s;
				for (int k = 0; k < 3; k++)
				{
					m.pos[k] = lerp16(s.pos[k], x.pos[k], num, den);
					m.angle[k] = lerp16(s.angle[k], x.angle[k], num, den);
				}
				PoseShard(&m, g_verts_mid, pm);
			}
			BeginReplay(r); // the draw starts from the GTE state of the real draw
			g_hdr_slot = nullptr;
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0x64);
		BlockMatrix(b, 0x1000);
		BlockHeader(h, b, c, 0x660C0);
		Cursor(h, 0) = (uint8_t *)g_model_mid;
		Cursor(h, 4) = (uint8_t *)g_model2_mid;
		*(const void **)(h + 8) = g_verts_mid;
		if (next) ScrollHalf(h, num, den);
		PacketCursor() = RenderBlock(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x64);
	}

	static void PillarHeld(const DrawRec &r, int num, int den)
	{
		PillarNode a, b;
		memcpy(&a, g_nodes[&r - g_draws.r], sizeof(a));
		b = a;
		b.yaw = (int16_t)(b.yaw + b.yaw_vel);
		b.counter++;
		bool next = b.counter < 0x1E;
		if (next) PillarUpdate(&b);
		uint8_t *h = AllocHeader(0x58);
		*(int32_t *)(h + 0xC) = 0;
		int32_t fa = PillarFade(a.counter);
		*(int32_t *)(h + 0xC) = fa;
		if (fa < 0x1000)
		{
			PillarNode m = a;
			if (next)
			{
				m.yaw = lerp16(a.yaw, b.yaw, num, den);
				m.scale = lerp16(a.scale, b.scale, num, den);
				int32_t fb = PillarFade(b.counter);
				if (fb < 0x1000) *(int32_t *)(h + 0xC) = lerp_i(fa, fb, num, den);
			}
			PillarDraw(&m, h);
		}
		FieldFree(0x58);
	}

	static void BurstHeld(const DrawRec &r, int num, int den)
	{
		BurstNode a, b;
		memcpy(&a, g_nodes[&r - g_draws.r], sizeof(a));
		b = a;
		b.counter++;
		bool next = b.counter < 0x14;
		if (next) BurstUpdate(&b);
		uint8_t *h = AllocHeader(0x58);
		*(int32_t *)(h + 0xC) = 0;
		int32_t fa = BurstFade(a.counter);
		*(int32_t *)(h + 0xC) = fa;
		if (fa < 0x1000)
		{
			BurstNode m = a;
			if (next)
			{
				m.scale = lerp16(a.scale, b.scale, num, den);
				int32_t fb = BurstFade(b.counter);
				if (fb < 0x1000) *(int32_t *)(h + 0xC) = lerp_i(fa, fb, num, den);
			}
			BurstDraw(&m, h);
		}
		FieldFree(0x58);
	}

	static void HeldFrame(int num, int den)
	{
		HeldScope scope(PacketCursor(), g_held_packets, sizeof(g_held_packets), g_draws.packets);
		for (int i = 0; i < g_draws.n; i++)
		{
			const DrawRec &r = g_draws.r[i];
			BeginReplay(r);
			switch (r.fn)
			{
			case HELD_BlockGrow:
			case HELD_BlockShards:
			{
				g_hdr_slot = nullptr;
				const IceBlockNode *b = BlockOf(i);
				if (!b) break;
				if (r.fn == HELD_BlockGrow) BlockGrowHeld(r, b, num, den);
				else BlockShardsHeld(r, b, num, den);
				break;
			}
			case ORIG_PillarTask: PillarHeld(r, num, den); break;
			case ORIG_BurstTask: BurstHeld(r, num, den); break;
			case ORIG_ChunkTask:
			{
				ChunkNode a, b;
				memcpy(&a, g_nodes[&r - g_draws.r], sizeof(a));
				b = a;
				b.counter++;
				if (b.counter < 0x10) ChunkUpdate(&b);
				else b = a;
				ChunkNode m = a;
				for (int k = 0; k < 3; k++) m.pos[k] = lerp16(a.pos[k], b.pos[k], num, den);
				ChunkDraw(&m);
				break;
			}
			case ORIG_MistTask:
			{
				MistNode a, b;
				memcpy(&a, g_nodes[&r - g_draws.r], sizeof(a));
				b = a;
				b.counter++;
				if (b.counter < 0x10) MistUpdate(&b);
				else b = a;
				MistNode m = a;
				m.pos[0] = lerp16(a.pos[0], b.pos[0], num, den);
				m.pos[2] = lerp16(a.pos[2], b.pos[2], num, den);
				MistDraw(&m);
				break;
			}
			default:
				g_hdr_slot = nullptr;
				break;
			}
		}
	}
}

	static void register_mag103_held()
	{
		mark_held(blizzara103::ORIG_IceBlockTask);
		mark_held(blizzara103::ORIG_PillarTask);
		mark_held(blizzara103::ORIG_BurstTask);
		mark_held(blizzara103::ORIG_ChunkTask);
		mark_held(blizzara103::ORIG_MistTask);
		register_module_held(103, blizzara103::HeldReady, blizzara103::HeldFrame);
	}
}
