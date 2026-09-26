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

// Effect 3: Thunder (spell, MAG_003_*).
//
// Structure (setup MAG_003_THUNDER 0x700C10 -> _Init 0x700C20, file loader 0x575610 = nothing;
// the textures are uploaded by the setup from exe data 0x14FA790):
//   RootTask (0x700CA0) - alternates the packet arena (magic buffer + 0 / + 0x8000); every 5
//     ticks (counter 1) spawns the bolt of the next action on the action's first target (it
//     waits while a bolt of 15 frames or less is still running on that target), runs the bolt
//     queue, ends when the queue is empty after the first spawn.
//   Bolt (0x700DE0) - frame 0: full-screen flash quad; frames 1..24: the lightning strike, up to
//     8 sprite-sequence layers keyed on the frame (one of them on a tilted plane at the
//     target's feet); sound at frame 1, damage at frame 5, ends after frame 24.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2557000..0x2557098 (bolt pool 3 x 0x1C, texture base, context, root pool
// 1 x 0x14, packet cursor 0x2557074, queues 0x2557078).

#include "mag_common.h"

namespace ff8fx
{
namespace thunder003
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline TaskQueuePair &Queues() { return var<TaskQueuePair>(0x2557078); } // .first = root, .second = bolts
	inline uint32_t &TexBase() { return var<uint32_t>(0x2557054); }          // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2557058); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2557074); }

