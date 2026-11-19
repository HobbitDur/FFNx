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

// Effect 301: Gargantua's burrow (enemy attack 265 of kernel.bin, no name; Gargantua c0m094; MAG_301_*).
//
// Structure (setup 0x66D8E0 -> _Init 0x66D910, file loader 0x66D8F0 = mag300.tim (name at 0x103DCA0) into 0x25076D8; _Init
// queues the TIM, keeps the context (0x25076DC) and the CASTER's entity (0x25076E0, its word +0 in 0x2507740, never read
// again), keeps the words +0 of the party entities 0 / 1 / 2 (0x2507744 / 48 / 4C) and sets their bit 2, sets bits 2 / 3 of
// the caster's word +0, sets up the root queue 0x2507718 (pool 0x2507728, 1 x 0x14) and starts camera animation 0x103DB5C):
//   RootTask (0x66D9F0) - alternates the packet arena (magic buffer + 0x6204 / + 0x16204), sets bits 2 / 3 of the caster's
//     word +0; on counter 1 (once) sets up the pools (master 1 x 0xE14 at the magic buffer, rocks 0x20 x 0x1C at + 0xE14,
//     dust 0x20 x 0x18 at + 0x1194) and starts the master: voice slot 0x103BBD8, layout 0x103D46C, the caster's default effect
//     position (y = 0), the caster's colour +0x2C kept, an empty ground-mark table. Then the master, rock and dust queues; when
//     the master queue is empty: bit 2 of the party entities' words put back, bits 2 / 3 of the caster's word cleared, the end.
//   MasterTask (0x66DBD0) - counters 1..141. The caster's bit 3 cleared every tick. Counters 1..121: the layout at the
//     position (prim player, callback 0x66DD50), the caster's bit 2 set, the caster's body redrawn by the module (0x66E300:
//     polygons whose three / four vertices all lie below the ground line y > 0x40 are left out; polygons crossing it leave a
//     ground mark at their centre every 8th polygon); counters 1..101: the rocks and dust puffs of this tick from the spawn
//     stream (0x103DC9C: a count, then 10 words per rock / 6 words per puff). From counter 122: bits 2 / 3 of the caster
//     cleared; counters 122..141 (d = counter - 122): the caster's colour +0x2C = its kept colour faded by
//     0x1000 - sin((20 - d) x 0x400 / 20) (byte +0x2F = 0x32). Every tick: the ground marks (12 ticks each, flipbook
//     0x103CFB8). Sound 0x103DC98 at counter 1, the voice slot released at counter 138.
//   RockTask (0x66EC50) - prim model 0x103D314 at its position, turned by RotX(x angle) x RotY(y angle) (bytes x 16), then
//     moves by its velocity bytes (x / z x 16, y x 4), bounces (y velocity -v / 2 at or below the ground), falls (y velocity +=
//     2), spins; 16 ticks.
//   DustTask (0x66EE60) - sprite sequence 0x103D118 through the shadow-rotation camera (0x1000, -0x400), frame = its counter,
//     then moves by its velocity bytes; 16 ticks.
//   PrimCallback (0x66DD50) - one layout object drawn by 0x7043B0 (header 0x58): vertex frame (or blend into the block's
//     buffer), rotation from the record's angles, at the block's point + the record's offset, through the camera, scale, fade.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update); the master spawns its rocks / puffs after its
// layout and body draws (they draw in the same tick), the marks after the body draw spawned this tick's marks.
// ENTITY WRITES: the caster's word +0 bits 2 / 3, its world bone matrices (0x5095B0) and bone matrices from its pose (0x508C90)
// on every body draw, its colour +0x2C / +0x2F (counters 122..141); bit 2 of the words +0 of the party entities 0..2 (put
// back at the end). No position word is written.
// CAMERA: the setup's camera animation 0x103DB5C (engine); no camera word is written by the tasks.
// Module globals: 0x25076D8..0x2507758 (texture file 0x25076D8, context 0x25076DC, caster 0x25076E0, the queues 0x25076E8
// dust / 0x25076F8 rocks / 0x2507708 master / 0x2507718 root, root pool 0x2507728, packet cursor 0x250773C, the kept words
// 0x2507740..0x250774C, magic buffer base 0x2507754). Exe data the module writes: the spawn stream pointer 0x103DC9C
// (0x103BD00 at start; it is never put back: a second cast reads on past the 101 ticks of the stream).
// MAGIC BUFFER: + 0x1494 = the layout's blended vertex frames AND the body's vertex buffer (16 bytes per vertex).
// STACK WORDS: the mark sprites' draw (0x66E090) keeps the bucket of its last inserted sprite in its own `mode` argument
// slot: the next sprite's depth is shifted by it (and 31). The 4th words of the vectors loaded into GTE V0 (the record offset,
// the dust position) reach the GTE's VZ0 high half (0x1CA8A16) only.
// FUJIN: Fujin's described Gargantua burrow (effect_301.json) runs this code: its memory through own::R() (fx_port.h),
// its spawns through own::Fn(), the file loader (0x66D8F0) and the setup (0x66D8E0 -> _Init 0x66D910) ported below; its
// parameters (BurrowParams: the sounds and the master's ticks, the layout, the body's ground line and marks, the spawn
// stream's ticks, the rocks, the puffs, the marks, the colour fade; the layout, the rock mesh, 2 flipbooks, the spawn
// stream, the texture name and the camera track). A copy reads its own spawn stream at the game's pointer's offset (the
// pointer stays the game's exe word). FX_PART tags its parts.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"

