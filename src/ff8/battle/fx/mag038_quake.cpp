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

// Effect 38: Quake (spell, MAG_038_*).
//
// Structure (setup MAG_038_QUAKE 0x8D8510, file loader 0x575620; code 0x8D8510..0x8DC530). The
// module is built on the shared effect library that follows it (0x8DC530.., "Cure emitter"
// helpers: Effect_AddTaskAndInitFromCtx links a child to its parent, +0x28 = live children,
// +0x29 = phase index into a table of phase functions, +0x24 = tick counter, +0x26 bit0 = done):
//   Root (0x8D8610) - alternates the packet arena, spawns one emitter per action (phase 1/2),
//     runs the four effect queues, ends when they are empty.
//   Emitter (0x8D8720) - follows the target; phase 0 spawns the ground sequence + the camera
//     shake task when the battle stage has a ground group (stage slot 1 visible with objects),
//     else the fallback task (sounds + shake only); phase 1 applies the damage.
//   Ground sequence (0x8D8800) - 12 phases. Every phase up to 7 re-renders the stage ground
//     group (0x8D8A40 -> 0x8D8B10) through the module's own mesh: every triangle / quad of the
//     ground is split into sub-triangles (one per edge + the centroid), each tagged with one of 10
//     zones (a 16 x 16 zone map of the ground, 0x1642568); the zone ripples (sin wave, then fall)
//     lift the sub-triangles, cracks open along zone borders (crack lines, then side walls), rocks,
//     dust and chips are thrown, the party / enemies are lifted and dropped. The mesh state
//     machine (0x279299E): 0 build, 1 find zone borders (16 ticks), 2 wait for the quake, 3 crack
//     lines grow (13 ticks), 4 crack walls grow + chips (12 ticks), 5 ripples + rocks + dust
//     (40 ticks), 6 ripples settle, 7 done (stage ground visible again).
//     Phases: 1 build, 2 wait 16, 3 the side opposite the targets (the caster's) drawn with the
//     stage light, 4 it fades to black + quake sound, 5 it is hidden and the targets' shadows go
//     off, 6 wait for the ripples + second sound, 7 the targets bounce on the ripples,
//     8 restore flags, 9 fade back, 10 restore colours and end.
//   Camera shake (0x8DC250) - camera shake word 0x1D97712 from table A (ticks 3..), then table B
//     while the ripples run.
//   Debris (0x8DA900) - a ground piece thrown up by a ripple: spins, falls back, two triangles.
//   Rock (0x8DB030) / dust (0x8DB410) / chip (0x8DB630) - sprite flipbooks thrown from the ground.
//   Fallback (0x8DC360) - no ground group: two sounds and table B shake.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x278C8E0..0x2792E60 (packet cursor, zone ripples, saved entity flags /
// positions, task pools, mesh state, packet templates) + the magic buffer (packet arenas, mesh
// entries 0x70 bytes each at +0x28000, vertex zone masks at +0x33600).

#include "mag_common.h"

namespace ff8fx
{
namespace quake038
{
	using namespace eng;
	using namespace magc;

	// ------------------------------------------------------------------
	// raw memory access (node / entry layouts follow the listings offset by offset)
	// ------------------------------------------------------------------
	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline int8_t &S8(uint32_t p, int32_t o) { return AT<int8_t>(p, o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }

	// --- module globals ---
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x278C8E0); }
	inline int16_t &WaveTick() { return var<int16_t>(0x278C8E4); }     // state 1 tick (0..16)
	inline int16_t &HalfZ() { return var<int16_t>(0x278C8E6); }        // zone map half extents
	inline int16_t &HalfX() { return var<int16_t>(0x278C8E8); }
	inline int16_t &CrackTick() { return var<int16_t>(0x278C8EA); }    // state 3 tick
	inline int16_t &PrimCount() { return var<int16_t>(0x278C8EC); }    // ground prims (last id)
	static const uint32_t SAVED_FLAGS = 0x278C8F0;                      // 7 x u32: entity flags at phase 3
	static const uint32_t ZONES = 0x278C910;                            // 10 x 16 bytes (see ZoneReset)
	inline int16_t &TargetsParty() { return var<int16_t>(0x278C9B0); } // 1: the first target is a party member
	inline int16_t &FallTick() { return var<int16_t>(0x2792B40); }      // state 5 tick
	inline uint32_t &Arena1() { return var<uint32_t>(0x2792B44); }
	inline uint32_t &Arena0() { return var<uint32_t>(0x2792B48); }
	inline int16_t &DebrisCount() { return var<int16_t>(0x279299C); }
	inline int16_t &State() { return var<int16_t>(0x279299E); }         // mesh state 0..7
	static const uint32_t CRACK_TEMPLATE = 0x27929A0;                    // POLY_FT4 template (crack lines)
	inline uint32_t &SeqCtx() { return var<uint32_t>(0x27929C8); }      // ground sequence node
	inline int16_t &VertexBase() { return var<int16_t>(0x2792D68); }
	inline int16_t &ChipCount() { return var<int16_t>(0x2792D6A); }
	inline int16_t &RockCount() { return var<int16_t>(0x2792D6C); }
	inline int16_t &ZoneOffsetZ() { return var<int16_t>(0x2792D6E); }
	inline int16_t &RippleTick() { return var<int16_t>(0x2792D70); }   // state 4 tick
	static const uint32_t SAVED_POS = 0x2792D78;                         // 7 x 0x14: entity positions
	inline int16_t &BoundXHi() { return var<int16_t>(0x2792E04); }
	inline int16_t &BoundXLo() { return var<int16_t>(0x2792E06); }
	inline uint32_t &VertexZones() { return var<uint32_t>(0x2792E0C); } // per vertex: u16 zone mask, u16 count
	inline int16_t &DustCount() { return var<int16_t>(0x2792E18); }
	inline uint32_t &Entries() { return var<uint32_t>(0x2792E1C); }     // 0x70-byte mesh entries
	inline int16_t &QuakeGo() { return var<int16_t>(0x2792E20); }
	inline int16_t &BoundZHi() { return var<int16_t>(0x2792E2C); }
	inline int16_t &BoundZLo() { return var<int16_t>(0x2792E2E); }
	static const uint32_t WALL_TEMPLATE = 0x2792E38;                     // POLY_FT4 template (crack walls)

	// queues (TaskQueue headers)
	static const uint32_t Q_DEBRIS = 0x278E538, Q_SPRITES = 0x2792988, Q_EMITTER = 0x27929D0, Q_SEQUENCE = 0x2792B50;

	// exe data
	static const uint32_t ZONE_MAP = 0x1642568;       // 16 x 32 bytes: zone of a ground cell
	static const uint32_t ZONE_MAP_B = 0x1642968;     // 16 x 32 bytes: crack delay of a ground cell
	static const uint32_t WALL_DEPTH = 0x164253C;     // s16 per crack-wall step
	static const uint32_t STEP_SHADE = 0x1642558;     // u8 grey level per crack step
	static const uint32_t SHAKE_A = 0x164364C;        // s16 per tick, 0x7F ends
	static const uint32_t SHAKE_B = 0x1643678;        // s16 x 16
	static const uint32_t SEQ_Rock = 0x16434A0, SEQ_Dust = 0x164335C, SEQ_Chip = 0x1643218;
	static const void *const SOUND_Rumble = (const void *)0x1642D68;
	static const void *const SOUND_Quake = (const void *)0x1642D6C;

	static const uint32_t STAGE_GROUND = 0x1D989C0;   // stage slot 1: skeleton, objects, animation
	static const uint32_t ENTITY0 = 0x1D972C0, ENTITY3 = 0x1D97494, ENTITY_SIZE = 0x9C;
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// --- task / phase functions ---
	static const uint32_t ORIG_Root = 0x8D8610;
	static const uint32_t ORIG_Emitter = 0x8D8720;
	static const uint32_t ORIG_Sequence = 0x8D8800;
	static const uint32_t ORIG_Debris = 0x8DA900;
	static const uint32_t ORIG_Rock = 0x8DB030;
	static const uint32_t ORIG_Dust = 0x8DB410;
	static const uint32_t ORIG_Chip = 0x8DB630;
	static const uint32_t ORIG_Shake = 0x8DC250;
	static const uint32_t ORIG_Fallback = 0x8DC360;

	// --- engine / effect library functions (original addresses) ---
	// effect library (0x8DC530..): parent-linked task spawn, emitter anchor, matrices
	inline void ReleaseLinkedTask(uint32_t n) { fn<void (__cdecl *)(uint32_t)>(0x8DC530)(n); }
	inline uint32_t AddLinkedTask(uint32_t q, uint32_t task, int32_t size, uint32_t parent) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, int32_t, uint32_t)>(0x8DC540)(q, task, size, parent); }
	inline void EmitterUpdatePos(uint32_t n) { fn<void (__cdecl *)(uint32_t)>(0x8DC610)(n); }
	inline void EmitterComputeBounds(uint32_t n) { fn<void (__cdecl *)(uint32_t)>(0x8DC870)(n); }
	inline void UpdateTargetPosFromBones(uint32_t n) { fn<void (__cdecl *)(uint32_t)>(0x8DC740)(n); }
	inline void CopyMidpointFromSource(uint32_t n, void *out) { fn<void (__cdecl *)(uint32_t, void *)>(0x8DC700)(n, out); }
	inline void MatrixUnit(void *m) { fn<void (__cdecl *)(void *)>(0x8DD770)(m); }                        // MAG_022_sub_8DD770
	inline void MatrixRotY(void *m, int32_t a) { fn<void (__cdecl *)(void *, int32_t)>(0x8DD8A0)(m, a); } // MAG_022_sub_8DD8A0
	inline void MatrixRotX(void *m, int32_t a) { fn<void (__cdecl *)(void *, int32_t)>(0x8DD7E0)(m, a); } // sub_8DD7E0
	// battle
	inline void ReadAnimation(uint32_t header, uint32_t cmd, int32_t a) { fn<void (__cdecl *)(uint32_t, uint32_t, int32_t)>(0x509440)(header, cmd, a); }
	inline void ChainTransformationConditional(uint32_t entity, int32_t kind) { fn<void (__cdecl *)(uint32_t, int32_t)>(0x505CE0)(entity, kind); }
	inline int32_t CartesianToGameAngle(int32_t dx, int32_t dz) { return fn<int32_t (__cdecl *)(int32_t, int32_t)>(0x56D160)(dx, dz); }
	// GTE
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(int32_t *dst) { fn<void (__cdecl *)(int32_t *)>(0x45E3C0)(dst); }
	inline void GteStoreSXY012_FT3(uint32_t p) { fn<void (__cdecl *)(uint32_t)>(0x45E2E0)(p); }
	inline void GteStoreSXY012_FT4(uint32_t p) { fn<void (__cdecl *)(uint32_t)>(0x45E320)(p); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(uint32_t *dst) { fn<void (__cdecl *)(uint32_t *)>(0x45E3D0)(dst); }
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteSetBackgroundVector(int32_t a, int32_t b, int32_t c) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DCF0)(a, b, c); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }

	// OT slot of an OTZ word (bucket list base + (otz >> shift) words)
	inline uint32_t OtSlot(uint32_t ot, uint32_t otz, int shift) { return ot + (otz >> shift) * 4; }

	// an effect-library node: +0x0C cast context, +0x10 root, +0x18 parent, +0x24 ticks, +0x26
	// flags (bit0 done, bit2 hidden), +0x28 live children, +0x29 phase, +0x2A action, +0x2D first
	// target slot
	inline int PhaseOf(uint32_t n) { return S8(n, 0x29); }
	inline void NextPhase(uint32_t n) { U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1); }
	static uint32_t EndCheck(uint32_t n)
	{
		U16(n, 0x24) = (uint16_t)(U16(n, 0x24) + 1);
		if ((U8(n, 0x26) & 1) && U8(n, 0x28) == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	inline uint32_t Entry(int32_t id) { return Entries() + (uint32_t)(int16_t)id * 0x70; }
	inline uint32_t EntityAt(int slot) { return ENTITY0 + ENTITY_SIZE * slot; }

	// the ground sub-triangle drawn for edge k of a prim: its three vertex copies (edge ends + the
	// prim's centroid) lifted by the zone height
	struct EdgeVerts { int16_t v[12]; };

	// uninitialised stack words the original reads (see Ground_8D8B10): the high halves of the
	// dwords it copies from its vector locals come from older frames at the same depth
	struct StackJunk { uint16_t w6, wA; };
}
}

