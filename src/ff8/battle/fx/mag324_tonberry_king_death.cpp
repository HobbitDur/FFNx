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

// Effect 324: Tonberry King's death (Tonberry King c0m083; MAG_324_*): the King fades out in a swarm of rising sparks while
// a ghost of him (his pose and place when it started) fades to black, two spinning sprites at his chest, a voice clip.
//
// Structure (setup 0x596BC0 (not ported): keeps the context (0x2203C48), the target slot (0x22010A0) and the CASTER slot
// (0x2203C4C), the root in its 1-node pool 0x2201AA8 and the cue in the task queue 0x2202E28 (0x64 nodes of 0x24 at
// 0x2202E38), frees the 0x64 cells of the spark table 0x2201AC8, starts camera animation 0xCD09F0 and queues the TIM of
// the file loader 0x596BA0 (the texture file named at 0xCD1278 -> [0x2203C50])):
//   RootTask (0x5979A0, = Chain Gun 190's root) - the packet arena 0x2213C60 on odd counters, else 0x220BC60 (the cursor
//     0x221BC60); runs the task queue; ends when it is empty.
//   CueTask (0x596C70) - every tick (also draw-only ticks) the caster's bone 0xF0 point (0x502170, flag 0x1000) ->
//     0x2203C58; then, unless draw-only (battle_to_update_flags & 0x201): counter 0 the voice clip 0xCD1104 (0x4A29A0 ->
//     0x221BC64); 8 the tint task (on the caster slot); 0xC the two spark emitters; 0x12 the ghost task; odd counters
//     0x7D..0x87 a spin sprite (sides alternate); 1 the sound 0xCD1210 at the point; 0x82 the damage of action 0's first
//     target record; 0xC5 the voice released (if still playing); ends after counter 0xC8.
//   EmitterA (0x596E30) / EmitterB (0x597130) - up to counter 0x2E (not draw-only), two sparks per tick in free cells of
//     the table (A: flag 1, size rand % 0x600 - 0x28 c + 0x800, at a random caster bone (0x502170, flag rand % 0x1000) +-
//     (0xFA - 3 c), rising speed -5 - rand % 25; B: flag 2, size rand % 0x600 + 0x20 (0x18 - c), +- 3 (0x64 - c)); then
//     every cell of its flag drawn: sequence 0xCD0B88 (A) / 0xCD0D00 (B), frame = the cell's counter, the position moved
//     toward the camera by size / 8 (GTE light matrix = camera, 0x56BC50); unless draw-only: a finished sequence frees the
//     cell, else its counter steps (A: it rises). Ends when it drew no live cell.
//   TintTask (0x597410) - the caster's colour (+0x28): counters 0..3 from the battle's far colour (0xB8B7D8) to 0xCD1218[0]
//     (GTE DPCS), 4..0x29 the 24 colours of 0xCD1218 in turn, from 0x2A fading to black; its flags bit 11 (+0x01 bit 3) set
//     every tick, cleared at its end (counter 0x32).
//   GhostTask (0x597520) - counter 0: the caster's bone matrices (0x597750) and matrix (+0x40) kept, its +0x7C bit 1
//     cleared; then each tick: the caster drawn as kept (counters > 6: the kept matrix; its object mask 2; from counter
//     0x10 its colour fading to black, +0x2B = 2, +0x07 = 0 left behind) through 0x5977D0 (RenderGeometry mode 4 into the
//     frame arena), its bones / mask / colour / matrix put back; ends after counter 0x20.
//   SpinTask (0x597870) - sequence 0xCD0B2C (mode 8) at the bone point through the shadow-rotation camera (scale +0x1C =
//     0x800), turned 0x800 per side, 0xC8 to its side, 0x3C up; ends when its sequence is finished.
// DRAW-ONLY ticks (battle_to_update_flags & 0x201): every task draws, nothing advances (the cue only refreshes the point).
// DRAW ORDER: every task draws THEN updates.
// ENTITY / CAMERA: the CASTER's entity is written: colour +0x28, flags bit 11, +0x7C bit 1 (ghost start), +0x2B / +0x07
// (ghost fade), and (put back the same tick) its bone matrices, +0x40, +0x7C, +0x28 around the ghost draw; the camera
// animation is the engine's.
// Module globals: 0x2201080..0x2203C60 (kept matrix 0x2201080, target slot 0x22010A0, kept bones 0x22010A8, root queue /
// pool 0x2201AB8 / 0x2201AA8, spark table 0x2201AC8 (0x64 x 0x18), current bones 0x2202428, task queue / pool 0x2202E28 /
// 0x2202E38, context 0x2203C48, caster slot 0x2203C4C, texture file 0x2203C50, bone point 0x2203C58), the packet arenas
// 0x220BC60 / 0x2213C60 (0x8000 each), the cursor 0x221BC60 and the voice 0x221BC64.
// NEVER-WRITTEN STACK WORDS: none read (the spin task's arg word to 0x571BC0 is read as a word; the scratch blocks' unset
// words are never read).
// FUJIN: Fujin's described Tonberry King death (effect_324.json) runs this code: its memory through own::R() (fx_port.h),
// its spawns through own::Fn(), the file loader (0x596BA0) and the setup (0x596BC0) ported below; its parameters
// (TonkParams: the voice and the sound, the cue's ticks, the two spark emitters, the tint, the ghost, the spin sprites;
// the three flipbooks, the 24 tint colours, the texture name and the camera track). FX_PART tags its parts.

