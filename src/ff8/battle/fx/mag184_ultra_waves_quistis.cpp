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

// Effect 184: Ultra Waves (Quistis' Blue Magic, kernel.bin section 19 "Ultra Waves", magic +4 = 184;
// MAG_184_ULTRA_WAVES_QUISTIS*), cast by a party member (Quistis, her whip) on the enemies.
//
// The module 0x5C7FA0..0x5C9A30 is a copy of Ultra Waves 154 (mag154_ultra_waves.cpp, Caterchipillar's
// attack), function for function: 17 of its 19 functions are identical up to addresses (exe data
// -0x86678, module globals -0x147110; the setup, the root / texture tasks, the grid, the capture, the
// warp and the private prim-model renderer). The two that differ:
//   Controller (0x5C8070) - counter 1 binds the module's animation set (com data 0xD328B4, out block
//     0x22BD140) to the caster (BS_BindEntityModelFromComData 0x5022C0) and starts its chain
//     transformation 1 (QueueChainTransformation 0x505C00: Quistis' casting animation); the four
//     emitters and the sound come at counter 6 (spawn position of the caster's effect anchor 0xF1 with
//     flag 0x800 instead of 0x1000), the ScreenWarp at 0xE, the damage at 0x29, the task ends after 0x2D
//     (154: 1 / 8 / 0x24 / 0x28).
//   Emitter (0x5C8930) - a particle grows by 1/7 per tick (154: 1/6) from spawn scale 0x300 (154: 0x400).
//
// Structure (setup MAG_184_ULTRA_WAVES_QUISTIS 0x5C7FC0, file loader MAG_184_ULTRA_WAVES_QUISTIS_FL
// 0x5C7FA0 = the texture file named at 0xD348B8 (mag153.tim, the same file as 154) into 0x22D7DE8; code
// 0x5C7FA0..0x5C9A30). The setup keeps the context, the caster slot and the first target slot of action
// 0, sets up the root queue 0x22BD130 (2 x 0x10) with RootTask and the effect queue 0x22BDAC0
// (0x64 x 0x24) with the Controller, frees the 100 ring particles, starts camera animation 0xD32750
// and queues the TIM.
//   RootTask (0x5C99A0) - alternates the packet arena (0x22DB3C4 / 0x22E73C4, cursor 0x22F33C4),
//     runs the effect queue; when it is empty queues WaitTexRestore in the root queue and ends.
//   WaitTexRestore (0x5C9A00) - at counter 1 starts the engine texture restore task (on the even
//     packet arena 0x22DB3C4), ends when its done flag is set.
//   Controller (0x5C8070) - see above; nothing in the draw-only mode.
//   Emitter (0x5C8930) - draws every ring particle of its mask (pool 0x22BD160, 100 x 0x18) with the
//     module's prim-model renderer (ring 0xD34450, colour from the 16-entry cycle 0xD347B8, from life 6
//     faded), then advances it (freed at life 12). Counters 0..20 spawn one particle at the emitter.
//     Ends from counter 12 once none of its particles is left.
//   ScreenWarp (0x5C8440) - counters 0..34: re-renders the four stage groups into the capture area of
//     VRAM, displaces the 41 x 28 grid in depth by a travelling sine wave and draws it as 27 x 40
//     textured quads of the capture (colour 0xD346C8[counter]); counter 0 hides the stage groups from
//     the battle's own draw (bit 1 of 0x1D98991 + 0x2C k), 35 shows them again and ends.
// Every task but RootTask / WaitTexRestore tests the draw-only flags (battle_to_update_flags & 0x201).
// Entity writes: the caster's animation set and chain transformation (Controller counter 1, through the
// engine); camera: the setup's camera animation 0xD32750 (engine).
// Module globals: 0x22BD108..0x22F33C8 (target slot, root pool / queue, the bind block 0x22BD140,
// particle pool, effect queue and node pool, context, caster slot, grid quad records 0x22BE8E8, texture
// file, grid 0x22D7DF0, the draw offset 0x22DB3C0, the packet arenas and cursor 0x22F33C4).
// Exe data written: the OT links of the static capture packets 0xD34868 (2 x 0x18).

#include "mag_common.h"

