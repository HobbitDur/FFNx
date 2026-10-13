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

// Effect 130: Eerie Sound Wave (enemy attack 185 of kernel.bin, Malboro c0m048; MAG_130_*). No
// other kernel.bin entry uses effect 130.
//
// Structure (setup MAG_130_EERIE_SOUND_WAVE 0x6B4240 -> _Init 0x6B4270, file loader 0x6B4250 = the
// texture file named at 0x12238B0; the setup keeps the CASTER's entity, starts camera animation
// 0x1223810 and queues the TIM; setup / loader / _Init are the copies of effects 126..129):
//   MasterTask (0x6B42F0) - alternates the packet arena (magic buffer + 0xF7C / + 0x18F7C); on
//     counter 1 sets up the pools (emitter 1 x 0x4CC, waves 0xC x 0x74, impacts 3 x 0xC0) and starts
//     the emitter: the targets of action 0 sorted by entity x (largest first). Then the emitter,
//     wave and impact queues; ends when all three are empty.
//   EmitterTask (0x6B4520) - the layout 0x1221138 at the caster's bone 0xB (y scale 0x4000) lifted
//     by 200, turned by the caster's facing; ends with the layout. Sound at counter 10. Every 3rd
//     counter below 12 x targets: a wave from the caster's bone 0xB towards target counter / 12;
//     the 4th wave of each target (counter % 12 == 9) carries the hit.
//   WaveTask (0x6B4960) - the layout 0x12228A8 moving from its start to the target's default
//     effect position (sine ease over 0x18 ticks, then on the target), turned along the start ->
//     target line (0x50CBA0); ends with the layout. A hit wave starts an impact on its first tick.
//   ImpactTask (0x6B4AD0) - the layout 0x1220990 at the target's default effect position (taken at
//     the spawn); on the layout's frame 8 the damage of every target record of action 0 that is this
//     entity (0x6B4B90, as Junk's 0x6E87A0); ends with the layout.
//   PrimCallback (0x6B46C0, Moomba's 0x6ECCE0 on this module's cursor) draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (no draw-only pass).
// The tasks write no entity position and no camera words (the camera animation is the engine's).
// Module globals: 0x25210A8..0x2521110 (caster 0x25210A8, texture file 0x25210AC, magic buffer base
// 0x25210B0, context 0x25210B4, master pool 0x25210B8, packet cursor 0x25210CC, the queues
// 0x25210D0 impacts / 0x25210E0 waves / 0x25210F0 emitter / 0x2521100 master).
//
// NEVER-WRITTEN STACK WORDS (the game's executor 0x508420 calls every task of a queue at one esp;
// the master runs the emitter, wave and impact queues 4 bytes deeper each: emitter entry esp E,
// waves E - 4, impacts E - 8). Between two effect ticks the battle loop 0x500900 runs the engine
// queues 0x1D96AA4 / 0x1D96A8C / 0x1D96AA0 at the effect root's depth: what lies below the master's
// executors when a tick starts is engine residue (taken as 0 here; no output depends on it):
//   - ImpactTask never writes its matrix's m[2][2] (entry esp - 0x18): the identity it builds keeps
//     the word below, which ComposeAffineTransform reads. Its last writer is the last task that ran
//     before it in the tick: the previous impact (its composed m[2][2]), else the last wave (its
//     composed m[2][0], wave entry esp - 0x1C), else the emitter (its composed m[1][1], entry esp
//     - 0x20), else the residue at the tick's start (0). Stack().m22 (see ImpactTask). No draw of the
//     impact layout depends on the word (the matrix's third column reaches no vertex).
//   - EmitterTask's second spawn position is copied to the wave's +0x14..+0x1B with its 4th word
//     (entry esp - 0x2A) never written; no code reads the wave's +0x1A. The emitter is the tick's
//     first task: on the first tick the master's Memset32 call left its push of the root executor's
//     edi there (0: the master is the root queue's first node); later ticks: the residue (0).

#include "mag_common.h"

