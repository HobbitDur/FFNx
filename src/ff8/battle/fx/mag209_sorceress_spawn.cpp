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

// Effect 209: "Sorceress" spawn (enemy attacks 319 / 327 of the time-compression sorceresses c0m115 / c0m116 /
// c0m117; MAG_209_*): the battle stage is swapped for the next one while the stage, rendered into VRAM, ripples
// on a warped screen grid.
//
// Structure (file loader 0x575520 = a bare ret: no file; setup 0x637500, not ported: it runs once):
//   setup - packet arena = Magic_TextureOFF_ToEAX1 (0x24F7A5C), summon data 0x209FAB8 (0x24F4484, the grid packet
//     table); cast context 0x24F4478, caster slot 0x24F4480, first target slot 0x24F361C; root queue 0x24F3640
//     (pool 0x24F3620, 2 x 0x10) with the master, sub-queue 0x24F3658 (pool 0x24F3668, 100 x 0x24) with timeline A
//     (0x6375D0), or B (0x638BD0) for the wormlike sorceress (caster entity +4 == 0x85); camera animation 0xE25AF0,
//     TIM 0xF47340 queued.
//   Master (0x639100) - packet cursor 0x24F7A60 = arena (+0x10000 on odd counters, not read), runs the sub-queue;
//     when it is empty the end task 0x639170 (battle textures back: 0x508630) goes into the root queue.
//   Timelines A / B (130 ticks): stage swap (2: load, 52: BS_ChangeStage, waits for the load), the screen task
//     (16), caster flag 8 (0 .. 20 / 65), prims (1: 0x638490, 12 x 0x6385E0, A: 0x638770; then 0x638900 /
//     0x638A80 series), screen flash in and out, B: screen fades, camera shake (0x1D97712), its voice slot.
//   Screen task (0x637A40, 101 ticks): 0-35 the VRAM copy as tiles (0x6383B0); 36-37 + flash; 38-81 the stage
//     on a rippling grid (0x637F40 / 0x6380F0 / 0x638250); 82-97 the grid fading to the new stage colours;
//     the stage is rendered into VRAM (0x240, 0x100) every tick (0x637D30) except 36..81; ambient colours.
// Every task but the master / end task tests battle_to_update_flags_dword_1D96A9C & 0x201 (draw-only when set);
// the timelines return at once on bit 0, and with bit 0x200 only while a file load is pending.
// Renderers of other modules called through their original addresses: MAG_166_sub_64F0A0 (prim model with two
// draw-mode words), MAG_066_RenderScrollPrimModel 0x650720.
// FUJIN: Fujin's described "Sorceress" spawn (effect_209.json) runs this code: its memory through own::R() (fx_port.h),
// its spawns through own::Fn(), the setup (0x637500) ported below (its file loader is the engine's bare ret); its
// parameters (SorceressSpawnParams: the sounds and the timelines' ticks, the screen flash, the screen task's phases, the
// ripple, the glow, the bursts, the swirl, the rising and the closing rings; the TIM, the camera track, the meshes, the
// screen's and the swirl's colour tables). The grid stays code: its 28 x 41 points (the module's words) and 27 x 40 cells
// (corner pointers and two GPU quads each, built in the summon data by GridInit every run) are work memory whose size is
// the screen's (8 pixels a cell over 320 x 216). FX_PART tags its parts.

