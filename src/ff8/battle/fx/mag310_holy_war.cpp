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
// Effect 310: Holy War (battle items Holy War-trial / Holy War: kernel.bin battle item +4 = 310; MAG_310_*). No
// other kernel.bin entry uses effect 310. One of two adjacent modules built from one source (309 Hero: one
// target; 310 Holy War: every target of the action, one every 5 ticks).
//
// Structure (setup MAG_310 0x6664A0 -> 0x6664D0, file loader 0x6664B0 = the texture file named at 0x10244D8, the
// same name for both modules; the setup keeps the cast context (0x2507434) and the caster's entity (0x2507438, never
// read again), queues the TIM and starts the root task in its 1-node pool 0x2507460):
//   RootTask (0x666540) - alternates the packet arena (magic buffer + 0x16E8 / + 0x116E8 by tick parity,
//     cursor 0x2507474);
//     its counter runs 0..4 and wraps; on counter 1 (first time: the target pool, 6 x 0x3D0 bytes at the
//     start of the magic buffer) it starts the target task of the next target of action 0 (one every 5
//     ticks while targets are left); then the target queue; ends when the target queue is empty.
//   PlayTask (0x666730) - one target, 0x28 ticks. Set up at its start: the anchor = the target's
//     default effect position (0x571400) with y = h / 8 + h / 2 above its +0x24 (h = its +0x92), the scale
//     0x400 + its +0x26 / 2, three prim-model layouts (main 0x10231D8, 0x13C bytes; orbit 0x1023A60 twice,
//     0xBC bytes each) and 16 shards (rotation angles 0 / rand / 256 i, spin 0 / rand / rand, CRT rand
//     0x55CBD2, 3 draws each). Each tick, in this order, drawing THEN advancing:
//     - counter < 0x12: the two orbit layouts at opposite points of a spiral around the anchor (angle
//       q / 2 + 0x800 j, radius (0x1000 - q) * scale >> 12, q = (counter << 12) / 18; depth 0);
//     - counter < 0x28: the main layout at the anchor (depth -scale / 8);
//     - 0x10 <= counter < 0x24: the 16 shards (model 0x102421C, 0x7043B0 mode 0xF0): rotation from their
//       angles, scaled by the scale, at the anchor through the camera; fade (k = counter - 0x10)
//       0x1000 - k * 0x1000 / 5 for k <= 5, else (k - 5) * 0x1000 / 15; each shard's angles then turn by
//       its spin;
//     - sound 0x10244D4 on counter 1;
//     - counter 0x20: the action result of this target (0x666A20: every target record of action 0
//       whose slot is this entity's, ApplyActionResultToTarget: the invincibility);
//     - ends after counter 0x27.
//   PrimCallback (0x666A90) - one object of a layout (X-ATM092's 0x686120 without its flags 0x40000 / 0x1000):
//     vertex frame (or blend into the morph buffer, magic buffer + 0x16E0: 8 bytes below packet arena A; no
//     layout of the module blends), rotation from the record's
//     angles (0x701310); when the play block's word +0x14 is non-zero (see NEVER-WRITTEN STACK WORDS)
//     the record offset and the matrix are scaled by the block's scale; flag 0x200: the anchor
//     through the camera + the offset, else the offset + the anchor through the camera, composed with
//     it; + the camera translation; scale (flag 0x100: 3x3 product, else scale3DMatrix); fade; 0x7043B0.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: no task writes an entity, its position or the camera words (only read: the
// target's position, +0x24 / +0x26 / +0x92, the camera matrix 0x1D97778); the invincibility is the
// engine's ApplyActionResultToTarget.
// Module globals: 0x2507430..0x250747C (texture file 0x2507430, context 0x2507434, caster entity 0x2507438, target
// queue 0x2507440, root queue 0x2507450, root pool 0x2507460 (1 x 0x14), packet cursor 0x2507474, magic buffer base
// 0x2507478). The target pool, the morph buffer and the packet arenas are in the magic buffer.
// NEVER-WRITTEN STACK WORDS (the play task's prim block, a stack block of the play task; the game's
// executor 0x508420 calls every task of a queue at one esp):
//   - word +0x14 (play entry esp - 0xC = root entry esp - 0x34): non-zero = the callback scales the
//     layouts' offsets and matrices by the block's scale. No play task writes it, so every play task of
//     a tick reads what was there when the queue started. On the tick the root starts a play task: the
//     last write of its set-up calls (DR0 write watch under the game's executor),
//     its push of the new node's first orbit layout (node + 0x258) for DecodeModelPrimLayout (non-zero);
//     on the other ticks: the engine's residue between two ticks (the harnesses clear the stack: 0, or
//     0xA5A5A5A5). The port reads that word at its own entry for the first play task of a tick
//     (PlayTask), see PrimScaled.
//   - the orbit blocks' anchor word +6 and the callback's offset word +6 only reach the GTE VZ0 high
//     half; the callback's matrix pad +0x12 only the GTE R33 word's high half.

