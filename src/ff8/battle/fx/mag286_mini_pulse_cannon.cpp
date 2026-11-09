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

// Effect 286: Mini Pulse Cannon (enemy attack 293 of kernel.bin; Droma c0m110; MAG_286_*).
//
// Structure (setup 0x679E10 -> _Init 0x679E40, file loader 0x679E20 = mag285.tim (name at 0x1084AE8) into 0x2507CA8; _Init
// keeps the context and the CASTER's entity (0x2507CB0), sets up the master queue 0x2507CD8 (pool 0x2507CE8, 1 x 0x14), starts
// camera animation 0x10847B0 and queues the TIM):
//   MasterTask (0x679EC0) - alternates the packet arena (magic buffer + 0xFD4 / + 0x10FD4); on counter 1 (once) sets up the
//     pools (main 1 x 0x6D4 at + 0x480, sparks 0x20 x 0x24 at + 0) and starts the main task (target = action 0's first target,
//     its default effect position, layouts 0x107DD94 / 0x10809EC / 0x1078BC8 / 0x107D004). Then: the caster's world bone
//     matrices (0x5095B0), the main queue (when it is empty: the caster's entity word +0 loses bit 3), the caster's bone
//     matrices rebuilt from its pose (0x508C90), the spark queue; ends when both queues are empty.
//   MainTask (0x67A070) - counters 25..66: the muzzle point P = the caster's bone 2 at offset (-1718, 0, -737) (0x67A770), the
//     muzzle layout A in RotY(caster facing) at P composed with the camera (depth -0x20); counters 25..32: 2 sparks around P
//     (CRT rand: angle (c + 0x10) x 0x100 + rand & 0x3F - 0x20 (- 0x800 for the second), radius 0x200 + rand & 0x3FF, depth
//     rand & 0xFF - 0x80, turned by RotY; they fly back to P in 10 ticks); the beam layout B in the basis of P -> the target's
//     position (y: P - the target's height until counter 38, then scaled by (0x18 - c) / 10 down to 0 at counter 49), stretched
//     along its axis (z scale 0x1000 until counter 39, then the distance / 4, from counter 52 the distance / 2), texture scroll
//     (c & 3) x 0x20 for its objects 0 / 1. Counters 49..68: the impact layouts C (at the target's position, facing P, flat) and
//     D (the same at the target's height, depth -0x80). Sound 0x10847AC at counter 24; damage of action 0's first target record
//     at counter 53; ends after counter 68.
//   SparkTask (0x67A7C0) - sequence (+0x0C) through the shadow-rotation camera (+0x10 = 0x4B0, -0x4B0 / 4), frame = its counter,
//     then moves by its velocity; ends when its counter reaches its life (+0x22).
//   PrimCallback (0x67A4E0) - one layout object drawn by 0x701DD0 (header 0x6C): with a texture scroll in the block (+0x22),
//     objects 0 / 1 get the texture window 0x100 x 0x100 scrolled by it (object 0: v base 0, object 1: v base 0x80).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update); the main task spawns its sparks before its draws.
// ENTITY WRITES: the caster - its world bone matrices (0x5095B0) and bone matrices from its pose (0x508C90) every tick, its
// word +0 bit 3 cleared every tick once the main task has ended; the target's action result (engine). No entity position word
// is written (entity +0x0E / +0x24 / bones only read).
// CAMERA: the setup's camera animation 0x10847B0 (engine); no camera word is written by the tasks.
// Module globals: 0x2507CA8..0x2507D04 (texture file 0x2507CA8, context 0x2507CAC, caster 0x2507CB0, the queues 0x2507CB8
// sparks / 0x2507CC8 main / 0x2507CD8 master, master pool 0x2507CE8, packet cursor 0x2507CFC, magic buffer base 0x2507D00).
// STACK WORDS: the main task's parameter block (+0x20 depth, +0x22 scroll, +0x24 blend buffer) is written by the counters
// 25..66 part; the layout C play of counters 67 / 68 reads it as an earlier call left it (RESIDUE in MainTask). The 4th words of
// the vectors loaded into GTE V0 (the spark's offset: its frame word; the callback's record offset) reach the GTE's VZ0 high half
// (0x1CA8A16) only.

#include "mag_common.h"

