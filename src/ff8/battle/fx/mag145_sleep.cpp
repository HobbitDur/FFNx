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

// Effect 145: Sleep (spell, MAG_145_*).
//
// Structure (setup MAG_145_SLEEP 0x6185D0, file loader MAG_145_SLEEP_FL 0x6185B0; the setup keeps
// the cast context, attacker and first target, queues the root in 0x24AA828 (one 0x10-byte node,
// pool 0x24AA818) and the first director in the effect queue 0x24AAAC0 (0x64 nodes of 0x24 bytes,
// pool 0x24AAAD0), frees the 40 "Zzz" entries, plays camera animation 0xDD67B4 and uploads the
// texture):
//   Root (0x61AA80) - alternates the packet arena (0x24AB95C / 0x24B395C), runs the effect queue,
//     then draws and advances the "Zzz" sprites (0x61AAD0); ends when the queue is empty.
//   Director (0x6186A0), one per action, 61 ticks (nothing in the draw-only mode): at 0 the
//     target's effect anchor with the ground height and the height of its bone 0xF0, 1 the main
//     visual and the sound, every odd tick 3..0x1D one "Zzz" sprite around the target, 0x1A six
//     orbiting bubbles, 0x1E damage, 0x2D the next action's director.
//   Main visual (0x619100), 27 ticks: a bubble prim model (0xDD746C, scale 0x360) circling the
//     target on a shrinking radius, rising, spinning faster and faster, fading in over 8 ticks;
//     a copy flattened on the ground (fade from its height) and a trail of the 3 previous
//     positions (ring 0x24AB8F0, 3 x 12 bytes per action), each with its flattened copy; until
//     tick 0x1A it hides the target (entity flags |= 4) and redraws its battle model tinted by the
//     sleep colour within 700 units of the bubble (0x619770); throws 3 particles per tick 4..0x14.
//   Orbiting bubble (0x618BB0), 18 ticks: the same prim model (scale 0x180..0x1FF) thrown out and
//     up, bouncing on the ground (new random upward speed and spin), a trail of its 3 previous
//     positions (ring 0x24AA838, 3 x 12 bytes per action and bubble), fading out from tick 0xE;
//     one particle per tick 2..0x11.
//   Particle (0x619000) - flipbook 0xDD6FF0 drifting and decelerating (1/32), shrinking; 9 frames.
//   Zzz sprites (40 x 12 bytes at 0x24AA638: frame, scale, x, y, z, rotation) - flipbook 0xDD7114,
//     16 frames at a fixed place, drawn by the root.
//   Target model (0x619770 -> 0x619880 -> 0x619B70): the target's shadow, then every visible object
//     of its model through its bone matrices into the vertex buffer 0x1D98B3C, drawn as flat
//     textured triangles / quads; every visible face with a corner within the radius of the
//     bubble gets a Gouraud overlay (semi-transparent, colour = the sleep colour depth-cued by
//     1 - distance / radius per corner); the weapon (+0x78) through RenderGeometry.
// Every task draws first; in the draw-only mode (battle_to_update_flags & 0x201) it returns after
// drawing.
// Module globals: 0x24AA630..0x24BB960 (first target, Zzz sprites, bubble trails, queues and
// pools, cast context 0x24AB8E0, attacker, texture file, main trails, packet arenas, packet
// cursor 0x24BB95C).

#include "mag_common.h"

namespace ff8fx
{
namespace sleep145
{
	using namespace eng;
	using namespace magc;

	// raw memory access (the model renderer follows the listing offset by offset)
	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline uint32_t P(const void *p) { return (uint32_t)p; }
	inline uint32_t &H32(uint8_t *h, int off) { return *(uint32_t *)(h + off); }

