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

// The generic effect sprites of the hit-effect queue (engine code 0x570AC0..0x570CC0, outside every effect module): the
// dust a character raises when it runs, the hit flares / slash streaks on a target, the impact puffs of a hit, and every
// other sprite flipbook of the battle effect library's sequence table 0xC7E554 (32 sequences, InitEffectSequenceFromData
// format).
//
// How they start: the battle action sequence (AnimSeq opcodes 99 / B1 = footsteps, B0 / B4 = effect on the entity / on
// the current target, AnimSeq_DispatchActionOpcode 0x504BB0), ApplyActionResultToTarget (0x506690: sequence 21 on a hit)
// and the composite effect 0x56F9C0 queue a REQUEST task into the hit-effect queue (TASK_QUEUE 0x209FAA8) through
// CreateEffectTaskBoneType 0x570E60 / CreateEffectTaskDefaultType2 0x570F20 / createEffectTaskDefaultType 0x571050 /
// 0x570E00: node +0x1C = the definition (table index, z rotation, scale, palette), +0x20 = the table's sequence. The
// request (DispatchEffectCommand 0x570CC0, not ported: it draws nothing) resolves the position (entity default point,
// direct, bone, matrix) once, then adds EFFECT_FUNCTION_LIST[sequence header byte 3] (0xC7EFA0) to the same queue:
//   0 = 0x570BC0 the sequence's sprite (every table entry 0..31)                                  <- ported here
//   1 = 0x5707A0 camera shake, 2 = 0x56F9C0 composite effect, 3 = 0x5705A0 hit flash (prim model)  (original code)
//   4 = 0x570AC0 shared sequence 5 with palette 0x11 (table entry 33)                             <- ported here
//
//   Sprite 0x570BC0 - each tick: TransformCameraByShadowRotation at the node's point (scale (def byte 2 & 0x7F) << 8,
//     shadow offset = the request's 4th word, else scale / 16), then frame `frame` of the sequence through a 0xB4-byte
//     Field_Alloc header (InitEffectSequenceFromData 0x571C80 into the render list's OT + 0x44 and its packet cursor
//     0x1D8E054): header flags 3 (z rotation = def byte 1 << 4, x scale -1 when def byte 2 bit 7), | 0x10 with palette
//     select byte 0 = def byte 3 & 0x7F when def byte 3 bit 7. Then (unless the effects-paused bit 0x1D96A9C bit 0 is set:
//     draw only) frame + 1; it ends after the sequence's last frame (the frame table's next offset is negative).
//   Shared sprite 0x570AC0 - the same with table entry 5 (entry 0 when entry 5's header top byte is set), flags 0x13,
//     palette select byte 0 = 0x11.
// The task never moves: everything that moves is in the sequence's frames (EffectSprite_EmitFramePrims 0x571DC0, 20-byte
// prims: width, intensity, height, gpu code, u0, v0, clut, centre offset x / y, angle, flags / tpage (0xC00 = rotation
// and scale, bits 12..15 = palette select byte), scale x / y).
//
// Quirk (not observable): 0x570BC0 / 0x570AC0 load the definition bytes 1..3 with one dword read whose top byte is a
// never-written stack word; only its low two bytes are used.

#include "mag_common.h"

namespace ff8fx
{
namespace sprite
{
	using namespace eng;
	using namespace magc;

	static const uint32_t ORIG_Sprite = 0x570BC0;       // EFFECT_FUNCTION_LIST[0]: the definition's sequence
	static const uint32_t ORIG_SharedSprite = 0x570AC0; // EFFECT_FUNCTION_LIST[4]: shared sequence 5, palette 0x11

#pragma pack(push, 1)
	struct SpriteNode
	{
		TaskNode hdr;
		int16_t frame;      // +0x0C sequence frame (0 from the request)
		int16_t pad0E;
		int16_t pos[3];     // +0x10 the point the request resolved
		int16_t shade;      // +0x16 TransformCameraByShadowRotation offset (0: scale / 16)
		uint32_t source;    // +0x18 the request's source (entity, bone, matrix entity; unused here)
		uint8_t id;         // +0x1C definition byte 0: table index
		uint8_t rot;        // +0x1D z rotation, 1/256 turn
		uint8_t scale;      // +0x1E bits 0..6: scale in 1/16, bit 7: mirrored in x
		uint8_t palette;    // +0x1F bit 7: palette select byte 0 = bits 0..6
		const uint8_t *seq; // +0x20 the sequence (0x570BC0)
	};
#pragma pack(pop)
	static_assert(sizeof(SpriteNode) == 0x24, "sprite node");

