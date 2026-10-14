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

// Effect 135: Reflect Beam (enemy attack 177 of kernel.bin, Elastoid c0m026; MAG_135_*). No other
// kernel.bin entry uses effect 135.
//
// Structure (setup MAG_135_REFLECT_BEAM 0x6AEEA0 -> _Init 0x6AEED0, file loader 0x6AEEB0 = the
// texture file named at 0x1201EB0; the setup keeps the CASTER's entity (0x2520DD0), starts camera
// animation 0x1201D8C and queues the TIM):
//   MasterTask (0x6AEF50) - alternates the packet arena (magic buffer + 0x2F64 / + 0x1AF64); on
//     counter 1 sets up the pools once (sweep 1 x 0x2E0 at + 0x2A64, shots 8 x 0x26C at + 0x1704,
//     reflections 3 x 0x32C at + 0xD80, sprites 0x60 x 0x24 at + 0) and starts the sweep: the
//     targets of action 0 (entities, their order in the action), the centre of their default
//     effect positions, the targets sorted by entity x (+0x1C) descending, layout 0x11FDAA4. Then the
//     sweep queue, the caster's world bone matrices (0x5095B0), the shot queue, the caster's bone
//     matrices rebuilt from its pose (0x508C90), the reflection and sprite queues; ends when all
//     four are empty.
//   SweepTask (0x6AF230) - counters 0..0x39. Screen flash from counter 15 (rise over 4 ticks, hold,
//     fall from counter 45; off at the end). Counters 15..0x22: P = the caster's bone 0x1F (0x502170
//     flag 0x1000); three beam segments (prim model 0x11FA580, oldest first, the two trailing ones
//     fading (i + 4) x 0x200), segment i at s = counter - 15 - i in 0..9: its end X runs from the
//     centre's x + 2500 down by 5000 x s / 10, the basis of (X, centre y, z) - P at P, scaled by
//     diag(w, w, L) (sine of t / 4 + 0x200, t = s x 0x1000 / 10: w = (sine + 0x800) / 2, L = sine x
//     |X - P| / 0x800, through the module matrix 0x2520E48); the leading segment starts a reflection
//     on the next sorted target once X < its x (one per tick). Then layout +0x28 at P (depth -0x80).
//     Sound at counter 0; a shot at counters 0, 4, .. 16 (from the caster's bones 29 / 30 / 32 in turn).
//   ShotTask (0x6AFD20) - counters 0..0xC: Q = the caster's world bone (0x1201EA8 table) through
//     (0, 0, -0x1000); layout +0x1C at Q; the bone basis (rows normalised, 0x56BDE0) at Q; E = Q +
//     basis x (0, 0, 4a), a = (counter + 1) x 0x1000 / 13; E at or above the floor (y <= 0): E moved
//     onto the floor along the line Q -> E (kept as the hit point, the hit flag set), length 17/16
//     |E - Q|, else 4a; the beam
//     model in the basis scaled by diag(w, w, length), w = sine(a / 2) / 8. While the hit flag is set
//     layout +0x214 plays at the hit point (flag cleared when it ends); ends after counter 0xC
//     without a hit flag.
//   ReflectTask (0x6AF900) - at the target's bone 0xF1 (y + 0x80): counters 0..0xF: a glint on every
//     even counter (random offset); counters 0x10..: layout +0x1C (depth -0x80), a burst every 4th
//     counter up to 0x1F (random offset), 4 sparks per tick at counters 0x10..0x13 (random
//     velocities, drag 1/16); damage of the target's records of action 0 at counter 0x19
//     (0x6AFCB0); ends with the layout.
//   BurstTask (0x6AFB00) / GlintTask (0x6AFB80) / SparkTask (0x6AFC00) - sprite sequences 0x11FA3C0
//     (0x1770, -0x400, 16 frames) / 0x11FA208 (0x800, -0x400, 7 frames) / 0x11FA2CC (0x800, -0x200,
//     8 frames; then velocity += drag, position += velocity) through the shadow-rotation camera.
//   PrimCallback (0x6AF610, Petrify Stare's 0x6B9770 with Silence's flag 0x100 product) draws every
//     layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update).
// ENTITY / CAMERA: the master rebuilds the caster entity's bone matrices (0x5095B0 world, 0x508C90
// from the pose); no task writes an entity position (+0x1C..+0x20 only read) or the camera words (the
// camera animation is the engine's). The screen flash level is the engine's (0x5713E0).
// Module globals: 0x2520DD0..0x2520E5A (caster entity 0x2520DD0, texture file 0x2520DD4, magic
// buffer base 0x2520DD8, context 0x2520DDC, master pool 0x2520DE0, packet cursor 0x2520DF4, the
// queues 0x2520DF8 sprites / 0x2520E08 reflections / 0x2520E18 shots / 0x2520E28 sweep / 0x2520E38
// master, the beam scale matrix 0x2520E48).
// NEVER-WRITTEN STACK WORDS: ShotTask's end point word +6 (entry esp - 0x42) is copied into the hit
// point's 4th word (node +0x1A, 0x6AFF04) on a floor hit: the high half of the caster entity + 0x40
// (see ShotResidue); that word then only reaches the GTE V0 pad. The 4th words of the
// vectors loaded into GTE V0 (the shot's (0, 0, -0x1000) and end point, the sweep's P in the layout
// block, the callback's record offset) reach the GTE's VZ0 high half (0x1CA8A16) only. The matrices'
// pad words are never read.

