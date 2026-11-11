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

// Effect 313: Ray-Bomb (enemy attack 200 of kernel.bin, GIM52A c0m001; MAG_313_*).
//
// The module 0x59B0B0..0x59CB70 is Ray-Bomb 10 (mag010_ray_bomb.cpp, X-ATM092's attack) function for function (near4
// w\p313_10.txt, w\amap_313.txt: exe data per item (camera animation 0xCD59B0, models / flipbooks 0xCD5B0C..0xCD82E0,
// tables 0xCD8CF0..0xCD8D68, the dust tables 3 entries instead of 6), module globals -0x28DB70). 13 of its 14
// functions are identical to 10's up to addresses (setup, root, dust particles, debris, ring, puff, beam, spark, flash,
// charge, bounce, restore); the director (0x59B180) has the same code with another timeline and these changes:
//   muzzle = the caster's effect point 0xF0 (flag 0x1000; 10: 0x680) + rotation * (0, 0xC8, 0x32) (10: (0, 800, -930));
//   charge glows at 0x1A (scale 0xC00, step 0x4C; 10: 1, 0x1000, 0x66); dust 0x1A..0x37, 3 a tick (size 0x900..0xDFF,
//   speed 0x5A..0xC7; 10: 1..0x1E, 6, 0x1000..0x13FF, 0x96..0x121); muzzle flash 0x38; at 0x4A TWO beams (scale 0x1000;
//   the first turned -0x400 spinning +0x46 / +0xA0 as 10's, the second turned +0x400 spinning -0x46 / -0xA0; 10: one at
//   0x22, scale 0x800); smoke puffs 0x63..0x68 (+ the texture restore at 0x63); shockwave ring 0x79; debris 0x79..0x7C;
//   bounces 0x7A; screen flash counter << 7 up to 0x10, (0xBE - counter) << 8 from 0xB6; the muzzle sound at 0xA is a
//   plain BdPlaySE 0xCD8CE8 (10: positional, plus the blast sound at 0x51); ends after 0xBE.
//
// Structure (setup MAG_313_UNKNOWN 0x59B0D0, file loader MAG_313_UNKNOWN_FL 0x59B0B0 = the texture named at 0xCD8D80
// "mag312.tim" into 0x223CC88; the setup keeps the context and the caster slot, sets up the root queue 0x223B688 (pool
// 2 x 0x10) and the effect queue 0x223BE18 (pool 100 x 0x24), spawns the root and the director, starts camera animation
// 0xCD59B0, queues the TIM and clears the 80 dust particles):
//   RootTask (0x59CA20) - alternates the packet arena (magic buffer / + 0x8000), runs the effect queue, then the dust
//     particles; ends when both are empty.
//   Director (0x59B180) - the whole attack on one 191-tick timeline (above).
//   Debris (0x59B9F0) - prim model (table 0xCD8D40) tumbling up and falling back, fades at the end.
//   Ring (0x59BB60) - prim model 0xCD6038 stretched upwards, fades from 10, ends at 24.
//   Puff (0x59BCA0) - sprite 0xCD5CB8 after a delay, darkening, ends after its life.
//   Beam (0x59BDF0) - prim model 0xCD82E0 from the muzzle to its end point, rolled to face the camera, stretched to the
//     distance (half length at counter 1), spinning; from counter 2 spawns 3-7 sparks a tick at its end; fades from 3,
//     ends at 10.
//   Spark (0x59C250) - flipbook 0xCD5CE0 rising at a constant speed.
//   Flash (0x59C2F0) - prim model 0xCD7C38 at the muzzle, shrinking, fades in / out.
//   Charge (0x59C3E0) - flipbook 0xCD5B0C at the muzzle after a delay, growing, fades in / out.
//   Bounce (0x59C4F0) - one party entity: saved (flags & 0x1020, rotation words +0x0C..+0x13, position +0x1C..+0x23 in
//     0x223CC38 + slot * 0x14), spun and thrown up around its effect position for 24 ticks then bounced for 8, 16 dust
//     particles at 24 (+ chain transformation 6 on a target) and 12 at 31; from 32 it waits while the entity's state
//     (+0x74 -> byte 0) is 6, then restores the entity and applies the damage (targets only).
//   Restore (0x59C9E0) - engine texture restore task (magic buffer + 0x10000), ends when done.
//   Particles (0x59CA70) - 80 dust sprites 0xCD5E8C in 0x223B698 (0x18 each), drifting and slowing down (1/16 a tick),
//     freed when the flipbook ends.
// Every task tests the draw-only flags (battle_to_update_flags 0x201): the director and the restore do nothing, the
// others draw and do not update.
// Module globals: 0x223B668..0x223CCA0 (root pool, root queue, particles, effect queue + pool, saved entity states,
// targets' centre 0x223CC78, context 0x223CC80, caster slot 0x223CC84, texture file 0x223CC88, muzzle 0x223CC90, magic
// buffer base 0x223CC98, packet cursor 0x223CC9C).
// Fujin's described Ray-Bomb (GIM52A) (effect_313.json) runs this code: its memory through own::R() (fx_port.h: a
// copy's block of its own), its spawns through own::Fn(), the setup and the file loader ported below; its
// parameters (RayBomb313Params: Ray-Bomb 10's - the director's timeline, sounds, sizes and speeds - with module 313's
// second beam, charge and dust sizes) are set from the program's.

#include "mag_common.h"
#include "fx_layouts.h"
#include "fx_params.h"
#include <stddef.h>

