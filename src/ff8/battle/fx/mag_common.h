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

// Helpers shared by the ported magic (spell) modules: the cast context and battle action data
// every spell reads, the battle entities, and typed wrappers of the engine functions the spell
// modules call (original addresses, FF8_EN 1.2 US).
//
// Spell modules are compiled like the GF modules (one source file per spell, own globals), but
// they run in the battle's draw-only mode switch: every task tests
// battle_to_update_flags (0x1D96A9C) & 0x201 and then only draws.

#pragma once

#include "fx_port.h"

namespace ff8fx
{
namespace magc
{
	using namespace eng;

#pragma pack(push, 1)
	// BattleActionTaskData (20 bytes): one action of the cast
	struct ActionData
	{
		uint8_t attacker;      // +0x00 caster slot
		uint8_t command;       // +0x01
		uint8_t pad02[4];
		uint16_t effect_id;    // +0x06
		uint8_t *targets;      // +0x08 target records, 24 bytes each, +0 = target slot
		uint8_t pad0C[4];
		uint8_t target_count;  // +0x10
		uint8_t last_action;   // +0x11 (read from action 0) number of actions - 1
		uint8_t pad12[2];
	};
	// g_MagicCastContext (0x1D99A78, 12 bytes): what the spell's setup function receives
	struct CastContext
	{
		uint8_t attacker;      // +0x00
		uint8_t flags;         // +0x01 bit0 = no camera animation / texture upload, bit1 = no texture restore (Fira)
		uint16_t target_mask;  // +0x02
		ActionData *actions;   // +0x04
		uint8_t sequence_mode, done, submode, pad;
	};
#pragma pack(pop)
	static_assert(sizeof(ActionData) == 20, "BattleActionTaskData is 20 bytes");
	static_assert(sizeof(CastContext) == 12, "MagicCastContext is 12 bytes");

	const int TARGET_STRIDE = 24;

	// battle_to_update_flags: bit0 / bit9 = draw-only mode (tasks draw, do not update)
	inline uint32_t UpdateFlags() { return var<uint32_t>(0x1D96A9C); }
	inline bool DrawOnly() { return (UpdateFlags() & 0x201) != 0; }

	// battle entity slots (0x9C bytes each): +0x24 height, +0x26 size
	inline uint8_t *Entity(int slot) { return (uint8_t *)(0x1D972C0 + 0x9C * slot); }
	inline int16_t EntityHeight(int slot) { return *(int16_t *)(Entity(slot) + 0x24); }
	inline int16_t EntitySize(int slot) { return *(int16_t *)(Entity(slot) + 0x26); }

	// ---- engine functions the spell modules call
	inline int32_t CrtRand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); } // rand()
	// effect anchor of an entity: x, y, z (middle of its two effect bones) and its height
	inline void GetDefaultEffectPosition(uint8_t *entity, int16_t out[4]) { fn<void (__cdecl *)(uint8_t *, int16_t *)>(0x571400)(entity, out); }
	inline void BdPlaySE(const void *sound, int32_t a, int32_t b) { fn<void (__cdecl *)(const void *, int32_t, int32_t)>(0x501330)(sound, a, b); }
	inline void BdPlaySE3D(const void *sound, int32_t volume, const int16_t *pos) { fn<void (__cdecl *)(const void *, int32_t, const int16_t *)>(0x5013A0)(sound, volume, pos); }
	inline void ApplyActionResultToTargets(uint8_t *targets, uint32_t count) { fn<void (__cdecl *)(uint8_t *, uint32_t)>(0x506BA0)(targets, count); }
	inline void ApplyActionResultToTarget(uint8_t *target) { fn<void (__cdecl *)(uint8_t *)>(0x506690)(target); }
	// au_re_BdLinkTask_6: full-screen fade task in the hit-effect queue 0x209FAA8
	inline void ScreenFadeTask(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x5712C0)(a, b, c, d); }
	// sub_508630: anim-seq task that restores the battle textures; *done becomes non-zero when finished
	inline void TextureRestoreTask(void *tex, void *done) { fn<void (__cdecl *)(void *, void *)>(0x508630)(tex, done); }
	// matrices / vectors (VECTOR = 3 x int32, SVECTOR = 3 x int16)
	inline void Scale3DMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(0x56BEF0)(m, v); }
	inline void ComposeZYXRotationMatrix(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x56CE30)(angles, out); }
	inline void NormalizeVector(const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); }
	inline void CrossProduct(const int32_t *a, const int32_t *b, int32_t *out) { fn<void (__cdecl *)(const int32_t *, const int32_t *, int32_t *)>(0x56BBF0)(a, b, out); }
	inline void BuildAxisAngleRotationMatrix(int32_t angle, Mat4x3 *out, const int32_t *axis) { fn<void (__cdecl *)(int32_t, Mat4x3 *, const int32_t *)>(0x5714F0)(angle, out, axis); }
	inline void TransformVectorBy3x3Matrix(const Mat4x3 *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int32_t *, int32_t *)>(0x56C600)(m, in, out); }
	// rotation taking a onto b: returns the angle, writes the unit axis
	inline int32_t RotationBetweenVectors(const int32_t *a, const int32_t *b, int32_t *axis) { return fn<int32_t (__cdecl *)(const int32_t *, const int32_t *, int32_t *)>(0x571480)(a, b, axis); }
	// shared effect sprite sequences (flipbooks) of the battle effect library
	inline void *EffectSpriteSharedSequence(int id) { return fn<void *(__cdecl *)(int)>(0x5710B0)(id); }

}
}

#ifdef FF8_FX_HELD
#include "mag_common_held.h"
#endif

namespace ff8fx
{
namespace magc
{
	// draw headers live on the Field_Alloc scratch stack (sprite sequence 0xB4 bytes, prim model
	// 0x58 bytes); fields a draw does not set keep what the scratch held
	static inline uint8_t *AllocHeader(int size)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(size);
		// 30 fps layer: see mag_common_held.h
		FX_HELD(held_header(h, size);)
		return h;
	}
}
}