#ifdef FF8_FX_HELD
#include "mag038_quake_held.h"
#endif

namespace ff8fx
{
namespace quake038
{
	// ==================================================================
	// zone ripples (10 records of 16 bytes at 0x278C910):
	//   +0 zone index, +2 height (drawn lift, <= 0 = up), +4 angle, +6 angle speed, +8 amplitude,
	//   +0xA phase (0 rise along sin, 1 fall), +0xC fall speed
	// ==================================================================
	// sub_8DA5B0: reset a zone (random speed / amplitude by zone)
	static void ZoneReset(uint32_t z)
	{
		int32_t kind = S16(z, 0);
		S16(z, 0xA) = 0;
		S16(z, 2) = 0;
		S16(z, 4) = 0;
		S16(z, 0xC) = 0;
		switch (kind)
		{
		case 0:
			S16(z, 6) = 0;
			S16(z, 8) = 0;
			break;
		case 6: case 9:
			S16(z, 6) = (int16_t)((CrtRand() & 0x7F) + 0x100);
			S16(z, 8) = (int16_t)((CrtRand() & 0x3FF) + 0x400);
			break;
		case 1: case 2: case 3: case 4: case 7: case 8:
			S16(z, 6) = (int16_t)((CrtRand() & 0x7F) + 0x100);
			S16(z, 8) = (int16_t)((CrtRand() & 0x3FF) + 0x100);
			break;
		case 5:
			S16(z, 6) = (int16_t)((CrtRand() & 0x7F) + 0x100);
			S16(z, 8) = (int16_t)((CrtRand() & 0xFF) + 0x100);
			break;
		default:
			break;
		}
	}

	// sub_8DA580: every zone gets its index and a reset
	static void ZonesInit()
	{
		for (int i = 0; i < 10; i++)
		{
			uint32_t z = ZONES + 0x10 * i;
			S16(z, 0) = (int16_t)i;
			ZoneReset(z);
		}
	}

	// sub_8DA680: rise along sin(angle) * amplitude up to angle 0x400, then fall back (a zone that
	// lands is reset)
	static void ZonesUpdate()
	{
		for (int i = 0; i < 10; i++)
		{
			uint32_t z = ZONES + 0x10 * i;
			switch (S16(z, 0xA))
			{
			case 0:
			{
				S16(z, 4) = (int16_t)(S16(z, 4) + S16(z, 6));
				if (S16(z, 4) > 0x400)
				{
					S16(z, 4) = 0x400;
					S16(z, 0xA) = (int16_t)(S16(z, 0xA) + 1);
				}
				int32_t s = ComputeSin(S16(z, 4));
				S16(z, 2) = (int16_t)-(mul32(s, S16(z, 8)) / 4096);
				break;
			}
			case 1:
				S16(z, 0xC) = (int16_t)(S16(z, 0xC) + 0x60);
				S16(z, 2) = (int16_t)(S16(z, 2) + S16(z, 0xC));
				if (S16(z, 2) >= 0) ZoneReset(z);
				break;
			}
		}
	}

	// sub_8DA710: every lifted zone falls back to 0; 1 when all are down
	static int32_t ZonesSettle()
	{
		int32_t all = 1;
		for (int i = 0; i < 10; i++)
		{
			uint32_t z = ZONES + 0x10 * i;
			if (S16(z, 2) == 0) continue;
			S16(z, 0xC) = (int16_t)(S16(z, 0xC) + 0x60);
			S16(z, 2) = (int16_t)(S16(z, 2) + S16(z, 0xC));
			if (S16(z, 2) < 0) all = 0;
			else S16(z, 2) = 0;
		}
		return all;
	}

	inline int16_t ZoneHeight(int zone) { return S16(ZONES + 0x10 * zone, 2); }

	// sub_8D9A80 / sub_8D9B30: ground cell of (x, z) (clamped to the half extents, 16 x 16 cells)
	// looked up in a 16 x 32 byte map
	static uint16_t ZoneLookup(uint32_t map, int32_t x, int32_t z)
	{
		int16_t hx = HalfX();
		int16_t nx = (int16_t)-x;
		int16_t cx;
		if (nx <= -hx) cx = (int16_t)-hx;
		else if (nx >= hx - 1) cx = (int16_t)(hx - 1);
		else cx = nx;
		int32_t col = (int16_t)(hx + cx) / (hx / 16);
		int16_t hz = HalfZ();
		int16_t cz = (int16_t)((int16_t)z - ZoneOffsetZ());
		if (cz <= -hz) cz = (int16_t)-hz;
		else if (cz >= hz - 1) cz = (int16_t)(hz - 1);
		int32_t row = (int16_t)(hz + cz) / (hz / 16);
		return U8(map, (int16_t)row * 32 + (int16_t)col);
	}

	// ==================================================================
	// mesh entries (0x70 bytes, [0x2792E1C] + id * 0x70):
	//   +0x00 vertices (x, y, z, pad) x 4; a triangle keeps its centroid in slot 3, a quad in
	//         slot 4 (+0x20)
	//   +0x28 colour + code 0x24, +0x2C colour * 3 / 5 + code 0x2C, +0x30 tpage, +0x32 clut
	//   +0x34 vertex ids (4), then prim id + 0x190 (the centroid's id)
	//   +0x40 crack wall step per (edge, side) (3 x 4), +0x4C spawn guard bytes
	//   +0x58 zone per edge, +0x5C crack delay per edge, +0x60 border bits per edge (bit m: side m)
	//   +0x64 u per vertex slot, +0x69 v per vertex slot, +0x6E 1 triangle / 2 quad
	// ==================================================================
	// sub_8D9380: triangle (20-byte colour triangle of the stage object) into entry `id`
	static void BuildTriangle(int32_t id, uint32_t vtx, uint32_t poly)
	{
		uint32_t e = Entry(id);
		U8(e, 0x6E) = 1;
		int32_t i0 = U16(poly, 0) & 0xFFF, i1 = U16(poly, 2) & 0xFFF, i2 = U16(poly, 4) & 0xFFF;
		int16_t base = VertexBase();
		S16(e, 0x34) = (int16_t)(base + i0);
		S16(e, 0x36) = (int16_t)(base + i1);
		S16(e, 0x38) = (int16_t)(base + i2);
		S16(e, 0x3A) = (int16_t)(id + 0x190);
		const int16_t *p0 = (const int16_t *)(vtx + i0 * 8), *p1 = (const int16_t *)(vtx + i1 * 8), *p2 = (const int16_t *)(vtx + i2 * 8);
		S16(e, 0) = p0[0]; S16(e, 2) = p0[1]; S16(e, 4) = p0[2];
		S16(e, 8) = p1[0]; S16(e, 0xA) = p1[1]; S16(e, 0xC) = p1[2];
		S16(e, 0x10) = p2[0]; S16(e, 0x12) = p2[1]; S16(e, 0x14) = p2[2];
		S16(e, 0x18) = (int16_t)((p2[0] + p1[0] + p0[0]) / 3);
		S16(e, 0x1A) = (int16_t)((p2[1] + p1[1] + p0[1]) / 3);
		S16(e, 0x1C) = (int16_t)((p2[2] + p1[2] + p0[2]) / 3);
		U8(e, 0x6B) = U8(poly, 7);
		U8(e, 0x64) = U8(poly, 8);
		U8(e, 0x69) = U8(poly, 9);
		U8(e, 0x6A) = U8(poly, 0xD);
		U8(e, 0x65) = U8(poly, 0xC);
		U8(e, 0x66) = U8(poly, 6);
		U8(e, 0x2B) = 0x24;
		U8(e, 0x67) = (uint8_t)((U8(poly, 6) + U8(poly, 0xC) + U8(poly, 8)) / 3);
		U8(e, 0x6C) = (uint8_t)((U8(poly, 7) + U8(poly, 0xD) + U8(poly, 9)) / 3);
		U16(e, 0x30) = U16(poly, 0xE);
		U16(e, 0x32) = U16(poly, 0xA);
		U8(e, 0x28) = U8(poly, 0x10);
		U8(e, 0x2A) = U8(poly, 0x12);
		U8(e, 0x29) = U8(poly, 0x11);
		U8(e, 0x2C) = (uint8_t)(U8(poly, 0x10) * 3 / 5);
		U8(e, 0x2D) = (uint8_t)(U8(poly, 0x11) * 3 / 5);
		U8(e, 0x2E) = (uint8_t)(U8(poly, 0x12) * 3 / 5);
		U8(e, 0x2F) = 0x2C;
	}

	// sub_8D90A0: quad (24-byte colour quad) into entry `id`
	static void BuildQuad(int32_t id, uint32_t vtx, uint32_t poly)
	{
		uint32_t e = Entry(id);
		U8(e, 0x6E) = 2;
		int32_t i0 = U16(poly, 0) & 0xFFF, i1 = U16(poly, 2) & 0xFFF, i2 = U16(poly, 4) & 0xFFF, i3 = U16(poly, 6) & 0xFFF;
		int16_t base = VertexBase();
		S16(e, 0x34) = (int16_t)(base + i0);
		S16(e, 0x36) = (int16_t)(base + i1);
		S16(e, 0x38) = (int16_t)(base + i2);
		S16(e, 0x3A) = (int16_t)(base + i3);
		S16(e, 0x3C) = (int16_t)(id + 0x190);
		const int16_t *p0 = (const int16_t *)(vtx + i0 * 8), *p1 = (const int16_t *)(vtx + i1 * 8);
		const int16_t *p2 = (const int16_t *)(vtx + i2 * 8), *p3 = (const int16_t *)(vtx + i3 * 8);
		S16(e, 0) = p0[0]; S16(e, 2) = p0[1]; S16(e, 4) = p0[2];
		S16(e, 8) = p1[0]; S16(e, 0xA) = p1[1]; S16(e, 0xC) = p1[2];
		S16(e, 0x10) = p2[0]; S16(e, 0x12) = p2[1]; S16(e, 0x14) = p2[2];
		S16(e, 0x18) = p3[0]; S16(e, 0x1A) = p3[1]; S16(e, 0x1C) = p3[2];
		S16(e, 0x20) = (int16_t)((p3[0] + p2[0] + p1[0] + p0[0]) / 4);
		S16(e, 0x22) = (int16_t)((p3[1] + p2[1] + p1[1] + p0[1]) / 4);
		S16(e, 0x24) = (int16_t)((p3[2] + p2[2] + p1[2] + p0[2]) / 4);
		U8(e, 0x66) = U8(poly, 0x10);
		U8(e, 0x69) = U8(poly, 9);
		U8(e, 0x64) = U8(poly, 8);
		U8(e, 0x65) = U8(poly, 0xC);
		U8(e, 0x6A) = U8(poly, 0xD);
		U8(e, 0x6B) = U8(poly, 0x11);
		U8(e, 0x67) = U8(poly, 0x12);
		U8(e, 0x6C) = U8(poly, 0x13);
		U8(e, 0x68) = (uint8_t)((U8(poly, 0x12) + U8(poly, 0x10) + U8(poly, 0xC) + U8(poly, 8)) / 4);
		U8(e, 0x6D) = (uint8_t)((U8(poly, 0x13) + U8(poly, 0x11) + U8(poly, 0xD) + U8(poly, 9)) / 4);
		U16(e, 0x32) = U16(poly, 0xA);
		U16(e, 0x30) = U16(poly, 0xE);
		U8(e, 0x28) = U8(poly, 0x14);
		U8(e, 0x2A) = U8(poly, 0x16);
		U8(e, 0x29) = U8(poly, 0x15);
		U8(e, 0x2C) = (uint8_t)(U8(poly, 0x14) * 3 / 5);
		U8(e, 0x2D) = (uint8_t)(U8(poly, 0x15) * 3 / 5);
		U8(e, 0x2B) = 0x24;
		U8(e, 0x2E) = (uint8_t)(U8(poly, 0x16) * 3 / 5);
		U8(e, 0x2F) = 0x2C;
	}

