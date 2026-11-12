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

// Effect 316: the caster's model swept by a light (enemy attack 203 of kernel.bin, no name; Red Giant c0m130; no other
// kernel.bin entry uses effect 316; MAG_316_*).
//
// Structure (setup 0x6610B0 -> _Init 0x6610E0, file loader 0x6610C0 = mag315.tim (name at 0x1016084) into 0x2505520; _Init
// queues that TIM and the exe's TIM 0x1005E64, keeps the context (0x2505524) and the CASTER's entity (0x2505528), clears the
// caster's entity flags 0x0C, sets up the root queue 0x2505540 (pool 0x2505550, 1 x 0x14), hides the three party entities'
// normal draw (flag 4) and starts camera animation 0x1005C50):
//   RootTask (0x661190) - alternates the module packet arena (magic buffer + 0x881C / + 0x1881C); on counter 2 (once): music
//     volume fade 0x47E3C0(0xF0), the main pool (1 x 0x81C at the magic buffer), the main task (voice slot 0x1005D54, the
//     caster's colour word +0x2C kept, particle cursor = first slot) and every polygon of the caster's model shown again
//     (0x6612C0 -> 0x6612E0 -> 064's 0x6E5D20). Then each tick: the main queue, the screen flash level (0x5713E0); ends
//     when the main queue is empty.
//   MainTask (0x661350) - counters 0..0x137: screen flash ramps 0..7 / 0x131..0x137 (125 a step), 1000 between;
//     counters 0x14..0x95: the caster drawn by sweep A (0x661550: shadow, then its model, every vertex's grey level from its
//     y against a level falling from 0x200 by 40 a tick to -0xDA0 at 0x78); counters 0x96..0x12B: sweep B (0x661CC0: the
//     level rising from -0xDA0 back to 0x200 over 150 ticks; every lit polygon may throw a particle, module LCG
//     0x2505568); both set the caster's flag 4 (the module draws it); counters 0x96..0x121: the caster's colour word +0x2C
//     lerped from the kept word toward 0x2505570 (0, never written) and its byte +0x2F = 0x32; from 0x3C the caster's flag 8;
//     every tick the particles (0x662720: 128 slots, draw THEN move by 4 x velocity, life down by 1); sound 0x1005D50 at
//     0x2D, voice slot released at 0x134; at 0x138: flag 8 cleared, the colour word put back, the party's flag 4 cleared, end.
// Sweep draws: the caster's world bone matrices (0x5095B0), its model projected object by object through its bone matrices
// (A: 8-byte vertices in the engine scratch [0x1D98B3C], 0x663760 (314's code); B: 16-byte vertices with the bone-space
// position at magic buffer + 0x81C, 0x662630), each vertex's grey g = clamp((y - level) / 4 + 0x40, 0, 0x80); a facing
// polygon whose vertices are all g = 0: textured flat (frame arena); all 0x80: textured flat, texture page / CLUT words
// moved by 0xB / 0x414 (A only for g = 0, B draws nothing then); otherwise a gouraud copy in the frame arena (A: opaque, the
// grey; B: semi-transparent, moved page / CLUT) plus a module-arena copy (A: semi-transparent textured, 0x80 - grey; B: a
// semi-transparent untextured gouraud plus a draw-mode packet 0xE10002C0); then the bone matrices from the pose (0x508C90).
// Particles: sequence 0x1005AE4 (frame = frame count - life), drawn by 318's sprite renderer 0x65F4B0 into OT slot 2.
// Draw-only flags (battle_to_update_flags 0x201) are not tested: everything updates always.
// ENTITY WRITES: the caster - flags word +0x00 (bit 2 while drawn, bit 3 from 0x3C to the end), colour word +0x2C / byte
// +0x2F, its world / pose bone matrices every drawn tick; the party's flags word bit 2 (setup / end).
// CAMERA: the setup's camera animation 0x1005C50 (engine); no camera word is written by the tasks.
// Module globals: 0x2505520..0x2505578 (texture file 0x2505520, context 0x2505524, caster 0x2505528, main queue 0x2505530,
// root queue 0x2505540, root pool 0x2505550, packet cursor 0x2505564, module LCG 0x2505568, flash level 0x250556C, colour
// lerp target 0x2505570, magic buffer base 0x2505574).
// CODE OF OTHER MODULES: 0x663760 (sweep A's vertex projection) lies in Abadon 314's range, 0x65F4B0 (the sprite renderer,
// no IDB start) in Krysta 318's: both ported here. The polygon / vertex draws 0x661720 / 0x661EB0 and 0x662630 have no IDB start.
// NEVER-WRITTEN STACK WORDS: none read. Sweep A's vertex entries keep the scratch's bytes +6 / +7 when the vertex is behind
// the camera (only the depth word is cleared), B's keep +7 always and +6 then: the polygon tests read them (as vanilla).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_model_sweep.h"

