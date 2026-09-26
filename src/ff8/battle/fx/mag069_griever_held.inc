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

// 30 fps part of mag069_griever.cpp: memos of what the real tick drew and the held-frame drawing
// (included at the end of mag069_griever.cpp)

namespace ff8fx
{
namespace g069
{
	// ------------------------------------------------------------------
	// held-frame memo of what the real tick drew
	// ------------------------------------------------------------------
	struct TimelineMemo
	{
		uint32_t tick;
		TimelineNode *node;
		int32_t c;          // counter the tick ran with
		bool advanced;      // the counter moved on (no stall on the load / pause test)
		bool prim; PrimCtx ctx;
		bool attacker;      // 0x6FECF0 drew the attacker
		bool models; int32_t thr;
		bool sparks;        // sparks drawn (c < 48); updated after the draw when c < 47
		bool fade; int32_t fade_t;
	};
	static TimelineMemo g_tm;
	static uint8_t g_shard_prev[SHARD_COUNT * 0x3C], g_shard_held[SHARD_COUNT * 0x3C];
	static uint8_t g_spark_prev[SPARK_COUNT * 0x10], g_spark_held[SPARK_COUNT * 0x10];

	// real tick on which the ported master last ran
	static uint32_t g_ported_tick = 0xFFFFFFFF;

	static void held_note_master() { g_ported_tick = g_real_tick; }

	static void held_note_timeline(TimelineNode *node, int32_t c)
	{
		TimelineMemo &M = g_tm;
		M.tick = g_real_tick;
		M.node = node;
		M.c = c;
		M.advanced = false;
		M.prim = M.attacker = M.models = M.sparks = M.fade = false;
	}

	static void held_note_prim(const PrimCtx &ctx)
	{
		g_tm.prim = true;
		g_tm.ctx = ctx;
	}

	static void held_note_attacker() { g_tm.attacker = true; }

	static void held_note_models(int32_t thr)
	{
		g_tm.models = true;
		g_tm.thr = thr;
	}

	static void held_note_sparks()
	{
		g_tm.sparks = true;
		memcpy(g_spark_prev, ModelBuffer() + MB_SPARKS, sizeof(g_spark_prev));
	}

	static void held_note_shards() { memcpy(g_shard_prev, ModelBuffer() + MB_SHARDS, sizeof(g_shard_prev)); }

	static void held_note_fade(int32_t t)
	{
		g_tm.fade = true;
		g_tm.fade_t = t;
	}

	static void held_note_advanced() { g_tm.advanced = true; }

	// ------------------------------------------------------------------
	// The shattering rasteriser draw-only (0x6FF6E0 / 0x6FF770 / 0x6FF880 without the shard
	// spawns: faces below the threshold are skipped, no allocation, no LCG draw) on the
	// in-between pose (the caller saves / restores the skeleton)
	// ------------------------------------------------------------------