	// --- module globals ---
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x24AAAC0; }
	inline CastContext *&Ctx() { return var<CastContext *>(0x24AB8E0); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x24BB95C); }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }
	static const uint32_t ZZZ = 0x24AA638, ZZZ_END = 0x24AA818;  // 40 x 12 bytes
	static const uint32_t BUBBLE_TRAILS = 0x24AA838;              // [action][bubble][3] x 12 bytes
	static const uint32_t MAIN_TRAILS = 0x24AB8F0;                // [action][3] x 12 bytes
	static const uint32_t ARENA_EVEN = 0x24AB95C, ARENA_ODD = 0x24B395C;

	static const uint32_t ORIG_Root = 0x61AA80;
	static const uint32_t ORIG_Director = 0x6186A0;
	static const uint32_t ORIG_Bubble = 0x618BB0;
	static const uint32_t ORIG_Particle = 0x619000;
	static const uint32_t ORIG_Main = 0x619100;
	static const uint32_t MODEL_Bubble = 0xDD746C;
	static const uint32_t SEQ_Particle = 0xDD6FF0, SEQ_Zzz = 0xDD7114;
	static const void *const SOUND_Sleep = (const void *)0xDD7904;

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// ---- engine functions (original addresses)
	inline void GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t mode, int16_t *out) { fn<void (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, mode, out); }
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	inline uint32_t DrawShadow(uint32_t entity, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	inline uint32_t RenderGeometry(uint32_t model, uint32_t hdr, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(0x5099D0)(model, hdr, ot, mode, cursor); }
	// software GTE: register files 0x1CA8A10 (data) / 0x1CA927C (control)
	inline void GteWriteData(uint32_t value, uint32_t reg) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45D7C0)(value, reg); }
	inline void GteReadData(void *dst, uint32_t reg) { fn<void (__cdecl *)(void *, uint32_t)>(0x45D7A0)(dst, reg); }
	inline void GteWriteCtrl(uint32_t value, uint32_t reg) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45D7F0)(value, reg); }
	inline void GteReadCtrl(void *dst, uint32_t reg) { fn<void (__cdecl *)(void *, uint32_t)>(0x45D7D0)(dst, reg); }
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteLoadV0u(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x45DF80)(v); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void GteReadSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteReadSXY2u(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E260)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); }     // someCameraWork_45DD60
	inline void GteSetFarColorB(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DDA0)(r, g, b); }    // pre_someCameraWork_45DD60 (calls 0x45DD60)
	inline void GteLoadRGBC(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(c); }        // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(c); }       // set_param_with_dword_1CA8A68
	inline void GteMVMVA_RotV0Tr2() { fn<void (__cdecl *)()>(0x460830)(); }
	inline void GteMVMVA_LightIR() { fn<void (__cdecl *)()>(0x460810)(); }
	inline void GteReadMAC123u(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E450)(dst); }
	inline void GteSetIR0FromWord(uint32_t p) { fn<void (__cdecl *)(uint32_t)>(0x64DF80)(p); }  // sub_64DF80: IR0 = *(int16 *)p
	inline void GteReadBK(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x64DFA0)(dst); }      // sub_64DFA0: control regs 13..15 -> 3 dwords
	inline void InsertPrim(uint32_t bucket, uint32_t pkt) { InsertPrimAutoDepth(bucket, (void *)pkt); }

	// ------------------------------------------------------------------
	// Node layouts (effect queue: pool of 0x64 nodes of 0x24 bytes)
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode { TaskNode hdr; uint16_t counter; uint16_t pad; };
	struct DirectorNode
	{
		TaskNode hdr;
		int16_t counter; // +0x0C
		int16_t action;  // +0x0E
		int16_t pos[4];  // +0x10 effect anchor x, ground height (entity +0x24), z, height of bone 0xF0
		uint8_t pad18[8];
		int16_t slot;    // +0x20 target slot
		int16_t pad22;
	};
	struct MainNode
	{
		TaskNode hdr;
		int16_t counter; // +0x0C
		int16_t action;  // +0x0E
		int16_t pos[3];  // +0x10 centre x, y (rising), z
		int16_t vy;      // +0x16 rise per tick (until tick 0x12)
		int16_t spin;    // +0x18 spin angle
		int16_t spin_v;  // +0x1A spin speed (grows by 1/16 per tick)
		int16_t orbit;   // +0x1C angle on the circle around the centre
		int16_t orbit_v; // +0x1E its speed
		int16_t radius;  // +0x20 circle radius (shrinks until tick 0x12)
		int16_t shrink;  // +0x22 radius step
	};
	struct BubbleNode
	{
		TaskNode hdr;
		int16_t counter; // +0x0C
		int16_t action;  // +0x0E
		int16_t pos[3];  // +0x10
		int16_t index;   // +0x16 bubble 0..5 (its trail)
		int16_t vx;      // +0x18 (decays 1/64)
		int16_t vy;      // +0x1A
		int16_t vz;      // +0x1C (decays 1/64)
		int16_t ay;      // +0x1E y acceleration (grows 1/32)
		int16_t scale;   // +0x20
		int16_t angle;   // +0x22 y rotation (random turn at every bounce)
	};
	struct ParticleNode
	{
		TaskNode hdr;
		int16_t frame;   // +0x0C
		int16_t delay;   // +0x0E ticks before the first draw
		int16_t pos[3];  // +0x10
		int16_t pad16;   // +0x16 (never read; see the spawners)
		int16_t vel[3];  // +0x18 (decay 1/32)
		int16_t pad1E;
		int16_t scale;   // +0x20 (-0x30 per tick)
		int16_t pad22;
	};
	// Zzz sprite (40 at 0x24AA638)
	struct Zzz { int16_t frame, scale, pos[3], rot; };
	// trail entry: a previous draw of a bubble
	struct Trail { uint32_t used; int16_t pos[3], angle; };
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10, "root node is 0x10 bytes");
	static_assert(sizeof(DirectorNode) == 0x24 && sizeof(MainNode) == 0x24 && sizeof(BubbleNode) == 0x24 && sizeof(ParticleNode) == 0x24, "Sleep nodes are 0x24 bytes");
	static_assert(sizeof(Zzz) == 12 && sizeof(Trail) == 12, "Zzz / trail entries are 12 bytes");

	inline Trail *MainTrail(int32_t action) { return (Trail *)(MAIN_TRAILS + (uint32_t)(action * 3 * 12)); }
	inline Trail *BubbleTrail(int32_t action, int32_t index) { return (Trail *)(BUBBLE_TRAILS + (uint32_t)((action * 6 + index) * 3 * 12)); }

	// x / 18 as compiled (0x38E38E39, sar 2)
	static int32_t Div18(int32_t x)
	{
		int32_t q = (int32_t)(((int64_t)x * 0x38E38E39) >> 32) >> 2;
		return q + (int32_t)((uint32_t)q >> 31);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag145_sleep_held.h"
#endif

namespace ff8fx
{
namespace sleep145
{
	// ==================================================================
	// Target model (the target hidden, redrawn tinted near the bubble)
	// Header (0xDC bytes on the scratch): +0x00 primitive list, +0x04 vertex buffer, +0x08..+0x0E
	// primitive counts, +0x10 / +0x14..+0x1A as RenderGeometry's, +0x1C code bits (entity +0x28),
	// +0x20 visible objects, +0x28..+0x2A depth cue, +0x48 NCLIP, +0x50 OTZ, +0x58 / +0x60 / +0x68 /
	// +0x70 corner copies (+6 of each: its overlay fade), +0x78 / +0x88 GTE results, +0xA8 bubble
	// centre (3 dwords), +0xB4 squared radius, +0xB8 sleep colour, +0xBC camera x entity matrix
	// ==================================================================

	// 0x619B70: the object's flat textured triangles (0x10-byte records) and quads (0x14-byte
	// records); a visible face with a corner within the radius gets a Gouraud overlay (G3 / G4)
	// one bucket closer: corner colour = far colour (the sleep colour) x (1 - d / radius), d the
	// corner's squared distance to the centre plus (face normal . offset) >> 8. The GTE light,
	// background and colour matrix registers hold the work values (the camera rotation, kept in the
	// colour matrix registers, is put back after the outer product).
	static uint32_t GlowPrims(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t pk = cursor;
		const uint32_t verts = U32(hdr, 4);
		uint32_t rec = U32(hdr, 0);
		int32_t w10, w4, w3c, w34;  // the listing's stack words (argument slots reused)
		uint32_t bucket = 0;
		for (uint32_t i = 0; i < U16(hdr, 8); i++, rec += 0x10)
		{
			U32(hdr, 0x58) = U32(verts + (U16(rec, 0) & 0xFFF) * 8, 0);
			U32(hdr, 0x5C) = U32(verts + (U16(rec, 0) & 0xFFF) * 8, 4);
			U32(hdr, 0x60) = U32(verts + (U16(rec, 2) & 0xFFF) * 8, 0);
			U32(hdr, 0x64) = U32(verts + (U16(rec, 2) & 0xFFF) * 8, 4);
			U32(hdr, 0x68) = U32(verts + (U16(rec, 4) & 0xFFF) * 8, 0);
			U32(hdr, 0x6C) = U32(verts + (U16(rec, 4) & 0xFFF) * 8, 4);
			GteLoadV012(hdr + 0x58, hdr + 0x60, hdr + 0x68);
			GteRTPT();
			U32(pk, 0) = 0x7000000;
			const uint32_t w = U32(rec, 0xC);
			U32(pk, 4) = (w & 0x2000000) | U32(hdr, 0x1C) | 0x24000000;
			U32(pk, 0xC) = U32(rec, 8);
			U32(pk, 0x14) = w & 0x1FFFFFF;
			GteNCLIP();
			GteReadMAC0(hdr + 0x48);
			if (S32(hdr, 0x48) <= 0) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3();
			S32(pk, 0x1C) = S32(rec, 4) >> 16;
			GteReadOTZ32(hdr + 0x50);
			bucket = ot + (uint32_t)shl32(S32(hdr, 0x50) >> (shift & 31), 2);
			InsertPrim(bucket, pk);
			// corner 0: offset -> IR (squared by SQR), light row 1 = x, y / z
			w10 = S16(hdr, 0x58) - S32(hdr, 0xA8);
			w3c = S16(hdr, 0x5A) - S32(hdr, 0xAC);
			w4 = S16(hdr, 0x5C);
			w34 = S32(hdr, 0xB0);
			GteWriteData((uint32_t)w10, 9);
			GteWriteData((uint32_t)w3c, 10);
			w4 = w4 - w34;
			GteWriteData((uint32_t)w4, 11);
			w3c = shl32(w3c, 16);
			w10 = w10 & 0xFFFF;
			GteSQR();
			w10 = w10 | w3c;
			w4 = w4 & 0xFFFF;
			GteWriteCtrl((uint32_t)w10, 8);
			GteWriteCtrl((uint32_t)w4, 9);
			// corner 1
			w10 = S16(hdr, 0x60) - S32(hdr, 0xA8);
			w3c = S16(hdr, 0x62) - S32(hdr, 0xAC);
			w4 = S16(hdr, 0x64);
			w34 = S32(hdr, 0xB0);
			GteWriteData((uint32_t)w10, 9);
			GteWriteData((uint32_t)w3c, 10);
			w4 = w4 - w34;
			GteWriteData((uint32_t)w4, 11);
			w3c = w3c & 0xFFFF;
			w4 = shl32(w4, 16) | w3c;
			GteReadCtrl(&w34, 9);
			w10 = shl32(w10, 16);
			w34 = w34 | w10;
			GteWriteCtrl((uint32_t)w34, 9);
			GteWriteCtrl((uint32_t)w4, 10);
			GteReadData(&w10, 0x19);
			GteReadData(&w4, 0x1A);
			GteReadData(&w3c, 0x1B);
			w10 = (int32_t)((uint32_t)w10 + ((uint32_t)w3c + (uint32_t)w4));
			GteSQR();
			GteWriteCtrl((uint32_t)w10, 0xD);
			// corner 2
			w10 = S16(hdr, 0x68) - S32(hdr, 0xA8);
			w3c = S16(hdr, 0x6A) - S32(hdr, 0xAC);
			w4 = S16(hdr, 0x6C);
			w34 = S32(hdr, 0xB0);
			GteWriteData((uint32_t)w10, 9);
			GteWriteData((uint32_t)w3c, 10);
			w4 = w4 - w34;
			GteWriteData((uint32_t)w4, 11);
			w3c = shl32(w3c, 16);
			w10 = (w10 & 0xFFFF) | w3c;
			GteWriteCtrl((uint32_t)w10, 0xB);
			GteWriteCtrl((uint32_t)w4, 0xC);
			GteReadData(&w10, 0x19);
			GteReadData(&w4, 0x1A);
			GteReadData(&w3c, 0x1B);
			w10 = (int32_t)((uint32_t)w10 + ((uint32_t)w3c + (uint32_t)w4));
			GteSQR();
			GteWriteCtrl((uint32_t)w10, 0xE);
			// face normal: outer product of the edges (rotation diagonal x IR)
			{
				int32_t c = S16(hdr, 0x60);
				int32_t a = S16(hdr, 0x68) - c;
				int32_t d = S16(hdr, 0x58);
				c -= d;
				GteWriteCtrl((uint32_t)a, 0);
				w3c = d;
				w10 = a;
				w4 = c;
				GteWriteData((uint32_t)c, 9);
			}
			GteReadData(&w10, 0x19);
			GteReadData(&w4, 0x1A);
			GteReadData(&w3c, 0x1B);
			w10 = (int32_t)((uint32_t)w10 + ((uint32_t)w3c + (uint32_t)w4));
			GteWriteCtrl((uint32_t)w10, 0xF);
			{
				int32_t c = S16(hdr, 0x62);
				int32_t a = S16(hdr, 0x6A) - c;
				int32_t d = S16(hdr, 0x5A);
				c -= d;
				GteWriteCtrl((uint32_t)a, 2);
				w3c = d;
				w10 = a;
				w4 = c;
				GteWriteData((uint32_t)c, 10);
			}
			{
				int32_t c = S16(hdr, 0x64);
				int32_t a = S16(hdr, 0x6C) - c;
				int32_t d = S16(hdr, 0x5C);
				c -= d;
				GteWriteCtrl((uint32_t)a, 4);
				w3c = d;
				w10 = a;
				w4 = c;
				GteWriteData((uint32_t)c, 11);
			}
			GteOP();
			GteReadData(&w10, 9);
			GteMVMVA_LightIR();
			pk += 0x20;
			uint32_t mask = 0;
			GteReadMAC123u(hdr + 0x78);
			GteReadBK(hdr + 0x88);
			const int32_t r2 = S32(hdr, 0xB4);
			int32_t v = (S32(hdr, 0x78) >> 8) + S32(hdr, 0x88);
			if (v < r2)
			{
				mask = 1;
				S16(hdr, 0x5E) = (int16_t)(0x1000 - shl32(v, 12) / r2);
			}
			v = (S32(hdr, 0x7C) >> 8) + S32(hdr, 0x8C);
			if (v < r2)
			{
				mask |= 2;
				S16(hdr, 0x66) = (int16_t)(0x1000 - shl32(v, 12) / r2);
			}
			v = (S32(hdr, 0x80) >> 8) + S32(hdr, 0x90);
			if (v < r2)
			{
				mask |= 4;
				S16(hdr, 0x6E) = (int16_t)(0x1000 - shl32(v, 12) / r2);
			}
			uint32_t c0, c1, c2;
			GteReadCtrl(&c0, 0x10);
			GteReadCtrl(&c1, 0x11);
			GteReadCtrl(&c2, 0x12);
			GteWriteCtrl(c0, 0);
			GteWriteCtrl(c1, 2);
			GteWriteCtrl(c2, 4);
			if (!mask) continue;
			U32(pk, 4) = 0x32000000;
			U32(pk, 0) = 0x6000000;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadRGBC(pk + 4);
			GteSetIR0FromWord(hdr + 0x5E);
			GteDPCS();
			GteStoreRGB2(pk + 4);
			GteSetIR0FromWord(hdr + 0x66);
			GteDPCS();
			GteStoreRGB2(pk + 0xC);
			GteSetIR0FromWord(hdr + 0x6E);
			GteDPCS();
			GteStoreRGB2(pk + 0x14);
			InsertPrim(bucket - 4, pk);
			pk += 0x1C;
		}
		const uint32_t quads = rec + 4;
		for (uint32_t i = 0; i < U16(hdr, 0xA); i++)
		{
			const uint32_t q = quads + i * 0x14;
			U32(hdr, 0x58) = U32(verts + (U16(q, -4) & 0xFFF) * 8, 0);
			U32(hdr, 0x5C) = U32(verts + (U16(q, -4) & 0xFFF) * 8, 4);
			U32(hdr, 0x60) = U32(verts + (U16(q, -2) & 0xFFF) * 8, 0);
			U32(hdr, 0x64) = U32(verts + (U16(q, -2) & 0xFFF) * 8, 4);
			U32(hdr, 0x68) = U32(verts + (U16(q, 0) & 0xFFF) * 8, 0);
			U32(hdr, 0x6C) = U32(verts + (U16(q, 0) & 0xFFF) * 8, 4);
			GteLoadV012(hdr + 0x58, hdr + 0x60, hdr + 0x68);
			GteRTPT();
			U32(pk, 0) = 0x9000000;
			U32(hdr, 0x70) = U32(verts + (U16(q, 2) & 0xFFF) * 8, 0);
			U32(hdr, 0x74) = U32(verts + (U16(q, 2) & 0xFFF) * 8, 4);
			const uint32_t w = U32(q, 8);
			U32(pk, 0xC) = U32(q, 4);
			U32(pk, 0x14) = w & 0x1FFFFFF;
			U32(pk, 4) = (w & 0x2000000) | U32(hdr, 0x1C) | 0x2C000000;
			GteNCLIP();
			GteReadMAC0(hdr + 0x48);
			if (S32(hdr, 0x48) <= 0) continue;
			const uint32_t xy0 = pk + 8;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0u(hdr + 0x70);
			GteRTPS();
			U32(pk, 0x1C) = U32(q, 0xC);
			S32(pk, 0x24) = S32(q, 0xC) >> 16;
			GteReadSXY2u(pk + 0x20);
			GteAVSZ4();
			GteReadOTZ32(hdr + 0x50);
			bucket = ot + (uint32_t)shl32(S32(hdr, 0x50) >> (shift & 31), 2);
			InsertPrim(bucket, pk);
			// corner 0
			w10 = S16(hdr, 0x58) - S32(hdr, 0xA8);
			w3c = S16(hdr, 0x5A) - S32(hdr, 0xAC);
			w4 = S16(hdr, 0x5C);
			w34 = S32(hdr, 0xB0);
			GteWriteData((uint32_t)w10, 9);
			GteWriteData((uint32_t)w3c, 10);
			w4 = w4 - w34;
			GteWriteData((uint32_t)w4, 11);
			w3c = shl32(w3c, 16);
			w10 = w10 & 0xFFFF;
			GteSQR();
			w10 = w10 | w3c;
			w4 = w4 & 0xFFFF;
			GteWriteCtrl((uint32_t)w10, 8);
			GteWriteCtrl((uint32_t)w4, 9);
			// corner 1
			w10 = S16(hdr, 0x60) - S32(hdr, 0xA8);
			w3c = S16(hdr, 0x62) - S32(hdr, 0xAC);
			w4 = S16(hdr, 0x64);
			w34 = S32(hdr, 0xB0);
			GteWriteData((uint32_t)w10, 9);
			GteWriteData((uint32_t)w3c, 10);
			w4 = w4 - w34;
			GteWriteData((uint32_t)w4, 11);
			w3c = w3c & 0xFFFF;
			w4 = shl32(w4, 16) | w3c;
			GteReadCtrl(&w34, 9);
			w10 = shl32(w10, 16);
			w34 = w34 | w10;
			GteWriteCtrl((uint32_t)w34, 9);
			GteWriteCtrl((uint32_t)w4, 10);
			GteReadData(&w10, 0x19);
			GteReadData(&w4, 0x1A);
			GteReadData(&w3c, 0x1B);
			w10 = (int32_t)((uint32_t)w10 + ((uint32_t)w3c + (uint32_t)w4));
			GteSQR();
			GteWriteCtrl((uint32_t)w10, 0xD);
			// corner 2
			w10 = S16(hdr, 0x68) - S32(hdr, 0xA8);
			w3c = S16(hdr, 0x6A) - S32(hdr, 0xAC);
			w4 = S16(hdr, 0x6C);
			w34 = S32(hdr, 0xB0);
			GteWriteData((uint32_t)w10, 9);
			GteWriteData((uint32_t)w3c, 10);
			w4 = w4 - w34;
			GteWriteData((uint32_t)w4, 11);
			w3c = shl32(w3c, 16);
			w10 = (w10 & 0xFFFF) | w3c;
			GteWriteCtrl((uint32_t)w10, 0xB);
			GteWriteCtrl((uint32_t)w4, 0xC);
			GteReadData(&w10, 0x19);
			GteReadData(&w4, 0x1A);
			GteReadData(&w3c, 0x1B);
			w10 = (int32_t)((uint32_t)w10 + ((uint32_t)w3c + (uint32_t)w4));
			GteSQR();
			GteWriteCtrl((uint32_t)w10, 0xE);
			// face normal (outer product of the edges of corners 0..2)
			{
				int32_t c = S16(hdr, 0x60);
				int32_t a = S16(hdr, 0x68) - c;
				int32_t d = S16(hdr, 0x58);
				c -= d;
				GteWriteCtrl((uint32_t)a, 0);
				w3c = d;
				w10 = a;
				w4 = c;
				GteWriteData((uint32_t)c, 9);
			}
			GteReadData(&w10, 0x19);
			GteReadData(&w4, 0x1A);
			GteReadData(&w3c, 0x1B);
			w10 = (int32_t)((uint32_t)w10 + ((uint32_t)w3c + (uint32_t)w4));
			GteWriteCtrl((uint32_t)w10, 0xF);
			{
				int32_t c = S16(hdr, 0x62);
				int32_t a = S16(hdr, 0x6A) - c;
				int32_t d = S16(hdr, 0x5A);
				c -= d;
				GteWriteCtrl((uint32_t)a, 2);
				w3c = d;
				w10 = a;
				w4 = c;
				GteWriteData((uint32_t)c, 10);
			}
			{
				int32_t c = S16(hdr, 0x64);
				int32_t a = S16(hdr, 0x6C) - c;
				int32_t d = S16(hdr, 0x5C);
				c -= d;
				GteWriteCtrl((uint32_t)a, 4);
				w3c = d;
				w10 = a;
				w4 = c;
				GteWriteData((uint32_t)c, 11);
			}
			GteOP();
			GteReadData(&w10, 9);
			GteReadData(&w4, 10);
			GteReadData(&w3c, 11);
			// the normal replaces corner 0's copy; corner 3 is done on the CPU
			S16(hdr, 0x58) = (int16_t)w10;
			S16(hdr, 0x5A) = (int16_t)w4;
			S16(hdr, 0x5C) = (int16_t)w3c;
			GteMVMVA_LightIR();
			S16(hdr, 0x74) = (int16_t)(S16(hdr, 0x74) - (int16_t)U32(hdr, 0xB0));
			S16(hdr, 0x70) = (int16_t)(S16(hdr, 0x70) - (int16_t)U32(hdr, 0xA8));
			S16(hdr, 0x72) = (int16_t)(S16(hdr, 0x72) - (int16_t)U32(hdr, 0xAC));
			uint32_t mask = 0;
			GteReadMAC123u(hdr + 0x78);
			GteReadBK(hdr + 0x88);
			const int32_t r2 = S32(hdr, 0xB4);
			int32_t v = (S32(hdr, 0x78) >> 8) + S32(hdr, 0x88);
			if (v < r2)
			{
				mask = 1;
				S16(hdr, 0x5E) = (int16_t)(0x1000 - shl32(v, 12) / r2);
			}
			v = (S32(hdr, 0x7C) >> 8) + S32(hdr, 0x8C);
			if (v < r2)
			{
				mask |= 2;
				S16(hdr, 0x66) = (int16_t)(0x1000 - shl32(v, 12) / r2);
			}
			v = (S32(hdr, 0x80) >> 8) + S32(hdr, 0x90);
			if (v < r2)
			{
				mask |= 4;
				S16(hdr, 0x6E) = (int16_t)(0x1000 - shl32(v, 12) / r2);
			}
			{
				uint32_t d = (uint32_t)mul32(S16(hdr, 0x5A), S16(hdr, 0x72));
				d += (uint32_t)mul32(S16(hdr, 0x5C), S16(hdr, 0x74));
				d += (uint32_t)mul32(S16(hdr, 0x58), S16(hdr, 0x70));
				v = (int32_t)d >> 8;
				v = (int32_t)((uint32_t)v + (uint32_t)mul32(S16(hdr, 0x70), S16(hdr, 0x70)));
				v = (int32_t)((uint32_t)v + (uint32_t)mul32(S16(hdr, 0x72), S16(hdr, 0x72)));
				v = (int32_t)((uint32_t)v + (uint32_t)mul32(S16(hdr, 0x74), S16(hdr, 0x74)));
				if (v < r2)
				{
					mask |= 8;
					S16(hdr, 0x76) = (int16_t)(0x1000 - shl32(v, 12) / r2);
				}
			}
			uint32_t c0, c1, c2;
			GteReadCtrl(&c0, 0x10);
			GteReadCtrl(&c1, 0x11);
			GteReadCtrl(&c2, 0x12);
			GteWriteCtrl(c0, 0);
			GteWriteCtrl(c1, 2);
			GteWriteCtrl(c2, 4);
			if (!mask)
			{
				pk += 0x28;
				continue;
			}
			const uint32_t xy = U32(xy0, 0);
			pk += 0x28;
			U32(pk, 8) = xy;
			U32(pk, 0) = 0x8000000;
			U32(pk, 4) = 0x3A000000;
			GteReadSXY012(pk + 0x10, pk + 0x18, pk + 0x20);
			GteLoadRGBC(pk + 4);
			GteSetIR0FromWord(hdr + 0x5E);
			GteDPCS();
			GteStoreRGB2(pk + 4);
			GteSetIR0FromWord(hdr + 0x66);
			GteDPCS();
			GteStoreRGB2(pk + 0xC);
			GteSetIR0FromWord(hdr + 0x6E);
			GteDPCS();
			GteStoreRGB2(pk + 0x14);
			GteSetIR0FromWord(hdr + 0x76);
			GteDPCS();
			GteStoreRGB2(pk + 0x1C);
			InsertPrim(bucket - 4, pk);
			pk += 0x24;
		}
		return pk;
	}

	// 0x619880: the camera rotation words go to the colour matrix registers (16..18), the far colour
	// = the header's sleep colour; every visible object: its vertices through their bone matrices
	// (world space) into the vertex buffer (+4), then its faces through the camera (GlowPrims)
	static uint32_t ModelObjects(uint32_t model, uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t cam0 = U32(0x1D97778, 0), cam8 = U32(0x1D97778, 8), cam10 = U32(0x1D97778, 0x10);
		const uint32_t bones = U32(model, 0) + 0x10;
		uint32_t table = U32(model, 4);
		const int32_t count = S32(table, 0);
		table += 4;
		GteWriteCtrl(cam0, 0x10);
		GteWriteCtrl(cam8, 0x11);
		GteWriteCtrl(cam10, 0x12);
		GteSetFarColor(U8(hdr, 0xB8), U8(hdr, 0xB9), U8(hdr, 0xBA));
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t obj = U32(model, 4) + U32(table, 0);
			table += 4;
			if (!(U32(hdr, 0x20) & (1u << (i & 31)))) continue;
			int32_t groups = S16(obj, 0);
			uint32_t out = U32(hdr, 4);
			obj += 2;
			if (groups > 0)
			{
				do
				{
					const int32_t bone = S16(obj, 0);
					obj += 2;
					const Mat4x3 *m = (const Mat4x3 *)(bones + (uint32_t)(bone * 0x30) + 0x10);
					GteSetRotMatrixCtrl(m);
					GteSetTransVectorCtrl(m);
					int32_t nv = S16(obj, 0);
					obj += 2;
					for (; nv != 0; nv--)
					{
						const uint32_t xy = (uint32_t)U16(obj, 0) | ((uint32_t)U16(obj, 2) << 16);
						const uint32_t z = U16(obj, 4);
						GteWriteData(xy, 0);
						GteWriteData(z, 1);
						obj += 6;
						out += 8;
						GteMVMVA_RotV0Tr2();
						int32_t a, b, c;
						GteReadData(&a, 9);
						GteReadData(&b, 10);
						GteReadData(&c, 11);
						U32(out, -8) = (uint32_t)shl32(b, 16) | ((uint32_t)a & 0xFFFF);
						U16(out, -4) = (uint16_t)c;
					}
				} while (--groups != 0);
			}
			obj = (obj + 3) & ~3u;
			U16(hdr, 8) = U16(obj, 0);
			U16(hdr, 0xA) = U16(obj, 2);
			U16(hdr, 0xC) = U16(obj, 4);
			U16(hdr, 0xE) = U16(obj, 6);
			U32(hdr, 0) = obj + 0xC;
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			cursor = GlowPrims(hdr, ot, shift, cursor);
		}
		return cursor;
	}

	// 0x619770: the entity's shadow, its model (ModelObjects), its weapon (+0x78) through
	// RenderGeometry; the bone matrices are rebuilt from the pose afterwards
	static void ModelRender(uint32_t hdr, uint32_t entity)
	{
		const uint32_t anim = entity + 0x60;
		// 30 fps layer: see mag145_sleep_held.inc
		FX_HELD(held_model_pose(entity);)
		if (!(U8(entity, 0) & 0x20))
			FrameCursor() = DrawShadow(entity, var<uint32_t>(0x1D8E04C) + 0x4064, 0x10, FrameCursor());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)(hdr + 0xBC));
		ComputeBonesWorldMatrices((void *)anim, (void *)(entity + 0x40));
		U32(hdr, 0x1C) = U32(entity, 0x28);
		U32(hdr, 0x20) = U32(entity, 0x7C);
		const uint8_t cue = U8(entity, 7);
		U8(hdr, 0x2A) = cue;
		U8(hdr, 0x29) = cue;
		U8(hdr, 0x28) = cue;
		U32(hdr, 4) = var<uint32_t>(0x1D98B3C);
		U32(hdr, 0x10) = var<uint32_t>(0x1D969A8);
		U16(hdr, 0x14) = 0;
		U16(hdr, 0x16) = 0;
		U16(hdr, 0x18) = 0x140;
		U16(hdr, 0x1A) = 0xD8;
		FrameCursor() = ModelObjects(U32(anim, 4), hdr, RenderOT44(), 2, FrameCursor());
		BuildBoneMatricesFromPose((void *)anim);
		const uint32_t weapon = U32(entity, 0x78);
		if (weapon && !(U8(entity, 1) & 2))
		{
			U32(hdr, 0x20) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices((void *)weapon, (void *)(hdr + 0xBC));
			FrameCursor() = RenderGeometry(U32(weapon, 4), hdr, RenderOT44(), 4, FrameCursor());
			BuildBoneMatricesFromPose((void *)weapon);
		}
	}

	// ==================================================================
	// Main visual (0x619100)
	// ==================================================================
	struct MainFrame
	{
		uint32_t slot;   // target slot
		int16_t pos[4];  // bubble centre as drawn, + the low word of its fade
		int32_t fade2;   // fade of the flattened copies
	};

	// y rotation angles (0, a, 0) -> matrix
	static void RotY(int16_t a, Mat4x3 *m)
	{
		int16_t ang[3] = { 0, a, 0 };
		BuildRotationMatrixFromAngles(ang, m);
	}

	// the bubble on its circle, its ground copy, its trail (3 previous draws, each with its ground
	// copy) and until tick 0x1A the target tinted by the sleep colour around the bubble
	static void MainDraw(const MainNode *n, const Trail *trail, MainFrame *f)
	{
		const int32_t c = n->counter;
		f->slot = Ctx()->actions[n->action].targets[0];
		Mat4x3 r = {}, m = {}, v = {};
		int16_t off[3] = { 0, 0, (int16_t)-n->radius };
		RotY(n->orbit, &r);
		MatrixMulVector(&r, off, off);
		RotY(n->spin, &m);
		m.t[0] = n->pos[0] + off[0];
		m.t[1] = n->pos[1];
		m.t[2] = n->pos[2] + off[2];
		int32_t s[3] = { 0x360, 0x360, 0x360 };
		Scale3DMatrix(&m, s);
		f->pos[0] = (int16_t)m.t[0];
		f->pos[1] = (int16_t)m.t[1];
		f->pos[2] = (int16_t)m.t[2];
		ComposeAffineTransform(&Camera(), &m, &v);
		GteSetRotMatrix(&v);
		GteSetTransVector(&v);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		H32(h, 0) = MODEL_Bubble;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		H32(h, 0xC) = 0;
		H32(h, 0x1C) = 0;
		if (c < 8)
		{
			int32_t fade = shl32(8 - c, 9);
			// 30 fps layer: see mag145_sleep_held.inc
			FX_HELD(fade = held_ramp(fade, c, -0x200, 0x1000);)
			H32(h, 0x1C) = 0xC0;
			H32(h, 0xC) = (uint32_t)fade;
		}
		f->pos[3] = (int16_t)H32(h, 0xC);
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		// the ground copy: y column and y translation zeroed
		m.m[2][1] = 0;
		m.m[1][1] = 0;
		m.m[0][1] = 0;
		m.t[1] = 0;
		ComposeAffineTransform(&Camera(), &m, &v);
		GteSetRotMatrix(&v);
		GteSetTransVector(&v);
		if (H32(h, 0xC) == 0)
		{
			// (height << 12) / -2000 as compiled (0xEF9DB22D, sar 7), sign-extended from 20 bits
			int32_t y = n->pos[1];
			if (y < -2000) y = -2000;
			int32_t q = (int32_t)(((int64_t)shl32(y, 12) * (int32_t)0xEF9DB22D) >> 32) >> 7;
			q += (int32_t)((uint32_t)q >> 31);
			H32(h, 0xC) = (uint32_t)(shl32(q, 12) >> 12);
		}
		H32(h, 0x1C) |= 0xC0;
		f->fade2 = (int32_t)H32(h, 0xC);
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		int32_t k = c - 1 < 0 ? 0 : (c - 1) % 3;
		for (int32_t fade = 0x580;;)
		{
			const Trail *e = &trail[k];
			if (!e->used) break;
			RotY(e->angle, &m);
			m.t[0] = e->pos[0];
			m.t[1] = e->pos[1];
			m.t[2] = e->pos[2];
			s[0] = s[1] = s[2] = 0x360;
			Scale3DMatrix(&m, s);
			ComposeAffineTransform(&Camera(), &m, &v);
			GteSetRotMatrix(&v);
			GteSetTransVector(&v);
			H32(h, 0xC) = (uint32_t)fade;
			PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
			m.m[2][1] = 0;
			m.m[1][1] = 0;
			m.m[0][1] = 0;
			m.t[1] = 0;
			ComposeAffineTransform(&Camera(), &m, &v);
			GteSetRotMatrix(&v);
			GteSetTransVector(&v);
			H32(h, 0xC) = (uint32_t)f->fade2;
			PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
			if (--k < 0) k = 2;
			fade += 0x200;
			if (fade >= 0xB80) break;
		}
		FieldFree(0x58);
		if (c < 0x1A)
		{
			uint8_t *entity = Entity(f->slot);
			entity[0] |= 4;
			const uint32_t hm = P(FieldAlloc(0xDC));
			S32(hm, 0xA8) = f->pos[0];
			S32(hm, 0xAC) = f->pos[1];
			S32(hm, 0xB0) = f->pos[2];
			S32(hm, 0xB4) = 0x77A10; // 700 squared
			U8(hm, 0xB8) = 0xA0;
			U8(hm, 0xB9) = 0xB0;
			U8(hm, 0xBA) = 8;
			GteSetFarColorB(0, 0, 0);
			GteLoadRGBC(hm + 0xB8);
			GteSetIR0(f->pos[3]);
			GteDPCS();
			GteStoreRGB2(hm + 0xB8);
			ModelRender(hm, P(entity));
			FieldFree(0xDC);
		}
	}

	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *p = (MainNode *)n;
		Trail *trail = MainTrail(p->action);
		// 30 fps layer: see mag145_sleep_held.inc
		FX_HELD(held_note_draw(ORIG_Main, p, trail);)
		MainFrame f;
		MainDraw(p, trail, &f);
		if (DrawOnly()) return 0;
		const int16_t c = p->counter;
		Trail *e = &trail[c % 3];
		e->used = 1;
		e->pos[0] = f.pos[0];
		e->pos[1] = f.pos[1];
		e->pos[2] = f.pos[2];
		e->angle = p->spin;
		if (c < 0x12)
		{
			p->pos[1] = (int16_t)(p->pos[1] + p->vy);
			p->radius = (int16_t)(p->radius - p->shrink);
		}
		const int16_t sv = p->spin_v;
		p->spin = (int16_t)(p->spin + sv);
		p->spin_v = (int16_t)(sv + (int16_t)(sv >> 4));
		p->orbit = (int16_t)(p->orbit + p->orbit_v);
		if (c >= 4 && c <= 0x14)
		{
			for (int k = 0; k < 3; k++)
			{
				ParticleNode *q = (ParticleNode *)AddTaskToQueue(EffectQueue(), ORIG_Particle);
				q->frame = 0;
				q->delay = (int16_t)(CrtRand() % 4 + k);
				memcpy(q->pos, f.pos, 8); // + the fade word into +0x16
				q->pos[0] = (int16_t)(q->pos[0] + (int16_t)(CrtRand() % 280 - 140));
				q->pos[1] = (int16_t)(q->pos[1] + (int16_t)(CrtRand() % 280 - 93));
				q->pos[2] = (int16_t)(q->pos[2] + (int16_t)(CrtRand() % 280 - 140));
				q->vel[0] = (int16_t)(CrtRand() % 100 - 50);
				q->vel[1] = (int16_t)(CrtRand() % 60 - 10);
				q->vel[2] = (int16_t)(CrtRand() % 100 - 50);
				q->scale = (int16_t)(CrtRand() % 0x700 + 0xA00);
			}
		}
		p->counter++;
		if (p->counter <= 0x1A) return 0;
		if (p->action >= 0) *(uint16_t *)Entity(f.slot) &= 0xFFFB;
		return TASK_END;
	}

	// ==================================================================
	// Orbiting bubble (0x618BB0)
	// ==================================================================

	// the bubble (fading out from tick 0xE) and its trail; `drawn` = its position as drawn
	static void BubbleDraw(const BubbleNode *n, const Trail *trail, int16_t drawn[3])
	{
		const int32_t c = n->counter;
		Mat4x3 m = {}, v = {};
		RotY(n->angle, &m);
		m.t[0] = n->pos[0];
		m.t[1] = n->pos[1];
		m.t[2] = n->pos[2];
		int32_t s[3] = { n->scale, n->scale, n->scale };
		Scale3DMatrix(&m, s);
		drawn[0] = (int16_t)m.t[0];
		drawn[1] = (int16_t)m.t[1];
		drawn[2] = (int16_t)m.t[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		H32(h, 0) = MODEL_Bubble;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		H32(h, 0x1C) = 0;
		if (c >= 0xE)
		{
			int32_t fade = shl32(c - 0xE, 10);
			// 30 fps layer: see mag145_sleep_held.inc
			FX_HELD(fade = held_ramp(fade, c, 0x400, -0x3800);)
			H32(h, 0x1C) = 0xC0;
			H32(h, 0xC) = (uint32_t)fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		H32(h, 0x1C) |= 0xC0;
		int32_t k = c - 1 < 0 ? 0 : (c - 1) % 3;
		for (int32_t fade = 0x580;;)
		{
			const Trail *e = &trail[k];
			if (!e->used) break;
			RotY(e->angle, &m);
			m.t[0] = e->pos[0];
			m.t[1] = e->pos[1];
			m.t[2] = e->pos[2];
			s[0] = s[1] = s[2] = n->scale;
			Scale3DMatrix(&m, s);
			ComposeAffineTransform(&Camera(), &m, &v);
			GteSetRotMatrix(&v);
			GteSetTransVector(&v);
			H32(h, 0xC) = (uint32_t)fade;
			PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
			if (--k < 0) k = 2;
			fade += 0x200;
			if (fade >= 0xB80) break;
		}
		FieldFree(0x58);
	}

	static uint32_t __cdecl BubbleTask(TaskNode *n)
	{
		BubbleNode *p = (BubbleNode *)n;
		Trail *trail = BubbleTrail(p->action, p->index);
		// 30 fps layer: see mag145_sleep_held.inc
		FX_HELD(held_note_draw(ORIG_Bubble, p, trail);)
		int16_t drawn[3];
		BubbleDraw(p, trail, drawn);
		if (DrawOnly()) return 0;
		const int16_t c = p->counter;
		Trail *e = &trail[c % 3];
		e->used = 1;
		e->pos[0] = drawn[0];
		e->pos[1] = drawn[1];
		e->pos[2] = drawn[2];
		e->angle = p->angle;
		int16_t v = p->vx;
		p->pos[0] = (int16_t)(p->pos[0] + v);
		p->vx = (int16_t)(v - (int16_t)(v >> 6));
		const int16_t vy = p->vy;
		p->pos[1] = (int16_t)(p->pos[1] + vy);
		const int16_t ay = p->ay;
		p->vy = (int16_t)(ay + vy);
		p->ay = (int16_t)(ay + (int16_t)(ay >> 5));
		if (p->pos[1] > 0)
		{
			// bounce: new upward speed, gravity and a random turn
			p->pos[1] = 0;
			p->vy = (int16_t)(-130 - CrtRand() % 80);
			p->ay = (int16_t)(CrtRand() % 60 + 50);
			p->angle = (int16_t)(p->angle + (int16_t)(CrtRand() % 2048));
		}
		v = p->vz;
		p->pos[2] = (int16_t)(p->pos[2] + v);
		p->vz = (int16_t)(v - (int16_t)(v >> 6));
		if (c >= 2 && c <= 0x12)
		{
			ParticleNode *q = (ParticleNode *)AddTaskToQueue(EffectQueue(), ORIG_Particle);
			q->frame = 0;
			q->delay = (int16_t)(CrtRand() % 3);
			q->pos[0] = drawn[0];
			q->pos[1] = drawn[1];
			q->pos[2] = drawn[2];
			// UNINIT 0x618E34 / 0x618F3F: the original copies its position as two dwords from a stack
			// SVECTOR whose pad word ([esp+0x1E], i.e. entry esp - 0x52) it never writes: that pad lands
			// in the particle's +0x16 (never read; the trail entry's copy is overwritten by the angle).
			// It holds what older frames left there: 0 in every harness run, taken as 0 here.
			q->pad16 = 0;
			q->pos[0] = (int16_t)(q->pos[0] + (int16_t)(CrtRand() % 280 - 140));
			q->pos[1] = (int16_t)(q->pos[1] + (int16_t)(CrtRand() % 280 - 93));
			q->pos[2] = (int16_t)(q->pos[2] + (int16_t)(CrtRand() % 280 - 140));
			q->vel[0] = (int16_t)(CrtRand() % 100 - 50);
			q->vel[1] = (int16_t)(CrtRand() % 60 - 10);
			q->vel[2] = (int16_t)(CrtRand() % 100 - 50);
			q->scale = (int16_t)(CrtRand() % 0x600 + 0xA00);
		}
		p->counter++;
		return p->counter >= 0x12 ? TASK_END : 0;
	}

	// ==================================================================
	// Particle (0x619000): flipbook frame `frame`
	// ==================================================================
	static void ParticleDraw(const ParticleNode *p)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 3));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint16_t *)(h + 4) = (uint16_t)p->frame;
		H32(h, 0) = SEQ_Particle;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// drift: decelerate by 1/32, shrink by 0x30
	static void ParticleUpdate(ParticleNode *p)
	{
		p->scale = (int16_t)(p->scale - 0x30);
		for (int k = 0; k < 3; k++)
		{
			const int16_t v = p->vel[k];
			p->pos[k] = (int16_t)(p->pos[k] + v);
			p->vel[k] = (int16_t)(v - (int16_t)(v >> 5));
		}
		p->frame++;
	}

	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag145_sleep_held.inc
		FX_HELD(held_note_draw(ORIG_Particle, p, nullptr);)
		ParticleDraw(p);
		if (DrawOnly()) return 0;
		ParticleUpdate(p);
		return p->frame >= 9 ? TASK_END : 0;
	}

	// ==================================================================
	// Zzz sprites (0x61AAD0, run by the root after the effect queue)
	// ==================================================================
	static void ZzzDraw(uint8_t *h, const Zzz *z)
	{
		*(uint16_t *)(h + 4) = (uint16_t)z->frame;
		H32(h, 8) = (uint32_t)shl32(z->rot, 2);
		TransformCameraByShadowRotation(z->pos, z->scale, -(z->scale >> 3));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
	}

	static uint8_t *ZzzBegin()
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		H32(h, 0) = SEQ_Zzz;
		*(uint16_t *)(h + 0x24) = 1;
		return h;
	}

	static void ZzzTick()
	{
		// 30 fps layer: see mag145_sleep_held.inc
		FX_HELD(held_note_zzz();)
		uint8_t *h = ZzzBegin();
		for (Zzz *z = (Zzz *)ZZZ; P(z) < ZZZ_END; z++)
		{
			if (z->frame < 0) continue;
			ZzzDraw(h, z);
			if (DrawOnly()) continue;
			z->frame++;
			if (z->frame >= 0x10) z->frame = -1;
		}
		FieldFree(0xB4);
	}

	// ==================================================================
	// Root task (0x61AA80): node +0x0C counter
	// ==================================================================
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag145_sleep_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = ARENA_ODD;
		if (!(r->counter & 1)) PacketCursor() = ARENA_EVEN;
		int alive = ExecuteTaskQueue(EffectQueue());
		ZzzTick();
		r->counter++;
		return alive ? 0 : TASK_END;
	}

	// ==================================================================
	// Director (0x6186A0): one per action (node +0x0C counter, +0x0E action, +0x20 target slot)
	// ==================================================================
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		if (DrawOnly()) return 0;
		DirectorNode *d = (DirectorNode *)n;
		if (d->counter == 0)
		{
			uint8_t *entity = Entity(d->slot);
			GetDefaultEffectPosition(entity, d->pos);
			d->pos[1] = EntityHeight(d->slot);
			int16_t top[3];
			GetEffectSpawnPosition(entity, 0xF0, 0, top);
			d->pos[3] = top[1];
		}
		if (d->counter == 1)
		{
			int32_t spread = (EntitySize(d->slot) * 5000) >> 12;
			if (spread > 0x578) spread = 0x578;
			MainNode *m = (MainNode *)AddTaskToQueue(EffectQueue(), ORIG_Main);
			memcpy(m->pos, d->pos, 8);
			m->pos[1] = (int16_t)(m->pos[1] - 200);
			m->action = d->action;
			m->counter = 0;
			m->vy = (int16_t)Div18(d->pos[3] - m->pos[1] - 500);
			m->spin = (int16_t)(CrtRand() % 4096);
			m->spin_v = (int16_t)(CrtRand() % 50 + 70);
			m->orbit = (int16_t)(CrtRand() % 4096);
			const int32_t r = CrtRand() % 100;
			m->radius = (int16_t)spread;
			m->orbit_v = (int16_t)(r + 400);
			m->shrink = (int16_t)Div18(spread);
			Trail *t = MainTrail(d->action);
			for (int k = 0; k < 3; k++) t[k].used = 0;
		}
		if (d->counter == 0x1A)
		{
			// six bubbles thrown out around a random direction, 682 (1/6 turn) apart
			int16_t out0[3] = { 0, 0, (int16_t)0xF000 };
			const int32_t r = CrtRand() % 4096;
			for (int32_t k = 0; k < 6; k++)
			{
				int16_t ang[3] = { 0, (int16_t)(r + k * 682), 0 };
				Mat4x3 m = {};
				int16_t dir[3];
				BuildRotationMatrixFromAngles(ang, &m);
				MatrixMulVector(&m, out0, dir);
				BubbleNode *b = (BubbleNode *)AddTaskToQueue(EffectQueue(), ORIG_Bubble);
				b->action = d->action;
				b->counter = 0;
				b->pos[0] = d->pos[0];
				b->pos[1] = (int16_t)(d->pos[3] - 500);
				b->pos[2] = d->pos[2];
				b->index = (int16_t)k;
				const int32_t speed = CrtRand() % 30 + 100;
				b->vx = (int16_t)((dir[0] * speed) >> 12);
				b->vy = (int16_t)(-130 - CrtRand() % 40);
				b->vz = (int16_t)((dir[2] * speed) >> 12);
				b->ay = (int16_t)(CrtRand() % 60 + 50);
				b->scale = (int16_t)(CrtRand() % 128 + 0x180);
				b->angle = (int16_t)(CrtRand() % 4096);
				Trail *t = BubbleTrail(d->action, k);
				for (int j = 0; j < 3; j++) t[j].used = 0;
			}
		}
		const int16_t c = d->counter;
		if (c >= 3 && c <= 0x1E && (c & 1))
		{
			// one Zzz sprite in a free entry, within 700 of the target's anchor
			int16_t pos[4];
			GetDefaultEffectPosition(Entity(d->slot), pos);
			for (int spawned = 0; spawned < 1; spawned++)
			{
				int idx = 0;
				Zzz *z = (Zzz *)ZZZ;
				while (z->frame >= 0)
				{
					z++;
					idx++;
					if (P(z) >= ZZZ_END) break;
				}
				if (P(z) >= ZZZ_END || idx >= 0x28) break;
				z->frame = 0;
				z->scale = (int16_t)(CrtRand() % 0x700 + 0xA00);
				z->pos[0] = (int16_t)(CrtRand() % 0x578 + pos[0] - 0x2BC);
				z->pos[1] = (int16_t)(CrtRand() % 0x578 + pos[1] - 0x2BC);
				z->pos[2] = (int16_t)(CrtRand() % 0x578 + pos[2] - 0x2BC);
				z->rot = (int16_t)((CrtRand() % 4) << 8);
			}
		}
		if (d->counter == 0x1E) ApplyActionResultToTarget(Ctx()->actions[d->action].targets);
		if (d->counter == 0x2D)
		{
			const int32_t next = d->action + 1;
			if (next <= Ctx()->actions[0].last_action)
			{
				DirectorNode *t = (DirectorNode *)AddTaskToQueue(EffectQueue(), ORIG_Director);
				t->counter = 0;
				t->action = (int16_t)next;
				t->slot = (int16_t)(uint16_t)Ctx()->actions[next].targets[0];
			}
		}
		if (d->counter == 1)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(Entity(d->slot), pos);
			BdPlaySE3D(SOUND_Sleep, 0, pos);
		}
		d->counter++;
		return d->counter > 0x3C ? TASK_END : 0;
	}
}

	void register_mag145_sleep()
	{
		register_port(sleep145::ORIG_Root, (void *)sleep145::RootTask, "S145 RootTask", 145);
		register_port(sleep145::ORIG_Director, (void *)sleep145::DirectorTask, "S145 DirectorTask", 145);
		register_port(sleep145::ORIG_Main, (void *)sleep145::MainTask, "S145 MainTask", 145);
		register_port(sleep145::ORIG_Bubble, (void *)sleep145::BubbleTask, "S145 BubbleTask", 145);
		register_port(sleep145::ORIG_Particle, (void *)sleep145::ParticleTask, "S145 ParticleTask", 145);
		// 30 fps layer: see mag145_sleep_held.inc
		FX_HELD(register_mag145_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag145_sleep_held.inc"
#endif
