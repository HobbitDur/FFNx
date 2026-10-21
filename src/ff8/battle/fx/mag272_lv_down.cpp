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
// Effect 272: LV Down (command ability LV Down: kernel.bin command ability data entry 6 +0 = 272; MAG_272_*). No
// other kernel.bin entry uses effect 272. LV Up (effect 271, mag271_lv_up.cpp) is a sibling module built from the
// same source (its own globals, layout and renderer copies); both share the target bounding-box walker 0x68DF50,
// ported here (lv27x::ModelBounds).
//
// Structure (setup MAG_272 0x68DC70 -> 0x68DCA0, not ported; file loader 0x68DC80 = the texture file named at
// 0x11162F8 -> [0x2508544]; the setup keeps the cast context (0x250854C) and the caster's entity (0x2508540, never
// read again), queues the TIM, starts the root task in its 1-node pool 0x2508550 and plays the camera animation
// 0x1115968 (Battle_PlayCameraAnimation 0x5099A0: the engine's camera script, no module task writes the camera)):
//   RootTask (0x68DD20) - alternates the packet arena (magic buffer + 0x504 / + 0x10504 by tick parity, cursor
//     0x2508564); on counter 1 sets up the play pool (1 x 0x4FC bytes at the start of the magic buffer, once) and
//     starts the play task on the first target of action [root +0x0E] (then +0x0E counts up: one play task in all):
//     the target's bounding box (0x68DEE0: bone world matrices 0x5095B0, every vertex of its model through its
//     bone, 0x68DF50; bone matrices rebuilt from the pose 0x508C90 afterwards), its default effect position
//     (0x571400) with y = the box's middle height, the block scale = the box's extents (x = z = the largest of
//     the three, y = the height), the layout 0x111122C (0x4D0 bytes), sound 0x1115964. Every tick: the
//     orthonormalised camera rotation 0x2508568 (0x67AAE0), then the play queue; ends when it is empty.
//   PlayTask (0x68E090) - plays the layout (prim player 0x701970, callback 0x68E140) with a stack block: the anchor,
//     the extents as scale, word +0x14 = 1 (offsets and matrices scaled), depth -(target +0x26) >> 4, the texture
//     scroll counter -counter (word +0x1C, only its low byte is read), the morph buffer (magic buffer + 0x4FC: 8
//     bytes below packet arena A); when the layout has finished: the action result of action 0's first target
//     (ApplyActionResultToTarget 0x506690) and the task ends.
//   PrimCallback (0x68E140) - one object of the layout: vertex frame / blend (0x701390), rotation (record flag
//     0x400: 0x701430, else 0x701310), offset and matrix scaled by the block's scale; record flag 0x1000: the
//     anchor through the camera + the offset through the orthonormalised camera 0x2508568, the rotation composed
//     with that camera; 0x200: the anchor through the camera + the offset; else the offset + the anchor through
//     the camera, composed with it; + the camera translation; record scale (scale3DMatrix); fade; then by the
//     record's object index: 0 / 1 the scrolled-mesh renderer 0x68EB80 (texture windows +0x24 / +0x2C, V scroll
//     (scroll byte << (object + 3)) & 0x7F wrapping by 0x80), 2 / 4 the plain renderer 0x68E570 (object 2 at twice
//     the depth), 3 (4 x depth) and the others 0x7043B0; the depth (header +0x18) is added to every OTZ (clamped
//     at 0) by the module renderers.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// ENTITY / CAMERA WRITES: the root's bounding box (tick 1 only) rebuilds the target's bone matrices (0x5095B0 world
// matrices, then 0x508C90 from its pose: entity +0x60 animation header's bone buffer); no task writes an entity's
// position or the camera words (the camera matrix 0x1D97778 is only read); the level change is the engine's
// ApplyActionResultToTarget.
// Module globals: 0x2508540..0x25085A8 (caster entity 0x2508540, texture file 0x2508544, magic buffer base 0x2508548,
// context 0x250854C, root pool 0x2508550 (1 x 0x14), packet cursor 0x2508564, orthonormalised camera 0x2508568,
// play queue 0x2508588, root queue 0x2508598). The play pool, the morph buffer and the packet arenas are in the
// magic buffer.
// NEVER-WRITTEN WORDS: the play block's word +0x1E (only the low byte of +0x1C is read); the callback's matrix pad
// +0x12 (GTE R33 high half only) and offset word +6 (GTE VZ0 high half only); the anchor's 4th word (the 4th word
// of 0x571400, GTE VZ0 high half only).

