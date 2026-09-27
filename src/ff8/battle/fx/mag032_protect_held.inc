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

// 30 fps part of mag032_protect.cpp: the module's held frame and the held registration (included at
// the end of mag032_protect.cpp)
//
// Held frames (30 fps): Protect draws three things, in the master's queue order - screen flashes
// (0x86BE70), particles (0x86C5C0, updated THEN drawn in the same tick) and the shield models (prim-
// model players 0x86C000 through the draw callback 0x86C160). The real tick records every draw in
// order; the held frame replays them in the same order into a private packet buffer:
//   * flash: its colour half way to the colour its script gives on the next tick (a colour ramp;
//     the last drawn colour holds when the script ends);
//   * particle: a copy of the node moved half way to the position the next motion step gives
//     (velocity += velocity / -5, position += velocity: deterministic); the flipbook frame holds;
//   * shield: prim::play_held (the exact in-between records of the real tick's play) with the
//     parameter block rebuilt from the current camera (the block is camera * facing * position).
// Every replayed draw starts from the GTE state its real draw started from, and its draw header gets
// the scratch bytes the real draw's header was allocated on (fields the draw does not set), so at
// fraction 0 a held frame inserts exactly the real tick's primitives. The stage wobble words
// (0x1D98992 + k * 0x2C, a 0x100-per-tick ramp read by the stage renderer, not drawn here) hold.
// The module globals (blend buffer), the Field_Alloc scratch, the shadow-camera scratch 0x21DFED0,
// the renderer temporaries, the GTE registers and the packet cursor are put back.

namespace ff8fx
{
namespace act
{
namespace protect
{
	static const uint32_t BSS_LO = 0x26C3890, BSS_HI = 0x26CDCC8;
	static const uint32_t GTE_D = magc::GTE_DATA, GTE_D_N = magc::GTE_DATA_SIZE, GTE_C = magc::GTE_CTRL, GTE_C_N = magc::GTE_CTRL_SIZE;
	static const int PARTICLE_BYTES = 0x88;
	static const int MAX_EVENTS = 512, MAX_PARTICLES = 256, MAX_SHIELDS = 16, MAX_FLASHES = 8, MAX_SLOTS = 1024;

	enum { EV_FLASH, EV_PARTICLE, EV_SHIELD };
	struct Event { int kind, index; };
	struct FlashRec { uint32_t colour, next; };
	struct ParticleRec { uint8_t node[PARTICLE_BYTES]; uint8_t gte[GTE_D_N + GTE_C_N]; };
	struct ShieldRec { uint32_t node; uint8_t gte[GTE_D_N + GTE_C_N]; };
	struct HeaderSlot { bool has; uint8_t bytes[0xB4]; };

	static Event g_ev[MAX_EVENTS];
	static FlashRec g_flash[MAX_FLASHES];
	static ParticleRec g_part[MAX_PARTICLES];
	static ShieldRec g_shield[MAX_SHIELDS];
	static HeaderSlot g_slot[MAX_SLOTS];
	static int g_nev = 0, g_nflash = 0, g_npart = 0, g_nshield = 0, g_nslot = 0;
	static uint32_t g_tick = 0xFFFFFFFF;
	static uint32_t g_pkt_start = 0; // module packet cursor at the tick's first draw
	static bool g_replay = false;     // a held frame replays the records
	static int g_slot_pos = 0;        // held frame: next header slot
	static HeaderSlot *g_cur = nullptr;
	static uint8_t g_held_packets[0x40000];

	static void held_begin_tick()
	{
		if (g_tick == g_real_tick) return;
		g_tick = g_real_tick;
		g_nev = g_nflash = g_npart = g_nshield = g_nslot = 0;
		g_pkt_start = MEM<uint32_t>(PACKET_CURSOR);
	}

	static void add_event(int kind, int index)
	{
		if (g_nev < MAX_EVENTS) g_ev[g_nev++] = { kind, index };
	}

