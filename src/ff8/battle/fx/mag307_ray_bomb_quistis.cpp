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

// Effect 307: Ray-Bomb (Quistis' Blue Magic, kernel.bin section 19 "Ray-Bomb", magic +4 = 307;
// MAG_307_RAY_BOMB_QUISTIS*), cast by a party member (Quistis, her whip) on the enemies.
//
// The module 0x5A1150..0x5A2C70 adapts enemy Ray-Bomb 10 (mag010_ray_bomb.cpp, X-ATM092's attack):
// 9 of its 15 functions are identical up to addresses (setup, root, restore, debris, ring, puff, beam,
// spark; exe data -0x109458, module globals -0x2882D0 / -0x288280, the root pool / queue -0x2882F0);
// the director is rewritten and five functions differ:
//   Director (0x5A1220) - every tick (also in the draw-only mode, before its test) the muzzle 0x2242580
//     = the caster's bone 0xF0 matrix (MAG_307_sub_5A2AC0) through the caster's world matrix, plus
//     (0xDAC, 0, -1000) in that frame: the muzzle FOLLOWS Quistis' animating hand (10: taken once at 0
//     from the effect anchor). Timeline: 0 targets' centre, node +0x16 = caster size * 1900 / 4096
//     (sprite / flash spread); 1 the caster's animation set (com data 0xCDF86C, out block 0x2240F18)
//     bound (BS_BindEntityModelFromComData) + chain transformation 3 (Quistis' casting animation);
//     0xF / 0x20 the caster's flags bit 8 on / off; 0xC two charge glows; 0xC..0x1D two dust particles
//     a tick from each of the 2 muzzle table entries; 0x1B muzzle flash; 0x25 the beam; 0x3E..0x43 1-2
//     smoke puffs a tick at 5 spots (+ at 0x3E the texture restore task in the root queue); 0x54
//     shockwave ring; 0x54..0x57 2-3 debris a tick at 5 spots; 0x55 one bounce task per TARGET (enemy);
//     screen flash ramps 0..8 / 0x7F..0x87; sounds at 0xA (muzzle) and 0x51 (centre); ends after 0x87.
//   Flash (0x5A23E0) - camera-facing size node +0x16 (10: scale / 8); fades out from 0x16, shrinks
//     until 0xA, ends at 0x1C (10: 0x1E / 0x12 / 0x24).
//   Charge (0x5A24D0) - camera-facing size node +0x16 / 2 (10: scale / 16); fades out from 0xE (level
//     0x60 - 16 c), ends at 0x16 (10: 0x16 / 0xE0 / 0x1E).
//   Bounce (0x5A25E0) - one task per target: no chain transformation at 24, no wait on the entity's
//     state; at 32 the entity is restored and its action result applied.
//   Particles (0x5A2B50) - a particle at frame 0 is not drawn (its sequence end flag cleared); camera
//     size / 8 (10: / 16); grows by 1/32 a tick, slows by 1/8 (10: -, 1/16).
//   MAG_307_sub_5A2AC0 (new) - copies an entity skeleton bone matrix (0xF0.. = effect bone table).
//
// Structure (setup MAG_307_RAY_BOMB_QUISTIS 0x5A1170, file loader MAG_307_RAY_BOMB_QUISTIS_FL 0x5A1150
// = the texture named at 0xCE4158 into 0x2242578; the setup keeps the context and the caster slot,
// sets up the root queue 0x2240F08 (pool 2 x 0x10) and the effect queue 0x22416B8 (pool 100 x 0x24),
// spawns the root and the director, starts camera animation 0xCDF6D8, queues the TIM and clears the 80
// dust particles):
//   RootTask (0x5A2B00) - alternates the packet arena (magic buffer / + 0x8000), runs the effect
//     queue, then the dust particles; ends when both are empty.
//   Debris (0x5A1AE0) - prim model (table 0xCE4128) tumbling up and falling back, fades at the end.
//   Ring (0x5A1C50) - prim model 0xCE190C stretched upwards, fades from 10, ends at 24.
//   Puff (0x5A1D90) - sprite 0xCE158C after a delay, darkening, ends after its life.
//   Beam (0x5A1EE0) - prim model 0xCE3BB4 from the muzzle to its end point, rolled to face the
//     camera, stretched to the distance (half length at counter 1), spinning; from counter 2 spawns
//     3-7 sparks a tick at its end; fades from 3, ends at 10.
//   Spark (0x5A2340) - flipbook 0xCE15B4 rising at a constant speed.
//   Flash (0x5A23E0) - prim model 0xCE350C at the muzzle, shrinking, fades in / out.
//   Charge (0x5A24D0) - flipbook 0xCE13E0 at the muzzle after a delay, growing, fades in / out.
//   Bounce (0x5A25E0) - one target: saved (flags & 0x1020, rotation words +0x0C..+0x13, position
//     +0x1C..+0x23 in 0x22424D8 + slot * 0x14), spun and thrown up around its effect position for 24
//     ticks then bounced for 8, 16 dust particles at 24 and 12 at 31; at 32 restored + damage.
//   Restore (0x5A2A80) - engine texture restore task (magic buffer + 0x10000), ends when done.
//   Particles (0x5A2B50) - 80 dust sprites 0xCE1760 in 0x2240F38 (0x18 each).
// Every task tests the draw-only flags (battle_to_update_flags 0x201): the director computes the muzzle
// and does nothing else, the restore does nothing, the others draw and do not update.
// Entity writes: caster - animation set and chain transformation (counter 1, through the engine), flags
// bit 8 (0xF..0x1F); targets - flags 0x1020, rotation +0x0C..+0x13 and position +0x1C..+0x23 (Bounce,
// restored), their action results (Bounce counter 32, engine). Camera: the setup's camera animation
// 0xCDF6D8 (engine); no camera word is written by the tasks. Stage: the screen flash level words.
// Module globals: 0x2240EE8..0x2242590 (root pool, root queue, the bind block 0x2240F18, particles,
// effect queue + pool, saved entity states 0x22424D8, targets' centre 0x2242568, context 0x2242570,
// caster slot 0x2242574, texture file, muzzle 0x2242580, magic buffer base 0x2242588, packet cursor
// 0x224258C).

