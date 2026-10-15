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

// Effect 176: Sand Storm (enemy attack 80 of kernel.bin, used by Adamantoise c0m046; MAG_176_*).
// Not a copy of effect 155 (Fastitocalon's Sand Storm): its funnels are Tornado's (146) ring funnel
// (0x617460 / 0x617720 shapes with 8 instead of 16 vertices per ring and their own parameters).
//
// Structure (setup MAG_176_SAND_STORM_Init 0x5D00F0, file loader 0x5D00D0 = the texture file named at
// 0xD45178; code 0x5D00D0..0x5D1AB0). The setup keeps the packet arena base (Magic_TextureOFF_ToEAX1,
// 0x237456C), the cast context, the caster's slot and the first target's slot, builds the root queue
// 0x2368598 (pool 0x2368588, 1 x 0x10) with the root task and the effect queue 0x23733E8 (pool
// 0x23733F8, 100 x 0x24) with the director, clears the sand pool, starts camera animation 0xD44EB0
// and queues the TIM.
//   Root (0x5D1A60) - parity 0x2374568 = counter & 1 picks the packet arena half (base / base +
//     0xC000, cursor 0x2374570) and the funnel packet copy, runs the effect queue, ends when it is
//     empty.
//   Director (0x5D01B0) - tick 0: the caster's default effect position (0x2374558, its y and w words
//     swapped, unused), the targets' centre (0x2374298), both funnels reset (0x5D0550); tick 12: the
//     two funnels (bones 0x15 / 0x16 of the caster, mirrored sway) and the two sand emitters; tick
//     14: the wind sound; tick 57: one lift task per target of action 0 (random delay 0..5 ticks,
//     drawn towards the farther funnel when the nearer is funnel A); ends after 120.
//   Funnel init (0x5D0550) - the funnel's 512 POLY_FT4 packets (16 rings x 8 quads x 2 parity
//     copies: two texture pages, v from the ring) and its 17 rings (0x14 bytes: x, y, z, -, tilt
//     angles x / y / z, -, radius) stacked 6.25 apart, radius 300; the wander point cleared.
//   Funnel (0x5D0740) - parameters from five counter phases (grow 0..25, rise 26..45, pulse 46..55,
//     hold 56..71, widen 72..87); stands at its bone position until 25, then walks to the targets'
//     centre over 38 ticks with a sway through the caster's matrix; publishes its base (x, 0, z,
//     radius) for the sand and the lifts (A 0x23685A8, B 0x23685B0); updates the rings (0x5D0F30,
//     also on draw-only ticks) and draws them (0x5D0C80) at spin +-100 per tick, fading in over 16
//     and out over the last 8 ticks; ends at 88.
//   Rings (0x5D0F30) - rings 16..1 take the previous ring's tilt with a random jitter (-64..63),
//     ring 0 the wander offset, which moves along random-walk angles; each ring is placed by its
//     tilt (sway vector), height and radius (sin(sin) wave).
//   Funnel draw (0x5D0C80) - 8 vertices per ring (+0x40 twist per ring), quad j of ring r between
//     rings r and r - 1 into packet 32 (r - 1) + 2 j + parity, colour = fade.
//   Sand (0x5D11F0) - two emitters (mask 1 = funnel A, 2 = funnel B) of sprites 0xD44FA0 in the
//     shared pool 0x23685B8 (150 x 0x18) thrown outwards around the funnel base and rising (1..3
//     per tick over ticks 0..80), sliding and slowing down.
//   Lift (0x5D14B0) - one per target: saves its entity (flags, rotation, position: 0x2374208, 7 x
//     0x14), spins it up into the funnel (entity rotation / position written directly), holds it,
//     drops it at 44, restores the entity at 52 and starts the landing dust (+ sound).
//   Landing dust (0x5D1820) - 6 sprites per tick (ticks 0..2, mask 0x100 << target index) sliding
//     outwards from the landing point; tick 2: the target's damage.
// Every task but the root tests battle_to_update_flags (0x1D96A9C) & 0x201: the funnels still
// update their rings and draw, the sand draws, the lifts write the entity of their phase (from
// tick 44: the drop again), nothing advances, the director does nothing.
// Module globals: 0x2368580..0x2374574 (selectors, queues, bases, sand pool, funnel packets, rings,
// wander points, entity saves, cast context, arena base / parity / cursor); exe data written: the
// funnel matrix translation 0xD4516C..0xD45177.

#include "mag_common.h"

namespace ff8fx
{
namespace sandstorm176
{
	using namespace eng;
	using namespace magc;

	// ------------------------------------------------------------------
	// raw memory access (layouts follow the listings offset by offset)
	// ------------------------------------------------------------------
	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }

