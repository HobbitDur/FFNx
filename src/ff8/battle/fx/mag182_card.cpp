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

// Effect 182: Card (command ability Card: kernel.bin command ability data entry 10 +0 = 182;
// MAG_182_*). No other kernel.bin entry uses effect 182. Sparkles rise around the target, two
// rings turn on the floor and twelve shards fly up; when the target is carded it fades out and
// a card spins up out of it while twelve small cards drift away leaving trails.
//
// Structure (setup MAG_182_CARD 0x5C9FE0, file loader 0x5C9FC0 = the texture file named at
// 0xD3D3E4 (mag181.tim, kept in 0x2306510); the setup keeps the texture base 0x23065CC
// (Magic_TextureOFF), the cast context 0x2306508, the first target's slot 0x2304220 and the
// caster's slot 0x230650C, sets up the root queue 0x2304238 (1 node) and the effect queue
// 0x23056E8 (0x64 nodes of 0x24 bytes), clears both particle pools and queues the TIM. Target
// record 0 +3 bit 2 set: DirectorA 0x5CA0E0 and camera animation 0xD39FD8 (no card: the short
// version), else DirectorB 0x5CA970 and camera animation 0xD39370):
//   RootTask (0x5CB4E0) - packet arena = the texture base (+ 0xC000 on odd counters; cursor
//     0x23065D0), runs the effect queue, ends when it is empty.
//   Directors (nothing in the draw-only mode) - counter 0: the target's entity flag bit 3 (8) is
//     cleared (kept in node +0x0E), its default effect position into node +0x10, its entity
//     +0x3C into +0x16, its size (entity +0x26) clamped to 0x600..0x1100 into +0x1C (+0x1E =
//     size * 2200 >> 12 before the lower clamp); 1: Sparkles, two Rings, twelve Shards, sound
//     (0xD3D38C A / 0xD3D388 B at the target's default effect position); screen flash
//     (Effect_SetScreenFlash) counter << 8 up to 8, (end - counter) << 8 over the last 8 ticks.
//     DirectorA ends after 0x28 (flag bit 3 put back + the action result at 0x23); DirectorB:
//     0x14 TargetFade, 0x18 twelve Cards, 0x1A CardModel, 0x40 flag bit 3 put back + the action
//     result, ends after 0x41.
//   Sparkles (0x5CA490) - pool 0x2304D88 (0x64 x 0x18): sprite sequence 0xD3ACCC, colour table
//     0xD3D3C4 by frame & 7; they rise (y += vy, vy -= vy >> 4); 3 spawned per tick 0..0xE
//     around the target; ends at counter >= 4 once none is left.
//   Rings (0x5CA6B0) - model 0xD3B3D8 on the floor (y 0) at the target, turning (+0x18 by +0x1A)
//     and growing (+0x1C by +0x1E, +0x1E += +0x1E / 32), fade (0x18 - c) << 8 below 8, 0x800,
//     (c - 0x12) << 8 from 0x1A; 0x22 ticks. The second ring's +0x1E is never written (the
//     effect pool node's leftover).
//   Shards (0x5CA810) - after a delay (+0x16) twelve models (table 0xD3D394 by +0x0E) turning
//     (+0x18 by +0x1A), their y scale +0x20 growing by +0x22 below counter 0xA, scale +0x1C
//     shrinking by +0x1E from 0xE; 0x18 ticks.
//   Cards (0x5CAED0, twelve, mask 1 << k in +0x0E) - after a delay (+0x18) a card sprite
//     (sequence 0xD3ABC0, frame = counter below 8) drifting up (y -= vy, vy -= vy / 10) and a
//     trail: pool 0x2304248 (0x78 x 0x18) entries of its mask (3 per tick while counter <=
//     +0x1A + 5), static, playing the sequence from frame 6; ends at counter >= 4 once its trail
//     is gone.
//   CardModel (0x5CB130) - model 0xD3AE78 turning (+0x18 by +0x1A, +0x1A -= +0x1A / 16) and
//     rising from counter 0x10 (y -= +0x1E), drawn below counter 0x1E (fading out from 0x16,
//     (c - 0x16) << 9); its world matrices of the last 5 ticks (0x2306518, 5 x 0x24: flag +
//     matrix) drawn as after-images (fade 0xC00 + 0x20 * k cumulative), 5 of them, 0x23 - c from
//     counter 0x1E; 0x23 ticks.
//   TargetFade (0x5CB390) - the target (slot in +0x0E) fades to the battle's far colours over 6
//     ticks: entity +0x07 = (0x1000 - sin(c * 1024 / 6)) * 255 >> 12, +0x2C / +0x28 = the depth
//     cue of 0x32000000 / 0 toward the far colours 0xB8B9A8 / 0xB8B7D8 by IR0 0x1000 - sin,
//     +0x2B = 2; entity flag bit 11 (0x800) set at its first tick; at the end flag bit 11 put back
//     (saved in +0x1A) and bit 2 (4) set: the battle renderer no longer draws the target.
// Every drawing task draws THEN updates; nothing moves in the draw-only mode. The module writes
// the target's entity flags (bit 3 cleared during the effect; bits 11 / 2 and the colours
// +0x07 / +0x28..+0x2F by TargetFade) and no camera word (the camera is the engine's animation).
// Module globals: 0x2304220..0x23065D4 (target slot, root pool / queue, trail pool, sparkle
// pool, effect queue / pool, context 0x2306508, caster slot 0x230650C, texture file 0x2306510,
// after-image matrices 0x2306518, texture base 0x23065CC, packet cursor 0x23065D0).

