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

// 30 fps layer of the native effect code (True30FPS branch, built with FF8_FX_HELD).
//
// The vanilla code (fx_port.h and the module files) runs every logic tick exactly like the
// original game. On the extra frames between two logic ticks ("held frames") each module's
// held part draws the exact in-between state: magNNN_held.h (declarations used by the
// FX_HELD(...) statements of the vanilla file) and magNNN_held.inc (the held-frame drawing),
// both included by the vanilla file when FF8_FX_HELD is defined.

#pragma once

#include "fx_port.h"

namespace ff8fx
{
	// g_real_tick counts the effect's real (logic) ticks; the frame gate increments it before each.
	extern uint32_t g_real_tick;

	// A module's ready() says its ports ran on the last real tick (their memos are current);
	// draw(num, den) draws the state at tick + num/den.
	void register_module_held(int effect_id, bool (*ready)(), void (*draw)(int num, int den));
	bool held_ready(int effect_id);
	void held_draw(int effect_id, int num, int den);
	// task functions whose in-between state their module redraws (the generic packet replay
	// skips the packets those tasks drew on the last real tick)
	void mark_held(uint32_t orig);
	bool held_redraws(uint32_t orig);
	// held-frame camera of an effect that writes the battle camera itself (eye / look-at words
	// at 0xB8B7F0 / 0xB8B7F8): the module predicts the next tick's camera and returns the
	// in-between one; false = it does not drive the camera now
	typedef bool (*HeldCameraFn)(int num, int den, int16_t world[3], int16_t lookat[3]);
	void register_module_camera(int effect_id, HeldCameraFn fn);
	bool held_camera(int effect_id, int num, int den, int16_t world[3], int16_t lookat[3]);
	// registrations that belong to no single module (fx_held.cpp); called at the end of
	// register_all() (each module registers its own held part from its register function)
	void register_all_held();

	// Pose of a standard battle model (BattleAnimHeader + its BattleAnimCmd) at tick + num/den:
	// the engine reader runs one frame ahead on copies, the bone matrices are built from the
	// exact midpoint pose (angles the short way round), the real pose values are put back.
	// A completed animation holds its pose. (Implemented in ff8_opengl.cpp.)
	void pose_midpoint(void *anim_header, void *anim_cmd, int num, int den);

	// per-node memo of the state a task drew on the current real tick (for the in-between
	// state of tasks that draw then update). Entries from older ticks count as free slots.
	template<typename T, int N = 1024>
	struct NodeMemo
	{
		struct Slot { const void *node; uint32_t tick; T v; };
		Slot s[N];
		static uint32_t hash(const void *p) { return ((uint32_t)p * 2654435761u) >> 10; }
		T *put(const void *node)
		{
			for (uint32_t h = hash(node), i = 0; i < N; i++)
			{
				Slot &x = s[(h + i) & (N - 1)];
				if (x.tick != g_real_tick || x.node == node) { x.node = node; x.tick = g_real_tick; return &x.v; }
			}
			return nullptr;
		}
		const T *get(const void *node) const
		{
			for (uint32_t h = hash(node), i = 0; i < N; i++)
			{
				const Slot &x = s[(h + i) & (N - 1)];
				if (x.tick != g_real_tick) return nullptr;
				if (x.node == node) return &x.v;
			}
			return nullptr;
		}
	};

	// a + (b - a) * num / den, on the integer types the game uses (angles wrap as int16)
	inline int32_t lerp_i(int32_t a, int32_t b, int num, int den) { return a + (b - a) * num / den; }
	inline int16_t lerp_angle(int16_t a, int16_t b, int num, int den) { return (int16_t)(a + (int16_t)(b - a) * num / den); }

	namespace prim
	{
		// held frame: calls cb with the exact in-between records of what play drew on this real
		// tick (nothing when it drew nothing); never changes the player's state
		void play_held(Layout *l, Callback cb, int arg, int num, int den);
	}
}
