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
// FUJIN: Fujin's described Red Giant death (effect_316.json) runs this code: its memory through own::R() (fx_port.h; the
// colour target 0x2505570, never written, too), its spawns through own::Fn(), the file loader (0x6610C0) and the setup
// (0x6610B0 -> _Init 0x6610E0) ported below; its parameters (RedGiantParams: the sounds and the main's ticks, the screen
// flash, the two light sweeps (windows, levels, the texture moves), the colour fade, the particles; their flipbook, the
// exe's TIM, the texture name and the camera track). FX_PART tags its parts.

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
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x2505520)); }        // the effect's file (its TIM)
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x2505524)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x2505528)); }        // caster entity (setup)
	inline TaskQueue *QMain() { return (TaskQueue *)own::R(0x2505530); }           // pool: magic buffer, 1 x 0x81C
	inline TaskQueue *QRoot() { return (TaskQueue *)own::R(0x2505540); }           // pool 0x2505550, 1 x 0x14
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x2505564)); }
	inline uint32_t &Lcg() { return var<uint32_t>(own::R(0x2505568)); }
	inline uint32_t &Flash() { return var<uint32_t>(own::R(0x250556C)); }
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2505574)); }        // Magic_TextureOFF (magic buffer)

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

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_SWEEP_A = 1, PART_SWEEP_B = 2, PART_PARTICLES = 3 };
	struct RedGiantParams
	{
		struct { uint32_t start, voice; } sounds;      // the sound, the voice (its slot claimed at the start)
		struct { int16_t sound, release, end; } timing; // the main's ticks: the sound, the voice released, its end
		struct { int16_t ramp, level; } flash;        // the screen flash: up / down over `ramp` ticks, its level between
		// the light level both sweeps measure the caster's vertices against (grey = (y - level) / 4 + 0x40), its top, the
		// texture page / CLUT moves of the lit polygons
		struct { int16_t top, page, clut; } sweep;
		// sweep A (falling): its ticks, the level falling by range over `fall` ticks (then held)
		struct { int16_t start, ticks, range, fall; } sweep_a;
		// sweep B (rising, its lit polygons throwing particles): its ticks (the level rising by range over them)
		struct { int16_t start, ticks, range; } sweep_b;
		struct { int16_t start, ticks, alpha; } fade;  // the caster's colour faded to black: its ticks, its byte +0x2F
		// the particles thrown by sweep B's lit polygons: the chance (of 0x8000), their ticks, size (+ a random below 8),
		// their velocity's scale a tick
		struct { int16_t chance, life, size, speed; } particles;
		struct { uint32_t particle; } seqs;             // the particles' flipbook
		struct { uint32_t tim, texture_name, camera; } res; // its resources (data): the exe's TIM, texture file name, camera track
	};
	static const RedGiantParams GAME = { { 0x1005D50, 0x1005D54 }, { 0x2D, 0x134, 0x138 }, { 8, 1000 }, { 0x200, PAGE_MOVE, CLUT_MOVE },
		{ 0x14, 0x82, 4000, 0x64 }, { 0x96, 0x96, 4000 }, { 0x96, 0x8C, 0x32 }, { 0x800, 0xB, 0x400, 4 }, { SEQ_Particle },
		{ TIM_Exe, TEXTURE_NAME, CAMERA } };
	static RedGiantParams live = GAME;
	static const RedGiantParams &PG() { return live; }

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
		s->life = PG().particles.life;
		s->size = (int16_t)((Rand15() >> 0xC) + PG().particles.size);
		s->vel[0] = (int8_t)((Rand15() >> 0xD) - 2);
		s->vel[1] = (int8_t)(0xFD - (Rand15() >> 0xB));
		s->vel[2] = (int8_t)((Rand15() >> 0xD) - 2);
	}

	// a lit polygon of sweep B may throw a particle from its centre (module LCG < 0x800)
	static void SparkTri(uint8_t *h)
	{
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(if (held_drawing()) return;)
		if (Rand15() >= PG().particles.chance) return;
		F<int16_t>(h, 0x7C) = (int16_t)(((int32_t)F<int16_t>(h, 0x44) + F<int16_t>(h, 0x34) + F<int16_t>(h, 0x24)) / 3);
		F<int16_t>(h, 0x7E) = (int16_t)(((int32_t)F<int16_t>(h, 0x46) + F<int16_t>(h, 0x36) + F<int16_t>(h, 0x26)) / 3);
		F<int16_t>(h, 0x80) = (int16_t)(((int32_t)F<int16_t>(h, 0x48) + F<int16_t>(h, 0x38) + F<int16_t>(h, 0x28)) / 3);
		SpawnParticle(h);
	}

	static void SparkQuad(uint8_t *h)
	{
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(if (held_drawing()) return;)
		if (Rand15() >= PG().particles.chance) return;
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
		const uint32_t seq = fxp::sequence(PG().seqs.particle, SEQ_Particle);
		F<uint32_t>(h, 0) = seq;
		F<int16_t>(h, 0x94) = size;
		F<int16_t>(h, 0x8C) = size;
		F<int16_t>(h, 0x84) = size;
		const uint32_t idx = (var<uint32_t>(seq + 8) & 0xFFFF) - (uint32_t)(int32_t)p->life;
		const uint32_t rec = (uint32_t)var<uint16_t>(seq + 8 + idx * 2) + seq;
		F<uint32_t>(h, 0x2C) = rec;
		F<uint32_t>(h, 0x30) = var<uint32_t>(rec);
		F<uint32_t>(h, 0x2C) = rec + 4;
		PacketCursor() = DrawSprites(h, OT(0x44), 2, PacketCursor(), false);
		p->life--;
		const int32_t speed = PG().particles.speed;
		p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(p->vel[0] * speed));
		p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(p->vel[1] * speed));
		p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(p->vel[2] * speed));
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

	// sweep levels: A at counter c (0x14..0x95), B at counter c (0x96..0x12B) (the parameters' windows)
	static int16_t LevelA(int32_t c)
	{
		const RedGiantParams &g = PG();
		const uint32_t t = (uint32_t)(c - g.sweep_a.start), range = (uint32_t)(int32_t)g.sweep_a.range;
		return t < (uint32_t)g.sweep_a.fall ? (int16_t)(g.sweep.top - (int32_t)(t * range / (uint32_t)g.sweep_a.fall))
		                                    : (int16_t)(g.sweep.top - (int32_t)range);
	}
	static int16_t LevelB(int32_t c)
	{
		const RedGiantParams &g = PG();
		const uint32_t t = (uint32_t)(c - g.sweep_b.start), range = (uint32_t)(int32_t)g.sweep_b.range, ticks = (uint32_t)g.sweep_b.ticks;
		return (int16_t)(g.sweep.top - (int32_t)((range * ticks - t * range) / ticks));
	}

	// ------------------------------------------------------------------
	// Main task (0x661350)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const RedGiantParams &g = PG();
		const int16_t c = m->counter;
		if (c >= g.timing.end)
		{
			*(uint16_t *)Caster() &= 0xFFF7;
			*(uint32_t *)(Caster() + 0x2C) = m->colour;
			*(uint16_t *)Entity(0) &= 0xFFFB;
			*(uint16_t *)Entity(1) &= 0xFFFB;
			*(uint16_t *)Entity(2) &= 0xFFFB;
			return TASK_END;
		}
		if (c < g.flash.ramp) Flash() = (uint32_t)(mul32(c, g.flash.level) / g.flash.ramp);
		else if (c > g.timing.end - g.flash.ramp) Flash() = (uint32_t)(mul32(g.timing.end - c, g.flash.level) / g.flash.ramp);
		else Flash() = (uint32_t)(int32_t)g.flash.level;
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(held_note_main(m);)
		if ((uint32_t)(c - g.sweep_a.start) < (uint32_t)g.sweep_a.ticks)
		{
			FX_PART(PART_SWEEP_A);
			SweepA(Caster(), LevelA(c), g.sweep.page, g.sweep.clut, PacketCursor());
			*Caster() |= 4;
		}
		if ((uint32_t)(c - g.sweep_b.start) < (uint32_t)g.sweep_b.ticks)
		{
			FX_PART(PART_SWEEP_B);
			SweepB(m, Caster(), TexBase() + VERTS_B, LevelB(c), g.sweep.page, g.sweep.clut);
			*Caster() |= 4;
		}
		const uint32_t t3 = (uint32_t)(m->counter - g.fade.start);
		if (t3 < (uint32_t)g.fade.ticks)
		{
			const int32_t w = (int32_t)((t3 << 12) / (uint32_t)g.fade.ticks);
			ColourLerp(&m->colour, own::R(COLOUR_Target), 0x1000 - w, w, Caster() + 0x2C);
			Caster()[0x2F] = (uint8_t)g.fade.alpha;
		}
		if (m->counter >= 0x3C) *Caster() |= 8;
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(held_note_particles(m);)
		FX_PART(PART_PARTICLES);
		Particles(m);
		if (m->counter == g.timing.sound)
		{
			BdPlaySE((const void *)fxp::sound(g.sounds.start, (uint32_t)SOUND_Start), 0, 0x80);
			m->counter++;
			return 0;
		}
		if (m->counter == g.timing.release) ReleaseVoice(m->voice);
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
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), own::Fn(ORIG_MainTask));
			FillDwords(&m->counter, 0, 0x204);
			m->voice = ClaimVoiceSlot((const void *)fxp::sound(PG().sounds.voice, (uint32_t)SOUND_Voice), 1, 0x80);
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

	// ------------------------------------------------------------------
	// File loader (0x6610C0) and setup (0x6610B0 -> _Init 0x6610E0): the game runs its own; Fujin's programs these (its
	// memory may be a copy's): the TIMs, the magic buffer, the context, the caster (flags 0x0C cleared), the root, the
	// party's normal draw hidden, the camera
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(PG().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(TexFile());
		QueueTimUpload(fxp::tim(PG().res.tim, TIM_Exe));
		TexBase() = MagicTextureOff();
		Ctx() = ctx;
		Caster() = Entity(ctx->attacker);
		*(uint16_t *)Caster() &= 0xFFF3;
		InitTaskQueuePool(QRoot(), (void *)own::R(0x2505550), 0x14, 1);
		FillDwords((uint8_t *)AddTaskToQueue(QRoot(), own::Fn(ORIG_RootTask)) + 0xC, 0, 2);
		*(uint16_t *)Entity(0) |= 4;
		*(uint16_t *)Entity(1) |= 4;
		*(uint16_t *)Entity(2) |= 4;
		PlayCameraAnimation((const void *)fxp::camera(PG().res.camera, CAMERA));
		return QRoot();
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "main", "Main", "The main's ticks, the sound and the voice (claimed at the start, released near the end), the screen flash, the music faded; the TIMs and the camera" },
			{ PART_SWEEP_A, "sweep_a", "Sweep down", "The caster drawn lit by a light level falling down its body (its vertices grey by their height)" },
			{ PART_SWEEP_B, "sweep_b", "Sweep up", "The caster drawn by the level rising back, its lit polygons semi-transparent, some throwing particles" },
			{ 0, "fade", "Colour fade", "The caster's colour faded to black" },
			{ PART_PARTICLES, "particles", "Particles", "Particles thrown up from sweep B's lit polygons (a flipbook by their life)" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing", "Ticks", "The main's ticks" },
		};