#include "fx_port.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace s209
{
	using namespace eng;

	// --- engine functions used by this module (original addresses) ---
	namespace x
	{
		inline int32_t CrtRand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); }
		inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
		inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); } // Math_SqrtInt
		inline void ComposeZYXRotationMatrix(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x56CE30)(angles, out); }
		inline void Scale3DMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); }
		inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
		inline void GetDefaultEffectPosition(void *entity, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x571400)(entity, out); }
		inline void SetProjection(int32_t h) { fn<void (__cdecl *)(int32_t)>(0x56CD00)(h); } // Battle_SetProjectionH
		inline void SetScreenFlash2(int32_t level, int32_t mode) { fn<void (__cdecl *)(int32_t, int32_t)>(0x5713E0)(level, mode); } // Effect_SetScreenFlash
		inline void ScreenFade(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x5712C0)(a, b, c, d); } // Effect_SpawnScreenFadeTask
		inline uint32_t RenderGeometry(void *geom, void *hdr, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(geom, hdr, ot, shift, cursor); }
		// sound / stage / files / battle
		inline void BdPlaySE(uint32_t se, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(0x501330)(se, a, b); }
		inline uint32_t ClaimVoiceSlot2(uint32_t sound, int32_t a, int32_t b) { return fn<uint32_t (__cdecl *)(uint32_t, int32_t, int32_t)>(0x4A29A0)(sound, a, b); } // BdSound_ClaimVoiceSlot
		inline int32_t VoiceSlotBusy(uint32_t slot) { return fn<int32_t (__cdecl *)(uint32_t)>(0x4A2900)(slot); }
		inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
		inline int32_t LoadState() { return fn<int32_t (__cdecl *)()>(0x508500)(); }                       // sub_508500: < 0 = file load busy
		inline uint32_t SummonData() { return fn<uint32_t (__cdecl *)()>(0x571B70)(); }                   // sub_571B70: 0x209FAB8
		inline void WitchGetNewStage(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x50E3A0)(dst); }     // BS_WitchFight_GetNewStage
		inline void ChangeStage(uint32_t src) { fn<void (__cdecl *)(uint32_t)>(0x50E3C0)(src); }          // BS_ChangeStage
		inline void PlayCameraAnimation(const void *track) { fn<void (__cdecl *)(const void *)>(0x5099A0)(track); }   // Battle_PlayCameraAnimation
		inline uint32_t QueueTimUpload(uint32_t tim) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x505E30)(tim); }     // Battle_QueueTIMUpload_GetEOF
		inline void ApplyActionResultToTarget(uint32_t rec) { fn<void (__cdecl *)(uint32_t)>(0x506690)(rec); }
		inline void RestoreTexture(uint32_t buffer, void *flag) { fn<void (__cdecl *)(uint32_t, void *)>(0x508630)(buffer, flag); } // Battle_RestoreDefaultEffectTexture
		// GPU / GTE
		inline void ClearImage(const int16_t *rect, int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(const int16_t *, int32_t, int32_t, int32_t)>(0x45BEE0)(rect, r, g, b); } // isDravingOverlayImage
		inline void GetDrawEnv(void *env) { fn<void (__cdecl *)(void *)>(0x45B640)(env); }               // GPU_GetDrawEnv
		inline void SetDefDrawEnv(void *env, int32_t x, int32_t y, int32_t w, int32_t h) { fn<void (__cdecl *)(void *, int32_t, int32_t, int32_t, int32_t)>(0x45C2A0)(env, x, y, w, h); }
		inline void SetDrawEnvPacket(void *packet, const void *env) { fn<void (__cdecl *)(void *, const void *)>(0x45C0F0)(packet, env); } // SSIGPU_SetDrawEnv
		inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
		inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); }
		inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
		inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
		inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
		// renderers of other modules
		inline uint32_t PrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x64F0A0)(h, ot, mode, cursor); } // MAG_166_sub_64F0A0
		inline uint32_t ScrollPrimModel(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x650720)(h, ot, mode, cursor); } // MAG_066_RenderScrollPrimModel
	}
	using namespace x;

	// --- module globals ---
	inline bool Paused() { return (var<uint32_t>(0x1D96A9C) & 0x201) != 0; } // battle_to_update_flags_dword_1D96A9C
	inline TaskQueue &RootQueue() { return var<TaskQueue>(own::R(0x24F3640)); }
	inline TaskQueue &SubQueue() { return var<TaskQueue>(own::R(0x24F3658)); }
	inline uint8_t *CastCtx() { return var<uint8_t *>(own::R(0x24F4478)); }
	inline uint32_t CasterSlot() { return var<uint32_t>(own::R(0x24F4480)); }
	inline uint8_t *Caster() { return (uint8_t *)(0x1D972C0 + 156 * CasterSlot()); }
	inline uint32_t &Cursor() { return var<uint32_t>(0x1D8E054); }   // battle_texture_data_ptr_1D8E054
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }
	static const uint32_t GRID = 0x24F4488;      // 28 rows x 41 columns of 0xC-byte grid points (to 0x24F7A58)
	static const uint32_t GRID_END = 0x24F7A58;
	inline uint8_t *Cells() { return var<uint8_t *>(own::R(0x24F4484)); }    // 27 x 40 cells of 0x60 bytes (summon data)
	inline uint32_t &ColA() { return var<uint32_t>(own::R(0x24F7A58)); }     // 0xB8B7D8 (background colour) at the start
	inline uint32_t &ColB() { return var<uint32_t>(own::R(0x24F3650)); }     // 0xB8B9A8 (ambient colour) at the start
	inline uint32_t &StageColA() { return var<uint32_t>(own::R(0x24F3618)); } // the same after the stage swap
	inline uint32_t &StageColB() { return var<uint32_t>(own::R(0x24F447C)); }

	static const uint32_t ORIG_Master = 0x639100;
	static const uint32_t ORIG_EndTask = 0x639170;
	static const uint32_t ORIG_TimelineA = 0x6375D0;
	static const uint32_t ORIG_TimelineB = 0x638BD0;
	static const uint32_t ORIG_ScreenTask = 0x637A40;
	static const uint32_t ORIG_PrimATask = 0x638490;
	static const uint32_t ORIG_PrimBTask = 0x6385E0;
	static const uint32_t ORIG_PrimCTask = 0x638770;
	static const uint32_t ORIG_PrimDTask = 0x638900;
	static const uint32_t ORIG_PrimETask = 0x638A80;
	static const uint32_t ORIG_Setup = 0x637500;
	static const uint32_t TIM = 0xF47340, CAMERA_ANIM = 0xE25AF0;
	static const uint32_t MODEL_Glow = 0xE288F0, MODEL_Swirl = 0xE26340, MODEL_Rise = 0xE277B8, MODEL_Ring = 0xE290E0;
	static const uint32_t BURSTS = 0xE2B400;                 // the 12 burst meshes' table
	static const uint32_t COLOURS_IN = 0xE2B430, COLOURS = 0xE2B450, SWIRL_COLOURS = 0xE2B4B0;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_SCREEN = 1, PART_GLOW = 2, PART_BURSTS = 3, PART_SWIRL = 4, PART_RISE = 5, PART_RINGS = 6 };
	struct SorceressSpawnParams
	{
		uint32_t sound_a, sound_b;  // the timelines' sound (A: c0m115 / c0m116, B: the wormlike c0m117)
		// the timelines' ticks: the sound, the stage file's load, the stage swap, the screen task, the glow / bursts / swirl, the
		// action result, the end; the caster hidden from tick 0 to hide_a / hide_b
		struct { int16_t sound, load, swap, screen, start, result, end, hide_a, hide_b; } timing;
		struct { int16_t in, in_step, out, out_step; } flash; // the screen flash rising over its first ticks, falling from out
		// the screen task's phases: the VRAM copy's tiles until tiles, white until flash, the ripple until fade_from, the grid
		// fading to the new stage's colours until fade_to, the end at ticks
		struct { int16_t tiles, flash, fade_from, fade_to, ticks; } screen;
		// the ripple around the caster's head on screen: its steps, the wave's number and speed, its height, reach and shrink
		struct { int16_t steps, wave, speed, amp, range, shrink; } ripple;
		struct { int16_t size, grow, out, life; } glow;     // the glow mesh at the caster: size (and growth), grow ticks, fade out from, life
		// the bursts: count, spin, size (and growth), the second scale (/ 6: and its growth) + a random below size2_rand, their
		// grow ticks, life (B's: life_b)
		struct { int16_t count, spin, size, size2, size2_rand, grow, grow2, life, life_b; } bursts;
		struct { int16_t size, scroll, fade, out, out_step, life; } swirl;  // A's scrolling swirl at the caster's head height
		// the rising rings at the caster's head (+ a random height): A from first_a every every_a ticks to last_a, B's; size
		// (/ 8: and growth) + a random below size_rand, life
		struct { int16_t first_a, every_a, last_a, first_b, every_b, last_b, height, size, size_rand, life; } rise;
		// the closing rings at the caster's feet: A's tick, B's from first_b every every_b to last_b; size (/ 12, B's x 3)
		struct { int16_t tick_a, first_b, every_b, last_b, size, size_rand, life; } rings;
		struct { uint32_t glow, swirl, rise, ring, bursts[12]; } models;  // the meshes (prim models)
		struct { uint32_t colours_in, colours, swirl; } tables; // the screen's colours (8, then a 24-colour cycle), the swirl's 18
		struct { uint32_t tim, camera; } res;               // its resources (data): the TIM its setup uploads, the camera track
	};
	static const SorceressSpawnParams GAME = { 0xE2B3F4, 0xE2B3F8,
		{ 1, 2, 0x34, 0x10, 1, 0x7C, 0x82, 0x14, 0x41 }, { 0x10, 96, 0x7A, 192 }, { 0x24, 0x26, 0x52, 0x62, 0x64 },
		{ 0x3C, 96, 385, 500, 600, 9 }, { 0x3AA, 0x10, 0x66, 0x6E }, { 12, 0x1E, 0x100, 0xC00, 0x400, 0x10, 0x20, 0x6E, 0x37 },
		{ 0x1800, 8, 0x600, 0x66, 320, 0x6E }, { 0x1F, 10, 0x64, 0x49, 6, 0x5A, 0x258, 0xE00, 0x800, 0x18 },
		{ 0x73, 0x60, 6, 0x6E, 0x2000, 0x400, 0x14 },
		{ MODEL_Glow, MODEL_Swirl, MODEL_Rise, MODEL_Ring, { 0xE29588, 0xE297F8, 0xE29A68, 0xE29CD8, 0xE29F48, 0xE2A1B8, 0xE2A428,
		  0xE2A698, 0xE2A908, 0xE2AB78, 0xE2ADE8, 0xE2B058 } }, { COLOURS_IN, COLOURS, SWIRL_COLOURS }, { TIM, CAMERA_ANIM } };
	static SorceressSpawnParams live = GAME;
	static const SorceressSpawnParams &PG() { return live; }
	// a tick of a spawn series: from first every `every` ticks to last
	inline bool SeriesTick(int32_t c, int32_t first, int32_t every, int32_t last) { return c >= first && c <= last && (c - first) % every == 0; }
	// the screen task's first grid tick: past the tiles and the white (the grid made then; the game's: 0x26)
	inline int32_t GridStart() { return PG().screen.tiles > PG().screen.flash ? PG().screen.tiles : PG().screen.flash; }

	// every sub-queue node is 0x24 bytes (pool 0x24F3668); field use differs per task
#pragma pack(push, 1)
	struct Node24
	{
		TaskNode hdr;
		int16_t c;                  // +0x0C tick counter
		int16_t e;                  // +0x0E index / end task done flag
		int16_t p10, p12, p14, p16; // +0x10 position (p16: B prims' short-life flag)
		int16_t a18, s1A;           // +0x18 angle, its speed
		int16_t s1C, s1E;           // +0x1C scale, its speed
		int16_t s20, s22;           // +0x20 second scale, its speed
	};
#pragma pack(pop)
	static_assert(sizeof(Node24) == 0x24, "sub-queue node is 0x24 bytes");
}
}

#ifdef FF8_FX_HELD
#include "mag209_sorceress_spawn_held.h"
#endif

namespace ff8fx
{
namespace s209
{
	// ------------------------------------------------------------------
	// Master (0x639100) and end task (0x639170)
	// ------------------------------------------------------------------
	static uint32_t __cdecl Master(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint32_t arena = var<uint32_t>(own::R(0x24F7A5C));
		var<uint32_t>(own::R(0x24F7A60)) = (*(const uint8_t *)&n->c & 1) ? arena + 0x10000 : arena;
		// 30 fps layer: see mag209_sorceress_spawn_held.inc
		FX_HELD(held_note_master();)
		int left = ExecuteTaskQueue(&SubQueue());
		if (!left)
		{
			Node24 *e = (Node24 *)AddTaskToQueue(&RootQueue(), own::Fn(ORIG_EndTask));
			e->c = 0;
			e->e = 0;
		}
		n->c++;
		return left ? 0 : TASK_END;
	}