#include "mag_common.h"
#include "fx_params.h"
#include <stddef.h>

namespace ff8fx
{
namespace tonk324
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x2203C48)); }
	inline int32_t &CasterSlot() { return var<int32_t>(own::R(0x2203C4C)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x221BC60)); }
	inline uint32_t &Voice() { return var<uint32_t>(own::R(0x221BC64)); }
	inline int16_t *BonePoint() { return (int16_t *)own::R(0x2203C58); }           // the caster's bone 0xF0 (the cue, every tick)
	inline TaskQueue *QTasks() { return (TaskQueue *)own::R(0x2202E28); }          // pool 0x2202E38: 0x64 x 0x24
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	static const uint32_t KEPT_MATRIX = 0x2201080, KEPT_BONES = 0x22010A8, CUR_BONES = 0x2202428;
	static const uint32_t ARENA_A = 0x2213C60, ARENA_B = 0x220BC60;   // odd / even root counters

	static const uint32_t ORIG_RootTask = 0x5979A0;
	static const uint32_t ORIG_CueTask = 0x596C70;
	static const uint32_t ORIG_EmitterA = 0x596E30;
	static const uint32_t ORIG_EmitterB = 0x597130;
	static const uint32_t ORIG_TintTask = 0x597410;
	static const uint32_t ORIG_GhostTask = 0x597520;
	static const uint32_t ORIG_SpinTask = 0x597870;
	static const uint32_t SEQ_SparkA = 0xCD0B88, SEQ_SparkB = 0xCD0D00, SEQ_Spin = 0xCD0B2C; // sprite sequences
	static const uint32_t TINTS = 0xCD1218;              // 24 colour words
	static const void *const VOICE = (const void *)0xCD1104;
	static const void *const SOUND = (const void *)0xCD1210;
	static const int CELLS = 0x64;

	static const uint32_t ORIG_FileLoader = 0x596BA0, ORIG_Setup = 0x596BC0;
	static const uint32_t TEXTURE_NAME = 0xCD1278, CAMERA_ANIM = 0xCD09F0;
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x2203C50)); }      // the effect's file (its TIM)
	inline int32_t &TargetSlot() { return var<int32_t>(own::R(0x22010A0)); }     // the first target (setup)

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_SPARKS_A = 1, PART_SPARKS_B = 2, PART_GHOST = 3, PART_SPINS = 4 };
	struct TonkParams
	{
		struct { uint32_t voice, start; } sounds;         // the voice clip (a voice slot) and the sound at the caster
		// the cue's ticks: the voice, the sound, the tint, the emitters, the ghost, the spin sprites (odd ticks first..last),
		// the damage, the voice released, its end
		struct { int16_t voice, sound, tint, emitters, ghost, spin_first, spin_last, damage, release, end; } timing;
		// the emitters: new sparks while their tick <= ticks, count a tick each, at random caster bones, sized a random
		// below size_spread + their own size
		struct { int16_t ticks, count, size_spread; } sparks;
		// emitter A: size - shrink x tick, scatter +-(spread - narrow x tick), rising rise + a random below rise_spread
		struct { int16_t size, shrink, spread, narrow, rise, rise_spread; } a;
		struct { int16_t size, shrink, spread, narrow; } b; // emitter B: the same, not rising
		// the caster's tint: from the battle's colour to the first tint colour over `in` ticks, the 24 colours in turn,
		// from `out` faded to black by fade a tick, its end
		struct { int16_t in, out, fade, end; } tint;
		// the ghost: the caster's pose and place kept, drawn as kept (its place after `hold` ticks), faded to black from
		// fade_start by fade a tick, its end
		struct { int16_t hold, fade_start, fade, end; } ghost;
		// the spin sprites at the caster's point: size, turn per side, side offset, height
		struct { int16_t size, turn, side, up; } spins;
		struct { uint32_t spark_a, spark_b, spin; } seqs; // the flipbooks
		struct { uint32_t tints; } tables;                // the 24 tint colours
		struct { uint32_t texture_name, camera; } res;    // its resources (data): texture file name, camera track
	};
	static const TonkParams GAME = { { 0xCD1104, 0xCD1210 }, { 0, 1, 8, 0xC, 0x12, 0x7D, 0x87, 0x82, 0xC5, 0xC8 },
		{ 0x2E, 2, 0x600 }, { 0x800, 0x28, 0xFA, 3, 5, 0x19 }, { 0x300, 0x20, 0x12C, 3 }, { 4, 0x2A, 0x200, 0x32 },
		{ 6, 0x10, 0x100, 0x20 }, { 0x800, 0x800, 0xC8, 0x3C }, { SEQ_SparkA, SEQ_SparkB, SEQ_Spin }, { TINTS },
		{ TEXTURE_NAME, CAMERA_ANIM } };
	static TonkParams live = GAME;
	static const TonkParams &PG() { return live; }
	inline uint32_t Tints() { return fxp::data(PG().tables.tints, TINTS, 0x60); }

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t VoiceActive(uint32_t v) { return fn<int32_t (__cdecl *)(uint32_t)>(0x4A2900)(v); }        // BdSound_IsVoiceSlotActive
	inline void ReleaseVoiceSlot(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(v); }               // BdSound_ReleaseVoiceSlot
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); }
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteSetBackFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); }      // BK = m translation
	inline void GteMVMVA_LightV0_Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }                              // MAC = L * V0 + BK
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }                   // R = s * I
	inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); } // TR = v
	inline uint32_t RenderGeometry(uint32_t group, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(group, h, ot, mode, cursor); }
	// TransformCameraByShadowRotation (0x571BC0) returns its matrix (0x21DFED0)
	inline Mat4x3 *ShadowCamera(const int16_t *pos, int32_t a, int32_t b) { return fn<Mat4x3 *(__cdecl *)(const int16_t *, int32_t, int32_t)>(0x571BC0)(pos, a, b); }
	inline uint32_t &FrameArena() { return var<uint32_t>(0x1D8E054); }    // battle_texture_data_ptr (RenderGeometry packets)

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes (0x2201AA8)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C its parity picks the packet arena
		int16_t pad0E;
	};
	struct TaskBody // the task queue's nodes (0x24 bytes)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t word0E;    // +0x0E cue: spin side toggle; tint: the caster slot; spin: its side
		uint8_t pad10[0xC];
		int16_t word1C;    // +0x1C tint: colour index; spin: scale (0x800)
		uint8_t pad1E[6];
	};
	struct Cell // the spark table (0x64 cells of 0x18 bytes)
	{
		uint32_t flag;     // +0x00 1 = emitter A's, 2 = B's, 0 = free
		int16_t counter;   // +0x04 sequence frame
		int16_t size;      // +0x06 toward the camera by size / 8
		int16_t pos[3];    // +0x08
		int16_t pad0E[2];
		int16_t vy;        // +0x12 emitter A: rising speed
		int16_t pad14[2];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(TaskBody) == 0x24 && sizeof(Cell) == 0x18, "Tonberry King death nodes");
	inline Cell *Cells() { return (Cell *)own::R(0x2201AC8); }

	// ------------------------------------------------------------------
	// Shared pieces
	// ------------------------------------------------------------------
	// 0x597750: the skeleton's bone matrices (record + 0x10, 0x20 bytes each) -> dst
	static void BonesOut(uint8_t *anim, uint8_t *dst)
	{
		uint8_t *sk = **(uint8_t ***)(anim + 4);
		int32_t n = sk[0];
		if (n <= 0) return;
		uint8_t *src = sk + 0x20;
		for (; n != 0; n--, src += 0x30, dst += 0x20) memcpy(dst, src, 0x20);
	}

	// 0x597790: src -> the skeleton's bone matrices
	static void BonesIn(uint8_t *anim, const uint8_t *src)
	{
		uint8_t *sk = **(uint8_t ***)(anim + 4);
		if (sk[0] == 0) return;
		uint8_t *dst = sk + 0x20;
		for (int32_t i = 0; i < sk[0]; i++, src += 0x20, dst += 0x30) memcpy(dst, src, 0x20);
	}

	// 0x5977D0 (= Degenerator 174's 0x5D5120): the entity's model through its current bones, camera x its matrix
	// (RenderGeometry mode 4 into the frame arena)
	static void ModelDraw(uint8_t *ent)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x4C);
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(ent + 0x40), (Mat4x3 *)w);
		ComputeBonesWorldMatrices(ent + 0x60, w);
		*(uint32_t *)(w + 0x24) = var<uint32_t>(0x1D98B3C);
		*(int16_t *)(w + 0x34) = 0;
		*(int16_t *)(w + 0x36) = 0;
		*(uint32_t *)(w + 0x3C) = *(const uint32_t *)(ent + 0x28);
		*(uint32_t *)(w + 0x30) = var<uint32_t>(0x1D969A8);
		w[0x4A] = ent[7];
		w[0x49] = ent[7];
		w[0x48] = ent[7];
		*(int16_t *)(w + 0x38) = 0x140;
		*(int16_t *)(w + 0x3A) = 0xD8;
		*(uint32_t *)(w + 0x40) = *(const uint32_t *)(ent + 0x7C);
		FrameArena() = RenderGeometry(*(const uint32_t *)(ent + 0x64), w + 0x20, RenderOT(), 4, FrameArena());
		FieldFree(0x4C);
	}

	// the emitters' draw (0x596FC0..0x5970E3 / 0x59729F..0x5973BA): every cell of flag `bit`, sequence seq, frame = its
	// counter, moved toward the camera (light matrix = camera, BK = camera translation) by size / 8; unless draw-only, a
	// finished sequence frees its cell, else the cell's counter steps (rise: + vy). Returns the live cells drawn.
	static int32_t SparkDraw(uint8_t *w, Cell *cells, uint32_t bit, uint32_t seq, bool rise)
	{
		uint8_t *h = AllocHeader(0xB4);
		*(int16_t *)(h + 0x24) = 0;
		int32_t live = 0;
		memcpy(w + 8, &Camera(), 0x20);
		*(uint32_t *)h = seq;
		GteSetLightMatrix(w + 8);
		GteSetBackFromTrans(w + 8);
		for (int i = 0; i < CELLS; i++)
		{
			Cell &c = cells[i];
			if (!((uint8_t)c.flag & bit)) continue;
			GteLoadV0(c.pos);
			GteMVMVA_LightV0_Bk();
			GteSetRotScale(c.size);
			*(int16_t *)(h + 4) = c.counter;
			int32_t *mac = (int32_t *)(w + 0x38), *dir = (int32_t *)(w + 0x28);
			GteReadMAC123(mac);
			NormalizeVector(mac, dir);
			const int32_t k = 0 - (c.size >> 3);
			mac[0] += mul32(dir[0], k) >> 12;
			mac[1] += mul32(dir[1], k) >> 12;
			mac[2] += mul32(dir[2], k) >> 12;
			GteSetTransFromVec32(mac);
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0) c.flag = 0;
			else
			{
				c.counter++;
				if (rise) c.pos[1] = (int16_t)(c.pos[1] + c.vy);
				live++;
			}
		}
		FieldFree(0xB4);
		return live;
	}

	// the spin sprite's draw (0x597870..0x597960)
	static const uint8_t *SpinDraw(const TaskBody *t, const int16_t *point)
	{
		int16_t ang[3] = { 0, (int16_t)(t->word0E * PG().spins.turn), 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		const int32_t sc[3] = { t->word1C, t->word1C, t->word1C };
		Scale3DMatrix(&m, sc);
		const Mat4x3 *p = ShadowCamera(point, t->word1C, 0 - (t->word1C >> 4));
		m.t[0] = (t->word0E ? PG().spins.side : -PG().spins.side) + p->t[0];
		m.t[1] = p->t[1] - PG().spins.up;
		m.t[2] = p->t[2];
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = fxp::sequence(PG().seqs.spin, SEQ_Spin);
		*(int16_t *)(h + 4) = t->counter;
		*(int16_t *)(h + 0x24) = 8;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
		return h; // (its +0x28, the sequence's end mark, is read after the free)
	}
}
}