#include "mag_common.h"

namespace ff8fx
{
namespace lv27x
{
	// MAG_272_sub_68DF50 (shared with LV Up 271): the box (block +0x14 min x y z, +0x1A max x y z) of every vertex of
	// a battle model through its bone's world matrix
	void ModelBounds(uint8_t *block, uint8_t *anim_header);
}
namespace lv272
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline CastContext *&Ctx() { return var<CastContext *>(0x250854C); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2508548); }          // Magic_TextureOFF (magic buffer)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2508564); }
	inline Mat4x3 &CameraN() { return var<Mat4x3>(0x2508568); }               // orthonormalised camera rotation
	inline TaskQueue *QPlay() { return (TaskQueue *)0x2508588; }              // pool: magic buffer, 1 x 0x4FC

	static const uint32_t ORIG_RootTask = 0x68DD20;
	static const uint32_t ORIG_PlayTask = 0x68E090;
	static const uint32_t MODEL_LAYOUT = 0x111122C;   // prim-model layout data
	static const uint32_t LAYOUT_BYTES = 0x4D0;
	static const uint32_t NODE_BYTES = 0x4FC;         // play node
	static const uint32_t MORPH = 0x4FC;              // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0x504;            // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x10504;
	static const void *const SOUND = (const void *)0x1115964;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }     // MAG_017_sub_701310
	inline void RotationFromAnglesZYX(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701430)(angles, out); }  // MAG_117_RotationFromAnglesZYX
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_RenderPrimModel
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteStoreOTZ(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }   // GTE_ReadOTZ
	inline void GteSetFarColour(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); }
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2508550)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t action;    // +0x0E action whose first target the next play task takes
		uint8_t started;   // +0x0F play pool set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct PlayNode // pool of 1 node of 0x4FC bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t action;    // +0x0E (written, never read)
		uint8_t *entity;   // +0x10 the target's battle entity
		int16_t anchor[4]; // +0x14 x, y (the box's middle height), z, the 4th word of 0x571400
		int32_t scale[3];  // +0x1C the box's extents: x = z = the largest, y = the height
		int32_t pad28;     // +0x28 (cleared, never written)
		uint8_t layout[LAYOUT_BYTES]; // +0x2C
	};
	// the prim-player callback's parameter block (a stack block of the play task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 anchor (4th word: GTE VZ0 high half only)
		int32_t scale[3];  // +0x08
		int32_t scaled;    // +0x14 1: offsets and matrices scaled by the block's scale
		int32_t depth;     // +0x18 -(target +0x26) >> 4
		int16_t scroll;    // +0x1C texture scroll counter (low byte read)
		int16_t pad1E;     // +0x1E never written, never read
		uint32_t morph;    // +0x20 blended vertex frames
	};
	struct BoundsBlock // 0x68DEE0's Field_Alloc block (0x20 bytes)
	{
		uint32_t pad00;
		uint8_t *cursor;   // +0x04 model data cursor
		uint8_t *matrix;   // +0x08 bone world matrix
		int16_t v[3];      // +0x0C vertex through it
		int16_t pad12;
		int16_t box[6];    // +0x14 min x y z, max x y z
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(PlayNode) == NODE_BYTES, "LV Down nodes");
	static_assert(sizeof(PrimArg) == 0x24 && sizeof(BoundsBlock) == 0x20, "LV Down blocks");

	// ------------------------------------------------------------------
	// Module renderers (header 0x58 bytes, the callback's: +0x00 model, +0x04 vertices, +0x08 fade colour, +0x0C fade,
	// +0x18 depth added to every OTZ, +0x1C mode, +0x22 V scroll, +0x24 / +0x2C texture windows (+0x32 low byte: the
	// scroll wrap), +0x34 list cursor, +0x40 OTZ, +0x44 FLAG)
	// ------------------------------------------------------------------
	static inline bool OutX(const uint8_t *xy) { const int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(const uint8_t *xy) { const int16_t v = *(const int16_t *)xy; return v < 0 || v > 0x6C0; }
	static uint8_t *&ListCursor(uint8_t *h) { return *(uint8_t **)(h + 0x34); }

	// the OTZ plus the header's depth, clamped at 0
	static int32_t DepthOTZ(uint8_t *h)
	{
		int32_t &z = *(int32_t *)(h + 0x40);
		z = (int32_t)((uint32_t)z + (uint32_t)*(const int32_t *)(h + 0x18));
		if (z < 0) z = 0;
		return z;
	}

	// GP0(E2) texture window word of the 4 halves at tw (+0 / +2 bytes, +4 / +6 words)
	static uint32_t TexWindow(const uint8_t *tw)
	{
		uint32_t e = (uint32_t)(tw[2] & 0xF8) | 0xFFFE2000u;
		e <<= 5;
		e |= (uint32_t)(tw[0] & 0xF8);
		e <<= 5;
		e |= ~((uint32_t)*(const uint16_t *)(tw + 6) - 1) & 0xF8;
		e <<= 2;
		e |= (uint32_t)((int32_t)~((uint32_t)*(const uint16_t *)(tw + 4) - 1) >> 3) & 0x1F;
		return e;
	}

	// the texture window prims around a scrolled quad: window +0x24 inserted first, then the quad, then window +0x2C
	static uint8_t *QuadWindows(const uint8_t *h, uint32_t bucket, uint8_t *pk, uint8_t *p2)
	{
		*(uint32_t *)p2 = 0x2000000;
		*(uint32_t *)(p2 + 4) = TexWindow(h + 0x24);
		*(uint32_t *)(p2 + 8) = 0;
		InsertPrimAutoDepth(bucket, p2);
		InsertPrimAutoDepth(bucket, pk);
		uint8_t *p3 = p2 + 0xC;
		*(uint32_t *)p3 = 0x2000000;
		*(uint32_t *)(p3 + 4) = TexWindow(h + 0x2C);
		*(uint32_t *)(p3 + 8) = 0;
		InsertPrimAutoDepth(bucket, p3);
		return p3 + 0xC;
	}

	// the V coordinates at o[0..3] scrolled by the header's +0x22, wrapping by its byte +0x32 when one passes 0xFF
	static void ScrollV(const uint8_t *h, uint8_t *pk, const int *o)
	{
		const uint32_t scroll = *(const uint16_t *)(h + 0x22);
		const uint32_t v3 = pk[o[3]] + scroll, v0 = pk[o[0]] + scroll, v1 = pk[o[1]] + scroll, v2 = pk[o[2]] + scroll;
		uint32_t w3 = v3;
		if ((int32_t)(v3 | v2 | v1 | v0) > 0xFF)
		{
			const uint8_t wrap = h[0x32];
			pk[o[0]] = (uint8_t)(v0 - wrap);
			pk[o[1]] = (uint8_t)(v1 - wrap);
			pk[o[2]] = (uint8_t)(v2 - wrap);
			w3 = (uint8_t)(v3 - wrap);
		}
		else
		{
			pk[o[0]] = (uint8_t)v0;
			pk[o[1]] = (uint8_t)v1;
			pk[o[2]] = (uint8_t)v2;
		}
		pk[o[3]] = (uint8_t)w3;
	}

	// the screen clip flags of a quad (x 0..0xA00, y 0..0x6C0) read after RTPS of its 4th vertex: false = all four
	// vertices off one side
	static bool QuadOnScreen(uint8_t *x0, uint8_t *x1, uint8_t *x2, uint8_t *x3)
	{
		uint32_t clip = 0;
		if (OutX(x0)) clip = 1;
		if (OutX(x1)) clip |= 2;
		if (OutX(x2)) clip |= 4;
		if (OutY(x0 + 2)) clip |= 0x10;
		if (OutY(x1 + 2)) clip |= 0x20;
		if (OutY(x2 + 2)) clip |= 0x40;
		GteReadSXY2(x3);
		GteAVSZ4();
		if (OutX(x3)) clip |= 8;
		if (OutY(x3 + 2)) clip |= 0x80;
		return (clip & 0xF) != 0xF && (clip & 0xF0) != 0xF0;
	}

	// sub_68EBC0: textured quads (0x18-byte records), fade toward the far colour, V scrolled, between the texture
	// windows (0x28-byte packets + 2 x 0xC)
	static uint32_t ScrollFT4(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = ListCursor(h);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		const uint8_t *verts = *(uint8_t **)(h + 4);
		ListCursor(h) = rec;
		uint8_t *pk = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		static const int V[4] = { 0xD, 0x15, 0x1D, 0x25 };
		for (int32_t left = count; left; left--, rec += 0x18)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)pk = 0x9000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)rec;
			*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			*(uint32_t *)(pk + 0x1C) = uv;
			*(uint32_t *)(pk + 0x24) = uv >> 16;
			GteReadFLAG(h + 0x44);
			if (*(const uint32_t *)(h + 0x44) & 0x60000) continue;
			GteReadSXY012Split(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			if (!QuadOnScreen(pk + 8, pk + 0x10, pk + 0x18, pk + 0x20)) continue;
			GteStoreOTZ(h + 0x40);
			if (*(const int32_t *)(h + 0xC))
			{
				GteSetFarColour(h[8], h[9], h[0xA]);
				GteLoadRGBC(pk + 4);
				GteSetIR0(*(const int32_t *)(h + 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			ScrollV(h, pk, V);
			const uint32_t bucket = ot + 4 * (uint32_t)(DepthOTZ(h) >> shift);
			pk = QuadWindows(h, bucket, pk, pk + 0x28);
		}
		ListCursor(h) = rec;
		return (uint32_t)pk;
	}

	// sub_68F030: Gouraud textured quads (0x24-byte records, colours at +0x18..+0x20), fade toward the far colour, V
	// scrolled, between the texture windows (0x34-byte packets + 2 x 0xC)
	static uint32_t ScrollGT4(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = ListCursor(h);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		const uint8_t *verts = *(uint8_t **)(h + 4);
		ListCursor(h) = rec;
		uint8_t *pk = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		static const int V[4] = { 0xD, 0x19, 0x25, 0x31 };
		for (int32_t left = count; left; left--, rec += 0x24)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)pk = 0xC000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)rec;
			*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(pk + 0x18) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			*(uint32_t *)(pk + 0x24) = uv;
			*(uint32_t *)(pk + 0x30) = uv >> 16;
			GteReadFLAG(h + 0x44);
			if (*(const uint32_t *)(h + 0x44) & 0x60000) continue;
			GteReadSXY012Split(pk + 8, pk + 0x14, pk + 0x20);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			if (!QuadOnScreen(pk + 8, pk + 0x14, pk + 0x20, pk + 0x2C)) continue;
			GteStoreOTZ(h + 0x40);
			if (*(const int32_t *)(h + 0xC))
			{
				GteSetFarColour(h[8], h[9], h[0xA]);
				GteLoadRGB012(rec + 0x18, rec + 0x1C, rec + 0x20);
				GteSetIR0(*(const int32_t *)(h + 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 0x28);
				GteLoadRGBC(pk + 4);
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			else
			{
				*(uint32_t *)(pk + 0x10) = *(const uint32_t *)(rec + 0x18);
				*(uint32_t *)(pk + 0x1C) = *(const uint32_t *)(rec + 0x1C);
				*(uint32_t *)(pk + 0x28) = *(const uint32_t *)(rec + 0x20);
			}
			ScrollV(h, pk, V);
			const uint32_t bucket = ot + 4 * (uint32_t)(DepthOTZ(h) >> shift);
			pk = QuadWindows(h, bucket, pk, pk + 0x34);
		}
		ListCursor(h) = rec;
		return (uint32_t)pk;
	}

	// MAG_272_sub_68EB80: the model's textured quads, then (three list counts on) its Gouraud quads
	static uint32_t ScrolledMesh(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *model = *(uint8_t **)h;
		ListCursor(h) = model + *(const uint32_t *)model + 0xC;
		cursor = ScrollFT4(h, ot, shift, cursor);
		ListCursor(h) += 0xC;
		return ScrollGT4(h, ot, shift, cursor);
	}

	// sub_68E5D0: textured quads (0x18-byte records), fade toward the far colour, behind a draw mode 0xE1000220
	// (0x28-byte packets + 8)
	static uint32_t FlatFT4(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = ListCursor(h);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		const uint8_t *verts = *(uint8_t **)(h + 4);
		ListCursor(h) = rec;
		uint8_t *pk = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x18)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)pk = 0x9000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)rec;
			*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(rec + 0x10);
			const uint32_t uv = *(const uint32_t *)(rec + 0x14);
			*(uint32_t *)(pk + 0x1C) = uv;
			*(uint32_t *)(pk + 0x24) = uv >> 16;
			GteReadFLAG(h + 0x44);
			if (*(const uint32_t *)(h + 0x44) & 0x60000) continue;
			GteReadSXY012Split(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			if (!QuadOnScreen(pk + 8, pk + 0x10, pk + 0x18, pk + 0x20)) continue;
			GteStoreOTZ(h + 0x40);
			if (*(const int32_t *)(h + 0xC))
			{
				GteSetFarColour(h[8], h[9], h[0xA]);
				GteLoadRGBC(pk + 4);
				GteSetIR0(*(const int32_t *)(h + 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			const uint32_t bucket = ot + 4 * (uint32_t)(DepthOTZ(h) >> shift);
			uint8_t *md = pk + 0x28;
			*(uint32_t *)md = 0x01000000;
			*(uint32_t *)(md + 4) = 0xE1000220;
			InsertPrimAutoDepth(bucket, md);
			InsertPrimAutoDepth(bucket, pk);
			pk += 0x30;
		}
		ListCursor(h) = rec;
		return (uint32_t)pk;
	}

	// sub_68E8C0: Gouraud quads (0x18-byte records, 0x24-byte packets) between the draw modes 0xE1000220 / 0xE1000240
	static uint32_t GouraudG4(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *rec = ListCursor(h);
		const int32_t count = *(const int32_t *)rec;
		rec += 4;
		const uint8_t *verts = *(uint8_t **)(h + 4);
		ListCursor(h) = rec;
		uint8_t *pk = (uint8_t *)cursor;
		if (count <= 0) return cursor;
		for (int32_t left = count; left; left--, rec += 0x18)
		{
			GteLoadV012(verts + 4 * *(const uint16_t *)(rec + 4), verts + 4 * *(const uint16_t *)(rec + 6), verts + 4 * *(const uint16_t *)(rec + 8));
			GteRTPT();
			*(uint32_t *)pk = 0x8000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)rec;
			GteReadFLAG(h + 0x44);
			if (*(const uint32_t *)(h + 0x44) & 0x60000) continue;
			GteReadSXY012Split(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0(verts + 4 * *(const uint16_t *)(rec + 0xA));
			GteRTPS();
			if (!QuadOnScreen(pk + 8, pk + 0x10, pk + 0x18, pk + 0x20)) continue;
			GteStoreOTZ(h + 0x40);
			*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(rec + 0xC);
			*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(rec + 0x10);
			*(uint32_t *)(pk + 0x1C) = *(const uint32_t *)(rec + 0x14);
			const uint32_t bucket = ot + 4 * (uint32_t)(DepthOTZ(h) >> shift);
			uint8_t *md = pk + 0x24;
			*(uint32_t *)md = 0x01000000;
			*(uint32_t *)(md + 4) = 0xE1000220;
			InsertPrimAutoDepth(bucket, md);
			InsertPrimAutoDepth(bucket, pk);
			uint8_t *md2 = pk + 0x2C;
			*(uint32_t *)md2 = 0x01000000;
			*(uint32_t *)(md2 + 4) = 0xE1000240;
			InsertPrimAutoDepth(bucket, md2);
			pk += 0x34;
		}
		ListCursor(h) = rec;
		return (uint32_t)pk;
	}

	// MAG_272_sub_68E570: list 1 (textured quads), list 2 skipped, list 3 (Gouraud quads)
	static uint32_t PlainMesh(uint8_t *h, uint32_t ot, int shift, uint32_t cursor)
	{
		uint8_t *m = *(uint8_t **)h;
		ListCursor(h) = m + *(const uint32_t *)m + 0xC;
		uint32_t r = cursor;
		if (*(const uint32_t *)ListCursor(h) != 0) r = FlatFT4(h, ot, shift, cursor);
		else ListCursor(h) += 4;
		ListCursor(h) += 4;
		if (*(const uint32_t *)ListCursor(h) != 0) return GouraudG4(h, ot, shift, r);
		return r;
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x68E140): one object of the layout
	// ------------------------------------------------------------------
	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
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
		Mat4x3 m = {}; // (pad +0x12 never written in the original: GTE R33 word only)
		const uint32_t flags = r->flags;
		if (flags & 0x400) RotationFromAnglesZYX(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		int16_t off[4] = {}; // (4th word never written in the original: GTE VZ0 high half only)
		const int32_t scaled = arg->scaled;
		if (scaled)
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
		if (scaled) Scale3DMatrix(&m, arg->scale);
		if (flags & 0x1000)
		{
			// the anchor through the camera + the offset through the orthonormalised camera, composed with it
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			int32_t o[3];
			GteSetRotMatrixCtrl(&CameraN());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(o);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)o[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)o[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)o[2]);
			MatrixMultiply(&CameraN(), &m);
		}
		else if (flags & 0x200)
		{
			// the anchor through the camera + the offset
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)off[1]);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)off[0]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)off[2]);
		}
		else
		{
			// the offset + the anchor through the camera, composed with it
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
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
		const int32_t depth = arg->depth;
		*(int32_t *)(h + 0x18) = depth;
		const int16_t obj = (int16_t)r->flags_lo;
		uint32_t cur;
		if (obj <= 1)
		{
			*(int16_t *)(h + 0x26) = 0;
			*(int16_t *)(h + 0x24) = 0;
			*(int16_t *)(h + 0x2A) = 0x100;
			*(int16_t *)(h + 0x28) = 0x100;
			*(int16_t *)(h + 0x2E) = 0;
			*(int16_t *)(h + 0x2C) = 0;
			*(int16_t *)(h + 0x30) = 0x100;
			*(int16_t *)(h + 0x32) = 0x80;
			// the scroll byte shifted as one byte (shl dl, cl)
			*(uint16_t *)(h + 0x22) = (uint16_t)((uint8_t)((uint32_t)(uint8_t)arg->scroll << ((obj + 3) & 0x1F)) & 0x7F);
			cur = ScrolledMesh(h, RenderOT(), 2, PacketCursor());
		}
		else if (obj == 2)
		{
			*(int32_t *)(h + 0x18) = depth + depth;
			cur = PlainMesh(h, RenderOT(), 2, PacketCursor());
		}
		else if (obj == 4)
		{
			cur = PlainMesh(h, RenderOT(), 2, PacketCursor());
		}
		else
		{
			if (obj == 3) *(int32_t *)(h + 0x18) = depth * 4;
			cur = RenderPrimModel2(h, RenderOT(), 2, PacketCursor());
		}
		PacketCursor() = cur;
		FieldFree(0x58);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag272_lv_down_held.h"
#endif

namespace ff8fx
{
namespace lv27x
{
	using namespace eng;

	void ModelBounds(uint8_t *block_, uint8_t *anim_header)
	{
		lv272::BoundsBlock *b = (lv272::BoundsBlock *)block_;
		const uint8_t *model = *(uint8_t **)(anim_header + 4);
		const uint32_t *parts = *(uint32_t **)(model + 4);
		uint8_t *mats = *(uint8_t **)model + 0x10;
		uint32_t nparts = *parts++;
		for (; nparts; nparts--)
		{
			uint8_t *part = *(uint8_t **)(model + 4) + *parts++;
			b->cursor = part;
			const int32_t ngroups = *(const int16_t *)part;
			b->cursor = part + 2;
			for (int32_t g = ngroups; g; g--)
			{
				const int32_t bone = *(const int16_t *)b->cursor;
				b->cursor += 2;
				b->matrix = mats + bone * 0x30 + 0x10;
				GteSetRotMatrixCtrl((const Mat4x3 *)b->matrix);
				GteSetTransVectorCtrl((const Mat4x3 *)b->matrix);
				const int32_t nverts = *(const int16_t *)b->cursor;
				b->cursor += 2;
				for (int32_t v = nverts; v; v--)
				{
					fn<void (__cdecl *)(const void *)>(0x703FB0)(b->cursor); // GTE_LoadV0_u16
					GteMVMVA_RotV0Tr();
					b->cursor += 6;
					GteStoreIR123(b->v);
					for (int k = 0; k < 3; k++)
					{
						const int16_t c = b->v[k];
						if (b->box[k] > c) b->box[k] = c;
						if (b->box[3 + k] < c) b->box[3 + k] = c;
					}
				}
			}
		}
	}
}

namespace lv272
{
	// MAG_272_sub_68DEE0: the box (min x y z, max x y z) of the target's model at its current pose
	static void TargetBounds(uint8_t *entity, int16_t out[6])
	{
		BoundsBlock *b = (BoundsBlock *)FieldAlloc(0x20);
		uint8_t *anim = entity + 0x60;
		ComputeBonesWorldMatrices(anim, entity + 0x40);
		b->box[2] = 0x7FFF;
		b->box[1] = 0x7FFF;
		b->box[0] = 0x7FFF;
		b->box[5] = -0x7FFF;
		b->box[4] = -0x7FFF;
		b->box[3] = -0x7FFF;
		lv27x::ModelBounds((uint8_t *)b, anim);
		memcpy(out, b->box, 12);
		FieldFree(0x20);
		BuildBoneMatricesFromPose(anim);
	}

	// ------------------------------------------------------------------
	// Play (0x68E090)
	// ------------------------------------------------------------------
	static uint32_t __cdecl PlayTask(TaskNode *n)
	{
		PlayNode *p = (PlayNode *)n;
		PrimArg arg;
		arg.morph = TexBase() + MORPH;
		arg.depth = -(int32_t)*(const int16_t *)(p->entity + 0x26) >> 4;
		arg.scale[0] = p->scale[0];
		arg.scale[1] = p->scale[1];
		arg.scale[2] = p->scale[2];
		memcpy(arg.pos, p->anchor, sizeof(arg.pos));
		arg.scaled = 1;
		arg.scroll = (int16_t)-p->counter;
		arg.pad1E = 0; // (never written in the original, never read)
		// 30 fps layer: see mag272_lv_down_held.inc
		FX_HELD(held_note_play(p);)
		if (prim::play((prim::Layout *)p->layout, PrimCallback, (int)&arg, 0) == 0)
		{
			ApplyActionResultToTarget(Ctx()->actions[0].targets);
			return TASK_END;
		}
		p->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x68DD20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag272_lv_down_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QPlay(), (void *)TexBase(), NODE_BYTES, 1);
			}
			PlayNode *p = (PlayNode *)AddTaskToQueue(QPlay(), ORIG_PlayTask);
			Memset32(&p->counter, 0, 0x13C);
			p->action = (int16_t)r->action;
			p->entity = Entity(Ctx()->actions[p->action].targets[0]);
			int16_t box[6];
			TargetBounds(p->entity, box);
			GetDefaultEffectPosition(p->entity, p->anchor);
			p->anchor[1] = (int16_t)(((int32_t)box[4] + box[1]) >> 1);
			p->scale[1] = (int32_t)box[4] - box[1];
			const int32_t dx = (int32_t)box[3] - box[0];
			p->scale[0] = dx;
			const int32_t dz = (int32_t)box[5] - box[2];
			p->scale[2] = dz;
			if (dx < dz) p->scale[0] = dz;
			else p->scale[2] = dx;
			if (p->scale[1] > p->scale[0])
			{
				p->scale[2] = p->scale[1];
				p->scale[0] = p->scale[1];
			}
			DecodeModelPrimLayout(MODEL_LAYOUT, p->layout, LAYOUT_BYTES);
			r->action++;
			BdPlaySE(SOUND, 0, 0x80);
		}
		EffectCameraMatrix(&Camera(), &CameraN());
		int a;
		if (r->started) a = ExecuteTaskQueue(QPlay());
		else a = (int)n;
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag272_lv_down()
	{
		register_port(lv272::ORIG_RootTask, (void *)lv272::RootTask, "272 RootTask", 272);
		register_port(lv272::ORIG_PlayTask, (void *)lv272::PlayTask, "272 PlayTask", 272);
		// 30 fps layer: see mag272_lv_down_held.inc
		FX_HELD(register_mag272_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag272_lv_down_held.inc"
#endif
