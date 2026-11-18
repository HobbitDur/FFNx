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
namespace degen306
{
	using namespace eng;
	using namespace magc;

	template<typename T> static inline T &F(uint32_t base, int32_t off) { return *(T *)(base + off); }
	static inline int16_t &S16(uint32_t p, int32_t o) { return F<int16_t>(p, o); }
	static inline uint16_t &U16(uint32_t p, int32_t o) { return F<uint16_t>(p, o); }
	static inline int32_t &S32(uint32_t p, int32_t o) { return F<int32_t>(p, o); }
	static inline uint32_t &U32(uint32_t p, int32_t o) { return F<uint32_t>(p, o); }
	static inline uint8_t &U8(uint32_t p, int32_t o) { return F<uint8_t>(p, o); }

	inline int32_t &TargetSlot() { return var<int32_t>(us(0x2242590)); }
	inline TaskQueue *QRoot() { return (TaskQueue *)us(0x22425B8); }
	inline TaskQueue *QEffects() { return (TaskQueue *)us(0x2243348); }
	inline CastContext *&Ctx() { return var<CastContext *>(us(0x2244218)); }
	inline int32_t &CasterSlot() { return var<int32_t>(us(0x224421C)); }
	inline uint32_t &TexBase() { return var<uint32_t>(us(0x2245644)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(us(0x2245648)); }
	inline uint32_t &FrameCursor() { return var<uint32_t>(us(0x1D8E054)); }
	inline uint32_t &VertexBuffer() { return var<uint32_t>(us(0x1D98B3C)); }
	inline uint32_t OTBase() { return var<uint32_t>(us(0x1D8E04C)); }
	inline uint32_t Ent(int32_t slot) { return us(0x1D972C0) + (uint32_t)(slot * 0x9C); }
	inline uint8_t *Targets() { return Ctx()->actions->targets; }

	static const uint32_t PARTS = 0x22425E8, PARTS_END = 0x2242F48;
	static const int PART_COUNT = 100;
	static const uint32_t SAVE = 0x2244168;
	static const uint32_t TARGET_Pos = 0x2244210;
	static const uint32_t HAND_Pos = 0x2244228;
	static const uint32_t BIND_Block = 0x22425C8;
	static const uint32_t COM_Anim = 0xCE454C;

	static const uint32_t ORIG_RootTask = 0x5A5760;
	static const uint32_t ORIG_TexRestore = 0x5A5720;
	static const uint32_t ORIG_Controller = 0x5A2D50;
	static const uint32_t ORIG_Core = 0x5A3490;
	static const uint32_t ORIG_Pulse = 0x5A35D0;
	static const uint32_t ORIG_Flicker = 0x5A3710;
	static const uint32_t ORIG_ScreenRing = 0x5A3840;
	static const uint32_t ORIG_Vortex = 0x5A3970;
	static const uint32_t ORIG_Swallow = 0x5A47C0;
	static const uint32_t ORIG_Debris = 0x5A4D80;
	static const uint32_t ORIG_FadeIn = 0x5A5070;
	static const uint32_t ORIG_FadeOut = 0x5A5270;
	static const uint32_t ORIG_TargetRing = 0x5A53B0;
	static const uint32_t ORIG_Pull = 0x5A5540;
	static const uint32_t MODEL_Core = 0xCEAB94, MODEL_Pulse = 0xCEAF2C, MODEL_ScreenRing = 0xCE643C, MODEL_TargetRing = 0xCEA2E4;
	static const uint32_t MODEL_Vortex = 0xCE7A2C;
	static const uint32_t SEQ_Flicker = 0xCE60C0, SEQ_Debris = 0xCE62A8;
	static const uint32_t *VortexColours() { return (const uint32_t *)us(0xCEB2A0); }
	static const void *SOUND_Cast() { return (const void *)us(0xCEB284); }
	static const void *CAMERA_Anim() { return (const void *)us(0xCE4164); }

	inline void BindEntityModelFromComData(uint32_t entity, uint32_t out, uint32_t data) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(us(0x5022C0))(entity, out, data); }
	inline void QueueChainTransformation(uint32_t entity, int32_t id) { fn<void (__cdecl *)(uint32_t, int32_t)>(us(0x505C00))(entity, id); }
	inline int32_t GetEffectSpawnPosition(uint32_t entity, int32_t bone, int32_t flag, uint32_t out) { return fn<int32_t (__cdecl *)(uint32_t, int32_t, int32_t, uint32_t)>(us(0x502170))(entity, bone, flag, out); }
	inline void MatVec(const void *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const void *, const int16_t *, int16_t *)>(us(0x56C4F0))(m, in, out); }
	inline uint32_t DrawShadow(uint32_t entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, int32_t, uint32_t)>(us(0x5088A0))(entity, ot, mode, cursor); }
	inline void GteLoadV0Words(const void *v) { fn<void (__cdecl *)(const void *)>(us(0x64DEE0))(v); }
	inline void GteMVMVA_RotV0Tr2() { fn<void (__cdecl *)()>(us(0x460830))(); }
	inline void GteStoreIRVertex(void *out) { fn<void (__cdecl *)(void *)>(us(0x64DF10))(out); }
	inline void GteSetFarColour(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(us(0x45DD60))(r, g, b); }
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(us(0x45DFE0))(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(us(0x45FE10))(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E210))(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(us(0x45EE10))(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E3C0))(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(us(0x45E270))(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(us(0x45E5C0))(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(us(0x45E610))(); }
	inline void GteStoreOTZ(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E3D0))(dst); }
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(us(0x45E110))(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(us(0x45F270))(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(us(0x45E360))(c); }
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(us(0x45E120))(a, b, c); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(us(0x45F4C0))(); }
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(us(0x45E370))(a, b, c); }

	struct Cue { uint32_t flags; int32_t fade; };

#pragma pack(push, 1)
	struct RenderHeader
	{
		uint32_t model;
		uint32_t verts;
		uint8_t far_rgb[4];
		int32_t fade;
		uint16_t u_off;
		uint16_t pad12;
		uint16_t v_off;
		uint16_t pad16;
		int32_t depth;
		uint32_t flags;
		uint32_t colour;
		uint32_t list;
		int32_t mac0;
		uint32_t pad2C;
		int32_t otz;
		uint32_t gte_flag;
		uint8_t pad38[0x20];
		uint32_t scale_rgb;
		uint32_t c[4];
	};
#pragma pack(pop)
	static_assert(sizeof(RenderHeader) == 0x6C, "Degenerator (Quistis) render header");
}
}

#ifdef FF8_FX_HELD
#include "mag306_degenerator_quistis_held.h"
#endif

namespace ff8fx
{
namespace degen306
{
	template<typename T> static inline T &At(uint32_t base, uint32_t off) { return *(T *)(base + off); }