	// sub_8D9950: zone and crack delay of edge k (cell of the centroid of vertices a, b, n); the
	// three vertices get the zone's bit in their mask (a new bit also counts the vertex)
	static void TagEdge(int32_t id, int32_t k, int32_t a, int32_t b, int32_t n)
	{
		uint32_t e = Entry(id);
		int32_t xm = (S16(e, b * 8) + S16(e, n * 8) + S16(e, a * 8)) / 3;
		int32_t zm = (S16(e, n * 8 + 4) + S16(e, b * 8 + 4) + S16(e, a * 8 + 4)) / 3;
		uint16_t zone = ZoneLookup(ZONE_MAP, xm, zm);
		U8(e, k + 0x58) = (uint8_t)zone;
		U8(e, k + 0x5C) = (uint8_t)ZoneLookup(ZONE_MAP_B, xm, zm);
		uint32_t bit = 1u << ((uint8_t)zone & 31);
		int32_t va = S16(e, a * 2 + 0x34), vb = S16(e, b * 2 + 0x34), vn = S16(e, n * 2 + 0x34);
		uint32_t tbl = VertexZones();
		const int32_t vs[3] = { va, vb, vn };
		for (int r = 5; r; r--)
		{
			for (int32_t v : vs)
			{
				uint16_t m = U16(tbl, v * 4);
				if (!(uint16_t)(m & bit))
				{
					U16(tbl, v * 4) = (uint16_t)(m | bit);
					U16(tbl, v * 4 + 2) = (uint16_t)(U16(tbl, v * 4 + 2) + 1);
				}
			}
		}
	}

	// sub_8D9730: does prim j share the vertex pair (lo, hi) on an edge of another zone than `val`?
	// marks border bit `bit` on edge k of prim id and the matching side of j's edge
	static int32_t MatchEdge(int32_t id, int32_t j, uint16_t val, int16_t lo, int16_t hi, int32_t k, int32_t bit)
	{
		uint32_t ei = Entry(id), ej = Entry(j);
		uint8_t mask = (uint8_t)(1u << (bit & 31));
		int32_t found = 0;
		int32_t nedges;
		switch (U8(ej, 0x6E))
		{
		case 1: nedges = 3; break;
		case 2: nedges = 4; break;
		default: nedges = 0; break;
		}
		for (int m = 0; m < nedges; m++)
		{
			if (val == (uint16_t)U8(ej, m + 0x58)) continue;
			int32_t a, b, c;
			if (U8(ej, 0x6E) == 1)
			{
				c = 3;
				if (m == 1) { a = 1; b = 2; }
				else if (m == 2) { a = 2; b = 0; }
				else { a = 0; b = 1; }
			}
			else
			{
				c = 4;
				if (m == 1) { a = 1; b = 3; }
				else if (m == 2) { a = 3; b = 2; }
				else if (m == 3) { a = 2; b = 0; }
				else { a = 0; b = 1; }
			}
			int16_t va = S16(ej, a * 2 + 0x34), vb = S16(ej, b * 2 + 0x34), vc = S16(ej, c * 2 + 0x34);
			int16_t x = va, y = vb;
			if (x > y) { int16_t t = x; x = y; y = t; }
			if (lo == x && hi == y)
			{
				found = 1;
				U8(ei, k + 0x60) |= mask;
				U8(ej, m + 0x60) |= 1;
			}
			x = vb; y = vc;
			if (x > y) { int16_t t = x; x = y; y = t; }
			if (lo == x && hi == y)
			{
				found = 1;
				U8(ei, k + 0x60) |= mask;
				U8(ej, m + 0x60) |= 2;
			}
			x = vc; y = va;
			if (x > y) { int16_t t = x; x = y; y = t; }
			if (lo == x && hi == y)
			{
				found = 1;
				U8(ei, k + 0x60) |= mask;
				U8(ej, m + 0x60) |= 4;
			}
		}
		return found;
	}

	// sub_8D9640: side `bit` (vertices p, q) of edge k of prim id: when both vertices touch
	// several zones, find the prim sharing that side in another zone
	static void FindBorder(int32_t id, int32_t p, int32_t q, int32_t k, int32_t bit)
	{
		uint32_t e = Entry(id);
		uint32_t tbl = VertexZones();
		int16_t vp = S16(e, p * 2 + 0x34);
		if (S16(tbl, vp * 4 + 2) <= 1) return;
		int16_t vq = S16(e, q * 2 + 0x34);
		if (S16(tbl, vq * 4 + 2) <= 1) return;
		if (U8(e, k + 0x60) & (uint8_t)(1u << (bit & 31))) return;
		uint16_t val = U8(e, k + 0x58);
		int16_t lo = vp, hi = vq;
		if (lo > hi) { int16_t t = lo; lo = hi; hi = t; }
		uint8_t type = U8(e, 0x6E);
		if ((type == 1 && hi == 3) || (type == 2 && hi == 4))
		{
			MatchEdge(id, id, val, lo, hi, k, bit);
			return;
		}
		for (int32_t j = 0; j < PrimCount(); j++)
			if (MatchEdge(id, j, val, lo, hi, k, bit) == 1) return;
	}

	// sub_8D95E0: the prims whose id matches the state-1 tick (id % 16) look for their borders
	static void FindBorders(int32_t id, int32_t k, int32_t a, int32_t b, int32_t n)
	{
		if ((int16_t)id % 16 != WaveTick()) return;
		FindBorder(id, b, a, k, 0);
		FindBorder(id, n, b, k, 1);
		FindBorder(id, a, n, k, 2);
	}

	// ==================================================================
	// ground drawing
	// ==================================================================
	// FT4 culled when all four corners are off to the right and below (unsigned >= 0xA00, >= 0x780)
	static bool OffScreen4(uint32_t p)
	{
		return U16(p, 8) >= 0xA00 && U16(p, 0x10) >= 0xA00 && U16(p, 0x18) >= 0xA00 && U16(p, 0x20) >= 0xA00
			&& U16(p, 0xA) >= 0x780 && U16(p, 0x12) >= 0x780 && U16(p, 0x1A) >= 0x780 && U16(p, 0x22) >= 0x780;
	}

	// sub_8DA3E0: side wall under a raised edge (p1 - p2 at the zone height h, bottom at +16)
	static uint32_t RaisedWall(uint32_t e, uint32_t cur, uint32_t ot, const int16_t *p1, const int16_t *p2, int32_t i1, int32_t i2, int32_t h)
	{
		int16_t *w = (int16_t *)FieldAlloc(0x20);
		memcpy(w + 8, p1, 8);
		memcpy(w, p1, 8);
		memcpy(w + 12, p2, 8);
		memcpy(w + 4, p2, 8);
		w[9] = (int16_t)(w[9] + (int16_t)(0x10 - h));
		w[13] = (int16_t)(w[13] + (int16_t)(0x10 - h));
		GteLoadV012(w, w + 4, w + 8);
		GteRTPT();
		GteNCLIP();
		int32_t mac0;
		GteReadMAC0(&mac0);
		if (mac0 > 0)
		{
			GteStoreSXY012_FT4(cur);
			GteLoadV0(w + 12);
			GteRTPS();
			GteReadSXY2((void *)(cur + 0x20));
			if (!OffScreen4(cur))
			{
				GteAVSZ4();
				uint32_t otz;
				GteReadOTZ32(&otz);
				U8(cur, 7) = 0x2C;
				U8(cur, 0x1D) = U8(e, i1 + 0x69);
				U8(cur, 0xD) = U8(e, i1 + 0x69);
				U8(cur, 0x1C) = U8(e, i1 + 0x64);
				U8(cur, 0xC) = U8(e, i1 + 0x64);
				U32(cur, 0) = 0x9000000;
				U8(cur, 0x24) = U8(e, i2 + 0x64);
				U8(cur, 0x14) = U8(e, i2 + 0x64);
				U8(cur, 0x25) = U8(e, i2 + 0x69);
				U8(cur, 0x15) = U8(e, i2 + 0x69);
				U16(cur, 0xE) = U16(e, 0x32);
				U16(cur, 0x16) = U16(e, 0x30);
				U32(cur, 4) = U32(e, 0x2C);
				InsertPrimAutoDepth(OtSlot(ot, otz, 2), (void *)cur);
				cur += 0x28;
			}
		}
		FieldFree(0x20);
		return cur;
	}

	// sub_8D9F70: crack line along a border side (pA - pB), widened 0xC0 across the side, grows
	// once the state-3 tick passes the edge's crack delay; shade of the side's wall step (read by the caller)
	static uint32_t CrackLine(uint32_t e, uint32_t cur, const int16_t *pA, const int16_t *pB, int32_t k, int16_t tick, uint8_t step)
	{
		uint32_t ot = RenderOT44();
		if (tick <= (int16_t)U8(e, k + 0x5C)) return cur;
		int16_t v[4][4];
		memcpy(v[0], pA, 8);
		memcpy(v[1], pA, 8);
		memcpy(v[2], pB, 8);
		memcpy(v[3], pB, 8);
		Mat4x3 mtx = {};
		MatrixUnit(&mtx);
		int32_t ang = CartesianToGameAngle(pA[0] - pB[0], pA[2] - pB[2]) & 0xFFF;
		MatrixRotY(&mtx, (int16_t)ang);
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		int16_t s[4] = { -0xC0, 0, 0, 0 };
		GteSetLightMatrix(&mtx);
		GteSetBackgroundVector(mtx.t[0], mtx.t[1], mtx.t[2]);
		GteLoadV0(s);
		GteMVMVA_LightV0Bk();
		int16_t ir[4];
		GteStoreIR123(ir);
		v[0][0] = (int16_t)(v[0][0] + ir[0]);
		v[1][0] = (int16_t)(v[1][0] - ir[0]);
		v[2][0] = (int16_t)(v[2][0] + ir[0]);
		v[3][0] = (int16_t)(v[3][0] - ir[0]);
		v[0][2] = (int16_t)(v[0][2] + ir[2]);
		v[1][2] = (int16_t)(v[1][2] - ir[2]);
		v[2][2] = (int16_t)(v[2][2] + ir[2]);
		v[3][2] = (int16_t)(v[3][2] - ir[2]);
		GteLoadV012(v[0], v[1], v[2]);
		GteRTPT();
		GteNCLIP();
		int32_t mac0;
		GteReadMAC0(&mac0);
		if (mac0 <= 0) return cur;
		GteStoreSXY012_FT4(cur);
		GteLoadV0(v[3]);
		GteRTPS();
		GteReadSXY2((void *)(cur + 0x20));
		if (OffScreen4(cur)) return cur;
		GteAVSZ4();
		uint32_t otz;
		GteReadOTZ32(&otz);
		U32(cur, 0) = U32(CRACK_TEMPLATE, 0);
		U8(cur, 7) = U8(CRACK_TEMPLATE, 7);
		U32(cur, 0xC) = U32(CRACK_TEMPLATE, 0xC);
		U32(cur, 0x14) = U32(CRACK_TEMPLATE, 0x14);
		U32(cur, 0x1C) = U32(CRACK_TEMPLATE, 0x1C);
		U32(cur, 0x24) = U32(CRACK_TEMPLATE, 0x24);
		U16(cur, 0xE) = 0x3C54;
		uint8_t shade = U8(STEP_SHADE, step);
		U8(cur, 6) = shade;
		U8(cur, 5) = shade;
		U8(cur, 4) = shade;
		InsertPrimAutoDepth(OtSlot(ot, otz, 2), (void *)cur);
		return cur + 0x28;
	}

	// sub_8DB580: a chip thrown from the middle of a growing wall's top edge (1 in 4, while the
	// state-4 tick is below 12); the guard byte is never raised (the chip decrements the next one)
	static void SpawnChip(const int16_t *pA, const int16_t *pB, uint32_t guard)
	{
		uint32_t ctx = SeqCtx();
		if (U8(guard, 0) >= 1) return;
		if (RippleTick() >= 0xC) return;
		if (CrtRand() & 3) return;
		ChipCount() = (int16_t)(ChipCount() + 1);
		uint32_t n = AddLinkedTask(Q_SPRITES, ORIG_Chip, 0xAC, ctx);
		S16(n, 0x1C) = (int16_t)((pA[0] + pB[0]) / 2);
		int32_t r = CrtRand();
		S16(n, 0x1E) = (int16_t)((pA[1] + pB[1]) / 2 - (r & 0x1FF));
		S16(n, 0x20) = (int16_t)((pA[2] + pB[2]) / 2);
		U32(n, 0xA8) = guard + 1;
	}

