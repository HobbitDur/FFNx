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
// Effect 288: Rapture (kernel.bin magic Rapture +4 = 288: Selphie's Slot; MAG_288_*). Module 0x6791F0..0x679E10.
//
// Structure (setup 0x6791F0 -> 0x679220, file loader 0x679200 = the texture file named at 0x1078998; the setup queues the TIM,
// takes the magic buffer (0x2507CA4), keeps the cast context (0x2507C44), starts the root task in its 1-node pool 0x2507C88 and the
// camera animation 0x10787F0):
//   RootTask (0x679290) - alternates the packet arena (magic buffer + 0x2460 / + 0x1A460 by tick parity, cursor 0x2507CA0); a
//     counter runs 0..9 and wraps, a second one counts the ticks; on the wrapping counter's 1 (first time: the pools - target:
//     magic buffer + 0, 6 x 0x420; feather: + 0x18C0, 0x30 x 0x24; sparkle: + 0x1F80, 0x30 x 0x14) a TargetTask on the next
//     target of action 0 (one every 10 ticks); the screen flash: level (tick k = count - 1 < 0x4A) k * 0x100 for k < 0xC, 0xC00,
//     (0x49 - k) * 0x100 from 0x3E (exact: the * 2/3 >> 3 of the code), colour = ColourLerp along the 4-byte table 0x1078980
//     (k * 0x5000 / 74: entry >> 12, weight & 0xFFF); Effect_SetScreenFlash(level, colour); runs the target, feather, sparkle
//     queues; ends (flash 0, 0) when the target queue is empty.
//   TargetTask (0x679510) - 0x4B calls (counter 0..0x4A): every call the target's default effect position (0x571400) and the
//     scale s = ((size >> 1) + 0x800) >> 2 (size = its +0x26); counter 4..7: two feathers (rand: scale, angle, radius, fall
//     speed, angles, spins) around it; 4..0x1D: layout 0x10749A8 at its effect point 0xF1 (x / z; y = its +0x24), turned by
//     its facing (-(+0xE)), through the camera, scaled by s; 8..0x49 while its flags +0 have bit 1 set and bit 2 clear: layout
//     0x1075800 at its effect point 0xF0, same frame, depth ((size + 0x800) * z) >> 15 (z of 0x1078978 through the frame);
//     sound 0x1078970 on 0; even counters < 0x10: a sparkle (rand: angle, radius, height); 0x17..0x3E: entity +1 |= 0x10 and
//     its height +0x1E = the saved one + a cos curve (flag: the action record's +3 bit 2 set: a bounce; else up and away);
//     0x3F: flag bit 0x1000 cleared, height back, its action result (0x679C10), flags +0 |= 4 unless the flag is set.
//   FeatherTask (0x679C80) - feather model 0x10747B0 (Effect_RenderPrimModel 0x572200, mode 0) at its point, angles, scale; the
//     angles turn by the spins, y falls by its speed; after 10 ticks the scale shrinks by 0x100 a tick, ends below 0x100.
//   SparkleTask (0x679D90) - sequence 0x1074658 frame = its counter at its point through the camera (TransformCamera-
//     ByShadowRotation 0x1000 / -0x400; header 0xB4 on the Field_Alloc scratch, allocated first), rising 0x1E a tick, 12 frames.
//   PrimCallback (0x679970) - one object of a layout: vertex frame (or blend into the morph buffer, magic buffer + 0x2340),
//     rotation (flag 0x40000: 0x7015B0, else 0x701310); offset and matrix scaled by the block scale (block word +0x2C set: always);
//     flag 0x200: the offset as the translation, else the offset through the block matrix (and, without flag 0x8000, the rotation
//     composed with it); + the block translation; record scale (scale3DMatrix); fade; depth = the block's; 0x7043B0.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY WRITES: the targets' flags +0 (bit 0x1000 set 0x17..0x3E, cleared at 0x3F; bit 2 set at 0x3F unless the flag), height +0x1E
// (0x17..0x3F), action results (0x679C10: ApplyActionResultToTarget), the screen flash (Effect_SetScreenFlash: every entity draw entry
// 0x1D98990 + k * 0x2C, +2 level, +0x28 colour). Reads: the targets' bones (effect points 0xF0 / 0xF1, GetEffectSpawnPosition: their
// current pose and world matrix +0x40, every tick), +0x0E facing, +0x24, +0x26 size, flags +0. CAMERA: no camera word; the camera
// animation 0x10787F0 (Battle_PlayCameraAnimation) at the setup; GetEffectSpawnPosition writes the camera-state word 0x1D9770C.
// Module globals: 0x2507C40..0x2507CA8 (texture file 0x2507C40, context 0x2507C44, sparkle queue 0x2507C48, feather queue 0x2507C58,
// target queue 0x2507C68, root queue 0x2507C78, root pool 0x2507C88 (1 x 0x18), packet cursor 0x2507CA0, magic buffer base 0x2507CA4).
// NEVER-WRITTEN STACK WORDS: none read (the play blocks are fully written; the callback's offset word +6 and matrix pad only reach
// the GTE VZ0 / R33 high halves; the feather / sparkle headers keep the Field_Alloc scratch, as the port allocates the same way).

