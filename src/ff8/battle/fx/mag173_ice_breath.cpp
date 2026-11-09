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

// Effect 173: Ice Breath (enemy attack 38 of kernel.bin, Snow Lion c0m013; the Cronos mod's battle
// item 20 "Arctic Wind" plays it from a party member on all enemies) (MAG_173_*).
//
// Structure (setup MAG_173_ICE_BREATH_Init 0x5D74B0, file loader MAG_173_ICE_BREATH_FL 0x5D7490 =
// texture 0xD61E1C (mag172.tim) into 0x2398A9C; the setup keeps the texture base 0x239A8C8, the cast
// context 0x2398A78, the first target's slot 0x239727C, the CASTER slot 0x2398A98, queues the root
// (queue 0x23972D0, pool 0x23972B0, 2 x 0x10) and the director (queue 0x2397C40, pool 0x2397C50,
// 0x64 x 0x24), clears the particle table, plays camera animation 0xD5E67C and uploads the texture):
//   Root (0x5D9BF0) - packet cursor 0x239A8CC = texture base (+0xC000 on odd ticks), runs the director
//     queue then the crystal player queue 0x239A8D0; when the director queue is empty queues the
//     texture restore (0x5D9C70) and ends.
//   Director (0x5D7570), 0x60 ticks; EVERY tick, draw-only mode included: the anchor 0x2398AA0 = the
//     caster's bone 0xF0 (0x502170) + (0, 0, -0x96) through the caster's rotation, the midpoint
//     0x2398A90 (x / z half way to the targets' centre 0x2398A60, y 0), the floor direction 0x2398A68
//     (anchor -> midpoint, normalised) and its length 0x2397278 (sqrt of 0x56BC50's result); then
//     (nothing more in the draw-only mode, battle_to_update_flags & 0x201):
//       1     targets' centre (0x5020A0), crystal player queue (pool texture base + 0x18000, 0x40 x
//             0x11C), sound 0xD61D84
//       2     mouth swirl (0x5D7BA0)
//       8     mouth glow (0x5D7FE0)
//       0xE   four breath cones (0x5D7E70, delays 0xD61D90, z angles 0xD61DA0)
//       0x24  sound 0xD61D88; 0x25 breath puffs (0x5D80C0); 0x26 crystal spawner (0x5D8E20)
//       0x46  five ice-trail meshes (0x5D83E0, meshes 0xD61DF0): their polygon states (0xD61E04, in
//             the module globals) built (0x5D7980 / 0x5D79C0)
//       0x4A  ice shards (0x5D8B40); 0x55 damage (every target record of action 0)
//       screen flash 0x502020: level tick * 0xC0 up to tick 0x10, (0x5F - tick) * 0x180 from 0x57;
//       0x60  flash off, ends.
//   Particle table 0x23972E0 (100 x 0x18, kind dword +0: 1 swirl, 2 puff, 4 shard; +4 frame):
//   Mouth swirl (0x5D7BA0, ticks 0..12: three a tick): +6 size, +8 axis, +0xE angle, +0x10 radius,
//     +0x12 radius step, +0x16 angle step; drawn at anchor + the radius vector (0, 0, r) turned about
//     the axis (sprite 0xD5EAA4); ends at tick 0x24 (its entries cleared).
//   Breath puffs (0x5D80C0, ticks 0..0x1E: two a tick): +6 size (+1/16 a tick), +8 position, +0x10
//     velocity (-1/32 a tick; y only below -300); spawned at the anchor along the floor direction
//     turned by the task's sweep angle (+0x18 by +0x1A); sprite 0xD5E74C, colour 0xD61DB0 by frame.
//   Ice shards (0x5D8B40, ticks 0..2: seven a tick): around the midpoint, flying away from the
//     task's spawn point (+0xE speed, +0x10 unit direction), sprite 0xD5E8F8 stretched along it.
//   Mouth glow (0x5D7FE0): prim model 0xD5F710 facing the camera at the anchor, growing; 0x24 ticks.
//   Breath cones (0x5D7E70): prim model 0xD5ED20 at the anchor of tick 0xE, caster facing, shrinking,
//     fading from its tick 12; 16 ticks.
//   Crystal spawner (0x5D8E20, 0x1F ticks, nothing in the draw-only mode): one crystal player a tick
//     (layout 0xD61AD4 decoded by 0x5D8FB0) along its own sweep of the floor direction.
//   Crystal players (0x5D90B0, queue 0x239A8D0): the module's prim-model player (0x5D95B0, the shared
//     player 0x701970 reading its pause from battle_to_update_flags bit 0: ff8fx::prim::play) with
//     the draw callback 0x5D9130 (rotation 0x5D93A0, z spin + uniform scale from the node, vertex
//     frames blended by 0x5D9510 into texture base + 0x1C700, object flags 0xD61E18).
//   Ice trail (0x5D83E0, x5): mesh 0xD61DF0[i] at the anchor of tick 0x46 on the floor, turned to the
//     targets; ticks 0..9 its polygons appear by their delay (0x5D8920 / 0x5D8990), from tick 10 they
//     fly apart (0x5D8520 / 0x5D8590: offset, velocity -1/16 a tick, spin, depth cue by phase,
//     16 ticks each); ends when none is drawn.
// Every task draws THEN updates; the pool tasks, the trail, the glow and the cones skip their update
// in the draw-only mode (the cones also their delay count).
// The effect writes no entity field and no camera word (the setup's camera animation drives it).
// Module globals: 0x2397278..0x239A8E0 (floor length, first target, root queue + pool, particle
// table, director queue + pool, centre, direction, context, swept direction 0x2398A80, midpoint,
// caster slot, texture file, anchor, the five polygon state tables 0x2398AA8..0x239A8C8, texture
// base, packet cursor, crystal player queue).

#include "mag_common.h"

namespace ff8fx
{
namespace ib173
{
	using namespace eng;
	using namespace magc;

	template<typename T> static inline T &F(uint32_t base, int32_t off) { return *(T *)(base + off); }
	static inline int16_t &S16(uint32_t p, int32_t o) { return F<int16_t>(p, o); }
	static inline uint16_t &U16(uint32_t p, int32_t o) { return F<uint16_t>(p, o); }
	static inline int32_t &S32(uint32_t p, int32_t o) { return F<int32_t>(p, o); }
	static inline uint32_t &U32(uint32_t p, int32_t o) { return F<uint32_t>(p, o); }
	static inline uint8_t &U8(uint32_t p, int32_t o) { return F<uint8_t>(p, o); }

	// --- module globals ---
	inline int32_t &FloorLength() { return var<int32_t>(0x2397278); }          // |anchor -> midpoint|
	inline TaskQueue *RootQueue() { return (TaskQueue *)0x23972D0; }           // pool 0x23972B0, 2 x 0x10
	inline TaskQueue *FxQueue() { return (TaskQueue *)0x2397C40; }             // pool 0x2397C50, 0x64 x 0x24
	inline TaskQueue *PlayerQueue() { return (TaskQueue *)0x239A8D0; }         // pool texture base + 0x18000, 0x40 x 0x11C
	static const uint32_t CENTRE = 0x2398A60;      // targets' centre (x, y, z words)
	static const uint32_t DIR = 0x2398A68;         // anchor -> midpoint on the floor (3 x int32, normalised)
	static const uint32_t SWEPT = 0x2398A80;       // DIR turned by the puff task's sweep (3 x int32)
	static const uint32_t MID = 0x2398A90;         // midpoint (x, 0, z, pad words)
	static const uint32_t ANCHOR = 0x2398AA0;      // caster's mouth (x, y, z, pad words)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2398A78); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x2398A98); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x239A8C8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x239A8CC); }
	inline uint32_t OT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline uint32_t Ent(int32_t slot) { return 0x1D972C0 + (uint32_t)(slot * 0x9C); }
	inline bool PlayerPaused() { return (var<uint8_t>(0x1D96A9C) & 1) != 0; } // the module's player tests bit 0 only