	// the drawing part of sub_8DA200: wall under a crack side at step `step` (depth offset of the
	// step, shade of the next step); returns the cursor, *drawn when inserted
	static uint32_t CrackWallDraw(uint32_t e, uint32_t cur, const int16_t *pA, const int16_t *pB, uint8_t step, int16_t *w, bool *drawn)
	{
		uint32_t ot = RenderOT44();
		*drawn = false;
		// w: pA lowered, pB lowered, pA, pB
		memcpy(w + 8, pA, 8);
		memcpy(w + 12, pB, 8);
		memcpy(w, pA, 8);
		memcpy(w + 4, pB, 8);
		int16_t d = S16(WALL_DEPTH, step * 2);
		w[1] = (int16_t)(w[1] + d);
		w[5] = (int16_t)(w[5] + d);
		GteLoadV012(w, w + 4, w + 8);
		GteRTPT();
		GteNCLIP();
		int32_t mac0;
		GteReadMAC0(&mac0);
		if (mac0 <= 0) return cur;
		GteStoreSXY012_FT4(cur);
		GteLoadV0(w + 12);
		GteRTPS();
		GteReadSXY2((void *)(cur + 0x20));
		if (OffScreen4(cur)) return cur;
		GteAVSZ4();
		uint32_t otz;
		GteReadOTZ32(&otz);
		U32(cur, 0) = U32(WALL_TEMPLATE, 0);
		U32(cur, 0xC) = U32(WALL_TEMPLATE, 0xC);
		U8(cur, 7) = U8(WALL_TEMPLATE, 7);
		U32(cur, 0x1C) = U32(WALL_TEMPLATE, 0x1C);
		U32(cur, 0x14) = U32(WALL_TEMPLATE, 0x14);
		U32(cur, 0x24) = U32(WALL_TEMPLATE, 0x24);
		uint8_t shade = U8(STEP_SHADE, (uint8_t)(step + 1));
		U8(cur, 6) = shade;
		U8(cur, 5) = shade;
		U8(cur, 4) = shade;
		InsertPrimAutoDepth(OtSlot(ot, otz, 2), (void *)cur);
		*drawn = true;
		return cur + 0x28;
	}

	// sub_8DA200: crack wall of side m of edge k: one step deeper every tick (up to 13 steps)
	// while the state-4 tick runs; a drawn wall may throw a chip
	static uint32_t CrackWall(uint32_t e, uint32_t cur, const int16_t *pA, const int16_t *pB, int32_t k, int32_t m)
	{
		if (RippleTick() == 0) return cur;
		uint32_t slot = e + 3 * k + m;
		uint8_t step = U8(slot, 0x40);
		if (step > 0xC) return cur;
		U8(slot, 0x40) = (uint8_t)(step + 1);
		// 30 fps layer: see mag038_quake_held.inc
		FX_HELD(held_note_wall(m, step);)
		int16_t w[16];
		bool drawn;
		cur = CrackWallDraw(e, cur, pA, pB, step, w, &drawn);
		if (drawn && ChipCount() < 0x5A) SpawnChip(w, w + 8, slot + 0x4C);
		return cur;
	}

	// the drawing part of sub_8D9BE0 once the zones are raised (state >= 5): the sub-triangle,
	// then the side walls along its border sides
	static uint32_t RaisedEdge(uint32_t e, int32_t k, int32_t a, int32_t b, int32_t n, const int16_t *v, int32_t h, uint32_t ot, uint32_t cur)
	{
		GteLoadV012(v, v + 4, v + 8);
		GteRTPT();
		GteNCLIP();
		int32_t mac0;
		GteReadMAC0(&mac0);
		if (mac0 > 0)
		{
			GteStoreSXY012_FT3(cur);
			bool off = U16(cur, 8) >= 0xA00 && U16(cur, 0x10) >= 0xA00 && U16(cur, 0x18) >= 0xA00
				&& U16(cur, 0xA) >= 0x780 && U16(cur, 0x12) >= 0x780 && U16(cur, 0x1A) >= 0x780;
			if (!off)
			{
				GteAVSZ3();
				uint32_t otz;
				GteReadOTZ32(&otz);
				U8(cur, 7) = 0x24;
				U8(cur, 0xC) = U8(e, a + 0x64);
				U8(cur, 0xD) = U8(e, a + 0x69);
				U32(cur, 0) = 0x7000000;
				U8(cur, 0x14) = U8(e, b + 0x64);
				U8(cur, 0x15) = U8(e, b + 0x69);
				U8(cur, 0x1D) = U8(e, n + 0x69);
				U16(cur, 0x16) = U16(e, 0x30);
				U8(cur, 0x1C) = U8(e, n + 0x64);
				U16(cur, 0xE) = U16(e, 0x32);
				U32(cur, 4) = U32(e, 0x28);
				InsertPrimAutoDepth(OtSlot(ot, otz, 2), (void *)cur);
				cur += 0x20;
			}
		}
		uint8_t flags = U8(e, k + 0x60);
		if (flags & 1) cur = RaisedWall(e, cur, ot, v + 4, v, b, a, h);
		if (U8(e, k + 0x60) & 2) cur = RaisedWall(e, cur, ot, v + 8, v + 4, n, b, h);
		if (U8(e, k + 0x60) & 4) cur = RaisedWall(e, cur, ot, v, v + 8, a, n, h);
		return cur;
	}

	// the crack part of sub_8D9BE0 (states 3 and 4): crack lines along the border sides, and
	// in state 4 the growing walls
	static uint32_t CrackEdge(uint32_t e, int32_t k, const int16_t *v, int16_t tick, uint32_t cur, bool walls)
	{
		if (U8(e, k + 0x60) & 1) cur = CrackLine(e, cur, v + 4, v, k, tick, U8(e, 3 * k + 0x40));
		if (U8(e, k + 0x60) & 2) cur = CrackLine(e, cur, v + 8, v + 4, k, tick, U8(e, 3 * k + 1 + 0x40));
		if (U8(e, k + 0x60) & 4) cur = CrackLine(e, cur, v, v + 8, k, tick, U8(e, 3 * k + 2 + 0x40));
		if (!walls) return cur;
		if (U8(e, k + 0x60) & 1) cur = CrackWall(e, cur, v + 4, v, k, 0);
		if (U8(e, k + 0x60) & 2) cur = CrackWall(e, cur, v + 8, v + 4, k, 1);
		if (U8(e, k + 0x60) & 4) cur = CrackWall(e, cur, v, v + 8, k, 2);
		return cur;
	}

	// sub_8D9BE0: edge k (vertex slots a, b + centroid slot n) of prim id
	static uint32_t DrawEdge(int32_t id, int32_t k, uint32_t cur, int32_t a, int32_t b, int32_t n)
	{
		uint32_t e = Entry(id);
		uint32_t ot = RenderOT44();
		int32_t h = 0;
		int16_t *v = (int16_t *)FieldAlloc(0x18);
		int16_t st = State();
		if (st > 2)
		{
			memcpy(v, (const void *)(e + a * 8), 8);
			memcpy(v + 4, (const void *)(e + b * 8), 8);
			memcpy(v + 8, (const void *)(e + n * 8), 8);
			h = ZoneHeight(U8(e, k + 0x58));
			v[1] = (int16_t)(v[1] + h);
			v[5] = (int16_t)(v[5] + h);
			v[9] = (int16_t)(v[9] + h);
		}
		// 30 fps layer: see mag038_quake_held.inc
		FX_HELD(held_note_edge(id, k, a, b, n, st, (int16_t)h);)
		if (st > 4) cur = RaisedEdge(e, k, a, b, n, v, (int16_t)h, ot, cur);
		if (st == 3 || st == 4) cur = CrackEdge(e, k, v, CrackTick(), cur, st == 4);
		FieldFree(0x18);
		return cur;
	}

	// sub_8DB210: a random point of the ground (inside a random prim's first sub-triangle, then
	// moved up to +-0x400 in x / z) within the zone map; 8 tries
	static int32_t RandomGroundPoint(int16_t *out)
	{
		for (int t = 0; t < 8; t++)
		{
			int32_t r = CrtRand();
			int16_t idx = (int16_t)(r % PrimCount());
			uint32_t e = Entry(idx);
			int32_t n;
			switch (U8(e, 0x6E))
			{
			case 1: n = 3; break;
			case 2: n = 4; break;
			default: continue;
			}
			int16_t x = (int16_t)((S16(e, 8) + S16(e, 0) + S16(e, n * 8)) / 3);
			out[0] = x;
			out[1] = (int16_t)((S16(e, n * 8 + 2) + S16(e, 0xA) + S16(e, 2)) / 3);
			int16_t z = (int16_t)((S16(e, n * 8 + 4) + S16(e, 0xC) + S16(e, 4)) / 3);
			out[2] = z;
			if (x < BoundXLo() || x > BoundXHi() || z < BoundZLo() || z > BoundZHi()) continue;
			out[0] = (int16_t)(out[0] + (int16_t)((CrtRand() & 0x7FF) - 0x400));
			out[2] = (int16_t)(out[2] + (int16_t)((CrtRand() & 0x7FF) - 0x400));
			return 1;
		}
		return 0;
	}

	// sub_8DAE10: debris vertex j from entry vertex vi: top at a random 1/8..1 of the way from the
	// vertex to the debris centre, bottom a random fraction of that, below
	static void DebrisVertex(uint32_t n, uint32_t e, int32_t j, int32_t vi)
	{
		int32_t r = CrtRand() % 0xE00;
		S16(n, j * 8 + 0x5C) = (int16_t)(mul32(r + 0x200, S16(n, 0x1C) - S16(e, vi * 8)) / 4096);
		r = CrtRand() % 0xE00;
		S16(n, j * 8 + 0x60) = (int16_t)(mul32(r + 0x200, S16(n, 0x20) - S16(e, vi * 8 + 4)) / 4096);
		S16(n, j * 8 + 0x5E) = (int16_t)(S16(n, 0x1E) - S16(e, vi * 8 + 2));
		r = CrtRand() % 0xE00;
		S16(n, j * 8 + 0x74) = (int16_t)(mul32(r + 0x200, S16(n, j * 8 + 0x5C)) / 4096);
		r = CrtRand() % 0xE00;
		S16(n, j * 8 + 0x78) = (int16_t)(mul32(r + 0x200, S16(n, j * 8 + 0x60)) / 4096);
		r = CrtRand() % 0x300;
		uint8_t u = U8(e, vi + 0x64), v = U8(e, vi + 0x69);
		S16(n, j * 8 + 0x76) = (int16_t)((int16_t)(r + S16(n, j * 8 + 0x5E)) + 0x80);
		U8(n, j + 0x97) = u;
		U8(n, j + 0x94) = u;
		U8(n, j + 0x9D) = v;
		U8(n, j + 0x9A) = v;
	}