namespace ff8fx
{
namespace burrow301
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x25076DC)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x25076E0)); }        // caster entity (setup)
	inline TaskQueue *QDust() { return (TaskQueue *)own::R(0x25076E8); }           // pool: + 0x1194, 0x20 x 0x18
	inline TaskQueue *QRocks() { return (TaskQueue *)own::R(0x25076F8); }          // pool: + 0xE14, 0x20 x 0x1C
	inline TaskQueue *QMaster() { return (TaskQueue *)own::R(0x2507708); }         // pool: magic buffer, 1 x 0xE14
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x250773C)); }
	inline uint16_t &PartyWord(int i) { return var<uint16_t>(own::R(0x2507744) + i * 4); } // the party entities' words +0 (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2507754)); }        // Magic_TextureOFF (magic buffer)
	inline uint8_t *&Stream() { return var<uint8_t *>(0x103DC9C); }        // spawn stream (exe data)
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }    // battle_texture_data_ptr (frame arena)
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_RootTask = 0x66D9F0;
	static const uint32_t ORIG_MasterTask = 0x66DBD0;
	static const uint32_t ORIG_RockTask = 0x66EC50;
	static const uint32_t ORIG_DustTask = 0x66EE60;
	static const uint32_t ORIG_PrimCallback = 0x66DD50;  // not a task: the layout's callback
	static const uint32_t POOL_Rocks = 0xE14, POOL_Dust = 0x1194;
	static const uint32_t VERTS = 0x1494;               // magic buffer: blended vertex frames / the body's vertices
	static const uint32_t ARENA_A = 0x6204, ARENA_B = 0x16204; // magic buffer: packet arenas
	static const uint32_t MODEL_Layout = 0x103D46C;      // prim-model layout
	static const uint32_t MODEL_Rock = 0x103D314;        // prim model
	static const uint32_t SEQ_Dust = 0x103D118;          // sprite sequence
	static const uint32_t SEQ_Mark = 0x103CFB8;          // mark flipbook: +8 frame count, then the frames' offsets
	static const void *const SOUND_Voice = (const void *)0x103BBD8;
	static const void *const SOUND_Start = (const void *)0x103DC98;
	static const int32_t GROUND = 0x40;                 // the body's ground line (y below it: under the ground)

	static const uint32_t ORIG_FileLoader = 0x66D8F0, ORIG_Setup = 0x66D8E0;
	static const uint32_t TEXTURE_NAME = 0x103DCA0, CAMERA_ANIM = 0x103DB5C;
	static const uint32_t STREAM_Data = 0x103BD00, STREAM_SIZE = 0x12B8;   // the spawn stream: the 101 ticks' rocks and puffs
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x25076D8)); }     // the effect's file (its TIM)

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_LAYOUT = 1, PART_BODY = 2, PART_MARKS = 3, PART_ROCKS = 4, PART_DUST = 5 };
	struct BurrowParams
	{
		struct { uint32_t start, voice; } sounds;      // the start sound, the voice (its slot claimed at the start)
		struct { int16_t start, release, end; } timing; // the master's ticks: the start sound, the voice released, its end
		struct { int16_t ticks, depth; } layout;       // the layout (and the body) on the master's ticks below `ticks`, depth
		// the body: its ground line (vertices below it are under the ground), the marks' depth limit (its z - this), every
		// n-th polygon crossing the line leaves a mark
		struct { int16_t ground, mark_depth, mark_every; } body;
		struct { int16_t ticks; } spawn;               // the stream's rocks and puffs on the master's ticks below this
		// the rocks: their ticks, the fall speed's growth a tick, the velocity bytes' scale (x / z, y)
		struct { int16_t life, gravity, speed_xz, speed_y; } rocks;
		struct { int16_t life, size; } dust;           // the puffs: their ticks (their flipbook's frames), size
		struct { int16_t life; } marks;                // the ground marks' ticks (their flipbook's frame count - life)
		// the caster's colour faded back in from `start` over `ticks` (its byte +0x2F set to alpha)
		struct { int16_t start, ticks, alpha; } fade;
		struct { uint32_t main; } layouts;             // the prim-model layout at the caster
		struct { uint32_t rock; } models;              // the rocks' mesh
		struct { uint32_t dust, mark; } seqs;          // the puffs' and the marks' flipbooks
		struct { uint32_t stream; } tables;            // the spawn stream (a count, 10 words a rock; a count, 6 words a puff)
		struct { uint32_t texture_name, camera; } res; // its resources (data): texture file name, camera track
	};
	static const BurrowParams GAME = { { 0x103DC98, 0x103BBD8 }, { 1, 0x8A, 0x8E }, { 0x7A, 0x100 }, { GROUND, 0x320, 8 },
		{ 0x66 }, { 0x10, 2, 16, 4 }, { 0x10, 0x1000 }, { 0xC }, { 0x7A, 0x14, 0x32 }, { MODEL_Layout }, { MODEL_Rock },
		{ SEQ_Dust, SEQ_Mark }, { STREAM_Data }, { TEXTURE_NAME, CAMERA_ANIM } };
	static BurrowParams live = GAME;
	static const BurrowParams &PG() { return live; }

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline uint32_t RenderPrimModel70(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_RenderPrimModel
	inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }                // Effect_RotationYMatrix
	inline void RotX(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701220)(angle, out); }                // MAG_063_RotationXMatrix
	inline void MatrixMultiply3x3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }  // a.rot = a.rot x b.rot
	inline void GteLoadIRBytes(const void *src) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(src); }   // IR1..3 = 3 x u8
	inline void GteStoreIRBytes(void *dst) { fn<void (__cdecl *)(void *)>(0x45E410)(dst); }        // IR1..3 low bytes
	// software GTE
	inline void GteWriteDataReg(uint32_t value, int32_t reg) { fn<void (__cdecl *)(uint32_t, int32_t)>(0x45D7C0)(value, reg); }
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // V0 = 3 x u16
	inline void GteStoreIR123Minus8(void *end) { fn<void (__cdecl *)(void *)>(0x703EF0)(end); } // IR1..3 -> end - 8
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteSetSZ123(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45E1D0)(a, b, c, d); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }    // GTE_ReadOTZ (dword)
	inline void GteLoadIRColumn(const void *m) { fn<void (__cdecl *)(const void *)>(0x45E180)(m); } // GTE_LoadIRFromMatrixColumn
	inline void GteStoreIRColumn(void *m) { fn<void (__cdecl *)(void *)>(0x45E470)(m); }           // GTE_StoreIRToMatrixColumn
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }
	inline void GteLoadV0Dwords(const void *v) { fn<void (__cdecl *)(const void *)>(0x45E060)(v); } // GTE_LoadV0FromDwords
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2507728)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct Mark // a ground mark (8 bytes)
	{
		int16_t x, z;      // +0x00 world position (y = 0)
		int16_t index;     // +0x04 the polygon index that left it
		int16_t life;      // +0x06 ticks left (12 .. 1; 0 = free)
	};
	struct MasterNode // pool of 1 node of 0xE14 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 1..141
		int16_t pad0E;
		uint32_t voice;    // +0x10 voice slot
		int16_t pos[4];    // +0x14 the caster's default effect position (y = 0)
		uint8_t layout[0x90]; // +0x1C
		Mark marks[0x80];  // +0xAC
		uint8_t index[0x960]; // +0x4AC per polygon index: its mark (0xFF: none; never set by the module)
		Mark *next;        // +0xE0C next mark slot to try
		uint32_t colour;   // +0xE10 the caster's colour +0x2C as it was
	};
	struct RockNode // pool of 0x20 nodes of 0x1C bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 0..15
		int8_t vel[3];     // +0x14 x / z x 16, y x 4
		int8_t gravity;    // +0x17 (2)
		int8_t angle[2];   // +0x18 x / y (x 16)
		int8_t spin[2];    // +0x1A
	};
	struct DustNode // pool of 0x20 nodes of 0x18 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame, 0..15
		int8_t vel[3];     // +0x14
		uint8_t pad17;     // +0x17 (2, unused)
	};
	// the prim-player callback's parameter block (a stack block of the master task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 the effect position (4th word: copied, unused)
		int32_t depth;     // +0x08 the header's +0x18 (0x100)
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(MasterNode) == 0xE14 && sizeof(RockNode) == 0x1C && sizeof(DustNode) == 0x18, "Gargantua burrow nodes");
	static_assert(sizeof(Mark) == 8 && sizeof(PrimArg) == 0x10, "Gargantua burrow blocks");

	template<typename T> static inline T &F(uint8_t *s, uint32_t off) { return *(T *)(s + off); }
}
}

#ifdef FF8_FX_HELD
#include "mag301_gargantua_burrow_held.h"
#endif

