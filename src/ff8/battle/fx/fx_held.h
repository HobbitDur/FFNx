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
	// held frame: the battle camera shake offsets (0x1D97710, x/y/z) the next real tick leaves,
	// as an effect's prediction computed them (bit c of written = component c written by the
	// effect); the frame's view is built with the in-between offsets (ff8_opengl.cpp)
	void held_shake_next(const int16_t next[3], unsigned written);
	// held frame: battle entities (FF8BattleEntitySlotData) the effect moves itself. A real frame draws
	// an entity with the fields of the tick BEFORE it (the engine renders, then rebuilds its matrix); the
	// held frames between the real frames of ticks N and N+1 draw the matrix of lerp(fields N-1, fields N)
	// (the gate keeps both, ff8_opengl.cpp ff8_bgate_ent_*; real frames are never touched). The module
	// lists the entities it drives and says which changes are not motion. Components:
	//   0..2  based position +0x1C / +0x1E / +0x20      3..5  angles +0x0C / +0x0E / +0x10 (short way)
	//   6..8  state offset *(+0x74) + 0x24 / 0x26 / 0x28
	//   9..11 render translation +0x54 / +0x58 / +0x5C, ONLY for an effect that writes it into the matrix
	//         itself on the real tick ("direct": that frame draws it): from = what tick N wrote, next =
	//         what tick N+1 writes (the effect predicts it)
	// jump bit c (0..8): the change of component c made by tick N (between the frames of N-1 and N) is
	// discrete (teleport, put back, start / end placement): the held frames keep drawing tick N-1's value.
	// jump bit c (9..11): the direct translation holds from; move bit c (9..11): lerp(from, next).
	// bit 12 (HE_DROT): the effect writes the matrix rotation itself (drot, the 3x3 s16 of +0x40..+0x51)
	// on tick N and N+1 (or only N): the held frames draw drot.
	// A listed entity belongs to the effect on this frame: the gate's steady-motion extrapolation
	// (ff8_bgate_move_nudge) leaves it alone.
	struct HeldEntity { uint8_t *ent; uint16_t move, jump; int32_t from[12], next[12]; int16_t drot[9]; };
	enum : uint16_t { HE_POS = 0x007, HE_ANG = 0x038, HE_OFS = 0x1C0, HE_TRANS = 0xE00, HE_DROT = 0x1000, HE_ALL = 0x1FFF };
	// fills out[0..max) and returns the count (0 = the effect drives no entity now)
	typedef int (*HeldEntityFn)(HeldEntity *out, int max);
	void register_module_entity(int effect_id, HeldEntityFn fn);
	int held_entities(int effect_id, HeldEntity *out, int max);
	// module helpers: field c of an entity (nullptr: no state block); an entry for ent (nothing marked);
	// a discrete change of tick N; a direct translation (9..11) continuous or held
	inline int16_t *held_entity_field(uint8_t *ent, int c)
	{
		if (c < 3) return (int16_t *)(ent + 0x1C) + c;
		if (c < 6) return (int16_t *)(ent + 0x0C) + (c - 3);
		uint8_t *st = *(uint8_t **)(ent + 0x74);
		return st ? (int16_t *)(st + 0x24) + (c - 6) : nullptr;
	}
	inline void held_entity_init(HeldEntity &h, uint8_t *ent)
	{
		h.ent = ent;
		h.move = h.jump = 0;
		for (int c = 0; c < 12; c++) h.from[c] = h.next[c] = 0;
		for (int k = 0; k < 9; k++) h.drot[k] = 0;
	}
	inline void held_entity_jump(HeldEntity &h, int c) { h.jump |= (uint16_t)(1u << c); h.move &= (uint16_t)~(1u << c); }
	inline void held_entity_direct(HeldEntity &h, int c, int32_t from, int32_t next)
	{
		h.from[c] = from;
		h.next[c] = next;
		h.move |= (uint16_t)(1u << c);
	}
	inline void held_entity_direct_hold(HeldEntity &h, int c, int32_t from)
	{
		h.from[c] = h.next[c] = from;
		held_entity_jump(h, c);
	}
	inline void held_entity_direct_rot(HeldEntity &h, const int16_t r[9])
	{
		for (int k = 0; k < 9; k++) h.drot[k] = r[k];
		h.jump |= HE_DROT;
	}
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