#include "mag_common.h"

namespace ff8fx
{
namespace raybomb307
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline TaskQueue *QRoot() { return (TaskQueue *)0x2240F08; }
	inline TaskQueue *QEffect() { return (TaskQueue *)0x22416B8; }
	inline int16_t *Centre() { return (int16_t *)0x2242568; }             // targets' centre (x, y, z, pad)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2242570); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x2242574); }
	inline int16_t *Muzzle() { return (int16_t *)0x2242580; }             // x, y, z, pad
	inline uint32_t &TexBase() { return var<uint32_t>(0x2242588); }       // Magic_TextureOFF (magic buffer)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x224258C); }
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_Director = 0x5A1220;
	static const uint32_t ORIG_Debris = 0x5A1AE0;
	static const uint32_t ORIG_Ring = 0x5A1C50;
	static const uint32_t ORIG_Puff = 0x5A1D90;
	static const uint32_t ORIG_Beam = 0x5A1EE0;
	static const uint32_t ORIG_Spark = 0x5A2340;
	static const uint32_t ORIG_Flash = 0x5A23E0;
	static const uint32_t ORIG_Charge = 0x5A24D0;
	static const uint32_t ORIG_Bounce = 0x5A25E0;
	static const uint32_t ORIG_Restore = 0x5A2A80;
	static const uint32_t ORIG_RootTask = 0x5A2B00;
	static const uint32_t ORIG_Particles = 0x5A2B50;   // not a task: called by the root

	static const uint32_t SEQ_Dust = 0xCE1760, SEQ_Puff = 0xCE158C, SEQ_Spark = 0xCE15B4, SEQ_Charge = 0xCE13E0;
	static const uint32_t MODEL_Ring = 0xCE190C, MODEL_Beam = 0xCE3BB4, MODEL_Flash = 0xCE350C;
	static const uint32_t TABLE_DebrisModel = 0xCE4128;   // 2 prim models
	static const uint32_t TABLE_DustOffset = 0xCE4138;    // 2 x SVECTOR: muzzle dust start offsets (pad words copied along)
	static const uint32_t TABLE_DustDir = 0xCE4148;       // 2 x SVECTOR: muzzle dust directions
	static const uint32_t TABLE_PuffX = 0xCE40D8, TABLE_PuffZ = 0xCE4100;     // 5 spots (dword stride)
	static const uint32_t TABLE_DebrisX = 0xCE40EC, TABLE_DebrisZ = 0xCE4114; // 5 spots (dword stride)
	static const void *const SOUND_Muzzle = (const void *)0xCE40CC;
	static const void *const SOUND_Blast = (const void *)0xCE40D0;
	static const uint32_t BIND_Block = 0x2240F18;                      // 0x20-byte block BS_BindEntityModelFromComData fills
	static const uint32_t COM_Anim = 0xCDF86C;                         // the module's animation set (com data)

	// engine functions not in fx_port.h / mag_common.h
	inline void BindEntityModelFromComData(uint8_t *entity, uint32_t out, uint32_t data) { fn<void (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData
	inline void CalculateCenterPosition(uint32_t mask, int16_t *out) { fn<void (__cdecl *)(uint32_t, int16_t *)>(0x5020A0)(mask, out); }
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); } // matrixMultiplyVector (rotation only)
	inline int32_t NormalizeSVector(const int16_t *in, int16_t *out) { return fn<int32_t (__cdecl *)(const int16_t *, int16_t *)>(0x56BDE0)(in, out); } // sub_56BDE0
	inline int32_t NormalizeVectorLen2(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // returns x*x + y*y + z*z
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); } // GTE_MatrixMultiply
	inline void QueueChainTransformation(uint8_t *entity, int32_t id) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, id); }

	// ------------------------------------------------------------------
	// Node layouts (effect queue: pool of 100 nodes of 0x24 bytes; root queue: 2 x 0x10)
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode { TaskNode hdr; uint16_t counter; uint16_t pad; };
	struct DirectorNode { TaskNode hdr; int16_t counter; uint8_t pad0E[8]; int16_t spread; uint8_t pad18[0xC]; }; // +0x16: the caster's size * 1900 / 4096
	struct RestoreNode { TaskNode hdr; int16_t counter; int16_t done; };
	struct DebrisNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t life;      // +0x0E fades over the last 8 ticks
		int16_t pos[3];    // +0x10
		int16_t vy;        // +0x16 slows by 1/9 a tick
		int16_t model;     // +0x18 index into 0xCE4128
		int16_t scale;     // +0x1A
		int16_t rx;        // +0x1C angles (x, 0, z)
		int16_t rx_step;   // +0x1E
		int16_t rz;        // +0x20
		int16_t rz_step;   // +0x22
	};
	struct RingNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		uint8_t pad16[6];
		int16_t sxz;       // +0x1C scale x / z
		int16_t pad1E;
		int16_t sy;        // +0x20 scale y
		int16_t sy_step;   // +0x22 halves a tick
	};
	struct PuffNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t delay;     // +0x0E
		int16_t pos[3];    // +0x10
		int16_t pad16;
		int16_t angle;     // +0x18 y rotation
		int16_t pad1A;
		int16_t scale;     // +0x1C
		int16_t pad1E;
		int16_t life;      // +0x20
		int16_t pad22;
	};
	struct BeamNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[3];    // +0x10 end point
		int16_t pad16;
		int16_t angle;     // +0x18 spin
		int16_t angle_step;// +0x1A grows by 1/16 a tick
		int16_t pad1C[2];
		int16_t scale;     // +0x20 width
		int16_t pad22;
	};
	struct SparkNode
	{
		TaskNode hdr;
		int16_t frame;     // +0x0C
		int16_t life;      // +0x0E
		int16_t pos[3];    // +0x10
		int16_t vy;        // +0x16
		uint8_t pad18[4];
		int16_t scale;     // +0x1C
		uint8_t pad1E[6];
	};
	struct FlashNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E[8];
		int16_t spread;    // +0x16 camera-facing size (the director's)
		uint8_t pad18[4];
		int16_t scale;     // +0x1C
		int16_t scale_step;// +0x1E grows by 1/32 a tick
		uint8_t pad20[4];
	};
	struct ChargeNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t delay;     // +0x0E
		uint8_t pad10[6];
		int16_t spread;    // +0x16 camera-facing size x 2 (the director's)
		int16_t size;      // +0x18 sprite header +0x08
		int16_t pad1A;
		int16_t scale;     // +0x1C
		int16_t scale_step;// +0x1E
		int16_t frame;     // +0x20
		int16_t pad22;
	};
	struct BounceNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t slot;      // +0x0E target entity
		int16_t pos[4];    // +0x10 its default effect position (taken at 0)
		int16_t spin;      // +0x18 y rotation reached at 24 (x 24)
		int16_t target;    // +0x1A index in the action's targets
		uint8_t pad1C[4];
		int16_t amp;       // +0x20 throw height (negative = up)
		int16_t pad22;
	};
	struct Particle // 0x2240F38, 80 of them
	{
		uint32_t flag;     // +0x00 bit0 = in use
		int16_t counter;   // +0x04 flipbook frame
		int16_t scale;     // +0x06
		int16_t pos[3];    // +0x08
		int16_t pad0E;
		int16_t vel[3];    // +0x10
		int16_t pad16;
	};
	struct SavedEntity // 0x22424D8 + slot * 0x14
	{
		uint16_t flags;    // entity +0x00 & 0x1020
		uint16_t pad;
		uint32_t rot0;     // entity +0x0C
		uint32_t rot1;     // entity +0x10
		uint32_t pos0;     // entity +0x1C
		uint32_t pos1;     // entity +0x20
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(DirectorNode) == 0x24 && sizeof(DebrisNode) == 0x24 && sizeof(RingNode) == 0x24, "Ray-Bomb (Quistis) nodes");
	static_assert(sizeof(PuffNode) == 0x24 && sizeof(BeamNode) == 0x24 && sizeof(SparkNode) == 0x24 && sizeof(FlashNode) == 0x24, "Ray-Bomb (Quistis) nodes");
	static_assert(sizeof(ChargeNode) == 0x24 && sizeof(BounceNode) == 0x24, "Ray-Bomb (Quistis) nodes");
	static_assert(sizeof(Particle) == 0x18 && sizeof(SavedEntity) == 0x14, "Ray-Bomb particles / saved entities");

	inline Particle *Particles() { return (Particle *)0x2240F38; }
	static const int PARTICLES = 80;
	inline SavedEntity *Saved(int slot) { return (SavedEntity *)(0x22424D8 + 0x14 * slot); }
}
}

