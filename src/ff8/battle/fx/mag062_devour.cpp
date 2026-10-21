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

// Effect 62: Devour (command ability Devour; MAG_062_*). Its master is a copy of the actor/heal effect library's
// master (see act_engine.h): the same code as Pain's 0x865720 (mag034_pain.cpp) with this module's addresses; the
// master's state helpers are the library's (engine ports, through this module's descriptor).
//
// Setup MAG_062_DEVOUR_Init 0x7E4B00 (runs once, not ported; file loader MAG_062_DEVOUR_FL 0x7E4AB0: mag061.tim ->
// 0x2601318, mag061_0us.tim (the title strip) -> 0x2601900, mag061_1/2/3.tim (the three pictures) -> 0x2601904 /
// 0x26018F8 / 0x26018FC) creates the root queue 0x2601408 with the master (0x64 bytes); unless the cast context's flag
// +1 bit 0 it plays the camera animation 0x15962D8 (0x5099A0) and uploads mag061.tim; packet arenas mag061.tim + 0 /
// + 0x6000 by tick parity (0x26018F4 / 0x26018F0, second cursor pair 0x2601778 / 0x2601774); two task pools: 0x2601780
// (4 x 0x58: emitters) and 0x2601418 (10 x 0x54: the cast's tasks).
//   Master (0x7E4C10) - camera copy 0x2793E58, packet arena by tick parity (sprite cursor 0x260131C), bone follow,
//     11-state table (0x7E4D60 one emitter per action; the action loop 0x7E57F0 once the emitter releases the master:
//     root +0x63), the two queues (live task count -> +0x5E), ends when nothing is left.
//   Emitter (0x7E4D90), one per action: bone-follow anchor + model bounds (CURE_Emitter helpers); state 0 (0x7E4E00)
//     starts the cast task 0x7E4E30, state 1 (0x7E5780) releases the master at tick 0x4B, state 2 (0x7E57A0) applies
//     the action result of its target from tick 0x53 (0x506690) and finishes.
//   Cast task (0x7E4E30): state 0 (0x7E4E90) uploads the title strip and one of the three pictures picked by rand() % 3
//     (+0x4E), state 1 (0x7E4F10) starts the darkening 0x7E5290, the music fade 0x7E55F0 and the picture 0x7E4F60,
//     state 2 (0x7E5710) at tick 0x23 plays the sound of the outcome (table 0x15962B4 indexed by the action's first
//     target record word +6 clamped to 0..8) and finishes.
//   Darkening (0x7E5290): at tick 10 (0x7E5300) keeps the seven entities' flag words in +0x30..+0x48; next tick (0x7E5340)
//     sets the four colour/fade layers of 0x1D9898C (0x2C bytes each) to black at full level (colour bytes +0x2C..+0x2E
//     = 0, level word +6 = 0x1000), tints every visible entity (flag bit 2) black (flag |= 0x800, colour 0x7E53F0 =
//     the library's tint 0x733950 at 0x1000) and REMOVES THE TARGET: unless the action's first target record word +6
//     is 8, it sets bit 2 of the flag byte +0 of the entity in slot +0x2D (entity flag 4: not drawn). From tick 0x54
//     (0x7E54F0, 0x7E5510) the layer level and the tint go down by 0x200 per tick to 0 (tint 0 restores the base
//     colour); then (0x7E5560) every visible entity gets the bits 0x804 of its kept flag word back, the target flag
//     |= 4 again (same test) and the task finishes: the devoured target stays hidden.
//   Music fade (0x7E55F0): from tick 10 the music volume (Music_SetVolumeImmediate channel 0) goes from 0x7F down by 8
//     per tick to 0, after tick 0x53 back up by 8 per tick to 0x7F.
//   Picture (0x7E4F60): from tick 7 two screen-space layers drawn each tick after the update: the picture (two
//     textured quads, 0x7E5010) and the title strip (two semi-transparent quads, 0x7E51C0) scrolling left by 4 pixels
//     per tick (+0x1C); from tick 0x4B the brightness +0x50 (0x80) goes down by 0x10 per tick; at 0 it finishes.
//     Quads: 0x7E5090 (POLY_FT4 in the module arena, OT bucket [0x1D8E04C] + 0x14, SSIGPU_InsertPrimAltViewport).
// No task tests the draw-only flags (battle_to_update_flags 0x201); the module does not write the battle camera (the
// camera animation is the setup's 0x5099A0 call) nor an entity's position.
// Module globals: 0x2601318..0x2601920 (file pointers, packet cursors, pools, queues), camera copy 0x2793E58 (0x20).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace devour
{
	// the module descriptor of this copy of the library: the emitter queue (G_258BE30) and the emitter task (F_73A1A0)
	// are the only module addresses its engine ports use (spawn helper 0x7E4D60 = a_73A170)
	static Mod make_mod()
	{
		Mod m = {};
		m.name = "devour";
		m.effect_id = 62;
		m.lo = 0x7E4AB0;
		m.hi = 0x7E58A0;
		m.g[G_258BE30] = 0x2601780;
		m.f[F_73A1A0] = 0x7E4D90;
		return m;
	}
	static const Mod MOD_062 = make_mod();

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x260131C;   // module sprite packet cursor (tick parity)
	static const uint32_t Q_EMITTER = 0x2601780;       // master queues, in run order: emitters (0x58 bytes x 4)
	static const uint32_t Q_TASKS = 0x2601418;         // the cast's tasks (0x54 x 10)
	static const uint32_t TIM_TITLE = 0x2601900;       // mag061_0us.tim (title strip)
	static const uint32_t TIM_PIC1 = 0x2601904;        // mag061_1.tim
	static const uint32_t TIM_PIC2 = 0x26018F8;        // mag061_2.tim
	static const uint32_t TIM_PIC3 = 0x26018FC;        // mag061_3.tim
	static const uint32_t SOUND_TABLE = 0x15962B4;     // 9 sound descriptors by outcome (target record word +6)
	static const uint32_t FADE_LAYERS = 0x1D9898C;     // 4 colour/fade layers of 0x2C bytes: word +6 level, bytes +0x2C.. colour
	static const uint32_t ENTITIES = 0x1D972C0;        // battle entities (0x9C bytes, 7 slots)

	static const uint32_t ORIG_Master = 0x7E4C10;
	static const uint32_t ORIG_Emitter = 0x7E4D90;
	static const uint32_t ORIG_Cast = 0x7E4E30;
	static const uint32_t ORIG_Picture = 0x7E4F60;
	static const uint32_t ORIG_Darken = 0x7E5290;
	static const uint32_t ORIG_Music = 0x7E55F0;

	// common task tail (frame counter already incremented): end (release the linked task, return 2)
	// when finished (status read before the increment) and no children are alive
	static inline uint32_t task_end(uint32_t node, uint8_t status)
	{
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}

	// the action's first target record (action 0 of the cast context, target 0): its word +6 is the outcome
	static inline uint32_t first_target(uint32_t node)
	{
		const uint32_t acts = U32(U32(node, 0xC), 4);
		return U32(acts, 8);
	}
}
}
}