	// sub_8DA750: a debris piece of a random prim (its first sub-triangle) inside the zone map;
	// `junk` = the uninitialised high half of the original's z word (copied to +0x22)
	static void SpawnDebris(uint16_t junk, StackJunk *sj)
	{
		uint32_t ctx = SeqCtx();
		int32_t r = CrtRand();
		int16_t idx = (int16_t)(r % PrimCount());
		uint32_t e = Entry(idx);
		int32_t n;
		switch (U8(e, 0x6E))
		{
		case 1: n = 3; break;
		case 2: n = 4; break;
		default: return;
		}
		int16_t x = (int16_t)((S16(e, 8) + S16(e, 0) + S16(e, n * 8)) / 3);
		int16_t y = (int16_t)((S16(e, 0xA) + S16(e, n * 8 + 2) + S16(e, 2)) / 3);
		int16_t z = (int16_t)((S16(e, 0xC) + S16(e, n * 8 + 4) + S16(e, 4)) / 3);
		sj->wA = (uint16_t)y;
		if (x <= BoundXLo() || x >= BoundXHi() || z <= BoundZLo() || z >= BoundZHi()) return;
		DebrisCount() = (int16_t)(DebrisCount() + 1);
		uint32_t d = AddLinkedTask(Q_DEBRIS, ORIG_Debris, 0xB0, ctx);
		S16(d, 0x1C) = x;
		S16(d, 0x1E) = y;
		S16(d, 0x20) = z;
		U16(d, 0x22) = junk;
		DebrisVertex(d, e, 0, 0);
		DebrisVertex(d, e, 1, 1);
		DebrisVertex(d, e, 2, n);
		U16(d, 0xA2) = U16(e, 0x32);
		U32(d, 0xA8) = U32(e, 0x2C);
		U32(d, 0xAC) = U32(e, 0x2C);
		U32(d, 0xA4) = U32(e, 0x28);
		U16(d, 0xA0) = U16(e, 0x30);
		U8(d, 0xAB) = U8(e, 0x2B);
		uint16_t zone = ZoneLookup(ZONE_MAP, S16(d, 0x1C), S16(d, 0x20));
		S16(d, 0x1E) = ZoneHeight((int16_t)zone);
	}

	// sub_8DAF40: a rock burst: `count` rock sprites on random ground points around the first one
	static void SpawnRocks(int32_t count, uint16_t junk)
	{
		int16_t p[3];
		uint32_t ctx = SeqCtx();
		if (RandomGroundPoint(p) != 1) return;
		RockCount() = (int16_t)(RockCount() + 1);
		uint32_t n = AddLinkedTask(Q_SPRITES, ORIG_Rock, 0xAC, ctx);
		S16(n, 0x58) = p[0];
		S16(n, 0x5A) = p[1];
		S16(n, 0x5C) = p[2];
		U16(n, 0x5E) = junk;
		uint16_t zone = ZoneLookup(ZONE_MAP, p[0], p[2]);
		S16(n, 0x60) = 1;
		S16(n, 0x5A) = ZoneHeight((int16_t)zone);
		if (count <= 1) return;
		uint32_t rec = n + 0x64;
		for (int i = count - 1; i; i--, rec += 0xC)
		{
			if (RandomGroundPoint(p) != 1) continue;
			S16(rec, 0) = p[0];
			S16(rec, 2) = p[1];
			S16(rec, 4) = p[2];
			U16(rec, 6) = junk;
			zone = ZoneLookup(ZONE_MAP, p[0], p[2]);
			S16(rec, 2) = ZoneHeight((int16_t)zone);
			S16(rec, 2) = (int16_t)(S16(rec, 2) + (int16_t)((CrtRand() & 0x3FF) - 0x200));
			S16(rec, 8) = 1;
		}
	}

	// sub_8DB330: a dust burst: 6 dust sprites on random ground points
	static void SpawnDust(uint16_t junk)
	{
		int16_t p[3];
		uint32_t ctx = SeqCtx();
		if (RandomGroundPoint(p) != 1) return;
		DustCount() = (int16_t)(DustCount() + 1);
		uint32_t n = AddLinkedTask(Q_SPRITES, ORIG_Dust, 0xAC, ctx);
		S16(n, 0x58) = p[0];
		S16(n, 0x5A) = p[1];
		S16(n, 0x5C) = p[2];
		U16(n, 0x5E) = junk;
		uint16_t zone = ZoneLookup(ZONE_MAP, p[0], p[2]);
		S16(n, 0x60) = 1;
		S16(n, 0x5A) = ZoneHeight((int16_t)zone);
		uint32_t rec = n + 0x64;
		for (int i = 5; i; i--, rec += 0xC)
		{
			if (RandomGroundPoint(p) != 1) continue;
			S16(rec, 0) = p[0];
			S16(rec, 2) = p[1];
			S16(rec, 4) = p[2];
			U16(rec, 6) = junk;
			zone = ZoneLookup(ZONE_MAP, p[0], p[2]);
			S16(rec, 2) = ZoneHeight((int16_t)zone);
			S16(rec, 2) = (int16_t)(S16(rec, 2) - (int16_t)(CrtRand() & 0x3FF));
			S16(rec, 8) = 1;
		}
	}

	// sub_8D8B10: the stage ground group through the quake mesh, then one step of the mesh state
	// machine. State 0 transforms the ground vertices (each vertex group by its bone matrix, as
	// the GTE light matrix) into the vertex workspace and builds the mesh entries; state 1 finds
	// the zone borders; states >= 3 draw.
	static uint32_t Ground(uint32_t model, uint32_t hdr, uint32_t cur)
	{
		uint32_t vtx = U32(hdr, 4);
		int32_t nverts_total = 0;
		uint32_t bones = U32(model, 0) + 0x10;
		uint32_t objects = U32(model, 4);
		int32_t nobjects = S32(objects, 0);
		int16_t *tmp = (int16_t *)FieldAlloc(0xC);
		uint32_t offsets = objects + 4;
		S32((uint32_t)tmp, 8) = -1;
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		// 30 fps layer: see mag038_quake_held.inc
		FX_HELD(held_note_ground();)
		int32_t id = 0;
		int32_t nverts = 0;
		for (int32_t i = 0; i < nobjects; i++)
		{
			uint32_t out = vtx;
			uint32_t obj = U32(model, 4) + U32(offsets, 0);
			offsets += 4;
			if (!((uint32_t)S32((uint32_t)tmp, 8) >> (i & 31) & 1)) continue;
			int32_t ngroups = S16(obj, 0);
			obj += 2;
			nverts = 0;
			for (; ngroups > 0; ngroups--)
			{
				if (State() != 0)
				{
					nverts = S16(obj, 2);
					nverts_total += nverts;
					obj += 2 + nverts * 6 + 2;
					continue;
				}
				uint32_t bone = bones + S16(obj, 0) * 0x30 + 0x10;
				obj += 2;
				GteSetLightMatrix((const void *)bone);
				GteSetBackgroundVector(S32(bone, 0x14), S32(bone, 0x18), S32(bone, 0x1C));
				nverts = S16(obj, 0);
				obj += 2;
				if (nverts <= 0) continue;
				nverts_total += nverts;
				for (int32_t c = nverts; c; c--)
				{
					tmp[0] = S16(obj, 0);
					tmp[1] = S16(obj, 2);
					tmp[2] = S16(obj, 4);
					obj += 6;
					GteLoadV0(tmp);
					GteMVMVA_LightV0Bk();
					GteStoreIR123(tmp);
					S16(out, 0) = tmp[0];
					S16(out, 2) = tmp[1];
					S16(out, 4) = tmp[2];
					out += 8;
				}
			}
			VertexBase() = (int16_t)nverts;
			uint32_t lists = ((obj + 3) & ~3u) + 4;
			int32_t ntri = S16(lists, 0), nquad = S16(lists, 2);
			uint32_t poly = lists + 8;
			for (; ntri > 0; ntri--, poly += 0x14)
			{
				int16_t st = State();
				id++;
				if (st == 0)
				{
					BuildTriangle(id, vtx, poly);
					TagEdge(id, 0, 0, 1, 3);
					TagEdge(id, 1, 1, 2, 3);
					TagEdge(id, 2, 2, 0, 3);
				}
				else if (st == 1)
				{
					FindBorders(id, 0, 0, 1, 3);
					FindBorders(id, 1, 1, 2, 3);
					FindBorders(id, 2, 2, 0, 3);
				}
				cur = DrawEdge(id, 0, cur, 0, 1, 3);
				cur = DrawEdge(id, 1, cur, 1, 2, 3);
				cur = DrawEdge(id, 2, cur, 2, 0, 3);
			}
			for (; nquad > 0; nquad--, poly += 0x18)
			{
				int16_t st = State();
				id++;
				if (st == 0)
				{
					BuildQuad(id, vtx, poly);
					TagEdge(id, 0, 0, 1, 4);
					TagEdge(id, 1, 1, 3, 4);
					TagEdge(id, 2, 3, 2, 4);
					TagEdge(id, 3, 2, 0, 4);
				}
				else if (st == 1)
				{
					FindBorders(id, 0, 0, 1, 4);
					FindBorders(id, 1, 1, 3, 4);
					FindBorders(id, 2, 3, 2, 4);
					FindBorders(id, 3, 2, 0, 4);
				}
				cur = DrawEdge(id, 0, cur, 0, 1, 4);
				cur = DrawEdge(id, 1, cur, 1, 3, 4);
				cur = DrawEdge(id, 2, cur, 3, 2, 4);
				cur = DrawEdge(id, 3, cur, 2, 0, 4);
			}
		}
		FieldFree(0xC);
		// words the original reads from its stack without writing them: the spawners' vector
		// locals sit where 0x8DA680 saved ebx / ebp (the 0xC-byte scratch and vertex workspace
		// pointers) and where the debris spawner left its y; after the rock spawner the return
		// address into this function (0x008D902F)
		StackJunk sj;
		sj.w6 = (uint16_t)((uint32_t)tmp >> 16);
		sj.wA = (uint16_t)(vtx >> 16);
		(void)nverts_total;
		switch (State())
		{
		case 0:
			PrimCount() = (int16_t)id;
			WaveTick() = 0;
			State() = 1;
			break;
		case 1:
			WaveTick() = (int16_t)(WaveTick() + 1);
			if (WaveTick() > 0x10) State() = 2;
			break;
		case 2:
			if (QuakeGo() == 1) State() = 3;
			break;
		case 3:
			CrackTick() = (int16_t)(CrackTick() + 1);
			if (CrackTick() > 0xD) State() = 4;
			break;
		case 4:
			RippleTick() = (int16_t)(RippleTick() + 1);
			if (RippleTick() > 0xC)
			{
				ZonesInit();
				State() = 5;
			}
			break;
		case 5:
		{
			U8(0x1D989BD, 0) &= 0xFD;
			ZonesUpdate();
			int16_t t = (int16_t)(FallTick() + 1);
			FallTick() = t;
			if (t > 0x28) State() = 6;
			if (DebrisCount() < 0x1E)
			{
				for (int c = 0; c < 6; c++) SpawnDebris(sj.w6, &sj);
				t = FallTick();
			}
			bool rocks = false;
			if (RockCount() < 0xA)
			{
				SpawnRocks(t < 0x22 ? 6 : (t < 0x25 ? 5 : 4), sj.wA);
				rocks = true;
			}
			if (DustCount() < 0xD) SpawnDust(rocks ? 0x008D : sj.w6);
			break;
		}
		case 6:
			if (ZonesSettle() == 1)
			{
				State() = 7;
				U8(0x1D989BD, 0) |= 2;
			}
			break;
		}
		return cur;
	}

	// sub_8D8A40: the ground render: GTE = camera (ComposeAffineTransform with a unit matrix),
	// ground animation re-read, then the ground through the quake mesh
	static void GroundRender()
	{
		Mat4x3 unit = {}, cam = {};
		MatrixUnit(&unit);
		unit.t[0] = 0;
		unit.t[1] = 0;
		unit.t[2] = 0;
		ComposeAffineTransform(&Camera(), &unit, &cam);
		GteSetRotMatrix(&cam);
		GteSetTransVector(&cam);
		uint32_t hdr = (uint32_t)FieldAlloc(0x2C);
		U16(hdr, 0x24) = U16(0x1D989BE, 0);
		S16(hdr, 0x14) = 0;
		S16(hdr, 0x16) = 0;
		S16(hdr, 0x18) = 0x140;
		S16(hdr, 0x1A) = 0xD8;
		U32(hdr, 0x1C) = U32(0x1D989E4, 0);
		U32(hdr, 4) = var<uint32_t>(0x1D98B3C);
		S32(hdr, 0x20) = -1;
		U32(hdr, 0x10) = var<uint32_t>(0x1D969A8);
		ReadAnimation(0x1D989D0, 0x1D989DC, 0);
		PacketCursor() = Ground(STAGE_GROUND, hdr, PacketCursor());
		FieldFree(0x2C);
	}

