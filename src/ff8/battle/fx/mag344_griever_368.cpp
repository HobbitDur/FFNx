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
// Effect 344: enemy attack 368 of kernel.bin (no name), used by {Griever} c0m122 only (MAG_344_*, module code
// 0x62A650..0x62A7E0). It draws nothing: a 36-tick script of sound / camera / damage cues.
//
// Structure (setup MAG_344_UNKNOWN 0x62A650 = effect table index 343, not ported; the file loader of that table entry is
// the empty 0x575580: no file): the setup keeps the cast context (0x24E0BC0), the caster slot (0x24E0BC4) and the first
// target's slot (0x24DFD70, never read again), starts the root task in its 1-node pool 0x24DFD78 (queue 0x24DFD88,
// returned) and the cue task in the pool 0x24DFDB0 (0x64 x 0x24, queue 0x24DFDA0), both counters 0.
//   RootTask (0x62A7A0) - points the module packet cursor 0x24F0BC8 at the arena 0x24E8BC8 on odd counters, 0x24E0BC8 on
//     even ones (nothing is ever drawn into them), runs the cue queue, ends when it is empty.
//   CueTask (0x62A6D0) - does nothing while the battle only draws (battle_to_update_flags 0x1D96A9C & 0x201). Counter 0:
//     music volume transition (Music_SetVolumeTrans 0x46BBC0(0, 0x78, 0)); 0x1E: hands the exe block 0xDEE388 to
//     BS_R0win_QueueVictoryFanfare 0x501A20 with the done byte 0x24DFD98; 0x1F: waits (counter kept) while that byte is
//     0; 0x20: camera animation 0xE106D8 (Battle_PlayCameraAnimation 0x5099A0) and the caster's chain transformation 0x14
//     (QueueChainTransformation 0x505C00); 0x22: music channel volume command (0x501B10(0, 0x7F)) and the action result
//     of the first target record of action 0 (ApplyActionResultToTarget 0x506690). Ends after counter 0x23.
// ENTITY / CAMERA WRITES: none by the module (the camera script and the chain transformation are the engine's).
// Module globals: 0x24DFD70..0x24F0BCC (first target slot 0x24DFD70, root pool 0x24DFD78 (1 x 0x10), root queue 0x24DFD88,
// done byte 0x24DFD98, cue queue 0x24DFDA0, cue pool 0x24DFDB0 (0x64 x 0x24), context 0x24E0BC0, caster slot 0x24E0BC4,
// packet arenas 0x24E0BC8 / 0x24E8BC8 (0x8000 each), packet cursor 0x24F0BC8).
// Fujin's described effect 344 (effect_344.json) runs this code: its memory through own::R() (fx_port.h), its spawns
// through own::Fn(), the setup ported below (it has no file); its parameters (Griever368Params: the cue ticks, the music
// commands' arguments, the caster's chain transformation and the camera track). It draws nothing: no part is tagged.

#include "mag_common.h"
#include "fx_params.h"
#include <stddef.h>

