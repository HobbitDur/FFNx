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

// Effect 338: Moomba - Friendship / MoombaMoomba (timeline-A GF family, MAG_338_*).
//
// Structure (see gf_study/gf_inventory_timeline.md 3.1):
//   SetupSummon (0x6EBD60, not a task; thunk 0x6EBD50) - TIM uploads, cast context, attacker /
//     first target entities, root queue 0x2556210 (pool 0x2556220, 1 x 0x14) with the master,
//     engine camera animation 0x1370900 (Battle_PlayCameraAnimation: the module never writes
//     the battle camera itself).
//   SequenceTask (master, 0x6EBE20) - flips the two packet cursors (three 64 KB arenas in the
//     model buffer), spawns the creature at tick 2 (sparks reset, three pools carved out of the
//     model buffer), runs the queues creature 0x2556200, prims 0x25561F0, debris 0x25561E0;
//     ends when the creature is gone and the last stream is done. No debug pause flag.
//   CreatureTask (0x6EC0F0) - 76-tick timeline: screen flash, Moomba (battle model, drawn on
//     0..67), the ring prim model (0..7, 56..67), prim-model tasks and sparks spawned at the
//     target's / Moomba's vertices, bursts with debris (39..48), damage (48), the sprite-spark
//     pool (64 x 0x18 at model buffer + 0: drawn then moved every tick), sounds, voice, stream.
//   PrimTask (0x6ECC50) - a keyframed prim-model player (shared player 0x701970, callback
//     0x6ECCE0 used through its original address: pure draw), ends with its animation.
//   DebrisTask (0x6ED0C0) - a spinning, falling prim-model chunk (7..10 ticks).
// Module globals: 0x25561C8..0x2556253 (gf_study/gf_global_ranges.md).

#include "fx_port.h"

namespace ff8fx
{
namespace m338
{
	using namespace eng;

	// --- module globals ---
	inline uint8_t *&CastCtx() { return var<uint8_t *>(0x25561CC); }
	inline uint8_t *&Attacker() { return var<uint8_t *>(0x25561D0); }      // BattleEntitySlotData of the caster
	inline uint8_t *&Target() { return var<uint8_t *>(0x25561D4); }        // BattleEntitySlotData of the first target
	inline uint8_t *&SparkHint() { return var<uint8_t *>(0x25561D8); }     // next spark slot to try
	inline TaskQueue &QueueDebris() { return var<TaskQueue>(0x25561E0); }  // pool ModelBuffer + 0x600 (64 x 0x24)
	inline TaskQueue &QueuePrims() { return var<TaskQueue>(0x25561F0); }   // pool ModelBuffer + 0xF00 (8 x 0x198)
	inline TaskQueue &QueueCreature() { return var<TaskQueue>(0x2556200); }// pool ModelBuffer + 0x1BC0 (1 x 0xD0)
	inline TaskQueue &QueueRoot() { return var<TaskQueue>(0x2556210); }    // pool 0x2556220 (1 x 0x14)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2556234); }
	inline uint32_t &PacketCursor2() { return var<uint32_t>(0x2556238); }  // the next arena (4th arg of the model draw)
	inline uint32_t &Flag23C() { return var<uint32_t>(0x255623C); }       // zeroed by the master every tick and at the end
	// position words: 0 x, 1 y, 2 z + depth/4 + 0x400 of the target; 4 x, 5 y + height, 6 z + depth/4
	inline int16_t &W(int k) { return var<int16_t>(0x2556240 + 2 * k); }
	inline uint8_t *&ModelBuffer() { return var<uint8_t *>(0x2556250); }  // = Magic_TextureOFF_ToEAX1()

	static const uint32_t ORIG_SequenceTask = 0x6EBE20;
	static const uint32_t ORIG_CreatureTask = 0x6EC0F0;
	static const uint32_t ORIG_PrimTask = 0x6ECC50;
	static const uint32_t ORIG_DebrisTask = 0x6ED0C0;
	static const uint32_t CB_PrimObject = 0x6ECCE0;      // prim-model object callback (pure draw)
	static const uint32_t MODEL_Ring = 0x1371640;        // ring prim model (0x7043B0 renderer)
	static const uint32_t MODEL_Debris = 0x1372CE8;      // debris prim model
	static const uint32_t SEQ_Spark = 0x137130C;         // spark flipbooks
	static const uint32_t SEQ_SparkFlash = 0x1371464;
	static const uint32_t SEQ_SparkFly = 0x13711FC;

	// model buffer regions
	static const uint32_t MB_SPARKS = 0x0;               // 64 x 0x18
	static const uint32_t MB_SPARKS_SIZE = 0x600;
	static const uint32_t MB_SCRATCH = 0x1C90;           // model draw vertex pool / prim morph output
	static const uint32_t MB_SCRATCH_SIZE = 0x4000;
	static const uint32_t MB_STREAM_STATE = 0x5C90;
	static const uint32_t MB_ARENA0 = 0x5CD8;            // arenas 0x5CD8 / 0x15CD8 / 0x25CD8 (0x10000 each)

	static uint32_t g_ported_tick = 0xFFFFFFFF;          // real tick on which the ported master last ran