namespace ff8fx
{
namespace ultra184
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &TargetSlot() { return var<int32_t>(0x22BD108); }       // first target of action 0 (written by the setup)
	inline TaskQueue *QRoot() { return (TaskQueue *)0x22BD130; }            // pool 0x22BD110, 2 x 0x10
	inline TaskQueue *QEffects() { return (TaskQueue *)0x22BDAC0; }         // pool 0x22BDAD0, 0x64 x 0x24
	inline CastContext *&Ctx() { return var<CastContext *>(0x22BE8E0); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x22BE8E4); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x22F33C4); }
	inline uint32_t &FrameArena() { return var<uint32_t>(0x1D8E054); }     // battle frame packet arena
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }
	inline uint8_t DisplayBuffer() { return var<uint8_t>(0x1D96A80); }

	static const uint32_t ARENA_EVEN = 0x22DB3C4, ARENA_ODD = 0x22E73C4;
	static const uint32_t GRID = 0x22D7DF0;        // 28 rows x 41 vertices of 0xC bytes
	static const uint32_t GRID_END = 0x22DB3C0;
	static const uint32_t GRID_BYTES = GRID_END - GRID;
	static const uint32_t QUADS = 0x22BE8E8;       // 27 rows x 40 quads of 0x60 bytes
	static const uint32_t QUADS_BYTES = 27 * 40 * 0x60;
	static const uint32_t OFFSET_XY = 0x22DB3C0;   // draw offset of the capture's offset packet
	static const uint32_t PKT_Capture = 0xD34868;  // 2 x 0x18 (per display buffer), alternate viewport
	static const uint32_t DRAWENV = 0x1D969C8;     // 2 x 0x5C
	static const uint32_t STAGE_Anim0 = 0x1D989A4; // stage group k: animation header 0x1D989A4 + 0x2C k (group = -0x10, flags byte -0x13)
	static const uint32_t PARTS = 0x22BD160;       // 100 x 0x18
	static const int PART_COUNT = 100;
	static const int32_t WARP_TICKS = 0x23;

	static const uint32_t ORIG_RootTask = 0x5C99A0;
	static const uint32_t ORIG_WaitTexRestore = 0x5C9A00;
	static const uint32_t ORIG_Controller = 0x5C8070;
	static const uint32_t ORIG_Emitter = 0x5C8930;
	static const uint32_t ORIG_ScreenWarp = 0x5C8440;
	static const uint32_t MODEL_Ring = 0xD34450;
	static const uint32_t *const WarpColours = (const uint32_t *)0xD346C8;  // 0x23 quad colours
	static const uint32_t EMITTER_Vectors = 0xD34778;                       // 4 x (offset SVECTOR, direction SVECTOR)
	static const uint32_t *const RingColours = (const uint32_t *)0xD347B8;  // 16 x (code / rgb)
	static const int32_t *const StageDraw = (const int32_t *)0xD347F8;      // 4 x (OT word offset, RenderGeometry mode)
	static const void *const SOUND_Waves = (const void *)0xD346C0;
	static const void *const CAMERA_Anim = (const void *)0xD32750;          // (setup)
	static const uint32_t BIND_Block = 0x22BD140;                          // 0x20-byte block BS_BindEntityModelFromComData fills
	static const uint32_t COM_Anim = 0xD328B4;                             // the module's animation set (com data)

	// engine functions not in fx_port.h / mag_common.h
	inline void BindEntityModelFromComData(uint8_t *entity, uint32_t out, uint32_t data) { fn<void (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData
	inline void QueueChainTransformation(uint8_t *entity, int32_t id) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, id); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void MatVec(const void *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const void *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }   // matrixMultiplyVector (rotation only)
	inline void NormalizeSVector(const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const int16_t *, int16_t *)>(0x56BDE0)(in, out); }      // sub_56BDE0
	inline void MulMatrixInPlace(Mat4x3 *m, const Mat4x3 *n) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(m, n); }               // sub_56C220
	inline int32_t Cos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }                                                   // computeCosine
	inline void BuildDrawEnvPacket(uint32_t packet, uint32_t env) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C0F0)(packet, env); }        // sub_45C0F0
	inline void BuildDrawAreaPacket(uint32_t packet, const int16_t *rect) { fn<void (__cdecl *)(uint32_t, const int16_t *)>(0x45C940)(packet, rect); } // sub_45C940
	inline void BuildDrawOffsetPacket(uint32_t packet, uint32_t xy) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C9B0)(packet, xy); }      // sub_45C9B0
	inline void InsertPrimAltViewport(uint32_t bucket, uint32_t packet) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C8E0)(bucket, packet); }
	inline void GetScreenOffset(int32_t *x, int32_t *y) { fn<void (__cdecl *)(int32_t *, int32_t *)>(0x56CD10)(x, y); }                    // sub_56CD10
	inline void SetScreenOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x56CCE0)(x, y); }                          // Call_Bs_ParseCamera
	inline uint32_t RenderGeometry(uint32_t group, uint32_t header, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, int32_t, uint32_t)>(0x5099D0)(group, header, ot, mode, cursor); }
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
		int16_t pad0E;
		int16_t anchor[4]; // +0x10 caster's effect anchor 0xF1 (4th word: the spawn position's)
		uint8_t pad18[0xC];
	};
	struct EmitterNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint16_t mask;     // +0x0E particle owner bit (1 / 4 / 0x10 / 0x40)
		int16_t pos[3];    // +0x10
		int16_t pad16;     // +0x16 never written (copied along into the particle's +0x0E, overwritten)
		int16_t dir[3];    // +0x18 unit flight direction
		int16_t pad1E;
		int16_t angle;     // +0x20 spin of the next particle
		int16_t colour;    // +0x22 colour cycle index of the next particle
	};
	struct WarpNode
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E[0x16];
	};
	struct Particle // pool 0x22BD160, 100 x 0x18
	{
		uint16_t mask;     // +0x00 owner emitter bit, 0 = free
		int16_t colour;    // +0x02 colour cycle index (0..15)
		int16_t life;      // +0x04
		int16_t scale;     // +0x06
		int16_t pos[3];    // +0x08
		int16_t angle;     // +0x0E spin about the emitter direction
		int16_t vel[3];    // +0x10
		int16_t radius;    // +0x16 distance from the axis
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
	// the Emitter's matrix work area (0x88 bytes on the scratch stack)
	struct EmitterWork
	{
		int16_t offs[4];   // +0x00 radius vector (4th word never written)
		Mat4x3 m;          // +0x08 flight direction, then camera
		Mat4x3 spin;       // +0x28 spin about the direction (translation never written)
		int32_t scale[4];  // +0x48 (4th never written)
		int32_t from[4];   // +0x58 (0, 0, -1)
		int32_t to[4];     // +0x68 emitter direction
		int32_t axis[4];   // +0x78 (4th never written)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10, "Ultra Waves (Quistis) root nodes");
	static_assert(sizeof(ControllerNode) == 0x24 && sizeof(EmitterNode) == 0x24 && sizeof(WarpNode) == 0x24, "Ultra Waves nodes");
	static_assert(sizeof(Particle) == 0x18 && sizeof(RenderHeader) == 0x6C && sizeof(EmitterWork) == 0x88, "Ultra Waves particle / render header / work");

	inline Particle *Parts() { return (Particle *)PARTS; }
}
}