#define RPF(member) (uint32_t)offsetof(RedGiantParams, member)
		static const Field FIELDS[] = {
			{ "main", "sounds.start", "Sound", "At the sound's tick", RPF(sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "main", "sounds.voice", "Voice", "The sound block its voice slot is claimed with (at the start)", RPF(sounds.voice), fxp::Sound, 0, 0x7FFFFFFF },
			{ "main", "timing.sound", "Sound", "The main's tick of the sound", RPF(timing.sound), fxp::I16, 0, 400 },
			{ "main", "timing.release", "Voice released", "Its tick the voice slot is released", RPF(timing.release), fxp::I16, 0, 400 },
			{ "main", "timing.end", "End", "Its last tick (the caster's colour put back, the party drawn again)", RPF(timing.end), fxp::I16, 1, 400 },
			{ "main", "flash.ramp", "Flash ramp", "The screen flash's ticks up at the start and down at the end", RPF(flash.ramp), fxp::I16, 1, 400 },
			{ "main", "flash.level", "Flash level", "The screen flash's level between", RPF(flash.level), fxp::I16, 0, 4000 },
			{ "main", "res.tim", "Exe TIM", "The image its setup uploads besides the texture file (as it is)", RPF(res.tim), fxp::Tim, 0, 0x7FFFFFFF },
			{ "main", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", RPF(res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "main", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", RPF(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "sweep_a", "sweep.top", "Top level", "The light level's top (sweep A starts there, sweep B ends there; y down)", RPF(sweep.top), fxp::I16, -0x4000, 0x4000 },
			{ "sweep_a", "sweep.page", "Page move", "The lit polygons' texture page word moved by this (both sweeps)", RPF(sweep.page), fxp::I16, -0x400, 0x400 },
			{ "sweep_a", "sweep.clut", "CLUT move", "Their CLUT word moved by this (both sweeps)", RPF(sweep.clut), fxp::I16, -0x8000, 0x7FFF },
			{ "sweep_a", "sweep_a.start", "Start", "The main's tick it starts", RPF(sweep_a.start), fxp::I16, 0, 400 },
			{ "sweep_a", "sweep_a.ticks", "Ticks", "Its ticks", RPF(sweep_a.ticks), fxp::I16, 0, 400 },
			{ "sweep_a", "sweep_a.range", "Fall", "The level falls by this from the top", RPF(sweep_a.range), fxp::I16, 0, 0x4000 },
			{ "sweep_a", "sweep_a.fall", "Fall ticks", "Over that many ticks (then it holds)", RPF(sweep_a.fall), fxp::I16, 1, 400 },
			{ "sweep_b", "sweep_b.start", "Start", "The main's tick it starts", RPF(sweep_b.start), fxp::I16, 0, 400 },
			{ "sweep_b", "sweep_b.ticks", "Ticks", "Its ticks (the level rises back to the top over them)", RPF(sweep_b.ticks), fxp::I16, 1, 400 },
			{ "sweep_b", "sweep_b.range", "Rise", "The level rises by this to the top", RPF(sweep_b.range), fxp::I16, 0, 0x4000 },
			{ "fade", "fade.start", "Start", "The main's tick it starts", RPF(fade.start), fxp::I16, 0, 400 },
			{ "fade", "fade.ticks", "Ticks", "Its ticks", RPF(fade.ticks), fxp::I16, 1, 400 },
			{ "fade", "fade.alpha", "Alpha byte", "The caster's byte +0x2F while it fades", RPF(fade.alpha), fxp::I16, 0, 0xFF },
			{ "particles", "particles.chance", "Chance", "A lit polygon throws one when a random below 0x8000 is below this", RPF(particles.chance), fxp::I16, 0, 0x7FFF },
			{ "particles", "particles.life", "Life", "Their ticks (below their flipbook's frame count: 11 at most)", RPF(particles.life), fxp::I16, 1, 11 },
			{ "particles", "particles.size", "Size", "Their size (+ a random below 8)", RPF(particles.size), fxp::I16, 0, 0x4000, fxp::Scale },
			{ "particles", "particles.speed", "Speed", "Their velocity's scale a tick", RPF(particles.speed), fxp::I16, -64, 64 },
			{ "particles", "seqs.particle", "Flipbook", "The particles' flipbook", RPF(seqs.particle), fxp::Seq, 0, 0x7FFFFFFF },
		};
#undef RPF
	}
}

	void register_mag316_red_giant()
	{
		fxp::register_params({ 316, "Red Giant death", &redgiant316::live, &redgiant316::GAME, sizeof(redgiant316::RedGiantParams),
		                       redgiant316::table::PARTS, (int)(sizeof(redgiant316::table::PARTS) / sizeof(redgiant316::table::PARTS[0])),
		                       redgiant316::table::FIELDS, (int)(sizeof(redgiant316::table::FIELDS) / sizeof(redgiant316::table::FIELDS[0])),
		                       redgiant316::table::GROUPS, (int)(sizeof(redgiant316::table::GROUPS) / sizeof(redgiant316::table::GROUPS[0])) });
		own::note(redgiant316::ORIG_FileLoader, (void *)redgiant316::FileLoader);
		own::note(redgiant316::ORIG_Setup, (void *)redgiant316::Setup);
		own::note(redgiant316::ORIG_RootTask, (void *)redgiant316::RootTask);
		register_port(redgiant316::ORIG_RootTask, (void *)redgiant316::RootTask, "316 RootTask", 316);
		own::note(redgiant316::ORIG_MainTask, (void *)redgiant316::MainTask);
		register_port(redgiant316::ORIG_MainTask, (void *)redgiant316::MainTask, "316 MainTask", 316);
		// 30 fps layer: see mag316_red_giant_held.inc
		FX_HELD(register_mag316_held();)
	}
	FX_LAB_MODULE(register_mag316_red_giant);
}

#ifdef FF8_FX_HELD
#include "mag316_red_giant_held.inc"
#endif