	// --- engine and module functions called through their original addresses ---
	namespace x
	{
		inline int32_t Rand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); }
		inline int32_t Cos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
		// MAG_063_sub_701270 / 701220: rotation about Y / X (3x3 + pad, 0x14 bytes; angle = low 16 bits)
		inline void RotY(int32_t angle, void *out) { fn<void (__cdecl *)(int32_t, void *)>(0x701270)(angle, out); }
		inline void RotX(int32_t angle, void *out) { fn<void (__cdecl *)(int32_t, void *)>(0x701220)(angle, out); }
		// sub_56CB90: out = (a * wa + b * wb) >> 12 through the GTE (3 x s16)
		inline void Blend(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CB90)(a, b, wa, wb, out); }
		// sub_56C220: 3x3 product into a
		inline void MulMatrix(void *a, const void *b) { fn<void (__cdecl *)(void *, const void *)>(0x56C220)(a, b); }
		// MAG_070_sub_7043B0: prim model draw (header: +0 model, +8, +0xC fade, +0x18, +0x1C mode)
		inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
		// MAG_065_sub_6FC9E0: flipbook sprite draw (header 0xB4)
		inline uint32_t SpriteDraw(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x6FC9E0)(h, ot, mode, cursor); }
		inline void Decode(uint32_t src, void *layout, int size) { fn<void (__cdecl *)(uint32_t, void *, int)>(0x7016B0)(src, layout, size); }
		// battle model
		inline void SetAnim(void *e, int id) { fn<void (__cdecl *)(void *, int)>(0x6574D0)(e, id); }
		inline void AdvanceAnim(void *e) { fn<void (__cdecl *)(void *)>(0x6FBDB0)(e); }
		inline uint32_t DrawModel(void *e, void *buf, uint32_t cursor, uint32_t a4) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, uint32_t)>(0x6A79B0)(e, buf, cursor, a4); }
		inline void BindModel(void *e, void *sections, uint32_t data) { fn<void (__cdecl *)(void *, void *, uint32_t)>(0x6EC060)(e, sections, data); }
		// MAG_070_sub_6DFD90: model height range of a battle entity (2 x s16)
		inline void Heights(void *entity, void *out) { fn<void (__cdecl *)(void *, void *)>(0x6DFD90)(entity, out); }
		inline void SpawnPosition(void *entity, int bone, int a, void *out) { fn<void (__cdecl *)(void *, int, int, void *)>(0x502170)(entity, bone, a, out); }
		// battle / sound / streams
		inline void ChainTransformation(void *entity, int kind) { fn<void (__cdecl *)(void *, int)>(0x505C00)(entity, kind); }
		inline void ApplyResultToTarget(void *targets) { fn<void (__cdecl *)(void *)>(0x506690)(targets); }
		inline void PlaySE(uint32_t se, int a, int b) { fn<void (__cdecl *)(uint32_t, int, int)>(0x501330)(se, a, b); }
		inline void ReleaseVoice(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
		inline void Load(int id, uint32_t dst, int n) { fn<void (__cdecl *)(int, uint32_t, int)>(0x5341D0)(id, dst, n); }
		inline void PreLoad() { fn<void (__cdecl *)()>(0x534210)(); }
		inline int PreLoad0(int a) { return fn<int (__cdecl *)(int)>(0x5342C0)(a); }
	}