	static inline bool OutX(uint32_t xy) { int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(uint32_t xy) { int16_t v = *(const int16_t *)(xy + 2); return v < 0 || v > 0x6C0; }

	static inline void UvOffsets(const RenderHeader *h, uint32_t pkt, uint32_t u_word, uint32_t v_word)
	{
		const uint32_t fl = h->flags;
		if (fl & 0x400) At<uint16_t>(pkt, u_word) = (uint16_t)(At<uint16_t>(pkt, u_word) + h->u_off);
		else if (fl & 0x100) At<uint16_t>(pkt, u_word) = h->u_off;
		if (fl & 0x800) At<uint16_t>(pkt, v_word) = (uint16_t)(At<uint16_t>(pkt, v_word) + h->v_off);
		else if (fl & 0x200) At<uint16_t>(pkt, v_word) = h->v_off;
	}

	static inline void ScaleColours(RenderHeader *h, int n)
	{
		h->scale_rgb = h->colour;
		uint8_t *s = (uint8_t *)&h->scale_rgb;
		for (int k = 0; k < n; k++)
		{
			uint8_t *c = (uint8_t *)&h->c[k];
			c[0] = (uint8_t)((uint32_t)c[0] * s[0] >> 7);
			c[1] = (uint8_t)((uint32_t)c[1] * s[1] >> 7);
			c[2] = (uint8_t)((uint32_t)c[2] * s[2] >> 7);
		}
	}

	static inline uint32_t Bucket(uint32_t ot, const RenderHeader *h, int32_t shift) { return ot + (uint32_t)(h->otz >> shift) * 4; }

	static uint32_t ListFT3(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012(vb + (uint32_t)At<uint16_t>(rec, 4) * 4, vb + (uint32_t)At<uint16_t>(rec, 6) * 4, vb + (uint32_t)At<uint16_t>(rec, 8) * 4);
			GteRTPT();
			uint32_t code = h->colour | 0x24000000;
			At<uint32_t>(pkt, 0) = 0x7000000;
			At<uint32_t>(pkt, 4) = code;
			if (h->flags & 1) At<uint32_t>(pkt, 4) = code | 0x2000000;
			if (h->flags & 4) At<uint32_t>(pkt, 4) &= 0xFDFFFFFF;
			const uint32_t d = (uint32_t)h->depth;
			At<uint32_t>(pkt, 0xC) = At<uint32_t>(rec, 0xC) + d;
			At<uint32_t>(pkt, 0x14) = At<uint32_t>(rec, 0x10) + d;
			At<uint32_t>(pkt, 0x1C) = (At<uint32_t>(rec, 8) >> 16) + d;
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				UvOffsets(h, pkt, 0x16, 0xE);
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x10)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x10), (void *)(pkt + 0x18));
					GteAVSZ3();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0x10)) clip |= 2;
					if (OutX(pkt + 0x18)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0x10)) clip |= 0x20;
					if (OutY(pkt + 0x18)) clip |= 0x40;
					if ((clip & 7) != 7 && (clip & 0x70) != 0x70)
					{
						GteStoreOTZ(&h->otz);
						if (h->flags & 0x40)
						{
							GteLoadRGBC((void *)(pkt + 4));
							GteSetIR0(h->fade);
							GteDPCS();
							GteStoreRGB2((void *)(pkt + 4));
						}
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x20;
					}
				}
			}
			rec += 0x14;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	static uint32_t ListFT4(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012(vb + (uint32_t)At<uint16_t>(rec, 4) * 4, vb + (uint32_t)At<uint16_t>(rec, 6) * 4, vb + (uint32_t)At<uint16_t>(rec, 8) * 4);
			GteRTPT();
			uint32_t code = h->colour | 0x2C000000;
			At<uint32_t>(pkt, 0) = 0x9000000;
			At<uint32_t>(pkt, 4) = code;
			if (h->flags & 1) At<uint32_t>(pkt, 4) = code | 0x2000000;
			if (h->flags & 4) At<uint32_t>(pkt, 4) &= 0xFDFFFFFF;
			const uint32_t d = (uint32_t)h->depth;
			At<uint32_t>(pkt, 0xC) = At<uint32_t>(rec, 0xC) + d;
			const uint32_t uv23 = (d << 16) + d + At<uint32_t>(rec, 0x14);
			At<uint32_t>(pkt, 0x1C) = uv23;
			At<uint32_t>(pkt, 0x14) = At<uint32_t>(rec, 0x10) + d;
			At<uint32_t>(pkt, 0x24) = uv23 >> 16;
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				UvOffsets(h, pkt, 0x16, 0xE);
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x10)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x10), (void *)(pkt + 0x18));
					GteLoadV0((const void *)(vb + (uint32_t)At<uint16_t>(rec, 0xA) * 4));
					GteRTPS();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0x10)) clip |= 2;
					if (OutX(pkt + 0x18)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0x10)) clip |= 0x20;
					if (OutY(pkt + 0x18)) clip |= 0x40;
					GteReadSXY2((void *)(pkt + 0x20));
					GteAVSZ4();
					if (OutX(pkt + 0x20)) clip |= 8;
					if (OutY(pkt + 0x20)) clip |= 0x80;
					if ((clip & 0xF) != 0xF && (clip & 0xF0) != 0xF0)
					{
						GteStoreOTZ(&h->otz);
						if (h->flags & 0x40)
						{
							GteLoadRGBC((void *)(pkt + 4));
							GteSetIR0(h->fade);
							GteDPCS();
							GteStoreRGB2((void *)(pkt + 4));
						}
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x28;
					}
				}
			}
			rec += 0x18;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	static uint32_t ListGT3(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012(vb + (uint32_t)At<uint16_t>(rec, 4) * 4, vb + (uint32_t)At<uint16_t>(rec, 6) * 4, vb + (uint32_t)At<uint16_t>(rec, 8) * 4);
			GteRTPT();
			const uint32_t code = At<uint32_t>(rec, 0);
			At<uint32_t>(pkt, 0) = 0x9000000;
			h->c[0] = code;
			if (h->flags & 2) h->c[0] = code | 0x2000000;
			if (h->flags & 8) h->c[0] &= 0xFDFFFFFF;
			const uint32_t d = (uint32_t)h->depth;
			At<uint32_t>(pkt, 0xC) = At<uint32_t>(rec, 0xC) + d;
			At<uint32_t>(pkt, 0x18) = At<uint32_t>(rec, 0x10) + d;
			At<uint32_t>(pkt, 0x24) = (At<uint32_t>(rec, 8) >> 16) + d;
			h->c[1] = At<uint32_t>(rec, 0x14);
			h->c[2] = At<uint32_t>(rec, 0x18);
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				UvOffsets(h, pkt, 0x1A, 0xE);
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x20)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x14), (void *)(pkt + 0x20));
					GteAVSZ3();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0x14)) clip |= 2;
					if (OutX(pkt + 0x20)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0x14)) clip |= 0x20;
					if (OutY(pkt + 0x20)) clip |= 0x40;
					if ((clip & 7) != 7 && (clip & 0x70) != 0x70)
					{
						GteStoreOTZ(&h->otz);
						ScaleColours(h, 3);
						if (h->flags & 0x80)
						{
							GteLoadRGB012(&h->c[1], &h->c[2], &h->c[0]);
							GteSetIR0(h->fade);
							GteDPCT();
							GteStoreRGB012((void *)(pkt + 0x10), (void *)(pkt + 0x1C), (void *)(pkt + 4));
						}
						else
						{
							At<uint32_t>(pkt, 4) = h->c[0];
							At<uint32_t>(pkt, 0x10) = h->c[1];
							At<uint32_t>(pkt, 0x1C) = h->c[2];
						}
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x28;
					}
				}
			}
			rec += 0x1C;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	static uint32_t ListGT4(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t list = h->list;
		int32_t count = At<int32_t>(list, 0);
		uint32_t rec = list + 4;
		h->list = rec;
		const uint32_t vb = h->verts;
		if (count <= 0)
		{
			h->list = rec;
			return cursor;
		}
		uint32_t pkt = cursor;
		do
		{
			GteLoadV012(vb + (uint32_t)At<uint16_t>(rec, 4) * 4, vb + (uint32_t)At<uint16_t>(rec, 6) * 4, vb + (uint32_t)At<uint16_t>(rec, 8) * 4);
			GteRTPT();
			const uint32_t code = At<uint32_t>(rec, 0);
			At<uint32_t>(pkt, 0) = 0xC000000;
			h->c[0] = code;
			if (h->flags & 2) h->c[0] = code | 0x2000000;
			if (h->flags & 8) h->c[0] &= 0xFDFFFFFF;
			const uint32_t d = (uint32_t)h->depth;
			At<uint32_t>(pkt, 0xC) = At<uint32_t>(rec, 0xC) + d;
			const uint32_t uv23 = (d << 16) + d + At<uint32_t>(rec, 0x14);
			At<uint32_t>(pkt, 0x18) = At<uint32_t>(rec, 0x10) + d;
			At<uint32_t>(pkt, 0x24) = uv23;
			At<uint32_t>(pkt, 0x30) = uv23 >> 16;
			h->c[1] = At<uint32_t>(rec, 0x18);
			h->c[2] = At<uint32_t>(rec, 0x1C);
			h->c[3] = At<uint32_t>(rec, 0x20);
			GteReadFLAG(&h->gte_flag);
			if (!(h->gte_flag & 0x60000))
			{
				GteNCLIP();
				UvOffsets(h, pkt, 0x1A, 0xE);
				GteReadMAC0(&h->mac0);
				const int32_t mac0 = h->mac0;
				if (mac0 != 0 && (mac0 > 0 || (h->flags & 0x20)))
				{
					GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x14), (void *)(pkt + 0x20));
					GteLoadV0((const void *)(vb + (uint32_t)At<uint16_t>(rec, 0xA) * 4));
					GteRTPS();
					uint32_t clip = OutX(pkt + 8) ? 1 : 0;
					if (OutX(pkt + 0x14)) clip |= 2;
					if (OutX(pkt + 0x20)) clip |= 4;
					if (OutY(pkt + 8)) clip |= 0x10;
					if (OutY(pkt + 0x14)) clip |= 0x20;
					if (OutY(pkt + 0x20)) clip |= 0x40;
					GteReadSXY2((void *)(pkt + 0x2C));
					GteAVSZ4();
					if (OutX(pkt + 0x2C)) clip |= 8;
					if (OutY(pkt + 0x2C)) clip |= 0x80;
					if ((clip & 0xF) != 0xF && (clip & 0xF0) != 0xF0)
					{
						GteStoreOTZ(&h->otz);
						ScaleColours(h, 4);
						if (h->flags & 0x80)
						{
							GteLoadRGB012(&h->c[1], &h->c[2], &h->c[3]);
							GteSetIR0(h->fade);
							GteDPCT();
							GteStoreRGB012((void *)(pkt + 0x10), (void *)(pkt + 0x1C), (void *)(pkt + 0x28));
							GteLoadRGBC(&h->c[0]);
							GteDPCS();
							GteStoreRGB2((void *)(pkt + 4));
						}
						else
						{
							At<uint32_t>(pkt, 4) = h->c[0];
							At<uint32_t>(pkt, 0x10) = h->c[1];
							At<uint32_t>(pkt, 0x1C) = h->c[2];
							At<uint32_t>(pkt, 0x28) = h->c[3];
						}
						InsertPrimAutoDepth(Bucket(ot, h, shift), (void *)pkt);
						pkt += 0x34;
					}
				}
			}
			rec += 0x24;
		} while (--count);
		h->list = rec;
		return pkt;
	}

	static uint32_t RenderModel(RenderHeader *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t fl = h->flags;
		if (!(fl & 0x2000)) h->verts = h->model + 8;
		h->list = At<uint32_t>(h->model, 0) + h->model;
		if (!(fl & 0x1000)) h->depth = 0;
		GteSetFarColour(h->far_rgb[0], h->far_rgb[1], h->far_rgb[2]);
		h->list += 8;
		if (At<uint32_t>(h->list, 0) != 0) cursor = ListFT3(h, ot, shift, cursor);
		else h->list += 4;
		if (At<uint32_t>(h->list, 0) != 0) cursor = ListFT4(h, ot, shift, cursor);
		else h->list += 4;
		h->list += 8;
		if (At<uint32_t>(h->list, 0) != 0) cursor = ListGT3(h, ot, shift, cursor);
		else h->list += 4;
		if (At<uint32_t>(h->list, 0) != 0) cursor = ListGT4(h, ot, shift, cursor);
		else h->list += 4;
		return cursor;
	}

	static uint32_t __cdecl RootTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		FX_HELD(held_note_root();)
		if (U8(n, 0xC) & 1) PacketCursor() = TexBase() + 0xC000;
		else PacketCursor() = TexBase();
		const int alive = ExecuteTaskQueue(QEffects());
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return alive ? 0 : TASK_END;
	}

	static uint32_t __cdecl TexRestore(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		if (S16(n, 0xC) == 1)
		{
			S16(n, 0xE) = 0;
			TextureRestoreTask((void *)(TexBase() + 0x20000), (void *)(n + 0xE));
		}
		const int16_t done = S16(n, 0xE);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return done ? TASK_END : 0;
	}

	static void EntityColours(uint32_t e, int32_t level)
	{
		uint32_t col = 0x32000000;
		GteSetFarColour(var<uint8_t>(us(0xB8B9A8)), var<uint8_t>(us(0xB8B9A9)), var<uint8_t>(us(0xB8B9AA)));
		GteLoadRGBC(&col);
		GteSetIR0(level);
		GteDPCS();
		GteStoreRGB2((void *)(e + 0x2C));
		GteSetFarColour(var<uint8_t>(us(0xB8B7D8)), var<uint8_t>(us(0xB8B7D9)), var<uint8_t>(us(0xB8B7DA)));
		col = 0;
		GteLoadRGBC(&col);
		GteDPCS();
		GteStoreRGB2((void *)(e + 0x28));
	}

	static uint32_t __cdecl FadeInTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const int32_t level = shl32(S16(n, 0xC), 12) / S16(n, 0x18);
		const uint32_t e = Ent(S16(n, 0xE));
		EntityColours(e, level);
		U8(e, 0x2B) = 2;
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c == 0) U16(e, 0) = (uint16_t)((U16(e, 0) & 0xFFFB) | 0x800);
		S16(n, 0xC) = (int16_t)(c + 1);
		if (S16(n, 0xC) < S16(n, 0x18)) return 0;
		U32(e, 0x28) = var<uint32_t>(us(0xB8B7D8));
		U16(e, 0) = (uint16_t)((U16(e, 0) & 0xF7FF) | U16(n, 0x1A));
		return TASK_END;
	}

	static uint32_t __cdecl FadeOutTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const int32_t level = 0x1000 - shl32(S16(n, 0xC), 12) / S16(n, 0x18);
		const uint32_t e = Ent(S16(n, 0xE));
		U8(e, 7) = 0;
		EntityColours(e, level);
		U8(e, 0x2B) = 2;
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c == 0) U8(e, 1) |= 8;
		S16(n, 0xC) = (int16_t)(c + 1);
		if (S16(n, 0xC) < S16(n, 0x18)) return 0;
		U16(e, 0) = (uint16_t)((U16(e, 0) & 0xF7FF) | U16(n, 0x1A) | 4);
		return TASK_END;
	}

	static void FadeOutOthers(int32_t duration)
	{
		int32_t target = TargetSlot();
		for (int32_t k = 0; k < 7; k++)
		{
			const uint32_t e = Ent(k), f = us(SAVE) + 0x18 * (uint32_t)k;
			const uint16_t fl = U16(e, 0);
			bool spawn = (fl & 2) && !(fl & 4) && k != CasterSlot() && k != target;
			if (spawn)
			{
				uint32_t l = U32(e, 0x8C);
				if (l != 0 && l != e)
				{
					const uint32_t te = Ent(target);
					do
					{
						if ((U8(l, 0) & 2) && l == te)
						{
							spawn = false;
							break;
						}
						l = U32(l, 0x8C);
					} while (l != e);
				}
			}
			if (!spawn)
			{
				U32(f, 0) = 0;
				continue;
			}
			U32(f, 0) = 1;
			const uint32_t t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_FadeOut));
			S16(t, 0xC) = 0;
			S16(t, 0x1A) = (int16_t)(U16(e, 0) & 0x800);
			target = TargetSlot();
			S16(t, 0xE) = (int16_t)k;
			S16(t, 0x18) = (int16_t)duration;
		}
	}

	static void FadeInAll(int32_t duration)
	{
		for (int32_t k = 0; k < 7; k++)
		{
			if (U32(us(SAVE) + 0x18 * (uint32_t)k, 0) == 0) continue;
			const uint32_t t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_FadeIn));
			S16(t, 0xC) = 0;
			S16(t, 0xE) = (int16_t)k;
			S16(t, 0x18) = (int16_t)duration;
			S16(t, 0x1A) = (int16_t)(U16(Ent(k), 0) & 0x800);
		}
	}

	static void StageShow()
	{
		for (uint32_t a = us(0x1D98991); a < us(0x1D98A41); a += 0x2C) U8(a, 0) |= 2;
	}
	static void StageHide()
	{
		for (uint32_t a = us(0x1D98991); a < us(0x1D98A41); a += 0x2C) U8(a, 0) &= 0xFD;
	}

	static uint32_t __cdecl Controller(TaskNode *nd)
	{
		FX_HELD(held_note_controller(nd);)
		if (DrawOnly()) return 0;
		const uint32_t n = (uint32_t)nd;
		const int16_t c = S16(n, 0xC);
		if (c == 1)
		{
			BindEntityModelFromComData(Ent(CasterSlot()), us(BIND_Block), us(COM_Anim));
			QueueChainTransformation(Ent(CasterSlot()), 2);
		}
		if (c == 0xA)
		{
			int16_t v[4];
			GetEffectSpawnPosition(Ent(CasterSlot()), 0x19, 0x1000, us(HAND_Pos));
			GetEffectSpawnPosition(Ent(CasterSlot()), 0x1A, 0x1000, (uint32_t)v);
			S16(us(HAND_Pos), 0) = (int16_t)((S16(us(HAND_Pos), 0) + v[0]) >> 1);
			v[0] = 0;
			S16(us(HAND_Pos), 2) = (int16_t)((S16(us(HAND_Pos), 2) + v[1]) >> 1);
			S16(us(HAND_Pos), 4) = (int16_t)((S16(us(HAND_Pos), 4) + v[2]) >> 1);
			v[1] = 0;
			v[2] = -50;
			MatVec((const void *)(Ent(CasterSlot()) + 0x40), v, v);
			S16(us(HAND_Pos), 0) = (int16_t)(S16(us(HAND_Pos), 0) + v[0]);
			S16(us(HAND_Pos), 2) = (int16_t)(S16(us(HAND_Pos), 2) + v[1]);
			S16(us(HAND_Pos), 4) = (int16_t)(S16(us(HAND_Pos), 4) + v[2]);
			const uint32_t t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_Core));
			U32(t, 0x10) = U32(us(HAND_Pos), 0);
			U32(t, 0x14) = U32(us(HAND_Pos), 4);
			S16(t, 0xC) = 0;
			S16(t, 0x1C) = 0x700;
			S16(t, 0x1E) = 0xF;
			S16(t, 0x18) = S16(Ent(CasterSlot()), 0xE);
		}
		if (c >= 0xA && c <= 0x64 && c % 3 == 1)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_Pulse));
			U32(t, 0x10) = U32(us(HAND_Pos), 0);
			U32(t, 0x14) = U32(us(HAND_Pos), 4);
			S16(t, 0xC) = 0;
			S16(t, 0x18) = S16(Ent(CasterSlot()), 0xE);
			S16(t, 0x1E) = 0x1C0;
			S16(t, 0x1C) = 0x1C0;
		}
		if (c == 0xA)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_Flicker));
			U32(t, 0x10) = U32(us(HAND_Pos), 0);
			S16(t, 0xC) = 0;
			U32(t, 0x14) = U32(us(HAND_Pos), 4);
		}
		if (c == 0x20)
		{
			uint32_t t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_ScreenRing));
			S16(t, 0xC) = 0;
			S16(t, 0x18) = (int16_t)(CrtRand() % 0x1000);
			S16(t, 0x1A) = 0x10;
			S16(t, 0x1C) = 0x2E0;
			t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_ScreenRing));
			S16(t, 0xC) = 0;
			S16(t, 0x18) = (int16_t)(CrtRand() % 0x1000);
			S16(t, 0x1A) = -0x20;
			S16(t, 0x1C) = 0x360;
		}
		if (c == 0x2A)
		{
			if (TargetSlot() < 3) GetEffectSpawnPosition(Ent(TargetSlot()), 0xF1, 0, us(TARGET_Pos));
			else GetDefaultEffectPosition((uint8_t *)Ent(TargetSlot()), (int16_t *)us(TARGET_Pos));
			for (int k = 0; k < 2; k++)
			{
				const uint32_t t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_TargetRing));
				U32(t, 0x10) = U32(us(TARGET_Pos), 0);
				S16(t, 0xC) = 0;
				U32(t, 0x14) = U32(us(TARGET_Pos), 4);
				S16(t, 0x18) = (int16_t)(CrtRand() % 0x1000);
				S16(t, 0x1A) = (int16_t)(k ? -0x14 : 0x14);
				const int16_t s = (int16_t)(shl32(S16(Ent(TargetSlot()), 0x26), 0xB) >> 0xC);
				S16(t, 0x1C) = s;
				S16(t, 0x1E) = (int16_t)(s / 18);
			}
		}
		if (c == 0x3E)
		{
			for (int k = 0; k < 2; k++)
			{
				const uint32_t t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_Vortex));
				U32(t, 0x10) = U32(us(TARGET_Pos), 0);
				S16(t, 0xC) = 0;
				U32(t, 0x14) = U32(us(TARGET_Pos), 4);
				S16(t, 0x18) = (int16_t)(CrtRand() % 0x1000);
				S16(t, 0x1A) = (int16_t)(k ? 0x3C : 0x28);
				S16(t, 0x20) = (int16_t)(k ? 0x800 : 0);
				const int16_t s = (int16_t)(shl32(S16(Ent(TargetSlot()), 0x26), 0xC) >> 0xB);
				S16(t, 0x1C) = s;
				S16(t, 0x1E) = (int16_t)(s / 40);
			}
		}
		if (c == 0x48 && !(Targets()[3] & 4))
		{
			uint32_t t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_Swallow));
			U32(t, 0x10) = U32(us(TARGET_Pos), 0);
			S16(t, 0xC) = 0;
			U32(t, 0x14) = U32(us(TARGET_Pos), 4);
			t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_Pull));
			U32(t, 0x10) = U32(us(TARGET_Pos), 0);
			S16(t, 0xC) = 0;
			S16(t, 0xE) = (int16_t)TargetSlot();
			U32(t, 0x14) = U32(us(TARGET_Pos), 4);
			S16(t, 0x18) = (int16_t)shl32(CrtRand() % 2 + 1, 12);
		}
		if (c == 0x65 && !(Targets()[3] & 4))
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(QEffects(), us(ORIG_Debris));
			U32(t, 0x10) = U32(us(TARGET_Pos), 0);
			S16(t, 0xC) = 0;
			U32(t, 0x14) = U32(us(TARGET_Pos), 4);
		}
		if (c == 0x75)
		{
			uint8_t *tg = Targets();
			if (!(tg[3] & 4)) U8(Ent(TargetSlot()), 0) |= 4;
			ApplyActionResultToTarget(tg);
		}
		if (c == 0x47)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(QRoot(), us(ORIG_TexRestore));
			S16(t, 0xC) = 0;
		}
		if (c == 0) FadeOutOthers(8);
		if (c <= 8) SetScreenFlash((uint32_t)shl32(c, 8), 0);
		if (c >= 0xA && c < 0x1A) SetScreenFlash((uint32_t)(c * 0x4E + 0x4F4), 0);
		if (c == 0x1A) StageHide();
		if (c == 0x72)
		{
			StageShow();
			FadeInAll(8);
		}
		if (c >= 0x72) SetScreenFlash((uint32_t)shl32(0x7A - c, 9), 0);
		if (c == 1) BdPlaySE(SOUND_Cast(), 0, 0x80);
		S16(n, 0xC) = (int16_t)(c + 1);
		if (S16(n, 0xC) <= 0x7A) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	static void PrimDraw(uint32_t model, Cue cue, uint32_t ot, int32_t mode)
	{
		const uint32_t h = (uint32_t)FieldAlloc(0x58);
		U32(h, 0) = model;
		U32(h, 8) = 0;
		U32(h, 0x1C) = 0x33;
		if (cue.flags == 0xF3)
		{
			S32(h, 0xC) = cue.fade;
			U32(h, 0x1C) = 0xF3;
		}
		PacketCursor() = RenderPrimModel((void *)h, ot, mode, PacketCursor());
		FieldFree(0x58);
	}

	static void HandModelDraw(uint32_t n, uint32_t model, Cue cue)
	{
		int16_t a[4];
		Mat4x3 m;
		int32_t s[3];
		a[0] = 0;
		a[1] = S16(n, 0x18);
		a[2] = 0;
		ComposeZYXRotationMatrix(a, &m);
		m.t[0] = S16(n, 0x10);
		m.t[1] = S16(n, 0x12);
		s[2] = s[1] = s[0] = S16(n, 0x1C);
		m.t[2] = S16(n, 0x14);
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimDraw(model, cue, OTBase() + 0x44, 2);
	}

	static Cue CoreCue(int32_t c)
	{
		if (c < 4) return { 0xF3u, shl32(4 - c, 10) };
		if (c >= 0x5A) return { 0xF3u, shl32(c - 0x5A, 9) };
		return { 0x33u, 0 };
	}

	static uint32_t __cdecl CoreTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		FX_HELD(held_note_draw(us(ORIG_Core), n);)
		HandModelDraw(n, us(MODEL_Core), CoreCue(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) - S16(n, 0x1E));
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x62 ? TASK_END : 0;
	}

	static Cue PulseCue(int32_t c)
	{
		if (c >= 2) return { 0xF3u, shl32(c - 2, 9) };
		return { 0x33u, 0 };
	}

	static uint32_t __cdecl PulseTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		FX_HELD(held_note_draw(us(ORIG_Pulse), n);)
		HandModelDraw(n, us(MODEL_Pulse), PulseCue(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		const int16_t d = S16(n, 0x1E);
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + d);
		S16(n, 0x1E) = (int16_t)(d - d / 16);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0xA ? TASK_END : 0;
	}

	static void FlickerDraw()
	{
		int16_t a[4];
		Mat4x3 m;
		int32_t s[3];
		a[0] = 0;
		a[1] = 0;
		a[2] = 0;
		BuildRotationMatrixFromAngles(a, &m);
		s[2] = s[1] = s[0] = 0x100;
		m.t[0] = 0;
		m.t[1] = 0;
		m.t[2] = var<int16_t>(us(0x1D8E038));
		Scale3DMatrix(&m, s);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		U32(h, 0) = us(SEQ_Flicker);
		U16(h, 4) = 0;
		U16(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData((void *)h, OTBase() + 0x44, 0xE, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl FlickerTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		FX_HELD(held_note_draw(us(ORIG_Flicker), n);)
		if (U8(n, 0xC) & 1) FlickerDraw();
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x1E ? TASK_END : 0;
	}

	static Cue ScreenRingCue(int32_t c)
	{
		if (c < 8) return { 0xF3u, shl32(8 - c, 9) };
		if (c >= 0x32) return { 0xF3u, shl32(c - 0x32, 9) };
		return { 0x33u, 0 };
	}

	static void ScreenRingDraw(uint32_t n, Cue cue)
	{
		int16_t a[4];
		Mat4x3 m;
		int32_t s[3];
		a[0] = 0;
		a[1] = 0;
		a[2] = S16(n, 0x18);
		ComposeZYXRotationMatrix(a, &m);
		m.t[2] = var<int16_t>(us(0x1D8E038));
		m.t[0] = 0;
		m.t[1] = 0;
		s[2] = s[1] = s[0] = S16(n, 0x1C);
		Scale3DMatrix(&m, s);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimDraw(us(MODEL_ScreenRing), cue, OTBase() + 0x4068, -0xF2);
	}

	static uint32_t __cdecl ScreenRingTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		FX_HELD(held_note_draw(us(ORIG_ScreenRing), n);)
		ScreenRingDraw(n, ScreenRingCue(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		S16(n, 0x18) = (int16_t)(S16(n, 0x18) + S16(n, 0x1A));
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x3A ? TASK_END : 0;
	}

	static Cue TargetRingCue(int32_t c)
	{
		if (c < 6) return { 0xF3u, 0x1000 - c * 682 };
		if (c >= 0x16) return { 0xF3u, (c - 0x16) * 682 };
		return { 0x33u, 0 };
	}

	static void TargetRingDraw(uint32_t n, Cue cue)
	{
		int16_t a[4];
		Mat4x3 m;
		int32_t s[3];
		a[0] = 0;
		a[1] = 0;
		a[2] = S16(n, 0x18);
		ComposeZYXRotationMatrix(a, &m);
		m.t[0] = S16(n, 0x10);
		m.t[1] = S16(n, 0x12);
		s[2] = s[1] = s[0] = S16(n, 0x1C);
		m.t[2] = S16(n, 0x14);
		Scale3DMatrix(&m, s);
		TransformVectorBy3x3Matrix(&Camera(), m.t, m.t);
		m.t[0] += Camera().t[0];
		m.t[1] += Camera().t[1];
		m.t[2] += Camera().t[2];
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimDraw(us(MODEL_TargetRing), cue, OTBase() + 0x44, 2);
	}

	static uint32_t __cdecl TargetRingTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		FX_HELD(held_note_draw(us(ORIG_TargetRing), n);)
		TargetRingDraw(n, TargetRingCue(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		S16(n, 0x18) = (int16_t)(S16(n, 0x18) + S16(n, 0x1A));
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + S16(n, 0x1E));
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x1C ? TASK_END : 0;
	}

	static Cue VortexCue(int32_t c)
	{
		if (c < 4) return { 0xF3u, shl32(4 - c, 10) };
		return { 0x33u, 0 };
	}

	static void VortexDraw(uint32_t n, uint32_t colour, Cue cue)
	{
		int16_t a[4];
		Mat4x3 m;
		int32_t s[3];
		a[1] = S16(n, 0x18);
		a[0] = 0;
		a[2] = S16(n, 0x20);
		BuildRotationMatrixFromAngles(a, &m);
		m.t[0] = S16(n, 0x10);
		m.t[2] = S16(n, 0x14);
		m.t[1] = S16(n, 0x12);
		s[2] = s[1] = s[0] = S16(n, 0x1C);
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		RenderHeader *h = (RenderHeader *)FieldAlloc(0x6C);
		*(uint32_t *)h->far_rgb = 0;
		h->model = us(MODEL_Vortex);
		h->flags = 0x33;
		h->colour = colour;
		if (cue.flags == 0xF3)
		{
			h->flags = 0xF3;
			h->fade = cue.fade;
		}
		PacketCursor() = RenderModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x6C);
	}

	static uint32_t __cdecl VortexTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		FX_HELD(held_note_draw(us(ORIG_Vortex), n);)
		const int32_t c = S16(n, 0xC);
		VortexDraw(n, VortexColours()[c % 12], VortexCue(c));
		if (DrawOnly()) return 0;
		S16(n, 0x18) = (int16_t)(S16(n, 0x18) + S16(n, 0x1A));
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) - S16(n, 0x1E));
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x28 ? TASK_END : 0;
	}

	static uint32_t SwallowPrims(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t pkt = cursor;
		const uint32_t verts = U32(hdr, 4);
		uint32_t rec = U32(hdr, 0);
		for (int32_t k = 0; k < (int32_t)U16(hdr, 8); k++, rec += 0x10)
		{
			GteLoadV012(verts + (U16(rec, 0) & 0xFFF) * 8, verts + (U16(rec, 2) & 0xFFF) * 8, verts + (U16(rec, 4) & 0xFFF) * 8);
			GteRTPT();
			const uint32_t w = U32(rec, 0xC);
			U32(pkt, 0x14) = w & 0x1FFFFFF;
			U32(pkt, 0) = 0x7000000;
			U32(pkt, 4) = (w & 0x2000000) | U32(hdr, 0x1C) | 0x24000000;
			U32(pkt, 0xC) = U32(rec, 8);
			GteReadFLAG((void *)(hdr + 0x3C));
			if (U32(hdr, 0x3C) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0((void *)(hdr + 0x30));
			if (S32(hdr, 0x30) <= 0) continue;
			GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x10), (void *)(pkt + 0x18));
			GteAVSZ3();
			S32(pkt, 0x1C) = S32(rec, 4) >> 16;
			GteStoreOTZ((void *)(hdr + 0x38));
			InsertPrimAutoDepth(ot + (uint32_t)shl32(S32(hdr, 0x38) >> (shift & 31), 2), (void *)pkt);
			pkt += 0x20;
		}
		if (U16(hdr, 0xA) == 0) return pkt;
		rec += 2;
		for (int32_t k = 0; k < (int32_t)U16(hdr, 0xA); k++, rec += 0x14)
		{
			GteLoadV012(verts + (U16(rec, -2) & 0xFFF) * 8, verts + (U16(rec, 0) & 0xFFF) * 8, verts + (U16(rec, 2) & 0xFFF) * 8);
			GteRTPT();
			const uint32_t w = U32(rec, 0xA);
			U32(pkt, 0x14) = w & 0x1FFFFFF;
			U32(pkt, 0) = 0x9000000;
			U32(pkt, 4) = (w & 0x2000000) | U32(hdr, 0x1C) | 0x2C000000;
			U32(pkt, 0xC) = U32(rec, 6);
			GteReadFLAG((void *)(hdr + 0x3C));
			if (U32(hdr, 0x3C) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0((void *)(hdr + 0x30));
			if (S32(hdr, 0x30) <= 0) continue;
			GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x10), (void *)(pkt + 0x18));
			GteLoadV0((const void *)(verts + (U16(rec, 4) & 0xFFF) * 8));
			GteRTPS();
			const uint32_t w2 = U32(rec, 0xE);
			U32(pkt, 0x1C) = w2;
			S32(pkt, 0x24) = (int32_t)w2 >> 16;
			GteReadSXY2((void *)(pkt + 0x20));
			GteAVSZ4();
			GteStoreOTZ((void *)(hdr + 0x38));
			InsertPrimAutoDepth(ot + (uint32_t)shl32(S32(hdr, 0x38) >> (shift & 31), 2), (void *)pkt);
			pkt += 0x28;
		}
		return pkt;
	}

	static uint32_t SwallowModel(uint32_t model, uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t bones = U32(model, 0) + 0x10;
		uint32_t table = U32(model, 4);
		const int32_t count = S32(table, 0);
		table += 4;
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t obj = U32(model, 4) + U32(table, 0);
			table += 4;
			if (!(U32(hdr, 0x20) & (1u << (i & 31)))) continue;
			int32_t groups = S16(obj, 0);
			uint32_t out = U32(hdr, 4);
			obj += 2;
			for (; groups > 0; groups--)
			{
				const int32_t bone = S16(obj, 0);
				obj += 2;
				const Mat4x3 *m = (const Mat4x3 *)(bones + (uint32_t)(bone * 0x30) + 0x10);
				GteSetRotMatrixCtrl(m);
				GteSetTransVectorCtrl(m);
				int32_t nv = S16(obj, 0);
				obj += 2;
				for (; nv > 0; nv--, out += 8)
				{
					GteLoadV0Words((const void *)obj);
					obj += 6;
					GteMVMVA_RotV0Tr2();
					GteStoreIRVertex((void *)out);
					const int32_t f = S32(hdr, 0x2C);
					S16(out, 0) = (int16_t)(S16(out, 0) + (int16_t)(mul32(S16(hdr, 0x60) - S16(out, 0), f) >> 12));
					S16(out, 2) = (int16_t)(S16(out, 2) + (int16_t)(mul32(S16(hdr, 0x62) - S16(out, 2), f) >> 12));
					S16(out, 4) = (int16_t)(S16(out, 4) + (int16_t)(mul32(S16(hdr, 0x64) - S16(out, 4), f) >> 12));
				}
			}
			obj = (obj + 3) & ~3u;
			U16(hdr, 8) = U16(obj, 0);
			U16(hdr, 0xA) = U16(obj, 2);
			U16(hdr, 0xC) = U16(obj, 4);
			U16(hdr, 0xE) = U16(obj, 6);
			U32(hdr, 0) = obj + 0xC;
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			cursor = SwallowPrims(hdr, ot, shift, cursor);
		}
		return cursor;
	}

	static void SwallowEntity(uint32_t hdr, uint32_t entity)
	{
		const uint32_t anim = entity + 0x60;
		if (!(U8(entity, 0) & 0x20))
			FrameCursor() = DrawShadow(entity, OTBase() + 0x4064, 0x10, FrameCursor());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)(hdr + 0x68));
		ComputeBonesWorldMatrices((void *)anim, (void *)(entity + 0x40));
		U32(hdr, 4) = VertexBuffer();
		U32(hdr, 0x1C) = U32(entity, 0x28);
		U16(hdr, 0x14) = 0;
		U16(hdr, 0x16) = 0;
		U32(hdr, 0x20) = U32(entity, 0x7C);
		const uint8_t cue = U8(entity, 7);
		U8(hdr, 0x2A) = cue;
		U8(hdr, 0x29) = cue;
		U8(hdr, 0x28) = cue;
		U32(hdr, 0x10) = var<uint32_t>(us(0x1D969A8));
		U16(hdr, 0x18) = 0x140;
		U16(hdr, 0x1A) = 0xD8;
		FrameCursor() = SwallowModel(U32(anim, 4), hdr, OTBase() + 0x44, 2, FrameCursor());
		BuildBoneMatricesFromPose((void *)anim);
		const uint32_t second = U32(entity, 0x78);
		if (second && !(U8(entity, 1) & 2))
		{
			U32(hdr, 0x20) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices((void *)second, (void *)(entity + 0x40));
			FrameCursor() = SwallowModel(U32(second, 4), hdr, OTBase() + 0x44, 2, FrameCursor());
			BuildBoneMatricesFromPose((void *)second);
		}
	}

	static int32_t SwallowFactor(int32_t c) { return shl32(c, 12) / 30; }

	static void SwallowDraw(uint32_t n, int32_t factor)
	{
		U8(Ent(TargetSlot()), 0) |= 4;
		const uint32_t hdr = (uint32_t)FieldAlloc(0x88);
		S32(hdr, 0x2C) = factor;
		U32(hdr, 0x60) = U32(n, 0x10);
		U32(hdr, 0x64) = U32(n, 0x14);
		SwallowEntity(hdr, Ent(TargetSlot()));
		FieldFree(0x88);
	}

	static uint32_t __cdecl SwallowTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		if (S16(n, 0xC) < 0x1E)
		{
			FX_HELD(held_note_draw(us(ORIG_Swallow), n);)
			SwallowDraw(n, SwallowFactor(S16(n, 0xC)));
		}
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) > 0x1E ? TASK_END : 0;
	}

	static uint32_t __cdecl PullTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const int16_t c = S16(n, 0xC);
		const int32_t slot = S16(n, 0xE);
		const uint32_t e = Ent(slot), sv = us(SAVE) + 0x18 * (uint32_t)slot;
		if (c == 0)
		{
			U16(sv, 4) = (uint16_t)(U16(e, 0) & 0x1020);
			U32(sv, 0x10) = U32(e, 0x1C);
			U32(sv, 0x14) = U32(e, 0x20);
			U32(sv, 8) = U32(e, 0xC);
			U32(sv, 0xC) = U32(e, 0x10);
		}
		U16(e, 0) |= 0x1020;
		S16(e, 0xC) = (int16_t)(mul32(S16(n, 0x18), c) / 30);
		S16(e, 0xE) = (int16_t)(shl32(c, 12) / 30);
		int16_t v[4];
		Mat4x3 m;
		v[0] = (int16_t)(S16(n, 0x10) - S16(sv, 0x10));
		v[2] = (int16_t)(S16(n, 0x14) - S16(sv, 0x14));
		v[1] = (int16_t)(S16(n, 0x12) - S16(sv, 0x12));
		ComposeZYXRotationMatrix((const int16_t *)(e + 0xC), &m);
		MatVec(&m, v, v);
		S16(e, 0x1C) = (int16_t)(S16(n, 0x10) - v[0]);
		S16(e, 0x1E) = (int16_t)(S16(n, 0x12) - v[1]);
		S16(e, 0x20) = (int16_t)(S16(n, 0x14) - v[2]);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(c + 1);
		if (S16(n, 0xC) < 0x1E) return 0;
		U16(e, 0) = (uint16_t)((U16(e, 0) & 0xEFDF) | U16(sv, 4));
		U32(e, 0x1C) = U32(sv, 0x10);
		U32(e, 0x20) = U32(sv, 0x14);
		U32(e, 0xC) = U32(sv, 8);
		U32(e, 0x10) = U32(sv, 0xC);
		return TASK_END;
	}

	static void DebrisDraw(uint32_t h, uint32_t p)
	{
		U16(h, 4) = U16(p, 4);
		const int16_t s = S16(p, 6);
		TransformCameraByShadowRotation((const void *)(p + 8), s, -(s >> 4));
		PacketCursor() = InitEffectSequenceFromData((void *)h, OTBase() + 0x44, 2, PacketCursor());
	}

	static void DebrisSpawn(uint32_t n)
	{
		for (int k = 0; k < 20; k++)
		{
			uint32_t p = us(PARTS);
			int32_t i = 0;
			while (U32(p, 0) != 0)
			{
				p += 0x18;
				i++;
				if (p >= us(PARTS_END)) return;
			}
			if (i >= PART_COUNT) return;
			U32(p, 0) = 2;
			S16(p, 4) = 0;
			S16(p, 6) = (int16_t)(CrtRand() % 0x300 + 0x100);
			U32(p, 8) = U32(n, 0x10);
			U32(p, 0xC) = U32(n, 0x14);
			int32_t v[3];
			v[0] = CrtRand() % 0x1000 - 0x800;
			v[1] = CrtRand() % 0x1000 - 0x800;
			v[2] = CrtRand() % 0x1000 - 0x800;
			NormalizeVector(v, v);
			const int32_t speed = CrtRand() % 200 + 0xAA;
			S16(p, 0x10) = (int16_t)(mul32(speed, v[0]) >> 12);
			S16(p, 0x12) = (int16_t)(mul32(speed, v[1]) >> 12);
			S16(p, 0x14) = (int16_t)(mul32(speed, v[2]) >> 12);
		}
	}

	static uint32_t __cdecl DebrisTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		U32(h, 0) = us(SEQ_Debris);
		U16(h, 0x24) = 0;
		FX_HELD(held_note_debris(n, h);)
		int32_t alive = 0;
		for (uint32_t p = us(PARTS); p < us(PARTS_END); p += 0x18)
		{
			if (!(U8(p, 0) & 2)) continue;
			DebrisDraw(h, p);
			if (DrawOnly()) continue;
			S16(p, 4) = (int16_t)(S16(p, 4) + 1);
			if (S16(h, 0x28) < 0)
			{
				U32(p, 0) = 0;
				continue;
			}
			const int16_t s = S16(p, 6);
			S16(p, 6) = (int16_t)(s - (int16_t)(s >> 4));
			S16(p, 8) = (int16_t)(S16(p, 8) + S16(p, 0x10));
			S16(p, 0xA) = (int16_t)(S16(p, 0xA) + S16(p, 0x12));
			S16(p, 0xC) = (int16_t)(S16(p, 0xC) + S16(p, 0x14));
			for (int k = 0; k < 3; k++)
			{
				const int16_t v = S16(p, 0x10 + 2 * k);
				S16(p, 0x10 + 2 * k) = (int16_t)(v - (int16_t)(v >> 2));
			}
			alive++;
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c >= 0 && c <= 2) DebrisSpawn(n);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) < 4) return 0;
		return alive == 0 ? TASK_END : 0;
	}
}

	void register_mag306_degenerator_quistis()
	{
		register_port(degen306::ORIG_RootTask, (void *)degen306::RootTask, "D306 RootTask", 306);
		register_port(degen306::ORIG_TexRestore, (void *)degen306::TexRestore, "D306 TexRestore", 306);
		register_port(degen306::ORIG_Controller, (void *)degen306::Controller, "D306 Controller", 306);
		register_port(degen306::ORIG_Core, (void *)degen306::CoreTask, "D306 Core", 306);
		register_port(degen306::ORIG_Pulse, (void *)degen306::PulseTask, "D306 Pulse", 306);
		register_port(degen306::ORIG_Flicker, (void *)degen306::FlickerTask, "D306 Flicker", 306);
		register_port(degen306::ORIG_ScreenRing, (void *)degen306::ScreenRingTask, "D306 ScreenRing", 306);
		register_port(degen306::ORIG_Vortex, (void *)degen306::VortexTask, "D306 Vortex", 306);
		register_port(degen306::ORIG_Swallow, (void *)degen306::SwallowTask, "D306 Swallow", 306);
		register_port(degen306::ORIG_Debris, (void *)degen306::DebrisTask, "D306 Debris", 306);
		register_port(degen306::ORIG_FadeIn, (void *)degen306::FadeInTask, "D306 FadeIn", 306);
		register_port(degen306::ORIG_FadeOut, (void *)degen306::FadeOutTask, "D306 FadeOut", 306);
		register_port(degen306::ORIG_TargetRing, (void *)degen306::TargetRingTask, "D306 TargetRing", 306);
		register_port(degen306::ORIG_Pull, (void *)degen306::PullTask, "D306 Pull", 306);
		FX_HELD(register_mag306_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag306_degenerator_quistis_held.inc"
#endif