namespace ff8fx
{
namespace redgiant316
{
	using namespace eng;
	using namespace magc;
	using namespace msweep;

	// --- module globals ---
	inline uint32_t &TexFile() { return var<uint32_t>(0x2505520); }        // the effect's file (its TIM)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2505524); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2505528); }        // caster entity (setup)
	inline TaskQueue *QMain() { return (TaskQueue *)0x2505530; }           // pool: magic buffer, 1 x 0x81C
	inline TaskQueue *QRoot() { return (TaskQueue *)0x2505540; }           // pool 0x2505550, 1 x 0x14
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2505564); }
	inline uint32_t &Lcg() { return var<uint32_t>(0x2505568); }
	inline uint32_t &Flash() { return var<uint32_t>(0x250556C); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2505574); }        // Magic_TextureOFF (magic buffer)

	static const uint32_t ORIG_Setup = 0x6610B0, ORIG_FileLoader = 0x6610C0;
	static const uint32_t ORIG_RootTask = 0x661190;
	static const uint32_t ORIG_MainTask = 0x661350;
	static const uint32_t TEXTURE_NAME = 0x1016084;    // "mag315.tim"
	static const uint32_t TIM_Exe = 0x1005E64;          // the exe's own TIM
	static const uint32_t CAMERA = 0x1005C50;
	static const uint32_t SEQ_Particle = 0x1005AE4;     // sprite sequence (frame table at + 8)
	static const uint32_t COLOUR_Target = 0x2505570;    // colour lerp target (never written: 0)
	static const void *const SOUND_Voice = (const void *)0x1005D54;
	static const void *const SOUND_Start = (const void *)0x1005D50;
	static const uint32_t VERTS_B = 0x81C;              // magic buffer: sweep B's vertices
	static const uint32_t ARENA_A = 0x881C, ARENA_B = 0x1881C;
	static const int16_t PAGE_MOVE = 0xB, CLUT_MOVE = 0x414;

	// engine functions not in fx_port.h / mag_common.h / fx_model_sweep.h
	inline void FillDwords(void *dst, uint32_t value, uint32_t dwords) { fn<void (__cdecl *)(void *, uint32_t, uint32_t)>(0x6DA380)(dst, value, dwords); } // Effect_FillDwords
	inline uint32_t MagicTextureOff() { return fn<uint32_t (__cdecl *)()>(0x571B50)(); }                                // Magic_TextureOFF_ToEAX1

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2505550)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F main pool set up, main task started
		uint32_t parity;   // +0x10 packet arena parity
	};
	struct Particle // 16 bytes
	{
		int16_t pos[3];    // +0x00
		int16_t life;      // +0x06 ticks left (0 = free); frame = frame count - life
		int16_t size;      // +0x08 sprite scale (4096 = as drawn)
		int16_t pad0A;
		int8_t vel[3];     // +0x0C x 4 a tick
		int8_t pad0F;
	};
	struct MainNode // pool of 1 node of 0x81C bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint32_t voice;    // +0x10 voice slot (0x4A29A0)
		uint32_t colour;   // +0x14 the caster's colour word +0x2C at the start
		Particle parts[0x80]; // +0x18
		Particle *next;    // +0x818 slot the next particle tries first
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(Particle) == 0x10 && sizeof(MainNode) == 0x81C, "316 nodes");

	// Sweep draw headers (Field_Alloc blocks; the polygon / vertex draws get block + 0x20):
	//   A (0x84 bytes, h = block + 0x20): +0 polygon cursor, +4 vertex buffer, +8 / +0xA triangle / quad counts,
	//     +0xC..+0x12 0, 0, 0x140, 0xD8, +0x14 model colour (entity +0x28), +0x18 object mask (entity +0x7C), +0x1C..+0x3B
	//     the polygon's vertices (8 bytes each), +0x3C NCLIP, +0x44 OTZ, +0x4C level, +0x4E / +0x50 page / CLUT moves,
	//     +0x54 vertex data cursor, +0x58 vertex output cursor, +0x5C frame arena cursor, +0x60 module packet cursor.
	//   B (0xB0 bytes, h = block + 0x20): as A to +0x18, +0x1C..+0x5B the polygon's vertices (16 bytes each), +0x5C NCLIP,
	//     +0x64 OTZ, +0x6C level, +0x6E / +0x70 page / CLUT moves, +0x74 / +0x78 vertex cursors, +0x7C particle position,
	//     +0x84 the main node, +0x88 frame arena cursor, +0x8C module packet cursor.
	// Particle draw header (0xB4 bytes): +0 sequence, +0x2C frame record, +0x30 its sprite count, +0x40 GTE FLAG,
	//   +0x44..+0x63 the quad's corners, +0x64 sprite matrix, +0x78 sprite offset, +0x84 scale matrix (+0x98 its position).
}
}