namespace ff8fx
{
namespace griever344
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x24E0BC0)); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(own::R(0x24E0BC4)); }
	inline uint8_t &FanfareDone() { return var<uint8_t>(own::R(0x24DFD98)); }      // done byte of 0x501A20
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x24F0BC8)); }
	inline TaskQueue *QCue() { return (TaskQueue *)own::R(0x24DFDA0); }              // pool 0x24DFDB0, 0x64 x 0x24
	inline uint32_t &FirstTarget() { return var<uint32_t>(own::R(0x24DFD70)); }       // (never read again)
	inline TaskQueue *QRoot() { return (TaskQueue *)own::R(0x24DFD88); }             // pool 0x24DFD78, 1 x 0x10

	static const uint32_t ORIG_RootTask = 0x62A7A0;
	static const uint32_t ORIG_CueTask = 0x62A6D0;
	static const uint32_t ARENA_A = 0x24E8BC8;       // packet arenas (odd / even counter)
	static const uint32_t ARENA_B = 0x24E0BC8;
	static const uint32_t FANFARE_DATA = 0xDEE388;   // exe block handed to 0x501A20
	static const uint32_t CAMERA_ANIM = 0xE106D8;    // camera animation track
	static const uint32_t ORIG_Setup = 0x62A650;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	struct Griever368Params
	{
		struct { int16_t volume, fanfare, wait, camera, channel, damage, end; } cues;   // the cue task's ticks
		struct { int32_t a, b, c; } volume;     // Music_SetVolumeTrans 0x46BBC0's arguments
		struct { int32_t a, b; } channel;       // BS_Music_SetChannelVolumeCommand 0x501B10's
		int32_t chain;                          // the caster's chain transformation (QueueChainTransformation 0x505C00)
		struct { uint32_t camera; } res;        // its resources (data): the camera track
	};
	static const Griever368Params GAME = { { 0, 0x1E, 0x1F, 0x20, 0x22, 0x22, 0x23 }, { 0, 0x78, 0 }, { 0, 0x7F }, 0x14, { CAMERA_ANIM } };
	static Griever368Params live = GAME;
	static const Griever368Params &P() { return live; }

	// engine functions not in fx_port.h / mag_common.h
	inline void Music_SetVolumeTrans(uint32_t a1, uint32_t a2, uint32_t a3) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x46BBC0)(a1, a2, a3); }
	inline void QueueChainTransformation(uint8_t *entity, uint32_t chain) { fn<void (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, chain); }
	inline void BS_R0win_QueueVictoryFanfare(uint32_t data, uint8_t *done) { fn<void (__cdecl *)(uint32_t, uint8_t *)>(0x501A20)(data, done); }
	inline void BS_Music_SetChannelVolumeCommand(uint32_t a1, uint32_t a2) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x501B10)(a1, a2); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes (0x24DFD78)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint16_t pad0E;
	};
	struct CueNode // pool of 0x64 nodes of 0x24 bytes (0x24DFDB0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E[0x16];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(CueNode) == 0x24, "344 nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag344_griever_368_held.h"
#endif

namespace ff8fx
{
namespace griever344
{
	// ------------------------------------------------------------------
	// Cue task (0x62A6D0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl CueTask(TaskNode *n)
	{
		CueNode *c = (CueNode *)n;
		if (UpdateFlags() & 0x201) return 0;
		if (c->counter == P().cues.volume) Music_SetVolumeTrans((uint32_t)P().volume.a, (uint32_t)P().volume.b, (uint32_t)P().volume.c);
		if (c->counter == P().cues.fanfare) BS_R0win_QueueVictoryFanfare(FANFARE_DATA, &FanfareDone());
		if (FanfareDone() == 0 && c->counter == P().cues.wait) return 0;
		if (c->counter == P().cues.camera)
		{
			fn<void (__cdecl *)(const void *)>(0x5099A0)((const void *)fxp::camera(P().res.camera, CAMERA_ANIM)); // Battle_PlayCameraAnimation
			QueueChainTransformation(Entity(CasterSlot()), (uint32_t)P().chain);
		}
		if (c->counter == P().cues.channel) BS_Music_SetChannelVolumeCommand((uint32_t)P().channel.a, (uint32_t)P().channel.b);
		if (c->counter == P().cues.damage) ApplyActionResultToTarget(Ctx()->actions->targets);
		c->counter++;
		return c->counter > P().cues.end ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Setup (0x62A650, the effect table's entry 343; its file loader is the engine's empty 0x575580): the game runs its own;
	// Fujin's programs this (its memory may be a copy's): the cast, its caster's and first target's slots, the root task in
	// its pool, the cue task in its pool
	// ------------------------------------------------------------------
	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		Ctx() = ctx;
		FirstTarget() = ctx->actions->targets[0];
		CasterSlot() = ctx->attacker;
		InitTaskQueuePool(QRoot(), (void *)own::R(0x24DFD78), 0x10, 1);
		((RootNode *)AddTaskToQueue(QRoot(), own::Fn(ORIG_RootTask)))->counter = 0;
		InitTaskQueuePool(QCue(), (void *)own::R(0x24DFDB0), 0x24, 0x64);
		((CueNode *)AddTaskToQueue(QCue(), own::Fn(ORIG_CueTask)))->counter = 0;
		return QRoot();
	}