#ifdef FF8_FX_HELD
#include "mag062_devour_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace devour
{
	// 0x7E4C10 (MAG_062_sub_7E4C10; = Pain 0x865720): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the two queues (live task count -> node +0x5E)
	static uint32_t __cdecl Master(uint32_t a1)
	{
		g_mod = &MOD_062;

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x2601914) = 0x2793E58;
		MEM<uint32_t>(0x2601338) = 0x2793E58;
		states[0] = 0x7E4D30;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x7E4D40;
		states[2] = 0x7E4D50;
		states[3] = 0x7E4D60;
		states[4] = 0x7E57F0;
		states[5] = 0x7E5830;
		states[6] = 0x7E5840;
		states[7] = 0x7E5850;
		states[8] = 0x7E5860;
		states[9] = 0x7E5870;
		states[10] = 0x7E5890; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x26018F4);
			const uint32_t v2 = MEM<uint32_t>(0x2601778);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x2601334) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x26018F0);
			const uint32_t v2 = MEM<uint32_t>(0x2601774);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x2601334) = v2;
		}
		// 30 fps layer: see mag062_devour_held.inc
		FX_HELD(held_note_master();)
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_EMITTER));
		U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(Q_TASKS));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7E4D50 (MAG_062_sub_7E4D50; = Pain 0x865860): master state - wait until no child is alive, next state
	static uint32_t __cdecl WaitChildren(uint32_t a1)
	{
		if (U8(a1, 0x28) == 0)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7E4D90 (MAG_062_sub_7E4D90): EMITTER task (one per action) - bone-follow anchor and model bounds, state
	// {0x7E4E00 start the cast task, 0x7E5780 release the master, 0x7E57A0 action result, ret}
	static uint32_t __cdecl Emitter(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x7E4E00;
		states[1] = 0x7E5780;
		states[2] = 0x7E57A0;
		states[3] = 0x7E57E0; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7E4E00 (MAG_062_sub_7E4E00): emitter state 0 - starts the cast task 0x7E4E30 (0x54 bytes, task queue) under
	// the emitter, next state
	static uint32_t __cdecl StartCast(uint32_t a1)
	{
		x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Cast, 0x54, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7E5780 (MAG_062_sub_7E5780): emitter state 1 - at tick 0x4B releases the master (root +0x63 = 0: its action
	// loop may start the next emitter), next state
	static uint32_t __cdecl ReleaseMaster(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x4B)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x7E57A0 (MAG_062_sub_7E57A0): emitter state 2 - from tick 0x53: action result of its target (0x506690),
	// finished, next state
	static uint32_t __cdecl Damage(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x53)
			return 0; // void
		const int32_t action = S8(node, 0x2A);
		const int32_t target = S8(node, 0x2B);
		const uint32_t acts = U32(U32(node, 0xC), 4);
		const uint32_t targets = U32(acts + (uint32_t)(action * 20), 8);
		x::ApplyActionResultToTarget(targets + (uint32_t)(target * 24));
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x7E4E30 (MAG_062_sub_7E4E30): CAST task - state {0x7E4E90 textures, 0x7E4F10 start the parts, 0x7E5710
	// sound, ret}
	static uint32_t __cdecl Cast(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x7E4E90;
		states[1] = 0x7E4F10;
		states[2] = 0x7E5710;
		states[3] = 0x7E5770; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7E4E90 (sub_7E4E90): cast state 0 - uploads the title strip, picks a picture by rand() % 3 (+0x4E) and
	// uploads it (0: mag061_1, 1: mag061_2, 2: mag061_3), next state
	static uint32_t __cdecl LoadTextures(uint32_t a1)
	{
		x::Battle_QueueTIMUpload_GetEOF(MEM<uint32_t>(TIM_TITLE));
		const int32_t r = (int32_t)x::CrtRand();
		const uint32_t node = a1;
		const int16_t pick = (int16_t)(r % 3);
		S16(node, 0x4E) = pick;
		switch (pick)
		{
		case 0:
			x::Battle_QueueTIMUpload_GetEOF(MEM<uint32_t>(TIM_PIC1));
			break;
		case 1:
			x::Battle_QueueTIMUpload_GetEOF(MEM<uint32_t>(TIM_PIC2));
			break;
		case 2:
			x::Battle_QueueTIMUpload_GetEOF(MEM<uint32_t>(TIM_PIC3));
			break;
		}
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7E4F10 (sub_7E4F10): cast state 1 - starts the darkening 0x7E5290, the music fade 0x7E55F0 and the picture
	// 0x7E4F60 (0x54 bytes each, task queue) under the cast task, next state
	static uint32_t __cdecl StartParts(uint32_t a1)
	{
		x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Darken, 0x54, a1);
		x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Music, 0x54, a1);
		x::Effect_AddTaskAndInitFromCtx(Q_TASKS, ORIG_Picture, 0x54, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x7E5710 (sub_7E5710): cast state 2 - from tick 0x23: the sound of the outcome (the action's first target record
	// word +6, clamped to 0..8, indexes the table 0x15962B4), finished, next state
	static uint32_t __cdecl Sound(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x23)
			return 0; // void
		int16_t k = S16(first_target(node), 6);
		if (k < 0)
			k = 0;
		else if (k > 8)
			k = 8;
		x::BdPlaySE(MEM<uint32_t>(SOUND_TABLE + (uint32_t)((int32_t)k * 4)), 0, 0x80);
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// ---- picture

	// 0x7E5090 (sub_7E5090): one screen-space textured quad (POLY_FT4, 0x28 bytes) at the module packet cursor:
	// x, y, w, h on screen; texture page from VRAM (vx, vy) (8-bit, mode 1), uv (u, v) .. (u + w, v + h) clamped to
	// 0xFF, CLUT at (cx, cy), grey level `colour`, semi-transparent when `semi` == 1; OT bucket [0x1D8E04C] + 0x14
	static uint32_t Quad(uint32_t x, uint32_t y, uint32_t vx, uint32_t vy, uint32_t w, uint32_t h, uint32_t u, uint32_t v,
		uint32_t cx, uint32_t cy, uint32_t colour, uint32_t semi)
	{
		const uint32_t p = MEM<uint32_t>(PACKET_CURSOR);
		U8(p, 7) = 0x2C;
		U8(p, 7) = (uint8_t)((uint16_t)semi == 1 ? 0x2E : 0x2C);
		const int32_t vys = (int16_t)vy;
		const uint32_t tpage = ((((vx & 0x3C0) | 0x2800) >> 6) | ((uint32_t)(vys >> 4) & 0x10)) | ((uint32_t)(vys & 0x200) << 2);
		U32(p, 0) = 0x9000000;
		U16(p, 0x16) = (uint16_t)tpage;
		U16(p, 0xE) = (uint16_t)(((uint32_t)((int32_t)cx >> 4) & 0x3F) | (cy << 6));
		const uint8_t ub = (uint8_t)u;
		U8(p, 0x1C) = ub;
		U8(p, 0xC) = ub;
		if ((int32_t)(int16_t)w + (int32_t)(int16_t)u >= 0x100)
		{
			U8(p, 0x24) = 0xFF;
			U8(p, 0x14) = 0xFF;
		}
		else
		{
			const uint8_t u1 = (uint8_t)((uint8_t)w + ub);
			U8(p, 0x24) = u1;
			U8(p, 0x14) = u1;
		}
		const uint8_t vb = (uint8_t)v;
		U8(p, 0x15) = vb;
		U8(p, 0xD) = vb;
		if ((int32_t)(int16_t)h + (int32_t)(int16_t)v >= 0x100)
		{
			U8(p, 0x25) = 0xFF;
			U8(p, 0x1D) = 0xFF;
		}
		else
		{
			const uint8_t v1 = (uint8_t)((uint8_t)h + vb);
			U8(p, 0x25) = v1;
			U8(p, 0x1D) = v1;
		}
		U16(p, 0x18) = (uint16_t)x;
		U16(p, 8) = (uint16_t)x;
		U16(p, 0x20) = (uint16_t)(x + w);
		U16(p, 0x10) = (uint16_t)(x + w);
		U16(p, 0x12) = (uint16_t)y;
		U16(p, 0xA) = (uint16_t)y;
		U16(p, 0x22) = (uint16_t)(y + h);
		U16(p, 0x1A) = (uint16_t)(y + h);
		const uint8_t c = (uint8_t)colour;
		U8(p, 6) = c;
		U8(p, 5) = c;
		U8(p, 4) = c;
		x::SSIGPU_InsertPrimAltViewport(MEM<uint32_t>(0x1D8E04C) + 0x14, p);
		MEM<uint32_t>(PACKET_CURSOR) = p + 0x28;
		return 0; // void
	}

	// 0x7E5010 (sub_7E5010): the picture (VRAM (0x180, 0x100) 128 x 216 at x 0 and (0x1C0, 0x100) 192 x 216 at x
	// 0x80, CLUT (0x140, 0xF0)), opaque, brightness +0x50
	static uint32_t Picture(uint32_t node)
	{
		Quad(0, 0, 0x180, 0x100, 0x80, 0xD8, 0, 0, 0x140, 0xF0, U16(node, 0x50), 0);
		Quad(0x80, 0, 0x1C0, 0x100, 0xC0, 0xD8, 0, 0, 0x140, 0xF0, U16(node, 0x50), 0);
		return 0; // void
	}

	// 0x7E51C0 (sub_7E51C0): the title strip (VRAM (0x180, 0x100) 256 x 40 and (0x200, 0x100) 192 x 40 from v 0xD8,
	// CLUT (0x140, 0xF1)) at y 0x10, x = +0x1C + 0x60 (int16) and + 0x100, semi-transparent, brightness +0x50
	static uint32_t Strip(uint32_t node)
	{
		const uint16_t x = (uint16_t)(U16(node, 0x1C) + 0x60);
		Quad(x, 0x10, 0x180, 0x100, 0x100, 0x28, 0, 0xD8, 0x140, 0xF1, U16(node, 0x50), 1);
		Quad((uint16_t)(x + 0x100), 0x10, 0x200, 0x100, 0xC0, 0x28, 0, 0xD8, 0x140, 0xF1, U16(node, 0x50), 1);
		return 0; // void
	}

	// 0x7E4F60 (sub_7E4F60): PICTURE task - state {0x7E4FC0 wait, 0x7E4FE0 scroll, 0x7E5240 fade out, ret}
	static uint32_t __cdecl PictureTask(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x7E4FC0;
		states[1] = 0x7E4FE0;
		states[2] = 0x7E5240;
		states[3] = 0x7E5280; // nullsub (ret)
		const int8_t state = S8(node, 0x29);
		callp(states[state], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		const uint32_t r = task_end(node, status);
		// 30 fps layer: see mag062_devour_held.inc
		FX_HELD(held_note_picture(node, state, r);)
		return r;
	}

	// 0x7E4FC0 (sub_7E4FC0): picture state 0 - from tick 7: scroll +0x1C = 0, brightness +0x50 = 0x80, next state
	static uint32_t __cdecl PictureWait(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 7)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x1C) = 0;
			U16(a1, 0x50) = 0x80;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return a1;
	}

	// 0x7E4FE0 (sub_7E4FE0): picture state 1 - scroll -4, draws the strip and the picture; from tick 0x4B next state
	static uint32_t __cdecl PictureScroll(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) - 4);
		Strip(node);
		Picture(node);
		if (S16(node, 0x24) >= 0x4B)
			U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7E5240 (sub_7E5240): picture state 2 - brightness -0x10; at 0: finished (+0x50 = 0, nothing drawn), next
	// state; else scroll -4, draws the strip and the picture
	static uint32_t __cdecl PictureFade(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x50) = (uint16_t)(U16(node, 0x50) - 0x10);
		if (S16(node, 0x50) <= 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U16(node, 0x50) = 0;
			U8(node, 0x29) = (uint8_t)(st + 1);
			return 0; // void
		}
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) - 4);
		Strip(node);
		Picture(node);
		return 0; // void
	}

	// ---- darkening

	// 0x7E53B0 (sub_7E53B0): every visible entity (flag bit 2): flag |= 0x800, colour tint `level` (4.12) with
	// `mode` (0x7E53F0 = the library's 0x733950, engine port a_733950)
	static void TintAll(uint32_t level, uint32_t mode)
	{
		uint32_t e = ENTITIES;
		for (int k = 7; k != 0; k--, e += 0x9C)
		{
			uint16_t f = U16(e, 0);
			if ((f & 2) != 0)
			{
				f = (uint16_t)(f | 0x800);
				U16(e, 0) = f;
				a_733950(e, level, mode);
			}
		}
	}

	// target removal (0x7E5340 / 0x7E5560): unless the action's first target record word +6 is 8, the entity in slot
	// +0x2D gets flag 4 (byte +0: not drawn)
	static void HideTarget(uint32_t node)
	{
		if (U16(first_target(node), 6) == 8)
			return;
		const uint32_t slot = U8(node, 0x2D);
		U8(ENTITIES + slot * 0x9C, 0) |= 4;
	}

	// 0x7E5290 (sub_7E5290): DARKENING task - state {0x7E5300 keep flags, 0x7E5340 black, 0x7E54F0 wait, 0x7E5510
	// fade out, 0x7E5560 restore, ret}
	static uint32_t __cdecl Darken(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[6];
		states[0] = 0x7E5300;
		states[1] = 0x7E5340;
		states[2] = 0x7E54F0;
		states[3] = 0x7E5510;
		states[4] = 0x7E5560;
		states[5] = 0x7E55E0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7E5300 (sub_7E5300): darkening state 0 - from tick 10: the seven entities' flag words -> +0x30 + 4 * k
	// (zero-extended dwords), next state
	static uint32_t __cdecl DarkenKeep(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t e = ENTITIES;
		if (S16(node, 0x24) < 0xA)
			return node;
		for (int k = 0; k < 7; k++, e += 0x9C)
			U32(node, 0x30 + 4 * k) = U16(e, 0);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return node;
	}

	// 0x7E5340 (sub_7E5340): darkening state 1 - level +0x4C = 0x1000; the four fade layers black (colour bytes
	// +0x2C..+0x2E = 0) at level 0x1000; every visible entity tinted black; the target hidden; next state
	static uint32_t __cdecl DarkenIn(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x4C) = 0x1000;
		uint32_t p = FADE_LAYERS + 0x2D;
		for (int k = 4; k != 0; k--, p += 0x2C)
		{
			U8(p, 1) = 0;
			U8(p, 0) = 0;
			U8(p, -1) = 0;
			U16(p, -0x27) = 0x1000;
		}
		TintAll(0x1000, 0);
		HideTarget(node);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x7E54F0 (sub_7E54F0): darkening state 2 - after tick 0x53: level +0x4C = 0x1000, next state
	static uint32_t __cdecl DarkenWait(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x53)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x4C) = 0x1000;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return a1;
	}

	// 0x7E5510 (sub_7E5510): darkening state 3 - level -0x200 (at 0: next state), written to the four fade layers'
	// level words and the entities' tint
	static uint32_t __cdecl DarkenOut(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x4C) = (uint16_t)(U16(node, 0x4C) + 0xFE00);
		if (S16(node, 0x4C) <= 0)
		{
			const uint8_t st = U8(node, 0x29);
			U16(node, 0x4C) = 0;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(node, 0x4C);
		uint32_t p = FADE_LAYERS + 6;
		for (int k = 4; k != 0; k--, p += 0x2C)
			U16(p, 0) = level;
		TintAll(level, 0);
		return 0; // void
	}

	// 0x7E5560 (sub_7E5560): darkening state 4 - every visible entity gets the bits 0x804 of its kept flag word back;
	// the target hidden again; finished, next state
	static uint32_t __cdecl DarkenRestore(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t e = ENTITIES;
		uint32_t kept = node + 0x30;
		for (int k = 7; k != 0; k--, e += 0x9C, kept += 4)
		{
			const uint16_t f = U16(e, 0);
			if ((f & 2) != 0)
				U16(e, 0) = (uint16_t)(((U16(kept, 0) ^ f) & 0x804) ^ f);
		}
		HideTarget(node);
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// ---- music

	// 0x7E55F0 (sub_7E55F0): MUSIC task - state {0x7E5660 wait, 0x7E5680 down, 0x7E56B0 wait, 0x7E56C0 up, ret}
	static uint32_t __cdecl Music(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[5];
		states[0] = 0x7E5660;
		states[1] = 0x7E5680;
		states[2] = 0x7E56B0;
		states[3] = 0x7E56C0;
		states[4] = 0x7E5700; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x7E5660 (sub_7E5660): music state 0 - from tick 10: volume +0x4C = 0x7F, next state
	static uint32_t __cdecl MusicWait(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0xA)
		{
			const uint8_t st = U8(a1, 0x29);
			U16(a1, 0x4C) = 0x7F;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		return a1;
	}

	// 0x7E5680 (sub_7E5680): music state 1 - volume -8 (at 0: next state), Music_SetVolumeImmediate(0, volume)
	static uint32_t __cdecl MusicDown(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x4C) = (uint16_t)(U16(node, 0x4C) - 8);
		if (S16(node, 0x4C) <= 0)
		{
			const uint8_t st = U8(node, 0x29);
			U16(node, 0x4C) = 0;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return x::Music_SetVolumeImmediate(0, (uint32_t)(int32_t)S16(node, 0x4C));
	}

	// 0x7E56B0 (sub_7E56B0): music state 2 - after tick 0x53 next state
	static uint32_t __cdecl MusicWait2(uint32_t a1)
	{
		if (S16(a1, 0x24) > 0x53)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x7E56C0 (sub_7E56C0): music state 3 - volume +8 (at 0x7F: finished, next state), Music_SetVolumeImmediate(0,
	// volume)
	static uint32_t __cdecl MusicUp(uint32_t a1)
	{
		const uint32_t node = a1;
		U16(node, 0x4C) = (uint16_t)(U16(node, 0x4C) + 8);
		if (S16(node, 0x4C) >= 0x7F)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U16(node, 0x4C) = 0x7F;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return x::Music_SetVolumeImmediate(0, (uint32_t)(int32_t)S16(node, 0x4C));
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// the module's functions served by the engine's ports (identical code up to module addresses)
	static const ModPort ENGINE_PORTS[] = {
		{ 0x7E4D30, (void *)a_73A0D0, "062 MAG_062_sub_7E4D30" },
		{ 0x7E4D40, (void *)a_73A0D0, "062 MAG_062_sub_7E4D40" },
		{ 0x7E4D60, (void *)a_73A170, "062 MAG_062_sub_7E4D60" },
		{ 0x7E5280, (void *)a_73A6D0, "062 nullsub_1460" },
		{ 0x7E55E0, (void *)a_73A6D0, "062 nullsub_1461" },
		{ 0x7E5700, (void *)a_73A6D0, "062 nullsub_1462" },
		{ 0x7E5770, (void *)a_73A6D0, "062 nullsub_1459" },
		{ 0x7E57E0, (void *)a_73A6D0, "062 nullsub_1458" },
		{ 0x7E57F0, (void *)a_747500, "062 MAG_062_sub_7E57F0" },
		{ 0x7E5830, (void *)a_73A0D0, "062 MAG_062_sub_7E5830" },
		{ 0x7E5840, (void *)a_747550, "062 MAG_062_sub_7E5840" },
		{ 0x7E5850, (void *)a_73A0D0, "062 MAG_062_sub_7E5850" },
		{ 0x7E5860, (void *)a_73A0D0, "062 MAG_062_sub_7E5860" },
		{ 0x7E5870, (void *)a_7475A0, "062 MAG_062_sub_7E5870" },
		{ 0x7E5890, (void *)a_73A6D0, "062 nullsub_1457" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ ORIG_Master, (void *)Master, "062 Devour Master" },
		{ 0x7E4D50, (void *)WaitChildren, "062 WaitChildren" },
		{ ORIG_Emitter, (void *)Emitter, "062 Emitter" },
		{ 0x7E4E00, (void *)StartCast, "062 StartCast" },
		{ 0x7E5780, (void *)ReleaseMaster, "062 ReleaseMaster" },
		{ 0x7E57A0, (void *)Damage, "062 Damage" },
		{ ORIG_Cast, (void *)Cast, "062 Cast" },
		{ 0x7E4E90, (void *)LoadTextures, "062 LoadTextures" },
		{ 0x7E4F10, (void *)StartParts, "062 StartParts" },
		{ 0x7E5710, (void *)Sound, "062 Sound" },
		{ ORIG_Picture, (void *)PictureTask, "062 Picture" },
		{ 0x7E4FC0, (void *)PictureWait, "062 PictureWait" },
		{ 0x7E4FE0, (void *)PictureScroll, "062 PictureScroll" },
		{ 0x7E5240, (void *)PictureFade, "062 PictureFade" },
		{ ORIG_Darken, (void *)Darken, "062 Darken" },
		{ 0x7E5300, (void *)DarkenKeep, "062 DarkenKeep" },
		{ 0x7E5340, (void *)DarkenIn, "062 DarkenIn" },
		{ 0x7E54F0, (void *)DarkenWait, "062 DarkenWait" },
		{ 0x7E5510, (void *)DarkenOut, "062 DarkenOut" },
		{ 0x7E5560, (void *)DarkenRestore, "062 DarkenRestore" },
		{ ORIG_Music, (void *)Music, "062 Music" },
		{ 0x7E5660, (void *)MusicWait, "062 MusicWait" },
		{ 0x7E5680, (void *)MusicDown, "062 MusicDown" },
		{ 0x7E56B0, (void *)MusicWait2, "062 MusicWait2" },
		{ 0x7E56C0, (void *)MusicUp, "062 MusicUp" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag062_devour()
	{
		act::register_module(62);
		for (const act::devour::ModPort *p = act::devour::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(62, p->addr, p->port, p->name);
		for (const act::devour::ModPort *p = act::devour::PORTS; p->addr; p++)
			act::register_module_port(62, p->addr, p->port, p->name);
		// 30 fps layer: see mag062_devour_held.inc
		FX_HELD(register_mag062_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag062_devour_held.inc"
#endif