	static uint32_t __cdecl EndTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 1) RestoreTexture(var<uint32_t>(own::R(0x24F7A5C)), &n->e);
		int16_t done = n->e;
		n->c++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------
	// MAG_209_sub_637CC0 / 637CE0: the battle stage groups stru_1D9898C[0..3] +5 bit 1 (hide / show the stage)
	static void HideStage() { for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) |= 2; }
	static void ShowStage() { for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) &= 0xFD; }

	// MAG_209_sub_637D00 (= MAG_076_SetAmbientColour): ambient colour 0xB8B9A8 and every shown entity's colour
	static void SetAmbient(uint32_t c)
	{
		var<uint32_t>(0xB8B9A8) = c;
		for (uint32_t a = 0x1D972EC; a < 0x1D97730; a += 0x9C)
			if (var<uint8_t>(a - 0x2C) & 2) var<uint32_t>(a) = c;
	}

	// MAG_209_sub_638370 (= GF_327_ResetRotation): rotation part = identity (translation and pad kept)
	static void ResetRotation(void *m)
	{
		uint32_t *d = (uint32_t *)m;
		d[3] = 0;
		d[2] = 0;
		d[1] = 0;
		d[0] = 0;
		uint16_t *w = (uint16_t *)m;
		w[8] = 0x1000;
		w[4] = 0x1000;
		w[0] = 0x1000;
	}

	// MAG_209_sub_637E90: a draw-environment packet at the OT bucket ot: drawing area (x, y) 0x140 x 0xD8 (the
	// current environment's texture page / dither / draw-on-display), background cleared to black
	static void DrawEnvAt(int32_t x, int32_t y, int32_t ot)
	{
		uint8_t env[0x5C], cur[0x5C];
		GetDrawEnv(cur);
		uint8_t *pk = (uint8_t *)Cursor();
		Cursor() = (uint32_t)pk + 0x40;
		SetDefDrawEnv(env, x, y, 0x140, 0xD8);
		*(uint16_t *)(env + 0x14) = *(const uint16_t *)(cur + 0x14);
		env[0x17] = cur[0x17];
		env[0x16] = cur[0x16];
		env[0x18] = 1;
		env[0x1B] = 0;
		env[0x1A] = 0;
		env[0x19] = 0;
		SetDrawEnvPacket(pk, env);
		InsertPrimAutoDepth(OTBase() + 4 * (uint32_t)ot, pk);
	}

	// MAG_209_sub_637D30: the battle stage rendered into VRAM (0x240, 0x100) (cleared first): every stage group's bone
	// matrices rebuilt, the shown ones drawn between a draw environment at (0x240, 0x100) (OT 0x1121) and one back at
	// (0, 0) (the lowest OT bucket of the shown groups, table 0xE2B4F8)
	static void StageToVram()
	{
		int16_t rect[4] = { 0x240, 0x100, 0x140, 0xD8 };
		ClearImage(rect, 0, 0, 0);
		int32_t lo = 0x1121;
		uint8_t *h = (uint8_t *)FieldAlloc(0x2C);
		*(int16_t *)(h + 0x14) = 0;
		*(int16_t *)(h + 0x16) = 0;
		*(int16_t *)(h + 0x1A) = 0xD8;
		*(uint32_t *)(h + 0x10) = var<uint32_t>(0x1D969A8);
		*(int16_t *)(h + 0x18) = 0x140;
		*(uint32_t *)(h + 4) = var<uint32_t>(0x1D98B3C);
		*(uint32_t *)(h + 0x20) = 0xFFFFFFFF;
		for (int i = 0; i < 4; i++)
		{
			if (!(var<uint8_t>(0x1D98991 + 0x2C * i) & 1)) continue;
			int32_t o = var<int32_t>(0xE2B4F8 + 8 * i);
			if (o < lo) lo = o;
		}
		DrawEnvAt(0, 0, lo);
		for (int i = 0; i < 4; i++)
		{
			uint8_t *p = (uint8_t *)(0x1D989A4 + 0x2C * i);
			const int32_t *tb = (const int32_t *)(0xE2B4F8 + 8 * i);
			BuildBoneMatricesFromPose(p);
			ComputeBonesWorldMatrices(p, &Camera());
			if (!(p[-0x13] & 1)) continue;
			*(uint32_t *)(h + 0x1C) = *(const uint32_t *)(p + 0x14);
			*(uint16_t *)(h + 0x24) = *(const uint16_t *)(p - 0x12);
			Cursor() = RenderGeometry(p - 0x10, h, OTBase() + 4 * (uint32_t)tb[0], tb[1], Cursor());
			if (tb[0] < lo) lo = tb[0];
		}
		DrawEnvAt(0x240, 0x100, 0x1121);
		FieldFree(0x2C);
	}

	// MAG_209_sub_6383B0: the VRAM copy drawn back over the screen: 10 x 7 textured 32 x 32 sprites (colour c,
	// texture pages 0x139 + column / 2), at the far OT bucket 0x4068 through the alternative viewport
	static void Tiles(uint32_t c)
	{
		uint8_t *p = (uint8_t *)Cursor();
		for (int32_t i = 0; i < 10; i++)
		{
			const uint8_t u = (uint8_t)((i & 1) << 5);
			for (int32_t j = 0; j < 7; j++)
			{
				p[0xC] = u;
				*(uint16_t *)(p + 0x12) = 0x20;
				*(uint16_t *)(p + 0x10) = 0x20;
				*(uint32_t *)p = 0x4000000;
				*(uint32_t *)(p + 4) = c;
				*(int16_t *)(p + 8) = (int16_t)(i << 5);
				*(int16_t *)(p + 0xA) = (int16_t)(j << 5);
				p[0xD] = (uint8_t)(j << 5);
				InsertPrimAltViewport(OTBase() + 0x4068, p);
				p += 0x14;
			}
			*(uint32_t *)p = 0x1000000;
			*(uint32_t *)(p + 4) = (((uint32_t)(i / 2 + 0x139)) & 0x9FF) | 0xE1000200;
			InsertPrimAutoDepth(OTBase() + 0x4068, p);
			p += 8;
		}
		Cursor() = (uint32_t)p;
	}

	// MAG_209_sub_637F40: the grid (28 x 41 points 8 apart around (-0xA4, -0x70), flat, edges flagged: rows 1,
	// columns 2) and its 27 x 40 cells in the summon data: corner pointers and two textured quads (frame parity)
	// on the VRAM copy (texture page 0x139 + u / 64, the last column / row one texel short)
	static void GridInit()
	{
		uint8_t *g = (uint8_t *)own::R(GRID);
		for (int32_t row = 0; row < 0x1C; row++)
			for (int32_t col = 0; col < 0x29; col++, g += 0xC)
			{
				*(int16_t *)(g + 4) = (int16_t)(-0xA4 + 8 * col);
				*(int16_t *)(g + 6) = (int16_t)(-0x70 + 8 * row);
				*(int16_t *)(g + 8) = 0;
				*(int16_t *)(g + 0xA) = 0;
				if (row == 0 || row == 0x1B) *(int16_t *)(g + 0xA) = 1;
				if (col == 0 || col == 0x28) *(uint16_t *)(g + 0xA) |= 2;
			}
		uint8_t *cell = Cells();
		uint32_t pt = own::R(GRID);
		int32_t v0 = 0;
		for (int32_t row = 0; row < 0x1B; row++)
		{
			const int32_t v1 = v0 + 8;
			int32_t u = 0;
			for (int32_t col = 0; col < 0x28; col++, cell += 0x60)
			{
				uint8_t *q = cell + 0x2C;
				*(uint32_t *)(cell + 8) = pt + 0x1EC;
				*(uint32_t *)cell = pt;
				*(uint32_t *)(cell + 0xC) = pt + 0x1F8;
				*(uint32_t *)(cell + 4) = pt + 0xC;
				pt += 0xC;
				const int32_t um = u % 64;
				const int32_t u_old = u;
				u += 8;
				const uint16_t tpage = (uint16_t)((u_old / 64 + 0x19) | 0x120);
				*(uint32_t *)(q - 0x1C) = 0x9000000;
				*(uint32_t *)(q - 0x18) = 0x2C808080;
				q[0] = (uint8_t)um;
				q[-0x10] = (uint8_t)um;
				const int32_t ur = u >= 0x140 ? um + 7 : um + 8;
				q[8] = (uint8_t)ur;
				q[-8] = (uint8_t)ur;
				const int32_t vb = v1 >= 0xD8 ? v1 - 1 : v1;
				q[-7] = (uint8_t)v0;
				q[-0xF] = (uint8_t)v0;
				q[9] = (uint8_t)vb;
				q[1] = (uint8_t)vb;
				*(uint16_t *)(q - 6) = tpage;
				*(uint32_t *)(q + 0xC) = 0x9000000;
				*(uint32_t *)(q + 0x10) = 0x2C808080;
				q[0x28] = (uint8_t)um;
				q[0x18] = (uint8_t)um;
				q[0x21] = (uint8_t)v0;
				q[0x19] = (uint8_t)v0;
				q[0x30] = (uint8_t)ur;
				q[0x20] = (uint8_t)ur;
				*(uint16_t *)(q + 0x22) = tpage;
				q[0x31] = (uint8_t)vb;
				q[0x29] = (uint8_t)vb;
			}
			pt += 0xC;
			v0 = v1;
		}
	}

	// MAG_209_sub_6380F0: the grid ripple of step i (of n): around the caster's bone 0xF1 on screen, every inner
	// point's depth = (cos(96 d - 385 i) - 1) x amp x (600 - 9 i - d) / (600 - 9 i) >> 13, amp = 500 sin(i 2048 / n)
	static void GridRipple(int32_t i, int32_t n)
	{
		int16_t *s = (int16_t *)FieldAlloc(0xC);
		GetEffectSpawnPosition(Caster(), 0xF1, 0, s);
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		GteLoadV0(s);
		GteRTPS();
		GteReadSXY2(s + 4);
		s[4] = (int16_t)((int16_t)(s[4] >> 3) - 0xA4);
		s[5] = (int16_t)((int16_t)(s[5] >> 3) - 0x70);
		const SorceressSpawnParams &rp = PG();
		const int32_t phase = rp.ripple.speed * i;
		const int32_t amp = (rp.ripple.amp * ComputeSin(shl32(i, 11) / n)) >> 12;
		const int32_t range = rp.ripple.range - rp.ripple.shrink * i;
		uint8_t *g = (uint8_t *)own::R(GRID);
		for (int32_t k = 0; k < 0x1C * 0x29; k++, g += 0xC)
		{
			if (*(uint8_t *)(g + 0xA) & 3) continue;
			const int32_t dx = *(int16_t *)(g + 4) - s[4];
			const int32_t dy = *(int16_t *)(g + 6) - s[5];
			const int32_t d = Sqrt(dy * dy + dx * dx);
			const int32_t c = ComputeCos(rp.ripple.wave * d - phase) - 0x1000;
			const int32_t f = (range - d) * amp / range;
			*(int16_t *)(g + 8) = (int16_t)((c * f) >> 13);
		}
		FieldFree(0xC);
	}

	// MAG_209_sub_638250: every grid point projected (rotation identity, translation (4, 4, 0x800), GTE H 0x800,
	// screen pixels), then the cells' quads of this frame parity (colour c) at the far OT bucket 0x4068 through the
	// alternative viewport
	static void GridDraw(int32_t parity, uint32_t c)
	{
		Mat4x3 m;
		ResetRotation(&m);
		m.t[0] = 4;
		m.t[1] = 4;
		m.t[2] = 0x800;
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		SetProjection(0x800);
		for (uint32_t g = own::R(GRID); g < own::R(GRID_END); g += 0xC)
		{
			GteLoadV0((const void *)(g + 4));
			GteRTPS();
			GteReadSXY2((void *)g);
			*(int16_t *)g = (int16_t)(*(int16_t *)g >> 3);
			*(int16_t *)(g + 2) = (int16_t)(*(int16_t *)(g + 2) >> 3);
		}
		SetProjection(var<int16_t>(0x1D8E038));
		const uint32_t col = c | 0x2C000000;
		uint8_t *cell = Cells();
		for (int32_t k = 0; k < 0x1B * 0x28; k++, cell += 0x60)
		{
			uint32_t *q = (uint32_t *)(cell + 0x18 + 0x28 * parity);
			const uint32_t *const *pt = (const uint32_t *const *)cell;
			q[-1] = col;
			q[0] = *pt[0];
			q[2] = *pt[1];
			q[4] = *pt[2];
			q[6] = *pt[3];
			InsertPrimAltViewport(OTBase() + 0x4068, q - 2);
		}
	}

	// ------------------------------------------------------------------
	// Screen task (0x637A40, timeline tick 16), 101 ticks; colour of the tick: table 0xE2B430 for 0..7, then
	// 0xE2B450[(c - 8) % 24]
	// ------------------------------------------------------------------
	static uint32_t ScreenColour(int32_t c)
	{
		if (c < 8) return ((const uint32_t *)fxp::data(PG().tables.colours_in, COLOURS_IN, 32, 4))[c];
		return ((const uint32_t *)fxp::data(PG().tables.colours, COLOURS, 96, 4))[(c - 8) % 24];
	}

	static void ScreenDraw(int32_t c)
	{
		uint32_t col = ScreenColour(c);
		uint32_t bg, amb;   // background / ambient colours of the tick
		bool stage = true;
		const SorceressSpawnParams &g = PG();
		if (c < g.screen.tiles)
		{
			Tiles(col | 0x64000000);
			bg = ColA();
			amb = ColB();
		}
		else if (c < g.screen.flash)
		{
			Tiles(col | 0x64000000);
			bg = ColA();
			amb = ColB();
			SetScreenFlash2(0x1000, 0);
			stage = false;
		}
		else if (c < g.screen.fade_from)
		{
			const int32_t i = c - g.screen.flash;
			if (c == GridStart()) GridInit();   // (the game's: i == 0)
			bg = ColA();
			amb = ColB();
			GridRipple(i, g.ripple.steps);
			GridDraw(var<uint8_t>(0x1D96A80), col);
			SetScreenFlash2(0x1000, 0);
			stage = false;
		}
		else if (c < g.screen.fade_to)
		{
			const int32_t k = c - g.screen.fade_from;
			const int32_t w = shl32(k, 12) / (g.screen.fade_to - g.screen.fade_from);
			if (c == GridStart()) GridInit();   // (a fade from the grid's first tick: made here)
			uint32_t gcol = col | 0x2000000, rgbc;
			GteSetFarColor(0, 0, 0);
			rgbc = gcol;
			GteLoadRGBC(&rgbc);
			GteSetIR0(w);
			GteDPCS();
			GteStoreRGB2(&gcol);
			GteSetFarColor(var<uint8_t>(own::R(0x24F3618)), var<uint8_t>(own::R(0x24F3619)), var<uint8_t>(own::R(0x24F361A)));
			GteLoadRGBC(&ColA());
			GteDPCS();
			GteStoreRGB2(&bg);
			GteSetFarColor(var<uint8_t>(own::R(0x24F447C)), var<uint8_t>(own::R(0x24F447D)), var<uint8_t>(own::R(0x24F447E)));
			GteLoadRGBC(&ColB());
			GteDPCS();
			GteStoreRGB2(&amb);
			GridRipple(c - g.screen.flash, g.ripple.steps);
			GridDraw(var<uint8_t>(0x1D96A80), gcol);
			SetScreenFlash2(0x1000 - ((5 * w * 512) >> 12), 0);
		}
		else
		{
			bg = StageColA();
			amb = StageColB();
			SetScreenFlash2(0x600, 0);
		}
		if (stage) StageToVram();
		var<uint32_t>(0xB8B7D8) = bg;
		SetAmbient(amb);
	}

	static uint32_t __cdecl ScreenTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag209_sorceress_spawn_held.inc
		FX_HELD(held_note_gte();)
		if (n->c < PG().screen.ticks)
		{
			FX_PART(PART_SCREEN);
			ScreenDraw(n->c);
			FX_HELD(held_note_screen(n);)
		}
		if (Paused()) return 0;
		if (n->c == 0) ShowStage();
		if (n->c == PG().screen.ticks) HideStage();
		n->c++;
		return n->c > PG().screen.ticks ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Prims (node-driven prim models in the camera frame)
	// ------------------------------------------------------------------
	// t = position (y: an override), angles (0, ay, 0), scale (sx, sy, sx)
	static void PrimMatrix(const Node24 *n, int16_t ay, int32_t sx, int32_t sy, int32_t ty)
	{
		int16_t v[4] = { 0, ay, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(v, &m);
		m.t[0] = n->p10;
		m.t[1] = ty;
		int32_t sc[3] = { sx, sy, sx };
		m.t[2] = n->p14;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	// 0x638490 (timeline tick 1): model 0xE288F0 through MAG_166_sub_64F0A0 (draw modes 0xE1000240 / 0xE1000220),
	// growing 16 ticks (+0x1E decaying by 1/8), fading out from 102; 110 ticks
	static void PrimADraw(const Node24 *n, int32_t s)
	{
		PrimMatrix(n, 0, s, s, n->p12);
		uint8_t *h = (uint8_t *)FieldAlloc(0x60);
		*(uint32_t *)h = fxp::model(PG().models.glow, MODEL_Glow);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		*(uint32_t *)(h + 0x58) = 0xE1000240;
		*(uint32_t *)(h + 0x5C) = 0xE1000220;
		if (n->c >= PG().glow.out)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = shl32(n->c - PG().glow.out, 9);
		}
		Cursor() = PrimModel2(h, OTBase() + 0x44, 2, Cursor());
		FieldFree(0x60);
	}

	static uint32_t __cdecl PrimATask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag209_sorceress_spawn_held.inc
		FX_HELD(held_note_gte();)
		FX_PART(PART_GLOW);
		PrimADraw(n, n->s1C);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c < PG().glow.grow)
		{
			int16_t v = n->s1E;
			n->s1C = (int16_t)(n->s1C + v);
			n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 8));
		}
		n->c = (int16_t)(c + 1);
		return n->c >= PG().glow.life ? TASK_END : 0;
	}

	// 0x6385E0 (timeline tick 1, x 12): model table 0xE2B400[e], turning (+0x1A), scale (+0x1C, +0x20, +0x1C)
	// growing 16 / 32 ticks (speeds decaying by 1/8), fading out over its last 8 ticks; 110 ticks (55 with +0x16)
	static int32_t PrimBEnd(const Node24 *n) { return n->p16 ? PG().bursts.life_b : PG().bursts.life; }

	static void PrimBDraw(const Node24 *n, int16_t a, int32_t sx, int32_t sy)
	{
		PrimMatrix(n, a, sx, sy, n->p12);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)(h + 8) = 0;
		const int32_t end = PrimBEnd(n);
		*(uint32_t *)h = fxp::model(PG().models.bursts[n->e], var<uint32_t>(BURSTS + 4 * (int32_t)n->e));
		*(uint32_t *)(h + 0x1C) = 0x33;
		const int32_t c = n->c;
		if (c >= end - 8)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = shl32(c - (end - 8), 9);
		}
		Cursor() = RenderPrimModel(h, OTBase() + 0x44, 2, Cursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl PrimBTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag209_sorceress_spawn_held.inc
		FX_HELD(held_note_gte();)
		FX_PART(PART_BURSTS);
		PrimBDraw(n, n->a18, n->s1C, n->s20);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		const int32_t end = PrimBEnd(n);
		n->a18 = (int16_t)(n->a18 + n->s1A);
		int16_t c = n->c;
		if (c < PG().bursts.grow)
		{
			int16_t v = n->s1E;
			n->s1C = (int16_t)(n->s1C + v);
			n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 8));
		}
		if (c < PG().bursts.grow2)
		{
			int16_t v = n->s22;
			n->s20 = (int16_t)(n->s20 + v);
			n->s22 = (int16_t)(v - (int16_t)((int32_t)v / 8));
		}
		n->c = (int16_t)(c + 1);
		return n->c >= end ? TASK_END : 0;
	}

	// 0x638770 (timeline A tick 1): scrolling prim model 0xE26340 (MAG_066_RenderScrollPrimModel) at the node's x / z
	// and the caster's bone 0xF1 height, scroll 8 per tick, texture word table 0xE2B4B0[c % 18], fading out from 102;
	// 110 ticks
	static void PrimCDraw(const Node24 *n, int32_t c)
	{
		int16_t v[4] = { 0, 0, 0, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(v, &m);
		GetEffectSpawnPosition(Caster(), 0xF1, 0, v);
		m.t[0] = n->p10;
		m.t[1] = v[1];
		int32_t s = n->s1C;
		int32_t sc[3] = { s, s, s };
		m.t[2] = n->p14;
		Scale3DMatrix(&m, sc);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x90);
		*(uint32_t *)(h + 8) = 0;
		*(int16_t *)(h + 0x18) = 0;
		*(int16_t *)(h + 0x1A) = 0;
		*(uint32_t *)(h + 0x10) = 0;
		*(uint32_t *)h = fxp::model(PG().models.swirl, MODEL_Swirl);
		*(uint32_t *)(h + 0xC) = PG().swirl.fade;
		*(uint32_t *)(h + 0x20) = 0xF3;
		*(int32_t *)(h + 0x14) = c * PG().swirl.scroll;
		*(int16_t *)(h + 0x1C) = 0x40;
		*(int16_t *)(h + 0x1E) = 0x80;
		*(uint32_t *)(h + 0x24) = ((const uint32_t *)fxp::data(PG().tables.swirl, SWIRL_COLOURS, 72, 4))[c % 0x12];
		if (c >= PG().swirl.out) *(int32_t *)(h + 0xC) = PG().swirl.fade + PG().swirl.out_step * (c - PG().swirl.out);
		Cursor() = ScrollPrimModel(h, OTBase() + 0x44, 2, Cursor());
		FieldFree(0x90);
	}

	static uint32_t __cdecl PrimCTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag209_sorceress_spawn_held.inc
		FX_HELD(held_note_gte();)
		FX_PART(PART_SWIRL);
		PrimCDraw(n, n->c);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= PG().swirl.life ? TASK_END : 0;
	}

	// 0x638900 (A: ticks 31, 41 .. 91; B: 73, 79, 85): model 0xE277B8 at the caster's bone 0xF1 (+ random height),
	// turning, growing (+0x1E decaying by 1/16), fading in 2 ticks and out from 8; 24 ticks
	static void PrimDDraw(const Node24 *n, int16_t a, int32_t s)
	{
		PrimMatrix(n, a, s, s, n->p12);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		const int32_t c = n->c;
		*(uint32_t *)h = fxp::model(PG().models.rise, MODEL_Rise);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (c < 2)
		{
			*(int32_t *)(h + 0xC) = shl32(2 - c, 11);
			*(uint32_t *)(h + 0x1C) = 0xF3;
		}
		else if (c >= 8)
		{
			*(int32_t *)(h + 0xC) = shl32(c - 8, 8);
			*(uint32_t *)(h + 0x1C) = 0xF3;
		}
		Cursor() = RenderPrimModel(h, OTBase() + 0x44, 2, Cursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl PrimDTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag209_sorceress_spawn_held.inc
		FX_HELD(held_note_gte();)
		FX_PART(PART_RISE);
		PrimDDraw(n, n->a18, n->s1C);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->a18 = (int16_t)(n->a18 + n->s1A);
		n->s1C = (int16_t)(n->s1C + v);
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 16));
		v = n->s22;
		n->s20 = (int16_t)(n->s20 + v);
		n->c++;
		n->s22 = (int16_t)(v - (int16_t)((int32_t)v / 16));
		return n->c >= PG().rise.life ? TASK_END : 0;
	}

	// 0x638A80 (A: tick 115; B: ticks 96 .. 108 by 6): model 0xE290E0 at the caster's bone 0xF1 x / z (height 0),
	// turning (speed decaying by 1/16), growing (1/16), fading out from 4; 20 ticks
	static void PrimEDraw(const Node24 *n, int16_t a, int32_t s)
	{
		PrimMatrix(n, a, s, s, n->p12);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		const int32_t c = n->c;
		*(uint32_t *)h = fxp::model(PG().models.ring, MODEL_Ring);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (c >= 4)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = shl32(c - 4, 8);
		}
		Cursor() = RenderPrimModel(h, OTBase() + 0x44, 2, Cursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl PrimETask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag209_sorceress_spawn_held.inc
		FX_HELD(held_note_gte();)
		FX_PART(PART_RINGS);
		PrimEDraw(n, n->a18, n->s1C);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t a = n->s1A, v = n->s1E;
		n->a18 = (int16_t)(n->a18 + a);
		n->s1A = (int16_t)(a - (int16_t)(a >> 4));
		n->s1C = (int16_t)(n->s1C + v);
		n->c++;
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 16));
		return n->c >= PG().rings.life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Timelines A (0x6375D0) and B (0x638BD0), 131 ticks
	// ------------------------------------------------------------------
	static Node24 *Spawn(uint32_t fn_)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), own::Fn(fn_));
		return n;
	}

	static void SpawnBursts(const Node24 *t, int16_t p16)
	{
		int32_t r = CrtRand();
		const int16_t a = (int16_t)(r % 4096);
		for (int16_t k = 0; k < PG().bursts.count && k < 12; k++)
		{
			Node24 *n = Spawn(ORIG_PrimBTask);
			*(uint32_t *)&n->p10 = *(const uint32_t *)&t->p10;
			*(uint32_t *)&n->p14 = *(const uint32_t *)&t->p14;
			n->c = 0;
			n->p16 = p16;
			n->e = k;
			n->a18 = a;
			n->s1A = PG().bursts.spin;
			n->s1E = PG().bursts.size;
			n->s1C = PG().bursts.size;
			r = CrtRand() % PG().bursts.size2_rand + PG().bursts.size2;
			n->s22 = (int16_t)(r / 6);
			n->s20 = (int16_t)(r / 6);
		}
	}

	static void SpawnD()
	{
		Node24 *n = Spawn(ORIG_PrimDTask);
		n->c = 0;
		GetEffectSpawnPosition(Caster(), 0xF1, 0, &n->p10);
		n->p12 = (int16_t)(n->p12 + (int16_t)(CrtRand() % PG().rise.height));
		n->a18 = (int16_t)(CrtRand() % 2048);
		n->s1A = (int16_t)(-0x28 - CrtRand() % 0x78);
		int32_t r = CrtRand() % PG().rise.size_rand + PG().rise.size;
		n->s1E = (int16_t)(r / 8);
		n->s1C = (int16_t)(r / 8);
		r = CrtRand() % 2048 + 0x600;
		n->s22 = (int16_t)(r / 8);
		n->s20 = (int16_t)(r / 8);
	}

	static void SpawnE(bool b)
	{
		Node24 *n = Spawn(ORIG_PrimETask);
		n->c = 0;
		GetEffectSpawnPosition(Caster(), 0xF1, 0, &n->p10);
		n->p12 = 0;
		n->a18 = (int16_t)(CrtRand() % 4096);
		if (b) n->s1A = (int16_t)((CrtRand() % 0x28) * 2 + 0x64);
		else n->s1A = (int16_t)(CrtRand() % 0x28 + 0x32);
		int32_t r = CrtRand() % PG().rings.size_rand + PG().rings.size;
		if (b) r *= 3;
		n->s1E = (int16_t)(r / 12);
		n->s1C = (int16_t)(r / 12);
	}

	static void Flash(int16_t c)
	{
		const SorceressSpawnParams &g = PG();
		if (c < g.flash.in) SetScreenFlash2(g.flash.in_step * c, 0);
		else if (c >= g.flash.out) SetScreenFlash2(g.flash.out_step * (g.timing.end - c), 0);
	}

	// returns -1 when the tick waits (file load pending)
	static int32_t TimelineStart(Node24 *t, bool b)
	{
		const uint32_t f = var<uint32_t>(0x1D96A9C);
		if (f & 0x201)
		{
			if (f & 1) return -1;
			if (LoadState() < 0) return -1;
		}
		if (t->c == 0)
		{
			GetDefaultEffectPosition(Caster(), &t->p10);
			uint32_t a = var<uint32_t>(0xB8B7D8), c = var<uint32_t>(0xB8B9A8);
			t->p12 = (int16_t)(t->p12 + (int16_t)0xF8F8);
			ColA() = a;
			ColB() = c;
			if (b) var<uint32_t>(own::R(0x24F7A64)) = ClaimVoiceSlot2(0xE2B2C8, 1, 0x80);
		}
		const SorceressSpawnParams &g = PG();
		if (t->c == g.timing.load) WitchGetNewStage(SummonData());
		if (t->c == g.timing.swap)
		{
			if (LoadState() < 0) return -1;
			ChangeStage(SummonData());
			StageColA() = var<uint32_t>(0xB8B7D8);
			StageColB() = var<uint32_t>(0xB8B9A8);
		}
		if (t->c == g.timing.screen) Spawn(ORIG_ScreenTask)->c = 0;
		if (t->c == 0) Caster()[0] |= 8;
		if (t->c == (b ? g.timing.hide_b : g.timing.hide_a)) *(uint16_t *)Caster() &= 0xFFF7;
		if (t->c == g.timing.start)
		{
			Node24 *n = Spawn(ORIG_PrimATask);
			*(uint32_t *)&n->p10 = *(const uint32_t *)&t->p10;
			*(uint32_t *)&n->p14 = *(const uint32_t *)&t->p14;
			n->c = 0;
			n->s1E = g.glow.size;
			n->s1C = g.glow.size;
		}
		return 0;
	}

	static uint32_t TimelineEnd(Node24 *t)
	{
		t->c++;
		if (t->c <= PG().timing.end) return 0;
		SetScreenFlash2(0, 0);
		return TASK_END;
	}

	static uint32_t __cdecl TimelineA(TaskNode *tn)
	{
		Node24 *t = (Node24 *)tn;
		if (TimelineStart(t, false) < 0) return 0;
		const SorceressSpawnParams &g = PG();
		if (t->c == g.timing.start)
		{
			SpawnBursts(t, 0);
			Node24 *n = Spawn(ORIG_PrimCTask);
			*(uint32_t *)&n->p10 = *(const uint32_t *)&t->p10;
			n->c = 0;
			*(uint32_t *)&n->p14 = *(const uint32_t *)&t->p14;
			n->s1C = g.swirl.size;
		}
		int16_t c = t->c;
		if (SeriesTick(c, g.rise.first_a, g.rise.every_a, g.rise.last_a)) SpawnD();
		if (t->c == g.rings.tick_a) SpawnE(false);
		Flash(t->c);
		if (t->c == g.timing.result) ApplyActionResultToTarget(*(uint32_t *)(*(uint8_t **)(CastCtx() + 4) + 8));
		if (t->c == g.timing.sound) BdPlaySE(fxp::sound(g.sound_a, 0xE2B3F4), 0, 0x80);
		return TimelineEnd(t);
	}

	static uint32_t __cdecl TimelineB(TaskNode *tn)
	{
		Node24 *t = (Node24 *)tn;
		if (TimelineStart(t, true) < 0) return 0;
		const SorceressSpawnParams &g = PG();
		if (t->c == g.timing.start) SpawnBursts(t, 1);
		int16_t c = t->c;
		if (SeriesTick(c, g.rise.first_b, g.rise.every_b, g.rise.last_b)) SpawnD();
		c = t->c;
		if (SeriesTick(c, g.rings.first_b, g.rings.every_b, g.rings.last_b)) SpawnE(true);
		if (t->c == 0x16) ScreenFade(0, 2, 4, 0xFF);
		if (t->c == 0x1C) ScreenFade(0, 2, 4, 0xFF);
		if (t->c == 0x32) ScreenFade(0, 2, 8, 0xFF);
		c = t->c;
		if (c >= 0x5D && c < 0x65)
		{
			int32_t s = ComputeSin(shl32(c - 0x30, 10) / 8 + 0x400);
			int32_t v = (60 * s) >> 12;
			if (!(t->c & 1)) v = -v;
			var<int16_t>(0x1D97712) = (int16_t)v;   // camera shake
		}
		Flash(t->c);
		if (t->c == g.timing.result) ApplyActionResultToTarget(*(uint32_t *)(*(uint8_t **)(CastCtx() + 4) + 8));
		if (t->c == g.timing.sound) BdPlaySE(fxp::sound(g.sound_b, 0xE2B3F8), 0, 0x80);
		if (t->c == 0x7F)
			if (VoiceSlotBusy(var<uint32_t>(own::R(0x24F7A64)))) ReleaseVoiceSlot(var<uint32_t>(own::R(0x24F7A64)));
		return TimelineEnd(t);
	}

	// ------------------------------------------------------------------
	// Setup (0x637500): the game runs its own; Fujin's programs this (its memory may be a copy's): the packet arena, the
	// summon data (the grid cells), the cast, its first target's and its caster's slots, the root (the master) and the
	// sub-queue with timeline B for the wormlike sorceress (its caster's com id 0x85), else A; the camera and the TIM
	// ------------------------------------------------------------------
	static TaskQueue *__cdecl Setup(uint8_t *ctx)
	{
		var<uint32_t>(own::R(0x24F7A5C)) = (uint32_t)layouts::MagicBuffer();
		var<uint32_t>(own::R(0x24F4484)) = SummonData();
		var<uint8_t *>(own::R(0x24F4478)) = ctx;
		var<uint32_t>(own::R(0x24F361C)) = **(uint8_t **)(*(uint8_t **)(ctx + 4) + 8);   // the first target's slot
		var<uint32_t>(own::R(0x24F4480)) = ctx[0];                                         // the caster's
		InitTaskQueuePool(&RootQueue(), (void *)own::R(0x24F3620), 0x10, 2);
		((Node24 *)AddTaskToQueue(&RootQueue(), own::Fn(ORIG_Master)))->c = 0;
		InitTaskQueuePool(&SubQueue(), (void *)own::R(0x24F3668), 0x24, 100);
		const bool b = Caster()[4] == 0x85;
		((Node24 *)AddTaskToQueue(&SubQueue(), own::Fn(b ? ORIG_TimelineB : ORIG_TimelineA)))->c = 0;
		PlayCameraAnimation((const void *)fxp::camera(PG().res.camera, CAMERA_ANIM));
		QueueTimUpload(fxp::tim(PG().res.tim, TIM));
		return &RootQueue();
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "timeline", "Timeline", "The timelines (A: c0m115 / c0m116, B: the wormlike c0m117): the sound, the stage's file loaded then the stage swapped, the caster hidden, the screen flash, the action result; B's screen fades, camera shake and voice; the TIM and the camera" },
			{ PART_SCREEN, "screen", "Screen", "The stage rendered into VRAM (0x240, 0x100) and drawn back as tiles, a white flash, then on a rippling grid around the caster's head fading to the new stage's colours" },
			{ PART_GLOW, "glow", "Glow", "The glow mesh at the caster, growing then fading out" },
			{ PART_BURSTS, "bursts", "Bursts", "The 12 burst meshes at the caster, turning and growing, fading out over their last ticks" },
			{ PART_SWIRL, "swirl", "Swirl", "Timeline A's scrolling swirl mesh at the caster's head height, its colour cycling, fading out" },
			{ PART_RISE, "rise", "Rising rings", "Rings rising at the caster's head, turning, growing, fading in and out" },
			{ PART_RINGS, "rings", "Closing rings", "Rings at the caster's feet, turning and growing, fading out" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing", "Ticks", "The timelines' ticks" },
		};