	// battle_to_update_flags bit 0: effects paused (draw, do not update)
	inline bool Paused() { return (UpdateFlags() & 1) != 0; }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x1D8E054); }

	// the shared sprite's sequence: table 0xC7E554 entry 5, entry 0 when entry 5's header top byte is set
	static const uint8_t *SharedSequence()
	{
		const uint8_t *s = var<const uint8_t *>(0xC7E554 + 5 * 4);
		if (*(const uint32_t *)s & 0xFF000000) s = var<const uint8_t *>(0xC7E554);
		return s;
	}

	// frame `frame` of `seq` at the node's point; returns the frame table's next offset (negative: the last frame)
	static int16_t Draw(const SpriteNode *s, const void *seq, int16_t frame, bool shared)
	{
		const int32_t size = (int32_t)(s->scale & 0x7F) << 8;
		int32_t shade = s->shade;
		if (shade == 0) shade = size >> 4;
		TransformCameraByShadowRotation(s->pos, size, -shade);
		uint8_t *h = AllocHeader(0xB4);
		*(const void **)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(int32_t *)(h + 8) = (int32_t)s->rot << 4;                     // z rotation (flag 1)
		*(int32_t *)(h + 0xC) = (s->scale & 0x80) ? -0x1000 : 0x1000;   // x scale (flag 2): mirrored
		*(int32_t *)(h + 0x10) = 0x1000;                                // y scale
		if (shared)
		{
			h[0x20] = 0x11;
			*(uint16_t *)(h + 0x24) = 0x13;
		}
		else
		{
			*(uint16_t *)(h + 0x24) = 3;
			if (s->palette & 0x80)
			{
				*(uint16_t *)(h + 0x24) = 0x13; // palette select byte 0 from the definition
				h[0x20] = (uint8_t)(s->palette & 0x7F);
			}
		}
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		const int16_t next = *(const int16_t *)(h + 0x28);
		FieldFree(0xB4);
		return next;
	}
}
}

#ifdef FF8_FX_HELD
#include "fx_sprite_held.h"
#endif

namespace ff8fx
{
namespace sprite
{
	// draw, then (not paused) the next frame; the task ends after the last frame
	static uint32_t Tick(TaskNode *n, bool shared)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see fx_sprite_held.inc
		FX_HELD(held_note_draw(shared ? ORIG_SharedSprite : ORIG_Sprite, s);)
		const int16_t next = Draw(s, shared ? SharedSequence() : s->seq, s->frame, shared);
		FX_HELD(held_note_drawn(s);)
		if (Paused()) return 0;
		s->frame++;
		return next < 0 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Sprite (0x570BC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SpriteTask(TaskNode *n) { return Tick(n, false); }

	// ------------------------------------------------------------------
	// Shared sprite (0x570AC0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SharedSpriteTask(TaskNode *n) { return Tick(n, true); }
}

	void register_fx_sprite()
	{
		register_port(sprite::ORIG_Sprite, (void *)sprite::SpriteTask, "sprite 570BC0 BattleFx_TickSpriteEffect", 0);
		register_port(sprite::ORIG_SharedSprite, (void *)sprite::SharedSpriteTask, "sprite 570AC0 BattleFx_TickSharedSpriteEffect", 0);
		// 30 fps layer: see fx_sprite_held.inc
		FX_HELD(register_fx_sprite_held();)
	}
}

#ifdef FF8_FX_HELD
#include "fx_sprite_held.inc"
#endif