	static const uint32_t POOL = 0x23972E0, POOL_END = 0x2397C40;   // 100 x 0x18, kind dword +0

	static const uint32_t ORIG_Director = 0x5D7570;
	static const uint32_t ORIG_Swirl = 0x5D7BA0;
	static const uint32_t ORIG_Cone = 0x5D7E70;
	static const uint32_t ORIG_Glow = 0x5D7FE0;
	static const uint32_t ORIG_Puffs = 0x5D80C0;
	static const uint32_t ORIG_Trail = 0x5D83E0;
	static const uint32_t ORIG_Shards = 0x5D8B40;
	static const uint32_t ORIG_Spawner = 0x5D8E20;
	static const uint32_t ORIG_Crystal = 0x5D90B0;
	static const uint32_t ORIG_Root = 0x5D9BF0;
	static const uint32_t ORIG_TexRestore = 0x5D9C70;

	static const uint32_t SEQ_Swirl = 0xD5EAA4, SEQ_Puff = 0xD5E74C, SEQ_Shard = 0xD5E8F8;
	static const uint32_t MODEL_Cone = 0xD5ED20, MODEL_Glow = 0xD5F710;
	static const uint32_t LAYOUT_Crystal = 0xD61AD4;
	static const uint32_t CRYSTAL_FLAGS = 0xD61E18;  // object flag table of the crystal layout
	static const uint32_t CONE_DELAYS = 0xD61D90;    // 4 words, stride 4
	static const uint32_t CONE_ANGLES = 0xD61DA0;    // 4 words (z angle), stride 4
	static const uint32_t PUFF_COLOURS = 0xD61DB0;   // header +0x1C by frame (16 entries)
	static const uint32_t TRAIL_MESHES = 0xD61DF0;   // 5 mesh pointers
	static const uint32_t TRAIL_STATES = 0xD61E04;   // 5 polygon state tables (0x20 per polygon)
	static const void *const SOUND_Start = (const void *)0xD61D84;
	static const void *const SOUND_Blast = (const void *)0xD61D88;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint32_t entity, int32_t bone, int32_t flag, uint32_t out) { return fn<int32_t (__cdecl *)(uint32_t, int32_t, int32_t, uint32_t)>(0x502170)(entity, bone, flag, out); }
	inline void CalculateCenterPosition(uint32_t mask, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x5020A0)(mask, out); } // low 16 bits of the mask
	inline int32_t NormalizeVectorToFixedPoint(uint32_t in, uint32_t out) { return fn<int32_t (__cdecl *)(uint32_t, uint32_t)>(0x56BC50)(in, out); }
	inline int32_t VectorNormalS16(uint32_t in, uint32_t out) { return fn<int32_t (__cdecl *)(uint32_t, uint32_t)>(0x56BDE0)(in, out); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }           // Math_SqrtInt
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void MatrixMulVector(uint32_t m, uint32_t in, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector (SVECTOR)
	inline void MatrixMultiply3(uint32_t a, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x56C220)(a, b); }    // a = a * b (3x3)
	inline void Matrix3x3MultiplyPSX(uint32_t a, uint32_t b, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56C090)(a, b, out); }
	inline void TextureRestore(uint32_t tex, uint32_t done) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x508630)(tex, done); } // Battle_RestoreDefaultEffectTexture
	// software GTE
	inline void GteSetFarColour(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); }
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void GteReadSXY012Split(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteStoreOTZ(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }   // GTE_ReadOTZ
	inline void GteLoadRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E120)(a, b, c); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E370)(a, b, c); }

	// first free table entry (kind dword 0), or 0 when the table is full
	static uint32_t PoolFree()
	{
		uint32_t e = POOL;
		int32_t i = 0;
		while (S32(e, 0) != 0)
		{
			e += 0x18;
			i++;
			if (e >= POOL_END) return 0;
		}
		return i < 100 ? e : 0;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag173_ice_breath_held.h"
#endif

namespace ff8fx
{
namespace ib173
{
	// ------------------------------------------------------------------
	// Table sprites (0x571C80 sequences)
	// ------------------------------------------------------------------
	// a mouth swirl (0x5D7BA0): w + 8 = the anchor; the radius vector (0, 0, +0x10) turned by angle +0xE
	// about the axis +8 (matrix at w + 0x10), moved to the anchor, pushed toward the camera by size / 16
	static void SwirlDraw(uint32_t h, uint32_t w, uint32_t e)
	{
		S16(h, 4) = S16(e, 4);
		S16(w, 4) = S16(e, 0x10);
		S32(w, 0x34) = S16(e, 0xA);
		const int32_t angle = S16(e, 0xE);
		S32(w, 0x30) = S16(e, 8);
		S16(w, 0) = 0;
		S16(w, 2) = 0;
		S32(w, 0x38) = S16(e, 0xC);
		BuildAxisAngleRotationMatrix(angle, (Mat4x3 *)(w + 0x10), (const int32_t *)(w + 0x30));
		MatrixMulVector(w + 0x10, w, w);
		S16(w, 0) = (int16_t)(S16(w, 0) + S16(w, 8));
		S16(w, 2) = (int16_t)(S16(w, 2) + S16(w, 0xA));
		S16(w, 4) = (int16_t)(S16(w, 4) + S16(w, 0xC));
		const int16_t size = S16(e, 6);
		TransformCameraByShadowRotation((const void *)w, size, -(size >> 4));
		PacketCursor() = InitEffectSequenceFromData((void *)h, OT44(), 2, PacketCursor());
	}

	// a breath puff (0x5D80C0): colour by frame, position +8, pushed toward the camera by size / 16
	static void PuffDraw(uint32_t h, uint32_t e)
	{
		const uint16_t frame = U16(e, 4);
		S16(h, 4) = (int16_t)frame;
		U32(h, 0x1C) = U32(PUFF_COLOURS + (uint32_t)frame * 4, 0);
		const int16_t size = S16(e, 6);
		TransformCameraByShadowRotation((const void *)(e + 8), size, -(size >> 4));
		PacketCursor() = InitEffectSequenceFromData((void *)h, OT44(), 2, PacketCursor());
	}

	// an ice shard (0x5D8B40): w + 8 = identity, w + 0x58 = (-0x1000, 0, 0); the sprite turned from
	// -x onto its direction +0x10, stretched (size, size / 2, size / 2), at its position +8
	static void ShardDraw(uint32_t h, uint32_t w, uint32_t e)
	{
		S32(w, 0x3C) = S16(e, 8);
		S32(w, 0x40) = S16(e, 0xA);
		S32(w, 0x44) = S16(e, 0xC);
		S32(w, 0x68) = S16(e, 0x10);
		S32(w, 0x6C) = S16(e, 0x12);
		S32(w, 0x70) = S16(e, 0x14);
		const int32_t angle = RotationBetweenVectors((const int32_t *)(w + 0x58), (const int32_t *)(w + 0x68), (int32_t *)(w + 0x78));
		BuildAxisAngleRotationMatrix(angle, (Mat4x3 *)(w + 0x28), (const int32_t *)(w + 0x78));
		MatrixMultiply3(w + 0x28, w + 8);
		const int32_t size = S16(e, 6);
		S32(w, 0x48) = size;
		S32(w, 0x50) = size >> 1;
		S32(w, 0x4C) = size >> 1;
		Scale3DMatrix((Mat4x3 *)(w + 0x28), (const int32_t *)(w + 0x48));
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(w + 0x28), (Mat4x3 *)(w + 0x28));
		GteSetRotMatrix((const Mat4x3 *)(w + 0x28));
		GteSetTransVector((const Mat4x3 *)(w + 0x28));
		S16(h, 4) = S16(e, 4);
		PacketCursor() = InitEffectSequenceFromData((void *)h, OT44(), 2, PacketCursor());
	}

	// motion after a draw (frame + 1 first; the entry ends when its sequence did: header +0x28 < 0)
	static void SwirlMove(uint32_t e)
	{
		S16(e, 0xE) = (int16_t)(S16(e, 0xE) + S16(e, 0x16));
		S16(e, 0x10) = (int16_t)(S16(e, 0x10) - S16(e, 0x12));
	}

	static void PuffMove(uint32_t e)
	{
		const int16_t s = S16(e, 6);
		S16(e, 6) = (int16_t)(s + (int16_t)(s >> 4));
		S16(e, 8) = (int16_t)(S16(e, 8) + S16(e, 0x10));
		S16(e, 0xC) = (int16_t)(S16(e, 0xC) + S16(e, 0x14));
		const int16_t vx = S16(e, 0x10);
		S16(e, 0x10) = (int16_t)(vx - (int16_t)(vx >> 5));
		const int16_t vz = S16(e, 0x14);
		S16(e, 0x14) = (int16_t)(vz - (int16_t)(vz >> 5));
		const int16_t y = S16(e, 0xA);
		if (y < -300)
		{
			S16(e, 0xA) = (int16_t)(y + S16(e, 0x12));
			const int16_t vy = S16(e, 0x12);
			S16(e, 0x12) = (int16_t)(vy - (int16_t)(vy >> 5));
		}
	}

	static void ShardMove(uint32_t w, uint32_t e)
	{
		const int32_t speed = S16(e, 0xE);
		S16(e, 8) = (int16_t)(S16(e, 8) + (int16_t)(mul32(speed, S32(w, 0x68)) >> 12));
		S16(e, 0xC) = (int16_t)(S16(e, 0xC) + (int16_t)(mul32(speed, S32(w, 0x70)) >> 12));
	}

	// ------------------------------------------------------------------
	// Mouth swirl (0x5D7BA0): node +0xC counter
	// ------------------------------------------------------------------
	static uint32_t __cdecl SwirlTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		const uint32_t w = (uint32_t)FieldAlloc(0x40);
		U32(w, 8) = U32(ANCHOR, 0);
		U32(h, 0) = SEQ_Swirl;
		S16(h, 0x24) = 0;
		U32(w, 0xC) = U32(ANCHOR, 4);
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_pool(ORIG_Swirl, n, h);)
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
		{
			if (!(U8(e, 0) & 1)) continue;
			SwirlDraw(h, w, e);
			if (DrawOnly()) continue;
			S16(e, 4) = (int16_t)(S16(e, 4) + 1);
			if (S16(h, 0x28) < 0) U32(e, 0) = 0;
			else SwirlMove(e);
		}
		FieldFree(0x40);
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c <= 0xC)
		{
			for (int k = 0; k < 3; k++)
			{
				const uint32_t e = PoolFree();
				if (!e) break;
				U32(e, 0) = 1;
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % 0x600 + 0x600);
				S16(e, 8) = (int16_t)(CrtRand() % 0x1000 - 0x800);
				S16(e, 0xA) = (int16_t)(CrtRand() % 0x1000 - 0x800);
				S16(e, 0xC) = (int16_t)(CrtRand() % 0x1000 - 0x800);
				VectorNormalS16(e + 8, e + 8);
				S16(e, 0xE) = (int16_t)(CrtRand() % 0x1000);
				S16(e, 0x10) = (int16_t)(CrtRand() % 0x4B0 + 0x190);
				const int32_t div = CrtRand() % 0xE + 0x12;
				S16(e, 0x12) = (int16_t)(S16(e, 0x10) / div);
				S16(e, 0x16) = (int16_t)(CrtRand() % 0x32 + 0x19);
			}
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) < 0x24) return 0;
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
			if (U8(e, 0) & 1) U32(e, 0) = 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Breath cones (0x5D7E70): node +0xC counter, +0xE delay, +0x10 position, +0x18 / +0x1A y / z
	// angles, +0x1C size, +0x1E size step
	// ------------------------------------------------------------------
	// a depth cue of a prim-model header: flags 0xF3 + level, or 0x33 (no cue: the level is not written)
	struct Cue { uint32_t flags; int32_t fade; };
	static Cue ConeCue(int32_t c) { return c >= 0xC ? Cue{ 0xF3u, shl32(c - 0xC, 10) } : Cue{ 0x33u, 0 }; }
	static Cue GlowCue(int32_t c) { return c >= 0x20 ? Cue{ 0xF3u, shl32(c - 0x20, 10) } : Cue{ 0x33u, 0 }; }

	static void ConeDraw(uint32_t n, Cue cue)
	{
		int16_t angles[3] = { 0, S16(n, 0x18), S16(n, 0x1A) };
		Mat4x3 m;
		ComposeZYXRotationMatrix(angles, &m);
		m.t[0] = S16(n, 0x10);
		m.t[2] = S16(n, 0x14);
		m.t[1] = S16(n, 0x12);
		const int32_t s = S16(n, 0x1C);
		const int32_t sv[3] = { s, s, s };
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t h = (uint32_t)FieldAlloc(0x58);
		U32(h, 0) = MODEL_Cone;
		U32(h, 8) = 0;
		U32(h, 0x1C) = cue.flags;
		if (cue.flags == 0xF3) S32(h, 0xC) = cue.fade;
		PacketCursor() = RenderPrimModel((void *)h, OT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the cone's update after its draw (size -= step, step -= step / 20)
	static void ConeMove(uint32_t n)
	{
		const int16_t d = S16(n, 0x1E);
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) - d);
		S16(n, 0x1E) = (int16_t)(d - (int16_t)(d / 20));
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
	}

	static uint32_t __cdecl ConeTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const int16_t delay = S16(n, 0xE);
		if (delay > 0)
		{
			if (DrawOnly()) return 0;
			S16(n, 0xE) = (int16_t)(delay - 1);
			return 0;
		}
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_node(ORIG_Cone, n);)
		ConeDraw(n, ConeCue(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		ConeMove(n);
		return S16(n, 0xC) >= 0x10 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Mouth glow (0x5D7FE0): node +0xC counter, +0x1C size, +0x1E growth
	// ------------------------------------------------------------------
	static void GlowDraw(uint32_t n, uint32_t anchor, Cue cue)
	{
		const int16_t s = S16(n, 0x1C);
		TransformCameraByShadowRotation((const void *)anchor, s, -(s >> 3));
		const uint32_t h = (uint32_t)FieldAlloc(0x58);
		U32(h, 0) = MODEL_Glow;
		U32(h, 8) = 0;
		U32(h, 0x1C) = cue.flags;
		if (cue.flags == 0xF3) S32(h, 0xC) = cue.fade;
		PacketCursor() = RenderPrimModel((void *)h, OT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// the glow's update after its draw (from its tick 0x18 the growth speeds up)
	static void GlowMove(uint32_t n)
	{
		const int16_t g = S16(n, 0x1E);
		const int16_t c = S16(n, 0xC);
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + g);
		if (c >= 0x18)
		{
			const int16_t a = (int16_t)(g + 0x20);
			S16(n, 0x1E) = (int16_t)(a + (int16_t)(a / 3));
		}
		S16(n, 0xC) = (int16_t)(c + 1);
	}

	static uint32_t __cdecl GlowTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_node(ORIG_Glow, n);)
		GlowDraw(n, ANCHOR, GlowCue(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		GlowMove(n);
		return S16(n, 0xC) >= 0x24 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Breath puffs (0x5D80C0): node +0xC counter, +0x18 sweep angle, +0x1A sweep step
	// ------------------------------------------------------------------
	static uint32_t __cdecl PuffsTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		int32_t live = 0;
		U32(h, 0) = SEQ_Puff;
		S16(h, 0x24) = 4;
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_pool(ORIG_Puffs, n, h);)
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
		{
			if (!(U8(e, 0) & 2)) continue;
			PuffDraw(h, e);
			if (DrawOnly()) continue;
			S16(e, 4) = (int16_t)(S16(e, 4) + 1);
			if (S16(h, 0x28) < 0)
			{
				U32(e, 0) = 0;
				continue;
			}
			PuffMove(e);
			live++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c <= 0x1E)
		{
			const int16_t a = S16(n, 0x18);
			int16_t angles[3] = { 0, a, 0 };
			S16(n, 0x18) = (int16_t)(S16(n, 0x1A) + a);
			Mat4x3 m;
			BuildRotationMatrixFromAngles(angles, &m);
			TransformVectorBy3x3Matrix(&m, (const int32_t *)DIR, (int32_t *)SWEPT);
			for (int k = 0; k < 2; k++)
			{
				const uint32_t e = PoolFree();
				if (!e) break;
				U32(e, 0) = 2;
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % 0x600 + 0xA00);
				U32(e, 8) = U32(ANCHOR, 0);
				U32(e, 0xC) = U32(ANCHOR, 4);
				S16(e, 8) = (int16_t)(S16(e, 8) + (int16_t)(CrtRand() % 200 - 100));
				const int32_t ry = CrtRand() % 200;
				S16(e, 0xA) = (int16_t)(S16(e, 0xA) + (int16_t)(ry - (int16_t)(S16(n, 0xC) << 2)));
				S16(e, 0xC) = (int16_t)(S16(e, 0xC) + (int16_t)(CrtRand() % 200 - 100));
				const int32_t speed = CrtRand() % 0x96 + 0x122;
				const int32_t jx = CrtRand() % 20;
				S16(e, 0x10) = (int16_t)(jx + (mul32(S32(SWEPT, 0), speed) >> 12) - 10);
				const int32_t up = CrtRand() % 0x78;
				const int32_t jy = CrtRand() % 20;
				S16(e, 0x12) = (int16_t)(up + jy + (mul32(S32(SWEPT, 4), speed) >> 12) - 10);
				const int32_t jz = CrtRand() % 20;
				S16(e, 0x14) = (int16_t)(jz + (mul32(S32(SWEPT, 8), speed) >> 12) - 10);
			}
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) >= 4 && live == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Ice shards (0x5D8B40): node +0xC counter, +0x10 spawn point (x, y, z words)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ShardsTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		const uint32_t w = (uint32_t)FieldAlloc(0x88);
		U32(h, 0) = SEQ_Shard;
		S16(h, 0x24) = 8;
		int32_t live = 0;
		S32(w, 0x58) = -0x1000;
		S32(w, 0x5C) = 0;
		S32(w, 0x60) = 0;
		S16(w, 0) = 0;
		S16(w, 2) = 0;
		S16(w, 4) = 0;
		BuildRotationMatrixFromAngles((const int16_t *)w, (Mat4x3 *)(w + 8));
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_pool(ORIG_Shards, n, h);)
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
		{
			if (!(U8(e, 0) & 4)) continue;
			ShardDraw(h, w, e);
			if (DrawOnly()) continue;
			S16(e, 4) = (int16_t)(S16(e, 4) + 1);
			if (S16(h, 0x28) < 0)
			{
				U32(e, 0) = 0;
				continue;
			}
			ShardMove(w, e);
			live++;
		}
		FieldFree(0x88);
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c <= 2)
		{
			for (int k = 0; k < 7; k++)
			{
				const uint32_t e = PoolFree();
				if (!e) break;
				U32(e, 0) = 4;
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % 0x1000 + 0x1000);
				U32(e, 8) = U32(MID, 0);
				U32(e, 0xC) = U32(MID, 4);
				S16(e, 8) = (int16_t)(S16(e, 8) + (int16_t)(CrtRand() % 0x1518 - 0xA8C));
				S16(e, 0xC) = (int16_t)(S16(e, 0xC) + (int16_t)(CrtRand() % 0xA8C - 0x546));
				S16(e, 0xE) = (int16_t)(CrtRand() % 0x46 + 0x1E);
				S16(e, 0x10) = (int16_t)(S16(e, 8) - S16(n, 0x10));
				S16(e, 0x12) = 0;
				S16(e, 0x14) = (int16_t)(S16(e, 0xC) - S16(n, 0x14));
				VectorNormalS16(e + 0x10, e + 0x10);
			}
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) >= 4 && live == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Ice trail (0x5D83E0): node +0xC counter, +0xE mesh index, +0x10 position, +0x18 y angle,
	// +0x1C size. Draw header (0xAC, Field_Alloc): +0 mesh, +4 vertices, +8 far colour, +0xC depth
	// cue, +0x1C flags (0x20: both faces), +0x20 time, +0x24 mesh index -> "drew something",
	// +0x28 polygon state cursor, +0x2C polygon cursor, +0x30 MAC0, +0x38 OTZ, +0x3C FLAG,
	// +0x40 / +0x48 / +0x50 the three vertices, +0x64 temporaries, +0x6C mesh matrix (camera x
	// node), +0x8C polygon matrix (translation +0xA0).
	// Mesh polygon (0x1C bytes): +0 colour 0 + code, +4 / +6 / +8 vertex offsets (/ 4; +8's high
	// half = uv 2), +0xC uv 0 + clut, +0x10 uv 1 + tpage, +0x14 / +0x18 colours 1 / 2.
	// Polygon state (0x20 bytes): +0 centroid (words), +6 delay, +8 offset, +0xE phase (-1 =
	// gone), +0x10 velocity, +0x18 / +0x1C x / z angles, +0x1A / +0x1E their steps.
	// ------------------------------------------------------------------
	// 0x5D79C0: the polygon states of a mesh from its polygons (header +0x2C) and vertices; the
	// delay = (|centroid + (0, 0, length)| - length) * 12 / 6300, the velocity along that vector
	// (0x5A..0x103), random spin steps
	static void TrailBuildStates(uint32_t h, uint32_t verts, int32_t length)
	{
		uint32_t poly = U32(h, 0x2C);
		uint32_t st = U32(h, 0x28);
		const int32_t count = S32(poly, 0);
		poly += 4;
		U32(h, 0x2C) = poly;
		if (count > 0)
		{
			for (int32_t k = count; k != 0; k--)
			{
				const uint32_t a = verts + (uint32_t)U16(poly, 4) * 4;
				const uint32_t b = verts + (uint32_t)U16(poly, 6) * 4;
				const uint32_t c = verts + (uint32_t)U16(poly, 8) * 4;
				const int32_t sx = S16(a, 0) + S16(b, 0) + S16(c, 0);
				const int32_t sy = S16(a, 2) + S16(b, 2) + S16(c, 2);
				const int32_t sz = S16(a, 4) + S16(b, 4) + S16(c, 4);
				const int32_t r = CrtRand();
				const int32_t cx = sx / 3, cy = sy / 3, cz = sz / 3;
				S16(h, 0x58) = (int16_t)cx;
				S16(h, 0x5A) = (int16_t)cy;
				S16(h, 0x5C) = (int16_t)(length + cz);
				const int32_t len = Sqrt(VectorNormalS16(h + 0x58, h + 0x58));
				const int32_t delay = mul32(len - length, 12) / 6300;
				S16(st, 0) = (int16_t)cx;
				S16(st, 2) = (int16_t)cy;
				S16(st, 0xE) = 0;
				S16(st, 0xC) = 0;
				S16(st, 0xA) = 0;
				S16(st, 8) = 0;
				S16(st, 6) = (int16_t)delay;
				S16(st, 4) = (int16_t)cz;
				const int32_t speed = r % 0xAA + 0x5A;
				poly += 0x1C;
				st += 0x20;
				S16(st, -0x10) = (int16_t)(mul32(S16(h, 0x58), speed) >> 12);
				S16(st, -0xE) = (int16_t)(mul32(S16(h, 0x5A), speed) >> 12);
				S16(st, -0xC) = (int16_t)(mul32(S16(h, 0x5C), speed) >> 12);
				S16(st, -8) = 0;
				S16(st, -4) = 0;
				S16(st, -6) = (int16_t)((r & 7) << 4);
				S16(st, -2) = (int16_t)(((r >> 3) & 7) << 3);
			}
		}
		U32(h, 0x2C) = poly;
		U32(h, 0x28) = st;
	}

	// 0x5D7980: header +0 = mesh, +0x24 = its index -> state table, vertices, polygons
	static void TrailInitStates(uint32_t h)
	{
		const uint32_t mesh = U32(h, 0);
		U32(h, 0x28) = U32(TRAIL_STATES + (uint32_t)S32(h, 0x24) * 4, 0);
		U32(h, 4) = mesh + 8;
		U32(h, 0x2C) = U32(mesh, 0) + mesh + 0x18;
		TrailBuildStates(h, mesh + 8, FloorLength());
	}

	// one polygon (shared tail of 0x5D8590 / 0x5D8990): vertices +0x40 / +0x48 / +0x50 through the
	// GTE as set, a POLY_GT3 at cursor (header words written first), culled by the GTE flags and
	// the winding (both faces with header flag 0x20), depth cue +0xC toward the far colour;
	// returns the cursor after it when it was inserted
	static uint32_t TrailPoly(uint32_t h, uint32_t poly, uint32_t p, uint32_t ot, int32_t mode)
	{
		GteLoadV012(h + 0x40, h + 0x48, h + 0x50);
		GteRTPT();
		U32(p, 0xC) = U32(poly, 0xC);
		U32(p, 4) = U32(poly, 0);
		U32(p, 0) = 0x9000000;
		U32(p, 0x18) = U32(poly, 0x10);
		U32(p, 0x24) = U32(poly, 8) >> 16;
		GteReadFLAG(h + 0x3C);
		if (U32(h, 0x3C) & 0x60000) return p;
		GteNCLIP();
		GteReadMAC0(h + 0x30);
		const int32_t mac0 = S32(h, 0x30);
		if (mac0 == 0) return p;
		if (mac0 < 0 && !(U8(h, 0x1C) & 0x20)) return p;
		GteReadSXY012Split(p + 8, p + 0x14, p + 0x20);
		GteAVSZ3();
		GteStoreOTZ(h + 0x38);
		GteLoadRGB012(poly + 0x14, poly + 0x18, p + 4);
		GteSetIR0(S32(h, 0xC));
		GteDPCT();
		GteStoreRGB012(p + 0x10, p + 0x1C, p + 4);
		InsertPrimAutoDepth(ot + (uint32_t)((S32(h, 0x38) >> mode) * 4), (void *)p);
		return p + 0x28;
	}

	// the three vertices of a polygon as the mesh has them
	static void TrailVerts(uint32_t h, uint32_t poly, uint32_t verts)
	{
		const uint32_t a = verts + (uint32_t)U16(poly, 4) * 4;
		U32(h, 0x40) = U32(a, 0);
		U32(h, 0x44) = U32(a, 4);
		const uint32_t b = verts + (uint32_t)U16(poly, 6) * 4;
		U32(h, 0x48) = U32(b, 0);
		U32(h, 0x4C) = U32(b, 4);
		const uint32_t c = verts + (uint32_t)U16(poly, 8) * 4;
		U32(h, 0x50) = U32(c, 0);
		U32(h, 0x54) = U32(c, 4);
	}

	// a flying polygon's state after its draw (offset += velocity, velocity -1/16, spin, phase + 1;
	// gone after 16)
	static void TrailPolyMove(uint32_t st)
	{
		S16(st, 8) = (int16_t)(S16(st, 8) + S16(st, 0x10));
		S16(st, 0xA) = (int16_t)(S16(st, 0xA) + S16(st, 0x12));
		S16(st, 0xC) = (int16_t)(S16(st, 0xC) + S16(st, 0x14));
		const int16_t vx = S16(st, 0x10);
		S16(st, 0x10) = (int16_t)(vx - (int16_t)(vx >> 4));
		const int16_t vy = S16(st, 0x12);
		S16(st, 0x12) = (int16_t)(vy - (int16_t)(vy >> 4));
		S16(st, 0x1C) = (int16_t)(S16(st, 0x1C) + S16(st, 0x1E));
		const int16_t vz = S16(st, 0x14);
		S16(st, 0x14) = (int16_t)(vz - (int16_t)(vz >> 4));
		S16(st, 0x18) = (int16_t)(S16(st, 0x18) + S16(st, 0x1A));
		const int16_t ph = (int16_t)(S16(st, 0xE) + 1);
		S16(st, 0xE) = ph;
		if (ph >= 0x10) S16(st, 0xE) = -1;
	}

	// 0x5D8590: the polygons flying apart (time +0x20 = trail counter - 10): a polygon whose delay
	// has passed is drawn around its centroid: its vertices minus the centroid, turned by its
	// angles, moved to centroid + offset (the offset before its move), depth cue phase * 0x100
	// (phase 0: -0x1000); the others are drawn whole
	static uint32_t TrailFlyPolys(uint32_t h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		const int32_t t = S32(h, 0x20);
		uint32_t st = U32(h, 0x28);
		const uint32_t verts = U32(h, 4);
		uint32_t poly = U32(h, 0x2C);
		const int32_t count = S32(poly, 0);
		poly += 4;
		U32(h, 0x2C) = poly;
		if (count > 0)
		{
			for (int32_t k = count; k != 0; k--, poly += 0x1C, st += 0x20)
			{
				if (S16(st, 0xE) < 0) continue;
				if (S16(st, 6) < t)
				{
					U32(h, 0x64) = U32(st, 0);
					U32(h, 0x68) = U32(st, 4);
					TrailVerts(h, poly, verts);
					for (int v = 0x40; v <= 0x50; v += 8)
					{
						S16(h, v) = (int16_t)(S16(h, v) - S16(h, 0x64));
						S16(h, v + 2) = (int16_t)(S16(h, v + 2) - S16(h, 0x66));
						S16(h, v + 4) = (int16_t)(S16(h, v + 4) - S16(h, 0x68));
					}
					S32(h, 0xA0) = S16(st, 8) + S16(h, 0x64);
					S32(h, 0xA4) = S16(st, 0xA) + S16(h, 0x66);
					S32(h, 0xA8) = S16(st, 0xC) + S16(h, 0x68);
					const int16_t ph = S16(st, 0xE);
					S32(h, 0xC) = ph != 0 ? shl32(ph, 8) : -0x1000;
					if (!DrawOnly()) TrailPolyMove(st);
					S16(h, 0x66) = 0;
					S16(h, 0x68) = S16(st, 0x1C);
					S16(h, 0x64) = S16(st, 0x18);
					BuildRotationMatrixFromAngles((const int16_t *)(h + 0x64), (Mat4x3 *)(h + 0x8C));
					ComposeAffineTransform((const Mat4x3 *)(h + 0x6C), (const Mat4x3 *)(h + 0x8C), (Mat4x3 *)(h + 0x8C));
					GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x8C));
					GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x8C));
				}
				else
				{
					TrailVerts(h, poly, verts);
					GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x6C));
					GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x6C));
					S32(h, 0xC) = 0;
				}
				cursor = TrailPoly(h, poly, cursor, ot, mode);
			}
		}
		U32(h, 0x28) = st;
		U32(h, 0x2C) = poly;
		return cursor;
	}

	// 0x5D8990: the trail appearing (time +0x20 = trail counter): a polygon is drawn once its delay
	// is below the time (from time 12 every polygon)
	static uint32_t TrailGrowPolys(uint32_t h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		const int32_t t = S32(h, 0x20);
		const uint32_t verts = U32(h, 4);
		uint32_t poly = U32(h, 0x2C);
		const int32_t count = S32(poly, 0);
		poly += 4;
		uint32_t st = U32(h, 0x28);
		U32(h, 0x2C) = poly;
		if (count > 0)
		{
			for (int32_t k = count; k != 0; k--, poly += 0x1C, st += 0x20)
			{
				if (S16(st, 6) >= t && t < 0xC) continue;
				TrailVerts(h, poly, verts);
				GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x6C));
				GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x6C));
				S32(h, 0xC) = 0;
				cursor = TrailPoly(h, poly, cursor, ot, mode);
			}
		}
		U32(h, 0x28) = st;
		U32(h, 0x2C) = poly;
		return cursor;
	}

	// 0x5D8520 / 0x5D8920: header +4 / +0x2C from the mesh, the far colour (+8), the polygon state
	// table of mesh +0x24; +0x24 = something drawn
	static uint32_t TrailDraw(uint32_t h, uint32_t ot, int32_t mode, uint32_t cursor, bool fly)
	{
		const uint32_t mesh = U32(h, 0);
		U32(h, 4) = mesh + 8;
		U32(h, 0x2C) = U32(mesh, 0) + mesh;
		GteSetFarColour(U8(h, 8), U8(h, 9), U8(h, 0xA));
		U32(h, 0x2C) = U32(h, 0x2C) + 0x18;
		U32(h, 0x28) = U32(TRAIL_STATES + (uint32_t)S32(h, 0x24) * 4, 0);
		const uint32_t r = fly ? TrailFlyPolys(h, ot, mode, cursor) : TrailGrowPolys(h, ot, mode, cursor);
		S32(h, 0x24) = cursor != r ? 1 : 0;
		return r;
	}

	static uint32_t __cdecl TrailTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		int16_t angles[3] = { 0, S16(n, 0x18), 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(angles, &m);
		m.t[0] = S16(n, 0x10);
		m.t[1] = S16(n, 0x12);
		const int32_t s = S16(n, 0x1C);
		const int32_t sv[3] = { s, s, s };
		m.t[2] = S16(n, 0x14);
		Scale3DMatrix(&m, sv);
		const uint32_t h = (uint32_t)FieldAlloc(0xAC);
		S32(h, 0x24) = S16(n, 0xE);
		U32(h, 0x1C) = 0x30;
		U32(h, 0) = U32(TRAIL_MESHES + (uint32_t)S16(n, 0xE) * 4, 0);
		U32(h, 8) = 0;
		ComposeAffineTransform(&Camera(), &m, (Mat4x3 *)(h + 0x6C));
		const int16_t c = S16(n, 0xC);
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_trail(n, h);)
		if (c >= 0xA)
		{
			S32(h, 0x20) = c - 0xA;
			PacketCursor() = TrailDraw(h, OT44(), 2, PacketCursor(), true);
		}
		else
		{
			S32(h, 0x20) = c;
			PacketCursor() = TrailDraw(h, OT44(), 2, PacketCursor(), false);
		}
		FieldFree(0xAC);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) > 0xA && S32(h, 0x24) == 0) return TASK_END; // (the freed header still holds it)
		return 0;
	}

	// ------------------------------------------------------------------
	// Crystals: the module's copies of the prim-model library (decoder, rotations, vertex blend)
	// ------------------------------------------------------------------
	// 0x5D9090: `count` dwords of `value` at dst
	static void Memset32(uint32_t dst, uint32_t value, uint32_t count)
	{
		for (uint32_t i = 0; i < count; i++)
			U32(dst, (int32_t)(i * 4)) = value;
	}

	// 0x5D8FB0 (= Effect_DecodeModelPrimLayout 0x7016B0): clears `size` bytes of the layout, +0 =
	// data, every object's scale integrator (after its position / rotation ones) = 0x10000000
	static void DecodeLayout(uint32_t data, uint32_t out, int32_t size)
	{
		Memset32(out, 0, (uint32_t)(size >> 2));
		U32(out, 0) = data;
		uint32_t c = out + 8;
		const uint32_t tab = data + U32(data, 4);
		int32_t n = S32(tab, 0);
		uint32_t p = tab + 8;
		for (; n > 0; n--)
		{
			const uint32_t off = U32(p, 0);
			p += 4;
			const uint32_t f = U32(tab + off, 4);
			if (f & 4) c += 0x24;
			else if (f & 2) c += 0x18;
			else if (f & 1) c += 0xC;
			if (f & 0x20) c += 0x24;
			else if (f & 0x10) c += 0x18;
			else if (f & 8) c += 0xC;
			if (f & 0x1C0)
			{
				U32(c, 8) = 0x10000000;
				U32(c, 4) = 0x10000000;
				U32(c, 0) = 0x10000000;
			}
			if (f & 0x100) c += 0x24;
			else if (f & 0x80) c += 0x18;
			else if (f & 0x40) c += 0xC;
			if (f & 0x800) c += 0x18;
			else if (f & 0x400) c += 0x10;
			else if (f & 0x200) c += 8;
			if (f & 0x1000) c += 0xC;
			if (f & 0x2000) c += 0x10;
		}
	}

	// 0x5D9420 / 0x5D9470 / 0x5D94C0: rotation about x / y / z (the 3x3 and its pad cleared first;
	// the y matrix is built for -angle)
	static void RotationX(int16_t angle, uint32_t m)
	{
		Memset32(m, 0, 5);
		const int32_t s = ComputeSin(angle);
		const int32_t c = ComputeCos(angle);
		S16(m, 0) = 0x1000;
		S16(m, 8) = (int16_t)c;
		S16(m, 0xE) = (int16_t)-s;
		S16(m, 0xA) = (int16_t)s;
		S16(m, 0x10) = (int16_t)c;
	}

	static void RotationY(int16_t angle, uint32_t m)
	{
		const int32_t a = -(int32_t)angle;
		Memset32(m, 0, 5);
		const int32_t s = ComputeSin(a);
		const int32_t c = ComputeCos(a);
		S16(m, 0xC) = (int16_t)s;
		S16(m, 4) = (int16_t)-s;
		S16(m, 0) = (int16_t)c;
		S16(m, 8) = 0x1000;
		S16(m, 0x10) = (int16_t)c;
	}

	static void RotationZ(int16_t angle, uint32_t m)
	{
		Memset32(m, 0, 5);
		const int32_t s = ComputeSin(angle);
		const int32_t c = ComputeCos(angle);
		S16(m, 0) = (int16_t)c;
		S16(m, 6) = (int16_t)-s;
		S16(m, 2) = (int16_t)s;
		S16(m, 8) = (int16_t)c;
		S16(m, 0x10) = 0x1000;
	}

	// 0x5D93A0 (= MAG_017_sub_701310): out = (Rz(a2) x Rx(a0)) x Ry(a1) (Matrix3x3_MultiplyPSX)
	static void RotationFromAngles(uint32_t angles, uint32_t out)
	{
		Mat4x3 mx, mz, my, mzx;
		RotationZ(S16(angles, 4), (uint32_t)&mz);
		RotationX(S16(angles, 0), (uint32_t)&mx);
		Matrix3x3MultiplyPSX((uint32_t)&mz, (uint32_t)&mx, (uint32_t)&mzx);
		RotationY(S16(angles, 2), (uint32_t)&my);
		Matrix3x3MultiplyPSX((uint32_t)&mzx, (uint32_t)&my, out);
	}

	// 0x5D9510 (= MAG_017_sub_701390): vertex frames f0 and f1 of a model blended by t (4.12) into
	// out, through the GTE (IR0 interpolation)
	static void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out)
	{
		const int32_t w0 = 0x1000 - t;
		uint32_t a = f0 == 0 ? model + 0xC : model + (uint32_t)mul32(S32(model, 4), f0) * 8 + 0xC;
		uint32_t b = f1 == 0 ? model + 0xC : model + (uint32_t)mul32(S32(model, 4), f1) * 8 + 0xC;
		uint32_t n = U32(model, 4);
		for (; n != 0; n--)
		{
			GteSetIR0(w0);
			GteLoadIR123((const void *)a);
			GteGPF();
			GteSetIR0(t);
			GteLoadIR123((const void *)b);
			GteGPL();
			GteStoreIR123((void *)out);
			out += 8;
			a += 8;
			b += 8;
		}
	}

	// 0x5D9130: crystal player draw callback (layout l, record r, parameter block b on the player
	// task's stack: +0 position, +8 object flag table, +0xC blend buffer, +0x10 rotation added when
	// +0x16, +0x18 scale applied when +0x24): the object's vertex frame (blended between two frames
	// by 0x5D9510), its matrix (record rotation by 0x5D93A0; record offset + block position: flag
	// 0x10000000 = position through the camera, else composed with it), the record scale, the
	// block scale, the fade (record +0x24 toward record colour)
	static void __cdecl CrystalCallback(prim::Layout *lp, prim::Record *rp, int arg)
	{
		const uint32_t l = (uint32_t)lp, rec = (uint32_t)rp, blk = (uint32_t)arg;
		if (((uint32_t)(int32_t)S16(rec, 0x1C) | U32(rec, 0x18)) == 0)
			return; // zero scale: nothing drawn
		if (S16(rec, 0x24) >= 0x1000 && U32(rec, 0x20) == 0)
			return; // full fade to black: nothing drawn
		const uint32_t h = (uint32_t)FieldAlloc(0x58);
		const uint32_t data = U32(l, 0);
		const uint32_t model = data + U32(data, S16(rec, 2) * 4 + 8);
		U32(h, 0) = model;
		const int16_t f1 = S16(rec, 0x2A);
		const int16_t f0 = S16(rec, 0x28);
		auto frame_ptr = [model](int16_t f) -> uint32_t {
			if (f == 0)
				return model + 0xC;
			return model + (uint32_t)mul32(S32(model, 4), (int32_t)f) * 8 + 0xC;
		};
		if (f0 == f1)
			U32(h, 4) = frame_ptr(f0);
		else
		{
			const int16_t t = S16(rec, 0x26);
			if (t == 0)
				U32(h, 4) = frame_ptr(f0);
			else if (t == 0x1000)
				U32(h, 4) = frame_ptr(f1);
			else
			{
				BlendVertexFrames(model, f0, f1, t, U32(blk, 0xC));
				U32(h, 4) = U32(blk, 0xC);
			}
		}
		if (U16(blk, 0x16) != 0)
		{
			S16(rec, 0x10) = (int16_t)(S16(rec, 0x10) + S16(blk, 0x10));
			S16(rec, 0x12) = (int16_t)(S16(rec, 0x12) + S16(blk, 0x12));
			S16(rec, 0x14) = (int16_t)(S16(rec, 0x14) + S16(blk, 0x14));
		}
		Mat4x3 m;
		RotationFromAngles(rec + 0x10, (uint32_t)&m);
		m.t[0] = S16(rec, 8) + S16(blk, 0);
		m.t[1] = S16(rec, 0xA) + S16(blk, 2);
		m.t[2] = S16(rec, 0xC) + S16(blk, 4);
		const uint32_t flags = U32(blk, 8) + (uint32_t)(S16(rec, 0) * 4);
		if (U32(flags, 0) & 0x10000000)
		{
			TransformVectorBy3x3Matrix(&Camera(), m.t, m.t);
			const Mat4x3 &cam = Camera();
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)cam.t[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)cam.t[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)cam.t[2]);
		}
		else
			ComposeAffineTransform(&Camera(), &m, &m);
		if (U32(rec, 0x18) != 0x10001000 || S16(rec, 0x1C) != 0x1000)
		{
			const int32_t sv[3] = { S16(rec, 0x18), S16(rec, 0x1A), S16(rec, 0x1C) };
			Scale3DMatrix(&m, sv);
		}
		if (U32(blk, 0x24) != 0)
			Scale3DMatrix(&m, (const int32_t *)(blk + 0x18));
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t f = (U32(flags, 0) & 0xDFFF) | 0x2000;
		S32(h, 0xC) = S16(rec, 0x24);
		U32(h, 0x1C) = f;
		if (S32(h, 0xC) != 0)
		{
			U32(h, 8) = U32(rec, 0x20);
			U32(h, 0x1C) = f | 0xC0;
		}
		PacketCursor() = RenderPrimModel((void *)h, OT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Crystal player (0x5D90B0, queue 0x239A8D0): node +0x10 position, +0x18 scale, +0x1A z spin,
	// +0x1C the layout
	// ------------------------------------------------------------------
	static void CrystalBlock(uint32_t n, uint32_t blk)
	{
		U32(blk, 8) = CRYSTAL_FLAGS;
		U32(blk, 0) = U32(n, 0x10);
		U32(blk, 0xC) = TexBase() + 0x1C700;
		U32(blk, 4) = U32(n, 0x14);
		S16(blk, 0x12) = 0;
		S16(blk, 0x10) = 0;
		S16(blk, 0x14) = S16(n, 0x1A);
		const int32_t s = S16(n, 0x18);
		S32(blk, 0x20) = s;
		S32(blk, 0x1C) = s;
		S32(blk, 0x18) = s;
		S16(blk, 0x16) = 1;
		S32(blk, 0x24) = 1;
	}

	static uint32_t __cdecl CrystalTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		uint8_t blk[0x28];
		CrystalBlock(n, (uint32_t)blk);
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_crystal(n);)
		const int left = prim::play((prim::Layout *)(n + 0x1C), CrystalCallback, (int)(uint32_t)blk, PlayerPaused() ? 1 : 0);
		return left ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Crystal spawner (0x5D8E20): node +0xC counter, +0x18 sweep angle, +0x1A sweep step
	// ------------------------------------------------------------------
	static uint32_t __cdecl SpawnerTask(TaskNode *nd)
	{
		if (DrawOnly()) return 0;
		const uint32_t n = (uint32_t)nd;
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c <= 0x1E)
		{
			const int16_t a = S16(n, 0x18);
			int16_t angles[3] = { 0, a, 0 };
			S16(n, 0x18) = (int16_t)(S16(n, 0x1A) + a);
			Mat4x3 m;
			BuildRotationMatrixFromAngles(angles, &m);
			int32_t d[3];
			TransformVectorBy3x3Matrix(&m, (const int32_t *)SWEPT, d);
			const int32_t r = CrtRand() % 0x1000 + 0x300;
			const int32_t dist = shl32(mul32(r, FloorLength()), 1) >> 12;
			const uint32_t t = (uint32_t)AddTaskToQueue(PlayerQueue(), ORIG_Crystal);
			DecodeLayout(LAYOUT_Crystal, t + 0x1C, 0x40);
			const int32_t rx = CrtRand() % 0x320;
			S16(t, 0x10) = (int16_t)((mul32(d[0], dist) >> 12) + (int32_t)(U32(ANCHOR, 0) + (uint32_t)rx) - 0x190);
			const int32_t ry = CrtRand() % 0x320;
			S16(t, 0x12) = (int16_t)((int16_t)(ry + (mul32(d[1], dist) >> 12)) + S16(ANCHOR, 2) - 0x190);
			const int32_t rz = CrtRand() % 0x320;
			S16(t, 0x14) = (int16_t)((mul32(d[2], dist) >> 12) + (int32_t)(U32(ANCHOR, 4) + (uint32_t)rz) - 0x190);
			S16(t, 0x1A) = (int16_t)(CrtRand() % 0x400);
			S16(t, 0x18) = (int16_t)(CrtRand() % 0xA00 + 0x600);
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) > 0x1E ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5D7570): node +0xC counter
	// ------------------------------------------------------------------
	// the anchor (caster bone 0xF0 + (0, 0, -0x96) through the caster's rotation) into out (x, y, z
	// words; the pad word untouched)
	static void ComputeAnchor(uint32_t out)
	{
		const int32_t slot = CasterSlot();
		GetEffectSpawnPosition(Ent(slot), 0xF0, 0x1000, out);
		int32_t v[3] = { 0, 0, -0x96 };
		TransformVectorBy3x3Matrix((const Mat4x3 *)(Ent(CasterSlot()) + 0x40), v, v);
		const int16_t x = (int16_t)(S16(out, 0) + (int16_t)v[0]);
		const int16_t z = (int16_t)(S16(out, 4) + (int16_t)v[2]);
		S16(out, 2) = (int16_t)(S16(out, 2) + (int16_t)v[1]);
		S16(out, 0) = x;
		S16(out, 4) = z;
	}

	static uint32_t __cdecl DirectorTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		ComputeAnchor(ANCHOR);
		const int32_t ax = S16(ANCHOR, 0), az = S16(ANCHOR, 4);
		S16(MID, 2) = 0;
		S16(MID, 0) = (int16_t)((S16(CENTRE, 0) + ax) / 2);
		S32(DIR, 4) = 0;
		S16(MID, 4) = (int16_t)((S16(CENTRE, 4) + az) / 2);
		S32(DIR, 0) = S16(MID, 0) - ax;
		S32(DIR, 8) = S16(MID, 4) - az;
		FloorLength() = Sqrt(NormalizeVectorToFixedPoint(DIR, DIR));
		if (DrawOnly()) return 0;
		if (S16(n, 0xC) == 1)
		{
			CalculateCenterPosition(U16((uint32_t)Ctx(), 2), CENTRE);
			InitTaskQueuePool(PlayerQueue(), (void *)(TexBase() + 0x18000), 0x11C, 0x40);
		}
		if (S16(n, 0xC) == 2)
		{
			TaskNode *t = AddTaskToQueue(FxQueue(), ORIG_Swirl);
			S16((uint32_t)t, 0xC) = 0;
		}
		if (S16(n, 0xC) == 0xE)
		{
			for (int k = 0; k < 4; k++)
			{
				const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Cone);
				S16(t, 0xE) = S16(CONE_DELAYS + (uint32_t)k * 4, 0);
				U32(t, 0x10) = U32(ANCHOR, 0);
				U32(t, 0x14) = U32(ANCHOR, 4);
				S16(t, 0xC) = 0;
				S16(t, 0x1C) = 0x1400;
				S16(t, 0x1E) = 0x16D;
				S16(t, 0x18) = S16(Ent(CasterSlot()), 0xE);
				S16(t, 0x1A) = S16(CONE_ANGLES + (uint32_t)k * 4, 0);
			}
		}
		if (S16(n, 0xC) == 8)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Glow);
			S16(t, 0xC) = 0;
			S16(t, 0x1C) = 0x280;
			S16(t, 0x1E) = 0xA0;
		}
		if (S16(n, 0xC) == 0x25)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Puffs);
			S16(t, 0xC) = 0;
			S16(t, 0x18) = 0x200;
			S16(t, 0x1A) = -0x22;
		}
		if (S16(n, 0xC) == 0x26)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Spawner);
			S16(t, 0xC) = 0;
			S16(t, 0x1A) = -0x10;
			S16(t, 0x18) = 0x1A0;
		}
		if (S16(n, 0xC) == 0x46)
		{
			const uint32_t h = (uint32_t)FieldAlloc(0xAC);
			for (int k = 0; k < 5; k++)
			{
				const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Trail);
				U32(t, 0x10) = U32(ANCHOR, 0);
				U32(t, 0x14) = U32(ANCHOR, 4);
				S16(t, 0xC) = 0;
				S16(t, 0xE) = (int16_t)k;
				S16(t, 0x12) = 0;
				S16(t, 0x1C) = 0x1000;
				const int16_t ry = S16(Ent(CasterSlot()), 0xE);
				U32(h, 0) = U32(TRAIL_MESHES + (uint32_t)k * 4, 0);
				S32(h, 0x24) = k;
				S16(t, 0x18) = (int16_t)((ry - 0x800) & 0xFFF);
				TrailInitStates(h);
			}
			FieldFree(0xAC);
		}
		if (S16(n, 0xC) == 0x4A)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Shards);
			U32(t, 0x10) = U32(ANCHOR, 0);
			S16(t, 0xC) = 0;
			U32(t, 0x14) = U32(ANCHOR, 4);
		}
		if (S16(n, 0xC) == 0x55)
		{
			ActionData *a = Ctx()->actions;
			ApplyActionResultToTargets(a->targets, a->target_count);
		}
		const int16_t c = S16(n, 0xC);
		if (c <= 0x10) SetScreenFlash((uint32_t)(c * 0xC0), 0x502020);
		else if (c >= 0x57) SetScreenFlash((uint32_t)((0x5F - c) * 0x180), 0x502020);
		if (S16(n, 0xC) == 1) BdPlaySE(SOUND_Start, 0, 0x80);
		if (S16(n, 0xC) == 0x24) BdPlaySE(SOUND_Blast, 0, 0x80);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) <= 0x5F) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Root (0x5D9BF0) and texture restore wait (0x5D9C70)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_root();)
		if (U8(n, 0xC) & 1) PacketCursor() = TexBase() + 0xC000;
		else PacketCursor() = TexBase();
		const int running = ExecuteTaskQueue(FxQueue());
		ExecuteTaskQueue(PlayerQueue());
		if (!running)
		{
			const uint32_t w = (uint32_t)AddTaskToQueue(RootQueue(), ORIG_TexRestore);
			S16(w, 0xC) = 0;
			S16(w, 0xE) = 0;
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return running ? 0 : TASK_END;
	}

	static uint32_t __cdecl TexRestoreTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		if (S16(n, 0xC) == 1) TextureRestore(TexBase(), n + 0xE);
		const int16_t done = S16(n, 0xE);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return done ? TASK_END : 0;
	}
}

	void register_mag173_ice_breath()
	{
		register_port(ib173::ORIG_Root, (void *)ib173::RootTask, "173 RootTask", 173);
		register_port(ib173::ORIG_TexRestore, (void *)ib173::TexRestoreTask, "173 TexRestore", 173);
		register_port(ib173::ORIG_Director, (void *)ib173::DirectorTask, "173 Director", 173);
		register_port(ib173::ORIG_Swirl, (void *)ib173::SwirlTask, "173 MouthSwirl", 173);
		register_port(ib173::ORIG_Cone, (void *)ib173::ConeTask, "173 BreathCone", 173);
		register_port(ib173::ORIG_Glow, (void *)ib173::GlowTask, "173 MouthGlow", 173);
		register_port(ib173::ORIG_Puffs, (void *)ib173::PuffsTask, "173 BreathPuffs", 173);
		register_port(ib173::ORIG_Trail, (void *)ib173::TrailTask, "173 IceTrail", 173);
		register_port(ib173::ORIG_Shards, (void *)ib173::ShardsTask, "173 IceShards", 173);
		register_port(ib173::ORIG_Spawner, (void *)ib173::SpawnerTask, "173 CrystalSpawner", 173);
		register_port(ib173::ORIG_Crystal, (void *)ib173::CrystalTask, "173 Crystal", 173);
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(register_mag173_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag173_ice_breath_held.inc"
#endif