#ifdef FF8_FX_HELD
#include "mag316_red_giant_held.h"
#endif

namespace ff8fx
{
namespace redgiant316
{
	using namespace eng;
	using namespace magc;
	using namespace msweep;

	// ------------------------------------------------------------------
	// Module LCG (0x662570): seed = (seed * 125 + 14) & 0x7FFF
	// ------------------------------------------------------------------
	static int32_t Rand15()
	{
		Lcg() = (Lcg() * 125 + 0xE) & 0x7FFF;
		return (int32_t)Lcg();
	}

	// ------------------------------------------------------------------
	// Particle spawn (0x662590): at h +0x7C, into the slot the cursor names when free, else the first free slot (slots
	// 0..0x7F; none: no particle); the cursor goes to the slot after it (wrapping); life 0xB, size 0x400 + rand >> 12,
	// velocity x / z (rand >> 13) - 2, y 0xFD - (rand >> 11)
	// ------------------------------------------------------------------
	static void SpawnParticle(uint8_t *h)
	{
		MainNode *m = F<MainNode *>(h, 0x84);
		Particle *s = m->next;
		if (s->life != 0)
		{
			s = &m->parts[0];
			if (s->life != 0)
			{
				int k = 0x80;
				do
				{
					s++;
					if (--k == 0) return;
				} while (s->life != 0);
			}
		}
		m->next = s < &m->parts[0x7F] ? s + 1 : &m->parts[0];
		*(uint32_t *)&s->pos[0] = F<uint32_t>(h, 0x7C);
		*(uint32_t *)&s->pos[2] = F<uint32_t>(h, 0x80);
		s->life = 0xB;
		s->size = (int16_t)((Rand15() >> 0xC) + 0x400);
		s->vel[0] = (int8_t)((Rand15() >> 0xD) - 2);
		s->vel[1] = (int8_t)(0xFD - (Rand15() >> 0xB));
		s->vel[2] = (int8_t)((Rand15() >> 0xD) - 2);
	}

	// a lit polygon of sweep B may throw a particle from its centre (module LCG < 0x800)
	static void SparkTri(uint8_t *h)
	{
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(if (held_drawing()) return;)
		if (Rand15() >= 0x800) return;
		F<int16_t>(h, 0x7C) = (int16_t)(((int32_t)F<int16_t>(h, 0x44) + F<int16_t>(h, 0x34) + F<int16_t>(h, 0x24)) / 3);
		F<int16_t>(h, 0x7E) = (int16_t)(((int32_t)F<int16_t>(h, 0x46) + F<int16_t>(h, 0x36) + F<int16_t>(h, 0x26)) / 3);
		F<int16_t>(h, 0x80) = (int16_t)(((int32_t)F<int16_t>(h, 0x48) + F<int16_t>(h, 0x38) + F<int16_t>(h, 0x28)) / 3);
		SpawnParticle(h);
	}

	static void SparkQuad(uint8_t *h)
	{
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(if (held_drawing()) return;)
		if (Rand15() >= 0x800) return;
		F<int16_t>(h, 0x7C) = (int16_t)(((int32_t)F<int16_t>(h, 0x54) + F<int16_t>(h, 0x44) + F<int16_t>(h, 0x34) + F<int16_t>(h, 0x24)) / 4);
		F<int16_t>(h, 0x7E) = (int16_t)(((int32_t)F<int16_t>(h, 0x56) + F<int16_t>(h, 0x46) + F<int16_t>(h, 0x36) + F<int16_t>(h, 0x26)) / 4);
		F<int16_t>(h, 0x80) = (int16_t)(((int32_t)F<int16_t>(h, 0x58) + F<int16_t>(h, 0x48) + F<int16_t>(h, 0x38) + F<int16_t>(h, 0x28)) / 4);
		SpawnParticle(h);
	}

