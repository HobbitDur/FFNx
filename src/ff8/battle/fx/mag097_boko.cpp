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

// Effects 097-100: Boko - ChocoFire / ChocoFlare / ChocoMeteor / ChocoBocle (actor state-machine GF
// family, see act_engine.h). The four modules are copies of one effect program with per-summon
// parts:
//   * code present in several Boko modules is ported once (b_XXXXXX, canonical = the ChocoFire
//     copy, else the first module that has it); the module addresses it uses go through the Boko
//     descriptor BG()/BF() (BOKO_097..BOKO_100, selected by act::g_mod), like G()/F() of the engine;
//   * code of one module only is ported with raw addresses: b7_ ChocoFire, b8_ ChocoFlare,
//     b9_ ChocoMeteor, b10_ ChocoBocle.
// Masters: ChocoFire / ChocoFlare b_729BD0, ChocoMeteor / ChocoBocle the engine master a_739F40.
// Each director steps the phase script at its origin block; phases spawn the chocobo actor
// (b_7306A0 / b9_71E8D0 / b10_715540, drawn by a_746C10 then its texture animation a_739890), its
// shadow sprite (b_730C10), the prim-model particle tasks (b_72A720 -> draw callback b_72A840 or
// b10_70F7D0 with the Boko primitive-list renderers b_72BA00..b_72CAC0 / b10_70D530..b10_70E800),
// ChocoFire's emitter stage (b7_72FBC0), ChocoMeteor's meteor / trails / sparks (b9_720080..),
// ChocoBocle's target wobble (b10_716C20, custom renderer b10_716DD0 of the target's own model,
// entity +0 hide bit / +0xC) and chicobo creature (0x7175F0). Loads 0x48D0A0 go to 0x20F3218 and
// to fixed buffers 0x19D3018..0x19F3EF8.
// Module globals: 097 0x2575268..0x257B740, 098 0x256E358..0x2575268, 099 0x2565A30..0x256E358,
// 100 0x255EAC8..0x2565A30 (+ camera copy 0x2793E58).
// Held frames (30 fps): the creature actors (midpoint pose) and every prim-model task (all their
// packets come from the prim-model draw callbacks, redrawn by prim::play_held) are redrawn in
// between by act::held_frame; every other task stays on the generic packet path. Camera:
// act::held_camera.

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
	// engine functions not in act::x (file-local wrappers)
	namespace xm
	{
		static inline uint32_t GTE_SetBackgroundVector(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DCF0)(a1, a2, a3); }
		static inline uint32_t GTE_SetLightMatrix(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE50)(a1); }
		static inline uint32_t sub_45E0B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E0B0)(a1); }
		static inline uint32_t sub_45E160(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a1, a2, a3); }
		static inline uint32_t sub_45E220(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E220)(a1); }
		static inline uint32_t sub_45E9D0() { return fn<uint32_t (__cdecl *)()>(0x45E9D0)(); }
		static inline uint32_t sub_45EBF0() { return fn<uint32_t (__cdecl *)()>(0x45EBF0)(); }
		static inline uint32_t GTE_MVMVA_LightV0_Bk() { return fn<uint32_t (__cdecl *)()>(0x4607E0)(); }
		static inline uint32_t TransformCameraByShadowRotation(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x571BC0)(a1, a2, a3); }
		static inline uint32_t InitEffectSequenceFromData(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x571C80)(a1, a2, a3, a4); }
		static inline uint32_t MAG_063_sub_7015B0(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x7015B0)(a1, a2); }
	}
	// Boko modules (097-100): address classes of the code shared by several Boko modules
	// (canonical = the ChocoFire address when ChocoFire has it, else the first module that has it)
	enum BGIdx : int
	{
		BG_257B5BC,
		BG_2575378,
		BG_2579EBC,
		BG_2579544,
		BG_257526C,
		BG_2575374,
		BG_2579EB8,
		BG_2579540,
		BG_2579D48,
		BG_257B5A0,
		BG_257B168,
		BG_2575350,
		BG_257B5A8,
		BG_257B158,
		BG_2579D38,
		BG_2579530,
		BG_2575458,
		BG_152B5B8,
		BG_1683420,
		BG_168C654,
		BG_168F8BC,
		BG_1688434,
		BG_152B5BC,
		BG_257802C,
		BG_257B1E0,
		BG_257B5CC,
		BG_152BA38,
		BG_152BA80,
		BG_257AFB8,
		BG_168354C,
		BG_2579EC8,
		BG_152BB58,
		BG_152B5C8,
		BG_152B5C0,
		NBG
	};
	enum BFIdx : int
	{
		BF_729D50,
		BF_729D60,
		BF_729D70,
		BF_729DF0,
		BF_731CF0,
		BF_731D30,
		BF_731D40,
		BF_731D50,
		BF_731D80,
		BF_731D90,
		BF_731DB0,
		BF_72A380,
		BF_72AB20,
		BF_72A5C0,
		BF_72A660,
		BF_72A610,
		BF_72FBC0,
		BF_72A720,
		BF_72A840,
		BF_72FC70,
		BF_72FC90,
		BF_730490,
		BF_72FF80,
		BF_730610,
		BF_7306A0,
		BF_730B00,
		BF_730BA0,
		BF_730BD0,
		BF_730E30,
		BF_730EA0,
		BF_730EF0,
		BF_730F10,
		BF_730F50,
		BF_730F80,
		BF_730FB0,
		BF_730760,
		BF_730940,
		BF_730B60,
		BF_730C10,
		BF_730D00,
		BF_730D50,
		BF_730D70,
		BF_730DD0,
		BF_730E00,
		BF_730E20,
		BF_730C90,
		BF_730DA0,
		BF_730E80,
		BF_730EC0,
		BF_7318D0,
		BF_731920,
		BF_731940,
		BF_7311C0,
		BF_7319D0,
		BF_731A20,
		BF_731A40,
		BF_7186C0,
		NBF
	};
	struct BokoMod { int effect_id; uint32_t g[NBG]; uint32_t f[NBF]; };
	extern const BokoMod BOKO_097, BOKO_098, BOKO_099, BOKO_100;
	inline const BokoMod &boko_mod()
	{
		switch (g_mod->effect_id)
		{
		case 98: return BOKO_098;
		case 99: return BOKO_099;
		case 100: return BOKO_100;
		default: return BOKO_097;
		}
	}
	inline uint32_t BG(int k) { return boko_mod().g[k]; }
	inline uint32_t BF(int k) { return boko_mod().f[k]; }
	const BokoMod BOKO_097 = { 97,
		{ 0x257B5BC, 0x2575378, 0x2579EBC, 0x2579544, 0x257526C, 0x2575374, 0x2579EB8, 0x2579540, 0x2579D48, 0x257B5A0, 0x257B168, 0x2575350, 0x257B5A8, 0x257B158, 0x2579D38, 0x2579530, 0x2575458, 0x152B5B8, 0x1683420, 0x168C654, 0x168F8BC, 0x1688434, 0x152B5BC, 0x257802C, 0x257B1E0, 0x257B5CC, 0x152BA38, 0x152BA80, 0x257AFB8, 0x168354C, 0x2579EC8, 0x152BB58, 0x152B5C8, 0x152B5C0 },
		{ 0x729D50, 0x729D60, 0x729D70, 0x729DF0, 0x731CF0, 0x731D30, 0x731D40, 0x731D50, 0x731D80, 0x731D90, 0x731DB0, 0x72A380, 0x72AB20, 0x72A5C0, 0x72A660, 0x72A610, 0x72FBC0, 0x72A720, 0x72A840, 0x72FC70, 0x72FC90, 0x730490, 0x72FF80, 0x730610, 0x7306A0, 0x730B00, 0x730BA0, 0x730BD0, 0x730E30, 0x730EA0, 0x730EF0, 0x730F10, 0x730F50, 0x730F80, 0x730FB0, 0x730760, 0x730940, 0x730B60, 0x730C10, 0x730D00, 0x730D50, 0x730D70, 0x730DD0, 0x730E00, 0x730E20, 0x730C90, 0x730DA0, 0x730E80, 0x730EC0, 0x7318D0, 0x731920, 0x731940, 0x7311C0, 0x7319D0, 0x731A20, 0x731A40, 0x0 } };
	const BokoMod BOKO_098 = { 98,
		{ 0x25750E4, 0x256E468, 0x2573C3C, 0x25732C4, 0x256E35C, 0x256E464, 0x2573C38, 0x25732C0, 0x2573AC8, 0x25750C8, 0x2574EE8, 0x256E440, 0x25750D0, 0x2574ED8, 0x2573AB8, 0x25732B0, 0x256E548, 0x152AF30, 0x0, 0x0, 0x0, 0x0, 0x152AF34, 0x257111C, 0x2574F60, 0x25750F4, 0x152B3D0, 0x152B418, 0x2574D38, 0x167482C, 0x2573C48, 0x152B580, 0x152AF40, 0x152AF38 },
		{ 0x721B50, 0x721B60, 0x721B70, 0x721BF0, 0x729970, 0x7299B0, 0x7299C0, 0x7299D0, 0x729A00, 0x729A10, 0x729A30, 0x722180, 0x0, 0x0, 0x0, 0x0, 0x0, 0x722520, 0x722640, 0x7277B0, 0x7277D0, 0x727FD0, 0x727AC0, 0x728180, 0x728210, 0x728670, 0x728710, 0x728740, 0x7289A0, 0x728A10, 0x728A60, 0x728A80, 0x728AB0, 0x728AE0, 0x728B10, 0x7282D0, 0x7284B0, 0x7286D0, 0x728780, 0x728870, 0x7288C0, 0x7288E0, 0x728940, 0x728970, 0x728990, 0x728800, 0x728910, 0x7289F0, 0x728A30, 0x729550, 0x7295A0, 0x7295C0, 0x728E60, 0x729650, 0x7296A0, 0x7296C0, 0x0 } };
	const BokoMod BOKO_099 = { 99,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x256D9D0, 0x0, 0x0, 0x0, 0x1529FA0, 0x0, 0x0, 0x0, 0x0, 0x1529FA4, 0x2569334, 0x256DA58, 0x256E1E4, 0x152AC48, 0x152AC90, 0x256D6F0, 0x1665B08, 0x0, 0x152AED0, 0x0, 0x1529FA8 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x718A70, 0x718B90, 0x71DD00, 0x71DD20, 0x71E520, 0x0, 0x71E840, 0x71E8D0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x71ED80, 0x0, 0x71EF30, 0x71EF80, 0x71EFB0, 0x71F010, 0x71F050, 0x71F070, 0x71EEC0, 0x0, 0x71F0D0, 0x71F110, 0x71FD40, 0x71FD90, 0x71FDB0, 0x71F9E0, 0x71FE40, 0x71FE90, 0x71FEB0, 0x7186C0 } };
	const BokoMod BOKO_100 = { 100,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25656B8, 0x0, 0x2563420, 0x255ECB8, 0x15297F0, 0x1656CB8, 0x165FEEC, 0x1663154, 0x165BCCC, 0x15297F4, 0x256188C, 0x2565740, 0x256588C, 0x1529CD8, 0x1529D20, 0x25653D8, 0x1656DE4, 0x25642E8, 0x1529F18, 0x1529804, 0x15297F8 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x70FAF0, 0x70F550, 0x70F5F0, 0x70F5A0, 0x7148C0, 0x70F6B0, 0x70F7D0, 0x7149A0, 0x7149C0, 0x7151C0, 0x0, 0x7154B0, 0x715540, 0x7176B0, 0x717710, 0x7177E0, 0x717860, 0x7178A0, 0x717930, 0x717980, 0x7179C0, 0x7179F0, 0x717B70, 0x715600, 0x7157E0, 0x715A00, 0x715AB0, 0x715BA0, 0x715BF0, 0x715C10, 0x715C70, 0x715CA0, 0x715CC0, 0x715B30, 0x715C40, 0x715D20, 0x715300, 0x7167C0, 0x716810, 0x716830, 0x716460, 0x7168C0, 0x716910, 0x716940, 0x70F2E0 } };

	// module ports of this file (32-bit words in and out, cdecl, like the originals)
	uint32_t __cdecl b8_728050(uint32_t a1); // 0x728050 sub_728050, 13 insns, 098:728050
	uint32_t __cdecl b_729BD0(uint32_t a1); // 0x729BD0 MAG_097_sub_729BD0 TASK, 85 insns, 097:729BD0 098:7219D0
	uint32_t __cdecl b_72A350(void); // 0x72A350 MAG_097_sub_72A350, 13 insns, 097:72A350 098:722150
	uint32_t __cdecl b_72A540(uint32_t a1); // 0x72A540 sub_72A540, 36 insns, 097:72A540 100:70F4D0
	uint32_t __cdecl b_72A610(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6); // 0x72A610 sub_72A610, 17 insns, 097:72A610 100:70F5A0
	uint32_t __cdecl b_72A6C0(uint32_t a1); // 0x72A6C0 sub_72A6C0, 30 insns, 097:72A6C0 098:7224C0 099:718A10 100:70F650
	uint32_t __cdecl b_72A720(uint32_t a1); // 0x72A720 sub_72A720, 80 insns, 097:72A720 098:722520 099:718A70 100:70F6B0
	uint32_t __cdecl b_72A840(uint32_t a1, uint32_t a2, uint32_t a3); // 0x72A840 sub_72A840, 214 insns, 097:72A840 098:722640 099:718B90
	uint32_t __cdecl b_72AAF0(uint32_t a1); // 0x72AAF0 sub_72AAF0, 13 insns, 097:72AAF0,731570,731920,731A20 098:7228F0,728D30,729210,7295A0,7296A0 099:718E40,71FD90,71FE90 100:70FAC0,716810
	uint32_t __cdecl b_72AD60(uint32_t a1); // 0x72AD60 sub_72AD60, 66 insns, 097:72AD60 098:722B60 099:7190B0 100:70FD30
	uint32_t __cdecl b_72AFE0(void); // 0x72AFE0 sub_72AFE0, 70 insns, 097:72AFE0 098:722DE0 099:719330 100:70FFB0
	uint32_t __cdecl b_72BA00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x72BA00 sub_72BA00, 172 insns, 097:72BA00 098:723800 099:719D50 100:7109D0
	uint32_t __cdecl b_72BC10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x72BC10 sub_72BC10, 206 insns, 097:72BC10 098:723A10 099:719F60 100:710BE0
	uint32_t __cdecl b_72C370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x72C370 sub_72C370, 184 insns, 097:72C370 098:724170 099:71A6C0 100:711340
	uint32_t __cdecl b_72C5A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x72C5A0 sub_72C5A0, 226 insns, 097:72C5A0 098:7243A0 099:71A8F0 100:711570
	uint32_t __cdecl b_72C860(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x72C860 sub_72C860, 206 insns, 097:72C860 098:724660 099:71ABB0 100:711830
	uint32_t __cdecl b_72CAC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x72CAC0 sub_72CAC0, 245 insns, 097:72CAC0 098:7248C0 099:71AE10 100:711A90
	uint32_t __cdecl b_72E220(uint32_t a1, uint32_t a2); // 0x72E220 sub_72E220, 80 insns, 097:72E220 098:725D50 099:71C2A0 100:712F20
	uint32_t __cdecl b_72EF80(uint32_t a1); // 0x72EF80 sub_72EF80, 439 insns, 097:72EF80 098:726AB0 099:71D000 100:713C80
	uint32_t __cdecl b_72FC40(uint32_t a1); // 0x72FC40 sub_72FC40, 10 insns, 097:72FC40 098:727780 099:71DCD0 100:714970
	uint32_t __cdecl b_730460(uint32_t a1); // 0x730460 sub_730460, 14 insns, 097:730460 098:727FA0 099:71E4F0 100:715190
	uint32_t __cdecl b_730510(uint32_t a1); // 0x730510 sub_730510, 9 insns, 097:730510 098:728080
	uint32_t __cdecl b_7305D0(uint32_t a1); // 0x7305D0 sub_7305D0, 16 insns, 097:7305D0 098:728140 099:71E800 100:715470
	uint32_t __cdecl b_730660(uint32_t a1); // 0x730660 MAG_097_SpawnBokoCreature, 19 insns, 097:730660 098:7281D0 099:71E890 100:715500
	uint32_t __cdecl b_7306A0(uint32_t a1); // 0x7306A0 sub_7306A0 TASK, 47 insns, 097:7306A0 098:728210 100:7175F0
	uint32_t __cdecl b_730B00(uint32_t a1); // 0x730B00 sub_730B00, 26 insns, 097:730B00 098:728670 099:71ED20 100:7159A0
	uint32_t __cdecl b_730BD0(uint32_t a1); // 0x730BD0 sub_730BD0, 21 insns, 097:730BD0 098:728740 100:715A70
	uint32_t __cdecl b_730C10(uint32_t a1); // 0x730C10 sub_730C10 TASK, 34 insns, 097:730C10 098:728780 099:71EE40 100:715AB0
	uint32_t __cdecl b_730C90(uint32_t a1, uint32_t a2); // 0x730C90 sub_730C90, 32 insns, 097:730C90 098:728800 099:71EEC0 100:715B30
	uint32_t __cdecl b_730D00(uint32_t a1); // 0x730D00 sub_730D00, 20 insns, 097:730D00 098:728870 100:715BA0
	uint32_t __cdecl b_730D50(uint32_t a1); // 0x730D50 sub_730D50, 9 insns, 097:730D50 098:7288C0 100:715BF0
	uint32_t __cdecl b_730D70(uint32_t a1); // 0x730D70 sub_730D70, 13 insns, 097:730D70 098:7288E0 100:715C10
	uint32_t __cdecl b_730DD0(uint32_t a1); // 0x730DD0 sub_730DD0, 14 insns, 097:730DD0 098:728940 100:715C70
	uint32_t __cdecl b_730E30(uint32_t a1); // 0x730E30 sub_730E30, 25 insns, 097:730E30 098:7289A0 099:71F080 100:715CD0
	uint32_t __cdecl b_730EA0(uint32_t a1); // 0x730EA0 sub_730EA0, 12 insns, 097:730EA0 098:728A10 099:71F0F0 100:715D40
	uint32_t __cdecl b_730EF0(uint32_t a1); // 0x730EF0 sub_730EF0, 10 insns, 097:730EF0 098:728A60 099:71F140 100:715D60
	uint32_t __cdecl b_730F50(uint32_t a1); // 0x730F50 sub_730F50, 16 insns, 097:730F50 098:728AB0
	uint32_t __cdecl b_730FE0(uint32_t a1); // 0x730FE0 sub_730FE0, 11 insns, 097:730FE0 099:71F1F0 100:715E50
	uint32_t __cdecl b_731090(uint32_t a1); // 0x731090 sub_731090, 11 insns, 097:731090 098:728BE0 099:71F2A0
	uint32_t __cdecl b_731520(uint32_t a1); // 0x731520 sub_731520, 26 insns, 097:731520 098:7291C0
	uint32_t __cdecl b_7315D0(uint32_t a1); // 0x7315D0 sub_7315D0, 10 insns, 097:7315D0 100:716070
	uint32_t __cdecl b_731690(uint32_t a1); // 0x731690 sub_731690, 6 insns, 097:731690 098:729330 099:721160
	uint32_t __cdecl b_7316B0(uint32_t a1); // 0x7316B0 sub_7316B0, 18 insns, 097:7316B0 099:721180 100:716BD0
	uint32_t __cdecl b_731720(uint32_t a1); // 0x731720 sub_731720, 16 insns, 097:731720 098:7293C0
	uint32_t __cdecl b_731840(uint32_t a1); // 0x731840 sub_731840 TASK, 48 insns, 097:731840 098:7294C0 099:71F950 100:7163D0
	uint32_t __cdecl b_7318D0(uint32_t a1); // 0x7318D0 sub_7318D0, 27 insns, 097:7318D0 098:729550 099:71FD40 100:7167C0
	uint32_t __cdecl b_731950(uint32_t a1); // 0x731950 sub_731950 TASK, 38 insns, 097:731950 098:7295D0 099:71FDC0 100:716840
	uint32_t __cdecl b_7319D0(uint32_t a1); // 0x7319D0 sub_7319D0, 26 insns, 097:7319D0 098:729650 099:71FE40 100:7168C0
	uint32_t __cdecl b_731AF0(uint32_t a1); // 0x731AF0 sub_731AF0, 18 insns, 097:731AF0 098:729770 099:7210B0 100:716B00
	uint32_t __cdecl b_731BE0(uint32_t a1); // 0x731BE0 sub_731BE0, 18 insns, 097:731BE0 098:729860
	uint32_t __cdecl b_728A80(uint32_t a1); // 0x728A80 sub_728A80, 18 insns, 098:728A80 099:71F160 100:715D80
	uint32_t __cdecl b_718670(void); // 0x718670 MAG_099_sub_718670, 17 insns, 099:718670 100:70F290
	uint32_t __cdecl b_71F500(uint32_t a1); // 0x71F500 sub_71F500, 8 insns, 099:71F500 100:715F60
	uint32_t __cdecl b_71F8E0(uint32_t a1); // 0x71F8E0 sub_71F8E0, 10 insns, 099:71F8E0 100:716360
	uint32_t __cdecl b_71F900(uint32_t a1); // 0x71F900 sub_71F900, 20 insns, 099:71F900 100:716380
	uint32_t __cdecl b7_729EA0(uint32_t a1); // 0x729EA0 MAG_097_sub_729EA0, 21 insns, 097:729EA0
	uint32_t __cdecl b7_729EF0(void); // 0x729EF0 MAG_097_sub_729EF0, 40 insns, 097:729EF0
	uint32_t __cdecl b7_72A410(uint32_t a1); // 0x72A410 MAG_097_sub_72A410 TASK, 45 insns, 097:72A410
	uint32_t __cdecl b7_72A4F0(uint32_t a1); // 0x72A4F0 sub_72A4F0, 22 insns, 097:72A4F0
	uint32_t __cdecl b7_72AB80(uint32_t a1); // 0x72AB80 sub_72AB80, 15 insns, 097:72AB80
	uint32_t __cdecl b7_72B690(uint32_t a1); // 0x72B690 sub_72B690, 39 insns, 097:72B690
	uint32_t __cdecl b7_72CEE0(void); // 0x72CEE0 sub_72CEE0, 21 insns, 097:72CEE0
	uint32_t __cdecl b7_72CF20(uint32_t a1); // 0x72CF20 sub_72CF20, 46 insns, 097:72CF20
	uint32_t __cdecl b7_72D7C0(uint32_t a1, uint32_t a2); // 0x72D7C0 sub_72D7C0, 526 insns, 097:72D7C0
	uint32_t __cdecl b7_72DF60(uint32_t a1, uint32_t a2); // 0x72DF60 sub_72DF60, 244 insns, 097:72DF60
	uint32_t __cdecl b7_72FB90(uint32_t a1); // 0x72FB90 sub_72FB90, 13 insns, 097:72FB90
	uint32_t __cdecl b7_72FBC0(uint32_t a1); // 0x72FBC0 sub_72FBC0 TASK, 33 insns, 097:72FBC0
	uint32_t __cdecl b7_730F10(uint32_t a1); // 0x730F10 sub_730F10, 22 insns, 097:730F10
	uint32_t __cdecl b7_7310C0(uint32_t a1); // 0x7310C0 sub_7310C0, 21 insns, 097:7310C0
	uint32_t __cdecl b7_731100(uint32_t a1); // 0x731100 sub_731100 TASK, 68 insns, 097:731100
	uint32_t __cdecl b7_7315A0(uint32_t a1); // 0x7315A0 sub_7315A0, 11 insns, 097:7315A0
	uint32_t __cdecl b7_731600(uint32_t a1); // 0x731600 sub_731600, 16 insns, 097:731600
	uint32_t __cdecl b7_731780(uint32_t a1); // 0x731780 sub_731780, 58 insns, 097:731780
	uint32_t __cdecl b8_721CA0(uint32_t a1); // 0x721CA0 MAG_098_sub_721CA0, 21 insns, 098:721CA0
	uint32_t __cdecl b8_721CF0(void); // 0x721CF0 MAG_098_sub_721CF0, 40 insns, 098:721CF0
	uint32_t __cdecl b8_722210(uint32_t a1); // 0x722210 MAG_098_sub_722210 TASK, 46 insns, 098:722210
	uint32_t __cdecl b8_7222F0(uint32_t a1); // 0x7222F0 sub_7222F0, 22 insns, 098:7222F0
	uint32_t __cdecl b8_722340(uint32_t a1); // 0x722340 sub_722340, 36 insns, 098:722340
	uint32_t __cdecl b8_722410(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6); // 0x722410 sub_722410, 17 insns, 098:722410
	uint32_t __cdecl b8_7276F0(uint32_t a1); // 0x7276F0 sub_7276F0 TASK, 34 insns, 098:7276F0
	uint32_t __cdecl b8_728AE0(uint32_t a1); // 0x728AE0 sub_728AE0, 13 insns, 098:728AE0
	uint32_t __cdecl b8_728B20(uint32_t a1); // 0x728B20 sub_728B20, 11 insns, 098:728B20
	uint32_t __cdecl b8_728C10(uint32_t a1); // 0x728C10 sub_728C10, 22 insns, 098:728C10
	uint32_t __cdecl b8_728CC0(uint32_t a1); // 0x728CC0 MAG_310_HolyWar_Code, 31 insns, 098:728CC0
	uint32_t __cdecl b8_728D60(uint32_t a1); // 0x728D60 sub_728D60, 21 insns, 098:728D60
	uint32_t __cdecl b8_728DA0(uint32_t a1); // 0x728DA0 sub_728DA0 TASK, 68 insns, 098:728DA0
	uint32_t __cdecl b8_729240(uint32_t a1); // 0x729240 sub_729240, 11 insns, 098:729240
	uint32_t __cdecl b8_729270(uint32_t a1); // 0x729270 sub_729270, 10 insns, 098:729270
	uint32_t __cdecl b8_7292A0(uint32_t a1); // 0x7292A0 sub_7292A0, 16 insns, 098:7292A0
	uint32_t __cdecl b8_729350(uint32_t a1); // 0x729350 sub_729350, 18 insns, 098:729350
	uint32_t __cdecl b8_729420(uint32_t a1); // 0x729420 sub_729420, 47 insns, 098:729420
	uint32_t __cdecl b9_7181B0(uint32_t a1); // 0x7181B0 MAG_099_sub_7181B0, 21 insns, 099:7181B0
	uint32_t __cdecl b9_718200(void); // 0x718200 MAG_099_sub_718200, 40 insns, 099:718200
	uint32_t __cdecl b9_718750(uint32_t a1); // 0x718750 MAG_099_sub_718750 TASK, 48 insns, 099:718750
	uint32_t __cdecl b9_718840(uint32_t a1); // 0x718840 sub_718840, 22 insns, 099:718840
	uint32_t __cdecl b9_718890(uint32_t a1); // 0x718890 sub_718890, 36 insns, 099:718890
	uint32_t __cdecl b9_718960(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6); // 0x718960 sub_718960, 17 insns, 099:718960
	uint32_t __cdecl b9_71DC40(uint32_t a1); // 0x71DC40 sub_71DC40 TASK, 35 insns, 099:71DC40
	uint32_t __cdecl b9_71E570(uint32_t a1); // 0x71E570 sub_71E570, 21 insns, 099:71E570
	uint32_t __cdecl b9_71E5C0(uint32_t a1); // 0x71E5C0 sub_71E5C0, 21 insns, 099:71E5C0
	uint32_t __cdecl b9_71E610(uint32_t a1); // 0x71E610 sub_71E610, 38 insns, 099:71E610
	uint32_t __cdecl b9_71E6C0(uint32_t a1); // 0x71E6C0 sub_71E6C0, 17 insns, 099:71E6C0
	uint32_t __cdecl b9_71E700(uint32_t a1); // 0x71E700 sub_71E700, 26 insns, 099:71E700
	uint32_t __cdecl b9_71E8D0(uint32_t a1); // 0x71E8D0 sub_71E8D0 TASK, 46 insns, 099:71E8D0
	uint32_t __cdecl b9_71EDF0(uint32_t a1); // 0x71EDF0 sub_71EDF0, 21 insns, 099:71EDF0
	uint32_t __cdecl b9_71EF30(uint32_t a1); // 0x71EF30 sub_71EF30, 20 insns, 099:71EF30
	uint32_t __cdecl b9_71EF80(uint32_t a1); // 0x71EF80 sub_71EF80, 9 insns, 099:71EF80
	uint32_t __cdecl b9_71EFB0(uint32_t a1); // 0x71EFB0 sub_71EFB0, 13 insns, 099:71EFB0
	uint32_t __cdecl b9_71F010(uint32_t a1); // 0x71F010 sub_71F010, 14 insns, 099:71F010
	uint32_t __cdecl b9_71F190(uint32_t a1); // 0x71F190 sub_71F190, 15 insns, 099:71F190
	uint32_t __cdecl b9_71F2D0(uint32_t a1); // 0x71F2D0 sub_71F2D0, 16 insns, 099:71F2D0
	uint32_t __cdecl b9_71F3A0(uint32_t a1); // 0x71F3A0 sub_71F3A0, 18 insns, 099:71F3A0
	uint32_t __cdecl b9_71F450(uint32_t a1); // 0x71F450 sub_71F450, 6 insns, 099:71F450
	uint32_t __cdecl b9_71F470(uint32_t a1); // 0x71F470 sub_71F470, 18 insns, 099:71F470
	uint32_t __cdecl b9_71F4C0(uint32_t a1); // 0x71F4C0 sub_71F4C0, 11 insns, 099:71F4C0
	uint32_t __cdecl b9_71F520(uint32_t a1); // 0x71F520 sub_71F520, 11 insns, 099:71F520
	uint32_t __cdecl b9_71F550(uint32_t a1); // 0x71F550 sub_71F550, 11 insns, 099:71F550
	uint32_t __cdecl b9_71F5A0(uint32_t a1); // 0x71F5A0 sub_71F5A0, 11 insns, 099:71F5A0
	uint32_t __cdecl b9_71F5C0(uint32_t a1); // 0x71F5C0 sub_71F5C0, 16 insns, 099:71F5C0
	uint32_t __cdecl b9_71F620(uint32_t a1); // 0x71F620 sub_71F620, 162 insns, 099:71F620
	uint32_t __cdecl b9_71FF20(uint32_t a1); // 0x71FF20 sub_71FF20, 25 insns, 099:71FF20
	uint32_t __cdecl b9_71FF70(uint32_t a1); // 0x71FF70 sub_71FF70, 15 insns, 099:71FF70
	uint32_t __cdecl b9_720020(uint32_t a1); // 0x720020 sub_720020, 26 insns, 099:720020
	uint32_t __cdecl b9_720080(uint32_t a1); // 0x720080 sub_720080, 33 insns, 099:720080
	uint32_t __cdecl b9_7200F0(uint32_t a1); // 0x7200F0 sub_7200F0, 18 insns, 099:7200F0
	uint32_t __cdecl b9_720140(uint32_t a1); // 0x720140 sub_720140, 45 insns, 099:720140
	uint32_t __cdecl b9_720250(uint32_t a1); // 0x720250 sub_720250, 21 insns, 099:720250
	uint32_t __cdecl b9_720290(uint32_t a1); // 0x720290 sub_720290, 22 insns, 099:720290
	uint32_t __cdecl b9_7202F0(uint32_t a1); // 0x7202F0 sub_7202F0, 13 insns, 099:7202F0
	uint32_t __cdecl b9_720380(uint32_t a1); // 0x720380 sub_720380, 25 insns, 099:720380
	uint32_t __cdecl b9_7203D0(uint32_t a1); // 0x7203D0 sub_7203D0, 15 insns, 099:7203D0
	uint32_t __cdecl b9_720470(uint32_t a1); // 0x720470 sub_720470, 25 insns, 099:720470
	uint32_t __cdecl b9_7204C0(uint32_t a1); // 0x7204C0 sub_7204C0, 15 insns, 099:7204C0
	uint32_t __cdecl b9_720500(uint32_t a1); // 0x720500 sub_720500 TASK, 32 insns, 099:720500
	uint32_t __cdecl b9_720570(uint32_t a1); // 0x720570 sub_720570, 10 insns, 099:720570
	uint32_t __cdecl b9_7205A0(uint32_t a1); // 0x7205A0 sub_7205A0, 9 insns, 099:7205A0
	uint32_t __cdecl b9_7205D0(uint32_t a1); // 0x7205D0 sub_7205D0, 23 insns, 099:7205D0
	uint32_t __cdecl b9_720670(uint32_t a1); // 0x720670 sub_720670, 27 insns, 099:720670
	uint32_t __cdecl b9_7206D0(uint32_t a1); // 0x7206D0 sub_7206D0, 46 insns, 099:7206D0
	uint32_t __cdecl b9_720790(uint32_t a1); // 0x720790 sub_720790 TASK, 27 insns, 099:720790
	uint32_t __cdecl b9_7207E0(uint32_t a1); // 0x7207E0 sub_7207E0, 77 insns, 099:7207E0
	uint32_t __cdecl b9_7208D0(uint32_t a1); // 0x7208D0 sub_7208D0 TASK, 31 insns, 099:7208D0
	uint32_t __cdecl b9_720940(uint32_t a1); // 0x720940 sub_720940, 8 insns, 099:720940
	uint32_t __cdecl b9_720960(uint32_t a1); // 0x720960 sub_720960, 18 insns, 099:720960
	uint32_t __cdecl b9_7209B0(uint32_t a1, uint32_t a2); // 0x7209B0 sub_7209B0, 84 insns, 099:7209B0
	uint32_t __cdecl b9_720AE0(uint32_t a1, uint32_t a2); // 0x720AE0 sub_720AE0, 54 insns, 099:720AE0
	uint32_t __cdecl b9_720B90(uint32_t a1); // 0x720B90 sub_720B90 TASK, 88 insns, 099:720B90
	uint32_t __cdecl b9_720C90(uint32_t a1); // 0x720C90 sub_720C90, 34 insns, 099:720C90
	uint32_t __cdecl b9_720D00(uint32_t a1); // 0x720D00 sub_720D00, 7 insns, 099:720D00
	uint32_t __cdecl b9_720D20(uint32_t a1); // 0x720D20 sub_720D20, 19 insns, 099:720D20
	uint32_t __cdecl b9_720D70(uint32_t a1, uint32_t a2); // 0x720D70 sub_720D70, 91 insns, 099:720D70
	uint32_t __cdecl b9_720EA0(uint32_t a1); // 0x720EA0 sub_720EA0 TASK, 89 insns, 099:720EA0
	uint32_t __cdecl b9_720FA0(uint32_t a1); // 0x720FA0 sub_720FA0, 7 insns, 099:720FA0
	uint32_t __cdecl b9_720FC0(uint32_t a1); // 0x720FC0 sub_720FC0, 19 insns, 099:720FC0
	uint32_t __cdecl b9_7211D0(uint32_t a1); // 0x7211D0 sub_7211D0 TASK, 30 insns, 099:7211D0
	uint32_t __cdecl b9_721320(uint32_t a1); // 0x721320 sub_721320, 15 insns, 099:721320
	uint32_t __cdecl b9_721360(uint32_t a1); // 0x721360 sub_721360, 11 insns, 099:721360
	uint32_t __cdecl b9_721390(uint32_t a1); // 0x721390 sub_721390 TASK, 30 insns, 099:721390
	uint32_t __cdecl b9_7213F0(uint32_t a1); // 0x7213F0 sub_7213F0, 14 insns, 099:7213F0
	uint32_t __cdecl b9_721430(uint32_t a1); // 0x721430 sub_721430, 9 insns, 099:721430
	uint32_t __cdecl b9_7214C0(uint32_t a1); // 0x7214C0 sub_7214C0, 8 insns, 099:7214C0
	uint32_t __cdecl b9_7214E0(uint32_t a1); // 0x7214E0 sub_7214E0, 18 insns, 099:7214E0
	uint32_t __cdecl b9_721610(uint32_t a1); // 0x721610 sub_721610, 8 insns, 099:721610
	uint32_t __cdecl b9_721630(uint32_t a1); // 0x721630 sub_721630, 11 insns, 099:721630
	uint32_t __cdecl b9_721660(uint32_t a1); // 0x721660 sub_721660, 6 insns, 099:721660
	uint32_t __cdecl b10_70D530(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5); // 0x70D530 sub_70D530, 148 insns, 100:70D530
	uint32_t __cdecl b10_70D680(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5); // 0x70D680 sub_70D680, 180 insns, 100:70D680
	uint32_t __cdecl b10_70D8A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5); // 0x70D8A0 sub_70D8A0, 214 insns, 100:70D8A0
	uint32_t __cdecl b10_70DB40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5); // 0x70DB40 sub_70DB40, 204 insns, 100:70DB40
	uint32_t __cdecl b10_70DDA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5); // 0x70DDA0 sub_70DDA0, 232 insns, 100:70DDA0
	uint32_t __cdecl b10_70E060(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5); // 0x70E060 sub_70E060, 192 insns, 100:70E060
	uint32_t __cdecl b10_70E2A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5); // 0x70E2A0 sub_70E2A0, 234 insns, 100:70E2A0
	uint32_t __cdecl b10_70E580(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5); // 0x70E580 sub_70E580, 215 insns, 100:70E580
	uint32_t __cdecl b10_70E800(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5); // 0x70E800 sub_70E800, 253 insns, 100:70E800
	uint32_t __cdecl b10_70EDE0(uint32_t a1); // 0x70EDE0 MAG_100_sub_70EDE0, 21 insns, 100:70EDE0
	uint32_t __cdecl b10_70EE30(void); // 0x70EE30 MAG_100_sub_70EE30, 40 insns, 100:70EE30
	uint32_t __cdecl b10_70F370(uint32_t a1); // 0x70F370 MAG_100_sub_70F370 TASK, 51 insns, 100:70F370
	uint32_t __cdecl b10_70F480(uint32_t a1); // 0x70F480 sub_70F480, 22 insns, 100:70F480
	uint32_t __cdecl b10_70F7D0(uint32_t a1, uint32_t a2, uint32_t a3); // 0x70F7D0 sub_70F7D0, 237 insns, 100:70F7D0
	uint32_t __cdecl b10_7148C0(uint32_t a1); // 0x7148C0 MAG_100_BokoCreatureTask TASK, 39 insns, 100:7148C0
	uint32_t __cdecl b10_715210(uint32_t a1); // 0x715210 sub_715210, 16 insns, 100:715210
	uint32_t __cdecl b10_715250(uint32_t a1); // 0x715250 sub_715250, 15 insns, 100:715250
	uint32_t __cdecl b10_715280(uint32_t a1); // 0x715280 sub_715280, 19 insns, 100:715280
	uint32_t __cdecl b10_7152C0(uint32_t a1); // 0x7152C0 sub_7152C0, 7 insns, 100:7152C0
	uint32_t __cdecl b10_7152E0(uint32_t a1); // 0x7152E0 sub_7152E0, 9 insns, 100:7152E0
	uint32_t __cdecl b10_715330(uint32_t a1); // 0x715330 sub_715330, 20 insns, 100:715330
	uint32_t __cdecl b10_7153B0(uint32_t a1); // 0x7153B0 sub_7153B0, 9 insns, 100:7153B0
	uint32_t __cdecl b10_715540(uint32_t a1); // 0x715540 sub_715540 TASK, 48 insns, 100:715540
	uint32_t __cdecl b10_715DB0(uint32_t a1); // 0x715DB0 sub_715DB0, 10 insns, 100:715DB0
	uint32_t __cdecl b10_715DD0(uint32_t a1); // 0x715DD0 sub_715DD0, 12 insns, 100:715DD0
	uint32_t __cdecl b10_715DF0(uint32_t a1); // 0x715DF0 sub_715DF0, 15 insns, 100:715DF0
	uint32_t __cdecl b10_715F80(uint32_t a1); // 0x715F80 sub_715F80, 19 insns, 100:715F80
	uint32_t __cdecl b10_716040(uint32_t a1); // 0x716040 sub_716040, 11 insns, 100:716040
	uint32_t __cdecl b10_7160C0(uint32_t a1); // 0x7160C0 sub_7160C0, 16 insns, 100:7160C0
	uint32_t __cdecl b10_716120(uint32_t a1); // 0x716120 sub_716120, 136 insns, 100:716120
	uint32_t __cdecl b10_716910(uint32_t a1); // 0x716910 sub_716910, 15 insns, 100:716910
	uint32_t __cdecl b10_7169B0(uint32_t a1); // 0x7169B0 sub_7169B0, 30 insns, 100:7169B0
	uint32_t __cdecl b10_716A20(uint32_t a1); // 0x716A20 sub_716A20, 15 insns, 100:716A20
	uint32_t __cdecl b10_716BB0(uint32_t a1); // 0x716BB0 sub_716BB0, 6 insns, 100:716BB0
	uint32_t __cdecl b10_716C20(uint32_t a1); // 0x716C20 sub_716C20 TASK, 43 insns, 100:716C20
	uint32_t __cdecl b10_716CD0(uint32_t a1); // 0x716CD0 sub_716CD0, 84 insns, 100:716CD0
	uint32_t __cdecl b10_716DD0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x716DD0 sub_716DD0, 426 insns, 100:716DD0
	uint32_t __cdecl b10_717330(uint32_t a1); // 0x717330 sub_717330, 6 insns, 100:717330
	uint32_t __cdecl b10_717350(uint32_t a1); // 0x717350 sub_717350, 71 insns, 100:717350
	uint32_t __cdecl b10_717450(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x717450 sub_717450, 29 insns, 100:717450
	uint32_t __cdecl b10_7174B0(uint32_t a1); // 0x7174B0 sub_7174B0, 6 insns, 100:7174B0
	uint32_t __cdecl b10_7174D0(uint32_t a1); // 0x7174D0 sub_7174D0, 23 insns, 100:7174D0
	uint32_t __cdecl b10_717540(uint32_t a1); // 0x717540 sub_717540, 15 insns, 100:717540
	uint32_t __cdecl b10_717590(uint32_t a1); // 0x717590 sub_717590, 19 insns, 100:717590
	uint32_t __cdecl b10_7176B0(uint32_t a1); // 0x7176B0 sub_7176B0, 26 insns, 100:7176B0
	uint32_t __cdecl b10_717710(uint32_t a1); // 0x717710 sub_717710, 15 insns, 100:717710
	uint32_t __cdecl b10_717750(uint32_t a1); // 0x717750 sub_717750, 7 insns, 100:717750
	uint32_t __cdecl b10_717770(uint32_t a1); // 0x717770 sub_717770, 38 insns, 100:717770
	uint32_t __cdecl b10_7177E0(uint32_t a1); // 0x7177E0 sub_7177E0, 9 insns, 100:7177E0
	uint32_t __cdecl b10_717800(uint32_t a1); // 0x717800 sub_717800, 21 insns, 100:717800
	uint32_t __cdecl b10_717860(uint32_t a1); // 0x717860 __cintrindisp1, 15 insns, 100:717860
	uint32_t __cdecl b10_7178A0(uint32_t a1); // 0x7178A0 sub_7178A0, 27 insns, 100:7178A0
	uint32_t __cdecl b10_717910(uint32_t a1); // 0x717910 sub_717910, 7 insns, 100:717910
	uint32_t __cdecl b10_717930(uint32_t a1); // 0x717930 sub_717930, 21 insns, 100:717930
	uint32_t __cdecl b10_717980(uint32_t a1); // 0x717980 sub_717980, 17 insns, 100:717980
	uint32_t __cdecl b10_7179C0(uint32_t a1); // 0x7179C0 sub_7179C0, 11 insns, 100:7179C0
	uint32_t __cdecl b10_7179F0(uint32_t a1); // 0x7179F0 sub_7179F0, 21 insns, 100:7179F0
	uint32_t __cdecl b10_717A50(uint32_t a1, uint32_t a2); // 0x717A50 sub_717A50, 10 insns, 100:717A50

	// ====================================================================================
	// part b1
	// ====================================================================================
	// Boko modules (097 ChocoFire / 098 ChocoFlare / 099 ChocoMeteor / 100 ChocoBocle), part b1:
	// code shared by several Boko modules - master task, effect-origin setup, prim-model (particle)
	// task setup + per-tick prim-model play and its draw callback, small state handlers, and the
	// flat-shaded triangle / quad mesh emitters (never run in the harness).
	// Module addresses go through BG()/BF() (the address in the current Boko module).
	namespace
	{
		// vertex frame f of a prim-model mesh: base + 0xC + f * vertex_count * 8
		inline uint32_t b1_frame_ptr(uint32_t base, int16_t f)
		{
			if (f == 0)
				return base + 0xC;
			const int32_t n = mul32(S32(base, 4), (int32_t)f);
			return base + (uint32_t)shl32(n, 3) + 0xC;
		}
		// 16-byte copy done dword by dword (read then write, like the mov pairs of the listing)
		inline void b1_copy16(uint32_t dst, uint32_t src)
		{
			for (int i = 0; i < 16; i += 4)
				U32(dst, i) = U32(src, i);
		}
		// screen-range test of a projected coordinate (s16): outside [0, lim] -> true
		inline bool b1_out(int16_t v, int16_t lim)
		{
			return v < 0 || v > lim;
		}
	}

	// 0x729BD0 (Boko master task, 097 MAG_097_sub_729BD0, 098 0x7219D0): copies the camera matrix,
	// picks this tick's double-buffered globals, runs the state handler +0x29, then the 7 task queues
	uint32_t __cdecl b_729BD0(uint32_t a1)
	{
		set_mod_by_code(U32(a1, 8));      // node +8 = original task function address -> current module
		g_ported_tick = g_real_tick;

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);  // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(BG(BG_257B5BC)) = 0x2793E58;
		MEM<uint32_t>(BG(BG_2575378)) = 0x2793E58;
		states[0] = BF(BF_729D50);
		const uint8_t parity = U8(node, 0x5C);  // low byte of the tick counter +0x5C
		states[1] = BF(BF_729D60);
		states[2] = BF(BF_729D70);
		states[3] = BF(BF_729DF0);
		states[4] = BF(BF_731CF0);
		states[5] = BF(BF_731D30);
		states[6] = BF(BF_731D40);
		states[7] = BF(BF_731D50);
		states[8] = BF(BF_731D80);
		states[9] = BF(BF_731D90);
		states[10] = BF(BF_731DB0);  // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(BG(BG_2579EBC));
			const uint32_t v2 = MEM<uint32_t>(BG(BG_2579544));
			MEM<uint32_t>(BG(BG_257526C)) = v1;
			MEM<uint32_t>(BG(BG_2575374)) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(BG(BG_2579EB8));
			const uint32_t v2 = MEM<uint32_t>(BG(BG_2579540));
			MEM<uint32_t>(BG(BG_257526C)) = v1;
			MEM<uint32_t>(BG(BG_2575374)) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;               // live task count of this tick
		MEM<uint16_t>(BG(BG_257B5A0)) = 0;
		MEM<uint16_t>(BG(BG_257B168)) = 0;
		const uint32_t queues[7] = { BG(BG_2579D48), BG(BG_2575350), BG(BG_257B5A8), BG(BG_257B158),
			BG(BG_2579D38), BG(BG_2579530), BG(BG_2575458) };
		for (int i = 0; i < 7; i++)
		{
			const uint32_t n = x::ExecuteTaskQueue(queues[i]);
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)n);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x72A350 (097 MAG_097_sub_72A350, 098 0x722150): clears the effect origin block [BG_152B5B8]
	// (0x54 bytes), sets its angle word +4 = 0x800 and centres +8 on the live battle entities
	uint32_t __cdecl b_72A350(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(BG(BG_152B5B8)), 0x54);
		const uint32_t p = MEM<uint32_t>(BG(BG_152B5B8));
		U16(p, 0) = 0;
		U16(p, 2) = 0;
		U16(p, 4) = 0x800;
		return a_73A720(p + 8);
	}

	// 0x718670 (099 MAG_099_sub_718670, 100 0x70F290): like b_72A350, then clamps the centre x
	// (+8) of the origin block to >= 0
	uint32_t __cdecl b_718670(void)
	{
		x::MAG_007_sub_8DCC00(MEM<uint32_t>(BG(BG_152B5B8)), 0x54);
		uint32_t p = MEM<uint32_t>(BG(BG_152B5B8));
		U16(p, 0) = 0;
		U16(p, 2) = 0;
		U16(p, 4) = 0x800;
		a_73A720(p + 8);
		p = MEM<uint32_t>(BG(BG_152B5B8));
		if (S16(p, 8) <= 0)
			U16(p, 8) = 0;
		return p;
	}

	// 0x71F500 (099 sub_71F500, 100 0x715F60): state handler - waits for the script/state
	// condition a_73B640(6), then advances the state index +0x29
	uint32_t __cdecl b_71F500(uint32_t a1)
	{
		const uint32_t r = a_73B640(6);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x71F8E0 (099 sub_71F8E0, 100 0x716360): state handler - counts the wait timer +0x38 down,
	// at <= 0 clears it and advances the state index +0x29
	uint32_t __cdecl b_71F8E0(uint32_t a1)
	{
		U16(a1, 0x38) = (uint16_t)(U16(a1, 0x38) - 1);
		const int16_t t = S16(a1, 0x38);
		if (t > 0)
			return a1;
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x38) = 0;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x71F900 (099 sub_71F900, 100 0x716380): state handler - steps the word list [+0x34] at
	// index +0x38: 0x7F ends it (finished flag, next state), else the word goes to the shared
	// battle global 0x1D97712
	uint32_t __cdecl b_71F900(uint32_t a1)
	{
		const int16_t idx = S16(a1, 0x38);
		const uint32_t list = U32(a1, 0x34);
		const uint16_t w = U16(list, (int32_t)idx * 2);
		if (w == 0x7F)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
			U16(a1, 0x38) = (uint16_t)(idx + 1);
			return a1;
		}
		MEM<uint16_t>(0x1D97712) = w;
		U16(a1, 0x38) = (uint16_t)(idx + 1);
		return a1;
	}

	// 0x728A80 (098 sub_728A80, 099 0x71F160, 100 0x715D80): creature state handler - steps the
	// model animation; once the script condition a_73B640(4) holds, sets animation 4 and advances +0x29
	uint32_t __cdecl b_728A80(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (a_73B640(4) == 0)
			return 0; // void
		x::au_re_Battle_ReadAnimation_7(a1, 4);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x72A540 (097 sub_72A540, 100 0x70F4D0): director state handler - claims the voice slot
	// (stored in origin block +0x52), spawns the model task (a_73C100), the prim-model task
	// (b_72A610), the 0x70-byte task BF_72FBC0 into queue BG_2575458, queues the TIM upload
	// BG_1688434 and advances +0x29
	uint32_t __cdecl b_72A540(uint32_t a1)
	{
		const uint32_t slot = x::BdSound_ClaimVoiceSlot(BG(BG_1683420), 1, 0x80);
		const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
		const uint32_t node = a1;
		U16(origin, 0x52) = (uint16_t)slot;
		a_73C100(node, BF(BF_72AB20), BG(BG_168C654), 0x12, 0x2D, 2);
		b_72A610(node, BF(BF_72A660), BG(BG_168F8BC), 0x4AC, 0, 0);
		x::Effect_AddTaskAndInitFromCtx(BG(BG_2575458), BF(BF_72FBC0), 0x70, node);
		x::Battle_QueueTIMUpload_GetEOF(BG(BG_1688434));
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x72A610 (097 sub_72A610, 100 0x70F5A0): spawns a 0x540-byte prim-model task a2 (parent a1)
	// into queue BG_2579530: +0x74 = model data a3, +0x78 = (s16)a4, +0x80 = a5, +0x82 = a6
	uint32_t __cdecl b_72A610(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t node = x::Effect_AddTaskAndInitFromCtx(BG(BG_2579530), a2, 0x540, a1);
		U16(node, 0x80) = (uint16_t)a5;
		U32(node, 0x74) = a3;
		U32(node, 0x78) = (uint32_t)(int32_t)(int16_t)a4;
		U16(node, 0x82) = (uint16_t)a6;
		return node;
	}

	// 0x72A6C0 (097 sub_72A6C0, 098 0x7224C0, 099 0x718A10, 100 0x70F650): prim-model task state 0 -
	// decodes the prim layout (+0x74 data, +0x78 id) into +0x94, position = origin block, scale 1.0,
	// plays the first frame (b_72A720), plays SE BG_152B5BC, advances +0x29
	uint32_t __cdecl b_72A6C0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
		const uint32_t o0 = U32(origin, 0);
		const uint32_t o4 = U32(origin, 4);
		U32(node, 0x1C) = o0;
		U32(node, 0x20) = o4;
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		b_72A720(node);
		x::BdPlaySE(BG(BG_152B5BC), 0, 0x80);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x72A720 (097 sub_72A720, 098 0x722520, 099 0x718A70, 100 0x70F6B0): builds the prim-model
	// effect matrix (rotations +0x62/+0x60/+0x64, scale +0x50, position +0x1C, camera) and the 0x5C
	// parameter block, then plays the prim-model layout +0x94 with the draw callback BF_72A840;
	// returns the player result (0 = play finished)
	uint32_t __cdecl b_72A720(uint32_t a1)
	{
		// stack block (0x5C bytes) handed to the callback: +0x00 Mat4x3 effect matrix,
		// +0x38 = node+0x70, +0x44 = node+0x7C, +0x48 = BG_257B1E0 (vertex lerp scratch),
		// +0x4C..+0x56 words = node +0x8C/+0x8E/+0x90/+0x92/+0x86/+0x80. +0x20..+0x37, +0x3C..+0x43,
		// +0x58 are never written (UNINIT in the original, zeroed here; the callback b_72A840 does
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
		U16(L, 0x54) = U16(node, 0x86);
		const uint32_t v70 = U32(node, 0x70);
		const uint32_t v7c = U32(node, 0x7C);
		U16(L, 0x4C) = U16(node, 0x8C);
		U32(L, 0x38) = v70;
		const uint16_t w90 = U16(node, 0x90);
		const uint16_t w92 = U16(node, 0x92);
		U32(L, 0x44) = v7c;
		const uint16_t w8e = U16(node, 0x8E);
		U16(L, 0x50) = w90;
		const uint32_t paused = MEM<uint32_t>(BG(BG_257802C));
		U16(L, 0x52) = w92;
		U16(L, 0x4E) = w8e;
		const uint16_t w80 = U16(node, 0x80);
		U32(L, 0x48) = BG(BG_257B1E0);
		U16(L, 0x56) = w80;
		return prim_play(node + 0x94, BF(BF_72A840), L, paused);
	}

	// 0x72A840 (097 sub_72A840, 098 0x722640, 099 0x718B90): prim-model record draw callback -
	// picks / blends the vertex frame, builds the object matrix (record rotation, position rotated
	// by the effect matrix unless flag 0x200, optional scale) and renders the mesh into the effect
	// OT (Effect_RenderPrimModel, 0x58-byte Field_Alloc header)
	uint32_t __cdecl b_72A840(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack (0x48 bytes): +0x00 SVECTOR position (+6 pad unwritten), +0x08 scale (3x3 s16
		// diagonal matrix or 3 x int32), +0x28 Mat4x3 object matrix (+0x3C translation)
		alignas(4) uint8_t loc[0x48] = {};
		const uint32_t L = P(loc);
		const uint32_t M = L + 0x28;
		const uint32_t rec = a2;
		if (((uint32_t)(int32_t)S16(rec, 0x1C) | U32(rec, 0x18)) == 0)
			return 0; // zero scale: nothing drawn (eax = 0)
		if (S16(rec, 0x24) >= 0x1000 && U32(rec, 0x20) == 0)
			return 0; // full alpha with black colour: nothing drawn (eax = 0)

		const uint32_t hdr = x::Field_Alloc(0x58);
		const int32_t model = S16(rec, 2);
		const uint32_t data = U32(a1, 0);
		uint32_t blk = a3;
		const uint32_t base = data + U32(data + (uint32_t)shl32(model, 2), 8);
		const int16_t f1 = S16(rec, 0x2A);   // second vertex frame
		const int16_t f0 = S16(rec, 0x28);   // first vertex frame
		U32(hdr, 0) = base;
		if (f0 == f1)
			U32(hdr, 4) = b1_frame_ptr(base, f0);
		else
		{
			const int16_t t = S16(rec, 0x26);   // blend factor 0..0x1000
			if (t == 0)
				U32(hdr, 4) = b1_frame_ptr(base, f0);
			else if (t == 0x1000)
				U32(hdr, 4) = b1_frame_ptr(base, f1);
			else
			{
				const uint32_t out = U32(blk, 0x48);
				x::MAG_017_sub_701390(base, (uint32_t)(int32_t)f0, (uint32_t)(int32_t)f1, (uint32_t)(int32_t)t, out);
				blk = a3;
				U32(hdr, 4) = U32(blk, 0x48);
			}
		}

		if ((U32(rec, 4) & 0x40000) != 0)
			xm::MAG_063_sub_7015B0(rec + 0x10, M);
		else
			x::MAG_017_sub_701310(rec + 0x10, M);
		const uint16_t vx = U16(rec, 8);
		const uint16_t vy = U16(rec, 0xA);
		const uint16_t vz = U16(rec, 0xC);
		const bool raw_pos = (U8(rec, 5) & 2) != 0;   // flag 0x200: position not rotated
		U16(L, 0) = vx;
		U16(L, 2) = vy;
		U16(L, 4) = vz;
		int32_t tx, ty, tz;
		if (raw_pos)
		{
			tx = (int16_t)vx;
			ty = (int16_t)vy;
			tz = (int16_t)vz;
		}
		else
		{
			x::GTE_SetRotMatrix(blk);
			x::GTE_LoadV0(L);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			x::GTE_MatrixMultiply(blk, M);
			tz = S32(M, 0x1C);
			ty = S32(M, 0x18);
			tx = S32(M, 0x14);
		}
		S32(M, 0x14) = add32(tx, S32(blk, 0x14));
		S32(M, 0x18) = add32(ty, S32(blk, 0x18));
		const uint32_t sxy = U32(rec, 0x18);
		S32(M, 0x1C) = add32(tz, S32(blk, 0x1C));
		if (sxy != 0x10001000 || S16(rec, 0x1C) != 0x1000)
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
		return x::Field_Free(0x58);
	}

	// 0x72AAF0 (097 sub_72AAF0 and copies 0x731570/0x731920/0x731A20, 098/099/100 copies):
	// prim-model task state handler - plays one frame (b_72A720); when the play ended (0) sets
	// the finished flag +0x26 bit0 and advances +0x29
	uint32_t __cdecl b_72AAF0(uint32_t a1)
	{
		const uint32_t r = b_72A720(a1);
		if (r != 0)
			return r;
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x72AD60 (097 sub_72AD60, 098 0x722B60, 099 0x7190B0, 100 0x70FD30): sets the arena
	// [BG_257B5CC] +0x1B4 vector to node +0x290/+0x292/+0x294 (<< 16, fixed point) and copies that
	// 16-byte vector to the history slots +0x204, +0x1F4, +0x1E4, +0x1D4, +0x1C4 (never runs in
	// the harness)
	uint32_t __cdecl b_72AD60(uint32_t a1)
	{
		const uint32_t arena = MEM<uint32_t>(BG(BG_257B5CC));
		const uint32_t v = arena + 0x1B4;
		U32(v, 0) = (uint32_t)shl32((int32_t)S16(a1, 0x290), 16);
		const int32_t y = S16(a1, 0x292);
		const int32_t z = S16(a1, 0x294);
		U32(arena, 0x1B8) = (uint32_t)shl32(y, 16);
		U32(arena, 0x1BC) = (uint32_t)shl32(z, 16);
		b1_copy16(arena + 0x204, v);
		b1_copy16(arena + 0x1F4, v);
		b1_copy16(arena + 0x1E4, v);
		b1_copy16(arena + 0x1D4, v);
		b1_copy16(arena + 0x1C4, v);
		return U32(v, 0xC);
	}

	// 0x72AFE0 (097 sub_72AFE0, 098 0x722DE0, 099 0x719330, 100 0x70FFB0): for i < count
	// (arena [BG_257B5CC] +0x1C, re-read each pass) copies the 16-byte vector arena +0x194 into
	// arena +0x54/+0x94/+0xD4/+0x114/+0x154 + 0x10*i (5 trail histories; never runs in the harness)
	uint32_t __cdecl b_72AFE0(void)
	{
		const uint32_t arena = MEM<uint32_t>(BG(BG_257B5CC));
		int32_t i = 0;
		if (S16(arena, 0x1C) <= 0)
			return arena; // void (eax = arena)
		const uint32_t src = arena + 0x194;
		uint32_t dst = arena + 0x94;
		do
		{
			i++;
			// quirk: for i >= 4 the destinations reach the source (+0x194): the copies are done
			// dword by dword in listing order, so the overlap behaves like the original
			b1_copy16(dst - 0x40, src);
			b1_copy16(dst, src);
			b1_copy16(dst + 0x40, src);
			b1_copy16(dst + 0x80, src);
			b1_copy16(dst + 0xC0, src);
			dst += 0x10;
		} while (i < (int32_t)S16(arena, 0x1C));
		return dst; // void (eax = last destination pointer)
	}

	// 0x72BA00 (097 sub_72BA00, 098 0x723800, 099 0x719D50, 100 0x7109D0): flat-shaded triangle
	// mesh emitter - for each 0xC-byte face at ctx +0x20 (count first) projects its 3 vertices
	// (ctx +4 base, word indices *4), builds a POLY_F3 packet (0x14 bytes) at a4, culls (GTE flag,
	// backface unless ctx+0x1C bit 0x10, screen range), optionally light-colours it (ctx+0x1C bit
	// 0x40) and inserts it into OT a2 at OTZ >> a3; returns the new packet cursor (never runs in
	// the harness)
	uint32_t __cdecl b_72BA00(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		const uint32_t cnt_ptr = U32(ctx, 0x20);
		const uint32_t count = U32(cnt_ptr, 0);
		uint32_t face = cnt_ptr + 4;
		U32(ctx, 0x20) = face;
		const uint32_t vbase = U32(ctx, 4);
		uint32_t cur = a4;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = face;
			return cur;
		}
		uint32_t pk = cur + 0xC;   // edi: packet +0xC (xy1)
		uint32_t left = count;
		do
		{
			const uint32_t pcur = cur;
			const uint32_t v2 = vbase + (uint32_t)U16(face, 8) * 4;
			const uint32_t v1 = vbase + (uint32_t)U16(face, 6) * 4;
			const uint32_t v0 = vbase + (uint32_t)U16(face, 4) * 4;
			x::GTE_LoadV012(v0, v1, v2);
			x::GTE_RTPT();
			const uint32_t fl = U32(ctx, 0x1C);
			const uint32_t code = U32(face, 0);
			U32(pcur, 0) = 0x4000000;          // tag: 4 words
			U32(pk, -8) = code;                // colour + code
			if ((fl & 1) != 0)
				U32(pk, -8) = code | 0x2000000;   // semi-transparent
			if ((fl & 4) != 0)
				U32(pk, -8) &= 0xFDFFFFFFu;
			x::GTE_ReadFLAG(ctx + 0x30);
			if ((U32(ctx, 0x30) & 0x60000) == 0)
			{
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10) != 0)
				{
					x::GTE_ReadSXY012_Split(pk - 4, pk, pk + 4);
					x::GTE_AVSZ3();
					uint32_t clip = 0;
					if (b1_out(S16(pk, -4), 0xA00)) clip = 1;
					if (b1_out(S16(pk, 0), 0xA00)) clip |= 2;
					if (b1_out(S16(pk, 4), 0xA00)) clip |= 4;
					if (b1_out(S16(pk, -2), 0x6C0)) clip |= 0x10;
					if (b1_out(S16(pk, 2), 0x6C0)) clip |= 0x20;
					if (b1_out(S16(pk, 6), 0x6C0)) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if ((U8(ctx, 0x1C) & 0x40) != 0)
						{
							x::set_unk_1CA8A28(pk - 8);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pk - 8);
						}
						const int32_t otz = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)shl32(otz, 2), pcur);
						cur = pcur + 0x14;
						pk += 0x14;
					}
				}
			}
			face += 0xC;
			left--;
		} while (left != 0);
		U32(ctx, 0x20) = face;
		return cur;
	}

	// 0x72BC10 (097 sub_72BC10, 098 0x723A10, 099 0x719F60, 100 0x710BE0): flat-shaded quad mesh
	// emitter - like b_72BA00 with 4 vertices per 0xC-byte face (indices +4/+6/+8, 4th +0xA),
	// POLY_F4 packets (0x18 bytes, tag 0x5000000), AVSZ4 depth; returns the new packet cursor
	// (never runs in the harness)
	uint32_t __cdecl b_72BC10(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		const uint32_t cnt_ptr = U32(ctx, 0x20);
		const uint32_t count = U32(cnt_ptr, 0);
		uint32_t face = cnt_ptr + 4;
		U32(ctx, 0x20) = face;
		const uint32_t vbase = U32(ctx, 4);
		uint32_t cur = a4;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = face;
			return cur;
		}
		uint32_t pk = cur + 0xC;   // esi: packet +0xC (xy1)
		uint32_t left = count;
		do
		{
			const uint32_t pcur = cur;
			const uint32_t v2 = vbase + (uint32_t)U16(face, 8) * 4;
			const uint32_t v1 = vbase + (uint32_t)U16(face, 6) * 4;
			const uint32_t v0 = vbase + (uint32_t)U16(face, 4) * 4;
			x::GTE_LoadV012(v0, v1, v2);
			x::GTE_RTPT();
			const uint32_t fl = U32(ctx, 0x1C);
			const uint32_t code = U32(face, 0);
			U32(pcur, 0) = 0x5000000;          // tag: 5 words
			U32(pk, -8) = code;                // colour + code
			if ((fl & 1) != 0)
				U32(pk, -8) = code | 0x2000000;   // semi-transparent
			if ((fl & 4) != 0)
				U32(pk, -8) &= 0xFDFFFFFFu;
			x::GTE_ReadFLAG(ctx + 0x30);
			if ((U32(ctx, 0x30) & 0x60000) == 0)
			{
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10) != 0)
				{
					x::GTE_ReadSXY012_Split(pk - 4, pk, pk + 4);
					const uint32_t v3 = vbase + (uint32_t)U16(face, 0xA) * 4;
					x::GTE_LoadV0(v3);
					x::GTE_RTPS();
					uint32_t clip = 0;
					if (b1_out(S16(pk, -4), 0xA00)) clip = 1;
					if (b1_out(S16(pk, 0), 0xA00)) clip |= 2;
					if (b1_out(S16(pk, 4), 0xA00)) clip |= 4;
					if (b1_out(S16(pk, -2), 0x6C0)) clip |= 0x10;
					if (b1_out(S16(pk, 2), 0x6C0)) clip |= 0x20;
					if (b1_out(S16(pk, 6), 0x6C0)) clip |= 0x40;
					x::GTE_ReadSXY2(pk + 8);
					x::GTE_AVSZ4();
					if (b1_out(S16(pk, 8), 0xA00)) clip |= 8;
					if (b1_out(S16(pk, 0xA), 0x6C0)) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if ((U8(ctx, 0x1C) & 0x40) != 0)
						{
							x::set_unk_1CA8A28(pk - 8);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pk - 8);
						}
						const int32_t otz = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)shl32(otz, 2), pcur);
						cur = pcur + 0x18;
						pk += 0x18;
					}
				}
			}
			face += 0xC;
			left--;
		} while (left != 0);
		U32(ctx, 0x20) = face;
		return cur;
	}

	// ====================================================================================
	// part b2
	// ====================================================================================
	// Boko modules (097-100), part b2: code shared by the four Boko summons - the four prim-list
	// emitters of the Boko prim-model renderer (Gouraud triangles / quads, Gouraud-textured triangles
	// / quads through the software GTE), the per-frame vertex morph of a Boko model and the actor
	// position interpolation between two reference points (keyframe tables of the actor data).
	namespace
	{
		// x / y screen range tests of the primitive emitters (signed 16-bit compares)
		inline bool b2_out_x(int16_t v) { return v < 0 || v > 0xA00; }
		inline bool b2_out_y(int16_t v) { return v < 0 || v > 0x6C0; }
		// cdq; and edx, 0xFFFF; add eax, edx; sar eax, 16  (= signed 16.16 -> integer, toward zero)
		inline int32_t b2_fx(uint32_t v)
		{
			int32_t s = (int32_t)v;
			return add32(s, (s >> 31) & 0xFFFF) >> 16;
		}
	}

	// 0x72C370 (Boko shared; 097 sub_72C370, 098 0x724170, 099 0x71A6C0, 100 0x711340): prim-list
	// block "Gouraud triangles" of the Boko prim renderer (ctx a1: +4 vertices, +0xC depth-cue
	// colour, +0x1C flags, +0x20 list cursor): count at *(ctx+0x20), then per 0x14-byte record
	// (code+rgb0, u16 vertex indices +4/+6/+8, rgb1 +0xC, rgb2 +0x10) RTPT, builds a 0x1C-byte
	// POLY_G3 at the cursor (tag 0x06000000; flags 2 = semi-trans on, 8 = off, 0x20 = no cull,
	// 0x80 = GTE-lit colours), rejects GTE-flagged / back-facing / fully off-screen ones,
	// InsertPrim at OT a2[OTZ >> a3]. Returns the new packet cursor; ctx+0x20 = end of the list.
	uint32_t __cdecl b_72C370(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;                 // [esp+0x10]
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // edi
		const uint32_t vbase = U32(ctx, 4);   // (the original stores it into its a2 slot)
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return a4;
		}
		uint32_t pkt = cursor;                // esi = pkt + 0x10 (advances with the cursor)
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x6000000;       // tag: 6 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;            // [esp+0x18]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_AVSZ3();
					if (b2_out_x(S16(pkt, 8))) clip = 1;
					if (b2_out_x(S16(pkt, 0x10))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x18))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x12))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x1A))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x0C, rec + 0x10, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x0C, pkt + 0x14, pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x0C), c2 = U32(rec, 0x10);
							U32(pkt, 0x0C) = c1;
							U32(pkt, 0x14) = c2;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x1C;
						pkt += 0x1C;
					}
				}
			}
			rec += 0x14;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return cursor;
	}

	// 0x72C5A0 (Boko shared; 097 sub_72C5A0, 098 0x7243A0, 099 0x71A8F0, 100 0x711570): prim-list
	// block "Gouraud quads": same as 0x72C370 for 0x18-byte records (4th vertex index +0x0A,
	// rgb1..3 at +0x0C/+0x10/+0x14) -> 0x24-byte POLY_G4 packets (tag 0x08000000), 4th vertex
	// projected with RTPS, AVSZ4, off-screen test on the 4 corners. Returns the new packet cursor.
	uint32_t __cdecl b_72C5A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;                 // [esp+0x14]
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return a4;
		}
		uint32_t pkt = cursor;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(cursor, 0) = 0x8000000;       // tag: 8 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;            // [esp+0x10]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					if (b2_out_x(S16(pkt, 8))) clip = 1;
					if (b2_out_x(S16(pkt, 0x10))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x18))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x12))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x1A))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					if (b2_out_x(S16(pkt, 0x20))) clip |= 8;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x0C, rec + 0x10, rec + 0x14);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x0C, pkt + 0x14, pkt + 0x1C);
							x::set_unk_1CA8A28(pkt + 4);
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x0C), c2 = U32(rec, 0x10), c3 = U32(rec, 0x14);
							U32(pkt, 0x0C) = c1;
							U32(pkt, 0x14) = c2;
							U32(pkt, 0x1C) = c3;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x24;
						pkt += 0x24;
					}
				}
			}
			rec += 0x18;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return cursor;
	}

	// 0x72C860 (Boko shared; 097 sub_72C860, 098 0x724660, 099 0x71ABB0, 100 0x711830): prim-list
	// block "Gouraud-textured triangles": per 0x1C-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8, uv2 in the high half of +8, uv0|clut +0xC, uv1|tpage +0x10, rgb1 +0x14, rgb2 +0x18)
	// RTPT, builds a 0x28-byte POLY_GT3 (tag 0x09000000) with the texture offset ctx+0x18 added to
	// the uv words and the optional tpage (ctx+0x10, flags 0x400 add / 0x100 set) / clut (ctx+0x14,
	// flags 0x800 add / 0x200 set) overrides, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl b_72C860(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;              // ebp
		uint32_t pkt = a4;                    // esi
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // edi
		const uint32_t vbase = U32(ctx, 4);   // (stored into the a4 slot)
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0x9000000;          // tag: 9 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			uint32_t toff = U32(ctx, 0x18);
			uint32_t uv0 = U32(rec, 0x0C);
			uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0x0C) = uv0 + toff;
			uint32_t uv2 = U32(rec, 8) >> 16;
			U32(pkt, 0x18) = uv1 + toff;
			U32(pkt, 0x24) = uv2 + toff;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t f2 = U32(ctx, 0x1C);
				if (f2 & 0x400)
					U16(pkt, 0x1A) = (uint16_t)(U16(pkt, 0x1A) + U16(ctx, 0x10));
				else if (f2 & 0x100)
					U16(pkt, 0x1A) = U16(ctx, 0x10);
				if (f2 & 0x800)
					U16(pkt, 0x0E) = (uint16_t)(U16(pkt, 0x0E) + U16(ctx, 0x14));
				else if (f2 & 0x200)
					U16(pkt, 0x0E) = U16(ctx, 0x14);
				uint32_t zero = 0;            // [esp+0x10]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_AVSZ3();
					uint32_t clip = b2_out_x(S16(pkt, 8)) ? 1 : zero;
					if (b2_out_x(S16(pkt, 0x14))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x20))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0x14, rec + 0x18, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0x0C));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 4);
						}
						else
						{
							uint32_t c1 = U32(rec, 0x14), c2 = U32(rec, 0x18);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, pkt);
						pkt += 0x28;
					}
				}
			}
			rec += 0x1C;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x72CAC0 (Boko shared; 097 sub_72CAC0, 098 0x7248C0, 099 0x71AE10, 100 0x711A90): prim-list
	// block "Gouraud-textured quads": per 0x24-byte record (code+rgb0, u16 vertex indices
	// +4/+6/+8/+0xA, uv0|clut +0xC, uv1|tpage +0x10, uv2|uv3<<16 +0x14, rgb1..3 +0x18/+0x1C/+0x20)
	// RTPT + RTPS, builds a 0x34-byte POLY_GT4 (tag 0x0C000000) with the texture offset ctx+0x18 and
	// the tpage / clut overrides of 0x72C860, rejects GTE-flagged / back-facing / off-screen ones,
	// optional GTE-lit colours (flag 0x80), InsertPrim at OT a2[OTZ >> a3]. Returns the new cursor.
	uint32_t __cdecl b_72CAC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;              // ebx
		uint32_t pkt = a4;                    // esi
		uint32_t list = U32(ctx, 0x20);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // edi
		const uint32_t vbase = U32(ctx, 4);   // (stored into the a4 slot)
		U32(ctx, 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x1C);
			uint32_t code = U32(rec, 0);
			U32(pkt, 0) = 0xC000000;          // tag: 12 words
			U32(pkt, 4) = code;
			if (flags & 2)
			{
				code |= 0x2000000;
				U32(pkt, 4) = code;
			}
			if (flags & 8)
				U32(pkt, 4) &= 0xFDFFFFFF;
			uint32_t toff = U32(ctx, 0x18);
			uint32_t uv0 = U32(rec, 0x0C);
			uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0x0C) = uv0 + toff;
			uint32_t w23 = (toff << 16) + toff;
			uv1 += toff;
			w23 += U32(rec, 0x14);
			U32(pkt, 0x24) = w23;
			U32(pkt, 0x18) = uv1;
			U32(pkt, 0x30) = w23 >> 16;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t f2 = U32(ctx, 0x1C);
				if (f2 & 0x400)
					U16(pkt, 0x1A) = (uint16_t)(U16(pkt, 0x1A) + U16(ctx, 0x10));
				else if (f2 & 0x100)
					U16(pkt, 0x1A) = U16(ctx, 0x10);
				if (f2 & 0x800)
					U16(pkt, 0x0E) = (uint16_t)(U16(pkt, 0x0E) + U16(ctx, 0x14));
				else if (f2 & 0x200)
					U16(pkt, 0x0E) = U16(ctx, 0x14);
				uint32_t zero = 0;            // [esp+0x14]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					uint32_t clip = b2_out_x(S16(pkt, 8)) ? 1 : zero;
					if (b2_out_x(S16(pkt, 0x14))) clip |= 2;
					if (b2_out_x(S16(pkt, 0x20))) clip |= 4;
					if (b2_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (b2_out_y(S16(pkt, 0x16))) clip |= 0x20;
					if (b2_out_y(S16(pkt, 0x22))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x2C);
					x::GTE_AVSZ4();
					if (b2_out_x(S16(pkt, 0x2C))) clip |= 8;
					if (b2_out_y(S16(pkt, 0x2E))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
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
							uint32_t c1 = U32(rec, 0x18), c2 = U32(rec, 0x1C), c3 = U32(rec, 0x20);
							U32(pkt, 0x10) = c1;
							U32(pkt, 0x1C) = c2;
							U32(pkt, 0x28) = c3;
						}
						int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, pkt);
						pkt += 0x34;
					}
				}
			}
			rec += 0x24;
		} while (--count);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x72E220 (Boko shared; 097 sub_72E220, 098 0x725D50, 099 0x71C2A0, 100 0x712F20): vertex morph
	// of a Boko model for frame S16(a1+0x1CA) of actor data a2: destination vertex block = state
	// +0x22C table [a2+0x12C[frame]], sources = state+0x230 table [a2+0x130[frame]] and
	// [a2+0x134[frame]], weight w = S16 a2+0x138[frame] (4096 = second source); writes
	// dst[k] = src1[k] * (4096 - w) + src2[k] * w through the GTE (0x45E9D0 / 0x45EBF0), using an
	// 8-byte weight block on the field stack. Does nothing when a source is missing.
	uint32_t __cdecl b_72E220(uint32_t a1, uint32_t a2)
	{
		const uint32_t data = a2;                              // edi
		int32_t frame = S16(a1, 0x1CA);
		uint32_t i_dst = U8(U32(data, 0x12C), frame);
		uint32_t st = U32(BG(BG_257B5CC), 0);                  // actor state block
		uint32_t dst_tab = U32(st, 0x22C);
		uint32_t src_tab = U32(st, 0x230);
		uint32_t dst = U32(dst_tab, i_dst * 4);                // ebx
		uint32_t i_src1 = U8(U32(data, 0x130), frame);
		uint32_t src1 = U32(src_tab, i_src1 * 4);              // ebp
		uint32_t i_src2 = U8(U32(data, 0x134), frame);
		uint32_t src2 = U32(src_tab, i_src2 * 4);              // [esp+0x18] (a2 slot)
		if (src1 == 0 || src2 == 0)
			return 0; // void
		uint32_t w_blk = x::Field_Alloc(8);                    // esi: {+0 4096 - w, +4 w}
		int32_t frame2 = S16(a1, 0x1CA);                       // (re-read through the a1 slot)
		uint32_t n = U32(dst, 4);
		int32_t w = S16(U32(data, 0x138), frame2 * 2);
		U32(w_blk, 4) = (uint32_t)w;
		U32(w_blk, 0) = (uint32_t)sub32(0x1000, w);
		if ((int32_t)n > 0)
		{
			uint32_t out = dst + 8;                            // ebx
			uint32_t in2 = src2 + 8;                           // edi
			uint32_t delta = src1 - src2;                      // ebp
			do
			{
				x::set_dword_1CA8A30(U32(w_blk, 0));
				xm::sub_45E0B0(in2 + delta);                   // = src1 + 8 + 8k
				xm::sub_45E9D0();
				x::set_dword_1CA8A30(U32(w_blk, 4));
				xm::sub_45E0B0(in2);
				xm::sub_45EBF0();
				x::GTE_StoreIR123(out);
				out += 8;
				in2 += 8;
			} while (--n != 0);
		}
		x::Field_Free(8);
		return 0; // void
	}

	// 0x72EF80 (Boko shared; 097 sub_72EF80, 098 0x726AB0, 099 0x71D000, 100 0x713C80): actor position
	// for its current key a1+0x60: actor data = state+0x224[S8 a1+0x6A]; start point A = reference
	// point (kind data+0x100[key]: 1 = state points +0x1D4/0x1E4/0x1F4/0x204 (16.16), 0 = the actor's
	// target slot S8 a1+0x6B points +0x94/+0xD4/+0x114/+0x154; index data+0x108[key]) plus the
	// offset (data+0x110/0x114/0x118) rotated by the actor matrix a1+0x2C; end point B likewise
	// (kind +0x104, index +0x10C, offset +0x11C/0x120/0x124; unknown kind = the rotated first
	// offset); a1+0x4C/0x50/0x54 = (A * 4096 + (B - A) * t) << 4 with t = S16 data+0x128[key].
	uint32_t __cdecl b_72EF80(uint32_t a1)
	{
		const uint32_t act = a1;                               // esi
		const uint32_t st = U32(BG(BG_257B5CC), 0);            // ecx
		int32_t aidx = S8(act, 0x6A);
		int32_t key = S16(act, 0x60);                          // ebx
		const uint32_t data = U32(U32(st, 0x224), aidx * 4);   // edi / [esp+0x10]
		uint8_t sel = U8(U32(data, 0x108), key);               // bp (movzx)
		uint8_t kind = U8(U32(data, 0x100), key);
		// UNINIT 0x72F1CE: the start point is a stack SVECTOR ([esp+0x14] x, [esp+0x16] y,
		// [esp+0x18] z) that only the switch cases fill; an unknown kind / index > 3 leaves it
		// uninitialised (0 here)
		int16_t ax = 0, ay = 0, az = 0;
		uint32_t base = 0;
		bool have = false;
		if (kind == 0)
		{
			if (sel <= 3)
			{
				static const uint32_t offs0[4] = { 0x94, 0xD4, 0x114, 0x154 };     // table 0x72F5DC
				base = st + (uint32_t)((int32_t)S8(act, 0x6B) << 4) + offs0[sel];
				have = true;
			}
		}
		else if (kind == 1)
		{
			if (sel <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5CC
				base = st + offs1[sel];
				have = true;
			}
		}
		if (have)
		{
			int32_t vx = b2_fx(U32(base, 0));
			int32_t vy = b2_fx(U32(base, 4));
			ay = (int16_t)vy;
			int32_t vz = b2_fx(U32(base, 8));
			az = (int16_t)vz;
			ax = (int16_t)vx;
		}
		// first offset (SVECTOR [esp+0x1C]) rotated by the actor matrix -> IR1..3 ([esp+0x24], shorts)
		int16_t v[4];
		int16_t ir[8] = {};
		v[0] = (int16_t)U16(U32(data, 0x110), key * 2);
		v[1] = (int16_t)U16(U32(data, 0x114), key * 2);
		v[2] = (int16_t)U16(U32(data, 0x118), key * 2);
		v[3] = 0;                                              // (pad, not read by LoadV0)
		x::GTE_SetRotMatrix(act + 0x2C);
		x::GTE_LoadV0(P(v));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(ir));
		az = (int16_t)(az + ir[2]);
		ay = (int16_t)(ay + ir[1]);
		const int32_t key2 = S16(act, 0x60);                   // ecx
		uint8_t kind2 = U8(U32(data, 0x104), key2);
		uint8_t sel2 = U8(U32(data, 0x10C), key2);
		ax = (int16_t)(ax + ir[0]);
		int16_t bx, by, bz;                                    // si, di, bx
		uint32_t base2 = 0;
		bool have2 = false;
		if (kind2 == 0)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs0[4] = { 0x94, 0xD4, 0x114, 0x154 };     // table 0x72F5FC
				uint32_t g = U32(BG(BG_257B5CC), 0);
				base2 = (uint32_t)((int32_t)S8(act, 0x6B) << 4) + g + offs0[sel2];
				have2 = true;
			}
		}
		else if (kind2 == 1)
		{
			if (sel2 <= 3)
			{
				static const uint32_t offs1[4] = { 0x1D4, 0x1E4, 0x1F4, 0x204 };  // table 0x72F5EC
				uint32_t g = U32(BG(BG_257B5CC), 0);
				base2 = g + offs1[sel2];
				have2 = true;
			}
		}
		if (have2)
		{
			int32_t vx = b2_fx(U32(base2, 0));
			int32_t vy = b2_fx(U32(base2, 4));
			int32_t vz = b2_fx(U32(base2, 8));
			bx = (int16_t)vx;
			by = (int16_t)vy;
			bz = (int16_t)vz;
		}
		else
		{
			// quirk 0x72F4EA: an unknown end kind / index > 3 reuses the rotated first offset (the IR
			// of the first MVMVA, still in [esp+0x24..0x29])
			bz = ir[2];
			by = ir[1];
			bx = ir[0];
		}
		// second offset rotated by the actor matrix
		v[0] = (int16_t)U16(U32(data, 0x11C), key2 * 2);
		v[1] = (int16_t)U16(U32(data, 0x120), key2 * 2);
		v[2] = (int16_t)U16(U32(data, 0x124), key2 * 2);
		x::GTE_SetRotMatrix(act + 0x2C);
		x::GTE_LoadV0(P(v));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(ir));
		bx = (int16_t)(bx + ir[0]);
		by = (int16_t)(by + ir[1]);
		const int32_t key3 = S16(act, 0x60);
		const uint32_t ttab = U32(data, 0x128);
		bz = (int16_t)(bz + ir[2]);
		const int32_t t = S16(ttab, key3 * 2);
		int32_t a = ax;
		S32(act, 0x4C) = shl32(add32(mul32(sub32(bx, a), t), shl32(a, 12)), 4);
		a = ay;
		S32(act, 0x50) = shl32(add32(mul32(sub32(by, a), t), shl32(a, 12)), 4);
		a = az;
		S32(act, 0x54) = shl32(add32(mul32(sub32(bz, a), t), shl32(a, 12)), 4);
		return 0; // void
	}

	// ====================================================================================
	// part b3
	// ====================================================================================
	// Part b3: code shared by several Boko modules (097 ChocoFire / 098 ChocoFlare / 099 ChocoMeteor /
	// 100 ChocoBocle): director states around the Boko creature, the creature actor task (0x7306A0),
	// its shadow task (0x730C10), two prim-model particle tasks (0x731840 / 0x731950) and the fade
	// states that write the four battle-entity fade words 0x1D98992 + 0x2C*k.
	// Module addresses go through BG()/BF() (address in the current Boko module).
	namespace
	{
		// common task tail: frame counter +1, ends (release linked task, return 2) when the
		// finished flag (+0x26 bit0) is set and no child is alive (+0x28 == 0)
		inline uint32_t b3_task_tail(uint32_t node)
		{
			const uint8_t status = U8(node, 0x26);
			U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
			if ((status & 1) != 0 && U8(node, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(node);
				return 2;
			}
			return 0;
		}

		inline void b3_next_state(uint32_t node)
		{
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}

		// the four battle-entity fade words 0x1D98992 + 0x2C*k (k = 0..3) = value
		inline void b3_write_fade_words(uint16_t v)
		{
			uint32_t p = 0x1D98992;
			for (int k = 4; k != 0; --k)
			{
				MEM<uint16_t>(p) = v;
				p += 0x2C;
			}
		}

		// cdq / and edx,3 / add / sar 2 / lea [base + eax*4]: section pointer = base + (off / 4) * 4
		inline uint32_t b3_section(uint32_t base, int32_t off)
		{
			return (uint32_t)(base + ((uint32_t)(off / 4) << 2));
		}
	}

	// 0x72FC40 (Boko shared; 097:72FC40 098:727780 099:71DCD0 100:714970): director state - resets
	// the camera script (a_73AB00), starts camera script BG_152BA38 at the director origin, next
	// state.
	uint32_t __cdecl b_72FC40(uint32_t a1)
	{
		a_73AB00();
		const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
		a_73AB20(BG(BG_152BA38), origin, 0);
		b3_next_state(a1);
		return a1;
	}

	// 0x730460 (Boko shared; 097:730460 098:727FA0 099:71E4F0 100:715190): director state - once
	// camera step 2 is reached (a_73AAE0(2)), starts camera script BG_152BA80, next state.
	uint32_t __cdecl b_730460(uint32_t a1)
	{
		const uint32_t r = a_73AAE0(2);
		if (r == 0)
			return 0;
		const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
		a_73AB20(BG(BG_152BA80), origin, 0);
		b3_next_state(a1);
		return a1;
	}

	// 0x730510 (Boko shared; 097:730510 098:728080): director state - steps the camera script
	// (a_73AE10), next state once camera step 6 is reached.
	uint32_t __cdecl b_730510(uint32_t a1)
	{
		a_73AE10();
		const uint32_t r = a_73AAE0(6);
		if (r == 0)
			return 0;
		b3_next_state(a1);
		return a1;
	}

	// 0x7305D0 (Boko shared; 097:7305D0 098:728140 099:71E800 100:715470): director state - when
	// the context word +0x166 is 0, a_73BB60(0) and next state; else a_73BB60(1) from frame 0x1B.
	uint32_t __cdecl b_7305D0(uint32_t a1)
	{
		const uint32_t ctx = MEM<uint32_t>(BG(BG_257AFB8));
		if (U16(ctx, 0x166) == 0)
		{
			a_73BB60(0);
			b3_next_state(a1);
			return a1;
		}
		if (S16(a1, 0x24) >= 0x1B)
			return a_73BB60(1);
		return ctx;
	}

	// 0x730660 (Boko shared, IDA MAG_097_SpawnBokoCreature; 097:730660 098:7281D0 099:71E890
	// 100:715500): director state - from frame 0x1C spawns the creature actor task (BF_7306A0,
	// node 0x140) in queue BG_257B158, binds model container BG_168354C anim 1, next state.
	uint32_t __cdecl b_730660(uint32_t a1)
	{
		if (S16(a1, 0x24) < 0x1C)
			return 0;
		const uint32_t task = x::Effect_AddTaskAndInitFromCtx(BG(BG_257B158), BF(BF_7306A0), 0x140, a1);
		x::Effect_BindModelContainerSetAnim(task, BG(BG_168354C), 1);
		b3_next_state(a1);
		return 0; // void (eax = low byte of the new state)
	}

	// 0x7306A0 (Boko shared; 097:7306A0 098:728210 100:7175F0): Boko creature actor TASK (node
	// 0x140) - runs state [+0x29] of a 10-state table, then (unless hidden, +0x26 bit2) draws the
	// model (a_746C10 with draw arg BG_2579EC8) and its texture animation (a_739890).
	uint32_t __cdecl b_7306A0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[10];
		states[0] = BF(BF_730B00);
		states[1] = BF(BF_730BA0);
		states[2] = BF(BF_730BD0);
		states[3] = BF(BF_730E30);
		states[4] = BF(BF_730EA0);
		states[5] = BF(BF_730EF0);
		states[6] = BF(BF_730F10);
		states[7] = BF(BF_730F50);
		states[8] = BF(BF_730F80);
		states[9] = BF(BF_730FB0);
		callp(states[S8(node, 0x29)], node);
		// draw (after the state update)
		if ((U8(node, 0x26) & 4) == 0)
		{
			const uint32_t cursor = MEM<uint32_t>(0x1D8E054);
			MEM<uint32_t>(0x1D8E054) = a_746C10(node, BG(BG_2579EC8), cursor);
			a_739890(node);
		}
		return b3_task_tail(node);
	}

	// 0x730B00 (Boko shared; 097:730B00 098:728670 099:71ED20 100:7159A0): creature state 0 -
	// +0x4C/+0x50 = director origin, scale block +0x114 = 0x1800^3 (+0x60 -> it), anim 0,
	// a_739AC0(node, BG_152BB58, 0), next state.
	uint32_t __cdecl b_730B00(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
		const uint32_t xy = U32(origin, 0);
		const uint32_t zw = U32(origin, 4);
		U32(node, 0x4C) = xy;
		U32(node, 0x50) = zw;
		U32(node, 0x60) = node + 0x114;
		U32(node, 0x11C) = 0x1800;
		U32(node, 0x118) = 0x1800;
		U32(node, 0x114) = 0x1800;
		x::au_re_Battle_ReadAnimation_7(node, 0);
		a_739AC0(node, BG(BG_152BB58), 0);
		b3_next_state(node);
		return 0; // void
	}

	// 0x730BD0 (Boko shared; 097:730BD0 098:728740 100:715A70): creature state - steps the pose
	// (0x8DD1C0); when +0x136 >= 0 sets anim 2 and spawns the shadow task (BF_730C10, node 0x70)
	// in queue BG_2575458, next state.
	uint32_t __cdecl b_730BD0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (S16(node, 0x136) < 0)
			return 0;
		x::au_re_Battle_ReadAnimation_7(node, 2);
		x::Effect_AddTaskAndInitFromCtx(BG(BG_2575458), BF(BF_730C10), 0x70, node);
		b3_next_state(node);
		return 0; // void
	}

	// 0x730C10 (Boko shared; 097:730C10 098:728780 099:71EE40 100:715AB0): shadow TASK (node 0x70)
	// - runs state [+0x29] of a 6-state table, then draws the shadow sequence (b_730C90, 0x800).
	uint32_t __cdecl b_730C10(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[6];
		states[0] = BF(BF_730D00);
		states[1] = BF(BF_730D50);
		states[2] = BF(BF_730D70);
		states[3] = BF(BF_730DD0);
		states[4] = BF(BF_730E00);
		states[5] = BF(BF_730E20);
		callp(states[S8(node, 0x29)], node);
		// draw (after the state update)
		b_730C90(node, 0x800);
		return b3_task_tail(node);
	}

	// 0x730C90 (Boko shared; 097:730C90 098:728800 099:71EEC0 100:715B30): shadow draw - unless
	// hidden (+0x26 bit2), sets the shadow camera (0x571BC0 on +0x1C, scale a2, angle +0x54) and
	// emits the effect sequence +0x4C (frame +0x50) into the effect OT (0x571C80, mode 2) through a
	// 0xB4-byte scratch block.
	uint32_t __cdecl b_730C90(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		if ((U8(node, 0x26) & 4) != 0)
			return 0;
		xm::TransformCameraByShadowRotation(node + 0x1C, a2, (uint32_t)(int32_t)S16(node, 0x54));
		const uint32_t blk = x::Field_Alloc(0xB4);
		const uint32_t seq = U32(node, 0x4C);
		const uint16_t frame = U16(node, 0x50);
		U32(blk, 0) = seq;
		const uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		U16(blk, 4) = frame;
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 0x24) = 0;
		MEM<uint32_t>(0x1D8E054) = xm::InitEffectSequenceFromData(blk, ot, 2, cursor);
		return x::Field_Free(0xB4);
	}

	// 0x730D00 (Boko shared; 097:730D00 098:728870 100:715BA0): shadow state 0 - hidden, position
	// = director origin + (0, -0x300, -0x280), sequence BG_152B5C8 frame 0x13, angle 0xFF00,
	// delay +0x6C = 4, next state.
	uint32_t __cdecl b_730D00(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
		const uint32_t xy = U32(origin, 0);
		const uint32_t zw = U32(origin, 4);
		U16(node, 0x26) = (uint16_t)(U16(node, 0x26) | 4);
		U32(node, 0x1C) = xy;
		U32(node, 0x20) = zw;
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + 0xFD00);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + 0xFD80);
		U32(node, 0x4C) = BG(BG_152B5C8);
		U16(node, 0x52) = 0x13;
		U16(node, 0x54) = 0xFF00;
		U16(node, 0x6C) = 4;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x730D50 (Boko shared; 097:730D50 098:7288C0 100:715BF0): shadow state - counts +0x6C down,
	// at <= 0 shows the shadow (clears +0x26 bit2), next state.
	uint32_t __cdecl b_730D50(uint32_t a1)
	{
		U16(a1, 0x6C) = (uint16_t)(U16(a1, 0x6C) - 1);
		if (S16(a1, 0x6C) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) & 0xFB);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return a1;
	}

	// 0x730D70 (Boko shared; 097:730D70 098:7288E0 100:715C10): shadow state - steps the sequence
	// frame (a_743C20); once frame +0x50 > 8: delay +0x6C = 2, next state.
	uint32_t __cdecl b_730D70(uint32_t a1)
	{
		const uint32_t node = a1;
		a_743C20(node);
		if (S16(node, 0x50) > 8)
		{
			U16(node, 0x6C) = 2;
			b3_next_state(node);
		}
		return 0; // void
	}

	// 0x730DD0 (Boko shared; 097:730DD0 098:728940 100:715C70): shadow state - steps the sequence
	// frame (a_743C20), clamps frame +0x50 > 9 back to 8, counts +0x6C down, next state at <= 0.
	uint32_t __cdecl b_730DD0(uint32_t a1)
	{
		const uint32_t node = a1;
		a_743C20(node);
		if (S16(node, 0x50) > 9)
			U16(node, 0x50) = 8;
		U16(node, 0x6C) = (uint16_t)(U16(node, 0x6C) - 1);
		if (S16(node, 0x6C) <= 0)
			b3_next_state(node);
		return 0; // void
	}

	// 0x730E30 (Boko shared; 097:730E30 098:7289A0 099:71F080 100:715CD0): creature state - when
	// the animation ended (0x8DD220 == 1) and a_73B7E0(1): anim 3, sound BdPlaySE(BG_152B5C0, 0,
	// 0x80), next state.
	uint32_t __cdecl b_730E30(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t r = x::au_re_Battle_ReadAnimation_8(node);
		if (r != 1)
			return r;
		r = a_73B7E0(r);
		if (r == 0)
			return 0;
		x::au_re_Battle_ReadAnimation_7(node, 3);
		x::BdPlaySE(BG(BG_152B5C0), 0, 0x80);
		b3_next_state(node);
		return 0; // void
	}

	// 0x730EA0 (Boko shared; 097:730EA0 098:728A10 099:71F0F0 100:715D40): creature state - steps
	// the pose (0x8DD1C0), next state once a_73B640(2).
	uint32_t __cdecl b_730EA0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		const uint32_t r = a_73B640(2);
		if (r != 0)
			b3_next_state(node);
		return r;
	}

	// 0x730EF0 (Boko shared; 097:730EF0 098:728A60 099:71F140 100:715D60): creature state - steps
	// the pose (0x8DD1C0), next state once +0x136 >= 0x32.
	uint32_t __cdecl b_730EF0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (S16(node, 0x136) >= 0x32)
			b3_next_state(node);
		return 0; // void
	}

	// 0x730F50 (Boko shared; 097:730F50 098:728AB0): creature state - when the animation ended
	// (0x8DD220 == 1): anim 5, next state.
	uint32_t __cdecl b_730F50(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t r = x::au_re_Battle_ReadAnimation_8(node);
		if (r != 1)
			return r;
		x::au_re_Battle_ReadAnimation_7(node, 5);
		b3_next_state(node);
		return 0; // void
	}

	// 0x730FE0 (Boko shared; 097:730FE0 099:71F1F0 100:715E50): creature state - when the context
	// word +0x166 is 0 and a_73B640(1): next state.
	uint32_t __cdecl b_730FE0(uint32_t a1)
	{
		const uint32_t ctx = MEM<uint32_t>(BG(BG_257AFB8));
		if (U16(ctx, 0x166) != 0)
			return ctx;
		const uint32_t r = a_73B640(1);
		if (r == 0)
			return 0;
		b3_next_state(a1);
		return a1;
	}

	// 0x731090 (Boko shared; 097:731090 098:728BE0 099:71F2A0): state - once a_73B7E0(4): delay
	// +0x44 = 0x10, next state.
	uint32_t __cdecl b_731090(uint32_t a1)
	{
		const uint32_t r = a_73B7E0(4);
		if (r == 0)
			return 0;
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x44) = 0x10;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x731520 (Boko shared; 097:731520 098:7291C0): prim-model particle state 0 - decodes the prim
	// layout of +0x74 (index +0x78) into +0x94, position = director point +8, scale 0x1000^3,
	// plays the first frame (b_72A720), next state.
	uint32_t __cdecl b_731520(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
		const uint32_t xy = U32(origin, 8);
		const uint32_t zw = U32(origin, 0xC);
		U32(node, 0x1C) = xy;
		U32(node, 0x20) = zw;
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		b_72A720(node);
		b3_next_state(node);
		return 0; // void
	}

	// 0x7315D0 (Boko shared; 097:7315D0 100:716070): state - counts +0x44 down; at <= 0x12 sets
	// the director word +0x48 = 1, next state.
	uint32_t __cdecl b_7315D0(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) <= 0x12)
		{
			const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
			U16(origin, 0x48) = 1;
			b3_next_state(a1);
		}
		return a1;
	}

	// 0x731690 (Boko shared; 097:731690 098:729330 099:721160): fade state 0 - level +0x1C =
	// 0xC00, next state.
	uint32_t __cdecl b_731690(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0xC00;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x7316B0 (Boko shared; 097:7316B0 099:721180 100:716BD0): fade-out state - level +0x1C
	// -= 0x80 down to 0 (then finished, next state), written to the 4 entity fade words
	// 0x1D98992 + 0x2C*k.
	uint32_t __cdecl b_7316B0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) - 0x80);
		if (S16(a1, 0x1C) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U16(a1, 0x1C) = 0;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		b3_write_fade_words(level);
		return 0; // void
	}

	// 0x731720 (Boko shared; 097:731720 098:7293C0): state - once a_73B640(6): 0x4A2940(director
	// word +0x52), finished, next state.
	uint32_t __cdecl b_731720(uint32_t a1)
	{
		const uint32_t r = a_73B640(6);
		if (r == 0)
			return 0;
		const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
		x::sub_4A2940((uint32_t)(int32_t)S16(origin, 0x52));
		const uint8_t st = U8(a1, 0x29);
		U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x731840 (Boko shared; 097:731840 098:7294C0 099:71F950 100:7163D0): prim-model particle
	// TASK - runs state [+0x29] of a 3-state table (init / play+draw / nullsub), then steps the
	// texture animation of the model sections at data[+8] and data[+0x14] (a_7340A0, mode 0xC).
	uint32_t __cdecl b_731840(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = BF(BF_7318D0);
		states[1] = BF(BF_731920);
		states[2] = BF(BF_731940);
		callp(states[S8(node, 0x29)], node);
		uint32_t data = U32(node, 0x74);
		a_7340A0(b3_section(data, S32(data, 8)), 0xC);
		data = U32(node, 0x74);
		a_7340A0(b3_section(data, S32(data, 0x14)), 0xC);
		return b3_task_tail(node);
	}

	// 0x7318D0 (Boko shared; 097:7318D0 098:729550 099:71FD40 100:7167C0): particle state 0 (task
	// 0x731840) - decodes the prim layout, position = director origin with y - 0x180, scale
	// 0x1000^3, plays the first frame (b_72A720), next state.
	uint32_t __cdecl b_7318D0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
		const uint32_t xy = U32(origin, 0);
		const uint32_t zw = U32(origin, 4);
		U32(node, 0x1C) = xy;
		U32(node, 0x20) = zw;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + 0xFE80);
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		b_72A720(node);
		b3_next_state(node);
		return 0; // void
	}

	// 0x731950 (Boko shared; 097:731950 098:7295D0 099:71FDC0 100:716840): prim-model particle
	// TASK - runs state [+0x29] of a 3-state table (init / play+draw / nullsub), then steps the
	// texture animation of the model section at data[+0x10] (a_7340A0, mode 0x12).
	uint32_t __cdecl b_731950(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = BF(BF_7319D0);
		states[1] = BF(BF_731A20);
		states[2] = BF(BF_731A40);
		callp(states[S8(node, 0x29)], node);
		const uint32_t data = U32(node, 0x74);
		a_7340A0(b3_section(data, S32(data, 0x10)), 0x12);
		return b3_task_tail(node);
	}

	// 0x7319D0 (Boko shared; 097:7319D0 098:729650 099:71FE40 100:7168C0): particle state 0 (task
	// 0x731950) - decodes the prim layout, position = director origin + (0, -0x400, -0x180), scale
	// 0x1000^3, next state (no first play).
	uint32_t __cdecl b_7319D0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		const uint32_t origin = MEM<uint32_t>(BG(BG_152B5B8));
		const uint32_t xy = U32(origin, 0);
		const uint32_t zw = U32(origin, 4);
		U32(node, 0x1C) = xy;
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		const uint8_t st = U8(node, 0x29);
		U32(node, 0x20) = zw;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + 0xFC00);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + 0xFE80);
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x731AF0 (Boko shared; 097:731AF0 098:729770 099:7210B0 100:716B00): fade-in state - level
	// +0x1C += 0x80 up to 0x600 (then finished, next state), written to the 4 entity fade words
	// 0x1D98992 + 0x2C*k.
	uint32_t __cdecl b_731AF0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x80);
		if (S16(a1, 0x1C) >= 0x600)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U16(a1, 0x1C) = 0x600;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		b3_write_fade_words(level);
		return 0; // void
	}

	// 0x731BE0 (Boko shared; 097:731BE0 098:729860): fade-in state - level +0x1C += 0x100 up to
	// 0xC00 (then finished, next state), written to the 4 entity fade words 0x1D98992 + 0x2C*k.
	uint32_t __cdecl b_731BE0(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x100);
		if (S16(a1, 0x1C) >= 0xC00)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U16(a1, 0x1C) = 0xC00;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		b3_write_fade_words(level);
		return 0; // void
	}

	// ====================================================================================
	// part b71
	// ====================================================================================
	// Boko ChocoFire (097) module-only code, part b71 (raw module addresses).
	// Director data block: [0x152B5B8] (+0x40 current phase, +0x42 requested phase, +0x44 next
	// requested phase, +0x46 ticks in phase). Emitter director: [0x257B5CC] (= stage node + 0x34:
	// +0x14 live particle count, +0x16 live emitter count, +0x18 tick, +0x1A min ticks, +0x20 stage,
	// +0x224 descriptor table, +0x22C texture entry table, +0x2C emitter instance list).
	// Asset table: [0x257AFB8] (= 0x257B5D0, 0x16C bytes). Camera matrix pointer: [0x2575378].
	namespace
	{
		// common task tail (status read before ++frame counter): end when finished and no child alive
		inline uint32_t b71_task_end(uint32_t node, uint8_t status)
		{
			if ((status & 1) != 0 && U8(node, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(node);
				return 2;
			}
			return 0;
		}
		inline void b71_next_state(uint32_t node)
		{
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		}
		// &data[data[off] / 4] (cdq / and 3 / add / sar 2 = signed division by 4, then lea *4)
		inline uint32_t b71_section(uint32_t data, int32_t off)
		{
			const int32_t v = S32(data, off);
			return data + (uint32_t)shl32(v / 4, 2);
		}
		// 0x72D882.. / 0x72DA9E..: ordered rotations of emitter matrix m = node+0x8C for rotation
		// modes 3 and 4 (+0x1B; both jump tables hold the same code): sub 0/1 start from the parent
		// matrix (parent+0x2C, rep movsd 8 dwords) or identity, sub 2/3 from the camera rotation;
		// sub 0/2 apply +0xD0 (8DD960), +0xCC (8DD7E0), +0xCE (8DD8A0), sub 1/3 +0xCE, +0xCC, +0xD0
		void b71_build_rot(uint32_t node, int32_t sub, uint32_t parent)
		{
			const uint32_t m = node + 0x8C;
			if ((uint32_t)sub > 3)
				return;
			if (sub == 0 || sub == 1)
			{
				if (parent != 0)
					memcpy((void *)m, (const void *)(parent + 0x2C), 8 * 4);
				else
					x::MAG_022_sub_8DD770(m);
			}
			else
			{
				x::UnpackRotationMatrix(MEM<uint32_t>(0x2575378), m);
			}
			if (sub == 0 || sub == 2)
			{
				if (U16(node, 0xD0) != 0)
					x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				if (U16(node, 0xCC) != 0)
					x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
				if (U16(node, 0xCE) != 0)
					x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
			}
			else
			{
				if (U16(node, 0xCE) != 0)
					x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
				if (U16(node, 0xCC) != 0)
					x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
				if (U16(node, 0xD0) != 0)
					x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
			}
		}
		// 0x72DD0B / 0x72DE19: world position (16.16 dwords at src) -> +0xA0..A8 (integer part,
		// signed /65536), then view transform with the camera matrix cam: node matrix columns
		// rotated by the camera rotation into +0xAC.., position -> +0xC0..C8 (MAC1..3)
		void b71_world_to_view(uint32_t node, uint32_t src, uint32_t cam)
		{
			U32(node, 0xA0) = (uint32_t)(S32(src, 0) / 65536);
			U32(node, 0xA4) = (uint32_t)(S32(src, 4) / 65536);
			U32(node, 0xA8) = (uint32_t)(S32(src, 8) / 65536);
			x::GTE_SetRotMatrix(cam);
			x::GTE_LoadIRFromMatrixColumn(node + 0x8C);
			x::GTE_MVMVA_RotIR();
			x::GTE_StoreIRToMatrixColumn(node + 0xAC);
			x::GTE_LoadIRFromMatrixColumn(node + 0x8E);
			x::GTE_MVMVA_RotIR();
			x::GTE_StoreIRToMatrixColumn(node + 0xAE);
			x::GTE_LoadIRFromMatrixColumn(node + 0x90);
			x::GTE_MVMVA_RotIR();
			x::GTE_StoreIRToMatrixColumn(node + 0xB0);
			x::GTE_SetTransVector(MEM<uint32_t>(0x2575378));
			x::GTE_LoadV0FromDwords(node + 0xA0);
			x::GTE_MVMVA_RotV0_Tr();
			x::GTE_ReadMAC123(node + 0xC0);
		}
		// 0x72DF9C-shaped record (3 V bytes: +0xC byte0, +0x10 byte0, +0x8 byte2): scroll by d,
		// all three wrapped back by 0x40 when any leaves 0..0x7F (unsigned compare), masked to 7 bits
		inline void b71_scroll3(uint32_t r, uint32_t d)
		{
			const uint32_t w10 = U32(r, 0x10);
			const uint32_t w0C = U32(r, 0x0C);
			const uint32_t w08 = U32(r, 0x08);
			uint32_t v0 = (w0C & 0x7F) + d;
			uint32_t v1 = (w10 & 0x7F) + d;
			uint32_t v2 = ((w08 >> 16) & 0x7F) + d;
			if (v0 > 0x7F || v1 > 0x7F || v2 > 0x7F)
			{
				v0 -= 0x40;
				v1 -= 0x40;
				v2 -= 0x40;
			}
			U32(r, 0x0C) = (w0C & 0xFFFFFF00u) | (v0 & 0x7F);
			U32(r, 0x10) = (w10 & 0xFFFFFF00u) | (v1 & 0x7F);
			U32(r, 0x08) = (w08 & 0xFF00FFFFu) | ((v2 & 0x7F) << 16);
		}
		// 0x72E026-shaped record (4 V bytes: +0xC byte0, +0x10 byte0, +0x14 byte0 and byte2)
		inline void b71_scroll4(uint32_t r, uint32_t d)
		{
			const uint32_t w0C = U32(r, 0x0C);
			const uint32_t w10 = U32(r, 0x10);
			const uint32_t w14 = U32(r, 0x14);
			uint32_t v0 = (w0C & 0x7F) + d;
			uint32_t v1 = (w10 & 0x7F) + d;
			uint32_t v2 = (w14 & 0x7F) + d;
			uint32_t v3 = ((w14 >> 16) & 0x7F) + d;
			if (v0 > 0x7F || v1 > 0x7F || v2 > 0x7F || v3 > 0x7F)
			{
				v0 -= 0x40;
				v1 -= 0x40;
				v2 -= 0x40;
				v3 -= 0x40;
			}
			U32(r, 0x10) = (w10 & 0xFFFFFF00u) | (v1 & 0x7F);
			U32(r, 0x0C) = (w0C & 0xFFFFFF00u) | (v0 & 0x7F);
			U32(r, 0x14) = (w14 & 0xFF00FF00u) | (v2 & 0x7F) | ((v3 & 0x7F) << 16);
		}
	}

	// 0x729EA0 (module 097 MAG_097_sub_729EA0): master state - init director data (b_72A350) and the
	// asset table (b7_729EF0), spawn task 0x729FF0 (0x30 B, queue 0x257B5A8) and the director task
	// 0x72A410 (0x48 B, queue 0x2575350), next state
	uint32_t __cdecl b7_729EA0(uint32_t a1)
	{
		b_72A350();
		b7_729EF0();
		x::Effect_AddTaskAndInitFromCtx(0x257B5A8, 0x729FF0, 0x30, a1);
		x::Effect_AddTaskAndInitFromCtx(0x2575350, 0x72A410, 0x48, a1);
		b71_next_state(a1);
		return 0; // void
	}

	// 0x729EF0 (module 097 MAG_097_sub_729EF0): clears the 0x16C-byte asset table 0x257B5D0
	// ([0x257AFB8]), sets [0x2579EC0] = 0x152B930, reserves 0xA000 bytes of the arena cursor
	// [0x257B5C8] (-> table +0x100) and points the model slots +0x104..+0x160 at the loaded files
	// (0x19DB018 default, 0x19DFEF8, 0x19E4958, 0x19E8660); +0x164/+0x166 = 0
	uint32_t __cdecl b7_729EF0(void)
	{
		MEM<uint32_t>(0x2579EC0) = 0x152B930;
		MEM<uint32_t>(0x257AFB8) = 0x257B5D0;
		x::MAG_007_sub_8DCC00(0x257B5D0, 0x16C);
		const uint32_t t = MEM<uint32_t>(0x257AFB8);
		uint32_t cur = MEM<uint32_t>(0x257B5C8);
		U32(t, 0x100) = cur;
		cur += 0xA000;
		MEM<uint32_t>(0x257B5C8) = cur;
		const uint32_t def = 0x19DB018;
		U32(t, 0x114) = def;
		U32(t, 0x118) = def;
		U32(t, 0x11C) = def;
		U32(t, 0x120) = def;
		U32(t, 0x104) = def;
		U32(t, 0x108) = 0x19DFEF8;
		U32(t, 0x10C) = def;
		U32(t, 0x110) = def;
		U32(t, 0x124) = def;
		U32(t, 0x128) = def;
		U32(t, 0x12C) = 0x19E4958;
		U32(t, 0x130) = 0x19E8660;
		for (int32_t o = 0x134; o <= 0x160; o += 4)
			U32(t, o) = def;
		U16(t, 0x164) = 0;
		U16(t, 0x166) = 0;
		return 0; // void
	}

	// 0x72A410 (module 097 MAG_097_sub_72A410): ChocoFire DIRECTOR task - phase change handling
	// (b7_72A4F0), then runs the 18-state director state table; ends when finished and childless
	uint32_t __cdecl b7_72A410(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[18];
		states[0] = 0x72A540;
		states[1] = 0x730570;
		states[2] = 0x7305D0;
		states[3] = 0x730660;
		states[4] = 0x730FC0;
		states[5] = 0x730FE0;
		states[6] = 0x731010;
		states[7] = 0x731030;
		states[8] = 0x731050;
		states[9] = 0x731070;
		states[10] = 0x731090;
		states[11] = 0x7310C0;
		states[12] = 0x7315A0;
		states[13] = 0x7315D0;
		states[14] = 0x731600;
		states[15] = 0x731700;
		states[16] = 0x731720;
		states[17] = 0x731760;
		b7_72A4F0(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return b71_task_end(node, status);
	}

	// 0x72A4F0 (module 097 sub_72A4F0): director phase bookkeeping on [0x152B5B8]: ++ticks in phase
	// (+0x46); requested phase (+0x42) != current (+0x40) -> switch, reset ticks, spawn the phase's
	// tasks (b7_731780); next requested (+0x44) != requested -> take it (then nullsub)
	uint32_t __cdecl b7_72A4F0(uint32_t a1)
	{
		uint32_t d = MEM<uint32_t>(0x152B5B8);
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			b7_731780(a1);
			d = MEM<uint32_t>(0x152B5B8);
		}
		const uint16_t next = U16(d, 0x44);
		if (U16(d, 0x42) != next)
		{
			U16(d, 0x42) = next;
			a_73A6D0();
		}
		return 0; // void
	}

	// 0x72AB80 (module 097 sub_72AB80): emitter-stage state - once the frame counter (+0x24) reaches
	// +0x298 runs 0x72ABB0 (a_73F990) and the emitter stage step (b7_72B690), next state
	uint32_t __cdecl b7_72AB80(uint32_t a1)
	{
		if (S16(a1, 0x24) < S16(a1, 0x298))
			return 0; // void
		a_73F990(a1);
		b7_72B690(a1);
		b71_next_state(a1);
		return 0; // void
	}

	// 0x72B690 (module 097 sub_72B690): emitter stage machine of node a1 (director = a1+0x34 ->
	// [0x257B5CC]; stage +0x20): 0 -> 1; 1 -> step emitters/particles (a_743190, b7_72CEE0,
	// a_740700, a_735440), ++tick, -> 2 when tick >= min ticks and nothing alive; 2 -> returns 1;
	// always accumulates the live counts into [0x257B5A0] / [0x257B168]
	uint32_t __cdecl b7_72B690(uint32_t a1)
	{
		uint32_t dir = a1 + 0x34;
		uint32_t done = 0;
		MEM<uint32_t>(0x257B5CC) = dir;
		const uint16_t stage = U16(dir, 0x20);
		switch ((int32_t)(int16_t)stage)
		{
		case 0:
			U16(dir, 0x20) = (uint16_t)(stage + 1);
			break;
		case 1:
			a_743190();
			b7_72CEE0();
			a_740700();
			a_735440();
			dir = MEM<uint32_t>(0x257B5CC);
			U16(dir, 0x18) = (uint16_t)(U16(dir, 0x18) + 1);
			if (S16(dir, 0x18) >= S16(dir, 0x1A) && U16(dir, 0x14) == 0 && U16(dir, 0x16) == 0)
				U16(dir, 0x20) = (uint16_t)(U16(dir, 0x20) + 1);
			break;
		case 2:
			done = 1;
			break;
		default:
			break;
		}
		const uint16_t n1 = U16(dir, 0x14);
		const uint16_t n2 = U16(dir, 0x16);
		MEM<uint16_t>(0x257B5A0) = (uint16_t)(MEM<uint16_t>(0x257B5A0) + n1);
		MEM<uint16_t>(0x257B168) = (uint16_t)(MEM<uint16_t>(0x257B168) + n2);
		return done;
	}

	// 0x72CEE0 (module 097 sub_72CEE0): walks the emitter instance list (director +0x2C, next +4):
	// kind (+8) 1 -> b7_72CF20 (update + transform), 0 -> 0x72E480 (a_737ED0)
	uint32_t __cdecl b7_72CEE0(void)
	{
		uint32_t e = U32(MEM<uint32_t>(0x257B5CC), 0x2C);
		while (e != 0)
		{
			const int32_t kind = (int32_t)S16(e, 8);
			if (kind == 1)
				b7_72CF20(e);
			else if (kind == 0)
				a_737ED0(e);
			e = U32(e, 4);
		}
		return 0; // void
	}

	// 0x72CF20 (module 097 sub_72CF20): one emitter instance a1 (descriptor = director+0x224
	// [+0x1D6]): state (+0x1CC) 0 -> animate (0x72D6F0), end test (0x72E330: -> state 1, clear
	// texture +0x170), else spawn (0x72CFB0) + transform (b7_72D7C0) and ++frame (+0x1CA);
	// state 1 -> unlink (0x72F640), +0x1D7 = 0, --director emitter count (+0x16)
	uint32_t __cdecl b7_72CF20(uint32_t a1)
	{
		const uint32_t e = a1;
		const uint32_t table = U32(MEM<uint32_t>(0x257B5CC), 0x224);
		const int32_t di = (int32_t)S8(e, 0x1D6);
		const uint32_t desc = U32(table + (uint32_t)(di * 4), 0);
		const int32_t state = (int32_t)S16(e, 0x1CC);
		if (state == 1)
		{
			a_742CD0(e);
			const uint32_t dir = MEM<uint32_t>(0x257B5CC);
			U8(e, 0x1D7) = 0;
			U16(dir, 0x16) = (uint16_t)(U16(dir, 0x16) - 1);
			return 0; // void
		}
		if (state != 0)
			return 0; // void
		a_741050(e, desc);
		if (a_7419C0(e, desc) == 0)
		{
			a_740910(e, desc);
			b7_72D7C0(e, desc);
			U16(e, 0x1CA) = (uint16_t)(U16(e, 0x1CA) + 1);
			return 0; // void
		}
		U16(e, 0x1CC) = (uint16_t)(U16(e, 0x1CC) + 1);
		U32(e, 0x170) = 0;
		return 0; // void
	}

	// 0x72D7C0 (module 097 sub_72D7C0; ChocoFire variant of engine 0x741120): builds the transform
	// of emitter instance a1 from descriptor a2: keyframed modes 4/5 (+0x16) pick the frame's
	// texture entry (+0x170; 5 also calls 0x72E220) and, when +0x1D6 == 0, scroll the texture V by
	// -8 (b7_72DF60); rotation matrix +0x8C by +0x1B (camera+roll / X 0x400 / camera yaw / ordered
	// rotations from parent-or-identity or camera); frame colour/scale (+0x16C..E, +0x1CE,
	// +0xD4..D8 -> scale3DMatrix); projects the position(s) (+0xDC.., stride 0x10, count +0x1D8):
	// one -> +0xC0..C8 (+0xC8 += depth bias a2+0x38), several -> s16 x,y,z list at +0x19C (stride 8)
	uint32_t __cdecl b7_72D7C0(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = a1;
		const uint32_t desc = a2;
		const uint32_t m = node + 0x8C;
		uint8_t mode = U8(desc, 0x16);
		if (mode == 4 || mode == 5)
		{
			const int32_t frame = (int32_t)S16(node, 0x1CA);
			const uint32_t tex = U8(U32(desc, 0x12C) + (uint32_t)frame, 0);
			const uint32_t dir = MEM<uint32_t>(0x257B5CC);
			U32(node, 0x170) = U32(U32(dir, 0x22C) + tex * 4, 0); // texture entry of this frame
			if (mode == 5)
				b_72E220(node, desc);
			// 0x72D80F
			if (U8(node, 0x1D6) == 0)
				b7_72DF60(U32(node, 0x170), (uint32_t)-8);
		}

		// 0x72D82A
		const int32_t rot_mode = (int32_t)S8(desc, 0x1B);
		switch ((uint32_t)rot_mode)
		{
		case 0: // 0x72D83E: camera rotation + roll (+0xD0)
			x::UnpackRotationMatrix(MEM<uint32_t>(0x2575378), m);
			if (U16(node, 0xD0) != 0)
				x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
			break;
		case 1: // 0x72DA30: identity rotated by 0x400 (8DD7E0)
			x::MAG_022_sub_8DD770(m);
			x::sub_8DD7E0(m, 0x400);
			break;
		case 2: // 0x72DA4F: identity + camera yaw
		{
			const uint32_t yaw = x::sub_8DD7B0(MEM<uint32_t>(0x2575378));
			x::MAG_022_sub_8DD770(m);
			if ((uint16_t)yaw != 0)
				x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)(int16_t)yaw);
			break;
		}
		case 3: // 0x72D868 (table 0x72DF40)
		case 4: // 0x72DA84 (table 0x72DF50, same code)
		{
			const int32_t sub = (int32_t)S8(desc, 0x1D);
			const uint32_t parent = U32(node, 0x1BC);
			b71_build_rot(node, sub, parent);
			break;
		}
		default:
			break;
		}

		// 0x72DC45
		mode = U8(desc, 0x16);
		if (mode == 4 || mode == 5)
		{
			const int32_t frame = (int32_t)S16(node, 0x1CA);
			U8(node, 0x16C) = U8(U32(desc, 0xF0) + (uint32_t)frame, 0);
			U8(node, 0x16D) = U8(U32(desc, 0xF4) + (uint32_t)frame, 0);
			U8(node, 0x16E) = U8(U32(desc, 0xF8) + (uint32_t)frame, 0);
			U16(node, 0x1CE) = U16(U32(desc, 0xFC) + (uint32_t)(frame * 2), 0);
			U16(node, 0xD4) = U16(U32(desc, 0xE4) + (uint32_t)(frame * 2), 0); // scale x
			U16(node, 0xD6) = U16(U32(desc, 0xE8) + (uint32_t)(frame * 2), 0); // scale y
			const uint16_t sz = U16(U32(desc, 0xEC) + (uint32_t)(frame * 2), 0);
			int32_t scale[4]; // [esp+0x10] (the 4th dword is never written by the original)
			scale[0] = (int32_t)S16(node, 0xD4);
			scale[1] = (int32_t)S16(node, 0xD6);
			U16(node, 0xD8) = sz;                                  // scale z
			scale[2] = (int32_t)(int16_t)sz;
			scale[3] = 0; // UNINIT [esp+0x1C] at 0x72DCE2 (not read by scale3DMatrix)
			x::scale3DMatrix(m, P(scale));
		}

		// 0x72DCFD
		const uint8_t count = U8(node, 0x1D8);
		if (count == 1)
		{
			b71_world_to_view(node, node + 0xDC, MEM<uint32_t>(0x2575378));
			U32(node, 0xC8) = (uint32_t)add32(S32(node, 0xC8), (int32_t)S16(desc, 0x38));
			return 0; // void
		}
		// 0x72DDF7: point list
		if ((int8_t)count > 0)
		{
			int32_t i = 0;
			uint32_t src = node + 0xDC;   // esi - 4
			uint32_t dst = node + 0x19C;  // edi - 2
			do
			{
				const uint32_t cam = MEM<uint32_t>(0x2575378);
				b71_world_to_view(node, src, cam);
				U16(dst, 0) = U16(node, 0xC0);
				U16(dst, 2) = U16(node, 0xC4);
				const uint16_t z = (uint16_t)(U16(node, 0xC8) + U16(desc, 0x38));
				src += 0x10;
				i++;
				U16(dst, 4) = z;
				dst += 8;
			} while (i < (int32_t)S8(node, 0x1D8));
		}
		return 0; // void
	}

	// 0x72DF60 (module 097 sub_72DF60): scrolls the texture V coordinates of every textured
	// primitive of model a1 (4 textured lists: 0x14 / 0x18-byte records after two skipped 12-byte
	// lists, then 0x1C / 0x24-byte records after a skipped 0x14 and 0x18 list) by a2 (-8), wrapping
	// into the 128-texel page (-0x40 on all of a primitive's Vs when any leaves 0..0x7F)
	uint32_t __cdecl b7_72DF60(uint32_t a1, uint32_t a2)
	{
		const uint32_t d = a2;
		uint32_t p = b71_section(a1, 0);
		// skip two lists of 12-byte records
		p = p + (uint32_t)mul32(S32(p, 0), 12) + 4;
		p = p + (uint32_t)mul32(S32(p, 0), 12) + 4;
		// 0x72DF8F: 0x14-byte textured triangles
		int32_t n = S32(p, 0);
		p += 4;
		for (; n > 0; n--)
		{
			b71_scroll3(p, d);
			p += 0x14;
		}
		// 0x72E015: 0x18-byte textured quads
		n = S32(p, 0);
		p += 4;
		for (; n > 0; n--)
		{
			b71_scroll4(p, d);
			p += 0x18;
		}
		// 0x72E0CA: skip a 0x14-byte list and a 0x18-byte list
		p = p + (uint32_t)mul32(S32(p, 0), 20) + 4;
		p = p + (uint32_t)mul32(S32(p, 0), 24) + 4;
		// 0x72E0DC: 0x1C-byte triangles
		n = S32(p, 0);
		p += 4;
		for (; n > 0; n--)
		{
			b71_scroll3(p, d);
			p += 0x1C;
		}
		// 0x72E162: 0x24-byte quads
		n = S32(p, 0);
		p += 4;
		for (; n > 0; n--)
		{
			b71_scroll4(p, d);
			p += 0x24;
		}
		return 0; // void
	}

	// 0x72FB90 (module 097 sub_72FB90): runs the emitter stage (b7_72B690); when it reports done
	// marks the node finished (+0x26 |= 1) and advances the state
	uint32_t __cdecl b7_72FB90(uint32_t a1)
	{
		if (b7_72B690(a1) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x72FBC0 (module 097 sub_72FBC0): emitter-stage TASK - runs its 8-state table (0x72FC40 ..
	// 0x730560); ends when finished and childless
	uint32_t __cdecl b7_72FBC0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[8];
		states[0] = 0x72FC40;
		states[1] = 0x730440;
		states[2] = 0x730460;
		states[3] = 0x7304B0;
		states[4] = 0x7304E0;
		states[5] = 0x730510;
		states[6] = 0x730530;
		states[7] = 0x730560;
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return b71_task_end(node, status);
	}

	// 0x730F10 (module 097 sub_730F10): creature state - steps the pose (0x8DD1C0); at script mark 4
	// (a_73B640) starts animation 4 and plays sound effect 0x152B5C4 (BdPlaySE, pan 0x80), next state
	uint32_t __cdecl b7_730F10(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (a_73B640(4) != 0)
		{
			x::au_re_Battle_ReadAnimation_7(a1, 4);
			x::BdPlaySE(0x152B5C4, 0, 0x80);
			b71_next_state(a1);
		}
		return 0; // void
	}

	// 0x7310C0 (module 097 sub_7310C0): director state - counts down +0x44; at <= 0 spawns the
	// prim-model task 0x731100 (b_72A610: model [0x257AFB8]+0x130, 0x290 B, kind 3), next state
	uint32_t __cdecl b7_7310C0(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) > 0)
			return 0; // void
		const uint32_t model = U32(MEM<uint32_t>(0x257AFB8), 0x130);
		b_72A610(a1, 0x731100, model, 0x290, 3, 0);
		b71_next_state(a1);
		return 0; // void
	}

	// 0x731100 (module 097 sub_731100): model TASK - runs its 3-state table (0x731520, 0x731570,
	// nullsub), then animates the texture/UV sections +0xC, +0x10, +0x1C, +0x20 of the model at
	// +0x74 (a_7340A0 step 6); ends when finished and childless
	uint32_t __cdecl b7_731100(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x731520;
		states[1] = 0x731570;
		states[2] = 0x731590;
		callp(states[S8(node, 0x29)], node);
		a_7340A0(b71_section(U32(node, 0x74), 0x0C), 6);
		a_7340A0(b71_section(U32(node, 0x74), 0x10), 6);
		a_7340A0(b71_section(U32(node, 0x74), 0x1C), 6);
		a_7340A0(b71_section(U32(node, 0x74), 0x20), 6);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return b71_task_end(node, status);
	}

	// 0x7315A0 (module 097 sub_7315A0): director state - at script mark 5 (a_73B640) sets the
	// countdown +0x44 = 0x20, next state
	uint32_t __cdecl b7_7315A0(uint32_t a1)
	{
		if (a_73B640(5) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x44) = 0x20;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x731600 (module 097 sub_731600): director state - counts down +0x44; at <= 0x10 spawns task
	// 0x731630 (0x70 B, queue 0x2575458), next state
	uint32_t __cdecl b7_731600(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) > 0x10)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(0x2575458, 0x731630, 0x70, a1);
		b71_next_state(a1);
		return 0; // void
	}

	// 0x731780 (module 097 sub_731780): phase entry spawns by the new phase [0x152B5B8]+0x40:
	// 2 -> model task 0x731840 ([0x257AFB8]+0x128, 0x44C B, kind 1) + task 0x731A50 (0x70 B, queue
	// 0x2575458); 4 -> model task 0x731950 (+0x12C, 0x170 B, kind 2) + task 0x72AB20 (0x73C100:
	// +0x108, 0, 0x2D, 2); 5 -> task 0x731B40 (0x70 B, queue 0x2575458)
	uint32_t __cdecl b7_731780(uint32_t a1)
	{
		const int32_t phase = (int32_t)S16(MEM<uint32_t>(0x152B5B8), 0x40);
		if (phase == 2)
		{
			const uint32_t model = U32(MEM<uint32_t>(0x257AFB8), 0x128);
			b_72A610(a1, 0x731840, model, 0x44C, 1, 0);
			x::Effect_AddTaskAndInitFromCtx(0x2575458, 0x731A50, 0x70, a1);
		}
		else if (phase == 4)
		{
			const uint32_t model = U32(MEM<uint32_t>(0x257AFB8), 0x12C);
			b_72A610(a1, 0x731950, model, 0x170, 2, 0);
			const uint32_t model2 = U32(MEM<uint32_t>(0x257AFB8), 0x108);
			a_73C100(a1, 0x72AB20, model2, 0, 0x2D, 2);
		}
		else if (phase == 5)
		{
			x::Effect_AddTaskAndInitFromCtx(0x2575458, 0x731B40, 0x70, a1);
		}
		return 0; // void
	}

	// ====================================================================================
	// part b81
	// ====================================================================================
	// Effect 098: ChocoFlare (Boko) - module-only functions (part b81), raw module addresses.
	// Module globals used: 0x152AF30 phase-script cursor (u16 +0x40 phase shown, +0x42 phase
	// requested, +0x44 phase next, +0x46 frames in phase, +0x48 flag, +0x52 claimed voice slot),
	// 0x2574D38 asset table (= 0x25750F8: +0x100 arena, +0x104.. model/prim-layout pointers),
	// task queues 0x256E440 / 0x256E548 / 0x25750D0 / 0x25732B0.
	// 0x721CA0 (module 098 MAG_098_sub_721CA0): director state 0: resets (b_72A350), binds the
	// asset table (b8_721CF0), spawns the 0x721DF0 task (queue 0x25750D0) and the phase director
	// 0x722210 (queue 0x256E440); advances the state index
	uint32_t __cdecl b8_721CA0(uint32_t a1)
	{
		b_72A350(); // the original pushes a1 (unused)
		b8_721CF0();
		x::Effect_AddTaskAndInitFromCtx(0x25750D0, 0x721DF0, 0x30, a1);
		x::Effect_AddTaskAndInitFromCtx(0x256E440, 0x722210, 0x48, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x721CF0 (module 098 MAG_098_sub_721CF0): initialises the asset table 0x25750F8 (0x16C
	// bytes, via 0x8DCC00), carves a 0xA000-byte arena from 0x25750F0 and fills the model /
	// prim-layout pointers (0x19DB018 default, 0x19DFEF8, 0x19E187C, 0x19E5584, 0x19E8E70)
	uint32_t __cdecl b8_721CF0(void)
	{
		MEM<uint32_t>(0x2573C40) = 0x152B2A8;
		MEM<uint32_t>(0x2574D38) = 0x25750F8;
		x::MAG_007_sub_8DCC00(0x25750F8, 0x16C);
		uint32_t t = MEM<uint32_t>(0x2574D38);
		uint32_t arena = MEM<uint32_t>(0x25750F0);
		U32(t, 0x100) = arena;
		MEM<uint32_t>(0x25750F0) = arena + 0xA000;
		const uint32_t base = 0x19DB018;
		U32(t, 0x114) = base;
		U32(t, 0x118) = base;
		U32(t, 0x11C) = base;
		U32(t, 0x120) = base;
		U32(t, 0x104) = base;
		U32(t, 0x108) = 0x19DFEF8;
		U32(t, 0x10C) = base;
		U32(t, 0x110) = base;
		U32(t, 0x124) = base;
		U32(t, 0x128) = base;
		U32(t, 0x12C) = 0x19E187C;
		U32(t, 0x130) = 0x19E5584;
		U32(t, 0x134) = 0x19E8E70;
		for (int32_t o = 0x138; o <= 0x160; o += 4)
			U32(t, o) = base;
		U16(t, 0x164) = 0;
		U16(t, 0x166) = 0;
		return 0; // void
	}

	// 0x722210 (module 098 MAG_098_sub_722210): TASK phase director: steps the phase script
	// (b8_7222F0), then runs state [+0x29] of its 19-entry state table; ends when finished and
	// no child is alive
	uint32_t __cdecl b8_722210(uint32_t a1)
	{
		const uint32_t tab[19] = {
			0x722340, 0x7280E0, 0x728140, 0x7281D0, 0x728B20, 0x728B40, 0x728B60, 0x728B80,
			0x728BA0, 0x728BC0, 0x728BE0, 0x728C10, 0x728D60, 0x729240, 0x729270, 0x7292A0,
			0x7293A0, 0x7293C0, 0x729400,
		};
		b8_7222F0(a1);
		callp(tab[S8(a1, 0x29)], a1);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((U8(a1, 0x26) & 1) != 0 && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x7222F0 (module 098 sub_7222F0): phase script step: counts frames in the phase (+0x46);
	// on a new requested phase (+0x42 != +0x40) resets the counter and spawns the phase's tasks
	// (b8_729420); latches +0x44 into +0x42 (then calls the null state 0x729410)
	uint32_t __cdecl b8_7222F0(uint32_t a1)
	{
		uint32_t cur = MEM<uint32_t>(0x152AF30);
		U16(cur, 0x46) = (uint16_t)(U16(cur, 0x46) + 1);
		uint16_t req = U16(cur, 0x42);
		if (U16(cur, 0x40) != req)
		{
			U16(cur, 0x40) = req;
			U16(cur, 0x46) = 0;
			b8_729420(a1);
			cur = MEM<uint32_t>(0x152AF30);
		}
		uint16_t nxt = U16(cur, 0x44);
		if (U16(cur, 0x42) != nxt)
		{
			U16(cur, 0x42) = nxt;
			a_73A6D0(); // 0x729410 nullsub (the original pushes a1)
		}
		return 0; // void
	}

	// 0x722340 (module 098 sub_722340): director state: claims a sound voice slot (kept in
	// cursor +0x52), spawns the 0x722920 task (a_73C100, layout 0x167D934), the prim-model task
	// 0x722460 (layout 0x1680B9C) and the sub-director 0x7276F0 (queue 0x256E548), queues the
	// TIM upload 0x1679714; advances the state index
	uint32_t __cdecl b8_722340(uint32_t a1)
	{
		uint32_t slot = x::BdSound_ClaimVoiceSlot(0x16746FC, 1, 0x80);
		uint32_t cur = MEM<uint32_t>(0x152AF30);
		U16(cur, 0x52) = (uint16_t)slot;
		a_73C100(a1, 0x722920, 0x167D934, 0x12, 0x2D, 2);
		b8_722410(a1, 0x722460, 0x1680B9C, 0x4AC, 0, 0);
		x::Effect_AddTaskAndInitFromCtx(0x256E548, 0x7276F0, 0x70, a1);
		x::Battle_QueueTIMUpload_GetEOF(0x1679714);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x722410 (module 098 sub_722410): spawns a 0x864-byte prim-model task fn=a2 (queue
	// 0x25732B0, parent a1) with layout +0x74 = a3, +0x78 = (s16)a4, u16 +0x80 = a5,
	// u16 +0x82 = a6; returns the node
	uint32_t __cdecl b8_722410(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x25732B0, a2, 0x864, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		U32(t, 0x78) = (uint32_t)(int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x7276F0 (module 098 sub_7276F0): TASK sub-director (camera script): runs state [+0x29]
	// of its 9-entry state table; ends when finished and no child is alive
	uint32_t __cdecl b8_7276F0(uint32_t a1)
	{
		const uint32_t tab[9] = {
			0x727780, 0x727F80, 0x727FA0, 0x727FF0, 0x728020, 0x728050, 0x728080, 0x7280A0, 0x7280D0,
		};
		callp(tab[S8(a1, 0x29)], a1);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((U8(a1, 0x26) & 1) != 0 && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x728050 (module 098 sub_728050): state: when the camera stepper a_73AE10 reports the end
	// of its script (1), starts the camera script 0x152B538 at cursor+8 and advances the state
	// index (same code as a_7334C0 with another script)
	uint32_t __cdecl b8_728050(uint32_t a1)
	{
		if (a_73AE10() == 1)
		{
			uint32_t cur = MEM<uint32_t>(0x152AF30);
			a_73AB20(0x152B538, cur + 8, 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x728AE0 (module 098 sub_728AE0): state: advances the node's animation (0x8DD1C0); once
	// the frame counter s16 +0x136 reaches 20 marks the node finished (+0x26 |= 5) and advances
	uint32_t __cdecl b8_728AE0(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		if (S16(a1, 0x136) >= 0x14)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x728B20 (module 098 sub_728B20): state: waits for a_73A950() == 1 and a_73B640(1) != 0,
	// then advances the state index
	uint32_t __cdecl b8_728B20(uint32_t a1)
	{
		if (a_73A950() == 1)
		{
			if (a_73B640(1) != 0)
				U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x728C10 (module 098 sub_728C10): state: counts down u16 +0x44; at <= 0 spawns the
	// prim-model task 0x728C60 (asset +0x130, 0x494, 3), reloads +0x44 = 24 and advances
	uint32_t __cdecl b8_728C10(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) <= 0)
		{
			uint32_t layout = U32(MEM<uint32_t>(0x2574D38), 0x130);
			b8_722410(a1, 0x728C60, layout, 0x494, 3, 0);
			uint8_t st = U8(a1, 0x29);
			U16(a1, 0x44) = 0x18;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x728CC0 (module 098 MAG_310_HolyWar_Code): prim-model task state 0: decodes the layout
	// (+0x74, frame +0x78) into +0x94, takes the position from cursor +8/+0xC (y - 500), scale
	// 1.0 on 3 axes, b_72A720 setup, plays sound 0x152AF3C; advances the state index
	uint32_t __cdecl b8_728CC0(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		uint32_t cur = MEM<uint32_t>(0x152AF30);
		uint32_t xy = U32(cur, 8);
		uint32_t zw = U32(cur, 0xC);
		U32(a1, 0x1C) = xy;
		U32(a1, 0x20) = zw;
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + 0xFE0C);
		U32(a1, 0x58) = 0x1000;
		U32(a1, 0x54) = 0x1000;
		U32(a1, 0x50) = 0x1000;
		b_72A720(a1);
		x::BdPlaySE(0x152AF3C, 0, 0x80);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x728D60 (module 098 sub_728D60): state: counts down u16 +0x44; at <= 0 spawns the
	// prim-model task 0x728DA0 (asset +0x134, 2000, 4) and advances the state index
	uint32_t __cdecl b8_728D60(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) <= 0)
		{
			uint32_t layout = U32(MEM<uint32_t>(0x2574D38), 0x134);
			b8_722410(a1, 0x728DA0, layout, 0x7D0, 4, 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// b8_728DA0 helper: layout + (s32 layout[+off] / 4) * 4 = the layout section at byte offset
	// layout[+off] (cdq/and 3/add/sar 2 = signed division by 4)
	static uint32_t b81_section(uint32_t layout, int32_t off)
	{
		int32_t q = S32(layout, off) / 4;
		return layout + (uint32_t)q * 4u;
	}

	// 0x728DA0 (module 098 sub_728DA0): TASK prim-model task: runs state [+0x29] of its 3-entry
	// table, then animates 4 sections of the layout +0x74 with a_7340A0 (sections at header
	// +8 (6), +0xC (4), +0x10 (6), +0x1C (8)); ends when finished and no child is alive
	uint32_t __cdecl b8_728DA0(uint32_t a1)
	{
		const uint32_t tab[3] = { 0x7291C0, 0x729210, 0x729230 };
		callp(tab[S8(a1, 0x29)], a1);
		a_7340A0(b81_section(U32(a1, 0x74), 0x8), 6);
		a_7340A0(b81_section(U32(a1, 0x74), 0xC), 4);
		a_7340A0(b81_section(U32(a1, 0x74), 0x10), 6);
		a_7340A0(b81_section(U32(a1, 0x74), 0x1C), 8);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((U8(a1, 0x26) & 1) != 0 && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x729240 (module 098 sub_729240): state: waits for a_73B640(5) != 0, then sets the
	// countdown u16 +0x44 = 44 and advances the state index
	uint32_t __cdecl b8_729240(uint32_t a1)
	{
		if (a_73B640(5) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U16(a1, 0x44) = 0x2C;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x729270 (module 098 sub_729270): state: counts down u16 +0x44; at <= 24 sets the cursor
	// flag u16 +0x48 = 1 and advances the state index
	uint32_t __cdecl b8_729270(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) <= 0x18)
		{
			U16(MEM<uint32_t>(0x152AF30), 0x48) = 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7292A0 (module 098 sub_7292A0): state: counts down u16 +0x44; at <= 8 spawns the
	// 0x7292D0 task (queue 0x256E548) and advances the state index
	uint32_t __cdecl b8_7292A0(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) <= 8)
		{
			x::Effect_AddTaskAndInitFromCtx(0x256E548, 0x7292D0, 0x70, a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x729350 (module 098 sub_729350): state: fades s16 +0x1C down by 192 per tick; at <= 0
	// clamps to 0, marks finished and advances; copies +0x1C into the 4 records (stride 0x2C)
	// of the shared table at 0x1D98992
	uint32_t __cdecl b8_729350(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xFF40);
		if (S16(a1, 0x1C) <= 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		uint16_t v = U16(a1, 0x1C);
		uint32_t p = 0x1D98992; // shared table (raw)
		for (int n = 4; n != 0; n--, p += 0x2C)
			MEM<uint16_t>(p) = v;
		return 0; // void
	}

	// 0x729420 (module 098 sub_729420): spawns the tasks of the new phase [cursor +0x40]:
	// 2 = prim-model task 0x7294C0 (asset +0x128, 1100, 1) + task 0x7296D0; 4 = prim-model
	// task 0x7295D0 (asset +0x12C, 368, 2); 5 = task 0x7297C0 (queue 0x256E548)
	uint32_t __cdecl b8_729420(uint32_t a1)
	{
		int32_t phase = S16(MEM<uint32_t>(0x152AF30), 0x40);
		if (phase == 2)
		{
			uint32_t layout = U32(MEM<uint32_t>(0x2574D38), 0x128);
			b8_722410(a1, 0x7294C0, layout, 0x44C, 1, 0);
			x::Effect_AddTaskAndInitFromCtx(0x256E548, 0x7296D0, 0x70, a1);
		}
		else if (phase == 4)
		{
			uint32_t layout = U32(MEM<uint32_t>(0x2574D38), 0x12C);
			b8_722410(a1, 0x7295D0, layout, 0x170, 2, 0);
		}
		else if (phase == 5)
		{
			x::Effect_AddTaskAndInitFromCtx(0x256E548, 0x7297C0, 0x70, a1);
		}
		return 0; // void
	}

	// ====================================================================================
	// part b91
	// ====================================================================================
	// ChocoMeteor (099) module-only code, part b91: master state init, director data, the director
	// task 0x718750 (21-state script) and its phase spawner 0x71F620, the camera task 0x71DC40
	// (keyframe camera states), the Boko creature actor 0x71E8D0 and its states, and the prim-model
	// particle task states (meteor / rock / dust models).
namespace
{
	// common task tail: the task ends (returns 2, releasing its linked task) when the status flag
	// bit0 (finished) is set and no child holds the busy lock
	uint32_t b91_task_end(uint32_t node, uint8_t status)
	{
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}
	inline void b91_next(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	// mov cl,[+0x29]; or byte [+0x26],1; inc cl; mov [+0x29],cl
	inline void b91_finish(uint32_t node)
	{
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) = (uint8_t)(U8(node, 0x26) | 1);
		U8(node, 0x29) = (uint8_t)(st + 1);
	}
}

	// 0x7181B0 (module 099 MAG_099_sub_7181B0): master state - inits the director data (b_718670 +
	// b9_718200), spawns task 0x718310 (0x30 B, queue 0x256E1C0) and the director task 0x718750
	// (0x48 B, queue 0x2565B18), next state.
	uint32_t __cdecl b9_7181B0(uint32_t a1)
	{
		b_718670();
		b9_718200();
		x::Effect_AddTaskAndInitFromCtx(0x256E1C0, 0x718310, 0x30, a1);
		x::Effect_AddTaskAndInitFromCtx(0x2565B18, 0x718750, 0x48, a1);
		b91_next(a1);
		return 0; // void
	}

	// 0x718200 (module 099 MAG_099_sub_718200): clears the 0x16C-byte asset table 0x256E1E8
	// ([0x256D6F0] = it, [0x256C5F8] = 0x152AAA0), gives it 0xA000 bytes of the arena [0x256E1E0]
	// (+0x100) and fills its model/data pointer slots +0x104..+0x160 (loaded file addresses).
	uint32_t __cdecl b9_718200(void)
	{
		MEM<uint32_t>(0x256C5F8) = 0x152AAA0;
		MEM<uint32_t>(0x256D6F0) = 0x256E1E8;
		x::MAG_007_sub_8DCC00(0x256E1E8, 0x16C);
		const uint32_t t = MEM<uint32_t>(0x256D6F0);
		const uint32_t arena = MEM<uint32_t>(0x256E1E0);
		U32(t, 0x100) = arena;
		MEM<uint32_t>(0x256E1E0) = arena + 0xA000;
		const uint32_t base = 0x19D3018;
		U32(t, 0x114) = base;
		U32(t, 0x118) = base;
		U32(t, 0x11C) = base;
		U32(t, 0x120) = base;
		U32(t, 0x104) = base;
		U32(t, 0x108) = base;
		U32(t, 0x10C) = base;
		U32(t, 0x110) = base;
		U32(t, 0x124) = base;
		U32(t, 0x128) = 0x19E3018;
		U32(t, 0x12C) = 0x19EB018;
		U32(t, 0x130) = 0x19EED20;
		U32(t, 0x134) = 0x19F0F40;
		U32(t, 0x138) = 0x19DF380;
		U32(t, 0x13C) = 0x19E2054;
		U32(t, 0x140) = 0x19E4778;
		U32(t, 0x144) = 0x19DD2D0;
		U32(t, 0x148) = 0x19DE510;
		U32(t, 0x14C) = base;
		U32(t, 0x150) = base;
		U32(t, 0x154) = base;
		U32(t, 0x158) = base;
		U32(t, 0x15C) = base;
		U32(t, 0x160) = base;
		U16(t, 0x164) = 0;
		U16(t, 0x166) = 0;
		return t; // eax = table
	}

	// 0x718750 (module 099 MAG_099_sub_718750): ChocoMeteor DIRECTOR task - phase bookkeeping
	// (b9_718840), runs its 21-state script table; ends when finished with no children. No drawing.
	uint32_t __cdecl b9_718750(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[21];
		states[0] = 0x718890;
		states[1] = 0x71E7A0;
		states[2] = 0x71E800;
		states[3] = 0x71E890;
		states[4] = 0x71F1D0;
		states[5] = 0x71F1F0;
		states[6] = 0x71F220;
		states[7] = 0x71F240;
		states[8] = 0x71F260;
		states[9] = 0x71F280;
		states[10] = 0x71F2A0;
		states[11] = 0x71F2D0;
		states[12] = 0x71F4C0;
		states[13] = 0x71F4E0;
		states[14] = 0x71F500;
		states[15] = 0x71F520;
		states[16] = 0x71F550;
		states[17] = 0x71F580;
		states[18] = 0x71F5A0;
		states[19] = 0x71F5C0;
		states[20] = 0x71F600;  // nullsub
		b9_718840(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return b91_task_end(node, status);
	}

	// 0x718840 (module 099 sub_718840): director phase bookkeeping on the director data
	// [0x1529FA0]: ++phase timer +0x46; when the requested phase +0x42 differs from the current
	// +0x40, switch (+0x40 = +0x42, timer = 0) and spawn that phase's tasks (b9_71F620); when +0x44
	// differs from +0x42, +0x42 = +0x44 (nullsub hook).
	uint32_t __cdecl b9_718840(uint32_t a1)
	{
		uint32_t d = MEM<uint32_t>(0x1529FA0);
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		const uint16_t req = U16(d, 0x42);
		if (U16(d, 0x40) != req)
		{
			U16(d, 0x40) = req;
			U16(d, 0x46) = 0;
			b9_71F620(a1);
			d = MEM<uint32_t>(0x1529FA0);
		}
		const uint16_t nxt = U16(d, 0x44);
		if (U16(d, 0x42) != nxt)
		{
			U16(d, 0x42) = nxt;
			return a_73A6D0();
		}
		return d;
	}

	// 0x718890 (module 099 sub_718890): director state 0 - claims a sound voice slot (0x16659D8,
	// id -> director data +0x52), spawns the creature actor (a_73C100: task 0x718E70, model
	// 0x166A9F0), the prim particle task 0x7189B0 (model 0x166DC58 index 0x4AC), the camera task
	// 0x71DC40 (0xB0 B, queue 0x2565C20), queues the TIM upload of 0x16704DC, next state.
	uint32_t __cdecl b9_718890(uint32_t a1)
	{
		const uint32_t slot = x::BdSound_ClaimVoiceSlot(0x16659D8, 1, 0x80);
		const uint32_t d = MEM<uint32_t>(0x1529FA0);
		const uint32_t node = a1;
		U16(d, 0x52) = (uint16_t)slot;
		a_73C100(node, 0x718E70, 0x166A9F0, 0x12, 0x2D, 2);
		b9_718960(node, 0x7189B0, 0x166DC58, 0x4AC, 0, 0);
		x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x71DC40, 0xB0, node);
		x::Battle_QueueTIMUpload_GetEOF(0x16704DC);
		b91_next(node);
		return 0; // void
	}

	// 0x718960 (module 099 sub_718960): spawns a prim-model particle task a2 (0x548 B, queue
	// 0x256B9D8, parent a1) with model a3 (+0x74), layout index (s16)a4 (+0x78), id a5 (+0x80),
	// a6 (+0x82); returns the node.
	uint32_t __cdecl b9_718960(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(0x256B9D8, a2, 0x548, a1);
		U16(t, 0x80) = (uint16_t)a5;
		U32(t, 0x74) = a3;
		S32(t, 0x78) = (int32_t)(int16_t)a4;
		U16(t, 0x82) = (uint16_t)a6;
		return t;
	}

	// 0x71DC40 (module 099 sub_71DC40): CAMERA task - runs its 10-state keyframe-camera script;
	// ends when finished with no children. No drawing.
	uint32_t __cdecl b9_71DC40(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[10];
		states[0] = 0x71DCD0;
		states[1] = 0x71E4D0;
		states[2] = 0x71E4F0;
		states[3] = 0x71E540;
		states[4] = 0x71E570;
		states[5] = 0x71E5C0;
		states[6] = 0x71E610;
		states[7] = 0x71E6C0;
		states[8] = 0x71E700;
		states[9] = 0x71E790;  // nullsub
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return b91_task_end(node, status);
	}

	// camera move start from a stack base vector (0, 0, 0, pad): b9_71E570 / b9_71E5C0
	static void b91_cam_from_origin(uint32_t key)
	{
		// stack vector [esp+0..7]: three zero words; the pad word +6 is never written.
		// UNINIT 0x71E588..0x71E5A2 / 0x71E5D8..0x71E5F2: a_73AB20 copies the dword +4 (z + pad)
		// into the camera block +0xA8, then into the start/end base points +0x68/+0x70 (pads =
		// camera block +0xAA/+0x6A/+0x72, 0x256D6AA/0x256D66A/0x256D672 with the block at
		// 0x256D600) -> 0 here. Only pad words, never read back as coordinates (words only).
		alignas(4) uint16_t v[4];
		v[0] = 0;
		v[1] = 0;
		v[2] = 0;
		v[3] = 0;  // UNINIT
		a_73AB20(key, P(v), 0);
	}

	// 0x71E570 (module 099 sub_71E570): camera state 4 - steps the camera move (a_73AE10); at
	// step 5 starts camera key 0x152AD20 from the origin, next state.
	uint32_t __cdecl b9_71E570(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(5) != 0)
		{
			b91_cam_from_origin(0x152AD20);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x71E5C0 (module 099 sub_71E5C0): camera state 5 - steps the camera move; at step 8 starts
	// camera key 0x152AD68 from the origin, next state.
	uint32_t __cdecl b9_71E5C0(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(8) != 0)
		{
			b91_cam_from_origin(0x152AD68);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x71E610 (module 099 sub_71E610): camera state 6 - free orbit: accelerates the camera block
	// [0x152AC40] distance speed +0x86 (+0x10, max 0x80) into distance +0x5E and the yaw speed
	// +0x8C (+1, max 0x20) into angle +0xD8 (12-bit, values < 0x800 snap to 0), writes the camera
	// (a_73B160); at step 10 starts camera key 0x152AE40 from the director origin, next state.
	uint32_t __cdecl b9_71E610(uint32_t a1)
	{
		const uint32_t c = MEM<uint32_t>(0x152AC40);
		U16(c, 0x86) = (uint16_t)(U16(c, 0x86) + 0x10);
		if (S16(c, 0x86) >= 0x80)
			U16(c, 0x86) = 0x80;
		U16(c, 0x5E) = (uint16_t)(U16(c, 0x5E) + U16(c, 0x86));
		U16(c, 0x8C) = (uint16_t)(U16(c, 0x8C) + 1);
		if (S16(c, 0x8C) >= 0x20)
			U16(c, 0x8C) = 0x20;
		const int16_t ang = (int16_t)((uint16_t)(U16(c, 0xD8) + U16(c, 0x8C)) & 0xFFF);
		U16(c, 0xD8) = (uint16_t)ang;
		if (ang < 0x800 && ang >= 0)
			U16(c, 0xD8) = 0;
		a_73B160();
		if (a_73AAE0(0xA) != 0)
		{
			const uint32_t d = MEM<uint32_t>(0x1529FA0);
			a_73AB20(0x152AE40, d + 8, 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x71E6C0 (module 099 sub_71E6C0): camera state 7 - steps the camera move; when done starts
	// camera key 0x152AE88 from the director origin, camera block yaw speed +0x8E = 6, next state.
	uint32_t __cdecl b9_71E6C0(uint32_t a1)
	{
		if (a_73AE10() == 1)
		{
			const uint32_t d = MEM<uint32_t>(0x1529FA0);
			a_73AB20(0x152AE88, d + 8, 0);
			const uint32_t c = MEM<uint32_t>(0x152AC40);
			U16(c, 0x8E) = 6;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x71E700 (module 099 sub_71E700): camera state 8 - steps the camera move; from step 0x42
	// the yaw speed +0x8E decays by 1 (min 0); yaw +0xDA += speed (12-bit); writes the camera; at
	// step 11 marks the task finished, next state.
	uint32_t __cdecl b9_71E700(uint32_t a1)
	{
		a_73AE10();
		const uint32_t c = MEM<uint32_t>(0x152AC40);
		if (S16(c, 0xE2) >= 0x42)
		{
			U16(c, 0x8E) = (uint16_t)(U16(c, 0x8E) - 1);
			if (S16(c, 0x8E) < 0)
				U16(c, 0x8E) = 0;
		}
		U16(c, 0xDA) = (uint16_t)((uint16_t)(U16(c, 0x8E) + U16(c, 0xDA)) & 0xFFF);
		a_73B160();
		if (a_73AAE0(0xB) != 0)
		{
			a_73A6D0();
			b91_finish(a1);
		}
		return 0; // void
	}

	// 0x71E8D0 (module 099 sub_71E8D0): Boko CREATURE actor task - runs its 9-state script, then
	// (unless hidden, +0x26 bit2) draws the model (a_746C10 with draw context 0x256C600 at the
	// packet cursor) and a_739890; ends when finished with no children.
	uint32_t __cdecl b9_71E8D0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[9];
		states[0] = 0x71ED20;
		states[1] = 0x71EDC0;
		states[2] = 0x71EDF0;
		states[3] = 0x71F080;
		states[4] = 0x71F0F0;
		states[5] = 0x71F140;
		states[6] = 0x71F160;
		states[7] = 0x71F190;
		states[8] = 0x71F1C0;  // nullsub
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 4) == 0)
		{
			// draw (after the update)
			const uint32_t cursor = MEM<uint32_t>(0x1D8E054);
			MEM<uint32_t>(0x1D8E054) = a_746C10(node, 0x256C600, cursor);
			a_739890(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return b91_task_end(node, status);
	}

	// 0x71EDF0 (module 099 sub_71EDF0): creature state 2 - advances the animation (sub_8DD1C0);
	// once +0x136 is >= 0: switches to animation 2 and spawns task 0x71EE40 (0xB0 B, queue
	// 0x2565C20), next state.
	uint32_t __cdecl b9_71EDF0(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (S16(node, 0x136) >= 0)
		{
			x::au_re_Battle_ReadAnimation_7(node, 2);
			x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x71EE40, 0xB0, node);
			b91_next(node);
		}
		return 0; // void
	}

	// 0x71EF30 (module 099 sub_71EF30): sprite state 0 - hides the node (+0x26 |= 4), position =
	// director origin + (0, -0x300, -0x280), sprite table +0x4C = 0x1529FB4, +0x52 = 0x13,
	// +0x54 = 0xFF00, timer +0xAC = 4, next state.
	uint32_t __cdecl b9_71EF30(uint32_t a1)
	{
		const uint32_t d = MEM<uint32_t>(0x1529FA0);
		const uint32_t node = a1;
		const uint32_t xy = U32(d, 0);
		const uint32_t zp = U32(d, 4);
		U16(node, 0x26) = (uint16_t)(U16(node, 0x26) | 4);
		U32(node, 0x1C) = xy;
		U32(node, 0x20) = zp;
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + 0xFD00);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + 0xFD80);
		U32(node, 0x4C) = 0x1529FB4;
		U16(node, 0x52) = 0x13;
		U16(node, 0x54) = 0xFF00;
		U16(node, 0xAC) = 4;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x71EF80 (module 099 sub_71EF80): sprite state - counts timer +0xAC down; at 0 shows the
	// node (+0x26 &= ~4), next state.
	uint32_t __cdecl b9_71EF80(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0xAC) = (uint16_t)(U16(node, 0xAC) - 1);
		if (S16(node, 0xAC) <= 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) = (uint8_t)(U8(node, 0x26) & 0xFB);
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x71EFB0 (module 099 sub_71EFB0): sprite state - steps the sprite animation (a_743C20);
	// once frame +0x50 > 8: timer +0xAC = 2, next state.
	uint32_t __cdecl b9_71EFB0(uint32_t a1)
	{
		const uint32_t node = a1;
		a_743C20(node);
		if (S16(node, 0x50) > 8)
		{
			const uint8_t st = U8(node, 0x29);
			U16(node, 0xAC) = 2;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x71F010 (module 099 sub_71F010): sprite state - steps the sprite animation holding frame
	// +0x50 at 8 (when > 9); counts timer +0xAC down, next state at 0.
	uint32_t __cdecl b9_71F010(uint32_t a1)
	{
		const uint32_t node = a1;
		a_743C20(node);
		if (S16(node, 0x50) > 9)
			U16(node, 0x50) = 8;
		U16(node, 0xAC) = (uint16_t)(U16(node, 0xAC) - 1);
		if (S16(node, 0xAC) <= 0)
			b91_next(node);
		return 0; // void
	}

	// 0x71F190 (module 099 sub_71F190): creature state 7 - at step 5 (a_73B7E0) hides + finishes
	// (+0x26 |= 5), next state; else advances the animation (sub_8DD1C0).
	uint32_t __cdecl b9_71F190(uint32_t a1)
	{
		if (a_73B7E0(5) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 5);
			U8(a1, 0x29) = (uint8_t)(st + 1);
			return a1;
		}
		return x::sub_8DD1C0(a1);
	}

	// 0x71F2D0 (module 099 sub_71F2D0): director state 11 - counts +0x44 down; at 0 spawns task
	// 0x71F310 (0xB0 B, queue 0x2565C20), next state.
	uint32_t __cdecl b9_71F2D0(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x44) = (uint16_t)(U16(node, 0x44) - 1);
		if (S16(node, 0x44) <= 0)
		{
			x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x71F310, 0xB0, node);
			b91_next(node);
		}
		return 0; // void
	}

	// 0x71F3A0 (module 099 sub_71F3A0): state - at step 5 runs a_746A10 + a_73B9F0, plays sound
	// effect 0x1529FAC (BdPlaySE), timer +0xAC = 0x24, next state.
	uint32_t __cdecl b9_71F3A0(uint32_t a1)
	{
		if (a_73B7E0(5) != 0)
		{
			a_746A10();
			a_73B9F0();
			x::BdPlaySE(0x1529FAC, 0, 0x80);
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0xAC) = 0x24;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x71F450 (module 099 sub_71F450): state - counts timer +0xAC down, next state at 0.
	uint32_t __cdecl b9_71F450(uint32_t a1)
	{
		U16(a1, 0xAC) = (uint16_t)(U16(a1, 0xAC) - 1);
		if (S16(a1, 0xAC) <= 0)
			b91_next(a1);
		return 0; // void
	}

	// 0x71F470 (module 099 sub_71F470): state - at step 7 (a_73B640) a_73BB60(0) and finishes,
	// next state; otherwise a_73BB60(1).
	uint32_t __cdecl b9_71F470(uint32_t a1)
	{
		if (a_73B640(7) != 0)
		{
			a_73BB60(0);
			b91_finish(a1);
			return 0; // void
		}
		return a_73BB60(1);
	}

	// 0x71F4C0 (module 099 sub_71F4C0): director state 12 - from step 5, waits for
	// a_73A950 == 1, next state.
	uint32_t __cdecl b9_71F4C0(uint32_t a1)
	{
		if (a_73B7E0(5) != 0)
		{
			if (a_73A950() == 1)
				b91_next(a1);
		}
		return 0; // void
	}

	// 0x71F520 (module 099 sub_71F520): director state 15 - at step 9: counter +0x44 = 0x20,
	// next state.
	uint32_t __cdecl b9_71F520(uint32_t a1)
	{
		if (a_73B7E0(9) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x44) = 0x20;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x71F550 (module 099 sub_71F550): director state 16 - counts +0x44 down; at 0: +0x44 = 0x28,
	// director data +0x48 = 1, next state.
	uint32_t __cdecl b9_71F550(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) <= 0)
		{
			const uint32_t d = MEM<uint32_t>(0x1529FA0);
			U16(a1, 0x44) = 0x28;
			U16(d, 0x48) = 1;
			b91_next(a1);
		}
		return 0; // void
	}

	// 0x71F5A0 (module 099 sub_71F5A0): director state 18 - at step 11 (a_73B640) finishes,
	// next state.
	uint32_t __cdecl b9_71F5A0(uint32_t a1)
	{
		if (a_73B640(0xB) != 0)
			b91_finish(a1);
		return 0; // void
	}

	// 0x71F5C0 (module 099 sub_71F5C0): director state 19 - at step 11 releases the sound voice
	// slot (sub_4A2940(director data +0x52)), finishes, next state.
	uint32_t __cdecl b9_71F5C0(uint32_t a1)
	{
		if (a_73B7E0(0xB) != 0)
		{
			const uint32_t d = MEM<uint32_t>(0x1529FA0);
			x::sub_4A2940((uint32_t)(int32_t)S16(d, 0x52));
			b91_finish(a1);
		}
		return 0; // void
	}

	// 0x71F620 (module 099 sub_71F620): director phase spawner - spawns the tasks of the new
	// phase (director data +0x40, 2..10): particle models (b9_718960) from the asset table
	// [0x256D6F0], helper tasks (queues 0x2565C20 / 0x2569338); phase 10: creature actor + two
	// 0x40-byte tasks (queue 0x256C470: 0x71F880 key 0x152AEF0 / 8, 0x8DDC30 key 0x152AF04 / 9).
	uint32_t __cdecl b9_71F620(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t d = MEM<uint32_t>(0x1529FA0);
		const int32_t sel = (int32_t)S16(d, 0x40) - 2;
		switch (sel)  // jump table 0x71F85C (9 entries, phases 3 and 6 empty)
		{
		case 0:
		{
			const uint32_t tab = MEM<uint32_t>(0x256D6F0);
			b9_718960(node, 0x71F950, U32(tab, 0x128), 0x44C, 1, 0);
			return x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x721010, 0xB0, node);
		}
		case 2:
		{
			const uint32_t tab = MEM<uint32_t>(0x256D6F0);
			return b9_718960(node, 0x71FDC0, U32(tab, 0x12C), 0x170, 2, 0);
		}
		case 3:
		{
			const uint32_t tab = MEM<uint32_t>(0x256D6F0);
			b9_718960(node, 0x71FEC0, U32(tab, 0x130), 0xB0, 3, 0);
			x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x721100, 0xB0, node);
			x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x720500, 0xB0, node);
			return x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x720790, 0xB0, node);
		}
		case 5:
			return x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x7215A0, 0xB0, node);
		case 6:
		{
			x::Effect_AddTaskAndInitFromCtx(0x2569338, 0x7211D0, 0x58, node);
			x::Effect_AddTaskAndInitFromCtx(0x2569338, 0x721390, 0x58, node);
			uint32_t tab = MEM<uint32_t>(0x256D6F0);
			b9_718960(node, 0x71FFB0, U32(tab, 0x134), 0x2C, 4, 0);
			tab = MEM<uint32_t>(0x256D6F0);
			b9_718960(node, 0x7201F0, U32(tab, 0x138), 0x194, 5, 0);
			tab = MEM<uint32_t>(0x256D6F0);
			b9_718960(node, 0x720320, U32(tab, 0x13C), 0x30, 6, 0);
			tab = MEM<uint32_t>(0x256D6F0);
			return b9_718960(node, 0x720410, U32(tab, 0x140), 0x1E8, 7, 0);
		}
		case 7:
			return x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x721460, 0xB0, node);
		case 8:
		{
			const uint32_t tab = MEM<uint32_t>(0x256D6F0);
			a_73C100(node, 0x718E70, U32(tab, 0x108), 4, 0x2D, 2);
			const uint32_t t1 = x::Effect_AddTaskAndInitFromCtx(0x256C470, 0x71F880, 0x40, node);
			U32(t1, 0x34) = 0x152AEF0;
			U16(t1, 0x38) = 8;
			const uint32_t t2 = x::Effect_AddTaskAndInitFromCtx(0x256C470, 0x8DDC30, 0x40, node);
			U32(t2, 0x34) = 0x152AF04;
			U16(t2, 0x38) = 9;
			return t2;
		}
		default:  // phases 3, 6 and out of range: nothing
			return (uint32_t)sel;
		}
	}

	// 0x71FF20 (module 099 sub_71FF20): particle state 0 - decodes the prim layout (model +0x74,
	// index +0x78) into +0x94, position 0, scale 0x1000^3, plays the first frame (b_72A720), next
	// state.
	uint32_t __cdecl b9_71FF20(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U16(node, 0x1C) = 0;
		U16(node, 0x1E) = 0;
		U16(node, 0x20) = 0;
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		b_72A720(node);
		b91_next(node);
		return 0; // void
	}

	// 0x71FF70 (module 099 sub_71FF70): particle state - at step 8 finishes (next state), else
	// plays a frame (b_72A720).
	uint32_t __cdecl b9_71FF70(uint32_t a1)
	{
		if (a_73B7E0(8) != 0)
		{
			b91_finish(a1);
			return 0; // void
		}
		return b_72A720(a1);
	}

	// 0x720020 (module 099 sub_720020): meteor particle state 0 - decodes the prim layout,
	// position (0x1000, -0x1000, 0), velocity (+0x68, +0x6A) = (-0x180, 0x200), scale 0x1000^3,
	// plays the first frame, next state.
	uint32_t __cdecl b9_720020(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U16(node, 0x1C) = 0x1000;
		U16(node, 0x1E) = 0xF000;
		U16(node, 0x20) = 0;
		U16(node, 0x68) = 0xFE80;
		U16(node, 0x6A) = 0x200;
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		b_72A720(node);
		b91_next(node);
		return 0; // void
	}

	// 0x720080 (module 099 sub_720080): meteor fall - saves the previous position (+0x540/+0x544),
	// moves by velocity +0x68/+0x6A/+0x6C, spins +0x64 (-0x40, 12-bit); once y >= 0 (ground):
	// timer +0x84 = 10, next state; else plays a frame and emits the trail (b9_7209B0(node, 10)).
	uint32_t __cdecl b9_720080(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t old_xy = U32(node, 0x1C);
		const uint16_t vx = U16(node, 0x68);
		const uint32_t old_zp = U32(node, 0x20);
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + vx);
		const uint16_t spin = U16(node, 0x64);
		U32(node, 0x540) = old_xy;
		const uint16_t vy = U16(node, 0x6A);
		U32(node, 0x544) = old_zp;
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + vy);
		const uint16_t vz = U16(node, 0x6C);
		const int16_t y = S16(node, 0x1E);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + vz);
		U16(node, 0x64) = (uint16_t)((uint16_t)(spin - 0x40) & 0xFFF);
		if (y >= 0)
		{
			const uint8_t st = U8(node, 0x29);
			U16(node, 0x84) = 0xA;
			U8(node, 0x29) = (uint8_t)(st + 1);
			return 0; // void
		}
		b_72A720(node);
		return b9_7209B0(node, 0xA);
	}

	// 0x7200F0 (module 099 sub_7200F0): meteor impact wait - counts +0x84 down; at 0 sets the
	// position (x = -5 * vx, y = 0x600 - 5 * vy) for the bounce, next state.
	uint32_t __cdecl b9_7200F0(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x84) = (uint16_t)(U16(node, 0x84) - 1);
		if (S16(node, 0x84) <= 0)
		{
			const uint16_t nx = (uint16_t)(U16(node, 0x68) * 0xFFFBu);  // imul cx, cx, -5
			const uint16_t vy5 = (uint16_t)(U16(node, 0x6A) * 5u);      // imul dx, dx, 5
			U16(node, 0x1C) = nx;
			U16(node, 0x84) = 0;
			U16(node, 0x1E) = (uint16_t)(0x600u - vy5);
			b91_next(node);
		}
		return 0; // void
	}

	// 0x720140 (module 099 sub_720140): meteor bounce - ++timer +0x84 (ticks 4..8 emit sparks
	// b9_720D70(node, 10)), moves like b9_720080; at step 10 finishes (next state), else plays a
	// frame and emits the trail (b9_7209B0(node, 10)).
	uint32_t __cdecl b9_720140(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x84) = (uint16_t)(U16(node, 0x84) + 1);
		const int16_t t = S16(node, 0x84);
		if (t >= 4 && t <= 8)
			b9_720D70(node, 0xA);
		const uint16_t vx = U16(node, 0x68);
		const uint32_t old_xy = U32(node, 0x1C);
		const uint32_t old_zp = U32(node, 0x20);
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + vx);
		const uint16_t spin = U16(node, 0x64);
		U32(node, 0x540) = old_xy;
		const uint16_t vy = U16(node, 0x6A);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + vy);
		U32(node, 0x544) = old_zp;
		const uint16_t vz = U16(node, 0x6C);
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + vz);
		U16(node, 0x64) = (uint16_t)((uint16_t)(spin - 0x40) & 0xFFF);
		if (a_73B7E0(0xA) != 0)
		{
			b91_finish(node);
			return 0; // void
		}
		b_72A720(node);
		return b9_7209B0(node, 0xA);
	}

	// 0x720250 (module 099 sub_720250): particle state 0 - decodes the prim layout, position
	// (0, 0, 0x360), plays the first frame (b9_720290), next state.
	uint32_t __cdecl b9_720250(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U16(node, 0x1C) = 0;
		U16(node, 0x1E) = 0;
		U16(node, 0x20) = 0x360;
		b9_720290(node);
		b91_next(node);
		return 0; // void
	}

	// 0x720290 (module 099 sub_720290): particle play step (no camera compose): identity matrix
	// (8DD770) with translation = position +0x1C/+0x1E/+0x20, +0x48 = 0x256DA58, runs the
	// prim-model player on +0x94 with draw callback 0x718B90 (paused = [0x2569334]); returns the
	// frames left (0 = finished).
	uint32_t __cdecl b9_720290(uint32_t a1)
	{
		// stack block (0x5C bytes): +0x00 matrix (8DD770), +0x14/+0x18/+0x1C translation,
		// +0x48 = 0x256DA58; the rest is never written (UNINIT in the original, zeroed here; the
		// callback 0x718B90 reads only the matrix, the translation and +0x48)
		alignas(4) uint8_t blk[0x5C] = {};
		const uint32_t L = P(blk);
		x::MAG_022_sub_8DD770(L);
		const uint32_t node = a1;
		U32(L, 0x48) = 0x256DA58;
		S32(L, 0x14) = (int32_t)S16(node, 0x1C);
		S32(L, 0x18) = (int32_t)S16(node, 0x1E);
		const int32_t pz = S16(node, 0x20);
		const uint32_t paused = MEM<uint32_t>(0x2569334);
		S32(L, 0x1C) = pz;
		return prim_play(node + 0x94, 0x718B90, L, paused);
	}

	// 0x7202F0 (module 099 sub_7202F0): particle state - plays a frame (b9_720290); when the
	// model is finished, finishes the task, next state.
	uint32_t __cdecl b9_7202F0(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t left = b9_720290(node);
		if (left == 0)
			b91_finish(node);
		return 0; // void
	}

	// 0x720380 (module 099 sub_720380): particle state 0 - decodes the prim layout, position
	// (0, 0x600, 0), scale 0x1000^3, plays the first frame (b_72A720), next state.
	uint32_t __cdecl b9_720380(uint32_t a1)
	{
		const uint32_t node = a1;
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, U32(node, 0x78));
		U16(node, 0x1C) = 0;
		U16(node, 0x20) = 0;
		U16(node, 0x1E) = 0x600;
		U32(node, 0x58) = 0x1000;
		U32(node, 0x54) = 0x1000;
		U32(node, 0x50) = 0x1000;
		b_72A720(node);
		b91_next(node);
		return 0; // void
	}

	// 0x7203D0 (module 099 sub_7203D0): particle state - at step 10 finishes (next state), else
	// plays a frame (b_72A720).
	uint32_t __cdecl b9_7203D0(uint32_t a1)
	{
		if (a_73B7E0(0xA) != 0)
		{
			b91_finish(a1);
			return 0; // void
		}
		return b_72A720(a1);
	}

	// ====================================================================================
	// part b92
	// ====================================================================================
	// Boko module 099 (ChocoMeteor) - module-only code, part b92 (0x720470 .. 0x721660, raw addresses).
	// Node layout used here (TaskNodeMagicHeal head, 0xB0-byte particle nodes):
	//   +0x1C/+0x1E/+0x20 s16 position (x, y, z), +0x4C sequence/prim data ptr, +0x50 s16/u32 scale,
	//   +0x52 s16 sequence mode, +0x6C + 8*i particle positions (4 entries), +0x8C + 8*i particle
	//   velocities (4 entries), +0xAC s16 countdown, +0xAE s16 script flag to wait for.
	namespace
	{
		// common task tail: frame counter +1, end (release + return 2) when finished and no live child
		inline uint32_t b92_task_end(uint32_t node)
		{
			const uint8_t status = U8(node, 0x26);
			U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
			if ((status & 1) != 0 && U8(node, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(node);
				return 2;
			}
			return 0;
		}

		// C '/' of a sign-extended value by 2^sh (cdq / and / add / sar)
		inline int32_t b92_div(int32_t v, int sh) { return v / (1 << sh); }

		// shared particle integrator of 0x720B90 / 0x720EA0 for entry i: velocity -= velocity/8
		// (x, y, z in turn), position += velocity/16, node position (+0x1C..+0x23) = particle
		inline void b92_particle_step(uint32_t node, int i)
		{
			const uint32_t v = node + 0x8C + 8 * i;  // esi - 2
			const uint32_t p = node + 0x6C + 8 * i;  // esi - 0x22
			for (int k = 0; k < 3; k++)
			{
				const uint16_t c = U16(v, 2 * k);
				U16(v, 2 * k) = (uint16_t)(c - (uint16_t)b92_div((int16_t)c, 3));
			}
			for (int k = 0; k < 3; k++)
			{
				const int32_t d = b92_div(S16(v, 2 * k), 4);
				U16(p, 2 * k) = (uint16_t)(U16(p, 2 * k) + (uint16_t)d);
			}
			U32(node, 0x1C) = U32(p, 0);
			U32(node, 0x20) = U32(p, 4);
		}
	}

	// 0x720470 (module 099 sub_720470): state - decode the prim layout (+0x74 -> +0x94), x = z = 0,
	// y = 0x600, scale 0x1000 on 3 axes, b_72A720 (prim-model play), next state.
	uint32_t __cdecl b9_720470(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U16(a1, 0x1C) = 0;
		U16(a1, 0x20) = 0;
		U16(a1, 0x1E) = 0x600;
		U32(a1, 0x58) = 0x1000;
		U32(a1, 0x54) = 0x1000;
		U32(a1, 0x50) = 0x1000;
		b_72A720(a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;  // void
	}

	// 0x7204C0 (module 099 sub_7204C0): state - once script flag 0xA is set: finished + next state,
	// else b_72A720 (prim-model play).
	uint32_t __cdecl b9_7204C0(uint32_t a1)
	{
		if (a_73B7E0(0xA) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
			return 0;  // void
		}
		b_72A720(a1);
		return 0;  // void
	}

	// 0x720500 (module 099 sub_720500): TASK - state machine {720570, 7205A0, 7205D0, nop}, then
	// b_730C90(node, 0x1800) (sequence draw), task tail.
	uint32_t __cdecl b9_720500(uint32_t a1)
	{
		const uint32_t states[4] = { 0x720570, 0x7205A0, 0x7205D0, 0x720780 };
		callp(states[S8(a1, 0x29)], a1);
		b_730C90(a1, 0x1800);
		return b92_task_end(a1);
	}

	// 0x720570 (module 099 sub_720570): state - hidden (+0x26 bit2), sequence 0x152A460 mode 0xE,
	// y = 0xE000, countdown 0x10, next state.
	uint32_t __cdecl b9_720570(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U8(a1, 0x26) |= 4;
		U32(a1, 0x4C) = 0x152A460;
		U16(a1, 0x52) = 0xE;
		U16(a1, 0x1E) = 0xE000;
		U16(a1, 0xAC) = 0x10;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0;  // void
	}

	// 0x7205A0 (module 099 sub_7205A0): state - countdown +0xAC; at <= 0 unhide (clear bit2), next state.
	uint32_t __cdecl b9_7205A0(uint32_t a1)
	{
		U16(a1, 0xAC) = (uint16_t)(U16(a1, 0xAC) - 1);
		if (S16(a1, 0xAC) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) &= 0xFB;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0;  // void
	}

	// 0x7205D0 (module 099 sub_7205D0): state - when the sequence ends (a_743C20): spawn the
	// prim-model task 0x720610 (b9_718960, layout [0x256D6F0]+0x134, 0x2C, 4, 0), finished, next state.
	uint32_t __cdecl b9_7205D0(uint32_t a1)
	{
		if (a_743C20(a1) != 0)
		{
			const uint32_t asset = U32(MEM<uint32_t>(0x256D6F0), 0x134);
			b9_718960(a1, 0x720610, asset, 0x2C, 4, 0);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;  // void
	}

	// 0x720670 (module 099 sub_720670): state - decode the prim layout, x = z = 0, y = 0xE000,
	// velocity (+0x68, +0x6A) = (-0x180, 0x200), scale 0x100 on 3 axes, b_72A720, next state.
	uint32_t __cdecl b9_720670(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		U16(a1, 0x1C) = 0;
		U16(a1, 0x20) = 0;
		U16(a1, 0x1E) = 0xE000;
		U16(a1, 0x68) = 0xFE80;
		U16(a1, 0x6A) = 0x200;
		U32(a1, 0x58) = 0x100;
		U32(a1, 0x54) = 0x100;
		U32(a1, 0x50) = 0x100;
		b_72A720(a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;  // void
	}

	// 0x7206D0 (module 099 sub_7206D0): state - grow the scale by 0x300 up to 0x1000, remember the
	// previous position (+0x540), move by the velocity (+0x68..+0x6C), spin +0x64 -= 0x40; at
	// script flag 8: finished + next state, else b_72A720 and spawn a trail (b9_7209B0(node, 8)).
	uint32_t __cdecl b9_7206D0(uint32_t a1)
	{
		const int32_t sc = S32(a1, 0x50);
		if (sc < 0x1000)
		{
			const int32_t n = add32(sc, 0x300);
			S32(a1, 0x50) = n;
			if (n >= 0x1000)
				S32(a1, 0x50) = 0x1000;
			const uint32_t v = U32(a1, 0x50);
			U32(a1, 0x58) = v;
			U32(a1, 0x54) = v;
		}
		const uint16_t vx = U16(a1, 0x68);
		const uint32_t oldxy = U32(a1, 0x1C);
		const uint32_t oldz = U32(a1, 0x20);
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + vx);
		const uint16_t rot = U16(a1, 0x64);
		U32(a1, 0x540) = oldxy;
		const uint16_t vy = U16(a1, 0x6A);
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + vy);
		U32(a1, 0x544) = oldz;
		const uint16_t vz = U16(a1, 0x6C);
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + vz);
		U16(a1, 0x64) = (uint16_t)((uint16_t)(rot - 0x40) & 0xFFF);
		if (a_73B7E0(8) != 0)
		{
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0;  // void
		}
		b_72A720(a1);
		b9_7209B0(a1, 8);
		return 0;  // void
	}

	// 0x720790 (module 099 sub_720790): TASK - state machine {7207E0, nop}, task tail (no draw).
	uint32_t __cdecl b9_720790(uint32_t a1)
	{
		const uint32_t states[2] = { 0x7207E0, 0x7209A0 };
		callp(states[S8(a1, 0x29)], a1);
		return b92_task_end(a1);
	}

	// 0x7207E0 (module 099 sub_7207E0): state - each tick spawn 2 sparkle tasks 0x7208D0 (0xB0 B,
	// queue 0x2565C20, random sequence 0x152A70C/7D0/894 mode 6) placed at rot(rand&0xFFF,
	// 0x80..0x2FF) * (0, -0x2400, 0); at script flag 8 finished + next state.
	uint32_t __cdecl b9_7207E0(uint32_t a1)
	{
		alignas(4) int16_t vec[4];     // [esp+0x10]
		alignas(4) uint8_t mat[0x20];  // [esp+0x18]
		for (int n = 2; n != 0; n--)
		{
			uint32_t node = x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x7208D0, 0xB0, a1);
			const int32_t r = (int32_t)x::CrtRand();
			switch (r % 3)
			{
			case 0: U32(node, 0x4C) = 0x152A70C; break;
			case 1: U32(node, 0x4C) = 0x152A7D0; break;
			case 2: U32(node, 0x4C) = 0x152A894; break;
			default: break;
			}
			U16(node, 0x52) = 6;
			x::MAG_022_sub_8DD770(P(mat));
			const uint32_t ry = x::CrtRand() & 0xFFF;
			x::MAG_022_sub_8DD8A0(P(mat), ry);
			const int32_t r2 = (int32_t)x::CrtRand();
			x::sub_8DD7E0(P(mat), (uint32_t)(r2 % 0x280 + 0x80));
			node += 0x1C;
			vec[0] = 0;
			vec[1] = (int16_t)0xDC00;
			vec[2] = 0;
			x::matrixMultiplyVector(P(mat), P(vec), node);
		}
		if (a_73B7E0(8) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0;  // void
	}

	// 0x7208D0 (module 099 sub_7208D0): TASK - state machine {720940, 720960, nop}, then
	// b_730C90(node, 0x1600) (sequence draw), task tail.
	uint32_t __cdecl b9_7208D0(uint32_t a1)
	{
		const uint32_t states[3] = { 0x720940, 0x720960, 0x720990 };
		callp(states[S8(a1, 0x29)], a1);
		b_730C90(a1, 0x1600);
		return b92_task_end(a1);
	}

	// 0x720940 (module 099 sub_720940): state - sequence 0x152A70C mode 6, y = 0xE000, next state.
	uint32_t __cdecl b9_720940(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x152A70C;
		U16(a1, 0x52) = 6;
		U16(a1, 0x1E) = 0xE000;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0;  // void
	}

	// 0x720960 (module 099 sub_720960): state - when the sequence ends or script flag 8 is set:
	// finished + hidden, next state.
	uint32_t __cdecl b9_720960(uint32_t a1)
	{
		if (a_743C20(a1) != 0 || a_73B7E0(8) != 0)
		{
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;  // void
	}

	// 0x7209B0 (module 099 sub_7209B0): spawns a trail task 0x720B90 (0xB0 B, queue 0x2565C20):
	// 4 particles at quarter steps from the parent position towards its previous position (+0x540),
	// each jittered by b9_720AE0(0x100), random velocity in [-0x200, 0x1FF]; wait flag +0xAE = a2.
	uint32_t __cdecl b9_7209B0(uint32_t a1, uint32_t a2)
	{
		const uint32_t node = x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x720B90, 0xB0, a1);
		alignas(4) uint32_t pos[2];  // [esp+0x10]: s16 x, y, z (+ the parent's word +0x22)
		pos[0] = U32(a1, 0x1C);
		pos[1] = U32(a1, 0x20);
		U16(node, 0xAE) = (uint16_t)a2;
		const int32_t dx = (S16(a1, 0x540) - S16(a1, 0x1C)) / 4;
		const int32_t dy = (S16(a1, 0x542) - S16(a1, 0x1E)) / 4;
		const int32_t dz = (S16(a1, 0x544) - S16(a1, 0x20)) / 4;
		const uint16_t dzw = (uint16_t)dz;  // [esp+0x1C]
		uint32_t e = node + 0x8C;
		for (int n = 4; n != 0; n--)
		{
			U32(node, 0x1C) = pos[0];
			U32(node, 0x20) = pos[1];
			b9_720AE0(node, 0x100);
			U32(e, -0x20) = U32(node, 0x1C);
			U32(e, -0x1C) = U32(node, 0x20);
			const uint32_t r0 = x::CrtRand();
			U16(e, 0) = (uint16_t)((r0 & 0x3FF) - 0x200);
			const uint32_t r1 = x::CrtRand();
			U16(e, 2) = (uint16_t)((r1 & 0x3FF) - 0x200);
			const uint32_t r2 = x::CrtRand();
			U16(P(pos), 0) = (uint16_t)(U16(P(pos), 0) + (uint16_t)dx);
			U16(P(pos), 2) = (uint16_t)(U16(P(pos), 2) + (uint16_t)dy);
			U16(e, 4) = (uint16_t)((r2 & 0x3FF) - 0x200);
			U16(P(pos), 4) = (uint16_t)(U16(P(pos), 4) + dzw);
			e += 8;
		}
		return 0;  // void
	}

	// 0x720AE0 (module 099 sub_720AE0): jitters the node position (+0x1C) by
	// rot(rand&0xFFF, rand&0xFFF) * (0, 0, rand % a2) (a2 low word, 0 -> 1).
	uint32_t __cdecl b9_720AE0(uint32_t a1, uint32_t a2)
	{
		uint32_t range = a2;
		if ((uint16_t)range == 0)
			range = 1;
		alignas(4) int16_t vec[4];     // [esp+0x0C]
		alignas(4) int16_t out[4];     // [esp+0x14]
		alignas(4) uint8_t mat[0x20];  // [esp+0x1C]
		const uint32_t ra = x::CrtRand() & 0xFFF;
		const uint32_t rb = x::CrtRand() & 0xFFF;
		x::MAG_022_sub_8DD770(P(mat));
		x::MAG_022_sub_8DD8A0(P(mat), (uint32_t)(int32_t)(int16_t)ra);
		x::sub_8DD7E0(P(mat), (uint32_t)(int32_t)(int16_t)rb);
		vec[0] = 0;
		vec[1] = 0;
		const int32_t r = (int32_t)x::CrtRand();
		vec[2] = (int16_t)(r % (int32_t)(int16_t)range);
		x::matrixMultiplyVector(P(mat), P(vec), P(out));
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + (uint16_t)out[0]);
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + (uint16_t)out[1]);
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + (uint16_t)out[2]);
		return 0;  // void
	}

	// 0x720B90 (module 099 sub_720B90): TASK - state machine {720D00, 720D20, nop}, then for the 4
	// particles: damp velocity (-1/8), move (+v/16), draw the sequence there (b9_720C90); task tail.
	uint32_t __cdecl b9_720B90(uint32_t a1)
	{
		const uint32_t states[3] = { 0x720D00, 0x720D20, 0x720D60 };
		callp(states[S8(a1, 0x29)], a1);
		for (int i = 0; i < 4; i++)
		{
			b92_particle_step(a1, i);
			b9_720C90(a1);
		}
		return b92_task_end(a1);
	}

	// 0x720C90 (module 099 sub_720C90): draw - unless hidden (+0x26 bit2): shadow-rotation camera
	// at the node position (scale 0x1000, angle +0x54), InitEffectSequenceFromData(seq +0x4C,
	// frame +0x50) into the effect OT (+0x44), mode 2; packet cursor 0x1D8E054 advanced.
	uint32_t __cdecl b9_720C90(uint32_t a1)
	{
		if ((U8(a1, 0x26) & 4) != 0)
			return 0;  // void
		const uint32_t blk = x::Field_Alloc(0xB4);
		xm::TransformCameraByShadowRotation(a1 + 0x1C, 0x1000, (uint32_t)(int32_t)S16(a1, 0x54));
		const uint32_t seq = U32(a1, 0x4C);
		const uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		const uint16_t frame = U16(a1, 0x50);
		U32(blk, 0) = seq;
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(blk, 4) = frame;
		U16(blk, 0x24) = 0;
		const uint32_t nc = xm::InitEffectSequenceFromData(blk, ot, 2, cursor);
		MEM<uint32_t>(0x1D8E054) = nc;
		x::Field_Free(0xB4);
		return 0;  // void
	}

	// 0x720D00 (module 099 sub_720D00): state - sequence 0x152A958 mode 0xB, next state.
	uint32_t __cdecl b9_720D00(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x152A958;
		U16(a1, 0x52) = 0xB;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0;  // void
	}

	// 0x720D20 (module 099 sub_720D20): state - when the sequence ends or the script flag
	// +0xAE is set: finished + hidden, next state.
	uint32_t __cdecl b9_720D20(uint32_t a1)
	{
		// a_743C20 returned 0 in eax, so the pushed dword is the zero-extended word +0xAE
		if (a_743C20(a1) != 0 || a_73B7E0(U16(a1, 0xAE)) != 0)
		{
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;  // void
	}

	// 0x720D70 (module 099 sub_720D70): spawns a burst task 0x720EA0 (0xB0 B, queue 0x2565C20)
	// with 4 particles on a ring around the parent position (start angle rand&0xFFF, step 0x400,
	// radius 0x80..0xFF), outward velocity 0x400..0xBFF, up velocity 0x200..0x5FF; wait flag = a2.
	uint32_t __cdecl b9_720D70(uint32_t a1, uint32_t a2)
	{
		uint32_t e = x::Effect_AddTaskAndInitFromCtx(0x2565C20, 0x720EA0, 0xB0, a1);
		U16(e, 0xAE) = (uint16_t)a2;
		uint32_t angle = x::CrtRand() & 0xFFF;  // [esp+0x1C]
		e += 0x6C;
		for (int n = 4; n != 0; n--)
		{
			U32(e, 0) = U32(a1, 0x1C);
			U32(e, 4) = U32(a1, 0x20);
			const uint32_t r0 = x::CrtRand();
			const uint32_t a = (uint32_t)(int32_t)(int16_t)angle;
			const int32_t radius = (int16_t)(uint16_t)((r0 & 0x7F) + 0x80);
			const int32_t s0 = (int32_t)x::computeSin(a);
			U16(e, 0) = (uint16_t)(U16(e, 0) + (uint16_t)(mul32(s0, radius) / 0x1000));
			const int32_t c0 = (int32_t)x::computeCosine(a);
			U16(e, 4) = (uint16_t)(U16(e, 4) + (uint16_t)(mul32(c0, radius) / 0x1000));
			const uint32_t r1 = x::CrtRand();
			const uint32_t spd = (r1 & 0x7FF) + 0x400;
			const uint32_t r2 = x::CrtRand();
			const uint32_t up = (r2 & 0x3FF) + 0x200;
			const int32_t speed = (int16_t)(uint16_t)spd;
			const int32_t s1 = (int32_t)x::computeSin(a);
			U16(e, 0x20) = (uint16_t)(mul32(s1, speed) / 0x1000);
			U16(e, 0x22) = (uint16_t)up;
			const int32_t c1 = (int32_t)x::computeCosine(a);
			e += 8;
			U16(e, 0x1C) = (uint16_t)(mul32(c1, speed) / 0x1000);
			angle = (angle + 0x400) & 0xFFF;
		}
		return 0;  // void
	}

	// 0x720EA0 (module 099 sub_720EA0): TASK - state machine {720FA0, 720FC0, nop}, then for the 4
	// particles: damp velocity (-1/8), move (+v/16), draw via b_730C90(node, 0x2000); task tail.
	uint32_t __cdecl b9_720EA0(uint32_t a1)
	{
		const uint32_t states[3] = { 0x720FA0, 0x720FC0, 0x721000 };
		callp(states[S8(a1, 0x29)], a1);
		for (int i = 0; i < 4; i++)
		{
			b92_particle_step(a1, i);
			b_730C90(a1, 0x2000);
		}
		return b92_task_end(a1);
	}

	// 0x720FA0 (module 099 sub_720FA0): state - sequence 0x152A31C mode 0xB, next state.
	uint32_t __cdecl b9_720FA0(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x152A31C;
		U16(a1, 0x52) = 0xB;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0;  // void
	}

	// 0x720FC0 (module 099 sub_720FC0): state - when the sequence ends or the script flag +0xAE
	// is set: finished + hidden, next state.
	uint32_t __cdecl b9_720FC0(uint32_t a1)
	{
		// a_743C20 returned 0 in eax, so the pushed dword is the zero-extended word +0xAE
		if (a_743C20(a1) != 0 || a_73B7E0(U16(a1, 0xAE)) != 0)
		{
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0;  // void
	}

	// 0x7211D0 (module 099 sub_7211D0): TASK - state machine {721320, 721360, nop}, then
	// a_7458E0 (model draw), task tail.
	uint32_t __cdecl b9_7211D0(uint32_t a1)
	{
		const uint32_t states[3] = { 0x721320, 0x721360, 0x721380 };
		callp(states[S8(a1, 0x29)], a1);
		a_7458E0(a1);
		return b92_task_end(a1);
	}

	// 0x721320 (module 099 sub_721320): state - model [0x256D6F0]+0x144, scale (0x1000, 0x2000,
	// 0x1000), y = 0x1400, z = 0x2000, next state.
	uint32_t __cdecl b9_721320(uint32_t a1)
	{
		const uint32_t model = U32(MEM<uint32_t>(0x256D6F0), 0x144);
		U32(a1, 0x4C) = model;
		U32(a1, 0x30) = 0x1000;
		U32(a1, 0x38) = 0x1000;
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1E) = 0x1400;
		U16(a1, 0x20) = 0x2000;
		U32(a1, 0x34) = 0x2000;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0;  // void
	}

	// 0x721360 (module 099 sub_721360): state - at script flag 0xA: finished + next state.
	uint32_t __cdecl b9_721360(uint32_t a1)
	{
		if (a_73B7E0(0xA) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0;  // void
	}

	// 0x721390 (module 099 sub_721390): TASK - state machine {7213F0, 721430, nop}, then
	// a_7458E0 (model draw), task tail.
	uint32_t __cdecl b9_721390(uint32_t a1)
	{
		const uint32_t states[3] = { 0x7213F0, 0x721430, 0x721450 };
		callp(states[S8(a1, 0x29)], a1);
		a_7458E0(a1);
		return b92_task_end(a1);
	}

	// 0x7213F0 (module 099 sub_7213F0): state - model [0x256D6F0]+0x148, scale 0x1000 on 3 axes,
	// y = 0, countdown +0x54 = 0x10, next state.
	uint32_t __cdecl b9_7213F0(uint32_t a1)
	{
		const uint32_t model = U32(MEM<uint32_t>(0x256D6F0), 0x148);
		U32(a1, 0x4C) = model;
		U32(a1, 0x38) = 0x1000;
		U32(a1, 0x34) = 0x1000;
		U32(a1, 0x30) = 0x1000;
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1E) = 0;
		U16(a1, 0x54) = 0x10;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0;  // void
	}

	// 0x721430 (module 099 sub_721430): state - countdown +0x54; at <= 0 finished + next state.
	uint32_t __cdecl b9_721430(uint32_t a1)
	{
		U16(a1, 0x54) = (uint16_t)(U16(a1, 0x54) - 1);
		if (S16(a1, 0x54) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0;  // void
	}

	// 0x7214C0 (module 099 sub_7214C0): state - wait for script flag 0xA (a_73B640), next state.
	uint32_t __cdecl b9_7214C0(uint32_t a1)
	{
		if (a_73B640(0xA) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;  // void
	}

	// 0x7214E0 (module 099 sub_7214E0): state - at script flag 0xA: a_746AE0 + a_73BA90, play SE
	// BdPlaySE(0x1529FB0, 0, 0x80), finished + next state.
	uint32_t __cdecl b9_7214E0(uint32_t a1)
	{
		if (a_73B7E0(0xA) != 0)
		{
			a_746AE0();
			a_73BA90();
			x::BdPlaySE(0x1529FB0, 0, 0x80);
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0;  // void
	}

	// 0x721610 (module 099 sub_721610): state - wait for script flag 8 (a_73B640), next state.
	uint32_t __cdecl b9_721610(uint32_t a1)
	{
		if (a_73B640(8) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;  // void
	}

	// 0x721630 (module 099 sub_721630): state - at script flag 8: countdown +0xAC = 0x22, next state.
	uint32_t __cdecl b9_721630(uint32_t a1)
	{
		if (a_73B7E0(8) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0xAC) = 0x22;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0;  // void
	}

	// 0x721660 (module 099 sub_721660): state - countdown +0xAC; at <= 0 next state.
	uint32_t __cdecl b9_721660(uint32_t a1)
	{
		U16(a1, 0xAC) = (uint16_t)(U16(a1, 0xAC) - 1);
		if (S16(a1, 0xAC) <= 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0;  // void
	}

	// ====================================================================================
	// part b101
	// ====================================================================================
	// ChocoBocle (effect 100) module-only code, part b101: the primitive-model renderer of the module
	// with a depth bias (5th argument) and a 320x216 screen clip. b10_70D530 runs the eight primitive
	// lists of a model draw context; this part holds the list renderers for flat / textured / gouraud
	// triangles and quads (the textured-gouraud lists 0x70E580 / 0x70E800 are in another part).
	//
	// Draw context (as in a_7355E0): +0 model, +4 vertices, +8..+0xA colour, +0xC depth-cue colour,
	// +0x10 / +0x14 tpage / CLUT override, +0x18 texture offset, +0x1C flags, +0x20 list cursor,
	// +0x24 MAC0 scratch, +0x2C OTZ scratch, +0x30 GTE FLAG scratch.
	namespace
	{
		// screen clip test of the renderers of this module: outside 0..hi (signed 16-bit)
		inline bool b101_out(int16_t v, int16_t hi) { return v < 0 || v > hi; }

		// common tail of every renderer: OTZ (ctx+0x2C) += (int16)a5, clamped to >= 0x10,
		// InsertPrim at OT a2 [otz >> a3]
		inline void b101_insert(uint32_t ctx, uint32_t a2, uint32_t a3, uint32_t a5, uint32_t pkt)
		{
			int32_t z = add32(S32(ctx, 0x2C), (int32_t)(int16_t)a5);
			S32(ctx, 0x2C) = z;
			if (z < 0x10)
				S32(ctx, 0x2C) = 0x10;
			int32_t sz = S32(ctx, 0x2C) >> (a3 & 31);
			x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)sz * 4, pkt);
		}
	}

	// 0x70D530 (module 100 sub_70D530): renders a primitive model through its draw context a1:
	// sets the GTE colour, then runs the 8 primitive-list renderers in order (F3, F4, FT3, FT4, G3,
	// G4, GT3, GT4; an empty list is skipped), threading the packet cursor a4; returns the cursor
	uint32_t __cdecl b10_70D530(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		uint32_t flags = U32(a1, 0x1C);
		if (!(flags & 0x2000))
			U32(a1, 4) = U32(a1, 0) + 8;
		uint32_t model = U32(a1, 0);
		U32(a1, 0x20) = U32(model, 0) + model;
		if (!(flags & 0x1000))
			U32(a1, 0x18) = 0;
		{
			uint32_t b = U8(a1, 0xA);
			uint32_t g = U8(a1, 9);
			uint32_t r = U8(a1, 8);
			x::someCameraWork_45DD60(r, g, b);
		}
		uint32_t cur = a4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = b10_70D680(a1, a2, a3, cur, a5);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = b10_70D8A0(a1, a2, a3, cur, a5);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = b10_70DB40(a1, a2, a3, cur, a5);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = b10_70DDA0(a1, a2, a3, cur, a5);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = b10_70E060(a1, a2, a3, cur, a5);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = b10_70E2A0(a1, a2, a3, cur, a5);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = b10_70E580(a1, a2, a3, cur, a5);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			return b10_70E800(a1, a2, a3, cur, a5);
		U32(a1, 0x20) = U32(a1, 0x20) + 4;
		return cur;
	}

	// 0x70D680 (module 100 sub_70D680): renders the flat-triangle list (POLY_F3, 0xC-byte records
	// {colour+code, v0, v1, v2, pad} -> 0x14-byte packets): RTPT, semi-trans from flags bit0/bit2,
	// GTE-flag / backface (MAC0, flag 0x10 = two-sided) / 320x216 clip rejection, optional depth-cue
	// colour (flag 0x40), OTZ + bias a5 (>= 0x10), InsertPrim at OT a2 [otz >> a3]; returns the cursor
	uint32_t __cdecl b10_70D680(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t list = U32(ctx, 0x20);
		int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		U32(ctx, 0x20) = rec;
		uint32_t vtx = U32(ctx, 4);
		if (count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		int32_t n = count;
		do
		{
			x::GTE_LoadV012(vtx + (uint32_t)U16(rec, 4) * 4, vtx + (uint32_t)U16(rec, 6) * 4, vtx + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t fl = U32(ctx, 0x1C);
			uint32_t w0 = U32(rec, 0);
			U32(pkt, 0) = 0x4000000;
			U32(pkt, 4) = w0;
			if (fl & 1)
				U32(pkt, 4) = w0 | 0x2000000;
			if (fl & 4)
				U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t zero = 0; // [esp+0x18]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0xC, pkt + 0x10);
					x::GTE_AVSZ3();
					uint32_t clip = b101_out(S16(pkt, 8), 0x140) ? 1 : zero;
					if (b101_out(S16(pkt, 0xC), 0x140)) clip |= 2;
					if (b101_out(S16(pkt, 0x10), 0x140)) clip |= 4;
					if (b101_out(S16(pkt, 0xA), 0xD8)) clip |= 0x10;
					if (b101_out(S16(pkt, 0xE), 0xD8)) clip |= 0x20;
					if (b101_out(S16(pkt, 0x12), 0xD8)) clip |= 0x40;
					if ((clip & 7) != 7 && (clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x40)
						{
							x::set_unk_1CA8A28(pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						b101_insert(ctx, a2, a3, a5, pkt);
						pkt += 0x14;
					}
				}
			}
			rec += 0xC;
		} while (--n != 0);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x70D8A0 (module 100 sub_70D8A0): renders the flat-quad list (POLY_F4, 0xC-byte records
	// {colour+code, v0, v1, v2, v3} -> 0x18-byte packets): RTPT + RTPS, semi-trans from flags
	// bit0/bit2, GTE-flag / backface (flag 0x10 = two-sided) / 320x216 clip rejection, optional
	// depth-cue colour (flag 0x40), OTZ + bias a5 (>= 0x10), InsertPrim at OT a2 [otz >> a3];
	// returns the cursor
	uint32_t __cdecl b10_70D8A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t list = U32(ctx, 0x20);
		int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		U32(ctx, 0x20) = rec;
		uint32_t vtx = U32(ctx, 4);
		if (count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		int32_t n = count;
		do
		{
			x::GTE_LoadV012(vtx + (uint32_t)U16(rec, 4) * 4, vtx + (uint32_t)U16(rec, 6) * 4, vtx + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t fl = U32(ctx, 0x1C);
			uint32_t w0 = U32(rec, 0);
			U32(pkt, 0) = 0x5000000;
			U32(pkt, 4) = w0;
			if (fl & 1)
				U32(pkt, 4) = w0 | 0x2000000;
			if (fl & 4)
				U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0; // [esp+0x10]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0xC, pkt + 0x10);
					x::GTE_LoadV0(vtx + (uint32_t)U16(rec, 0xA) * 4);
					x::GTE_RTPS();
					if (b101_out(S16(pkt, 8), 0x140)) clip = 1;
					if (b101_out(S16(pkt, 0xC), 0x140)) clip |= 2;
					if (b101_out(S16(pkt, 0x10), 0x140)) clip |= 4;
					if (b101_out(S16(pkt, 0xA), 0xD8)) clip |= 0x10;
					if (b101_out(S16(pkt, 0xE), 0xD8)) clip |= 0x20;
					if (b101_out(S16(pkt, 0x12), 0xD8)) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x14);
					x::GTE_AVSZ4();
					if (b101_out(S16(pkt, 0x14), 0x140)) clip |= 8;
					if (b101_out(S16(pkt, 0x16), 0xD8)) clip |= 0x80;
					if ((clip & 0xF) != 0xF && (clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x40)
						{
							x::set_unk_1CA8A28(pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						b101_insert(ctx, a2, a3, a5, pkt);
						pkt += 0x18;
					}
				}
			}
			rec += 0xC;
		} while (--n != 0);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x70DB40 (module 100 sub_70DB40): renders the textured-triangle list (POLY_FT3, 0x14-byte
	// records -> 0x20-byte packets): RTPT, semi-trans from flags bit0/bit2, texture offset +0x18,
	// GTE-flag rejection, tpage/CLUT override (flags 0x400 add / 0x100 set tpage, 0x800 / 0x200
	// CLUT), backface (flag 0x10 = two-sided) / 320x216 clip rejection, optional depth-cue colour
	// (flag 0x40), OTZ + bias a5 (>= 0x10), InsertPrim at OT a2 [otz >> a3]; returns the cursor
	uint32_t __cdecl b10_70DB40(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t list = U32(ctx, 0x20);
		int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		uint32_t vtx = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if (count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		int32_t n = count;
		do
		{
			x::GTE_LoadV012(vtx + (uint32_t)U16(rec, 4) * 4, vtx + (uint32_t)U16(rec, 6) * 4, vtx + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t fl = U32(ctx, 0x1C);
			uint32_t w0 = U32(rec, 0);
			U32(pkt, 0) = 0x7000000;
			U32(pkt, 4) = w0;
			if (fl & 1)
				U32(pkt, 4) = w0 | 0x2000000;
			if (fl & 4)
				U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFF;
			uint32_t toff = U32(ctx, 0x18);
			uint32_t uv0 = U32(rec, 0xC);
			uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0xC) = uv0 + toff;
			uint32_t uv2 = U32(rec, 8) >> 16;
			U32(pkt, 0x14) = uv1 + toff;
			U32(pkt, 0x1C) = uv2 + toff;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t f2 = U32(ctx, 0x1C);
				if (f2 & 0x400)
					U16(pkt, 0x16) = (uint16_t)(U16(pkt, 0x16) + U16(ctx, 0x10));
				else if (f2 & 0x100)
					U16(pkt, 0x16) = U16(ctx, 0x10);
				if (f2 & 0x800)
					U16(pkt, 0xE) = (uint16_t)(U16(pkt, 0xE) + U16(ctx, 0x14));
				else if (f2 & 0x200)
					U16(pkt, 0xE) = U16(ctx, 0x14);
				uint32_t zero = 0; // [esp+0x14]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_AVSZ3();
					uint32_t clip = b101_out(S16(pkt, 8), 0x140) ? 1 : zero;
					if (b101_out(S16(pkt, 0x10), 0x140)) clip |= 2;
					if (b101_out(S16(pkt, 0x18), 0x140)) clip |= 4;
					if (b101_out(S16(pkt, 0xA), 0xD8)) clip |= 0x10;
					if (b101_out(S16(pkt, 0x12), 0xD8)) clip |= 0x20;
					if (b101_out(S16(pkt, 0x1A), 0xD8)) clip |= 0x40;
					if ((clip & 7) != 7 && (clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x40)
						{
							x::set_unk_1CA8A28(pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						b101_insert(ctx, a2, a3, a5, pkt);
						pkt += 0x20;
					}
				}
			}
			rec += 0x14;
		} while (--n != 0);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x70DDA0 (module 100 sub_70DDA0): renders the textured-quad list (POLY_FT4, 0x18-byte
	// records -> 0x28-byte packets): RTPT + RTPS, semi-trans from flags bit0/bit2, texture offset
	// +0x18, GTE-flag rejection, tpage/CLUT override, backface (flag 0x10 = two-sided) / 320x216
	// clip rejection, optional depth-cue colour (flag 0x40), OTZ + bias a5 (>= 0x10), InsertPrim
	// at OT a2 [otz >> a3]; returns the cursor
	uint32_t __cdecl b10_70DDA0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t list = U32(ctx, 0x20);
		int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		uint32_t vtx = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if (count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		int32_t n = count;
		do
		{
			x::GTE_LoadV012(vtx + (uint32_t)U16(rec, 4) * 4, vtx + (uint32_t)U16(rec, 6) * 4, vtx + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t fl = U32(ctx, 0x1C);
			uint32_t w0 = U32(rec, 0);
			U32(pkt, 0) = 0x9000000;
			U32(pkt, 4) = w0;
			if (fl & 1)
				U32(pkt, 4) = w0 | 0x2000000;
			if (fl & 4)
				U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFF;
			uint32_t toff = U32(ctx, 0x18);
			uint32_t uv0 = U32(rec, 0xC);
			uint32_t uv1 = U32(rec, 0x10);
			U32(pkt, 0xC) = uv0 + toff;
			uint32_t w1c = (toff << 16) + toff;
			uv1 += toff;
			w1c += U32(rec, 0x14);
			U32(pkt, 0x1C) = w1c;
			U32(pkt, 0x14) = uv1;
			U32(pkt, 0x24) = w1c >> 16;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t f2 = U32(ctx, 0x1C);
				if (f2 & 0x400)
					U16(pkt, 0x16) = (uint16_t)(U16(pkt, 0x16) + U16(ctx, 0x10));
				else if (f2 & 0x100)
					U16(pkt, 0x16) = U16(ctx, 0x10);
				if (f2 & 0x800)
					U16(pkt, 0xE) = (uint16_t)(U16(pkt, 0xE) + U16(ctx, 0x14));
				else if (f2 & 0x200)
					U16(pkt, 0xE) = U16(ctx, 0x14);
				uint32_t zero = 0; // [esp+0x14]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_LoadV0(vtx + (uint32_t)U16(rec, 0xA) * 4);
					x::GTE_RTPS();
					uint32_t clip = b101_out(S16(pkt, 8), 0x140) ? 1 : zero;
					if (b101_out(S16(pkt, 0x10), 0x140)) clip |= 2;
					if (b101_out(S16(pkt, 0x18), 0x140)) clip |= 4;
					if (b101_out(S16(pkt, 0xA), 0xD8)) clip |= 0x10;
					if (b101_out(S16(pkt, 0x12), 0xD8)) clip |= 0x20;
					if (b101_out(S16(pkt, 0x1A), 0xD8)) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					if (b101_out(S16(pkt, 0x20), 0x140)) clip |= 8;
					if (b101_out(S16(pkt, 0x22), 0xD8)) clip |= 0x80;
					if ((clip & 0xF) != 0xF && (clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x40)
						{
							x::set_unk_1CA8A28(pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						b101_insert(ctx, a2, a3, a5, pkt);
						pkt += 0x28;
					}
				}
			}
			rec += 0x18;
		} while (--n != 0);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x70E060 (module 100 sub_70E060): renders the gouraud-triangle list (POLY_G3, 0x14-byte
	// records {c0+code, v0, v1, v2, pad, c1, c2} -> 0x1C-byte packets): RTPT, semi-trans from
	// flags bit1/bit3, GTE-flag / backface (flag 0x20 = two-sided) / 320x216 clip rejection,
	// colours depth-cued through the GTE when flag 0x80 (else c1/c2 copied), OTZ + bias a5
	// (>= 0x10), InsertPrim at OT a2 [otz >> a3]; returns the cursor
	uint32_t __cdecl b10_70E060(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t list = U32(ctx, 0x20);
		int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		uint32_t vtx = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if (count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		int32_t n = count;
		do
		{
			x::GTE_LoadV012(vtx + (uint32_t)U16(rec, 4) * 4, vtx + (uint32_t)U16(rec, 6) * 4, vtx + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t fl = U32(ctx, 0x1C);
			uint32_t w0 = U32(rec, 0);
			U32(pkt, 0) = 0x6000000;
			U32(pkt, 4) = w0;
			if (fl & 2)
				U32(pkt, 4) = w0 | 0x2000000;
			if (fl & 8)
				U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t zero = 0; // [esp+0x18]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_AVSZ3();
					uint32_t clip = b101_out(S16(pkt, 8), 0x140) ? 1 : zero;
					if (b101_out(S16(pkt, 0x10), 0x140)) clip |= 2;
					if (b101_out(S16(pkt, 0x18), 0x140)) clip |= 4;
					if (b101_out(S16(pkt, 0xA), 0xD8)) clip |= 0x10;
					if (b101_out(S16(pkt, 0x12), 0xD8)) clip |= 0x20;
					if (b101_out(S16(pkt, 0x1A), 0xD8)) clip |= 0x40;
					if ((clip & 7) != 7 && (clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0xC, rec + 0x10, pkt + 4);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0xC, pkt + 0x14, pkt + 4);
						}
						else
						{
							U32(pkt, 0xC) = U32(rec, 0xC);
							U32(pkt, 0x14) = U32(rec, 0x10);
						}
						b101_insert(ctx, a2, a3, a5, pkt);
						pkt += 0x1C;
					}
				}
			}
			rec += 0x14;
		} while (--n != 0);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x70E2A0 (module 100 sub_70E2A0): renders the gouraud-quad list (POLY_G4, 0x18-byte records
	// {c0+code, v0, v1, v2, v3, c1, c2, c3} -> 0x24-byte packets): RTPT + RTPS, semi-trans from
	// flags bit1/bit3, GTE-flag / backface (flag 0x20 = two-sided) / 320x216 clip rejection,
	// colours depth-cued through the GTE when flag 0x80 (else c1..c3 copied), OTZ + bias a5
	// (>= 0x10), InsertPrim at OT a2 [otz >> a3]; returns the cursor
	uint32_t __cdecl b10_70E2A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t list = U32(ctx, 0x20);
		int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		uint32_t vtx = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if (count <= 0)
		{
			U32(ctx, 0x20) = rec;
			return pkt;
		}
		int32_t n = count;
		do
		{
			x::GTE_LoadV012(vtx + (uint32_t)U16(rec, 4) * 4, vtx + (uint32_t)U16(rec, 6) * 4, vtx + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t fl = U32(ctx, 0x1C);
			uint32_t w0 = U32(rec, 0);
			U32(pkt, 0) = 0x8000000;
			U32(pkt, 4) = w0;
			if (fl & 2)
				U32(pkt, 4) = w0 | 0x2000000;
			if (fl & 8)
				U32(pkt, 4) = U32(pkt, 4) & 0xFDFFFFFF;
			x::GTE_ReadFLAG(ctx + 0x30);
			if (!(U32(ctx, 0x30) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0; // [esp+0x10]
				x::GTE_ReadMAC0(ctx + 0x24);
				if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_LoadV0(vtx + (uint32_t)U16(rec, 0xA) * 4);
					x::GTE_RTPS();
					if (b101_out(S16(pkt, 8), 0x140)) clip = 1;
					if (b101_out(S16(pkt, 0x10), 0x140)) clip |= 2;
					if (b101_out(S16(pkt, 0x18), 0x140)) clip |= 4;
					if (b101_out(S16(pkt, 0xA), 0xD8)) clip |= 0x10;
					if (b101_out(S16(pkt, 0x12), 0xD8)) clip |= 0x20;
					if (b101_out(S16(pkt, 0x1A), 0xD8)) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					if (b101_out(S16(pkt, 0x20), 0x140)) clip |= 8;
					if (b101_out(S16(pkt, 0x22), 0xD8)) clip |= 0x80;
					if ((clip & 0xF) != 0xF && (clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x2C);
						if (U8(ctx, 0x1C) & 0x80)
						{
							x::sub_45E120(rec + 0xC, rec + 0x10, rec + 0x14);
							x::set_dword_1CA8A30(U32(ctx, 0xC));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0xC, pkt + 0x14, pkt + 0x1C);
							x::set_unk_1CA8A28(pkt + 4);
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						else
						{
							U32(pkt, 0xC) = U32(rec, 0xC);
							U32(pkt, 0x14) = U32(rec, 0x10);
							U32(pkt, 0x1C) = U32(rec, 0x14);
						}
						b101_insert(ctx, a2, a3, a5, pkt);
						pkt += 0x24;
					}
				}
			}
			rec += 0x18;
		} while (--n != 0);
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// ====================================================================================
	// part b102
	// ====================================================================================
	// Part b102: module-only code of ChocoBocle (effect 100), raw addresses.
	// Prim-model mesh drawers (GT3 / GT4 groups of the 0x70D530 group dispatcher), the prim-model
	// player draw callback, the particle director setup, the three state-machine tasks of the
	// module (prim particle director 0x70F370, Boko creature script 0x7148C0, creature actor
	// 0x715540) and their small state handlers.
	namespace
	{
		// common task tail: task ends (2) when finished (status bit0, read before ++frame) and no
		// child alive (+0x28, read after)
		inline uint32_t b102_task_end(uint32_t node, uint8_t status)
		{
			if ((status & 1) != 0 && U8(node, 0x28) == 0)
			{
				x::Effect_ReleaseLinkedTask(node);
				return 2;
			}
			return 0;
		}

		// screen clip test of one projected coordinate (outside 0..lim, signed 16-bit)
		inline bool b102_out(int16_t v, int16_t lim)
		{
			return v < 0 || v > lim;
		}
	}

	// 0x70E580 (module 100 sub_70E580): prim-model group 7 drawer - for each 0x1C-byte record of
	// the header's group list (+0x20) projects 3 vertices (RTPT) and emits a POLY_GT3 (0x28 bytes,
	// tag 0x9000000; semi-trans / uv offset / tpage / clut overrides of header +0x1C), culled by
	// GTE flags, backface (MAC0 < 0 unless flag 0x20) and screen clip; optional lighting (flag 0x80);
	// inserted at OT a2 + (max(OTZ + bias a5, 0x10) >> a3) * 4. Returns the new packet cursor.
	uint32_t __cdecl b10_70E580(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		const uint32_t hdr = a1;
		uint32_t pkt = a4;
		const uint32_t list = U32(hdr, 0x20);
		int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t verts = U32(hdr, 4);   // (kept in the a4 argument slot)
		U32(hdr, 0x20) = rec;
		if (count <= 0)
		{
			U32(hdr, 0x20) = rec;
			return pkt;
		}
		do
		{
			{
				const uint32_t v2 = verts + U16(rec, 8) * 4u;
				const uint32_t v1 = verts + U16(rec, 6) * 4u;
				const uint32_t v0 = verts + U16(rec, 4) * 4u;
				x::GTE_LoadV012(v0, v1, v2);
			}
			x::GTE_RTPT();
			{
				const uint32_t fl = U32(hdr, 0x1C);
				const uint32_t c0 = U32(rec, 0);
				U32(pkt, 0) = 0x9000000;
				U32(pkt, 4) = c0;
				if ((fl & 2) != 0)
					U32(pkt, 4) = c0 | 0x2000000;       // semi-transparent
				if ((fl & 8) != 0)
					U32(pkt, 4) &= 0xFDFFFFFF;          // opaque
			}
			{
				const uint32_t uvo = U32(hdr, 0x18);    // uv offset
				U32(pkt, 0xC) = U32(rec, 0xC) + uvo;    // uv0 / clut
				U32(pkt, 0x18) = U32(rec, 0x10) + uvo;  // uv1 / tpage
				U32(pkt, 0x24) = (U32(rec, 8) >> 16) + uvo;  // uv2 (record +0xA)
			}
			x::GTE_ReadFLAG(hdr + 0x30);
			if ((U32(hdr, 0x30) & 0x60000) == 0)
			{
				x::GTE_NCLIP();
				const uint32_t fl = U32(hdr, 0x1C);
				if ((fl & 0x400) != 0)
					U16(pkt, 0x1A) = (uint16_t)(U16(pkt, 0x1A) + U16(hdr, 0x10));   // tpage +=
				else if ((fl & 0x100) != 0)
					U16(pkt, 0x1A) = U16(hdr, 0x10);                               // tpage =
				if ((fl & 0x800) != 0)
					U16(pkt, 0xE) = (uint16_t)(U16(pkt, 0xE) + U16(hdr, 0x14));    // clut +=
				else if ((fl & 0x200) != 0)
					U16(pkt, 0xE) = U16(hdr, 0x14);                                // clut =
				const uint32_t clip_init = 0;   // [esp+0x10]
				x::GTE_ReadMAC0(hdr + 0x24);
				if (S32(hdr, 0x24) >= 0 || (U8(hdr, 0x1C) & 0x20) != 0)
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_AVSZ3();
					uint32_t clip = b102_out(S16(pkt, 8), 0x140) ? 1u : clip_init;
					if (b102_out(S16(pkt, 0x14), 0x140))
						clip |= 2;
					if (b102_out(S16(pkt, 0x20), 0x140))
						clip |= 4;
					if (b102_out(S16(pkt, 0xA), 0xD8))
						clip |= 0x10;
					if (b102_out(S16(pkt, 0x16), 0xD8))
						clip |= 0x20;
					if (b102_out(S16(pkt, 0x22), 0xD8))
						clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(hdr + 0x2C);
						if ((U8(hdr, 0x1C) & 0x80) != 0)
						{
							// lit: colours 1, 2 of the record + packet colour 0
							x::sub_45E120(rec + 0x14, rec + 0x18, pkt + 4);
							x::set_dword_1CA8A30(U32(hdr, 0xC));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 4);
						}
						else
						{
							U32(pkt, 0x10) = U32(rec, 0x14);
							U32(pkt, 0x1C) = U32(rec, 0x18);
						}
						const int32_t z = add32(S32(hdr, 0x2C), (int32_t)(int16_t)a5);
						S32(hdr, 0x2C) = z;
						if (z < 0x10)
							S32(hdr, 0x2C) = 0x10;
						const int32_t zz = S32(hdr, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)zz * 4u, pkt);
						pkt += 0x28;
					}
				}
			}
			rec += 0x1C;
		} while (--count != 0);
		U32(hdr, 0x20) = rec;
		return pkt;
	}

	// 0x70E800 (module 100 sub_70E800): prim-model group 8 drawer - like 0x70E580 for 0x24-byte
	// records of 4 vertices (RTPT + RTPS): emits POLY_GT4 (0x34 bytes, tag 0xC000000), 4-point
	// screen clip, AVSZ4 depth, optional 4-colour lighting (flag 0x80). Returns the new cursor.
	uint32_t __cdecl b10_70E800(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		const uint32_t hdr = a1;
		uint32_t pkt = a4;
		const uint32_t list = U32(hdr, 0x20);
		int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t verts = U32(hdr, 4);   // (kept in the a4 argument slot)
		U32(hdr, 0x20) = rec;
		if (count <= 0)
		{
			U32(hdr, 0x20) = rec;
			return pkt;
		}
		do
		{
			{
				const uint32_t v2 = verts + U16(rec, 8) * 4u;
				const uint32_t v1 = verts + U16(rec, 6) * 4u;
				const uint32_t v0 = verts + U16(rec, 4) * 4u;
				x::GTE_LoadV012(v0, v1, v2);
			}
			x::GTE_RTPT();
			{
				const uint32_t fl = U32(hdr, 0x1C);
				const uint32_t c0 = U32(rec, 0);
				U32(pkt, 0) = 0xC000000;
				U32(pkt, 4) = c0;
				if ((fl & 2) != 0)
					U32(pkt, 4) = c0 | 0x2000000;       // semi-transparent
				if ((fl & 8) != 0)
					U32(pkt, 4) &= 0xFDFFFFFF;          // opaque
			}
			{
				const uint32_t uvo = U32(hdr, 0x18);    // uv offset
				U32(pkt, 0xC) = U32(rec, 0xC) + uvo;    // uv0 / clut
				const uint32_t uv1 = U32(rec, 0x10) + uvo;
				const uint32_t uv23 = (uvo << 16) + uvo + U32(rec, 0x14);  // uv2 | uv3 << 16
				U32(pkt, 0x24) = uv23;                  // uv2 (+ uv3 in the pad half)
				U32(pkt, 0x18) = uv1;                   // uv1 / tpage
				U32(pkt, 0x30) = uv23 >> 16;            // uv3
			}
			x::GTE_ReadFLAG(hdr + 0x30);
			if ((U32(hdr, 0x30) & 0x60000) == 0)
			{
				x::GTE_NCLIP();
				const uint32_t fl = U32(hdr, 0x1C);
				if ((fl & 0x400) != 0)
					U16(pkt, 0x1A) = (uint16_t)(U16(pkt, 0x1A) + U16(hdr, 0x10));   // tpage +=
				else if ((fl & 0x100) != 0)
					U16(pkt, 0x1A) = U16(hdr, 0x10);                               // tpage =
				if ((fl & 0x800) != 0)
					U16(pkt, 0xE) = (uint16_t)(U16(pkt, 0xE) + U16(hdr, 0x14));    // clut +=
				else if ((fl & 0x200) != 0)
					U16(pkt, 0xE) = U16(hdr, 0x14);                                // clut =
				const uint32_t clip_init = 0;   // [esp+0x14]
				x::GTE_ReadMAC0(hdr + 0x24);
				if (S32(hdr, 0x24) >= 0 || (U8(hdr, 0x1C) & 0x20) != 0)
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x14, pkt + 0x20);
					x::GTE_LoadV0(verts + U16(rec, 0xA) * 4u);
					x::GTE_RTPS();
					uint32_t clip = b102_out(S16(pkt, 8), 0x140) ? 1u : clip_init;
					if (b102_out(S16(pkt, 0x14), 0x140))
						clip |= 2;
					if (b102_out(S16(pkt, 0x20), 0x140))
						clip |= 4;
					if (b102_out(S16(pkt, 0xA), 0xD8))
						clip |= 0x10;
					if (b102_out(S16(pkt, 0x16), 0xD8))
						clip |= 0x20;
					if (b102_out(S16(pkt, 0x22), 0xD8))
						clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x2C);
					x::GTE_AVSZ4();
					if (b102_out(S16(pkt, 0x2C), 0x140))
						clip |= 8;
					if (b102_out(S16(pkt, 0x2E), 0xD8))
						clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(hdr + 0x2C);
						if ((U8(hdr, 0x1C) & 0x80) != 0)
						{
							// lit: colours 1..3 of the record, then colour 0 from the packet
							x::sub_45E120(rec + 0x18, rec + 0x1C, rec + 0x20);
							x::set_dword_1CA8A30(U32(hdr, 0xC));
							x::sub_45F4C0();
							x::sub_45E370(pkt + 0x10, pkt + 0x1C, pkt + 0x28);
							x::set_unk_1CA8A28(pkt + 4);
							x::sub_45F270();
							x::set_param_with_dword_1CA8A68(pkt + 4);
						}
						else
						{
							U32(pkt, 0x10) = U32(rec, 0x18);
							U32(pkt, 0x1C) = U32(rec, 0x1C);
							U32(pkt, 0x28) = U32(rec, 0x20);
						}
						const int32_t z = add32(S32(hdr, 0x2C), (int32_t)(int16_t)a5);
						S32(hdr, 0x2C) = z;
						if (z < 0x10)
							S32(hdr, 0x2C) = 0x10;
						const int32_t zz = S32(hdr, 0x2C) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)zz * 4u, pkt);
						pkt += 0x34;
					}
				}
			}
			rec += 0x24;
		} while (--count != 0);
		U32(hdr, 0x20) = rec;
		return pkt;
	}

	// 0x70EDE0 (module 100 MAG_100_sub_70EDE0): master state - director setup (0x70F290 =
	// b_718670, model container b10_70EE30), spawns task 0x70EF30 (0x30, queue 0x2565868) and the
	// prim particle director task 0x70F370 (0x48, queue 0x255EBB0), next state.
	uint32_t __cdecl b10_70EDE0(uint32_t a1)
	{
		const uint32_t node = a1;
		b_718670();   // (node pushed as an unused argument)
		b10_70EE30();
		x::Effect_AddTaskAndInitFromCtx(0x2565868, 0x70EF30, 0x30, node);
		x::Effect_AddTaskAndInitFromCtx(0x255EBB0, 0x70F370, 0x48, node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x70EE30 (module 100 MAG_100_sub_70EE30): initialises the model container 0x25658C0 (0x16C
	// bytes, MAG_007_sub_8DCC00), gives it a 0xA000-byte slice of the arena cursor 0x2565888 (+0x100)
	// and its model/texture pointers (+0x104..+0x160), clears +0x164/+0x166; 0x25642E0 = 0x1529B70.
	uint32_t __cdecl b10_70EE30(void)
	{
		MEM<uint32_t>(0x25642E0) = 0x1529B70;
		MEM<uint32_t>(0x25653D8) = 0x25658C0;
		x::MAG_007_sub_8DCC00(0x25658C0, 0x16C);
		const uint32_t c = MEM<uint32_t>(0x25653D8);
		const uint32_t arena = MEM<uint32_t>(0x2565888);
		U32(c, 0x100) = arena;
		MEM<uint32_t>(0x2565888) = arena + 0xA000;
		const uint32_t dflt = 0x19D5018;
		U32(c, 0x114) = dflt;
		U32(c, 0x118) = 0x19E8678;
		U32(c, 0x11C) = dflt;
		U32(c, 0x120) = dflt;
		U32(c, 0x104) = dflt;
		U32(c, 0x108) = 0x19E3018;
		U32(c, 0x10C) = dflt;
		U32(c, 0x110) = dflt;
		U32(c, 0x124) = dflt;
		U32(c, 0x128) = 0x19EF018;
		U32(c, 0x12C) = 0x19E4970;
		U32(c, 0x130) = 0x19DC678;
		for (int32_t o = 0x134; o <= 0x160; o += 4)
			U32(c, o) = dflt;
		U16(c, 0x164) = 0;
		U16(c, 0x166) = 0;
		return c; // (eax = container pointer at ret)
	}

	// 0x70F370 (module 100 MAG_100_sub_70F370): prim particle director TASK (node 0x48) - script
	// cursor step b10_70F480, then 24-state table on +0x29, ++frame counter; ends when finished.
	uint32_t __cdecl b10_70F370(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[24];
		states[0] = 0x70F4D0;
		states[1] = 0x715410;
		states[2] = 0x715470;
		states[3] = 0x715500;
		states[4] = 0x715E30;
		states[5] = 0x715E50;
		states[6] = 0x715E80;
		states[7] = 0x715EA0;
		states[8] = 0x715EC0;
		states[9] = 0x715EE0;
		states[10] = 0x715F00;
		states[11] = 0x715F20;
		states[12] = 0x715F40;
		states[13] = 0x715F60;
		states[14] = 0x715F80;
		states[15] = 0x715FC0;
		states[16] = 0x715FE0;
		states[17] = 0x716000;
		states[18] = 0x716020;
		states[19] = 0x716040;
		states[20] = 0x716070;
		states[21] = 0x7160A0;
		states[22] = 0x7160C0;
		states[23] = 0x716100;
		b10_70F480(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return b102_task_end(node, status);
	}

	// 0x70F480 (module 100 sub_70F480): script cursor of 0x15297F0 - ++tick (+0x46); on a new
	// requested step (+0x42 != +0x40) takes it, resets the tick and runs b10_716120; on a new
	// pending step (+0x44 != +0x42) takes it (nullsub 0x716110).
	uint32_t __cdecl b10_70F480(uint32_t a1)
	{
		uint32_t d = MEM<uint32_t>(0x15297F0);
		U16(d, 0x46) = (uint16_t)(U16(d, 0x46) + 1);
		uint16_t v = U16(d, 0x42);
		if (U16(d, 0x40) != v)
		{
			U16(d, 0x40) = v;
			U16(d, 0x46) = 0;
			b10_716120(a1);
			d = MEM<uint32_t>(0x15297F0);
		}
		v = U16(d, 0x44);
		if (U16(d, 0x42) != v)
		{
			U16(d, 0x42) = v;
			a_73A6D0();   // 0x716110 nullsub (a1 pushed)
		}
		return 0; // void
	}

	// 0x70F7D0 (module 100 sub_70F7D0): prim-model player draw callback (layout a1, record a2,
	// block a3) - picks the object's vertex frame (lerped by MAG_017_sub_701390), builds its matrix
	// (rotation 0x7015B0/0x701310, parent-rotated or raw position, scale), fills a 0x58-byte
	// Field_Alloc render header (0x2000/0x2030 |0xC0 colour) and draws it into OT base+0x44 with
	// Effect_RenderPrimModel, or with the group dispatcher b10_70D530 when block +0x54 != 0.
	uint32_t __cdecl b10_70F7D0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack (0x48 bytes): +0x00 SVECTOR position, +0x08 scale (3x3 s16 diagonal matrix or 3 x
		// int32), +0x28 Mat4x3 object matrix (+0x3C translation)
		alignas(4) uint8_t loc[0x48] = {};
		const uint32_t L = P(loc);
		const uint32_t M = L + 0x28;
		const uint32_t rec = a2;
		const uint32_t blk = a3;
		if (((uint32_t)(int32_t)S16(rec, 0x1C) | U32(rec, 0x18)) == 0)
			return 0; // void (zero scale: nothing drawn)
		if (S16(rec, 0x24) >= 0x1000 && U32(rec, 0x20) == 0)
			return 0; // void (full alpha with black colour: nothing drawn)

		const uint32_t hdr = x::Field_Alloc(0x58);
		const int32_t model = S16(rec, 2);
		const uint32_t data = U32(a1, 0);
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

		if ((U32(rec, 4) & 0x40000) != 0)
			xm::MAG_063_sub_7015B0(rec + 0x10, M);
		else
			x::MAG_017_sub_701310(rec + 0x10, M);
		const uint16_t vx = U16(rec, 8);
		const uint16_t vy = U16(rec, 0xA);
		const uint16_t vz = U16(rec, 0xC);
		U16(L, 0) = vx;
		U16(L, 2) = vy;
		U16(L, 4) = vz;
		if ((U8(rec, 5) & 2) != 0)
		{
			// flag 0x200: raw position (not rotated by the block matrix)
			S32(M, 0x14) = (int16_t)vx;
			S32(M, 0x18) = (int16_t)vy;
			S32(M, 0x1C) = (int16_t)vz;
		}
		else
		{
			x::GTE_SetRotMatrix(blk);
			x::GTE_LoadV0(L);
			x::GTE_MVMVA_RotV0();
			x::GTE_ReadMAC123(M + 0x14);
			if ((U32(rec, 4) & 0x8000) == 0)
				x::GTE_MatrixMultiply(blk, M);
		}
		{
			const int32_t bx = S32(blk, 0x14);
			const int32_t t0 = S32(M, 0x14);
			const int32_t t1 = S32(M, 0x18);
			S32(M, 0x14) = add32(t0, bx);
			const int32_t by = S32(blk, 0x18);
			S32(M, 0x18) = add32(t1, by);
			const int32_t t2 = S32(M, 0x1C);
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
		const int32_t alpha = S16(rec, 0x24);
		U32(hdr, 0xC) = (uint32_t)alpha;
		if (alpha != 0)
		{
			U32(hdr, 0x1C) |= 0xC0;
			U32(hdr, 8) = U32(rec, 0x20);   // colour
		}
		const uint16_t groups = U16(blk, 0x54);
		const uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		uint32_t next;
		if (groups == 0)
			next = x::Effect_RenderPrimModel(hdr, ot, 2, cursor);
		else
			// quirk 0x70FA5C: `mov bp, [ebp+0x54]` only replaces the low word of ebp (= a3), so the
			// 5th argument is (a3 & 0xFFFF0000) | word +0x54 (the callees use its low word)
			next = b10_70D530(hdr, ot, 2, cursor, (blk & 0xFFFF0000u) | groups);
		MEM<uint32_t>(0x1D8E054) = next;
		x::Field_Free(0x58);
		return 0; // void (prim player callback)
	}

	// 0x7148C0 (module 100 MAG_100_BokoCreatureTask): Boko creature script TASK - 14-state table
	// on +0x29 (camera/script steps), ++frame counter; ends when finished.
	uint32_t __cdecl b10_7148C0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[14];
		states[0] = 0x714970;
		states[1] = 0x715170;
		states[2] = 0x715190;
		states[3] = 0x7151E0;
		states[4] = 0x715210;
		states[5] = 0x715250;
		states[6] = 0x715280;
		states[7] = 0x7152C0;
		states[8] = 0x7152E0;
		states[9] = 0x715330;
		states[10] = 0x715380;
		states[11] = 0x7153B0;
		states[12] = 0x7153D0;
		states[13] = 0x715400;
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return b102_task_end(node, status);
	}

	// 0x715210 (module 100 sub_715210): state - camera step (a_73AE10); at script time 5 starts
	// camera script 0x1529DB0 at the director origin (0x15297F0 +8), next state.
	uint32_t __cdecl b10_715210(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(5) != 0)
		{
			a_73AB20(0x1529DB0, MEM<uint32_t>(0x15297F0) + 8, 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x715250 (module 100 sub_715250): state - at script time 6 starts camera script 0x1529DF8
	// at the director origin, next state (no camera step).
	uint32_t __cdecl b10_715250(uint32_t a1)
	{
		if (a_73AAE0(6) != 0)
		{
			a_73AB20(0x1529DF8, MEM<uint32_t>(0x15297F0) + 8, 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x715280 (module 100 sub_715280): state - camera step; at script time 8 starts camera
	// script 0x1529E40, arms a 9-tick timer (+0x6C), next state.
	uint32_t __cdecl b10_715280(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(8) != 0)
		{
			a_73AB20(0x1529E40, MEM<uint32_t>(0x15297F0) + 8, 0);
			U16(a1, 0x6C) = 9;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7152C0 (module 100 sub_7152C0): state - camera step; counts the +0x6C timer down, next
	// state when it reaches <= 0.
	uint32_t __cdecl b10_7152C0(uint32_t a1)
	{
		a_73AE10();
		U16(a1, 0x6C) = (uint16_t)(U16(a1, 0x6C) - 1);
		if (S16(a1, 0x6C) <= 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7152E0 (module 100 sub_7152E0): state - camera step; next state at script step 9
	// (a_73B640).
	uint32_t __cdecl b10_7152E0(uint32_t a1)
	{
		a_73AE10();
		if (a_73B640(9) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x715330 (module 100 sub_715330): state - camera step; at script time 9 starts camera
	// script 0x1529E88, plays SE 0x1529800 (BdPlaySE(.., 0, 0x80)), next state.
	uint32_t __cdecl b10_715330(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(9) != 0)
		{
			a_73AB20(0x1529E88, MEM<uint32_t>(0x15297F0) + 8, 0);
			x::BdPlaySE(0x1529800, 0, 0x80);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7153B0 (module 100 sub_7153B0): state - camera step; next state at script time 0xA.
	uint32_t __cdecl b10_7153B0(uint32_t a1)
	{
		a_73AE10();
		if (a_73AAE0(0xA) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x715540 (module 100 sub_715540): Boko CREATURE ACTOR task - 11-state table on +0x29
	// (animation control), then unless hidden (+0x26 bit2) draws the model (a_746C10, OT list
	// 0x25642E8) and steps its texture animation (a_739890); ++frame counter; ends when finished.
	uint32_t __cdecl b10_715540(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[11];
		states[0] = 0x7159A0;
		states[1] = 0x715A40;
		states[2] = 0x715A70;
		states[3] = 0x715CD0;
		states[4] = 0x715D40;
		states[5] = 0x715D60;
		states[6] = 0x715D80;
		states[7] = 0x715DB0;
		states[8] = 0x715DD0;
		states[9] = 0x715DF0;
		states[10] = 0x715E20;
		callp(states[S8(node, 0x29)], node);
		if ((U8(node, 0x26) & 4) == 0)
		{
			const uint32_t cursor = MEM<uint32_t>(0x1D8E054);
			MEM<uint32_t>(0x1D8E054) = a_746C10(node, 0x25642E8, cursor);
			a_739890(node);
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return b102_task_end(node, status);
	}

	// 0x715DB0 (module 100 sub_715DB0): creature state - steps the animation; next state once
	// the animation frame (+0x136) reaches 0x10.
	uint32_t __cdecl b10_715DB0(uint32_t a1)
	{
		x::au_re_Battle_ReadAnimation_8(a1);
		if (S16(a1, 0x136) >= 0x10)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x715DD0 (module 100 sub_715DD0): creature state - steps the animation; next state at
	// script step 5 (a_73B640).
	uint32_t __cdecl b10_715DD0(uint32_t a1)
	{
		x::au_re_Battle_ReadAnimation_8(a1);
		if (a_73B640(5) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x715DF0 (module 100 sub_715DF0): creature state - steps the animation; at script step 5
	// (a_73B7E0) hides + finishes the actor (+0x26 |= 5), next state.
	uint32_t __cdecl b10_715DF0(uint32_t a1)
	{
		x::au_re_Battle_ReadAnimation_8(a1);
		if (a_73B7E0(5) != 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 5;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// ====================================================================================
	// part b103
	// ====================================================================================
	// ChocoBocle (effect 100) module-only code, part b103: the director's spawn switch, the
	// "target wobble" actor (task 0x716C20 + its custom target-model renderer 0x716DD0) and the
	// Boko creature state helpers 0x7176B0..0x717A50. Raw module addresses (listings NEW\lst\b10_*.txt).
	namespace
	{
		// battle entity of a node's target slot (+0x2D): 0x1D972C0 + 0x9C * slot
		inline uint32_t b103_entity_of(uint32_t node) { return 0x1D972C0u + (uint32_t)U8(node, 0x2D) * 0x9Cu; }
		inline void b103_next_state(uint32_t node) { U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1); }
	}

	// 0x715F80 (module 100 sub_715F80): state: once the director's step (+0x40) reached 6, copies the
	// frame counter to +0x44, plays sound effect 0x15297FC (BdPlaySE(.., 0, 0x80)) and advances the state
	uint32_t __cdecl b10_715F80(uint32_t a1)
	{
		if (a_73B7E0(6) == 0)
			return 0;
		U16(a1, 0x44) = U16(a1, 0x24);
		x::BdPlaySE(0x15297FC, 0, 0x80);
		b103_next_state(a1);
		return 0; // void
	}

	// 0x716040 (module 100 sub_716040): state: once the director's step reached 8, sets +0x44 = 100
	// and advances the state
	uint32_t __cdecl b10_716040(uint32_t a1)
	{
		if (a_73B7E0(8) == 0)
			return 0;
		U16(a1, 0x44) = 0x64;
		b103_next_state(a1);
		return 0; // void
	}

	// 0x7160C0 (module 100 sub_7160C0): state: at director gate 10 calls sub_4A2940(director +0x52)
	// (sound voice release), finishes the node and advances the state
	uint32_t __cdecl b10_7160C0(uint32_t a1)
	{
		if (a_73B640(0xA) == 0)
			return 0;
		x::sub_4A2940((uint32_t)(int32_t)S16(U32(0x15297F0, 0), 0x52));
		U8(a1, 0x26) |= 1;
		b103_next_state(a1);
		return 0; // void
	}

	// 0x716120 (module 100 sub_716120): director step switch (director +0x40): 2/4/8 spawn the
	// prim-model plays (b_72A610) + their holder tasks, 6 spawns the camera/model actors (a_73C100,
	// 0x7175F0 + anim bind), 9 spawns one "target wobble" actor 0x716C20 per subtarget of the action
	uint32_t __cdecl b10_716120(uint32_t a1)
	{
		uint32_t dir = U32(0x15297F0, 0);
		int32_t step = (int32_t)S16(dir, 0x40);
		uint32_t d;
		uint32_t t;
		switch (step)
		{
		case 2:
			d = U32(0x25653D8, 0);
			b_72A610(a1, 0x7163D0, U32(d, 0x128), 0x44C, 1, 0);
			return x::Effect_AddTaskAndInitFromCtx(0x255ECB8, 0x716A60, 0x70, a1);
		case 4:
			d = U32(0x25653D8, 0);
			b_72A610(a1, 0x716840, U32(d, 0x12C), 0x170, 2, 0);
			return x::Effect_AddTaskAndInitFromCtx(0x255ECB8, 0x716B50, 0x70, a1);
		case 6:
			d = U32(0x25653D8, 0);
			a_73C100(a1, 0x70FAF0, U32(d, 0x108), 0, 0x2D, 2);
			t = x::Effect_AddTaskAndInitFromCtx(0x25656B8, 0x7175F0, 0x140, a1);
			d = U32(0x25653D8, 0);
			return x::Effect_BindModelContainerSetAnim(t, U32(d, 0x118), 0x3C);
		case 8:
			d = U32(0x25653D8, 0);
			b_72A610(a1, 0x716950, U32(d, 0x130), 0x32C, 3, 0);
			t = x::Effect_AddTaskAndInitFromCtx(0x2564158, 0x716300, 0x40, a1);
			d = U32(0x25653D8, 0);
			U32(t, 0x34) = 0x1529F70;
			U16(t, 0x38) = 0xF;
			return a_73C100(a1, 0x70FAF0, U32(d, 0x10C), 0xF, 0x2D, 2);
		case 9:
		{
			// cast context +4 -> 20-byte action records: +0x08 subtarget list (24-byte records,
			// byte 0 = target slot), +0x10 u8 subtarget count
			int32_t act = (int32_t)S8(a1, 0x2A);
			uint32_t acts = U32(U32(a1, 0xC), 4);
			if (U8(acts + (uint32_t)(act * 20), 0x10) == 0)
				return acts;
			int32_t i = 0;
			uint32_t off = 0;
			for (;;)
			{
				t = x::Effect_AddTaskAndInitFromCtx(0x2561890, 0x716C20, 0xD0, a1);
				int32_t a = (int32_t)S8(a1, 0x2A);
				uint32_t rec = U32(U32(a1, 0xC), 4) + (uint32_t)(a * 20);
				i++;
				off += 0x18;
				U8(t, 0x2D) = U8(U32(rec, 8) + off, -0x18);
				uint32_t n = U8(rec, 0x10);
				if (i >= (int32_t)n)
					return n;
			}
		}
		default:
			return (uint32_t)(step - 2);
		}
	}

	// 0x716910 (module 100 sub_716910): state: once the director's step reached 5 finishes the node
	// and advances the state, else runs the prim-play holder step b_72A720
	uint32_t __cdecl b10_716910(uint32_t a1)
	{
		if (a_73B7E0(5) != 0)
		{
			U8(a1, 0x26) |= 1;
			b103_next_state(a1);
			return 0; // void
		}
		return b_72A720(a1);
	}

	// 0x7169B0 (module 100 sub_7169B0): state: decodes the node's prim layout (+0x74 -> +0x94), sets the
	// play parameters (+0x60 = 0x480, +0x58 = 0x2800, scale +0x50/+0x54 = 0x1000, +0x86 = 0xF000),
	// position = director +8/+0xC offset by (+0xA800, +0x400), runs b_72A720, advances the state
	uint32_t __cdecl b10_7169B0(uint32_t a1)
	{
		x::Effect_DecodeModelPrimLayout(U32(a1, 0x74), a1 + 0x94, U32(a1, 0x78));
		uint32_t dir = U32(0x15297F0, 0);
		U16(a1, 0x60) = 0x480;
		U32(a1, 0x58) = 0x2800;
		uint32_t p0 = U32(dir, 8);
		uint32_t p1 = U32(dir, 0xC);
		U32(a1, 0x1C) = p0;
		U32(a1, 0x20) = p1;
		U16(a1, 0x1E) = (uint16_t)(U16(a1, 0x1E) + 0xA800);
		U16(a1, 0x20) = (uint16_t)(U16(a1, 0x20) + 0x400);
		U32(a1, 0x50) = 0x1000;
		U32(a1, 0x54) = 0x1000;
		U16(a1, 0x86) = 0xF000;
		b_72A720(a1);
		b103_next_state(a1);
		return 0; // void
	}

	// 0x716A20 (module 100 sub_716A20): state: once the director's step reached 9 finishes the node
	// and advances the state, else runs the prim-play holder step b_72A720
	uint32_t __cdecl b10_716A20(uint32_t a1)
	{
		if (a_73B7E0(9) != 0)
		{
			U8(a1, 0x26) |= 1;
			b103_next_state(a1);
			return 0; // void
		}
		return b_72A720(a1);
	}

	// 0x716BB0 (module 100 sub_716BB0): state: sets +0x1C = 0x600 and advances the state
	uint32_t __cdecl b10_716BB0(uint32_t a1)
	{
		U16(a1, 0x1C) = 0x600;
		b103_next_state(a1);
		return 0; // void
	}

	// 0x716C20 (module 100 sub_716C20): "target wobble" actor task: runs its state (table below); while
	// flag +0x26 bit3 is set, advances the wobble phase +0xC0 by 0x180 and draws the target's own
	// model through 0x716CD0 (render block node +0x6C); ends when finished and no child is alive
	uint32_t __cdecl b10_716C20(uint32_t a1)
	{
		uint32_t tab[7] = { 0x717330, 0x717350, 0x7174B0, 0x7174D0, 0x717540, 0x717590, 0x7175E0 };
		callp(tab[S8(a1, 0x29)], a1);
		if (U8(a1, 0x26) & 8)
		{
			uint32_t r = a1 + 0x6C;
			U16(r, 0x54) = (uint16_t)((U16(a1, 0xC0) + 0x180) & 0xFFF);
			b10_716CD0(r);
		}
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((U8(a1, 0x26) & 1) && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x716CD0 (module 100 sub_716CD0): draws the target entity (render block a1 = node +0x6C, entity at
	// +0x48): copies the entity's 0x20-byte world matrix (+0x40), builds its bone matrices, composes
	// camera x world into a1 +0x20 (rotation) / +0x34 (translation), draws the entity shadow
	// (sub_5088A0 into OT +0x4040), then the body (entity +0x64) and the weapon (entity +0x78 -> +4)
	// with the wobble renderer 0x716DD0, and rebuilds the bone matrices
	uint32_t __cdecl b10_716CD0(uint32_t a1)
	{
		uint32_t ent = U32(a1, 0x48);
		memcpy((void *)a1, (const void *)(ent + 0x40), 0x20);
		uint32_t pose = ent + 0x60;
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		x::GTE_SetRotMatrix(0x1D97778);
		x::GTE_LoadIRFromMatrixColumn(a1);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(a1 + 0x20);
		x::GTE_LoadIRFromMatrixColumn(a1 + 2);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(a1 + 0x22);
		x::GTE_LoadIRFromMatrixColumn(a1 + 4);
		x::GTE_MVMVA_RotIR();
		x::GTE_StoreIRToMatrixColumn(a1 + 0x24);
		x::GTE_SetTransVector(0x1D97778);
		x::GTE_LoadV0FromDwords(a1 + 0x14);
		x::GTE_MVMVA_RotV0_Tr();
		x::GTE_ReadMAC123(a1 + 0x34);
		uint32_t cp = U32(a1, 0x4C);
		uint32_t ot = U32(0x1D8E04C, 0) + 0x4040;
		uint32_t cur = U32(cp, 0);
		uint32_t r = x::sub_5088A0(ent, ot, 0x10, cur);
		cp = U32(a1, 0x4C);
		uint32_t ot44 = U32(0x1D8E04C, 0) + 0x44;
		U32(cp, 0) = r;
		b10_716DD0(U32(ent, 0x64), ot44, 4, a1);
		uint32_t wpn = U32(ent, 0x78);
		if (wpn != 0)
		{
			uint32_t ot2 = U32(0x1D8E04C, 0) + 0x44;
			b10_716DD0(U32(wpn, 4), ot2, 4, a1);
		}
		x::BattleModel_BuildBoneMatricesFromPose(pose);
		return 0; // void
	}

	// 0x716DD0 (module 100 sub_716DD0): custom renderer of a battle model (a1 = geometry {+0 bone
	// matrices - 0x10, +4 object table}, a2 = OT, a3 = depth shift, a4 = render block): per visible
	// object (entity +0x7C mask) lights every vertex with its bone's light matrix, squashes z (+0x5A)
	// and adds a cosine wobble to z (amplitude +0x56*+0x58, phase +0x54 + +0x5C*y), projects them
	// (RTPS) and emits back-face-culled textured triangles (POLY_FT3, 7 words) and quads (POLY_FT4,
	// 9 words) tinted with the entity colour (+0x28), inserted with SSIGPU_InsertPrimDepthKeys
	uint32_t __cdecl b10_716DD0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		uint32_t node = a4;
		uint32_t verts = U32(U32(node, 0x44), 4);  // vertex scratch: 16 B {x,y,z,?, sxy, sz}
		uint32_t cursor = U32(U32(node, 0x4C), 0); // packet cursor (local +0x18)
		uint32_t tab = U32(a1, 4);                 // object table {count, offsets[count]}
		uint32_t bones = U32(a1, 0) + 0x10;        // local +0x28
		int32_t count = S32(tab, 0);               // local +0x24
		uint32_t ent = U32(node, 0x48);            // local +0x20 (reused as the object index)
		uint32_t w = x::Field_Alloc(0x64);         // scratch block
		tab += 4;
		uint32_t mask = U32(ent, 0x7C);
		uint32_t rgb = U32(ent, 0x28) & 0xFFFFFF;
		U32(w, 0x50) = mask;                        // visible objects
		U32(w, 0x48) = rgb | 0x24000000;            // POLY_FT3 code + colour
		int32_t amp_a = (int32_t)S16(node, 0x56);
		U32(w, 0x4C) = rgb | 0x2C000000;            // POLY_FT4 code + colour
		uint16_t s58 = U16(node, 0x58);
		U16(w, 0x5C) = s58;
		U16(w, 0x5E) = U16(node, 0x5A);             // z squash (4.12)
		U16(w, 0x5A) = (uint16_t)(mul32(amp_a, (int32_t)(int16_t)s58) / 4096); // wobble amplitude
		U16(w, 0x60) = U16(node, 0x5C);             // phase per y unit
		U16(w, 0x56) = U16(node, 0x54);             // base phase
		x::GTE_SetRotMatrix(node + 0x20);
		x::GTE_SetTransVector(node + 0x20);
		for (int32_t i = 0; i < count; i++)
		{
			tab += 4;
			uint32_t wv = verts;                                  // local +0x14
			uint32_t obj = U32(a1, 4) + U32(tab, -4);             // local +0x30
			if (((U32(w, 0x50) >> (i & 31)) & 1) == 0)
				continue;
			int32_t ngroups = (int32_t)S16(obj, 0);
			uint32_t groups = obj + 2;                            // local +0x2C
			uint32_t p = groups;
			// pass 1: light + wobble every vertex into the scratch
			for (int32_t g = ngroups; g > 0; g--)
			{
				int32_t bone = (int32_t)S16(p, 0);
				p += 2;
				uint32_t m = (uint32_t)(bone * 3 * 16) + bones + 0x10;
				xm::GTE_SetLightMatrix(m);
				uint32_t bk0 = U32(m, 0x14);
				uint32_t bk1 = U32(m, 0x18);
				uint32_t bk2 = U32(m, 0x1C);
				xm::GTE_SetBackgroundVector(bk0, bk1, bk2);
				int32_t nv = (int32_t)S16(p, 0);
				p += 2;
				uint32_t v = w + 0x28;
				for (int32_t k = nv; k > 0; k--)
				{
					uint16_t vx = U16(p, 0);
					uint16_t vy = U16(p, 2);
					p += 4;
					U16(v, 0) = vx;
					U16(w, 0x2A) = vy;
					U16(w, 0x2C) = U16(p, 0);
					p += 2;
					x::GTE_LoadV0(v);
					xm::GTE_MVMVA_LightV0_Bk();
					x::GTE_StoreIR123(v);
					uint16_t z = U16(w, 0x2C);
					int32_t sq = ((int32_t)(int16_t)z * (int32_t)S16(w, 0x5E)) / 4096;
					uint16_t ph = (uint16_t)((uint16_t)(U16(w, 0x60) * U16(w, 0x2A)) + U16(w, 0x56));
					U16(w, 0x2C) = (uint16_t)(z - (uint16_t)sq);
					ph &= 0xFFF;
					U16(w, 0x54) = ph;
					int32_t c = (int32_t)x::computeCosine((uint32_t)(int32_t)(int16_t)ph);
					int32_t d = mul32(c, (int32_t)S16(w, 0x5A)) / 32768;
					U16(w, 0x2C) = (uint16_t)(U16(w, 0x2C) + (uint16_t)d);
					uint32_t lo = U32(v, 0);
					uint32_t hi = U32(v, 4);
					U16(w, 0x58) = (uint16_t)d;
					U32(wv, 0) = lo;
					U32(wv, 4) = hi;
					wv += 0x10;
				}
			}
			// pass 2: project every vertex (sxy -> +8, sz -> +0xC)
			uint32_t q = groups;
			uint32_t vp = verts;
			int32_t ng2 = (int32_t)S16(obj, 0);
			for (int32_t g = ng2; g > 0; g--)
			{
				int32_t nv = (int32_t)S16(q, 2);
				q += 4;
				if (nv <= 0)
					continue;
				uint32_t zp = vp + 0xC;
				q += (uint32_t)(nv * 3 * 2);
				for (int32_t k = nv; k > 0; k--)
				{
					uint32_t lo = U32(vp, 0);
					uint32_t hi = U32(vp, 4);
					U32(w, 0x10) = lo;
					U32(w, 0x14) = hi;
					x::GTE_LoadV0(w + 0x10);
					x::GTE_RTPS();
					x::GTE_ReadSXY2(zp - 4);
					xm::sub_45E220(zp);
					vp += 0x10;
					zp += 0x10;
				}
			}
			// faces (4-aligned): u16 tri count, u16 quad count, 8 bytes, then the records
			uint32_t f = (q + 3) & 0xFFFFFFFCu;
			int32_t ntri = (int32_t)S16(f, 0);
			int32_t nquad = (int32_t)S16(f, 2);
			f += 0xC;
			uint32_t pk = cursor;
			// triangles: 16-byte records {u16 i0,i1,i2, uv2, u32 uv0+clut, u32 uv1+tpage}
			for (int32_t k = ntri; k > 0; k--, f += 0x10)
			{
				uint32_t i2 = U16(f, 4) & 0xFFF;
				uint32_t i1 = U16(f, 2) & 0xFFF;
				uint32_t i0 = U16(f, 0) & 0xFFF;
				U32(w, 0x40) = i2;
				U32(w, 0x3C) = i1;
				U32(w, 0x38) = i0;
				uint32_t s2 = U32(verts + (i2 << 4), 8);
				uint32_t s1 = U32(verts + (i1 << 4), 8);
				uint32_t s0 = U32(verts + (i0 << 4), 8);
				U32(w, 0) = s0;
				U32(w, 4) = s1;
				U32(w, 8) = s2;
				xm::sub_45E160(s0, s1, s2);
				x::GTE_NCLIP();
				x::GTE_ReadMAC0(w + 0x34);
				if (S32(w, 0x34) <= 0)
					continue;
				U32(pk, 8) = U32(w, 0);
				uint32_t uv0 = U32(f, 8);
				U32(pk, 0x10) = U32(w, 4);
				uint32_t uv1 = U32(f, 0xC);
				U32(pk, 0x18) = U32(w, 8);
				uint16_t uv2 = U16(f, 6);
				U32(pk, 0xC) = uv0;
				uint32_t code = U32(w, 0x48);
				U32(pk, 0) = 0x7000000;
				bool semi = (U8(f, 0xF) & 2) != 0;
				U32(pk, 0x14) = uv1;
				U16(pk, 0x1C) = uv2;
				U32(pk, 4) = code;
				if (semi)
					U8(pk, 7) |= 2;
				int32_t z2 = S32(verts + (U32(w, 0x40) << 4), 0xC);
				int32_t z1 = S32(verts + (U32(w, 0x3C) << 4), 0xC);
				int32_t z0 = S32(verts + (U32(w, 0x38) << 4), 0xC);
				int32_t sum = add32(add32(z1, z0), z2);
				// sum / 3 (magic 0x55555556: high dword + its sign bit)
				int32_t hq = (int32_t)(((int64_t)sum * 0x55555556LL) >> 32);
				hq = add32(hq, (int32_t)((uint32_t)hq >> 31));
				int32_t key = hq >> (a3 & 31);
				U32(w, 0x30) = (uint32_t)key;
				x::SSIGPU_InsertPrimDepthKeys(a2 + (uint32_t)key * 4, pk, (uint32_t)z0, (uint32_t)z1, (uint32_t)z2, 0);
				pk += 0x20;
			}
			// quads: 20-byte records {u16 i0,i1,i2,i3, u32 uv0+clut, u32 uv1+tpage, u16 uv2, u16 uv3}
			if (nquad > 0)
			{
				f += 4;
				for (int32_t k = nquad; k > 0; k--, f += 0x14)
				{
					uint32_t i2 = U16(f, 0) & 0xFFF;
					uint32_t i1 = U16(f, -2) & 0xFFF;
					uint32_t i0 = U16(f, -4) & 0xFFF;
					U32(w, 0x40) = i2;
					U32(w, 0x3C) = i1;
					U32(w, 0x38) = i0;
					uint32_t s2 = U32(verts + (i2 << 4), 8);
					uint32_t s1 = U32(verts + (i1 << 4), 8);
					uint32_t s0 = U32(verts + (i0 << 4), 8);
					U32(w, 0) = s0;
					U32(w, 4) = s1;
					U32(w, 8) = s2;
					xm::sub_45E160(s0, s1, s2);
					x::GTE_NCLIP();
					x::GTE_ReadMAC0(w + 0x34);
					if (S32(w, 0x34) <= 0)
						continue;
					uint32_t i3 = U16(f, 2) & 0xFFF;
					U32(pk, 8) = U32(w, 0);
					U32(w, 0x44) = i3;
					uint32_t v3 = verts + (i3 << 4);
					U32(pk, 0x10) = U32(w, 4);
					U32(pk, 0) = 0x9000000;
					uint32_t s3 = U32(v3, 8);
					U32(pk, 0x18) = U32(w, 8);
					uint32_t uv1 = U32(f, 8);
					U32(w, 0xC) = s3;
					U32(pk, 0x20) = s3;
					uint32_t uv0 = U32(f, 4);
					bool semi = (U8(f, 0xB) & 2) != 0;
					U32(pk, 0xC) = uv0;
					uint16_t uv2 = U16(f, 0xC);
					U32(pk, 0x14) = uv1;
					uint16_t uv3 = U16(f, 0xE);
					U16(pk, 0x1C) = uv2;
					uint32_t code = U32(w, 0x4C);
					U16(pk, 0x24) = uv3;
					U32(pk, 4) = code;
					if (semi)
						U8(pk, 7) |= 2;
					int32_t z3 = S32(v3, 0xC);
					int32_t z2 = S32(verts + (U32(w, 0x40) << 4), 0xC);
					int32_t z1 = S32(verts + (U32(w, 0x3C) << 4), 0xC);
					int32_t z0 = S32(verts + (U32(w, 0x38) << 4), 0xC);
					int32_t sum = add32(add32(add32(z0, z3), z1), z2);
					int32_t key = sum >> ((a3 + 2) & 31);
					U32(w, 0x30) = (uint32_t)key;
					x::SSIGPU_InsertPrimDepthKeys(a2 + (uint32_t)key * 4, pk, (uint32_t)z0, (uint32_t)z1, (uint32_t)z2, (uint32_t)z3);
					pk += 0x28;
				}
			}
			cursor = pk;
		}
		U32(U32(node, 0x4C), 0) = cursor;
		x::Field_Free(0x64);
		return 0; // void
	}

	// 0x717330 (module 100 sub_717330): wobble state 0: timer +0xCC = 4, next state
	uint32_t __cdecl b10_717330(uint32_t a1)
	{
		U16(a1, 0xCC) = 4;
		b103_next_state(a1);
		return 0; // void
	}

	// 0x717350 (module 100 sub_717350): wobble state 1: when timer +0xCC runs out, sets up the render
	// block (+0x6C, 0x717450) on the target entity, hides the entity's own draw (entity +0 |= 4),
	// enables the custom draw (+0x26 |= 8), wobble amplitude = clamp(entity height +0x3C - +0x36,
	// 0x400, 0x1000), phase step by height (8..2), entity +0xC = 0xC00, timer 45, next state
	uint32_t __cdecl b10_717350(uint32_t a1)
	{
		uint32_t r = a1 + 0x6C;
		uint32_t ent = b103_entity_of(a1);
		U16(a1, 0xCC) = (uint16_t)(U16(a1, 0xCC) - 1);
		if (S16(a1, 0xCC) > 0)
			return 0;
		b10_717450(r, U32(U32(0x25653D8, 0), 0x100), 0x2565890, ent);
		U8(ent, 0) |= 4;
		int16_t h = (int16_t)(U16(ent, 0x3C) - U16(ent, 0x36));
		U16(a1, 0x26) |= 8;
		U16(r, 0x56) = (uint16_t)h;
		if (h <= 0x400)
			U16(r, 0x56) = 0x400;
		else if (h >= 0x1000)
			U16(r, 0x56) = 0x1000;
		U16(r, 0x5C) = (uint16_t)h;
		uint16_t step;
		if (h <= 0x500)
			step = 8;
		else if (h <= 0x700)
			step = 7;
		else if (h <= 0x900)
			step = 6;
		else if (h <= 0xB00)
			step = 5;
		else if (h <= 0xD00)
			step = 4;
		else
			step = (uint16_t)((h <= 0xF00 ? 1 : 0) + 2);
		U16(r, 0x5C) = step;
		U16(r, 0x5A) = 0x1000;
		U16(r, 0x58) = 0;
		U16(ent, 0xC) = 0xC00;
		U16(a1, 0xCC) = 0x2D;
		b103_next_state(a1);
		return 0; // void
	}

	// 0x717450 (module 100 sub_717450): inits a render block a1 (MAG_007_sub_8DCC00(a1, 0x60)):
	// +0x40 = a2, +0x44 = a3 (vertex scratch holder, +4 = a2), +0x48 = entity a4, +0x4C/+0x50 = packet
	// cursor 0x1D8E054; resets holder a3 fields +0x14..+0x24 (0x140, grey 0x80 x3, -1)
	uint32_t __cdecl b10_717450(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		x::MAG_007_sub_8DCC00(a1, 0x60);
		U32(a1, 0x48) = a4;
		U32(a1, 0x40) = a2;
		U32(a1, 0x4C) = 0x1D8E054;
		U32(a1, 0x50) = 0x1D8E054;
		U32(a3, 4) = a2;
		U32(a1, 0x44) = a3;
		U16(a3, 0x14) = 0;
		U16(a3, 0x16) = 0;
		U16(a3, 0x18) = 0x140;
		U16(a3, 0x1A) = 0;
		U8(a3, 0x1C) = 0x80;
		U8(a3, 0x1D) = 0x80;
		U8(a3, 0x1E) = 0x80;
		U32(a3, 0x20) = 0xFFFFFFFF;
		U16(a3, 0x24) = 0;
		return a3;
	}

	// 0x7174B0 (module 100 sub_7174B0): wobble state 2: counts timer +0xCC down, next state at 0
	uint32_t __cdecl b10_7174B0(uint32_t a1)
	{
		U16(a1, 0xCC) = (uint16_t)(U16(a1, 0xCC) - 1);
		if (S16(a1, 0xCC) <= 0)
			b103_next_state(a1);
		return 0; // void
	}

	// 0x7174D0 (module 100 sub_7174D0): wobble state 3: turns the target entity (+0xC += 0x40, mod
	// 0x1000) until it is back at or below 0x800 (then +0xC = 0, next state); ramps +0xC4 up by 0x200
	// to 0x1000
	uint32_t __cdecl b10_7174D0(uint32_t a1)
	{
		uint32_t ent = b103_entity_of(a1);
		uint16_t ang = (uint16_t)((U16(ent, 0xC) + 0x40) & 0xFFF);
		U16(ent, 0xC) = ang;
		if ((int16_t)ang <= 0x800)
		{
			U16(ent, 0xC) = 0;
			b103_next_state(a1);
		}
		U16(a1, 0xC4) = (uint16_t)(U16(a1, 0xC4) + 0x200);
		if (S16(a1, 0xC4) >= 0x1000)
			U16(a1, 0xC4) = 0x1000;
		return 0; // void
	}

	// 0x717540 (module 100 sub_717540): wobble state 4: ramps +0xC4 down by 0x200 (next state at 0),
	// copies it to +0xC6
	uint32_t __cdecl b10_717540(uint32_t a1)
	{
		U16(a1, 0xC4) = (uint16_t)(U16(a1, 0xC4) + 0xFE00);
		if (S16(a1, 0xC4) <= 0)
		{
			U16(a1, 0xC4) = 0;
			b103_next_state(a1);
		}
		U16(a1, 0xC6) = U16(a1, 0xC4);
		return 0; // void
	}

	// 0x717590 (module 100 sub_717590): wobble state 5: when timer +0xCC runs out, shows the target
	// entity again (entity +0 &= ~4), stops the custom draw (+0x26 &= ~8), finishes, next state
	uint32_t __cdecl b10_717590(uint32_t a1)
	{
		uint32_t ent = b103_entity_of(a1);
		U16(a1, 0xCC) = (uint16_t)(U16(a1, 0xCC) - 1);
		if (S16(a1, 0xCC) > 0)
			return 0;
		U16(ent, 0) &= 0xFFFB;
		U16(a1, 0x26) = (uint16_t)((U16(a1, 0x26) & 0xFFF7) | 1);
		b103_next_state(a1);
		return 0; // void
	}

	// 0x7176B0 (module 100 sub_7176B0): creature state: position = director +8/+0xC with +0x4E =
	// 0xB000, scale +0x114..+0x11C = 0x2800 (+0x60 -> +0x114), +0x3E = 0x800, anim 0, enables the
	// draw (+0x26 |= 4), timer +0x134 = 8, next state
	uint32_t __cdecl b10_7176B0(uint32_t a1)
	{
		uint32_t dir = U32(0x15297F0, 0);
		uint32_t p0 = U32(dir, 8);
		uint32_t p1 = U32(dir, 0xC);
		U32(a1, 0x4C) = p0;
		U32(a1, 0x50) = p1;
		U16(a1, 0x4E) = 0xB000;
		U32(a1, 0x60) = a1 + 0x114;
		U32(a1, 0x11C) = 0x2800;
		U32(a1, 0x118) = 0x2800;
		U32(a1, 0x114) = 0x2800;
		U16(a1, 0x3E) = 0x800;
		x::au_re_Battle_ReadAnimation_7(a1, 0);
		U8(a1, 0x26) |= 4;
		U16(a1, 0x134) = 8;
		b103_next_state(a1);
		return 0; // void
	}

	// 0x717710 (module 100 sub_717710): creature state: when timer +0x134 runs out, hides the model
	// (+0x26 &= ~4), sets the darkened colour (0x717750), timer 30, next state
	uint32_t __cdecl b10_717710(uint32_t a1)
	{
		U16(a1, 0x134) = (uint16_t)(U16(a1, 0x134) - 1);
		if (S16(a1, 0x134) > 0)
			return 0;
		U8(a1, 0x26) &= 0xFB;
		b10_717750(a1);
		U16(a1, 0x134) = 0x1E;
		b103_next_state(a1);
		return 0; // void
	}

	// 0x717750 (module 100 sub_717750): +0x13E = 0x2400, darkness +0x13C = 0x1000, applies it (0x717770)
	uint32_t __cdecl b10_717750(uint32_t a1)
	{
		U16(a1, 0x13E) = 0x2400;
		U16(a1, 0x13C) = 0x1000;
		return b10_717770(a1);
	}

	// 0x717770 (module 100 sub_717770): creature colour +0x5C..+0x5E = c - c*(+0x13C)/4096 for the
	// three colour bytes c at 0xB8B9A8
	uint32_t __cdecl b10_717770(uint32_t a1)
	{
		uint32_t col = U32(0xB8B9A8, 0);
		int32_t k = (int32_t)S16(a1, 0x13C);
		uint8_t c0 = (uint8_t)(col & 0xFF);
		int32_t d0 = mul32((int32_t)c0, k) / 4096;
		U8(a1, 0x5C) = (uint8_t)(c0 - (uint8_t)d0);
		uint8_t c1 = (uint8_t)((col >> 8) & 0xFF);
		int32_t d1 = mul32((int32_t)c1, k) / 4096;
		U8(a1, 0x5D) = (uint8_t)(c1 - (uint8_t)d1);
		uint8_t c2 = U8(0xB8B9AA, 0);
		int32_t d2 = mul32((int32_t)c2, k) / 4096;
		U8(a1, 0x5E) = (uint8_t)(c2 - (uint8_t)d2);
		return 0; // void
	}

	// 0x7177E0 (module 100 sub_7177E0): creature state: counts timer +0x134 down (next state at 0),
	// then steps the colour fade (0x717800)
	uint32_t __cdecl b10_7177E0(uint32_t a1)
	{
		U16(a1, 0x134) = (uint16_t)(U16(a1, 0x134) - 1);
		if (S16(a1, 0x134) <= 0)
			b103_next_state(a1);
		return b10_717800(a1);
	}

	// 0x717800 (module 100 sub_717800): fade step: +0x13E -= 0x78 down to 0x1400 (unless it is 0x1000),
	// darkness +0x13C -= 0x69 down to 0x200 (unless 0), applies the colour (0x717770)
	uint32_t __cdecl b10_717800(uint32_t a1)
	{
		uint16_t v = U16(a1, 0x13E);
		if (v != 0x1000)
		{
			v = (uint16_t)(v - 0x78);
			U16(a1, 0x13E) = v;
			if ((int16_t)v <= 0x1400)
				U16(a1, 0x13E) = 0x1400;
		}
		uint16_t d = U16(a1, 0x13C);
		if (d != 0)
		{
			d = (uint16_t)(d - 0x69);
			U16(a1, 0x13C) = d;
			if ((int16_t)d <= 0x200)
				U16(a1, 0x13C) = 0x200;
		}
		return b10_717770(a1);
	}

	// 0x717860 (module 100 __cintrindisp1, misnamed): creature state: at director gate 8 resets timer
	// +0x134, sets the rise speed +0x126 = 0x200 and advances the state; steps the fade (0x717800)
	uint32_t __cdecl b10_717860(uint32_t a1)
	{
		if (a_73B640(8) != 0)
		{
			U16(a1, 0x134) = 0;
			U16(a1, 0x126) = 0x200;
			b103_next_state(a1);
		}
		return b10_717800(a1);
	}

	// 0x7178A0 (module 100 sub_7178A0): creature state: on the 4th tick resets the colour (0x717910);
	// accelerates (+0x126 += 0x60) and moves +0x4E by it; once the director's step reached 9 snaps
	// +0x4E to the first word of the table 0x1529F64 and advances the state; steps the fade
	uint32_t __cdecl b10_7178A0(uint32_t a1)
	{
		U16(a1, 0x134) = (uint16_t)(U16(a1, 0x134) + 1);
		if (U16(a1, 0x134) == 4)
			b10_717910(a1);
		U16(a1, 0x126) = (uint16_t)(U16(a1, 0x126) + 0x60);
		U16(a1, 0x4E) = (uint16_t)(U16(a1, 0x4E) + U16(a1, 0x126));
		if (a_73B7E0(9) != 0)
		{
			uint16_t y = U16(0x1529F64, 0);
			U16(a1, 0x134) = 0;
			U16(a1, 0x4E) = y;
			b103_next_state(a1);
		}
		return b10_717800(a1);
	}

	// 0x717910 (module 100 sub_717910): +0x13E = 0x1000, darkness +0x13C = 0, applies it (0x717770)
	uint32_t __cdecl b10_717910(uint32_t a1)
	{
		U16(a1, 0x13E) = 0x1000;
		U16(a1, 0x13C) = 0;
		return b10_717770(a1);
	}

	// 0x717930 (module 100 sub_717930): creature state: +0x4E follows the word table 0x1529F64
	// (index ++timer +0x134); at its 0 terminator plays anim 1, arms the sequence 0x1529F38
	// (a_739AC0) and advances the state
	uint32_t __cdecl b10_717930(uint32_t a1)
	{
		U16(a1, 0x134) = (uint16_t)(U16(a1, 0x134) + 1);
		int32_t idx = (int32_t)S16(a1, 0x134);
		uint16_t y = U16(0x1529F64 + (uint32_t)(idx * 2), 0);
		U16(a1, 0x4E) = y;
		if (y != 0)
			return 0;
		x::au_re_Battle_ReadAnimation_7(a1, 1);
		a_739AC0(a1, 0x1529F38, 0);
		b103_next_state(a1);
		return 0; // void
	}

	// 0x717980 (module 100 sub_717980): creature state: when the animation ends plays anim 2, timer
	// +0x134 = 10, next state
	uint32_t __cdecl b10_717980(uint32_t a1)
	{
		if (x::au_re_Battle_ReadAnimation_8(a1) != 1)
			return 0;
		x::au_re_Battle_ReadAnimation_7(a1, 2);
		U16(a1, 0x134) = 0xA;
		b103_next_state(a1);
		return 0; // void
	}

	// 0x7179C0 (module 100 sub_7179C0): creature state: steps the animation (sub_8DD1C0), counts
	// timer +0x134 down, next state at 0
	uint32_t __cdecl b10_7179C0(uint32_t a1)
	{
		x::sub_8DD1C0(a1);
		U16(a1, 0x134) = (uint16_t)(U16(a1, 0x134) - 1);
		if (S16(a1, 0x134) <= 0)
			b103_next_state(a1);
		return 0; // void
	}

	// 0x7179F0 (module 100 sub_7179F0): creature state: fade-out +0x13A += 0x100 up to 0x1000 (then
	// finishes: +0x26 |= 5, next state); darkness +0x13C = +0x13A, tints the model (0x717A50 mode 1)
	// and applies the colour (0x717770)
	uint32_t __cdecl b10_7179F0(uint32_t a1)
	{
		U16(a1, 0x13A) = (uint16_t)(U16(a1, 0x13A) + 0x100);
		if (S16(a1, 0x13A) >= 0x1000)
		{
			U8(a1, 0x26) |= 5;
			U16(a1, 0x13A) = 0x1000;
			b103_next_state(a1);
		}
		U16(a1, 0x13C) = U16(a1, 0x13A);
		b10_717A50(a1, 1);
		return b10_717770(a1);
	}

	// 0x717A50 (module 100 sub_717A50): tints the node's model (+0x30) with amount +0x13A, mode a2
	// (a_733950)
	uint32_t __cdecl b10_717A50(uint32_t a1, uint32_t a2)
	{
		// quirk 0x717A59: `mov cx, [eax+0x13A]; push ecx` - the high half of the pushed amount is the
		// caller's ecx (garbage); a_733950 only reads its low 16 bits
		uint32_t amount = U16(a1, 0x13A);
		return a_733950(a1 + 0x30, amount, a2);
	}

namespace boko
{
	struct ModPort { uint32_t addr; void *port; const char *name; };
	static const ModPort PORTS_097[] = {
		{ 0x729BD0, (void *)b_729BD0, "097 MAG_097_sub_729BD0" },
		{ 0x729EA0, (void *)b7_729EA0, "097 MAG_097_sub_729EA0" },
		{ 0x729EF0, (void *)b7_729EF0, "097 MAG_097_sub_729EF0" },
		{ 0x72A350, (void *)b_72A350, "097 MAG_097_sub_72A350" },
		{ 0x72A410, (void *)b7_72A410, "097 MAG_097_sub_72A410" },
		{ 0x72A4F0, (void *)b7_72A4F0, "097 sub_72A4F0" },
		{ 0x72A540, (void *)b_72A540, "097 sub_72A540" },
		{ 0x72A610, (void *)b_72A610, "097 sub_72A610" },
		{ 0x72A6C0, (void *)b_72A6C0, "097 sub_72A6C0" },
		{ 0x72A720, (void *)b_72A720, "097 sub_72A720" },
		{ 0x72A840, (void *)b_72A840, "097 sub_72A840" },
		{ 0x72AAF0, (void *)b_72AAF0, "097 sub_72AAF0" },
		{ 0x72AB80, (void *)b7_72AB80, "097 sub_72AB80" },
		{ 0x72AD60, (void *)b_72AD60, "097 sub_72AD60" },
		{ 0x72AFE0, (void *)b_72AFE0, "097 sub_72AFE0" },
		{ 0x72B690, (void *)b7_72B690, "097 sub_72B690" },
		{ 0x72BA00, (void *)b_72BA00, "097 sub_72BA00" },
		{ 0x72BC10, (void *)b_72BC10, "097 sub_72BC10" },
		{ 0x72C370, (void *)b_72C370, "097 sub_72C370" },
		{ 0x72C5A0, (void *)b_72C5A0, "097 sub_72C5A0" },
		{ 0x72C860, (void *)b_72C860, "097 sub_72C860" },
		{ 0x72CAC0, (void *)b_72CAC0, "097 sub_72CAC0" },
		{ 0x72CEE0, (void *)b7_72CEE0, "097 sub_72CEE0" },
		{ 0x72CF20, (void *)b7_72CF20, "097 sub_72CF20" },
		{ 0x72D7C0, (void *)b7_72D7C0, "097 sub_72D7C0" },
		{ 0x72DF60, (void *)b7_72DF60, "097 sub_72DF60" },
		{ 0x72E220, (void *)b_72E220, "097 sub_72E220" },
		{ 0x72EF80, (void *)b_72EF80, "097 sub_72EF80" },
		{ 0x72FB90, (void *)b7_72FB90, "097 sub_72FB90" },
		{ 0x72FBC0, (void *)b7_72FBC0, "097 sub_72FBC0" },
		{ 0x72FC40, (void *)b_72FC40, "097 sub_72FC40" },
		{ 0x730460, (void *)b_730460, "097 sub_730460" },
		{ 0x730510, (void *)b_730510, "097 sub_730510" },
		{ 0x7305D0, (void *)b_7305D0, "097 sub_7305D0" },
		{ 0x730660, (void *)b_730660, "097 MAG_097_SpawnBokoCreature" },
		{ 0x7306A0, (void *)b_7306A0, "097 sub_7306A0" },
		{ 0x730B00, (void *)b_730B00, "097 sub_730B00" },
		{ 0x730BD0, (void *)b_730BD0, "097 sub_730BD0" },
		{ 0x730C10, (void *)b_730C10, "097 sub_730C10" },
		{ 0x730C90, (void *)b_730C90, "097 sub_730C90" },
		{ 0x730D00, (void *)b_730D00, "097 sub_730D00" },
		{ 0x730D50, (void *)b_730D50, "097 sub_730D50" },
		{ 0x730D70, (void *)b_730D70, "097 sub_730D70" },
		{ 0x730DD0, (void *)b_730DD0, "097 sub_730DD0" },
		{ 0x730E30, (void *)b_730E30, "097 sub_730E30" },
		{ 0x730EA0, (void *)b_730EA0, "097 sub_730EA0" },
		{ 0x730EF0, (void *)b_730EF0, "097 sub_730EF0" },
		{ 0x730F10, (void *)b7_730F10, "097 sub_730F10" },
		{ 0x730F50, (void *)b_730F50, "097 sub_730F50" },
		{ 0x730FE0, (void *)b_730FE0, "097 sub_730FE0" },
		{ 0x731090, (void *)b_731090, "097 sub_731090" },
		{ 0x7310C0, (void *)b7_7310C0, "097 sub_7310C0" },
		{ 0x731100, (void *)b7_731100, "097 sub_731100" },
		{ 0x731520, (void *)b_731520, "097 sub_731520" },
		{ 0x731570, (void *)b_72AAF0, "097 sub_731570" },
		{ 0x7315A0, (void *)b7_7315A0, "097 sub_7315A0" },
		{ 0x7315D0, (void *)b_7315D0, "097 sub_7315D0" },
		{ 0x731600, (void *)b7_731600, "097 sub_731600" },
		{ 0x731690, (void *)b_731690, "097 sub_731690" },
		{ 0x7316B0, (void *)b_7316B0, "097 sub_7316B0" },
		{ 0x731720, (void *)b_731720, "097 sub_731720" },
		{ 0x731780, (void *)b7_731780, "097 sub_731780" },
		{ 0x731840, (void *)b_731840, "097 sub_731840" },
		{ 0x7318D0, (void *)b_7318D0, "097 sub_7318D0" },
		{ 0x731920, (void *)b_72AAF0, "097 sub_731920" },
		{ 0x731950, (void *)b_731950, "097 sub_731950" },
		{ 0x7319D0, (void *)b_7319D0, "097 sub_7319D0" },
		{ 0x731A20, (void *)b_72AAF0, "097 sub_731A20" },
		{ 0x731AF0, (void *)b_731AF0, "097 sub_731AF0" },
		{ 0x731BE0, (void *)b_731BE0, "097 sub_731BE0" },
		{ 0, nullptr, nullptr } };
	static const ModPort PORTS_098[] = {
		{ 0x7219D0, (void *)b_729BD0, "098 MAG_098_sub_7219D0" },
		{ 0x721CA0, (void *)b8_721CA0, "098 MAG_098_sub_721CA0" },
		{ 0x721CF0, (void *)b8_721CF0, "098 MAG_098_sub_721CF0" },
		{ 0x722150, (void *)b_72A350, "098 MAG_098_sub_722150" },
		{ 0x722210, (void *)b8_722210, "098 MAG_098_sub_722210" },
		{ 0x7222F0, (void *)b8_7222F0, "098 sub_7222F0" },
		{ 0x722340, (void *)b8_722340, "098 sub_722340" },
		{ 0x722410, (void *)b8_722410, "098 sub_722410" },
		{ 0x7224C0, (void *)b_72A6C0, "098 sub_7224C0" },
		{ 0x722520, (void *)b_72A720, "098 sub_722520" },
		{ 0x722640, (void *)b_72A840, "098 sub_722640" },
		{ 0x7228F0, (void *)b_72AAF0, "098 sub_7228F0" },
		{ 0x722B60, (void *)b_72AD60, "098 sub_722B60" },
		{ 0x722DE0, (void *)b_72AFE0, "098 sub_722DE0" },
		{ 0x723800, (void *)b_72BA00, "098 sub_723800" },
		{ 0x723A10, (void *)b_72BC10, "098 sub_723A10" },
		{ 0x724170, (void *)b_72C370, "098 sub_724170" },
		{ 0x7243A0, (void *)b_72C5A0, "098 sub_7243A0" },
		{ 0x724660, (void *)b_72C860, "098 sub_724660" },
		{ 0x7248C0, (void *)b_72CAC0, "098 sub_7248C0" },
		{ 0x725D50, (void *)b_72E220, "098 sub_725D50" },
		{ 0x726AB0, (void *)b_72EF80, "098 sub_726AB0" },
		{ 0x7276F0, (void *)b8_7276F0, "098 sub_7276F0" },
		{ 0x727780, (void *)b_72FC40, "098 sub_727780" },
		{ 0x727FA0, (void *)b_730460, "098 sub_727FA0" },
		{ 0x728050, (void *)b8_728050, "098 sub_728050" },
		{ 0x728080, (void *)b_730510, "098 sub_728080" },
		{ 0x728140, (void *)b_7305D0, "098 sub_728140" },
		{ 0x7281D0, (void *)b_730660, "098 MAG_098_SpawnBokoCreature" },
		{ 0x728210, (void *)b_7306A0, "098 sub_728210" },
		{ 0x728670, (void *)b_730B00, "098 sub_728670" },
		{ 0x728740, (void *)b_730BD0, "098 sub_728740" },
		{ 0x728780, (void *)b_730C10, "098 sub_728780" },
		{ 0x728800, (void *)b_730C90, "098 sub_728800" },
		{ 0x728870, (void *)b_730D00, "098 sub_728870" },
		{ 0x7288C0, (void *)b_730D50, "098 sub_7288C0" },
		{ 0x7288E0, (void *)b_730D70, "098 sub_7288E0" },
		{ 0x728940, (void *)b_730DD0, "098 sub_728940" },
		{ 0x7289A0, (void *)b_730E30, "098 sub_7289A0" },
		{ 0x728A10, (void *)b_730EA0, "098 sub_728A10" },
		{ 0x728A60, (void *)b_730EF0, "098 sub_728A60" },
		{ 0x728A80, (void *)b_728A80, "098 sub_728A80" },
		{ 0x728AB0, (void *)b_730F50, "098 sub_728AB0" },
		{ 0x728AE0, (void *)b8_728AE0, "098 sub_728AE0" },
		{ 0x728B20, (void *)b8_728B20, "098 sub_728B20" },
		{ 0x728BE0, (void *)b_731090, "098 sub_728BE0" },
		{ 0x728C10, (void *)b8_728C10, "098 sub_728C10" },
		{ 0x728CC0, (void *)b8_728CC0, "098 MAG_310_HolyWar_Code" },
		{ 0x728D30, (void *)b_72AAF0, "098 sub_728D30" },
		{ 0x728D60, (void *)b8_728D60, "098 sub_728D60" },
		{ 0x728DA0, (void *)b8_728DA0, "098 sub_728DA0" },
		{ 0x7291C0, (void *)b_731520, "098 sub_7291C0" },
		{ 0x729210, (void *)b_72AAF0, "098 sub_729210" },
		{ 0x729240, (void *)b8_729240, "098 sub_729240" },
		{ 0x729270, (void *)b8_729270, "098 sub_729270" },
		{ 0x7292A0, (void *)b8_7292A0, "098 sub_7292A0" },
		{ 0x729330, (void *)b_731690, "098 sub_729330" },
		{ 0x729350, (void *)b8_729350, "098 sub_729350" },
		{ 0x7293C0, (void *)b_731720, "098 sub_7293C0" },
		{ 0x729420, (void *)b8_729420, "098 sub_729420" },
		{ 0x7294C0, (void *)b_731840, "098 sub_7294C0" },
		{ 0x729550, (void *)b_7318D0, "098 sub_729550" },
		{ 0x7295A0, (void *)b_72AAF0, "098 sub_7295A0" },
		{ 0x7295D0, (void *)b_731950, "098 sub_7295D0" },
		{ 0x729650, (void *)b_7319D0, "098 sub_729650" },
		{ 0x7296A0, (void *)b_72AAF0, "098 sub_7296A0" },
		{ 0x729770, (void *)b_731AF0, "098 sub_729770" },
		{ 0x729860, (void *)b_731BE0, "098 sub_729860" },
		{ 0, nullptr, nullptr } };
	static const ModPort PORTS_099[] = {
		{ 0x7181B0, (void *)b9_7181B0, "099 MAG_099_sub_7181B0" },
		{ 0x718200, (void *)b9_718200, "099 MAG_099_sub_718200" },
		{ 0x718670, (void *)b_718670, "099 MAG_099_sub_718670" },
		{ 0x718750, (void *)b9_718750, "099 MAG_099_sub_718750" },
		{ 0x718840, (void *)b9_718840, "099 sub_718840" },
		{ 0x718890, (void *)b9_718890, "099 sub_718890" },
		{ 0x718960, (void *)b9_718960, "099 sub_718960" },
		{ 0x718A10, (void *)b_72A6C0, "099 sub_718A10" },
		{ 0x718A70, (void *)b_72A720, "099 sub_718A70" },
		{ 0x718B90, (void *)b_72A840, "099 sub_718B90" },
		{ 0x718E40, (void *)b_72AAF0, "099 sub_718E40" },
		{ 0x7190B0, (void *)b_72AD60, "099 sub_7190B0" },
		{ 0x719330, (void *)b_72AFE0, "099 sub_719330" },
		{ 0x719D50, (void *)b_72BA00, "099 sub_719D50" },
		{ 0x719F60, (void *)b_72BC10, "099 sub_719F60" },
		{ 0x71A6C0, (void *)b_72C370, "099 sub_71A6C0" },
		{ 0x71A8F0, (void *)b_72C5A0, "099 sub_71A8F0" },
		{ 0x71ABB0, (void *)b_72C860, "099 sub_71ABB0" },
		{ 0x71AE10, (void *)b_72CAC0, "099 sub_71AE10" },
		{ 0x71C2A0, (void *)b_72E220, "099 sub_71C2A0" },
		{ 0x71D000, (void *)b_72EF80, "099 sub_71D000" },
		{ 0x71DC40, (void *)b9_71DC40, "099 sub_71DC40" },
		{ 0x71DCD0, (void *)b_72FC40, "099 sub_71DCD0" },
		{ 0x71E4F0, (void *)b_730460, "099 sub_71E4F0" },
		{ 0x71E570, (void *)b9_71E570, "099 sub_71E570" },
		{ 0x71E5C0, (void *)b9_71E5C0, "099 sub_71E5C0" },
		{ 0x71E610, (void *)b9_71E610, "099 sub_71E610" },
		{ 0x71E6C0, (void *)b9_71E6C0, "099 sub_71E6C0" },
		{ 0x71E700, (void *)b9_71E700, "099 sub_71E700" },
		{ 0x71E800, (void *)b_7305D0, "099 sub_71E800" },
		{ 0x71E890, (void *)b_730660, "099 MAG_099_SpawnBokoCreature" },
		{ 0x71E8D0, (void *)b9_71E8D0, "099 sub_71E8D0" },
		{ 0x71ED20, (void *)b_730B00, "099 sub_71ED20" },
		{ 0x71EDF0, (void *)b9_71EDF0, "099 sub_71EDF0" },
		{ 0x71EE40, (void *)b_730C10, "099 sub_71EE40" },
		{ 0x71EEC0, (void *)b_730C90, "099 sub_71EEC0" },
		{ 0x71EF30, (void *)b9_71EF30, "099 sub_71EF30" },
		{ 0x71EF80, (void *)b9_71EF80, "099 sub_71EF80" },
		{ 0x71EFB0, (void *)b9_71EFB0, "099 sub_71EFB0" },
		{ 0x71F010, (void *)b9_71F010, "099 sub_71F010" },
		{ 0x71F080, (void *)b_730E30, "099 sub_71F080" },
		{ 0x71F0F0, (void *)b_730EA0, "099 sub_71F0F0" },
		{ 0x71F140, (void *)b_730EF0, "099 sub_71F140" },
		{ 0x71F160, (void *)b_728A80, "099 sub_71F160" },
		{ 0x71F190, (void *)b9_71F190, "099 sub_71F190" },
		{ 0x71F1F0, (void *)b_730FE0, "099 sub_71F1F0" },
		{ 0x71F2A0, (void *)b_731090, "099 sub_71F2A0" },
		{ 0x71F2D0, (void *)b9_71F2D0, "099 sub_71F2D0" },
		{ 0x71F3A0, (void *)b9_71F3A0, "099 sub_71F3A0" },
		{ 0x71F450, (void *)b9_71F450, "099 sub_71F450" },
		{ 0x71F470, (void *)b9_71F470, "099 sub_71F470" },
		{ 0x71F4C0, (void *)b9_71F4C0, "099 sub_71F4C0" },
		{ 0x71F500, (void *)b_71F500, "099 sub_71F500" },
		{ 0x71F520, (void *)b9_71F520, "099 sub_71F520" },
		{ 0x71F550, (void *)b9_71F550, "099 sub_71F550" },
		{ 0x71F5A0, (void *)b9_71F5A0, "099 sub_71F5A0" },
		{ 0x71F5C0, (void *)b9_71F5C0, "099 sub_71F5C0" },
		{ 0x71F620, (void *)b9_71F620, "099 sub_71F620" },
		{ 0x71F8E0, (void *)b_71F8E0, "099 sub_71F8E0" },
		{ 0x71F900, (void *)b_71F900, "099 sub_71F900" },
		{ 0x71F950, (void *)b_731840, "099 sub_71F950" },
		{ 0x71FD40, (void *)b_7318D0, "099 sub_71FD40" },
		{ 0x71FD90, (void *)b_72AAF0, "099 sub_71FD90" },
		{ 0x71FDC0, (void *)b_731950, "099 sub_71FDC0" },
		{ 0x71FE40, (void *)b_7319D0, "099 sub_71FE40" },
		{ 0x71FE90, (void *)b_72AAF0, "099 sub_71FE90" },
		{ 0x71FF20, (void *)b9_71FF20, "099 sub_71FF20" },
		{ 0x71FF70, (void *)b9_71FF70, "099 sub_71FF70" },
		{ 0x720020, (void *)b9_720020, "099 sub_720020" },
		{ 0x720080, (void *)b9_720080, "099 sub_720080" },
		{ 0x7200F0, (void *)b9_7200F0, "099 sub_7200F0" },
		{ 0x720140, (void *)b9_720140, "099 sub_720140" },
		{ 0x720250, (void *)b9_720250, "099 sub_720250" },
		{ 0x720290, (void *)b9_720290, "099 sub_720290" },
		{ 0x7202F0, (void *)b9_7202F0, "099 sub_7202F0" },
		{ 0x720380, (void *)b9_720380, "099 sub_720380" },
		{ 0x7203D0, (void *)b9_7203D0, "099 sub_7203D0" },
		{ 0x720470, (void *)b9_720470, "099 sub_720470" },
		{ 0x7204C0, (void *)b9_7204C0, "099 sub_7204C0" },
		{ 0x720500, (void *)b9_720500, "099 sub_720500" },
		{ 0x720570, (void *)b9_720570, "099 sub_720570" },
		{ 0x7205A0, (void *)b9_7205A0, "099 sub_7205A0" },
		{ 0x7205D0, (void *)b9_7205D0, "099 sub_7205D0" },
		{ 0x720670, (void *)b9_720670, "099 sub_720670" },
		{ 0x7206D0, (void *)b9_7206D0, "099 sub_7206D0" },
		{ 0x720790, (void *)b9_720790, "099 sub_720790" },
		{ 0x7207E0, (void *)b9_7207E0, "099 sub_7207E0" },
		{ 0x7208D0, (void *)b9_7208D0, "099 sub_7208D0" },
		{ 0x720940, (void *)b9_720940, "099 sub_720940" },
		{ 0x720960, (void *)b9_720960, "099 sub_720960" },
		{ 0x7209B0, (void *)b9_7209B0, "099 sub_7209B0" },
		{ 0x720AE0, (void *)b9_720AE0, "099 sub_720AE0" },
		{ 0x720B90, (void *)b9_720B90, "099 sub_720B90" },
		{ 0x720C90, (void *)b9_720C90, "099 sub_720C90" },
		{ 0x720D00, (void *)b9_720D00, "099 sub_720D00" },
		{ 0x720D20, (void *)b9_720D20, "099 sub_720D20" },
		{ 0x720D70, (void *)b9_720D70, "099 sub_720D70" },
		{ 0x720EA0, (void *)b9_720EA0, "099 sub_720EA0" },
		{ 0x720FA0, (void *)b9_720FA0, "099 sub_720FA0" },
		{ 0x720FC0, (void *)b9_720FC0, "099 sub_720FC0" },
		{ 0x7210B0, (void *)b_731AF0, "099 sub_7210B0" },
		{ 0x721160, (void *)b_731690, "099 sub_721160" },
		{ 0x721180, (void *)b_7316B0, "099 sub_721180" },
		{ 0x7211D0, (void *)b9_7211D0, "099 sub_7211D0" },
		{ 0x721320, (void *)b9_721320, "099 sub_721320" },
		{ 0x721360, (void *)b9_721360, "099 sub_721360" },
		{ 0x721390, (void *)b9_721390, "099 sub_721390" },
		{ 0x7213F0, (void *)b9_7213F0, "099 sub_7213F0" },
		{ 0x721430, (void *)b9_721430, "099 sub_721430" },
		{ 0x7214C0, (void *)b9_7214C0, "099 sub_7214C0" },
		{ 0x7214E0, (void *)b9_7214E0, "099 sub_7214E0" },
		{ 0x721610, (void *)b9_721610, "099 sub_721610" },
		{ 0x721630, (void *)b9_721630, "099 sub_721630" },
		{ 0x721660, (void *)b9_721660, "099 sub_721660" },
		{ 0, nullptr, nullptr } };
	static const ModPort PORTS_100[] = {
		{ 0x70D530, (void *)b10_70D530, "100 sub_70D530" },
		{ 0x70D680, (void *)b10_70D680, "100 sub_70D680" },
		{ 0x70D8A0, (void *)b10_70D8A0, "100 sub_70D8A0" },
		{ 0x70DB40, (void *)b10_70DB40, "100 sub_70DB40" },
		{ 0x70DDA0, (void *)b10_70DDA0, "100 sub_70DDA0" },
		{ 0x70E060, (void *)b10_70E060, "100 sub_70E060" },
		{ 0x70E2A0, (void *)b10_70E2A0, "100 sub_70E2A0" },
		{ 0x70E580, (void *)b10_70E580, "100 sub_70E580" },
		{ 0x70E800, (void *)b10_70E800, "100 sub_70E800" },
		{ 0x70EDE0, (void *)b10_70EDE0, "100 MAG_100_sub_70EDE0" },
		{ 0x70EE30, (void *)b10_70EE30, "100 MAG_100_sub_70EE30" },
		{ 0x70F290, (void *)b_718670, "100 MAG_100_sub_70F290" },
		{ 0x70F370, (void *)b10_70F370, "100 MAG_100_sub_70F370" },
		{ 0x70F480, (void *)b10_70F480, "100 sub_70F480" },
		{ 0x70F4D0, (void *)b_72A540, "100 MAG_100_SetupBokoCreature" },
		{ 0x70F5A0, (void *)b_72A610, "100 sub_70F5A0" },
		{ 0x70F650, (void *)b_72A6C0, "100 sub_70F650" },
		{ 0x70F6B0, (void *)b_72A720, "100 sub_70F6B0" },
		{ 0x70F7D0, (void *)b10_70F7D0, "100 sub_70F7D0" },
		{ 0x70FAC0, (void *)b_72AAF0, "100 sub_70FAC0" },
		{ 0x70FD30, (void *)b_72AD60, "100 sub_70FD30" },
		{ 0x70FFB0, (void *)b_72AFE0, "100 sub_70FFB0" },
		{ 0x7109D0, (void *)b_72BA00, "100 sub_7109D0" },
		{ 0x710BE0, (void *)b_72BC10, "100 sub_710BE0" },
		{ 0x711340, (void *)b_72C370, "100 sub_711340" },
		{ 0x711570, (void *)b_72C5A0, "100 sub_711570" },
		{ 0x711830, (void *)b_72C860, "100 sub_711830" },
		{ 0x711A90, (void *)b_72CAC0, "100 sub_711A90" },
		{ 0x712F20, (void *)b_72E220, "100 sub_712F20" },
		{ 0x713C80, (void *)b_72EF80, "100 sub_713C80" },
		{ 0x7148C0, (void *)b10_7148C0, "100 MAG_100_BokoCreatureTask" },
		{ 0x714970, (void *)b_72FC40, "100 sub_714970" },
		{ 0x715190, (void *)b_730460, "100 sub_715190" },
		{ 0x715210, (void *)b10_715210, "100 sub_715210" },
		{ 0x715250, (void *)b10_715250, "100 sub_715250" },
		{ 0x715280, (void *)b10_715280, "100 sub_715280" },
		{ 0x7152C0, (void *)b10_7152C0, "100 sub_7152C0" },
		{ 0x7152E0, (void *)b10_7152E0, "100 sub_7152E0" },
		{ 0x715330, (void *)b10_715330, "100 sub_715330" },
		{ 0x7153B0, (void *)b10_7153B0, "100 sub_7153B0" },
		{ 0x715470, (void *)b_7305D0, "100 sub_715470" },
		{ 0x715500, (void *)b_730660, "100 MAG_100_SpawnBokoCreature" },
		{ 0x715540, (void *)b10_715540, "100 sub_715540" },
		{ 0x7159A0, (void *)b_730B00, "100 sub_7159A0" },
		{ 0x715A70, (void *)b_730BD0, "100 sub_715A70" },
		{ 0x715AB0, (void *)b_730C10, "100 sub_715AB0" },
		{ 0x715B30, (void *)b_730C90, "100 sub_715B30" },
		{ 0x715BA0, (void *)b_730D00, "100 sub_715BA0" },
		{ 0x715BF0, (void *)b_730D50, "100 sub_715BF0" },
		{ 0x715C10, (void *)b_730D70, "100 sub_715C10" },
		{ 0x715C70, (void *)b_730DD0, "100 sub_715C70" },
		{ 0x715CD0, (void *)b_730E30, "100 sub_715CD0" },
		{ 0x715D40, (void *)b_730EA0, "100 sub_715D40" },
		{ 0x715D60, (void *)b_730EF0, "100 sub_715D60" },
		{ 0x715D80, (void *)b_728A80, "100 sub_715D80" },
		{ 0x715DB0, (void *)b10_715DB0, "100 sub_715DB0" },
		{ 0x715DD0, (void *)b10_715DD0, "100 sub_715DD0" },
		{ 0x715DF0, (void *)b10_715DF0, "100 sub_715DF0" },
		{ 0x715E50, (void *)b_730FE0, "100 sub_715E50" },
		{ 0x715F60, (void *)b_71F500, "100 sub_715F60" },
		{ 0x715F80, (void *)b10_715F80, "100 sub_715F80" },
		{ 0x716040, (void *)b10_716040, "100 sub_716040" },
		{ 0x716070, (void *)b_7315D0, "100 sub_716070" },
		{ 0x7160C0, (void *)b10_7160C0, "100 sub_7160C0" },
		{ 0x716120, (void *)b10_716120, "100 sub_716120" },
		{ 0x716360, (void *)b_71F8E0, "100 sub_716360" },
		{ 0x716380, (void *)b_71F900, "100 sub_716380" },
		{ 0x7163D0, (void *)b_731840, "100 sub_7163D0" },
		{ 0x7167C0, (void *)b_7318D0, "100 sub_7167C0" },
		{ 0x716810, (void *)b_72AAF0, "100 sub_716810" },
		{ 0x716840, (void *)b_731950, "100 sub_716840" },
		{ 0x7168C0, (void *)b_7319D0, "100 sub_7168C0" },
		{ 0x716910, (void *)b10_716910, "100 sub_716910" },
		{ 0x7169B0, (void *)b10_7169B0, "100 sub_7169B0" },
		{ 0x716A20, (void *)b10_716A20, "100 sub_716A20" },
		{ 0x716B00, (void *)b_731AF0, "100 sub_716B00" },
		{ 0x716BB0, (void *)b10_716BB0, "100 sub_716BB0" },
		{ 0x716BD0, (void *)b_7316B0, "100 sub_716BD0" },
		{ 0x716C20, (void *)b10_716C20, "100 sub_716C20" },
		{ 0x716CD0, (void *)b10_716CD0, "100 sub_716CD0" },
		{ 0x716DD0, (void *)b10_716DD0, "100 sub_716DD0" },
		{ 0x717330, (void *)b10_717330, "100 sub_717330" },
		{ 0x717350, (void *)b10_717350, "100 sub_717350" },
		{ 0x717450, (void *)b10_717450, "100 sub_717450" },
		{ 0x7174B0, (void *)b10_7174B0, "100 sub_7174B0" },
		{ 0x7174D0, (void *)b10_7174D0, "100 sub_7174D0" },
		{ 0x717540, (void *)b10_717540, "100 sub_717540" },
		{ 0x717590, (void *)b10_717590, "100 sub_717590" },
		{ 0x7175F0, (void *)b_7306A0, "100 sub_7175F0" },
		{ 0x7176B0, (void *)b10_7176B0, "100 sub_7176B0" },
		{ 0x717710, (void *)b10_717710, "100 sub_717710" },
		{ 0x717750, (void *)b10_717750, "100 sub_717750" },
		{ 0x717770, (void *)b10_717770, "100 sub_717770" },
		{ 0x7177E0, (void *)b10_7177E0, "100 sub_7177E0" },
		{ 0x717800, (void *)b10_717800, "100 sub_717800" },
		{ 0x717860, (void *)b10_717860, "100 __cintrindisp1" },
		{ 0x7178A0, (void *)b10_7178A0, "100 sub_7178A0" },
		{ 0x717910, (void *)b10_717910, "100 sub_717910" },
		{ 0x717930, (void *)b10_717930, "100 sub_717930" },
		{ 0x717980, (void *)b10_717980, "100 sub_717980" },
		{ 0x7179C0, (void *)b10_7179C0, "100 sub_7179C0" },
		{ 0x7179F0, (void *)b10_7179F0, "100 sub_7179F0" },
		{ 0x717A50, (void *)b10_717A50, "100 sub_717A50" },
		{ 0, nullptr, nullptr } };
	// tasks whose drawing the held frame redraws: the creature actor(s) and the prim-model tasks
	// (every packet of these comes from their prim-model plays / draw callbacks)
	static const uint32_t HELD_097[] = { 0x7306A0, 0x72A660, 0x731100, 0x731840, 0x731950, 0 };
	static const uint32_t HELD_098[] = { 0x728210, 0x722460, 0x728C60, 0x728DA0, 0x7294C0, 0x7295D0, 0 };
	static const uint32_t HELD_099[] = { 0x71E8D0, 0x7189B0, 0x71F950, 0x71FDC0, 0x71FEC0, 0x71FFB0, 0x7201F0, 0x720320, 0x720410, 0x720610, 0 };
	static const uint32_t HELD_100[] = { 0x715540, 0x7175F0, 0x70F5F0, 0x7163D0, 0x716840, 0x716950, 0 };
	// ChocoBocle's second creature kind (0x7175F0 = the Boko chocobo actor of 097/098)
	static const HeldCreature MORE_100[] = { { 0x25656B8, 0x7175F0, 0x25642E8 }, { 0, 0, 0 } };
	// module globals: each module's block up to the next module's
	static const HeldDesc HELD_D097 = { &MOD_097, 0x257B158, 0x7306A0, 0x2579EC8, 0x2575268, 0x257B740 };
	static const HeldDesc HELD_D098 = { &MOD_098, 0x2574ED8, 0x728210, 0x2573C48, 0x256E358, 0x2575268 };
	static const HeldDesc HELD_D099 = { &MOD_099, 0x256D9D0, 0x71E8D0, 0x256C600, 0x2565A30, 0x256E358 };
	static const HeldDesc HELD_D100 = { &MOD_100, 0x25656B8, 0x715540, 0x25642E8, 0x255EAC8, 0x2565A30, MORE_100 };
	static bool is_in(const uint32_t *l, uint32_t a) { for (; *l; l++) if (*l == a) return true; return false; }
	static bool HeldReady() { return held_ready(); }
	template<const HeldDesc *D> static void HeldFrame(int num, int den) { held_frame(*D, num, den); }
	template<int E> static bool HeldCamera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		const Mod *m = g_mod;
		g_mod = find_mod(E);
		bool r = held_camera(num, den, world, lookat);
		g_mod = m;
		return r;
	}
	static void reg(int eff, const ModPort *ports, const uint32_t *held, void (*frame)(int, int), HeldCameraFn cam)
	{
		register_module(eff, held);
		for (const ModPort *p = ports; p->addr; p++)
			register_module_port(eff, p->addr, p->port, p->name, is_in(held, p->addr));
		register_module_held(eff, HeldReady, frame);
		register_module_camera(eff, cam);
	}
}
}
	void register_mag097_boko()
	{
		using namespace act::boko;
		reg(97, PORTS_097, HELD_097, HeldFrame<&HELD_D097>, HeldCamera<97>);
		reg(98, PORTS_098, HELD_098, HeldFrame<&HELD_D098>, HeldCamera<98>);
		reg(99, PORTS_099, HELD_099, HeldFrame<&HELD_D099>, HeldCamera<99>);
		reg(100, PORTS_100, HELD_100, HeldFrame<&HELD_D100>, HeldCamera<100>);
	}
}
