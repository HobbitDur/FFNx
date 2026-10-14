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

// Effect 289: "Brrawghh!" (enemy attack 131 of kernel.bin, Gerogero c0m034 and Gunblade c0m142;
// MAG_289_*). No other kernel.bin entry uses effect 289.
//
// Structure (setup MAG_289_UNKNOWN 0x6777E0 -> _Init 0x677810, file loader 0x6777F0 = the texture
// file named at 0x1074648; the setup keeps the action context (0x2507BD4) and the CASTER's entity
// (0x2507BD8), starts camera animation 0x10724E0 and queues the TIM):
//   MasterTask (0x677890) - alternates the packet arena (magic buffer + 0x1A40 / + 0x11A40); on
//     counter 1 sets up the pools (sprites 0x40 x 0x24 at + 0, splash layouts 0x20 x 0x54 at + 0x900,
//     main 1 x 0x1A0 at + 0x1380), starts the main task (the first target of action 0, its default
//     effect point 0x571400, layout 0x1070804) and rewrites the w word of every vertex of the two tube
//     meshes 0x106EDE4 / 0x106FAF4 as -z (0x677A10; static exe tables rewritten on EVERY cast, with the
//     same values). Then the main, sprite and splash queues; ends when the main AND the splash queues
//     are empty (sprites are abandoned).
//   MainTask (0x677A30) - counters 0..0x1D. Counters 0..0x15: t = counter * 4096 / 22; P = the target
//     point moved along x / -z by (t - 0x800) / 2 at the target's height word +0x24; B = the caster's
//     bone 9 point 0x1072640 (0x678390, the bone matrices as they are); M = the basis of P - B (y and
//     z the other way) scaled by its length, at B; three tube draws (0x678510) through the camera x M:
//     mesh A, then mesh B twice (angles, wave amplitudes, fade offset, U scroll and x / y scale from
//     the counter and sines of t / 2 and t). Counters 2..0xB: four spit sprites per tick at B (CRT
//     rand: direction, speed, scale) through M; counters 0xE..0x15: a drip sprite and a splat sprite
//     at P (CRT rand); counters 8..0x15: a splash layout task at P (layout 0x1071E08) and a splat
//     sprite. Counters 0xA..0x1D: layout 0x1070804 at the target point. Sound at counter 0, damage
//     at counter 0xF.
//   TubeDraw (0x678510) - the mesh bent by 0x6790E0 (RotZ(angle) scaled in x / y, each y lowered by
//     the table 0x1072648 at w / 2 and w, w -> |w - offset| up to 0x1000) into magic buffer + 0x1520,
//     a 0x6C header (texture windows 0x100 x 0x100 and 0x80 x 0x100, U scroll, wrap 0x80) drawn by
//     0x6785C0: far colour black, flat textured quads 0x678620 then Gouraud textured quads 0x678B00
//     (back faces culled unless header flag 0x10 / 0x20; a vertex w of 0x1000 or more hides the quad,
//     else w depth-cues its colours; header flags 2 / 8 force / clear semi-transparency).
//   SpriteTask (0x6783F0, Saliva's spray 0x6AA890 shape) - a sprite sequence through the
//     shadow-rotation camera; position += velocity; ends after its frame count.
//   SplashTask (0x678490) - its layout at its point (RotY(0) composed with the camera, depth -0x80);
//     ends when the layout ends.
//   PrimCallback (0x6780F0, = Saliva's 0x6AA510 on this module's cursor) draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws THEN updates (the main counter after its draws, a layout in its
// play, a sprite after its draw).
// ENTITY / CAMERA: no task writes an entity (positions +0x1C..+0x20 and flags only read) or the camera
// words (the camera animation is the engine's).
// EXE DATA WRITES: the tube meshes 0x106EDE4 / 0x106FAF4 (vertex w words, every cast, counter 1).
// Module globals: 0x2507BD0..0x2507C3C (texture file 0x2507BD0, context 0x2507BD4, caster entity
// 0x2507BD8, the queues 0x2507BE0 sprites / 0x2507BF0 splashes / 0x2507C00 main / 0x2507C10 master,
// master pool 0x2507C20, packet cursor 0x2507C34, magic buffer base 0x2507C38).
// NEVER-WRITTEN STACK WORDS (the game's executor 0x508420 calls every task of a queue at one esp; the
// master pushes its three queue arguments without popping: main entry esp E, sprites E - 4, splashes
// E - 8):
//   - MainTask's moved target point P word +6 (entry esp - 0x7E) is never written; on counters 8..0x15
//     it is copied into a new splash node's word +0x12 (never read). Nothing the tick runs before the
//     main task reaches it (the main task is the first module task of every tick): tick-start residue,
//     i.e. whatever the battle loop left at that depth (0 under the game executor of the verification
//     harness, which clears the stack at the tick start). MODEL: 0 (SplashResidue).
//   - MainTask's bone point B word +6 (entry esp - 0x86) goes into a spit sprite's frame word, which the
//     next statements overwrite with 0; the block / basis matrices' pad words (GTE R33 pad 0x1CA928E only)
//     and the callback's record offset word +6 (GTE VZ0 high half 0x1CA8A16 only) reach no output.
//   - The tube header's flag byte +0x1C (back faces, semi-transparency) is never written: Field_Alloc
//     scratch, read where the port allocates it too.

