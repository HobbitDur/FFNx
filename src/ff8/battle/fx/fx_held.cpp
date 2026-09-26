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

// Registry of the 30 fps layer (see fx_held.h): which modules draw held frames themselves,
// their held-frame cameras, and the task functions whose packets the generic replay skips.
// Filled by the modules' held register functions (FX_HELD seams at the end of each vanilla
// register function) and by register_all_held() below, all from register_all().

#include "fx_held.h"

namespace ff8fx
{
	uint32_t g_real_tick = 0;

	// ------------------------------------------------------------------ module held frames
	struct ModuleHeld { int effect_id; bool (*ready)(); void (*draw)(int, int); };
	static const int MODULES_MAX = 128;
	static ModuleHeld g_held[MODULES_MAX];
	static int g_nheld = 0;

	void register_module_held(int effect_id, bool (*ready)(), void (*draw)(int, int))
	{
		if (g_nheld < MODULES_MAX) g_held[g_nheld++] = { effect_id, ready, draw };
	}

	bool held_ready(int effect_id)
	{
		for (int i = 0; i < g_nheld; i++)
			if (g_held[i].effect_id == effect_id) return g_held[i].ready();
		return false;
	}

	void held_draw(int effect_id, int num, int den)
	{
		for (int i = 0; i < g_nheld; i++)
			if (g_held[i].effect_id == effect_id) { g_held[i].draw(num, den); return; }
	}

	// ------------------------------------------------------------------ held-frame cameras
	static struct { int effect_id; HeldCameraFn fn; } g_cam[MODULES_MAX];
	static int g_ncam = 0;

	void register_module_camera(int effect_id, HeldCameraFn fn)
	{
		if (g_ncam < MODULES_MAX) g_cam[g_ncam++] = { effect_id, fn };
	}

	bool held_camera(int effect_id, int num, int den, int16_t world[3], int16_t lookat[3])
	{
		for (int i = 0; i < g_ncam; i++)
			if (g_cam[i].effect_id == effect_id) return g_cam[i].fn(num, den, world, lookat);
		return false;
	}

	// ------------------------------------------------------------------ held tasks
	// open-addressed set of task function addresses (the replay asks for every primitive)
	static const int HELD_TASKS_SIZE = 2048; // power of two, > 2 x the marked tasks
	static uint32_t g_held_tasks[HELD_TASKS_SIZE];
	static int g_nheld_tasks = 0;

	static inline uint32_t task_slot(uint32_t orig) { return (orig * 2654435761u) >> 21; }

	void mark_held(uint32_t orig)
	{
		if (!orig || held_redraws(orig) || g_nheld_tasks >= HELD_TASKS_SIZE / 2) return;
		uint32_t h = task_slot(orig);
		while (g_held_tasks[h & (HELD_TASKS_SIZE - 1)]) h++;
		g_held_tasks[h & (HELD_TASKS_SIZE - 1)] = orig;
		g_nheld_tasks++;
	}

	bool held_redraws(uint32_t orig)
	{
		for (uint32_t h = task_slot(orig);; h++)
		{
			uint32_t v = g_held_tasks[h & (HELD_TASKS_SIZE - 1)];
			if (!v) return false;
			if (v == orig) return true;
		}
	}

	// ------------------------------------------------------------------ shared registrations
	// the shared camera-script task 0x63E9C0 (Doomtrain's code file, mag191_doomtrain_held.inc)
	// drives the camera of several modules (191 registers it itself)
	bool camscript_held_camera(int num, int den, int16_t world[3], int16_t lookat[3]);

	void register_all_held()
	{
		register_module_camera(185, camscript_held_camera);
		register_module_camera(199, camscript_held_camera);
		register_module_camera(187, camscript_held_camera);
		for (int id = 327; id <= 330; id++) register_module_camera(id, camscript_held_camera);
		register_module_camera(326, camscript_held_camera);
	}
}
