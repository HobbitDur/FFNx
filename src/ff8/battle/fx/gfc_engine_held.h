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

// 30 fps part of the GF cinematic engine (gfc_engine.h / gfc_engine.cpp): the held-frame modes
// and the declarations used by the FX_HELD(...) statements of the engine and of its clone files
// (mag_ifrit.cpp .. mag_eden.cpp). Included by gfc_engine.h when FF8_FX_HELD is defined; the
// held-frame code of gfc_engine.cpp is gfc_engine_held.inc (included at its end).
#pragma once
#include "fx_held.h"

namespace ff8fx
{
namespace gfc
{
	struct Clone;

	// ------------------------------------------------------------------------------------
	// modes
	// ------------------------------------------------------------------------------------
	// g_predict: the VM runs one tick ahead on saved state for a held frame; every engine call
	// with a side effect outside the snapshot (sound, music, streams, file loads, VRAM uploads,
	// battle damage, camera animation, battle entity/model writes) is skipped by its wrapper.
	extern bool g_predict;
	// g_held: a held-frame (30 fps) draw is running; num/den = position between the last real
	// tick (0) and the next one (den). The dispatcher forces rt->boneSkipFlag while it is set.
	// real_skip = the value rt->boneSkipFlag had on the real tick (before the held draw forced it).
	struct Held { bool active; int num, den; uint8_t real_skip; };
	extern Held g_held;
	inline bool held_predicting() { return g_predict; }
	inline bool held_active() { return g_held.active; }

	// Writes the VM does OUTSIDE the snapshot set (engine block STATE_LO..STATE_HI, the bone
	// array, the arena [ctx+0x70, ctx+0x74), the RT/WS scratch block) must be announced with
	// guard(addr, size) right before the write: while predicting, the original bytes are
	// journaled and put back when the prediction ends. (No effect on real ticks.)
	void guard_record(const void *p, int n);
	inline void guard(const void *p, int n) { if (g_predict) guard_record(p, n); }
	// guard only when the address lies outside the engine state block (vm_a: resource file
	// table slots)
	inline void guard_ext(const void *p, int n)
	{
		uint32_t a = (uint32_t)p;
		if (a < STATE_LO || a + (uint32_t)n > STATE_HI) guard(p, n);
	}
	// guard a write that should land inside the engine block but whose index comes from data
	// (light slot, aux slot): only an out-of-range index reaches memory outside the snapshot
	inline void guard_outside_block(const void *p, int n) { guard_ext(p, n); }
	// a write at bone+off (off from script data) may leave the bone; announce it then
	inline void guard_bone(uint8_t *bone, int32_t off, int n)
	{
		if (off < 0 || off + n > 0x100) guard(bone + off, n);
	}
	// blob::AltTextureUpload while predicting: the per-tick budget logic (ctx+0xD2, < 12
	// uploads, returns 1 when over budget) without the upload
	inline int32_t held_alt_texture_budget()
	{
		uint8_t &n = U8(CTX(), 0xD2);
		if (n >= 12) return 1;
		n++;
		return 0;
	}
	// GTE far colour (control regs 21..23) and DQA/DQB (27, 28) + the fog near/far globals
	// written by 0x45DDA0 / 0x56CCC0 / 0x56CCA0: persistent render state outside the snapshot
	inline void guard_fog_state()
	{
		if (!g_predict) return;
		guard((void *)0x1CA92D0, 12);   // GTE ctrl RFC/GFC/BFC (0x1CA927C + 21*4)
		guard((void *)0x1CA92E8, 8);    // GTE ctrl DQA/DQB (0x1CA927C + 27*4)
		guard((void *)0x209AB64, 4);    // fog near (sub_56CCC0)
		guard((void *)0xC78BF0, 4);     // fog far (sub_56CCA0)
	}

	// ------------------------------------------------------------------------------------
	// held frames of a clone (gfc_engine_held.inc)
	// ------------------------------------------------------------------------------------
	bool HeldReady(Clone &c);
	void HeldFrame(Clone &c, int num, int den);
	bool HeldCamera(Clone &c, int num, int den, int16_t world[3], int16_t lookat[3]);
	// draw handler ids besides 3 whose bone+0xBC block is an embedded battle model block
	// (+0x14 -> skeleton), saved around held draws (Leviathan: 20, 24)
	void held_model_draws(Clone &c, uint8_t d0, uint8_t d1 = 0, uint8_t d2 = 0, uint8_t d3 = 0);

	// ------------------------------------------------------------------------------------
	// FX_HELD statements of gfc_engine.cpp (gfc_engine_held.inc)
	// ------------------------------------------------------------------------------------
	// SequenceTick: the ports ran on this real tick / the tick's Workspace + RuntimeSlot copy
	void held_note_tick();
	void held_note_scratch();
	// h_B26AD0 (frozen path): on a held frame whose real tick advanced the animation, the bone
	// matrices come from the exact in-between pose (pose_midpoint); returns true then
	bool held_model_pose(uint8_t *blk, uint8_t *hdr, uint8_t *cmd);
	// Draw 6 particle system (frozen path): held-frame position type + draw of particle p
	void held_particle_draw(uint8_t *p);
	// Draw 37 ribbon trail: scratch save / VXY0 of the real tick / scratch restore
	void held_ribbon_save(uint8_t *ring);
	void held_ribbon_v0();
	void held_ribbon_restore();
	// Draw 26 screen capture strips: capture of the real tick / held draws to skip
	void held_note_capture(uint8_t *b);
	bool held_capture_skip(uint8_t *b);
	// Draw 39 starfield: memo of the stars the real tick drew / held-frame draw
	void held_star_begin(const uint8_t *blk);
	void held_star_drawn(int32_t i);
	void dh_39_StarfieldHeld();
}
}