#include "fx_port.h"
#include "mag_common.h"
#include "mag_ice_common.h"

namespace ff8fx
{
namespace brr289
{
	using namespace eng;
	using namespace magc;
	using namespace icec;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2507BD4); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2507BD8); }        // caster entity (setup)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507C34); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507C38); }        // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2507BE0; }        // pool: magic buffer, 0x40 x 0x24
	inline TaskQueue *QSplashes() { return (TaskQueue *)0x2507BF0; }       // pool: + 0x900, 0x20 x 0x54
	inline TaskQueue *QMain() { return (TaskQueue *)0x2507C00; }           // pool: + 0x1380, 1 x 0x1A0

	static const uint32_t ORIG_MasterTask = 0x677890;
	static const uint32_t ORIG_MainTask = 0x677A30;
	static const uint32_t ORIG_SpriteTask = 0x6783F0;
	static const uint32_t ORIG_SplashTask = 0x678490;
	static const uint32_t VERTS = 0x1520;                // magic buffer: bent tube vertices / blended vertex frames
	static const uint32_t ARENA_A = 0x1A40;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11A40;
	static uint8_t *const TUBE_A = (uint8_t *)0x106EDE4; // tube meshes: list offset, count, vertices (w rewritten), lists
	static uint8_t *const TUBE_B = (uint8_t *)0x106FAF4;
	static const uint32_t LAYOUT_Target = 0x1070804;     // prim-model layouts
	static const uint32_t LAYOUT_Splash = 0x1071E08;
	static const uint32_t SEQ_Drip = 0x106EC38;          // sprite sequences
	static const uint32_t SEQ_Splat = 0x106E9B0;
	static const uint32_t SEQ_Spit = 0x106EAF4;
	static const int16_t *const POINT = (const int16_t *)0x1072640;   // caster bone point (x, y, z, bone 9)
	static const int16_t *const WAVE = (const int16_t *)0x1072648;    // 0x1001 x s16, by vertex w and w / 2
	static const void *const SOUND_Roar = (const void *)0x1072638;

	// engine functions not in fx_port.h / mag_common.h / mag_ice_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAngles2(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // sub_56C220: a = a x b (3x3)
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }   // MAG_063_sub_701270
	inline void RotZ(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x7012C0)(angle, out); }   // MAG_065_sub_7012C0
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }        // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                                       // sub_45F270
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }               // set_param_with_dword_1CA8A68

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2507C20)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x1A0 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint8_t *target;   // +0x10 first target of action 0
		int16_t pos[4];    // +0x14 the target's default effect point
		uint8_t layout[0x184]; // +0x1C layout 0x1070804
	};
	struct SpriteNode // pool of 0x40 nodes of 0x24 bytes
	{
		TaskNode hdr;
		uint32_t seq;      // +0x0C sprite sequence
		int32_t scale;     // +0x10
		int16_t pos[3];    // +0x14
		int16_t frame;     // +0x1A (also the 4th word of pos)
		int16_t vel[3];    // +0x1C
		int16_t frames;    // +0x22 (also the 4th word of vel)
	};
	struct SplashNode // pool of 0x20 nodes of 0x54 bytes
	{
		TaskNode hdr;
		int16_t pos[4];    // +0x0C (+0x12: a stack word of the main task, never read, see SplashResidue)
		uint8_t layout[0x40]; // +0x14 layout 0x1071E08
	};
	// the prim-player callback's parameter block (a stack block of the playing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 rotation (pad word +0x12: R33 pad only) and anchor
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x1A0, "Brrawghh nodes");
	static_assert(sizeof(SpriteNode) == 0x24 && sizeof(SplashNode) == 0x54, "Brrawghh nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Brrawghh prim block");

	static uint8_t *&Cursor(uint8_t *h, int off) { return *(uint8_t **)(h + off); }
	static bool OffX(int16_t x) { return x < 0 || x > 0xA00; }
	static bool OffY(int16_t y) { return y < 0 || y > 0x6C0; }

	// ------------------------------------------------------------------
	// Tube renderer (0x6785C0). Header (0x6C, Field_Alloc): +0x00 mesh, +0x04 vertices, +0x08 far
	// colour bytes (0), +0x1C flags (never written: Field_Alloc scratch), +0x20 U scroll, +0x24 /
	// +0x2C texture windows, +0x30 U wrap byte, +0x34 list cursor, +0x38 MAC0, +0x40 OTZ, +0x44
	// FLAG, +0x48 the first vertex of the quad (its w = the depth cue).
	// ------------------------------------------------------------------
	static void ScrollU(uint8_t *p, const int *off, uint16_t add, uint8_t wrap)
	{
		uint32_t v[4], o = 0;
		for (int k = 0; k < 4; k++)
		{
			v[k] = p[off[k]] + (uint32_t)add;
			o |= v[k];
		}
		for (int k = 0; k < 4; k++) p[off[k]] = (uint8_t)(o > 0xFF ? v[k] - wrap : v[k]);
	}

	// the main packet between the texture windows +0x24 / +0x2C (no draw mode)
	static uint8_t *Windowed(const uint8_t *h, uint32_t otp, uint8_t *main, uint8_t *p)
	{
		uint8_t *t1 = p;
		*(uint32_t *)t1 = 0x02000000;
		*(uint32_t *)(t1 + 4) = TextureWindow(h + 0x24);
		*(uint32_t *)(t1 + 8) = 0;
		InsertPrimAutoDepth(otp, t1);
		InsertPrimAutoDepth(otp, main);
		uint8_t *t2 = p + 0xC;
		*(uint32_t *)t2 = 0x02000000;
		*(uint32_t *)(t2 + 4) = TextureWindow(h + 0x2C);
		*(uint32_t *)(t2 + 8) = 0;
		InsertPrimAutoDepth(otp, t2);
		return p + 0x18;
	}

	// a quad's screen clip bits (x 1 / 2 / 4 / 8, y 0x10 / 0x20 / 0x40 / 0x80): drawn unless all four off
	// one side
	static uint32_t TriClip(const uint8_t *a, const uint8_t *b, const uint8_t *c)
	{
		uint32_t clip = 0;
		if (OffX(*(const int16_t *)a)) clip = 1;
		if (OffX(*(const int16_t *)b)) clip |= 2;
		if (OffX(*(const int16_t *)c)) clip |= 4;
		if (OffY(*(const int16_t *)(a + 2))) clip |= 0x10;
		if (OffY(*(const int16_t *)(b + 2))) clip |= 0x20;
		if (OffY(*(const int16_t *)(c + 2))) clip |= 0x40;
		return clip;
	}
	static bool QuadClipped(uint32_t clip3, const uint8_t *x3)
	{
		uint32_t clip = clip3;
		if (OffX(*(const int16_t *)x3)) clip |= 8;
		if (OffY(*(const int16_t *)(x3 + 2))) clip |= 0x80;
		return (clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0;
	}

	// the quad's first vertex into the header (+0x48); its w word (+0x4E): 0x1000 or more = hidden
	static bool FirstVertex(uint8_t *h, const uint8_t *verts, const uint8_t *rec)
	{
		const uint32_t i = *(const uint16_t *)(rec + 4);
		*(uint32_t *)(h + 0x48) = *(const uint32_t *)(verts + 4 * i);
		*(uint32_t *)(h + 0x4C) = *(const uint32_t *)(verts + 4 * i + 4);
		return *(const int16_t *)(h + 0x4E) < 0x1000;
	}

	// 0x678620: flat textured quads (0x18-byte records, 0x28-byte packets), back faces culled unless
	// header flag 0x10, colour depth-cued by the first vertex's w
	static uint32_t FlatTexQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x34);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		const uint8_t *verts = Cursor(h, 4);
		Cursor(h, 0x34) = rec;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x18)
		{
			if (!FirstVertex(h, verts, rec)) continue;
			GteLoadV012(h + 0x48, verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)p = 0x09000000;
			*(uint32_t *)(p + 4) = *(const uint32_t *)rec;
			*(uint32_t *)(p + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(p + 0x14) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			*(uint32_t *)(p + 0x1C) = uv;
			*(uint32_t *)(p + 0x24) = uv >> 16;
			GteReadFLAG(h + 0x44);
			if (*(const uint32_t *)(h + 0x44) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x38);
			if (*(const int32_t *)(h + 0x38) < 0 && !(h[0x1C] & 0x10)) continue;
			GteReadSXY012(p + 8, p + 0x10, p + 0x18);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			const uint32_t clip = TriClip(p + 8, p + 0x10, p + 0x18);
			GteReadSXY2(p + 0x20);
			GteAVSZ4();
			if (QuadClipped(clip, p + 0x20)) continue;
			GteReadOTZReg(h + 0x40);
			const int16_t w = *(const int16_t *)(h + 0x4E);
			if (w != 0)
			{
				GteLoadRGBC(p + 4);
				GteSetIR0(w);
				GteDPCS();
				GteStoreRGB2(p + 4);
			}
			const uint32_t otp = ot + 4 * (*(const int32_t *)(h + 0x40) >> shift);
			if (*(const uint16_t *)(h + 0x20) != 0)
			{
				static const int us[4] = { 0xC, 0x14, 0x1C, 0x24 };
				ScrollU(p, us, *(const uint16_t *)(h + 0x20), h[0x30]);
				p = Windowed(h, otp, p, p + 0x28);
			}
			else
			{
				InsertPrimAutoDepth(otp, p);
				p += 0x28;
			}
		}
		Cursor(h, 0x34) = rec;
		return (uint32_t)p;
	}

	// 0x678B00: Gouraud textured quads (0x24-byte records, 0x34-byte packets), header flag 2 / 8 sets /
	// clears semi-transparency, back faces culled unless flag 0x20, colours depth-cued by the first
	// vertex's w
	static uint32_t GouraudTexQuads(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = Cursor(h, 0x34);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		const uint8_t *verts = Cursor(h, 4);
		Cursor(h, 0x34) = rec;
		uint8_t *p = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x24)
		{
			if (!FirstVertex(h, verts, rec)) continue;
			GteLoadV012(h + 0x48, verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)p = 0x0C000000;
			*(uint32_t *)(p + 4) = *(const uint32_t *)rec;
			if (h[0x1C] & 2) *(uint32_t *)(p + 4) |= 0x02000000;
			if (h[0x1C] & 8) *(uint32_t *)(p + 4) &= 0xFDFFFFFF;
			*(uint32_t *)(p + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(p + 0x18) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			*(uint32_t *)(p + 0x24) = uv;
			*(uint32_t *)(p + 0x30) = uv >> 16;
			GteReadFLAG(h + 0x44);
			if (*(const uint32_t *)(h + 0x44) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x38);
			if (*(const int32_t *)(h + 0x38) < 0 && !(h[0x1C] & 0x20)) continue;
			GteReadSXY012(p + 8, p + 0x14, p + 0x20);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			const uint32_t clip = TriClip(p + 8, p + 0x14, p + 0x20);
			GteReadSXY2(p + 0x2C);
			GteAVSZ4();
			if (QuadClipped(clip, p + 0x2C)) continue;
			GteReadOTZReg(h + 0x40);
			const int16_t w = *(const int16_t *)(h + 0x4E);
			if (w != 0)
			{
				GteLoadRGB012(rec + 0x18, rec + 0x1C, rec + 0x20);
				GteSetIR0(w);
				GteDPCT();
				GteStoreRGB012(p + 0x10, p + 0x1C, p + 0x28);
				GteLoadRGBC(p + 4);
				GteDPCS();
				GteStoreRGB2(p + 4);
			}
			else
			{
				*(uint32_t *)(p + 0x10) = *(const uint32_t *)(rec + 0x18);
				*(uint32_t *)(p + 0x1C) = *(const uint32_t *)(rec + 0x1C);
				*(uint32_t *)(p + 0x28) = *(const uint32_t *)(rec + 0x20);
			}
			const uint32_t otp = ot + 4 * (*(const int32_t *)(h + 0x40) >> shift);
			if (*(const uint16_t *)(h + 0x20) != 0)
			{
				static const int us[4] = { 0xC, 0x18, 0x24, 0x30 };
				ScrollU(p, us, *(const uint16_t *)(h + 0x20), h[0x30]);
				p = Windowed(h, otp, p, p + 0x34);
			}
			else
			{
				InsertPrimAutoDepth(otp, p);
				p += 0x34;
			}
		}
		Cursor(h, 0x34) = rec;
		return (uint32_t)p;
	}

	// 0x6785C0: far colour from the header bytes +8..+0xA, the first list (flat textured quads), then
	// the second (Gouraud textured quads); each list follows 0xC bytes after the previous one's end
	static uint32_t TubeRender(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *m = Cursor(h, 0);
		Cursor(h, 0x34) = m + *(const uint32_t *)m;
		GteSetFarColor(h[8], h[9], h[0xA]);
		Cursor(h, 0x34) += 0xC;
		const uint32_t r = FlatTexQuads(h, ot, shift, cursor);
		Cursor(h, 0x34) += 0xC;
		return GouraudTexQuads(h, ot, shift, r);
	}

	// 0x6790E0: the mesh's vertices through RotZ(angle) (x / y columns scaled by `scale` unless 0x1000)
	// into out; w -> |w - wofs| up to 0x1000; y lowered by (WAVE[w / 2] * amp_a + WAVE[w] * amp_b) >> 12
	static void TubeBend(const uint8_t *model, int32_t angle, int32_t amp_a, int32_t amp_b, int32_t wofs, int32_t scale, int16_t *out)
	{
		uint32_t count = *(const uint32_t *)(model + 4);
		const int16_t *v = (const int16_t *)(model + 8);
		Mat4x3 R = {};
		RotZ(angle, &R);
		if (scale != 0x1000)
		{
			R.m[0][0] = (int16_t)(mul32(R.m[0][0], scale) >> 12);
			R.m[1][0] = (int16_t)(mul32(R.m[1][0], scale) >> 12);
			R.m[0][1] = (int16_t)(mul32(R.m[0][1], scale) >> 12);
			R.m[1][1] = (int16_t)(mul32(R.m[1][1], scale) >> 12);
		}
		GteSetRotMatrix(&R);
		do
		{
			GteLoadV0(v);
			GteMVMVA_RotV0();
			int32_t d = (int32_t)((uint32_t)(int32_t)v[3] - (uint32_t)wofs);
			if (d < 0) d = -d;
			if (d > 0x1000) d = 0x1000;
			out[3] = (int16_t)d;
			const int32_t w = v[3];
			const int32_t b = WAVE[w];
			const int32_t a = WAVE[w >> 1];
			v += 4;
			GteStoreIR123(out);
			const int32_t y = (int32_t)((uint32_t)mul32(a, amp_a) + (uint32_t)mul32(b, amp_b)) >> 12;
			out[1] = (int16_t)(out[1] - (int16_t)y);
			out += 4;
		} while (--count);
	}

	// 0x678510: one tube draw: the mesh bent (TubeBend) into `verts`, a 0x6C header (windows 0x100 x
	// 0x100 at 0 / 0 and 0x80 x 0x100 at 0 / 0, U scroll uadd & 0x7F, wrap 0x80), the camera x M
	static void TubeDraw(const Mat4x3 *m, const uint8_t *model, int32_t angle, int32_t amp_a, int32_t amp_b, int32_t wofs, int32_t uadd, int32_t scale, uint32_t verts)
	{
		TubeBend(model, angle, amp_a, amp_b, wofs, scale, (int16_t *)verts);
		uint8_t *h = AllocHeader(0x6C);
		*(uint16_t *)(h + 0x26) = 0;
		*(uint16_t *)(h + 0x24) = 0;
		*(uint16_t *)(h + 0x2A) = 0x100;
		*(uint16_t *)(h + 0x28) = 0x100;
		*(uint16_t *)(h + 0x2E) = 0;
		*(uint16_t *)(h + 0x2C) = 0;
		*(uint16_t *)(h + 0x30) = 0x80;
		*(uint16_t *)(h + 0x32) = 0x100;
		*(uint16_t *)(h + 0x20) = (uint16_t)(uadd & 0x7F);
		*(const uint8_t **)h = model;
		*(uint32_t *)(h + 4) = verts;
		*(uint32_t *)(h + 8) = 0;
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		PacketCursor() = TubeRender(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x6C);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6780F0, = Saliva's 0x6AA510 on this module's cursor): one object of a
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

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------
	// 0x677A10: w of every vertex = -z (the static exe meshes, rewritten on every cast)
	static void NegateZ(uint8_t *model)
	{
		uint32_t count = *(const uint32_t *)(model + 4);
		uint8_t *p = model + 0xE;
		do
		{
			*(int16_t *)p = (int16_t)-*(const int16_t *)(p - 2);
			p += 8;
		} while (--count);
	}

	// 0x678390 (= Saliva's 0x6AA7B0 / Hypnotize's 0x6B18A0): a point through the caster's bone (word +6)
	// composed with its matrix (the bone matrices as they are)
	static void BonePoint(const int16_t *v, int16_t *out)
	{
		uint8_t *c = Caster();
		Mat4x3 m = {};
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(**(uint8_t ***)(c + 0x64) + 0x20 + (int32_t)v[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// ------------------------------------------------------------------
	// The tubes of a main counter (counters 0..0x15)
	// ------------------------------------------------------------------
	static int32_t TubeT(int32_t c) { return shl32(c, 12) / 22; }
	// draw k's arguments (TubeDraw angle, amp_a, amp_b, wofs, uadd, scale), its sines in the original's
	// order
	static void TubeArgs(int32_t c, int k, int32_t *x)
	{
		const int32_t t = TubeT(c);
		const int32_t uadd = shl32(-c, 3);
		if (k == 0)
		{
			x[0] = shl32(c, 4);
			x[1] = ComputeSin(t >> 1) / 4 + 0xFA;
			x[2] = -(t / 8);
			x[3] = c < 0xC ? c * 558 - 4648 : c > 0xE ? c * 877 - 10230 : 0x800;
			x[4] = uadd;
			x[5] = c > 0xA ? (0x16 - c) * 0x174 : 0x1000;
			return;
		}
		int32_t wofs = c * 558 - 4648;
		if (wofs > 0) wofs = shl32(wofs, 1);
		const int32_t scale = c > 2 ? (0x16 - c) * 0x174 : 0x1000;
		if (k == 1)
		{
			x[0] = shl32(c, 4);
			x[2] = ComputeSin(t >> 1) / 8;
			x[1] = ComputeSin(t >> 1) / 4 + 0x15E;
		}
		else
		{
			x[0] = shl32(-c, 4);
			x[2] = -(ComputeSin(t) / 8);
			x[1] = ComputeSin(t >> 1) / 4 + 0x96;
		}
		x[3] = wofs;
		x[4] = uadd;
		x[5] = scale;
	}
	// P: the target point moved along x / -z by (t - 0x800) / 2, at the target's height word +0x24
	static void TubeTarget(const MainNode *w, int32_t c, int16_t *P)
	{
		const int32_t d = (TubeT(c) - 0x800) >> 1;
		P[0] = (int16_t)(w->pos[0] + d);
		P[1] = *(const int16_t *)(w->target + 0x24);
		P[2] = (int16_t)(w->pos[2] - d);
	}
	// M: the basis of P - B (y and z the other way) scaled by its length, at B
	static void TubeBasis(const int16_t *P, const int16_t *B, Mat4x3 *M)
	{
		const int32_t D[3] = { (int32_t)P[0] - B[0], (int32_t)B[1] - P[1], (int32_t)B[2] - P[2] };
		const int32_t r = BuildOrthonormalBasis(D, M);
		const int32_t s[3] = { r, r, r };
		Scale3DMatrix(M, s);
		M->t[0] = B[0];
		M->t[1] = B[1];
		M->t[2] = B[2];
	}
	static const uint8_t *const TUBE_MESH[3] = { TUBE_A, TUBE_B, TUBE_B };

	static void Play(void *layout, const PrimArg *arg)
	{
		prim::play((prim::Layout *)layout, PrimCallback, (int)arg, 0);
	}

	// the block of a layout at a point: RotY(0) at the point composed with the camera
	static void BlockAt(PrimArg *arg, const int16_t *p)
	{
		RotY(0, &arg->m);
		arg->m.t[0] = p[0];
		arg->m.t[1] = p[1];
		arg->m.t[2] = p[2];
		ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
	}

	// the main task's moved target point P word +6 (entry esp - 0x7E): never written, copied into a
	// splash node's word +0x12 (never read) on counters 8..0x15; tick-start residue (see the module
	// comment), 0 under the game executor
	static int16_t SplashResidue()
	{
		return 0;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag289_brrawghh_held.h"
#endif

namespace ff8fx
{
namespace brr289
{
	// ------------------------------------------------------------------
	// Sprites (0x6783F0, Saliva's spray 0x6AA890 shape): sequence through the shadow-rotation
	// camera, frame +0x1A of +0x22
	// ------------------------------------------------------------------
	static void SpriteDraw(uint32_t seq, const int16_t *pos, int32_t scale, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, (int16_t)scale, -(scale >> 2));
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SpriteTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag289_brrawghh_held.inc
		FX_HELD(held_note_sprite(n);)
		SpriteDraw(s->seq, s->pos, s->scale, s->frame);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->frame++;
		return s->frame < s->frames ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Splash (0x678490): its layout at its point
	// ------------------------------------------------------------------
	static uint32_t __cdecl SplashTask(TaskNode *n)
	{
		SplashNode *s = (SplashNode *)n;
		PrimArg arg;
		arg.m.pad = 0;
		BlockAt(&arg, s->pos);
		arg.morph = TexBase() + VERTS;
		arg.depth = -0x80;
		// 30 fps layer: see mag289_brrawghh_held.inc
		FX_HELD(held_note_play(SPLASH, n, (prim::Layout *)s->layout);)
		return prim::play((prim::Layout *)s->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Main (0x677A30)
	// ------------------------------------------------------------------
	// a new sprite at pos +- spread on x / z, at the target's height + rand * (its word +0x92) >> 15
	static void SpawnAtTarget(SpriteNode *s, const MainNode *w, uint32_t seq, int32_t mask, int32_t half)
	{
		s->seq = seq;
		s->pos[0] = (int16_t)((int16_t)((CrtRand() & mask) + w->pos[0]) - half);
		s->pos[2] = (int16_t)((int16_t)((CrtRand() & mask) + w->pos[2]) - half);
		const uint8_t *e = w->target;
		s->pos[1] = (int16_t)((int16_t)(mul32(CrtRand(), *(const int16_t *)(e + 0x92)) >> 15) + *(const int16_t *)(e + 0x24));
	}
	// a sprite velocity through the rotation the GTE holds (M)
	static void RotateVelocity(SpriteNode *s, int16_t frames, int32_t mask, int32_t add)
	{
		GteLoadV0(s->vel);
		GteMVMVA_RotV0();
		s->frames = frames;
		s->frame = 0;
		s->scale = (CrtRand() & mask) + add;
		GteStoreIR123(s->vel);
	}

	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *w = (MainNode *)n;
		int16_t B[4];      // entry esp - 0x8C: the bone point (4th word never written: copied into a spit sprite's frame word, then overwritten)
		B[3] = 0;
		int16_t P[4];      // entry esp - 0x84: the moved target point (4th word never written, see SplashResidue)
		P[3] = SplashResidue();
		Mat4x3 M = {};     // entry esp - 0x68
		const int16_t c = w->counter;
		if (c < 0x16)
		{
			TubeTarget(w, c, P);
			BonePoint(POINT, B);
			TubeBasis(P, B, &M);
			Mat4x3 CM = {};
			ComposeAffineTransform(&Camera(), &M, &CM);
			for (int k = 0; k < 3; k++)
			{
				int32_t x[6];
				TubeArgs(c, k, x);
				// 30 fps layer: see mag289_brrawghh_held.inc
				FX_HELD(held_note_tube(n, k, B, P);)
				TubeDraw(&CM, TUBE_MESH[k], x[0], x[1], x[2], x[3], x[4], x[5], TexBase() + VERTS);
			}
		}
		const int32_t e = c - 0xA;
		if ((uint32_t)e < 0x14)
		{
			if ((uint32_t)e >= 4 && (uint32_t)e < 0xC)
			{
				GteSetRotMatrix(&M);
				SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
				if (s)
				{
					SpawnAtTarget(s, w, SEQ_Drip, 0x1FF, 0x100);
					s->vel[1] = 0;
					s->vel[2] = 0;
					s->vel[0] = 0;
					s->frames = 0x10;
					s->frame = 0;
					s->scale = (CrtRand() & 0x1FF) + 0x5DC;
				}
				s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
				if (s)
				{
					SpawnAtTarget(s, w, SEQ_Splat, 0xFF, 0x80);
					s->vel[0] = 0;
					s->vel[2] = (int16_t)(-0x20 - (CrtRand() & 0xF));
					s->vel[1] = (int16_t)(-0x20 - (CrtRand() & 0xF));
					RotateVelocity(s, 0xC, 0x3FF, 0x7D0);
				}
			}
			PrimArg arg;
			arg.m.pad = 0;
			BlockAt(&arg, w->pos);
			arg.morph = TexBase() + VERTS;
			arg.depth = -0x80;
			// 30 fps layer: see mag289_brrawghh_held.inc
			FX_HELD(held_note_play(TARGET, n, (prim::Layout *)w->layout);)
			Play(w->layout, &arg);
		}
		if (c == 0) BdPlaySE(SOUND_Roar, 0, 0x80);
		if ((uint32_t)(c - 2) < 0xA)
		{
			GteSetRotMatrix(&M);
			for (int i = 4; i; i--)
			{
				SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
				if (!s) continue;
				s->seq = SEQ_Spit;
				memcpy(s->pos, B, 8);
				const int32_t a = CrtRand();
				const int32_t r = (CrtRand() & 0xF) + 0x20;
				s->vel[0] = (int16_t)(mul32(ComputeSin(a), r) >> 12);
				s->vel[1] = (int16_t)(mul32(ComputeCos(a), r) >> 12);
				s->vel[2] = (int16_t)(-0x20 - (CrtRand() & 0x10));
				RotateVelocity(s, 0xC, 0x1FF, 0x3E8);
			}
		}
		if ((uint32_t)(c - 8) < 0xE)
		{
			SplashNode *l = (SplashNode *)AddTaskToQueue(QSplashes(), ORIG_SplashTask);
			if (l)
			{
				memcpy(l->pos, P, 8);
				DecodeModelPrimLayout(LAYOUT_Splash, l->layout, 0x40);
			}
			GteSetRotMatrix(&M);
			SpriteNode *s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
			if (s)
			{
				memcpy(s->pos, P, 8);
				s->seq = SEQ_Splat;
				s->vel[0] = 0;
				s->vel[2] = (int16_t)(-0x10 - (CrtRand() & 0xF));
				s->vel[1] = (int16_t)(-0x20 - (CrtRand() & 0xF));
				RotateVelocity(s, 0xC, 0x3FF, 0x7D0);
			}
		}
		if (w->counter == 0xF) ApplyActionResultToTarget(Ctx()->actions->targets);
		w->counter++;
		return w->counter < 0x1E ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Master (0x677890)
	// ------------------------------------------------------------------
	static void StartMain(MasterNode *m)
	{
		m->started = 1;
		InitTaskQueuePool(QMain(), (void *)(TexBase() + 0x1380), 0x1A0, 1);
		InitTaskQueuePool(QSprites(), (void *)TexBase(), 0x24, 0x40);
		InitTaskQueuePool(QSplashes(), (void *)(TexBase() + 0x900), 0x54, 0x20);
		MainNode *w = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
		Memset32(&w->counter, 0, 0x65);
		w->target = Entity(Ctx()->actions->targets[0]);
		DecodeModelPrimLayout(LAYOUT_Target, w->layout, 0x184);
		GetDefaultEffectPosition(w->target, w->pos);
		NegateZ(TUBE_A);
		NegateZ(TUBE_B);
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag289_brrawghh_held.inc
		FX_HELD(held_note_master();)
		if (m->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			m->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			m->arena = 1;
		}
		if (m->counter == 1 && !m->started) StartMain(m);
		int r_main, r_splash;
		if (m->started)
		{
			r_main = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QSprites());
			r_splash = ExecuteTaskQueue(QSplashes());
		}
		else r_main = r_splash = (int)n;
		if (m->started && r_main == 0 && r_splash == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag289_brrawghh()
	{
		register_port(brr289::ORIG_MasterTask, (void *)brr289::MasterTask, "289 MasterTask", 289);
		register_port(brr289::ORIG_MainTask, (void *)brr289::MainTask, "289 MainTask", 289);
		register_port(brr289::ORIG_SpriteTask, (void *)brr289::SpriteTask, "289 SpriteTask", 289);
		register_port(brr289::ORIG_SplashTask, (void *)brr289::SplashTask, "289 SplashTask", 289);
		// 30 fps layer: see mag289_brrawghh_held.inc
		FX_HELD(register_mag289_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag289_brrawghh_held.inc"
#endif
