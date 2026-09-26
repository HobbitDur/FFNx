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

// Effect 118: Aero (spell, MAG_118_*).
//
// Structure (setup MAG_118_AERO 0x6C0C80 -> _Init 0x6C0CB0, file loader 0x6C0C90 = the texture
// file named at 0x128E2C4; the setup starts camera animation 0x128D5B0 and queues the TIM):
//   RootTask (0x6C0D20) - alternates the packet arena (magic buffer + 0x2584 / + 0x1A584); on
//     counter 1 of a 10-tick cycle sets up the pools (first time) and starts the target task of the
//     next action on its first target (no wait); computes the effect camera, runs the target and
//     leaf queues, ends when both are empty.
//   Target (0x6C0EC0) - node 0x68C, one per action: sound at 8; from tick 11 plays two prim-model
//     layouts with the callback WindPart (0x6C10A0): the whirlwind 0x1287DBC at the target's feet
//     and 0x128C110 at its effect anchor, both scaled by its size; over 0..19 spawns a leaf per
//     tick; when both layouts are over: damage, ends.
//   Leaf (0x6C14B0) - node 0x3C: the leaf prim model 0x1287C44 spinning on three axes: while its
//     target task is younger than 40 ticks it grows in and circles the target, climbing (spiral
//     radius shrinking with height), then it falls, bouncing on the ground and drifting outwards,
//     and shrinks away after 17 more ticks.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x25215D8..0x2521650 (texture file, magic buffer base, context, root pool,
// packet cursor 0x25215FC, effect camera 0x2521600, leaf / target / root queues). The target
// pool (3 x 0x68C), the leaf pool (64 x 0x3C), the morph vertex buffer (+0x22A4) and the packet
// arenas are in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace aero118
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x25215DC); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25215E0); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25215FC); }
	inline Mat4x3 &EffectCamera() { return var<Mat4x3>(0x2521600); }
	inline TaskQueue *QLeaves() { return (TaskQueue *)0x2521620; }          // pool: + 0x13A4, 0x40 x 0x3C
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2521630; }         // pool: magic buffer, 3 x 0x68C
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6C0D20;
	static const uint32_t ORIG_TargetTask = 0x6C0EC0;
	static const uint32_t ORIG_LeafTask = 0x6C14B0;
	static const uint32_t MODEL_Whirl = 0x1287DBC;       // prim-model layout data (0x4EC-byte layout)
	static const uint32_t MODEL_Gust = 0x128C110;        // prim-model layout data (0x184-byte layout)
	static const uint32_t MODEL_Leaf = 0x1287C44;        // prim model
	static const void *const SOUND_Aero = (const void *)0x128D5AC;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_sub_7043B0: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // sub_56C220: a = a * b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..9, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes of 0x68C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index (-1 once the damage is done)
		uint8_t *entity;   // +0x10 target entity
		int16_t pos[4];    // +0x14 effect anchor x, y, z, height (GetDefaultEffectPosition)
		uint8_t whirl[0x4EC]; // +0x1C prim-model layout (prim::Layout: data, frame, state)
		uint8_t gust[0x184];  // +0x508
	};
	struct LeafNode // pool of 0x40 nodes of 0x3C bytes
	{
		TaskNode hdr;
		int16_t action;    // +0x0C (the target task's action, not read)
		int16_t reach;     // +0x0E spiral reach (target size / 2 + 0x800)
		int16_t scale;     // +0x10 size 4.12, grows to 0x1000
		int16_t angle;     // +0x12 around the target
		int16_t speed;     // +0x14 angle step
		int16_t cx;        // +0x16 target anchor x
		int16_t cz;        // +0x18 target anchor z
		int16_t pos[3];    // +0x1A x, y (0..0xFF at the spawn), z
		int16_t pad20;
		int16_t cosv;      // +0x22 cos(angle) of the last circling tick
		int16_t vy;        // +0x24 vertical speed
		int16_t sinv;      // +0x26 sin(angle) of the last circling tick
		int16_t pad28;
		int16_t ang[3];    // +0x2A spin angles
		int16_t fall;      // +0x30 ticks since the fall started
		int8_t spin[3];    // +0x32 spin speeds (* 8)
		uint8_t pad35[3];
		void *owner;       // +0x38 its target task
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 x, y, z (+ height word)
		int32_t scale[3];  // +0x08 4.12, applied to the record offsets and the matrix
		int32_t scaled;    // +0x14 scale present
		int32_t depth;     // +0x18 prim header +0x18
		uint32_t morph;    // +0x1C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x68C && sizeof(LeafNode) == 0x3C, "Aero nodes");
	static_assert(sizeof(PrimArg) == 0x20, "Aero prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x6C10A0): one object of the whirlwind / gust models
	// ------------------------------------------------------------------
	static void __cdecl WindPart(prim::Layout *l, prim::Record *r, int arg_)
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
		Mat4x3 m = {};
		RotationFromAngles(r->rot, &m);
		// the record's offset, scaled by the block's scale
		int16_t off[4] = {};
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
		if (flags & 0x1000)
		{
			// the block position through the camera, the offset through the effect camera
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			GteSetRotMatrixCtrl(&EffectCamera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t mac[3];
			GteReadMAC123(mac);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)mac[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)mac[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)mac[2]);
			if (!(r->flags & 0x8000)) MatrixMultiply(&EffectCamera(), &m);
		}
		else
		{
			// block position + offset through the camera, rotation composed with the camera
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				// diagonal matrix product (sub_56C220)
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MatrixMultiply3(&m, &d);
			}
			else
			{
				// each row of the rotation times its scale (a scale of 1.0 leaves the row)
				for (int k = 0; k < 3; k++)
				{
					const int16_t s = r->scale[k];
					if (s == 0x1000) continue;
					m.m[k][0] = (int16_t)(mul32(m.m[k][0], s) >> 12);
					m.m[k][1] = (int16_t)(mul32(m.m[k][1], s) >> 12);
					m.m[k][2] = (int16_t)(mul32(m.m[k][2], s) >> 12);
				}
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
		PacketCursor() = RenderPrimModel2(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Leaf (0x6C14B0)
	// ------------------------------------------------------------------
	// the tick's motion before the draw; false = finished (shrunk away)
	static bool LeafMove(LeafNode *f)
	{
		const int16_t oc = ((const TargetNode *)f->owner)->counter;
		if (oc < 0x28)
		{
			// circling the target, climbing: the spiral radius shrinks with the height
			if (oc >= 0xA)
			{
				f->vy = (int16_t)(f->vy - 4);
				f->speed = (int16_t)(f->speed + 0x10);
			}
			f->scale = (int16_t)(f->scale + 0x100);
			if (f->scale > 0x1000) f->scale = 0x1000;
			f->pos[1] = (int16_t)(f->pos[1] + f->vy);
			const int32_t y = f->pos[1] >> 6;
			const int32_t radius = shl32(f->reach, 5) / (mul32(y, y) + 0x200);
			f->angle = (int16_t)(f->angle + f->speed);
			const int32_t c = ComputeCos(f->angle);
			f->cosv = (int16_t)c;
			f->pos[0] = (int16_t)((mul32((int16_t)c, radius) >> 9) + f->cx);
			const int32_t s = ComputeSin(f->angle);
			f->sinv = (int16_t)s;
			f->pos[2] = (int16_t)((mul32((int16_t)s, radius) >> 9) + f->cz);
		}
		else
		{
			// falling, bouncing on the ground, drifting outwards; shrinks away after 17 ticks
			const int16_t n = f->fall;
			f->fall = (int16_t)(n + 1);
			if (n > 0x10)
			{
				f->scale = (int16_t)(f->scale - 0x200);
				if (f->scale <= 0) return false;
			}
			f->vy = (int16_t)(f->vy + 0x10);
			f->pos[1] = (int16_t)(f->pos[1] + f->vy);
			if (f->pos[1] >= 0) f->vy = (int16_t)-f->vy;
			f->pos[0] = (int16_t)(f->pos[0] + (int16_t)(f->cosv >> 6));
			f->pos[2] = (int16_t)(f->pos[2] + (int16_t)(f->sinv >> 6));
		}
		f->ang[0] = (int16_t)(f->ang[0] + (int16_t)(f->spin[0] << 3));
		f->ang[1] = (int16_t)(f->ang[1] + (int16_t)(f->spin[1] << 3));
		f->ang[2] = (int16_t)(f->ang[2] + (int16_t)(f->spin[2] << 3));
		return true;
	}

	static void LeafDraw(const LeafNode *f)
	{
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_Leaf;
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(f->ang, &m);
		const int32_t v[3] = { f->scale, f->scale, f->scale };
		Scale3DMatrix(&m, v);
		m.t[0] = f->pos[0];
		m.t[1] = f->pos[1];
		m.t[2] = f->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag118_aero_held.h"
#endif

namespace ff8fx
{
namespace aero118
{
	static uint32_t __cdecl LeafTask(TaskNode *n)
	{
		LeafNode *f = (LeafNode *)n;
		if (!LeafMove(f)) return TASK_END;
		// 30 fps layer: see mag118_aero_held.inc
		FX_HELD(held_note_leaf(f);)
		LeafDraw(f);
		return 0;
	}

	// ------------------------------------------------------------------
	// Target (0x6C0EC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		int alive = 1;
		if (t->counter == 8) BdPlaySE3D(SOUND_Aero, 1, t->pos);
		const int32_t reach = (*(const int16_t *)(t->entity + 0x26) >> 1) + 0x800;
		if (t->counter > 0xA)
		{
			// the whirlwind at the target's feet, the gust at its anchor, scaled by its size
			PrimArg arg;
			arg.pos[0] = t->pos[0];
			arg.pos[2] = t->pos[2];
			arg.pos[1] = *(const int16_t *)(t->entity + 0x24);
			arg.pos[3] = 0; // never written by the original, never read
			arg.scale[2] = reach;
			arg.scale[1] = reach;
			arg.scale[0] = reach;
			arg.scaled = 1;
			arg.depth = (int32_t)(0xFFFFF800u - (uint32_t)(int32_t)*(const int16_t *)(t->entity + 0x26)) >> 5;
			arg.morph = TexBase() + 0x22A4;
			// 30 fps layer: see mag118_aero_held.inc
			FX_HELD(held_note_play(t->whirl, &arg);)
			alive = prim::play((prim::Layout *)t->whirl, WindPart, (int)&arg, 0);
			GetDefaultEffectPosition(t->entity, arg.pos);
			// 30 fps layer: see mag118_aero_held.inc
			FX_HELD(held_note_play(t->gust, &arg);)
			alive |= prim::play((prim::Layout *)t->gust, WindPart, (int)&arg, 0);
		}
		if ((uint16_t)t->counter < 0x14)
		{
			LeafNode *f = (LeafNode *)AddTaskToQueue(QLeaves(), ORIG_LeafTask);
			if (f)
			{
				Memset32(&f->action, 0, 0xC);
				f->action = t->action;
				f->cx = t->pos[0];
				f->cz = t->pos[2];
				const int32_t y = CrtRand() & 0xFF;
				f->owner = t;
				f->pos[1] = (int16_t)y;
				f->reach = (int16_t)reach;
				f->scale = 0x200;
				f->angle = (int16_t)CrtRand();
				f->speed = 0x40;
				f->ang[0] = (int16_t)CrtRand();
				f->ang[1] = (int16_t)CrtRand();
				f->ang[2] = (int16_t)CrtRand();
				f->spin[0] = (int8_t)(CrtRand() + 0x80);
				f->spin[1] = (int8_t)(CrtRand() + 0x80);
				const int32_t s2 = CrtRand();
				f->vy = (int16_t)0xFFFC;
				f->spin[2] = (int8_t)(s2 + 0x80);
			}
		}
		t->counter++;
		if (alive == 0)
		{
			ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
			t->action = -1;
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6C0D20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag118_aero_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x2584;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x1A584;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x68C, 3);
				InitTaskQueuePool(QLeaves(), (void *)(TexBase() + 0x13A4), 0x3C, 0x40);
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
				Memset32(&t->counter, 0, 0x1A0);
				t->action = (int16_t)(uint16_t)r->action;
				t->entity = Entity(Ctx()->actions[t->action].targets[0]);
				GetDefaultEffectPosition(t->entity, t->pos);
				DecodeModelPrimLayout(MODEL_Whirl, t->whirl, 0x4EC);
				DecodeModelPrimLayout(MODEL_Gust, t->gust, 0x184);
				r->action++;
			}
		}
		EffectCameraMatrix(&Camera(), &EffectCamera());
		int a, b;
		if (r->started)
		{
			a = ExecuteTaskQueue(QTargets());
			b = ExecuteTaskQueue(QLeaves());
		}
		else
		{
			a = (int)n;
			b = (int)n;
		}
		if (r->started && a == 0 && b == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 0;
		return 0;
	}
}

	void register_mag118_aero()
	{
		register_port(aero118::ORIG_RootTask, (void *)aero118::RootTask, "A118 RootTask", 118);
		register_port(aero118::ORIG_TargetTask, (void *)aero118::TargetTask, "A118 TargetTask", 118);
		register_port(aero118::ORIG_LeafTask, (void *)aero118::LeafTask, "A118 LeafTask", 118);
		// 30 fps layer: see mag118_aero_held.inc
		FX_HELD(register_mag118_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag118_aero_held.inc"
#endif