#include "mag_common.h"
#include <intrin.h>

namespace ff8fx
{
namespace holywar310
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507434); }
	inline TaskQueue *QPlay() { return (TaskQueue *)0x2507440; }             // pool: magic buffer, 6 x 0x3D0
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507474); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507478); }          // Magic_TextureOFF (magic buffer)

	static const uint32_t ORIG_RootTask = 0x666540;
	static const uint32_t ORIG_PlayTask = 0x666730;
	static const uint32_t MODEL_MAIN = 0x10231D8;     // prim-model layout data
	static const uint32_t MODEL_ORBIT = 0x1023A60;
	static const uint32_t MODEL_DEBRIS = 0x102421C;   // shard prim model
	static const uint32_t NODE_BYTES = 0x3D0;     // play node
	static const uint32_t MORPH = 0x16E0;          // magic buffer: blended vertex frames
	static const void *const SOUND = (const void *)0x10244D4;
	static const uint32_t ARENA_A = 0x16E8;         // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x116E8;
	static const int32_t MAIN_TICKS = 0x28, ORBIT_TICKS = 0x12, DEBRIS_FIRST = 0x10, DEBRIS_TICKS = 0x14;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_RenderPrimModel: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // MatrixMultiply3x3
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2507460)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C runs 0..4
		uint8_t next;      // +0x0E next target index
		uint8_t started;   // +0x0F target pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct Shard // 0x10 bytes
	{
		int16_t ang[3];    // +0x00 rotation angles
		int16_t pad06;     // +0x06 (never written: the magic buffer's bytes; 0x56CD50 reads 3 words)
		int16_t spin[3];   // +0x08 added to the angles after each draw
		int16_t pad0E;
	};
	struct PlayNode // pool of 6 node(s) of 0x3D0 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t scale;     // +0x0E 0x400 + the target's +0x26 / 2
		uint8_t *entity;   // +0x10 the target
		Shard shard[16];   // +0x14
		int16_t anchor[4]; // +0x114 x, y, z, the 4th word of 0x571400
		uint8_t main[0x13C];   // +0x11C main layout
		uint8_t orbit[2][0xBC]; // +0x258 orbit layouts
	};
	// the prim-player callback's parameter block (a stack block of the play task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 x, y, z (+6: never written in the original, GTE VZ0 high half only)
		int32_t scale[3];  // +0x08
		int32_t scaled;    // +0x14 never written in the original (PrimScaled)
		int32_t depth;     // +0x18
		uint32_t morph;    // +0x1C blended vertex frames
	};
	// the shard draw's Field_Alloc block (0x88 bytes)
	struct ShardDraw
	{
		Mat4x3 m;          // +0x00
		uint8_t h[0x58];   // +0x20 prim model header (+0 model, +8 colour, +0xC fade, +0x18 depth, +0x1C mode)
		int32_t scale[3];  // +0x78
		uint8_t pad84[4];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(PlayNode) == NODE_BYTES, "Holy War nodes");
	static_assert(sizeof(PrimArg) == 0x20 && sizeof(ShardDraw) == 0x88, "Holy War blocks");

	// ------------------------------------------------------------------
	// Prim-player callback (0x666A90): one object of a layout
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		const int32_t object = (int16_t)r->flags_lo;
		uint8_t *model = l->data + *(const int32_t *)(l->data + object * 4 + 8);
		*(uint8_t **)h = model;
		const int16_t f0 = r->b0, f1 = r->b1;
		const int32_t nverts = *(const int32_t *)(model + 4);
		if (f0 == f1)
		{
			if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
			else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
		}
		else
		{
			const int16_t t = r->b;
			if (t == 0)
			{
				if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
			}
			else if (t == 0x1000)
			{
				if (f1 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f1) * 8 + 0xC;
			}
			else
			{
				BlendVertexFrames((uint32_t)model, f0, f1, t, arg->morph);
				*(uint32_t *)(h + 4) = arg->morph;
			}
		}
		Mat4x3 m = {}; // (pad +0x12 never written in the original: GTE R33 word only)
		RotationFromAngles(r->rot, &m);
		int16_t off[4] = {}; // (4th word never written in the original: GTE VZ0 high half only)
		const int32_t scaled = arg->scaled;
		if (scaled)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
		}
		else
		{
			off[0] = r->pos[0];
			off[1] = r->pos[1];
			off[2] = r->pos[2];
		}
		if (scaled) Scale3DMatrix(&m, arg->scale);
		const uint32_t flags = r->flags;
		if (flags & 0x200)
		{
			// the anchor through the camera (its 4th word goes to the GTE V0 pad only) + the offset
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			// the offset + the anchor through the camera, composed with it
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (flags & 0x100)
			{
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MatrixMultiply3(&m, &d);
			}
			else
			{
				const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		PacketCursor() = RenderPrimModel2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// orbit layout j's position at counter c (the play task's spiral)
	static void OrbitPos(const PlayNode *p, int32_t c, int j, int16_t out[3])
	{
		const uint32_t q = (uint32_t)shl32(c, 12) / 18u;
		const int32_t radius = mul32((int32_t)(0x1000 - q), p->scale) / 0x1000;
		const int32_t angle = ((int32_t)q >> 1) + 0x800 * j;
		out[0] = (int16_t)(mul32(ComputeSin(angle), radius) / 0x1000 + p->anchor[0]);
		out[1] = p->anchor[1];
		out[2] = (int16_t)(mul32(ComputeCos(angle), radius) / 0x1000 + p->anchor[2]);
	}

	// the shards' fade at shard tick k (0..0x13)
	static int32_t ShardFade(int32_t k)
	{
		const uint32_t t = (uint32_t)shl32(k, 12);
		if ((uint32_t)k <= 5) return (int32_t)(0x1000 - t / 5u);
		return (int32_t)((uint32_t)shl32(k - 5, 12) / 15u);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag310_holy_war_held.h"
#endif

namespace ff8fx
{
namespace holywar310
{
	// The play block's word +0x14 is never written by the play task (entry esp - 0xC): what the stack
	// holds there when the play queue starts (no play task writes it); non-zero = the callback scales
	// the layouts' offsets and matrices by the block's scale. On a tick the root starts a play task its
	// set-up calls wrote it last (the new node + 0x258, pushed for DecodeModelPrimLayout);
	// on the other ticks it is the engine's residue between two ticks, read at the first play task's
	// entry (the game's executor calls every task of the queue at one esp; the following tasks of the
	// tick read the same word).
	static int32_t g_scaled = 0;       // the word the play tasks of this tick read
	static bool g_spawned = false;     // the root started a play task on this tick
	static bool g_first = false;       // no play task ran yet on this tick
	static int32_t g_spawn_word = 0;   // what its set-up calls left in the word
	static inline int32_t PrimScaled() { return g_scaled; }

	// ------------------------------------------------------------------
	// Play (0x666730)
	// ------------------------------------------------------------------
	static __declspec(noinline) uint32_t PlayTaskBody(PlayNode *p, int32_t entry_word)
	{
		if (g_first)
		{
			g_first = false;
			g_scaled = g_spawned ? g_spawn_word : entry_word;
		}
		PrimArg arg;
		arg.pos[3] = 0;
		arg.scaled = PrimScaled();
		// 30 fps layer: see mag310_holy_war_held.inc
		FX_HELD(held_note_play(p);)
		if ((uint32_t)(int32_t)p->counter < (uint32_t)ORBIT_TICKS)
		{
			for (int j = 0; j < 2; j++)
			{
				OrbitPos(p, p->counter, j, arg.pos);
				arg.scale[2] = p->scale;
				arg.scale[1] = p->scale;
				arg.scale[0] = p->scale;
				arg.depth = 0;
				arg.morph = TexBase() + MORPH;
				prim::play((prim::Layout *)p->orbit[j], PrimCallback, (int)&arg, 0);
			}
		}
		if ((uint32_t)(int32_t)p->counter < (uint32_t)MAIN_TICKS)
		{
			memcpy(arg.pos, p->anchor, sizeof(arg.pos));
			arg.scale[2] = p->scale;
			arg.scale[1] = p->scale;
			arg.scale[0] = p->scale;
			arg.depth = -(p->scale / 8);
			arg.morph = TexBase() + MORPH;
			prim::play((prim::Layout *)p->main, PrimCallback, (int)&arg, 0);
		}
		const int32_t k = p->counter - DEBRIS_FIRST;
		if ((uint32_t)k < (uint32_t)DEBRIS_TICKS)
		{
			ShardDraw *d = (ShardDraw *)FieldAlloc(0x88);
			*(uint32_t *)(d->h + 0) = MODEL_DEBRIS;
			*(uint32_t *)(d->h + 8) = 0;
			*(int32_t *)(d->h + 0xC) = ShardFade(k);
			*(uint32_t *)(d->h + 0x1C) = 0xF0;
			*(int32_t *)(d->h + 0x18) = -(p->scale / 8);
			for (int i = 0; i < 16; i++)
			{
				Shard &s = p->shard[i];
				BuildRotationMatrixFromAngles(s.ang, &d->m);
				d->scale[2] = p->scale;
				d->scale[1] = p->scale;
				d->scale[0] = p->scale;
				Scale3DMatrix(&d->m, d->scale);
				d->m.t[0] = p->anchor[0];
				d->m.t[1] = p->anchor[1];
				d->m.t[2] = p->anchor[2];
				ComposeAffineTransform(&Camera(), &d->m, &d->m);
				GteSetRotMatrix(&d->m);
				GteSetTransVector(&d->m);
				PacketCursor() = RenderPrimModel2(d->h, RenderOT(), 2, PacketCursor());
				s.ang[0] = (int16_t)(s.ang[0] + s.spin[0]);
				s.ang[1] = (int16_t)(s.ang[1] + s.spin[1]);
				s.ang[2] = (int16_t)(s.ang[2] + s.spin[2]);
			}
			FieldFree(0x88);
		}
		if (p->counter == 1) BdPlaySE(SOUND, 0, 0x80);
		if (p->counter == 0x20)
		{
			// 0x666A20: every target record of action 0 whose slot is this entity's
			const int32_t slot = (int32_t)((uint32_t)p->entity - 0x1D972C0) / 0x9C;
			for (int32_t i = 0; i < (int32_t)Ctx()->actions[0].target_count; i++)
			{
				uint8_t *t = Ctx()->actions[0].targets + i * TARGET_STRIDE;
				if ((int32_t)*t == slot) ApplyActionResultToTarget(t);
			}
		}
		p->counter++;
		return p->counter < MAIN_TICKS ? 0 : TASK_END;
	}
	static uint32_t __cdecl PlayTask(TaskNode *n)
	{
		// the original's block word +0x14 (entry esp - 0xC), read before anything is written below this entry
		return PlayTaskBody((PlayNode *)n, *(const int32_t *)((const uint8_t *)_AddressOfReturnAddress() - 0xC));
	}

	// ------------------------------------------------------------------
	// Root task (0x666540)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag310_holy_war_held.inc
		FX_HELD(held_note_root();)
		g_spawned = false;
		g_first = true;
		if (r->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QPlay(), (void *)TexBase(), NODE_BYTES, 6);
			}
		}
		if (r->counter == 1 && r->next < Ctx()->actions[0].target_count)
		{
			PlayNode *p = (PlayNode *)AddTaskToQueue(QPlay(), ORIG_PlayTask);
			g_spawned = true;
			g_spawn_word = (int32_t)p->orbit[0];
			uint8_t *e = Entity(Ctx()->actions[0].targets[r->next * TARGET_STRIDE]);
			p->counter = 0;
			p->entity = e;
			p->scale = (int16_t)((*(const int16_t *)(e + 0x26) >> 1) + 0x400);
			GetDefaultEffectPosition(e, p->anchor);
			// y: h / 8 + h / 2 above the target's +0x24 (h = its +0x92)
			const int32_t h = *(const int16_t *)(e + 0x92);
			p->anchor[1] = (int16_t)(h / 8 + h / 2 + *(const int16_t *)(e + 0x24));
			DecodeModelPrimLayout(MODEL_MAIN, p->main, 0x13C);
			DecodeModelPrimLayout(MODEL_ORBIT, p->orbit[0], 0xBC);
			DecodeModelPrimLayout(MODEL_ORBIT, p->orbit[1], 0xBC);
			for (int i = 0; i < 16; i++)
			{
				Shard &s = p->shard[i];
				s.ang[0] = 0;
				s.ang[1] = (int16_t)((CrtRand() & 0x7FF) - 0x400);
				s.spin[0] = 0;
				s.ang[2] = (int16_t)(i << 8);
				s.spin[1] = (int16_t)((CrtRand() & 0x3F) - 0x20);
				s.spin[2] = (int16_t)((CrtRand() & 0x3F) - 0x20);
			}
			r->next++;
		}
		int a;
		if (r->started) a = ExecuteTaskQueue(QPlay());
		else a = (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		if (r->counter > 4) r->counter = 0;
		return 0;
	}
}

	void register_mag310_holy_war()
	{
		register_port(holywar310::ORIG_RootTask, (void *)holywar310::RootTask, "310 RootTask", 310);
		register_port(holywar310::ORIG_PlayTask, (void *)holywar310::PlayTask, "310 PlayTask", 310);
		// 30 fps layer: see mag310_holy_war_held.inc
		FX_HELD(register_mag310_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag310_holy_war_held.inc"
#endif
