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

// Effect 17: Elvoret's entrance (Dollet communication tower, scene encounter 29). No kernel.bin entry
// uses effect 17: Elvoret c0m091's AI init script launches it itself, `target(SELF); prepareAnim(15);
// attackAnimation(17)` (AI opcode 0x1E at 0x489A28: command 0xFC, the effect id byte through
// DRAW_TARGET_ID 0x1D27ADA -> 0x1D27ADC (Battle_applyDamage 0x4905E4) -> the action's effect id ->
// BattleActionSequence_Tick_MagicEffect 0x50B190); the caster and the only target are Elvoret's own
// slot. Elvoret is hidden in the encounter (entity +0x00 bit 2) and enters when Biggs' / Wedge's
// death script runs `enter(2)` after effect 110.
//
// Structure (setup MAG_017_ELVORET_ENTRANCE 0x7003C0 -> 0x7003D0; the file loader 0x575600 is a
// nullsub: the TIM 0x14D8F58 is in exe data). The setup keeps the caster's entity, puts it high
// above the ground (entity +0x1E = 0xEAC0), sets its entity flag 0x1000, starts camera animation
// 0x14D8BAC (the engine's) and queues the TIM:
//   RootTask (0x700460) - node 0x14: alternates the packet arena (magic buffer + 0x1BA4 /
//     + 0x11BA4), sets the caster's flag 0x1000 every tick; on counter 1 (the counter stops at 10)
//     shows the caster (entity +0x00 bit 2 cleared), sets up the pools and starts the main task
//     (voice slot 0x14D8E48, prim-model layout 0x14D734C); runs the main and particle queues; ends
//     when both are empty (caster flag 0x1000 cleared).
//   MainTask (0x7005C0) - node 0x1FC, counter 0..0x7A:
//     0x14 / 0x28: caster sound (BdPlayActorSE);
//     0..0x54: the caster comes down (entity +0x1E += 0x40 per tick: 0xEAC0 -> 0); 0..0x20: the
//       battle ambient colour 0xB8B9A8 faded from black (0x2556FF8, never written) to the caster's
//       +0x2C colour (ColourLerp, weight counter * 0x80);
//     0x50..0x63: a ring burst: (sin((counter - 0x50) * 4096 / 40) >> 9) + 4 particles around the
//       caster's ground point (x, z on a circle of 0x400 at a random start angle, y = entity
//       height), velocity outwards scaled by a random factor;
//     0..0x54: the caster's wings (effect bone 0x1D, flag 0x400): sound 0x14D8E40 every 25 ticks,
//       up to 2 particles per tick (rand * 20 >> 15 > counter % 25), random size and velocity;
//     0x55: the party members (slots 0..2) whose entity +0x00 bit 1 is set and +0x08 & 3 is 0
//       react (QueueChainTransformation 4);
//     0x32..0x7A: the prim-model layout at the caster's (x, height, z) with PartCallback; sound
//       0x14D8E44 at 0x50; the voice slot released at 0x76; ends after 0x7A.
//   ParticleTask (0x700B70) - node 0x20: sprite sequence 0x14D71EC (frame = counter, 13 frames,
//     camera-facing scale / roll) at its position, then position += velocity.
//   PartCallback (0x700940, = Bad Breath's 0x6CB910 / Berserk's 0x6D03D0): one object of the
//     layout: vertex frames (blended by the shared helper 0x701390 into magic buffer + 0x1B9C),
//     rotation (0x701310), the record offset + the block anchor, camera-aligned or composed with
//     the camera, scaled, draw flags from the table 0x14FA778.
// The shared helpers 0x701200 / 0x701310 / 0x701390 (the matrix and blend library many modules
// call) are not module code: called at their original addresses.
// ENTITY WRITES: the caster's position +0x1E (y, every tick 0..0x54: it MOVES a battle entity), its
// flags +0x00 (bit 2 cleared at counter 1, bit 0x1000 set every tick, cleared at the end);
// party members' chain transformation at 0x55. The battle ambient colour 0xB8B9A8 (ticks 0..0x20).
// CAMERA: none written by the module (the setup's engine camera animation).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2556FA0..0x2556FFC (caster entity, magic buffer base, context, root pool,
// packet cursor 0x2556FC4, particle / main / root queues, the black colour 0x2556FF8). The particle
// pool (0x80 x 0x20), the main pool (+ 0x1000), the morph buffer (+ 0x1B9C) and the packet arenas
// are in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace elvoret017
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2556FA0); }         // the caster's entity
	inline uint32_t &TexBase() { return var<uint32_t>(0x2556FA4); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2556FA8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2556FC4); }
	inline TaskQueue *QParticles() { return (TaskQueue *)0x2556FC8; }       // pool: magic buffer, 0x80 x 0x20
	inline TaskQueue *QMain() { return (TaskQueue *)0x2556FD8; }            // pool: + 0x1000, 1 x 0x1FC
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x700460;
	static const uint32_t ORIG_MainTask = 0x7005C0;
	static const uint32_t ORIG_ParticleTask = 0x700B70;
	static const uint32_t ORIG_PartCallback = 0x700940;
	static const uint32_t MODEL_Layout = 0x14D734C;     // prim-model layout data (0x1E8-byte layout)
	static const uint32_t PART_Flags = 0x14FA778;       // per-object flags of the layout
	static const uint32_t SEQ_Particle = 0x14D71EC;     // particle sprite sequence (13 frames)
	static const uint32_t COLOUR_Black = 0x2556FF8;     // module global, never written (0)
	static const uint32_t AMBIENT = 0xB8B9A8;           // battle ambient colour
	static const void *const SOUND_Voice = (const void *)0x14D8E48;
	static const void *const SOUND_Wings = (const void *)0x14D8E40;
	static const void *const SOUND_Land = (const void *)0x14D8E44;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
	inline void BdPlayActorSE(uint8_t *entity, int32_t a, int32_t b, int32_t c) { fn<void (__cdecl *)(uint8_t *, int32_t, int32_t, int32_t)>(0x5015A0)(entity, a, b, c); }
	inline void QueueChainTransformation(uint8_t *entity, int32_t chain) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, chain); }
	// ColourLerp 0x56CC00: out = a * wa + b * wb (4.12 weights, per channel)
	inline void ColourLerp(uint32_t out, uint32_t a, int32_t wa, int32_t wb, const void *b) { fn<void (__cdecl *)(uint32_t, uint32_t, int32_t, int32_t, const void *)>(0x56CC00)(out, a, wa, wb, b); }
	// shared matrix / blend library (not module code)
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C the main task starts at 1 (stops at 10)
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x1FC bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..0x7A
		int16_t pad0E;
		uint32_t voice;    // +0x10 voice slot (0x14D8E48)
		uint8_t layout[0x1E8]; // +0x14 prim-model layout (prim::Layout: data, frame, state)
	};
	struct ParticleNode // pool of 0x80 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C sequence frame (13 frames)
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t scale;     // +0x16
		int16_t vel[3];    // +0x18
		int16_t pad1E;
	};
	// the prim-player callback's parameter block (a stack block of the main task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 the caster's x, height, z; +0x06 not written here (not read)
		uint32_t flags;    // +0x08 per-object flags table
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(MainNode) == 0x1FC && sizeof(ParticleNode) == 0x20, "Elvoret entrance nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Elvoret entrance prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x700940, a copy of Bad Breath's 0x6CB910): one object of the layout
	// ------------------------------------------------------------------
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
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
		// the matrix's translation is the record offset + the block position (int32)
		Mat4x3 m = {};
		RotationFromAngles(r->rot, &m);
		m.t[0] = (int32_t)r->pos[0] + arg->pos[0];
		m.t[1] = (int32_t)r->pos[1] + arg->pos[1];
		m.t[2] = (int32_t)r->pos[2] + arg->pos[2];
		const uint32_t *flags = (const uint32_t *)arg->flags;
		if (flags[(int16_t)r->index] & 0x10000000)
		{
			// camera-aligned: only the position goes through the camera
			TransformVectorBy3x3Matrix(&Camera(), m.t, m.t);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		}
		else ComposeAffineTransform(&Camera(), &m, &m);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t mode = (flags[(int16_t)r->index] & 0xDFFF) | 0x2000;
		*(uint32_t *)(h + 0x1C) = mode;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = mode | 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Particles (draw THEN update)
	// ------------------------------------------------------------------
	static void ParticleDraw(const ParticleNode *p)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(p->pos, p->scale, -((int32_t)p->scale >> 2));
		*(uint32_t *)h = SEQ_Particle;
		*(int16_t *)(h + 4) = p->counter;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void ParticleStep(ParticleNode *p)
	{
		p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
		p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
		p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
		p->counter++;
	}

	// the ring burst's particle count at a main counter in 0x50..0x63: (sin(k * 4096 / 40) >> 9) + 4
	static int32_t BurstCount(int16_t counter)
	{
		const uint32_t x = (uint32_t)((int32_t)counter - 0x50) << 12;
		const uint32_t a = (uint32_t)(((uint64_t)x * 0xCCCCCCCDu) >> 32) >> 5;
		return (ComputeSin((int32_t)a) >> 9) + 4;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag017_elvoret_entrance_held.h"
#endif

namespace ff8fx
{
namespace elvoret017
{
	// ------------------------------------------------------------------
	// Particle (0x700B70)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		// 30 fps layer: see mag017_elvoret_entrance_held.inc
		FX_HELD(held_note_particle(p);)
		ParticleDraw(p);
		ParticleStep(p);
		return p->counter < 0xD ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Main (0x7005C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		if (m->counter == 0x14 || m->counter == 0x28) BdPlayActorSE(Caster(), 0, 0, 0x80);
		if ((uint32_t)(int32_t)m->counter < 0x55)
		{
			if ((uint32_t)(int32_t)m->counter <= 0x20)
			{
				const int32_t t = (int32_t)(((uint32_t)(int32_t)m->counter & 0xFFFFF) << 7);
				ColourLerp(AMBIENT, COLOUR_Black, t, 0x1000 - t, Caster() + 0x2C);
			}
			*(int16_t *)(Caster() + 0x1E) = (int16_t)(*(const int16_t *)(Caster() + 0x1E) + 0x40);
		}
		if ((uint32_t)((int32_t)m->counter - 0x50) < 0x14)
		{
			// the ring burst around the caster's ground point
			int32_t count = BurstCount(m->counter);
			int32_t angle = CrtRand();
			const int32_t step = 0x1000 / count;
			do
			{
				ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
				if (p)
				{
					Memset32(&p->counter, 0, 5);
					const int16_t c = (int16_t)(ComputeCos(angle) >> 2);
					p->vel[0] = c;
					p->pos[0] = c;
					const int16_t s = (int16_t)(ComputeSin(angle) >> 2);
					p->vel[2] = s;
					p->pos[2] = s;
					p->pos[0] = (int16_t)(p->pos[0] + *(const int16_t *)(Caster() + 0x1C));
					p->pos[2] = (int16_t)(*(const int16_t *)(Caster() + 0x20) + s);
					p->pos[1] = *(const int16_t *)(Caster() + 0x24);
					const int32_t r = CrtRand() & 0xFFF;
					const int32_t f = r + (0x400 - (r >> 2));
					p->vel[0] = (int16_t)(mul32(p->vel[0], f) >> 14);
					p->vel[2] = (int16_t)(mul32(p->vel[2], f) >> 14);
					p->scale = (int16_t)((CrtRand() & 0xFFF) + 0x800);
				}
				angle += step;
			} while (--count != 0);
		}
		if ((uint32_t)(int32_t)m->counter < 0x55)
		{
			// the wings: a sound every 25 ticks, up to two particles per tick
			const uint32_t phase = (uint32_t)(int32_t)m->counter % 0x19;
			int16_t sp[4];
			sp[3] = 0; // see below: overwritten by the particle size before anything reads it
			GetEffectSpawnPosition(Caster(), 0x1D, 0x400, sp);
			if (phase == 0) BdPlaySE3D(SOUND_Wings, 0, sp);
			for (int k = 0; k < 2; k++)
			{
				const uint32_t chance = (uint32_t)(mul32(CrtRand(), 20) >> 15);
				if (chance <= phase) continue;
				ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
				if (!p) continue;
				Memset32(&p->counter, 0, 5);
				memcpy(p->pos, sp, 8); // x, y, z and the word after (overwritten by the size)
				p->scale = (int16_t)((CrtRand() & 0x3FF) + 0x800);
				p->vel[0] = (int16_t)((CrtRand() & 0x3F) - 0x20);
				p->vel[1] = (int16_t)((CrtRand() & 0x3F) - 0x20);
				p->vel[2] = (int16_t)((CrtRand() & 0x3F) - 0x20);
			}
		}
		if (m->counter == 0x55)
		{
			// the party reacts
			for (int s = 0; s < 3; s++)
			{
				uint8_t *e = Entity(s);
				if ((e[0] & 2) && !(e[8] & 3)) QueueChainTransformation(e, 4);
			}
		}
		if (m->counter >= 0x32)
		{
			if (m->counter == 0x50) BdPlaySE3D(SOUND_Land, 0, (const int16_t *)(Caster() + 0x1C));
			PrimArg arg;
			arg.pos[0] = *(const int16_t *)(Caster() + 0x1C);
			arg.pos[2] = *(const int16_t *)(Caster() + 0x20);
			arg.pos[1] = *(const int16_t *)(Caster() + 0x24);
			arg.pos[3] = 0; // stack word the original leaves (the callback does not read it)
			arg.flags = PART_Flags;
			arg.morph = TexBase() + 0x1B9C;
			// 30 fps layer: see mag017_elvoret_entrance_held.inc
			FX_HELD(held_note_play(m, &arg);)
			prim::play((prim::Layout *)m->layout, PartCallback, (int)&arg, 0);
		}
		if (m->counter == 0x76) ReleaseVoiceSlot(m->voice);
		const int16_t c = m->counter;
		if (c >= 0x7A) return TASK_END;
		m->counter = (int16_t)(c + 1);
		return 0;
	}

	// ------------------------------------------------------------------
	// Root (0x700460)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag017_elvoret_entrance_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1BA4;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x11BA4;
			r->arena = 1;
		}
		Caster()[1] |= 0x10;
		if (r->counter == 1)
		{
			if (!r->started)
			{
				*(uint16_t *)Caster() &= 0xFFFB;
				r->started = 1;
				InitTaskQueuePool(QMain(), (void *)(TexBase() + 0x1000), 0x1FC, 1);
				InitTaskQueuePool(QParticles(), (void *)TexBase(), 0x20, 0x80);
			}
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			if (m)
			{
				Memset32(&m->counter, 0, 0x7C);
				m->voice = ClaimVoiceSlot(SOUND_Voice, 1, 0x80);
				DecodeModelPrimLayout(MODEL_Layout, m->layout, 0x1E8);
			}
		}
		int a, b;
		if (r->started)
		{
			a = ExecuteTaskQueue(QMain());
			b = ExecuteTaskQueue(QParticles());
		}
		else a = b = (int)n;
		if (r->started && a == 0 && b == 0)
		{
			*(uint16_t *)Caster() &= 0xEFFF;
			return TASK_END;
		}
		r->counter++;
		if (r->counter >= 10) r->counter = 10;
		return 0;
	}
}

	void register_mag017_elvoret_entrance()
	{
		register_port(elvoret017::ORIG_RootTask, (void *)elvoret017::RootTask, "017 RootTask", 17);
		register_port(elvoret017::ORIG_MainTask, (void *)elvoret017::MainTask, "017 MainTask", 17);
		register_port(elvoret017::ORIG_ParticleTask, (void *)elvoret017::ParticleTask, "017 ParticleTask", 17);
		// 30 fps layer: see mag017_elvoret_entrance_held.inc
		FX_HELD(register_mag017_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag017_elvoret_entrance_held.inc"
#endif