	// ------------------------------------------------------------------
	// Sweep B's vertices (0x662630, no IDB start): 16-byte entries (sxy, sz, grey, -, bone-space x / y / z)
	// ------------------------------------------------------------------
	static void ProjectB(uint8_t *h)
	{
		const uint8_t *data = F<uint8_t *>(h, 0x74);
		uint8_t *out = F<uint8_t *>(h, 0x78);
		const int32_t n = *(const int16_t *)data;
		data += 2;
		if (n != 0)
		{
			for (int32_t k = n; k != 0; k--)
			{
				GteLoadV0u16(data);
				GteMVMVA_RotV0Tr();
				out += 0x10;
				data += 6;
				GteStoreIR123Minus8(out);
			}
		}
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		out = F<uint8_t *>(h, 0x78);
		if (n != 0)
		{
			for (int32_t k = n; k != 0; k--)
			{
				GteLoadV0(out + 8);
				GteRTPS();
				const uint8_t g = GreyOf(*(const int16_t *)(out + 0xA), F<int16_t>(h, 0x6C));
				GteReadSXY2(h + 0x1C);
				GteReadOTZ(h + 0x20);
				if (F<int16_t>(h, 0x20) > 0)
				{
					out[6] = g;
					*(uint32_t *)out = F<uint32_t>(h, 0x1C);
					*(uint16_t *)(out + 4) = F<uint16_t>(h, 0x20);
				}
				else *(uint16_t *)(out + 4) = 0;
				out += 0x10;
			}
		}
		F<const uint8_t *>(h, 0x74) = data;
		F<uint8_t *>(h, 0x78) = out;
	}

	static inline void Copy16(uint8_t *dst, const uint8_t *src) { memcpy(dst, src, 0x10); }

