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
	static const Region streams_c199[] = { { 0xCF3564, 0x30 } };
	// Pandemona: texture load state machine cell (outside the module range)
	static const Region streams_p291[] = { { 0x13BBBE8, 4 } };
	// Odin: ripple-texture destination in exe data, pool/scratch pointer cells, summon data (loads 0x227/0x229/0x22A)
	static const Region streams_o187[] = { { 0xFA94A8, 0x4000 }, { 0xE41E50, 0xC }, { 0xE41E98, 4 }, { 0x209FAB8, 0x40000 } };
	// Doomtrain: pool pointer cells, summon data (loads 0x22E..0x231), shadow-camera scratch
	static const Region streams_d191[] = { { 0xE3C8C0, 0x18 }, { 0x209FAB8, 0x40000 }, { 0x21DFED0, 0x20 } };
	// Gilgamesh: the 16 sword model files in .data (their skeletons are rebuilt), summon data
	// (loads 0x2EE/0x2F0/0x2F1), pool pointer cells
	static const Region streams_g327[] = { { 0xE808D0, 0x10C50 }, { 0x209FAB8, 0x40000 }, { 0xCD0398, 0xC } };
	// Odin reverse: loads 0x2E3/0x2E8 into exe data + the ripple texture right after (0xEECC2C..0xEF4C30),
	// pool/scratch pointer cells, summon data (loads 0x2E5/0x2E7), the camera script's saved camera
	static const Region streams_o326[] = { { 0xEECC2C, 0x8004 }, { 0xE19668, 0xC }, { 0xE196B0, 4 }, { 0x209FAB8, 0x40000 }, { 0x24FD250, 0x110 } };
	// GF cinematic engine (Ifrit family): mesh depth scale, fog words, camera-related word
	static const Region streams_gfc[] = { { 0x1877DA8, 4 }, { 0x209AB64, 0x10 }, { 0xC78BF0, 0x10 }, { 0x1D96DC4, 4 } };
	// actor family: module scratch stack + camera copy (0x2793DA4..0x2793E78), ParsePolygons temporaries,
	// static cells the modules write, Siren's load destinations outside the 1 MB buffer
	static const Region streams_a095[] = { { 0x2793DA4, 0xD4 }, { 0x153386C, 0x154 }, { 0x16A40EC, 0x1284 }, { 0x2795110, 0x68 }, { 0x19D1018, 0x23600 }, { 0x1D99C18, 8 } };
	static const Region streams_a096[] = { { 0x2793DA4, 0xD4 }, { 0x152BBBC, 0x60E }, { 0x169227C, 0xA4 }, { 0x1696140, 0xBCA4 }, { 0x1D99C18, 8 } };
	// Shiva: glow-ring node cell, summon data (CharacterLoad 0x221/0x222/0x224 destination, read by
	// BdTransSummonStream), TransformCameraByShadowRotation scratch
	static const Region streams_s185[] = { { 0xD32508, 4 }, { 0x209FAB8, 0x40000 }, { 0x21DFED0, 0x20 } };

	const Module modules[] = {
		// timeline-A (own pause flag, creature spawned by the master at counter 2)
		{ 116, 0x25216D8, 0x25217D0, 0, 0, true, "Quezacotl", streams_q116, 2 },
		{ 325, 0x250517C, 0x2505230, 0, 0, false, "Diablos" },
		{ 278, 0x2508110, 0x25081FC, 0, 0, false, "Carbuncle" },
		{ 291, 0x2556258, 0x25562F8, 0, 0, false, "Pandemona", streams_p291, 1 },
		{ 140, 0x2517AA0, 0x2517B50, 0, 0, false, "Phoenix" },
		{ 338, 0x25561C8, 0x2556254, 0, 0, false, "Moomba" },
		{ 69,  0x2556628, 0x2556F98, 0, 0, false, "Griever" },
		// timeline-B (draw-only mode on battle_to_update_flags bit0, creature spawned by the timeline)
		{ 185, 0x22BC128, 0x22BD108, 0, 0, false, "Shiva", streams_s185, 3 },
		{ 199, 0x2259950, 0x225A8E4, 0xCF3A68, 4, false, "Cactuar", streams_c199, 1 }, // + its private rand seed
		{ 187, 0x24FD458, 0x24FE910, 0, 0, false, "Odin", streams_o187, 4 },
		{ 326, 0x24F0BD0, 0x24F2308, 0, 0, false, "Odin (reverse)", streams_o326, 5 },
		{ 191, 0x24FBD68, 0x24FD458, 0, 0, false, "Doomtrain", streams_d191, 3 },
		{ 327, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Zantetsuken)", streams_g327, 3 },
		{ 328, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Masamune)", streams_g327, 3 },
		{ 329, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Excalibur)", streams_g327, 3 },
		{ 330, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Excalipoor)", streams_g327, 3 },
		// GF cinematic engine family (engine state block 0x2796E00..0x2798C40)
		{ 201, 0x2796E00, 0x2798C40, 0, 0, false, "Ifrit", streams_gfc, 4 },
		// the six other compilations: same engine block, plus each clone's queue/file pointer statics below it
		{ 6,   0x2796EF8, 0x2798C40, 0, 0, false, "Leviathan", streams_gfc, 4 },
		{ 202, 0x2796DE0, 0x2798C40, 0, 0, false, "Bahamut", streams_gfc, 4 },
		{ 203, 0x2796DA8, 0x2798C40, 0, 0, false, "Cerberus", streams_gfc, 4 },
		{ 204, 0x2796D70, 0x2798C40, 0, 0, false, "Alexander", streams_gfc, 4 },
		{ 205, 0x2796D30, 0x2798C40, 0, 0, false, "Brothers", streams_gfc, 4 },
		{ 206, 0x2796CF8, 0x2798C40, 0, 0, false, "Eden", streams_gfc, 4 },
		// actor state-machine family (the heal-spell effect library)
		{ 95,  0x257F8A0, 0x258FCF4, 0, 0, false, "Siren", streams_a095, 6 },
		{ 96,  0x257B740, 0x257F8A0, 0, 0, false, "MiniMog", streams_a096, 5 },
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
		{ 0x1CA9230, 0xD0, "GTE control registers" },
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
		{ 0x505EB0, 0, "QueueBlitCommand" },
		{ 0x46BD40, 0, "SdStreamingVolumeTranslation" },
		{ 0x47CF50, 0, "Battle_RequestScreenFeedback" },
		// actor family (Siren, MiniMog, Tonberry, Boko): music volume fades, conditional hit reaction
		{ 0x46BB40, 0, "Music_SetVolumeImmediate" },
		{ 0x505CE0, 0, "QueueChainTransformationConditional" },
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
