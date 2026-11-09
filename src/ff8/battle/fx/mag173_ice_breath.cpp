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
// Fujin's described Ice Breath (effect_173.json) runs this code: its memory through own::R() (fx_port.h),
// its spawns through own::Fn(), the setup and the file loader ported below; its parameters
// (IceBreathParams: the sounds, the director's ticks, the mouth, the screen flash, each part's counts,
// sizes, spreads, speeds and sweeps, its data: flipbooks, meshes, the crystal layout, tables, resources);
// FX_PART tags its parts in the lab.

#include <cmath>
#include <algorithm>
#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

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
	inline int32_t &FloorLength() { return var<int32_t>(own::R(0x2397278)); }          // |anchor -> midpoint|
	inline TaskQueue *RootQueue() { return (TaskQueue *)own::R(0x23972D0); }           // pool 0x23972B0, 2 x 0x10
	inline TaskQueue *FxQueue() { return (TaskQueue *)own::R(0x2397C40); }             // pool 0x2397C50, 0x64 x 0x24
	inline TaskQueue *PlayerQueue() { return (TaskQueue *)own::R(0x239A8D0); }         // pool texture base + 0x18000, 0x40 x 0x11C
	static const uint32_t CENTRE = 0x2398A60;      // targets' centre (x, y, z words)
	static const uint32_t DIR = 0x2398A68;         // anchor -> midpoint on the floor (3 x int32, normalised)
	static const uint32_t SWEPT = 0x2398A80;       // DIR turned by the puff task's sweep (3 x int32)
	static const uint32_t MID = 0x2398A90;         // midpoint (x, 0, z, pad words)
	static const uint32_t ANCHOR = 0x2398AA0;      // caster's mouth (x, y, z, pad words)
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x2398A78)); }
	inline int32_t &CasterSlot() { return var<int32_t>(own::R(0x2398A98)); }
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x239A8C8)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x239A8CC)); }
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x2398A9C)); }          // the effect's file (its TIM)
	inline int32_t &FirstTarget() { return var<int32_t>(own::R(0x239727C)); }        // the first target's slot
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
	static const uint32_t ORIG_FileLoader = 0x5D7490, ORIG_Setup = 0x5D74B0;
	static const uint32_t TEXTURE_NAME = 0xD61E1C, CAMERA = 0xD5E67C;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_SWIRL = 1, PART_CONES = 2, PART_GLOW = 3, PART_PUFFS = 4, PART_CRYSTALS = 5, PART_TRAIL = 6, PART_SHARDS = 7 };
	struct IceBreathParams
	{
		struct { uint32_t start, blast; } sounds;  // the director's tick 1, its blast tick
		struct { int16_t swirl, glow, cones, puffs, crystals, trail, shards, damage, blast, end; } director; // its ticks
		// the mouth: this far along the caster's z from its bone 0xF0; or (at_targets) the targets' centre + (x, z) through
		// the caster's rotation, at height y (not the game's: a new effect that happens on the monsters)
		// (each_target, bone: the puffs, crystals and shards around each target's effect point instead)
		struct { int16_t mouth, at_targets, x, y, z, each_target, bone; } anchor;
		struct { int32_t colour; int16_t in_end, in_step, out_start, out_step; } flash; // the screen flash
		struct { int16_t spawn_last, per_tick, life, size_min, size_rand, radius_min, radius_rand, shrink_min, shrink_rand, spin_min, spin_rand; } swirl;
		struct { int16_t count, size, shrink, fade_from, life; } cones;
		struct { int16_t size, growth, speedup_from, fade_from, life; } glow;
		// (ahead, depth: not the game's - where a new effect's puffs start along the way, their spread's depth along it)
		struct { int16_t spawn_last, per_tick, size_min, size_rand, spread, speed_min, speed_rand, rise_rand, sweep_start, sweep_step, floor, ahead, depth; } puffs;
		struct { int16_t spawn_last, sweep_start, sweep_step, dist_min, dist_rand, offset, spin_rand, scale_min, scale_rand; } crystals;
		struct { int16_t size, grow_ticks, full, speed_min, speed_rand; } trail;
		// (swirl, rise, widen, ground: not the game's - on each target, a small tornado)
		struct { int16_t spawn_last, per_tick, size_min, size_rand, spread_x, spread_z, speed_min, speed_rand, swirl, rise, widen, ground; } shards;
		struct { uint32_t mesh0, mesh1, mesh2, mesh3, mesh4; } trail_meshes; // the ice trail's five meshes (prim models)
		struct { uint32_t swirl, puff, shard; } seqs; // the sprite sequences drawn (flipbooks)
		struct { uint32_t crystal; } layouts; // the prim-model layouts played (their models' animation)
		struct { uint32_t cone, glow; } models; // the prim models drawn (meshes)
		struct { uint32_t cone_delays, cone_angles, puff_colours, crystal_flags; } tables; // its small tables (data): colours, fades, counts, offsets
		struct { uint32_t texture_name, camera; } res; // its resources (data): texture file name, camera track, TIMs, animation sets
	};
	static const IceBreathParams GAME = {
		{ 0xD61D84, 0xD61D88 },
		{ 2, 8, 0xE, 0x25, 0x26, 0x46, 0x4A, 0x55, 0x24, 0x5F },
		{ -0x96, 0, 0, -600, 2000, 0, 0xF1 },
		{ 0x502020, 0x10, 0xC0, 0x57, 0x180 },
		{ 0xC, 3, 0x24, 0x600, 0x600, 0x190, 0x4B0, 0x12, 0xE, 0x19, 0x32 },
		{ 4, 0x1400, 0x16D, 0xC, 0x10 },
		{ 0x280, 0xA0, 0x18, 0x20, 0x24 },
		{ 0x1E, 2, 0xA00, 0x600, 200, 0x122, 0x96, 0x78, 0x200, -0x22, -300, 0, 0x1000 },
		{ 0x1E, 0x1A0, -0x10, 0x300, 0x1000, 0x320, 0x400, 0x600, 0xA00 },
		{ 0x1000, 0xA, 0xC, 0x5A, 0xAA },
		{ 2, 7, 0x1000, 0x1000, 0x1518, 0xA8C, 0x1E, 0x46, 0, 40, 64, 0 },
		{ 0xD5FB00, 0xD60AFC, 0xD6108C, 0xD6161C, 0xD61878 },
		{ SEQ_Swirl, SEQ_Puff, SEQ_Shard },
		{ LAYOUT_Crystal },
		{ MODEL_Cone, MODEL_Glow },
		{ CONE_DELAYS, CONE_ANGLES, PUFF_COLOURS, CRYSTAL_FLAGS },
		{ TEXTURE_NAME, CAMERA } };
	static IceBreathParams live = GAME;
	static const IceBreathParams &P() { return live; }

	// a trail mesh (its index 0..4): a prim model of the game's vertex count whose only primitives are its
	// gouraud textured triangles (its code reads the GT3 list at lists + 0x18 and keeps a state per triangle in
	// a table of the game's count: no more of them) - else the game's
	static uint32_t TrailMesh(int32_t k)
	{
		const uint32_t game = *(const uint32_t *)(TRAIL_MESHES + (uint32_t)k * 4);
		const uint32_t m = fxp::model((&P().trail_meshes.mesh0)[k], game);
		if (m == game) return game;
		const uint32_t lists = m + *(const uint32_t *)m;
		for (uint32_t l = 0; l < 6; l++)
			if (*(const uint32_t *)(lists + l * 4)) return game;
		return *(const uint32_t *)(lists + 0x18) <= *(const uint32_t *)(game + *(const uint32_t *)game + 0x18) ? m : game;
	}
	// a trail mesh's polygon state table (0xD61E04: the module's own words, where the running copy has them)
	inline uint32_t TrailStates(int32_t k) { return own::R(*(const uint32_t *)(TRAIL_STATES + (uint32_t)k * 4)); }

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
		uint32_t e = own::R(POOL);
		int32_t i = 0;
		while (S32(e, 0) != 0)
		{
			e += 0x18;
			i++;
			if (e >= own::R(POOL_END)) return 0;
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
		FX_PART(PART_SWIRL);
		PacketCursor() = InitEffectSequenceFromData((void *)h, OT44(), 2, PacketCursor());
		FX_PART(0);
	}

	// a breath puff (0x5D80C0): colour by frame, position +8, pushed toward the camera by size / 16
	static void PuffDraw(uint32_t h, uint32_t e)
	{
		const uint16_t frame = U16(e, 4);
		S16(h, 4) = (int16_t)frame;
		U32(h, 0x1C) = U32(fxp::data(P().tables.puff_colours, PUFF_COLOURS, 64, 4) + (uint32_t)frame * 4, 0);
		const int16_t size = S16(e, 6);
		TransformCameraByShadowRotation((const void *)(e + 8), size, -(size >> 4));
		FX_PART(PART_PUFFS);
		PacketCursor() = InitEffectSequenceFromData((void *)h, OT44(), 2, PacketCursor());
		FX_PART(0);
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
		FX_PART(PART_SHARDS);
		PacketCursor() = InitEffectSequenceFromData((void *)h, OT44(), 2, PacketCursor());
		FX_PART(0);
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
		if (y < P().puffs.floor)
		{
			S16(e, 0xA) = (int16_t)(y + S16(e, 0x12));
			const int16_t vy = S16(e, 0x12);
			S16(e, 0x12) = (int16_t)(vy - (int16_t)(vy >> 5));
		}
	}

	static bool TargetPoint(int32_t k, int16_t p[4]);

	// (not the game's) a shard of a small tornado: turned about its target's point (+0x16: which target) by shards.swirl
	// a tick (4096 a turn), rising by shards.rise, its radius widening by shards.widen / 4096; drawn along its way round
	static void ShardSpin(uint32_t e)
	{
		int16_t at[4];
		if (!TargetPoint(S16(e, 0x16), at)) return;
		const double a = P().shards.swirl * 3.14159265358979 / 2048.0, grow = 1.0 + P().shards.widen / 4096.0;
		const double x = (S16(e, 8) - at[0]) * grow, z = (S16(e, 0xC) - at[2]) * grow;
		const double nx = x * std::cos(a) - z * std::sin(a), nz = x * std::sin(a) + z * std::cos(a);
		S16(e, 8) = (int16_t)(at[0] + std::lround(nx));
		S16(e, 0xC) = (int16_t)(at[2] + std::lround(nz));
		S16(e, 0xA) = (int16_t)(S16(e, 0xA) - P().shards.rise);
		// its way: the tangent of the turn, tilted up by the rise
		const double len = std::max(1.0, std::sqrt(nx * nx + nz * nz));
		const double step = len * std::fabs(a);
		const double sign = a < 0 ? -1.0 : 1.0;
		S16(e, 0x10) = (int16_t)std::lround(-nz / len * step * sign);
		S16(e, 0x12) = (int16_t)(-P().shards.rise);
		S16(e, 0x14) = (int16_t)std::lround(nx / len * step * sign);
		if (!S16(e, 0x10) && !S16(e, 0x12) && !S16(e, 0x14)) S16(e, 0x10) = 1;
		VectorNormalS16(e + 0x10, e + 0x10);
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
		U32(w, 8) = U32(own::R(ANCHOR), 0);
		U32(h, 0) = fxp::sequence(P().seqs.swirl, SEQ_Swirl);
		S16(h, 0x24) = 0;
		U32(w, 0xC) = U32(own::R(ANCHOR), 4);
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_pool(ORIG_Swirl, n, h);)
		for (uint32_t e = own::R(POOL); e < own::R(POOL_END); e += 0x18)
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
		if (c >= 0 && c <= P().swirl.spawn_last)
		{
			for (int k = 0; k < P().swirl.per_tick; k++)
			{
				const uint32_t e = PoolFree();
				if (!e) break;
				U32(e, 0) = 1;
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % P().swirl.size_rand + P().swirl.size_min);
				S16(e, 8) = (int16_t)(CrtRand() % 0x1000 - 0x800);
				S16(e, 0xA) = (int16_t)(CrtRand() % 0x1000 - 0x800);
				S16(e, 0xC) = (int16_t)(CrtRand() % 0x1000 - 0x800);
				VectorNormalS16(e + 8, e + 8);
				S16(e, 0xE) = (int16_t)(CrtRand() % 0x1000);
				S16(e, 0x10) = (int16_t)(CrtRand() % P().swirl.radius_rand + P().swirl.radius_min);
				const int32_t div = CrtRand() % P().swirl.shrink_rand + P().swirl.shrink_min;
				S16(e, 0x12) = (int16_t)(S16(e, 0x10) / div);
				S16(e, 0x16) = (int16_t)(CrtRand() % P().swirl.spin_rand + P().swirl.spin_min);
			}
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) < P().swirl.life) return 0;
		for (uint32_t e = own::R(POOL); e < own::R(POOL_END); e += 0x18)
			if (U8(e, 0) & 1) U32(e, 0) = 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Breath cones (0x5D7E70): node +0xC counter, +0xE delay, +0x10 position, +0x18 / +0x1A y / z
	// angles, +0x1C size, +0x1E size step
	// ------------------------------------------------------------------
	// a depth cue of a prim-model header: flags 0xF3 + level, or 0x33 (no cue: the level is not written)
	struct Cue { uint32_t flags; int32_t fade; };
	static Cue ConeCue(int32_t c) { const int32_t f = P().cones.fade_from; return c >= f ? Cue{ 0xF3u, shl32(c - f, 10) } : Cue{ 0x33u, 0 }; }
	static Cue GlowCue(int32_t c) { const int32_t f = P().glow.fade_from; return c >= f ? Cue{ 0xF3u, shl32(c - f, 10) } : Cue{ 0x33u, 0 }; }

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
		U32(h, 0) = fxp::model(P().models.cone, MODEL_Cone);
		U32(h, 8) = 0;
		U32(h, 0x1C) = cue.flags;
		if (cue.flags == 0xF3) S32(h, 0xC) = cue.fade;
		FX_PART(PART_CONES);
		PacketCursor() = RenderPrimModel((void *)h, OT44(), 2, PacketCursor());
		FX_PART(0);
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
		return S16(n, 0xC) >= P().cones.life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Mouth glow (0x5D7FE0): node +0xC counter, +0x1C size, +0x1E growth
	// ------------------------------------------------------------------
	static void GlowDraw(uint32_t n, uint32_t anchor, Cue cue)
	{
		const int16_t s = S16(n, 0x1C);
		TransformCameraByShadowRotation((const void *)anchor, s, -(s >> 3));
		const uint32_t h = (uint32_t)FieldAlloc(0x58);
		U32(h, 0) = fxp::model(P().models.glow, MODEL_Glow);
		U32(h, 8) = 0;
		U32(h, 0x1C) = cue.flags;
		if (cue.flags == 0xF3) S32(h, 0xC) = cue.fade;
		FX_PART(PART_GLOW);
		PacketCursor() = RenderPrimModel((void *)h, OT44(), 2, PacketCursor());
		FX_PART(0);
		FieldFree(0x58);
	}

	// the glow's update after its draw (from its tick 0x18 the growth speeds up)
	static void GlowMove(uint32_t n)
	{
		const int16_t g = S16(n, 0x1E);
		const int16_t c = S16(n, 0xC);
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + g);
		if (c >= P().glow.speedup_from)
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
		GlowDraw(n, own::R(ANCHOR), GlowCue(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		GlowMove(n);
		return S16(n, 0xC) >= P().glow.life ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Breath puffs (0x5D80C0): node +0xC counter, +0x18 sweep angle, +0x1A sweep step
	// ------------------------------------------------------------------
	// (not the game's) anchor.each_target: the k-th of the action's targets (slot order, cycling), its effect point
	// anchor.bone (0xF1: its body - the live pose, so a flying monster's hover too) into p; false: not asked, no target
	static bool TargetPoint(int32_t k, int16_t p[4])
	{
		if (!P().anchor.at_targets || !P().anchor.each_target) return false;
		const uint32_t mask = U16((uint32_t)Ctx(), 2);
		int32_t slots[8], count = 0;
		for (int32_t s = 0; s < 8; s++)
			if (mask & (1u << s)) slots[count++] = s;
		if (!count) return false;
		GetEffectSpawnPosition(Ent(slots[(k < 0 ? -k : k) % count]), P().anchor.bone, 0, (uint32_t)p);
		return true;
	}

	static uint32_t __cdecl PuffsTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		int32_t live = 0;
		U32(h, 0) = fxp::sequence(P().seqs.puff, SEQ_Puff);
		S16(h, 0x24) = 4;
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_pool(ORIG_Puffs, n, h);)
		for (uint32_t e = own::R(POOL); e < own::R(POOL_END); e += 0x18)
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
		if (c >= 0 && c <= P().puffs.spawn_last)
		{
			const int16_t a = S16(n, 0x18);
			int16_t angles[3] = { 0, a, 0 };
			S16(n, 0x18) = (int16_t)(S16(n, 0x1A) + a);
			Mat4x3 m;
			BuildRotationMatrixFromAngles(angles, &m);
			TransformVectorBy3x3Matrix(&m, (const int32_t *)own::R(DIR), (int32_t *)own::R(SWEPT));
			for (int k = 0; k < P().puffs.per_tick; k++)
			{
				const uint32_t e = PoolFree();
				if (!e) break;
				U32(e, 0) = 2;
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % P().puffs.size_rand + P().puffs.size_min);
				U32(e, 8) = U32(own::R(ANCHOR), 0);
				U32(e, 0xC) = U32(own::R(ANCHOR), 4);
				const int32_t spread = P().puffs.spread;
				if (P().puffs.ahead)        // (not the game's) moved along the way, onto the targets
				{
					S16(e, 8) = (int16_t)(S16(e, 8) + (int16_t)(mul32(S32(own::R(DIR), 0), P().puffs.ahead) >> 12));
					S16(e, 0xC) = (int16_t)(S16(e, 0xC) + (int16_t)(mul32(S32(own::R(DIR), 8), P().puffs.ahead) >> 12));
				}
				// the spread along z: its depth (4096 = the spread, the game's)
				const int32_t depth = P().puffs.depth == 0x1000 ? spread : std::max<int32_t>(1, mul32(spread, P().puffs.depth) >> 12);
				int16_t at[4];
				if (TargetPoint(k + S16(n, 0xC) * P().puffs.per_tick, at))
				{
					// (not the game's) around a target's effect point, centred on it in y too
					S16(e, 8) = (int16_t)(at[0] + (int16_t)(CrtRand() % spread - spread / 2));
					S16(e, 0xA) = (int16_t)(at[1] + (int16_t)(CrtRand() % spread - spread / 2));
					S16(e, 0xC) = (int16_t)(at[2] + (int16_t)(CrtRand() % depth - depth / 2));
				}
				else
				{
					S16(e, 8) = (int16_t)(S16(e, 8) + (int16_t)(CrtRand() % spread - spread / 2));
					const int32_t ry = CrtRand() % spread;
					S16(e, 0xA) = (int16_t)(S16(e, 0xA) + (int16_t)(ry - (int16_t)(S16(n, 0xC) << 2)));
					S16(e, 0xC) = (int16_t)(S16(e, 0xC) + (int16_t)(CrtRand() % depth - depth / 2));
				}
				const int32_t speed = CrtRand() % P().puffs.speed_rand + P().puffs.speed_min;
				const int32_t jx = CrtRand() % 20;
				S16(e, 0x10) = (int16_t)(jx + (mul32(S32(own::R(SWEPT), 0), speed) >> 12) - 10);
				const int32_t up = CrtRand() % P().puffs.rise_rand;
				const int32_t jy = CrtRand() % 20;
				S16(e, 0x12) = (int16_t)(up + jy + (mul32(S32(own::R(SWEPT), 4), speed) >> 12) - 10);
				const int32_t jz = CrtRand() % 20;
				S16(e, 0x14) = (int16_t)(jz + (mul32(S32(own::R(SWEPT), 8), speed) >> 12) - 10);
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
		U32(h, 0) = fxp::sequence(P().seqs.shard, SEQ_Shard);
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
		for (uint32_t e = own::R(POOL); e < own::R(POOL_END); e += 0x18)
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
			if (P().shards.swirl && P().anchor.at_targets && P().anchor.each_target) ShardSpin(e);
			else ShardMove(w, e);
			live++;
		}
		FieldFree(0x88);
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c <= P().shards.spawn_last)
		{
			for (int k = 0; k < P().shards.per_tick; k++)
			{
				const uint32_t e = PoolFree();
				if (!e) break;
				U32(e, 0) = 4;
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % P().shards.size_rand + P().shards.size_min);
				int16_t at[4];
				if (TargetPoint(k + c * P().shards.per_tick, at))
				{
					// (not the game's) around a target's effect point, flying away from it
					S16(e, 8) = (int16_t)(at[0] + (int16_t)(CrtRand() % P().shards.spread_x - P().shards.spread_x / 2));
					// a tornado from the ground: on the floor under it
					S16(e, 0xA) = (P().shards.swirl && P().shards.ground) ? (int16_t)0 : at[1];
					S16(e, 0xC) = (int16_t)(at[2] + (int16_t)(CrtRand() % P().shards.spread_z - P().shards.spread_z / 2));
					S16(e, 0xE) = (int16_t)(CrtRand() % P().shards.speed_rand + P().shards.speed_min);
					S16(e, 0x10) = (int16_t)(S16(e, 8) - at[0]);
					S16(e, 0x12) = 0;
					S16(e, 0x14) = (int16_t)(S16(e, 0xC) - at[2]);
					if (!S16(e, 0x10) && !S16(e, 0x14)) S16(e, 0x10) = 1;
					VectorNormalS16(e + 0x10, e + 0x10);
					S16(e, 0x16) = (int16_t)(k + c * P().shards.per_tick);      // its target (a tornado turns round it)
					continue;
				}
				U32(e, 8) = U32(own::R(MID), 0);
				U32(e, 0xC) = U32(own::R(MID), 4);
				S16(e, 8) = (int16_t)(S16(e, 8) + (int16_t)(CrtRand() % P().shards.spread_x - P().shards.spread_x / 2));
				S16(e, 0xC) = (int16_t)(S16(e, 0xC) + (int16_t)(CrtRand() % P().shards.spread_z - P().shards.spread_z / 2));
				S16(e, 0xE) = (int16_t)(CrtRand() % P().shards.speed_rand + P().shards.speed_min);
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
				const int32_t speed = r % P().trail.speed_rand + P().trail.speed_min;
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
		U32(h, 0x28) = TrailStates(S32(h, 0x24));
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
				if (S16(st, 6) >= t && t < P().trail.full) continue;
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
		U32(h, 0x28) = TrailStates(S32(h, 0x24));
		FX_PART(PART_TRAIL);
		const uint32_t r = fly ? TrailFlyPolys(h, ot, mode, cursor) : TrailGrowPolys(h, ot, mode, cursor);
		FX_PART(0);
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
		U32(h, 0) = TrailMesh(S16(n, 0xE));
		U32(h, 8) = 0;
		ComposeAffineTransform(&Camera(), &m, (Mat4x3 *)(h + 0x6C));
		const int16_t c = S16(n, 0xC);
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(held_note_trail(n, h);)
		if (c >= P().trail.grow_ticks)
		{
			S32(h, 0x20) = c - P().trail.grow_ticks;
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
		if (S16(n, 0xC) > P().trail.grow_ticks && S32(h, 0x24) == 0) return TASK_END; // (the freed header still holds it)
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
		FX_PART(PART_CRYSTALS);
		PacketCursor() = RenderPrimModel((void *)h, OT44(), 2, PacketCursor());
		FX_PART(0);
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Crystal player (0x5D90B0, queue 0x239A8D0): node +0x10 position, +0x18 scale, +0x1A z spin,
	// +0x1C the layout
	// ------------------------------------------------------------------
	static void CrystalBlock(uint32_t n, uint32_t blk)
	{
		U32(blk, 8) = fxp::data(P().tables.crystal_flags, CRYSTAL_FLAGS, 4, 4);
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
		if (c >= 0 && c <= P().crystals.spawn_last)
		{
			const int16_t a = S16(n, 0x18);
			int16_t angles[3] = { 0, a, 0 };
			S16(n, 0x18) = (int16_t)(S16(n, 0x1A) + a);
			Mat4x3 m;
			BuildRotationMatrixFromAngles(angles, &m);
			int32_t d[3];
			TransformVectorBy3x3Matrix(&m, (const int32_t *)own::R(SWEPT), d);
			const int32_t r = CrtRand() % P().crystals.dist_rand + P().crystals.dist_min;
			const int32_t dist = shl32(mul32(r, FloorLength()), 1) >> 12;
			const uint32_t t = (uint32_t)AddTaskToQueue(PlayerQueue(), own::Fn(ORIG_Crystal));
			DecodeLayout(fxp::layout(P().layouts.crystal, LAYOUT_Crystal, 0x40), t + 0x1C, 0x40);
			const int32_t off = P().crystals.offset, half = off / 2;
			int16_t at[4];
			if (TargetPoint(c, at))
			{
				// (not the game's) around a target's effect point
				S16(t, 0x10) = (int16_t)(at[0] + CrtRand() % off - half);
				S16(t, 0x12) = (int16_t)(at[1] + CrtRand() % off - half);
				S16(t, 0x14) = (int16_t)(at[2] + CrtRand() % off - half);
				S16(t, 0x1A) = (int16_t)(CrtRand() % P().crystals.spin_rand);
				S16(t, 0x18) = (int16_t)(CrtRand() % P().crystals.scale_rand + P().crystals.scale_min);
				S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
				return S16(n, 0xC) > P().crystals.spawn_last ? TASK_END : 0;
			}
			const int32_t rx = CrtRand() % off;
			S16(t, 0x10) = (int16_t)((mul32(d[0], dist) >> 12) + (int32_t)(U32(own::R(ANCHOR), 0) + (uint32_t)rx) - half);
			const int32_t ry = CrtRand() % off;
			S16(t, 0x12) = (int16_t)((int16_t)(ry + (mul32(d[1], dist) >> 12)) + S16(own::R(ANCHOR), 2) - half);
			const int32_t rz = CrtRand() % off;
			S16(t, 0x14) = (int16_t)((mul32(d[2], dist) >> 12) + (int32_t)(U32(own::R(ANCHOR), 4) + (uint32_t)rz) - half);
			S16(t, 0x1A) = (int16_t)(CrtRand() % P().crystals.spin_rand);
			S16(t, 0x18) = (int16_t)(CrtRand() % P().crystals.scale_rand + P().crystals.scale_min);
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) > P().crystals.spawn_last ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5D7570): node +0xC counter
	// ------------------------------------------------------------------
	// the anchor (caster bone 0xF0 + (0, 0, -0x96) through the caster's rotation) into out (x, y, z
	// words; the pad word untouched)
	static void ComputeAnchor(uint32_t out)
	{
		const int32_t slot = CasterSlot();
		if (P().anchor.at_targets)
		{
			// (not the game's) on the targets: their centre (the game takes it at tick 1 only) + (x, z) through the caster's
			// rotation (z > 0: toward the caster), at height y
			CalculateCenterPosition(U16((uint32_t)Ctx(), 2), own::R(CENTRE));
			int32_t v[3] = { P().anchor.x, 0, P().anchor.z };
			TransformVectorBy3x3Matrix((const Mat4x3 *)(Ent(slot) + 0x40), v, v);
			S16(out, 0) = (int16_t)(S16(own::R(CENTRE), 0) + (int16_t)v[0]);
			S16(out, 2) = P().anchor.y;
			S16(out, 4) = (int16_t)(S16(own::R(CENTRE), 4) + (int16_t)v[2]);
			return;
		}
		GetEffectSpawnPosition(Ent(slot), 0xF0, 0x1000, out);
		int32_t v[3] = { 0, 0, P().anchor.mouth };
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
		ComputeAnchor(own::R(ANCHOR));
		const int32_t ax = S16(own::R(ANCHOR), 0), az = S16(own::R(ANCHOR), 4);
		S16(own::R(MID), 2) = 0;
		S16(own::R(MID), 0) = (int16_t)((S16(own::R(CENTRE), 0) + ax) / 2);
		S32(own::R(DIR), 4) = 0;
		S16(own::R(MID), 4) = (int16_t)((S16(own::R(CENTRE), 4) + az) / 2);
		S32(own::R(DIR), 0) = S16(own::R(MID), 0) - ax;
		S32(own::R(DIR), 8) = S16(own::R(MID), 4) - az;
		FloorLength() = Sqrt(NormalizeVectorToFixedPoint(own::R(DIR), own::R(DIR)));
		if (DrawOnly()) return 0;
		if (S16(n, 0xC) == 1)
		{
			CalculateCenterPosition(U16((uint32_t)Ctx(), 2), own::R(CENTRE));
			InitTaskQueuePool(PlayerQueue(), (void *)(TexBase() + 0x18000), 0x11C, 0x40);
		}
		if (S16(n, 0xC) == P().director.swirl)
		{
			TaskNode *t = AddTaskToQueue(FxQueue(), own::Fn(ORIG_Swirl));
			S16((uint32_t)t, 0xC) = 0;
		}
		if (S16(n, 0xC) == P().director.cones)
		{
			for (int k = 0; k < P().cones.count; k++)
			{
				const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), own::Fn(ORIG_Cone));
				S16(t, 0xE) = S16(fxp::data(P().tables.cone_delays, CONE_DELAYS, 16) + (uint32_t)k * 4, 0);
				U32(t, 0x10) = U32(own::R(ANCHOR), 0);
				U32(t, 0x14) = U32(own::R(ANCHOR), 4);
				S16(t, 0xC) = 0;
				S16(t, 0x1C) = P().cones.size;
				S16(t, 0x1E) = P().cones.shrink;
				S16(t, 0x18) = S16(Ent(CasterSlot()), 0xE);
				S16(t, 0x1A) = S16(fxp::data(P().tables.cone_angles, CONE_ANGLES, 16) + (uint32_t)k * 4, 0);
			}
		}
		if (S16(n, 0xC) == P().director.glow)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), own::Fn(ORIG_Glow));
			S16(t, 0xC) = 0;
			S16(t, 0x1C) = P().glow.size;
			S16(t, 0x1E) = P().glow.growth;
		}
		if (S16(n, 0xC) == P().director.puffs)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), own::Fn(ORIG_Puffs));
			S16(t, 0xC) = 0;
			S16(t, 0x18) = P().puffs.sweep_start;
			S16(t, 0x1A) = P().puffs.sweep_step;
		}
		if (S16(n, 0xC) == P().director.crystals)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), own::Fn(ORIG_Spawner));
			S16(t, 0xC) = 0;
			S16(t, 0x1A) = P().crystals.sweep_step;
			S16(t, 0x18) = P().crystals.sweep_start;
		}
		if (S16(n, 0xC) == P().director.trail)
		{
			const uint32_t h = (uint32_t)FieldAlloc(0xAC);
			for (int k = 0; k < 5; k++)
			{
				const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), own::Fn(ORIG_Trail));
				U32(t, 0x10) = U32(own::R(ANCHOR), 0);
				U32(t, 0x14) = U32(own::R(ANCHOR), 4);
				S16(t, 0xC) = 0;
				S16(t, 0xE) = (int16_t)k;
				S16(t, 0x12) = 0;
				S16(t, 0x1C) = P().trail.size;
				const int16_t ry = S16(Ent(CasterSlot()), 0xE);
				U32(h, 0) = TrailMesh(k);
				S32(h, 0x24) = k;
				S16(t, 0x18) = (int16_t)((ry - 0x800) & 0xFFF);
				TrailInitStates(h);
			}
			FieldFree(0xAC);
		}
		if (S16(n, 0xC) == P().director.shards)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), own::Fn(ORIG_Shards));
			U32(t, 0x10) = U32(own::R(ANCHOR), 0);
			S16(t, 0xC) = 0;
			U32(t, 0x14) = U32(own::R(ANCHOR), 4);
		}
		if (S16(n, 0xC) == P().director.damage)
		{
			ActionData *a = Ctx()->actions;
			ApplyActionResultToTargets(a->targets, a->target_count);
		}
		const int16_t c = S16(n, 0xC);
		const auto &fl = P().flash;
		if (c <= fl.in_end) SetScreenFlash((uint32_t)(c * fl.in_step), (uint32_t)fl.colour);
		else if (c >= fl.out_start) SetScreenFlash((uint32_t)((P().director.end - c) * fl.out_step), (uint32_t)fl.colour);
		if (S16(n, 0xC) == 1) BdPlaySE((const void *)fxp::sound(P().sounds.start, (uint32_t)SOUND_Start), 0, 0x80);
		if (S16(n, 0xC) == P().director.blast) BdPlaySE((const void *)fxp::sound(P().sounds.blast, (uint32_t)SOUND_Blast), 0, 0x80);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) <= P().director.end) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// File loader (0x5D7490) and setup (0x5D74B0): the game runs its own; Fujin's programs these (its memory
	// may be a copy's): the texture, the magic buffer, the cast, its first target's and its caster's slots, the
	// root and the director, the particle table freed, the camera and the TIM
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(P().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		TexBase() = layouts::MagicBuffer();
		Ctx() = ctx;
		FirstTarget() = ctx->actions[0].targets[0];
		CasterSlot() = ctx->attacker;
		InitTaskQueuePool(RootQueue(), (void *)own::R(0x23972B0), 0x10, 2);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(RootQueue(), own::Fn(ORIG_Root)) + 0xC) = 0;
		InitTaskQueuePool(FxQueue(), (void *)own::R(0x2397C50), 0x24, 0x64);
		*(uint16_t *)((uint8_t *)AddTaskToQueue(FxQueue(), own::Fn(ORIG_Director)) + 0xC) = 0;
		for (uint32_t a = own::R(POOL); a < own::R(POOL_END); a += 0x18) *(int32_t *)a = 0;
		PlayCameraAnimation((const void *)fxp::camera(P().res.camera, CAMERA));
		QueueTimUpload(TexFile());
		return RootQueue();
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
			const uint32_t w = (uint32_t)AddTaskToQueue(RootQueue(), own::Fn(ORIG_TexRestore));
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
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "director", "Director", "The timeline: when each part starts, the mouth anchor (the caster's bone 0xF0 and an offset, retaken every tick), the screen flash in and out, the sounds; the damage" },
			{ PART_SWIRL, "swirl", "Mouth swirl", "Frost sprites swirling about the caster's mouth: a few a tick, each on a random axis, its radius shrinking as it spins; cleared at its end" },
			{ PART_CONES, "cones", "Breath cones", "Cone models blown from the mouth along the caster's facing, each after its delay and turned on its z axis, shrinking, fading at their end" },
			{ PART_GLOW, "glow", "Mouth glow", "A glow model at the mouth facing the camera, growing (faster from a tick), fading at its end" },
			{ PART_PUFFS, "puffs", "Breath puffs", "Frost puffs blown from the mouth along a sweep of the way to the targets, growing, slowing, falling to a floor, coloured by frame" },
			{ PART_CRYSTALS, "crystals", "Crystals", "Crystal layouts (prim-model players) placed along a sweep of the breath, one a tick, each spun and scaled at random" },
			{ PART_TRAIL, "trail", "Ice trail", "Five ice meshes on the floor from the caster toward the targets: their triangles appear outward, then fly apart spinning and darkening" },
			{ PART_SHARDS, "shards", "Ice shards", "Shard sprites around the midpoint flying away from the mouth, stretched along their way" },
		};
		static const fxp::Group GROUPS[] = {
			{ "sounds", "Sounds", "The start, the blast" },
			{ "flash", "Screen flash", "Its colour, up to a tick its level rising by a step, from a tick falling by a step to the director's end" },
		};
		static const Field FIELDS[] = {
			{ "director", "sounds.start", "Start", "At the director's tick 1", (uint32_t)offsetof(IceBreathParams, sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "sounds.blast", "Blast", "At the director's blast tick", (uint32_t)offsetof(IceBreathParams, sounds.blast), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "director.swirl", "Swirl tick", "The mouth swirl starts at this tick of the director", (uint32_t)offsetof(IceBreathParams, director.swirl), fxp::I16, 0, 300 },
			{ "director", "director.glow", "Glow tick", "The mouth glow starts at this tick", (uint32_t)offsetof(IceBreathParams, director.glow), fxp::I16, 0, 300 },
			{ "director", "director.cones", "Cones tick", "The breath cones start at this tick (at the mouth of then)", (uint32_t)offsetof(IceBreathParams, director.cones), fxp::I16, 0, 300 },
			{ "director", "director.puffs", "Puffs tick", "The breath puffs start at this tick", (uint32_t)offsetof(IceBreathParams, director.puffs), fxp::I16, 0, 300 },
			{ "director", "director.crystals", "Crystals tick", "The crystal spawner starts at this tick (from 1: their pool is made at tick 1)", (uint32_t)offsetof(IceBreathParams, director.crystals), fxp::I16, 1, 300 },
			{ "director", "director.trail", "Trail tick", "The ice trail starts at this tick (at the mouth of then, turned away from the caster's facing)", (uint32_t)offsetof(IceBreathParams, director.trail), fxp::I16, 0, 300 },
			{ "director", "director.shards", "Shards tick", "The ice shards start at this tick (flying away from the mouth of then)", (uint32_t)offsetof(IceBreathParams, director.shards), fxp::I16, 0, 300 },
			{ "director", "director.damage", "Damage tick", "The damage (every target of the action) at this tick", (uint32_t)offsetof(IceBreathParams, director.damage), fxp::I16, 0, 300 },
			{ "director", "director.blast", "Blast tick", "The blast sound at this tick", (uint32_t)offsetof(IceBreathParams, director.blast), fxp::I16, 0, 300 },
			{ "director", "director.end", "End tick", "The director's last tick (the screen flash fades out toward it; the effect ends once every part has)", (uint32_t)offsetof(IceBreathParams, director.end), fxp::I16, 1, 300 },
			{ "director", "anchor.mouth", "Mouth offset", "The mouth: this far along the caster's facing (its z, negative = ahead) from its bone 0xF0", (uint32_t)offsetof(IceBreathParams, anchor.mouth), fxp::I16, -4000, 4000 },
			{ "director", "anchor.at_targets", "On the targets", "0: everything starts at the caster's mouth (the game). 1: at the targets instead, nothing launched from the caster - the anchor is their centre moved by the next values (a new effect's choice)", (uint32_t)offsetof(IceBreathParams, anchor.at_targets), fxp::I16, 0, 1 },
			{ "director", "anchor.x", "Targets: sideways", "On the targets: the anchor this far sideways of their centre (the caster's x)", (uint32_t)offsetof(IceBreathParams, anchor.x), fxp::I16, -8000, 8000 },
			{ "director", "anchor.y", "Targets: height", "On the targets: the anchor's height (y, negative = up; the floor is 0)", (uint32_t)offsetof(IceBreathParams, anchor.y), fxp::I16, -4000, 0 },
			{ "director", "anchor.z", "Targets: before them", "On the targets: the anchor this far before their centre, toward the caster (the way it blows is from there to the centre and on; at least 1)", (uint32_t)offsetof(IceBreathParams, anchor.z), fxp::I16, -8000, 8000 },
			{ "director", "anchor.each_target", "On each target", "On the targets: 1 = the puffs, the crystals and the ice shards around each target's effect point (the next value), cycling through the targets - a flying monster's height too; the floor ice stays under them", (uint32_t)offsetof(IceBreathParams, anchor.each_target), fxp::I16, 0, 1 },
			{ "director", "anchor.bone", "Target point", "On each target: which point of it (0xF1: its body; 0xF0 and up: the model's named points, below: a bone)", (uint32_t)offsetof(IceBreathParams, anchor.bone), fxp::I16, 0, 0xFF },
			{ "director", "flash.colour", "Colour", "The screen flash's colour (0xBBGGRR)", (uint32_t)offsetof(IceBreathParams, flash.colour), fxp::I32, 0, 0xFFFFFF },
			{ "director", "flash.in_end", "Rise until", "The flash level rises up to this tick", (uint32_t)offsetof(IceBreathParams, flash.in_end), fxp::I16, -1, 300 },
			{ "director", "flash.in_step", "Rise step", "Its level: the tick x this (4096 = full)", (uint32_t)offsetof(IceBreathParams, flash.in_step), fxp::I16, 0, 0x1000 },
			{ "director", "flash.out_start", "Fall from", "From this tick the level falls", (uint32_t)offsetof(IceBreathParams, flash.out_start), fxp::I16, 0, 300 },
			{ "director", "flash.out_step", "Fall step", "Its level: (the end tick - the tick) x this", (uint32_t)offsetof(IceBreathParams, flash.out_step), fxp::I16, 0, 0x1000 },
			{ "swirl", "swirl.spawn_last", "Last spawn tick", "Sprites spawn from its tick 0 to this one", (uint32_t)offsetof(IceBreathParams, swirl.spawn_last), fxp::I16, -1, 300 },
			{ "swirl", "swirl.per_tick", "A tick", "How many spawn a tick (100 particles at most, shared with the puffs and the shards)", (uint32_t)offsetof(IceBreathParams, swirl.per_tick), fxp::I16, 0, 100 },
			{ "swirl", "swirl.life", "End tick", "At this tick of its own the swirl ends (its sprites cleared)", (uint32_t)offsetof(IceBreathParams, swirl.life), fxp::I16, 1, 300 },
			{ "swirl", "swirl.size_min", "Size", "A sprite's size: this + a random below the next", (uint32_t)offsetof(IceBreathParams, swirl.size_min), fxp::I16, 0, 0x7FFF },
			{ "swirl", "swirl.size_rand", "Size random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, swirl.size_rand), fxp::I16, 1, 0x7FFF },
			{ "swirl", "swirl.radius_min", "Radius", "Its distance from the mouth: this + a random below the next", (uint32_t)offsetof(IceBreathParams, swirl.radius_min), fxp::I16, 0, 0x7FFF },
			{ "swirl", "swirl.radius_rand", "Radius random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, swirl.radius_rand), fxp::I16, 1, 0x7FFF },
			{ "swirl", "swirl.shrink_min", "Shrink divisor", "Its radius shrinks a tick by the radius / (this + a random below the next)", (uint32_t)offsetof(IceBreathParams, swirl.shrink_min), fxp::I16, 1, 0x7FFF },
			{ "swirl", "swirl.shrink_rand", "Shrink random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, swirl.shrink_rand), fxp::I16, 1, 0x7FFF },
			{ "swirl", "swirl.spin_min", "Spin", "Its angle about its axis a tick: this + a random below the next", (uint32_t)offsetof(IceBreathParams, swirl.spin_min), fxp::I16, 0, 0x1000, fxp::Angle },
			{ "swirl", "swirl.spin_rand", "Spin random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, swirl.spin_rand), fxp::I16, 1, 0x1000, fxp::Angle },
			{ "cones", "cones.count", "Count", "How many cones (their delays and z angles: the tables, 4 at most)", (uint32_t)offsetof(IceBreathParams, cones.count), fxp::I16, 0, 4 },
			{ "cones", "cones.size", "Size", "A cone's starting size (4096 = its model's)", (uint32_t)offsetof(IceBreathParams, cones.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "cones", "cones.shrink", "Shrink", "Its size shrinks a tick by this (which loses 1/20 a tick)", (uint32_t)offsetof(IceBreathParams, cones.shrink), fxp::I16, -0x7FFF, 0x7FFF },
			{ "cones", "cones.fade_from", "Fade from", "From this tick of its own it fades (1024 a tick)", (uint32_t)offsetof(IceBreathParams, cones.fade_from), fxp::I16, 0, 300 },
			{ "cones", "cones.life", "Life", "A cone ends at this tick of its own (after its delay)", (uint32_t)offsetof(IceBreathParams, cones.life), fxp::I16, 1, 300 },
			{ "glow", "glow.size", "Size", "The glow's starting size (4096 = its model's)", (uint32_t)offsetof(IceBreathParams, glow.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "glow", "glow.growth", "Growth", "Its size grows a tick by this", (uint32_t)offsetof(IceBreathParams, glow.growth), fxp::I16, -0x7FFF, 0x7FFF },
			{ "glow", "glow.speedup_from", "Faster from", "From this tick of its own the growth speeds up ((growth + 32) x 4 / 3 a tick)", (uint32_t)offsetof(IceBreathParams, glow.speedup_from), fxp::I16, 0, 300 },
			{ "glow", "glow.fade_from", "Fade from", "From this tick of its own it fades (1024 a tick)", (uint32_t)offsetof(IceBreathParams, glow.fade_from), fxp::I16, 0, 300 },
			{ "glow", "glow.life", "Life", "The glow ends at this tick of its own", (uint32_t)offsetof(IceBreathParams, glow.life), fxp::I16, 1, 300 },
			{ "puffs", "puffs.spawn_last", "Last spawn tick", "Puffs spawn from its tick 0 to this one", (uint32_t)offsetof(IceBreathParams, puffs.spawn_last), fxp::I16, -1, 300 },
			{ "puffs", "puffs.per_tick", "A tick", "How many spawn a tick (100 particles at most, shared)", (uint32_t)offsetof(IceBreathParams, puffs.per_tick), fxp::I16, 0, 100 },
			{ "puffs", "puffs.size_min", "Size", "A puff's size: this + a random below the next (it grows 1/16 a tick)", (uint32_t)offsetof(IceBreathParams, puffs.size_min), fxp::I16, 0, 0x7FFF },
			{ "puffs", "puffs.size_rand", "Size random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, puffs.size_rand), fxp::I16, 1, 0x7FFF },
			{ "puffs", "puffs.spread", "Spread", "Where it starts: the mouth + a random below this (x, z: centred; y: minus 4 a tick of its own)", (uint32_t)offsetof(IceBreathParams, puffs.spread), fxp::I16, 1, 0x7FFF },
			{ "puffs", "puffs.speed_min", "Speed", "Its speed along the swept way: this + a random below the next (losing 1/32 a tick)", (uint32_t)offsetof(IceBreathParams, puffs.speed_min), fxp::I16, 0, 0x7FFF },
			{ "puffs", "puffs.speed_rand", "Speed random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, puffs.speed_rand), fxp::I16, 1, 0x7FFF },
			{ "puffs", "puffs.rise_rand", "Fall random", "Its downward speed: a random below this (+ the way's)", (uint32_t)offsetof(IceBreathParams, puffs.rise_rand), fxp::I16, 1, 0x7FFF },
			{ "puffs", "puffs.sweep_start", "Sweep start", "The way to the targets turned about y by this at its tick 0", (uint32_t)offsetof(IceBreathParams, puffs.sweep_start), fxp::I16, -0x1000, 0x1000, fxp::Angle },
			{ "puffs", "puffs.sweep_step", "Sweep step", "and by this more each tick", (uint32_t)offsetof(IceBreathParams, puffs.sweep_step), fxp::I16, -0x1000, 0x1000, fxp::Angle },
			{ "puffs", "puffs.floor", "Floor", "A puff moves up or down only while above this height (y, negative = up)", (uint32_t)offsetof(IceBreathParams, puffs.floor), fxp::I16, -0x7FFF, 0x7FFF },
			{ "puffs", "puffs.ahead", "Ahead", "Where they start: this far from the anchor along the way to the targets (0: at the anchor, the game; a new effect on the targets: onto them)", (uint32_t)offsetof(IceBreathParams, puffs.ahead), fxp::I16, -8000, 8000 },
			{ "puffs", "puffs.depth", "Depth", "Their spread along z, for the spread's (4096 = the same as x and y, the game; less: a thinner cloud toward the caster)", (uint32_t)offsetof(IceBreathParams, puffs.depth), fxp::I16, 1, 4096 },
			{ "crystals", "crystals.spawn_last", "Last spawn tick", "One crystal a tick from its tick 0 to this one", (uint32_t)offsetof(IceBreathParams, crystals.spawn_last), fxp::I16, -1, 63 },
			{ "crystals", "crystals.sweep_start", "Sweep start", "The swept breath way turned about y by this at its tick 0", (uint32_t)offsetof(IceBreathParams, crystals.sweep_start), fxp::I16, -0x1000, 0x1000, fxp::Angle },
			{ "crystals", "crystals.sweep_step", "Sweep step", "and by this more each tick", (uint32_t)offsetof(IceBreathParams, crystals.sweep_step), fxp::I16, -0x1000, 0x1000, fxp::Angle },
			{ "crystals", "crystals.dist_min", "Distance", "How far along it: (this + a random below the next) x twice the mouth-to-midpoint length / 4096", (uint32_t)offsetof(IceBreathParams, crystals.dist_min), fxp::I16, 0, 0x7FFF },
			{ "crystals", "crystals.dist_rand", "Distance random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, crystals.dist_rand), fxp::I16, 1, 0x7FFF },
			{ "crystals", "crystals.offset", "Offset", "Moved by a random below this on each axis (centred)", (uint32_t)offsetof(IceBreathParams, crystals.offset), fxp::I16, 1, 0x7FFF },
			{ "crystals", "crystals.spin_rand", "Spin random", "Its z spin a frame: a random below this", (uint32_t)offsetof(IceBreathParams, crystals.spin_rand), fxp::I16, 1, 0x1000, fxp::Angle },
			{ "crystals", "crystals.scale_min", "Scale", "Its scale: this + a random below the next (4096 = the layout's)", (uint32_t)offsetof(IceBreathParams, crystals.scale_min), fxp::I16, 0, 0x7FFF },
			{ "crystals", "crystals.scale_rand", "Scale random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, crystals.scale_rand), fxp::I16, 1, 0x7FFF },
			{ "trail", "trail.size", "Size", "The meshes' scale (4096 = theirs)", (uint32_t)offsetof(IceBreathParams, trail.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "trail", "trail.grow_ticks", "Appear ticks", "Its triangles appear for this many ticks (each by its distance), then fly apart", (uint32_t)offsetof(IceBreathParams, trail.grow_ticks), fxp::I16, 0, 300 },
			{ "trail", "trail.full", "All shown at", "While appearing, from this tick every triangle is shown", (uint32_t)offsetof(IceBreathParams, trail.full), fxp::I16, 0, 300 },
			{ "trail", "trail.speed_min", "Speed", "A flying triangle's speed outward: this + a random below the next (losing 1/16 a tick)", (uint32_t)offsetof(IceBreathParams, trail.speed_min), fxp::I16, 0, 0x7FFF },
			{ "trail", "trail.speed_rand", "Speed random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, trail.speed_rand), fxp::I16, 1, 0x7FFF },
			{ "shards", "shards.spawn_last", "Last spawn tick", "Shards spawn from its tick 0 to this one", (uint32_t)offsetof(IceBreathParams, shards.spawn_last), fxp::I16, -1, 300 },
			{ "shards", "shards.per_tick", "A tick", "How many spawn a tick (100 particles at most, shared)", (uint32_t)offsetof(IceBreathParams, shards.per_tick), fxp::I16, 0, 100 },
			{ "shards", "shards.size_min", "Size", "A shard's size: this + a random below the next", (uint32_t)offsetof(IceBreathParams, shards.size_min), fxp::I16, 0, 0x7FFF },
			{ "shards", "shards.size_rand", "Size random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, shards.size_rand), fxp::I16, 1, 0x7FFF },
			{ "shards", "shards.spread_x", "Spread x", "Around the midpoint: a random below this on x (centred)", (uint32_t)offsetof(IceBreathParams, shards.spread_x), fxp::I16, 1, 0x7FFF },
			{ "shards", "shards.spread_z", "Spread z", "and below this on z (centred)", (uint32_t)offsetof(IceBreathParams, shards.spread_z), fxp::I16, 1, 0x7FFF },
			{ "shards", "shards.speed_min", "Speed", "Its speed away from the mouth: this + a random below the next", (uint32_t)offsetof(IceBreathParams, shards.speed_min), fxp::I16, 0, 0x7FFF },
			{ "shards", "shards.speed_rand", "Speed random", "(at least 1)", (uint32_t)offsetof(IceBreathParams, shards.speed_rand), fxp::I16, 1, 0x7FFF },
			{ "shards", "shards.swirl", "Tornado turn", "On each target: 0 = the shards fly straight away from it (the game); else they turn round it this much a tick (4096 a turn; negative: the other way), a small tornado", (uint32_t)offsetof(IceBreathParams, shards.swirl), fxp::I16, -2048, 2048 },
			{ "shards", "shards.rise", "Tornado rise", "A tornado's shards rise this much a tick", (uint32_t)offsetof(IceBreathParams, shards.rise), fxp::I16, -400, 400 },
			{ "shards", "shards.widen", "Tornado widening", "A tornado's radius grows by this / 4096 a tick (0: a column)", (uint32_t)offsetof(IceBreathParams, shards.widen), fxp::I16, -1024, 1024 },
			{ "shards", "shards.ground", "Tornado from the ground", "A tornado's shards: 1 = start on the floor under their target and rise from there; 0 = start at the target's point", (uint32_t)offsetof(IceBreathParams, shards.ground), fxp::I16, 0, 1 },
			{ "trail", "trail_meshes.mesh0", "Mesh 0", "The ice trail's mesh 0 (a prim model: an edited one keeps its vertices and has only gouraud textured triangles, no more than the game's)", (uint32_t)offsetof(IceBreathParams, trail_meshes.mesh0), fxp::Model, 0, 0x7FFFFFFF },
			{ "trail", "trail_meshes.mesh1", "Mesh 1", "The ice trail's mesh 1 (a prim model: an edited one keeps its vertices and has only gouraud textured triangles, no more than the game's)", (uint32_t)offsetof(IceBreathParams, trail_meshes.mesh1), fxp::Model, 0, 0x7FFFFFFF },
			{ "trail", "trail_meshes.mesh2", "Mesh 2", "The ice trail's mesh 2 (a prim model: an edited one keeps its vertices and has only gouraud textured triangles, no more than the game's)", (uint32_t)offsetof(IceBreathParams, trail_meshes.mesh2), fxp::Model, 0, 0x7FFFFFFF },
			{ "trail", "trail_meshes.mesh3", "Mesh 3", "The ice trail's mesh 3 (a prim model: an edited one keeps its vertices and has only gouraud textured triangles, no more than the game's)", (uint32_t)offsetof(IceBreathParams, trail_meshes.mesh3), fxp::Model, 0, 0x7FFFFFFF },
			{ "trail", "trail_meshes.mesh4", "Mesh 4", "The ice trail's mesh 4 (a prim model: an edited one keeps its vertices and has only gouraud textured triangles, no more than the game's)", (uint32_t)offsetof(IceBreathParams, trail_meshes.mesh4), fxp::Model, 0, 0x7FFFFFFF },
			{ "swirl", "seqs.swirl", "Swirl", "The sprite sequence swirl (a flipbook of 24 frames: an edited one keeps as many)", (uint32_t)offsetof(IceBreathParams, seqs.swirl), fxp::Seq, 0, 0x7FFFFFFF },
			{ "puffs", "seqs.puff", "Puff", "The sprite sequence puff (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(IceBreathParams, seqs.puff), fxp::Seq, 0, 0x7FFFFFFF },
			{ "shards", "seqs.shard", "Shard", "The sprite sequence shard (a flipbook of 16 frames: an edited one keeps as many)", (uint32_t)offsetof(IceBreathParams, seqs.shard), fxp::Seq, 0, 0x7FFFFFFF },
			{ "crystals", "layouts.crystal", "Crystal", "The prim-model layout crystal (its models' animation)", (uint32_t)offsetof(IceBreathParams, layouts.crystal), fxp::Layout, 0, 0x7FFFFFFF },
			{ "cones", "models.cone", "Cone model", "The prim model cone (a mesh of 73 vertices, 72 primitives: an edited one keeps at least as many vertices)", (uint32_t)offsetof(IceBreathParams, models.cone), fxp::Model, 0, 0x7FFFFFFF },
			{ "glow", "models.glow", "Glow model", "The prim model glow (a mesh of 33 vertices, 32 primitives: an edited one keeps at least as many vertices)", (uint32_t)offsetof(IceBreathParams, models.glow), fxp::Model, 0, 0x7FFFFFFF },
			{ "cones", "tables.cone_delays", "Cone delays", "Each breath cone's delay (ticks before it shows and starts its clock) (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(IceBreathParams, tables.cone_delays), fxp::Data, 0, 0x7FFFFFFF },
			{ "cones", "tables.cone_angles", "Cone z angles", "Each breath cone's turn about its z axis (4096 = a full turn) (16 bytes: an edited one keeps its length)", (uint32_t)offsetof(IceBreathParams, tables.cone_angles), fxp::Data, 0, 0x7FFFFFFF },
			{ "puffs", "tables.puff_colours", "Puff colours", "A breath puff's colour words by its frame (red, green, blue, the GPU code byte) (64 bytes: an edited one keeps its length)", (uint32_t)offsetof(IceBreathParams, tables.puff_colours), fxp::Data, 0, 0x7FFFFFFF },
			{ "crystals", "tables.crystal_flags", "Crystal flags", "The crystal layout's object flag word (0x10000000: its position through the camera; its draw flags, the semi-transparency bit forced on) (4 bytes: an edited one keeps its length)", (uint32_t)offsetof(IceBreathParams, tables.crystal_flags), fxp::Data, 0, 0x7FFFFFFF },
			{ "director", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(IceBreathParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "director", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(IceBreathParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag173_ice_breath()
	{
		fxp::register_params({ 173, "Ice Breath", &ib173::live, &ib173::GAME, sizeof(ib173::IceBreathParams),
		                       ib173::table::PARTS, (int)(sizeof(ib173::table::PARTS) / sizeof(ib173::table::PARTS[0])),
		                       ib173::table::FIELDS, (int)(sizeof(ib173::table::FIELDS) / sizeof(ib173::table::FIELDS[0])),
		                       ib173::table::GROUPS, (int)(sizeof(ib173::table::GROUPS) / sizeof(ib173::table::GROUPS[0])) });
		own::note(ib173::ORIG_FileLoader, (void *)ib173::FileLoader);
		own::note(ib173::ORIG_Setup, (void *)ib173::Setup);
		own::note(ib173::ORIG_Root, (void *)ib173::RootTask);
		register_port(ib173::ORIG_Root, (void *)ib173::RootTask, "173 RootTask", 173);
		own::note(ib173::ORIG_TexRestore, (void *)ib173::TexRestoreTask);
		register_port(ib173::ORIG_TexRestore, (void *)ib173::TexRestoreTask, "173 TexRestore", 173);
		own::note(ib173::ORIG_Director, (void *)ib173::DirectorTask);
		register_port(ib173::ORIG_Director, (void *)ib173::DirectorTask, "173 Director", 173);
		own::note(ib173::ORIG_Swirl, (void *)ib173::SwirlTask);
		register_port(ib173::ORIG_Swirl, (void *)ib173::SwirlTask, "173 MouthSwirl", 173);
		own::note(ib173::ORIG_Cone, (void *)ib173::ConeTask);
		register_port(ib173::ORIG_Cone, (void *)ib173::ConeTask, "173 BreathCone", 173);
		own::note(ib173::ORIG_Glow, (void *)ib173::GlowTask);
		register_port(ib173::ORIG_Glow, (void *)ib173::GlowTask, "173 MouthGlow", 173);
		own::note(ib173::ORIG_Puffs, (void *)ib173::PuffsTask);
		register_port(ib173::ORIG_Puffs, (void *)ib173::PuffsTask, "173 BreathPuffs", 173);
		own::note(ib173::ORIG_Trail, (void *)ib173::TrailTask);
		register_port(ib173::ORIG_Trail, (void *)ib173::TrailTask, "173 IceTrail", 173);
		own::note(ib173::ORIG_Shards, (void *)ib173::ShardsTask);
		register_port(ib173::ORIG_Shards, (void *)ib173::ShardsTask, "173 IceShards", 173);
		own::note(ib173::ORIG_Spawner, (void *)ib173::SpawnerTask);
		register_port(ib173::ORIG_Spawner, (void *)ib173::SpawnerTask, "173 CrystalSpawner", 173);
		own::note(ib173::ORIG_Crystal, (void *)ib173::CrystalTask);
		register_port(ib173::ORIG_Crystal, (void *)ib173::CrystalTask, "173 Crystal", 173);
		// 30 fps layer: see mag173_ice_breath_held.inc
		FX_HELD(register_mag173_held();)
	}
	FX_LAB_MODULE(register_mag173_ice_breath);
}

#ifdef FF8_FX_HELD
#include "mag173_ice_breath_held.inc"
#endif