#include "mag_common.h"

namespace ff8fx
{
namespace rapture288
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507C44); }
	inline TaskQueue *QSparkle() { return (TaskQueue *)0x2507C48; }      // pool: magic buffer + 0x1F80, 0x30 x 0x14
	inline TaskQueue *QFeather() { return (TaskQueue *)0x2507C58; }      // pool: magic buffer + 0x18C0, 0x30 x 0x24
	inline TaskQueue *QTarget() { return (TaskQueue *)0x2507C68; }       // pool: magic buffer + 0, 6 x 0x420
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507CA0); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507CA4); }      // Magic_TextureOFF (magic buffer)

	static const uint32_t ORIG_RootTask = 0x679290;
	static const uint32_t ORIG_TargetTask = 0x679510;
	static const uint32_t ORIG_FeatherTask = 0x679C80;
	static const uint32_t ORIG_SparkleTask = 0x679D90;
	static const uint32_t LAYOUT_A = 0x10749A8;   // prim-model layout at the effect point 0xF1 (0xFC bytes decoded)
	static const uint32_t LAYOUT_B = 0x1075800;   // prim-model layout at the effect point 0xF0 (0x30C bytes decoded)
	static const uint32_t FEATHER_MODEL = 0x10747B0;
	static const uint32_t SEQUENCE = 0x1074658;   // sparkle sequence
	static const void *const SOUND = (const void *)0x1078970;
	static const uint32_t DEPTH_VEC = 0x1078978;  // 3 x int16: the layout B depth probe
	static const uint32_t COLOURS = 0x1078980;    // the flash colour table (4-byte entries)
	static const uint32_t ARENA_A = 0x2460;        // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x1A460;
	static const uint32_t MORPH = 0x2340;          // magic buffer: blended vertex frames
	static const int32_t TARGET_END = 0x4A, SPARKLE_FRAMES = 12;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void RotationY(int16_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)((int32_t)(uint16_t)angle, out); }   // Effect_RotationYMatrix (low word only)
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_RenderPrimModel: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
	inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); } // ColourLerp
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline uint32_t mulhi_u(uint32_t a, uint32_t b) { return (uint32_t)(((uint64_t)a * b) >> 32); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x18 bytes (0x2507C88)
	{
		TaskNode hdr;
		int16_t wrap;      // +0x0C 0..9
		int16_t ticks;     // +0x0E
		uint8_t next;      // +0x10 next target index
		uint8_t started;   // +0x11 the pools are set up
		uint8_t pad12[2];
		uint32_t arena;    // +0x14 packet arena parity
	};
	struct TargetNode // pool of 6 nodes of 0x420 bytes (magic buffer + 0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t saved_y;   // +0x0E the target's height +0x1E at the start
		uint8_t flag;      // +0x10 its action record's +3 & 4 (bounce back)
		uint8_t pad11[3];
		uint8_t *entity;   // +0x14
		uint8_t layout_a[0xFC];  // +0x18
		uint8_t layout_b[0x30C]; // +0x114
	};
	struct FeatherNode // pool of 0x30 nodes of 0x24 bytes (magic buffer + 0x18C0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t scale;     // +0x0E
		int16_t pos[3];    // +0x10
		int16_t vy;        // +0x16 added to y every tick
		int16_t ang[3];    // +0x18
		int16_t pad1E;
		int8_t spin[3];    // +0x20 added to the angles every tick
		uint8_t pad23;
	};
	struct SparkleNode // pool of 0x30 nodes of 0x14 bytes (magic buffer + 0x1F80)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C (y rises 0x1E a tick)
		int16_t frame;     // +0x12
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 the layout frame (facing, effect point) through the camera
		int32_t scale[3];  // +0x20 s, s, s
		int32_t scaled;    // +0x2C 1
		int32_t depth;     // +0x30
		uint32_t morph;    // +0x34 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x18 && sizeof(TargetNode) == 0x420 && sizeof(FeatherNode) == 0x24 && sizeof(SparkleNode) == 0x14, "Rapture nodes");
	static_assert(sizeof(PrimArg) == 0x38, "Rapture block");

	inline int16_t &S16(uint8_t *p, int o) { return *(int16_t *)(p + o); }

	// ------------------------------------------------------------------
	// Prim-player callback (0x679970): one object of a layout
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
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
		Mat4x3 m = {}; // (pad +0x12 never written in the original: GTE R33 word only)
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		const int32_t scaled = arg->scaled;
		int16_t off[4] = {}; // (4th word never written in the original: GTE VZ0 high half only)
		if (scaled)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
		}
		else
		{
			off[0] = r->pos[0];
			off[1] = r->pos[1];
			off[2] = r->pos[2];
		}
		if (scaled) Scale3DMatrix(&m, arg->scale);
		const uint32_t flags = r->flags;
		if (flags & 0x200)
		{
			m.t[0] = off[0];
			m.t[1] = off[1];
			m.t[2] = off[2];
		}
		else
		{
			// the offset through the block matrix; the rotation composed with it (unless flag 0x8000)
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&arg->m, &m);
		}
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
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
		PacketCursor() = RenderPrimModel2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the block scale of a target (its +0x26 size)
	static int32_t TargetScale(int16_t size) { return ((((int32_t)size >> 1) + 0x800) >> 2); }

	// layout A's frame: the facing turn, the effect point 0xF1 (x / z) at the height +0x24, through the camera
	static void BlockA(uint8_t *e, int16_t facing, PrimArg *a)
	{
		RotationY((int16_t)-facing, &a->m);
		int16_t p[3];
		GetEffectSpawnPosition(e, 0xF1, 0, p);
		a->m.t[0] = p[0];
		a->m.t[2] = p[2];
		a->m.t[1] = S16(e, 0x24);
		ComposeAffineTransform(&Camera(), &a->m, &a->m);
	}
	// layout B's frame: the facing turn at the effect point 0xF0, through the camera; its depth from the probe vector
	static void BlockB(uint8_t *e, int16_t facing, PrimArg *a)
	{
		RotationY((int16_t)-facing, &a->m);
		int16_t p[3];
		GetEffectSpawnPosition(e, 0xF0, 0, p);
		a->m.t[0] = p[0];
		a->m.t[1] = p[1];
		a->m.t[2] = p[2];
		ComposeAffineTransform(&Camera(), &a->m, &a->m);
		int16_t v[3];
		MatrixMultiplyVector(&a->m, (const int16_t *)DEPTH_VEC, v);
		a->depth = mul32((int32_t)S16(e, 0x26) + 0x800, v[2]) >> 15;
	}

	// a feather / sparkle draw (the task's draw part; the held frames draw copies)
	static void FeatherDraw(const FeatherNode *f)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		Mat4x3 m = {}; // (pad +0x12: GTE R33 word only)
		BuildRotationMatrixFromAngles(f->ang, &m);
		m.t[0] = f->pos[0];
		m.t[1] = f->pos[1];
		m.t[2] = f->pos[2];
		const int32_t v[3] = { f->scale, f->scale, f->scale };
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = FEATHER_MODEL;
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
	static void SparkleDraw(const SparkleNode *s)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(s->pos, 0x1000, -0x400);
		*(uint32_t *)h = SEQUENCE;
		*(int16_t *)(h + 4) = s->frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag288_rapture_held.h"
