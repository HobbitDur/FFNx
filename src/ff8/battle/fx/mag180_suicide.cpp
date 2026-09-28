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

// Effect 180: Suicide (enemy attack 62 of kernel.bin, the Bomb's self-destruct; MAG_180_*).
//
// Structure (setup MAG_180_SUICIDE_Init 0x5CC840, file loader 0x5CC820 = the texture file named at
// 0xD445BC (mag179.tim); the setup keeps the cast context and the CASTER's slot, sets up the root
// queue 0x2307DB0 (1 node) and the effect queue 0x2308720 (0x64 nodes of 0x24 bytes), clears the
// particle pool, starts camera animation 0xD413B8 and queues the TIM):
//   RootTask (0x5CDED0) - alternates the packet arena (magic buffer / + 0xC000) on the counter's
//     parity, runs the effect queue, ends when it is empty. No draw-only test.
//   Director (0x5CC900) - one node, counter 0..0x32 (nothing in the draw-only mode): 1 = the
//     fuse sparks task and sound 0xD445B4, 0x0F = the caster's default effect position and the
//     explosion task, 0x10 = the caster glow, 0x1B = the second ring + the sparks task, 0x1C = two
//     rings (random spin / speed) + the smoke task, 0x26 = damage (action 0's targets).
//   CasterGlow (0x5CCB60) - counters 0..0x17: draws the caster's battle model itself (entity
//     flags |= 4) with every polygon followed by an additive flat copy whose colour is 0xC0C0C0
//     faded in by IR0 = counter << 8 (full from 0x10); after counter 0x18 the caster is hidden
//     (flags |= 0x10). Model draw = shadow + a copy of the battle model renderer (0x5CCD50 /
//     0x5CCEA0) that adds the flat overlay polygons; the second model (+0x78) too.
//   Explosion (0x5CD1C0) - particles of type 1 (prim model 0xD4353C stretched along its
//     velocity, 6 ticks, shrinking 1/16 per tick, fading from tick 2), 6 spawned per tick 0..4 at
//     the effect position flying outwards, decelerating 1/4 per tick.
//   Ring (0x5CD560) / Ring2 (0x5CD6C0) - prim models 0xD417D4 / 0xD4380C at the effect position,
//     spun about y (Ring only, slowing 1/16 per tick) and growing (speed - speed / 7 per tick),
//     fading from tick 8 / 10; end at 0x10 / 0x12.
//   Sparks (0x5CD800) - particles of type 2, flipbook 0xD41674, 7 per tick 0..7 flying outwards.
//   Smoke (0x5CDAC0) - particles of type 4, flipbook 0xD414C8, 2 per tick 0..0xD around the
//     effect position, fixed.
//   FuseSparks (0x5CDC80) - particles of type 8, flipbook 0xD41674 randomly mirrored, 3 per tick
//     0..0xC at random effect bones of the caster.
// Particle pool 0x2307DC0: 0x64 entries of 0x18 bytes shared by the four particle tasks (type bits
// 1 / 2 / 4 / 8); each task draws then updates its own entries (in the draw-only mode it only
// draws) and spawns into free entries; the particle tasks end at counter >= 4 once no entry of
// their type is left.
// Module globals: 0x2307D98..0x2309554 (target slot, root pool / queue, particle pool, effect
// queue / pool, context 0x2309540, caster slot 0x2309544, texture file, magic buffer base,
// packet cursor 0x2309550).

#include "mag_common.h"

namespace ff8fx
{
namespace suicide180
{
	using namespace eng;
	using namespace magc;