	// the header slot of the draw that starts now: a new one on the real tick, the next recorded
	// one on a held frame
	static HeaderSlot *take_slot()
	{
		if (g_replay) return g_slot_pos < g_nslot ? &g_slot[g_slot_pos++] : nullptr;
		if (g_nslot >= MAX_SLOTS) return nullptr;
		HeaderSlot *s = &g_slot[g_nslot++];
		s->has = false;
		return s;
	}

	static void save_gte(uint8_t *dst)
	{
		memcpy(dst, (const void *)GTE_D, GTE_D_N);
		memcpy(dst + GTE_D_N, (const void *)GTE_C, GTE_C_N);
	}

	static void load_gte(const uint8_t *src)
	{
		memcpy((void *)GTE_D, src, GTE_D_N);
		memcpy((void *)GTE_C, src + GTE_D_N, GTE_C_N);
	}

	// the colour the flash's script gives on the next tick (the current one when it ends or waits)
	static uint32_t FlashNext(uint32_t node)
	{
		const uint32_t colour = U32(node, 0x34);
		const uint16_t idx = U16(node, 0x38);
		const uint32_t script = U32(node, 0x30);
		const uint32_t entry = U32(script, (int32_t)(int16_t)idx * 4);
		const uint16_t hi = (uint16_t)(entry >> 16);
		if ((hi >> 8) == 0xFF) return colour;
		if ((hi >> 8) == 0xFE)
		{
			const int16_t tick = (int16_t)(U16(node, 0x24) + 1);
			if (tick >= (int16_t)(hi & 0xFF)) return U32(script, (int32_t)(int16_t)(uint16_t)(idx + 1) * 4);
			return colour;
		}
		return entry;
	}

	static void held_note_flash(uint32_t node)
	{
		held_begin_tick();
		if (g_nflash >= MAX_FLASHES) return;
		g_flash[g_nflash] = { U32(node, 0x34), FlashNext(node) };
		add_event(EV_FLASH, g_nflash++);
	}

	static void held_note_particle(uint32_t node)
	{
		g_cur = nullptr;
		if (g_replay)
		{
			g_cur = take_slot();
			return;
		}
		held_begin_tick();
		if (g_npart >= MAX_PARTICLES) return;
		ParticleRec &r = g_part[g_npart];
		memcpy(r.node, (const void *)node, PARTICLE_BYTES);
		save_gte(r.gte);
		g_cur = take_slot();
		add_event(EV_PARTICLE, g_npart++);
	}

	static void held_note_shield(uint32_t node)
	{
		held_begin_tick();
		if (g_nshield >= MAX_SHIELDS) return;
		ShieldRec &r = g_shield[g_nshield];
		r.node = node;
		save_gte(r.gte);
		add_event(EV_SHIELD, g_nshield++);
	}

	static void held_note_callback()
	{
		if (!g_replay) held_begin_tick();
		g_cur = take_slot();
	}

	static void held_header(uint32_t hdr, int size)
	{
		if (!g_cur) return;
		if (g_replay)
		{
			if (g_cur->has) memcpy((void *)hdr, g_cur->bytes, size);
		}
		else
		{
			memcpy(g_cur->bytes, (const void *)hdr, size);
			g_cur->has = true;
		}
		g_cur = nullptr;
	}

	static bool HeldReady() { return held_ready(); }

	// colour half way (low three bytes; the top byte is the packet code, rewritten by the draw)
	static uint32_t LerpColour(uint32_t a, uint32_t b, int num, int den)
	{
		uint32_t c = a & 0xFF000000;
		for (int k = 0; k < 24; k += 8)
			c |= (uint32_t)(lerp_i((int32_t)((a >> k) & 0xFF), (int32_t)((b >> k) & 0xFF), num, den) & 0xFF) << k;
		return c;
	}

