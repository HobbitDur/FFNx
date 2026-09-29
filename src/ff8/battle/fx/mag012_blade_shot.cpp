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

// Effect 12: Blade Shot (enemy attack 7 of kernel.bin, Mesmerize; MAG_012_*).
//
// Structure (setup MAG_012_BLADE_SHOT 0x6E8AE0 -> _Init 0x6E8AF0, file loader 0x6E8AC0 = the
// texture file named at 0x1365714 (mag011.tim); the setup keeps the CASTER's entity (clears its
// +0x7C bit 7) and the first target's entity, starts camera animation 0x1365634, queues the TIM,
// and starts the root task with the blade's flight from the caster's anchor 7 (+0x1C) to the
// target's default effect position (+0x24) over 32 ticks, the direction (+0x5C) and its angle):
//   RootTask (0x6E8C90) - alternates the packet arena (magic buffer + 0x2546100 / 0x254E100);
//     sound 0x136562C at the caster's hand on counter 5;
//     counters 0..31: the blade flies out (motion t += 0x80 per tick; the speed follows a cube-root
//       ramp 0x800 + cbrt-like(t/2 - 0x400)/16, sideways swing along the direction, a lift arc,
//       spin about x), drawn as prim model 0x13646EC (ZYX-like rotation 0x6E9AB0) and tilting
//       (+0x58 -= 0x20 per tick);
//     counters 32..61: it turns (once: start / end swapped, a side velocity from the last step) and
//       flies back towards the caster's hand anchor 7 (fetched every tick, 600 higher), spinning
//       0x210 per tick; the caster's blade object (+0x7C bit 7) is hidden from 32 until 62;
//     counters 32..35: impact sprite 0x1362C80 at the target's anchor 0xF1 (damage at 32);
//     counters 32..36: flash sprite 0x13628FC at the blade position of counter 32;
//     counters 32..51 / 34..53: two expanding rings (model 0x136401C, 0x6EA3E0) at the impact;
//     counters 32..51: two morphing wing pairs (models 0x1362E3C / 0x136372C, their vertex tables
//       blended in place from two key tables by 0x6E9970) turned +-0x71 / +-0x11C about the facing;
//     counters 32..41: two sparks per tick (particle task, sequence 0x1362AEC);
//     counters 32, 34, 36: five glints (particle task, sequence 0x136298C) and five streaks (streak
//       task, sequence 0x13627E4) thrown out of the impact;
//     counters 0..61: two after-images of the blade per tick (trail task; the first 12 degrees
//       behind); counters 60..64: catch sprite 0x1362C80 at the blade (+100 y), sound 0x1365630 at
//       60; runs the trail and particle queues; ends from counter 65 when both are empty.
//   PartTask (0x6E9B50, node 0x34) / StreakTask (0x6E9C10) - sprite at the node, draw THEN update
//     (velocity += acceleration, position += velocity); the streak's sprite is turned along its
//     screen-space direction (header +8).
//   TrailTask (0x6E9D60, node 0x20) - the blade model 0x1364F14 at a fixed pose, fading over 8 ticks
//     (depth-cued towards blue), drawn by the module's own prim-model renderer 0x6E9E80
//     (triangles 0x6E9EF0 / quads 0x6EA150, GTE-lit, far colour from header +8..+0xA).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2545EF0 caster entity, 0x2545EF4 texture file, 0x25460F8 target entity,
// 0x25460FC context, 0x2556100 packet cursor, queues 0x2556110 (root, pool 0x2545188 1 x 0x64),
// 0x2556120 (trails, pool 0x2545EF8 0x10 x 0x20), 0x2556130 (particles, pool 0x25451F0 0x40 x 0x34);
// the streak reads the (never written) vector 0x2556108. The morph models' vertex tables
// (0x1362E44 / 0x1363734, 49 vertices) are rewritten every tick.

#include "mag_common.h"

