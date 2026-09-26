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

// Effect 095: Siren - Silent Voice (actor state-machine GF family, see act_engine.h).
//
// Setup 0x739DA0 (runs once, not ported) creates the root queue 0x257FA80 with the master
// (engine port a_739F40) and nine task pools. The master runs the module's state table
// (arenas, asset table, load script 0x1533850, director), then every pool each tick.
// Choreography: the director (s_73A7B0) steps the "cut" script at [0x1533010]; each new cut
// spawns its tasks (s_73BCF0): cut 1 wave model, 2 notes model + sparkles, 3 the creature actor
// (s_746B60, pose via 0x8DD1C0/0x8DD220, drawn by a_746C10) with the harp models and particle
// actors, 4 water surfaces (s_745AD0) + orbit sprites, 6 target sparks, 7 per-target particle
// systems, 8 the end task. The camera script task (s_73A9E0) drives the battle camera through
// the engine's keyed moves (a_73AB20/a_73AE10) and channel script (a_73B3B0/a_73B4B0).
// Module globals: 0x257F8A0..0x258FCF4 (+ camera copy 0x2793E58, 0x20 bytes).

#include "act_engine.h"

#ifdef FF8_FX_HELD
#include "mag095_siren_held.h"
#endif

namespace ff8fx
{
namespace act
{
	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl s_73A0F0(uint32_t a1); // 0x73A0F0 MAG_095_sub_73A0F0, 15 insns
	uint32_t __cdecl s_73A130(void); // 0x73A130 MAG_095_sub_73A130, 15 insns
	uint32_t __cdecl s_73A220(uint32_t a1); // 0x73A220 MAG_095_sub_73A220, 21 insns
	uint32_t __cdecl s_73A270(void); // 0x73A270 MAG_095_sub_73A270, 41 insns
	uint32_t __cdecl s_73A6E0(void); // 0x73A6E0 MAG_095_sub_73A6E0, 15 insns
	uint32_t __cdecl s_73A7B0(uint32_t a1); // 0x73A7B0 MAG_095_sub_73A7B0 TASK, 55 insns
	uint32_t __cdecl s_73A8E0(uint32_t a1); // 0x73A8E0 sub_73A8E0, 22 insns
	uint32_t __cdecl s_73A990(uint32_t a1); // 0x73A990 sub_73A990, 21 insns
	uint32_t __cdecl s_73A9E0(uint32_t a1); // 0x73A9E0 sub_73A9E0 TASK, 40 insns
	uint32_t __cdecl s_73AAA0(uint32_t a1); // 0x73AAA0 sub_73AAA0, 16 insns
	uint32_t __cdecl s_73B2F0(uint32_t a1); // 0x73B2F0 sub_73B2F0, 15 insns
	uint32_t __cdecl s_73B320(uint32_t a1); // 0x73B320 sub_73B320, 15 insns
	uint32_t __cdecl s_73B380(uint32_t a1); // 0x73B380 sub_73B380, 12 insns
	uint32_t __cdecl s_73B480(uint32_t a1); // 0x73B480 sub_73B480, 9 insns
	uint32_t __cdecl s_73B670(uint32_t a1); // 0x73B670 sub_73B670, 17 insns
	uint32_t __cdecl s_73B6B0(uint32_t a1); // 0x73B6B0 sub_73B6B0, 7 insns
	uint32_t __cdecl s_73B6D0(uint32_t a1); // 0x73B6D0 sub_73B6D0, 9 insns
	uint32_t __cdecl s_73B6F0(uint32_t a1); // 0x73B6F0 sub_73B6F0, 16 insns
	uint32_t __cdecl s_73B730(uint32_t a1); // 0x73B730 sub_73B730, 9 insns
	uint32_t __cdecl s_73B7B0(uint32_t a1); // 0x73B7B0 sub_73B7B0, 13 insns
	uint32_t __cdecl s_73B800(uint32_t a1); // 0x73B800 sub_73B800, 16 insns
	uint32_t __cdecl s_73B840(uint32_t a1); // 0x73B840 sub_73B840, 16 insns
	uint32_t __cdecl s_73B880(uint32_t a1); // 0x73B880 sub_73B880, 6 insns
	uint32_t __cdecl s_73B900(uint32_t a1); // 0x73B900 sub_73B900, 19 insns
	uint32_t __cdecl s_73B950(uint32_t a1); // 0x73B950 sub_73B950, 22 insns
	uint32_t __cdecl s_73B9A0(uint32_t a1); // 0x73B9A0 sub_73B9A0, 13 insns
	uint32_t __cdecl s_73B9D0(uint32_t a1); // 0x73B9D0 sub_73B9D0, 9 insns
	uint32_t __cdecl s_73BA30(uint32_t a1); // 0x73BA30 sub_73BA30, 6 insns
	uint32_t __cdecl s_73BA50(uint32_t a1); // 0x73BA50 sub_73BA50, 8 insns
	uint32_t __cdecl s_73BA70(uint32_t a1); // 0x73BA70 sub_73BA70, 9 insns
	uint32_t __cdecl s_73BAD0(uint32_t a1); // 0x73BAD0 sub_73BAD0, 13 insns
	uint32_t __cdecl s_73BB20(uint32_t a1); // 0x73BB20 sub_73BB20, 16 insns
	uint32_t __cdecl s_73BBB0(uint32_t a1); // 0x73BBB0 sub_73BBB0, 6 insns
	uint32_t __cdecl s_73BBF0(uint32_t a1); // 0x73BBF0 sub_73BBF0, 8 insns
	uint32_t __cdecl s_73BC10(uint32_t a1); // 0x73BC10 sub_73BC10, 11 insns
	uint32_t __cdecl s_73BC60(uint32_t a1); // 0x73BC60 sub_73BC60, 13 insns
	uint32_t __cdecl s_73BC90(uint32_t a1); // 0x73BC90 sub_73BC90, 8 insns
	uint32_t __cdecl s_73BCF0(uint32_t a1); // 0x73BCF0 sub_73BCF0, 319 insns
	uint32_t __cdecl s_73C150(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6); // 0x73C150 sub_73C150, 17 insns
	uint32_t __cdecl s_73C1A0(uint32_t a1); // 0x73C1A0 sub_73C1A0 TASK, 36 insns
	uint32_t __cdecl s_73C220(uint32_t a1); // 0x73C220 sub_73C220, 28 insns
	uint32_t __cdecl s_73C3B0(uint32_t a1, uint32_t a2, uint32_t a3); // 0x73C3B0 sub_73C3B0, 295 insns
	uint32_t __cdecl s_73C790(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x73C790 sub_73C790, 132 insns
	uint32_t __cdecl s_73CD70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x73CD70 sub_73CD70, 373 insns
	uint32_t __cdecl s_73D1F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x73D1F0 sub_73D1F0, 450 insns
	uint32_t __cdecl s_73E1A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x73E1A0 sub_73E1A0, 513 insns
	uint32_t __cdecl s_73E7F0(uint32_t a1); // 0x73E7F0 sub_73E7F0, 142 insns
	uint32_t __cdecl s_73EB10(uint32_t a1); // 0x73EB10 sub_73EB10, 139 insns
	uint32_t __cdecl s_73EE10(uint32_t a1); // 0x73EE10 sub_73EE10, 15 insns
	uint32_t __cdecl s_73EE50(uint32_t a1); // 0x73EE50 sub_73EE50 TASK, 32 insns
	uint32_t __cdecl s_73EEC0(uint32_t a1); // 0x73EEC0 sub_73EEC0, 28 insns
	uint32_t __cdecl s_73EF20(uint32_t a1); // 0x73EF20 sub_73EF20, 15 insns
	uint32_t __cdecl s_73EF60(uint32_t a1); // 0x73EF60 sub_73EF60 TASK, 34 insns
	uint32_t __cdecl s_73EFE0(uint32_t a1); // 0x73EFE0 sub_73EFE0, 25 insns
	uint32_t __cdecl s_73F030(uint32_t a1); // 0x73F030 sub_73F030, 21 insns
	uint32_t __cdecl s_73F070(uint32_t a1); // 0x73F070 sub_73F070, 28 insns
	uint32_t __cdecl s_73F0C0(uint32_t a1); // 0x73F0C0 sub_73F0C0, 20 insns
	uint32_t __cdecl s_73F180(uint32_t a1); // 0x73F180 sub_73F180, 23 insns
	uint32_t __cdecl s_73F1D0(uint32_t a1); // 0x73F1D0 sub_73F1D0, 5 insns
	uint32_t __cdecl s_73F1E0(uint32_t a1); // 0x73F1E0 sub_73F1E0, 18 insns
	uint32_t __cdecl s_73F220(uint32_t a1); // 0x73F220 sub_73F220, 13 insns
	uint32_t __cdecl s_73F250(uint32_t a1); // 0x73F250 sub_73F250 TASK, 29 insns
	uint32_t __cdecl s_73F2B0(uint32_t a1); // 0x73F2B0 sub_73F2B0, 23 insns
	uint32_t __cdecl s_73F300(uint32_t a1); // 0x73F300 sub_73F300, 5 insns
	uint32_t __cdecl s_73F310(uint32_t a1); // 0x73F310 sub_73F310, 23 insns
	uint32_t __cdecl s_73F3E0(uint32_t a1); // 0x73F3E0 sub_73F3E0, 23 insns
	uint32_t __cdecl s_73F430(uint32_t a1); // 0x73F430 sub_73F430, 5 insns
	uint32_t __cdecl s_73F440(uint32_t a1); // 0x73F440 sub_73F440, 27 insns
	uint32_t __cdecl s_73F4A0(uint32_t a1); // 0x73F4A0 sub_73F4A0, 23 insns
	uint32_t __cdecl s_73F570(uint32_t a1); // 0x73F570 sub_73F570, 23 insns
	uint32_t __cdecl s_73F5C0(uint32_t a1); // 0x73F5C0 sub_73F5C0, 5 insns
	uint32_t __cdecl s_73F5D0(uint32_t a1); // 0x73F5D0 sub_73F5D0, 30 insns
	uint32_t __cdecl s_73F640(uint32_t a1); // 0x73F640 sub_73F640, 23 insns
	uint32_t __cdecl s_73F6A0(uint32_t a1); // 0x73F6A0 sub_73F6A0 TASK, 31 insns
	uint32_t __cdecl s_73F710(uint32_t a1); // 0x73F710 sub_73F710, 23 insns
	uint32_t __cdecl s_73F760(uint32_t a1); // 0x73F760 sub_73F760, 5 insns
	uint32_t __cdecl s_73F770(uint32_t a1); // 0x73F770 sub_73F770, 10 insns
	uint32_t __cdecl s_73F790(uint32_t a1); // 0x73F790 sub_73F790, 17 insns
	uint32_t __cdecl s_73F7C0(uint32_t a1); // 0x73F7C0 sub_73F7C0, 18 insns
	uint32_t __cdecl s_73F860(uint32_t a1); // 0x73F860 sub_73F860, 35 insns
	uint32_t __cdecl s_73F8D0(uint32_t a1); // 0x73F8D0 sub_73F8D0, 13 insns
	uint32_t __cdecl s_73F960(uint32_t a1); // 0x73F960 sub_73F960, 15 insns
	uint32_t __cdecl s_740470(uint32_t a1); // 0x740470 sub_740470, 39 insns
	uint32_t __cdecl s_740500(void); // 0x740500 sub_740500, 28 insns
	uint32_t __cdecl s_740840(void); // 0x740840 sub_740840, 21 insns
	uint32_t __cdecl s_741B10(uint32_t a1); // 0x741B10 sub_741B10, 27 insns
	uint32_t __cdecl s_741B90(uint32_t a1); // 0x741B90 sub_741B90, 50 insns
	uint32_t __cdecl s_741C10(uint32_t a1, uint32_t a2, uint32_t a3); // 0x741C10 sub_741C10, 509 insns
	uint32_t __cdecl s_7423F0(uint32_t a1, uint32_t a2); // 0x7423F0 sub_7423F0, 59 insns
	uint32_t __cdecl s_743220(uint32_t a1); // 0x743220 sub_743220, 13 insns
	uint32_t __cdecl s_7432C0(uint32_t a1); // 0x7432C0 sub_7432C0, 79 insns
	uint32_t __cdecl s_7433C0(uint32_t a1); // 0x7433C0 sub_7433C0 TASK, 88 insns
	uint32_t __cdecl s_7434C0(uint32_t a1); // 0x7434C0 sub_7434C0, 78 insns
	uint32_t __cdecl s_743B90(uint32_t a1); // 0x743B90 sub_743B90, 34 insns
	uint32_t __cdecl s_743C70(uint32_t a1); // 0x743C70 sub_743C70 TASK, 29 insns
	uint32_t __cdecl s_743CD0(uint32_t a1); // 0x743CD0 sub_743CD0, 12 insns
	uint32_t __cdecl s_743D10(uint32_t a1); // 0x743D10 sub_743D10, 63 insns
	uint32_t __cdecl s_743DD0(uint32_t a1); // 0x743DD0 sub_743DD0 TASK, 146 insns
	uint32_t __cdecl s_743FD0(uint32_t a1); // 0x743FD0 sub_743FD0, 19 insns
	uint32_t __cdecl s_744010(uint32_t a1); // 0x744010 sub_744010, 18 insns
	uint32_t __cdecl s_744060(uint32_t a1); // 0x744060 sub_744060 TASK, 29 insns
	uint32_t __cdecl s_7440C0(uint32_t a1); // 0x7440C0 sub_7440C0, 12 insns
	uint32_t __cdecl s_744100(uint32_t a1); // 0x744100 sub_744100, 68 insns
	uint32_t __cdecl s_7441D0(uint32_t a1); // 0x7441D0 sub_7441D0 TASK, 145 insns
	uint32_t __cdecl s_7443C0(uint32_t a1); // 0x7443C0 sub_7443C0, 19 insns
	uint32_t __cdecl s_744400(uint32_t a1); // 0x744400 sub_744400, 18 insns
	uint32_t __cdecl s_744450(uint32_t a1); // 0x744450 sub_744450 TASK, 29 insns
	uint32_t __cdecl s_7444B0(uint32_t a1); // 0x7444B0 sub_7444B0, 12 insns
	uint32_t __cdecl s_7444F0(uint32_t a1); // 0x7444F0 sub_7444F0, 41 insns
	uint32_t __cdecl s_744570(uint32_t a1); // 0x744570 sub_744570 TASK, 126 insns
	uint32_t __cdecl s_744730(uint32_t a1); // 0x744730 sub_744730, 19 insns
	uint32_t __cdecl s_744770(uint32_t a1); // 0x744770 sub_744770, 18 insns
	uint32_t __cdecl s_744820(uint32_t a1); // 0x744820 sub_744820, 45 insns
	uint32_t __cdecl s_7448A0(uint32_t a1); // 0x7448A0 sub_7448A0, 144 insns
	uint32_t __cdecl s_744AA0(uint32_t a1); // 0x744AA0 sub_744AA0 TASK, 88 insns
	uint32_t __cdecl s_744BA0(uint32_t a1); // 0x744BA0 sub_744BA0, 16 insns
	uint32_t __cdecl s_744BE0(uint32_t a1); // 0x744BE0 sub_744BE0, 25 insns
	uint32_t __cdecl s_744C30(uint32_t a1); // 0x744C30 sub_744C30 TASK, 86 insns
	uint32_t __cdecl s_744D30(uint32_t a1, uint32_t a2); // 0x744D30 sub_744D30, 91 insns
	uint32_t __cdecl s_744E80(uint32_t a1); // 0x744E80 sub_744E80, 19 insns
	uint32_t __cdecl s_744ED0(uint32_t a1); // 0x744ED0 sub_744ED0, 26 insns
	uint32_t __cdecl s_744F30(uint32_t a1); // 0x744F30 sub_744F30, 25 insns
	uint32_t __cdecl s_744F90(uint32_t a1); // 0x744F90 sub_744F90 TASK, 29 insns
	uint32_t __cdecl s_744FF0(uint32_t a1); // 0x744FF0 sub_744FF0, 8 insns
	uint32_t __cdecl s_745010(uint32_t a1); // 0x745010 sub_745010, 6 insns
	uint32_t __cdecl s_745030(uint32_t a1); // 0x745030 sub_745030, 97 insns
	uint32_t __cdecl s_7451E0(uint32_t a1); // 0x7451E0 sub_7451E0, 15 insns
	uint32_t __cdecl s_745210(uint32_t a1); // 0x745210 sub_745210, 13 insns
	uint32_t __cdecl s_7452B0(uint32_t a1); // 0x7452B0 sub_7452B0, 21 insns
	uint32_t __cdecl s_7452F0(uint32_t a1); // 0x7452F0 sub_7452F0 TASK, 38 insns
	uint32_t __cdecl s_745380(uint32_t a1); // 0x745380 sub_745380, 79 insns
	uint32_t __cdecl s_745490(uint32_t a1); // 0x745490 sub_745490, 79 insns
	uint32_t __cdecl s_7455B0(uint32_t a1); // 0x7455B0 sub_7455B0, 16 insns
	uint32_t __cdecl s_7455F0(uint32_t a1); // 0x7455F0 sub_7455F0, 6 insns
	uint32_t __cdecl s_745610(uint32_t a1); // 0x745610 sub_745610, 11 insns
	uint32_t __cdecl s_745650(uint32_t a1); // 0x745650 sub_745650, 19 insns
	uint32_t __cdecl s_7456A0(uint32_t a1); // 0x7456A0 sub_7456A0 TASK, 29 insns
	uint32_t __cdecl s_745700(uint32_t a1); // 0x745700 sub_745700, 14 insns
	uint32_t __cdecl s_745730(uint32_t a1); // 0x745730 sub_745730, 5 insns
	uint32_t __cdecl s_745740(uint32_t a1); // 0x745740 sub_745740, 78 insns
	uint32_t __cdecl s_745840(uint32_t a1); // 0x745840 sub_745840 TASK, 42 insns
	uint32_t __cdecl s_7459D0(uint32_t a1); // 0x7459D0 sub_7459D0, 22 insns
	uint32_t __cdecl s_745A20(uint32_t a1); // 0x745A20 sub_745A20, 16 insns
	uint32_t __cdecl s_745A60(uint32_t a1); // 0x745A60 sub_745A60, 6 insns
	uint32_t __cdecl s_745A80(uint32_t a1); // 0x745A80 sub_745A80, 11 insns
	uint32_t __cdecl s_745AD0(uint32_t a1); // 0x745AD0 sub_745AD0 TASK, 31 insns
	uint32_t __cdecl s_745B40(uint32_t a1); // 0x745B40 sub_745B40, 25 insns
	uint32_t __cdecl s_745BB0(uint32_t a1); // 0x745BB0 sub_745BB0, 19 insns
	uint32_t __cdecl s_745BF0(uint32_t a1, uint32_t a2, uint32_t a3); // 0x745BF0 sub_745BF0, 182 insns
	uint32_t __cdecl s_745E80(uint32_t a1); // 0x745E80 sub_745E80, 57 insns
	uint32_t __cdecl s_745F50(uint32_t a1, uint32_t a2); // 0x745F50 sub_745F50, 539 insns
	uint32_t __cdecl s_7466A0(uint32_t a1); // 0x7466A0 sub_7466A0 TASK, 62 insns
	uint32_t __cdecl s_746760(uint32_t a1); // 0x746760 sub_746760, 7 insns
	uint32_t __cdecl s_7467B0(uint32_t a1); // 0x7467B0 sub_7467B0, 22 insns
	uint32_t __cdecl s_746800(uint32_t a1); // 0x746800 sub_746800, 36 insns
	uint32_t __cdecl s_746870(uint32_t a1); // 0x746870 sub_746870, 60 insns
	uint32_t __cdecl s_7469C0(uint32_t a1); // 0x7469C0 sub_7469C0, 21 insns
	uint32_t __cdecl s_746AA0(uint32_t a1); // 0x746AA0 sub_746AA0, 19 insns
	uint32_t __cdecl s_746B10(uint32_t a1); // 0x746B10 sub_746B10, 18 insns
	uint32_t __cdecl s_746B60(uint32_t a1); // 0x746B60 GF_095Siren_CreatureActorTask TASK, 43 insns
	uint32_t __cdecl s_746DF0(uint32_t a1); // 0x746DF0 sub_746DF0, 24 insns
	uint32_t __cdecl s_746E50(uint32_t a1); // 0x746E50 sub_746E50, 18 insns
	uint32_t __cdecl s_746E80(uint32_t a1); // 0x746E80 sub_746E80, 22 insns
	uint32_t __cdecl s_746ED0(uint32_t a1); // 0x746ED0 sub_746ED0, 33 insns
	uint32_t __cdecl s_746F40(uint32_t a1); // 0x746F40 sub_746F40, 44 insns
	uint32_t __cdecl s_746FD0(uint32_t a1); // 0x746FD0 sub_746FD0 TASK, 90 insns
	uint32_t __cdecl s_747110(uint32_t a1); // 0x747110 sub_747110, 7 insns
	uint32_t __cdecl s_747160(uint32_t a1); // 0x747160 sub_747160, 54 insns
	uint32_t __cdecl s_747220(uint32_t a1); // 0x747220 sub_747220 TASK, 89 insns
	uint32_t __cdecl s_747320(uint32_t a1); // 0x747320 sub_747320, 8 insns
	uint32_t __cdecl s_747370(uint32_t a1); // 0x747370 sub_747370, 29 insns
	uint32_t __cdecl s_7473D0(uint32_t a1); // 0x7473D0 sub_7473D0, 14 insns

	// ====================================================================================
	// part s1
	// ====================================================================================
	// Siren (MAG_095) module-unique functions, part s1: the Siren director task and its cut
	// (choreography) state machine, the camera-script task, the cut spawner and the cut-1 model
	// emitter task (s_73C1A0) with its init state.
	//
	// Director script block (module global 0x1533010 -> 0x54-byte block, "cut script state"):
	//   +0x00..+0x04 words (0, 0, 0xCC0), +0x08.. bounds block filled by a_73A720,
	//   +0x40 s16 current cut (the cut whose spawns ran), +0x42 s16 requested cut (applied next
	//   tick: becomes +0x40 and runs the spawner), +0x44 s16 pending cut (becomes +0x42 next tick),
	//   +0x46 s16 frames since the current cut started, +0x48 s16 flag, +0x52 s16 claimed voice slot.
	// Cut helpers (engine): a_73B640(n) sets pending cut = n when pending == current == n-1 (returns 1);
	// a_73AAE0(n) = (requested >= n); a_73B7E0(n) = (current >= n).
	namespace
	{
		// eax after `mov al, [node+0x29]; inc al; mov [node+0x29], al`: low byte = new state index,
		// upper bytes = whatever eax held before
		inline uint32_t s1_inc_state_al(uint32_t node, uint32_t eax_before)
		{
			uint8_t v = (uint8_t)(U8(node, 0x29) + 1);
			U8(node, 0x29) = v;
			return (eax_before & 0xFFFFFF00u) | v;
		}

		// cast context (+0x0C) -> action record (0x14 bytes) of the node's action index (+0x2A):
		// record +0x08 = target list (0x18-byte records, byte 0 = target slot), +0x10 = u8 target count
		inline uint32_t s1_action_rec(uint32_t node)
		{
			int32_t act_idx = S8(node, 0x2A);
			uint32_t actions = U32(U32(node, 0x0C), 4);
			return actions + (uint32_t)(act_idx * 5) * 4;
		}
	}

	// 0x73A0F0 (module; Siren MAG_095_sub_73A0F0): state: once no child is alive (+0x28 == 0),
	// carves two scratch arenas (0x10E0 B -> 0x258EC54, 0xC240 B -> 0x258FB6C) from the arena
	// cursor 0x258FB74, clears them and four Siren words (s_73A130), advances the state
	uint32_t __cdecl s_73A0F0(uint32_t a1)
	{
		uint32_t node = a1;
		if (U8(node, 0x28) != 0) return 0; // eax = busy byte + leftover upper bits, unused by the dispatcher
		uint32_t cur = U32(0x258FB74, 0);
		U32(0x258EC54, 0) = cur;
		cur += 0x10E0;
		U32(0x258FB6C, 0) = cur;
		cur += 0xC240;
		U32(0x258FB74, 0) = cur;
		uint32_t r = s_73A130();
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return r; // 0
	}

	// 0x73A130 (module; Siren MAG_095_sub_73A130): clears the two scratch arenas (0x10E0 / 0xC240
	// bytes, MAG_007_sub_8DCC00) and zeroes the words 0x2585AE0, 0x258EDD0, 0x258E89C, 0x257F8A8
	uint32_t __cdecl s_73A130(void)
	{
		x::MAG_007_sub_8DCC00(U32(0x258EC54, 0), 0x10E0);
		x::MAG_007_sub_8DCC00(U32(0x258FB6C, 0), 0xC240);
		U16(0x2585AE0, 0) = 0;
		U16(0x258EDD0, 0) = 0;
		U16(0x258E89C, 0) = 0;
		U16(0x257F8A8, 0) = 0;
		return 0;
	}

	// 0x73A220 (module; Siren MAG_095_sub_73A220): state: inits the director script block
	// (s_73A6E0) and the asset table (s_73A270), spawns task 0x73A380 (0x30 B) into queue 0x258FB48
	// and the Siren director task s_73A7B0 (0x48 B) into queue 0x257F988, advances the state
	uint32_t __cdecl s_73A220(uint32_t a1)
	{
		uint32_t node = a1;
		s_73A6E0(); // quirk 0x73A225: node pushed as an argument the callee does not read
		s_73A270();
		x::Effect_AddTaskAndInitFromCtx(0x258FB48, 0x73A380, 0x30, node);
		uint32_t r = x::Effect_AddTaskAndInitFromCtx(0x257F988, 0x73A7B0, 0x48, node);
		return s1_inc_state_al(node, r);
	}

	// 0x73A270 (module; Siren MAG_095_sub_73A270): builds the Siren asset table at 0x258FB88
	// (0x16C B, pointer stored in 0x258EAA0): +0x100 = 0xA000-byte arena carved from 0x258FB74,
	// +0x104..+0x160 = model / sequence data pointers inside the loaded Siren file
	// (0x19D1018..0x19F19BC), +0x164/+0x166 = 0; sets 0x258BFA8 = 0x1533850; returns the table
	uint32_t __cdecl s_73A270(void)
	{
		U32(0x258BFA8, 0) = 0x1533850;
		U32(0x258EAA0, 0) = 0x258FB88;
		x::MAG_007_sub_8DCC00(0x258FB88, 0x16C);
		uint32_t tab = U32(0x258EAA0, 0);
		uint32_t cur = U32(0x258FB74, 0);
		const uint32_t edx = 0x19F1018;
		U32(tab, 0x100) = cur;
		cur += 0xA000;
		U32(0x258FB74, 0) = cur;
		const uint32_t ecx = 0x19D1018;
		U32(tab, 0x114) = ecx; // creature model container (bound in cut 3)
		U32(tab, 0x118) = ecx;
		U32(tab, 0x11C) = ecx;
		U32(tab, 0x120) = ecx;
		U32(tab, 0x104) = ecx;
		U32(tab, 0x108) = 0x19DFCC4;
		U32(tab, 0x10C) = edx;
		U32(tab, 0x110) = ecx;
		U32(tab, 0x124) = ecx;
		U32(tab, 0x128) = edx;
		U32(tab, 0x12C) = 0x19E10F8;
		U32(tab, 0x130) = 0x19EBF30;
		U32(tab, 0x134) = 0x19ED200;
		U32(tab, 0x138) = 0x19EE4D0;
		U32(tab, 0x13C) = 0x19F19BC;
		U32(tab, 0x140) = ecx;
		U32(tab, 0x144) = 0x19E0F58;
		U32(tab, 0x148) = 0x19E1028;
		U32(tab, 0x14C) = ecx;
		U32(tab, 0x150) = ecx;
		U32(tab, 0x154) = ecx;
		U32(tab, 0x158) = ecx;
		U32(tab, 0x15C) = ecx;
		U32(tab, 0x160) = ecx;
		U16(tab, 0x164) = 0;
		U16(tab, 0x166) = 0;
		return tab;
	}

	// 0x73A6E0 (module; Siren MAG_095_sub_73A6E0): clears the director script block (0x54 B at
	// [0x1533010]), sets its header words (0, 0, 0xCC0), fills its +0x08 bounds block from the
	// battle entities (a_73A720) and zeroes word +0x08
	uint32_t __cdecl s_73A6E0(void)
	{
		x::MAG_007_sub_8DCC00(U32(0x1533010, 0), 0x54);
		uint32_t blk = U32(0x1533010, 0);
		U16(blk, 0) = 0;
		U16(blk, 2) = 0;
		U16(blk, 4) = 0xCC0;
		uint32_t r = a_73A720(blk + 8);
		uint32_t blk2 = U32(0x1533010, 0);
		U16(blk2, 8) = 0;
		return r;
	}

	// 0x73A7B0 (module; Siren MAG_095_sub_73A7B0): TASK, Siren director: advances the cut script
	// (s_73A8E0, runs the cut spawner), runs the choreography state (+0x29, 28-entry table),
	// counts frames (+0x24); ends when finished (+0x26 bit0) and no child alive (+0x28)
	uint32_t __cdecl s_73A7B0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[28];
		states[0] = 0x73A930;  // wait for the bounds (a_73A950)
		states[1] = 0x73A990;  // frame >= 4: claim voice, spawn the camera-script task
		states[2] = 0x73B790;
		states[3] = 0x73B7B0;  // cut >= 1: sound 0x1533014
		states[4] = 0x73B800;
		states[5] = 0x73B880;
		states[6] = 0x73B8A0;
		states[7] = 0x73B8C0;
		states[8] = 0x73B8E0;
		states[9] = 0x73B900;
		states[10] = 0x73B950;
		states[11] = 0x73B9A0;
		states[12] = 0x73B9D0;
		states[13] = 0x73BA30;
		states[14] = 0x73BA50;
		states[15] = 0x73BA70;
		states[16] = 0x73BAD0;
		states[17] = 0x73BB00;
		states[18] = 0x73BB20;
		states[19] = 0x73BBB0;
		states[20] = 0x73BBD0;
		states[21] = 0x73BBF0;
		states[22] = 0x73BC10;
		states[23] = 0x73BC40;
		states[24] = 0x73BC60;
		states[25] = 0x73BC90;
		states[26] = 0x73BCB0;
		states[27] = 0x73BCD0;
		s_73A8E0(node);
		int32_t st = S8(node, 0x29);
		callp(states[st], node);
		uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x73A8E0 (module; Siren sub_73A8E0): cut script tick: counts frames in the cut (+0x46);
	// requested cut (+0x42) != current (+0x40) -> becomes current, frame count 0, runs the cut
	// spawner s_73BCF0; then pending (+0x44) != requested -> becomes requested (next tick)
	uint32_t __cdecl s_73A8E0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t blk = U32(0x1533010, 0);
		U16(blk, 0x46) = (uint16_t)(U16(blk, 0x46) + 1);
		uint16_t req = U16(blk, 0x42);
		if (U16(blk, 0x40) != req)
		{
			U16(blk, 0x40) = req;
			U16(blk, 0x46) = 0;
			s_73BCF0(node);
			blk = U32(0x1533010, 0);
		}
		uint16_t pend = U16(blk, 0x44);
		if (U16(blk, 0x42) != pend)
		{
			U16(blk, 0x42) = pend;
			a_73A6D0(); // 0x73BCE0 (nullsub twin; node pushed and ignored)
		}
		return blk;
	}

