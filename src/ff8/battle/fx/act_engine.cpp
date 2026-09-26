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

// Actor state-machine GF family engine (see act_engine.h): the generated module descriptors and
// engine-port address lists of the seven modules, the dispatch / registration / held-frame core,
// and the engine ports (every function whose code is shared by at least two modules), grouped
// by the parts they were ported in. Every port follows the FF8_EN.exe listing of its canonical
// copy instruction by instruction and was verified bit-exact in the offline differential harness
// (Siren 095 and MiniMog 096 run to their end, every tick, every byte, every engine call).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
	// ---- generated: module descriptors ----
	const Mod MOD_090 = { "tonberry", 90, 0x762320, 0x7684D0,
		{ 0x15474D0, 0x0, 0x1547168, 0x1547510, 0x154EB08, 0x259EFB4, 0x0, 0x0, 0x0, 0x0, 0x25A4C1C, 0x25A4C28, 0x25A4C20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25A2118, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x25A2B80, 0x0, 0x0, 0x25A2CF8, 0x25A3DEC, 0x0, 0x0, 0x0, 0x25A4008, 0x0, 0x0, 0x0, 0x25A4C18, 0x0 },
		{ 0x0, 0x762E60, 0x762E90, 0x763690, 0x763790, 0x763920, 0x763970, 0x7639C0, 0x7639E0, 0x763A00, 0x763A30, 0x763A80, 0x763AA0, 0x763AC0, 0x763B10, 0x763B30, 0x763B60, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x762690, 0x762710, 0x7628B0, 0x762A50, 0x762A80, 0x762AB0, 0x762AC0, 0x762AD0, 0x762AE0, 0x762AF0, 0x762B30, 0x762BA0, 0x763C10, 0x762EC0, 0x762EE0, 0x7631D0, 0x763520, 0x763740, 0x763820, 0x763870, 0x763940, 0x763B50, 0x0, 0x764260, 0x767150, 0x767190, 0x767460, 0x767480, 0x7674D0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x766A40, 0x766F50, 0x768370, 0x7683B0, 0x7683D0, 0x7683F0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
	const Mod MOD_095 = { "siren", 95, 0x739D80, 0x7475D0,
		{ 0x0, 0x0, 0x1533010, 0x15339D0, 0x1533D18, 0x1D969A8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x257F8A0, 0x257F8A4, 0x257F988, 0x257F9AC, 0x257F9B0, 0x257FA90, 0x2585AE0, 0x2585E38, 0x2585EF0, 0x258A010, 0x258A020, 0x258A024, 0x258BA90, 0x258BE20, 0x258BE30, 0x258BFA0, 0x258BFA4, 0x258BFA8, 0x258EAA0, 0x258EC40, 0x258EC50, 0x258EC54, 0x258EF98, 0x258FB40, 0x258FB48, 0x258FB68, 0x258FB70, 0x258FB78 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x73A0D0, 0x73A0E0, 0x73A0F0, 0x73A170, 0x73A1A0, 0x73A220, 0x73A3E0, 0x73A580, 0x73A5B0, 0x73A5E0, 0x73A5F0, 0x73A600, 0x73A610, 0x73A620, 0x73A660, 0x73A6D0, 0x73A950, 0x73AAE0, 0x73AB20, 0x73AE10, 0x73B160, 0x73B430, 0x73B520, 0x73B570, 0x73B640, 0x73B770, 0x73B7E0, 0x73C3B0, 0x73F180, 0x73F1D0, 0x73F1E0, 0x73F220, 0x73F240, 0x73FB40, 0x73FC20, 0x73FDC0, 0x73FE90, 0x73FFB0, 0x740210, 0x7407C0, 0x740910, 0x740F70, 0x740FC0, 0x741050, 0x741120, 0x7418B0, 0x7419C0, 0x741A40, 0x741A70, 0x742290, 0x7422C0, 0x7423A0, 0x7424C0, 0x742610, 0x742CA0, 0x742CD0, 0x742EB0, 0x742FF0, 0x743720, 0x743C20, 0x747440, 0x7474B0, 0x7474D0, 0x7474F0, 0x747500, 0x747540, 0x747550, 0x747560, 0x747590, 0x7475A0, 0x7475C0 } };
	const Mod MOD_096 = { "minimog", 96, 0x731DC0, 0x739D80,
		{ 0x152BBA0, 0x1532F80, 0x152BB90, 0x152BC00, 0x0, 0x1D969A8, 0x257B748, 0x257F4D4, 0x257F698, 0x257F720, 0x257F728, 0x257F730, 0x257B740, 0x0, 0x0, 0x0, 0x257B850, 0x0, 0x257CF20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x257E258, 0x0, 0x257E268, 0x0, 0x0, 0x257E3E0, 0x257F4D8, 0x0, 0x257F688, 0x257F68C, 0x0, 0x257F704, 0x0, 0x257F71C, 0x0, 0x257F72C },
		{ 0x732160, 0x732AC0, 0x7332C0, 0x733310, 0x733340, 0x733370, 0x7333A0, 0x7333D0, 0x733400, 0x733430, 0x733460, 0x733490, 0x7334C0, 0x7334F0, 0x733510, 0x733530, 0x733560, 0x7348D0, 0x7353B0, 0x735440, 0x7354A0, 0x7355E0, 0x735720, 0x735930, 0x735BB0, 0x735DF0, 0x736090, 0x7362C0, 0x736580, 0x7367E0, 0x736AC0, 0x736C00, 0x736C40, 0x737ED0, 0x737F20, 0x737F50, 0x737FD0, 0x7386C0, 0x738710, 0x7387B0, 0x738870, 0x7390C0, 0x7394C0, 0x739550, 0x739960, 0x0, 0x0, 0x0, 0x0, 0x7321D0, 0x732250, 0x7323F0, 0x732590, 0x7325C0, 0x7325F0, 0x732600, 0x732610, 0x732620, 0x732630, 0x732670, 0x7326E0, 0x733A80, 0x0, 0x732B10, 0x732E00, 0x733150, 0x0, 0x0, 0x0, 0x732920, 0x733550, 0x7329E0, 0x0, 0x733700, 0x733730, 0x733760, 0x733780, 0x7337C0, 0x734A80, 0x734B60, 0x734D00, 0x734DD0, 0x734EF0, 0x735150, 0x736B80, 0x736CD0, 0x737330, 0x737380, 0x737410, 0x7374E0, 0x737C70, 0x737D80, 0x737E00, 0x737E30, 0x738650, 0x738680, 0x738760, 0x738880, 0x7389D0, 0x739060, 0x739090, 0x739270, 0x7393B0, 0x0, 0x0, 0x739BF0, 0x739C60, 0x739C80, 0x739CA0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
	const Mod MOD_097 = { "chocofire", 97, 0x729A40, 0x731DC0,
		{ 0x0, 0x152BB10, 0x152B5B8, 0x152BA30, 0x152BAC8, 0x1D969A8, 0x2575270, 0x257AFB4, 0x257B178, 0x257B5C0, 0x257B5C8, 0x0, 0x2575268, 0x0, 0x0, 0x0, 0x2575378, 0x0, 0x2578028, 0x257802C, 0x0, 0x0, 0x0, 0x0, 0x2579D38, 0x0, 0x2579D48, 0x0, 0x0, 0x2579EC0, 0x257AFB8, 0x0, 0x0, 0x257B16C, 0x0, 0x0, 0x0, 0x257B5BC, 0x257B5C4, 0x257B5CC },
		{ 0x729DB0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x72B780, 0x72B8C0, 0x72BA00, 0x72BC10, 0x72BE90, 0x72C0D0, 0x72C370, 0x72C5A0, 0x72C860, 0x72CAC0, 0x0, 0x0, 0x0, 0x0, 0x72E4D0, 0x72E500, 0x72E580, 0x72EC70, 0x72ECC0, 0x72ED60, 0x72EE20, 0x72F670, 0x72FA70, 0x0, 0x730A10, 0x0, 0x0, 0x0, 0x0, 0x729E20, 0x729EA0, 0x72A050, 0x72A1F0, 0x72A220, 0x72A250, 0x72A260, 0x72A270, 0x72A280, 0x72A290, 0x72A2D0, 0x72A340, 0x730590, 0x730490, 0x72FC90, 0x72FF80, 0x7302D0, 0x0, 0x0, 0x0, 0x730EC0, 0x730550, 0x730E80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x72AD60, 0x72AE40, 0x72AFE0, 0x72B0B0, 0x72B1D0, 0x72B430, 0x72CE60, 0x0, 0x72D610, 0x72D660, 0x0, 0x0, 0x0, 0x0, 0x72E3B0, 0x72E3E0, 0x72EC00, 0x72EC30, 0x72ED10, 0x72EE30, 0x72EF80, 0x72F610, 0x72F640, 0x72F820, 0x72F960, 0x0, 0x730DA0, 0x731C30, 0x731CA0, 0x731CC0, 0x731CE0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
	const Mod MOD_098 = { "chocoflare", 98, 0x721840, 0x729A40,
		{ 0x0, 0x152B4F0, 0x152AF30, 0x152B3C8, 0x152B460, 0x1D969A8, 0x256E360, 0x2574D34, 0x2574EF8, 0x25750E8, 0x25750F0, 0x0, 0x256E358, 0x0, 0x0, 0x0, 0x256E468, 0x0, 0x2571118, 0x257111C, 0x0, 0x0, 0x0, 0x0, 0x2573AB8, 0x0, 0x2573AC8, 0x0, 0x0, 0x2573C40, 0x2574D38, 0x0, 0x2574EE8, 0x2574EEC, 0x0, 0x25750C8, 0x0, 0x25750E4, 0x25750EC, 0x25750F4 },
		{ 0x721BB0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7229B0, 0x723490, 0x723520, 0x723580, 0x7236C0, 0x723800, 0x723A10, 0x723C90, 0x723ED0, 0x724170, 0x7243A0, 0x724660, 0x7248C0, 0x724BA0, 0x724CE0, 0x724D20, 0x725FB0, 0x726000, 0x726030, 0x7260B0, 0x7267A0, 0x7267F0, 0x726890, 0x726950, 0x7271A0, 0x7275A0, 0x727630, 0x728580, 0x0, 0x0, 0x0, 0x0, 0x721C20, 0x721CA0, 0x721E50, 0x721FF0, 0x722020, 0x722050, 0x722060, 0x722070, 0x722080, 0x722090, 0x7220D0, 0x722140, 0x728100, 0x727FD0, 0x7277D0, 0x727AC0, 0x727E10, 0x0, 0x0, 0x0, 0x728A30, 0x7280C0, 0x7289F0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x722B60, 0x722C40, 0x722DE0, 0x722EB0, 0x722FD0, 0x723230, 0x724C60, 0x724DB0, 0x725410, 0x725460, 0x7254F0, 0x7255C0, 0x725D50, 0x725E60, 0x725EE0, 0x725F10, 0x726730, 0x726760, 0x726840, 0x726960, 0x726AB0, 0x727140, 0x727170, 0x727350, 0x727490, 0x0, 0x728910, 0x7298B0, 0x729920, 0x729940, 0x729960, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
	const Mod MOD_099 = { "chocometeor", 99, 0x717D10, 0x721840,
		{ 0x0, 0x0, 0x1529FA0, 0x152AC40, 0x152ACD8, 0x1D969A8, 0x2565A38, 0x256D6EC, 0x256D9F0, 0x256E1D8, 0x256E1E0, 0x0, 0x2565A30, 0x2565A34, 0x2565B18, 0x2565B3C, 0x2565B40, 0x2565C20, 0x2569330, 0x2569334, 0x2569338, 0x256B9D8, 0x256B9E8, 0x256B9EC, 0x256C1E0, 0x256C470, 0x256C480, 0x256C5F0, 0x256C5F4, 0x256C5F8, 0x256D6F0, 0x256D9D0, 0x256D9E0, 0x256D9E4, 0x0, 0x256E1B8, 0x256E1C0, 0x256E1D4, 0x256E1DC, 0x256E1E4 },
		{ 0x7180C0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x718F00, 0x7199E0, 0x719A70, 0x719AD0, 0x719C10, 0x719D50, 0x719F60, 0x71A1E0, 0x71A420, 0x71A6C0, 0x71A8F0, 0x71ABB0, 0x71AE10, 0x71B0F0, 0x71B230, 0x71B270, 0x71C500, 0x71C550, 0x71C580, 0x71C600, 0x71CCF0, 0x71CD40, 0x71CDE0, 0x71CEA0, 0x71D6F0, 0x71DAF0, 0x71DB80, 0x71EC30, 0x718060, 0x718070, 0x718080, 0x718100, 0x718130, 0x7181B0, 0x718370, 0x718510, 0x718540, 0x718570, 0x718580, 0x718590, 0x7185A0, 0x7185B0, 0x7185F0, 0x718660, 0x71E7C0, 0x71E520, 0x71DD20, 0x71E010, 0x71E360, 0x0, 0x0, 0x0, 0x71F110, 0x0, 0x71F0D0, 0x0, 0x71F380, 0x71F3A0, 0x71F450, 0x71F470, 0x71F4B0, 0x7190B0, 0x719190, 0x719330, 0x719400, 0x719520, 0x719780, 0x71B1B0, 0x71B300, 0x71B960, 0x71B9B0, 0x71BA40, 0x71BB10, 0x71C2A0, 0x71C3B0, 0x71C430, 0x71C460, 0x71CC80, 0x71CCB0, 0x71CD90, 0x71CEB0, 0x71D000, 0x71D690, 0x71D6C0, 0x71D8A0, 0x71D9E0, 0x0, 0x71EFE0, 0x7216B0, 0x721720, 0x721740, 0x721760, 0x721770, 0x7217B0, 0x7217C0, 0x7217D0, 0x721800, 0x721810, 0x721830 } };
	const Mod MOD_100 = { "chocobocle", 100, 0x70D370, 0x717D10,
		{ 0x0, 0x1529ED0, 0x15297F0, 0x1529CD0, 0x1529D68, 0x1D969A8, 0x255EAD0, 0x25653D4, 0x25656D8, 0x2565880, 0x2565888, 0x0, 0x255EAC8, 0x255EACC, 0x255EBB0, 0x255EBD4, 0x255EBD8, 0x255ECB8, 0x2561888, 0x256188C, 0x2561890, 0x2563420, 0x2563430, 0x2563434, 0x2563EC8, 0x2564158, 0x2564168, 0x25642D8, 0x25642DC, 0x25642E0, 0x25653D8, 0x25656B8, 0x25656C8, 0x25656CC, 0x0, 0x2565860, 0x2565868, 0x256587C, 0x2565884, 0x256588C },
		{ 0x70ECF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x70FB80, 0x710660, 0x7106F0, 0x710750, 0x710890, 0x7109D0, 0x710BE0, 0x710E60, 0x7110A0, 0x711340, 0x711570, 0x711830, 0x711A90, 0x711D70, 0x711EB0, 0x711EF0, 0x713180, 0x7131D0, 0x713200, 0x713280, 0x713970, 0x7139C0, 0x713A60, 0x713B20, 0x714370, 0x714770, 0x714800, 0x7158B0, 0x70EC90, 0x70ECA0, 0x70ECB0, 0x70ED30, 0x70ED60, 0x70EDE0, 0x70EF90, 0x70F130, 0x70F160, 0x70F190, 0x70F1A0, 0x70F1B0, 0x70F1C0, 0x70F1D0, 0x70F210, 0x70F280, 0x715430, 0x7151C0, 0x7149C0, 0x714CB0, 0x715000, 0x0, 0x0, 0x0, 0x715300, 0x7153F0, 0x715D20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x70FD30, 0x70FE10, 0x70FFB0, 0x710080, 0x7101A0, 0x710400, 0x711E30, 0x711F80, 0x7125E0, 0x712630, 0x7126C0, 0x712790, 0x712F20, 0x713030, 0x7130B0, 0x7130E0, 0x713900, 0x713930, 0x713A10, 0x713B30, 0x713C80, 0x714310, 0x714340, 0x714520, 0x714660, 0x0, 0x715C40, 0x717B80, 0x717BF0, 0x717C10, 0x717C30, 0x717C40, 0x717C80, 0x717C90, 0x717CA0, 0x717CD0, 0x717CE0, 0x717D00 } };
	struct AddrPort { uint32_t addr; void *port; const char *name; };
	static const AddrPort ENGINE_PORTS_090[] = {
		{ 0x762760, (void *)a_7322A0, "090 MAG_090_sub_762760" },
		{ 0x762DA0, (void *)a_732A00, "090 sub_762DA0" },
		{ 0x7678D0, (void *)a_733950, "090 sub_7678D0" },
		{ 0x763C50, (void *)a_733AC0, "090 sub_763C50" },
		{ 0x763920, (void *)a_733B70, "090 sub_763920" },
		{ 0x762630, (void *)a_73A0D0, "090 MAG_090_sub_762630" },
		{ 0x762640, (void *)a_73A0D0, "090 MAG_090_sub_762640" },
		{ 0x768440, (void *)a_73A0D0, "090 MAG_090_sub_768440" },
		{ 0x762660, (void *)a_73A170, "090 MAG_090_sub_762660" },
		{ 0x762690, (void *)a_73A1A0, "090 MAG_090_sub_762690" },
		{ 0x762850, (void *)a_73A380, "090 MAG_090_sub_762850" },
		{ 0x767B40, (void *)a_73A380, "090 sub_767B40" },
		{ 0x7681B0, (void *)a_73A380, "090 sub_7681B0" },
		{ 0x7682A0, (void *)a_73A380, "090 sub_7682A0" },
		{ 0x7628B0, (void *)a_73A3E0, "090 sub_7628B0" },
		{ 0x762A50, (void *)a_73A580, "090 sub_762A50" },
		{ 0x762A80, (void *)a_73A5B0, "090 sub_762A80" },
		{ 0x762AB0, (void *)a_73A5E0, "090 sub_762AB0" },
		{ 0x762AC0, (void *)a_73A5E0, "090 sub_762AC0" },
		{ 0x762AD0, (void *)a_73A5E0, "090 sub_762AD0" },
		{ 0x762AE0, (void *)a_73A5E0, "090 sub_762AE0" },
		{ 0x762AF0, (void *)a_73A620, "090 sub_762AF0" },
		{ 0x762B30, (void *)a_73A660, "090 sub_762B30" },
		{ 0x762BA0, (void *)a_73A6D0, "090 nullsub_1247" },
		{ 0x763B50, (void *)a_73A6D0, "090 nullsub_1248" },
		{ 0x763B60, (void *)a_73A6D0, "090 nullsub_1249" },
		{ 0x763D40, (void *)a_73A6D0, "090 nullsub_1250" },
		{ 0x763D50, (void *)a_73A6D0, "090 nullsub_1251" },
		{ 0x7667A0, (void *)a_73A6D0, "090 nullsub_1252" },
		{ 0x766FA0, (void *)a_73A6D0, "090 nullsub_1253" },
		{ 0x7670D0, (void *)a_73A6D0, "090 nullsub_1254" },
		{ 0x767450, (void *)a_73A6D0, "090 nullsub_1255" },
		{ 0x7674D0, (void *)a_73A6D0, "090 nullsub_1256" },
		{ 0x767C60, (void *)a_73A6D0, "090 nullsub_1257" },
		{ 0x767DE0, (void *)a_73A6D0, "090 nullsub_1258" },
		{ 0x7681A0, (void *)a_73A6D0, "090 nullsub_1259" },
		{ 0x768290, (void *)a_73A6D0, "090 nullsub_1260" },
		{ 0x768360, (void *)a_73A6D0, "090 nullsub_1261" },
		{ 0x7683F0, (void *)a_73A6D0, "090 nullsub_1262" },
		{ 0x7684C0, (void *)a_73A6D0, "090 nullsub_1246" },
		{ 0x763BF0, (void *)a_73A930, "090 sub_763BF0" },
		{ 0x763C10, (void *)a_73A950, "090 sub_763C10" },
		{ 0x762EC0, (void *)a_73AAE0, "090 sub_762EC0" },
		{ 0x762E70, (void *)a_73AB00, "090 sub_762E70" },
		{ 0x762EE0, (void *)a_73AB20, "090 sub_762EE0" },
		{ 0x7631D0, (void *)a_73AE10, "090 sub_7631D0" },
		{ 0x763520, (void *)a_73B160, "090 sub_763520" },
		{ 0x7639C0, (void *)a_73B2D0, "090 sub_7639C0" },
		{ 0x763A80, (void *)a_73B2D0, "090 sub_763A80" },
		{ 0x763A00, (void *)a_73B350, "090 sub_763A00" },
		{ 0x7636C0, (void *)a_73B3B0, "090 sub_7636C0" },
		{ 0x763740, (void *)a_73B430, "090 sub_763740" },
		{ 0x7637B0, (void *)a_73B4B0, "090 sub_7637B0" },
		{ 0x763820, (void *)a_73B520, "090 sub_763820" },
		{ 0x763870, (void *)a_73B570, "090 sub_763870" },
		{ 0x763C70, (void *)a_73B620, "090 sub_763C70" },
		{ 0x763940, (void *)a_73B640, "090 sub_763940" },
		{ 0x763B30, (void *)a_73B750, "090 sub_763B30" },
		{ 0x763B70, (void *)a_73B790, "090 sub_763B70" },
		{ 0x763BB0, (void *)a_73B7E0, "090 sub_763BB0" },
		{ 0x7640E0, (void *)a_73B8A0, "090 sub_7640E0" },
		{ 0x767ED0, (void *)a_73BB60, "090 sub_767ED0" },
		{ 0x764130, (void *)a_73C280, "090 sub_764130" },
		{ 0x765600, (void *)a_73D770, "090 sub_765600" },
		{ 0x765850, (void *)a_73D9C0, "090 sub_765850" },
		{ 0x7670E0, (void *)a_73F110, "090 sub_7670E0" },
		{ 0x766900, (void *)a_7435E0, "090 InitEffectSequenceFromData_c5" },
		{ 0x766A40, (void *)a_743720, "090 sub_766A40" },
		{ 0x766F80, (void *)a_743C00, "090 sub_766F80" },
		{ 0x766F50, (void *)a_743C20, "090 sub_766F50" },
		{ 0x767610, (void *)a_746C10, "090 sub_767610" },
		{ 0x7683B0, (void *)a_7474B0, "090 MAG_090_sub_7683B0" },
		{ 0x7683D0, (void *)a_7474D0, "090 MAG_090_sub_7683D0" },
		{ 0x768400, (void *)a_747500, "090 MAG_090_sub_768400" },
		{ 0x768450, (void *)a_747550, "090 MAG_090_sub_768450" },
		{ 0x768460, (void *)a_747560, "090 MAG_090_sub_768460" },
		{ 0x768490, (void *)a_747590, "090 MAG_090_sub_768490" },
		{ 0x763D20, (void *)a_7475A0, "090 sub_763D20" },
		{ 0x7684A0, (void *)a_7475A0, "090 MAG_090_sub_7684A0" },
		{ 0, nullptr, nullptr }
	};
	static const AddrPort ENGINE_PORTS_095[] = {
		{ 0x739F40, (void *)a_739F40, "095 GF_095Siren_SequenceTick" },
		{ 0x73A0D0, (void *)a_73A0D0, "095 MAG_095_sub_73A0D0" },
		{ 0x73A0E0, (void *)a_73A0D0, "095 MAG_095_sub_73A0E0" },
		{ 0x7432B0, (void *)a_73A0D0, "095 sub_7432B0" },
		{ 0x743D00, (void *)a_73A0D0, "095 sub_743D00" },
		{ 0x7440F0, (void *)a_73A0D0, "095 sub_7440F0" },
		{ 0x7444E0, (void *)a_73A0D0, "095 sub_7444E0" },
		{ 0x747540, (void *)a_73A0D0, "095 MAG_095_sub_747540" },
		{ 0x73A170, (void *)a_73A170, "095 MAG_095_sub_73A170" },
		{ 0x73A1A0, (void *)a_73A1A0, "095 MAG_095_sub_73A1A0" },
		{ 0x73A380, (void *)a_73A380, "095 MAG_095_sub_73A380" },
		{ 0x73F800, (void *)a_73A380, "095 sub_73F800" },
		{ 0x73F900, (void *)a_73A380, "095 sub_73F900" },
		{ 0x743250, (void *)a_73A380, "095 sub_743250" },
		{ 0x7447C0, (void *)a_73A380, "095 sub_7447C0" },
		{ 0x745180, (void *)a_73A380, "095 sub_745180" },
		{ 0x745250, (void *)a_73A380, "095 sub_745250" },
		{ 0x746920, (void *)a_73A380, "095 sub_746920" },
		{ 0x746A40, (void *)a_73A380, "095 sub_746A40" },
		{ 0x73A3E0, (void *)a_73A3E0, "095 sub_73A3E0" },
		{ 0x73A580, (void *)a_73A580, "095 sub_73A580" },
		{ 0x73A5B0, (void *)a_73A5B0, "095 sub_73A5B0" },
		{ 0x73A5E0, (void *)a_73A5E0, "095 sub_73A5E0" },
		{ 0x73A5F0, (void *)a_73A5E0, "095 sub_73A5F0" },
		{ 0x73A600, (void *)a_73A5E0, "095 sub_73A600" },
		{ 0x73A610, (void *)a_73A5E0, "095 sub_73A610" },
		{ 0x73A620, (void *)a_73A620, "095 sub_73A620" },
		{ 0x73A660, (void *)a_73A660, "095 sub_73A660" },
		{ 0x73A6D0, (void *)a_73A6D0, "095 nullsub_1171" },
		{ 0x73B770, (void *)a_73A6D0, "095 nullsub_1172" },
		{ 0x73B780, (void *)a_73A6D0, "095 nullsub_1173" },
		{ 0x73BCD0, (void *)a_73A6D0, "095 nullsub_1174" },
		{ 0x73BCE0, (void *)a_73A6D0, "095 nullsub_1175" },
		{ 0x73EE40, (void *)a_73A6D0, "095 nullsub_1176" },
		{ 0x73EF50, (void *)a_73A6D0, "095 nullsub_1177" },
		{ 0x73F100, (void *)a_73A6D0, "095 nullsub_1178" },
		{ 0x73F240, (void *)a_73A6D0, "095 nullsub_1179" },
		{ 0x73F360, (void *)a_73A6D0, "095 nullsub_1180" },
		{ 0x73F4F0, (void *)a_73A6D0, "095 nullsub_1181" },
		{ 0x73F690, (void *)a_73A6D0, "095 nullsub_1182" },
		{ 0x73F7F0, (void *)a_73A6D0, "095 nullsub_1183" },
		{ 0x73F8F0, (void *)a_73A6D0, "095 nullsub_1184" },
		{ 0x743240, (void *)a_73A6D0, "095 nullsub_1185" },
		{ 0x743C50, (void *)a_73A6D0, "095 nullsub_1186" },
		{ 0x743C60, (void *)a_73A6D0, "095 nullsub_1187" },
		{ 0x744040, (void *)a_73A6D0, "095 nullsub_1188" },
		{ 0x744050, (void *)a_73A6D0, "095 nullsub_1189" },
		{ 0x744430, (void *)a_73A6D0, "095 nullsub_1190" },
		{ 0x744440, (void *)a_73A6D0, "095 nullsub_1191" },
		{ 0x7447A0, (void *)a_73A6D0, "095 nullsub_1192" },
		{ 0x7447B0, (void *)a_73A6D0, "095 nullsub_1193" },
		{ 0x744C20, (void *)a_73A6D0, "095 nullsub_1194" },
		{ 0x744F70, (void *)a_73A6D0, "095 nullsub_1195" },
		{ 0x744F80, (void *)a_73A6D0, "095 nullsub_1196" },
		{ 0x745230, (void *)a_73A6D0, "095 nullsub_1197" },
		{ 0x745240, (void *)a_73A6D0, "095 nullsub_1198" },
		{ 0x745640, (void *)a_73A6D0, "095 nullsub_1199" },
		{ 0x745690, (void *)a_73A6D0, "095 nullsub_1200" },
		{ 0x745AB0, (void *)a_73A6D0, "095 nullsub_1201" },
		{ 0x745AC0, (void *)a_73A6D0, "095 nullsub_1202" },
		{ 0x7467A0, (void *)a_73A6D0, "095 nullsub_1203" },
		{ 0x746910, (void *)a_73A6D0, "095 nullsub_1204" },
		{ 0x746A30, (void *)a_73A6D0, "095 nullsub_1205" },
		{ 0x746B50, (void *)a_73A6D0, "095 nullsub_1206" },
		{ 0x747150, (void *)a_73A6D0, "095 nullsub_1207" },
		{ 0x747360, (void *)a_73A6D0, "095 nullsub_1208" },
		{ 0x747430, (void *)a_73A6D0, "095 nullsub_1209" },
		{ 0x7474F0, (void *)a_73A6D0, "095 nullsub_1210" },
		{ 0x7475C0, (void *)a_73A6D0, "095 nullsub_1170" },
		{ 0x73A720, (void *)a_73A720, "095 sub_73A720" },
		{ 0x73A930, (void *)a_73A930, "095 sub_73A930" },
		{ 0x73B8E0, (void *)a_73A930, "095 sub_73B8E0" },
		{ 0x73BB00, (void *)a_73A930, "095 sub_73BB00" },
		{ 0x73A950, (void *)a_73A950, "095 sub_73A950" },
		{ 0x73AAE0, (void *)a_73AAE0, "095 sub_73AAE0" },
		{ 0x73AB00, (void *)a_73AB00, "095 sub_73AB00" },
		{ 0x73AB20, (void *)a_73AB20, "095 sub_73AB20" },
		{ 0x73AE10, (void *)a_73AE10, "095 sub_73AE10" },
		{ 0x73B160, (void *)a_73B160, "095 sub_73B160" },
		{ 0x73B2D0, (void *)a_73B2D0, "095 sub_73B2D0" },
		{ 0x73B350, (void *)a_73B350, "095 sub_73B350" },
		{ 0x73B3B0, (void *)a_73B3B0, "095 sub_73B3B0" },
		{ 0x73B430, (void *)a_73B430, "095 sub_73B430" },
		{ 0x73B4B0, (void *)a_73B4B0, "095 sub_73B4B0" },
		{ 0x73B520, (void *)a_73B520, "095 sub_73B520" },
		{ 0x73B570, (void *)a_73B570, "095 sub_73B570" },
		{ 0x73B620, (void *)a_73B620, "095 sub_73B620" },
		{ 0x73B640, (void *)a_73B640, "095 sub_73B640" },
		{ 0x73B750, (void *)a_73B750, "095 sub_73B750" },
		{ 0x73B790, (void *)a_73B790, "095 sub_73B790" },
		{ 0x73B7E0, (void *)a_73B7E0, "095 sub_73B7E0" },
		{ 0x73B8A0, (void *)a_73B8A0, "095 sub_73B8A0" },
		{ 0x73B8C0, (void *)a_73B8C0, "095 sub_73B8C0" },
		{ 0x73B9F0, (void *)a_73B9F0, "095 sub_73B9F0" },
		{ 0x73BA90, (void *)a_73BA90, "095 sub_73BA90" },
		{ 0x73BB60, (void *)a_73BB60, "095 sub_73BB60" },
		{ 0x73BBD0, (void *)a_73BBD0, "095 sub_73BBD0" },
		{ 0x73BC40, (void *)a_73BC40, "095 sub_73BC40" },
		{ 0x73BCB0, (void *)a_73BCB0, "095 sub_73BCB0" },
		{ 0x73C100, (void *)a_73C100, "095 sub_73C100" },
		{ 0x73C280, (void *)a_73C280, "095 sub_73C280" },
		{ 0x73D770, (void *)a_73D770, "095 sub_73D770" },
		{ 0x73D9C0, (void *)a_73D9C0, "095 sub_73D9C0" },
		{ 0x73F110, (void *)a_73F110, "095 sub_73F110" },
		{ 0x73F370, (void *)a_73F110, "095 sub_73F370" },
		{ 0x73F500, (void *)a_73F110, "095 sub_73F500" },
		{ 0x73F990, (void *)a_73F990, "095 sub_73F990" },
		{ 0x73FC20, (void *)a_73FC20, "095 sub_73FC20" },
		{ 0x73FE90, (void *)a_73FE90, "095 sub_73FE90" },
		{ 0x73FFB0, (void *)a_73FFB0, "095 sub_73FFB0" },
		{ 0x740210, (void *)a_740210, "095 sub_740210" },
		{ 0x740700, (void *)a_740700, "095 sub_740700" },
		{ 0x7407C0, (void *)a_7407C0, "095 sub_7407C0" },
		{ 0x740880, (void *)a_740880, "095 sub_740880" },
		{ 0x740910, (void *)a_740910, "095 sub_740910" },
		{ 0x740F70, (void *)a_740F70, "095 sub_740F70" },
		{ 0x740FC0, (void *)a_740FC0, "095 sub_740FC0" },
		{ 0x741050, (void *)a_741050, "095 sub_741050" },
		{ 0x741120, (void *)a_741120, "095 sub_741120" },
		{ 0x7419C0, (void *)a_7419C0, "095 sub_7419C0" },
		{ 0x741A40, (void *)a_741A40, "095 sub_741A40" },
		{ 0x741A70, (void *)a_741A70, "095 sub_741A70" },
		{ 0x741B60, (void *)a_741B60, "095 sub_741B60" },
		{ 0x742290, (void *)a_742290, "095 au_re__rand_8" },
		{ 0x7422C0, (void *)a_7422C0, "095 sub_7422C0" },
		{ 0x742300, (void *)a_742300, "095 sub_742300" },
		{ 0x742350, (void *)a_742350, "095 sub_742350" },
		{ 0x7423A0, (void *)a_7423A0, "095 au_re__rand_8_0" },
		{ 0x7424B0, (void *)a_7424B0, "095 sub_7424B0" },
		{ 0x7424C0, (void *)a_7424C0, "095 sub_7424C0" },
		{ 0x742CA0, (void *)a_742CA0, "095 sub_742CA0" },
		{ 0x742CD0, (void *)a_742CD0, "095 sub_742CD0" },
		{ 0x742D00, (void *)a_742D00, "095 sub_742D00" },
		{ 0x742EB0, (void *)a_742EB0, "095 sub_742EB0" },
		{ 0x742FF0, (void *)a_742FF0, "095 sub_742FF0" },
		{ 0x743100, (void *)a_743100, "095 sub_743100" },
		{ 0x743190, (void *)a_743190, "095 sub_743190" },
		{ 0x7435E0, (void *)a_7435E0, "095 InitEffectSequenceFromData_c2" },
		{ 0x743720, (void *)a_743720, "095 sub_743720" },
		{ 0x743C00, (void *)a_743C00, "095 sub_743C00" },
		{ 0x746780, (void *)a_743C00, "095 sub_746780" },
		{ 0x747130, (void *)a_743C00, "095 sub_747130" },
		{ 0x747340, (void *)a_743C00, "095 sub_747340" },
		{ 0x743C20, (void *)a_743C20, "095 sub_743C20" },
		{ 0x7458E0, (void *)a_7458E0, "095 sub_7458E0" },
		{ 0x746980, (void *)a_746980, "095 sub_746980" },
		{ 0x746A10, (void *)a_746A10, "095 sub_746A10" },
		{ 0x746AE0, (void *)a_746AE0, "095 sub_746AE0" },
		{ 0x746C10, (void *)a_746C10, "095 GF_095Siren_DrawModel" },
		{ 0x747400, (void *)a_747400, "095 sub_747400" },
		{ 0x747440, (void *)a_747440, "095 MAG_095_sub_747440" },
		{ 0x7474B0, (void *)a_7474B0, "095 MAG_095_sub_7474B0" },
		{ 0x7474D0, (void *)a_7474D0, "095 MAG_095_sub_7474D0" },
		{ 0x747500, (void *)a_747500, "095 MAG_095_sub_747500" },
		{ 0x747550, (void *)a_747550, "095 MAG_095_sub_747550" },
		{ 0x747560, (void *)a_747560, "095 MAG_095_sub_747560" },
		{ 0x747590, (void *)a_747590, "095 MAG_095_sub_747590" },
		{ 0x7475A0, (void *)a_7475A0, "095 MAG_095_sub_7475A0" },
		{ 0, nullptr, nullptr }
	};
	static const AddrPort ENGINE_PORTS_096[] = {
		{ 0x732120, (void *)a_732120, "096 MAG_096_sub_732120" },
		{ 0x732160, (void *)a_732160, "096 MAG_096_sub_732160" },
		{ 0x7322A0, (void *)a_7322A0, "096 MAG_096_sub_7322A0" },
		{ 0x732A00, (void *)a_732A00, "096 sub_732A00" },
		{ 0x7334C0, (void *)a_7334C0, "096 sub_7334C0" },
		{ 0x7335D0, (void *)a_7335D0, "096 sub_7335D0" },
		{ 0x733760, (void *)a_733760, "096 sub_733760" },
		{ 0x733950, (void *)a_733950, "096 sub_733950" },
		{ 0x733AC0, (void *)a_733AC0, "096 sub_733AC0" },
		{ 0x733AE0, (void *)a_733AE0, "096 sub_733AE0" },
		{ 0x733B70, (void *)a_733B70, "096 sub_733B70" },
		{ 0x7340A0, (void *)a_7340A0, "096 sub_7340A0" },
		{ 0x7348A0, (void *)a_7348A0, "096 sub_7348A0" },
		{ 0x7353B0, (void *)a_7353B0, "096 sub_7353B0" },
		{ 0x735440, (void *)a_735440, "096 sub_735440" },
		{ 0x7354A0, (void *)a_7354A0, "096 sub_7354A0" },
		{ 0x7355E0, (void *)a_7355E0, "096 sub_7355E0" },
		{ 0x735BB0, (void *)a_735BB0, "096 sub_735BB0" },
		{ 0x735DF0, (void *)a_735DF0, "096 sub_735DF0" },
		{ 0x736C00, (void *)a_736C00, "096 sub_736C00" },
		{ 0x737ED0, (void *)a_737ED0, "096 sub_737ED0" },
		{ 0x737F50, (void *)a_737F50, "096 sub_737F50" },
		{ 0x737FD0, (void *)a_737FD0, "096 sub_737FD0" },
		{ 0x7387B0, (void *)a_7387B0, "096 sub_7387B0" },
		{ 0x7395E0, (void *)a_7395E0, "096 sub_7395E0" },
		{ 0x739890, (void *)a_739890, "096 sub_739890" },
		{ 0x739960, (void *)a_739960, "096 sub_739960" },
		{ 0x739AC0, (void *)a_739AC0, "096 sub_739AC0" },
		{ 0x739B40, (void *)a_739B40, "096 sub_739B40" },
		{ 0x732100, (void *)a_73A0D0, "096 MAG_096_sub_732100" },
		{ 0x732110, (void *)a_73A0D0, "096 MAG_096_sub_732110" },
		{ 0x739CF0, (void *)a_73A0D0, "096 MAG_096_sub_739CF0" },
		{ 0x7321A0, (void *)a_73A170, "096 MAG_096_sub_7321A0" },
		{ 0x7321D0, (void *)a_73A1A0, "096 MAG_096_sub_7321D0" },
		{ 0x732390, (void *)a_73A380, "096 MAG_096_sub_732390" },
		{ 0x733570, (void *)a_73A380, "096 sub_733570" },
		{ 0x733810, (void *)a_73A380, "096 sub_733810" },
		{ 0x733BE0, (void *)a_73A380, "096 sub_733BE0" },
		{ 0x733CE0, (void *)a_73A380, "096 sub_733CE0" },
		{ 0x734840, (void *)a_73A380, "096 sub_734840" },
		{ 0x7323F0, (void *)a_73A3E0, "096 sub_7323F0" },
		{ 0x732590, (void *)a_73A580, "096 sub_732590" },
		{ 0x7325C0, (void *)a_73A5B0, "096 sub_7325C0" },
		{ 0x7325F0, (void *)a_73A5E0, "096 sub_7325F0" },
		{ 0x732600, (void *)a_73A5E0, "096 sub_732600" },
		{ 0x732610, (void *)a_73A5E0, "096 sub_732610" },
		{ 0x732620, (void *)a_73A5E0, "096 sub_732620" },
		{ 0x732630, (void *)a_73A620, "096 sub_732630" },
		{ 0x732670, (void *)a_73A660, "096 sub_732670" },
		{ 0x7326E0, (void *)a_73A6D0, "096 nullsub_1156" },
		{ 0x733550, (void *)a_73A6D0, "096 nullsub_1166" },
		{ 0x733560, (void *)a_73A6D0, "096 nullsub_1167" },
		{ 0x733680, (void *)a_73A6D0, "096 nullsub_1168" },
		{ 0x7337C0, (void *)a_73A6D0, "096 nullsub_1169" },
		{ 0x733A50, (void *)a_73A6D0, "096 nullsub_1157" },
		{ 0x733CD0, (void *)a_73A6D0, "096 nullsub_1158" },
		{ 0x733DE0, (void *)a_73A6D0, "096 nullsub_1159" },
		{ 0x733E70, (void *)a_73A6D0, "096 nullsub_1160" },
		{ 0x733E80, (void *)a_73A6D0, "096 nullsub_1161" },
		{ 0x734830, (void *)a_73A6D0, "096 nullsub_1162" },
		{ 0x739600, (void *)a_73A6D0, "096 nullsub_1163" },
		{ 0x739BE0, (void *)a_73A6D0, "096 nullsub_1164" },
		{ 0x739CA0, (void *)a_73A6D0, "096 nullsub_1165" },
		{ 0x739D70, (void *)a_73A6D0, "096 nullsub_1155" },
		{ 0x732720, (void *)a_73A720, "096 sub_732720" },
		{ 0x733A60, (void *)a_73A930, "096 sub_733A60" },
		{ 0x733A80, (void *)a_73A950, "096 sub_733A80" },
		{ 0x7332F0, (void *)a_73AAE0, "096 sub_7332F0" },
		{ 0x732AF0, (void *)a_73AB00, "096 sub_732AF0" },
		{ 0x732B10, (void *)a_73AB20, "096 sub_732B10" },
		{ 0x732E00, (void *)a_73AE10, "096 sub_732E00" },
		{ 0x733150, (void *)a_73B160, "096 sub_733150" },
		{ 0x7334F0, (void *)a_73B2D0, "096 sub_7334F0" },
		{ 0x732920, (void *)a_73B640, "096 sub_732920" },
		{ 0x733530, (void *)a_73B750, "096 sub_733530" },
		{ 0x732900, (void *)a_73B790, "096 sub_732900" },
		{ 0x7329E0, (void *)a_73B7E0, "096 sub_7329E0" },
		{ 0x733B00, (void *)a_73B8A0, "096 sub_733B00" },
		{ 0x7338D0, (void *)a_73B9F0, "096 sub_7338D0" },
		{ 0x733D60, (void *)a_73BA90, "096 sub_733D60" },
		{ 0x733B50, (void *)a_73BC40, "096 sub_733B50" },
		{ 0x733DF0, (void *)a_73BC40, "096 sub_733DF0" },
		{ 0x733F20, (void *)a_73C100, "096 sub_733F20" },
		{ 0x733690, (void *)a_73F110, "096 sub_733690" },
		{ 0x7348D0, (void *)a_73F990, "096 sub_7348D0" },
		{ 0x734B60, (void *)a_73FC20, "096 sub_734B60" },
		{ 0x734DD0, (void *)a_73FE90, "096 sub_734DD0" },
		{ 0x734EF0, (void *)a_73FFB0, "096 sub_734EF0" },
		{ 0x735150, (void *)a_740210, "096 sub_735150" },
		{ 0x736AC0, (void *)a_740700, "096 sub_736AC0" },
		{ 0x736B80, (void *)a_7407C0, "096 sub_736B80" },
		{ 0x736C40, (void *)a_740880, "096 sub_736C40" },
		{ 0x736CD0, (void *)a_740910, "096 sub_736CD0" },
		{ 0x737330, (void *)a_740F70, "096 sub_737330" },
		{ 0x737380, (void *)a_740FC0, "096 sub_737380" },
		{ 0x737410, (void *)a_741050, "096 sub_737410" },
		{ 0x7374E0, (void *)a_741120, "096 sub_7374E0" },
		{ 0x737D80, (void *)a_7419C0, "096 sub_737D80" },
		{ 0x737E00, (void *)a_741A40, "096 sub_737E00" },
		{ 0x737E30, (void *)a_741A70, "096 sub_737E30" },
		{ 0x737F20, (void *)a_741B60, "096 sub_737F20" },
		{ 0x738650, (void *)a_742290, "096 au_re__rand_7" },
		{ 0x738680, (void *)a_7422C0, "096 sub_738680" },
		{ 0x7386C0, (void *)a_742300, "096 sub_7386C0" },
		{ 0x738710, (void *)a_742350, "096 sub_738710" },
		{ 0x738760, (void *)a_7423A0, "096 au_re__rand_7_0" },
		{ 0x738870, (void *)a_7424B0, "096 sub_738870" },
		{ 0x738880, (void *)a_7424C0, "096 sub_738880" },
		{ 0x739060, (void *)a_742CA0, "096 sub_739060" },
		{ 0x739090, (void *)a_742CD0, "096 sub_739090" },
		{ 0x7390C0, (void *)a_742D00, "096 sub_7390C0" },
		{ 0x739270, (void *)a_742EB0, "096 sub_739270" },
		{ 0x7393B0, (void *)a_742FF0, "096 sub_7393B0" },
		{ 0x7394C0, (void *)a_743100, "096 sub_7394C0" },
		{ 0x739550, (void *)a_743190, "096 sub_739550" },
		{ 0x733660, (void *)a_746A10, "096 sub_733660" },
		{ 0x733C60, (void *)a_746AE0, "096 sub_733C60" },
		{ 0x7396B0, (void *)a_746C10, "096 sub_7396B0" },
		{ 0x739BF0, (void *)a_747440, "096 MAG_096_sub_739BF0" },
		{ 0x739C60, (void *)a_7474B0, "096 MAG_096_sub_739C60" },
		{ 0x739C80, (void *)a_7474D0, "096 MAG_096_sub_739C80" },
		{ 0x739CB0, (void *)a_747500, "096 MAG_096_sub_739CB0" },
		{ 0x739D00, (void *)a_747550, "096 MAG_096_sub_739D00" },
		{ 0x739D10, (void *)a_747560, "096 MAG_096_sub_739D10" },
		{ 0x739D40, (void *)a_747590, "096 MAG_096_sub_739D40" },
		{ 0x733E50, (void *)a_7475A0, "096 sub_733E50" },
		{ 0x739D50, (void *)a_7475A0, "096 MAG_096_sub_739D50" },
		{ 0, nullptr, nullptr }
	};
	static const AddrPort ENGINE_PORTS_097[] = {
		{ 0x729D70, (void *)a_732120, "097 MAG_097_sub_729D70" },
		{ 0x729DB0, (void *)a_732160, "097 MAG_097_sub_729DB0" },
		{ 0x7304E0, (void *)a_7334C0, "097 sub_7304E0" },
		{ 0x731AB0, (void *)a_7335D0, "097 sub_731AB0" },
		{ 0x731050, (void *)a_733AC0, "097 sub_731050" },
		{ 0x731070, (void *)a_733B70, "097 sub_731070" },
		{ 0x7311C0, (void *)a_7340A0, "097 sub_7311C0" },
		{ 0x72B720, (void *)a_735440, "097 sub_72B720" },
		{ 0x72B780, (void *)a_7354A0, "097 sub_72B780" },
		{ 0x72B8C0, (void *)a_7355E0, "097 sub_72B8C0" },
		{ 0x72BE90, (void *)a_735BB0, "097 sub_72BE90" },
		{ 0x72C0D0, (void *)a_735DF0, "097 sub_72C0D0" },
		{ 0x72E480, (void *)a_737ED0, "097 sub_72E480" },
		{ 0x72E500, (void *)a_737F50, "097 sub_72E500" },
		{ 0x72E580, (void *)a_737FD0, "097 sub_72E580" },
		{ 0x72ED60, (void *)a_7387B0, "097 sub_72ED60" },
		{ 0x730940, (void *)a_739890, "097 sub_730940" },
		{ 0x730A10, (void *)a_739960, "097 sub_730A10" },
		{ 0x730B60, (void *)a_739AC0, "097 sub_730B60" },
		{ 0x730BA0, (void *)a_739B40, "097 sub_730BA0" },
		{ 0x729D50, (void *)a_73A0D0, "097 MAG_097_sub_729D50" },
		{ 0x729D60, (void *)a_73A0D0, "097 MAG_097_sub_729D60" },
		{ 0x731D30, (void *)a_73A0D0, "097 MAG_097_sub_731D30" },
		{ 0x729DF0, (void *)a_73A170, "097 MAG_097_sub_729DF0" },
		{ 0x729E20, (void *)a_73A1A0, "097 MAG_097_sub_729E20" },
		{ 0x729FF0, (void *)a_73A380, "097 MAG_097_sub_729FF0" },
		{ 0x72A660, (void *)a_73A380, "097 sub_72A660" },
		{ 0x72AB20, (void *)a_73A380, "097 sub_72AB20" },
		{ 0x731630, (void *)a_73A380, "097 sub_731630" },
		{ 0x731A50, (void *)a_73A380, "097 sub_731A50" },
		{ 0x731B40, (void *)a_73A380, "097 sub_731B40" },
		{ 0x72A050, (void *)a_73A3E0, "097 sub_72A050" },
		{ 0x72A1F0, (void *)a_73A580, "097 sub_72A1F0" },
		{ 0x72A220, (void *)a_73A5B0, "097 sub_72A220" },
		{ 0x72A250, (void *)a_73A5E0, "097 sub_72A250" },
		{ 0x72A260, (void *)a_73A5E0, "097 sub_72A260" },
		{ 0x72A270, (void *)a_73A5E0, "097 sub_72A270" },
		{ 0x72A280, (void *)a_73A5E0, "097 sub_72A280" },
		{ 0x72A290, (void *)a_73A620, "097 sub_72A290" },
		{ 0x72A2D0, (void *)a_73A660, "097 sub_72A2D0" },
		{ 0x72A340, (void *)a_73A6D0, "097 nullsub_1139" },
		{ 0x72AB10, (void *)a_73A6D0, "097 nullsub_1140" },
		{ 0x72FBB0, (void *)a_73A6D0, "097 nullsub_1141" },
		{ 0x730550, (void *)a_73A6D0, "097 nullsub_1142" },
		{ 0x730560, (void *)a_73A6D0, "097 nullsub_1143" },
		{ 0x730E20, (void *)a_73A6D0, "097 nullsub_1144" },
		{ 0x730FB0, (void *)a_73A6D0, "097 nullsub_1145" },
		{ 0x731590, (void *)a_73A6D0, "097 nullsub_1146" },
		{ 0x7316F0, (void *)a_73A6D0, "097 nullsub_1147" },
		{ 0x731760, (void *)a_73A6D0, "097 nullsub_1148" },
		{ 0x731770, (void *)a_73A6D0, "097 nullsub_1149" },
		{ 0x731940, (void *)a_73A6D0, "097 nullsub_1150" },
		{ 0x731A40, (void *)a_73A6D0, "097 nullsub_1151" },
		{ 0x731B30, (void *)a_73A6D0, "097 nullsub_1152" },
		{ 0x731C20, (void *)a_73A6D0, "097 nullsub_1153" },
		{ 0x731CE0, (void *)a_73A6D0, "097 nullsub_1154" },
		{ 0x731DB0, (void *)a_73A6D0, "097 nullsub_1138" },
		{ 0x72A380, (void *)a_73A720, "097 sub_72A380" },
		{ 0x730570, (void *)a_73A930, "097 sub_730570" },
		{ 0x730FC0, (void *)a_73A930, "097 sub_730FC0" },
		{ 0x731030, (void *)a_73A930, "097 sub_731030" },
		{ 0x730590, (void *)a_73A950, "097 sub_730590" },
		{ 0x730490, (void *)a_73AAE0, "097 sub_730490" },
		{ 0x72FC70, (void *)a_73AB00, "097 sub_72FC70" },
		{ 0x72FC90, (void *)a_73AB20, "097 sub_72FC90" },
		{ 0x72FF80, (void *)a_73AE10, "097 sub_72FF80" },
		{ 0x7302D0, (void *)a_73B160, "097 sub_7302D0" },
		{ 0x730440, (void *)a_73B2D0, "097 sub_730440" },
		{ 0x7304B0, (void *)a_73B350, "097 sub_7304B0" },
		{ 0x730EC0, (void *)a_73B640, "097 sub_730EC0" },
		{ 0x730530, (void *)a_73B750, "097 sub_730530" },
		{ 0x730E80, (void *)a_73B7E0, "097 sub_730E80" },
		{ 0x731010, (void *)a_73B8C0, "097 sub_731010" },
		{ 0x730610, (void *)a_73BB60, "097 sub_730610" },
		{ 0x731700, (void *)a_73BC40, "097 sub_731700" },
		{ 0x72A5C0, (void *)a_73C100, "097 sub_72A5C0" },
		{ 0x72ABB0, (void *)a_73F990, "097 sub_72ABB0" },
		{ 0x72AE40, (void *)a_73FC20, "097 sub_72AE40" },
		{ 0x72B0B0, (void *)a_73FE90, "097 sub_72B0B0" },
		{ 0x72B1D0, (void *)a_73FFB0, "097 sub_72B1D0" },
		{ 0x72B430, (void *)a_740210, "097 sub_72B430" },
		{ 0x72CDA0, (void *)a_740700, "097 sub_72CDA0" },
		{ 0x72CE60, (void *)a_7407C0, "097 sub_72CE60" },
		{ 0x72CFB0, (void *)a_740910, "097 sub_72CFB0" },
		{ 0x72D610, (void *)a_740F70, "097 sub_72D610" },
		{ 0x72D660, (void *)a_740FC0, "097 sub_72D660" },
		{ 0x72D6F0, (void *)a_741050, "097 sub_72D6F0" },
		{ 0x72E330, (void *)a_7419C0, "097 sub_72E330" },
		{ 0x72E3B0, (void *)a_741A40, "097 sub_72E3B0" },
		{ 0x72E3E0, (void *)a_741A70, "097 sub_72E3E0" },
		{ 0x72E4D0, (void *)a_741B60, "097 sub_72E4D0" },
		{ 0x72EC00, (void *)a_742290, "097 au_re__rand_6" },
		{ 0x72EC30, (void *)a_7422C0, "097 sub_72EC30" },
		{ 0x72EC70, (void *)a_742300, "097 sub_72EC70" },
		{ 0x72ECC0, (void *)a_742350, "097 sub_72ECC0" },
		{ 0x72ED10, (void *)a_7423A0, "097 au_re__rand_6_0" },
		{ 0x72EE20, (void *)a_7424B0, "097 sub_72EE20" },
		{ 0x72EE30, (void *)a_7424C0, "097 sub_72EE30" },
		{ 0x72F610, (void *)a_742CA0, "097 sub_72F610" },
		{ 0x72F640, (void *)a_742CD0, "097 sub_72F640" },
		{ 0x72F670, (void *)a_742D00, "097 sub_72F670" },
		{ 0x72F820, (void *)a_742EB0, "097 sub_72F820" },
		{ 0x72F960, (void *)a_742FF0, "097 sub_72F960" },
		{ 0x72FA70, (void *)a_743100, "097 sub_72FA70" },
		{ 0x72FB00, (void *)a_743190, "097 sub_72FB00" },
		{ 0x730E00, (void *)a_743C00, "097 sub_730E00" },
		{ 0x730DA0, (void *)a_743C20, "097 sub_730DA0" },
		{ 0x731BA0, (void *)a_746980, "097 sub_731BA0" },
		{ 0x730760, (void *)a_746C10, "097 sub_730760" },
		{ 0x730F80, (void *)a_747400, "097 sub_730F80" },
		{ 0x731C30, (void *)a_747440, "097 MAG_097_sub_731C30" },
		{ 0x731CA0, (void *)a_7474B0, "097 MAG_097_sub_731CA0" },
		{ 0x731CC0, (void *)a_7474D0, "097 MAG_097_sub_731CC0" },
		{ 0x731CF0, (void *)a_747500, "097 MAG_097_sub_731CF0" },
		{ 0x731D40, (void *)a_747550, "097 MAG_097_sub_731D40" },
		{ 0x731D50, (void *)a_747560, "097 MAG_097_sub_731D50" },
		{ 0x731D80, (void *)a_747590, "097 MAG_097_sub_731D80" },
		{ 0x731D90, (void *)a_7475A0, "097 MAG_097_sub_731D90" },
		{ 0, nullptr, nullptr }
	};
	static const AddrPort ENGINE_PORTS_098[] = {
		{ 0x721B70, (void *)a_732120, "098 MAG_098_sub_721B70" },
		{ 0x721BB0, (void *)a_732160, "098 MAG_098_sub_721BB0" },
		{ 0x728020, (void *)a_7334C0, "098 sub_728020" },
		{ 0x729730, (void *)a_7335D0, "098 sub_729730" },
		{ 0x728B40, (void *)a_733AC0, "098 sub_728B40" },
		{ 0x728BA0, (void *)a_733AC0, "098 sub_728BA0" },
		{ 0x728BC0, (void *)a_733B70, "098 sub_728BC0" },
		{ 0x728E60, (void *)a_7340A0, "098 sub_728E60" },
		{ 0x722980, (void *)a_7348A0, "098 sub_722980" },
		{ 0x723490, (void *)a_7353B0, "098 sub_723490" },
		{ 0x723520, (void *)a_735440, "098 sub_723520" },
		{ 0x723580, (void *)a_7354A0, "098 sub_723580" },
		{ 0x7236C0, (void *)a_7355E0, "098 sub_7236C0" },
		{ 0x723C90, (void *)a_735BB0, "098 sub_723C90" },
		{ 0x723ED0, (void *)a_735DF0, "098 sub_723ED0" },
		{ 0x724CE0, (void *)a_736C00, "098 sub_724CE0" },
		{ 0x725FB0, (void *)a_737ED0, "098 sub_725FB0" },
		{ 0x726030, (void *)a_737F50, "098 sub_726030" },
		{ 0x7260B0, (void *)a_737FD0, "098 sub_7260B0" },
		{ 0x726890, (void *)a_7387B0, "098 sub_726890" },
		{ 0x7276C0, (void *)a_7395E0, "098 sub_7276C0" },
		{ 0x7284B0, (void *)a_739890, "098 sub_7284B0" },
		{ 0x728580, (void *)a_739960, "098 sub_728580" },
		{ 0x7286D0, (void *)a_739AC0, "098 sub_7286D0" },
		{ 0x728710, (void *)a_739B40, "098 sub_728710" },
		{ 0x721B50, (void *)a_73A0D0, "098 MAG_098_sub_721B50" },
		{ 0x721B60, (void *)a_73A0D0, "098 MAG_098_sub_721B60" },
		{ 0x7299B0, (void *)a_73A0D0, "098 MAG_098_sub_7299B0" },
		{ 0x721BF0, (void *)a_73A170, "098 MAG_098_sub_721BF0" },
		{ 0x721C20, (void *)a_73A1A0, "098 MAG_098_sub_721C20" },
		{ 0x721DF0, (void *)a_73A380, "098 MAG_098_sub_721DF0" },
		{ 0x722460, (void *)a_73A380, "098 sub_722460" },
		{ 0x722920, (void *)a_73A380, "098 sub_722920" },
		{ 0x728C60, (void *)a_73A380, "098 sub_728C60" },
		{ 0x7292D0, (void *)a_73A380, "098 sub_7292D0" },
		{ 0x7296D0, (void *)a_73A380, "098 sub_7296D0" },
		{ 0x7297C0, (void *)a_73A380, "098 sub_7297C0" },
		{ 0x721E50, (void *)a_73A3E0, "098 sub_721E50" },
		{ 0x721FF0, (void *)a_73A580, "098 sub_721FF0" },
		{ 0x722020, (void *)a_73A5B0, "098 sub_722020" },
		{ 0x722050, (void *)a_73A5E0, "098 sub_722050" },
		{ 0x722060, (void *)a_73A5E0, "098 sub_722060" },
		{ 0x722070, (void *)a_73A5E0, "098 sub_722070" },
		{ 0x722080, (void *)a_73A5E0, "098 sub_722080" },
		{ 0x722090, (void *)a_73A620, "098 sub_722090" },
		{ 0x7220D0, (void *)a_73A660, "098 sub_7220D0" },
		{ 0x722140, (void *)a_73A6D0, "098 nullsub_1121" },
		{ 0x722910, (void *)a_73A6D0, "098 nullsub_1122" },
		{ 0x7276E0, (void *)a_73A6D0, "098 nullsub_1123" },
		{ 0x7280C0, (void *)a_73A6D0, "098 nullsub_1124" },
		{ 0x7280D0, (void *)a_73A6D0, "098 nullsub_1125" },
		{ 0x728990, (void *)a_73A6D0, "098 nullsub_1126" },
		{ 0x728B10, (void *)a_73A6D0, "098 nullsub_1127" },
		{ 0x728D50, (void *)a_73A6D0, "098 nullsub_1128" },
		{ 0x729230, (void *)a_73A6D0, "098 nullsub_1129" },
		{ 0x729390, (void *)a_73A6D0, "098 nullsub_1130" },
		{ 0x729400, (void *)a_73A6D0, "098 nullsub_1131" },
		{ 0x729410, (void *)a_73A6D0, "098 nullsub_1132" },
		{ 0x7295C0, (void *)a_73A6D0, "098 nullsub_1133" },
		{ 0x7296C0, (void *)a_73A6D0, "098 nullsub_1134" },
		{ 0x7297B0, (void *)a_73A6D0, "098 nullsub_1135" },
		{ 0x7298A0, (void *)a_73A6D0, "098 nullsub_1136" },
		{ 0x729960, (void *)a_73A6D0, "098 nullsub_1137" },
		{ 0x729A30, (void *)a_73A6D0, "098 nullsub_1120" },
		{ 0x722180, (void *)a_73A720, "098 sub_722180" },
		{ 0x7280E0, (void *)a_73A930, "098 sub_7280E0" },
		{ 0x728B80, (void *)a_73A930, "098 sub_728B80" },
		{ 0x728100, (void *)a_73A950, "098 sub_728100" },
		{ 0x727FD0, (void *)a_73AAE0, "098 sub_727FD0" },
		{ 0x7277B0, (void *)a_73AB00, "098 sub_7277B0" },
		{ 0x7277D0, (void *)a_73AB20, "098 sub_7277D0" },
		{ 0x727AC0, (void *)a_73AE10, "098 sub_727AC0" },
		{ 0x727E10, (void *)a_73B160, "098 sub_727E10" },
		{ 0x727F80, (void *)a_73B2D0, "098 sub_727F80" },
		{ 0x727FF0, (void *)a_73B350, "098 sub_727FF0" },
		{ 0x728A30, (void *)a_73B640, "098 sub_728A30" },
		{ 0x7280A0, (void *)a_73B750, "098 sub_7280A0" },
		{ 0x7289F0, (void *)a_73B7E0, "098 sub_7289F0" },
		{ 0x728B60, (void *)a_73B8C0, "098 sub_728B60" },
		{ 0x728180, (void *)a_73BB60, "098 sub_728180" },
		{ 0x7293A0, (void *)a_73BC40, "098 sub_7293A0" },
		{ 0x7223C0, (void *)a_73C100, "098 sub_7223C0" },
		{ 0x7229B0, (void *)a_73F990, "098 sub_7229B0" },
		{ 0x722C40, (void *)a_73FC20, "098 sub_722C40" },
		{ 0x722EB0, (void *)a_73FE90, "098 sub_722EB0" },
		{ 0x722FD0, (void *)a_73FFB0, "098 sub_722FD0" },
		{ 0x723230, (void *)a_740210, "098 sub_723230" },
		{ 0x724BA0, (void *)a_740700, "098 sub_724BA0" },
		{ 0x724C60, (void *)a_7407C0, "098 sub_724C60" },
		{ 0x724D20, (void *)a_740880, "098 sub_724D20" },
		{ 0x724DB0, (void *)a_740910, "098 sub_724DB0" },
		{ 0x725410, (void *)a_740F70, "098 sub_725410" },
		{ 0x725460, (void *)a_740FC0, "098 sub_725460" },
		{ 0x7254F0, (void *)a_741050, "098 sub_7254F0" },
		{ 0x7255C0, (void *)a_741120, "098 sub_7255C0" },
		{ 0x725E60, (void *)a_7419C0, "098 sub_725E60" },
		{ 0x725EE0, (void *)a_741A40, "098 sub_725EE0" },
		{ 0x725F10, (void *)a_741A70, "098 sub_725F10" },
		{ 0x726000, (void *)a_741B60, "098 sub_726000" },
		{ 0x726730, (void *)a_742290, "098 au_re__rand_5" },
		{ 0x726760, (void *)a_7422C0, "098 sub_726760" },
		{ 0x7267A0, (void *)a_742300, "098 sub_7267A0" },
		{ 0x7267F0, (void *)a_742350, "098 sub_7267F0" },
		{ 0x726840, (void *)a_7423A0, "098 au_re__rand_5_0" },
		{ 0x726950, (void *)a_7424B0, "098 sub_726950" },
		{ 0x726960, (void *)a_7424C0, "098 sub_726960" },
		{ 0x727140, (void *)a_742CA0, "098 sub_727140" },
		{ 0x727170, (void *)a_742CD0, "098 sub_727170" },
		{ 0x7271A0, (void *)a_742D00, "098 sub_7271A0" },
		{ 0x727350, (void *)a_742EB0, "098 sub_727350" },
		{ 0x727490, (void *)a_742FF0, "098 sub_727490" },
		{ 0x7275A0, (void *)a_743100, "098 sub_7275A0" },
		{ 0x727630, (void *)a_743190, "098 sub_727630" },
		{ 0x728970, (void *)a_743C00, "098 sub_728970" },
		{ 0x728910, (void *)a_743C20, "098 sub_728910" },
		{ 0x729820, (void *)a_746980, "098 sub_729820" },
		{ 0x7282D0, (void *)a_746C10, "098 sub_7282D0" },
		{ 0x7298B0, (void *)a_747440, "098 MAG_098_sub_7298B0" },
		{ 0x729920, (void *)a_7474B0, "098 MAG_098_sub_729920" },
		{ 0x729940, (void *)a_7474D0, "098 MAG_098_sub_729940" },
		{ 0x729970, (void *)a_747500, "098 MAG_098_sub_729970" },
		{ 0x7299C0, (void *)a_747550, "098 MAG_098_sub_7299C0" },
		{ 0x7299D0, (void *)a_747560, "098 MAG_098_sub_7299D0" },
		{ 0x729A00, (void *)a_747590, "098 MAG_098_sub_729A00" },
		{ 0x729A10, (void *)a_7475A0, "098 MAG_098_sub_729A10" },
		{ 0, nullptr, nullptr }
	};
	static const AddrPort ENGINE_PORTS_099[] = {
		{ 0x718080, (void *)a_732120, "099 MAG_099_sub_718080" },
		{ 0x7180C0, (void *)a_732160, "099 MAG_099_sub_7180C0" },
		{ 0x721070, (void *)a_7335D0, "099 sub_721070" },
		{ 0x71F260, (void *)a_733AC0, "099 sub_71F260" },
		{ 0x71F4E0, (void *)a_733AC0, "099 sub_71F4E0" },
		{ 0x71F280, (void *)a_733B70, "099 sub_71F280" },
		{ 0x71F9E0, (void *)a_7340A0, "099 sub_71F9E0" },
		{ 0x718ED0, (void *)a_7348A0, "099 sub_718ED0" },
		{ 0x7199E0, (void *)a_7353B0, "099 sub_7199E0" },
		{ 0x719A70, (void *)a_735440, "099 sub_719A70" },
		{ 0x719AD0, (void *)a_7354A0, "099 sub_719AD0" },
		{ 0x719C10, (void *)a_7355E0, "099 sub_719C10" },
		{ 0x71A1E0, (void *)a_735BB0, "099 sub_71A1E0" },
		{ 0x71A420, (void *)a_735DF0, "099 sub_71A420" },
		{ 0x71B230, (void *)a_736C00, "099 sub_71B230" },
		{ 0x71C500, (void *)a_737ED0, "099 sub_71C500" },
		{ 0x71C580, (void *)a_737F50, "099 sub_71C580" },
		{ 0x71C600, (void *)a_737FD0, "099 sub_71C600" },
		{ 0x71CDE0, (void *)a_7387B0, "099 sub_71CDE0" },
		{ 0x71DC10, (void *)a_7395E0, "099 sub_71DC10" },
		{ 0x71EB60, (void *)a_739890, "099 sub_71EB60" },
		{ 0x71EC30, (void *)a_739960, "099 sub_71EC30" },
		{ 0x71ED80, (void *)a_739AC0, "099 sub_71ED80" },
		{ 0x71EDC0, (void *)a_739B40, "099 sub_71EDC0" },
		{ 0x717ED0, (void *)a_739F40, "099 MAG_099_sub_717ED0" },
		{ 0x718060, (void *)a_73A0D0, "099 MAG_099_sub_718060" },
		{ 0x718070, (void *)a_73A0D0, "099 MAG_099_sub_718070" },
		{ 0x7217B0, (void *)a_73A0D0, "099 MAG_099_sub_7217B0" },
		{ 0x718100, (void *)a_73A170, "099 MAG_099_sub_718100" },
		{ 0x718130, (void *)a_73A1A0, "099 MAG_099_sub_718130" },
		{ 0x718310, (void *)a_73A380, "099 MAG_099_sub_718310" },
		{ 0x7189B0, (void *)a_73A380, "099 sub_7189B0" },
		{ 0x718E70, (void *)a_73A380, "099 sub_718E70" },
		{ 0x71F880, (void *)a_73A380, "099 sub_71F880" },
		{ 0x71FEC0, (void *)a_73A380, "099 sub_71FEC0" },
		{ 0x7201F0, (void *)a_73A380, "099 sub_7201F0" },
		{ 0x720320, (void *)a_73A380, "099 sub_720320" },
		{ 0x720410, (void *)a_73A380, "099 sub_720410" },
		{ 0x720610, (void *)a_73A380, "099 sub_720610" },
		{ 0x721010, (void *)a_73A380, "099 sub_721010" },
		{ 0x721100, (void *)a_73A380, "099 sub_721100" },
		{ 0x721460, (void *)a_73A380, "099 sub_721460" },
		{ 0x718370, (void *)a_73A3E0, "099 sub_718370" },
		{ 0x718510, (void *)a_73A580, "099 sub_718510" },
		{ 0x718540, (void *)a_73A5B0, "099 sub_718540" },
		{ 0x718570, (void *)a_73A5E0, "099 sub_718570" },
		{ 0x718580, (void *)a_73A5E0, "099 sub_718580" },
		{ 0x718590, (void *)a_73A5E0, "099 sub_718590" },
		{ 0x7185A0, (void *)a_73A5E0, "099 sub_7185A0" },
		{ 0x7185B0, (void *)a_73A620, "099 sub_7185B0" },
		{ 0x7185F0, (void *)a_73A660, "099 sub_7185F0" },
		{ 0x718660, (void *)a_73A6D0, "099 nullsub_1088" },
		{ 0x718E60, (void *)a_73A6D0, "099 nullsub_1089" },
		{ 0x71DC30, (void *)a_73A6D0, "099 nullsub_1090" },
		{ 0x71E780, (void *)a_73A6D0, "099 nullsub_1119" },
		{ 0x71E790, (void *)a_73A6D0, "099 nullsub_1091" },
		{ 0x71F070, (void *)a_73A6D0, "099 nullsub_1092" },
		{ 0x71F1C0, (void *)a_73A6D0, "099 nullsub_1093" },
		{ 0x71F4B0, (void *)a_73A6D0, "099 nullsub_1094" },
		{ 0x71F600, (void *)a_73A6D0, "099 nullsub_1095" },
		{ 0x71F610, (void *)a_73A6D0, "099 nullsub_1096" },
		{ 0x71F940, (void *)a_73A6D0, "099 nullsub_1097" },
		{ 0x71FDB0, (void *)a_73A6D0, "099 nullsub_1098" },
		{ 0x71FEB0, (void *)a_73A6D0, "099 nullsub_1099" },
		{ 0x71FFA0, (void *)a_73A6D0, "099 nullsub_1100" },
		{ 0x7201E0, (void *)a_73A6D0, "099 nullsub_1101" },
		{ 0x720310, (void *)a_73A6D0, "099 nullsub_1102" },
		{ 0x720400, (void *)a_73A6D0, "099 nullsub_1103" },
		{ 0x7204F0, (void *)a_73A6D0, "099 nullsub_1104" },
		{ 0x720770, (void *)a_73A6D0, "099 nullsub_1105" },
		{ 0x720780, (void *)a_73A6D0, "099 nullsub_1106" },
		{ 0x720990, (void *)a_73A6D0, "099 nullsub_1107" },
		{ 0x7209A0, (void *)a_73A6D0, "099 nullsub_1108" },
		{ 0x720D60, (void *)a_73A6D0, "099 nullsub_1109" },
		{ 0x721000, (void *)a_73A6D0, "099 nullsub_1110" },
		{ 0x7210F0, (void *)a_73A6D0, "099 nullsub_1111" },
		{ 0x7211C0, (void *)a_73A6D0, "099 nullsub_1112" },
		{ 0x721380, (void *)a_73A6D0, "099 nullsub_1113" },
		{ 0x721450, (void *)a_73A6D0, "099 nullsub_1114" },
		{ 0x721590, (void *)a_73A6D0, "099 nullsub_1115" },
		{ 0x7216A0, (void *)a_73A6D0, "099 nullsub_1116" },
		{ 0x721760, (void *)a_73A6D0, "099 nullsub_1117" },
		{ 0x721830, (void *)a_73A6D0, "099 nullsub_1087" },
		{ 0x7186C0, (void *)a_73A720, "099 sub_7186C0" },
		{ 0x71E7A0, (void *)a_73A930, "099 sub_71E7A0" },
		{ 0x71F1D0, (void *)a_73A930, "099 sub_71F1D0" },
		{ 0x71F240, (void *)a_73A930, "099 sub_71F240" },
		{ 0x71E7C0, (void *)a_73A950, "099 sub_71E7C0" },
		{ 0x71E520, (void *)a_73AAE0, "099 sub_71E520" },
		{ 0x71DD00, (void *)a_73AB00, "099 sub_71DD00" },
		{ 0x71DD20, (void *)a_73AB20, "099 sub_71DD20" },
		{ 0x71E010, (void *)a_73AE10, "099 sub_71E010" },
		{ 0x71E360, (void *)a_73B160, "099 sub_71E360" },
		{ 0x71E4D0, (void *)a_73B2D0, "099 sub_71E4D0" },
		{ 0x71E540, (void *)a_73B350, "099 sub_71E540" },
		{ 0x71F380, (void *)a_73B620, "099 sub_71F380" },
		{ 0x71F110, (void *)a_73B640, "099 sub_71F110" },
		{ 0x71F0D0, (void *)a_73B7E0, "099 sub_71F0D0" },
		{ 0x71F220, (void *)a_73B8C0, "099 sub_71F220" },
		{ 0x71F410, (void *)a_73B9F0, "099 sub_71F410" },
		{ 0x721550, (void *)a_73BA90, "099 sub_721550" },
		{ 0x71E840, (void *)a_73BB60, "099 sub_71E840" },
		{ 0x71F580, (void *)a_73BC40, "099 sub_71F580" },
		{ 0x721680, (void *)a_73BCB0, "099 sub_721680" },
		{ 0x718910, (void *)a_73C100, "099 sub_718910" },
		{ 0x71F310, (void *)a_73F110, "099 sub_71F310" },
		{ 0x71FFB0, (void *)a_73F110, "099 sub_71FFB0" },
		{ 0x7215A0, (void *)a_73F110, "099 sub_7215A0" },
		{ 0x718F00, (void *)a_73F990, "099 sub_718F00" },
		{ 0x719190, (void *)a_73FC20, "099 sub_719190" },
		{ 0x719400, (void *)a_73FE90, "099 sub_719400" },
		{ 0x719520, (void *)a_73FFB0, "099 sub_719520" },
		{ 0x719780, (void *)a_740210, "099 sub_719780" },
		{ 0x71B0F0, (void *)a_740700, "099 sub_71B0F0" },
		{ 0x71B1B0, (void *)a_7407C0, "099 sub_71B1B0" },
		{ 0x71B270, (void *)a_740880, "099 sub_71B270" },
		{ 0x71B300, (void *)a_740910, "099 sub_71B300" },
		{ 0x71B960, (void *)a_740F70, "099 sub_71B960" },
		{ 0x71B9B0, (void *)a_740FC0, "099 sub_71B9B0" },
		{ 0x71BA40, (void *)a_741050, "099 sub_71BA40" },
		{ 0x71BB10, (void *)a_741120, "099 sub_71BB10" },
		{ 0x71C3B0, (void *)a_7419C0, "099 sub_71C3B0" },
		{ 0x71C430, (void *)a_741A40, "099 sub_71C430" },
		{ 0x71C460, (void *)a_741A70, "099 sub_71C460" },
		{ 0x71C550, (void *)a_741B60, "099 sub_71C550" },
		{ 0x71CC80, (void *)a_742290, "099 au_re__rand_4" },
		{ 0x71CCB0, (void *)a_7422C0, "099 sub_71CCB0" },
		{ 0x71CCF0, (void *)a_742300, "099 sub_71CCF0" },
		{ 0x71CD40, (void *)a_742350, "099 sub_71CD40" },
		{ 0x71CD90, (void *)a_7423A0, "099 au_re__rand_4_0" },
		{ 0x71CEA0, (void *)a_7424B0, "099 sub_71CEA0" },
		{ 0x71CEB0, (void *)a_7424C0, "099 sub_71CEB0" },
		{ 0x71D690, (void *)a_742CA0, "099 sub_71D690" },
		{ 0x71D6C0, (void *)a_742CD0, "099 sub_71D6C0" },
		{ 0x71D6F0, (void *)a_742D00, "099 sub_71D6F0" },
		{ 0x71D8A0, (void *)a_742EB0, "099 sub_71D8A0" },
		{ 0x71D9E0, (void *)a_742FF0, "099 sub_71D9E0" },
		{ 0x71DAF0, (void *)a_743100, "099 sub_71DAF0" },
		{ 0x71DB80, (void *)a_743190, "099 sub_71DB80" },
		{ 0x71F050, (void *)a_743C00, "099 sub_71F050" },
		{ 0x71EFE0, (void *)a_743C20, "099 sub_71EFE0" },
		{ 0x721230, (void *)a_7458E0, "099 sub_721230" },
		{ 0x71F3F0, (void *)a_746A10, "099 sub_71F3F0" },
		{ 0x721520, (void *)a_746AE0, "099 sub_721520" },
		{ 0x71E980, (void *)a_746C10, "099 sub_71E980" },
		{ 0x7216B0, (void *)a_747440, "099 MAG_099_sub_7216B0" },
		{ 0x721720, (void *)a_7474B0, "099 MAG_099_sub_721720" },
		{ 0x721740, (void *)a_7474D0, "099 MAG_099_sub_721740" },
		{ 0x721770, (void *)a_747500, "099 MAG_099_sub_721770" },
		{ 0x7217C0, (void *)a_747550, "099 MAG_099_sub_7217C0" },
		{ 0x7217D0, (void *)a_747560, "099 MAG_099_sub_7217D0" },
		{ 0x721800, (void *)a_747590, "099 MAG_099_sub_721800" },
		{ 0x721810, (void *)a_7475A0, "099 MAG_099_sub_721810" },
		{ 0, nullptr, nullptr }
	};
	static const AddrPort ENGINE_PORTS_100[] = {
		{ 0x70ECB0, (void *)a_732120, "100 MAG_100_sub_70ECB0" },
		{ 0x70ECF0, (void *)a_732160, "100 MAG_100_sub_70ECF0" },
		{ 0x715380, (void *)a_7334C0, "100 sub_715380" },
		{ 0x716AC0, (void *)a_7335D0, "100 sub_716AC0" },
		{ 0x715F00, (void *)a_733760, "100 sub_715F00" },
		{ 0x717A70, (void *)a_733950, "100 sub_717A70" },
		{ 0x715EC0, (void *)a_733AC0, "100 sub_715EC0" },
		{ 0x715F40, (void *)a_733AC0, "100 sub_715F40" },
		{ 0x715FE0, (void *)a_733AC0, "100 sub_715FE0" },
		{ 0x716020, (void *)a_733AE0, "100 sub_716020" },
		{ 0x715EE0, (void *)a_733B70, "100 sub_715EE0" },
		{ 0x716460, (void *)a_7340A0, "100 sub_716460" },
		{ 0x70FB50, (void *)a_7348A0, "100 sub_70FB50" },
		{ 0x710660, (void *)a_7353B0, "100 sub_710660" },
		{ 0x7106F0, (void *)a_735440, "100 sub_7106F0" },
		{ 0x710750, (void *)a_7354A0, "100 sub_710750" },
		{ 0x710890, (void *)a_7355E0, "100 sub_710890" },
		{ 0x710E60, (void *)a_735BB0, "100 sub_710E60" },
		{ 0x7110A0, (void *)a_735DF0, "100 sub_7110A0" },
		{ 0x711EB0, (void *)a_736C00, "100 sub_711EB0" },
		{ 0x713180, (void *)a_737ED0, "100 sub_713180" },
		{ 0x713200, (void *)a_737F50, "100 sub_713200" },
		{ 0x713280, (void *)a_737FD0, "100 sub_713280" },
		{ 0x713A60, (void *)a_7387B0, "100 sub_713A60" },
		{ 0x714890, (void *)a_7395E0, "100 sub_714890" },
		{ 0x7157E0, (void *)a_739890, "100 sub_7157E0" },
		{ 0x7158B0, (void *)a_739960, "100 sub_7158B0" },
		{ 0x715A00, (void *)a_739AC0, "100 sub_715A00" },
		{ 0x715A40, (void *)a_739B40, "100 sub_715A40" },
		{ 0x70EB00, (void *)a_739F40, "100 MAG_100_sub_70EB00" },
		{ 0x70EC90, (void *)a_73A0D0, "100 MAG_100_sub_70EC90" },
		{ 0x70ECA0, (void *)a_73A0D0, "100 MAG_100_sub_70ECA0" },
		{ 0x717C80, (void *)a_73A0D0, "100 MAG_100_sub_717C80" },
		{ 0x70ED30, (void *)a_73A170, "100 MAG_100_sub_70ED30" },
		{ 0x70ED60, (void *)a_73A1A0, "100 MAG_100_sub_70ED60" },
		{ 0x70EF30, (void *)a_73A380, "100 MAG_100_sub_70EF30" },
		{ 0x70F5F0, (void *)a_73A380, "100 sub_70F5F0" },
		{ 0x70FAF0, (void *)a_73A380, "100 sub_70FAF0" },
		{ 0x716300, (void *)a_73A380, "100 sub_716300" },
		{ 0x716950, (void *)a_73A380, "100 sub_716950" },
		{ 0x716A60, (void *)a_73A380, "100 sub_716A60" },
		{ 0x716B50, (void *)a_73A380, "100 sub_716B50" },
		{ 0x70EF90, (void *)a_73A3E0, "100 sub_70EF90" },
		{ 0x70F130, (void *)a_73A580, "100 sub_70F130" },
		{ 0x70F160, (void *)a_73A5B0, "100 sub_70F160" },
		{ 0x70F190, (void *)a_73A5E0, "100 sub_70F190" },
		{ 0x70F1A0, (void *)a_73A5E0, "100 sub_70F1A0" },
		{ 0x70F1B0, (void *)a_73A5E0, "100 sub_70F1B0" },
		{ 0x70F1C0, (void *)a_73A5E0, "100 sub_70F1C0" },
		{ 0x70F1D0, (void *)a_73A620, "100 sub_70F1D0" },
		{ 0x70F210, (void *)a_73A660, "100 sub_70F210" },
		{ 0x70F280, (void *)a_73A6D0, "100 nullsub_1069" },
		{ 0x70FAE0, (void *)a_73A6D0, "100 nullsub_1070" },
		{ 0x7148B0, (void *)a_73A6D0, "100 nullsub_1071" },
		{ 0x7153F0, (void *)a_73A6D0, "100 nullsub_1072" },
		{ 0x715400, (void *)a_73A6D0, "100 nullsub_1073" },
		{ 0x715CC0, (void *)a_73A6D0, "100 nullsub_1074" },
		{ 0x715E20, (void *)a_73A6D0, "100 nullsub_1075" },
		{ 0x716100, (void *)a_73A6D0, "100 nullsub_1076" },
		{ 0x716110, (void *)a_73A6D0, "100 nullsub_1077" },
		{ 0x7163C0, (void *)a_73A6D0, "100 nullsub_1078" },
		{ 0x716830, (void *)a_73A6D0, "100 nullsub_1079" },
		{ 0x716940, (void *)a_73A6D0, "100 nullsub_1080" },
		{ 0x716A50, (void *)a_73A6D0, "100 nullsub_1081" },
		{ 0x716B40, (void *)a_73A6D0, "100 nullsub_1082" },
		{ 0x716C10, (void *)a_73A6D0, "100 nullsub_1083" },
		{ 0x7175E0, (void *)a_73A6D0, "100 nullsub_1084" },
		{ 0x717B70, (void *)a_73A6D0, "100 nullsub_1085" },
		{ 0x717C30, (void *)a_73A6D0, "100 nullsub_1086" },
		{ 0x717D00, (void *)a_73A6D0, "100 nullsub_1068" },
		{ 0x70F2E0, (void *)a_73A720, "100 sub_70F2E0" },
		{ 0x715410, (void *)a_73A930, "100 sub_715410" },
		{ 0x715E30, (void *)a_73A930, "100 sub_715E30" },
		{ 0x715EA0, (void *)a_73A930, "100 sub_715EA0" },
		{ 0x715F20, (void *)a_73A930, "100 sub_715F20" },
		{ 0x715FC0, (void *)a_73A930, "100 sub_715FC0" },
		{ 0x715430, (void *)a_73A950, "100 sub_715430" },
		{ 0x7151C0, (void *)a_73AAE0, "100 sub_7151C0" },
		{ 0x7149A0, (void *)a_73AB00, "100 sub_7149A0" },
		{ 0x7149C0, (void *)a_73AB20, "100 sub_7149C0" },
		{ 0x714CB0, (void *)a_73AE10, "100 sub_714CB0" },
		{ 0x715000, (void *)a_73B160, "100 sub_715000" },
		{ 0x715170, (void *)a_73B2D0, "100 sub_715170" },
		{ 0x7151E0, (void *)a_73B350, "100 sub_7151E0" },
		{ 0x715300, (void *)a_73B640, "100 sub_715300" },
		{ 0x7153D0, (void *)a_73B750, "100 sub_7153D0" },
		{ 0x715D20, (void *)a_73B7E0, "100 sub_715D20" },
		{ 0x715E80, (void *)a_73B8C0, "100 sub_715E80" },
		{ 0x7154B0, (void *)a_73BB60, "100 sub_7154B0" },
		{ 0x716000, (void *)a_73BBD0, "100 sub_716000" },
		{ 0x7160A0, (void *)a_73BC40, "100 sub_7160A0" },
		{ 0x70F550, (void *)a_73C100, "100 sub_70F550" },
		{ 0x70FB80, (void *)a_73F990, "100 sub_70FB80" },
		{ 0x70FE10, (void *)a_73FC20, "100 sub_70FE10" },
		{ 0x710080, (void *)a_73FE90, "100 sub_710080" },
		{ 0x7101A0, (void *)a_73FFB0, "100 sub_7101A0" },
		{ 0x710400, (void *)a_740210, "100 sub_710400" },
		{ 0x711D70, (void *)a_740700, "100 sub_711D70" },
		{ 0x711E30, (void *)a_7407C0, "100 sub_711E30" },
		{ 0x711EF0, (void *)a_740880, "100 sub_711EF0" },
		{ 0x711F80, (void *)a_740910, "100 sub_711F80" },
		{ 0x7125E0, (void *)a_740F70, "100 sub_7125E0" },
		{ 0x712630, (void *)a_740FC0, "100 sub_712630" },
		{ 0x7126C0, (void *)a_741050, "100 sub_7126C0" },
		{ 0x712790, (void *)a_741120, "100 sub_712790" },
		{ 0x713030, (void *)a_7419C0, "100 sub_713030" },
		{ 0x7130B0, (void *)a_741A40, "100 sub_7130B0" },
		{ 0x7130E0, (void *)a_741A70, "100 sub_7130E0" },
		{ 0x7131D0, (void *)a_741B60, "100 sub_7131D0" },
		{ 0x713900, (void *)a_742290, "100 au_re__rand_3" },
		{ 0x713930, (void *)a_7422C0, "100 sub_713930" },
		{ 0x713970, (void *)a_742300, "100 sub_713970" },
		{ 0x7139C0, (void *)a_742350, "100 sub_7139C0" },
		{ 0x713A10, (void *)a_7423A0, "100 au_re__rand_3_0" },
		{ 0x713B20, (void *)a_7424B0, "100 sub_713B20" },
		{ 0x713B30, (void *)a_7424C0, "100 sub_713B30" },
		{ 0x714310, (void *)a_742CA0, "100 sub_714310" },
		{ 0x714340, (void *)a_742CD0, "100 sub_714340" },
		{ 0x714370, (void *)a_742D00, "100 sub_714370" },
		{ 0x714520, (void *)a_742EB0, "100 sub_714520" },
		{ 0x714660, (void *)a_742FF0, "100 sub_714660" },
		{ 0x714770, (void *)a_743100, "100 sub_714770" },
		{ 0x714800, (void *)a_743190, "100 sub_714800" },
		{ 0x715CA0, (void *)a_743C00, "100 sub_715CA0" },
		{ 0x715C40, (void *)a_743C20, "100 sub_715C40" },
		{ 0x715600, (void *)a_746C10, "100 sub_715600" },
		{ 0x717B80, (void *)a_747440, "100 MAG_100_sub_717B80" },
		{ 0x717BF0, (void *)a_7474B0, "100 MAG_100_sub_717BF0" },
		{ 0x717C10, (void *)a_7474D0, "100 MAG_100_sub_717C10" },
		{ 0x717C40, (void *)a_747500, "100 MAG_100_sub_717C40" },
		{ 0x717C90, (void *)a_747550, "100 MAG_100_sub_717C90" },
		{ 0x717CA0, (void *)a_747560, "100 MAG_100_sub_717CA0" },
		{ 0x717CD0, (void *)a_747590, "100 MAG_100_sub_717CD0" },
		{ 0x717CE0, (void *)a_7475A0, "100 MAG_100_sub_717CE0" },
		{ 0, nullptr, nullptr }
	};

	// ------------------------------------------------------------------------------------
	// module descriptors, dispatch table, registration
	// ------------------------------------------------------------------------------------
	const Mod *g_mod = &MOD_095;
	uint32_t g_ported_tick = 0xFFFFFFFF;

	static const Mod *const MODS[] = { &MOD_090, &MOD_095, &MOD_096, &MOD_097, &MOD_098, &MOD_099, &MOD_100 };

	const Mod *find_mod(int effect_id)
	{
		for (const Mod *m : MODS)
			if (m->effect_id == effect_id) return m;
		return nullptr;
	}

	void set_mod_by_code(uint32_t code_addr)
	{
		for (const Mod *m : MODS)
			if (code_addr >= m->lo && code_addr < m->hi) { g_mod = m; return; }
	}

	// original address -> port (open addressing; engine ports of every registered module plus the
	// modules' own ports)
	static const int DISPATCH_SIZE = 4096; // power of two
	static uint32_t g_disp_key[DISPATCH_SIZE];
	static void *g_disp_val[DISPATCH_SIZE];
	static inline uint32_t disp_slot(uint32_t a) { return (a * 2654435761u) >> 20; }

	void *port_of(uint32_t orig)
	{
		for (uint32_t h = disp_slot(orig);; h++)
		{
			uint32_t k = g_disp_key[h & (DISPATCH_SIZE - 1)];
			if (k == orig) return g_disp_val[h & (DISPATCH_SIZE - 1)];
			if (k == 0) return nullptr;
		}
	}

	void add_port(uint32_t orig, void *port)
	{
		for (uint32_t h = disp_slot(orig);; h++)
		{
			uint32_t &k = g_disp_key[h & (DISPATCH_SIZE - 1)];
			if (k == 0 || k == orig)
			{
				k = orig;
				g_disp_val[h & (DISPATCH_SIZE - 1)] = port;
				return;
			}
		}
	}

	void remove_port(uint32_t orig)
	{
		for (uint32_t h = disp_slot(orig);; h++)
		{
			uint32_t k = g_disp_key[h & (DISPATCH_SIZE - 1)];
			if (k == orig) { g_disp_val[h & (DISPATCH_SIZE - 1)] = nullptr; return; }
			if (k == 0) return;
		}
	}

	static const AddrPort *engine_ports(int effect_id)
	{
		switch (effect_id)
		{
		case 90: return ENGINE_PORTS_090;
		case 95: return ENGINE_PORTS_095;
		case 96: return ENGINE_PORTS_096;
		case 97: return ENGINE_PORTS_097;
		case 98: return ENGINE_PORTS_098;
		case 99: return ENGINE_PORTS_099;
		case 100: return ENGINE_PORTS_100;
		}
		return nullptr;
	}

	void register_module_port(int effect_id, uint32_t orig, void *port, const char *name, bool held)
	{
		add_port(orig, port);
		ff8fx::register_port(orig, port, name, effect_id, held);
	}

	void register_module(int effect_id, const uint32_t *held_tasks)
	{
		// the shared-engine task 0x8DDC30 (and its state handlers) serve every module
		static bool global_done = false;
		if (!global_done)
		{
			global_done = true;
			add_port(0x8DDC30, (void *)a_8DDC30);
			add_port(0x8DDCA0, (void *)a_8DDCA0);
			add_port(0x8DDCD0, (void *)a_8DDCD0);
		}
		ff8fx::register_port(0x8DDC30, (void *)a_8DDC30, "act 8DDC30 shared engine task", effect_id, false);
		for (const AddrPort *p = engine_ports(effect_id); p && p->addr; p++)
		{
			bool held = false;
			for (const uint32_t *h = held_tasks; h && *h; h++) held |= *h == p->addr;
			register_module_port(effect_id, p->addr, p->port, p->name, held);
		}
	}

	// (harness / bring-up) original address, in the current module, of an engine port
	uint32_t orig_of_port(void *port)
	{
		for (const AddrPort *p = engine_ports(g_mod->effect_id); p && p->addr; p++)
			if (p->port == port) return p->addr;
		return 0;
	}

	// ------------------------------------------------------------------------------------
	// held frames (30 fps)
	// ------------------------------------------------------------------------------------
	// camera: the engine's two camera steppers (a_73AE10 interpolated move, a_73B4B0 keyframe
	// tracks) both end in a_73B160, which writes the battle camera words; they note here that
	// they ran on this real tick
	uint32_t g_cam_tick = 0xFFFFFFFF;
	int g_cam_kind = 0; // 1 = a_73AE10, 2 = a_73B4B0

	// saves / restores everything a camera step writes: the module's camera block (0xEC bytes
	// at *G_15339D0), the camera words, projection distance, 0x1D977A2 and the GTE registers
	struct CamSave
	{
		uint8_t block[0xEC];
		uint8_t words[0x10];
		uint16_t proj_h, w977a2;
		uint8_t gte[0x1CA9300 - 0x1CA8A10];
		uint32_t fa;
		uint8_t scratch[0x1000];
	};
	static void cam_save(CamSave &s, uint32_t blk)
	{
		memcpy(s.block, (const void *)blk, sizeof(s.block));
		memcpy(s.words, (const void *)0xB8B7F0, sizeof(s.words));
		s.proj_h = MEM<uint16_t>(0x1D8E038);
		s.w977a2 = MEM<uint16_t>(0x1D977A2);
		memcpy(s.gte, (const void *)0x1CA8A10, sizeof(s.gte));
		s.fa = MEM<uint32_t>(0x1D999C4); // the matrix helpers use the Field_Alloc scratch
		memcpy(s.scratch, (const void *)s.fa, sizeof(s.scratch));
	}
	static void cam_restore(const CamSave &s, uint32_t blk)
	{
		memcpy((void *)blk, s.block, sizeof(s.block));
		memcpy((void *)0xB8B7F0, s.words, sizeof(s.words));
		MEM<uint16_t>(0x1D8E038) = s.proj_h;
		MEM<uint16_t>(0x1D977A2) = s.w977a2;
		memcpy((void *)0x1CA8A10, s.gte, sizeof(s.gte));
		memcpy((void *)s.fa, s.scratch, sizeof(s.scratch));
		MEM<uint32_t>(0x1D999C4) = s.fa;
	}

	bool held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		if (g_cam_tick != g_real_tick || g_ported_tick != g_real_tick) return false;
		uint32_t blk = MEM<uint32_t>(G(G_15339D0));
		if (!blk) return false;
		const int16_t *eye = (const int16_t *)0xB8B7F0, *at = (const int16_t *)0xB8B7F8;
		int16_t e0[3] = { eye[0], eye[1], eye[2] }, a0[3] = { at[0], at[1], at[2] };
		// predict: the same stepper runs on the next tick (pure memory + GTE, restored below)
		static CamSave s;
		cam_save(s, blk);
		uint32_t tick = g_cam_tick;
		int kind = g_cam_kind;
		if (kind == 1) a_73AE10();
		else a_73B4B0();
		int16_t e1[3] = { eye[0], eye[1], eye[2] }, a1[3] = { at[0], at[1], at[2] };
		cam_restore(s, blk);
		g_cam_tick = tick;
		g_cam_kind = kind;
		// a jump larger than a normal step is a cut: hold
		for (int i = 0; i < 3; i++)
		{
			int32_t de = e1[i] - e0[i], da = a1[i] - a0[i];
			if (de > 4000 || de < -4000 || da > 4000 || da < -4000)
			{
				for (int k = 0; k < 3; k++) { world[k] = e0[k]; lookat[k] = a0[k]; }
				return true;
			}
		}
		for (int i = 0; i < 3; i++)
		{
			world[i] = (int16_t)lerp_i(e0[i], e1[i], num, den);
			lookat[i] = (int16_t)lerp_i(a0[i], a1[i], num, den);
		}
		return true;
	}

	// held frame of a module: in-between redraw of its prim-model plays (prim::play_held) and of
	// its creature actor(s) (pose_midpoint + DrawModel a_746C10), into a private packet buffer.
	// Everything these draws write besides packets is saved and put back: the module's globals
	// (the prim callbacks refresh module tables, e.g. Siren's orbit key tables), the module scratch
	// stack, the Field_Alloc scratch, the shadow temporaries of 0x5088A0, the packet cursor, the
	// creature's model matrix / bbox / bone matrices (rebuilt from the pose by DrawModel itself)
	static uint8_t g_held_packets[0x100000];
	static uint8_t g_held_bss[0x20000];
	void held_frame(const HeldDesc &d, int num, int den)
	{
		if (g_ported_tick != g_real_tick) return;
		const Mod *saved_mod = g_mod;
		g_mod = d.mod;
		uint32_t bss_n = d.bss_hi - d.bss_lo;
		if (bss_n > sizeof(g_held_bss)) bss_n = sizeof(g_held_bss);
		memcpy(g_held_bss, (const void *)d.bss_lo, bss_n);
		static uint8_t camcopy[0x20], scratch[0x8000], shadow[0x80], gte[0x1CA9300 - 0x1CA8A10];
		memcpy(gte, (const void *)0x1CA8A10, sizeof(gte)); // GTE register files (the gate saves them too)
		uint32_t parsepoly[2] = { MEM<uint32_t>(0x1D99C18), MEM<uint32_t>(0x1D99C1C) }; // RenderGeometry (ParsePolygons) temporaries
		memcpy(camcopy, (const void *)0x2793E58, sizeof(camcopy));
		uint32_t fa = MEM<uint32_t>(0x1D999C4);
		memcpy(scratch, (const void *)fa, sizeof(scratch));
		memcpy(shadow, (const void *)0x1D999C8, sizeof(shadow));
		uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		MEM<uint32_t>(0x1D8E054) = P(g_held_packets);

		held_draw_prim_plays(num, den);

		auto draw_creatures = [&](uint32_t queue, uint32_t task, uint32_t draw_arg)
		{
			for (uint32_t node = MEM<uint32_t>(queue); node; node = U32(node, 4))
			{
				if (U32(node, 8) != task) continue;
				if (U8(node, 0x26) & 4) continue; // hidden: the real tick did not draw it either
				static uint8_t mdl[0x40], place[0x30];
				memcpy(mdl, (const void *)(node + 0x60), sizeof(mdl)); // bbox +0x64..0x6E, matrix +0x70..0x8F
				memcpy(place, (const void *)(node + 0x30), sizeof(place)); // placement (position +0x4C..0x50)
				pose_midpoint((void *)(node + 0x90), (void *)(node + 0x9C), num, den);
				if (d.adjust) d.adjust(node, num, den);
				MEM<uint32_t>(0x1D8E054) = a_746C10(node, draw_arg, MEM<uint32_t>(0x1D8E054));
				memcpy((void *)(node + 0x30), place, sizeof(place));
				memcpy((void *)(node + 0x60), mdl, sizeof(mdl));
			}
		};
		draw_creatures(d.creature_queue, d.creature_task, d.draw_arg);
		for (const HeldCreature *c = d.more; c && c->queue; c++)
			draw_creatures(c->queue, c->task, c->draw_arg);

		MEM<uint32_t>(0x1D8E054) = cursor;
		memcpy((void *)0x1D999C8, shadow, sizeof(shadow));
		memcpy((void *)fa, scratch, sizeof(scratch));
		MEM<uint32_t>(0x1D999C4) = fa;
		memcpy((void *)0x2793E58, camcopy, sizeof(camcopy));
		memcpy((void *)d.bss_lo, g_held_bss, bss_n);
		memcpy((void *)0x1CA8A10, gte, sizeof(gte));
		MEM<uint32_t>(0x1D99C18) = parsepoly[0];
		MEM<uint32_t>(0x1D99C1C) = parsepoly[1];
		g_mod = saved_mod;
	}

	bool held_ready() { return g_ported_tick == g_real_tick; }

	// ------------------------------------------------------------------------------------
	// prim-model player (MAG_011_sub_701970): the native twin ff8fx::prim::play with the ported
	// draw callback; every play of the real tick is remembered (layout, callback, the 0x5C-byte
	// parameter block) so the held frame can redraw it in between (prim::play_held)
	// ------------------------------------------------------------------------------------
	struct PrimPlay { uint32_t layout, cb; const Mod *mod; uint8_t arg[0x5C]; };
	static PrimPlay g_prim_plays[256];
	static int g_nprim_plays = 0;
	static uint32_t g_prim_tick = 0xFFFFFFFF;

	uint32_t prim_play(uint32_t layout, uint32_t cb, uint32_t arg, uint32_t paused)
	{
		void *p = port_of(cb);
		uint32_t f = p ? P(p) : cb;
		int r = ff8fx::prim::play((ff8fx::prim::Layout *)layout, (ff8fx::prim::Callback)f, (int)arg, (int)paused);
		if (g_prim_tick != g_real_tick) { g_prim_tick = g_real_tick; g_nprim_plays = 0; }
		if (g_nprim_plays < 256)
		{
			PrimPlay &m = g_prim_plays[g_nprim_plays++];
			m.layout = layout;
			m.cb = f;
			m.mod = g_mod;
			memcpy(m.arg, (const void *)arg, sizeof(m.arg));
		}
		return (uint32_t)r;
	}

	void held_draw_prim_plays(int num, int den)
	{
		if (g_prim_tick != g_real_tick) return;
		for (int i = 0; i < g_nprim_plays; i++)
		{
			const PrimPlay &m = g_prim_plays[i];
			if (m.mod != g_mod) continue;
			alignas(4) uint8_t arg[0x5C];
			memcpy(arg, m.arg, sizeof(arg));
			ff8fx::prim::play_held((ff8fx::prim::Layout *)m.layout, (ff8fx::prim::Callback)m.cb, (int)P(arg), num, den);
		}
	}

	// ====================================================================================
	// part e1
	// ====================================================================================
	// Part e1: MiniMog-canonical engine functions (module 096 copies) + the shared 0x8DDxxx
	// screen-tint task (global engine code, raw addresses).
	namespace
	{
		// screen-space clip test of a projected coordinate (x: 0..0xA00, y: 0..0x6C0)
		inline bool e1_out(int16_t v, int16_t hi) { return v < 0 || v > hi; }

		// 0x7340A0 pattern for records whose texture words are +8 (low 24 bits kept, top byte =
		// V of one vertex), +0xC, +0x10 (V in bits 8..15): shifts the three V coordinates by
		// `sh`, all three -0x8000 when any of them passes 0xFF00 (lists 1 and 3 of 0x7340A0)
		void e1_shift_v_a(uint32_t it, uint32_t sh)
		{
			uint32_t w8 = U32(it, 8);
			uint32_t wc = U32(it, 0xC);
			uint32_t w10 = U32(it, 0x10);
			uint32_t keep8 = w8 & 0xFFFFFF;
			uint32_t d = ((w8 >> 24) & 0xFF) << 8;
			uint32_t s = wc & 0xFF00;
			uint32_t e = w10 & 0xFF00;
			uint32_t bp = wc;
			uint32_t bx = w10;
			s += sh;
			bp &= 0xFFFF00FF;
			bx &= 0xFFFF00FF;
			e += sh;
			d += sh;
			if (s > 0xFF00 || e > 0xFF00 || d > 0xFF00)
			{
				s -= 0x8000;
				e -= 0x8000;
				d -= 0x8000;
			}
			s &= 0xFF00;
			d &= 0xFFFFFF00;
			bp |= s;
			e &= 0xFF00;
			U32(it, 0xC) = bp;
			d <<= 16;
			bx |= e;
			keep8 |= d;
			U32(it, 0x10) = bx;
			U32(it, 8) = keep8;
		}

		// same for records whose texture words are +0xC, +0x10, +0x14 (quads: 4 V coordinates,
		// lists 2 and 4 of 0x7340A0). list4 = the write order of list 4 (+0x10, +0xC, +0x14)
		void e1_shift_v_b(uint32_t it, uint32_t sh, bool list4)
		{
			uint32_t wc = U32(it, 0xC);
			uint32_t w10 = U32(it, 0x10);
			uint32_t s = wc & 0xFF00;
			uint32_t keepc = wc & 0xFFFF00FF;
			s += sh;
			uint32_t keep10 = w10 & 0xFFFF00FF;
			uint32_t bp = w10 & 0xFF00;
			uint32_t w14 = U32(it, 0x14);
			uint32_t di = w14 & 0xFF00FF;
			uint32_t bx = w14 & 0xFF00;
			uint32_t d = ((w14 >> 24) & 0xFF) << 8;
			bp += sh;
			bx += sh;
			d += sh;
			if (s > 0xFF00 || bp > 0xFF00 || bx > 0xFF00 || d > 0xFF00)
			{
				s -= 0x8000;
				bp -= 0x8000;
				bx -= 0x8000;
				d -= 0x8000;
			}
			d &= 0xFFFFFF00;
			s &= 0xFF00;
			d <<= 16;
			bx &= 0xFF00;
			bp &= 0xFF00;
			uint32_t outc = keepc | s;
			uint32_t out10 = keep10 | bp;
			uint32_t out14 = di | bx | d;
			if (list4)
			{
				U32(it, 0x10) = out10;
				U32(it, 0xC) = outc;
				U32(it, 0x14) = out14;
			}
			else
			{
				U32(it, 0xC) = outc;
				U32(it, 0x10) = out10;
				U32(it, 0x14) = out14;
			}
		}
	}

	// 0x732120 (engine; MiniMog MAG_096_sub_732120): state: when no child is alive (+0x28 == 0),
	// carves two work buffers (0x10E0 and 0x10680 bytes) off the module arena cursor, clears
	// them (a_732160) and advances the state index
	uint32_t __cdecl a_732120(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
		{
			uint32_t cur = U32(G(G_257F728), 0);
			U32(G(G_258EC54), 0) = cur;              // buffer A (0x10E0 bytes)
			cur += 0x10E0;
			U32(G(G_257F720), 0) = cur;              // buffer B (0x10680 bytes = 99 x 0x2A0 particle slots)
			cur += 0x10680;
			U32(G(G_257F728), 0) = cur;              // arena cursor
			a_732160();
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x732160 (engine; MiniMog MAG_096_sub_732160): clears the two work buffers (0x10E0 /
	// 0x10680 bytes) and zeroes four module u16 counters/cursors
	uint32_t __cdecl a_732160(void)
	{
		x::MAG_007_sub_8DCC00(U32(G(G_258EC54), 0), 0x10E0);
		x::MAG_007_sub_8DCC00(U32(G(G_257F720), 0), 0x10680);
		U16(G(G_2585AE0), 0) = 0;
		U16(G(G_257F698), 0) = 0;  // particle slot search cursor (a_7387B0)
		U16(G(G_257F4D4), 0) = 0;
		U16(G(G_257B748), 0) = 0;
		return 0; // eax = 0 (xor eax, eax)
	}

	// 0x7322A0 (engine; MiniMog MAG_096_sub_7322A0): initialises the module's 0x16C-byte global
	// state block: pointers set, block cleared, a 0xA000-byte buffer carved off the arena
	// (+0x100), 24 pointers +0x104..+0x160 reset to the shared default 0x19DB018, +0x164/+0x166
	// zeroed
	uint32_t __cdecl a_7322A0(void)
	{
		U32(G(G_258BFA8), 0) = G(G_152BBA0);
		U32(G(G_258EAA0), 0) = G(G_257F730);
		x::MAG_007_sub_8DCC00(G(G_257F730), 0x16C);
		uint32_t st = U32(G(G_258EAA0), 0);
		uint32_t cur = U32(G(G_257F728), 0);
		U32(st, 0x100) = cur;
		cur += 0xA000;
		U32(G(G_257F728), 0) = cur;
		const uint32_t def = 0x19DB018; // shared global (raw)
		U32(st, 0x114) = def;
		U32(st, 0x118) = def;
		U32(st, 0x11C) = def;
		U32(st, 0x120) = def;
		U32(st, 0x104) = def;
		U32(st, 0x108) = def;
		U32(st, 0x10C) = def;
		U32(st, 0x110) = def;
		U32(st, 0x124) = def;
		U32(st, 0x128) = def;
		U32(st, 0x12C) = def;
		U32(st, 0x130) = def;
		U32(st, 0x134) = def;
		U32(st, 0x138) = def;
		U32(st, 0x13C) = def;
		U32(st, 0x140) = def;
		U32(st, 0x144) = def;
		U32(st, 0x148) = def;
		U32(st, 0x14C) = def;
		U32(st, 0x150) = def;
		U32(st, 0x154) = def;
		U32(st, 0x158) = def;
		U32(st, 0x15C) = def;
		U32(st, 0x160) = def;
		U16(st, 0x164) = 0;
		U16(st, 0x166) = 0;  // busy counter polled by a_733AC0
		return 0; // void (eax = state block, unused by the caller)
	}

	// 0x732A00 (engine; MiniMog sub_732A00) TASK: 16-state machine (state table on the stack,
	// index +0x29), frame counter +0x24; ends (releases the linked task) once +0x26 bit0 is
	// set and no child is alive
	uint32_t __cdecl a_732A00(uint32_t a1)
	{
		uint32_t tab[16];
		tab[0] = F(F_732AC0);
		tab[1] = F(F_7332C0);
		tab[2] = F(F_733310);
		tab[3] = F(F_733340);
		tab[4] = F(F_733370);
		tab[5] = F(F_7333A0);
		tab[6] = F(F_7333D0);
		tab[7] = F(F_733400);
		tab[8] = F(F_733430);
		tab[9] = F(F_733460);
		tab[10] = F(F_733490);
		tab[11] = F(F_7334C0);
		tab[12] = F(F_7334F0);
		tab[13] = F(F_733510);
		tab[14] = F(F_733530);
		tab[15] = F(F_733560);
		callp(tab[(int32_t)S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((status & 1) && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x7334C0 (engine; MiniMog sub_7334C0): state: when a_73AE10 reports 1, runs a_73AB20 on
	// the module script block (G_1532F80, script data +8) and advances the state index
	uint32_t __cdecl a_7334C0(uint32_t a1)
	{
		if (a_73AE10() == 1)
		{
			uint32_t data = U32(G(G_1533010), 0);
			a_73AB20(G(G_1532F80), data + 8, 0);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7335D0 (engine; MiniMog sub_7335D0): state: clears node +0x1C and, in 4 records of
	// 0x2C bytes of the shared table at 0x1D98992, the u16 +0 and the bytes +0x26..+0x28;
	// advances the state index
	uint32_t __cdecl a_7335D0(uint32_t a1)
	{
		U16(a1, 0x1C) = 0;
		uint32_t p = 0x1D989BA; // shared global (raw)
		for (int n = 4; n != 0; n--)
		{
			U16(p, -0x28) = 0;
			U8(p, 0) = 0;
			U8(p, -1) = 0;
			U8(p, -2) = 0;
			p += 0x2C;
		}
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x733760 (engine; MiniMog sub_733760): state: advances the state index when a_73B7E0(4)
	// returns non-zero
	uint32_t __cdecl a_733760(uint32_t a1)
	{
		if (a_73B7E0(4) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x733950 (engine; MiniMog sub_733950): sets a battle entity's colour tint: arg2 = amount
	// (4.12), arg3 = mode (1 = base colour halved, 1/2/3 set +0x2B = 2, 3 also sets
	// +7 = (255 - 255*amount/4096) >> 2); +0x28..+0x2A = c - c*amount/4096 with c = the colour
	// dword at 0xB8B7D8; amount 0 restores the base colour dword and clears +0x2B / +7
	uint32_t __cdecl a_733950(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		uint8_t c[4];
		uint32_t base = U32(0xB8B7D8, 0); // shared global (raw)
		memcpy(c, &base, 4);
		int16_t amount = (int16_t)a2;
		if (amount == 0)
		{
			U32(a1, 0x28) = base;
			U8(a1, 0x2B) = 0;
			U8(a1, 7) = 0;
			return 0; // void
		}
		int32_t mode = (int16_t)a3;
		if (mode == 1)
		{
			c[0] = (uint8_t)(c[0] >> 1);
			c[1] = (uint8_t)(c[1] >> 1);
			c[2] = (uint8_t)(c[2] >> 1);
			U8(a1, 0x2B) = 2;
		}
		else if (mode == 2)
		{
			U8(a1, 0x2B) = 2;
		}
		else if (mode == 3)
		{
			int32_t v = (int32_t)amount;
			U8(a1, 0x2B) = 2;
			int32_t q = sub32(shl32(v, 8), v) / 4096;
			// `or dl, 0xFF` (dl = 0 or 0xFF from the cdq mask) -> 0xFF
			U8(a1, 7) = (uint8_t)((uint8_t)(0xFF - (uint8_t)q) >> 2);
		}
		int32_t k = (int32_t)amount;
		for (int i = 0; i < 3; i++)
		{
			int32_t d = mul32((int32_t)c[i], k) / 4096;
			U8(a1, 0x28 + i) = (uint8_t)(c[i] - (uint8_t)d);
		}
		return 0; // void
	}

	// 0x733AC0 (engine; MiniMog sub_733AC0): state: advances the state index once the module
	// state block's busy counter (+0x166) is 0
	uint32_t __cdecl a_733AC0(uint32_t a1)
	{
		uint32_t st = U32(G(G_258EAA0), 0);
		if (U16(st, 0x166) == 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x733AE0 (engine; MiniMog sub_733AE0): state: advances the state index 20 frames after
	// the frame stamp +0x44 (frame counter +0x24)
	uint32_t __cdecl a_733AE0(uint32_t a1)
	{
		int32_t start = S16(a1, 0x44);
		int32_t now = S16(a1, 0x24);
		if (now - start >= 0x14)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x733B70 (engine; MiniMog sub_733B70): state: advances the state index when a_73B640(3)
	// returns non-zero
	uint32_t __cdecl a_733B70(uint32_t a1)
	{
		if (a_73B640(3) != 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7340A0 (engine; MiniMog sub_7340A0): scrolls the texture V coordinates of every textured
	// primitive (4 lists: FT3 0x14 B, FT4 0x18 B, GT3 0x1C B, GT4 0x24 B) of a model primitive
	// block by arg2 rows (<<8), all coordinates of a primitive -0x8000 when one passes 0xFF00
	uint32_t __cdecl a_7340A0(uint32_t a1, uint32_t a2)
	{
		int32_t idx = S32(a1, 0) / 4;
		uint32_t p = a1 + (uint32_t)idx * 4;
		uint32_t n = U32(p, 0);
		uint32_t sh = a2 << 8;
		p = p + n * 12 + 4;          // skip the vertex block (12-byte entries)
		n = U32(p, 0);
		p = p + n * 12 + 4;          // skip the next 12-byte list
		int32_t cnt = S32(p, 0);
		p += 4;
		if (cnt > 0)
		{
			do
			{
				e1_shift_v_a(p, sh);
				p += 0x14;
			} while (--cnt != 0);
		}
		cnt = S32(p, 0);
		p += 4;
		if (cnt > 0)
		{
			do
			{
				e1_shift_v_b(p, sh, false);
				p += 0x18;
			} while (--cnt != 0);
		}
		n = U32(p, 0);
		p = p + n * 20 + 4;          // skip the untextured lists (20 / 24-byte entries)
		n = U32(p, 0);
		p = p + n * 24 + 4;
		cnt = S32(p, 0);
		p += 4;
		if (cnt > 0)
		{
			do
			{
				e1_shift_v_a(p, sh);
				p += 0x1C;
			} while (--cnt != 0);
		}
		cnt = S32(p, 0);
		p += 4;
		if (cnt > 0)
		{
			do
			{
				e1_shift_v_b(p, sh, true);
				p += 0x24;
			} while (--cnt != 0);
		}
		return 0; // void
	}

	// 0x7348A0 (engine; MiniMog sub_7348A0): state: once the frame counter +0x24 reaches
	// +0x298, runs a_73F990 and one step of the actor system (a_7353B0), advances the state
	uint32_t __cdecl a_7348A0(uint32_t a1)
	{
		if (S16(a1, 0x24) >= S16(a1, 0x298))
		{
			a_73F990(a1);
			a_7353B0(a1);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x7353B0 (engine; MiniMog sub_7353B0): one step of the actor system at node+0x34 (made
	// current in G_258FB78): phase +0x20: 0 -> 1; 1 -> updates the actors (a_743190, a_736C00,
	// a_740700) and draws them (a_735440), counts +0x18 up to +0x1A and moves to phase 2 when
	// nothing is pending (+0x14/+0x16 == 0); 2 -> returns 1 (done). Always adds +0x14/+0x16 to
	// the module counters G_258FB40 / G_258EC50
	uint32_t __cdecl a_7353B0(uint32_t a1)
	{
		uint32_t done = 0;
		uint32_t sys = a1 + 0x34;
		U32(G(G_258FB78), 0) = sys;
		uint16_t phase = U16(sys, 0x20);
		switch ((int16_t)phase)
		{
		case 0:
			U16(sys, 0x20) = (uint16_t)(phase + 1);
			break;
		case 1:
			a_743190();
			a_736C00();
			a_740700();
			a_735440();
			sys = U32(G(G_258FB78), 0);
			U16(sys, 0x18) = (uint16_t)(U16(sys, 0x18) + 1);
			if (S16(sys, 0x18) >= S16(sys, 0x1A) && U16(sys, 0x14) == 0 && U16(sys, 0x16) == 0)
				U16(sys, 0x20) = (uint16_t)(U16(sys, 0x20) + 1);
			break;
		case 2:
			done = 1;
			break;
		default:
			break;
		}
		uint16_t c14 = U16(sys, 0x14);
		uint16_t c16 = U16(sys, 0x16);
		U16(G(G_258FB40), 0) = (uint16_t)(U16(G(G_258FB40), 0) + c14);
		U16(G(G_258EC50), 0) = (uint16_t)(U16(G(G_258EC50), 0) + c16);
		return done;
	}

	// 0x735440 (engine; MiniMog sub_735440): draw pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) of type 1 whose definition (sys+0x224 table, index +0x1D6)
	// has kind 4 or 5 and which has a model (+0x170), draws it (a_7354A0)
	uint32_t __cdecl a_735440(void)
	{
		uint32_t act = U32(U32(G(G_258FB78), 0), 0x2C);
		while (act != 0)
		{
			if (U16(act, 8) == 1)
			{
				uint32_t sys = U32(G(G_258FB78), 0);
				int32_t k = S8(act, 0x1D6);
				uint32_t def = U32(U32(sys, 0x224), k * 4);
				uint8_t kind = U8(def, 0x16);
				if ((kind == 4 || kind == 5) && U32(act, 0x170) != 0)
					a_7354A0(act, def);
			}
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x7354A0 (engine; MiniMog sub_7354A0): draws one actor's primitive model: pushes a 0x58-
	// byte draw context on the module stack G_258FB68 (model +0x170; flags 0x30 when def+0x3A
	// == 0; colour +0x16C / depth-cue +0x1CE -> ctx +8 / +0xC, flags |0xC0), renders the model
	// (a_7355E0 into the effect OT, mode 2) once with the actor matrix (+0xAC), or +0x1D8 times
	// with the translation replaced by the positions at +0x19C (8-byte stride); pops the context
	uint32_t __cdecl a_7354A0(uint32_t a1, uint32_t a2)
	{
		uint32_t ctx = U32(G(G_258FB68), 0) - 0x58;
		bool def3a = U16(a2, 0x3A) != 0;
		uint32_t model = U32(a1, 0x170);
		U32(G(G_258FB68), 0) = ctx;
		uint32_t saved_ctx = ctx; // [esp+0xC]
		U32(ctx, 0) = model;
		U32(ctx, 0x1C) = 0;
		if (!def3a)
			U32(ctx, 0x1C) = 0x30;
		uint16_t col = U16(a1, 0x1CE);
		if (col != 0)
		{
			U32(ctx, 8) = U32(a1, 0x16C);
			uint32_t fl = U32(ctx, 0x1C);
			U32(ctx, 0xC) = (uint32_t)(int32_t)(int16_t)col;
			U32(ctx, 0x1C) = fl | 0xC0;
		}
		int8_t copies = S8(a1, 0x1D8);
		if (copies == 1)
		{
			uint32_t m = a1 + 0xAC;
			x::GTE_SetRotMatrix(m);
			x::GTE_SetTransVector(m);
			uint32_t cursor = U32(0x1D8E054, 0);
			uint32_t ot = U32(0x1D8E04C, 0) + 0x44;
			uint32_t r = a_7355E0(ctx, ot, 2, cursor);
			U32(0x1D8E054, 0) = r;
		}
		else if (copies > 0)
		{
			uint32_t m = a1 + 0xAC;
			uint32_t pos = a1 + 0x19E;
			int32_t i = 0;
			int32_t n;
			do
			{
				S32(a1, 0xC0) = S16(pos, -2);
				S32(a1, 0xC4) = S16(pos, 0);
				S32(a1, 0xC8) = S16(pos, 2);
				x::GTE_SetRotMatrix(m);
				x::GTE_SetTransVector(m);
				uint32_t cursor = U32(0x1D8E054, 0);
				uint32_t ot = U32(0x1D8E04C, 0) + 0x44;
				uint32_t r = a_7355E0(saved_ctx, ot, 2, cursor);
				n = S8(a1, 0x1D8);
				i++;
				pos += 8;
				U32(0x1D8E054, 0) = r;
			} while (i < n);
		}
		uint32_t top = U32(G(G_258FB68), 0) + 0x58;
		U32(G(G_258FB68), 0) = top;
		return top;
	}

	// 0x7355E0 (engine; MiniMog sub_7355E0): renders a primitive model through its draw context
	// (ctx+0 model, +4 vertices, +8..+0xA colour, +0x18 texture offset, +0x1C flags, +0x20 list
	// cursor): sets the GTE colour and runs the 8 primitive-list renderers in order (an empty
	// list is skipped); returns the packet cursor
	uint32_t __cdecl a_7355E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
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
			cur = callp(F(F_735720), a1, a2, a3, cur);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = callp(F(F_735930), a1, a2, a3, cur);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = a_735BB0(a1, a2, a3, cur);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = a_735DF0(a1, a2, a3, cur);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = callp(F(F_736090), a1, a2, a3, cur);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = callp(F(F_7362C0), a1, a2, a3, cur);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			cur = callp(F(F_736580), a1, a2, a3, cur);
		else
			U32(a1, 0x20) = U32(a1, 0x20) + 4;
		if (U32(U32(a1, 0x20), 0) != 0)
			return callp(F(F_7367E0), a1, a2, a3, cur);
		U32(a1, 0x20) = U32(a1, 0x20) + 4;
		return cur;
	}

	// 0x735BB0 (engine; MiniMog sub_735BB0): renders the textured-triangle list (POLY_FT3,
	// 0x14-byte records -> 0x20-byte packets) of a primitive model: RTPT, semi-trans bit from
	// the context flags, texture offset, GTE-flag / backface (MAC0) / screen-clip rejection,
	// optional depth-cue colour, InsertPrim at OT arg2 [otz >> arg3]; returns the packet cursor
	uint32_t __cdecl a_735BB0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t list = U32(ctx, 0x20);
		int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		uint32_t vtx = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if (count > 0)
		{
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
					uint32_t zero = 0; // [esp+0x10]
					x::GTE_ReadMAC0(ctx + 0x24);
					if (S32(ctx, 0x24) >= 0 || (U8(ctx, 0x1C) & 0x10))
					{
						x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
						x::GTE_AVSZ3();
						uint32_t clip = e1_out(S16(pkt, 8), 0xA00) ? 1 : zero;
						if (e1_out(S16(pkt, 0x10), 0xA00)) clip |= 2;
						if (e1_out(S16(pkt, 0x18), 0xA00)) clip |= 4;
						if (e1_out(S16(pkt, 0xA), 0x6C0)) clip |= 0x10;
						if (e1_out(S16(pkt, 0x12), 0x6C0)) clip |= 0x20;
						if (e1_out(S16(pkt, 0x1A), 0x6C0)) clip |= 0x40;
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
							int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
							x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, pkt);
							pkt += 0x20;
						}
					}
				}
				rec += 0x14;
			} while (--n != 0);
		}
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x735DF0 (engine; MiniMog sub_735DF0): renders the textured-quad list (POLY_FT4, 0x18-byte
	// records -> 0x28-byte packets) of a primitive model: RTPT + RTPS, texture offset, GTE-flag
	// / backface (MAC0) / screen-clip rejection, optional depth-cue colour, InsertPrim at OT
	// arg2 [otz >> arg3]; returns the packet cursor
	uint32_t __cdecl a_735DF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		uint32_t ctx = a1;
		uint32_t pkt = a4;
		uint32_t list = U32(ctx, 0x20);
		int32_t count = S32(list, 0);
		uint32_t rec = list + 4;
		uint32_t vtx = U32(ctx, 4);
		U32(ctx, 0x20) = rec;
		if (count > 0)
		{
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
						uint32_t clip = e1_out(S16(pkt, 8), 0xA00) ? 1 : zero;
						if (e1_out(S16(pkt, 0x10), 0xA00)) clip |= 2;
						if (e1_out(S16(pkt, 0x18), 0xA00)) clip |= 4;
						if (e1_out(S16(pkt, 0xA), 0x6C0)) clip |= 0x10;
						if (e1_out(S16(pkt, 0x12), 0x6C0)) clip |= 0x20;
						if (e1_out(S16(pkt, 0x1A), 0x6C0)) clip |= 0x40;
						x::GTE_ReadSXY2(pkt + 0x20);
						x::GTE_AVSZ4();
						if (e1_out(S16(pkt, 0x20), 0xA00)) clip |= 8;
						if (e1_out(S16(pkt, 0x22), 0x6C0)) clip |= 0x80;
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
							int32_t z = S32(ctx, 0x2C) >> (a3 & 31);
							x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, pkt);
							pkt += 0x28;
						}
					}
				}
				rec += 0x18;
			} while (--n != 0);
		}
		U32(ctx, 0x20) = rec;
		return pkt;
	}

	// 0x736C00 (engine; MiniMog sub_736C00): update pass of the actor system: for every actor of
	// the list (sys+0x2C, next +4) runs its type's update (type 0 a_737ED0, type 1 a_740880)
	uint32_t __cdecl a_736C00(void)
	{
		uint32_t act = U32(U32(G(G_258FB78), 0), 0x2C);
		while (act != 0)
		{
			int32_t type = S16(act, 8);
			if (type == 0)
				a_737ED0(act);
			else if (type == 1)
				a_740880(act);
			act = U32(act, 4);
		}
		return 0; // void
	}

	// 0x737ED0 (engine; MiniMog sub_737ED0): update of a type-0 actor (emitter): first-frame
	// init (a_742D00, +0x62 set), motion by mode +0x68 (3: F_7424C0, 4: F_742610), emission
	// (a_737F50), frame counter +0x60, then a_741B60
	uint32_t __cdecl a_737ED0(uint32_t a1)
	{
		if (U16(a1, 0x62) == 0)
		{
			a_742D00(a1);
			U16(a1, 0x62) = (uint16_t)(U16(a1, 0x62) + 1);
		}
		int32_t mode = S8(a1, 0x68);
		if (mode == 3)
			callp(F(F_7424C0), a1);
		else if (mode == 4)
			callp(F(F_742610), a1);
		a_737F50(a1);
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) + 1);
		a_741B60(a1);
		return 0; // void
	}

	// 0x737F50 (engine; MiniMog sub_737F50): emission step of an emitter: for its frame +0x60
	// (<= 39) reads the spawn count from its definition's per-frame table (+0xC8) and spawns
	// them through F_737FD0 in batches of the definition's batch size (+0x36), then the rest
	uint32_t __cdecl a_737F50(uint32_t a1)
	{
		uint32_t sys = U32(G(G_258FB78), 0);
		int32_t k = S8(a1, 0x6A);
		uint32_t def = U32(U32(sys, 0x224), k * 4);
		int16_t frame = S16(a1, 0x60);
		if (frame > 0x27)
			return 0; // void
		uint32_t total = U8(U32(def, 0xC8), (int32_t)frame);
		if (total == 0)
			return 0; // void
		int32_t batch = S8(def, 0x36);
		int32_t full = (int32_t)total / batch;
		int32_t rest = (int32_t)total % batch;
		if (full > 0)
		{
			do
			{
				int32_t b = S8(def, 0x36);
				a_737FD0(a1, def, (uint32_t)b);
			} while (--full != 0);
		}
		if (rest > 0)
			a_737FD0(a1, def, (uint32_t)rest);
		return 0; // void
	}

	// 0x7387B0 (engine; MiniMog sub_7387B0): allocates a particle slot (0x2A0 bytes) in the 99-
	// slot pool G_257F720 (round-robin from G_257F698, in-use byte +0x1D7, 100 tries): clears
	// it, links the emitter arg1 (+0x1BC), definition index arg2 (+0x1D6), copies arg1+0x6B
	// (+0x1D9), counts it in sys+0x16 and initialises it (a_742CA0(slot, 1)); returns the slot
	// or 0
	uint32_t __cdecl a_7387B0(uint32_t a1, uint32_t a2)
	{
		uint32_t pool = U32(G(G_257F720), 0);
		int32_t i = S16(G(G_257F698), 0);
		uint32_t slot = 0;
		int32_t tries = 0;
		for (;;)
		{
			uint32_t off = (uint32_t)mul32(i, 0x2A0);
			if (U8(off + pool, 0x1D7) == 0)
			{
				slot = (uint32_t)mul32(i, 0x2A0) + pool;
				x::MAG_007_sub_8DCC00(slot, 0x2A0);
				uint8_t def_idx = (uint8_t)a2;
				U32(slot, 0x1BC) = a1;
				uint8_t b6b = U8(a1, 0x6B);
				uint32_t sys = U32(G(G_258FB78), 0);
				U8(slot, 0x1D7) = 1;
				U16(sys, 0x16) = (uint16_t)(U16(sys, 0x16) + 1);
				U8(slot, 0x1D6) = def_idx;
				U8(slot, 0x1D9) = b6b;
				a_742CA0(slot, 1);
				break;
			}
			i++;
			if (i >= 0x63)
				i = 0;
			tries++;
			if (tries >= 0x64)
				break;
		}
		i++;
		if (i >= 0x63)
			U16(G(G_257F698), 0) = 0;
		else
			U16(G(G_257F698), 0) = (uint16_t)i;
		return slot;
	}

	// 0x7395E0 (engine; MiniMog sub_7395E0): state: steps the actor system (a_7353B0); when it
	// reports done, sets the finished flag (+0x26 bit0) and advances the state index
	uint32_t __cdecl a_7395E0(uint32_t a1)
	{
		if (a_7353B0(a1) != 0)
		{
			uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x739890 (engine; MiniMog sub_739890): texture-animation script player (+0x110 script of
	// {u16 frame, u16 duration} pairs, +0x12C index, +0x12E page, +0x130 frame, +0x132 wait;
	// frame 0xFF = end, 0xFE = loop): on each new step uploads the frame to VRAM (a_739960)
	uint32_t __cdecl a_739890(uint32_t a1)
	{
		uint32_t scr = U32(a1, 0x110);
		if (scr == 0)
			return 0; // void
		uint32_t idx;
		uint16_t wait = U16(a1, 0x132);
		if (wait == 0)
		{
			idx = U16(a1, 0x12C);
		}
		else
		{
			uint16_t w = (uint16_t)(wait - 1);
			U16(a1, 0x132) = w;
			if ((int16_t)w > 0)
				return 0; // void
			U16(a1, 0x12C) = (uint16_t)(U16(a1, 0x12C) + 1);
			idx = U16(a1, 0x12C);
		}
		uint32_t ent = scr + idx * 4;
		uint16_t frame = U16(ent, 0);
		U16(a1, 0x130) = frame;
		U16(a1, 0x132) = U16(ent, 2);
		if (frame == 0xFF)
		{
			U32(a1, 0x110) = 0;
			return 0; // void
		}
		if (frame == 0xFE)
		{
			uint16_t f0 = U16(scr, 0);
			uint16_t d0 = U16(scr, 2);
			U16(a1, 0x12C) = 0;
			U16(a1, 0x130) = f0;
			U16(a1, 0x132) = d0;
		}
		// quirk 0x739933/0x73993A: the two u16 arguments are pushed as full registers whose high
		// halves are those of the entry / script addresses (a_739960 masks them with 0xFFFF)
		uint32_t arg_frame = (ent & 0xFFFF0000) | U16(a1, 0x130);
		uint32_t arg_page = (scr & 0xFFFF0000) | U16(a1, 0x12E);
		a_739960(a1 + 0x30, arg_page, arg_frame);
		return 0; // void
	}

	// 0x739960 (engine; MiniMog sub_739960): uploads texture-animation frame arg3 of page arg2
	// of a model (arg1+0x84 header, +0x30 page table): picks the VRAM page among the header's
	// 12-bit page mask (+2), builds the destination rect (x = cell x + (13 - page/2)*64,
	// y = cell y + (page&1)*128 + 256, w/h from the page record) and queues a VRAM blit
	uint32_t __cdecl a_739960(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		uint32_t hdr = U32(a1, 0x84);
		uint32_t tab = U32(hdr, 0x30);
		if (tab == 0)
			return 0; // void
		uint8_t off = U8(tab + (a2 & 0xFFFF), 0);
		if (off == 0)
			return 0; // void
		uint32_t rec = (uint32_t)off + tab;
		uint32_t cell = rec + (a3 & 0xFFFF) * 2 + 5;
		int32_t page = U8(rec, 0);
		uint32_t mask = U16(hdr, 2);
		for (int32_t bit = 0; bit < 12; bit++)
		{
			if (mask & (1u << bit))
			{
				int32_t t = page;
				page--;
				if (t == 0)
				{
					page = bit;
					break;
				}
			}
		}
		int32_t parity = page & 1;
		int32_t half = page / 2;
		int32_t xbase = shl32(13 - half, 6);
		int32_t ybit = shl32(parity, 7);
		uint16_t rect[4];
		rect[0] = (uint16_t)((uint32_t)U8(cell, 0) + (uint32_t)xbase);
		rect[1] = (uint16_t)((uint32_t)U8(cell, 1) + (uint32_t)ybit + 0x100);
		rect[2] = U8(rec, 3);
		rect[3] = U8(rec, 4);
		// high halves of the pushed registers come from `half` (0 here: page < 256)
		uint32_t hi = (uint32_t)half & 0xFFFF0000;
		uint32_t arg_y = (hi | U8(rec, 2)) + (uint32_t)ybit + 0x100;
		uint32_t arg_x = (hi | U8(rec, 1)) + (uint32_t)xbase;
		x::QueueBlitCommand(P(rect), arg_x, arg_y);
		return 0; // void
	}

	// 0x739AC0 (engine; MiniMog sub_739AC0): starts a texture-animation script: +0x110 = script
	// arg2, +0x12E = page arg3, index / frame / wait cleared
	uint32_t __cdecl a_739AC0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		U32(a1, 0x110) = a2;
		U16(a1, 0x12E) = (uint16_t)a3;
		U16(a1, 0x12C) = 0;
		U16(a1, 0x130) = 0;
		U16(a1, 0x132) = 0;
		return 0; // void
	}

	// 0x739B40 (engine; MiniMog sub_739B40): state: when the node's model animation reports
	// completion (au_re_Battle_ReadAnimation_8 == 1), calls au_re_Battle_ReadAnimation_7(node, 1)
	// and advances the state index
	uint32_t __cdecl a_739B40(uint32_t a1)
	{
		uint32_t r = x::au_re_Battle_ReadAnimation_8(a1);
		if (r == 1)
		{
			x::au_re_Battle_ReadAnimation_7(a1, r);
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return 0; // void
	}

	// 0x8DDC30 (GLOBAL engine code): screen-tint task: 3-state machine {0x8DDCA0 delay,
	// 0x8DDCD0 colour script + draw, 0x8DDEB0}, script clock +0x3C (not while paused,
	// 0x2795914), frame counter +0x24; ends once +0x26 bit0 is set and no child is alive
	uint32_t __cdecl a_8DDC30(uint32_t a1)
	{
		uint32_t tab[3];
		tab[0] = 0x8DDCA0;
		tab[1] = 0x8DDCD0;
		tab[2] = 0x8DDEB0;
		callp(tab[(int32_t)S8(a1, 0x29)], a1);
		if (U32(0x2795914, 0) == 0)
			U16(a1, 0x3C) = (uint16_t)(U16(a1, 0x3C) + 1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((status & 1) && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x8DDCA0 (GLOBAL engine code): state 0 of the screen-tint task: counts the delay +0x38
	// down (not while paused, 0x2795914) and advances the state index when it reaches 0
	uint32_t __cdecl a_8DDCA0(uint32_t a1)
	{
		if (U32(0x2795914, 0) == 0)
		{
			U16(a1, 0x38) = (uint16_t)(U16(a1, 0x38) - 1);
			if (S16(a1, 0x38) <= 0)
			{
				uint8_t st = U8(a1, 0x29);
				U16(a1, 0x38) = 0;
				U8(a1, 0x29) = (uint8_t)(st + 1);
			}
		}
		return 0; // void
	}

	// 0x8DDCD0 (GLOBAL engine code): state 1 of the screen-tint task: steps the colour script
	// (+0x34 dwords, index +0x3A; top byte 0xFF = end -> finished flag, 0xFE = hold until the
	// frame counter +0x24 reaches byte 2 then take the next dword, else the dword is the colour
	// +0x30), then draws four semi-transparent flat quads covering the 320x240 screen (static
	// packets 0x2795110..0x279516C) plus a draw-mode packet (0x2795170) into OT base + 0x1C
	uint32_t __cdecl a_8DDCD0(uint32_t a1)
	{
		uint16_t idx = U16(a1, 0x3A);
		uint32_t script = U32(a1, 0x34);
		uint32_t ent = U32(script, (int32_t)(int16_t)idx * 4);
		// quirk 0x8DDCE3: the entry is stored into this function's own argument slot
		// ([esp+0xC]); the caller does not re-read it, not observable
		uint8_t op = (uint8_t)(ent >> 24);
		uint8_t b2 = (uint8_t)(ent >> 16);
		if (op == 0xFF)
		{
			U8(a1, 0x26) = (uint8_t)(U8(a1, 0x26) | 1);
			return 0; // void
		}
		if (op == 0xFE)
		{
			if (S16(a1, 0x24) >= (int16_t)(uint16_t)b2 && U32(0x2795914, 0) == 0)
			{
				uint16_t ni = (uint16_t)(idx + 1);
				U16(a1, 0x3A) = ni;
				U32(a1, 0x30) = U32(script, (int32_t)(int16_t)ni * 4);
			}
		}
		else
		{
			U32(a1, 0x30) = ent;
			if (U32(0x2795914, 0) == 0)
				U16(a1, 0x3A) = (uint16_t)(idx + 1);
		}
		uint32_t ot = U32(0x1D8E04C, 0) + 0x1C;
		const uint8_t code = 0x2A;        // POLY_F4, semi-transparent
		const uint32_t v_mid = 0x7800A0;  // (160, 120)

		U32(0x2795114, 0) = U32(a1, 0x30);
		U32(0x2795110, 0) = 0x5000000;
		U8(0x2795117, 0) = code;
		U32(0x2795118, 0) = 0;
		U32(0x279511C, 0) = 0xA0;
		U32(0x2795120, 0) = 0x780000;
		U32(0x2795124, 0) = v_mid;
		x::SSIGPU_InsertPrimAltViewport(ot, 0x2795110);

		U32(0x279512C, 0) = U32(a1, 0x30);
		U32(0x2795128, 0) = 0x5000000;
		U8(0x279512F, 0) = code;
		U32(0x2795130, 0) = 0xA0;
		U32(0x2795134, 0) = 0x140;
		U32(0x2795138, 0) = v_mid;
		U32(0x279513C, 0) = 0x780140;
		x::SSIGPU_InsertPrimAltViewport(ot, 0x2795128);

		U32(0x2795144, 0) = U32(a1, 0x30);
		U32(0x2795140, 0) = 0x5000000;
		U8(0x2795147, 0) = code;
		U32(0x2795148, 0) = 0x780000;
		U32(0x279514C, 0) = v_mid;
		U32(0x2795150, 0) = 0xF00000;
		U32(0x2795154, 0) = 0xF000A0;
		x::SSIGPU_InsertPrimAltViewport(ot, 0x2795140);

		U32(0x279515C, 0) = U32(a1, 0x30);
		U32(0x2795158, 0) = 0x5000000;
		U8(0x279515F, 0) = code;
		U32(0x2795160, 0) = v_mid;
		U32(0x2795164, 0) = 0x780140;
		U32(0x2795168, 0) = 0xF000A0;
		U32(0x279516C, 0) = 0xF00140;
		x::SSIGPU_InsertPrimAltViewport(ot, 0x2795158);

		uint32_t tpage = x::sub_45C690(0, 1, 0x280, 0) & 0xFFFF;
		x::sub_45BFC0(0x2795170, 0, 0, tpage, 0);
		x::SSIGPU_InsertPrimAltViewport(ot, 0x2795170);
		return 0; // void
	}

	// ====================================================================================
	// part e2
	// ====================================================================================
	// Part e2 of the actor state-machine GF family port (Siren 095 / MiniMog 096 "Cure engine"):
	// the Siren master sequence task, the file-load script task, the scripted battle camera
	// (keyframe camera 0x73AB20/0x73AE10/0x73B160 and channel-script camera 0x73B3B0/0x73B4B0)
	// and the small state helpers of the master's state tables.
	namespace
	{
		// camera state block (pointer held in module global G_15339D0)
		inline uint32_t e2_cam() { return MEM<uint32_t>(G(G_15339D0)); }
		// script director block (pointer held in module global G_1533010)
		inline uint32_t e2_dir() { return MEM<uint32_t>(G(G_1533010)); }
		// load-script context (pointer held in module global G_258EAA0)
		inline uint32_t e2_ctx() { return MEM<uint32_t>(G(G_258EAA0)); }
		// load-script cursor (16-byte records, pointer held in module global G_258BFA8)
		inline uint32_t e2_rec() { return MEM<uint32_t>(G(G_258BFA8)); }

		// ((to - from) * t) >> 12 rounded toward zero (imul r32 / cdq / and 0xfff / add / sar 12
		// = C '/' 4096 on int32, identical for every 32-bit input)
		inline int32_t e2_mulq12(int32_t to, int32_t from, int32_t t)
		{
			return mul32(sub32(to, from), t) / 4096;
		}

		// angle lerp of a_73AE10 (12-bit angles): wrap_from == false -> when from > to, to += 0x1000;
		// wrap_from == true -> when from < to, from += 0x1000 (16-bit wraps as the movsx of the
		// 16-bit register does); result & 0xfff
		inline uint16_t e2_angle_lerp(uint32_t c, int32_t from_off, int32_t to_off, bool wrap_from)
		{
			int16_t si = S16(c, from_off);
			int16_t ax = S16(c, to_off);
			if (!wrap_from)
			{
				if (si > ax) ax = (int16_t)(uint16_t)((uint16_t)ax + 0x1000);
			}
			else
			{
				if (si < ax) si = (int16_t)(uint16_t)((uint16_t)si + 0x1000);
			}
			int32_t r = e2_mulq12((int32_t)ax, (int32_t)si, (int32_t)S16(c, 0xD0));
			// add eax, esi: only the low 12 bits survive, the stale high half of esi is irrelevant
			return (uint16_t)(add32(r, (int32_t)si) & 0xFFF);
		}

		// plain lerp of a_73AE10 (16-bit store): from + ((to - from) * t >> 12)
		inline uint16_t e2_lerp(uint32_t c, int32_t from_off, int32_t to_off)
		{
			int16_t si = S16(c, from_off);
			int32_t r = e2_mulq12((int32_t)S16(c, to_off), (int32_t)si, (int32_t)S16(c, 0xD0));
			return (uint16_t)add32(r, (int32_t)si);
		}
	}

	// 0x739F40 (engine; Siren GF_095Siren_SequenceTick, also 099/100): master sequence task -
	// snapshots the camera matrix into 0x2793E58, picks the double-buffered module pointers from
	// its tick parity (+0x5C), runs its 11-state table, then executes the 9 effect task queues
	// (sum of their results in +0x5E), advances its counters and ends when finished and idle
	uint32_t __cdecl a_739F40(uint32_t a1)
	{
		set_mod_by_code(U32(a1, 8));      // node +8 = original task function address -> current module
		g_ported_tick = g_real_tick;

		// rep movsd: camera matrix (Mat4x3, 8 dwords) -> shared snapshot 0x2793E58
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4);
		MEM<uint32_t>(G(G_258FB68)) = 0x2793E58;
		MEM<uint32_t>(G(G_257F9B0)) = 0x2793E58;
		uint32_t states[11] = {
			F(F_73A0D0), F(F_73A0E0), F(F_73A0F0), F(F_73A170), F(F_747500), F(F_747540),
			F(F_747550), F(F_747560), F(F_747590), F(F_7475A0), F(F_7475C0),
		};
		if (U8(a1, 0x5C) & 1) // +0x5C tick counter parity -> buffer 1
		{
			uint32_t v1 = MEM<uint32_t>(G(G_258BFA4));
			uint32_t v2 = MEM<uint32_t>(G(G_258A024));
			MEM<uint32_t>(G(G_257F8A4)) = v1;
			MEM<uint32_t>(G(G_257F9AC)) = v2;
		}
		else
		{
			uint32_t v1 = MEM<uint32_t>(G(G_258BFA0));
			uint32_t v2 = MEM<uint32_t>(G(G_258A020));
			MEM<uint32_t>(G(G_257F8A4)) = v1;
			MEM<uint32_t>(G(G_257F9AC)) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(a1);
		callp(states[S8(a1, 0x29)], a1); // +0x29 state index

		U16(a1, 0x5E) = 0; // live task count of this tick
		U16(G(G_258FB40), 0) = 0;
		U16(G(G_258EC50), 0) = 0;
		static const int queues[9] = { G_258BE30, G_257F988, G_258FB48, G_258EC40, G_258BA90,
			G_258A010, G_257FA90, G_2585EF0, G_258BE20 };
		for (int i = 0; i < 9; i++)
		{
			uint32_t n = x::ExecuteTaskQueue(G(queues[i]));
			U16(a1, 0x5E) = (uint16_t)(U16(a1, 0x5E) + (uint16_t)n);
		}
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x5C) = (uint16_t)(U16(a1, 0x5C) + 1);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1); // frame counter
		if ((status & 1) && U8(a1, 0x28) == 0) // finished and no live children
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x73A0D0 (engine): state helper - advances the node's state index (+0x29)
	uint32_t __cdecl a_73A0D0(uint32_t a1)
	{
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73A170 (engine): state helper - sets +0x63, spawns the task 0x73A1A0 (node 0x58) into
	// queue G_258BE30 as a child of the node, advances the state
	uint32_t __cdecl a_73A170(uint32_t a1)
	{
		U8(a1, 0x63) = 1;
		x::Effect_AddTaskAndInitFromCtx(G(G_258BE30), F(F_73A1A0), 0x58, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x73A1A0 (engine; TASK): emitter-style task - updates its position and model bounds, runs
	// its 5-state table (0x73A220, 0x747440, 0x7474B0, 0x7474D0, 0x7474F0), ticks the frame counter
	uint32_t __cdecl a_73A1A0(uint32_t a1)
	{
		uint32_t states[5] = { F(F_73A220), F(F_747440), F(F_7474B0), F(F_7474D0), F(F_7474F0) };
		x::MAG_001_CURE_Emitter_UpdatePos(a1);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(a1);
		callp(states[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((status & 1) && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x73A380 (engine; TASK): file-load script task - state 0 issues the command at the
	// load-script cursor (0x73A3E0), state 1 waits for it (0x73A660), state 2 idles (nullsub)
	uint32_t __cdecl a_73A380(uint32_t a1)
	{
		// this task body exists several times per module with different state tables (Siren
		// 0x73A380 0x73F800 0x73F900 0x743250 0x7447C0 0x745180 0x745250 0x746920 0x746A40, MiniMog
		// 0x732390 0x733570 0x733810 0x733BE0 0x733CE0 0x734840 ...): the table is read from the
		// running copy's own code (the three `mov [esp+x], imm32` at +0x0D/+0x19/+0x21)
		const uint32_t fn = U32(a1, 8);
		uint32_t states[3] = { U32(fn, 0x0D), U32(fn, 0x19), U32(fn, 0x21) };
		callp(states[S8(a1, 0x29)], a1);
		uint8_t status = U8(a1, 0x26);
		U16(a1, 0x24) = (uint16_t)(U16(a1, 0x24) + 1);
		if ((status & 1) && U8(a1, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(a1);
			return 2;
		}
		return 0;
	}

	// 0x73A3E0 (engine): load-script command, record {u16 op, u16 file, u16 slot, .., +8 size,
	// +0xC dest}: op 0 = clear ctx+0x166; op 1 / 6 = load file to ctx+0x100 (callback TIM
	// upload 0x73A580 / summon stream 0x73A5B0); ops 2..5 = load into slot table
	// ctx+0x104/0x124/0x144/0x114 and record it (0x73A620); op 0xFF = end (finishes the node);
	// loads set ctx+0x164 busy and ++state
	uint32_t __cdecl a_73A3E0(uint32_t a1)
	{
		uint32_t rec = e2_rec();
		uint32_t op = U16(rec, 0);
		if ((int32_t)op > 0xFF)
			return 0; // void
		if (op == 0xFF)
		{
			U16(e2_ctx(), 0x166) = 0;
			U8(a1, 0x26) |= 1;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		if (op > 6)
			return 0; // void
		uint32_t ctx, cb, dst;
		switch (op)
		{
		case 0:
			U16(e2_ctx(), 0x166) = 0;
			return 0; // void
		case 1:
			ctx = e2_ctx();
			U16(ctx, 0x164) = 1;
			dst = U32(ctx, 0x100);
			U32(rec, 0xC) = dst;
			x::pre_LoadBattleFile(U16(rec, 2), dst, 0, F(F_73A580));
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		case 6:
			ctx = e2_ctx();
			U16(ctx, 0x164) = 1;
			dst = U32(ctx, 0x100);
			U32(rec, 0xC) = dst;
			x::pre_LoadBattleFile(U16(rec, 2), dst, 0, F(F_73A5B0));
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		default:
		{
			int32_t base;
			if (op == 2) { cb = F(F_73A5E0); base = 0x104; }
			else if (op == 3) { cb = F(F_73A5F0); base = 0x124; }
			else if (op == 4) { cb = F(F_73A600); base = 0x144; }
			else { cb = F(F_73A610); base = 0x114; }
			ctx = e2_ctx();
			uint32_t slot = U16(rec, 4);
			U16(ctx, 0x164) = 1;
			dst = U32(ctx, (int32_t)(slot * 4) + base);
			U32(rec, 0xC) = dst;
			x::pre_LoadBattleFile(U16(rec, 2), dst, 0, cb);
			a_73A620();
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return 0; // void
		}
		}
	}

	// 0x73A580 (engine): load callback - queues the loaded TIM (record +0xC) for VRAM upload,
	// clears the busy flag ctx+0x164
	uint32_t __cdecl a_73A580(void)
	{
		x::Battle_QueueTIMUpload_GetEOF(U32(e2_rec(), 0xC));
		U16(e2_ctx(), 0x164) = 0;
		return 0; // void
	}

	// 0x73A5B0 (engine): load callback - hands the loaded summon stream (record +0xC) to
	// BdTransSummonStream with ready flag ctx+0x16A, clears ctx+0x164
	uint32_t __cdecl a_73A5B0(void)
	{
		uint32_t flag = e2_ctx() + 0x16A;
		uint32_t data = U32(e2_rec(), 0xC);
		x::BdTransSummonStream(data, flag);
		U16(e2_ctx(), 0x164) = 0;
		return 0; // void
	}

	// 0x73A5E0 (engine): load callback - clears the busy flag ctx+0x164
	uint32_t __cdecl a_73A5E0(void)
	{
		U16(e2_ctx(), 0x164) = 0;
		return 0; // void
	}

	// 0x73A620 (engine): records the loaded file: ctx[n] = start (+0xC), ctx[0x20 + n] = end
	// (start + size +8), n = ctx+0x168 (then ++)
	uint32_t __cdecl a_73A620(void)
	{
		uint32_t ctx = e2_ctx();
		uint32_t rec = e2_rec();
		int32_t n = S16(ctx, 0x168);
		uint32_t start = U32(rec, 0xC);
		uint32_t size = U32(rec, 8);
		U32(ctx, n * 4) = start;
		uint32_t end = start + size;
		int32_t n2 = S16(ctx, 0x168);
		U32(ctx, n2 * 4 + 0x80) = end;
		U16(ctx, 0x168) = (uint16_t)(U16(ctx, 0x168) + 1);
		return 0; // void
	}

	// 0x73A660 (engine): load-script wait state - finished node: ++state; else when the load is
	// done (ctx+0x164 == 0; op 6 also needs the stream ready flag ctx+0x16A, which it clears)
	// advances the cursor by one record and goes back to state 0 (0x73A3E0)
	uint32_t __cdecl a_73A660(uint32_t a1)
	{
		if (U8(a1, 0x26) & 1)
		{
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
			return a1;
		}
		if (U16(e2_rec(), 0) == 6)
		{
			uint32_t ctx = e2_ctx();
			if (U16(ctx, 0x164) != 0) return a1;
			if (U8(ctx, 0x16A) == 0) return a1;
			U8(ctx, 0x16A) = 0;
		}
		else
		{
			if (U16(e2_ctx(), 0x164) != 0) return a1;
		}
		uint32_t next = e2_rec() + 0x10;
		uint8_t st = (uint8_t)(U8(a1, 0x29) - 1);
		MEM<uint32_t>(G(G_258BFA8)) = next;
		U8(a1, 0x29) = st;
		return a1;
	}

	// 0x73A6D0 (engine; nullsub_1171): empty state
	uint32_t __cdecl a_73A6D0(void)
	{
		return 0; // void
	}

	// 0x73A720 (engine): centre of the visible monsters (battle entities 3..6 with flag bit 1):
	// out[0] = (min x + max x) / 2, out[1] = 0, out[2] = (min z + max z) / 2 (entity +0x1C/+0x20)
	uint32_t __cdecl a_73A720(uint32_t a1)
	{
		uint32_t count = 0;
		int16_t max_z = 0, min_z = 0, max_x = 0, min_x = 0;
		for (uint32_t e = 0x1D974B4; e < 0x1D97724; e += 0x9C)
		{
			if (!(U8(e, -0x20) & 2))
				continue;
			if ((uint16_t)count == 0)
			{
				max_x = S16(e, -4);
				max_z = S16(e, 0);
				min_x = max_x;
				min_z = max_z;
			}
			else
			{
				int16_t x = S16(e, -4);
				if (x < min_x) min_x = x;
				else if (x > max_x) max_x = x;
				int16_t z = S16(e, 0);
				if (z < min_z) min_z = z;
				else if (z > max_z) max_z = z;
			}
			count++;
		}
		int32_t cx = ((int32_t)max_x + (int32_t)min_x) / 2;
		U16(a1, 2) = 0;
		U16(a1, 0) = (uint16_t)cx;
		int32_t cz = ((int32_t)max_z + (int32_t)min_z) / 2;
		U16(a1, 4) = (uint16_t)cz;
		return (uint32_t)cz;
	}

	// 0x73A930 (engine): state helper - arms the next load-script step (0x73A950); ++state when armed
	uint32_t __cdecl a_73A930(uint32_t a1)
	{
		uint32_t r = a_73A950();
		if (r != 1)
			return r;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73A950 (engine): if ctx+0x166 is clear and the load-script record is op 0: sets
	// ctx+0x166, advances the cursor by one record, returns 1; else 0
	uint32_t __cdecl a_73A950(void)
	{
		uint32_t ctx = e2_ctx();
		if (U16(ctx, 0x166) != 0)
			return 0;
		uint32_t rec = e2_rec();
		if (U16(rec, 0) != 0)
			return 0;
		U16(ctx, 0x166) = 1;
		MEM<uint32_t>(G(G_258BFA8)) = rec + 0x10;
		return 1;
	}

	// 0x73AAE0 (engine): director step test - (s16)dir+0x42 >= (s16)a1
	uint32_t __cdecl a_73AAE0(uint32_t a1)
	{
		uint32_t dir = e2_dir();
		return S16(dir, 0x42) >= (int16_t)a1 ? 1u : 0u;
	}

	// 0x73AB00 (engine): MAG_007_sub_8DCC00(camera block, 0xEC) (resets the camera block)
	uint32_t __cdecl a_73AB00(void)
	{
		uint32_t cam = e2_cam();
		return x::MAG_007_sub_8DCC00(cam, 0xEC);
	}

	// 0x73AB20 (engine): starts a keyframe camera move. a1 = camera key {+0 t-curve ptr,
	// +4/+0xC (mode 0) or +0x14/+0x1C (mode 1) start/end local vectors, +0x24.. params},
	// a2 = base position, a3 = 0: start angles/roll/distance from the key, 1: from the current
	// camera. Fills the camera block, computes the start/end points (base + RotY(cam+0xE8) *
	// vector), then evaluates the first step (0x73AE10) and returns its "done" flag.
	uint32_t __cdecl a_73AB20(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// stack frame 0x28: [0x00..0x07] vector out (B), [0x08..0x27] rotation matrix (A)
		alignas(4) uint8_t fr[0x28] = {};
		uint32_t vB = P(fr), mA = P(fr) + 8;
		x::MAG_022_sub_8DD770(mA);
		uint32_t c = e2_cam();
		x::MAG_022_sub_8DD8A0(mA, (uint32_t)(int32_t)S16(c, 0xE8));
		c = e2_cam();
		U32(c, 0xAC) = a1;
		U16(c, 0xE2) = 0; // step counter
		U16(c, 0xD0) = 0; // t (4.12)
		U32(c, 0x50) = U32(a1, 0); // t curve pointer (0 = speed-driven)
		U32(c, 0xA4) = U32(a2, 0); // base position
		U32(c, 0xA8) = U32(a2, 4);
		int16_t mode = (int16_t)a3;
		if (mode == 0)
		{
			U16(c, 0xC2) = U16(a1, 0x2A);
			U16(c, 0xC6) = U16(a1, 0x2E);
			U16(c, 0xCA) = U16(a1, 0x32);
			U16(c, 0xBA) = U16(a1, 0x36);
			U16(c, 0xBE) = U16(a1, 0x26);
		}
		else if (mode == 1)
		{
			U16(c, 0xC2) = U16(c, 0xD8);
			U16(c, 0xC6) = U16(c, 0xDA);
			U16(c, 0xCA) = U16(c, 0xDC);
			U16(c, 0xBA) = U16(c, 0xE0);
			U16(c, 0xBE) = U16(c, 0xDE);
		}
		U16(c, 0xD2) = U16(a1, 0x3A); // speed
		U16(c, 0xD4) = U16(a1, 0x3C); // acceleration
		U16(c, 0xD6) = U16(a1, 0x3E); // speed limit
		U16(c, 0xCE) = U16(a1, 0x24); // camera mode (0 = look-at driven, 1 = eye driven)
		U16(c, 0xC4) = U16(a1, 0x2C);
		U16(c, 0xC8) = U16(a1, 0x30);
		U16(c, 0xCC) = U16(a1, 0x34);
		U16(c, 0xB4) = U16(a1, 0x40); // angle wrap direction flags
		U16(c, 0xB6) = U16(a1, 0x42);
		U16(c, 0xB8) = U16(a1, 0x44);
		U16(c, 0xBC) = U16(a1, 0x38);
		U16(c, 0xC0) = U16(a1, 0x28);
		U16(c, 0xC6) = (uint16_t)((uint16_t)(U16(c, 0xC6) + U16(c, 0xE8)) & 0xFFF);
		U16(c, 0xC8) = (uint16_t)((uint16_t)(U16(c, 0xC8) + U16(c, 0xE8)) & 0xFFF);
		int16_t cam_mode = S16(c, 0xCE);
		if (cam_mode == 1)
		{
			U32(c, 0x74) = U32(c, 0xA4);
			U32(c, 0x78) = U32(c, 0xA8);
			x::matrixMultiplyVector(mA, a1 + 0x14, vB);
			c = e2_cam();
			U16(c, 0x74) = (uint16_t)(U16(c, 0x74) + U16(vB, 0));
			U16(c, 0x76) = (uint16_t)(U16(c, 0x76) + U16(vB, 2));
			U16(c, 0x78) = (uint16_t)(U16(c, 0x78) + U16(vB, 4));
			U32(c, 0x7C) = U32(c, 0xA4);
			U32(c, 0x80) = U32(c, 0xA8);
			x::matrixMultiplyVector(mA, a1 + 0x1C, vB);
			c = e2_cam();
			U16(c, 0x7C) = (uint16_t)(U16(c, 0x7C) + U16(vB, 0));
			U16(c, 0x7E) = (uint16_t)(U16(c, 0x7E) + U16(vB, 2));
			U16(c, 0x80) = (uint16_t)(U16(c, 0x80) + U16(vB, 4));
		}
		else if (cam_mode == 0)
		{
			U32(c, 0x64) = U32(c, 0xA4);
			U32(c, 0x68) = U32(c, 0xA8);
			x::matrixMultiplyVector(mA, a1 + 4, vB);
			c = e2_cam();
			U16(c, 0x64) = (uint16_t)(U16(c, 0x64) + U16(vB, 0));
			U16(c, 0x66) = (uint16_t)(U16(c, 0x66) + U16(vB, 2));
			U16(c, 0x68) = (uint16_t)(U16(c, 0x68) + U16(vB, 4));
			U32(c, 0x6C) = U32(c, 0xA4);
			U32(c, 0x70) = U32(c, 0xA8);
			x::matrixMultiplyVector(mA, a1 + 0xC, vB);
			c = e2_cam();
			U16(c, 0x6C) = (uint16_t)(U16(c, 0x6C) + U16(vB, 0));
			U16(c, 0x6E) = (uint16_t)(U16(c, 0x6E) + U16(vB, 2));
			U16(c, 0x70) = (uint16_t)(U16(c, 0x70) + U16(vB, 4));
		}
		return a_73AE10();
	}

	// 0x73AE10 (engine): keyframe camera step - ++step (+0xE2), advances t (+0xD0, 4.12) from the
	// t curve (+0x50, u16 per step, 0x1000 = last) or by speed += acceleration (+0xD2/+0xD4,
	// limit +0xD6), clamps at 0x1000 (returns 1 = move done), lerps the angles D8/DA/DC (12-bit,
	// wrap flags B4/B6/B8), roll E0, distance DE and the driven point (mode CE: 0 = look-at
	// 5C..60 from 64..6C, 1 = eye base 54..58 from 74..7C), then writes the battle camera (0x73B160)
	uint32_t __cdecl a_73AE10(void)
	{
		g_cam_tick = g_real_tick; // held-frame camera: this stepper ran on this tick
		g_cam_kind = 1;
		uint32_t c = e2_cam();
		uint32_t done = 0;
		U16(c, 0xE2) = (uint16_t)(U16(c, 0xE2) + 1);
		uint32_t curve = U32(c, 0x50);
		if (curve == 0)
		{
			U16(c, 0xD2) = (uint16_t)(U16(c, 0xD2) + U16(c, 0xD4)); // speed += accel
			int16_t accel = S16(c, 0xD4);
			int16_t speed = S16(c, 0xD2);
			if (accel > 0)
			{
				int16_t lim = S16(c, 0xD6);
				if (speed > lim) U16(c, 0xD2) = (uint16_t)lim;
			}
			else if (accel < 0)
			{
				int16_t lim = S16(c, 0xD6);
				if (speed < lim) U16(c, 0xD2) = (uint16_t)lim;
			}
			U16(c, 0xD0) = (uint16_t)(U16(c, 0xD0) + U16(c, 0xD2));
		}
		else
		{
			uint16_t t = U16(curve, 0);
			U16(c, 0xD0) = t;
			if (t != 0x1000)
				U32(c, 0x50) = curve + 2;
		}
		if (S16(c, 0xD0) >= 0x1000)
		{
			U16(c, 0xD0) = 0x1000;
			done = 1;
		}

		// angle D8 from C2 -> C4 (B4 == 0: wrap the end, else wrap the start)
		{
			bool wrap_from = U16(c, 0xB4) != 0;
			U16(c, 0xD8) = e2_angle_lerp(c, 0xC2, 0xC4, wrap_from);
		}
		// angle DA from C6 -> C8 (B6: 0 / 1; any other value leaves DA unchanged)
		{
			int16_t sel = S16(c, 0xB6);
			if (sel == 0)
				U16(c, 0xDA) = e2_angle_lerp(c, 0xC6, 0xC8, false);
			else if (sel == 1)
				U16(c, 0xDA) = e2_angle_lerp(c, 0xC6, 0xC8, true);
		}
		// angle DC from CA -> CC
		{
			bool wrap_from = U16(c, 0xB8) != 0;
			U16(c, 0xDC) = e2_angle_lerp(c, 0xCA, 0xCC, wrap_from);
		}
		U16(c, 0xE0) = e2_lerp(c, 0xBA, 0xBC); // -> 0x1D8E038
		U16(c, 0xDE) = e2_lerp(c, 0xBE, 0xC0); // distance
		int16_t cam_mode = S16(c, 0xCE);
		if (cam_mode == 1)
		{
			U16(c, 0x54) = e2_lerp(c, 0x74, 0x7C);
			U16(c, 0x56) = e2_lerp(c, 0x76, 0x7E);
			U16(c, 0x58) = e2_lerp(c, 0x78, 0x80);
		}
		else if (cam_mode == 0)
		{
			U16(c, 0x5C) = e2_lerp(c, 0x64, 0x6C);
			U16(c, 0x5E) = e2_lerp(c, 0x66, 0x6E);
			U16(c, 0x60) = e2_lerp(c, 0x68, 0x70);
		}
		a_73B160();
		return done;
	}

	// 0x73B160 (engine): writes the battle camera from the camera block: offset = Rot(DA, D8) *
	// (0, 0, DE) (GTE MVMVA); mode CE 0: eye = look-at(5C..60) + offset (stored to 54..58);
	// mode 1: eye = 54..58 + offset, look-at = eye + offset (stored to 5C..60); then
	// 0xB8B7F0/F4 = eye, 0xB8B7F8/FC = look-at, 0x1D8E038 = E0, 0x1D977A2 = DC
	uint32_t __cdecl a_73B160(void)
	{
		// stack frame 0x38: [0..7] eye (3 x s16 + pad), [8..0xF] rotated offset (IR1..3),
		// [0x10..0x15] V0 = (0, 0, distance), [0x18..0x37] rotation matrix
		alignas(4) uint8_t fr[0x38] = {};
		uint32_t L = P(fr);
		// UNINIT 0x73B293 / 0x73B2B1 / 0x73B281: the eye words L+0..L+5 are never written when the
		// mode CE is neither 0 nor 1, and the pad word L+6 is never written at all (it goes to
		// 0xB8B7F6 and, in mode 0, to cam+0x5A): we start them with the current camera words
		U16(L, 0) = MEM<uint16_t>(0xB8B7F0);
		U16(L, 2) = MEM<uint16_t>(0xB8B7F2);
		U16(L, 4) = MEM<uint16_t>(0xB8B7F4);
		U16(L, 6) = MEM<uint16_t>(0xB8B7F6);
		uint32_t mat = L + 0x18;
		x::MAG_022_sub_8DD770(mat);
		uint32_t c = e2_cam();
		x::MAG_022_sub_8DD8A0(mat, (uint32_t)(int32_t)S16(c, 0xDA));
		c = e2_cam();
		x::sub_8DD7E0(mat, (uint32_t)(int32_t)S16(c, 0xD8));
		c = e2_cam();
		U16(L, 0x10) = 0;
		U16(L, 0x12) = 0;
		U16(L, 0x14) = U16(c, 0xDE);
		x::GTE_SetRotMatrix(mat);
		x::GTE_LoadV0(L + 0x10);
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(L + 8);
		c = e2_cam();
		int16_t cam_mode = S16(c, 0xCE);
		if (cam_mode == 1)
		{
			U16(L, 0) = (uint16_t)(U16(c, 0x54) + U16(L, 8));
			U16(L, 2) = (uint16_t)(U16(c, 0x56) + U16(L, 0xA));
			U16(L, 4) = (uint16_t)(U16(c, 0x58) + U16(L, 0xC));
			U16(c, 0x5C) = (uint16_t)(U16(L, 0) + U16(L, 8));
			U16(c, 0x5E) = (uint16_t)(U16(L, 2) + U16(L, 0xA));
			U16(c, 0x60) = (uint16_t)(U16(L, 4) + U16(L, 0xC));
		}
		else if (cam_mode == 0)
		{
			U16(L, 0) = (uint16_t)(U16(c, 0x5C) + U16(L, 8));
			U16(L, 2) = (uint16_t)(U16(c, 0x5E) + U16(L, 0xA));
			U16(L, 4) = (uint16_t)(U16(c, 0x60) + U16(L, 0xC));
			U32(c, 0x54) = U32(L, 0);
			U32(c, 0x58) = U32(L, 4); // quirk 0x73B284: dword store, the stack pad word L+6 lands in cam+0x5A
		}
		uint32_t look_xy = U32(c, 0x5C);
		uint32_t look_z = U32(c, 0x60); // quirk 0x73B28A: dword read, cam+0x62 goes to 0xB8B7FE
		MEM<uint32_t>(0xB8B7F8) = look_xy;
		MEM<uint32_t>(0xB8B7F0) = U32(L, 0);
		uint16_t e0 = U16(c, 0xE0);
		uint16_t dc = U16(c, 0xDC);
		MEM<uint32_t>(0xB8B7FC) = look_z;
		MEM<uint16_t>(0x1D8E038) = e0;
		MEM<uint32_t>(0xB8B7F4) = U32(L, 4); // quirk 0x73B2BC: dword store, pad word L+6 -> 0xB8B7F6
		MEM<uint16_t>(0x1D977A2) = dc;
		return 0; // void
	}

	// 0x73B2D0 (engine): state helper - one keyframe camera step (0x73AE10); ++state when done
	uint32_t __cdecl a_73B2D0(uint32_t a1)
	{
		uint32_t r = a_73AE10();
		if (r != 1)
			return r;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B350 (engine): state helper - keyframe camera step, then once the director reached
	// step 4 (dir+0x42 >= 4) starts the camera key G_1533D18 from the director base (mode 0), ++state
	uint32_t __cdecl a_73B350(uint32_t a1)
	{
		a_73AE10();
		uint32_t r = a_73AAE0(4);
		if (r == 0)
			return 0;
		uint32_t dir = e2_dir();
		a_73AB20(G(G_1533D18), dir, 0);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B3B0 (engine): starts the channel-script camera: a1 = 4 key-list pointers -> channels
	// cam+0x00/0x14/0x28/0x3C, resets the script clock cam+0xE6, reads each channel's first key
	// (0x73B430) with current values look-at Y (5E), distance (DE), yaw (D8), pitch (DA)
	uint32_t __cdecl a_73B3B0(uint32_t a1)
	{
		uint32_t c = e2_cam();
		U16(c, 0xE6) = 0;
		U32(c, 0) = U32(a1, 0);
		U32(c, 0x14) = U32(a1, 4);
		uint32_t k2 = U32(a1, 8);
		uint32_t k3 = U32(a1, 0xC);
		U32(c, 0x28) = k2;
		U32(c, 0x3C) = k3;
		a_73B430(c, c + 0x5E);
		c = e2_cam();
		a_73B430(c + 0x14, c + 0xDE);
		c = e2_cam();
		a_73B430(c + 0x28, c + 0xD8);
		c = e2_cam();
		return a_73B430(c + 0x3C, c + 0xDA);
	}

	// 0x73B430 (engine): camera channel - reads the key at *(*ch): op -> ch+8; op 1 = lerp
	// {from (0x7FFE = current value *a2) -> ch+0xA, to -> ch+0xC}, op 2 = wait {time -> ch+0x10};
	// ch+4 = cursor after the key (op 0 values / op 1 t curve). Returns the cursor.
	uint32_t __cdecl a_73B430(uint32_t a1, uint32_t a2)
	{
		uint32_t p = U32(U32(a1, 0), 0);
		uint16_t op = U16(p, 0);
		p += 2;
		U16(a1, 8) = op;
		int32_t sop = (int16_t)op;
		if (sop == 1)
		{
			uint16_t from = U16(p, 0);
			if (from == 0x7FFE)
				from = U16(a2, 0);
			p += 2;
			U16(a1, 0xA) = from;
			U16(a1, 0xC) = U16(p, 0);
			p += 2;
		}
		else if (sop == 2)
		{
			U16(a1, 0x10) = U16(p, 0);
			p += 2;
			U32(a1, 4) = p;
			return p;
		}
		U32(a1, 4) = p;
		return p;
	}

	// 0x73B4B0 (engine): channel-script camera tick - steps the 4 channels (0x73B520) into
	// look-at Y / distance / yaw / pitch, ++script clock cam+0xE6, writes the battle camera (0x73B160)
	uint32_t __cdecl a_73B4B0(void)
	{
		g_cam_tick = g_real_tick; // held-frame camera: this stepper ran on this tick
		g_cam_kind = 2;
		uint32_t c = e2_cam();
		a_73B520(c, c + 0x5E);
		c = e2_cam();
		a_73B520(c + 0x14, c + 0xDE);
		c = e2_cam();
		a_73B520(c + 0x28, c + 0xD8);
		c = e2_cam();
		a_73B520(c + 0x3C, c + 0xDA);
		c = e2_cam();
		U16(c, 0xE6) = (uint16_t)(U16(c, 0xE6) + 1);
		return a_73B160(); // jmp 0x73B160 (tail call)
	}

	// 0x73B520 (engine): camera channel step - list end (*(*ch) == 0) -> 1; runs the key
	// (0x73B570); when the key is over moves to the next key pointer and reads it (0x73B430);
	// returns 1 at the list end, else 0
	uint32_t __cdecl a_73B520(uint32_t a1, uint32_t a2)
	{
		if (U32(U32(a1, 0), 0) == 0)
			return 1;
		uint32_t r = a_73B570(a1, a2);
		if (r == 1)
		{
			uint32_t np = U32(a1, 0) + 4;
			U32(a1, 0) = np;
			if (U32(np, 0) == 0)
				return 1;
			a_73B430(a1, a2);
		}
		return 0;
	}

	// 0x73B570 (engine): camera channel key - op 0: *a2 = next raw value (0x7FFF = end -> 1);
	// op 1: *a2 = from + (to - from) * t >> 12 with t from the curve (0x7FFF = end -> 1);
	// op 2: 1 when the script clock cam+0xE6 reached ch+0x10; the value is kept in ch+0xE
	uint32_t __cdecl a_73B570(uint32_t a1, uint32_t a2)
	{
		int32_t op = S16(a1, 8);
		if (op == 0)
		{
			uint32_t p = U32(a1, 4);
			uint16_t v = U16(p, 0);
			if (v == 0x7FFF)
				return 1;
			U16(a1, 0xE) = v;
			U32(a1, 4) = p + 2;
			U16(a2, 0) = v;
			return 0;
		}
		if (op == 1)
		{
			uint32_t p = U32(a1, 4);
			uint16_t t = U16(p, 0);
			if (t == 0x7FFF)
				return 1;
			int16_t from = S16(a1, 0xA);
			U32(a1, 4) = p + 2;
			int32_t r = e2_mulq12((int32_t)S16(a1, 0xC), (int32_t)from, (int32_t)(int16_t)t);
			uint16_t v = (uint16_t)add32(r, (int32_t)from);
			U16(a1, 0xE) = v;
			U16(a2, 0) = v;
			return 0;
		}
		if (op == 2)
		{
			uint32_t c = e2_cam();
			if (S16(c, 0xE6) < S16(a1, 0x10))
				return 0;
			return 1;
		}
		return 0;
	}

	// 0x73B620 (engine): state helper - director gate 5 (0x73B640); ++state when passed
	uint32_t __cdecl a_73B620(uint32_t a1)
	{
		uint32_t r = a_73B640(5);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B640 (engine): director gate - when dir+0x44 == dir+0x40 == a1 - 1, sets dir+0x44 = a1
	// and returns 1 (this effect reached sync point a1), else 0
	uint32_t __cdecl a_73B640(uint32_t a1)
	{
		uint32_t dir = e2_dir();
		uint16_t cur = U16(dir, 0x44);
		if (cur != U16(dir, 0x40))
			return 0;
		int16_t want = (int16_t)a1;
		if ((int32_t)(int16_t)cur != (int32_t)want - 1)
			return 0;
		U16(dir, 0x44) = (uint16_t)want;
		return 1;
	}

	// 0x73B750 (engine): end state - calls the empty 0x73B770, finishes the node, ++state
	uint32_t __cdecl a_73B750(uint32_t a1)
	{
		a_73A6D0(); // 0x73B770 = empty function (same code as nullsub 0x73A6D0)
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B790 (engine): state helper - director gate 1 (0x73B640); ++state when passed
	uint32_t __cdecl a_73B790(uint32_t a1)
	{
		uint32_t r = a_73B640(1);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B7E0 (engine): director test - (s16)dir+0x40 >= (s16)a1
	uint32_t __cdecl a_73B7E0(uint32_t a1)
	{
		uint32_t dir = e2_dir();
		return S16(dir, 0x40) >= (int16_t)a1 ? 1u : 0u;
	}

	// 0x73B8A0 (engine): state helper - director gate 2 (0x73B640); ++state when passed
	uint32_t __cdecl a_73B8A0(uint32_t a1)
	{
		uint32_t r = a_73B640(2);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B8C0 (engine): state helper - waits until dir+0x40 >= 2 (0x73B7E0), then ++state
	uint32_t __cdecl a_73B8C0(uint32_t a1)
	{
		uint32_t r = a_73B7E0(2);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73B9F0 (engine): hides the monsters: for battle entities 3..6 with flag bit 1, saves
	// their flag word to dir+0x2C[i] and sets flag bit 2
	uint32_t __cdecl a_73B9F0(void)
	{
		uint32_t save = e2_dir() + 0x2C;
		for (uint32_t e = 0x1D97494; e < 0x1D97704; e += 0x9C, save += 4)
		{
			uint16_t f = U16(e, 0);
			if (f & 2)
			{
				U32(save, 0) = (uint32_t)f;
				U16(e, 0) = (uint16_t)(f | 4);
			}
		}
		return 0; // void
	}

	// 0x73BA90 (engine): restores flag bit 2 of battle entities 3..6 (flag bit 1 set) from the
	// words saved by 0x73B9F0
	uint32_t __cdecl a_73BA90(void)
	{
		uint32_t save = e2_dir() + 0x2C;
		for (uint32_t e = 0x1D97494; e < 0x1D97704; e += 0x9C, save += 4)
		{
			uint16_t f = U16(e, 0);
			if (f & 2)
			{
				uint32_t v = ((uint32_t)(uint8_t)(U8(save, 0) ^ (uint8_t)f) & 4) ^ (uint32_t)f;
				U16(e, 0) = (uint16_t)v;
			}
		}
		return 0; // void
	}

	// 0x73BB60 (engine): pause switch - a1 == 0: when G_258FB70 == 1 clears it and G_2585E38;
	// a1 != 0: when G_258FB70 == 0 sets both to 1 (G_2585E38 = the prim players' pause flag);
	// calls the empty engine hook nullsub_1029 on a change
	uint32_t __cdecl a_73BB60(uint32_t a1)
	{
		uint32_t cur = MEM<uint32_t>(G(G_258FB70));
		if (a1 == 0)
		{
			if (cur != 1)
				return 0;
			MEM<uint32_t>(G(G_258FB70)) = 0;
			MEM<uint32_t>(G(G_2585E38)) = 0;
			x::nullsub_1029(); // (the original pushes a dummy 0 argument)
			return 0; // void
		}
		if (cur != 0)
			return 0;
		MEM<uint32_t>(G(G_258FB70)) = 1;
		MEM<uint32_t>(G(G_2585E38)) = 1;
		x::nullsub_1029(); // (the original pushes a dummy 1 argument)
		return 0; // void
	}

	// 0x73BBD0 (engine): state helper - director gate 7 (0x73B640); ++state when passed
	uint32_t __cdecl a_73BBD0(uint32_t a1)
	{
		uint32_t r = a_73B640(7);
		if (r == 0)
			return 0;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BC40 (engine): state helper - counts down the node's timer +0x44, ++state at <= 0
	uint32_t __cdecl a_73BC40(uint32_t a1)
	{
		U16(a1, 0x44) = (uint16_t)(U16(a1, 0x44) - 1);
		if (S16(a1, 0x44) <= 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73BCB0 (engine): state helper - director gate 9 (0x73B640); when passed finishes the
	// node and ++state
	uint32_t __cdecl a_73BCB0(uint32_t a1)
	{
		uint32_t r = a_73B640(9);
		if (r == 0)
			return 0;
		U8(a1, 0x26) |= 1;
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x73C100 (engine): spawns task a2 (node 0x2A4) into queue G_258BA90 as a child of a1, with
	// +0x30 = a3, +0x298 = a4, +0x29C = a6, +0x29E = a5 (words); returns the node
	uint32_t __cdecl a_73C100(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6)
	{
		uint32_t n = x::Effect_AddTaskAndInitFromCtx(G(G_258BA90), a2, 0x2A4, a1);
		U32(n, 0x30) = a3;
		U16(n, 0x298) = (uint16_t)a4;
		U16(n, 0x29C) = (uint16_t)a6;
		U16(n, 0x29E) = (uint16_t)a5;
		return n;
	}

	// 0x73C280 (engine): draws the node's prim model: matrix = camera * (rotations +0x62/+0x60/
	// +0x64, scale +0x50, translation +0x1C..0x20), plays the prim player at +0x94 with callback
	// 0x73C3B0 and a parameter block (matrix + node colours/params), paused by G_2585E38
	uint32_t __cdecl a_73C280(uint32_t a1)
	{
		// stack frame 0x5C: [0..0x1F] matrix, [0x38] a1+0x70, [0x44] a1+0x7C, [0x48] &G_258EF98,
		// [0x4C..0x59] words 8C 8E 90 92 86 80 88 (0x20..0x37, 0x3C..0x43, 0x5A are not written;
		// the callback 0x73C3B0 does not read them)
		alignas(4) uint8_t fr[0x5C] = {};
		uint32_t L = P(fr);
		x::MAG_022_sub_8DD770(L);
		uint16_t ax = U16(a1, 0x62);
		if (ax != 0)
			x::MAG_022_sub_8DD8A0(L, (uint32_t)(int32_t)(int16_t)ax);
		ax = U16(a1, 0x60);
		if (ax != 0)
			x::sub_8DD7E0(L, (uint32_t)(int32_t)(int16_t)ax);
		ax = U16(a1, 0x64);
		if (ax != 0)
			x::sub_8DD960(L, (uint32_t)(int32_t)(int16_t)ax);
		x::scale3DMatrix(L, a1 + 0x50);
		S32(L, 0x14) = S16(a1, 0x1C);
		S32(L, 0x18) = S16(a1, 0x1E);
		S32(L, 0x1C) = S16(a1, 0x20);
		x::ComposeAffineTransform(0x1D97778, L, L);
		U16(L, 0x54) = U16(a1, 0x86);
		U32(L, 0x44) = U32(a1, 0x7C);
		U16(L, 0x4C) = U16(a1, 0x8C);
		U16(L, 0x4E) = U16(a1, 0x8E);
		U32(L, 0x38) = U32(a1, 0x70);
		U16(L, 0x52) = U16(a1, 0x92);
		uint32_t paused = MEM<uint32_t>(G(G_2585E38));
		U16(L, 0x56) = U16(a1, 0x80);
		U16(L, 0x50) = U16(a1, 0x90);
		U32(L, 0x48) = G(G_258EF98);
		U16(L, 0x58) = U16(a1, 0x88);
		return prim_play(a1 + 0x94, F(F_73C3B0), L, paused);
	}

	// ====================================================================================
	// part e3
	// ====================================================================================
	// Actor state-machine GF family - part e3: Gouraud primitive emitters of the model prim lists
	// (triangles / quads through the software GTE), the 5-state actor task dispatcher, the actor
	// director set-up (state block node+0x34 = *G_258FB78: target positions, bounds, distance,
	// pointer relocation of the loaded actor data) and the per-actor motion step + sprite draw.
	// sign-extended 16-bit word as a 32-bit argument word
	static inline uint32_t e3_sx16(uint16_t v) { return (uint32_t)(int32_t)(int16_t)v; }
	// x/y screen range tests of the primitive emitters (signed 16-bit compares)
	static inline bool e3_out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool e3_out_y(int16_t v) { return v < 0 || v > 0x6C0; }

	// 0x73D770 (engine; Siren sub_73D770, Tonberry copy): prim-list block "Gouraud triangles":
	// count at *(ctx+0x2C), then per 0x14-byte record (dword code+rgb0, u16 vertex indices
	// +4/+6/+8, rgb1 +0x0C, rgb2 +0x10) projects the 3 vertices (RTPT), builds a 0x1C-byte POLY_G3
	// packet at the cursor (tag 0x06000000; ctx+0x14 flags: 2 = semi-transparent on, 8 = off,
	// 0x20 = no back-face cull, 0x80 = GTE-lit colours), rejects GTE-flagged / back-facing / fully
	// off-screen triangles and inserts the others in OT a2 at ((OTZ + ctx+0x10) clamped >= 0) >> a3.
	// Returns the new packet cursor; ctx+0x2C = end of the record list.
	uint32_t __cdecl a_73D770(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;                 // [esp+0x10]
		uint32_t list = U32(ctx, 0x2C);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;              // edi
		const uint32_t vbase = U32(ctx, 4);   // (the original stores it into its a4 slot)
		U32(ctx, 0x2C) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return a4;
		}
		uint32_t pkt = cursor;                // esi = pkt + 0x10 (advances with the cursor)
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x14);
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
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (!(U32(ctx, 0x3C) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;            // [esp+0x18]
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) >= 0 || (U8(ctx, 0x14) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_AVSZ3();
					if (e3_out_x(S16(pkt, 8))) clip = 1;
					if (e3_out_x(S16(pkt, 0x10))) clip |= 2;
					if (e3_out_x(S16(pkt, 0x18))) clip |= 4;
					if (e3_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (e3_out_y(S16(pkt, 0x12))) clip |= 0x20;
					if (e3_out_y(S16(pkt, 0x1A))) clip |= 0x40;
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						x::GTE_ReadOTZ(ctx + 0x38);
						if (U8(ctx, 0x14) & 0x80)
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
						int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
						S32(ctx, 0x38) = z;
						if (z < 0)
							S32(ctx, 0x38) = 0;
						z = S32(ctx, 0x38) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x1C;
						pkt += 0x1C;
					}
				}
			}
			rec += 0x14;
		} while (--count);
		U32(ctx, 0x2C) = rec;
		return cursor;
	}

	// 0x73D9C0 (engine; Siren sub_73D9C0, Tonberry copy): prim-list block "Gouraud quads": same as
	// 0x73D770 for 0x18-byte records (4th vertex index +0x0A, rgb1..3 at +0x0C/+0x10/+0x14) ->
	// 0x24-byte POLY_G4 packets (tag 0x08000000), 4th vertex projected with RTPS, AVSZ4, off-screen
	// test on the 4 corners. Returns the new packet cursor.
	uint32_t __cdecl a_73D9C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t ctx = a1;
		uint32_t cursor = a4;                 // [esp+0x14]
		uint32_t list = U32(ctx, 0x2C);
		uint32_t count = U32(list, 0);
		uint32_t rec = list + 4;
		const uint32_t vbase = U32(ctx, 4);
		U32(ctx, 0x2C) = rec;
		if ((int32_t)count <= 0)
		{
			U32(ctx, 0x2C) = rec;
			return a4;
		}
		uint32_t pkt = cursor;
		do
		{
			x::GTE_LoadV012(vbase + (uint32_t)U16(rec, 4) * 4, vbase + (uint32_t)U16(rec, 6) * 4, vbase + (uint32_t)U16(rec, 8) * 4);
			x::GTE_RTPT();
			uint32_t flags = U32(ctx, 0x14);
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
			x::GTE_ReadFLAG(ctx + 0x3C);
			if (!(U32(ctx, 0x3C) & 0x60000))
			{
				x::GTE_NCLIP();
				uint32_t clip = 0;            // [esp+0x10]
				x::GTE_ReadMAC0(ctx + 0x30);
				if (S32(ctx, 0x30) >= 0 || (U8(ctx, 0x14) & 0x20))
				{
					x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					x::GTE_LoadV0(vbase + (uint32_t)U16(rec, 0x0A) * 4);
					x::GTE_RTPS();
					if (e3_out_x(S16(pkt, 8))) clip = 1;
					if (e3_out_x(S16(pkt, 0x10))) clip |= 2;
					if (e3_out_x(S16(pkt, 0x18))) clip |= 4;
					if (e3_out_y(S16(pkt, 0x0A))) clip |= 0x10;
					if (e3_out_y(S16(pkt, 0x12))) clip |= 0x20;
					if (e3_out_y(S16(pkt, 0x1A))) clip |= 0x40;
					x::GTE_ReadSXY2(pkt + 0x20);
					x::GTE_AVSZ4();
					if (e3_out_x(S16(pkt, 0x20))) clip |= 8;
					if (e3_out_y(S16(pkt, 0x22))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						x::GTE_ReadOTZ(ctx + 0x38);
						if (U8(ctx, 0x14) & 0x80)
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
						int32_t z = add32(S32(ctx, 0x38), S32(ctx, 0x10));
						S32(ctx, 0x38) = z;
						if (z < 0)
							S32(ctx, 0x38) = 0;
						z = S32(ctx, 0x38) >> (a3 & 31);
						x::SSIGPU_InsertPrimAutoDepth(a2 + (uint32_t)z * 4, cursor);
						cursor += 0x24;
						pkt += 0x24;
					}
				}
			}
			rec += 0x18;
		} while (--count);
		U32(ctx, 0x2C) = rec;
		return cursor;
	}

	// 0x73F110 (engine TASK; Siren sub_73F110, also 090/096/099): actor task dispatcher - runs
	// state s8 node+0x29 of the 5-entry table {73F180, 73F1D0, 73F1E0, 73F220, 73F240} (module
	// copies), ++frame counter +0x24, ends (releases the linked task, returns 2) when finished
	// (+0x26 bit0) and no child is alive (+0x28 == 0).
	uint32_t __cdecl a_73F110(uint32_t a1)
	{
		const uint32_t node = a1;
		// this task body exists several times per module with different state tables (Siren
		// 0x73F110 0x73F370 0x73F500, Chocometeor 0x71F310 0x71FFB0 0x7215A0): the table is read
		// from the running copy's own code (the five `mov [esp+x], imm32` at +0x0D..+0x31)
		const uint32_t fn = U32(node, 8);
		uint32_t states[5] = { U32(fn, 0x0D), U32(fn, 0x19), U32(fn, 0x21), U32(fn, 0x29), U32(fn, 0x31) };
		int32_t state = S8(node, 0x29);
		// the original indexes its 5-slot stack table without a bound check (states 0..4 only)
		callp(states[state], node);
		uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		if ((status & 1) && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// 0x73F990 (engine; Siren sub_73F990, all 6 actor modules): actor director set-up for node a1:
	// *G_258FB78 = state block node+0x34; copies node+0x29A/0x29C(mode)/0x29E into it, points its 4
	// tables (+0x224..+0x230) into the loaded actor data (node+0x30; relocated once by 0x740210,
	// flag data+0), fills the target slot list (+4.., count = root +0x5A) from the cast context's
	// action record, sets start / per-target / bounds positions (0x73FC20, 0x73FFB0; modes 1/3/4
	// extra set-ups) and stores the caster-to-first-target distance in state+0.
	uint32_t __cdecl a_73F990(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t data = U32(node, 0x30);        // edi: loaded actor data
		uint16_t w29a = U16(node, 0x29A);
		uint16_t w29c = U16(node, 0x29C);
		uint32_t st = node + 0x34;                    // eax: director state block
		const uint32_t root = U32(node, 0x10);        // ebx
		MEM<uint32_t>(G(G_258FB78)) = st;
		U16(st, 0x22) = w29a;
		uint16_t caster = (uint16_t)U8(node, 0x2C);   // bp (movzx)
		U16(st, 0x24) = w29c;                         // mode
		U32(st, 0x224) = data + 4;
		U32(st, 0x228) = data + 0x44;
		U32(st, 0x22C) = data + 0x84;
		uint32_t relocated = U32(data, 0);
		U32(st, 0x230) = data + 0xC4;
		if (relocated == 0)
		{
			a_740210(data);
			st = MEM<uint32_t>(G(G_258FB78));
			U32(data, 0) = 1;
		}
		uint16_t ntargets = U16(root, 0x5A);
		U16(st, 0x1E) = caster;
		U16(st, 0x1C) = ntargets;
		if ((int16_t)ntargets > 0)
		{
			int32_t i = 0;
			do
			{
				int32_t action = S8(node, 0x2A);
				uint32_t tab = U32(U32(node, 0x0C), 4);
				uint32_t rec = U32(tab + (uint32_t)mul32(action, 5) * 4, 8);
				U32(st, 4 + 4 * i) = U8(rec, 0x18 * i);   // target slot
				i++;
			} while (i < S16(st, 0x1C));
		}
		U16(st, 0x1A) = U16(node, 0x29E);
		a_73FC20();
		st = MEM<uint32_t>(G(G_258FB78));
		if (U16(st, 0x24) == 4)
			callp(F(F_73FB40), node);
		a_73FFB0();   // (the original pushes node; the callee takes no argument)
		st = MEM<uint32_t>(G(G_258FB78));
		if (U16(st, 0x24) == 1)
		{
			callp(F(F_73FDC0), node);   // 0-arg function, node pushed like the original
			st = MEM<uint32_t>(G(G_258FB78));
		}
		if (U16(st, 0x24) == 3)
		{
			a_73FE90(node);
			st = MEM<uint32_t>(G(G_258FB78));
		}
		// distance caster entity -> first target entity (s16 x/y/z at entity +0x1C/+0x1E/+0x20)
		int32_t s1 = S16(st, 0x1E);
		uint32_t e1 = 0x1D972C0 + (uint32_t)mul32(s1, 0x9C);
		uint32_t d10 = U32(e1, 0x1C);
		uint32_t d14 = U32(e1, 0x20);
		uint32_t s2 = U32(st, 4);
		uint32_t e2 = 0x1D972C0 + s2 * 0x9C;
		uint32_t d18 = U32(e2, 0x1C);
		int32_t dx = (int16_t)(uint16_t)((uint16_t)d10 - (uint16_t)d18);
		int32_t dy = (int16_t)(uint16_t)((uint16_t)(d10 >> 16) - (uint16_t)(d18 >> 16));
		int32_t dz = (int16_t)(uint16_t)((uint16_t)d14 - (uint16_t)U32(e2, 0x20));
		int32_t sq = add32(add32(mul32(dx, dx), mul32(dy, dy)), mul32(dz, dz));
		uint32_t dist = x::Sqrt((uint32_t)sq);
		st = MEM<uint32_t>(G(G_258FB78));
		U32(st, 0) = dist;
		return 0; // void
	}

	// 0x73FC20 (engine; Siren sub_73FC20): director start positions from the caster entity
	// (s16 state+0x1E): spawn point 0xF1 with y = entity y -> 16.16 at +0x1C4 (and +0x1D4 with
	// y = 0), point 0xF1 -> +0x1E4, point 0xF0 -> +0x1F4, +0x204 = start x / entity +0x3C / start z
	// (mode 2 = the fixed position *G_1533010 instead of the spawn points).
	uint32_t __cdecl a_73FC20(void)
	{
		uint32_t st = MEM<uint32_t>(G(G_258FB78));
		int32_t slot = S16(st, 0x1E);
		const uint32_t ent = 0x1D972C0 + (uint32_t)mul32(slot, 0x9C);
		alignas(4) uint8_t pos[8] = {};               // [esp+4] s16 x,y,z (+pad)
		const uint32_t pp = P(pos);
		if (U16(st, 0x24) == 2)
		{
			uint32_t fixed = MEM<uint32_t>(G(G_1533010));
			U32(pp, 0) = U32(fixed, 0);
			U32(pp, 4) = U32(fixed, 4);
		}
		else
		{
			x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
			st = MEM<uint32_t>(G(G_258FB78));
		}
		U16(pp, 2) = U16(ent, 0x24);
		S32(st, 0x1C4) = shl32(S16(pp, 0), 16);
		S32(st, 0x1C8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1CC) = shl32(S16(pp, 4), 16);
		U32(st, 0x1D4) = U32(st, 0x1C4);
		U32(st, 0x1D8) = 0;
		U32(st, 0x1DC) = U32(st, 0x1CC);
		if (U16(st, 0x24) == 2)
		{
			uint32_t fixed = MEM<uint32_t>(G(G_1533010));
			U32(pp, 0) = U32(fixed, 0);
			U32(pp, 4) = U32(fixed, 4);
		}
		else
		{
			x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
			st = MEM<uint32_t>(G(G_258FB78));
		}
		S32(st, 0x1E4) = shl32(S16(pp, 0), 16);
		S32(st, 0x1E8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1EC) = shl32(S16(pp, 4), 16);
		if (U16(st, 0x24) == 2)
		{
			uint32_t fixed = MEM<uint32_t>(G(G_1533010));
			U32(pp, 0) = U32(fixed, 0);
			U32(pp, 4) = U32(fixed, 4);
		}
		else
		{
			x::GetEffectSpawnPosition(ent, 0xF0, 0, pp);
			st = MEM<uint32_t>(G(G_258FB78));
		}
		S32(st, 0x1F4) = shl32(S16(pp, 0), 16);
		S32(st, 0x1F8) = shl32(S16(pp, 2), 16);
		S32(st, 0x1FC) = shl32(S16(pp, 4), 16);
		uint32_t sx = U32(st, 0x1C4);
		int32_t h = shl32(S16(ent, 0x3C), 16);
		U32(st, 0x204) = sx;
		S32(st, 0x208) = h;
		U32(st, 0x20C) = U32(st, 0x1CC);
		return 0; // void
	}

	// 0x73FE90 (engine; Siren sub_73FE90): mode-3 set-up: position node a1 +0x288/+0x28A/+0x28C
	// (s16 -> 16.16) written to state+0x1A4 and copied (16 bytes) into the 5 per-target position
	// arrays (+0x154/+0x114/+0xD4/+0x94/+0x54, stride 0x10) of every target (count state+0x1C).
	uint32_t __cdecl a_73FE90(uint32_t a1)
	{
		const uint32_t st = MEM<uint32_t>(G(G_258FB78));   // edx
		if (S16(st, 0x1C) <= 0)
			return 0; // void
		int32_t px = shl32(S16(a1, 0x288), 16);
		int32_t pz = shl32(S16(a1, 0x28C), 16);
		int32_t py = shl32(S16(a1, 0x28A), 16);
		// (the original keeps py in its own argument slot [esp+0x1C], overwriting a1 there)
		const uint32_t src = st + 0x1A4;                  // ecx
		uint32_t base = st + 0x114;                       // eax
		int32_t i = 0;                                    // [esp+0x10]
		static const int32_t dst_off[5] = { 0x40, 0, -0x40, -0x80, -0xC0 };
		do
		{
			S32(src, 0) = px;
			S32(st, 0x1A8) = py;
			S32(st, 0x1AC) = pz;
			for (int k = 0; k < 5; k++)
			{
				uint32_t dst = base + dst_off[k];
				U32(dst, 0) = U32(src, 0);
				U32(dst, 4) = U32(src, 4);
				U32(dst, 8) = U32(src, 8);
				U32(dst, 0xC) = U32(src, 0xC);
			}
			base += 0x10;
			i++;
		} while (i < S16(st, 0x1C));
		return 0; // void
	}

	// 0x73FFB0 (engine; Siren sub_73FFB0): per-target positions: for every target slot
	// (state+4.., count +0x1C): spawn point 0xF1 with y = entity y -> 16.16 at +0x54 and +0x94
	// (y = 0), point 0xF1 -> +0xD4, point 0xF0 -> +0x114, +0x154 = x / entity +0x3C / z (mode 2:
	// the fixed position *G_1533010 +8 instead); then the x/z centre of the +0x54 positions ->
	// +0x194/+0x198(=0)/+0x19C.
	uint32_t __cdecl a_73FFB0(void)
	{
		uint32_t st = MEM<uint32_t>(G(G_258FB78));        // ecx
		if (S16(st, 0x1C) > 0)
		{
			uint32_t fixed = MEM<uint32_t>(G(G_1533010));  // edx
			uint32_t off = 0;                             // esi
			uint32_t sofs = 4;                            // ebx
			int32_t i = 0;                                // ebp
			alignas(4) uint8_t pos[8] = {};               // [esp+0x18]
			const uint32_t pp = P(pos);
			do
			{
				uint32_t slot = U32(sofs + st, 0);
				const uint32_t ent = 0x1D972C0 + slot * 0x9C;   // edi
				if (U16(st, 0x24) == 2)
				{
					U32(pp, 0) = U32(fixed, 8);
					U32(pp, 4) = U32(fixed, 0xC);
				}
				else
				{
					x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
					st = MEM<uint32_t>(G(G_258FB78));
					fixed = MEM<uint32_t>(G(G_1533010));
				}
				U16(pp, 2) = U16(ent, 0x24);
				uint32_t e = off + st;
				S32(e, 0x54) = shl32(S16(pp, 0), 16);
				S32(e, 0x58) = shl32(S16(pp, 2), 16);
				S32(e, 0x5C) = shl32(S16(pp, 4), 16);
				U32(e, 0x94) = U32(e, 0x54);
				U32(e, 0x98) = 0;
				U32(e, 0x9C) = U32(e, 0x5C);
				if (U16(st, 0x24) == 2)
				{
					U32(pp, 0) = U32(fixed, 8);
					U32(pp, 4) = U32(fixed, 0xC);
				}
				else
				{
					x::GetEffectSpawnPosition(ent, 0xF1, 0, pp);
					st = MEM<uint32_t>(G(G_258FB78));
					fixed = MEM<uint32_t>(G(G_1533010));
				}
				e = off + st;
				S32(e, 0xD4) = shl32(S16(pp, 0), 16);
				S32(e, 0xD8) = shl32(S16(pp, 2), 16);
				S32(e, 0xDC) = shl32(S16(pp, 4), 16);
				if (U16(st, 0x24) == 2)
				{
					U32(pp, 0) = U32(fixed, 8);
					U32(pp, 4) = U32(fixed, 0xC);
				}
				else
				{
					x::GetEffectSpawnPosition(ent, 0xF0, 0, pp);
					st = MEM<uint32_t>(G(G_258FB78));
					fixed = MEM<uint32_t>(G(G_1533010));
				}
				e = off + st;
				S32(e, 0x114) = shl32(S16(pp, 0), 16);
				sofs += 4;
				S32(e, 0x118) = shl32(S16(pp, 2), 16);
				S32(e, 0x11C) = shl32(S16(pp, 4), 16);
				U32(e, 0x154) = U32(e, 0x54);
				S32(e, 0x158) = shl32(S16(ent, 0x3C), 16);
				U32(e, 0x15C) = U32(e, 0x5C);
				off += 0x10;
				i++;
			} while (i < S16(st, 0x1C));
		}
		// bounds of the +0x54 positions (x, z) over the targets
		int32_t n = S16(st, 0x1C);
		int32_t min_x = 0, max_x = 0, min_z = 0, max_z = 0;   // [esp+0x10], edi, ebx, esi
		if (n > 0)
		{
			uint32_t p = st + 0x5C;
			uint32_t left = (uint32_t)n;
			uint32_t idx = 0;
			do
			{
				if ((uint16_t)idx == 0)   // test bp, bp
				{
					max_x = S32(p, -8);
					max_z = S32(p, 0);
					min_x = max_x;
					min_z = max_z;
				}
				else
				{
					int32_t vx = S32(p, -8);
					if (vx < min_x)
						min_x = vx;
					else if (vx > max_x)
						max_x = vx;
					int32_t vz = S32(p, 0);
					if (vz < min_z)
						min_z = vz;
					else if (vz > max_z)
						max_z = vz;
				}
				idx++;
				p += 0x10;
			} while (--left);
		}
		S32(st, 0x194) = add32(max_x, min_x) / 2;
		S32(st, 0x198) = 0;
		S32(st, 0x19C) = add32(max_z, min_z) / 2;
		return 0; // void
	}

	// 0x740210 (engine; Siren sub_740210): relocates the loaded actor data by base a1: the 4
	// tables of 16 pointers (state +0x224/+0x228/+0x22C/+0x230) get +a1 on non-null entries, and
	// every non-null entry of the first table gets +a1 on its 32 dwords +0xC8..+0x144.
	uint32_t __cdecl a_740210(uint32_t a1)
	{
		const uint32_t base = a1;
		const uint32_t st = MEM<uint32_t>(G(G_258FB78));
		const uint32_t tab0 = U32(st, 0x224);
		for (int t = 0; t < 4; t++)
		{
			uint32_t tab = U32(st, 0x224 + 4 * t);
			for (int k = 0; k < 16; k++)
			{
				uint32_t v = U32(tab, 4 * k);
				if (v != 0)
					U32(tab, 4 * k) = v + base;
			}
		}
		for (int k = 0; k < 16; k++)
		{
			uint32_t e = U32(tab0, 4 * k);
			if (e == 0)
				continue;
			for (int j = 0; j < 32; j++)
				U32(e, 0xC8 + 4 * j) += base;
		}
		return 0; // void
	}

	// 0x740700 (engine; Siren sub_740700): draws the sprite sequences of the director's actor list
	// (state +0x2C, next +4): every actor of kind +8 == 1 whose bone entry kind (+0x16) is 0/1/2
	// and that has a sequence (+0x170): once (+0x1D8 == 1) or once per sub-position (s16 x/y/z at
	// +0x19C.. stride 8 -> +0xC0..+0xC8) for +0x1D8 > 1, through 0x7407C0.
	uint32_t __cdecl a_740700(void)
	{
		uint32_t act = U32(MEM<uint32_t>(G(G_258FB78)), 0x2C);
		if (act == 0)
			return 0; // void
		do
		{
			if (U16(act, 8) == 1)
			{
				uint32_t st = MEM<uint32_t>(G(G_258FB78));
				int32_t b = S8(act, 0x1D6);
				uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // ebx
				uint8_t kind = U8(bone, 0x16);
				if ((kind == 0 || kind == 1 || kind == 2) && U32(act, 0x170) != 0)
				{
					int8_t n = S8(act, 0x1D8);
					if (n == 1)
					{
						a_7407C0(act);   // (the original also pushes the bone entry, unused)
					}
					else if (n > 0)
					{
						int32_t i = 0;
						uint32_t sub = act + 0x19E;   // edi
						do
						{
							S32(act, 0xC0) = S16(sub, -2);
							S32(act, 0xC4) = S16(sub, 0);
							S32(act, 0xC8) = S16(sub, 2);
							a_7407C0(act);
							i++;
							sub += 8;
						} while (i < S8(act, 0x1D8));
					}
				}
			}
			act = U32(act, 4);
		} while (act != 0);
		return 0; // void
	}

	// 0x7407C0 (engine; Siren sub_7407C0): draws actor a1's sprite sequence: loads its matrix
	// (+0xAC) into the GTE and plays sequence +0x170 (s8 +0x1D0 -> header +4) through
	// InitEffectSequenceFromData_c19 into the effect OT (+0x44), on a 0xB4-byte header taken from
	// the module scratch stack *G_258FB68; advances the packet cursor 0x1D8E054.
	uint32_t __cdecl a_7407C0(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t hdr = MEM<uint32_t>(G(G_258FB68)) - 0xB4;
		MEM<uint32_t>(G(G_258FB68)) = hdr;
		const uint32_t mtx = act + 0xAC;
		x::GTE_SetRotMatrix(mtx);
		x::GTE_SetTransVector(mtx);
		uint32_t seq = U32(act, 0x170);
		uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		int16_t variant = (int16_t)S8(act, 0x1D0);
		U32(hdr, 0) = seq;
		uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		S16(hdr, 4) = variant;
		U16(hdr, 0x24) = 0;
		uint32_t r = x::InitEffectSequenceFromData_c19(hdr, ot, 2, cursor);
		MEM<uint32_t>(0x1D8E054) = r;
		MEM<uint32_t>(G(G_258FB68)) = MEM<uint32_t>(G(G_258FB68)) + 0xB4;
		return 0; // void
	}

	// 0x740880 (engine; Siren sub_740880): one update step of actor a1 (phase s16 +0x1CC):
	// phase 0 = 0x741050, then if 0x7419C0 reports the end -> phase 1 and sequence +0x170 = 0,
	// else motion step 0x740910 + 0x741120 and ++frame +0x1CA; phase 1 = 0x742CD0, clears +0x1D7
	// and decrements the director's live-actor count (state +0x16).
	uint32_t __cdecl a_740880(uint32_t a1)
	{
		const uint32_t act = a1;
		uint32_t st = MEM<uint32_t>(G(G_258FB78));
		int32_t b = S8(act, 0x1D6);
		const uint32_t bone = U32(U32(st, 0x224) + (uint32_t)b * 4, 0);   // edi
		int32_t phase = S16(act, 0x1CC);
		if (phase == 0)
		{
			a_741050(act, bone);
			if (a_7419C0(act, bone) == 0)
			{
				a_740910(act, bone);
				a_741120(act, bone);
				U16(act, 0x1CA) = (uint16_t)(U16(act, 0x1CA) + 1);
				return 0; // void
			}
			U16(act, 0x1CC) = (uint16_t)(U16(act, 0x1CC) + 1);
			U32(act, 0x170) = 0;
			return 0; // void
		}
		if (phase == 1)
		{
			a_742CD0(act);
			st = MEM<uint32_t>(G(G_258FB78));
			U8(act, 0x1D7) = 0;
			U16(st, 0x16) = (uint16_t)(U16(st, 0x16) - 1);
		}
		return 0; // void
	}

	// 0x740910 (engine; Siren sub_740910): motion step of actor a1 with bone/animation entry a2:
	// angles +0xCC/+0xCE/+0xD0 = base (+0x194/+0x198) + per-frame tables (a2 +0xD8/+0xDC/+0xE0,
	// frame s16 +0x1CA) & 0xFFF; saves position +0xDC -> +0x11C; per sub-position (count s8 +0x1D8):
	// a2+0x27 = 1 -> offsets +0x12C from the frame tables a2 +0x13C/+0x140/+0x144, 0 -> integration
	// (speed +0x174 += +0x184, velocity +0x220 += +0x260, offset +0x1E0 += velocity, gravity +0x1DC,
	// hooks 0x740F70 / 0x740FC0 when +0x1DA, speed pushed along the matrix +0x0C) of offsets +0x12C;
	// then (a2+0x26 == 1) the offsets are rotated by a matrix chosen by a2+0x1D (identity or model
	// matrix +0x1BC+0x2C, or unpacked *G_257F9B0, then the 3 angles in two orders); a2+0x22: 1 =
	// positions +0xDC = model +0x4C + offset, 0 = offset; a2+0x1B = 1 -> position y = 0.
	uint32_t __cdecl a_740910(uint32_t a1, uint32_t a2)
	{
		const uint32_t act = a1;    // ebp
		const uint32_t ent = a2;    // [esp+0x90]
		// stack locals (0x78 bytes from [esp+0x10]): [esp+0x18] vec3, [esp+0x28] 3x3+T matrix,
		// [esp+0x48] 16-byte offsets. The offset array is sized for 127 entries here; the original
		// frame has room for 4 and would overrun for more (s8 +0x1D8 is 1..4 in the game data).
		alignas(4) uint8_t loc[0x38 + 127 * 0x10] = {};
		const uint32_t vec = P(loc) + 0x08;
		const uint32_t mat = P(loc) + 0x18;
		const uint32_t out = P(loc) + 0x38;

		uint32_t b194 = U32(act, 0x194);
		uint32_t b198 = U32(act, 0x198);
		U32(act, 0xCC) = b194;
		U32(act, 0xD0) = b198;
		int32_t fo = shl32(S16(act, 0x1CA), 1);
		uint16_t d;
		d = U16(U32(ent, 0xD8) + (uint32_t)fo, 0);
		U16(act, 0xCC) = (uint16_t)(U16(act, 0xCC) + d);
		d = U16(U32(ent, 0xDC) + (uint32_t)fo, 0);
		U16(act, 0xCE) = (uint16_t)(U16(act, 0xCE) + d);
		d = U16(U32(ent, 0xE0) + (uint32_t)fo, 0);
		U16(act, 0xD0) = (uint16_t)(U16(act, 0xD0) + d);
		U16(act, 0xCC) &= 0xFFF;
		U16(act, 0xCE) &= 0xFFF;
		U16(act, 0xD0) &= 0xFFF;
		for (int k = 0; k < 4; k++)
			U32(act, 0x11C + 4 * k) = U32(act, 0xDC + 4 * k);

		int32_t mode = S8(ent, 0x27);
		if (mode == 1)
		{
			if (S8(act, 0x1D8) > 0)
			{
				int32_t i = 0;
				uint32_t off = act + 0x12C;   // ebx
				do
				{
					S32(off, 0) = shl32(S16(U32(ent, 0x13C) + (uint32_t)S16(act, 0x1CA) * 2, 0), 16);
					S32(off, 4) = shl32(S16(U32(ent, 0x140) + (uint32_t)S16(act, 0x1CA) * 2, 0), 16);
					S32(off, 8) = shl32(S16(U32(ent, 0x144) + (uint32_t)S16(act, 0x1CA) * 2, 0), 16);
					uint32_t model = U32(act, 0x1BC);
					if (model != 0)
						for (int k = 0; k < 8; k++)
							U32(mat, 4 * k) = U32(model + 0x2C, 4 * k);
					// quirk 0x740A4E: the model matrix just copied (0x740A47) is overwritten by the
					// identity (only its pad word +0x12 survives): the offsets are "rotated" by I
					x::MAG_022_sub_8DD770(mat);
					x::TransformVectorBy3x3Matrix(mat, off, off);
					i++;
					off += 0x10;
				} while (i < S8(act, 0x1D8));
			}
		}
		else if (mode == 0)
		{
			if (S8(act, 0x1D8) > 0)
			{
				int32_t i = 0;                  // [esp+0x10]
				uint32_t rows = act + 0x0C;     // [esp+0x14] matrices, stride 0x20
				uint32_t v = act + 0x220;       // esi: velocity (+0x40 acceleration, -0x40 offset)
				uint32_t spd = act + 0x174;     // ebx: speed (+0x10 its acceleration), stride 4
				do
				{
					if (act + 0x184 != 0 || act + 0x174 != 0)   // compiler address test, always true
					{
						S32(spd, 0) = add32(S32(spd, 0), S32(spd, 0x10));
						uint16_t hook = U16(act, 0x1DA);
						if (hook != 0)
							a_740F70(e3_sx16(hook), spd);
						uint32_t s = U32(spd, 0);
						U32(vec, 0) = 0;
						U32(vec, 8) = 0;
						U32(vec, 4) = 0u - s;
						x::TransformVectorBy3x3Matrix(rows, vec, mat);
						S32(v, -0xF4) = add32(S32(v, -0xF4), S32(mat, 0));
						S32(v, -0xF0) = add32(S32(v, -0xF0), S32(mat, 4));
						S32(v, -0xEC) = add32(S32(v, -0xEC), S32(mat, 8));
					}
					S32(v, 0) = add32(S32(v, 0), S32(v, 0x40));
					S32(v, 4) = add32(S32(v, 4), S32(v, 0x44));
					S32(v, 8) = add32(S32(v, 8), S32(v, 0x48));
					S32(v, -0x40) = add32(S32(v, -0x40), S32(v, 0));
					S32(v, -0x3C) = add32(S32(v, -0x3C), S32(v, 4));
					S32(v, -0x38) = add32(S32(v, -0x38), S32(v, 8));
					S32(v, -0x3C) = add32(S32(v, -0x3C), S32(act, 0x1DC));
					const uint32_t pos = v - 0x40;   // edi
					uint16_t hook = U16(act, 0x1DA);
					if (hook != 0)
						a_740FC0(e3_sx16(hook), pos);
					S32(v, -0xF4) = add32(S32(v, -0xF4), S32(pos, 0));
					spd += 4;
					S32(v, -0xF0) = add32(S32(v, -0xF0), S32(v, -0x3C));
					S32(v, -0xEC) = add32(S32(v, -0xEC), S32(v, -0x38));
					v += 0x10;
					i++;
					rows += 0x20;
				} while (i < S8(act, 0x1D8));
			}
		}

		if (U8(ent, 0x26) == 1)
		{
			int32_t sel = S8(ent, 0x1D);
			uint32_t model = U32(act, 0x1BC);   // esi
			switch ((uint32_t)sel)   // jump table 0x740F5C
			{
			case 0:   // identity or model matrix, then Z(+0xD0), X(+0xCC), Y(+0xCE)
				x::MAG_022_sub_8DD770(mat);
				if (model != 0)
					for (int k = 0; k < 8; k++)
						U32(mat, 4 * k) = U32(model + 0x2C, 4 * k);
				if (U16(act, 0xD0) != 0) x::sub_8DD960(mat, e3_sx16(U16(act, 0xD0)));
				if (U16(act, 0xCC) != 0) x::sub_8DD7E0(mat, e3_sx16(U16(act, 0xCC)));
				if (U16(act, 0xCE) != 0) x::MAG_022_sub_8DD8A0(mat, e3_sx16(U16(act, 0xCE)));
				break;
			case 1:   // identity or model matrix, then Y, X, Z
				x::MAG_022_sub_8DD770(mat);
				if (model != 0)
					for (int k = 0; k < 8; k++)
						U32(mat, 4 * k) = U32(model + 0x2C, 4 * k);
				if (U16(act, 0xCE) != 0) x::MAG_022_sub_8DD8A0(mat, e3_sx16(U16(act, 0xCE)));
				if (U16(act, 0xCC) != 0) x::sub_8DD7E0(mat, e3_sx16(U16(act, 0xCC)));
				if (U16(act, 0xD0) != 0) x::sub_8DD960(mat, e3_sx16(U16(act, 0xD0)));
				break;
			case 2:   // unpacked *G_257F9B0 matrix, then Z, X, Y
				x::UnpackRotationMatrix(MEM<uint32_t>(G(G_257F9B0)), mat);
				if (U16(act, 0xD0) != 0) x::sub_8DD960(mat, e3_sx16(U16(act, 0xD0)));
				if (U16(act, 0xCC) != 0) x::sub_8DD7E0(mat, e3_sx16(U16(act, 0xCC)));
				if (U16(act, 0xCE) != 0) x::MAG_022_sub_8DD8A0(mat, e3_sx16(U16(act, 0xCE)));
				break;
			case 3:   // unpacked *G_257F9B0 matrix, then Y, X, Z
				x::UnpackRotationMatrix(MEM<uint32_t>(G(G_257F9B0)), mat);
				if (U16(act, 0xCE) != 0) x::MAG_022_sub_8DD8A0(mat, e3_sx16(U16(act, 0xCE)));
				if (U16(act, 0xCC) != 0) x::sub_8DD7E0(mat, e3_sx16(U16(act, 0xCC)));
				if (U16(act, 0xD0) != 0) x::sub_8DD960(mat, e3_sx16(U16(act, 0xD0)));
				break;
			default:
				// UNINIT 0x740C15: a selector > 3 leaves the local matrix as it is (garbage from an
				// earlier stack frame, or what this call left in it above); here: that, else zero
				break;
			}
			if (S8(act, 0x1D8) > 0)
			{
				int32_t i = 0;
				uint32_t o = out, src = act + 0x12C;
				do
				{
					x::TransformVectorBy3x3Matrix(mat, src, o);
					i++;
					o += 0x10;
					src += 0x10;
				} while (i < S8(act, 0x1D8));
			}
		}
		else
		{
			int32_t n = S8(act, 0x1D8);
			if (n > 0)
			{
				uint32_t dwords = ((uint32_t)n & 0xFFFFFFF) << 2;   // rep movsd
				for (uint32_t k = 0; k < dwords; k++)
					U32(out, 4 * k) = U32(act + 0x12C, 4 * k);
			}
		}

		int32_t place = S8(ent, 0x22);
		if (place == 1)
		{
			uint32_t model = U32(act, 0x1BC);
			if (model != 0 && S8(act, 0x1D8) > 0)
			{
				const uint32_t mpos = model + 0x4C;   // [esp+0x14]
				int32_t i = 0;
				uint32_t o = out, dst = act + 0xDC;
				do
				{
					U32(dst, 0) = U32(mpos, 0);
					U32(dst, 4) = U32(mpos, 4);
					U32(dst, 8) = U32(mpos, 8);
					U32(dst, 0xC) = U32(mpos, 0xC);
					S32(dst, 0) = add32(S32(dst, 0), S32(o, 0));
					S32(dst, 4) = add32(S32(dst, 4), S32(o, 4));
					S32(dst, 8) = add32(S32(dst, 8), S32(o, 8));
					i++;
					o += 0x10;
					dst += 0x10;
				} while (i < S8(act, 0x1D8));
			}
		}
		else if (place == 0)
		{
			if (S8(act, 0x1D8) > 0)
			{
				int32_t i = 0;
				uint32_t o = out, dst = act + 0xDC;
				do
				{
					i++;
					U32(dst, 0) = U32(o, 0);
					U32(dst, 4) = U32(o, 4);
					U32(dst, 8) = U32(o, 8);
					U32(dst, 0xC) = U32(o, 0xC);
					o += 0x10;
					dst += 0x10;
				} while (i < S8(act, 0x1D8));
			}
		}

		if (U8(ent, 0x1B) == 1 && S8(act, 0x1D8) > 0)
		{
			int32_t i = 0;
			uint32_t y = act + 0xE0;
			do
			{
				U32(y, 0) = 0;
				i++;
				y += 0x10;
			} while (i < S8(act, 0x1D8));
		}
		return 0; // void
	}

	// ====================================================================================
	// part e4
	// ====================================================================================
	// part e4: emitter-instance / particle helpers of the actor state-machine library
	// (damping, instance transform, 0x6C particle-slot pool + list, random ranges, particle anchors).
	namespace
	{
		// cdq / and edx,0xFF / add / sar 8 : truncating division by 256
		inline int32_t e4_div256(int32_t v) { return v / 256; }
		// cdq / and edx,0xFFFF / add / sar 16 : truncating division by 65536 (16.16 -> integer part)
		inline int32_t e4_div65536(int32_t v) { return v / 65536; }

		// shared body of 0x740F70 / 0x740FC0: *p -= ((*p/256) * t)/256, returns the decrement
		inline int32_t e4_decay(uint32_t p, int32_t t)
		{
			int32_t v = S32(p, 0);
			int32_t d = e4_div256(mul32(e4_div256(v), t));
			S32(p, 0) = sub32(v, d);
			return d;
		}

		// 0x741651-0x74171F / 0x74175F-0x74181A (identical sequences): 16.16 position (3 dwords at
		// src) -> integer dwords at +0xA0, rotate the instance matrix (+0x8C) by the camera matrix
		// (*G_257F9B0) column by column into +0xAC, then transform +0xA0 by the camera rotation +
		// translation into +0xC0 (MAC1..3, 3 dwords)
		void e4_world_to_view(uint32_t node, uint32_t src)
		{
			U32(node, 0xA0) = (uint32_t)e4_div65536(S32(src, 0));
			U32(node, 0xA4) = (uint32_t)e4_div65536(S32(src, 4));
			U32(node, 0xA8) = (uint32_t)e4_div65536(S32(src, 8));
			x::GTE_SetRotMatrix(U32(G(G_257F9B0), 0));
			x::GTE_LoadIRFromMatrixColumn(node + 0x8C);
			x::GTE_MVMVA_RotIR();
			x::GTE_StoreIRToMatrixColumn(node + 0xAC);
			x::GTE_LoadIRFromMatrixColumn(node + 0x8E);
			x::GTE_MVMVA_RotIR();
			x::GTE_StoreIRToMatrixColumn(node + 0xAE);
			x::GTE_LoadIRFromMatrixColumn(node + 0x90);
			x::GTE_MVMVA_RotIR();
			x::GTE_StoreIRToMatrixColumn(node + 0xB0);
			x::GTE_SetTransVector(U32(G(G_257F9B0), 0));
			x::GTE_LoadV0FromDwords(node + 0xA0);
			x::GTE_MVMVA_RotV0_Tr();
			x::GTE_ReadMAC123(node + 0xC0);
		}

		// rotation build of 0x741120 cases 3 and 4 (identical code behind the two jump tables
		// 0x741884 / 0x741894): base = parent matrix copy (+0x1BC node +0x2C) or identity, or the
		// camera matrix; then rotations +0xD0 (8DD960) / +0xCC (8DD7E0) / +0xCE (8DD8A0), skipped when 0
		void e4_build_rot(uint32_t node, int32_t sub, uint32_t parent)
		{
			uint32_t m = node + 0x8C;
			switch ((uint32_t)sub)
			{
				case 0: // 0x7411C8 / 0x7413E4: parent|identity, d0, cc, ce
					if (parent != 0)
						memcpy((void *)m, (void *)(parent + 0x2C), 32);
					else
						x::MAG_022_sub_8DD770(m);
					if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
					if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
					if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
					break;
				case 1: // 0x74123E / 0x74145A: parent|identity, ce, cc, d0
					if (parent != 0)
						memcpy((void *)m, (void *)(parent + 0x2C), 32);
					else
						x::MAG_022_sub_8DD770(m);
					if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
					if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
					if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
					break;
				case 2: // 0x7412AF / 0x7414CB: camera, d0, cc, ce
					x::UnpackRotationMatrix(U32(G(G_257F9B0), 0), m);
					if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
					if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
					if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
					break;
				case 3: // 0x741316 / 0x74152B: camera, ce, cc, d0
					x::UnpackRotationMatrix(U32(G(G_257F9B0), 0), m);
					if (U16(node, 0xCE) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0xCE));
					if (U16(node, 0xCC) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0xCC));
					if (U16(node, 0xD0) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
					break;
				default:
					break;
			}
		}

		// SVECTOR of the descriptor offset: integer parts of the 3 16.16 dwords at desc+0/4/8
		inline void e4_load_offset(uint8_t *v, uint32_t desc)
		{
			*(int16_t *)(v + 0) = (int16_t)e4_div65536(S32(desc, 0));
			*(int16_t *)(v + 2) = (int16_t)e4_div65536(S32(desc, 4));
			*(int16_t *)(v + 4) = (int16_t)e4_div65536(S32(desc, 8));
		}
	}

	// 0x740F70 (engine; Siren sub_740F70, in 095-100): clamps t (a1) to [0,0x10000] and damps the
	// dword *a2 by (*a2/256 * t)/256; returns the decrement
	uint32_t __cdecl a_740F70(uint32_t a1, uint32_t a2)
	{
		int32_t t = (int32_t)a1;
		if (t > 0x10000)
			t = 0x10000;
		else if (t < 0)
			t = 0;
		return (uint32_t)e4_decay(a2, t);
	}

	// 0x740FC0 (engine; Siren sub_740FC0, in 095-100): same damping as 0x740F70 on the 3-dword
	// vector a2 (x, y, z in order); returns the z decrement
	uint32_t __cdecl a_740FC0(uint32_t a1, uint32_t a2)
	{
		int32_t t = (int32_t)a1;
		if (t > 0x10000)
			t = 0x10000;
		else if (t < 0)
			t = 0;
		e4_decay(a2, t);
		e4_decay(a2 + 4, t);
		return (uint32_t)e4_decay(a2 + 8, t);
	}

	// 0x741050 (engine; Siren sub_741050, in 095-100): advances the texture-animation frame of
	// emitter instance a1 (+0x1D0 frame, +0x1D1 last frame, +0x1D2 done flag, +0x1D3 loop start,
	// +0x1D4 loop end, +0x1D5 loops left) by the mode byte of descriptor a2 (+0x16):
	// 0 = play once then set done, 1 = wrap forever, 2 = loop [1D3..1D4] 1D5 times, then play out
	uint32_t __cdecl a_741050(uint32_t a1, uint32_t a2)
	{
		int32_t mode = (int32_t)S8(a2, 0x16);
		if (mode == 0)
		{
			// 0x7410EA
			int8_t cnt = (int8_t)(U8(a1, 0x1D0) + 1);
			int8_t last = S8(a1, 0x1D1);
			U8(a1, 0x1D0) = (uint8_t)cnt;
			if (cnt > last)
			{
				U8(a1, 0x1D0) = 0;
				U8(a1, 0x1D2) = 1;
			}
			return a1;
		}
		if (mode == 1)
		{
			// 0x7410C6
			int8_t cnt = (int8_t)(U8(a1, 0x1D0) + 1);
			int8_t last = S8(a1, 0x1D1);
			U8(a1, 0x1D0) = (uint8_t)cnt;
			if (cnt > last)
				U8(a1, 0x1D0) = 0;
			return a1;
		}
		if (mode == 2)
		{
			// 0x74106D
			int8_t cnt = (int8_t)(U8(a1, 0x1D0) + 1);
			int8_t loop_end = S8(a1, 0x1D4);
			U8(a1, 0x1D0) = (uint8_t)cnt;
			if (cnt > loop_end)
			{
				int8_t left = S8(a1, 0x1D5);
				if (left > 0)
				{
					U8(a1, 0x1D5) = (uint8_t)(left - 1);
					U8(a1, 0x1D0) = U8(a1, 0x1D3);
				}
			}
			// 0x7410A7
			if (S8(a1, 0x1D0) > S8(a1, 0x1D1))
			{
				U8(a1, 0x1D0) = 0;
				U8(a1, 0x1D2) = 1;
			}
			return a1;
		}
		return (uint32_t)(mode - 2); // eax left by the dispatch (the caller ignores it)
	}

	// 0x741120 (engine; Siren sub_741120, in 095 096 098 099 100): builds the transform of emitter
	// instance a1 from descriptor a2: keyframed modes 4/5 (+0x16) pick the frame's texture entry
	// (+0x170, 5 also calls 0x7418B0) and colour/scale (+0x16C..E, +0x1CE, +0xD4..D8, applied by
	// scale3DMatrix); rotation matrix +0x8C by +0x1B (camera+roll / fixed X 0x400 / camera yaw /
	// parent-or-identity or camera with ordered rotations); then projects the position(s)
	// (+0xDC.. 16.16 stride 0x10, +0x1D8 = count) to view space: one point -> +0xC0..C8 (+0xC8 +=
	// depth bias a2+0x38), several -> packed s16 x,y,z list at +0x19C (stride 8)
	uint32_t __cdecl a_741120(uint32_t a1, uint32_t a2)
	{
		uint32_t node = a1;
		uint32_t desc = a2;
		uint8_t mode = U8(desc, 0x16);
		if (mode == 4 || mode == 5)
		{
			int32_t frame = (int32_t)S16(node, 0x1CA);
			uint32_t tex = U8(U32(desc, 0x12C) + frame, 0);
			uint32_t dir = U32(G(G_258FB78), 0);
			U32(node, 0x170) = U32(U32(dir, 0x22C) + tex * 4, 0); // texture entry of this frame
			if (mode == 5)
				callp(F(F_7418B0), node, desc);
		}

		// 0x74116F
		int32_t rot_mode = (int32_t)S8(desc, 0x1B);
		uint32_t m = node + 0x8C;
		switch ((uint32_t)rot_mode)
		{
			case 0: // 0x741183: camera rotation + roll (+0xD0)
				x::UnpackRotationMatrix(U32(G(G_257F9B0), 0), m);
				if (U16(node, 0xD0) != 0)
					x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0xD0));
				break;
			case 1: // 0x741377: identity rotated by 0x400 (8DD7E0)
				x::MAG_022_sub_8DD770(m);
				x::sub_8DD7E0(m, 0x400);
				break;
			case 2: // 0x741396: identity + camera yaw
			{
				uint32_t yaw = x::sub_8DD7B0(U32(G(G_257F9B0), 0));
				x::MAG_022_sub_8DD770(m);
				if ((uint16_t)yaw != 0)
					x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)(int16_t)yaw);
				break;
			}
			case 3: // 0x7411AE
			case 4: // 0x7413CA (same code, own jump table)
			{
				int32_t sub = (int32_t)S8(desc, 0x1D);
				uint32_t parent = U32(node, 0x1BC);
				e4_build_rot(node, sub, parent);
				break;
			}
			default:
				break;
		}

		// 0x74158B
		mode = U8(desc, 0x16);
		if (mode == 4 || mode == 5)
		{
			int32_t frame = (int32_t)S16(node, 0x1CA);
			U8(node, 0x16C) = U8(U32(desc, 0xF0) + frame, 0);
			U8(node, 0x16D) = U8(U32(desc, 0xF4) + frame, 0);
			U8(node, 0x16E) = U8(U32(desc, 0xF8) + frame, 0);
			U16(node, 0x1CE) = U16(U32(desc, 0xFC) + frame * 2, 0);
			U16(node, 0xD4) = U16(U32(desc, 0xE4) + frame * 2, 0); // scale x
			U16(node, 0xD6) = U16(U32(desc, 0xE8) + frame * 2, 0); // scale y
			uint16_t sz = U16(U32(desc, 0xEC) + frame * 2, 0);
			int32_t scale[4]; // [esp+0x10] (the 4th dword is never written by the original)
			scale[0] = (int32_t)S16(node, 0xD4);
			scale[1] = (int32_t)S16(node, 0xD6);
			U16(node, 0xD8) = sz;                                  // scale z
			scale[2] = (int32_t)(int16_t)sz;
			scale[3] = 0;
			x::scale3DMatrix(m, P(scale));
		}

		// 0x741643
		uint8_t count = U8(node, 0x1D8);
		if (count == 1)
		{
			e4_world_to_view(node, node + 0xDC);
			U32(node, 0xC8) = (uint32_t)add32(S32(node, 0xC8), (int32_t)S16(desc, 0x38));
			return 0; // void
		}
		// 0x74173D: point list
		if ((int8_t)count > 0)
		{
			int32_t i = 0;
			uint32_t src = node + 0xDC;   // esi - 4
			uint32_t dst = node + 0x19C;  // edi - 2
			do
			{
				e4_world_to_view(node, src);
				U16(dst, 0) = U16(node, 0xC0);
				U16(dst, 2) = U16(node, 0xC4);
				uint16_t z = (uint16_t)(U16(node, 0xC8) + U16(desc, 0x38));
				i++;
				src += 0x10;
				U16(dst, 4) = z;
				dst += 8;
			} while (i < (int32_t)S8(node, 0x1D8));
		}
		return 0; // void
	}

	// 0x7419C0 (engine; Siren sub_7419C0, in 095-100): end-of-life test of emitter instance a1 with
	// descriptor a2 (+0x1E: 0 = when the animation is done +0x1D2, 1 = when timer +0x1C8 runs out,
	// 2 = either); on end spawns the follow-up particle (0x741A40) and returns 1, else 0
	uint32_t __cdecl a_7419C0(uint32_t a1, uint32_t a2)
	{
		int32_t kind = (int32_t)S8(a2, 0x1E);
		if (kind == 2)
		{
			// 0x7419D3
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) < 0)
			{
				a_741A40(a1, a2);
				return 1;
			}
			// -> 0x741A21
		}
		else if (kind == 1)
		{
			// 0x7419F8
			U16(a1, 0x1C8) = (uint16_t)(U16(a1, 0x1C8) - 1);
			if (S16(a1, 0x1C8) >= 0)
				return 0;
			a_741A40(a1, a2);
			return 1;
		}
		else if (kind != 0)
		{
			return 0;
		}
		// 0x741A21
		if (U8(a1, 0x1D2) == 1)
		{
			a_741A40(a1, a2);
			return 1;
		}
		return 0;
	}

	// 0x741A40 (engine; Siren sub_741A40, in 095-100): if descriptor a2 chains a follow-up
	// (+0x30 == 1) allocates a particle owned by instance a1 with descriptor index a2+0x31 and
	// anchor a1+0x1D9 (0x741A70)
	uint32_t __cdecl a_741A40(uint32_t a1, uint32_t a2)
	{
		if (U8(a2, 0x30) == 1)
		{
			// quirk 0x741A4E/0x741A53: `movsx cx/dx, byte` then push ecx/edx: the high halves are a2's
			// high word (ecx) and the caller's edx (UNINIT, 0 here); 0x741A70 only reads the low bytes.
			uint32_t desc_index = (a2 & 0xFFFF0000u) | (uint16_t)(int16_t)S8(a2, 0x31);
			uint32_t anchor = (uint16_t)(int16_t)S8(a1, 0x1D9);
			return a_741A70(a1, desc_index, anchor);
		}
		return a1; // void (eax = a1 at the only call site, 0x7419C0)
	}

	// 0x741A70 (engine; Siren sub_741A70, in 095-100): allocates a free 0x6C particle slot from the
	// 39-entry pool *G_258EC54 (round robin from cursor G_2585AE0; +0x69 in use), zeroes it, sets
	// owner +0x5C = a1, descriptor index +0x6A = a2, anchor +0x6B = a3, counts it in the director
	// (dir+0x14) and appends it to the particle list with state 0 (0x742CA0); returns slot or 0
	uint32_t __cdecl a_741A70(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		uint32_t pool = U32(G(G_258EC54), 0);
		int32_t idx = (int32_t)S16(G(G_2585AE0), 0);
		uint32_t slot = 0;
		int32_t tries = 0;
		do
		{
			if (U8(pool + (uint32_t)(idx * 27) * 4, 0x69) == 0)
			{
				// 0x741AA2
				slot = pool + (uint32_t)(idx * 27) * 4;
				x::MAG_007_sub_8DCC00(slot, 0x6C);
				U8(slot, 0x6A) = (uint8_t)a2;
				uint32_t dir = U32(G(G_258FB78), 0);
				U8(slot, 0x69) = 1;
				U16(dir, 0x14) = (uint16_t)(U16(dir, 0x14) + 1);
				U32(slot, 0x5C) = a1;
				U8(slot, 0x6B) = (uint8_t)a3;
				a_742CA0(slot, 0);
				break;
			}
			idx++;
			if (idx >= 0x27)
				idx = 0;
			tries++;
		} while (tries < 0x28);
		// 0x741AE0
		idx++;
		if (idx < 0x27)
			U16(G(G_2585AE0), 0) = (uint16_t)idx;
		else
			U16(G(G_2585AE0), 0) = 0;
		return slot;
	}

	// 0x741B60 (engine; Siren sub_741B60, in 095-100): particle life countdown: decrements +0x64;
	// at <= 0 unlinks the particle (0x742CD0), frees its slot (+0x69 = 0) and decrements the
	// director's particle count (dir+0x14)
	uint32_t __cdecl a_741B60(uint32_t a1)
	{
		U16(a1, 0x64) = (uint16_t)(U16(a1, 0x64) - 1);
		if (S16(a1, 0x64) > 0)
			return 0; // void
		a_742CD0(a1);
		uint32_t dir = U32(G(G_258FB78), 0);
		U8(a1, 0x69) = 0;
		U16(dir, 0x14) = (uint16_t)(U16(dir, 0x14) - 1);
		return 0; // void
	}

	// 0x742290 (engine; Siren au_re__rand_8, in 095-100): random integer from a1 towards a2:
	// a1 + rand() % (a2 - a1) (a1 - rem when a2 < a1); a1 when equal (1 rand draw otherwise)
	uint32_t __cdecl a_742290(uint32_t a1, uint32_t a2)
	{
		int32_t lo = (int32_t)a1;
		int32_t hi = (int32_t)a2;
		if (lo == hi)
			return (uint32_t)lo;
		int32_t range = sub32(hi, lo);
		int32_t r = (int32_t)x::CrtRand();
		int32_t rem = r % range;
		if (range < 0)
			return (uint32_t)sub32(lo, rem);
		return (uint32_t)add32(rem, lo);
	}

	// 0x7422C0 (engine; Siren sub_7422C0, in 095-100): like 0x742290 with a wide random
	// rand() * rand() (2 draws)
	uint32_t __cdecl a_7422C0(uint32_t a1, uint32_t a2)
	{
		int32_t lo = (int32_t)a1;
		int32_t hi = (int32_t)a2;
		if (lo == hi)
			return (uint32_t)lo;
		int32_t range = sub32(hi, lo);
		int32_t r1 = (int32_t)x::CrtRand();
		int32_t r2 = (int32_t)x::CrtRand();
		int32_t prod = mul32(r2, r1);
		int32_t rem = prod % range;
		if (range < 0)
			return (uint32_t)sub32(lo, rem);
		return (uint32_t)add32(rem, lo);
	}

	// 0x742300 (engine; Siren sub_742300, in 095-100): a1[i] = random between a2[i] and a3[i]
	// (0x7422C0) for 3 dwords in order; returns the last value
	uint32_t __cdecl a_742300(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		uint32_t r = a_7422C0(U32(a2, 0), U32(a3, 0));
		uint32_t hi1 = U32(a3, 4);
		U32(a1, 0) = r;
		r = a_7422C0(U32(a2, 4), hi1);
		uint32_t hi2 = U32(a3, 8);
		uint32_t lo2 = U32(a2, 8);
		U32(a1, 4) = r;
		r = a_7422C0(lo2, hi2);
		U32(a1, 8) = r;
		return r;
	}

	// 0x742350 (engine; Siren sub_742350, in 095-100): a1[i] = angle a2[i] jittered by +-a3[i]/2
	// and wrapped to 0..0xFFF (0x7423A0), 3 words in order; returns the last value
	uint32_t __cdecl a_742350(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// the original pushes eax/ecx after 16-bit loads (garbage high halves); 0x7423A0 reads only
		// the low words
		uint32_t r = a_7423A0(U16(a2, 0), U16(a3, 0));
		uint16_t rng1 = U16(a3, 2);
		U16(a1, 0) = (uint16_t)r;
		r = a_7423A0(U16(a2, 2), rng1);
		uint16_t rng2 = U16(a3, 4);
		uint16_t base2 = U16(a2, 4);
		U16(a1, 2) = (uint16_t)r;
		r = a_7423A0(base2, rng2);
		U16(a1, 4) = (uint16_t)r;
		return r;
	}

	// 0x7423A0 (engine; Siren au_re__rand_8_0, in 095-100): angle (s16)a1 + random in
	// [-(s16)a2/2, (s16)a2/2] (0x742290), wrapped into 0..0xFFF
	uint32_t __cdecl a_7423A0(uint32_t a1, uint32_t a2)
	{
		int32_t spread = (int32_t)(int16_t)a2;
		int32_t half = spread / 2;
		uint32_t r = a_742290((uint32_t)(-half), (uint32_t)half);
		uint32_t v = (uint32_t)add32((int32_t)r, (int32_t)(int16_t)a1);
		if ((int32_t)v < 0)
		{
			// 0x7423DC
			uint32_t k = ((0x1000u - v) >> 12) << 12;
			return v + k;
		}
		if ((int32_t)v < 0x1000)
			return v;
		uint32_t k = (0u - (v >> 12)) << 12;
		return v + k;
	}

	// 0x7424B0 (engine; Siren sub_7424B0, in 095-100): returns word a1+8 minus 1
	uint32_t __cdecl a_7424B0(uint32_t a1)
	{
		// quirk 0x7424B4: `mov ax, [eax+8]; dec eax` keeps a1's high word in eax (callers use al)
		return ((a1 & 0xFFFF0000u) | U16(a1, 8)) - 1;
	}

	// 0x7424C0 (engine; Siren sub_7424C0, in 095-100): attaches particle a1 to its owner emitter
	// instance (+0x5C, none -> nothing): position +0x4C..0x58 = owner position +0xDC (or the midpoint
	// of owner +0xDC/+0x11C when descriptor +0x35 == 1); when descriptor +0x1C == 3 also copies the
	// owner matrix (+0x8C -> +0x2C) and adds the descriptor offset rotated by it (<<16)
	uint32_t __cdecl a_7424C0(uint32_t a1)
	{
		uint32_t dir = U32(G(G_258FB78), 0);
		uint32_t table = U32(dir, 0x224);
		int32_t di = (int32_t)S8(a1, 0x6A);
		uint32_t owner = U32(a1, 0x5C);
		uint32_t desc = U32(table + di * 4, 0);
		if (owner == 0)
			return 0; // void
		if (U8(desc, 0x35) == 1)
		{
			U32(a1, 0x4C) = (uint32_t)(add32(S32(owner, 0x11C), S32(owner, 0xDC)) / 2);
			U32(a1, 0x50) = (uint32_t)(add32(S32(owner, 0x120), S32(owner, 0xE0)) / 2);
			U32(a1, 0x54) = (uint32_t)(add32(S32(owner, 0x124), S32(owner, 0xE4)) / 2);
		}
		else
		{
			uint32_t s = owner + 0xDC;
			U32(a1, 0x4C) = U32(s, 0);
			U32(a1, 0x50) = U32(s, 4);
			U32(a1, 0x54) = U32(s, 8);
			U32(a1, 0x58) = U32(s, 0xC);
		}
		// 0x742559
		if (U8(desc, 0x1C) != 3)
			return 0; // void
		memcpy((void *)(a1 + 0x2C), (void *)(owner + 0x8C), 32);
		uint8_t local[0x10] = {}; // [esp+0x10]: SVECTOR at +0 (pad +6 never written), IR123 at +8
		e4_load_offset(local, desc);
		x::GTE_SetRotMatrix(a1 + 0x2C);
		x::GTE_LoadV0(P(local));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(local + 8));
		int32_t ir1 = *(int16_t *)(local + 8);
		int32_t ir2 = *(int16_t *)(local + 0xA);
		int32_t ir3 = *(int16_t *)(local + 0xC);
		uint32_t px = U32(a1, 0x4C);
		U32(a1, 0x50) = (uint32_t)add32(S32(a1, 0x50), shl32(ir2, 16));
		U32(a1, 0x4C) = (uint32_t)add32((int32_t)px, shl32(ir1, 16));
		U32(a1, 0x54) = (uint32_t)add32(S32(a1, 0x54), shl32(ir3, 16));
		return 0; // void
	}

	// 0x742CA0 (engine; Siren sub_742CA0, in 095-100): sets particle a1 state word +8 = a2 and appends
	// it to the director's particle list (dir+0x2C head, dir+0x30 tail; node +0 prev, +4 next)
	uint32_t __cdecl a_742CA0(uint32_t a1, uint32_t a2)
	{
		U16(a1, 8) = (uint16_t)a2;
		uint32_t dir = U32(G(G_258FB78), 0);
		if (U32(dir, 0x2C) == 0)
		{
			U32(dir, 0x2C) = a1;
			U32(dir, 0x30) = a1;
			return a1;
		}
		uint32_t tail = U32(dir, 0x30);
		U32(dir, 0x30) = a1;
		U32(a1, 0) = tail;
		U32(tail, 4) = a1;
		return a1;
	}

	// 0x742CD0 (engine; Siren sub_742CD0, in 095-100): unlinks particle a1 from the director's
	// particle list (dir+0x2C head, dir+0x30 tail); returns its prev link
	uint32_t __cdecl a_742CD0(uint32_t a1)
	{
		uint32_t dir = U32(G(G_258FB78), 0);
		uint32_t prev = U32(a1, 0);
		uint32_t next = U32(a1, 4);
		if (prev == 0)
			U32(dir, 0x2C) = next;
		else
			U32(prev, 4) = next;
		if (next == 0)
			U32(dir, 0x30) = prev;
		else
			U32(next, 0) = prev;
		return prev;
	}

	// 0x742D00 (engine; Siren sub_742D00, in 095-100): particle init: copies descriptor placement
	// kind +0x11 to +0x68, builds the particle matrix +0x2C by descriptor +0x1C (identity / yaw
	// focus->anchor / yaw anchor->focus / owner matrix / yaw of battle entity dir+0x1E), places it
	// by kind (0x742EB0 / 0x742FF0 / 0x7424C0 / 0x742610) and sets its life +0x64 = desc +0x12
	uint32_t __cdecl a_742D00(uint32_t a1)
	{
		uint32_t dir = U32(G(G_258FB78), 0);
		uint32_t table = U32(dir, 0x224);
		int32_t di = (int32_t)S8(a1, 0x6A);
		uint32_t owner = U32(a1, 0x5C);
		uint32_t desc = U32(table + di * 4, 0); // ebp, kept across the calls
		U8(a1, 0x68) = U8(desc, 0x11);
		uint32_t m = a1 + 0x2C;
		switch ((uint32_t)(int32_t)S8(desc, 0x1C))
		{
			case 0: // 0x742D38
				x::MAG_022_sub_8DD770(m);
				break;
			case 1: // 0x742D49: yaw of (focus dir+0x1C4/0x1CC - anchor point dir+0x54/0x5C + 16*anchor)
			{
				x::MAG_022_sub_8DD770(m);
				int32_t k = shl32((int32_t)S8(a1, 0x6B), 4);
				uint32_t d = U32(G(G_258FB78), 0);
				uint32_t e = (uint32_t)k + d;
				int32_t dx = sub32(S32(d, 0x1C4), S32(e, 0x54));
				int32_t dz = sub32(S32(d, 0x1CC), S32(e, 0x5C));
				uint32_t ang = x::CartesianToGameAngle((uint32_t)dx, (uint32_t)dz);
				if (ang != 0)
					x::MAG_022_sub_8DD8A0(m, ang);
				break;
			}
			case 2: // 0x742D8B: yaw of (anchor point - focus)
			{
				x::MAG_022_sub_8DD770(m);
				int32_t k = shl32((int32_t)S8(a1, 0x6B), 4);
				uint32_t d = U32(G(G_258FB78), 0);
				int32_t dx = sub32(S32((uint32_t)k + d, 0x54), S32(d, 0x1C4));
				int32_t dz = sub32(S32((uint32_t)k + d, 0x5C), S32(d, 0x1CC));
				uint32_t ang = x::CartesianToGameAngle((uint32_t)dx, (uint32_t)dz);
				if (ang != 0)
					x::MAG_022_sub_8DD8A0(m, ang);
				break;
			}
			case 3: // 0x742DCB: owner instance matrix
				memcpy((void *)m, (void *)(owner + 0x8C), 32);
				break;
			case 4: // 0x742DDD: yaw of battle entity dir+0x1E (entity +0xE)
			{
				x::MAG_022_sub_8DD770(m);
				uint32_t d = U32(G(G_258FB78), 0);
				int32_t ent = (int32_t)S16(d, 0x1E);
				int32_t yaw = (int32_t)S16(0x1D972CE + (uint32_t)(ent * 39) * 4, 0);
				if (yaw != 0)
					x::MAG_022_sub_8DD8A0(m, (uint32_t)yaw);
				break;
			}
			default:
				break;
		}
		// 0x742E11
		switch ((uint32_t)(int32_t)S8(a1, 0x68))
		{
			case 0: a_742EB0(a1); break;             // 0x742E21
			case 1: a_742FF0(a1); break;             // 0x742E38 (listing: not ported yet; part e4 ports it)
			case 2:
			case 3: a_7424C0(a1); break;             // 0x742E4F
			case 4: callp(F(F_742610), a1); break;   // 0x742E66
			default: break;
		}
		U16(a1, 0x64) = (uint16_t)(int16_t)S8(desc, 0x12);
		return 0; // void
	}

	// 0x742EB0 (engine; Siren sub_742EB0, in 095-100): places particle a1 at one of the director's
	// four per-anchor point sets (dir+0x94/0xD4/0x114/0x154 + 16*anchor, by descriptor +0x25), then
	// adds the descriptor offset rotated by a stack copy of the particle matrix +0x2C (<<16)
	uint32_t __cdecl a_742EB0(uint32_t a1)
	{
		uint32_t dir = U32(G(G_258FB78), 0);
		uint32_t table = U32(dir, 0x224);
		uint32_t desc = U32(table + (int32_t)S8(a1, 0x6A) * 4, 0);
		uint32_t base = 0;
		bool has = true;
		switch ((uint32_t)(int32_t)S8(desc, 0x25))
		{
			case 0: base = 0x94; break;
			case 1: base = 0xD4; break;
			case 2: base = 0x114; break;
			case 3: base = 0x154; break;
			default: has = false; break;
		}
		if (has)
		{
			uint32_t src = (uint32_t)shl32((int32_t)S8(a1, 0x6B), 4) + dir + base;
			U32(a1, 0x4C) = U32(src, 0);
			U32(a1, 0x50) = U32(src, 4);
			uint32_t s8 = U32(src, 8);
			uint32_t sc = U32(src, 0xC);
			U32(a1, 0x54) = s8;
			U32(a1, 0x58) = sc;
		}
		// 0x742F34
		uint8_t local[0x30] = {}; // [esp+0xC]: SVECTOR +0 (pad +6 never written), IR123 +8, matrix +0x10
		e4_load_offset(local, desc);
		memcpy(local + 0x10, (void *)(a1 + 0x2C), 32);
		x::GTE_SetRotMatrix(P(local + 0x10));
		x::GTE_LoadV0(P(local));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(local + 8));
		int32_t ir1 = *(int16_t *)(local + 8);
		int32_t ir2 = *(int16_t *)(local + 0xA);
		int32_t ir3 = *(int16_t *)(local + 0xC);
		U32(a1, 0x4C) = (uint32_t)add32(S32(a1, 0x4C), shl32(ir1, 16));
		U32(a1, 0x50) = (uint32_t)add32(S32(a1, 0x50), shl32(ir2, 16));
		U32(a1, 0x54) = (uint32_t)add32(S32(a1, 0x54), shl32(ir3, 16));
		return 0; // void
	}

	// 0x742FF0 (engine; Siren sub_742FF0, in 095-100): places particle a1 at one of the director's
	// four single points (dir+0x1D4/0x1E4/0x1F4/0x204, by descriptor +0x25), then adds the
	// descriptor offset rotated by the particle matrix +0x2C (<<16)
	uint32_t __cdecl a_742FF0(uint32_t a1)
	{
		uint32_t dir = U32(G(G_258FB78), 0);
		uint32_t table = U32(dir, 0x224);
		uint32_t desc = U32(table + (int32_t)S8(a1, 0x6A) * 4, 0);
		uint32_t base = 0;
		bool has = true;
		switch ((uint32_t)(int32_t)S8(desc, 0x25))
		{
			case 0: base = 0x1D4; break;
			case 1: base = 0x1E4; break;
			case 2: base = 0x1F4; break;
			case 3: base = 0x204; break;
			default: has = false; break;
		}
		if (has)
		{
			uint32_t src = dir + base;
			U32(a1, 0x4C) = U32(src, 0);
			U32(a1, 0x50) = U32(src, 4);
			uint32_t s8 = U32(src, 8);
			uint32_t sc = U32(src, 0xC);
			U32(a1, 0x54) = s8;
			U32(a1, 0x58) = sc;
		}
		// 0x74304F
		uint8_t local[0x10] = {}; // [esp+8]: SVECTOR +0 (pad +6 never written), IR123 +8
		e4_load_offset(local, desc);
		x::GTE_SetRotMatrix(a1 + 0x2C);
		x::GTE_LoadV0(P(local));
		x::GTE_MVMVA_RotV0();
		x::GTE_StoreIR123(P(local + 8));
		int32_t ir1 = *(int16_t *)(local + 8);
		int32_t ir2 = *(int16_t *)(local + 0xA);
		int32_t ir3 = *(int16_t *)(local + 0xC);
		U32(a1, 0x4C) = (uint32_t)add32(S32(a1, 0x4C), shl32(ir1, 16));
		U32(a1, 0x50) = (uint32_t)add32(S32(a1, 0x50), shl32(ir2, 16));
		U32(a1, 0x54) = (uint32_t)add32(S32(a1, 0x54), shl32(ir3, 16));
		return 0; // void
	}

	// ====================================================================================
	// part e5
	// ====================================================================================
	// Part e5: actor state-machine engine helpers 0x743100-0x7475A0 (Siren canonical addresses):
	// sprite-strip sequence init/draw (0x7435E0/0x743720), creature model draw (0x746C10), prim-model
	// draw (0x7458E0), sub-effect spawners (0x743100/0x743190) and the small state-table handlers.
	// 0x743100 (engine; Siren sub_743100): a1 = actor/effect object (+0x1D9 s8 variant), a2 = spawn
	// descriptor: for each of the 4 bytes a2+0x28..0x2B equal to 1, spawns a sub-effect slot
	// (a_741A70) with id s8 a2+0x2C..0x2F and variant s8 a1+0x1D9.
	uint32_t __cdecl a_743100(uint32_t a1, uint32_t a2)
	{
		const uint32_t obj = a1;   // edi
		const uint32_t desc = a2;  // esi
		// quirk 0x743112..0x743179: args 2/3 are pushed as 32-bit registers of which only the low
		// 16 bits are set (movsx r16); the high halves are leftover register bits. a_741A70 only
		// reads their low byte, so the zero-extended 16-bit value is passed here.
		if (U8(desc, 0x28) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2C);
			a_741A70(obj, id, variant);
		}
		if (U8(desc, 0x29) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2D);
			a_741A70(obj, id, variant);
		}
		if (U8(desc, 0x2A) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2E);
			a_741A70(obj, id, variant);
		}
		if (U8(desc, 0x2B) == 1)
		{
			uint32_t variant = (uint16_t)(int16_t)S8(obj, 0x1D9);
			uint32_t id = (uint16_t)(int16_t)S8(desc, 0x2F);
			a_741A70(obj, id, variant);
		}
		return 0; // void
	}

	// 0x743190 (engine; Siren sub_743190): for each of the 16 director slots (director G_258FB78,
	// +0x224 slot table, +0x22 u16 skip mask) whose entry has kind (+0x11) 0/1/4 and group (+0x13)
	// equal to director +0x18: spawns sub-effect slots via a_741A70(0, slot, n) - n = 0..count-1
	// (count = director +0x1C) when entry +0x37 == 1, else once with n = 0.
	uint32_t __cdecl a_743190(void)
	{
		uint32_t dir = MEM<uint32_t>(G(G_258FB78));  // eax (re-read only after the calls)
		for (int32_t slot = 0; slot < 0x10; slot++)
		{
			uint32_t table = U32(dir, 0x224);
			uint32_t entry = U32(table, slot * 4);
			if (entry == 0)
				continue;
			uint32_t bit = (uint32_t)shl32(1, slot);
			uint32_t mask = U16(dir, 0x22);
			if (bit & mask)
				continue;
			uint8_t kind = U8(entry, 0x11);
			if (kind != 0 && kind != 1 && kind != 4)
				continue;
			int16_t group = (int16_t)S8(entry, 0x13);
			if (group != S16(dir, 0x18))
				continue;
			if (U8(entry, 0x37) == 1)
			{
				int32_t n = 0;
				if (S16(dir, 0x1C) <= 0)
					continue;
				do
				{
					a_741A70(0, (uint32_t)slot, (uint32_t)n);
					dir = MEM<uint32_t>(G(G_258FB78));
					n++;
				} while (n < (int32_t)S16(dir, 0x1C));
			}
			else
			{
				a_741A70(0, (uint32_t)slot, 0);
				dir = MEM<uint32_t>(G(G_258FB78));
			}
		}
		return 0; // void
	}

	// 0x7435E0 (engine; Siren InitEffectSequenceFromData_c2): (re)starts a sprite-strip sequence
	// node a1: a1+0 data header, u16 a1+4 = sequence index -> +0x26/+0x28 u16 params, +0x2C quad
	// list, +0x30 quad count (bit31 -> a1+0x25 bit0 = semi-trans DR_MODE prim), clears the quad
	// z's, copies the camera matrix to a1+0x84 and applies the Z roll / scale, sets the default
	// colour / palette offsets / brightness bias, then draws the quads (a_743720) into packet
	// cursor a4 (OT a2, OT index/shift a3) and returns the new cursor.
	uint32_t __cdecl a_7435E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t node = a1;              // esi
		const uint32_t data = U32(node, 0);    // ebx
		U16(node, 0x26) = U16(data, 8);
		uint32_t seq = U16(node, 4);
		uint32_t offs_tab = data + 10;
		uint32_t offs = U16(offs_tab, seq * 2);
		U16(node, 0x28) = U16(offs_tab, seq * 2 + 2);
		uint32_t list = offs + data;
		int32_t count = S32(list, 0);
		list += 4;
		S32(node, 0x30) = count;
		U32(node, 0x2C) = list;
		if (count < 0)
		{
			count &= 0x7FFFFFFF;
			U8(node, 0x25) |= 1;
			S32(node, 0x30) = count;
		}
		const uint32_t cam = node + 0x84;      // edi: local camera matrix
		U16(node, 0x60) = 0;
		U16(node, 0x58) = 0;
		U16(node, 0x50) = 0;
		U16(node, 0x48) = 0;
		x::CopyCameraStateToBuffer(cam);
		uint16_t fl = U16(node, 0x24);
		if (fl & 1)
		{
			int32_t roll = S16(0x1D977A2, 0);
			x::ApplyZRotation((uint32_t)sub32(S32(node, 8), roll), cam);
		}
		else if (!(fl & 0x200))
		{
			int32_t roll = S16(0x1D977A2, 0);
			x::ApplyZRotation((uint32_t)sub32(0, roll), cam);
		}
		if (U8(node, 0x24) & 2)
		{
			U32(node, 0x14) = 0x1000;
			x::ScaleMatrix3x3(cam, node + 0xC);
		}
		fl = U16(node, 0x24);
		if (!(fl & 4))
			U32(node, 0x1C) = 0x808080;
		if (!(fl & 0x10))
			U8(node, 0x20) = U8(data, 4);
		if (!(fl & 0x20))
			U8(node, 0x21) = U8(data, 5);
		if (!(fl & 0x40))
			U8(node, 0x22) = U8(data, 6);
		if (!(fl & 0x80))
			U8(node, 0x23) = U8(data, 7);
		if (fl & 8)
			U32(node, 0xB0) = 1;
		else
			U32(node, 0xB0) = 0;
		U32(node, 0xAC) = 0;           // cached angle
		U32(node, 0xA4) = 0x1000;      // cached cos
		U32(node, 0xA8) = 0;           // cached sin
		U32(node, 0x3C) = 0xFFFFFFFF;  // depth: < 0 = compute (AVSZ4) on the first drawn quad
		return a_743720(node, a2, a3, a4);
	}

	// 0x743720 (engine; Siren sub_743720): draws the sprite-strip quads of node a1 (+0x2C list of
	// 0x14-byte entries, +0x30 count): per quad builds a rotated/scaled matrix (+0x64), a POLY_FT4
	// (0x28 bytes) at cursor a4 coloured by +0x1C..0x1E, UVs from the entry, CLUT row offset by the
	// palette byte +0x20[flags>>12], tpage = flags & 0x1FF, projected with RTPT/RTPS through the
	// camera matrix +0x84, then inserts it into OT a2 (depth-key tables 0x1CA8A50..5C). The first
	// quad with +0x3C < 0 computes the depth (AVSZ4 + bias +0xB4, >> a3) and, with +0x25 bit0, a
	// DR_MODE (semi-trans) prim; later quads reuse that depth. Returns the new packet cursor.
	uint32_t __cdecl a_743720(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
	{
		const uint32_t node = a1;               // esi
		uint32_t pkt = a4;                      // edi
		const int32_t count = S32(node, 0x30);  // [esp+0x14] local
		uint32_t ent = U32(node, 0x2C);         // ebx
		// the a3/a4 stack slots are reused as locals by the original:
		//   a3 slot = OT index; the auto-depth path overwrites it with the computed depth
		//   a4 slot = quad width (then the OT pointer in the semi-trans path)
		uint32_t ot_slot = a3;
		uint32_t a4slot = a4;
		if (count <= 0)
			return pkt;
		for (int32_t i = 0; i < count; i++, ent += 0x14)
		{
			const int32_t flags = S16(ent, 0xE);  // [esp+0x10] local (sign-extended)
			// rotation/scale matrix +0x64 (3x3 s16)
			U16(node, 0x72) = 0;
			U16(node, 0x70) = 0;
			U16(node, 0x6E) = 0;
			U16(node, 0x6A) = 0;
			U16(node, 0x68) = 0;
			U16(node, 0x66) = 0;
			U16(node, 0x74) = 0x1000;
			if (flags & 0xC00)
			{
				int32_t ang = S16(ent, 0xC);
				if (ang == 0)
				{
					U32(node, 0xA4) = 0x1000;
					U32(node, 0xA8) = 0;
					U32(node, 0xAC) = 0;
				}
				else if (ang != S32(node, 0xAC))
				{
					uint32_t c = x::computeCosine((uint32_t)ang);
					U32(node, 0xA4) = c;
					uint32_t s = x::computeSin((uint32_t)ang);
					U32(node, 0xA8) = s;
					S32(node, 0xAC) = ang;
				}
				int32_t sx = S16(ent, 0x10);
				int32_t c = S32(node, 0xA4);
				U16(node, 0x64) = (uint16_t)(mul32(c, sx) >> 12);
				int32_t s = S32(node, 0xA8);
				int32_t m10 = mul32(s, sx);
				int32_t sy = S16(ent, 0x12);
				int32_t m01 = mul32(s, sy);
				int32_t m11 = mul32(c, sy);
				m01 = sub32(0, m01);  // neg before sar
				m10 >>= 12;
				m01 >>= 12;
				m11 >>= 12;
				U16(node, 0x6A) = (uint16_t)m10;
				U16(node, 0x66) = (uint16_t)m01;
				U16(node, 0x6C) = (uint16_t)m11;
			}
			else
			{
				U16(node, 0x6C) = 0x1000;
				U16(node, 0x64) = 0x1000;
			}
			// quad corners (SVECTOR x,y at +0x44/+0x4C/+0x54/+0x5C, z cleared by the init)
			const uint32_t w = U8(ent, 0);
			const uint32_t h = U8(ent, 2);
			a4slot = w;
			const uint32_t hl = h;  // [esp+0x14] local
			uint32_t w8 = w << 3;
			U16(node, 0x5C) = (uint16_t)w8;
			U16(node, 0x54) = (uint16_t)(0u - w8);
			U16(node, 0x44) = (uint16_t)(0u - w8);
			uint32_t h8 = h << 3;
			U16(node, 0x4C) = (uint16_t)w8;
			U16(node, 0x4E) = (uint16_t)(0u - h8);
			U16(node, 0x46) = (uint16_t)(0u - h8);
			int32_t cx = add32(shl32(S16(ent, 8), 4), (int32_t)w8);
			U16(node, 0x5E) = (uint16_t)h8;
			int32_t cy = add32(shl32(S16(ent, 0xA), 4), (int32_t)h8);
			U16(node, 0x56) = (uint16_t)h8;
			S32(node, 0x7C) = cy;
			S32(node, 0x78) = cx;  // quad offset (VECTOR +0x78)
			U32(node, 0x80) = 0;
			// rotate the matrix columns and the offset by the camera matrix +0x84
			x::GTE_SetRotMatrix(node + 0x84);
			const uint32_t rot = node + 0x64;  // ebp
			x::GTE_LoadIRFromMatrixColumn(rot);
			x::GTE_MVMVA_RotIR();
			uint32_t d0 = U32(ent, 0);
			uint32_t d1 = U32(ent, 4);
			U32(pkt, 0) = 0x9000000;  // tag: 9 words
			U32(pkt, 4) = d0;         // [w, intensity, h, code]
			U32(pkt, 0xC) = d1;       // u0 v0 clut
			x::GTE_StoreIRToMatrixColumn(rot);
			x::GTE_LoadIRFromMatrixColumn(node + 0x66);
			x::GTE_MVMVA_RotIR();
			{
				uint8_t in = U8(pkt, 5);
				uint32_t cr = U8(node, 0x1C);
				U8(pkt, 6) = in;
				U8(pkt, 4) = (uint8_t)(((uint32_t)in * cr) >> 7);  // r
			}
			x::GTE_StoreIRToMatrixColumn(node + 0x66);
			x::GTE_LoadIRFromMatrixColumn(node + 0x68);
			x::GTE_MVMVA_RotIR();
			{
				uint32_t in = U8(pkt, 5);
				uint32_t cg = U8(node, 0x1D);
				uint32_t g = in * cg;
				uint8_t u = U8(ent, 4);
				U8(pkt, 5) = (uint8_t)(g >> 7);  // g
				U8(pkt, 0x1C) = u;               // u2
			}
			x::GTE_StoreIRToMatrixColumn(node + 0x68);
			x::GTE_SetTransVector(node + 0x84);
			x::GTE_LoadV0FromDwords(node + 0x78);
			x::GTE_MVMVA_RotV0_Tr();
			{
				uint32_t cb = U8(node, 0x1E);
				uint32_t in = U8(pkt, 6);
				uint32_t b = cb * in;
				uint8_t v = U8(ent, 5);
				U8(pkt, 6) = (uint8_t)(b >> 7);  // b
				U8(pkt, 0x15) = v;               // v1
			}
			x::GTE_ReadMAC123(node + 0x78);
			x::GTE_SetRotMatrix(rot);
			x::GTE_SetTransVector(rot);
			x::GTE_LoadV012(node + 0x44, node + 0x4C, node + 0x54);
			x::GTE_RTPT();
			{
				int32_t u = add32(sub32((int32_t)U8(ent, 4), S32(node, 0xB0)), (int32_t)a4slot);
				if (u >= 0x100)
					u = 0xFF;
				U8(pkt, 0x24) = (uint8_t)u;  // u3
				U8(pkt, 0x14) = (uint8_t)u;  // u1
			}
			{
				uint32_t pal_idx = ((uint32_t)flags & 0xFFFF) >> 12;
				int16_t pal = (int16_t)S8(node + pal_idx, 0x20);
				U16(pkt, 0xE) = (uint16_t)(U16(pkt, 0xE) + (uint16_t)(pal << 6));  // clut row
			}
			x::GTE_ReadFLAG(node + 0x40);
			if (U32(node, 0x40) & 0x60000)
				continue;  // projection overflow: quad skipped (cursor not advanced)
			U16(pkt, 0x16) = (uint16_t)(flags & 0x1FF);  // tpage
			x::GTE_ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
			x::GTE_LoadV0(node + 0x5C);
			x::GTE_RTPS();
			{
				int32_t v = add32(sub32((int32_t)U8(ent, 5), S32(node, 0xB0)), (int32_t)hl);
				if (v >= 0x100)
					v = 0xFF;
				U8(pkt, 0x25) = (uint8_t)v;  // v3
				U8(pkt, 0x1D) = (uint8_t)v;  // v2
			}
			x::GTE_ReadSXY2(pkt + 0x20);
			const uint32_t ioff = (uint32_t)i * 4;
			int32_t z = S32(node, 0x3C);
			if (z >= 0)
			{
				// depth already known: bias it again, insert at the stored OT index
				z = add32(z, S16(node, 0xB4));
				S32(node, 0x3C) = z;
				if (z < 0)
					S32(node, 0x3C) = 0;
				uint32_t k3 = MEM<uint32_t>(0x1CA8A5C) + ioff;
				uint32_t k2 = MEM<uint32_t>(0x1CA8A58) + ioff;
				uint32_t k1 = MEM<uint32_t>(0x1CA8A54) + ioff;
				uint32_t k0 = MEM<uint32_t>(0x1CA8A50) + ioff;
				x::SSIGPU_InsertPrimDepthKeys(a2 + ot_slot * 4, pkt, k0, k1, k2, k3);
				pkt += 0x28;
			}
			else
			{
				x::GTE_AVSZ4();
				x::GTE_ReadOTZ(node + 0x3C);
				z = add32(S32(node, 0x3C), S16(node, 0xB4));
				S32(node, 0x3C) = z;
				if (z < 0)
					S32(node, 0x3C) = 0;
				int32_t depth = S32(node, 0x3C) >> (ot_slot & 31);
				ot_slot = (uint32_t)depth;  // quirk 0x743ABB: the a3 slot now holds the depth
				if (U8(node, 0x25) & 1)
				{
					uint32_t mode = pkt + 0x28;
					uint32_t ot = a2 + (uint32_t)depth * 4;
					U32(mode, 0) = 0x1000000;   // tag: 1 word
					U32(mode, 4) = 0xE1000220;  // draw mode (semi-trans)
					a4slot = ot;
					x::SSIGPU_InsertPrimAutoDepth(ot, mode);
					uint32_t k3 = MEM<uint32_t>(0x1CA8A5C) + ioff;
					uint32_t k2 = MEM<uint32_t>(0x1CA8A58) + ioff;
					uint32_t k1 = MEM<uint32_t>(0x1CA8A54) + ioff;
					uint32_t k0 = MEM<uint32_t>(0x1CA8A50) + ioff;
					x::SSIGPU_InsertPrimDepthKeys(a4slot, pkt, k0, k1, k2, k3);
					pkt = mode + 0xC;  // quirk 0x743B1E: advances 0x28+0xC (DR_MODE slot is 12 bytes)
				}
				else
				{
					uint32_t k3 = MEM<uint32_t>(0x1CA8A5C) + ioff;
					uint32_t k2 = MEM<uint32_t>(0x1CA8A58) + ioff;
					uint32_t k1 = MEM<uint32_t>(0x1CA8A54) + ioff;
					uint32_t k0 = MEM<uint32_t>(0x1CA8A50) + ioff;
					x::SSIGPU_InsertPrimDepthKeys(a2 + (uint32_t)depth * 4, pkt, k0, k1, k2, k3);
					pkt += 0x28;
				}
			}
		}
		return pkt;
	}

	// 0x743C00 (engine; Siren sub_743C00): state handler: advances the +0x50 counter (a_743C20);
	// when it passes +0x52, marks the node finished (+0x26 bit0) and goes to the next state.
	uint32_t __cdecl a_743C00(uint32_t a1)
	{
		const uint32_t node = a1;
		if (a_743C20(node) != 0)
		{
			uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x743C20 (engine; Siren sub_743C20): increments the s16 counter +0x50; when it exceeds the
	// limit +0x52, clamps it, sets +0x26 bit2 and returns 1, else returns 0.
	uint32_t __cdecl a_743C20(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) + 1);
		int16_t cur = S16(node, 0x50);
		int16_t lim = S16(node, 0x52);
		if (cur > lim)
		{
			U8(node, 0x26) |= 4;
			S16(node, 0x50) = lim;
			return 1;
		}
		return 0;
	}

	// 0x7458E0 (engine; Siren sub_7458E0): unless +0x26 bit2 (hidden): builds a local matrix from
	// the s16 angles +0x46/+0x44/+0x48, position +0x1C..0x20 and scale +0x30, composes it with the
	// camera 0x1D97778, loads it into the GTE and draws the prim model (+0x4C model, +0x40, +0x50)
	// with Effect_RenderPrimModel into the effect OT (0x1D8E04C+0x44) at packet cursor 0x1D8E054.
	uint32_t __cdecl a_7458E0(uint32_t a1)
	{
		const uint32_t node = a1;  // esi
		if (U8(node, 0x26) & 4)
			return 0; // void
		// local Mat4x3 [esp+4] (3x3 s16 + pad + VECTOR t at +0x14); written by 0x8DD770 and the
		// translation stores (UNINIT 0x7458F2: pad bytes +0x12/+0x13, zeroed here, never read)
		uint32_t mbuf[8] = {};
		const uint32_t m = P(mbuf);
		x::MAG_022_sub_8DD770(m);
		x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(node, 0x46));
		x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(node, 0x44));
		x::sub_8DD960(m, (uint32_t)(int32_t)S16(node, 0x48));
		int32_t tx = S16(node, 0x1C);
		int32_t ty = S16(node, 0x1E);
		int32_t tz = S16(node, 0x20);
		S32(m, 0x14) = tx;
		S32(m, 0x18) = ty;
		S32(m, 0x1C) = tz;
		x::scale3DMatrix(m, node + 0x30);
		x::ComposeAffineTransform(0x1D97778, m, m);
		x::GTE_SetRotMatrix_W(m);
		x::GTE_SetTransVector_W(m);
		uint32_t prm = x::Field_Alloc(0x58);
		uint32_t model = U32(node, 0x4C);
		uint32_t v40 = U32(node, 0x40);
		U32(prm, 0) = model;
		U32(prm, 8) = v40;
		int32_t v50 = S16(node, 0x50);
		uint32_t cursor = MEM<uint32_t>(0x1D8E054);
		S32(prm, 0xC) = v50;
		uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U32(prm, 0x1C) = 0xF0;
		uint32_t next = x::Effect_RenderPrimModel(prm, ot, 2, cursor);
		MEM<uint32_t>(0x1D8E054) = next;
		x::Field_Free(0x58);
		return 0; // void
	}

	// 0x746980 (engine; Siren sub_746980): state handler: sets node +0x1C = 0x600 and, for 4
	// records of stride 0x2C (k = 0..3), u16 0x1D98992+k*0x2C = 0x600 and bytes
	// 0x1D989B8..0x1D989BA +k*0x2C = 0; goes to the next state.
	uint32_t __cdecl a_746980(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x1C) = 0x600;
		uint32_t p = 0x1D989BA;
		for (int k = 0; k < 4; k++)
		{
			U16(p, -0x28) = 0x600;
			U8(p, 0) = 0;
			U8(p, -1) = 0;
			U8(p, -2) = 0;
			p += 0x2C;
		}
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x746A10 (engine; Siren sub_746A10): clears bit1 of byte 0x1D98991 + k*0x2C, k = 0..3.
	uint32_t __cdecl a_746A10(void)
	{
		uint32_t p = 0x1D98991;
		for (int k = 0; k < 4; k++)
		{
			uint8_t b = U8(p, 0);
			p += 0x2C;
			U8(p, -0x2C) = (uint8_t)(b & 0xFD);
		}
		return 0; // void
	}

	// 0x746AE0 (engine; Siren sub_746AE0): for the 4 records at 0x1D98991 + k*0x2C copies bit2
	// into bit1 (restores the flag cleared by a_746A10).
	uint32_t __cdecl a_746AE0(void)
	{
		uint32_t p = 0x1D98991;
		for (int k = 0; k < 4; k++)
		{
			uint8_t b = U8(p, 0);
			p += 0x2C;
			uint8_t bit = (uint8_t)((b >> 1) & 2);
			uint8_t keep = (uint8_t)(b & 0xFD);
			U8(p, -0x2C) = (uint8_t)(bit | keep);
		}
		return 0; // void
	}

	// 0x746C10 (engine; Siren GF_095Siren_DrawModel): draws the battle model of creature node a1:
	// model instance mdl = a1+0x30 (+0 flags, +7 brightness, +0xC s16 rotation angles, +0x1C s16
	// pos, +0x28, +0x34..+0x3E s16 bbox, +0x40 Mat4x3, +0x60 skeleton = a1+0x90 BattleAnimHeader,
	// +0x64 = a1+0x94 geometry, +0x7C = a1+0xAC), scale vector a1+0x114, bbox shrink u16 a1+0x13E.
	// Builds the model matrix, the bone world matrices, RenderGeometry into the effect OT
	// (0x1D8E04C+0x44) at packet cursor a3 (a2 = render param stored in the work struct +0x24),
	// rebuilds the bone matrices from the pose, updates the bbox (sub_5086F0, shrunk toward its
	// centre by +0x13E/4096 when != 0x1000), draws the shadow (sub_5088A0, OT +0x4064) unless mdl
	// flag 0x20, and returns the new packet cursor.
	uint32_t __cdecl a_746C10(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		const uint32_t mdl = a1 + 0x30;            // esi
		const uint32_t ws = x::Field_Alloc(0x4C);  // edi: render work struct
		const uint32_t mat = mdl + 0x40;           // ebp
		const uint32_t skel = mdl + 0x60;          // ebx
		x::ComposeZYXRotationMatrix(mdl + 0xC, mat);
		int32_t px = S16(mdl, 0x1C);
		int32_t py = S16(mdl, 0x1E);
		int32_t pz = S16(mdl, 0x20);
		S32(mdl, 0x54) = px;
		S32(mdl, 0x58) = py;
		S32(mdl, 0x5C) = pz;
		x::scale3DMatrix(mat, a1 + 0x114);
		x::ComposeAffineTransform(0x1D97778, mat, ws);
		x::BS_ComputeBonesWorldMatrices(skel, ws);
		uint32_t v28 = U32(mdl, 0x28);
		U32(ws, 0x24) = a2;
		uint8_t bright = U8(mdl, 7);
		U8(ws, 0x4A) = bright;
		U8(ws, 0x49) = bright;
		U8(ws, 0x48) = bright;
		uint32_t v7c = U32(mdl, 0x7C);
		U32(ws, 0x3C) = v28;
		U16(ws, 0x34) = 0;
		U16(ws, 0x36) = 0;
		U16(ws, 0x44) = 0;
		uint32_t g969a8 = MEM<uint32_t>(G(G_1D969A8));
		U32(ws, 0x40) = v7c;
		uint32_t otbase = MEM<uint32_t>(0x1D8E04C);
		U16(ws, 0x38) = 0x140;  // screen w
		U16(ws, 0x3A) = 0xD8;   // screen h
		U32(ws, 0x30) = g969a8;
		uint32_t geom = U32(skel, 4);
		uint32_t cursor = x::RenderGeometry(geom, ws + 0x20, otbase + 0x44, 4, a3);  // -> a3 slot
		x::BattleModel_BuildBoneMatricesFromPose(skel);
		x::sub_5086F0(mdl);
		uint16_t shrink = U16(a1, 0x13E);
		if (shrink != 0x1000)
		{
			// bbox shrink toward the centre: s16 +0x34/+0x3A pair and +0x38/+0x3E pair
			// (quirk 0x746D37/0x746D71: the centre is re-read as a dword from a stack slot whose
			// high half is uninitialised; only the low 16 bits of the sums are stored -> exact)
			int32_t b3a = S16(mdl, 0x3A);
			int32_t b34 = S16(mdl, 0x34);
			int32_t midA = (b3a + b34) / 2;
			int32_t b38 = S16(mdl, 0x38);
			int32_t midB = (S16(mdl, 0x3E) + b38) / 2;
			int32_t cA = (int16_t)midA;
			int32_t s = (int16_t)shrink;
			S16(mdl, 0x34) = (int16_t)add32(mul32(b34 - cA, s) / 4096, cA);
			S16(mdl, 0x3A) = (int16_t)add32(mul32(b3a - cA, s) / 4096, cA);
			int32_t cB = (int16_t)midB;
			S16(mdl, 0x38) = (int16_t)add32(mul32(S16(mdl, 0x38) - cB, s) / 4096, cB);
			S16(mdl, 0x3E) = (int16_t)add32(mul32(S16(mdl, 0x3E) - cB, s) / 4096, cB);
		}
		if (!(U8(mdl, 0) & 0x20))
		{
			uint32_t ot2 = MEM<uint32_t>(0x1D8E04C) + 0x4064;
			cursor = x::sub_5088A0(mdl, ot2, 0x10, cursor);
		}
		x::Field_Free(0x4C);
		return cursor;
	}

	// 0x747400 (engine; Siren sub_747400): state handler: steps the node's animation
	// (sub_8DD1C0); once the director counter (G_1533010 +0x40) >= 6 (a_73B7E0), marks the node
	// finished + hidden (+0x26 |= 5) and goes to the next state.
	uint32_t __cdecl a_747400(uint32_t a1)
	{
		const uint32_t node = a1;
		x::sub_8DD1C0(node);
		if (a_73B7E0(6) != 0)
		{
			uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 5;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x747440 (engine; Siren MAG_095_sub_747440): state handler: when the director flag
	// (G_1533010 +0x48) == 1, applies the action results (ApplyActionResultToTarget) of every
	// target record (0x18 bytes, list +8, count u8 +0x10) of action entry cast->+4 [s8 node+0x2A]
	// (0x14 bytes each) and goes to the next state.
	uint32_t __cdecl a_747440(uint32_t a1)
	{
		uint32_t d = MEM<uint32_t>(G(G_1533010));
		if (U16(d, 0x48) != 1)
			return 0; // void
		const uint32_t node = a1;  // esi
		int32_t n = 0;             // edi
		int32_t act = S8(node, 0x2A);
		uint32_t ctx = U32(node, 0xC);
		uint32_t entry = U32(ctx, 4) + (uint32_t)(act * 5) * 4;
		if (U8(entry, 0x10) != 0)
		{
			uint32_t off = 0;  // ebx
			do
			{
				uint32_t rec = U32(entry, 8) + off;
				x::ApplyActionResultToTarget(rec);
				act = S8(node, 0x2A);
				n++;
				off += 0x18;
				ctx = U32(node, 0xC);
				entry = U32(ctx, 4) + (uint32_t)(act * 5) * 4;
			} while (n < (int32_t)U8(entry, 0x10));
		}
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7474B0 (engine; Siren MAG_095_sub_7474B0): state handler: waits until the node has no
	// live children (+0x28 == 0) or the director flag G_1533010 +0x4C == 1, then next state.
	uint32_t __cdecl a_7474B0(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U8(node, 0x28) != 0)
		{
			uint32_t d = MEM<uint32_t>(G(G_1533010));
			if (U16(d, 0x4C) != 1)
				return 0; // void
		}
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7474D0 (engine; Siren MAG_095_sub_7474D0): state handler: marks the node finished
	// (+0x26 bit0), clears root director +0x63 and goes to the next state.
	uint32_t __cdecl a_7474D0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t root = U32(node, 0x10);
		U8(node, 0x26) |= 1;
		U8(root, 0x63) = 0;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x747500 (engine; Siren MAG_095_sub_747500): state handler (target loop): when +0x63 == 0
	// and the action index s8 +0x2A < s16 +0x58, advances +0x2A and +0x2E and steps the state
	// back one (repeat the previous state for the next target); else goes to the next state.
	// With +0x63 != 0 it waits.
	uint32_t __cdecl a_747500(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U8(node, 0x63) != 0)
			return 0; // void
		uint8_t idx = U8(node, 0x2A);
		if ((int16_t)(int8_t)idx < S16(node, 0x58))
		{
			uint8_t loops = U8(node, 0x2E);
			U8(node, 0x2A) = (uint8_t)(idx + 1);
			uint8_t st = U8(node, 0x29);
			U8(node, 0x2E) = (uint8_t)(loops + 1);
			U8(node, 0x29) = (uint8_t)(st - 1);
			return 0; // void
		}
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x747550 (engine; Siren MAG_095_sub_747550): state handler: waits until u16 +0x5E == 0,
	// then next state.
	uint32_t __cdecl a_747550(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U16(node, 0x5E) == 0)
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x747560 (engine; Siren MAG_095_sub_747560): state handler: sub_508630(*G_257F8A0,
	// node+0x62) (request whose completion byte is +0x62) and goes to the next state.
	uint32_t __cdecl a_747560(uint32_t a1)
	{
		uint32_t g = MEM<uint32_t>(G(G_257F8A0));
		const uint32_t node = a1;
		x::sub_508630(g, node + 0x62);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x747590 (engine; Siren MAG_095_sub_747590): state handler: waits until byte +0x62 != 0
	// (set by the sub_508630 request), then next state.
	uint32_t __cdecl a_747590(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U8(node, 0x62) != 0)
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7475A0 (engine; Siren MAG_095_sub_7475A0): state handler: marks the node finished
	// (+0x26 bit0) and goes to the next state.
	uint32_t __cdecl a_7475A0(uint32_t a1)
	{
		const uint32_t node = a1;
		uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// ====================================================================================
	// part e6
	// ====================================================================================
	// Part e6 - engine particle-actor spawner a_737FD0 (MiniMog 096 0x737FD0, Boko 097 0x72E580,
	// 098 0x7260B0, 099 0x71C600, 100 0x713280). Instruction-for-instruction identical to the Siren
	// copy s_741C10 (part s4) except that the module globals are reached through G():
	// G_258FB68 = module scratch stack pointer, G_258FB78 = module context.
	namespace
	{
		// 4-dword copy (the inline mov pairs of 0x7380C5 / 0x7380E9)
		inline void e6_copy16(uint32_t dst, uint32_t src)
		{
			U32(dst, 0) = U32(src, 0);
			U32(dst, 4) = U32(src, 4);
			U32(dst, 8) = U32(src, 8);
			U32(dst, 12) = U32(src, 12);
		}

		// identity matrix m, then rotations Z, X, Y by the angle words at ang (+4, +0, +2), each
		// only when nonzero (0x738136.. / 0x7382B7.. / 0x73848F..)
		inline void e6_rotate_zxy(uint32_t m, uint32_t ang)
		{
			x::MAG_022_sub_8DD770(m);
			if (U16(ang, 4) != 0) x::sub_8DD960(m, (uint32_t)(int32_t)S16(ang, 4));
			if (U16(ang, 0) != 0) x::sub_8DD7E0(m, (uint32_t)(int32_t)S16(ang, 0));
			if (U16(ang, 2) != 0) x::MAG_022_sub_8DD8A0(m, (uint32_t)(int32_t)S16(ang, 2));
		}

		// 0x73822B / 0x7383E9: v = sp+0x20 = (0, -0x1000, 0) rotated by the matrix at sp
		// (IR1..3 stored back into v)
		inline void e6_rotate_down_vector(uint32_t sp)
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

		// word = rand() % s16(word), only when the word is nonzero (0x7381FD.. / 0x738360..)
		inline void e6_rand_mod(uint32_t sp, int32_t off)
		{
			if (U16(sp, off) != 0)
			{
				int32_t r = (int32_t)x::CrtRand();
				U16(sp, off) = (uint16_t)(r % (int32_t)S16(sp, off));
			}
		}
	}

	// 0x737FD0 (engine; MiniMog sub_737FD0, Boko 0x72E580/0x7260B0/0x71C600/0x713280): allocates a
	// particle actor record (a_7387B0) for the emitter a1 and initialises its a3 particles from the
	// descriptor a2: start positions (+0xDC/+0x12C, spread along a randomly rotated down vector
	// when desc+0x34 = 1/2), per-particle rotation matrices (+0x0C+0x20i, multiplied by the emitter
	// matrix a1+0x2C), speeds (+0x174/+0x184), key colours (+0x1E0..), then a_743100; uses 0x50
	// bytes of the module scratch stack [G_258FB68]
	uint32_t __cdecl a_737FD0(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// scratch: +0 matrix, +0x20 vector, +0x28/+0x30 angles, +0x38 offset, +0x48 spread keys,
		// +0x4E key index
		uint32_t sp = U32(G(G_258FB68), 0) - 0x50;
		int16_t kind = (int16_t)S8(a1, 0x6A);
		U32(G(G_258FB68), 0) = sp;
		// quirk 0x737FE1: `movsx ax` - the high half of the pushed dword is stale eax; a_7387B0
		// only reads the low byte (0x73880E `mov cl, [esp+0x1c]`)
		uint32_t node = a_7387B0(a1, (uint16_t)kind);
		uint32_t desc = a2;
		uint8_t a3_lo = (uint8_t)a3;
		uint16_t w60 = U16(a1, 0x60);
		uint8_t mode = U8(desc, 0x16);
		U8(node, 0x1D8) = a3_lo;                           // particle count
		U16(sp, 0x4E) = w60;                               // key index into the descriptor tables
		if (mode == 0 || mode == 1 || mode == 2)
		{
			uint32_t ctx = U32(G(G_258FB78), 0);
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
			// (the original keeps these pointers in its own argument slots a1 / a2: compiler reuse)
			uint32_t src = a1 + 0x4C;          // emitter position (16 bytes)
			uint32_t emat = a1 + 0x2C;         // emitter matrix
			uint32_t out_speed = node + 0x184;
			uint32_t mat = node + 0x0C;
			uint32_t pos = node + 0x12C;
			for (int32_t n = (int32_t)a3; n != 0; n--)
			{
				e6_copy16(pos - 0x50, src);    // +0xDC+0x10i: origin
				if (U8(desc, 0x22) == 0)
					e6_copy16(pos, src);       // +0x12C+0x10i: current position
				uint8_t spread = U8(desc, 0x34);
				if (spread == 1)
				{
					// 0x738110: random direction over the whole sphere
					U16(sp, 0x28) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2A) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2C) = (uint16_t)(x::CrtRand() & 0xFFF);
					e6_rotate_zxy(sp, sp + 0x28);
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
						e6_rand_mod(sp, 0x48);
						e6_rand_mod(sp, 0x4A);
						e6_rand_mod(sp, 0x4C);
					}
					e6_rotate_down_vector(sp);
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
					// 0x7382AD: random direction on a ring (angles rand, 0, 0x400)
					U16(sp, 0x28) = (uint16_t)(x::CrtRand() & 0xFFF);
					U16(sp, 0x2A) = 0;
					U16(sp, 0x2C) = 0x400;
					e6_rotate_zxy(sp, sp + 0x28);
					int32_t i2 = shl32(S16(sp, 0x4E), 1);
					if (S8(desc, 0x33) != 1)
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), i2);
						e6_rand_mod(sp, 0x48);
						e6_rand_mod(sp, 0x4A);
						e6_rand_mod(sp, 0x4C);
					}
					else
					{
						U16(sp, 0x48) = U16(U32(desc, 0xCC), i2);
						U16(sp, 0x4A) = U16(U32(desc, 0xD0), i2);
						e6_rand_mod(sp, 0x4A);
						int32_t e = S16(sp, 0x4E);
						U16(sp, 0x4C) = U16(U32(desc, 0xD4), shl32(e, 1));
					}
					if ((x::CrtRand() & 1) != 0)
						U16(sp, 0x4A) = (uint16_t)(0 - U16(sp, 0x4A));
					e6_rotate_down_vector(sp);
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
				// 0x738461: particle rotation angles sp+0x30 (random range, or a copy of the
				// spread angles; any other mode keeps the stale scratch words)
				int32_t rmode = S8(desc, 0x21);
				if (rmode == 0)
					a_742350(sp + 0x30, desc + 0x3C, desc + 0x44);
				else if (rmode == 1)
				{
					// quirk 0x73846D: copies the dwords sp+0x28 / sp+0x2C, including the word
					// +0x2E this function never writes (stale module scratch, deterministic;
					// +0x28..+0x2C are also stale when desc+0x34 is neither 1 nor 2)
					uint32_t c0 = U32(sp, 0x28);
					uint32_t c1 = U32(sp, 0x2C);
					U32(sp, 0x30) = c0;
					U32(sp, 0x34) = c1;
				}
				e6_rotate_zxy(mat, sp + 0x30);
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
					U32(out_speed, -0x10) = r1;           // +0x174+4i
					uint32_t r2 = a_7422C0(U32(desc, 0x54), U32(desc, 0x58));
					U32(out_speed, 0) = r2;               // +0x184+4i
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
				// three key-colour sets per particle (+0x1E0 / +0x220 / +0x260, 16 bytes each)
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
		U32(G(G_258FB68), 0) = U32(G(G_258FB68), 0) + 0x50;
		return 0; // void
	}
}
}
