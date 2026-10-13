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

// Effect 134: Beam Laser (enemy attack 176 of kernel.bin, Elastoid c0m026; MAG_134_*). No other
// kernel.bin entry uses effect 134 (Elastoid's Reflect Beam, attack 177, is effect 135).
//
// Structure (setup MAG_134_BEAM_LASER 0x6B0070 -> _Init 0x6B00A0, file loader 0x6B0080 = the
// texture file named at 0x12077A8; the setup keeps the CASTER's entity (its flag word bit 3 set,
// the original bit kept at 0x2520E74), starts camera animation 0x1207680 and queues the TIM):
//   MasterTask (0x6B0130) - sets the caster's entity flag bit 3 every tick; alternates the packet
//     arena (magic buffer + 0xDE8 / + 0x10DE8); on counter 1 sets up the pools once (beam 1 x 0x540
//     at + 0x720, glows 0x10 x 0x14 at + 0x5E0, sparks 0x10 x 0x1C at + 0x420, hits 8 x 0x84 at
//     + 0) and, while its action index <= action 0's last_action, starts a beam at the first target
//     of that action (a beam already running on the same entity: counter back to 0, retried next
//     tick; the pool full: no beam). Then the beam, glow, spark and hit queues; ends when all four
//     are empty, the caster's flag bit 3 put back.
//   BeamTask (0x6B03B0) - counters 0..0x2E. Frame: P = the caster's bone 0xD (0x502170 flag 0x800),
//     T = the target's default effect position, basis M of (T - P) in the floor plane (0x50CBA0),
//     R = M with rows 0 / 2 negated.
//       counters 0..0xD: two charge layouts 0x12038DC at P + M (0, +-500 sin a, 500 cos a),
//         a = counter * 0x800 / 14 (0x6B0EB0: camera rotation, depth 0);
//       counters 9..0x21: the muzzle layout 0x1205270 at Q = P + M (0, 0, -500); T lifted (from
//         counter 14 by an ease on the target's size word +0x26); the basis M2 of (T - P) with Q as
//         its translation, composed with the camera: layout 0x12065F0 (depth 0);
//       counters 14..0x21: T at or below the floor is moved onto the floor along P -> T (the hit
//         point at -0x200 kept); the beam = prim model 0x1202214 (0x572200) in M2 x diag(w, w, L),
//         w = sin(e / 2) / 8 (e: cosine ease, then a linear fall), L = 9/8 |T - P| (twice while T.y
//         < the target's word +0x92); fade (counter - 26) * 0x200 from counter 26. Spawns:
//         T on the floor -> a hit layout (R, the hit point); T within the target's upper half ->
//         a glow sprite;
//       counters 0x16..0x2D: the impact layout 0x1204004 at the target's bone 0xF1 (taken at the
//         spawn) in R composed with the camera (depth -0x80); counters 0x16..0x19: 4 sparks each
//         (CRT rand velocities).
//     Sound at counter 2, damage of action 0's first target record at 0x1A.
//   HitTask (0x6B0D30) - its layout 0x1207150 in its frame composed with the camera; ends with it.
//   SparkTask (0x6B0D90) - sprite sequence 0x1202068 through the shadow-rotation camera (0xC00,
//     -0x300), flipbook frame = its counter, then moves by its velocity; 16 ticks.
//   GlowTask (0x6B0E30) - sprite sequence 0x1201EBC (0x1000, -0x400), frame = counter; 16 ticks.
//   PrimCallback (0x6B0AB0, Moomba's 0x6ECCE0 without its 0x40000 variant, on this module's
//     cursor) draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update).
// ENTITY / CAMERA: the master writes the caster entity's flag word (+0x00 bit 3); no task writes an
// entity position (+0x1C..+0x20 only read) or the camera words (the camera animation is the engine's).
// Module globals: 0x2520E68..0x2520EE8 (caster entity 0x2520E68, texture file 0x2520E6C, magic
// buffer base 0x2520E70, caster flag word 0x2520E74, context 0x2520E78, master pool 0x2520E80,
// packet cursor 0x2520E94, the queues 0x2520E98 hits / 0x2520EA8 sparks / 0x2520EB8 glows /
// 0x2520EC8 beam / 0x2520ED8 master).
// NEVER-WRITTEN STACK WORDS: BeamTask loads entry esp - 0x9C (its local +0x34) into ebx on counters
// >= 14 (0x6B0535) before anything writes it; every path that uses ebx afterwards (0x6B0894, counters
// 14..0x21) assigns it first (0x6B059D / 0x6B05F7): dead read. The 4th words of the vectors loaded
// into GTE V0 (0x6B0EB0's offset, the callback's record offset) reach the GTE's VZ0 high half
// (0x1CA8A16) only. The matrices' pad words are never read; the beam's prim header keeps the scratch's
// +0x04 / +0x18 (and +0x0C when it does not fade), as in the game.