#ifdef FF8_FX_HELD
#include "mag307_ray_bomb_quistis_held.h"
#endif

namespace ff8fx
{
namespace raybomb307
{
	// first free dust particle (flag 0), or -1
	static int FreeParticle()
	{
		for (int i = 0; i < PARTICLES; i++)
			if (Particles()[i].flag == 0) return i;
		return -1;
	}

	// ------------------------------------------------------------------
	// Dust particles (0x5A2B50): draw every particle in use (not at frame 0), then drift / slow it down / grow
	// ------------------------------------------------------------------
	static void ParticleDraw(uint8_t *h, const Particle *p)
	{
		*(int16_t *)(h + 4) = p->counter;
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 3));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
	}

	static uint32_t ParticlePass()
	{
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_particles();)
		uint8_t *h = AllocHeader(0xB4);
		uint32_t count = 0;
		*(uint32_t *)h = SEQ_Dust;
		*(int16_t *)(h + 0x24) = 0;
		for (int i = 0; i < PARTICLES; i++)
		{
			Particle *p = &Particles()[i];
			if (!(p->flag & 1)) continue;
			// a particle at frame 0 is not drawn (its sequence end flag cleared)
			if (p->counter != 0) ParticleDraw(h, p);
			else *(int16_t *)(h + 0x28) = 0;
			if (DrawOnly()) continue;
			p->counter++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				p->flag = 0;
				continue;
			}
			p->scale = (int16_t)(p->scale + (int16_t)(p->scale >> 5));
			p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
			p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
			p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
			p->vel[0] = (int16_t)(p->vel[0] - (int16_t)(p->vel[0] >> 3));
			p->vel[1] = (int16_t)(p->vel[1] - (int16_t)(p->vel[1] >> 3));
			count++;
			p->vel[2] = (int16_t)(p->vel[2] - (int16_t)(p->vel[2] >> 3));
		}
		FieldFree(0xB4);
		return count;
	}

	// ------------------------------------------------------------------
	// Root task (0x5A2B00): node +0x0C counter
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_root();)
		if (r->counter & 1) PacketCursor() = TexBase() + 0x8000;
		else PacketCursor() = TexBase();
		uint32_t alive = (uint32_t)ExecuteTaskQueue(QEffect());
		uint32_t dust = ParticlePass();
		r->counter++;
		return (dust | alive) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Restore (0x5A2A80): engine texture restore task, ends when it says done
	// ------------------------------------------------------------------
	static uint32_t __cdecl RestoreTask(TaskNode *n)
	{
		RestoreNode *t = (RestoreNode *)n;
		if (t->counter == 0)
		{
			t->done = 0;
			TextureRestoreTask((void *)(TexBase() + 0x10000), &t->done);
		}
		int16_t done = t->done;
		t->counter++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Debris (0x5A1AE0): prim model, angles (rx, 0, rz), uniform scale
	// ------------------------------------------------------------------
	static void DebrisHeader(uint8_t *h, const DebrisNode *p)
	{
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint32_t *)h = var<uint32_t>(TABLE_DebrisModel + 4 * p->model);
		if (p->counter >= p->life - 8)
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(int32_t *)(h + 0xC) = shl32(p->counter - p->life + 8, 9);
			*(uint32_t *)(h + 0x1C) = 0xC3;
		}
	}

	static void DebrisDraw(const DebrisNode *p)
	{
		int16_t angles[4] = { p->rx, 0, p->rz, 0 };
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(angles, &m);
		m.t[0] = p->pos[0];
		m.t[2] = p->pos[2];
		m.t[1] = p->pos[1];
		int32_t s[3] = { p->scale, p->scale, p->scale };
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x58);
		DebrisHeader(h, p);
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_fix_header(h);)
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// rise / fall: y += vy, vy -= vy / 9; tumble
	static void DebrisUpdate(DebrisNode *p)
	{
		int16_t v = p->vy;
		p->pos[1] = (int16_t)(p->pos[1] + v);
		p->rz = (int16_t)(p->rz + p->rz_step);
		p->vy = (int16_t)(v - v / 9);
		p->rx = (int16_t)(p->rx + p->rx_step);
		p->counter++;
	}

	static uint32_t __cdecl DebrisTask(TaskNode *n)
	{
		DebrisNode *p = (DebrisNode *)n;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_draw(ORIG_Debris, p);)
		DebrisDraw(p);
		if (DrawOnly()) return 0;
		DebrisUpdate(p);
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter >= p->life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ring (0x5A1C50): prim model 0xCE190C, no rotation, scale (sxz, sy, sxz)
	// ------------------------------------------------------------------
	static void RingHeader(uint8_t *h, const RingNode *p)
	{
		*(uint32_t *)h = MODEL_Ring;
		*(uint32_t *)(h + 0x1C) = 0x30;
		if (p->counter >= 10)
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0xF0;
			*(int32_t *)(h + 0xC) = (p->counter - 10) * 0x124;
		}
	}

	static void RingDraw(const RingNode *p)
	{
		int16_t angles[4] = { 0, 0, 0, 0 };
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(angles, &m);
		m.t[1] = p->pos[1];
		m.t[0] = p->pos[0];
		m.t[2] = p->pos[2];
		int32_t s[3] = { p->sxz, p->sy, p->sxz };
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x58);
		RingHeader(h, p);
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_fix_header(h);)
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void RingUpdate(RingNode *p)
	{
		int16_t v = p->sy_step;
		p->sy = (int16_t)(p->sy + v);
		p->counter++;
		p->sy_step = (int16_t)(v - (int16_t)(v >> 1));
	}

	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *p = (RingNode *)n;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_draw(ORIG_Ring, p);)
		RingDraw(p);
		if (DrawOnly()) return 0;
		RingUpdate(p);
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter >= 0x18 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Puff (0x5A1D90): sprite 0xCE158C turned (0x400, angle, 0), darkening
	// ------------------------------------------------------------------
	static void PuffHeader(uint8_t *h, const PuffNode *p)
	{
		*(uint16_t *)(h + 4) = 0;
		*(uint32_t *)h = SEQ_Puff;
		int32_t q = 0x80 / (int32_t)p->life;
		*(uint16_t *)(h + 0x24) = 0xC;
		uint8_t level = (uint8_t)(0x80 - (uint8_t)(q * p->counter));
		h[0x1E] = level;
		h[0x1D] = level;
		h[0x1C] = level;
	}

	static void PuffDraw(const PuffNode *p)
	{
		int16_t angles[4] = { 0x400, p->angle, 0, 0 };
		Mat4x3 m = {};
		ComposeZYXRotationMatrix(angles, &m);
		m.t[0] = p->pos[0];
		m.t[1] = p->pos[1];
		m.t[2] = p->pos[2];
		int32_t s[3] = { p->scale, p->scale, p->scale };
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0xB4);
		PuffHeader(h, p);
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_fix_header(h);)
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl PuffTask(TaskNode *n)
	{
		PuffNode *p = (PuffNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_draw(ORIG_Puff, p);)
		PuffDraw(p);
		if (DrawOnly()) return 0;
		p->counter++;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter >= p->life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Beam (0x5A1EE0): prim model 0xCE3BB4 from the muzzle towards pos, rolled about its axis so
	// that it faces the camera eye (0xB8B7F0), length = distance; spark spawn point in `end`
	// ------------------------------------------------------------------
	static void BeamHeader(uint8_t *h, const BeamNode *p)
	{
		*(uint32_t *)h = MODEL_Beam;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (p->counter >= 3)
		{
			int32_t c = p->counter - 3;
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = c * 0x2AA;
		}
	}

	static void BeamDraw(const BeamNode *p, int16_t end[4])
	{
		int32_t down[3] = { 0, 0, -0x1000 };
		int16_t angles[4] = { 0, p->angle, 0, 0 };
		Mat4x3 m1 = {};
		BuildRotationMatrixFromAngles(angles, &m1);
		// axis / first cross product / second axis share one stack slot in the original
		int32_t d[3], dir[3], axis[3];
		d[0] = p->pos[0] - Muzzle()[0];
		d[1] = p->pos[1] - Muzzle()[1];
		d[2] = p->pos[2] - Muzzle()[2];
		int32_t len2 = NormalizeVectorLen2(d, dir);
		TransformVectorBy3x3Matrix(&m1, dir, dir);
		int32_t angle = RotationBetweenVectors(down, dir, axis);
		BuildAxisAngleRotationMatrix(angle, &m1, axis);
		m1.t[0] = Muzzle()[0];
		m1.t[1] = Muzzle()[1];
		m1.t[2] = Muzzle()[2];
		// the model's "up" in the beam frame
		down[0] = 0;
		down[1] = -0x1000;
		down[2] = 0;
		TransformVectorBy3x3Matrix(&m1, down, down);
		int32_t eye[3];
		eye[0] = var<int16_t>(0xB8B7F0) - Muzzle()[0];
		eye[1] = var<int16_t>(0xB8B7F2) - Muzzle()[1];
		eye[2] = var<int16_t>(0xB8B7F4) - Muzzle()[2];
		NormalizeVectorLen2(eye, eye);
		int32_t c2[3];
		CrossProduct(dir, eye, axis);
		CrossProduct(eye, axis, c2);
		angle = RotationBetweenVectors(down, c2, axis);
		int32_t dot = mul32(dir[2], axis[2]) + mul32(dir[1], axis[1]);
		dot += mul32(dir[0], axis[0]);
		if (dot < 0) angle = -angle;
		Mat4x3 m2 = {};
		BuildAxisAngleRotationMatrix(angle, &m2, dir);
		MatrixMultiply(&m2, &m1);
		int32_t len = Sqrt(len2);
		// spark spawn point: muzzle + dir * distance (4th word = the muzzle's pad word)
		memcpy(end, Muzzle(), 8);
		end[0] = (int16_t)(end[0] + (mul32(dir[0], len) >> 12));
		end[1] = (int16_t)(end[1] + (mul32(dir[1], len) >> 12));
		end[2] = (int16_t)(end[2] + (mul32(dir[2], len) >> 12));
		const int32_t full = shl32(len, 12) / 1046;
		int32_t stretch = full;
		if (p->counter < 2) stretch = (full / 2) * p->counter;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_fix_stretch(&stretch, full);)
		int32_t s[3] = { p->scale, p->scale, stretch };
		Scale3DMatrix(&m1, s);
		ComposeAffineTransform(&Camera(), &m1, &m1);
		GteSetRotMatrix(&m1);
		GteSetTransVector(&m1);
		uint8_t *h = AllocHeader(0x58);
		BeamHeader(h, p);
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_fix_header(h);)
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void BeamSpin(BeamNode *p)
	{
		int16_t v = p->angle_step;
		p->angle = (int16_t)(p->angle + v);
		p->angle_step = (int16_t)(v + (int16_t)(v >> 4));
	}

	static uint32_t __cdecl BeamTask(TaskNode *n)
	{
		BeamNode *p = (BeamNode *)n;
		int16_t end[4];
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_draw(ORIG_Beam, p);)
		BeamDraw(p, end);
		if (DrawOnly()) return 0;
		BeamSpin(p);
		if (p->counter >= 2)
		{
			int32_t k = CrtRand() % 5 + 3;
			if (k > 0)
			{
				do
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QEffect(), ORIG_Spark);
					s->frame = 0;
					int32_t r = CrtRand() & 7;
					memcpy(s->pos, end, 4);
					s->life = (int16_t)(r + 8);
					memcpy(&s->pos[2], &end[2], 4);
					r = CrtRand() % 1000;
					s->pos[1] = 0;
					s->pos[0] = (int16_t)(s->pos[0] + (r - 500));
					r = CrtRand() % 900;
					s->pos[2] = (int16_t)(s->pos[2] + (r - 400));
					r = CrtRand() % 20;
					s->vy = (int16_t)(-10 - r);
					r = CrtRand() % 0x700;
					s->scale = (int16_t)(r + 0xC00);
				} while (--k);
			}
		}
		p->counter++;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter > 9 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Spark (0x5A2340): flipbook 0xCE15B4 rising at a constant speed
	// ------------------------------------------------------------------
	static void SparkDraw(const SparkNode *p)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 3));
		uint8_t *h = AllocHeader(0xB4);
		*(uint16_t *)(h + 4) = (uint16_t)p->frame;
		*(uint32_t *)h = SEQ_Spark;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *p = (SparkNode *)n;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_draw(ORIG_Spark, p);)
		SparkDraw(p);
		if (DrawOnly()) return 0;
		p->pos[1] = (int16_t)(p->pos[1] + p->vy);
		p->frame++;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_after(p);)
		return p->frame >= p->life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Flash (0x5A23E0): prim model 0xCE350C at the muzzle (camera facing, size node +0x16), fades in then out
	// ------------------------------------------------------------------
	static void FlashHeader(uint8_t *h, const FlashNode *p)
	{
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint32_t *)h = MODEL_Flash;
		if (p->counter < 0x10)
		{
			*(int32_t *)(h + 0xC) = (0x10 - p->counter) << 8;
			*(uint32_t *)(h + 0x1C) = 0xC0;
		}
		else if (p->counter >= 0x16)
		{
			*(int32_t *)(h + 0xC) = (p->counter - 0x16) * 0x2AA;
			*(uint32_t *)(h + 0x1C) = 0xC0;
		}
	}

	static void FlashDraw(const FlashNode *p)
	{
		TransformCameraByShadowRotation(Muzzle(), p->scale, -p->spread);
		uint8_t *h = AllocHeader(0x58);
		FlashHeader(h, p);
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_fix_header(h);)
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void FlashUpdate(FlashNode *p)
	{
		if (p->counter < 0xA)
		{
			int16_t v = p->scale_step;
			p->scale = (int16_t)(p->scale - v);
			p->scale_step = (int16_t)(v + (int16_t)(v >> 5));
		}
		p->counter++;
	}

	static uint32_t __cdecl FlashTask(TaskNode *n)
	{
		FlashNode *p = (FlashNode *)n;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_draw(ORIG_Flash, p);)
		FlashDraw(p);
		if (DrawOnly()) return 0;
		FlashUpdate(p);
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter >= 0x1C ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Charge (0x5A24D0): flipbook 0xCE13E0 at the muzzle (camera facing, size node +0x16 / 2), growing, fades in (0..7) / out (14..)
	// ------------------------------------------------------------------
	static void ChargeHeader(uint8_t *h, const ChargeNode *p)
	{
		*(int32_t *)(h + 8) = p->size;
		*(uint32_t *)h = SEQ_Charge;
		*(int16_t *)(h + 4) = p->frame;
		*(uint16_t *)(h + 0x24) = 1;
		if (p->counter < 8 || p->counter >= 0xE)
		{
			uint8_t level = p->counter < 8 ? (uint8_t)(p->counter << 4) : (uint8_t)(0x60 - (uint8_t)(p->counter << 4));
			h[0x1E] = level;
			h[0x1D] = level;
			h[0x1C] = level;
			*(uint16_t *)(h + 0x24) = 5;
		}
	}

	static void ChargeDraw(const ChargeNode *p, uint8_t **header)
	{
		TransformCameraByShadowRotation(Muzzle(), p->scale, -(p->spread >> 1));
		uint8_t *h = AllocHeader(0xB4);
		ChargeHeader(h, p);
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_fix_header(h);)
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
		*header = h;
	}

	static uint32_t __cdecl ChargeTask(TaskNode *n)
	{
		ChargeNode *p = (ChargeNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		uint8_t *h;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_draw(ORIG_Charge, p);)
		ChargeDraw(p, &h);
		if (DrawOnly()) return 0;
		// the flipbook restarts when it has ended (header +0x28 negative; read after the free, the
		// scratch is not reused in between)
		if (*(int16_t *)(h + 0x28) < 0) p->frame = 0;
		else p->frame++;
		p->scale = (int16_t)(p->scale + p->scale_step);
		p->counter++;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter >= 0x16 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Bounce (0x5A25E0): throws one target (see the module comment)
	// ------------------------------------------------------------------
	static void BounceDust(const BounceNode *b, int n, int32_t scale_mod, int32_t scale_add, int32_t speed_mod, int32_t speed_add, bool mask_scale)
	{
		for (int k = 0; k < n; k++)
		{
			int i = FreeParticle();
			if (i < 0) return;
			Particle *p = &Particles()[i];
			p->flag = 1;
			p->counter = 0;
			int32_t r = CrtRand();
			r = mask_scale ? (r & 0x3FF) : (r % scale_mod);
			p->scale = (int16_t)(r + scale_add);
			p->pos[0] = b->pos[0];
			p->pos[1] = 0;
			p->pos[2] = b->pos[2];
			int32_t a = CrtRand() & 0xFFF;
			int32_t speed = CrtRand() % speed_mod + speed_add;
			p->vel[0] = (int16_t)(mul32(ComputeCos(a), speed) >> 12);
			p->vel[1] = 0;
			p->vel[2] = (int16_t)(mul32(ComputeSin(a), speed) >> 12);
		}
	}

	static uint32_t __cdecl BounceTask(TaskNode *n)
	{
		BounceNode *b = (BounceNode *)n;
		const int slot = b->slot;
		uint8_t *e = Entity(slot);
		SavedEntity *sv = Saved(slot);
		if (b->counter == 0)
		{
			GetDefaultEffectPosition(e, b->pos);
			sv->flags = (uint16_t)(*(uint16_t *)e & 0x1020);
			sv->pos0 = *(uint32_t *)(e + 0x1C);
			sv->pos1 = *(uint32_t *)(e + 0x20);
			sv->rot0 = *(uint32_t *)(e + 0x0C);
			sv->rot1 = *(uint32_t *)(e + 0x10);
		}
		if (b->counter <= 0x18)
		{
			// spun (y by spin * counter / 24, x by a half turn) and thrown up about its effect
			// position: position = p - R * (p - saved position) + amp * sin(counter / 24 half turn)
			int32_t a = shl32(b->counter, 11) / 24;
			*(uint16_t *)e |= 0x1020;
			int32_t lift = mul32(ComputeSin(a), b->amp);
			*(int16_t *)(e + 0x0C) = (int16_t)(mul32(b->spin, b->counter) / 24);
			*(int16_t *)(e + 0x0E) = (int16_t)(shl32(b->counter, 13) / 24);
			lift >>= 12;
			int16_t v[4];
			v[0] = (int16_t)(b->pos[0] - *(const int16_t *)&sv->pos0);
			v[1] = (int16_t)(b->pos[1] - *((const int16_t *)&sv->pos0 + 1));
			Mat4x3 m = {};
			v[2] = (int16_t)(b->pos[2] - *(const int16_t *)&sv->pos1);
			v[3] = 0;
			ComposeZYXRotationMatrix((const int16_t *)(e + 0x0C), &m);
			MatrixMulVector(&m, v, v);
			int16_t y = (int16_t)(b->pos[1] - v[1]);
			int16_t x = (int16_t)(b->pos[0] - v[0]);
			int16_t z = (int16_t)(b->pos[2] - v[2]);
			y = (int16_t)(y + lift);
			*(int16_t *)(e + 0x1C) = x;
			*(int16_t *)(e + 0x1E) = y;
			*(int16_t *)(e + 0x20) = z;
		}
		else if (b->counter <= 0x20)
		{
			// small bounce
			int32_t a = shl32(b->counter - 0x18, 11) / 8;
			*(uint16_t *)e |= 0x1020;
			*(int16_t *)(e + 0x1E) = (int16_t)(mul32(ComputeSin(a), b->amp >> 3) >> 12);
		}
		if (DrawOnly()) return 0;
		if (b->counter == 0x18) BounceDust(b, 16, 0, 0xA00, 90, 0x78, true);
		if (b->counter == 0x1F) BounceDust(b, 12, 0x300, 0x800, 60, 100, false);
		b->counter++;
		if (b->counter < 0x20) return 0;
		*(uint16_t *)e = (uint16_t)((*(uint16_t *)e & 0xEFDF) | sv->flags);
		*(uint32_t *)(e + 0x1C) = sv->pos0;
		*(uint32_t *)(e + 0x20) = sv->pos1;
		*(uint32_t *)(e + 0x0C) = sv->rot0;
		*(uint32_t *)(e + 0x10) = sv->rot1;
		ApplyActionResultToTarget(Ctx()->actions->targets + b->target * TARGET_STRIDE);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// MAG_307_sub_5A2AC0 (0x5A2AC0): the bone matrix of an entity's skeleton (anim header -> com data -> skeleton;
	// bones 0xF0.. through the skeleton's effect bone table at +4) into `out`
	// ------------------------------------------------------------------
	static void BoneMatrix(const uint8_t *anim, int32_t bone, Mat4x3 *out)
	{
		const uint8_t *skel = **(uint8_t *const *const *)(anim + 4);
		if (bone >= 0xF0) bone = skel[bone - 0xEC];
		memcpy(out, skel + bone * 0x30 + 0x20, 0x20);
	}

	// the muzzle (the director's first statements, every tick): the caster's bone 0xF0 through its world
	// matrix, plus (0xDAC, 0, -1000) in that frame
	static void ComputeMuzzle()
	{
		Mat4x3 m;
		int16_t v[4];
		uint8_t *caster = Entity(CasterSlot());
		BoneMatrix(caster + 0x60, 0xF0, &m);
		ComposeAffineTransform((const Mat4x3 *)(caster + 0x40), &m, &m);
		Muzzle()[0] = (int16_t)m.t[0];
		Muzzle()[1] = (int16_t)m.t[1];
		Muzzle()[2] = (int16_t)m.t[2];
		v[0] = 0xDAC;
		v[1] = 0;
		v[2] = (int16_t)0xFC18;
		// (v[3] is never written: the original adds the dword v[2] | v[3] << 16 to m.t[2] and keeps the low word)
		MatrixMulVector(&m, v, v);
		Muzzle()[0] = (int16_t)(v[0] + m.t[0]);
		Muzzle()[1] = (int16_t)(v[1] + m.t[1]);
		Muzzle()[2] = (int16_t)(v[2] + m.t[2]);
	}

	// ------------------------------------------------------------------
	// Director (0x5A1220): node +0xC counter, +0x16 the caster's size * 1900 / 4096 (counter 0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(held_note_director(d);)
		ComputeMuzzle();
		if (DrawOnly()) return 0;
		uint8_t *caster = Entity(CasterSlot());
		const Mat4x3 *crot = (const Mat4x3 *)(caster + 0x40);
		if (d->counter == 1)
		{
			BindEntityModelFromComData(caster, BIND_Block, COM_Anim);
			QueueChainTransformation(Entity(CasterSlot()), 3);
		}
		if (d->counter == 0xF) caster[0] |= 8;
		if (d->counter == 0x20) *(uint16_t *)caster &= 0xFFF7;
		if (d->counter == 0)
		{
			CalculateCenterPosition(Ctx()->target_mask, Centre());
			d->spread = (int16_t)(mul32(*(const int16_t *)(Entity(CasterSlot()) + 0x26), 1900) >> 12);
		}
		if (d->counter == 0xC)
		{
			for (int k = 0; k < 2; k++)
			{
				ChargeNode *c = (ChargeNode *)AddTaskToQueue(QEffect(), ORIG_Charge);
				c->counter = 0;
				c->delay = (int16_t)(CrtRand() % 6 + k);
				c->spread = d->spread;
				int32_t r = CrtRand() % 0x320;
				c->scale = 0xA00;
				c->scale_step = 0x40;
				c->frame = 0;
				c->size = (int16_t)(r + 0x190);
			}
		}
		if (d->counter >= 0xC && d->counter <= 0x1D)
		{
			// two dust particles from each of the 2 muzzle table entries
			for (int k = 0; k < 2; k++)
			{
				for (int j = 0; j < 2; j++)
				{
					int i = FreeParticle();
					if (i < 0) break;
					Particle *p = &Particles()[i];
					p->flag = 1;
					p->counter = 0;
					int32_t r = CrtRand() % 0x300;
					int16_t off[4];
					memcpy(off, (const void *)(TABLE_DustOffset + 8 * k), 8);
					p->scale = (int16_t)(r + 0x200);
					MatrixMulVector(crot, off, off);
					memcpy(p->pos, Muzzle(), 8);
					p->pos[0] = (int16_t)(p->pos[0] + off[0]);
					p->pos[1] = (int16_t)(p->pos[1] + off[1]);
					p->pos[2] = (int16_t)(p->pos[2] + off[2]);
					int16_t dir[4];
					memcpy(dir, (const void *)(TABLE_DustDir + 8 * k), 8);
					MatrixMulVector(crot, dir, dir);
					NormalizeSVector(dir, dir);
					int32_t speed = CrtRand() % 100 + 0x3C;
					p->vel[0] = (int16_t)(mul32(dir[0], speed) >> 12);
					p->vel[1] = (int16_t)(mul32(dir[1], speed) >> 12);
					p->vel[2] = (int16_t)(mul32(dir[2], speed) >> 12);
				}
			}
		}
		if (d->counter == 0x1B)
		{
			FlashNode *f = (FlashNode *)AddTaskToQueue(QEffect(), ORIG_Flash);
			f->counter = 0;
			f->spread = d->spread;
			f->scale = 0x3333;
			f->scale_step = 0x3F0;
		}
		if (d->counter == 0x25)
		{
			BeamNode *b = (BeamNode *)AddTaskToQueue(QEffect(), ORIG_Beam);
			int16_t v[4];
			v[0] = (int16_t)(Centre()[0] - Muzzle()[0]);
			b->counter = 0;
			v[1] = 0;
			v[2] = (int16_t)(Centre()[2] - Muzzle()[2]);
			int16_t angles[4] = { 0, (int16_t)0xFC00, 0, 0 };
			Mat4x3 m;
			BuildRotationMatrixFromAngles(angles, &m);
			MatrixMulVector(&m, v, b->pos);
			b->pos[0] = (int16_t)(b->pos[0] + Muzzle()[0]);
			b->pos[2] = (int16_t)(b->pos[2] + Muzzle()[2]);
			b->pos[1] = Centre()[1];
			b->angle = 0x46;
			b->angle_step = 0xA0;
			b->scale = 0x800;
		}
		if (d->counter >= 0x3E && d->counter <= 0x43)
		{
			for (int k = 0; k < 5; k++)
			{
				int32_t cnt = (CrtRand() & 1) + 1;
				for (int j = cnt; j > 0; j--)
				{
					PuffNode *p = (PuffNode *)AddTaskToQueue(QEffect(), ORIG_Puff);
					p->counter = 0;
					int32_t r = CrtRand() & 7;
					memcpy(p->pos, Centre(), 4);
					p->delay = (int16_t)(r + cnt);
					memcpy(&p->pos[2], &Centre()[2], 4);
					r = CrtRand() % 0x320;
					p->pos[1] = 0;
					p->pos[0] = (int16_t)(p->pos[0] + (int16_t)((int16_t)r + var<int16_t>(TABLE_PuffX + 4 * k)) - 0x190);
					r = CrtRand() % 0x320;
					p->pos[2] = (int16_t)(p->pos[2] + (int16_t)((int16_t)r + var<int16_t>(TABLE_PuffZ + 4 * k)) - 0x12C);
					p->angle = (int16_t)(CrtRand() & 0xFFF);
					r = CrtRand() % 0xE00;
					p->scale = (int16_t)(r + 0x800);
					p->life = (int16_t)((CrtRand() & 3) + 4);
				}
			}
		}
		if (d->counter == 0x54)
		{
			RingNode *g = (RingNode *)AddTaskToQueue(QEffect(), ORIG_Ring);
			memcpy(g->pos, Centre(), 4);
			memcpy(&g->pos[2], &Centre()[2], 4);
			g->counter = 0;
			g->sxz = 0x3924;
			g->sy = 0;
			g->sy_step = 0x1C92;
		}
		if (d->counter >= 0x54 && d->counter <= 0x57)
		{
			for (int k = 0; k < 5; k++)
			{
				int32_t cnt = (CrtRand() & 1) + 2;
				int32_t scale = cnt * 400;
				for (int j = 0; j < cnt; j++)
				{
					DebrisNode *p = (DebrisNode *)AddTaskToQueue(QEffect(), ORIG_Debris);
					p->counter = 0;
					int32_t r = CrtRand() & 7;
					memcpy(p->pos, Centre(), 4);
					p->life = (int16_t)(r + j + 0xB);
					memcpy(&p->pos[2], &Centre()[2], 4);
					r = CrtRand() % 0xE10;
					p->pos[0] = (int16_t)(p->pos[0] + (int16_t)((int16_t)r + var<int16_t>(TABLE_DebrisX + 4 * k)) - 0x708);
					r = CrtRand() % 0x3C;
					p->pos[1] = (int16_t)-r;
					r = CrtRand() % 0xE10;
					p->pos[2] = (int16_t)(p->pos[2] + (int16_t)((int16_t)r + var<int16_t>(TABLE_DebrisZ + 4 * k)) - 0x708);
					r = CrtRand() % 0x1F4;
					p->vy = (int16_t)(-30 - r);
					p->model = (int16_t)(CrtRand() & 1);
					r = CrtRand() % 0x500;
					p->rx_step = p->vy;
					p->scale = (int16_t)(r + 0x100);
					p->rx = (int16_t)scale;
					r = CrtRand() % 100;
					p->rz = (int16_t)(r + 0x28);
					p->rz_step = (int16_t)(p->rz >> 2);
				}
			}
		}
		if (d->counter == 0x3E)
		{
			RestoreNode *t = (RestoreNode *)AddTaskToQueue(QRoot(), ORIG_Restore);
			t->counter = 0;
		}
		if (d->counter == 0x55)
		{
			// one bounce per target of action 0
			for (int k = 0; k < Ctx()->actions->target_count; k++)
			{
				const int slot = Ctx()->actions->targets[k * TARGET_STRIDE];
				BounceNode *b = (BounceNode *)AddTaskToQueue(QEffect(), ORIG_Bounce);
				b->counter = 0;
				b->slot = (int16_t)slot;
				int32_t r = CrtRand() % 3;
				b->target = (int16_t)k;
				b->spin = (int16_t)((r + 1) << 12);
				r = CrtRand() % 9000;
				b->amp = (int16_t)(-0xDAC - r);
			}
		}
		if (d->counter <= 8) SetScreenFlash((uint32_t)shl32(d->counter, 8), 0);
		else if (d->counter >= 0x7F) SetScreenFlash((uint32_t)shl32(0x87 - d->counter, 8), 0);
		if (d->counter == 0xA) BdPlaySE3D(SOUND_Muzzle, 0, Muzzle());
		if (d->counter == 0x51) BdPlaySE3D(SOUND_Blast, 0x100, Centre());
		d->counter++;
		if (d->counter <= 0x87) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}
}

	void register_mag307_ray_bomb_quistis()
	{
		register_port(raybomb307::ORIG_RootTask, (void *)raybomb307::RootTask, "R307 RootTask", 307);
		register_port(raybomb307::ORIG_Director, (void *)raybomb307::DirectorTask, "R307 Director", 307);
		register_port(raybomb307::ORIG_Debris, (void *)raybomb307::DebrisTask, "R307 Debris", 307);
		register_port(raybomb307::ORIG_Ring, (void *)raybomb307::RingTask, "R307 Ring", 307);
		register_port(raybomb307::ORIG_Puff, (void *)raybomb307::PuffTask, "R307 Puff", 307);
		register_port(raybomb307::ORIG_Beam, (void *)raybomb307::BeamTask, "R307 Beam", 307);
		register_port(raybomb307::ORIG_Spark, (void *)raybomb307::SparkTask, "R307 Spark", 307);
		register_port(raybomb307::ORIG_Flash, (void *)raybomb307::FlashTask, "R307 Flash", 307);
		register_port(raybomb307::ORIG_Charge, (void *)raybomb307::ChargeTask, "R307 Charge", 307);
		register_port(raybomb307::ORIG_Bounce, (void *)raybomb307::BounceTask, "R307 Bounce", 307);
		register_port(raybomb307::ORIG_Restore, (void *)raybomb307::RestoreTask, "R307 Restore", 307);
		// 30 fps layer: see mag307_ray_bomb_quistis_held.inc
		FX_HELD(register_mag307_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag307_ray_bomb_quistis_held.inc"
#endif