#include "mag_common.h"

namespace ff8fx
{
namespace reflect135
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2520DD0); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2520DD8); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2520DDC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2520DF4); }
	inline Mat4x3 &Scale() { return var<Mat4x3>(0x2520E48); }              // beam scale (diagonal only written)
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2520DF8; }        // pool: magic buffer, 0x60 x 0x24
	inline TaskQueue *QReflects() { return (TaskQueue *)0x2520E08; }       // pool: + 0xD80, 3 x 0x32C
	inline TaskQueue *QShots() { return (TaskQueue *)0x2520E18; }          // pool: + 0x1704, 8 x 0x26C
	inline TaskQueue *QSweep() { return (TaskQueue *)0x2520E28; }          // pool: + 0x2A64, 1 x 0x2E0

	static const uint32_t ORIG_MasterTask = 0x6AEF50;
	static const uint32_t ORIG_SweepTask = 0x6AF230;
	static const uint32_t ORIG_ReflectTask = 0x6AF900;
	static const uint32_t ORIG_BurstTask = 0x6AFB00;
	static const uint32_t ORIG_GlintTask = 0x6AFB80;
	static const uint32_t ORIG_SparkTask = 0x6AFC00;
	static const uint32_t ORIG_ShotTask = 0x6AFD20;
	static const uint32_t MORPH = 0x2D44;                // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x2F64;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x1AF64;
	static const uint32_t MODEL_Sweep = 0x11FDAA4;       // prim-model layouts
	static const uint32_t MODEL_Reflect = 0x120031C;
	static const uint32_t MODEL_Shot = 0x11FBF48;
	static const uint32_t MODEL_ShotHit = 0x1201644;
	static const uint32_t PRIM_Beam = 0x11FA580;         // prim model (0x572200)
	static const uint32_t SEQ_Burst = 0x11FA3C0;         // sprite sequences
	static const uint32_t SEQ_Glint = 0x11FA208;
	static const uint32_t SEQ_Spark = 0x11FA2CC;
	static const uint32_t SHOT_Bones = 0x1201EA8;        // 3 caster bones (29, 30, 32)
	static const void *const SOUND_Beam = (const void *)0x1201D88;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // sub_56C220: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void NormalizeVector16(const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const int16_t *, int16_t *)>(0x56BDE0)(in, out); } // sub_56BDE0
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2520DE0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct SweepNode // pool of 1 node of 0x2E0 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t count;     // +0x0E targets of action 0
		uint8_t order[3];  // +0x10 the sorted targets' indices in the action
		uint8_t next;      // +0x13 next sorted target to reflect on
		uint8_t *ents[3];  // +0x14 sorted target entities
		int16_t centre[3]; // +0x20 centre of their default effect positions
		int16_t pad26;
		uint8_t layout[0x2B8]; // +0x28
	};
	struct ShotNode // pool of 8 nodes of 0x26C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t hit;       // +0x0E hit layout playing
		const int16_t *bone; // +0x10 caster bone (entry of SHOT_Bones)
		int16_t hitpos[4]; // +0x14 floor hit point (+0x1A: a stack word, see ShotResidue)
		uint8_t layout[0x1F8]; // +0x1C
		uint8_t hitlayout[0x58]; // +0x214
	};
	struct ReflectNode // pool of 3 nodes of 0x32C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t order;     // +0x0E the target's index in the action
		uint8_t *ent;      // +0x10 target entity
		int16_t pos[4];    // +0x14 its bone 0xF1, y + 0x80 (+0x1A never written)
		uint8_t layout[0x310]; // +0x1C
	};
	struct SpriteNode // pool of 0x60 nodes of 0x24 bytes (bursts, glints, sparks)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
		int16_t vel[3];    // +0x14 sparks
		int16_t pad1A;
		int16_t drag[3];   // +0x1C sparks
		int16_t pad22;
	};
	// the prim-player callback's parameter block (a stack block of the playing task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 anchor x, y, z (+ a word only loaded into the GTE V0 pad)
		int32_t depth;     // +0x08 prim header +0x18
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(SweepNode) == 0x2E0 && sizeof(ShotNode) == 0x26C, "Reflect Beam nodes");
	static_assert(sizeof(ReflectNode) == 0x32C && sizeof(SpriteNode) == 0x24, "Reflect Beam nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Reflect Beam prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x6AF610, Petrify Stare's 0x6B9770 with Silence's flag 0x100 product):
	// one object of a layout. Vertex frame (or blend into the parameter block's morph buffer),
	// rotation from the record's angles (flag 0x40000: 0x7015B0), flag 0x200 = the offset stays in
	// screen axes (the anchor through the camera, the offset added unrotated, the rotation not
	// composed with the camera), else anchor + offset through the camera; scale (flag 0x100: 3x3
	// product, else scale3DMatrix), fade, drawn by 0x7043B0.
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
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x200)
		{
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
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
			if (r->flags & 0x100)
			{
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MulMatrix3(&m, &d);
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

	static void Play(void *layout, const int16_t *pos4, int32_t depth)
	{
		PrimArg arg;
		memcpy(arg.pos, pos4, 8);
		arg.depth = depth;
		arg.morph = TexBase() + MORPH;
		prim::play((prim::Layout *)layout, PrimCallback, (int)&arg, 0);
	}

	// the beam model (0x11FA580) in the basis M scaled by diag(w, w, L) through the module matrix
	// 0x2520E48, composed with the camera, on the header h
	static void BeamRender(uint8_t *h, Mat4x3 *M, int16_t w, int16_t L)
	{
		Scale().m[1][1] = w;
		Scale().m[0][0] = w;
		Scale().m[2][2] = L;
		MulMatrix3(M, &Scale());
		*(uint32_t *)h = PRIM_Beam;
		ComposeAffineTransform(&Camera(), M, M);
		GteSetRotMatrix(M);
		GteSetTransVector(M);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
	}

	// ------------------------------------------------------------------
	// Sweep beam segments (0x6AF2FF..0x6AF4FF)
	// ------------------------------------------------------------------
	// segment state at s = counter - 15 - i (0..9): t, its end X
	static int32_t SegT(int32_t s) { return (int32_t)(((uint32_t)s << 12) / 10u); }
	static int16_t SegX(const SweepNode *w, int32_t t) { return (int16_t)(w->centre[0] - (int16_t)(mul32(t, 5000) >> 12) + 0x9C4); }
	static int32_t SegSine(int32_t t) { return ComputeSin((t >> 2) + 0x200); }
	// one segment on the shared header h: the basis of (X - P0, P1 - cy, P2 - cz) at P, scale from the
	// sine (w = (sine + 0x800) / 2, L = sine x |v| / 0x800)
	static void SegmentRender(uint8_t *h, const SweepNode *w, const int16_t *P, int16_t X, int32_t sine)
	{
		const int32_t v[3] = { (int32_t)X - P[0], (int32_t)P[1] - w->centre[1], (int32_t)P[2] - w->centre[2] };
		Mat4x3 M;
		const int32_t len = BuildOrthonormalBasis(v, &M);
		M.t[0] = P[0];
		M.t[1] = P[1];
		M.t[2] = P[2];
		BeamRender(h, &M, (int16_t)((sine + 0x800) >> 1), (int16_t)(mul32(sine, len) >> 11));
	}

	// the screen flash level of the sweep counter c (>= 15)
	static int32_t FlashLevel(int32_t c)
	{
		if (c < 0x13) return (int32_t)mul32(c - 15, 2200) / 4;
		if (c < 0x2D) return 0x898;
		return (int32_t)mul32(0x39 - c, 2200) / 12;
	}

	// ShotTask's end point word +6 (entry esp - 0x42, never written), copied into the hit point's 4th word
	// (node +0x1A, 0x6AFF04): the master's BS_ComputeBonesWorldMatrices (0x5095B0) runs just before the shot
	// queue, one stack level up; its last ComposeAffineTransform (0x56C2F0) pushes its first argument
	// (the caster entity + 0x40) for 0x56C600 (0x56C312) at exactly that word's dword: the word is the high
	// half of that pointer (the caster has bones, so 0x56C2F0 always runs)
	static int16_t ShotResidue()
	{
		return (int16_t)(((uint32_t)Caster() + 0x40) >> 16);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag135_reflect_beam_held.h"
#endif

namespace ff8fx
{
namespace reflect135
{
	// ------------------------------------------------------------------
	// Sprites (0x6AFB00 burst, 0x6AFB80 glint, 0x6AFC00 spark)
	// ------------------------------------------------------------------
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

	static uint32_t __cdecl BurstTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag135_reflect_beam_held.inc
		FX_HELD(held_note_sprite(SPR_BURST, n);)
		SpriteDraw(s->pos, s->counter, SEQ_Burst, 0x1770, -0x400);
		s->counter++;
		return s->counter < 0x10 ? 0 : TASK_END;
	}

	static uint32_t __cdecl GlintTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag135_reflect_beam_held.inc
		FX_HELD(held_note_sprite(SPR_GLINT, n);)
		SpriteDraw(s->pos, s->counter, SEQ_Glint, 0x800, -0x400);
		s->counter++;
		return s->counter < 7 ? 0 : TASK_END;
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag135_reflect_beam_held.inc
		FX_HELD(held_note_sprite(SPR_SPARK, n);)
		SpriteDraw(s->pos, s->counter, SEQ_Spark, 0x800, -0x200);
		s->vel[0] = (int16_t)(s->vel[0] + s->drag[0]);
		s->vel[1] = (int16_t)(s->vel[1] + s->drag[1]);
		s->vel[2] = (int16_t)(s->vel[2] + s->drag[2]);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->counter++;
		return s->counter < 8 ? 0 : TASK_END;
	}

	// sub_6AFCB0 (as Eerie Sound Wave's 0x6B4B90): the damage of every target record of action 0
	// that is this entity
	static void DamageEntity(uint8_t *entity)
	{
		const int32_t slot = (int32_t)((uint32_t)entity - 0x1D972C0u) / 0x9C;
		for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
		{
			uint8_t *t = Ctx()->actions[0].targets + i * TARGET_STRIDE;
			if ((int32_t)t[0] == slot) ApplyActionResultToTarget(t);
		}
	}

	// ------------------------------------------------------------------
	// Reflection (0x6AF900)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ReflectTask(TaskNode *n)
	{
		ReflectNode *r = (ReflectNode *)n;
		if (r->counter < 0x10)
		{
			if (!(r->counter & 1))
			{
				SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_GlintTask);
				if (s)
				{
					s->counter = 0;
					s->pos[0] = (int16_t)((CrtRand() & 0xFF) + r->pos[0] - 0x80);
					s->pos[1] = (int16_t)((CrtRand() & 0x7F) + r->pos[1] - 0x40);
					s->pos[2] = (int16_t)((CrtRand() & 0xFF) + r->pos[2] - 0x80);
				}
			}
		}
		else
		{
			if (r->counter < 0x20 && !(r->counter & 3))
			{
				SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_BurstTask);
				if (s)
				{
					s->counter = 0;
					s->pos[0] = (int16_t)((CrtRand() & 0x1FF) + r->pos[0] - 0x100);
					s->pos[1] = (int16_t)((CrtRand() & 0x1FF) + r->pos[1] - 0x100);
					s->pos[2] = (int16_t)((CrtRand() & 0x1FF) + r->pos[2] - 0x100);
				}
			}
			if (r->counter < 0x14)
			{
				for (int k = 0; k < 4; k++)
				{
					SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SparkTask);
					if (!s) continue;
					memcpy(s->pos, r->pos, 4);
					memcpy(&s->pos[2], &r->pos[2], 4);
					s->counter = 0;
					s->vel[0] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
					s->vel[1] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
					const int32_t z = (CrtRand() & 0x1FF) - 0x100;
					s->drag[0] = (int16_t)((0 - (int32_t)s->vel[0]) >> 4);
					s->vel[2] = (int16_t)z;
					s->drag[1] = (int16_t)((0 - (int32_t)s->vel[1]) >> 4);
					s->drag[2] = (int16_t)((0 - (int32_t)(int16_t)z) >> 4);
				}
			}
			// 30 fps layer: see mag135_reflect_beam_held.inc
			FX_HELD(held_note_play(PLAY_REFLECT, n, (prim::Layout *)r->layout);)
			PrimArg arg;
			memcpy(arg.pos, r->pos, 8);
			arg.depth = -0x80;
			arg.morph = TexBase() + MORPH;
			if (!prim::play((prim::Layout *)r->layout, PrimCallback, (int)&arg, 0)) return TASK_END;
		}
		if (r->counter == 0x19) DamageEntity(r->ent);
		r->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Shot (0x6AFD20)
	// ------------------------------------------------------------------
	// the caster's bone matrix of the shot (world matrices while the shot queue runs)
	static const Mat4x3 *ShotBone(const ShotNode *s)
	{
		return (const Mat4x3 *)(**(uint8_t ***)(Caster() + 0x64) + (int32_t)*s->bone * 0x30 + 0x20);
	}
	static int32_t ShotAngle(int32_t counter) { return ((counter + 1) << 12) / 13; }
	// Q = the bone through (0, 0, -0x1000); the basis = the bone's rows normalised, at Q
	static void ShotFrame(const Mat4x3 *bone, int16_t *Q, Mat4x3 *m)
	{
		memcpy(m->m, bone->m, sizeof(m->m));
		GteSetRotMatrixCtrl(m);
		GteSetTransVectorCtrl(bone);
		Q[0] = 0;
		Q[1] = 0;
		Q[2] = -0x1000;
		GteLoadV0(Q);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(Q);
	}
	static void ShotBasis(Mat4x3 *m, const int16_t *Q)
	{
		m->t[0] = Q[0];
		m->t[1] = Q[1];
		m->t[2] = Q[2];
		NormalizeVector16(m->m[0], m->m[0]);
		NormalizeVector16(m->m[1], m->m[1]);
		NormalizeVector16(m->m[2], m->m[2]);
	}
	// E = the basis through (0, 0, 4a)
	static void ShotPoint(Mat4x3 *m, int32_t a, int16_t *E)
	{
		GteSetRotMatrixCtrl(m);
		GteSetTransVectorCtrl(m);
		E[0] = 0;
		E[1] = 0;
		E[2] = (int16_t)(a * 4);
		GteLoadV0(E);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(E);
	}
	// E at or above the floor (y <= 0): E moved onto the floor along the line Q -> E (the hit point),
	// the length 17/16 |E - Q| (returns true); below it: 4a
	static bool ShotProject(const int16_t *Q, int32_t a, int16_t *E, int32_t *len)
	{
		if (E[1] > 0)
		{
			*len = a * 4;
			return false;
		}
		const int32_t q1 = Q[1];
		const int32_t dy = (int32_t)E[1] - q1;
		E[1] = 0;
		E[0] = (int16_t)(mul32((int32_t)Q[0] - E[0], q1) / dy + Q[0]);
		E[2] = (int16_t)(mul32((int32_t)Q[2] - E[2], q1) / dy + Q[2]);
		const int32_t d[3] = { (int32_t)E[0] - Q[0], 0 - q1, (int32_t)E[2] - Q[2] };
		GteLoadIR123_32b(d);
		GteSQR();
		int32_t mac[3];
		GteReadMAC123(mac);
		const int32_t r = Sqrt((int32_t)((uint32_t)mac[1] + (uint32_t)mac[2] + (uint32_t)mac[0]));
		*len = r + (r >> 4);
		return true;
	}
	static void ShotBeam(Mat4x3 *m, int16_t w, int16_t L)
	{
		Scale().m[1][1] = w;
		Scale().m[0][0] = w;
		Scale().m[2][2] = L;
		MulMatrix3(m, &Scale());
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = PRIM_Beam;
		*(uint32_t *)(h + 0x1C) = 0x30;
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
	static int16_t ShotWidth(int32_t a) { return (int16_t)(ComputeSin(a >> 1) >> 3); }

	static uint32_t __cdecl ShotTask(TaskNode *n)
	{
		ShotNode *s = (ShotNode *)n;
		const Mat4x3 *bone = ShotBone(s);
		if (s->counter < 0xD)
		{
			Mat4x3 m;
			int16_t Q[4] = {}; // (word +6 never written: only loaded into the GTE V0 pad)
			ShotFrame(bone, Q, &m);
			// 30 fps layer: see mag135_reflect_beam_held.inc
			FX_HELD(held_note_play(PLAY_SHOT, n, (prim::Layout *)s->layout);)
			Play(s->layout, Q, -0x80);
			ShotBasis(&m, Q);
			const int32_t a = ShotAngle(s->counter);
			int16_t E[4] = {};
			int32_t len;
			ShotPoint(&m, a, E);
			if (ShotProject(Q, a, E, &len))
			{
				s->hit = 1;
				memcpy(s->hitpos, E, 6);
				s->hitpos[3] = ShotResidue();
			}
			FX_HELD(held_note_shot_beam(n);)
			ShotBeam(&m, ShotWidth(a), (int16_t)len);
		}
		else if (s->hit == 0) return TASK_END;
		if (s->hit != 0)
		{
			FX_HELD(held_note_play(PLAY_SHOTHIT, n, (prim::Layout *)s->hitlayout);)
			PrimArg arg;
			memcpy(arg.pos, s->hitpos, 8);
			arg.depth = 0;
			arg.morph = TexBase() + MORPH;
			if (!prim::play((prim::Layout *)s->hitlayout, PrimCallback, (int)&arg, 0)) s->hit = 0;
		}
		s->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Sweep (0x6AF230)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SweepTask(TaskNode *n)
	{
		SweepNode *w = (SweepNode *)n;
		if (w->counter >= 15) SetScreenFlash((uint32_t)FlashLevel(w->counter), 0);
		const int32_t s0 = w->counter - 15;
		if ((uint32_t)s0 < 0x14)
		{
			int16_t P[4] = {}; // (word +6 never written: only loaded into the GTE V0 pad)
			GetEffectSpawnPosition(Caster(), 0x1F, 0x1000, P);
			// 30 fps layer: see mag135_reflect_beam_held.inc
			FX_HELD(held_note_sweep(n);)
			uint8_t *h = AllocHeader(0x58);
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			for (int i = 2; i >= 0; i--)
			{
				const int32_t s = s0 - i;
				if ((uint32_t)s >= 10) continue;
				const int32_t t = SegT(s);
				const int16_t X = SegX(w, t);
				if (i != 0)
				{
					*(uint32_t *)(h + 0x1C) = 0xF0;
					*(int32_t *)(h + 0xC) = (i + 4) << 9;
				}
				else
				{
					*(uint32_t *)(h + 0x1C) = 0x30;
					if ((int16_t)w->next < w->count)
					{
						uint8_t *ent = w->ents[w->next];
						if (X < *(const int16_t *)(ent + 0x1C))
						{
							ReflectNode *r = (ReflectNode *)AddTaskToQueue(QReflects(), ORIG_ReflectTask);
							if (r)
							{
								r->counter = 0;
								r->order = w->order[w->next];
								r->ent = ent;
								GetEffectSpawnPosition(ent, 0xF1, 0, r->pos);
								r->pos[1] = (int16_t)(r->pos[1] + 0x80);
								DecodeModelPrimLayout(MODEL_Reflect, r->layout, 0x310);
							}
							w->next++;
						}
					}
				}
				SegmentRender(h, w, P, X, SegSine(t));
			}
			FieldFree(0x58);
			FX_HELD(held_note_play(PLAY_SWEEP, n, (prim::Layout *)w->layout);)
			Play(w->layout, P, -0x80);
		}
		if (w->counter == 0) BdPlaySE(SOUND_Beam, 0, 0x80);
		const int32_t c = w->counter;
		if ((uint32_t)c < 0x14 && !(c & 3))
		{
			ShotNode *s = (ShotNode *)AddTaskToQueue(QShots(), ORIG_ShotTask);
			if (s)
			{
				s->hit = 0;
				s->counter = 0;
				s->bone = (const int16_t *)SHOT_Bones + ((uint32_t)c >> 2) % 3u;
				DecodeModelPrimLayout(MODEL_Shot, s->layout, 0x1F8);
				DecodeModelPrimLayout(MODEL_ShotHit, s->hitlayout, 0x58);
			}
		}
		if (w->counter >= 0x39)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		w->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x6AEF50)
	// ------------------------------------------------------------------
	static void StartSweep()
	{
		SweepNode *w = (SweepNode *)AddTaskToQueue(QSweep(), ORIG_SweepTask);
		Memset32(&w->counter, 0, 0xB5);
		const ActionData *act = Ctx()->actions;
		w->count = act->target_count;
		w->centre[2] = 0;
		w->centre[1] = 0;
		w->centre[0] = 0;
		for (int i = 0; i < w->count; i++)
		{
			w->order[i] = (uint8_t)i;
			uint8_t *ent = Entity(Ctx()->actions->targets[i * TARGET_STRIDE]);
			w->ents[i] = ent;
			int16_t p[4];
			GetDefaultEffectPosition(ent, p);
			w->centre[0] = (int16_t)(w->centre[0] + p[0]);
			w->centre[1] = (int16_t)(w->centre[1] + p[1]);
			w->centre[2] = (int16_t)(w->centre[2] + p[2]);
		}
		const int32_t count = w->count;
		w->centre[0] = (int16_t)(w->centre[0] / count);
		w->centre[1] = (int16_t)(w->centre[1] / count);
		w->centre[2] = (int16_t)(w->centre[2] / count);
		// sorted by entity x, descending (the sweep runs towards -x)
		for (int i = 0; i < w->count - 1; i++)
		{
			for (int j = i + 1; j < w->count; j++)
			{
				uint8_t *a = w->ents[i], *b = w->ents[j];
				if (*(const int16_t *)(a + 0x1C) < *(const int16_t *)(b + 0x1C))
				{
					w->ents[i] = b;
					w->ents[j] = a;
					const uint8_t o = w->order[j];
					w->order[j] = w->order[i];
					w->order[i] = o;
				}
			}
		}
		DecodeModelPrimLayout(MODEL_Sweep, w->layout, 0x2B8);
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag135_reflect_beam_held.inc
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
		if (m->counter == 1 && !m->started)
		{
			m->started = 1;
			InitTaskQueuePool(QSweep(), (void *)(TexBase() + 0x2A64), 0x2E0, 1);
			InitTaskQueuePool(QShots(), (void *)(TexBase() + 0x1704), 0x26C, 8);
			InitTaskQueuePool(QReflects(), (void *)(TexBase() + 0xD80), 0x32C, 3);
			InitTaskQueuePool(QSprites(), (void *)TexBase(), 0x24, 0x60);
			StartSweep();
		}
		int a0, a1, a2, a3;
		if (m->started)
		{
			a0 = ExecuteTaskQueue(QSweep());
			ComputeBonesWorldMatrices(Caster() + 0x60, Caster() + 0x40);
			a1 = ExecuteTaskQueue(QShots());
			BuildBoneMatricesFromPose(Caster() + 0x60);
			a2 = ExecuteTaskQueue(QReflects());
			a3 = ExecuteTaskQueue(QSprites());
		}
		else a0 = a1 = a2 = a3 = (int)n;
		if (m->started && a0 == 0 && a1 == 0 && a2 == 0 && a3 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag135_reflect_beam()
	{
		register_port(reflect135::ORIG_MasterTask, (void *)reflect135::MasterTask, "135 MasterTask", 135);
		register_port(reflect135::ORIG_SweepTask, (void *)reflect135::SweepTask, "135 SweepTask", 135);
		register_port(reflect135::ORIG_ShotTask, (void *)reflect135::ShotTask, "135 ShotTask", 135);
		register_port(reflect135::ORIG_ReflectTask, (void *)reflect135::ReflectTask, "135 ReflectTask", 135);
		register_port(reflect135::ORIG_BurstTask, (void *)reflect135::BurstTask, "135 BurstTask", 135);
		register_port(reflect135::ORIG_GlintTask, (void *)reflect135::GlintTask, "135 GlintTask", 135);
		register_port(reflect135::ORIG_SparkTask, (void *)reflect135::SparkTask, "135 SparkTask", 135);
		// 30 fps layer: see mag135_reflect_beam_held.inc
		FX_HELD(register_mag135_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag135_reflect_beam_held.inc"
#endif
