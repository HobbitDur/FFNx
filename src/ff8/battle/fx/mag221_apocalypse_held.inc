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

// 30 fps part of mag221_apocalypse.cpp: the held frames are the cinematic engine's
// (gfc_engine_held.inc): the node poses / matrix nodes interpolated between two real ticks, every
// draw handler re-run in its frozen (boneSkipFlag) form on the saved engine state. Camera:
// HeldCamera interpolates the battle camera words the VM writes (op 0x039).
//   - Draw 50 ObjectShatter: nothing comes loose or is thrown on a held frame (0xAABFF0 draws rand:
//     launches stay 15 Hz, on the real tick); every loose piece is drawn at the exact in-between of its
//     state N and its next step (0xAAC5A0 run on a copy as the next tick runs it: radius, angle,
//     spin bytes and scale between N and N+1, the angles the short way round), in the colour of the
//     real tick (the colour index its draw read; the next tick's at the full step); a frozen tick
//     (boneSkipFlag or engine freeze) holds N. Pieces the next tick releases / throws (0xAABFF0) do
//     so on that real tick (held(1) differs there by design).
//   - Draw 69 EntityTint writes the flags word +0 and the colour word +0x28 of battle entities
//     (outside the engine state): a held frame runs it on the in-between bone (outPos: the tint of the
//     in-between colour), the words of the real tick come back when the next real tick starts
//     (TintRestore, called by the module's SequenceTask before the engine tick: each word the held
//     frame left untouched since), as draw 47 PaletteHsvShift (gfc_palshift_held.inc).
//   - Draws 51 / 68 (grid sheets), prim 58, bone 11 and the shared handlers are pure functions of the
//     interpolated bone state: the frozen draw path as every generic handler.

namespace ff8fx
{
namespace gfc
{
namespace apocalypse221
{
	// ------------------------------------------------------------------------------------
	// draw 50
	// ------------------------------------------------------------------------------------
	static inline int32_t held_mix(int32_t a, int32_t b) { return a + (int32_t)((int64_t)(b - a) * g_held.num / g_held.den); }

	// the age the piece's colour comes from: the real draw read it before its step (0xAAC5A0 adds 1 unless
	// the real tick ran with boneSkipFlag), so before the full step the age of the real tick's draw (its
	// colour), at the full step the stored one (the next tick's colour)
	static int32_t PieceHeldAge(const uint8_t *p)
	{
		return S16(p, 2) - (g_held.num < g_held.den && g_held.real_skip == 0 ? 1 : 0);
	}

	// the piece p (state N, as the real draw left it) at the in-between of its next step, in buf
	static uint8_t *PieceHeld(uint8_t *p, uint8_t *buf)
	{
		memcpy(buf, p, 0x24);
		if (g_held.real_skip != 0 || U8(CTX(), 0x35) != 0 || g_held.num <= 0 || g_held.den <= 0) return buf;   // frozen: N
		uint8_t s1[0x24];
		memcpy(s1, p, 0x24);
		const uint8_t skip = U8(RT(), 0x45);
		U8(RT(), 0x45) = 0;
		PieceStep_AAC5A0(s1);
		U8(RT(), 0x45) = skip;
		S16(buf, 0xC) = (int16_t)held_mix(S16(p, 0xC), S16(s1, 0xC));
		S16(buf, 0x12) = (int16_t)held_mix(S16(p, 0x12), S16(p, 0x12) + (int16_t)(U16(s1, 0x12) - U16(p, 0x12)));
		for (int o = 0x18; o <= 0x1A; o++)
			U8(buf, o) = (uint8_t)held_mix(U8(p, o), U8(p, o) + (int8_t)(U8(s1, o) - U8(p, o)));
		U16(buf, 0x20) = (uint16_t)held_mix(U16(p, 0x20), U16(s1, 0x20));
		return buf;
	}

	// ------------------------------------------------------------------------------------
	// draw 69
	// ------------------------------------------------------------------------------------
	static struct { bool held; uint8_t *ent[7]; uint16_t f_real[7], f_drawn[7]; uint32_t c_real[7], c_drawn[7]; } g_tint = {};

	// end of every draw 69: real tick -> the words it leaves; held frame -> the words it drew
	static void TintNote()
	{
		if (held_predicting()) return;
		const bool h = g_held.active;
		for (int k = 0; k < 7; k++)
		{
			uint8_t *e = PTR(SCENE(), 0x60 + 4 * k);
			if (!h) g_tint.ent[k] = e;
			if (!e) continue;
			(h ? g_tint.f_drawn : g_tint.f_real)[k] = U16(e, 0);
			(h ? g_tint.c_drawn : g_tint.c_real)[k] = U32(e, 0x28);
		}
		g_tint.held = h;
	}

	// start of the next real tick: the real tick's words back where a held frame drew
	static void TintRestore()
	{
		if (!g_tint.held) return;
		g_tint.held = false;
		for (int k = 0; k < 7; k++)
		{
			uint8_t *e = g_tint.ent[k];
			if (!e) continue;
			if (U16(e, 0) == g_tint.f_drawn[k]) U16(e, 0) = g_tint.f_real[k];
			if (U32(e, 0x28) == g_tint.c_drawn[k]) U32(e, 0x28) = g_tint.c_real[k];
		}
	}

	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_mag221_apocalypse_held()
	{
		using namespace gfc;
		mark_held(0xAA5BD0);
		// the entity-joint VM ops (0x05B / 0x060 / 0x06E / 0x075) read a joint of an animating battle
		// entity: at its next pose while predicting (gfc_engine_held.inc held_entity_next_pose)
		held_entity_joints(apocalypse221::g_c);
		register_module_held(221, apocalypse221::Ready, apocalypse221::Frame);
		register_module_camera(221, apocalypse221::Camera);
	}
}
