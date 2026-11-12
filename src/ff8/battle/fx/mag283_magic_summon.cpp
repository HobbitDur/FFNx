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

// Effect 283: Magic Summon (enemy attack 261 of kernel.bin; Sphinxara c0m012; MAG_283_*).
//
// Structure (setup 0x67C400 -> _Init 0x67C430, file loader 0x67C410 = mag282.tim (name at 0x109EAAC) into 0x2507E20; _Init
// queues the TIM, keeps the context (0x2507E24) and the CASTER's entity (0x2507E28, never read again), sets up the master queue
// 0x2507E40 (pool 0x2507E50, 1 x 0x14) and starts camera animation 0x1094FC0):
//   MasterTask (0x67C4B0) - alternates the packet arena (magic buffer + 0x650 / + 0x10650); on counter 1 (once) sets up the
//     main pool (1 x 0x648 at the magic buffer) and starts the main task: target = action 0's first target (its entity word +0
//     gets bit 2), layouts 0x1095178 (A) / 0x1098A18 (B), its default effect position, the anchor of A (the position's x / z at
//     the target's height +0x24), its colour +0x2C and its size +0x26. Then the main queue; when it is empty: screen flash 0
//     and the effect ends.
//   MainTask (0x67C610) - screen flash: level counter x 275 until 8, (60 - counter) x 275 from 52, else 0x898 in the colour
//     blended along the 8-entry table 0x109968C (counter x 7 / 60; black at both ends). Counters 0..59: layout A at its anchor
//     (block scale = the target's size, depth -0x200, texture scroll (counter & 15) x 8 of object 1). Counters 24..43 (e =
//     counter - 24): the target's colour +0x2C = its kept colour faded by 0x1000 - sin((20 - e) x 0x400 / 20) (byte +0x2F =
//     0x32); while the position projects in front of the camera, the target is redrawn warped (0x67CC10, weight row 20 - e,
//     radius = the previous draw's largest offset). Counter 44: the target's bit 2 cleared and its colour put back. Counters
//     24..59: layout B at the position. Sound 0x1099688 at counter 10; ends after counter 59.
//   PrimCallback (0x67C8A0) - one layout object drawn by 0x701DD0 (header 0x6C): offset and object scaled by the block's scale;
//     flag 0x1000: the block's point through the camera + the offset through the matrix 0x2507E70 (composed with it unless flag
//     0x8000), else point + offset through the camera (composed with it unless flag 0x8000); + the camera's translation,
//     scale3DMatrix, fade, the texture scroll of object 1 (the block's +0x1A), depth = the block's +0x18.
//   Warp (0x67CC10 -> 0x67CD80 -> 0x67D250 / 0x67CEA0): the target's shadow, then its body (+0x64) and weapon (+0x78) drawn
//     with their own textures (each triangle / quad followed by a subtractive flat copy in the faded colour and a draw-mode
//     packet, same bucket), every vertex pushed away from the projected position by the weight row's factor of its distance
//     (u8 [128][128] 0x109AAAC) to the radius; into the frame arena (0x1D8E054) with the bone workspace (0x1D98B3C).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update).
// ENTITY WRITES: the target's word +0 bit 2 (set at the start, cleared at counter 44), its colour +0x2C / +0x2F (counters
//   24..44), its bone matrices (world matrices 0x5095B0 / from its pose 0x508C90 by the warp). No position word is written.
// CAMERA: the setup's camera animation 0x1094FC0 (engine); no camera word is written by the tasks.
// Module globals: 0x2507E20..0x2507E90 (texture file 0x2507E20, context 0x2507E24, caster 0x2507E28, the queues 0x2507E30 main
// / 0x2507E40 master, master pool 0x2507E50, packet cursor 0x2507E64, magic buffer base 0x2507E68, the matrix 0x2507E70 the
// callback's flag 0x1000 reads (never written by the module)).
// STACK WORDS: the flash colour is blended (3 bytes, 0x56CC00) over the main task's own argument slot: its 4th byte is the
// node pointer's high byte. The 4th words of the vectors loaded into GTE V0 (the callback's record offset) reach the GTE's
// VZ0 high half (0x1CA8A16) only. The warp keeps its counters / bucket in its argument slots (not re-read by the callers).
// SCREEN UNITS (PC): the warp's centre (0x67C7B8) and its vertices (0x67D297) are the GTE's SXY in eighths of a pixel, used as
// pixels (distance clamp 0x80, the distance table, the radius): the warp reaches ~16 px (the clip window is already x 8).