	// ------------------------------------------------------------------
	// module globals
	// ------------------------------------------------------------------
	inline uint32_t &RingSel() { return var<uint32_t>(0x2368580); }      // rings of the funnel being updated / drawn
	inline uint32_t &TargetSlot() { return var<uint32_t>(0x2368584); }   // first target's slot (setup, unused)
	inline uint32_t &PacketSel() { return var<uint32_t>(0x2374560); }    // packets of the funnel being drawn
	inline uint32_t &HeadSel() { return var<uint32_t>(0x2374564); }      // wander point of the funnel being updated
	inline CastContext *&Ctx() { return var<CastContext *>(0x237454C); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x2374550); }
	inline uint32_t &TexFile() { return var<uint32_t>(0x2374554); }
	inline uint32_t &Parity() { return var<uint32_t>(0x2374568); }       // root counter & 1
	inline uint32_t &ArenaBase() { return var<uint32_t>(0x237456C); }    // Magic_TextureOFF_ToEAX1 (setup)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2374570); }
	static const uint32_t Q_ROOT = 0x2368598, Q_FX = 0x23733E8;
	static const uint32_t POINT_A = 0x23685A8, POINT_B = 0x23685B0;     // funnel base: x, y (0), z, radius
	static const uint32_t POOL = 0x23685B8, POOL_END = 0x23693C8;       // 150 x 0x18 sprites
	static const uint32_t PACKETS_A = 0x23693C8, PACKETS_B = 0x236E3C8; // 512 x 0x28 (POLY_FT4) each
	static const uint32_t HEAD_A = 0x23733C8, HEAD_B = 0x23733D8;       // wander offset x, y, z, -, angles x, y, z, -
	static const uint32_t SAVE = 0x2374208, SAVE_SIZE = 0x14;           // per slot: flags, rotation (8), position (8)
	static const uint32_t CENTRE = 0x2374298;                           // targets' centre
	static const uint32_t CASTER_POS = 0x2374558;                       // caster's default position (y / w swapped)
	static const uint32_t RINGS_A = 0x23742A0, RINGS_B = 0x23743F8, RING_SIZE = 0x14; // 17 rings each
	static const uint32_t ARENA_HALF = 0xC000;

	// exe data
	static const uint32_t FUNNEL_Matrix = 0xD45158;   // its translation 0xD4516C = the funnel position
	static const uint32_t SEQ_Sand = 0xD44FA0;
	static const void *const SOUND_Wind = (const void *)0xD4514C;
	static const void *const SOUND_Land = (const void *)0xD45150;

	// battle
	static const uint32_t ENTITY0 = 0x1D972C0, ENTITY_SIZE = 0x9C;
	static const uint32_t CAMERA = 0x1D97778;
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }

	// task functions
	static const uint32_t ORIG_Root = 0x5D1A60;
	static const uint32_t ORIG_Director = 0x5D01B0;
	static const uint32_t ORIG_Funnel = 0x5D0740;
	static const uint32_t ORIG_Sand = 0x5D11F0;
	static const uint32_t ORIG_Lift = 0x5D14B0;
	static const uint32_t ORIG_Landing = 0x5D1820;

	// engine functions (original addresses)
	inline uint32_t Add(uint32_t q, uint32_t task) { return (uint32_t)AddTaskToQueue((TaskQueue *)q, task); }
	inline uint32_t Alloc(int size) { return (uint32_t)FieldAlloc(size); }
	inline uint32_t Entity(int slot) { return ENTITY0 + ENTITY_SIZE * slot; }
	inline int32_t Sin(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D130)(a); }   // computeSin
	inline int32_t Cos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }   // computeCosine
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }  // sqrt: (int)sqrt((double)v)
	inline void ComposeZYX(uint32_t angles, uint32_t m) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x56CE30)(angles, m); }
	inline void Compose(uint32_t a, uint32_t b, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56C2F0)(a, b, out); }
	inline void MatVec(uint32_t m, uint32_t in, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void SetRotW(uint32_t m) { fn<void (__cdecl *)(uint32_t)>(0x56BAE0)(m); }        // GTE_SetRotMatrix_W
	inline void SetTransW(uint32_t m) { fn<void (__cdecl *)(uint32_t)>(0x56BB30)(m); }      // GTE_SetTransVector_W
	inline void LoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void LoadV0(uint32_t a) { fn<void (__cdecl *)(uint32_t)>(0x45DF80)(a); }
	inline void RTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void RTPS() { fn<void (__cdecl *)()>(0x45FAA0)(); }
	inline void AVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void ReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void ReadOTZ(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
	inline void ReadSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); } // GTE_ReadSXY012_Split
	inline void ReadSXY2(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E260)(dst); }
	inline void Insert(uint32_t slot, uint32_t packet) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C7A0)(slot, packet); } // SSIGPU_InsertPrimAutoDepth
	inline void ShadowRotation(uint32_t pos, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(0x571BC0)(pos, a, b); } // TransformCameraByShadowRotation
	inline uint32_t SpriteSeq(uint32_t hdr, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, int32_t, uint32_t)>(0x571C80)(hdr, ot, mode, cursor); } // InitEffectSequenceFromData
	inline void SpawnPosition(uint32_t entity, int32_t bone, int32_t frac, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, uint32_t)>(0x502170)(entity, bone, frac, out); } // GetEffectSpawnPosition
	// CalculateCenterPosition (0x5020A0): centre of the entities of a slot mask (low 16 bits)
	inline void CalculateCenterPosition(uint32_t mask, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x5020A0)(mask, out); }

	// ------------------------------------------------------------------
	// Funnel parameters (0x5D0740): a1 height, a2 radius, a3 sway, a4 radius wave depth, a5 wave
	// frequency, a6 wave phase (the arguments of 0x5D0F30)
	// ------------------------------------------------------------------
	struct FunnelParams { int32_t p[6]; };

	static FunnelParams FunnelPhase(int32_t c, uint32_t node)
	{
		FunnelParams P;
		if (c < 0x1A)
		{
			const int32_t t = shl32(c, 12) / 26;
			P.p[0] = (mul32(t, 200) >> 12) + 0x64;
			P.p[1] = (mul32(t, 500) >> 12) + 0x12C;
			P.p[2] = mul32(t, 100) >> 12;
			P.p[3] = 0x1000;
			P.p[4] = 0;
			P.p[5] = 0;
		}
		else if (c - 0x1A < 0x14)
		{
			const int32_t t = shl32(c - 0x1A, 12) / 20;
			P.p[0] = (mul32(t, 600) >> 12) + 0x12C;
			P.p[1] = (mul32(t, 700) >> 12) + 0x320;
			P.p[2] = (mul32(t, 100) >> 12) + 0x64;
			P.p[3] = (mul32(t, -2176) >> 12) + 0x1000;
			P.p[4] = mul32(t, 1024) >> 12;
			P.p[5] = 0;
		}
		else if (c - 0x2E < 0xA)
		{
			const int32_t u = c - 0x2E;
			const int32_t s = Sin(u * 102 + 0x400);
			const int32_t t = shl32(u, 12) / 10;
			P.p[0] = (mul32(0x1000 - s, 5100) >> 12) + 0x384;
			P.p[1] = (mul32(t, 300) >> 12) + 0x5DC;
			P.p[2] = (mul32(t, 800) >> 12) + 0xC8;
			P.p[3] = (mul32(t, -512) >> 12) + 0x780;
			P.p[4] = (mul32(t, 1024) >> 12) + 0x400;
			P.p[5] = 0;
		}
		else if (c - 0x38 < 0x10)
		{
			const int32_t t = shl32(c - 0x38, 12) / 16;
			P.p[0] = 0x1770;
			P.p[1] = 0x708;
			P.p[2] = 0x3E8;
			P.p[3] = (mul32(t, -256) >> 12) + 0x580;
			P.p[4] = 0x800;
			P.p[5] = mul32(t, 256) >> 12;
		}
		else if (c - 0x48 < 0x10)
		{
			const int32_t t = shl32(c - 0x48, 12) / 16;
			P.p[0] = (mul32(t, 4000) >> 12) + 0x1770;
			P.p[1] = (mul32(t, 2600) >> 12) + 0x708;
			P.p[2] = (mul32(t, -300) >> 12) + 0x3E8;
			P.p[3] = 0x480;
			P.p[4] = (mul32(t, -1024) >> 12) + 0x800;
			P.p[5] = 0x100;
		}
		else
		{
			// never reached (the task ends at 88): the original passes its node pointer for the first four
			// and two never-written locals for the last two
			for (int k = 0; k < 6; k++) P.p[k] = (int32_t)node;
		}
		return P;
	}

	// the funnel's position on a counter (from 26: towards the targets' centre with the sway through
	// the caster's matrix)
	static void FunnelPosition(uint32_t f, int16_t *x, int16_t *z)
	{
		const int16_t c = S16(f, 0xC);
		if (c < 0x1A)
		{
			*x = S16(f, 0x10);
			*z = S16(f, 0x14);
			return;
		}
		const int32_t t = shl32(c - 0x1A, 12) / 38;
		*x = (int16_t)((mul32((int32_t)S16(CENTRE, 0) - S16(f, 0x10), t) >> 12) + S16(f, 0x10));
		*z = (int16_t)((mul32((int32_t)S16(CENTRE, 4) - S16(f, 0x14), t) >> 12) + S16(f, 0x14));
		int16_t v[4];
		v[0] = (int16_t)(mul32(Sin(S16(f, 0x20)), S16(f, 0x1C)) >> 12);
		v[1] = 0;
		v[2] = 0;
		MatVec(Entity(CasterSlot()) + 0x40, (uint32_t)v, (uint32_t)v);
		*x = (int16_t)(*x + v[0]);
		*z = (int16_t)(*z + v[2]);
	}

	// fade: in over 16 ticks, out over the last 8
	inline int32_t FunnelAlpha(int32_t c) { return c < 0x10 ? c * 8 : (c >= 0x50 ? (0x58 - c) << 4 : 0x80); }

	// 0x5D0F30 (second half): ring k from its tilt angles: the sway vector (0x200 + 7/8 * 256 k) * a3
	// tilted, height -(256 k * a1) >> 12, radius a2 * (1 - (1 - a4) * sin(sin(256 k * a5 + a6) / 4)).
	// h = 0x28 bytes of scratch (angles, matrix).
	static void RingPlace(uint32_t ring, int32_t k, const FunnelParams &P, uint32_t h)
	{
		const int32_t t = shl32(k, 12) / 16;
		S16(h, 0) = (int16_t)(mul32(Sin(S16(ring, 8)), 0x120) >> 12);
		S16(h, 2) = (int16_t)(mul32(Sin(S16(ring, 0xA)), 0x120) >> 12);
		S16(h, 4) = (int16_t)(mul32(Sin(S16(ring, 0xC)), 0x120) >> 12);
		ComposeZYX(h, h + 8);
		const int32_t r = (shl32(t * 7, 9) >> 12) + 0x200;
		S16(h, 0) = (int16_t)((int32_t)(0u - (uint32_t)mul32(r, P.p[2])) >> 12);
		S16(h, 2) = 0;
		S16(h, 4) = 0;
		MatVec(h + 8, h, h);
		S16(ring, 0) = S16(h, 0);
		const int32_t hy = mul32(t, P.p[0]) >> 12;
		S16(ring, 2) = (int16_t)((int16_t)(S16(h, 2) >> 4) - hy);
		S16(ring, 4) = S16(h, 4);
		const int32_t ph = (mul32(t, P.p[4]) >> 12) + P.p[5];
		const int32_t s = Sin(Sin(ph) >> 2);
		const int32_t w = 0x1000 - (mul32(0x1000 - P.p[3], s) >> 12);
		S32(ring, 0x10) = mul32(w, P.p[1]) >> 12;
	}

	// 0x5D0C80: the funnel from the 17 rings: 8 vertices per ring at angle + 512 j (+ 0x40 per ring),
	// quad j of ring r between rings r and r - 1, into packet 32 (r - 1) + 2 j + parity
	static void FunnelDraw(uint32_t rings, uint32_t packets, int32_t angle, int32_t alpha)
	{
		const uint32_t h = Alloc(0xA0);
		uint32_t colour = (uint32_t)alpha | 0x2E00;
		colour = (colour << 8) | (uint32_t)alpha;
		colour = (colour << 8) | (uint32_t)alpha;
		int32_t radius = S32(rings, 0x10);
		U32(h, 0) = U32(rings, 0);
		U32(h, 4) = U32(rings, 4);
		for (int32_t j = 0; j < 8; j++)
		{
			const int32_t a = j * 0x200 + angle;
			const uint32_t v = h + 8 + 8 * j;
			S16(v, 0) = (int16_t)((int16_t)(mul32(Cos(a), radius) >> 12) + S16(h, 0));
			S16(v, 2) = S16(h, 2);
			S16(v, 4) = (int16_t)((int16_t)(mul32(Sin(a), radius) >> 12) + S16(h, 4));
		}
		U32(h, 0x48) = U32(h, 8);
		U32(h, 0x4C) = U32(h, 0xC);
		uint32_t par = 0;
		int32_t idx = 0;
		for (uint32_t ring = rings + RING_SIZE; ring < rings + 17 * RING_SIZE; ring += RING_SIZE, idx += 0x20)
		{
			U32(h, 0) = U32(ring, 0);
			radius = S32(ring, 0x10);
			angle += 0x40;
			par ^= 1;
			U32(h, 4) = U32(ring, 4);
			const uint32_t cur = h + par * 0x48 + 8;
			for (int32_t j = 0; j < 8; j++)
			{
				const int32_t a = j * 0x200 + angle;
				const uint32_t v = cur + 8 * j;
				S16(v, 0) = (int16_t)((int16_t)(mul32(Cos(a), radius) >> 12) + S16(h, 0));
				S16(v, 2) = S16(h, 2);
				S16(v, 4) = (int16_t)((int16_t)(mul32(Sin(a), radius) >> 12) + S16(h, 4));
			}
			U32(cur, 0x40) = U32(cur, 0);
			U32(cur, 0x44) = U32(cur, 4);
			const uint32_t prev = h + (par ^ 1) * 0x48 + 8;
			for (int32_t j = 0; j < 8; j++)
			{
				const uint32_t pk = packets + (uint32_t)(idx + 2 * j + (int32_t)Parity()) * 0x28;
				LoadV012(cur + 8 * j, cur + 8 * j + 8, prev + 8 * j);
				RTPT();
				U32(pk, 4) = colour;
				ReadFLAG(h + 0x9C);
				if (U32(h, 0x9C) & 0x60000) continue;
				ReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
				LoadV0(prev + 8 * j + 8);
				RTPS();
				ReadSXY2(pk + 0x20);
				AVSZ4();
				ReadOTZ(h + 0x98);
				Insert(OTBase() + (S32(h, 0x98) >> 2) * 4 + 0x44, pk);
			}
		}
		FieldFree(0xA0);
	}

	// one sand / dust sprite (pool entry e: +0 owner mask, +4 frame, +6 scale, +8 position, +0x10
	// speed x, y, z) with the task's sequence header
	static void SandDraw(uint32_t h, uint32_t e)
	{
		const uint16_t scale = U16(e, 6);
		U16(h, 4) = U16(e, 4);
		ShadowRotation(e + 8, (int32_t)scale, -((int32_t)(int16_t)scale >> 4));
		PacketCursor() = SpriteSeq(h, OTBase() + 0x44, 2, PacketCursor());
	}
}
}

