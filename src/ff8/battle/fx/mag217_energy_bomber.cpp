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

// Effect 217: Energy Bomber (enemy attack 316 of kernel.bin, "Engery Bomber"; Adel c0m112; MAG_217_*). No other
// kernel.bin entry uses effect 217.
//
// Structure (setup MAG_217 0x694E60 -> _Init 0x694E90, file loader 0x694E70 = the TIM named at 0x113C19C into
// 0x25087F0; the setup keeps the CASTER's entity and the entity of target 0 of action 0, starts camera animation
// 0x11355D4 and queues the TIM):
//   RootTask (0x694F30) - alternates the packet arena (magic buffer + 0x1244 / + 0x13244); on counter 1 sets up the
//     pools once (main 1 x 0x684 at + 0x800, sparks 0x40 x 0x20 at + 0) and the main task (layout 0x1135CDC decoded
//     into its +0x44, the target's default effect position 0x571400 into its +0x28); then the main and spark queues;
//     ends when the main queue is empty (sparks still alive are dropped).
//   MainTask (0x695050) - node 0x684, 0x57 ticks:
//     screen flash 0x5713E0 every tick: 0x8FC c / 12 up to 0xB, 0x8FC, (0x56 - c) 0x8FC / 12 from 0x4A (0 at 0x56);
//     counters 0..0x42 the energy ball, layout 0x1135CDC played at its point P (depth -0x80, blended frames at magic
//     buffer + 0xE84): 0..0x2E at the caster's bone 0x27 (0x502170) 0x200 higher (0x2E: its velocity = the last
//     step), 0x2F..0x36 thrown along that velocity, 0x37..0x42 three eased hops (four nested sines of the step: a
//     random point 0x800 around the target 2000 above it (x) and 3/4 of the way back in z, then the point mirrored
//     in x, then the target; 0x56CB90 from the last point); from 0x37 a two-point trail of the ball (prim model
//     0x1135A6C at each point through the camera, faded 0xC00 / 0xE00, the engine's renderer 0x572200);
//     counters 0x3F..0x55 the burst layout 0x113A5DC (decoded at 0x3F into +0x3DC) at the target's position, 0x40..0x43
//     sixteen sparks a tick from it (random direction 0x6A6020 x 0x200..0x3FF, drag -v / 16);
//     sound 0x11355D0 at 0, the target's action result at 0x4E; ends at 0x56 (the flash off).
//   SparkTask (0x695930) - node 0x20, 0xC ticks, DRAWS THEN UPDATES: sprite sequence 0x1135900 through the
//     shadow-rotation camera (0x800, -0x200), flipbook frame = its counter; velocity += drag, position += velocity.
//   PrimCallback (0x695690, Holy Water 296's 0x673BD0: layouts::AnchorPrimCallback with the record flags 0x40000 and
//     0x200) draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: the main task updates the ball, then draws the layout, the trail, the burst; the sparks draw then update.
// ENTITY / CAMERA: no task writes an entity or the camera words (the camera animation is the engine's); the screen
// flash (engine state 0x5713E0); the target's action result (0x506690).
// Module globals: 0x25087F0..0x2508870 (texture file 0x25087F0, context 0x25087F4, caster entity 0x25087F8, target
// entity 0x25087FC, queues 0x2508800 sparks / 0x2508810 main / 0x2508820 root, root pool 0x2508830, packet cursor
// 0x2508844, magic buffer base 0x250886C).
// NEVER-WRITTEN STACK WORDS: the ball point's 4th word (MainTask's local P +6: 0x502170 and 0x56CB90 store 3 words)
// is copied into the node (+0x16 on 0..0x2E, the trail points' +6) and the callback block (GTE V0 pad only); read by
// nothing that draws - modelled by PointPad: on counter 0 (the root's setup tick) the high half of the entity pointer the
// setup's GetDefaultEffectPosition 0x571400 call left there (0x01D9), later 0 (verified under the game executor, oe).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace bomber217
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexFile() { return var<uint32_t>(0x25087F0); }               // the effect's file (its TIM)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25087F4); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x25087F8); }               // caster entity (setup)
	inline uint8_t *&Target() { return var<uint8_t *>(0x25087FC); }               // target 0 of action 0 (setup)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2508800; }                // pool: + 0, 0x40 x 0x20
	inline TaskQueue *QMain() { return (TaskQueue *)0x2508810; }                  // pool: + 0x800, 1 x 0x684
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2508844); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x250886C); }               // Magic_TextureOFF (magic buffer)

	static const uint32_t ORIG_RootTask = 0x694F30;
	static const uint32_t ORIG_MainTask = 0x695050;
	static const uint32_t ORIG_SparkTask = 0x695930;
	static const uint32_t ORIG_PrimCallback = 0x695690; // not a task: the layouts' prim callback
	static const uint32_t POOL_Main = 0x800;
	static const uint32_t MORPH = 0xE84;                 // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x1244;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x13244;
	static const uint32_t LAYOUT_Ball = 0x1135CDC;       // prim-model layouts
	static const uint32_t LAYOUT_Burst = 0x113A5DC;
	static const uint32_t MODEL_Trail = 0x1135A6C;       // prim model
	static const uint32_t SEQ_Spark = 0x1135900;         // sprite sequence
	static const void *const SOUND_Start = (const void *)0x11355D0;
	static const uint32_t FLASH = 0x8FC;                 // screen flash level

	// engine functions not in fx_port.h / mag_common.h
	using layouts::GetEffectSpawnPosition;
	using layouts::DecodeModelPrimLayout;
	inline void BlendVectors(const int16_t *a, const int16_t *b, int32_t wa, int32_t wb, int16_t *out) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int16_t *)>(0x56CB90)(a, b, wa, wb, out); } // (a wa + b wb) >> 12, 3 words
	inline void Apply3D(const Mat4x3 *m, const int16_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int32_t *)>(0x56C3D0)(m, in, out); } // apply3DTransform (rotation only)
	inline void DirectionFromAngles(int32_t pitch, int32_t yaw, int16_t *out) { fn<void (__cdecl *)(int32_t, int32_t, int16_t *)>(0x6A6020)(pitch, yaw, out); } // MAG_063_sub_6A6020
	inline uint32_t OT() { return var<uint32_t>(0x1D8E04C); }                    // g_Battle_FrameRenderListBase

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2508830)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x684 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[4];    // +0x10 the ball's last point (0..0x36) / the hop's start (0x37..)
		int16_t vel[3];    // +0x18 its throw velocity (0x2E)
		int16_t pad1E;
		int16_t hop[4];    // +0x20 the hop's end
		int16_t target[4]; // +0x28 the target's default effect position
		int16_t trail[2][4]; // +0x30 the trail points (newest first)
		int16_t count;     // +0x40 trail points
		int16_t index;     // +0x42 first trail point drawn
		uint8_t layout_ball[0x398];  // +0x44
		uint8_t layout_burst[0x2A8]; // +0x3DC
	};
	struct SparkNode // pool of 0x40 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
		int16_t vel[3];    // +0x14
		int16_t drag[3];   // +0x1A -velocity / 16, added to the velocity every tick
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(MainNode) == 0x684 && sizeof(SparkNode) == 0x20, "217 nodes");

	// the ball point's 4th word: never written (stack residue in the original). Counter 0 runs in the root's setup tick: the
	// word holds what that setup's calls left there, the high half of a battle-entity pointer (0x1D972C0 + slot x 0x9C: 0x01D9,
	// the target pushed for GetDefaultEffectPosition 0x571400); later counters: the tick-start residue, 0 (game executor)
	static int16_t PointPad(int32_t c) { return c == 0 ? (int16_t)((uint32_t)Target() >> 16) : 0; }

	// the play of a layout at a point (the callback block: point, depth -0x80, blended frames)
	static void Play(uint8_t *layout, const int16_t *point)
	{
		layouts::AnchorArg arg;
		memcpy(arg.anchor, point, 8);
		arg.depth = -0x80;
		arg.morph = TexBase() + MORPH;
		arg.honoured = layouts::ANCHOR_ROT_YXZ | layouts::ANCHOR_OFFSET_AS_IS;
		arg.cursor = &PacketCursor();
		prim::play((prim::Layout *)layout, layouts::AnchorPrimCallback, (int)&arg, 0);
	}

	// the hop weight of step k: four nested sines
	static int32_t HopWeight(int32_t k)
	{
		int32_t s = ComputeSin((int32_t)((((uint32_t)k << 12) >> 2) + 0x200) >> 2);
		s = ComputeSin(s >> 2);
		s = ComputeSin(s >> 2);
		return ComputeSin(s >> 2);
	}

	// the ball's point on counter c (< 0x43): the node's state stepped as that tick does (P: 4 words). rand_hop
	// = the random hop end of 0x37 (the real tick draws it: CRT rand); without it that counter is not computed
	// (false)
	static bool BallPoint(MainNode *m, int32_t c, int16_t *P, bool rand_hop)
	{
		if (c < 0x2F)
		{
			GetEffectSpawnPosition(Caster(), 0x27, 0, P);
			P[1] = (int16_t)(P[1] - 0x200);
			if (c == 0x2E)
			{
				m->vel[0] = (int16_t)(P[0] - m->pos[0]);
				m->vel[1] = (int16_t)(P[1] - m->pos[1]);
				m->vel[2] = (int16_t)(P[2] - m->pos[2]);
			}
			memcpy(m->pos, P, 8);
			return true;
		}
		if (c < 0x37)
		{
			m->pos[0] = (int16_t)(m->pos[0] + m->vel[0]);
			m->pos[1] = (int16_t)(m->pos[1] + m->vel[1]);
			m->pos[2] = (int16_t)(m->pos[2] + m->vel[2]);
			memcpy(P, m->pos, 8);
			return true;
		}
		int32_t s;
		if (c < 0x3B)
		{
			s = HopWeight(c - 0x37);
			if (c == 0x37)
			{
				if (!rand_hop) return false;
				const int32_t r = CrtRand();
				m->hop[1] = (int16_t)(m->target[1] - 0x7D0);
				m->hop[0] = (int16_t)((int16_t)((r & 0xFFF) + m->target[0]) - 0x800);
				m->hop[2] = (int16_t)(((int32_t)m->pos[2] - m->target[2]) * 3 / 4 + m->target[2]);
			}
		}
		else if (c < 0x3F)
		{
			s = HopWeight(c - 0x3B);
			if (c == 0x3B)
			{
				memcpy(m->pos, m->hop, 8);
				m->hop[0] = (int16_t)-m->hop[0];
				m->hop[1] = m->target[1];
				m->hop[2] = (int16_t)(((int32_t)m->pos[2] - m->target[2]) * 3 / 4 + m->target[2]);
			}
		}
		else
		{
			s = HopWeight(c - 0x3F);
			if (c == 0x3F)
			{
				memcpy(m->pos, m->hop, 8);
				m->hop[0] = m->target[0];
				m->hop[1] = m->target[1];
				m->hop[2] = m->target[2];
			}
		}
		BlendVectors(m->pos, m->hop, 0x1000 - s, s, P);
		return true;
	}

	// the trail point i (prim model through the camera, faded 0xC00 + i x 0x200) with the header h
	static void TrailDraw(uint8_t *h, const int16_t *point, int32_t i)
	{
		Mat4x3 *m = (Mat4x3 *)(h + 0x58);
		Apply3D(&Camera(), point, m->t);
		m->t[0] = (int32_t)((uint32_t)m->t[0] + (uint32_t)Camera().t[0]);
		m->t[1] = (int32_t)((uint32_t)m->t[1] + (uint32_t)Camera().t[1]);
		m->t[2] = (int32_t)((uint32_t)m->t[2] + (uint32_t)Camera().t[2]);
		m->m[2][2] = 0x1000;
		m->m[1][1] = 0x1000;
		m->m[0][0] = 0x1000;
		m->m[2][1] = 0;
		m->m[2][0] = 0;
		m->m[1][2] = 0;
		m->m[1][0] = 0;
		m->m[0][2] = 0;
		m->m[0][1] = 0;
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		*(uint32_t *)h = MODEL_Trail;
		*(uint32_t *)(h + 0x1C) = 0x30;
		*(uint32_t *)(h + 8) = 0;
		*(int32_t *)(h + 0xC) = shl32(i, 10) / 2 + 0xC00;
		PacketCursor() = RenderPrimModel(h, OT() + 0x44, 2, PacketCursor());
	}

	// the spark (0x695930..0x69598F): sequence frame at pos through the shadow-rotation camera
	static void SparkDraw(int16_t *pos, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, 0x800, -0x200);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Spark;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, OT() + 0x44, 2, PacketCursor());
		FieldFree(0xB4);
	}

	static int32_t FlashLevel(int32_t c)
	{
		if (c < 0xC) return mul32(c, FLASH) / 12;
		if (c >= 0x4A) return mul32(0x56 - c, FLASH) / 12;
		return FLASH;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag217_energy_bomber_held.h"
#endif

namespace ff8fx
{
namespace bomber217
{
	// ------------------------------------------------------------------
	// Spark (0x695930)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		// 30 fps layer: see mag217_energy_bomber_held.inc
		FX_HELD(held_note_spark(n);)
		SparkDraw(s->pos, s->counter);
		s->vel[0] = (int16_t)(s->vel[0] + s->drag[0]);
		s->vel[1] = (int16_t)(s->vel[1] + s->drag[1]);
		s->vel[2] = (int16_t)(s->vel[2] + s->drag[2]);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->counter++;
		return s->counter < 0xC ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Main (0x695050)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		SetScreenFlash(FlashLevel(m->counter), 0);
		const int32_t c = m->counter;
		if ((uint32_t)c < 0x43)
		{
			int16_t P[4];
			P[3] = PointPad(c);
			BallPoint(m, c, P, true);
			// 30 fps layer: see mag217_energy_bomber_held.inc
			FX_HELD(held_note_ball(m, P);)
			Play(m->layout_ball, P);
			if (m->count > 0)
			{
				// 30 fps layer: see mag217_energy_bomber_held.inc
				FX_HELD(held_note_trail(m);)
				uint8_t *h = (uint8_t *)FieldAlloc(0x78);
				for (int32_t i = m->index; i < m->count; i++) TrailDraw(h, m->trail[i], i);
				FieldFree(0x78);
				m->index++;
			}
			if ((uint32_t)c >= 0x37)
			{
				memcpy(m->trail[1], m->trail[0], 8);
				m->count++;
				memcpy(m->trail[0], P, 8);
				if (m->count > 2) m->count = 2;
				m->index = 0;
			}
		}
		const int32_t k = m->counter - 0x3F;
		if ((uint32_t)k < 0x17)
		{
			if (k == 0) DecodeModelPrimLayout(LAYOUT_Burst, m->layout_burst, 0x2A8);
			else if (k < 5)
			{
				SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
				for (int left = 0x10; s; )
				{
					s->counter = 0;
					s->pos[0] = m->target[0];
					s->pos[1] = m->target[1];
					s->pos[2] = m->target[2];
					s->counter = 0;
					const int32_t yaw = CrtRand();
					const int32_t pitch = CrtRand();
					DirectionFromAngles(pitch, yaw, s->vel);
					const int32_t speed = (CrtRand() & 0x1FF) + 0x200;
					s->vel[0] = (int16_t)(mul32(s->vel[0], speed) >> 12);
					s->vel[1] = (int16_t)(mul32(s->vel[1], speed) >> 12);
					s->vel[2] = (int16_t)(mul32(s->vel[2], speed) >> 12);
					s->drag[0] = (int16_t)(-(int32_t)s->vel[0] >> 4);
					s->drag[1] = (int16_t)(-(int32_t)s->vel[1] >> 4);
					s->drag[2] = (int16_t)(-(int32_t)s->vel[2] >> 4);
					if (--left == 0) break;
					s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
				}
			}
			// 30 fps layer: see mag217_energy_bomber_held.inc
			FX_HELD(held_note_burst(m);)
			Play(m->layout_burst, m->target);
		}
		if (m->counter == 0) BdPlaySE(SOUND_Start, 0, 0x80);
		if (m->counter == 0x4E) ApplyActionResultToTarget(Ctx()->actions->targets);
		if (m->counter >= 0x56)
		{
			if (m->counter == 0x56)
			{
				SetScreenFlash(0, 0);
				m->counter++;
			}
			return TASK_END;
		}
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root (0x694F30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *rt = (RootNode *)n;
		// 30 fps layer: see mag217_energy_bomber_held.inc
		FX_HELD(held_note_root();)
		if (rt->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			rt->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			rt->arena = 1;
		}
		if (rt->counter == 1 && !rt->started)
		{
			rt->started = 1;
			InitTaskQueuePool(QMain(), (void *)(TexBase() + POOL_Main), 0x684, 1);
			InitTaskQueuePool(QSparks(), (void *)TexBase(), 0x20, 0x40);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&m->counter, 0, 0x19E);
			DecodeModelPrimLayout(LAYOUT_Ball, m->layout_ball, 0x398);
			GetDefaultEffectPosition(Target(), m->target);
		}
		int main = (int)n;
		if (rt->started)
		{
			main = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QSparks());
		}
		if (rt->started && main == 0)
		{
			// 30 fps layer: see mag217_energy_bomber_held.inc
			FX_HELD(held_note_end();)
			return TASK_END;
		}
		rt->counter++;
		return 0;
	}
}

	void register_mag217_energy_bomber()
	{
		register_port(bomber217::ORIG_RootTask, (void *)bomber217::RootTask, "217 RootTask", 217);
		register_port(bomber217::ORIG_MainTask, (void *)bomber217::MainTask, "217 MainTask", 217);
		register_port(bomber217::ORIG_SparkTask, (void *)bomber217::SparkTask, "217 SparkTask", 217);
		// 30 fps layer: see mag217_energy_bomber_held.inc
		FX_HELD(register_mag217_held();)
	}
	FX_LAB_MODULE(register_mag217_energy_bomber);
}

#ifdef FF8_FX_HELD
#include "mag217_energy_bomber_held.inc"
#endif
