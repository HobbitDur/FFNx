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

// Effect 152: Ultrasonic Waves (enemy attacks 58 and 63 of kernel.bin, Red Bat; MAG_152_*).
//
// Structure (setup MAG_152_ULTRASONIC_WAVES_Init 0x60AFB0, file loader 0x60AF90 = the texture
// file named at 0xDBEC80 (mag151.tim); the setup keeps the context, the caster slot and the
// first target slot of action 0, sets up the root queue (2 x 0x10) with RootTask and the effect
// queue (0x64 x 0x24) with the Controller, frees the 100 burst particles, starts camera
// animation 0xDBE370 and queues the TIM):
//   RootTask (0x60C8F0) - alternates the packet arena (magic buffer + 0 / + 0x8000), runs the
//     effect queue; when it is empty queues WaitTexRestore in the root queue and ends.
//   WaitTexRestore (0x60C960) - at counter 1 starts the engine texture restore task, ends when
//     its done flag is set.
//   Controller (0x60B070) - every tick (also in the draw-only mode): the rotation 0x244D638
//     taking (0, 0, -1) onto the direction caster anchor -> target position. Then, updating
//     ticks only: counter 0 takes the caster's effect anchor 0xF0 (0x244D660) and the target's
//     default effect position (0x244D628); counters 1 / 6 / 11 spawn a Wave at the anchor
//     (+-55 jitter, random roll, size 0x900..0xCFF); counters 5..20 spawn a Ring at the anchor
//     flying 1/18 of the way to the target per tick; 25 spawns BurstA, 26 BurstB and BurstC at
//     the target; 35 damage (action 0, first target); 1 sound 0xDBEC50; ends after 40.
//   Wave (0x60B400) - flipbook 0xDBE5CC (12 frames, frame = counter) at a fixed position, roll
//     and size; from frame 8 a grey level (12 - frame) * 32 in draw mode 0xD. Ends with the
//     flipbook.
//   Ring (0x60B4C0) - the ring prim model 0xDBE9E0: rotated about its z axis by its angle, scaled,
//     turned along the flight direction (0x244D638), moved towards the camera by size / 16;
//     colour from the 10-entry cycle 0xDBEC58, from counter 16 faded to black ((counter - 16)
//     << 10, draw flags 0xF3 instead of 0x33). Moves, spins and grows every tick; ends after 20.
//     Drawn by the module's private prim-model renderer (0x60B6C0: flat textured triangles
//     0x60B7A0, flat textured quads 0x60BA00, gouraud textured triangles 0x60BCB0 and quads
//     0x60BFD0; the ring only has flat textured quads, the other lists are ported as coded).
//   BurstA / BurstB / BurstC (0x60C3C0 / 0x60C580 / 0x60C740) - each draws the particles of its
//     type (1 / 2 / 4) of the shared pool 0x244BEA8 (100 x 0x18) with its flipbook (0xDBE420
//     mode 8 / 0xDBE814 / 0xDBE904), advancing their frames (a particle is freed when its
//     flipbook ends), and spawns 2 / 1 / 1 particles per tick for counters 0..8 / 0..13 / 0..13
//     around its position (+-400 / +-450). Ends from counter 4 once none of its particles is left.
// Every task but RootTask / WaitTexRestore tests the draw-only flags (battle_to_update_flags &
// 0x201): the Controller after the rotation, the others after drawing.
// Module globals: 0x244BE70..0x244D670 (target slot, root pool / queue, particle pool, effect
// queue and node pool, target position, context, caster slot, rotation, texture file, anchor,
// magic buffer base 0x244D668, packet cursor 0x244D66C).

#include "mag_common.h"

