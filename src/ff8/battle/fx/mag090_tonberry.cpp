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

// Effect 090: Tonberry - Chef's Knife (actor state-machine GF family, see act_engine.h).
//
// Setup 0x762360 (runs once, not ported) creates the root queue with the master t_7624D0 and the
// task pools. The master copies the camera matrix to 0x2793E58, picks the double-buffered packet
// pools by tick parity ([0x259EEA8] = the module packet cursor), runs its state table, then six
// queues. The director (t_762C60) steps the phase script at [0x1547168]; each new phase spawns its
// tasks (t_763D60): the prim-model particle task t_763EB0 (draw callback t_764260 -> t_764540 and
// the eight primitive-list renderers t_764670..t_766090), the creature actor t_7674E0 (walks by
// velocity/16 per tick, drawn by a_746C10 into the module pool, bone 0x18 published to 0x25A4C10),
// its ring particles t_767290 and sparks t_767C70 (drawn by t_7672F0 through a_7435E0), the
// prim-model actors t_7667B0 / t_766FB0 (t_766830), entity fades (0x1D98992 table) and the damage
// state t_768370 (0x506690). Sound: voice slot 0x4A29A0 / 0x4A2940, SE 0x501330, stream 0x501460,
// summon stream 0x5018C0. Camera: the engine's keyed moves (a_73AB20 / a_73AE10) and tracks.
// Module globals: 0x259EEA8..0x25A4E00 (+ camera copy 0x2793E58, 0x20 bytes; module data
// 0x15474EC..0x1547A1C).
// Held frames (30 fps): the creature actor (position advanced by velocity/16 * num/den, midpoint
// pose) and the two prim-model tasks 0x763EB0 / 0x767B40 (all their packets come from the prim-model
// callback, redrawn by prim::play_held) are redrawn in between by act::held_frame; every other task
// stays on the generic packet path. Camera: act::held_camera.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
	// engine functions not in act::x (file-local wrappers)
	namespace xm
	{
		static inline uint32_t BdPlayStream(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x501460)(a1, a2, a3, a4); }
		static inline uint32_t sub_8DE160(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DE160)(a1, a2); }
		static inline uint32_t sub_8DE1F0(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DE1F0)(a1, a2); }
	}
	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl t_7624D0(uint32_t a1); // 0x7624D0 MAG_090_sub_7624D0 TASK, 80 insns, 090:7624D0
	uint32_t __cdecl t_762650(uint32_t a1); // 0x762650 MAG_090_sub_762650, 6 insns, 090:762650
	uint32_t __cdecl t_762710(uint32_t a1); // 0x762710 MAG_090_sub_762710, 21 insns, 090:762710
	uint32_t __cdecl t_762BB0(uint32_t a1); // 0x762BB0 MAG_090_sub_762BB0, 50 insns, 090:762BB0
	uint32_t __cdecl t_762C60(uint32_t a1); // 0x762C60 MAG_090_sub_762C60 TASK, 39 insns, 090:762C60
	uint32_t __cdecl t_762D10(uint32_t a1); // 0x762D10 sub_762D10, 22 insns, 090:762D10
	uint32_t __cdecl t_762D60(uint32_t a1); // 0x762D60 sub_762D60, 19 insns, 090:762D60
	uint32_t __cdecl t_762E60(uint32_t a1); // 0x762E60 sub_762E60, 4 insns, 090:762E60
	uint32_t __cdecl t_762E90(uint32_t a1); // 0x762E90 sub_762E90, 14 insns, 090:762E90
	uint32_t __cdecl t_763690(uint32_t a1); // 0x763690 sub_763690, 12 insns, 090:763690
	uint32_t __cdecl t_763790(uint32_t a1); // 0x763790 sub_763790, 9 insns, 090:763790
	uint32_t __cdecl t_763970(uint32_t a1); // 0x763970 sub_763970, 19 insns, 090:763970
	uint32_t __cdecl t_7639E0(uint32_t a1); // 0x7639E0 sub_7639E0, 9 insns, 090:7639E0
	uint32_t __cdecl t_763A30(uint32_t a1); // 0x763A30 sub_763A30, 22 insns, 090:763A30
	uint32_t __cdecl t_763AA0(uint32_t a1); // 0x763AA0 sub_763AA0, 9 insns, 090:763AA0
	uint32_t __cdecl t_763AC0(uint32_t a1); // 0x763AC0 sub_763AC0, 22 insns, 090:763AC0
	uint32_t __cdecl t_763B10(uint32_t a1); // 0x763B10 sub_763B10, 9 insns, 090:763B10
	uint32_t __cdecl t_763B90(uint32_t a1); // 0x763B90 sub_763B90, 8 insns, 090:763B90
	uint32_t __cdecl t_763BD0(uint32_t a1); // 0x763BD0 sub_763BD0, 8 insns, 090:763BD0
	uint32_t __cdecl t_763C90(uint32_t a1); // 0x763C90 sub_763C90, 11 insns, 090:763C90
	uint32_t __cdecl t_763CC0(uint32_t a1); // 0x763CC0 sub_763CC0, 10 insns, 090:763CC0
	uint32_t __cdecl t_763CF0(uint32_t a1); // 0x763CF0 sub_763CF0, 13 insns, 090:763CF0
	uint32_t __cdecl t_763D60(uint32_t a1); // 0x763D60 MAG_090_SpawnTonberryCreature, 68 insns, 090:763D60
	uint32_t __cdecl t_763E60(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6); // 0x763E60 sub_763E60, 17 insns, 090:763E60
	uint32_t __cdecl t_763EB0(uint32_t a1); // 0x763EB0 sub_763EB0 TASK, 51 insns, 090:763EB0
	uint32_t __cdecl t_763F80(uint32_t a1); // 0x763F80 sub_763F80, 44 insns, 090:763F80
	uint32_t __cdecl t_764020(uint32_t a1, uint32_t a2, uint32_t a3); // 0x764020 sub_764020, 23 insns, 090:764020
	uint32_t __cdecl t_764070(uint32_t a1); // 0x764070 sub_764070, 31 insns, 090:764070
	uint32_t __cdecl t_764100(uint32_t a1); // 0x764100 sub_764100, 15 insns, 090:764100
	uint32_t __cdecl t_764260(uint32_t a1, uint32_t a2, uint32_t a3); // 0x764260 sub_764260, 228 insns, 090:764260
	uint32_t __cdecl t_764540(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x764540 sub_764540, 132 insns, 090:764540
	uint32_t __cdecl t_764670(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x764670 sub_764670, 178 insns, 090:764670
	uint32_t __cdecl t_764890(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x764890 sub_764890, 213 insns, 090:764890
	uint32_t __cdecl t_764B20(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x764B20 sub_764B20, 410 insns, 090:764B20
	uint32_t __cdecl t_765010(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x765010 sub_765010, 485 insns, 090:765010
	uint32_t __cdecl t_765B30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x765B30 sub_765B30, 446 insns, 090:765B30
	uint32_t __cdecl t_766090(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x766090 sub_766090, 553 insns, 090:766090
	uint32_t __cdecl t_766760(uint32_t a1); // 0x766760 sub_766760, 25 insns, 090:766760
	uint32_t __cdecl t_7667B0(uint32_t a1); // 0x7667B0 sub_7667B0 TASK, 33 insns, 090:7667B0
	uint32_t __cdecl t_766830(uint32_t a1, uint32_t a2); // 0x766830 sub_766830, 58 insns, 090:766830
	uint32_t __cdecl t_766EB0(uint32_t a1); // 0x766EB0 sub_766EB0, 14 insns, 090:766EB0
	uint32_t __cdecl t_766F00(uint32_t a1); // 0x766F00 sub_766F00, 9 insns, 090:766F00
	uint32_t __cdecl t_766F20(uint32_t a1); // 0x766F20 sub_766F20, 14 insns, 090:766F20
	uint32_t __cdecl t_766FB0(uint32_t a1); // 0x766FB0 sub_766FB0 TASK, 32 insns, 090:766FB0
	uint32_t __cdecl t_767020(uint32_t a1); // 0x767020 sub_767020, 14 insns, 090:767020
	uint32_t __cdecl t_767070(uint32_t a1); // 0x767070 sub_767070, 9 insns, 090:767070
	uint32_t __cdecl t_767090(uint32_t a1); // 0x767090 sub_767090, 19 insns, 090:767090
	uint32_t __cdecl t_767150(uint32_t a1); // 0x767150 sub_767150, 15 insns, 090:767150
	uint32_t __cdecl t_767190(uint32_t a1); // 0x767190 sub_767190, 20 insns, 090:767190
	uint32_t __cdecl t_7671D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x7671D0 sub_7671D0, 63 insns, 090:7671D0
	uint32_t __cdecl t_767290(uint32_t a1); // 0x767290 sub_767290 TASK, 30 insns, 090:767290
	uint32_t __cdecl t_7672F0(uint32_t a1); // 0x7672F0 sub_7672F0, 78 insns, 090:7672F0
	uint32_t __cdecl t_767410(uint32_t a1); // 0x767410 sub_767410, 8 insns, 090:767410
	uint32_t __cdecl t_767430(uint32_t a1); // 0x767430 sub_767430, 13 insns, 090:767430
	uint32_t __cdecl t_767460(uint32_t a1); // 0x767460 sub_767460, 9 insns, 090:767460
	uint32_t __cdecl t_767480(uint32_t a1); // 0x767480 sub_767480, 24 insns, 090:767480
	uint32_t __cdecl t_7674E0(uint32_t a1); // 0x7674E0 GF_090Tonberry_CreatureActorTask TASK, 76 insns, 090:7674E0
	uint32_t __cdecl t_7677F0(uint32_t a1); // 0x7677F0 sub_7677F0, 34 insns, 090:7677F0
	uint32_t __cdecl t_767870(uint32_t a1); // 0x767870 sub_767870, 15 insns, 090:767870
	uint32_t __cdecl t_7678B0(uint32_t a1, uint32_t a2); // 0x7678B0 sub_7678B0, 10 insns, 090:7678B0
	uint32_t __cdecl t_7679D0(uint32_t a1); // 0x7679D0 sub_7679D0, 26 insns, 090:7679D0
	uint32_t __cdecl t_767A40(uint32_t a1); // 0x767A40 sub_767A40, 74 insns, 090:767A40
	uint32_t __cdecl t_767BA0(uint32_t a1); // 0x767BA0 sub_767BA0, 28 insns, 090:767BA0
	uint32_t __cdecl t_767C00(uint32_t a1); // 0x767C00 sub_767C00, 25 insns, 090:767C00
	uint32_t __cdecl t_767C70(uint32_t a1); // 0x767C70 sub_767C70 TASK, 30 insns, 090:767C70
	uint32_t __cdecl t_767CD0(uint32_t a1); // 0x767CD0 sub_767CD0, 29 insns, 090:767CD0
	uint32_t __cdecl t_767D40(uint32_t a1); // 0x767D40 sub_767D40, 60 insns, 090:767D40
	uint32_t __cdecl t_767DF0(uint32_t a1); // 0x767DF0 sub_767DF0, 15 insns, 090:767DF0
	uint32_t __cdecl t_767E20(uint32_t a1); // 0x767E20 sub_767E20, 16 insns, 090:767E20
	uint32_t __cdecl t_767E60(uint32_t a1); // 0x767E60 sub_767E60, 30 insns, 090:767E60
	uint32_t __cdecl t_767F20(uint32_t a1); // 0x767F20 sub_767F20, 53 insns, 090:767F20
	uint32_t __cdecl t_767FE0(uint32_t a1); // 0x767FE0 sub_767FE0, 20 insns, 090:767FE0
	uint32_t __cdecl t_768030(uint32_t a1); // 0x768030 sub_768030, 20 insns, 090:768030
	uint32_t __cdecl t_768080(uint32_t a1); // 0x768080 sub_768080, 11 insns, 090:768080
	uint32_t __cdecl t_7680B0(uint32_t a1); // 0x7680B0 sub_7680B0, 6 insns, 090:7680B0
	uint32_t __cdecl t_7680D0(uint32_t a1); // 0x7680D0 sub_7680D0, 22 insns, 090:7680D0
	uint32_t __cdecl t_768130(uint32_t a1); // 0x768130 sub_768130, 38 insns, 090:768130
	uint32_t __cdecl t_768210(uint32_t a1); // 0x768210 sub_768210, 18 insns, 090:768210
	uint32_t __cdecl t_768250(uint32_t a1); // 0x768250 sub_768250, 18 insns, 090:768250
	uint32_t __cdecl t_768300(uint32_t a1); // 0x768300 sub_768300, 8 insns, 090:768300
	uint32_t __cdecl t_768320(uint32_t a1); // 0x768320 sub_768320, 18 insns, 090:768320
	uint32_t __cdecl t_768370(uint32_t a1); // 0x768370 MAG_090_sub_768370, 21 insns, 090:768370

	// ====================================================================================
	// part t1
	// ====================================================================================
	// Tonberry (090) module part t1: master task, director task and its state handlers, the phase
	// spawner, the prim-model particle task (0x763EB0) and its draw callback chain
	// (0x764260 -> 0x764540 -> 0x764670 flat-triangle list).
	//
	// Module data: dword [0x1547168] = Tonberry director data (0x54 bytes):
	//   +0x00/+0x04 effect origin (x,y / z,pad words), +0x08..+0x0F spawn position of the caster,
	//   +0x40 current phase, +0x42 requested phase, +0x44 next requested phase, +0x46 ticks in phase,
	//   +0x48 end flag, +0x4A caster->origin angle, +0x4E caster entity word +0x26,
	//   +0x52 claimed voice slot.
	// [0x25A4C04] = per-model depth offset table (s16 per model index) of the prim-model particle draw.
	namespace
	{
		inline uint32_t t1_data() { return MEM<uint32_t>(0x1547168); }
		inline void t1_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
		// common task tail (after ++frame counter): end (release linked task, return 2) when
		// finished (status read before the increment) and no children are alive
		inline uint32_t t1_task_end(uint32_t node, uint8_t status)
		{
			if ((status & 1) != 0 && U8(node, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(node);
				return 2;
			}
			return 0;
		}
		// screen-space clip tests of the flat-triangle list (x 0..0xA00, y 0..0x6C0)
		inline bool t1_out_x(int16_t v) { return v < 0 || v > 0xA00; }
		inline bool t1_out_y(int16_t v) { return v < 0 || v > 0x6C0; }
	}

	// 0x7624D0 (module 090 MAG_090_sub_7624D0): Tonberry MASTER task - snapshots the camera matrix
	// into 0x2793E58, picks the double-buffered pools by tick parity, runs the 11-state master
	// state table, then executes the 6 module task queues (live task count -> node +0x5E).
	uint32_t __cdecl t_7624D0(uint32_t a1)
	{
		set_mod_by_code(U32(a1, 8));      // node +8 = original task function address -> current module
		g_ported_tick = g_real_tick;

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x25A4C08) = 0x2793E58;
		MEM<uint32_t>(0x259EFB8) = 0x2793E58;
		states[0] = 0x762630;
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = 0x762640;
		states[2] = 0x762650;
		states[3] = 0x762660;
		states[4] = 0x768400;
		states[5] = 0x768440;
		states[6] = 0x768450;
		states[7] = 0x768460;
		states[8] = 0x768490;
		states[9] = 0x7684A0;
		states[10] = 0x7684C0;  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x25A2CF4);
			const uint32_t v2 = MEM<uint32_t>(0x25A2B74);
			MEM<uint32_t>(0x259EEA8) = v1;
			MEM<uint32_t>(0x259EFB4) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x25A2CF0);
			const uint32_t v2 = MEM<uint32_t>(0x25A2B70);
			MEM<uint32_t>(0x259EEA8) = v1;
			MEM<uint32_t>(0x259EFB4) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		static const uint32_t queues[6] = { 0x25A2B80, 0x259EF90, 0x25A4BF0, 0x25A3F90, 0x259F098, 0x25A2B60 };
		for (int i = 0; i < 6; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return t1_task_end(node, status);
	}

	// 0x762650 (module 090 MAG_090_sub_762650): master state - waits until no child task is alive
	// (+0x28 == 0), then next state.
	uint32_t __cdecl t_762650(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x762710 (module 090 MAG_090_sub_762710): master state - init director data (t_762BB0 +
	// a_7322A0), spawns task 0x762850 (0x30 B, queue 0x25A4BF0) and the director task 0x762C60
	// (0x48 B, queue 0x259EF90), next state.
	uint32_t __cdecl t_762710(uint32_t a1)
	{
		t_762BB0(a1);
		a_7322A0();
		x::Effect_AddTaskAndInitFromCtx(0x25A4BF0, 0x762850, 0x30, a1);
		x::Effect_AddTaskAndInitFromCtx(0x259EF90, 0x762C60, 0x48, a1);
		t1_next_state(a1);
		return 0; // void
	}

	// 0x762BB0 (module 090 MAG_090_sub_762BB0): clears the 0x54-byte director data [0x1547168],
	// gets the caster's spawn position (+0x08), origin = spawn position with y cleared and z +0xC00,
	// +0x4A = angle caster->origin - 0x800, +0x4E = caster entity word +0x26.
	uint32_t __cdecl t_762BB0(uint32_t a1)
	{
		x::MAG_007_sub_8DCC00(t1_data(), 0x54);
		{
			const uint32_t spawn = t1_data() + 8;
			const uint32_t slot = U8(a1, 0x2D);
			x::GetEffectSpawnPosition(0x1D972C0 + slot * 0x9C, 0xF0, 0, spawn);
		}
		const uint32_t d = t1_data();
		U16(d, 0xA) = 0;
		U32(d, 0) = U32(d, 8);
		U32(d, 4) = U32(d, 0xC);
		U16(d, 4) = (uint16_t)(U16(d, 4) + 0xC00);
		const uint16_t dx = (uint16_t)(U16(d, 8) - U16(d, 0));
		const uint16_t dz = (uint16_t)(U16(d, 0xC) - U16(d, 4));
		const uint32_t ang = x::CartesianToGameAngle((uint32_t)(int32_t)(int16_t)dx, (uint32_t)(int32_t)(int16_t)dz);
		const uint32_t d2 = t1_data();
		U16(d2, 0x4A) = (uint16_t)((ang - 0x800) & 0xFFF);
		const uint32_t slot = U8(a1, 0x2D);
		const uint16_t w = MEM<uint16_t>(0x1D972E6 + slot * 0x9C);
		U16(d2, 0x4E) = w;
		return 0; // void
	}

	// 0x762C60 (module 090 MAG_090_sub_762C60): Tonberry DIRECTOR task - phase bookkeeping
	// (t_762D10), then runs the 12-state director state table.
	uint32_t __cdecl t_762C60(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[12];
		states[0] = 0x762D60;
		states[1] = 0x763B70;
		states[2] = 0x763B90;
		states[3] = 0x763BD0;
		states[4] = 0x763BF0;
		states[5] = 0x763C50;
		states[6] = 0x763C70;
		states[7] = 0x763C90;
		states[8] = 0x763CC0;
		states[9] = 0x763CF0;
		states[10] = 0x763D20;
		states[11] = 0x763D40;  // nullsub
		t_762D10(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return t1_task_end(node, status);
	}

	// 0x762D10 (module 090 sub_762D10): director phase bookkeeping - ++ticks in phase; a new
	// requested phase becomes current (ticks = 0, spawns its tasks via t_763D60); the next
	// requested phase becomes requested (a_73A6D0).
	uint32_t __cdecl t_762D10(uint32_t a1)
	{
		uint32_t d = t1_data();
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			t_763D60(a1);
			d = t1_data();
		}
		const uint16_t nxt = U16(d, 0x44);
		if (U16(d, 0x42) != nxt)
		{
			U16(d, 0x42) = nxt;
			a_73A6D0();
		}
		return 0; // void
	}

	// 0x762D60 (module 090 sub_762D60): director state 0 - claims a voice slot (0x16D7CD0) into
	// data +0x52, spawns task 0x762DA0 (0x7C B, queue 0x259F098), next state.
	uint32_t __cdecl t_762D60(uint32_t a1)
	{
		const uint32_t slot = x::BdSound_ClaimVoiceSlot(0x16D7CD0, 1, 0x80);
		const uint32_t d = t1_data();
		U16(d, 0x52) = (uint16_t)slot;
		x::Effect_AddTaskAndInitFromCtx(0x259F098, 0x762DA0, 0x7C, a1);
		t1_next_state(a1);
		return 0; // void
	}

	// 0x762E60 (module 090 sub_762E60): master state - a_73AB00, next state.
	uint32_t __cdecl t_762E60(uint32_t a1)
	{
		a_73AB00();
		t1_next_state(a1);
		return 0; // void
	}

	// 0x762E90 (module 090 sub_762E90): script state - at script cue 2 starts script 0x154E9E8
	// (arg = director data), next state.
	uint32_t __cdecl t_762E90(uint32_t a1)
	{
		if (a_73AAE0(2) != 0)
		{
			a_73AB20(0x154E9E8, t1_data(), 0);
			t1_next_state(a1);
		}
		return 0; // void
	}

	// 0x763690 (module 090 sub_763690): script state - when the script ends starts camera script
	// 0x154E9D8 (a_73B3B0), +0x74 = 0x63 ticks, next state.
	uint32_t __cdecl t_763690(uint32_t a1)
	{
		if (a_73AE10() == 1)
		{
			a_73B3B0(0x154E9D8);
			U16(a1, 0x74) = 0x63;
			t1_next_state(a1);
		}
		return 0; // void
	}

	// 0x763790 (module 090 sub_763790): script state - steps the camera script (a_73B4B0) for
	// +0x74 ticks, then next state.
	uint32_t __cdecl t_763790(uint32_t a1)
	{
		a_73B4B0();  // (the original pushes 0x154E9D8, unused by the 0-arg callee)
		U16(a1, 0x74) = (uint16_t)(U16(a1, 0x74) - 1);
		if (S16(a1, 0x74) <= 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x763970 (module 090 sub_763970): script state - steps the script; at cue 3 plays SE
	// 0x1547170 and starts script 0x154EAC0 (arg = director data), next state.
	uint32_t __cdecl t_763970(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(3) != 0)
		{
			x::BdPlaySE(0x1547170, 0, 0x80);
			a_73AB20(0x154EAC0, t1_data(), 0);
			t1_next_state(a1);
		}
		return 0; // void
	}

	// 0x7639E0 (module 090 sub_7639E0): script state - steps the script; next state at cue 4
	// (a_73B640).
	uint32_t __cdecl t_7639E0(uint32_t a1)
	{
		a_73AE10();
		if (a_73B640(4) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x763A30 (module 090 sub_763A30): script state - steps the script; at cue 6 starts script
	// 0x154EB50 (caster word +0x4E < 0x1000) or 0x154EB98 (arg = data +8), next state.
	uint32_t __cdecl t_763A30(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(6) != 0)
		{
			const uint32_t d = t1_data();
			if (S16(d, 0x4E) < 0x1000)
				a_73AB20(0x154EB50, d + 8, 0);
			else
				a_73AB20(0x154EB98, d + 8, 0);
			t1_next_state(a1);
		}
		return 0; // void
	}

	// 0x763AA0 (module 090 sub_763AA0): script state - steps the script; next state at cue 7
	// (a_73B640).
	uint32_t __cdecl t_763AA0(uint32_t a1)
	{
		a_73AE10();
		if (a_73B640(7) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x763AC0 (module 090 sub_763AC0): script state - steps the script; at cue 7 starts script
	// 0x154EBE0 (caster word +0x4E < 0x1000) or 0x154EC28 (arg = data +8), next state.
	uint32_t __cdecl t_763AC0(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(7) != 0)
		{
			const uint32_t d = t1_data();
			if (S16(d, 0x4E) < 0x1000)
				a_73AB20(0x154EBE0, d + 8, 0);
			else
				a_73AB20(0x154EC28, d + 8, 0);
			t1_next_state(a1);
		}
		return 0; // void
	}

	// 0x763B10 (module 090 sub_763B10): script state - steps the script; next state at cue 9
	// (a_73AAE0).
	uint32_t __cdecl t_763B10(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(9) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x763B90 (module 090 sub_763B90): director state - next state at director cue 1 (a_73B7E0).
	uint32_t __cdecl t_763B90(uint32_t a1)
	{
		if (a_73B7E0(1) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x763BD0 (module 090 sub_763BD0): director state - next state at director cue 3 (a_73B7E0).
	uint32_t __cdecl t_763BD0(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
			t1_next_state(a1);
		return 0; // void
	}

	// 0x763C90 (module 090 sub_763C90): director state - at director cue 7 waits 0x2C ticks
	// (+0x44), next state.
	uint32_t __cdecl t_763C90(uint32_t a1)
	{
		if (a_73B7E0(7) != 0)
		{
			U16(a1, 0x44) = 0x2C;
			t1_next_state(a1);
		}
		return 0; // void
	}

	// 0x763CC0 (module 090 sub_763CC0): director state - counts +0x44 down; at 0 sets data +0x48
	// = 1 (end flag), next state.
	uint32_t __cdecl t_763CC0(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) <= 0)
		{
			U16(t1_data(), 0x48) = 1;
			t1_next_state(a1);
		}
		return 0; // void
	}

	// 0x763CF0 (module 090 sub_763CF0): director state - at cue 9 (a_73B640) releases the voice
	// slot data +0x52 (0x4A2940), next state.
	uint32_t __cdecl t_763CF0(uint32_t a1)
	{
		if (a_73B640(9) != 0)
		{
			x::sub_4A2940((uint32_t)(int32_t)S16(t1_data(), 0x52));
			t1_next_state(a1);
		}
		return 0; // void
	}

	// 0x763D60 (module 090 MAG_090_SpawnTonberryCreature): spawns the tasks of the new phase:
	// 1 = prim-model particle task 0x763EB0 (model 0x16D7DEC, 0x48C) + task 0x7681B0; 2 = the
	// creature actor 0x7674E0 (0x148 B, queue 0x25A3F90, model container 0x1547514 anim 3) + task
	// 0x7670E0; 6 = 0x766FB0; 7 = 0x7667B0; 8 = 0x7682A0 (0x7C B, queue 0x259F098).
	uint32_t __cdecl t_763D60(uint32_t a1)
	{
		const uint32_t k = (uint32_t)((int32_t)S16(t1_data(), 0x40) - 1);
		switch (k)  // jump table 0x763E40 (ja 7 -> nothing)
		{
		case 0:
			t_763E60(a1, 0x763EB0, 0x16D7DEC, 0x48C, 0, 0);
			return x::Effect_AddTaskAndInitFromCtx(0x259F098, 0x7681B0, 0x7C, a1);
		case 1:
		{
			const uint32_t creature = x::Effect_AddTaskAndInitFromCtx(0x25A3F90, 0x7674E0, 0x148, a1);
			x::Effect_BindModelContainerSetAnim(creature, 0x1547514, 3);
			return x::Effect_AddTaskAndInitFromCtx(0x259F098, 0x7670E0, 0x7C, a1);
		}
		case 5:
			return x::Effect_AddTaskAndInitFromCtx(0x259F098, 0x766FB0, 0x7C, a1);
		case 6:
			return x::Effect_AddTaskAndInitFromCtx(0x259F098, 0x7667B0, 0x7C, a1);
		case 7:
			return x::Effect_AddTaskAndInitFromCtx(0x259F098, 0x7682A0, 0x7C, a1);
		default:
			return k;
		}
	}

	// 0x763E60 (module 090 sub_763E60): spawns task a2 (0x520 B, queue 0x25A2B60, parent a1) and
	// sets its model data +0x74 = a3, +0x78 = (s16)a4, +0x80 = a5, +0x82 = a6; returns the node.
	uint32_t __cdecl t_763E60(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25A2B60, a2, 0x520, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		U32(t, 0x78) = (uint32_t)(int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x763EB0 (module 090 sub_763EB0): Tonberry prim-model PARTICLE task (node 0x520) - selects
	// the per-model depth table [0x25A4C04] by the director's ticks in phase (0x1E/0x5A/0x64/0x69/
	// 0x71), then runs its 5-state table (init t_763F80 / play a_73B8A0 / t_764100 / t_766760 /
	// nullsub).
	uint32_t __cdecl t_763EB0(uint32_t a1)
	{
		uint32_t states[5];
		const uint32_t d = t1_data();
		states[0] = 0x763F80;
		states[1] = 0x7640E0;
		states[2] = 0x764100;
		const uint16_t ticks = U16(d, 0x46);
		states[3] = 0x766760;
		states[4] = 0x7667A0;  // nullsub
		if (ticks == 0x1E)
			MEM<uint32_t>(0x25A4C04) = 0x1547468;
		else if (ticks == 0x5A)
			MEM<uint32_t>(0x25A4C04) = 0x154747C;
		else if (ticks == 0x64)
			MEM<uint32_t>(0x25A4C04) = 0x1547490;
		else if (ticks == 0x69)
			MEM<uint32_t>(0x25A4C04) = 0x15474A4;
		else if (ticks == 0x71)
			MEM<uint32_t>(0x25A4C04) = 0x15474B8;
		const uint32_t node = a1;
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return t1_task_end(node, status);
	}

	// 0x763F80 (module 090 sub_763F80): particle task state 0 - from frame 6: decodes the prim
	// layout of model +0x74 (index +0x78) into +0x94, position = director origin (y cleared) +
	// the vector (0,0,0x180) rotated by yaw +0x62, depth table [0x25A4C04] = 0x1547454, scale
	// 0x1000^3, bounds (t_764070), next state.
	uint32_t __cdecl t_763F80(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 6)
			return 0; // void
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		alignas(4) uint16_t off[4] = {};   // [esp+4] SVECTOR out of t_764020
		const uint32_t d = t1_data();
		const uint32_t xy = U32(d, 0);
		const uint32_t zp = U32(d, 4);
		U32(node, 0x1C) = xy;
		// quirk 0x763FB8: `mov cx, [esi+0x62]` keeps the high word of ecx (= high word of data
		// dword +0); t_764020 only reads the low word
		const uint32_t yaw = (xy & 0xFFFF0000u) | U16(node, 0x62);
		U32(node, 0x20) = zp;
		U16(node, 0x1E) = 0;
		t_764020(yaw, 0x180, P(off));
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + off[1]);
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + off[0]);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + off[2]);
		MEM<uint32_t>(0x25A4C04) = 0x1547454;
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		t_764070(node);
		t1_next_state(node);
		return 0; // void
	}

	// 0x764020 (module 090 sub_764020): rotates the vector (0, 0, a2) by the yaw a1 (s16) into the
	// SVECTOR a3 (identity 0x8DD770, rotate-Y 0x8DD8A0, matrixMultiplyVector).
	uint32_t __cdecl t_764020(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack: +0x00 SVECTOR (0, 0, a2, pad unwritten), +0x08 Mat4x3 (0x20 bytes)
		alignas(4) uint8_t loc[0x28] = {};
		const uint32_t L = P(loc);
		const uint32_t M = L + 8;
		U16(L, 0) = 0;
		U16(L, 2) = 0;
		U16(L, 4) = (uint16_t)a2;
		x::MAG_022_sub_8DD770(M);
		x::MAG_022_sub_8DD8A0(M, (uint32_t)(int32_t)(int16_t)a1);
		return x::matrixMultiplyVector(M, L, a3);
	}

	// 0x764070 (module 090 sub_764070): bounds of the particle model - min/max box (init
	// 0x7FFF/-0x7FFF) of section data[+0x24] of model +0x74 (0x8DE160), stored for the position
	// +0x1C (0x8DE1F0).
	uint32_t __cdecl t_764070(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x74);
		const int32_t v = S32(data, 0x24);
		const uint32_t sec = data + (uint32_t)shl32(v / 4, 2);
		alignas(4) int16_t box[6];   // [esp+4]: min x,y,z / max x,y,z
		box[2] = 0x7FFF;
		box[1] = 0x7FFF;
		box[0] = 0x7FFF;
		box[5] = (int16_t)0x8001;
		box[4] = (int16_t)0x8001;
		box[3] = (int16_t)0x8001;
		xm::sub_8DE160(sec, P(box));
		return xm::sub_8DE1F0(node + 0x1C, P(box));
	}

	// 0x764100 (module 090 sub_764100): particle task state - at director cue 2 (a_73B7E0) runs
	// a_73C280 on the node, next state.
	uint32_t __cdecl t_764100(uint32_t a1)
	{
		if (a_73B7E0(2) != 0)
		{
			a_73C280(a1);
			t1_next_state(a1);
		}
		return 0; // void
	}

	// 0x764260 (module 090 sub_764260): prim-model player draw callback (layout a1, record a2,
	// block a3) - picks the object's vertex frame (lerped by MAG_017_sub_701390), builds its matrix
	// (rotation, parent-rotated position unless record flag 0x200, scale), fills a 0x68-byte
	// Field_Alloc render header (flags 0x2000/0x2030 |0xC |0xC0, colour, alpha, depth offset =
	// block +0x58 or, when block +0x56 == 0, the per-model table [0x25A4C04]) and draws it with
	// t_764540 into OT base+0x44 (shift 2).
	uint32_t __cdecl t_764260(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack (esp after push ebp/esi): +0x10 SVECTOR position (+0x16 pad unwritten),
		// +0x18 scale (3 x int32), +0x28 Mat4x3 object matrix (+0x3C translation)
		alignas(4) uint8_t loc[0x48] = {};
		const uint32_t L = P(loc);
		const uint32_t V = L + 0x10;
		const uint32_t S = L + 0x18;
		const uint32_t M = L + 0x28;
		const uint32_t rec = a2;
		if (((uint32_t)(int32_t)S16(rec, 0x1C) | U32(rec, 0x18)) == 0)
			return 0; // void (zero scale: nothing drawn)
		if (S16(rec, 0x24) >= 0x1000 && U32(rec, 0x20) == 0)
			return 0; // void

		const uint32_t hdr = x::Field_Alloc(0x68);
		const int32_t model = S16(rec, 2);
		const uint32_t data = U32(a1, 0);
		const uint32_t blk = a3;
		const uint32_t base = data + U32(data, model * 4 + 8);
		const int16_t f1 = S16(rec, 0x2A);   // second vertex frame
		const int16_t f0 = S16(rec, 0x28);   // first vertex frame
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
			const int16_t t = S16(rec, 0x26);   // blend factor 0..0x1000
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

		x::MAG_017_sub_701310(rec + 0x10, M);
		const uint16_t vx = U16(rec, 8);
		const uint16_t vy = U16(rec, 0xA);
		const uint16_t vz = U16(rec, 0xC);
		U16(V, 0) = vx;
		U16(V, 2) = vy;
		U16(V, 4) = vz;
		int32_t t0, t1, t2;
		if ((U8(rec, 5) & 2) != 0)
		{
			// record flag 0x200: position not rotated by the parent (no GTE_MatrixMultiply either)
			t0 = (int16_t)vx;
			t1 = (int16_t)vy;
			t2 = (int16_t)vz;
		}
		else
		{
			x::GTE_SetRotMatrix(blk);
			x::GTE_LoadV0(V);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			x::GTE_MatrixMultiply(blk, M);
			t2 = S32(M, 0x1C);
			t1 = S32(M, 0x18);
			t0 = S32(M, 0x14);
		}
		S32(M, 0x14) = add32(t0, S32(blk, 0x14));
		S32(M, 0x18) = add32(t1, S32(blk, 0x18));
		const uint32_t sxy = U32(rec, 0x18);
		S32(M, 0x1C) = add32(t2, S32(blk, 0x1C));
		if (sxy != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			S32(S, 0) = S16(rec, 0x18);
			S32(S, 4) = S16(rec, 0x1A);
			S32(S, 8) = S16(rec, 0x1C);
			x::scale3DMatrix(M, S);
		}
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		const uint32_t flags = U32(rec, 4);
		U32(hdr, 0x14) = (flags & 0x4000) != 0 ? 0x2000u : 0x2030u;
		if ((flags & 0x2000) != 0)
			U32(hdr, 0x14) |= 0xC;
		const int32_t alpha = S16(rec, 0x24);
		U32(hdr, 0xC) = (uint32_t)alpha;
		if (alpha != 0)
		{
			U32(hdr, 0x14) |= 0xC0;
			U32(hdr, 8) = U32(rec, 0x20);   // colour
		}
		U32(hdr, 0x10) = (uint32_t)(int32_t)S16(blk, 0x58);   // depth offset
		uint32_t cursor, ot;
		if (U16(blk, 0x56) == 0)
		{
			const int32_t mi = S16(rec, 2);
			const uint32_t tab = MEM<uint32_t>(0x25A4C04);
			U16(hdr, 0x18) = 0;
			U16(hdr, 0x1A) = 0;
			U16(hdr, 0x1E) = 0;
			const int32_t dz = S16(tab, mi * 2);   // per-model depth offset
			cursor = MEM<uint32_t>(0x1D8E054);
			U16(hdr, 0x22) = 0x100;
			U16(hdr, 0x20) = 0x100;
			U16(hdr, 0x2A) = 0x100;
			U16(hdr, 0x28) = 0x100;
			ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
			U32(hdr, 0x10) = (uint32_t)dz;
			U16(hdr, 0x1C) = 0;
			U16(hdr, 0x26) = 0;
			U16(hdr, 0x24) = 0;
		}
		else
		{
			cursor = MEM<uint32_t>(0x1D8E054);
			ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
			U16(hdr, 0x18) = 0;
			U16(hdr, 0x1A) = 0;
			U16(hdr, 0x1E) = 0;
			U16(hdr, 0x1C) = 0;
			U16(hdr, 0x22) = 0x100;
			U16(hdr, 0x20) = 0x100;
			U16(hdr, 0x26) = 0;
			U16(hdr, 0x24) = 0;
			U16(hdr, 0x2A) = 0x100;
			U16(hdr, 0x28) = 0x100;
		}
		MEM<uint32_t>(0x1D8E054) = t_764540(hdr, ot, 2, cursor);
		x::Field_Free(0x68);
		return 0; // void (prim player callback)
	}

	// 0x764540 (module 090 sub_764540): renders a prim model through the render header a1 (OT a2,
	// depth shift a3, packet cursor a4): vertex base = header+0 +8 unless flag 0x2000, sets the
	// GTE far colour from header colour bytes +8..+0xA (0x45DD60), then walks the 8 primitive
	// lists (flat tri t_764670, t_764890, t_764B20, t_765010, a_73D770, a_73D9C0, t_765B30,
	// t_766090; an empty list is skipped); returns the new packet cursor.
	uint32_t __cdecl t_764540(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t hdr = a1;
		if ((U32(hdr, 0x14) & 0x2000) == 0)
			U32(hdr, 4) = U32(hdr, 0) + 8;
		{
			const uint32_t model = U32(hdr, 0);
			const uint32_t b = U8(hdr, 0xA);
			U32(hdr, 0x2C) = U32(model, 0) + model;   // primitive lists
			const uint32_t g = U8(hdr, 9);
			const uint32_t r = U8(hdr, 8);
			x::someCameraWork_45DD60(r, g, b);
		}
		uint32_t cursor = a4;
		typedef uint32_t (__cdecl *Draw)(uint32_t, uint32_t, uint32_t, uint32_t);
		static const Draw lists[8] = { t_764670, t_764890, t_764B20, t_765010, a_73D770, a_73D9C0, t_765B30, t_766090 };
		for (int i = 0; i < 8; i++)
		{
			const uint32_t list = U32(hdr, 0x2C);
			if (U32(list, 0) != 0)
				cursor = lists[i](hdr, a2, a3, cursor);
			else
				U32(hdr, 0x2C) = list + 4;
		}
		return cursor;
	}

	// 0x764670 (module 090 sub_764670): prim-list block "flat triangles" (12-byte records:
	// code+rgb, 3 vertex indices at +4/+6/+8) -> POLY_F3 packets (0x14 B): RTPT, semi-transparency
	// from header flags 1/4, GTE flag / backface (flag 0x10 = double-sided) / screen clip rejects,
	// optional depth-cued colour (flag 0x40, level header +0xC), OT z = (OTZ + header +0x10) >> a3,
	// InsertPrimAutoDepth; advances the list pointer header +0x2C, returns the cursor.
	uint32_t __cdecl t_764670(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;                 // (the original keeps it in its a1 slot)
		const uint32_t list = U32(ctx, 0x2C);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // ebp / [esp+0xc]
		U32(ctx, 0x2C) = rec;
		const uint32_t vbase = U32(ctx, 4);   // (the original stores it into its a4 slot)
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return a4;
		}
		uint32_t pkt = cursor + 0xC;          // edi (advances with the cursor)
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			const uint32_t flags = U32(ctx, 0x14);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x4000000;       // tag: 4 words
			U32(pkt, -8) = code;
			if (flags & 1)
			{
				code |= 0x2000000;
				U32(pkt, -8) = code;
			}
			if (flags & 4)
				U32(pkt, -8) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (!(U32(ctx, 0x3C) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;            // [esp+0x18]
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) >= 0 || (U8(ctx, 0x14) & 0x10))
				{
					x::GTE_ReadSXY012_Split(pkt - 4, pkt, pkt + 4);
					x::GTE_AVSZ3();
					if (t1_out_x(S16(pkt, -4))) clip = 1;
					if (t1_out_x(S16(pkt, 0))) clip |= 2;
					if (t1_out_x(S16(pkt, 4))) clip |= 4;
					if (t1_out_y(S16(pkt, -2))) clip |= 0x10;
					if (t1_out_y(S16(pkt, 2))) clip |= 0x20;
					if (t1_out_y(S16(pkt, 6))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x38);
						if (U8(ctx, 0x14) & 0x40)
						{
							x::set_unk_1CA8A28(pkt - 8);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt - 8);
						}
						int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
						S32(ctx, 0x38) = z;
						if (z < 0)
							S32(ctx, 0x38) = 0;
						z = S32(ctx, 0x38) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x14;
						pkt += 0x14;
					}
				}
			}
			rec += 0xC;
		} while (--count);
		U32(ctx, 0x2C) = rec;
		return cursor;
	}

	// ====================================================================================
	// part t2
	// ====================================================================================
	// part t2: Tonberry (090) model renderer primitive lists - flat quads (t_764890), textured
	// triangles (t_764B20) and textured quads (t_765010). All three read a render context a1
	// (+0x04 vertex base, +0x0C depth-cue far colour, +0x10 OTZ bias, +0x14 flags: bit0 semi-trans on,
	// bit2 semi-trans off, bit4 draw back faces, bit6 depth cue; +0x18/+0x1A u/v scroll, +0x1C/+0x24
	// texture-window RECTs, +0x28/+0x2A u/v wrap sizes (byte), +0x2C primitive-list cursor, +0x30 MAC0,
	// +0x38 OTZ, +0x3C GTE FLAG), emit packets from cursor a4 into OT a2 at (OTZ + bias) >> a3 and
	// return the new packet cursor.
	namespace
	{
		// GP0 0xE2 texture-window word from a RECT {x,y,w,h} (u16 each) at ctx+b
		// (0x764E3D-0x764E7C / 0x764EDF-0x764F1E; 0x76540C-0x76544A / 0x7654B3-0x7654F1)
		uint32_t t2_twin(uint32_t ctx, int32_t b)
		{
			uint32_t ecx = (uint32_t)(U8(ctx, b + 2) & 0xF8) | 0xFFFE2000u;  // y
			uint32_t edx = (uint32_t)(U8(ctx, b) & 0xF8);                     // x
			ecx <<= 5;
			ecx |= edx;
			edx = U16(ctx, b + 6);                                            // h
			ecx <<= 5;
			edx = ~(edx - 1) & 0xF8;
			ecx |= edx;
			edx = U16(ctx, b + 4);                                            // w
			ecx <<= 2;
			edx = (uint32_t)((int32_t)~(edx - 1) >> 3) & 0x1F;
			ecx |= edx;
			return ecx;
		}

		// screen x outside [0, 0xA00] / y outside [0, 0x6C0] (signed 16-bit SXY halves)
		inline bool t2_outx(uint32_t p) { const int16_t v = S16(p, 0); return v < 0 || v > 0xA00; }
		inline bool t2_outy(uint32_t p) { const int16_t v = S16(p, 0); return v < 0 || v > 0x6C0; }

		// depth cue of the packet colour word at pCode towards the far colour ctx+0x0C
		inline void t2_depth_cue(uint32_t ctx, uint32_t pCode)
		{
			x::set_unk_1CA8A28(pCode);
			x::set_dword_1CA8A30(U32(ctx, 0xC));
			x::sub_45F270();
			x::set_param_with_dword_1CA8A68(pCode);
		}

		// OTZ += bias (clamped to 0 when negative); returns the OT entry a2 + (OTZ >> a3) * 4
		inline uint32_t t2_ot(uint32_t ctx, uint32_t a2, uint32_t a3)
		{
			const int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
			S32(ctx, 0x38) = z;
			if (z < 0)
				S32(ctx, 0x38) = 0;
			return a2 + (uint32_t)(S32(ctx, 0x38) >> (a3 & 31)) * 4u;
		}
	}

	// 0x764890 (module 090 sub_764890): draws the flat-quad list of render context a1 (count, then
	// records of 0xC bytes: code/colour, 4 vertex indices): RTPT + RTPS, rejects on GTE FLAG /
	// back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit 0x40), emits
	// one POLY_F4 (0x18 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. Returns the new cursor.
	uint32_t __cdecl t_764890(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t rec = U32(ctx, 0x2C);
		int32_t count = S32(rec, 0);
		rec += 4;
		U32(ctx, 0x2C) = rec;
		const uint32_t vbase = U32(ctx, 4);
		if (count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x5000000;  // tag: 5 words
				U32(pkt, 4) = code;
				if (flags & 1)
					U32(pkt, 4) = code | 0x2000000;  // semi-transparent on
				if (flags & 4)
					U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFFu;  // semi-transparent off
			}
			x::GTE_ReadFLAG(ctx + 0x3C);
			if ((U32(ctx, 0x3C) & 0x60000) != 0)
				goto next;
			x::GTE_NCLIP();
			{
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x10))
					goto next;
				x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0xC, pkt + 0x10);
				x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0xA) * 4u);
				x::GTE_RTPS();
				if (t2_outx(pkt + 8))
					clip = 1;
				if (t2_outx(pkt + 0xC))
					clip |= 2;
				if (t2_outx(pkt + 0x10))
					clip |= 4;
				if (t2_outy(pkt + 0xA))
					clip |= 0x10;
				if (t2_outy(pkt + 0xE))
					clip |= 0x20;
				if (t2_outy(pkt + 0x12))
					clip |= 0x40;
				x::GTE_ReadSXY2(pkt + 0x14);
				x::GTE_AVSZ4();
				if (t2_outx(pkt + 0x14))
					clip |= 8;
				if (t2_outy(pkt + 0x16))
					clip |= 0x80;
				if ((clip & 0xF) == 0xF)
					goto next;
				if ((clip & 0xF0) == 0xF0)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
				t2_depth_cue(ctx, pkt + 4);
			{
				const uint32_t ot = t2_ot(ctx, a2, a3);
				x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
				pkt += 0x18;
			}
		next:
			rec += 0xC;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x764B20 (module 090 sub_764B20): draws the textured-triangle list of render context a1
	// (count, then records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage,
	// uv2 in the high half of +8): RTPT, rejects on GTE FLAG / back-face / fully off-screen, optional
	// depth cue, emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ(AVSZ3) + bias) >> a3.
	// With a UV scroll (+0x18/+0x1A) the texel bytes are scrolled (wrapped by +0x28/+0x2A) and the
	// poly is bracketed by two 0xE2 texture-window prims (0xC B each, windows +0x1C / +0x24);
	// without one a DR_MODE prim (0xC B, tpage abr 1) is inserted before the poly.
	uint32_t __cdecl t_764B20(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t rec = U32(ctx, 0x2C);
		int32_t count = S32(rec, 0);
		rec += 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x2C) = rec;
		if (count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x7000000;  // tag: 7 words
				U32(pkt, 4) = code;
				if (flags & 1)
					U32(pkt, 4) = code | 0x2000000;
				if (flags & 4)
					U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFFu;
				const uint32_t uv2 = U32(rec, 8) >> 16;
				U32(pkt, 0xC) = U32(rec, 0xC);   // uv0 + clut
				U32(pkt, 0x14) = U32(rec, 0x10); // uv1 + tpage
				U32(pkt, 0x1C) = uv2;            // uv2
			}
			x::GTE_ReadFLAG(ctx + 0x3C);
			if ((U32(ctx, 0x3C) & 0x60000) != 0)
				goto next;
			x::GTE_NCLIP();
			{
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x10))
					goto next;
				x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
				x::GTE_AVSZ3();
				if (t2_outx(pkt + 8))
					clip = 1;
				if (t2_outx(pkt + 0x10))
					clip |= 2;
				if (t2_outx(pkt + 0x18))
					clip |= 4;
				if (t2_outy(pkt + 0xA))
					clip |= 0x10;
				if (t2_outy(pkt + 0x12))
					clip |= 0x20;
				if (t2_outy(pkt + 0x1A))
					clip |= 0x40;
				if ((clip & 7) == 7)
					goto next;
				if ((clip & 0x70) == 0x70)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
				t2_depth_cue(ctx, pkt + 4);
			{
				const uint32_t ot = t2_ot(ctx, a2, a3);
				const uint16_t add0 = U16(ctx, 0x18);
				const uint16_t add1 = U16(ctx, 0x1A);
				if ((uint16_t)(add0 | add1) != 0)
				{
					if (add0 != 0)
					{
						const uint32_t a = add0;
						const uint32_t c2 = U8(pkt, 0x1C) + a;
						const uint32_t c0 = U8(pkt, 0xC) + a;
						const uint32_t c1 = U8(pkt, 0x14) + a;
						uint8_t s = 0;
						if ((int32_t)(c2 | c1 | c0) > 0xFF)
							s = U8(ctx, 0x28);  // u wrap
						U8(pkt, 0xC) = (uint8_t)(c0 - s);
						U8(pkt, 0x14) = (uint8_t)(c1 - s);
						U8(pkt, 0x1C) = (uint8_t)(c2 - s);
					}
					const uint16_t add1b = U16(ctx, 0x1A);  // re-read (0x764D8F)
					if (add1b != 0)
					{
						const uint32_t a = add1b;
						const uint32_t c2 = U8(pkt, 0x1D) + a;
						const uint32_t c0 = U8(pkt, 0xD) + a;
						const uint32_t c1 = U8(pkt, 0x15) + a;
						uint8_t s = 0;
						if ((int32_t)(c2 | c1 | c0) > 0xFF)
							s = U8(ctx, 0x2A);  // v wrap
						U8(pkt, 0xD) = (uint8_t)(c0 - s);
						U8(pkt, 0x15) = (uint8_t)(c1 - s);
						U8(pkt, 0x1D) = (uint8_t)(c2 - s);
					}
					const uint32_t poly = pkt;
					uint32_t prim = pkt + 0x20;
					pkt += 0x2C;
					U32(prim, 0) = 0x2000000;
					uint32_t tw = 0;
					if (ctx + 0x1C != 0)
						tw = t2_twin(ctx, 0x1C);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
					prim = pkt;
					pkt += 0xC;
					U32(prim, 0) = 0x2000000;
					tw = 0;
					if (ctx + 0x24 != 0)
						tw = t2_twin(ctx, 0x24);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
				}
				else
				{
					const uint32_t poly = pkt;
					const uint32_t mode = pkt + 0x20;
					pkt += 0x2C;
					const uint32_t tpage = x::sub_45C690(0, 1, 0, 0);  // GetTPage(tp 0, abr 1, x 0, y 0)
					x::sub_45BFC0(mode, 0, 0, tpage & 0xFFFF, 0);        // SetDrawMode(mode, dfe 0, dtd 0, tpage, tw NULL)
					x::SSIGPU_InsertPrimAutoDepth(ot, mode);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
				}
			}
		next:
			rec += 0x14;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x765010 (module 090 sub_765010): draws the textured-quad list of render context a1 (count,
	// then records of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16):
	// RTPT + RTPS, rejects on GTE FLAG / back-face / fully off-screen, optional depth cue, emits one
	// POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3. With a UV scroll the texel
	// bytes are scrolled and the poly is bracketed by two 0xE2 texture-window prims; without one a
	// DR_MODE prim (tpage abr 1) is inserted before the poly.
	uint32_t __cdecl t_765010(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t rec = U32(ctx, 0x2C);
		int32_t count = S32(rec, 0);
		rec += 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x2C) = rec;
		if (count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x9000000;  // tag: 9 words
				U32(pkt, 4) = code;
				if (flags & 1)
					U32(pkt, 4) = code | 0x2000000;
				if (flags & 4)
					U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFFu;
				U32(pkt, 0xC) = U32(rec, 0xC);   // uv0 + clut
				const uint32_t uv1 = U32(rec, 0x10);
				const uint32_t uv23 = U32(rec, 0x14);
				U32(pkt, 0x1C) = uv23;           // uv2 (uv3 lands in the pad half, as in the original)
				U32(pkt, 0x14) = uv1;            // uv1 + tpage
				U32(pkt, 0x24) = uv23 >> 16;     // uv3
			}
			x::GTE_ReadFLAG(ctx + 0x3C);
			if ((U32(ctx, 0x3C) & 0x60000) != 0)
				goto next;
			x::GTE_NCLIP();
			{
				uint32_t clip = 0;
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x10))
					goto next;
				x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
				x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0xA) * 4u);
				x::GTE_RTPS();
				if (t2_outx(pkt + 8))
					clip = 1;
				if (t2_outx(pkt + 0x10))
					clip |= 2;
				if (t2_outx(pkt + 0x18))
					clip |= 4;
				if (t2_outy(pkt + 0xA))
					clip |= 0x10;
				if (t2_outy(pkt + 0x12))
					clip |= 0x20;
				if (t2_outy(pkt + 0x1A))
					clip |= 0x40;
				x::GTE_ReadSXY2(pkt + 0x20);
				x::GTE_AVSZ4();
				if (t2_outx(pkt + 0x20))
					clip |= 8;
				if (t2_outy(pkt + 0x22))
					clip |= 0x80;
				if ((clip & 0xF) == 0xF)
					goto next;
				if ((clip & 0xF0) == 0xF0)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
				t2_depth_cue(ctx, pkt + 4);
			{
				const uint32_t ot = t2_ot(ctx, a2, a3);
				const uint16_t add0 = U16(ctx, 0x18);
				const uint16_t add1 = U16(ctx, 0x1A);
				if ((uint16_t)(add0 | add1) != 0)
				{
					if (add0 != 0)
					{
						const uint32_t a = add0;
						const uint32_t c0 = U8(pkt, 0xC) + a;
						const uint32_t c1 = U8(pkt, 0x14) + a;
						const uint32_t c2 = U8(pkt, 0x1C) + a;
						const uint32_t c3 = U8(pkt, 0x24) + a;
						uint8_t s = 0;
						if ((int32_t)(c3 | c2 | c1 | c0) > 0xFF)
							s = U8(ctx, 0x28);  // u wrap
						U8(pkt, 0xC) = (uint8_t)(c0 - s);
						U8(pkt, 0x14) = (uint8_t)(c1 - s);
						U8(pkt, 0x1C) = (uint8_t)(c2 - s);
						U8(pkt, 0x24) = (uint8_t)(c3 - s);
					}
					const uint16_t add1b = U16(ctx, 0x1A);  // re-read (0x765326)
					if (add1b != 0)
					{
						const uint32_t a = add1b;
						const uint32_t c0 = U8(pkt, 0xD) + a;
						const uint32_t c1 = U8(pkt, 0x15) + a;
						const uint32_t c2 = U8(pkt, 0x1D) + a;
						const uint32_t c3 = U8(pkt, 0x25) + a;
						uint8_t s = 0;
						if ((int32_t)(c3 | c2 | c1 | c0) > 0xFF)
							s = U8(ctx, 0x2A);  // v wrap
						U8(pkt, 0xD) = (uint8_t)(c0 - s);
						U8(pkt, 0x15) = (uint8_t)(c1 - s);
						U8(pkt, 0x1D) = (uint8_t)(c2 - s);
						U8(pkt, 0x25) = (uint8_t)(c3 - s);
					}
					const uint32_t poly = pkt;
					uint32_t prim = pkt + 0x28;
					pkt += 0x34;
					U32(prim, 0) = 0x2000000;
					uint32_t tw = 0;
					if (ctx + 0x1C != 0)
						tw = t2_twin(ctx, 0x1C);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
					prim = pkt;
					pkt += 0xC;
					U32(prim, 0) = 0x2000000;
					tw = 0;
					if (ctx + 0x24 != 0)
						tw = t2_twin(ctx, 0x24);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
				}
				else
				{
					const uint32_t poly = pkt;
					const uint32_t mode = pkt + 0x28;
					pkt += 0x34;
					const uint32_t tpage = x::sub_45C690(0, 1, 0, 0);  // GetTPage(tp 0, abr 1, x 0, y 0)
					x::sub_45BFC0(mode, 0, 0, tpage & 0xFFFF, 0);        // SetDrawMode(mode, dfe 0, dtd 0, tpage, tw NULL)
					x::SSIGPU_InsertPrimAutoDepth(ot, mode);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
				}
			}
		next:
			rec += 0x18;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// ====================================================================================
	// part t3
	// ====================================================================================
	// Tonberry (090) part t3: the two texture-scrolling prim-list block drawers (Gouraud-textured
	// triangles / quads with UV scroll + texture window), the "wait, then finish" state, the
	// prim-model actor task and its per-tick model draw.
	namespace
	{
		// screen bounds test of a projected coordinate (x: 0..0xA00, y: 0..0x6C0, signed 16-bit)
		inline bool t3_out_x(int16_t v) { return v < 0 || v > 0xA00; }
		inline bool t3_out_y(int16_t v) { return v < 0 || v > 0x6C0; }

		// E2 texture-window command word from a {u16 x, u16 y, u16 w, u16 h} block at p
		// (0x765E84 / 0x765F39 / 0x766527 / 0x7665EC): offsets = low byte & 0xF8, masks from
		// ~(w-1) / ~(h-1); 0xFFFE2000 << 12 = 0xE2000000
		inline uint32_t t3_texwin(uint32_t p)
		{
			uint32_t t = ((uint32_t)U8(p, 2) & 0xF8) | 0xFFFE2000u;
			t = (t << 5) | ((uint32_t)U8(p, 0) & 0xF8);
			t = (t << 5) | (~((uint32_t)U16(p, 6) - 1) & 0xF8);
			t = (t << 2) | ((~((uint32_t)U16(p, 4) - 1) >> 3) & 0x1F);
			return t;
		}

		// adds d to the n (3 or 4) texture bytes pkt+off[i]; when any sum exceeds 0xFF, every sum
		// is wrapped by subtracting the byte `wrap` (8-bit stores)
		inline void t3_scroll(uint32_t pkt, const int32_t *off, int n, uint16_t d, uint8_t wrap)
		{
			uint32_t s[4];
			uint32_t any = 0;
			for (int i = 0; i < n; i++)
			{
				s[i] = (uint32_t)U8(pkt, off[i]) + (uint32_t)d;
				any |= s[i];
			}
			const bool over = (int32_t)any > 0xFF;
			for (int i = 0; i < n; i++)
				U8(pkt, off[i]) = over ? (uint8_t)(s[i] - wrap) : (uint8_t)s[i];
		}
	}

	// 0x765B30 (module 090 sub_765B30): prim-list block "Gouraud-textured triangles, UV scroll":
	// 0x1C-byte records (code/rgb0 +0, vertex indices +4/+6/+8, uv2 +0x0A, uv0+clut +0x0C,
	// uv1+tpage +0x10, rgb1 +0x14, rgb2 +0x18) -> POLY_GT3 (tag 0x09000000) with RTPT, back-face
	// (NCLIP, unless ctx+0x14 bit 0x20) and off-screen culling, optional lighting (bit 0x80),
	// z = OTZ + ctx+0x10 (>= 0) >> a3 into OT a2. When the scroll ctx+0x18/+0x1A (u/v) is set, the
	// UVs are scrolled (wrapped by ctx+0x28/+0x2A) and the poly is framed by two E2 texture-window
	// packets (ctx+0x1C set, ctx+0x24 restore); else followed by one draw-mode packet (tpage abr 1).
	// Returns the new packet cursor; ctx+0x2C advances past the block.
	uint32_t __cdecl t_765B30(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		static const int32_t U_OFF[3] = { 0x0C, 0x18, 0x24 };
		static const int32_t V_OFF[3] = { 0x0D, 0x19, 0x25 };
		const uint32_t ctx = a1;
		uint32_t pkt = a4;                              // esi
		const uint32_t list = U32(ctx, 0x2C);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;                        // ebx / [esp+0x28]
		const uint32_t vbase = U32(ctx, 4);             // [esp+0x40]
		U32(ctx, 0x2C) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			const uint32_t flags = U32(ctx, 0x14);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0x9000000;                    // tag: 9 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;                      // semi-transparent
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			const uint32_t uv0 = U32(rec, 0x0C);
			const uint32_t w8 = U32(rec, 8);
			const uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0x0C) = uv0;
			U32(pkt, 0x18) = uv1;
			U32(pkt, 0x24) = w8 >> 16;                  // uv2 (record +0x0A)
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (!(U32(ctx, 0x3C) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;                      // [esp+0x24]
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) >= 0 || (U8(ctx, 0x14) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_AVSZ3();
					if (t3_out_x(S16(pkt, 8))) clip = 1;
					if (t3_out_x(S16(pkt, 0x14))) clip |= 2;
					if (t3_out_x(S16(pkt, 0x20))) clip |= 4;
					if (t3_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (t3_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (t3_out_y(S16(pkt, 0x22))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x38);
						if (U8(ctx, 0x14) & 0x80)
						{
							// lit: colours rgb1, rgb2 and the packet's rgb0
							x::sub_45E120(rec + 0x14, rec + 0x18, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 4);
						}
						else
						{
							const uint32_t c1 = U32(rec, 0x14), c2 = U32(rec, 0x18);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
						}
						int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
						S32(ctx, 0x38) = z;
						if (z < 0)
							S32(ctx, 0x38) = 0;
						z = S32(ctx, 0x38) >> (a3 & 31);
						const uint32_t ot = a2 + (uint32_t)z * 4;   // [esp+0x20]
						const uint16_t du = U16(ctx, 0x18);
						const uint16_t dv = U16(ctx, 0x1A);
						if ((uint16_t)(du | dv) != 0)
						{
							if (du != 0)
								t3_scroll(pkt, U_OFF, 3, du, U8(ctx, 0x28));
							const uint16_t dv2 = U16(ctx, 0x1A);
							if (dv2 != 0)
								t3_scroll(pkt, V_OFF, 3, dv2, U8(ctx, 0x2A));
							// OT is LIFO: texture window set (ctx+0x1C), poly, window restore (ctx+0x24)
							const uint32_t w1 = pkt + 0x28;
							U32(w1, 0) = 0x2000000;
							const uint32_t t1 = (ctx + 0x1C) != 0 ? t3_texwin(ctx + 0x1C) : 0;
							U32(w1, 4) = t1;
							U32(w1, 8) = 0;
							x::SSIGPU_InsertPrimAutoDepth(ot, w1);
							x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
							const uint32_t w2 = pkt + 0x34;
							U32(w2, 0) = 0x2000000;
							const uint32_t t2 = (ctx + 0x24) != 0 ? t3_texwin(ctx + 0x24) : 0;
							U32(w2, 4) = t2;
							U32(w2, 8) = 0;
							x::SSIGPU_InsertPrimAutoDepth(ot, w2);
							pkt += 0x40;
						}
						else
						{
							// draw-mode packet (tpage abr 1) inserted before the poly (drawn after it)
							const uint32_t dm = pkt + 0x28;
							const uint32_t tp = x::sub_45C690(0, 1, 0, 0);
							x::sub_45BFC0(dm, 0, 0, tp & 0xFFFF, 0);
							x::SSIGPU_InsertPrimAutoDepth(ot, dm);
							x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
							pkt += 0x34;
						}
					}
				}
			}
			rec += 0x1C;
		} while (--count);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x766090 (module 090 sub_766090): prim-list block "Gouraud-textured quads, UV scroll":
	// 0x24-byte records (code/rgb0 +0, vertex indices +4/+6/+8/+0x0A, uv0+clut +0x0C,
	// uv1+tpage +0x10, uv2 +0x14 / uv3 +0x16, rgb1..3 +0x18/+0x1C/+0x20) -> POLY_GT4 (tag
	// 0x0C000000), RTPT + RTPS for the 4th corner, AVSZ4, same culling / lighting / z / scroll /
	// texture-window framing as 0x765B30. Returns the new packet cursor; ctx+0x2C advances.
	uint32_t __cdecl t_766090(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		static const int32_t U_OFF[4] = { 0x0C, 0x18, 0x24, 0x30 };
		static const int32_t V_OFF[4] = { 0x0D, 0x19, 0x25, 0x31 };
		const uint32_t ctx = a1;
		uint32_t pkt = a4;                              // esi
		const uint32_t list = U32(ctx, 0x2C);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;                        // ebp / [esp+0x30]
		const uint32_t vbase = U32(ctx, 4);             // [esp+0x4c]
		U32(ctx, 0x2C) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			const uint32_t flags = U32(ctx, 0x14);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0xC000000;                    // tag: 12 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;                      // semi-transparent
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			const uint32_t uv0 = U32(rec, 0x0C);
			const uint32_t uv1 = U32(rec, 0x10);
			const uint32_t uv23 = U32(rec, 0x14);
			U32(pkt, 0x0C) = uv0;
			U32(pkt, 0x24) = uv23;
			U32(pkt, 0x18) = uv1;
			U32(pkt, 0x30) = uv23 >> 16;
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (!(U32(ctx, 0x3C) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;                      // [esp+0x40]
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) >= 0 || (U8(ctx, 0x14) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					if (t3_out_x(S16(pkt, 8))) clip = 1;
					if (t3_out_x(S16(pkt, 0x14))) clip |= 2;
					if (t3_out_x(S16(pkt, 0x20))) clip |= 4;
					if (t3_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (t3_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (t3_out_y(S16(pkt, 0x22))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x2C);
					x::GTE_AVSZ4();
					if (t3_out_x(S16(pkt, 0x2C))) clip |= 8;
					if (t3_out_y(S16(pkt, 0x2E))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x38);
						if (U8(ctx, 0x14) & 0x80)
						{
							x::sub_45E120(rec + 0x18, rec + 0x1C, rec + 0x20);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 0x28);
							x::set_unk_1CA8A28(pkt + 4);
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						else
						{
							const uint32_t c1 = U32(rec, 0x18), c2 = U32(rec, 0x1C), c3 = U32(rec, 0x20);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
							U32(pkt, 0x28) = c3;
						}
						int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
						S32(ctx, 0x38) = z;
						if (z < 0)
							S32(ctx, 0x38) = 0;
						z = S32(ctx, 0x38) >> (a3 & 31);
						const uint32_t ot = a2 + (uint32_t)z * 4;   // ebp
						const uint16_t du = U16(ctx, 0x18);
						const uint16_t dv = U16(ctx, 0x1A);
						if ((uint16_t)(du | dv) != 0)
						{
							if (du != 0)
								t3_scroll(pkt, U_OFF, 4, du, U8(ctx, 0x28));
							const uint16_t dv2 = U16(ctx, 0x1A);
							if (dv2 != 0)
								t3_scroll(pkt, V_OFF, 4, dv2, U8(ctx, 0x2A));
							const uint32_t w1 = pkt + 0x34;
							U32(w1, 0) = 0x2000000;
							const uint32_t t1 = (ctx + 0x1C) != 0 ? t3_texwin(ctx + 0x1C) : 0;
							U32(w1, 4) = t1;
							U32(w1, 8) = 0;
							x::SSIGPU_InsertPrimAutoDepth(ot, w1);
							x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
							const uint32_t w2 = pkt + 0x40;
							U32(w2, 0) = 0x2000000;
							const uint32_t t2 = (ctx + 0x24) != 0 ? t3_texwin(ctx + 0x24) : 0;
							U32(w2, 4) = t2;
							U32(w2, 8) = 0;
							x::SSIGPU_InsertPrimAutoDepth(ot, w2);
							pkt += 0x4C;
						}
						else
						{
							const uint32_t dm = pkt + 0x34;
							const uint32_t tp = x::sub_45C690(0, 1, 0, 0);
							x::sub_45BFC0(dm, 0, 0, tp & 0xFFFF, 0);
							x::SSIGPU_InsertPrimAutoDepth(ot, dm);
							x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
							pkt += 0x40;
						}
					}
				}
			}
			rec += 0x24;
		} while (--count);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x766760 (module 090 sub_766760): state "wait": when a_73B7E0(3) is set, or else when
	// a_73C280(node) returns 0, marks the node finished (+0x26 |= 1) and advances the state (+0x29).
	uint32_t __cdecl t_766760(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_73B7E0(3) != 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
			return node;   // eax = node
		}
		const uint32_t r = a_73C280(node);
		if (r == 0)
		{
			const uint8_t st = (uint8_t)(U8(node, 0x29) + 1);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = st;
			return st;     // eax = 0 with al = new state
		}
		return r;
	}

	// 0x7667B0 (module 090 sub_7667B0, TASK): Tonberry prim-model actor - runs state s8 +0x29 of
	// {766EB0, 766F00, 766F20, 766F80, 766FA0}, draws its model (t_766830, scale 0x100), ++frame
	// counter +0x24; ends (releases the linked task, returns 2) when finished with no live child.
	uint32_t __cdecl t_7667B0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x766EB0;
		states[1] = 0x766F00;
		states[2] = 0x766F20;
		states[3] = 0x766F80;
		states[4] = 0x766FA0;
		// update: the state (no bound check, states 0..4)
		callp(states[S8(node, 0x29)], node);
		// draw (after the update)
		t_766830(node, 0x100);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x766830 (module 090 sub_766830): draws the actor's prim model unless hidden (+0x26 bit 2):
	// Field_Alloc(0xE8) block, matrix at +0xB8 = identity (scaled by the s16 a2 unless 0x1000),
	// translation node +0x1C/+0x1E/+0x20, loaded as GTE rot/trans; block +0 = model data node+0x4C,
	// +4 = node+0x50, +0x24 = 0, +0xB4 = node+0x56; renders it with a_7435E0 into OT base+0x44
	// (mode 2) at packet cursor [0x259EEA8] (updated), frees the block.
	uint32_t __cdecl t_766830(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		if (U8(node, 0x26) & 4)
			return 0; // void (hidden)
		const uint32_t blk = x::Field_Alloc(0xE8);
		const uint32_t M = blk + 0xB8;                  // Mat4x3
		x::MAG_022_sub_8DD770(M);
		const uint16_t scale = (uint16_t)a2;
		if (scale != 0x1000)
		{
			const int32_t s = (int16_t)scale;
			S32(blk, 0xE0) = s;
			S32(blk, 0xDC) = s;
			S32(blk, 0xD8) = s;
			x::scale3DMatrix(M, blk + 0xD8);
		}
		const int32_t px = S16(node, 0x1C);
		const int32_t py = S16(node, 0x1E);
		const int32_t pz = S16(node, 0x20);
		S32(blk, 0xCC) = px;
		S32(blk, 0xD0) = py;
		S32(blk, 0xD4) = pz;
		x::GTE_SetRotMatrix(M);
		x::GTE_SetTransVector(M);
		const uint32_t model = U32(node, 0x4C);
		const uint16_t w50 = U16(node, 0x50);
		const uint16_t w56 = U16(node, 0x56);
		U32(blk, 0) = model;
		const uint32_t cursor = MEM<uint32_t>(0x259EEA8);
		U16(blk, 4) = w50;
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = w56;
		MEM<uint32_t>(0x259EEA8) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xE8);
		return 0; // void
	}

	// ====================================================================================
	// part t4
	// ====================================================================================
	// Effect 090 Tonberry, part t4: the prim-model tasks 0x766FB0 / 0x767290 / 0x767C70 (shared draw
	// 0x7672F0), the particle-ring spawner 0x7671D0, the Tonberry CREATURE ACTOR task 0x7674E0 and its
	// 13 state handlers, and the remaining small state handlers of other Tonberry tasks (entity fade
	// 0x768210.., damage 0x768370).
	//
	// Creature actor node (0x7674E0): +0x30 = the battle-model container (+0x30+0x1C = +0x4C/+0x4E/
	// +0x50 world position words, +0x3E facing angle), +0x124/+0x126/+0x128 velocity words (added /16
	// to the position each tick, BEFORE the draw), +0x134 wait counter, +0x136 animation frame,
	// +0x138 animation step counter, +0x13A/+0x13C fade level (0..0x1000), +0x140/+0x144 saved
	// velocity.
	namespace
	{
		// common task tail (status byte read before the frame-counter increment): end (release the
		// linked task, return 2) when finished and no child alive
		inline uint32_t t4_task_end(uint32_t node, uint8_t status)
		{
			if ((status & 1) != 0 && U8(node, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(node);
				return 2;
			}
			return 0;
		}
		inline void t4_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
		// cdq / and edx,2^n-1 / add / sar n  on a sign-extended word = C division toward zero
		inline int16_t t4_div16(int16_t v) { return (int16_t)((int32_t)v / 16); }
		inline int16_t t4_div8(int16_t v) { return (int16_t)((int32_t)v / 8); }
	}

	// 0x766EB0 (module 090 sub_766EB0): prim task state 0 - hides the draw (+0x26 bit2), position
	// (0x40, -0x1C, [[0x1547510]+0xE0]), prim layout 0x1547174, colour word 0xFF00, frame 5, next state.
	uint32_t __cdecl t_766EB0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t src = MEM<uint32_t>(0x1547510);
		const uint16_t z = U16(src, 0xE0);
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 4;
		U16(node, 0x1C) = 0x40;
		U16(node, 0x1E) = 0xFFE4;
		U16(node, 0x20) = z;
		U32(node, 0x4C) = 0x1547174;
		U16(node, 0x56) = 0xFF00;
		U16(node, 0x52) = 5;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x766F00 (module 090 sub_766F00): prim task state 1 - at frame counter >= 0x14 shows the
	// draw (clears +0x26 bit2), loop count +0x74 = 4, next state.
	uint32_t __cdecl t_766F00(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) >= 0x14)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) &= 0xFB;
			U16(node, 0x74) = 4;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x766F20 (module 090 sub_766F20): prim task state 2 - steps the prim animation (a_743C20);
	// when its frame +0x50 passes 4 rewinds it to 3 and counts +0x74 down, next state at 0.
	uint32_t __cdecl t_766F20(uint32_t a1)
	{
		const uint32_t node = a1;
		a_743C20(node);
		if (S16(node, 0x50) > 4)
		{
			U16(node, 0x74) = (uint16_t)(U16(node, 0x74) - 1);
			const int16_t left = S16(node, 0x74);
			U16(node, 0x50) = 3;
			if (left <= 0)
				t4_next_state(node);
		}
		return 0; // void
	}

	// 0x766FB0 (module 090 sub_766FB0): prim-model task (states 0x767020/0x767070/0x767090) with
	// the per-tick helper t_766830(node, 0x100); ends when finished and no child alive.
	uint32_t __cdecl t_766FB0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x767020;
		states[1] = 0x767070;
		states[2] = 0x767090;
		states[3] = 0x7670D0;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		t_766830(node, 0x100);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return t4_task_end(node, status);
	}

	// 0x767020 (module 090 sub_767020): prim task state 0 - hides the draw, position (-0x38, -0x58,
	// [[0x1547510]+0xE0]), prim layout 0x1547258, colour 0xFF00, frame 0, next state.
	uint32_t __cdecl t_767020(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t src = MEM<uint32_t>(0x1547510);
		const uint16_t z = U16(src, 0xE0);
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 4;
		U16(node, 0x1C) = 0xFFC8;
		U16(node, 0x1E) = 0xFFA8;
		U16(node, 0x20) = z;
		U32(node, 0x4C) = 0x1547258;
		U16(node, 0x56) = 0xFF00;
		U16(node, 0x52) = 0;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x767070 (module 090 sub_767070): prim task state 1 - (frame counter >= 0, i.e. at once)
	// shows the draw, rise speed +0x6E = 0x18, next state.
	uint32_t __cdecl t_767070(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) >= 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) &= 0xFB;
			U16(node, 0x6E) = 0x18;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x767090 (module 090 sub_767090): prim task state 2 - y += speed/16; when director flag 7 is
	// set (a_73B7E0) hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl t_767090(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + t4_div16(S16(node, 0x6E)));
		if (a_73B7E0(7) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x767150 (module 090 sub_767150): spawner state - at frame counter >= 0x29 takes the effect
	// origin ([0x1547168] +0/+4) raised by 0x120 in z, ring radius +0x78 = 0x40, index +0x74 = 0,
	// next state.
	uint32_t __cdecl t_767150(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) >= 0x29)
		{
			const uint32_t d = MEM<uint32_t>(0x1547168);
			U16(node, 0x78) = 0x40;
			U16(node, 0x74) = 0;
			const uint32_t xy = U32(d, 0);
			const uint32_t zp = U32(d, 4);
			U32(node, 0x1C) = xy;
			U32(node, 0x20) = zp;
			const uint8_t st = U8(node, 0x29);
			U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + 0x120);
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x767190 (module 090 sub_767190): spawner state - spawns ring particles by the count table
	// 0x154EC70 (t_7671D0, spread 0x20), next state at the table's 0x7F end; radius +0x78 grows
	// by 8 up to 0xC0, index +0x74++.
	uint32_t __cdecl t_767190(uint32_t a1)
	{
		const uint32_t node = a1;
		// argument high half = eax on entry (garbage); t_7671D0 only reads the low word
		if (t_7671D0(node, 0x154EC70, U16(node, 0x74), 0x20) == 0)
			t4_next_state(node);
		U16(node, 0x78) = (uint16_t)(U16(node, 0x78) + 8);
		const int16_t r = S16(node, 0x78);
		U16(node, 0x74) = (uint16_t)(U16(node, 0x74) + 1);
		if (r > 0xC0)
			U16(node, 0x78) = 0xC0;
		return 0; // void
	}

	// 0x7671D0 (module 090 sub_7671D0): reads count = table a2[(s16)a3]; 0x7F -> returns 0 (end);
	// else spawns `count` particle tasks 0x767290 (0x7C bytes, queue 0x259F098) at the spawner
	// position + (sin, cos)(rand&0xFFF) * (radius +0x78 + rand % (s16)a4) in x/z; returns 1.
	uint32_t __cdecl t_7671D0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t spawner = a1;
		const uint16_t count16 = U8(a2 + (uint32_t)(int32_t)(int16_t)a3, 0);
		if (count16 == 0x7F)
			return 0;
		int32_t count = (int32_t)(int16_t)count16;
		if (count > 0)
		{
			do
			{
				const uint32_t p = x::Effect_AddTaskAndInitFromCtx(0x259F098, 0x767290, 0x7C, spawner);
				const uint32_t r1 = x::CrtRand();
				const uint32_t ang = r1 & 0xFFF;
				const uint32_t r2 = x::CrtRand();
				const int32_t rem = (int32_t)r2 % (int32_t)(int16_t)a4;
				const uint32_t xy = U32(spawner, 0x1C);
				const uint32_t zp = U32(spawner, 0x20);
				const int32_t angs = (int32_t)(int16_t)ang;
				U32(p, 0x1C) = xy;
				U32(p, 0x20) = zp;
				const int32_t radius = (int16_t)(uint16_t)((uint32_t)rem + U16(spawner, 0x78));
				const int32_t s = (int32_t)x::computeSin((uint32_t)angs);
				U16(p, 0x1C) = (uint16_t)(U16(p, 0x1C) + (int16_t)(mul32(s, radius) / 4096));
				const int32_t c = (int32_t)x::computeCosine((uint32_t)angs);
				U16(p, 0x20) = (uint16_t)(U16(p, 0x20) + (int16_t)(mul32(c, radius) / 4096));
			} while (--count != 0);
		}
		return 1;
	}

	// 0x767290 (module 090 sub_767290): ring particle task (states 0x767410/0x767430) drawn by
	// t_7672F0; ends when finished and no child alive.
	uint32_t __cdecl t_767290(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x767410;
		states[1] = 0x767430;
		states[2] = 0x767450;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		t_7672F0(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return t4_task_end(node, status);
	}

	// 0x7672F0 (module 090 sub_7672F0): prim draw of a particle node (unless hidden, +0x26 bit2):
	// builds camera rotation * position in a 0xD8-byte scratch block, fills the prim parameter block
	// (layout +0x4C, frame +0x50, colour +0x56) and emits it with a_7435E0 into the effect OT
	// (OT base +0x44) from the module packet pool [0x259EEA8].
	uint32_t __cdecl t_7672F0(uint32_t a1)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0; // void
		const uint32_t blk = x::Field_Alloc(0xD8);
		const uint32_t mat = blk + 0xB8;
		x::UnpackRotationMatrix(0x1D97778, mat);
		const int32_t px = S16(node, 0x1C);
		const int32_t py = S16(node, 0x1E);
		const int32_t pz = S16(node, 0x20);
		const uint32_t tr = blk + 0xCC;
		S32(blk, 0xD0) = py;
		S32(blk, 0xD4) = pz;
		S32(tr, 0) = px;
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(mat);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(mat);
		x::GTE_LoadIRFromMatrixColumn(blk + 0xBA);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0xBA);
		x::GTE_LoadIRFromMatrixColumn(blk + 0xBC);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(blk + 0xBC);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(tr);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(tr);
		x::GTE_SetRotMatrix(mat);
		x::GTE_SetTransVector(mat);
		U32(blk, 0) = U32(node, 0x4C);
		const uint16_t frame = U16(node, 0x50);
		const uint16_t colour = U16(node, 0x56);
		U16(blk, 4) = frame;
		const uint32_t cursor = MEM<uint32_t>(0x259EEA8);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		U16(blk, 0xB4) = colour;
		MEM<uint32_t>(0x259EEA8) = a_7435E0(blk, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x767410 (module 090 sub_767410): particle state 0 - prim layout 0x1547280, colour 0, frame 6,
	// next state.
	uint32_t __cdecl t_767410(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint8_t st = U8(node, 0x29);
		U32(node, 0x4C) = 0x1547280;
		U16(node, 0x56) = 0;
		U16(node, 0x52) = 6;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x767430 (module 090 sub_767430): particle state 1 - steps the prim animation (a_743C20); at
	// its end hides and finishes the task (+0x26 |= 5), next state.
	uint32_t __cdecl t_767430(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x767460 (module 090 sub_767460): spawner state - at frame counter >= 0x52: radius +0x78 =
	// 0x80, index +0x74 = 0, next state.
	uint32_t __cdecl t_767460(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) >= 0x52)
		{
			const uint8_t st = U8(node, 0x29);
			U16(node, 0x78) = 0x80;
			U16(node, 0x74) = 0;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x767480 (module 090 sub_767480): spawner state - lowers z by 0x10 on frames 0x5C..0x64,
	// spawns ring particles by the count table 0x154EC90 (spread 0x80); at the table end finishes
	// the task (+0x26 |= 1), next state; index +0x74++.
	uint32_t __cdecl t_767480(uint32_t a1)
	{
		const uint32_t node = a1;
		const int16_t f = S16(node, 0x24);
		if (f >= 0x5C && f <= 0x64)
			U16(node, 0x20) = (uint16_t)(U16(node, 0x20) - 0x10);
		// argument high half = eax (frame counter read, high half from entry); only the low word is read
		if (t_7671D0(node, 0x154EC90, U16(node, 0x74), 0x80) == 0)
		{
			U8(node, 0x26) |= 1;
			t4_next_state(node);
		}
		U16(node, 0x74) = (uint16_t)(U16(node, 0x74) + 1);
		return 0; // void
	}

	// 0x7674E0 (module 090 GF_090Tonberry_CreatureActorTask): Tonberry creature actor - runs the
	// state handler (14 states), moves the model position (+0x4C/+0x4E/+0x50) by velocity/16
	// (+0x124/+0x126/+0x128), then (unless hidden, +0x26 bit2) draws the model with a_746C10 into
	// the module packet pool [0x259EEA8] and publishes the model's bone-0x18 position to 0x25A4C10.
	uint32_t __cdecl t_7674E0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[14];
		states[0] = 0x7677F0;
		states[1] = 0x767870;
		states[2] = 0x7679D0;
		states[3] = 0x767A40;
		states[4] = 0x767DF0;
		states[5] = 0x767E20;
		states[6] = 0x767E60;
		states[7] = 0x767F20;
		states[8] = 0x767FE0;
		states[9] = 0x768030;
		states[10] = 0x768080;
		states[11] = 0x7680B0;
		states[12] = 0x7680D0;
		states[13] = 0x7681A0;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		// UPDATE: position += velocity / 16 (before the draw)
		const uint32_t model = node + 0x30;
		U16(model, 0x1C) = (uint16_t)(U16(model, 0x1C) + t4_div16(S16(node, 0x124)));
		U16(model, 0x1E) = (uint16_t)(U16(model, 0x1E) + t4_div16(S16(node, 0x126)));
		U16(model, 0x20) = (uint16_t)(U16(model, 0x20) + t4_div16(S16(node, 0x128)));
		// DRAW
		if ((U8(node, 0x26) & 4) == 0)
		{
			const uint32_t cursor = MEM<uint32_t>(0x259EEA8);
			const uint32_t next = a_746C10(node, 0x25A2D00, cursor);
			MEM<uint32_t>(0x259EEA8) = next;
			x::GetEffectSpawnPosition(model, 0x18, 0x9FC, 0x25A4C10);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return t4_task_end(node, status);
	}

	// 0x7677F0 (module 090 sub_7677F0): creature state 0 - hidden, model flag 0x20, position =
	// effect origin ([0x1547168]), scale 0x1000^3 at +0x114 (pointer +0x60), [0x25A2B78] = 0,
	// starts animation 3, plays SE 0x154716C and stream 0x16D7DE8, next state.
	uint32_t __cdecl t_7677F0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t d = MEM<uint32_t>(0x1547168);
		const uint32_t xy = U32(d, 0);
		const uint32_t zp = U32(d, 4);
		U8(node, 0x26) |= 4;
		U8(node, 0x30) |= 0x20;
		U32(node, 0x4C) = xy;
		U32(node, 0x50) = zp;
		MEM<uint16_t>(0x25A2B78) = 0;
		U32(node, 0x60) = node + 0x114;
		U32(node, 0x11C) = 0x1000;
		U32(node, 0x118) = 0x1000;
		U32(node, 0x114) = 0x1000;
		x::au_re_Battle_ReadAnimation_7(node, 3);
		x::BdPlaySE(0x154716C, 0, 0x80);
		xm::BdPlayStream(0x16D7DE8, 0x80, 1, 0x7F);
		t4_next_state(node);
		return 0; // void
	}

	// 0x767870 (module 090 sub_767870): creature state 1 - at frame counter >= 0x1C shows the model
	// with fade level 0x1000, next state; applies the fade (+0x13C = +0x13A, t_7678B0 mode 0).
	uint32_t __cdecl t_767870(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) >= 0x1C)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) &= 0xFB;
			U16(node, 0x13A) = 0x1000;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		U16(node, 0x13C) = U16(node, 0x13A);
		t_7678B0(node, 0);
		return 0; // void
	}

	// 0x7678B0 (module 090 sub_7678B0): applies the fade level +0x13A to the model container
	// (node+0x30) with mode a2 (a_733950).
	uint32_t __cdecl t_7678B0(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		// the level argument's high half is ecx garbage; a_733950 only reads the low word
		a_733950(node + 0x30, U16(node, 0x13A), a2);
		return 0; // void
	}

	// 0x7679D0 (module 090 sub_7679D0): creature state 2 - fades +0x13A down by 0x155 per tick
	// (floor 0); at frame counter >= 0x28 sets it 0, starts animation 0, next state; applies the
	// fade (mode 0).
	uint32_t __cdecl t_7679D0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) >= 0x28)
		{
			U16(node, 0x13A) = 0;
			x::au_re_Battle_ReadAnimation_7(node, 0);
			t4_next_state(node);
		}
		else
		{
			U16(node, 0x13A) = (uint16_t)(U16(node, 0x13A) + 0xFEAB);
			if (S16(node, 0x13A) < 0)
				U16(node, 0x13A) = 0;
		}
		U16(node, 0x13C) = U16(node, 0x13A);
		t_7678B0(node, 0);
		return 0; // void
	}

	// 0x767A40 (module 090 sub_767A40): creature state 3 (walk) - on animation frames 0x4D..0x54
	// spawns a spark task 0x767C70 (queue 0x259F098) at a random bone position (+-0x80 jitter);
	// at frame 0x5A sets [0x25A2B78] = 0xFF00 and starts sub-effect 0x767B40 (t_763E60); at the
	// animation end sets velocity +0x124 = facing(+0x3E) * -0x130 (t_764020), starts animation 1,
	// next state.
	uint32_t __cdecl t_767A40(uint32_t a1)
	{
		const uint32_t node = a1;
		const int16_t af = S16(node, 0x136);
		if (af >= 0x4D && af <= 0x54)
		{
			const uint32_t r0 = x::CrtRand();
			const uint32_t bone = r0 & 0xFFF;
			const uint32_t p = x::Effect_AddTaskAndInitFromCtx(0x259F098, 0x767C70, 0x7C, node);
			const uint32_t dst = p + 0x1C;
			const uint32_t r1 = x::CrtRand();
			U16(dst, 0) = (uint16_t)((r1 & 0xFF) - 0x80);
			const uint32_t r2 = x::CrtRand();
			U16(p, 0x1E) = (uint16_t)((r2 & 0xFF) - 0x80);
			const uint32_t r3 = x::CrtRand();
			U16(p, 0x20) = (uint16_t)((r3 & 0xFF) - 0x80);
			x::GetEffectSpawnPosition(node + 0x30, 0xD, (uint32_t)(int32_t)(int16_t)bone, dst);
		}
		if (U16(node, 0x136) == 0x5A)
		{
			MEM<uint16_t>(0x25A2B78) = 0xFF00;
			t_763E60(node, 0x767B40, 0x16F0F40, 0x14, 1, 0);
		}
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			// `mov ax, [esi+0x3E]` with eax = 1 (the result) -> high half 0
			t_764020(U16(node, 0x3E), 0xFFFFFED0, node + 0x124);
			x::au_re_Battle_ReadAnimation_7(node, 1);
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x767BA0 (module 090 sub_767BA0): spark state 0 - decodes the prim layout (+0x74, +0x78) into
	// +0x94, y 0, scale 0x1000^3 (+0x50..+0x58), position = [0x25A4C10..] (creature bone position),
	// colour +0x88 = [0x25A2B78], first step (a_73C280), next state.
	uint32_t __cdecl t_767BA0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t lay_b = U32(node, 0x78);
		const uint32_t lay_a = U32(node, 0x74);
		x::Effect_DecodeModelPrimLayout(lay_a, node + 0x94, lay_b);
		const uint32_t zp = MEM<uint32_t>(0x25A4C14);
		const uint16_t col = MEM<uint16_t>(0x25A2B78);
		U16(node, 0x1E) = 0;
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		U32(node, 0x1C) = MEM<uint32_t>(0x25A4C10);  // overwrites the +0x1E store above
		U32(node, 0x20) = zp;
		U16(node, 0x88) = col;
		a_73C280(node);
		t4_next_state(node);
		return 0; // void
	}

	// 0x767C00 (module 090 sub_767C00): spark state 1 - follows [0x25A4C10..] and colour
	// [0x25A2B78]; finishes (+0x26 |= 1, next state) when the position word is 0x7FFF (creature
	// gone) and again when the prim step a_73C280 returns 0.
	uint32_t __cdecl t_767C00(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t xy = MEM<uint32_t>(0x25A4C10);
		const uint32_t zp = MEM<uint32_t>(0x25A4C14);
		const uint16_t col = MEM<uint16_t>(0x25A2B78);
		U32(node, 0x1C) = xy;
		U32(node, 0x20) = zp;
		U16(node, 0x88) = col;
		if ((uint16_t)xy == 0x7FFF)
		{
			U8(node, 0x26) |= 1;
			t4_next_state(node);
		}
		if (a_73C280(node) == 0)
		{
			U8(node, 0x26) |= 1;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x767C70 (module 090 sub_767C70): spark task (states 0x767CD0/0x767D40) drawn by t_7672F0;
	// ends when finished and no child alive.
	uint32_t __cdecl t_767C70(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x767CD0;
		states[1] = 0x767D40;
		states[2] = 0x767DE0;  // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		t_7672F0(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return t4_task_end(node, status);
	}

	// 0x767CD0 (module 090 sub_767CD0): spark state 0 - prim layout 0x1547344, colour 0, frame 9,
	// random velocity x = rand%0x300 - 0x180, y = -0x280 - rand%0x300, z = rand%0x300 + 0x280,
	// next state.
	uint32_t __cdecl t_767CD0(uint32_t a1)
	{
		const uint32_t node = a1;
		U32(node, 0x4C) = 0x1547344;
		U16(node, 0x56) = 0;
		U16(node, 0x52) = 9;
		const uint32_t r1 = x::CrtRand();
		U16(node, 0x6C) = (uint16_t)((int32_t)r1 % 0x300 - 0x180);
		const uint32_t r2 = x::CrtRand();
		U16(node, 0x6E) = (uint16_t)(0xFFFFFD80u - (uint32_t)((int32_t)r2 % 0x300));
		const uint32_t r3 = x::CrtRand();
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x70) = (uint16_t)((int32_t)r3 % 0x300 + 0x280);
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x767D40 (module 090 sub_767D40): spark state 1 - velocity damped by 1/8 per tick (y also
	// pulled down by 0x60), position += velocity/16; steps the prim animation (a_743C20), at its
	// end hides and finishes (+0x26 |= 5), next state.
	uint32_t __cdecl t_767D40(uint32_t a1)
	{
		const uint32_t node = a1;
		uint16_t vx = U16(node, 0x6C);
		uint16_t vy = U16(node, 0x6E);
		vy = (uint16_t)(vy + 0x60);
		uint16_t vz = U16(node, 0x70);
		vx = (uint16_t)(vx - t4_div8((int16_t)vx));
		U16(node, 0x6C) = vx;
		vy = (uint16_t)(vy - t4_div8((int16_t)vy));
		U16(node, 0x6E) = vy;
		vz = (uint16_t)(vz - t4_div8((int16_t)vz));
		U16(node, 0x70) = vz;
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + t4_div16((int16_t)vx));
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + t4_div16((int16_t)vy));
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + t4_div16((int16_t)vz));
		if (a_743C20(node) != 0)
		{
			U8(node, 0x26) |= 5;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x767DF0 (module 090 sub_767DF0): creature state 4 - steps the animation (0x8DD1C0); when
	// director flag 3 is set (a_73B7E0) clears model flag 0x20, next state.
	uint32_t __cdecl t_767DF0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (a_73B7E0(3) != 0)
		{
			U16(node, 0x30) &= 0xFFDF;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x767E20 (module 090 sub_767E20): creature state 5 - steps the animation; after 3 animation
	// steps (+0x138) saves the velocity (+0x124..+0x12B -> +0x140..+0x147), next state.
	uint32_t __cdecl t_767E20(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (S16(node, 0x138) >= 3)
		{
			const uint32_t v0 = U32(node, 0x124);
			const uint32_t v1 = U32(node, 0x128);
			U32(node, 0x140) = v0;
			U32(node, 0x144) = v1;
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x767E60 (module 090 sub_767E60): creature state 6 - waits for director flag 6 (a_73B640):
	// until then the actor stands (a_73BB60(1), velocity 0); then steps the animation,
	// a_73BB60(0), restores the saved velocity (+0x140 -> +0x124), next state.
	uint32_t __cdecl t_767E60(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_73B640(6) != 0)
		{
			x::sub_8DD1C0(node);
			a_73BB60(0);
			const uint32_t v0 = U32(node, 0x140);
			const uint32_t v1 = U32(node, 0x144);
			U32(node, 0x124) = v0;
			const uint8_t st = U8(node, 0x29);
			U32(node, 0x128) = v1;
			U8(node, 0x29) = (uint8_t)(st + 1);
			return 0; // void
		}
		a_73BB60(1);
		U16(node, 0x128) = 0;
		U16(node, 0x126) = 0;
		U16(node, 0x124) = 0;
		return 0; // void
	}

	// 0x767F20 (module 090 sub_767F20): creature state 7 - steps the animation; when director flag 6
	// is set (a_73B7E0): position = effect target point ([0x1547168] +8/+0xC), facing +0x3E =
	// [+0x4A], velocity +0x124 = facing * -0x130, then backs the position off by facing * -0x180
	// and facing * -0x17C (t_764020), next state.
	uint32_t __cdecl t_767F20(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (a_73B7E0(6) == 0)
			return 0; // void
		const uint32_t d = MEM<uint32_t>(0x1547168);
		const uint32_t xy = U32(d, 8);
		const uint16_t facing = U16(d, 0x4A);
		const uint32_t zp = U32(d, 0xC);
		U32(node, 0x4C) = xy;
		U16(node, 0x3E) = facing;
		U32(node, 0x50) = zp;
		// argument high halves (ecx / eax) are left-over callee garbage; t_764020 reads the low word
		t_764020(facing, 0xFFFFFED0, node + 0x124);
		int16_t off[4] = {0, 0, 0, 0};  // 8-byte stack local [esp+4] (words 0..2 written and read)
		t_764020(U16(node, 0x3E), 0xFFFFFE80, P(off));
		U16(node, 0x4C) = (uint16_t)(U16(node, 0x4C) - (uint16_t)off[0]);
		U16(node, 0x4E) = (uint16_t)(U16(node, 0x4E) - (uint16_t)off[1]);
		const uint16_t facing2 = U16(node, 0x3E);
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) - (uint16_t)off[2]);
		t_764020(facing2, 0xFFFFFE84, P(off));
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) - (uint16_t)off[2]);
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x4C) = (uint16_t)(U16(node, 0x4C) - (uint16_t)off[0]);
		U16(node, 0x4E) = (uint16_t)(U16(node, 0x4E) - (uint16_t)off[1]);
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x767FE0 (module 090 sub_767FE0): creature state 8 - steps the animation; after 4 steps
	// stops (velocity 0), starts animation 2, next state.
	uint32_t __cdecl t_767FE0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (S16(node, 0x138) >= 4)
		{
			U16(node, 0x128) = 0;
			U16(node, 0x126) = 0;
			U16(node, 0x124) = 0;
			x::au_re_Battle_ReadAnimation_7(node, 2);
			t4_next_state(node);
		}
		return 0; // void
	}

	// 0x768030 (module 090 sub_768030): creature state 9 (knife stab) - at animation frame 0x1E
	// plays the summon stream (0x5018C0); at the animation end wait +0x134 = 0x3C, next state.
	uint32_t __cdecl t_768030(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U16(node, 0x136) == 0x1E)
			x::BdPlaySummonStream(0x80, 1, 0x7F);
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			const uint8_t st = U8(node, 0x29);
			U16(node, 0x134) = 0x3C;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x768080 (module 090 sub_768080): creature state 10 - when director flag 8 is set
	// (a_73B640) wait +0x134 = 0xA, next state.
	uint32_t __cdecl t_768080(uint32_t a1)
	{
		if (a_73B640(8) != 0)
		{
			const uint32_t node = a1;
			const uint8_t st = U8(node, 0x29);
			U16(node, 0x134) = 0xA;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x7680B0 (module 090 sub_7680B0): creature state 11 - counts +0x134 down, next state at 0.
	uint32_t __cdecl t_7680B0(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x134) = (uint16_t)(U16(node, 0x134) - 1);
		if (S16(node, 0x134) <= 0)
			t4_next_state(node);
		return 0; // void
	}

	// 0x7680D0 (module 090 sub_7680D0): creature state 12 - fades out: +0x13A += 0x100; at 0x1000
	// hides and finishes (+0x26 |= 5), [0x25A4C10] = 0x7FFF (sparks stop), next state; applies the
	// fade (mode 1) and the colour darkening (t_768130).
	uint32_t __cdecl t_7680D0(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x13A) = (uint16_t)(U16(node, 0x13A) + 0x100);
		if (S16(node, 0x13A) >= 0x1000)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 5;
			U16(node, 0x13A) = 0x1000;
			MEM<uint16_t>(0x25A4C10) = 0x7FFF;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		U16(node, 0x13C) = U16(node, 0x13A);
		t_7678B0(node, 1);
		t_768130(node);
		return 0; // void
	}

	// 0x768130 (module 090 sub_768130): model colour +0x5C..+0x5E = the battle ambient colour bytes
	// [0xB8B9A8..0xB8B9AA] each reduced by (byte * level +0x13C) / 4096.
	uint32_t __cdecl t_768130(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t amb = MEM<uint32_t>(0xB8B9A8);
		const int32_t level = S16(node, 0x13C);
		const uint8_t c0 = (uint8_t)(amb & 0xFF);
		const uint8_t c1 = (uint8_t)((amb >> 8) & 0xFF);
		const int32_t d0 = mul32((int32_t)c0, level) / 4096;
		U8(node, 0x5C) = (uint8_t)(c0 - (uint8_t)d0);
		const int32_t d1 = mul32((int32_t)c1, level) / 4096;
		U8(node, 0x5D) = (uint8_t)(c1 - (uint8_t)d1);
		const uint8_t c2 = MEM<uint8_t>(0xB8B9AA);
		const int32_t d2 = mul32((int32_t)c2, level) / 4096;
		U8(node, 0x5E) = (uint8_t)(c2 - (uint8_t)d2);
		return 0; // void
	}

	// 0x768210 (module 090 sub_768210): entity-fade state 0 - level +0x76 = 0; for the 4 battle
	// entities (stride 0x2C) clears the level word 0x1D98992 and the three colour bytes
	// 0x1D989B8..0x1D989BA, next state.
	uint32_t __cdecl t_768210(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x76) = 0;
		uint32_t p = 0x1D989BA;
		for (int k = 4; k != 0; --k)
		{
			MEM<uint16_t>(p - 0x28) = 0;
			MEM<uint8_t>(p) = 0;
			MEM<uint8_t>(p - 1) = 0;
			MEM<uint8_t>(p - 2) = 0;
			p += 0x2C;
		}
		t4_next_state(node);
		return 0; // void
	}

	// 0x768250 (module 090 sub_768250): entity-fade state 1 - level +0x76 += 0x100 up to 0x400
	// (then finishes, +0x26 |= 1, next state), written to the 4 entity level words 0x1D98992+k*0x2C.
	uint32_t __cdecl t_768250(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x76) = (uint16_t)(U16(node, 0x76) + 0x100);
		if (S16(node, 0x76) >= 0x400)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U16(node, 0x76) = 0x400;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(node, 0x76);
		uint32_t p = 0x1D98992;
		for (int k = 4; k != 0; --k)
		{
			MEM<uint16_t>(p) = level;
			p += 0x2C;
		}
		return 0; // void
	}

	// 0x768300 (module 090 sub_768300): entity-fade state - at frame counter >= 0x1E level +0x76 =
	// 0x400, next state.
	uint32_t __cdecl t_768300(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) >= 0x1E)
		{
			const uint8_t st = U8(node, 0x29);
			U16(node, 0x76) = 0x400;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x768320 (module 090 sub_768320): entity-fade state - level +0x76 -= 0x100 down to 0 (then
	// finishes, +0x26 |= 1, next state), written to the 4 entity level words 0x1D98992+k*0x2C.
	uint32_t __cdecl t_768320(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x76) = (uint16_t)(U16(node, 0x76) + 0xFF00);
		if (S16(node, 0x76) <= 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U16(node, 0x76) = 0;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(node, 0x76);
		uint32_t p = 0x1D98992;
		for (int k = 4; k != 0; --k)
		{
			MEM<uint16_t>(p) = level;
			p += 0x2C;
		}
		return 0; // void
	}

	// 0x768370 (module 090 MAG_090_sub_768370): damage state - when [[0x1547168]+0x48] == 1 applies
	// the action result of (action +0x2A, subtarget +0x2B) of the cast context (0x506690), next state.
	uint32_t __cdecl t_768370(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1547168);
		if (U16(d, 0x48) != 1)
			return 0; // void
		const uint32_t node = a1;
		const int32_t act = S8(node, 0x2A);
		const uint32_t ctx = U32(node, 0xC);
		const int32_t sub = S8(node, 0x2B);
		const uint32_t results = U32(ctx, 4);
		const uint32_t arr = U32(results + (uint32_t)(act * 20), 8);
		x::ApplyActionResultToTarget(arr + (uint32_t)(sub * 24));
		t4_next_state(node);
		return 0; // void
	}

namespace tonberry
{
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort PORTS[] = {
		{ 0x7624D0, (void *)t_7624D0, "090 MAG_090_sub_7624D0" },
		{ 0x762650, (void *)t_762650, "090 MAG_090_sub_762650" },
		{ 0x762710, (void *)t_762710, "090 MAG_090_sub_762710" },
		{ 0x762BB0, (void *)t_762BB0, "090 MAG_090_sub_762BB0" },
		{ 0x762C60, (void *)t_762C60, "090 MAG_090_sub_762C60" },
		{ 0x762D10, (void *)t_762D10, "090 sub_762D10" },
		{ 0x762D60, (void *)t_762D60, "090 sub_762D60" },
		{ 0x762E60, (void *)t_762E60, "090 sub_762E60" },
		{ 0x762E90, (void *)t_762E90, "090 sub_762E90" },
		{ 0x763690, (void *)t_763690, "090 sub_763690" },
		{ 0x763790, (void *)t_763790, "090 sub_763790" },
		{ 0x763970, (void *)t_763970, "090 sub_763970" },
		{ 0x7639E0, (void *)t_7639E0, "090 sub_7639E0" },
		{ 0x763A30, (void *)t_763A30, "090 sub_763A30" },
		{ 0x763AA0, (void *)t_763AA0, "090 sub_763AA0" },
		{ 0x763AC0, (void *)t_763AC0, "090 sub_763AC0" },
		{ 0x763B10, (void *)t_763B10, "090 sub_763B10" },
		{ 0x763B90, (void *)t_763B90, "090 sub_763B90" },
		{ 0x763BD0, (void *)t_763BD0, "090 sub_763BD0" },
		{ 0x763C90, (void *)t_763C90, "090 sub_763C90" },
		{ 0x763CC0, (void *)t_763CC0, "090 sub_763CC0" },
		{ 0x763CF0, (void *)t_763CF0, "090 sub_763CF0" },
		{ 0x763D60, (void *)t_763D60, "090 MAG_090_SpawnTonberryCreature" },
		{ 0x763E60, (void *)t_763E60, "090 sub_763E60" },
		{ 0x763EB0, (void *)t_763EB0, "090 sub_763EB0" },
		{ 0x763F80, (void *)t_763F80, "090 sub_763F80" },
		{ 0x764020, (void *)t_764020, "090 sub_764020" },
		{ 0x764070, (void *)t_764070, "090 sub_764070" },
		{ 0x764100, (void *)t_764100, "090 sub_764100" },
		{ 0x764260, (void *)t_764260, "090 sub_764260" },
		{ 0x764540, (void *)t_764540, "090 sub_764540" },
		{ 0x764670, (void *)t_764670, "090 sub_764670" },
		{ 0x764890, (void *)t_764890, "090 sub_764890" },
		{ 0x764B20, (void *)t_764B20, "090 sub_764B20" },
		{ 0x765010, (void *)t_765010, "090 sub_765010" },
		{ 0x765B30, (void *)t_765B30, "090 sub_765B30" },
		{ 0x766090, (void *)t_766090, "090 sub_766090" },
		{ 0x766760, (void *)t_766760, "090 sub_766760" },
		{ 0x7667B0, (void *)t_7667B0, "090 sub_7667B0" },
		{ 0x766830, (void *)t_766830, "090 sub_766830" },
		{ 0x766EB0, (void *)t_766EB0, "090 sub_766EB0" },
		{ 0x766F00, (void *)t_766F00, "090 sub_766F00" },
		{ 0x766F20, (void *)t_766F20, "090 sub_766F20" },
		{ 0x766FB0, (void *)t_766FB0, "090 sub_766FB0" },
		{ 0x767020, (void *)t_767020, "090 sub_767020" },
		{ 0x767070, (void *)t_767070, "090 sub_767070" },
		{ 0x767090, (void *)t_767090, "090 sub_767090" },
		{ 0x767150, (void *)t_767150, "090 sub_767150" },
		{ 0x767190, (void *)t_767190, "090 sub_767190" },
		{ 0x7671D0, (void *)t_7671D0, "090 sub_7671D0" },
		{ 0x767290, (void *)t_767290, "090 sub_767290" },
		{ 0x7672F0, (void *)t_7672F0, "090 sub_7672F0" },
		{ 0x767410, (void *)t_767410, "090 sub_767410" },
		{ 0x767430, (void *)t_767430, "090 sub_767430" },
		{ 0x767460, (void *)t_767460, "090 sub_767460" },
		{ 0x767480, (void *)t_767480, "090 sub_767480" },
		{ 0x7674E0, (void *)t_7674E0, "090 GF_090Tonberry_CreatureActorTask" },
		{ 0x7677F0, (void *)t_7677F0, "090 sub_7677F0" },
		{ 0x767870, (void *)t_767870, "090 sub_767870" },
		{ 0x7678B0, (void *)t_7678B0, "090 sub_7678B0" },
		{ 0x7679D0, (void *)t_7679D0, "090 sub_7679D0" },
		{ 0x767A40, (void *)t_767A40, "090 sub_767A40" },
		{ 0x767BA0, (void *)t_767BA0, "090 sub_767BA0" },
		{ 0x767C00, (void *)t_767C00, "090 sub_767C00" },
		{ 0x767C70, (void *)t_767C70, "090 sub_767C70" },
		{ 0x767CD0, (void *)t_767CD0, "090 sub_767CD0" },
		{ 0x767D40, (void *)t_767D40, "090 sub_767D40" },
		{ 0x767DF0, (void *)t_767DF0, "090 sub_767DF0" },
		{ 0x767E20, (void *)t_767E20, "090 sub_767E20" },
		{ 0x767E60, (void *)t_767E60, "090 sub_767E60" },
		{ 0x767F20, (void *)t_767F20, "090 sub_767F20" },
		{ 0x767FE0, (void *)t_767FE0, "090 sub_767FE0" },
		{ 0x768030, (void *)t_768030, "090 sub_768030" },
		{ 0x768080, (void *)t_768080, "090 sub_768080" },
		{ 0x7680B0, (void *)t_7680B0, "090 sub_7680B0" },
		{ 0x7680D0, (void *)t_7680D0, "090 sub_7680D0" },
		{ 0x768130, (void *)t_768130, "090 sub_768130" },
		{ 0x768210, (void *)t_768210, "090 sub_768210" },
		{ 0x768250, (void *)t_768250, "090 sub_768250" },
		{ 0x768300, (void *)t_768300, "090 sub_768300" },
		{ 0x768320, (void *)t_768320, "090 sub_768320" },
		{ 0x768370, (void *)t_768370, "090 MAG_090_sub_768370" },
		{ 0, nullptr, nullptr }
	};
	// tasks whose drawing the held frame redraws: the creature actor (0x7674E0) and the two
	// prim-model tasks (0x763EB0, 0x767B40 - every packet they emit comes from the prim-model
	// draw callback 0x764260, so prim::play_held redraws all of it)
	static const uint32_t HELD_TASKS[] = { 0x7674E0, 0x763EB0, 0x767B40, 0 };
	static bool is_held(uint32_t a) { for (const uint32_t *h = HELD_TASKS; *h; h++) if (*h == a) return true; return false; }
	// the creature walks: every tick 0x7674E0 adds velocity/16 (s16 +0x124/+0x126/+0x128) to its
	// position (+0x4C/+0x4E/+0x50) before drawing; the held frame adds the same step scaled by
	// num/den (the velocity of the next tick is taken to be the current one)
	static void Adjust(uint32_t node, int num, int den)
	{
		for (int i = 0; i < 3; i++)
		{
			const int32_t step = (int32_t)S16(node, 0x124 + 2 * i) / 16;
			S16(node, 0x4C + 2 * i) = (int16_t)(S16(node, 0x4C + 2 * i) + step * num / den);
		}
	}
	// module globals 0x259EEA8..0x25A4E00 (task pools, packet pools, arenas, script/camera state)
	static const HeldDesc HELD = { &MOD_090, 0x25A3F90, 0x7674E0, 0x25A2D00, 0x259EEA8, 0x25A4E00, nullptr, Adjust };
	static bool HeldReady() { return held_ready(); }
	static void HeldFrame(int num, int den) { held_frame(HELD, num, den); }
	static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		const Mod *m = g_mod;
		g_mod = &MOD_090;
		bool r = held_camera(num, den, world, lookat);
		g_mod = m;
		return r;
	}
}
}
	void register_mag090_tonberry()
	{
		act::register_module(90, act::tonberry::HELD_TASKS);
		for (const act::tonberry::ModPort *p = act::tonberry::PORTS; p->addr; p++)
			act::register_module_port(90, p->addr, p->port, p->name, act::tonberry::is_held(p->addr));
		register_module_held(90, act::tonberry::HeldReady, act::tonberry::HeldFrame);
		register_module_camera(90, act::tonberry::HeldCamera);
	}
}