	// ------------------------------------------------------------------
	// Sweep B's polygons (0x661EB0, no IDB start)
	// ------------------------------------------------------------------
	static void PolysB(uint8_t *h, uint32_t ot, int32_t shift)
	{
		const uint8_t *verts = F<const uint8_t *>(h, 4);
		const uint8_t *p = F<const uint8_t *>(h, 0);
		uint8_t *pk = F<uint8_t *>(h, 0x88);
		for (uint32_t i = 0; i < F<uint16_t>(h, 8); i++, p += 0x10)
		{
			Copy16(h + 0x1C, verts + ((*(const uint16_t *)p & 0xFFF) << 4));
			Copy16(h + 0x2C, verts + ((*(const uint16_t *)(p + 2) & 0xFFF) << 4));
			Copy16(h + 0x3C, verts + ((*(const uint16_t *)(p + 4) & 0xFFF) << 4));
			if ((int16_t)(F<uint16_t>(h, 0x30) | F<uint16_t>(h, 0x40) | F<uint16_t>(h, 0x20)) <= 0) continue;
			if ((uint8_t)(h[0x32] | h[0x42] | h[0x22]) == 0) continue;
			GteSetSXY012(F<uint32_t>(h, 0x1C), F<uint32_t>(h, 0x2C), F<uint32_t>(h, 0x3C));
			GteNCLIP();
			GteReadMAC0(h + 0x5C);
			if (F<int32_t>(h, 0x5C) < 0) continue;
			GteSetSZ123(F<int16_t>(h, 0x20), F<int16_t>(h, 0x30), F<int16_t>(h, 0x40));
			GteAVSZ3();
			GteReadOTZWord(h + 0x64);
			const uint32_t bucket = ot + (uint32_t)(F<int32_t>(h, 0x64) >> (shift & 31)) * 4;
			if ((uint8_t)(h[0x32] & h[0x42] & h[0x22]) == 0x80)
			{
				F<uint32_t>(pk, 0) = 0x7000000;
				F<uint32_t>(pk, 4) = F<uint32_t>(h, 0x14) | 0x24000000;
				F<uint32_t>(pk, 8) = F<uint32_t>(h, 0x1C);
				F<uint32_t>(pk, 0x10) = F<uint32_t>(h, 0x2C);
				F<uint32_t>(pk, 0x18) = F<uint32_t>(h, 0x3C);
				F<uint32_t>(pk, 0xC) = *(const uint32_t *)(p + 8);
				F<uint32_t>(pk, 0x14) = *(const uint32_t *)(p + 0xC);
				F<uint16_t>(pk, 0x1C) = *(const uint16_t *)(p + 6);
				F<int16_t>(pk, 0x16) = (int16_t)(F<int16_t>(pk, 0x16) + F<int16_t>(h, 0x6E));
				F<int16_t>(pk, 0xE) = (int16_t)(F<int16_t>(pk, 0xE) + F<int16_t>(h, 0x70));
				InsertPrimAutoDepth(bucket, pk);
				pk += 0x20;
				continue;
			}
			uint8_t *q = F<uint8_t *>(h, 0x8C);
			uint8_t *g = pk;
			pk += 0x28;
			F<uint8_t *>(h, 0x8C) = q + 0x1C;
			F<uint32_t>(g, 0) = 0x9000000;
			F<uint32_t>(q, 0) = 0x6000000;
			F<uint32_t>(q, 8) = F<uint32_t>(g, 8) = F<uint32_t>(h, 0x1C);
			F<uint32_t>(q, 0x10) = F<uint32_t>(g, 0x14) = F<uint32_t>(h, 0x2C);
			F<uint32_t>(q, 0x18) = F<uint32_t>(g, 0x20) = F<uint32_t>(h, 0x3C);
			F<uint32_t>(g, 0xC) = *(const uint32_t *)(p + 8);
			F<uint32_t>(g, 0x18) = *(const uint32_t *)(p + 0xC);
			F<uint16_t>(g, 0x24) = *(const uint16_t *)(p + 6);
			F<uint32_t>(g, 4) = Grey(h[0x22]) | 0x36000000;
			F<uint32_t>(q, 4) = Grey(h[0x22]) | 0x32000000;
			F<uint32_t>(q, 0xC) = F<uint32_t>(g, 0x10) = Grey(h[0x32]);
			F<uint32_t>(q, 0x14) = F<uint32_t>(g, 0x1C) = Grey(h[0x42]);
			F<int16_t>(g, 0x1A) = (int16_t)(F<int16_t>(g, 0x1A) + F<int16_t>(h, 0x6E));
			F<int16_t>(g, 0xE) = (int16_t)(F<int16_t>(g, 0xE) + F<int16_t>(h, 0x70));
			InsertPrimAutoDepth(bucket, g);
			InsertPrimAutoDepth(bucket, q);
			uint8_t *d = F<uint8_t *>(h, 0x8C);
			F<uint8_t *>(h, 0x8C) = d + 8;
			F<uint32_t>(d, 0) = 0x1000000;
			F<uint32_t>(d, 4) = 0xE10002C0;
			InsertPrimAutoDepth(bucket, d);
			SparkTri(h);
		}
		p += 4;
		for (uint32_t i = 0; i < F<uint16_t>(h, 0xA); i++, p += 0x14)
		{
			Copy16(h + 0x1C, verts + ((*(const uint16_t *)(p - 4) & 0xFFF) << 4));
			Copy16(h + 0x2C, verts + ((*(const uint16_t *)(p - 2) & 0xFFF) << 4));
			Copy16(h + 0x3C, verts + ((*(const uint16_t *)p & 0xFFF) << 4));
			Copy16(h + 0x4C, verts + ((*(const uint16_t *)(p + 2) & 0xFFF) << 4));
			if ((int16_t)(F<uint16_t>(h, 0x30) | F<uint16_t>(h, 0x50) | F<uint16_t>(h, 0x40) | F<uint16_t>(h, 0x20)) <= 0) continue;
			if ((uint8_t)(h[0x32] | h[0x42] | h[0x52] | h[0x22]) == 0) continue;
			GteSetSXY012(F<uint32_t>(h, 0x1C), F<uint32_t>(h, 0x2C), F<uint32_t>(h, 0x3C));
			GteNCLIP();
			GteReadMAC0(h + 0x5C);
			if (F<int32_t>(h, 0x5C) < 0) continue;
			GteSetSZ0123(F<int16_t>(h, 0x20), F<int16_t>(h, 0x30), F<int16_t>(h, 0x40), F<int16_t>(h, 0x50));
			GteAVSZ4();
			GteReadOTZWord(h + 0x64);
			const uint32_t bucket = ot + (uint32_t)(F<int32_t>(h, 0x64) >> (shift & 31)) * 4;
			if ((uint8_t)(h[0x32] & h[0x42] & h[0x52] & h[0x22]) == 0x80)
			{
				F<uint32_t>(pk, 0) = 0x9000000;
				F<uint32_t>(pk, 4) = F<uint32_t>(h, 0x14) | 0x2C000000;
				F<uint32_t>(pk, 8) = F<uint32_t>(h, 0x1C);
				F<uint32_t>(pk, 0x10) = F<uint32_t>(h, 0x2C);
				F<uint32_t>(pk, 0x18) = F<uint32_t>(h, 0x3C);
				F<uint32_t>(pk, 0x20) = F<uint32_t>(h, 0x4C);
				F<uint32_t>(pk, 0xC) = *(const uint32_t *)(p + 4);
				F<uint32_t>(pk, 0x14) = *(const uint32_t *)(p + 8);
				F<uint16_t>(pk, 0x1C) = *(const uint16_t *)(p + 0xC);
				F<uint16_t>(pk, 0x24) = *(const uint16_t *)(p + 0xE);
				F<int16_t>(pk, 0x16) = (int16_t)(F<int16_t>(pk, 0x16) + F<int16_t>(h, 0x6E));
				F<int16_t>(pk, 0xE) = (int16_t)(F<int16_t>(pk, 0xE) + F<int16_t>(h, 0x70));
				InsertPrimAutoDepth(bucket, pk);
				pk += 0x28;
				continue;
			}
			uint8_t *q = F<uint8_t *>(h, 0x8C);
			uint8_t *g = pk;
			pk += 0x34;
			F<uint8_t *>(h, 0x8C) = q + 0x24;
			F<uint32_t>(g, 0) = 0xC000000;
			F<uint32_t>(q, 0) = 0x8000000;
			F<uint32_t>(q, 8) = F<uint32_t>(g, 8) = F<uint32_t>(h, 0x1C);
			F<uint32_t>(q, 0x10) = F<uint32_t>(g, 0x14) = F<uint32_t>(h, 0x2C);
			F<uint32_t>(q, 0x18) = F<uint32_t>(g, 0x20) = F<uint32_t>(h, 0x3C);
			F<uint32_t>(q, 0x20) = F<uint32_t>(g, 0x2C) = F<uint32_t>(h, 0x4C);
			F<uint32_t>(g, 0xC) = *(const uint32_t *)(p + 4);
			F<uint32_t>(g, 0x18) = *(const uint32_t *)(p + 8);
			F<uint16_t>(g, 0x24) = *(const uint16_t *)(p + 0xC);
			F<uint16_t>(g, 0x30) = *(const uint16_t *)(p + 0xE);
			F<uint32_t>(g, 4) = Grey(h[0x22]) | 0x3E000000;
			F<uint32_t>(q, 4) = Grey(h[0x22]) | 0x3A000000;
			F<uint32_t>(q, 0xC) = F<uint32_t>(g, 0x10) = Grey(h[0x32]);
			F<uint32_t>(q, 0x14) = F<uint32_t>(g, 0x1C) = Grey(h[0x42]);
			F<uint32_t>(q, 0x1C) = F<uint32_t>(g, 0x28) = Grey(h[0x52]);
			F<int16_t>(g, 0x1A) = (int16_t)(F<int16_t>(g, 0x1A) + F<int16_t>(h, 0x6E));
			F<int16_t>(g, 0xE) = (int16_t)(F<int16_t>(g, 0xE) + F<int16_t>(h, 0x70));
			InsertPrimAutoDepth(bucket, g);
			InsertPrimAutoDepth(bucket, q);
			uint8_t *d = F<uint8_t *>(h, 0x8C);
			F<uint8_t *>(h, 0x8C) = d + 8;
			F<uint32_t>(d, 0) = 0x1000000;
			F<uint32_t>(d, 4) = 0xE10002C0;
			InsertPrimAutoDepth(bucket, d);
			SparkQuad(h);
		}
		F<uint8_t *>(h, 0x88) = pk;
	}