namespace ff8fx
{
namespace burrow301
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x66DD50): one object of the layout. Vertex frame (or blend into the block's buffer), rotation
	// from the record's angles at the block's point + the record's offset, composed with the camera, scale (scale3DMatrix),
	// fade, drawn by 0x7043B0 (header 0x58; the fields it does not set keep the Field_Alloc scratch)
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
		RotationFromAngles(r->rot, &m);
		m.t[0] = (int32_t)r->pos[0] + arg->pos[0];
		m.t[1] = (int32_t)r->pos[1] + arg->pos[1];
		m.t[2] = (int32_t)r->pos[2] + arg->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2000;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20C0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		PacketCursor() = RenderPrimModel70(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// The body (0x66E300 -> 0x66E3B0 -> 0x66E9C0 / 0x66E4C0): the caster's model drawn by the module
	// ------------------------------------------------------------------
	// Work block (Field_Alloc 0x88): +0x00 polygon cursor, +0x04 vertex buffer, +0x08 / +0x0A triangle / quad counts, +0x0C
	// clip rectangle (x0, y0, x1, y1: x 8 in the test), +0x14 colour word (entity +0x28), +0x18 object mask (entity +0x7C),
	// +0x1C / +0x2C / +0x3C / +0x4C four vertices (16 bytes: sxy, otz, clip flags +6, ground side +7 (1 above / 2 below),
	// the world point +8), +0x5C NCLIP, +0x64 OTZ, +0x6C ground line, +0x70 master counter, +0x74 depth limit of the marks
	// (entity +0x5C - 0x320), +0x78 object cursor, +0x7C bucket table, +0x80 bucket shift, +0x84 vertex cursor.

	// sub_66E930: a ground mark at (x, 0, z) for polygon index i: its mark if it has one (never), else the next free slot
	// (from the cursor, then from the first slot), nothing when all 128 are in use; 12 ticks
	static void MarkAt(int32_t x, int32_t z, int32_t i)
	{
		// 30 fps layer: see mag301_gargantua_burrow_held.inc
		FX_HELD(if (held_no_marks()) return;)
		uint8_t *base = (uint8_t *)TexBase();
		const int16_t index = (int16_t)i;
		const uint8_t slot = base[0x4AC + index];
		Mark *e;
		if (slot != 0xFF) e = (Mark *)(base + slot * 8 + 0xAC);
		else
		{
			e = *(Mark **)(base + 0xE0C);
			if (e->life != 0)
			{
				e = (Mark *)(base + 0xAC);
				if (*(const int16_t *)(base + 0xB2) != 0)
				{
					int32_t n = 0x80;
					do
					{
						e++;
						if (--n == 0) return;
					} while (e->life != 0);
				}
			}
			*(Mark **)(base + 0xE0C) = (uintptr_t)e >= (uintptr_t)(base + 0x4A4) ? (Mark *)(base + 0xAC) : e + 1;
			e->index = index;
		}
		e->x = (int16_t)x;
		e->z = (int16_t)z;
		e->life = PG().marks.life;
	}

	// sub_66E9C0: one bone group of vertices: each through the bone matrix (world point at +8), then projected with the camera,
	// clip-flagged and sided against the ground line
	static void BodyVertices(uint8_t *s)
	{
		uint8_t *p = F<uint8_t *>(s, 0x78);
		uint8_t *out = F<uint8_t *>(s, 0x84);
		const int32_t n = *(const int16_t *)p;
		p += 2;
		if (n != 0)
		{
			uint32_t left = (uint32_t)n;
			do
			{
				GteLoadV0u(p);
				GteMVMVA_RotV0Tr();
				out += 0x10;
				p += 6;
				GteStoreIR123Minus8(out);
			} while (--left != 0);
		}
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		out = F<uint8_t *>(s, 0x84);
		if (n != 0)
		{
			uint32_t left = (uint32_t)n;
			do
			{
				GteLoadV0(out + 8);
				GteRTPS();
				const uint8_t side = (uint8_t)(((int32_t)*(const int16_t *)(out + 0xA) > F<int32_t>(s, 0x6C)) + 1);
				GteReadSXY2(s + 0x1C);
				GteReadOTZ(s + 0x20);
				if (F<uint16_t>(s, 0x20) == 0)
					out[6] = 0x10;
				else
				{
					s[0x23] = side;
					const int32_t vx = F<int16_t>(s, 0x1C);
					if (vx < shl32(F<int16_t>(s, 0xC), 3)) s[0x22] |= 1;
					else if (vx >= shl32(F<int16_t>(s, 0x10), 3)) s[0x22] |= 2;
					const int32_t vy = F<int16_t>(s, 0x1E);
					if (vy < shl32(F<int16_t>(s, 0xE), 3)) s[0x22] |= 4;
					else if (vy >= shl32(F<int16_t>(s, 0x12), 3)) s[0x22] |= 8;
					*(uint32_t *)out = F<uint32_t>(s, 0x1C);
					*(uint32_t *)(out + 4) = F<uint32_t>(s, 0x20);
				}
				out += 0x10;
			} while (--left != 0);
		}
		F<uint8_t *>(s, 0x84) = out;
		F<uint8_t *>(s, 0x78) = p;
	}

	static inline void CopyVertex(uint8_t *s, uint32_t off, const uint8_t *verts, uint16_t index)
	{
		const uint8_t *v = verts + (uint32_t)(index & 0xFFF) * 0x10;
		F<uint32_t>(s, off) = *(const uint32_t *)v;
		F<uint32_t>(s, off + 4) = *(const uint32_t *)(v + 4);
		F<uint32_t>(s, off + 8) = *(const uint32_t *)(v + 8);
		F<uint32_t>(s, off + 0xC) = *(const uint32_t *)(v + 0xC);
	}

	// sub_66E4C0: the object's textured triangles and quads (each polygon's own semi-transparency bit kept, the code byte of
	// the colour word or'ed in). A polygon all under the ground is left out; one crossing the ground line leaves a mark at its
	// centre when (master counter + its index) & 7 is 0 and its first vertex's z is beyond the depth limit
	static uint32_t BodyPolygons(uint8_t *s, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *verts = F<uint8_t *>(s, 4);
		const uint8_t *p = F<uint8_t *>(s, 0);
		const uint16_t tris = F<uint16_t>(s, 8);
		uint16_t k = tris;
		F<uint16_t>(s, 8) = (uint16_t)(k - 1);
		if (k != 0)
		{
			do
			{
				CopyVertex(s, 0x1C, verts, *(const uint16_t *)p);
				CopyVertex(s, 0x2C, verts, *(const uint16_t *)(p + 2));
				CopyVertex(s, 0x3C, verts, *(const uint16_t *)(p + 4));
				const uint8_t a = s[0x22], b = s[0x32], c = s[0x42];
				if ((uint8_t)(a | b | c) < 0x10 && !(c & (a & b)))
				{
					const uint8_t side = s[0x43] & s[0x33] & s[0x23];
					if (side != 2)
					{
						if (side == 0 && (uint32_t)(F<int32_t>(s, 0x70) + F<uint16_t>(s, 8)) % (uint32_t)PG().body.mark_every == 0 && (int32_t)F<int16_t>(s, 0x28) > F<int32_t>(s, 0x74))
						{
							const int32_t z = ((int32_t)F<int16_t>(s, 0x28) + F<int16_t>(s, 0x48) + F<int16_t>(s, 0x38)) / 3;
							const int32_t x = ((int32_t)F<int16_t>(s, 0x44) + F<int16_t>(s, 0x34) + F<int16_t>(s, 0x24)) / 3;
							MarkAt(x, z, F<uint16_t>(s, 8));
						}
						GteSetSXY012(F<uint32_t>(s, 0x1C), F<uint32_t>(s, 0x2C), F<uint32_t>(s, 0x3C));
						GteNCLIP();
						GteReadMAC0(s + 0x5C);
						if (F<int32_t>(s, 0x5C) >= 0)
						{
							GteSetSZ123(F<uint16_t>(s, 0x20), F<uint16_t>(s, 0x30), F<uint16_t>(s, 0x40));
							GteAVSZ3();
							*(uint32_t *)pk = 0x7000000;
							*(uint32_t *)(pk + 4) = ((((uint32_t)*(const uint16_t *)(p + 0xE) & 0x200) | 0x2400) << 16) | F<uint32_t>(s, 0x14);
							*(uint32_t *)(pk + 8) = F<uint32_t>(s, 0x1C);
							*(uint32_t *)(pk + 0x10) = F<uint32_t>(s, 0x2C);
							*(uint32_t *)(pk + 0x18) = F<uint32_t>(s, 0x3C);
							*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 8);
							*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 0xC);
							*(uint16_t *)(pk + 0x16) &= 0xFDFF;
							*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 6);
							GteReadOTZWord(s + 0x64);
							InsertPrimAutoDepth(F<uint32_t>(s, 0x7C) + (uint32_t)(F<int32_t>(s, 0x64) >> (F<int32_t>(s, 0x80) & 31)) * 4, pk);
							pk += 0x20;
						}
					}
				}
				k = F<uint16_t>(s, 8);
				p += 0x10;
				F<uint16_t>(s, 8) = (uint16_t)(k - 1);
			} while (k != 0);
		}
		k = F<uint16_t>(s, 0xA);
		uint16_t rest = (uint16_t)(k - 1);
		F<uint16_t>(s, 0xA) = rest;
		if (k != 0)
		{
			p += 4;
			do
			{
				CopyVertex(s, 0x1C, verts, *(const uint16_t *)(p - 4));
				CopyVertex(s, 0x2C, verts, *(const uint16_t *)(p - 2));
				CopyVertex(s, 0x3C, verts, *(const uint16_t *)p);
				CopyVertex(s, 0x4C, verts, *(const uint16_t *)(p + 2));
				const uint8_t d = s[0x52], a = s[0x22], b = s[0x32], c = s[0x42];
				if ((uint8_t)(d | a | b | c) < 0x10 && !(c & (d & a & b)))
				{
					const uint8_t side = s[0x53] & s[0x43] & s[0x33] & s[0x23];
					if (side != 2)
					{
						if (side == 0 && (uint32_t)(F<int32_t>(s, 0x70) + rest) % (uint32_t)PG().body.mark_every == 0 && (int32_t)F<int16_t>(s, 0x28) > F<int32_t>(s, 0x74))
						{
							const int32_t z = ((int32_t)F<int16_t>(s, 0x48) + F<int16_t>(s, 0x28) + F<int16_t>(s, 0x58) + F<int16_t>(s, 0x38)) / 4;
							const int32_t x = ((int32_t)F<int16_t>(s, 0x54) + F<int16_t>(s, 0x44) + F<int16_t>(s, 0x34) + F<int16_t>(s, 0x24)) / 4;
							MarkAt(x, z, (uint16_t)(rest + tris));
						}
						GteSetSXY012(F<uint32_t>(s, 0x1C), F<uint32_t>(s, 0x2C), F<uint32_t>(s, 0x3C));
						GteNCLIP();
						GteReadMAC0(s + 0x5C);
						if (F<int32_t>(s, 0x5C) >= 0)
						{
							GteSetSZ0123(F<uint16_t>(s, 0x20), F<uint16_t>(s, 0x30), F<uint16_t>(s, 0x40), F<uint16_t>(s, 0x50));
							GteAVSZ4();
							*(uint32_t *)pk = 0x9000000;
							*(uint32_t *)(pk + 4) = ((((uint32_t)*(const uint16_t *)(p + 0xA) & 0x200) | 0x2C00) << 16) | F<uint32_t>(s, 0x14);
							*(uint32_t *)(pk + 8) = F<uint32_t>(s, 0x1C);
							*(uint32_t *)(pk + 0x10) = F<uint32_t>(s, 0x2C);
							*(uint32_t *)(pk + 0x18) = F<uint32_t>(s, 0x3C);
							*(uint32_t *)(pk + 0x20) = F<uint32_t>(s, 0x4C);
							*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 4);
							*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 8);
							*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 0xC);
							*(uint16_t *)(pk + 0x16) &= 0xFDFF;
							*(uint16_t *)(pk + 0x24) = *(const uint16_t *)(p + 0xE);
							GteReadOTZWord(s + 0x64);
							InsertPrimAutoDepth(F<uint32_t>(s, 0x7C) + (uint32_t)(F<int32_t>(s, 0x64) >> (F<int32_t>(s, 0x80) & 31)) * 4, pk);
							pk += 0x28;
						}
					}
				}
				k = F<uint16_t>(s, 0xA);
				p += 0x14;
				rest = (uint16_t)(k - 1);
				F<uint16_t>(s, 0xA) = rest;
			} while (k != 0);
		}
		return (uint32_t)pk;
	}

	// sub_66E3B0: every object of the model the mask shows (bone groups of vertices, then polygons)
	static uint32_t BodyObjects(const uint8_t *model, uint8_t *s, uint32_t cursor)
	{
		const uint8_t *table = *(uint8_t *const *)(model + 4);
		const uint8_t *bones = *(uint8_t *const *)model + 0x10;
		const int32_t count = *(const int32_t *)table;
		const uint32_t *offs = (const uint32_t *)(table + 4);
		for (int32_t i = 0; i < count; i++)
		{
			F<uint8_t *>(s, 0x78) = (uint8_t *)(*(uint8_t *const *)(model + 4) + offs[i]);
			if (!(F<uint32_t>(s, 0x18) & (1u << (i & 31)))) continue;
			F<uint32_t>(s, 0x84) = F<uint32_t>(s, 4);
			uint8_t *obj = F<uint8_t *>(s, 0x78);
			int32_t groups = *(const int16_t *)obj;
			F<uint8_t *>(s, 0x78) = obj + 2;
			if (groups > 0)
			{
				do
				{
					obj = F<uint8_t *>(s, 0x78);
					const int32_t bone = *(const int16_t *)obj;
					F<uint8_t *>(s, 0x78) = obj + 2;
					const Mat4x3 *bm = (const Mat4x3 *)(bones + bone * 0x30 + 0x10);
					GteSetRotMatrixCtrl(bm);
					GteSetTransVectorCtrl(bm);
					BodyVertices(s);
				} while (--groups != 0);
			}
			obj = (uint8_t *)(((uintptr_t)F<uint8_t *>(s, 0x78) + 3) & ~(uintptr_t)3);
			F<uint16_t>(s, 8) = *(const uint16_t *)obj;
			F<uint16_t>(s, 0xA) = *(const uint16_t *)(obj + 2);
			F<uint8_t *>(s, 0x78) = obj + 0xC;
			F<uint8_t *>(s, 0) = obj + 0xC;
			cursor = BodyPolygons(s, cursor);
		}
		return cursor;
	}

	// sub_66E300: the entity's body (+0x64) into the frame arena, its vertices into buf; the world bone matrices of its pose
	// (0x5095B0) first, its bone matrices rebuilt from its pose (0x508C90) after
	static void DrawBody(uint8_t *entity, int32_t counter, int32_t ground, uint32_t buf)
	{
		uint8_t *s = (uint8_t *)FieldAlloc(0x88);
		F<int32_t>(s, 0x6C) = ground;
		F<int32_t>(s, 0x70) = counter;
		F<int32_t>(s, 0x74) = *(const int32_t *)(entity + 0x5C) - PG().body.mark_depth;
		F<uint32_t>(s, 4) = buf;
		ComputeBonesWorldMatrices(entity + 0x60, entity + 0x40);
		F<int16_t>(s, 0x10) = 0x140;
		F<int16_t>(s, 0xC) = 0;
		F<int16_t>(s, 0xE) = 0;
		F<int16_t>(s, 0x12) = 0xD8;
		F<uint32_t>(s, 0x14) = *(const uint32_t *)(entity + 0x28);
		F<uint32_t>(s, 0x18) = *(const uint32_t *)(entity + 0x7C);
		F<uint32_t>(s, 0x80) = 0;
		F<uint32_t>(s, 0x7C) = RenderOT44();
		FrameCursor() = BodyObjects(*(const uint8_t *const *)(entity + 0x64), s, FrameCursor());
		BuildBoneMatricesFromPose(entity + 0x60);
		FieldFree(0x88);
	}

	// ------------------------------------------------------------------
	// Ground marks (0x66DF20 -> 0x66DFB0 -> 0x66E090)
	// ------------------------------------------------------------------
	// sub_66E090: the frame's sprites (0x14-byte records: +0 colour word whose bytes 0 / 2 are also the half width / height,
	// +4 uv / clut, +8 / +0xA offset (x 16), +0xE tpage, +0x10 / +0x12 scale x / y) as quads around the work block's point
	// (+0x84 matrix, +0x98 translation), depth (OTZ >> shift) - 0x40; the shift is the `mode` argument, then the last depth
	// (see STACK WORDS)
	static uint32_t MarkSprites(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const int32_t count = F<int32_t>(h, 0x30);
		const uint8_t *r = F<uint8_t *>(h, 0x2C);
		int32_t shift = mode;
		if (count > 0)
		{
			uint32_t left = (uint32_t)count;
			do
			{
				F<uint16_t>(h, 0x72) = 0;
				F<uint16_t>(h, 0x70) = 0;
				F<uint16_t>(h, 0x6E) = 0;
				F<uint16_t>(h, 0x6A) = 0;
				F<uint16_t>(h, 0x68) = 0;
				F<uint16_t>(h, 0x66) = 0;
				F<uint16_t>(h, 0x74) = 0x1000;
				F<uint16_t>(h, 0x64) = *(const uint16_t *)(r + 0x10);
				F<uint16_t>(h, 0x6A) = 0;
				F<uint16_t>(h, 0x66) = 0;
				F<uint16_t>(h, 0x6C) = *(const uint16_t *)(r + 0x12);
				const int32_t tpage = *(const int16_t *)(r + 0xE);
				const uint32_t w = r[0], hh = r[2];
				const int32_t w8 = (int32_t)(w << 3), h8 = (int32_t)(hh << 3);
				F<int16_t>(h, 0x54) = (int16_t)-w8;
				F<int16_t>(h, 0x44) = (int16_t)-w8;
				F<int16_t>(h, 0x5C) = (int16_t)w8;
				F<int16_t>(h, 0x4C) = (int16_t)w8;
				F<int16_t>(h, 0x4E) = (int16_t)-h8;
				F<int16_t>(h, 0x46) = (int16_t)-h8;
				F<int16_t>(h, 0x5E) = (int16_t)h8;
				F<int16_t>(h, 0x56) = (int16_t)h8;
				F<int32_t>(h, 0x78) = shl32(*(const int16_t *)(r + 8), 4) + w8;
				F<int32_t>(h, 0x7C) = shl32(*(const int16_t *)(r + 0xA), 4) + h8;
				F<int32_t>(h, 0x80) = 0;
				GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x84));
				GteLoadIRColumn(h + 0x64);
				GteMVMVA_RotIR();
				*(uint32_t *)pk = 0x9000000;
				*(uint32_t *)(pk + 4) = *(const uint32_t *)r;
				GteStoreIRColumn(h + 0x64);
				GteLoadIRColumn(h + 0x66);
				GteMVMVA_RotIR();
				*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(r + 4);
				GteStoreIRColumn(h + 0x66);
				GteLoadIRColumn(h + 0x68);
				GteMVMVA_RotIR();
				pk[0x1C] = r[4];
				GteStoreIRColumn(h + 0x68);
				GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x84));
				GteLoadV0Dwords(h + 0x78);
				GteMVMVA_RotV0Tr();
				pk[0x15] = r[5];
				GteReadMAC123((int32_t *)(h + 0x78));
				GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x64));
				GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x64));
				GteLoadV012(h + 0x44, h + 0x4C, h + 0x54);
				GteRTPT();
				uint32_t u = r[4] + w;
				if ((int32_t)u >= 0x100) u = 0xFF;
				pk[0x24] = (uint8_t)u;
				pk[0x14] = (uint8_t)u;
				GteReadFLAG(h + 0x40);
				if (!(F<uint32_t>(h, 0x40) & 0x60000))
				{
					*(uint16_t *)(pk + 0x16) = (uint16_t)(tpage & 0x1FF);
					GteReadSXY012Split(pk + 8, pk + 0x10, pk + 0x18);
					GteLoadV0(h + 0x5C);
					GteRTPS();
					uint32_t v = r[5] + hh;
					if ((int32_t)v >= 0x100) v = 0xFF;
					pk[0x25] = (uint8_t)v;
					pk[0x1D] = (uint8_t)v;
					GteReadSXY2(pk + 0x20);
					GteAVSZ4();
					GteReadOTZWord(h + 0x3C);
					const int32_t z = (F<int32_t>(h, 0x3C) >> (shift & 31)) - 0x40;
					shift = z;
					if (z > 0)
					{
						InsertPrimAutoDepth(ot + (uint32_t)z * 4, pk);
						pk += 0x28;
					}
				}
				r += 0x14;
			} while (--left != 0);
		}
		return (uint32_t)pk;
	}

	// sub_66DFB0 (draw): the mark's point (x, 0, z) through the camera, flipbook frame 12 - life
	static void MarkSprite(uint8_t *h, const Mark *e)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		GteWriteDataReg((uint16_t)e->x, 0);
		GteWriteDataReg((uint16_t)e->z, 1);
		GteMVMVA_RotV0Tr();
		GteReadMAC123((int32_t *)(h + 0x98));
		const uint32_t seq = fxp::sequence(PG().seqs.mark, SEQ_Mark);
		*(uint32_t *)h = seq;
		F<uint16_t>(h, 0x94) = 0x1000;
		F<uint16_t>(h, 0x8C) = 0x1000;
		F<uint16_t>(h, 0x84) = 0x1000;
		const uint32_t f = (*(const uint32_t *)(seq + 8) & 0xFFFF) - (uint32_t)(int32_t)e->life;
		const uint32_t frame = *(const uint16_t *)(seq + 8 + f * 2) + seq;
		F<uint32_t>(h, 0x2C) = frame;
		F<uint32_t>(h, 0x30) = *(const uint32_t *)frame;
		F<uint32_t>(h, 0x2C) = frame + 4;
		PacketCursor() = MarkSprites(h, RenderOT44(), 2, PacketCursor());
	}

	// sub_66DFB0: draw, then one tick less (a freed mark clears its polygon's entry)
	static void MarkDraw(uint8_t *h, Mark *e)
	{
		MarkSprite(h, e);
		e->life--;
		if (e->life == 0) ((uint8_t *)TexBase())[0x4AC + e->index] = 0xFF;
	}

	// sub_66DF20: every live mark (work block 0xB4: the identity rotation and the sprite vertices' z words zeroed)
	static void DrawMarks(MasterNode *m)
	{
		// 30 fps layer: see mag301_gargantua_burrow_held.inc
		FX_HELD(held_note_marks(m);)
		FX_PART(PART_MARKS);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint16_t>(h, 0x92) = 0;
		F<uint16_t>(h, 0x90) = 0;
		F<uint16_t>(h, 0x8E) = 0;
		F<uint16_t>(h, 0x8A) = 0;
		F<uint16_t>(h, 0x88) = 0;
		F<uint16_t>(h, 0x86) = 0;
		F<uint16_t>(h, 0x60) = 0;
		F<uint16_t>(h, 0x58) = 0;
		F<uint16_t>(h, 0x50) = 0;
		F<uint16_t>(h, 0x48) = 0;
		Mark *e = m->marks;
		for (int32_t n = 0x80; n != 0; n--, e++)
			if (e->life != 0) MarkDraw(h, e);
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// Rock (0x66EC50)
	// ------------------------------------------------------------------
	static void RockDraw(const int16_t pos[3], int16_t ax, int16_t ay)
	{
		uint8_t *h = AllocHeader(0x98);
		RotY(ay, (Mat4x3 *)(h + 0x78));
		RotX(ax, (Mat4x3 *)(h + 0x58));
		MatrixMultiply3x3((Mat4x3 *)(h + 0x58), (const Mat4x3 *)(h + 0x78));
		Mat4x3 *m = (Mat4x3 *)(h + 0x58);
		m->t[0] = pos[0];
		m->t[1] = pos[1];
		m->t[2] = pos[2];
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		*(uint32_t *)h = fxp::model(PG().models.rock, MODEL_Rock);
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x98);
	}

	static uint32_t __cdecl RockTask(TaskNode *n)
	{
		RockNode *r = (RockNode *)n;
		// 30 fps layer: see mag301_gargantua_burrow_held.inc
		FX_HELD(held_note_rock(r);)
		FX_PART(PART_ROCKS);
		RockDraw(r->pos, (int16_t)(r->angle[0] << 4), (int16_t)(r->angle[1] << 4));
		const BurrowParams &g = PG();
		r->angle[0] = (int8_t)(r->angle[0] + r->spin[0]);
		r->angle[1] = (int8_t)(r->angle[1] + r->spin[1]);
		r->pos[0] = (int16_t)(r->pos[0] + (int16_t)(r->vel[0] * g.rocks.speed_xz));
		r->pos[2] = (int16_t)(r->pos[2] + (int16_t)(r->vel[2] * g.rocks.speed_xz));
		r->pos[1] = (int16_t)(r->pos[1] + (int16_t)(r->vel[1] * g.rocks.speed_y));
		if (r->pos[1] >= 0) r->vel[1] = (int8_t)(-(int32_t)r->vel[1] >> 1);
		r->vel[1] = (int8_t)(r->vel[1] + r->gravity);
		r->counter++;
		return r->counter < g.rocks.life ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Dust puff (0x66EE60)
	// ------------------------------------------------------------------
	static void DustDraw(const int16_t *pos, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, PG().dust.size, -0x400);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = fxp::sequence(PG().seqs.dust, SEQ_Dust);
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl DustTask(TaskNode *n)
	{
		DustNode *d = (DustNode *)n;
		// 30 fps layer: see mag301_gargantua_burrow_held.inc
		FX_HELD(held_note_dust(d);)
		FX_PART(PART_DUST);
		DustDraw(d->pos, d->counter);
		d->pos[0] = (int16_t)(d->pos[0] + d->vel[0]);
		d->pos[1] = (int16_t)(d->pos[1] + d->vel[1]);
		d->pos[2] = (int16_t)(d->pos[2] + d->vel[2]);
		d->counter++;
		return d->counter < PG().dust.life ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Spawns from the stream (0x66EB10 rocks, 0x66ED70 dust): a count word, then per node its offset from the caster's
	// position words (+0x54 / +0x58 / +0x5C) and its bytes, one per word
	// ------------------------------------------------------------------
	// the stream where its pointer is: the game's (the pointer, exe data, is never put back), or the stream parameter read at
	// the pointer's offset (nullptr when `need` bytes from there pass its end: a copy's second cast spawns nothing)
	static const uint8_t *StreamAt(uint32_t need)
	{
		const uint32_t base = fxp::data(PG().tables.stream, STREAM_Data, STREAM_SIZE);
		if (base == STREAM_Data) return Stream();
		const uint32_t at = (uint32_t)Stream() - STREAM_Data;
		return at <= STREAM_SIZE && need <= STREAM_SIZE - at ? (const uint8_t *)(base + at) : nullptr;
	}

	static void SpawnRocks()
	{
		const uint8_t *g = StreamAt(2);
		if (!g) return;
		const int32_t count = *(const int16_t *)g;
		if (count < 0 || !(g = StreamAt(2 + 20 * (uint32_t)count))) return;
		Stream() = Stream() + 2 + 20 * count;
		g += 2;
		for (int32_t k = 0; k < count; k++, g += 20)
		{
			RockNode *r = (RockNode *)AddTaskToQueue(QRocks(), own::Fn(ORIG_RockTask));
			if (!r) continue;
			r->pos[0] = (int16_t)(*(const int16_t *)(Caster() + 0x54) + *(const int16_t *)g);
			r->pos[1] = (int16_t)(*(const int16_t *)(Caster() + 0x58) + *(const int16_t *)(g + 2));
			r->pos[2] = (int16_t)(*(const int16_t *)(Caster() + 0x5C) + *(const int16_t *)(g + 4));
			r->counter = 0;
			r->vel[0] = *(const int8_t *)(g + 6);
			r->vel[1] = *(const int8_t *)(g + 8);
			r->vel[2] = *(const int8_t *)(g + 10);
			r->gravity = (int8_t)PG().rocks.gravity;
			r->angle[0] = *(const int8_t *)(g + 12);
			r->angle[1] = *(const int8_t *)(g + 14);
			r->spin[0] = *(const int8_t *)(g + 16);
			r->spin[1] = *(const int8_t *)(g + 18);
		}
	}

	static void SpawnDust()
	{
		const uint8_t *g = StreamAt(2);
		if (!g) return;
		const int32_t count = *(const int16_t *)g;
		if (count < 0 || !(g = StreamAt(2 + 12 * (uint32_t)count))) return;
		Stream() = Stream() + 2 + 12 * count;
		g += 2;
		for (int32_t k = 0; k < count; k++, g += 12)
		{
			DustNode *d = (DustNode *)AddTaskToQueue(QDust(), own::Fn(ORIG_DustTask));
			if (!d) continue;
			d->pos[0] = (int16_t)(*(const int16_t *)(Caster() + 0x54) + *(const int16_t *)g);
			d->pos[1] = (int16_t)(*(const int16_t *)(Caster() + 0x58) + *(const int16_t *)(g + 2));
			d->pos[2] = (int16_t)(*(const int16_t *)(Caster() + 0x5C) + *(const int16_t *)(g + 4));
			d->counter = 0;
			d->vel[0] = *(const int8_t *)(g + 6);
			d->vel[1] = *(const int8_t *)(g + 8);
			d->vel[2] = *(const int8_t *)(g + 10);
			d->pad17 = 2;
		}
	}

	// ------------------------------------------------------------------
	// Master (0x66DBD0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		m->counter++;
		const BurrowParams &g = PG();
		if (m->counter >= g.timing.end)
		{
			*(uint16_t *)Caster() &= 0xFFF3;
			return TASK_END;
		}
		*(uint16_t *)Caster() &= 0xFFF7;
		const int32_t c = m->counter;
		if ((uint32_t)c < (uint32_t)g.layout.ticks)
		{
			PrimArg arg;
			memcpy(arg.pos, m->pos, sizeof(arg.pos));
			arg.morph = TexBase() + VERTS;
			arg.depth = g.layout.depth;
			// 30 fps layer: see mag301_gargantua_burrow_held.inc
			FX_HELD(held_note_play(m, &arg);)
			FX_PART(PART_LAYOUT);
			prim::play((prim::Layout *)m->layout, PrimCallback, (int)&arg, 0);
			*(uint8_t *)Caster() |= 4;
			// 30 fps layer: see mag301_gargantua_burrow_held.inc
			FX_HELD(held_note_body(m);)
			FX_PART(PART_BODY);
			DrawBody(Caster(), c, g.body.ground, TexBase() + VERTS);
			if ((uint32_t)c < (uint32_t)g.spawn.ticks)
			{
				SpawnRocks();
				SpawnDust();
			}
		}
		else *(uint16_t *)Caster() &= 0xFFF3;
		const int32_t d = m->counter - g.fade.start;
		if ((uint32_t)d <= (uint32_t)g.fade.ticks)
		{
			GteSetIR0(0x1000 - ComputeSin(shl32(g.fade.ticks - d, 10) / g.fade.ticks));
			GteLoadIRBytes(&m->colour);
			GteGPF();
			GteStoreIRBytes(Caster() + 0x2C);
			Caster()[0x2F] = (uint8_t)g.fade.alpha;
			if (d == g.fade.ticks) *(uint32_t *)(Caster() + 0x2C) = m->colour;
		}
		DrawMarks(m);
		if (m->counter == g.timing.start)
		{
			BdPlaySE((const void *)fxp::sound(g.sounds.start, (uint32_t)SOUND_Start), 0, 0x80);
			return 0;
		}
		if (m->counter == g.timing.release) ReleaseVoiceSlot(m->voice);
		return 0;
	}

	// ------------------------------------------------------------------
	// Root (0x66D9F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *rn = (RootNode *)n;
		// 30 fps layer: see mag301_gargantua_burrow_held.inc
		FX_HELD(held_note_root();)
		if (rn->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			rn->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			rn->arena = 1;
		}
		*(uint8_t *)Caster() |= 0xC;
		if (rn->counter == 1 && !rn->started)
		{
			rn->started = 1;
			InitTaskQueuePool(QMaster(), (void *)TexBase(), 0xE14, 1);
			InitTaskQueuePool(QRocks(), (void *)(TexBase() + POOL_Rocks), 0x1C, 0x20);
			InitTaskQueuePool(QDust(), (void *)(TexBase() + POOL_Dust), 0x18, 0x20);
			MasterNode *m = (MasterNode *)AddTaskToQueue(QMaster(), own::Fn(ORIG_MasterTask));
			Memset32(&m->counter, 0, 0x382);
			Memset32(m->index, 0xFFFFFFFF, 0x258);
			m->voice = ClaimVoiceSlot((const void *)fxp::sound(PG().sounds.voice, (uint32_t)SOUND_Voice), 1, 0x80);
			DecodeModelPrimLayout(fxp::layout(PG().layouts.main, MODEL_Layout, 0x90), m->layout, 0x90);
			GetDefaultEffectPosition(Caster(), m->pos);
			m->pos[1] = 0;
			m->colour = *(const uint32_t *)(Caster() + 0x2C);
			m->next = m->marks;
		}
		int a0;
		if (rn->started)
		{
			a0 = ExecuteTaskQueue(QMaster());
			ExecuteTaskQueue(QRocks());
			ExecuteTaskQueue(QDust());
		}
		else a0 = (int)n;
		if (rn->started && a0 == 0)
		{
			for (int i = 0; i < 3; i++)
			{
				uint16_t &w = *(uint16_t *)Entity(i);
				w ^= (uint16_t)((PartyWord(i) ^ w) & 4);
			}
			*(uint16_t *)Caster() &= 0xFFF3;
			return TASK_END;
		}
		rn->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// File loader (0x66D8F0) and setup (0x66D8E0 -> _Init 0x66D910): the game runs its own; Fujin's programs these (its
	// memory may be a copy's): the TIM, the magic buffer, the party entities' words +0 kept (their bit 2 set), the context, the
	// caster (its word +0 kept, bits 2 / 3 set), the root, the camera
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(PG().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }    // BS_Camera_ArmReturnAfterEffect

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(TexFile());
		TexBase() = (uint32_t)layouts::MagicBuffer();
		var<uint32_t>(own::R(0x2507744)) = *(const uint16_t *)Entity(0);
		const uint32_t w2 = *(const uint16_t *)Entity(2), w1 = *(const uint16_t *)Entity(1);
		*(uint16_t *)Entity(0) |= 4;
		*(uint16_t *)Entity(1) |= 4;
		*(uint16_t *)Entity(2) |= 4;
		var<uint32_t>(own::R(0x2507748)) = w1;
		var<uint32_t>(own::R(0x250774C)) = w2;
		Ctx() = ctx;
		Caster() = Entity(ctx->attacker);
		var<uint32_t>(own::R(0x2507740)) = *(const uint16_t *)Caster();
		*Caster() |= 0xC;
		TaskQueue *root = (TaskQueue *)own::R(0x2507718);
		InitTaskQueuePool(root, (void *)own::R(0x2507728), 0x14, 1);
		Memset32((uint8_t *)AddTaskToQueue(root, own::Fn(ORIG_RootTask)) + 0xC, 0, 2);
		CameraArmReturn();
		PlayCameraAnimation((const void *)fxp::camera(PG().res.camera, CAMERA_ANIM));
		return root;
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "master", "Master", "The master's ticks, the start sound and the voice (claimed at the start, released near the end), the caster's colour faded back in at the end; the texture and the camera" },
			{ PART_LAYOUT, "layout", "Layout", "The prim-model layout at the caster's effect position (on the ground)" },
			{ PART_BODY, "body", "Body", "The caster drawn by the module: polygons all below the ground line left out, every n-th one crossing it leaving a ground mark" },
			{ PART_MARKS, "marks", "Ground marks", "A flipbook on the ground where the body crosses it" },
			{ PART_ROCKS, "rocks", "Rocks", "Rocks from the spawn stream: a mesh at the caster, thrown, spinning, falling and bouncing" },
			{ PART_DUST, "dust", "Dust", "Dust puffs from the spawn stream: a flipbook drifting from the caster" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing", "Ticks", "The master's ticks" },
			{ "fade", "Colour fade", "The caster's colour faded back in" },
		};
