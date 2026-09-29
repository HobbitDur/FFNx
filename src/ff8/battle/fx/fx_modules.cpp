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

// Per-effect module table, engine regions and external call sites (see fx_modules.h).
// Module global ranges come from gf_study/gf_global_ranges.md.

#include "fx_modules.h"

namespace ff8fx::mod
{
	// Quezacotl: its streamed files
	static const Region streams_q116[] = { { 0x1298C68, 0x109FC }, { 0x12A9664, 0x4D9C } };
	// Cactuar: pool/table pointer cells 0xCF3564..0xCF3593 (written at the creature's first tick)
	static const Region streams_c199[] = { { 0xCF3564, 0x30 }, { 0x24FBDA4, 0x162B } }; // + shared camera script cells
	// Pandemona: texture load state machine cell (outside the module range)
	static const Region streams_p291[] = { { 0x13BBBE8, 4 } };
	// Odin: ripple-texture destination in exe data, pool/scratch pointer cells, summon data (loads 0x227/0x229/0x22A)
	static const Region streams_o187[] = { { 0xFA94A8, 0x4000 }, { 0xE41E50, 0xC }, { 0xE41E98, 4 }, { 0x209FAB8, 0x40000 }, { 0x24FBDA4, 0x162B } };
	// Doomtrain: pool pointer cells, summon data (loads 0x22E..0x231), shadow-camera scratch
	static const Region streams_d191[] = { { 0xE3C8C0, 0x18 }, { 0x209FAB8, 0x40000 }, { 0x21DFED0, 0x20 }, { 0x1D99C18, 8 } };
	// Gilgamesh: the 16 sword model files in .data (their skeletons are rebuilt), summon data
	// (loads 0x2EE/0x2F0/0x2F1), pool pointer cells
	static const Region streams_g327[] = { { 0xE808D0, 0x10C50 }, { 0x209FAB8, 0x40000 }, { 0xCD0398, 0xC }, { 0x24FBDA4, 0x162B } };
	// Odin reverse: loads 0x2E3/0x2E8 into exe data + the ripple texture right after (0xEECC2C..0xEF4C30),
	// pool/scratch pointer cells, summon data (loads 0x2E5/0x2E7), the camera script's saved camera
	static const Region streams_o326[] = { { 0xEECC2C, 0x8004 }, { 0xE19668, 0xC }, { 0xE196B0, 4 }, { 0x209FAB8, 0x40000 }, { 0x24FBDA4, 0x162B } };
	// GF cinematic engine (Ifrit family): mesh depth scale, fog words, camera-related word
	static const Region streams_gfc[] = { { 0x1877DA8, 4 }, { 0x209AB64, 0x10 }, { 0xC78BF0, 0x10 }, { 0x1D96DC4, 4 }, { 0x2798C40, 0x35C } }; // + tail of the per-vertex depth array 0x2798C18 (Breath reaches 0x2798F9A)
	// actor family: module scratch stack + camera copy (0x2793DA4..0x2793E78), ParsePolygons temporaries,
	// static cells the modules write, Siren's load destinations outside the 1 MB buffer
	static const Region streams_a095[] = { { 0x2793DA4, 0xD4 }, { 0x153386C, 0x154 }, { 0x16A40EC, 0x1284 }, { 0x2795110, 0x68 }, { 0x19D1018, 0x23600 }, { 0x1D99C18, 8 } };
	// Tonberry / Boko: module data, shared scratch + camera copy, RenderGeometry temporaries,
	// TransformCameraByShadowRotation scratch, Boko load destinations outside the 1 MB buffer
	static const Region streams_a090[] = { { 0x15474EC, 0x530 }, { 0x2793DA4, 0xD4 }, { 0x2795960, 0x218 }, { 0x1D99C18, 8 } };
	static const Region streams_a097[] = { { 0x152B94C, 0xD4 }, { 0x168356C, 0xC340 }, { 0x2793DA4, 0xD4 }, { 0x1D99C18, 8 }, { 0x21DFED0, 0x20 }, { 0x19D1018, 0x23600 } };
	static const Region streams_a098[] = { { 0x152B2C4, 0xF4 }, { 0x167484C, 0xC340 }, { 0x2793DA4, 0xD4 }, { 0x1D99C18, 8 }, { 0x21DFED0, 0x20 }, { 0x19D1018, 0x23600 } };
	static const Region streams_a099[] = { { 0x152AABC, 0x174 }, { 0x1665B28, 0x8120 }, { 0x2793DA4, 0xD4 }, { 0x1D99C18, 8 }, { 0x21DFED0, 0x20 }, { 0x19D1018, 0x23600 }, { 0x2795113, 0x68 } };
	static const Region streams_a100[] = { { 0x1529B8C, 0x134 }, { 0x1656E04, 0xC340 }, { 0x2793DA4, 0xD4 }, { 0x1D99C18, 8 }, { 0x21DFED0, 0x20 }, { 0x19D1018, 0x23600 } };
	// Fire: per-target free flags (exe data) + shadow-camera scratch; Fira/Firaga: scratch
	static const Region streams_f002[] = { { 0xDEE360, 0x1C }, { 0x21DFED0, 0x20 } };
	static const Region streams_f142[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_f143[] = { { 0x21DFED0, 0x20 } };
	// Thunder/Thundara: shadow-camera scratch; Thundaga: + the shard meshes it rewrites in exe data
	static const Region streams_t003[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_t102[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_t105[] = { { 0x130E55C, 0x3040 }, { 0x21DFED0, 0x20 } };
	// Blizzard/Blizzara: shadow-camera scratch; Blizzaga: + the static UV model list and the
	// sparkle matrix translation it rewrites in exe data
	static const Region streams_i144[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_i103[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_i104[] = { { 0x1315774, 0x1900 }, { 0x13197C4, 0xC }, { 0x21DFED0, 0x20 } };
	// Bio: acid CLUT and the mesh vertex blocks it rewrites in exe data; Holy: draw-environment
	// restore packets in exe data; Quake/Ultima/both: shadow-camera scratch
	static const Region streams_b022[] = { { 0x16146FC, 0x100 }, { 0x1615CEC, 0x2D8 }, { 0x1617E44, 0x150 }, { 0x1619890, 0xE98 }, { 0x161F758, 0x810 }, { 0x21DFED0, 0x20 } };
	static const Region streams_q038[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_u149[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_h175[] = { { 0x21DFED0, 0x20 }, { 0xD592C0, 0x30 } };
	// Meltdown: entity-shadow temporaries, RenderGeometry temporaries; Pain: camera copy;
	// Drain/Death (actor library copies): scratch stack + camera copy, actor data they rewrite in exe
	// data, Death's reaper model container
	static const Region streams_m148[] = { { 0x1D999C8, 0x80 }, { 0xB8B9E4, 0xC }, { 0x1D99C18, 8 }, { 0x21DFED0, 0x20 } };
	static const Region streams_p034[] = { { 0x2793E58, 0x20 } };
	static const Region streams_d039[] = { { 0x2793DA4, 0xD4 }, { 0x15D1F18, 0x140 } };
	// Confuse (actor library copy): the two morph vertex arrays it rewrites in exe data + scratch;
	// Flare/Sleep: shadow-camera scratch; Tornado: funnel matrix translation and capture OT link in
	// exe data, scratch, RenderGeometry temporary (entities, camera and stage flag are engine regions)
	static const Region streams_c036[] = { { 0x15D9E0C, 0x590 }, { 0x15DBAE4, 0x590 }, { 0x2793DA4, 0xD4 } };
	static const Region streams_f124[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_s145[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_t146[] = { { 0xDD674C, 0xC }, { 0xDD6780, 4 }, { 0x21DFED0, 0x20 }, { 0x1D99C18, 8 } };
	// Demi: the mote matrix it rewrites in exe data (also read by the silhouette pull), scratch and
	// entity-shadow temporaries; Dispel/Aura: shadow-camera scratch
	static const Region streams_d107[] = { { 0x12F9978, 0x20 }, { 0x21DFED0, 0x20 }, { 0x1D999C8, 0x80 }, { 0xB8B9E4, 0xC } };
	static const Region streams_d109[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_a111[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_b117[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_b108[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_r147[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_e024[] = { { 0x2793DA4, 0xD4 }, { 0x17E48E4, 0x9C }, { 0x17E9DA8, 0x738 } };
	static const Region streams_p032[] = { { 0x2793E58, 0x20 }, { 0x21DFED0, 0x20 } };
	static const Region streams_s033[] = { { 0x2793DA4, 0xD4 } };
	static const Region streams_l035[] = { { 0x2792E74, 4 }, { 0x21DFED0, 0x20 } };
	static const Region streams_f027[] = { { 0x17DE100, 0x104 }, { 0x17E398C, 0xF48 }, { 0x2793DA4, 0xD4 } };
	static const Region streams_c028[] = { { 0x17D5F4C, 0xD4 }, { 0x17DCC48, 0x14A8 }, { 0x2793DA4, 0xD4 } };
	static const Region streams_c025[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_c001[] = { { 0x2792E74, 4 }, { 0x2793E58, 0x20 }, { 0x21DFED0, 0x20 } };
	static const Region streams_c004[] = { { 0x163FFD8, 0x25E }, { 0x21DFED0, 0x20 } };
	static const Region streams_c021[] = { { 0x1627978, 0x27E }, { 0x21DFED0, 0x20 } };
	static const Region streams_c040[] = { { 0x15CDDF4, 0x180 }, { 0x15CE1DC, 0x500 }, { 0x1D2A278, 1 }, { 0x1D2B330, 0xA4 }, { 0x1D2B558, 0x32 }, { 0x2795BD0, 4 }, { 0x279CC68, 0x2C } };
	static const Region streams_e029[] = { { 0x17D27DC, 0x98 }, { 0x17D56AC, 0x890 }, { 0x2793DA4, 0xD4 } };
	static const Region streams_c026[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_r010[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_s011[] = { { 0x1365728, 0x9A0 }, { 0x21DFED0, 0x20 } };
	static const Region streams_s155[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_g168[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_u154[] = { { 0x1D99C18, 8 }, { 0xDBAEE0, 0x30 } };
	static const Region streams_s180[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_g169[] = { { 0x21DFED0, 0x20 }, { 0x1D999C8, 0x80 }, { 0xB8B9E4, 0xC } };
	static const Region streams_a126[] = { { 0x124227C, 4 } };
	static const Region streams_w153[] = { { 0x21DFED0, 0x20 } };
	static const Region streams_m013[] = { { 0x163257C, 0x169C }, { 0x21DFED0, 0x20 }, { 0x1D9770C, 2 } };
	static const Region streams_d232[] = { { 0x1877DA8, 4 }, { 0x209AB64, 0x10 }, { 0xC78BF0, 0x10 }, { 0x1D96DC4, 4 }, { 0x2798C40, 0x478 }, { 0x1D99C18, 8 } }; // streams_gfc with the depth-array tail up to 0x27990B6
	static const Region streams_d014[] = { { 0x2793DA4, 0xD4 }, { 0x1D99C18, 8 }, { 0x162C97C, 0x548 }, { 0x1801FD0, 0x6FD0 } };
	static const Region streams_a096[] = { { 0x2793DA4, 0xD4 }, { 0x152BBBC, 0x60E }, { 0x169227C, 0xA4 }, { 0x1696140, 0xBCA4 }, { 0x1D99C18, 8 } };
	// Shiva: glow-ring node cell, summon data (CharacterLoad 0x221/0x222/0x224 destination, read by
	// BdTransSummonStream), TransformCameraByShadowRotation scratch
	static const Region streams_s185[] = { { 0xD32508, 4 }, { 0x209FAB8, 0x40000 }, { 0x21DFED0, 0x20 }, { 0x24FBDA4, 0x162B }, { 0x1D99C18, 8 } };
	static const Region streams_p140[] = { { 0x11B539C, 0xB17 }, { 0x1D99C18, 8 }, { 0x21DFED0, 0x20 } };
	static const Region streams_c278[] = { { 0x10B6FB4, 0x5A9 }, { 0x1D99C18, 8 }, { 0x21DFED0, 0x20 } };
	static const Region streams_m338[] = { { 0x136BB50, 0x549 }, { 0x1D99C18, 8 } };

	const Module modules[] = {
		// timeline-A (own pause flag, creature spawned by the master at counter 2)
		{ 116, 0x25216D8, 0x25217D0, 0, 0, true, "Quezacotl", streams_q116, 2 },
		{ 325, 0x250517C, 0x2505230, 0, 0, false, "Diablos" },
		{ 278, 0x2508110, 0x25081FC, 0, 0, false, "Carbuncle", streams_c278, 3 },
		{ 291, 0x2556258, 0x25562F8, 0, 0, false, "Pandemona", streams_p291, 1 },
		{ 140, 0x2517AA0, 0x2517B50, 0, 0, false, "Phoenix", streams_p140, 3 },
		{ 338, 0x25561C8, 0x2556254, 0, 0, false, "Moomba", streams_m338, 2 },
		{ 69,  0x2556628, 0x2556F98, 0, 0, false, "Griever" },
		// timeline-B (draw-only mode on battle_to_update_flags bit0, creature spawned by the timeline)
		{ 185, 0x22BC128, 0x22BD108, 0, 0, false, "Shiva", streams_s185, 5 },
		{ 199, 0x2259950, 0x225A8E4, 0xCF3A68, 4, false, "Cactuar", streams_c199, 2 }, // + its private rand seed
		{ 187, 0x24FD458, 0x24FE910, 0, 0, false, "Odin", streams_o187, 5 },
		{ 326, 0x24F0BD0, 0x24F2308, 0, 0, false, "Odin (reverse)", streams_o326, 5 },
		{ 191, 0x24FBD68, 0x24FD458, 0, 0, false, "Doomtrain", streams_d191, 4 },
		{ 327, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Zantetsuken)", streams_g327, 4 },
		{ 328, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Masamune)", streams_g327, 4 },
		{ 329, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Excalibur)", streams_g327, 4 },
		{ 330, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Excalipoor)", streams_g327, 4 },
		// GF cinematic engine family (engine state block 0x2796E00..0x2798C40)
		{ 201, 0x2796E00, 0x2798C40, 0, 0, false, "Ifrit", streams_gfc, 5 },
		// the six other compilations: same engine block, plus each clone's queue/file pointer statics below it
		{ 6,   0x2796EF8, 0x2798C40, 0, 0, false, "Leviathan", streams_gfc, 5 },
		{ 202, 0x2796DE0, 0x2798C40, 0, 0, false, "Bahamut", streams_gfc, 5 },
		{ 203, 0x2796DA8, 0x2798C40, 0, 0, false, "Cerberus", streams_gfc, 5 },
		{ 204, 0x2796D70, 0x2798C40, 0, 0, false, "Alexander", streams_gfc, 5 },
		{ 205, 0x2796D30, 0x2798C40, 0, 0, false, "Brothers", streams_gfc, 5 },
		{ 206, 0x2796CF8, 0x2798C40, 0, 0, false, "Eden", streams_gfc, 5 },
		// actor state-machine family (the heal-spell effect library)
		{ 95,  0x257F8A0, 0x258FCF4, 0, 0, false, "Siren", streams_a095, 6 },
		{ 96,  0x257B740, 0x257F8A0, 0, 0, false, "MiniMog", streams_a096, 5 },
		{ 90,  0x259EEA8, 0x25A4E00, 0, 0, false, "Tonberry", streams_a090, 4 },
		{ 97,  0x2575268, 0x257B740, 0, 0, false, "Boko ChocoFire", streams_a097, 6 },
		{ 98,  0x256E358, 0x2575268, 0, 0, false, "Boko ChocoFlare", streams_a098, 6 },
		{ 99,  0x2565A30, 0x256E358, 0, 0, false, "Boko ChocoMeteor", streams_a099, 7 },
		{ 100, 0x255EAC8, 0x2565A30, 0, 0, false, "Boko ChocoBocle", streams_a100, 6 },
		// spells
		{ 2,   0x24CA818, 0x24DFD70, 0, 0, false, "Fire", streams_f002, 2 },
		{ 142, 0x24BFB48, 0x24C10B4, 0, 0, false, "Fira", streams_f142, 1 },
		{ 143, 0x24BECC8, 0x24BFB40, 0, 0, false, "Firaga", streams_f143, 1 },
		{ 3,   0x2557000, 0x2557098, 0, 0, false, "Thunder", streams_t003, 1 },
		{ 102, 0x2543F18, 0x2544E10, 0, 0, false, "Thundara", streams_t102, 1 },
		{ 105, 0x2543CF0, 0x2543DF0, 0, 0, false, "Thundaga", streams_t105, 2 },
		{ 144, 0x24BB960, 0x24BECC4, 0, 0, false, "Blizzard", streams_i144, 1 },
		{ 103, 0x2543E90, 0x2543F18, 0, 0, false, "Blizzara", streams_i103, 1 },
		{ 104, 0x2543DF0, 0x2543E90, 0, 0, false, "Blizzaga", streams_i104, 3 },
		{ 22,  0x27044E0, 0x2714F24, 0, 0, false, "Bio", streams_b022, 6 },
		{ 38,  0x278C8E0, 0x2792E60, 0, 0, false, "Quake", streams_q038, 1 },
		{ 149, 0x2461E88, 0x2464BA8, 0, 0, false, "Ultima", streams_u149, 1 },
		{ 175, 0x2374578, 0x2394208, 0x1D99C18, 4, false, "Holy", streams_h175, 2 }, // + RenderGeometry temporary
		{ 148, 0x2464BA8, 0x2476660, 0, 0, false, "Meltdown", streams_m148, 4 },
		{ 34,  0x26BA9E0, 0x26C0380, 0, 0, false, "Pain", streams_p034, 1 },
		{ 39,  0x269D958, 0x269E764, 0, 0, false, "Drain", streams_d039, 2 },
		{ 14,  0x273AE80, 0x27428A0, 0, 0, false, "Death", streams_d014, 4 },
		{ 36,  0x26A8DB8, 0x26A9BC4, 0, 0, false, "Confuse", streams_c036, 3 },
		{ 124, 0x25213D0, 0x2521448, 0, 0, false, "Flare", streams_f124, 1 },
		{ 145, 0x24AA630, 0x24BB960, 0x1D99C18, 8, false, "Sleep", streams_s145, 1 },
		{ 146, 0x2491BC0, 0x24AA62C, 0, 0, false, "Tornado", streams_t146, 4 },
		{ 107, 0x2521B30, 0x2521BB8, 0, 0, false, "Demi", streams_d107, 4 },
		{ 109, 0x2521A68, 0x2521AC0, 0, 0, false, "Dispel", streams_d109, 1 },
		{ 111, 0x2521970, 0x25219C8, 0, 0, false, "Aura", streams_a111, 1 },
		{ 118, 0x25215D8, 0x2521650, 0, 0, false, "Aero", nullptr, 0 },
		{ 123, 0x2521448, 0x25214B0, 0, 0, false, "Slow", nullptr, 0 },
		{ 119, 0x2521590, 0x25215D8, 0, 0, false, "Stop", nullptr, 0 },
		{ 125, 0x2521368, 0x25213D0, 0, 0, false, "Haste", nullptr, 0 },
		{ 115, 0x25217D0, 0x2521818, 0, 0, false, "Float", nullptr, 0 },
		{ 114, 0x2521818, 0x252186C, 0, 0, false, "Zombie", nullptr, 0 },
		{ 117, 0x2521650, 0x25216D8, 0, 0, false, "Break", streams_b117, 1 },
		{ 108, 0x2521AC0, 0x2521B30, 0, 0, false, "Berserk", streams_b108, 1 },
		{ 122, 0x25214B0, 0x25214F8, 0, 0, false, "Silence", nullptr, 0 },
		{ 121, 0x25214F8, 0x2521540, 0, 0, false, "Blind", nullptr, 0 },
		{ 147, 0x2476660, 0x2491BBC, 0, 0, false, "Regen", streams_r147, 1 },
		{ 24, 0x2700470, 0x27011B4, 0, 0, false, "Esuna", streams_e024, 3 },
		{ 32, 0x26C3890, 0x26CDCC8, 0, 0, false, "Protect", streams_p032, 2 },
		{ 33, 0x26C0380, 0x26C30EC, 0, 0, false, "Shell", streams_s033, 1 },
		{ 35, 0x26AC780, 0x26BA9C8, 0, 0, false, "Life", streams_l035, 2 },
		{ 27, 0x26D8308, 0x26D904C, 0, 0, false, "Full-life", streams_f027, 3 },
		{ 28, 0x26D74F8, 0x26D8304, 0, 0, false, "Curaga", streams_c028, 3 },
		{ 106, 0x2521BB8, 0x2543CF0, 0, 0, false, "Reflect", nullptr, 0 },
		{ 25, 0x26E5908, 0x2700470, 0, 0, false, "Cura", streams_c025, 1 },
		{ 1, 0x277AEC0, 0x278C8E0, 0, 0, false, "Cure", streams_c001, 3 },
		{ 4, 0x276FB00, 0x277AEA8, 0, 0, false, "Double", streams_c004, 2 },
		{ 21, 0x2714F28, 0x2721580, 0, 0, false, "Triple", streams_c021, 2 },
		{ 40, 0x269A160, 0x269D93C, 0, 0, false, "Scan", streams_c040, 7 },
		{ 222, 0x2796BA8, 0x2798C40, 0, 0, false, "Water", streams_gfc, 5 },
		{ 223, 0x2796B70, 0x2798C40, 0, 0, false, "Meteor", streams_gfc, 5 },
		{ 120, 0x2521540, 0x2521590, 0, 0, false, "Petrify Stare", nullptr, 0 },
		{ 152, 0x244BE70, 0x244D670, 0, 0, false, "Ultrasonic Waves", nullptr, 0 },
		{ 127, 0x2521270, 0x25212C0, 0, 0, false, "Petrify Stare (Cockatrice)", nullptr, 0 },
		{ 29, 0x26D6A40, 0x26D74F4, 0, 0, false, "Wind Blast", streams_e029, 3 },
		{ 235, 0x27969B0, 0x2798C40, 0, 0, false, "Breath", streams_gfc, 5 },
		{ 231, 0x2796A90, 0x2798C40, 0, 0, false, "Disease Breath", streams_gfc, 5 },
		{ 233, 0x2796A20, 0x2798C40, 0, 0, false, "Earthquake", streams_gfc, 5 },
		{ 26, 0x26D9050, 0x26E58F0, 0, 0, false, "Clash", streams_c026, 1 },
		{ 10, 0x24C91D8, 0x24CA810, 0, 0, false, "Ray-Bomb", streams_r010, 1 },
		{ 11, 0x2556140, 0x25561C8, 0, 0, false, "Storm Breath", streams_s011, 2 },
		{ 155, 0x23F07F0, 0x2404234, 0, 0, false, "Sand Storm", streams_s155, 1 },
		{ 168, 0x23B3C10, 0x23BD350, 0, 0, false, "Sleeping Gas", streams_g168, 1 },
		{ 154, 0x2404238, 0x243A4D8, 0, 0, false, "Ultra Waves", streams_u154, 2 },
		{ 180, 0x2307D98, 0x2309554, 0, 0, false, "Suicide", streams_s180, 1 },
		{ 169, 0x23B1370, 0x23B3C10, 0x1D99C18, 8, false, "Gastric Juice", streams_g169, 3 },
		{ 126, 0x25212C0, 0x2521368, 0, 0, false, "Electric Discharge", streams_a126, 1 },
		{ 153, 0x243A4D8, 0x244BE70, 0, 0, false, "Sticky Web", streams_w153, 1 },
		{ 13, 0x27428A8, 0x274EFE4, 0, 0, false, "Dark Mist", streams_m013, 3 },
		{ 232, 0x2796A58, 0x2798C40, 0, 0, false, "Breath of Death", streams_d232, 6 },
	};
	const int module_count = (int)(sizeof(modules) / sizeof(modules[0]));

	const Region engine_regions[] = {
		{ 0x20DFAB8, 0x100000, "magic buffer" }, // MAGIC_TEXTURE_BUFFER_BASE: creature, particle pools, packet arenas
		{ 0x21DFAB8, 4, "magic buffer cursor" },  // its fill cursor (zeroed with it by Magic_ClearMemoryForTex)
		{ 0x1D99A88, 4, "MAGIC_TEXTURE_BUFFER_PTR" },
		{ 0xB8B7F0, 0x20, "battle camera" },      // Battle_Camera_world/LookAt + ReturnView
		{ 0x1D97700, 0xB0, "camera state" },      // camera shake, roll, blend, BD_LINK_TASK_HEADER_CAMERA, view matrix
		{ 0x1D8E038, 0x20, "render list globals" }, // word_1D8E038 .. battle_texture_data_ptr_1D8E054 (frame packet cursor)
		{ 0x1CA8A10, 0x70, "GTE data registers" },
		{ 0x1CA9210, 0xF0, "GTE control registers" },            // + the float light-matrix mirror below them
		{ 0x209D078, 0x2A40, "hit-effect queue pool + header" },  // effects spawn screen fades into it
		{ 0x1D9898C, 0xDC, "battle entity array" },  // screen flash, currentBsId visibility bits
		{ 0x1D972C0, 0x440, "BattleEntitySlotData" }, // entity_flags hide bits, Carbuncle's party lift, Odin/Doomtrain target edits
		{ 0x1D99AB0, 0x20, "effect light/position" }, // shared effect light/position struct (Moomba)
		{ 0x1DCD6E0, 0x10, "streamed-file loader" },  // dword_1DCD6E4/E8/EC
		{ 0x1D999C4, 4, "Field_Alloc pointer" },
		{ 0x2557098, 4, "effect LCG seed" },          // shared effect LCG seed (MAG_106_sub_7059E0)
		{ 0x1CFF6F4, 4, "screen feedback request" },  // g_Battle_ScreenFeedbackRequest
		{ 0x1D98220, 0x204, "VRAM upload queue" },    // + count
		{ 0x1CA8828, 4, "ssigpu_execution_cur" },
	};
	const int engine_region_count = (int)(sizeof(engine_regions) / sizeof(engine_regions[0]));

	const ExtSite ext_sites[] = {
		{ 0x501330, 3, "BdPlaySE" },
		{ 0x5018C0, 3, "BdPlaySummonStream" },
		{ 0x501860, 2, "BdTransSummonStream" },
		{ 0x4A29A0, 3, "BdSound_ClaimVoiceSlot" },
		{ 0x4A2940, 1, "BdSound_ReleaseVoiceSlot" },
		{ 0x48D0A0, 4, "pre_LoadBattleFile" },
		{ 0x508480, 0, "BattleFile_CharacterLoad" },
		{ 0x506BA0, 2, "ApplyActionResultToTargets" },
		{ 0x506690, 0, "ApplyActionResultToTarget" },
		{ 0x505C00, 0, "QueueChainTransformation" },
		{ 0x506C10, 1, "AddTaskToQueueAnimSeq" },
		// battle-stage model swaps (Griever: attacker model 36/37 at tick 50, swap at 110)
		{ 0x512AA0, 2, "loadBS_36Or37" },
		{ 0x512AC0, 2, "BS_SwapModel_512AC0" },
		// GF cinematic engine (Ifrit family): sound/music, CLUT upload and battle calls it makes directly
		{ 0x46B3A0, 0, "Sound_46B3A0" },
		{ 0x46B3E0, 0, "Sound_46B3E0" },
		{ 0x46B450, 0, "Sound_46B450" },
		{ 0x46BBC0, 0, "Sound_46BBC0" },
		{ 0x47E3C0, 0, "Music_47E3C0" },
		{ 0xB666F0, 0, "GfCinematic_ClutUpload" },
		{ 0x4A8480, 0, "Battle_4A8480" },
		{ 0x501E40, 0, "Battle_501E40" },
		{ 0x501F30, 0, "Battle_501F30" },
		{ 0x504270, 0, "Battle_504270" },
		{ 0x5099A0, 0, "Battle_5099A0" },
		{ 0x50A730, 0, "Battle_50A730" },
		// cinematic clones: off-screen model/stage renders (pose battle models, execute an OT),
		// blit queue, streaming volume, screen feedback request (Eden draw 58)
		{ 0xB65810, 0, "GfCinematic_RenderPartyModelsOffscreen" },
		{ 0xB65D10, 0, "GfCinematic_RenderBattleStageOffscreen" },
		{ 0xB65F30, 0, "GfCinematic_RenderBattleModelOffscreen" },
		{ 0xB65B00, 0, "GfCinematic_RenderStageGroundOffscreen" },
		{ 0x505EB0, 0, "QueueBlitCommand" },
		{ 0x46BD40, 0, "SdStreamingVolumeTranslation" },
		{ 0x47CF50, 0, "Battle_RequestScreenFeedback" },
		// actor family (Siren, MiniMog, Tonberry, Boko): music volume fades, conditional hit reaction
		{ 0x46BB40, 0, "Music_SetVolumeImmediate" },
		{ 0x505CE0, 0, "QueueChainTransformationConditional" },
		// Tonberry's stream, Boko's TIM uploads
		{ 0x501460, 0, "BdPlayStream" },
		{ 0x505E30, 0, "Battle_QueueTIMUpload_GetEOF" },
		// cinematic GFs, Water, Meteor, Odin: VRAM uploads; texture restore; misc engine calls
		{ 0x505DF0, 0, "Battle_QueueVramUpload" },
		{ 0x508630, 0, "Battle_RestoreDefaultEffectTexture" },
		{ 0x4A2900, 0, "Engine_4A2900" },
		{ 0x403D99, 0, "OutputDebugString_1" },
		// Meteor: VRAM readback request (VM 0x035)
		{ 0x505E70, 0, "Battle_QueueVramReadback_Type2" },
		// spells: positional sound (3rd argument points into the caller's stack: 2 compared)
		{ 0x5013A0, 2, "BdPlaySE3D" },
		// Scan: pad input (the target viewer and the text pager poll the pad), text layers,
		// scan text, entity names
		{ 0x49ED30, 2, "read_pad_held_raw" },
		{ 0x4A2D60, 1, "remap_pad_input" },
		{ 0x49F0A0, 3, "ReadAnalog_49F0A0" },
		{ 0x4A0410, 1, "Text_SetLayerText" },
		{ 0x4A0700, 0, "Text_4A0700" },
		{ 0x4A07A0, 0, "setTextPosition" },
		{ 0x4A0640, 0, "Text_4A0640" },
		{ 0x4A0680, 0, "Text_4A0680" },
		{ 0x49FBC0, 0, "Text_49FBC0" },
		{ 0xB68390, 1, "manageScanText_Dup" },
		{ 0xB68810, 0, "ScanText_B68810" },
		{ 0xB687C0, 0, "ScanText_B687C0" },
		{ 0x47EAF0, 1, "CharacterName_47EAF0" },
		{ 0x495100, 1, "MonsterName_495100" },
	};
	const int ext_site_count = (int)(sizeof(ext_sites) / sizeof(ext_sites[0]));

	const Module *find_module(int effect_id)
	{
		for (int m = 0; m < module_count; m++)
			if (modules[m].effect_id == effect_id) return &modules[m];
		return nullptr;
	}

	uint32_t streams_bytes(const Module &m)
	{
		uint32_t n = 0;
		for (int i = 0; i < m.nstreams; i++) n += m.streams[i].size;
		return n;
	}
}