// Fujin's described Magic Summon (effect_283.json) runs this code: its memory through own::R() (fx_port.h), its spawns
// through own::Fn(), the setup and the file loader ported below; its parameters (SummonParams: the sound, the timeline,
// the screen flash and its colours, the layouts' ticks, depth and scroll, the warp's start, the two layouts, the texture
// name and the camera track). FX_PART tags its parts in the lab.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace magicsummon283
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x2507E24)); }
	inline TaskQueue *QMain() { return (TaskQueue *)own::R(0x2507E30); }           // pool: magic buffer, 1 x 0x648
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x2507E64)); }
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2507E68)); }        // Magic_TextureOFF (magic buffer)
	inline const Mat4x3 *MatrixFlag1000() { return (const Mat4x3 *)own::R(0x2507E70); } // the callback's flag 0x1000 frame (never written)
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x2507E20)); }          // the effect's file (its TIM)
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x2507E28)); }          // caster entity (setup; never read again)
	inline TaskQueue *QMaster() { return (TaskQueue *)own::R(0x2507E40); }           // 1 x 0x14 (pool 0x2507E50)
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }    // battle_texture_data_ptr (frame arena)
	inline uint32_t &BoneWorkspace() { return var<uint32_t>(0x1D98B3C); }  // warp vertex buffer
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_MasterTask = 0x67C4B0;
	static const uint32_t ORIG_MainTask = 0x67C610;
	static const uint32_t ORIG_PrimCallback = 0x67C8A0;   // not a task: the layouts' callback
	static const uint32_t MORPH = 0x648;                 // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x650, ARENA_B = 0x10650; // magic buffer: packet arenas
	static const uint32_t MODEL_A = 0x1095178;           // prim-model layouts
	static const uint32_t MODEL_B = 0x1098A18;
	static const uint32_t FLASH_Colours = 0x109968C;     // 8 + 1 colours (3 bytes used)
	static const uint32_t WARP_Weights = 0x10996AC;      // u16 [20][128]: the offset's weight by distance (4.12), per step
	static const uint32_t WARP_Distance = 0x109AAAC;     // u8 [128][128]: the distance of (|dy|, |dx|) (row 20 of the weights)
	static const void *const SOUND_Summon = (const void *)0x1099688;
	static const uint32_t ORIG_FileLoader = 0x67C410, ORIG_Setup = 0x67C400;
	static const uint32_t TEXTURE_NAME = 0x109EAAC, CAMERA_ANIM = 0x1094FC0;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_CIRCLE = 1, PART_WARP = 2, PART_SUMMON = 3 };
	struct SummonParams
	{
		uint32_t sound;                                  // at the main task's sound tick
		struct { int16_t sound, end; } timing;           // the main task's ticks (layout A plays until the end)
		// the screen flash: up over rise ticks and down over the last fall ticks by step a tick, black; between: level in the
		// colours blended along the 8 colours over the main task
		struct { int16_t level, rise, fall, step; } flash;
		struct { int16_t depth, scroll; } circle;        // layout A at the target's feet: its depth, its object 1's texture scroll a tick
		struct { int16_t tick, ticks, depth; } summon;   // from tick: the target warped away (20 ticks), layout B for ticks
		struct { uint32_t a, b; } layouts;               // the prim-model layouts played (their models' animation)
		struct { uint32_t flash_colours; } tables;       // the screen flash's colours (r, g, b, -)
		struct { uint32_t texture_name, camera; } res;   // its resources (data): texture file name, camera track
	};
	static const SummonParams GAME = { 0x1099688, { 0xA, 0x3C }, { 0x898, 8, 8, 275 }, { -0x200, 8 }, { 0x18, 0x24, -0x200 },
		{ MODEL_A, MODEL_B }, { FLASH_Colours }, { TEXTURE_NAME, CAMERA_ANIM } };
	static SummonParams live = GAME;
	static const SummonParams &PM() { return live; }

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b.rot = a.rot x b.rot
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); } // MAG_063_RenderPrimSet
	// ColourLerp 0x56CC00: out (3 bytes) = a x wa + b x wb (4.12 weights, per channel)
	inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); }
	inline void GteLoadIRBytes(const void *src) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(src); }   // IR1..3 = 3 x u8
	inline void GteStoreIRBytes(void *dst) { fn<void (__cdecl *)(void *)>(0x45E410)(dst); }        // IR1..3 low bytes
	// sub_5088A0: an entity's ground shadow (returns the packet cursor)
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	// software GTE
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // MAG_069_sub_703FB0: V0 = 3 x u16
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteSetSZ123(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45E1D0)(a, b, c, d); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }    // GTE_ReadOTZ (dword)

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2507E50)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x648 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..59
		int16_t radius;    // +0x0E warp: the largest offset of the last draw (at most 0x80)
		int32_t size;      // +0x10 the target's size (entity +0x26): the blocks' scale
		uint32_t colour;   // +0x14 the target's colour (entity +0x2C) as it was
		int16_t anchor[4]; // +0x18 layout A: the position's x, the target's height (+0x24), the position's z, 0
		int16_t pos[4];    // +0x20 the target's default effect position
		uint8_t *ent;      // +0x28 target entity (action 0's first target)
		uint8_t layoutA[0x4BC]; // +0x2C
		uint8_t layoutB[0x160]; // +0x4E8
	};
	// the prim-player callback's parameter block (a stack block of the main task)
	struct PrimArg
	{
		int16_t anchor[4]; // +0x00 point (4th word: GTE V0 pad only)
		int32_t scale[3];  // +0x08 the record offsets and the object scaled by it (4.12)
		int32_t scaled;    // +0x14 (always 1)
		int16_t depth;     // +0x18 the header's +0x18
		int16_t scroll;    // +0x1A texture scroll of object 1 (0: none)
		uint32_t morph;    // +0x1C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x648, "Magic Summon nodes");
	static_assert(sizeof(PrimArg) == 0x20, "Magic Summon prim block");

	template<typename T> static inline T &F(uint8_t *s, uint32_t off) { return *(T *)(s + off); }

	// ------------------------------------------------------------------
	// Warp (0x67CC10 -> 0x67CD80 -> 0x67D250 / 0x67CEA0): the target model, its vertices pushed away from a point on screen
	// ------------------------------------------------------------------
	// Work block (Field_Alloc 0x7C): +0x00 camera x entity matrix, then S = +0x20:
	//   S+0x00 polygon cursor, +0x04 vertex buffer, +0x08 triangle / quad counts, +0x0C clip rectangle (x0, y0, x1, y1: x 8
	//   in the test), +0x14 colour (code byte 2 when faded), +0x18 object mask, +0x1C..+0x3B four vertices (sx, sy, otz, clip
	//   flags), +0x3C NCLIP, +0x44 OTZ, +0x4C weight row, +0x50 radius, +0x52 / +0x54 the point on screen (SXY), +0x56 largest
	//   offset, +0x58 the fade.

	// sub_67D250: one bone group of vertices, projected, pushed, clip-flagged
	static void WarpVertices(uint8_t **objp, uint8_t **outp, uint8_t *s)
	{
		uint8_t *p = *objp;
		uint8_t *out = *outp;
		const int32_t n = *(const int16_t *)p;
		p += 2;
		if (n == 0)
		{
			*objp = p;
			*outp = out;
			return;
		}
		uint32_t left = (uint32_t)n;
		do
		{
			GteLoadV0u(p);
			GteRTPS();
			p += 6;
			GteReadSXY2(s + 0x1C);
			GteReadOTZ(s + 0x20);
			if (F<uint16_t>(s, 0x20) == 0)
				*(uint16_t *)(out + 6) = 0x10;
			else
			{
				const int16_t cx = F<int16_t>(s, 0x52);
				const int16_t cy = F<int16_t>(s, 0x54);
				const int32_t dx = (int16_t)(F<int16_t>(s, 0x1C) - cx);
				const int32_t dy = (int16_t)(F<int16_t>(s, 0x1E) - cy);
				int32_t adx = dx;
				if ((int16_t)dx < 0) adx = -adx;
				if ((int16_t)adx >= 0x80) adx = 0x80;
				int32_t ady = dy;
				if ((int16_t)dy < 0) ady = -ady;
				if ((int16_t)ady >= 0x80) ady = 0x80;
				if (F<int16_t>(s, 0x56) < (int16_t)adx) F<int16_t>(s, 0x56) = (int16_t)adx;
				if (F<int16_t>(s, 0x56) < (int16_t)ady) F<int16_t>(s, 0x56) = (int16_t)ady;
				const uint32_t d = *(const uint8_t *)(WARP_Distance + (int32_t)(int16_t)adx + shl32((int16_t)ady, 7));
				if (d != 0)
				{
					const int32_t radius = F<int16_t>(s, 0x50);
					if ((int32_t)d < radius)
					{
						const int32_t q = (int32_t)(d << 7) / radius;
						const uint32_t w = *(const uint16_t *)(F<uint32_t>(s, 0x4C) + q * 2);
						F<int16_t>(s, 0x1C) = (int16_t)((mul32(dx, (int32_t)w) >> 12) + cx);
						F<int16_t>(s, 0x1E) = (int16_t)((mul32(dy, (int32_t)w) >> 12) + cy);
					}
				}
				const int32_t vx = F<int16_t>(s, 0x1C);
				if (vx < shl32(F<int16_t>(s, 0xC), 3)) s[0x22] |= 1;
				else if (vx >= shl32(F<int16_t>(s, 0x10), 3)) s[0x22] |= 2;
				const int32_t vy = F<int16_t>(s, 0x1E);
				if (vy < shl32(F<int16_t>(s, 0xE), 3)) s[0x22] |= 4;
				else if (vy >= shl32(F<int16_t>(s, 0x12), 3)) s[0x22] |= 8;
				*(uint32_t *)out = F<uint32_t>(s, 0x1C);
				*(uint32_t *)(out + 4) = F<uint32_t>(s, 0x20);
			}
			out += 8;
		} while (--left != 0);
		*objp = p;
		*outp = out;
	}

	static inline void CopyVertex(uint8_t *s, uint32_t off, const uint8_t *verts, uint16_t index)
	{
		const uint8_t *v = verts + (uint32_t)(index & 0xFFF) * 8;
		F<uint32_t>(s, off) = *(const uint32_t *)v;
		F<uint32_t>(s, off + 4) = *(const uint32_t *)(v + 4);
	}

	// sub_67CEA0: the object's textured triangles and quads (each polygon's own semi-transparency bit kept, the code byte of the
	// colour or'ed in), each followed by a flat subtractive copy in the colour and a draw-mode packet, in the same bucket
	static uint32_t WarpPolygons(uint8_t *s, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *verts = F<uint8_t *>(s, 4);
		const uint8_t *p = F<uint8_t *>(s, 0);
		uint16_t k = F<uint16_t>(s, 8);
		F<uint16_t>(s, 8) = (uint16_t)(k - 1);
		if (k != 0)
		{
			do
			{
				CopyVertex(s, 0x1C, verts, *(const uint16_t *)p);
				CopyVertex(s, 0x24, verts, *(const uint16_t *)(p + 2));
				CopyVertex(s, 0x2C, verts, *(const uint16_t *)(p + 4));
				const uint8_t a = s[0x22], b = s[0x2A], c = s[0x32];
				if ((uint8_t)(a | b | c) < 0x10 && !(c & (a & b)))
				{
					GteSetSXY012(F<uint32_t>(s, 0x1C), F<uint32_t>(s, 0x24), F<uint32_t>(s, 0x2C));
					GteNCLIP();
					GteReadMAC0(s + 0x3C);
					if (F<int32_t>(s, 0x3C) >= 0)
					{
						GteSetSZ123(F<uint16_t>(s, 0x20), F<uint16_t>(s, 0x28), F<uint16_t>(s, 0x30));
						GteAVSZ3();
						*(uint32_t *)pk = 0x7000000;
						*(uint32_t *)(pk + 4) = ((((uint32_t)*(const uint16_t *)(p + 0xE) & 0x200) | 0x2400) << 16) | F<uint32_t>(s, 0x14);
						*(uint32_t *)(pk + 8) = F<uint32_t>(s, 0x1C);
						*(uint32_t *)(pk + 0x10) = F<uint32_t>(s, 0x24);
						*(uint32_t *)(pk + 0x18) = F<uint32_t>(s, 0x2C);
						*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 8);
						*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 0xC);
						*(uint16_t *)(pk + 0x16) &= 0xFDFF;
						*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 6);
						GteReadOTZWord(s + 0x44);
						const uint32_t bucket = ot + (uint32_t)(F<int32_t>(s, 0x44) >> shift) * 4;
						InsertPrimAutoDepth(bucket, pk);
						uint8_t *flat = pk + 0x20;
						*(uint32_t *)flat = 0x4000000;
						*(uint32_t *)(flat + 4) = F<uint32_t>(s, 0x14) | 0x22000000;
						*(uint32_t *)(flat + 8) = F<uint32_t>(s, 0x1C);
						*(uint32_t *)(flat + 0xC) = F<uint32_t>(s, 0x24);
						*(uint32_t *)(flat + 0x10) = F<uint32_t>(s, 0x2C);
						InsertPrimAutoDepth(bucket, flat);
						uint8_t *mode = pk + 0x34;
						*(uint32_t *)mode = 0x1000000;
						*(uint32_t *)(mode + 4) = 0xE1000240;
						InsertPrimAutoDepth(bucket, mode);
						pk += 0x3C;
					}
				}
				k = F<uint16_t>(s, 8);
				p += 0x10;
				F<uint16_t>(s, 8) = (uint16_t)(k - 1);
			} while (k != 0);
		}
		k = F<uint16_t>(s, 0xA);
		F<uint16_t>(s, 0xA) = (uint16_t)(k - 1);
		if (k != 0)
		{
			p += 4;
			do
			{
				CopyVertex(s, 0x1C, verts, *(const uint16_t *)(p - 4));
				CopyVertex(s, 0x24, verts, *(const uint16_t *)(p - 2));
				CopyVertex(s, 0x2C, verts, *(const uint16_t *)p);
				CopyVertex(s, 0x34, verts, *(const uint16_t *)(p + 2));
				const uint8_t d = s[0x3A], a = s[0x22], b = s[0x2A], c = s[0x32];
				if ((uint8_t)(d | a | b | c) < 0x10 && !(c & (d & a & b)))
				{
					GteSetSXY012(F<uint32_t>(s, 0x1C), F<uint32_t>(s, 0x24), F<uint32_t>(s, 0x2C));
					GteNCLIP();
					GteReadMAC0(s + 0x3C);
					if (F<int32_t>(s, 0x3C) >= 0)
					{
						GteSetSZ0123(F<uint16_t>(s, 0x20), F<uint16_t>(s, 0x28), F<uint16_t>(s, 0x30), F<uint16_t>(s, 0x38));
						GteAVSZ4();
						*(uint32_t *)pk = 0x9000000;
						*(uint32_t *)(pk + 4) = ((((uint32_t)*(const uint16_t *)(p + 0xA) & 0x200) | 0x2C00) << 16) | F<uint32_t>(s, 0x14);
						*(uint32_t *)(pk + 8) = F<uint32_t>(s, 0x1C);
						*(uint32_t *)(pk + 0x10) = F<uint32_t>(s, 0x24);
						*(uint32_t *)(pk + 0x18) = F<uint32_t>(s, 0x2C);
						*(uint32_t *)(pk + 0x20) = F<uint32_t>(s, 0x34);
						*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 4);
						*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 8);
						*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 0xC);
						*(uint16_t *)(pk + 0x16) &= 0xFDFF;
						*(uint16_t *)(pk + 0x24) = *(const uint16_t *)(p + 0xE);
						GteReadOTZWord(s + 0x44);
						const uint32_t bucket = ot + (uint32_t)(F<int32_t>(s, 0x44) >> shift) * 4;
						InsertPrimAutoDepth(bucket, pk);
						uint8_t *flat = pk + 0x28;
						*(uint32_t *)flat = 0x5000000;
						*(uint32_t *)(flat + 4) = F<uint32_t>(s, 0x14) | 0x2A000000;
						*(uint32_t *)(flat + 8) = F<uint32_t>(s, 0x1C);
						*(uint32_t *)(flat + 0xC) = F<uint32_t>(s, 0x24);
						*(uint32_t *)(flat + 0x10) = F<uint32_t>(s, 0x2C);
						*(uint32_t *)(flat + 0x14) = F<uint32_t>(s, 0x34);
						InsertPrimAutoDepth(bucket, flat);
						uint8_t *mode = pk + 0x40;
						*(uint32_t *)mode = 0x1000000;
						*(uint32_t *)(mode + 4) = 0xE1000240;
						InsertPrimAutoDepth(bucket, mode);
						pk += 0x48;
					}
				}
				k = F<uint16_t>(s, 0xA);
				p += 0x14;
				F<uint16_t>(s, 0xA) = (uint16_t)(k - 1);
			} while (k != 0);
		}
		return (uint32_t)pk;
	}

	// the bone groups of one object, warped into out; returns the end of out
	static uint8_t *WarpGroups(const uint8_t *bones, uint8_t **objp, uint8_t *out, uint8_t *s)
	{
		uint8_t *obj = *objp;
		int32_t groups = *(const int16_t *)obj;
		obj += 2;
		if (groups > 0)
		{
			do
			{
				const int32_t bone = *(const int16_t *)obj;
				obj += 2;
				const Mat4x3 *bm = (const Mat4x3 *)(bones + bone * 0x30 + 0x10);
				GteSetRotMatrixCtrl(bm);
				GteSetTransVectorCtrl(bm);
				WarpVertices(&obj, &out, s);
			} while (--groups != 0);
		}
		*objp = obj;
		return out;
	}

	// the object's two polygon counts (after its vertex groups, 4-aligned; 4 more words skipped) and its polygon list
	static void WarpCounts(uint8_t *obj, uint8_t *s)
	{
		obj = (uint8_t *)(((uintptr_t)obj + 3) & ~(uintptr_t)3);
		F<uint16_t>(s, 8) = *(const uint16_t *)obj;
		F<uint16_t>(s, 0xA) = *(const uint16_t *)(obj + 2);
		F<uint8_t *>(s, 0) = obj + 0xC;
	}

	// sub_67CD80: every object of a model the mask shows (bone groups of vertices, then polygons)
	static uint32_t WarpModel(const uint8_t *model, uint8_t *s, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint8_t *table = *(uint8_t *const *)(model + 4);
		const uint8_t *bones = *(uint8_t *const *)model + 0x10;
		const int32_t count = *(const int32_t *)table;
		const uint32_t *offs = (const uint32_t *)(table + 4);
		for (int32_t i = 0; i < count; i++)
		{
			uint8_t *obj = (uint8_t *)(*(uint8_t *const *)(model + 4) + offs[i]);
			if (!(F<uint32_t>(s, 0x18) & (1u << (i & 31)))) continue;
			WarpGroups(bones, &obj, F<uint8_t *>(s, 4), s);
			WarpCounts(obj, s);
			cursor = WarpPolygons(s, ot, shift, cursor);
		}
		return cursor;
	}

	// sub_67CC10 (head / tail): the work block for the target at (sx, sy) on screen; the colour = the entity's +0x28 faded by
	// 0x1000 - fade
	static uint8_t *WarpBegin(uint8_t *entity, int16_t sx, int16_t sy, int16_t radius, int32_t step, int32_t fade)
	{
		uint8_t *c = (uint8_t *)FieldAlloc(0x7C);
		F<int16_t>(c, 0x72) = sx;
		F<int16_t>(c, 0x74) = sy;
		F<int32_t>(c, 0x78) = fade;
		F<int16_t>(c, 0x70) = radius;
		F<uint32_t>(c, 0x6C) = WARP_Weights + (uint32_t)shl32(step, 8);
		F<int16_t>(c, 0x76) = 0;
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)c);
		FrameCursor() = DrawShadow(entity, RenderOT(0x4040), 0x10, FrameCursor());
		ComputeBonesWorldMatrices(entity + 0x60, c);
		F<int16_t>(c, 0x2C) = 0;
		F<int16_t>(c, 0x2E) = 0;
		F<uint32_t>(c, 0x24) = BoneWorkspace();
		F<int16_t>(c, 0x30) = 0x140;
		F<int16_t>(c, 0x32) = 0xD8;
		const int32_t w = 0x1000 - fade;
		GteSetIR0(w);
		GteLoadIRBytes(entity + 0x28);
		GteGPF();
		GteStoreIRBytes(c + 0x34);
		c[0x37] = w == 0x1000 ? 0 : 2;
		F<uint32_t>(c, 0x38) = *(const uint32_t *)(entity + 0x7C);
		return c;
	}

	// sub_67CC10: the shadow, then the body (+0x64) and the weapon (+0x78) of the entity, into the frame arena; returns the
	// largest offset (the next draw's radius)
	static int32_t DrawWarped(uint8_t *entity, int16_t sx, int16_t sy, int16_t radius, int32_t step, int32_t fade)
	{
		uint8_t *c = WarpBegin(entity, sx, sy, radius, step, fade);
		uint8_t *s = c + 0x20;
		FrameCursor() = WarpModel(*(const uint8_t *const *)(entity + 0x64), s, RenderOT(0x44), 0, FrameCursor());
		BuildBoneMatricesFromPose(entity + 0x60);
		uint8_t *weapon = *(uint8_t *const *)(entity + 0x78);
		if (weapon)
		{
			F<uint32_t>(c, 0x38) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(weapon, c);
			FrameCursor() = WarpModel(*(const uint8_t *const *)(weapon + 4), s, RenderOT(0x44), 0, FrameCursor());
			BuildBoneMatricesFromPose(weapon);
		}
		FieldFree(0x7C);
		return F<int16_t>(c, 0x76);
	}

	// the main task's fade of counter e (24..43 -> 0..19): sin((20 - e) x 0x400 / 20)
	static int32_t WarpFade(int32_t step) { return ComputeSin(shl32(step, 10) / 20); }

	// the warp's centre: the position through the camera (SXY); false when it is not in front of the camera
	static bool WarpCentre(const int16_t *pos, int16_t sxy[2])
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(pos);
		GteRTPS();
		uint32_t otz;
		GteReadSXY2(sxy);
		GteReadOTZ(&otz);
		return (uint16_t)otz != 0;
	}

	// the layout block (A: at the anchor with the texture scroll, B: at the position)
	static void SetBlock(PrimArg *arg, const MainNode *m, const int16_t *point, int16_t scroll, int16_t depth = -0x200)
	{
		memcpy(arg->anchor, point, sizeof(arg->anchor));
		arg->scale[0] = m->size;
		arg->scale[1] = m->size;
		arg->scale[2] = m->size;
		arg->scaled = 1;
		arg->depth = depth;
		arg->scroll = scroll;
		arg->morph = TexBase() + MORPH;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag283_magic_summon_held.h"
#endif

namespace ff8fx
{
namespace magicsummon283
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x67C8A0): one object of a layout. Vertex frame (or blend into the block's morph buffer), rotation
	// from the record's angles; the record's offset and the object scaled by the block's scale; flag 0x1000: the block's point
	// through the camera + the offset through the matrix 0x2507E70, composed with it (unless flag 0x8000); else point + offset
	// through the camera, composed with it (unless flag 0x8000); + the camera's translation, scale (scale3DMatrix), fade, the
	// texture scroll of object 1, depth, drawn by 0x701DD0 (header 0x6C; the fields it does not set keep the Field_Alloc scratch)
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x6C);
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
		// the record's offset, scaled (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (arg->scaled)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
			Scale3DMatrix(&m, arg->scale);
		}
		if (r->flags & 0x1000)
		{
			int32_t v[3];
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->anchor);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			GteSetRotMatrixCtrl(MatrixFlag1000());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(v);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)v[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)v[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)v[2]);
			if (!(r->flags & 0x8000)) MatrixMultiply(MatrixFlag1000(), &m);
		}
		else
		{
			off[0] = (int16_t)(off[0] + arg->anchor[0]);
			off[1] = (int16_t)(off[1] + arg->anchor[1]);
			off[2] = (int16_t)(off[2] + arg->anchor[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&Camera(), &m);
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
		if (arg->scroll != 0 && r->flags_lo == 1)
		{
			*(uint16_t *)(h + 0x26) = 0;
			*(uint16_t *)(h + 0x24) = 0;
			*(uint16_t *)(h + 0x2A) = 0x100;
			*(uint16_t *)(h + 0x28) = 0x100;
			*(uint16_t *)(h + 0x2E) = 0;
			*(uint16_t *)(h + 0x2C) = 0;
			*(uint16_t *)(h + 0x32) = 0x80;
			*(uint16_t *)(h + 0x30) = 0x100;
			*(int16_t *)(h + 0x22) = arg->scroll;
		}
		else *(int16_t *)(h + 0x22) = 0;
		*(int32_t *)(h + 0x18) = arg->depth;
		*(uint16_t *)(h + 0x20) = 0;
		PacketCursor() = RenderPrimSet(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x6C);
	}

	// ------------------------------------------------------------------
	// File loader (0x67C410) and setup (0x67C400 -> _Init 0x67C430): the game runs its own; Fujin's programs these (its
	// memory may be a copy's): the TIM, the magic buffer, the cast and its caster's entity, the master (it sets up the
	// rest), the camera
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(PM().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(TexFile());
		TexBase() = layouts::MagicBuffer();
		Ctx() = ctx;
		Caster() = Entity(ctx->attacker);
		InitTaskQueuePool(QMaster(), (void *)own::R(0x2507E50), 0x14, 1);
		Memset32((uint8_t *)AddTaskToQueue(QMaster(), own::Fn(ORIG_MasterTask)) + 0xC, 0, 2);
		PlayCameraAnimation((const void *)fxp::camera(PM().res.camera, CAMERA_ANIM));
		return QMaster();
	}

	// ------------------------------------------------------------------
	// Main (0x67C610)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		// the flash colour: blended (3 bytes) over the task's argument slot, its 4th byte = the node pointer's high byte
		uint32_t colour = (uint32_t)(uintptr_t)n;
		const int32_t end = PM().timing.end;
		const uint32_t colours = fxp::data(PM().tables.flash_colours, FLASH_Colours, 36);
		const int32_t t = shl32(m->counter * 7, 12) / end;
		const int32_t i = t >> 12, frac = t & 0xFFF;
		FX_PART(PART_CIRCLE);
		if ((uint32_t)i < 8) BlendColour((const void *)(colours + i * 4), (const void *)(colours + 4 + i * 4), 0x1000 - frac, frac, &colour);
		int32_t level;
		if (m->counter < PM().flash.rise || m->counter >= end - PM().flash.fall)
		{
			const int32_t c = m->counter < PM().flash.rise ? m->counter : end - m->counter;
			level = shl32(c * PM().flash.step, 3) >> 3;
			colour = 0;
		}
		else level = PM().flash.level;
		SetScreenFlash((uint32_t)level, colour);
		PrimArg arg;
		if ((uint32_t)(int32_t)m->counter < (uint32_t)end)
		{
			SetBlock(&arg, m, m->anchor, (int16_t)((m->counter & 0xF) * PM().circle.scroll), PM().circle.depth);
			// 30 fps layer: see mag283_magic_summon_held.inc
			FX_HELD(held_note_play(m, PLAY_A, &arg);)
			prim::play((prim::Layout *)m->layoutA, PrimCallback, (int)&arg, 0);
		}
		const int32_t e = m->counter - PM().summon.tick;
		if ((uint32_t)e < (uint32_t)PM().summon.ticks)
		{
			FX_PART(PART_WARP);
			if ((uint32_t)e < 0x14)
			{
				const int32_t step = 0x14 - e;
				const int32_t fade = WarpFade(step);
				GteSetIR0(0x1000 - fade);
				GteLoadIRBytes(&m->colour);
				GteGPF();
				GteStoreIRBytes(m->ent + 0x2C);
				m->ent[0x2F] = 0x32;
				int16_t sxy[2];
				if (WarpCentre(m->pos, sxy))
				{
					// 30 fps layer: see mag283_magic_summon_held.inc
					FX_HELD(held_note_warp(m, e);)
					m->radius = (int16_t)DrawWarped(m->ent, sxy[0], sxy[1], m->radius, step, fade);
				}
			}
			if (e == 0x14)
			{
				*(uint16_t *)m->ent &= 0xFFFB;
				*(uint32_t *)(m->ent + 0x2C) = m->colour;
			}
			FX_PART(PART_SUMMON);
			SetBlock(&arg, m, m->pos, 0, PM().summon.depth);
			// 30 fps layer: see mag283_magic_summon_held.inc
			FX_HELD(held_note_play(m, PLAY_B, &arg);)
			prim::play((prim::Layout *)m->layoutB, PrimCallback, (int)&arg, 0);
		}
		FX_PART(0);
		if (m->counter == PM().timing.sound) BdPlaySE((const void *)fxp::sound(PM().sound, (uint32_t)SOUND_Summon), 0, 0x80);
		m->counter++;
		return m->counter < end ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Master (0x67C4B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *mn = (MasterNode *)n;
		// 30 fps layer: see mag283_magic_summon_held.inc
		FX_HELD(held_note_master();)
		if (mn->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			mn->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			mn->arena = 1;
		}
		if (mn->counter == 1 && !mn->started)
		{
			mn->started = 1;
			InitTaskQueuePool(QMain(), (void *)TexBase(), 0x648, 1);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), own::Fn(ORIG_MainTask));
			Memset32(&m->counter, 0, 0x18F);
			m->ent = Entity(Ctx()->actions[0].targets[0]);
			m->ent[0] |= 4;
			DecodeModelPrimLayout(fxp::layout(PM().layouts.a, MODEL_A, 0x4BC), m->layoutA, 0x4BC);
			DecodeModelPrimLayout(fxp::layout(PM().layouts.b, MODEL_B, 0x160), m->layoutB, 0x160);
			GetDefaultEffectPosition(m->ent, m->pos);
			m->anchor[0] = m->pos[0];
			m->anchor[2] = m->pos[2];
			m->anchor[1] = *(const int16_t *)(m->ent + 0x24);
			m->colour = *(const uint32_t *)(m->ent + 0x2C);
			m->size = *(const int16_t *)(m->ent + 0x26);
		}
		const int a = mn->started ? ExecuteTaskQueue(QMain()) : (int)n;
		if (mn->started && a == 0)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		mn->counter++;
		return 0;
	}
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "director", "Director", "The main task's timeline: the sound, its end; the screen flash and its colours; the texture and the camera" },
			{ PART_CIRCLE, "circle", "Circle", "Layout A at the target's feet, its texture scrolling" },
			{ PART_WARP, "warp", "Warp", "The target redrawn warped away from its middle on screen, its colour fading (not a parameter: the warp's tables are code)" },
			{ PART_SUMMON, "summon", "Summon", "Layout B at the target, from the warp's start" },
		};
		static const fxp::Group GROUPS[] = {
			{ "flash", "Screen flash", "Its rise and fall (black), its level between them in the colours" },
		};
		static const Field FIELDS[] = {
			{ "director", "sound", "Sound", "At the main task's sound tick", (uint32_t)offsetof(SummonParams, sound), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "timing.sound", "Sound tick", "The sound at this tick of the main task", (uint32_t)offsetof(SummonParams, timing.sound), fxp::I16, 0, 200 },
			{ "director", "timing.end", "End tick", "The main task's ticks (layout A plays until then; the flash colours span them)", (uint32_t)offsetof(SummonParams, timing.end), fxp::I16, 1, 200 },
			{ "director", "flash.level", "Level", "The screen flash's level between its rise and its fall (4096 = full), in the colours", (uint32_t)offsetof(SummonParams, flash.level), fxp::I16, 0, 0x1000 },
			{ "director", "flash.rise", "Rise ticks", "It first rises over this many ticks, black (the tick x the step)", (uint32_t)offsetof(SummonParams, flash.rise), fxp::I16, 0, 200 },
			{ "director", "flash.fall", "Fall ticks", "It falls over the last ticks, black ((the end - the tick) x the step)", (uint32_t)offsetof(SummonParams, flash.fall), fxp::I16, 0, 200 },
			{ "director", "flash.step", "Step", "", (uint32_t)offsetof(SummonParams, flash.step), fxp::I16, 0, 0x1000 },
			{ "director", "tables.flash_colours", "Flash colours", "The colours the flash goes through over the main task: 9 x (red, green, blue, -), the 9th the 8th's end (36 bytes)", (uint32_t)offsetof(SummonParams, tables.flash_colours), fxp::Data, 0, 0x7FFFFFFF },
			{ "director", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(SummonParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "director", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(SummonParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "circle", "circle.depth", "Depth", "Layout A's depth offset (negative = nearer)", (uint32_t)offsetof(SummonParams, circle.depth), fxp::I16, -8192, 0x2000 },
			{ "circle", "circle.scroll", "Scroll", "Its object 1's texture scrolls this much a tick (16 ticks a cycle; 0: none)", (uint32_t)offsetof(SummonParams, circle.scroll), fxp::I16, -64, 64 },
			{ "circle", "layouts.a", "Layout A", "The prim-model layout at the target's feet (its models' animation), scaled by the target's size", (uint32_t)offsetof(SummonParams, layouts.a), fxp::Layout, 0, 0x7FFFFFFF },
			{ "summon", "summon.tick", "Tick", "From this tick the target is warped away (20 ticks: its colour fading out, redrawn pushed from its middle on screen) and layout B plays", (uint32_t)offsetof(SummonParams, summon.tick), fxp::I16, 0, 200 },
			{ "summon", "summon.ticks", "Ticks", "Layout B plays this many ticks (the target is put back after the warp's 20)", (uint32_t)offsetof(SummonParams, summon.ticks), fxp::I16, 0, 200 },
			{ "summon", "summon.depth", "Depth", "Layout B's depth offset (negative = nearer)", (uint32_t)offsetof(SummonParams, summon.depth), fxp::I16, -8192, 0x2000 },
			{ "summon", "layouts.b", "Layout B", "The prim-model layout at the target (its models' animation), scaled by the target's size", (uint32_t)offsetof(SummonParams, layouts.b), fxp::Layout, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag283_magic_summon()
	{
		fxp::register_params({ 283, "Magic Summon", &magicsummon283::live, &magicsummon283::GAME, sizeof(magicsummon283::SummonParams),
		                       magicsummon283::table::PARTS, (int)(sizeof(magicsummon283::table::PARTS) / sizeof(magicsummon283::table::PARTS[0])),
		                       magicsummon283::table::FIELDS, (int)(sizeof(magicsummon283::table::FIELDS) / sizeof(magicsummon283::table::FIELDS[0])),
		                       magicsummon283::table::GROUPS, (int)(sizeof(magicsummon283::table::GROUPS) / sizeof(magicsummon283::table::GROUPS[0])) });
		own::note(magicsummon283::ORIG_FileLoader, (void *)magicsummon283::FileLoader);
		own::note(magicsummon283::ORIG_Setup, (void *)magicsummon283::Setup);
		own::note(magicsummon283::ORIG_MasterTask, (void *)magicsummon283::MasterTask);
		register_port(magicsummon283::ORIG_MasterTask, (void *)magicsummon283::MasterTask, "283 MasterTask", 283);
		own::note(magicsummon283::ORIG_MainTask, (void *)magicsummon283::MainTask);
		register_port(magicsummon283::ORIG_MainTask, (void *)magicsummon283::MainTask, "283 MainTask", 283);
		// 30 fps layer: see mag283_magic_summon_held.inc
		FX_HELD(register_mag283_held();)
	}
	FX_LAB_MODULE(register_mag283_magic_summon);
}

#ifdef FF8_FX_HELD
#include "mag283_magic_summon_held.inc"
#endif