#include "mag_common.h"

namespace ff8fx
{
namespace card182
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2306508); }
	inline uint32_t &TargetSlot() { return var<uint32_t>(0x2304220); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x23065CC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x23065D0); }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x23056E8; }
	static const uint32_t TRAIL_POOL = 0x2304248, TRAIL_END = 0x2304D88;    // 0x78 x 0x18 (card trails)
	static const uint32_t SPARK_POOL = 0x2304D88, SPARK_END = 0x23056E8;    // 0x64 x 0x18 (sparkles)
	static const uint32_t AFTER = 0x2306518, AFTER_END = 0x23065CC;          // 5 x 0x24: CardModel world matrices
	static const uint32_t ARENA_HALF = 0xC000;

	static const uint32_t ORIG_RootTask = 0x5CB4E0;
	static const uint32_t ORIG_DirectorA = 0x5CA0E0;
	static const uint32_t ORIG_DirectorB = 0x5CA970;
	static const uint32_t ORIG_Sparkles = 0x5CA490;
	static const uint32_t ORIG_Ring = 0x5CA6B0;
	static const uint32_t ORIG_Shard = 0x5CA810;
	static const uint32_t ORIG_Card = 0x5CAED0;
	static const uint32_t ORIG_CardModel = 0x5CB130;
	static const uint32_t ORIG_TargetFade = 0x5CB390;
	static const uint32_t SEQ_Sparkle = 0xD3ACCC, SEQ_Card = 0xD3ABC0;
	static const uint32_t MODEL_Ring = 0xD3B3D8, MODEL_Card = 0xD3AE78;
	static const uint32_t SHARD_MODELS = 0xD3D394;                            // 12 model pointers
	static const uint32_t SPARK_COLOURS = 0xD3D3C4;                           // 8 colour words
	static const void *const SOUND_CardA = (const void *)0xD3D38C;
	static const void *const SOUND_CardB = (const void *)0xD3D388;

	// ---- engine functions (original addresses)
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // GTE_SetFarColor
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }      // GTE_LoadRGBC
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }                 // GTE_StoreRGB2

	// ------------------------------------------------------------------
	// Node / particle layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad;
	};
	struct Node // effect pool, 0x64 nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t c;         // +0x0C counter
		int16_t e;         // +0x0E directors: the target's flag bit 3; shards: model index; cards: mask; fade: target slot
		int16_t x, y, z;   // +0x10 position
		int16_t w16;       // +0x16 directors / sparkles: top y (entity +0x3C); shards: delay
		int16_t w18;       // +0x18 angle; cards: delay; fade: duration
		int16_t w1A;       // +0x1A angle rate; cards: their delay; fade: the target's flag bit 11
		int16_t w1C;       // +0x1C scale / size; cards: sprite scale
		int16_t w1E;       // +0x1E scale rate / spread / vy
		int16_t w20;       // +0x20 shards: y scale
		int16_t w22;       // +0x22 shards: y scale rate
	};
	struct Particle // pool entry, 0x18 bytes
	{
		uint32_t tag;      // +0x00 sparkles: 1 = alive (bit 0); trails: the card's mask (0 = free)
		int16_t frame;     // +0x04 flipbook frame
		int16_t scale;     // +0x06
		int16_t pos[4];    // +0x08 x, y, z (+ trails: the card's +0x16 word)
		int16_t w10;       // +0x10
		int16_t vy;        // +0x12 sparkles: rising speed
		uint32_t w14;      // +0x14
	};
	struct AfterImage // 0x24 bytes
	{
		uint32_t used;
		Mat4x3 m;          // world matrix (rotation, scale, translation) of the tick
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Card nodes");
	static_assert(sizeof(Particle) == 0x18 && sizeof(AfterImage) == 0x24, "Card pools");

	inline Particle *TrailPool() { return (Particle *)TRAIL_POOL; }
	inline Particle *SparkPool() { return (Particle *)SPARK_POOL; }
	inline AfterImage *After() { return (AfterImage *)AFTER; }
	inline uint8_t *Target() { return Entity((int)TargetSlot()); }
	inline uint32_t &Cursor() { return PacketCursor(); }

	static uint32_t EndAt(int16_t c, int16_t n) { return c >= n ? TASK_END : 0; }

	// model matrix: rotation (0, angle, 0), translation, scale (sx, sy, sz), composed with the camera; GTE set.
	// `world` (if given) receives the matrix before the camera (CardModel's after-images)
	static void ModelMatrix(int16_t angle, int32_t tx, int32_t ty, int32_t tz, int32_t sx, int32_t sy, int32_t sz, Mat4x3 *world)
	{
		int16_t ang[4]; // (4th word never written)
		ang[0] = 0;
		ang[1] = angle;
		ang[2] = 0;
		Mat4x3 m; // (pad +0x12 never written in the original: GTE R33 high half only; CardModel copies it into its after-image)
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = tx;
		m.t[1] = ty;
		m.t[2] = tz;
		int32_t sv[3];
		sv[0] = sx;
		sv[1] = sy;
		sv[2] = sz;
		Scale3DMatrix(&m, sv);
		if (world) *world = m;
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	// ------------------------------------------------------------------
	// Sparkles (pool 0x2304D88): one entry drawn with the task's sprite header
	// ------------------------------------------------------------------
	static uint8_t *SparkHeader()
	{
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Sparkle;
		*(int16_t *)(h + 0x24) = 4;
		return h;
	}
	static void SparkDraw(uint8_t *h, const Particle *p)
	{
		*(int16_t *)(h + 4) = p->frame;
		*(uint32_t *)(h + 0x1C) = var<uint32_t>(SPARK_COLOURS + (uint32_t)((int32_t)p->frame % 8) * 4);
		TransformCameraByShadowRotation(p->pos, p->scale, -((int32_t)p->scale >> 4));
		Cursor() = InitEffectSequenceFromData(h, RenderOT(), 2, Cursor());
	}

	// ------------------------------------------------------------------
	// Rings (0x5CA6B0)
	// ------------------------------------------------------------------
	static int32_t RingFade(int32_t c) { return c < 8 ? shl32(0x18 - c, 8) : c >= 0x1A ? shl32(c - 0x12, 8) : 0x800; }
	static void RingDraw(const Node *n, int16_t angle, int16_t scale, int32_t fade)
	{
		ModelMatrix(angle, n->x, 0, n->z, scale, scale, scale, nullptr);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_Ring;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		*(int32_t *)(h + 0xC) = fade;
		Cursor() = RenderPrimModel(h, RenderOT(), 2, Cursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Shards (0x5CA810)
	// ------------------------------------------------------------------
	static void ShardDraw(const Node *n, int16_t angle, int16_t scale, int16_t yscale)
	{
		ModelMatrix(angle, n->x, 0, n->z, scale, yscale, scale, nullptr);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		*(uint32_t *)h = var<uint32_t>(SHARD_MODELS + (uint32_t)n->e * 4);
		Cursor() = RenderPrimModel(h, RenderOT(), 2, Cursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Cards (0x5CAED0): the card sprite and its trail entries (the task's sprite header)
	// ------------------------------------------------------------------
	static uint8_t *CardHeader()
	{
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Card;
		*(int16_t *)(h + 0x24) = 8;
		return h;
	}
	static void CardSpriteDraw(uint8_t *h, int16_t frame, const int16_t *pos, int16_t scale)
	{
		*(int16_t *)(h + 4) = frame;
		TransformCameraByShadowRotation(pos, scale, -((int32_t)scale >> 3));
		Cursor() = InitEffectSequenceFromData(h, RenderOT(), 2, Cursor());
	}
	static void TrailDraw(uint8_t *h, const Particle *p)
	{
		*(int16_t *)(h + 4) = p->frame;
		TransformCameraByShadowRotation(p->pos, p->scale, -((int32_t)p->scale >> 4));
		Cursor() = InitEffectSequenceFromData(h, RenderOT(), 2, Cursor());
	}

	// ------------------------------------------------------------------
	// CardModel (0x5CB130)
	// ------------------------------------------------------------------
	// the after-images drawn at counter c: their number and the first ring slot (the previous tick's)
	static int32_t AfterCount(int32_t c) { return c >= 0x1E ? 0x23 - c : 5; }
	static int32_t AfterFirst(int32_t c) { return c - 1 < 0 ? 0 : (c - 1) % 5; }
	// the main model (counter below 0x1E; from 0x16 fading out) at the given state, its world matrix into
	// after-image slot c % 5; then the after-images from the ring (each through the camera, the fade growing by
	// 0x20 * k). The header is the one 0x58-byte block of the draw.
	static void CardModelDraw(int16_t c, int16_t angle, int16_t x, int16_t y, int16_t z, int16_t scale, AfterImage *ring, int32_t main_fade)
	{
		AfterImage *slot = &ring[(int32_t)c % 5];
		slot->used = 1;
		ModelMatrix(angle, x, y, z, scale, scale, scale, &slot->m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_Card;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		if (c < 0x1E)
		{
			if (c >= 0x16)
			{
				*(uint32_t *)(h + 0x1C) = 0xC3;
				*(int32_t *)(h + 0xC) = main_fade;
			}
			Cursor() = RenderPrimModel(h, RenderOT(), 2, Cursor());
		}
		*(uint32_t *)(h + 0x1C) |= 0xC3;
		*(int32_t *)(h + 0xC) = 0xC00;
		int32_t k = AfterFirst(c);
		const int32_t count = AfterCount(c);
		int32_t step = 0;
		for (int32_t i = 0; i < count; i++, step += 0x20)
		{
			if (ring[k].used == 0) break;
			Mat4x3 m; // (pad +0x12: the slot's copy of the world matrix pad, GTE R33 high half only)
			ComposeAffineTransform(&Camera(), &ring[k].m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			*(int32_t *)(h + 0xC) += step;
			Cursor() = RenderPrimModel(h, RenderOT(), 2, Cursor());
			if (--k < 0) k = 4;
		}
		FieldFree(0x58);
	}
	static int32_t CardModelFade(int32_t c) { return shl32(c - 0x16, 9); }

	// ------------------------------------------------------------------
	// TargetFade (0x5CB390): the target's colours by IR0 = 0x1000 - sin(c * 1024 / duration)
	// ------------------------------------------------------------------
	static void TargetFadeColours(uint8_t *e, int32_t ir0)
	{
		uint32_t colour = 0x32000000;
		e[7] = (uint8_t)((uint32_t)(shl32(ir0, 8) - ir0) >> 12);
		const uint32_t far0 = var<uint32_t>(0xB8B9A8);
		GteSetFarColor(far0 & 0xFF, (far0 >> 8) & 0xFF, var<uint8_t>(0xB8B9AA));
		GteLoadRGBC(&colour);
		GteSetIR0(ir0);
		GteDPCS();
		GteStoreRGB2(e + 0x2C);
		const uint32_t far1 = var<uint32_t>(0xB8B7D8);
		GteSetFarColor(far1 & 0xFF, (far1 >> 8) & 0xFF, var<uint8_t>(0xB8B7DA));
		uint32_t black = 0;
		GteLoadRGBC(&black);
		GteDPCS();
		GteStoreRGB2(e + 0x28);
		e[0x2B] = 2;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag182_card_held.h"
#endif

namespace ff8fx
{
namespace card182
{
	// ------------------------------------------------------------------
	// Sparkles (0x5CA490)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparklesTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		// 30 fps layer: see mag182_card_held.inc
		FX_HELD(held_note_pool(ORIG_Sparkles, n);)
		uint8_t *h = SparkHeader();
		int alive = 0;
		for (Particle *p = SparkPool(); (uint32_t)p < SPARK_END; p++)
		{
			if (!(p->tag & 1)) continue;
			SparkDraw(h, p);
			if (DrawOnly()) continue;
			p->frame++;
			if (*(int16_t *)(h + 0x28) < 0) { p->tag = 0; continue; }
			p->pos[1] = (int16_t)(p->pos[1] + p->vy);
			p->vy = (int16_t)(p->vy - (int16_t)(p->vy >> 4));
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (n->c >= 0 && n->c <= 0xE)
		{
			for (int k = 0; k < 3; k++)
			{
				int i = 0;
				Particle *p = SparkPool();
				for (; p->tag != 0; p++, i++)
					if ((uint32_t)(p + 1) >= SPARK_END) { p = nullptr; break; }
				if (!p || i >= 0x64) break;
				p->tag = 1;
				p->frame = 0;
				const int16_t s = n->w1E;
				p->scale = (int16_t)(((CrtRand() % 0x1400 + 0x1700) * n->w1C) >> 12);
				const int32_t rx = CrtRand() % (2 * (int32_t)s);
				p->pos[0] = (int16_t)(rx + n->x - s);
				const int32_t ry = CrtRand() % ((int32_t)n->w1E >> 1);
				p->pos[1] = (int16_t)(n->w16 - ry);
				const int16_t s2 = n->w1E;
				const int32_t rz = CrtRand() % (2 * (int32_t)s2);
				p->pos[2] = (int16_t)(rz + n->z - s2);
				p->vy = (int16_t)(-0x3C - CrtRand() % 0x78);
			}
		}
		n->c++;
		if (n->c >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Rings (0x5CA6B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		// 30 fps layer: see mag182_card_held.inc
		FX_HELD(held_note_node(ORIG_Ring, n);)
		RingDraw(n, n->w18, n->w1C, RingFade(n->c));
		if (DrawOnly()) return 0;
		const int16_t ra = n->w1A, rs = n->w1E;
		n->w18 = (int16_t)(n->w18 + ra);
		n->w1C = (int16_t)(n->w1C + rs);
		n->w1E = (int16_t)(rs + rs / 32);
		n->c++;
		return EndAt(n->c, 0x22);
	}

	// ------------------------------------------------------------------
	// Shards (0x5CA810)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ShardTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		if (n->w16 > 0)
		{
			if (DrawOnly()) return 0;
			n->w16--;
			return 0;
		}
		// 30 fps layer: see mag182_card_held.inc
		FX_HELD(held_note_node(ORIG_Shard, n);)
		ShardDraw(n, n->w18, n->w1C, n->w20);
		if (DrawOnly()) return 0;
		n->w18 = (int16_t)(n->w18 + n->w1A);
		if (n->c < 0xA) n->w20 = (int16_t)(n->w20 + n->w22);
		else if (n->c >= 0xE) n->w1C = (int16_t)(n->w1C - n->w1E);
		n->c++;
		return EndAt(n->c, 0x18);
	}

	// ------------------------------------------------------------------
	// Cards (0x5CAED0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl CardTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		if (n->w18 > 0)
		{
			if (!DrawOnly()) n->w18--;
			return 0;
		}
		// 30 fps layer: see mag182_card_held.inc
		FX_HELD(held_note_pool(ORIG_Card, n);)
		uint8_t *h = CardHeader();
		int alive = 0;
		if (n->c < 8) CardSpriteDraw(h, n->c, &n->x, n->w1C);
		for (Particle *p = TrailPool(); (uint32_t)p < TRAIL_END; p++)
		{
			if (!(p->tag & (uint32_t)(int32_t)n->e)) continue;
			TrailDraw(h, p);
			if (DrawOnly()) continue;
			p->frame++;
			if (*(int16_t *)(h + 0x28) < 0) p->tag = 0;
			else alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (n->c >= 0 && (int32_t)n->c <= (int32_t)n->w1A + 5)
		{
			for (int k = 0; k < 3; k++)
			{
				int i = 0;
				Particle *p = TrailPool();
				for (; p->tag != 0; p++, i++)
					if ((uint32_t)(p + 1) >= TRAIL_END) { p = nullptr; break; }
				if (!p || i >= 0x78) break;
				p->tag = (uint32_t)(int32_t)n->e;
				p->frame = 6;
				p->scale = (int16_t)(CrtRand() % 0x280 + 0x400);
				*(uint32_t *)&p->pos[0] = *(const uint32_t *)&n->x;
				*(uint32_t *)&p->pos[2] = *(const uint32_t *)&n->z;
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(CrtRand() % 0x104 - 0x82));
				p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(CrtRand() % 0x82 - 0x41));
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(CrtRand() % 0x104 - 0x82));
			}
		}
		const int16_t vy = n->w1E;
		n->y = (int16_t)(n->y - vy);
		n->w1E = (int16_t)(vy - vy / 10);
		n->c++;
		if (n->c < 4 || alive) return 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// CardModel (0x5CB130)
	// ------------------------------------------------------------------
	static uint32_t __cdecl CardModelTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		// 30 fps layer: see mag182_card_held.inc
		FX_HELD(held_note_node(ORIG_CardModel, n);)
		CardModelDraw(n->c, n->w18, n->x, n->y, n->z, n->w1C, After(), CardModelFade(n->c));
		if (DrawOnly()) return 0;
		if (n->c >= 0x10) n->y = (int16_t)(n->y - n->w1E);
		const int16_t w = n->w1A;
		n->w18 = (int16_t)(n->w18 + w);
		n->w1A = (int16_t)(w - w / 16);
		n->c++;
		return EndAt(n->c, 0x23);
	}

	// ------------------------------------------------------------------
	// TargetFade (0x5CB390)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetFadeTask(TaskNode *tn)
	{
		Node *n = (Node *)tn;
		const int32_t ir0 = 0x1000 - ComputeSin(shl32(n->c, 10) / n->w18);
		uint8_t *e = Entity(n->e);
		TargetFadeColours(e, ir0);
		if (DrawOnly()) return 0;
		if (n->c == 0) e[1] |= 8;
		n->c++;
		if (n->c < n->w18) return 0;
		uint16_t &flags = *(uint16_t *)e;
		flags = (uint16_t)(((flags & 0xF7FF) | (uint16_t)n->w1A) | 4);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Directors (0x5CA0E0 / 0x5CA970)
	// ------------------------------------------------------------------
	// counter 0: the target's flag bit 3 kept and cleared, its position / top / size
	static void DirectorStart(Node *t)
	{
		uint16_t &flags = *(uint16_t *)Target();
		const uint16_t f = flags;
		t->e = (int16_t)(f & 8);
		flags = (uint16_t)(f & 0xFFF7);
		GetDefaultEffectPosition(Target(), &t->x);
		t->w16 = *(const int16_t *)(Target() + 0x3C);
		const int16_t size = EntitySize((int)TargetSlot());
		t->w1C = size;
		if (size > 0x1100) t->w1C = 0x1100;
		const int16_t s = t->w1C;
		t->w1E = (int16_t)(mul32(s, 2200) >> 12);
		if (s < 0x600) t->w1C = 0x600;
	}

	// counter 1: sparkles, two rings, twelve shards
	static void DirectorSpawn(Node *t)
	{
		Node *s = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Sparkles);
		*(uint32_t *)&s->x = *(const uint32_t *)&t->x;
		*(uint32_t *)&s->z = *(const uint32_t *)&t->z;
		s->w1C = t->w1C;
		s->c = 0;
		s->w1E = t->w1E;
		Node *r1 = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Ring);
		*(uint32_t *)&r1->x = *(const uint32_t *)&t->x;
		r1->c = 0;
		*(uint32_t *)&r1->z = *(const uint32_t *)&t->z;
		r1->w18 = (int16_t)(CrtRand() % 0x1000);
		const int32_t rr = CrtRand() % 0x28;
		const int32_t sc = (((int32_t)t->w1C * 5) << 9) >> 12;
		r1->w1A = (int16_t)(rr + 0x14);
		r1->w1E = (int16_t)(sc / 16);
		r1->w1C = (int16_t)(sc / 16);
		Node *r2 = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Ring);
		*(uint32_t *)&r2->x = *(const uint32_t *)&t->x;
		r2->c = 0;
		*(uint32_t *)&r2->z = *(const uint32_t *)&t->z;
		const int32_t a2 = CrtRand() % 0x1000;
		r2->w1A = (int16_t)-r1->w1A;
		r2->w18 = (int16_t)a2;
		r2->w1C = r1->w1C;
		for (int k = 0; k < 0xC; k++)
		{
			Node *d = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Shard);
			*(uint32_t *)&d->x = *(const uint32_t *)&t->x;
			d->c = 0;
			d->e = (int16_t)k;
			*(uint32_t *)&d->z = *(const uint32_t *)&t->z;
			d->w16 = (int16_t)(CrtRand() % 6 + k);
			d->w18 = (int16_t)(CrtRand() % 0x1000);
			int16_t rate = (int16_t)(CrtRand() % 0x3C + 0x19);
			d->w1A = rate;
			if (rate & 1) d->w1A = (int16_t)-rate;
			const int16_t sc2 = (int16_t)(((CrtRand() % 0x400 + 0x900) * (int32_t)t->w1C) >> 12);
			d->w1C = sc2;
			d->w1E = (int16_t)(sc2 / 10);
			const int16_t yr = (int16_t)((CrtRand() % 0x500 + 0xA00) / 10);
			d->w22 = yr;
			d->w20 = yr;
		}
	}

	// the screen flash: counter << 8 up to 8, (end - counter) << 8 from end - 8
	static void DirectorFlash(int16_t c, int16_t end)
	{
		if (c <= 8) SetScreenFlash(shl32(c, 8), 0);
		else if (c >= end - 8) SetScreenFlash(shl32(end - c, 8), 0);
	}

	static void DirectorSound(const void *sound)
	{
		int16_t pos[4];
		GetDefaultEffectPosition(Target(), pos);
		BdPlaySE3D(sound, 0, pos);
	}

	// the target's flag bit 3 put back, then the action result
	static void DirectorResult(Node *t)
	{
		*(uint16_t *)Target() |= (uint16_t)t->e;
		ApplyActionResultToTarget(Ctx()->actions[0].targets);
	}

	static uint32_t __cdecl DirectorATask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node *t = (Node *)tn;
		if (t->c == 0) DirectorStart(t);
		if (t->c == 1) DirectorSpawn(t);
		if (t->c == 0x23) DirectorResult(t);
		DirectorFlash(t->c, 0x28);
		if (t->c == 1) DirectorSound(SOUND_CardA);
		t->c++;
		if (t->c <= 0x28) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	static uint32_t __cdecl DirectorBTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node *t = (Node *)tn;
		if (t->c == 0) DirectorStart(t);
		if (t->c == 1) DirectorSpawn(t);
		if (t->c == 0x18)
		{
			for (int k = 0; k < 0xC; k++)
			{
				Node *d = (Node *)AddTaskToQueue(EffectQueue(), ORIG_Card);
				d->c = 0;
				d->e = (int16_t)(1 << k);
				d->x = (int16_t)(CrtRand() % 0x384 + t->x - 0x1C2);
				d->y = (int16_t)(t->w16 - CrtRand() % 0x1C2 - 0x64);
				d->z = (int16_t)(CrtRand() % 0x384 + t->z - 0x1C2);
				const int16_t delay = (int16_t)(CrtRand() % 8 + k);
				d->w1A = delay;
				d->w18 = delay;
				d->w1C = (int16_t)(CrtRand() % 0x700 + 0xA00);
				d->w1E = (int16_t)(CrtRand() % 0xA0 + 0x3C);
			}
		}
		if (t->c == 0x1A)
		{
			Node *m = (Node *)AddTaskToQueue(EffectQueue(), ORIG_CardModel);
			m->c = 0;
			m->x = t->x;
			m->y = (int16_t)(t->w16 - 0x1F4);
			m->z = t->z;
			m->w18 = (int16_t)(CrtRand() % 0x1000);
			m->w1C = 0x800;
			m->w1A = (int16_t)(CrtRand() % 0xC8 + 0x3E8);
			m->w1E = (int16_t)(CrtRand() % 0x23 + 0x41);
			for (AfterImage *a = After(); (uint32_t)a < AFTER_END; a++) a->used = 0;
		}
		if (t->c == 0x14)
		{
			Node *f = (Node *)AddTaskToQueue(EffectQueue(), ORIG_TargetFade);
			f->c = 0;
			f->e = (int16_t)TargetSlot();
			f->w18 = 6;
			f->w1A = (int16_t)(*(const uint16_t *)Target() & 0x800);
		}
		if (t->c == 0x40) DirectorResult(t);
		DirectorFlash(t->c, 0x41);
		if (t->c == 1) DirectorSound(SOUND_CardB);
		t->c++;
		if (t->c <= 0x41) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Root task (0x5CB4E0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *tn)
	{
		RootNode *r = (RootNode *)tn;
		if (r->counter & 1) PacketCursor() = TexBase() + ARENA_HALF;
		else PacketCursor() = TexBase();
		// 30 fps layer: see mag182_card_held.inc
		FX_HELD(held_note_root();)
		const int a = ExecuteTaskQueue(EffectQueue());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag182_card()
	{
		register_port(card182::ORIG_RootTask, (void *)card182::RootTask, "C182 RootTask", 182);
		register_port(card182::ORIG_DirectorA, (void *)card182::DirectorATask, "C182 DirectorA", 182);
		register_port(card182::ORIG_DirectorB, (void *)card182::DirectorBTask, "C182 DirectorB", 182);
		register_port(card182::ORIG_Sparkles, (void *)card182::SparklesTask, "C182 Sparkles", 182);
		register_port(card182::ORIG_Ring, (void *)card182::RingTask, "C182 Ring", 182);
		register_port(card182::ORIG_Shard, (void *)card182::ShardTask, "C182 Shard", 182);
		register_port(card182::ORIG_Card, (void *)card182::CardTask, "C182 Card", 182);
		register_port(card182::ORIG_CardModel, (void *)card182::CardModelTask, "C182 CardModel", 182);
		register_port(card182::ORIG_TargetFade, (void *)card182::TargetFadeTask, "C182 TargetFade", 182);
		// 30 fps layer: see mag182_card_held.inc
		FX_HELD(register_mag182_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag182_card_held.inc"
#endif
