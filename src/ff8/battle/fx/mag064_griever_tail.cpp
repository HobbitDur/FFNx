/****************************************************************************/
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

#include "mag_common.h"

namespace ff8fx
{
namespace gtail064
{
	using namespace eng;
	using namespace magc;

	inline uint8_t *&Caster() { return var<uint8_t *>(0x2545020); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x254508C); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2545098); }
	inline uint32_t &FrameArena() { return var<uint32_t>(0x1D8E054); }
	inline TaskQueue *QSprites() { return (TaskQueue *)0x2545028; }
	inline TaskQueue *QLayouts() { return (TaskQueue *)0x2545038; }
	inline TaskQueue *QScripts() { return (TaskQueue *)0x2545048; }
	inline TaskQueue *QMaster() { return (TaskQueue *)0x2545058; }
	inline uint32_t OT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x6E5260;
	static const uint32_t ORIG_MasterTask = 0x6E53F0;
	static const uint32_t ORIG_ScriptTask = 0x6E5DD0;
	static const uint32_t ORIG_LayoutTask = 0x6E6310;
	static const uint32_t ORIG_SpriteTask = 0x6E67F0;
	static const uint32_t LAYOUTS = 0x13473C8;
	static const uint32_t SEQUENCES = 0x13433C0;
	static const void *const SOUND_Voice = (const void *)0x1342EF8;
	static const void *const SOUND_Start = (const void *)0x1342EF4;
	static const uint32_t SCRIPTS[18] = {
		0x134B560, 0x134B7FC, 0x134B880, 0x134B904, 0x134B988, 0x134BA0C, 0x134BA90, 0x134BB14, 0x134BB98,
		0x134BC1C, 0x134BCA0, 0x134BD04, 0x134BD68, 0x134BD9C, 0x134BE00, 0x134BE64, 0x134BEC8, 0x134BF2C };

	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline void MatMulScaleDiag(Mat4x3 *a, const int16_t *b) { fn<void (__cdecl *)(Mat4x3 *, const int16_t *)>(0x56C220)(a, b); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); }
	inline uint32_t FadeTile(int32_t t, int32_t r, int32_t g, int32_t b, int32_t in, int32_t hold, int32_t out, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x700150)(t, r, g, b, in, hold, out, cursor); }
	inline void PoseBones(uint8_t *anim_header, const void *pose) { fn<void (__cdecl *)(uint8_t *, const void *)>(0x662C00)(anim_header, pose); }
	inline void ProjectGroup(const uint8_t **data, uint8_t **out, uint8_t *h, int32_t key) { fn<void (__cdecl *)(const uint8_t **, uint8_t **, uint8_t *, int32_t)>(0x676E00)(data, out, h, key); }
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, shift, cursor); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void ReleaseVoice(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteSetSZ123(int32_t a, int32_t b, int32_t c) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x45E1D0)(a, b, c, d); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }

#pragma pack(push, 1)
	struct PoseEntry
	{
		int16_t pos[3];
		int16_t vis;
		int16_t rot[3];
		int16_t pad;
	};
	struct RootNode
	{
		TaskNode hdr;
		int16_t counter;
		uint8_t pad0E;
		uint8_t started;
		uint32_t parity;
	};
	struct MasterNode
	{
		TaskNode hdr;
		int16_t counter;
		int16_t pad0E;
		uint32_t voice;
		uint32_t pad14;
		PoseEntry pose[71];
	};
	struct ScriptNode
	{
		TaskNode hdr;
		uint8_t *entity;
		PoseEntry *pose;
		PoseEntry *cur;
		const int16_t *ip;
		int16_t vel[3];
		int16_t pad22;
		int16_t avel[3];
		int16_t pad2A;
	};
	struct LayoutNode
	{
		TaskNode hdr;
		int16_t pos[3];
		int16_t scale;
		int16_t turn;
		int16_t depth;
		int16_t counter;
		int16_t pad1A;
		uint32_t objs;
		uint8_t layout[0x578];
	};
	struct SpriteNode
	{
		TaskNode hdr;
		int16_t pos[3];
		int16_t size;
		uint8_t frame;
		uint8_t seq;
		int8_t vel[3];
		int8_t acc[2];
		int8_t pad1B;
	};
	struct PrimArg
	{
		Mat4x3 m;
		int32_t scale[3];
		int32_t scaled;
		uint32_t objs;
		int16_t depth;
		int16_t scroll;
		uint32_t pos[2];
		uint32_t morph;
	};