#pragma pack(push, 1)
	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
	struct MasterNode // root pool 0x2556220, 0x14 bytes
	{
		TaskNode hdr;
		uint16_t counter; // +0x0C
		uint8_t pad0E;
		uint8_t spawned;  // +0x0F creature spawned
		uint32_t parity;  // +0x10 packet arena parity
	};

	struct CreatureNode // the single node of queue 0x2556200 = ModelBuffer + 0x1BC0, 0xD0 bytes
	{
		TaskNode hdr;
		int16_t counter;        // +0x0C
		int16_t pad0E;
		uint32_t voice;         // +0x10 BdSound_ClaimVoiceSlot
		int16_t h0, h1;         // +0x14 target height range (MAG_070_sub_6DFD90)
		uint8_t *attacker;      // +0x18
		uint8_t pad1C[8];
		uint8_t model[0x9C];    // +0x24 Moomba: E+0x1C pos words, E+0x28 colour, E+0x40 root Mat4x3 (translation
		                        //   node+0x78/+0x7C/+0x80), E+0x60 BattleAnimHeader, E+0x64 sections, E+0x6C BattleAnimCmd
		uint8_t sections[0x10]; // +0xC0
	};

	struct PrimNode // queue 0x25561F0 (8 x 0x198)
	{
		TaskNode hdr;
		int16_t pos[3];         // +0x0C placement
		int16_t yaw;            // +0x12 always 0
		uint8_t layout[0x184];  // +0x14 prim-model player
	};

	struct DebrisNode // queue 0x25561E0 (64 x 0x24)
	{
		TaskNode hdr;
		int16_t pos[3];         // +0x0C
		int16_t vel[3];         // +0x12
		int8_t acc[3];          // +0x18 added to vel each tick (0, 0x14, 0)
		uint8_t life;           // +0x1B ticks left (7..10)
		uint8_t rot[2];         // +0x1C angles x, y (x 16)
		uint8_t spin[2];        // +0x1E added to rot each tick
		int16_t size;           // +0x20 uniform scale (Y rotation scaled by it)
		int16_t pad22;
	};

	struct Spark // sprite particle, 64 x 0x18 at ModelBuffer + 0
	{
		uint32_t seq;           // +0x00 flipbook data
		int16_t pos[3];         // +0x04
		int16_t life;           // +0x0A 0 = free; flipbook frame = n - (life - 1) % n
		int16_t vel[3];         // +0x0C
		int16_t unused12;       // +0x12 set on spawn, never read
		int8_t acc[3];          // +0x14 x 4 added to vel (z: added to the position only)
		uint8_t pad17;
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14, "master node is 0x14 bytes");
	static_assert(sizeof(CreatureNode) == 0xD0, "creature node is 0xD0 bytes");
	static_assert(sizeof(PrimNode) == 0x198, "prim node is 0x198 bytes");
	static_assert(sizeof(DebrisNode) == 0x24, "debris node is 0x24 bytes");
	static_assert(sizeof(Spark) == 0x18, "spark is 0x18 bytes");

	inline Mat4x3 &Root(CreatureNode *cn) { return *(Mat4x3 *)(cn->model + 0x40); }
	inline Spark *Sparks(uint8_t *mb) { return (Spark *)(mb + MB_SPARKS); }

	// trunc(trunc(x / -10) / 4) as compiled (imul 0x99999999, sar 2, sign fix, sar 2)
	static int32_t DivNeg10Sar2(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * (int32_t)0x99999999) >> 32);
		hi >>= 2;
		hi += (int32_t)((uint32_t)hi >> 31);
		return hi >> 2;
	}

	// ------------------------------------------------------------------
	// Sparks (sprite particles in the model buffer)
	// ------------------------------------------------------------------

	// 0x6EC0C0: all slots free, hint on slot 0
	static void SparksReset()
	{
		uint8_t *p = ModelBuffer() + 0xA;
		for (int i = 0x40; i; i--, p += 0x18) *(int16_t *)p = 0;
		SparkHint() = ModelBuffer();
	}

	// 0x6EC980: the hint slot if free, else the first free slot; the hint moves to the next slot
	static Spark *SparkAlloc()
	{
		uint8_t *MB = ModelBuffer();
		uint8_t *p = SparkHint();
		if (*(int16_t *)(p + 0xA) != 0)
		{
			p = MB;
			if (*(int16_t *)(p + 0xA) != 0)
			{
				int i = 0x40;
				for (;;)
				{
					p += 0x18;
					if (--i == 0) return nullptr;
					if (*(int16_t *)(p + 0xA) == 0) break;
				}
			}
		}
		SparkHint() = p < MB + 0x5E8 ? p + 0x18 : MB;
		return (Spark *)p;
	}

	// 0x6EC850: every live spark as a flipbook frame at its camera-space position (scale 1/2)
	static void SparksDraw(Spark *pool)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(int16_t *)(h + 0x60) = 0;
		*(int16_t *)(h + 0x58) = 0;
		*(int16_t *)(h + 0x50) = 0;
		*(int16_t *)(h + 0x48) = 0;
		for (int i = 0; i < 0x40; i++)
		{
			Spark *s = &pool[i];
			if (s->life == 0) continue;
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			GteLoadV0(s->pos);
			GteMVMVA_RotV0Tr();
			GteReadMAC123((int32_t *)(h + 0x98));
			memset(h + 0x84, 0, 0x14);
			*(int16_t *)(h + 0x94) = 0x800;
			*(int16_t *)(h + 0x8C) = 0x800;
			*(int16_t *)(h + 0x84) = 0x800;
			uint8_t *seq = (uint8_t *)s->seq;
			*(uint8_t **)h = seq;
			int32_t n = *(int16_t *)(seq + 8);
			int32_t k = n - ((int32_t)s->life - 1) % n;
			uint8_t *frame = seq + *(int16_t *)(seq + 8 + 2 * k);
			*(uint8_t **)(h + 0x2C) = frame;
			int32_t v = *(int32_t *)frame;
			*(int32_t *)(h + 0x30) = v;
			if (v < 0)
			{
				// (sticks for the following sparks of this call: the header is not reset)
				h[0x25] |= 1;
				*(int32_t *)(h + 0x30) = v & 0x7FFFFFFF;
			}
			*(uint8_t **)(h + 0x2C) = frame + 4;
			PacketCursor() = x::SpriteDraw(h, RenderOT(), 2, PacketCursor());
		}
		FieldFree(0xB4);
	}

	// 0x6EC9D0: position += velocity, velocity += 4 * acc (z: the acceleration goes to the
	// position only), one tick of life
	static void SparksMove(Spark *pool)
	{
		for (int i = 0; i < 0x40; i++)
		{
			Spark *s = &pool[i];
			if (s->life == 0) continue;
			int16_t vx = s->vel[0], vy = s->vel[1];
			s->pos[0] = (int16_t)(s->pos[0] + vx);
			s->pos[1] = (int16_t)(s->pos[1] + vy);
			s->vel[0] = (int16_t)(vx + s->acc[0] * 4);
			s->life--;
			s->vel[1] = (int16_t)(vy + s->acc[1] * 4);
			s->pos[2] = (int16_t)(s->pos[2] + (int16_t)(s->acc[2] * 4 + s->vel[2]));
		}
	}

	// 0x6ECB60: world position of a model vertex record {x, y, z, bone} of Moomba (rebuilds the
	// bone matrices from the current pose first)
	static void VertexPos(const int16_t *v, int16_t *out)
	{
		uint8_t *MB = ModelBuffer();
		uint8_t *bones = *(uint8_t **)*(uint8_t **)(MB + 0x1BC0 + 0x88) + 0x10;
		BuildBoneMatricesFromPose(MB + 0x1BC0 + 0x84);
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(MB + 0x1BC0 + 0x64), (const Mat4x3 *)(bones + 48 * (int32_t)v[3] + 0x10), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// 0x6ECA30: one flash spark (7 ticks) and 7 flying sparks (10 ticks) at a model vertex,
	// 500 in front of it
	static void SpawnSparksAt(uint32_t vertex)
	{
		int16_t at[4]; // 4th word never written (the dword copy of z carries it, only z is kept)
		VertexPos((const int16_t *)vertex, at);
		Spark *s = SparkAlloc();
		if (!s) return;
		s->pos[0] = at[0];
		s->pos[1] = at[1];
		s->life = 7;
		s->pos[2] = (int16_t)(at[2] - 500);
		s->vel[2] = 0;
		s->vel[0] = 0;
		s->vel[1] = 0;
		*(uint32_t *)s->acc = 0;
		s->unused12 = 0xBB8;
		s->seq = SEQ_SparkFlash;
		for (int k = 7;;)
		{
			s = SparkAlloc();
			if (!s) return;
			s->pos[0] = at[0];
			s->pos[1] = at[1];
			s->pos[2] = (int16_t)(at[2] - 500);
			s->life = 0xA;
			int32_t r = x::Rand();
			s->vel[0] = (int16_t)((r & 0x7F) + 0x80);
			r = x::Rand();
			int32_t vy = (r & 0x7F) - 0x40;
			s->vel[1] = (int16_t)vy;
			s->pos[1] = (int16_t)(s->pos[1] + vy * 2);
			r = x::Rand();
			if (r & 1) s->vel[0] = (int16_t)-s->vel[0];
			s->acc[2] = 0;
			s->acc[0] = (int8_t)DivNeg10Sar2(s->vel[0]);
			s->vel[2] = 0;
			s->acc[1] = (int8_t)DivNeg10Sar2(s->vel[1]);
			s->unused12 = 0xC00;
			s->seq = SEQ_SparkFly;
			if (--k == 0) return;
		}
	}

	// ------------------------------------------------------------------
	// Draw helpers
	// ------------------------------------------------------------------

	// 0x6EC7B0: prim model (0x7043B0 renderer) at (x, y, z) tilted about X, fade a5
	static void DrawRing(int32_t px, int32_t py, int32_t pz, int32_t angle, int32_t fade, uint32_t model)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x78);
		x::RotX(angle, w);
		Mat4x3 *m = (Mat4x3 *)w;
		m->t[0] = px;
		m->t[1] = py;
		m->t[2] = pz;
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		*(int32_t *)(w + 0x28) = 0;
		*(int32_t *)(w + 0x2C) = fade;
		*(int32_t *)(w + 0x3C) = 0xF0;
		*(uint32_t *)(w + 0x20) = model;
		*(int32_t *)(w + 0x38) = -0x80;
		PacketCursor() = x::RenderPrim2(w + 0x20, RenderOT(), 2, PacketCursor());
		FieldFree(0x78);
	}

	// 0x6ED1E0 (IDA: MAG_210_sub_6ED1E0): rotation about Y scaled by r (m11 = r)
	static void ScaledRotY(int32_t angle, int32_t radius, uint8_t *out)
	{
		memset(out, 0, 0x14);
		int32_t a = -(int32_t)(int16_t)angle;
		int32_t r = (int16_t)radius;
		int32_t s = mul32(ComputeSin(a), r) >> 12;
		int32_t c = mul32(x::Cos(a), r) >> 12;
		*(int16_t *)(out + 0xC) = (int16_t)s;
		*(int16_t *)(out + 8) = (int16_t)radius;
		*(int16_t *)(out + 0) = (int16_t)c;
		*(int16_t *)(out + 4) = (int16_t)-s;
		*(int16_t *)(out + 0x10) = (int16_t)c;
	}

	// draw part of 0x6ED0C0: debris chunk, rot (x 16) about X then scaled Y rotation
	static void DebrisDraw(const int16_t pos[3], int32_t rx, int32_t ry, int16_t size)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x98);
		ScaledRotY(ry, size, h + 0x78);
		x::RotX(rx, h + 0x58);
		x::MulMatrix(h + 0x58, h + 0x78);
		Mat4x3 *m = (Mat4x3 *)(h + 0x58);
		m->t[0] = pos[0];
		m->t[1] = pos[1];
		m->t[2] = pos[2];
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		*(uint32_t *)h = MODEL_Debris;
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x98);
	}

	// callback context of 0x6ECCE0 (arg of the prim player), 0x28 bytes on the task's stack
	static void PrimCtx(const PrimNode *p, uint8_t *ctx)
	{
		x::RotY(p->yaw, ctx);
		Mat4x3 *m = (Mat4x3 *)ctx;
		m->t[0] = p->pos[0];
		m->t[1] = p->pos[1];
		m->t[2] = p->pos[2];
		ComposeAffineTransform(&Camera(), m, m);
		*(uint32_t *)(ctx + 0x24) = (uint32_t)(ModelBuffer() + MB_SCRATCH); // morph output
		*(int32_t *)(ctx + 0x20) = -0x80;                                  // header +0x18
	}

	// ------------------------------------------------------------------
	// Held-frame bookkeeping of the real tick
	// ------------------------------------------------------------------
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct PoseMemo { uint32_t tick; bool ok; uint32_t size; uint8_t pose[SKEL_MAX]; };

	struct CreatureMemo
	{
		uint32_t tick;
		CreatureNode *node;
		int16_t c;                  // counter the tick ran with
		bool ring; int32_t ring_x, ring_y, ring_z, ring_angle, ring_fade;
		bool model, restart;        // model drawn; its animation restarted before the draw
		int32_t root_y, root_z;     // root it was drawn with
		bool s0_ok; uint32_t s0_size; uint8_t s0[SKEL_MAX]; // skeleton at the start of the tick (drawn matrices)
		bool sparks; Spark pool[0x40];                      // the sparks as drawn
	};
	static CreatureMemo g_cm = { 0xFFFFFFFF };
	static PoseMemo g_built[2] = { { 0xFFFFFFFF }, { 0xFFFFFFFF } }; // pose the matrices were built from after the draw, per tick parity

	struct PrimRun { PrimNode *node; };
	static uint32_t g_prim_tick = 0xFFFFFFFF;
	static int g_prim_n = 0;
	static PrimRun g_prim_run[16];

	struct DebrisRun { DebrisNode *node; bool ended; int16_t pos[3]; uint8_t rot[2]; int16_t size; };
	static uint32_t g_debris_tick = 0xFFFFFFFF;
	static int g_debris_n = 0;
	static DebrisRun g_debris_run[0x40];

	static uint8_t *Skeleton(CreatureNode *cn, uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(cn->model + 0x64); // BattleAnimHeader.comFileData
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= SKEL_MAX ? sk : nullptr;
	}

	// ------------------------------------------------------------------
	// Master task (0x6EBE20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SequenceTask(TaskNode *n)
	{
		MasterNode *node = (MasterNode *)n;
		g_ported_tick = g_real_tick;
		if (node->parity)
		{
			PacketCursor() = (uint32_t)(ModelBuffer() + MB_ARENA0);
			PacketCursor2() = (uint32_t)(ModelBuffer() + MB_ARENA0 + 0x10000);
			node->parity = 0;
		}
		else
		{
			PacketCursor() = (uint32_t)(ModelBuffer() + MB_ARENA0 + 0x10000);
			PacketCursor2() = (uint32_t)(ModelBuffer() + MB_ARENA0 + 0x20000);
			node->parity = 1;
		}
		Flag23C() = 0;

		if (node->counter == 2 && !node->spawned)
		{
			SparksReset();
			node->spawned = 1;
			InitTaskQueuePool(&QueueDebris(), ModelBuffer() + 0x600, 0x24, 0x40);
			InitTaskQueuePool(&QueueCreature(), ModelBuffer() + 0x1BC0, 0xD0, 1);
			InitTaskQueuePool(&QueuePrims(), ModelBuffer() + 0xF00, 0x198, 8);
			CreatureNode *cn = (CreatureNode *)AddTaskToQueue(&QueueCreature(), ORIG_CreatureTask);
			Memset32(&cn->counter, 0, 0x31);
			cn->attacker = Attacker();
			x::Heights(Target(), &cn->h0);
			cn->voice = ClaimVoiceSlot((const void *)0x13710CC, 1, 0x80);
			x::BindModel(cn->model, cn->sections, 0x136BB30);
			x::SetAnim(cn->model, 0);
			x::RotY(0, &Root(cn));
			uint8_t *t = Target();
			W(0) = *(int16_t *)(t + 0x1C);
			W(1) = *(int16_t *)(t + 0x1E);
			W(2) = (int16_t)(*(int16_t *)(t + 0x26) / 4 + *(int16_t *)(t + 0x20) + 0x400);
			int16_t tx = *(int16_t *)(t + 0x1C);
			W(4) = tx;
			W(5) = (int16_t)(cn->h1 + *(int16_t *)(t + 0x1E));
			W(6) = (int16_t)(*(int16_t *)(t + 0x26) / 4 + *(int16_t *)(t + 0x20));
			int16_t *mpos = (int16_t *)(cn->model + 0x1C);
			Root(cn).t[0] = tx;
			mpos[0] = tx;
			Root(cn).t[1] = 0;
			mpos[1] = 0;
			Root(cn).t[2] = W(6);
			mpos[2] = W(6);
			StreamStateInit(ModelBuffer() + MB_STREAM_STATE);
		}

		uint32_t left = (uint32_t)n; // the original keeps the node pointer here: non-zero
		if (node->spawned)
		{
			left = (uint32_t)ExecuteTaskQueue(&QueueCreature());
			ExecuteTaskQueue(&QueuePrims());
			ExecuteTaskQueue(&QueueDebris());
		}
		if (node->spawned && left == 0)
		{
			x::PreLoad();
			return x::PreLoad0(0) ? TASK_END : 0;
		}
		node->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Prim-model task (0x6ECC50) and its spawners
	// ------------------------------------------------------------------

	// 0x6ECBE0: prim-model task at a target-relative offset (vec + target, y + yoff)
	static void SpawnPrim(uint32_t vec, uint32_t layout, int32_t yoff)
	{
		PrimNode *p = (PrimNode *)AddTaskToQueue(&QueuePrims(), ORIG_PrimTask);
		if (!p) return;
		const int16_t *v = (const int16_t *)vec;
		p->pos[0] = (int16_t)(v[0] + W(4));
		p->pos[1] = (int16_t)(v[1] + W(1) + yoff);
		p->pos[2] = (int16_t)(v[2] + W(6));
		p->yaw = 0;
		x::Decode(layout, p->layout, 0x184);
	}

	// 0x6ECF80: burst prim model on the ground under a model vertex, 8 debris chunks there
	static void SpawnBurst(uint32_t vertex)
	{
		PrimNode *p = (PrimNode *)AddTaskToQueue(&QueuePrims(), ORIG_PrimTask);
		if (!p) return;
		int16_t at[4];
		VertexPos((const int16_t *)vertex, at);
		p->pos[1] = 0;
		p->pos[0] = at[0];
		p->pos[2] = at[2];
		p->yaw = 0;
		x::Decode(0x13755B8, p->layout, 0xF8);
		DebrisNode *d = (DebrisNode *)AddTaskToQueue(&QueueDebris(), ORIG_DebrisTask);
		if (!d) return;
		for (int k = 8;;)
		{
			*(uint32_t *)d->acc = 0; // acc and life
			d->acc[1] = 0x14;
			int32_t r = x::Rand();
			d->size = (int16_t)((r & 0x7FF) + 0x800);
			d->pos[0] = at[0];
			d->pos[1] = at[1];
			d->pos[2] = at[2];
			r = x::Rand();
			d->life = (uint8_t)((r & 3) + 7);
			r = x::Rand();
			d->vel[0] = (int16_t)((r & 0x7F) - 0x40);
			r = x::Rand();
			d->vel[2] = (int16_t)(-0x80 - (r & 0x7F));
			r = x::Rand();
			d->vel[1] = (int16_t)(-0x80 - (r & 0x7F));
			d->rot[0] = (uint8_t)x::Rand();
			d->rot[1] = (uint8_t)x::Rand();
			d->spin[0] = (uint8_t)x::Rand();
			d->spin[1] = (uint8_t)x::Rand();
			if (--k == 0) return;
			d = (DebrisNode *)AddTaskToQueue(&QueueDebris(), ORIG_DebrisTask);
			if (!d) return;
		}
	}

	static uint32_t __cdecl PrimTask(TaskNode *n)
	{
		PrimNode *p = (PrimNode *)n;
		if (g_prim_tick != g_real_tick) { g_prim_tick = g_real_tick; g_prim_n = 0; }
		if (g_prim_n < 16) g_prim_run[g_prim_n++].node = p;
		uint8_t ctx[0x28];
		PrimCtx(p, ctx);
		int r = prim::play((prim::Layout *)p->layout, (prim::Callback)CB_PrimObject, (int)ctx, 0);
		return r ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Debris chunk (0x6ED0C0): draw, then one tick of life, spin, fall
	// ------------------------------------------------------------------
	inline int32_t DebrisAngle(uint8_t b) { return (int16_t)((int8_t)b * 16); }

	static uint32_t __cdecl DebrisTask(TaskNode *n)
	{
		DebrisNode *d = (DebrisNode *)n;
		if (g_debris_tick != g_real_tick) { g_debris_tick = g_real_tick; g_debris_n = 0; }
		DebrisRun *run = g_debris_n < 0x40 ? &g_debris_run[g_debris_n++] : nullptr;
		if (run)
		{
			run->node = d;
			run->ended = false;
			memcpy(run->pos, d->pos, sizeof(run->pos));
			run->rot[0] = d->rot[0];
			run->rot[1] = d->rot[1];
			run->size = d->size;
		}
		DebrisDraw(d->pos, DebrisAngle(d->rot[0]), DebrisAngle(d->rot[1]), d->size);
		d->life--;
		if (d->life == 0)
		{
			if (run) run->ended = true;
			return TASK_END;
		}
		d->rot[0] = (uint8_t)(d->rot[0] + d->spin[0]);
		int16_t vy = d->vel[1], vx = d->vel[0];
		d->rot[1] = (uint8_t)(d->rot[1] + d->spin[1]);
		d->pos[1] = (int16_t)(d->pos[1] + vy);
		d->pos[0] = (int16_t)(d->pos[0] + vx);
		int16_t vz = d->vel[2];
		d->pos[2] = (int16_t)(d->pos[2] + vz);
		d->vel[0] = (int16_t)(d->acc[0] + vx);
		d->vel[1] = (int16_t)(d->acc[1] + vy);
		d->vel[2] = (int16_t)(d->acc[2] + vz);
		return 0;
	}

	// ------------------------------------------------------------------
	// Creature timeline (0x6EC0F0): one node at ModelBuffer + 0x1BC0, 76 ticks
	// ------------------------------------------------------------------
	static void Chain() { x::ChainTransformation(Target(), 4); }

	// ring fade / angle of tick c (0..7 and 56..67), pure (held-frame prediction)
	static bool RingAt(int32_t c, int32_t &angle, int32_t &fade)
	{
		if ((uint32_t)c < 8)
		{
			uint32_t q7 = ((uint32_t)c << 10) / 7;
			int32_t co = x::Cos((int32_t)q7);
			fade = 0x1000 - x::Cos((0x1000 - co) >> 2);
			angle = (int32_t)((uint32_t)c << 8);
			return true;
		}
		uint32_t e = (uint32_t)(c - 0x38);
		if (e < 0xC)
		{
			uint32_t q = ((e << 12) / 12) >> 2;
			int32_t s = ComputeSin((int32_t)q) >> 2;
			fade = 0x1000 - ComputeSin(s);
			angle = (int32_t)(e << 8);
			return true;
		}
		return false;
	}

	// root y / z the tick with counter c sets before its draws (from y / z), pure
	static void RootAt(int32_t c, int32_t &y, int32_t &z)
	{
		uint32_t e8 = (uint32_t)(c - 8);
		if (e8 < 0x30 && e8 - 0x18 < 5)
		{
			int32_t s = ComputeSin(0x400 - (int32_t)(((e8 - 0x18) << 10) >> 2));
			int16_t out[4];
			x::Blend((const void *)0x2556240, (const void *)0x2556248, 0x1000 - s, s, out);
			y = out[1];
			z = out[2];
		}
		if ((uint32_t)c < 8)
		{
			int32_t a = 0x400 - (int32_t)(((uint32_t)c << 10) / 7);
			int32_t s = ComputeSin(a);
			y = (int32_t)W(5) - mul32(s, 2000) / 4096;
			z = mul32(a, 4000) / 1024 + W(6);
		}
		uint32_t e = (uint32_t)(c - 0x38);
		if (e < 4)
		{
			int32_t b = (int32_t)((e << 10) / 3);
			int32_t s = ComputeSin(b);
			y = (int32_t)W(1) - mul32(s, 2000) / 4096;
			z = mul32(b, 1000) / 1024 + W(2);
		}
		e = (uint32_t)(c - 0x3C);
		if (e < 8)
		{
			int32_t a = (int32_t)(((e + 1) << 10) >> 3);
			int32_t s = ComputeSin(a);
			y = (int32_t)W(1) - mul32(s, 4000) / 4096 - 0x7D0;
			z = mul32(a, 4000) / 1024 + W(2) + 0x3E8;
		}
	}

	static uint32_t __cdecl CreatureTask(TaskNode *n)
	{
		CreatureNode *cn = (CreatureNode *)n;
		uint8_t *e = cn->model;
		Mat4x3 &root = Root(cn);

		CreatureMemo &M = g_cm;
		M.tick = g_real_tick;
		M.node = cn;
		M.c = cn->counter;
		M.ring = M.model = M.restart = M.sparks = false;
		{
			uint32_t size = 0;
			uint8_t *sk = Skeleton(cn, &size);
			M.s0_ok = sk != nullptr;
			if (sk) { memcpy(M.s0, sk, size); M.s0_size = size; }
		}

		// screen flash: up over 0..7, down over 69..75
		{
			int16_t c = cn->counter;
			int32_t flash;
			if (c < 8) flash = shl32(c, 11) >> 3;
			else if (c > 0x44) flash = shl32(0x4C - c, 11) >> 3;
			else flash = 0x800;
			SetScreenFlash((uint32_t)flash, 0);
		}

		if (cn->counter == 7)
		{
			SpawnPrim(0x1376678, 0x1372E60, cn->h1);
			Chain();
		}

		uint32_t e8 = (uint32_t)((int32_t)cn->counter - 8);
		if (e8 < 0x30)
		{
			switch ((int32_t)e8 - 4) // counter - 12 (jump table 0x6EC754 / index 0x6EC77C)
			{
			case 0: SpawnPrim(0x1376680, 0x137420C, cn->h1); Chain(); break;
			case 3: SpawnPrim(0x1376688, 0x1372E60, cn->h1); Chain(); break;
			case 6: SpawnPrim(0x1376690, 0x137420C, cn->h1); Chain(); break;
			case 11: case 18: SpawnSparksAt(0x1376670); Chain(); break;
			case 14: case 21: SpawnSparksAt(0x1376668); Chain(); break;
			case 27: Chain(); SpawnBurst(0x1376660); break;
			case 30: SpawnBurst(0x1376658); break;
			case 33: SpawnBurst(0x1376660); break;
			case 36:
				x::ApplyResultToTarget(*(void **)(*(uint8_t **)(CastCtx() + 4) + 8));
				SpawnBurst(0x1376658);
				break;
			default: break;
			}

			// 40..50: two sparks at random bones of the target (a full pool ends the tick here)
			if (e8 >= 0x20 && e8 <= 0x2A)
			{
				int count = 2;
				Spark *s = SparkAlloc();
				if (!s) return 0;
				for (;;)
				{
					s->life = 0xC;
					int32_t r = x::Rand();
					uint8_t *t = Target();
					uint32_t nb = **(uint8_t **)*(uint8_t **)(t + 0x64); // skeleton bone count
					x::SpawnPosition(t, mul32(r, (int32_t)nb) >> 15, 0, s->pos);
					s->vel[0] = 0;
					s->vel[1] = 0;
					s->vel[2] = 0;
					*(uint32_t *)s->acc = 0;
					r = x::Rand();
					s->seq = SEQ_Spark;
					s->unused12 = (int16_t)((r & 0xFFF) + 0x800);
					if (--count == 0) break;
					s = SparkAlloc();
					if (!s) return 0;
				}
			}

			// 32..36: Moomba slides from the raised target point to the target
			uint32_t e32 = e8 - 0x18;
			if (e32 < 5)
			{
				int32_t s = ComputeSin(0x400 - (int32_t)((e32 << 10) >> 2));
				int16_t out[4];
				x::Blend((const void *)0x2556240, (const void *)0x2556248, 0x1000 - s, s, out);
				root.t[1] = out[1];
				root.t[2] = out[2];
			}
		}

		// 0..7: Moomba drops in, ring
		{
			uint32_t c = (uint32_t)(int32_t)cn->counter;
			if (c < 8)
			{
				uint32_t q7 = (c << 10) / 7;
				int32_t a = 0x400 - (int32_t)q7;
				int32_t s = ComputeSin(a);
				root.t[1] = (int32_t)W(5) - mul32(s, 2000) / 4096;
				root.t[2] = mul32(a, 4000) / 1024 + W(6);
				int32_t co = x::Cos((int32_t)q7);
				int32_t fade = 0x1000 - x::Cos((0x1000 - co) >> 2);
				M.ring = true;
				M.ring_x = root.t[0]; M.ring_y = root.t[1]; M.ring_z = root.t[2];
				M.ring_angle = (int32_t)(c << 8); M.ring_fade = fade;
				DrawRing(root.t[0], root.t[1], root.t[2], (int32_t)(c << 8), fade, MODEL_Ring);
			}
		}

		// 56..59: burst of 9 sparks around the target, Moomba hops
		{
			uint32_t ee = (uint32_t)((int32_t)cn->counter - 0x38);
			if (ee < 4)
			{
				int32_t b = (int32_t)((ee << 10) / 3);
				int count = 8;
				Spark *s = SparkAlloc();
				if (s)
				{
					for (;;)
					{
						int32_t r1 = x::Rand();
						int32_t r2 = x::Rand();
						int32_t rad = (r2 & 0x3F) + 0x20;
						int32_t sn = ComputeSin(r1);
						s->vel[1] = 0;
						s->vel[0] = (int16_t)(mul32(sn, rad) / 4096);
						int32_t cs = x::Cos(r1);
						int32_t vz = mul32(cs, rad) / 4096;
						s->vel[2] = (int16_t)vz;
						s->pos[0] = (int16_t)(s->vel[0] + W(0));
						s->pos[1] = W(1);
						s->acc[2] = 0;
						s->pos[2] = (int16_t)(vz + W(2));
						s->acc[1] = 0;
						s->acc[0] = 0;
						s->life = 0xC;
						int32_t r3 = x::Rand();
						s->seq = SEQ_Spark;
						s->unused12 = (int16_t)((r3 & 0xFFF) + 0xBB8);
						int old = count--;
						if (old == 0) break;
						s = SparkAlloc();
						if (!s) break;
					}
				}
				int32_t sn = ComputeSin(b);
				root.t[1] = (int32_t)W(1) - mul32(sn, 2000) / 4096;
				root.t[2] = mul32(b, 1000) / 1024 + W(2);
			}
		}

		// 60..67: Moomba hops back
		{
			uint32_t ee = (uint32_t)((int32_t)cn->counter - 0x3C);
			if (ee < 8)
			{
				int32_t a = (int32_t)(((ee + 1) << 10) >> 3);
				int32_t s = ComputeSin(a);
				root.t[1] = (int32_t)W(1) - mul32(s, 4000) / 4096 - 0x7D0;
				root.t[2] = mul32(a, 4000) / 1024 + W(2) + 0x3E8;
			}
		}

		// 56..67: ring
		{
			uint32_t ee = (uint32_t)((int32_t)cn->counter - 0x38);
			if (ee < 0xC)
			{
				uint32_t q = ((ee << 12) / 12) >> 2;
				int32_t s = ComputeSin((int32_t)q) >> 2;
				int32_t fade = 0x1000 - ComputeSin(s);
				M.ring = true;
				M.ring_x = root.t[0]; M.ring_y = root.t[1]; M.ring_z = root.t[2];
				M.ring_angle = (int32_t)(ee << 8); M.ring_fade = fade;
				DrawRing(root.t[0], root.t[1], root.t[2], (int32_t)(ee << 8), fade, MODEL_Ring);
			}
		}

		// 0..67: Moomba (anim read, then the draw uses the matrices built at the end of the
		// previous draw: the pose the tick started with)
		if ((uint16_t)cn->counter < 0x44)
		{
			x::AdvanceAnim(e);
			M.restart = e[0x6C + 6] == 0; // completed: restarted, pose zeroed
			M.model = true;
			M.root_y = root.t[1];
			M.root_z = root.t[2];
			PacketCursor() = x::DrawModel(e, ModelBuffer() + MB_SCRATCH, PacketCursor(), PacketCursor2());
			PoseMemo &b = g_built[g_real_tick & 1];
			uint32_t size = 0;
			uint8_t *sk = Skeleton(cn, &size);
			b.tick = g_real_tick;
			b.ok = sk != nullptr;
			if (sk) { memcpy(b.pose, sk, size); b.size = size; }
		}

		// shared effect light / position frame
		x::RotY(0, (void *)0x1D99AB0);
		var<int32_t>(0x1D99AC4) = root.t[0];
		var<int32_t>(0x1D99AC8) = root.t[1];
		var<int32_t>(0x1D99ACC) = root.t[2];

		// sparks: drawn, then moved
		M.sparks = true;
		memcpy(M.pool, Sparks(ModelBuffer()), sizeof(M.pool));
		SparksDraw(Sparks(ModelBuffer()));
		SparksMove(Sparks(ModelBuffer()));

		if (cn->counter == 0) x::PlaySE(0x13710C8, 0, 0x80);
		{
			int16_t c = cn->counter;
			if (c == 8) x::SetAnim(e, 1);
			else if (c == 0x38) x::SetAnim(e, 2);
			else if (c == 0x3C) x::SetAnim(e, 3);
		}
		if (cn->counter == 0x48) x::ReleaseVoice(cn->voice);
		cn->counter++;
		if (cn->counter < 0x4C) return 0;
		Flag23C() = 0;
		x::Load(0x16A, (uint32_t)(ModelBuffer() + MB_ARENA0), 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Exact vanilla shapes: everything that moves is drawn half way
	// between what the real tick drew and what the next tick draws; flipbook frames, spawns and
	// the screen flash keep their 15 Hz steps. Nothing here changes game state: no RNG, no
	// spawn, no sound; the model skeleton and root, the model-buffer scratch and the packet
	// cursors are put back.
	// ------------------------------------------------------------------
	static void PoseBlend(uint8_t *sk, const uint8_t *from, int num, int den)
	{
		// sk holds the pose the next tick draws, from the pose drawn this tick: write the
		// midpoint (angles the short way round) into sk's pose fields
		int nb = sk[0];
		bool scaled = (sk[1] & 1) != 0;
		for (int a = 0; a < 3; a++)
		{
			int16_t *p = (int16_t *)(sk + 8) + a;
			int16_t f = ((const int16_t *)(from + 8))[a];
			*p = (int16_t)(f + ((int32_t)*p - f) * num / den);
		}
		for (int b = 0; b < nb; b++)
		{
			int16_t *p = (int16_t *)(sk + 16 + 48 * b + 4);
			const int16_t *f = (const int16_t *)(from + 16 + 48 * b + 4);
			for (int a = 0; a < 3; a++)
			{
				int32_t d = (((int32_t)p[a] - f[a] + 2048) & 4095) - 2048;
				p[a] = (int16_t)(f[a] + d * num / den);
			}
			if (scaled)
				for (int a = 3; a < 6; a++) p[a] = (int16_t)(f[a] + ((int32_t)p[a] - f[a]) * num / den);
		}
	}

	inline int16_t Lerp16(int16_t a, int16_t b, int num, int den) { return (int16_t)(a + (int16_t)(b - a) * num / den); }

	static uint8_t g_skel_save[SKEL_MAX];
	static Spark g_held_pool[0x40];

	static void CreatureHeld(int num, int den)
	{
		const CreatureMemo &M = g_cm;
		if (M.tick != g_real_tick || !M.node) return;
		CreatureNode *cn = M.node; // (an ended node's memory is intact until the next tick)
		int32_t c0 = M.c, c1 = cn->counter;
		bool step = c1 == c0 + 1 && c1 < 0x4C;

		// next tick's root (the drops / slides / hops are computed before the draws)
		int32_t ny = Root(cn).t[1], nz = Root(cn).t[2];
		if (step) RootAt(c1, ny, nz);

		if (M.ring)
		{
			int32_t y = M.ring_y, z = M.ring_z, angle = M.ring_angle, fade = M.ring_fade;
			int32_t na, nf;
			if (step && RingAt(c1, na, nf))
			{
				y = lerp_i(y, ny, num, den);
				z = lerp_i(z, nz, num, den);
				angle = lerp_i(angle, na, num, den);
				fade = lerp_i(fade, nf, num, den);
			}
			DrawRing(M.ring_x, y, z, angle, fade, MODEL_Ring);
		}

		if (M.model)
		{
			uint32_t size = 0;
			uint8_t *sk = Skeleton(cn, &size);
			if (sk)
			{
				Mat4x3 root_save = Root(cn);
				memcpy(g_skel_save, sk, size);
				bool next = step && c1 < 0x44;
				const PoseMemo &drawn = g_built[(g_real_tick - 1) & 1], &built = g_built[g_real_tick & 1];
				bool blend = next && !M.restart && drawn.ok && drawn.tick == g_real_tick - 1 && drawn.size == size
					&& built.ok && built.tick == g_real_tick && built.size == size;
				Root(cn).t[1] = next ? lerp_i(M.root_y, ny, num, den) : M.root_y;
				Root(cn).t[2] = next ? lerp_i(M.root_z, nz, num, den) : M.root_z;
				if (blend)
				{
					memcpy(sk, built.pose, size);
					PoseBlend(sk, drawn.pose, num, den);
					BuildBoneMatricesFromPose(cn->model + 0x60);
				}
				else if (M.s0_ok && M.s0_size == size)
					memcpy(sk, M.s0, size); // the matrices drawn this tick, as they were
				PacketCursor() = x::DrawModel(cn->model, ModelBuffer() + MB_SCRATCH, PacketCursor(), PacketCursor2());
				memcpy(sk, g_skel_save, size);
				Root(cn) = root_save;
			}
		}

		if (M.sparks)
		{
			// as drawn (flipbook frame of the drawn life), positions towards the moved ones
			const Spark *cur = Sparks(ModelBuffer());
			memcpy(g_held_pool, M.pool, sizeof(g_held_pool));
			for (int i = 0; i < 0x40; i++)
			{
				Spark &s = g_held_pool[i];
				if (s.life == 0) continue;
				for (int k = 0; k < 3; k++) s.pos[k] = Lerp16(s.pos[k], cur[i].pos[k], num, den);
			}
			SparksDraw(g_held_pool);
		}
	}

	static void PrimsHeld(int num, int den)
	{
		if (g_prim_tick != g_real_tick) return;
		for (int i = 0; i < g_prim_n; i++)
		{
			PrimNode *p = g_prim_run[i].node;
			uint8_t ctx[0x28];
			PrimCtx(p, ctx);
			// in between the records drawn and the next ones; a finished player holds its last
			prim::play_held((prim::Layout *)p->layout, (prim::Callback)CB_PrimObject, (int)ctx, num, den);
		}
	}

	static void DebrisHeld(int num, int den)
	{
		if (g_debris_tick != g_real_tick) return;
		for (int i = 0; i < g_debris_n; i++)
		{
			const DebrisRun &r = g_debris_run[i];
			int16_t pos[3] = { r.pos[0], r.pos[1], r.pos[2] };
			int32_t rx = DebrisAngle(r.rot[0]), ry = DebrisAngle(r.rot[1]);
			if (!r.ended) // the last tick's chunk holds (vanilla shows it until the next tick)
			{
				const DebrisNode *d = r.node;
				for (int k = 0; k < 3; k++) pos[k] = Lerp16(r.pos[k], d->pos[k], num, den);
				rx += (int8_t)(d->rot[0] - r.rot[0]) * 16 * num / den;
				ry += (int8_t)(d->rot[1] - r.rot[1]) * 16 * num / den;
			}
			DebrisDraw(pos, rx, ry, r.size);
		}
	}

	static uint8_t g_held_packets[0x60000]; // cursor arena 0x40000, then the "next arena" argument
	static uint8_t g_scratch_save[MB_SCRATCH_SIZE];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	// mirrors the master's queue order (creature: ring, Moomba, sparks; prims; debris); packets go
	// to a private buffer (both module cursors redirected and put back); the model-buffer scratch
	// the model draw and the prim morphs write is saved and put back
	static void HeldFrame(int num, int den)
	{
		uint8_t *MB = ModelBuffer();
		if (!MB) return;
		uint32_t cursor = PacketCursor(), cursor2 = PacketCursor2();
		memcpy(g_scratch_save, MB + MB_SCRATCH, sizeof(g_scratch_save));
		PacketCursor() = (uint32_t)g_held_packets;
		PacketCursor2() = (uint32_t)g_held_packets + 0x40000;

		CreatureHeld(num, den);
		PrimsHeld(num, den);
		DebrisHeld(num, den);

		memcpy(MB + MB_SCRATCH, g_scratch_save, sizeof(g_scratch_save));
		PacketCursor() = cursor;
		PacketCursor2() = cursor2;
	}
}

	void register_mag338_moomba()
	{
		register_port(m338::ORIG_SequenceTask, (void *)m338::SequenceTask, "M338 SequenceTask", 338);
		register_port(m338::ORIG_CreatureTask, (void *)m338::CreatureTask, "M338 CreatureTask", 338, true);
		register_port(m338::ORIG_PrimTask, (void *)m338::PrimTask, "M338 PrimTask", 338, true);
		register_port(m338::ORIG_DebrisTask, (void *)m338::DebrisTask, "M338 DebrisTask", 338, true);
		register_module_held(338, m338::HeldReady, m338::HeldFrame);
	}
}