	static void HeldFrame(int num, int den)
	{
		if (!HeldReady() || g_tick != g_real_tick) return;
		const Mod *saved_mod = g_mod;
		g_mod = &MOD_032;
		static uint8_t bss[BSS_HI - BSS_LO], scratch[0x8000], gte[0x1CA9300 - 0x1CA8A10], shadow[0x20], ent_shadow[0x80];
		const uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(bss, (const void *)BSS_LO, sizeof(bss));
		memcpy(scratch, (const void *)fa, sizeof(scratch));
		memcpy(gte, (const void *)0x1CA8A10, sizeof(gte));
		memcpy(shadow, (const void *)0x21DFED0, sizeof(shadow));
		memcpy(ent_shadow, (const void *)0x1D999C8, sizeof(ent_shadow));
		const uint32_t parsepoly[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) }; // RenderGeometry temporaries
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		// the buffer gets the real tick's packets: the replayed draws write the same packets at the
		// same offsets over them (the halves a draw does not write are the real tick's)
		const uint32_t used = cursor - g_pkt_start;
		if (g_pkt_start && cursor > g_pkt_start && used < sizeof(g_held_packets)) memcpy(g_held_packets, (const void *)g_pkt_start, used);
		MEM<uint32_t>(PACKET_CURSOR) = P(g_held_packets);

		g_replay = true;
		g_slot_pos = 0;
		g_cur = nullptr;
		alignas(4) static uint8_t copy[PARTICLE_BYTES], next[PARTICLE_BYTES];
		for (int e = 0; e < g_nev; e++)
		{
			const Event &ev = g_ev[e];
			if (ev.kind == EV_FLASH)
			{
				const FlashRec &f = g_flash[ev.index];
				FlashDraw(LerpColour(f.colour, f.next, num, den));
			}
			else if (ev.kind == EV_PARTICLE)
			{
				const ParticleRec &r = g_part[ev.index];
				memcpy(copy, r.node, PARTICLE_BYTES);
				// in its motion state the next tick moves it (ParticleStep); flipbook frame holds
				if (U8(P(copy), 0x29) == 1)
				{
					memcpy(next, copy, PARTICLE_BYTES);
					ParticleStep(P(next));
					for (int k = 0; k < 3; k++)
						S16(P(copy), 0x1C + 2 * k) = lerp_angle(S16(P(copy), 0x1C + 2 * k), S16(P(next), 0x1C + 2 * k), num, den);
				}
				load_gte(r.gte);
				ParticleDraw(P(copy));
			}
			else
			{
				const ShieldRec &r = g_shield[ev.index];
				alignas(4) uint8_t fr[0x5C] = {};
				ShieldMatrix(r.node, P(fr));
				U32(P(fr), 0x48) = BLEND_BUFFER;
				load_gte(r.gte);
				prim::play_held((prim::Layout *)(r.node + 0x94), (prim::Callback)(void *)ShieldDraw, (int)P(fr), num, den);
			}
		}
		g_replay = false;
		g_cur = nullptr;

		MEM<uint32_t>(PACKET_CURSOR) = cursor;
		MEM<uint32_t>(0x1D99C18) = parsepoly[0];
		MEM<uint32_t>(0x1D99C1C) = parsepoly[1];
		memcpy((void *)0x1D999C8, ent_shadow, sizeof(ent_shadow));
		memcpy((void *)0x21DFED0, shadow, sizeof(shadow));
		memcpy((void *)0x1CA8A10, gte, sizeof(gte));
		memcpy((void *)fa, scratch, sizeof(scratch));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)BSS_LO, bss, sizeof(bss));
		g_mod = saved_mod;
	}
}
}
	static void register_mag032_held()
	{
		mark_held(act::protect::ORIG_Flash);
		mark_held(act::protect::ORIG_Particle);
		mark_held(act::protect::ORIG_ShieldPlayer);
		register_module_held(32, act::protect::HeldReady, act::protect::HeldFrame);
	}
}
