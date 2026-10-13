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

// Effect 101: Trauma's grab (enemy attack 290 of kernel.bin, no name; Trauma c0m109; no other
// kernel.bin entry uses effect 101; MAG_101_*). Not the player's Thundara: that is effect 102.
//
// Structure (setup MAG_101_UNK5 0x6DD0C0 -> 0x6DD0F0, file loader 0x6DD0D0 = the texture file named
// at 0x13249E0 (mag100.tim); the setup keeps the cast context, the CASTER's entity and the first
// action's first TARGET's entity (target flags |= 4 | 8), starts camera animation 0x13248C0 and
// queues the TIM; its root queue 0x2544E48 holds one 0x14-byte node):
//   RootTask (0x6DD1A0) - alternates the packet arena (magic buffer + 0x450 / + 0x10450); on counter 1
//     (once) sets up the grab pool (1 x 0x2D0 at the magic buffer), starts the grab task, saves the
//     target's position / colour words and size in it, puts the target in front of the caster
//     (caster x, y 300, caster z - 1000) and starts its chain transformation 0xE; runs the grab
//     queue; ends (screen flash 0) when the grab queue is empty.
//   GrabTask (0x6DD300) - node 0x2D0, counter 0..0x35, every tick:
//     target entity +0x30 = 0; screen flash level 0 -> 0x898 over 12 ticks, 0x898, back to 0 from 42;
//     counters 0..20: the target's colour (+0x2C rgb) scaled by s = 0x1000 - sin((20 - c) * 1024 / 20),
//       +0x2F = 0x32, +0x30 = the scale vector 0x2544E18 = (s, s, s), y from s; at 20 the colour
//       words are put back;
//     counters 0..0x25: layout 0x1321BEC at (caster x, -0xA8C, caster z + 1000), depth offset -0x80;
//     counter 0: sound 0x13249DC;
//     counters 0..0x25: the target held at the caster (x + the word +0x34 of its model header, y 1200,
//       z - 400; this overrides the y above), its position copied to +0x54..+0x5C, its rotation
//       matrix +0x40..+0x50 = a half turn about y; at counter 23 its flag bit 2 is cleared;
//     counters 0x26..0x35: flag bit 3 cleared; the target's position moves towards the saved one
//       (VecLerp of the current position, weight (c - 0x26) / 16 of the saved one);
//     at 0x36 the saved position / colour words are put back, screen flash 0, end.
//     No damage is applied by this module.
//   PartCallback (0x6DD630) - one object of the layout (same code as Petrify Stare's 0x6B9770):
//     record flag 0x40000 selects the rotation builder 0x7015B0, else 0x701310; flag 0x200 = the
//     offset stays in screen axes, else anchor + offset through the camera; scale by scale3DMatrix.
//     Blended vertex frames go to magic buffer + 0x2D0 (0x180 bytes before arena A).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// The effect WRITES the target entity: position words +0x1C / +0x1E / +0x20 (+0x22 when restoring),
// +0x2C..+0x33 (colour, scale vector pointer), rotation matrix +0x40..+0x51, +0x54..+0x5F, flags +0.
// It does not write the camera.
// Module globals: 0x2544E10..0x2544E74 (texture file, scale vector, context, caster / target entities,
// grab / root queues, root pool, packet cursor 0x2544E6C, magic buffer base 0x2544E70). The grab pool,
// the blend buffer (+0x2D0) and the packet arenas are in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace grab101
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t *ScaleVector() { return (int32_t *)0x2544E18; }          // (s, s, s) for the target
	inline CastContext *&Ctx() { return var<CastContext *>(0x2544E28); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2544E2C); }          // caster entity (setup)
	inline uint8_t *&Target() { return var<uint8_t *>(0x2544E30); }          // target entity (setup)
	inline TaskQueue *QGrab() { return (TaskQueue *)0x2544E38; }             // pool: magic buffer, 1 x 0x2D0
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2544E6C); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2544E70); }          // Magic_TextureOFF (magic buffer)
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6DD1A0;
	static const uint32_t ORIG_GrabTask = 0x6DD300;
	static const uint32_t MODEL_Grab = 0x1321BEC;        // prim-model layout data (0x2B0-byte layout)
	static const void *const SOUND_Grab = (const void *)0x13249DC;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	// MAG_070_sub_7043B0: prim model draw (header +0 model, +4 vertices, +8 fade colour, +0xC fade,
	// +0x18 depth offset, +0x1C mode)
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	// 0x45E0E0: IR1..3 = 3 x u8 (zero-extended); 0x45E410: IR1..3 low bytes -> 3 x u8
	inline void GteLoadIRBytes(const void *rgb) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(rgb); }
	inline void GteStoreIRBytes(void *rgb) { fn<void (__cdecl *)(void *)>(0x45E410)(rgb); }
	// sub_56CB90: out = (a * wa + b * wb) >> 12 on 3 x s16
	inline void VecLerp(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CB90)(a, b, wa, wb, out); }
	// QueueChainTransformation (0x505C00): battle anim-seq task id on the entity
	inline void QueueChainTransformation(uint8_t *entity, int32_t id) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, id); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F grab pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct GrabNode // pool of 1 node of 0x2D0 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int32_t size;      // +0x10 target entity +0x26 (kept, never read)
		uint32_t colour;   // +0x14 target entity +0x2C..+0x2F as taken
		uint32_t pos_xy;   // +0x18 target entity +0x1C / +0x1E as taken
		uint32_t pos_z;    // +0x1C target entity +0x20 / +0x22 as taken
		uint8_t layout[0x2B0]; // +0x20 prim-model layout (prim::Layout: data, frame, state)
	};
	// the prim-player callback's parameter block (a stack block of the grab task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 anchor x, y, z (+ a word never written)
		int32_t depth;     // +0x08 prim header +0x18
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(GrabNode) == 0x2D0, "Trauma grab nodes");
	static_assert(sizeof(PrimArg) == 0x10, "Trauma grab prim block");

	// ------------------------------------------------------------------
	// Prim-player callback (0x6DD630): one object of the layout
	// ------------------------------------------------------------------
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
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
		Mat4x3 m = {};
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x200)
		{
			// the anchor through the camera, the offset added in screen axes
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			// anchor + offset through the camera, rotation composed with the camera
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
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
		PacketCursor() = RenderPrimModel2(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag101_trauma_grab_held.h"
#endif

namespace ff8fx
{
namespace grab101
{
	// screen flash level of a grab counter (x / 12 compiled as 0x2AAAAAAB, sar 1)
	static uint32_t FlashLevel(int16_t c)
	{
		if (c < 0xC) return (uint32_t)(((int32_t)c * 0x898) / 12);
		if (c >= 0x2A) return (uint32_t)(((0x36 - (int32_t)c) * 0x898) / 12);
		return 0x898;
	}

	// ------------------------------------------------------------------
	// Grab (0x6DD300)
	// ------------------------------------------------------------------
	static uint32_t __cdecl GrabTask(TaskNode *n)
	{
		GrabNode *g = (GrabNode *)n;
		*(uint32_t *)(Target() + 0x30) = 0;
		SetScreenFlash(FlashLevel(g->counter), 0);
		const int32_t c = g->counter;
		if ((uint32_t)c < 0x26)
		{
			if ((uint32_t)c <= 0x14)
			{
				const int32_t s = 0x1000 - ComputeSin(((0x14 - c) << 10) / 20);
				GteSetIR0(s);
				GteLoadIRBytes(&g->colour);
				GteGPF();
				GteStoreIRBytes(Target() + 0x2C);
				*(uint8_t *)(Target() + 0x2F) = 0x32;
				*(int32_t **)(Target() + 0x30) = ScaleVector();
				ScaleVector()[2] = s;
				ScaleVector()[1] = s;
				ScaleVector()[0] = s;
				*(int16_t *)(Target() + 0x1E) = (int16_t)((s * 3 - 0x3000) * 900 / 4096 + 0x12C);
				if (c == 0x14) *(uint32_t *)(Target() + 0x2C) = g->colour;
			}
			PrimArg arg;
			arg.pos[0] = *(const int16_t *)(Caster() + 0x1C);
			arg.pos[1] = (int16_t)0xF574;
			arg.pos[2] = (int16_t)(*(const int16_t *)(Caster() + 0x20) + 0x3E8);
			// never written by the original (stack word), only loaded into the GTE V0 pad
			arg.pos[3] = 0;
			arg.depth = -0x80;
			arg.morph = TexBase() + 0x2D0;
			// 30 fps layer: see mag101_trauma_grab_held.inc
			FX_HELD(held_note_play((prim::Layout *)g->layout, &arg);)
			prim::play((prim::Layout *)g->layout, PartCallback, (int)&arg, 0);
		}
		if (g->counter == 0) BdPlaySE(SOUND_Grab, 0, 0x80);
		const int32_t e = (int32_t)g->counter - 0x26;
		if (e < 0x10)
		{
			const int32_t t = (e << 12) / 16;
			if (t < 0)
			{
				// held at the caster, turned half way round
				const uint8_t *hdr = **(uint8_t *const *const *)(Target() + 0x64);
				*(int16_t *)(Target() + 0x1C) = (int16_t)(*(const int16_t *)(hdr + 0x34) + *(const int16_t *)(Caster() + 0x1C));
				*(int16_t *)(Target() + 0x20) = (int16_t)(*(const int16_t *)(Caster() + 0x20) - 0x190);
				*(int16_t *)(Target() + 0x1E) = 0x4B0;
				*(int32_t *)(Target() + 0x54) = *(const int16_t *)(Target() + 0x1C);
				*(int32_t *)(Target() + 0x58) = *(const int16_t *)(Target() + 0x1E);
				*(int32_t *)(Target() + 0x5C) = *(const int16_t *)(Target() + 0x20);
				*(int16_t *)(Target() + 0x40) = (int16_t)0xF000;
				*(int16_t *)(Target() + 0x46) = 0;
				*(int16_t *)(Target() + 0x4C) = 0;
				*(int16_t *)(Target() + 0x42) = 0;
				*(int16_t *)(Target() + 0x48) = 0x1000;
				*(int16_t *)(Target() + 0x4E) = 0;
				*(int16_t *)(Target() + 0x44) = 0;
				*(int16_t *)(Target() + 0x4A) = 0;
				*(int16_t *)(Target() + 0x50) = (int16_t)0xF000;
				if (e == -0xF) *(uint16_t *)Target() &= 0xFFFB;
			}
			else
			{
				// back towards the saved position
				*(uint16_t *)Target() &= 0xFFF7;
				VecLerp(Target() + 0x1C, &g->pos_xy, 0x1000 - t, t, Target() + 0x1C);
			}
		}
		g->counter++;
		if (g->counter >= 0x36)
		{
			*(uint32_t *)(Target() + 0x1C) = g->pos_xy;
			*(uint32_t *)(Target() + 0x20) = g->pos_z;
			*(uint32_t *)(Target() + 0x2C) = g->colour;
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6DD1A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag101_trauma_grab_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x450;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x10450;
			r->arena = 1;
		}
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			InitTaskQueuePool(QGrab(), (void *)TexBase(), 0x2D0, 1);
			GrabNode *g = (GrabNode *)AddTaskToQueue(QGrab(), ORIG_GrabTask);
			Memset32(&g->counter, 0, 0xB1);
			DecodeModelPrimLayout(MODEL_Grab, g->layout, 0x2B0);
			g->pos_xy = *(const uint32_t *)(Target() + 0x1C);
			g->pos_z = *(const uint32_t *)(Target() + 0x20);
			g->colour = *(const uint32_t *)(Target() + 0x2C);
			g->size = *(const int16_t *)(Target() + 0x26);
			*(int16_t *)(Target() + 0x1C) = *(const int16_t *)(Caster() + 0x1C);
			*(int16_t *)(Target() + 0x20) = (int16_t)(*(const int16_t *)(Caster() + 0x20) - 0x3E8);
			*(int16_t *)(Target() + 0x1E) = 0x12C;
			QueueChainTransformation(Target(), 0xE);
		}
		int a;
		if (r->started) a = ExecuteTaskQueue(QGrab());
		else a = (int)n;
		if (r->started && a == 0)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		r->counter++;
		return 0;
	}
}

	void register_mag101_trauma_grab()
	{
		register_port(grab101::ORIG_RootTask, (void *)grab101::RootTask, "101 RootTask", 101);
		register_port(grab101::ORIG_GrabTask, (void *)grab101::GrabTask, "101 GrabTask", 101);
		// 30 fps layer: see mag101_trauma_grab_held.inc
		FX_HELD(register_mag101_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag101_trauma_grab_held.inc"
#endif