namespace ff8fx
{
namespace blade012
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2545EF0); }          // caster entity (setup)
	inline uint8_t *&Target() { return var<uint8_t *>(0x25460F8); }          // first target's entity (setup)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25460FC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2556100); }
	inline TaskQueue *QTrails() { return (TaskQueue *)0x2556120; }
	inline TaskQueue *QParts() { return (TaskQueue *)0x2556130; }
	static const uint32_t ARENA_A = 0x2546100, ARENA_B = 0x254E100;
	static const uint32_t STATIC_V0 = 0x2556108;         // streak: vector never written by the module

	static const uint32_t ORIG_RootTask = 0x6E8C90;
	static const uint32_t ORIG_PartTask = 0x6E9B50;
	static const uint32_t ORIG_StreakTask = 0x6E9C10;
	static const uint32_t ORIG_TrailTask = 0x6E9D60;
	static const uint32_t MODEL_Blade = 0x13646EC;       // prim models
	static const uint32_t MODEL_Ring = 0x136401C;
	static const uint32_t MODEL_Trail = 0x1364F14;
	static const uint32_t MODEL_WingA = 0x1362E3C;       // morph models (vertex table at +8)
	static const uint32_t KEYS_WingA0 = 0x136340C, KEYS_WingA1 = 0x136359C;
	static const uint32_t MODEL_WingB = 0x136372C;
	static const uint32_t KEYS_WingB0 = 0x1363CFC, KEYS_WingB1 = 0x1363E8C;
	static const uint32_t SEQ_Hit = 0x1362C80;           // sprite sequences
	static const uint32_t SEQ_Flash = 0x13628FC;
	static const uint32_t SEQ_Spark = 0x1362AEC;
	static const uint32_t SEQ_Glint = 0x136298C;
	static const uint32_t SEQ_Streak = 0x13627E4;
	static const void *const SOUND_Throw = (const void *)0x136562C;
	static const void *const SOUND_Catch = (const void *)0x1365630;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline int32_t CartesianToGameAngle(int32_t x, int32_t y) { return fn<int32_t (__cdecl *)(int32_t, int32_t)>(0x56D160)(x, y); }
	inline void ApplyZRotation(int32_t angle, Mat4x3 *m) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x56D090)(angle, m); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); } // GTE_MatrixMultiply: b = a * b
	// software GTE (prim lists of the module's own renderer)
	namespace gte
	{
		inline void LoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
		inline void RTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
		inline void RTPS() { fn<void (__cdecl *)()>(0x45FAA0)(); }
		inline void ReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
		inline void NCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
		inline void ReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
		inline void ReadSXY012_Split(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
		inline void ReadSXY2(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E260)(dst); }
		inline void AVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
		inline void AVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
		inline void ReadOTZ(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
		inline void SetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
		inline void LoadRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E120)(a, b, c); } // sub_45E120
		inline void DPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }                                   // sub_45F4C0
		inline void StoreRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E370)(a, b, c); } // sub_45E370
		inline void LoadRGBC(uint32_t src) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(src); }         // set_unk_1CA8A28
		inline void DPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                                   // sub_45F270
		inline void StoreRGB2(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(dst); }        // set_param_with_dword_1CA8A68
	}

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	// a straight motion from start to end, t 0..0x1000 (4.12)
	struct Motion
	{
		int16_t pos[4];    // +0x00 current point (pos[3] = copy of start[3])
		int16_t start[3];  // +0x08
		int16_t t;         // +0x0E
		int16_t end[3];    // +0x10
		int16_t dt;        // +0x16
	};
	struct RootNode // pool of 1 node of 0x64 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint32_t arena;    // +0x10 packet arena parity
		Motion mot;        // +0x14 flight (out: caster hand -> target; back: target -> hand)
		int16_t blade[3];  // +0x2C blade position
		int16_t ang;       // +0x32 out: speed ramp angle of the tick
		int16_t prev[4];   // +0x34 blade position (+ angle) of the previous tick
		int16_t accx;      // +0x3C back: side offsets
		int16_t pad3E;
		int16_t accz;      // +0x40
		int16_t pad42;
		int16_t velx;      // +0x44 back: side offset steps
		int16_t pad46;
		int16_t velz;      // +0x48
		int16_t pad4A;
		int16_t hit[4];    // +0x4C impact position (target anchor 0xF1, then the blade at 32)
		int16_t rot[4];    // +0x54 blade rotation (x spin, y facing, z tilt)
		int16_t dirx;      // +0x5C unit direction (4.12), negated on the turn
		int16_t dirz;      // +0x5E
		uint8_t turned;    // +0x60
		uint8_t pad61[3];
	};
	struct PartNode // particle queue: pool of 0x40 nodes of 0x34 bytes (particles and streaks)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C flipbook frame
		int16_t pad0E;
		uint32_t seq;      // +0x10 sprite sequence
		int16_t pos[3];    // +0x14
		int16_t life;      // +0x1A
		int16_t vel[3];    // +0x1C
		int16_t scale;     // +0x22 (streaks: 0, unused)
		int16_t acc[3];    // +0x24
		int16_t pad2A;
		int16_t dir[4];    // +0x2C streaks: initial velocity (screen direction reference)
	};
	struct TrailNode // pool of 0x10 nodes of 0x20 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[3];    // +0x10 blade position at spawn
		int16_t fade;      // +0x16 strength (counter << 9 until counter 8)
		int16_t rot[4];    // +0x18 blade rotation at spawn
	};