namespace ff8fx
{
namespace ultrasonic152
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &TargetSlot() { return var<int32_t>(0x244BE70); }       // first target of action 0
	inline TaskQueue *QRoot() { return (TaskQueue *)0x244BE98; }            // pool 0x244BE78, 2 x 0x10
	inline TaskQueue *QEffects() { return (TaskQueue *)0x244C808; }         // pool 0x244C818, 0x64 x 0x24
	inline int16_t *TargetPos() { return (int16_t *)0x244D628; }            // target's default effect position
	inline CastContext *&Ctx() { return var<CastContext *>(0x244D630); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x244D634); }
	inline Mat4x3 &Rot() { return var<Mat4x3>(0x244D638); }                 // (0, 0, -1) -> flight direction
	inline int16_t *Anchor() { return (int16_t *)0x244D660; }               // caster's effect anchor 0xF0
	inline uint32_t &TexBase() { return var<uint32_t>(0x244D668); }         // Magic_TextureOFF (magic buffer)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x244D66C); }
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	static const uint32_t ORIG_RootTask = 0x60C8F0;
	static const uint32_t ORIG_WaitTexRestore = 0x60C960;
	static const uint32_t ORIG_Controller = 0x60B070;
	static const uint32_t ORIG_WaveTask = 0x60B400;
	static const uint32_t ORIG_RingTask = 0x60B4C0;
	static const uint32_t ORIG_BurstA = 0x60C3C0;
	static const uint32_t ORIG_BurstB = 0x60C580;
	static const uint32_t ORIG_BurstC = 0x60C740;
	static const uint32_t SEQ_Wave = 0xDBE5CC;
	static const uint32_t SEQ_BurstA = 0xDBE420;
	static const uint32_t SEQ_BurstB = 0xDBE814;
	static const uint32_t SEQ_BurstC = 0xDBE904;
	static const uint32_t MODEL_Ring = 0xDBE9E0;
	static const uint32_t *const RingColours = (const uint32_t *)0xDBEC58; // 10 x (code / rgb)
	static const void *const SOUND_Waves = (const void *)0xDBEC50;

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	// software GTE (the prim-model renderer)
	inline void GteSetFarColour(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteStoreOTZ(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }      // GTE_ReadOTZ
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); } // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }         // set_param_with_dword_1CA8A68
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // root pool: 2 nodes of 0x10 bytes (RootTask, WaitTexRestore)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t done;      // +0x0E WaitTexRestore: texture restore finished
	};
	struct ControllerNode // effect pool: 0x64 nodes of 0x24 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E[0x16];
	};
	struct WaveNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C flipbook frame
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t pad16;     // +0x16 (the anchor's 4th word)
		int16_t roll;      // +0x18
		int16_t pad1A;
		int16_t scale;     // +0x1C
		uint8_t pad1E[6];
	};
	struct RingNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t angle;     // +0x16 about the model's z axis
		int16_t vel[3];    // +0x18 1/18 of caster anchor -> target
		int16_t spin;      // +0x1E
		int16_t scale;     // +0x20
		int16_t grow;      // +0x22
	};
	struct BurstNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		int16_t pos[4];    // +0x10 the target position (4th word copied along)
		uint8_t pad18[0xC];
	};
	struct Particle // pool 0x244BEA8, 100 x 0x18
	{
		uint32_t type;     // +0x00 0 = free, 1 / 2 / 4 = BurstA / B / C
		int16_t frame;     // +0x04 flipbook frame
		int16_t scale;     // +0x06
		int16_t pos[4];    // +0x08 (4th word copied along)
		uint8_t pad10[8];
	};
	// render header of the private prim-model renderer (0x6C bytes on the scratch stack)
	struct RenderHeader
	{
		uint32_t model;    // +0x00
		uint32_t verts;    // +0x04 vertex base (model + 8 unless flag 0x2000)
		uint8_t far_rgb[4];// +0x08 GTE far colour
		int32_t fade;      // +0x0C depth-cue level (IR0) with flag 0x40 / 0x80
		uint16_t u_off;    // +0x10 flags 0x400 (add) / 0x100 (set): packet word after uv1 / uv0
		uint16_t pad12;
		uint16_t v_off;    // +0x14 flags 0x800 (add) / 0x200 (set): packet word after uv0
		uint16_t pad16;
		int32_t depth;     // +0x18 added to the uv words (0 unless flag 0x1000)
		uint32_t flags;    // +0x1C
		uint32_t colour;   // +0x20 code / colour of the flat lists, colour scale of the gouraud lists
		uint32_t list;     // +0x24 current primitive list
		int32_t mac0;      // +0x28
		uint32_t pad2C;
		int32_t otz;       // +0x30
		uint32_t gte_flag; // +0x34
		uint8_t pad38[0x20];
		uint32_t scale_rgb;// +0x58 copy of +0x20 (gouraud lists)
		uint32_t c[4];     // +0x5C vertex colours (gouraud lists)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10, "Ultrasonic Waves root nodes");
	static_assert(sizeof(ControllerNode) == 0x24 && sizeof(WaveNode) == 0x24 && sizeof(RingNode) == 0x24 && sizeof(BurstNode) == 0x24, "Ultrasonic Waves nodes");
	static_assert(sizeof(Particle) == 0x18 && sizeof(RenderHeader) == 0x6C, "Ultrasonic Waves particle / render header");

	inline Particle *Parts() { return (Particle *)0x244BEA8; }
	static const int PART_COUNT = 100;
}
}

