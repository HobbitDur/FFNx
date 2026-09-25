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

#include "fx_port.h"

namespace ff8fx
{
	struct Port { uint32_t orig; void *port; const char *name; int effect_id; bool held; };

	static const int PORTS_MAX = 8192;
	static Port g_ports[PORTS_MAX];
	static int g_nports = 0;
	// open-addressed index orig -> port slot (the executor looks every task up)
	static const int INDEX_SIZE = 16384; // power of two, > 2 x PORTS_MAX
	static int16_t g_index[INDEX_SIZE];
	static bool g_index_ready = false;

	bool g_active = false;
	uint32_t g_real_tick = 0;

	struct ModuleHeld { int effect_id; bool (*ready)(); void (*draw)(int, int); };
	static ModuleHeld g_held[64];
	static int g_nheld = 0;

	static inline uint32_t slot_of(uint32_t orig) { return (orig * 2654435761u) >> 18; }

	void register_port(uint32_t orig, void *port, const char *name, int effect_id, bool held)
	{
		if (!g_index_ready)
		{
			for (int i = 0; i < INDEX_SIZE; i++) g_index[i] = -1;
			g_index_ready = true;
		}
		if (g_nports >= PORTS_MAX) return;
		g_ports[g_nports] = { orig, port, name, effect_id, held };
		uint32_t h = slot_of(orig);
		while (g_index[h & (INDEX_SIZE - 1)] >= 0) h++;
		g_index[h & (INDEX_SIZE - 1)] = (int16_t)g_nports;
		g_nports++;
	}

	static const Port *find(uint32_t orig)
	{
		if (!g_index_ready) return nullptr;
		for (uint32_t h = slot_of(orig);; h++)
		{
			int i = g_index[h & (INDEX_SIZE - 1)];
			if (i < 0) return nullptr;
			if (g_ports[i].orig == orig) return &g_ports[i];
		}
	}

	void *lookup(uint32_t orig)
	{
		if (!g_active) return nullptr;
		const Port *p = find(orig);
		return p ? p->port : nullptr;
	}

	const char *port_name(uint32_t orig)
	{
		const Port *p = find(orig);
		return p ? p->name : nullptr;
	}

	bool held_redraws(uint32_t orig)
	{
		const Port *p = find(orig);
		return p && p->held;
	}

	void register_module_held(int effect_id, bool (*ready)(), void (*draw)(int, int))
	{
		if (g_nheld < 64) g_held[g_nheld++] = { effect_id, ready, draw };
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

	bool module_ported(int effect_id)
	{
		for (int i = 0; i < g_nports; i++)
			if (g_ports[i].effect_id == effect_id) return true;
		return false;
	}

	void register_all()
	{
		static bool done = false;
		if (done) return;
		done = true;
		register_mag116_quezacotl();
	}
}
