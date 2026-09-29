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

// 30 fps part of act_engine.cpp (see act_engine_held.inc): the held-frame API of the actor engine
// family used by the module held parts, and the functions the FX_HELD statements call (engine and
// module master tasks)
#pragma once
#include "fx_held.h"
#include "act_engine.h"

namespace ff8fx
{
namespace act
{
	// ---- FX_HELD statements of the real tick ----
	// a ported master task (a_739F40, m_731F70, t_7624D0, b_729BD0) runs on this real tick
	void held_note_master();
	// camera steppers a_73AE10 (kind 1) / a_73B4B0 (kind 2) ran on this real tick
	void held_note_camera(int kind);
	// prim_play: remembers the play (layout, callback run f, 0x5C-byte parameter block) for the held frame
	void held_note_prim_play(uint32_t layout, uint32_t f, uint32_t arg);

	// ---- held frames ----
	// held-frame camera of the engine's camera script (register_module_camera)
	bool held_camera(int num, int den, int16_t world[3], int16_t lookat[3]);
	// held frame (30 fps) of a module: in-between redraw of the prim-model plays of this real tick
	// and of the creature actor(s) (task function creature_task in creature_queue, drawn with
	// a_746C10(node, draw_arg, cursor)); module globals [bss_lo, bss_hi) are saved/restored.
	// more: further creature kinds (0-terminated list, e.g. ChocoBocle's two creatures);
	// adjust: called for every creature node after its midpoint pose, before the draw, to move
	// its placement words (node +0x30..+0x5F, put back after the draw) to the in-between state
	// actor_sys: the module's actor-system tasks (see HeldActorSys)
	// node_tasks: the module's node tasks (see HeldNodeTask); more_draws: the module's own held draws;
	// tint_queue: the queue of its screen tints (held_draw_tints; the module marks 0x8DDC30 held)
	struct HeldCreature { uint32_t queue, task, draw_arg; };
	// actor systems: prim-model tasks (queue G_258BA90) whose state 1 steps an actor system at node
	// +0x34 (a_7395E0 / Siren s_743220, s_745210: update pass then draw pass); their actors are
	// redrawn in between (held_draw_actor_systems). model_draw: the module's model-actor draw (kinds
	// 4 / 5; default a_7354A0)
	struct HeldActorSys { uint32_t task; void (*model_draw)(uint32_t act, uint32_t def) = nullptr; };
	// node tasks: tasks that update then draw from their node (held_draw_node_tasks): queue() = the
	// task queue, size = node bytes, step = the task's update on a copy (states, motion; no draw, no
	// spawn), draw = the task's draw from a node, lerp = its continuous words (NT_* bits: position
	// +0x1C s16 x3, angles +0x44 x3, scale +0x30 s32 x3, angle +0x54, 4 particle positions +0x6C)
	enum { NT_POS = 1, NT_ANG = 2, NT_SCALE = 4, NT_ANG54 = 8, NT_PART4 = 16 };
	// cursor: the packet cursor the draw advances (0 = 0x1D8E054; Tonberry's module pool 0x259EEA8)
	struct HeldNodeTask { uint32_t (*queue)(); uint32_t task, size; void (*step)(uint32_t nxt); void (*draw)(uint32_t node); uint32_t lerp; uint32_t cursor = 0; };
	struct HeldDesc
	{
		const Mod *mod;
		uint32_t creature_queue, creature_task, draw_arg, bss_lo, bss_hi;
		const HeldCreature *more = nullptr;
		void (*adjust)(uint32_t node, int num, int den) = nullptr;
		const HeldActorSys *actor_sys = nullptr; // 0-terminated (task 0)
		const HeldNodeTask *node_tasks = nullptr; // 0-terminated (task 0)
		void (*more_draws)(int num, int den) = nullptr; // the module's own held draws (inside the held frame's saved state)
		uint32_t tint_queue = 0; // the queue of its screen tints (engine task 0x8DDC30), 0 = none
	};
	void held_frame(const HeldDesc &d, int num, int den);
	// during a held frame: where the module global `addr` keeps the value the real tick left (the
	// held frame's saved copy of the module globals; the held redraws may have changed the live one)
	const void *held_real_state(uint32_t addr);
	bool held_ready();
	// held frame: in-between redraw of every prim play of the current module on this real tick
	void held_draw_prim_plays(int num, int den);

	// ---- registration ----
	// held_tasks: 0-terminated list of a module's original task addresses whose drawing is redrawn
	// by the module's held frame; marks those among the module's engine ports (register_module)
	// held (mark_held). The module's own ports are marked by its held part.
	void register_module_held_tasks(int effect_id, const uint32_t *held_tasks);
}
}