#endif

namespace ff8fx
{
namespace rapture288
{
	// ------------------------------------------------------------------
	// Action result of a target (0x679C10): every target record of action 0 whose slot is this entity's
	// ------------------------------------------------------------------
	static void ApplyResult(uint8_t *e)
	{
		const int32_t slot = (int32_t)((uint32_t)e - 0x1D972C0) / 0x9C;
		for (int32_t i = 0; i < (int32_t)Ctx()->actions[0].target_count; i++)
		{
			uint8_t *t = Ctx()->actions[0].targets + i * TARGET_STRIDE;
			if ((int32_t)*t == slot) ApplyActionResultToTarget(t);
		}
	}

	// ------------------------------------------------------------------
	// Sparkle (0x679D90): sequence frame = counter at the point, rising, 12 frames
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkleTask(TaskNode *n)
	{
		SparkleNode *s = (SparkleNode *)n;
		// 30 fps layer: see mag288_rapture_held.inc
		FX_HELD(held_note_part(s, 2);)
		SparkleDraw(s);
		s->pos[1] = (int16_t)(s->pos[1] + 0x1E);
		s->frame++;
		return s->frame < SPARKLE_FRAMES ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Feather (0x679C80): drawn, then turned / falling; shrinks after 10 ticks
	// ------------------------------------------------------------------
	static uint32_t __cdecl FeatherTask(TaskNode *n)
	{
		FeatherNode *f = (FeatherNode *)n;
		// 30 fps layer: see mag288_rapture_held.inc
		FX_HELD(held_note_part(f, 1);)
		FeatherDraw(f);
		f->ang[0] = (int16_t)(f->ang[0] + (int16_t)f->spin[0]);
		f->ang[1] = (int16_t)(f->ang[1] + (int16_t)f->spin[1]);
		f->ang[2] = (int16_t)(f->ang[2] + (int16_t)f->spin[2]);
		f->pos[1] = (int16_t)(f->pos[1] + f->vy);
		f->counter++;
		if (f->counter >= 10)
		{
			f->scale = (int16_t)(f->scale - 0x100);
			if (f->scale < 0x100) return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Target (0x679510)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		uint8_t *e = t->entity;
		// 30 fps layer: see mag288_rapture_held.inc
		FX_HELD(held_note_target(t);)
		int16_t def[4];
		GetDefaultEffectPosition(e, def);
		const int32_t s = TargetScale(S16(e, 0x26));
		PrimArg arg;
		arg.scale[0] = arg.scale[1] = arg.scale[2] = s;
		arg.scaled = 1;
		const int32_t c = t->counter;
		if ((uint32_t)(c - 4) < 0x1A)
		{
			if ((uint32_t)(c - 4) < 4)
			{
				for (int k = 0; k < 2; k++)
				{
					FeatherNode *f = (FeatherNode *)AddTaskToQueue(QFeather(), ORIG_FeatherTask);
					if (!f) continue;
					f->counter = 0;
					f->scale = (int16_t)((CrtRand() & 0x7FF) + 0x800);
					const int32_t a = CrtRand();
					const int32_t rad = mul32(CrtRand() + 0x10000, s) >> 16;
					f->pos[0] = (int16_t)((mul32(ComputeSin(a), rad) >> 12) + def[0]);
					f->pos[1] = 0;
					f->pos[2] = (int16_t)((mul32(ComputeCos(a), rad) >> 12) + def[2]);
					f->vy = (int16_t)((int32_t)(0u - (uint32_t)mul32(CrtRand() + 0x8000, s)) >> 18);
					f->ang[0] = (int16_t)CrtRand();
					f->ang[1] = (int16_t)CrtRand();
					f->ang[2] = (int16_t)CrtRand();
					f->spin[0] = (int8_t)(CrtRand() + 0x80);
					f->spin[1] = (int8_t)(CrtRand() + 0x80);
					f->spin[2] = (int8_t)(CrtRand() + 0x80);
				}
			}
			BlockA(e, S16(e, 0xE), &arg);
			arg.morph = TexBase() + MORPH;
			arg.depth = 0;
			prim::play((prim::Layout *)t->layout_a, PrimCallback, (int)&arg, 0);
		}
		if ((uint32_t)(c - 8) < 0x42 && !(*(const uint16_t *)e & 4) && (*(const uint16_t *)e & 2))
		{
			BlockB(e, S16(e, 0xE), &arg);
			arg.morph = TexBase() + MORPH;
			prim::play((prim::Layout *)t->layout_b, PrimCallback, (int)&arg, 0);
		}
		if (c == 0) BdPlaySE(SOUND, 0, 0x80);
		if ((uint32_t)c < 0x10 && !(c & 1))
		{
			SparkleNode *p = (SparkleNode *)AddTaskToQueue(QSparkle(), ORIG_SparkleTask);
			if (p)
			{
				const int32_t a = CrtRand();
				const int32_t rad = mul32(CrtRand() + 0x10000, s) >> 16;
				p->pos[0] = (int16_t)((mul32(ComputeSin(a), rad) >> 12) + def[0]);
				p->pos[2] = (int16_t)((mul32(ComputeCos(a), rad) >> 12) + def[2]);
				p->pos[1] = (int16_t)((int32_t)(0u - (uint32_t)mul32(CrtRand() + 0x8000, s)) >> 15);
				p->frame = 0;
			}
		}
		const uint32_t k = (uint32_t)(c - 0x17);
		if (k < 0x28)
		{
			const int32_t w = (int32_t)(mulhi_u(k << 13, 0xCCCCCCCDu) >> 5);
			e[1] |= 0x10;
			if (t->flag) S16(e, 0x1E) = (int16_t)((mul32(ComputeCos(w) - 0x1000, s) >> 14) + t->saved_y);
			else
			{
				const int32_t a = ComputeCos(w >> 2);
				const int32_t b = ComputeCos((0x1000 - a) >> 2);
				const int32_t up = mul32(0x1000 - b, s);
				const int32_t bob = mul32(ComputeCos(w) - 0x1000, s);
				S16(e, 0x1E) = (int16_t)((bob >> 12) - (up >> 10) + t->saved_y);
			}
		}
		else if (k == 0x28)
		{
			*(uint16_t *)e &= 0xEFFF;
			S16(e, 0x1E) = t->saved_y;
			ApplyResult(e);
			if (!t->flag) e[0] |= 4;
		}
		if (t->counter >= TARGET_END) return TASK_END;
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x679290)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag288_rapture_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->arena = 1;
		}
		if (r->wrap == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTarget(), (void *)TexBase(), 0x420, 6);
				InitTaskQueuePool(QFeather(), (void *)(TexBase() + 0x18C0), 0x24, 0x30);
				InitTaskQueuePool(QSparkle(), (void *)(TexBase() + 0x1F80), 0x14, 0x30);
			}
			if (r->next < Ctx()->actions[0].target_count)
			{
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTarget(), ORIG_TargetTask);
				if (t)
				{
					Memset32((uint8_t *)t + 0xC, 0, 0x105);
					const uint8_t *rec = Ctx()->actions[0].targets + r->next * TARGET_STRIDE;
					t->entity = Entity(rec[0]);
					t->flag = rec[3] & 4;
					t->saved_y = S16(t->entity, 0x1E);
					DecodeModelPrimLayout(LAYOUT_A, t->layout_a, 0xFC);
					DecodeModelPrimLayout(LAYOUT_B, t->layout_b, 0x30C);
				}
				r->next++;
			}
		}
		// the screen flash of tick k = ticks - 1
		uint32_t level = 0, colour = 0;
		const uint32_t k = (uint32_t)((int32_t)r->ticks - 1);
		if (k < 0x4A)
		{
			if (k < 0xC) level = mulhi_u((k * 3) << 10, 0xAAAAAAABu) >> 3;
			else if (k >= 0x3E) level = mulhi_u(0x37800u - ((k * 3) << 10), 0xAAAAAAABu) >> 3;
			else level = 0xC00;
			const uint32_t x = (k * 5) << 12;
			const uint32_t q = mulhi_u(x, 0xBACF914Du);
			const uint32_t w = (((x - q) >> 1) + q) >> 6;
			const int32_t idx = (int32_t)w >> 12, frac = (int32_t)(w & 0xFFF);
			BlendColour((const void *)(COLOURS + idx * 4), (const void *)(COLOURS + 4 + idx * 4), 0x1000 - frac, frac, &colour);
		}
		int a;
		if (r->started)
		{
			a = ExecuteTaskQueue(QTarget());
			ExecuteTaskQueue(QFeather());
			ExecuteTaskQueue(QSparkle());
		}
		else a = (int)colour;
		SetScreenFlash(level, colour);
		if (r->started && a == 0)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		r->wrap++;
		if (r->wrap >= 10) r->wrap = 0;
		r->ticks++;
		return 0;
	}
}

	void register_mag288_rapture()
	{
		register_port(rapture288::ORIG_RootTask, (void *)rapture288::RootTask, "288 RootTask", 288);
		register_port(rapture288::ORIG_TargetTask, (void *)rapture288::TargetTask, "288 TargetTask", 288);
		register_port(rapture288::ORIG_FeatherTask, (void *)rapture288::FeatherTask, "288 FeatherTask", 288);
		register_port(rapture288::ORIG_SparkleTask, (void *)rapture288::SparkleTask, "288 SparkleTask", 288);
		// 30 fps layer: see mag288_rapture_held.inc
		FX_HELD(register_mag288_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag288_rapture_held.inc"
#endif