	// ==================================================================
	// root and emitter
	// ==================================================================
	// sub_8D86F0 (root phase 1): the emitter of the current action
	static void Root_SpawnEmitter(uint32_t n)
	{
		U8(n, 0x63) = 1;
		AddLinkedTask(Q_EMITTER, ORIG_Emitter, 0x58, n);
		NextPhase(n);
	}

	// sub_8DC4D0 (root phase 2): next action once the emitter is done
	static void Root_NextAction(uint32_t n)
	{
		if (U8(n, 0x63)) return;
		uint8_t a = U8(n, 0x2A);
		if ((int16_t)(int8_t)a < S16(n, 0x58))
		{
			U8(n, 0x2A) = (uint8_t)(a + 1);
			U8(n, 0x29) = (uint8_t)(U8(n, 0x29) - 1);
		}
		else NextPhase(n);
	}

	// MAG_038_QUAKE_Tick (0x8D8610): root task
	static uint32_t __cdecl RootTask(TaskNode *node)
	{
		uint32_t n = (uint32_t)node;
		// 30 fps layer: see mag038_quake_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (U8(n, 0x5C) & 1) ? Arena0() : Arena1();
		UpdateTargetPosFromBones(n);
		switch (PhaseOf(n))
		{
		case 0: NextPhase(n); break;                 // sub_8D86E0
		case 1: Root_SpawnEmitter(n); break;
		case 2: Root_NextAction(n); break;
		case 3:                                      // sub_8DC500: done when the queues are empty
			if (S16(n, 0x5E) == 0)
			{
				U8(n, 0x26) |= 1;
				NextPhase(n);
			}
			break;
		default: break;                              // nullsub
		}
		U16(n, 0x5E) = (uint16_t)ExecuteTaskQueue((TaskQueue *)Q_EMITTER);
		U16(n, 0x5E) = (uint16_t)(U16(n, 0x5E) + ExecuteTaskQueue((TaskQueue *)Q_SEQUENCE));
		U16(n, 0x5E) = (uint16_t)(U16(n, 0x5E) + ExecuteTaskQueue((TaskQueue *)Q_SPRITES));
		U16(n, 0x5E) = (uint16_t)(U16(n, 0x5E) + ExecuteTaskQueue((TaskQueue *)Q_DEBRIS));
		U16(n, 0x5C) = (uint16_t)(U16(n, 0x5C) + 1);
		return EndCheck(n);
	}

	// sub_8D8790 (emitter phase 0): ground sequence + camera shake when the stage ground group is
	// shown and has objects, else the fallback
	static void Emitter_Start(uint32_t n)
	{
		if ((U8(0x1D989BD, 0) & 1) && U32(U32(0x1D989C4, 0), 0) != 0)
		{
			AddLinkedTask(Q_SEQUENCE, ORIG_Sequence, 0x34, n);
			AddLinkedTask(Q_SEQUENCE, ORIG_Shake, 0x34, n);
		}
		else AddLinkedTask(Q_SEQUENCE, ORIG_Fallback, 0x34, n);
		NextPhase(n);
	}

	// sub_8DC450 (emitter phase 1): damage to every target of the action once the children ended
	static void Emitter_Damage(uint32_t n)
	{
		if (U8(n, 0x28)) return;
		ActionData *ad = &((CastContext *)U32(n, 0xC))->actions[S8(n, 0x2A)];
		if (ad->target_count)
		{
			int off = 0;
			for (int t = 0; t < ((CastContext *)U32(n, 0xC))->actions[S8(n, 0x2A)].target_count; t++, off += TARGET_STRIDE)
				ApplyActionResultToTarget(((CastContext *)U32(n, 0xC))->actions[S8(n, 0x2A)].targets + off);
		}
		U8(n, 0x26) |= 1;
		U8(U32(n, 0x10), 0x63) = 0;
		NextPhase(n);
	}

	// sub_8D8720: emitter task
	static uint32_t __cdecl EmitterTask(TaskNode *node)
	{
		uint32_t n = (uint32_t)node;
		EmitterUpdatePos(n);
		EmitterComputeBounds(n);
		switch (PhaseOf(n))
		{
		case 0: Emitter_Start(n); break;
		case 1: Emitter_Damage(n); break;
		default: break;
		}
		U16(n, 0x24) = (uint16_t)(U16(n, 0x24) + 1);
		if ((U8(n, 0x26) & 1) && U8(n, 0x28) == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ==================================================================
	// ground sequence (0x8D8800)
	// ==================================================================
	// MAG_038_QUAKE_InitGroundMeshBuffers (0x8D88C0)
	static void InitGround(uint32_t n)
	{
		SeqCtx() = n;
		uint8_t first = U8(n, 0x2D);
		uint32_t entries = Entries();
		BoundXLo() = -0x2000;
		BoundZLo() = -0x2000;
		TargetsParty() = first < 3 ? 1 : 0;
		HalfX() = 0x2000;
		HalfZ() = 0x2000;
		BoundXHi() = 0x2000;
		BoundZHi() = 0x2000;
		memset((void *)entries, 0, 0x2D80 * 4);
		memset((void *)VertexZones(), 0, 0x330 * 4);
		U8(0x27929C4, 0) = 0x1F;
		U8(0x27929B4, 0) = 0x1F;
		U8(0x27929C5, 0) = 0x7F;
		U8(0x27929BD, 0) = 0x7F;
		U8(0x2792E5C, 0) = 0x7F;
		U8(0x2792E4C, 0) = 0x7F;
		ZoneOffsetZ() = 0;
		State() = 0;
		QuakeGo() = 0;
		CrackTick() = 0;
		RippleTick() = 0;
		FallTick() = 0;
		DebrisCount() = 0;
		RockCount() = 0;
		DustCount() = 0;
		ChipCount() = 0;
		U32(0x27929A0, 0) = 0x9000000;
		U8(0x27929BC, 0) = 0;
		U8(0x27929AC, 0) = 0;
		U8(0x27929B5, 0) = 0;
		U8(0x27929AD, 0) = 0;
		U8(0x27929A7, 0) = 0x2E;
		U16(0x27929B6, 0) = 0xB9;
		U32(0x2792E38, 0) = 0x9000000;
		U8(0x2792E54, 0) = 0;
		U8(0x2792E44, 0) = 0;
		U8(0x2792E4D, 0) = 0;
		U8(0x2792E45, 0) = 0;
		U8(0x2792E5D, 0) = 0xFF;
		U8(0x2792E55, 0) = 0xFF;
		U8(0x2792E3F, 0) = 0x2E;
		U16(0x2792E4E, 0) = 0xBA;
		U16(0x2792E46, 0) = 0x3C14;
	}

	// the side opposite the targets (the caster's): slots 3..6 when the first target is a party
	// member, else 0..2; `fn` gets (entity, saved flags cell)
	template<typename F> static void ForCasterSide(bool enemies, F fn)
	{
		if (enemies)
			for (int s = 3; s < 7; s++) fn(EntityAt(s), SAVED_FLAGS + 4 * s);
		else
			for (int s = 0; s < 3; s++) fn(EntityAt(s), SAVED_FLAGS + 4 * s);
	}

	// darkening of an entity's two colour words toward black by level / 4096
	static void Darken(uint32_t ent, int32_t level, uint32_t light)
	{
		uint8_t l0 = (uint8_t)light, l1 = (uint8_t)(light >> 8);
		U8(ent, 0x28) = (uint8_t)(l0 - (uint8_t)(mul32(l0, level) / 4096));
		U8(ent, 0x29) = (uint8_t)(l1 - (uint8_t)(mul32(l1, level) / 4096));
		uint8_t l2 = U8(0xB8B7DA, 0);
		U8(ent, 0x2A) = (uint8_t)(l2 - (uint8_t)(mul32(l2, level) / 4096));
		uint8_t m0 = U8(0xB8B9A8, 0);
		U8(ent, 0x2C) = (uint8_t)(m0 - (uint8_t)(mul32(m0, level) / 4096));
		uint8_t m1 = U8(0xB8B9A9, 0);
		U8(ent, 0x2D) = (uint8_t)(m1 - (uint8_t)(mul32(m1, level) / 4096));
		uint8_t m2 = U8(0xB8B9AA, 0);
		U8(ent, 0x2E) = (uint8_t)(m2 - (uint8_t)(mul32(m2, level) / 4096));
	}

	static void DarkenCasterSide(uint32_t n)
	{
		uint32_t light = var<uint32_t>(0xB8B7D8);
		ForCasterSide(TargetsParty() != 0, [&](uint32_t ent, uint32_t saved) {
			if (!(U8(saved, 0) & 4)) Darken(ent, S16(n, 0x1C), light);
		});
	}

	// sub_8DB7B0 (phase 3): save the entity flags; the caster's side drawn with the stage light
	// (colour mode 2)
	static void Seq_Darken(uint32_t n)
	{
		for (int s = 0; s < 7; s++) U32(SAVED_FLAGS, 4 * s) = U16(EntityAt(s), 0);
		S16(n, 0x1C) = 0;
		uint32_t light = var<uint32_t>(0xB8B7D8);
		ForCasterSide(TargetsParty() != 0, [&](uint32_t ent, uint32_t saved) {
			if (U8(saved, 0) & 4) return;
			U8(ent, 1) |= 8;
			U32(ent, 0x28) = light;
			U8(ent, 0x2B) = 2;
		});
		GroundRender();
		NextPhase(n);
	}

	// sub_8DB870 (phase 4): fade the caster's side to black; at full black the quake starts
	static void Seq_FadeOut(uint32_t n)
	{
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + 0x200);
		if (S16(n, 0x1C) >= 0x1000)
		{
			S16(n, 0x1C) = 0x1000;
			QuakeGo() = 1;
			BdPlaySE(SOUND_Rumble, 0, 0x80);
			NextPhase(n);
		}
		DarkenCasterSide(n);
		GroundRender();
	}

	// sub_8DBAC0 (phase 5): the caster's side hidden (bit 2), the targets' side shadows off (0x1020)
	static void Seq_Hide(uint32_t n)
	{
		if (TargetsParty())
		{
			for (int s = 3; s < 7; s++) if (!(U8(SAVED_FLAGS, 4 * s) & 4)) U8(EntityAt(s), 0) |= 4;
			for (int s = 0; s < 3; s++) if (!(U8(SAVED_FLAGS, 4 * s) & 4)) U16(EntityAt(s), 0) |= 0x1020;
		}
		else
		{
			for (int s = 0; s < 3; s++) if (!(U8(SAVED_FLAGS, 4 * s) & 4)) U8(EntityAt(s), 0) |= 4;
			for (int s = 3; s < 7; s++) if (!(U8(SAVED_FLAGS, 4 * s) & 4)) U16(EntityAt(s), 0) |= 0x1020;
		}
		NextPhase(n);
		GroundRender();
	}

	// sub_8DBBA0: save every entity's position words
	static void SavePositions()
	{
		for (int s = 0; s < 7; s++)
		{
			uint32_t rec = SAVED_POS + 0x14 * s, ent = EntityAt(s);
			U32(rec, 0) = U32(ent, 0xC);
			U32(rec, 4) = U32(ent, 0x10);
			U32(rec, 8) = U32(ent, 0x1C);
			U32(rec, 0xC) = U32(ent, 0x20);
			U16(rec, 0x10) = 0;
			U16(rec, 0x12) = 0;
		}
	}

	// sub_8DBB70 (phase 6): once the ripples start: positions saved, second sound
	static void Seq_WaitRipples(uint32_t n)
	{
		if (State() == 5)
		{
			SavePositions();
			BdPlaySE(SOUND_Quake, 1, 0x80);
			U8(n, 0x29) = (uint8_t)(U8(n, 0x29) + 1);
		}
		GroundRender();
	}

	// sub_8DBCA0: an entity on the ripples: state 0 follows its zone's height (jittered positions
	// while lifted, else thrown up at -0x30); state 1 falls back (+0x30 per tick) and may flinch
	static void Bounce(uint32_t n, uint32_t ent, int slot)
	{
		uint32_t rec = SAVED_POS + 0x14 * slot;
		uint16_t zone = ZoneLookup(ZONE_MAP, S16(rec, 8), S16(rec, 0xC));
		int16_t h = ZoneHeight((int16_t)zone);
		switch (S16(rec, 0x10))
		{
		case 0:
			if (S16(ent, 0x1E) < h)
			{
				S16(rec, 0x12) = -0x30;
				U32(ent, 0xC) = U32(rec, 0);
				S16(rec, 0x10) = (int16_t)(S16(rec, 0x10) + 1);
				U32(ent, 0x10) = U32(rec, 4);
				S16(ent, 0x1C) = S16(rec, 8);
				S16(ent, 0x20) = S16(rec, 0xC);
				return;
			}
			S16(ent, 0x1E) = h;
			S16(ent, 0xC) = (int16_t)((int16_t)((CrtRand() & 0x7F) + S16(rec, 0)) - 0x40);
			S16(ent, 0xE) = (int16_t)((int16_t)((CrtRand() & 0x7F) + S16(rec, 2)) - 0x40);
			S16(ent, 0x10) = (int16_t)((int16_t)((CrtRand() & 0x7F) + S16(rec, 4)) - 0x40);
			S16(ent, 0x1C) = (int16_t)((int16_t)((CrtRand() & 0x7F) + S16(rec, 8)) - 0x40);
			S16(ent, 0x20) = (int16_t)((int16_t)((CrtRand() & 0x7F) + S16(rec, 0xC)) - 0x40);
			return;
		case 1:
		{
			S16(rec, 0x12) = (int16_t)(S16(rec, 0x12) + 0x30);
			S16(ent, 0x1E) = (int16_t)(S16(ent, 0x1E) + S16(rec, 0x12));
			if (S16(ent, 0x1E) < h) return;
			S16(ent, 0x1E) = h;
			uint8_t f = U8(ent, 0);
			S16(rec, 0x12) = 0;
			if (f & 2)
			{
				ActionData *ad = &((CastContext *)U32(n, 0xC))->actions[S8(n, 0x2A)];
				int cnt = ad->target_count;
				for (int t = 0; t < cnt; t++)
				{
					if (ent != EntityAt(ad->targets[TARGET_STRIDE * t])) continue;
					int32_t r = CrtRand();
					uint8_t f8 = U8(ent, 8);
					if (r & 1)
					{
						if (!(f8 & 2)) ChainTransformationConditional(ent, 4);
					}
					else if (!(f8 & 2)) ChainTransformationConditional(ent, 5);
					break;
				}
			}
			S16(rec, 0x10) = (int16_t)(S16(rec, 0x10) - 1);
			return;
		}
		default:
			return;
		}
	}

	// sub_8DBE40: every entity back to its saved position
	static void RestorePositions()
	{
		for (int s = 0; s < 7; s++)
		{
			uint32_t rec = SAVED_POS + 0x14 * s, ent = EntityAt(s);
			U32(ent, 0xC) = U32(rec, 0);
			U32(ent, 0x10) = U32(rec, 4);
			U32(ent, 0x1C) = U32(rec, 8);
			U32(ent, 0x20) = U32(rec, 0xC);
		}
	}

	// sub_8DBBF0 (phase 7): the targets' side bounces while the mesh runs; positions restored at
	// the end
	static void Seq_Bounce(uint32_t n)
	{
		if (TargetsParty())
		{
			for (int s = 0; s < 3; s++) if (!(U8(SAVED_FLAGS, 4 * s) & 4)) Bounce(n, EntityAt(s), s);
		}
		else
		{
			for (int k = 0; k < 4; k++) if (!(U8(SAVED_FLAGS, 4 * (3 + k)) & 4)) Bounce(n, EntityAt(3 + k), k + 3);
		}
		GroundRender();
		if (State() == 7)
		{
			RestorePositions();
			NextPhase(n);
		}
	}

	// sub_8DBE80 (phase 8): hide / shadow flags back from the saved ones (as the original pairs
	// them: bit 2 of entity 3 + k from saved flags k)
	static void Seq_RestoreFlags(uint32_t n)
	{
		S16(n, 0x1C) = 0x1000;
		if (TargetsParty())
		{
			for (int k = 0; k < 4; k++)
			{
				if (U8(SAVED_FLAGS, 4 * k + 0xC) & 4) continue;
				uint32_t ent = EntityAt(3 + k);
				U16(ent, 0) ^= (uint16_t)((U8(SAVED_FLAGS, 4 * k) ^ U8(ent, 0)) & 4);
			}
			for (int s = 0; s < 3; s++)
			{
				if (U8(SAVED_FLAGS, 4 * s) & 4) continue;
				uint32_t ent = EntityAt(s);
				U16(ent, 0) ^= (uint16_t)((U16(SAVED_FLAGS, 4 * s) ^ U16(ent, 0)) & 0x1020);
			}
		}
		else
		{
			for (int s = 0; s < 3; s++)
			{
				uint8_t f = U8(SAVED_FLAGS, 4 * s);
				if (f & 4) continue;
				uint32_t ent = EntityAt(s);
				U16(ent, 0) ^= (uint16_t)((f ^ U8(ent, 0)) & 4);
			}
			for (int k = 0; k < 4; k++)
			{
				if (U8(SAVED_FLAGS, 4 * k + 0xC) & 4) continue;
				uint32_t ent = EntityAt(3 + k);
				U16(ent, 0) ^= (uint16_t)((U16(SAVED_FLAGS, 4 * k) ^ U16(ent, 0)) & 0x1020);
			}
		}
		NextPhase(n);
	}

	// sub_8DBF60 (phase 9): fade back from black
	static void Seq_FadeIn(uint32_t n)
	{
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) - 0x100);
		if (S16(n, 0x1C) <= 0)
		{
			S16(n, 0x1C) = 0;
			NextPhase(n);
		}
		DarkenCasterSide(n);
	}