#ifdef FF8_FX_HELD
#include "mag176_sand_storm_adamantoise_held.h"
#endif

namespace ff8fx
{
namespace sandstorm176
{
	// ------------------------------------------------------------------
	// Funnel init (0x5D0550): packets and rings of the selected funnel
	// ------------------------------------------------------------------
	static void FunnelInit()
	{
		// rings 0..7: texture page 0xB9, CLUT 0x3F54
		for (int32_t i = 0; i < 8; i++)
		{
			const uint8_t al = (uint8_t)(i * 31);
			uint8_t top = (uint8_t)(al + 0x1F);
			if (i == 7) top = 0xFF;
			const uint8_t v23 = (uint8_t)(0xFF - al), v01 = (uint8_t)(0xFF - top);
			uint8_t u02 = 0x3F, u13 = 0;
			uint32_t p = PacketSel() + (uint32_t)i * 0x500;
			for (int32_t j = 0; j < 8; j++)
			{
				u02 ^= 0x3F;
				u13 ^= 0x3F;
				for (int32_t k = 0; k < 2; k++, p += 0x28)
				{
					U8(p, 0x1C) = u02;
					U8(p, 0xC) = u02;
					U32(p, 0) = 0x09000000;
					U32(p, 4) = 0x2E808080;
					U8(p, 0x24) = u13;
					U8(p, 0x14) = u13;
					U8(p, 0x15) = v01;
					U8(p, 0xD) = v01;
					U8(p, 0x25) = v23;
					U8(p, 0x1D) = v23;
					U16(p, 0xE) = 0x3F54;
					U16(p, 0x16) = 0xB9;
				}
			}
		}
		// rings 8..15: texture page 0xB8, CLUT 0x3F94
		for (int32_t i = 0; i < 8; i++)
		{
			const uint8_t al = (uint8_t)(i * 31);
			uint8_t top = (uint8_t)(al + 0x1F);
			if (i == 7) top = 0xFF;
			const uint8_t v23 = (uint8_t)(0xFF - al), v01 = (uint8_t)(0xFF - top);
			uint8_t u02 = 0x3F, u13 = 0;
			for (int32_t j = 0; j < 8; j++)
			{
				uint32_t p = PacketSel() + 0x2800 + (uint32_t)(i * 16 + j) * 0x50;
				u02 ^= 0x3F;
				u13 ^= 0x3F;
				for (int32_t k = 0; k < 2; k++, p += 0x28)
				{
					U8(p, 0x15) = v01;
					U8(p, 0xD) = v01;
					U32(p, 0) = 0x09000000;
					U32(p, 4) = 0x2E808080;
					U8(p, 0x1C) = u02;
					U8(p, 0xC) = u02;
					U8(p, 0x24) = u13;
					U8(p, 0x14) = u13;
					U8(p, 0x25) = v23;
					U8(p, 0x1D) = v23;
					U16(p, 0xE) = 0x3F94;
					U16(p, 0x16) = 0xB8;
				}
			}
		}
		uint32_t r = RingSel();
		for (int32_t e = 0; e < 0x6A4; e += 0x64, r += RING_SIZE)
		{
			S16(r, 0) = 0;
			S16(r, 2) = (int16_t)(e / 16);
			S16(r, 4) = 0;
			S32(r, 0x10) = 0x12C;
		}
		const uint32_t w = HeadSel();
		S16(w, 4) = 0;
		S16(w, 2) = 0;
		S16(w, 0) = 0;
		S16(w, 0xC) = 0;
		S16(w, 0xA) = 0;
		S16(w, 8) = 0;
	}