	// 0x6FF880 draw-only
	static uint32_t FacesHeld(uint8_t *sc, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *f = *(uint8_t **)sc;
		const uint8_t *verts = *(uint8_t **)(sc + 4);
		uint8_t *pkt = (uint8_t *)cursor;
		uint32_t thr = U16(sc, 0x14);

		// triangles, 0x14 bytes: u16 v0, v1, v2, uv2; u32 uv0, uv1, colour
		for (;;)
		{
			int16_t c = S16(sc, 8);
			S16(sc, 8) = (int16_t)(c - 1);
			if (!c) break;
			int32_t *dp = *(int32_t **)(sc + 0x18);
			int32_t d = *dp;
			*(int32_t **)(sc + 0x18) = dp + 1;
			if (d != -1)
			{
				CopyVertex(sc, 0x20, verts, U16(f, 0));
				CopyVertex(sc, 0x30, verts, U16(f, 2));
				CopyVertex(sc, 0x40, verts, U16(f, 4));
				if (d >= (int32_t)thr) // below: a shard on the real tick, nothing here
				{
					uint8_t o0 = sc[0x26], o1 = sc[0x36], o2 = sc[0x46];
					if ((uint8_t)(o0 | o1 | o2) < 0x10 && !(o2 & (uint8_t)(o0 & o1)))
					{
						w::SetSXY012(U32(sc, 0x20), U32(sc, 0x30), U32(sc, 0x40));
						w::NCLIP();
						w::ReadMAC0(sc + 0x60);
						if (S32(sc, 0x60) >= 0)
						{
							w::SetSZ123(U16(sc, 0x24), U16(sc, 0x34), U16(sc, 0x44));
							w::AVSZ3();
							U32(pkt, 0) = 0x07000000;
							U32(pkt, 8) = U32(sc, 0x20);
							U32(pkt, 0x10) = U32(sc, 0x30);
							U32(pkt, 0x18) = U32(sc, 0x40);
							U32(pkt, 4) = ((uint32_t)(U16(f, 0xE) & 0x200) << 16) | U32(f, 0x10);
							U32(pkt, 0xC) = U32(f, 8);
							U32(pkt, 0x14) = U32(f, 0xC);
							U16(pkt, 0x16) &= 0xFDFF;
							U16(pkt, 0x1C) = U16(f, 6);
							w::ReadOTZ(sc + 0x68);
							InsertPrimAutoDepth((uint32_t)(ot + 4 * (uint32_t)(S32(sc, 0x68) >> (shift & 31))), pkt);
							pkt += 0x20;
						}
					}
				}
			}
			f += 0x14;
		}

		// quads, 0x18 bytes: u16 v0..v3; u32 uv0, uv1; u16 uv2, uv3; u32 colour
		for (;;)
		{
			int16_t c = S16(sc, 0xA);
			S16(sc, 0xA) = (int16_t)(c - 1);
			if (!c) break;
			int32_t *dp = *(int32_t **)(sc + 0x18);
			int32_t d = *dp;
			*(int32_t **)(sc + 0x18) = dp + 1;
			if (d != -1)
			{
				CopyVertex(sc, 0x20, verts, U16(f, 0));
				CopyVertex(sc, 0x30, verts, U16(f, 2));
				CopyVertex(sc, 0x40, verts, U16(f, 4));
				CopyVertex(sc, 0x50, verts, U16(f, 6));
				if (d >= (int32_t)thr) // below: a shard on the real tick, nothing here
				{
					uint8_t o3 = sc[0x56], o0 = sc[0x26], o1 = sc[0x36], o2 = sc[0x46];
					if ((uint8_t)(o3 | o0 | o1 | o2) < 0x10 && !(o2 & (uint8_t)(o3 & o0 & o1)))
					{
						w::SetSXY012(U32(sc, 0x20), U32(sc, 0x30), U32(sc, 0x40));
						w::NCLIP();
						w::ReadMAC0(sc + 0x60);
						if (S32(sc, 0x60) >= 0)
						{
							w::SetSZ0123(U16(sc, 0x24), U16(sc, 0x34), U16(sc, 0x44), U16(sc, 0x54));
							w::AVSZ4();
							U32(pkt, 0) = 0x09000000;
							U32(pkt, 8) = U32(sc, 0x20);
							U32(pkt, 0x10) = U32(sc, 0x30);
							U32(pkt, 0x18) = U32(sc, 0x40);
							U32(pkt, 0x20) = U32(sc, 0x50);
							U32(pkt, 4) = ((uint32_t)(U16(f, 0xE) & 0x200) << 16) | U32(f, 0x14);
							U32(pkt, 0xC) = U32(f, 8);
							U32(pkt, 0x14) = U32(f, 0xC);
							U16(pkt, 0x1C) = U16(f, 0x10);
							U16(pkt, 0x16) &= 0xFDFF;
							U16(pkt, 0x24) = U16(f, 0x12);
							w::ReadOTZ(sc + 0x68);
							InsertPrimAutoDepth((uint32_t)(ot + 4 * (uint32_t)(S32(sc, 0x68) >> (shift & 31))), pkt);
							pkt += 0x28;
						}
					}
				}
			}
			f += 0x18;
		}
		return (uint32_t)pkt;
	}

