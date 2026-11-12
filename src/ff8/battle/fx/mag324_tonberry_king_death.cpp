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

#include "mag_common.h"
#include <stddef.h>

namespace ff8fx
{
namespace tonk324
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2203C48); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x2203C4C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x221BC60); }
	inline uint32_t &Voice() { return var<uint32_t>(0x221BC64); }
	inline int16_t *BonePoint() { return (int16_t *)0x2203C58; }           // the caster's bone 0xF0 (the cue, every tick)
	inline TaskQueue *QTasks() { return (TaskQueue *)0x2202E28; }          // pool 0x2202E38: 0x64 x 0x24
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
	inline Cell *Cells() { return (Cell *)0x2201AC8; }

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
		int16_t ang[3] = { 0, (int16_t)(t->word0E << 11), 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		const int32_t sc[3] = { t->word1C, t->word1C, t->word1C };
		Scale3DMatrix(&m, sc);
		const Mat4x3 *p = ShadowCamera(point, t->word1C, 0 - (t->word1C >> 4));
		m.t[0] = (t->word0E ? 0xC8 : -0xC8) + p->t[0];
		m.t[1] = p->t[1] - 0x3C;
		m.t[2] = p->t[2];
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Spin;
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
			BonesOut(Caster() + 0x60, (uint8_t *)KEPT_BONES);
			uint8_t *e = Caster();
			*(uint32_t *)(e + 0x7C) &= ~2u;
			memcpy((void *)KEPT_MATRIX, e + 0x40, 0x20);
		}
		BonesOut(Caster() + 0x60, (uint8_t *)CUR_BONES);
		uint8_t *e = Caster();
		const uint32_t tint = *(const uint32_t *)(e + 0x28);
		const uint32_t mask = *(const uint32_t *)(e + 0x7C);
		uint8_t live[0x20];
		if (t->counter > 6)
		{
			memcpy(live, e + 0x40, 0x20);
			memcpy(e + 0x40, (const void *)KEPT_MATRIX, 0x20);
		}
		*(uint32_t *)(e + 0x7C) = 2;
		BonesIn(e + 0x60, (const uint8_t *)KEPT_BONES);
		if (t->counter >= 0x10)
		{
			GteSetFarColor(0, 0, 0);
			GteLoadRGBC(e + 0x28);
			GteSetIR0((t->counter - 0x10) << 8);
			GteDPCS();
			GteStoreRGB2(e + 0x28);
			e[0x2B] = 2;
			e[7] = 0;
		}
		// 30 fps layer: see mag324_tonberry_king_death_held.inc
		FX_HELD(held_note_ghost(n);)
		ModelDraw(e);
		BonesIn(Caster() + 0x60, (const uint8_t *)CUR_BONES);
		e = Caster();
		const int16_t c = t->counter;
		*(uint32_t *)(e + 0x7C) = mask;
		*(uint32_t *)(e + 0x28) = tint;
		if (c > 6) memcpy(e + 0x40, live, 0x20);
		if (DrawOnly()) return 0;
		t->counter = (int16_t)(c + 1);
		return t->counter > 0x20 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Tint (0x597410)
	// ------------------------------------------------------------------
	// the tint's colour of counter c (0x597417..0x5974CD) -> e + 0x28
	static void TintColour(TaskBody *t, uint8_t *e, int16_t c)
	{
		if (c < 4)
		{
			const uint32_t far = var<uint32_t>(0xB8B7D8);
			GteSetFarColor(far & 0xFF, (far >> 8) & 0xFF, var<uint8_t>(0xB8B7DA));
			GteLoadRGBC((const void *)TINTS);
			GteSetIR0((4 - c) << 10);
			GteDPCS();
			GteStoreRGB2(e + 0x28);
		}
		else if (c >= 0x2A)
		{
			GteSetFarColor(0, 0, 0);
			GteLoadRGBC((const void *)(TINTS + t->word1C * 4));
			GteSetIR0((c - 0x2A) << 9);
			GteDPCS();
			GteStoreRGB2(e + 0x28);
		}
		else
		{
			const int32_t idx = (c - 4) % 0x18;
			t->word1C = (int16_t)idx;
			*(uint32_t *)(e + 0x28) = *(const uint32_t *)(TINTS + idx * 4);
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
		if (t->counter < 0x32) return 0;
		*(uint16_t *)e = (uint16_t)(flags & 0xF7FF);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Emitters (0x596E30 / 0x597130)
	// ------------------------------------------------------------------
	static uint32_t __cdecl EmitterA(TaskNode *n)
	{
		TaskBody *t = (TaskBody *)n;
		uint8_t *w = (uint8_t *)FieldAlloc(0x48);
		if (!DrawOnly() && t->counter <= 0x2E)
		{
			const int32_t nb = **(const uint8_t *const *)*(uint8_t *const *)(Caster() + 0x64);
			for (int i = 0; i < 2; i++)
			{
				int k = 0;
				while (k < CELLS && Cells()[k].flag != 0) k++;
				if (k >= CELLS) break;
				Cell &c = Cells()[k];
				c.flag = 1;
				c.counter = 0;
				const int32_t r0 = CrtRand() % 0x600;
				c.size = (int16_t)(r0 - (int16_t)(t->counter * 0x28) + 0x800);
				const int32_t bone = nb ? CrtRand() % nb : 0;
				GetEffectSpawnPosition(Caster(), bone, CrtRand() % 0x1000, c.pos);
				const int32_t s = 0xFA - t->counter * 3, range = s + s;
				c.pos[0] = (int16_t)(c.pos[0] + (CrtRand() % range - s));
				c.pos[1] = (int16_t)(c.pos[1] + (CrtRand() % range - s));
				c.pos[2] = (int16_t)(c.pos[2] + (CrtRand() % range - s));
				c.vy = (int16_t)(-5 - CrtRand() % 0x19);
			}
		}
		// 30 fps layer: see mag324_tonberry_king_death_held.inc
		FX_HELD(held_note_sparks(n, 1);)
		const int32_t live = SparkDraw(w, Cells(), 1, SEQ_SparkA, true);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		t->counter++;
		return live ? 0 : TASK_END;
	}

	static uint32_t __cdecl EmitterB(TaskNode *n)
	{
		TaskBody *t = (TaskBody *)n;
		uint8_t *w = (uint8_t *)FieldAlloc(0x48);
		if (!DrawOnly() && t->counter <= 0x2E)
		{
			const int32_t nb = **(const uint8_t *const *)*(uint8_t *const *)(Caster() + 0x64);
			for (int i = 0; i < 2; i++)
			{
				int k = 0;
				while (k < CELLS && Cells()[k].flag != 0) k++;
				if (k >= CELLS) break;
				Cell &c = Cells()[k];
				c.flag = 2;
				c.counter = 0;
				const int32_t r0 = CrtRand() % 0x600;
				c.size = (int16_t)(r0 + (int16_t)((int16_t)(0x18 - t->counter) << 5));
				const int32_t bone = nb ? CrtRand() % nb : 0;
				GetEffectSpawnPosition(Caster(), bone, CrtRand() % 0x1000, c.pos);
				const int32_t s = (0x64 - t->counter) * 3, range = s + s;
				c.pos[0] = (int16_t)(c.pos[0] + (CrtRand() % range - s));
				c.pos[1] = (int16_t)(c.pos[1] + (CrtRand() % range - s));
				c.pos[2] = (int16_t)(c.pos[2] + (CrtRand() % range - s));
			}
		}
		FX_HELD(held_note_sparks(n, 2);)
		const int32_t live = SparkDraw(w, Cells(), 2, SEQ_SparkB, false);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		t->counter++;
		return live ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Cue (0x596C70)
	// ------------------------------------------------------------------
	static TaskBody *AddTask(uint32_t fn) { return (TaskBody *)AddTaskToQueue(QTasks(), fn); }

	static uint32_t __cdecl CueTask(TaskNode *n)
	{
		TaskBody *t = (TaskBody *)n;
		GetEffectSpawnPosition(Caster(), 0xF0, 0x1000, BonePoint());
		if (DrawOnly()) return 0;
		if (t->counter == 0) Voice() = ClaimVoiceSlot(VOICE, 1, 0x80);
		if (t->counter == 0xC)
		{
			AddTask(ORIG_EmitterA)->counter = 0;
			t->word0E = 0;
			if (t->counter == 0xC) AddTask(ORIG_EmitterB)->counter = 0;
		}
		if (t->counter == 8)
		{
			TaskBody *a = AddTask(ORIG_TintTask);
			a->counter = 0;
			a->word0E = (int16_t)CasterSlot();
		}
		if (t->counter == 0x12) AddTask(ORIG_GhostTask)->counter = 0;
		if (t->counter >= 0x7D && t->counter <= 0x87 && t->counter % 2 == 1)
		{
			TaskBody *a = AddTask(ORIG_SpinTask);
			const int16_t side = t->word0E;
			a->counter = 0;
			a->word0E = (int16_t)(side & 1);
			a->word1C = 0x800;
			t->word0E = (int16_t)(side + 1);
		}
		if (t->counter == 0x82) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (t->counter == 1) BdPlaySE3D(SOUND, 0, BonePoint());
		if (t->counter == 0xC5 && VoiceActive(Voice())) ReleaseVoiceSlot(Voice());
		t->counter++;
		return t->counter > 0xC8 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root (0x5979A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag324_tonberry_king_death_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = ARENA_A;
		if (!(*(const uint8_t *)&r->counter & 1)) PacketCursor() = ARENA_B;
		const int a = ExecuteTaskQueue(QTasks());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag324_tonberry_king_death()
	{
		register_port(tonk324::ORIG_RootTask, (void *)tonk324::RootTask, "324 RootTask", 324);
		register_port(tonk324::ORIG_CueTask, (void *)tonk324::CueTask, "324 CueTask", 324);
		register_port(tonk324::ORIG_EmitterA, (void *)tonk324::EmitterA, "324 EmitterA", 324);
		register_port(tonk324::ORIG_EmitterB, (void *)tonk324::EmitterB, "324 EmitterB", 324);
		register_port(tonk324::ORIG_TintTask, (void *)tonk324::TintTask, "324 TintTask", 324);
		register_port(tonk324::ORIG_GhostTask, (void *)tonk324::GhostTask, "324 GhostTask", 324);
		register_port(tonk324::ORIG_SpinTask, (void *)tonk324::SpinTask, "324 SpinTask", 324);
		// 30 fps layer: see mag324_tonberry_king_death_held.inc
		FX_HELD(register_mag324_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag324_tonberry_king_death_held.inc"
#endif