	// ------------------------------------------------------------------
	// Root task (0x62A7A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag344_griever_368_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = own::R(ARENA_A);
		if (!(r->counter & 1)) PacketCursor() = own::R(ARENA_B);
		const int left = ExecuteTaskQueue(QCue());
		r->counter++;
		return left ? 0 : TASK_END;
	}
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "cues", "Cues", "It draws nothing: a cue task's script - the music's volume transition at its first tick, the final battle music handed to the victory fanfare player (an exe block: a sample bank and an AKAO score, song id 0x5B), a wait for it, the camera track and the caster's chain transformation, the music channel volume command and the damage (the first target of action 0), the end" },
		};
		static const Field FIELDS[] = {
			{ "cues", "cues.volume", "Volume tick", "The music's volume transition (Music_SetVolumeTrans) at this tick of the cue task", (uint32_t)offsetof(Griever368Params, cues.volume), fxp::I16, -1, 300 },
			{ "cues", "cues.fanfare", "Fanfare tick", "The fanfare block handed to the victory fanfare player (BS_R0win_QueueVictoryFanfare) at this tick", (uint32_t)offsetof(Griever368Params, cues.fanfare), fxp::I16, -1, 300 },
			{ "cues", "cues.wait", "Wait tick", "At this tick the cue task waits (its counter kept) until the fanfare player is done", (uint32_t)offsetof(Griever368Params, cues.wait), fxp::I16, -1, 300 },
			{ "cues", "cues.camera", "Camera tick", "The camera track and the caster's chain transformation at this tick", (uint32_t)offsetof(Griever368Params, cues.camera), fxp::I16, -1, 300 },
			{ "cues", "cues.channel", "Channel volume tick", "The music channel volume command (BS_Music_SetChannelVolumeCommand) at this tick", (uint32_t)offsetof(Griever368Params, cues.channel), fxp::I16, -1, 300 },
			{ "cues", "cues.damage", "Damage tick", "The action result of action 0's first target at this tick", (uint32_t)offsetof(Griever368Params, cues.damage), fxp::I16, -1, 300 },
			{ "cues", "cues.end", "End tick", "The cue task's last tick (the effect ends with it)", (uint32_t)offsetof(Griever368Params, cues.end), fxp::I16, 0, 300 },
			{ "cues", "volume.a", "Volume: first", "Music_SetVolumeTrans 0x46BBC0's first argument", (uint32_t)offsetof(Griever368Params, volume.a), fxp::I32, 0, 0xFFFF },
			{ "cues", "volume.b", "Volume: second", "Its second argument (the game's 120)", (uint32_t)offsetof(Griever368Params, volume.b), fxp::I32, 0, 0xFFFF },
			{ "cues", "volume.c", "Volume: third", "Its third argument", (uint32_t)offsetof(Griever368Params, volume.c), fxp::I32, 0, 0xFFFF },
			{ "cues", "channel.a", "Channel: first", "BS_Music_SetChannelVolumeCommand 0x501B10's first argument", (uint32_t)offsetof(Griever368Params, channel.a), fxp::I32, 0, 0xFFFF },
			{ "cues", "channel.b", "Channel: volume", "Its second argument: the volume (the game's 127)", (uint32_t)offsetof(Griever368Params, channel.b), fxp::I32, 0, 0xFFFF },
			{ "cues", "chain", "Chain transformation", "The caster's chain transformation (its animation sequence) queued at the camera tick", (uint32_t)offsetof(Griever368Params, chain), fxp::I32, 0, 255 },
			{ "cues", "res.camera", "Camera track", "The camera track played at the camera tick (any length: its offsets are its own)", (uint32_t)offsetof(Griever368Params, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag344_griever_368()
	{
		fxp::register_params({ 344, "Final Battle Music", &griever344::live, &griever344::GAME, sizeof(griever344::Griever368Params),
		                       griever344::table::PARTS, (int)(sizeof(griever344::table::PARTS) / sizeof(griever344::table::PARTS[0])),
		                       griever344::table::FIELDS, (int)(sizeof(griever344::table::FIELDS) / sizeof(griever344::table::FIELDS[0])),
		                       nullptr, 0 });
		own::note(griever344::ORIG_Setup, (void *)griever344::Setup);
		own::note(griever344::ORIG_RootTask, (void *)griever344::RootTask);
		register_port(griever344::ORIG_RootTask, (void *)griever344::RootTask, "344 RootTask", 344);
		own::note(griever344::ORIG_CueTask, (void *)griever344::CueTask);
		register_port(griever344::ORIG_CueTask, (void *)griever344::CueTask, "344 CueTask", 344);
		// 30 fps layer: see mag344_griever_368_held.inc
		FX_HELD(register_mag344_held();)
	}
	FX_LAB_MODULE(register_mag344_griever_368);
}

#ifdef FF8_FX_HELD
#include "mag344_griever_368_held.inc"
#endif