	// 0x6FF770 draw-only
	static uint32_t ObjectsHeld(uint8_t *com, uint8_t *sc, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *bones = *(uint8_t **)com + 0x10;
		const uint32_t *offs = (const uint32_t *)(*(uint8_t **)(com + 4) + 4);
		int32_t count = (int32_t)offs[-1];
		if (count <= 0) return cursor;
		do
		{
			uint8_t *obj = *(uint8_t **)(com + 4) + *offs++;
			uint8_t *vcur = *(uint8_t **)(sc + 4);
			int32_t groups = *(int16_t *)obj;
			obj += 2;
			if (groups > 0)
			{
				int32_t g = groups;
				do
				{
					int32_t bone = *(int16_t *)obj;
					obj += 2;
					const Mat4x3 *m = (const Mat4x3 *)(bones + 48 * bone + 0x10);
					GteSetRotMatrixCtrl(m);
					GteSetTransVectorCtrl(m);
					VertexGroup(&obj, &vcur, sc);
				} while (--g);
			}
			obj = (uint8_t *)(((uint32_t)obj + 3) & ~3u) + 4;
			S16(sc, 8) = *(int16_t *)obj;
			obj += 2;
			S16(sc, 0xA) = *(int16_t *)obj;
			obj += 6;
			*(uint8_t **)sc = obj;
			cursor = FacesHeld(sc, ot, shift, cursor);
		} while (--count);
		return cursor;
	}