#pragma pack(pop)
	static_assert(sizeof(PoseEntry) == 0x10 && sizeof(RootNode) == 0x14 && sizeof(MasterNode) == 0x488, "Tail 064 nodes");
	static_assert(sizeof(ScriptNode) == 0x2C && sizeof(LayoutNode) == 0x598 && sizeof(SpriteNode) == 0x1C, "Tail 064 nodes");
	static_assert(sizeof(PrimArg) == 0x44, "Tail 064 prim block");
}
}

#ifdef FF8_FX_HELD
#include "mag064_griever_tail_held.h"
#endif

namespace ff8fx
{
namespace gtail064
{
	using namespace eng;
	using namespace magc;

	static uint32_t DrawPolys(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *verts = *(const uint8_t *const *)(h + 4);
		const uint8_t *p = *(const uint8_t *const *)h;
		int16_t c = *(int16_t *)(h + 8);
		*(int16_t *)(h + 8) = (int16_t)(c - 1);
		while (c != 0)
		{
			if (!(p[0xF] & 0x80))
			{
				const uint32_t i0 = *(const uint16_t *)p & 0xFFF, i1 = *(const uint16_t *)(p + 2) & 0xFFF, i2 = *(const uint16_t *)(p + 4) & 0xFFF;
				*(uint32_t *)(h + 0x1C) = *(const uint32_t *)(verts + i0 * 8);
				*(uint32_t *)(h + 0x20) = *(const uint32_t *)(verts + i0 * 8 + 4);
				*(uint32_t *)(h + 0x24) = *(const uint32_t *)(verts + i1 * 8);
				*(uint32_t *)(h + 0x28) = *(const uint32_t *)(verts + i1 * 8 + 4);
				*(uint32_t *)(h + 0x2C) = *(const uint32_t *)(verts + i2 * 8);
				*(uint32_t *)(h + 0x30) = *(const uint32_t *)(verts + i2 * 8 + 4);
				const uint8_t f0 = h[0x22], f1 = h[0x2A], f2 = h[0x32];
				if ((uint8_t)(f0 | f1 | f2) < 0x10 && !(f2 & (uint8_t)(f0 & f1)) && h[0x23] == h[0x2B] && h[0x23] == h[0x33])
				{
					GteSetSXY012(*(const uint32_t *)(h + 0x1C), *(const uint32_t *)(h + 0x24), *(const uint32_t *)(h + 0x2C));
					GteNCLIP();
					GteReadMAC0(h + 0x3C);
					if (*(const int32_t *)(h + 0x3C) >= 0)
					{
						GteSetSZ123(*(const int16_t *)(h + 0x20), *(const int16_t *)(h + 0x28), *(const int16_t *)(h + 0x30));
						GteAVSZ3();
						*(uint32_t *)pk = 0x7000000;
						*(uint32_t *)(pk + 4) = ((uint32_t)((*(const uint16_t *)(p + 0xE) & 0x200) | 0x2400) << 16) | *(const uint32_t *)(h + 0x14);
						*(uint32_t *)(pk + 8) = *(const uint32_t *)(h + 0x1C);
						*(uint32_t *)(pk + 0x10) = *(const uint32_t *)(h + 0x24);
						*(uint32_t *)(pk + 0x18) = *(const uint32_t *)(h + 0x2C);
						*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 8);
						*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 0xC);
						*(uint16_t *)(pk + 0x16) &= 0xFDFF;
						*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 6);
						GteReadOTZWord(h + 0x44);
						InsertPrimAutoDepth(ot + (uint32_t)(*(const int32_t *)(h + 0x44) >> (shift & 31)) * 4, pk);
						pk += 0x20;
					}
				}
			}
			p += 0x10;
			c = *(int16_t *)(h + 8);
			*(int16_t *)(h + 8) = (int16_t)(c - 1);
		}
		c = *(int16_t *)(h + 0xA);
		*(int16_t *)(h + 0xA) = (int16_t)(c - 1);
		while (c != 0)
		{
			if (!(p[0xF] & 0x80))
			{
				const uint32_t i0 = *(const uint16_t *)p & 0xFFF, i1 = *(const uint16_t *)(p + 2) & 0xFFF;
				const uint32_t i2 = *(const uint16_t *)(p + 4) & 0xFFF, i3 = *(const uint16_t *)(p + 6) & 0xFFF;
				*(uint32_t *)(h + 0x1C) = *(const uint32_t *)(verts + i0 * 8);
				*(uint32_t *)(h + 0x20) = *(const uint32_t *)(verts + i0 * 8 + 4);
				*(uint32_t *)(h + 0x24) = *(const uint32_t *)(verts + i1 * 8);
				*(uint32_t *)(h + 0x28) = *(const uint32_t *)(verts + i1 * 8 + 4);
				*(uint32_t *)(h + 0x2C) = *(const uint32_t *)(verts + i2 * 8);
				*(uint32_t *)(h + 0x30) = *(const uint32_t *)(verts + i2 * 8 + 4);
				*(uint32_t *)(h + 0x34) = *(const uint32_t *)(verts + i3 * 8);
				*(uint32_t *)(h + 0x38) = *(const uint32_t *)(verts + i3 * 8 + 4);
				const uint8_t f0 = h[0x22], f1 = h[0x2A], f2 = h[0x32], f3 = h[0x3A];
				if ((uint8_t)(f3 | f0 | f1 | f2) < 0x10 && !(f2 & (uint8_t)(f3 & f0 & f1)) && h[0x23] == h[0x2B] && h[0x23] == h[0x33] && h[0x23] == h[0x3B])
				{
					GteSetSXY012(*(const uint32_t *)(h + 0x1C), *(const uint32_t *)(h + 0x24), *(const uint32_t *)(h + 0x2C));
					GteNCLIP();
					GteReadMAC0(h + 0x3C);
					if (*(const int32_t *)(h + 0x3C) >= 0)
					{
						GteSetSZ0123(*(const int16_t *)(h + 0x20), *(const int16_t *)(h + 0x28), *(const int16_t *)(h + 0x30), *(const int16_t *)(h + 0x38));
						GteAVSZ4();
						*(uint32_t *)pk = 0x9000000;
						*(uint32_t *)(pk + 4) = ((uint32_t)((*(const uint16_t *)(p + 0xE) & 0x200) | 0x2C00) << 16) | *(const uint32_t *)(h + 0x14);
						*(uint32_t *)(pk + 8) = *(const uint32_t *)(h + 0x1C);
						*(uint32_t *)(pk + 0x10) = *(const uint32_t *)(h + 0x24);
						*(uint32_t *)(pk + 0x18) = *(const uint32_t *)(h + 0x2C);
						*(uint32_t *)(pk + 0x20) = *(const uint32_t *)(h + 0x34);
						*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 8);
						*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 0xC);
						*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 0x10);
						*(uint16_t *)(pk + 0x16) &= 0xFDFF;
						*(uint16_t *)(pk + 0x24) = *(const uint16_t *)(p + 0x12);
						GteReadOTZWord(h + 0x44);
						InsertPrimAutoDepth(ot + (uint32_t)(*(const int32_t *)(h + 0x44) >> (shift & 31)) * 4, pk);
						pk += 0x28;
					}
				}
			}
			p += 0x14;
			c = *(int16_t *)(h + 0xA);
			*(int16_t *)(h + 0xA) = (int16_t)(c - 1);
		}
		return (uint32_t)pk;
	}

	static uint32_t DrawModel(const uint8_t *model, uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t *objs = *(const uint32_t *const *)(model + 4);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const int32_t count = (int32_t)objs[0];
		if (count <= 0) return cursor;
		for (int32_t i = 0; i < count; i++)
		{
			const uint8_t *p = *(const uint8_t *const *)(model + 4) + objs[1 + i];
			if (!(*(const uint32_t *)(h + 0x18) & (1u << (i & 31)))) continue;
			uint8_t *out = *(uint8_t **)(h + 4);
			const int32_t groups = *(const int16_t *)p;
			p += 2;
			if (groups > 0)
			{
				for (int32_t g = groups; g != 0; g--)
				{
					const int32_t bone = *(const int16_t *)p;
					p += 2;
					const PoseEntry *pe = *(const PoseEntry *const *)(h + 0x4C) + bone;
					if (pe->vis < 0x80)
					{
						const Mat4x3 *bm = (const Mat4x3 *)(bones + mul32(bone, 0x30) + 0x10);
						GteSetRotMatrixCtrl(bm);
						GteSetTransVectorCtrl(bm);
						ProjectGroup(&p, &out, h, *(const uint8_t *)&pe->vis);
					}
					else
					{
						const int32_t n = *(const int16_t *)p;
						p = p + n * 6 + 2;
						for (int32_t k = n; k != 0; k--)
						{
							out[6] = 0x10;
							out += 8;
						}
					}
				}
			}
			p = (const uint8_t *)(((uint32_t)p + 3) & ~3u);
			*(uint16_t *)(h + 8) = *(const uint16_t *)p;
			*(uint16_t *)(h + 0xA) = *(const uint16_t *)(p + 2);
			*(const uint8_t **)h = p + 0xC;
			cursor = DrawPolys(h, ot, shift, cursor);
		}
		return cursor;
	}

	static void CasterDraw(uint8_t *entity, PoseEntry *pose)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x98);
		uint8_t *anim = entity + 0x60;
		FrameArena() = DrawShadow(entity, OT(0x4040), 0x10, FrameArena());
		PoseBones(anim, pose);
		uint8_t *skel = **(uint8_t ***)(entity + 0x64);
		if (skel[0] != 0)
		{
			for (int32_t i = 0; i < skel[0]; i++)
			{
				Mat4x3 *bm = (Mat4x3 *)(skel + 0x20 + i * 0x30);
				ComposeAffineTransform((const Mat4x3 *)(entity + 0x40), bm, bm);
				bm->t[0] = (int32_t)((uint32_t)bm->t[0] + (uint32_t)(int32_t)pose[i].pos[0]);
				bm->t[1] = (int32_t)((uint32_t)bm->t[1] + (uint32_t)(int32_t)pose[i].pos[1]);
				bm->t[2] = (int32_t)((uint32_t)bm->t[2] + (uint32_t)(int32_t)pose[i].pos[2]);
				ComposeAffineTransform(&Camera(), bm, bm);
			}
		}
		*(PoseEntry **)(h + 0x4C) = pose;
		*(uint32_t *)(h + 4) = var<uint32_t>(0x1D98B3C);
		*(uint16_t *)(h + 0xC) = 0;
		*(uint16_t *)(h + 0xE) = 0;
		*(uint16_t *)(h + 0x10) = 0x140;
		*(uint16_t *)(h + 0x12) = 0xD8;
		*(uint32_t *)(h + 0x14) = *(const uint32_t *)(entity + 0x28);
		*(uint32_t *)(h + 0x18) = *(const uint32_t *)(entity + 0x7C);
		FrameArena() = DrawModel(*(const uint8_t *const *)(anim + 4), h, OT(0x44), 0, FrameArena());
		PoseBones(anim, pose);
		FieldFree(0x98);
	}

	static void ShowPolys(uint8_t *p)
	{
		uint8_t *poly = *(uint8_t **)p;
		int16_t c = *(int16_t *)(p + 4);
		*(int16_t *)(p + 4) = (int16_t)(c - 1);
		while (c != 0)
		{
			*(uint16_t *)(poly + 0xE) &= 0x7FFF;
			poly += 0x10;
			c = *(int16_t *)(p + 4);
			*(int16_t *)(p + 4) = (int16_t)(c - 1);
		}
		c = *(int16_t *)(p + 6);
		*(int16_t *)(p + 6) = (int16_t)(c - 1);
		if (c != 0)
		{
			poly += 0xE;
			do
			{
				*(uint16_t *)poly &= 0x7FFF;
				c = *(int16_t *)(p + 6);
				poly += 0x14;
				*(int16_t *)(p + 6) = (int16_t)(c - 1);
			} while (c != 0);
		}
	}

	static void ShowModelPolys(const uint8_t *model, uint8_t *p)
	{
		const uint32_t *tab = *(const uint32_t *const *)(model + 4);
		const int32_t count = (int32_t)*tab++;
		if (count <= 0) return;
		for (int32_t n = count; n != 0; n--)
		{
			uint8_t *o = (uint8_t *)(*tab++ + *(const uint32_t *)(model + 4));
			const int32_t groups = *(const int16_t *)o;
			o += 2;
			if (groups > 0)
			{
				for (int32_t g = groups; g != 0; g--)
				{
					const int32_t nv = *(const int16_t *)(o + 2);
					o += 2;
					o = o + nv * 6 + 2;
				}
			}
			o = (uint8_t *)(((uint32_t)o + 3) & ~3u);
			*(uint16_t *)(p + 4) = *(const uint16_t *)o;
			o += 2;
			*(uint16_t *)(p + 6) = *(const uint16_t *)o;
			o += 0xA;
			*(uint8_t **)p = o;
			ShowPolys(p);
		}
	}

	static void ShowAllPolys(uint8_t *entity)
	{
		uint8_t *p = (uint8_t *)FieldAlloc(8);
		ShowModelPolys(*(const uint8_t *const *)(entity + 0x64), p);
		FieldFree(8);
	}

	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
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
		Mat4x3 m = {};
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		int16_t off[4];
		if (arg->scaled != 0)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
		}
		else
		{
			off[0] = r->pos[0];
			off[1] = r->pos[1];
			off[2] = r->pos[2];
		}
		off[3] = 0;
		if (arg->scaled != 0) Scale3DMatrix(&m, arg->scale);
		int32_t t0, t1, t2;
		if (r->flags & 0x200)
		{
			t0 = off[0];
			t1 = off[1];
			t2 = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
			t0 = m.t[0];
			t1 = m.t[1];
			t2 = m.t[2];
		}
		m.t[0] = (int32_t)((uint32_t)t0 + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)t1 + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)t2 + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				const int16_t s[9] = { r->scale[0], 0, 0, 0, r->scale[1], 0, 0, 0, r->scale[2] };
				MatMulScaleDiag(&m, s);
			}
			else
			{
				const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		*(uint16_t *)(h + 0x22) = 0;
		*(uint16_t *)(h + 0x20) = 0;
		*(uint16_t *)(h + 0x26) = 0;
		*(uint16_t *)(h + 0x24) = 0;
		*(uint16_t *)(h + 0x2A) = 0x100;
		*(uint16_t *)(h + 0x28) = 0x100;
		if (*(const uint8_t *)(arg->objs + object) == '1')
		{
			*(uint16_t *)(h + 0x2C) = 0;
			*(uint16_t *)(h + 0x2E) = 0x80;
			*(uint16_t *)(h + 0x32) = 0x80;
			*(uint16_t *)(h + 0x30) = 0x100;
			*(uint16_t *)(h + 0x22) = (uint16_t)(*(const uint8_t *)&arg->scroll & 0x7F);
		}
		PacketCursor() = RenderPrimSet(h, OT(0x44), 2, PacketCursor());
		FieldFree(0x6C);
	}

	static uint32_t __cdecl LayoutTask(TaskNode *n)
	{
		LayoutNode *L = (LayoutNode *)n;
		PrimArg arg;
		arg.morph = TexBase() + 0x26F20;
		RotY(L->turn, &arg.m);
		arg.m.t[0] = L->pos[0];
		arg.m.t[1] = L->pos[1];
		arg.m.t[2] = L->pos[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.scale[2] = L->scale;
		arg.scale[1] = L->scale;
		arg.scale[0] = L->scale;
		arg.depth = L->depth;
		arg.pos[0] = *(const uint32_t *)&L->pos[0];
		arg.objs = L->objs;
		arg.pos[1] = *(const uint32_t *)&L->pos[2];
		arg.scaled = 1;
		arg.scroll = (int16_t)(L->counter << 6);
		FX_HELD(held_note_play(L, &arg);)
		if (prim::play((prim::Layout *)L->layout, PartCallback, (int)&arg, 0) == 0) return TASK_END;
		L->counter++;
		return 0;
	}

	static void SpriteDraw(const SpriteNode *s)
	{
		TransformCameraByShadowRotation(s->pos, s->size, -0x400);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		const uint8_t *seq = *(const uint8_t *const *)(SEQUENCES + s->seq * 4);
		*(const uint8_t **)h = seq;
		*(uint16_t *)(h + 4) = s->frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, OT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SpriteTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		const int32_t frames = *(const int16_t *)(*(const uint8_t *const *)(SEQUENCES + s->seq * 4) + 8);
		FX_HELD(held_note_sprite(s);)
		SpriteDraw(s);
		const int8_t vx = s->vel[0];
		s->pos[0] = (int16_t)(s->pos[0] + (int16_t)(vx * 4));
		const int8_t vy = s->vel[1];
		s->pos[1] = (int16_t)(s->pos[1] + (int16_t)(vy * 4));
		s->pos[2] = (int16_t)(s->pos[2] + (int16_t)(s->vel[2] * 4));
		s->vel[0] = (int8_t)(s->acc[0] + vx);
		s->vel[1] = (int8_t)(s->acc[1] * 2 + vy);
		s->frame++;
		return s->frame < frames ? 0 : TASK_END;
	}

	static const int16_t *SpawnLayout(const int16_t *ip, uint8_t *entity, PoseEntry *pose, int32_t ground)
	{
		const int32_t bone = ip[0];
		const int32_t flag = ip[1];
		const int32_t layout = ip[2];
		const int32_t scale = ip[3];
		const int32_t turn = ip[4];
		const int32_t depth = ip[5];
		ip += 6;
		LayoutNode *L = (LayoutNode *)AddTaskToQueue(QLayouts(), ORIG_LayoutTask);
		if (!L) return ip;
		uint8_t *anim = entity + 0x60;
		PoseBones(anim, pose);
		GetEffectSpawnPosition(entity, bone, flag, L->pos);
		const PoseEntry *pe = pose + bone;
		L->pos[0] = (int16_t)(L->pos[0] + pe->pos[0]);
		if (ground != 0) L->pos[1] = 0;
		else L->pos[1] = (int16_t)(L->pos[1] + pe->pos[1]);
		L->pos[2] = (int16_t)(L->pos[2] + pe->pos[2]);
		L->counter = 0;
		L->scale = (int16_t)scale;
		L->turn = (int16_t)turn;
		L->depth = (int16_t)depth;
		L->objs = LAYOUTS + mul32(layout, 12) + 8;
		BuildBoneMatricesFromPose(anim);
		DecodeModelPrimLayout(*(const uint32_t *)(LAYOUTS + mul32(layout, 12)), L->layout, *(const int32_t *)(LAYOUTS + mul32(layout, 12) + 4));
		return ip;
	}

	static const int16_t *SpawnSprite(const int16_t *ip, uint8_t *entity, PoseEntry *pose)
	{
		const int32_t mode = *ip++;
		SpriteNode *s;
		if (mode == 0) s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
		else s = (SpriteNode *)entity;
		if (!s) return ip + 10;
		PoseBones(entity + 0x60, pose);
		const int32_t bone = ip[0];
		GetEffectSpawnPosition(entity, bone, ip[1], s->pos);
		const PoseEntry *pe = pose + bone;
		s->pos[0] = (int16_t)(s->pos[0] + pe->pos[0]);
		s->pos[1] = (int16_t)(s->pos[1] + pe->pos[1]);
		s->pos[2] = (int16_t)(s->pos[2] + pe->pos[2]);
		s->frame = 0;
		s->seq = (uint8_t)ip[2];
		s->size = ip[3];
		s->vel[0] = (int8_t)ip[4];
		s->vel[1] = (int8_t)ip[5];
		s->vel[2] = (int8_t)ip[6];
		s->acc[0] = (int8_t)ip[7];
		s->acc[1] = (int8_t)ip[8];
		s->pad1B = (int8_t)ip[9];
		return ip + 10;
	}

	static int32_t Spread(int32_t range, int32_t base)
	{
		if (range == 0) return base;
		return (mul32(CrtRand(), range) >> 15) + base;
	}

	static const int16_t *SpawnSprites(const int16_t *ip, uint8_t *entity, PoseEntry *pose)
	{
		const int32_t mode = ip[0];
		const int32_t bone = ip[1];
		const int32_t flag = ip[2];
		const int32_t seq = ip[3];
		const int32_t size = ip[4];
		int32_t count = ip[5];
		int32_t r[18];
		for (int k = 0; k < 18; k++) r[k] = ip[6 + k];
		ip += 24;
		PoseBones(entity + 0x60, pose);
		int16_t at[3];
		GetEffectSpawnPosition(entity, bone, flag, at);
		const PoseEntry *pe = pose + bone;
		at[0] = (int16_t)(at[0] + pe->pos[0]);
		at[1] = (int16_t)(at[1] + pe->pos[1]);
		at[2] = (int16_t)(at[2] + pe->pos[2]);
		while (count-- != 0)
		{
			SpriteNode *s = (SpriteNode *)entity;
			if (mode == 0) s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
			if (!s) break;
			s->pos[0] = (int16_t)(Spread(r[0], r[1]) + at[0]);
			s->pos[1] = (int16_t)(Spread(r[2], r[3]) + at[1]);
			s->pos[2] = (int16_t)(Spread(r[4], r[5]) + at[2]);
			s->seq = (uint8_t)seq;
			s->frame = 0;
			s->size = (int16_t)size;
			s->vel[0] = (int8_t)Spread(r[6], r[7]);
			s->vel[1] = (int8_t)Spread(r[8], r[9]);
			s->vel[2] = (int8_t)Spread(r[10], r[11]);
			s->acc[0] = (int8_t)Spread(r[12], r[13]);
			s->acc[1] = (int8_t)Spread(r[14], r[15]);
			s->pad1B = (int8_t)Spread(r[16], r[17]);
		}
		return ip;
	}

	static const int16_t *ScriptQuad(const int16_t *ip)
	{
		uint8_t *pk = (uint8_t *)PacketCursor();
		PacketCursor() += 0x18;
		*(uint16_t *)(pk + 8) = 0;
		*(uint16_t *)(pk + 0x10) = 0;
		*(uint16_t *)(pk + 0xE) = 0;
		*(uint16_t *)(pk + 0xA) = 0;
		*(uint16_t *)(pk + 0x14) = 0x140;
		*(uint16_t *)(pk + 0xC) = 0x140;
		*(uint16_t *)(pk + 0x16) = 0xD8;
		*(uint16_t *)(pk + 0x12) = 0xD8;
		pk[4] = *(const uint8_t *)ip;
		ip++;
		pk[5] = *(const uint8_t *)ip;
		ip++;
		*(uint32_t *)pk = 0x5000000;
		pk[6] = *(const uint8_t *)ip;
		pk[7] = 0x2A;
		InsertPrimAltViewport(OT(0x20), pk);
		return ip + 1;
	}

	static uint32_t __cdecl ScriptTask(TaskNode *n)
	{
		ScriptNode *s = (ScriptNode *)n;
		const int16_t *ip = s->ip;
		PoseEntry *e = s->cur;
		for (;;)
		{
			const int32_t op = *ip++;
			switch (op)
			{
			case 0x1000: e->pos[0] = ip[0]; e->pos[1] = ip[1]; e->pos[2] = ip[2]; ip += 3; break;
			case 0x1001:
				e->pos[0] = (int16_t)(e->pos[0] + ip[0]);
				e->pos[1] = (int16_t)(e->pos[1] + ip[1]);
				e->pos[2] = (int16_t)(e->pos[2] + ip[2]);
				ip += 3;
				break;
			case 0x1002: s->vel[0] = ip[0]; s->vel[1] = ip[1]; s->vel[2] = ip[2]; ip += 3; break;
			case 0x1003:
				s->vel[0] = (int16_t)(s->vel[0] + ip[0]);
				s->vel[1] = (int16_t)(s->vel[1] + ip[1]);
				s->vel[2] = (int16_t)(s->vel[2] + ip[2]);
				ip += 3;
				break;
			case 0x1004:
				e->pos[0] = (int16_t)(e->pos[0] + s->vel[0]);
				e->pos[1] = (int16_t)(e->pos[1] + s->vel[1]);
				e->pos[2] = (int16_t)(e->pos[2] + s->vel[2]);
				break;
			case 0x100A: e->vis = *ip++; break;
			case 0x1010: e->rot[0] = ip[0]; e->rot[1] = ip[1]; e->rot[2] = ip[2]; ip += 3; break;
			case 0x1011:
				e->rot[0] = (int16_t)(e->rot[0] + ip[0]);
				e->rot[1] = (int16_t)(e->rot[1] + ip[1]);
				e->rot[2] = (int16_t)(e->rot[2] + ip[2]);
				ip += 3;
				break;
			case 0x1012: s->avel[0] = ip[0]; s->avel[1] = ip[1]; s->avel[2] = ip[2]; ip += 3; break;
			case 0x1013:
				s->avel[0] = (int16_t)(s->avel[0] + ip[0]);
				s->avel[1] = (int16_t)(s->avel[1] + ip[1]);
				s->avel[2] = (int16_t)(s->avel[2] + ip[2]);
				ip += 3;
				break;
			case 0x1014:
				e->rot[0] = (int16_t)(e->rot[0] + s->avel[0]);
				e->rot[1] = (int16_t)(e->rot[1] + s->avel[1]);
				e->rot[2] = (int16_t)(e->rot[2] + s->avel[2]);
				break;
			case 0x1080:
			{
				const uint32_t *src = (const uint32_t *)(s->pose + *ip++);
				uint32_t *dst = (uint32_t *)e;
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				dst[3] = src[3];
				break;
			}
			case 0x1081:
			{
				const uint32_t *src = (const uint32_t *)(s->pose + *ip++);
				((uint32_t *)e)[0] = src[0];
				((uint32_t *)e)[1] = src[1];
				break;
			}
			case 0x1082:
			{
				const uint32_t *src = (const uint32_t *)(s->pose + *ip++);
				((uint32_t *)e)[2] = src[2];
				((uint32_t *)e)[3] = src[3];
				break;
			}
			case 0x1090: e->vis = 0x80; break;
			case 0x10A0: ip = SpawnLayout(ip, s->entity, s->pose, 0); break;
			case 0x10A1: ip = SpawnLayout(ip, s->entity, s->pose, 1); break;
			case 0x10A8: ip = SpawnSprite(ip, s->entity, s->pose); break;
			case 0x10A9: ip = SpawnSprites(ip, s->entity, s->pose); break;
			case 0x10B0:
				FX_HELD(held_note_quad(ip);)
				ip = ScriptQuad(ip);
				break;
			case 0x10F0:
			{
				int16_t *w = (int16_t *)ip;
				if (w[0] != 0)
				{
					w[0] = (int16_t)(w[0] - 1);
					ip = ip + 1 + w[2] - 2;
				}
				else
				{
					w[0] = w[1];
					ip += 3;
				}
				break;
			}
			case 0x1100: return TASK_END;
			default:
				s->ip = ip;
				return 0;
			}
		}
	}

	static void StartScript(uint8_t *entity, PoseEntry *pose, const int16_t *script)
	{
		ScriptNode *s = (ScriptNode *)AddTaskToQueue(QScripts(), ORIG_ScriptTask);
		Memset32(&s->cur, 0, 8);
		s->entity = entity;
		s->pose = pose;
		s->cur = pose + script[0];
		s->ip = script + 1;
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		if (m->counter >= 0xAA)
		{
			*(uint16_t *)Caster() &= 0xFFF7;
			*(uint32_t *)(Caster() + 0x7C) = 3;
			ShowAllPolys(Caster());
			*(uint16_t *)Caster() &= 0xFFFB;
			*(uint16_t *)Entity(0) &= 0xFFFB;
			*(uint16_t *)Entity(1) &= 0xFFFB;
			*(uint16_t *)Entity(2) &= 0xFFFB;
			return TASK_END;
		}
		if (m->counter == 0)
		{
			for (int i = 0; i < 18; i++) StartScript(Caster(), m->pose, (const int16_t *)SCRIPTS[i]);
		}
		static const int16_t TILES[3] = { 0x13, 0x18, 0x2B };
		for (int i = 0; i < 3; i++)
		{
			const int32_t t = m->counter - TILES[i];
			if ((uint32_t)t < 4)
			{
				FX_HELD(held_note_tile(t);)
				PacketCursor() = FadeTile(t, 0xFF, 0xFF, 0xFF, 0, 1, 3, PacketCursor());
			}
		}
		if (m->counter >= 0)
		{
			FX_HELD(held_note_caster(m);)
			CasterDraw(Caster(), m->pose);
			*Caster() |= 4;
		}
		if (m->counter == 10) BdPlaySE(SOUND_Start, 0, 0x80);
		if (m->counter == 0xA6) ReleaseVoice(m->voice);
		m->counter++;
		return 0;
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		FX_HELD(held_note_root();)
		if (r->parity)
		{
			PacketCursor() = TexBase() + 0x6F20;
			r->parity = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x16F20;
			r->parity = 1;
		}
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			InitTaskQueuePool(QMaster(), (void *)(TexBase() + 0x318), 0x488, 1);
			InitTaskQueuePool(QScripts(), (void *)TexBase(), 0x2C, 0x12);
			InitTaskQueuePool(QLayouts(), (void *)(TexBase() + 0x7A0), 0x598, 0x10);
			InitTaskQueuePool(QSprites(), (void *)(TexBase() + 0x6120), 0x1C, 0x80);
			MasterNode *m = (MasterNode *)AddTaskToQueue(QMaster(), ORIG_MasterTask);
			Memset32(&m->counter, 0, 0x11F);
			m->voice = ClaimVoiceSlot(SOUND_Voice, 1, 0x80);
		}
		int a;
		if (r->started)
		{
			a = ExecuteTaskQueue(QMaster());
			PoseBones(Caster() + 0x60, (const void *)(TexBase() + 0x330));
			ExecuteTaskQueue(QScripts());
			ExecuteTaskQueue(QLayouts());
			ExecuteTaskQueue(QSprites());
			BuildBoneMatricesFromPose(Caster() + 0x60);
		}
		else a = (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag064_griever_tail()
	{
		register_port(gtail064::ORIG_RootTask, (void *)gtail064::RootTask, "T064 RootTask", 64);
		register_port(gtail064::ORIG_MasterTask, (void *)gtail064::MasterTask, "T064 MasterTask", 64);
		register_port(gtail064::ORIG_ScriptTask, (void *)gtail064::ScriptTask, "T064 ScriptTask", 64);
		register_port(gtail064::ORIG_LayoutTask, (void *)gtail064::LayoutTask, "T064 LayoutTask", 64);
		register_port(gtail064::ORIG_SpriteTask, (void *)gtail064::SpriteTask, "T064 SpriteTask", 64);
		FX_HELD(register_mag064_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag064_griever_tail_held.inc"
#endif
