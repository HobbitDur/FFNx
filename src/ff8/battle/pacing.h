/****************************************************************************/
//    Copyright (C) 2009 Aali132                                            //
//    Copyright (C) 2018 quantumpencil                                      //
//    Copyright (C) 2018 Maxime Bacoux                                      //
//    Copyright (C) 2020 Chris Rizzitello                                   //
//    Copyright (C) 2020 John Pritchard                                     //
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

#pragma once

#include <stdint.h>

// FF8 battle pacing: run the battle module at a higher host frame rate while every
// battle subsystem keeps its original 15 ticks per second.
//
// The PC battle module is frame-locked at 15 fps and catches up the battle UI by ticking
// it 4 times back to back per frame, with a single pad read per frame: menus, cursor and
// gauges move in bursts and presses shorter than a frame (GF Boost, Renzokuken, Zell's
// Duel) are merged or lost. With pacing on, the battle loop runs at 15 * n fps; one host
// frame in n is a "real tick" where the battle logic advances exactly like a vanilla frame,
// the other n - 1 are "held" frames that redraw the last tick unchanged. The UI runs
// 4 / n ticks per host frame (one per frame at 60 fps) and the pad is read every host
// frame, so both keep the vanilla 60 ticks/s but spread evenly, like the PlayStation.
//
// FF8 2000 / Steam, English 1.2 only (FF8_EN.exe addresses); install verifies the code first.

// Host frames per real battle tick for a battle host rate: 2 at 30 fps, 4 at 60 fps.
// Installs the hooks. Returns false (nothing patched) on an unsupported executable or n.
bool ff8_battle_pacing_init(int host_frames_per_tick);

bool ff8_battle_pacing_enabled();
// Host frames per real tick (n), 1 when pacing is off
int ff8_battle_pacing_frames_per_tick();
// Position of the current host frame inside its tick: 0 = real tick, 1..n-1 = held frame
int ff8_battle_pacing_phase();
bool ff8_battle_pacing_is_real_tick();
// Host battle frames since the battle started
uint32_t ff8_battle_pacing_frame_number();

// Frame limiter integration (ff8_limit_fps): target rate of the battle module, and the
// number of extra pad reads to spread over one host frame (so the pad is read 60 times per
// second whatever the host rate; 0 at 60 fps).
double ff8_battle_pacing_battle_framerate();
int ff8_battle_pacing_extra_pad_reads();
void ff8_battle_pacing_read_pad();
// Called once per host frame from the frame limiter, whatever the game mode
void ff8_battle_pacing_on_host_frame(uint32_t driver_mode);

// Extension points for a layer that draws something better than a plain hold on held
// frames (in-between frames). Every callback is optional; each held-frame callback returns
// true when it has drawn the frame itself, false to fall back to the plain hold/redraw.
enum ff8_battle_pacing_queue
{
	FF8_BATTLE_PACING_QUEUE_EFFECT = 0,     // magic / GF / limit / Draw effect tree
	FF8_BATTLE_PACING_QUEUE_HIT_EFFECT = 1, // hit sparks, camera shakes, footstep dust...
};

struct ff8_battle_pacing_layer
{
	// New battle: every per-battle state must be dropped (addresses are reused)
	void (*on_battle_start)();
	// Start of every host battle frame, before any battle logic (phase already updated)
	void (*on_frame_begin)(bool real_tick);
	// End of a real tick of an effect queue (ret = the queue's return value, 0 = finished)
	void (*on_effect_tick)(ff8_battle_pacing_queue queue, void *ctx, int ret);
	// Held frame of an effect queue; orig = the engine's queue tick
	bool (*effect_held_frame)(ff8_battle_pacing_queue queue, void *ctx, int (__cdecl *orig)(void *));
	// Held frame of a model animation (Battle_ReadAnimation arguments)
	bool (*anim_held_frame)(void *anim_header, void *anim_cmd);
};

void ff8_battle_pacing_set_layer(const ff8_battle_pacing_layer *layer);