	// ------------------------------------------------------------------
	// Sweep B's model (0x661DC0)
	// ------------------------------------------------------------------
	static void ModelB(const uint8_t *model, uint8_t *h, uint32_t ot, int32_t shift)
	{
		const uint32_t *objs = *(const uint32_t *const *)(model + 4);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const int32_t count = (int32_t)objs[0];
		for (int32_t i = 0; i < count; i++)
		{
			F<const uint8_t *>(h, 0x74) = *(const uint8_t *const *)(model + 4) + objs[1 + i];
			if (!(F<uint32_t>(h, 0x18) & (1u << (i & 31)))) continue;
			F<uint32_t>(h, 0x78) = F<uint32_t>(h, 4);
			const uint8_t *p = F<const uint8_t *>(h, 0x74);
			const int32_t groups = *(const int16_t *)p;
			F<const uint8_t *>(h, 0x74) = p + 2;
			if (groups > 0)
			{
				for (int32_t g = groups; g != 0; g--)
				{
					const uint8_t *q = F<const uint8_t *>(h, 0x74);
					const int32_t bone = *(const int16_t *)q;
					F<const uint8_t *>(h, 0x74) = q + 2;
					const Mat4x3 *bm = (const Mat4x3 *)(bones + mul32(bone, 0x30) + 0x10);
					GteSetRotMatrixCtrl(bm);
					GteSetTransVectorCtrl(bm);
					ProjectB(h);
				}
			}
			p = (const uint8_t *)((F<uint32_t>(h, 0x74) + 3) & ~3u);
			F<uint16_t>(h, 8) = *(const uint16_t *)p;
			F<uint16_t>(h, 0xA) = *(const uint16_t *)(p + 2);
			F<const uint8_t *>(h, 0x74) = p + 0xC;
			F<const uint8_t *>(h, 0) = p + 0xC;
			PolysB(h, ot, shift);
		}
	}

