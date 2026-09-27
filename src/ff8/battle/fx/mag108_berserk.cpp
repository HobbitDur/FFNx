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

// Effect 108: Berserk (spell, MAG_108_*).
//
// Structure (setup MAG_108_BERSERK 0x6CFCD0 -> _Init 0x6CFD00, file loader 0x6CFCE0 = the texture
// file named at 0x12EC34C (mag107.tim); the setup starts camera animation 0x12EB9D4 and queues the
// TIM):
//   RootTask (0x6CFD70) - alternates the packet arena (magic buffer + 0x554C / + 0x1D54C); on
//     counter 1 of a 10-tick cycle sets up the pools (first time) and starts the main task of the
//     next action on its first target, unless a main task of 43 ticks or less still runs on that
//     target (the cycle then restarts); runs the main, swirl and particle queues, ends when all
//     three are empty.
//   Main (0x6CFF60, "MainVisual_Tick") - node 0x8C4, one per action (spawn anchor = effect bone
//     0xF1 at the target's height, effect anchor = GetDefaultEffectPosition): over 0..3 a swirl
//     task per tick; a flash sprite sequence (0x12E6DDC) over 17..22; the glow prim model
//     0x12E7010 over 17..31 (uniform scale 0x400 + sin, fade rising); three embers per tick over
//     17..31 around the spawn anchor, a spark every other tick over 9..16 around the effect anchor
//     (random); over 10..17 eight light rays per tick (0x6D0B40) in the node's ray table
//     (32 x 0x30); a screen fade at 17; the burst prim-model layout 0x12E7BC0 with the callback
//     BurstPart (0x6D03D0) over 17..52; every tick the live rays are drawn and updated (0x6D0600);
//     sound at 0, the status at 43, ends from 53 once no ray is left.
//   Swirl (0x6D0C20) - node 0x420: a textured ribbon of up to 32 positions around the effect anchor
//     (0x6D0DC0 push, 0x6D0E50 draw: Gouraud-shaded textured quads between consecutive positions,
//     each between two texture-window packets); over its ticks 0..6 four positions per tick, the
//     position's brightness rises then falls; ends after tick 8 once fewer than two positions are
//     left.
//   Particle (0x6D1250) - node 0x1C: a sprite sequence in place, frame = its counter; the embers
//     (0x12E6E98) rise, decelerating (14 frames), the sparks (0x12E6C48) stay (15 frames).
// Direct calls into other spell modules (ported here): MAG_103_sub_6D67B0 (dword copy),
// MAG_011_sub_6CF070 (z rotation matrix), 0x6D9CF0 (Blizzaga's ribbon point).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2521AC0..0x2521B30 (texture file, magic buffer base, context, root pool,
// packet cursor 0x2521AE4, particle / swirl / main / root queues; 0x2521B28 / 0x2521B2C are handed
// to the ray spawner, which never reads them). The main pool (3 x 0x8C4), the swirl pool
// (12 x 0x420, + 0x1A4C), the particle pool (64 x 0x1C, + 0x4BCC), the morph vertex buffer
// (+ 0x52CC) and the packet arenas are in the magic buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace berserk108
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexBase() { return var<uint32_t>(0x2521AC4); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2521AC8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2521AE4); }
	inline TaskQueue *QParticles() { return (TaskQueue *)0x2521AE8; }       // pool: + 0x4BCC, 0x40 x 0x1C
	inline TaskQueue *QSwirls() { return (TaskQueue *)0x2521AF8; }          // pool: + 0x1A4C, 12 x 0x420
	inline TaskQueue *QMains() { return (TaskQueue *)0x2521B08; }           // pool: magic buffer, 3 x 0x8C4
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6CFD70;
	static const uint32_t ORIG_MainTask = 0x6CFF60;
	static const uint32_t ORIG_SwirlTask = 0x6D0C20;
	static const uint32_t ORIG_ParticleTask = 0x6D1250;
	static const uint32_t MODEL_Burst = 0x12E7BC0;       // prim-model layout data (0x29C-byte layout)
	static const uint32_t MODEL_Glow = 0x12E7010;        // prim model
	static const uint32_t PART_Flags = 0x12EC330;        // per-object flags of the burst layout
	static const uint32_t SEQ_Flash = 0x12E6DDC;
	static const uint32_t SEQ_Ember = 0x12E6E98;
	static const uint32_t SEQ_Spark = 0x12E6C48;
	static const uint32_t RAY_Colours = 0x12EC320;       // 4 dwords: centre colour, its fade, edge colour, its fade
	static const uint32_t RAY_Unused = 0x2521B28;        // 2 dwords handed to the ray spawner, not read
	static const uint32_t TEXWIN_Strip = 0x12EC310;      // RECT of the swirl's texture window
	static const uint32_t TEXWIN_Reset = 0x12EC318;      // RECT restored after each quad
	static const void *const SOUND_Berserk = (const void *)0x12EC30C;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline void MatrixMultiply3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }    // sub_56C220: a = a * b (3x3)
	inline void BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { fn<void (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(int32_t *dst) { fn<void (__cdecl *)(int32_t *)>(0x45E3C0)(dst); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..9, a main task spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	// a light ray (0x30 bytes): a Gouraud triangle + quad fan from the effect anchor outwards
	struct Ray
	{
		int16_t pad00;     // +0x00 (never written)
		int16_t tilt;      // +0x02 random, fixed
		int16_t spin;      // +0x04 (rand & 0x7FF) - 0x400, += speed every tick
		int16_t alive;     // +0x06
		uint8_t col0[4];   // +0x08 centre colour (3 bytes fade)
		uint8_t dec0[4];   // +0x0C its fade per tick
		uint8_t col1[4];   // +0x10 middle colour
		uint8_t dec1[4];   // +0x14
		uint8_t col2[4];   // +0x18 tip colour (never set by the spawner: 0 from the node clear)
		uint8_t dec2[4];   // +0x1C
		int16_t speed;     // +0x20 spin step, +-0x64
		int16_t length;    // +0x22 (draw: >> 3)
		int16_t length_vel; // +0x24
		int16_t length_acc; // +0x26
		int16_t width;     // +0x28 (draw: >> 3)
		int16_t width_vel; // +0x2A
		int16_t width_acc; // +0x2C
		int16_t pad2E;
	};
	struct MainNode // pool of 3 nodes of 0x8C4 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E action index
		uint8_t pad10[4];
		uint8_t *entity;   // +0x14 target entity
		int16_t spawn[4];  // +0x18 GetEffectSpawnPosition(bone 0xF1) x, (entity height), z, +0x1E
		int16_t pos[4];    // +0x20 effect anchor x, y, z, height (GetDefaultEffectPosition)
		Ray rays[32];      // +0x28
		uint8_t burst[0x29C]; // +0x628 prim-model layout (prim::Layout: data, frame, state)
	};
	// a swirl position (0x20 bytes)
	struct SwirlEntry
	{
		int16_t angle;     // +0x00 around the anchor, waving (+ sin(phase) >> 6 per tick)
		int16_t z;         // +0x02 depth (effect frame)
		int16_t radius;    // +0x04
		int16_t phase;     // +0x06 wave phase, + 0x200 per tick
		uint32_t sxy0;     // +0x08 projections of the two edge points
		int32_t otz0;      // +0x0C (compared as int16)
		uint32_t sxy1;     // +0x10
		int32_t otz1;      // +0x14
		int16_t bright;    // +0x18 0x10..0xFF
		int16_t bright_vel; // +0x1A
		uint16_t shade;    // +0x1C texture u offset
		uint16_t pad1E;
	};
	struct SwirlNode // pool of 12 nodes of 0x420 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t angle;     // +0x10 angle of the next position, + 0x20 per position
		int16_t pad12;     // +0x12 (set to 0)
		int16_t pad14;
		int16_t amp;       // +0x16 radius wave amplitude (0x800)
		int32_t count;     // +0x18 positions
		struct MainNode *owner; // +0x1C
		SwirlEntry trail[32]; // +0x20
	};
	struct ParticleNode // pool of 0x40 nodes of 0x1C bytes
	{
		TaskNode hdr;
		uint8_t counter;   // +0x0C sequence frame
		int8_t accel;      // +0x0D ember: vertical acceleration
		int16_t scale;     // +0x0E
		int16_t pos[3];    // +0x10
		int16_t vel;       // +0x16 ember: vertical speed
		uint32_t seq;      // +0x18 sprite sequence data
	};
	// the prim-player callback's parameter block (a stack block of the main task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 spawn anchor x, y, z, +0x1E word
		uint32_t flags;    // +0x08 per-object flags table
		uint32_t morph;    // +0x0C blended vertex frames
	};
	// the swirl's stack frame (0x6D0C20 esp+0x10..): the basis's translation is the anchor
	struct SwirlFrame
	{
		int32_t v[3];      // +0x10 -x, -y, z of the anchor (camera space)
		uint32_t pad1C;
		Mat4x3 basis;      // +0x20 (translation +0x34 = the anchor in camera space)
		Mat4x3 roll;       // +0x40 (3x3 only)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(Ray) == 0x30 && sizeof(MainNode) == 0x8C4, "Berserk nodes");
	static_assert(sizeof(SwirlEntry) == 0x20 && sizeof(SwirlNode) == 0x420 && sizeof(ParticleNode) == 0x1C, "Berserk nodes");
	static_assert(sizeof(PrimArg) == 0x10 && sizeof(SwirlFrame) == 0x50, "Berserk blocks");

	// unsigned x / 15 (mul 0x88888889, shr 3)
	static inline uint32_t Div15(uint32_t x) { return (uint32_t)(((uint64_t)x * 0x88888889u) >> 35); }

	// ------------------------------------------------------------------
	// Functions of other spell modules the module calls directly
	// ------------------------------------------------------------------
	// MAG_103_sub_6D67B0: copy n dwords
	static void CopyDwords(void *dst, const void *src, int32_t n)
	{
		uint32_t *d = (uint32_t *)dst;
		const uint32_t *s = (const uint32_t *)src;
		for (; n; n--) *d++ = *s++;
	}

	// MAG_011_sub_6CF070: rotation about z (3x3 only: pad and translation not written)
	static void ZRotation(int16_t angle, Mat4x3 *m)
	{
		const int32_t s = ComputeSin(angle);
		const int32_t c = ComputeCos(angle);
		m->m[0][1] = (int16_t)-s;
		m->m[1][0] = (int16_t)s;
		m->m[0][0] = (int16_t)c;
		m->m[0][2] = 0;
		m->m[1][1] = (int16_t)c;
		m->m[1][2] = 0;
		m->m[2][0] = 0;
		m->m[2][1] = 0;
		m->m[2][2] = 0x1000;
	}

	// 0x6D9CF0 (Blizzaga's ribbon point): x, y on a circle of radius r / 4, z = z / 4 - z
	static void SwirlPoint(int16_t *out, int32_t angle, int32_t z, int32_t r)
	{
		out[0] = (int16_t)(mul32(ComputeCos(angle), r) >> 14);
		out[1] = (int16_t)(mul32(ComputeSin(angle), r) >> 14);
		out[2] = (int16_t)((z >> 2) - z);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6D03D0): one object of the burst layout
	// ------------------------------------------------------------------
	static void __cdecl BurstPart(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
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
		// the matrix's translation is the record offset + the block position (int32)
		Mat4x3 m = {};
		RotationFromAngles(r->rot, &m);
		m.t[0] = (int32_t)r->pos[0] + arg->pos[0];
		m.t[1] = (int32_t)r->pos[1] + arg->pos[1];
		m.t[2] = (int32_t)r->pos[2] + arg->pos[2];
		const uint32_t *flags = (const uint32_t *)arg->flags;
		if (flags[(int16_t)r->index] & 0x10000000)
		{
			// camera-aligned: only the position goes through the camera
			TransformVectorBy3x3Matrix(&Camera(), m.t, m.t);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		}
		else ComposeAffineTransform(&Camera(), &m, &m);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t mode = (flags[(int16_t)r->index] & 0xDFFF) | 0x2000;
		*(uint32_t *)(h + 0x1C) = mode;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = mode | 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Light rays (0x6D0B40 spawn, 0x6D0600 draw + update, 0x6D0870 prims, 0x6D0AE0 direction)
	// ------------------------------------------------------------------
	// 0x6D0B40: n new rays in free slots (the scan restarts from the first slot for each; stops
	// when the table is full). p8 / p9 are never read.
	static void RaySpawn(int32_t max, Ray *rays, int32_t n, const uint32_t *c0, const uint32_t *d0, const uint32_t *c1, const uint32_t *d1,
		const void *p8, const void *p9, int32_t speed, int32_t length, int32_t length_vel, int32_t length_acc, int32_t width, int32_t width_vel, int32_t width_acc)
	{
		(void)p8;
		(void)p9;
		if (n <= 0) return;
		int32_t made = 0;
		for (;;)
		{
			Ray *e = rays;
			int32_t left = max;
			for (;;)
			{
				if (left-- == 0) return;
				if (e->alive == 0) break;
				e++;
			}
			e->alive = 1;
			e->tilt = (int16_t)CrtRand();
			e->spin = (int16_t)((CrtRand() & 0x7FF) - 0x400);
			if (CrtRand() & 1) e->speed = (int16_t)speed;
			else e->speed = (int16_t)-speed;
			made++;
			*(uint32_t *)e->col0 = *c0;
			*(uint32_t *)e->dec0 = *d0;
			*(uint32_t *)e->col1 = *c1;
			e->length = (int16_t)length;
			e->width = (int16_t)width;
			*(uint32_t *)e->dec1 = *d1;
			e->length_vel = (int16_t)length_vel;
			e->length_acc = (int16_t)length_acc;
			e->width_vel = (int16_t)width_vel;
			e->width_acc = (int16_t)width_acc;
			if (made >= n) return;
		}
	}

	// 0x6D0AE0: unit direction of a ray (tilt from the -z axis, spin around it)
	static void RayDirection(const Ray *e, int16_t *out)
	{
		const int32_t s = ComputeSin(e->tilt);
		const int32_t c = ComputeCos(e->tilt);
		out[2] = (int16_t)-c;
		out[1] = (int16_t)(mul32(ComputeSin(e->spin), s) >> 12);
		out[0] = (int16_t)((int32_t)(0u - (uint32_t)mul32(ComputeCos(e->spin), s)) >> 12);
	}

	// 0x6D0870: the ray from `base` to `length` along its direction, `width` wide at the tip
	// (perpendicular on screen, scaled by the tip's depth): a Gouraud triangle (centre colour to
	// half way) and a Gouraud quad (half way to the tip), both semi-transparent
	static uint32_t RayPrims(const Ray *e, int32_t width, int32_t base, int32_t length, const uint8_t *c0, const uint8_t *c1, const uint8_t *c2,
		uint32_t ot, int32_t shift, uint32_t cursor)
	{
		int16_t d[3];
		RayDirection(e, d);
		// the 4th word is never written by the original (stack; it only reaches the VZ0 high half)
		int16_t v[4];
		v[3] = 0;
		v[0] = (int16_t)(mul32(d[0], base) >> 12);
		v[1] = (int16_t)(mul32(d[1], base) >> 12);
		v[2] = (int16_t)(mul32(d[2], base) >> 12);
		GteLoadV0(v);
		GteRTPS();
		uint8_t *p = (uint8_t *)cursor;
		GteReadSXY2(p + 8);
		int32_t otz1;
		GteReadOTZ(&otz1);
		if (otz1 <= 0) return cursor;
		v[0] = (int16_t)(mul32(d[0], length) >> 12);
		v[1] = (int16_t)(mul32(d[1], length) >> 12);
		v[2] = (int16_t)(mul32(d[2], length) >> 12);
		GteLoadV0(v);
		GteRTPS();
		GteReadSXY2(p + 0x10);
		int32_t otz2;
		GteReadOTZ(&otz2);
		if (otz2 <= 0) return cursor;
		const int32_t sx1 = *(const int16_t *)(p + 8);
		const int32_t sy2 = *(const int16_t *)(p + 0x12);
		const int32_t dx = *(const int16_t *)(p + 0x10) - sx1;
		const int32_t dy = *(const int16_t *)(p + 0xA) - sy2;
		const int32_t len = Sqrt(mul32(dy, dy) + mul32(dx, dx));
		if (len == 0) return cursor;
		uint8_t *q = p + 0x1C;
		int32_t px = shl32(dy, 12) / len;
		px = mul32(px, width) / otz2;
		int32_t py = shl32(dx, 12) / len;
		px >>= 4;
		py = mul32(py, width) / otz2;
		// tip +- the perpendicular
		const int16_t tx = *(const int16_t *)(p + 0x10);
		*(int16_t *)(q + 0x18) = (int16_t)(tx + (int16_t)px);
		*(int16_t *)(q + 0x20) = (int16_t)(tx - (int16_t)px);
		py >>= 4;
		const int16_t ty = *(const int16_t *)(p + 0x12);
		*(int16_t *)(q + 0x1A) = (int16_t)(ty + (int16_t)py);
		*(int16_t *)(q + 0x22) = (int16_t)(ty - (int16_t)py);
		// half way between the centre and the two tip corners
		int16_t h = (int16_t)(((int32_t)*(const int16_t *)(q + 0x18) + *(const int16_t *)(p + 8)) >> 1);
		*(int16_t *)(q + 8) = h;
		*(int16_t *)(p + 0x10) = h;
		h = (int16_t)(((int32_t)*(const int16_t *)(q + 0x20) + *(const int16_t *)(p + 8)) >> 1);
		*(int16_t *)(q + 0x10) = h;
		*(int16_t *)(p + 0x18) = h;
		h = (int16_t)(((int32_t)*(const int16_t *)(q + 0x1A) + *(const int16_t *)(p + 0xA)) >> 1);
		*(int16_t *)(q + 0xA) = h;
		*(int16_t *)(p + 0x12) = h;
		h = (int16_t)(((int32_t)*(const int16_t *)(q + 0x22) + *(const int16_t *)(p + 0xA)) >> 1);
		*(int16_t *)(q + 0x12) = h;
		*(int16_t *)(p + 0x1A) = h;
		*(uint32_t *)(p + 4) = *(const uint32_t *)c0;
		const uint32_t k1 = *(const uint32_t *)c1;
		*(uint32_t *)(q + 0xC) = k1;
		*(uint32_t *)(q + 4) = k1;
		*(uint32_t *)(p + 0x14) = k1;
		*(uint32_t *)(p + 0xC) = k1;
		const uint32_t k2 = *(const uint32_t *)c2;
		*(uint32_t *)(q + 0x1C) = k2;
		*(uint32_t *)(q + 0x14) = k2;
		*(uint32_t *)p = 0x6000000;
		p[7] = 0x32;
		*(uint32_t *)q = 0x8000000;
		q[7] = 0x3A;
		const int32_t z = ((otz1 + otz2) / 2) >> shift;
		const uint32_t bucket = ot + (uint32_t)z * 4;
		InsertPrimAutoDepth(bucket, p);
		InsertPrimAutoDepth(bucket, q);
		return (uint32_t)(q + 0x24);
	}

	// one byte of a ray colour down by its fade, not below 0
	static inline uint8_t FadeByte(uint8_t c, uint8_t d) { const int32_t v = (int32_t)c - d; return (uint8_t)(v < 0 ? 0 : v); }

	// the update part of 0x6D0600: spin, colour fades, length / width motion; dead when every colour
	// is 0 or the length or the width is no longer positive
	static void RayUpdate(Ray *e)
	{
		e->spin = (int16_t)(e->spin + e->speed);
		e->col0[0] = FadeByte(e->col0[0], e->dec0[0]);
		e->col0[1] = FadeByte(e->col0[1], e->dec0[1]);
		e->col0[2] = FadeByte(e->col0[2], e->dec0[2]);
		e->col1[0] = FadeByte(e->col1[0], e->dec1[0]);
		e->col1[1] = FadeByte(e->col1[1], e->dec1[1]);
		e->col1[2] = FadeByte(e->col1[2], e->dec1[2]);
		e->col2[0] = FadeByte(e->col2[0], e->dec2[0]);
		e->col2[1] = FadeByte(e->col2[1], e->dec2[1]);
		e->col2[2] = FadeByte(e->col2[2], e->dec2[2]);
		if (!e->col0[0] && !e->col0[1] && !e->col0[2] && !e->col1[0] && !e->col1[1] && !e->col1[2] && !e->col2[0] && !e->col2[1] && !e->col2[2])
		{
			e->alive = 0;
			return;
		}
		e->length_vel = (int16_t)(e->length_vel + e->length_acc);
		e->length = (int16_t)(e->length + e->length_vel);
		if (e->length <= 0)
		{
			e->alive = 0;
			return;
		}
		e->width_vel = (int16_t)(e->width_vel + e->width_acc);
		e->width = (int16_t)(e->width + e->width_vel);
		if (e->width <= 0) e->alive = 0;
	}

	// 0x6D0600: every live ray drawn around pos (the effect frame is set up at the first one:
	// camera rotation, pos in camera space), then updated; *alive = rays drawn
	static uint32_t RayDraw(int32_t max, Ray *rays, const int16_t *pos, int32_t *alive, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		int32_t drawn = 0;
		if (max == 0)
		{
			*alive = 0;
			return cursor;
		}
		Mat4x3 m;
		Ray *e = rays;
		for (int32_t k = max; k; k--, e++)
		{
			if (e->alive == 0) continue;
			if (drawn++ == 0)
			{
				CopyDwords(&m, &Camera(), 5);
				GteSetRotMatrixCtrl(&m);
				GteLoadV0(pos);
				GteMVMVA_RotV0();
				GteReadMAC123(m.t);
				m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
				m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
				m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
				GteSetRotMatrix(&m);
				GteSetTransVector(&m);
			}
			cursor = RayPrims(e, e->width >> 3, 0, e->length >> 3, e->col0, e->col1, e->col2, ot, shift, cursor);
			RayUpdate(e);
		}
		*alive = drawn;
		return cursor;
	}

	// ------------------------------------------------------------------
	// Swirl ribbon (0x6D0DC0 push, 0x6D0E50 draw)
	// ------------------------------------------------------------------
	// 0x6D0DC0: a new position in front (the others move back while fewer than max; at max the
	// front one is overwritten)
	static int32_t SwirlPush(int32_t max, int32_t n, SwirlEntry *t, int32_t angle, int32_t z, int32_t radius, int32_t phase, int32_t bright,
		int32_t bright_vel, int32_t shade)
	{
		if (n < max)
		{
			uint32_t *d = (uint32_t *)t;
			for (int32_t k = n * 8; k > 0; k--) d[k + 7] = d[k - 1];
			n++;
		}
		t[0].angle = (int16_t)angle;
		t[0].z = (int16_t)z;
		t[0].radius = (int16_t)radius;
		t[0].phase = (int16_t)phase;
		t[0].bright = (int16_t)bright;
		t[0].bright_vel = (int16_t)bright_vel;
		t[0].shade = (uint16_t)shade;
		return n;
	}

	// 0x6D0E50, first part: every position still at 0x10 or brighter steps its brightness
	// (speed + d, brightness + speed / 8, kept in 0x10..0xFF)
	static void SwirlFade(int32_t n, SwirlEntry *t, int32_t d)
	{
		for (int32_t k = 0; k < n; k++)
		{
			SwirlEntry &e = t[k];
			if (e.bright < 0x10) continue;
			e.bright_vel = (int16_t)(e.bright_vel + d);
			e.bright = (int16_t)(e.bright + (int16_t)(e.bright_vel >> 3));
			if (e.bright < 0x10) e.bright = 0x10;
			else if (e.bright > 0xFF) e.bright = 0xFF;
		}
	}

	// the texture-window command (GP0 0xE2) of a RECT {x, y, w, h}, exactly as computed
	static uint32_t TexWindow(uint32_t rect)
	{
		uint32_t c = (*(const uint8_t *)(rect + 2) & 0xF8) | 0xFFFE2000u;
		c = (c << 5) | (*(const uint32_t *)rect & 0xF8);
		c = (c << 5) | (~((uint32_t)*(const uint16_t *)(rect + 6) - 1) & 0xF8);
		c = (c << 2) | ((uint32_t)((int32_t)~(*(const uint32_t *)(rect + 4) - 1) >> 3) & 0x1F);
		return c;
	}

	// 0x6D0E50, rest: the last position is dropped when it and the one before are dark; every
	// position's two edge points are projected (angle -+ 0x80), then its angle waves; a textured
	// Gouraud quad between consecutive positions (brightness as colour, u = shade + position's
	// shade), between two texture-window packets, unless a corner is behind the camera
	static uint32_t SwirlStrip(int32_t *pn, SwirlEntry *t, int32_t shade, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		int32_t n = *pn;
		if (n < 2)
		{
			*pn = n;
			return cursor;
		}
		if (t[n - 1].bright <= 0x10 && t[n - 2].bright <= 0x10)
		{
			n--;
			if (n < 2)
			{
				*pn = n;
				return cursor;
			}
		}
		for (int32_t k = 0; k < n; k++)
		{
			SwirlEntry &e = t[k];
			// the 4th words are never written by the original (stack; they only reach the VZ0 high half)
			int16_t a[4], b[4];
			a[3] = 0;
			b[3] = 0;
			SwirlPoint(a, (int32_t)e.angle - 0x80, e.z, e.radius);
			GteLoadV0(a);
			GteRTPS();
			GteReadSXY2(&e.sxy0);
			GteReadOTZ(&e.otz0);
			SwirlPoint(b, (int32_t)e.angle + 0x80, e.z, e.radius);
			GteLoadV0(b);
			GteRTPS();
			GteReadSXY2(&e.sxy1);
			GteReadOTZ(&e.otz1);
			const int32_t s = ComputeSin(e.phase);
			e.phase = (int16_t)(e.phase + 0x200);
			e.angle = (int16_t)(e.angle + (int16_t)(s >> 6));
		}
		shift += 2;
		uint8_t *p = (uint8_t *)cursor;
		for (int32_t k = 0; k < n - 1; k++)
		{
			const SwirlEntry &a = t[k], &b = t[k + 1];
			if ((int16_t)a.otz0 <= 0 || (int16_t)a.otz1 <= 0 || (int16_t)b.otz0 <= 0 || (int16_t)b.otz1 <= 0) continue;
			uint8_t *q = p;
			p += 0x34;
			uint32_t u = (uint32_t)(shade + a.shade);
			uint32_t u2 = u + 0xF;
			if ((int32_t)(u2 | u) > 0xFF)
			{
				u -= 0x80;
				u2 -= 0x80;
			}
			*(uint32_t *)(q + 0x18) = u | 0xB9FF00;
			*(uint32_t *)(q + 0xC) = u | 0x3D94C000;
			*(uint32_t *)(q + 8) = a.sxy0;
			*(uint32_t *)(q + 0x14) = a.sxy1;
			int32_t mac1, mac2;
			GteSetSXY012(a.sxy0, a.sxy1, b.sxy0);
			GteNCLIP();
			GteReadMAC0(&mac1);
			GteSetSXY012(a.sxy1, b.sxy1, b.sxy0);
			GteNCLIP();
			GteReadMAC0(&mac2);
			const int32_t s1 = mac1 > 0, s2 = mac2 > 0;
			if (s1 == s2)
			{
				*(uint32_t *)(q + 0x20) = b.sxy0;
				*(uint32_t *)(q + 0x2C) = b.sxy1;
				*(uint32_t *)(q + 0x24) = u2 | 0xC000;
				u2 |= 0xFF00;
			}
			else
			{
				*(uint32_t *)(q + 0x20) = b.sxy1;
				*(uint32_t *)(q + 0x2C) = b.sxy0;
				*(uint32_t *)(q + 0x24) = u2 | 0xFF00;
				u2 |= 0xC000;
			}
			*(uint32_t *)(q + 0x30) = u2;
			uint32_t c = (uint32_t)(int32_t)a.bright;
			const uint32_t ca = (((c << 8) | c) << 8) | c;
			*(uint32_t *)(q + 0x10) = ca;
			*(uint32_t *)(q + 4) = ca;
			c = (uint32_t)(int32_t)b.bright;
			const uint32_t cb = (((c << 8) | c) << 8) | c;
			*(uint32_t *)q = 0xC000000;
			q[7] = 0x3E;
			*(uint32_t *)(q + 0x28) = cb;
			*(uint32_t *)(q + 0x1C) = cb;
			const int32_t z = ((int32_t)(int16_t)b.otz1 + (int16_t)b.otz0 + (int16_t)a.otz1 + (int16_t)a.otz0) >> shift;
			uint8_t *w = p;
			p += 0xC;
			*(uint32_t *)w = 0x2000000;
			const uint32_t bucket = ot + (uint32_t)z * 4;
			// (the original tests the RECT address, a constant, for null)
			*(uint32_t *)(w + 4) = TexWindow(TEXWIN_Strip);
			*(uint32_t *)(w + 8) = 0;
			InsertPrimAutoDepth(bucket, w);
			InsertPrimAutoDepth(bucket, q);
			w = p;
			p += 0xC;
			*(uint32_t *)w = 0x2000000;
			*(uint32_t *)(w + 4) = TexWindow(TEXWIN_Reset);
			*(uint32_t *)(w + 8) = 0;
			InsertPrimAutoDepth(bucket, w);
		}
		*pn = n;
		return (uint32_t)p;
	}

	// 0x6D0E50
	static uint32_t SwirlDraw(int32_t *pn, SwirlEntry *t, int32_t d, int32_t shade, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		SwirlFade(*pn, t, d);
		return SwirlStrip(pn, t, shade, ot, shift, cursor);
	}

	// the swirl's effect frame, first part: the effect anchor in camera space (basis translation)
	static void SwirlAnchor(const int16_t *pos, SwirlFrame *f)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(pos);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(f->basis.t);
	}

	// second part: a basis facing the camera from the anchor (times a z rotation by 0), loaded
	static void SwirlBasis(SwirlFrame *f)
	{
		f->v[0] = (int32_t)(0u - (uint32_t)f->basis.t[0]);
		f->v[1] = (int32_t)(0u - (uint32_t)f->basis.t[1]);
		f->v[2] = f->basis.t[2];
		BuildOrthonormalBasis(f->v, &f->basis);
		ZRotation(0, &f->roll);
		MatrixMultiply3(&f->basis, &f->roll);
		GteSetRotMatrix(&f->basis);
		GteSetTransVector(&f->basis);
	}

	// ------------------------------------------------------------------
	// Main task draws
	// ------------------------------------------------------------------
	// flash sprite sequence at the effect anchor
	static void FlashDraw(const int16_t *pos, int32_t e)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(pos, 0x2000, (int32_t)0xFFFFF800);
		*(uint32_t *)h = SEQ_Flash;
		*(int16_t *)(h + 4) = (int16_t)e;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// glow prim model: uniform scale 0x400 + sin((e << 10) / 15), fade (e << 11) / 15 + 0x800
	static inline int32_t GlowScale(int32_t e) { return ComputeSin((int32_t)Div15((uint32_t)shl32(e, 10))) + 0x400; }
	static inline int32_t GlowFade(int32_t e) { return (int32_t)(Div15((uint32_t)shl32(e, 11)) + 0x800); }

	static void GlowDraw(const int16_t *pos, int32_t scale, int32_t fade)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		// the matrix pad is never written by the original (stack)
		Mat4x3 m = {};
		m.m[0][0] = (int16_t)scale;
		m.m[1][1] = (int16_t)scale;
		m.m[2][2] = (int16_t)scale;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(pos);
		GteMVMVA_RotV0Tr();
		GteReadMAC123(m.t);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = MODEL_Glow;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(int32_t *)(h + 0xC) = fade;
		*(uint32_t *)(h + 0x1C) = 0xF0;
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Particle (0x6D1250): a sprite sequence, frame = its counter
	// ------------------------------------------------------------------
	static void ParticleDraw(const ParticleNode *p)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		// the original pushes the scale as eax with only its low word loaded: the high word is the
		// header address's (0x571BC0 reads the low word only)
		TransformCameraByShadowRotation(p->pos, (int32_t)(((uint32_t)h & 0xFFFF0000u) | (uint16_t)p->scale), -((int32_t)p->scale >> 2));
		*(uint32_t *)h = p->seq;
		*(int16_t *)(h + 4) = (int16_t)(int8_t)p->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag108_berserk_held.h"
#endif

namespace ff8fx
{
namespace berserk108
{
	static uint32_t __cdecl ParticleTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		// 30 fps layer: see mag108_berserk_held.inc
		FX_HELD(held_note_particle(p);)
		ParticleDraw(p);
		if (p->seq == SEQ_Ember)
		{
			p->vel = (int16_t)(p->vel + p->accel);
			p->pos[1] = (int16_t)(p->pos[1] + p->vel);
			p->counter++;
			return (int8_t)p->counter < 0xE ? 0 : TASK_END;
		}
		p->counter++;
		return (int8_t)p->counter < 0xF ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Swirl (0x6D0C20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SwirlTask(TaskNode *n)
	{
		SwirlNode *s = (SwirlNode *)n;
		SwirlFrame f = {};
		SwirlAnchor(s->owner->pos, &f);
		// over ticks 0..6 four positions per tick: position e = tick * 4 + i, w = 1 - e / 24
		for (int32_t i = 0; i < 4; i++)
		{
			if (s->counter > 6) continue;
			const int32_t e = i + s->counter * 4;
			const int32_t x = shl32(e, 12);
			int32_t q = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32) >> 2;
			q += (int32_t)((uint32_t)q >> 31);
			const int32_t w = 0x1000 - q;
			const int32_t z = mul32(f.basis.t[2], w) >> 12;
			const int32_t radius = (mul32(ComputeSin(w >> 1), s->amp) >> 12) + w;
			const int32_t shade = (7 - (e & 7)) << 4;
			const int32_t count = SwirlPush(0x20, s->count, s->trail, s->angle, z, radius, w, 0x10, 0x100, shade);
			s->angle = (int16_t)(s->angle + 0x20);
			s->count = count;
		}
		SwirlBasis(&f);
		// 30 fps layer: see mag108_berserk_held.inc
		FX_HELD(held_note_swirl(s);)
		PacketCursor() = SwirlDraw(&s->count, s->trail, -0x28, shl32(s->counter, 5), RenderOT(0x44), 2, PacketCursor());
		s->counter++;
		if (s->counter > 8 && s->count < 2) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Main (0x6CFF60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const int32_t c0 = m->counter;
		if ((uint32_t)c0 < 4)
		{
			SwirlNode *s = (SwirlNode *)AddTaskToQueue(QSwirls(), ORIG_SwirlTask);
			if (s)
			{
				Memset32(&s->counter, 0, 0x105);
				s->owner = m;
				s->angle = (int16_t)(shl32(c0, 10) + 0x200);
				s->pad12 = 0;
				s->amp = 0x800;
			}
		}
		int32_t e = m->counter - 0x11;
		if ((uint32_t)e < 6)
		{
			// 30 fps layer: see mag108_berserk_held.inc
			FX_HELD(held_note_flash(m, e);)
			FlashDraw(m->pos, e);
		}
		e = m->counter - 0x11;
		if ((uint32_t)e < 0xF)
		{
			// 30 fps layer: see mag108_berserk_held.inc
			FX_HELD(held_note_glow(m, e);)
			GlowDraw(m->pos, GlowScale(e), GlowFade(e));
		}
		// three embers per tick over 17..31 on a circle around the spawn anchor (random start
		// angle, a third of a turn apart, random distance, height and deceleration)
		e = m->counter - 0x11;
		if ((uint32_t)e < 0xF)
		{
			int32_t angle = CrtRand();
			for (int32_t k = 3; k; k--)
			{
				const int32_t dist = (CrtRand() & 0xFF) + 0x96;
				ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
				if (!p) continue;
				Memset32(&p->counter, 0, 4);
				p->seq = SEQ_Ember;
				p->scale = 0x800;
				p->pos[0] = (int16_t)((mul32(ComputeSin(angle), dist) >> 12) + m->spawn[0]);
				p->pos[2] = (int16_t)((mul32(ComputeCos(angle), dist) >> 12) + m->spawn[2]);
				const int32_t r1 = CrtRand();
				p->pos[1] = (int16_t)(m->spawn[1] - (int16_t)(r1 >> 5));
				const int32_t r2 = CrtRand();
				angle += 0x555;
				p->accel = (int8_t)(uint8_t)(0xFE - (r2 & 0xF));
			}
		}
		// a spark every other tick over 9..16 around the effect anchor
		e = m->counter - 9;
		if ((uint32_t)e < 8 && !(e & 1))
		{
			ParticleNode *p = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_ParticleTask);
			if (p)
			{
				Memset32(&p->counter, 0, 4);
				p->seq = SEQ_Spark;
				p->scale = 0x1000;
				const int32_t r1 = CrtRand();
				p->pos[0] = (int16_t)((r1 & 0x1FF) + m->pos[0] - 0x100);
				const int32_t r2 = CrtRand();
				p->pos[1] = (int16_t)((r2 & 0x1FF) + m->pos[1] - 0x100);
				const int32_t r3 = CrtRand();
				p->pos[2] = (int16_t)((r3 & 0x1FF) + m->pos[2] - 0x100);
			}
		}
		if ((uint32_t)(m->counter - 0xA) < 8)
		{
			const uint32_t *col = (const uint32_t *)RAY_Colours;
			RaySpawn(0x20, m->rays, 8, col, col + 1, col + 2, col + 3, (const void *)RAY_Unused, (const void *)(RAY_Unused + 4),
				0x64, 0xA0, 0xB40, -0x20, 0xA0, 0x40, -0x14);
		}
		if (m->counter == 0x11) ScreenFadeTask(0, 1, 1, 0x80);
		if ((uint32_t)(m->counter - 0x11) < 0x24)
		{
			PrimArg arg;
			*(uint32_t *)&arg.pos[0] = *(const uint32_t *)&m->spawn[0];
			*(uint32_t *)&arg.pos[2] = *(const uint32_t *)&m->spawn[2];
			arg.flags = PART_Flags;
			arg.morph = TexBase() + 0x52CC;
			// 30 fps layer: see mag108_berserk_held.inc
			FX_HELD(held_note_play(m, &arg);)
			prim::play((prim::Layout *)m->burst, BurstPart, (int)&arg, 0);
		}
		int32_t alive;
		// 30 fps layer: see mag108_berserk_held.inc
		FX_HELD(held_note_rays(m);)
		PacketCursor() = RayDraw(0x20, m->rays, m->pos, &alive, RenderOT(0x44), 2, PacketCursor());
		if (m->counter == 0)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(m->entity, pos);
			BdPlaySE3D(SOUND_Berserk, 0x101, pos);
		}
		if (m->counter == 0x2B) ApplyActionResultToTarget(Ctx()->actions[m->action].targets);
		if (m->counter >= 0x35 && alive == 0) return TASK_END;
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x6CFD70)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag108_berserk_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x554C;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x1D54C;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QMains(), (void *)TexBase(), 0x8C4, 3);
				InitTaskQueuePool(QSwirls(), (void *)(TexBase() + 0x1A4C), 0x420, 0xC);
				InitTaskQueuePool(QParticles(), (void *)(TexBase() + 0x4BCC), 0x1C, 0x40);
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				// wait while a main task of 43 ticks or less runs on this target
				MainNode *pool = (MainNode *)TexBase();
				bool busy = false;
				for (int k = 0; k < 3; k++)
					if ((*(const uint8_t *)&pool[k].hdr.flags & 1) && pool[k].entity == entity && pool[k].counter <= 0x2B)
					{
						busy = true;
						break;
					}
				if (busy) r->counter = 0;
				else
				{
					MainNode *m = (MainNode *)AddTaskToQueue(QMains(), ORIG_MainTask);
					Memset32(&m->counter, 0, 0x22E);
					m->action = (int16_t)(uint16_t)r->action;
					m->entity = entity;
					GetEffectSpawnPosition(entity, 0xF1, 0, m->spawn);
					m->spawn[1] = *(const int16_t *)(entity + 0x24); // the entity's height
					GetDefaultEffectPosition(m->entity, m->pos);
					DecodeModelPrimLayout(MODEL_Burst, m->burst, 0x29C);
					r->action++;
				}
			}
		}
		int a, b, c;
		if (r->started)
		{
			a = ExecuteTaskQueue(QMains());
			b = ExecuteTaskQueue(QSwirls());
			c = ExecuteTaskQueue(QParticles());
		}
		else
		{
			a = (int)n;
			b = (int)n;
			c = (int)n;
		}
		if (r->started && a == 0 && b == 0 && c == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 0;
		return 0;
	}
}

	void register_mag108_berserk()
	{
		register_port(berserk108::ORIG_RootTask, (void *)berserk108::RootTask, "B108 RootTask", 108);
		register_port(berserk108::ORIG_MainTask, (void *)berserk108::MainTask, "B108 MainTask", 108);
		register_port(berserk108::ORIG_SwirlTask, (void *)berserk108::SwirlTask, "B108 SwirlTask", 108);
		register_port(berserk108::ORIG_ParticleTask, (void *)berserk108::ParticleTask, "B108 ParticleTask", 108);
		// 30 fps layer: see mag108_berserk_held.inc
		FX_HELD(register_mag108_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag108_berserk_held.inc"
#endif