	// ------------------------------------------------------------------
	// Rings (0x5D0F30): rings 16..1 take the previous ring's tilt with a random jitter (-64..63);
	// ring 0 takes the wander offset, which moves along random-walk angles
	// ------------------------------------------------------------------
	static void FunnelRings(const FunnelParams &P)
	{
		const uint32_t h = Alloc(0x28);
		for (int32_t k = 16; k >= 0; k--)
		{
			const uint32_t ring = RingSel() + (uint32_t)k * RING_SIZE;
			if (k > 0)
			{
				for (int m = 0; m < 3; m++)
				{
					const int32_t r = CrtRand() % 128;
					S16(ring, 8 + 2 * m) = (int16_t)(r + S16(ring, 8 + 2 * m - (int32_t)RING_SIZE) - 0x40);
				}
			}
			else
			{
				const uint32_t w = HeadSel();
				S16(ring, 8) = S16(w, 0);
				S16(ring, 0xA) = S16(w, 2);
				S16(ring, 0xC) = S16(w, 4);
				for (int m = 0; m < 3; m++)
				{
					const int32_t r = CrtRand() % 512;
					S16(HeadSel(), 8 + 2 * m) = (int16_t)(S16(HeadSel(), 8 + 2 * m) + (int16_t)(r - 0x100));
				}
				for (int m = 0; m < 3; m++)
					S16(HeadSel(), 2 * m) = (int16_t)(S16(HeadSel(), 2 * m) + (int16_t)(shl32(Sin(S16(HeadSel(), 8 + 2 * m)), 9) >> 12));
			}
			RingPlace(ring, k, P, h);
		}
		FieldFree(0x28);
	}