#define MPF(member) (uint32_t)offsetof(BurrowParams, member)
		static const Field FIELDS[] = {
			{ "master", "sounds.start", "Sound", "At the start sound's tick", MPF(sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "sounds.voice", "Voice", "The sound block its voice slot is claimed with (at the start)", MPF(sounds.voice), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "timing.start", "Sound", "The master's tick of the start sound", MPF(timing.start), fxp::I16, 0, 400 },
			{ "master", "timing.release", "Voice released", "Its tick the voice slot is released", MPF(timing.release), fxp::I16, 0, 400 },
			{ "master", "timing.end", "End", "Its last tick (the effect ends with it)", MPF(timing.end), fxp::I16, 1, 400 },
			{ "master", "fade.start", "Fade start", "The master's tick the caster's colour starts fading back in (its own draw is back from there)", MPF(fade.start), fxp::I16, 0, 400 },
			{ "master", "fade.ticks", "Fade ticks", "Its ticks (at least 1): the colour = its kept colour x (1 - sin of the quarter turn left)", MPF(fade.ticks), fxp::I16, 1, 400 },
			{ "master", "fade.alpha", "Fade byte", "His colour's 4th byte set meanwhile", MPF(fade.alpha), fxp::I16, 0, 255 },
			{ "master", "spawn.ticks", "Spawn ticks", "The spawn stream's rocks and puffs on the master's ticks below this (the stream holds 101 ticks)", MPF(spawn.ticks), fxp::I16, 0, 102 },
			{ "master", "tables.stream", "Spawn stream", "The rocks and puffs of each tick (0x12B8 bytes: a count, then 10 words a rock - offset x / y / z, velocity x / y / z, angle x / y, spin x / y; a count, then 6 words a puff - offset, velocity; edit their numbers, keep their shape)", MPF(tables.stream), fxp::Data, 0, 0x7FFFFFFF },
			{ "master", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", MPF(res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "master", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", MPF(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "layout", "layout.ticks", "Ticks", "The layout and the body drawn on the master's ticks below this (then the caster's own draw)", MPF(layout.ticks), fxp::I16, 0, 400 },
			{ "layout", "layout.depth", "Depth", "The layout's depth offset", MPF(layout.depth), fxp::I16, -0x1000, 0x1000 },
			{ "layout", "layouts.main", "Layout", "The layout", MPF(layouts.main), fxp::Layout, 0, 0x7FFFFFFF },
			{ "body", "body.ground", "Ground line", "Vertices below this height are under the ground (a polygon all under it is left out)", MPF(body.ground), fxp::I16, -0x1000, 0x1000 },
			{ "body", "body.mark_depth", "Mark depth", "A crossing polygon leaves a mark when its first vertex's z is beyond the caster's - this", MPF(body.mark_depth), fxp::I16, -0x4000, 0x4000 },
			{ "body", "body.mark_every", "Mark every", "Every n-th crossing polygon (by its index and the tick) leaves a mark", MPF(body.mark_every), fxp::I16, 1, 64 },
			{ "marks", "marks.life", "Life", "A mark's ticks (its flipbook's frame count - its life left: 12 at most)", MPF(marks.life), fxp::I16, 1, 12 },
			{ "marks", "seqs.mark", "Flipbook", "The marks' flipbook", MPF(seqs.mark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "rocks", "rocks.life", "Life", "A rock's ticks", MPF(rocks.life), fxp::I16, 1, 400 },
			{ "rocks", "rocks.gravity", "Gravity", "Its y velocity byte grows by this a tick (it bounces at half its speed on the ground)", MPF(rocks.gravity), fxp::I16, -64, 64 },
			{ "rocks", "rocks.speed_xz", "Speed x / z", "It moves by its x / z velocity bytes x this a tick", MPF(rocks.speed_xz), fxp::I16, -128, 128 },
			{ "rocks", "rocks.speed_y", "Speed y", "It moves by its y velocity byte x this a tick", MPF(rocks.speed_y), fxp::I16, -128, 128 },
			{ "rocks", "models.rock", "Mesh", "The rocks' mesh", MPF(models.rock), fxp::Model, 0, 0x7FFFFFFF },
			{ "dust", "dust.life", "Life", "A puff's ticks (its flipbook's frame = its tick: 16 at most)", MPF(dust.life), fxp::I16, 1, 16 },
			{ "dust", "dust.size", "Size", "Its size", MPF(dust.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "dust", "seqs.dust", "Flipbook", "The puffs' flipbook", MPF(seqs.dust), fxp::Seq, 0, 0x7FFFFFFF },
		};
#undef MPF
	}
}

	void register_mag301_gargantua_burrow()
	{
		fxp::register_params({ 301, "Gargantua burrow", &burrow301::live, &burrow301::GAME, sizeof(burrow301::BurrowParams),
		                       burrow301::table::PARTS, (int)(sizeof(burrow301::table::PARTS) / sizeof(burrow301::table::PARTS[0])),
		                       burrow301::table::FIELDS, (int)(sizeof(burrow301::table::FIELDS) / sizeof(burrow301::table::FIELDS[0])),
		                       burrow301::table::GROUPS, (int)(sizeof(burrow301::table::GROUPS) / sizeof(burrow301::table::GROUPS[0])) });
		own::note(burrow301::ORIG_FileLoader, (void *)burrow301::FileLoader);
		own::note(burrow301::ORIG_Setup, (void *)burrow301::Setup);
		own::note(burrow301::ORIG_RootTask, (void *)burrow301::RootTask);
		register_port(burrow301::ORIG_RootTask, (void *)burrow301::RootTask, "301 RootTask", 301);
		own::note(burrow301::ORIG_MasterTask, (void *)burrow301::MasterTask);
		register_port(burrow301::ORIG_MasterTask, (void *)burrow301::MasterTask, "301 MasterTask", 301);
		own::note(burrow301::ORIG_RockTask, (void *)burrow301::RockTask);
		register_port(burrow301::ORIG_RockTask, (void *)burrow301::RockTask, "301 RockTask", 301);
		own::note(burrow301::ORIG_DustTask, (void *)burrow301::DustTask);
		register_port(burrow301::ORIG_DustTask, (void *)burrow301::DustTask, "301 DustTask", 301);
		// 30 fps layer: see mag301_gargantua_burrow_held.inc
		FX_HELD(register_mag301_held();)
	}
	FX_LAB_MODULE(register_mag301_gargantua_burrow);
}

#ifdef FF8_FX_HELD
#include "mag301_gargantua_burrow_held.inc"
#endif