	// sub_8DC180 (phase 10): colour mode bit back, colours = stage light; the sequence ends
	static void Seq_End(uint32_t n)
	{
		uint32_t light = var<uint32_t>(0xB8B7D8), light2 = var<uint32_t>(0xB8B9A8);
		if (TargetsParty())
		{
			for (int k = 0; k < 4; k++)
			{
				if (U8(SAVED_FLAGS, 4 * k + 0xC) & 4) continue;
				uint32_t ent = EntityAt(3 + k);
				U16(ent, 0) ^= (uint16_t)((U16(ent, 0) ^ U16(SAVED_FLAGS, 4 * k)) & 0x800);
				U32(ent, 0x28) = light;
				U32(ent, 0x2C) = light2;
			}
		}
		else
		{
			for (int s = 0; s < 3; s++)
			{
				if (U8(SAVED_FLAGS, 4 * s) & 4) continue;
				uint32_t ent = EntityAt(s);
				U16(ent, 0) ^= (uint16_t)((U16(ent, 0) ^ U16(SAVED_FLAGS, 4 * s)) & 0x800);
				U32(ent, 0x28) = light;
				U32(ent, 0x2C) = light2;
			}
		}
		U8(n, 0x26) |= 1;
		NextPhase(n);
	}

	// MAG_038_QUAKE_MainSequence_11Phase (0x8D8800)
	static uint32_t __cdecl SequenceTask(TaskNode *node)
	{
		uint32_t n = (uint32_t)node;
		switch (PhaseOf(n))
		{
		case 0:                                      // MAG_038_QUAKE_Phase0_InitGround
			InitGround(n);
			NextPhase(n);
			break;
		case 1:                                      // sub_8D8A10: build the mesh
			GroundRender();
			S16(n, 0x30) = 0x10;
			NextPhase(n);
			break;
		case 2:                                      // sub_8DB770: 16 ticks
		{
			GroundRender();
			S16(n, 0x30) = (int16_t)(S16(n, 0x30) - 1);
			if (S16(n, 0x30) < 0)
			{
				uint8_t mid[16];
				CopyMidpointFromSource(n, mid);
				NextPhase(n);
			}
			break;
		}
		case 3: Seq_Darken(n); break;
		case 4: Seq_FadeOut(n); break;
		case 5: Seq_Hide(n); break;
		case 6: Seq_WaitRipples(n); break;
		case 7: Seq_Bounce(n); break;
		case 8: Seq_RestoreFlags(n); break;
		case 9: Seq_FadeIn(n); break;
		case 10: Seq_End(n); break;
		default: break;                              // nullsub
		}
		return EndCheck(n);
	}

	// ==================================================================
	// camera shake (0x8DC250) and the fallback (0x8DC360)
	// ==================================================================
	inline int16_t &CameraShake() { return var<int16_t>(0x1D97712); }

	static uint32_t __cdecl ShakeTask(TaskNode *node)
	{
		uint32_t n = (uint32_t)node;
		switch (PhaseOf(n))
		{
		case 0:                                      // sub_8DC2C0: wait 3 ticks
			if (S16(n, 0x24) > 2) NextPhase(n);
			break;
		case 1:                                      // MAG_038_QUAKE_SetShakeFromTableA
		{
			int16_t s = S16(SHAKE_A, S16(n, 0x24) * 2);
			if (s == 0x7F) NextPhase(n);
			else CameraShake() = s;
			break;
		}
		case 2:                                      // sub_8DC300: wait for the ripples
			if (State() == 5) NextPhase(n);
			break;
		case 3:                                      // MAG_038_QUAKE_SetShakeFromTableB
			CameraShake() = S16(SHAKE_B, (U8(n, 0x24) & 0xF) * 2);
			if (State() == 7)
			{
				U8(n, 0x26) |= 1;
				NextPhase(n);
			}
			break;
		default: break;
		}
		return EndCheck(n);
	}

	static uint32_t __cdecl FallbackTask(TaskNode *node)
	{
		uint32_t n = (uint32_t)node;
		switch (PhaseOf(n))
		{
		case 0:                                      // au_re_BdPlaySE_8
			BdPlaySE(SOUND_Rumble, 0, 0x80);
			NextPhase(n);
			break;
		case 1:                                      // sub_8DC3E0
			if (S16(n, 0x24) >= 0x30)
			{
				BdPlaySE(SOUND_Quake, 1, 0x80);
				NextPhase(n);
			}
			break;
		case 2:                                      // MAG_038_QUAKE_SetShakeFromTableB2
		{
			int16_t t = S16(n, 0x24);
			CameraShake() = S16(SHAKE_B, (t & 0xF) * 2);
			if (t >= 0x60)
			{
				U8(n, 0x26) |= 1;
				NextPhase(n);
			}
			break;
		}
		default: break;
		}
		return EndCheck(n);
	}

	// ==================================================================
	// debris (0x8DA900): two triangles (top +0x5C, bottom +0x74, 3 vertices each) joined by
	// three quads, spinning (+0x44 about X, +0x46 about Y), thrown up and falling back
	// ==================================================================
	// sub_8DAA40: triangle of vertices i0, i1, i2 (u / v per vertex at +0x94 / +0x9A)
	static void DebrisTriangle(uint32_t n, uint32_t ot, int shift, int16_t side, int32_t i0, int32_t i1, int32_t i2)
	{
		uint32_t cur = PacketCursor();
		GteLoadV012((const void *)(n + i0 * 8 + 0x5C), (const void *)(n + i1 * 8 + 0x5C), (const void *)(n + i2 * 8 + 0x5C));
		GteRTPT();
		GteNCLIP();
		int32_t mac0;
		GteReadMAC0(&mac0);
		if (mac0 <= 0) return;
		GteStoreSXY012_FT3(cur);
		// (the original's off-screen test always passes)
		GteAVSZ3();
		uint32_t otz;
		GteReadOTZ32(&otz);
		otz >>= shift;
		U8(cur, 0xC) = U8(n, i0 + 0x94);
		U8(cur, 0xD) = U8(n, i0 + 0x9A);
		U8(cur, 0x14) = U8(n, i1 + 0x94);
		U8(cur, 0x15) = U8(n, i1 + 0x9A);
		U8(cur, 0x1C) = U8(n, i2 + 0x94);
		U32(cur, 0) = 0x7000000;
		U8(cur, 7) = 0x24;
		U8(cur, 0x1D) = U8(n, i2 + 0x9A);
		U16(cur, 0xE) = U16(n, 0xA2);
		U16(cur, 0x16) = U16(n, 0xA0);
		U32(cur, 4) = side == 0 ? U32(n, 0xA4) : U32(n, 0xA8);
		InsertPrimAutoDepth(ot + otz * 4, (void *)cur);
		PacketCursor() = cur + 0x20;
	}

