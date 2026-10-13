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

// Effect 132: Dissolving Acid (enemy attack 183 of kernel.bin, Malboro c0m048; MAG_132_*). No other
// kernel.bin entry uses effect 132.
//
// Structure (setup MAG_132_DISOLVING_ACID 0x6B19F0 -> _Init 0x6B1A20, file loader 0x6B1A00 = the
// texture file named at 0x121906C; the setup keeps the CASTER's entity and the cast context, starts
// camera animation 0x1214E1C and queues the TIM; setup / loader / _Init are the copies of 126..131):
//   MasterTask (0x6B1AA0) - alternates the packet arena (magic buffer + 0x4BDC / + 0x14BDC); on
//     counter 1 sets up the pools (first time: targets 3 x 0x14 at + 0x3B80, layouts 0x10 x 0x2D8 at
//     + 0xE00, streams 0x10 x 0x40 at + 0xA00, an unused queue 0x20 x 0x18 at + 0x700, drops 0x40 x
//     0x1C at + 0, the 0x80 stream points at + 0x3BBC freed) and starts a target task for the next
//     action's first target (the action index runs 0..last_action, one per counter-1 tick; while a
//     target task of that entity runs the counter stays at 1; after a spawn the counter moves on, so
//     only action 0 is started). Then the target, layout, unused and stream queues and the drop list
//     (0x6B1CC0); ends when all five are empty.
//   TargetTask (0x6B1DB0) - counter 0..0x2D: sound at 0xA; on the even counters 0x14..0x1C one acid
//     stream: its start = the table point 0x1214FE8 + 16 i through the caster's bone (0x6B2B70), its
//     end = a random bone of the target (CRT rand), its control point = start + (the table's second
//     vector through the caster's matrix) / 2 (x, z) (0x6B1EC0); when the table's flag word is 0 the
//     end drops to the floor (y = 0) beside the target, x + table x / 2 through the caster's matrix.
//     Damage at 0x2D, then ends.
//   StreamTask (0x6B2030) - node 0x40, UPDATES THEN DRAWS: a chain of points (pool + 0x3BBC, 0x1C
//     bytes) grows by one point per tick (8 more after the first); the phase += 0x100 picks the head's
//     weight w = table 0x1219038[phase / 256] - 0xC0, every next point 0xC0 less (w >= 0x1080: 0x1000);
//     each point = quadratic Bezier (start, control, end) at w through the GTE (IR0 interpolation);
//     a head past 0x1080 is freed (the first one spawns the end layout: 0x6B23F0 splash on the body,
//     0x6B27E0 puddle on the floor); fewer than 2 points: ends. Three random drops per tick (CRT rand
//     & 3 = a point index) from that point towards the next. Then every point projected (OTZ =
//     SZ3 / 4 via 0x704090), the textured ribbon 0x6B2870 (edges 0x6B29E0 = Breath's / Quezacotl's
//     ribbon edges, GP0 0x3E quads for the first 8 points), and counters 0..7 a sprite sequence
//     0x1212D90 at the start.
//   DropTask (0x6B2310) - node 0x1C, DRAWS THEN UPDATES: run by the drop list (0x6B1CC0 / 0x6B1D60, a
//     per-tick sprite header for model 0x1212B70 on the Field_Alloc scratch, published in 0x2520F84):
//     the drop's position through the camera into the header, flipbook frame (counter) through
//     0x683490; 8 frames; moves by its velocity.
//   SplashTask (0x6B2450) - node 0x2D8, DRAWS THEN UPDATES: layout 0x1212F64 or 0x1213C04 (CRT rand)
//     at the stream's end, callback 0x6B2560; sequences 0x1212E6C (counters 0..8) and 0x1212C4C
//     (counters 4..15) through the shadow-rotation camera; 16 ticks.
//   PuddleTask (0x6B2820) - node 0x2D8, DRAWS THEN UPDATES: layout 0x12148A4 on the floor, callback
//     0x6B2560; ends when the layout is finished.
//   PartCallback (0x6B2560) - Petrify Stare's 0x6BED30 / Trauma's 0x6DD630 without the rotation
//     variants and the depth word, drawn by 0x572200.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA: no task writes a battle entity (position +0x1C..+0x20 only read) or the camera
// words (the camera animation is the engine's).
// Module globals: 0x2520F60..0x2520FF0 (caster entity 0x2520F60, texture file 0x2520F64, magic
// buffer base 0x2520F68, context 0x2520F6C, master pool 0x2520F70, drop header 0x2520F84, packet
// cursor 0x2520F88, the queues 0x2520F90 drops / 0x2520FA0 unused / 0x2520FB0 streams / 0x2520FC0
// layouts / 0x2520FD0 targets / 0x2520FE0 master).
// NEVER-WRITTEN STACK WORDS (the game's executor 0x508420 calls every task of a queue at one esp;
// the master pushes one more queue argument per executor call: target tasks start at E, layouts at
// E - 4, the unused queue at E - 8, streams at E - 0xC; DR0 write watches under the game executor):
//   - the ribbon draw's joint direction local (0x6B2870 [esp], stream task entry esp - 0x2C): read
//     when the chain's first edge has no screen length (its first two points on one pixel). Its last
//     writer is the projection loop's last 0x704090 (push ecx, then GTE_ReadDataReg(0x13) into that
//     slot): the dword SZ3 of the last point (Ribbon's slot argument).
//   - the stream's start comes from a stack vector of the target task (E - 8) whose 4th word (E - 2)
//     0x6B2B70 never writes; 0x6B1EC0 copies it into the stream node's +0x1E (overwritten) and +0x26
//     (kept, never read). The target task is the first task of a tick: the word is what lies there
//     when the tick starts. The module's own writers come after it in the tick (the stream queue
//     executor's push of the master's edi, each drop's return address 0x6B1D75); between two effect
//     ticks the battle loop runs at the effect root's depth: engine residue, taken as 0.