#include "mag_common.h"

namespace ff8fx
{
namespace beam134
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2520E68); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2520E70); }        // Magic_TextureOFF (magic buffer)
	inline uint32_t &CasterFlags() { return var<uint32_t>(0x2520E74); }    // the caster's flag word at the setup
	inline CastContext *&Ctx() { return var<CastContext *>(0x2520E78); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2520E94); }
	inline TaskQueue *QHits() { return (TaskQueue *)0x2520E98; }           // pool: magic buffer, 8 x 0x84
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2520EA8; }         // pool: + 0x420, 0x10 x 0x1C
	inline TaskQueue *QGlows() { return (TaskQueue *)0x2520EB8; }          // pool: + 0x5E0, 0x10 x 0x14
	inline TaskQueue *QBeam() { return (TaskQueue *)0x2520EC8; }           // pool: + 0x720, 1 x 0x540

	static const uint32_t ORIG_MasterTask = 0x6B0130;
	static const uint32_t ORIG_BeamTask = 0x6B03B0;
	static const uint32_t ORIG_HitTask = 0x6B0D30;
	static const uint32_t ORIG_SparkTask = 0x6B0D90;
	static const uint32_t ORIG_GlowTask = 0x6B0E30;
	static const uint32_t MORPH = 0xC60;                 // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0xDE8;               // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x10DE8;
	static const uint32_t POOL_Beam = 0x720;
	static const uint32_t MODEL_Charge = 0x12038DC;      // prim-model layouts
	static const uint32_t MODEL_Muzzle = 0x1205270;
	static const uint32_t MODEL_Ray = 0x12065F0;
	static const uint32_t MODEL_Impact = 0x1204004;
	static const uint32_t MODEL_Hit = 0x1207150;
	static const uint32_t PRIM_Beam = 0x1202214;         // prim model (0x572200)
	static const uint32_t SEQ_Spark = 0x1202068;         // sprite sequences
	static const uint32_t SEQ_Glow = 0x1201EBC;
	static const void *const SOUND_Beam = (const void *)0x120767C;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // sub_56C220: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2520E80)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t index;     // +0x0E next action to start a beam for
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct BeamNode // pool of 1 node of 0x540 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t index;     // +0x0E its action
		uint8_t *ent;      // +0x10 target entity (first target of its action)
		int16_t spawn[4];  // +0x14 target bone 0xF1 at the spawn
		uint8_t charge[2][0x58]; // +0x1C / +0x74 charge layouts
		uint8_t muzzle[0x220];   // +0xCC
		uint8_t ray[0xA4];       // +0x2EC
		uint8_t impact[0x1B0];   // +0x390
	};
	struct HitNode // pool of 8 nodes of 0x84 bytes
	{
		TaskNode hdr;
		Mat4x3 frame;      // +0x0C R, translation = the hit point (pad +0x1E never written)
		uint8_t layout[0x58]; // +0x2C
	};
	struct SparkNode // pool of 0x10 nodes of 0x1C bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
		int16_t vel[3];    // +0x14
		int16_t pad1A;
	};
	struct GlowNode // pool of 0x10 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
	};
	// the prim-player callback's parameter block (a stack block of the drawing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame (camera composed in)
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(BeamNode) == 0x540 && sizeof(HitNode) == 0x84, "Beam Laser nodes");
	static_assert(sizeof(SparkNode) == 0x1C && sizeof(GlowNode) == 0x14, "Beam Laser nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Beam Laser prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x6B0AB0, Moomba's 0x6ECCE0 without its 0x40000 variant, on this
	// module's cursor): one object of a layout. Vertex frame (or blend into the parameter block's
	// morph buffer), rotation from the record's angles, the record's offset through the parameter
	// matrix (flag 0x200: as is), the matrix composed with it, scale (flag 0x100: 3x3 product, else
	// scale3DMatrix), fade, drawn by 0x7043B0.
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x58);
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
		int32_t t[3];
		if (r->flags & 0x200)
		{
			t[0] = off[0];
			t[1] = off[1];
			t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
			t[0] = m.t[0];
			t[1] = m.t[1];
			t[2] = m.t[2];
		}
		m.t[0] = (int32_t)((uint32_t)t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				int16_t s[9] = { r->scale[0], 0, 0, 0, r->scale[1], 0, 0, 0, r->scale[2] };
				MulMatrix3(&m, s);
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
		PacketCursor() = RenderPrim2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// 0x6B0EB0 (frame part): the offset v through the basis M, plus P (v is rewritten), then through the
	// camera: camera rotation, translation camera x v + camera t; depth 0
	static void PointFrame(const Mat4x3 *M, const int16_t *P, int16_t *v, PrimArg *arg)
	{
		GteSetRotMatrixCtrl(M);
		GteLoadV0(v);
		GteMVMVA_RotV0();
		GteStoreIR123(v);
		v[0] = (int16_t)(v[0] + P[0]);
		v[1] = (int16_t)(v[1] + P[1]);
		v[2] = (int16_t)(v[2] + P[2]);
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(v);
		GteMVMVA_RotV0();
		int32_t mac[3];
		GteReadMAC123(mac);
		memcpy(&arg->m.m, &Camera().m, sizeof(arg->m.m));
		arg->m.t[0] = (int32_t)((uint32_t)mac[0] + (uint32_t)Camera().t[0]);
		arg->m.t[1] = (int32_t)((uint32_t)mac[1] + (uint32_t)Camera().t[1]);
		arg->m.t[2] = (int32_t)((uint32_t)mac[2] + (uint32_t)Camera().t[2]);
		arg->depth = 0;
		arg->morph = TexBase() + MORPH;
	}

	// the beam's frame: P = the caster's bone 0xD, T = the target's default effect position, M = the
	// basis of (T - P) in the floor plane (0x6B03B0..0x6B0492)
	struct BeamFrame
	{
		int16_t P[4];
		int16_t T[4];
		Mat4x3 M;
		Mat4x3 R;          // M, rows 0 / 2 negated (translation set where used)
	};
	static void BuildFrame(const BeamNode *b, BeamFrame *f)
	{
		GetEffectSpawnPosition(Caster(), 0xD, 0x800, f->P);
		GetDefaultEffectPosition(b->ent, f->T);
		const int32_t v[3] = { (int32_t)f->T[0] - f->P[0], 0, (int32_t)f->P[2] - f->T[2] };
		BuildOrthonormalBasis(v, &f->M);
		for (int k = 0; k < 3; k++)
		{
			f->R.m[0][k] = (int16_t)(0 - f->M.m[0][k]);
			f->R.m[1][k] = f->M.m[1][k];
			f->R.m[2][k] = (int16_t)(0 - f->M.m[2][k]);
		}
	}

	// the charge angle (counters 0..0xD) and its offsets
	static int32_t ChargeAngle(int32_t counter) { return (int32_t)((uint32_t)(counter << 11) / 14u); }
	static void ChargeOffsets(int32_t a, int16_t *s, int16_t *c)
	{
		*s = (int16_t)((ComputeSin(a) * 500) >> 12);
		*c = (int16_t)((ComputeCos(a) * 500) >> 12);
	}

	// counters 9..0x21: the target lift, the beam's width angle and its fade (0x6B0575..0x6B060E);
	// e = counter - 14 (unsigned compares)
	struct Ease
	{
		int16_t lift;      // added to T.y
		int32_t width;     // ebx (counters 14..0x21)
		int32_t fade;
	};
	static Ease BeamEase(int32_t counter, int16_t size)
	{
		Ease r;
		const uint32_t e = (uint32_t)(counter - 14);
		if (e < 0xC)
		{
			r.width = 0x1000 - ComputeCos((int32_t)((e << 10) / 12u));
			r.lift = (int16_t)((int32_t)size * 4 - (mul32(size, r.width) >> 9));
			r.fade = 0;
		}
		else if (e < 0x14)
		{
			r.fade = (int32_t)((e - 0xC) << 9);
			r.lift = (int16_t)((int32_t)size * 2 - (shl32(mul32((int32_t)e + 0x34, size), 2) >> 6));
			r.width = 0x280 - (int32_t)(e << 5);
		}
		else
		{
			r.lift = (int16_t)(size * 2);
			r.fade = 0;
			r.width = 0;
		}
		return r;
	}

	// counters 14..0x21: T at or below the floor (y >= 0) onto the floor along P -> T, the hit point
	// at -0x200 (0x6B06CE..0x6B0766)
	static void FloorHit(const int16_t *P, int16_t *T, int16_t *hit)
	{
		const int32_t dy = (int32_t)T[1] - P[1];
		const int32_t ax = mul32((int32_t)P[0] - T[0], P[1]);
		const int32_t az = mul32((int32_t)P[2] - T[2], P[1]);
		const int32_t dh = dy - 0x200;
		hit[0] = (int16_t)(ax / dh + P[0]);
		hit[1] = 0;
		hit[2] = (int16_t)(az / dh + P[2]);
		T[0] = (int16_t)(ax / dy + P[0]);
		T[2] = (int16_t)(az / dy + P[2]);
		T[1] = 0;
	}

	// the beam prim model (0x6B07A9..0x6B095D): the basis of (T - P) scaled by (w, w, L), composed with
	// the camera (M2's translation = Q); the hit kind (0 none, 1 hit layout, 2 glow) for the caller
	struct BeamDraw
	{
		Mat4x3 M2;
		int32_t len;       // 9/8 |T - P| (x 2 while T.y < the target's word +0x92)
		int kind;
	};
	static void BeamShape(const BeamNode *b, const int16_t *P, const int16_t *T, BeamDraw *d)
	{
		const int32_t v[3] = { (int32_t)T[0] - P[0], (int32_t)P[1] - T[1], (int32_t)P[2] - T[2] };
		int32_t len = BuildOrthonormalBasis(v, &d->M2);
		len += len >> 3;
		d->kind = 0;
		if (T[1] < *(const int16_t *)(b->ent + 0x92)) len <<= 1;
		else if (T[1] >= 0) d->kind = 1;
		else
		{
			const int32_t h = *(const int16_t *)(b->ent + 0x26);
			const int32_t up = -(int32_t)T[1];
			if (up >= (h >> 1) && up <= h) d->kind = 2;
		}
		d->len = len;
	}
	static int16_t BeamWidth(int32_t angle) { return (int16_t)(ComputeSin(angle >> 1) >> 3); }
	static void BeamRender(Mat4x3 *M2, int32_t len, int16_t w, int32_t fade)
	{
		Mat4x3 s;
		Memset32(&s, 0, 8);
		s.m[0][0] = w;
		s.m[1][1] = w;
		s.m[2][2] = (int16_t)len;
		MulMatrix3(M2, &s);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = PRIM_Beam;
		if (fade == 0) *(uint32_t *)(h + 0x1C) = 0x30;
		else
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) = 0xF0;
		}
		ComposeAffineTransform(&Camera(), M2, M2);
		GteSetRotMatrix(M2);
		GteSetTransVector(M2);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the sprites (0x6B0D93..0x6B0DF0 / 0x6B0E32..0x6B0E8B)
	static void SpriteDraw(const int16_t *pos, int16_t frame, uint32_t seq, int32_t a, int32_t b)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(pos, a, b);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag134_beam_laser_held.h"