	// sub_8DABC0: side quad a4 a5 a6 a7 (u / v of a4 on the left corners, of a5 on the right)
	static void DebrisQuad(uint32_t n, uint32_t ot, int shift, int32_t a4, int32_t a5, int32_t a6, int32_t a7)
	{
		uint32_t cur = PacketCursor();
		GteLoadV012((const void *)(n + a4 * 8 + 0x5C), (const void *)(n + a5 * 8 + 0x5C), (const void *)(n + a6 * 8 + 0x5C));
		GteRTPT();
		GteNCLIP();
		int32_t mac0;
		GteReadMAC0(&mac0);
		if (mac0 <= 0) return;
		GteStoreSXY012_FT4(cur);
		GteLoadV0((const void *)(n + a7 * 8 + 0x5C));
		GteRTPS();
		GteReadSXY2((void *)(cur + 0x20));
		// (the original's off-screen test always passes)
		GteAVSZ4();
		uint32_t otz;
		GteReadOTZ32(&otz);
		otz >>= shift;
		U8(cur, 0x1C) = U8(n, a4 + 0x94);
		U8(cur, 0xC) = U8(n, a4 + 0x94);
		U8(cur, 7) = 0x2C;
		U8(cur, 0x1D) = U8(n, a4 + 0x9A);
		U8(cur, 0xD) = U8(n, a4 + 0x9A);
		U32(cur, 0) = 0x9000000;
		U8(cur, 0x24) = U8(n, a5 + 0x94);
		U8(cur, 0x14) = U8(n, a5 + 0x94);
		U16(cur, 0x16) = U16(n, 0xA0);
		U8(cur, 0x25) = U8(n, a5 + 0x9A);
		U8(cur, 0x15) = U8(n, a5 + 0x9A);
		U32(cur, 4) = U32(n, 0xAC);
		U16(cur, 0xE) = U16(n, 0xA2);
		InsertPrimAutoDepth(ot + otz * 4, (void *)cur);
		PacketCursor() = cur + 0x28;
	}

	// sub_8DA960: GTE = camera * (rotate Y +0x46, X +0x44, at the debris position)
	static void DebrisDraw(uint32_t n)
	{
		uint32_t ot = RenderOT44();
		Mat4x3 m = {};
		MatrixUnit(&m);
		m.t[0] = S16(n, 0x1C);
		m.t[1] = S16(n, 0x1E);
		m.t[2] = S16(n, 0x20);
		MatrixRotY(&m, S16(n, 0x46));
		MatrixRotX(&m, S16(n, 0x44));
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		DebrisTriangle(n, ot, 2, 0, 0, 1, 2);
		DebrisTriangle(n, ot, 2, 1, 5, 4, 3);
		DebrisQuad(n, ot, 2, 1, 0, 4, 3);
		DebrisQuad(n, ot, 2, 2, 1, 5, 4);
		DebrisQuad(n, ot, 2, 0, 2, 3, 5);
	}

	// sub_8DADA0: spin, gravity 0x40, lands (ends) at y >= 0
	static void DebrisMove(uint32_t n, bool live)
	{
		uint16_t ax = (uint16_t)((U16(n, 0x8C) + U16(n, 0x44)) & 0xFFF);
		uint16_t ay = (uint16_t)((U16(n, 0x8E) + U16(n, 0x46)) & 0xFFF);
		S16(n, 0x56) = (int16_t)(S16(n, 0x56) + 0x40);
		U16(n, 0x44) = ax;
		S16(n, 0x1E) = (int16_t)(S16(n, 0x1E) + S16(n, 0x56));
		U16(n, 0x46) = ay;
		if (live && S16(n, 0x1E) >= 0)
		{
			DebrisCount() = (int16_t)(DebrisCount() - 1);
			U8(n, 0x26) |= 1;
			NextPhase(n);
		}
	}

	static uint32_t __cdecl DebrisTask(TaskNode *node)
	{
		uint32_t n = (uint32_t)node;
		switch (PhaseOf(n))
		{
		case 0:                                      // sub_8DAD50: up speed, spin speeds
		{
			int32_t r = CrtRand();
			S16(n, 0x56) = (int16_t)(-0x80 - (r & 0xFF));
			S16(n, 0x8C) = (int16_t)((CrtRand() & 0x7F) - 0x40);
			S16(n, 0x8E) = (int16_t)((CrtRand() & 0x7F) - 0x40);
			NextPhase(n);
			break;
		}
		case 1: DebrisMove(n, true); break;
		default: break;
		}
		// 30 fps layer: see mag038_quake_held.inc
		FX_HELD(held_note_debris(n);)
		DebrisDraw(n);
		return EndCheck(n);
	}

	// ==================================================================
	// sprites: rock (0x8DB030), dust (0x8DB410), chip (0x8DB630). Node: +0x1C draw position,
	// +0x4C sequence, +0x50 frame, +0x52 last frame, +0x54 size; rock / dust: 6 records of 0xC
	// bytes at +0x58 (x, y, z, pad, drawn flag); +0xA0..+0xA6 velocity / acceleration
	// ==================================================================
	// sub_8DB0F0
	static void SpriteDraw(uint32_t n)
	{
		if (U8(n, 0x26) & 4) return;
		// 30 fps layer: see mag038_quake_held.inc
		FX_HELD(held_note_sprite(n);)
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation((const void *)(n + 0x1C), 0x1000, S16(n, 0x54));
		U32((uint32_t)h, 0) = U32(n, 0x4C);
		U16((uint32_t)h, 4) = U16(n, 0x50);
		U16((uint32_t)h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// sub_8DB1D0: next flipbook frame; 1 (and hidden) past the last one
	static int32_t SpriteStep(uint32_t n)
	{
		S16(n, 0x50) = (int16_t)(S16(n, 0x50) + 1);
		if (S16(n, 0x50) > S16(n, 0x52))
		{
			U8(n, 0x26) |= 4;
			S16(n, 0x50) = S16(n, 0x52);
			return 1;
		}
		return 0;
	}

	// rock: vy += acceleration, damped by 1/8; every record rises / falls by vy
	static int16_t RockSpeed(uint32_t n)
	{
		int16_t v = (int16_t)(S16(n, 0xA6) + S16(n, 0xA2));
		return (int16_t)(v - (int16_t)(v / 8));
	}

	static uint32_t __cdecl RockTask(TaskNode *node)
	{
		uint32_t n = (uint32_t)node;
		switch (PhaseOf(n))
		{
		case 0:                                      // sub_8DB160
		{
			U32(n, 0x4C) = SEQ_Rock;
			S16(n, 0x52) = 0xF;
			int32_t r = CrtRand();
			S16(n, 0xA6) = (int16_t)(-8 - (r & 0xF));
			NextPhase(n);
			break;
		}
		case 1:                                      // sub_8DB1A0
			if (SpriteStep(n))
			{
				RockCount() = (int16_t)(RockCount() - 1);
				U8(n, 0x26) |= 1;
				NextPhase(n);
			}
			break;
		default: break;
		}
		S16(n, 0xA2) = RockSpeed(n);
		for (uint32_t rec = n + 0x58, i = 0; i < 6; i++, rec += 0xC)
		{
			if (S16(rec, 8) != 1) continue;
			S16(rec, 2) = (int16_t)(S16(rec, 2) + S16(n, 0xA2));
			U32(n, 0x1C) = U32(rec, 0);
			U32(n, 0x20) = U32(rec, 4);
			SpriteDraw(n);
		}
		return EndCheck(n);
	}

	static uint32_t __cdecl DustTask(TaskNode *node)
	{
		uint32_t n = (uint32_t)node;
		switch (PhaseOf(n))
		{
		case 0:                                      // sub_8DB4D0
		{
			U32(n, 0x4C) = SEQ_Dust;
			S16(n, 0x52) = 0xB;
			int32_t r = CrtRand();
			S16(n, 0xA2) = (int16_t)(-0x64 - (r & 0x7F));
			S16(n, 0xA6) = (int16_t)((CrtRand() & 7) + 6);
			S16(n, 0xA0) = (int16_t)((CrtRand() & 0x7F) - 0x40);
			S16(n, 0xA4) = (int16_t)((CrtRand() & 0x7F) - 0x40);
			NextPhase(n);
			break;
		}
		case 1:                                      // sub_8DB540
			if (SpriteStep(n))
			{
				DustCount() = (int16_t)(DustCount() - 1);
				U8(n, 0x26) |= 1;
				NextPhase(n);
			}
			break;
		default: break;
		}
		S16(n, 0xA2) = (int16_t)(S16(n, 0xA2) + S16(n, 0xA6));
		for (uint32_t rec = n + 0x58, i = 0; i < 6; i++, rec += 0xC)
		{
			if (S16(rec, 8) != 1) continue;
			S16(rec, 0) = (int16_t)(S16(rec, 0) + S16(n, 0xA0));
			S16(rec, 2) = (int16_t)(S16(rec, 2) + S16(n, 0xA2));
			S16(rec, 4) = (int16_t)(S16(rec, 4) + S16(n, 0xA4));
			U32(n, 0x1C) = U32(rec, 0);
			U32(n, 0x20) = U32(rec, 4);
			SpriteDraw(n);
		}
		return EndCheck(n);
	}

	// chip: vy damped by 1/16, position += velocity
	static void ChipMove(uint32_t n)
	{
		int16_t v = S16(n, 0xA2);
		int16_t nv = (int16_t)(v - (int16_t)(v / 16));
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + S16(n, 0xA0));
		S16(n, 0x1E) = (int16_t)(S16(n, 0x1E) + nv);
		S16(n, 0x20) = (int16_t)(S16(n, 0x20) + S16(n, 0xA4));
		S16(n, 0xA2) = nv;
	}

	static uint32_t __cdecl ChipTask(TaskNode *node)
	{
		uint32_t n = (uint32_t)node;
		switch (PhaseOf(n))
		{
		case 0:                                      // sub_8DB690
		{
			U32(n, 0x4C) = SEQ_Chip;
			S16(n, 0x52) = 0xB;
			int32_t r = CrtRand();
			S16(n, 0xA2) = (int16_t)(-0x30 - (r & 0xFF));
			S16(n, 0xA0) = (int16_t)((CrtRand() & 0x7F) - 0x40);
			S16(n, 0xA4) = (int16_t)((CrtRand() & 0x7F) - 0x40);
			NextPhase(n);
			break;
		}
		case 1:                                      // sub_8DB6F0
			ChipMove(n);
			if (SpriteStep(n))
			{
				uint32_t guard = U32(n, 0xA8);
				ChipCount() = (int16_t)(ChipCount() - 1);
				U8(n, 0x26) |= 1;
				U8(guard, 0) = (uint8_t)(U8(guard, 0) - 1);
				NextPhase(n);
			}
			break;
		default: break;
		}
		SpriteDraw(n);
		return EndCheck(n);
	}
}

	void register_mag038_quake()
	{
		register_port(quake038::ORIG_Root, (void *)quake038::RootTask, "Q038 RootTask", 38);
		register_port(quake038::ORIG_Emitter, (void *)quake038::EmitterTask, "Q038 EmitterTask", 38);
		register_port(quake038::ORIG_Sequence, (void *)quake038::SequenceTask, "Q038 SequenceTask", 38);
		register_port(quake038::ORIG_Shake, (void *)quake038::ShakeTask, "Q038 ShakeTask", 38);
		register_port(quake038::ORIG_Fallback, (void *)quake038::FallbackTask, "Q038 FallbackTask", 38);
		register_port(quake038::ORIG_Debris, (void *)quake038::DebrisTask, "Q038 DebrisTask", 38);
		register_port(quake038::ORIG_Rock, (void *)quake038::RockTask, "Q038 RockTask", 38);
		register_port(quake038::ORIG_Dust, (void *)quake038::DustTask, "Q038 DustTask", 38);
		register_port(quake038::ORIG_Chip, (void *)quake038::ChipTask, "Q038 ChipTask", 38);
		// 30 fps layer: see mag038_quake_held.inc
		FX_HELD(register_mag038_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag038_quake_held.inc"
#endif
