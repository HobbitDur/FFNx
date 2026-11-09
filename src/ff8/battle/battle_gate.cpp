/****************************************************************************/
//    Copyright (C) 2009 Aali132                                            //
//    Copyright (C) 2018 quantumpencil                                      //
//    Copyright (C) 2018 Maxime Bacoux                                      //
//    Copyright (C) 2020 myst6re                                            //
//    Copyright (C) 2020 Chris Rizzitello                                   //
//    Copyright (C) 2020 John Pritchard                                     //
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

#include "battle_gate.h"
#include "battle_host.h"
#include "battle_memory.h"
#include "fx/fx_held.h"
#include "fx/fx_modules.h"
#include "fx/fx_port.h"

#include <intrin.h>
#include <stdio.h>
#include <string.h>
#include <array>
#include <cmath>
#include <vector>

// Armed on any frame spent outside battle; the first battle frame after it resets every
// piece of per-battle 30fps state (see ff8_bgate_battle_reset).
bool ff8_bgate_left_battle = true;
bool ff8_bgate_active = false;
int ff8_bgate_n = 2;

// 60 Hz pad sampling in battle at 30fps. The engine reads the hardware ONCE per rendered
// frame (IsWindowNOTActive -> Input_ProcessInput 0x467D10, between the hidden and visible UI
// ticks), while the battle UI's pad ring (Input_PadRing_AdvanceAndComputeEdges, once per UI
// tick) computes press edges against that last read - so at 30fps only 30 samples/s reached
// the UI: GF Boost mashing capped at ~15 presses/s (PSX: 60 Hz vsync pad reads, ~30/s),
// short taps merged/lost in Zell's Duel and the menus. Here a second hardware read is taken
// halfway through the frame wait: the next frame's first UI tick sees it, the second UI tick
// sees the regular read -> 2 evenly spaced samples per frame = 60 Hz, like the PlayStation.
// The engine's own auto-repeat (dword_1CD02F0 gate, field/menu repeat) is suspended during
// the extra read so it does not tick twice per frame; battle uses its per-tick ring repeat.
static uint32_t ff8_bgate_mid_reads = 0; // diagnostics

void ff8_bgate_mid_frame_input()
{
	if (GetActiveWindow() == NULL)
		return;
	uint32_t *autorepeat = (uint32_t *)0x1CD02F0;
	uint32_t saved = *autorepeat;
	*autorepeat = 0;
	((void *(*)())0x467D10)(); // Input_ProcessInput
	*autorepeat = saved;
	ff8_bgate_mid_reads++;
}

// ---------------------------------------------------------------------------
// High-frame-rate battle mode - active at ff8_fps_limiter >= FPS_LIMITER_30FPS
// ---------------------------------------------------------------------------
// Vanilla PC locks the whole battle module at 15fps and catches up by running the
// battle-UI tick pair (isBattle_HUDupdate 0x4A8E30 + isBattle_HUDdisplay 0x4A84E0)
// 4x per rendered frame (3 hidden + 1 visible = 60 ticks/s), with ONE DirectInput
// poll per frame -> 15 Hz input sampling (lost/merged presses in Boost, Zell Duel
// and menu navigation). See wiki: Battle UI Timing and Input Sampling.
//
// Raising the limiter runs the module loop faster (ff8_limit_fps: limiter 2 -> 30fps,
// limiter 3 -> 60fps), which is what fixes the menu/input rate - but it also makes
// every battle subsystem run proportionally too fast. This block gates them back to
// native speed.
//
// ONE knob drives all of it: ff8_bgate_n, the number of host frames per real
// (15fps-equivalent) battle tick. 30fps -> 2, 60fps -> 4. It is derived from the
// configured limiter at install time (ff8_bgate_install_hooks), and every rate-
// dependent quantity below is expressed as a function of it rather than hardcoded,
// so switching between 30 and 60 is a config change, not a code change:
//
//   - UI tick pair: vanilla runs 4/frame; native rate needs 4/n per frame, so keep
//     the last (4/n - 1) hidden catch-up ticks plus the visible one (n=2 -> 2 ticks
//     per frame; n=4 -> 1). This single gate keeps ATB, Zell Duel countdown, Boost
//     tick, cursor blink and text speed at native pace, while the pad ring advances
//     on ticks that actually saw a fresh input poll.
//   - fresh-input latch divisor (CONST_BattleUI_TicksPerFrame @0xB8A3E4): 4 -> 4/n,
//     so the ctx+33 "fresh input" cadence matches the host frame rate.
//   - game time (Savemap_TickGameTimeAndCountdown): called 4x per loop iteration
//     unconditionally -> gate 1-in-n while in battle to hold native 60 ticks/s.
//   - battle model animation + AnimSeq choreography VM: advance 1-in-n, geometry
//     still rebuilt every tick (no double-buffer flicker).
//   - battle camera: keyframe time step 16 -> 16/n per tick (imm8 @0x503A80).
//   - magic/GF/limit/Draw effects: the effect tree ticks only on real frames (native
//     pace, nothing rewound or run twice); its draws are read from the SSIGPU arena and
//     redrawn on held frames, extrapolated per vertex from the last two ticks (identity
//     pairing + median vertex welding) - smooth for every effect, no per-spell code.
//   - entity animations: engine-native SLOW half-step reading forced at animation start;
//     animations still read 1-in-n (real Slow, stage models) get extrapolated poses.
//   - hit-effect task queue: same record/replay as effects; camera shake held.
//   - AnimSeq-spawned tasks (84/9F/99/B1/96 skipped, AD/AE/81/A6 counters native).
//   - status timers + Gilgamesh/Angelo countdown + end fade: real frames only.
//   - entity task visuals (status pulse/spin/bob/sprites, A8 fades), Renzokuken trigger clock and
//     the magic cast safety timeout: native pace.
//   - all per-battle state is reset at the start of each battle.
//
// (All addresses are FF8 2000 US/EN 1.2 specific, same as the 60fps branch.)
//
// Host frames per real battle tick: 2 at 30fps, 4 at 60fps. Set in
// ff8_bgate_install_hooks() from ff8_fps_limiter; 2 is only a placeholder default.
// Must divide 4 (the vanilla UI ticks/frame) exactly, which both values do.
// (ff8_bgate_n is declared above the frame limiter)

// false = diagnostics-only mode (vanilla behavior, no gating) - used to capture reference
// traces at ff8_fps_limiter < 2 for comparing against the gated build
// (ff8_bgate_active is declared above the frame limiter)

static int ff8_bgate_phase = 0; // 0 = advance frame, otherwise held (bumped in BdLink hook)
static uint32_t ff8_bgate_frame_no = 0; // host battle frames (bumped in the BdLink hook)
// Held-frame draw mode, cycled with F9 during battle (test builds): 0 = plain replay of tick N
// (no interpolation), 1 = vertices extrapolated, 2 = vertices + vertex colors extrapolated,
// 3 = nothing drawn on held frames (diagnostic: shows what the engine itself puts on screen),
// 4 = replay everything EXCEPT 15-bit (direct colour) textured prims, 5 = replay ONLY those
// (bisection: a screen-sized 15bpp quad is how an effect re-draws a copy of the screen)
static int ff8_bgate_fx_mode = 2;
// F5 (test builds): bypass the EFFECT pacing entirely - the effect tree ticks every host
// frame like stock FFNx (2x speed) and nothing is replayed. Bisection aid: tells whether a
// visual artifact comes from pacing the effect at all, or from something else in the mod.
static bool ff8_bgate_fx_bypass = false;
// F6 (test builds): also re-issue on held frames the VRAM commands the effect queued during
// its real tick (texture streaming included). Eden scrolls its tunnel texture 2 rows per tick;
// with the effect paced, that upload happens every other frame only.
static bool ff8_bgate_vq_replay_all = false;
static bool ff8_bgate_fx_log_stats = false; // true = one FFNx.log line per held frame (verbose)
// Per-effect totals, logged once when the effect finishes (one line per spell/GF cast)
struct ff8_bgate_fx_sum_t
{
	uint32_t ticks, held, interp_held, prims, match, far_, nosig, unparsed, welded, orphans, maxcol, vram_xfer, badcmd, ambiguous;
	uint32_t tpages[8], tpage_hits[8]; int ntpages; // diagnostics: texture pages used by replayed prims
	uint32_t vq_types[4];                          // VRAM queue commands the effect queued, by type
};

// TEMPORARY run-lag diagnostics: per-frame trace of party slots + AnimSeq events.
// Remove once the end-of-run hitch is understood.
#define FF8_BGATE_DIAG 0
static uint32_t ff8_bgate_diag_frame = 0;
#define FF8_BGATE_SLOT_BASE 0x1D97494u
#define FF8_BGATE_SLOT_IDX(slot) ((int)(((uint32_t)(slot) - FF8_BGATE_SLOT_BASE) / 156))

#if FF8_BGATE_DIAG
// render probe: log every actual BS_RenderBattleEntity execution for party entities,
// with the render-list cursor before/after (to catch list overflow = dropped geometry).
// A frame with no R-line for an entity = the render was skipped upstream (caller
// visibility conditions / task ordering).
static char (__cdecl *ff8_bgate_diag_render_orig)(void *) = nullptr;
static uint32_t ff8_bgate_diag_render_ri = 0;

char __cdecl ff8_bgate_diag_render_hook(void *slot)
{
	uint32_t cur_before = *(uint32_t *)0x1D8E054; // battle_texture_data_ptr (list cursor)
	unreplace_function(ff8_bgate_diag_render_ri);
	char r = ff8_bgate_diag_render_orig(slot);
	rereplace_function(ff8_bgate_diag_render_ri);
	if ((uint32_t)slot < FF8_BGATE_SLOT_BASE) // party entities live below the monster array
	{
		uint8_t *s = (uint8_t *)slot;
		ffnx_info("D f=%u R e=%p list=+%X vis=%08X mpal=%08X spal=%08X com=%08X\n",
			ff8_bgate_diag_frame, slot,
			*(uint32_t *)0x1D8E054 - cur_before,
			*(uint32_t *)(s + 0x7C),  // some_flag_data: per-object visibility bitmask
			*(uint32_t *)(s + 0x28),  // model_palette (RGB + ABE bit; fades)
			*(uint32_t *)(s + 0x2C),  // shadow_palette (0x32000000 = shadow skipped)
			*(uint32_t *)(s + 0x64)); // anim_header.comFileData (model swap detection)
	}
	return r;
}
#endif

// --- battle frame phase: bumped once per battle logic frame (BdLink_GF 0x500900) ---
static int (__cdecl *ff8_bgate_bdlink_orig)() = nullptr;
static uint32_t ff8_bgate_bdlink_ri = 0;

static void ff8_bgate_battle_reset();
static void ff8_bgate_move_restore_all();
static void ff8_bgate_cam_restore();
static void ff8_bgate_ent_frame_start();

// TEMP diagnostics (camera stuck after an effect): one line whenever the battle camera
// controller state changes. CURRENT_CAMERA_ANIMATION 0x1D97728, CAMERA_FLAG_RELATED
// 0x1D97704, cameraRelated_pointerAnimColl_flag/_2 0x1D97718/0x1D9771A,
// battle_to_update_flags 0x1D96A9C (bit 0x10 = magic effect owns the camera),
// C3_28_GF_data_pointer 0x1D96AAC (effect running).
static void ff8_bgate_camera_state_log()
{
	static uint32_t last[7] = {0xFFFFFFFF, 0, 0, 0, 0, 0, 0};
	static int32_t last_eye[3] = {0, 0, 0};
	uint32_t now[7] = {
		(uint32_t)*(uint16_t *)0x1D97728, (uint32_t)*(uint16_t *)0x1D97704,
		(uint32_t)*(uint8_t *)0x1D97718, (uint32_t)*(uint8_t *)0x1D9771A,
		*(uint32_t *)0x1D96A9C & 0x1F, (uint32_t)(*(uint32_t *)0x1D96AAC != 0),
		(uint32_t)*(uint16_t *)0x1D9771E }; // Battle_Camera_ReturnViewBlend
	int32_t eye[3] = { *(int16_t *)0xB8B7F0, *(int32_t *)0xB8B7F4, *(int16_t *)0xB8B7F2 };
	bool moved = abs(eye[0] - last_eye[0]) + abs(eye[1] - last_eye[1]) + abs(eye[2] - last_eye[2]) > 1500;
	if (memcmp(now, last, sizeof(now)) == 0 && !moved) return;
	memcpy(last, now, sizeof(now));
	memcpy(last_eye, eye, sizeof(eye));
	ffnx_info("30fps cam: f=%u ph=%d cur_anim=%04X cam_flag=%04X shot=%u coll2=%u upd_flags=%02X effect=%u retblend=%u eye=%d,%d,%d ret=%d,%d,%d%s\n",
		ff8_bgate_frame_no, ff8_bgate_phase, now[0], now[1], now[2], now[3], now[4], now[5], now[6],
		eye[0], eye[1], eye[2],
		(int)*(int16_t *)0xB8B800, (int)*(int32_t *)0xB8B804, (int)*(int16_t *)0xB8B802,
		moved ? " (moved)" : "");
}

// SCREEN FEEDBACK (Eden's / Ultima's... "ghost" of the whole screen, HUD included): an effect
// tick calls Battle_RequestScreenFeedback (0x47CF50, arg 0/1) which arms a one-frame request
// (0x1CFF6F4 = arg + 1). After the battle loop, battle_main_loop (0x47D1B3) consumes it: the
// screen rendered so far is captured into a texture and drawn back at 1/3 opacity (enlarged by
// 16px when the request is 2), then the request is cleared. Effects tick on real frames only, so
// on held frames nobody armed the request and the ghost vanished every other frame (the 15Hz
// "flash"). The request made during the last real frame is re-armed on the held frames.
static int ff8_bgate_feedback_req = 0;      // request armed during the current/last real frame
static bool ff8_bgate_feedback_rearm = true;
static bool ff8_bgate_gfc_f6 = true; // TEMP diagnostics: F6 = dedicated cinematic-GF redraw on/off
static uint32_t ff8_bgate_feedback_held = 0; // stats

int __cdecl ff8_bgate_feedback_request_hook(int mode)
{
	*(int *)0x1CFF6F4 = mode + 1;
	ff8_bgate_feedback_req = mode + 1;
	return mode + 1;
}

// TEMP diagnostics: F8 = save the next 60 presented frames as PNG (renderer.cpp burst capture)
#ifdef FF8_BATTLE_STANDALONE
static int ffnx_cap_left, ffnx_cap_seq, ffnx_cap_div;
static char ffnx_cap_label[96], ffnx_cap_dir[260];
#else
extern int ffnx_cap_left, ffnx_cap_seq, ffnx_cap_div;
extern char ffnx_cap_label[96], ffnx_cap_dir[260];
#endif
static int ff8_bgate_cap_burst = 0;

// END OF AN EFFECT, as the battle logic sees it. The action sequence ticks (BattleActionSequence_Tick_*, battle-stage
// queue 0x50090B: real frames only since ff8_bgate_stageq_gate, BEFORE the effect tick of the frame) wait for C3_28_GF_data_pointer (0x1D96AAC, via
// Magic_IsEffectOrCameraStillRunning) and g_HitEffectQueueBusy (0x1D96A90) to fall to 0. In vanilla the last tick of
// vanilla frame F clears them and the sequence sees it on frame F + 1. At 30 fps the last real tick is host frame 2F and
// the held frame 2F + 1 already saw the clear: everything the sequence does after the effect (camera return, next
// animation, next effect) started 1/30 s early, over the effect's last picture. The gates keep both signals up through
// the held frames after the last tick (the effect pointer: kept by the effect gate, released here at the next real frame
// before any queue runs; the busy flag: see ff8_bgate_eq_tick_gate), so the sequence sees the end at the vanilla time.
static uint32_t ff8_bgate_fx_end_hold = 0; // effect root queue that finished on the last real tick, kept in C3_28
static void ff8_bgate_fx_end_release()
{
	if (!ff8_bgate_fx_end_hold) return;
	if (*(uint32_t *)0x1D96AAC == ff8_bgate_fx_end_hold) *(uint32_t *)0x1D96AAC = 0; // (unless a new effect took its place)
	ff8_bgate_fx_end_hold = 0;
}

int __cdecl ff8_bgate_bdlink_hook()
{
	if (ff8_bgate_left_battle)
	{
		ff8_bgate_left_battle = false;
		ff8_bgate_battle_reset();
	}
	ff8_bgate_phase = (ff8_bgate_phase + 1) % ff8_bgate_n;
	ff8_bgate_frame_no++;
	_snprintf_s(ffnx_cap_label, sizeof(ffnx_cap_label), _TRUNCATE, "f%u_%s", ff8_bgate_frame_no, ff8_bgate_phase == 0 ? "REAL" : "held");
#ifndef FF8_BATTLE_STANDALONE // the F5-F9 test keys are not for players
	{
		static bool f8_down = false;
		// F8 = 60 full-resolution frames, F7 = 900 quarter-resolution frames (30 s, a whole summon)
		static bool f7_down = false;
		bool d7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
		bool long_burst = d7 && !f7_down;
		f7_down = d7;
		bool d8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
		if (((d8 && !f8_down) || long_burst) && ffnx_cap_left == 0)
		{
			_snprintf_s(ffnx_cap_dir, sizeof(ffnx_cap_dir), _TRUNCATE, "%s/capture30/burst%02d", basedir, ff8_bgate_cap_burst++);
			char cmd[300];
			_snprintf_s(cmd, sizeof(cmd), _TRUNCATE, "%s/capture30", basedir);
			CreateDirectoryA(cmd, NULL);
			CreateDirectoryA(ffnx_cap_dir, NULL);
			ffnx_cap_seq = 0;
			ffnx_cap_left = long_burst ? 900 : 60;
			ffnx_cap_div = long_burst ? 4 : 1;
			ffnx_info("capture: F7/F8 -> %d frames (1/%d res) to %s (n=%d bypass=%d mode=%d effect_id=%d)\n", ffnx_cap_left, ffnx_cap_div, ffnx_cap_dir, ff8_bgate_n, (int)ff8_bgate_fx_bypass, ff8_bgate_fx_mode, *(int *)0x1D99A68 + 1);
		}
		else if (long_burst && ffnx_cap_left > 1 && ffnx_cap_div > 1)
			ffnx_cap_left = 1; // F7 again = stop the long burst
		f8_down = d8;
		static bool f6_down = false;
		bool d6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
		if (d6 && !f6_down)
		{
			ff8_bgate_gfc_f6 = !ff8_bgate_gfc_f6;
			ffnx_info("30fps fx: F6 -> dedicated GF redraw on held frames %s\n", ff8_bgate_gfc_f6 ? "ON" : "OFF (generic 2D replay)");
		}
		f6_down = d6;
		static bool f5_down = false;
		bool d5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
		if (d5 && !f5_down)
		{
			ff8_bgate_fx_bypass = !ff8_bgate_fx_bypass;
			ffnx_info("30fps fx: F5 -> effect pacing %s\n", ff8_bgate_fx_bypass ? "BYPASSED (ticks every frame, 2x speed, no replay)" : "on (native pace + replay)");
		}
		f5_down = d5;
		static bool f9_down = false;
		bool down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
		if (down && !f9_down)
		{
			ff8_bgate_fx_mode = (ff8_bgate_fx_mode + 1) % 6;
			ffnx_info("30fps fx: F9 -> held-frame mode %d (%s)\n", ff8_bgate_fx_mode,
				ff8_bgate_fx_mode == 0 ? "plain replay / GF: camera only" : ff8_bgate_fx_mode == 1 ? "vertices / GF: + pure-op replay" :
				ff8_bgate_fx_mode == 2 ? "vertices + colors / GF: + midpoint state" : ff8_bgate_fx_mode == 3 ? "no replay (held frames draw nothing)" :
				ff8_bgate_fx_mode == 4 ? "replay without 15bpp prims" : "replay ONLY 15bpp prims");
		}
		f9_down = down;
	}
#endif
#if FF8_BGATE_DIAG
	ff8_bgate_diag_frame++;
#endif
	// the in-between camera written at the end of the last frame (for its view) is taken back
	// on EVERY frame: the frame's tasks always see the true camera words
	ff8_bgate_cam_restore();
	ff8_bgate_ent_frame_start();
	// held-frame position nudges are display-only: taken back on EVERY frame (the VM runs on held
	// frames too now and must read the true positions)
	ff8_bgate_move_restore_all();
	if (ff8_bgate_phase == 0)
	{
		ff8_bgate_feedback_req = 0; // re-armed by the effect if it still wants it this tick
		ff8_bgate_fx_end_release();
	}
	else if (ff8_bgate_feedback_req && !ff8_bgate_fx_bypass && ff8_bgate_feedback_rearm)
	{
		*(int *)0x1CFF6F4 = ff8_bgate_feedback_req;
		ff8_bgate_feedback_held++;
	}
	// slow battle frames (a stall the player sees): host frame time and the BdLink share, a few lines per battle
	static LARGE_INTEGER qpf = {}, last_entry = {};
	static int slow_lines = 0;
	if (!qpf.QuadPart) QueryPerformanceFrequency(&qpf);
	LARGE_INTEGER t0;
	QueryPerformanceCounter(&t0);
	if (ff8_bgate_frame_no <= 2) slow_lines = 0;
	const double frame_ms = last_entry.QuadPart ? 1000.0 * double(t0.QuadPart - last_entry.QuadPart) / double(qpf.QuadPart) : 0.0;
	last_entry = t0;
	unreplace_function(ff8_bgate_bdlink_ri);
	int r = ff8_bgate_bdlink_orig();
	rereplace_function(ff8_bgate_bdlink_ri);
	{
		LARGE_INTEGER t1;
		QueryPerformanceCounter(&t1);
		const double bdlink_ms = 1000.0 * double(t1.QuadPart - t0.QuadPart) / double(qpf.QuadPart);
		if ((frame_ms > 100.0 || bdlink_ms > 50.0) && slow_lines < 20)
		{
			slow_lines++;
			ffnx_info("battle 30fps: slow host frame f=%u ph=%d: %.1f ms since the previous BdLink, this BdLink %.1f ms (in action %d, result %d, loading %d, flow %u)\n",
				ff8_bgate_frame_no, ff8_bgate_phase, frame_ms, bdlink_ms, (int)*(uint8_t *)0x1D27B00, (int)*(uint8_t *)0x1CFF6E7,
				*(int32_t *)0x1D999C8, *(uint32_t *)0x1D27B04);
		}
	}
	ff8_bgate_camera_state_log();
	// (FADE_OUT_END_BATTLE_DURATION 0x1D27B0C: decremented by the battle loop after its BdLink, which runs on real
	// frames only: ff8_bgate_battleloop_thunk)
#if FF8_BGATE_DIAG
	{
		// final camera position after updateBattleCamera ran (world XZ packed s16 pair + Y),
		// plus the task functions currently queued on the camera task queue
		int16_t *wxz = (int16_t *)0xB8B7F0;
		int32_t wy = *(int32_t *)0xB8B7F4;
		char tasks[128]; tasks[0] = 0;
		uint32_t node = *(uint32_t *)0x1D97768; // BD_LINK_TASK_HEADER_CAMERA.head
		int guard = 0;
		while (node > 0x10000 && node < 0x7F000000 && guard++ < 8)
		{
			char one[24];
			sprintf(one, " %08X", *(uint32_t *)(node + 8)); // TaskNodeHeader.task_func
			strcat(tasks, one);
			node = *(uint32_t *)(node + 4); // next
		}
		// C3_28_GF_data_pointer: non-zero while a magic/GF effect is running. Squall's cast
		// sequence waits on it (C3 28). If it never clears, the effect never completed.
		uint32_t c328 = *(uint32_t *)0x1D96AAC;
		uint32_t qhead = 0;
		if (c328 > 0x10000 && c328 < 0x7F000000) qhead = *(uint32_t *)c328; // queue head
		ffnx_info("D f=%u CAMPOS x=%d z=%d y=%d c328=%08X qhead=%08X tasks:%s\n",
			ff8_bgate_diag_frame, (int)wxz[0], (int)wxz[1], wy, c328, qhead, tasks);
	}
#endif
	return r;
}

// --- UI tick reduction: 4 ticks/frame -> 2 ticks/frame (= native 60 ticks/s) ---
// battle_cardgame_main_loop calls the pair 3x with menu rendering DISABLED
// (menu_rendering_enabled @0x1D6D4AC == 0, the hidden catch-up ticks) then 1x
// enabled (the visible tick). Skip hidden ticks #0 and #1, keep #2 + visible.
// The pair is called from the main loop only, always update-then-display, so the
// skip decision made in the update hook is consumed by the display hook.
#define FF8_BGATE_MENU_RENDERING_ENABLED (*(uint32_t *)0x1D6D4AC)

static int (__cdecl *ff8_bgate_hudupdate_orig)() = nullptr;
static int (__cdecl *ff8_bgate_huddisplay_orig)() = nullptr;
static uint32_t ff8_bgate_hudupdate_ri = 0, ff8_bgate_huddisplay_ri = 0;
static bool ff8_bgate_tick_skip = false;
static int ff8_bgate_hidden_idx = 0;

int __cdecl ff8_bgate_hudupdate_hook()
{
	if (FF8_BGATE_MENU_RENDERING_ENABLED == 0)
	{
		// Hidden catch-up tick. Native pace needs 4/n ticks per rendered frame, and the visible
		// tick already provides one, so keep the LAST (4/n - 1) of the 3 hidden ticks and drop
		// the rest (n=2 -> keep #2 only; n=4 -> keep none, the visible tick alone is enough).
		ff8_bgate_tick_skip = (ff8_bgate_hidden_idx++ < 3 - (4 / ff8_bgate_n - 1));
	}
	else
	{
		// visible tick: always runs, and resyncs the hidden counter
		ff8_bgate_hidden_idx = 0;
		ff8_bgate_tick_skip = false;
	}
	if (ff8_bgate_tick_skip)
		return 0;
	unreplace_function(ff8_bgate_hudupdate_ri);
	int r = ff8_bgate_hudupdate_orig();
	rereplace_function(ff8_bgate_hudupdate_ri);
	// HUD blink/pulse counter (BattleUI ctx+0x2C, ++ at 0x4A8E9B) advances only on the
	// fresh-input latch tick, i.e. once per rendered frame: 15/s vanilla, 30/s here - the
	// limit-break arrow, blinking command text, list page arrows, active character name /
	// ATB pulse and the countdown's red flash all ran 2x fast. Keep 1 advance in n.
	uint8_t *ctx = *(uint8_t **)0x1D6D490; // BattleUI_CtxPtr
	if (ctx && ctx[33]) // ctx+33 = the latch fired on this tick
	{
		static uint32_t latches = 0;
		if ((latches++ % ff8_bgate_n) != 0)
			ctx[0x2C]--;
	}
	return r;
}

int __cdecl ff8_bgate_huddisplay_hook()
{
	if (ff8_bgate_tick_skip)
		return 0;
	unreplace_function(ff8_bgate_huddisplay_ri);
	int r = ff8_bgate_huddisplay_orig();
	rereplace_function(ff8_bgate_huddisplay_ri);
	return r;
}

// --- GF Boost phases at native pacing ---
// computeGFBoost_ (0x56DD70, once per UI tick) counts its safe/danger phases and total window
// (word_209CEF6 / word_209CEF4, kernel gfBoostParams x15 units) down only on ticks where
// BattleUI ctx+33 ("fresh input frame") is set. That latch is armed once per BATTLE frame by
// BdLink - the same engine logic on the PSX, whose battle also ran ~15 frames/s - so the
// native pace is 15 units/s on both machines (only the pad SAMPLING differed: 60 Hz on PSX,
// restored here by the mid-frame read). At 30fps the latch fires 30/s, which made the phases
// and the whole Boost window 2x short: let only 1 latch in n reach the countdown.
// (A first attempt counted every UI tick = 60/s "to match the PSX" - 4x too fast: the
// "phases 4x longer than PSX" note it was based on was wrong.)
static void (__cdecl *ff8_bgate_boost_orig)() = nullptr;
static uint32_t ff8_bgate_boost_ri = 0;

// TEMP diagnostics: where do Boost's button-press edges come from? A press edge seen on the
// FIRST UI tick of a frame can only come from the mid-frame read (the regular read happens
// between the two ticks); edges on the second tick come from the regular read.
static struct { uint32_t first, second, calls, mid0; bool active; } ff8_bgate_boost_diag;

void __cdecl ff8_bgate_boost_hook()
{
	uint8_t *ctx = *(uint8_t **)0x1D6D490; // BattleUI_CtxPtr
	{
		auto &d = ff8_bgate_boost_diag;
		if (!d.active) { memset(&d, 0, sizeof(d)); d.active = true; d.mid0 = ff8_bgate_mid_reads; }
		d.calls++;
		int pressed = ((int(__cdecl *)(int))0x4A8420)(0); // read_pad_pressed_remapped
		if (pressed & 0xF0) // any face button edge (Boost uses Square = 0x80 after remap)
		{
			if (*(uint32_t *)0x1D6D4AC == 0) d.first++; // menu rendering off = hidden = 1st tick
			else d.second++;
		}
		if (*(uint8_t *)0x209CEFB == 6) // Boost state: finished
		{
			ffnx_info("30fps boost: face-button edges on 1st tick (mid-frame read)=%u, on 2nd tick (regular read)=%u | ui ticks=%u mid-frame reads=%u\n",
				d.first, d.second, d.calls, ff8_bgate_mid_reads - d.mid0);
			d.active = false;
		}
	}
	uint8_t saved = ctx ? ctx[33] : 0;
	if (ctx && ctx[33])
	{
		static uint32_t latches = 0;
		if ((latches++ % ff8_bgate_n) != 0)
			ctx[33] = 0; // this latch does not count for the phase timers
	}
	unreplace_function(ff8_bgate_boost_ri);
	ff8_bgate_boost_orig();
	rereplace_function(ff8_bgate_boost_ri);
	if (ctx) ctx[33] = saved;
}

// --- Renzokuken / gunblade trigger clock at native pace ---
// Renzokuken_UpdateTrigger (0x4BA6C0, BattleUI window 6 callback, once per UI tick) runs the trigger
// clock as Renzokuken_ClockMajor (0x1D76798) += 4 on each fresh-input latch (BattleUI ctx+33) plus
// Renzokuken_ClockSub (0x1D7679A) +1 per UI tick in between (0..3). The latch fires once per rendered
// frame, 30/s here: the bar ran 2x fast and the clock skipped values (the auto-crit test v6 == 25 could
// be missed). As for GF Boost, only 1 latch in n counts: 60 UI ticks/s with a latch every 4 ticks, the
// vanilla clock tick for tick.
static unsigned char (__cdecl *ff8_bgate_renzo_orig)() = nullptr;
static uint32_t ff8_bgate_renzo_ri = 0;

unsigned char __cdecl ff8_bgate_renzo_hook()
{
	uint8_t *ctx = *(uint8_t **)0x1D6D490; // BattleUI_CtxPtr
	uint8_t saved = ctx ? ctx[33] : 0;
	if (ctx && ctx[33])
	{
		static uint32_t latches = 0;
		if ((latches++ % ff8_bgate_n) != 0)
			ctx[33] = 0; // this latch does not move the clock
	}
	unreplace_function(ff8_bgate_renzo_ri);
	unsigned char r = ff8_bgate_renzo_orig();
	rereplace_function(ff8_bgate_renzo_ri);
	if (ctx) ctx[33] = saved;
	return r;
}

// --- game time: called 4x per loop iteration unconditionally, so it scales with the host
// frame rate (120/s at 30fps, 240/s at 60fps) -> keep 1-in-n while in battle = native 60/s ---
static int (__cdecl *ff8_bgate_savemap_tick_orig)() = nullptr;
static uint32_t ff8_bgate_savemap_tick_ri = 0;

int __cdecl ff8_bgate_savemap_tick_hook()
{
	struct game_mode *mode = getmode_cached();
	if (mode->driver_mode == MODE_BATTLE)
	{
		static uint32_t n = 0;
		if ((n++ % ff8_bgate_n) != 0)
			return 0; // held: keep game time / battle countdown at native 60 ticks/s
	}
	unreplace_function(ff8_bgate_savemap_tick_ri);
	int r = ff8_bgate_savemap_tick_orig();
	rereplace_function(ff8_bgate_savemap_tick_ri);
	return r;
}

/// --- battle model animation: CONVERTED DATA, read at the data rate (30 Hz) ---
// With 30 fps on, every battle data file is assumed converted to 30 fps by the FF8UltimateEditor
// tools (no marker, no detection, no fallback; study DAT30FPS_STUDY.md 7.2 / 12): every entity and
// weapon animation has exactly 2n frames for the n it had, and the sequences (section 5) count
// ticks of 1/30 s. So the reader and the AnimSeq VM simply run at the data rate:
//   ff8_bgate_data_div = host frames per 30 Hz data tick (1 at 30 fps, 2 at 60 fps).
// Battle_ReadAnimation (0x508F90) consumes a bit-packed DELTA stream (frame 0 absolute, later frames
// deltas): an anim_cmd held for k host frames shows the exact look-ahead midpoint pose
// (ff8_bgate_pose_lookahead) on its held calls.
//
//   status   vanilla 15 Hz data       converted data, FFNx
//   normal   plain                    plain, 1 read per data tick
//   Haste    FAST (engine)            FAST, untouched
//   Slow     SLOW: total = 2N-1       SLOW cleared, each frame held 2 data ticks (user decision
//                                     10-22: no byte wrap of total_frames, no 128-frame limit)
//
// Entity / weapon anim_cmds get their policy at every start (Battle_QueueAnimation 0x509520 ->
// pre_Battle_ReadAnimation 0x509440). Unknown anim_cmds (stage models read by
// BattleStage_AnimateAndRenderGroups 0x50102E, effect models read inside the 15 Hz effect tick)
// keep the former rule: read on real frames (phase 0), look-ahead midpoint on held frames.
#define FF8_BGATE_ANIM_FLAG_SLOW 0x01
#define FF8_BGATE_ANIM_FLAG_FAST 0x02

static int ff8_bgate_data_div = 1; // host frames per 30 Hz data tick (set at install: n / 2)

// a 30 Hz data tick (AnimSeq VM, camera player, VM-created tasks run on it)
static inline bool ff8_bgate_data_tick() { return (ff8_bgate_phase % ff8_bgate_data_div) == 0; }

// Per anim_cmd: hold divisor (reader calls per animation frame) and the calls made since the
// animation started; slow = engine Slow status emulated by the hold (the C3 frame variables report
// the doubled counts the sequences expect then).
struct ff8_bgate_anim_policy_t { void *cmd; int div; bool slow; uint32_t calls; };
static ff8_bgate_anim_policy_t ff8_bgate_anim_policy[32];

static void ff8_bgate_anim_policy_set(void *cmd, int div, bool slow)
{
	int free_slot = -1;
	for (int i = 0; i < 32; i++)
	{
		if (ff8_bgate_anim_policy[i].cmd == cmd) { free_slot = i; break; }
		if (!ff8_bgate_anim_policy[i].cmd && free_slot < 0) free_slot = i;
	}
	if (free_slot >= 0)
		ff8_bgate_anim_policy[free_slot] = { cmd, div, slow, 0 };
}

static ff8_bgate_anim_policy_t *ff8_bgate_anim_policy_find(void *cmd)
{
	for (int i = 0; i < 32; i++)
		if (ff8_bgate_anim_policy[i].cmd == cmd) return &ff8_bgate_anim_policy[i];
	return nullptr;
}

static int ff8_bgate_anim_policy_get(void *cmd)
{
	const ff8_bgate_anim_policy_t *p = ff8_bgate_anim_policy_find(cmd);
	return p ? p->div : ff8_bgate_n; // unknown anim_cmd (stage / effect model): hold 1-in-n
}

// --- AnimSeq C3 special vars 0x09/0x0A under the emulated Slow status ---
// AnimSeq_ReadSpecialVar_C3 (0x5044B0): var 0x0A = anim_cmd->total_frames, var 0x09 =
// anim_cmd->current_frame. Vanilla Slow doubles total_frames (2N-1) and counts current_frame in
// calls, so "C3 0A E5 7F ... decrement per VM run" countdowns last the slowed animation. The Slow
// emulation keeps the anim_cmd undoubled: report data ticks instead (0A = 2N, 09 = 1 + data ticks
// since the start, capped), so those loops still last exactly the held animation. Without Slow the
// values are the engine's, untouched.
// CRITICAL - the signature must be int/int, NOT uint16_t: the function returns SIGN-EXTENDED 32-bit
// values in EAX (movsx at 0x5044D0 / 0x5044ED / 0x50455E ...) and the interpreter consumes the full
// EAX; a uint16_t wrapper turns every negative variable into a huge positive one.
#define FF8_BGATE_ACTIVE_ANIM_CMD (*(uint8_t **)0x1D981F8)

static int (__cdecl *ff8_bgate_c3var_orig)(int) = nullptr;
static uint32_t ff8_bgate_c3var_ri = 0;

int __cdecl ff8_bgate_c3var_hook(int var)
{
	unreplace_function(ff8_bgate_c3var_ri);
	int r = ff8_bgate_c3var_orig(var);
	rereplace_function(ff8_bgate_c3var_ri);
	if (var == 0x09 || var == 0x0A)
	{
		const ff8_bgate_anim_policy_t *p = ff8_bgate_anim_policy_find(FF8_BGATE_ACTIVE_ANIM_CMD);
		if (p && p->slow)
		{
			const int total = 2 * (int)FF8_BGATE_ACTIVE_ANIM_CMD[7];
			const int cur = (int)(p->calls / (uint32_t)ff8_bgate_data_div) + 1;
			r = (var == 0x0A) ? total : (cur < total ? cur : total);
		}
	}
	return r;
}

// Battle_QueueAnimation (0x509520) is the ONLY entity/weapon animation starter (AnimSeq VM
// opcode < 0x80, opcode A0, idle restart) and the place where the engine applies the
// Slow/Haste status to the flags. Our policy must be applied to exactly the two
// pre_Battle_ReadAnimation calls it makes (weapon then entity) - hence this scope marker.
static bool ff8_bgate_inside_queue_anim = false;
static int (__cdecl *ff8_bgate_queueanim_orig)(void *, int) = nullptr;
static uint32_t ff8_bgate_queueanim_ri = 0;

int __cdecl ff8_bgate_queueanim_hook(void *entity_slot, int opcode)
{
#if FF8_BGATE_DIAG
	ffnx_info("D f=%u p=%d s=%d e=%p QUEUE anim=%02X\n",
		ff8_bgate_diag_frame, ff8_bgate_phase, FF8_BGATE_SLOT_IDX(entity_slot), entity_slot, opcode & 0xFFFF);
#endif
	ff8_bgate_inside_queue_anim = true;
	unreplace_function(ff8_bgate_queueanim_ri);
	int r = ff8_bgate_queueanim_orig(entity_slot, opcode);
	rereplace_function(ff8_bgate_queueanim_ri);
	ff8_bgate_inside_queue_anim = false;
	return r;
}

// pre_Battle_ReadAnimation resets the anim state and doubles total_frames if SLOW is set, so the
// policy is written into the flags BEFORE the original body runs. Called from anywhere else (stage
// load, effect model animators) the flags are left untouched.
static int (__cdecl *ff8_bgate_preread_orig)(void *, void *, int) = nullptr;
static uint32_t ff8_bgate_preread_ri = 0;
static bool ff8_bgate_inside_preread = false;

int __cdecl ff8_bgate_preread_hook(void *anim_header, void *anim_cmd, int animID)
{
	if (ff8_bgate_inside_queue_anim)
	{
		uint8_t *flags = (uint8_t *)anim_cmd + 1;
		const bool slow = (*flags & FF8_BGATE_ANIM_FLAG_SLOW) != 0;
		// Slow status: no engine doubling, every frame held 2 data ticks instead; FAST unchanged
		if (slow) *flags &= (uint8_t)~FF8_BGATE_ANIM_FLAG_SLOW;
		ff8_bgate_anim_policy_set(anim_cmd, slow ? 2 * ff8_bgate_data_div : ff8_bgate_data_div, slow);
	}
	// the original reads frame 0 itself (0x5094B7 -> 0x508F90): that read is never held nor counted
	ff8_bgate_inside_preread = true;
	unreplace_function(ff8_bgate_preread_ri);
	int r = ff8_bgate_preread_orig(anim_header, anim_cmd, animID);
	rereplace_function(ff8_bgate_preread_ri);
	ff8_bgate_inside_preread = false;
	return r;
}

// --- AB (Renzokuken / gunblade trigger): getAnimationFrameCount 0x5094D0, only caller AB 0x504EA2 ---
// The R1 windows (0x4BA4C0: 4*cumsum - 27, against a clock +4 per 15 Hz tick, kept at native pace by
// the renzo hook) are built from the frame counts of the AB list: a converted animation has 2n frames
// for the n vanilla had, so the native count is (2n + 1) / 2 = n (study 1.4, user decision 10-21 (4)).
int __cdecl ff8_bgate_framecount_hook(uint8_t *entity, int animation_id)
{
	const uint8_t *com = *(const uint8_t *const *)(entity + 0x64); // anim_header.comFileData
	const uint8_t *sec3 = *(const uint8_t *const *)(com + 8);     // comFileModelAnimationSection3
	const int frames = sec3[*(const uint32_t *)(sec3 + 4 + 4 * animation_id)];
	return (frames + 1) / 2;
}

static int (__cdecl *ff8_bgate_readanim_orig)(void *, void *) = nullptr;
static uint32_t ff8_bgate_readanim_ri = 0;

// Safety net: a fault while decoding an animation bitstream (stream read past its end)
// finishes that animation instead of taking the game down - the engine then does what it
// does at any normal completion (requeue/next animation). Logged with everything needed to
// find the cause. anim_cmd: +0 anim id, +1 flags, +6 current frame, +7 total frames.
static int ff8_bgate_readanim_guarded(void *header, void *anim_cmd)
{
	__try
	{
		return ff8_bgate_readanim_orig(header, anim_cmd);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		uint8_t *c = (uint8_t *)anim_cmd;
		const ff8_bgate_anim_policy_t *p = ff8_bgate_anim_policy_find(anim_cmd);
		ffnx_info("battle 30fps: animation read FAULTED (header=%p anim_cmd=%p anim=%u flags=%02X frame=%u/%u policy div=%d slow=%d) -> animation finished early\n",
			header, anim_cmd, (unsigned)c[0], (unsigned)c[1], (unsigned)c[6], (unsigned)c[7],
			ff8_bgate_anim_policy_get(anim_cmd), p ? (int)p->slow : 0);
		c[6] = c[7]; // complete
		return 1;    // READ_ANIMATION_RETURN_ANIMATION_COMPLETE
	}
}

static inline int ff8_bgate_scale_round(int v, int num, int den);

// --- held-frame pose extrapolation for animations read 1-in-div (real Slow status,
// stage models, any anim_cmd with a hold divisor) ---
// A pose is the skeleton section of the model (ComFileSectionSkeleton, reached through
// BattleAnimHeader->comFileData->[0]): root position int16 x/y/z at +8/+10/+12, then
// nbBones (+0) bones of 48 bytes from +16 with rotX/Y/Z at +4/+6/+8 (4096 = full turn)
// and, when per_bone_scale_flag (+1) is set, scaleX/Y/Z at +10/+12/+14.
// ProcessFieldEntitiesTransformation (0x508C90) turns that pose into the bone matrices.
// After every real read the pose is recorded (prev/cur); a held frame writes
// cur + (cur - prev) * k/div into the skeleton (rotations the short way round), builds
// the matrices from it, then puts cur back - the stream reader works in deltas on top of
// the skeleton, so the next real read must find the exact real pose again.
// the skeleton's bone count is a byte: cover all of them (Shiva has 69; a model over the old
// limit of 64 silently held its pose on every held frame)
#define FF8_BGATE_POSE_BONES 256
#define FF8_BGATE_POSE_SLOTS 64
struct ff8_bgate_pose_t
{
	int16_t root[3];
	int16_t v[FF8_BGATE_POSE_BONES][6]; // rot x/y/z, scale x/y/z
};
static uint8_t *ff8_bgate_skeleton(void *header)
{
	uint8_t *com = *(uint8_t **)((uint8_t *)header + 4); // BattleAnimHeader.comFileData
	return com ? *(uint8_t **)com : nullptr;            // -> comFileSkeletonSection1
}

static void ff8_bgate_pose_read(const uint8_t *sk, int nb, bool scaled, ff8_bgate_pose_t &p)
{
	memcpy(p.root, sk + 8, 6);
	for (int b = 0; b < nb; b++)
	{
		const uint8_t *bone = sk + 16 + 48 * b;
		memcpy(p.v[b], bone + 4, scaled ? 12 : 6);
	}
}

static void ff8_bgate_pose_write(uint8_t *sk, int nb, bool scaled, const ff8_bgate_pose_t &p)
{
	memcpy(sk + 8, p.root, 6);
	for (int b = 0; b < nb; b++)
	{
		uint8_t *bone = sk + 16 + 48 * b;
		memcpy(bone + 4, p.v[b], scaled ? 12 : 6);
	}
}

// Held frame of an animation read once per logic tick: run the engine's own reader one frame
// ahead on the real state, keep the pose it produces, put the animation command and the
// whole skeleton section (pose + bone matrices) back, then build the bone matrices from the
// exact midpoint pose (angles the short way round) and restore the real pose values. The
// reader is a pure function of the animation command and the skeleton (its bit cursor lives
// in a scratch block rebuilt from the command on every call), so the next real read finds
// exactly the vanilla state. A completed animation has no next frame in its own stream (the
// sequence script decides what plays next): it holds its last pose for that half tick, as
// vanilla shows it until the next tick.
// Per-bone scales: every frame of the stream carries its own scale flag (skeleton +1 bit 0,
// written by the reader at 0x50905E / 0x509169; set: the frame's scales follow its rotations,
// as absolute values). BattleModel_BuildBoneMatricesFromPose builds an unscaled pose exactly as
// the same pose with the flag set and every scale 1024 (bit-identical bone matrices on every
// frame of every battle model: work\posemid oracle), so the flag only says "all scales 1024":
// the in-between of a frame pair whose flag changes is the scaled pose with the unscaled side's
// scales taken as 1024 (no N / N+1 pick needed; the flag is never half way, the scales are).
// At fraction 1 the next frame's pose is built exactly as the reader left it (its flag, its
// own spelling of every angle): the bone matrices are the next tick's, bit for bit.
static uint8_t ff8_bgate_la_skel_save[16 + 48 * FF8_BGATE_POSE_BONES];

static void ff8_bgate_pose_blend(void *header, uint8_t *sk, int nb, const ff8_bgate_pose_t &cur, bool cur_scaled, const ff8_bgate_pose_t &next, bool next_scaled, int k, int div);

static void ff8_bgate_pose_lookahead(void *header, void *anim_cmd, int k, int div)
{
	uint8_t *sk = ff8_bgate_skeleton(header);
	uint8_t *cmd = (uint8_t *)anim_cmd;
	if (!sk || sk[0] == 0 || sk[0] > FF8_BGATE_POSE_BONES || cmd[6] >= cmd[7])
	{
		((void(__cdecl *)(void *))0x508C90)(header); // plain hold
		return;
	}
	int nb = sk[0];
	uint32_t size = 16 + 48 * nb;
	uint8_t cmd_save[8];
	memcpy(cmd_save, cmd, 8);
	memcpy(ff8_bgate_la_skel_save, sk, size);
	static ff8_bgate_pose_t cur, next;
	const bool cur_scaled = (sk[1] & 1) != 0;
	ff8_bgate_pose_read(sk, nb, true, cur); // scale words too, whatever the flag (put back as they were)
	unreplace_function(ff8_bgate_readanim_ri);
	ff8_bgate_readanim_guarded(header, anim_cmd);
	rereplace_function(ff8_bgate_readanim_ri);
	const bool next_scaled = (sk[1] & 1) != 0; // the flag of the frame just read
	ff8_bgate_pose_read(sk, nb, true, next);
	memcpy(sk, ff8_bgate_la_skel_save, size);
	memcpy(cmd, cmd_save, 8);
	ff8_bgate_pose_blend(header, sk, nb, cur, cur_scaled, next, next_scaled, k, div);
}

// The in-between pose of cur and next at k / div (fraction 1 = next exactly, its flag and spelling), bone matrices built from
// it, then the real pose values put back (cur, its flag). Shared by the look-ahead above and pose_midpoint_to.
static void ff8_bgate_pose_blend(void *header, uint8_t *sk, int nb, const ff8_bgate_pose_t &cur, bool cur_scaled, const ff8_bgate_pose_t &next, bool next_scaled, int k, int div)
{
	static ff8_bgate_pose_t mid;
	const uint8_t flag_save = sk[1];
	bool mid_scaled;
	if (k >= div)
	{
		mid = next; // fraction 1: the next frame as read
		mid_scaled = next_scaled;
	}
	else
	{
		mid_scaled = cur_scaled || next_scaled;
		for (int a = 0; a < 3; a++)
			mid.root[a] = (int16_t)(cur.root[a] + ff8_bgate_scale_round(next.root[a] - cur.root[a], k, div));
		for (int b = 0; b < nb; b++)
		{
			// a bone rotation Ry(-y) Rx(-x) Rz(z) is also written (2048 - x, y + 2048, z + 2048): the
			// animation data switches between the two near x = +-1024, and blending the numbers across
			// such a switch turns the bone half way round; blend toward the spelling of the next pose
			// nearer the current one (each angle the shortest way round)
			int d[3], e[3], sd = 0, se = 0;
			for (int a = 0; a < 3; a++)
			{
				d[a] = ((next.v[b][a] - cur.v[b][a] + 2048) & 4095) - 2048;
				sd += d[a] < 0 ? -d[a] : d[a];
			}
			e[0] = ((2048 - next.v[b][0] - cur.v[b][0] + 2048) & 4095) - 2048;
			e[1] = ((next.v[b][1] - cur.v[b][1]) & 4095) - 2048;
			e[2] = ((next.v[b][2] - cur.v[b][2]) & 4095) - 2048;
			for (int a = 0; a < 3; a++) se += e[a] < 0 ? -e[a] : e[a];
			const int *dd = se < sd ? e : d;
			for (int a = 0; a < 3; a++)
				mid.v[b][a] = (int16_t)(cur.v[b][a] + ff8_bgate_scale_round(dd[a], k, div));
			if (mid_scaled)
				for (int a = 3; a < 6; a++)
				{
					const int c = cur_scaled ? cur.v[b][a] : 1024, n = next_scaled ? next.v[b][a] : 1024;
					mid.v[b][a] = (int16_t)(c + ff8_bgate_scale_round(n - c, k, div));
				}
		}
	}
	sk[1] = (uint8_t)((sk[1] & ~1) | (mid_scaled ? 1 : 0));
	ff8_bgate_pose_write(sk, nb, mid_scaled, mid);
	((void(__cdecl *)(void *))0x508C90)(header); // BattleModel_BuildBoneMatricesFromPose
	sk[1] = flag_save;
	ff8_bgate_pose_write(sk, nb, true, cur);
}


namespace ff8fx
{
	// Ported effects draw a battle entity "at its in-between pose" on held frames. An entity / weapon
	// animation read at the data rate (policy div 1) already shows its own pose on every host frame:
	// its in-between pose IS the current one (only the prediction at fraction 1, the next read, moves).
	void pose_midpoint(void *anim_header, void *anim_cmd, int num, int den)
	{
		const ff8_bgate_anim_policy_t *p = ff8_bgate_anim_policy_find(anim_cmd);
		if (p && p->div == 1 && num < den)
		{
			((void(__cdecl *)(void *))0x508C90)(anim_header); // BattleModel_BuildBoneMatricesFromPose
			return;
		}
		ff8_bgate_pose_lookahead(anim_header, anim_cmd, num, den);
	}

	// the in-between pose of the skeleton's current pose and the pose held by next_skeleton (a copy of the same skeleton after
	// whatever the next tick does to it: an effect-model anim advance that loops or restarts, a pose drawn earlier, ...)
	void pose_midpoint_to(void *anim_header, const void *next_skeleton, int num, int den)
	{
		uint8_t *sk = ff8_bgate_skeleton(anim_header);
		const uint8_t *nx = (const uint8_t *)next_skeleton;
		if (!sk || !nx || sk[0] == 0 || sk[0] > FF8_BGATE_POSE_BONES || nx[0] != sk[0])
		{
			((void(__cdecl *)(void *))0x508C90)(anim_header); // plain hold
			return;
		}
		const int nb = sk[0];
		static ff8_bgate_pose_t cur, next;
		ff8_bgate_pose_read(sk, nb, true, cur);
		ff8_bgate_pose_read(nx, nb, true, next);
		ff8_bgate_pose_blend(anim_header, sk, nb, cur, (sk[1] & 1) != 0, next, (nx[1] & 1) != 0, num, den);
	}
}

int __cdecl ff8_bgate_readanim_hook(void *header, void *anim_cmd)
{
	uint8_t *cmd = (uint8_t *)anim_cmd;
	if (ff8_bgate_inside_preread)
	{
		// frame 0 of an animation start (pre_Battle_ReadAnimation): always read, never counted
		unreplace_function(ff8_bgate_readanim_ri);
		int r0 = ff8_bgate_readanim_guarded(header, anim_cmd);
		rereplace_function(ff8_bgate_readanim_ri);
		return r0;
	}
	ff8_bgate_anim_policy_t *p = ff8_bgate_anim_policy_find(anim_cmd);
	if (p)
	{
		// entity / weapon animation (converted data): every call is a read, except under the emulated
		// Slow status (or at 60 fps) where each frame is held div calls (look-ahead midpoint on the held
		// calls, the last frame held too: the animation lasts exactly div x its frame count)
		p->calls++;
		if (p->div > 1 && (p->calls % (uint32_t)p->div) != 0)
		{
			if (cmd[6] != 0 && cmd[6] < cmd[7]) ff8_bgate_pose_lookahead(header, anim_cmd, (int)(p->calls % (uint32_t)p->div), p->div);
			else ((void(__cdecl *)(void *))0x508C90)(header); // plain hold (frame 0 never held: no T-pose)
			return 0; // "frame processed, not complete" -> animation continues
		}
	}
	// COMPLETED animation: the original early-outs (return 1) WITHOUT rebuilding the double-buffered
	// geometry; a completion surfacing on a frame where nothing requeues leaves it stale for one frame.
	// Rebuild before returning (harmless when the VM requeues in the same call, as vanilla does).
	if (cmd[6] >= cmd[7])
	{
		((void(__cdecl *)(void *))0x508C90)(header); // BattleModel_BuildBoneMatricesFromPose
		return 1; // READ_ANIMATION_RETURN_ANIMATION_COMPLETE (same as the early-out)
	}
	if (!p)
	{
		// stage / effect model (not converted, or read inside the 15 Hz effect tick, where phase is 0):
		// read on real frames, the look-ahead midpoint on held frames (former behaviour)
		const int div = ff8_bgate_n;
		if (div > 1 && cmd[6] != 0 && (ff8_bgate_phase % div) != 0)
		{
			ff8_bgate_pose_lookahead(header, anim_cmd, ff8_bgate_phase % div, div);
			return 0;
		}
	}
	unreplace_function(ff8_bgate_readanim_ri);
	int r = ff8_bgate_readanim_guarded(header, anim_cmd);
	rereplace_function(ff8_bgate_readanim_ri);
	return r;
}

/// AnimSeq_UpdateEntityPerFrame (0x504290) runs the entity's CHOREOGRAPHY VM (section 5: run-up
// movement, animation transitions, frame delays, sound / effect triggers, background 9A sequences)
// and advances the skeletal frame (AdvanceAnimationBy1AndCheckCompletion 0x5094F0). Converted data:
// it runs on every 30 Hz data tick (every host frame at 30 fps), exactly as vanilla runs it on its
// data's 15 Hz tick. The sequences of a converted file count 30 Hz ticks themselves (waits and
// counters x2, steps /2, 9A bodies self-gated to 15 Hz by a toggle variable), so nothing is paced
// here. Tasks created while it runs are tagged as VM tasks (ff8_bgate_vmtask_*). Effect launches:
// the handshake flags the VM sets (cast ready 0x10 / fire 0x20 / done 0x200, C3 0x28) are levels the
// 15 Hz effect code polls on its real tick: a flag set on a held frame starts the effect at the next
// real frame (study 7.3, user decision 10-22).
static bool ff8_bgate_in_vm = false;
static int (__cdecl *ff8_bgate_animseq_upd_orig)(void *) = nullptr;
static uint32_t ff8_bgate_animseq_upd_ri = 0;

// --- held frames: battle entities moved by a natively ported effect (fx_held.h HeldEntity) ---
// When does an entity move reach the screen? One battle frame (BdLink 0x500900): the effect tick
// (0x50093A) writes the entity's fields, then the entity queue (0x500971) runs each entity's task
// (0x502AB0): RENDER first (BS_RenderBattleEntity 0x502D40, with the +0x40 matrix the END of the
// previous frame built), then the animation update, then the matrix rebuild from the fields (rotation
// +0x0C, position +0x1C + height +0x22, state offset *(+0x74)+0x24, root bone offset, scales). So the
// real frame of tick N draws the fields of tick N-1 (the rebuild of the frame before it), except a
// translation the effect writes into the matrix itself (+0x54..+0x5C), which that frame draws.
// Real frames stay exactly vanilla: nothing here writes an entity field or matrix that a real frame
// renders or any battle logic reads. A held frame of phase p (between the real frame of tick N, which
// draws N-1, and the next one, which draws N) draws, for each entity the running effect lists, the
// matrix the engine would build from lerp(fields N-1, fields N, p/n) (angles the short way round,
// positions never wrap; a component the effect marks as a discrete change of tick N holds N-1): built
// on a private copy of the entity by the same engine calls as the rebuild, swapped in around this
// entity's render call only and taken out right after it. The effect's own direct translation is
// drawn lerp(written on tick N, written by tick N + 1, p/n) (the effect predicts it; discrete: held), a
// rotation it writes itself as written.
// "Fields N-1" = what the queue's rebuild used in the frame before the last real frame, "fields N" =
// the live ones, which must still be what the last real frame's queue left (else something moved the
// entity on this held frame: hands off, its own matrix is drawn). An entity in a multi-part chain
// (+0x8C) is not handled (its parts copy the chain's matrix). A listed entity is the effect's: the
// steady-motion extrapolation (ff8_bgate_move_nudge, which writes fields on held frames) leaves it
// alone. Nothing listed / no registered prediction: nothing here runs.
#define FF8_BGATE_ENT_MAX 8
#define FF8_BGATE_ENT_SLOTS 7       // FF8BattleEntitySlotData array 0x1D972C0 (party 0..2, monsters 3..6)
static ff8fx::HeldEntity ff8_bgate_ent_req[FF8_BGATE_ENT_MAX];
static int ff8_bgate_nent_req = 0;  // entities the effect listed on this held frame
static int16_t ff8_bgate_ent_last[FF8_BGATE_ENT_SLOTS][9];  // fields the last queue's rebuild used
static int16_t ff8_bgate_ent_prev[FF8_BGATE_ENT_SLOTS][9];  // ... the queue before the last real frame (tick N-1)
static int16_t ff8_bgate_ent_real[FF8_BGATE_ENT_SLOTS][9];  // ... the last real frame's queue (tick N)
static bool ff8_bgate_ent_last_ok = false, ff8_bgate_ent_prev_ok = false, ff8_bgate_ent_real_ok = false;
static uint8_t ff8_bgate_ent_live[0x20];                    // the entity's own matrix while swapped out
static uint8_t *ff8_bgate_ent_swapped = nullptr;
static uint32_t ff8_bgate_ent_frames = 0, ff8_bgate_ent_handsoff = 0; // stats

static int ff8_bgate_ent_index(const void *slot)
{
	const uint32_t o = (uint32_t)slot - 0x1D972C0u;
	return (o % 0x9C) == 0 && o / 0x9C < FF8_BGATE_ENT_SLOTS ? (int)(o / 0x9C) : -1;
}

static void ff8_bgate_ent_read(uint8_t *e, int16_t v[9])
{
	for (int c = 0; c < 9; c++)
	{
		const int16_t *p = ff8fx::held_entity_field(e, c);
		v[c] = p ? *p : 0;
	}
}

// start of every battle frame (BdLink hook): a held frame lists its entities anew; a real frame makes
// the last queue's fields the "previous tick" ones of its held frames
static void ff8_bgate_ent_frame_start()
{
	ff8_bgate_nent_req = 0;
	if (ff8_bgate_phase == 0)
	{
		memcpy(ff8_bgate_ent_prev, ff8_bgate_ent_last, sizeof(ff8_bgate_ent_prev));
		ff8_bgate_ent_prev_ok = ff8_bgate_ent_last_ok;
		ff8_bgate_ent_real_ok = false;
	}
}

// effect gate, held frame, right after the module's held draw (same saved GTE / scratch state)
static void ff8_bgate_ent_collect(int eid)
{
	ff8_bgate_nent_req = ff8fx::held_entities(eid, ff8_bgate_ent_req, FF8_BGATE_ENT_MAX);
}

// the effect listed this entity on this held frame (the extrapolation keeps off it)
static bool ff8_bgate_ent_claimed(const void *slot)
{
	for (int i = 0; i < ff8_bgate_nent_req; i++)
		if (ff8_bgate_ent_req[i].ent == slot) return true;
	return false;
}

// after the entity queue (every frame): the fields its rebuilds used (read only)
static void ff8_bgate_ent_after_queue()
{
	for (int s = 0; s < FF8_BGATE_ENT_SLOTS; s++) ff8_bgate_ent_read((uint8_t *)(0x1D972C0 + 0x9C * s), ff8_bgate_ent_last[s]);
	ff8_bgate_ent_last_ok = true;
	if (ff8_bgate_phase == 0)
	{
		memcpy(ff8_bgate_ent_real, ff8_bgate_ent_last, sizeof(ff8_bgate_ent_real));
		ff8_bgate_ent_real_ok = true;
	}
}

// lerp(a, b, phase / n) with the gate's rounding (as ff8_bgate_scale_round: half away from zero)
static int32_t ff8_bgate_ent_lerp(int c, int32_t a, int32_t b)
{
	if (c < 9) { a = (int16_t)a; b = (int16_t)b; }              // s16 fields (positions never wrap round)
	int64_t d = (int64_t)b - a;
	if (c >= 3 && c < 6) d = ((d + 0x800) & 0xFFF) - 0x800;     // angle, the short way round
	const int64_t t = d * ff8_bgate_phase;
	const int64_t s = t >= 0 ? (t + ff8_bgate_n / 2) / ff8_bgate_n : -((-t + ff8_bgate_n / 2) / ff8_bgate_n);
	return c < 9 ? (int32_t)(int16_t)(a + s) : (int32_t)(a + s);
}

// the entity task's matrix rebuild (0x502BE6..0x502CDD) on the entity copy t: rotation from +0x0C,
// translation from +0x1C (+0x22 on y) and the state offset through it, the root bone's offset kept
// through the scales (+0x30 vector, the size scales x +0x9A); engine calls, private scratch
static void ff8_bgate_ent_rebuild(uint8_t *t)
{
	alignas(4) uint8_t w[0x28];
	alignas(4) int32_t tmp[4], sc[3];
	uint8_t *m = t + 0x40;
	((void *(__cdecl *)(void *, void *))0x56CE30)(t + 0xC, m);                 // ComposeZYXRotationMatrix
	*(int32_t *)(t + 0x54) = *(const int16_t *)(t + 0x1C);
	*(int32_t *)(t + 0x58) = *(const int16_t *)(t + 0x22) + *(const int16_t *)(t + 0x1E);
	*(int32_t *)(t + 0x5C) = *(const int16_t *)(t + 0x20);
	((void (__cdecl *)(void *))0x509B30)(m);                                   // GTE rotation / translation = m
	((void (__cdecl *)(void *, void *, void *))0x56C820)(*(uint8_t **)(t + 0x74) + 0x24, t + 0x54, tmp); // TransformCoordinateToCameraSpace
	uint8_t *root = **(uint8_t ***)(t + 0x64) + 0x34;                          // skeleton bone 0 + 0x24
	((void (__cdecl *)(void *, void *, void *))0x56C600)(m, root, w + 8);      // TransformVectorBy3x3Matrix
	if (*(void **)(t + 0x30)) ((void (__cdecl *)(void *, void *))0x56BEF0)(m, *(void **)(t + 0x30)); // scale3DMatrix
	const int32_t ms = *(const int16_t *)(t + 0x9A);
	sc[0] = (*(const int16_t *)(t + 0x94) * ms) >> 12;
	sc[1] = (*(const int16_t *)(t + 0x96) * ms) >> 12;
	sc[2] = (*(const int16_t *)(t + 0x98) * ms) >> 12;
	((void (__cdecl *)(void *, void *))0x56BEF0)(m, sc);
	((void (__cdecl *)(void *, void *, void *))0x56C600)(m, root, w + 0x18);
	*(int32_t *)(t + 0x54) += *(const int32_t *)(w + 8) - *(const int32_t *)(w + 0x18);
	*(int32_t *)(t + 0x5C) += *(const int32_t *)(w + 0x10) - *(const int32_t *)(w + 0x20);
}

// the held frame's matrix of a listed entity (false: it draws its own); out = 0x20 bytes
static bool ff8_bgate_ent_display(uint8_t *slot, uint8_t *out)
{
	if (ff8_bgate_phase == 0 || !ff8_bgate_ent_prev_ok || !ff8_bgate_ent_real_ok) return false;
	const int s = ff8_bgate_ent_index(slot);
	if (s < 0 || *(void **)(slot + 0x8C) || !*(void **)(slot + 0x74) || !*(void **)(slot + 0x64)) return false;
	const ff8fx::HeldEntity *h = nullptr;
	for (int i = 0; i < ff8_bgate_nent_req; i++)
		if (ff8_bgate_ent_req[i].ent == slot) h = &ff8_bgate_ent_req[i];
	if (!h) return false;
	int16_t now[9];
	ff8_bgate_ent_read(slot, now);
	if (memcmp(now, ff8_bgate_ent_real[s], sizeof(now))) { ff8_bgate_ent_handsoff++; return false; } // moved on this held frame
	alignas(4) static uint8_t t[0x9C], st[0x30];
	memcpy(t, slot, sizeof(t));
	memcpy(st, *(uint8_t **)(slot + 0x74), sizeof(st));
	*(uint8_t **)(t + 0x74) = st;
	for (int c = 0; c < 9; c++)
		*ff8fx::held_entity_field(t, c) = (int16_t)((h->jump >> c & 1) ? ff8_bgate_ent_prev[s][c] : ff8_bgate_ent_lerp(c, ff8_bgate_ent_prev[s][c], now[c]));
	static uint8_t gte_data[0x90], gte_ctrl[0xF0];
	memcpy(gte_data, (void *)0x1CA8A10, sizeof(gte_data));
	memcpy(gte_ctrl, (void *)0x1CA9210, sizeof(gte_ctrl));
	ff8_bgate_ent_rebuild(t);
	memcpy((void *)0x1CA8A10, gte_data, sizeof(gte_data));
	memcpy((void *)0x1CA9210, gte_ctrl, sizeof(gte_ctrl));
	memcpy(out, t + 0x40, 0x20);
	for (int c = 9; c < 12; c++)
	{
		if (h->jump >> c & 1) *(int32_t *)(out + 0x14 + 4 * (c - 9)) = h->from[c];
		else if (h->move >> c & 1) *(int32_t *)(out + 0x14 + 4 * (c - 9)) = ff8_bgate_ent_lerp(c, h->from[c], h->next[c]);
	}
	if ((h->move | h->jump) & ff8fx::HE_DROT) memcpy(out, h->drot, sizeof(h->drot)); // the effect's own rotation matrix
	return true;
}

// around BS_RenderBattleEntity (called by the entity task): held frames only, this render only
static void ff8_bgate_ent_render_begin(uint8_t *slot)
{
	alignas(4) uint8_t m[0x20];
	if (ff8_bgate_ent_swapped || !ff8_bgate_ent_display(slot, m)) return;
	memcpy(ff8_bgate_ent_live, slot + 0x40, sizeof(ff8_bgate_ent_live));
	memcpy(slot + 0x40, m, sizeof(m));
	ff8_bgate_ent_swapped = slot;
	ff8_bgate_ent_frames++;
}

static void ff8_bgate_ent_render_end(uint8_t *slot)
{
	if (ff8_bgate_ent_swapped != slot) return;
	memcpy(slot + 0x40, ff8_bgate_ent_live, sizeof(ff8_bgate_ent_live));
	ff8_bgate_ent_swapped = nullptr;
}

/// --- held-frame extrapolation of CODE-DRIVEN entity movement (15 Hz effect writes) ---
// The choreography VM moves entities on every data tick now (converted data): nothing to smooth.
// What still moves an entity only on real frames is 15 Hz code - an effect that is not natively
// ported writing the entity position from its tick (KnockBack-like moves). On held frames that
// position is nudged forward by phase/n of the last real step, and the true value is put back at the
// start of the next frame, before any logic reads it.
//  - only STEADY motion is extrapolated (the last two real steps agree), so a lone spike (root
//    motion folded in once per loop, a teleport) never qualifies;
//  - hands off as soon as anything moved the entity on a held frame (its VM, a 9E / AD / AE task,
//    which run every host frame): such an entity is smooth already, and stays excluded until two
//    real ticks passed with no held-frame move;
//  - an entity the running ported effect lists (ff8_bgate_ent_*) is the effect's.
// Positions handled: FF8BattleEntitySlotData.based_position (+0x1C, int16 z/y/x) and the
// state controller's position offset (*(slot+0x74) + 0x24, int16 x3).
#define FF8_BGATE_MOVE_SLOTS 16
struct ff8_bgate_move_t
{
	void *slot;
	bool have, have_d;
	int16_t last[6];     // positions at the end of the last real update
	int16_t d1[6], d2[6]; // last two real steps
	bool nudged;
	int16_t written[6];  // values we wrote on the held frame (validity check on restore)
	uint8_t held_moved;  // real ticks left during which the entity counts as moved on held frames
};
static ff8_bgate_move_t ff8_bgate_move[FF8_BGATE_MOVE_SLOTS];

static int16_t *ff8_bgate_move_pos(void *slot, int i)
{
	uint8_t *s = (uint8_t *)slot;
	if (i < 3)
		return (int16_t *)(s + 0x1C) + i;
	uint8_t *st = *(uint8_t **)(s + 0x74);
	return st ? (int16_t *)(st + 0x24) + (i - 3) : nullptr;
}

static ff8_bgate_move_t *ff8_bgate_move_get(void *slot)
{
	ff8_bgate_move_t *free_slot = nullptr;
	for (int i = 0; i < FF8_BGATE_MOVE_SLOTS; i++)
	{
		if (ff8_bgate_move[i].slot == slot) return &ff8_bgate_move[i];
		if (!ff8_bgate_move[i].slot && !free_slot) free_slot = &ff8_bgate_move[i];
	}
	if (!free_slot) return nullptr;
	memset(free_slot, 0, sizeof(*free_slot));
	free_slot->slot = slot;
	return free_slot;
}

// start of every frame (BdLink hook, before any battle logic): put the true positions back
static void ff8_bgate_move_restore_all()
{
	for (int m = 0; m < FF8_BGATE_MOVE_SLOTS; m++)
	{
		ff8_bgate_move_t &mv = ff8_bgate_move[m];
		if (!mv.slot || !mv.nudged) continue;
		mv.nudged = false;
		for (int i = 0; i < 6; i++)
		{
			int16_t *p = ff8_bgate_move_pos(mv.slot, i);
			if (p && *p == mv.written[i]) // untouched since we wrote it: restore
				*p = mv.last[i];
		}
	}
}

// end of a real-frame update: record this native step
static void ff8_bgate_move_record(void *slot)
{
	ff8_bgate_move_t *mv = ff8_bgate_move_get(slot);
	if (!mv) return;
	for (int i = 0; i < 6; i++)
	{
		int16_t *p = ff8_bgate_move_pos(slot, i);
		int16_t v = p ? *p : 0;
		if (mv->have)
		{
			mv->d2[i] = mv->d1[i];
			mv->d1[i] = (int16_t)(v - mv->last[i]);
		}
		mv->last[i] = v;
	}
	mv->have_d = mv->have;
	mv->have = true;
	if (mv->held_moved) mv->held_moved--;
}

// held frame, after the entity's VM ran: nudge steady real-frame-only movement forward
static void ff8_bgate_move_nudge(void *slot)
{
	ff8_bgate_move_t *mv = ff8_bgate_move_get(slot);
	if (!mv || !mv->have) return;
	for (int i = 0; i < 6; i++)
	{
		int16_t *p = ff8_bgate_move_pos(slot, i);
		if (p && *p != mv->last[i])
		{
			mv->held_moved = 2; // moved on a held frame (VM, 9E task...): smooth on its own
			return;
		}
	}
	if (ff8_bgate_ent_claimed(slot)) return; // the running effect drives this entity (ff8_bgate_ent_*)
	if (!mv->have_d || mv->nudged || mv->held_moved) return;
	bool any = false;
	for (int i = 0; i < 6; i++)
	{
		int16_t *p = ff8_bgate_move_pos(slot, i);
		int d1 = mv->d1[i], d2 = mv->d2[i];
		int tol = abs(d1) / 2; if (tol < 4) tol = 4;
		bool steady = d1 != 0 && abs(d1) < 1024 && abs(d1 - d2) <= tol;
		mv->written[i] = p ? *p : 0;
		if (p && steady)
		{
			*p = (int16_t)(mv->last[i] + ff8_bgate_scale_round(d1, ff8_bgate_phase, ff8_bgate_n));
			mv->written[i] = *p;
			any = true;
		}
	}
	mv->nudged = any;
}

int __cdecl ff8_bgate_animseq_upd_hook(void *slot_data_struct)
{
#if FF8_BGATE_DIAG
	{
		uint8_t *s = (uint8_t *)slot_data_struct;
		uint8_t *st = (uint8_t *)*(uint32_t *)(s + 0x74); // battle_state_controler_pointer
		ffnx_info("D f=%u p=%d e=%p com=%u anim=%02X cf=%u/%u seq=%u pc=%u dly=%u flg=%04X pos=%d,%d,%d\n",
			ff8_bgate_diag_frame, ff8_bgate_phase, slot_data_struct, s[4],
			s[0x6C], s[0x72], s[0x73],
			st[0], (unsigned)*(uint16_t *)(st + 0x2E), st[1],
			(unsigned)*(uint16_t *)(st + 0x2C),
			(int)*(int16_t *)(st + 0x24), (int)*(int16_t *)(st + 0x26),
			(int)*(int16_t *)(st + 0x28));
	}
#endif
	int r = 0;
	if (ff8_bgate_data_tick())
	{
		// converted data: the VM runs on every data tick (every host frame at 30 fps)
		ff8_bgate_in_vm = true;
		unreplace_function(ff8_bgate_animseq_upd_ri);
		r = ff8_bgate_animseq_upd_orig(slot_data_struct);
		rereplace_function(ff8_bgate_animseq_upd_ri);
		ff8_bgate_in_vm = false;
	}
	else
	{
		// 60 fps only: host frame between two data ticks - the reader shows its look-ahead midpoint
		((int(__cdecl *)(void *))0x5094F0)(slot_data_struct);
	}
	if (ff8_bgate_phase == 0) ff8_bgate_move_record(slot_data_struct);
	else ff8_bgate_move_nudge(slot_data_struct);
	return r;
}

// entity queue call (0x500971 ExecuteTaskQueue(g_BattleEntityTaskQueue)) and the entity task's
// render call (0x502B95 BS_RenderBattleEntity): see ff8_bgate_ent_* above
static int (__cdecl *ff8_bgate_entq_orig)(void *) = nullptr;
static char (__cdecl *ff8_bgate_entrender_orig)(void *) = nullptr;

int __cdecl ff8_bgate_entq_hook(void *queue)
{
	int r = ff8_bgate_entq_orig(queue);
	ff8_bgate_ent_after_queue();
	return r;
}

char __cdecl ff8_bgate_entrender_hook(void *slot)
{
	ff8_bgate_ent_render_begin((uint8_t *)slot);
	char r = ff8_bgate_entrender_orig(slot);
	ff8_bgate_ent_render_end((uint8_t *)slot);
	return r;
}

// --- run-up movement task (AnimSeq opcode 9E, only creator AnimSeq_DispatchActionOpcode 0x505585) ---
// moveEntityToTargetSmoothly (0x50F750) is a time-parameterized lerp: one counter++ per task tick,
// position = origin + (counter/movement_param) * (target - origin). The task ticks every host frame;
// converted data already doubled movement_param (30 Hz ticks), so at 30 fps it is left alone; at
// 60 fps it is scaled by data_div (the curve is a pure ratio: same positions, finer steps).
static void *(__cdecl *ff8_bgate_anim9e_orig)(void *, void *, int) = nullptr;
static uint32_t ff8_bgate_anim9e_ri = 0;

void *__cdecl ff8_bgate_anim9e_hook(void *active_entity, void *param_entity, int movement_param)
{
	unreplace_function(ff8_bgate_anim9e_ri);
	void *task = ff8_bgate_anim9e_orig(active_entity, param_entity, movement_param);
	rereplace_function(ff8_bgate_anim9e_ri);
	if (task)
	{
		uint16_t *param = (uint16_t *)((uint8_t *)task + 0x14); // TaskNodeAnimSeq_9E.movement_param
#if FF8_BGATE_DIAG
		ffnx_info("D f=%u p=%d 9E s=%d->s=%d param=%u (scaled to %u)\n",
			ff8_bgate_diag_frame, ff8_bgate_phase, FF8_BGATE_SLOT_IDX(active_entity),
			FF8_BGATE_SLOT_IDX(param_entity), (unsigned)*param, (unsigned)(*param * ff8_bgate_n));
#endif
		// converted data: the 9E duration is already in 30 Hz ticks (x2 by the converter); the task
		// ticks every host frame, so only 60 fps scales it (x data_div)
		if (*param > 0 && ff8_bgate_data_div > 1)
			*param = (uint16_t)(*param * ff8_bgate_data_div);
	}
	return task;
}

// --- battle camera: gate the keyframe player to real frames (see install-site comment
// for why the step-halving patch this replaced caused a hard engine hang) ---
static int (__cdecl *ff8_bgate_camanim_orig)(void *) = nullptr;
static uint32_t ff8_bgate_camanim_ri = 0;

// Converted data: every camera keyframe collection (monster sec6 / char sec5, stage, MAG, r0win, exe
// effect tracks .bcam) has its durations x2 (30 Hz ticks): the player runs on every data tick with
// the VANILLA step (16 per call, never halved - see the install-site note on the hang), as vanilla
// runs it on its own data's tick. Effect code (15 Hz) only STARTS a track (Battle_PlayCameraAnimation);
// the player task (BD_LINK_TASK_HEADER_CAMERA) steps it every frame.
int __cdecl ff8_bgate_camanim_hook(void *task)
{
	if (!ff8_bgate_data_tick())
		return 0; // 60 fps only: between two data ticks, the camera struct stays put
	unreplace_function(ff8_bgate_camanim_ri);
	int r = ff8_bgate_camanim_orig(task);
	rereplace_function(ff8_bgate_camanim_ri);
	return r;
}

// --- battle camera: the RENDER-FACING camera of held frames ---
// The keyframe player's internal catch-up loop is where the hang lived (see above) - its
// internal state is never touched on held frames. The four globals updateBattleCamera (0x504060)
// writes - Battle_Camera_world_XZ_s16/Y and Battle_Camera_LookAt_XZ_s16/Y - are a separate, simple
// output the view matrix is built from: the hook below lets the original run untouched, then
// writes the in-between camera into them for the view build only (see ff8_bgate_updatecam_hook).

static int (__cdecl *ff8_bgate_updatecam_orig)() = nullptr;
static uint32_t ff8_bgate_updatecam_ri = 0;

// --- battle camera script VM at native rate ---
// updateBattleCamera runs every host frame and steps the camera script VM
// BS_UpdateCameraSequence (0x509610; section-6 camera scripts: which shot plays, when it
// ends) on each call -> 2x fast. That script ending too early broke the camera return
// after cinematic enemy abilities (Bite Bug effect 234): Magic_EffectEnd_ResetCamera...
// (0x50AED0) only arms the return to the saved view (CAMERA_WOBBLING_FACTOR = 4096 ->
// dword_B8B800 view restored) while the script's anim collection is still active
// (cameraRelated_pointerAnimColl_flag); it had already finished, so the camera stayed
// wherever the effect left it ("locked on the target"). Real frames only; the VM returns
// its new setting pointer, so held frames return the current one unchanged.
static void *(__cdecl *ff8_bgate_camseq_orig)() = nullptr;
static uint32_t ff8_bgate_camseq_ri = 0;

// Converted data: camera scripts have their 01 yields (outside the C3 0x10 shot-flag loops) and 04 XX
// ease ticks x2 (FF8GameData/dat/camera30.py): the VM runs on every data tick.
void *__cdecl ff8_bgate_camseq_hook()
{
	if (!ff8_bgate_data_tick())
		return *(void **)0x1D99A34; // CURRENT_CAMERA_SETTING_ADDR (60 fps only: between two data ticks)
	unreplace_function(ff8_bgate_camseq_ri);
	void *r = ff8_bgate_camseq_orig();
	rereplace_function(ff8_bgate_camseq_ri);
	return r;
}

// Held-frame extrapolation must never OVERRIDE a camera the engine set on purpose. The
// return-to-view after an effect is a ONE-SHOT snap (Magic_EffectEnd_ResetCameraAndEntityFlags
// arms Battle_Camera_ReturnViewBlend = 4096, updateBattleCamera copies the return view once);
// when it landed on a held frame, the extrapolation from the effect's last shots replaced it,
// and since the engine never rewrites a settled camera the view stayed "locked on the target"
// (Bite Bug effect 234, victory fanfare). So: if the original call changed the camera on a
// held frame, keep its value and restart the history there. And the extrapolated value is
// display-only: the true value is restored at the start of the next real frame (BdLink hook),
// so it can never linger in the camera globals.
static int32_t ff8_bgate_cam_true[4], ff8_bgate_cam_written[4];
static bool ff8_bgate_cam_nudged = false;

// field of view (word_1D8E038) and roll (g_BattleCam_Roll 0x1D977A2) are written by the
// keyframe player itself: their held-frame midpoints are restored the same way
static int16_t ff8_bgate_cam_fovroll_true[2], ff8_bgate_cam_fovroll_written[2];
static bool ff8_bgate_cam_fovroll_nudged = false;

static void ff8_bgate_cam_restore()
{
	if (ff8_bgate_cam_fovroll_nudged)
	{
		ff8_bgate_cam_fovroll_nudged = false;
		int16_t *fr[2] = { (int16_t *)0x1D8E038, (int16_t *)0x1D977A2 };
		for (int i = 0; i < 2; i++)
			if (*fr[i] == ff8_bgate_cam_fovroll_written[i]) *fr[i] = ff8_bgate_cam_fovroll_true[i];
	}
	if (!ff8_bgate_cam_nudged) return;
	ff8_bgate_cam_nudged = false;
	int32_t *g[4] = { (int32_t *)0xB8B7F0, (int32_t *)0xB8B7F4, (int32_t *)0xB8B7F8, (int32_t *)0xB8B7FC };
	for (int i = 0; i < 4; i++)
		if (*g[i] == ff8_bgate_cam_written[i]) // untouched since we wrote it
			*g[i] = ff8_bgate_cam_true[i];
}

// TRUE 30 FPS camera shots: the keyframe player (ProcessCameraAnimation 0x5035E0) is run one
// tick ahead with the vanilla time step on a saved copy of its whole state - the camera struct
// (BattleStageCameraMainData, 1316 bytes, task node +0x0C), field of view, roll and the
// shot mask - and everything is put back; the held frame then shows the exact midpoint of the
// camera (eye, target, fov, roll) between this tick and the next. A shot that ends on the next
// tick has no next camera of its own (the camera script picks what follows): that half tick
// holds. Cameras written directly by effects keep the extrapolation below.
// (FF8_BGATE_CAM_LOOKAHEAD 1 = the former design: the held frame's camera predicted one tick
// ahead - keyframe player run ahead on a saved copy, ported effects' held_camera, extrapolation -
// and written at the end of the HELD frame. That was half a tick off: see the hook below.)
#define FF8_BGATE_CAM_LOOKAHEAD 0
#if FF8_BGATE_CAM_LOOKAHEAD
struct ff8_bgate_cam_next_t { bool valid; uint8_t *cs; int16_t v[8]; int16_t fov, roll; };

static void ff8_bgate_cam_lookahead(ff8_bgate_cam_next_t &nx)
{
	nx.valid = false;
	uint8_t *node = *(uint8_t **)0x1D97768; // BD_LINK_TASK_HEADER_CAMERA.head
	for (; node; node = *(uint8_t **)(node + 4))
		if (*(uint32_t *)(node + 8) == 0x5035E0) break;
	if (!node) return;
	uint8_t *cs = *(uint8_t **)(node + 0x0C);
	if (!cs || cs != *(uint8_t **)0x1D97798) return; // the struct the output reads (cameraStructPointer)
	static uint8_t cs_save[1316];
	memcpy(cs_save, cs, sizeof(cs_save));
	int16_t fov = *(int16_t *)0x1D8E038, roll = *(int16_t *)0x1D977A2;
	uint16_t shots = *(uint16_t *)0x1D97718;
	uint32_t pool = *(uint32_t *)0x1D999C4;
	// a segment switch can capture the return view (BS_Camera_CaptureReturnView 0x503300):
	// Battle_Camera_ReturnView_* (0xB8B800, 16 bytes), its fov (0x1D977A0) and roll (0x1D9771C)
	uint8_t ret_view[16];
	memcpy(ret_view, (void *)0xB8B800, sizeof(ret_view));
	int16_t ret_fov = *(int16_t *)0x1D977A0, ret_roll = *(int16_t *)0x1D9771C;
	uint32_t r = 2;
	__try
	{
		unreplace_function(ff8_bgate_camanim_ri);
		r = (uint32_t)ff8_bgate_camanim_orig(node);
		rereplace_function(ff8_bgate_camanim_ri);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		rereplace_function(ff8_bgate_camanim_ri);
		r = 2;
	}
	if (r != 2)
	{
		memcpy(nx.v, cs + 20, sizeof(nx.v)); // CurrentCameraPos Z/X/Y, pad, CurrentLookAt Z/X/Y, pad
		nx.fov = *(int16_t *)0x1D8E038;
		nx.roll = *(int16_t *)0x1D977A2;
		nx.cs = cs;
		nx.valid = true;
	}
	memcpy(cs, cs_save, sizeof(cs_save));
	*(int16_t *)0x1D8E038 = fov;
	*(int16_t *)0x1D977A2 = roll;
	*(uint16_t *)0x1D97718 = shots;
	*(uint32_t *)0x1D999C4 = pool;
	memcpy((void *)0xB8B800, ret_view, sizeof(ret_view));
	*(int16_t *)0x1D977A0 = ret_fov;
	*(int16_t *)0x1D9771C = ret_roll;
}

#endif

#if FF8_BGATE_CAM_LOOKAHEAD
// held frame of a natively ported effect that drives the camera: its exact in-between camera
static bool ff8_bgate_fx_held_camera(int16_t world[3], int16_t lookat[3]);
#endif

// Per-frame camera diagnostics during an effect (with ff8_battle_fx_held_check): which path set
// the camera, the eye before/after the engine's update and the final one, the shake offsets.
static struct { const char *path; int32_t pre[4], eng[4], fin[4]; bool valid; } ff8_bgate_camdbg = {};
static uint32_t ff8_bgate_camdbg_lines = 0;

// WHEN each camera reaches the screen (EXE order of one battle frame, BdLink 0x500900):
//   0x500905..0x500971  every task queue draws - entities (BS_RenderBattleEntity 0x502D40), the
//                       stage (BS_Task_UpdateAndRenderStage 0x50DFF0 -> BS_RenderRelated 0x500FD0),
//                       the effect tick 0x50093A (and on held frames its redraw) - all GTE-transform
//                       with the view matrix 0x1D97778 AS IT IS when the frame starts
//   0x500988            updateBattleCamera: keyframe player / effect-written words -> 0xB8B7F0..FC
//   0x500992            BS_Camera_BuildViewMatrixApplyShake: 0xB8B7F0 + roll + shake -> 0x1D97778
//   0x5005A0 (jmp)      OT drawn; 0x5006E4 field of view 0x1D8E038 -> GTE H for the NEXT frame
// So the camera words (and fov, roll, shake) left at the end of frame F are what frame F+1 draws
// with: vanilla draws tick N's geometry with the camera of tick N-1. The held frame after real
// tick N shows the geometry of N + phase/n, so it must draw with the camera of N-1 + phase/n,
// and the next real frame with the camera of tick N exactly (vanilla). Both are known when the
// frame ends - the last two real ticks' cameras - no prediction: at the end of the frame of
// phase p the words are set to lerp(C[N-1], C[N], (p+1)/n) (the last frame before a real one
// leaves the true C[N] untouched) and the true words are put back when the next frame starts.
// A cut (an eye / look-at component jumping more than 1500 in one tick, as the harness's
// FXA_LCCUT and the ported effects' own cut rule) keeps C[N-1] for the held frames: the cut
// shows on the real frame, as in vanilla. If the engine itself moves the camera on a held frame
// (return snap, blend), its value is kept and the history restarts there.
#define FF8_BGATE_CAM_CUT 1500
struct ff8_bgate_cam_real_t { int32_t w[4]; int16_t fov, roll; bool valid; };
static ff8_bgate_cam_real_t ff8_bgate_cam_r0 = {}, ff8_bgate_cam_r1 = {}; // r1 = C[N] (last real frame), r0 = C[N-1]

static void ff8_bgate_cam_read(ff8_bgate_cam_real_t &c)
{
	memcpy(c.w, (void *)0xB8B7F0, sizeof(c.w));
	c.fov = *(int16_t *)0x1D8E038;
	c.roll = *(int16_t *)0x1D977A2;
	c.valid = true;
}

// eye 0xB8B7F0 and look-at 0xB8B7F8 are SVECTORs (s16 x, y, z + pad): the view build reads them
// with movsx word (matrixMultiplyVector 0x56C4F0, BS_Camera_LookAt 0x50CCF0). The pads (+6) hold
// whatever the writer left (Siren's A/B shows different pad bytes for the original and the port):
// never interpolated, the true pads stay
static const int ff8_bgate_cam_comp[6] = { 0, 1, 2, 4, 5, 6 }; // s16 indices of eye x/y/z, look-at x/y/z

// Which cameras still get the in-between: only those that move on real frames alone, i.e. the
// camera words 15 Hz effect code writes from its tick (not ported to a held camera). A camera that
// moved on a held frame is played from converted data every frame: no in-between on the following
// real frame either (ff8_bgate_cam_held_moved).
static bool ff8_bgate_cam_held_moved = false;

int __cdecl ff8_bgate_updatecam_hook()
{
	int32_t *g = (int32_t *)0xB8B7F0; // eye SVECTOR, look-at SVECTOR (as 4 dwords)
	int32_t before[4] = { g[0], g[1], g[2], g[3] };

	unreplace_function(ff8_bgate_updatecam_ri);
	int r = ff8_bgate_updatecam_orig();
	rereplace_function(ff8_bgate_updatecam_ri);

	int32_t after[4] = { g[0], g[1], g[2], g[3] };
	memcpy(ff8_bgate_camdbg.pre, before, sizeof(before));
	memcpy(ff8_bgate_camdbg.eng, after, sizeof(after));
	ff8_bgate_camdbg.valid = true;
	if (ff8_bgate_phase == 0)
	{
		// real frame: the fully-vanilla camera of this tick
		ff8_bgate_camdbg.path = "real";
		ff8_bgate_cam_r0 = ff8_bgate_cam_r1;
		ff8_bgate_cam_read(ff8_bgate_cam_r1);
		// the camera also moved on the held frame(s) since the last real one: it is played from data
		// every frame (converted keyframes / camera script) - shown as is, no in-between
		if (ff8_bgate_cam_held_moved)
		{
			ff8_bgate_camdbg.path = "real(data)";
			ff8_bgate_cam_r0 = ff8_bgate_cam_r1;
			ff8_bgate_cam_held_moved = false;
		}
	}
	else if (memcmp(before, after, sizeof(before)) != 0)
	{
		// the engine moved the camera itself on this held frame (data camera, return snap, blend):
		// that value wins from now on
		ff8_bgate_camdbg.path = "engine";
		ff8_bgate_cam_read(ff8_bgate_cam_r1);
		ff8_bgate_cam_r0 = ff8_bgate_cam_r1;
		ff8_bgate_cam_held_moved = true;
	}
	else
		ff8_bgate_camdbg.path = "held";

	// the view built next (0x500992) and the fov loaded at the loop top are the NEXT frame's
	const int num = ff8_bgate_phase + 1, den = ff8_bgate_n;
	if (num >= den || !ff8_bgate_cam_r0.valid || !ff8_bgate_cam_r1.valid) return r;
	const int16_t *a = (const int16_t *)ff8_bgate_cam_r0.w, *b = (const int16_t *)ff8_bgate_cam_r1.w;
	int16_t *g16 = (int16_t *)0xB8B7F0;
	bool cut = false;
	for (int k : ff8_bgate_cam_comp)
		if (abs((int32_t)b[k] - a[k]) > FF8_BGATE_CAM_CUT) cut = true;
	if (cut) ff8_bgate_camdbg.path = ff8_bgate_phase == 0 ? "real(cut)" : "held(cut)";
	memcpy(ff8_bgate_cam_true, after, sizeof(after));
	for (int k : ff8_bgate_cam_comp)
		g16[k] = cut ? a[k] : (int16_t)(a[k] + ff8_bgate_scale_round((int32_t)b[k] - a[k], num, den));
	memcpy(ff8_bgate_cam_written, g, sizeof(ff8_bgate_cam_written));
	ff8_bgate_cam_nudged = true;
	int16_t *fov = (int16_t *)0x1D8E038, *roll = (int16_t *)0x1D977A2;
	ff8_bgate_cam_fovroll_true[0] = *fov;
	ff8_bgate_cam_fovroll_true[1] = *roll;
	const ff8_bgate_cam_real_t &c0 = ff8_bgate_cam_r0, &c1 = ff8_bgate_cam_r1;
	*fov = cut ? c0.fov : (int16_t)(c0.fov + ff8_bgate_scale_round(c1.fov - c0.fov, num, den));
	*roll = cut ? c0.roll : (int16_t)(c0.roll + ff8_bgate_scale_round(c1.roll - c0.roll, num, den));
	ff8_bgate_cam_fovroll_written[0] = *fov;
	ff8_bgate_cam_fovroll_written[1] = *roll;
	ff8_bgate_cam_fovroll_nudged = true;
	return r;
}

// --- magic/GF effect pacing: native-rate tick + OT-insert record/replay ---
// The battle effect tick (call @0x50093A -> ExecuteTaskQueue(C3_28_GF_data_pointer))
// both ADVANCES and DRAWS the active magic/GF effect tree. Two failed designs first:
// skipping the tick on held frames = 15fps flicker (no draw); snapshot/restore around
// a held tick = SOFTLOCKS, because effect state is spread over regions that cannot be
// enumerated (per-effect BSS block, a packet scratch growing 71520 bytes/state, and a
// SHARED GROWING PARTICLE HEAP at ~0x020Fxxxx whose list header has no pool bounds -
// measured live on Esuna: its emitter's live-particle counts sat inside the restore
// window while the particles died outside it, so the counts never reached zero and the
// busy_lock completion chain hung the battle forever).
//
// This design never rewinds anything. The tick runs ONLY on real frames (phase 0), so
// every effect - magic, Draw/Stock, limits, GF summons - advances at native 15fps by
// construction. For the draw: all GPU packets enter the frame via the SSIGPU ordering
// table through exactly two primitives, SSIGPU_InsertPrimDepthKeys (0x45C870) and
// SSIGPU_InsertPrimAutoDepth (0x45C7A0). During the real tick we RECORD every insert
// (bucket, packet, depth keys); on held frames we RE-ISSUE the same inserts against the
// current frame's OT (rebased: the OT lives at g_Battle_FrameRenderListBase+68 and
// flips per frame). This is safe because:
//  - the insert allocates its linkage node from the per-frame ssigpu execution arena
//    and only refreshes the packet tag's link bits - re-inserting a packet is a
//    first-class engine operation;
//  - effect packet memory is double-buffered per TICK (activeVram/vramEven/vramOdd),
//    and at n=2 the frame-flat buffer parity of real frames is untouched on held
//    frames, so recorded packet pointers stay valid until the next real tick. (n=4
//    would need packet copies for flat-buffer packets - not handled yet.)
// No held tick also means no re-triggered SFX or damage numbers (the old suppression
// hooks are gone), and no per-effect whitelists: everything is paced uniformly.
#define FF8_BGATE_CUR_OT() (*(uint32_t *)0x1D8E04C + 68) // g_Battle_FrameRenderListBase + 68
// the whole ordering-table span effects can insert into: the render list's 17 header buckets
// (some draws use indices below 0, e.g. Quezacotl) plus the OT proper
#define FF8_BGATE_OT_SPAN_BASE() (*(uint32_t *)0x1D8E04C)
#define FF8_BGATE_OT_SPAN_WORDS (17 + 4386)

static int (__cdecl *ff8_bgate_effect_tick_orig)(void *effect_ctx) = nullptr;

// Held-frame interpolation of the effect draws (all magic, GF summons, limits, Draw...).
// Every effect part - particle, sprite, prim model, mesh, GF creature - reaches the screen
// as a PSX GPU packet whose vertices are already projected to screen space. So instead of
// per-spell work, the held frame is built from the last TWO real ticks: each primitive of
// tick N is paired with its counterpart in tick N-1 and its vertices (and vertex colors)
// are moved forward by (N - (N-1)) * phase / n. At n=2 that is exactly half a native step,
// i.e. the in-between pose, and it matches the camera, which is extrapolated the same way.
// Texture coordinates are never touched: flipbook sprite art keeps its native 15fps
// cadence, only the motion is smoothed. Anything that cannot be paired safely (new particle,
// changed primitive type/texture page, jump larger than FF8_BGATE_FX_MAX_STEP) is redrawn
// exactly as tick N drew it - the previous hold behaviour, never worse.
//
// Packets are COPIED at the end of each real tick (1) so the tick N-1 content survives
// the effects' own double-buffered packet arenas, and (2) so the held-frame inserts link
// our copies into the OT instead of re-linking game memory the effect may already be
// recycling (the cause of the Cure replay fault the blacklist below was guarding against).
// Recording: every primitive reaches the frame as a 24-byte node in the SSIGPU execution
// arena, linked into an ordering-table bucket. FOUR engine functions create nodes -
// 0x45C740 (plain), 0x45C7A0 (GTE auto depth), 0x45C870 (explicit depth keys) and 0x45C8E0
// (msk=1: the executor's alternate viewport mode) - and 0x45C860 edits the last node's msk
// afterwards. Hooking two of them missed draws (Leviathan flashed at 15Hz: part of it was
// drawn on real frames only) and replaying through one of them lost msk. So the tick's draws
// are read straight from the arena instead: the nodes created during the effect tick are
// exactly [cursor before, cursor after), each one with its final keys and msk, and its
// bucket is found by walking the OT (effect nodes sit on top of their bucket's chain, since
// nothing else inserts during the tick). No hooks at all on the shared insert primitives.
#define FF8_BGATE_EXEC_START 0x1C48828u                // ssigpu_execution_start
#define FF8_BGATE_EXEC_CUR (*(uint32_t *)0x1CA8828)     // ssigpu_execution_cur
#define FF8_BGATE_EXEC_LIMIT 0x60000u                   // arena size, as checked by the inserts
#define FF8_BGATE_OT_BUCKETS 4386
#define FF8_BGATE_RLIST_CUR (*(uint32_t *)0x1D8E054)    // battle model render-list cursor (diagnostics)
#define FF8_BGATE_FX_MAX_PRIMS 4096
#define FF8_BGATE_FX_ARENA_WORDS (256 * 1024)  // 1 MB of packet copies per tick
#define FF8_BGATE_FX_MAX_VERTS 64              // vertices tracked per packet
#define FF8_BGATE_FX_MAX_STEP 48               // px per native step; beyond = cut, don't extrapolate
#define FF8_BGATE_FX_MAX_STEP_INORDER 128      // same, for an exact-UV counterpart (meshes: unambiguous)

#pragma pack(push, 1)
struct ff8_bgate_exec_node // struc_34_ssgi_execution
{
	uint32_t *pkt;   // +0x00 packet (tag low 24 bits = previous bucket head, low part)
	int32_t k[4];    // +0x04 depth keys
	uint16_t msk;    // +0x14 bit0 = alternate viewport mode
	uint8_t link_hi; // +0x16 previous bucket head, high byte
	uint8_t pad;
};
#pragma pack(pop)
static_assert(sizeof(ff8_bgate_exec_node) == 24, "SSIGPU execution node is 24 bytes");

static int16_t ff8_bgate_fx_node_bucket[FF8_BGATE_FX_MAX_PRIMS];

// task node -> range of SSIGPU nodes it created (indices relative to the tick's first node)
struct ff8_bgate_task_rng { void *node; uint16_t a, b; uint32_t func; bool ended; }; // func: the function the task ran, ended: it returned TASK_END
#define FF8_BGATE_TASK_LOG 1024

// One real tick's worth of effect draws, copied
struct ff8_bgate_fx_prim
{
	int32_t k[4];
	uint16_t bucket; // index in the battle OT
	uint16_t msk;
	uint16_t ei;     // index of the node in the tick (task ranges refer to it)
	const uint32_t *src_pkt; // where the packet lived in the game's packet arena (re-read at display)
	uint32_t off;    // word offset of the packet copy in the arena
	uint32_t words;  // packet size in words, tag included
	uint32_t sig, sig_uv; // identities (0/0 + !parsed when the packet could not be parsed)
	int32_t next_uv, next_w; // hash chains (see ff8_bgate_fx_snap)
	bool parsed;
};
#define FF8_BGATE_FX_HASH 4096 // chain heads per identity table (power of two)
struct ff8_bgate_fx_snap
{
	ff8_bgate_fx_prim prim[FF8_BGATE_FX_MAX_PRIMS];
	int32_t head_uv[FF8_BGATE_FX_HASH]; // strong identity -> first primitive with it
	int32_t head_w[FF8_BGATE_FX_HASH];  // weak identity -> first primitive with it
	int n;
	int orphans;          // nodes whose bucket could not be found (not replayed)
	uint32_t rlist_delta; // bytes the effect tick pushed to the battle model render list
	uint32_t used;
	void *ctx;
	uint32_t frame;
	bool valid;
	uint32_t arena[FF8_BGATE_FX_ARENA_WORDS];
	// look-ahead pairing (ff8_bgate_la_*): which task drew each primitive
	ff8_bgate_task_rng tasks[FF8_BGATE_TASK_LOG];
	int ntasks;
	int16_t ei2prim[FF8_BGATE_FX_MAX_PRIMS]; // node index -> primitive index (-1: orphan)
	int16_t task_of[FF8_BGATE_FX_MAX_PRIMS]; // primitive -> innermost task entry (-1: none)
};
// One recorder per gated task queue that draws. Two exist: the magic/GF effect tree
// (C3_28_GF_data_pointer, call @0x50093A) and the hit-effect queue (call @0x500923:
// impact sparks, hit camera shakes, footstep dust, Renzokuken parts, MAG_332..343...),
// which vanilla also ticks once per host frame. The recorder code below works on the
// CURRENT recorder (ff8_bgate_R), selected by each gate before it records or replays.
struct ff8_bgate_rec_t
{
	const char *name;
	ff8_bgate_fx_snap snaps[2];
	int cur;              // snaps[cur] = tick N, snaps[cur ^ 1] = tick N-1
	bool replay_ok;
	uint32_t rec_begin;   // arena cursor before the real tick
	uint32_t rec_ot, rec_rlist;
	void *blacklist[16];
	int blacklist_n;
	int last_r;           // queue return value of the last real tick (reported on held frames)
	// the last real tick emptied the queue (r == 0): vanilla shows that tick's picture for its whole frame, so the held
	// frames after it replay the tick's draws as drawn (snaps[cur]); dropped at the next real frame (ff8_bgate_tail_*)
	bool tail;
	bool tail_otclear;
	ff8_bgate_fx_sum_t sum;
	uint32_t out[FF8_BGATE_FX_ARENA_WORDS]; // held-frame packets (live until the frame is drawn)
	ff8_bgate_fx_snap la;                   // tick N+1 (look-ahead), when available
};
static ff8_bgate_rec_t ff8_bgate_rec_fx = { "effect" };
static ff8_bgate_rec_t ff8_bgate_rec_eq = { "hit-effect queue" };
static ff8_bgate_rec_t *ff8_bgate_R = &ff8_bgate_rec_fx;
#define ff8_bgate_fx_snaps (ff8_bgate_R->snaps)
#define ff8_bgate_fx_cur (ff8_bgate_R->cur)
#define ff8_bgate_fx_out (ff8_bgate_R->out)
#define ff8_bgate_fx_replay_ok (ff8_bgate_R->replay_ok)
#define ff8_bgate_fx_rec_begin (ff8_bgate_R->rec_begin)
#define ff8_bgate_fx_rec_ot (ff8_bgate_R->rec_ot)
#define ff8_bgate_fx_rec_rlist (ff8_bgate_R->rec_rlist)
#define ff8_bgate_fx_sum (ff8_bgate_R->sum)
#define ff8_bgate_fx_replay_blacklist (ff8_bgate_R->blacklist)
#define ff8_bgate_fx_replay_blacklist_n (ff8_bgate_R->blacklist_n)

// Per-task draw ranges for the look-ahead pairing (see ff8_bgate_la_* below): the native
// task queue executor (the ExecuteTaskQueue 0x508420 replacement of src/ff8/battle/fx,
// fx_dispatch.cpp) calls the observer below, which notes while recording which SSIGPU nodes
// each task created and which queues ran (their pools are part of the look-ahead snapshot).
struct ff8_bgate_task_node { uint16_t flags; uint16_t seq; ff8_bgate_task_node *next; int (__cdecl *func)(ff8_bgate_task_node *); };
struct ff8_bgate_task_queue { ff8_bgate_task_node *head, *tail; uint8_t *pool; uint16_t node_size, capacity; };
static bool ff8_bgate_etq_rec = false;
static bool ff8_bgate_etq_rec_eq = false; // the hit-effect queue's real tick: task ranges only (no queue / pool bookkeeping)
static ff8_bgate_task_rng ff8_bgate_etq_log[FF8_BGATE_TASK_LOG];
static int ff8_bgate_etq_log_n = 0;
static bool ff8_bgate_etq_log_overflow = false;
static bool ff8_bgate_etq_cycle = false;
static void *ff8_bgate_etq_queues[32];
static int ff8_bgate_etq_queues_n = 0;
static bool ff8_bgate_la_active = false;       // held frame being built from the look-ahead
static bool ff8_bgate_la_in_lookahead = false; // look-ahead pass running (new queues get saved on first sight)
static void ff8_bgate_la_pool_save(ff8_bgate_task_queue *q);

static void ff8_bgate_etq_queue_start(ff8fx::TaskQueue *qv)
{
	if (!ff8_bgate_etq_rec) return;
	int k = 0;
	while (k < ff8_bgate_etq_queues_n && ff8_bgate_etq_queues[k] != qv) k++;
	if (k == ff8_bgate_etq_queues_n)
	{
		if (k < 32) ff8_bgate_etq_queues[ff8_bgate_etq_queues_n++] = qv;
		if (ff8_bgate_la_in_lookahead) ff8_bgate_la_pool_save((ff8_bgate_task_queue *)qv); // first seen inside the look-ahead: save before it runs
	}
}

// the function each running task was called with (a task may switch its node's function): one entry per nested call
static uint32_t ff8_bgate_etq_fn_stack[64];
static int ff8_bgate_etq_fn_depth = 0;

static bool ff8_bgate_etq_task_before(ff8fx::TaskQueue *q, ff8fx::TaskNode *n, int index, uint32_t *cookie)
{
	if (index >= 65536)
	{
		ffnx_info("30fps la: task list of queue %p cycles (lookahead=%d) - aborted\n", q, (int)ff8_bgate_la_in_lookahead);
		ff8_bgate_etq_cycle = true;
		return false;
	}
	*cookie = FF8_BGATE_EXEC_CUR;
	if (ff8_bgate_etq_fn_depth < 64) ff8_bgate_etq_fn_stack[ff8_bgate_etq_fn_depth] = (uint32_t)n->func;
	ff8_bgate_etq_fn_depth++;
	return true;
}

static void ff8_bgate_etq_task_after(ff8fx::TaskQueue *, ff8fx::TaskNode *n, uint32_t ret, uint32_t before)
{
	uint32_t after = FF8_BGATE_EXEC_CUR;
	uint32_t fn = (uint32_t)n->func;
	if (ff8_bgate_etq_fn_depth > 0 && --ff8_bgate_etq_fn_depth < 64) fn = ff8_bgate_etq_fn_stack[ff8_bgate_etq_fn_depth];
	if ((ff8_bgate_etq_rec || ff8_bgate_etq_rec_eq) && after > before && before >= ff8_bgate_fx_rec_begin)
	{
		if (ff8_bgate_etq_log_n < FF8_BGATE_TASK_LOG)
		{
			ff8_bgate_task_rng &e = ff8_bgate_etq_log[ff8_bgate_etq_log_n++];
			e.node = n;
			e.a = (uint16_t)((before - ff8_bgate_fx_rec_begin) / sizeof(ff8_bgate_exec_node));
			e.b = (uint16_t)((after - ff8_bgate_fx_rec_begin) / sizeof(ff8_bgate_exec_node));
			e.func = fn;
			e.ended = (ret & 2) != 0;
		}
		else ff8_bgate_etq_log_overflow = true;
	}
}

static const ff8fx::ExecObserver ff8_bgate_etq_observer = { ff8_bgate_etq_queue_start, ff8_bgate_etq_task_before, ff8_bgate_etq_task_after };


// Walks a PSX GPU packet (tag word + GP0 commands) and lists the word indexes holding
// screen vertices (int16 x | int16 y << 16) and vertex colors (0x00BBGGRR). The signature
// hashes everything that identifies "the same primitive" across ticks: size, command
// codes and texture page/CLUT - but not UVs (flipbook frames advance on the same sprite)
// nor positions/colors (those are what moves). Returns false on commands it does not know
// (VRAM transfers, fills...): such a packet is redrawn as-is.
struct ff8_bgate_pkt_info
{
	uint16_t xy[FF8_BGATE_FX_MAX_VERTS];
	uint16_t col[FF8_BGATE_FX_MAX_VERTS];
	int nxy, ncol;
	uint32_t sig;      // weak identity: packet shape + texture page/CLUT
	uint32_t sig_uv;   // strong identity: weak + every UV word (a mesh triangle keeps its UVs)
	uint32_t tpage;    // texture page word of the last textured primitive (diagnostics)
	uint32_t last_cmd; // command being decoded when parsing stopped (diagnostics)
};

static bool ff8_bgate_pkt_parse(const uint32_t *p, uint32_t words, ff8_bgate_pkt_info &pi)
{
	pi.nxy = pi.ncol = 0;
	pi.last_cmd = 0;
	pi.tpage = 0xFFFFFFFF;
	uint32_t sig = 2166136261u ^ words;
	uint32_t uv = 2166136261u;
	#define FF8_BGATE_UV(v) (uv = (uv ^ (uint32_t)(v)) * 16777619u)
	#define FF8_BGATE_SIG(v) (sig = (sig ^ (uint32_t)(v)) * 16777619u)
	#define FF8_BGATE_XY(w) do { if (pi.nxy >= FF8_BGATE_FX_MAX_VERTS) return false; pi.xy[pi.nxy++] = (uint16_t)(w); } while (0)
	#define FF8_BGATE_COL(w) do { if (pi.ncol >= FF8_BGATE_FX_MAX_VERTS) return false; pi.col[pi.ncol++] = (uint16_t)(w); } while (0)
	uint32_t w = 1;
	while (w < words)
	{
		uint32_t cmd = p[w] >> 24;
		pi.last_cmd = cmd;
		FF8_BGATE_SIG(cmd);
		if (cmd >= 0x20 && cmd < 0x40) // polygon: color, then per vertex [color] xy [uv]
		{
			int nv = (cmd & 0x08) ? 4 : 3;
			bool tex = (cmd & 0x04) != 0, gouraud = (cmd & 0x10) != 0;
			uint32_t need = 1 + nv + (tex ? nv : 0) + (gouraud ? nv - 1 : 0);
			if (w + need > words) return false;
			FF8_BGATE_COL(w++);
			for (int v = 0; v < nv; v++)
			{
				if (v > 0 && gouraud) FF8_BGATE_COL(w++);
				FF8_BGATE_XY(w++);
				if (tex)
				{
					if (v == 1) pi.tpage = p[w] >> 16;
					if (v < 2) FF8_BGATE_SIG(p[w] >> 16); // v0: CLUT, v1: texture page
					FF8_BGATE_UV(p[w]);
					w++;
				}
			}
		}
		else if (cmd >= 0x40 && cmd < 0x60) // line / polyline
		{
			bool poly = (cmd & 0x08) != 0, gouraud = (cmd & 0x10) != 0;
			if (w + 2 > words) return false;
			FF8_BGATE_COL(w++);
			FF8_BGATE_XY(w++);
			if (!poly)
			{
				if (gouraud) { if (w >= words) return false; FF8_BGATE_COL(w++); }
				if (w >= words) return false;
				FF8_BGATE_XY(w++);
			}
			else
			{
				while (w < words && (p[w] & 0xF000F000) != 0x50005000)
				{
					if (gouraud) FF8_BGATE_COL(w++);
					if (w >= words) return false;
					FF8_BGATE_XY(w++);
				}
				w++; // terminator
			}
		}
		else if (cmd >= 0x60 && cmd < 0x80) // rectangle / sprite: color, xy, [uv+clut], [w,h]
		{
			bool tex = (cmd & 0x04) != 0, var = (cmd & 0x18) == 0;
			uint32_t need = 2 + (tex ? 1 : 0) + (var ? 1 : 0);
			if (w + need > words) return false;
			FF8_BGATE_COL(w++);
			FF8_BGATE_XY(w++);
			if (tex) { FF8_BGATE_SIG(p[w] >> 16); FF8_BGATE_UV(p[w]); w++; }
			if (var) { FF8_BGATE_SIG(p[w]); w++; }
		}
		else if (cmd >= 0xE1 && cmd <= 0xE6) // draw mode / texture window / area / offset / mask
		{
			FF8_BGATE_SIG(p[w]);
			w++;
		}
		else if (cmd == 0x00 && p[w] == 0) // NOP padding
			w++;
		else
			return false;
	}
	pi.sig = sig;
	pi.sig_uv = (uv == 2166136261u) ? 0 : (sig ^ uv) * 16777619u; // 0 = untextured: no strong identity
	return true;
	#undef FF8_BGATE_SIG
	#undef FF8_BGATE_XY
	#undef FF8_BGATE_COL
	#undef FF8_BGATE_UV
}

// Largest per-vertex screen step between the two versions of a primitive
static int ff8_bgate_pkt_step(const uint32_t *a, const uint32_t *b, const ff8_bgate_pkt_info &pi)
{
	int d = 0;
	for (int i = 0; i < pi.nxy; i++)
	{
		uint32_t va = a[pi.xy[i]], vb = b[pi.xy[i]];
		int dx = abs((int)(int16_t)(va & 0xFFFF) - (int)(int16_t)(vb & 0xFFFF));
		int dy = abs((int)(int16_t)(va >> 16) - (int)(int16_t)(vb >> 16));
		if (dx > d) d = dx;
		if (dy > d) d = dy;
	}
	return d;
}

// round(v * num / den), symmetric around 0
static inline int ff8_bgate_scale_round(int v, int num, int den)
{
	int t = v * num;
	return (t >= 0) ? (t + den / 2) / den : -((-t + den / 2) / den);
}

#define FF8_BGATE_FX_MAX_COLOR_STEP 12
// dst (a copy of cur) := cur + (cur - prev) * num / den, on vertices and vertex colors
static int ff8_bgate_pkt_extrapolate(uint32_t *dst, const uint32_t *cur, const uint32_t *prev, const ff8_bgate_pkt_info &pi, int num, int den, bool colors, int cap = FF8_BGATE_FX_MAX_COLOR_STEP)
{
	int max_dc = 0;
	for (int i = 0; i < pi.nxy; i++)
	{
		uint32_t c = cur[pi.xy[i]], p = prev[pi.xy[i]];
		int cx = (int16_t)(c & 0xFFFF), cy = (int16_t)(c >> 16);
		int px = (int16_t)(p & 0xFFFF), py = (int16_t)(p >> 16);
		int x = cx + ff8_bgate_scale_round(cx - px, num, den);
		int y = cy + ff8_bgate_scale_round(cy - py, num, den);
		if (x < -32768) x = -32768; if (x > 32767) x = 32767;
		if (y < -32768) y = -32768; if (y > 32767) y = 32767;
		dst[pi.xy[i]] = (uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16);
	}
	if (!colors)
		return 0;
	for (int i = 0; i < pi.ncol; i++)
	{
		uint32_t c = cur[pi.col[i]], p = prev[pi.col[i]];
		uint32_t out = c & 0xFF000000; // command byte (first color word) stays
		for (int s = 0; s < 24; s += 8)
		{
			int cc = (c >> s) & 0xFF, pc = (p >> s) & 0xFF;
			int step = ff8_bgate_scale_round(cc - pc, num, den);
			// capped: a fade keeps its slope, a sudden brightness jump (flash onset) is not
			// amplified - overshooting additive primitives saturate to white for one frame
			if (step > cap) step = cap;
			if (step < -cap) step = -cap;
			int v = cc + step;
			if (v < 0) v = 0; if (v > 255) v = 255;
			if (abs(v - cc) > max_dc) max_dc = abs(v - cc);
			out |= (uint32_t)v << s;
		}
		dst[pi.col[i]] = out;
	}
	return max_dc;
}

// Indexes the tick's primitives by identity, so the next held frame can find each one's
// previous-tick counterpart without depending on the (culling-sensitive) emission order
static void ff8_bgate_fx_index(ff8_bgate_fx_snap &s)
{
	static ff8_bgate_pkt_info pi;
	for (int h = 0; h < FF8_BGATE_FX_HASH; h++)
		s.head_uv[h] = s.head_w[h] = -1;
	for (int i = s.n - 1; i >= 0; i--) // backwards so chains come out in emission order
	{
		ff8_bgate_fx_prim &e = s.prim[i];
		e.parsed = ff8_bgate_pkt_parse(&s.arena[e.off], e.words, pi) && pi.nxy > 0;
		e.sig = e.parsed ? pi.sig : 0;
		e.sig_uv = e.parsed ? pi.sig_uv : 0;
		e.next_uv = e.next_w = -1;
		if (!e.parsed)
			continue;
		uint32_t hu = e.sig_uv & (FF8_BGATE_FX_HASH - 1), hw = e.sig & (FF8_BGATE_FX_HASH - 1);
		if (e.sig_uv != 0) { e.next_uv = s.head_uv[hu]; s.head_uv[hu] = i; }
		e.next_w = s.head_w[hw]; s.head_w[hw] = i;
	}
}

// End of a real tick: copy every node the effect created, while the packets are fresh
static void ff8_bgate_fx_capture_unsafe(ff8_bgate_fx_snap &s)
{
	uint32_t begin = ff8_bgate_fx_rec_begin, end = FF8_BGATE_EXEC_CUR;
	if (end < begin || (end - begin) % sizeof(ff8_bgate_exec_node) != 0)
		return; // arena was flushed during the tick - nothing trustworthy to replay
	int n = (int)((end - begin) / sizeof(ff8_bgate_exec_node));
	if (n > FF8_BGATE_FX_MAX_PRIMS)
	{
		ffnx_info("30fps fx: %d draws in one effect tick (> %d), held-frame redraw skipped\n", n, FF8_BGATE_FX_MAX_PRIMS);
		return;
	}
	for (int i = 0; i < n; i++)
		ff8_bgate_fx_node_bucket[i] = -1;

	// bucket of each node: walk every bucket's chain while it stays inside the tick's nodes
	uint32_t *ot = (uint32_t *)ff8_bgate_fx_rec_ot;
	for (int b = 0; b < FF8_BGATE_OT_BUCKETS; b++)
	{
		uint32_t v = ot[b];
		while (v >= begin && v < end && (v - begin) % sizeof(ff8_bgate_exec_node) == 0)
		{
			int i = (int)((v - begin) / sizeof(ff8_bgate_exec_node));
			if (ff8_bgate_fx_node_bucket[i] >= 0)
				break;
			ff8_bgate_fx_node_bucket[i] = (int16_t)b;
			const ff8_bgate_exec_node *node = (const ff8_bgate_exec_node *)v;
			v = (node->pkt[0] & 0xFFFFFF) | ((uint32_t)node->link_hi << 24);
		}
	}

	for (int i = 0; i < n; i++)
	{
		const ff8_bgate_exec_node *node = (const ff8_bgate_exec_node *)(begin + i * sizeof(ff8_bgate_exec_node));
		if (ff8_bgate_fx_node_bucket[i] < 0)
		{
			s.orphans++;
			continue;
		}
		uint32_t words = (node->pkt[0] >> 24) + 1;
		if (s.used + words > FF8_BGATE_FX_ARENA_WORDS)
			return;
		ff8_bgate_fx_prim &d = s.prim[s.n++];
		memcpy(d.k, node->k, sizeof(d.k));
		d.bucket = (uint16_t)ff8_bgate_fx_node_bucket[i];
		d.msk = node->msk;
		d.ei = (uint16_t)i;
		d.off = s.used;
		d.words = words;
		d.src_pkt = node->pkt;
		memcpy(&s.arena[s.used], node->pkt, words * 4);
		s.used += words;
	}
	ff8_bgate_fx_index(s);
	s.valid = true;
}

// PACKET ALIASING (Eden's "warped screen", vanilla behaviour): a cinematic effect's tick saves
// the packet-arena cursor (battle_texture_data_ptr), draws - some bones allocate their packets
// IN THE FRAME ARENA - then rewinds the cursor. The stage/entity draws that follow in BdLink
// allocate from the same cursor and OVERWRITE those packets before the ordering table is
// executed (display @0x5006DF), so at display time the effect's OT nodes point at other draws'
// data: that aliased content is what the screen shows every frame in vanilla (stable). The
// copies taken at tick end held the effect's TRUE packets, so held frames showed something the
// real frames never did -> the ghost flashed at 15Hz. Re-reading every captured packet from its
// original address at display time reproduces exactly what vanilla displays; the number of
// packets that changed in between is reported (realiased) to confirm the mechanism.
static bool ff8_bgate_fx_recapture_pending = false;
// Cinematic effects call SSIGPU_ClearOrderingTable(OT, 4096) at the start of their tick,
// discarding everything inserted earlier in the frame (camera/AnimSeq/hit-effect queues).
// Detected on real ticks (a bucket that held nodes before the tick is back to its cleared
// link afterwards) and reproduced on held frames before the replay, so the same draws are
// discarded on both kinds of frame.
static bool ff8_bgate_fx_otclear = false;
static uint32_t ff8_bgate_fx_ot_probe[2]; // bucket 1 and 4095 heads before the tick
static uint32_t ff8_bgate_fx_realiased = 0;

static void ff8_bgate_fx_recapture_unsafe(ff8_bgate_fx_snap &s)
{
	for (int i = 0; i < s.n; i++)
	{
		ff8_bgate_fx_prim &d = s.prim[i];
		const uint32_t *pkt = d.src_pkt;
		if (!pkt) continue;
		uint32_t words = (pkt[0] >> 24) + 1;
		if (words != d.words) // size byte overwritten too: keep the copy that still fits
			words = (words < d.words) ? words : d.words;
		if (memcmp(&s.arena[d.off], pkt, words * 4) != 0)
		{
			memcpy(&s.arena[d.off], pkt, words * 4);
			ff8_bgate_fx_realiased++;
		}
	}
	ff8_bgate_fx_index(s); // identities may have changed with the content
}

static void ff8_bgate_fx_recapture_at_display()
{
	if (!ff8_bgate_fx_recapture_pending) return;
	ff8_bgate_fx_recapture_pending = false;
	ff8_bgate_R = &ff8_bgate_rec_fx;
	ff8_bgate_fx_snap &s = ff8_bgate_fx_snaps[ff8_bgate_fx_cur];
	if (!s.valid) return;
	uint32_t before = ff8_bgate_fx_realiased;
	__try
	{
		ff8_bgate_fx_recapture_unsafe(s);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ffnx_info("30fps: display-time packet re-read faulted (effect_ctx=%p)\n", s.ctx);
	}
	static uint32_t logged = 0;
	if (ff8_bgate_fx_sum.ticks <= 8) logged = 0;
	if (logged < 8)
	{
		logged++;
		ffnx_info("30fps alias: f=%u effect_id=%d prims=%d realiased_this_frame=%u otclear_seen=%d\n",
			ff8_bgate_frame_no, *(int *)0x1D99A68 + 1, s.n, ff8_bgate_fx_realiased - before, (int)ff8_bgate_fx_otclear);
	}
}

// display_texture_related_sub_45D610 (0x45D610): executes the battle OT (SSIGPU_DrawOrderingTableAndReset)
static int (__cdecl *ff8_bgate_display_orig)(unsigned int) = nullptr;
static uint32_t ff8_bgate_display_ri = 0;

int __cdecl ff8_bgate_display_hook(unsigned int arg)
{
	ff8_bgate_fx_recapture_at_display();
	unreplace_function(ff8_bgate_display_ri);
	int r = ff8_bgate_display_orig(arg);
	rereplace_function(ff8_bgate_display_ri);
	return r;
}

static void ff8_bgate_fx_capture(void *effect_ctx)
{
	ff8_bgate_fx_cur ^= 1;
	ff8_bgate_fx_snap &s = ff8_bgate_fx_snaps[ff8_bgate_fx_cur];
	s.n = 0;
	s.ntasks = 0; // no task tags unless attached for this tick
	s.orphans = 0;
	s.rlist_delta = FF8_BGATE_RLIST_CUR - ff8_bgate_fx_rec_rlist;
	s.used = 0;
	s.ctx = effect_ctx;
	s.frame = ff8_bgate_frame_no;
	s.valid = false;
	__try
	{
		ff8_bgate_fx_capture_unsafe(s);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		s.valid = false;
		ffnx_info("30fps: effect_ctx=%p packet copy faulted, held frame skipped\n", effect_ctx);
	}
}

// Blacklist of effect_ctx whose replay faulted once - never replayed again (that effect
// steps at native 15fps with no held-frame redraw, everything else keeps replaying).

static bool ff8_bgate_fx_replay_is_blacklisted(void *ctx)
{
	for (int i = 0; i < ff8_bgate_fx_replay_blacklist_n; i++)
		if (ff8_bgate_fx_replay_blacklist[i] == ctx) return true;
	return false;
}

static void ff8_bgate_fx_replay_blacklist_add(void *ctx)
{
	if (ff8_bgate_fx_replay_is_blacklisted(ctx)) return;
	if (ff8_bgate_fx_replay_blacklist_n < 16) ff8_bgate_fx_replay_blacklist[ff8_bgate_fx_replay_blacklist_n++] = ctx;
}

// Vertex weld table for one held frame: screen position of a tick-N vertex -> summed
// displacement of every paired primitive that has a vertex there. Generation-stamped so it
// never needs clearing (a stale entry from an older frame reads as empty).
#define FF8_BGATE_WELD_SLOTS 65536 // power of two, > 4 x FF8_BGATE_FX_MAX_PRIMS
#define FF8_BGATE_WELD_SAMPLES 8
struct ff8_bgate_weld_slot { uint32_t key, gen; int32_t n; int16_t dx[FF8_BGATE_WELD_SAMPLES], dy[FF8_BGATE_WELD_SAMPLES]; };
static ff8_bgate_weld_slot ff8_bgate_weld[FF8_BGATE_WELD_SLOTS];
static uint32_t ff8_bgate_fx_used[FF8_BGATE_FX_MAX_PRIMS]; // prev primitive already paired (== weld gen)
static uint32_t ff8_bgate_weld_gen = 0;

static ff8_bgate_weld_slot *ff8_bgate_weld_find(uint32_t key, bool create)
{
	uint32_t h = (key * 2654435761u) >> 16;
	for (int probe = 0; probe < 64; probe++)
	{
		ff8_bgate_weld_slot &s = ff8_bgate_weld[(h + probe) & (FF8_BGATE_WELD_SLOTS - 1)];
		if (s.gen != ff8_bgate_weld_gen)
		{
			if (!create) return nullptr;
			s.gen = ff8_bgate_weld_gen; s.key = key; s.n = 0;
			return &s;
		}
		if (s.key == key) return &s;
	}
	return nullptr; // table saturated around this key: vertex simply is not welded
}

static void ff8_bgate_weld_add(uint32_t key, int dx, int dy)
{
	ff8_bgate_weld_slot *s = ff8_bgate_weld_find(key, true);
	if (!s || s->n >= FF8_BGATE_WELD_SAMPLES) return;
	s->dx[s->n] = (int16_t)dx;
	s->dy[s->n] = (int16_t)dy;
	s->n++;
}

static int ff8_bgate_median(int16_t *v, int n)
{
	for (int i = 1; i < n; i++) // n <= 8: insertion sort
	{
		int16_t x = v[i];
		int j = i - 1;
		while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
		v[j + 1] = x;
	}
	return v[(n - 1) / 2]; // lower median: an even split leans to the smaller-index sample
}

// One displacement per vertex position, voted by every paired primitive touching it: the
// component-wise median, so a minority of wrong pairs (repeated texture tiles give several
// triangles identical UVs) is outvoted instead of tearing the mesh. Two disagreeing votes
// keep the smaller movement - the conservative choice when there is no majority.
static bool ff8_bgate_weld_get(uint32_t key, int &dx, int &dy)
{
	ff8_bgate_weld_slot *s = ff8_bgate_weld_find(key, false);
	if (!s || s->n == 0) return false;
	if (s->n == 2)
	{
		int a = abs(s->dx[0]) + abs(s->dy[0]), b = abs(s->dx[1]) + abs(s->dy[1]);
		int k = (a <= b) ? 0 : 1;
		dx = s->dx[k]; dy = s->dy[k];
		return true;
	}
	int16_t tx[FF8_BGATE_WELD_SAMPLES], ty[FF8_BGATE_WELD_SAMPLES];
	memcpy(tx, s->dx, s->n * sizeof(int16_t));
	memcpy(ty, s->dy, s->n * sizeof(int16_t));
	dx = ff8_bgate_median(tx, s->n);
	dy = ff8_bgate_median(ty, s->n);
	return true;
}

// A primitive and its true counterpart one native tick earlier have nearly the same shape;
// a wrong pair (a neighbouring tile with the same UVs, a different particle) usually does
// not. Rejects pairs whose edges changed length by more than 1.5x (edges under 4px ignored).
static bool ff8_bgate_pkt_same_shape(const uint32_t *a, const uint32_t *b, const ff8_bgate_pkt_info &pi)
{
	if (pi.nxy < 3)
		return true; // sprites/lines: position only, nothing to compare
	for (int i = 0; i < pi.nxy; i++)
	{
		int j = (i + 1) % pi.nxy;
		uint32_t a0 = a[pi.xy[i]], a1 = a[pi.xy[j]], b0 = b[pi.xy[i]], b1 = b[pi.xy[j]];
		int ax = (int16_t)(a1 & 0xFFFF) - (int16_t)(a0 & 0xFFFF), ay = (int16_t)(a1 >> 16) - (int16_t)(a0 >> 16);
		int bx = (int16_t)(b1 & 0xFFFF) - (int16_t)(b0 & 0xFFFF), by = (int16_t)(b1 >> 16) - (int16_t)(b0 >> 16);
		long long la = (long long)ax * ax + (long long)ay * ay, lb = (long long)bx * bx + (long long)by * by;
		if (la < 16 && lb < 16)
			continue;
		// (1.5)^2 = 2.25 -> 4 * la > 9 * lb, both ways
		if (4 * la > 9 * lb || 4 * lb > 9 * la)
			return false;
	}
	return true;
}

// Held frame: insert an extrapolated copy of every primitive of tick N into this frame's OT
// look-ahead pairing: the counterpart of tick-N primitive i inside tick N+1 (la), by task:
// same task node, same rank when the task drew the same number of primitives, else the
// nearest same primitive within that task (and only when clearly nearer than the runner-up)
static int ff8_bgate_la_pair(const ff8_bgate_fx_snap &cur, const ff8_bgate_fx_snap &la, int i, const ff8_bgate_pkt_info &ci, const uint32_t *src, int &d)
{
	const ff8_bgate_fx_prim &e = cur.prim[i];
	int t = cur.task_of[i];
	if (t < 0) return -1;
	static int cache_t = -1, cache_lt = -1;
	static uint32_t cache_frame = 0xFFFFFFFF;
	if (cache_t != t || cache_frame != ff8_bgate_frame_no)
	{
		cache_t = t; cache_frame = ff8_bgate_frame_no; cache_lt = -1;
		for (int k = 0; k < la.ntasks; k++)
			if (la.tasks[k].node == cur.tasks[t].node) { cache_lt = k; break; }
	}
	if (cache_lt < 0) return -1;
	const ff8_bgate_task_rng &ct = cur.tasks[t], &lt = la.tasks[cache_lt];
	if (ct.b - ct.a == lt.b - lt.a)
	{
		uint32_t ei = lt.a + (e.ei - ct.a);
		int j = ei < FF8_BGATE_FX_MAX_PRIMS ? la.ei2prim[ei] : -1;
		if (j >= 0 && la.prim[j].words == e.words && la.prim[j].sig == ci.sig && ff8_bgate_fx_used[j] != ff8_bgate_weld_gen)
		{
			d = ff8_bgate_pkt_step(src, &la.arena[la.prim[j].off], ci);
			return d <= FF8_BGATE_FX_MAX_STEP_INORDER ? j : -1;
		}
	}
	int best = -1, best_d = 0x7FFFFFFF, second_d = 0x7FFFFFFF;
	for (uint32_t ei = lt.a; ei < lt.b && ei < FF8_BGATE_FX_MAX_PRIMS; ei++)
	{
		int j = la.ei2prim[ei];
		if (j < 0 || la.prim[j].words != e.words || la.prim[j].sig != ci.sig || ff8_bgate_fx_used[j] == ff8_bgate_weld_gen) continue;
		int dd = ff8_bgate_pkt_step(src, &la.arena[la.prim[j].off], ci);
		if (dd < best_d) { second_d = best_d; best = j; best_d = dd; }
		else if (dd < second_d) second_d = dd;
	}
	if (best < 0 || best_d > FF8_BGATE_FX_MAX_STEP) return -1;
	if (second_d != 0x7FFFFFFF && best_d * 2 + 2 > second_d) return -1;
	d = best_d;
	return best;
}

static bool ff8_bgate_fx_dump_armed = true; // TEMP diagnostics: dump once per game run

// Old approximate held frames (generic 2D interpolation, look-ahead, timeline creature
// half-steps, cinematic op replay): superseded by the native held frames, off.
#define FF8_BGATE_GENERIC_INTERP 0
#define FF8_BGATE_LEGACY_HELD 0
static bool ff8_bgate_fx_skip_held = false; // replay without the tasks redrawn natively
static uint32_t ff8_bgate_fx_unclaimed_prims = 0; // old-frame prims no native held task claims (not drawn)

static void ff8_bgate_fx_replay_unsafe()
{
	const ff8_bgate_fx_snap &cur = ff8_bgate_fx_snaps[ff8_bgate_fx_cur];
	const ff8_bgate_fx_snap &prev = ff8_bgate_fx_snaps[ff8_bgate_fx_cur ^ 1];
	const ff8_bgate_fx_snap &la = ff8_bgate_R->la;
	// look-ahead mode: interpolate toward the exact next tick (per-task pairing)
	bool la_mode = ff8_bgate_la_active && la.valid && la.ctx == cur.ctx && la.frame == cur.frame;
	// only interpolate against the directly preceding real tick of the same effect
	// Exact rule: the generic 2D interpolation (moving the last tick's primitives by guesswork) is
	// off. A held frame of an effect without a native held frame repeats the last real frame as is,
	// which is what vanilla shows at 15 fps. (FF8_BGATE_GENERIC_INTERP re-enables it for study.)
	bool interp = FF8_BGATE_GENERIC_INTERP && (la_mode || ((ff8_bgate_fx_mode == 1 || ff8_bgate_fx_mode == 2) && prev.valid && prev.ctx == cur.ctx && cur.frame - prev.frame == (uint32_t)ff8_bgate_n));
	int st_match = 0, st_nosig = 0, st_far = 0, st_unparsed = 0, st_maxcol = 0, st_welded = 0;
	uint32_t st_badcmd = 0;
	uint32_t cur_ot = FF8_BGATE_CUR_OT();
	uint32_t out_used = 0;
	static ff8_bgate_pkt_info ci;

	// Pass 1: pair primitives, extrapolate colors of the paired ones, and collect one screen
	// displacement per VERTEX POSITION (welding). Pass 2 applies those displacements to every
	// primitive touching the position, paired or not - so triangles sharing a corner always
	// move that corner identically and meshes never crack open along their edges (per-primitive
	// decisions left gaps wherever a neighbour was rejected: Leviathan showed its triangles).
	ff8_bgate_weld_gen++;
	// TEMP diagnostics: per-primitive dump (Eden 206 screen collapse, ticks 61..64)
	bool dump = ff8_bgate_R == &ff8_bgate_rec_fx && *(int *)0x1D99A68 + 1 == 206 && false && ff8_bgate_fx_dump_armed;
	static int dump_pair[2048], dump_d[2048]; static uint8_t dump_kind[2048];
	for (int i = 0; i < cur.n; i++)
	{
		const ff8_bgate_fx_prim &e = cur.prim[i];
		const uint32_t *src = &cur.arena[e.off];
		uint32_t *dst = &ff8_bgate_fx_out[out_used];
		memcpy(dst, src, e.words * 4);
		out_used += e.words;
		if (i < 2048) { dump_pair[i] = -1; dump_d[i] = -1; dump_kind[i] = 0; }

		bool parsed = interp && ff8_bgate_pkt_parse(src, e.words, ci);
		if (interp && !parsed) { st_unparsed++; st_badcmd = ci.last_cmd; ff8_bgate_fx_sum.badcmd = ci.last_cmd; }
		if (parsed && la_mode)
		{
			int d = 0;
			int j = ff8_bgate_la_pair(cur, la, i, ci, src, d);
			if (j >= 0)
			{
				const uint32_t *lp = &la.arena[la.prim[j].off];
				if (ff8_bgate_pkt_same_shape(src, lp, ci))
				{
					ff8_bgate_fx_used[j] = ff8_bgate_weld_gen;
					// dst := cur + (la - cur) * phase / n, colours uncapped (both ends are real)
					int dc = ff8_bgate_pkt_extrapolate(dst, src, lp, ci, -ff8_bgate_phase, ff8_bgate_n, ff8_bgate_fx_mode >= 2, 255);
					if (dc > st_maxcol) st_maxcol = dc;
					for (int v = 0; v < ci.nxy; v++)
					{
						uint32_t c = src[ci.xy[v]], l = lp[ci.xy[v]];
						ff8_bgate_weld_add(c,
							ff8_bgate_scale_round((int)(int16_t)(l & 0xFFFF) - (int)(int16_t)(c & 0xFFFF), ff8_bgate_phase, ff8_bgate_n),
							ff8_bgate_scale_round((int)(int16_t)(l >> 16) - (int)(int16_t)(c >> 16), ff8_bgate_phase, ff8_bgate_n));
					}
					st_match++;
				}
				else st_far++;
			}
			else st_nosig++;
		}
		else if (parsed && (ci.nxy > 0 || ci.ncol > 0))
		{
			// counterpart = the closest not-yet-used primitive of tick N-1 with the same
			// STRONG identity (same UVs: a mesh triangle, or a particle on the same sprite
			// frame); failing that, the same WEAK identity within FF8_BGATE_FX_MAX_STEP
			// (flipbook sprites whose UVs advanced). Independent of emission order, which
			// shifts whenever a few triangles get culled or particles spawn/die.
			int best = -1, best_d = 0x7FFFFFFF, limit = FF8_BGATE_FX_MAX_STEP_INORDER;
			for (int c = ci.sig_uv ? prev.head_uv[ci.sig_uv & (FF8_BGATE_FX_HASH - 1)] : -1; c >= 0; c = prev.prim[c].next_uv)
			{
				const ff8_bgate_fx_prim &q = prev.prim[c];
				if (q.sig_uv != ci.sig_uv || q.words != e.words || ff8_bgate_fx_used[c] == ff8_bgate_weld_gen) continue;
				int d = ff8_bgate_pkt_step(src, &prev.arena[q.off], ci);
				if (d < best_d) { best = c; best_d = d; }
			}
			if (best < 0 || best_d > FF8_BGATE_FX_MAX_STEP_INORDER)
			{
				best = -1; best_d = 0x7FFFFFFF; limit = FF8_BGATE_FX_MAX_STEP;
				int second_d = 0x7FFFFFFF;
				for (int c = prev.head_w[ci.sig & (FF8_BGATE_FX_HASH - 1)]; c >= 0; c = prev.prim[c].next_w)
				{
					const ff8_bgate_fx_prim &q = prev.prim[c];
					if (q.sig != ci.sig || q.words != e.words || ff8_bgate_fx_used[c] == ff8_bgate_weld_gen) continue;
					int d = ff8_bgate_pkt_step(src, &prev.arena[q.off], ci);
					if (d < best_d) { second_d = best_d; best = c; best_d = d; }
					else if (d < second_d) second_d = d;
				}
				// The weak identity is shared by every primitive of a batch (all rows of a
				// screen-warp, all particles of a cloud): "the nearest one" is only meaningful
				// when it is clearly nearer than the runner-up. Eden's TV-collapse draws the
				// screen as ~300 one-pixel rows whose UVs change every tick; each row paired
				// with whatever row sat at its new height and the held frame was torn apart.
				if (best >= 0 && second_d != 0x7FFFFFFF && best_d * 2 + 2 > second_d)
				{
					best = -1;
					ff8_bgate_fx_sum.ambiguous++;
				}
			}
			if (best >= 0 && i < 2048) { dump_pair[i] = best; dump_d[i] = best_d; dump_kind[i] = (limit == FF8_BGATE_FX_MAX_STEP_INORDER) ? 1 : 2; }
			if (best >= 0)
			{
				const uint32_t *qp = &prev.arena[prev.prim[best].off];
				if (best_d <= limit && ff8_bgate_pkt_same_shape(src, qp, ci))
				{
					if (i < 2048) dump_kind[i] |= 0x10; // accepted
					ff8_bgate_fx_used[best] = ff8_bgate_weld_gen;
					int dc = ff8_bgate_pkt_extrapolate(dst, src, qp, ci, ff8_bgate_phase, ff8_bgate_n, ff8_bgate_fx_mode >= 2);
					if (dc > st_maxcol) st_maxcol = dc;
					for (int v = 0; v < ci.nxy; v++)
					{
						uint32_t c = src[ci.xy[v]], p = qp[ci.xy[v]];
						ff8_bgate_weld_add(c,
							ff8_bgate_scale_round((int)(int16_t)(c & 0xFFFF) - (int)(int16_t)(p & 0xFFFF), ff8_bgate_phase, ff8_bgate_n),
							ff8_bgate_scale_round((int)(int16_t)(c >> 16) - (int)(int16_t)(p >> 16), ff8_bgate_phase, ff8_bgate_n));
					}
					st_match++;
				}
				else st_far++;
			}
			else st_nosig++;
		}
	}

	// Pass 2: welded vertex positions, then link every primitive into this frame's OT
	out_used = 0;
	for (int i = 0; i < cur.n; i++)
	{
		const ff8_bgate_fx_prim &e = cur.prim[i];
		const uint32_t *src = &cur.arena[e.off];
		uint32_t *dst = &ff8_bgate_fx_out[out_used];
		out_used += e.words;

		if (interp && ff8_bgate_pkt_parse(src, e.words, ci))
		{
			if (ci.tpage != 0xFFFFFFFF) // diagnostics: which texture pages this effect draws from
			{
				auto &s = ff8_bgate_fx_sum;
				int k = 0;
				while (k < s.ntpages && s.tpages[k] != ci.tpage) k++;
				if (k < 8)
				{
					if (k == s.ntpages) { s.tpages[k] = ci.tpage; s.tpage_hits[k] = 0; s.ntpages++; }
					s.tpage_hits[k]++;
				}
			}
			for (int v = 0; v < ci.nxy; v++)
			{
				uint32_t c = src[ci.xy[v]];
				int dx, dy;
				if (!ff8_bgate_weld_get(c, dx, dy))
					continue; // no paired primitive touches this position: stays as tick N drew it
				// the voted displacement overrides each primitive's own, so shared corners agree
				int x = (int)(int16_t)(c & 0xFFFF) + dx, y = (int)(int16_t)(c >> 16) + dy;
				if (x < -32768) x = -32768; if (x > 32767) x = 32767;
				if (y < -32768) y = -32768; if (y > 32767) y = 32767;
				dst[ci.xy[v]] = (uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16);
				st_welded++;
			}
		}

		// native held frame: the port redraws the whole effect in between; nothing of the last real
		// frame is replayed (the generic 2D warp under the native draw showed the old frame again,
		// ghosted and warped). Primitives no held task claims are counted, not drawn.
		if (ff8_bgate_fx_skip_held)
		{
			if (!(cur.ntasks > 0 && cur.task_of[i] >= 0
				&& ff8fx::held_redraws((uint32_t)((ff8_bgate_task_node *)cur.tasks[cur.task_of[i]].node)->func)))
				ff8_bgate_fx_unclaimed_prims++;
			continue;
		}

		if (ff8_bgate_fx_mode == 4 || ff8_bgate_fx_mode == 5)
		{
			static ff8_bgate_pkt_info bi;
			bool is15 = ff8_bgate_pkt_parse(src, e.words, bi) && bi.tpage != 0xFFFFFFFF && ((bi.tpage >> 7) & 3) == 2;
			if ((ff8_bgate_fx_mode == 4) == is15)
				continue; // mode 4 skips the 15bpp prims, mode 5 keeps only them
		}

		// VRAM transfer commands (GP0 0x80-0x9F VRAM->VRAM copy, 0xA0 CPU->VRAM, 0xC0 VRAM->CPU)
		// are never replayed: effects such as Eden (206) copy the framebuffer into a texture to
		// build mirrors/distortions; repeating that copy on a held frame grabs whatever the
		// screen holds at that moment (other buffer, 30fps UI) -> duplicated, flashing UI.
		// The texture keeps the last real frame's copy, exactly like vanilla at 15fps.
		{
			static ff8_bgate_pkt_info xi;
			if (!ff8_bgate_pkt_parse(src, e.words, xi) && xi.last_cmd >= 0x80 && xi.last_cmd <= 0xDF)
			{
				ff8_bgate_fx_sum.vram_xfer++;
				ff8_bgate_fx_sum.badcmd = xi.last_cmd;
				continue;
			}
		}

		if (dump && i < 2048)
		{
			char l[500]; int o = 0;
			o += _snprintf_s(l + o, sizeof(l) - o, _TRUNCATE, "30fps dump: t=%u i=%d bucket=%u words=%u pair=%d d=%d kind=%02X |", ff8_bgate_fx_sum.ticks, i, e.bucket, e.words, dump_pair[i], dump_d[i], dump_kind[i]);
			for (uint32_t k = 1; k < e.words && k < 10 && o > 0 && o < 400; k++)
				o += _snprintf_s(l + o, sizeof(l) - o, _TRUNCATE, " %08X", src[k]);
			if (o > 0 && o < 400) o += _snprintf_s(l + o, sizeof(l) - o, _TRUNCATE, " | out");
			for (uint32_t k = 1; k < e.words && k < 10 && o > 0 && o < 480; k++)
				if (dst[k] != src[k]) o += _snprintf_s(l + o, sizeof(l) - o, _TRUNCATE, " [%u]=%08X", k, dst[k]);
			if (dump_pair[i] >= 0 && o > 0 && o < 440)
			{
				const uint32_t *qp = &prev.arena[prev.prim[dump_pair[i]].off];
				o += _snprintf_s(l + o, sizeof(l) - o, _TRUNCATE, " | prev %08X %08X %08X", qp[1], e.words > 2 ? qp[2] : 0, e.words > 3 ? qp[3] : 0);
			}
			ffnx_info("%s\n", l);
		}

		// link it exactly like the engine's inserts do (SSIGPU_InsertPrimDepthKeys + msk)
		if (FF8_BGATE_EXEC_CUR - FF8_BGATE_EXEC_START >= FF8_BGATE_EXEC_LIMIT)
			break; // draw list full - same limit the engine applies
		ff8_bgate_exec_node *node = (ff8_bgate_exec_node *)FF8_BGATE_EXEC_CUR;
		uint32_t *bucket = (uint32_t *)cur_ot + e.bucket;
		uint32_t old_head = *bucket;
		node->pkt = dst;
		memcpy(node->k, e.k, sizeof(node->k));
		node->msk = e.msk;
		node->link_hi = (uint8_t)(old_head >> 24);
		*bucket = (uint32_t)node;
		dst[0] = (dst[0] & 0xFF000000) | (old_head & 0xFFFFFF);
		FF8_BGATE_EXEC_CUR += sizeof(ff8_bgate_exec_node);
	}
	if (dump && ff8_bgate_fx_sum.ticks == 300) ff8_bgate_fx_dump_armed = false;
	ff8_bgate_fx_sum.held++;
	if (interp) ff8_bgate_fx_sum.interp_held++;
	ff8_bgate_fx_sum.prims += cur.n;
	ff8_bgate_fx_sum.match += st_match;
	ff8_bgate_fx_sum.far_ += st_far;
	ff8_bgate_fx_sum.nosig += st_nosig;
	ff8_bgate_fx_sum.unparsed += st_unparsed;
	ff8_bgate_fx_sum.welded += st_welded;
	ff8_bgate_fx_sum.orphans += cur.orphans;
	if ((uint32_t)st_maxcol > ff8_bgate_fx_sum.maxcol) ff8_bgate_fx_sum.maxcol = st_maxcol;
	if (ff8_bgate_fx_log_stats)
		ffnx_info("30fps fx: f=%u ctx=%p mode=%d interp=%d prims=%d prev=%d match=%d nosig=%d far=%d unparsed=%d(last cmd %02X) orphans=%d welded=%d maxcol=%d words=%u\n",
			ff8_bgate_frame_no, cur.ctx, ff8_bgate_fx_mode, (int)interp, cur.n, prev.n, st_match, st_nosig, st_far,
			st_unparsed, st_badcmd, cur.orphans, st_welded, st_maxcol, out_used);
}

static void ff8_bgate_fx_replay(void *effect_ctx)
{
	if (ff8_bgate_fx_replay_is_blacklisted(effect_ctx) || !ff8_bgate_fx_snaps[ff8_bgate_fx_cur].valid)
		return;
	__try
	{
		ff8_bgate_fx_replay_unsafe();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ff8_bgate_fx_replay_blacklist_add(effect_ctx);
		ffnx_info("30fps: effect_ctx=%p faulted during held-frame replay -> blacklisted (native 15fps, no redraw)\n", effect_ctx);
	}
}

// ENDING TASKS on a native held frame: a task that ends on a real tick (TASK_END after its draw) is gone from the live
// queues the module's held part walks, but vanilla shows what it drew until the next tick. For the tasks whose module asks
// for it (ff8fx::keep_ended), the packets the task drew on the last real tick are linked again as drawn (fraction 0: no
// next tick to blend toward), in their place among the held draw's insertions [h0, h1): ff8fx::held_merge_kept gives the
// order (the real tick's insertions, the held draw's taking the place of the others); the held draw's nodes are unlinked
// from the heads of their buckets and every node is linked again in that order. If a held node is not at a bucket head
// (own OT, cleared OT...) the kept packets are only added after the held draw.
static uint32_t ff8_bgate_fx_kept_prims = 0; // stats (summary of the native held frames)
static void ff8_bgate_fx_keep_ended_unsafe(uint32_t h0, uint32_t h1)
{
	const ff8_bgate_fx_snap &cur = ff8_bgate_fx_snaps[ff8_bgate_fx_cur];
	if (!cur.valid || cur.ntasks <= 0 || h1 < h0 || (h1 - h0) % sizeof(ff8_bgate_exec_node) != 0) return;
	static bool kept[FF8_BGATE_FX_MAX_PRIMS];
	static ff8_bgate_pkt_info ki;
	int nkept = 0;
	for (int i = 0; i < cur.n; i++)
	{
		const int t = cur.task_of[i];
		kept[i] = t >= 0 && ((cur.tasks[t].ended && ff8fx::keeps_ended(cur.tasks[t].func)) || ff8fx::keeps_drawn(cur.tasks[t].func));
		// (VRAM transfers are never replayed, see ff8_bgate_fx_replay_unsafe)
		if (kept[i] && !ff8_bgate_pkt_parse(&cur.arena[cur.prim[i].off], cur.prim[i].words, ki) && ki.last_cmd >= 0x80 && ki.last_cmd <= 0xDF) kept[i] = false;
		if (kept[i]) nkept++;
	}
	if (!nkept) return;
	uint32_t *ot = (uint32_t *)FF8_BGATE_CUR_OT();
	const uint32_t SZ = sizeof(ff8_bgate_exec_node);
	int nheld = (int)((h1 - h0) / SZ);
	static int16_t held_bucket[FF8_BGATE_FX_MAX_PRIMS];
	bool merge = nheld <= FF8_BGATE_FX_MAX_PRIMS;
	if (merge)
	{
		for (int j = 0; j < nheld; j++) held_bucket[j] = -1;
		int found = 0;
		for (int b = 0; b < FF8_BGATE_OT_BUCKETS; b++)
			for (uint32_t v = ot[b]; v >= h0 && v < h1 && (v - h0) % SZ == 0; )
			{
				const int j = (int)((v - h0) / SZ);
				if (held_bucket[j] >= 0) break;
				held_bucket[j] = (int16_t)b;
				found++;
				const ff8_bgate_exec_node *nd = (const ff8_bgate_exec_node *)v;
				v = (nd->pkt[0] & 0xFFFFFF) | ((uint32_t)nd->link_hi << 24);
			}
		merge = found == nheld;
	}
	if (merge)
		for (int b = 0; b < FF8_BGATE_OT_BUCKETS; b++)
		{
			uint32_t v = ot[b];
			while (v >= h0 && v < h1 && (v - h0) % SZ == 0)
			{
				const ff8_bgate_exec_node *nd = (const ff8_bgate_exec_node *)v;
				v = (nd->pkt[0] & 0xFFFFFF) | ((uint32_t)nd->link_hi << 24);
			}
			ot[b] = v;
		}
	uint32_t out_used = 0;
	auto link = [&](ff8_bgate_exec_node *node, uint32_t *pkt, int bucket) {
		const uint32_t old_head = ot[bucket];
		node->pkt = pkt;
		node->link_hi = (uint8_t)(old_head >> 24);
		ot[bucket] = (uint32_t)node;
		pkt[0] = (pkt[0] & 0xFF000000) | (old_head & 0xFFFFFF);
	};
	ff8fx::held_merge_kept(kept, cur.n, merge ? nheld : 0, [&](bool real, int k) {
		if (!real) { ff8_bgate_exec_node *node = (ff8_bgate_exec_node *)(h0 + (uint32_t)k * SZ); link(node, node->pkt, held_bucket[k]); return; }
		const ff8_bgate_fx_prim &e = cur.prim[k];
		if (out_used + e.words > FF8_BGATE_FX_ARENA_WORDS || FF8_BGATE_EXEC_CUR - FF8_BGATE_EXEC_START >= FF8_BGATE_EXEC_LIMIT) return;
		uint32_t *dst = &ff8_bgate_fx_out[out_used];
		memcpy(dst, &cur.arena[e.off], e.words * 4);
		out_used += e.words;
		ff8_bgate_exec_node *node = (ff8_bgate_exec_node *)FF8_BGATE_EXEC_CUR;
		memcpy(node->k, e.k, sizeof(node->k));
		node->msk = e.msk;
		FF8_BGATE_EXEC_CUR += SZ;
		link(node, dst, e.bucket);
		ff8_bgate_fx_kept_prims++;
	});
}

static void ff8_bgate_fx_keep_ended(void *effect_ctx, uint32_t h0, uint32_t h1)
{
	if (ff8_bgate_fx_replay_is_blacklisted(effect_ctx)) return;
	__try { ff8_bgate_fx_keep_ended_unsafe(h0, h1); }
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ff8_bgate_fx_replay_blacklist_add(effect_ctx);
		ffnx_info("30fps: effect_ctx=%p faulted while keeping the ended tasks' draws -> blacklisted\n", effect_ctx);
	}
}

// One line per finished effect: which effect (MAGIC_EFFECT_INDEX @0x1D99A68 = effect id - 1,
// same numbering as Fujin), how many held frames were interpolated and how well pairing
// went. match% low or unparsed/orphans > 0 point at an effect worth a closer look.
// diagnostics: texture pages the replayed primitives sampled, and the battle's two draw
// environments (PSX DRAWENV: clip rect x/y/w/h then draw offset) - a primitive whose texture
// page points inside a draw buffer is sampling the SCREEN (Eden's mirror/warp).
static const char *ff8_bgate_fx_tpages_str()
{
	static char buf[1024];
	int o = 0;
	auto &s = ff8_bgate_fx_sum;
	#define FF8_BGATE_APPEND(...) do { if (o < (int)sizeof(buf) - 1) o += _snprintf_s(buf + o, sizeof(buf) - o, _TRUNCATE, __VA_ARGS__); if (o < 0 || o > (int)sizeof(buf) - 1) o = (int)sizeof(buf) - 1; } while (0)
	FF8_BGATE_APPEND(" | tpages:");
	for (int i = 0; i < s.ntpages && o < 150; i++)
	{
		uint32_t tp = s.tpages[i];
		// PSX tpage word: bits 0-3 = X base / 64, bit 4 = Y base / 256, bits 5-6 = blend, 7-8 = bpp
		FF8_BGATE_APPEND(" %04X(x=%u,y=%u,abr=%u,bpp=%u)x%u", tp, (tp & 0xF) * 64, ((tp >> 4) & 1) * 256,
			(tp >> 5) & 3, (tp >> 7) & 3, s.tpage_hits[i]);
	}
	int16_t *d0 = (int16_t *)0x1D969C8, *d1 = (int16_t *)0x1D96A24;
	int16_t *p0 = (int16_t *)0x1D96980, *p1 = (int16_t *)0x1D96994; // DISPENV per parity
	FF8_BGATE_APPEND(" | parity=%u dispenv0=%d,%d %dx%d dispenv1=%d,%d %dx%d rlistbase=%08X pktarena=%08X",
		*(uint8_t *)0x1D96A80, p0[0], p0[1], p0[2], p0[3], p1[0], p1[1], p1[2], p1[3],
		*(uint32_t *)0x1D8E04C, *(uint32_t *)0x1D8E054);
	FF8_BGATE_APPEND(" | realiased=%u", ff8_bgate_fx_realiased);
	ff8_bgate_fx_realiased = 0;
	FF8_BGATE_APPEND(" | vramq t0=%u t1=%u t2=%u t3=%u", s.vq_types[0], s.vq_types[1], s.vq_types[2], s.vq_types[3]);
	FF8_BGATE_APPEND(" | drawenv0=%d,%d %dx%d drawenv1=%d,%d %dx%d",
		d0[0], d0[1], d0[2], d0[3], d1[0], d1[1], d1[2], d1[3]);
	#undef FF8_BGATE_APPEND
	return buf;
}

static void ff8_bgate_fx_summary(const char *why)
{
	if (ff8_bgate_fx_sum.ticks == 0 || ff8_bgate_fx_sum.prims == 0)
	{
		// nothing was ever redrawn (empty queue polled, one-tick effect): not worth a line
		memset(&ff8_bgate_fx_sum, 0, sizeof(ff8_bgate_fx_sum));
		return;
	}
	uint32_t pr = ff8_bgate_fx_sum.prims ? ff8_bgate_fx_sum.prims : 1;
	ffnx_info("30fps fx summary (%s %s): effect_id=%d ticks=%u held=%u interpolated=%u | prims=%u paired=%u%% rejected=%u%% new=%u%% unparsed=%u(cmd %02X) vram_xfer_skipped=%u ambiguous=%u orphans=%u welded_vtx=%u maxcol=%u mode=%d%s\n",
		ff8_bgate_R->name, why, ff8_bgate_R == &ff8_bgate_rec_fx ? *(int *)0x1D99A68 + 1 : -1, ff8_bgate_fx_sum.ticks, ff8_bgate_fx_sum.held, ff8_bgate_fx_sum.interp_held,
		ff8_bgate_fx_sum.prims, (ff8_bgate_fx_sum.match * 100) / pr, (ff8_bgate_fx_sum.far_ * 100) / pr,
		(ff8_bgate_fx_sum.nosig * 100) / pr, ff8_bgate_fx_sum.unparsed, ff8_bgate_fx_sum.badcmd, ff8_bgate_fx_sum.vram_xfer, ff8_bgate_fx_sum.ambiguous, ff8_bgate_fx_sum.orphans,
		ff8_bgate_fx_sum.welded, ff8_bgate_fx_sum.maxcol, ff8_bgate_fx_mode, ff8_bgate_fx_tpages_str());
	memset(&ff8_bgate_fx_sum, 0, sizeof(ff8_bgate_fx_sum));
}

// Battle VRAM command queue (32 slots of 16 bytes at 0x1D98220, counter g_next_command_slot
// at 0x1D98420, applied once per frame by Battle_FlushVramCommandQueue): effects queue their
// texture work there. Types: 0 = raw rect upload, 1 = TIM upload, 2 = VRAM read-back,
// 3 = VRAM->VRAM blit. Eden (206) refreshes the screen SNAPSHOT it warps (UI included - that
// ghost is vanilla behaviour) with such a copy every tick; gated to real frames it refreshed
// at 15/s while the screen is redrawn at 30/s, so one frame in two showed a stale snapshot:
// the ghost flashed. Re-issue only the COPY types (2, 3) on held frames - uploads of fixed
// texture data (0, 1) would just cost time.
#define FF8_BGATE_VQ_SLOT(i) ((uint8_t *)(0x1D98220 + 16 * (i)))
#define FF8_BGATE_VQ_COUNT (*(int *)0x1D98420)
static uint8_t ff8_bgate_vq_copy[8][16];
static int ff8_bgate_vq_copies = 0;

static uint32_t ff8_bgate_vq_trace = 0; // frames left to trace

static void ff8_bgate_vq_dump(const char *what, uint8_t *c)
{
	int16_t *r = (int16_t *)(c + 4);
	uint32_t d = *(uint32_t *)(c + 12);
	ffnx_info("30fps vq: f=%u ph=%d parity=%u arena=%08X rlist=%08X %s type=%u src=(%d,%d %dx%d) dst=(%u,%u) raw=%08X %08X %08X %08X\n",
		ff8_bgate_frame_no, ff8_bgate_phase, *(uint8_t *)0x1D96A80, *(uint32_t *)0x1D8E054, *(uint32_t *)0x1D8E04C,
		what, c[0], r[0], r[1], r[2], r[3],
		d & 0xFFFF, d >> 16, *(uint32_t *)c, *(uint32_t *)(c + 4), *(uint32_t *)(c + 8), d);
}

static void ff8_bgate_vq_record(int from, int to)
{
	ff8_bgate_vq_copies = 0;
	for (int i = from; i < to && i < 32; i++)
	{
		uint8_t *c = FF8_BGATE_VQ_SLOT(i);
		if (ff8_bgate_vq_trace) ff8_bgate_vq_dump("queued-by-effect", c);
		if ((ff8_bgate_vq_replay_all || c[0] == 2 || c[0] == 3) && ff8_bgate_vq_copies < 8)
			memcpy(ff8_bgate_vq_copy[ff8_bgate_vq_copies++], c, 16);
	}
}

static void ff8_bgate_vq_replay()
{
	for (int i = 0; i < ff8_bgate_vq_copies; i++)
	{
		int slot = FF8_BGATE_VQ_COUNT;
		if (slot >= 31) return; // the engine's own bound (slot 31 is never written)
		memcpy(FF8_BGATE_VQ_SLOT(slot), ff8_bgate_vq_copy[i], 16);
		FF8_BGATE_VQ_COUNT = slot + 1;
		if (ff8_bgate_vq_trace) ff8_bgate_vq_dump("re-queued-held", FF8_BGATE_VQ_SLOT(slot));
	}
}

// TEMP diagnostics: frame-state differ. Snapshots renderer / camera / battle / cinematic
// globals at the effect-tick point on every frame; on selected held frames logs the dwords
// that differ from the last real frame but did NOT differ between the last two real frames.
struct ff8_bgate_diff_region { uint32_t addr, dwords; };
static const ff8_bgate_diff_region ff8_bgate_diff_regions[] = {
	{ 0x1D8E000, 64 },  // battle render list header / packet arena ptrs / fov / screen offsets
	{ 0xB7CC00, 16 },   // SSIGPU pass state (B7CC00/04/1C/20/24)
	{ 0x1D97700, 48 },  // battle camera globals
	{ 0x1D96A80, 12 },  // parity, update flags, queues
	{ 0x2797300, 456 }, // cinematic engine runtime globals
	{ 0x1CA8828, 8 },   // ssigpu exec cursor etc.
};
#define FF8_BGATE_DIFF_TOTAL (64 + 16 + 48 + 12 + 456 + 8)
static uint32_t ff8_bgate_diff_real[2][FF8_BGATE_DIFF_TOTAL]; // [0] = last real, [1] = the one before
static bool ff8_bgate_diff_real_ok[2];

static void ff8_bgate_diff_snapshot(uint32_t *dst)
{
	int o = 0;
	for (auto &r : ff8_bgate_diff_regions)
	{
		memcpy(dst + o, (void *)r.addr, r.dwords * 4);
		o += r.dwords;
	}
}

static void ff8_bgate_diff_frame(bool real)
{
	static uint32_t held[FF8_BGATE_DIFF_TOTAL];
	if (real)
	{
		memcpy(ff8_bgate_diff_real[1], ff8_bgate_diff_real[0], sizeof(ff8_bgate_diff_real[0]));
		ff8_bgate_diff_real_ok[1] = ff8_bgate_diff_real_ok[0];
		ff8_bgate_diff_snapshot(ff8_bgate_diff_real[0]);
		ff8_bgate_diff_real_ok[0] = true;
		return;
	}
	uint32_t tk = ff8_bgate_fx_sum.ticks;
	if (!(tk >= 150 && tk <= 154) || !ff8_bgate_diff_real_ok[0] || !ff8_bgate_diff_real_ok[1])
		return;
	ff8_bgate_diff_snapshot(held);
	char line[900]; int o = 0, n = 0;
	int idx = 0;
	for (auto &r : ff8_bgate_diff_regions)
	{
		for (uint32_t i = 0; i < r.dwords; i++, idx++)
		{
			uint32_t h = held[idx], r0 = ff8_bgate_diff_real[0][idx], r1 = ff8_bgate_diff_real[1][idx];
			if (h != r0 && r0 == r1)
			{
				if (o < (int)sizeof(line) - 40)
					o += sprintf(line + o, " %08X:%08X->%08X", r.addr + i * 4, r0, h);
				n++;
			}
		}
	}
	line[o] = 0;
	ffnx_info("30fps diff: f=%u tick=%u held-vs-real (stable across real) n=%d%s\n", ff8_bgate_frame_no, tk, n, line);
}

// --- dedicated held-frame path for the "cinematic engine" GF summons ("true 30 fps") ---
// Ifrit 201, Leviathan 006, Bahamut 202, Cerberus 203, Alexander 204, Brothers 205 and Eden 206
// are seven compilations of one engine (study: gf_study/cinematic_*.md, true30_*.md). A tick is
//   SequenceTick: ++seqCounter, rand(), copy the battle view into node 0 (CamMatrixMain),
//                 if (!paused) { integrator (vel += acc; accum += vel; outputs), anim VM }
//                 BuildMatricesAndDraw                                    <- DrawHandlerTable[bone+0x1C]
// On PC the whole VM runs after the integrator; the integrator is the ONLY per-tick rate in the
// animation: everything else is script events separated by waits. So the exact in-between state
// of tick k-1 -> k is known before tick k runs: accum_{k-1} + vel_k / 2 with vel_k = vel + acc.
// A held frame (host frame between two ticks) therefore
//   - calls the effect queue with battle_to_update_flags bit 0 set: SequenceTick runs its preamble
//     (node 0 = the smoothed battle view, packet cursors) and skips the animation phases;
//     seqCounter and the CRT rand seed are put back afterwards;
//   - inside BuildMatricesAndDraw (hooked): snapshots the bones, node matrices, light sets, the
//     particle pools and the lazy handler pools, then writes the midpoint state: integrator half
//     step per bone of the bone order list (+ its bone handler), particles advanced by half a
//     step, and REPLAYS the pure output opcodes the VM executed during the last real tick (node
//     matrices, light sets, camera, colours, shadow) - pure functions of the bone outputs, node 0
//     and their operands, so they yield the exact in-between matrices with the new camera;
//   - draws with rt->boneSkipFlag = 0xFF (the engine's own draw-without-advancing switch for the
//     embedded model / sprite / particle handlers) and restores everything.
// Real ticks run unmodified, so every second host frame is bit-identical to vanilla.
// The generic 2D replay remains the fallback (F6, and anything not recognised).
struct ff8_bgate_gfc_module { int effect_id; uint32_t build_draw, draw_table, step_model, bone_table; const char *name; };
static const ff8_bgate_gfc_module ff8_bgate_gfc_modules[] = {
	{ 201, 0xB2ABE0, 0x1874D6C, 0xB26AD0, 0x1874B80, "Ifrit" },
	{ 6,   0xB5F5D0, 0x18776C0, 0xB5A480, 0x18774D4, "Leviathan" },
	{ 202, 0xB20270, 0x18741B8, 0xB19E90, 0x1873FCC, "Bahamut" },
	{ 203, 0xB135D0, 0x1873384, 0xB0D3F0, 0x1873198, "Cerberus" },
	{ 204, 0xB06E00, 0x187281C, 0xB00FF0, 0x1872630, "Alexander" },
	{ 205, 0xAF9ED0, 0x1871CA8, 0xAF5760, 0x1871ABC, "Brothers" },
	{ 206, 0xAEEC40, 0x187119C, 0xAE4150, 0x1870F78, "Eden" },
};
#define FF8_BGATE_GFC_MODULES 7
#define FF8_BGATE_GFC_VM_TABLE(dt)   ((dt) + 0x1A4) // VmOpcodeTable follows the draw table (328 slots)
// pure output opcodes recorded during the real tick and replayed on the held frame (true30_anim_side.md 4.4):
// matrix nodes / billboards, light set, camera (sub-op 0 only) + shake, colours from outputs,
// entity transform, entity shadow draw
static const int ff8_bgate_gfc_ops[] = { 0x65, 0x66, 0x67, 0x69, 0x6A, 0x7A, 0x84, 0xC4, 0x104, 0x105, 0x116, 0xCB,
	0x93, 0x39, 0x47, 0x40, 0x83, 0xD3, 0x48, 0x71 };
#define FF8_BGATE_GFC_OPS 20

#define FF8_BGATE_GFC_CTX      (*(uint8_t **)0x27973EC)  // g_GfCinematic_SequenceCtxPtr
#define FF8_BGATE_GFC_RT       (*(uint8_t **)0x27973B8)  // g_GfCinematic_RuntimeSlotPtr
#define FF8_BGATE_GFC_CURBONE  (*(uint8_t **)0x27973E8)  // g_GfCinematic_CurBonePtr
#define FF8_BGATE_GFC_CURSOR   (*(uint32_t *)0x2797450)  // g_GfCinematic_StreamCursor (VM instruction pointer)
#define FF8_BGATE_GFC_BONEORDER ((uint8_t *)0x2797454)   // g_GfCinematic_BoneOrderList (0xFF terminated, bit7 = runs while frozen)
#define FF8_BGATE_GFC_DRAWORDER ((uint8_t *)0x2797554)   // g_GfCinematic_DrawOrderList (0xFF terminated)
#define FF8_BGATE_GFC_NODES_BEGIN 0x27977A4              // light sets, billboard matrices, 64 node matrices
#define FF8_BGATE_GFC_NODES_END   0x27981E8
#define FF8_BGATE_GFC_MAX_BONES 128
#define FF8_BGATE_GFC_ARENA_MAX (1024 * 1024)
#define FF8_BGATE_GFC_POOL_MAX (256 * 1024)
#define FF8_BGATE_GFC_LOG_MAX 1024

#define ff8_bgate_gfc_enabled ff8_bgate_gfc_f6
static bool ff8_bgate_gfc_redraw = false;   // inside the held-frame second call
static bool ff8_bgate_gfc_logging = false;  // inside a real tick: pure opcodes are recorded
static uint32_t ff8_bgate_gfc_orig_op[FF8_BGATE_GFC_MODULES][FF8_BGATE_GFC_OPS];
static uint32_t ff8_bgate_gfc_build_ri[FF8_BGATE_GFC_MODULES];

struct ff8_bgate_gfc_logent { uint32_t fn; uint8_t *bone; uint32_t cursor; uint16_t opword; uint8_t op_idx; };
static ff8_bgate_gfc_logent ff8_bgate_gfc_log[FF8_BGATE_GFC_LOG_MAX];
static int ff8_bgate_gfc_log_n = 0;
static bool ff8_bgate_gfc_log_overflow = false;

static uint32_t ff8_bgate_gfc_tick = 0;      // real ticks seen for the running summon
static uint32_t ff8_bgate_gfc_faults = 0;    // held-frame faults of the running summon
static uint8_t *ff8_bgate_gfc_ctx = nullptr; // ctx of the running summon (seqCounter at +0x32)
static uint32_t ff8_bgate_gfc_arena_base = 0;
static uint8_t *ff8_bgate_gfc_save_bones = nullptr, *ff8_bgate_gfc_save_arena = nullptr, *ff8_bgate_gfc_save_pools = nullptr;
static uint8_t ff8_bgate_gfc_save_nodes[FF8_BGATE_GFC_NODES_END - FF8_BGATE_GFC_NODES_BEGIN];
static struct { uint32_t redraws, replayed_ops, bones_stepped, particles_stepped, models_stepped, model_faults, arena_bytes; uint8_t handlers[73]; } ff8_bgate_gfc_stats;

// Embedded battle models (draw handler 3): the creature itself. Its keyframes are read by the
// standard Battle_ReadAnimation, one frame per real tick, from the draw handler. The pose lives in
// the model's skeleton section (root at +8, bones of 48 bytes from +16 with rotations at +4/+6/+8,
// 4096 = one turn) and the reader's whole state is the 8-byte BattleAnimCmd at block+0x20, so the
// NEXT keyframe can be read ahead: snapshot cmd + skeleton, step once exactly as the next tick will
// (restart included), keep the resulting pose, put everything back, then write the midpoint pose
// and let the draw build its matrices from it (draw-only path of the step function).
#define FF8_BGATE_GFC_MODEL_SAVE (128 * 1024)
#define FF8_BGATE_GFC_MODEL_MAX 8
struct ff8_bgate_gfc_model_save { uint8_t *block; uint8_t *skel; uint32_t size; uint32_t off; uint8_t cmd[8]; uint16_t counter; };
static ff8_bgate_gfc_model_save ff8_bgate_gfc_models[FF8_BGATE_GFC_MODEL_MAX];
static int ff8_bgate_gfc_models_n = 0;
static uint8_t *ff8_bgate_gfc_model_buf = nullptr;

static bool ff8_bgate_gfc_step_model_guarded(uint32_t fn, uint8_t *block)
{
	__try
	{
		((void (__cdecl *)(uint8_t *))fn)(block);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

static inline int16_t ff8_bgate_gfc_mid_angle(int16_t cur, int16_t next)
{
	int d = ((int)next - (int)cur) & 0xFFF;
	if (d >= 2048) d -= 4096;
	return (int16_t)(cur + d / 2);
}

static void ff8_bgate_gfc_models_snapshot(uint8_t *bones, int n, int m, bool frozen)
{
	ff8_bgate_gfc_models_n = 0;
	if (!ff8_bgate_gfc_model_buf) ff8_bgate_gfc_model_buf = (uint8_t *)malloc(FF8_BGATE_GFC_MODEL_SAVE);
	if (!ff8_bgate_gfc_model_buf) return;
	uint32_t used = 0;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF && ff8_bgate_gfc_models_n < FF8_BGATE_GFC_MODEL_MAX; i++)
	{
		int id = FF8_BGATE_GFC_DRAWORDER[i] & 0x7F;
		if (id >= n) continue;
		uint8_t *b = bones + id * 0x100;
		if (b[0x1C] != 3) continue;
		uint8_t *block = *(uint8_t **)(b + 0xBC);
		if (!block) continue;
		uint8_t *skel = *(uint8_t **)(block + 0x30);
		if (!skel) continue;
		uint32_t size = 16 + 48 * skel[0];
		if (used + size > FF8_BGATE_GFC_MODEL_SAVE) break;
		ff8_bgate_gfc_model_save &s = ff8_bgate_gfc_models[ff8_bgate_gfc_models_n++];
		s.block = block; s.skel = skel; s.size = size; s.off = used;
		memcpy(s.cmd, block + 0x20, 8);
		s.counter = *(uint16_t *)block;
		memcpy(ff8_bgate_gfc_model_buf + used, skel, size);
		used += size;

		// the model advances next tick only if the step function will take its reading branch
		uint16_t flags = *(uint16_t *)(block + 2);
		if (frozen || (flags & 3) || s.counter == 0 || ff8_bgate_fx_mode < 2) continue;
		if (!ff8_bgate_gfc_step_model_guarded(ff8_bgate_gfc_modules[m].step_model, block))
		{
			ff8_bgate_gfc_stats.model_faults++;
			memcpy(skel, ff8_bgate_gfc_model_buf + s.off, size);
			memcpy(block + 0x20, s.cmd, 8);
			*(uint16_t *)block = s.counter;
			continue;
		}
		// next pose is in the skeleton now; current pose in the snapshot
		const uint8_t *cur = ff8_bgate_gfc_model_buf + s.off;
		int nb = skel[0];
		int16_t root[3], rot[64][3], scl[64][3];
		if (nb > 64) nb = 64;
		for (int k = 0; k < 3; k++) root[k] = (int16_t)(*(int16_t *)(cur + 8 + 2 * k) + (*(int16_t *)(skel + 8 + 2 * k) - *(int16_t *)(cur + 8 + 2 * k)) / 2);
		for (int j = 0; j < nb; j++)
			for (int k = 0; k < 3; k++)
			{
				rot[j][k] = ff8_bgate_gfc_mid_angle(*(int16_t *)(cur + 16 + 48 * j + 4 + 2 * k), *(int16_t *)(skel + 16 + 48 * j + 4 + 2 * k));
				int16_t sc = *(int16_t *)(cur + 16 + 48 * j + 10 + 2 * k), sn = *(int16_t *)(skel + 16 + 48 * j + 10 + 2 * k);
				scl[j][k] = (int16_t)(sc + (sn - sc) / 2);
			}
		bool scale = cur[1] != 0;
		// back to the current state, then the midpoint pose on top of it
		memcpy(skel, cur, size);
		memcpy(block + 0x20, s.cmd, 8);
		*(uint16_t *)block = s.counter;
		for (int k = 0; k < 3; k++) *(int16_t *)(skel + 8 + 2 * k) = root[k];
		for (int j = 0; j < nb; j++)
			for (int k = 0; k < 3; k++)
			{
				*(int16_t *)(skel + 16 + 48 * j + 4 + 2 * k) = rot[j][k];
				if (scale) *(int16_t *)(skel + 16 + 48 * j + 10 + 2 * k) = scl[j][k];
			}
		ff8_bgate_gfc_stats.models_stepped++;
	}
}

static void ff8_bgate_gfc_models_restore()
{
	for (int i = 0; i < ff8_bgate_gfc_models_n; i++)
	{
		ff8_bgate_gfc_model_save &s = ff8_bgate_gfc_models[i];
		memcpy(s.skel, ff8_bgate_gfc_model_buf + s.off, s.size);
		memcpy(s.block + 0x20, s.cmd, 8);
		*(uint16_t *)s.block = s.counter;
	}
	ff8_bgate_gfc_models_n = 0;
}

static int ff8_bgate_gfc_module_of_effect()
{
	int id = *(int *)0x1D99A68 + 1; // MAGIC_EFFECT_INDEX + 1
	for (int m = 0; m < FF8_BGATE_GFC_MODULES; m++)
		if (ff8_bgate_gfc_modules[m].effect_id == id) return m;
	return -1;
}

static void ff8_bgate_gfc_reset()
{
	ff8_bgate_gfc_tick = 0;
	ff8_bgate_gfc_ctx = nullptr;
	ff8_bgate_gfc_arena_base = 0;
	ff8_bgate_gfc_log_n = 0;
	ff8_bgate_gfc_faults = 0;
	ff8_bgate_gfc_redraw = ff8_bgate_gfc_logging = false;
}

template <int M, int J> static int __cdecl ff8_bgate_gfc_op_wrap()
{
	if (ff8_bgate_gfc_logging && !ff8_bgate_gfc_redraw)
	{
		if (ff8_bgate_gfc_log_n < FF8_BGATE_GFC_LOG_MAX)
		{
			ff8_bgate_gfc_logent &e = ff8_bgate_gfc_log[ff8_bgate_gfc_log_n++];
			e.fn = ff8_bgate_gfc_orig_op[M][J];
			e.bone = FF8_BGATE_GFC_CURBONE;
			e.cursor = FF8_BGATE_GFC_CURSOR;
			e.opword = *(uint16_t *)(FF8_BGATE_GFC_RT + 0x4A); // rt->curOpcode
			e.op_idx = (uint8_t)J;
		}
		else ff8_bgate_gfc_log_overflow = true;
	}
	return ((int (__cdecl *)())ff8_bgate_gfc_orig_op[M][J])();
}

static int ff8_bgate_gfc_bone_count()
{
	int n = 0;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_BONEORDER[i] != 0xFF; i++)
		if ((FF8_BGATE_GFC_BONEORDER[i] & 0x7F) + 1 > n) n = (FF8_BGATE_GFC_BONEORDER[i] & 0x7F) + 1;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF; i++)
		if ((FF8_BGATE_GFC_DRAWORDER[i] & 0x7F) + 1 > n) n = (FF8_BGATE_GFC_DRAWORDER[i] & 0x7F) + 1;
	return n > FF8_BGATE_GFC_MAX_BONES ? FF8_BGATE_GFC_MAX_BONES : n;
}

// Half of the integrator step the next tick will take (semi-implicit Euler, as the engine does it):
// vel' = vel + acc << 12 (when enabled), accum += vel' >> 1; then the outputs exactly as the
// integrator derives them (outAngle = HIWORD(accumRot), then the bone handler for outPos).
static void ff8_bgate_gfc_step_bones(uint8_t *bones, int m, bool frozen)
{
	uint32_t *handlers = (uint32_t *)ff8_bgate_gfc_modules[m].bone_table;
	uint8_t *rt = FF8_BGATE_GFC_RT;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_BONEORDER[i] != 0xFF; i++)
	{
		uint8_t e = FF8_BGATE_GFC_BONEORDER[i];
		if (frozen && !(e & 0x80)) continue; // same freeze test as the integrator
		int id = e & 0x7F;
		if (id >= FF8_BGATE_GFC_MAX_BONES) continue;
		uint8_t *b = bones + id * 0x100;
		uint8_t flags = b[0x1A];
		int32_t *accRot = (int32_t *)(b + 0x50), *accPos = (int32_t *)(b + 0x5C);
		int32_t *velRot = (int32_t *)(b + 0x68), *velPos = (int32_t *)(b + 0x74);
		int16_t *aRot = (int16_t *)(b + 0x80), *aPos = (int16_t *)(b + 0x86);
		for (int k = 0; k < 3; k++)
		{
			int32_t vr = velRot[k] + ((flags & 1) ? ((int32_t)aRot[k] << 12) : 0);
			int32_t vp = velPos[k] + ((flags & 8) ? ((int32_t)aPos[k] << 12) : 0);
			accRot[k] += vr >> 1;
			accPos[k] += vp >> 1;
		}
		for (int k = 0; k < 3; k++) *(int16_t *)(b + 0x8C + 2 * k) = (int16_t)(accRot[k] >> 16);
		uint32_t h = b[0x18];
		if (h < 12 && handlers[h])
		{
			rt[0x42] = (uint8_t)id;
			FF8_BGATE_GFC_CURBONE = b;
			((void (__cdecl *)())handlers[h])();
		}
		ff8_bgate_gfc_stats.bones_stepped++;
	}
}

// Particle pools (draw handler 6): live particles advanced by half of the handler's own step
// (vel += acc; pos += 16*vel; rgb += drgb) - display only, the pools are restored after the draw.
static uint32_t ff8_bgate_gfc_pools_save(uint8_t *bones, int n, bool restore)
{
	uint32_t used = 0;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF; i++)
	{
		int id = FF8_BGATE_GFC_DRAWORDER[i] & 0x7F;
		if (id >= n) continue;
		uint8_t *b = bones + id * 0x100;
		if (b[0x1C] != 6) continue;
		uint8_t *pool = *(uint8_t **)(b + 0xB8);
		if (!pool) continue;
		uint32_t cnt = *(uint16_t *)pool;
		uint32_t bytes = 16 + 80 * cnt;
		if (cnt == 0 || cnt > 2048 || used + bytes > FF8_BGATE_GFC_POOL_MAX) continue;
		if (restore) memcpy(pool, ff8_bgate_gfc_save_pools + used, bytes);
		else
		{
			memcpy(ff8_bgate_gfc_save_pools + used, pool, bytes);
			if (ff8_bgate_fx_mode >= 2)
				for (uint32_t p = 0; p < cnt; p++)
				{
					uint8_t *q = pool + 16 + 80 * p;
					if (!*(uint32_t *)(q + 4) || !*(uint32_t *)(q + 0x30)) continue; // inactive / invisible
					int16_t *vel = (int16_t *)(q + 0x20), *acc = (int16_t *)(q + 0x28);
					int32_t *ch = (int32_t *)(q + 0x10);
					for (int c = 0; c < 4; c++)
					{
						int32_t v = (int32_t)vel[c] + acc[c];
						ch[c] += 8 * v;
					}
					for (int c = 0; c < 3; c++)
					{
						int v = q[0x38 + c] + ((int8_t)q[0x3C + c]) / 2;
						q[0x38 + c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
					}
					ff8_bgate_gfc_stats.particles_stepped++;
				}
		}
		used += bytes;
	}
	return used;
}

static uint32_t ff8_bgate_gfc_fault_addr = 0, ff8_bgate_gfc_fault_code = 0;

// TEMP diagnostics: copies of the mesh headers (16 dwords) of every mesh bone, compared after
// each phase of the held frame to find who rewrites a mesh
#define FF8_BGATE_GFC_WATCH_MAX 96
static struct { int id; uint32_t *mesh; uint32_t hdr[16]; } ff8_bgate_gfc_watch[FF8_BGATE_GFC_WATCH_MAX];
static int ff8_bgate_gfc_watch_n = 0;

static void ff8_bgate_gfc_watch_take(uint8_t *bones, int n)
{
	ff8_bgate_gfc_watch_n = 0;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF && ff8_bgate_gfc_watch_n < FF8_BGATE_GFC_WATCH_MAX; i++)
	{
		int id = FF8_BGATE_GFC_DRAWORDER[i] & 0x7F;
		if (id >= n) continue;
		uint8_t *b = bones + id * 0x100;
		int h = b[0x1C];
		if (h != 1 && h != 2 && h != 7 && h != 9 && h != 27) continue;
		uint32_t *mesh = *(uint32_t **)(b + 0xD8);
		if (!mesh || IsBadReadPtr(mesh, 64)) continue;
		auto &w = ff8_bgate_gfc_watch[ff8_bgate_gfc_watch_n++];
		w.id = id; w.mesh = mesh;
		memcpy(w.hdr, mesh, 64);
	}
}

static void ff8_bgate_gfc_watch_check(const char *phase, int m)
{
	for (int i = 0; i < ff8_bgate_gfc_watch_n; i++)
	{
		auto &w = ff8_bgate_gfc_watch[i];
		if (IsBadReadPtr(w.mesh, 64) || memcmp(w.hdr, w.mesh, 64) == 0) continue;
		char l[400]; int o = 0;
		for (int k = 0; k < 16 && o < 360; k++)
			if (w.hdr[k] != w.mesh[k]) o += sprintf(l + o, " [%d]%08X->%08X", k, w.hdr[k], w.mesh[k]);
		l[o] = 0;
		uint8_t *cx = FF8_BGATE_GFC_CTX;
		ffnx_info("30fps gf: %s MESH HEADER CHANGED after %s (tick %u): bone %d mesh=%p%s | ctx+74=%08X +7C=%08X +D4=%08X +D8=%08X frame_pkt=%08X\n", ff8_bgate_gfc_modules[m].name, phase, ff8_bgate_gfc_tick, w.id, w.mesh, l,
			*(uint32_t *)(cx + 0x74), *(uint32_t *)(cx + 0x7C), *(uint32_t *)(cx + 0xD4), *(uint32_t *)(cx + 0xD8), *(uint32_t *)0x1D8E054);
		memcpy(w.hdr, w.mesh, 64);
	}
}


static int ff8_bgate_gfc_fault_filter(EXCEPTION_POINTERS *ep)
{
	ff8_bgate_gfc_fault_code = ep->ExceptionRecord->ExceptionCode;
	ff8_bgate_gfc_fault_addr = (uint32_t)ep->ExceptionRecord->ExceptionAddress;
	return EXCEPTION_EXECUTE_HANDLER;
}

// the state-changing part of a held frame (midpoint, replay, draw) - a fault here must not take the
// game down: everything is restored by the caller and the frame is simply incomplete
static bool ff8_bgate_gfc_held_body(int m, uint8_t *bones, int n, bool frozen, uint8_t *rt, int (__cdecl *orig)())
{
	__try
	{
		ff8_bgate_gfc_watch_take(bones, n);
		if (ff8_bgate_fx_mode >= 2) ff8_bgate_gfc_step_bones(bones, m, frozen);
		ff8_bgate_gfc_watch_check("bone half-step", m);
		ff8_bgate_gfc_pools_save(bones, n, false);
		ff8_bgate_gfc_watch_check("particle half-step", m);
		ff8_bgate_gfc_models_snapshot(bones, n, m, frozen);
		ff8_bgate_gfc_watch_check("model look-ahead", m);
		// what AdvanceAnimChannels_Neg does before the first opcode of a tick: the replayed
		// opcodes (0x71 links shadow packets through it) must not see a stale cursor
		*(uint32_t *)(rt + 0x4C) = *(uint32_t *)(rt + 0x38);
		*(uint16_t *)(rt + 0x3E) = 0;
		if (ff8_bgate_fx_mode >= 1 && !ff8_bgate_gfc_log_overflow)
			for (int i = 0; i < ff8_bgate_gfc_log_n; i++)
			{
				const ff8_bgate_gfc_logent &e = ff8_bgate_gfc_log[i];
				if (ff8_bgate_gfc_ops[e.op_idx] == 0x39 && (e.opword >> 12) != 0) continue; // only the camera sub-op is pure
				if (ff8_bgate_gfc_ops[e.op_idx] == 0x71) continue; // TEMP experiment: entity shadow not replayed (Bahamut corruption)
				FF8_BGATE_GFC_CURBONE = e.bone;
				FF8_BGATE_GFC_CURSOR = e.cursor;
				*(uint16_t *)(rt + 0x4A) = e.opword;
				((int (__cdecl *)())e.fn)();
				ff8_bgate_gfc_stats.replayed_ops++;
			}
		ff8_bgate_gfc_watch_check("pure-op replay", m);
		rt[0x45] = 0xFF; // boneSkipFlag: draw handlers 3/5/6/37... draw without advancing
		unreplace_function(ff8_bgate_gfc_build_ri[m]);
		orig();
		rereplace_function(ff8_bgate_gfc_build_ri[m]);
		ff8_bgate_gfc_watch_check("held draw", m);
		return true;
	}
	__except (ff8_bgate_gfc_fault_filter(GetExceptionInformation()))
	{
		rereplace_function(ff8_bgate_gfc_build_ri[m]);
		return false;
	}
}

static void ff8_bgate_gfc_fault_report(int m, uint8_t *bones, int n, uint8_t *rt)
{
	int id = rt[0x42];
	uint8_t *b = bones + (id < n ? id : 0) * 0x100;
	char ops[200]; int o = 0;
	for (int i = 0; i < ff8_bgate_gfc_log_n && o < 180; i++)
		if (ff8_bgate_gfc_log[i].bone == b) o += sprintf(ops + o, " %X", ff8_bgate_gfc_ops[ff8_bgate_gfc_log[i].op_idx]);
	ops[o] = 0;
	char dl[300]; int d = 0;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF && d < 280; i++)
	{
		int bid = FF8_BGATE_GFC_DRAWORDER[i] & 0x7F;
		d += sprintf(dl + d, " %d:h%d", bid, bid < n ? bones[bid * 0x100 + 0x1C] : -1);
	}
	dl[d] = 0;
	ffnx_info("30fps gf: %s HELD-FRAME FAULT %08X at %08X (tick %u, fault %u): cur bone %d id=%u draw=%d bonehdl=%d parent=%u +B8=%08X +BC=%08X +D8=%08X flags4A=%04X | ops replayed for it:%s | draw list:%s\n",
		ff8_bgate_gfc_modules[m].name, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr, ff8_bgate_gfc_tick, ff8_bgate_gfc_faults, id,
		*(uint16_t *)(b + 0x12), b[0x1C], b[0x18], *(uint16_t *)(b + 0x9C), *(uint32_t *)(b + 0xB8), *(uint32_t *)(b + 0xBC), *(uint32_t *)(b + 0xD8), *(uint16_t *)(b + 0x4A), ops, dl);
}

static uint32_t ff8_bgate_gfc_last_arena_len = 0;

static bool ff8_bgate_gfc_real_body(int m, int (__cdecl *orig)(), int *r)
{
	__try
	{
		unreplace_function(ff8_bgate_gfc_build_ri[m]);
		*r = orig();
		rereplace_function(ff8_bgate_gfc_build_ri[m]);
		return true;
	}
	__except (ff8_bgate_gfc_fault_filter(GetExceptionInformation()))
	{
		rereplace_function(ff8_bgate_gfc_build_ri[m]);
		return false;
	}
}

// TEMP diagnostics: before a real tick draws, check every mesh bone's mesh pointer
static void ff8_bgate_gfc_validate(int m, uint8_t *bones, int n, uint8_t *ctx)
{
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF; i++)
	{
		int id = FF8_BGATE_GFC_DRAWORDER[i] & 0x7F;
		if (id >= n) continue;
		uint8_t *b = bones + id * 0x100;
		int h = b[0x1C];
		if (h != 1 && h != 2 && h != 7 && h != 9 && h != 27) continue;
		uint32_t *mesh = *(uint32_t **)(b + 0xD8);
		bool bad = !mesh || IsBadReadPtr(mesh, 0x24);
		uint32_t vofs = 0, vcnt = 0;
		if (!bad)
		{
			vofs = mesh[5]; vcnt = mesh[6];
			bad = vcnt > 4096 || IsBadReadPtr((uint8_t *)mesh + vofs, 8 * (vcnt ? vcnt : 1));
		}
		if (bad)
		{
			ffnx_info("30fps gf: %s BAD MESH before real tick %u: bone %d id=%u draw=%d parent=%u mesh=%p vofs=%u vcnt=%u +B8=%08X +BC=%08X flags4A=%04X drawFlags=%02X | ctx+74=%08X arena_base=%08X last_len=%u seq=%u\n",
				ff8_bgate_gfc_modules[m].name, ff8_bgate_gfc_tick, id, *(uint16_t *)(b + 0x12), h, *(uint16_t *)(b + 0x9C), mesh, vofs, vcnt,
				*(uint32_t *)(b + 0xB8), *(uint32_t *)(b + 0xBC), *(uint16_t *)(b + 0x4A), b[0x4C],
				*(uint32_t *)(ctx + 0x74), ff8_bgate_gfc_arena_base, ff8_bgate_gfc_last_arena_len, *(uint16_t *)(ctx + 0x32));
		}
	}
}

static int ff8_bgate_gfc_build_draw(int m)
{
	int (__cdecl *orig)() = (int (__cdecl *)())ff8_bgate_gfc_modules[m].build_draw;
	uint8_t *ctx = FF8_BGATE_GFC_CTX, *rt = FF8_BGATE_GFC_RT;
	uint8_t *bones = *(uint8_t **)(ctx + 0x90);
	int n = ff8_bgate_gfc_bone_count();
	int r;

	if (!ff8_bgate_gfc_redraw)
	{
		if (ff8_bgate_gfc_ctx != ctx || ff8_bgate_gfc_arena_base == 0)
		{
			// first draw of this summon: nothing lazy has been allocated from the bump arena yet
			ff8_bgate_gfc_reset();
			ff8_bgate_gfc_ctx = ctx;
			ff8_bgate_gfc_arena_base = *(uint32_t *)(ctx + 0x74);
			ffnx_info("30fps gf: %s cinematic summon, ctx=%p bones=%d arena=%08X\n", ff8_bgate_gfc_modules[m].name, ctx, n, ff8_bgate_gfc_arena_base);
		}
		ff8_bgate_gfc_watch_check("the real tick's VM", m);
		ff8_bgate_gfc_validate(m, bones, n, ctx);
		if (!ff8_bgate_gfc_real_body(m, orig, &r))
		{
			ff8_bgate_gfc_faults = 99; // this summon is off the dedicated path from now on
			ffnx_info("30fps gf: %s REAL-TICK FAULT after a held frame: see the bone below\n", ff8_bgate_gfc_modules[m].name);
			ff8_bgate_gfc_fault_report(m, bones, n, rt);
			r = 0;
		}
		ff8_bgate_gfc_tick++;
		for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF; i++)
		{
			int id = bones[(FF8_BGATE_GFC_DRAWORDER[i] & 0x7F) * 0x100 + 0x1C];
			if (id < 73) ff8_bgate_gfc_stats.handlers[id] = 1;
		}
		return r;
	}

	// ---- held frame: second draw, at the exact midpoint of the coming tick ----
	if (!ff8_bgate_gfc_save_bones) ff8_bgate_gfc_save_bones = (uint8_t *)malloc(FF8_BGATE_GFC_MAX_BONES * 0x100);
	if (!ff8_bgate_gfc_save_arena) ff8_bgate_gfc_save_arena = (uint8_t *)malloc(FF8_BGATE_GFC_ARENA_MAX);
	if (!ff8_bgate_gfc_save_pools) ff8_bgate_gfc_save_pools = (uint8_t *)malloc(FF8_BGATE_GFC_POOL_MAX);
	uint32_t arena_top = *(uint32_t *)(ctx + 0x74);
	uint32_t arena_len = (arena_top > ff8_bgate_gfc_arena_base && arena_top - ff8_bgate_gfc_arena_base <= FF8_BGATE_GFC_ARENA_MAX) ? arena_top - ff8_bgate_gfc_arena_base : 0;
	uint8_t rt_save[0x50];
	memcpy(rt_save, rt, sizeof(rt_save));
	uint8_t *curbone_save = FF8_BGATE_GFC_CURBONE;
	uint32_t cursor_save = FF8_BGATE_GFC_CURSOR;
	bool frozen = rt[0x45] != 0;
	memcpy(ff8_bgate_gfc_save_bones, bones, n * 0x100);
	memcpy(ff8_bgate_gfc_save_nodes, (void *)FF8_BGATE_GFC_NODES_BEGIN, sizeof(ff8_bgate_gfc_save_nodes));
	if (arena_len) memcpy(ff8_bgate_gfc_save_arena, (void *)ff8_bgate_gfc_arena_base, arena_len);
	ff8_bgate_gfc_stats.arena_bytes = arena_len;
	ff8_bgate_gfc_last_arena_len = arena_len;
	// node 0 (the camera) has just been refreshed by SequenceTick's preamble from the smoothed view

	// F9 sub-modes: 0 = camera only, 1 = + node/light/camera replay, >= 2 = + midpoint state
	r = 0;
	uint32_t pool_save = *(uint32_t *)0x1D999C4;
	if (!ff8_bgate_gfc_held_body(m, bones, n, frozen, rt, orig))
	{
		ff8_bgate_gfc_faults++;
		ff8_bgate_gfc_fault_report(m, bones, n, rt);
	}
	*(uint32_t *)0x1D999C4 = pool_save;

	ff8_bgate_gfc_models_restore();
	ff8_bgate_gfc_pools_save(bones, n, true);
	if (arena_len) memcpy((void *)ff8_bgate_gfc_arena_base, ff8_bgate_gfc_save_arena, arena_len);
	memcpy((void *)FF8_BGATE_GFC_NODES_BEGIN, ff8_bgate_gfc_save_nodes, sizeof(ff8_bgate_gfc_save_nodes));
	memcpy(bones, ff8_bgate_gfc_save_bones, n * 0x100);
	FF8_BGATE_GFC_CURSOR = cursor_save;
	FF8_BGATE_GFC_CURBONE = curbone_save;
	memcpy(rt, rt_save, sizeof(rt_save));
	ff8_bgate_gfc_watch_check("restores", m);
	if (ff8_bgate_gfc_tick >= 505 && ff8_bgate_gfc_tick <= 517)
		ffnx_info("30fps gf: held end tick %u: ctx+74=%08X +7C=%08X +D4=%08X +D8=%08X frame_pkt=%08X rt+38=%08X rt+4C=%08X\n", ff8_bgate_gfc_tick,
			*(uint32_t *)(ctx + 0x74), *(uint32_t *)(ctx + 0x7C), *(uint32_t *)(ctx + 0xD4), *(uint32_t *)(ctx + 0xD8), *(uint32_t *)0x1D8E054, *(uint32_t *)(rt + 0x38), *(uint32_t *)(rt + 0x4C));
	ff8_bgate_gfc_stats.redraws++;
	return r;
}

template <int M> static int __cdecl ff8_bgate_gfc_build_hook() { return ff8_bgate_gfc_build_draw(M); }

// held frame of the effect-tree gate: true = the summon drew itself, no 2D replay wanted
static bool ff8_bgate_gfc_held_frame(void *queue, int (__cdecl *orig)(void *))
{
	if (!ff8_bgate_gfc_enabled || ff8_bgate_gfc_module_of_effect() < 0) return false;
	if (!ff8_bgate_gfc_ctx || ff8_bgate_gfc_tick < 1 || ff8_bgate_gfc_faults >= 3) return false;
	uint32_t *flags = (uint32_t *)0x1D96A9C; // battle_to_update_flags: bit 0 = effects paused
	uint32_t flags_save = *flags;
	uint16_t seq_save = *(uint16_t *)(ff8_bgate_gfc_ctx + 0x32);
	uint32_t *seed = (uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14); // _getptd()->_holdrand
	uint32_t seed_save = *seed;
	*flags |= 1;
	ff8_bgate_gfc_redraw = true;
	orig(queue);
	ff8_bgate_gfc_redraw = false;
	*flags = (*flags & ~1u) | (flags_save & 1);
	*seed = seed_save;
	*(uint16_t *)(ff8_bgate_gfc_ctx + 0x32) = seq_save;
	return true;
}

template <int M> static void ff8_bgate_gfc_install_module()
{
	static int (__cdecl *const wraps[FF8_BGATE_GFC_OPS])() = {
		&ff8_bgate_gfc_op_wrap<M, 0>, &ff8_bgate_gfc_op_wrap<M, 1>, &ff8_bgate_gfc_op_wrap<M, 2>, &ff8_bgate_gfc_op_wrap<M, 3>, &ff8_bgate_gfc_op_wrap<M, 4>,
		&ff8_bgate_gfc_op_wrap<M, 5>, &ff8_bgate_gfc_op_wrap<M, 6>, &ff8_bgate_gfc_op_wrap<M, 7>, &ff8_bgate_gfc_op_wrap<M, 8>, &ff8_bgate_gfc_op_wrap<M, 9>,
		&ff8_bgate_gfc_op_wrap<M, 10>, &ff8_bgate_gfc_op_wrap<M, 11>, &ff8_bgate_gfc_op_wrap<M, 12>, &ff8_bgate_gfc_op_wrap<M, 13>, &ff8_bgate_gfc_op_wrap<M, 14>,
		&ff8_bgate_gfc_op_wrap<M, 15>, &ff8_bgate_gfc_op_wrap<M, 16>, &ff8_bgate_gfc_op_wrap<M, 17>, &ff8_bgate_gfc_op_wrap<M, 18>, &ff8_bgate_gfc_op_wrap<M, 19> };
	const ff8_bgate_gfc_module &g = ff8_bgate_gfc_modules[M];
	for (int j = 0; j < FF8_BGATE_GFC_OPS; j++)
	{
		uint32_t slot = FF8_BGATE_GFC_VM_TABLE(g.draw_table) + 4 * ff8_bgate_gfc_ops[j];
		ff8_bgate_gfc_orig_op[M][j] = *(uint32_t *)slot;
		if (ff8_bgate_gfc_orig_op[M][j]) patch_code_dword(slot, (DWORD)wraps[j]);
	}
	ff8_bgate_gfc_build_ri[M] = replace_function(g.build_draw, (void *)&ff8_bgate_gfc_build_hook<M>);
}

static void ff8_bgate_gfc_install()
{
	ff8_bgate_gfc_install_module<0>(); ff8_bgate_gfc_install_module<1>(); ff8_bgate_gfc_install_module<2>();
	ff8_bgate_gfc_install_module<3>(); ff8_bgate_gfc_install_module<4>(); ff8_bgate_gfc_install_module<5>();
	ff8_bgate_gfc_install_module<6>();
}

// --- dedicated held-frame path for the "timeline" GF summons (family B) ---
// Shiva 185, Cactuar 199, Odin 187 / 326, Doomtrain 191, Gilgamesh 327-330 (study:
// gf_study/gf_inventory_timeline.md). Their effect queue holds a tiny master task (runs a
// sub-queue, ++counter at node+12) and the sub-queue holds the timeline, the creature and the
// particle tasks. Every one of those tasks tests battle_to_update_flags & 0x201: it DRAWS from
// its current state, then returns before its events, spawns, rand(), sound and state advance
// when the bit is set. So a held frame = the effect queue called once more with bit 0 set:
// the whole summon is redrawn (with the smoothed battle camera) and nothing advances, except
// the master counters (restored) and the CRT seed (restored).
// The creature is a standard battle entity (BattleAnimHeader at E+96, BattleAnimCmd at E+108,
// keyframes read by Battle_ReadAnimation): its pose is half-stepped by look-ahead, exactly as
// the cinematic engine's embedded models.
struct ff8_bgate_tlb_module { int effect_id; uint32_t creature, advance_fn; const char *name; };
static const ff8_bgate_tlb_module ff8_bgate_tlb_modules[] = {
	{ 185, 0x22BD018, 0x5C4EA0, "Shiva" },
	{ 199, 0x225A838, 0x5A8FD0, "Cactuar" },
	{ 187, 0x24FD8D8, 0x64A260, "Odin" },
	{ 326, 0, 0, "Odin (Zantetsuken Reverse)" },
	{ 191, 0, 0, "Doomtrain" },
	{ 327, 0, 0, "Gilgamesh" }, { 328, 0, 0, "Gilgamesh" }, { 329, 0, 0, "Gilgamesh" }, { 330, 0, 0, "Gilgamesh" },
};
#define FF8_BGATE_TLB_MODULES 9
#define FF8_BGATE_TLB_SKEL_MAX (64 * 1024)

static int ff8_bgate_tlb_cur = -1;        // running summon (-1 = none)
static void *ff8_bgate_tlb_queue = nullptr;
static uint32_t ff8_bgate_tlb_tick = 0, ff8_bgate_tlb_redraws = 0, ff8_bgate_tlb_model_steps = 0, ff8_bgate_tlb_faults = 0;
static uint8_t *ff8_bgate_tlb_skel_save = nullptr;
static uint8_t *ff8_bgate_tlb_skel = nullptr; // skeleton section being held (null = none)
static uint32_t ff8_bgate_tlb_skel_size = 0;
static uint8_t ff8_bgate_tlb_cmd_save[8];

static int ff8_bgate_tlb_module_of_effect()
{
	int id = *(int *)0x1D99A68 + 1;
	for (int m = 0; m < FF8_BGATE_TLB_MODULES; m++)
		if (ff8_bgate_tlb_modules[m].effect_id == id) return m;
	return -1;
}

static void ff8_bgate_tlb_reset()
{
	ff8_bgate_tlb_cur = -1;
	ff8_bgate_tlb_queue = nullptr;
	ff8_bgate_tlb_tick = ff8_bgate_tlb_redraws = ff8_bgate_tlb_model_steps = ff8_bgate_tlb_faults = 0;
	ff8_bgate_tlb_skel = nullptr;
}

static bool ff8_bgate_tlb_call_guarded(uint32_t fn, void *arg)
{
	__try
	{
		((void (__cdecl *)(void *))fn)(arg);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

// Field_Alloc/Field_Free are a bump stack (GLOBAL_MEMORY_POOL 0x1D999C4); a fault inside game code
// leaves it pushed, and every later Field_Alloc then runs past the region: heap corruption. The
// pool pointer is restored after every held pass, fault or not.
static bool ff8_bgate_tlb_queue_guarded(int (__cdecl *orig)(void *), void *queue)
{
	uint32_t pool = *(uint32_t *)0x1D999C4;
	bool ok;
	__try
	{
		orig(queue);
		ok = true;
	}
	__except (ff8_bgate_gfc_fault_filter(GetExceptionInformation()))
	{
		ok = false;
	}
	*(uint32_t *)0x1D999C4 = pool;
	return ok;
}

// pose(skel) := midpoint of cur (snapshot) and skel (next), root + rotations + scales
static void ff8_bgate_pose_write_mid(uint8_t *skel, const uint8_t *cur)
{
	int nb = skel[0];
	bool scale = cur[1] != 0;
	for (int k = 0; k < 3; k++)
	{
		int16_t c = *(int16_t *)(cur + 8 + 2 * k), nx = *(int16_t *)(skel + 8 + 2 * k);
		*(int16_t *)(skel + 8 + 2 * k) = (int16_t)(c + (nx - c) / 2);
	}
	for (int j = 0; j < nb; j++)
		for (int k = 0; k < 3; k++)
		{
			uint8_t *rc = skel + 16 + 48 * j + 4 + 2 * k;
			*(int16_t *)rc = ff8_bgate_gfc_mid_angle(*(int16_t *)(cur + 16 + 48 * j + 4 + 2 * k), *(int16_t *)rc);
			if (scale)
			{
				uint8_t *sc = skel + 16 + 48 * j + 10 + 2 * k;
				int16_t c = *(int16_t *)(cur + 16 + 48 * j + 10 + 2 * k);
				*(int16_t *)sc = (int16_t)(c + (*(int16_t *)sc - c) / 2);
			}
		}
	// everything else of the record (matrices) is rebuilt by the draw from this pose
	for (int j = 0; j < nb; j++)
		memcpy(skel + 16 + 48 * j + 16, cur + 16 + 48 * j + 16, 32);
}

// creature half-step: snapshot, read the next keyframe as the next tick will, midpoint, restore later
static uint8_t *ff8_bgate_tlb_E = nullptr; // creature block being held

static void ff8_bgate_tlb_creature_halfstep(uint32_t creature, uint32_t advance_fn)
{
	ff8_bgate_tlb_skel = nullptr;
	if (!creature || !advance_fn || ff8_bgate_fx_mode < 2) return;
	uint8_t *E = (uint8_t *)creature;
	ff8_bgate_tlb_E = E;
	uint8_t *cmd = E + 108;
	static uint8_t *logged_for = nullptr;
	#define FF8_BGATE_TLB_SKIP(why) do { if (logged_for != E) { logged_for = E; ffnx_info("30fps tl: creature half-step skipped: %s (E=%p cmd=%02X %02X .. %02X %02X com=%p)\n", why, E, cmd[0], cmd[1], cmd[6], cmd[7], *(void **)(E + 100)); } return; } while (0)
	if (cmd[7] == 0) FF8_BGATE_TLB_SKIP("no animation set");
	uint8_t *com = *(uint8_t **)(E + 100);
	if (!com || IsBadReadPtr(com, 4)) FF8_BGATE_TLB_SKIP("bad model data pointer");
	uint8_t *skel = *(uint8_t **)com;
	if (!skel || IsBadReadPtr(skel, 16)) FF8_BGATE_TLB_SKIP("bad skeleton pointer");
	uint32_t size = 16 + 48 * skel[0];
	if (size > FF8_BGATE_TLB_SKEL_MAX || IsBadReadPtr(skel, size)) FF8_BGATE_TLB_SKIP("skeleton size");
	#undef FF8_BGATE_TLB_SKIP
	logged_for = nullptr;
	if (!ff8_bgate_tlb_skel_save) ff8_bgate_tlb_skel_save = (uint8_t *)malloc(FF8_BGATE_TLB_SKEL_MAX);
	if (!ff8_bgate_tlb_skel_save) return;
	memcpy(ff8_bgate_tlb_skel_save, skel, size);
	memcpy(ff8_bgate_tlb_cmd_save, cmd, 8);
	ff8_bgate_tlb_skel = skel;
	ff8_bgate_tlb_skel_size = size;
	if (!ff8_bgate_tlb_call_guarded(advance_fn, E))
	{
		ff8_bgate_tlb_faults++;
		memcpy(skel, ff8_bgate_tlb_skel_save, size);
		memcpy(cmd, ff8_bgate_tlb_cmd_save, 8);
		return;
	}
	// next pose is in the skeleton, current in the snapshot; the cmd goes back to current now
	memcpy(cmd, ff8_bgate_tlb_cmd_save, 8);
	ff8_bgate_pose_write_mid(skel, ff8_bgate_tlb_skel_save);
	ff8_bgate_tlb_model_steps++;
}

static void ff8_bgate_tlb_creature_restore()
{
	if (!ff8_bgate_tlb_skel) return;
	memcpy(ff8_bgate_tlb_skel, ff8_bgate_tlb_skel_save, ff8_bgate_tlb_skel_size);
	memcpy(ff8_bgate_tlb_E + 108, ff8_bgate_tlb_cmd_save, 8);
	ff8_bgate_tlb_skel = nullptr;
}

// real tick of the effect queue: track the running summon
static void ff8_bgate_tlb_real_tick(void *queue, int r)
{
	int m = ff8_bgate_tlb_module_of_effect();
	if (m < 0) return;
	if (ff8_bgate_tlb_cur != m || ff8_bgate_tlb_queue != queue)
	{
		ff8_bgate_tlb_reset();
		ff8_bgate_tlb_cur = m;
		ff8_bgate_tlb_queue = queue;
		ffnx_info("30fps tl: %s timeline summon, queue=%p\n", ff8_bgate_tlb_modules[m].name, queue);
	}
	ff8_bgate_tlb_tick++;
	if (r == 0)
	{
		ffnx_info("30fps tl: %s finished, real ticks=%u held redraws=%u creature half-steps=%u faults=%u\n",
			ff8_bgate_tlb_modules[m].name, ff8_bgate_tlb_tick, ff8_bgate_tlb_redraws, ff8_bgate_tlb_model_steps, ff8_bgate_tlb_faults);
		ff8_bgate_tlb_reset();
	}
}

// held frame: true = the summon drew itself, no 2D replay wanted
static bool ff8_bgate_tlb_held_frame(void *queue, int (__cdecl *orig)(void *))
{
	if (!ff8_bgate_gfc_enabled) return false;
	int m = ff8_bgate_tlb_cur;
	if (m < 0 || queue != ff8_bgate_tlb_queue || ff8_bgate_tlb_tick < 1 || ff8_bgate_tlb_faults >= 3) return false;
	const ff8_bgate_tlb_module &g = ff8_bgate_tlb_modules[m];
	uint32_t *flags = (uint32_t *)0x1D96A9C;
	uint32_t flags_save = *flags;
	uint32_t *seed = (uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14); // _getptd()->_holdrand
	uint32_t seed_save = *seed;
	// master task node = head of the effect queue; its counter (node+12) advances even when paused
	uint8_t *master = *(uint8_t **)queue;
	uint16_t counter_save = master ? *(uint16_t *)(master + 12) : 0;

	ff8_bgate_tlb_creature_halfstep(g.creature, g.advance_fn);
	*flags |= 1;
	bool ok = ff8_bgate_tlb_queue_guarded(orig, queue);
	*flags = (*flags & ~1u) | (flags_save & 1);
	*seed = seed_save;
	if (master) *(uint16_t *)(master + 12) = counter_save;
	ff8_bgate_tlb_creature_restore();
	if (!ok)
	{
		ff8_bgate_tlb_faults++;
		ffnx_info("30fps tl: %s HELD-FRAME FAULT %08X at %08X (tick %u, fault %u)\n", g.name, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr, ff8_bgate_tlb_tick, ff8_bgate_tlb_faults);
	}
	ff8_bgate_tlb_redraws++;
	return true;
}

// --- dedicated held-frame path for the "timeline" GF summons (family A) ---
// Quezacotl 116 (precedent), later Diablos 325, Carbuncle 278, Pandemona 291, Phoenix 140.
// Each has a debug PAUSE global the retail game never sets: when non-zero the master keeps its
// counter, does not spawn, the creature's keyframe advance and the relative camera moves are
// skipped and every particle task draws then returns. What the pause does NOT cover is the
// creature task itself: its counter still increments and its frame-keyed one-shots (sounds,
// streamed loads, pool re-inits, task spawns, damage application) fire again for the same
// frame. So a held pass sets the pause global AND a side-effect shield: during the pass those
// engine entry points are no-ops (AddTaskToQueue hands out a scratch node), then the creature
// counter, the master's arena toggle, the CRT seed and the effect's own LCG seed are restored.
// The creature (standard battle entity in the effect's model buffer) gets the look-ahead
// half-step of the timeline-B path.
struct ff8_bgate_tla_module {
	int effect_id; uint32_t pause_global, model_buffer_global; int creature_off, creature_counter_off;
	uint32_t advance_fn, seed_global; int master_toggle_off, master_toggle_size; const char *name;
};
static const ff8_bgate_tla_module ff8_bgate_tla_modules[] = {
	{ 116, 0x25216DC, 0x25217AC, 40, 12, 0x6FBDB0, 0x25217A0, 16, 4, "Quezacotl" },
};
#define FF8_BGATE_TLA_MODULES 1

static int ff8_bgate_tla_cur = -1;
static void *ff8_bgate_tla_queue = nullptr;
static uint32_t ff8_bgate_tla_tick = 0, ff8_bgate_tla_redraws = 0, ff8_bgate_tla_model_steps = 0, ff8_bgate_tla_faults = 0;

// side-effect shield: engine entry points that must not fire twice per tick. They are patched
// only for the duration of a held pass (5-byte jump to a stub, original bytes put back right
// after): no permanent hook, nothing forwarded, nothing left in FFNx's replacement table.
static uint8_t ff8_bgate_shield_node[8192];
static int __cdecl ff8_bgate_shield_zero() { return 0; }
static int __cdecl ff8_bgate_shield_one() { return 1; }
static void *__cdecl ff8_bgate_shield_scratch_node() { memset(ff8_bgate_shield_node, 0, sizeof(ff8_bgate_shield_node)); return ff8_bgate_shield_node; }
struct ff8_bgate_shield_site { uint32_t addr; void *stub; uint8_t saved[5]; };
static ff8_bgate_shield_site ff8_bgate_shield_sites[] = {
	{ 0x501330, (void *)ff8_bgate_shield_zero },         // BdPlaySE
	{ 0x5018C0, (void *)ff8_bgate_shield_zero },         // BdPlaySummonStream
	{ 0x501860, (void *)ff8_bgate_shield_zero },         // BdTransSummonStream
	{ 0x4A29A0, (void *)ff8_bgate_shield_zero },         // BdSound_ClaimVoiceSlot
	{ 0x4A2940, (void *)ff8_bgate_shield_zero },         // voice slot release
	{ 0x5341D0, (void *)ff8_bgate_shield_zero },         // streamed file load request
	{ 0x508480, (void *)ff8_bgate_shield_zero },         // BattleFile_CharacterLoad
	{ 0x508300, (void *)ff8_bgate_shield_zero },         // BdLink_InitTaskQueuePool (would wipe live particles)
	{ 0x508360, (void *)ff8_bgate_shield_scratch_node }, // AddTaskToQueue -> scratch node
	{ 0x506BA0, (void *)ff8_bgate_shield_zero },         // ApplyActionResultToTargets
	{ 0x506690, (void *)ff8_bgate_shield_zero },         // ApplyActionResultToTarget
	{ 0x505C00, (void *)ff8_bgate_shield_zero },         // QueueChainTransformation
	{ 0x534270, (void *)ff8_bgate_shield_one },          // loader poll (idle branch resets the queue): "busy" = the task waits as on a real load frame
	{ 0x534210, (void *)ff8_bgate_shield_zero },         // loader pump (pre_LoadBattleFile of the next queued file)
	{ 0x534110, (void *)ff8_bgate_shield_zero },         // loader state init (dword_1DCD6EC/E4) at creature spawn
};
#define FF8_BGATE_SHIELD_SITES 15
static bool ff8_bgate_shield = false;

static void ff8_bgate_shield_set(bool on)
{
	if (on == ff8_bgate_shield) return;
	ff8_bgate_shield = on;
	for (int i = 0; i < FF8_BGATE_SHIELD_SITES; i++)
	{
		ff8_bgate_shield_site &s = ff8_bgate_shield_sites[i];
		uint8_t *code = (uint8_t *)s.addr;
		DWORD old;
		if (!VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &old)) continue;
		if (on)
		{
			memcpy(s.saved, code, 5);
			code[0] = 0xE9;
			*(int32_t *)(code + 1) = (int32_t)((uint8_t *)s.stub - (code + 5));
		}
		else memcpy(code, s.saved, 5);
		VirtualProtect(code, 5, old, &old);
		FlushInstructionCache(GetCurrentProcess(), code, 5);
	}
}

static int ff8_bgate_tla_module_of_effect()
{
	int id = *(int *)0x1D99A68 + 1;
	for (int m = 0; m < FF8_BGATE_TLA_MODULES; m++)
		if (ff8_bgate_tla_modules[m].effect_id == id) return m;
	return -1;
}

static void ff8_bgate_tla_reset()
{
	ff8_bgate_tla_cur = -1;
	ff8_bgate_tla_queue = nullptr;
	ff8_bgate_tla_tick = ff8_bgate_tla_redraws = ff8_bgate_tla_model_steps = ff8_bgate_tla_faults = 0;
	ff8_bgate_shield_set(false);
}

static void ff8_bgate_tla_real_tick(void *queue, int r)
{
	int m = ff8_bgate_tla_module_of_effect();
	if (m < 0) return;
	if (ff8_bgate_tla_cur != m || ff8_bgate_tla_queue != queue)
	{
		ff8_bgate_tla_reset();
		ff8_bgate_tla_cur = m;
		ff8_bgate_tla_queue = queue;
		ffnx_info("30fps tl: %s timeline-A summon, queue=%p\n", ff8_bgate_tla_modules[m].name, queue);
	}
	ff8_bgate_tla_tick++;
	if (r == 0)
	{
		ffnx_info("30fps tl: %s finished, real ticks=%u held redraws=%u creature half-steps=%u faults=%u\n",
			ff8_bgate_tla_modules[m].name, ff8_bgate_tla_tick, ff8_bgate_tla_redraws, ff8_bgate_tla_model_steps, ff8_bgate_tla_faults);
		ff8_bgate_tla_reset();
	}
}

static bool ff8_bgate_tla_held_frame(void *queue, int (__cdecl *orig)(void *))
{
	if (!ff8_bgate_gfc_enabled) return false;
	int m = ff8_bgate_tla_cur;
	if (m < 0 || queue != ff8_bgate_tla_queue || ff8_bgate_tla_tick < 1 || ff8_bgate_tla_faults >= 3) return false;
	const ff8_bgate_tla_module &g = ff8_bgate_tla_modules[m];
	uint8_t *master = *(uint8_t **)queue;
	if (!master) return false;
	uint8_t *buf = *(uint8_t **)g.model_buffer_global;
	bool spawned = master[15] != 0 && buf != nullptr; // "creature spawned" byte of the master node
	uint32_t *seed = (uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14);
	uint32_t seed_save = *seed, gfseed_save = *(uint32_t *)g.seed_global;
	uint16_t master_counter_save = *(uint16_t *)(master + 12);
	uint32_t toggle_save = *(uint32_t *)(master + g.master_toggle_off);
	uint16_t creature_counter_save = spawned ? *(uint16_t *)(buf + g.creature_counter_off) : 0;

	if (spawned) ff8_bgate_tlb_creature_halfstep((uint32_t)(buf + g.creature_off), g.advance_fn);
	*(uint32_t *)g.pause_global = 1;
	ff8_bgate_shield_set(true);
	bool ok = ff8_bgate_tlb_queue_guarded(orig, queue);
	ff8_bgate_shield_set(false);
	*(uint32_t *)g.pause_global = 0;
	*seed = seed_save;
	*(uint32_t *)g.seed_global = gfseed_save;
	*(uint16_t *)(master + 12) = master_counter_save;
	if (g.master_toggle_size == 4) *(uint32_t *)(master + g.master_toggle_off) = toggle_save;
	else *(uint8_t *)(master + g.master_toggle_off) = (uint8_t)toggle_save;
	if (spawned) *(uint16_t *)(buf + g.creature_counter_off) = creature_counter_save;
	ff8_bgate_tlb_creature_restore();
	if (!ok)
	{
		ff8_bgate_tla_faults++;
		ffnx_info("30fps tl: %s HELD-FRAME FAULT %08X at %08X (tick %u, fault %u)\n", g.name, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr, ff8_bgate_tla_tick, ff8_bgate_tla_faults);
	}
	ff8_bgate_tla_redraws++;
	return true;
}

// --- look-ahead packet midpoint for the timeline GF families (ff8_bgate_la_*) ---
// Quezacotl's storm, Shiva's ice, Cactuar's needles...: hundreds of small particle tasks,
// each with its own private layout (position/velocity fields packed into the task node) and
// each drawing then advancing inside one function. Instead of teaching the mod every layout,
// the held frame is built from the EXACT next state: right after every real tick N the
// effect is run one more time (tick N+1) under a full state snapshot - the 1 MB magic
// buffer (creature, particle pools, packet arenas), the module's own globals, the task pools
// it executed, the camera, the GTE registers, the seeds, the ordering table and the draw-list
// cursors - with the side-effect shield up (no sound, no loads, no damage, no task spawn);
// its draws are copied, then everything is put back. The held frame draws every tick-N
// primitive half way to its tick-N+1 counterpart. Pairing is exact: the task queue executor
// is re-implemented while recording so every primitive is tagged with the task node that
// drew it, and a task's k-th primitive at N is paired with its k-th at N+1 (nearest same
// primitive within the task when the counts differ). The creature is included - its mesh
// triangles pair the same way - so no separate pose half-step is needed. When the look-ahead
// is unavailable (fault, overflow) the held frame falls back to the pause-mode paths
// (ff8_bgate_tla_* / ff8_bgate_tlb_*) and then to the generic replay.
// The module table (ff8fx::mod::modules: module globals data_lo..data_hi, extra cell, streams,
// lookahead = held frames use the look-ahead) and the engine regions every effect tick may
// touch (ff8fx::mod::engine_regions, saved before the look-ahead and restored after it,
// whatever happened) are shared with the verifier: src/ff8/battle/fx/fx_modules.cpp.
#define FF8_BGATE_LA_DATA_MAX (ff8fx::mod::DATA_MAX) // Siren module bss is 0x10454 bytes
#define FF8_BGATE_LA_POOLS_MAX 0x80000
static uint8_t ff8_bgate_la_save_regions[0x100000 + 0x1000];
static uint8_t ff8_bgate_la_save_data[FF8_BGATE_LA_DATA_MAX];
static uint8_t ff8_bgate_la_save_pools[FF8_BGATE_LA_POOLS_MAX];
static uint32_t ff8_bgate_la_save_ot[FF8_BGATE_OT_SPAN_WORDS];

// task pools of the queues the effect ran: header + node storage, saved before the look-ahead
// (a queue first seen inside the look-ahead is saved by the executor before it runs)
struct ff8_bgate_la_pool { ff8_bgate_task_queue *q; ff8_bgate_task_queue hdr; uint32_t off, size; };
static ff8_bgate_la_pool ff8_bgate_la_pools[32];
static int ff8_bgate_la_npools = 0;
static uint32_t ff8_bgate_la_pools_used = 0;
static bool ff8_bgate_la_pools_warned = false;

static void ff8_bgate_la_pool_save(ff8_bgate_task_queue *q)
{
	if (ff8_bgate_la_npools >= 32) return;
	ff8_bgate_la_pool &p = ff8_bgate_la_pools[ff8_bgate_la_npools++];
	p.q = q; p.hdr = *q; p.off = ff8_bgate_la_pools_used; p.size = 0;
	uint32_t size = (uint32_t)q->node_size * q->capacity;
	if (q->pool && size && size <= 0x10000 && ff8_bgate_la_pools_used + size <= FF8_BGATE_LA_POOLS_MAX && !IsBadReadPtr(q->pool, size))
	{
		memcpy(ff8_bgate_la_save_pools + p.off, q->pool, size);
		p.size = size;
		ff8_bgate_la_pools_used += size;
	}
	else if (q->pool && size && !ff8_bgate_la_pools_warned)
	{
		ff8_bgate_la_pools_warned = true;
		ffnx_info("30fps la: pool of queue %p not saved (%u bytes)\n", q, size);
	}
}

static int ff8_bgate_la_cur = -1;
static void *ff8_bgate_la_queue = nullptr;
static uint32_t ff8_bgate_la_ticks = 0, ff8_bgate_la_runs = 0, ff8_bgate_la_faults = 0, ff8_bgate_la_held = 0, ff8_bgate_la_fallback = 0;

static int ff8_bgate_la_module_of_effect()
{
	int id = *(int *)0x1D99A68 + 1;
	for (int m = 0; m < ff8fx::mod::module_count; m++)
		if (ff8fx::mod::modules[m].effect_id == id) return m;
	return -1;
}

static bool ff8_bgate_fxv_live = false; // the running summon is being verified (ff8_bgate_fxv_*)

static void ff8_bgate_la_reset()
{
	ff8_bgate_la_cur = -1;
	ff8_bgate_la_queue = nullptr;
	ff8_bgate_la_ticks = ff8_bgate_la_runs = ff8_bgate_la_faults = ff8_bgate_la_held = ff8_bgate_la_fallback = 0;
	ff8_bgate_la_pools_warned = false;
	ff8_bgate_etq_queues_n = 0;
	ff8_bgate_rec_fx.la.valid = false;
	ff8_bgate_fxv_live = false;
}

// Called before the real tick's queue call: is this a look-ahead effect? (turns task recording on)
static bool ff8_bgate_la_real_tick_begin(void *queue)
{
	int m = ff8_bgate_la_module_of_effect();
	if (m < 0 || !ff8_bgate_gfc_enabled) { if (ff8_bgate_la_cur >= 0) ff8_bgate_la_reset(); return false; }
	if (ff8_bgate_la_cur != m || ff8_bgate_la_queue != queue)
	{
		ff8_bgate_la_reset();
		ff8_bgate_la_cur = m;
		ff8_bgate_la_queue = queue;
		ffnx_info("30fps la: %s look-ahead summon, queue=%p\n", ff8fx::mod::modules[m].name, queue);
	}
	ff8_bgate_la_ticks++;
	ff8_bgate_etq_log_n = 0;
	ff8_bgate_etq_log_overflow = false;
	ff8_bgate_etq_rec = true;
	return true;
}

static void ff8_bgate_fxv_summary(const char *name);
static void ff8_bgate_la_finish()
{
	if (ff8_bgate_la_cur < 0) return;
	ffnx_info("30fps la: %s finished, real ticks=%u look-aheads=%u faults=%u held frames=%u (fallback %u) queues=%d\n",
		ff8fx::mod::modules[ff8_bgate_la_cur].name, ff8_bgate_la_ticks, ff8_bgate_la_runs, ff8_bgate_la_faults, ff8_bgate_la_held, ff8_bgate_la_fallback, ff8_bgate_etq_queues_n);
	ff8_bgate_fxv_summary(ff8fx::mod::modules[ff8_bgate_la_cur].name);
	ff8_bgate_la_reset();
}

// copies the recorded task ranges into the snapshot and tags every primitive with the
// innermost task that drew it (the log is in post-order: inner tasks before their master)
static void ff8_bgate_la_attach_tasks(ff8_bgate_fx_snap &s)
{
	s.ntasks = ff8_bgate_etq_log_n;
	memcpy(s.tasks, ff8_bgate_etq_log, sizeof(ff8_bgate_task_rng) * s.ntasks);
	for (int i = 0; i < FF8_BGATE_FX_MAX_PRIMS; i++) s.ei2prim[i] = -1;
	for (int i = 0; i < s.n; i++) { s.ei2prim[s.prim[i].ei] = (int16_t)i; s.task_of[i] = -1; }
	for (int t = 0; t < s.ntasks; t++)
		for (uint32_t ei = s.tasks[t].a; ei < s.tasks[t].b && ei < FF8_BGATE_FX_MAX_PRIMS; ei++)
		{
			int p = s.ei2prim[ei];
			if (p >= 0 && s.task_of[p] < 0) s.task_of[p] = (int16_t)t;
		}
}

// Snapshot of everything an effect tick may modify (regions, module globals, task pools of
// the queues seen so far, ordering table, CRT seed); shared by the look-ahead and the verifier.
static uint8_t ff8_bgate_snap_extra[16];
// sum of a module's stream regions (Gilgamesh: sword files 0x10C50 + summon data 0x40000 + cells)
#define FF8_BGATE_SNAP_STREAMS_MAX (ff8fx::mod::STREAMS_MAX)
static uint8_t ff8_bgate_snap_streams[FF8_BGATE_SNAP_STREAMS_MAX];
static uint32_t ff8_bgate_snap_seed = 0;
// Field_Alloc scratch (bump stack growing up from GLOBAL_MEMORY_POOL 0x1D999C4): effects
// allocate draw headers there and some engine calls read fields they never set, so the
// memory above the pointer is part of the state a tick sees
static uint8_t ff8_bgate_snap_scratch[0x1000];
static uint32_t ff8_bgate_snap_scratch_addr = 0;

static uint32_t *ff8_bgate_crt_seed() { return (uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14); }

static void ff8_bgate_snap_take(const ff8fx::mod::Module &g)
{
	uint32_t o = 0;
	for (int i = 0; i < ff8fx::mod::engine_region_count; i++) { memcpy(ff8_bgate_la_save_regions + o, (void *)ff8fx::mod::engine_regions[i].addr, ff8fx::mod::engine_regions[i].size); o += ff8fx::mod::engine_regions[i].size; }
	memcpy(ff8_bgate_la_save_data, (void *)g.data_lo, g.data_hi - g.data_lo);
	if (g.extra) memcpy(ff8_bgate_snap_extra, (void *)g.extra, g.extra_size);
	for (int i = 0, so = 0; i < g.nstreams; so += g.streams[i].size, i++)
		memcpy(ff8_bgate_snap_streams + so, (void *)g.streams[i].addr, g.streams[i].size);
	ff8_bgate_la_npools = 0;
	ff8_bgate_la_pools_used = 0;
	for (int i = 0; i < ff8_bgate_etq_queues_n; i++)
		ff8_bgate_la_pool_save((ff8_bgate_task_queue *)ff8_bgate_etq_queues[i]);
	memcpy(ff8_bgate_la_save_ot, (void *)FF8_BGATE_OT_SPAN_BASE(), sizeof(ff8_bgate_la_save_ot));
	ff8_bgate_snap_seed = *ff8_bgate_crt_seed();
	ff8_bgate_snap_scratch_addr = *(uint32_t *)0x1D999C4;
	memcpy(ff8_bgate_snap_scratch, (void *)ff8_bgate_snap_scratch_addr, sizeof(ff8_bgate_snap_scratch));
}

static void ff8_bgate_snap_restore(const ff8fx::mod::Module &g)
{
	memcpy((void *)FF8_BGATE_OT_SPAN_BASE(), ff8_bgate_la_save_ot, sizeof(ff8_bgate_la_save_ot));
	for (int i = ff8_bgate_la_npools - 1; i >= 0; i--)
	{
		ff8_bgate_la_pool &p = ff8_bgate_la_pools[i];
		if (p.size) memcpy(p.q->pool, ff8_bgate_la_save_pools + p.off, p.size);
		*p.q = p.hdr;
	}
	memcpy((void *)g.data_lo, ff8_bgate_la_save_data, g.data_hi - g.data_lo);
	if (g.extra) memcpy((void *)g.extra, ff8_bgate_snap_extra, g.extra_size);
	for (int i = 0, so = 0; i < g.nstreams; so += g.streams[i].size, i++)
		memcpy((void *)g.streams[i].addr, ff8_bgate_snap_streams + so, g.streams[i].size);
	uint32_t o = 0;
	for (int i = 0; i < ff8fx::mod::engine_region_count; i++) { memcpy((void *)ff8fx::mod::engine_regions[i].addr, ff8_bgate_la_save_regions + o, ff8fx::mod::engine_regions[i].size); o += ff8fx::mod::engine_regions[i].size; }
	*ff8_bgate_crt_seed() = ff8_bgate_snap_seed;
	memcpy((void *)ff8_bgate_snap_scratch_addr, ff8_bgate_snap_scratch, sizeof(ff8_bgate_snap_scratch));
}

// TEMP diagnostics: how well did the look-ahead of tick N predict the real tick N+1? Same task
// node, same rank: identical packet / same size but different (max vertex step) / count mismatch.
static void ff8_bgate_la_check(const ff8_bgate_fx_snap &cur)
{
	const ff8_bgate_fx_snap &la = ff8_bgate_rec_fx.la;
	if (ff8_bgate_la_cur < 0 || !la.valid || la.frame + (uint32_t)ff8_bgate_n != cur.frame) return;
	int tasks = 0, found = 0, identical = 0, differ = 0, mismatch = 0, maxd = 0, missing = 0;
	static ff8_bgate_pkt_info pi;
	for (int t = 0; t < cur.ntasks; t++)
	{
		const ff8_bgate_task_rng &ct = cur.tasks[t];
		if (ct.b <= ct.a) continue;
		tasks++;
		int lt = -1;
		for (int k = 0; k < la.ntasks; k++) if (la.tasks[k].node == ct.node) { lt = k; break; }
		if (lt < 0) { missing++; continue; }
		found++;
		const ff8_bgate_task_rng &l = la.tasks[lt];
		if (l.b - l.a != ct.b - ct.a) { mismatch++; continue; }
		for (uint32_t k = 0; k < (uint32_t)(ct.b - ct.a); k++)
		{
			uint32_t ec = ct.a + k, el = l.a + k;
			if (ec >= FF8_BGATE_FX_MAX_PRIMS || el >= FF8_BGATE_FX_MAX_PRIMS) continue;
			int pc = cur.ei2prim[ec], pl = la.ei2prim[el];
			if (pc < 0 || pl < 0) continue;
			const ff8_bgate_fx_prim &a = cur.prim[pc], &b = la.prim[pl];
			if (a.words == b.words && memcmp(&cur.arena[a.off + 1], &la.arena[b.off + 1], (a.words - 1) * 4) == 0) { identical++; continue; }
			differ++;
			if (a.words == b.words && ff8_bgate_pkt_parse(&cur.arena[a.off], a.words, pi))
			{
				int d = ff8_bgate_pkt_step(&cur.arena[a.off], &la.arena[b.off], pi);
				if (d > maxd) maxd = d;
			}
		}
	}
	if (ff8_bgate_la_ticks <= 400)
		ffnx_info("30fps la: check t=%u tasks=%d found=%d missing=%d count_mismatch=%d identical=%d differ=%d maxstep=%d\n",
			ff8_bgate_la_ticks, tasks, found, missing, mismatch, identical, differ, maxd);
}

// The look-ahead itself: run tick N+1 under snapshot, capture its draws, put everything back.
static void ff8_bgate_la_lookahead(void *queue, int (__cdecl *orig)(void *))
{
	int m = ff8_bgate_la_cur;
	ff8_bgate_fx_snap &la = ff8_bgate_rec_fx.la;
	la.valid = false;
	if (m < 0 || queue != ff8_bgate_la_queue || ff8_bgate_la_faults >= 3 || ff8_bgate_fx_mode < 2) return;
	const ff8fx::mod::Module &g = ff8fx::mod::modules[m];
	if (!g.lookahead || !ff8fx::mod::fits(g)) return;

	// --- snapshot ---
	ff8_bgate_snap_take(g);
	uint32_t rec_begin_save = ff8_bgate_fx_rec_begin;

	// --- tick N+1, shielded ---
	if (ff8_bgate_la_ticks <= 400)
		ffnx_info("30fps la: t=%u begin (real n=%d tasks=%d overflow=%d queues=%d pools=%d/%u bytes)\n", ff8_bgate_la_ticks,
			ff8_bgate_rec_fx.snaps[ff8_bgate_rec_fx.cur].n, ff8_bgate_rec_fx.snaps[ff8_bgate_rec_fx.cur].ntasks, (int)ff8_bgate_etq_log_overflow,
			ff8_bgate_etq_queues_n, ff8_bgate_la_npools, ff8_bgate_la_pools_used);
	ff8_bgate_etq_cycle = false;
	ff8_bgate_fx_rec_begin = FF8_BGATE_EXEC_CUR;
	ff8_bgate_etq_log_n = 0;
	ff8_bgate_etq_log_overflow = false;
	ff8_bgate_etq_rec = true;
	ff8_bgate_la_in_lookahead = true;
	ff8_bgate_shield_set(true);
	bool ok = ff8_bgate_tlb_queue_guarded(orig, queue);
	ff8_bgate_shield_set(false);
	ff8_bgate_la_in_lookahead = false;
	ff8_bgate_etq_rec = false;
	ff8_bgate_la_runs++;

	// --- capture its draws (packets still live in the effect's arenas) ---
	if (ok)
	{
		la.n = 0; la.orphans = 0; la.used = 0; la.rlist_delta = 0;
		la.ctx = queue; la.frame = ff8_bgate_frame_no; la.valid = false;
		__try { ff8_bgate_fx_capture_unsafe(la); }
		__except (EXCEPTION_EXECUTE_HANDLER) { la.valid = false; }
		if (la.valid) ff8_bgate_la_attach_tasks(la);
	}

	// --- restore ---
	ff8_bgate_snap_restore(g);
	ff8_bgate_fx_rec_begin = rec_begin_save;
	if (ff8_bgate_etq_cycle) { la.valid = false; ff8_bgate_la_faults++; }
	if (!ok)
	{
		ff8_bgate_la_faults++;
		la.valid = false;
		ffnx_info("30fps la: %s LOOK-AHEAD FAULT %08X at %08X (tick %u, fault %u)\n", g.name, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr, ff8_bgate_la_ticks, ff8_bgate_la_faults);
	}
	if (ff8_bgate_la_ticks <= 400)
		ffnx_info("30fps la: t=%u done ok=%d valid=%d la n=%d tasks=%d overflow=%d pools=%d\n", ff8_bgate_la_ticks, (int)ok, (int)la.valid, la.n, la.ntasks, (int)ff8_bgate_etq_log_overflow, ff8_bgate_la_npools);
}

// Held frame: draw tick N half way to tick N+1 (exact per-task pairing in the replay)
static bool ff8_bgate_la_held_frame(void *queue)
{
	if (ff8_bgate_la_cur < 0 || queue != ff8_bgate_la_queue || !ff8_bgate_gfc_enabled) return false;
	const ff8_bgate_fx_snap &la = ff8_bgate_rec_fx.la, &cur = ff8_bgate_rec_fx.snaps[ff8_bgate_rec_fx.cur];
	if (!la.valid || !cur.valid || la.ctx != cur.ctx || la.frame != cur.frame) { ff8_bgate_la_fallback++; return false; }
	ff8_bgate_la_active = true;
	ff8_bgate_fx_replay(queue);
	ff8_bgate_la_active = false;
	ff8_bgate_la_held++;
	return true;
}

// --- native effect ports (ff8fx, src/ff8/battle/fx) ---
// A real tick of an effect whose module is ported and fits the verifier's snapshot runs through
// the verifier of the native effect code (fx_verify.cpp: the original code and the ports from one
// snapshot, compared, the original's result kept on any difference and logged as
// "FF8 battle fx: ... MISMATCH"). The executor observer above logs each task's draw range in
// both runs; the log of the run whose result is kept stays (held frames skip the primitives of
// the tasks their module redraws).
static ff8_bgate_task_rng ff8_bgate_fxv_log_a[FF8_BGATE_TASK_LOG];
static int ff8_bgate_fxv_log_a_n = 0;
static uint32_t ff8_bgate_fxv_held_frames = 0; // native held frames of the running summon

static void ff8_bgate_fxv_event(ff8fx::VerifyEvent e)
{
	switch (e)
	{
	case ff8fx::VERIFY_ORIGINAL_RUN:
		ff8_bgate_etq_log_n = 0;
		ff8_bgate_etq_log_overflow = false;
		break;
	case ff8fx::VERIFY_PORT_RUN: // keep the original run's log, start the port run's
		ff8_bgate_fxv_log_a_n = ff8_bgate_etq_log_n;
		memcpy(ff8_bgate_fxv_log_a, ff8_bgate_etq_log, sizeof(ff8_bgate_task_rng) * ff8_bgate_etq_log_n);
		ff8_bgate_etq_log_n = 0;
		break;
	case ff8fx::VERIFY_KEPT_ORIGINAL: // the original's state was put back: its log too
		ff8_bgate_etq_log_n = ff8_bgate_fxv_log_a_n;
		memcpy(ff8_bgate_etq_log, ff8_bgate_fxv_log_a, sizeof(ff8_bgate_task_rng) * ff8_bgate_fxv_log_a_n);
		break;
	case ff8fx::VERIFY_KEPT_PORT:
		break;
	}
}

static bool ff8_bgate_fxv_wanted()
{
	if (ff8_bgate_la_cur < 0 || !ff8_battle_fx_native) return false;
	const ff8fx::mod::Module &g = ff8fx::mod::modules[ff8_bgate_la_cur];
	return ff8fx::module_ported(g.effect_id) && ff8fx::mod::fits(g);
}

// One real tick of a ported effect (task recording is on: ff8_bgate_la_real_tick_begin)
static int ff8_bgate_fxv_tick(void *queue, int (__cdecl *orig)(void *))
{
	ff8_bgate_fxv_live = true;
	return ff8fx::verify_tick(queue, orig);
}

// Held-frame leak check (ff8_battle_fx_held_check, diagnostics only): a native held frame must
// leave the running effect's state (module globals, extra cell, streams, magic buffer) exactly
// as the real tick left it. The areas are copied before the held draw and compared after it;
// the first differing byte of each area is logged. Nothing is restored: the game runs as usual.
static const uint32_t FF8_BGATE_HC_MAX = 0x180000;
static uint8_t *ff8_bgate_hc_buf = nullptr;
struct ff8_bgate_hc_area { uint32_t addr, size; const char *name; };
static ff8_bgate_hc_area ff8_bgate_hc_list[16];
static int ff8_bgate_hc_n = 0;
static uint32_t ff8_bgate_hc_checked = 0, ff8_bgate_hc_leaks = 0, ff8_bgate_hc_logged = 0;

static void ff8_bgate_hc_before(int m)
{
	ff8_bgate_hc_n = 0;
	if (!ff8_battle_fx_held_check || m < 0) return;
	if (!ff8_bgate_hc_buf) ff8_bgate_hc_buf = (uint8_t *)malloc(FF8_BGATE_HC_MAX);
	if (!ff8_bgate_hc_buf) return;
	const ff8fx::mod::Module &g = ff8fx::mod::modules[m];
	ff8_bgate_hc_list[ff8_bgate_hc_n++] = { g.data_lo, g.data_hi - g.data_lo, "module globals" };
	if (g.extra) ff8_bgate_hc_list[ff8_bgate_hc_n++] = { g.extra, g.extra_size, "module extra" };
	for (int i = 0; i < g.nstreams && ff8_bgate_hc_n < 15; i++)
		ff8_bgate_hc_list[ff8_bgate_hc_n++] = { g.streams[i].addr, g.streams[i].size, "stream" };
	ff8_bgate_hc_list[ff8_bgate_hc_n++] = { 0x20DFAB8, 0x100000, "magic buffer" };
	uint32_t off = 0;
	for (int i = 0; i < ff8_bgate_hc_n; i++)
	{
		if (off + ff8_bgate_hc_list[i].size > FF8_BGATE_HC_MAX) { ff8_bgate_hc_n = i; break; }
		memcpy(ff8_bgate_hc_buf + off, (void *)ff8_bgate_hc_list[i].addr, ff8_bgate_hc_list[i].size);
		off += ff8_bgate_hc_list[i].size;
	}
}

static void ff8_bgate_hc_after(int m, uint32_t tick)
{
	if (!ff8_bgate_hc_n) return;
	ff8_bgate_hc_checked++;
	bool leaked = false;
	uint32_t off = 0;
	for (int i = 0; i < ff8_bgate_hc_n; i++)
	{
		const ff8_bgate_hc_area &a = ff8_bgate_hc_list[i];
		const uint8_t *was = ff8_bgate_hc_buf + off, *now = (const uint8_t *)a.addr;
		off += a.size;
		if (!memcmp(was, now, a.size)) continue;
		leaked = true;
		uint32_t first = 0, n = 0;
		for (uint32_t k = 0; k < a.size; k++)
			if (was[k] != now[k]) { if (!n) first = k; n++; }
		if (ff8_bgate_hc_logged < 16)
		{
			ff8_bgate_hc_logged++;
			ffnx_error("30fps held check: %s tick %u LEAK in %s %08X+%X: %u bytes differ, first at %08X (was %02X now %02X)\n",
				ff8fx::mod::modules[m].name, tick, a.name, a.addr, a.size, n, a.addr + first, was[first], now[first]);
		}
	}
	if (leaked) ff8_bgate_hc_leaks++;
	ff8_bgate_hc_n = 0;
}

static void ff8_bgate_fxv_summary(const char *name)
{
	if (!ff8_bgate_fxv_held_frames) return;
	if (ff8_battle_fx_held_check)
		ffnx_info("30fps held: %s native held frames=%u, checked=%u, frames with a leak=%u, old-frame prims not claimed by a held task=%u, ended tasks' prims kept=%u\n", name, ff8_bgate_fxv_held_frames, ff8_bgate_hc_checked, ff8_bgate_hc_leaks, ff8_bgate_fx_unclaimed_prims, ff8_bgate_fx_kept_prims);
	else
		ffnx_info("30fps held: %s native held frames=%u, ended tasks' prims kept=%u\n", name, ff8_bgate_fxv_held_frames, ff8_bgate_fx_kept_prims);
	ff8_bgate_fxv_held_frames = 0;
	ff8_bgate_fx_kept_prims = 0;
	ff8_bgate_hc_checked = ff8_bgate_hc_leaks = ff8_bgate_hc_logged = 0;
	ff8_bgate_fx_unclaimed_prims = 0;
}

// --- tail of a finished queue (ff8_bgate_rec_t::tail) ---
// The real tick that empties a queue (r == 0) draws its last picture; vanilla shows it until its next frame (1/15 s), so
// the held frames after that tick replay its draws exactly as drawn. The magic effect tree's tick call is skipped once
// the battle loop clears the effect pointer (0x500946): its tail is drawn from the hit-effect queue gate, which runs every
// frame just before that call (same place in the frame). The next real frame drops it.
static void ff8_bgate_tail_drop(ff8_bgate_rec_t *rec)
{
	if (!rec->tail) return;
	rec->tail = rec->tail_otclear = false;
	rec->snaps[rec->cur].valid = false;
	memset(&rec->sum, 0, sizeof(rec->sum)); // (the tail's replays were reported with the finished queue)
}

static void ff8_bgate_tail_held(ff8_bgate_rec_t *rec)
{
	if (!rec->tail || ff8_bgate_phase == 0 || ff8_bgate_fx_mode == 3) return;
	ff8_bgate_rec_t *save = ff8_bgate_R;
	ff8_bgate_R = rec;
	if (rec->tail_otclear)
		((void (__cdecl *)(void *, int))0x45D530)((void *)FF8_BGATE_CUR_OT(), 4096); // the tick cleared the OT first, as every held frame of it does
	ff8_bgate_fx_replay(rec->snaps[rec->cur].ctx);
	ff8_bgate_R = save;
}

// HIT-EFFECT QUEUE NATIVE HELD FRAMES. The queue (0x209FAA8) belongs to no effect: hit sparks, fades, the magic-cast
// glow, Renzokuken parts... Its tasks whose function a module registered (ff8fx::register_eq_task, fx_held.h) run their
// ports on the queue's real ticks (ff8fx::g_eq_active) and note what they draw; the real tick also notes which task drew
// which primitive (task_of, as the effect tree's look-ahead does). A held frame then walks the real tick's primitives in
// order: a primitive of a native task with a held draw is replaced, at that task's first primitive, by the task's own
// in-between draw (ff8fx::eq_held_draw: tick N toward tick N+1 at phase / n, predicted from the noted state, nothing
// changed); a task that ended on the tick keeps its packets as drawn (ff8fx::keeps_ended), as does a task whose draw
// has nothing for this node; every other primitive is linked again as drawn (the generic replay, unchanged). So the draw
// order of the real tick is kept between the native draws and the replayed ones. The held draws use the GTE, the
// Field_Alloc scratch, the shadow scratch 0x21DFED0 and the render list's packet cursor 0x1D8E054: the modules put them
// back after each draw (their packets go to buffers of their own), and the gate saves / restores them around the whole
// frame as well, as around the effect tree's native held draw. No held draw calls rand() or spawns a task.
static bool ff8_bgate_eq_native_on = true; // off after a held-draw fault (back to the generic replay)
static uint32_t ff8_bgate_eq_held_frames = 0, ff8_bgate_eq_native_draws = 0, ff8_bgate_eq_fallback_draws = 0;
static uint32_t ff8_bgate_eq_kept_prims = 0, ff8_bgate_eq_replayed_prims = 0, ff8_bgate_eq_faults = 0;

static void ff8_bgate_eq_held_unsafe()
{
	const ff8_bgate_fx_snap &cur = ff8_bgate_fx_snaps[ff8_bgate_fx_cur];
	static uint8_t task_state[FF8_BGATE_TASK_LOG]; // 0 = not seen yet, 1 = drawn natively, 2 = linked as drawn
	memset(task_state, 0, cur.ntasks > 0 ? (size_t)cur.ntasks : 0);
	static ff8_bgate_pkt_info xi;
	const uint32_t cur_ot = FF8_BGATE_CUR_OT();
	uint32_t out_used = 0;
	for (int i = 0; i < cur.n; i++)
	{
		const ff8_bgate_fx_prim &e = cur.prim[i];
		const uint32_t *src = &cur.arena[e.off];
		const int t = cur.ntasks > 0 ? cur.task_of[i] : -1;
		if (t >= 0 && ff8fx::eq_has_draw(cur.tasks[t].func))
		{
			if (cur.tasks[t].ended && ff8fx::keeps_ended(cur.tasks[t].func))
				ff8_bgate_eq_kept_prims++;
			else
			{
				if (task_state[t] == 0)
				{
					const bool ok = ff8fx::eq_held_draw(cur.tasks[t].func, cur.tasks[t].node, ff8_bgate_phase, ff8_bgate_n);
					task_state[t] = ok ? 1 : 2;
					if (ok) ff8_bgate_eq_native_draws++;
					else ff8_bgate_eq_fallback_draws++;
				}
				if (task_state[t] == 1) continue; // its in-between draw takes the place of its packets
			}
		}
		else ff8_bgate_eq_replayed_prims++;
		// linked again as drawn, exactly like ff8_bgate_fx_replay_unsafe (VRAM transfers are never replayed)
		if (!ff8_bgate_pkt_parse(src, e.words, xi) && xi.last_cmd >= 0x80 && xi.last_cmd <= 0xDF) continue;
		if (FF8_BGATE_EXEC_CUR - FF8_BGATE_EXEC_START >= FF8_BGATE_EXEC_LIMIT) break;
		uint32_t *dst = &ff8_bgate_fx_out[out_used];
		memcpy(dst, src, e.words * 4);
		out_used += e.words;
		ff8_bgate_exec_node *node = (ff8_bgate_exec_node *)FF8_BGATE_EXEC_CUR;
		uint32_t *bucket = (uint32_t *)cur_ot + e.bucket;
		const uint32_t old_head = *bucket;
		node->pkt = dst;
		memcpy(node->k, e.k, sizeof(node->k));
		node->msk = e.msk;
		node->link_hi = (uint8_t)(old_head >> 24);
		*bucket = (uint32_t)node;
		dst[0] = (dst[0] & 0xFF000000) | (old_head & 0xFFFFFF);
		FF8_BGATE_EXEC_CUR += sizeof(ff8_bgate_exec_node);
	}
}

// true: the held frame is drawn (the snapshot holds a draw of a native task), false: generic replay
static bool ff8_bgate_eq_held(void *ctx)
{
	if (!ff8_battle_fx_native || !ff8_bgate_eq_native_on || ff8_bgate_fx_replay_is_blacklisted(ctx)) return false;
	const ff8_bgate_fx_snap &cur = ff8_bgate_fx_snaps[ff8_bgate_fx_cur];
	if (!cur.valid || cur.ntasks <= 0) return false;
	bool any = false;
	for (int t = 0; t < cur.ntasks && !any; t++) any = ff8fx::eq_has_draw(cur.tasks[t].func);
	if (!any) return false;
	static uint8_t gte_data[0x90], gte_ctrl[0xF0];
	memcpy(gte_data, (void *)0x1CA8A10, sizeof(gte_data));
	memcpy(gte_ctrl, (void *)0x1CA9210, sizeof(gte_ctrl));
	const uint32_t pool = *(uint32_t *)0x1D999C4;
	static uint8_t fa_scr[ff8fx::mod::FIELD_ALLOC_SCRATCH], fa_tmp[0x84], t_c18[8], t_ed0[0x20];
	const bool fa_ok = pool && !IsBadReadPtr((void *)pool, sizeof(fa_scr));
	if (fa_ok) memcpy(fa_scr, (void *)pool, sizeof(fa_scr));
	memcpy(fa_tmp, (void *)0x1D999C4, sizeof(fa_tmp));
	memcpy(t_c18, (void *)0x1D99C18, sizeof(t_c18));
	memcpy(t_ed0, (void *)0x21DFED0, sizeof(t_ed0));
	const uint32_t cursor = *(uint32_t *)0x1D8E054;
	__try { ff8_bgate_eq_held_unsafe(); }
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ff8_bgate_eq_faults++;
		ff8_bgate_eq_native_on = false;
		ffnx_info("30fps held: hit-effect queue native held draw FAULT (f=%u): generic replay from now on\n", ff8_bgate_frame_no);
	}
	*(uint32_t *)0x1D8E054 = cursor;
	memcpy((void *)0x1D999C4, fa_tmp, sizeof(fa_tmp));
	if (fa_ok) memcpy((void *)pool, fa_scr, sizeof(fa_scr));
	memcpy((void *)0x1D99C18, t_c18, sizeof(t_c18));
	memcpy((void *)0x21DFED0, t_ed0, sizeof(t_ed0));
	memcpy((void *)0x1CA8A10, gte_data, sizeof(gte_data));
	memcpy((void *)0x1CA9210, gte_ctrl, sizeof(gte_ctrl));
	ff8_bgate_eq_held_frames++;
	return true;
}

// the queue emptied on this real tick: summary of its native held frames since it last emptied
static void ff8_bgate_eq_finish()
{
	if (!ff8_bgate_eq_held_frames) return;
	ffnx_info("30fps held: hit-effect queue native held frames=%u, task draws=%u (no state: %u), ended tasks' prims kept=%u, prims replayed=%u, faults=%u\n",
		ff8_bgate_eq_held_frames, ff8_bgate_eq_native_draws, ff8_bgate_eq_fallback_draws, ff8_bgate_eq_kept_prims, ff8_bgate_eq_replayed_prims, ff8_bgate_eq_faults);
	ff8_bgate_eq_held_frames = ff8_bgate_eq_native_draws = ff8_bgate_eq_fallback_draws = ff8_bgate_eq_kept_prims = ff8_bgate_eq_replayed_prims = 0;
}

// Shared gate body for a recorded queue (ff8_bgate_R already selected): real frame =
// tick at native rate and capture its draws; held frame = no tick, replay the draws
// extrapolated. held_ret is what the caller sees on held frames.
static int ff8_bgate_gate_tick(void *ctx, int (__cdecl *orig)(void *), int held_ret)
{
	if (ff8_bgate_fx_bypass)
		return orig(ctx); // F5: stock behaviour (every host frame), no recording, no replay
	if (ff8_bgate_phase == 0)
	{
		// real frame: advance + draw at native rate; the draws it makes are the arena nodes
		// created between these two cursor reads (see the recording note above)
		ff8_bgate_tail_drop(ff8_bgate_R);
		ff8_bgate_etq_fn_depth = 0;
		ff8_bgate_fx_rec_begin = FF8_BGATE_EXEC_CUR;
		ff8_bgate_fx_rec_ot = FF8_BGATE_CUR_OT();
		if (ff8_bgate_R == &ff8_bgate_rec_fx)
		{
			uint32_t *ot = (uint32_t *)ff8_bgate_fx_rec_ot;
			ff8_bgate_fx_ot_probe[0] = ot[1];
			ff8_bgate_fx_ot_probe[1] = ot[4095];
		}
		ff8_bgate_fx_rec_rlist = FF8_BGATE_RLIST_CUR;
		ff8_bgate_fx_replay_ok = true;
		int vq_before = FF8_BGATE_VQ_COUNT;
		if (ff8_bgate_R == &ff8_bgate_rec_fx)
		{
			ff8_bgate_gfc_logging = ff8_bgate_gfc_module_of_effect() >= 0;
			ff8_bgate_gfc_log_n = 0;
			ff8_bgate_gfc_log_overflow = false;
		}
		if (ff8_bgate_R == &ff8_bgate_rec_fx) ff8fx::g_real_tick++;
		bool la_tick = ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_la_real_tick_begin(ctx);
		bool fxv_tick = la_tick && ff8_bgate_fxv_wanted();
		// hit-effect queue: its native tasks (ff8fx::register_eq_task) run their ports, and the draw range of every task
		// is noted (task_of) for the held frames (ff8_bgate_eq_held)
		const bool eq_tick = ff8_bgate_R == &ff8_bgate_rec_eq;
		if (eq_tick)
		{
			ff8fx::g_eq_tick++;
			ff8_bgate_etq_log_n = 0;
			ff8_bgate_etq_log_overflow = false;
			ff8_bgate_etq_rec_eq = true;
			ff8fx::g_eq_active = ff8_battle_fx_native && ff8_bgate_eq_native_on;
		}
		int r = fxv_tick ? ff8_bgate_fxv_tick(ctx, orig) : orig(ctx);
		ff8_bgate_etq_rec = false;
		ff8_bgate_etq_rec_eq = false;
		ff8fx::g_eq_active = false;
		ff8_bgate_gfc_logging = false;
		ff8_bgate_vq_record(vq_before, FF8_BGATE_VQ_COUNT);
		for (int i = vq_before; i < FF8_BGATE_VQ_COUNT && i < 32; i++)
		{
			uint8_t ty = *FF8_BGATE_VQ_SLOT(i);
			if (ty < 4) ff8_bgate_fx_sum.vq_types[ty]++;
		}
		if (ff8_bgate_R == &ff8_bgate_rec_fx)
		{
			// cleared link = &bucket[i-1]; a bucket that held a node before and is back to its
			// cleared link now was wiped by the effect (its own draws go into other buckets
			// or were prepended after the clear - a still-linked node also means "not cleared")
			uint32_t *ot = (uint32_t *)ff8_bgate_fx_rec_ot;
			bool had1 = ff8_bgate_fx_ot_probe[0] != (uint32_t)&ot[0], had4095 = ff8_bgate_fx_ot_probe[1] != (uint32_t)&ot[4094];
			bool cleared = (had1 && ot[1] == (uint32_t)&ot[0]) || (had4095 && ot[4095] == (uint32_t)&ot[4094]);
			if (cleared) ff8_bgate_fx_otclear = true;
		}
		if (ff8_bgate_R == &ff8_bgate_rec_fx) ff8_bgate_diff_frame(true);
		ff8_bgate_R->last_r = r;
		ff8_bgate_fx_sum.ticks++;
		if (ff8_bgate_R == &ff8_bgate_rec_fx)
			_snprintf_s(ffnx_cap_label, sizeof(ffnx_cap_label), _TRUNCATE, "f%u_REAL_fx%d_t%u", ff8_bgate_frame_no, *(int *)0x1D99A68 + 1, ff8_bgate_fx_sum.ticks);
		// long effects (Eden runs ~1250 ticks): report progress so the diagnostics do not
		// depend on the effect reaching its end
		if (ff8_bgate_vq_trace)
		{
			ffnx_info("30fps frame: f=%u ph=%d parity=%u arena=%08X rlist=%08X nodes=%d\n",
				ff8_bgate_frame_no, ff8_bgate_phase, *(uint8_t *)0x1D96A80, *(uint32_t *)0x1D8E054,
				*(uint32_t *)0x1D8E04C, ff8_bgate_fx_snaps[ff8_bgate_fx_cur].n);
			ff8_bgate_vq_trace--;
		}
		if (ff8_bgate_fx_sum.ticks % 300 == 0)
			ff8_bgate_fx_summary("in progress");
		if (ff8_bgate_R == &ff8_bgate_rec_fx) { ff8_bgate_tlb_real_tick(ctx, r); ff8_bgate_tla_real_tick(ctx, r); }
		if (r == 0)
		{
			// queue empty = effect finished. Vanilla displays this last tick's draws for its whole frame: they are
			// captured as any tick's and replayed as drawn on the held frames that follow (the tail, no in-between:
			// there is no next tick), then dropped at the next real frame. Never paired with, nor drawn into, the
			// next effect: the replay of a new effect stays off until its own first real tick.
			bool tail = !ff8_bgate_fx_replay_is_blacklisted(ctx);
			if (tail)
			{
				ff8_bgate_fx_capture(ctx);
				tail = ff8_bgate_fx_snaps[ff8_bgate_fx_cur].valid && ff8_bgate_fx_snaps[ff8_bgate_fx_cur].n > 0;
			}
			if (eq_tick) ff8_bgate_eq_finish();
			if (tail && ff8_bgate_R == &ff8_bgate_rec_fx)
				ff8_bgate_fx_recapture_pending = true; // what the display shows (packet aliasing), as any tick
			ff8_bgate_R->tail = tail;
			ff8_bgate_R->tail_otclear = tail && ff8_bgate_fx_otclear;
			ff8_bgate_fx_summary("finished");
			if (ff8_bgate_R == &ff8_bgate_rec_fx) ff8_bgate_la_finish();
			if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_gfc_ctx)
			{
				char hl[256]; int ho = 0;
				for (int i = 0; i < 73; i++) if (ff8_bgate_gfc_stats.handlers[i] && ho < 240) ho += sprintf(hl + ho, " %d", i);
				ffnx_info("30fps gf: finished, mode=%d real ticks=%u held redraws=%u pure ops replayed=%u bone half-steps=%u particle half-steps=%u model half-steps=%u (faults %u) pool arena=%u bytes log_overflow=%d handlers:%s\n",
					ff8_bgate_fx_mode, ff8_bgate_gfc_tick, ff8_bgate_gfc_stats.redraws, ff8_bgate_gfc_stats.replayed_ops, ff8_bgate_gfc_stats.bones_stepped, ff8_bgate_gfc_stats.particles_stepped, ff8_bgate_gfc_stats.models_stepped, ff8_bgate_gfc_stats.model_faults, ff8_bgate_gfc_stats.arena_bytes, (int)ff8_bgate_gfc_log_overflow, hl);
				memset(&ff8_bgate_gfc_stats, 0, sizeof(ff8_bgate_gfc_stats));
				ff8_bgate_gfc_reset();
			}
			ff8_bgate_fx_replay_ok = false;
			ff8_bgate_fx_otclear = false;
			ff8_bgate_fx_snaps[ff8_bgate_fx_cur ^ 1].valid = false;
			if (!tail) ff8_bgate_fx_snaps[ff8_bgate_fx_cur].valid = false;
		}
		else if (ff8_bgate_fx_replay_ok)
		{
			ff8_bgate_fx_capture(ctx);
			if (ff8_bgate_R == &ff8_bgate_rec_fx)
				ff8_bgate_fx_recapture_pending = true;
			if (eq_tick && ff8_bgate_fx_snaps[ff8_bgate_fx_cur].valid)
				ff8_bgate_la_attach_tasks(ff8_bgate_fx_snaps[ff8_bgate_fx_cur]);
			if (la_tick && ff8_bgate_fx_snaps[ff8_bgate_fx_cur].valid)
				ff8_bgate_la_attach_tasks(ff8_bgate_fx_snaps[ff8_bgate_fx_cur]);
			if (la_tick && !fxv_tick && ff8_bgate_fx_snaps[ff8_bgate_fx_cur].valid)
			{
				ff8_bgate_la_check(ff8_bgate_fx_snaps[ff8_bgate_fx_cur]);
				ff8_bgate_la_lookahead(ctx, orig);
			}
		}
		return r;
	}
	if (ff8_bgate_vq_replay_all)
		ff8_bgate_vq_replay();
	// held frame: no advance; draw the in-between pose of the last real frame's primitives
	// (no VRAM command is re-queued: Eden's type-3 copies turned out to be small texture
	// animations inside the texture area, not screen snapshots, and repeating its type-0
	// streaming uploads would only re-upload the same rows)
	if (ff8_bgate_R == &ff8_bgate_rec_fx)
		_snprintf_s(ffnx_cap_label, sizeof(ffnx_cap_label), _TRUNCATE, "f%u_held_fx%d_t%u", ff8_bgate_frame_no, *(int *)0x1D99A68 + 1, ff8_bgate_fx_sum.ticks);
	if (ff8_bgate_R == &ff8_bgate_rec_fx) ff8_bgate_diff_frame(false);
	if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_otclear && ff8_bgate_fx_replay_ok)
		((void (__cdecl *)(void *, int))0x45D530)((void *)FF8_BGATE_CUR_OT(), 4096); // SSIGPU_ClearOrderingTable, as the effect's tick does
	if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_replay_ok && ff8_bgate_fxv_live && ff8_bgate_gfc_enabled && ff8_bgate_fx_mode >= 2)
	{
		int eid = *(int *)0x1D99A68 + 1;
		if (ff8fx::held_ready(eid))
		{
			ff8_bgate_fx_skip_held = true;
			ff8_bgate_fx_replay(ctx);
			ff8_bgate_fx_skip_held = false;
			// the native draw uses the GTE and the scratch stack like a real tick: both are put
			// back so the next real tick starts from exactly the state vanilla would have
			static uint8_t gte_data[0x90], gte_ctrl[0xF0];
			memcpy(gte_data, (void *)0x1CA8A10, sizeof(gte_data));
			memcpy(gte_ctrl, (void *)0x1CA9210, sizeof(gte_ctrl));
			uint32_t pool = *(uint32_t *)0x1D999C4;
			// scratch the held draw may write that a later real tick reads: the Field_Alloc window
			// above the pointer, the temporaries after it, the shadow scratch and 0x1D99C18
			static uint8_t fa_scr[ff8fx::mod::FIELD_ALLOC_SCRATCH], fa_tmp[0x84], t_c18[8], t_ed0[0x20];
			bool fa_ok = pool && !IsBadReadPtr((void *)pool, sizeof(fa_scr));
			if (fa_ok) memcpy(fa_scr, (void *)pool, sizeof(fa_scr));
			memcpy(fa_tmp, (void *)0x1D999C4, sizeof(fa_tmp));
			memcpy(t_c18, (void *)0x1D99C18, sizeof(t_c18));
			memcpy(t_ed0, (void *)0x21DFED0, sizeof(t_ed0));
			ff8_bgate_hc_before(ff8_bgate_la_cur);
			const uint32_t held_h0 = FF8_BGATE_EXEC_CUR;
			__try { ff8fx::held_draw(eid, ff8_bgate_phase, ff8_bgate_n); }
			__except (ff8_bgate_gfc_fault_filter(GetExceptionInformation()))
			{
				ffnx_info("30fps held: effect %d native held draw FAULT %08X at %08X\n", eid, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr);
			}
			// the tasks that ended on the last real tick keep their last picture (ff8fx::keep_ended)
			ff8_bgate_fx_keep_ended(ctx, held_h0, FF8_BGATE_EXEC_CUR);
			// the entities the effect moves: their next real tick's state (ff8_bgate_ent_*)
			__try { ff8_bgate_ent_collect(eid); }
			__except (EXCEPTION_EXECUTE_HANDLER) { ff8_bgate_nent_req = 0; }
			memcpy((void *)0x1D999C4, fa_tmp, sizeof(fa_tmp)); // pointer + temporaries
			if (fa_ok) memcpy((void *)pool, fa_scr, sizeof(fa_scr));
			memcpy((void *)0x1D99C18, t_c18, sizeof(t_c18));
			memcpy((void *)0x21DFED0, t_ed0, sizeof(t_ed0));
			memcpy((void *)0x1CA8A10, gte_data, sizeof(gte_data));
			memcpy((void *)0x1CA9210, gte_ctrl, sizeof(gte_ctrl));
			ff8_bgate_fxv_held_frames++;
			ff8_bgate_hc_after(ff8_bgate_la_cur, ff8_bgate_fx_sum.ticks);
			return held_ret;
		}
	}
	if (FF8_BGATE_LEGACY_HELD && ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_replay_ok && !ff8_bgate_fxv_live && ff8_bgate_la_held_frame(ctx))
		return held_ret;
	if (FF8_BGATE_LEGACY_HELD && ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_replay_ok && !ff8_bgate_fxv_live && ff8_bgate_tla_held_frame(ctx, orig))
		return held_ret;
	if (FF8_BGATE_LEGACY_HELD && ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_replay_ok && !ff8_bgate_fxv_live && ff8_bgate_tlb_held_frame(ctx, orig))
		return held_ret;
	if (FF8_BGATE_LEGACY_HELD && ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_replay_ok && ff8_bgate_gfc_held_frame(ctx, orig))
		return held_ret;
	if (ff8_bgate_R == &ff8_bgate_rec_eq && ff8_bgate_fx_replay_ok && ff8_bgate_fx_mode == 2 && ff8_bgate_eq_held(ctx))
		return held_ret;
	if (ff8_bgate_fx_replay_ok && ff8_bgate_fx_mode != 3)
		ff8_bgate_fx_replay(ctx);
	else if (ff8_bgate_R == &ff8_bgate_rec_eq)
		ff8_bgate_tail_held(ff8_bgate_R); // (the effect tree's tail is drawn by the hit-effect queue gate)
	return held_ret;
}

static bool ff8_bgate_fx_held_camera(int16_t world[3], int16_t lookat[3])
{
	if (ff8_bgate_phase == 0 || !ff8_bgate_fxv_live || !ff8_bgate_gfc_enabled || ff8_bgate_fx_mode < 2 || !ff8_bgate_fx_replay_ok)
		return false;
	int eid = *(int *)0x1D99A68 + 1;
	if (!ff8fx::held_ready(eid)) return false;
	// predictors may run the module's own camera math through the GTE and the scratch stack:
	// both are put back so the frame and the next real tick see exactly vanilla state
	static uint8_t gte_data[0x90], gte_ctrl[0xF0];
	memcpy(gte_data, (void *)0x1CA8A10, sizeof(gte_data));
	memcpy(gte_ctrl, (void *)0x1CA9210, sizeof(gte_ctrl));
	uint32_t pool = *(uint32_t *)0x1D999C4;
	// (the camera-script prediction 0x63E9C0 / CsRun works in a Field_Alloc block: the scratch above the pointer is put back
	// too, as around the native held draw: the next real tick's draw headers read the fields they do not set)
	static uint8_t fa_scr[ff8fx::mod::FIELD_ALLOC_SCRATCH];
	const bool fa_ok = pool && !IsBadReadPtr((void *)pool, sizeof(fa_scr));
	if (fa_ok) memcpy(fa_scr, (void *)pool, sizeof(fa_scr));
	bool ok = false;
	__try { ok = ff8fx::held_camera(eid, ff8_bgate_phase, ff8_bgate_n, world, lookat); }
	__except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
	*(uint32_t *)0x1D999C4 = pool;
	if (fa_ok) memcpy((void *)pool, fa_scr, sizeof(fa_scr));
	memcpy((void *)0x1CA8A10, gte_data, sizeof(gte_data));
	memcpy((void *)0x1CA9210, gte_ctrl, sizeof(gte_ctrl));
	return ok;
}

int __cdecl ff8_bgate_effect_tick_gate(void *effect_ctx)
{
	ff8_bgate_R = &ff8_bgate_rec_fx;
	// 1 on held frames: still running (don't let the caller clear the effect pointer)
	int r = ff8_bgate_gate_tick(effect_ctx, ff8_bgate_effect_tick_orig, 1);
	if (r == 0 && ff8_bgate_phase == 0 && !ff8_bgate_fx_bypass && ff8_bgate_n > 1)
	{
		// finished: the pointer stays set through the held frames of this tick (the empty queue is not run again: held
		// frames never tick), released at the next real frame (ff8_bgate_fx_end_release)
		ff8_bgate_fx_end_hold = (uint32_t)effect_ctx;
		return 1;
	}
	return r;
}

// Battle-stage queue (ExecuteTaskQueue(g_BattleStageTaskQueue) @0x50090B, the FIRST queue of BdLink): the battle
// LOGIC's state machines. Its first task is BattleTaskMainHandler (0x500CC0, the BATTLE_TASK_MANAGER FIFO: action
// boundaries, processBattleActionCompletion = BATTLE_CURRENTLY_IN_ACTION 0x1D27B00 back to 0, text / fade / anim
// requests), then the tasks it spawns: the action sequences (BattleActionSequence_Tick_* 0x50A9A0..0x50BEE0: wait for
// the AnimSeq flags 0x10 / 0x20, the hit-effect queue, the effect pointer, the camera; start the effect; mark the
// effect done; end the action), entity load / wait tasks, the r0win victory starter (0x503040), AI enable. All of it is
// code that polls and moves one state per call: no data is played there (the sequences, animations and cameras it
// starts are played by their own players, every host frame). Called every host frame, each state step took 1/30 s
// instead of 1/15 s: every reaction of the battle logic came 1/30 s early (the effect started ~0.1 s early after the
// cast's AnimSeq waits, and the 3 steps after the effect end - state 6 effect end, state 7 action-sequence done, the
// completion callback - ended the action 3 host frames early: Fira at 30 fps ended ~0.2 s before vanilla). Real frames
// only, like vanilla's one call per 15 Hz tick; on a real frame it runs before the effect tick, as in vanilla, so an
// effect is always started on a real frame and gets its first tick in that same frame.
static int (__cdecl *ff8_bgate_stageq_orig)(void *) = nullptr;

int __cdecl ff8_bgate_stageq_gate(void *queue)
{
	if (ff8_bgate_phase != 0 && ff8_bgate_n > 1)
		return 1; // (the caller ignores the return)
	return ff8_bgate_stageq_orig(queue);
}

// BattleTurn_ProcessActionQueue (0x485460, from FFBattleDirector_battleLoop @0x47D7C3, every battle loop iteration,
// just before that iteration's BdLink): when no action runs it starts the next queued one (BEGIN boundary =
// BATTLE_CURRENTLY_IN_ACTION 1) and queues its tasks for BattleTaskMainHandler. Vanilla starts an action and runs its
// first tasks in the same tick; a reaction AI section (queued by processBattleActionCompletion) even starts and ends
// there (Battle_FinishAISectionAndQueueReactions clears the flag again). With the stage queue on real frames, an
// action started on an iteration whose BdLink is held waited a frame for its tasks: the flag rose for one frame after
// every action end (the reaction section), and every action started 1 frame before its logic. Only on the iterations
// whose BdLink is a real frame (the hook bumps ff8_bgate_phase first: real <=> (phase + 1) % n == 0).
// Emulated CD load time of battle files: LoadBattleFile (0x482610) registers BattleFile_RunCompletionCountdown
// (0x482870) with frames_remaining = file size / 20480 (/ 10240 outside the fight); BattleFile_PollOneCallback runs it
// once per battle loop iteration, just before the iteration's BdLink (0x47D900 = poll + jmp BdLink), and the load
// "completes" (BATTLE_FILE_LOADING_STATE >= 0, completion callback) when it reaches 0. Per host frame, every load
// completed in half the vanilla ticks: Fira's texture restore (A8DEF.TIM 99168 bytes = 4 -> 5 polls) ended its
// WaitTexRestore ~2.5 ticks early (the action ended 0.16-0.26 s early), effect files (MagicCast state 3), GF files
// (GF_Cinematic state 2) and model loads came early too. While an action runs (BATTLE_CURRENTLY_IN_ACTION 0x1D27B00:
// set by the action start, cleared by its completion; the cast, effect, GF and texture-restore loads all happen inside
// one) or once the battle is decided (battle_result 0x1CFF6E7 != 0: victory / escape / defeat, e.g. the victory pose
// file r0win.dat that the win sequence waits on) the countdown counts only on the iterations whose BdLink is a real
// frame, so the cast / effect / sound / victory timing stays vanilla. Outside an action (the battle-start stage, monster and party loads) it counts
// every host frame: faster loading, nothing visible waits on it there. Not gated either if BdLink stopped running
// (ff8_bgate_frame_no unchanged), so a load can never stall.
static int (__cdecl *ff8_bgate_fileload_orig)() = nullptr;
static uint32_t ff8_bgate_fileload_ri = 0;

int __cdecl ff8_bgate_fileload_countdown_hook()
{
	static uint32_t last_frame_no = 0;
	static int stale = 0;
	stale = (ff8_bgate_frame_no == last_frame_no) ? stale + 1 : 0;
	last_frame_no = ff8_bgate_frame_no;
	if (ff8_bgate_n > 1 && !ff8_bgate_left_battle && stale < 2 && getmode_cached()->driver_mode == MODE_BATTLE
		&& (*(uint8_t *)0x1D27B00 != 0 || *(uint8_t *)0x1CFF6E7 != 0) && (ff8_bgate_phase + 1) % ff8_bgate_n != 0)
		return 0; // (BattleFile_PollOneCallback ignores the return; the slot stays registered)
	unreplace_function(ff8_bgate_fileload_ri);
	int r = ff8_bgate_fileload_orig();
	rereplace_function(ff8_bgate_fileload_ri);
	return r;
}

static int (__cdecl *ff8_bgate_actq_orig)() = nullptr;

int __cdecl ff8_bgate_actq_gate()
{
	if (ff8_bgate_n > 1 && (ff8_bgate_phase + 1) % ff8_bgate_n != 0)
		return 0; // (the caller ignores the return)
	return ff8_bgate_actq_orig();
}

// executeTextManagementBattleAction (0x482D50, battle loop @0x47D7FB, every iteration): runs the battle "text
// management" slots, all tick counters or one-shots on TEXT_PRINT_QUEUE[].time_wait: Battle_EndFadeTimerCallback
// (0x47E030, end of battle: time_wait = fade duration - 15, then the fade to black), BattleText_WaitAndResumeBattle
// (0x48ACD0, AI waits: battle suspended for time_wait), BattleText_PrintQueuedMessageAfterDelay (0x48AC90, AI message
// after time_wait), BattleText_PrintQueuedMessage (0x48AC60). No data. Per host frame they counted 2x fast (the screen
// went black ~1.1 s early after a won escape). Only on the iterations whose BdLink is a real frame, like the action queue.
// Battle LOGIC loop at 15 Hz: FFBattleDirector_battleLoop (0x47CCB0, from battle_cardgame_main_loop @0x47D113, once per
// host frame) is the battle's per-tick logic: the module/flow state machines (GLOBAL_BATTLE_MOD_STATE 0x1CFF83C
// countdown -> transition -> in battle; BATTLE_LOOP_FLOW_STATE: start encounter -> load monsters (monster info, party
// formation, weapon anim init) -> end load (BATTLE_IS_ACTIVE, AI init) -> fighting), and in the fight the victory /
// defeat / escape checks, the player command intake, monster turns, the action queue, status timers, the text-management
// timers, the end-of-battle countdown (FADE_OUT_END_BATTLE_DURATION 0x1D27B0C) - with its BdLink (0x500900) in the
// middle. Run once per host frame, every step of it came one host frame after the previous one instead of one tick:
// a flow transition or a command taken on a held-bound iteration started its models / sequences / ATB one frame off the
// 2 x tick grid (the converted data then ran half a tick out of phase with the 15 Hz logic and the effects).
// Now the whole loop runs on the iterations whose BdLink is a real frame, exactly as vanilla runs it once per tick; the
// other iterations only do what keeps the DATA running: the file poll + BdLink (the battle stage at 30 Hz, its held
// frames). States without a BdLink (the module start states before the first BdLink, the stage init iteration, and the
// exit chain once the end countdown is 0: unload, writeback, mode_StateGlobal 0x1CD8FC6 = 5 / 6 / 7, the module switch)
// run 1 call in n. The card game (state 8) is untouched. battleLoop is __usercall (ecx, ebp): naked thunk.
static uint32_t ff8_bgate_battleloop_orig = 0;

// 0 = run the battle loop, 1 = skip this call, 2 = data only (file poll + BdLink)
static int __cdecl ff8_bgate_battleloop_mode()
{
	static int k = 0;
	if (ff8_bgate_n <= 1 || getmode_cached()->driver_mode != MODE_BATTLE) { k = 0; return 0; }
	const uint16_t state = *(uint16_t *)0x1CD8FC6;        // mode_StateGlobal
	const uint32_t mod_state = *(uint32_t *)0x1CFF83C;    // GLOBAL_BATTLE_MOD_STATE (3 = in battle)
	const uint32_t load = *(uint32_t *)0x1CFF848;         // BATTLE_STAGE_LOAD_STATUS (0 init, 1 loaded, 2 unload)
	const uint8_t fade_out = *(uint8_t *)0x1D27B0C;
	const bool no_bdlink = state == 4 || state == 5 || state == 6 || state == 7
		|| (state == 3 && (mod_state != 3 || load != 1 || fade_out == 0));
	if (no_bdlink)
	{
		// one call per vanilla tick, the first one after a BdLink-driven stretch on the held slot
		k = (k + 1) % ff8_bgate_n;
		return k != 0 ? 1 : 0;
	}
	k = 0;
	if (state != 3) return 0; // (card game / unknown: untouched)
	// in battle, stage loaded: the logic on the iterations whose BdLink is real (the hook bumps the phase first)
	return (ff8_bgate_phase + 1) % ff8_bgate_n == 0 ? 0 : 2;
}

static void __cdecl ff8_bgate_battleloop_data_only()
{
	((int (__cdecl *)())0x48D0C0)();  // j_BattleFile_PollOneCallback (load countdowns: real-bound only, see the hook)
	((int (__cdecl *)())0x500900)();  // BattleRender_UpdateTasksAndSubmitFrame = BdLink (ff8_bgate_bdlink_hook)
}

static __declspec(naked) void ff8_bgate_battleloop_thunk()
{
	__asm
	{
		push eax
		push ecx
		push edx
		call ff8_bgate_battleloop_mode
		cmp eax, 1
		pop edx
		pop ecx
		pop eax
		jb run
		je skip
		push eax
		push ecx
		push edx
		call ff8_bgate_battleloop_data_only
		pop edx
		pop ecx
		pop eax
	skip:
		ret
	run:
		jmp dword ptr [ff8_bgate_battleloop_orig]
	}
}

static int (__cdecl *ff8_bgate_textmgmt_orig)() = nullptr;

int __cdecl ff8_bgate_textmgmt_gate()
{
	if (ff8_bgate_n > 1 && (ff8_bgate_phase + 1) % ff8_bgate_n != 0)
		return 0; // (the caller ignores the return)
	return ff8_bgate_textmgmt_orig();
}

// Hit-effect queue (ExecuteTaskQueue(dword_1D96AA0) @0x500923, TASK_QUEUE 0x209FAA8): fed by
// CreateCameraShakeTask, DispatchEffectCommand, CreateEffectTask* (weapon/monster impact
// effects), the 99/B1 footstep particles, Renzokuken (MAG_141/159/160/161), MAG_332..343.
// Vanilla ticked it once per host frame -> all of that ran 2x at 30fps. Same treatment as
// the magic effect tree: native ticks, extrapolated draws on held frames.
static int (__cdecl *ff8_bgate_eq_tick_orig)(void *) = nullptr;

int __cdecl ff8_bgate_eq_tick_gate(void *queue)
{
	ff8_bgate_R = &ff8_bgate_rec_eq;
	const int prev_r = ff8_bgate_rec_eq.last_r;
	int r = ff8_bgate_gate_tick(queue, ff8_bgate_eq_tick_orig, ff8_bgate_rec_eq.last_r);
	ff8_bgate_R = &ff8_bgate_rec_fx;
	// the busy flag (g_HitEffectQueueBusy = this return) falls when the queue empties: the real frame of that tick still
	// reports the previous count, its held frames report 0, so the action sequences see the end on the next real frame,
	// as in vanilla (see ff8_bgate_fx_end_hold)
	if (r == 0 && prev_r != 0 && ff8_bgate_phase == 0 && !ff8_bgate_fx_bypass && ff8_bgate_n > 1)
		r = prev_r;
	// the magic effect tree's tail (its tick call, next in the frame, is skipped once the effect pointer is cleared)
	if (!ff8_bgate_fx_bypass)
	{
		if (ff8_bgate_phase == 0) ff8_bgate_tail_drop(&ff8_bgate_rec_fx);
		else ff8_bgate_tail_held(&ff8_bgate_rec_fx);
	}
	return r;
}

// --- camera shake offsets: hold the last real frame's values on held frames ---
// someUnknownBSCameraOperations (0x5033E0, from BdLink) adds the shake offsets
// (0x1D97710/12/14, int16 x/y/z) to the view translation and then ZEROES them - every
// producer (AnimSeq 96, hit shakes, magic effects) rewrites them each tick. With the
// producers gated to real frames the shake vanished on held frames. A held frame shows the
// in-between offsets (last real + (next - last) * phase / n) for every component whose next
// value is known: producers that can compute the next tick's value report it during the held
// frame (AnimSeq 96 below, the cinematic engine's prediction through ff8fx::held_shake_next);
// a component with no known next value re-applies the last real one.
static int (__cdecl *ff8_bgate_camops_orig)() = nullptr;
static uint32_t ff8_bgate_camops_ri = 0;
static int16_t ff8_bgate_shake_last[3] = {0, 0, 0};
static int16_t ff8_bgate_shake_prev[3] = {0, 0, 0}; // the offsets of the real tick before
static int16_t ff8_bgate_shake_next[3] = {0, 0, 0};
static unsigned ff8_bgate_shake_next_mask = 0;          // bit c = next value of component c known
static uint32_t ff8_bgate_shake_next_frame = 0xFFFFFFFF; // host frame the prediction belongs to
static uint32_t ff8_bgate_t96_end_frame = 0xFFFFFFFF;    // real frame on which an AnimSeq 96 shake ended

static void ff8_bgate_shake_report(int c, int16_t v)
{
	if (ff8_bgate_shake_next_frame != ff8_bgate_frame_no)
	{
		ff8_bgate_shake_next_frame = ff8_bgate_frame_no;
		ff8_bgate_shake_next_mask = 0;
	}
	ff8_bgate_shake_next[c] = v;
	ff8_bgate_shake_next_mask |= 1u << c;
}

namespace ff8fx
{
	// the next real tick's shake offsets as an effect's prediction computed them: written
	// components carry their value, the others are left to the producers that ran before the
	// effect in the frame (AnimSeq tasks), or 0 (the view build clears the offsets every frame)
	void held_shake_next(const int16_t next[3], unsigned written)
	{
		if (ff8_bgate_phase == 0) return;
		for (int c = 0; c < 3; c++)
		{
			if (written & (1u << c)) ff8_bgate_shake_report(c, next[c]);
			else if (ff8_bgate_shake_next_frame != ff8_bgate_frame_no || !(ff8_bgate_shake_next_mask & (1u << c)))
				ff8_bgate_shake_report(c, 0);
		}
	}
}

int __cdecl ff8_bgate_camops_hook()
{
	int16_t *s = (int16_t *)0x1D97710;
	const bool camdbg_valid = ff8_bgate_camdbg.valid;
	ff8_bgate_camdbg.valid = false;
	// the view built now is the NEXT frame's (see ff8_bgate_updatecam_hook): the offsets follow the
	// camera words - the frame of phase p leaves lerp(S[N-1], S[N], (p+1)/n), S[N] = the offsets
	// the producers wrote during the last real tick (the view build zeroes them every frame); an
	// offset a producer wrote on a held frame itself is kept
	if (ff8_bgate_phase == 0)
	{
		memcpy(ff8_bgate_shake_prev, ff8_bgate_shake_last, sizeof(ff8_bgate_shake_prev));
		memcpy(ff8_bgate_shake_last, s, sizeof(ff8_bgate_shake_last));
	}
	if (ff8_bgate_phase == 0 || (s[0] == 0 && s[1] == 0 && s[2] == 0))
		for (int c = 0; c < 3; c++)
			s[c] = (int16_t)(ff8_bgate_shake_prev[c] + ff8_bgate_scale_round(ff8_bgate_shake_last[c] - ff8_bgate_shake_prev[c], ff8_bgate_phase + 1, ff8_bgate_n));
	if (ff8_battle_fx_held_check && ff8_bgate_fxv_live && camdbg_valid && ff8_bgate_camdbg_lines < 1500)
	{
		const int16_t *e = (const int16_t *)0xB8B7F0, *pr = (const int16_t *)ff8_bgate_camdbg.pre, *en = (const int16_t *)ff8_bgate_camdbg.eng;
		ff8_bgate_camdbg_lines++;
		ffnx_info("30fps camdbg: f=%u ph=%d path=%s pre=%d,%d,%d/%d,%d,%d eng=%d,%d,%d/%d,%d,%d final=%d,%d,%d/%d,%d,%d shake=%d,%d,%d%s\n",
			ff8_bgate_frame_no, ff8_bgate_phase, ff8_bgate_camdbg.path,
			pr[0], pr[1], pr[2], pr[4], pr[5], pr[6], en[0], en[1], en[2], en[4], en[5], en[6],
			e[0], e[1], e[2], e[4], e[5], e[6], s[0], s[1], s[2],
			(ff8_bgate_phase == 0) ? "" : " (held)");
	}
	unreplace_function(ff8_bgate_camops_ri);
	int r = ff8_bgate_camops_orig();
	rereplace_function(ff8_bgate_camops_ri);
	return r;
}

// --- AnimSeq-spawned tasks (TASK_QUEUE_ANIM_SEQ 0x1D986B8, ticked every host frame by
// ExecuteTaskQueue(dword_1D96A8C) @0x500917). Tick fns are DWORD __cdecl f(node*),
// 0 = keep running, 2 = remove. Node task data starts at +0x0C. ---
#define FF8_BGATE_TASK_HOOK(name, addr) \
	static DWORD (__cdecl *ff8_bgate_##name##_orig)(uint8_t *) = (DWORD (__cdecl *)(uint8_t *))(addr); \
	static uint32_t ff8_bgate_##name##_ri = 0; \
	static DWORD ff8_bgate_##name##_call(uint8_t *node) \
	{ \
		unreplace_function(ff8_bgate_##name##_ri); \
		DWORD r = ff8_bgate_##name##_orig(node); \
		rereplace_function(ff8_bgate_##name##_ri); \
		return r; \
	}

// --- VM tasks: AddTaskToQueueAnimSeq (0x506C10) is the single creator of AnimSeq-queue tasks ---
// A task created while an entity's choreography VM runs (ff8_bgate_in_vm) is a VM task: its
// duration / stamps come from converted data (84 duration, 9F tick stamps, AD/AE frames x2 by
// FF8GameData/dat/seqrewrite30.py), so it ticks every data tick. Any other creator (and a node
// recycled by one) is untagged.
#define FF8_BGATE_VMTASK_MAX 64
static void *ff8_bgate_vmtask[FF8_BGATE_VMTASK_MAX];
static void *(__cdecl *ff8_bgate_addtask_orig)(void *) = nullptr;
static uint32_t ff8_bgate_addtask_ri = 0;

static void ff8_bgate_vmtask_set(void *node, bool vm)
{
	int free_slot = -1;
	for (int i = 0; i < FF8_BGATE_VMTASK_MAX; i++)
	{
		if (ff8_bgate_vmtask[i] == node) ff8_bgate_vmtask[i] = nullptr;
		if (!ff8_bgate_vmtask[i] && free_slot < 0) free_slot = i;
	}
	if (vm && node && free_slot >= 0) ff8_bgate_vmtask[free_slot] = node;
}

static bool ff8_bgate_vmtask_is(const void *node)
{
	for (int i = 0; i < FF8_BGATE_VMTASK_MAX; i++)
		if (ff8_bgate_vmtask[i] == node) return true;
	return false;
}

void *__cdecl ff8_bgate_addtask_hook(void *callback)
{
	unreplace_function(ff8_bgate_addtask_ri);
	void *node = ff8_bgate_addtask_orig(callback);
	rereplace_function(ff8_bgate_addtask_ri);
	ff8_bgate_vmtask_set(node, ff8_bgate_in_vm);
	return node;
}

// a VM task ticks on every data tick; a tick that removes it (2) untags the node
#define FF8_BGATE_VMTASK_TICK(name, n) \
	if (ff8_bgate_vmtask_is(n)) \
	{ \
		if (!ff8_bgate_data_tick()) return 0; \
		DWORD r_ = ff8_bgate_##name##_call(n); \
		if (r_ == 2) ff8_bgate_vmtask_set(n, false); \
		return r_; \
	}

// Code-paced tasks (converted data leaves their parameters at vanilla, seqrewrite30.py: "96, 99/B1,
// 81, A6 and A8 run hardcoded 15-tick tasks that FFNx keeps at 15 Hz"): skipping held frames = native
// pace. 99/B1 footstep dust + step sounds (0x50F830), 96 camera shake (0x50F6C0: amplitudes and
// duration from the opcode, left unconverted; its offset is held by the camera hook above).
// Data-paced VM tasks: 84 sine wobble of a stage group (0x501F90), 9F texture toggle timeline
// (0x5057D0), AD/AE drag (0x50F500) - every data tick (FF8_BGATE_VMTASK_TICK); untagged ones keep the
// former 15 Hz pacing below.
FF8_BGATE_TASK_HOOK(t84, 0x501F90)
FF8_BGATE_TASK_HOOK(t9f, 0x5057D0)
FF8_BGATE_TASK_HOOK(tstep, 0x50F830)
FF8_BGATE_TASK_HOOK(t96, 0x50F6C0)
// 84 is a pure time ratio (sin((4096 - counter*4096/duration) / 4) into the 4 battle entity
// records +6, then counter++): the held frame writes the same formula at counter - 1 +
// phase/n, the next real tick writes the vanilla value again.
DWORD __cdecl ff8_bgate_t84_hook(uint8_t *n)
{
	FF8_BGATE_VMTASK_TICK(t84, n)
	if (!ff8_bgate_phase) return ff8_bgate_t84_call(n);
	int32_t c = *(int16_t *)(n + 0x0C), d = *(int16_t *)(n + 0x0E);
	if (d <= 0) return 0;
	int32_t q = (((c - 1) * ff8_bgate_n + ff8_bgate_phase) << 12) / (d * ff8_bgate_n);
	int16_t v = (int16_t)((int32_t(__cdecl *)(int32_t))0x56D130)((0x1000 - q) / 4);
	for (uint32_t a = 0x1D98992; a < 0x1D98A42; a += 0x2C) *(int16_t *)a = v;
	return 0;
}
DWORD __cdecl ff8_bgate_t9f_hook(uint8_t *n)
{
	FF8_BGATE_VMTASK_TICK(t9f, n)
	return ff8_bgate_phase ? 0 : ff8_bgate_t9f_call(n);
}
DWORD __cdecl ff8_bgate_tstep_hook(uint8_t *n) { return ff8_bgate_phase ? 0 : ff8_bgate_tstep_call(n); }
// Effect texture restore (BattleTask_LoadDefaultEffectTexture 0x508660, queued by Battle_RestoreDefaultEffectTexture
// 0x508630 from effect code: Fira's WaitTexRestore 0x61DE10, Siren, gfc VM 0x07B...): 3 states, one per call (load the
// default effect texture file, upload its TIM, set the done flag the effect waits for). Ticked every host frame, the
// flag came ~2 ticks early and every effect ending on that wait ended early (Fira: invisible last ticks, the action
// ended ~0.13 s before vanilla). Real frames only = one state per vanilla tick.
FF8_BGATE_TASK_HOOK(ttexrestore, 0x508660)
DWORD __cdecl ff8_bgate_ttexrestore_hook(uint8_t *n) { return ff8_bgate_phase ? 0 : ff8_bgate_ttexrestore_call(n); }
// Victory fanfare task (BS_R0win_PlayVictoryFanfareTask 0x501A70, AnimSeq queue, queued by the r0win win sequence): 3
// code states, one per call (music command 1, copy the AKAO fanfare + music command 2, raise the completion flag the
// win sequence waits on before the poses and the victory camera). Real frames only = one state per vanilla tick.
FF8_BGATE_TASK_HOOK(tfanfare, 0x501A70)
DWORD __cdecl ff8_bgate_tfanfare_hook(uint8_t *n) { return ff8_bgate_phase ? 0 : ff8_bgate_tfanfare_call(n); }

// Code timers that DRAW on every call: on a held frame they draw again without advancing (skipping them would
// make the picture vanish every other frame). Their durations are hardcoded or come from code (none from
// converted data), counted in vanilla ticks.
// - Battle text box task (processTextTask 0x506F70, TASK_QUEUE_PRINT, ticked by BdLink every host frame): +0x0E
//   text_speed = display ticks left (8 * combatMsgSpeed + 8 for the action captions, "Escaped", "Received X",
//   the AnimSeq text ops...), decremented while its channel is free, the task ends at 0. Each call also claims the
//   channel the text renderer shows. Held frame: the same call with the counter put back (never ends there).
FF8_BGATE_TASK_HOOK(ttext, 0x506F70)
DWORD __cdecl ff8_bgate_ttext_hook(uint8_t *n)
{
	if (!ff8_bgate_phase) return ff8_bgate_ttext_call(n);
	const uint8_t speed = n[0x0E];
	if (speed == 0) n[0x0E] = 1; // (a held call must not end it: the next real frame does)
	DWORD r = ff8_bgate_ttext_call(n);
	n[0x0E] = speed;
	return r & ~2u;
}
// - Screen fades (FF8BattleFadeTask: +0x0C elapsed, +0x0E duration, +0x10 start level, +0x11 end level; AnimSeq
//   queue): BattleFade_UpdateBlackOverlay 0x501D10 (battle start from black 15 / BattleStage_StartCameraAndRenderTask,
//   end of battle to black 13: system tasks 3 / 4) and BattleFade_UpdateWhiteOverlay 0x501E80 (GF cinematic VM op
//   0x08A). Each call draws level(elapsed) = start + (end - start) * ((elapsed << 12) / duration >> 12), linear per
//   tick, then advances elapsed (black: ends after drawing elapsed == duration; white: holds once elapsed >= duration).
//   Real frames run them as vanilla and remember the elapsed each one drew (e0). Held frame k of n: the same call with
//   elapsed = e0 * n + k and duration * n, i.e. the level k/n of the way from the last real draw (e0) to the next one
//   (e0 + 1), then both fields are put back and an end is ignored (the next real frame ends it, on the vanilla tick).
//   Never past the end level: a black fade's last real draw IS the end (the task is gone on the held frame after it,
//   as on vanilla's next tick); a white fade whose last real draw held at elapsed >= duration holds again. A fade not
//   drawn on a real frame yet (queued on a held frame) draws its level 0 without advancing. The black fade's countdown
//   enable at elapsed == 5 (idempotent) is kept off the held calls (elapsed 5 scaled to 10 / duration x 2).
FF8_BGATE_TASK_HOOK(tfadeblack, 0x501D10)
FF8_BGATE_TASK_HOOK(tfadewhite, 0x501E80)
static struct { uint8_t *node; int16_t e0; } ff8_bgate_fade_last[8]; // elapsed of each fade's last real draw

static int ff8_bgate_fade_slot(uint8_t *n, bool add)
{
	int free_slot = -1;
	for (int i = 0; i < 8; i++)
	{
		if (ff8_bgate_fade_last[i].node == n) return i;
		if (!ff8_bgate_fade_last[i].node && free_slot < 0) free_slot = i;
	}
	if (add && free_slot >= 0) { ff8_bgate_fade_last[free_slot].node = n; return free_slot; }
	return -1;
}

static DWORD ff8_bgate_fade_call(uint8_t *n, bool white)
{
	return white ? ff8_bgate_tfadewhite_call(n) : ff8_bgate_tfadeblack_call(n);
}

static DWORD ff8_bgate_fade_real(uint8_t *n, bool white)
{
	const int16_t e0 = *(int16_t *)(n + 0x0C);
	DWORD r = ff8_bgate_fade_call(n, white);
	const int s = ff8_bgate_fade_slot(n, r != 2);
	if (s >= 0)
	{
		if (r == 2) ff8_bgate_fade_last[s].node = nullptr; // ended (the node goes back to the pool)
		else ff8_bgate_fade_last[s].e0 = e0;
	}
	return r;
}

static DWORD ff8_bgate_fade_held(uint8_t *n, bool white)
{
	int16_t &elapsed = *(int16_t *)(n + 0x0C), &duration = *(int16_t *)(n + 0x0E);
	const int16_t e = elapsed, d = duration;
	int s = ff8_bgate_fade_slot(n, false);
	if (d <= 0) return 0;
	// the remembered draw must be this fade's last one (a node reused by a new fade: no previous level)
	if (s >= 0 && !(e == ff8_bgate_fade_last[s].e0 + 1 || (e == ff8_bgate_fade_last[s].e0 && e >= d))) s = -1;
	if (s < 0)
	{
		// not drawn on a real frame yet: its first level, no advance (never an end on a held frame)
		if (white ? (e < d && e + 1 >= d && n[0x11] == 0) : e >= d) return 0; // (this call would end it)
		ff8_bgate_fade_call(n, white);
		elapsed = e;
		return 0;
	}
	const int16_t e0 = ff8_bgate_fade_last[s].e0;
	int32_t ne, nd;
	if (e0 >= d) { ne = e0; nd = d; } // (white holding at its end level: holds)
	else
	{
		ne = (int32_t)e0 * ff8_bgate_n + ff8_bgate_phase; // < d * n: never past the next real draw
		nd = (int32_t)d * ff8_bgate_n;
		if (!white && ne == 5) { ne *= 2; nd *= 2; } // (keep the elapsed == 5 countdown enable to the real tick)
	}
	if (nd > 0x7FFF || ne > 0x7FFF) { ne = e0; nd = d; } // (out of the int16 fields: hold)
	elapsed = (int16_t)ne;
	duration = (int16_t)nd;
	ff8_bgate_fade_call(n, white);
	elapsed = e;
	duration = d;
	return 0;
}
DWORD __cdecl ff8_bgate_tfadeblack_hook(uint8_t *n) { return ff8_bgate_phase ? ff8_bgate_fade_held(n, false) : ff8_bgate_fade_real(n, false); }
DWORD __cdecl ff8_bgate_tfadewhite_hook(uint8_t *n) { return ff8_bgate_phase ? ff8_bgate_fade_held(n, true) : ff8_bgate_fade_real(n, true); }
// 96 (0x50F6C0): Y shake = start + (end - start) * i / duration, negated on odd i, i++; ends
// (returns 2) once i reaches the duration. Held frame: report the value the next real tick
// writes (the node's i is already the next index).
DWORD __cdecl ff8_bgate_t96_hook(uint8_t *n)
{
	if (!ff8_bgate_phase)
	{
		DWORD r = ff8_bgate_t96_call(n);
		if (r == 2) ff8_bgate_t96_end_frame = ff8_bgate_frame_no;
		return r;
	}
	const int16_t start = *(int16_t *)(n + 0x0C), end = *(int16_t *)(n + 0x0E);
	const int16_t dur = *(int16_t *)(n + 0x10), i = *(int16_t *)(n + 0x12);
	if (dur != 0)
	{
		int16_t v = (int16_t)(((((int32_t)i << 12) / dur) * (end - start) >> 12) + start);
		ff8_bgate_shake_report(1, (i & 1) ? (int16_t)-v : v);
	}
	return 0;
}

// AD/AE drag target to attacker bone (0x50F500): snaps the target to the bone EVERY tick
// (so it keeps following the smoothly animated attacker) - only its frames-left dword
// (+0x18) must count at native rate: restore it after held-frame ticks, and never let a
// held tick be the one that ends the task.
FF8_BGATE_TASK_HOOK(tdrag, 0x50F500)
DWORD __cdecl ff8_bgate_tdrag_hook(uint8_t *n)
{
	if (ff8_bgate_vmtask_is(n) && ff8_bgate_data_tick())
	{
		// VM task (AD/AE frames converted x2): every data tick
		DWORD r = ff8_bgate_tdrag_call(n);
		if (r == 2) ff8_bgate_vmtask_set(n, false);
		return r;
	}
	if (ff8_bgate_phase == 0)
		return ff8_bgate_tdrag_call(n);
	int32_t left = *(int32_t *)(n + 0x18);
	if (left == 1)
		return 0; // this tick would finish it: leave that to the next real frame
	DWORD r = ff8_bgate_tdrag_call(n);
	*(int32_t *)(n + 0x18) = left;
	return r;
}

// 81 restore model part (0x50F0E0): draws the fading part INSIDE the tick, so it must run
// every frame; its fade/lifetime counter (+0x13, 15 ticks) only advances on real frames.
FF8_BGATE_TASK_HOOK(t81, 0x50F0E0)
DWORD __cdecl ff8_bgate_t81_hook(uint8_t *n)
{
	if (ff8_bgate_phase == 0)
		return ff8_bgate_t81_call(n);
	uint8_t state = n[0x12], counter = n[0x13];
	ff8_bgate_t81_call(n);
	n[0x13] = (state == 0) ? 0 : counter; // first tick: init only, count from the next real one
	return 0;
}

// A6 detached model part with physics (0x50F2E0): also draws inside the tick. Its whole
// simulation state is the node (+0x0C..+0x2B: state, 15-tick counter, velocities, spin)
// plus the global DETACHED_PART_SAVED_MATRIX (0x1D99BF8, 32 B incl. position) from which
// the bone matrices are rebuilt every tick - so a held tick is run and then fully undone.
// TRUE 30 FPS: the task draws, THEN integrates (rotate the saved matrix by the spin +0x20,
// position += velocity +0x14/16/18, drag, gravity). The real tick's pre-update state is kept;
// a held frame draws from it with half the spin and half the velocity applied (physics skipped
// through battle_to_update_flags bit 0 for that one call), then the real state is put back.
FF8_BGATE_TASK_HOOK(ta6, 0x50F2E0)
static struct { uint8_t *node; uint8_t state[0x20], mtx[0x20]; bool valid; } ff8_bgate_a6_memo;
DWORD __cdecl ff8_bgate_ta6_hook(uint8_t *n)
{
	if (ff8_bgate_phase == 0)
	{
		ff8_bgate_a6_memo.node = n;
		memcpy(ff8_bgate_a6_memo.state, n + 0x0C, 0x20);
		memcpy(ff8_bgate_a6_memo.mtx, (void *)0x1D99BF8, 0x20);
		ff8_bgate_a6_memo.valid = n[0x12] != 0; // initialised (the first tick copies the bone matrix)
		return ff8_bgate_ta6_call(n);
	}
	if (ff8_bgate_a6_memo.valid && ff8_bgate_a6_memo.node == n && n[0x12] != 0)
	{
		uint8_t node_cur[0x20], mtx_cur[0x20];
		memcpy(node_cur, n + 0x0C, sizeof(node_cur));
		memcpy(mtx_cur, (void *)0x1D99BF8, sizeof(mtx_cur));
		const uint8_t *s = ff8_bgate_a6_memo.state; // node +0x0C..
		memcpy((void *)0x1D99BF8, ff8_bgate_a6_memo.mtx, 0x20);
		int16_t spin = *(const int16_t *)(s + 0x14);
		((void (__cdecl *)(int32_t, void *))0x56CFB0)(spin * ff8_bgate_phase / ff8_bgate_n, (void *)0x1D99BF8); // ApplyXRotation
		int32_t *pos = (int32_t *)0x1D99C0C;
		for (int i = 0; i < 3; i++)
			pos[i] += *(const int16_t *)(s + 0x08 + 2 * i) * ff8_bgate_phase / ff8_bgate_n;
		memcpy(n + 0x0C, s, 0x20); // the counter (fade) and state the real tick drew with
		uint32_t flags = *(uint32_t *)0x1D96A9C;
		*(uint32_t *)0x1D96A9C = flags | 1;
		ff8_bgate_ta6_call(n);
		*(uint32_t *)0x1D96A9C = flags;
		memcpy(n + 0x0C, node_cur, sizeof(node_cur));
		memcpy((void *)0x1D99BF8, mtx_cur, sizeof(mtx_cur));
		return 0;
	}
	uint8_t node_save[0x20], mtx_save[0x20];
	memcpy(node_save, n + 0x0C, sizeof(node_save));
	memcpy(mtx_save, (void *)0x1D99BF8, sizeof(mtx_save));
	ff8_bgate_ta6_call(n);
	memcpy(n + 0x0C, node_save, sizeof(node_save));
	memcpy((void *)0x1D99BF8, mtx_save, sizeof(mtx_save));
	return 0;
}

// --- status-effect timers + Gilgamesh/Angelo countdown: native rate ---
// FFBattleDirector_battleLoop (0x47CCB0) calls computeTimerStatus (0x483470) and
// summonGilgaAngelStartFight (0x482F80) once per LOOP ITERATION = once per host frame:
//  - computeTimerStatus: *timer -= speed (1/2/3 by Slow/normal/Haste) for every status
//    timer (regen/poison/doom/petrify/stop/sleep/shell/protect/reflect...), fires the
//    Regen heal each 60/speed, handles expiry -> every timed status lasted HALF as long;
//  - summonGilgaAngelStartFight: --DEAD_TIMER_TO_SUMMON_GILGA, and every frame it sits
//    at 0 rolls Gilgamesh (12/255) and the Angelo auto-actions -> twice as frequent.
// Both are pure per-tick counters with no cross-frame handshake, so running them on
// real frames only reproduces vanilla exactly. (The old 60fps build disabled the timer
// gate after an Odin crash that was never isolated; the camera-VM gate active in the
// same build - since replaced by the safe camera task gate - is the likelier culprit.)
static void (__cdecl *ff8_bgate_timerstatus_orig)() = nullptr;
static uint32_t ff8_bgate_timerstatus_ri = 0;
static void (__cdecl *ff8_bgate_gilga_orig)() = nullptr;
static uint32_t ff8_bgate_gilga_ri = 0;

void __cdecl ff8_bgate_gilga_hook()
{
	if (ff8_bgate_phase != 0)
		return;
	unreplace_function(ff8_bgate_gilga_ri);
	ff8_bgate_gilga_orig();
	rereplace_function(ff8_bgate_gilga_ri);
}

void __cdecl ff8_bgate_timerstatus_hook()
{
	if (ff8_bgate_phase != 0)
		return;
	unreplace_function(ff8_bgate_timerstatus_ri);
	ff8_bgate_timerstatus_orig();
	rereplace_function(ff8_bgate_timerstatus_ri);
}

// --- entity task visuals at native pace ---
// The per-entity task (pre_pre_linkedToAnimationSequence 0x502AB0) runs every host frame - only its
// AnimSeq VM is gated (ff8_bgate_animseq_upd_hook) - and its visuals count calls, not time: the status
// colour pulse, Confuse spin and Float bob (BattleEntity_UpdateStatusVisuals 0x50A0A0), the status
// sprites over the head (0x5711C0) and the A8 fades (BattleEntity_UpdateVisibilityFade 0x50C110) all
// ran 2x fast. Their counters now advance on real frames only. A held frame draws the half-way state of
// what is continuous (colour, angle, height, fade, drop) and the real frame's state of what is discrete
// (sprite frames, the random flicker, the step that ends a fade).
#define FF8_BGATE_VIS_SLOTS 16
struct ff8_bgate_vis_t
{
	void *slot;
	uint32_t fade_frame, ovl_frame;  // real frame (ff8_bgate_frame_no) of the records below
	uint8_t fade_mode, fade_counter; // A8 fade mode, and the counter its step read on that frame
	bool fade_wrote;                 // a held frame wrote fade_written over fade_pos (mode 9 drop)
	int16_t fade_pos[2], fade_written[2];
	uint32_t ovl_valid;             // bit i: ovl_in[i] / ovl_out[i] recorded on that frame
	uint8_t ovl_in[32], ovl_out[32]; // status sprite frame drawn / stored, by status bit
};
static ff8_bgate_vis_t ff8_bgate_vis[FF8_BGATE_VIS_SLOTS];

static ff8_bgate_vis_t *ff8_bgate_vis_get(void *slot)
{
	ff8_bgate_vis_t *free_slot = nullptr;
	for (int i = 0; i < FF8_BGATE_VIS_SLOTS; i++)
	{
		if (ff8_bgate_vis[i].slot == slot) return &ff8_bgate_vis[i];
		if (!ff8_bgate_vis[i].slot && !free_slot) free_slot = &ff8_bgate_vis[i];
	}
	if (!free_slot) return nullptr;
	*free_slot = {};
	free_slot->slot = slot;
	return free_slot;
}

// the real frame a held frame belongs to
static inline uint32_t ff8_bgate_real_frame_no() { return ff8_bgate_frame_no - ff8_bgate_phase; }

// FF8BattleEntitySlotData.pointer_unk2 (+0x88): 32 per-status visual counters, indexed by status bit
#define FF8_BGATE_VIS_COUNTERS(slot) (*(uint8_t **)((uint8_t *)(slot) + 0x88))

// BattleEntity_UpdateStatusPalettePulse (0x50A1C0): model_palette pulses with the sine of counter[k]++
// (k = 10/17/18/25 by status, 32 steps per cycle). Held frame: the colour of the next real frame, then
// the one of the real frame (counter - 1), the frame shows their mix at phase/n; the counter is put back.
static int (__cdecl *ff8_bgate_pulse_orig)(uint8_t *) = nullptr;
static uint32_t ff8_bgate_pulse_ri = 0;

static int ff8_bgate_pulse_call(uint8_t *slot)
{
	unreplace_function(ff8_bgate_pulse_ri);
	int r = ff8_bgate_pulse_orig(slot);
	rereplace_function(ff8_bgate_pulse_ri);
	return r;
}

int __cdecl ff8_bgate_pulse_hook(uint8_t *slot)
{
	uint8_t *cnt = FF8_BGATE_VIS_COUNTERS(slot);
	if (ff8_bgate_phase == 0 || !cnt)
		return ff8_bgate_pulse_call(slot);
	uint8_t saved[32];
	for (int i = 0; i < 32; i++) saved[i] = cnt[i];
	int r = ff8_bgate_pulse_call(slot); // the next real frame's colour
	int k = -1;
	for (int i = 0; i < 32 && k < 0; i++)
		if (cnt[i] != saved[i]) k = i;
	if (k >= 0)
	{
		const uint8_t next[3] = { slot[0x28], slot[0x29], slot[0x2A] }; // model_palette rgb
		for (int i = 0; i < 32; i++) cnt[i] = saved[i];
		cnt[k] = (uint8_t)(saved[k] - 1);
		ff8_bgate_pulse_call(slot); // the real frame's colour
		for (int c = 0; c < 3; c++)
			slot[0x28 + c] = (uint8_t)(slot[0x28 + c] + ff8_bgate_scale_round(next[c] - slot[0x28 + c], ff8_bgate_phase, ff8_bgate_n));
	}
	for (int i = 0; i < 32; i++) cnt[i] = saved[i];
	return r;
}

// BattleEntity_ApplyTurnBackOrConfuseSpin (0x50A370): Confuse turns the model by counter[11]++ * 0x100
// (16 steps per turn, multiplied into current_z_scale before the render). Held frame: the real frame's
// angle (counter - 1) plus phase/n of a step; the counter is put back.
static void (__cdecl *ff8_bgate_spin_orig)(uint8_t *) = nullptr;
static uint32_t ff8_bgate_spin_ri = 0;

void __cdecl ff8_bgate_spin_hook(uint8_t *slot)
{
	uint8_t *cnt = FF8_BGATE_VIS_COUNTERS(slot);
	const bool held = ff8_bgate_phase != 0 && cnt;
	const uint8_t c = cnt ? cnt[11] : 0;
	if (held)
		cnt[11] = (uint8_t)(c - 1);
	unreplace_function(ff8_bgate_spin_ri);
	ff8_bgate_spin_orig(slot);
	rereplace_function(ff8_bgate_spin_ri);
	if (held)
	{
		if (cnt[11] == c) // the Confuse spin ran
			((void *(__cdecl *)(int32_t, void *))0x56D020)(0x100 * ff8_bgate_phase / ff8_bgate_n, slot + 0x40); // ApplyYRotation
		cnt[11] = c;
	}
}

// BattleEntity_UpdateFloatBob (0x50A5F0): Float height shadow_z_maybe (+0x22) = (-100 * ((sin(counter[9]++
// * 0x100) + 0x1000) / 2) >> 12) - 200, 16 steps per cycle. Held frame: the same formula at the real
// frame's counter + phase/n; the counter is put back.
static void (__cdecl *ff8_bgate_bob_orig)(uint8_t *) = nullptr;
static uint32_t ff8_bgate_bob_ri = 0;

void __cdecl ff8_bgate_bob_hook(uint8_t *slot)
{
	uint8_t *cnt = FF8_BGATE_VIS_COUNTERS(slot);
	const uint8_t c = cnt ? cnt[9] : 0;
	unreplace_function(ff8_bgate_bob_ri);
	ff8_bgate_bob_orig(slot);
	rereplace_function(ff8_bgate_bob_ri);
	if (ff8_bgate_phase != 0 && cnt && cnt[9] != c) // held frame, and the bob ran
	{
		const int32_t a = (((int32_t)(uint8_t)(c - 1) * ff8_bgate_n + ff8_bgate_phase) << 8) / ff8_bgate_n;
		const int32_t s = (int32_t)((uint32_t (__cdecl *)(int32_t))0x56D130)(a); // computeSin
		*(int16_t *)(slot + 0x22) = (int16_t)(((-100 * ((s + 0x1000) / 2)) >> 12) - 200);
		cnt[9] = c;
	}
}

// sub_5711C0: the status sprite over the head (status bits 5-8 and 12). counter[i] = this(slot, i,
// counter[i]) draws sprite frame counter[i] and returns the next one (+1 per call, 0 at the end).
// Held frame: the real frame's sprite frame is drawn again and the counter keeps its value.
static int (__cdecl *ff8_bgate_ovl_orig)(uint8_t *, int, int) = nullptr;
static uint32_t ff8_bgate_ovl_ri = 0;

static int ff8_bgate_ovl_call(uint8_t *slot, int i, int frame)
{
	unreplace_function(ff8_bgate_ovl_ri);
	int r = ff8_bgate_ovl_orig(slot, i, frame);
	rereplace_function(ff8_bgate_ovl_ri);
	return r;
}

int __cdecl ff8_bgate_ovl_hook(uint8_t *slot, int i, int frame)
{
	ff8_bgate_vis_t *v = (i >= 0 && i < 32) ? ff8_bgate_vis_get(slot) : nullptr;
	if (ff8_bgate_phase == 0)
	{
		int r = ff8_bgate_ovl_call(slot, i, frame);
		if (v)
		{
			if (v->ovl_frame != ff8_bgate_frame_no)
			{
				v->ovl_frame = ff8_bgate_frame_no;
				v->ovl_valid = 0;
			}
			v->ovl_in[i] = (uint8_t)frame;
			v->ovl_out[i] = (uint8_t)r;
			v->ovl_valid |= 1u << i;
		}
		return r;
	}
	if (v && v->ovl_frame == ff8_bgate_real_frame_no() && (v->ovl_valid & (1u << i)) && v->ovl_out[i] == (uint8_t)frame)
		ff8_bgate_ovl_call(slot, i, v->ovl_in[i]);
	else
		ff8_bgate_ovl_call(slot, i, frame); // changed since the real frame: draw what it holds now
	return frame;
}

// BattleEntity_UpdateVisibilityFade (0x50C110): the A8 fades. Each step function counts its own calls
// (a8_fade_counter +6: 15 steps, the fade-ins count 2 per call) and rebuilds the colours (model and
// shadow palette, fade level +7) - mode 9 the drop position - from that counter. Held frame: the step
// runs on the live state for the next real frame's counter and for the counter the real frame read,
// the frame shows the mix of the two at phase/n, and the rest of the state is put back. The step that
// ends a fade keeps the real frame's look (the switch happens on the real frame), mode 8 (random white
// flicker) holds the real frame's flash, mode 5 never releases the model here (only its fade runs).
struct ff8_bgate_fade_t { uint8_t head[8]; int16_t pos[2]; uint8_t pal[8]; };

static void ff8_bgate_fade_read(const uint8_t *s, ff8_bgate_fade_t &f)
{
	for (int i = 0; i < 8; i++) f.head[i] = s[i]; // flags, combat flags, com_id, mode, counter, fade level
	f.pos[0] = *(const int16_t *)(s + 0x1E); // based_position_y / x (mode 9)
	f.pos[1] = *(const int16_t *)(s + 0x20);
	for (int i = 0; i < 8; i++) f.pal[i] = s[0x28 + i]; // model_palette, shadow_palette
}

static void ff8_bgate_fade_write(uint8_t *s, const ff8_bgate_fade_t &f)
{
	for (int i = 0; i < 8; i++) s[i] = f.head[i];
	*(int16_t *)(s + 0x1E) = f.pos[0];
	*(int16_t *)(s + 0x20) = f.pos[1];
	for (int i = 0; i < 8; i++) s[0x28 + i] = f.pal[i];
}

static uint32_t ff8_bgate_fade_step(uint8_t mode)
{
	switch (mode)
	{
	case 1: case 2: return 0x50C230;         // A8Fade_Mode1_2_FadeInSine
	case 3: case 5: case 6: return 0x50C410; // A8Fade_Mode3_6_FadeOutCosine (also the fade of mode 5)
	case 4: case 7: return 0x50C560;         // A8Fade_Mode4_7_FadeOutToColor
	case 9: return 0x50C6D0;                 // A8Fade_Mode9_DelayedDrop
	case 10: case 12: return 0x50C330;       // A8Fade_Mode10_12_FadeInLinear
	case 11: case 13: return 0x50C4D0;       // A8Fade_Mode11_13_FadeOutLinear
	}
	return 0;
}

static int (__cdecl *ff8_bgate_fade_orig)(uint8_t *) = nullptr;
static uint32_t ff8_bgate_fade_ri = 0;

int __cdecl ff8_bgate_fade_hook(uint8_t *slot)
{
	ff8_bgate_vis_t *v = ff8_bgate_vis_get(slot);
	const uint8_t mode = slot[5], counter = slot[6];
	if (v && v->fade_wrote)
	{
		// the position a held frame drew goes back to the true one, unless something else moved it since
		int16_t *pos = (int16_t *)(slot + 0x1E);
		if (pos[0] == v->fade_written[0] && pos[1] == v->fade_written[1])
		{
			pos[0] = v->fade_pos[0];
			pos[1] = v->fade_pos[1];
		}
		v->fade_wrote = false;
	}
	if (ff8_bgate_phase == 0)
	{
		if (v)
		{
			v->fade_frame = ff8_bgate_frame_no;
			v->fade_mode = mode;
			v->fade_counter = (mode == 5 && counter == 0) ? 1 : counter; // mode 5 counts its first call before the fade step
		}
		unreplace_function(ff8_bgate_fade_ri);
		int r = ff8_bgate_fade_orig(slot);
		rereplace_function(ff8_bgate_fade_ri);
		return r;
	}
	const bool rec = v && v->fade_frame == ff8_bgate_real_frame_no() && v->fade_mode == mode;
	if (mode == 8)
	{
		// counter = frames to the next flash (re-rolled at 0), the flash shows while it is below 2
		if (rec && v->fade_counter < 2)
		{
			((void (__cdecl *)(int, int, int))0x45DDA0)(0xFF, 0xFF, 0xFF); // GTE far colour
			((void *(__cdecl *)(void *, int, void *))0x56CA00)(slot + 0x28, (v->fade_counter + 1) << 11, slot + 0x28); // model_palette toward it
			slot[0x2B] = 0;
		}
		return 0;
	}
	typedef void (__cdecl *step_t)(uint8_t *);
	const step_t step = (step_t)ff8_bgate_fade_step(mode);
	if (!step)
		return 0;
	ff8_bgate_fade_t s, a, b;
	ff8_bgate_fade_read(slot, s);
	if (mode == 5 && counter == 0)
		slot[6] = 1; // the next real frame's mode 5 counts its first call before the fade step
	step(slot); // the next real frame's step
	ff8_bgate_fade_read(slot, b);
	ff8_bgate_fade_write(slot, s);
	const bool ends = b.head[5] != mode;
	bool have_a = false;
	if (rec)
	{
		slot[6] = v->fade_counter;
		step(slot); // the real frame's step
		ff8_bgate_fade_read(slot, a);
		ff8_bgate_fade_write(slot, s);
		have_a = a.head[5] == mode; // else that step ended the fade and this one restarted after it
	}
	if (!have_a)
	{
		if (ends)
			return 0;
		a = b; // the fade started after the real frame's step: show its first step
	}
	ff8_bgate_fade_t o = s;
	o.head[7] = a.head[7];
	for (int i = 0; i < 2; i++) o.pos[i] = a.pos[i];
	for (int i = 0; i < 8; i++) o.pal[i] = a.pal[i];
	if (!ends)
	{
		o.head[7] = (uint8_t)(a.head[7] + ff8_bgate_scale_round(b.head[7] - a.head[7], ff8_bgate_phase, ff8_bgate_n));
		for (int i = 0; i < 2; i++)
			o.pos[i] = (int16_t)(a.pos[i] + ff8_bgate_scale_round(b.pos[i] - a.pos[i], ff8_bgate_phase, ff8_bgate_n));
		for (int i = 0; i < 8; i++)
			if (i != 3 && i != 7) // the transparency bytes stay the real frame's
				o.pal[i] = (uint8_t)(a.pal[i] + ff8_bgate_scale_round(b.pal[i] - a.pal[i], ff8_bgate_phase, ff8_bgate_n));
	}
	if (v && (o.pos[0] != s.pos[0] || o.pos[1] != s.pos[1]))
	{
		// mode 9 drop: the drawn position is put back on the next call (held or real)
		for (int i = 0; i < 2; i++)
		{
			v->fade_pos[i] = s.pos[i];
			v->fade_written[i] = o.pos[i];
		}
		v->fade_wrote = true;
	}
	ff8_bgate_fade_write(slot, o);
	return 0;
}

// Per-battle state is keyed by addresses (anim_cmd, entity slot, effect ctx) that the next
// battle reuses for DIFFERENT models and effects. Carrying it over crashed the second battle
// of a session: an anim_cmd kept battle 1's "SLOW forced by us" policy, so its frame-count
// variables were halved for an animation that was no longer SLOW (see the C3 var hook).
static void ff8_bgate_battle_reset()
{
	for (ff8_bgate_rec_t *rec : { &ff8_bgate_rec_fx, &ff8_bgate_rec_eq })
	{
		ff8_bgate_R = rec;
		ff8_bgate_fx_summary("battle ended");
		rec->snaps[0].valid = rec->snaps[1].valid = false;
		rec->replay_ok = false;
		rec->tail = rec->tail_otclear = false;
		rec->blacklist_n = 0;
		rec->last_r = 0;
	}
	ff8_bgate_R = &ff8_bgate_rec_fx;
	ff8_bgate_fx_end_hold = 0;
	ff8_bgate_gfc_reset();
	ff8_bgate_tlb_reset();
	ff8_bgate_tla_reset();
	ff8_bgate_la_reset();
	ff8_bgate_feedback_req = 0;
	ff8_bgate_fx_otclear = false;
	memset(ff8_bgate_shake_last, 0, sizeof(ff8_bgate_shake_last));
	memset(ff8_bgate_anim_policy, 0, sizeof(ff8_bgate_anim_policy));
	memset(ff8_bgate_vmtask, 0, sizeof(ff8_bgate_vmtask));
	ff8_bgate_in_vm = false;
	ff8_bgate_inside_preread = false;
	ff8_bgate_cam_held_moved = false;
	memset(ff8_bgate_move, 0, sizeof(ff8_bgate_move));
	ff8_bgate_nent_req = 0;
	ff8_bgate_ent_last_ok = ff8_bgate_ent_prev_ok = ff8_bgate_ent_real_ok = false;
	ff8_bgate_ent_swapped = nullptr;
	for (ff8_bgate_vis_t &v : ff8_bgate_vis) v = {};
	ff8_bgate_cam_r0.valid = ff8_bgate_cam_r1.valid = false;
	memset(ff8_bgate_shake_prev, 0, sizeof(ff8_bgate_shake_prev));
	ff8_bgate_cam_nudged = false;
	ff8_bgate_inside_queue_anim = false;
	ff8_bgate_tick_skip = false;
	ff8_bgate_hidden_idx = 0;
	ff8_bgate_phase = ff8_bgate_n - 1; // the bump right after makes the battle's first BdLink a real frame (vanilla tick 0)
	ffnx_info("battle 30fps: new battle, per-battle state reset\n");
}

// DATA vs CODE: every hook below, classified (30 fps on = every battle data file is converted to
// 30 fps by the FF8UltimateEditor tools: no marker, no detection, no fallback; DAT30FPS_STUDY.md 12).
// DATA = plays converted battle data: runs on every 30 Hz data tick (every host frame at 30 fps),
// never gated, never extrapolated. CODE = hardcoded engine timing: kept at the native 15 Hz.
//   0x500900 BdLink                    phase driver (+ per-battle reset, display-only restores)
//   0x4A8E30/0x4A84E0 HUD, 0xB8A3E4    CODE  battle UI ticks / input latch
//   0x56DD70 Boost, 0x4BA6C0 Renzo     CODE  GF Boost, Renzokuken trigger clock
//   0x4701B0 savemap tick              CODE  game time / battle countdown
//   0x509520 QueueAnimation            DATA  scope marker for the policy below
//   0x509440 pre_ReadAnimation         DATA  entity/weapon policy div 1 (Slow: SLOW cleared, hold 2)
//   0x5044B0 C3 vars 09/0A             DATA  untouched (Slow emulation reports doubled counts)
//   0x508F90 ReadAnimation             DATA  entity/weapon: real read every call; stage + effect models
//                                            (no policy, read in stage code / the 15 Hz effect tick):
//                                            unchanged 1-in-n + look-ahead midpoint
//   0x504290 AnimSeq VM                DATA  every data tick; held-frame nudge only for 15 Hz code moves
//   0x5094D0 getAnimationFrameCount    DATA  AB windows: (2n+1)/2 = the native frame count
//   0x506C10 AddTaskToQueueAnimSeq     DATA  tags tasks created inside the VM
//   0x50F720 9E move task              DATA  converted duration as is (x data_div at 60 fps)
//   0x501F90 t84, 0x5057D0 t9f,
//   0x50F500 tdrag (AD/AE)             DATA  VM tasks every data tick (untagged: former 15 Hz pacing)
//   0x50F6C0 t96, 0x50F830 tstep,
//   0x50F0E0 t81, 0x50F2E0 ta6         CODE  15-tick tasks the converter leaves at vanilla: 15 Hz
//   0x5035E0 camera keyframe player    DATA  every data tick, vanilla step 16
//   0x509610 camera script VM          DATA  every data tick
//   0x504060 updateBattleCamera        DATA  as is when the camera moves on held frames; in-between
//                                            only for cameras 15 Hz effect code writes on real frames
//   0x5033E0 camera shake offsets      CODE  shake producers are 15 Hz code (96, hit queue, effects)
//   0x47CF50 screen feedback, 0x45D610 display, 0x50093A effect tree, 0x500923 hit-effect queue,
//   0x500971/0x502B95 entity queue/render (ported effects' entities), GF cinematic / timeline redraws
//                                      CODE  effects: native 15 Hz tick + held frames + draw replay
//   0x483470 status timers, 0x482F80 Gilgamesh/Angelo  CODE
//   0x50A1C0 pulse, 0x50A370 Confuse spin, 0x50A5F0 Float bob, 0x5711C0 overlay, 0x50C110 A8 fades
//                                      CODE  status visuals (A8 left unconverted by the converter)
//   0x50ACB4 magic cast timeout        CODE  x n host frames = native 60 s
//   battle stage code (the stage task draws every host frame and counts those calls): sky angle,
//   texture flipbooks, stage 19 / 37 / 44-46 / 92 / 137 / 142 effects = the 30FPS Battle mod's
//   30FPSBattleStage.hext (steps halved / frame counts doubled, 30 fps only); here:
//   0x50CB90 texture scroll            CODE  real frames only (a VRAM shift cannot be halved)
//   0x511EF0 stage 147 scripted task   CODE  state machine on real frames, held frames draw only

// A texture scroll (texture animation entry with 0 frames: waterfalls...) moves its VRAM rectangle by
// `speed` rows per call (BS_UpdateTextureAnimation 0x50CB20 -> BS_TextureAnim_ScrollBlit 0x50CA80):
// at 30 fps it scrolled twice as fast. Real frames only.
static void (__cdecl *ff8_bgate_texscroll_orig)(void *, int, int) = nullptr;
static void __cdecl ff8_bgate_texscroll_hook(void *block, int index, int speed)
{
	if (ff8_bgate_phase == 0) ff8_bgate_texscroll_orig(block, index, speed);
}

// Stage 147 (encounters 136 / 195 / 947 / 1000 / 1014): its second stage task 0x511EF0 runs a scripted
// state machine that moves battle entities by frame counts (task +0xE state, +0x10 frame counter), then
// draws the stage like the first task 0x511EB0 (texture animations 0 and 1, the stage groups). On held
// frames in state 1: only that drawing part.
static uint32_t (__cdecl *ff8_bgate_stage147_orig)(uint8_t *) = (uint32_t(__cdecl *)(uint8_t *))0x511EF0;
static uint32_t __cdecl ff8_bgate_stage147_hook(uint8_t *task)
{
	if (ff8_bgate_phase != 0 && *(int16_t *)(task + 0xC) == 1)
	{
		((void(__cdecl *)(int))0x51B0D0)(0); // BS_TickTextureAnimation (stage texture animation 0)
		((void(__cdecl *)(int))0x51B0D0)(1);
		((char(__cdecl *)())0x500FD0)();     // BattleStage_AnimateAndRenderGroups
		return 0;
	}
	return ff8_bgate_stage147_orig(task);
}

// Exported for test tools (AutoTest): the phase of the battle loop iteration in progress, read before its BdLink
// (e.g. at the input pump). 1 = this iteration's BdLink is a real frame (the battle logic runs in it: a pad read
// here is the vanilla tick's read), 0 = held (data only), -1 = no battle gate (not 30/60 fps, or not in a battle yet).
extern "C" __declspec(dllexport) int __cdecl ff8_bgate_next_bdlink_real()
{
	if (!ff8_bgate_active || ff8_bgate_left_battle || getmode_cached()->driver_mode != MODE_BATTLE) return -1;
	return (ff8_bgate_phase + 1) % ff8_bgate_n == 0 ? 1 : 0;
}

void ff8_bgate_install_hooks()
{
	// The one knob: host frames per real battle tick. ff8_limit_fps() drives the battle module
	// at 30fps for limiter 2 and 60fps for limiter 3, against a native 15fps tick.
	ff8_bgate_n = (ff8_fps_limiter >= FPS_LIMITER_60FPS) ? 4 : 2;
	// converted battle data is 30 Hz: host frames per data tick
	ff8_bgate_data_div = ff8_bgate_n / 2 > 1 ? ff8_bgate_n / 2 : 1;

	// Battle frame phase driver
	ff8_bgate_bdlink_orig = (int(__cdecl *)())0x500900;
	ff8_bgate_bdlink_ri = replace_function(0x500900, (void *)ff8_bgate_bdlink_hook);

	// UI tick reduction: vanilla 4/frame -> 4/n per frame (native 60 ticks/s either way)
	ff8_bgate_hudupdate_orig = (int(__cdecl *)())0x4A8E30;
	ff8_bgate_huddisplay_orig = (int(__cdecl *)())0x4A84E0;
	ff8_bgate_hudupdate_ri = replace_function(0x4A8E30, (void *)ff8_bgate_hudupdate_hook);
	ff8_bgate_huddisplay_ri = replace_function(0x4A84E0, (void *)ff8_bgate_huddisplay_hook);

	// Fresh-input latch divisor: 4 -> 4/n UI ticks, so the ctx+33 "fresh input" cadence tracks
	// the host frame rate (30fps -> 30/s, 60fps -> 60/s; vanilla PC was 15/s). This is also what
	// restores GF Boost's phase pacing, which keys off that latch rather than the tick counter.
	patch_code_dword(0xB8A3E4, 4 / ff8_bgate_n); // CONST_BattleUI_TicksPerFrame

	// GF Boost phases/window at native pacing (1 latch in n counts)
	ff8_bgate_boost_orig = (void(__cdecl *)())0x56DD70;
	ff8_bgate_boost_ri = replace_function(0x56DD70, (void *)ff8_bgate_boost_hook);

	// Renzokuken / gunblade trigger clock at native pace (1 latch in n counts)
	ff8_bgate_renzo_orig = (unsigned char(__cdecl *)())0x4BA6C0;
	ff8_bgate_renzo_ri = replace_function(0x4BA6C0, (void *)ff8_bgate_renzo_hook);

	// Game time / battle countdown at native 60 ticks/s
	ff8_bgate_savemap_tick_orig = (int(__cdecl *)())0x4701B0;
	ff8_bgate_savemap_tick_ri = replace_function(0x4701B0, (void *)ff8_bgate_savemap_tick_hook);

	// Battle model animation: converted data read at the data rate (per-anim_cmd policy: entities
	// every data tick, Slow emulated by a hold; stage / effect models 1-in-n + look-ahead), the
	// choreography VM every data tick, AB frame counts in native units
	ff8_bgate_queueanim_orig = (int(__cdecl *)(void *, int))0x509520;
	ff8_bgate_queueanim_ri = replace_function(0x509520, (void *)ff8_bgate_queueanim_hook);
	ff8_bgate_preread_orig = (int(__cdecl *)(void *, void *, int))0x509440;
	ff8_bgate_preread_ri = replace_function(0x509440, (void *)ff8_bgate_preread_hook);
	// choreography-visible frame counts back to native units under forced SLOW
	ff8_bgate_c3var_orig = (int(__cdecl *)(int))0x5044B0;
	ff8_bgate_c3var_ri = replace_function(0x5044B0, (void *)ff8_bgate_c3var_hook);
	ff8_bgate_readanim_orig = (int(__cdecl *)(void *, void *))0x508F90;
	ff8_bgate_readanim_ri = replace_function(0x508F90, (void *)ff8_bgate_readanim_hook);
	ff8_bgate_animseq_upd_orig = (int(__cdecl *)(void *))0x504290;
	ff8_bgate_animseq_upd_ri = replace_function(0x504290, (void *)ff8_bgate_animseq_upd_hook);
	replace_function(0x5094D0, (void *)ff8_bgate_framecount_hook); // getAnimationFrameCount, fully replaced
	// VM task tagging (single AnimSeq-queue task creator)
	ff8_bgate_addtask_orig = (void *(__cdecl *)(void *))0x506C10;
	ff8_bgate_addtask_ri = replace_function(0x506C10, (void *)ff8_bgate_addtask_hook);
	// run-up movement task: converted duration, x data_div at 60 fps only
	ff8_bgate_anim9e_orig = (void *(__cdecl *)(void *, void *, int))0x50F720;
	ff8_bgate_anim9e_ri = replace_function(0x50F720, (void *)ff8_bgate_anim9e_hook);
#if FF8_BGATE_DIAG
	ff8_bgate_diag_render_orig = (char(__cdecl *)(void *))0x502D40;
	ff8_bgate_diag_render_ri = replace_function(0x502D40, (void *)ff8_bgate_diag_render_hook);
#endif

	// Battle camera: gate the whole keyframe player (0x5035E0) to real frames instead of
	// halving its internal time step (imm8 @0x503A80, formerly patched to 16/n).
	//
	// *** DO NOT re-attempt the step-halving patch *** - it caused a genuine engine hang,
	// reproduced twice, hard (window "Not Responding", no exception, no log after the
	// point of entry - confirmed by exhaustive per-line flushed tracing that BdLink_GF
	// itself never returns). ProcessCameraAnimation contains a catch-up loop: "while
	// current_time >= this segment's total_time: advance to the next keyframe segment"
	// (0x5035E9-0x503613). That loop assumes the vanilla step (16); halving it can make
	// current_time never clear a segment whose total_time is <= the halved step, so the
	// loop spins forever re-reading keyframes - a true infinite loop, invisible to SEH
	// (no bad memory access). Triggered reliably by Cure, which calls
	// Battle_PlayCameraAnimation(&MAG_Cure_cameraAnimation) at cast start - that track
	// apparently has a short segment. Any other camera track could hit the same trap.
	//
	// Gating the task instead is unconditionally safe: on real frames the function runs
	// completely unmodified (same step, same segment math as vanilla), so it can only
	// ever do what vanilla already proves safe; on held frames it doesn't run at all and
	// the camera holds its last real position. Costs camera smoothness (steps at native
	// 15fps like models did before the SLOW-interpolation work) in exchange for zero
	// hang risk - the correct trade until camera data can be proven trap-free.
	ff8_bgate_camanim_orig = (int(__cdecl *)(void *))0x5035E0;
	ff8_bgate_camanim_ri = replace_function(0x5035E0, (void *)ff8_bgate_camanim_hook);
	// Camera smoothness recovered separately: extrapolate the render-facing output only
	// (see ff8_bgate_updatecam_hook above) - never re-touches the risky keyframe state.
	ff8_bgate_camseq_orig = (void *(__cdecl *)())0x509610;
	ff8_bgate_camseq_ri = replace_function(0x509610, (void *)ff8_bgate_camseq_hook);
	ff8_bgate_updatecam_orig = (int(__cdecl *)())0x504060;
	ff8_bgate_updatecam_ri = replace_function(0x504060, (void *)ff8_bgate_updatecam_hook);
	// screen feedback request (see ff8_bgate_feedback_request_hook); the 11-byte original is fully replaced
	replace_function(0x47CF50, (void *)ff8_bgate_feedback_request_hook);
	// cinematic-engine GF summons: held-frame 3D redraw (see ff8_bgate_gfc_*)
	ff8_bgate_gfc_install();
	// native effect code (src/ff8/battle/fx), hosted by the gate: its task queue executor
	// replaces 0x508420 (observed: task-tagged draws for the look-ahead pairing, see
	// ff8_bgate_la_*), the ports run only inside the verifier on real ticks (ff8_bgate_fxv_tick)
	// and, with ff8_battle_fx_verify, the native prim-model player is compared against the
	// original on every call (all effects)
	ff8fx::g_exec_observer = &ff8_bgate_etq_observer;
	ff8fx::g_verify_event = ff8_bgate_fxv_event;
	ff8fx::install_hosted();

	// effect packets re-read at display time (packet aliasing, see ff8_bgate_fx_recapture_*)
	ff8_bgate_display_orig = (int(__cdecl *)(unsigned int))0x45D610;
	ff8_bgate_display_ri = replace_function(0x45D610, (void *)ff8_bgate_display_hook);

	// Magic/GF effect pacing: native-rate tick + OT-insert record/replay. This paces the
	// WHOLE effect tree (magic, Draw/Stock, limits, GF summons - Ifrit's dedicated gates
	// are gone, the tick gating covers its entire subsystem) and needs no per-effect
	// knowledge, whitelists or SFX suppression: nothing runs twice, nothing is rewound.
	ff8_bgate_effect_tick_orig = (int(__cdecl *)(void *))get_relative_call(0x50093A, 0);
	replace_call(0x50093A, (void *)ff8_bgate_effect_tick_gate);
	// battle entities moved by a ported effect: the fields each entity queue's rebuild used (after the
	// queue), the in-between matrix around a listed entity's render on held frames (ff8_bgate_ent_*)
	ff8_bgate_entq_orig = (int(__cdecl *)(void *))get_relative_call(0x500971, 0);
	replace_call(0x500971, (void *)ff8_bgate_entq_hook);
	ff8_bgate_entrender_orig = (char(__cdecl *)(void *))get_relative_call(0x502B95, 0);
	replace_call(0x502B95, (void *)ff8_bgate_entrender_hook);
	// battle-stage queue (battle logic state machines): real frames only (see ff8_bgate_stageq_gate)
	ff8_bgate_stageq_orig = (int(__cdecl *)(void *))get_relative_call(0x50090B, 0);
	replace_call(0x50090B, (void *)ff8_bgate_stageq_gate);
	// next queued action started only on the loop iterations whose BdLink is a real frame (see ff8_bgate_actq_gate)
	ff8_bgate_actq_orig = (int(__cdecl *)())get_relative_call(0x47D7C3, 0);
	replace_call(0x47D7C3, (void *)ff8_bgate_actq_gate);
	// battle text-management timers (end-of-battle fade timer, AI waits / delayed messages) at native pace
	ff8_bgate_textmgmt_orig = (int(__cdecl *)())get_relative_call(0x47D7FB, 0);
	replace_call(0x47D7FB, (void *)ff8_bgate_textmgmt_gate);
	// battle logic loop on real frames, data-only iterations in between (see ff8_bgate_battleloop_mode)
	ff8_bgate_battleloop_orig = get_relative_call(0x47D113, 0);
	replace_call(0x47D113, (void *)ff8_bgate_battleloop_thunk);
	// emulated battle file load time counted on real frames (see ff8_bgate_fileload_countdown_hook)
	ff8_bgate_fileload_orig = (int(__cdecl *)())0x482870;
	ff8_bgate_fileload_ri = replace_function(0x482870, (void *)ff8_bgate_fileload_countdown_hook);
	// hit-effect queue: same record/replay pacing
	ff8_bgate_eq_tick_orig = (int(__cdecl *)(void *))get_relative_call(0x500923, 0);
	replace_call(0x500923, (void *)ff8_bgate_eq_tick_gate);
	// camera shake offsets held on held frames
	ff8_bgate_camops_orig = (int(__cdecl *)())0x5033E0;
	ff8_bgate_camops_ri = replace_function(0x5033E0, (void *)ff8_bgate_camops_hook);
	// AnimSeq-spawned tasks at native pace
	ff8_bgate_t84_ri = replace_function(0x501F90, (void *)ff8_bgate_t84_hook);
	ff8_bgate_t9f_ri = replace_function(0x5057D0, (void *)ff8_bgate_t9f_hook);
	ff8_bgate_tstep_ri = replace_function(0x50F830, (void *)ff8_bgate_tstep_hook);
	ff8_bgate_ttexrestore_ri = replace_function(0x508660, (void *)ff8_bgate_ttexrestore_hook);
	ff8_bgate_tfanfare_ri = replace_function(0x501A70, (void *)ff8_bgate_tfanfare_hook);
	// battle text box duration and the screen fades: draw every frame, advance on real frames only
	ff8_bgate_ttext_ri = replace_function(0x506F70, (void *)ff8_bgate_ttext_hook);
	ff8_bgate_tfadeblack_ri = replace_function(0x501D10, (void *)ff8_bgate_tfadeblack_hook);
	ff8_bgate_tfadewhite_ri = replace_function(0x501E80, (void *)ff8_bgate_tfadewhite_hook);
	ff8_bgate_t96_ri = replace_function(0x50F6C0, (void *)ff8_bgate_t96_hook);
	ff8_bgate_tdrag_ri = replace_function(0x50F500, (void *)ff8_bgate_tdrag_hook);
	ff8_bgate_t81_ri = replace_function(0x50F0E0, (void *)ff8_bgate_t81_hook);
	ff8_bgate_ta6_ri = replace_function(0x50F2E0, (void *)ff8_bgate_ta6_hook);

	// Status-effect timers + Gilgamesh/Angelo countdown at native rate
	ff8_bgate_timerstatus_orig = (void(__cdecl *)())0x483470;
	ff8_bgate_timerstatus_ri = replace_function(0x483470, (void *)ff8_bgate_timerstatus_hook);
	ff8_bgate_gilga_orig = (void(__cdecl *)())0x482F80;
	ff8_bgate_gilga_ri = replace_function(0x482F80, (void *)ff8_bgate_gilga_hook);

	// Entity task visuals at native pace: status pulse / Confuse spin / Float bob, status sprites, A8 fades
	ff8_bgate_pulse_orig = (int(__cdecl *)(uint8_t *))0x50A1C0;
	ff8_bgate_pulse_ri = replace_function(0x50A1C0, (void *)ff8_bgate_pulse_hook);
	ff8_bgate_spin_orig = (void(__cdecl *)(uint8_t *))0x50A370;
	ff8_bgate_spin_ri = replace_function(0x50A370, (void *)ff8_bgate_spin_hook);
	ff8_bgate_bob_orig = (void(__cdecl *)(uint8_t *))0x50A5F0;
	ff8_bgate_bob_ri = replace_function(0x50A5F0, (void *)ff8_bgate_bob_hook);
	ff8_bgate_ovl_orig = (int(__cdecl *)(uint8_t *, int, int))0x5711C0;
	ff8_bgate_ovl_ri = replace_function(0x5711C0, (void *)ff8_bgate_ovl_hook);
	ff8_bgate_fade_orig = (int(__cdecl *)(uint8_t *))0x50C110;
	ff8_bgate_fade_ri = replace_function(0x50C110, (void *)ff8_bgate_fade_hook);

	// battle stage code at native pace (the rest is the 30FPS Battle mod's 30FPSBattleStage.hext)
	ff8_bgate_texscroll_orig = (void(__cdecl *)(void *, int, int))get_relative_call(0x50CB90, 0);
	replace_call(0x50CB90, (void *)ff8_bgate_texscroll_hook);
	patch_code_dword(0x511E1A, (DWORD)ff8_bgate_stage147_hook); // push 0x511EF0 (AddTaskToQueue callback)

	// (Magic cast safety timeout "++MAGIC_EFFECT_TIMEOUT_CTR < 0x384" @0x50ACB4: no patch any more, the battle-stage
	// queue that runs BattleActionSequence_Tick_MagicCast is paced to real frames: 0x384 calls = the native 60 s)

	ffnx_info("battle %dfps: gates installed (n=%d -> UI %d ticks/frame, input latch %d; battle DATA at 30 Hz (data tick 1-in-%d): entity anims + AnimSeq VM + VM tasks + cameras unpaced; effects native-tick + draw replay)\n",
		15 * ff8_bgate_n, ff8_bgate_n, 4 / ff8_bgate_n, 4 / ff8_bgate_n, ff8_bgate_data_div);
}

void ff8_bgate_install_reference()
{
#if !FF8_BGATE_DIAG
	// REFERENCE mode (vanilla 15fps battle): nothing is gated (n = 1 -> every frame is a
	// real frame), only the BdLink hook runs, for its diagnostics log (camera state) - to
	// compare the 30fps build against the original timing.
	ff8_bgate_n = 1;
	ff8_bgate_bdlink_orig = (int(__cdecl *)())0x500900;
	ff8_bgate_bdlink_ri = replace_function(0x500900, (void *)ff8_bgate_bdlink_hook);
	ffnx_info("battle 30fps: REFERENCE mode (limiter < 30fps): no gating, diagnostics only\n");
#endif
#if FF8_BGATE_DIAG
	// diagnostics-only mode: vanilla behavior at 15fps, animation trace only - for
	// capturing a reference trace to compare choreography timing against the gated build
	ff8_bgate_bdlink_orig = (int(__cdecl *)())0x500900;
	ff8_bgate_bdlink_ri = replace_function(0x500900, (void *)ff8_bgate_bdlink_hook);
	ff8_bgate_queueanim_orig = (int(__cdecl *)(void *, int))0x509520;
	ff8_bgate_queueanim_ri = replace_function(0x509520, (void *)ff8_bgate_queueanim_hook);
	ff8_bgate_animseq_upd_orig = (int(__cdecl *)(void *))0x504290;
	ff8_bgate_animseq_upd_ri = replace_function(0x504290, (void *)ff8_bgate_animseq_upd_hook);
	ff8_bgate_diag_render_orig = (char(__cdecl *)(void *))0x502D40;
	ff8_bgate_diag_render_ri = replace_function(0x502D40, (void *)ff8_bgate_diag_render_hook);
	ffnx_info("battle DIAG-only mode: vanilla flow, animation trace enabled\n");
#endif
}