#ifdef FF8_FX_HELD
#include "mag152_ultrasonic_waves_held.h"
#endif

namespace ff8fx
{
namespace ultrasonic152
{
	template<typename T> static inline T &At(uint32_t base, uint32_t off) { return *(T *)(base + off); }

	// screen clip flags of one vertex (x out of 0..0xA00 -> bit, y out of 0..0x6C0 -> bit << 4)
	static inline bool OutX(uint32_t xy) { int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(uint32_t xy) { int16_t v = *(const int16_t *)(xy + 2); return v < 0 || v > 0x6C0; }

	// packet uv word offsets (flags 0x400 / 0x100 on `u_word`, 0x800 / 0x200 on `v_word`)
	static inline void UvOffsets(const RenderHeader *h, uint32_t pkt, uint32_t u_word, uint32_t v_word)
	{
		const uint32_t fl = h->flags;
		if (fl & 0x400) At<uint16_t>(pkt, u_word) = (uint16_t)(At<uint16_t>(pkt, u_word) + h->u_off);
		else if (fl & 0x100) At<uint16_t>(pkt, u_word) = h->u_off;
		if (fl & 0x800) At<uint16_t>(pkt, v_word) = (uint16_t)(At<uint16_t>(pkt, v_word) + h->v_off);
		else if (fl & 0x200) At<uint16_t>(pkt, v_word) = h->v_off;
	}

	// the gouraud lists scale their vertex colours by the header colour (x * c >> 7, bytes)
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

	// ------------------------------------------------------------------
	// Flat textured triangles (0x60B7A0): 0x14-byte records (code/rgb unused, vertex indices
	// +4/+6/+8, uv0/clut +0xC, uv1/tpage +0x10, uv2 = high half of +8) -> 0x20-byte packets
	// ------------------------------------------------------------------
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

	// ------------------------------------------------------------------
	// Flat textured quads (0x60BA00): 0x18-byte records (vertex indices +4/+6/+8/+0xA, uv0/clut
	// +0xC, uv1/tpage +0x10, uv2 / uv3 +0x14) -> 0x28-byte packets
	// ------------------------------------------------------------------
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

	// ------------------------------------------------------------------
	// Gouraud textured triangles (0x60BCB0): 0x1C-byte records (code + colour 0, vertex indices
	// +4/+6/+8, uv0/clut +0xC, uv1/tpage +0x10, colours 1 / 2 +0x14 / +0x18; uv2 = high half of +8)
	// -> 0x28-byte packets; semi-transparency from header flags 2 / 8, double-sided flag 0x20,
	// depth cue of the three colours with flag 0x80
	// ------------------------------------------------------------------
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

	// ------------------------------------------------------------------
	// Gouraud textured quads (0x60BFD0): 0x24-byte records (code + colour 0, vertex indices
	// +4/+6/+8/+0xA, uv0/clut +0xC, uv1/tpage +0x10, uv2 / uv3 +0x14, colours 1..3 +0x18..+0x20)
	// -> 0x34-byte packets (flags as the gouraud triangles)
	// ------------------------------------------------------------------
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

	// ------------------------------------------------------------------
	// Prim-model renderer (0x60B6C0): header h (0x6C bytes), OT, OT shift, packet cursor; the
	// model's list block starts with two skipped words, a third list type between the flat and
	// the gouraud lists is skipped too (2 words); an empty list is one zero word
	// ------------------------------------------------------------------
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

	// ------------------------------------------------------------------
	// Root task (0x60C8F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag152_ultrasonic_waves_held.inc
		FX_HELD(held_note_root();)
		if (r->counter & 1) PacketCursor() = TexBase() + 0x8000;
		else PacketCursor() = TexBase();
		int alive = ExecuteTaskQueue(QEffects());
		if (alive == 0)
		{
			RootNode *w = (RootNode *)AddTaskToQueue(QRoot(), ORIG_WaitTexRestore);
			w->counter = 0;
			w->done = 0;
		}
		r->counter++;
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Texture restore wait (0x60C960)
	// ------------------------------------------------------------------
	static uint32_t __cdecl WaitTexRestore(TaskNode *n)
	{
		RootNode *w = (RootNode *)n;
		if (w->counter == 1) TextureRestoreTask((void *)TexBase(), &w->done);
		int16_t done = w->done;
		w->counter++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Controller (0x60B070)
	// ------------------------------------------------------------------
	static uint32_t __cdecl Controller(TaskNode *n)
	{
		int32_t dir[3];
		dir[0] = TargetPos()[0] - Anchor()[0];
		dir[1] = TargetPos()[1] - Anchor()[1];
		dir[2] = TargetPos()[2] - Anchor()[2];
		int32_t down[3] = { 0, 0, -0x1000 };
		NormalizeVector(dir, dir);
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors(down, dir, axis);
		BuildAxisAngleRotationMatrix(angle, &Rot(), axis);
		if (DrawOnly()) return 0;
		ControllerNode *c = (ControllerNode *)n;
		if (c->counter == 0)
		{
			GetEffectSpawnPosition(Entity(CasterSlot()), 0xF0, 0x1000, Anchor());
			GetDefaultEffectPosition(Entity(TargetSlot()), TargetPos());
		}
		if (c->counter >= 1 && c->counter <= 0xE && c->counter % 5 == 1)
		{
			WaveNode *w = (WaveNode *)AddTaskToQueue(QEffects(), ORIG_WaveTask);
			memcpy(&w->pos[0], &Anchor()[0], 4);
			w->counter = 0;
			memcpy(&w->pos[2], &Anchor()[2], 4);
			w->pos[0] = (int16_t)(w->pos[0] + (int16_t)(CrtRand() % 0x6E - 0x37));
			w->pos[1] = (int16_t)(w->pos[1] + (int16_t)(CrtRand() % 0x6E - 0x37));
			w->pos[2] = (int16_t)(w->pos[2] + (int16_t)(CrtRand() % 0x6E - 0x37));
			w->roll = (int16_t)(CrtRand() % 0x1000);
			w->scale = (int16_t)(CrtRand() % 0x400 + 0x900);
		}
		if (c->counter >= 5 && c->counter <= 0x14)
		{
			RingNode *g = (RingNode *)AddTaskToQueue(QEffects(), ORIG_RingTask);
			memcpy(&g->pos[0], &Anchor()[0], 4);
			g->counter = 0;
			memcpy(&g->pos[2], &Anchor()[2], 4);
			g->angle = (int16_t)(CrtRand() % 0x1000);
			g->vel[0] = (int16_t)((TargetPos()[0] - Anchor()[0]) / 18);
			g->vel[1] = (int16_t)((TargetPos()[1] - Anchor()[1]) / 18);
			g->vel[2] = (int16_t)((TargetPos()[2] - Anchor()[2]) / 18);
			const int32_t r = CrtRand() % 0x28;
			g->grow = 0x97;
			g->scale = 0x97;
			g->spin = (int16_t)(r + 0xA);
		}
		if (c->counter == 0x19)
		{
			BurstNode *b = (BurstNode *)AddTaskToQueue(QEffects(), ORIG_BurstA);
			memcpy(&b->pos[0], &TargetPos()[0], 4);
			b->counter = 0;
			memcpy(&b->pos[2], &TargetPos()[2], 4);
		}
		if (c->counter == 0x1A)
		{
			BurstNode *b = (BurstNode *)AddTaskToQueue(QEffects(), ORIG_BurstB);
			memcpy(&b->pos[0], &TargetPos()[0], 4);
			b->counter = 0;
			memcpy(&b->pos[2], &TargetPos()[2], 4);
		}
		if (c->counter == 0x1A)
		{
			BurstNode *b = (BurstNode *)AddTaskToQueue(QEffects(), ORIG_BurstC);
			memcpy(&b->pos[0], &TargetPos()[0], 4);
			b->counter = 0;
			memcpy(&b->pos[2], &TargetPos()[2], 4);
		}
		if (c->counter == 0x23) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (c->counter == 1) BdPlaySE(SOUND_Waves, 0, 0x80);
		c->counter++;
		return c->counter > 0x28 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Wave (0x60B400): flipbook frame `counter`; from frame 8 draw mode 0xD with grey level
	// `level` (the task passes (12 - counter) * 32); returns the flipbook's next-frame word
	// (header +0x28, negative = last frame)
	// ------------------------------------------------------------------
	static int16_t WaveDraw(const WaveNode *p, int32_t level)
	{
		TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 3));
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = SEQ_Wave;
		*(uint16_t *)(h + 4) = (uint16_t)p->counter;
		*(int32_t *)(h + 8) = p->roll;
		*(uint16_t *)(h + 0x24) = 9;
		if (p->counter >= 8)
		{
			*(uint16_t *)(h + 0x24) = 0xD;
			const uint32_t c = (uint32_t)level;
			*(uint32_t *)(h + 0x1C) = (((c << 8) | c) << 8) | c;
		}
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
		// read after the free, as the original (the scratch keeps it)
		return *(const int16_t *)(h + 0x28);
	}

	static uint32_t __cdecl WaveTask(TaskNode *n)
	{
		WaveNode *p = (WaveNode *)n;
		// 30 fps layer: see mag152_ultrasonic_waves_held.inc
		FX_HELD(held_note_draw(ORIG_WaveTask, p);)
		const int16_t next = WaveDraw(p, shl32(0xC - p->counter, 5));
		if (DrawOnly()) return 0;
		p->counter++;
		return next < 0 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Ring (0x60B4C0): `colour` = RingColours[counter % 10], `fade` = (counter - 16) << 10 (used
	// from counter 16)
	// ------------------------------------------------------------------
	static void RingDraw(const RingNode *p, uint32_t colour, int32_t fade)
	{
		// the 4th angle word is never written by the original (a stack word)
		int16_t angles[4] = { 0, 0, p->angle, 0 };
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(angles, &m);
		m.t[0] = p->pos[0];
		m.t[1] = p->pos[1];
		m.t[2] = p->pos[2];
		int32_t v[3] = { p->scale, p->scale, p->scale };
		Scale3DMatrix(&m, v);
		MatrixMultiply(&Rot(), &m);
		MatrixMultiply(&Camera(), &m);
		TransformVectorBy3x3Matrix(&Camera(), m.t, m.t);
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		// towards the camera by size / 16
		NormalizeVector(m.t, v);
		const int32_t k = -(p->scale >> 4);
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(mul32(k, v[0]) >> 12));
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(mul32(k, v[1]) >> 12));
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(mul32(k, v[2]) >> 12));
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		RenderHeader *h = (RenderHeader *)AllocHeader(0x6C);
		h->far_rgb[0] = h->far_rgb[1] = h->far_rgb[2] = h->far_rgb[3] = 0;
		h->model = MODEL_Ring;
		h->flags = 0x33;
		h->colour = colour;
		if (p->counter >= 0x10)
		{
			h->flags = 0xF3;
			h->fade = fade;
		}
		PacketCursor() = RenderModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x6C);
	}