#pragma pack(pop)
	static_assert(sizeof(Motion) == 0x18 && sizeof(RootNode) == 0x64, "Blade Shot nodes");
	static_assert(sizeof(PartNode) == 0x34 && sizeof(TrailNode) == 0x20, "Blade Shot nodes");

	// Thunder_InitBoltTask (0x6DA380): rep stosd of `count` dwords
	static void FillDwords(void *dst, uint32_t value, uint32_t count)
	{
		uint32_t *d = (uint32_t *)dst;
		for (uint32_t k = 0; k < count; k++) d[k] = value;
	}

	// MAG_003_sub_6D5940: rotation about the X axis (translation and pad untouched)
	static void RotX(int16_t angle, Mat4x3 *m)
	{
		const int32_t s = ComputeSin(angle);
		const int32_t c = ComputeCos(angle);
		m->m[1][2] = (int16_t)-s;
		m->m[2][1] = (int16_t)s;
		m->m[0][0] = 0x1000;
		m->m[0][1] = 0;
		m->m[0][2] = 0;
		m->m[1][0] = 0;
		m->m[1][1] = (int16_t)c;
		m->m[2][0] = 0;
		m->m[2][2] = (int16_t)c;
	}

	// MAG_011_sub_6D9510: rotation about y (3x3 only)
	static void YawMatrix(int16_t yaw, Mat4x3 *m)
	{
		const int32_t sn = ComputeSin(yaw);
		const int32_t cs = ComputeCos(yaw);
		m->m[0][2] = (int16_t)sn;
		m->m[2][0] = (int16_t)-sn;
		m->m[0][0] = (int16_t)cs;
		m->m[0][1] = 0;
		m->m[1][0] = 0;
		m->m[1][1] = 0x1000;
		m->m[1][2] = 0;
		m->m[2][1] = 0;
		m->m[2][2] = (int16_t)cs;
	}

	// 0x6E9AB0: m = Ry(angles[1]) * Rz(angles[2]) * Rx(angles[0]) (3x3; the y matrix's own
	// translation words are never read)
	static void AngleMatrix(const int16_t *angles, Mat4x3 *m)
	{
		RotX(angles[0], m);
		ApplyZRotation(angles[2], m);
		Mat4x3 y;
		YawMatrix(angles[1], &y);
		MatrixMultiply(&y, m);
	}

	// 0x6E8C60: restart a motion at its start, t = 0, dt = 0x1000 / ticks
	static void MotionInit(Motion *m, int32_t ticks)
	{
		memcpy(m->pos, m->start, 8);
		m->t = 0;
		m->dt = (int16_t)(0x1000 / ticks);
	}

	// 0x6E9B00: t += dt, pos = start * (1 - t) + end * t (GTE interpolation)
	static void MotionStep(Motion *m)
	{
		m->t = (int16_t)(m->t + m->dt);
		GteSetIR0(0x1000 - (int32_t)m->t);
		GteLoadIR123(m->start);
		GteGPF();
		GteSetIR0((int32_t)m->t);
		GteLoadIR123(m->end);
		GteGPL();
		GteStoreIR123(m->pos);
	}

	// 0x6E9970: the vertex table of the header's model = key tables k0 / k1 (vertices at +8) blended by t
	static void MorphVertices(const uint8_t *h, uint32_t k0, uint32_t k1, int32_t t)
	{
		const uint32_t model = *(const uint32_t *)h;
		uint32_t dst = model + 8;
		uint32_t count = *(const uint32_t *)(model + 4);
		uint32_t a = k0 + 8, b = k1 + 8;
		for (; count != 0; count--, dst += 8, a += 8, b += 8)
		{
			GteSetIR0(0x1000 - t);
			GteLoadIR123((const void *)a);
			GteGPF();
			GteSetIR0(t);
			GteLoadIR123((const void *)b);
			GteGPL();
			GteStoreIR123((void *)dst);
		}
	}

	// 0x6E99F0: unit vector of the polar / azimuth angles ang[1] / ang[2] (ang[0] unused)
	static void SphereDir(const int16_t *ang, int16_t *out)
	{
		const int32_t s = ComputeSin(ang[1]);
		out[2] = (int16_t)ComputeCos(ang[1]);
		out[1] = (int16_t)(mul32(ComputeSin(ang[2]), s) >> 12);
		out[0] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeCos(ang[2]), s)) >> 12);
	}

	// 0x6E9A50: in turned about y by angle
	static void YawVector(const int16_t *in, int16_t *out, int32_t angle)
	{
		const int32_t s = (int32_t)(0u - (uint32_t)ComputeSin(angle));
		const int32_t c = ComputeCos(angle);
		out[0] = (int16_t)((int32_t)((uint32_t)mul32(in[0], c) - (uint32_t)mul32(in[2], s)) >> 12);
		out[2] = (int16_t)((int32_t)((uint32_t)mul32(in[0], s) + (uint32_t)mul32(in[2], c)) >> 12);
		out[1] = in[1];
	}

	// ------------------------------------------------------------------
	// The module's prim-model renderer (0x6E9E80) and its lists
	// ------------------------------------------------------------------
	static inline bool out_x(int16_t v) { return v < 0 || v > 0xA00; }
	static inline bool out_y(int16_t v) { return v < 0 || v > 0x6C0; }

	// 0x6E9EF0: Gouraud triangles, 0x14-byte records (code + rgb0, u16 vertex indices +4/+6/+8, rgb1
	// +0x0C, rgb2 +0x10); the three vertices are first copied into the header (+0x34/+0x3C/+0x44)
	// -> 0x1C-byte POLY_G3 (tag 0x06000000), colours depth-cued (far colour = header +8..+0xA, IR0 =
	// header +0xC); OT index (OTZ - 8) >> mode, only when > 0
	static uint32_t DrawTriangles(uint32_t h, uint32_t ot, uint32_t mode, uint32_t cursor)
	{
		uint32_t list = *(const uint32_t *)(h + 0x20);
		const uint32_t vbase = *(const uint32_t *)(h + 4);
		uint32_t count = *(const uint32_t *)list;
		uint32_t rec = list + 4;
		*(uint32_t *)(h + 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			*(uint32_t *)(h + 0x20) = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			const uint32_t v0 = vbase + (uint32_t)*(const uint16_t *)(rec + 4) * 4;
			*(uint32_t *)(h + 0x34) = *(const uint32_t *)v0;
			*(uint32_t *)(h + 0x38) = *(const uint32_t *)(v0 + 4);
			const uint32_t v1 = vbase + (uint32_t)*(const uint16_t *)(rec + 6) * 4;
			*(uint32_t *)(h + 0x3C) = *(const uint32_t *)v1;
			*(uint32_t *)(h + 0x40) = *(const uint32_t *)(v1 + 4);
			const uint32_t v2 = vbase + (uint32_t)*(const uint16_t *)(rec + 8) * 4;
			*(uint32_t *)(h + 0x44) = *(const uint32_t *)v2;
			*(uint32_t *)(h + 0x48) = *(const uint32_t *)(v2 + 4);
			gte::LoadV012(h + 0x34, h + 0x3C, h + 0x44);
			gte::RTPT();
			*(uint32_t *)cursor = 0x6000000;
			*(uint32_t *)(pkt + 4) = *(const uint32_t *)rec;
			gte::ReadFLAG(h + 0x30);
			if (!(*(const uint32_t *)(h + 0x30) & 0x60000))
			{
				gte::NCLIP();
				uint32_t clip = 0;
				gte::ReadMAC0(h + 0x24);
				if (*(const int32_t *)(h + 0x24) >= 0 || (*(const uint8_t *)(h + 0x1C) & 0x20))
				{
					gte::ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					gte::AVSZ3();
					if (out_x(*(const int16_t *)(pkt + 8))) clip = 1;
					if (out_x(*(const int16_t *)(pkt + 0x10))) clip |= 2;
					if (out_x(*(const int16_t *)(pkt + 0x18))) clip |= 4;
					if (out_y(*(const int16_t *)(pkt + 0x0A))) clip |= 0x10;
					if (out_y(*(const int16_t *)(pkt + 0x12))) clip |= 0x20;
					if (out_y(*(const int16_t *)(pkt + 0x1A))) clip |= 0x40;
					gte::ReadOTZ(h + 0x2C);
					if ((uint8_t)(clip & 7) != 7 && (uint8_t)(clip & 0x70) != 0x70)
					{
						const int32_t z = *(const int32_t *)(h + 0x2C) - 8;
						*(int32_t *)(h + 0x2C) = z;
						if (z > 0)
						{
							gte::SetFarColor(*(const uint8_t *)(h + 8), *(const uint8_t *)(h + 9), *(const uint8_t *)(h + 0xA));
							gte::LoadRGB012(rec + 0x0C, rec + 0x10, pkt + 4);
							GteSetIR0(*(const int32_t *)(h + 0xC));
							gte::DPCT();
							gte::StoreRGB012(pkt + 0x0C, pkt + 0x14, pkt + 4);
							InsertPrimAutoDepth(ot + (uint32_t)(*(const int32_t *)(h + 0x2C) >> (mode & 31)) * 4, (void *)cursor);
							cursor += 0x1C;
							pkt += 0x1C;
						}
					}
				}
			}
			rec += 0x14;
		} while (--count);
		*(uint32_t *)(h + 0x20) = rec;
		return cursor;
	}

	// 0x6EA150: Gouraud quads, 0x18-byte records (4th vertex index +0x0A, rgb1..3 +0x0C/+0x10/+0x14)
	// -> 0x24-byte POLY_G4 (tag 0x08000000), depth-cued like the triangles; a quad whose OTZ - 8 is
	// <= 0 still takes its packet space
	static uint32_t DrawQuads(uint32_t h, uint32_t ot, uint32_t mode, uint32_t cursor)
	{
		uint32_t list = *(const uint32_t *)(h + 0x20);
		const uint32_t vbase = *(const uint32_t *)(h + 4);
		uint32_t count = *(const uint32_t *)list;
		uint32_t rec = list + 4;
		*(uint32_t *)(h + 0x20) = rec;
		if ((int32_t)count <= 0)
		{
			*(uint32_t *)(h + 0x20) = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			gte::LoadV012(vbase + (uint32_t)*(const uint16_t *)(rec + 4) * 4, vbase + (uint32_t)*(const uint16_t *)(rec + 6) * 4, vbase + (uint32_t)*(const uint16_t *)(rec + 8) * 4);
			gte::RTPT();
			*(uint32_t *)cursor = 0x8000000;
			*(uint32_t *)(pkt + 4) = *(const uint32_t *)rec;
			gte::ReadFLAG(h + 0x30);
			if (!(*(const uint32_t *)(h + 0x30) & 0x60000))
			{
				gte::NCLIP();
				uint32_t clip = 0;
				gte::ReadMAC0(h + 0x24);
				if (*(const int32_t *)(h + 0x24) >= 0 || (*(const uint8_t *)(h + 0x1C) & 0x20))
				{
					gte::ReadSXY012_Split(pkt + 8, pkt + 0x10, pkt + 0x18);
					GteLoadV0((const void *)(vbase + (uint32_t)*(const uint16_t *)(rec + 0x0A) * 4));
					GteRTPS();
					if (out_x(*(const int16_t *)(pkt + 8))) clip = 1;
					if (out_x(*(const int16_t *)(pkt + 0x10))) clip |= 2;
					if (out_x(*(const int16_t *)(pkt + 0x18))) clip |= 4;
					if (out_y(*(const int16_t *)(pkt + 0x0A))) clip |= 0x10;
					if (out_y(*(const int16_t *)(pkt + 0x12))) clip |= 0x20;
					if (out_y(*(const int16_t *)(pkt + 0x1A))) clip |= 0x40;
					gte::ReadSXY2(pkt + 0x20);
					gte::AVSZ4();
					if (out_x(*(const int16_t *)(pkt + 0x20))) clip |= 8;
					if (out_y(*(const int16_t *)(pkt + 0x22))) clip |= 0x80;
					if ((uint8_t)(clip & 0xF) != 0xF && (uint8_t)(clip & 0xF0) != 0xF0)
					{
						gte::ReadOTZ(h + 0x2C);
						const int32_t z = *(const int32_t *)(h + 0x2C) - 8;
						*(int32_t *)(h + 0x2C) = z;
						if (z > 0)
						{
							gte::LoadRGB012(rec + 0x0C, rec + 0x10, rec + 0x14);
							GteSetIR0(*(const int32_t *)(h + 0xC));
							gte::DPCT();
							gte::StoreRGB012(pkt + 0x0C, pkt + 0x14, pkt + 0x1C);
							gte::LoadRGBC(pkt + 4);
							gte::DPCS();
							gte::StoreRGB2(pkt + 4);
							InsertPrimAutoDepth(ot + (uint32_t)(*(const int32_t *)(h + 0x2C) >> (mode & 31)) * 4, (void *)cursor);
						}
						cursor += 0x24;
						pkt += 0x24;
					}
				}
			}
			rec += 0x18;
		} while (--count);
		*(uint32_t *)(h + 0x20) = rec;
		return cursor;
	}

	// 0x6E9E80: header [0] = model (vertices at +8, lists at model + [model] + 0x10: triangles then quads)
	static uint32_t DrawModel(uint8_t *h, uint32_t ot, uint32_t mode, uint32_t cursor)
	{
		const uint32_t model = *(const uint32_t *)h;
		*(uint32_t *)(h + 4) = model + 8;
		uint32_t list = model + *(const uint32_t *)model + 0x10;
		*(uint32_t *)(h + 0x20) = list;
		if (*(const uint32_t *)list != 0)
			cursor = DrawTriangles((uint32_t)h, ot, mode, cursor);
		else
			*(uint32_t *)(h + 0x20) = list + 4;
		list = *(const uint32_t *)(h + 0x20);
		if (*(const uint32_t *)list != 0)
			return DrawQuads((uint32_t)h, ot, mode, cursor);
		*(uint32_t *)(h + 0x20) = list + 4;
		return cursor;
	}

	// ------------------------------------------------------------------
	// Draws
	// ------------------------------------------------------------------
	// the blade (prim model) at the root's position / rotation (header h allocated by the caller)
	static void BladeDraw(const RootNode *n, uint8_t *h)
	{
		Mat4x3 m;
		AngleMatrix(n->rot, &m);
		m.t[0] = n->blade[0];
		m.t[1] = n->blade[1];
		m.t[2] = n->blade[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = MODEL_Blade;
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
	}

	// a sprite sequence at pos, frame (header h allocated by the caller)
	static void SeqDraw(const int16_t *pos, uint8_t *h, uint32_t seq, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, 0x1000, -0x400);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	// ring expansion s (4.12 over 20 ticks) of 0x6EA3E0
	static uint32_t RingScale(int32_t k) { return (uint32_t)shl32(k, 12) / 20; }

	// 0x6EA3E0 body: the ring model at the impact, its z row stretched by 1 + s, pushed out along
	// the facing by s / 4, fading in with sin(s / 4)
	static void RingDraw(const RootNode *n, uint32_t s)
	{
		uint8_t *h = AllocHeader(0x58);
		Mat4x3 m;
		YawMatrix(n->rot[1], &m);
		const int32_t g = (int32_t)s + 0x1000;
		m.m[2][0] = (int16_t)(mul32(m.m[2][0], g) >> 12);
		const int32_t tx = (mul32(m.m[0][2], (int32_t)s) >> 14) + n->hit[0];
		m.m[2][2] = (int16_t)(mul32(m.m[2][2], g) >> 12);
		const int32_t tz = (mul32(m.m[0][0], (int32_t)s) >> 14) + n->hit[2];
		m.t[0] = tx;
		m.t[1] = *(const int16_t *)(Target() + 0x24);
		m.t[2] = tz;
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)h = MODEL_Ring;
		*(int32_t *)(h + 0xC) = ComputeSin((int32_t)s >> 2);
		*(uint32_t *)(h + 0x1C) = 0xF0;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// 0x6EA3E0: ring k (0..19) of the impact
	static void Ring(const RootNode *n, int32_t k)
	{
		if ((uint32_t)k >= 0x14) return;
		RingDraw(n, RingScale(k));
	}

	// a morphing wing pair at the impact: the model's vertices blended by sin(s / 4), drawn turned
	// by the facing +- yaw
	static void WingPair(const RootNode *n, uint32_t s, uint32_t model, uint32_t k0, uint32_t k1, int16_t yaw)
	{
		uint8_t *h = AllocHeader(0x58);
		const int32_t q = (int32_t)s >> 2;
		*(uint32_t *)h = model;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(int32_t *)(h + 0xC) = ComputeSin(q);
		*(uint32_t *)(h + 0x1C) = 0xF0;
		MorphVertices(h, k0, k1, ComputeSin(q));
		Mat4x3 m;
		YawMatrix((int16_t)(n->rot[1] + yaw), &m);
		m.t[0] = n->hit[0];
		m.t[1] = n->hit[1];
		m.t[2] = n->hit[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		YawMatrix((int16_t)(n->rot[1] - yaw), &m);
		m.t[0] = n->hit[0];
		m.t[1] = n->hit[1];
		m.t[2] = n->hit[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// particle / glint sprite (header h allocated by the caller)
	static void PartDraw(const PartNode *p, uint8_t *h)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -((int32_t)p->scale >> 2));
		*(uint32_t *)h = p->seq;
		*(int16_t *)(h + 4) = p->counter;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	// streak sprite, turned along its direction on screen (header h allocated by the caller)
	static void StreakDraw(const PartNode *p, uint8_t *h)
	{
		int16_t d[4]; // d[3] is never written (the camera transform reads x, y, z)
		d[0] = (int16_t)(p->pos[0] - p->dir[0]);
		d[1] = (int16_t)(p->pos[1] - p->dir[1]);
		d[2] = (int16_t)(p->pos[2] - p->dir[2]);
		TransformCameraByShadowRotation(d, 0x800, -0x200);
		GteLoadV0((const void *)STATIC_V0);
		GteMVMVA_RotV0Tr();
		int16_t a[3];
		GteStoreIR123(a);
		TransformCameraByShadowRotation(p->pos, 0x800, -0x200);
		GteLoadV0(p->dir);
		GteMVMVA_RotV0Tr();
		int16_t b[3];
		GteStoreIR123(b);
		const int32_t ang = CartesianToGameAngle((int32_t)a[1] - b[1], (int32_t)b[0] - a[0]);
		*(int32_t *)(h + 8) = (int32_t)(0u - (uint32_t)ang);
		*(uint32_t *)h = p->seq;
		*(int16_t *)(h + 4) = p->counter;
		*(int16_t *)(h + 0x24) = 1;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	// after-image fade angle of a trail counter
	static int32_t TrailAngle(int16_t counter) { return shl32(counter, 10) / 8; }

	// after-image: the blade model at the node's pose, depth-cued by sin(a) (and the node's fade)
	static void TrailDraw(const TrailNode *t, uint8_t *h, int32_t a)
	{
		Mat4x3 m;
		AngleMatrix(t->rot, &m);
		m.t[0] = t->pos[0];
		m.t[1] = t->pos[1];
		m.t[2] = t->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		int32_t v = ComputeSin(a);
		*(int32_t *)(h + 0xC) = v;
		if (t->fade < 0x1000)
		{
			v = 0x1000 - (mul32(0x1000 - v, t->fade) >> 12);
			*(int32_t *)(h + 0xC) = v;
		}
		h[9] = 0;
		h[8] = 0;
		v = *(const int32_t *)(h + 0xC);
		*(uint32_t *)(h + 0x1C) = 0xC0;
		*(uint32_t *)h = MODEL_Trail;
		h[0xA] = (uint8_t)((0x1000 - v) >> 5);
		*(int32_t *)(h + 0xC) = (v >> 1) + 0x800;
		PacketCursor() = DrawModel(h, RenderOT(), 2, PacketCursor());
	}

	// ------------------------------------------------------------------
	// Blade motion (root task)
	// ------------------------------------------------------------------
	// counters 0..31: flight out
	static void BladeOut(RootNode *n)
	{
		const int32_t x = ((int32_t)n->mot.t >> 1) - 0x400;
		int32_t r;
		if (x < 0)
			r = (int32_t)(0u - (uint32_t)Sqrt((int32_t)(0u - (uint32_t)mul32(mul32(x, x), x))));
		else
			r = Sqrt(mul32(mul32(x, x), x));
		const int32_t a = (r >> 4) + 0x800;
		MotionStep(&n->mot);
		n->rot[0] = (int16_t)(n->rot[0] + (int16_t)(((uint32_t)(uint16_t)((int16_t)a - n->ang)) * 4 + 0x294));
		n->ang = (int16_t)a;
		const int32_t s1 = ComputeSin(a >> 1);
		const int32_t s2 = ComputeSin(a);
		memcpy(n->prev, n->blade, 8);
		const int32_t e = s2 >> 2;
		n->blade[0] = (int16_t)((mul32(n->dirx, e) >> 12) + n->mot.pos[0]);
		n->blade[1] = (int16_t)(((int32_t)(0u - (uint32_t)mul32(s1, s1)) >> 13) + n->mot.pos[1]);
		n->blade[2] = (int16_t)((mul32(n->dirz, e) >> 12) + n->mot.pos[2]);
	}

	// counter 32 (once): the blade turns: side steps from its last step, start / end swapped, 30 ticks
	static void BladeTurn(RootNode *n)
	{
		n->turned = 1;
		int32_t d = (int32_t)n->mot.end[0] - n->mot.start[0];
		n->velx = (int16_t)((int16_t)(d / 32) - n->prev[0] + n->blade[0]);
		d = (int32_t)n->mot.end[2] - n->mot.start[2];
		n->velz = (int16_t)((int16_t)(d / 32) - n->prev[2] + n->blade[2]);
		uint8_t s[8];
		memcpy(s, n->mot.start, 8);
		memcpy(n->mot.start, n->mot.end, 8);
		memcpy(n->mot.end, s, 8);
		MotionInit(&n->mot, 0x1E);
		d = (int32_t)n->mot.start[0] - n->mot.end[0];
		n->velx = (int16_t)((int16_t)(d / 30 + n->velx) >> 1);
		d = (int32_t)n->mot.start[2] - n->mot.end[2];
		n->velz = (int16_t)((int16_t)(d / 30 + n->velz) >> 1);
		n->dirx = (int16_t)-n->dirx;
		n->dirz = (int16_t)-n->dirz;
	}

	// counters 32..61, after the hand anchor went to mot.end: flight back
	static void BladeBack(RootNode *n)
	{
		n->mot.end[1] = (int16_t)(n->mot.end[1] - 600);
		MotionStep(&n->mot);
		n->rot[0] = (int16_t)(n->rot[0] + 0x210);
		n->accx = (int16_t)(n->accx + n->velx);
		n->accz = (int16_t)(n->accz + n->velz);
		const int32_t t = n->mot.t;
		const int32_t w = 0x1000 - t;
		const int32_t ox = mul32(n->accx, w) >> 12;
		const int32_t oz = mul32(n->accz, w) >> 12;
		const int32_t s = ComputeSin(t >> 1);
		memcpy(n->prev, n->blade, 8);
		n->blade[0] = (int16_t)(n->mot.pos[0] + ox);
		n->blade[1] = (int16_t)(n->mot.pos[1] + ((int32_t)(0u - (uint32_t)s) >> 2));
		n->blade[2] = (int16_t)(n->mot.pos[2] + oz);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag012_blade_shot_held.h"
#endif

namespace ff8fx
{
namespace blade012
{
	// ------------------------------------------------------------------
	// Particle tasks
	// ------------------------------------------------------------------
	static uint32_t __cdecl PartTask(TaskNode *tn)
	{
		PartNode *p = (PartNode *)tn;
		// 30 fps layer: see mag012_blade_shot_held.inc
		FX_HELD(held_note(K_PART, p, sizeof(PartNode));)
		uint8_t *h = AllocHeader(0xB4);
		PartDraw(p, h);
		FieldFree(0xB4);
		p->counter++;
		if (p->counter >= p->life) return TASK_END;
		p->vel[1] = (int16_t)(p->vel[1] + p->acc[1]);
		p->vel[2] = (int16_t)(p->vel[2] + p->acc[2]);
		p->vel[0] = (int16_t)(p->vel[0] + p->acc[0]);
		p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
		p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
		p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
		return 0;
	}

	static uint32_t __cdecl StreakTask(TaskNode *tn)
	{
		PartNode *p = (PartNode *)tn;
		// 30 fps layer: see mag012_blade_shot_held.inc
		FX_HELD(held_note(K_STREAK, p, sizeof(PartNode));)
		uint8_t *h = AllocHeader(0xB4);
		StreakDraw(p, h);
		FieldFree(0xB4);
		p->counter++;
		if (p->counter >= p->life) return TASK_END;
		p->vel[0] = (int16_t)(p->vel[0] + p->acc[0]);
		p->vel[1] = (int16_t)(p->vel[1] + p->acc[1]);
		p->vel[2] = (int16_t)(p->vel[2] + p->acc[2]);
		p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
		p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
		p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
		return 0;
	}

	static uint32_t __cdecl TrailTask(TaskNode *tn)
	{
		TrailNode *t = (TrailNode *)tn;
		// 30 fps layer: see mag012_blade_shot_held.inc
		FX_HELD(held_note(K_TRAIL, t, sizeof(TrailNode));)
		uint8_t *h = AllocHeader(0x58);
		TrailDraw(t, h, TrailAngle(t->counter));
		FieldFree(0x58);
		t->counter++;
		return t->counter < 8 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Spawns (root task)
	// ------------------------------------------------------------------
	// counters 32..41: two sparks per tick falling out of the impact, one to each side
	static void SpawnSparks(const RootNode *n)
	{
		for (int32_t i = 2; i != 0; i--)
		{
			PartNode *p = (PartNode *)AddTaskToQueue(QParts(), ORIG_PartTask);
			if (!p) continue;
			FillDwords(&p->counter, 0, 10);
			const int32_t side = i & 1;
			int32_t a = ((CrtRand() - 0x4000) >> 6) + (side ? 0x400 : -0x400) + n->rot[1];
			p->pos[0] = (int16_t)((ComputeSin(a) >> 3) + n->hit[0]);
			p->pos[2] = (int16_t)((ComputeCos(a) >> 3) + n->hit[2]);
			p->pos[1] = (int16_t)(*(const int16_t *)(Target() + 0x24) - (CrtRand() >> 8));
			a = ((CrtRand() - 0x4000) >> 6) + (side ? 0x190 : -0x190) + n->rot[1];
			int16_t v = (int16_t)(ComputeSin(a) >> 4);
			p->vel[0] = v;
			p->acc[0] = (int16_t)(-(int32_t)v >> 5);
			v = (int16_t)(ComputeCos(a) >> 4);
			p->vel[2] = v;
			p->acc[2] = (int16_t)(-(int32_t)v >> 5);
			v = (int16_t)((int32_t)(0u - (uint32_t)CrtRand()) >> 10);
			p->vel[1] = v;
			p->seq = SEQ_Spark;
			p->acc[1] = (int16_t)(-(int32_t)v >> 5);
			p->life = 0xF;
			p->scale = (int16_t)((CrtRand() >> 4) + 0xC00);
		}
	}

	// counters 32, 34, 36: five particles thrown around the facing (glints: shift 5, 13 frames;
	// streaks: shift 4, 8 frames)
	static void SpawnFan(const RootNode *n, int32_t k, bool streaks)
	{
		int16_t ang[3]; // ang[0] is never used
		ang[1] = (int16_t)((CrtRand() >> 9) + (streaks ? 0x300 : 0x1E0));
		ang[2] = (int16_t)(k * 17 * 8 + (streaks ? 0 : 0x44));
		for (int32_t i = 5; i != 0; i--)
		{
			PartNode *p = (PartNode *)AddTaskToQueue(QParts(), streaks ? ORIG_StreakTask : ORIG_PartTask);
			if (p)
			{
				FillDwords(&p->counter, 0, 10);
				int16_t dir[3];
				SphereDir(ang, dir);
				YawVector(dir, p->pos, n->rot[1]);
				memcpy(p->vel, p->pos, 8);
				p->pos[0] = (int16_t)((p->pos[0] >> 3) + n->hit[0]);
				p->pos[1] = (int16_t)(n->hit[1] + (p->pos[1] >> 3));
				p->pos[2] = (int16_t)((p->pos[2] >> 3) + n->hit[2]);
				const int sh = streaks ? 4 : 5;
				p->vel[0] = (int16_t)(p->vel[0] >> sh);
				p->vel[1] = (int16_t)(p->vel[1] >> sh);
				p->vel[2] = (int16_t)(p->vel[2] >> sh);
				p->seq = streaks ? SEQ_Streak : SEQ_Glint;
				p->acc[0] = (int16_t)(-(int32_t)p->vel[0] >> 4);
				p->acc[1] = (int16_t)(-(int32_t)p->vel[1] >> 4);
				p->acc[2] = (int16_t)(-(int32_t)p->vel[2] >> 4);
				if (streaks)
				{
					memcpy(p->dir, p->vel, 8);
					p->life = 8;
				}
				else
				{
					p->life = 0xD;
					p->scale = 0x800;
				}
			}
			ang[2] = (int16_t)(ang[2] + (CrtRand() >> 9) + 0x313);
		}
	}

	// counters 0..61: two after-images of the blade (the first turned back by 0x14A)
	static void SpawnTrails(const RootNode *n)
	{
		for (int32_t i = 2; i != 0; i--)
		{
			TrailNode *t = (TrailNode *)AddTaskToQueue(QTrails(), ORIG_TrailTask);
			if (!t) continue;
			FillDwords(&t->counter, 0, 5);
			memcpy(t->pos, n->blade, 8);
			memcpy(t->rot, n->rot, 8);
			if (!(i & 1)) t->rot[0] = (int16_t)(t->rot[0] - 0x14A);
			if (n->counter < 8) t->fade = (int16_t)shl32(n->counter, 9);
			else t->fade = 0x1000;
		}
	}

	// ------------------------------------------------------------------
	// Root (0x6E8C90)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *tn)
	{
		RootNode *n = (RootNode *)tn;
		// 30 fps layer: see mag012_blade_shot_held.inc
		FX_HELD(held_note_root(n);)
		if (n->arena != 0)
		{
			PacketCursor() = ARENA_A;
			n->arena = 0;
		}
		else
		{
			PacketCursor() = ARENA_B;
			n->arena = 1;
		}
		if (n->counter == 5) BdPlaySE3D(SOUND_Throw, 0, n->mot.start);
		if ((uint16_t)n->counter < 0x20)
		{
			uint8_t *h = AllocHeader(0x58);
			BladeOut(n);
			// 30 fps layer: see mag012_blade_shot_held.inc
			FX_HELD(held_note_h(K_BLADE, n, h, 0x58, 0);)
			BladeDraw(n, h);
			FieldFree(0x58);
			n->rot[2] = (int16_t)(n->rot[2] - 0x20);
		}
		else
			n->rot[2] = 0;
		int32_t k = (int32_t)n->counter - 0x20;
		if ((uint32_t)k < 0x1E)
		{
			uint8_t *h = AllocHeader(0x58);
			if (k == 0 && n->turned == 0) BladeTurn(n);
			GetEffectSpawnPosition(Caster(), 7, 0, n->mot.end);
			BladeBack(n);
			// 30 fps layer: see mag012_blade_shot_held.inc
			FX_HELD(held_note_h(K_BLADE, n, h, 0x58, 0);)
			BladeDraw(n, h);
			FieldFree(0x58);
		}
		uint32_t &shown = *(uint32_t *)(Caster() + 0x7C);
		if (n->counter < 0x3E) shown &= 0xFFFFFF7Fu;
		else shown |= 0x80;
		k = (int32_t)n->counter - 0x20;
		if ((uint32_t)k < 4)
		{
			uint8_t *h = AllocHeader(0xB4);
			if (k == 0)
			{
				GetEffectSpawnPosition(Target(), 0xF1, 0, n->hit);
				ApplyActionResultToTarget(Ctx()->actions[0].targets);
			}
			// 30 fps layer: see mag012_blade_shot_held.inc
			FX_HELD(held_note_h(K_HIT, n, h, 0xB4, k);)
			SeqDraw(n->hit, h, SEQ_Hit, (int16_t)k);
			FieldFree(0xB4);
		}
		k = (int32_t)n->counter - 0x20;
		if ((uint32_t)k < 5)
		{
			uint8_t *h = AllocHeader(0xB4);
			if (k == 0) memcpy(n->hit, n->blade, 8);
			// 30 fps layer: see mag012_blade_shot_held.inc
			FX_HELD(held_note_h(K_FLASH, n, h, 0xB4, k);)
			SeqDraw(n->hit, h, SEQ_Flash, (int16_t)k);
			FieldFree(0xB4);
		}
		// 30 fps layer: see mag012_blade_shot_held.inc
		FX_HELD(held_note_k(K_RING, n, (int32_t)n->counter - 0x20);)
		Ring(n, (int32_t)n->counter - 0x20);
		// 30 fps layer: see mag012_blade_shot_held.inc
		FX_HELD(held_note_k(K_RING, n, (int32_t)n->counter - 0x22);)
		Ring(n, (int32_t)n->counter - 0x22);
		k = (int32_t)n->counter - 0x20;
		if ((uint32_t)k < 0x14)
		{
			// 30 fps layer: see mag012_blade_shot_held.inc
			FX_HELD(held_note_k(K_WING_A, n, k);)
			WingPair(n, RingScale(k), MODEL_WingA, KEYS_WingA0, KEYS_WingA1, 0x71);
		}
		k = (int32_t)n->counter - 0x20;
		if ((uint32_t)k < 0x14)
		{
			// 30 fps layer: see mag012_blade_shot_held.inc
			FX_HELD(held_note_k(K_WING_B, n, k);)
			WingPair(n, RingScale(k), MODEL_WingB, KEYS_WingB0, KEYS_WingB1, 0x11C);
		}
		k = (int32_t)n->counter - 0x20;
		if ((uint32_t)k < 0xA) SpawnSparks(n);
		k = (int32_t)n->counter - 0x20;
		if ((uint32_t)k < 6 && !(k & 1)) SpawnFan(n, k, false);
		k = (int32_t)n->counter - 0x20;
		if ((uint32_t)k < 6 && !(k & 1)) SpawnFan(n, k, true);
		if (n->counter < 0x3E) SpawnTrails(n);
		k = (int32_t)n->counter - 0x3C;
		if ((uint32_t)k < 5)
		{
			uint8_t *h = AllocHeader(0xB4);
			int16_t pos[4]; // pos[3] is never written
			pos[0] = n->blade[0];
			pos[1] = (int16_t)(n->blade[1] + 0x64);
			pos[2] = n->blade[2];
			// 30 fps layer: see mag012_blade_shot_held.inc
			FX_HELD(held_note_h(K_CATCH, n, h, 0xB4, k);)
			SeqDraw(pos, h, SEQ_Hit, (int16_t)k);
			FieldFree(0xB4);
			if (k == 0) BdPlaySE3D(SOUND_Catch, 0, pos);
		}
		const int trails = ExecuteTaskQueue(QTrails());
		const int parts = ExecuteTaskQueue(QParts());
		if (n->counter >= 0x41 && trails == 0 && parts == 0) return TASK_END;
		n->counter++;
		return 0;
	}
}

	void register_mag012_blade_shot()
	{
		register_port(blade012::ORIG_RootTask, (void *)blade012::RootTask, "B012 RootTask", 12);
		register_port(blade012::ORIG_PartTask, (void *)blade012::PartTask, "B012 PartTask", 12);
		register_port(blade012::ORIG_StreakTask, (void *)blade012::StreakTask, "B012 StreakTask", 12);
		register_port(blade012::ORIG_TrailTask, (void *)blade012::TrailTask, "B012 TrailTask", 12);
		FX_HELD(register_mag012_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag012_blade_shot_held.inc"
#endif