	// ------------------------------------------------------------------
	// Funnel (0x5D0740): node +0x0C counter, +0x0E funnel (0 = A, 1 = B), +0x10 bone position,
	// +0x1C sway amplitude, +0x1E its growth, +0x20 sway phase, +0x22 its speed
	// ------------------------------------------------------------------
	static uint32_t __cdecl FunnelTask(TaskNode *n)
	{
		const uint32_t f = (uint32_t)n;
		const FunnelParams P = FunnelPhase(S16(f, 0xC), f);
		int16_t x, z;
		FunnelPosition(f, &x, &z);
		if (S16(f, 0xC) >= 0x1A && !DrawOnly())
		{
			S16(f, 0x1C) = (int16_t)(S16(f, 0x1C) + S16(f, 0x1E));
			const int16_t pv = S16(f, 0x22);
			S16(f, 0x20) = (int16_t)(S16(f, 0x20) + pv);
			S16(f, 0x22) = (int16_t)(pv - (int16_t)(pv >> 5));
		}
		if (S16(f, 0xE) != 0)
		{
			RingSel() = RINGS_B;
			PacketSel() = PACKETS_B;
			HeadSel() = HEAD_B;
			S16(POINT_B, 0) = x;
			S16(POINT_B, 4) = z;
			S16(POINT_B, 6) = (int16_t)P.p[1];
		}
		else
		{
			RingSel() = RINGS_A;
			PacketSel() = PACKETS_A;
			HeadSel() = HEAD_A;
			S16(POINT_A, 0) = x;
			S16(POINT_A, 4) = z;
			S16(POINT_A, 6) = (int16_t)P.p[1];
		}
		FunnelRings(P);
		S32(FUNNEL_Matrix, 0x14) = x;
		S32(FUNNEL_Matrix, 0x18) = 0;
		S32(FUNNEL_Matrix, 0x1C) = z;
		Mat4x3 m;
		Compose(CAMERA, FUNNEL_Matrix, (uint32_t)&m);
		SetRotW((uint32_t)&m);
		SetTransW((uint32_t)&m);
		const int16_t c = S16(f, 0xC);
		const int32_t alpha = FunnelAlpha(c);
		int32_t angle = c * 100;
		if (S16(f, 0xE) != 0)
		{
			RingSel() = RINGS_B;
			PacketSel() = PACKETS_B;
			HeadSel() = HEAD_B;
			angle = -angle;
		}
		else
		{
			RingSel() = RINGS_A;
			PacketSel() = PACKETS_A;
			HeadSel() = HEAD_A;
		}
		// 30 fps layer: see mag176_sand_storm_adamantoise_held.inc
		FX_HELD(held_note_funnel(f, x, z);)
		FunnelDraw(RingSel(), PacketSel(), angle, alpha);
		if (DrawOnly()) return 0;
		S16(f, 0xC) = (int16_t)(S16(f, 0xC) + 1);
		return S16(f, 0xC) >= 0x58 ? TASK_END : 0;
	}