namespace ff8fx
{
namespace raybomb313
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline TaskQueue *QRoot() { return (TaskQueue *)own::R(0x223B688); }
	inline TaskQueue *QEffect() { return (TaskQueue *)own::R(0x223BE18); }
	inline int16_t *Centre() { return (int16_t *)own::R(0x223CC78); }             // targets' centre (x, y, z, pad)
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x223CC80)); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(own::R(0x223CC84)); }
	inline int16_t *Muzzle() { return (int16_t *)own::R(0x223CC90); }             // x, y, z, pad
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x223CC98)); }       // Magic_TextureOFF (magic buffer)
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x223CC9C)); }
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x223CC88)); }    // the effect's file (its TIM)

	static const uint32_t ORIG_Director = 0x59B180;
	static const uint32_t ORIG_Debris = 0x59B9F0;
	static const uint32_t ORIG_Ring = 0x59BB60;
	static const uint32_t ORIG_Puff = 0x59BCA0;
	static const uint32_t ORIG_Beam = 0x59BDF0;
	static const uint32_t ORIG_Spark = 0x59C250;
	static const uint32_t ORIG_Flash = 0x59C2F0;
	static const uint32_t ORIG_Charge = 0x59C3E0;
	static const uint32_t ORIG_Bounce = 0x59C4F0;
	static const uint32_t ORIG_Restore = 0x59C9E0;
	static const uint32_t ORIG_RootTask = 0x59CA20;
	static const uint32_t ORIG_Particles = 0x59CA70;   // not a task: called by the root
	static const uint32_t ORIG_FileLoader = 0x59B0B0, ORIG_Setup = 0x59B0D0;
	static const uint32_t TEXTURE_NAME = 0xCD8D80;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	struct RayBomb313Params
	{
		struct { uint32_t muzzle; int16_t muzzle_tick; } sound;                        // at the muzzle (no blast sound: Ray-Bomb 10's only)
		struct { int16_t up, back; } muzzle;                                           // from the caster's effect point, turned with it
		struct { int16_t tick, count, scale, scale_step, size, size_rand; } charge;    // the charge glows
		struct { int16_t first, last, count, speed, speed_rand, scale, scale_rand; } dust; // from the muzzle
		struct { int16_t tick, scale, scale_step; } flash;                             // the muzzle flash
		struct { int16_t tick, spin, spin_step, width, turn; } beam;                   // two beams, turned -turn / +turn, spinning both ways
		struct { int16_t first, last, scale, scale_rand; } puffs;                      // smoke at the targets
		struct { int16_t tick, width, rise; } ring;                                    // the shockwave
		struct { int16_t first, last, speed, speed_rand; } debris;
		struct { int16_t tick, height, height_rand; } bounce;                          // the party thrown up
		struct { int16_t in, out, end; } screen;                                       // the screen flash ramps, the end
		struct { uint32_t dust, puff, spark, charge; } seqs; // the sprite sequences drawn (flipbooks)
		struct { uint32_t ring, beam, flash; } models; // the prim models drawn (meshes)
		struct { uint32_t texture_name, camera; } res; // its resources (data): texture file name, camera track, TIMs, animation sets
		struct { uint32_t dust_offset, dust_dir, puff_x, puff_z, debris_x, debris_z; } tables; // its small tables (data): colours, fades, counts, offsets
	};
	static const RayBomb313Params GAME = {
		{ 0xCD8CE8, 0xA }, { 0xC8, -0x32 }, { 0x1A, 2, 0xC00, 0x4C, 0x190, 0x320 }, { 0x1A, 0x37, 3, 0x5A, 0x6E, 0x900, 0x500 },
		{ 0x38, 0x3333, 0x1D4 }, { 0x4A, 0x46, 0xA0, 0x1000, 0x400 }, { 0x63, 0x68, 0x800, 0xE00 }, { 0x79, 0x3924, 0x130C },
		{ 0x79, 0x7C, 30, 0x1F4 }, { 0x7A, 2500, 1000 }, { 0x10, 0xB6, 0xBE }, { 0xCD5E8C, 0xCD5CB8, 0xCD5CE0, 0xCD5B0C }, { 0xCD6038, 0xCD82E0, 0xCD7C38 }, { TEXTURE_NAME, 0xCD59B0 }, { 0xCD8D50, 0xCD8D68, 0xCD8CF0, 0xCD8D18, 0xCD8D04, 0xCD8D2C } };
	static RayBomb313Params live = GAME;
	static const RayBomb313Params &P() { return live; }

	static const uint32_t SEQ_Dust = 0xCD5E8C, SEQ_Puff = 0xCD5CB8, SEQ_Spark = 0xCD5CE0, SEQ_Charge = 0xCD5B0C;
	static const uint32_t MODEL_Ring = 0xCD6038, MODEL_Beam = 0xCD82E0, MODEL_Flash = 0xCD7C38;
	static const uint32_t TABLE_DebrisModel = 0xCD8D40;   // 2 prim models
	static const uint32_t TABLE_DustOffset = 0xCD8D50;    // 3 x SVECTOR: muzzle dust start offsets
	static const uint32_t TABLE_DustDir = 0xCD8D68;       // 3 x SVECTOR: muzzle dust directions
	static const uint32_t TABLE_PuffX = 0xCD8CF0, TABLE_PuffZ = 0xCD8D18;     // 5 spots (dword stride)
	static const uint32_t TABLE_DebrisX = 0xCD8D04, TABLE_DebrisZ = 0xCD8D2C; // 5 spots (dword stride)
	static const void *const SOUND_Muzzle = (const void *)0xCD8CE8;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
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
	struct DirectorNode { TaskNode hdr; int16_t counter; uint8_t pad[0x16]; };
	struct RestoreNode { TaskNode hdr; int16_t counter; int16_t done; };
	struct DebrisNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t life;      // +0x0E fades over the last 8 ticks
		int16_t pos[3];    // +0x10
		int16_t vy;        // +0x16 slows by 1/9 a tick
		int16_t model;     // +0x18 index into 0xCD8D40
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
		uint8_t pad0E[0xE];
		int16_t scale;     // +0x1C
		int16_t scale_step;// +0x1E grows by 1/32 a tick
		uint8_t pad20[4];
	};
	struct ChargeNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t delay;     // +0x0E
		uint8_t pad10[8];
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
		int16_t slot;      // +0x0E party entity
		int16_t pos[4];    // +0x10 its default effect position (taken at 0)
		int16_t spin;      // +0x18 y rotation reached at 24 (x 24)
		int16_t target;    // +0x1A index in the action's targets, -1 = not a target
		uint8_t pad1C[4];
		int16_t amp;       // +0x20 throw height (negative = up)
		int16_t pad22;
	};
	struct Particle // 0x223B698, 80 of them
	{
		uint32_t flag;     // +0x00 bit0 = in use
		int16_t counter;   // +0x04 flipbook frame
		int16_t scale;     // +0x06
		int16_t pos[3];    // +0x08
		int16_t pad0E;
		int16_t vel[3];    // +0x10
		int16_t pad16;
	};
	struct SavedEntity // 0x223CC38 + slot * 0x14
	{
		uint16_t flags;    // entity +0x00 & 0x1020
		uint16_t pad;
		uint32_t rot0;     // entity +0x0C
		uint32_t rot1;     // entity +0x10
		uint32_t pos0;     // entity +0x1C
		uint32_t pos1;     // entity +0x20
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(DirectorNode) == 0x24 && sizeof(DebrisNode) == 0x24 && sizeof(RingNode) == 0x24, "Ray-Bomb nodes");
	static_assert(sizeof(PuffNode) == 0x24 && sizeof(BeamNode) == 0x24 && sizeof(SparkNode) == 0x24 && sizeof(FlashNode) == 0x24, "Ray-Bomb nodes");
	static_assert(sizeof(ChargeNode) == 0x24 && sizeof(BounceNode) == 0x24, "Ray-Bomb nodes");
	static_assert(sizeof(Particle) == 0x18 && sizeof(SavedEntity) == 0x14, "Ray-Bomb particles / saved entities");

	inline Particle *Particles() { return (Particle *)own::R(0x223B698); }
	static const int PARTICLES = 80;
	inline SavedEntity *Saved(int slot) { return (SavedEntity *)(own::R(0x223CC38) + 0x14 * slot); }
}
}