	static void RingUpdate(RingNode *p)
	{
		p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
		p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
		p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
		p->angle = (int16_t)(p->angle + p->spin);
		p->scale = (int16_t)(p->scale + p->grow);
	}

	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *p = (RingNode *)n;
		// 30 fps layer: see mag152_ultrasonic_waves_held.inc
		FX_HELD(held_note_draw(ORIG_RingTask, p);)
		RingDraw(p, RingColours[p->counter % 10], shl32(p->counter - 0x10, 10));
		if (DrawOnly()) return 0;
		RingUpdate(p);
		p->counter++;
		return p->counter > 0x14 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Bursts (0x60C3C0 / 0x60C580 / 0x60C740)
	// ------------------------------------------------------------------
	// draws every particle of `type` in `parts` with the flipbook `seq` (header +0x24 = mode) and,
	// on updating ticks, advances its frame (freed when its flipbook ends); returns how many of
	// them continue
	static int BurstDraw(Particle *parts, uint32_t seq, uint16_t mode, uint32_t type)
	{
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = seq;
		*(uint16_t *)(h + 0x24) = mode;
		int alive = 0;
		for (int i = 0; i < PART_COUNT; i++)
		{
			Particle &e = parts[i];
			if (!((uint8_t)e.type & type)) continue;
			*(uint16_t *)(h + 4) = (uint16_t)e.frame;
			TransformCameraByShadowRotation(e.pos, e.scale, -(e.scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
			if (DrawOnly()) continue;
			e.frame++;
			if (*(const int16_t *)(h + 0x28) < 0) e.type = 0;
			else alive++;
		}
		FieldFree(0xB4);
		return alive;
	}

	static Particle *FreeParticle()
	{
		for (int i = 0; i < PART_COUNT; i++)
			if (Parts()[i].type == 0) return &Parts()[i];
		return nullptr;
	}

	static uint32_t __cdecl BurstA(TaskNode *n)
	{
		BurstNode *b = (BurstNode *)n;
		// 30 fps layer: see mag152_ultrasonic_waves_held.inc
		FX_HELD(held_note_burst(ORIG_BurstA, b);)
		const int alive = BurstDraw(Parts(), SEQ_BurstA, 8, 1);
		if (DrawOnly()) return 0;
		if (b->counter >= 0 && b->counter <= 8)
		{
			for (int k = 0; k < 2; k++)
			{
				Particle *e = FreeParticle();
				if (!e) break;
				e->type = 1;
				e->frame = 0;
				e->scale = (int16_t)(CrtRand() % 0x700 + 0x300);
				memcpy(&e->pos[0], &b->pos[0], 8);
				e->pos[0] = (int16_t)(e->pos[0] + (int16_t)(CrtRand() % 0x320 - 0x190));
				e->pos[1] = (int16_t)(e->pos[1] + (int16_t)(CrtRand() % 0x320 - 0x190));
				e->pos[2] = (int16_t)(e->pos[2] + (int16_t)(CrtRand() % 0x320 - 0x190));
			}
		}
		b->counter++;
		return (b->counter >= 4 && alive == 0) ? TASK_END : 0;
	}

	static uint32_t __cdecl BurstB(TaskNode *n)
	{
		BurstNode *b = (BurstNode *)n;
		// 30 fps layer: see mag152_ultrasonic_waves_held.inc
		FX_HELD(held_note_burst(ORIG_BurstB, b);)
		const int alive = BurstDraw(Parts(), SEQ_BurstB, 0, 2);
		if (DrawOnly()) return 0;
		if (b->counter >= 0 && b->counter <= 0xD)
		{
			Particle *e = FreeParticle();
			if (e)
			{
				e->type = 2;
				e->frame = 0;
				e->scale = (int16_t)(CrtRand() % 0x400 + 0x300);
				memcpy(&e->pos[0], &b->pos[0], 8);
				e->pos[0] = (int16_t)(e->pos[0] + (int16_t)(CrtRand() % 0x384 - 0x1C2));
				e->pos[1] = (int16_t)(e->pos[1] + (int16_t)(CrtRand() % 0x384 - 0x1C2));
				e->pos[2] = (int16_t)(e->pos[2] + (int16_t)(CrtRand() % 0x384 - 0x1C2));
			}
		}
		b->counter++;
		return (b->counter >= 4 && alive == 0) ? TASK_END : 0;
	}

	static uint32_t __cdecl BurstC(TaskNode *n)
	{
		BurstNode *b = (BurstNode *)n;
		// 30 fps layer: see mag152_ultrasonic_waves_held.inc
		FX_HELD(held_note_burst(ORIG_BurstC, b);)
		const int alive = BurstDraw(Parts(), SEQ_BurstC, 0, 4);
		if (DrawOnly()) return 0;
		if (b->counter >= 0 && b->counter <= 0xD)
		{
			Particle *e = FreeParticle();
			if (e)
			{
				e->type = 4;
				e->frame = 0;
				e->scale = (int16_t)(CrtRand() % 0x500 + 0x500);
				memcpy(&e->pos[0], &b->pos[0], 8);
				e->pos[0] = (int16_t)(e->pos[0] + (int16_t)(CrtRand() % 0x384 - 0x1C2));
				e->pos[1] = (int16_t)(e->pos[1] + (int16_t)(CrtRand() % 0x384 - 0x1C2));
				e->pos[2] = (int16_t)(e->pos[2] + (int16_t)(CrtRand() % 0x384 - 0x1C2));
			}
		}
		b->counter++;
		return (b->counter >= 4 && alive == 0) ? TASK_END : 0;
	}
}

	void register_mag152_ultrasonic_waves()
	{
		register_port(ultrasonic152::ORIG_RootTask, (void *)ultrasonic152::RootTask, "U152 RootTask", 152);
		register_port(ultrasonic152::ORIG_WaitTexRestore, (void *)ultrasonic152::WaitTexRestore, "U152 WaitTexRestore", 152);
		register_port(ultrasonic152::ORIG_Controller, (void *)ultrasonic152::Controller, "U152 Controller", 152);
		register_port(ultrasonic152::ORIG_WaveTask, (void *)ultrasonic152::WaveTask, "U152 WaveTask", 152);
		register_port(ultrasonic152::ORIG_RingTask, (void *)ultrasonic152::RingTask, "U152 RingTask", 152);
		register_port(ultrasonic152::ORIG_BurstA, (void *)ultrasonic152::BurstA, "U152 BurstA", 152);
		register_port(ultrasonic152::ORIG_BurstB, (void *)ultrasonic152::BurstB, "U152 BurstB", 152);
		register_port(ultrasonic152::ORIG_BurstC, (void *)ultrasonic152::BurstC, "U152 BurstC", 152);
		// 30 fps layer: see mag152_ultrasonic_waves_held.inc
		FX_HELD(register_mag152_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag152_ultrasonic_waves_held.inc"
#endif