	// first free pool entry (nullptr when the pool is full)
	static uint32_t FreeSand()
	{
		int32_t i = 0;
		uint32_t e = POOL;
		while (U32(e, 0) != 0)
		{
			e += 0x18;
			i++;
			if (e >= POOL_END) return 0;
		}
		return i < 0x96 ? POOL + (uint32_t)i * 0x18 : 0;
	}

	// ------------------------------------------------------------------
	// Sand (0x5D11F0): node +0x0C counter, +0x0E emitter mask (1 = funnel A, 2 = funnel B)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SandTask(TaskNode *n)
	{
		const uint32_t t = (uint32_t)n;
		const uint32_t h = Alloc(0xB4);
		U32(h, 0) = SEQ_Sand;
		S16(h, 0x24) = 0;
		int32_t alive = 0;
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
		{
			if (!(U32(e, 0) & (uint32_t)(int32_t)S16(t, 0xE))) continue;
			// 30 fps layer: see mag176_sand_storm_adamantoise_held.inc
			FX_HELD(held_note_sand(t, h, e);)
			SandDraw(h, e);
			if (DrawOnly()) continue;
			S16(e, 4) = (int16_t)(S16(e, 4) + 1);
			if (S16(h, 0x28) < 0)
			{
				U32(e, 0) = 0;
				continue;
			}
			S16(e, 8) = (int16_t)(S16(e, 8) + S16(e, 0x10));
			S16(e, 0xA) = (int16_t)(S16(e, 0xA) + S16(e, 0x12));
			S16(e, 0xC) = (int16_t)(S16(e, 0xC) + S16(e, 0x14));
			S16(e, 0x10) = (int16_t)(S16(e, 0x10) - (int16_t)(S16(e, 0x10) >> 3));
			S16(e, 0x12) = (int16_t)(S16(e, 0x12) + (int16_t)(S16(e, 0x12) >> 4));
			S16(e, 0x14) = (int16_t)(S16(e, 0x14) - (int16_t)(S16(e, 0x14) >> 3));
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(t, 0xC);
		if (c >= 0 && c <= 0x50)
		{
			const int32_t count = (c >> 5) + 1;
			const uint32_t pt = S16(t, 0xE) == 1 ? POINT_A : POINT_B;
			for (int32_t k = 0; k < count; k++)
			{
				const uint32_t e = FreeSand();
				if (!e) break;
				U32(e, 0) = (uint32_t)(int32_t)S16(t, 0xE);
				S16(e, 4) = 0;
				const int32_t r = CrtRand() % 0x1400;
				S16(e, 6) = (int16_t)(r + (int16_t)(S16(t, 0xC) + 0x300) * 2);
				U32(e, 8) = U32(pt, 0);
				U32(e, 0xC) = U32(pt, 4);
				const int32_t a = CrtRand() % 4096;
				const int32_t d = CrtRand() % 300 + S16(pt, 6);
				S16(e, 8) = (int16_t)(S16(e, 8) + (int16_t)(mul32(Cos(a), d) >> 12));
				S16(e, 0xC) = (int16_t)(S16(e, 0xC) + (int16_t)(mul32(Sin(a), d) >> 12));
				const int32_t s = CrtRand() % 50 + 0x14;
				S16(e, 0x10) = (int16_t)(mul32(Cos(a), s) >> 12);
				S16(e, 0x12) = (int16_t)(-10 - CrtRand() % 10);
				S16(e, 0x14) = (int16_t)(mul32(Sin(a), s) >> 12);
			}
		}
		S16(t, 0xC) = (int16_t)(S16(t, 0xC) + 1);
		if (S16(t, 0xC) >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Lift (0x5D14B0): node +0x0C counter, +0x0E target slot, +0x10 its bone 0xF1 position, +0x18
	// spin (0x1000 / 0x2000), +0x1A target index, +0x1C 1 = drawn towards funnel B, +0x1E delay,
	// +0x20 lift height, +0x22 its speed
	// ------------------------------------------------------------------
	static uint32_t __cdecl LiftTask(TaskNode *n)
	{
		const uint32_t l = (uint32_t)n;
		const int16_t delay = S16(l, 0x1E);
		if (delay > 0)
		{
			if (DrawOnly()) return 0;
			S16(l, 0x1E) = (int16_t)(delay - 1);
			return 0;
		}
		const int16_t c = S16(l, 0xC);
		const int32_t slot = S16(l, 0xE);
		const uint32_t ent = Entity(slot), save = SAVE + (uint32_t)slot * SAVE_SIZE;
		if (c == 0)
		{
			U16(save, 0) = (uint16_t)(U16(ent, 0) & 0x1020);
			U32(save, 0xC) = U32(ent, 0x1C);
			U32(save, 0x10) = U32(ent, 0x20);
			U32(save, 4) = U32(ent, 0xC);
			U32(save, 8) = U32(ent, 0x10);
		}
		if (c < 0x20)
		{
			U16(ent, 0) |= 0x1020;
			S16(ent, 0xC) = (int16_t)(mul32(S16(l, 0x18), c) / 44);
			S16(ent, 0xE) = (int16_t)(shl32(c, 12) / 32);
			int16_t v[4];
			v[0] = (int16_t)(S16(l, 0x10) - S16(save, 0xC));
			v[1] = (int16_t)(S16(l, 0x12) - S16(save, 0xE));
			v[2] = (int16_t)(S16(l, 0x14) - S16(save, 0x10));
			Mat4x3 m;
			ComposeZYX(ent + 0xC, (uint32_t)&m);
			MatVec((uint32_t)&m, (uint32_t)v, (uint32_t)v);
			S16(ent, 0x1C) = (int16_t)(S16(l, 0x10) - v[0]);
			S16(ent, 0x1E) = (int16_t)(S16(l, 0x12) - v[1]);
			S16(ent, 0x20) = (int16_t)(S16(l, 0x14) - v[2]);
			const uint32_t pt = S16(l, 0x1C) != 0 ? POINT_B : POINT_A;
			const int32_t frac = S16(ent, 0xE);
			S16(ent, 0x1C) = (int16_t)(S16(ent, 0x1C) + (int16_t)(mul32((int32_t)S16(pt, 0) - S16(l, 0x10), frac) >> 12));
			S16(ent, 0x1E) = (int16_t)(S16(ent, 0x1E) + S16(l, 0x20));
			S16(ent, 0x20) = (int16_t)(S16(ent, 0x20) + (int16_t)(mul32((int32_t)S16(pt, 4) - S16(l, 0x14), frac) >> 12));
		}
		else if (c < 0x2C)
		{
			if (c == 0x20) S16(l, 0x20) = (int16_t)(S16(l, 0x20) / 8);
		}
		else
		{
			S16(ent, 0x1C) = S16(save, 0xC);
			S16(ent, 0x1E) = (int16_t)(S16(ent, 0x1E) - S16(l, 0x20));
			S16(ent, 0x20) = S16(save, 0x10);
		}
		if (DrawOnly()) return 0;
		const int16_t c0 = S16(l, 0xC);
		if (c0 < 0x20)
		{
			const int16_t v = S16(l, 0x22);
			S16(l, 0x20) = (int16_t)(S16(l, 0x20) + v);
			S16(l, 0x22) = (int16_t)(v / 12 + v);
			if (S16(l, 0x20) < -10000) S16(l, 0x20) = -10000;
		}
		S16(l, 0xC) = (int16_t)(c0 + 1);
		if (S16(l, 0xC) < 0x34) return 0;
		U16(ent, 0) = (uint16_t)(U16(save, 0) | (U16(ent, 0) & 0xEFDF));
		U32(ent, 0x1C) = U32(save, 0xC);
		U32(ent, 0x20) = U32(save, 0x10);
		U32(ent, 0xC) = U32(save, 4);
		U32(ent, 0x10) = U32(save, 8);
		const uint32_t d = Add(Q_FX, ORIG_Landing);
		const int16_t index = S16(l, 0x1A);
		U32(d, 0x10) = U32(save, 0xC);
		U32(d, 0x14) = U32(save, 0x10);
		S16(d, 0xC) = 0;
		S16(d, 0xE) = (int16_t)(0x100 << (index & 0x1F));
		S16(d, 0x1A) = index;
		BdPlaySE(SOUND_Land, 0, 0x80);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Landing dust (0x5D1820): node +0x0C counter, +0x0E sprite mask (0x100 << target index), +0x10
	// landing position, +0x1A target index
	// ------------------------------------------------------------------
	static uint32_t __cdecl LandingTask(TaskNode *n)
	{
		const uint32_t t = (uint32_t)n;
		const uint32_t h = Alloc(0xB4);
		int32_t alive = 0;
		U32(h, 0) = SEQ_Sand;
		S16(h, 0x24) = 0;
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
		{
			if (!(U32(e, 0) & (uint32_t)(int32_t)S16(t, 0xE))) continue;
			// 30 fps layer: see mag176_sand_storm_adamantoise_held.inc
			FX_HELD(held_note_sand(t, h, e);)
			SandDraw(h, e);
			if (DrawOnly()) continue;
			S16(e, 4) = (int16_t)(S16(e, 4) + 1);
			if (S16(h, 0x28) < 0)
			{
				U32(e, 0) = 0;
				continue;
			}
			S16(e, 8) = (int16_t)(S16(e, 8) + S16(e, 0x10));
			S16(e, 0xC) = (int16_t)(S16(e, 0xC) + S16(e, 0x14));
			S16(e, 0x10) = (int16_t)(S16(e, 0x10) - (int16_t)(S16(e, 0x10) >> 3));
			S16(e, 0x14) = (int16_t)(S16(e, 0x14) - (int16_t)(S16(e, 0x14) >> 3));
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(t, 0xC);
		if (c >= 0 && c <= 2)
		{
			for (int32_t k = 0; k < 6; k++)
			{
				const uint32_t e = FreeSand();
				if (!e) break;
				U32(e, 0) = (uint32_t)(int32_t)S16(t, 0xE);
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % 0x500 + 0x400);
				U32(e, 8) = U32(t, 0x10);
				U32(e, 0xC) = U32(t, 0x14);
				const int32_t a = CrtRand() % 4096;
				const int32_t s = CrtRand() % 50 + 0x14;
				S16(e, 0x10) = (int16_t)(mul32(Cos(a), s) >> 12);
				S16(e, 0x14) = (int16_t)(mul32(Sin(a), s) >> 12);
			}
		}
		if (S16(t, 0xC) == 2) ApplyActionResultToTarget(Ctx()->actions->targets + S16(t, 0x1A) * TARGET_STRIDE);
		S16(t, 0xC) = (int16_t)(S16(t, 0xC) + 1);
		if (S16(t, 0xC) >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5D01B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		if (DrawOnly()) return 0;
		const uint32_t d = (uint32_t)n;
		if (S16(d, 0xC) == 0)
		{
			GetDefaultEffectPosition((uint8_t *)Entity(CasterSlot()), (int16_t *)CASTER_POS);
			const int16_t y = S16(CASTER_POS, 2);
			S16(CASTER_POS, 2) = S16(CASTER_POS, 6);
			S16(CASTER_POS, 6) = y;
			CalculateCenterPosition(Ctx()->target_mask, CENTRE);
			RingSel() = RINGS_A;
			PacketSel() = PACKETS_A;
			HeadSel() = HEAD_A;
			FunnelInit();
			RingSel() = RINGS_B;
			PacketSel() = PACKETS_B;
			HeadSel() = HEAD_B;
			FunnelInit();
		}
		if (S16(d, 0xC) == 0xC)
		{
			const uint32_t a = Add(Q_FX, ORIG_Funnel);
			S16(a, 0xC) = 0;
			S16(a, 0xE) = 0;
			S16(a, 0x1C) = (int16_t)(CrtRand() % 100 - 50);
			const int32_t g = CrtRand() % 40 + 0x14;
			S16(a, 0x1E) = (int16_t)g;
			if (S16(a, 0x1C) < 0) S16(a, 0x1E) = (int16_t)-g;
			S16(a, 0x20) = 0;
			S16(a, 0x22) = (int16_t)(CrtRand() % 150 + 0x96);
			SpawnPosition(Entity(CasterSlot()), 0x15, 0x1000, a + 0x10);
			const uint32_t b = Add(Q_FX, ORIG_Funnel);
			S16(b, 0x1C) = (int16_t)-S16(a, 0x1C);
			S16(b, 0xC) = 0;
			S16(b, 0xE) = 1;
			S16(b, 0x1E) = (int16_t)-S16(a, 0x1E);
			S16(b, 0x20) = 0;
			S16(b, 0x22) = S16(a, 0x22);
			SpawnPosition(Entity(CasterSlot()), 0x16, 0x1000, b + 0x10);
			S16(POINT_B, 2) = 0;
			S16(POINT_A, 2) = 0;
			S16(POINT_B, 6) = 0x12C;
			S16(POINT_A, 6) = 0x12C;
			if (S16(d, 0xC) == 0xC)
			{
				const uint32_t s1 = Add(Q_FX, ORIG_Sand);
				S16(s1, 0xC) = 0;
				S16(s1, 0xE) = 1;
				const uint32_t s2 = Add(Q_FX, ORIG_Sand);
				S16(s2, 0xC) = 0;
				S16(s2, 0xE) = 2;
			}
		}
		if (S16(d, 0xC) == 0x39)
		{
			for (int32_t i = 0; i < (int32_t)Ctx()->actions->target_count; i++)
			{
				const int32_t slot = Ctx()->actions->targets[i * TARGET_STRIDE];
				const uint32_t l = Add(Q_FX, ORIG_Lift);
				S16(l, 0xC) = 0;
				S16(l, 0xE) = (int16_t)slot;
				const int32_t r = CrtRand() % 2 + 1;
				S16(l, 0x1A) = (int16_t)i;
				S16(l, 0x18) = (int16_t)shl32(r, 12);
				S16(l, 0x20) = 0;
				const int32_t r2 = CrtRand() % 65;
				S16(l, 0x22) = (int16_t)(-45 - r2);
				SpawnPosition(Entity(slot), 0xF1, 0, l + 0x10);
				int32_t dx = (int32_t)S16(POINT_A, 0) - S16(l, 0x10), dz = (int32_t)S16(POINT_A, 4) - S16(l, 0x14);
				const int32_t da = Sqrt(mul32(dz, dz) + mul32(dx, dx));
				dx = (int32_t)S16(POINT_B, 0) - S16(l, 0x10);
				dz = (int32_t)S16(POINT_B, 4) - S16(l, 0x14);
				const int32_t db = Sqrt(mul32(dz, dz) + mul32(dx, dx));
				S16(l, 0x1C) = (int16_t)(da < db ? 1 : 0);
				S16(l, 0x1E) = (int16_t)(CrtRand() % 6);
			}
		}
		if (S16(d, 0xC) == 0xE) BdPlaySE(SOUND_Wind, 0, 0x80);
		S16(d, 0xC) = (int16_t)(S16(d, 0xC) + 1);
		return S16(d, 0xC) > 0x78 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root (0x5D1A60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		const uint32_t r = (uint32_t)n;
		// 30 fps layer: see mag176_sand_storm_adamantoise_held.inc
		FX_HELD(held_note_root();)
		Parity() = U8(r, 0xC) & 1;
		PacketCursor() = Parity() ? ArenaBase() + ARENA_HALF : ArenaBase();
		const int a = ExecuteTaskQueue((TaskQueue *)Q_FX);
		S16(r, 0xC) = (int16_t)(S16(r, 0xC) + 1);
		return a ? 0 : TASK_END;
	}
}

	void register_mag176_sand_storm_adamantoise()
	{
		register_port(sandstorm176::ORIG_Root, (void *)sandstorm176::RootTask, "176 RootTask", 176);
		register_port(sandstorm176::ORIG_Director, (void *)sandstorm176::DirectorTask, "176 DirectorTask", 176);
		register_port(sandstorm176::ORIG_Funnel, (void *)sandstorm176::FunnelTask, "176 FunnelTask", 176);
		register_port(sandstorm176::ORIG_Sand, (void *)sandstorm176::SandTask, "176 SandTask", 176);
		register_port(sandstorm176::ORIG_Lift, (void *)sandstorm176::LiftTask, "176 LiftTask", 176);
		register_port(sandstorm176::ORIG_Landing, (void *)sandstorm176::LandingTask, "176 LandingTask", 176);
		// 30 fps layer: see mag176_sand_storm_adamantoise_held.inc
		FX_HELD(register_mag176_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag176_sand_storm_adamantoise_held.inc"
#endif