#include "mag_common.h"
#include "mag_ice_common.h"

namespace ff8fx
{
namespace acid132
{
	using namespace eng;
	using namespace magc;
	using namespace icec;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2520F60); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2520F68); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2520F6C); }
	inline uint8_t *&DropHeader() { return var<uint8_t *>(0x2520F84); }    // the drop list's sprite header (per tick)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2520F88); }
	inline TaskQueue *QDrops() { return (TaskQueue *)0x2520F90; }          // pool: magic buffer, 0x40 x 0x1C
	inline TaskQueue *QUnused() { return (TaskQueue *)0x2520FA0; }         // pool: + 0x700, 0x20 x 0x18 (never used)
	inline TaskQueue *QStreams() { return (TaskQueue *)0x2520FB0; }        // pool: + 0xA00, 0x10 x 0x40
	inline TaskQueue *QLayouts() { return (TaskQueue *)0x2520FC0; }        // pool: + 0xE00, 0x10 x 0x2D8
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2520FD0; }        // pool: + 0x3B80, 3 x 0x14

	static const uint32_t ORIG_MasterTask = 0x6B1AA0;
	static const uint32_t ORIG_TargetTask = 0x6B1DB0;
	static const uint32_t ORIG_StreamTask = 0x6B2030;
	static const uint32_t ORIG_DropTask = 0x6B2310;
	static const uint32_t ORIG_SplashTask = 0x6B2450;
	static const uint32_t ORIG_PuddleTask = 0x6B2820;
	static const uint32_t POINTS = 0x3BBC;               // magic buffer: stream points, 0x80 x 0x1C
	static const uint32_t MORPH = 0x49BC;                // magic buffer: blended vertex frames
	static const uint32_t MODEL_Drop = 0x1212B70;        // drop sprite model (0x683490)
	static const uint32_t SEQ_Stream = 0x1212D90;        // sprite sequence at a stream's start
	static const uint32_t SEQ_Splash1 = 0x1212E6C;
	static const uint32_t SEQ_Splash2 = 0x1212C4C;
	static const uint32_t MODEL_Splash1 = 0x1212F64;     // prim-model layouts
	static const uint32_t MODEL_Splash2 = 0x1213C04;
	static const uint32_t MODEL_Puddle = 0x12148A4;
	static const void *const SOUND_Acid = (const void *)0x1214FD0;
	static const uint8_t *const STREAM_TABLE = (const uint8_t *)0x1214FE8; // 5 x 16: point, bone / vector, flag
	static const int16_t *const WEIGHTS = (const int16_t *)0x1219038;     // head weight by phase / 256
	static const uint8_t *const DIST_TABLE = (const uint8_t *)0x1215038;  // [|dy|][|dx|] -> length, 128 x 128

	// engine functions not in fx_port.h / mag_common.h / mag_ice_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void StoreOTZ4(int16_t *dst) { fn<void (__cdecl *)(int16_t *)>(0x704090)(dst); } // MAG_138_sub_704090: GTE SZ3 >> 2 (read into a stack slot)
	// MAG_106_sub_683490: the drop sprite (header +0x2C frame data, +0x30 its first word, +0x98 camera position)
	inline uint32_t DrawDropSprite(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x683490)(h, ot, mode, cursor); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2520F70)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t action;    // +0x0E next action to start
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 3 nodes of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E
		uint8_t *entity;   // +0x10 the action's first target
	};
	struct Point // stream point, pool of 0x80 x 0x1C (magic buffer + 0x3BBC)
	{
		int16_t pos[3];    // +0x00 world position
		int16_t width;     // +0x06 ribbon width (0x96); -1 = free
		int16_t sx, sy;    // +0x08 screen position, then the ribbon edge (one side)
		int16_t otz;       // +0x0C SZ3 / 4 (-1: hidden by a zero-length first edge)
		int16_t idx;       // +0x0E creation order in the chain
		int16_t sx2, sy2;  // +0x10 the ribbon edge (other side)
		uint8_t pad14[4];
		Point *next;       // +0x18
	};
	struct StreamNode // pool of 0x10 nodes of 0x40 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t count;     // +0x0E points in the chain
		Point *head;       // +0x10 oldest point
		Point *tail;       // +0x14 newest point
		int16_t start[3];  // +0x18 Bezier start (the caster's mouth)
		int16_t phase;     // +0x1E
		int16_t ctrl[4];   // +0x20 control point (+0x26: the never-written stack word, never read)
		int16_t end[4];    // +0x28 end (+0x2E: 1 = on the floor -> puddle, 0 = on the body -> splash)
		uint8_t pad30[8];
		int16_t slot;      // +0x38 the target's entity slot (never read)
		int16_t width;     // +0x3A point width (0x96)
		int16_t add;       // +0x3C points still to add
		int16_t pad3E;
	};
	struct DropNode // pool of 0x40 nodes of 0x1C bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t frame;     // +0x12
		int16_t vel[3];    // +0x14
		int16_t pad1A;
	};
	struct LayoutNode // pool of 0x10 nodes of 0x2D8 bytes (splash / puddle)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[4];    // +0x10 the stream's end (+0x16 its floor flag)
		uint8_t layout[0x2C0]; // +0x18 prim-model layout
	};
	// the prim-player callback's parameter block (a stack block of the layout tasks)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 the node's position (+0x10..+0x17)
		uint32_t morph;    // +0x08 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(TargetNode) == 0x14 && sizeof(Point) == 0x1C && sizeof(StreamNode) == 0x40, "Dissolving Acid nodes");
	static_assert(sizeof(DropNode) == 0x1C && sizeof(LayoutNode) == 0x2D8 && sizeof(PrimArg) == 0xC, "Dissolving Acid nodes");

	// the never-written stack word below the target task (module comment): engine residue, 0
	static const int16_t TARGET_PAD = 0;

	// ------------------------------------------------------------------
	// Prim-player callback (0x6B2560): one object of a splash / puddle layout. Vertex frame (or blend
	// into the block's morph buffer), rotation from the record's angles; flag 0x200 = the anchor
	// through the camera with the offset in screen axes, else anchor + offset through the camera and
	// the rotation composed with it; scaled (scale3DMatrix); drawn by 0x572200.
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
		RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x200)
		{
			// the anchor through the camera, the offset added in screen axes
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
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
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
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
		// +0x18 (depth offset) is not written: the scratch word under the header
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Sprite sequence at a fixed position through the shadow-rotation camera (header 0xB4 bytes)
	// ------------------------------------------------------------------
	static void SequenceAt(uint8_t *h, const int16_t *pos, int32_t a, int32_t b, uint32_t seq, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, a, b);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	// splash sequences of counter c (header allocated after the layout)
	static void SplashSequences(const LayoutNode *s)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		if (s->counter < 9) SequenceAt(h, s->pos, 0x800, -0x200, SEQ_Splash1, s->counter);
		const int32_t e = (int32_t)s->counter - 4;
		if ((uint32_t)e < 0xC) SequenceAt(h, s->pos, 0x800, -0x200, SEQ_Splash2, (int16_t)e);
		FieldFree(0xB4);
	}

	// stream sequence of counter c at the start (counters 0..7)
	static void StreamSequence(const StreamNode *s)
	{
		TransformCameraByShadowRotation(s->start, 0x1000, -0x400);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_Stream;
		*(int16_t *)(h + 4) = s->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// Drop list header (0x6B1CC0): sprite model 0x1212B70, rotation / scale words, colour from the
	// model; published in 0x2520F84
	// ------------------------------------------------------------------
	static uint8_t *DropListHeader()
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		DropHeader() = h;
		*(uint32_t *)h = MODEL_Drop;
		const uint8_t *model = *(const uint8_t **)h;
		*(uint16_t *)(h + 0x92) = 0;
		*(uint16_t *)(h + 0x90) = 0;
		*(uint16_t *)(h + 0x8E) = 0;
		*(uint16_t *)(h + 0x8A) = 0;
		*(uint16_t *)(h + 0x88) = 0;
		*(uint16_t *)(h + 0x86) = 0;
		*(uint16_t *)(h + 0x94) = 0x400;
		*(uint16_t *)(h + 0x8C) = 0x400;
		*(uint16_t *)(h + 0x84) = 0x400;
		*(uint16_t *)(h + 0x60) = 0;
		*(uint16_t *)(h + 0x58) = 0;
		*(uint16_t *)(h + 0x50) = 0;
		*(uint16_t *)(h + 0x48) = 0;
		h[0x20] = model[4];
		h[0x21] = model[5];
		h[0x22] = model[6];
		h[0x23] = model[7];
		return h;
	}

	// a drop's draw: its position through the camera into the header, flipbook frame
	static void DropDraw(uint8_t *h, const DropNode *d)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(d->pos);
		GteMVMVA_RotV0();
		int32_t *t = (int32_t *)(h + 0x98);
		GteReadMAC123(t);
		t[0] = (int32_t)((uint32_t)t[0] + (uint32_t)Camera().t[0]);
		t[1] = (int32_t)((uint32_t)t[1] + (uint32_t)Camera().t[1]);
		t[2] = (int32_t)((uint32_t)t[2] + (uint32_t)Camera().t[2]);
		uint8_t *model = *(uint8_t **)h;
		uint8_t *frame = model + *(const uint16_t *)(model + 0xA + (int32_t)d->frame * 2);
		*(uint8_t **)(h + 0x2C) = frame;
		*(uint32_t *)(h + 0x30) = *(const uint32_t *)frame;
		*(uint8_t **)(h + 0x2C) = frame + 4;
		PacketCursor() = DrawDropSprite(h, RenderOT(), 2, PacketCursor());
	}

	// ------------------------------------------------------------------
	// Stream points (0x6B1FF0: the first free point of the pool, its width set, next cleared)
	// ------------------------------------------------------------------
	static Point *AllocPoint(int16_t width)
	{
		Point *p = (Point *)(TexBase() + POINTS);
		for (int32_t i = 0x80; i; i--, p++)
		{
			if (p->width == -1)
			{
				p->next = nullptr;
				p->width = width;
				return p;
			}
		}
		return nullptr;
	}

	// a point of the stream at weight w: quadratic Bezier start / control / end through the GTE
	// (IR0 interpolation: (start * (1 - w) + control * w) * (1 - w) + end * w)
	static void BezierPoint(const StreamNode *s, int32_t w, int16_t *out)
	{
		const int32_t iw = 0x1000 - w;
		int16_t tmp[3];
		GteSetIR0(iw);
		GteLoadIR123(s->start);
		GteGPF();
		GteSetIR0(w);
		GteLoadIR123(s->ctrl);
		GteGPL();
		GteStoreIR123(tmp);
		GteSetIR0(iw);
		GteLoadIR123(tmp);
		GteGPF();
		GteSetIR0(w);
		GteLoadIR123(s->end);
		GteGPL();
		GteStoreIR123(out);
	}

	// projection of the chain (0x704090: OTZ = SZ3 >> 2); returns the last SZ3 (GTE data register 19,
	// what 0x704090 leaves in its slot below the stream task: the ribbon's direction local)
	static uint32_t ProjectChain(Point *p)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		do
		{
			GteLoadV0(p);
			GteRTPS();
			GteReadSXY2(&p->sx);
			StoreOTZ4(&p->otz);
			p = p->next;
		} while (p);
		return var<uint32_t>(0x1CA8A5C);
	}

	// Ribbon edges of point s towards n (0x6B29E0, Breath's 0x6B6E50 / Quezacotl's 0x6C8DE0). dir
	// carries the previous segment's direction (mitred joints).
	static void SegEdges(Point *s, const Point *n, const int16_t *prev_dir, int16_t *dir)
	{
		if (s->otz < 0) return;
		int32_t ox = 0, oy = 0;
		if (n)
		{
			if (n->otz < 0) return;
			int32_t dx = n->sx - s->sx;
			int32_t ndy = s->sy - n->sy;
			int32_t adx = dx < 0 ? -dx : dx, ady = ndy < 0 ? -ndy : ndy;
			if (adx >= 0x80) adx = 0x7F;
			if (ady >= 0x80) ady = 0x7F;
			int32_t len = DIST_TABLE[ady * 128 + adx];
			int32_t px, py;
			if (prev_dir)
			{
				if (len == 0)
				{
					dir[0] = prev_dir[0];
					dir[1] = prev_dir[1];
					px = prev_dir[0];
					py = prev_dir[1];
				}
				else
				{
					int32_t cx = shl32(ndy, 12) / len, cy = shl32(dx, 12) / len;
					int32_t sx = cx + prev_dir[0], sy = cy + prev_dir[1];
					dir[0] = (int16_t)cx;
					dir[1] = (int16_t)cy;
					if (sx == 0 && sy == 0) { sx = cx * 2; sy = cy * 2; }
					int32_t sq = (int32_t)((uint32_t)mul32(sx, sx) + (uint32_t)mul32(sy, sy));
					int32_t k = 0x4000 - (sq >> 13);
					px = mul32(k, sx) >> 13;
					py = mul32(k, sy) >> 13;
				}
			}
			else
			{
				if (len == 0) { s->otz = -1; return; }
				px = shl32(ndy, 12) / len;
				py = shl32(dx, 12) / len;
				dir[0] = (int16_t)px;
				dir[1] = (int16_t)py;
			}
			int32_t w = shl32(n->width, 5) / (s->otz + 0x200);
			ox = mul32(w, px) >> 12;
			oy = mul32(w, py) >> 12;
		}
		int16_t x = s->sx, y = s->sy;
		s->sx = (int16_t)(x - ox);
		s->sx2 = (int16_t)(x + ox);
		s->sy2 = (int16_t)(y + oy);
		s->sy = (int16_t)(y - oy);
	}

	// Ribbon (0x6B2870): the edges of every point (first, middle, last), then one GP0 0x3E quad
	// (gouraud 0x808080, textured, texture row by the point's index) per pair whose both points are
	// visible, for the first 8 points. The direction local starts with the slot word (module comment).
	static void Ribbon(Point *head, int32_t count, uint32_t slot)
	{
		int16_t dir[2] = { (int16_t)slot, (int16_t)(slot >> 16) };
		SegEdges(head, head->next, nullptr, dir);
		Point *s = head->next;
		for (int32_t i = count - 2; i; i--)
		{
			SegEdges(s, s->next, dir, dir);
			s = s->next;
		}
		SegEdges(s, nullptr, dir, nullptr);

		Point *a = head;
		for (int32_t i = count - 1; i; i--)
		{
			Point *b = a->next;
			if (a->otz > 0 && b->otz > 0 && a->idx <= 7)
			{
				uint32_t *p = (uint32_t *)PacketCursor();
				PacketCursor() += 0x34;
				p[2] = *(const uint32_t *)&a->sx;
				p[5] = *(const uint32_t *)&a->sx2;
				const uint32_t c = (uint32_t)mul32((uint16_t)(a->idx + 1), 7) << 13;
				uint32_t v = (c + 0x2000) & 0xFFFF;
				if (v > 0xFF00) v = 0xFF00;
				p[6] = v | 0xB90078;
				p[3] = v | 0x3C140048;
				*(uint16_t *)((uint8_t *)p + 0x24) = (uint16_t)(c | 0x48);
				*(uint16_t *)((uint8_t *)p + 0x30) = (uint16_t)(c | 0x78);
				p[8] = *(const uint32_t *)&b->sx;
				p[1] = 0x808080;
				p[11] = *(const uint32_t *)&b->sx2;
				p[4] = 0x808080;
				p[10] = 0x808080;
				p[7] = 0x808080;
				p[0] = 0xC000000;
				((uint8_t *)p)[7] = 0x3E;
				InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + (((int32_t)b->otz + (int32_t)a->otz) >> 3) * 4 + 0x44, p);
			}
			a = b;
		}
	}

	// ------------------------------------------------------------------
	// Layout spawns at the stream's end (0x6B23F0 splash / 0x6B27E0 puddle; no spawn check)
	// ------------------------------------------------------------------
	static void SpawnSplash(const int16_t *end)
	{
		LayoutNode *s = (LayoutNode *)AddTaskToQueue(QLayouts(), ORIG_SplashTask);
		s->counter = 0;
		*(uint32_t *)&s->pos[0] = *(const uint32_t *)&end[0];
		*(uint32_t *)&s->pos[2] = *(const uint32_t *)&end[2];
		if (CrtRand() & 1) DecodeModelPrimLayout(MODEL_Splash1, s->layout, 0xB0);
		else DecodeModelPrimLayout(MODEL_Splash2, s->layout, 0xB0);
	}
	static void SpawnPuddle(const int16_t *end)
	{
		LayoutNode *s = (LayoutNode *)AddTaskToQueue(QLayouts(), ORIG_PuddleTask);
		s->counter = 0;
		*(uint32_t *)&s->pos[0] = *(const uint32_t *)&end[0];
		*(uint32_t *)&s->pos[2] = *(const uint32_t *)&end[2];
		DecodeModelPrimLayout(MODEL_Puddle, s->layout, 0x78);
	}

	// ------------------------------------------------------------------
	// Stream spawn (0x6B1EC0): its first point, the node (start a1, control a1 + v / 2 in x / z, end
	// a2, which drops to the floor beside the target when the table's flag word is 0)
	// ------------------------------------------------------------------
	static void SpawnStream(const int16_t *a1, int16_t *a2, int32_t slot, int32_t width, const int16_t *v, const int16_t *entry)
	{
		Point *p = AllocPoint((int16_t)width);
		if (!p) return;
		StreamNode *s = (StreamNode *)AddTaskToQueue(QStreams(), ORIG_StreamTask);
		if (!s)
		{
			p->width = -1;
			return;
		}
		p->idx = 0;
		if (entry[3] == 0)
		{
			int16_t d[4] = { (int16_t)(entry[0] >> 1), 0, 0, 0 };
			MatrixMultiplyVector((const Mat4x3 *)(Caster() + 0x40), d, d);
			a2[0] = (int16_t)(a2[0] + d[0]);
			a2[2] = (int16_t)(a2[2] + d[2]);
			a2[1] = 0;
			a2[3] = 1;
		}
		else a2[3] = 0;
		s->counter = 0;
		memcpy(s->start, a1, 8);
		memcpy(s->ctrl, a1, 8);
		s->ctrl[0] = (int16_t)(s->ctrl[0] + (int16_t)(v[0] >> 1));
		s->ctrl[1] = (int16_t)(s->ctrl[1] + v[1]);
		s->ctrl[2] = (int16_t)(s->ctrl[2] + (int16_t)(v[2] >> 1));
		memcpy(s->end, a2, 8);
		s->tail = p;
		s->head = p;
		s->phase = 0;
		s->count = 1;
		s->slot = (int16_t)slot;
		s->add = 8;
		s->width = (int16_t)width;
	}

	// 0x6B2B70: a table point through the caster's bone (word +6) composed with its matrix
	static void BonePoint(const int16_t *entry, int16_t *out)
	{
		uint8_t *c = Caster();
		Mat4x3 m = {};
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(**(uint8_t ***)(c + 0x64) + 0x20 + (int32_t)entry[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(entry);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag132_dissolving_acid_held.h"
#endif

namespace ff8fx
{
namespace acid132
{
	// ------------------------------------------------------------------
	// Drop (0x6B2310): run by the drop list
	// ------------------------------------------------------------------
	static uint32_t __cdecl DropTask(TaskNode *n)
	{
		DropNode *d = (DropNode *)n;
		// 30 fps layer: see mag132_dissolving_acid_held.inc
		FX_HELD(held_note_drop(d);)
		DropDraw(DropHeader(), d);
		d->frame++;
		if (d->frame >= 8) return TASK_END;
		d->pos[1] = (int16_t)(d->pos[1] + d->vel[1]);
		d->pos[2] = (int16_t)(d->pos[2] + d->vel[2]);
		d->pos[0] = (int16_t)(d->pos[0] + d->vel[0]);
		return 0;
	}

	// Drop list (0x6B1CC0 + 0x6B1D60): the per-tick header, then every node of the list; finished
	// nodes are freed and unlinked, the tail becomes the last survivor. Returns the nodes left.
	static uint32_t RunDrops()
	{
		// 30 fps layer: see mag132_dissolving_acid_held.inc
		FX_HELD(held_note_drop_list();)
		DropListHeader();
		TaskQueue *q = QDrops();
		TaskNode *prev = nullptr;
		uint32_t alive = 0;
		for (TaskNode *t = q->head; t; t = t->next)
		{
			void *port = lookup((uint32_t)t->func);
			const uint32_t r = (port ? (TaskFn)port : t->func)(t);
			if (r & 2)
			{
				t->flags = 0;
				if (prev) prev->next = t->next;
				else q->head = t->next;
			}
			else
			{
				prev = t;
				alive++;
			}
		}
		q->tail = prev;
		FieldFree(0xB4);
		return alive;
	}

	// ------------------------------------------------------------------
	// Splash (0x6B2450) / puddle (0x6B2820) layouts
	// ------------------------------------------------------------------
	static uint32_t __cdecl SplashTask(TaskNode *n)
	{
		LayoutNode *s = (LayoutNode *)n;
		PrimArg arg;
		memcpy(arg.pos, s->pos, sizeof(arg.pos));
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag132_dissolving_acid_held.inc
		FX_HELD(held_note_layout(s, &arg, true);)
		prim::play((prim::Layout *)s->layout, PartCallback, (int)&arg, 0);
		// 30 fps layer: see mag132_dissolving_acid_held.inc
		FX_HELD(held_note_sequence(s, false);)
		SplashSequences(s);
		s->counter++;
		return s->counter < 0x10 ? 0 : TASK_END;
	}

	static uint32_t __cdecl PuddleTask(TaskNode *n)
	{
		LayoutNode *s = (LayoutNode *)n;
		PrimArg arg;
		memcpy(arg.pos, s->pos, sizeof(arg.pos));
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag132_dissolving_acid_held.inc
		FX_HELD(held_note_layout(s, &arg, false);)
		return prim::play((prim::Layout *)s->layout, PartCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Stream (0x6B2030)
	// ------------------------------------------------------------------
	static uint32_t __cdecl StreamTask(TaskNode *n)
	{
		StreamNode *s = (StreamNode *)n;
		bool grow = true;
		if (s->add > 0)
		{
			Point *p = AllocPoint(s->width);
			if (!p)
			{
				s->phase = 0x1000;
				grow = false;
			}
			else
			{
				p->idx = s->count;
				s->count++;
				s->tail->next = p;
				s->add--;
				s->tail = p;
			}
		}
		if (grow)
		{
			s->phase = (int16_t)(s->phase + 0x100);
			Point *p = s->head;
			int32_t w = WEIGHTS[(int32_t)s->phase / 256];
			do
			{
				w -= 0xC0;
				if (w > 0x1080 && p == s->head)
				{
					// the head passed the end: freed (the first point starts the end layout)
					if (p->idx == 0)
					{
						if (s->end[3] == 0) SpawnSplash(s->end);
						else SpawnPuddle(s->end);
					}
					Point *next = p->next;
					p->width = -1;
					s->count--;
					s->head = next;
					if (s->count < 2) return TASK_END;
				}
				if (w >= 0x1080) w = 0x1000;
				BezierPoint(s, w, p->pos);
				p = p->next;
			} while (p);

			// three random drops: from the point whose index is rand & 3 towards the next one
			for (int32_t k = 3; k; k--)
			{
				const int32_t r = CrtRand() & 3;
				Point *a = s->head;
				for (Point *b = a->next; b; a = b, b = b->next)
				{
					if (a->idx != r) continue;
					DropNode *d = (DropNode *)AddTaskToQueue(QDrops(), ORIG_DropTask);
					if (!d) continue;
					*(uint32_t *)&d->pos[0] = *(const uint32_t *)&a->pos[0];
					*(uint32_t *)&d->pos[2] = *(const uint32_t *)&a->pos[2];
					d->frame = 0;
					int32_t q = CrtRand();
					d->vel[0] = (int16_t)((q & 0x7F) + (((int32_t)b->pos[0] - (int32_t)a->pos[0]) >> 1) - 0x40);
					q = CrtRand();
					d->vel[1] = (int16_t)((q & 0x7F) + (((int32_t)b->pos[1] - (int32_t)a->pos[1]) >> 1) - 0x40);
					q = CrtRand();
					d->vel[2] = (int16_t)((q & 0x7F) + (((int32_t)b->pos[2] - (int32_t)a->pos[2]) >> 1) - 0x40);
				}
			}
		}
		// 30 fps layer: see mag132_dissolving_acid_held.inc
		FX_HELD(held_note_stream(s, grow);)
		const uint32_t slot = ProjectChain(s->head);
		if (s->count >= 2) Ribbon(s->head, s->count, slot);
		if (s->counter < 8)
		{
			// 30 fps layer: see mag132_dissolving_acid_held.inc
			FX_HELD(held_note_sequence(s, true);)
			StreamSequence(s);
		}
		s->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Target (0x6B1DB0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		if (t->counter == 0xA) BdPlaySE(SOUND_Acid, 0, 0x80);
		const uint32_t e = (uint32_t)((int32_t)t->counter - 0x14);
		if (e < 0xA && !(e & 1))
		{
			const int16_t *entry = (const int16_t *)(STREAM_TABLE + (e >> 1) * 16);
			int16_t a1[4];
			BonePoint(entry, a1);
			a1[3] = TARGET_PAD;
			uint8_t *ent = t->entity;
			int16_t a2[4];
			const int32_t bone = (int32_t)((uint32_t)CrtRand() * (uint32_t)***(uint8_t ***)(ent + 0x64)) >> 15;
			GetEffectSpawnPosition(ent, bone, 0, a2);
			int16_t v[4];
			MatrixMultiplyVector((const Mat4x3 *)(Caster() + 0x40), entry + 4, v);
			const int32_t slot = (int32_t)((uint32_t)t->entity - 0x1D972C0u) / 0x9C;
			SpawnStream(a1, a2, slot, 0x96, v, entry + 4);
		}
		if (t->counter >= 0x2D)
		{
			ApplyActionResultToTarget(Ctx()->actions[0].targets);
			return TASK_END;
		}
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x6B1AA0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag132_dissolving_acid_held.inc
		FX_HELD(held_note_master();)
		if (m->arena)
		{
			PacketCursor() = TexBase() + 0x4BDC;
			m->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x14BDC;
			m->arena = 1;
		}
		if (m->counter == 1)
		{
			if (!m->started)
			{
				m->started = 1;
				InitTaskQueuePool(QTargets(), (void *)(TexBase() + 0x3B80), 0x14, 3);
				InitTaskQueuePool(QLayouts(), (void *)(TexBase() + 0xE00), 0x2D8, 0x10);
				InitTaskQueuePool(QStreams(), (void *)(TexBase() + 0xA00), 0x40, 0x10);
				InitTaskQueuePool(QUnused(), (void *)(TexBase() + 0x700), 0x18, 0x20);
				InitTaskQueuePool(QDrops(), (void *)TexBase(), 0x1C, 0x40);
				Point *p = (Point *)(TexBase() + POINTS);
				for (int32_t i = 0x80; i; i--, p++) p->width = -1;
			}
			ActionData *actions = Ctx()->actions;
			if (m->action <= actions[0].last_action)
			{
				uint8_t *ent = Entity(actions[m->action].targets[0]);
				bool busy = false;
				const TargetNode *t = (const TargetNode *)(TexBase() + 0x3B80);
				for (int32_t i = 0; i < 3; i++, t++)
				{
					if ((t->hdr.flags & 1) && t->entity == ent)
					{
						busy = true;
						break;
					}
				}
				if (busy) m->counter = 0;
				else
				{
					TargetNode *nt = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
					if (nt)
					{
						Memset32(&nt->counter, 0, 2);
						nt->action = m->action;
						nt->entity = ent;
						m->action++;
					}
				}
			}
		}
		int a0, a1, a2, a3, a4;
		if (m->started)
		{
			a0 = ExecuteTaskQueue(QTargets());
			a1 = ExecuteTaskQueue(QLayouts());
			a2 = ExecuteTaskQueue(QUnused());
			a3 = ExecuteTaskQueue(QStreams());
			a4 = (int)RunDrops();
		}
		else a0 = a1 = a2 = a3 = a4 = (int)n;
		if (m->started && a0 == 0 && a1 == 0 && a2 == 0 && a3 == 0 && a4 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag132_dissolving_acid()
	{
		register_port(acid132::ORIG_MasterTask, (void *)acid132::MasterTask, "132 MasterTask", 132);
		register_port(acid132::ORIG_TargetTask, (void *)acid132::TargetTask, "132 TargetTask", 132);
		register_port(acid132::ORIG_StreamTask, (void *)acid132::StreamTask, "132 StreamTask", 132);
		register_port(acid132::ORIG_DropTask, (void *)acid132::DropTask, "132 DropTask", 132);
		register_port(acid132::ORIG_SplashTask, (void *)acid132::SplashTask, "132 SplashTask", 132);
		register_port(acid132::ORIG_PuddleTask, (void *)acid132::PuddleTask, "132 PuddleTask", 132);
		// 30 fps layer: see mag132_dissolving_acid_held.inc
		FX_HELD(register_mag132_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag132_dissolving_acid_held.inc"
#endif