	// raw memory access (the model renderer follows the listing offset by offset)
	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x2309540); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(0x2309544); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x230954C); }          // Magic_TextureOFF (magic buffer)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2309550); }
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x2308720; }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }       // frame packet arena cursor
	inline uint32_t &BoneWorkspace() { return var<uint32_t>(0x1D98B3C); }     // transformed model vertices
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }
	static const uint32_t POOL = 0x2307DC0, POOL_END = 0x2308720;             // 0x64 particles of 0x18 bytes

	static const uint32_t ORIG_RootTask = 0x5CDED0;
	static const uint32_t ORIG_Director = 0x5CC900;
	static const uint32_t ORIG_CasterGlow = 0x5CCB60;
	static const uint32_t ORIG_Explosion = 0x5CD1C0;
	static const uint32_t ORIG_Ring = 0x5CD560;
	static const uint32_t ORIG_Ring2 = 0x5CD6C0;
	static const uint32_t ORIG_Sparks = 0x5CD800;
	static const uint32_t ORIG_Smoke = 0x5CDAC0;
	static const uint32_t ORIG_FuseSparks = 0x5CDC80;
	static const uint32_t MODEL_Explosion = 0xD4353C, MODEL_Ring = 0xD417D4, MODEL_Ring2 = 0xD4380C;
	static const uint32_t SEQ_Spark = 0xD41674, SEQ_Smoke = 0xD414C8;
	static const void *const SOUND_Fuse = (const void *)0xD445B4;

	// ---- engine functions (original addresses)
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void GteLoadRGBC(void *c) { fn<void (__cdecl *)(void *)>(0x45E110)(c); }          // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }         // set_param_with_dword_1CA8A68
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void GteReadSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
	inline void GteMVMVA_RotV0Tr2() { fn<void (__cdecl *)()>(0x460830)(); }
	inline void InsertPrim(uint32_t bucket, uint32_t pkt) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C7A0)(bucket, pkt); }
	// GTE helpers of the Tornado module
	inline void GteLoadV0Words(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x64DEE0)(v); }    // MAG_146_sub_64DEE0: V0 = 3 words
	inline void GteStoreIRVertex(uint32_t out) { fn<void (__cdecl *)(uint32_t)>(0x64DF10)(out); } // MAG_146_sub_64DF10: IR1..3 -> 4 words (4th 0)
	// battle model helpers
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	// GetEffectSpawnPosition (0x502170): position of an entity's effect bone
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }

	// ------------------------------------------------------------------
	// Node / particle layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x10 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad;
	};
	struct EffectNode // pool of 0x64 nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t counter;     // +0x0C
		int16_t zero;        // +0x0E (rings: 0)
		int16_t pos[4];      // +0x10 effect position (x, y, z, pad)
		int16_t spin;        // +0x18 ring y rotation
		int16_t spin_speed;  // +0x1A
		int16_t scale;       // +0x1C ring scale
		int16_t scale_speed; // +0x1E
		uint32_t pad20;
	};
	struct Particle // 0x18 bytes
	{
		uint32_t type;       // +0x00 1 explosion, 2 sparks, 4 smoke, 8 fuse sparks (0 = free)
		int16_t frame;       // +0x04 age / flipbook frame
		int16_t scale;       // +0x06
		int16_t pos[4];      // +0x08 x, y, z, pad (copied with the effect position)
		int16_t vel[3];      // +0x10 (fuse sparks: mirror x / y)
		int16_t pad16;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(EffectNode) == 0x24, "Suicide nodes");
	static_assert(sizeof(Particle) == 0x18, "Suicide particle");

	inline Particle *Pool() { return (Particle *)POOL; }
	static const int POOL_COUNT = 0x64;
	inline uint8_t *Caster() { return Entity((int)CasterSlot()); }

	// a free pool entry (the scan stops at the pool's end), or -1
	static int FreeParticle()
	{
		int i = 0;
		for (uint32_t p = POOL; ; p += 0x18, i++)
		{
			if (U32(p, 0) == 0) break;
			if (p + 0x18 >= POOL_END) return -1;
		}
		if (i >= 0x64) return -1;
		return i;
	}

	// ------------------------------------------------------------------
	// Caster model renderer (copies of the battle model renderer)
	// ------------------------------------------------------------------
	// MAG_180_sub_5CCEA0: the polygons of one object (h: +0 primitives, +4 vertices, +8 / +0A
	// triangle / quad counts, +1C colour / mode bits, +2C MAC0, +34 OTZ, +38 FLAG, +5C overlay
	// colour); every visible textured polygon is followed by a flat semi-transparent copy one
	// bucket nearer
	static uint32_t RenderObjectPrims(uint32_t h, uint32_t ot, uint32_t shift, uint32_t pkt)
	{
		const uint32_t verts = U32(h, 4);
		uint32_t prim = U32(h, 0);
		for (int32_t i = 0; i < (int32_t)U16(h, 8); i++, prim += 0x10)
		{
			GteLoadV012(verts + (U16(prim, 0) & 0xFFF) * 8, verts + (U16(prim, 2) & 0xFFF) * 8, verts + (U16(prim, 4) & 0xFFF) * 8);
			GteRTPT();
			const uint32_t uv1 = U32(prim, 0xC);
			U32(pkt, 0x14) = uv1 & 0x1FFFFFF;
			U32(pkt, 0) = 0x7000000;
			U32(pkt, 4) = (uv1 & 0x2000000) | U32(h, 0x1C) | 0x24000000;
			U32(pkt, 0xC) = U32(prim, 8);
			GteReadFLAG(h + 0x38);
			if (U32(h, 0x38) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(h + 0x2C);
			if (S32(h, 0x2C) <= 0) continue;
			const uint32_t xy0 = pkt + 8, xy1 = pkt + 0x10, xy2 = pkt + 0x18;
			GteReadSXY012(xy0, xy1, xy2);
			GteAVSZ3();
			S32(pkt, 0x1C) = (int32_t)S16(prim, 6);
			GteReadOTZ32(h + 0x34);
			int32_t z = S32(h, 0x34) >> shift;
			InsertPrim(ot + (uint32_t)z * 4, pkt);
			if (z > 0) z--;
			pkt += 0x20;
			U32(pkt, 4) = U32(h, 0x5C) | 0x22000000;
			U32(pkt, 8) = U32(xy0, 0);
			U32(pkt, 0) = 0x4000000;
			U32(pkt, 0xC) = U32(xy1, 0);
			U32(pkt, 0x10) = U32(xy2, 0);
			InsertPrim(ot + (uint32_t)z * 4, pkt);
			pkt += 0x14;
		}
		if (U16(h, 0xA) != 0)
		{
			prim += 2;
			for (int32_t i = 0; i < (int32_t)U16(h, 0xA); i++, prim += 0x14)
			{
				GteLoadV012(verts + (U16(prim, -2) & 0xFFF) * 8, verts + (U16(prim, 0) & 0xFFF) * 8, verts + (U16(prim, 2) & 0xFFF) * 8);
				GteRTPT();
				const uint32_t uv1 = U32(prim, 0xA);
				U32(pkt, 0x14) = uv1 & 0x1FFFFFF;
				U32(pkt, 0) = 0x9000000;
				U32(pkt, 4) = (uv1 & 0x2000000) | U32(h, 0x1C) | 0x2C000000;
				U32(pkt, 0xC) = U32(prim, 6);
				GteReadFLAG(h + 0x38);
				if (U32(h, 0x38) & 0x60000) continue;
				GteNCLIP();
				GteReadMAC0(h + 0x2C);
				if (S32(h, 0x2C) <= 0) continue;
				const uint32_t xy0 = pkt + 8, xy1 = pkt + 0x10, xy2 = pkt + 0x18, xy3 = pkt + 0x20;
				GteReadSXY012(xy0, xy1, xy2);
				GteLoadV0((const void *)(verts + (U16(prim, 4) & 0xFFF) * 8));
				GteRTPS();
				const uint32_t uv23 = U32(prim, 0xE);
				U32(pkt, 0x1C) = uv23;
				S32(pkt, 0x24) = (int32_t)uv23 >> 16;
				GteReadSXY2((void *)xy3);
				GteAVSZ4();
				GteReadOTZ32(h + 0x34);
				int32_t z = S32(h, 0x34) >> shift;
				InsertPrim(ot + (uint32_t)z * 4, pkt);
				if (z > 0) z--;
				pkt += 0x28;
				U32(pkt, 4) = U32(h, 0x5C) | 0x2A000000;
				U32(pkt, 8) = U32(xy0, 0);
				U32(pkt, 0) = 0x5000000;
				U32(pkt, 0xC) = U32(xy1, 0);
				U32(pkt, 0x10) = U32(xy2, 0);
				U32(pkt, 0x14) = U32(xy3, 0);
				InsertPrim(ot + (uint32_t)z * 4, pkt);
				pkt += 0x18;
			}
		}
		return pkt;
	}

	// MAG_180_sub_5CCD50: one battle model (the objects of the visibility mask h +0x20): the
	// vertices of every bone group through the bone's matrix into h +4, then the polygons through
	// the camera
	static uint32_t RenderModel(uint32_t model, uint32_t h, uint32_t ot, uint32_t shift, uint32_t pkt)
	{
		const uint32_t bones = U32(model, 0) + 0x10;
		uint32_t table = U32(model, 4);
		const int32_t count = S32(table, 0);
		table += 4;
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t obj = U32(model, 4) + U32(table, 0);
			table += 4;
			if (!(U32(h, 0x20) & (1u << (i & 31)))) continue; // shl by cl (count mod 32)
			int32_t groups = S16(obj, 0);
			uint32_t out = U32(h, 4);
			obj += 2;
			for (; groups > 0; groups--)
			{
				const uint32_t bone = bones + (uint32_t)((int32_t)S16(obj, 0) * 0x30) + 0x10;
				obj += 2;
				GteSetRotMatrixCtrl((const Mat4x3 *)bone);
				GteSetTransVectorCtrl((const Mat4x3 *)bone);
				int32_t n = S16(obj, 0);
				obj += 2;
				for (; n > 0; n--)
				{
					GteLoadV0Words(obj);
					obj += 6;
					GteMVMVA_RotV0Tr2();
					GteStoreIRVertex(out);
					out += 8;
				}
			}
			const uint32_t a = (obj + 3) & 0xFFFFFFFC;
			U16(h, 8) = U16(a, 0);
			U16(h, 0xA) = U16(a, 2);
			U16(h, 0xC) = U16(a, 4);
			U16(h, 0xE) = U16(a, 6);
			U32(h, 0) = a + 0xC;
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			pkt = RenderObjectPrims(h, ot, shift, pkt);
		}
		return pkt;
	}

	// MAG_180_sub_5CCC40: the caster's shadow and battle model(s) with the overlay colour h +0x5C
	static void DrawCasterModel(uint32_t h, uint8_t *entity)
	{
		const uint32_t e = (uint32_t)entity;
		const uint32_t anim = e + 0x60;
		if (!(entity[0] & 0x20)) FrameCursor() = DrawShadow(entity, RenderOT(0x4064), 0x10, FrameCursor());
		const uint32_t world = e + 0x40;
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)world, (Mat4x3 *)(h + 0x60));
		ComputeBonesWorldMatrices((void *)anim, (void *)world);
		U32(h, 4) = BoneWorkspace();
		U32(h, 0x1C) = U32(e, 0x28);
		U16(h, 0x14) = 0;
		U16(h, 0x16) = 0;
		U32(h, 0x20) = U32(e, 0x7C);
		AT<uint8_t>(h, 0x2A) = entity[7];
		AT<uint8_t>(h, 0x29) = entity[7];
		AT<uint8_t>(h, 0x28) = entity[7];
		U32(h, 0x10) = var<uint32_t>(0x1D969A8);
		U16(h, 0x18) = 0x140;
		U16(h, 0x1A) = 0xD8;
		FrameCursor() = RenderModel(U32(anim, 4), h, RenderOT(0x44), 2, FrameCursor());
		BuildBoneMatricesFromPose((void *)anim);
		const uint32_t second = U32(e, 0x78);
		if (second && !(entity[1] & 2))
		{
			U32(h, 0x20) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices((void *)second, (void *)world);
			FrameCursor() = RenderModel(U32(second, 4), h, RenderOT(0x44), 2, FrameCursor());
			BuildBoneMatricesFromPose((void *)second);
		}
	}

	// the glow's IR0 at a counter (fade in over 16 ticks)
	static int32_t GlowLevel(int16_t counter) { return counter < 0x10 ? shl32(counter, 8) : 0x1000; }

	// the glow's draw (0x5CCB60 before its update): overlay colour 0xC0C0C0 * level, the caster model
	static void DrawGlow(uint8_t *entity, int32_t level)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x80);
		uint32_t *colour = (uint32_t *)(h + 0x5C);
		*colour = 0;
		GteSetFarColor(0xC0, 0xC0, 0xC0);
		GteLoadRGBC(colour);
		GteSetIR0(level);
		GteDPCS();
		GteStoreRGB2(colour);
		entity[0] |= 4;
		DrawCasterModel((uint32_t)h, entity);
		FieldFree(0x80);
	}

	// ------------------------------------------------------------------
	// Particle draws (one entry of the pool, with the task's draw header)
	// ------------------------------------------------------------------
	// explosion: prim model along the velocity (0x88 block m: +8 matrix, +0x28 camera copy,
	// +0x48 scale vector, +0x58 up (0, -0x1000, 0), +0x68 direction, +0x78 axis)
	static void DrawExplosionParticle(uint8_t *h, uint8_t *m, const Particle *p, int32_t fade)
	{
		const uint32_t b = (uint32_t)m;
		S32(b, 0x1C) = p->pos[0];
		S32(b, 0x20) = p->pos[1];
		S32(b, 0x24) = p->pos[2];
		int32_t *dir = (int32_t *)(m + 0x68);
		dir[0] = p->vel[0];
		dir[1] = p->vel[1];
		dir[2] = p->vel[2];
		NormalizeVector(dir, dir);
		int32_t *axis = (int32_t *)(m + 0x78);
		const int32_t angle = RotationBetweenVectors((const int32_t *)(m + 0x58), dir, axis);
		Mat4x3 *mat = (Mat4x3 *)(m + 8);
		BuildAxisAngleRotationMatrix(angle, mat, axis);
		S32(b, 0x50) = 0x1000;
		S32(b, 0x48) = 0x1000;
		S32(b, 0x4C) = p->scale;
		Scale3DMatrix(mat, (const int32_t *)(m + 0x48));
		ComposeAffineTransform((const Mat4x3 *)(m + 0x28), mat, mat);
		GteSetRotMatrix(mat);
		GteSetTransVector(mat);
		if (p->frame >= 2)
		{
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) |= 0xC0;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
	}

	// sparks / smoke: flipbook frame at the particle, its size through the shadow camera
	static void DrawSpriteParticle(uint8_t *h, const Particle *p)
	{
		*(int16_t *)(h + 4) = p->frame;
		TransformCameraByShadowRotation(p->pos, (uint16_t)p->scale, -((int32_t)p->scale >> 4));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
	}

	// fuse sparks: the same with the mirror words (+0x0C / +0x10 of the header)
	static void DrawFuseParticle(uint8_t *h, const Particle *p)
	{
		*(int16_t *)(h + 4) = p->frame;
		*(int32_t *)(h + 0xC) = p->vel[0];
		*(int32_t *)(h + 0x10) = p->vel[1];
		TransformCameraByShadowRotation(p->pos, (uint16_t)p->scale, -((int32_t)p->scale >> 4));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
	}

	// explosion particle's draw header (0x58 block): model, colour 0, mode 0x33
	static void ExplosionHeader(uint8_t *h)
	{
		*(uint32_t *)(h + 0) = MODEL_Explosion;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
	}
	// its 0x88 block: the camera copy and the up vector
	static void ExplosionBlock(uint8_t *m)
	{
		memcpy(m + 0x28, &Camera(), 0x20);
		*(int32_t *)(m + 0x58) = 0;
		*(int32_t *)(m + 0x5C) = -0x1000;
		*(int32_t *)(m + 0x60) = 0;
	}

	// ring: prim model spun about y, scaled, at the node's position; from counter fade_start the
	// header fades (fade = (counter - fade_start) << 9)
	static void DrawRing(const EffectNode *t, uint32_t model, int16_t fade_start, int32_t fade)
	{
		int16_t angles[4];
		angles[0] = 0;
		angles[1] = t->spin;
		angles[2] = 0;
		angles[3] = 0; // never written by the original (stack word)
		Mat4x3 m = {};
		ComposeZYXRotationMatrix(angles, &m);
		m.t[0] = t->pos[0];
		m.t[1] = t->pos[1];
		int32_t v[3];
		v[2] = t->scale;
		v[1] = t->scale;
		v[0] = t->scale;
		m.t[2] = t->pos[2];
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)(h + 0) = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (t->counter >= fade_start)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ring updates (pure)
	static void RingUpdate(EffectNode *t)
	{
		const int16_t ss = t->scale_speed;
		t->spin = (int16_t)(t->spin + t->spin_speed);
		t->spin_speed = (int16_t)(t->spin_speed - (int16_t)(t->spin_speed >> 4));
		t->scale = (int16_t)(t->scale + ss);
		t->scale_speed = (int16_t)(ss - (int32_t)ss / 7);
		t->counter++;
	}
	static void Ring2Update(EffectNode *t)
	{
		const int16_t ss = t->scale_speed;
		t->scale = (int16_t)(t->scale + ss);
		t->scale_speed = (int16_t)(ss - (int32_t)ss / 7);
		t->counter++;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag180_suicide_held.h"
#endif

namespace ff8fx
{
namespace suicide180
{
	// ------------------------------------------------------------------
	// Caster glow (0x5CCB60)
	// ------------------------------------------------------------------
	static uint32_t __cdecl CasterGlowTask(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		if (t->counter < 0x18)
		{
			// 30 fps layer: see mag180_suicide_held.inc
			FX_HELD(held_note_glow(t);)
			DrawGlow(Caster(), GlowLevel(t->counter));
		}
		if (DrawOnly()) return 0;
		t->counter++;
		if (t->counter <= 0x18) return 0;
		Caster()[0] |= 0x10;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Explosion (0x5CD1C0): particles of type 1
	// ------------------------------------------------------------------
	static uint32_t __cdecl ExplosionTask(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		// 30 fps layer: see mag180_suicide_held.inc
		FX_HELD(held_note_pool(ORIG_Explosion, t);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		uint8_t *m = (uint8_t *)FieldAlloc(0x88);
		ExplosionHeader(h);
		ExplosionBlock(m);
		int alive = 0;
		for (Particle *p = Pool(); (uint32_t)p < POOL_END; p++)
		{
			if (!(p->type & 1)) continue;
			DrawExplosionParticle(h, m, p, shl32(p->frame - 2, 10));
			if (DrawOnly()) continue;
			p->frame++;
			if (p->frame >= 6)
			{
				p->type = 0;
				continue;
			}
			p->scale = (int16_t)(p->scale - (int16_t)(p->scale >> 4));
			p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
			p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
			p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
			p->vel[0] = (int16_t)(p->vel[0] - (int16_t)(p->vel[0] >> 2));
			p->vel[1] = (int16_t)(p->vel[1] - (int16_t)(p->vel[1] >> 2));
			p->vel[2] = (int16_t)(p->vel[2] - (int16_t)(p->vel[2] >> 2));
			alive++;
		}
		FieldFree(0x88);
		FieldFree(0x58);
		if (DrawOnly()) return 0;
		if (t->counter >= 0 && t->counter <= 4)
		{
			for (int k = 0; k < 6; k++)
			{
				const int i = FreeParticle();
				if (i < 0) break;
				Particle *p = &Pool()[i];
				p->type = 1;
				p->frame = 0;
				p->scale = (int16_t)(CrtRand() % 0x600 + 0xE00);
				memcpy(p->pos, t->pos, 8);
				int32_t v[3];
				v[0] = CrtRand() % 0x1000 - 0x800;
				v[1] = CrtRand() % 0x1000 - 0x800;
				v[2] = CrtRand() % 0x1000 - 0x800;
				NormalizeVector(v, v);
				const int32_t d = CrtRand() % 0x7D0 + 0x4B0;
				const int32_t dx = mul32(d, v[0]), dy = mul32(d, v[1]), dz = mul32(d, v[2]);
				p->pos[0] = (int16_t)(p->pos[0] + (dx >> 12));
				p->pos[1] = (int16_t)(p->pos[1] + (dy >> 12));
				p->pos[2] = (int16_t)(p->pos[2] + (dz >> 12));
				const int32_t s = CrtRand() % 0xF0 + 0x190;
				p->vel[0] = (int16_t)((int32_t)(0u - (uint32_t)mul32(s, v[0])) >> 12);
				p->vel[1] = (int16_t)((int32_t)(0u - (uint32_t)mul32(s, v[1])) >> 12);
				p->vel[2] = (int16_t)((int32_t)(0u - (uint32_t)mul32(s, v[2])) >> 12);
			}
		}
		t->counter++;
		if (t->counter >= 6 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Rings (0x5CD560 / 0x5CD6C0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		// 30 fps layer: see mag180_suicide_held.inc
		FX_HELD(held_note_ring(ORIG_Ring, t);)
		DrawRing(t, MODEL_Ring, 8, shl32(t->counter - 8, 9));
		if (DrawOnly()) return 0;
		RingUpdate(t);
		return t->counter >= 0x10 ? TASK_END : 0;
	}

	static uint32_t __cdecl Ring2Task(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		// 30 fps layer: see mag180_suicide_held.inc
		FX_HELD(held_note_ring(ORIG_Ring2, t);)
		DrawRing(t, MODEL_Ring2, 0xA, shl32(t->counter - 0xA, 9));
		if (DrawOnly()) return 0;
		Ring2Update(t);
		return t->counter >= 0x12 ? TASK_END : 0;
	}

	// sprite particle header (0xB4 block): sequence, +0x24 = 0
	static uint8_t *SpriteHeader(uint32_t seq)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)(h + 0) = seq;
		*(int16_t *)(h + 0x24) = 0;
		return h;
	}

	// ------------------------------------------------------------------
	// Sparks (0x5CD800): particles of type 2
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparksTask(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		// 30 fps layer: see mag180_suicide_held.inc
		FX_HELD(held_note_pool(ORIG_Sparks, t);)
		uint8_t *h = SpriteHeader(SEQ_Spark);
		int alive = 0;
		for (Particle *p = Pool(); (uint32_t)p < POOL_END; p++)
		{
			if (!(p->type & 2)) continue;
			DrawSpriteParticle(h, p);
			if (DrawOnly()) continue;
			p->frame++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				p->type = 0;
				continue;
			}
			p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
			p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
			p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
			p->vel[0] = (int16_t)(p->vel[0] - (int16_t)(p->vel[0] >> 2));
			p->vel[1] = (int16_t)(p->vel[1] - (int16_t)(p->vel[1] >> 2));
			p->vel[2] = (int16_t)(p->vel[2] - (int16_t)(p->vel[2] >> 2));
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (t->counter >= 0 && t->counter <= 7)
		{
			for (int k = 0; k < 7; k++)
			{
				const int i = FreeParticle();
				if (i < 0) break;
				Particle *p = &Pool()[i];
				p->type = 2;
				p->frame = 0;
				p->scale = (int16_t)(CrtRand() % 0x1000 + 0x800);
				memcpy(p->pos, t->pos, 8);
				int32_t v[3];
				v[0] = CrtRand() % 0x1000 - 0x800;
				v[1] = CrtRand() % 0x1000 - 0x800;
				v[2] = CrtRand() % 0x1000 - 0x800;
				NormalizeVector(v, v);
				const int32_t d = CrtRand() % 0x258 + 0x190;
				const int32_t dx = mul32(d, v[0]), dy = mul32(d, v[1]), dz = mul32(d, v[2]);
				p->pos[0] = (int16_t)(p->pos[0] + (dx >> 12));
				p->pos[1] = (int16_t)(p->pos[1] + (dy >> 12));
				p->pos[2] = (int16_t)(p->pos[2] + (dz >> 12));
				const int32_t s = CrtRand() % 0xFA + 0x122;
				p->vel[0] = (int16_t)(mul32(s, v[0]) >> 12);
				p->vel[1] = (int16_t)(mul32(s, v[1]) >> 12);
				p->vel[2] = (int16_t)(mul32(s, v[2]) >> 12);
			}
		}
		t->counter++;
		if (t->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Smoke (0x5CDAC0): particles of type 4
	// ------------------------------------------------------------------
	static uint32_t __cdecl SmokeTask(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		// 30 fps layer: see mag180_suicide_held.inc
		FX_HELD(held_note_pool(ORIG_Smoke, t);)
		uint8_t *h = SpriteHeader(SEQ_Smoke);
		int alive = 0;
		for (Particle *p = Pool(); (uint32_t)p < POOL_END; p++)
		{
			if (!(p->type & 4)) continue;
			DrawSpriteParticle(h, p);
			if (DrawOnly()) continue;
			p->frame++;
			if (*(int16_t *)(h + 0x28) < 0) p->type = 0;
			else alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (t->counter >= 0 && t->counter <= 0xD)
		{
			for (int k = 0; k < 2; k++)
			{
				const int i = FreeParticle();
				if (i < 0) break;
				Particle *p = &Pool()[i];
				p->type = 4;
				p->frame = 0;
				p->scale = (int16_t)(CrtRand() % 0xF00 + 0xA00);
				memcpy(p->pos, t->pos, 8);
				p->pos[0] = (int16_t)(p->pos[0] + (CrtRand() % 0x4B0 - 0x258));
				p->pos[1] = (int16_t)(p->pos[1] + (CrtRand() % 0x4B0 - 0x258));
				p->pos[2] = (int16_t)(p->pos[2] + (CrtRand() % 0x4B0 - 0x258));
			}
		}
		t->counter++;
		if (t->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Fuse sparks (0x5CDC80): particles of type 8
	// ------------------------------------------------------------------
	static uint32_t __cdecl FuseSparksTask(TaskNode *n)
	{
		EffectNode *t = (EffectNode *)n;
		// 30 fps layer: see mag180_suicide_held.inc
		FX_HELD(held_note_pool(ORIG_FuseSparks, t);)
		uint8_t *h = SpriteHeader(SEQ_Spark);
		*(int32_t *)(h + 0x14) = 0x1000;
		int alive = 0;
		for (Particle *p = Pool(); (uint32_t)p < POOL_END; p++)
		{
			if (!(p->type & 8)) continue;
			DrawFuseParticle(h, p);
			if (DrawOnly()) continue;
			p->frame++;
			if (*(int16_t *)(h + 0x28) < 0) p->type = 0;
			else alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (t->counter >= 0 && t->counter <= 0xC)
		{
			// number of effect bones of the caster's model
			const int32_t bones = **(const uint8_t *const *)*(const uint32_t *)(Caster() + 0x64);
			for (int k = 0; k < 3; k++)
			{
				const int i = FreeParticle();
				if (i < 0) break;
				Particle *p = &Pool()[i];
				p->type = 8;
				p->frame = 0;
				const int32_t r = CrtRand() % 0x500;
				p->scale = (int16_t)(r + (int32_t)(uint16_t)(t->counter + 0xA0) * 8);
				const int32_t flag = CrtRand() % 0x1000;
				const int32_t bone = CrtRand() % bones;
				GetEffectSpawnPosition(Caster(), bone, flag, p->pos);
				p->pos[0] = (int16_t)(p->pos[0] + (CrtRand() % 0xC8 - 0x64));
				p->pos[1] = (int16_t)(p->pos[1] + (CrtRand() % 0xC8 - 0x64));
				p->pos[2] = (int16_t)(p->pos[2] + (CrtRand() % 0xC8 - 0x64));
				p->vel[0] = (p->pos[0] & 1) ? 0x1000 : -0x1000;
				p->vel[1] = (p->pos[1] & 1) ? 0x1000 : -0x1000;
			}
		}
		t->counter++;
		if (t->counter >= 4 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5CC900)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		if (DrawOnly()) return 0;
		EffectNode *t = (EffectNode *)n;
		if (t->counter == 0x10)
		{
			EffectNode *g = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_CasterGlow);
			g->counter = 0;
		}
		if (t->counter == 0xF)
		{
			GetDefaultEffectPosition(Caster(), t->pos);
			EffectNode *e = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_Explosion);
			memcpy(e->pos, t->pos, 8);
			e->counter = 0;
		}
		if (t->counter == 0x1C)
		{
			EffectNode *r = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_Ring);
			memcpy(r->pos, t->pos, 8);
			r->counter = 0;
			r->zero = 0;
			r->spin = (int16_t)(CrtRand() % 0x1000);
			const int32_t s = CrtRand() % 0x6E;
			r->scale_speed = 0x1CC;
			r->scale = 0x1CC;
			r->spin_speed = (int16_t)(s + 0x3C);
			r = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_Ring);
			memcpy(r->pos, t->pos, 8);
			r->counter = 0;
			r->zero = 0;
			r->spin = (int16_t)(CrtRand() % 0x1000);
			const int32_t s2 = CrtRand() % 0x82;
			r->spin_speed = (int16_t)(-0x50 - s2);
			r->scale_speed = 0x366;
			r->scale = 0x366;
		}
		if (t->counter == 0x1B)
		{
			EffectNode *r = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_Ring2);
			memcpy(r->pos, t->pos, 8);
			r->counter = 0;
			r->spin = (int16_t)(CrtRand() % 0x1000);
			r->scale_speed = 0x580;
			r->scale = 0x580;
			EffectNode *s = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_Sparks);
			memcpy(s->pos, t->pos, 8);
			s->counter = 0;
		}
		if (t->counter == 0x1C)
		{
			EffectNode *s = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_Smoke);
			memcpy(s->pos, t->pos, 8);
			s->counter = 0;
		}
		if (t->counter == 1)
		{
			EffectNode *f = (EffectNode *)AddTaskToQueue(EffectQueue(), ORIG_FuseSparks);
			f->counter = 0;
		}
		if (t->counter == 0x26) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (t->counter == 1) BdPlaySE(SOUND_Fuse, 0, 0x80);
		t->counter++;
		return t->counter > 0x32 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x5CDED0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag180_suicide_held.inc
		FX_HELD(held_note_root();)
		if (r->counter & 1) PacketCursor() = TexBase() + 0xC000;
		else PacketCursor() = TexBase();
		const int a = ExecuteTaskQueue(EffectQueue());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag180_suicide()
	{
		register_port(suicide180::ORIG_RootTask, (void *)suicide180::RootTask, "S180 RootTask", 180);
		register_port(suicide180::ORIG_Director, (void *)suicide180::DirectorTask, "S180 Director", 180);
		register_port(suicide180::ORIG_CasterGlow, (void *)suicide180::CasterGlowTask, "S180 CasterGlow", 180);
		register_port(suicide180::ORIG_Explosion, (void *)suicide180::ExplosionTask, "S180 Explosion", 180);
		register_port(suicide180::ORIG_Ring, (void *)suicide180::RingTask, "S180 Ring", 180);
		register_port(suicide180::ORIG_Ring2, (void *)suicide180::Ring2Task, "S180 Ring2", 180);
		register_port(suicide180::ORIG_Sparks, (void *)suicide180::SparksTask, "S180 Sparks", 180);
		register_port(suicide180::ORIG_Smoke, (void *)suicide180::SmokeTask, "S180 Smoke", 180);
		register_port(suicide180::ORIG_FuseSparks, (void *)suicide180::FuseSparksTask, "S180 FuseSparks", 180);
		// 30 fps layer: see mag180_suicide_held.inc
		FX_HELD(register_mag180_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag180_suicide_held.inc"
#endif