	// 0x73A990 (module; Siren sub_73A990): state: from frame 4, claims a voice slot for sound
	// 0x16A3FB0 (slot -> script +0x52), spawns the camera-script task s_73A9E0 (0xE0 B) into
	// queue 0x257FA90, advances the state
	uint32_t __cdecl s_73A990(uint32_t a1)
	{
		uint32_t node = a1;
		if (S16(node, 0x24) < 4) return 0; // eax leftover, unused
		uint32_t slot = x::BdSound_ClaimVoiceSlot(0x16A3FB0, 1, 0x80);
		uint32_t blk = U32(0x1533010, 0);
		U16(blk, 0x52) = (uint16_t)slot;
		uint32_t r = x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x73A9E0, 0xE0, node);
		return s1_inc_state_al(node, r);
	}

	// 0x73A9E0 (module; Siren sub_73A9E0): TASK, camera-script task: runs its state (+0x29,
	// 15-entry table: camera path set-ups a_73AB20 per cut, camera path steps a_73AE10 / a_73B4B0
	// with countdowns in +0xDC), counts frames; ends when finished and no child alive
	uint32_t __cdecl s_73A9E0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[15];
		states[0] = 0x73AAA0;
		int32_t st = S8(node, 0x29);
		states[1] = 0x73B2D0;
		states[2] = 0x73B2F0;
		states[3] = 0x73B320;
		states[4] = 0x73B350;
		states[5] = 0x73B380;
		states[6] = 0x73B480;
		states[7] = 0x73B620;
		states[8] = 0x73B670;
		states[9] = 0x73B6B0;
		states[10] = 0x73B6D0;
		states[11] = 0x73B6F0;
		states[12] = 0x73B730;
		states[13] = 0x73B750;
		states[14] = 0x73B780; // ret (not ported)
		callp(states[st], node);
		uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x73AAA0 (module; Siren sub_73AAA0): camera state 0: once cut >= 1 is requested, clears the
	// camera block (a_73AB00) and sets up camera path 0x1533C40 against the script bounds
	// ([0x1533010]+8), advances the state
	uint32_t __cdecl s_73AAA0(uint32_t a1)
	{
		uint32_t r = a_73AAE0(1);
		if (r == 0) return 0;
		a_73AB00();
		uint32_t blk = U32(0x1533010, 0);
		a_73AB20(0x1533C40, blk + 8, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B2F0 (module; Siren sub_73B2F0): camera state: once cut >= 2 is requested, sets up camera
	// path 0x1533C88 against the script bounds (+8), advances the state
	uint32_t __cdecl s_73B2F0(uint32_t a1)
	{
		uint32_t r = a_73AAE0(2);
		if (r == 0) return 0;
		uint32_t blk = U32(0x1533010, 0);
		a_73AB20(0x1533C88, blk + 8, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B320 (module; Siren sub_73B320): camera state: steps the camera path (a_73AE10); once cut
	// >= 3 is requested, sets up camera path 0x1533CD0 against the script block itself (no +8),
	// advances the state
	uint32_t __cdecl s_73B320(uint32_t a1)
	{
		a_73AE10();
		uint32_t r = a_73AAE0(3);
		if (r == 0) return 0;
		uint32_t blk = U32(0x1533010, 0);
		a_73AB20(0x1533CD0, blk, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B380 (module; Siren sub_73B380): camera state: steps the camera path; when it completes
	// (a_73AE10 == 1) starts camera sequence 0x1533C30 (a_73B3B0), countdown +0xDC = 0x68,
	// advances the state
	uint32_t __cdecl s_73B380(uint32_t a1)
	{
		uint32_t r = a_73AE10();
		if (r != 1) return r;
		a_73B3B0(0x1533C30);
		U16(a1, 0xDC) = 0x68;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B480 (module; Siren sub_73B480): camera state: steps camera sequence (a_73B4B0, writes the
	// battle camera); counts down +0xDC, advances the state when it reaches <= 0
	uint32_t __cdecl s_73B480(uint32_t a1)
	{
		a_73B4B0(); // quirk 0x73B480: 0x1533C30 pushed as an argument the callee does not read
		U16(a1, 0xDC) = (uint16_t)(U16(a1, 0xDC) - 1);
		if (S16(a1, 0xDC) <= 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B670 (module; Siren sub_73B670): camera state: once cut >= 5 is requested, sets up camera
	// path 0x1533D60 against the script block (no +8), countdown +0xDC = 0x20, advances the state
	uint32_t __cdecl s_73B670(uint32_t a1)
	{
		uint32_t r = a_73AAE0(5);
		if (r == 0) return 0;
		uint32_t blk = U32(0x1533010, 0);
		a_73AB20(0x1533D60, blk, 0);
		U16(a1, 0xDC) = 0x20;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B6B0 (module; Siren sub_73B6B0): camera state: steps the camera path; counts down +0xDC,
	// advances the state when it reaches <= 0
	uint32_t __cdecl s_73B6B0(uint32_t a1)
	{
		a_73AE10();
		U16(a1, 0xDC) = (uint16_t)(U16(a1, 0xDC) - 1);
		if (S16(a1, 0xDC) <= 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B6D0 (module; Siren sub_73B6D0): camera state: steps the camera path; asks for cut 6
	// (a_73B640(6)), advances the state when accepted
	uint32_t __cdecl s_73B6D0(uint32_t a1)
	{
		a_73AE10();
		uint32_t r = a_73B640(6);
		if (r == 0) return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B6F0 (module; Siren sub_73B6F0): camera state: steps the camera path; once cut >= 6 is
	// requested, sets up camera path 0x1533DA8 against the script bounds (+8), advances the state
	uint32_t __cdecl s_73B6F0(uint32_t a1)
	{
		a_73AE10();
		uint32_t r = a_73AAE0(6);
		if (r == 0) return 0;
		uint32_t blk = U32(0x1533010, 0);
		a_73AB20(0x1533DA8, blk + 8, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B730 (module; Siren sub_73B730): camera state: steps the camera path; advances the state
	// once cut >= 8 is requested
	uint32_t __cdecl s_73B730(uint32_t a1)
	{
		a_73AE10();
		uint32_t r = a_73AAE0(8);
		if (r == 0) return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B7B0 (module; Siren sub_73B7B0): director state: once cut >= 1 runs, plays sound effect
	// 0x1533014 (BdPlaySE), advances the state
	uint32_t __cdecl s_73B7B0(uint32_t a1)
	{
		uint32_t r = a_73B7E0(1);
		if (r == 0) return 0;
		x::BdPlaySE(0x1533014, 1, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B800 (module; Siren sub_73B800): director state: asset flag +0x166 == 0 -> clears the
	// Siren toggle (s_73B840(0)) and advances; else from cut frame 0x1D sets it (s_73B840(1)),
	// staying in this state
	uint32_t __cdecl s_73B800(uint32_t a1)
	{
		uint32_t tab = U32(0x258EAA0, 0);
		if (U16(tab, 0x166) == 0)
		{
			s_73B840(0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return a1;
		}
		uint32_t blk = U32(0x1533010, 0);
		if (S16(blk, 0x46) < 0x1D) return tab;
		return s_73B840(1);
	}

	// 0x73B840 (module; Siren sub_73B840): sets (a1 != 0) / clears (a1 == 0) the Siren toggle pair
	// 0x258FB70 / 0x2585E38 (0 or 1) when it changes; returns 1 when it set it, else 0
	uint32_t __cdecl s_73B840(uint32_t a1)
	{
		uint32_t cur = U32(0x258FB70, 0);
		if (a1 == 0)
		{
			if (cur == 1)
			{
				U32(0x258FB70, 0) = 0;
				U32(0x2585E38, 0) = 0;
			}
			return 0;
		}
		if (cur != 0) return 0;
		U32(0x258FB70, 0) = 1;
		U32(0x2585E38, 0) = 1;
		return 1;
	}

	// 0x73B880 (module; Siren sub_73B880): director state: advances once the cut frame count
	// (+0x46) reaches 0x1E
	uint32_t __cdecl s_73B880(uint32_t a1)
	{
		uint32_t blk = U32(0x1533010, 0);
		if (S16(blk, 0x46) < 0x1E) return blk;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B900 (module; Siren sub_73B900): director state: after cut frame 0x19, countdown +0x44 = 6,
	// spawns task 0x8DDC30 (0x40 B) into queue 0x258BE20 with +0x34 = data 0x1533E10, +0x38 = 0,
	// advances the state
	uint32_t __cdecl s_73B900(uint32_t a1)
	{
		uint32_t blk = U32(0x1533010, 0);
		if (S16(blk, 0x46) <= 0x19) return blk;
		uint32_t node = a1;
		U16(node, 0x44) = 6;
		uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x258BE20, 0x8DDC30, 0x40, node);
		U32(t, 0x34) = 0x1533E10;
		U16(t, 0x38) = 0;
		return s1_inc_state_al(node, t);
	}

	// 0x73B950 (module; Siren sub_73B950): director state: counts down +0x44; at <= 0: asset flag
	// +0x166 == 0 -> clears the Siren toggle and advances, else sets it (retried each tick)
	uint32_t __cdecl s_73B950(uint32_t a1)
	{
		uint32_t node = a1;
		U16(node, 0x44) = (uint16_t)(U16(node, 0x44) - 1);
		uint16_t cnt = U16(node, 0x44);
		if ((int16_t)cnt > 0) return cnt; // eax = ax + leftover upper bits, unused
		uint32_t tab = U32(0x258EAA0, 0);
		if (U16(tab, 0x166) == 0)
		{
			uint32_t r = s_73B840(0);
			return s1_inc_state_al(node, r);
		}
		return s_73B840(1);
	}

	// 0x73B9A0 (module; Siren sub_73B9A0): director state: asks for cut 3; when accepted plays sound
	// effect 0x1533018 and advances
	uint32_t __cdecl s_73B9A0(uint32_t a1)
	{
		uint32_t r = a_73B640(3);
		if (r == 0) return 0;
		x::BdPlaySE(0x1533018, 1, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B9D0 (module; Siren sub_73B9D0): director state: once cut >= 3 runs, calls a_73B9F0
	// (pass over the battle entities) and advances
	uint32_t __cdecl s_73B9D0(uint32_t a1)
	{
		uint32_t r = a_73B7E0(3);
		if (r == 0) return 0;
		a_73B9F0();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BA30 (module; Siren sub_73BA30): director state: advances after cut frame 0x2D
	uint32_t __cdecl s_73BA30(uint32_t a1)
	{
		uint32_t blk = U32(0x1533010, 0);
		if (S16(blk, 0x46) <= 0x2D) return blk;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BA50 (module; Siren sub_73BA50): director state: asks for cut 4, advances when accepted
	uint32_t __cdecl s_73BA50(uint32_t a1)
	{
		uint32_t r = a_73B640(4);
		if (r == 0) return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BA70 (module; Siren sub_73BA70): director state: once cut >= 5 runs, calls a_73BA90
	// (pass over the battle entities) and advances
	uint32_t __cdecl s_73BA70(uint32_t a1)
	{
		uint32_t r = a_73B7E0(5);
		if (r == 0) return 0;
		a_73BA90();
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BAD0 (module; Siren sub_73BAD0): director state: once cut >= 6 runs, plays sound effect
	// 0x1533020 and advances
	uint32_t __cdecl s_73BAD0(uint32_t a1)
	{
		uint32_t r = a_73B7E0(6);
		if (r == 0) return 0;
		x::BdPlaySE(0x1533020, 1, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BB20 (module; Siren sub_73BB20): director state: asset flag +0x166 == 0 -> a_73BB60(0)
	// (clears the toggle 0x258FB70/0x2585E38) and advances; else from cut frame 0x1D a_73BB60(1)
	uint32_t __cdecl s_73BB20(uint32_t a1)
	{
		uint32_t tab = U32(0x258EAA0, 0);
		if (U16(tab, 0x166) == 0)
		{
			a_73BB60(0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return a1;
		}
		uint32_t blk = U32(0x1533010, 0);
		if (S16(blk, 0x46) < 0x1D) return tab;
		return a_73BB60(1);
	}

	// 0x73BBB0 (module; Siren sub_73BBB0): director state: advances after cut frame 0x1E
	uint32_t __cdecl s_73BBB0(uint32_t a1)
	{
		uint32_t blk = U32(0x1533010, 0);
		if (S16(blk, 0x46) <= 0x1E) return blk;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BBF0 (module; Siren sub_73BBF0): director state: advances once cut >= 7 runs
	uint32_t __cdecl s_73BBF0(uint32_t a1)
	{
		uint32_t r = a_73B7E0(7);
		if (r == 0) return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BC10 (module; Siren sub_73BC10): director state: after cut frame 1, sets script +0x48 = 1,
	// countdown +0x44 = 8, advances
	uint32_t __cdecl s_73BC10(uint32_t a1)
	{
		uint32_t blk = U32(0x1533010, 0);
		if (S16(blk, 0x46) <= 1) return blk;
		U16(blk, 0x48) = 1;
		U16(a1, 0x44) = 8;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BC60 (module; Siren sub_73BC60): director state: asks for cut 8; when accepted releases
	// the voice slot claimed in script +0x52 (sub_4A2940) and advances
	uint32_t __cdecl s_73BC60(uint32_t a1)
	{
		uint32_t r = a_73B640(8);
		if (r == 0) return 0;
		uint32_t blk = U32(0x1533010, 0);
		x::sub_4A2940((uint32_t)(int32_t)S16(blk, 0x52));
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BC90 (module; Siren sub_73BC90): director state: advances once cut >= 8 runs
	uint32_t __cdecl s_73BC90(uint32_t a1)
	{
		uint32_t r = a_73B7E0(8);
		if (r == 0) return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BCF0 (module; Siren sub_73BCF0): cut spawner: on entry to cut n (script +0x40, 1..8)
	// spawns that cut's tasks: model emitters (s_73C150 into 0x258A010 / a_73C100, model data
	// from the asset table 0x258EAA0), effect tasks into queues 0x257FA90 / 0x2585EF0 (some once
	// per target, target slot -> node +0x2D) and, in cut 3, the Siren creature actor s_746B60
	// (queue 0x258EC40) bound to its model container; cut 5 spawns nothing
	uint32_t __cdecl s_73BCF0(uint32_t a1)
	{
		uint32_t blk = U32(0x1533010, 0);
		int32_t cut = S16(blk, 0x40);
		switch (cut) // jump table 0x73C0D8, index cut-1 (unsigned > 7 -> exit)
		{
		case 1: // 0x73BD0E
		{
			uint32_t node = a1;
			s_73C150(node, 0x73C1A0, 0x16A5380, 0x94, 0, 0);
			x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x743C70, 0xE0, node);
			x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x744060, 0xE0, node);
			x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x746920, 0xE0, node);
			break;
		}
		case 2: // 0x73BD72
		{
			uint32_t tab = U32(0x258EAA0, 0);
			uint32_t node = a1;
			s_73C150(node, 0x73EE50, U32(tab, 0x128), 0x48, 1, 0);
			x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x744450, 0xE0, node);
			uint32_t rec = s1_action_rec(node);
			if (U8(rec, 0x10) == 0) break;
			uint32_t i = 0;
			uint32_t off = 0;
			do // one per target
			{
				uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x7447C0, 0xE0, node);
				rec = s1_action_rec(node);
				i++;
				off += 0x18;
				uint32_t targets = U32(rec, 8);
				U8(t, 0x2D) = U8(targets + off - 0x18, 0); // target slot
			} while ((int32_t)i < (int32_t)U8(rec, 0x10));
			break;
		}
		case 3: // 0x73BE10
		{
			uint32_t node = a1;
			uint32_t actor = x::Effect_AddTaskAndInitFromCtx(0x258EC40, 0x746B60, 0x140, node);
			uint32_t tab = U32(0x258EAA0, 0);
			x::Effect_BindModelContainerSetAnim(actor, U32(tab, 0x114), 0);
			tab = U32(0x258EAA0, 0);
			s_73C150(node, 0x73EF60, U32(tab, 0x12C), 0x228, 2, 0);
			tab = U32(0x258EAA0, 0);
			s_73C150(node, 0x73F110, U32(tab, 0x130), 0x4A0, 3, 0);
			tab = U32(0x258EAA0, 0);
			s_73C150(node, 0x73F6A0, U32(tab, 0x138), 0xEC, 5, 0);
			x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x744F90, 0xE0, node);
			break;
		}
		case 4: // 0x73BEC0
		{
			uint32_t tab = U32(0x258EAA0, 0);
			uint32_t node = a1;
			s_73C150(node, 0x73F250, U32(tab, 0x134), 0x4A0, 4, 0);
			tab = U32(0x258EAA0, 0);
			s_73C150(node, 0x73F370, U32(tab, 0x134), 0x4A0, 4, 0);
			tab = U32(0x258EAA0, 0);
			s_73C150(node, 0x73F500, U32(tab, 0x134), 0x4A0, 4, 0);
			x::Effect_AddTaskAndInitFromCtx(0x2585EF0, 0x745250, 0x64, node);
			for (uint32_t i = 0; (int32_t)i < 4; i++)
			{
				uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x745AD0, 0xE0, node);
				U16(t, 0xDE) = (uint16_t)i; // instance index 0..3
			}
			break;
		}
		case 6: // 0x73BF6A
		{
			uint32_t node = a1;
			uint32_t rec = s1_action_rec(node);
			if (U8(rec, 0x10) != 0)
			{
				uint32_t i = 0;
				uint32_t off = 0;
				do // one per target
				{
					uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x2585EF0, 0x7456A0, 0x64, node);
					rec = s1_action_rec(node);
					i++;
					off += 0x18;
					uint32_t targets = U32(rec, 8);
					U8(t, 0x2D) = U8(targets + off - 0x18, 0);
				} while ((int32_t)i < (int32_t)U8(rec, 0x10));
			}
			rec = s1_action_rec(node);
			if (U8(rec, 0x10) == 0) break;
			uint32_t i = 0;
			uint32_t off = 0;
			do // one per target
			{
				uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x743250, 0xE0, node);
				rec = s1_action_rec(node);
				i++;
				uint32_t targets = U32(rec, 8);
				off += 0x18;
				U8(t, 0x2D) = U8(targets + off - 0x18, 0);
			} while ((int32_t)i < (int32_t)U8(rec, 0x10));
			break;
		}
		case 7: // 0x73C02A
		{
			uint32_t node = a1;
			uint32_t rec = s1_action_rec(node);
			if (U8(rec, 0x10) != 0)
			{
				uint32_t i = 0;
				uint32_t off = 0;
				do // one model emitter per target
				{
					uint32_t tab = U32(0x258EAA0, 0);
					uint32_t t = s_73C150(node, 0x73F800, U32(tab, 0x13C), 0x140, 6, 0);
					rec = s1_action_rec(node);
					i++;
					off += 0x18;
					uint32_t targets = U32(rec, 8);
					U8(t, 0x2D) = U8(targets + off - 0x18, 0);
				} while ((int32_t)i < (int32_t)U8(rec, 0x10));
			}
			uint32_t tab = U32(0x258EAA0, 0);
			a_73C100(node, 0x73F900, U32(tab, 0x10C), 0, 0x2D, 0);
			break;
		}
		case 8: // 0x73C0B7
			x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x746A40, 0xE0, a1);
			break;
		default: // cut 5 (table -> 0x73C0D3) and anything outside 1..8: nothing
			break;
		}
		return 0; // void
	}

	// 0x73C150 (module; Siren sub_73C150): spawns a model-emitter task a2 (0x534 B) into queue
	// 0x258A010 under a1: +0x74 = model data a3, +0x78 = (s16)a4 prim layout size, +0x80 = (u16)a5
	// model id, +0x82 = (u16)a6; returns the node
	uint32_t __cdecl s_73C150(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x258A010, a2, 0x534, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x73C1A0 (module; Siren sub_73C1A0): TASK, cut-1 model emitter: runs its state (+0x29: init
	// + first draw s_73C220, draw until cut >= 2 s_73EE10, idle), then scrolls two texture offsets
	// (+0x8C += 4, +0x90 -= 6, mod 0x80), counts frames; ends when finished and no child alive
	uint32_t __cdecl s_73C1A0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x73C220;
		int32_t st = S8(node, 0x29);
		states[1] = 0x73EE10;
		states[2] = 0x73EE40; // ret (not ported)
		callp(states[st], node);
		uint8_t u = U8(node, 0x8C);
		uint8_t v = U8(node, 0x90);
		uint8_t status = U8(node, 0x26);
		uint32_t nu = (uint32_t)(uint8_t)(u + 4) & 0x7F;
		uint32_t nv = (uint32_t)(uint8_t)(v - 6) & 0x7F;
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		U16(node, 0x8C) = (uint16_t)nu;
		U16(node, 0x90) = (uint16_t)nv;
		if ((status & 1) && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x73C220 (module; Siren sub_73C220): emitter init state: decodes the model prim layout
	// (+0x74 data, +0x78 size -> +0x94), position +0x1C/+0x20 from the script bounds (+8/+0xC),
	// +0x1E = 0, 16-bit +0x20 += 0xE000, scale +0x50/+0x54/+0x58 = 0x1000, draws it once
	// (a_73C280), advances the state
	uint32_t __cdecl s_73C220(uint32_t a1)
	{
		uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		uint32_t blk = U32(0x1533010, 0);
		uint32_t p0 = U32(blk, 8);
		uint32_t p1 = U32(blk, 0xC);
		U32(node, 0x1C) = p0;
		U32(node, 0x20) = p1;
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + 0xE000);
		U16(node, 0x1E) = 0;
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		uint32_t r = a_73C280(node);
		return s1_inc_state_al(node, r);
	}

	// ====================================================================================
	// part s2
	// ====================================================================================
	// Siren (095) model renderer: per-instance setup (s_73C3B0), primitive-list dispatcher (s_73C790),
	// textured-triangle list (s_73CD70) and textured-quad list (s_73D1F0).
	namespace
	{
		// GP0 0xE2 texture-window word built from a RECT {x,y,w,h} (u16 each) at ctx+b
		// (0x73D082-0x73D0C1 / 0x73D124-0x73D163; same sequence at 0x73D5EA / 0x73D696)
		uint32_t s2_twin(uint32_t ctx, int32_t b)
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
	}

	// 0x73C3B0 (module 095, callback passed by a_73C280 to MAG_011_sub_701970): builds a 0x68-byte
	// render context in the Field_Alloc scratch for one model instance (a2) of model bank a1 with
	// camera/environment block a3: picks the vertex frame (or interpolates two frames into the
	// buffer a3+0x48), builds the local matrix (angles, translation rotated by a3, scale), loads it
	// into the GTE, sets flags / depth cue / UV scroll / texture windows, draws all primitive lists
	// through s_73C790 into the effect OT (packet cursor 0x1D8E054), then optionally runs the extra
	// pass s_73EB10 (env+0x56 == 0, instance kind 0) or s_73E7F0 (env+0x56 == 1).
	uint32_t __cdecl s_73C3B0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t obj = a2;  // edi: instance (+0 kind, +2 model idx, +4 flags, +8 pos, +0x10 angles,
		                          //      +0x18 scale, +0x20 far colour, +0x24 depth cue, +0x26 lerp, +0x28/+0x2A frames)
		const uint32_t env = a3;  // ebx: camera matrix (Mat4x3) + render environment
		if (((int32_t)S16(obj, 0x1C) | S32(obj, 0x18)) == 0)
			return 0;  // void (zero scale)
		if (S16(obj, 0x24) >= 0x1000 && U32(obj, 0x20) == 0)
			return 0;  // void (fully faded to black)

		const uint32_t ctx = x::Field_Alloc(0x68);
		uint32_t model = U32(a1, 0);
		const int32_t mdl = S16(obj, 2);
		model = model + U32(model + (uint32_t)mdl * 4u + 8, 0);
		const uint16_t frB = U16(obj, 0x2A);
		const uint16_t frA = U16(obj, 0x28);
		U32(ctx, 0) = model;  // +0 model data
		// +4 vertex list of a frame: model + 0xC + frame * U32(model,4) * 8
		auto frame_ptr = [&](uint16_t f) -> uint32_t {
			if (f == 0)
				return model + 0xC;
			return model + (uint32_t)mul32(S32(model, 4), (int32_t)(int16_t)f) * 8u + 0xC;
		};
		if (frA == frB)
			U32(ctx, 4) = frame_ptr(frA);
		else
		{
			const uint16_t lerp = U16(obj, 0x26);
			if (lerp == 0)
				U32(ctx, 4) = frame_ptr(frA);
			else if (lerp == 0x1000)
				U32(ctx, 4) = frame_ptr(frB);
			else
			{
				// interpolated vertices written to the buffer env+0x48
				x::MAG_017_sub_701390(model, (uint32_t)(int32_t)(int16_t)frA, (uint32_t)(int32_t)(int16_t)frB,
					(uint32_t)(int32_t)S16(obj, 0x26), U32(env, 0x48));
				U32(ctx, 4) = U32(env, 0x48);
			}
		}

		// stack frame of the original (esp-relative offsets 0x10..0x57 after the prologue)
		alignas(4) uint8_t stk[0x48] = {};
		auto L = [&](int32_t off) -> uint32_t { return P(stk) + (uint32_t)(off - 0x10); };
		const uint32_t mat = L(0x38);  // MATRIX {s16 m[3][3]; s32 t[3] at +0x14 (= L 0x4C)}

		x::MAG_017_sub_701310(obj + 0x10, mat);  // rotation matrix from the angles
		const uint16_t px = U16(obj, 8);
		const uint16_t py = U16(obj, 0xA);
		const uint16_t pz = U16(obj, 0xC);
		U16(L(0x10), 0) = px;  // SVECTOR position
		U16(L(0x12), 0) = py;
		U16(L(0x14), 0) = pz;
		int32_t tx, ty, tz;
		if (U8(obj, 5) & 2)
		{
			// position already in view space
			tx = (int16_t)px;
			ty = (int16_t)py;
			tz = (int16_t)pz;
		}
		else
		{
			x::GTE_SetRotMatrix(env);
			x::GTE_LoadV0(L(0x10));
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(L(0x4C));
			x::GTE_MatrixMultiply(env, mat);
			tx = S32(L(0x4C), 0);
			ty = S32(L(0x50), 0);
			tz = S32(L(0x54), 0);
		}
		S32(L(0x4C), 0) = add32(tx, S32(env, 0x14));
		S32(L(0x50), 0) = add32(ty, S32(env, 0x18));
		S32(L(0x54), 0) = add32(tz, S32(env, 0x1C));
		if (!(U32(obj, 0x18) == 0x10001000u && U16(obj, 0x1C) == 0x1000))
		{
			if (U32(obj, 4) & 0x100)
			{
				// diagonal 3x3 short matrix at L 0x18
				U16(L(0x18), 0) = U16(obj, 0x18);
				U16(L(0x20), 0) = U16(obj, 0x1A);
				const uint16_t sz = U16(obj, 0x1C);
				U16(L(0x28), 0) = sz;
				U16(L(0x22), 0) = 0;
				U16(L(0x1C), 0) = 0;
				U16(L(0x26), 0) = 0;
				U16(L(0x1A), 0) = 0;
				U16(L(0x24), 0) = 0;
				U16(L(0x1E), 0) = 0;
				x::sub_56C220(mat, L(0x18));
			}
			else
			{
				S32(L(0x18), 0) = S16(obj, 0x18);  // VECTOR scale
				S32(L(0x1C), 0) = S16(obj, 0x1A);
				S32(L(0x20), 0) = S16(obj, 0x1C);
				x::scale3DMatrix(mat, L(0x18));
			}
		}
		x::GTE_SetRotMatrix_W(mat);
		x::GTE_SetTransVector_W(mat);

		// +0x14 render flags: 0x2000 keep frame vertex ptr, 1/4 semi-trans on/off, 0x10 no
		// back-face cull, 0x40 depth cue; +0xC depth-cue factor, +8 far colour
		const uint32_t ofl = U32(obj, 4);
		if (ofl & 0x4000)
			U32(ctx, 0x14) = 0x2000;
		else
			U32(ctx, 0x14) = 0x2030;
		if (ofl & 0x2000)
			U32(ctx, 0x14) = U32(ctx, 0x14) | 0xC;
		const int32_t fade = S16(obj, 0x24);
		S32(ctx, 0xC) = fade;
		if (fade != 0)
		{
			const uint32_t f = U32(ctx, 0x14);
			U32(ctx, 8) = U32(obj, 0x20);
			U32(ctx, 0x14) = f | 0xC0;
		}
		else
		{
			const uint32_t ef = U32(env, 0x44);
			if (ef != 0)
			{
				U32(ctx, 0xC) = ef;
				U32(ctx, 0x14) = U32(ctx, 0x14) | 0xC0;
				U32(ctx, 8) = U32(env, 0x38);
			}
		}

		const int32_t mode = S16(env, 0x56);
		S32(ctx, 0x10) = S16(env, 0x58);  // +0x10 OTZ bias
		// +0x18/+0x1A UV scroll (du,dv), +0x1C / +0x24 texture-window RECTs {x,y,w,h}
		// (the low bytes of +0x28/+0x2A double as the UV wrap subtrahends (window width))
		bool tail = true;
		switch (mode)
		{
		case 0:
			if (U16(obj, 0) == 0)
				U16(ctx, 0x18) = U16(env, 0x4C);
			else
				U16(ctx, 0x18) = U16(env, 0x50);
			break;
		case 1:
			U16(ctx, 0x18) = U16(env, 0x4C);
			break;
		case 2:
			if (U16(obj, 0) == 3)
				U16(ctx, 0x18) = U16(env, 0x4C);
			else
				tail = false;
			break;
		default:
			tail = false;
			break;
		}
		if (tail)
		{
			U16(ctx, 0x1A) = 0;
			U16(ctx, 0x1E) = 0;
			U16(ctx, 0x1C) = 0;
			U16(ctx, 0x22) = 0x100;
			U16(ctx, 0x20) = 0x100;
			U16(ctx, 0x26) = 0;
			U16(ctx, 0x24) = 0;
			U16(ctx, 0x28) = 0x80;
			U16(ctx, 0x2A) = 0x100;
		}
		else
		{
			U16(ctx, 0x18) = 0;
			U16(ctx, 0x1A) = 0;
			U16(ctx, 0x1E) = 0;
			U16(ctx, 0x1C) = 0;
			U16(ctx, 0x22) = 0x100;
			U16(ctx, 0x20) = 0x100;
			U16(ctx, 0x26) = 0;
			U16(ctx, 0x24) = 0;
			U16(ctx, 0x2A) = 0x100;
			U16(ctx, 0x28) = 0x100;
		}

		const uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		const uint32_t newCursor = s_73C790(ctx, ot, 2, cursor);
		MEM<uint32_t>(0x1D8E054) = newCursor;

		if (S16(env, 0x56) == 0 && U16(obj, 0) == 0)
		{
			MEM<uint16_t>(0x258FB80) = U16(obj, 8);
			MEM<uint16_t>(0x258FB82) = U16(obj, 0xA);
			MEM<uint16_t>(0x258FB84) = U16(obj, 0xC);
			MEM<int32_t>(0x258EC60) = S16(obj, 0x18);
			MEM<int32_t>(0x258EC64) = S16(obj, 0x1A);
			MEM<int32_t>(0x258EC68) = S16(obj, 0x1C);
			s_73EB10(ctx);
		}
		if (U16(env, 0x56) == 1)
		{
			MEM<uint16_t>(0x258FB80) = U16(obj, 8);
			MEM<uint16_t>(0x258FB82) = U16(obj, 0xA);
			MEM<uint16_t>(0x258FB84) = U16(obj, 0xC);
			MEM<int32_t>(0x258EC60) = S16(obj, 0x18);
			MEM<int32_t>(0x258EC64) = S16(obj, 0x1A);
			MEM<int32_t>(0x258EC68) = S16(obj, 0x1C);
			s_73E7F0(ctx);
		}
		x::Field_Free(0x68);
		return 0;  // void
	}

	// 0x73C790 (module 095): primitive-list dispatcher of render context a1: resets the frame
	// vertex pointer unless flag 0x2000, sets the primitive-list cursor (+0x2C), calls
	// someCameraWork_45DD60 with the far colour bytes +8/+9/+0xA, then for each of the 8 list kinds
	// draws it (count != 0) with its renderer (0x73C8C0, 0x73CAE0, FT3 s_73CD70, FT4 s_73D1F0,
	// a_73D770, a_73D9C0, 0x73DCA0, s_73E1A0) into OT a2 (depth shift a3) or skips the empty count;
	// returns the new packet cursor (starting from a4).
	uint32_t __cdecl s_73C790(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		if (!(U32(ctx, 0x14) & 0x2000))
			U32(ctx, 4) = U32(ctx, 0) + 8;
		const uint32_t model = U32(ctx, 0);
		const uint32_t fb = U8(ctx, 0xA);
		const uint32_t lists = U32(model, 0) + model;
		const uint32_t fg = U8(ctx, 9);
		U32(ctx, 0x2C) = lists;  // +0x2C cursor in the primitive lists
		const uint32_t fr = U8(ctx, 8);
		x::someCameraWork_45DD60(fr, fg, fb);

		uint32_t cur = a4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = callo(0x73C8C0, ctx, a2, a3, a4);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = callo(0x73CAE0, ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = s_73CD70(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = s_73D1F0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = a_73D770(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = a_73D9C0(ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			cur = callo(0x73DCA0, ctx, a2, a3, cur);
		else
			U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		if (U32(U32(ctx, 0x2C), 0) != 0)
			return s_73E1A0(ctx, a2, a3, cur);
		U32(ctx, 0x2C) = U32(ctx, 0x2C) + 4;
		return cur;
	}

	// 0x73CD70 (module 095): draws the textured-triangle list of render context a1 (count, then
	// records of 0x14 bytes: code/colour, 3 vertex indices, uv0+clut, uv1+tpage | uv2): RTPT, rejects
	// on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth cue (bit
	// 0x40), emits one POLY_FT3 (0x20 B) per face into OT a2 at (OTZ + bias) >> a3; with a UV scroll
	// (+0x18/+0x1A) the poly is bracketed by two 0xE2 texture-window prims (0xC B each).
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl s_73CD70(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		// field pointers of the current packet (the original keeps them in stack slots; always pkt + const)
		uint32_t pSXY2 = pkt + 0x18;
		uint32_t pSXY1 = pkt + 0x10;
		uint32_t pCode = pkt + 4;
		uint32_t pSXY0 = pkt + 8;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x7000000;  // tag: 7 words
				U32(pCode, 0) = code;
				if (flags & 1)
					U32(pCode, 0) = code | 0x2000000;  // semi-transparent on
				if (flags & 4)
					U32(pCode, 0) = U32(pCode, 0) & 0xFDFFFFFFu;  // semi-transparent off
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
				x::GTE_ReadSXY012_Split(pSXY0, pSXY1, pSXY2);
				x::GTE_AVSZ3();
				int16_t v = S16(pSXY0, 0);
				if (v < 0 || v > 0xA00)
					clip = 1;
				v = S16(pSXY1, 0);
				if (v < 0 || v > 0xA00)
					clip |= 2;
				v = S16(pSXY2, 0);
				if (v < 0 || v > 0xA00)
					clip |= 4;
				v = S16(pkt, 0xA);
				if (v < 0 || v > 0x6C0)
					clip |= 0x10;
				v = S16(pkt, 0x12);
				if (v < 0 || v > 0x6C0)
					clip |= 0x20;
				v = S16(pkt, 0x1A);
				if (v < 0 || v > 0x6C0)
					clip |= 0x40;
				if ((clip & 7) == 7)
					goto next;
				if ((clip & 0x70) == 0x70)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
			{
				// depth cue of the packet colour towards the far colour
				x::set_unk_1CA8A28(pCode);
				x::set_dword_1CA8A30(U32(ctx, 0xC));
				x::sub_45F270();
				x::set_param_with_dword_1CA8A68(pCode);
			}
			{
				const int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
				S32(ctx, 0x38) = z;
				if (z < 0)
					S32(ctx, 0x38) = 0;
				const uint32_t ot = a2 + (uint32_t)(S32(ctx, 0x38) >> (a3 & 31)) * 4u;
				const uint16_t add0 = U16(ctx, 0x18);
				const uint16_t add1 = U16(ctx, 0x1A);
				if ((uint16_t)(add0 | add1) == 0)
				{
					x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
					pSXY0 += 0x20;
					pkt += 0x20;
					pSXY1 += 0x20;
					pSXY2 += 0x20;
					pCode += 0x20;
				}
				else
				{
					if (add0 != 0)
					{
						const uint32_t a = add0;
						const uint32_t c2 = U8(pkt, 0x1C) + a;
						const uint32_t c1 = U8(pkt, 0x14) + a;
						const uint32_t c0 = U8(pkt, 0xC) + a;
						if ((int32_t)(c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x28);
							U8(pkt, 0xC) = (uint8_t)(c0 - s);
							U8(pkt, 0x14) = (uint8_t)(c1 - s);
							U8(pkt, 0x1C) = (uint8_t)(c2 - s);
						}
						else
						{
							U8(pkt, 0xC) = (uint8_t)c0;
							U8(pkt, 0x14) = (uint8_t)c1;
							U8(pkt, 0x1C) = (uint8_t)c2;
						}
					}
					const uint16_t add1b = U16(ctx, 0x1A);  // re-read (0x73CFDF)
					if (add1b != 0)
					{
						const uint32_t a = add1b;
						const uint32_t c2 = U8(pkt, 0x1D) + a;
						const uint32_t c1 = U8(pkt, 0x15) + a;
						const uint32_t c0 = U8(pkt, 0xD) + a;
						if ((int32_t)(c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x2A);
							U8(pkt, 0xD) = (uint8_t)(c0 - s);
							U8(pkt, 0x15) = (uint8_t)(c1 - s);
							U8(pkt, 0x1D) = (uint8_t)(c2 - s);
						}
						else
						{
							U8(pkt, 0xD) = (uint8_t)c0;
							U8(pkt, 0x15) = (uint8_t)c1;
							U8(pkt, 0x1D) = (uint8_t)c2;
						}
					}
					const uint32_t poly = pkt;
					pSXY1 += 0x2C;
					pSXY2 += 0x2C;
					pCode += 0x2C;
					pSXY0 += 0x2C;
					uint32_t prim = pkt + 0x20;
					pkt += 0x2C;
					U32(prim, 0) = 0x2000000;
					uint32_t tw = 0;
					if (ctx + 0x1C != 0)
						tw = s2_twin(ctx, 0x1C);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
					pSXY0 += 0xC;
					pSXY1 += 0xC;
					prim = pkt;
					pkt += 0xC;
					pSXY2 += 0xC;
					pCode += 0xC;
					U32(prim, 0) = 0x2000000;
					tw = 0;
					if (ctx + 0x24 != 0)
						tw = s2_twin(ctx, 0x24);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
				}
			}
		next:
			rec += 0x14;
			--count;
		} while (count != 0);
		U32(ctx, 0x2C) = rec;
		return pkt;
	}

	// 0x73D1F0 (module 095): draws the textured-quad list of render context a1 (count, then records
	// of 0x18 bytes: code/colour, 4 vertex indices, uv0+clut, uv1+tpage, uv2|uv3<<16): RTPT + RTPS,
	// rejects on GTE FLAG / back-face (unless ctx+0x14 bit 0x10) / fully off-screen, optional depth
	// cue (bit 0x40), emits one POLY_FT4 (0x28 B) per face into OT a2 at (OTZ(AVSZ4) + bias) >> a3;
	// with a UV scroll (+0x18 = du, +0x1A = dv) the poly is bracketed by two 0xE2 texture-window prims.
	// Returns the new packet cursor (starting from a4).
	uint32_t __cdecl s_73D1F0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
		uint32_t pSXY3 = pkt + 0x20;
		uint32_t pSXY2 = pkt + 0x18;
		uint32_t pSXY1 = pkt + 0x10;
		uint32_t pCode = pkt + 4;
		uint32_t pSXY0 = pkt + 8;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4u, vbase + (uint32_t)U16(rec, 6) * 4u,
				vbase + (uint32_t)U16(rec, 8) * 4u);
			x::GTE_RTPT();
			{
				const uint32_t flags = U32(ctx, 0x14);
				const uint32_t code = U32(rec, 0);
				U32(pkt, 0) = 0x9000000;  // tag: 9 words
				U32(pCode, 0) = code;
				if (flags & 1)
					U32(pCode, 0) = code | 0x2000000;
				if (flags & 4)
					U32(pCode, 0) = U32(pCode, 0) & 0xFDFFFFFFu;
				U32(pkt, 0xC) = U32(rec, 0xC);    // uv0 + clut
				const uint32_t uv1 = U32(rec, 0x10);
				const uint32_t uv23 = U32(rec, 0x14);
				U32(pkt, 0x1C) = uv23;             // uv2 (+ uv3 in the pad half)
				U32(pkt, 0x14) = uv1;              // uv1 + tpage
				U32(pkt, 0x24) = uv23 >> 16;       // uv3
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
				x::GTE_ReadSXY012_Split(pSXY0, pSXY1, pSXY2);
				x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0xA) * 4u);
				x::GTE_RTPS();
				int16_t v = S16(pSXY0, 0);
				if (v < 0 || v > 0xA00)
					clip = 1;
				v = S16(pSXY1, 0);
				if (v < 0 || v > 0xA00)
					clip |= 2;
				v = S16(pSXY2, 0);
				if (v < 0 || v > 0xA00)
					clip |= 4;
				v = S16(pkt, 0xA);
				if (v < 0 || v > 0x6C0)
					clip |= 0x10;
				v = S16(pkt, 0x12);
				if (v < 0 || v > 0x6C0)
					clip |= 0x20;
				v = S16(pkt, 0x1A);
				if (v < 0 || v > 0x6C0)
					clip |= 0x40;
				x::GTE_ReadSXY2(pSXY3);
				x::GTE_AVSZ4();
				v = S16(pSXY3, 0);
				if (v < 0 || v > 0xA00)
					clip |= 8;
				v = S16(pkt, 0x22);
				if (v < 0 || v > 0x6C0)
					clip |= 0x80;
				if ((clip & 0xF) == 0xF)
					goto next;
				if ((clip & 0xF0) == 0xF0)
					goto next;
			}
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x40)
			{
				x::set_unk_1CA8A28(pCode);
				x::set_dword_1CA8A30(U32(ctx, 0xC));
				x::sub_45F270();
				x::set_param_with_dword_1CA8A68(pCode);
			}
			{
				const int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
				S32(ctx, 0x38) = z;
				if (z < 0)
					S32(ctx, 0x38) = 0;
				const uint32_t ot = a2 + (uint32_t)(S32(ctx, 0x38) >> (a3 & 31)) * 4u;
				const uint16_t add0 = U16(ctx, 0x18);
				const uint16_t add1 = U16(ctx, 0x1A);
				if ((uint16_t)(add0 | add1) == 0)
				{
					x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
					pSXY0 += 0x28;
					pSXY1 += 0x28;
					pkt += 0x28;
					pSXY2 += 0x28;
					pSXY3 += 0x28;
					pCode += 0x28;
				}
				else
				{
					if (add0 != 0)
					{
						const uint32_t a = add0;
						const uint32_t c0 = U8(pkt, 0xC) + a;
						const uint32_t c1 = U8(pkt, 0x14) + a;
						const uint32_t c2 = U8(pkt, 0x1C) + a;
						const uint32_t c3 = U8(pkt, 0x24) + a;
						if ((int32_t)(c3 | c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x28);
							U8(pkt, 0xC) = (uint8_t)(c0 - s);
							U8(pkt, 0x14) = (uint8_t)(c1 - s);
							U8(pkt, 0x1C) = (uint8_t)(c2 - s);
							U8(pkt, 0x24) = (uint8_t)(c3 - s);
						}
						else
						{
							U8(pkt, 0x14) = (uint8_t)c1;
							U8(pkt, 0xC) = (uint8_t)c0;
							U8(pkt, 0x1C) = (uint8_t)c2;
							U8(pkt, 0x24) = (uint8_t)c3;
						}
					}
					const uint16_t add1b = U16(ctx, 0x1A);  // re-read (0x73D508)
					if (add1b != 0)
					{
						const uint32_t a = add1b;
						const uint32_t c0 = U8(pkt, 0xD) + a;
						const uint32_t c1 = U8(pkt, 0x15) + a;
						const uint32_t c2 = U8(pkt, 0x1D) + a;
						const uint32_t c3 = U8(pkt, 0x25) + a;
						if ((int32_t)(c3 | c2 | c1 | c0) > 0xFF)
						{
							const uint8_t s = U8(ctx, 0x2A);
							U8(pkt, 0xD) = (uint8_t)(c0 - s);
							U8(pkt, 0x15) = (uint8_t)(c1 - s);
							U8(pkt, 0x1D) = (uint8_t)(c2 - s);
							U8(pkt, 0x25) = (uint8_t)(c3 - s);
						}
						else
						{
							U8(pkt, 0x1D) = (uint8_t)c2;
							U8(pkt, 0xD) = (uint8_t)c0;
							U8(pkt, 0x15) = (uint8_t)c1;
							U8(pkt, 0x25) = (uint8_t)c3;
						}
					}
					pSXY0 += 0x34;
					pSXY3 += 0x34;
					pSXY2 += 0x34;
					const uint32_t poly = pkt;
					pCode += 0x34;
					pSXY1 += 0x34;
					uint32_t prim = pkt + 0x28;
					pkt += 0x34;
					U32(prim, 0) = 0x2000000;
					uint32_t tw = 0;
					if (ctx + 0x1C != 0)
						tw = s2_twin(ctx, 0x1C);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
					x::SSIGPU_InsertPrimAutoDepth(ot, poly);
					pSXY0 += 0xC;
					pSXY1 += 0xC;
					pSXY2 += 0xC;
					prim = pkt;
					pkt += 0xC;
					pSXY3 += 0xC;
					pCode += 0xC;
					U32(prim, 0) = 0x2000000;
					tw = 0;
					if (ctx + 0x24 != 0)
						tw = s2_twin(ctx, 0x24);
					U32(prim, 4) = tw;
					U32(prim, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ot, prim);
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
	// part s3
	// ====================================================================================
	// Siren (MAG 095) module functions, part s3: the textured-quad primitive emitter of the prim-model
	// player callback, the matrix/vector table copies of that callback, the four Siren model tasks
	// (0x73EE50 / 0x73EF60 / 0x73F250 / 0x73F6A0) with their state functions, the loose state
	// functions of other Siren tasks, and the particle-system update front (0x740470 ...).
	//
	// Model task node fields used here (beyond the common head):
	//   +0x1C/+0x1E/+0x20 s16 position (x, y, z), +0x50/+0x54/+0x58 scale (x, y, z, 4.12),
	//   +0x60/+0x62/+0x64 rotation angles (read by a_73C280), +0x74 model data, +0x78 layout arg,
	//   +0x7C alpha / blend level, +0x88 u16, +0x8C u16 animated byte (7-bit wrap),
	//   +0x94 prim player layout (Effect_DecodeModelPrimLayout / MAG_011_sub_701970).
	namespace
	{
		// PSX texture-window word (GP0 0xE2) built from a {u8 x, pad, u8 y, pad, u16 w, u16 h}
		// block at ctx+o, exactly as 0x73E625..0x73E664 / 0x73E6EF..0x73E72E compute it
		uint32_t s3_texwin(uint32_t ctx, int32_t o)
		{
			uint32_t c = ((uint32_t)U8(ctx, o + 2) & 0xF8) | 0xFFFE2000u;
			c = (c << 5) | ((uint32_t)U8(ctx, o) & 0xF8);
			c = (c << 5) | (~((uint32_t)U16(ctx, o + 6) - 1u) & 0xF8);
			c = (c << 2) | ((~((uint32_t)U16(ctx, o + 4) - 1u) >> 3) & 0x1F);
			return c;
		}

		// 0x73E4A0..0x73E50B / 0x73E517..0x73E582: add d to the 4 texcoord bytes pkt+o, +o+0xC,
		// +o+0x18, +o+0x24 (u or v of the 4 corners); when any sum exceeds 0xFF all four are
		// wrapped back by the window size byte at ctx+wo
		void s3_scroll_uv(uint32_t pkt, int32_t o, uint32_t d, uint32_t ctx, int32_t wo)
		{
			int32_t a = (int32_t)(U8(pkt, o) + d);
			int32_t c = (int32_t)(U8(pkt, o + 0xC) + d);
			int32_t e = (int32_t)(U8(pkt, o + 0x18) + d);
			int32_t b = (int32_t)(U8(pkt, o + 0x24) + d);
			if ((b | e | c | a) > 0xFF)
			{
				uint8_t w = U8(ctx, wo);
				U8(pkt, o) = (uint8_t)(a - w);
				U8(pkt, o + 0xC) = (uint8_t)(c - w);
				U8(pkt, o + 0x18) = (uint8_t)(e - w);
				U8(pkt, o + 0x24) = (uint8_t)(b - w);
			}
			else
			{
				U8(pkt, o) = (uint8_t)a;
				U8(pkt, o + 0xC) = (uint8_t)c;
				U8(pkt, o + 0x18) = (uint8_t)e;
				U8(pkt, o + 0x24) = (uint8_t)b;
			}
		}

		inline bool s3_off(int16_t v, int16_t lim) { return v < 0 || v > lim; }

		// one iteration of the 0x73E20F loop: projects one quad record (0x24 bytes at data) into the
		// POLY_GT4 at pkt; returns the packet cursor after what it emitted (pkt itself when culled)
		uint32_t s3_quad(uint32_t ctx, uint32_t data, uint32_t vbase, uint32_t ot_base, uint32_t shift, uint32_t pkt)
		{
			x::GTE_LoadV012(vbase + ((uint32_t)U16(data, 4) << 2), vbase + ((uint32_t)U16(data, 6) << 2), vbase + ((uint32_t)U16(data, 8) << 2));
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x14);
			uint32_t col0 = U32(data, 0);
			U32(pkt, 0) = 0x0C000000;  // tag: 12 words
			U32(pkt, 4) = col0;        // rgb0 + code
			if (flags & 2)
			{
				col0 |= 0x2000000;     // semi-transparent
				U32(pkt, 4) = col0;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			uint32_t uv0 = U32(data, 0xC);
			uint32_t uv1 = U32(data, 0x10);
			U32(pkt, 0xC) = uv0;           // uv0 + clut
			uint32_t uv23 = U32(data, 0x14);
			U32(pkt, 0x24) = uv23;         // uv2 (the pad half gets the uv3 bits)
			U32(pkt, 0x18) = uv1;          // uv1 + tpage
			U32(pkt, 0x30) = uv23 >> 16;   // uv3
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (U32(ctx, 0x3C) & 0x60000)
				return pkt;
			x::GTE_NCLIP();
			uint32_t clip = 0;
			x::GTE_ReadMAC0(ctx + 0x30);
			if (S32(ctx, 0x30) < 0 && !(U8(ctx, 0x14) & 0x20))
				return pkt;  // back face, not double-sided
			x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
			x::GTE_LoadV0(vbase + ((uint32_t)U16(data, 0xA) << 2));
			x::GTE_RTPS();
			if (s3_off(S16(pkt, 0x8), 0xA00)) clip = 1;
			if (s3_off(S16(pkt, 0x14), 0xA00)) clip |= 2;
			if (s3_off(S16(pkt, 0x20), 0xA00)) clip |= 4;
			if (s3_off(S16(pkt, 0xA), 0x6C0)) clip |= 0x10;
			if (s3_off(S16(pkt, 0x16), 0x6C0)) clip |= 0x20;
			if (s3_off(S16(pkt, 0x22), 0x6C0)) clip |= 0x40;
			x::GTE_ReadSXY2(pkt + 0x2C);
			x::GTE_AVSZ4();
			uint32_t c = clip;
			if (s3_off(S16(pkt, 0x2C), 0xA00)) c |= 8;
			if (s3_off(S16(pkt, 0x2E), 0x6C0)) c |= 0x80;
			if ((c & 0xF) == 0xF)
				return pkt;  // all 4 x off screen
			if ((c & 0xF0) == 0xF0)
				return pkt;  // all 4 y off screen
			x::GTE_ReadOTZ(ctx + 0x38);
			if (U8(ctx, 0x14) & 0x80)
			{
				// depth-cued colours: rgb1..3 from the record's 3 colours, rgb0 from the packet
				x::sub_45E120(data + 0x18, data + 0x1C, data + 0x20);
				x::set_dword_1CA8A30(U32(ctx, 0xC));
				x::sub_45F4C0();
				x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 0x28);
				x::set_unk_1CA8A28(pkt + 4);
				x::sub_45F270();
				x::set_param_with_dword_1CA8A68(pkt + 4);
			}
			else
			{
				uint32_t c1 = U32(data, 0x18);
				uint32_t c2 = U32(data, 0x1C);
				U32(pkt, 0x10) = c1;
				uint32_t c3 = U32(data, 0x20);
				U32(pkt, 0x1C) = c2;
				U32(pkt, 0x28) = c3;
			}
			int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));  // OTZ + bias
			S32(ctx, 0x38) = z;
			if (z < 0)
				S32(ctx, 0x38) = 0;
			uint32_t ot = ot_base + ((uint32_t)(S32(ctx, 0x38) >> (shift & 31)) << 2);
			uint16_t du = U16(ctx, 0x18);
			uint16_t dv = U16(ctx, 0x1A);
			if ((uint16_t)(du | dv) == 0)
			{
				x::SSIGPU_InsertPrimAutoDepth(ot, pkt);
				return pkt + 0x34;
			}
			if (du != 0)
				s3_scroll_uv(pkt, 0xC, du, ctx, 0x28);
			uint16_t dv2 = U16(ctx, 0x1A);
			if (dv2 != 0)
				s3_scroll_uv(pkt, 0xD, dv2, ctx, 0x2A);
			// scrolling quad: texture window A, the quad, texture window B (OT prepends, so
			// the GPU sees B, quad, A)
			uint32_t prim = pkt;
			uint32_t twa = pkt + 0x34;
			U32(twa, 0) = 0x2000000;
			uint32_t w = (ctx + 0x1C != 0) ? s3_texwin(ctx, 0x1C) : 0;
			U32(twa, 4) = w;
			U32(twa, 8) = 0;
			x::SSIGPU_InsertPrimAutoDepth(ot, twa);
			x::SSIGPU_InsertPrimAutoDepth(ot, prim);
			uint32_t twb = pkt + 0x40;
			U32(twb, 0) = 0x2000000;
			w = (ctx + 0x24 != 0) ? s3_texwin(ctx, 0x24) : 0;
			U32(twb, 4) = w;
			U32(twb, 8) = 0;
			x::SSIGPU_InsertPrimAutoDepth(ot, twb);
			return pkt + 0x4C;
		}

		// common task epilogue of the four model tasks (after the state call)
		inline uint32_t s3_task_end(uint32_t n, uint8_t status)
		{
			if ((status & 1) && U8(n, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(n);
				return 2;
			}
			return 0;
		}
	}

	// 0x73E1A0 (module 095): quad-list emitter (prim player object callback): for each 0x24-byte quad
	// record at ctx+0x2C (count first) RTPT/RTPS-projects its 4 vertices (ctx+4 vertex table), culls
	// (GTE FLAG, NCLIP unless ctx+0x14 bit 0x20, all-off-screen), writes a POLY_GT4 at a4 with
	// optional depth-cued colours, inserts it in OT a2 at ((OTZ + ctx+0x10) >> a3); with UV scroll
	// (ctx+0x18/0x1A) the quad is wrapped in two texture-window prims. Returns the new packet cursor.
	uint32_t __cdecl s_73E1A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t data = U32(ctx, 0x2C);
		int32_t count = S32(data, 0);
		data += 4;
		// quirk 0x73E1C2: the vertex base overwrites the caller's a4 argument slot and 0x73E299
		// reuses the a1 slot as the clip-flag local (caller stack only, not reproducible/needed)
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x2C) = data;
		if (count <= 0)
		{
			U32(ctx, 0x2C) = data;
			return pkt;
		}
		do
		{
			pkt = s3_quad(ctx, data, vbase, a2, a3, pkt);
			data += 0x24;
		} while (--count);
		U32(ctx, 0x2C) = data;
		return pkt;
	}

	// 0x73E7F0 (module 095): copies 25 dword pairs of the object record at [a1+4] (+0x88.. step 0x10,
	// last pair +0x200) to the table 0x258BAA0, then (tail 0x73EA60) transforms the vector 0x258BB00
	// by the matrix of angle 0x800 scaled by 0x258EC60 and translated by (0x258FB80, 0x258FB82,
	// [0x1533010]+0xC + 0x258FB84 - 0x1000) and stores IR1..3 to 0x258FB60
	uint32_t __cdecl s_73E7F0(uint32_t a1)
	{
		const uint32_t src = U32(a1, 4);
		for (int i = 0; i < 24; i++)
		{
			MEM<uint32_t>(0x258BAA0 + 8 * i) = U32(src, 0x88 + 0x10 * i);
			MEM<uint32_t>(0x258BAA4 + 8 * i) = U32(src, 0x8C + 0x10 * i);
		}
		// quirk 0x73EA31: the last pair is +0x200/+0x204, not +0x208/+0x20C
		MEM<uint32_t>(0x258BB60) = U32(src, 0x200);
		MEM<uint32_t>(0x258BB64) = U32(src, 0x204);

		// 0x73EA60: stack: +0 vector (8 bytes), +8 Mat4x3 (0x20 bytes)
		alignas(4) uint8_t loc[0x28];
		const uint32_t L = P(loc);
		U32(L, 0) = MEM<uint32_t>(0x258BB00);
		U32(L, 4) = MEM<uint32_t>(0x258BB04);
		x::MAG_022_sub_8DD770(L + 8);
		x::MAG_022_sub_8DD8A0(L + 8, 0x800);
		x::scale3DMatrix(L + 8, 0x258EC60);
		int32_t tx = MEM<int16_t>(0x258FB80);
		int32_t ty = MEM<int16_t>(0x258FB82);
		uint32_t g = MEM<uint32_t>(0x1533010);
		S32(L, 0x1C) = tx;
		S32(L, 0x20) = ty;
		int32_t gz = S16(g, 0xC);
		int32_t oz = MEM<int16_t>(0x258FB84);
		S32(L, 0x24) = add32(add32(gz, oz), -0x1000);
		x::GTE_SetRotMatrix_W(L + 8);
		x::GTE_SetTransVector_W(L + 8);
		x::GTE_LoadV0(L);
		x::GTE_MVMVA_RotV0_Tr_2();
		x::GTE_StoreIR123(0x258FB60);
		return 0; // void
	}

	// 0x73EB10 (module 095): copies the object record at [a1+4] into two tables: 16 dword pairs
	// from +0x48 (step 0x10) + the pair +0/+4 to 0x258E9A0, and 16 pairs from +0x40 + the pair
	// +8/+0xC to 0x258E8A0
	uint32_t __cdecl s_73EB10(uint32_t a1)
	{
		const uint32_t src = U32(a1, 4);
		for (int i = 0; i < 16; i++)
		{
			MEM<uint32_t>(0x258E9A0 + 8 * i) = U32(src, 0x48 + 0x10 * i);
			MEM<uint32_t>(0x258E9A4 + 8 * i) = U32(src, 0x4C + 0x10 * i);
		}
		MEM<uint32_t>(0x258EA20) = U32(src, 0);
		MEM<uint32_t>(0x258EA24) = U32(src, 4);
		for (int i = 0; i < 16; i++)
		{
			MEM<uint32_t>(0x258E8A0 + 8 * i) = U32(src, 0x40 + 0x10 * i);
			MEM<uint32_t>(0x258E8A4 + 8 * i) = U32(src, 0x44 + 0x10 * i);
		}
		MEM<uint32_t>(0x258E920) = U32(src, 8);
		MEM<uint32_t>(0x258E924) = U32(src, 0xC);
		return 0; // void
	}

	// 0x73EE10 (module 095): state of task 0x73C1A0: once the GF phase reaches 2 marks the node
	// finished and advances the state, else draws/advances the prim model (a_73C280)
	uint32_t __cdecl s_73EE10(uint32_t a1)
	{
		if (a_73B7E0(2))
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
			return 0; // void
		}
		a_73C280(a1);
		return 0; // void
	}

	// 0x73EE50 (module 095): TASK, model task A: state table {0x73EEC0, 0x73EF20, 0x73EF50};
	// then +0x8C = (+0x8C - 4) & 0x7F, frame counter++, ends when finished and no children
	uint32_t __cdecl s_73EE50(uint32_t a1)
	{
		const uint32_t tab[3] = { 0x73EEC0, 0x73EF20, 0x73EF50 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t b = U8(a1, 0x8C);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		U16(a1, 0x8C) = (uint16_t)((uint8_t)(b - 4) & 0x7F);
		return s3_task_end(a1, status);
	}

	// 0x73EEC0 (module 095): state 0 of task 0x73EE50: decodes the prim layout, places the model at
	// the GF position ([0x1533010]+8/+0xC, y forced 0, z - 0x1000), scale 1.0, draws, next state
	uint32_t __cdecl s_73EEC0(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		uint32_t g = MEM<uint32_t>(0x1533010);
		uint32_t p0 = U32(g, 8);
		uint32_t p1 = U32(g, 0xC);
		U32(a1, 0x1C) = p0;
		U32(a1, 0x20) = p1;
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + 0xF000);
		U16(a1, 0x1E) = 0;
		U32(a1, 0x58) = 0x1000;
		U32(a1, 0x54) = 0x1000;
		U32(a1, 0x50) = 0x1000;
		a_73C280(a1);
		U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73EF20 (module 095): state 1 of task 0x73EE50: finishes at GF phase 3, else draws the model
	uint32_t __cdecl s_73EF20(uint32_t a1)
	{
		if (a_73B7E0(3))
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
			return 0; // void
		}
		a_73C280(a1);
		return 0; // void
	}

	// 0x73EF60 (module 095): TASK, model task B: state table {0x73EFE0, 0x73F030, 0x73F070,
	// 0x73F0C0, 0x73F100}; then +0x8C = (+0x8C - 1) & 0x7F, frame counter++, ends when finished
	uint32_t __cdecl s_73EF60(uint32_t a1)
	{
		const uint32_t tab[5] = { 0x73EFE0, 0x73F030, 0x73F070, 0x73F0C0, 0x73F100 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t b = U8(a1, 0x8C);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		U16(a1, 0x8C) = (uint16_t)((uint8_t)(b - 1) & 0x7F);
		return s3_task_end(a1, status);
	}

	// 0x73EFE0 (module 095): state 0 of task 0x73EF60: decodes the prim layout, position (0,0,0),
	// scale 1.0, draws, next state
	uint32_t __cdecl s_73EFE0(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U16(a1, 0x1C) = 0;
		U16(a1, 0x1E) = 0;
		U16(a1, 0x20) = 0;
		U32(a1, 0x58) = 0x1000;
		U32(a1, 0x54) = 0x1000;
		U32(a1, 0x50) = 0x1000;
		a_73C280(a1);
		U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F030 (module 095): state 1 of task 0x73EF60: draws; model ended -> finished; else next
	// state at GF phase 5
	uint32_t __cdecl s_73F030(uint32_t a1)
	{
		if (a_73C280(a1) == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
			return 0; // void
		}
		if (a_73B7E0(5))
			U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F070 (module 095): state 2 of task 0x73EF60: +0x7C += 0x100 (max 0x800), draws; model
	// ended -> finished; else next state at GF phase 8
	uint32_t __cdecl s_73F070(uint32_t a1)
	{
		int32_t v = add32(S32(a1, 0x7C), 0x100);
		S32(a1, 0x7C) = v;
		if (v >= 0x800)
			S32(a1, 0x7C) = 0x800;
		if (a_73C280(a1) == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
			return 0; // void
		}
		if (a_73B7E0(8))
			U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F0C0 (module 095): state 3 of task 0x73EF60: draws; while the model runs +0x7C += 0x100;
	// finished when the model ended or +0x7C reached 0x1000 (clamped)
	uint32_t __cdecl s_73F0C0(uint32_t a1)
	{
		if (a_73C280(a1) != 0)
		{
			int32_t v = add32(S32(a1, 0x7C), 0x100);
			S32(a1, 0x7C) = v;
			if (v < 0x1000)
				return 0; // void
			S32(a1, 0x7C) = 0x1000;
		}
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F180 (module 095): state 0 of task 0x73F110: decodes the prim layout, scale 3.0, position
	// (0, -0x800, 0xCC0), +0x88 = 0xFF80, next state (no draw)
	uint32_t __cdecl s_73F180(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U32(a1, 0x58) = 0x3000;
		U32(a1, 0x54) = 0x3000;
		U32(a1, 0x50) = 0x3000;
		uint8_t st = (uint8_t)(U8(a1, 0x29) + 1);
		U16(a1, 0x1C) = 0;
		U16(a1, 0x1E) = 0xF800;
		U16(a1, 0x20) = 0xCC0;
		U16(a1, 0x88) = 0xFF80;
		U8(a1, 0x29) = st;
		return 0; // void
	}

	// 0x73F1D0 (module 095): wait state of task 0x73F110: next state at frame 0x18
	uint32_t __cdecl s_73F1D0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x18)
			U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F1E0 (module 095): state of task 0x73F110: at GF phase 4 sets y = -0x400, scale 0x600,
	// next state; always draws the model
	uint32_t __cdecl s_73F1E0(uint32_t a1)
	{
		if (a_73B7E0(4))
		{
			U16(a1, 0x1E) = 0xFC00;
			U32(a1, 0x58) = 0x600;
			U32(a1, 0x54) = 0x600;
			U32(a1, 0x50) = 0x600;
			U8(a1, 0x29)++;
		}
		a_73C280(a1);
		return 0; // void
	}

	// 0x73F220 (module 095): state of task 0x73F110: draws; finished when the model ended
	uint32_t __cdecl s_73F220(uint32_t a1)
	{
		if (a_73C280(a1) == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x73F250 (module 095): TASK, model task C: state table {0x73F2B0, 0x73F300, 0x73F310,
	// 0x73F360}; frame counter++, ends when finished and no children
	uint32_t __cdecl s_73F250(uint32_t a1)
	{
		const uint32_t tab[4] = { 0x73F2B0, 0x73F300, 0x73F310, 0x73F360 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return s3_task_end(a1, status);
	}

	// 0x73F2B0 (module 095): state 0 of task 0x73F250: decodes the prim layout, scale 0x600,
	// position (0, -0x400, 0xCC0), +0x88 = 0xFF80, next state (no draw)
	uint32_t __cdecl s_73F2B0(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U32(a1, 0x58) = 0x600;
		U32(a1, 0x54) = 0x600;
		U32(a1, 0x50) = 0x600;
		uint8_t st = (uint8_t)(U8(a1, 0x29) + 1);
		U16(a1, 0x1C) = 0;
		U16(a1, 0x1E) = 0xFC00;
		U16(a1, 0x20) = 0xCC0;
		U16(a1, 0x88) = 0xFF80;
		U8(a1, 0x29) = st;
		return 0; // void
	}

	// 0x73F300 (module 095): wait state of task 0x73F250: next state at frame 0x32
	uint32_t __cdecl s_73F300(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x32)
			U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F310 (module 095): state 2 of task 0x73F250: faces the camera (+0x62 = camera yaw - 0x800);
	// finished at GF phase 5 (no draw that tick) or when the drawn model ended
	uint32_t __cdecl s_73F310(uint32_t a1)
	{
		uint32_t yaw = x::sub_8DD7B0(0x1D97778);
		U16(a1, 0x62) = (uint16_t)((yaw - 0x800) & 0xFFF);
		if (a_73B7E0(5) != 0 || a_73C280(a1) == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x73F3E0 (module 095): state 0 of a model task: decodes the prim layout, scale 0x600,
	// position (0, -0x400, 0xCC0), +0x88 = 0xFF80, next state (no draw)
	uint32_t __cdecl s_73F3E0(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U32(a1, 0x58) = 0x600;
		U32(a1, 0x54) = 0x600;
		U32(a1, 0x50) = 0x600;
		uint8_t st = (uint8_t)(U8(a1, 0x29) + 1);
		U16(a1, 0x1C) = 0;
		U16(a1, 0x1E) = 0xFC00;
		U16(a1, 0x20) = 0xCC0;
		U16(a1, 0x88) = 0xFF80;
		U8(a1, 0x29) = st;
		return 0; // void
	}

	// 0x73F430 (module 095): wait state: next state at frame 0x50
	uint32_t __cdecl s_73F430(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x50)
			U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F440 (module 095): state: faces the camera (+0x62); at GF phase 5 sets scale 1.0, position
	// (0, -0x800, 0xCC0), next state; always draws the model
	uint32_t __cdecl s_73F440(uint32_t a1)
	{
		uint32_t yaw = x::sub_8DD7B0(0x1D97778);
		U16(a1, 0x62) = (uint16_t)((yaw - 0x800) & 0xFFF);
		if (a_73B7E0(5))
		{
			U16(a1, 0x1C) = 0;
			U32(a1, 0x58) = 0x1000;
			U32(a1, 0x54) = 0x1000;
			U32(a1, 0x50) = 0x1000;
			uint8_t st = (uint8_t)(U8(a1, 0x29) + 1);
			U16(a1, 0x1E) = 0xF800;
			U16(a1, 0x20) = 0xCC0;
			U8(a1, 0x29) = st;
		}
		a_73C280(a1);
		return 0; // void
	}

	// 0x73F4A0 (module 095): state: faces the camera (+0x62); finished at GF phase 6 (no draw that
	// tick) or when the drawn model ended
	uint32_t __cdecl s_73F4A0(uint32_t a1)
	{
		uint32_t yaw = x::sub_8DD7B0(0x1D97778);
		U16(a1, 0x62) = (uint16_t)((yaw - 0x800) & 0xFFF);
		if (a_73B7E0(6) != 0 || a_73C280(a1) == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x73F570 (module 095): state 0 of a model task: decodes the prim layout, scale 1.0,
	// position (0, -0x800, 0xCC0), +0x88 = 0xFF80, next state (no draw)
	uint32_t __cdecl s_73F570(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U32(a1, 0x58) = 0x1000;
		U32(a1, 0x54) = 0x1000;
		U32(a1, 0x50) = 0x1000;
		uint8_t st = (uint8_t)(U8(a1, 0x29) + 1);
		U16(a1, 0x1C) = 0;
		U16(a1, 0x1E) = 0xF800;
		U16(a1, 0x20) = 0xCC0;
		U16(a1, 0x88) = 0xFF80;
		U8(a1, 0x29) = st;
		return 0; // void
	}

	// 0x73F5C0 (module 095): wait state: next state at frame 0x82
	uint32_t __cdecl s_73F5C0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x82)
			U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F5D0 (module 095): state: faces the camera (+0x62); at GF phase 6 moves the model to the
	// GF position ([0x1533010]+8/+0xC, y = -0x600), scale 2.0, next state; always draws the model
	uint32_t __cdecl s_73F5D0(uint32_t a1)
	{
		uint32_t yaw = x::sub_8DD7B0(0x1D97778);
		U16(a1, 0x62) = (uint16_t)((yaw - 0x800) & 0xFFF);
		if (a_73B7E0(6))
		{
			uint32_t g = MEM<uint32_t>(0x1533010);
			uint32_t p0 = U32(g, 8);
			uint32_t p1 = U32(g, 0xC);
			U32(a1, 0x1C) = p0;
			U32(a1, 0x58) = 0x2000;
			U32(a1, 0x54) = 0x2000;
			U32(a1, 0x50) = 0x2000;
			uint8_t st = (uint8_t)(U8(a1, 0x29) + 1);
			U32(a1, 0x20) = p1;
			U16(a1, 0x1E) = 0xFA00;
			U8(a1, 0x29) = st;
		}
		a_73C280(a1);
		return 0; // void
	}

	// 0x73F640 (module 095): state: faces the camera (+0x62); finished at GF phase 9 (no draw that
	// tick) or when the drawn model ended
	uint32_t __cdecl s_73F640(uint32_t a1)
	{
		uint32_t yaw = x::sub_8DD7B0(0x1D97778);
		U16(a1, 0x62) = (uint16_t)((yaw - 0x800) & 0xFFF);
		if (a_73B7E0(9) != 0 || a_73C280(a1) == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x73F6A0 (module 095): TASK, model task D: state table {0x73F710, 0x73F760, 0x73F770,
	// 0x73F790, 0x73F7C0, 0x73F7F0}; frame counter++, ends when finished and no children
	uint32_t __cdecl s_73F6A0(uint32_t a1)
	{
		const uint32_t tab[6] = { 0x73F710, 0x73F760, 0x73F770, 0x73F790, 0x73F7C0, 0x73F7F0 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24)++;
		return s3_task_end(a1, status);
	}

	// 0x73F710 (module 095): state 0 of task 0x73F6A0: decodes the prim layout, position (0,0,0),
	// scale 1.0, next state (no draw)
	uint32_t __cdecl s_73F710(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U16(a1, 0x1C) = 0;
		U16(a1, 0x1E) = 0;
		U16(a1, 0x20) = 0;
		U32(a1, 0x58) = 0x1000;
		U32(a1, 0x54) = 0x1000;
		U32(a1, 0x50) = 0x1000;
		U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F760 (module 095): state 1 of task 0x73F6A0: next state at frame 0x1E
	uint32_t __cdecl s_73F760(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x1E)
			U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F770 (module 095): state 2 of task 0x73F6A0: draws; next state when the model ended
	uint32_t __cdecl s_73F770(uint32_t a1)
	{
		if (a_73C280(a1) == 0)
			U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F790 (module 095): state 3 of task 0x73F6A0: at frame 0x89 re-decodes the prim layout
	// (restarts the model), next state
	uint32_t __cdecl s_73F790(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x89)
		{
			x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x73F7C0 (module 095): state 4 of task 0x73F6A0: draws; finished when the model ended or at
	// GF phase 6
	uint32_t __cdecl s_73F7C0(uint32_t a1)
	{
		if (a_73C280(a1) == 0 || a_73B7E0(6) != 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x73F860 (module 095): state 0 of a model task: decodes the prim layout, places the model at
	// the spawn position (GetEffectSpawnPosition mode 0xF1) of battle entity slot +0x2D, scale 1.0,
	// draws, next state
	uint32_t __cdecl s_73F860(uint32_t a1)
	{
		uint32_t slot = U8(a1, 0x2D);
		uint32_t ent = 0x1D972C0 + slot * 0x9C;
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		x::GetEffectSpawnPosition(ent, 0xF1, 0, a1 + 0x1C);
		U32(a1, 0x58) = 0x1000;
		U32(a1, 0x54) = 0x1000;
		U32(a1, 0x50) = 0x1000;
		a_73C280(a1);
		U8(a1, 0x29)++;
		return 0; // void
	}

	// 0x73F8D0 (module 095): state: draws; finished when the model ended
	uint32_t __cdecl s_73F8D0(uint32_t a1)
	{
		if (a_73C280(a1) == 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x73F960 (module 095): state: once the frame counter reaches +0x298 initialises the particle
	// system (a_73F990) and runs its first update (s_740470), next state
	uint32_t __cdecl s_73F960(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			a_73F990(a1);
			s_740470(a1);
			U8(a1, 0x29)++;
		}
		return 0; // void
	}

	// 0x740470 (module 095): particle-system tick: sets the current system 0x258FB78 = a1+0x34 and
	// runs its phase (+0x20): 0 -> phase 1; 1 -> updates/draws (a_743190, s_740840, a_740700,
	// s_740500), frame +0x18++, phase 2 when +0x18 >= +0x1A and no live particles (+0x14, +0x16);
	// 2 -> returns 1 (done). Always adds the live counts +0x14/+0x16 to 0x258FB40 / 0x258EC50.
	uint32_t __cdecl s_740470(uint32_t a1)
	{
		uint32_t sys = a1 + 0x34;
		uint32_t done = 0;
		MEM<uint32_t>(0x258FB78) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			a_743190();
			s_740840();
			a_740700();
			s_740500();
			sys = MEM<uint32_t>(0x258FB78);
			U16(sys, 0x18)++;
			if (S16(sys, 0x18) >= S16(sys, 0x1A) && U16(sys, 0x14) == 0 && U16(sys, 0x16) == 0)
				U16(sys, 0x20)++;
			break;
		case 2:
			done = 1;
			break;
		default:
			break;
		}
		uint16_t n0 = U16(sys, 0x14);
		uint16_t n1 = U16(sys, 0x16);
		MEM<uint16_t>(0x258FB40) = (uint16_t)(MEM<uint16_t>(0x258FB40) + n0);
		MEM<uint16_t>(0x258EC50) = (uint16_t)(MEM<uint16_t>(0x258EC50) + n1);
		return done;
	}

	// 0x740500 (module 095): for every emitter of the current particle system (list [0x258FB78]+0x2C,
	// next +4) of type 1 whose descriptor ([sys+0x224][s8 +0x1D6]) has kind 4 or 5 and that has a
	// +0x170 pointer, calls the original 0x740560(emitter, descriptor)
	uint32_t __cdecl s_740500(void)
	{
		uint32_t e = U32(MEM<uint32_t>(0x258FB78), 0x2C);
		if (e == 0)
			return 0; // void
		do
		{
			if (U16(e, 8) == 1)
			{
				uint32_t sys = MEM<uint32_t>(0x258FB78);
				int32_t k = S8(e, 0x1D6);
				uint32_t desc = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(desc, 0x16);
				if ((kind == 4 || kind == 5) && U32(e, 0x170) != 0)
					callo(0x740560, e, desc);
			}
			e = U32(e, 4);
		} while (e != 0);
		return 0; // void
	}

	// 0x740840 (module 095): for every emitter of the current particle system (list
	// [0x258FB78]+0x2C, next +4): type 0 -> s_741B10, type 1 -> a_740880
	uint32_t __cdecl s_740840(void)
	{
		uint32_t e = U32(MEM<uint32_t>(0x258FB78), 0x2C);
		if (e == 0)
			return 0; // void
		do
		{
			int32_t type = S16(e, 8);
			if (type == 0)
				s_741B10(e);
			else if (type == 1)
				a_740880(e);
			e = U32(e, 4);
		} while (e != 0);
		return 0; // void
	}

	// 0x741B10 (module 095): type-0 emitter update: first call inits it (a_742D00, +0x62 = 1); motion
	// by mode +0x68 (3 -> a_7424C0, 4 -> original 0x742610); spawns particles (s_741B90); age
	// +0x60++; lifetime countdown (a_741B60)
	uint32_t __cdecl s_741B10(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62)++;
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			a_7424C0(a1);
		else if (mode == 4)
			callo(0x742610, a1);
		s_741B90(a1);
		U16(a1, 0x60)++;
		return a_741B60(a1);
	}

	// 0x741B90 (module 095): particle spawn of an emitter: count = byte table (descriptor
	// [sys+0x224][s8 +0x6A] +0xC8)[age +0x60] while age <= 0x27; spawns it in batches of
	// descriptor s8 +0x36 (s_741C10(emitter, descriptor, n)), remainder last
	uint32_t __cdecl s_741B90(uint32_t a1)
	{
		uint32_t sys = MEM<uint32_t>(0x258FB78);
		uint32_t table = U32(sys, 0x224);
		int32_t k = S8(a1, 0x6A);
		uint32_t desc = U32(table, k * 4);
		int16_t age = S16(a1, 0x60);
		if (age > 0x27)
			return 0; // void
		int32_t count = U8(U32(desc, 0xC8), (int32_t)age);
		if (count == 0)
			return 0; // void
		int32_t batch = S8(desc, 0x36);
		// idiv 0x741BD0/0x741BD7 (a zero batch size faults in the original as well)
		int32_t q = count / batch;
		int32_t r = count % batch;
		if (q > 0)
		{
			do
			{
				s_741C10(a1, desc, (uint32_t)(int32_t)S8(desc, 0x36));
			} while (--q);
		}
		if (r > 0)
			s_741C10(a1, desc, (uint32_t)r);
		return 0; // void
	}

	// ====================================================================================
	// part s4
	// ====================================================================================
	// Part s4 - Siren (095) module functions 0x741C10..0x7448A0:
	//   * 0x741C10 / 0x7423F0: allocation + initialisation of a Siren "particle actor" record
	//     (0x2A0-byte slots of the pool at [0x258FB6C], ring cursor 0x258EDD0);
	//   * the sparkle tasks: four-sprite groups (node +0x6C: 4 x {x,y,z,pad} positions,
	//     +0x8C: 4 x velocities / lerp keys) drawn with the effect sprite player a_7435E0
	//     (one call per sprite, s_7434C0);
	//   * their state functions (state tables called through callp).
	// Module raw addresses: 0x257FA90 task queue, 0x1533010 Siren root/position record,
	// 0x258FB80/82/84 Siren model offset, 0x258EC60 scale vector, 0x258E9A0 / 0x258E8A0 /
	// 0x258BAA0 key tables (8-byte {x,y,z,pad} keys, lerped between key k and k+1),
	// 0x258FB68 module scratch stack pointer, 0x258FB78 module context.
	namespace
	{
		// 4-dword copy (the inline mov/mov pairs of 0x741D05 / 0x741D29)
		inline void s4_copy16(uint32_t dst, uint32_t src)
		{
			U32(dst, 0) = U32(src, 0);
			U32(dst, 4) = U32(src, 4);
			U32(dst, 8) = U32(src, 8);
			U32(dst, 12) = U32(src, 12);
		}

		// identity matrix, then rotations Z, X, Y by the three angle words at ang (+4, +0, +2),
		// each only when nonzero (re-read after every call, as the listing does)
		inline void s4_rotate_zxy(uint32_t m, uint32_t ang)
		{
			x::MAG_022_sub_8DD770(m);
			if (U16(ang, 4) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(ang, 4));
			if (U16(ang, 0) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(ang, 0));
			if (U16(ang, 2) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(ang, 2));
		}

		// 0x741E6B / 0x742029: v = sp+0x20 = (0, -0x1000, 0) rotated by the matrix at sp (IR stored back)
		inline void s4_rotate_down_vector(uint32_t sp)
		{
			uint32_t v = sp + 0x20;
			U16(sp, 0x22) = 0xF000;
			U16(sp, 0x24) = 0;
			U16(v, 0) = 0;
			x::GTE_SetRotMatrix(sp);
			x::GTE_LoadV0(v);
			x::GTE_MVMVA_RotV0();
			x::GTE_StoreIR123(v);
		}

		// the rand() % s16(sp+off) of 0x741E2D.. (only when the word is nonzero)
		inline void s4_rand_mod(uint32_t sp, int32_t off)
		{
			if (U16(sp, off) != 0)
			{
				int32_t r = (int32_t)x::CrtRand();
				U16(sp, off) = (uint16_t)(r % (int32_t)S16(sp, off));
			}
		}

		// Siren sparkle orbit matrix (0x743DFC.. / 0x7441FC.. / 0x74459C..): identity, Y rotation
		// 0x800, scaled by the vector 0x258EC60, translation = Siren model offset (0x258FB80,
		// 0x258FB82 - ty_bias, [0x1533010]+0x0C + 0x258FB84 - tz_bias); loaded as the GTE
		// rotation + translation
		inline void s4_orbit_matrix(uint32_t m, int32_t ty_bias, int32_t tz_bias)
		{
			x::MAG_022_sub_8DD770(m);
			x::MAG_022_sub_8DD8A0(m, 0x800);
			x::scale3DMatrix(m, 0x258EC60);
			int32_t tx = S16(0x258FB80, 0);
			int32_t ty = sub32(S16(0x258FB82, 0), ty_bias);
			uint32_t root = U32(0x1533010, 0);
			S32(m, 0x14) = tx;
			S32(m, 0x18) = ty;
			int32_t tz = sub32(add32(S16(root, 0x0C), S16(0x258FB84, 0)), tz_bias);
			S32(m, 0x1C) = tz;
			x::GTE_SetRotMatrix_W(m);
			x::GTE_SetTransVector_W(m);
		}

		// one component of a key lerp: key[k].c + (key[k+1].c - key[k].c) * t / 16 (16-bit result)
		inline uint16_t s4_key_lerp(uint32_t tab, int32_t k8, int32_t comp, int32_t t)
		{
			uint16_t a = U16(tab + comp, k8);
			int32_t d = sub32(S16(tab + comp + 8, k8), (int16_t)a);
			int32_t q = mul32(d, t) / 16;
			return (uint16_t)(q + a);
		}

		// body shared by the tasks 0x743DD0 / 0x7441D0 (after their state call): 4 sprites, each at
		// lerp(key table, index +0xCC+2i, t +0xD4+2i) + node offset (+0xAC+8i), through the orbit
		// matrix -> node +0x6C+8i; then draws the 4 sprites and ends like every Cure-engine task
		uint32_t s4_orbit_sprites_task(uint32_t node, uint32_t keys, int32_t ty_bias, uint32_t table_hi)
		{
			uint8_t m[0x20];
			s4_orbit_matrix(P(m), ty_bias, 0x2000);
			uint32_t pt = node + 0xD4;        // [esp+0x10]: t word pointer (index word at -8)
			uint32_t ob = node + 0xAE;        // ebx
			for (int32_t n = 4; n != 0; n--)
			{
				int32_t k8 = shl32(S16(pt, -8), 3);
				int32_t t = S16(pt, 0);
				// stack vector [esp+0x1C]; its 4th word is the high half of the state table's
				// entry 1 (0x0074xxxx) the vector overlays
				int16_t v[4];
				v[3] = (int16_t)table_hi;
				uint16_t vx = s4_key_lerp(keys, k8, 0, t);
				vx = (uint16_t)(vx + U16(ob, -2));
				uint16_t vy = s4_key_lerp(keys, k8, 2, t);
				vy = (uint16_t)(vy + U16(ob, 0));
				uint16_t vz = s4_key_lerp(keys, k8, 4, t);
				vz = (uint16_t)(vz + U16(ob, 2));
				v[0] = (int16_t)vx;
				v[1] = (int16_t)vy;
				v[2] = (int16_t)vz;
				x::GTE_LoadV0(P(v));
				x::GTE_MVMVA_RotV0_Tr_2();
				x::GTE_StoreIR123(ob - 0x42);   // node +0x6C+8i
				pt += 2;
				ob += 8;
			}
			uint32_t pos = node + 0x6C;
			for (int32_t n = 4; n != 0; n--)
			{
				U32(node, 0x1C) = U32(pos, 0);
				U32(node, 0x20) = U32(pos, 4);
				s_7434C0(node);
				pos += 8;
			}
			uint8_t status = U8(node, 0x26);
			U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
			if ((status & 1) != 0 && U8(node, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(node);
				return 2;
			}
			return 0;
		}

		// the 4-state spawner tasks 0x743C70 / 0x744060 / 0x744450: state call, ++frame, end
		uint32_t s4_state_task(uint32_t node, const uint32_t *tab)
		{
			callp(tab[S8(node, 0x29)], node);
			uint8_t status = U8(node, 0x26);
			U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
			if ((status & 1) != 0 && U8(node, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(node);
				return 2;
			}
			return 0;
		}

		// 0x743CD0 / 0x7440C0 / 0x7444B0: node pos = Siren root pos (+8..+0xF), y = 0, z += dz; next state
		inline void s4_anchor_to_root(uint32_t node, uint16_t dz)
		{
			uint32_t root = U32(0x1533010, 0);
			uint32_t d0 = U32(root, 8);
			uint32_t d1 = U32(root, 0xC);
			U32(node, 0x1C) = d0;
			U32(node, 0x20) = d1;
			uint8_t st = U8(node, 0x29);
			U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + dz);
			U16(node, 0x1E) = 0;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}

		// fills the 4 orbit keys of a sparkle node (0x743D4B / 0x74413A): index = rand % 16 (signed
		// C remainder of a non-negative value), t = rand & 15, offset (sin, cos)(rand & 0xFFF) / 32
		void s4_fill_orbit_keys(uint32_t node)
		{
			uint32_t po = node + 0xB0;   // edi
			uint32_t pk = node + 0xD4;   // esi
			for (int32_t n = 4; n != 0; n--)
			{
				int32_t r = (int32_t)x::CrtRand();
				U16(pk, -8) = (uint16_t)(r % 16);
				U16(pk, 0) = (uint16_t)(x::CrtRand() & 0xF);
				int32_t ang = (int16_t)(uint16_t)(x::CrtRand() & 0xFFF);
				int32_t s = (int32_t)x::computeSin((uint32_t)ang);
				U16(po, -2) = (uint16_t)(s / 32);
				int32_t c = (int32_t)x::computeCosine((uint32_t)ang);
				pk += 2;
				U16(po, 0) = (uint16_t)(c / 32);
				po += 8;
			}
		}

		// spawns one "rising sparkle" node (0x7448EA / 0x7449B2 bodies)
		inline uint32_t s4_entity_of(uint32_t node)
		{
			uint32_t slot = U8(node, 0x2D);
			return 0x1D972C0 + slot * 0x9C;
		}
	}

	// 0x741C10 (module Siren, sub_741C10): allocates a particle actor record (s_7423F0) for the
	// emitter a1 and initialises its a3 particles from the descriptor a2: start positions
	// (+0xDC/+0x12C, spread along a randomly rotated down vector), per-particle rotation matrices
	// (+0x0C+0x20i, rotated by the emitter matrix a1+0x2C), speeds/scales (+0x174/+0x184), colour
	// keys (+0x1E0..), then a_743100; uses 0x50 bytes of the module scratch stack [0x258FB68]
	uint32_t __cdecl s_741C10(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		uint32_t sp = U32(0x258FB68, 0) - 0x50;           // scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(0x258FB68, 0) = sp;
		// quirk 0x741C21: `movsx ax` - the high half of the pushed word is stale eax; s_7423F0
		// only reads the low byte
		uint32_t node = s_7423F0(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(0x258FB78, 0);
			int32_t k = S8(a1, 0x6A);
			uint32_t v = U32(U32(ctx, 0x228), shl32(k, 2));
			U32(node, 0x170) = v;
			U8(node, 0x1D1) = (uint8_t)a_7424B0(v);
			if (U8(desc, 0x16) == 2)
			{
				uint8_t b17 = U8(desc, 0x17);
				uint8_t b18 = U8(desc, 0x18);
				int32_t s1a = S8(desc, 0x1A);
				U8(node, 0x1D3) = b17;
				int32_t s19 = S8(desc, 0x19);
				U8(node, 0x1D4) = b18;
				U8(node, 0x1D5) = (uint8_t)a_742290((uint32_t)s19, (uint32_t)s1a);
			}
		}
		U16(node, 0x1DA) = U16(desc, 0x64);
		if ((int32_t)a3 > 0)
		{
			// (the original keeps out_speed / mat in its own argument slots a1 / a2: compiler reuse)
			uint32_t src = a1 + 0x4C;          // emitter position (16 bytes)
			uint32_t emat = a1 + 0x2C;         // emitter matrix
			uint32_t out_speed = node + 0x184;
			uint32_t mat = node + 0x0C;
			uint32_t pos = node + 0x12C;
			for (int32_t n = (int32_t)a3; n != 0; n--)
			{
				s4_copy16(pos - 0x50, src);
				if (U8(desc, 0x22) == 0)
					s4_copy16(pos, src);
				uint8_t spread = U8(desc, 0x34);
				if (spread == 1)
				{
					U16(sp, 0x28) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2A) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2C) = (uint16_t)(x::CrtRand() & 0xFFF);
					s4_rotate_zxy(sp, sp + 0x28);
					int32_t i2 = shl32(S16(sp, 0x4E), 1);
					if (U8(desc, 0x33) != 0)
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), i2);
					}
					else
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), i2);
						s4_rand_mod(sp, 0x48);
						s4_rand_mod(sp, 0x4A);
						s4_rand_mod(sp, 0x4C);
					}
					s4_rotate_down_vector(sp);
					int32_t ex = mul32(S16(sp, 0x48), S16(sp, 0x20));
					int32_t ey = mul32(S16(sp, 0x4A), S16(sp, 0x22));
					int32_t ez = mul32(S16(sp, 0x4C), S16(sp, 0x24));
					int32_t old = S32(pos, 0);
					ex = shl32(ex, 4);
					S32(sp, 0x38) = ex;
					S32(pos, 0) = add32(old, ex);
					old = S32(pos, 4);
					ey = shl32(ey, 4);
					S32(sp, 0x3C) = ey;
					S32(pos, 4) = add32(old, ey);
					old = S32(pos, 8);
					ez = shl32(ez, 4);
					S32(sp, 0x40) = ez;
					S32(pos, 8) = add32(old, ez);
				}
				else if (spread == 2)
				{
					U16(sp, 0x28) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2A) = 0;
					U16(sp, 0x2C) = 0x400;
					s4_rotate_zxy(sp, sp + 0x28);
					int32_t i2 = shl32(S16(sp, 0x4E), 1);
					if (S8(desc, 0x33) != 1)
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), i2);
						s4_rand_mod(sp, 0x48);
						s4_rand_mod(sp, 0x4A);
						s4_rand_mod(sp, 0x4C);
					}
					else
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						s4_rand_mod(sp, 0x4A);
						int32_t e = S16(sp, 0x4E);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), shl32(e, 1));
					}
					if ((x::CrtRand() & 1) != 0)
						U16(sp, 0x4A) = (uint16_t)(0 - U16(sp, 0x4A));
					s4_rotate_down_vector(sp);
					// the y offset is the raw key << 16 (the rotated vector's y is not used)
					int32_t ex = mul32(S16(sp, 0x48), S16(sp, 0x20));
					int32_t ey = shl32(S16(sp, 0x4A), 16);
					int32_t oldx = S32(pos, 0);
					S32(sp, 0x3C) = ey;
					int32_t ez = mul32(S16(sp, 0x4C), S16(sp, 0x24));
					ex = shl32(ex, 4);
					S32(sp, 0x38) = ex;
					S32(pos, 0) = add32(oldx, ex);
					int32_t ey2 = S32(sp, 0x3C);
					S32(pos, 4) = add32(S32(pos, 4), ey2);
					int32_t oldz = S32(pos, 8);
					ez = shl32(ez, 4);
					S32(sp, 0x40) = ez;
					S32(pos, 8) = add32(oldz, ez);
				}
				// 0x7420A1: particle rotation angles sp+0x30
				int32_t rmode = S8(desc, 0x21);
				if (rmode == 0)
					a_742350(sp + 0x30, desc + 0x3C, desc + 0x44);
				else if (rmode == 1)
				{
					uint32_t c0 = U32(sp, 0x28);
					uint32_t c1 = U32(sp, 0x2C);
					U32(sp, 0x30) = c0;
					U32(sp, 0x34) = c1;
				}
				s4_rotate_zxy(mat, sp + 0x30);
				// particle matrix = emitter matrix * particle matrix (column by column)
				x::GTE_SetRotMatrix(emat);
				x::GTE_LoadIRFromMatrixColumn(mat);
				x::GTE_MVMVA_RotIR();
				x::GTE_StoreIRToMatrixColumn(mat);
				x::GTE_LoadIRFromMatrixColumn(mat + 2);
				x::GTE_MVMVA_RotIR();
				x::GTE_StoreIRToMatrixColumn(mat + 2);
				x::GTE_LoadIRFromMatrixColumn(mat + 4);
				x::GTE_MVMVA_RotIR();
				x::GTE_StoreIRToMatrixColumn(mat + 4);
				if (U8(desc, 0x27) == 0)
				{
					uint32_t r1 = a_7422C0(U32(desc, 0x4C), U32(desc, 0x50));
					U32(out_speed, -0x10) = r1;
					uint32_t r2 = a_7422C0(U32(desc, 0x54), U32(desc, 0x58));
					U32(out_speed, 0) = r2;
				}
				out_speed += 4;
				pos += 0x10;
				mat += 0x20;
			}
		}
		a_742350(node + 0x194, desc + 0x148, desc + 0x150);
		uint8_t m1e = U8(desc, 0x1E);
		if (m1e == 1 || m1e == 2)
		{
			int32_t s20 = S8(desc, 0x20);
			int32_t s1f = S8(desc, 0x1F);
			U16(node, 0x1C8) = (uint16_t)a_742290((uint32_t)s1f, (uint32_t)s20);
		}
		if (U8(desc, 0x27) == 0)
		{
			U32(node, 0x1DC) = a_7422C0(U32(desc, 0x5C), U32(desc, 0x60));
			if ((int32_t)a3 > 0)
			{
				uint32_t q = node + 0x220;
				for (int32_t n = (int32_t)a3; n != 0; n--)
				{
					a_742300(q - 0x40, desc + 0x68, desc + 0x78);
					a_742300(q, desc + 0x88, desc + 0x98);
					a_742300(q + 0x40, desc + 0xA8, desc + 0xB8);
					q += 0x10;
				}
			}
		}
		a_743100(node, desc);
		U32(0x258FB68, 0) = U32(0x258FB68, 0) + 0x50;
		return 0; // void
	}

	// 0x7423F0 (module Siren, sub_7423F0): finds a free 0x2A0-byte particle actor record in the
	// pool [0x258FB6C] (74 slots probed from the ring cursor 0x258EDD0, 73-entry wrap), clears it,
	// binds it to emitter a1 (+0x1BC), kind a2 (+0x1D6), marks it used (+0x1D7), ++ctx+0x16,
	// a_742CA0(rec, 1); advances the cursor; returns the record (0 when none is free)
	uint32_t __cdecl s_7423F0(uint32_t a1, uint32_t a2)
	{
		uint32_t pool = U32(0x258FB6C, 0);
		int32_t cur = S16(0x258EDD0, 0);
		uint32_t rec = 0;
		int32_t tries = 0;
		for (;;)
		{
			if (U8(pool + (uint32_t)mul32(cur, 0x2A0), 0x1D7) == 0)
			{
				rec = (uint32_t)mul32(cur, 0x2A0) + pool;
				x::MAG_007_sub_8DCC00(rec, 0x2A0);
				uint8_t kind = (uint8_t)a2;
				U32(rec, 0x1BC) = a1;
				uint8_t b6b = U8(a1, 0x6B);
				uint32_t ctx = U32(0x258FB78, 0);
				U8(rec, 0x1D7) = 1;
				U16(ctx, 0x16) = (uint16_t)(U16(ctx, 0x16) + 1);
				U8(rec, 0x1D6) = kind;
				U8(rec, 0x1D9) = b6b;
				a_742CA0(rec, 1);
				break;
			}
			cur++;
			if (cur >= 0x49)
				cur = 0;
			tries++;
			if (tries >= 0x4A)
				break;
		}
		cur++;
		if (cur < 0x49)
			U16(0x258EDD0, 0) = (uint16_t)cur;
		else
			U16(0x258EDD0, 0) = 0;
		return rec;
	}

	// 0x743220 (module Siren, sub_743220): state: when s_740470(node) is done, marks the node
	// finished (+0x26 |= 1) and advances its state
	uint32_t __cdecl s_743220(uint32_t a1)
	{
		if (s_740470(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x7432C0 (module Siren, sub_7432C0): state: spawns a sparkle task 0x7433C0 (size 0xE0) into
	// queue 0x257FA90 with 4 sprites at random points of the target entity's bounding box
	// (entity +0x34..+0x3E, offset by its position +0x1C..+0x20) -> node +0x6C+8i; when
	// a_73B7E0(7) the node is finished and advanced
	uint32_t __cdecl s_7432C0(uint32_t a1)
	{
		uint32_t ent = s4_entity_of(a1);
		uint32_t node = x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x7433C0, 0xE0, a1);
		uint32_t p = node + 0x6E;
		for (int32_t n = 4; n != 0; n--)
		{
			uint16_t lo = U16(ent, 0x34);
			int32_t r = (int32_t)x::CrtRand();
			int32_t q = mul32(r & 0xFFF, sub32(S16(ent, 0x3A), (int16_t)lo)) / 4096;
			lo = (uint16_t)(q + lo);
			uint16_t lo_y = U16(ent, 0x36);
			U16(p, -2) = (uint16_t)(lo + U16(ent, 0x1C));
			r = (int32_t)x::CrtRand();
			q = mul32(r & 0xFFF, sub32(S16(ent, 0x3C), (int16_t)lo_y)) / 4096;
			lo_y = (uint16_t)(q + lo_y);
			uint16_t lo_z = U16(ent, 0x38);
			U16(p, 0) = (uint16_t)(lo_y + U16(ent, 0x1E));
			r = (int32_t)x::CrtRand();
			q = mul32(r & 0xFFF, sub32(S16(ent, 0x3E), (int16_t)lo_z)) / 4096;
			p += 8;
			lo_z = (uint16_t)(q + lo_z);
			U16(p, -6) = (uint16_t)(lo_z + U16(ent, 0x20));
		}
		if (a_73B7E0(7) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
			return a1;
		}
		return 0;
	}

	// 0x7433C0 (module Siren, sub_7433C0 TASK): bounding-box sparkle: state (0x743B90 pick sprite,
	// 0x743C00 wait, 0x743C50), then for each of its 4 sprites: velocity (+0x8C+8i) *= 3/4,
	// position (+0x6C+8i) += velocity / 16, draw it (s_7434C0); ++frame; ends when finished
	uint32_t __cdecl s_7433C0(uint32_t a1)
	{
		uint32_t tab[3] = { 0x743B90, 0x743C00, 0x743C50 };
		callp(tab[S8(a1, 0x29)], a1);
		uint32_t v = a1 + 0x8E;
		for (int32_t n = 4; n != 0; n--)
		{
			int16_t c = S16(v, -2);
			U16(v, -2) = (uint16_t)((uint16_t)c - (int32_t)c / 4);
			c = S16(v, 0);
			U16(v, 0) = (uint16_t)((uint16_t)c - (int32_t)c / 4);
			c = S16(v, 2);
			U16(v, 2) = (uint16_t)((uint16_t)c - (int32_t)c / 4);
			U16(v, -0x22) = (uint16_t)(U16(v, -0x22) + (int32_t)S16(v, -2) / 16);
			U16(v, -0x20) = (uint16_t)(U16(v, -0x20) + (int32_t)S16(v, 0) / 16);
			U16(v, -0x1E) = (uint16_t)(U16(v, -0x1E) + (int32_t)S16(v, 2) / 16);
			U32(a1, 0x1C) = U32(v, -0x22);
			U32(a1, 0x20) = U32(v, -0x1E);
			s_7434C0(a1);
			v += 8;
		}
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((status & 1) != 0 && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x7434C0 (module Siren, sub_7434C0): draws one sprite of a sparkle node (unless +0x26 bit 2):
	// 0xD8-byte scratch header, camera matrix (unpacked, rotated by itself) projects the node
	// position +0x1C/+0x1E/+0x20 (-> header +0xCC), sprite data ptr +0x4C, frame +0x50, depth
	// +0x56 -> a_7435E0(header, OT+0x44, 2, packet cursor), cursor updated
	uint32_t __cdecl s_7434C0(uint32_t a1)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0; // void
		uint32_t hdr = x::Field_Alloc(0xD8);
		uint32_t m = hdr + 0xB8;
		x::UnpackRotationMatrix(0x1D97778, m);
		int32_t px = S16(a1, 0x1C);
		int32_t py = S16(a1, 0x1E);
		int32_t pz = S16(a1, 0x20);
		uint32_t vec = hdr + 0xCC;
		S32(hdr, 0xD0) = py;
		S32(hdr, 0xD4) = pz;
		S32(vec, 0) = px;
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(m);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(m);
		x::GTE_LoadIRFromMatrixColumn(hdr + 0xBA);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(hdr + 0xBA);
		x::GTE_LoadIRFromMatrixColumn(hdr + 0xBC);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(hdr + 0xBC);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(vec);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(vec);
		x::GTE_SetRotMatrix(m);
		x::GTE_SetTransVector(m);
		uint32_t data = U32(a1, 0x4C);
		uint16_t frame = U16(a1, 0x50);
		uint16_t depth = U16(a1, 0x56);
		U32(hdr, 0) = data;
		uint32_t cursor = U32(0x1D8E054, 0);
		U16(hdr, 4) = frame;
		uint32_t ot = U32(0x1D8E04C, 0) + 0x44;
		U16(hdr, 0x24) = 0;
		U16(hdr, 0xB4) = depth;
		U32(0x1D8E054, 0) = a_7435E0(hdr, ot, 2, cursor);
		x::Field_Free(0xD8);
		return 0; // void
	}

	// 0x743B90 (module Siren, sub_743B90): state: sprite depth -16, picks one of 3 sprite
	// sequences at random (0x15331D0 / 0x1533328 / 0x15334A0), 10 frames; next state
	uint32_t __cdecl s_743B90(uint32_t a1)
	{
		U16(a1, 0x56) = 0xFFF0;
		int32_t r = (int32_t)x::CrtRand();
		int32_t pick = r % 3;
		if (pick == 0)
			U32(a1, 0x4C) = 0x15331D0;
		else if (pick == 1)
		{
			uint8_t st = U8(a1, 0x29);
			U32(a1, 0x4C) = 0x1533328;
			U16(a1, 0x52) = 0xA;
			U8(a1, 0x29) = (uint8_t)(st + 1);
			return 0; // void
		}
		else if (pick == 2)
		{
			uint8_t st = U8(a1, 0x29);
			U32(a1, 0x4C) = 0x15334A0;
			U16(a1, 0x52) = 0xA;
			U8(a1, 0x29) = (uint8_t)(st + 1);
			return 0; // void
		}
		uint8_t st = U8(a1, 0x29);
		U16(a1, 0x52) = 0xA;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x743C70 (module Siren, sub_743C70 TASK): orbit-sparkle spawner, states 0x743CD0 (anchor
	// on Siren, z - 0x2000), 0x743D00 (next), 0x743D10 (spawn every other frame), 0x744050 (nop)
	uint32_t __cdecl s_743C70(uint32_t a1)
	{
		uint32_t tab[4] = { 0x743CD0, 0x743D00, 0x743D10, 0x744050 };
		return s4_state_task(a1, tab);
	}

	// 0x743CD0 (module Siren, sub_743CD0): state: node position = Siren root position with
	// y = 0, z - 0x2000; next state
	uint32_t __cdecl s_743CD0(uint32_t a1)
	{
		s4_anchor_to_root(a1, 0xE000);
		return a1;
	}

	// 0x743D10 (module Siren, sub_743D10): state: on odd frames spawns an orbit sparkle task
	// 0x743DD0 (size 0xE0, queue 0x257FA90) with 4 random orbit keys; finished + next state
	// when a_73B7E0(2)
	uint32_t __cdecl s_743D10(uint32_t a1)
	{
		if ((U8(a1, 0x24) & 1) != 0)
		{
			uint32_t node = x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x743DD0, 0xE0, a1);
			s4_fill_orbit_keys(node);
		}
		if (a_73B7E0(2) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x743DD0 (module Siren, sub_743DD0 TASK): orbit sparkle: state (0x743FD0 setup, 0x744010
	// wait, 0x744040), then its 4 sprites are placed on the key table 0x258E9A0 (lerp by t/16) +
	// node offset, transformed by the Siren orbit matrix (ty - 0x80, tz - 0x2000) and drawn
	uint32_t __cdecl s_743DD0(uint32_t a1)
	{
		uint32_t tab[3] = { 0x743FD0, 0x744010, 0x744040 };
		callp(tab[S8(a1, 0x29)], a1);
		return s4_orbit_sprites_task(a1, 0x258E9A0, 0x80, 0x744010 >> 16);
	}

	// 0x743FD0 (module Siren, sub_743FD0): state: sprite sequence 0x1533024, depth -0x30, random
	// start frame 0..7, 15 frames; hidden (+0x26 |= 4) when a_73B7E0(2); next state
	uint32_t __cdecl s_743FD0(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x1533024;
		U16(a1, 0x56) = 0xFFD0;
		uint32_t r = x::CrtRand() & 7;
		U16(a1, 0x50) = (uint16_t)r;
		U16(a1, 0x52) = 0xF;
		if (a_73B7E0(2) != 0)
			U8(a1, 0x26) |= 4;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x744010 (module Siren, sub_744010): state: when the sprite ended (a_743C20) or
	// a_73B7E0(2): finished + hidden (+0x26 |= 5), next state
	uint32_t __cdecl s_744010(uint32_t a1)
	{
		if (a_743C20(a1) != 0 || a_73B7E0(2) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x744060 (module Siren, sub_744060 TASK): burst orbit-sparkle spawner, states 0x7440C0
	// (anchor, z - 0x2000), 0x7440F0 (next), 0x744100 (spawn 5), 0x744440 (nop)
	uint32_t __cdecl s_744060(uint32_t a1)
	{
		uint32_t tab[4] = { 0x7440C0, 0x7440F0, 0x744100, 0x744440 };
		return s4_state_task(a1, tab);
	}

	// 0x7440C0 (module Siren, sub_7440C0): state: node position = Siren root, y = 0, z - 0x2000
	uint32_t __cdecl s_7440C0(uint32_t a1)
	{
		s4_anchor_to_root(a1, 0xE000);
		return a1;
	}

	// 0x744100 (module Siren, sub_744100): state: spawns 5 orbit sparkle tasks 0x7441D0 (size
	// 0xE0, queue 0x257FA90) with random orbit keys; finished + next when a_73B7E0(2)
	uint32_t __cdecl s_744100(uint32_t a1)
	{
		// (the loop counter lives in the pushed-ecx slot [esp+0x10])
		for (int32_t n = 5; n != 0; n--)
		{
			uint32_t node = x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x7441D0, 0xE0, a1);
			s4_fill_orbit_keys(node);
		}
		if (a_73B7E0(2) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
			return a1;
		}
		return 0;
	}

	// 0x7441D0 (module Siren, sub_7441D0 TASK): orbit sparkle (burst): state (0x7443C0 setup,
	// 0x744400 wait, 0x744430), 4 sprites on key table 0x258E8A0 + node offset through the
	// orbit matrix (tz - 0x2000), drawn
	uint32_t __cdecl s_7441D0(uint32_t a1)
	{
		uint32_t tab[3] = { 0x7443C0, 0x744400, 0x744430 };
		callp(tab[S8(a1, 0x29)], a1);
		return s4_orbit_sprites_task(a1, 0x258E8A0, 0, 0x744400 >> 16);
	}

	// 0x7443C0 (module Siren, sub_7443C0): state: sprite 0x1533024, random frame 0..7, depth
	// -0x40, 15 frames; hidden when a_73B7E0(2); next state
	uint32_t __cdecl s_7443C0(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x1533024;
		uint32_t r = x::CrtRand() & 7;
		U16(a1, 0x50) = (uint16_t)r;
		U16(a1, 0x56) = 0xFFC0;
		U16(a1, 0x52) = 0xF;
		if (a_73B7E0(2) != 0)
			U8(a1, 0x26) |= 4;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x744400 (module Siren, sub_744400): state: sprite ended (a_743C20) or a_73B7E0(2) ->
	// finished + hidden, next state
	uint32_t __cdecl s_744400(uint32_t a1)
	{
		if (a_743C20(a1) != 0 || a_73B7E0(2) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x744450 (module Siren, sub_744450 TASK): key-path sparkle spawner, states 0x7444B0
	// (anchor, z - 0x1000), 0x7444E0 (next), 0x7444F0 (spawn 3), 0x7447B0 (nop)
	uint32_t __cdecl s_744450(uint32_t a1)
	{
		uint32_t tab[4] = { 0x7444B0, 0x7444E0, 0x7444F0, 0x7447B0 };
		return s4_state_task(a1, tab);
	}

	// 0x7444B0 (module Siren, sub_7444B0): state: node position = Siren root, y = 0, z - 0x1000
	uint32_t __cdecl s_7444B0(uint32_t a1)
	{
		s4_anchor_to_root(a1, 0xF000);
		return a1;
	}

	// 0x7444F0 (module Siren, sub_7444F0): state: spawns 3 sparkle tasks 0x744570 (size 0xE0,
	// queue 0x257FA90), each with 4 keys (index rand % 24 at +0xCC+2i, t rand & 15 at +0xD4+2i);
	// finished + next when a_73B7E0(3)
	uint32_t __cdecl s_7444F0(uint32_t a1)
	{
		for (int32_t k = 3; k != 0; k--)
		{
			uint32_t node = x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x744570, 0xE0, a1);
			uint32_t pk = node + 0xD4;
			for (int32_t n = 4; n != 0; n--)
			{
				int32_t r = (int32_t)x::CrtRand();
				U16(pk, -8) = (uint16_t)(r % 0x18);
				U16(pk, 0) = (uint16_t)(x::CrtRand() & 0xF);
				pk += 2;
			}
		}
		if (a_73B7E0(3) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x744570 (module Siren, sub_744570 TASK): key-path sparkle: state (0x744730 setup, 0x744770
	// wait, 0x7447A0), 4 sprites at lerp(key table 0x258BAA0, t/16) through the orbit matrix
	// (tz - 0x1000) -> +0x6C+8i, drawn
	uint32_t __cdecl s_744570(uint32_t a1)
	{
		uint32_t tab[3] = { 0x744730, 0x744770, 0x7447A0 };
		callp(tab[S8(a1, 0x29)], a1);
		uint8_t m[0x20];
		s4_orbit_matrix(P(m), 0, 0x1000);
		uint32_t out = a1 + 0x6C;   // ebp
		uint32_t pt = a1 + 0xD4;    // edi
		for (int32_t n = 4; n != 0; n--)
		{
			int32_t k8 = shl32(S16(pt, -8), 3);
			int32_t t = S16(pt, 0);
			// stack vector [esp+0x14]; its 4th word is the high half of state entry 1 (0x744770)
			int16_t v[4];
			v[3] = (int16_t)(0x744770 >> 16);
			v[0] = (int16_t)s4_key_lerp(0x258BAA0, k8, 0, t);
			v[1] = (int16_t)s4_key_lerp(0x258BAA0, k8, 2, t);
			v[2] = (int16_t)s4_key_lerp(0x258BAA0, k8, 4, t);
			x::GTE_LoadV0(P(v));
			x::GTE_MVMVA_RotV0_Tr_2();
			x::GTE_StoreIR123(out);
			pt += 2;
			out += 8;
		}
		uint32_t pos = a1 + 0x6C;
		for (int32_t n = 4; n != 0; n--)
		{
			U32(a1, 0x1C) = U32(pos, 0);
			U32(a1, 0x20) = U32(pos, 4);
			s_7434C0(a1);
			pos += 8;
		}
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((status & 1) != 0 && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x744730 (module Siren, sub_744730): state: sprite 0x1533024, depth -16, random frame 0..7,
	// 15 frames; hidden when a_73B7E0(3); next state
	uint32_t __cdecl s_744730(uint32_t a1)
	{
		U32(a1, 0x4C) = 0x1533024;
		U16(a1, 0x56) = 0xFFF0;
		uint32_t r = x::CrtRand() & 7;
		U16(a1, 0x50) = (uint16_t)r;
		U16(a1, 0x52) = 0xF;
		if (a_73B7E0(3) != 0)
			U8(a1, 0x26) |= 4;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x744770 (module Siren, sub_744770): state: sprite ended (a_743C20) or a_73B7E0(3) ->
	// finished + hidden, next state
	uint32_t __cdecl s_744770(uint32_t a1)
	{
		if (a_743C20(a1) != 0 || a_73B7E0(3) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x744820 (module Siren, sub_744820): state: target entity (slot +0x2D): if its +0x3C <= -0x200
	// the node finishes; else once its y (+0x20) <= [0x258FB64] and it is not flagged (+8 bit 1),
	// queues a random hit transformation 4/5/6 (queueChainTransformationConditional), sets the
	// spawn countdown +0xDC = 8 and advances
	uint32_t __cdecl s_744820(uint32_t a1)
	{
		uint32_t ent = s4_entity_of(a1);
		if (S16(ent, 0x3C) <= (int16_t)0xFE00)
		{
			U8(a1, 0x26) |= 1;
			return 0; // void
		}
		if (S16(ent, 0x20) > S16(0x258FB64, 0))
			return 0; // void
		if ((U8(ent, 8) & 2) == 0)
		{
			int32_t r = (int32_t)x::CrtRand();
			int32_t pick = r % 3;
			if (pick == 0)
				x::queueChainTransformationConditional(ent, 4);
			else if (pick == 1)
				x::queueChainTransformationConditional(ent, 5);
			else if (pick == 2)
				x::queueChainTransformationConditional(ent, 6);
		}
		uint8_t st = U8(a1, 0x29);
		U16(a1, 0xDC) = 8;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x7448A0 (module Siren, sub_7448A0): state: on odd frames spawns a rising sparkle task
	// 0x744AA0 (4 points in the target's box, y velocity -(rand & 0x3FF)); every frame spawns a
	// splash task 0x744C30 (4 points, random velocities); --countdown +0xDC; finished + next when
	// a_73B7E0(3) or the countdown reaches 0
	uint32_t __cdecl s_7448A0(uint32_t a1)
	{
		// (the loop counters live in the argument slot [esp+0x14]; a1 is kept in ebx)
		if ((U8(a1, 0x24) & 1) != 0)
		{
			uint32_t ent = s4_entity_of(a1);
			uint32_t node = x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x744AA0, 0xE0, a1);
			uint32_t p = node + 0x6E;
			for (int32_t n = 4; n != 0; n--)
			{
				uint16_t lo = U16(ent, 0x34);
				int32_t r = (int32_t)x::CrtRand();
				int32_t q = mul32(r & 0xFFF, sub32(S16(ent, 0x3A), (int16_t)lo)) / 4096;
				U16(p, -2) = (uint16_t)(q + lo + U16(ent, 0x1C));
				r = (int32_t)x::CrtRand();
				uint16_t lo_z = U16(ent, 0x38);
				U16(p, 0) = (uint16_t)(0 - (r & 0x1FF));
				r = (int32_t)x::CrtRand();
				q = mul32(r & 0xFFF, sub32(S16(ent, 0x3E), (int16_t)lo_z)) / 4096;
				U16(p, 2) = (uint16_t)(q + U16(ent, 0x20) + lo_z);
				r = (int32_t)x::CrtRand();
				p += 8;
				U16(p, 0x18) = (uint16_t)(0 - (r & 0x3FF));   // velocity y, +0x8E+8i
			}
		}
		{
			uint32_t ent = s4_entity_of(a1);
			uint32_t node = x::Effect_AddTaskAndInitFromCtx(0x257FA90, 0x744C30, 0xE0, a1);
			uint32_t p = node + 0x6E;
			for (int32_t n = 4; n != 0; n--)
			{
				uint16_t lo = U16(ent, 0x34);
				int32_t r = (int32_t)x::CrtRand();
				int32_t q = mul32(r & 0xFFF, sub32(S16(ent, 0x3A), (int16_t)lo)) / 4096;
				U16(p, -2) = (uint16_t)(q + U16(ent, 0x1C) + lo);
				r = (int32_t)x::CrtRand();
				uint16_t lo_z = U16(ent, 0x38);
				U16(p, 0) = (uint16_t)(0 - (r & 0x1FF));
				r = (int32_t)x::CrtRand();
				q = mul32(r & 0xFFF, sub32(S16(ent, 0x3E), (int16_t)lo_z)) / 4096;
				U16(p, 2) = (uint16_t)(q + U16(ent, 0x20) + lo_z);
				r = (int32_t)x::CrtRand();
				U16(p, 0x1E) = (uint16_t)((r & 0x7FF) - 0x400);      // velocity x, +0x8C+8i
				r = (int32_t)x::CrtRand();
				U16(p, 0x22) = (uint16_t)((r & 0x7FF) - 0x400);      // velocity z, +0x90+8i
				r = (int32_t)x::CrtRand();
				U16(p, 0x20) = (uint16_t)(0xFFFFFA00u - (uint32_t)(r & 0x7FF)); // velocity y, +0x8E+8i
				p += 8;
			}
		}
		U16(a1, 0xDC) = (uint16_t)(U16(a1, 0xDC) - 1);
		if (a_73B7E0(3) != 0 || S16(a1, 0xDC) <= 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// ====================================================================================
	// part s5
	// ====================================================================================
	// Siren (095) unique module functions, part s5: 0x744AA0..0x745AA7 (sprite/particle tasks of the
	// Siren summon: 4-particle drifting sprite groups, the timed spawn script at 0x15339D8, the
	// orbiting sprite task 0x7452F0 and the target-bounding-box spark tasks 0x745740/0x745840).
	namespace
	{
		// common task tail: ++frame counter (+0x24); when finished (+0x26 bit0) and no live child
		// (+0x28 == 0) release the linked task and end the task (2)
		inline uint32_t s5_task_tail(uint32_t node)
		{
			uint8_t status = U8(node, 0x26);
			U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
			if ((status & 1) != 0 && U8(node, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(node);
				return 2;
			}
			return 0;
		}

		// state helper pattern: +0x26 |= flags; ++state index (+0x29)
		inline void s5_next_state(uint32_t node, uint8_t flags)
		{
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | flags);
			S8(node, 0x29) = (int8_t)(S8(node, 0x29) + 1);
		}
	}

	// 0x744AA0 (module 095, TASK): state machine {0x744BA0 init, 0x744BE0 play, 0x744C20}; then for 4
	// particles (pos words at +0x6C+8i, velocity at +0x8C+8i): velocity -= velocity/4, pos +=
	// velocity/16, copies the particle position to +0x1C/+0x20 and draws it (s_7434C0)
	uint32_t __cdecl s_744AA0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[3] = { 0x744BA0, 0x744BE0, 0x744C20 };
		callp(states[S8(node, 0x29)], node);
		uint32_t p = node + 0x8E;  // velocity y of particle i (x at -2, z at +2)
		for (int n = 4; n != 0; --n)
		{
			int16_t v = S16(p, -2);
			U16(p, -2) = (uint16_t)(v - (int16_t)(v / 4));
			v = S16(p, 0);
			U16(p, 0) = (uint16_t)(v - (int16_t)(v / 4));
			v = S16(p, 2);
			U16(p, 2) = (uint16_t)(v - (int16_t)(v / 4));
			U16(p, -0x22) = (uint16_t)(U16(p, -0x22) + (uint16_t)((int32_t)S16(p, -2) / 16));
			U16(p, -0x20) = (uint16_t)(U16(p, -0x20) + (uint16_t)((int32_t)S16(p, 0) / 16));
			U16(p, -0x1E) = (uint16_t)(U16(p, -0x1E) + (uint16_t)((int32_t)S16(p, 2) / 16));
			U32(node, 0x1C) = U32(p, -0x22);  // x,y
			U32(node, 0x20) = U32(p, -0x1E);  // z (+ following word)
			s_7434C0(node);                    // draws one sprite at +0x1C
			p += 8;
		}
		return s5_task_tail(node);
	}

	// 0x744BA0 (module 095): init state of 0x744AA0: sequence data 0x1533024 (+0x4C), +0x56 = -32,
	// +0x52 = 15; +0x26 |= 4 (hidden) when a_73B7E0(3); next state
	uint32_t __cdecl s_744BA0(uint32_t a1)
	{
		uint32_t node = a1;
		U32(node, 0x4C) = 0x1533024;
		U16(node, 0x56) = 0xFFE0;
		U16(node, 0x52) = 0xF;
		if (a_73B7E0(3) != 0)
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 4);
		S8(node, 0x29) = (int8_t)(S8(node, 0x29) + 1);
		return 0;  // void
	}

	// 0x744BE0 (module 095): play state: ends (+0x26 |= 5, next state) when a_73B7E0(3) or when the
	// sequence player a_743C20 reports the end
	uint32_t __cdecl s_744BE0(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
		{
			s5_next_state(a1, 5);
			return 0;  // void
		}
		uint32_t node = a1;
		if (a_743C20(node) != 0)
			s5_next_state(node, 5);
		return 0;  // void
	}

	// 0x744C30 (module 095, TASK): state machine {0x744E80, 0x744ED0, 0x744F30, 0x744F70}; then 4
	// particles (pos +0x6C+8i, vel +0x8C+8i): vel y += 0xC0 (gravity), vel x/z -= v/16,
	// pos += vel/16, copies pos to +0x1C/+0x20; only particle 0 is drawn (s_744D30, scale 0x3000)
	uint32_t __cdecl s_744C30(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[4] = { 0x744E80, 0x744ED0, 0x744F30, 0x744F70 };
		callp(states[S8(node, 0x29)], node);
		uint32_t p = node + 0x8C;  // velocity of particle i
		for (int32_t i = 0; i < 4; ++i)
		{
			U16(p, 2) = (uint16_t)(U16(p, 2) + 0xC0);
			int16_t v = S16(p, 0);
			U16(p, 0) = (uint16_t)(v - (int16_t)(v / 16));
			v = S16(p, 4);
			U16(p, 4) = (uint16_t)(v - (int16_t)(v / 16));
			U16(p, -0x20) = (uint16_t)(U16(p, -0x20) + (uint16_t)((int32_t)S16(p, 0) / 16));
			U16(p, -0x1E) = (uint16_t)(U16(p, -0x1E) + (uint16_t)((int32_t)S16(p, 2) / 16));
			U16(p, -0x1C) = (uint16_t)(U16(p, -0x1C) + (uint16_t)((int32_t)S16(p, 4) / 16));
			uint32_t xy = U32(p, -0x20);
			U32(node, 0x1C) = xy;
			uint32_t z = U32(p, -0x1C);
			U32(node, 0x20) = z;
			if (i < 1)
				s_744D30(node, 0x3000);
			p += 8;
		}
		return s5_task_tail(node);
	}

	// 0x744D30 (module 095): draws the node's sequence sprite (a_7435E0, OT = effect OT +0x44, 2) at
	// +0x1C/+0x1E/+0x20 through a 0xE8 scratch: camera rotation unpacked at +0xB8, scaled by a2 (s16,
	// 1.0 = 0x1000) when != 0x1000, rotated by the camera, position transformed by the camera; the
	// result is loaded as GTE rotation/translation. Skipped when +0x26 bit2 (hidden)
	uint32_t __cdecl s_744D30(uint32_t a1, uint32_t a2)
	{
		uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0;  // void
		uint32_t s = x::Field_Alloc(0xE8);
		uint32_t mat = s + 0xB8;  // Mat4x3: 3x3 s16 + translation s32[3] at +0x14 (s+0xCC)
		x::UnpackRotationMatrix(0x1D97778, mat);
		uint16_t scale = (uint16_t)a2;
		if (scale != 0x1000)
		{
			int32_t sc = (int16_t)scale;
			U32(s, 0xE0) = (uint32_t)sc;
			U32(s, 0xDC) = (uint32_t)sc;
			U32(s, 0xD8) = (uint32_t)sc;
			x::scale3DMatrix(mat, s + 0xD8);
		}
		int32_t px = S16(node, 0x1C);
		int32_t py = S16(node, 0x1E);
		int32_t pz = S16(node, 0x20);
		uint32_t pos = s + 0xCC;
		U32(s, 0xD0) = (uint32_t)py;
		U32(s, 0xD4) = (uint32_t)pz;
		U32(pos, 0) = (uint32_t)px;
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(mat);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(mat);
		x::GTE_LoadIRFromMatrixColumn(s + 0xBA);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(s + 0xBA);
		x::GTE_LoadIRFromMatrixColumn(s + 0xBC);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(s + 0xBC);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(pos);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(pos);
		x::GTE_SetRotMatrix(mat);
		x::GTE_SetTransVector(mat);
		uint32_t seq = U32(node, 0x4C);
		uint16_t frame = U16(node, 0x50);
		uint16_t w56 = U16(node, 0x56);
		U32(s, 0) = seq;
		uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		U16(s, 4) = frame;
		uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(s, 0x24) = 0;
		U16(s, 0xB4) = w56;
		uint32_t r = a_7435E0(s, ot, 2, cursor);
		MEM<uint32_t>(0x1D8E054) = r;
		x::Field_Free(0xE8);
		return 0;  // void
	}

	// 0x744E80 (module 095): init state of 0x744C30: sequence data 0x15335CC, +0x56 = -32, +0x52 = 8,
	// hidden (+0x26 |= 4) when a_73B7E0(3); repeat count +0xDC = rand & 7; next state
	uint32_t __cdecl s_744E80(uint32_t a1)
	{
		uint32_t node = a1;
		U32(node, 0x4C) = 0x15335CC;
		U16(node, 0x56) = 0xFFE0;
		U16(node, 0x52) = 8;
		if (a_73B7E0(3) != 0)
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 4);
		uint32_t r = x::CrtRand();
		U16(node, 0xDC) = (uint16_t)(r & 7);
		S8(node, 0x29) = (int8_t)(S8(node, 0x29) + 1);
		return 0;  // void
	}

	// 0x744ED0 (module 095): loop state: ends on a_73B7E0(3) (+0x26 |= 5); else plays the sequence
	// (a_743C20) and once its frame +0x50 > 1 rewinds it (+0x50 = 0), --repeat +0xDC, next state at 0
	uint32_t __cdecl s_744ED0(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
		{
			s5_next_state(a1, 5);
			return 0;  // void
		}
		uint32_t node = a1;
		a_743C20(node);
		if (S16(node, 0x50) > 1)
		{
			U16(node, 0xDC) = (uint16_t)(U16(node, 0xDC) - 1);
			bool more = S16(node, 0xDC) > 0;
			U16(node, 0x50) = 0;
			if (!more)
				S8(node, 0x29) = (int8_t)(S8(node, 0x29) + 1);
		}
		return 0;  // void
	}

	// 0x744F30 (module 095): last play state: ends (+0x26 |= 5, next state) on a_73B7E0(3) or at the
	// end of the sequence (a_743C20)
	uint32_t __cdecl s_744F30(uint32_t a1)
	{
		if (a_73B7E0(3) != 0)
		{
			s5_next_state(a1, 5);
			return 0;  // void
		}
		uint32_t node = a1;
		if (a_743C20(node) != 0)
			s5_next_state(node, 5);
		return 0;  // void
	}

	// 0x744F90 (module 095, TASK): spawn-script driver: state machine {0x744FF0 origin, 0x745010
	// reset cursor, 0x745030 run script, 0x745240 nullsub}; draws nothing
	uint32_t __cdecl s_744F90(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[4] = { 0x744FF0, 0x745010, 0x745030, 0x745240 };
		callp(states[S8(node, 0x29)], node);
		return s5_task_tail(node);
	}

	// 0x744FF0 (module 095): copies the position dwords (+8/+0xC) of the object at [0x1533010] to
	// the node's +0x1C/+0x20; next state
	uint32_t __cdecl s_744FF0(uint32_t a1)
	{
		uint32_t obj = MEM<uint32_t>(0x1533010);
		uint32_t xy = U32(obj, 8);
		uint32_t z = U32(obj, 0xC);
		U32(a1, 0x1C) = xy;
		U32(a1, 0x20) = z;
		S8(a1, 0x29) = (int8_t)(S8(a1, 0x29) + 1);
		return 0;  // void
	}

	// 0x745010 (module 095): script cursor +0xDC = 0; next state
	uint32_t __cdecl s_745010(uint32_t a1)
	{
		U16(a1, 0xDC) = 0;
		S8(a1, 0x29) = (int8_t)(S8(a1, 0x29) + 1);
		return 0;  // void
	}

	// 0x745030 (module 095): runs the timed spawn script at 0x15339D8 (10-byte entries {s16 frame,
	// angle, height, radius, flag}, frame 0x7FFF = end): for every entry whose frame <= +0x24 spawns a
	// 0x745180 (a_73A380) task via a_73C100(node, fn, asset, 0, 10, 3) (asset 0x16A40EC, or
	// [[0x258EAA0]+0x108] when flag != 0) and places it at (-sin*r, -0x180 + height - rand&63,
	// 0xCC0 - cos*r) (+0x288/+0x28A/+0x28C), angle = entry angle + rand&63 - 32; ends (+0x26 |= 1,
	// next state) on a_73B7E0(6) or at the end marker
	uint32_t __cdecl s_745030(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t entry = (uint32_t)add32(mul32((int32_t)S16(node, 0xDC), 10), 0x15339D8);
		if (a_73B7E0(6) != 0)
		{
			s5_next_state(node, 1);
			return 0;  // void
		}
		int16_t frame = S16(entry, 0);
		if ((uint16_t)frame == 0x7FFF)
		{
			s5_next_state(node, 1);
			return 0;  // void
		}
		if (frame > S16(node, 0x24))
			return 0;  // void
		do
		{
			uint32_t r = x::CrtRand();
			uint32_t ang = r & 0x3F;
			ang = (ang & 0xFFFF0000u) | (uint16_t)((uint16_t)ang + U16(entry, 2));  // add di, word
			ang = (uint32_t)sub32((int32_t)ang, 0x20) & 0xFFF;
			uint32_t asset;
			if (S16(entry, 8) == 0)
				asset = 0x16A40EC;
			else
				asset = U32(MEM<uint32_t>(0x258EAA0), 0x108);
			uint32_t t = a_73C100(node, 0x745180, asset, 0, 0xA, 3);
			if (t != 0)
			{
				int32_t a = (int16_t)ang;
				U16(t, 0x288) = 0;
				U16(t, 0x28A) = 0xFE80;
				U16(t, 0x28C) = 0xCC0;
				int32_t sn = (int32_t)x::computeSin((uint32_t)a);
				int32_t v = mul32(sn, S16(entry, 6)) / 4096;
				U16(t, 0x288) = (uint16_t)(U16(t, 0x288) + (uint16_t)(0u - (uint32_t)v));
				int32_t cs = (int32_t)x::computeCosine((uint32_t)a);
				v = mul32(cs, S16(entry, 6)) / 4096;
				U16(t, 0x28C) = (uint16_t)(U16(t, 0x28C) + (uint16_t)(0u - (uint32_t)v));
				uint32_t r2 = x::CrtRand();
				uint16_t h = (uint16_t)(U16(entry, 4) - (uint16_t)(r2 & 0x3F));
				U16(t, 0x28A) = (uint16_t)(U16(t, 0x28A) + h);
			}
			U16(node, 0xDC) = (uint16_t)(U16(node, 0xDC) + 1);
			entry = (uint32_t)add32(mul32((int32_t)S16(node, 0xDC), 10), 0x15339D8);
			// no end-marker test inside the loop: 0x7FFF is simply > any frame count
		} while (S16(entry, 0) <= S16(node, 0x24));
		return 0;  // void
	}

	// 0x7451E0 (module 095): once frame +0x24 >= +0x298 calls a_73F990 and s_740470; next state
	uint32_t __cdecl s_7451E0(uint32_t a1)
	{
		uint32_t node = a1;
		if (S16(node, 0x24) >= S16(node, 0x298))
		{
			a_73F990(node);
			s_740470(node);
			S8(node, 0x29) = (int8_t)(S8(node, 0x29) + 1);
		}
		return 0;  // void
	}

	// 0x745210 (module 095): when s_740470 returns nonzero: +0x26 |= 1 (finished), next state
	uint32_t __cdecl s_745210(uint32_t a1)
	{
		uint32_t node = a1;
		if (s_740470(node) != 0)
			s5_next_state(node, 1);
		return 0;  // void
	}

	// 0x7452B0 (module 095): spawns one 0x7452F0 orbiting sprite task (size 0x64) into queue
	// 0x2585EF0 with child index = +0x60 (post-increment), wait +0x5C = 4 + rand&3; next state
	uint32_t __cdecl s_7452B0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x2585EF0, 0x7452F0, 0x64, node);
		uint16_t idx = U16(node, 0x60);
		U16(t, 0x60) = idx;
		U16(node, 0x60) = (uint16_t)(idx + 1);
		uint32_t r = x::CrtRand();
		U16(node, 0x5C) = (uint16_t)((r & 3) + 4);
		S8(node, 0x29) = (int8_t)(S8(node, 0x29) + 1);
		return 0;  // void
	}

	// 0x7452F0 (module 095, TASK): orbiting sprite: state machine {0x745490 init, 0x7455B0 fade,
	// 0x7455F0 hold, 0x745610 fade back, 0x745640}; then UPDATE x += +0x54, spin +0x48 += +0x5E,
	// then DRAW (s_745380)
	uint32_t __cdecl s_7452F0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[5] = { 0x745490, 0x7455B0, 0x7455F0, 0x745610, 0x745640 };
		callp(states[S8(node, 0x29)], node);
		uint16_t spin = U16(node, 0x5E);
		uint16_t dx = U16(node, 0x54);
		spin = (uint16_t)(spin + U16(node, 0x48));
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + dx);
		U16(node, 0x48) = (uint16_t)(spin & 0xFFF);
		s_745380(node);
		return s5_task_tail(node);
	}

	// 0x745380 (module 095): draws the node's sprite with s_73C790 (OT = effect OT +0x44, 2): local
	// matrix = MAG_022 8DD770 (init) then rotations +0x46 (8DD8A0), +0x44 (8DD7E0), +0x48 (8DD960),
	// translation +0x1C/+0x1E/+0x20, scaled by the vector +0x30, loaded into the GTE; 0x68 scratch
	// sprite descriptor (+0 sprite data +0x4C, +8 = +0x40, +0xC = +0x50, +0x10 = +0x52, +0x14 = 0xF0,
	// four 0x100 words); skipped when +0x26 bit2 (hidden)
	uint32_t __cdecl s_745380(uint32_t a1)
	{
		uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0;  // void
		// local Mat4x3 (0x20 B): 3x3 s16 at +0, translation s32 at +0x14/+0x18/+0x1C
		// UNINIT 0x745380: the original stack buffer is not cleared (8DD770 fills the matrix; only the
		// padding word +0x12 keeps stack garbage) - zeroed here
		uint8_t m[0x20] = {};
		uint32_t L = P(m);
		x::MAG_022_sub_8DD770(L);
		x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)S16(node, 0x46));
		x::sub_8DD7E0(L, (uint32_t)(int32_t)S16(node, 0x44));
		x::sub_8DD960(L, (uint32_t)(int32_t)S16(node, 0x48));
		int32_t px = S16(node, 0x1C);
		int32_t py = S16(node, 0x1E);
		int32_t pz = S16(node, 0x20);
		U32(L, 0x14) = (uint32_t)px;
		U32(L, 0x18) = (uint32_t)py;
		U32(L, 0x1C) = (uint32_t)pz;
		x::scale3DMatrix(L, node + 0x30);
		x::GTE_SetRotMatrix_W(L);
		x::GTE_SetTransVector_W(L);
		uint32_t d = x::Field_Alloc(0x68);
		U32(d, 0) = U32(node, 0x4C);
		U32(d, 8) = U32(node, 0x40);
		U32(d, 0xC) = (uint32_t)(int32_t)S16(node, 0x50);
		U16(d, 0x18) = 0;
		U16(d, 0x1A) = 0;
		U16(d, 0x1E) = 0;
		U16(d, 0x1C) = 0;
		U16(d, 0x26) = 0;
		U16(d, 0x24) = 0;
		U32(d, 0x10) = (uint32_t)(int32_t)S16(node, 0x52);
		uint32_t ot = MEM<uint32_t>(0x1D8E04C);
		U16(d, 0x22) = 0x100;
		U16(d, 0x20) = 0x100;
		U16(d, 0x2A) = 0x100;
		U16(d, 0x28) = 0x100;
		uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		U32(d, 0x14) = 0xF0;
		uint32_t r = s_73C790(d, ot + 0x44, 2, cursor);
		MEM<uint32_t>(0x1D8E054) = r;
		x::Field_Free(0x68);
		return 0;  // void
	}

	// 0x745490 (module 095): init of the orbiting sprite: sprite [[0x258EAA0]+0x144], +0x52 = -0x200;
	// random start (either x in [-300,299], y = +-(120..149), or x = +-(150..299), y in [-120,119];
	// the sign by child index parity +0x60), z = 2 * [[0x15339D0]+0xE0], x speed +0x54 = +-(2..4),
	// scale +0x30/+0x34/+0x38 = 0x400 + rand%0xFFF, spin +0x48 random, +0x50 = 0x1000, spin speed
	// +0x5E = 16..31; next state (9 rand calls)
	uint32_t __cdecl s_745490(uint32_t a1)
	{
		uint32_t tab = MEM<uint32_t>(0x258EAA0);
		uint32_t node = a1;
		U32(node, 0x4C) = U32(tab, 0x144);
		U16(node, 0x52) = 0xFE00;
		int32_t r = (int32_t)x::CrtRand();
		if ((r & 1) != 0)
		{
			r = (int32_t)x::CrtRand();
			U16(node, 0x1C) = (uint16_t)(r % 600 - 300);
			r = (int32_t)x::CrtRand();
			int32_t y = r % 30 + 0x78;
			U16(node, 0x1E) = (uint16_t)y;
			if ((U8(node, 0x60) & 1) != 0)
				U16(node, 0x1E) = (uint16_t)(0u - (uint32_t)y);
		}
		else
		{
			r = (int32_t)x::CrtRand();
			int32_t xx = r % 150 + 0x96;
			U16(node, 0x1C) = (uint16_t)xx;
			if ((U8(node, 0x60) & 1) != 0)
				U16(node, 0x1C) = (uint16_t)(0u - (uint32_t)xx);
			r = (int32_t)x::CrtRand();
			U16(node, 0x1E) = (uint16_t)(r % 240 - 0x78);
		}
		uint32_t obj = MEM<uint32_t>(0x15339D0);
		U16(node, 0x20) = (uint16_t)(U16(obj, 0xE0) << 1);
		r = (int32_t)x::CrtRand();
		U16(node, 0x54) = (uint16_t)(r % 3 + 2);
		r = (int32_t)x::CrtRand();
		if ((r & 1) != 0)
			U16(node, 0x54) = (uint16_t)(0 - U16(node, 0x54));
		r = (int32_t)x::CrtRand();
		uint32_t sc = (uint32_t)(r % 0xFFF + 0x400);
		U32(node, 0x38) = sc;
		U32(node, 0x34) = sc;
		U32(node, 0x30) = sc;
		r = (int32_t)x::CrtRand();
		U16(node, 0x48) = (uint16_t)(r & 0xFFF);
		r = (int32_t)x::CrtRand();
		U16(node, 0x50) = 0x1000;
		U16(node, 0x5E) = (uint16_t)((r & 0xF) + 0x10);
		S8(node, 0x29) = (int8_t)(S8(node, 0x29) + 1);
		return 0;  // void
	}

	// 0x7455B0 (module 095): +0x50 -= 0x200 until <= 0 (clamped to 0), then wait +0x5C = 4 + rand&3;
	// next state
	uint32_t __cdecl s_7455B0(uint32_t a1)
	{
		uint32_t node = a1;
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + 0xFE00);
		if (S16(node, 0x50) <= 0)
		{
			U16(node, 0x50) = 0;
			uint32_t r = x::CrtRand();
			U16(node, 0x5C) = (uint16_t)((r & 3) + 4);
			S8(node, 0x29) = (int8_t)(S8(node, 0x29) + 1);
		}
		return 0;  // void
	}

	// 0x7455F0 (module 095): --wait +0x5C; next state at <= 0
	uint32_t __cdecl s_7455F0(uint32_t a1)
	{
		U16(a1, 0x5C) = (uint16_t)(U16(a1, 0x5C) - 1);
		if (S16(a1, 0x5C) <= 0)
			S8(a1, 0x29) = (int8_t)(S8(a1, 0x29) + 1);
		return 0;  // void
	}

	// 0x745610 (module 095): +0x50 += 0x200; at >= 0x1000: finished (+0x26 |= 1), clamp 0x1000,
	// next state
	uint32_t __cdecl s_745610(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x200);
		if (S16(a1, 0x50) >= 0x1000)
		{
			int8_t st = S8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U16(a1, 0x50) = 0x1000;
			S8(a1, 0x29) = (int8_t)(st + 1);
		}
		return 0;  // void
	}

	// 0x745650 (module 095): spawner wait state: --wait +0x5C; ends (+0x26 |= 1, next state) on
	// a_73B7E0(7); otherwise at wait <= 0 goes back one state (to the spawn state, 0x7452B0)
	uint32_t __cdecl s_745650(uint32_t a1)
	{
		uint32_t node = a1;
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) - 1);
		if (a_73B7E0(7) != 0)
		{
			s5_next_state(node, 1);
			return 0;  // void
		}
		if (S16(node, 0x5C) <= 0)
			S8(node, 0x29) = (int8_t)(S8(node, 0x29) - 1);
		return 0;  // void
	}

	// 0x7456A0 (module 095, TASK): target spark spawner: state machine {0x745700 target position,
	// 0x745730 delay 16 frames, 0x745740 spawn sparks, 0x745AC0 nullsub}; draws nothing
	uint32_t __cdecl s_7456A0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[4] = { 0x745700, 0x745730, 0x745740, 0x745AC0 };
		callp(states[S8(node, 0x29)], node);
		return s5_task_tail(node);
	}

	// 0x745700 (module 095): copies the position dwords (+0x1C/+0x20) of battle entity slot +0x2D
	// (0x1D972C0 + 0x9C*slot) to the node; next state
	uint32_t __cdecl s_745700(uint32_t a1)
	{
		uint32_t slot = U8(a1, 0x2D);
		uint32_t ent = 0x1D972C0 + slot * 39 * 4;
		U32(a1, 0x1C) = U32(ent, 0x1C);
		int8_t st = S8(a1, 0x29);
		U32(a1, 0x20) = U32(ent, 0x20);
		S8(a1, 0x29) = (int8_t)(st + 1);
		return 0;  // void
	}

	// 0x745730 (module 095): next state once frame +0x24 > 15
	uint32_t __cdecl s_745730(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0xF)
			S8(a1, 0x29) = (int8_t)(S8(a1, 0x29) + 1);
		return 0;  // void
	}

	// 0x745740 (module 095): every 4th frame ((frame + slot) & 3 == 0, target slot +0x2D) spawns a
	// 0x745840 spark task (size 0x64, queue 0x2585EF0) at a random point of the target entity's
	// bounding box (entity +0x34..+0x38 min, +0x3A..+0x3E max, plus entity position +0x1C..+0x20);
	// ends (+0x26 |= 1, next state) on a_73B7E0(7)
	uint32_t __cdecl s_745740(uint32_t a1)
	{
		uint32_t node = a1;
		uint8_t slot = U8(node, 0x2D);
		uint8_t ph = (uint8_t)(U8(node, 0x24) + slot);
		if ((ph & 3) == 0)
		{
			uint32_t ent = 0x1D972C0 + (uint32_t)slot * 39 * 4;
			uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x2585EF0, 0x745840, 0x64, node);
			uint16_t lo = U16(ent, 0x34);
			uint32_t r = x::CrtRand();
			int32_t v = mul32((int32_t)(r & 0xFFF), sub32(S16(ent, 0x3A), (int16_t)lo)) / 4096;
			uint16_t w = (uint16_t)((uint16_t)v + U16(ent, 0x1C));
			w = (uint16_t)(w + lo);
			lo = U16(ent, 0x36);
			U16(t, 0x1C) = w;
			r = x::CrtRand();
			v = mul32((int32_t)(r & 0xFFF), sub32(S16(ent, 0x3C), (int16_t)lo)) / 4096;
			w = (uint16_t)((uint16_t)v + U16(ent, 0x1E));
			w = (uint16_t)(w + lo);
			lo = U16(ent, 0x38);
			U16(t, 0x1E) = w;
			r = x::CrtRand();
			v = mul32((int32_t)(r & 0xFFF), sub32(S16(ent, 0x3E), (int16_t)lo)) / 4096;
			w = (uint16_t)((uint16_t)v + U16(ent, 0x20));
			w = (uint16_t)(w + lo);
			U16(t, 0x20) = w;
		}
		if (a_73B7E0(7) != 0)
			s5_next_state(node, 1);
		return 0;  // void
	}

	// 0x745840 (module 095, TASK): spark: state machine {0x7459D0 init, 0x745A20 fade, 0x745A60
	// hold, 0x745A80 fade back, 0x745AB0}; then UPDATE spin +0x48 += +0x5E, scale +0x30/+0x34/+0x38
	// = +0x38 + 0x100, then DRAW (a_7458E0)
	uint32_t __cdecl s_745840(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[5] = { 0x7459D0, 0x745A20, 0x745A60, 0x745A80, 0x745AB0 };
		callp(states[S8(node, 0x29)], node);
		uint16_t spin = (uint16_t)(U16(node, 0x5E) + U16(node, 0x48));
		U16(node, 0x48) = (uint16_t)(spin & 0xFFF);
		uint32_t sc = U32(node, 0x38) + 0x100;
		U32(node, 0x38) = sc;
		U32(node, 0x34) = sc;
		U32(node, 0x30) = sc;
		a_7458E0(node);
		return s5_task_tail(node);
	}

	// 0x7459D0 (module 095): spark init: sprite [[0x258EAA0]+0x148], scale 0x100, random spin +0x48,
	// +0x50 = 0x1000, spin speed +0x5E = 16..31; next state
	uint32_t __cdecl s_7459D0(uint32_t a1)
	{
		uint32_t tab = MEM<uint32_t>(0x258EAA0);
		uint32_t node = a1;
		U32(node, 0x4C) = U32(tab, 0x148);
		U32(node, 0x38) = 0x100;
		U32(node, 0x34) = 0x100;
		U32(node, 0x30) = 0x100;
		uint32_t r = x::CrtRand();
		U16(node, 0x48) = (uint16_t)(r & 0xFFF);
		r = x::CrtRand();
		U16(node, 0x50) = 0x1000;
		U16(node, 0x5E) = (uint16_t)((r & 0xF) + 0x10);
		S8(node, 0x29) = (int8_t)(S8(node, 0x29) + 1);
		return 0;  // void
	}

	// 0x745A20 (module 095): +0x50 -= 0x400 until <= 0 (clamped to 0), then wait +0x5C = 4 + rand&3;
	// next state
	uint32_t __cdecl s_745A20(uint32_t a1)
	{
		uint32_t node = a1;
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + 0xFC00);
		if (S16(node, 0x50) <= 0)
		{
			U16(node, 0x50) = 0;
			uint32_t r = x::CrtRand();
			U16(node, 0x5C) = (uint16_t)((r & 3) + 4);
			S8(node, 0x29) = (int8_t)(S8(node, 0x29) + 1);
		}
		return 0;  // void
	}

	// 0x745A60 (module 095): --wait +0x5C; next state at <= 0
	uint32_t __cdecl s_745A60(uint32_t a1)
	{
		U16(a1, 0x5C) = (uint16_t)(U16(a1, 0x5C) - 1);
		if (S16(a1, 0x5C) <= 0)
			S8(a1, 0x29) = (int8_t)(S8(a1, 0x29) + 1);
		return 0;  // void
	}

	// 0x745A80 (module 095): +0x50 += 0x200; at >= 0x1000: finished (+0x26 |= 1), clamp 0x1000,
	// next state
	uint32_t __cdecl s_745A80(uint32_t a1)
	{
		U16(a1, 0x50) = (uint16_t)(U16(a1, 0x50) + 0x200);
		if (S16(a1, 0x50) >= 0x1000)
		{
			int8_t st = S8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U16(a1, 0x50) = 0x1000;
			S8(a1, 0x29) = (int8_t)(st + 1);
		}
		return 0;  // void
	}

	// ====================================================================================
	// part s6
	// ====================================================================================
	// Part s6: Siren (095) module-unique functions 0x745AD0-0x7473D0: the water-surface director
	// (a 4-column x up-to-11-row grid of POLY_GT4 with scrolling UVs + texture-window packets), its
	// splash particles, the Siren creature actor task and its droplet/bubble particle tasks.
	namespace
	{
		// surface state block of a water-surface node: 0x2585AE8 + 0xD4 * node+0xDE
		inline uint32_t s6_surface(uint32_t node)
		{
			int32_t idx = S16(node, 0xde);
			int32_t k = add32(idx, mul32(idx, 2));      // lea ecx,[eax+eax*2]
			k = add32(k, mul32(k, 8));                   // lea ecx,[ecx+ecx*8]
			k = shl32(k, 1);
			k = sub32(k, idx);                           // 53 * idx
			return (uint32_t)add32(mul32(k, 4), 0x2585ae8);
		}

		// GPU texture-window command (0xE2) from the surface block at +o:
		// (+o+2 & 0xF8, +o & 0xF8, ~(+o+6 - 1) & 0xF8, (~(+o+4 - 1) >> 3) & 0x1F)
		// (0x7463D9-0x746416 for o=0x50, 0x74645F-0x74649C for o=0x58; only the low bits of the
		//  partially loaded registers reach the result, their garbage high halves are masked away)
		inline uint32_t s6_texwin(uint32_t edi, int32_t o)
		{
			uint32_t c = ((uint32_t)U8(edi, o + 2) & 0xf8) | 0xfffe2000u;
			c = (uint32_t)shl32((int32_t)c, 5);
			c |= (uint32_t)U8(edi, o) & 0xf8;
			c = (uint32_t)shl32((int32_t)c, 5);
			c |= (~((uint32_t)U16(edi, o + 6) - 1u)) & 0xf8;
			c = (uint32_t)shl32((int32_t)c, 2);
			uint32_t d = (uint32_t)((int32_t)(~((uint32_t)U16(edi, o + 4) - 1u)) >> 3) & 0x1f;
			return c | d;
		}
	}

	// 0x745AD0 (module 095): water-surface TASK: runs state handler [+0x29] (745B40 wait,
	// 745BB0 init surface, 745E80 grow, 7467B0 hold, 746800 fade, nullsub), ++frame +0x24,
	// ends (returns 2) when finished (+0x26 bit0) and no child alive (+0x28).
	uint32_t __cdecl s_745AD0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[6] = { 0x745b40, 0x745bb0, 0x745e80, 0x7467b0, 0x746800, 0x746910 };
		int32_t st = S8(node, 0x29);
		callp(states[st], node);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);   // frame counter
		if ((U8(node, 0x26) & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x745B40 (module 095): surface state 0: sets position (+0x1C = 0x80, +0x1E = -0x500,
	// +0x20 = script word +4) and waits until frame 45/65/85/105 (per surface index +0xDE) to
	// advance (index 2 can advance twice in one tick at frame >= 105).
	uint32_t __cdecl s_745B40(uint32_t a1)
	{
		uint32_t script = MEM<uint32_t>(0x1533010);
		uint16_t z = U16(script, 4);
		uint32_t node = a1;
		U16(node, 0x1c) = 0x80;
		uint32_t idx = (uint32_t)(int32_t)S16(node, 0xde);
		U16(node, 0x1e) = 0xfb00;
		U16(node, 0x20) = z;
		if (idx > 3)
			return 0; // void
		switch (idx)
		{
		case 0:
			if (S16(node, 0x24) >= 0x2d)
				U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			break;
		case 1:
			if (S16(node, 0x24) >= 0x41)
				U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			break;
		case 2:
			if (S16(node, 0x24) >= 0x55)
				U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			// quirk 0x745B8E: no ret, falls through into the case-3 test (0x745B91)
			if (S16(node, 0x24) >= 0x69)
				U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			break;
		case 3:
			if (S16(node, 0x24) >= 0x69)
				U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			break;
		}
		return 0; // void
	}

	// 0x745BB0 (module 095): surface state 1: initialises the surface block of this node
	// (s_745BF0) and advances the state.
	uint32_t __cdecl s_745BB0(uint32_t a1)
	{
		uint32_t node = a1;
		// quirk 0x745BBC: the third argument is the whole eax with only ax = +0xDE loaded (high half =
		// dispatcher's eax = state index 1 -> 0); the callee only reads it as a signed word.
		uint32_t which = (uint32_t)U16(node, 0xde);
		s_745BF0(node, s6_surface(node), which);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x745BF0 (module 095): initialises the 0xD4-byte surface block a2: clears it, builds the
	// POLY_GT4 template (+0x00..0x33: tag, colours 0x80, code 0x3E, UVs), the uv-tile words
	// (+0x60..0xBF), 3 row colours (+0x44/+0x48/+0x4C), the texture-window params (+0x50..0x5F),
	// the 11x4 base vertex grid at 0x258EE38 (x = 3/2*w - col*w, z = -row*0x400, w = 0xC0 + 0x60*row),
	// two random wave phases (+0xCE/+0xD0) and the rotation per surface index a3 (+0x3C/+0x3E/+0x40).
	// a1 (the node) is not used.
	uint32_t __cdecl s_745BF0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		(void)a1;
		uint32_t b = a2;
		x::MAG_007_sub_8DCC00(b, 0xd4);   // zero 0xD4 bytes (rep stosd)
		U16(b, 0x56) = 0x100;
		U16(b, 0x54) = 0x100;
		U16(b, 0x0e) = 0x3e54;
		U8(b, 0x0d) = 0x80;
		U8(b, 0x31) = 0xff;
		U8(b, 0x25) = 0xff;
		U8(b, 0x0c) = 0;
		uint32_t eax = U32(b, 0x0c);
		U8(b, 0x0c) = 0x2b;
		U32(b, 0x60) = eax;
		U32(b, 0x6c) = eax;
		eax = U32(b, 0x0c);
		U16(b, 0x1a) = 0xbc;
		U8(b, 0x19) = 0x80;
		U8(b, 0x30) = 0x2b;
		uint32_t edx = U32(b, 0x30);
		U8(b, 0x18) = 0x2b;
		uint32_t ecx = U32(b, 0x18);
		U8(b, 0x24) = 0;
		uint32_t edi = U32(b, 0x24);
		U32(b, 0x64) = eax;
		U32(b, 0x70) = eax;
		U32(b, 0x78) = ecx;
		U32(b, 0xa8) = edx;
		U32(b, 0x84) = ecx;
		U32(b, 0xb4) = edx;
		U8(b, 0x30) = 0x55;
		edx = U32(b, 0x30);
		U8(b, 0x18) = 0x55;
		ecx = U32(b, 0x18);
		U32(b, 0x90) = edi;
		U32(b, 0x9c) = edi;
		U8(b, 0x24) = 0x2b;
		edi = U32(b, 0x24);
		U8(b, 0x0c) = 0x55;
		U8(b, 0x30) = 0x7f;
		U8(b, 0x18) = 0x7f;
		eax = U32(b, 0x0c);
		U32(b, 0x7c) = ecx;
		U32(b, 0x88) = ecx;
		ecx = U32(b, 0x18);
		U16(b, 0x52) = 0;
		U16(b, 0x50) = 0;
		U16(b, 0x58) = 0;
		U16(b, 0x5a) = 0x80;
		U16(b, 0x5c) = 0x80;
		U16(b, 0x5e) = 0x80;
		U32(b, 0x00) = 0x0c000000;   // prim tag (12 words)
		U8(b, 0x07) = 0x3e;          // POLY_GT4, semi-transparent
		U8(b, 0x06) = 0x80;
		U8(b, 0x05) = 0x80;
		U8(b, 0x04) = 0x80;
		U8(b, 0x12) = 0x80;
		U8(b, 0x11) = 0x80;
		U8(b, 0x10) = 0x80;
		U8(b, 0x1e) = 0x80;
		U8(b, 0x1d) = 0x80;
		U8(b, 0x1c) = 0x80;
		U8(b, 0x2a) = 0x80;
		U8(b, 0x29) = 0x80;
		U8(b, 0x28) = 0x80;
		U32(b, 0x94) = edi;
		U32(b, 0xac) = edx;
		U32(b, 0xa0) = edi;
		U32(b, 0xb8) = edx;
		U8(b, 0x24) = 0x55;
		U32(b, 0x68) = eax;
		edx = U32(b, 0x24);
		edi = U32(b, 0x30);
		U32(b, 0x80) = ecx;
		U32(b, 0x8c) = ecx;
		U32(b, 0x98) = edx;
		U32(b, 0xb0) = edi;
		U32(b, 0x74) = eax;
		U32(b, 0xa4) = edx;
		U32(b, 0xbc) = edi;
		// row colours (rgb + code 0x3E): +0x4C crest, +0x48 mid, +0x44 edge (black)
		U8(b, 0x4f) = 0x3e;
		U8(b, 0x4e) = 0x80;
		U8(b, 0x4d) = 0x80;
		U8(b, 0x4c) = 0x80;
		U8(b, 0x4b) = 0x3e;
		U8(b, 0x4a) = 0x40;
		U8(b, 0x49) = 0x40;
		U8(b, 0x48) = 0x40;
		U8(b, 0x47) = 0x3e;
		U8(b, 0x46) = 0;
		U8(b, 0x45) = 0;
		U8(b, 0x44) = 0;

		// base vertex grid, 11 rows x 4 columns of SVECTOR (x, 0, z; pad word not written)
		uint32_t v = 0x258ee38;
		int32_t width = 0xc0;   // [esp+0x20] (argument slot reused)
		for (int32_t row = 0; row < 0xb; row++)
		{
			int32_t w16 = (int16_t)width;
			int32_t half = add32(w16, mul32(w16, 2)) / 2;   // 3*w/2 (cdq/sub/sar = trunc)
			int32_t z = shl32(-row, 0xa);
			for (int32_t col = 0; col < 4; col++)
			{
				int32_t xx = sub32(half, mul32(col, width));
				U16(v, 0) = (uint16_t)xx;
				U16(v, 2) = 0;
				U16(v, 4) = (uint16_t)z;
				v += 8;
			}
			width = add32(width, 0x60);
		}

		uint32_t r = x::CrtRand();
		U16(b, 0xce) = (uint16_t)(r & 0xfff);
		r = x::CrtRand();
		U16(b, 0xd0) = (uint16_t)(r & 0xfff);

		uint32_t which = (uint32_t)(int32_t)(int16_t)a3;
		switch (which)
		{
		case 0: U16(b, 0x3c) = 0x40; U16(b, 0x3e) = 0x180; U16(b, 0x40) = 0; break;
		case 1: U16(b, 0x3c) = 0x40; U16(b, 0x3e) = 0x80;  U16(b, 0x40) = 0; break;
		case 2: U16(b, 0x3c) = 0x40; U16(b, 0x3e) = 0xf80; U16(b, 0x40) = 0; break;
		case 3: U16(b, 0x3c) = 0x40; U16(b, 0x3e) = 0xe80; U16(b, 0x40) = 0; break;
		default: break;
		}
		return 0; // void
	}

	// 0x745E80 (module 095): surface state 2 (grow): updates + draws the surface (s_745F50); at
	// script phase 7 clears the hold timer +0xDC and advances; else once the surface has 10 rows
	// sets a random hold time +0xDC per surface index (2-5 / 0 / 4-7 / 6-9) and advances.
	uint32_t __cdecl s_745E80(uint32_t a1)
	{
		uint32_t node = a1;
		s_745F50(node, s6_surface(node));
		if (a_73B7E0(7) != 0)
		{
			U16(node, 0xdc) = 0;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			return 0; // void
		}
		int32_t idx = S16(node, 0xde);
		if (U16(s6_surface(node), 0xcc) != 0xa)   // cmp word [53*idx*4 + 0x2585BB4], 0xA
			return 0; // void
		uint32_t r;
		switch ((uint32_t)idx)
		{
		case 0:
			r = x::CrtRand();
			U16(node, 0xdc) = (uint16_t)((r & 3) + 2);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			break;
		case 1:
			U16(node, 0xdc) = 0;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			break;
		case 2:
			r = x::CrtRand();
			U16(node, 0xdc) = (uint16_t)((r & 3) + 4);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			break;
		case 3:
			r = x::CrtRand();
			U16(node, 0xdc) = (uint16_t)((r & 3) + 6);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			break;
		default:   // index > 3 (unsigned): 0x745F37
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
			break;
		}
		return 0; // void
	}

	// 0x745F50 (module 095): water surface update + draw (a1 node, a2 surface block). Grows the
	// row count +0xCC (max 10), scrolls the V of the uv-tile words (+0xC0/+0xC4/+0xC8), advances
	// the wave phases +0xCE/+0xD0, builds the displaced+rotated vertex grid at 0x258EC70 (base grid
	// 0x258EE38 + sin waves, rotation +0x3C/+0x3E, scale 0x200 or 0x1000 from phase 5, offset node
	// +0x1C..+0x20), the per-vertex colours at 0x2585E3C, then emits rows x 3 POLY_GT4, each with
	// 2 texture-window packets, into the OT at [0x1D8E04C]; from 4 rows on (when +0xDE + frame is
	// even and crest red >= 0x40) spawns one splash task s_7466A0 with 4 random surface points.
	uint32_t __cdecl s_745F50(uint32_t a1, uint32_t a2)
	{
		// stack frame of the original, addressed with the listing's offsets (no push outstanding);
		// a1/a2 slots at +0x68/+0x6C are reused as variables by the original
		alignas(4) uint8_t frame[0x70] = {};
		const uint32_t F0 = P(frame);
		U32(F0, 0x68) = a1;
		U32(F0, 0x6c) = a2;

		uint32_t ot = MEM<uint32_t>(0x1d8e04c);
		uint32_t edi = U32(F0, 0x6c);                  // surface block
		uint32_t ebx = MEM<uint32_t>(0x1d8e054);       // packet cursor
		U32(F0, 0x2c) = ot;
		U16(edi, 0xcc) = (uint16_t)(U16(edi, 0xcc) + 1);   // row count
		if (S16(edi, 0xcc) > 0xa)
			U16(edi, 0xcc) = 0xa;

		// V scroll of the three uv-tile rows
		uint32_t vs = (uint32_t)sub32((int32_t)U32(edi, 0xc0), 0x1000) & 0x7f00;
		U32(edi, 0xc0) = vs;
		U32(edi, 0xc4) = vs + 0x4000;
		U32(edi, 0xc8) = vs + 0x8000;
		for (uint32_t k = 0; k < 3; k++)
		{
			const uint32_t m = 0xffff00ffu;
			uint32_t b = edi + 0x78 + 4 * k;
			U32(b, -0x18) &= m;
			U32(b, 0) &= m;
			U32(b, 0x18) &= m;
			U32(b, 0x30) &= m;
			U32(b, -0xc) &= m;
			U32(b, 0xc) &= m;
			U32(b, 0x24) &= m;
			U32(b, 0x3c) &= m;
			U32(b, -0x18) |= U32(edi, 0xc0);
			U32(b, 0) |= U32(edi, 0xc0);
			U32(b, 0x18) |= U32(edi, 0xc4);
			U32(b, 0x30) |= U32(edi, 0xc4);
			U32(b, -0xc) |= U32(edi, 0xc4);
			U32(b, 0xc) |= U32(edi, 0xc4);
			U32(b, 0x24) |= U32(edi, 0xc8);
			U32(b, 0x3c) |= U32(edi, 0xc8);
		}

		// wave phases
		uint16_t ph1 = (uint16_t)((U16(edi, 0xce) + 0x80) & 0xfff);
		uint16_t ph2 = (uint16_t)((U16(edi, 0xd0) + 0x80) & 0xfff);
		U16(edi, 0xce) = ph1;
		U16(edi, 0xd0) = ph2;
		if (a_73B7E0(5) != 0)
		{
			U32(F0, 0x3c) = 0x1000;
			U32(F0, 0x38) = 0x1000;
			U32(F0, 0x34) = 0x1000;
			U32(F0, 0x28) = 0xc0;   // x-amplitude step per row
		}
		else
		{
			U32(F0, 0x28) = 0x60;
			U32(F0, 0x3c) = 0x200;
			U32(F0, 0x38) = 0x200;
			U32(F0, 0x34) = 0x200;
		}
		uint16_t cx = U16(edi, 0xce);
		uint16_t dx = U16(edi, 0xd0);
		uint32_t src = 0x258ee38;   // ebp
		U32(F0, 0x6c) = 0;          // x amplitude (argument slot reused)
		U32(F0, 0x18) = 0;          // y amplitude
		uint32_t dst = 0x258ec70;   // esi
		// UNINIT 0x7460E9/0x7460EE: only the low words of [esp+0x1C]/[esp+0x20] are written; the high
		// halves are never observed (movsx reads, and the 32-bit updates are masked with 0xFFF)
		U16(F0, 0x1c) = cx;
		U16(F0, 0x20) = dx;
		x::MAG_022_sub_8DD770(F0 + 0x44);                                  // identity matrix
		x::MAG_022_sub_8DD8A0(F0 + 0x44, (uint32_t)(int32_t)S16(edi, 0x3e));
		x::sub_8DD7E0(F0 + 0x44, (uint32_t)(int32_t)S16(edi, 0x3c));
		x::scale3DMatrix(F0 + 0x44, F0 + 0x34);
		x::GTE_SetRotMatrix(F0 + 0x44);

		U32(F0, 0x10) = 0;
		if (S16(edi, 0xcc) + 1 > 0)
		{
			do
			{
				U32(F0, 0x24) = 4;
				do
				{
					uint32_t w0 = U32(src, 0);
					uint32_t w1 = U32(src, 4);
					U32(dst, 0) = w0;
					U32(dst, 4) = w1;
					int32_t s = (int32_t)x::computeSin((uint32_t)(int32_t)S16(F0, 0x1c));
					int32_t t = mul32(s, S16(F0, 0x6c));
					U16(dst, 0) = (uint16_t)(U16(dst, 0) + (uint16_t)(t / 4096));
					s = (int32_t)x::computeSin((uint32_t)(int32_t)S16(F0, 0x20));
					t = mul32(s, S16(F0, 0x18));
					U16(dst, 2) = (uint16_t)(U16(dst, 2) + (uint16_t)(t / 4096));
					x::GTE_LoadV0(dst);
					x::GTE_MVMVA_RotV0();
					x::GTE_StoreIR123(dst);
					uint32_t node = U32(F0, 0x68);
					src += 8;
					dst += 8;
					uint16_t ox = U16(node, 0x1c);
					uint16_t oy = U16(node, 0x1e);
					U16(dst, -8) = (uint16_t)(U16(dst, -8) + ox);
					uint16_t oz = U16(node, 0x20);
					U16(dst, -6) = (uint16_t)(U16(dst, -6) + oy);
					U16(dst, -4) = (uint16_t)(U16(dst, -4) + oz);
					U32(F0, 0x24) = U32(F0, 0x24) - 1;
				} while (U32(F0, 0x24) != 0);
				U32(F0, 0x6c) = (uint32_t)add32((int32_t)U32(F0, 0x6c), (int32_t)U32(F0, 0x28));
				U32(F0, 0x18) = (uint32_t)add32((int32_t)U32(F0, 0x18), 0x40);
				U32(F0, 0x1c) = (U32(F0, 0x1c) + 0x100) & 0xfff;
				U32(F0, 0x20) = (U32(F0, 0x20) + 0x200) & 0xfff;
				U32(F0, 0x10) = U32(F0, 0x10) + 1;
			} while ((int32_t)U32(F0, 0x10) < S16(edi, 0xcc) + 1);
		}

		// per-vertex colours ((rows+1) x 4 dwords) at 0x2585E3C
		{
			int32_t n = S16(edi, 0xcc);
			uint32_t out = 0x2585e3c;
			for (int32_t r = 0; r < n + 1; r++)
			{
				for (int32_t c = 0; c < 4; c++)
				{
					uint32_t col = 0;
					bool mid = false;   // 0x746288 path
					if (r == 0)
						col = U32(edi, 0x44);
					else if (r == 1)
					{
						if (c == 0) col = U32(edi, 0x44);
						else mid = true;
					}
					else if (r == n)
						col = U32(edi, 0x44);
					else if (r == n - 1)
					{
						if (c == 0) col = U32(edi, 0x44);
						else mid = true;
					}
					else
					{
						if (c == 0) col = U32(edi, 0x44);
						else if (c == 3) col = U32(edi, 0x44);
						else col = U32(edi, 0x4c);
					}
					if (mid)
						col = (c == 3) ? U32(edi, 0x44) : U32(edi, 0x48);
					U32(out, 0) = col;
					out += 4;
				}
			}
		}

		// DRAW: rows x 3 POLY_GT4 (+ 2 texture-window packets each)
		U32(F0, 0x6c) = 0x258ec70;
		uint32_t colp = 0x2585e3c;   // ebp
		x::GTE_SetTransVector(0x1d97778);
		x::GTE_SetRotMatrix(0x1d97778);
		U32(F0, 0x10) = 0;
		if (S16(edi, 0xcc) > 0)
		{
			do
			{
				U32(F0, 0x24) = U32(F0, 0x10) & 1;          // odd row: other uv set
				U32(F0, 0x20) = U32(F0, 0x6c) + 8;
				U32(F0, 0x1c) = edi + 0x78;
				U32(F0, 0x28) = 3;
				do
				{
					uint32_t uv = U32(F0, 0x1c);
					uint32_t vp = U32(F0, 0x20);
					uint32_t odd = U32(F0, 0x24);
					U32(ebx, 0x00) = U32(edi, 0);
					U32(ebx, 0x04) = U32(colp, 0);
					U32(ebx, 0x10) = U32(colp, 4);
					U32(ebx, 0x1c) = U32(colp, 0x10);
					uint32_t d = U32(colp, 0x14);
					U32(ebx, 0x28) = d;
					uint32_t c;
					if (odd != 0)
					{
						c = U32(uv, -0x18);
						d = U32(uv, 0);
						U32(ebx, 0x0c) = c;
						c = U32(uv, 0x18);
						U32(ebx, 0x18) = d;
						d = U32(uv, 0x30);
						U32(ebx, 0x24) = c;
					}
					else
					{
						c = U32(uv, -0xc);
						d = U32(uv, 0xc);
						U32(ebx, 0x0c) = c;
						c = U32(uv, 0x24);
						U32(ebx, 0x18) = d;
						d = U32(uv, 0x3c);
						U32(ebx, 0x24) = c;
					}
					uint32_t v0 = U32(F0, 0x6c);
					U32(ebx, 0x30) = d;
					x::GTE_LoadV012(v0, vp, vp + 0x18);
					x::GTE_RTPT();
					x::GTE_StoreSXY012_PolyGT3_2(ebx);
					vp += 0x20;
					x::GTE_LoadV0(vp);
					x::GTE_RTPS();
					x::GTE_ReadSXY2(ebx + 0x2c);
					x::GTE_AVSZ4();
					x::GTE_ReadOTZ(F0 + 0x14);
					int32_t z = (int32_t)U32(F0, 0x14) >> 2;
					uint32_t pk = ebx + 0x34;
					uint32_t tw = edi + 0x50;
					U32(F0, 0x14) = (uint32_t)z;
					U32(pk, 0) = 0x2000000;
					uint32_t cmd = (tw != 0) ? s6_texwin(edi, 0x50) : 0;
					uint32_t otb = U32(F0, 0x2c);
					U32(pk, 4) = cmd;
					U32(pk, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(otb + U32(F0, 0x14) * 4, pk);
					uint32_t zz = U32(F0, 0x14);
					uint32_t poly = ebx;
					ebx = U32(F0, 0x2c);                    // ebx = OT base from here
					x::SSIGPU_InsertPrimAutoDepth(ebx + zz * 4, poly);
					pk += 0xc;
					tw = edi + 0x58;
					U32(pk, 0) = 0x2000000;
					cmd = (tw != 0) ? s6_texwin(edi, 0x58) : 0;
					U32(pk, 4) = cmd;
					U32(pk, 8) = 0;
					x::SSIGPU_InsertPrimAutoDepth(ebx + U32(F0, 0x14) * 4, pk);
					uint32_t e20 = U32(F0, 0x20);
					uint32_t e1c = U32(F0, 0x1c);
					ebx = pk + 0xc;                         // packet cursor
					uint32_t e6c = U32(F0, 0x6c) + 8;
					e20 += 8;
					uint32_t cnt = U32(F0, 0x28);
					colp += 4;
					e1c += 4;
					cnt--;
					U32(F0, 0x6c) = e6c;
					U32(F0, 0x20) = e20;
					U32(F0, 0x1c) = e1c;
					U32(F0, 0x28) = cnt;
				} while (U32(F0, 0x28) != 0);
				U32(F0, 0x6c) = U32(F0, 0x6c) + 8;
				colp += 4;
				U32(F0, 0x10) = U32(F0, 0x10) + 1;
			} while ((int32_t)U32(F0, 0x10) < S16(edi, 0xcc));
		}

		uint32_t node = U32(F0, 0x68);
		MEM<uint32_t>(0x1d8e054) = ebx;
		// quirk 0x74652D: `add edx, ecx` of two registers with only the low bytes loaded
		// (+0xDE, +0x24); only bit 0 of the sum is tested = parity of the two low bytes.
		uint8_t par = (uint8_t)(U8(node, 0xde) + U8(node, 0x24));
		if ((par & 1) == 0 && S16(edi, 0xcc) >= 4 && U8(edi, 0x4c) >= 0x40)
		{
			uint32_t task = x::Effect_AddTaskAndInitFromCtx(0x257fa90, 0x7466a0, 0xe0, node);
			uint32_t pt = task + 0x6e;
			U32(F0, 0x68) = 4;    // argument slot reused as loop counter
			do
			{
				uint32_t r = x::CrtRand();
				int32_t col = (int32_t)r % 4;
				r = x::CrtRand();
				int32_t den = S16(edi, 0xcc) + 1;
				int32_t rowr = (int32_t)r % den;
				int32_t row;
				if (a_73B7E0(5) != 0)
					row = S16(edi, 0xcc) - rowr / 2;
				else
					row = rowr / 2;
				int32_t idx = add32(col, mul32(row, 4));
				uint32_t vp = (uint32_t)add32(mul32(idx, 8), 0x258ec70);
				uint32_t q0 = U32(vp, 0);
				uint32_t q1 = U32(vp, 4);
				U32(F0, 0x2c) = q0;   // OT-base slot reused for the picked vertex
				U32(F0, 0x30) = q1;

				r = x::CrtRand();
				int32_t dlt = S16(vp, 8) - S16(vp, 0);
				int32_t t = mul32((int32_t)(r & 0xf), dlt);
				uint16_t px = (uint16_t)(U16(F0, 0x2c) + (uint16_t)(t / 16));

				r = x::CrtRand();
				dlt = S16(vp, 0x22) - S16(vp, 2);
				t = mul32((int32_t)(r & 0xf), dlt);
				U16(F0, 0x2e) = (uint16_t)(U16(F0, 0x2e) + (uint16_t)(t / 16));

				r = x::CrtRand();
				dlt = S16(vp, 0x24) - S16(vp, 4);
				t = mul32((int32_t)(r & 0xf), dlt);
				U16(pt, -2) = px;
				uint16_t py = U16(F0, 0x2e);
				uint16_t pz = (uint16_t)(U16(F0, 0x30) + (uint16_t)(t / 16));
				U16(pt, 0) = py;
				U16(pt, 2) = pz;
				if (a_73B7E0(5) != 0)
				{
					r = x::CrtRand();
					U16(pt, 0) = (uint16_t)(U16(pt, 0) + (uint16_t)((r & 0x3ff) - 0x200));
				}
				pt += 8;
				U32(F0, 0x68) = U32(F0, 0x68) - 1;
			} while (U32(F0, 0x68) != 0);
		}
		return 0; // void
	}

	// 0x7466A0 (module 095): splash TASK (4 points at +0x6C + 8*i): runs state [+0x29] (746760 init,
	// 746780 wait-done, nullsub) then for each point copies it to +0x1C/+0x20, selects sprite
	// +0x4C (0x15334A0 / 0x1533328 / 0x15331D0 x2) and draws it (s_7434C0 from phase 5, else
	// s_744D30 scale 0x400); ++frame, ends when finished and no child.
	uint32_t __cdecl s_7466A0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[3] = { 0x746760, 0x746780, 0x7467a0 };
		int32_t st = S8(node, 0x29);
		callp(states[st], node);
		for (int32_t i = 0; i < 4; i++)
		{
			uint32_t p0 = U32(node, i * 8 + 0x6c);
			U32(node, 0x1c) = p0;
			uint32_t p1 = U32(node, i * 8 + 0x70);
			U32(node, 0x20) = p1;
			if (i == 0)
				U32(node, 0x4c) = 0x15334a0;
			else if (i == 1)
				U32(node, 0x4c) = 0x1533328;
			else
				U32(node, 0x4c) = 0x15331d0;
			if (a_73B7E0(5) != 0)
				s_7434C0(node);
			else
				s_744D30(node, 0x400);
		}
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((U8(node, 0x26) & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x746760 (module 095): splash state 0: +0x56 = -16, +0x52 = 10, advance.
	uint32_t __cdecl s_746760(uint32_t a1)
	{
		uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U16(node, 0x56) = 0xfff0;
		st++;
		U16(node, 0x52) = 0xa;
		U8(node, 0x29) = st;
		return 0; // void
	}

	// 0x7467B0 (module 095): surface state 3 (hold): updates + draws the surface, counts +0xDC
	// down, advances at script phase 7 or when the timer reaches 0.
	uint32_t __cdecl s_7467B0(uint32_t a1)
	{
		uint32_t node = a1;
		s_745F50(node, s6_surface(node));
		U16(node, 0xdc) = (uint16_t)(U16(node, 0xdc) - 1);
		uint32_t r = a_73B7E0(7);
		if (r != 0 || !(S16(node, 0xdc) > 0))
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x746800 (module 095): surface state 4 (fade): updates + draws the surface, fades its colours
	// (s_746870); when fully faded: at phase 7 marks the node finished and advances, else loops
	// back to state 1 (re-initialise the surface).
	uint32_t __cdecl s_746800(uint32_t a1)
	{
		uint32_t node = a1;
		s_745F50(node, s6_surface(node));
		uint32_t r = s_746870(s6_surface(node));
		if (r != 1)
			return 0; // void
		if (a_73B7E0(7) != 0)
		{
			uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		else
			U8(node, 0x29) = 1;
		return 0; // void
	}

	// 0x746870 (module 095): fades the row colours of surface block a1: returns 1 when the crest
	// red +0x4C is already 0; else crest (+0x4C..4E) -= 8 (phase 7) / 4 and mid (+0x48..4A)
	// -= 4 / 2, zeroing both on underflow (crest > 0xE0); returns 0.
	uint32_t __cdecl s_746870(uint32_t a1)
	{
		uint32_t b = a1;
		if (U8(b, 0x4c) == 0)
			return 1;
		uint8_t step;
		if (a_73B7E0(7) != 0)
		{
			uint8_t d = (uint8_t)(U8(b, 0x4c) + 0xf8);
			uint8_t c = (uint8_t)(U8(b, 0x4d) + 0xf8);
			U8(b, 0x4c) = d;
			d = (uint8_t)(U8(b, 0x4e) + 0xf8);
			U8(b, 0x4d) = c;
			U8(b, 0x4e) = d;
			step = 0xfc;
		}
		else
		{
			uint8_t d = (uint8_t)(U8(b, 0x4c) + 0xfc);
			uint8_t c = (uint8_t)(U8(b, 0x4d) + 0xfc);
			U8(b, 0x4c) = d;
			d = (uint8_t)(U8(b, 0x4e) + 0xfc);
			U8(b, 0x4d) = c;
			U8(b, 0x4e) = d;
			step = 0xfe;
		}
		uint8_t c = (uint8_t)(U8(b, 0x48) + step);
		uint8_t d = (uint8_t)(U8(b, 0x49) + step);
		U8(b, 0x48) = c;
		c = (uint8_t)(U8(b, 0x4a) + step);
		uint8_t crest = U8(b, 0x4c);
		U8(b, 0x49) = d;
		U8(b, 0x4a) = c;
		if (crest > 0xe0)
		{
			U8(b, 0x4a) = 0;
			U8(b, 0x49) = 0;
			U8(b, 0x48) = 0;
			U8(b, 0x4e) = 0;
			U8(b, 0x4d) = 0;
			U8(b, 0x4c) = 0;
		}
		return 0;
	}

	// 0x7469C0 (module 095): state handler: from script phase 3 sets +0x1C = 0x1000 and the word
	// 0x1D98992 of the 4 battle model records (+0x2C each) to 0x1000, clears their flag bit 1
	// (a_746A10), marks the node finished and advances.
	uint32_t __cdecl s_7469C0(uint32_t a1)
	{
		if (a_73B7E0(3) == 0)
			return 0; // void
		uint32_t node = a1;
		uint32_t p = 0x1d98992;
		U16(node, 0x1c) = 0x1000;
		for (int32_t i = 0; i < 4; i++)
		{
			U16(p, 0) = 0x1000;
			p += 0x2c;
		}
		a_746A10();
		uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x746AA0 (module 095): state handler: a_746AE0 (model record flags), +0x1C = 0x800, the 4
	// model records' word 0x1D98992 = 0x800 and bytes 0x1D989B8..BA = 0 (+0x2C each), advance.
	uint32_t __cdecl s_746AA0(uint32_t a1)
	{
		a_746AE0();
		uint32_t node = a1;
		uint32_t p = 0x1d989ba;
		U16(node, 0x1c) = 0x800;
		for (int32_t i = 0; i < 4; i++)
		{
			U16(p, -0x28) = 0x800;
			U8(p, 0) = 0;
			U8(p, -1) = 0;
			U8(p, -2) = 0;
			p += 0x2c;
		}
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x746B10 (module 095): state handler: +0x1C -= 0x200; at <= 0 clamps to 0, marks the node
	// finished and advances; copies +0x1C to the 4 model records' word 0x1D98992 (+0x2C each).
	uint32_t __cdecl s_746B10(uint32_t a1)
	{
		uint32_t node = a1;
		U16(node, 0x1c) = (uint16_t)(U16(node, 0x1c) + 0xfe00);
		if (!(S16(node, 0x1c) > 0))
		{
			uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			st++;
			U16(node, 0x1c) = 0;
			U8(node, 0x29) = st;
		}
		uint16_t val = U16(node, 0x1c);
		uint32_t p = 0x1d98992;
		for (int32_t i = 0; i < 4; i++)
		{
			U16(p, 0) = val;
			p += 0x2c;
		}
		return 0; // void
	}

	// 0x746B60 (module 095): Siren creature actor TASK: runs state [+0x29] (746DF0 init, 746E50
	// intro loop, 746E80 wait + sounds, 746ED0 sing/fade-in, 747370 sing, 7473D0 loop, a_747400 loop
	// until phase 6 then hide, nullsub) = UPDATE; then unless hidden (+0x26 bit 2) DRAWS the model
	// (a_746C10, model 0x258BFB0, packet cursor 0x1D8E054); then ++frame; ends when finished and no child.
	uint32_t __cdecl s_746B60(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[8] = { 0x746df0, 0x746e50, 0x746e80, 0x746ed0, 0x747370, 0x7473d0, 0x747400, 0x747430 };
		int32_t st = S8(node, 0x29);
		callp(states[st], node);                                          // UPDATE
		if ((U8(node, 0x26) & 4) == 0)
		{
			uint32_t cur = MEM<uint32_t>(0x1d8e054);
			MEM<uint32_t>(0x1d8e054) = a_746C10(node, 0x258bfb0, cur);   // DRAW
		}
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((U8(node, 0x26) & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x746DF0 (module 095): creature state 0: colour 0x18 (+0x58..5A), position (0, -0x1E0,
	// 0xCC0) at +0x4C, scale vector 0x1000 x3 at +0x114 (pointer +0x60), starts animation 0.
	uint32_t __cdecl s_746DF0(uint32_t a1)
	{
		uint32_t node = a1;
		U8(node, 0x5a) = 0x18;
		U8(node, 0x59) = 0x18;
		U8(node, 0x58) = 0x18;
		uint32_t scl = node + 0x114;
		U16(node, 0x4c) = 0;
		U16(node, 0x4e) = 0xfe20;
		U16(node, 0x50) = 0xcc0;
		U32(node, 0x60) = scl;
		U32(node, 0x11c) = 0x1000;
		U32(node, 0x118) = 0x1000;
		U32(scl, 0) = 0x1000;
		x::au_re_Battle_ReadAnimation_7(node, 0);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x746E50 (module 095): creature state 1: advances the looping animation (sub_8DD1C0); at
	// script phase 4 sets colour 0x20 and advances.
	uint32_t __cdecl s_746E50(uint32_t a1)
	{
		uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (a_73B7E0(4) != 0)
		{
			U8(node, 0x5a) = 0x20;
			U8(node, 0x59) = 0x20;
			U8(node, 0x58) = 0x20;
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x746E80 (module 095): creature state 2: waits until script word +0x46 >= 18, then plays SE
	// (0x153301C, 1, 0x80) and the summon stream (0x80, 1, 0x7F), starts animation 1, advances.
	// No animation advance while waiting (the pose is held).
	uint32_t __cdecl s_746E80(uint32_t a1)
	{
		uint32_t script = MEM<uint32_t>(0x1533010);
		if (S16(script, 0x46) < 0x12)
			return 0; // void
		x::BdPlaySE(0x153301c, 1, 0x80);
		x::BdPlaySummonStream(0x80, 1, 0x7f);
		uint32_t node = a1;
		x::au_re_Battle_ReadAnimation_7(node, 1);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x746ED0 (module 095): creature state 3: anim frames 10..55 spawn 4 droplet tasks per tick
	// (s_746F40), from frame 30 2 bubble tasks per tick (s_747160); advances the looping animation;
	// fades the colour in (+0x58 += 4, at >= 0x80 clamps and advances), copies it to +0x59/+0x5A.
	uint32_t __cdecl s_746ED0(uint32_t a1)
	{
		uint32_t node = a1;
		int16_t fr = S16(node, 0x136);
		if (fr >= 0xa && fr <= 0x37)
			s_746F40(node);
		if (S16(node, 0x136) >= 0x1e)
			s_747160(node);
		x::sub_8DD1C0(node);
		uint8_t c = (uint8_t)(U8(node, 0x58) + 4);
		U8(node, 0x58) = c;
		if (c >= 0x80)
		{
			uint8_t st = U8(node, 0x29);
			U8(node, 0x58) = 0x80;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		uint8_t v = U8(node, 0x58);
		U8(node, 0x5a) = v;
		U8(node, 0x59) = v;
		return 0; // void
	}

	// 0x746F40 (module 095): spawns 4 droplet tasks s_746FD0 (size 0xE0, queue 0x257FA90) under a1;
	// the i-th new task gets a spawn position (GetEffectSpawnPosition from a1+0x30, random entry of
	// table 0x1533DF0[0..11], random angle) at +0x6C+8i and a random speed 0x100..0x1FF at +0x8E+8i.
	uint32_t __cdecl s_746F40(uint32_t a1)
	{
		for (int32_t ofs = 0; ofs < 0x20; )
		{
			uint32_t r = x::CrtRand();
			int32_t sel = (int32_t)r % 0xc;
			r = x::CrtRand();
			uint32_t ang = r & 0xfff;
			uint32_t task = x::Effect_AddTaskAndInitFromCtx(0x257fa90, 0x746fd0, 0xe0, a1);
			int32_t tabv = S16((uint32_t)add32(mul32((int16_t)sel, 2), 0x1533df0), 0);
			x::GetEffectSpawnPosition(a1 + 0x30, (uint32_t)tabv, (uint32_t)(int32_t)(int16_t)ang, ofs + task + 0x6c);
			r = x::CrtRand();
			uint32_t spd = (r & 0xff) + 0x100;
			ofs += 8;
			U16(task, ofs + 0x86) = (uint16_t)spd;
		}
		return 0; // void
	}

	// 0x746FD0 (module 095): droplet TASK (4 points +0x6C + 8i, velocity +0x8C + 8i): runs state
	// [+0x29] (747110 init, 747130 wait-done, nullsub), then per point: velocity -= velocity/4,
	// position += velocity/16, copies it to +0x1C/+0x20, sprite +0x4C (0x15336C4 first, else
	// 0x15334A0) and draws it (s_744D30 scale 0x400); ++frame, ends when finished and no child.
	uint32_t __cdecl s_746FD0(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[3] = { 0x747110, 0x747130, 0x747150 };
		int32_t st = S8(node, 0x29);
		callp(states[st], node);
		for (int32_t i = 0; i < 4; i++)
		{
			int32_t o = i * 8;
			for (int32_t k = 0; k < 3; k++)
			{
				uint16_t w = U16(node, o + 0x8c + 2 * k);
				U16(node, o + 0x8c + 2 * k) = (uint16_t)(w - (uint16_t)((int32_t)(int16_t)w / 4));
			}
			U16(node, o + 0x6c) = (uint16_t)(U16(node, o + 0x6c) + (uint16_t)((int32_t)S16(node, o + 0x8c) / 16));
			U16(node, o + 0x6e) = (uint16_t)(U16(node, o + 0x6e) + (uint16_t)((int32_t)S16(node, o + 0x8e) / 16));
			U16(node, o + 0x70) = (uint16_t)(U16(node, o + 0x70) + (uint16_t)((int32_t)S16(node, o + 0x90) / 16));
			uint32_t p0 = U32(node, o + 0x6c);
			U32(node, 0x1c) = p0;
			uint32_t p1 = U32(node, o + 0x70);
			U32(node, 0x20) = p1;
			U32(node, 0x4c) = (i == 0) ? 0x15336c4u : 0x15334a0u;
			s_744D30(node, 0x400);
		}
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((U8(node, 0x26) & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x747110 (module 095): droplet state 0: +0x56 = -16, +0x52 = 10, advance.
	uint32_t __cdecl s_747110(uint32_t a1)
	{
		uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U16(node, 0x56) = 0xfff0;
		st++;
		U16(node, 0x52) = 0xa;
		U8(node, 0x29) = st;
		return 0; // void
	}

	// 0x747160 (module 095): spawns 2 bubble tasks s_747220 (size 0xE0, queue 0x257FA90) under a1;
	// the i-th new task gets a spawn position (GetEffectSpawnPosition from a1+0x30, random entry of
	// table 0x1533E08[0..3], random angle) at +0x6C+8i and a random velocity (-0x200..0x1FF x3) at +0x8C+8i.
	uint32_t __cdecl s_747160(uint32_t a1)
	{
		for (int32_t ofs = 0; ofs < 0x10; )
		{
			uint32_t r = x::CrtRand();
			int32_t sel = (int32_t)r % 4;
			r = x::CrtRand();
			uint32_t ang = r & 0xfff;
			uint32_t task = x::Effect_AddTaskAndInitFromCtx(0x257fa90, 0x747220, 0xe0, a1);
			int32_t tabv = S16((uint32_t)add32(mul32((int16_t)sel, 2), 0x1533e08), 0);
			x::GetEffectSpawnPosition(a1 + 0x30, (uint32_t)tabv, (uint32_t)(int32_t)(int16_t)ang, ofs + task + 0x6c);
			r = x::CrtRand();
			U16(task, ofs + 0x8c) = (uint16_t)((r & 0x3ff) - 0x200);
			r = x::CrtRand();
			U16(task, ofs + 0x8e) = (uint16_t)((r & 0x3ff) - 0x200);
			r = x::CrtRand();
			ofs += 8;
			U16(task, ofs + 0x88) = (uint16_t)((r & 0x3ff) - 0x200);
		}
		return 0; // void
	}

	// 0x747220 (module 095): bubble TASK (4 points +0x6C + 8i, velocity +0x8C + 8i): runs state
	// [+0x29] (747320 init, 747340 wait-done, nullsub), then per point: velocity -= velocity/4,
	// position += velocity/16, copies it to +0x1C/+0x20 and draws it (s_744D30 scale 0x800);
	// ++frame, ends when finished and no child.
	uint32_t __cdecl s_747220(uint32_t a1)
	{
		uint32_t node = a1;
		uint32_t states[3] = { 0x747320, 0x747340, 0x747360 };
		int32_t st = S8(node, 0x29);
		callp(states[st], node);
		uint32_t v = node + 0x8e;
		for (int32_t n = 4; n != 0; n--)
		{
			uint16_t w = U16(v, -2);
			U16(v, -2) = (uint16_t)(w - (uint16_t)((int32_t)(int16_t)w / 4));
			w = U16(v, 0);
			U16(v, 0) = (uint16_t)(w - (uint16_t)((int32_t)(int16_t)w / 4));
			w = U16(v, 2);
			U16(v, 2) = (uint16_t)(w - (uint16_t)((int32_t)(int16_t)w / 4));
			U16(v, -0x22) = (uint16_t)(U16(v, -0x22) + (uint16_t)((int32_t)S16(v, -2) / 16));
			U16(v, -0x20) = (uint16_t)(U16(v, -0x20) + (uint16_t)((int32_t)S16(v, 0) / 16));
			U16(v, -0x1e) = (uint16_t)(U16(v, -0x1e) + (uint16_t)((int32_t)S16(v, 2) / 16));
			uint32_t p0 = U32(v, -0x22);
			U32(node, 0x1c) = p0;
			uint32_t p1 = U32(v, -0x1e);
			U32(node, 0x20) = p1;
			s_744D30(node, 0x800);
			v += 8;
		}
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((U8(node, 0x26) & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x747320 (module 095): bubble state 0: sprite +0x4C = 0x15334A0, +0x56 = -16, +0x52 = 10, advance.
	uint32_t __cdecl s_747320(uint32_t a1)
	{
		uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U32(node, 0x4c) = 0x15334a0;
		st++;
		U16(node, 0x56) = 0xfff0;
		U16(node, 0x52) = 0xa;
		U8(node, 0x29) = st;
		return 0; // void
	}

	// 0x747370 (module 095): creature state 4: droplets (anim frames 10..55) and bubbles (frame >= 30)
	// as in state 3; advances the one-shot animation (au_re_Battle_ReadAnimation_8); when it ends
	// starts animation 2 and advances.
	uint32_t __cdecl s_747370(uint32_t a1)
	{
		uint32_t node = a1;
		int16_t fr = S16(node, 0x136);
		if (fr >= 0xa && fr <= 0x37)
			s_746F40(node);
		if (S16(node, 0x136) >= 0x1e)
			s_747160(node);
		if (x::au_re_Battle_ReadAnimation_8(node) == 1)
		{
			x::au_re_Battle_ReadAnimation_7(node, 2);
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7473D0 (module 095): creature state 5: spawns 2 bubble tasks, advances the looping
	// animation, advances at script phase 5.
	uint32_t __cdecl s_7473D0(uint32_t a1)
	{
		uint32_t node = a1;
		s_747160(node);
		x::sub_8DD1C0(node);
		if (a_73B7E0(5) != 0)
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

namespace siren
{
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort PORTS[] = {
		{ 0x73A0F0, (void *)s_73A0F0, "095 MAG_095_sub_73A0F0" },
		{ 0x73A130, (void *)s_73A130, "095 MAG_095_sub_73A130" },
		{ 0x73A220, (void *)s_73A220, "095 MAG_095_sub_73A220" },
		{ 0x73A270, (void *)s_73A270, "095 MAG_095_sub_73A270" },
		{ 0x73A6E0, (void *)s_73A6E0, "095 MAG_095_sub_73A6E0" },
		{ 0x73A7B0, (void *)s_73A7B0, "095 MAG_095_sub_73A7B0" },
		{ 0x73A8E0, (void *)s_73A8E0, "095 sub_73A8E0" },
		{ 0x73A990, (void *)s_73A990, "095 sub_73A990" },
		{ 0x73A9E0, (void *)s_73A9E0, "095 sub_73A9E0" },
		{ 0x73AAA0, (void *)s_73AAA0, "095 sub_73AAA0" },
		{ 0x73B2F0, (void *)s_73B2F0, "095 sub_73B2F0" },
		{ 0x73B320, (void *)s_73B320, "095 sub_73B320" },
		{ 0x73B380, (void *)s_73B380, "095 sub_73B380" },
		{ 0x73B480, (void *)s_73B480, "095 sub_73B480" },
		{ 0x73B670, (void *)s_73B670, "095 sub_73B670" },
		{ 0x73B6B0, (void *)s_73B6B0, "095 sub_73B6B0" },
		{ 0x73B6D0, (void *)s_73B6D0, "095 sub_73B6D0" },
		{ 0x73B6F0, (void *)s_73B6F0, "095 sub_73B6F0" },
		{ 0x73B730, (void *)s_73B730, "095 sub_73B730" },
		{ 0x73B7B0, (void *)s_73B7B0, "095 sub_73B7B0" },
		{ 0x73B800, (void *)s_73B800, "095 sub_73B800" },
		{ 0x73B840, (void *)s_73B840, "095 sub_73B840" },
		{ 0x73B880, (void *)s_73B880, "095 sub_73B880" },
		{ 0x73B900, (void *)s_73B900, "095 sub_73B900" },
		{ 0x73B950, (void *)s_73B950, "095 sub_73B950" },
		{ 0x73B9A0, (void *)s_73B9A0, "095 sub_73B9A0" },
		{ 0x73B9D0, (void *)s_73B9D0, "095 sub_73B9D0" },
		{ 0x73BA30, (void *)s_73BA30, "095 sub_73BA30" },
		{ 0x73BA50, (void *)s_73BA50, "095 sub_73BA50" },
		{ 0x73BA70, (void *)s_73BA70, "095 sub_73BA70" },
		{ 0x73BAD0, (void *)s_73BAD0, "095 sub_73BAD0" },
		{ 0x73BB20, (void *)s_73BB20, "095 sub_73BB20" },
		{ 0x73BBB0, (void *)s_73BBB0, "095 sub_73BBB0" },
		{ 0x73BBF0, (void *)s_73BBF0, "095 sub_73BBF0" },
		{ 0x73BC10, (void *)s_73BC10, "095 sub_73BC10" },
		{ 0x73BC60, (void *)s_73BC60, "095 sub_73BC60" },
		{ 0x73BC90, (void *)s_73BC90, "095 sub_73BC90" },
		{ 0x73BCF0, (void *)s_73BCF0, "095 sub_73BCF0" },
		{ 0x73C150, (void *)s_73C150, "095 sub_73C150" },
		{ 0x73C1A0, (void *)s_73C1A0, "095 sub_73C1A0" },
		{ 0x73C220, (void *)s_73C220, "095 sub_73C220" },
		{ 0x73C3B0, (void *)s_73C3B0, "095 sub_73C3B0" },
		{ 0x73C790, (void *)s_73C790, "095 sub_73C790" },
		{ 0x73CD70, (void *)s_73CD70, "095 sub_73CD70" },
		{ 0x73D1F0, (void *)s_73D1F0, "095 sub_73D1F0" },
		{ 0x73E1A0, (void *)s_73E1A0, "095 sub_73E1A0" },
		{ 0x73E7F0, (void *)s_73E7F0, "095 sub_73E7F0" },
		{ 0x73EB10, (void *)s_73EB10, "095 sub_73EB10" },
		{ 0x73EE10, (void *)s_73EE10, "095 sub_73EE10" },
		{ 0x73EE50, (void *)s_73EE50, "095 sub_73EE50" },
		{ 0x73EEC0, (void *)s_73EEC0, "095 sub_73EEC0" },
		{ 0x73EF20, (void *)s_73EF20, "095 sub_73EF20" },
		{ 0x73EF60, (void *)s_73EF60, "095 sub_73EF60" },
		{ 0x73EFE0, (void *)s_73EFE0, "095 sub_73EFE0" },
		{ 0x73F030, (void *)s_73F030, "095 sub_73F030" },
		{ 0x73F070, (void *)s_73F070, "095 sub_73F070" },
		{ 0x73F0C0, (void *)s_73F0C0, "095 sub_73F0C0" },
		{ 0x73F180, (void *)s_73F180, "095 sub_73F180" },
		{ 0x73F1D0, (void *)s_73F1D0, "095 sub_73F1D0" },
		{ 0x73F1E0, (void *)s_73F1E0, "095 sub_73F1E0" },
		{ 0x73F220, (void *)s_73F220, "095 sub_73F220" },
		{ 0x73F250, (void *)s_73F250, "095 sub_73F250" },
		{ 0x73F2B0, (void *)s_73F2B0, "095 sub_73F2B0" },
		{ 0x73F300, (void *)s_73F300, "095 sub_73F300" },
		{ 0x73F310, (void *)s_73F310, "095 sub_73F310" },
		{ 0x73F3E0, (void *)s_73F3E0, "095 sub_73F3E0" },
		{ 0x73F430, (void *)s_73F430, "095 sub_73F430" },
		{ 0x73F440, (void *)s_73F440, "095 sub_73F440" },
		{ 0x73F4A0, (void *)s_73F4A0, "095 sub_73F4A0" },
		{ 0x73F570, (void *)s_73F570, "095 sub_73F570" },
		{ 0x73F5C0, (void *)s_73F5C0, "095 sub_73F5C0" },
		{ 0x73F5D0, (void *)s_73F5D0, "095 sub_73F5D0" },
		{ 0x73F640, (void *)s_73F640, "095 sub_73F640" },
		{ 0x73F6A0, (void *)s_73F6A0, "095 sub_73F6A0" },
		{ 0x73F710, (void *)s_73F710, "095 sub_73F710" },
		{ 0x73F760, (void *)s_73F760, "095 sub_73F760" },
		{ 0x73F770, (void *)s_73F770, "095 sub_73F770" },
		{ 0x73F790, (void *)s_73F790, "095 sub_73F790" },
		{ 0x73F7C0, (void *)s_73F7C0, "095 sub_73F7C0" },
		{ 0x73F860, (void *)s_73F860, "095 sub_73F860" },
		{ 0x73F8D0, (void *)s_73F8D0, "095 sub_73F8D0" },
		{ 0x73F960, (void *)s_73F960, "095 sub_73F960" },
		{ 0x740470, (void *)s_740470, "095 sub_740470" },
		{ 0x740500, (void *)s_740500, "095 sub_740500" },
		{ 0x740840, (void *)s_740840, "095 sub_740840" },
		{ 0x741B10, (void *)s_741B10, "095 sub_741B10" },
		{ 0x741B90, (void *)s_741B90, "095 sub_741B90" },
		{ 0x741C10, (void *)s_741C10, "095 sub_741C10" },
		{ 0x7423F0, (void *)s_7423F0, "095 sub_7423F0" },
		{ 0x743220, (void *)s_743220, "095 sub_743220" },
		{ 0x7432C0, (void *)s_7432C0, "095 sub_7432C0" },
		{ 0x7433C0, (void *)s_7433C0, "095 sub_7433C0" },
		{ 0x7434C0, (void *)s_7434C0, "095 sub_7434C0" },
		{ 0x743B90, (void *)s_743B90, "095 sub_743B90" },
		{ 0x743C70, (void *)s_743C70, "095 sub_743C70" },
		{ 0x743CD0, (void *)s_743CD0, "095 sub_743CD0" },
		{ 0x743D10, (void *)s_743D10, "095 sub_743D10" },
		{ 0x743DD0, (void *)s_743DD0, "095 sub_743DD0" },
		{ 0x743FD0, (void *)s_743FD0, "095 sub_743FD0" },
		{ 0x744010, (void *)s_744010, "095 sub_744010" },
		{ 0x744060, (void *)s_744060, "095 sub_744060" },
		{ 0x7440C0, (void *)s_7440C0, "095 sub_7440C0" },
		{ 0x744100, (void *)s_744100, "095 sub_744100" },
		{ 0x7441D0, (void *)s_7441D0, "095 sub_7441D0" },
		{ 0x7443C0, (void *)s_7443C0, "095 sub_7443C0" },
		{ 0x744400, (void *)s_744400, "095 sub_744400" },
		{ 0x744450, (void *)s_744450, "095 sub_744450" },
		{ 0x7444B0, (void *)s_7444B0, "095 sub_7444B0" },
		{ 0x7444F0, (void *)s_7444F0, "095 sub_7444F0" },
		{ 0x744570, (void *)s_744570, "095 sub_744570" },
		{ 0x744730, (void *)s_744730, "095 sub_744730" },
		{ 0x744770, (void *)s_744770, "095 sub_744770" },
		{ 0x744820, (void *)s_744820, "095 sub_744820" },
		{ 0x7448A0, (void *)s_7448A0, "095 sub_7448A0" },
		{ 0x744AA0, (void *)s_744AA0, "095 sub_744AA0" },
		{ 0x744BA0, (void *)s_744BA0, "095 sub_744BA0" },
		{ 0x744BE0, (void *)s_744BE0, "095 sub_744BE0" },
		{ 0x744C30, (void *)s_744C30, "095 sub_744C30" },
		{ 0x744D30, (void *)s_744D30, "095 sub_744D30" },
		{ 0x744E80, (void *)s_744E80, "095 sub_744E80" },
		{ 0x744ED0, (void *)s_744ED0, "095 sub_744ED0" },
		{ 0x744F30, (void *)s_744F30, "095 sub_744F30" },
		{ 0x744F90, (void *)s_744F90, "095 sub_744F90" },
		{ 0x744FF0, (void *)s_744FF0, "095 sub_744FF0" },
		{ 0x745010, (void *)s_745010, "095 sub_745010" },
		{ 0x745030, (void *)s_745030, "095 sub_745030" },
		{ 0x7451E0, (void *)s_7451E0, "095 sub_7451E0" },
		{ 0x745210, (void *)s_745210, "095 sub_745210" },
		{ 0x7452B0, (void *)s_7452B0, "095 sub_7452B0" },
		{ 0x7452F0, (void *)s_7452F0, "095 sub_7452F0" },
		{ 0x745380, (void *)s_745380, "095 sub_745380" },
		{ 0x745490, (void *)s_745490, "095 sub_745490" },
		{ 0x7455B0, (void *)s_7455B0, "095 sub_7455B0" },
		{ 0x7455F0, (void *)s_7455F0, "095 sub_7455F0" },
		{ 0x745610, (void *)s_745610, "095 sub_745610" },
		{ 0x745650, (void *)s_745650, "095 sub_745650" },
		{ 0x7456A0, (void *)s_7456A0, "095 sub_7456A0" },
		{ 0x745700, (void *)s_745700, "095 sub_745700" },
		{ 0x745730, (void *)s_745730, "095 sub_745730" },
		{ 0x745740, (void *)s_745740, "095 sub_745740" },
		{ 0x745840, (void *)s_745840, "095 sub_745840" },
		{ 0x7459D0, (void *)s_7459D0, "095 sub_7459D0" },
		{ 0x745A20, (void *)s_745A20, "095 sub_745A20" },
		{ 0x745A60, (void *)s_745A60, "095 sub_745A60" },
		{ 0x745A80, (void *)s_745A80, "095 sub_745A80" },
		{ 0x745AD0, (void *)s_745AD0, "095 sub_745AD0" },
		{ 0x745B40, (void *)s_745B40, "095 sub_745B40" },
		{ 0x745BB0, (void *)s_745BB0, "095 sub_745BB0" },
		{ 0x745BF0, (void *)s_745BF0, "095 sub_745BF0" },
		{ 0x745E80, (void *)s_745E80, "095 sub_745E80" },
		{ 0x745F50, (void *)s_745F50, "095 sub_745F50" },
		{ 0x7466A0, (void *)s_7466A0, "095 sub_7466A0" },
		{ 0x746760, (void *)s_746760, "095 sub_746760" },
		{ 0x7467B0, (void *)s_7467B0, "095 sub_7467B0" },
		{ 0x746800, (void *)s_746800, "095 sub_746800" },
		{ 0x746870, (void *)s_746870, "095 sub_746870" },
		{ 0x7469C0, (void *)s_7469C0, "095 sub_7469C0" },
		{ 0x746AA0, (void *)s_746AA0, "095 sub_746AA0" },
		{ 0x746B10, (void *)s_746B10, "095 sub_746B10" },
		{ 0x746B60, (void *)s_746B60, "095 GF_095Siren_CreatureActorTask" },
		{ 0x746DF0, (void *)s_746DF0, "095 sub_746DF0" },
		{ 0x746E50, (void *)s_746E50, "095 sub_746E50" },
		{ 0x746E80, (void *)s_746E80, "095 sub_746E80" },
		{ 0x746ED0, (void *)s_746ED0, "095 sub_746ED0" },
		{ 0x746F40, (void *)s_746F40, "095 sub_746F40" },
		{ 0x746FD0, (void *)s_746FD0, "095 sub_746FD0" },
		{ 0x747110, (void *)s_747110, "095 sub_747110" },
		{ 0x747160, (void *)s_747160, "095 sub_747160" },
		{ 0x747220, (void *)s_747220, "095 sub_747220" },
		{ 0x747320, (void *)s_747320, "095 sub_747320" },
		{ 0x747370, (void *)s_747370, "095 sub_747370" },
		{ 0x7473D0, (void *)s_7473D0, "095 sub_7473D0" },
		{ 0, nullptr, nullptr }
	};
}
}
	void register_mag095_siren()
	{
		act::register_module(95);
		for (const act::siren::ModPort *p = act::siren::PORTS; p->addr; p++)
			act::register_module_port(95, p->addr, p->port, p->name);
		// 30 fps layer: see mag095_siren_held.inc
		FX_HELD(register_mag095_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag095_siren_held.inc"
#endif