namespace ff8fx
{
namespace minipulse286
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507CAC); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2507CB0); }        // caster entity (setup)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2507CB8; }         // pool: magic buffer, 0x20 x 0x24
	inline TaskQueue *QMain() { return (TaskQueue *)0x2507CC8; }           // pool: + 0x480, 1 x 0x6D4
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507CFC); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507D00); }        // Magic_TextureOFF (magic buffer)
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_MasterTask = 0x679EC0;
	static const uint32_t ORIG_MainTask = 0x67A070;
	static const uint32_t ORIG_SparkTask = 0x67A7C0;
	static const uint32_t ORIG_PrimCallback = 0x67A4E0;   // not a task: the layouts' callback
	static const uint32_t POOL_Main = 0x480;
	static const uint32_t MORPH = 0xB54;                 // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0xFD4, ARENA_B = 0x10FD4; // magic buffer: packet arenas
	static const uint32_t MODEL_Muzzle = 0x107DD94;      // prim-model layouts
	static const uint32_t MODEL_Beam = 0x10809EC;
	static const uint32_t MODEL_Impact = 0x1078BC8;
	static const uint32_t MODEL_Ground = 0x107D004;
	static const uint32_t SEQ_Spark = 0x10789A8;         // sprite sequence
	static const int16_t *const MUZZLE = (const int16_t *)0x1084AE0; // offset (-1718, 0, -737), bone 2
	static const void *const SOUND_Shot = (const void *)0x10847AC;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b.rot = a.rot x b.rot
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); } // MAG_063_RenderPrimSet
	inline int32_t BuildOrthonormalBasis(int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); } // returns |v|
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }                // Effect_RotationYMatrix
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2507CE8)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x6D4 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint8_t *ent;      // +0x10 target entity (action 0's first target)
		int16_t pos[4];    // +0x14 the target's default effect position
		uint8_t muzzle[0x22C]; // +0x1C layout A
		uint8_t beam[0x158];   // +0x248 layout B
		uint8_t impact[0x2A4]; // +0x3A0 layout C
		uint8_t ground[0x90];  // +0x644 layout D
	};
	struct SparkNode // pool of 0x20 nodes of 0x24 bytes
	{
		TaskNode hdr;
		uint32_t seq;      // +0x0C sprite sequence
		int32_t shadow;    // +0x10 shadow-rotation camera distance (0x4B0)
		int16_t pos[3];    // +0x14
		int16_t counter;   // +0x1A flipbook frame
		int16_t vel[3];    // +0x1C -offset / 10
		int16_t life;      // +0x22
	};
	// the prim-player callback's parameter block (a stack block of the main task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame (camera composed in)
		int16_t depth;     // +0x20 the header's +0x18
		int16_t scroll;    // +0x22 texture scroll of objects 0 / 1 (0: none)
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x6D4 && sizeof(SparkNode) == 0x24, "Mini Pulse Cannon nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Mini Pulse Cannon prim block");

	// 0x67A770: the offset v (word +3 = bone) through the caster's bone matrix (world matrices of this tick) -> out (3 int32)
	static void BonePoint(const int16_t *v, int32_t *out)
	{
		const uint8_t *bones = *(uint8_t *const *)*(uint8_t *const *)(Caster() + 0x64);
		const Mat4x3 *bm = (const Mat4x3 *)(bones + v[3] * 0x30 + 0x20);
		GteSetRotMatrixCtrl(bm);
		GteSetTransVectorCtrl(bm);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(out);
	}

	// the beam's direction at main counter c from the muzzle point P (y: P - the target's height, faded out by counters 38..49)
	static void BeamVector(const MainNode *m, int32_t c, const int32_t P[3], int32_t v[3])
	{
		v[0] = (int32_t)m->pos[0] - P[0];
		v[2] = P[2] - (int32_t)m->pos[2];
		const int32_t dy = P[1] - (int32_t)*(const int16_t *)(m->ent + 0x24);
		if ((uint32_t)c < 0xE) v[1] = dy;
		else if ((uint32_t)c < 0x18) v[1] = mul32(dy, 0x18 - c) / 10;
		else v[1] = 0;
	}

	// the beam's z scale at main counter c from the beam's length
	static int32_t BeamScale(int32_t c, int32_t len)
	{
		if ((uint32_t)c > 0x1A) return len >> 1;
		if ((uint32_t)c > 0xE) return len >> 2;
		return 0x1000;
	}

	// the impact frame: the flat basis of P -> the target's position, at it
	static void ImpactFrame(const MainNode *m, const int32_t P[3], Mat4x3 *M)
	{
		int32_t v[3] = { P[0] - (int32_t)m->pos[0], 0, (int32_t)m->pos[2] - P[2] };
		BuildOrthonormalBasis(v, M);
		M->t[0] = m->pos[0];
		M->t[1] = m->pos[1];
		M->t[2] = m->pos[2];
	}

	static void SparkDraw(const SparkNode *s)
	{
		TransformCameraByShadowRotation(s->pos, (int16_t)s->shadow, -(s->shadow >> 2));
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = s->seq;
		*(int16_t *)(h + 4) = s->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag286_mini_pulse_cannon_held.h"
#endif

namespace ff8fx
{
namespace minipulse286
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x67A4E0): one object of a layout. Vertex frame (or blend into the parameter block's morph buffer),
	// rotation from the record's angles, the record's offset through the parameter matrix (flag 0x200: as is), the matrix
	// composed with it (unless flag 0x200 or 0x8000), scale (scale3DMatrix), fade, the texture scroll of objects 0 / 1, drawn by
	// 0x701DD0 (header 0x6C; the fields it does not set keep the Field_Alloc scratch)
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x6C);
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
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x200)
		{
			m.t[0] = off[0];
			m.t[1] = off[1];
			m.t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&arg->m, &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
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
		if (arg->scroll != 0 && (int16_t)r->flags_lo <= 1)
		{
			*(uint16_t *)(h + 0x26) = 0;
			*(uint16_t *)(h + 0x24) = 0;
			*(uint16_t *)(h + 0x2A) = 0x100;
			*(uint16_t *)(h + 0x28) = 0x100;
			*(uint16_t *)(h + 0x30) = 0x100;
			*(uint16_t *)(h + 0x2C) = 0x100;
			if ((int16_t)r->flags_lo == 0)
			{
				*(uint16_t *)(h + 0x2E) = 0;
				*(uint16_t *)(h + 0x32) = 0x80;
			}
			else
			{
				*(uint16_t *)(h + 0x2E) = 0x80;
				*(uint16_t *)(h + 0x32) = 0x80;
			}
			*(int16_t *)(h + 0x22) = arg->scroll;
		}
		else *(int16_t *)(h + 0x22) = 0;
		*(int32_t *)(h + 0x18) = arg->depth;
		*(uint16_t *)(h + 0x20) = 0;
		PacketCursor() = RenderPrimSet(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x6C);
	}

	// ------------------------------------------------------------------
	// Spark (0x67A7C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag286_mini_pulse_cannon_held.inc
		FX_HELD(held_note_spark(s);)
		SparkDraw(s);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->counter++;
		return s->counter < s->life ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Main (0x67A070)
	// ------------------------------------------------------------------
	// counters 25..32: two sparks around P (GTE rotation = the muzzle frame), flying back to P in 10 ticks
	static void SpawnSparks(int32_t c, const int32_t P[3])
	{
		int32_t count = 2;
		SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
		if (!s) return;
		int32_t base = shl32(c + 0x10, 8);
		for (;;)
		{
			s->seq = SEQ_Spark;
			const int32_t angle = (CrtRand() & 0x3F) + base - 0x20;
			const int32_t radius = (CrtRand() & 0x3FF) + 0x200;
			s->pos[0] = (int16_t)(mul32(ComputeSin(angle), radius) >> 12);
			s->pos[1] = (int16_t)(mul32(ComputeCos(angle), radius) >> 12);
			s->pos[2] = (int16_t)((CrtRand() & 0xFF) - 0x80);
			GteLoadV0(s->pos);
			GteMVMVA_RotV0();
			GteStoreIR123(s->pos);
			const int16_t x = s->pos[0], y = s->pos[1], z = s->pos[2];
			s->life = 0xA;
			s->vel[0] = (int16_t)((int32_t)x / -10);
			s->vel[1] = (int16_t)((int32_t)y / -10);
			s->vel[2] = (int16_t)((int32_t)z / -10);
			s->pos[0] = (int16_t)(x + P[0]);
			s->pos[1] = (int16_t)(y + P[1]);
			s->pos[2] = (int16_t)(z + P[2]);
			s->counter = 0;
			s->shadow = 0x4B0;
			base -= 0x800;
			if (--count == 0) break;
			s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
			if (!s) break;
		}
	}

	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const int32_t c = m->counter - 0x19;
		PrimArg arg;
		// RESIDUE (see STACK WORDS): the block's +0x20 / +0x22 / +0x24 as an earlier call left them
		*(uint32_t *)&arg.depth = 0;
		arg.morph = 0;
		Mat4x3 M = {};
		int32_t P[3];
		if ((uint32_t)c < 0x2A)
		{
			BonePoint(MUZZLE, P);
			M.t[0] = P[0];
			M.t[1] = P[1];
			M.t[2] = P[2];
			RotY(*(const int16_t *)(Caster() + 0xE), &M);
			if ((uint32_t)c < 8)
			{
				GteSetRotMatrix(&M);
				SpawnSparks(c, P);
			}
			ComposeAffineTransform(&Camera(), &M, &arg.m);
			arg.morph = TexBase() + MORPH;
			arg.depth = -0x20;
			arg.scroll = 0;
			// 30 fps layer: see mag286_mini_pulse_cannon_held.inc
			FX_HELD(held_note_main(m, PLAY_MUZZLE, &arg);)
			prim::play((prim::Layout *)m->muzzle, PrimCallback, (int)&arg, 0);
			int32_t v[3];
			BeamVector(m, c, P, v);
			const int32_t len = BuildOrthonormalBasis(v, &M);
			const int32_t sv[3] = { 0x1000, 0x1000, BeamScale(c, len) };
			Scale3DMatrix(&M, sv);
			ComposeAffineTransform(&Camera(), &M, &arg.m);
			arg.scroll = (int16_t)((c & 3) << 5);
			// 30 fps layer: see mag286_mini_pulse_cannon_held.inc
			FX_HELD(held_note_main(m, PLAY_BEAM, &arg);)
			prim::play((prim::Layout *)m->beam, PrimCallback, (int)&arg, 0);
		}
		if ((uint32_t)(m->counter - 0x31) < 0x14)
		{
			BonePoint(MUZZLE, P);
			ImpactFrame(m, P, &M);
			ComposeAffineTransform(&Camera(), &M, &arg.m);
			// 30 fps layer: see mag286_mini_pulse_cannon_held.inc
			FX_HELD(held_note_main(m, PLAY_IMPACT, &arg);)
			prim::play((prim::Layout *)m->impact, PrimCallback, (int)&arg, 0);
			M.t[1] = *(const int16_t *)(m->ent + 0x24);
			ComposeAffineTransform(&Camera(), &M, &arg.m);
			arg.morph = TexBase() + MORPH;
			arg.depth = -0x80;
			arg.scroll = 0;
			// 30 fps layer: see mag286_mini_pulse_cannon_held.inc
			FX_HELD(held_note_main(m, PLAY_GROUND, &arg);)
			prim::play((prim::Layout *)m->ground, PrimCallback, (int)&arg, 0);
		}
		if (m->counter == 0x18) BdPlaySE(SOUND_Shot, 0, 0x80);
		if (m->counter == 0x35) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		m->counter++;
		return m->counter < 0x45 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Master (0x679EC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *mn = (MasterNode *)n;
		// 30 fps layer: see mag286_mini_pulse_cannon_held.inc
		FX_HELD(held_note_master();)
		if (mn->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			mn->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			mn->arena = 1;
		}
		if (mn->counter == 1 && !mn->started)
		{
			mn->started = 1;
			InitTaskQueuePool(QMain(), (void *)(TexBase() + POOL_Main), 0x6D4, 1);
			InitTaskQueuePool(QSparks(), (void *)TexBase(), 0x24, 0x20);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&m->counter, 0, 0x1B2);
			m->ent = Entity(Ctx()->actions[0].targets[0]);
			DecodeModelPrimLayout(MODEL_Muzzle, m->muzzle, 0x22C);
			DecodeModelPrimLayout(MODEL_Beam, m->beam, 0x158);
			DecodeModelPrimLayout(MODEL_Impact, m->impact, 0x2A4);
			DecodeModelPrimLayout(MODEL_Ground, m->ground, 0x90);
			GetDefaultEffectPosition(m->ent, m->pos);
		}
		int a0, a1;
		if (mn->started)
		{
			uint8_t *caster = Caster();
			ComputeBonesWorldMatrices(caster + 0x60, caster + 0x40);
			a0 = ExecuteTaskQueue(QMain());
			if (a0 == 0) *(uint16_t *)Caster() &= 0xFFF7;
			BuildBoneMatricesFromPose(Caster() + 0x60);
			a1 = ExecuteTaskQueue(QSparks());
		}
		else a0 = a1 = (int)n;
		if (mn->started && !a0 && !a1) return TASK_END;
		mn->counter++;
		return 0;
	}
}

	void register_mag286_mini_pulse_cannon()
	{
		register_port(minipulse286::ORIG_MasterTask, (void *)minipulse286::MasterTask, "286 MasterTask", 286);
		register_port(minipulse286::ORIG_MainTask, (void *)minipulse286::MainTask, "286 MainTask", 286);
		register_port(minipulse286::ORIG_SparkTask, (void *)minipulse286::SparkTask, "286 SparkTask", 286);
		// 30 fps layer: see mag286_mini_pulse_cannon_held.inc
		FX_HELD(register_mag286_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag286_mini_pulse_cannon_held.inc"
#endif