#ifdef FF8_FX_HELD
#include "mag184_ultra_waves_quistis_held.h"
#endif

namespace ff8fx
{
namespace ultra184
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
	// Flat textured triangles (0x5C8D80): 0x14-byte records (code/rgb unused, vertex indices
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
	// Flat textured quads (0x5C8FE0): 0x18-byte records (vertex indices +4/+6/+8/+0xA, uv0/clut
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
	// Gouraud textured triangles (0x5C9290): 0x1C-byte records (code + colour 0, vertex indices
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
	// Gouraud textured quads (0x5C95B0): 0x24-byte records (code + colour 0, vertex indices
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
	// Prim-model renderer (0x5C8CA0, the same code as Ultrasonic Waves' 0x60B6C0 and Ultra Waves 154's 0x608890): header h (0x6C
	// bytes), OT, OT shift, packet cursor; the model's list block starts with two skipped words, a
	// third list type between the flat and the gouraud lists is skipped too (2 words); an empty
	// list is one zero word
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
	// Root task (0x5C99A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag184_ultra_waves_quistis_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = ARENA_ODD;
		if (!(r->counter & 1)) PacketCursor() = ARENA_EVEN;
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
	// Texture restore wait (0x5C9A00)
	// ------------------------------------------------------------------
	static uint32_t __cdecl WaitTexRestore(TaskNode *n)
	{
		RootNode *w = (RootNode *)n;
		if (w->counter == 1) TextureRestoreTask((void *)ARENA_EVEN, &w->done);
		int16_t done = w->done;
		w->counter++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// 0x5C82B0: the 41 x 28 screen grid (x -164..156, y -112..104 in steps of 8, z 0; flags 1 on
	// the top / bottom rows, 2 on the left / right columns) and its 27 x 40 quad records: vertex
	// pointers and, per display buffer, a POLY_FT4 of the capture (tpage 0x120 | (0x16 + x / 64),
	// u = x % 64, v = y; the last column / row samples 7 texels instead of 8)
	// ------------------------------------------------------------------
	static void GridInit()
	{
		uint32_t v = GRID;
		for (int32_t row = 0; row < 0x1C; row++)
			for (int32_t col = 0; col < 0x29; col++, v += 0xC)
			{
				At<int16_t>(v, 4) = (int16_t)(-0xA4 + 8 * col);
				At<int16_t>(v, 6) = (int16_t)(-0x70 + 8 * row);
				At<int16_t>(v, 8) = 0;
				At<int16_t>(v, 0xA) = 0;
				if (row == 0 || row == 0x1B) At<int16_t>(v, 0xA) = 1;
				if (col == 0 || col == 0x28) At<int16_t>(v, 0xA) |= 2;
			}
		uint32_t vtx = GRID;
		uint32_t rec = QUADS;
		for (int32_t row = 0; row < 0x1B; row++)
		{
			const int32_t v0 = 8 * row;
			int32_t v1 = v0 + 8;
			if (v1 >= 0xD8) v1--;
			for (int32_t col = 0; col < 0x28; col++, rec += 0x60)
			{
				const int32_t x = 8 * col;
				At<uint32_t>(rec, 0) = vtx;
				At<uint32_t>(rec, 4) = vtx + 0xC;
				At<uint32_t>(rec, 8) = vtx + 0x1EC;
				At<uint32_t>(rec, 0xC) = vtx + 0x1F8;
				vtx += 0xC;
				const uint8_t u0 = (uint8_t)(x % 64);
				const uint16_t tpage = (uint16_t)((x / 64 + 0x16) | 0x120);
				const uint8_t u1 = (uint8_t)(x + 8 >= 0x140 ? u0 + 7 : u0 + 8);
				for (uint32_t p = rec + 0x10; p < rec + 0x60; p += 0x28)
				{
					At<uint32_t>(p, 0) = 0x9000000;
					At<uint32_t>(p, 4) = 0x2C808080;
					At<uint8_t>(p, 0xC) = u0;
					At<uint8_t>(p, 0xD) = (uint8_t)v0;
					At<uint8_t>(p, 0x14) = u1;
					At<uint8_t>(p, 0x15) = (uint8_t)v0;
					At<uint16_t>(p, 0x16) = tpage;
					At<uint8_t>(p, 0x1C) = u0;
					At<uint8_t>(p, 0x1D) = (uint8_t)v1;
					At<uint8_t>(p, 0x24) = u1;
					At<uint8_t>(p, 0x25) = (uint8_t)v1;
				}
			}
			vtx += 0xC;
		}
	}

	// 0x5C84E0 / 0x5C84C0: the stage groups' "drawn by the stage task" bit off / on
	static void StageHide()
	{
		for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) At<uint8_t>(a, 0) &= 0xFD;
	}
	static void StageShow()
	{
		for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) At<uint8_t>(a, 0) |= 2;
	}

	// ------------------------------------------------------------------
	// 0x5C8500: renders the four stage groups into the capture area (0x180, 0x100, 320 x 216)
	// through the other buffer's draw environment with the screen offset at the area centre, then
	// the per-buffer static packet `capture_packets` (alternate viewport) and the draw area / offset
	// packets of the area (`offset_xy`: the offset packet's position words)
	// ------------------------------------------------------------------
	static void Capture(uint32_t capture_packets, uint32_t offset_xy)
	{
		int16_t rect[4] = { 0x180, 0x100, 0x140, 0xD8 };
		uint32_t pkt = PacketCursor();
		BuildDrawEnvPacket(pkt, DRAWENV + 0x5C * ((uint32_t)DisplayBuffer() ^ 1));
		InsertPrimAutoDepth(OTBase() + 0x406C, (void *)pkt);
		pkt += 0x40;
		PacketCursor() = pkt;
		int32_t ofx, ofy;
		GetScreenOffset(&ofx, &ofy);
		SetScreenOffset(rect[2] / 2, rect[3] / 2);
		const uint32_t h = (uint32_t)FieldAlloc(0x54);
		At<int16_t>(h, 0x3C) = 0;
		At<int16_t>(h, 0x3E) = 0;
		At<int16_t>(h, 0x40) = 0x140;
		At<int16_t>(h, 0x42) = 0xD8;
		At<uint32_t>(h, 0x2C) = var<uint32_t>(0x1D98B3C);
		At<uint32_t>(h, 0x38) = var<uint32_t>(0x1D969A8);
		At<int32_t>(h, 0x48) = -1;
		for (int k = 0; k < 4; k++)
		{
			const uint32_t anim = STAGE_Anim0 + 0x2C * (uint32_t)k;
			BuildBoneMatricesFromPose((void *)anim);
			ComputeBonesWorldMatrices((void *)anim, &Camera());
			if (At<uint8_t>(anim, (uint32_t)-0x13) & 1)
			{
				At<uint32_t>(h, 0x44) = At<uint32_t>(anim, 0x14);
				At<uint16_t>(h, 0x4C) = At<uint16_t>(anim, (uint32_t)-0x12);
				FrameArena() = RenderGeometry(anim - 0x10, h + 0x28, OTBase() + (uint32_t)StageDraw[2 * k] * 4, StageDraw[2 * k + 1], FrameArena());
			}
		}
		FieldFree(0x54);
		SetScreenOffset(ofx, ofy);
		InsertPrimAltViewport(OTBase() + 0x4484, capture_packets + 0x18 * (uint32_t)DisplayBuffer());
		pkt = PacketCursor();
		BuildDrawAreaPacket(pkt, rect);
		InsertPrimAutoDepth(OTBase() + 0x4484, (void *)pkt);
		pkt += 0xC;
		At<int16_t>(offset_xy, 0) = rect[0];
		At<int16_t>(offset_xy, 2) = rect[1];
		BuildDrawOffsetPacket(pkt, offset_xy);
		InsertPrimAutoDepth(OTBase() + 0x4484, (void *)pkt);
		pkt += 0xC;
		PacketCursor() = pkt;
	}

	// x / 41 and x / 5 with the compiler's magic numbers (truncating)
	static inline int32_t Div41(int32_t x) { int32_t h = (int32_t)(((int64_t)x * 0x63E7063F) >> 32) >> 4; return h + (int32_t)((uint32_t)h >> 31); }
	static inline int32_t Div5(int32_t x) { int32_t h = (int32_t)(((int64_t)x * 0x66666667) >> 32) >> 1; return h + (int32_t)((uint32_t)h >> 31); }

	// ------------------------------------------------------------------
	// 0x5C86E0: depth of every inner grid vertex for counter c of n ticks: a sine wave travelling
	// with c (phase c * (c / 2 + 208)), its amplitude sin(sin(c / n)) * 48 / 4096, bent per column
	// ------------------------------------------------------------------
	static void GridWave(int32_t c, int32_t n)
	{
		const int32_t phase = c * ((c >> 1) + 0xD0);
		int32_t s = ComputeSin(shl32(c, 10) / n);
		s = shl32(mul32(s, n) >> 12, 12) / n;
		const int32_t amp = mul32(ComputeSin(s), 48) >> 12;
		uint32_t v = GRID;
		for (int32_t rp = 0; rp < 0x7000; rp += 0x400)
			for (int32_t ca = 0; ca < 0x3D800; ca += 0x1800, v += 0xC)
			{
				if (At<uint16_t>(v, 0xA) & 3) continue;
				const int32_t a = Div41(ca);
				const int32_t q = ComputeSin(shl32(mul32(Cos(a), 3), 9) >> 12);
				int32_t e = (shl32(mul32(q, 7), 7) >> 12) + rp;
				e = Div5(mul32(e, 3));
				e += shl32(mul32(ComputeSin(a), 11), 7) >> 12;
				e += phase;
				At<int16_t>(v, 8) = (int16_t)(mul32(ComputeSin(e), amp) >> 12);
			}
	}

	// 0x5C8900: identity rotation (translation left as it is)
	static void IdentityRotation(Mat4x3 *m)
	{
		uint32_t *w = (uint32_t *)m;
		w[3] = 0;
		w[2] = 0;
		w[1] = 0;
		w[0] = 0;
		m->m[2][2] = 0x1000;
		m->m[1][1] = 0x1000;
		m->m[0][0] = 0x1000;
	}

	// ------------------------------------------------------------------
	// 0x5C8800: projects the grid (identity rotation, translation (4, 4, H)) and inserts its quads
	// (packet of display buffer `buffer`, colour `colour`) from the records `quads`
	// ------------------------------------------------------------------
	static void GridDraw(uint32_t quads, uint32_t buffer, uint32_t colour)
	{
		Mat4x3 m;
		m.pad = 0;
		IdentityRotation(&m);
		m.t[0] = 4;
		m.t[1] = 4;
		m.t[2] = var<int16_t>(0x1D8E038);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		for (uint32_t v = GRID; v < GRID_END; v += 0xC)
		{
			GteLoadV0((const void *)(v + 4));
			GteRTPS();
			GteReadSXY2((void *)v);
		}
		const uint32_t code = colour | 0x2C000000;
		uint32_t rec = quads;
		for (int row = 0; row < 0x1B; row++)
			for (int col = 0; col < 0x28; col++, rec += 0x60)
			{
				const uint32_t p = rec + 0x18 + buffer * 0x28;
				At<uint32_t>(p, (uint32_t)-4) = code;
				At<uint32_t>(p, 0) = At<uint32_t>(At<uint32_t>(rec, 0), 0);
				At<uint32_t>(p, 8) = At<uint32_t>(At<uint32_t>(rec, 4), 0);
				At<uint32_t>(p, 0x10) = At<uint32_t>(At<uint32_t>(rec, 8), 0);
				At<uint32_t>(p, 0x18) = At<uint32_t>(At<uint32_t>(rec, 0xC), 0);
				InsertPrimAutoDepth(OTBase() + 0x4068, (void *)(p - 8));
			}
	}

	// ------------------------------------------------------------------
	// Controller (0x5C8070)
	// ------------------------------------------------------------------
	static uint32_t __cdecl Controller(TaskNode *n)
	{
		// 30 fps layer: see mag184_ultra_waves_quistis_held.inc
		FX_HELD(held_note_controller(n);)
		if (DrawOnly()) return 0;
		ControllerNode *c = (ControllerNode *)n;
		if (c->counter == 0) GridInit();
		if (c->counter == 1)
		{
			BindEntityModelFromComData(Entity(CasterSlot()), BIND_Block, COM_Anim);
			QueueChainTransformation(Entity(CasterSlot()), 1);
		}
		if (c->counter == 6)
		{
			for (int k = 0; k < 4; k++)
			{
				const uint32_t vec = EMITTER_Vectors + 0x10 * (uint32_t)k;
				EmitterNode *e = (EmitterNode *)AddTaskToQueue(QEffects(), ORIG_Emitter);
				e->counter = 0;
				e->mask = (uint16_t)(1u << (2 * k));
				GetEffectSpawnPosition(Entity(CasterSlot()), 0xF1, 0x800, c->anchor);
				int16_t v[4];
				MatVec(Entity(CasterSlot()) + 0x40, (const int16_t *)vec, v);
				e->pos[0] = (int16_t)(c->anchor[0] + v[0]);
				e->pos[1] = (int16_t)(v[1] + c->anchor[1]);
				e->pos[2] = (int16_t)(c->anchor[2] + v[2]);
				MatVec(Entity(CasterSlot()) + 0x40, (const int16_t *)(vec + 8), v);
				e->dir[0] = (int16_t)(c->anchor[0] - e->pos[0] + v[0]);
				e->dir[1] = (int16_t)(v[1] - e->pos[1] + c->anchor[1]);
				e->dir[2] = (int16_t)(c->anchor[2] - e->pos[2] + v[2]);
				NormalizeSVector(e->dir, e->dir);
				e->angle = (int16_t)(CrtRand() % 0x1000);
				e->colour = 0;
			}
		}
		if (c->counter == 0xE)
		{
			WarpNode *w = (WarpNode *)AddTaskToQueue(QEffects(), ORIG_ScreenWarp);
			w->counter = 0;
		}
		if (c->counter == 0x29) ApplyActionResultToTargets(Ctx()->actions->targets, Ctx()->actions->target_count);
		if (c->counter == 6) BdPlaySE(SOUND_Waves, 0, 0x80);
		c->counter++;
		return c->counter > 0x2D ? TASK_END : 0;
	}


	// ------------------------------------------------------------------
	// Emitter (0x5C8930)
	// ------------------------------------------------------------------
	// the draw header / work area set-up before the particle loop
	static void EmitterBegin(RenderHeader *h, EmitterWork *w, const EmitterNode *e)
	{
		h->far_rgb[0] = h->far_rgb[1] = h->far_rgb[2] = h->far_rgb[3] = 0;
		w->from[0] = 0;
		w->from[1] = 0;
		h->model = MODEL_Ring;
		h->flags = 0x33;
		w->from[2] = -0x1000;
		w->to[0] = e->dir[0];
		w->to[1] = e->dir[1];
		w->to[2] = e->dir[2];
	}

	// one particle: `colour` = RingColours[colour index], `fade` = (life - 6) * 682 (used from
	// life 6; the flags stay 0xF3 for the following particles of the loop)
	static void ParticleDraw(RenderHeader *h, EmitterWork *w, const Particle &p, uint32_t colour, int32_t fade)
	{
		w->m.t[2] = p.pos[2];
		w->m.t[0] = p.pos[0];
		w->m.t[1] = p.pos[1];
		const int32_t angle = RotationBetweenVectors(w->from, w->to, w->axis);
		BuildAxisAngleRotationMatrix(angle, &w->m, w->axis);
		BuildAxisAngleRotationMatrix(p.angle, &w->spin, w->to);
		MulMatrixInPlace(&w->spin, &w->m);
		w->offs[1] = 0;
		w->offs[2] = 0;
		w->offs[0] = p.radius;
		MatVec(&w->spin, w->offs, w->offs);
		w->m.t[0] = (int32_t)((uint32_t)w->m.t[0] + (uint32_t)(int32_t)w->offs[0]);
		w->m.t[1] = (int32_t)((uint32_t)w->m.t[1] + (uint32_t)(int32_t)w->offs[1]);
		w->m.t[2] = (int32_t)((uint32_t)w->m.t[2] + (uint32_t)(int32_t)w->offs[2]);
		w->scale[2] = p.scale;
		w->scale[1] = p.scale;
		w->scale[0] = p.scale;
		Scale3DMatrix(&w->m, w->scale);
		ComposeAffineTransform(&Camera(), &w->m, &w->m);
		GteSetRotMatrix(&w->m);
		GteSetTransVector(&w->m);
		h->colour = colour;
		if (p.life >= 6)
		{
			h->fade = fade;
			h->flags |= 0xC0;
		}
		PacketCursor() = RenderModel(h, OTBase() + 0x44, 2, PacketCursor());
	}

	static inline int32_t ParticleFade(int32_t life) { return (life - 6) * 682; }

	// the update of a drawn particle; false = freed (life 12)
	static bool ParticleUpdate(Particle &p)
	{
		p.life++;
		if (p.life >= 0xC)
		{
			p.mask = 0;
			return false;
		}
		p.colour = (int16_t)(((uint8_t)p.colour + 1) & 0xF);
		p.scale = (int16_t)(p.scale + p.scale / 7);
		p.pos[0] = (int16_t)(p.pos[0] + p.vel[0]);
		p.pos[1] = (int16_t)(p.pos[1] + p.vel[1]);
		p.pos[2] = (int16_t)(p.pos[2] + p.vel[2]);
		p.angle = (int16_t)(p.angle + 0x200);
		p.radius = (int16_t)(p.radius + 0xA);
		return true;
	}

	// counters 0..20: one particle at the emitter (first free slot of the pool)
	static void EmitterSpawn(EmitterNode *e, Particle *parts)
	{
		if (e->counter < 0 || e->counter > 0x14) return;
		for (int i = 0; i < PART_COUNT; i++)
		{
			Particle &p = parts[i];
			if (p.mask != 0) continue;
			p.mask = e->mask;
			p.colour = e->colour;
			memcpy(&p.pos[0], &e->pos[0], 4);
			p.life = 0;
			p.scale = 0x300;
			memcpy(&p.pos[2], &e->pos[2], 4);
			p.angle = e->angle;
			p.vel[0] = (int16_t)(mul32(e->dir[0], 150) >> 12);
			p.vel[1] = (int16_t)(mul32(e->dir[1], 150) >> 12);
			p.vel[2] = (int16_t)(mul32(e->dir[2], 150) >> 12);
			p.radius = 100;
			return;
		}
	}

	static void EmitterAdvance(EmitterNode *e)
	{
		e->angle = (int16_t)(e->angle + 0x100);
		e->colour = (int16_t)(((uint8_t)e->colour - 1) & 0xF);
	}

	static uint32_t __cdecl Emitter(TaskNode *n)
	{
		EmitterNode *e = (EmitterNode *)n;
		// 30 fps layer: see mag184_ultra_waves_quistis_held.inc
		FX_HELD(held_note_emitter(e);)
		RenderHeader *h = (RenderHeader *)FieldAlloc(0x6C);
		EmitterWork *w = (EmitterWork *)FieldAlloc(0x88);
		int alive = 0;
		EmitterBegin(h, w, e);
		for (int i = 0; i < PART_COUNT; i++)
		{
			Particle &p = Parts()[i];
			if (!(e->mask & p.mask)) continue;
			ParticleDraw(h, w, p, RingColours[p.colour], ParticleFade(p.life));
			if (DrawOnly()) continue;
			if (ParticleUpdate(p)) alive++;
		}
		FieldFree(0x88);
		FieldFree(0x6C);
		if (DrawOnly()) return 0;
		EmitterSpawn(e, Parts());
		EmitterAdvance(e);
		e->counter++;
		if (e->counter < 0xC) return 0;
		return alive == 0 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// ScreenWarp (0x5C8440)
	// ------------------------------------------------------------------
	static void WarpDraw(int32_t c, uint32_t capture_packets, uint32_t offset_xy, uint32_t quads)
	{
		Capture(capture_packets, offset_xy);
		GridWave(c, WARP_TICKS);
		GridDraw(quads, DisplayBuffer(), WarpColours[c]);
	}

	static uint32_t __cdecl ScreenWarp(TaskNode *n)
	{
		WarpNode *w = (WarpNode *)n;
		if (w->counter < WARP_TICKS)
		{
			// 30 fps layer: see mag184_ultra_waves_quistis_held.inc
			FX_HELD(held_note_warp(w);)
			WarpDraw(w->counter, PKT_Capture, OFFSET_XY, QUADS);
		}
		if (DrawOnly()) return 0;
		if (w->counter == 0) StageHide();
		if (w->counter == WARP_TICKS) StageShow();
		w->counter++;
		return w->counter > WARP_TICKS ? TASK_END : 0;
	}
}

	void register_mag184_ultra_waves_quistis()
	{
		register_port(ultra184::ORIG_RootTask, (void *)ultra184::RootTask, "U184 RootTask", 184);
		register_port(ultra184::ORIG_WaitTexRestore, (void *)ultra184::WaitTexRestore, "U184 WaitTexRestore", 184);
		register_port(ultra184::ORIG_Controller, (void *)ultra184::Controller, "U184 Controller", 184);
		register_port(ultra184::ORIG_Emitter, (void *)ultra184::Emitter, "U184 Emitter", 184);
		register_port(ultra184::ORIG_ScreenWarp, (void *)ultra184::ScreenWarp, "U184 ScreenWarp", 184);
		// 30 fps layer: see mag184_ultra_waves_quistis_held.inc
		FX_HELD(register_mag184_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag184_ultra_waves_quistis_held.inc"
#endif