	// 0x6FF6E0 draw-only, on the pose at tick + num / den
	static uint8_t *ShatterDrawHeld(uint8_t *model, uint8_t *dist, int32_t thr, int32_t mask, uint8_t *scratch,
		uint32_t ot, int32_t shift, int num, int den)
	{
		uint8_t *sc = (uint8_t *)FieldAlloc(0x70);
		U16(sc, 0x14) = (uint16_t)thr;
		*(uint8_t **)(sc + 0x18) = dist;
		U16(sc, 0x16) = (uint16_t)mask;
		uint8_t *hdr = model + 0x14; // BattleAnimHeader (entry + 0x18)
		*(uint8_t **)(sc + 0x1C) = ModelBuffer() + MB_SPIN;
		pose_midpoint(hdr, hdr + 0xC, num, den);
		*(uint8_t **)(sc + 4) = scratch;
		S16(sc, 0xC) = 0;
		S16(sc, 0xE) = 0;
		S16(sc, 0x10) = 0x140;
		S16(sc, 0x12) = 0xD8;
		FrameCursor() = ObjectsHeld(*(uint8_t **)(hdr + 4), sc, ot, shift, FrameCursor());
		uint8_t *r = *(uint8_t **)(sc + 0x18);
		FieldFree(0x70);
		return r;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Everything the real tick drew, half way to the next tick:
	//   prim model   exact in-between records (prim::play_held), uv scroll + 8
	//   attacker     0x6FECF0 on the in-between pose (pose_midpoint)
	//   models       the shattering rasteriser draw-only on the in-between pose, with the
	//                tick's threshold (faces shatter in 15 Hz batches)
	//   sparks       drawn position lerped to the updated one, frame of the drawn life
	//   shards       drawn position and corners lerped to the updated ones (spin between)
	//   fade         level lerped to the next tick's
	// Objects the tick's update removed (shards / sparks at the end of their life) are not
	// drawn; objects spawned after the draw appear on the next tick. Vertex scratch goes to
	// private buffers, skeletons are put back byte for byte.
	// ------------------------------------------------------------------
	static uint8_t g_held_packets[0x80000];
	static uint8_t g_held_verts[0x10000];       // model / prim vertex scratch (vanilla: MB + 0x6000)
	static uint8_t g_held_attacker_verts[0x40000]; // attacker vertex scratch (vanilla: *0x1D98B3C)
	static uint8_t g_skel_save[16 + 48 * 256];

	struct SkelGuard
	{
		uint8_t *sk = nullptr;
		uint32_t size = 0;
		explicit SkelGuard(uint8_t *anim_header)
		{
			uint8_t *com = *(uint8_t **)(anim_header + 4);
			uint8_t *s = com ? *(uint8_t **)com : nullptr;
			uint32_t n = s ? 16 + 48 * (uint32_t)s[0] : 0;
			if (s && n <= sizeof(g_skel_save)) { sk = s; size = n; memcpy(g_skel_save, sk, size); }
		}
		~SkelGuard() { if (sk) memcpy(sk, g_skel_save, size); }
		bool ok() const { return sk != nullptr; }
	};

	static inline int16_t lerp16(int16_t a, int16_t b, int num, int den) { return (int16_t)(a + (int16_t)(b - a) * num / den); }

	static void TimelineHeld(TimelineNode *node, int num, int den)
	{
		const TimelineMemo &M = g_tm;
		uint8_t *MB = ModelBuffer();
		int32_t c = M.c;
		bool step = M.advanced && node->counter == c + 1;

		if (M.prim)
		{
			PrimCtx ctx = M.ctx;
			ctx.scratch = (uint32_t)g_held_verts;
			if (step && c + 1 < 0x5C) ctx.scroll = (int16_t)(ctx.scroll + 16 * num / den);
			prim::play_held((prim::Layout *)node->prim, (prim::Callback)CB_PrimObject, (int)&ctx, num, den);
		}

		if (M.attacker)
		{
			uint8_t *a = Attacker();
			SkelGuard guard(a + 0x60);
			if (guard.ok())
			{
				pose_midpoint(a + 0x60, a + 0x6C, num, den);
				w::AttackerDraw(a, 0, (uint32_t)g_held_attacker_verts);
			}
		}

		if (M.models)
		{
			uint8_t *r;
			{
				SkelGuard guard(MODEL_1 + 0x14);
				r = MB + MB_FACE_DIST;
				if (guard.ok())
					r = ShatterDrawHeld(MODEL_1, r, M.thr, 0x3F, g_held_verts, RenderList() + 0x4068, 0x10, num, den);
			}
			{
				SkelGuard guard(MODEL_2 + 0x14);
				if (guard.ok() && r != MB + MB_FACE_DIST)
					ShatterDrawHeld(MODEL_2, r, M.thr, 0x3F, g_held_verts, RenderList() + 0x44, 0, num, den);
			}
		}

		if (M.sparks)
		{
			// updated this tick (life - 1, pos + 4 vel) when c < 47, untouched at 47
			bool moved = c < 0x2F;
			const uint8_t *cur = MB + MB_SPARKS;
			for (int i = 0; i < SPARK_COUNT; i++)
			{
				const Spark &p = *(const Spark *)(g_spark_prev + 0x10 * i);
				const Spark &q = *(const Spark *)(cur + 0x10 * i);
				Spark &h = *(Spark *)(g_spark_held + 0x10 * i);
				h = p;
				bool same = p.life != 0 && q.life != 0 && q.seq == p.seq &&
					(moved ? q.life == (uint8_t)(p.life - 1) : q.life == p.life);
				if (!same) { h.life = 0; continue; }
				for (int k = 0; k < 3; k++) h.pos[k] = lerp16(p.pos[k], q.pos[k], num, den);
			}
			SparksDraw(g_spark_held);
		}

		{
			const uint8_t *cur = MB + MB_SHARDS;
			for (int i = 0; i < SHARD_COUNT; i++)
			{
				const Shard &p = *(const Shard *)(g_shard_prev + 0x3C * i);
				const Shard &q = *(const Shard *)(cur + 0x3C * i);
				Shard &h = *(Shard *)(g_shard_held + 0x3C * i);
				h = p;
				if (p.type == 0 || q.type != p.type || q.life != (int16_t)(p.life - 1)) { h.type = 0; continue; }
				for (int k = 0; k < 3; k++) h.pos[k] = lerp16(p.pos[k], q.pos[k], num, den);
				for (int j = 0; j < 4; j++)
					for (int k = 0; k < 3; k++) h.v[j][k] = lerp16(p.v[j][k], q.v[j][k], num, den);
			}
			ShardsDraw(g_shard_held);
		}

		if (M.fade)
		{
			int32_t lv = FadeLevel(M.fade_t);
			if (step && M.fade_t + 1 < 0x20) lv = lerp_i(lv, FadeLevel(M.fade_t + 1), num, den);
			PacketCursor() = w::Tile(0xFF, 0xFF, 0xFF, lv, PacketCursor());
		}
	}

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// mirrors the master: effect camera matrix from the (held) camera, then the timeline;
	// packets go to a private buffer so the real tick's arenas are untouched
	static void HeldFrame(int num, int den)
	{
		if (g_tm.tick != g_real_tick || !QueueTimeline().head || (TimelineNode *)QueueTimeline().head != g_tm.node) return;
		uint32_t cursor = PacketCursor(), frame_cursor = FrameCursor();
		Mat4x3 effect_camera = EffectCamera();
		PacketCursor() = (uint32_t)g_held_packets;
		FrameCursor() = (uint32_t)g_held_packets + 0x40000;
		EffectCameraMatrix(&Camera(), &EffectCamera());
		TimelineHeld(g_tm.node, num, den);
		EffectCamera() = effect_camera;
		PacketCursor() = cursor;
		FrameCursor() = frame_cursor;
	}
}

	static void register_mag069_held()
	{
		mark_held(g069::ORIG_TimelineTask);
		register_module_held(69, g069::HeldReady, g069::HeldFrame);
	}
}