#ifdef FF8_FX_HELD
#include "mag324_tonberry_king_death_held.h"
#endif

namespace ff8fx
{
namespace tonk324
{
	// ------------------------------------------------------------------
	// Spin sprite (0x597870)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SpinTask(TaskNode *n)
	{
		TaskBody *t = (TaskBody *)n;
		// 30 fps layer: see mag324_tonberry_king_death_held.inc
		FX_HELD(held_note_spin(n);)
		FX_PART(PART_SPINS);
		const uint8_t *h = SpinDraw(t, BonePoint());
		if (DrawOnly()) return 0;
		t->counter++;
		return *(const int16_t *)(h + 0x28) < 0 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ghost (0x597520)
	// ------------------------------------------------------------------
	static uint32_t __cdecl GhostTask(TaskNode *n)
	{
		TaskBody *t = (TaskBody *)n;
		if (t->counter == 0)
		{
			BonesOut(Caster() + 0x60, (uint8_t *)own::R(KEPT_BONES));
			uint8_t *e = Caster();
			*(uint32_t *)(e + 0x7C) &= ~2u;
			memcpy((void *)own::R(KEPT_MATRIX), e + 0x40, 0x20);
		}
		BonesOut(Caster() + 0x60, (uint8_t *)own::R(CUR_BONES));
		uint8_t *e = Caster();
		const uint32_t tint = *(const uint32_t *)(e + 0x28);
		const uint32_t mask = *(const uint32_t *)(e + 0x7C);
		uint8_t live[0x20];
		if (t->counter > PG().ghost.hold)
		{
			memcpy(live, e + 0x40, 0x20);
			memcpy(e + 0x40, (const void *)own::R(KEPT_MATRIX), 0x20);
		}
		*(uint32_t *)(e + 0x7C) = 2;
		BonesIn(e + 0x60, (const uint8_t *)own::R(KEPT_BONES));
		if (t->counter >= PG().ghost.fade_start)
		{
			GteSetFarColor(0, 0, 0);
			GteLoadRGBC(e + 0x28);
			GteSetIR0((t->counter - PG().ghost.fade_start) * PG().ghost.fade);
			GteDPCS();
			GteStoreRGB2(e + 0x28);
			e[0x2B] = 2;
			e[7] = 0;
		}
		// 30 fps layer: see mag324_tonberry_king_death_held.inc
		FX_HELD(held_note_ghost(n);)
		FX_PART(PART_GHOST);
		ModelDraw(e);
		BonesIn(Caster() + 0x60, (const uint8_t *)own::R(CUR_BONES));
		e = Caster();
		const int16_t c = t->counter;
		*(uint32_t *)(e + 0x7C) = mask;
		*(uint32_t *)(e + 0x28) = tint;
		if (c > PG().ghost.hold) memcpy(e + 0x40, live, 0x20);
		if (DrawOnly()) return 0;
		t->counter = (int16_t)(c + 1);
		return t->counter > PG().ghost.end ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Tint (0x597410)
	// ------------------------------------------------------------------
	// the tint's colour of counter c (0x597417..0x5974CD) -> e + 0x28
	static void TintColour(TaskBody *t, uint8_t *e, int16_t c)
	{
		const TonkParams &g = PG();
		if (c < g.tint.in)
		{
			const uint32_t far = var<uint32_t>(0xB8B7D8);
			GteSetFarColor(far & 0xFF, (far >> 8) & 0xFF, var<uint8_t>(0xB8B7DA));
			GteLoadRGBC((const void *)Tints());
			GteSetIR0(((g.tint.in - c) << 12) / g.tint.in);
			GteDPCS();
			GteStoreRGB2(e + 0x28);
		}
		else if (c >= g.tint.out)
		{
			GteSetFarColor(0, 0, 0);
			GteLoadRGBC((const void *)(Tints() + t->word1C * 4));
			GteSetIR0((c - g.tint.out) * g.tint.fade);
			GteDPCS();
			GteStoreRGB2(e + 0x28);
		}
		else
		{
			const int32_t idx = (c - g.tint.in) % 0x18;
			t->word1C = (int16_t)idx;
			*(uint32_t *)(e + 0x28) = *(const uint32_t *)(Tints() + idx * 4);
		}
	}

	static uint32_t __cdecl TintTask(TaskNode *n)
	{
		TaskBody *t = (TaskBody *)n;
		const int16_t c = t->counter;
		uint8_t *e = Entity(t->word0E);
		TintColour(t, e, c);
		e[1] |= 8;
		const uint16_t flags = *(const uint16_t *)e;
		if (DrawOnly()) return 0;
		t->counter++;
		if (t->counter < PG().tint.end) return 0;
		*(uint16_t *)e = (uint16_t)(flags & 0xF7FF);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Emitters (0x596E30 / 0x597130)
	// ------------------------------------------------------------------
	static uint32_t __cdecl EmitterA(TaskNode *n)
	{
		TaskBody *t = (TaskBody *)n;
		const TonkParams &g = PG();
		uint8_t *w = (uint8_t *)FieldAlloc(0x48);
		if (!DrawOnly() && t->counter <= g.sparks.ticks)
		{
			const int32_t nb = **(const uint8_t *const *)*(uint8_t *const *)(Caster() + 0x64);
			for (int i = 0; i < g.sparks.count; i++)
			{
				int k = 0;
				while (k < CELLS && Cells()[k].flag != 0) k++;
				if (k >= CELLS) break;
				Cell &c = Cells()[k];
				c.flag = 1;
				c.counter = 0;
				const int32_t r0 = CrtRand() % g.sparks.size_spread;
				c.size = (int16_t)(r0 - (int16_t)(t->counter * g.a.shrink) + g.a.size);
				const int32_t bone = nb ? CrtRand() % nb : 0;
				GetEffectSpawnPosition(Caster(), bone, CrtRand() % 0x1000, c.pos);
				int32_t s = g.a.spread - t->counter * g.a.narrow;
				if (s < 1) s = 1;   // (a scatter of at least 1)
				const int32_t range = s + s;
				c.pos[0] = (int16_t)(c.pos[0] + (CrtRand() % range - s));
				c.pos[1] = (int16_t)(c.pos[1] + (CrtRand() % range - s));
				c.pos[2] = (int16_t)(c.pos[2] + (CrtRand() % range - s));
				c.vy = (int16_t)(-g.a.rise - CrtRand() % g.a.rise_spread);
			}
		}
		// 30 fps layer: see mag324_tonberry_king_death_held.inc
		FX_HELD(held_note_sparks(n, 1);)
		FX_PART(PART_SPARKS_A);
		const int32_t live = SparkDraw(w, Cells(), 1, fxp::sequence(g.seqs.spark_a, SEQ_SparkA), true);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		t->counter++;
		return live ? 0 : TASK_END;
	}

	static uint32_t __cdecl EmitterB(TaskNode *n)
	{
		TaskBody *t = (TaskBody *)n;
		const TonkParams &g = PG();
		uint8_t *w = (uint8_t *)FieldAlloc(0x48);
		if (!DrawOnly() && t->counter <= g.sparks.ticks)
		{
			const int32_t nb = **(const uint8_t *const *)*(uint8_t *const *)(Caster() + 0x64);
			for (int i = 0; i < g.sparks.count; i++)
			{
				int k = 0;
				while (k < CELLS && Cells()[k].flag != 0) k++;
				if (k >= CELLS) break;
				Cell &c = Cells()[k];
				c.flag = 2;
				c.counter = 0;
				const int32_t r0 = CrtRand() % g.sparks.size_spread;
				c.size = (int16_t)(r0 + (int16_t)(g.b.size - t->counter * g.b.shrink));
				const int32_t bone = nb ? CrtRand() % nb : 0;
				GetEffectSpawnPosition(Caster(), bone, CrtRand() % 0x1000, c.pos);
				int32_t s = g.b.spread - t->counter * g.b.narrow;
				if (s < 1) s = 1;   // (a scatter of at least 1)
				const int32_t range = s + s;
				c.pos[0] = (int16_t)(c.pos[0] + (CrtRand() % range - s));
				c.pos[1] = (int16_t)(c.pos[1] + (CrtRand() % range - s));
				c.pos[2] = (int16_t)(c.pos[2] + (CrtRand() % range - s));
			}
		}
		FX_HELD(held_note_sparks(n, 2);)
		FX_PART(PART_SPARKS_B);
		const int32_t live = SparkDraw(w, Cells(), 2, fxp::sequence(g.seqs.spark_b, SEQ_SparkB), false);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		t->counter++;
		return live ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Cue (0x596C70)
	// ------------------------------------------------------------------
	static TaskBody *AddTask(uint32_t fn) { return (TaskBody *)AddTaskToQueue(QTasks(), own::Fn(fn)); }

	static uint32_t __cdecl CueTask(TaskNode *n)
	{
		TaskBody *t = (TaskBody *)n;
		GetEffectSpawnPosition(Caster(), 0xF0, 0x1000, BonePoint());
		if (DrawOnly()) return 0;
		const TonkParams &g = PG();
		if (t->counter == g.timing.voice) Voice() = ClaimVoiceSlot((const void *)fxp::sound(g.sounds.voice, (uint32_t)VOICE), 1, 0x80);
		if (t->counter == g.timing.emitters)
		{
			AddTask(ORIG_EmitterA)->counter = 0;
			t->word0E = 0;
			AddTask(ORIG_EmitterB)->counter = 0;
		}
		if (t->counter == g.timing.tint)
		{
			TaskBody *a = AddTask(ORIG_TintTask);
			a->counter = 0;
			a->word0E = (int16_t)CasterSlot();
		}
		if (t->counter == g.timing.ghost) AddTask(ORIG_GhostTask)->counter = 0;
		if (t->counter >= g.timing.spin_first && t->counter <= g.timing.spin_last && t->counter % 2 == 1)
		{
			TaskBody *a = AddTask(ORIG_SpinTask);
			const int16_t side = t->word0E;
			a->counter = 0;
			a->word0E = (int16_t)(side & 1);
			a->word1C = g.spins.size;
			t->word0E = (int16_t)(side + 1);
		}
		if (t->counter == g.timing.damage) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (t->counter == g.timing.sound) BdPlaySE3D((const void *)fxp::sound(g.sounds.start, (uint32_t)SOUND), 0, BonePoint());
		if (t->counter == g.timing.release && VoiceActive(Voice())) ReleaseVoiceSlot(Voice());
		t->counter++;
		return t->counter > g.timing.end ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root (0x5979A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag324_tonberry_king_death_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = own::R(ARENA_A);
		if (!(*(const uint8_t *)&r->counter & 1)) PacketCursor() = own::R(ARENA_B);
		const int a = ExecuteTaskQueue(QTasks());
		r->counter++;
		return a ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// File loader (0x596BA0) and setup (0x596BC0): the game runs its own; Fujin's programs these (its memory may be a
	// copy's): the context, the target and caster slots, the root and the cue, the spark table cleared, the camera, the TIM
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(PG().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		Ctx() = ctx;
		TargetSlot() = ctx->actions[0].targets[0];
		CasterSlot() = ctx->attacker;
		TaskQueue *root = (TaskQueue *)own::R(0x2201AB8);
		InitTaskQueuePool(root, (void *)own::R(0x2201AA8), 0x10, 1);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(root, own::Fn(ORIG_RootTask)) + 0xC) = 0;
		InitTaskQueuePool(QTasks(), (void *)own::R(0x2202E38), 0x24, 100);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(QTasks(), own::Fn(ORIG_CueTask)) + 0xC) = 0;
		for (uint32_t a = (uint32_t)Cells(); a < own::R(CUR_BONES); a += 0x18) *(int32_t *)a = 0;
		PlayCameraAnimation((const void *)fxp::camera(PG().res.camera, CAMERA_ANIM));
		QueueTimUpload(TexFile());
		return root;
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "main", "Cue", "The cue's ticks, the voice, the sound, the damage; the texture and the camera" },
			{ PART_SPARKS_A, "sparks_a", "Rising sparks", "Emitter A: sparks at random bones of the caster, shrinking, rising (a flipbook)" },
			{ PART_SPARKS_B, "sparks_b", "Sparks", "Emitter B: sparks at random bones of the caster, shrinking (a flipbook)" },
			{ 0, "tint", "Tint", "The caster's colour: from the battle's to the tint colours, then faded to black" },
			{ PART_GHOST, "ghost", "Ghost", "The caster drawn in its pose and place of the ghost's start, fading to black" },
			{ PART_SPINS, "spins", "Spin sprites", "Sprites at the caster's point on alternate sides (a flipbook)" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing", "Ticks", "The cue's ticks" },
		};
#define TPF(member) (uint32_t)offsetof(TonkParams, member)
		static const Field FIELDS[] = {
			{ "main", "sounds.voice", "Voice", "The voice clip (a voice slot claimed at its tick, released at the release tick)", TPF(sounds.voice), fxp::Sound, 0, 0x7FFFFFFF },
			{ "main", "sounds.start", "Sound", "At the sound's tick, at the caster's point", TPF(sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "main", "timing.voice", "Voice", "The cue's tick of the voice", TPF(timing.voice), fxp::I16, 0, 400 },
			{ "main", "timing.sound", "Sound", "Its tick of the sound", TPF(timing.sound), fxp::I16, 0, 400 },
			{ "main", "timing.tint", "Tint", "Its tick the tint starts", TPF(timing.tint), fxp::I16, 0, 400 },
			{ "main", "timing.emitters", "Emitters", "Its tick the two spark emitters start", TPF(timing.emitters), fxp::I16, 0, 400 },
			{ "main", "timing.ghost", "Ghost", "Its tick the ghost starts", TPF(timing.ghost), fxp::I16, 0, 400 },
			{ "main", "timing.spin_first", "Spins: first", "Its first tick of the spin sprites (one on each odd tick)", TPF(timing.spin_first), fxp::I16, 0, 400 },
			{ "main", "timing.spin_last", "Spins: last", "Its last tick of the spin sprites", TPF(timing.spin_last), fxp::I16, 0, 400 },
			{ "main", "timing.damage", "Damage", "Its tick the target's damage is shown", TPF(timing.damage), fxp::I16, 0, 400 },
			{ "main", "timing.release", "Voice release", "Its tick the voice is released (if still playing)", TPF(timing.release), fxp::I16, 0, 400 },
			{ "main", "timing.end", "End", "Its last tick", TPF(timing.end), fxp::I16, 0, 400 },
			{ "main", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", TPF(res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "main", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", TPF(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "sparks_a", "sparks.ticks", "Ticks", "Both emitters spawn while their tick is at most this", TPF(sparks.ticks), fxp::I16, 0, 400 },
			{ "sparks_a", "sparks.count", "Count", "New sparks a tick for each emitter (100 cells for both)", TPF(sparks.count), fxp::I16, 0, 100 },
			{ "sparks_a", "sparks.size_spread", "Size spread", "Both emitters' sparks: a random size below this added", TPF(sparks.size_spread), fxp::I16, 1, 0x4000 },
			{ "sparks_a", "a.size", "Size", "Its sparks' size (less shrink x the emitter's tick)", TPF(a.size), fxp::I16, -0x4000, 0x4000, fxp::Scale },
			{ "sparks_a", "a.shrink", "Shrink", "Their size's shrink a tick of the emitter", TPF(a.shrink), fxp::I16, -0x400, 0x400 },
			{ "sparks_a", "a.spread", "Scatter", "Their scatter around the bone: +- this (less narrow x the emitter's tick)", TPF(a.spread), fxp::I16, 1, 0x2000 },
			{ "sparks_a", "a.narrow", "Narrowing", "The scatter's narrowing a tick", TPF(a.narrow), fxp::I16, -0x100, 0x100 },
			{ "sparks_a", "a.rise", "Rise", "Their rising speed (plus a random below the rise spread)", TPF(a.rise), fxp::I16, -0x400, 0x400 },
			{ "sparks_a", "a.rise_spread", "Rise spread", "The random part of their rising speed", TPF(a.rise_spread), fxp::I16, 1, 0x400 },
			{ "sparks_a", "seqs.spark_a", "Flipbook", "Emitter A's flipbook (a spark lives through it)", TPF(seqs.spark_a), fxp::Seq, 0, 0x7FFFFFFF },
			{ "sparks_b", "b.size", "Size", "Its sparks' size (less shrink x the emitter's tick)", TPF(b.size), fxp::I16, -0x4000, 0x4000, fxp::Scale },
			{ "sparks_b", "b.shrink", "Shrink", "Their size's shrink a tick of the emitter", TPF(b.shrink), fxp::I16, -0x400, 0x400 },
			{ "sparks_b", "b.spread", "Scatter", "Their scatter around the bone: +- this (less narrow x the emitter's tick)", TPF(b.spread), fxp::I16, 1, 0x2000 },
			{ "sparks_b", "b.narrow", "Narrowing", "The scatter's narrowing a tick", TPF(b.narrow), fxp::I16, -0x100, 0x100 },
			{ "sparks_b", "seqs.spark_b", "Flipbook", "Emitter B's flipbook (a spark lives through it)", TPF(seqs.spark_b), fxp::Seq, 0, 0x7FFFFFFF },
			{ "tint", "tint.in", "Fade in", "Its ticks from the battle's colour to the first tint colour", TPF(tint.in), fxp::I16, 1, 400 },
			{ "tint", "tint.out", "Fade out", "Its tick the fade to black starts (the colours in turn before)", TPF(tint.out), fxp::I16, 0, 400 },
			{ "tint", "tint.fade", "Fade", "The fade to black a tick (of 4096)", TPF(tint.fade), fxp::I16, 0, 0x1000 },
			{ "tint", "tint.end", "End", "Its last tick (the caster's flag put back)", TPF(tint.end), fxp::I16, 1, 400 },
			{ "tint", "tables.tints", "Colours", "The 24 tint colours (a colour word each), in turn", TPF(tables.tints), fxp::Data, 0, 0x7FFFFFFF },
			{ "ghost", "ghost.hold", "Hold", "Drawn in the caster's place of its start after this many ticks", TPF(ghost.hold), fxp::I16, 0, 400 },
			{ "ghost", "ghost.fade_start", "Fade start", "Its tick it starts fading to black", TPF(ghost.fade_start), fxp::I16, 0, 400 },
			{ "ghost", "ghost.fade", "Fade", "Its fade a tick (of 4096)", TPF(ghost.fade), fxp::I16, 0, 0x1000 },
			{ "ghost", "ghost.end", "End", "Its last tick", TPF(ghost.end), fxp::I16, 0, 400 },
			{ "spins", "spins.size", "Size", "Their size", TPF(spins.size), fxp::I16, 0, 0x4000, fxp::Scale },
			{ "spins", "spins.turn", "Turn", "Their turn on the second side", TPF(spins.turn), fxp::I16, -0x800, 0x800, fxp::Angle },
			{ "spins", "spins.side", "Side", "Their offset to each side of the caster's point", TPF(spins.side), fxp::I16, -0x2000, 0x2000 },
			{ "spins", "spins.up", "Up", "Their offset up", TPF(spins.up), fxp::I16, -0x2000, 0x2000 },
			{ "spins", "seqs.spin", "Flipbook", "Their flipbook (a sprite lives through it)", TPF(seqs.spin), fxp::Seq, 0, 0x7FFFFFFF },
		};
#undef TPF
	}
}

	void register_mag324_tonberry_king_death()
	{
		fxp::register_params({ 324, "Tonberry King death", &tonk324::live, &tonk324::GAME, sizeof(tonk324::TonkParams),
		                       tonk324::table::PARTS, (int)(sizeof(tonk324::table::PARTS) / sizeof(tonk324::table::PARTS[0])),
		                       tonk324::table::FIELDS, (int)(sizeof(tonk324::table::FIELDS) / sizeof(tonk324::table::FIELDS[0])),
		                       tonk324::table::GROUPS, (int)(sizeof(tonk324::table::GROUPS) / sizeof(tonk324::table::GROUPS[0])) });
		own::note(tonk324::ORIG_FileLoader, (void *)tonk324::FileLoader);
		own::note(tonk324::ORIG_Setup, (void *)tonk324::Setup);
		own::note(tonk324::ORIG_RootTask, (void *)tonk324::RootTask);
		register_port(tonk324::ORIG_RootTask, (void *)tonk324::RootTask, "324 RootTask", 324);
		own::note(tonk324::ORIG_CueTask, (void *)tonk324::CueTask);
		register_port(tonk324::ORIG_CueTask, (void *)tonk324::CueTask, "324 CueTask", 324);
		own::note(tonk324::ORIG_EmitterA, (void *)tonk324::EmitterA);
		register_port(tonk324::ORIG_EmitterA, (void *)tonk324::EmitterA, "324 EmitterA", 324);
		own::note(tonk324::ORIG_EmitterB, (void *)tonk324::EmitterB);
		register_port(tonk324::ORIG_EmitterB, (void *)tonk324::EmitterB, "324 EmitterB", 324);
		own::note(tonk324::ORIG_TintTask, (void *)tonk324::TintTask);
		register_port(tonk324::ORIG_TintTask, (void *)tonk324::TintTask, "324 TintTask", 324);
		own::note(tonk324::ORIG_GhostTask, (void *)tonk324::GhostTask);
		register_port(tonk324::ORIG_GhostTask, (void *)tonk324::GhostTask, "324 GhostTask", 324);
		own::note(tonk324::ORIG_SpinTask, (void *)tonk324::SpinTask);
		register_port(tonk324::ORIG_SpinTask, (void *)tonk324::SpinTask, "324 SpinTask", 324);
		// 30 fps layer: see mag324_tonberry_king_death_held.inc
		FX_HELD(register_mag324_held();)
	}
	FX_LAB_MODULE(register_mag324_tonberry_king_death);
}

#ifdef FF8_FX_HELD
#include "mag324_tonberry_king_death_held.inc"
#endif