#endif

namespace ff8fx
{
namespace beam134
{
	// ------------------------------------------------------------------
	// Glow (0x6B0E30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl GlowTask(TaskNode *n)
	{
		GlowNode *g = (GlowNode *)n;
		// 30 fps layer: see mag134_beam_laser_held.inc
		FX_HELD(held_note_sprite(SPR_GLOW, n, g->pos, g->counter);)
		SpriteDraw(g->pos, g->counter, SEQ_Glow, 0x1000, -0x400);
		g->counter++;
		return g->counter < 0x10 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Spark (0x6B0D90)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag134_beam_laser_held.inc
		FX_HELD(held_note_sprite(SPR_SPARK, n, s->pos, s->counter);)
		SpriteDraw(s->pos, s->counter, SEQ_Spark, 0xC00, -0x300);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->counter++;
		return s->counter < 0x10 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Hit (0x6B0D30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *h = (HitNode *)n;
		PrimArg arg;
		ComposeAffineTransform(&Camera(), &h->frame, &arg.m);
		arg.depth = 0;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag134_beam_laser_held.inc
		FX_HELD(held_note_play(PLAY_HIT, n, (prim::Layout *)h->layout, nullptr);)
		return prim::play((prim::Layout *)h->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Beam (0x6B03B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl BeamTask(TaskNode *n)
	{
		BeamNode *b = (BeamNode *)n;
		BeamFrame f;
		BuildFrame(b, &f);
		const int32_t c = b->counter;
		int16_t v[4] = {};
		PrimArg arg;
		if ((uint32_t)c < 0xE)
		{
			int16_t s, co;
			ChargeOffsets(ChargeAngle(c), &s, &co);
			for (int k = 0; k < 2; k++)
			{
				v[0] = 0;
				v[1] = k ? (int16_t)(0 - s) : s;
				v[2] = co;
				PointFrame(&f.M, f.P, v, &arg);
				// 30 fps layer: see mag134_beam_laser_held.inc
				FX_HELD(held_note_play(PLAY_CHARGE0 + k, n, (prim::Layout *)b->charge[k], &f);)
				prim::play((prim::Layout *)b->charge[k], PrimCallback, (int)&arg, 0);
			}
		}
		if ((uint32_t)(c - 9) < 0x19)
		{
			v[0] = 0;
			v[1] = 0;
			v[2] = -500;
			PointFrame(&f.M, f.P, v, &arg);
			FX_HELD(held_note_play(PLAY_MUZZLE, n, (prim::Layout *)b->muzzle, &f);)
			prim::play((prim::Layout *)b->muzzle, PrimCallback, (int)&arg, 0);
			const Ease ez = BeamEase(c, *(const int16_t *)(b->ent + 0x26));
			int16_t T[4] = { f.T[0], (int16_t)(f.T[1] + ez.lift), f.T[2], f.T[3] };
			BeamDraw d;
			{
				const int32_t u[3] = { (int32_t)T[0] - f.P[0], (int32_t)f.P[1] - T[1], (int32_t)f.P[2] - T[2] };
				BuildOrthonormalBasis(u, &d.M2);
			}
			d.M2.t[0] = v[0];
			d.M2.t[1] = v[1];
			d.M2.t[2] = v[2];
			PrimArg ray;
			ComposeAffineTransform(&Camera(), &d.M2, &ray.m);
			ray.depth = 0;
			ray.morph = TexBase() + MORPH;
			FX_HELD(held_note_play(PLAY_RAY, n, (prim::Layout *)b->ray, &f);)
			prim::play((prim::Layout *)b->ray, PrimCallback, (int)&ray, 0);
			if ((uint32_t)(c - 14) < 0x14)
			{
				int16_t hit[3] = {};
				if (T[1] >= 0) FloorHit(f.P, T, hit);
				BeamShape(b, f.P, T, &d);
				if (d.kind == 1)
				{
					HitNode *h = (HitNode *)AddTaskToQueue(QHits(), ORIG_HitTask);
					if (h)
					{
						memcpy(&h->frame.m, &f.R.m, sizeof(h->frame.m));
						h->frame.t[0] = hit[0];
						h->frame.t[1] = hit[1];
						h->frame.t[2] = hit[2];
						DecodeModelPrimLayout(MODEL_Hit, h->layout, 0x58);
					}
				}
				else if (d.kind == 2)
				{
					GlowNode *g = (GlowNode *)AddTaskToQueue(QGlows(), ORIG_GlowTask);
					if (g)
					{
						g->pos[0] = T[0];
						g->pos[1] = T[1];
						g->pos[2] = T[2];
						g->counter = 0;
					}
				}
				FX_HELD(held_note_beam(n);)
				BeamRender(&d.M2, d.len, BeamWidth(ez.width), ez.fade);
			}
		}
		if ((uint32_t)(c - 0x16) < 0x18)
		{
			if ((uint32_t)(c - 0x16) < 4)
			{
				for (int k = 0; k < 4; k++)
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
					if (!s) continue;
					memcpy(s->pos, b->spawn, 4);
					*(uint32_t *)&s->pos[2] = *(const uint32_t *)&b->spawn[2];
					s->counter = 0;
					s->vel[0] = (int16_t)((CrtRand() & 0xFF) - 0x80);
					s->vel[1] = (int16_t)((CrtRand() & 0xFF) - 0x80);
					s->vel[2] = (int16_t)((CrtRand() & 0xFF) - 0x80);
				}
			}
			f.R.t[0] = b->spawn[0];
			f.R.t[1] = b->spawn[1];
			f.R.t[2] = b->spawn[2];
			ComposeAffineTransform(&Camera(), &f.R, &arg.m);
			arg.depth = -0x80;
			arg.morph = TexBase() + MORPH;
			FX_HELD(held_note_play(PLAY_IMPACT, n, (prim::Layout *)b->impact, &f);)
			prim::play((prim::Layout *)b->impact, PrimCallback, (int)&arg, 0);
		}
		if (b->counter == 2) BdPlaySE(SOUND_Beam, 0, 0x80);
		if (b->counter == 0x1A) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (b->counter >= 0x2E) return TASK_END;
		b->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x6B0130)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag134_beam_laser_held.inc
		FX_HELD(held_note_master();)
		if (m->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			m->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			m->arena = 1;
		}
		*Caster() |= 8;
		if (m->counter == 1)
		{
			if (!m->started)
			{
				m->started = 1;
				InitTaskQueuePool(QBeam(), (void *)(TexBase() + POOL_Beam), 0x540, 1);
				InitTaskQueuePool(QGlows(), (void *)(TexBase() + 0x5E0), 0x14, 0x10);
				InitTaskQueuePool(QSparks(), (void *)(TexBase() + 0x420), 0x1C, 0x10);
				InitTaskQueuePool(QHits(), (void *)TexBase(), 0x84, 8);
			}
			if (m->index <= Ctx()->actions[0].last_action)
			{
				uint8_t *ent = Entity(Ctx()->actions[m->index].targets[0]);
				const BeamNode *live = (const BeamNode *)(TexBase() + POOL_Beam);
				if ((live->hdr.flags & 1) && live->ent == ent) m->counter = 0;
				else
				{
					BeamNode *b = (BeamNode *)AddTaskToQueue(QBeam(), ORIG_BeamTask);
					if (b)
					{
						Memset32(&b->counter, 0, 0x14D);
						b->index = m->index;
						b->ent = ent;
						GetEffectSpawnPosition(ent, 0xF1, 0, b->spawn);
						m->index++;
						DecodeModelPrimLayout(MODEL_Charge, b->charge[0], 0x58);
						DecodeModelPrimLayout(MODEL_Charge, b->charge[1], 0x58);
						DecodeModelPrimLayout(MODEL_Muzzle, b->muzzle, 0x220);
						DecodeModelPrimLayout(MODEL_Ray, b->ray, 0xA4);
						DecodeModelPrimLayout(MODEL_Impact, b->impact, 0x1B0);
					}
				}
			}
		}
		int a0, a1, a2, a3;
		if (m->started)
		{
			a0 = ExecuteTaskQueue(QBeam());
			a1 = ExecuteTaskQueue(QGlows());
			a2 = ExecuteTaskQueue(QSparks());
			a3 = ExecuteTaskQueue(QHits());
		}
		else a0 = a1 = a2 = a3 = (int)n;
		if (m->started && a0 == 0 && a1 == 0 && a2 == 0 && a3 == 0)
		{
			uint16_t &w = *(uint16_t *)Caster();
			w = (uint16_t)(w ^ ((w ^ CasterFlags()) & 8));
			return TASK_END;
		}
		m->counter++;
		return 0;
	}
}

	void register_mag134_beam_laser()
	{
		register_port(beam134::ORIG_MasterTask, (void *)beam134::MasterTask, "134 MasterTask", 134);
		register_port(beam134::ORIG_BeamTask, (void *)beam134::BeamTask, "134 BeamTask", 134);
		register_port(beam134::ORIG_HitTask, (void *)beam134::HitTask, "134 HitTask", 134);
		register_port(beam134::ORIG_SparkTask, (void *)beam134::SparkTask, "134 SparkTask", 134);
		register_port(beam134::ORIG_GlowTask, (void *)beam134::GlowTask, "134 GlowTask", 134);
		// 30 fps layer: see mag134_beam_laser_held.inc
		FX_HELD(register_mag134_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag134_beam_laser_held.inc"
#endif
