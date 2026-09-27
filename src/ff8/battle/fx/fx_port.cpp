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
#ifdef FF8_FX_HELD
#include "fx_held.h"
#endif

namespace ff8fx
{
	struct Port { uint32_t orig; void *port; const char *name; int effect_id; };

	static const int PORTS_MAX = 8192;
	static Port g_ports[PORTS_MAX];
	static int g_nports = 0;
	// open-addressed index orig -> port slot (the executor looks every task up)
	static const int INDEX_SIZE = 16384; // power of two, > 2 x PORTS_MAX
	static int16_t g_index[INDEX_SIZE];
	static bool g_index_ready = false;

	bool g_active = false;

	static inline uint32_t slot_of(uint32_t orig) { return (orig * 2654435761u) >> 18; }

	void register_port(uint32_t orig, void *port, const char *name, int effect_id)
	{
		if (!g_index_ready)
		{
			for (int i = 0; i < INDEX_SIZE; i++) g_index[i] = -1;
			g_index_ready = true;
		}
		if (g_nports >= PORTS_MAX) return;
		g_ports[g_nports] = { orig, port, name, effect_id };
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

	// effect ids served by one module's ports (Gilgamesh: 327 Zantetsuken, 328 Masamune,
	// 329 Excalibur, 330 Excalipoor share one module)
	static int canonical_effect(int effect_id)
	{
		if (effect_id >= 328 && effect_id <= 330) return 327;
		return effect_id;
	}

	bool module_ported(int effect_id)
	{
		effect_id = canonical_effect(effect_id);
		for (int i = 0; i < g_nports; i++)
			if (g_ports[i].effect_id == effect_id) return true;
		return false;
	}

	void register_all()
	{
		static bool done = false; // install() and a host may both ask
		if (done) return;
		done = true;
		register_mag069_griever();
		register_mag116_quezacotl();
		register_mag140_phoenix();
		register_mag185_shiva();
		register_mag187_odin();
		register_mag199_cactuar();
		register_mag278_carbuncle();
		register_mag291_pandemona();
		register_mag325_diablos();
		register_mag326_odin_reverse();
		register_mag338_moomba();
		register_mag095_siren();
		register_mag096_minimog();
		register_mag090_tonberry();
		register_mag097_boko();
		register_mag002_fire();
		register_mag142_fira();
		register_mag143_firaga();
		register_mag003_thunder();
		register_mag102_thundara();
		register_mag105_thundaga();
		register_mag144_blizzard();
		register_mag103_blizzara();
		register_mag104_blizzaga();
		register_mag022_bio();
		register_mag038_quake();
		register_mag149_ultima();
		register_mag175_holy();
		register_mag034_pain();
		register_mag148_meltdown();
		register_mag039_drain();
		register_mag014_death();
		register_mag036_confuse();
		register_mag124_flare();
		register_mag145_sleep();
		register_mag146_tornado();
		register_mag107_demi();
		register_mag109_dispel();
		register_mag111_aura();
		register_mag118_aero();
		register_mag123_slow();
		register_mag119_stop();
		register_mag125_haste();
		register_mag115_float();
		register_mag114_zombie();
		register_mag117_break();
		register_mag108_berserk();
		register_mag122_silence();
		register_mag121_blind();
		register_mag147_regen();
		register_mag024_esuna();
		register_mag032_protect();
		register_mag033_shell();
		register_fx_glint();
		register_mag035_life();
		register_mag027_fulllife();
		register_mag028_curaga();
		register_mag106_reflect();
		register_mag025_cura();
		register_mag001_cure();
		register_mag004_double();
		register_mag021_triple();
		register_mag040_scan();
		register_gfc_ifrit();
		register_gfc_leviathan();
		register_gfc_bahamut();
		register_gfc_cerberus();
		register_gfc_alexander();
		register_gfc_brothers();
		register_gfc_eden();
		register_mag191_doomtrain();
		register_mag327_gilgamesh();
		// 30 fps layer: see fx_held.cpp
		FX_HELD(register_all_held();)
	}
}