#ifdef FF8_FX_HELD
#include "mag313_ray_bomb_held.h"
#endif

namespace ff8fx
{
namespace raybomb313
{
	// first free dust particle (flag 0), or -1
	static int FreeParticle()
	{
		for (int i = 0; i < PARTICLES; i++)
			if (Particles()[i].flag == 0) return i;
		return -1;
	}

	// ------------------------------------------------------------------
	// Dust particles (0x59CA70): draw every particle in use, then drift / slow it down
	// ------------------------------------------------------------------
	static void ParticleDraw(uint8_t *h, const Particle *p)
	{
		*(int16_t *)(h + 4) = p->counter;
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
	}

	static uint32_t ParticlePass()
	{
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_particles();)
		uint8_t *h = AllocHeader(0xB4);
		uint32_t count = 0;
		*(uint32_t *)h = fxp::sequence(P().seqs.dust, SEQ_Dust);
		*(int16_t *)(h + 0x24) = 0;
		for (int i = 0; i < PARTICLES; i++)
		{
			Particle *p = &Particles()[i];
			if (!(p->flag & 1)) continue;
			ParticleDraw(h, p);
			if (DrawOnly()) continue;
			p->counter++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				p->flag = 0;
				continue;
			}
			p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
			p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
			p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
			p->vel[0] = (int16_t)(p->vel[0] - (int16_t)(p->vel[0] >> 4));
			p->vel[1] = (int16_t)(p->vel[1] - (int16_t)(p->vel[1] >> 4));
			count++;
			p->vel[2] = (int16_t)(p->vel[2] - (int16_t)(p->vel[2] >> 4));
		}
		FieldFree(0xB4);
		return count;
	}

	// ------------------------------------------------------------------
	// Root task (0x59CA20): node +0x0C counter
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_root();)
		if (r->counter & 1) PacketCursor() = TexBase() + 0x8000;
		else PacketCursor() = TexBase();
		uint32_t alive = (uint32_t)ExecuteTaskQueue(QEffect());
		uint32_t dust = ParticlePass();
		r->counter++;
		return (dust | alive) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Restore (0x59C9E0): engine texture restore task, ends when it says done
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
	// Debris (0x59B9F0): prim model, angles (rx, 0, rz), uniform scale
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_draw(ORIG_Debris, p);)
		DebrisDraw(p);
		if (DrawOnly()) return 0;
		DebrisUpdate(p);
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter >= p->life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ring (0x59BB60): prim model 0xCD6038, no rotation, scale (sxz, sy, sxz)
	// ------------------------------------------------------------------
	static void RingHeader(uint8_t *h, const RingNode *p)
	{
		*(uint32_t *)h = fxp::model(P().models.ring, MODEL_Ring);
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_draw(ORIG_Ring, p);)
		RingDraw(p);
		if (DrawOnly()) return 0;
		RingUpdate(p);
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter >= 0x18 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Puff (0x59BCA0): sprite 0xCD5CB8 turned (0x400, angle, 0), darkening
	// ------------------------------------------------------------------
	static void PuffHeader(uint8_t *h, const PuffNode *p)
	{
		*(uint16_t *)(h + 4) = 0;
		*(uint32_t *)h = fxp::sequence(P().seqs.puff, SEQ_Puff);
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_draw(ORIG_Puff, p);)
		PuffDraw(p);
		if (DrawOnly()) return 0;
		p->counter++;
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter >= p->life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Beam (0x59BDF0): prim model 0xCD82E0 from the muzzle towards pos, rolled about its axis so
	// that it faces the camera eye (0xB8B7F0), length = distance; spark spawn point in `end`
	// ------------------------------------------------------------------
	static void BeamHeader(uint8_t *h, const BeamNode *p)
	{
		*(uint32_t *)h = fxp::model(P().models.beam, MODEL_Beam);
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_fix_stretch(&stretch, full);)
		int32_t s[3] = { p->scale, p->scale, stretch };
		Scale3DMatrix(&m1, s);
		ComposeAffineTransform(&Camera(), &m1, &m1);
		GteSetRotMatrix(&m1);
		GteSetTransVector(&m1);
		uint8_t *h = AllocHeader(0x58);
		BeamHeader(h, p);
		// 30 fps layer: see mag313_ray_bomb_held.inc
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
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
					SparkNode *s = (SparkNode *)AddTaskToQueue(QEffect(), own::Fn(ORIG_Spark));
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter > 9 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Spark (0x59C250): flipbook 0xCD5CE0 rising at a constant speed
	// ------------------------------------------------------------------
	static void SparkDraw(const SparkNode *p)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 3));
		uint8_t *h = AllocHeader(0xB4);
		*(uint16_t *)(h + 4) = (uint16_t)p->frame;
		*(uint32_t *)h = fxp::sequence(P().seqs.spark, SEQ_Spark);
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *p = (SparkNode *)n;
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_draw(ORIG_Spark, p);)
		SparkDraw(p);
		if (DrawOnly()) return 0;
		p->pos[1] = (int16_t)(p->pos[1] + p->vy);
		p->frame++;
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_after(p);)
		return p->frame >= p->life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Flash (0x59C2F0): prim model 0xCD7C38 at the muzzle (camera facing), fades in then out
	// ------------------------------------------------------------------
	static void FlashHeader(uint8_t *h, const FlashNode *p)
	{
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint32_t *)h = fxp::model(P().models.flash, MODEL_Flash);
		if (p->counter < 0x10)
		{
			*(int32_t *)(h + 0xC) = (0x10 - p->counter) << 8;
			*(uint32_t *)(h + 0x1C) = 0xC0;
		}
		else if (p->counter >= 0x1E)
		{
			*(int32_t *)(h + 0xC) = (p->counter - 0x1E) * 0x2AA;
			*(uint32_t *)(h + 0x1C) = 0xC0;
		}
	}

	static void FlashDraw(const FlashNode *p)
	{
		TransformCameraByShadowRotation(Muzzle(), p->scale, -(p->scale >> 3));
		uint8_t *h = AllocHeader(0x58);
		FlashHeader(h, p);
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_fix_header(h);)
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void FlashUpdate(FlashNode *p)
	{
		if (p->counter < 0x12)
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_draw(ORIG_Flash, p);)
		FlashDraw(p);
		if (DrawOnly()) return 0;
		FlashUpdate(p);
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter >= 0x24 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Charge (0x59C3E0): flipbook 0xCD5B0C at the muzzle, growing, fades in (0..7) / out (22..)
	// ------------------------------------------------------------------
	static void ChargeHeader(uint8_t *h, const ChargeNode *p)
	{
		*(int32_t *)(h + 8) = p->size;
		*(uint32_t *)h = fxp::sequence(P().seqs.charge, SEQ_Charge);
		*(int16_t *)(h + 4) = p->frame;
		*(uint16_t *)(h + 0x24) = 1;
		if (p->counter < 8 || p->counter >= 0x16)
		{
			uint8_t level = p->counter < 8 ? (uint8_t)(p->counter << 4) : (uint8_t)(0xE0 - (uint8_t)(p->counter << 4));
			h[0x1E] = level;
			h[0x1D] = level;
			h[0x1C] = level;
			*(uint16_t *)(h + 0x24) = 5;
		}
	}

	static void ChargeDraw(const ChargeNode *p, uint8_t **header)
	{
		TransformCameraByShadowRotation(Muzzle(), p->scale, -(p->scale >> 4));
		uint8_t *h = AllocHeader(0xB4);
		ChargeHeader(h, p);
		// 30 fps layer: see mag313_ray_bomb_held.inc
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
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_draw(ORIG_Charge, p);)
		ChargeDraw(p, &h);
		if (DrawOnly()) return 0;
		// the flipbook restarts when it has ended (header +0x28 negative; read after the free, the
		// scratch is not reused in between)
		if (*(int16_t *)(h + 0x28) < 0) p->frame = 0;
		else p->frame++;
		p->scale = (int16_t)(p->scale + p->scale_step);
		p->counter++;
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(held_note_after(p);)
		return p->counter >= 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Bounce (0x59C4F0): throws one party entity (see the module comment)
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
		if (b->counter == 0x18)
		{
			BounceDust(b, 16, 0, 0xA00, 90, 0x78, true);
			if (b->target >= 0) QueueChainTransformation(e, 6);
		}
		if (b->counter == 0x1F) BounceDust(b, 12, 0x300, 0x800, 60, 100, false);
		b->counter++;
		if (b->counter < 0x20) return 0;
		int16_t target = b->target;
		if (target >= 0 && **(uint8_t **)(e + 0x74) == 6) return 0;
		*(uint16_t *)e = (uint16_t)((*(uint16_t *)e & 0xEFDF) | sv->flags);
		*(uint32_t *)(e + 0x1C) = sv->pos0;
		*(uint32_t *)(e + 0x20) = sv->pos1;
		*(uint32_t *)(e + 0x0C) = sv->rot0;
		*(uint32_t *)(e + 0x10) = sv->rot1;
		if (target >= 0) ApplyActionResultToTarget(Ctx()->actions->targets + target * TARGET_STRIDE);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// File loader (0x59B0B0) and setup (0x59B0D0): the game runs its own; Fujin's programs these (its memory
	// may be a copy's): the magic buffer, the cast, the caster, the root, the effect queue with the director,
	// the camera, the texture, the dust cleared
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(P().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		TexBase() = layouts::MagicBuffer();
		Ctx() = ctx;
		CasterSlot() = ctx->attacker;
		InitTaskQueuePool(QRoot(), (void *)own::R(0x223B668), 0x10, 2);
		*(uint16_t *)(AddTaskToQueue(QRoot(), own::Fn(ORIG_RootTask)) + 1) = 0;
		InitTaskQueuePool(QEffect(), (void *)own::R(0x223BE28), 0x24, 100);
		*(uint16_t *)(AddTaskToQueue(QEffect(), own::Fn(ORIG_Director)) + 1) = 0;
		PlayCameraAnimation((const void *)fxp::camera(P().res.camera, 0xCD59B0));
		QueueTimUpload(TexFile());
		for (int i = 0; i < PARTICLES; i++) Particles()[i].flag = 0;
		return QRoot();
	}

	// ------------------------------------------------------------------
	// Director (0x59B180)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		if (DrawOnly()) return 0;
		uint8_t *caster = Entity(CasterSlot());
		const Mat4x3 *crot = (const Mat4x3 *)(caster + 0x40);
		if (d->counter == 0)
		{
			GetEffectSpawnPosition(caster, 0xF0, 0x1000, Muzzle());
			int16_t v[4] = { 0, P().muzzle.up, (int16_t)-P().muzzle.back, 0 };
			MatrixMulVector(crot, v, v);
			Muzzle()[0] = (int16_t)(Muzzle()[0] + v[0]);
			Muzzle()[1] = (int16_t)(Muzzle()[1] + v[1]);
			Muzzle()[2] = (int16_t)(Muzzle()[2] + v[2]);
			CalculateCenterPosition(Ctx()->target_mask, Centre());
		}
		if (d->counter == P().charge.tick)
		{
			for (int k = 0; k < P().charge.count; k++)
			{
				ChargeNode *c = (ChargeNode *)AddTaskToQueue(QEffect(), own::Fn(ORIG_Charge));
				c->counter = 0;
				c->delay = (int16_t)(CrtRand() % 6 + k);
				int32_t r = CrtRand() % P().charge.size_rand;
				c->scale = P().charge.scale;
				c->scale_step = P().charge.scale_step;
				c->frame = 0;
				c->size = (int16_t)(r + P().charge.size);
			}
		}
		if (d->counter >= P().dust.first && d->counter <= P().dust.last)
		{
			// 3 dust particles from the muzzle, one per table entry
			for (int k = 0; k < P().dust.count && k < 3; k++)
			{
				int i = FreeParticle();
				if (i < 0) break;
				Particle *p = &Particles()[i];
				p->flag = 1;
				p->counter = 0;
				p->scale = (int16_t)(CrtRand() % P().dust.scale_rand + P().dust.scale);
				int16_t off[4];
				memcpy(off, (const void *)(fxp::data(P().tables.dust_offset, TABLE_DustOffset, 24) + 8 * k), 8);
				MatrixMulVector(crot, off, off);
				memcpy(p->pos, Muzzle(), 8);
				p->pos[0] = (int16_t)(p->pos[0] + off[0]);
				p->pos[1] = (int16_t)(p->pos[1] + off[1]);
				int16_t dir[4];
				p->pos[2] = (int16_t)(p->pos[2] + off[2]);
				memcpy(dir, (const void *)(fxp::data(P().tables.dust_dir, TABLE_DustDir, 24) + 8 * k), 8);
				MatrixMulVector(crot, dir, dir);
				NormalizeSVector(dir, dir);
				int32_t speed = CrtRand() % P().dust.speed_rand + P().dust.speed;
				p->vel[0] = (int16_t)(mul32(dir[0], speed) >> 12);
				p->vel[1] = (int16_t)(mul32(dir[1], speed) >> 12);
				p->vel[2] = (int16_t)(mul32(dir[2], speed) >> 12);
			}
		}
		if (d->counter == P().flash.tick)
		{
			FlashNode *f = (FlashNode *)AddTaskToQueue(QEffect(), own::Fn(ORIG_Flash));
			f->counter = 0;
			f->scale = P().flash.scale;
			f->scale_step = P().flash.scale_step;
		}
		if (d->counter == P().beam.tick)
		{
			BeamNode *b = (BeamNode *)AddTaskToQueue(QEffect(), own::Fn(ORIG_Beam));
			int16_t v[4];
			v[0] = (int16_t)(Centre()[0] - Muzzle()[0]);
			b->counter = 0;
			v[1] = 0;
			v[2] = (int16_t)(Centre()[2] - Muzzle()[2]);
			v[3] = 0;
			int16_t angles[4] = { 0, (int16_t)-P().beam.turn, 0, 0 };
			Mat4x3 m = {};
			BuildRotationMatrixFromAngles(angles, &m);
			MatrixMulVector(&m, v, b->pos);
			b->pos[0] = (int16_t)(b->pos[0] + Muzzle()[0]);
			b->pos[2] = (int16_t)(b->pos[2] + Muzzle()[2]);
			b->pos[1] = Centre()[1];
			b->angle = P().beam.spin;
			b->angle_step = P().beam.spin_step;
			b->scale = P().beam.width;
			// the second beam: turned the other way (+0x400), spinning the other way
			b = (BeamNode *)AddTaskToQueue(QEffect(), own::Fn(ORIG_Beam));
			v[0] = (int16_t)(Centre()[0] - Muzzle()[0]);
			b->counter = 0;
			v[1] = 0;
			v[2] = (int16_t)(Centre()[2] - Muzzle()[2]);
			v[3] = 0;
			int16_t angles2[4] = { 0, P().beam.turn, 0, 0 };
			Mat4x3 m2 = {};
			BuildRotationMatrixFromAngles(angles2, &m2);
			MatrixMulVector(&m2, v, b->pos);
			b->pos[0] = (int16_t)(b->pos[0] + Muzzle()[0]);
			b->pos[2] = (int16_t)(b->pos[2] + Muzzle()[2]);
			b->pos[1] = Centre()[1];
			b->angle = (int16_t)-P().beam.spin;
			b->angle_step = (int16_t)-P().beam.spin_step;
			b->scale = P().beam.width;
		}
		if (d->counter >= P().puffs.first && d->counter <= P().puffs.last)
		{
			for (int k = 0; k < 5; k++)
			{
				int32_t cnt = (CrtRand() & 1) + 1;
				for (int j = cnt; j > 0; j--)
				{
					PuffNode *p = (PuffNode *)AddTaskToQueue(QEffect(), own::Fn(ORIG_Puff));
					p->counter = 0;
					int32_t r = CrtRand() & 7;
					memcpy(p->pos, Centre(), 4);
					p->delay = (int16_t)(r + cnt);
					memcpy(&p->pos[2], &Centre()[2], 4);
					r = CrtRand() % 0x320;
					p->pos[1] = 0;
					p->pos[0] = (int16_t)(p->pos[0] + (int16_t)((int16_t)r + var<int16_t>(fxp::data(P().tables.puff_x, TABLE_PuffX, 20) + 4 * k)) - 0x190);
					r = CrtRand() % 0x320;
					p->pos[2] = (int16_t)(p->pos[2] + (int16_t)((int16_t)r + var<int16_t>(fxp::data(P().tables.puff_z, TABLE_PuffZ, 20) + 4 * k)) - 0x12C);
					p->angle = (int16_t)(CrtRand() & 0xFFF);
					r = CrtRand() % P().puffs.scale_rand;
					p->scale = (int16_t)(r + P().puffs.scale);
					p->life = (int16_t)((CrtRand() & 3) + 4);
				}
			}
		}
		if (d->counter == P().ring.tick)
		{
			RingNode *g = (RingNode *)AddTaskToQueue(QEffect(), own::Fn(ORIG_Ring));
			memcpy(g->pos, Centre(), 4);
			g->counter = 0;
			memcpy(&g->pos[2], &Centre()[2], 4);
			g->sxz = P().ring.width;
			g->sy = 0;
			g->sy_step = P().ring.rise;
		}
		if (d->counter >= P().debris.first && d->counter <= P().debris.last)
		{
			for (int k = 0; k < 5; k++)
			{
				int32_t cnt = (CrtRand() & 1) + 2;
				int32_t scale = cnt * 400;
				for (int j = 0; j < cnt; j++)
				{
					DebrisNode *p = (DebrisNode *)AddTaskToQueue(QEffect(), own::Fn(ORIG_Debris));
					p->counter = 0;
					int32_t r = CrtRand() & 7;
					p->life = (int16_t)(r + j + 0xB);
					memcpy(p->pos, Centre(), 4);
					memcpy(&p->pos[2], &Centre()[2], 4);
					r = CrtRand() % 0xE10;
					p->pos[0] = (int16_t)(p->pos[0] + (int16_t)((int16_t)r + var<int16_t>(fxp::data(P().tables.debris_x, TABLE_DebrisX, 20) + 4 * k)) - 0x708);
					r = CrtRand() % 0x3C;
					p->pos[1] = (int16_t)-r;
					r = CrtRand() % 0xE10;
					p->pos[2] = (int16_t)(p->pos[2] + (int16_t)((int16_t)r + var<int16_t>(fxp::data(P().tables.debris_z, TABLE_DebrisZ, 20) + 4 * k)) - 0x708);
					r = CrtRand() % P().debris.speed_rand;
					p->vy = (int16_t)(-P().debris.speed - r);
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
		if (d->counter == P().puffs.first)   // (the texture restored with the first puffs)
		{
			RestoreNode *t = (RestoreNode *)AddTaskToQueue(QRoot(), own::Fn(ORIG_Restore));
			t->counter = 0;
		}
		if (d->counter == P().bounce.tick)
		{
			// one bounce per party entity with a model
			for (int slot = 0; slot < 3; slot++)
			{
				if (!(*Entity(slot) & 2)) continue;
				BounceNode *b = (BounceNode *)AddTaskToQueue(QEffect(), own::Fn(ORIG_Bounce));
				b->counter = 0;
				b->slot = (int16_t)slot;
				b->spin = (int16_t)((CrtRand() % 3 + 1) << 12);
				int32_t r = CrtRand() % P().bounce.height_rand;
				int16_t target = -1;
				int16_t amp = (int16_t)(-P().bounce.height - r);
				b->amp = amp;
				ActionData *a0 = Ctx()->actions;
				int count = a0->target_count;
				for (int j = 0; j < count; j++)
					if (a0->targets[j * TARGET_STRIDE] == slot) { target = (int16_t)j; break; }
				b->target = target;
				if (target < 0) b->amp = (int16_t)(amp >> 1);
			}
		}
		if (d->counter <= P().screen.in) SetScreenFlash((uint32_t)shl32(d->counter, 7), 0);
		else if (d->counter >= P().screen.out) SetScreenFlash((uint32_t)shl32(P().screen.end - d->counter, 8), 0);
		if (d->counter == P().sound.muzzle_tick) BdPlaySE((const void *)fxp::sound(P().sound.muzzle, (uint32_t)SOUND_Muzzle), 0, 0x80);
		d->counter++;
		if (d->counter <= P().screen.end) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}
	namespace table
	{
		using fxp::Field;
#define RB(part, member, label, doc, lo, hi) { part, #member, label, doc, (uint32_t)offsetof(RayBomb313Params, member), fxp::I16, lo, hi }
#define RB_SOUND(part, member, label, doc) { part, #member, label, doc, (uint32_t)offsetof(RayBomb313Params, member), fxp::Sound, 0, 0x7FFFFFFF }
		static const fxp::Part PARTS[] = {
			{ 0, "timing", "Timing", "The director's timeline: its sound, the screen flash, its end (draws nothing)" },
			{ 0, "charge", "Charge", "The muzzle in front of the caster, its charge glows and the dust blown from it" },
			{ 0, "flash", "Muzzle flash", "A prim model at the muzzle, shrinking" },
			{ 0, "beam", "Beam", "The two beams from the muzzle to the targets, spinning both ways, their sparks" },
			{ 0, "blast", "Blast", "Smoke puffs, the shockwave ring and debris at the targets" },
			{ 0, "bounce", "Bounce", "The party thrown up and bounced back (the damage on the targets)" },
		};
		static const fxp::Group GROUPS[] = {
			{ "muzzle", "Muzzle", "From the caster's effect point, turned with it: Up, Back" },
			{ "dust", "Dust", "From the muzzle, ticks First to Last: Count a tick (at most 3), speed Speed + rand % Spread, size Size + rand % Size spread" },
			{ "puffs", "Smoke", "Ticks First to Last: 1-2 puffs at 5 spots, scale Scale + rand % Spread (the texture restored at First)" },
			{ "debris", "Debris", "Ticks First to Last: 2-3 pieces at 5 spots thrown up by Speed + rand % Spread" },
		};
		static const Field FIELDS[] = {
			RB_SOUND("timing", sound.muzzle, "Muzzle sound", "Played at its tick"),
			RB("timing", sound.muzzle_tick, "Muzzle sound tick", "", 0, 200),
			RB("timing", screen.in, "Flash in", "The screen flashes up until this tick...", 0, 200),
			RB("timing", screen.out, "Flash out", "...and down from this one to the end", 0, 400),
			RB("timing", screen.end, "End", "The attack ends after this tick", 1, 400),
			RB("charge", muzzle.up, "Up", "", -0x2000, 0x2000),
			RB("charge", muzzle.back, "Back", "", -0x2000, 0x2000),
			RB("charge", charge.tick, "Glows tick", "The charge glows start at this tick...", 0, 200),
			RB("charge", charge.count, "Glows", "...this many", 0, 10),
			RB("charge", charge.scale, "Glow scale", "Each glow's scale at its start (4096: its own size)", 0, 0x7FFF),
			RB("charge", charge.scale_step, "Glow growth", "Each glow grows by this a tick", -0x1000, 0x1000),
			RB("charge", charge.size, "Glow size", "Its size: this + rand % Glow size spread", 0, 0x2000),
			RB("charge", charge.size_rand, "Glow size spread", "", 1, 0x2000),
			RB("charge", dust.first, "First", "", 0, 200),
			RB("charge", dust.last, "Last", "", 0, 200),
			RB("charge", dust.count, "Count", "", 0, 3),
			RB("charge", dust.speed, "Speed", "", 0, 0x1000),
			RB("charge", dust.speed_rand, "Spread", "", 1, 0x1000),
			RB("charge", dust.scale, "Size", "", 0, 0x7FFF),
			RB("charge", dust.scale_rand, "Size spread", "", 1, 0x7FFF),
			RB("flash", flash.tick, "Tick", "The muzzle flash at this tick", 0, 200),
			RB("flash", flash.scale, "Scale", "Its scale (4096: its own size)...", 0, 0x7FFF),
			RB("flash", flash.scale_step, "Shrink", "...down by this a tick", 0, 0x2000),
			RB("beam", beam.tick, "Tick", "The two beams at this tick", 0, 200),
			RB("beam", beam.spin, "Spin", "Their spin (4096: a turn; the second beam's the other way)...", -0x1000, 0x1000),
			RB("beam", beam.spin_step, "Spin step", "...up by this a tick", -0x1000, 0x1000),
			RB("beam", beam.width, "Width", "Their width (4096: its own)", 0, 0x7FFF),
			RB("beam", beam.turn, "Turn", "The first beam's end turned by -this round the muzzle, the second's by +this (4096: a turn)", -0x800, 0x800),
			RB("blast", puffs.first, "First", "", 0, 200),
			RB("blast", puffs.last, "Last", "", 0, 200),
			RB("blast", puffs.scale, "Scale", "", 0, 0x7FFF),
			RB("blast", puffs.scale_rand, "Spread", "", 1, 0x7FFF),
			RB("blast", ring.tick, "Shockwave tick", "The shockwave ring at this tick", 0, 200),
			RB("blast", ring.width, "Shockwave width", "Its width (4096: its own)", 0, 0x7FFF),
			RB("blast", ring.rise, "Shockwave rise", "Its height up by this, halving a tick", 0, 0x7FFF),
			RB("blast", debris.first, "First", "", 0, 200),
			RB("blast", debris.last, "Last", "", 0, 200),
			RB("blast", debris.speed, "Speed", "", 0, 0x2000),
			RB("blast", debris.speed_rand, "Spread", "", 1, 0x2000),
			RB("bounce", bounce.tick, "Tick", "The party thrown up at this tick", 0, 200),
			RB("bounce", bounce.height, "Height", "Each one up by this + rand % Height spread (half for those not hit)", 0, 0x4000),
			RB("bounce", bounce.height_rand, "Height spread", "", 1, 0x4000),
			{ "timing", "seqs.dust", "Dust", "The sprite sequence dust (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(RayBomb313Params, seqs.dust), fxp::Seq, 0, 0x7FFFFFFF },
			{ "timing", "seqs.puff", "Puff", "The sprite sequence puff (a flipbook of 1 frames: an edited one keeps as many)", (uint32_t)offsetof(RayBomb313Params, seqs.puff), fxp::Seq, 0, 0x7FFFFFFF },
			{ "timing", "seqs.spark", "Spark", "The sprite sequence spark (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(RayBomb313Params, seqs.spark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "timing", "seqs.charge", "Charge", "The sprite sequence charge (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(RayBomb313Params, seqs.charge), fxp::Seq, 0, 0x7FFFFFFF },
			{ "timing", "models.ring", "Ring model", "The prim model ring (a mesh of 144 vertices, 108 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(RayBomb313Params, models.ring), fxp::Model, 0, 0x7FFFFFFF },
			{ "timing", "models.beam", "Beam model", "The prim model beam (a mesh of 100 vertices, 72 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(RayBomb313Params, models.beam), fxp::Model, 0, 0x7FFFFFFF },
			{ "timing", "models.flash", "Flash model", "The prim model flash (a mesh of 55 vertices, 54 primitives: an edited one keeps its vertices (its code reads them by number))", (uint32_t)offsetof(RayBomb313Params, models.flash), fxp::Model, 0, 0x7FFFFFFF },
			{ "timing", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(RayBomb313Params, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "timing", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(RayBomb313Params, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "timing", "tables.dust_offset", "Dust offsets", "Where each dust puff starts from the muzzle (x, y, z) (24 bytes: an edited one keeps its length)", (uint32_t)offsetof(RayBomb313Params, tables.dust_offset), fxp::Data, 0, 0x7FFFFFFF },
			{ "timing", "tables.dust_dir", "Dust directions", "Each dust puff's direction (x, y, z) (24 bytes: an edited one keeps its length)", (uint32_t)offsetof(RayBomb313Params, tables.dust_dir), fxp::Data, 0, 0x7FFFFFFF },
			{ "timing", "tables.puff_x", "Puff x offsets", "Each puff's x offset (+ a random 0..0x31F, - 0x190) (20 bytes: an edited one keeps its length)", (uint32_t)offsetof(RayBomb313Params, tables.puff_x), fxp::Data, 0, 0x7FFFFFFF },
			{ "timing", "tables.puff_z", "Puff z offsets", "Each puff's z offset (+ a random, - 0x12C) (20 bytes: an edited one keeps its length)", (uint32_t)offsetof(RayBomb313Params, tables.puff_z), fxp::Data, 0, 0x7FFFFFFF },
			{ "timing", "tables.debris_x", "Debris x offsets", "Each debris' x offset (+ a random, - 0x708) (20 bytes: an edited one keeps its length)", (uint32_t)offsetof(RayBomb313Params, tables.debris_x), fxp::Data, 0, 0x7FFFFFFF },
			{ "timing", "tables.debris_z", "Debris z offsets", "Each debris' z offset (+ a random, - 0x708) (20 bytes: an edited one keeps its length)", (uint32_t)offsetof(RayBomb313Params, tables.debris_z), fxp::Data, 0, 0x7FFFFFFF },
		};
#undef RB_SOUND
#undef RB
	}
}

	void register_mag313_ray_bomb()
	{
		fxp::register_params({ 313, "Ray-Bomb (GIM52A)", &raybomb313::live, &raybomb313::GAME, sizeof(raybomb313::RayBomb313Params),
		                       raybomb313::table::PARTS, (int)(sizeof(raybomb313::table::PARTS) / sizeof(raybomb313::table::PARTS[0])),
		                       raybomb313::table::FIELDS, (int)(sizeof(raybomb313::table::FIELDS) / sizeof(raybomb313::table::FIELDS[0])),
		                       raybomb313::table::GROUPS, (int)(sizeof(raybomb313::table::GROUPS) / sizeof(raybomb313::table::GROUPS[0])) });
		own::note(raybomb313::ORIG_FileLoader, (void *)raybomb313::FileLoader);
		own::note(raybomb313::ORIG_Setup, (void *)raybomb313::Setup);
		own::note(raybomb313::ORIG_RootTask, (void *)raybomb313::RootTask);
		own::note(raybomb313::ORIG_Director, (void *)raybomb313::DirectorTask);
		own::note(raybomb313::ORIG_Debris, (void *)raybomb313::DebrisTask);
		own::note(raybomb313::ORIG_Ring, (void *)raybomb313::RingTask);
		own::note(raybomb313::ORIG_Puff, (void *)raybomb313::PuffTask);
		own::note(raybomb313::ORIG_Beam, (void *)raybomb313::BeamTask);
		own::note(raybomb313::ORIG_Spark, (void *)raybomb313::SparkTask);
		own::note(raybomb313::ORIG_Flash, (void *)raybomb313::FlashTask);
		own::note(raybomb313::ORIG_Charge, (void *)raybomb313::ChargeTask);
		own::note(raybomb313::ORIG_Bounce, (void *)raybomb313::BounceTask);
		own::note(raybomb313::ORIG_Restore, (void *)raybomb313::RestoreTask);
		register_port(raybomb313::ORIG_RootTask, (void *)raybomb313::RootTask, "R313 RootTask", 313);
		register_port(raybomb313::ORIG_Director, (void *)raybomb313::DirectorTask, "R313 Director", 313);
		register_port(raybomb313::ORIG_Debris, (void *)raybomb313::DebrisTask, "R313 Debris", 313);
		register_port(raybomb313::ORIG_Ring, (void *)raybomb313::RingTask, "R313 Ring", 313);
		register_port(raybomb313::ORIG_Puff, (void *)raybomb313::PuffTask, "R313 Puff", 313);
		register_port(raybomb313::ORIG_Beam, (void *)raybomb313::BeamTask, "R313 Beam", 313);
		register_port(raybomb313::ORIG_Spark, (void *)raybomb313::SparkTask, "R313 Spark", 313);
		register_port(raybomb313::ORIG_Flash, (void *)raybomb313::FlashTask, "R313 Flash", 313);
		register_port(raybomb313::ORIG_Charge, (void *)raybomb313::ChargeTask, "R313 Charge", 313);
		register_port(raybomb313::ORIG_Bounce, (void *)raybomb313::BounceTask, "R313 Bounce", 313);
		register_port(raybomb313::ORIG_Restore, (void *)raybomb313::RestoreTask, "R313 Restore", 313);
		// 30 fps layer: see mag313_ray_bomb_held.inc
		FX_HELD(register_mag313_held();)
	}
	FX_LAB_MODULE(register_mag313_ray_bomb);
}

#ifdef FF8_FX_HELD
#include "mag313_ray_bomb_held.inc"
#endif