	// ------------------------------------------------------------------
	// Sweep B (0x661CC0): as A with B's vertices (magic buffer + 0x81C) and polygons
	// ------------------------------------------------------------------
	static void SweepB(MainNode *m, uint8_t *ent, uint32_t verts, int16_t level, int16_t page, int16_t clut)
	{
		FrameArena() = DrawShadow(ent, OT(0x4040), 0x10, FrameArena());
		uint8_t *b = (uint8_t *)FieldAlloc(0xB0);
		uint8_t *anim = ent + 0x60;
		F<int16_t>(b, 0x8E) = page;
		F<int16_t>(b, 0x8C) = level;
		F<int16_t>(b, 0x90) = clut;
		F<MainNode *>(b, 0xA4) = m;
		ComputeBonesWorldMatrices(anim, ent + 0x40);
		F<uint32_t>(b, 0x24) = verts;
		F<int16_t>(b, 0x2C) = 0;
		F<int16_t>(b, 0x2E) = 0;
		F<int16_t>(b, 0x30) = 0x140;
		F<int16_t>(b, 0x32) = 0xD8;
		F<uint32_t>(b, 0x34) = F<uint32_t>(ent, 0x28);
		F<uint32_t>(b, 0x38) = F<uint32_t>(ent, 0x7C);
		F<uint32_t>(b, 0xA8) = FrameArena();
		F<uint32_t>(b, 0xAC) = PacketCursor();
		ModelB(F<const uint8_t *>(anim, 4), b + 0x20, OT(0x44), 0);
		FrameArena() = F<uint32_t>(b, 0xA8);
		PacketCursor() = F<uint32_t>(b, 0xAC);
		BuildBoneMatricesFromPose(anim);
		FieldFree(0xB0);
	}