	static const uint32_t ORIG_RootTask = 0x700CA0;
	static const uint32_t ORIG_BoltTask = 0x700DE0;
	static const uint32_t BOLT_POOL = 0x2557000;
	static const int BOLT_POOL_COUNT = 3;
	static const void *const SOUND_Thunder = (const void *)0x15244D0;
	static const void *const FLASH_Colour = (const void *)0x15244D4; // flash quad colour word
	static const uint32_t SEQ_Strike = 0x1522FB0;   // frames 0..6
	static const uint32_t SEQ_Upper = 0x1523204;    // frames 2..8, 200 units lower
	static const uint32_t SEQ_Core = 0x1523458;     // frames 1..9
	static const uint32_t SEQ_Arc = 0x1523578;      // frames 3..11
	static const uint32_t SEQ_Glow = 0x1523684;     // frames 6..14
	static const uint32_t SEQ_Flash = 0x15237A4;    // frames 1..9
	static const uint32_t SEQ_Ground = 0x1523DE0;   // frames 1..13, tilted plane at the target's feet
	static const uint32_t SEQ_Smoke = 0x1523928;    // frames 5..23

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// engine functions not in fx_port.h / mag_common.h
	// GetEffectSpawnPosition (0x502170): position of an entity's effect anchor `bone` (0xF1 = centre)
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); } // IR = MAC = R * V0 (no translation)
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C 0..4, a bolt spawn at 1
		uint8_t action;   // +0x0E next action
		uint8_t started;  // +0x0F set at the first spawn tick
		uint32_t arena;   // +0x10 packet arena parity
	};
	struct BoltNode // pool of 3 nodes of 0x1C bytes
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C frame
		int16_t action;   // +0x0E action index
		uint8_t *entity;  // +0x10 target entity
		int16_t pos[3];   // +0x14 effect anchor (x, y, z)
		int16_t size;     // +0x1A (-0x1000 - target size) / 4: sprite size argument
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(BoltNode) == 0x1C, "Thunder nodes");

	// Thunder_InitBoltTask (0x6DA380): rep stosd of `count` dwords
	static void FillDwords(void *dst, uint32_t value, uint32_t count)
	{
		uint32_t *d = (uint32_t *)dst;
		for (uint32_t k = 0; k < count; k++) d[k] = value;
	}

	// MAG_003_EmitScreenFlashQuad (0x7011A0): semi-transparent flat quad over the whole screen
	// (320 x 216) in the colour word at `colour`, in the header bucket render list + 0x20
	static uint32_t ScreenFlashQuad(const void *colour, uint32_t cursor)
	{
		uint8_t *p = (uint8_t *)cursor;
		*(uint16_t *)(p + 8) = 0;
		*(uint16_t *)(p + 0x10) = 0;
		*(uint16_t *)(p + 0xE) = 0;
		*(uint16_t *)(p + 0xA) = 0;
		*(uint16_t *)(p + 0x14) = 0x140;
		*(uint16_t *)(p + 0x16) = 0xD8;
		*(uint16_t *)(p + 0x12) = 0xD8;
		*(uint16_t *)(p + 0xC) = 0x140;
		*(uint32_t *)p = 0x5000000;
		*(uint32_t *)(p + 4) = *(const uint32_t *)colour;
		p[7] = 0x2A;
		InsertPrimAltViewport(var<uint32_t>(0x1D8E04C) + 0x20, p);
		return cursor + 0x18;
	}

	// MAG_003_sub_6D5940: rotation about the X axis (translation and pad untouched)
	static void RotX(int16_t angle, Mat4x3 *m)
	{
		int32_t s = ComputeSin(angle);
		int32_t c = ComputeCos(angle);
		m->m[1][2] = (int16_t)-s;
		m->m[2][1] = (int16_t)s;
		m->m[0][0] = 0x1000;
		m->m[0][1] = 0;
		m->m[0][2] = 0;
		m->m[1][0] = 0;
		m->m[1][1] = (int16_t)c;
		m->m[2][0] = 0;
		m->m[2][2] = (int16_t)c;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag003_thunder_held.h"
#endif

namespace ff8fx
{
namespace thunder003
{
	// ------------------------------------------------------------------
	// Root task (0x700CA0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag003_thunder_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase();
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x8000;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			r->started = 1;
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				// wait while a bolt of 15 frames or less runs on this target
				BoltNode *pool = (BoltNode *)BOLT_POOL;
				bool busy = false;
				for (int k = 0; k < BOLT_POOL_COUNT; k++)
					if ((pool[k].hdr.flags & 1) && pool[k].entity == entity && pool[k].counter <= 0xF)
					{
						busy = true;
						break;
					}
				if (busy) r->counter = 0;
				else
				{
					BoltNode *b = (BoltNode *)AddTaskToQueue(&Queues().second, ORIG_BoltTask);
					FillDwords(&b->counter, 0, 4);
					b->action = (int16_t)(uint16_t)r->action;
					b->entity = entity;
					GetEffectSpawnPosition(entity, 0xF1, 0, b->pos);
					b->size = (int16_t)((-0x1000 - (int32_t)*(int16_t *)(b->entity + 0x26)) >> 2);
					r->action++;
				}
			}
		}
		int alive = ExecuteTaskQueue(&Queues().second);
		if (r->started && !alive) return TASK_END;
		r->counter++;
		if (r->counter >= 5) r->counter = 0;
		return 0;
	}

	// ------------------------------------------------------------------
	// Bolt (0x700DE0)
	// ------------------------------------------------------------------
	static void Layer(uint8_t *h, uint32_t seq, int32_t frame)
	{
		*(uint32_t *)h = seq;
		*(uint16_t *)(h + 4) = (uint16_t)frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
	}

	// frame f >= 0 of the strike (bolt counter - 1)
	static void BoltLayers(const BoltNode *p, int32_t f)
	{
		uint8_t *h = AllocHeader(0xB4);
		// the stack slot of the lowered copy of the anchor is reused by the ground layer's vector:
		// its 4th word is the size when the lowered layer ran this tick (only the GTE's unused
		// VZ0 high half sees it)
		int16_t local[4] = { 0, 0, 0, 0 };
		if (f < 7)
		{
			TransformCameraByShadowRotation(p->pos, 0x1000, p->size);
			Layer(h, SEQ_Strike, f);
		}
		if (f >= 2 && f < 9)
		{
			int32_t size = p->size;
			memcpy(local, p->pos, 8);
			local[1] = (int16_t)(local[1] + 0xC8);
			TransformCameraByShadowRotation(local, 0x1000, size);
			Layer(h, SEQ_Upper, f - 2);
		}
		if (f >= 1 && f < 10)
		{
			TransformCameraByShadowRotation(p->pos, 0x1000, p->size);
			Layer(h, SEQ_Core, f - 1);
		}
		if (f >= 3 && f < 12)
		{
			TransformCameraByShadowRotation(p->pos, 0x1000, p->size);
			Layer(h, SEQ_Arc, f - 3);
		}
		if (f >= 6 && f < 15)
		{
			TransformCameraByShadowRotation(p->pos, 0x1000, p->size);
			Layer(h, SEQ_Glow, f - 6);
		}
		if (f >= 1)
		{
			if (f < 10)
			{
				TransformCameraByShadowRotation(p->pos, 0x1000, p->size);
				Layer(h, SEQ_Flash, f - 1);
			}
			if (f < 14)
			{
				// plane tilted 0x5A about X at (x, target height, z) in camera space, pushed 500 away
				Mat4x3 m = {};
				RotX(0x5A, &m);
				local[0] = p->pos[0];
				local[1] = *(int16_t *)(p->entity + 0x24);
				local[2] = p->pos[2];
				GteSetRotMatrixCtrl(&Camera());
				GteLoadV0(local);
				GteMVMVA_RotV0();
				GteReadMAC123(m.t);
				m.t[0] += Camera().t[0];
				m.t[1] += Camera().t[1];
				m.t[2] = m.t[2] + Camera().t[2] + 0x1F4;
				GteSetRotMatrix(&m);
				GteSetTransVector(&m);
				Layer(h, SEQ_Ground, f - 1);
			}
		}
		if (f >= 5 && f < 0x18)
		{
			TransformCameraByShadowRotation(p->pos, 0x1000, p->size);
			Layer(h, SEQ_Smoke, f - 5);
		}
		FieldFree(0xB4);
	}

	// what the bolt draws at its counter (frame 0 = the flash quad)
	static void BoltDraw(const BoltNode *p)
	{
		if (p->counter == 0) PacketCursor() = ScreenFlashQuad(FLASH_Colour, PacketCursor());
		if (p->counter >= 1) BoltLayers(p, p->counter - 1);
	}

	static uint32_t __cdecl BoltTask(TaskNode *n)
	{
		BoltNode *p = (BoltNode *)n;
		// (the flash is drawn at counter 0 only, the sound plays at counter 1 before the layers)
		if (p->counter == 1) BdPlaySE3D(SOUND_Thunder, 0x101, p->pos);
		// 30 fps layer: see mag003_thunder_held.inc
		FX_HELD(held_note_draw(ORIG_BoltTask, p);)
		BoltDraw(p);
		if (p->counter == 5) ApplyActionResultToTarget(Ctx()->actions[p->action].targets);
		p->counter++;
		return p->counter < 0x19 ? 0 : TASK_END;
	}
}

	void register_mag003_thunder()
	{
		register_port(thunder003::ORIG_RootTask, (void *)thunder003::RootTask, "T003 RootTask", 3);
		register_port(thunder003::ORIG_BoltTask, (void *)thunder003::BoltTask, "T003 BoltTask", 3);
		// 30 fps layer: see mag003_thunder_held.inc
		FX_HELD(register_mag003_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag003_thunder_held.inc"
#endif
