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

// Effect 096: MiniMog - Moogle Dance (actor state-machine GF family, see act_engine.h).
//
// Setup 0x731DE0 (runs once, not ported) creates the root queue 0x257B920 with the master
// m_731F70 and eight task pools. The director (m_7327B0) steps the phase script at
// [0x152BB90]; phase 2 spawns the creature (m_733E90: the moogle actor m_739610, pose via
// 0x8DD1C0/0x8DD220, drawn by a_746C10 then its texture animation a_739890 -> VRAM blit), the
// prim-model sparkles (m_733FC0) and the particle actors (a_7348A0 / a_7353B0). Screen/entity
// fades and the music volume (0x46BB40) are driven by the director's states.
// Module globals: 0x257B740..0x257F8A0 (+ camera copy 0x2793E58, 0x20 bytes).
// Held frames (30 fps): the creature actor and the prim-model sparkles (m_733FC0) are redrawn in
// between by act::held_frame; every other task stays on the generic packet path. Camera:
// act::held_camera.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl m_731F70(uint32_t a1); // 0x731F70 MAG_096_sub_731F70 TASK, 88 insns
	uint32_t __cdecl m_732250(uint32_t a1); // 0x732250 MAG_096_sub_732250, 21 insns
	uint32_t __cdecl m_7326F0(void); // 0x7326F0 MAG_096_sub_7326F0, 13 insns
	uint32_t __cdecl m_7327B0(uint32_t a1); // 0x7327B0 MAG_096_sub_7327B0 TASK, 56 insns
	uint32_t __cdecl m_7328B0(uint32_t a1); // 0x7328B0 sub_7328B0, 22 insns
	uint32_t __cdecl m_732950(uint32_t a1); // 0x732950 sub_732950, 39 insns
	uint32_t __cdecl m_732AC0(uint32_t a1); // 0x732AC0 sub_732AC0, 11 insns
	uint32_t __cdecl m_7332C0(uint32_t a1); // 0x7332C0 sub_7332C0, 15 insns
	uint32_t __cdecl m_733310(uint32_t a1); // 0x733310 sub_733310, 12 insns
	uint32_t __cdecl m_733340(uint32_t a1); // 0x733340 sub_733340, 12 insns
	uint32_t __cdecl m_733370(uint32_t a1); // 0x733370 sub_733370, 12 insns
	uint32_t __cdecl m_7333A0(uint32_t a1); // 0x7333A0 sub_7333A0, 12 insns
	uint32_t __cdecl m_7333D0(uint32_t a1); // 0x7333D0 sub_7333D0, 12 insns
	uint32_t __cdecl m_733400(uint32_t a1); // 0x733400 sub_733400, 12 insns
	uint32_t __cdecl m_733430(uint32_t a1); // 0x733430 sub_733430, 12 insns
	uint32_t __cdecl m_733460(uint32_t a1); // 0x733460 sub_733460, 12 insns
	uint32_t __cdecl m_733490(uint32_t a1); // 0x733490 sub_733490, 12 insns
	uint32_t __cdecl m_733510(uint32_t a1); // 0x733510 sub_733510, 8 insns
	uint32_t __cdecl m_733610(uint32_t a1); // 0x733610 sub_733610, 21 insns
	uint32_t __cdecl m_733700(uint32_t a1); // 0x733700 sub_733700, 11 insns
	uint32_t __cdecl m_733730(uint32_t a1); // 0x733730 sub_733730, 15 insns
	uint32_t __cdecl m_733780(uint32_t a1); // 0x733780 sub_733780, 16 insns
	uint32_t __cdecl m_7337D0(uint32_t a1); // 0x7337D0 sub_7337D0, 18 insns
	uint32_t __cdecl m_733870(uint32_t a1); // 0x733870 sub_733870, 6 insns
	uint32_t __cdecl m_733890(uint32_t a1); // 0x733890 sub_733890, 19 insns
	uint32_t __cdecl m_733910(uint32_t a1, uint32_t a2); // 0x733910 sub_733910, 23 insns
	uint32_t __cdecl m_733B20(uint32_t a1); // 0x733B20 sub_733B20, 11 insns
	uint32_t __cdecl m_733B90(uint32_t a1); // 0x733B90 sub_733B90, 24 insns
	uint32_t __cdecl m_733C40(uint32_t a1); // 0x733C40 sub_733C40, 7 insns
	uint32_t __cdecl m_733C90(uint32_t a1); // 0x733C90 sub_733C90, 18 insns
	uint32_t __cdecl m_733D40(uint32_t a1); // 0x733D40 sub_733D40, 7 insns
	uint32_t __cdecl m_733DA0(uint32_t a1); // 0x733DA0 sub_733DA0, 16 insns
	uint32_t __cdecl m_733E10(uint32_t a1); // 0x733E10 sub_733E10, 17 insns
	uint32_t __cdecl m_733E90(uint32_t a1); // 0x733E90 MAG_096_SpawnMogCreature, 40 insns
	uint32_t __cdecl m_733F70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6); // 0x733F70 sub_733F70, 17 insns
	uint32_t __cdecl m_733FC0(uint32_t a1); // 0x733FC0 sub_733FC0 TASK, 78 insns
	uint32_t __cdecl m_734400(uint32_t a1); // 0x734400 sub_734400, 27 insns
	uint32_t __cdecl m_734450(uint32_t a1); // 0x734450 sub_734450, 81 insns
	uint32_t __cdecl m_734580(uint32_t a1, uint32_t a2, uint32_t a3); // 0x734580 sub_734580, 207 insns
	uint32_t __cdecl m_734810(uint32_t a1); // 0x734810 sub_734810, 13 insns
	uint32_t __cdecl m_739610(uint32_t a1); // 0x739610 sub_739610 TASK, 43 insns
	uint32_t __cdecl m_739A50(uint32_t a1); // 0x739A50 sub_739A50, 30 insns
	uint32_t __cdecl m_739B00(uint32_t a1); // 0x739B00 sub_739B00, 23 insns
	uint32_t __cdecl m_739B70(uint32_t a1); // 0x739B70 sub_739B70, 20 insns
	uint32_t __cdecl m_739BB0(uint32_t a1); // 0x739BB0 sub_739BB0, 13 insns

	// ====================================================================================
	// part m1
	// ====================================================================================
	// MiniMog (096) module part m1: master task, director (state script) task, creature actor task,
	// prim-model particle task and their state handlers.
	//
	// Module state block: dword [0x152BB90] = 0x257F4E0 (MiniMog director data):
	//   +0x00/+0x04 effect origin (x,y / z,pad words), +0x40 current phase, +0x42 requested phase,
	//   +0x44 next requested phase, +0x46 ticks in phase, +0x48 "voice released" flag, +0x52 voice slot.
	namespace
	{
		inline uint32_t m1_add_task(uint32_t queue, uint32_t fn, uint32_t size, uint32_t parent)
		{
			return x::Effect_AddTaskAndInitFromCtx(queue, fn, size, parent);
		}
		// common task tail (after ++frame counter): end (release linked task, return 2) when
		// finished (status read before the increment) and no children are alive
		inline uint32_t m1_task_end(uint32_t node, uint8_t status)
		{
			if ((status & 1) != 0 && U8(node, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(node);
				return 2;
			}
			return 0;
		}
		// shared body of 0x733310..0x733490: step the script; when it ended (1) start the next one
		inline void m1_script_chain(uint32_t a1, uint32_t script)
		{
			if (a_73AE10() == 1)
			{
				a_73AB20(script, MEM<uint32_t>(0x152BB90), 0);
				U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			}
		}
		// &data[data[off] / 4] (cdq / and 3 / add / sar 2 = signed division by 4, then lea *4)
		inline uint32_t m1_section(uint32_t data, int32_t off)
		{
			const int32_t v = S32(data, off);
			return data + (uint32_t)shl32(v / 4, 2);
		}
	}

	// 0x731F70 (module 096 MAG_096_sub_731F70): MiniMog MASTER task - snapshots the camera matrix
	// into 0x2793E58, picks the double-buffered pools by tick parity, runs the 11-state master
	// state table, then executes the 8 module task queues (live task count -> node +0x5E).
	uint32_t __cdecl m_731F70(uint32_t a1)
	{
		set_mod_by_code(U32(a1, 8));      // node +8 = original task function address -> current module
		g_ported_tick = g_real_tick;

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x257F71C) = 0x2793E58;
		MEM<uint32_t>(0x257B850) = 0x2793E58;
		states[0] = 0x732100;
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = 0x732110;
		states[2] = 0x732120;
		states[3] = 0x7321A0;
		states[4] = 0x739CB0;
		states[5] = 0x739CF0;
		states[6] = 0x739D00;
		states[7] = 0x739D10;
		states[8] = 0x739D40;
		states[9] = 0x739D50;
		states[10] = 0x739D70;  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x257E3DC);
			const uint32_t v2 = MEM<uint32_t>(0x257DA64);
			MEM<uint32_t>(0x257B744) = v1;
			MEM<uint32_t>(0x257B84C) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x257E3D8);
			const uint32_t v2 = MEM<uint32_t>(0x257DA60);
			MEM<uint32_t>(0x257B744) = v1;
			MEM<uint32_t>(0x257B84C) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		MEM<uint16_t>(0x257F704) = 0;
		MEM<uint16_t>(0x257F688) = 0;
		static const uint32_t queues[8] = { 0x257E268, 0x257B828, 0x257F708, 0x257F678, 0x257E258, 0x257DA50, 0x257B930, 0x257CF28 };
		for (int i = 0; i < 8; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return m1_task_end(node, status);
	}

	// 0x732250 (module 096 MAG_096_sub_732250): master state - init director data (m_7326F0 +
	// a_7322A0), spawn task 0x732390 (0x30 B, queue 0x257F708) and the director task 0x7327B0
	// (0x48 B, queue 0x257B828), next state.
	uint32_t __cdecl m_732250(uint32_t a1)
	{
		m_7326F0();
		a_7322A0();
		m1_add_task(0x257F708, 0x732390, 0x30, a1);
		m1_add_task(0x257B828, 0x7327B0, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7326F0 (module 096 MAG_096_sub_7326F0): clears the 0x54-byte director data block
	// [0x152BB90], sets its origin words (0, 0xFF00, 0) and initialises block+8 (a_73A720).
	uint32_t __cdecl m_7326F0(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(0x152BB90), 0x54);
		const uint32_t d = MEM<uint32_t>(0x152BB90);
		U16(d, 0) = 0;
		U16(d, 2) = 0xFF00;
		U16(d, 4) = 0;
		a_73A720(d + 8);
		return 0; // void
	}

	// 0x7327B0 (module 096 MAG_096_sub_7327B0): MiniMog DIRECTOR task - phase bookkeeping
	// (m_7328B0), runs its 15-state script table, then emits a 3-word draw-mode packet
	// (0xE1000020) into OT bucket base+0x4484; ends when finished with no children.
	uint32_t __cdecl m_7327B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[15];
		states[0] = 0x732900;
		states[1] = 0x732950;
		states[2] = 0x7337D0;
		states[3] = 0x733A60;
		states[4] = 0x733AC0;
		states[5] = 0x733AE0;
		states[6] = 0x733B00;
		states[7] = 0x733B20;
		states[8] = 0x733B50;
		states[9] = 0x733B70;
		states[10] = 0x733B90;
		states[11] = 0x733DF0;
		states[12] = 0x733E10;
		states[13] = 0x733E50;
		states[14] = 0x733E70;
		m_7328B0(node);
		callp(states[S8(node, 0x29)], node);
		// draw: draw-mode (texpage) packet
		const uint32_t bucket = MEM<uint32_t>(0x1D8E04C) + 0x4484;
		const uint32_t pkt = MEM<uint32_t>(0x1D8E054);
		U32(pkt, 0) = 2;
		U32(pkt, 4) = 0xE1000020;
		U32(pkt, 8) = 0;
		x::SSIGPU_InsertPrimAutoDepth(bucket, pkt);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		MEM<uint32_t>(0x1D8E054) = pkt + 0xC;
		return m1_task_end(node, status);
	}

	// 0x7328B0 (module 096 sub_7328B0): director phase bookkeeping - ++ticks in phase; a new
	// requested phase (+0x42) becomes current (+0x40, timer reset, m_733E90 spawns its actors);
	// the queued phase (+0x44) becomes the requested one.
	uint32_t __cdecl m_7328B0(uint32_t a1)
	{
		uint32_t d = MEM<uint32_t>(0x152BB90);
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			m_733E90(a1);
			d = MEM<uint32_t>(0x152BB90);
		}
		const uint16_t next = U16(d, 0x44);
		if (U16(d, 0x42) != next)
		{
			U16(d, 0x42) = next;
			a_73A6D0();  // nullsub (pushed a1, unused)
		}
		return 0; // void
	}

	// 0x732950 (module 096 sub_732950): director state 1 - at script cue 1: play SE 0x152BB94,
	// spawn tasks 0x732A00/0x733570/0x733690 (0x70 B, queue 0x257B930), claim a voice slot for
	// sound 0x1692140 (-> director +0x52), wait 12 ticks (+0x44), next state.
	uint32_t __cdecl m_732950(uint32_t a1)
	{
		if (a_73B7E0(1) == 0)
			return 0;
		x::BdPlaySE(0x152BB94, 0, 0x80);
		const uint32_t node = a1;
		m1_add_task(0x257B930, 0x732A00, 0x70, node);
		m1_add_task(0x257B930, 0x733570, 0x70, node);
		m1_add_task(0x257B930, 0x733690, 0x70, node);
		const uint32_t slot = x::BdSound_ClaimVoiceSlot(0x1692140, 1, 0x80);
		const uint32_t d = MEM<uint32_t>(0x152BB90);
		U16(node, 0x44) = 0xC;
		U16(d, 0x52) = (uint16_t)slot;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x732AC0 (module 096 sub_732AC0): master state - a_73AB00, starts script 0x1532C68 on the
	// director data +8 (a_73AB20), next state.
	uint32_t __cdecl m_732AC0(uint32_t a1)
	{
		a_73AB00();
		const uint32_t d = MEM<uint32_t>(0x152BB90);
		a_73AB20(0x1532C68, d + 8, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7332C0 (module 096 sub_7332C0): script state - steps the running script (a_73AE10); at
	// cue 2 starts script 0x1532CB0 on the director data and advances.
	uint32_t __cdecl m_7332C0(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(2) != 0)
		{
			a_73AB20(0x1532CB0, MEM<uint32_t>(0x152BB90), 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x733310 (module 096 sub_733310): script state - when the script ends start 0x1532CF8, advance.
	uint32_t __cdecl m_733310(uint32_t a1) { m1_script_chain(a1, 0x1532CF8); return 0; } // void
	// 0x733340 (module 096 sub_733340): script state - when the script ends start 0x1532D40, advance.
	uint32_t __cdecl m_733340(uint32_t a1) { m1_script_chain(a1, 0x1532D40); return 0; } // void
	// 0x733370 (module 096 sub_733370): script state - when the script ends start 0x1532D88, advance.
	uint32_t __cdecl m_733370(uint32_t a1) { m1_script_chain(a1, 0x1532D88); return 0; } // void
	// 0x7333A0 (module 096 sub_7333A0): script state - when the script ends start 0x1532DD0, advance.
	uint32_t __cdecl m_7333A0(uint32_t a1) { m1_script_chain(a1, 0x1532DD0); return 0; } // void
	// 0x7333D0 (module 096 sub_7333D0): script state - when the script ends start 0x1532E18, advance.
	uint32_t __cdecl m_7333D0(uint32_t a1) { m1_script_chain(a1, 0x1532E18); return 0; } // void
	// 0x733400 (module 096 sub_733400): script state - when the script ends start 0x1532E60, advance.
	uint32_t __cdecl m_733400(uint32_t a1) { m1_script_chain(a1, 0x1532E60); return 0; } // void
	// 0x733430 (module 096 sub_733430): script state - when the script ends start 0x1532EA8, advance.
	uint32_t __cdecl m_733430(uint32_t a1) { m1_script_chain(a1, 0x1532EA8); return 0; } // void
	// 0x733460 (module 096 sub_733460): script state - when the script ends start 0x1532EF0, advance.
	uint32_t __cdecl m_733460(uint32_t a1) { m1_script_chain(a1, 0x1532EF0); return 0; } // void
	// 0x733490 (module 096 sub_733490): script state - when the script ends start 0x1532F38, advance.
	uint32_t __cdecl m_733490(uint32_t a1) { m1_script_chain(a1, 0x1532F38); return 0; } // void

	// 0x733510 (module 096 sub_733510): state - advance when script cue 4 is reached (a_73AAE0).
	uint32_t __cdecl m_733510(uint32_t a1)
	{
		if (a_73AAE0(4) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x733610 (module 096 sub_733610): fade-in state - level +0x1C += 0x100 up to 0x1000 (then
	// a_746A10, finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl m_733610(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x100);
		if (S16(a1, 0x1C) >= 0x1000)
		{
			U16(a1, 0x1C) = 0x1000;
			a_746A10();
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// 0x733700 (module 096 sub_733700): state - at script cue 2 set the music volume level
	// +0x1C = 0x7F, next state.
	uint32_t __cdecl m_733700(uint32_t a1)
	{
		if (a_73B7E0(2) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x1C) = 0x7F;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x733730 (module 096 sub_733730): music fade-out state - volume +0x1C -= 8 down to 0 (then
	// next state); sets the music volume every tick (Music_SetVolumeImmediate(0, vol)).
	uint32_t __cdecl m_733730(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) - 8);
		if (S16(a1, 0x1C) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x1C) = 0;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		x::Music_SetVolumeImmediate(0, (uint32_t)(int32_t)S16(a1, 0x1C));
		return 0; // void
	}

	// 0x733780 (module 096 sub_733780): music fade-in state - volume +0x1C += 8 up to 0x7F (then
	// finished, next state); sets the music volume every tick (Music_SetVolumeImmediate(0, vol)).
	uint32_t __cdecl m_733780(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 8);
		if (S16(a1, 0x1C) >= 0x7F)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0x7F;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		x::Music_SetVolumeImmediate(0, (uint32_t)(int32_t)S16(a1, 0x1C));
		return 0; // void
	}

	// 0x7337D0 (module 096 sub_7337D0): director state 2 - count down +0x44; at 0 spawn task
	// 0x733810 (0x70 B, queue 0x257B930), +0x44 = frame counter, next state.
	uint32_t __cdecl m_7337D0(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) <= 0)
		{
			m1_add_task(0x257B930, 0x733810, 0x70, a1);
			U16(a1, 0x44) = U16(a1, 0x24);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x733870 (module 096 sub_733870): state - level +0x1C = 0, next state.
	uint32_t __cdecl m_733870(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x733890 (module 096 sub_733890): entity fade-in state - level +0x1C += 0x100 up to 0x1000
	// (then a_73B9F0, finished, next state); applies the level to every visible battle entity with
	// mode 3 (m_733910).
	uint32_t __cdecl m_733890(uint32_t a1)
	{
		// eax on entry = movsx of the state byte (dispatcher); its high half ends up in the argument
		uint32_t eax_hi = (uint32_t)(int32_t)S8(a1, 0x29) & 0xFFFF0000u;
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x100);
		if (S16(a1, 0x1C) >= 0x1000)
		{
			U16(a1, 0x1C) = 0x1000;
			eax_hi = a_73B9F0() & 0xFFFF0000u;
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		// quirk 0x7338BC: `mov ax, [esi+0x1C]; push eax` - the argument's high half is left over in
		// eax (entry value / a_73B9F0 result); a_733950 only reads its low word
		m_733910(eax_hi | U16(a1, 0x1C), 3);
		return 0; // void
	}

	// 0x733910 (module 096 sub_733910): for each of the 4 battle entities 0x1D97494 + k*0x9C with
	// flag bit1 set: sets flag 0x800 and applies level a1 / mode a2 to it (a_733950).
	uint32_t __cdecl m_733910(uint32_t a1, uint32_t a2)
	{
		for (uint32_t ent = 0x1D97494; ent < 0x1D97704; ent += 0x9C)
		{
			uint16_t flags = U16(ent, 0);
			if ((flags & 2) != 0)
			{
				flags = (uint16_t)(flags | 0x800);
				U16(ent, 0) = flags;
				a_733950(ent, a1, a2);
			}
		}
		return 0; // void
	}

	// 0x733B20 (module 096 sub_733B20): director state 7 - at script cue 2 wait 0x90 ticks (+0x44),
	// next state.
	uint32_t __cdecl m_733B20(uint32_t a1)
	{
		if (a_73B7E0(2) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x44) = 0x90;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x733B90 (module 096 sub_733B90): director state 10 - at script cue 3 spawn tasks 0x733BE0
	// and 0x733CE0 (0x70 B, queue 0x257B930), wait 0x1E ticks, next state.
	uint32_t __cdecl m_733B90(uint32_t a1)
	{
		if (a_73B7E0(3) == 0)
			return 0;
		m1_add_task(0x257B930, 0x733BE0, 0x70, a1);
		m1_add_task(0x257B930, 0x733CE0, 0x70, a1);
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x44) = 0x1E;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x733C40 (module 096 sub_733C40): state - a_746AE0, level +0x1C = 0x1000, next state.
	uint32_t __cdecl m_733C40(uint32_t a1)
	{
		a_746AE0();
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x1000;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x733C90 (module 096 sub_733C90): fade-out state - level +0x1C -= 0x100 down to 0 (then
	// finished, next state); writes the level to the 4 words 0x1D98992 + k*0x2C.
	uint32_t __cdecl m_733C90(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xFF00);
		if (S16(a1, 0x1C) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// 0x733D40 (module 096 sub_733D40): state - a_73BA90, level +0x1C = 0x1000, next state.
	uint32_t __cdecl m_733D40(uint32_t a1)
	{
		a_73BA90();
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x1000;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x733DA0 (module 096 sub_733DA0): entity fade-out state - level +0x1C -= 0x100 down to 0
	// (then finished, next state); applies the level to the visible entities with mode 3 (m_733910).
	uint32_t __cdecl m_733DA0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xFF00);
		if (S16(a1, 0x1C) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		// quirk 0x733DC5: `mov ax, [eax+0x1C]; push eax` with eax = node pointer - the argument's
		// high half is the node address's high half (a_733950 only reads its low word)
		m_733910((a1 & 0xFFFF0000u) | U16(a1, 0x1C), 3);
		return 0; // void
	}

	// 0x733E10 (module 096 sub_733E10): director state 12 - at cue 4 (a_73B640) releases the voice
	// slot held in director +0x52 (sub_4A2940), director +0x48 = 1, next state.
	uint32_t __cdecl m_733E10(uint32_t a1)
	{
		if (a_73B640(4) == 0)
			return 0;
		x::sub_4A2940((uint32_t)(int32_t)S16(MEM<uint32_t>(0x152BB90), 0x52));
		const uint32_t d = MEM<uint32_t>(0x152BB90);
		const uint8_t st = U8(a1, 0x29);
		U16(d, 0x48) = 1;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x733E90 (module 096 MAG_096_SpawnMogCreature): on phase 2 spawns MiniMog: prim-model task
	// 0x734840 (0x169227C), particle task 0x733FC0 (model 0x169A1BC, 0x488), the creature actor
	// task 0x739610 (0x140 B, queue 0x257F678, model container 0x152BC04 anim 3) and a second
	// prim-model task 0x734840 (0x1696DE8, 0x6E).
	uint32_t __cdecl m_733E90(uint32_t a1)
	{
		const int32_t diff = (int32_t)S16(MEM<uint32_t>(0x152BB90), 0x40) - 2;
		if (diff != 0)
			return (uint32_t)diff;
		a_73C100(a1, 0x734840, 0x169227C, 0, 0x2D, 2);
		m_733F70(a1, 0x733FC0, 0x169A1BC, 0x488, 0, 0);
		const uint32_t creature = m1_add_task(0x257F678, 0x739610, 0x140, a1);
		x::Effect_BindModelContainerSetAnim(creature, 0x152BC04, 3);
		return a_73C100(a1, 0x734840, 0x1696DE8, 0x6E, 0x2D, 2);
	}

	// 0x733F70 (module 096 sub_733F70): spawns task a2 (0x51C B, queue 0x257DA50, parent a1) and
	// sets its model data +0x74 = a3, +0x78 = (s16)a4, +0x80 = a5, +0x82 = a6; returns the node.
	uint32_t __cdecl m_733F70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = m1_add_task(0x257DA50, a2, 0x51C, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		U32(t, 0x78) = (uint32_t)(int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x733FC0 (module 096 sub_733FC0): MiniMog prim-model PARTICLE task (node 0x51C) - 3-state
	// table (init / play+draw / nullsub), then steps the texture animation of 5 sections of its
	// model data (a_7340A0 on data + data[+0x3C/+0x40/+0x44/+0x54/+0x58], mode 2).
	uint32_t __cdecl m_733FC0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x734400;
		states[1] = 0x734810;
		states[2] = 0x734830;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		a_7340A0(m1_section(U32(node, 0x74), 0x3C), 2);
		a_7340A0(m1_section(U32(node, 0x74), 0x40), 2);
		a_7340A0(m1_section(U32(node, 0x74), 0x44), 2);
		a_7340A0(m1_section(U32(node, 0x74), 0x54), 2);
		a_7340A0(m1_section(U32(node, 0x74), 0x58), 2);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return m1_task_end(node, status);
	}

	// 0x734400 (module 096 sub_734400): particle task state 0 - decodes the prim layout of model
	// +0x74 (index +0x78) into +0x94, position = director origin (y word cleared), scale
	// 0x1000^3, plays the first frame (m_734450), next state.
	uint32_t __cdecl m_734400(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t d = MEM<uint32_t>(0x152BB90);
		const uint32_t xy = U32(d, 0);
		const uint32_t zp = U32(d, 4);
		U32(node, 0x1C) = xy;
		U32(node, 0x20) = zp;
		U16(node, 0x1E) = 0;
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		m_734450(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x734450 (module 096 sub_734450): particle play step - builds the effect matrix (rotations
	// +0x62/+0x60/+0x64, scale +0x50, position +0x1C) composed with the camera, fills the player
	// argument block and runs the prim-model player 0x701970 on +0x94 with draw callback 0x734580;
	// returns the frames left (0 = finished).
	uint32_t __cdecl m_734450(uint32_t a1)
	{
		// stack block (0x5C bytes) handed to the callback: +0x00 Mat4x3 effect matrix,
		// +0x38 = node+0x70, +0x44 = node+0x7C, +0x48 = [0x257F700] (vertex lerp scratch),
		// +0x4C..+0x56 words = node +0x8C/+0x8E/+0x90/+0x92/+0x86/+0x80. +0x20..+0x37, +0x3C..+0x43,
		// +0x58 are never written (UNINIT in the original, zeroed here; the callback 0x734580 does
		// not read them)
		alignas(4) uint8_t blk[0x5C] = {};
		const uint32_t L = P(blk);
		const uint32_t node = a1;
		x::MAG_022_sub_8DD770(L);
		int16_t ang = S16(node, 0x62);
		if (ang != 0)
			x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)ang);
		ang = S16(node, 0x60);
		if (ang != 0)
			x::sub_8DD7E0(L, (uint32_t)(int32_t)ang);
		ang = S16(node, 0x64);
		if (ang != 0)
			x::sub_8DD960(L, (uint32_t)(int32_t)ang);
		x::scale3DMatrix(L, node + 0x50);
		const int32_t px = S16(node, 0x1C);
		const int32_t py = S16(node, 0x1E);
		const int32_t pz = S16(node, 0x20);
		S32(L, 0x14) = px;
		S32(L, 0x18) = py;
		S32(L, 0x1C) = pz;
		x::ComposeAffineTransform(0x1D97778, L, L);
		U32(L, 0x48) = MEM<uint32_t>(0x257F700);
		U16(L, 0x54) = U16(node, 0x86);
		const uint32_t v7c = U32(node, 0x7C);
		U32(L, 0x38) = U32(node, 0x70);
		U16(L, 0x4C) = U16(node, 0x8C);
		const uint16_t w90 = U16(node, 0x90);
		const uint16_t w92 = U16(node, 0x92);
		U32(L, 0x44) = v7c;
		const uint16_t w8e = U16(node, 0x8E);
		U16(L, 0x50) = w90;
		const uint32_t paused = MEM<uint32_t>(0x257CF24);
		U16(L, 0x52) = w92;
		U16(L, 0x4E) = w8e;
		U16(L, 0x56) = U16(node, 0x80);
		return prim_play(node + 0x94, 0x734580, L, paused);
	}

	// 0x734580 (module 096 sub_734580): prim-model player draw callback (layout a1, record a2,
	// block a3 of m_734450) - picks the object's vertex frame (lerped by MAG_017_sub_701390 between
	// two frames), builds its matrix (rotation, parent-rotated position, scale), fills a 0x58-byte
	// Field_Alloc render header (flags 0x2000/0x2030 |0xC |0xC0, colour, alpha) and draws it with
	// Effect_RenderPrimModel into OT base+0x44 (mode 2).
	uint32_t __cdecl m_734580(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack (0x48 bytes): +0x00 SVECTOR position (+6 pad unwritten), +0x08 scale (3x3 s16
		// diagonal matrix or 3 x int32), +0x28 Mat4x3 object matrix (+0x3C translation)
		alignas(4) uint8_t loc[0x48] = {};
		const uint32_t L = P(loc);
		const uint32_t M = L + 0x28;
		const uint32_t rec = a2;
		if (((uint32_t)(int32_t)S16(rec, 0x1C) | U32(rec, 0x18)) == 0)
			return 0; // void (zero scale: nothing drawn)
		if (S16(rec, 0x24) >= 0x1000 && U32(rec, 0x20) == 0)
			return 0; // void (full alpha with black colour: nothing drawn)

		const uint32_t hdr = x::Field_Alloc(0x58);
		const int32_t model = S16(rec, 2);
		const uint32_t data = U32(a1, 0);
		const uint32_t blk = a3;
		const uint32_t base = data + U32(data, model * 4 + 8);
		const int16_t f1 = S16(rec, 0x2A);   // record b1: second vertex frame
		const int16_t f0 = S16(rec, 0x28);   // record b0: first vertex frame
		U32(hdr, 0) = base;
		// vertex frame f: base + 0xC + f * vertex_count * 8
		auto frame_ptr = [base](int16_t f) -> uint32_t {
			if (f == 0)
				return base + 0xC;
			const int32_t n = mul32(S32(base, 4), (int32_t)f);
			return base + (uint32_t)shl32(n, 3) + 0xC;
		};
		if (f0 == f1)
			U32(hdr, 4) = frame_ptr(f0);
		else
		{
			const int16_t t = S16(rec, 0x26);   // record b: blend factor 0..0x1000
			if (t == 0)
				U32(hdr, 4) = frame_ptr(f0);
			else if (t == 0x1000)
				U32(hdr, 4) = frame_ptr(f1);
			else
			{
				const uint32_t out = U32(blk, 0x48);
				x::MAG_017_sub_701390(base, (uint32_t)(int32_t)f0, (uint32_t)(int32_t)f1, (uint32_t)(int32_t)t, out);
				U32(hdr, 4) = U32(blk, 0x48);
			}
		}

		if ((U32(rec, 4) & 0x400) != 0)
			x::MAG_117_sub_701430(rec + 0x10, M);
		else
			x::MAG_017_sub_701310(rec + 0x10, M);
		const uint16_t vx = U16(rec, 8);
		const uint16_t vy = U16(rec, 0xA);
		const uint16_t vz = U16(rec, 0xC);
		U16(L, 0) = vx;
		U16(L, 2) = vy;
		U16(L, 4) = vz;
		x::GTE_SetRotMatrix(blk);
		x::GTE_LoadV0(L);
		x::GTE_MVMVA_RotV0();
		x::GTE_ReadMAC123(M + 0x14);
		x::GTE_MatrixMultiply(blk, M);
		{
			const int32_t bx = S32(blk, 0x14);
			const int32_t t0 = S32(M, 0x14);
			const int32_t t1 = S32(M, 0x18);
			const int32_t t2 = S32(M, 0x1C);
			S32(M, 0x14) = add32(t0, bx);
			const int32_t by = S32(blk, 0x18);
			S32(M, 0x18) = add32(t1, by);
			const int32_t bz = S32(blk, 0x1C);
			S32(M, 0x1C) = add32(t2, bz);
		}
		if (U32(rec, 0x18) != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			if ((U32(rec, 4) & 0x100) != 0)
			{
				// diagonal 3x3 s16 matrix at L+8
				U16(L, 0x08) = U16(rec, 0x18);
				U16(L, 0x10) = U16(rec, 0x1A);
				U16(L, 0x18) = U16(rec, 0x1C);
				U16(L, 0x12) = 0;
				U16(L, 0x0C) = 0;
				U16(L, 0x16) = 0;
				U16(L, 0x0A) = 0;
				U16(L, 0x14) = 0;
				U16(L, 0x0E) = 0;
				x::sub_56C220(M, L + 8);
			}
			else
			{
				S32(L, 0x08) = S16(rec, 0x18);
				S32(L, 0x0C) = S16(rec, 0x1A);
				S32(L, 0x10) = S16(rec, 0x1C);
				x::scale3DMatrix(M, L + 8);
			}
		}
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		const uint32_t flags = U32(rec, 4);
		U32(hdr, 0x1C) = (flags & 0x4000) != 0 ? 0x2000u : 0x2030u;
		if ((flags & 0x2000) != 0)
			U32(hdr, 0x1C) |= 0xC;
		const int32_t alpha = S16(rec, 0x24);
		U32(hdr, 0xC) = (uint32_t)alpha;
		if (alpha != 0)
		{
			const uint32_t mode = U32(hdr, 0x1C);
			U32(hdr, 8) = U32(rec, 0x20);   // colour
			U32(hdr, 0x1C) = mode | 0xC0;
		}
		const uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		MEM<uint32_t>(0x1D8E054) = x::Effect_RenderPrimModel(hdr, ot, 2, cursor);
		x::Field_Free(0x58);
		return 0; // void (prim player callback)
	}

	// 0x734810 (module 096 sub_734810): particle task state 1 - plays one frame (m_734450); when
	// the player reports 0 frames left: finished, next state.
	uint32_t __cdecl m_734810(uint32_t a1)
	{
		if (m_734450(a1) == 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x739610 (module 096 sub_739610): MiniMog CREATURE ACTOR task (node 0x140) - 6-state table
	// (animation control), then unless hidden (+0x26 bit2) draws the model (DrawModel 0x7396B0 =
	// a_746C10, OT list 0x257E3E8) and steps its texture animation (a_739890); ++frame counter.
	uint32_t __cdecl m_739610(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[6];
		states[0] = 0x739A50;
		states[1] = 0x739B00;
		states[2] = 0x739B40;
		states[3] = 0x739B70;
		states[4] = 0x739BB0;
		states[5] = 0x739BE0;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 4) == 0)
		{
			const uint32_t cursor = MEM<uint32_t>(0x1D8E054);
			MEM<uint32_t>(0x1D8E054) = a_746C10(node, 0x257E3E8, cursor);
			a_739890(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return m1_task_end(node, status);
	}

	// 0x739A50 (module 096 sub_739A50): creature state 0 - position = director origin, scale
	// 0x1000^3 at +0x114 (pointer +0x60), starts animation 0, texture-anim script 0x1532FC8 (1),
	// plays SE 0x152BB98, next state.
	uint32_t __cdecl m_739A50(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t d = MEM<uint32_t>(0x152BB90);
		const uint32_t xy = U32(d, 0);
		const uint32_t zp = U32(d, 4);
		U32(node, 0x4C) = xy;
		U32(node, 0x50) = zp;
		U32(node, 0x60) = node + 0x114;
		U32(node, 0x11C) = 0x1000;
		U32(node, 0x118) = 0x1000;
		U32(node, 0x114) = 0x1000;
		x::au_re_Battle_ReadAnimation_7(node, 0);
		a_739AC0(node, 0x1532FC8, 1);
		x::BdPlaySE(0x152BB98, 1, 0x80);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x739B00 (module 096 sub_739B00): creature state 1 - at frame 0x14 switches the texture-anim
	// script to 0x1532FD8; steps animation 0, at its end starts animation 1, next state.
	uint32_t __cdecl m_739B00(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U16(node, 0x24) == 0x14)
			a_739AC0(node, 0x1532FD8, 0);
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(node, 1);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x739B70 (module 096 sub_739B70): creature state 3 - steps the animation; at its end starts
	// animation 2 and texture-anim script 0x1532FE8, next state.
	uint32_t __cdecl m_739B70(uint32_t a1)
	{
		const uint32_t node = a1;
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(node, 2);
			a_739AC0(node, 0x1532FE8, 0);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x739BB0 (module 096 sub_739BB0): creature state 4 - steps the last animation (sub_8DD1C0);
	// from animation frame +0x136 >= 0x1F: finished + hidden (+0x26 |= 5), next state.
	uint32_t __cdecl m_739BB0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (S16(node, 0x136) >= 0x1F)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 5;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

namespace minimog
{
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort PORTS[] = {
		{ 0x731F70, (void *)m_731F70, "096 MAG_096_sub_731F70" },
		{ 0x732250, (void *)m_732250, "096 MAG_096_sub_732250" },
		{ 0x7326F0, (void *)m_7326F0, "096 MAG_096_sub_7326F0" },
		{ 0x7327B0, (void *)m_7327B0, "096 MAG_096_sub_7327B0" },
		{ 0x7328B0, (void *)m_7328B0, "096 sub_7328B0" },
		{ 0x732950, (void *)m_732950, "096 sub_732950" },
		{ 0x732AC0, (void *)m_732AC0, "096 sub_732AC0" },
		{ 0x7332C0, (void *)m_7332C0, "096 sub_7332C0" },
		{ 0x733310, (void *)m_733310, "096 sub_733310" },
		{ 0x733340, (void *)m_733340, "096 sub_733340" },
		{ 0x733370, (void *)m_733370, "096 sub_733370" },
		{ 0x7333A0, (void *)m_7333A0, "096 sub_7333A0" },
		{ 0x7333D0, (void *)m_7333D0, "096 sub_7333D0" },
		{ 0x733400, (void *)m_733400, "096 sub_733400" },
		{ 0x733430, (void *)m_733430, "096 sub_733430" },
		{ 0x733460, (void *)m_733460, "096 sub_733460" },
		{ 0x733490, (void *)m_733490, "096 sub_733490" },
		{ 0x733510, (void *)m_733510, "096 sub_733510" },
		{ 0x733610, (void *)m_733610, "096 sub_733610" },
		{ 0x733700, (void *)m_733700, "096 sub_733700" },
		{ 0x733730, (void *)m_733730, "096 sub_733730" },
		{ 0x733780, (void *)m_733780, "096 sub_733780" },
		{ 0x7337D0, (void *)m_7337D0, "096 sub_7337D0" },
		{ 0x733870, (void *)m_733870, "096 sub_733870" },
		{ 0x733890, (void *)m_733890, "096 sub_733890" },
		{ 0x733910, (void *)m_733910, "096 sub_733910" },
		{ 0x733B20, (void *)m_733B20, "096 sub_733B20" },
		{ 0x733B90, (void *)m_733B90, "096 sub_733B90" },
		{ 0x733C40, (void *)m_733C40, "096 sub_733C40" },
		{ 0x733C90, (void *)m_733C90, "096 sub_733C90" },
		{ 0x733D40, (void *)m_733D40, "096 sub_733D40" },
		{ 0x733DA0, (void *)m_733DA0, "096 sub_733DA0" },
		{ 0x733E10, (void *)m_733E10, "096 sub_733E10" },
		{ 0x733E90, (void *)m_733E90, "096 MAG_096_SpawnMogCreature" },
		{ 0x733F70, (void *)m_733F70, "096 sub_733F70" },
		{ 0x733FC0, (void *)m_733FC0, "096 sub_733FC0" },
		{ 0x734400, (void *)m_734400, "096 sub_734400" },
		{ 0x734450, (void *)m_734450, "096 sub_734450" },
		{ 0x734580, (void *)m_734580, "096 sub_734580" },
		{ 0x734810, (void *)m_734810, "096 sub_734810" },
		{ 0x739610, (void *)m_739610, "096 sub_739610" },
		{ 0x739A50, (void *)m_739A50, "096 sub_739A50" },
		{ 0x739B00, (void *)m_739B00, "096 sub_739B00" },
		{ 0x739B70, (void *)m_739B70, "096 sub_739B70" },
		{ 0x739BB0, (void *)m_739BB0, "096 sub_739BB0" },
		{ 0, nullptr, nullptr }
	};
	// tasks whose drawing the held frame redraws: the creature actor and the prim-model emitters
	static const uint32_t HELD_TASKS[] = { 0x739610, 0x733FC0, 0 };
	static bool is_held(uint32_t a) { for (const uint32_t *h = HELD_TASKS; *h; h++) if (*h == a) return true; return false; }
	// module globals 0x257B740..0x257F8A0 (task pools, arenas, script/camera state, key tables)
	static const HeldDesc HELD = { &MOD_096, 0x257F678, 0x739610, 0x257E3E8, 0x257B740, 0x257F8A0 };
	static bool HeldReady() { return held_ready(); }
	static void HeldFrame(int num, int den) { held_frame(HELD, num, den); }
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		const Mod *m = g_mod;
		g_mod = &MOD_096;
		bool r = held_camera(num, den, world, lookat);
		g_mod = m;
		return r;
	}
}
}
	void register_mag096_minimog()
	{
		act::register_module(96, act::minimog::HELD_TASKS);
		for (const act::minimog::ModPort *p = act::minimog::PORTS; p->addr; p++)
			act::register_module_port(96, p->addr, p->port, p->name, act::minimog::is_held(p->addr));
		register_module_held(96, act::minimog::HeldReady, act::minimog::HeldFrame);
		register_module_camera(96, act::minimog::HeldCamera);
	}
}