namespace ff8fx
{
namespace eerie130
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x25210A8); }         // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x25210B0); }         // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25210B4); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25210CC); }
	inline TaskQueue *QImpacts() { return (TaskQueue *)0x25210D0; }          // pool: magic buffer, 3 x 0xC0
	inline TaskQueue *QWaves() { return (TaskQueue *)0x25210E0; }            // pool: + 0x240, 0xC x 0x74
	inline TaskQueue *QEmitter() { return (TaskQueue *)0x25210F0; }          // pool: + 0x7B0, 1 x 0x4CC
	inline uint32_t RenderOT() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_MasterTask = 0x6B42F0;
	static const uint32_t ORIG_EmitterTask = 0x6B4520;
	static const uint32_t ORIG_WaveTask = 0x6B4960;
	static const uint32_t ORIG_ImpactTask = 0x6B4AD0;
	static const uint32_t MODEL_Emitter = 0x1221138;     // prim-model layouts
	static const uint32_t MODEL_Wave = 0x12228A8;
	static const uint32_t MODEL_Impact = 0x1220990;
	static const void *const SOUND_Wave = (const void *)0x122380C;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAngles2(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // sub_56C220: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }                // MAG_063_sub_701270
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	// sub_56CB90: out = (a * wa + b * wb) >> 12 through the GTE (IR loaded as u16, GPF / GPL)
	inline void BlendVectors(const int16_t *a, const int16_t *b, int32_t wa, int32_t wb, int16_t *out) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int16_t *)>(0x56CB90)(a, b, wa, wb, out); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x25210B8)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct EmitterNode // pool of 1 node of 0x4CC bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t count;     // +0x0E targets of action 0
		uint8_t *ents[3];  // +0x10 target entities (sorted by x, largest first)
		uint8_t idx[3];    // +0x1C their target record indexes
		uint8_t pad1F;
		uint8_t layout[0x4AC]; // +0x20 prim-model layout
	};
	struct WaveNode // pool of 0xC nodes of 0x74 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t no_hit;    // +0x0E 0 = the wave starts an impact on its first tick
		uint8_t idx;       // +0x0F target record index (not read)
		uint8_t *entity;   // +0x10 target entity
		int16_t start[4];  // +0x14 caster's bone 0xB at the spawn (+0x1A: never-written stack word, not read)
		uint8_t layout[0x58]; // +0x1C prim-model layout
	};
	struct ImpactNode // pool of 3 nodes of 0xC0 bytes
	{
		TaskNode hdr;
		int16_t pos[4];    // +0x0C the target's default effect position at the spawn
		uint8_t *entity;   // +0x14 target entity
		uint8_t layout[0xA8]; // +0x18 prim-model layout
	};
	// the prim-player callback's parameter block (a stack block of the drawing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 camera x object frame
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(EmitterNode) == 0x4CC && sizeof(WaveNode) == 0x74 && sizeof(ImpactNode) == 0xC0, "Eerie Sound Wave nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Eerie Sound Wave prim block");

	// the never-written stack word (see the module comment): what the module's own code last left in
	// the impact's m[2][2] slot during the tick
	struct StackWords
	{
		int16_t m22;
	};
	static StackWords &Stack()
	{
		static StackWords s = {};
		return s;
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6B46C0, Moomba's 0x6ECCE0 on this module's cursor): one object of a
	// layout. Vertex frame (or blend into the parameter block's morph buffer), rotation from the
	// record's angles, the record's offset through the parameter matrix (flag 0x200: as is), the
	// matrix composed with it, scale (flag 0x100: 3x3 product, else scale3DMatrix), drawn by 0x7043B0.
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x58);
		const int32_t object = (int16_t)r->flags_lo;
		uint8_t *model = l->data + *(const int32_t *)(l->data + object * 4 + 8);
		*(uint8_t **)h = model;
		const int16_t f0 = r->b0, f1 = r->b1;
		const int32_t nverts = *(const int32_t *)(model + 4);
		if (f0 == f1)
		{
			if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
			else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
		}
		else
		{
			const int16_t t = r->b;
			if (t == 0)
			{
				if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
			}
			else if (t == 0x1000)
			{
				if (f1 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f1) * 8 + 0xC;
			}
			else
			{
				BlendVertexFrames((uint32_t)model, f0, f1, t, arg->morph);
				*(uint32_t *)(h + 4) = arg->morph;
			}
		}
		Mat4x3 m = {};
		if (r->flags & 0x40000) RotationFromAngles2(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		int32_t t[3];
		if (r->flags & 0x200)
		{
			t[0] = off[0];
			t[1] = off[1];
			t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
			t[0] = m.t[0];
			t[1] = m.t[1];
			t[2] = m.t[2];
		}
		m.t[0] = (int32_t)((uint32_t)t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				int16_t s[9] = { r->scale[0], 0, 0, 0, r->scale[1], 0, 0, 0, r->scale[2] };
				MulMatrix3(&m, s);
			}
			else
			{
				const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		PacketCursor() = RenderPrim2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag130_eerie_sound_wave_held.h"
#endif

namespace ff8fx
{
namespace eerie130
{
	// sub_6B4B90 (as Junk's 0x6E87A0): the damage of every target record of action 0 that is this entity
	static void DamageEntity(uint8_t *entity)
	{
		const int32_t slot = (int32_t)((uint32_t)entity - 0x1D972C0u) / 0x9C;
		for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
		{
			uint8_t *t = Ctx()->actions[0].targets + i * TARGET_STRIDE;
			if ((int32_t)t[0] == slot) ApplyActionResultToTarget(t);
		}
	}

	// ------------------------------------------------------------------
	// Impact (0x6B4AD0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ImpactTask(TaskNode *n)
	{
		ImpactNode *p = (ImpactNode *)n;
		PrimArg arg;
		arg.m.m[1][1] = 0x1000;
		arg.m.m[0][0] = 0x1000;
		arg.m.t[0] = p->pos[0];
		arg.m.t[1] = p->pos[1];
		arg.m.m[2][1] = 0;
		arg.m.m[2][0] = 0;
		arg.m.m[1][2] = 0;
		arg.m.m[1][0] = 0;
		arg.m.m[0][2] = 0;
		arg.m.m[0][1] = 0;
		arg.m.t[2] = p->pos[2];
		// m[2][2] is never written: the word the previous task left in the slot (module comment)
		arg.m.m[2][2] = Stack().m22;
		arg.m.pad = 0;
		// 30 fps layer: see mag130_eerie_sound_wave_held.inc
		FX_HELD(held_note_play(PLAY_IMPACT, n, (prim::Layout *)p->layout, &arg.m);)
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		Stack().m22 = arg.m.m[2][2];
		arg.depth = -0x80;
		arg.morph = TexBase() + 0xC7C;
		const int played = prim::play((prim::Layout *)p->layout, PrimCallback, (int)&arg, 0);
		if (played == 0) return TASK_END;
		if (played == 8) DamageEntity(p->entity);
		return 0;
	}

	// ------------------------------------------------------------------
	// Wave (0x6B4960)
	// ------------------------------------------------------------------
	static uint32_t __cdecl WaveTask(TaskNode *n)
	{
		WaveNode *w = (WaveNode *)n;
		int16_t pos[4];
		GetDefaultEffectPosition(w->entity, pos);
		int32_t d[3];
		d[0] = (int32_t)pos[0] - w->start[0];
		d[1] = (int32_t)pos[1] - w->start[1];
		d[2] = (int32_t)pos[2] - w->start[2];
		if (w->counter < 0x18)
		{
			const int32_t s = ComputeSin(shl32(w->counter, 10) / 24);
			BlendVectors(w->start, pos, 0x1000 - s, s, pos);
		}
		if (w->counter == 0 && w->no_hit == 0)
		{
			ImpactNode *p = (ImpactNode *)AddTaskToQueue(QImpacts(), ORIG_ImpactTask);
			if (p)
			{
				GetDefaultEffectPosition(w->entity, p->pos);
				p->entity = w->entity;
				DecodeModelPrimLayout(MODEL_Impact, p->layout, 0xA8);
			}
		}
		PrimArg arg;
		BuildOrthonormalBasis(d, &arg.m);
		arg.m.t[0] = pos[0];
		arg.m.t[1] = pos[1];
		arg.m.t[2] = pos[2];
		// 30 fps layer: see mag130_eerie_sound_wave_held.inc
		FX_HELD(held_note_play(PLAY_WAVE, n, (prim::Layout *)w->layout, &arg.m);)
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		Stack().m22 = arg.m.m[2][0];
		arg.depth = 0;
		arg.morph = TexBase() + 0xC7C;
		if (prim::play((prim::Layout *)w->layout, PrimCallback, (int)&arg, 0) == 0) return TASK_END;
		w->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Emitter (0x6B4520)
	// ------------------------------------------------------------------
	static uint32_t __cdecl EmitterTask(TaskNode *n)
	{
		EmitterNode *e = (EmitterNode *)n;
		int16_t pos[4];
		GetEffectSpawnPosition(Caster(), 0xB, 0x4000, pos);
		pos[1] = (int16_t)(pos[1] - 200);
		PrimArg arg;
		RotY((int16_t)-*(const int16_t *)(Caster() + 0xE), &arg.m);
		arg.m.t[0] = pos[0];
		arg.m.t[1] = pos[1];
		arg.m.t[2] = pos[2];
		// 30 fps layer: see mag130_eerie_sound_wave_held.inc
		FX_HELD(held_note_play(PLAY_EMITTER, n, (prim::Layout *)e->layout, &arg.m);)
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		Stack().m22 = arg.m.m[1][1];
		arg.depth = -0x80;
		arg.morph = TexBase() + 0xC7C;
		if (prim::play((prim::Layout *)e->layout, PrimCallback, (int)&arg, 0) == 0) return TASK_END;
		if (e->counter == 10) BdPlaySE(SOUND_Wave, 0, 0x80);
		// the 4th word of the position is never written (module comment)
		pos[3] = 0;
		GetEffectSpawnPosition(Caster(), 0xB, 0x1000, pos);
		const int32_t c = e->counter;
		if (c < (int32_t)e->count * 12)
		{
			const int32_t r = c % 12;
			if ((uint32_t)r % 3 == 0)
			{
				WaveNode *w = (WaveNode *)AddTaskToQueue(QWaves(), ORIG_WaveTask);
				if (w)
				{
					w->idx = e->idx[e->counter / 12];
					w->no_hit = r != 9;
					w->entity = e->ents[e->counter / 12];
					w->counter = 0;
					memcpy(w->start, pos, 8);
					DecodeModelPrimLayout(MODEL_Wave, w->layout, 0x58);
				}
			}
		}
		e->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x6B42F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag130_eerie_sound_wave_held.inc
		FX_HELD(held_note_master();)
		// the stack below the executors holds the engine's residue when the tick starts (module comment)
		Stack().m22 = 0;
		if (m->arena)
		{
			PacketCursor() = TexBase() + 0xF7C;
			m->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x18F7C;
			m->arena = 1;
		}
		if (m->counter == 1 && !m->started)
		{
			m->started = 1;
			InitTaskQueuePool(QEmitter(), (void *)(TexBase() + 0x7B0), 0x4CC, 1);
			InitTaskQueuePool(QWaves(), (void *)(TexBase() + 0x240), 0x74, 0xC);
			InitTaskQueuePool(QImpacts(), (void *)TexBase(), 0xC0, 3);
			EmitterNode *e = (EmitterNode *)AddTaskToQueue(QEmitter(), ORIG_EmitterTask);
			Memset32(&e->counter, 0, 0x130);
			DecodeModelPrimLayout(MODEL_Emitter, e->layout, 0x4AC);
			uint8_t *const b = (uint8_t *)e;
			e->count = (int16_t)Ctx()->actions[0].target_count;
			// target record indexes (+0x1C + i) and entities (+0x10 + 4 i), written in this order
			for (int32_t i = 0; i < e->count; i++)
			{
				b[0x1C + i] = (uint8_t)i;
				*(uint8_t **)(b + 0x10 + 4 * i) = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
			}
			// sorted by entity x, largest first (the record indexes follow)
			for (int32_t i = 0; i < e->count - 1; i++)
			{
				for (int32_t j = i + 1; j < e->count; j++)
				{
					uint8_t *a = *(uint8_t **)(b + 0x10 + 4 * i);
					uint8_t *c = *(uint8_t **)(b + 0x10 + 4 * j);
					if (*(const int16_t *)(a + 0x1C) < *(const int16_t *)(c + 0x1C))
					{
						*(uint8_t **)(b + 0x10 + 4 * i) = c;
						*(uint8_t **)(b + 0x10 + 4 * j) = a;
						const uint8_t x = b[0x1C + j];
						const uint8_t y = b[0x1C + i];
						b[0x1C + i] = x;
						b[0x1C + j] = y;
					}
				}
			}
		}
		int a0, a1, a2;
		if (m->started)
		{
			a0 = ExecuteTaskQueue(QEmitter());
			a1 = ExecuteTaskQueue(QWaves());
			a2 = ExecuteTaskQueue(QImpacts());
		}
		else a0 = a1 = a2 = (int)n;
		if (m->started && a0 == 0 && a1 == 0 && a2 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag130_eerie_sound_wave()
	{
		register_port(eerie130::ORIG_MasterTask, (void *)eerie130::MasterTask, "130 MasterTask", 130);
		register_port(eerie130::ORIG_EmitterTask, (void *)eerie130::EmitterTask, "130 EmitterTask", 130);
		register_port(eerie130::ORIG_WaveTask, (void *)eerie130::WaveTask, "130 WaveTask", 130);
		register_port(eerie130::ORIG_ImpactTask, (void *)eerie130::ImpactTask, "130 ImpactTask", 130);
		// 30 fps layer: see mag130_eerie_sound_wave_held.inc
		FX_HELD(register_mag130_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag130_eerie_sound_wave_held.inc"
#endif