#define MPF(member) (uint32_t)offsetof(SorceressSpawnParams, member)
		static const Field FIELDS[] = {
			{ "timeline", "sound_a", "Sound A", "Timeline A's sound (c0m115 / c0m116)", MPF(sound_a), fxp::Sound, 0, 0x7FFFFFFF },
			{ "timeline", "sound_b", "Sound B", "Timeline B's sound (the wormlike c0m117)", MPF(sound_b), fxp::Sound, 0, 0x7FFFFFFF },
			{ "timeline", "timing.sound", "Sound", "The sound's tick", MPF(timing.sound), fxp::I16, 0, 400 },
			{ "timeline", "timing.load", "Stage load", "The next stage's file loaded (into the summon data)", MPF(timing.load), fxp::I16, 0, 400 },
			{ "timeline", "timing.swap", "Stage swap", "The stage swapped for it (waiting for the load)", MPF(timing.swap), fxp::I16, 0, 400 },
			{ "timeline", "timing.screen", "Screen", "The screen task starts", MPF(timing.screen), fxp::I16, 0, 400 },
			{ "timeline", "timing.start", "Meshes", "The glow, the bursts and A's swirl start", MPF(timing.start), fxp::I16, 0, 400 },
			{ "timeline", "timing.result", "Result", "The action result", MPF(timing.result), fxp::I16, 0, 400 },
			{ "timeline", "timing.end", "End", "The timeline's last tick (the screen flash cleared after it)", MPF(timing.end), fxp::I16, 0, 400 },
			{ "timeline", "timing.hide_a", "Shown again (A)", "The caster hidden from tick 0 to this one (timeline A)", MPF(timing.hide_a), fxp::I16, 0, 400 },
			{ "timeline", "timing.hide_b", "Shown again (B)", "The same for timeline B", MPF(timing.hide_b), fxp::I16, 0, 400 },
			{ "timeline", "flash.in", "Flash in", "The screen flash rising over the first ticks", MPF(flash.in), fxp::I16, 0, 400 },
			{ "timeline", "flash.in_step", "Flash in step", "By this a tick", MPF(flash.in_step), fxp::I16, 0, 0x1000 },
			{ "timeline", "flash.out", "Flash out from", "From this tick it falls to none at the end", MPF(flash.out), fxp::I16, 0, 400 },
			{ "timeline", "flash.out_step", "Flash out step", "By this a tick", MPF(flash.out_step), fxp::I16, 0, 0x1000 },
			{ "timeline", "res.tim", "TIM", "The texture its setup uploads (a TIM with a palette)", MPF(res.tim), fxp::Tim, 0, 0x7FFFFFFF },
			{ "timeline", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", MPF(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "screen", "screen.tiles", "Tiles", "The stage's VRAM copy drawn back as tiles until this tick of the screen task", MPF(screen.tiles), fxp::I16, 0, 400 },
			{ "screen", "screen.flash", "White", "White until this tick (the grid made then)", MPF(screen.flash), fxp::I16, 0, 400 },
			{ "screen", "screen.fade_from", "Fade from", "The rippling grid fading to the new stage's colours from this tick", MPF(screen.fade_from), fxp::I16, 0, 400 },
			{ "screen", "screen.fade_to", "Fade to", "Until this tick (more than the last)", MPF(screen.fade_to), fxp::I16, 1, 400 },
			{ "screen", "screen.ticks", "Ticks", "The screen task's ticks (the stage hidden at the last)", MPF(screen.ticks), fxp::I16, 0, 400 },
			{ "screen", "ripple.steps", "Ripple steps", "The ripple's height a half sine over this many steps (one a tick from the grid on; at least 1)", MPF(ripple.steps), fxp::I16, 1, 400 },
			{ "screen", "ripple.wave", "Wave", "The ripple's angle step a pixel from the caster's head", MPF(ripple.wave), fxp::I16, 0, 0x1000, fxp::Angle },
			{ "screen", "ripple.speed", "Speed", "Its angle step a tick (outwards)", MPF(ripple.speed), fxp::I16, -0x1000, 0x1000, fxp::Angle },
			{ "screen", "ripple.amp", "Height", "Its height", MPF(ripple.amp), fxp::I16, 0, 0x7FFF },
			{ "screen", "ripple.range", "Reach", "Its reach in pixels (fading to none there)", MPF(ripple.range), fxp::I16, 1, 0x7FFF },
			{ "screen", "ripple.shrink", "Shrink", "Its reach shrinking by this a step", MPF(ripple.shrink), fxp::I16, 0, 0x100 },
			{ "screen", "tables.colours_in", "Colours in", "The screen's colour on its first 8 ticks (8 words: r, g, b, 0)", MPF(tables.colours_in), fxp::Data, 0, 0x7FFFFFFF },
			{ "screen", "tables.colours", "Colour cycle", "Then this 24-colour cycle (24 words)", MPF(tables.colours), fxp::Data, 0, 0x7FFFFFFF },
			{ "glow", "glow.size", "Size", "Its size and its first growth (the growth slowing by 1/8 a tick)", MPF(glow.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "glow", "glow.grow", "Grow", "Growing on its first ticks", MPF(glow.grow), fxp::I16, 0, 400 },
			{ "glow", "glow.out", "Fade out from", "Fading out from this tick", MPF(glow.out), fxp::I16, 0, 400 },
			{ "glow", "glow.life", "Life", "Its ticks", MPF(glow.life), fxp::I16, 0, 400 },
			{ "glow", "models.glow", "Mesh", "The glow's prim model", MPF(models.glow), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "bursts.count", "Count", "How many (12 at most: one mesh each)", MPF(bursts.count), fxp::I16, 0, 12 },
			{ "bursts", "bursts.spin", "Spin", "Their angle step a tick", MPF(bursts.spin), fxp::I16, -0x800, 0x800, fxp::Angle },
			{ "bursts", "bursts.size", "Size", "Their size and first growth (slowing by 1/8 a tick)", MPF(bursts.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "bursts", "bursts.size2", "Height", "Their height (and first growth): (this + a random below the next) / 6", MPF(bursts.size2), fxp::I16, 0, 0x7FFF },
			{ "bursts", "bursts.size2_rand", "Height random", "(at least 1)", MPF(bursts.size2_rand), fxp::I16, 1, 0x7FFF },
			{ "bursts", "bursts.grow", "Grow", "Growing on their first ticks", MPF(bursts.grow), fxp::I16, 0, 400 },
			{ "bursts", "bursts.grow2", "Grow height", "Growing taller on their first ticks", MPF(bursts.grow2), fxp::I16, 0, 400 },
			{ "bursts", "bursts.life", "Life", "Their ticks (fading out over the last 8)", MPF(bursts.life), fxp::I16, 0, 400 },
			{ "bursts", "bursts.life_b", "Life (B)", "Timeline B's", MPF(bursts.life_b), fxp::I16, 0, 400 },
			{ "bursts", "models.bursts[0]", "Mesh 1", "The first burst's prim model", MPF(models.bursts[0]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[1]", "Mesh 2", "The second's", MPF(models.bursts[1]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[2]", "Mesh 3", "The third's", MPF(models.bursts[2]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[3]", "Mesh 4", "The fourth's", MPF(models.bursts[3]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[4]", "Mesh 5", "The fifth's", MPF(models.bursts[4]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[5]", "Mesh 6", "The sixth's", MPF(models.bursts[5]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[6]", "Mesh 7", "The seventh's", MPF(models.bursts[6]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[7]", "Mesh 8", "The eighth's", MPF(models.bursts[7]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[8]", "Mesh 9", "The ninth's", MPF(models.bursts[8]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[9]", "Mesh 10", "The tenth's", MPF(models.bursts[9]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[10]", "Mesh 11", "The eleventh's", MPF(models.bursts[10]), fxp::Model, 0, 0x7FFFFFFF },
			{ "bursts", "models.bursts[11]", "Mesh 12", "The twelfth's", MPF(models.bursts[11]), fxp::Model, 0, 0x7FFFFFFF },
			{ "swirl", "swirl.size", "Size", "Its size", MPF(swirl.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "swirl", "swirl.scroll", "Scroll", "Its texture scroll a tick", MPF(swirl.scroll), fxp::I16, -0x100, 0x100 },
			{ "swirl", "swirl.fade", "Fade", "Its fade (0x1000: none drawn)", MPF(swirl.fade), fxp::I16, 0, 0x1000 },
			{ "swirl", "swirl.out", "Fade out from", "Fading out from this tick", MPF(swirl.out), fxp::I16, 0, 400 },
			{ "swirl", "swirl.out_step", "Fade out step", "By this a tick", MPF(swirl.out_step), fxp::I16, 0, 0x1000 },
			{ "swirl", "swirl.life", "Life", "Its ticks", MPF(swirl.life), fxp::I16, 0, 400 },
			{ "swirl", "models.swirl", "Mesh", "The swirl's prim model (scrolling)", MPF(models.swirl), fxp::Model, 0, 0x7FFFFFFF },
			{ "swirl", "tables.swirl", "Colours", "Its colour cycle (18 words)", MPF(tables.swirl), fxp::Data, 0, 0x7FFFFFFF },
			{ "rise", "rise.first_a", "First (A)", "Timeline A's first rising ring", MPF(rise.first_a), fxp::I16, 0, 400 },
			{ "rise", "rise.every_a", "Every (A)", "Then one every this many ticks (at least 1)", MPF(rise.every_a), fxp::I16, 1, 400 },
			{ "rise", "rise.last_a", "Last (A)", "Until this tick", MPF(rise.last_a), fxp::I16, 0, 400 },
			{ "rise", "rise.first_b", "First (B)", "Timeline B's first", MPF(rise.first_b), fxp::I16, 0, 400 },
			{ "rise", "rise.every_b", "Every (B)", "(at least 1)", MPF(rise.every_b), fxp::I16, 1, 400 },
			{ "rise", "rise.last_b", "Last (B)", "Until this tick", MPF(rise.last_b), fxp::I16, 0, 400 },
			{ "rise", "rise.height", "Height random", "Up to this above the caster's head (at least 1)", MPF(rise.height), fxp::I16, 1, 0x7FFF },
			{ "rise", "rise.size", "Size", "Its size and first growth: (this + a random below the next) / 8 (slowing by 1/16 a tick)", MPF(rise.size), fxp::I16, 0, 0x7FFF },
			{ "rise", "rise.size_rand", "Size random", "(at least 1)", MPF(rise.size_rand), fxp::I16, 1, 0x7FFF },
			{ "rise", "rise.life", "Life", "Its ticks (fading in over 2, out from 8)", MPF(rise.life), fxp::I16, 0, 400 },
			{ "rise", "models.rise", "Mesh", "The rising ring's prim model", MPF(models.rise), fxp::Model, 0, 0x7FFFFFFF },
			{ "rings", "rings.tick_a", "Tick (A)", "Timeline A's one closing ring", MPF(rings.tick_a), fxp::I16, 0, 400 },
			{ "rings", "rings.first_b", "First (B)", "Timeline B's first", MPF(rings.first_b), fxp::I16, 0, 400 },
			{ "rings", "rings.every_b", "Every (B)", "Then one every this many ticks (at least 1)", MPF(rings.every_b), fxp::I16, 1, 400 },
			{ "rings", "rings.last_b", "Last (B)", "Until this tick", MPF(rings.last_b), fxp::I16, 0, 400 },
			{ "rings", "rings.size", "Size", "Its size and first growth: (this + a random below the next) / 12, B's x 3 (slowing by 1/16 a tick)", MPF(rings.size), fxp::I16, 0, 0x7FFF },
			{ "rings", "rings.size_rand", "Size random", "(at least 1)", MPF(rings.size_rand), fxp::I16, 1, 0x7FFF },
			{ "rings", "rings.life", "Life", "Its ticks (fading out from 4)", MPF(rings.life), fxp::I16, 0, 400 },
			{ "rings", "models.ring", "Mesh", "The closing ring's prim model", MPF(models.ring), fxp::Model, 0, 0x7FFFFFFF },
		};
#undef MPF
	}
}

	void register_mag209_sorceress_spawn()
	{
		fxp::register_params({ 209, "Sorceress spawn", &s209::live, &s209::GAME, sizeof(s209::SorceressSpawnParams),
		                       s209::table::PARTS, (int)(sizeof(s209::table::PARTS) / sizeof(s209::table::PARTS[0])),
		                       s209::table::FIELDS, (int)(sizeof(s209::table::FIELDS) / sizeof(s209::table::FIELDS[0])),
		                       s209::table::GROUPS, (int)(sizeof(s209::table::GROUPS) / sizeof(s209::table::GROUPS[0])) });
		own::note(s209::ORIG_Setup, (void *)s209::Setup);
		own::note(s209::ORIG_Master, (void *)s209::Master);
		register_port(s209::ORIG_Master, (void *)s209::Master, "209 Master", 209);
		own::note(s209::ORIG_EndTask, (void *)s209::EndTask);
		register_port(s209::ORIG_EndTask, (void *)s209::EndTask, "209 EndTask", 209);
		own::note(s209::ORIG_TimelineA, (void *)s209::TimelineA);
		register_port(s209::ORIG_TimelineA, (void *)s209::TimelineA, "209 TimelineA", 209);
		own::note(s209::ORIG_TimelineB, (void *)s209::TimelineB);
		register_port(s209::ORIG_TimelineB, (void *)s209::TimelineB, "209 TimelineB", 209);
		own::note(s209::ORIG_ScreenTask, (void *)s209::ScreenTask);
		register_port(s209::ORIG_ScreenTask, (void *)s209::ScreenTask, "209 ScreenTask", 209);
		own::note(s209::ORIG_PrimATask, (void *)s209::PrimATask);
		register_port(s209::ORIG_PrimATask, (void *)s209::PrimATask, "209 PrimATask", 209);
		own::note(s209::ORIG_PrimBTask, (void *)s209::PrimBTask);
		register_port(s209::ORIG_PrimBTask, (void *)s209::PrimBTask, "209 PrimBTask", 209);
		own::note(s209::ORIG_PrimCTask, (void *)s209::PrimCTask);
		register_port(s209::ORIG_PrimCTask, (void *)s209::PrimCTask, "209 PrimCTask", 209);
		own::note(s209::ORIG_PrimDTask, (void *)s209::PrimDTask);
		register_port(s209::ORIG_PrimDTask, (void *)s209::PrimDTask, "209 PrimDTask", 209);
		own::note(s209::ORIG_PrimETask, (void *)s209::PrimETask);
		register_port(s209::ORIG_PrimETask, (void *)s209::PrimETask, "209 PrimETask", 209);
		// 30 fps layer: see mag209_sorceress_spawn_held.inc
		FX_HELD(register_mag209_held();)
	}
	FX_LAB_MODULE(register_mag209_sorceress_spawn);
}

#ifdef FF8_FX_HELD
#include "mag209_sorceress_spawn_held.inc"
#endif