	// ------------------------------------------------------------------
	// Particle (0x6627B0): drawn THEN moved: the sequence frame (frame count - life) at its position through the camera,
	// scaled by its size, into OT slot 2 of the module arena; then position += 4 x velocity, life - 1
	// ------------------------------------------------------------------
	static void ParticleDraw(uint8_t *h, Particle *p)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		GteLoadV0(p);
		GteMVMVA_RotV0Tr();
		GteReadMAC123((int32_t *)(h + 0x98));
		const int16_t size = p->size;
		F<uint32_t>(h, 0) = SEQ_Particle;
		F<int16_t>(h, 0x94) = size;
		F<int16_t>(h, 0x8C) = size;
		F<int16_t>(h, 0x84) = size;
		const uint32_t idx = (var<uint32_t>(SEQ_Particle + 8) & 0xFFFF) - (uint32_t)(int32_t)p->life;
		const uint32_t rec = (uint32_t)var<uint16_t>(SEQ_Particle + 8 + idx * 2) + SEQ_Particle;
		F<uint32_t>(h, 0x2C) = rec;
		F<uint32_t>(h, 0x30) = var<uint32_t>(rec);
		F<uint32_t>(h, 0x2C) = rec + 4;
		PacketCursor() = DrawSprites(h, OT(0x44), 2, PacketCursor(), false);
		p->life--;
		p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(p->vel[0] << 2));
		p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(p->vel[1] << 2));
		p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(p->vel[2] << 2));
	}

	// the particles (0x662720): every live slot, in slot order
	static void Particles(MainNode *m)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<int16_t>(h, 0x92) = 0;
		F<int16_t>(h, 0x90) = 0;
		F<int16_t>(h, 0x8E) = 0;
		F<int16_t>(h, 0x8A) = 0;
		F<int16_t>(h, 0x88) = 0;
		F<int16_t>(h, 0x86) = 0;
		F<int16_t>(h, 0x60) = 0;
		F<int16_t>(h, 0x58) = 0;
		F<int16_t>(h, 0x50) = 0;
		F<int16_t>(h, 0x48) = 0;
		for (int k = 0; k < 0x80; k++)
			if (m->parts[k].life != 0) ParticleDraw(h, &m->parts[k]);
		FieldFree(0xB4);
	}

	// sweep levels: A at counter c (0x14..0x95), B at counter c (0x96..0x12B)
	static int16_t LevelA(int32_t c)
	{
		const uint32_t t = (uint32_t)(c - 0x14);
		return t < 0x64 ? (int16_t)(0x200 - (int32_t)(t * 4000u / 100u)) : (int16_t)-0xDA0;
	}
	static int16_t LevelB(int32_t c)
	{
		const uint32_t t = (uint32_t)(c - 0x96);
		return (int16_t)(0x200 - (int32_t)((600000u - t * 4000u) / 150u));
	}

	// ------------------------------------------------------------------
	// Main task (0x661350)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const int16_t c = m->counter;
		if (c >= 0x138)
		{
			*(uint16_t *)Caster() &= 0xFFF7;
			*(uint32_t *)(Caster() + 0x2C) = m->colour;
			*(uint16_t *)Entity(0) &= 0xFFFB;
			*(uint16_t *)Entity(1) &= 0xFFFB;
			*(uint16_t *)Entity(2) &= 0xFFFB;
			return TASK_END;
		}
		if (c < 8) Flash() = (uint32_t)(mul32(c, 1000) >> 3);
		else if (c > 0x130) Flash() = (uint32_t)(mul32(0x138 - c, 1000) >> 3);
		else Flash() = 0x3E8;
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(held_note_main(m);)
		if ((uint32_t)(c - 0x14) < 0x82)
		{
			SweepA(Caster(), LevelA(c), PAGE_MOVE, CLUT_MOVE, PacketCursor());
			*Caster() |= 4;
		}
		if ((uint32_t)(c - 0x96) < 0x96)
		{
			SweepB(m, Caster(), TexBase() + VERTS_B, LevelB(c), PAGE_MOVE, CLUT_MOVE);
			*Caster() |= 4;
		}
		const uint32_t t3 = (uint32_t)(m->counter - 0x96);
		if (t3 < 0x8C)
		{
			const int32_t w = (int32_t)((t3 << 12) / 140u);
			ColourLerp(&m->colour, COLOUR_Target, 0x1000 - w, w, Caster() + 0x2C);
			Caster()[0x2F] = 0x32;
		}
		if (m->counter >= 0x3C) *Caster() |= 8;
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(held_note_particles(m);)
		Particles(m);
		if (m->counter == 0x2D)
		{
			BdPlaySE(SOUND_Start, 0, 0x80);
			m->counter++;
			return 0;
		}
		if (m->counter == 0x134) ReleaseVoice(m->voice);
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x661190)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		if (r->parity != 0)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->parity = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->parity = 1;
		}
		Flash() = 0;
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(held_note_root();)
		if (r->counter == 2 && !r->started)
		{
			r->started = 1;
			MusicVolumeFade(0xF0);
			InitTaskQueuePool(QMain(), (void *)TexBase(), 0x81C, 1);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			FillDwords(&m->counter, 0, 0x204);
			m->voice = ClaimVoiceSlot(SOUND_Voice, 1, 0x80);
			m->next = &m->parts[0];
			m->colour = *(const uint32_t *)(Caster() + 0x2C);
			ShowAllPolys(Caster());
		}
		const int left = r->started ? ExecuteTaskQueue(QMain()) : (int)n;
		SetScreenFlash(Flash(), 0);
		if (r->started && left == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag316_red_giant()
	{
		register_port(redgiant316::ORIG_RootTask, (void *)redgiant316::RootTask, "316 RootTask", 316);
		register_port(redgiant316::ORIG_MainTask, (void *)redgiant316::MainTask, "316 MainTask", 316);
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(register_mag316_held();)
	}
	FX_LAB_MODULE(register_mag316_red_giant);
}

#ifdef FF8_FX_HELD
#include "mag316_red_giant_held.inc"
#endif
