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

// Effect 140: Phoenix - Rebirth Flame (timeline-A GF family, MAG_140_*).
//
// Structure (see gf_study/gf_inventory_timeline.md 3.1):
//   SequenceTask (master, 0x6A6530) - flips the packet arena, spawns the creature at tick 2,
//     runs the creature queue 0x2517AD8, then the spark queue 0x2517AC8 and the ember queue
//     0x2517AB8, sets the screen flash.
//   CreatureTask (0x6A6750) - 255-tick timeline (the counter runs twice as fast for ticks
//     1..60): fire trail flipbooks + three scrolling flame meshes (counter 1..100), white-out
//     tiles, the Phoenix model (101..236, fading/rising in for 20 ticks), three keyframed
//     prim-model stages (101..150, 151..194, 195..254), heat haze (235..254), camera moves,
//     streams, sounds, damage (245), voice release (251).
//   particle tasks: sparks 0x6A7D90 (flipbook, 16 ticks), embers 0x6A7BF0 (prim model, 16 ticks).
//   prim-model callback 0x6A7650 (drawn through the shared player, fx_primplayer.cpp).
// Module globals: 0x2517AA0..0x2517B50 (gf_study/gf_global_ranges.md).

#include "fx_port.h"

namespace ff8fx
{
namespace p140
{
	using namespace eng;

	// --- module globals ---
	inline uint8_t *&EmberPool() { return var<uint8_t *>(0x2517AA0); }   // tim buffer end - 0x10000 (set by _FL)
	inline uint8_t *&SparkPool() { return var<uint8_t *>(0x2517AA4); }   // tim buffer end - 0x20000
	inline uint32_t &Pause() { return var<uint32_t>(0x2517AAC); }        // debug pause, never set by the game
	inline uint8_t *&CastCtx() { return var<uint8_t *>(0x2517AB0); }
	inline TaskQueuePair &QueueParticles() { return var<TaskQueuePair>(0x2517AB8); } // .first embers, .second sparks
	inline TaskQueuePair &QueueCreature() { return var<TaskQueuePair>(0x2517AD8); }  // .first creature, .second root
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2517B0C); }
	inline int32_t &TargetsAvg() { return var<int32_t>(0x2517B10); }     // mean of the targets' BattleEntitySlotData +0x20
	inline int32_t &Base() { return var<int32_t>(0x2517B18); }           // TargetsAvg + 0xE6A
	inline uint32_t &Flash() { return var<uint32_t>(0x2517B20); }
	inline uint8_t &DoneFlag() { return var<uint8_t>(0x2517B24); }
	inline uint8_t *&ModelBuffer() { return var<uint8_t *>(0x2517B28); } // = Magic_TextureOFF_ToEAX1()
	inline Mat4x3 &EffectCamera() { return var<Mat4x3>(0x2517B30); }
	// engine
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }  // battle_texture_data_ptr_1D8E054
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }        // g_Battle_FrameRenderListBase
	inline int16_t &CamWorld(int k) { return var<int16_t>(0xB8B7F0 + 2 * k); }  // Battle_Camera_world x, y, z
	inline int16_t &CamLookAt(int k) { return var<int16_t>(0xB8B7F8 + 2 * k); } // Battle_Camera_LookAt x, y, z

	static const uint32_t ORIG_SequenceTask = 0x6A6530;
	static const uint32_t ORIG_CreatureTask = 0x6A6750;
	static const uint32_t ORIG_EmberTask = 0x6A7BF0;
	static const uint32_t ORIG_SparkTask = 0x6A7D90;
	static uint32_t g_ported_tick = 0xFFFFFFFF; // real tick on which the ported master last ran

	// --- engine functions this module calls (original addresses) ---
	inline int32_t Rand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); }
	// sub_56CB90: out = (a * wa + b * wb) >> 12 on 3 x s16 (GTE GPF/GPL, IR loaded zero-extended)
	inline void VecLerp(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CB90)(a, b, wa, wb, out); }
	// sub_56CC00: same on 3 x u8 (colours)
	inline void RgbLerp(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); }
	// MAG_063_sub_701270: rotation about Y (writes the 3x3 and the pad, not the translation)
	inline void RotY(int32_t angle, void *out) { fn<void (__cdecl *)(int32_t, void *)>(0x701270)(angle, out); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline int32_t ISqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline void MatMulVec(const void *m, const void *in, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x56C4F0)(m, in, out); }
	inline void DecodePrimLayout(const void *data, void *layout, int32_t size) { fn<void (__cdecl *)(const void *, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void BindModelSections(void *e, void *sections, const void *data) { fn<void (__cdecl *)(void *, void *, const void *)>(0x6EC060)(e, sections, data); }
	inline void ReadAnimationStart(void *e, int32_t anim) { fn<void (__cdecl *)(void *, int32_t)>(0x6574D0)(e, anim); }
	inline void AdvanceModelAnim(void *e) { fn<void (__cdecl *)(void *)>(0x6FBDB0)(e); }
	inline uint32_t RenderGeometry(void *sections, void *ctx, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(sections, ctx, ot, mode, cursor); }
	inline void ModelObjectSetup(void *ctx) { fn<void (__cdecl *)(void *)>(0x6FF1B0)(ctx); }
	inline uint32_t ModelObjectDraw(void *ctx, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t)>(0x6FEE90)(ctx, cursor); }
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZReg(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }   // OTZ register (after AVSZ)
	inline void GteLoadRGB(const void *src) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(src); } // 3 x u8 -> IR1..3
	inline void GteStoreRGB(void *dst) { fn<void (__cdecl *)(void *)>(0x45E410)(dst); }      // IR1..3 low bytes -> 3 x u8
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }                      // MAC = R * V0 (no translation)
	inline void GteMatrixMultiply(const void *a, void *b) { fn<void (__cdecl *)(const void *, void *)>(0x56C270)(a, b); }
	inline void MatMulScaleDiag(void *r, const void *s) { fn<void (__cdecl *)(void *, const void *)>(0x56C220)(r, s); }
	inline void ScaleMatrix(void *r, const int32_t *s) { fn<void (__cdecl *)(void *, const int32_t *)>(0x56BEF0)(r, s); }
	inline void PrimMorph(void *model, int32_t a, int32_t b, int32_t t, void *out) { fn<void (__cdecl *)(void *, int32_t, int32_t, int32_t, void *)>(0x701390)(model, a, b, t, out); }
	inline void PrimRotation(const void *angles, void *out) { fn<void (__cdecl *)(const void *, void *)>(0x701310)(angles, out); }
	inline void PrimRotationAlt(const void *angles, void *out) { fn<void (__cdecl *)(const void *, void *)>(0x7015B0)(angles, out); }
	inline uint32_t RenderPrimSet(void *hdr, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(hdr, ot, mode, cursor); }
	inline void InsertPrimAltViewport(uint32_t ot, void *p) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(ot, p); }
	inline void SetMoveImage(void *p, const int16_t *rect, int32_t x, int32_t y) { fn<void (__cdecl *)(void *, const int16_t *, int32_t, int32_t)>(0x45C060)(p, rect, x, y); }
	// streams / sound / battle
	inline void StreamLoad(int32_t id, void *buf, int32_t slot) { fn<void (__cdecl *)(int32_t, void *, int32_t)>(0x5341D0)(id, buf, slot); }
	inline int32_t StreamBusy() { return fn<int32_t (__cdecl *)()>(0x534270)(); }
	inline void BattlePreload() { fn<void (__cdecl *)()>(0x534210)(); }
	inline void PlaySummonStream(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x5018C0)(a, b, c); }
	inline void PlaySE(uint32_t sound, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(0x501330)(sound, a, b); }
	inline void ApplyActionResultToTargets(void *targets, int32_t count) { fn<void (__cdecl *)(void *, int32_t)>(0x506BA0)(targets, count); }
	inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
	inline void StreamWaitDone(void *buf, uint8_t *flag) { fn<void (__cdecl *)(void *, uint8_t *)>(0x508630)(buf, flag); }

#pragma pack(push, 1)
	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
	struct MasterNode // root pool 0x2517AF8, 0x14 bytes
	{
		TaskNode hdr;
		uint16_t counter; // +0x0C
		uint8_t pad0E;
		uint8_t spawned;  // +0x0F
		uint32_t parity;  // +0x10 packet arena
	};

	struct CreatureNode // the single node of queue 0x2517AD8 = ModelBuffer + 0, 0x878 bytes
	{
		TaskNode hdr;
		int16_t counter;          // +0x0C timeline tick
		int16_t ntargets;         // +0x0E
		uint32_t voice;           // +0x10 BdSound_ClaimVoiceSlot
		uint8_t *targets[10];     // +0x14 BattleEntitySlotData of each target (0x1D972C0 + 0x9C * id)
		uint32_t saved_color[10]; // +0x3C their tint (+0x28) at spawn, restored at the end
		uint8_t model[0x9C];      // +0x64 E: standard effect model block (Effect_BindModelContainerSections)
		                          //   E+0x28 colour, E+0x40 root Mat4x3 (+0xA4), E+0x60 BattleAnimHeader (+0xC4),
		                          //   E+0x6C BattleAnimCmd (+0xD0), E+0x7C object mask
		uint8_t sections[0x10];   // +0x100 model section pointers
		uint8_t prim_a[0x288];    // +0x110 prim-model player layout (stages 101 / 151 / 195)
		uint8_t prim_b[0x4E0];    // +0x398 second layout (stage 151)
	};

	struct EmberNode // queue 0x2517AB8 (4 x 0x18, pool dword_2517AA0)
	{
		TaskNode hdr;
		int16_t pos[3]; // +0x0C
		int16_t age;    // +0x12
		int16_t angle;  // +0x14 rotation about Y
		int16_t spin;   // +0x16
	};

	struct SparkNode // queue 0x2517AC8 (64 x 0x1C, pool dword_2517AA4)
	{
		TaskNode hdr;
		int16_t pos[3]; // +0x0C x, y, z
		int16_t age;    // +0x12 flipbook frame
		int16_t vel[2]; // +0x14 x, z (>> 4 per tick)
		int16_t acc[2]; // +0x18 x, z (brake: -+0x100 per tick against the velocity)
	};

	struct PrimCtx // callback context of the prim stages (creature stack local, arg of the player)
	{
		Mat4x3 m;          // +0x00 object frame
		int16_t shade;     // +0x20 -> render header +0x18
		int16_t scroll;    // +0x22 texture scroll (low byte & 0x7F)
		uint8_t *morph;    // +0x24 morph output buffer (ModelBuffer + 0x878)
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14, "master node is 0x14 bytes");
	static_assert(sizeof(CreatureNode) == 0x878, "creature node is 0x878 bytes");
	static_assert(sizeof(EmberNode) == 0x18, "ember node is 0x18 bytes");
	static_assert(sizeof(SparkNode) == 0x1C, "spark node is 0x1C bytes");
	static_assert(sizeof(PrimCtx) == 0x28, "prim context is 0x28 bytes");

	inline uint8_t *E(CreatureNode *c) { return c->model; }
	inline Mat4x3 &Root(CreatureNode *c) { return *(Mat4x3 *)(c->model + 0x40); }
	// target arrays through raw offsets, as the original (no bound)
	inline uint8_t *&Target(CreatureNode *c, int k) { return *(uint8_t **)((uint8_t *)c + 0x14 + 4 * k); }
	inline uint32_t *SavedColor(CreatureNode *c, int k) { return (uint32_t *)((uint8_t *)c + 0x3C + 4 * k); }

	// ------------------------------------------------------------------
	// Draw helpers
	// ------------------------------------------------------------------

	// 0x6A85A0: fire flipbook (sequence 0x11D0164) at the current GTE/camera setup
	static void Flipbook(uint8_t *hdr, int16_t frame)
	{
		*(uint32_t *)hdr = 0x11D0164;
		*(int16_t *)(hdr + 4) = frame;
		*(int16_t *)(hdr + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(hdr, RenderOT(), 2, PacketCursor());
	}

	// GP0(E2) texture window from 4 fields {x byte, y byte, w u16, h u16} (0x6A82D6..)
	static uint32_t TexWindow(const uint8_t *w)
	{
		uint32_t c = ((uint32_t)w[2] & 0xF8) | 0xFFFE2000u;
		c <<= 5;
		c |= (uint32_t)w[0] & 0xF8;
		c <<= 5;
		c |= (uint32_t)(-(int32_t)*(const uint16_t *)(w + 6)) & 0xF8;
		c <<= 2;
		c |= (uint32_t)(((int32_t)(0u - (uint32_t)*(const uint16_t *)(w + 4))) >> 3) & 0x1F;
		return c;
	}

	// 0x6A8140: one gouraud textured quad (GP0 0x3E) of a flame mesh, between two texture
	// window packets; h = mesh render header (0x98 bytes on the scratch stack)
	static uint32_t FlameQuad(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		GteLoadV012(h + 0x3C, h + 0x44, h + 0x4C);
		GteRTPT();
		uint32_t *p = (uint32_t *)cursor;
		p[0] = 0x0C000000;
		p[3] = *(uint32_t *)(h + 0x6C);
		p[6] = *(uint32_t *)(h + 0x70);
		uint32_t uv23 = *(uint32_t *)(h + 0x74);
		p[9] = uv23;
		p[12] = uv23 >> 16;
		GteReadFLAG(h + 0x34);
		if (*(uint32_t *)(h + 0x34) & 0x60000) return cursor;
		GteReadSXY012(p + 2, p + 5, p + 8);
		GteLoadV0(h + 0x54);
		GteRTPS();
		const int16_t *s = (const int16_t *)p;
		uint32_t out = 0;
		if (s[4] < 0 || s[4] > 0xA00) out = 1;     // x0
		if (s[10] < 0 || s[10] > 0xA00) out |= 2;  // x1
		if (s[16] < 0 || s[16] > 0xA00) out |= 4;  // x2
		if (s[5] < 0 || s[5] > 0x6C0) out |= 0x10; // y0
		if (s[11] < 0 || s[11] > 0x6C0) out |= 0x20;
		if (s[17] < 0 || s[17] > 0x6C0) out |= 0x40;
		GteReadSXY2(p + 11);
		GteAVSZ4();
		if (s[22] < 0 || s[22] > 0xA00) out |= 8;  // x3
		if (s[23] < 0 || s[23] > 0x6C0) out |= 0x80;
		if ((out & 0xF) == 0xF) return cursor;
		if ((out & 0xF0) == 0xF0) return cursor;
		GteReadOTZReg(h + 0x30);
		p[1] = *(uint32_t *)(h + 0x5C);
		p[4] = *(uint32_t *)(h + 0x60);
		p[7] = *(uint32_t *)(h + 0x64);
		p[10] = *(uint32_t *)(h + 0x68);
		uint32_t bucket = ot + 4 * (uint32_t)(*(int32_t *)(h + 0x30) >> mode);
		uint32_t *q = p + 13;
		q[0] = 0x02000000;
		q[1] = TexWindow(h + 0x14);
		q[2] = 0;
		InsertPrimAutoDepth(bucket, q);
		InsertPrimAutoDepth(bucket, p);
		uint32_t *r = q + 3;
		r[0] = 0x02000000;
		r[1] = TexWindow(h + 0x1C);
		r[2] = 0;
		InsertPrimAutoDepth(bucket, r);
		return (uint32_t)(r + 3);
	}

	// 0x6A7EB0: every face of the mesh, drawn twice (the second time mirrored in x).
	// Per vertex: w = h+0x38 (fade, 4.12 - 0x1000) + the vertex's 4th word; w <= 0 = black,
	// else the face colour scaled by min(w << 4, 0x1000). u += h+0x10 (scroll), wrapped by h+0x20.
	static uint32_t FlameMeshFaces(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		const uint8_t *vtx = *(const uint8_t **)(h + 4);
		uint8_t *face = *(uint8_t **)(h + 0x24);
		int32_t count = *(int32_t *)face;
		face += 4;
		*(uint8_t **)(h + 0x24) = face;
		if (count <= 0) return cursor;
		for (; count; count--)
		{
			static const int COLOR_OFS[4] = { 0, 0x18, 0x1C, 0x20 };
			for (int k = 0; k < 4; k++)
			{
				uint32_t idx = *(const uint16_t *)(face + 4 + 2 * k);
				memcpy(h + 0x3C + 8 * k, vtx + 4 * idx, 8);
				int32_t w = *(int32_t *)(h + 0x38) + (int32_t)*(int16_t *)(h + 0x42 + 8 * k);
				uint8_t *col = h + 0x5C + 4 * k;
				if (w <= 0) *(uint32_t *)col = k == 0 ? 0x3E000000u : 0u;
				else
				{
					*(uint32_t *)col = *(const uint32_t *)(face + COLOR_OFS[k]);
					w = shl32(w, 4);
					if (w < 0x1000)
					{
						GteSetIR0(w);
						GteLoadRGB(col);
						GteGPF();
						GteStoreRGB(col);
					}
				}
			}
			*(uint32_t *)(h + 0x6C) = *(const uint32_t *)(face + 0xC);
			*(uint32_t *)(h + 0x70) = *(const uint32_t *)(face + 0x10);
			*(uint16_t *)(h + 0x74) = *(const uint16_t *)(face + 0x14);
			*(uint16_t *)(h + 0x76) = *(const uint16_t *)(face + 0x16);
			uint32_t scroll = *(const uint16_t *)(h + 0x10);
			uint32_t u0 = h[0x6C] + scroll, u1 = h[0x70] + scroll, u2 = h[0x74] + scroll, u3 = h[0x76] + scroll;
			if ((int32_t)(u3 | u2 | u1 | u0) > 0xFF)
			{
				uint8_t wrap = h[0x20];
				h[0x6C] = (uint8_t)(u0 - wrap);
				h[0x70] = (uint8_t)(u1 - wrap);
				h[0x74] = (uint8_t)(u2 - wrap);
				h[0x76] = (uint8_t)(u3 - wrap);
			}
			else
			{
				h[0x6C] = (uint8_t)u0;
				h[0x70] = (uint8_t)u1;
				h[0x74] = (uint8_t)u2;
				h[0x76] = (uint8_t)u3;
			}
			cursor = FlameQuad(h, ot, mode, cursor);
			for (int k = 0; k < 4; k++)
			{
				int16_t *x = (int16_t *)(h + 0x3C + 8 * k);
				*x = (int16_t)-*x;
			}
			cursor = FlameQuad(h, ot, mode, cursor);
			face += 0x24;
		}
		*(uint8_t **)(h + 0x24) = face;
		return cursor;
	}

	// 0x6A7E80: faces = model + *model + 0x1C
	static uint32_t FlameMesh(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *model = *(uint8_t **)h;
		*(uint8_t **)(h + 0x24) = model + *(int32_t *)model + 0x1C;
		return FlameMeshFaces(h, ot, mode, cursor);
	}

	// 0x6A83B0: full-screen flat semi-transparent tile (GP0 0x2A), colour * (d - n) / d
	static bool TileColor(int32_t r, int32_t g, int32_t b, int32_t n, int32_t d, uint8_t rgb[3])
	{
		int32_t k = d - n;
		if (k <= 0) return false;
		rgb[0] = (uint8_t)(mul32(r, k) / d);
		rgb[1] = (uint8_t)(mul32(g, k) / d);
		rgb[2] = (uint8_t)(mul32(b, k) / d);
		return true;
	}

	static uint32_t TileEmit(const uint8_t rgb[3], uint32_t cursor)
	{
		uint8_t *p = (uint8_t *)cursor;
		int16_t *w = (int16_t *)p;
		w[4] = 0; w[5] = 0;          // (0, 0)
		w[6] = 0x140; w[7] = 0;      // (320, 0)
		w[8] = 0; w[9] = 0xD8;       // (0, 216)
		w[10] = 0x140; w[11] = 0xD8; // (320, 216)
		*(uint32_t *)p = 0x05000000;
		p[7] = 0x2A;
		p[4] = rgb[0];
		p[5] = rgb[1];
		p[6] = rgb[2];
		InsertPrimAltViewport(OTBase() + 0x20, p);
		return cursor + 0x18;
	}

	// 0x6A8450: heat haze. Every framebuffer line y = 8..219 of the buffer being drawn (x 0 or
	// 320 by the display parity dword_1D96A80) is shifted by (sin(phase + 0x80 * i) * amp * i) >> 20
	// pixels with MoveImage packets (a right shift goes through the scratch line (320, 251)).
	static void Haze(int32_t amp, int32_t phase, int32_t ot_index)
	{
		int32_t x0 = var<uint8_t>(0x1D96A80) != 0 ? 0 : 0x140;
		uint32_t bucket = OTBase() + 4 * (uint32_t)ot_index;
		int16_t line[4] = { (int16_t)x0, 8, 0x140, 1 };
		int16_t part[4] = { 0, 0, 0, 1 };
		int32_t scale = 0;
		do
		{
			int32_t off = mul32(ComputeSin(phase), scale) >> 20;
			phase += 0x80;
			uint8_t *p = (uint8_t *)PacketCursor();
			PacketCursor() += 0x18;
			if (off < 0)
			{
				part[2] = (int16_t)(off + 0x140);
				part[1] = line[1];
				part[0] = (int16_t)(x0 - off);
				SetMoveImage(p, part, x0, line[1]);
				InsertPrimAutoDepth(bucket, p);
			}
			else if (off > 0)
			{
				part[2] = (int16_t)(0x140 - off);
				part[1] = 0xFB;
				part[0] = 0x140;
				SetMoveImage(p, part, off + x0, line[1]);
				InsertPrimAutoDepth(bucket, p);
				uint8_t *p2 = (uint8_t *)PacketCursor();
				PacketCursor() += 0x18;
				SetMoveImage(p2, line, 0x140, 0xFB);
				InsertPrimAutoDepth(bucket, p2);
			}
			line[1]++;
			scale += amp;
		} while (line[1] < 0xDC);
	}

	// 0x6A79B0 (IDA MAG_089_sub_6A79B0): standard model draw (root composed with the camera)
	static uint32_t DrawModel(uint8_t *e, uint8_t *buf, uint32_t cursor, uint32_t mode)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x4C);
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(e + 0x40), (Mat4x3 *)w);
		ComputeBonesWorldMatrices(e + 0x60, w);
		*(uint8_t **)(w + 0x24) = buf;
		*(uint16_t *)(w + 0x34) = 0;
		*(uint16_t *)(w + 0x36) = 0;
		*(uint16_t *)(w + 0x38) = 0x140;
		*(uint16_t *)(w + 0x3A) = 0xD8;
		*(uint32_t *)(w + 0x48) = 0;
		*(uint32_t *)(w + 0x3C) = *(uint32_t *)(e + 0x28);
		*(uint16_t *)(w + 0x44) = 0;
		*(uint32_t *)(w + 0x40) = *(uint32_t *)(e + 0x7C);
		*(uint32_t *)(w + 0x30) = mode;
		uint32_t r = RenderGeometry(*(void **)(e + 0x64), w + 0x20, OTBase() + 0x44, 4, cursor);
		BuildBoneMatricesFromPose(e + 0x60);
		FieldFree(0x4C);
		return r;
	}

	// 0x6A7AE0: object loop of the fade-in draw: every object whose bit is set in ctx+0x18 is
	// skinned (sub_6FF1B0 per bone group) and drawn (sub_6FEE90)
	static uint32_t FadeRender(uint8_t **sections, uint8_t *ctx, uint32_t cursor)
	{
		const uint8_t *offs = sections[1];
		uint8_t *bones = sections[0] + 0x10;
		int32_t count = *(const int32_t *)offs;
		offs += 4;
		if (count <= 0) return cursor;
		uint8_t *&rd = *(uint8_t **)(ctx + 0x50);
		for (int32_t i = 0; i < count; i++)
		{
			rd = sections[1] + *(const int32_t *)offs;
			offs += 4;
			if (!(*(uint32_t *)(ctx + 0x18) & (1u << (i & 31)))) continue;
			*(uint32_t *)(ctx + 0x5C) = *(uint32_t *)(ctx + 4);
			int32_t n = *(int16_t *)rd;
			rd += 2;
			for (; n > 0; n--)
			{
				int32_t bone = *(int16_t *)rd;
				rd += 2;
				Mat4x3 *m = (Mat4x3 *)(bones + 48 * bone + 0x10);
				GteSetRotMatrixCtrl(m);
				GteSetTransVectorCtrl(m);
				ModelObjectSetup(ctx);
			}
			rd = (uint8_t *)(((uint32_t)rd + 3) & ~3u);
			*(uint16_t *)(ctx + 8) = *(uint16_t *)rd;
			rd += 2;
			*(uint16_t *)(ctx + 0xA) = *(uint16_t *)rd;
			rd += 0xA;
			*(uint8_t **)ctx = rd;
			cursor = ModelObjectDraw(ctx, cursor);
		}
		return cursor;
	}

	// 0x6A7A50: fade-in model draw (root not composed with the camera; own object renderer)
	static void DrawModelFade(uint8_t *e, uint32_t a2, uint8_t *buf)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x60);
		*(uint32_t *)(w + 0x4C) = a2;
		*(uint8_t **)(w + 4) = buf;
		ComputeBonesWorldMatrices(e + 0x60, e + 0x40);
		*(uint16_t *)(w + 0x10) = 0x140;
		*(uint16_t *)(w + 0xC) = 0;
		*(uint16_t *)(w + 0xE) = 0;
		*(uint16_t *)(w + 0x12) = 0xD8;
		*(uint32_t *)(w + 0x14) = *(uint32_t *)(e + 0x28);
		*(uint32_t *)(w + 0x18) = *(uint32_t *)(e + 0x7C);
		*(uint32_t *)(w + 0x58) = 0;
		*(uint32_t *)(w + 0x54) = OTBase() + 0x44;
		FrameCursor() = FadeRender(*(uint8_t ***)(e + 0x64), w, FrameCursor());
		BuildBoneMatricesFromPose(e + 0x60);
		FieldFree(0x60);
	}

	// 0x6A7650: prim-model object callback of the shared player (MAG_011_sub_701970):
	// picks the model (or morphs two frames into ctx->morph), builds the object matrix
	// (flag 0x1000 = effect-camera relative, 0x200 = absolute position, else relative to
	// ctx->m), scales it, then MAG_063_sub_701DD0 draws it.
	static void __cdecl PrimObject(prim::Layout *l, prim::Record *rec, int arg)
	{
		PrimCtx *ctx = (PrimCtx *)arg;
		if (((int32_t)rec->scale[2] | *(int32_t *)&rec->scale[0]) == 0) return;
		if (rec->a >= 0x1000 && *(uint32_t *)rec->rgb == 0) return;
		uint8_t *w = (uint8_t *)FieldAlloc(0x6C);
		uint8_t *data = l->data;
		uint8_t *model = data + *(int32_t *)(data + 8 + 4 * (int32_t)(int16_t)rec->flags_lo);
		*(uint8_t **)w = model;
		int16_t fa = rec->b0, fb = rec->b1;
		int16_t pick = fa;
		bool morph = false;
		if (fa != fb)
		{
			int16_t t = rec->b;
			if (t == 0x1000) pick = fb;
			else if (t != 0) morph = true;
			if (morph)
			{
				PrimMorph(model, fa, fb, t, ctx->morph);
				*(uint8_t **)(w + 4) = ctx->morph;
			}
		}
		if (!morph)
			*(uint8_t **)(w + 4) = pick == 0 ? model + 0xC : model + 0xC + 8 * mul32(*(int32_t *)(model + 4), pick);
		Mat4x3 R = {};
		if (rec->flags & 0x40000) PrimRotationAlt(rec->rot, &R);
		else PrimRotation(rec->rot, &R);
		uint32_t flags = rec->flags;
		int16_t v[4] = { rec->pos[0], rec->pos[1], rec->pos[2], 0 }; // 4th word: vanilla stack garbage (GTE VZ0 high half only)
		if (flags & 0x1000)
		{
			GteSetRotMatrixCtrl(&EffectCamera());
			GteLoadV0(v);
			GteMVMVA_RotV0();
			int32_t mac[3];
			GteReadMAC123(mac);
			R.t[0] = mac[0];
			R.t[1] = mac[1];
			R.t[2] = mac[2];
			GteMatrixMultiply(&EffectCamera(), &R);
		}
		else if (flags & 0x200)
		{
			R.t[0] = v[0];
			R.t[1] = v[1];
			R.t[2] = v[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&ctx->m);
			GteLoadV0(v);
			GteMVMVA_RotV0();
			GteReadMAC123(R.t);
			GteMatrixMultiply(&ctx->m, &R);
		}
		R.t[0] += ctx->m.t[0];
		R.t[1] += ctx->m.t[1];
		R.t[2] += ctx->m.t[2];
		if (!(*(uint32_t *)&rec->scale[0] == 0x10001000u && rec->scale[2] == 0x1000))
		{
			if (flags & 0x100)
			{
				int16_t s[9] = { rec->scale[0], 0, 0, 0, rec->scale[1], 0, 0, 0, rec->scale[2] };
				MatMulScaleDiag(&R, s);
			}
			else
			{
				int32_t s[3] = { rec->scale[0], rec->scale[1], rec->scale[2] };
				ScaleMatrix(&R, s);
			}
		}
		GteSetRotMatrix(&R);
		GteSetTransVector(&R);
		*(uint32_t *)(w + 0x1C) = 0x2030;
		*(int32_t *)(w + 0xC) = rec->a;
		if (rec->a != 0)
		{
			*(uint32_t *)(w + 0x1C) = 0x20F0;
			*(uint32_t *)(w + 8) = *(uint32_t *)rec->rgb;
		}
		uint8_t scroll = (uint8_t)ctx->scroll;
		if (flags & 0x3000000)
		{
			*(uint16_t *)(w + 0x26) = 0;
			*(uint16_t *)(w + 0x24) = 0;
			*(uint16_t *)(w + 0x2A) = 0x100;
			*(uint16_t *)(w + 0x28) = 0x100;
			if (flags & 0x1000000)
			{
				*(uint16_t *)(w + 0x20) = 0;
				*(uint16_t *)(w + 0x22) = scroll & 0x7F;
				*(uint16_t *)(w + 0x2C) = 0;
				*(uint16_t *)(w + 0x2E) = 0x80;
				*(uint16_t *)(w + 0x30) = 0x100;
				*(uint16_t *)(w + 0x32) = 0x80;
			}
			else
			{
				*(uint16_t *)(w + 0x22) = 0;
				*(uint16_t *)(w + 0x2C) = 0;
				*(uint16_t *)(w + 0x20) = (uint8_t)(0u - scroll) & 0x7F;
				*(uint16_t *)(w + 0x2E) = 0;
				*(uint16_t *)(w + 0x30) = 0x80;
				*(uint16_t *)(w + 0x32) = 0x100;
			}
		}
		else
		{
			*(uint16_t *)(w + 0x22) = 0;
			*(uint16_t *)(w + 0x20) = 0;
		}
		*(int32_t *)(w + 0x18) = ctx->shade;
		PacketCursor() = RenderPrimSet(w, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x6C);
	}

	// prim stage contexts (creature 0x6A6CCA.., 0x6A6E4F.., 0x6A700D..)
	static void CtxStage101(PrimCtx &c, int16_t scroll)
	{
		c.morph = ModelBuffer() + 0x878;
		c.shade = -128;
		RotY(-0x400, &c.m);
		c.m.t[0] = 0;
		c.m.t[1] = 0;
		c.m.t[2] = Base();
		ComposeAffineTransform(&Camera(), &c.m, &c.m);
		c.scroll = scroll;
	}

	// identity frame at depth 0x1000 (the objects use flags 0x1000/0x200 for their placement)
	// NOTE the pad of the matrix is the stack's previous content in vanilla (only reaches the
	// high half of GTE control register RT33 transiently); the port uses 0
	static void CtxIdentity(PrimCtx &c, int16_t shade, int16_t scroll)
	{
		c.morph = ModelBuffer() + 0x878;
		memset(&c.m, 0, sizeof(c.m));
		c.m.m[0][0] = c.m.m[1][1] = c.m.m[2][2] = 0x1000;
		c.m.t[2] = 0x1000;
		c.shade = shade;
		c.scroll = scroll;
	}

	// 0x6A7CE0: new spark (queue 0x2517AC8), x in +-xr/2, z0 + z in +-zr/2, y0 - [0, yr)
	static void SpawnSpark(int32_t z0, int32_t xr, int32_t zr, int32_t y0, int32_t yr)
	{
		if (Pause()) return;
		SparkNode *p = (SparkNode *)AddTaskToQueue(&QueueParticles().second, ORIG_SparkTask);
		if (!p) return;
		p->pos[0] = (int16_t)((Rand() & (xr - 1)) - (xr >> 1));
		p->pos[2] = (int16_t)((Rand() & (zr - 1)) - (zr >> 1) + z0);
		int32_t r = Rand();
		p->age = 0;
		p->pos[1] = (int16_t)(y0 - (r & (yr - 1)));
		p->vel[0] = (int16_t)((Rand() & 0x1FFF) - 0x1000);
		r = Rand();
		p->acc[1] = 0;
		p->acc[0] = 0;
		p->vel[1] = (int16_t)((r & 0x1FFF) - 0x1000);
	}

	// ------------------------------------------------------------------
	// Master task (0x6A6530)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SequenceTask(TaskNode *n)
	{
		MasterNode *node = (MasterNode *)n;
		g_ported_tick = g_real_tick;
		if (node->parity)
		{
			PacketCursor() = (uint32_t)(ModelBuffer() + 0x3758);
			node->parity = 0;
		}
		else
		{
			PacketCursor() = (uint32_t)(ModelBuffer() + 0x11758);
			node->parity = 1;
		}
		Flash() = 0;

		if (node->counter == 2 && !Pause() && !node->spawned)
		{
			node->spawned = 1;
			InitTaskQueuePool(&QueueCreature().first, ModelBuffer(), 0x878, 1);
			CreatureNode *c = (CreatureNode *)AddTaskToQueue(&QueueCreature().first, ORIG_CreatureTask);
			Memset32(&c->counter, 0, 0x21B);
			c->voice = ClaimVoiceSlot((const void *)0x11D0048, 1, 0x80);
			Mat4x3 &root = Root(c);
			root.m[2][2] = 0x2000;
			root.m[1][1] = 0x2000;
			root.m[0][0] = 0x2000;
			root.m[2][1] = 0;
			root.m[2][0] = 0;
			root.m[1][2] = 0;
			root.m[1][0] = 0;
			root.m[0][2] = 0;
			root.m[0][1] = 0;
			root.t[1] = 0;
			root.t[0] = 0;
			root.t[2] = Base();
			BindModelSections(E(c), c->sections, (const void *)0x11B537C);
			ReadAnimationStart(E(c), 0);
			uint8_t *action = *(uint8_t **)(CastCtx() + 4);
			c->ntargets = (int16_t)action[0x10];
			for (int k = 0; k < c->ntargets; k++)
			{
				uint8_t *act = *(uint8_t **)(CastCtx() + 4);
				uint32_t id = (*(uint8_t **)(act + 8))[0x18 * k];
				uint8_t *ent = (uint8_t *)(0x1D972C0 + 0x9C * id);
				Target(c, k) = ent;
				*SavedColor(c, k) = *(uint32_t *)(ent + 0x28);
			}
			StreamStateInit(ModelBuffer() + 0x1F758);
		}

		uint32_t creature_left = (uint32_t)n, sparks_left = (uint32_t)n; // the original keeps the node pointer: non-zero
		if (node->spawned)
		{
			EffectCameraMatrix(&Camera(), &EffectCamera());
			creature_left = (uint32_t)ExecuteTaskQueue(&QueueCreature().first);
			sparks_left = (uint32_t)ExecuteTaskQueue(&QueueParticles().second);
			ExecuteTaskQueue(&QueueParticles().first);
		}

		SetScreenFlash(Flash(), 0);
		if (Pause()) return 0;
		if (node->spawned && creature_left == 0 && sparks_left == 0)
			return DoneFlag() ? TASK_END : 0;
		node->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Creature timeline (0x6A6750)
	// ------------------------------------------------------------------

	// what the creature drew on the real tick (held frames draw between it and the next tick)
	struct CreatureMemo
	{
		uint32_t tick;
		const void *node;
		int16_t c0;     // counter on entry (fire trail stage)
		int16_t c1;     // counter after the fire trail stage (every later stage)
		bool model;     // the model was drawn
		bool fade;      // ... with the fade-in draw (0x6A7A50)
		Mat4x3 root;    // root matrix it was drawn with
	};
	static CreatureMemo g_creature = { 0xFFFFFFFF };

	// fire trail stage (counter 1..100, v4 = counter - 1): 26 fire flipbooks on a path
	// (s16 x3 points, 8 bytes apart, at 0x11D04BC) and their x mirror, then three scrolling
	// flame meshes fading in. The flipbook i sits at path position (86 * v4 - 43 * i) / 96.
	// dv/num/den: held frames advance v4 by dv * num / den (real tick: 0 / 0 / 1); real =
	// the camera cuts of v4 20 and 40 (vanilla writes them between the flipbooks).
	static void FireTrail(uint32_t v4, int32_t dv, int num, int den, bool real)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x98);
		RotY(0x800, h + 0x78);
		*(int32_t *)(h + 0x8C) = 0;
		*(int32_t *)(h + 0x90) = 0;
		*(int32_t *)(h + 0x94) = Base();
		int32_t span = 96 * den;
		int32_t v = (int32_t)(86 * v4) * den + 86 * dv * num;
		for (int i = 0; i < 26; i++, v -= 43 * den)
		{
			if (v < 0 || v >= 0x20A0 * den) continue;
			int32_t idx = v / span;
			int32_t f = shl32(v - idx * span, 12) / span;
			const uint8_t *path = (const uint8_t *)(0x11D04BC + 8 * idx);
			int16_t P[4] = { 0, 0, 0, 0 }; // 4th word: vanilla stack garbage (see the v4 20/40 cut)
			VecLerp(path, path + 8, 0x1000 - f, f, P);
			uint8_t *hdr = (uint8_t *)FieldAlloc(0xB4);
			P[2] = (int16_t)(Base() - P[2]);
			TransformCameraByShadowRotation(P, 0x640, -0x100);
			Flipbook(hdr, (int16_t)(i >> 1));
			if (real && i == 0 && v4 == 20)
			{
				// VANILLA: dword writes; the high word of LookAt z (0xB8B7FE, the vector pad) gets
				// the uninitialised stack word after P - not reproducible, the port keeps it
				var<uint32_t>(0xB8B7F8) = *(uint32_t *)&P[0];
				CamLookAt(2) = P[2];
				CamWorld(2) = (int16_t)(Base() + 0x7D0);
			}
			P[0] = (int16_t)-P[0];
			TransformCameraByShadowRotation(P, 0x640, -0x100);
			Flipbook(hdr, (int16_t)(i >> 1));
			if (real && i == 0 && v4 == 40)
			{
				var<uint32_t>(0xB8B7F8) = *(uint32_t *)&P[0];
				CamLookAt(2) = P[2];
				CamWorld(2) = (int16_t)(Base() + 0x7D0);
			}
			FieldFree(0xB4);
		}
		Mat4x3 *m = (Mat4x3 *)(h + 0x78);
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		*(int32_t *)(h + 0x38) = (int32_t)((uint32_t)shl32((int32_t)v4 * den + dv * num, 12) / (uint32_t)(95 * den)) - 0x1000;
		*(uint16_t *)(h + 0x16) = 0;
		*(uint16_t *)(h + 0x14) = 0;
		*(uint16_t *)(h + 0x1A) = 0x100;
		*(uint16_t *)(h + 0x18) = 0x100;
		*(uint16_t *)(h + 0x1E) = 0;
		*(uint16_t *)(h + 0x1C) = 0;
		*(uint16_t *)(h + 0x20) = 0x80;
		*(uint16_t *)(h + 0x22) = 0x100;
		*(uint16_t *)(h + 0x10) = (uint16_t)(v4 & 0x7F);
		*(uint32_t *)h = 0x11D04B4;
		*(uint32_t *)(h + 4) = 0x11D04BC;
		PacketCursor() = FlameMesh(h, OTBase() + 0x44, 2, PacketCursor());
		*(uint32_t *)h = 0x11D2594;
		*(uint16_t *)(h + 0x10) = (uint16_t)((v4 & 0x3F) << 1);
		*(uint32_t *)(h + 4) = 0x11D259C;
		PacketCursor() = FlameMesh(h, OTBase() + 0x44, 2, PacketCursor());
		*(uint32_t *)h = 0x11D3778;
		*(uint16_t *)(h + 0x10) = (uint16_t)((uint8_t)(0u - v4) & 0x7F);
		*(uint32_t *)(h + 4) = 0x11D3780;
		PacketCursor() = FlameMesh(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x98);
	}

	// tile of counter 93..116: fade out of a warm white
	static bool TileSpec93(int32_t c, uint8_t rgb[3])
	{
		uint32_t e = (uint32_t)(c - 93);
		if (e >= 0x18) return false;
		if (e < 8) return TileColor(0xFF, 0xC8, 0x8C, (int32_t)(8 - e), 8, rgb);
		return TileColor(0xFF, 0xC8, 0x8C, (int32_t)e - 8, 16, rgb);
	}
	// tiles of counter 195..254
	static bool TileSpec195a(int32_t c, uint8_t rgb[3])
	{
		uint32_t e = (uint32_t)(c - 195);
		if (e >= 0x3C) return false;
		return TileColor(0xB4, 0xB4, 0x78, (int32_t)e, 0x10, rgb);
	}
	static bool TileSpec195b(int32_t c, uint8_t rgb[3])
	{
		uint32_t e = (uint32_t)(c - 195);
		if (e >= 0x3C || e <= 0xC) return false;
		if (e > 0x10) return TileColor(0x96, 0x78, 0x4B, (int32_t)e - 0xC, 0x2C, rgb);
		return TileColor(0x96, 0x78, 0x4B, 4 - (ComputeSin(shl32((int32_t)e - 0xC, 8)) >> 10), 4, rgb);
	}

	static int32_t HazeAmp(uint32_t e) { return ComputeSin((int32_t)(((e - 40) << 11) / 20)) / 440; }

	// root translation y of the model at counter c (0x6A6C08..)
	static int32_t RootRise(int32_t c)
	{
		uint32_t e = (uint32_t)(c - 101);
		return e < 0x14 ? (int32_t)((0x14 - e) << 5) : 0;
	}

	static uint32_t __cdecl CreatureTask(TaskNode *n)
	{
		CreatureNode *cn = (CreatureNode *)n;
		CreatureMemo &memo = g_creature;
		memo.tick = g_real_tick;
		memo.node = n;
		memo.c0 = cn->counter;
		memo.model = false;

		// screen flash: ramps up over counter 0..7 and down over 248..255
		int16_t c = cn->counter;
		if (c < 8) Flash() = (uint32_t)(mul32(c, 2600) >> 3);
		else if (c > 0xF7) Flash() = (uint32_t)(mul32(0xFF - c, 2600) >> 3);
		else Flash() = 0xA28;

		// fire trail (counter 1..100)
		uint32_t v4 = (uint32_t)((int32_t)cn->counter - 1);
		if (v4 < 100)
		{
			FireTrail(v4, 0, 0, 1, true);
			if (!Pause())
			{
				if (v4 < 60)
				{
					if (v4 >= 20)
					{
						CamWorld(2) += 200;
						CamLookAt(2) += 100;
					}
					cn->counter++; // the trail runs at two counter steps per tick
				}
				else if (v4 == 60)
				{
					cn->counter = 0x51;
					CamLookAt(2) = (int16_t)Base();
					CamLookAt(0) = 0;
					CamLookAt(1) = 0;
					CamWorld(0) = 0;
					CamWorld(1) = (int16_t)0xF830;
					CamWorld(2) = (int16_t)(Base() + 0x7D0);
				}
				else if (v4 >= 80)
				{
					uint32_t t = ((v4 - 80) << 12) / 20;
					int16_t a[4] = { 0, 0, (int16_t)Base(), 0 };
					int16_t b[4] = { 0, 0x125, (int16_t)(Base() + 0xD1), 0 };
					VecLerp(a, b, 0x1000 - (int32_t)t, (int32_t)t, (void *)0xB8B7F8);
					a[0] = 0; a[1] = (int16_t)0xF830; a[2] = (int16_t)(Base() + 0x7D0);
					b[0] = 0; b[1] = (int16_t)0xF448; b[2] = (int16_t)(Base() + 0x1194);
					VecLerp(a, b, 0x1000 - (int32_t)t, (int32_t)t, (void *)0xB8B7F0);
				}
			}
		}
		memo.c1 = cn->counter;

		// white-out tile (counter 93..116)
		{
			uint8_t rgb[3];
			if (TileSpec93(cn->counter, rgb)) PacketCursor() = TileEmit(rgb, PacketCursor());
		}

		// the Phoenix model (counter 101..236)
		uint32_t e = (uint32_t)((int32_t)cn->counter - 101);
		if (e < 0x88)
		{
			if (!Pause()) AdvanceModelAnim(E(cn));
			memo.model = true;
			if (e < 0x14)
			{
				Root(cn).t[1] = (int32_t)((0x14 - e) << 5); // rises into place
				memo.fade = true;
				memo.root = Root(cn);
				DrawModelFade(E(cn), 0, ModelBuffer() + 0x878);
			}
			else
			{
				Root(cn).t[1] = 0;
				memo.fade = false;
				memo.root = Root(cn);
				FrameCursor() = DrawModel(E(cn), ModelBuffer() + 0x878, FrameCursor(), var<uint32_t>(0x1D969A8));
			}
		}

		// prim stage 1 (counter 101..150) + sparks every 4 ticks
		e = (uint32_t)((int32_t)cn->counter - 101);
		if (e < 0x32)
		{
			if (e == 0)
			{
				DecodePrimLayout((const void *)0x11C6C54, cn->prim_a, 0x178);
				InitTaskQueuePool(&QueueParticles().second, SparkPool(), 0x1C, 0x40);
			}
			if ((e & 3) == 0) SpawnSpark(Base(), 0x400, 0x800, 0, 0x400);
			PrimCtx ctx;
			CtxStage101(ctx, (int16_t)(e << 4));
			prim::play((prim::Layout *)cn->prim_a, PrimObject, (int)&ctx, (int)Pause());
		}

		// prim stage 2 (counter 151..194): two layouts, embers + sparks every 4 ticks
		e = (uint32_t)((int32_t)cn->counter - 151);
		if (e < 0x2C)
		{
			if (e == 0)
			{
				DecodePrimLayout((const void *)0x11C8FDC, cn->prim_a, 0x288);
				DecodePrimLayout((const void *)0x11CCBA0, cn->prim_b, 0x4E0);
				InitTaskQueuePool(&QueueParticles().first, EmberPool(), 0x18, 4);
			}
			if (!Pause() && (e & 3) == 0)
			{
				EmberNode *p = (EmberNode *)AddTaskToQueue(&QueueParticles().first, ORIG_EmberTask);
				if (p)
				{
					p->pos[0] = (int16_t)((Rand() & 0x1FF) - 0x100);
					p->pos[2] = (int16_t)((Rand() & 0x1FF) + Base() - 0x1C8);
					int32_t r = Rand();
					p->age = 0;
					p->pos[1] = (int16_t)(-0x200 - (r & 0x1FF));
					p->angle = (int16_t)Rand();
					p->spin = (int16_t)((Rand() & 0x3FF) - 0x200);
				}
				SpawnSpark(Base() - 200, 0x200, 0x400, 0, 0x200);
			}
			PrimCtx ctx;
			CtxIdentity(ctx, -0x400, (int16_t)(e << 3));
			if (e > 0x1E && !Pause())
			{
				CamWorld(0) += 0x30;
				CamWorld(1) += 0x30;
				CamWorld(2) -= 0x60;
			}
			prim::play((prim::Layout *)cn->prim_a, PrimObject, (int)&ctx, (int)Pause());
			ctx.shade = 0x20;
			ctx.m.t[0] = 0x100;
			ctx.m.t[1] = 0x200;
			prim::play((prim::Layout *)cn->prim_b, PrimObject, (int)&ctx, (int)Pause());
		}

		// prim stage 3 (counter 195..254): sparks, tiles, heat haze, targets tinted
		e = (uint32_t)((int32_t)cn->counter - 195);
		if (e < 0x3C)
		{
			for (int k = 4; k; k--) SpawnSpark(TargetsAvg() - 1000, 0x800, 0x200, 0, 0x800);
			if (e == 0) DecodePrimLayout((const void *)0x11CEF60, cn->prim_a, 0xF8);
			if (!Pause()) Root(cn).t[1] -= (int32_t)(e << 4); // (overwritten before the next model draw)
			uint8_t rgb[3];
			if (TileSpec195a(cn->counter, rgb)) PacketCursor() = TileEmit(rgb, PacketCursor());
			if (TileSpec195b(cn->counter, rgb)) PacketCursor() = TileEmit(rgb, PacketCursor());
			PrimCtx ctx;
			CtxIdentity(ctx, -0x400, (int16_t)((int32_t)e * 2 - (int32_t)(e << 4)));
			prim::play((prim::Layout *)cn->prim_a, PrimObject, (int)&ctx, (int)Pause());
			if (e >= 0x28)
			{
				int32_t s = ComputeSin((int32_t)(((e - 40) << 11) / 20));
				Haze(s / 440, (int32_t)(e << 7), 0x11);
				for (int k = 0; k < cn->ntargets; k++)
				{
					uint8_t *ent = Target(cn, k);
					ent[1] |= 8;
					RgbLerp((const void *)0x11D0160, SavedColor(cn, k), s, 0x1000 - s, ent + 0x28);
				}
			}
		}

		// streams
		c = cn->counter;
		uint8_t *stream = ModelBuffer() + 0x1F7A0;
		if (c == 0)
		{
			StreamLoad(0x210, stream, 1);
			StreamLoad(0x211, stream, 1);
			StreamLoad(0x212, stream, 0x10);
			StreamLoad(0x213, stream, 1);
			StreamLoad(0x214, stream, 3);
		}
		else if (c == 0x7E)
		{
			StreamLoad(0x215, stream, 4);
			StreamLoad(0x216, stream, 2);
		}
		else if (c == 0x96)
		{
			if (StreamBusy()) return 0;
			StreamLoad(0x217, stream, 1);
			StreamLoad(0x218, stream, 0);
		}
		else if (c == 0xC2)
		{
			if (StreamBusy()) return 0;
		}
		BattlePreload();
		c = cn->counter;
		if (c == 0x65)
		{
			if (StreamBusy()) return 0;
			PlaySummonStream(0x80, 1, 0x7F);
		}
		else if (c == 0xA1)
			PlaySummonStream(0x80, 1, 0x7F);
		c = cn->counter;
		if (c == 0) PlaySE(0x11D003C, 1, 0x80);
		else if (c == 0x65) PlaySE(0x11D0040, 1, 0x80);
		else if (c == 0xC3) PlaySE(0x11D0044, 1, 0x80);

		// camera
		c = cn->counter;
		if (c == 0)
		{
			var<int16_t>(0x1D977A2) = 0; // g_BattleCam_Roll
			CamLookAt(2) = (int16_t)Base();
			var<int16_t>(0x1D8E038) = 0x120;
			CamLookAt(0) = 0;
			CamLookAt(1) = 0;
			CamWorld(0) = 0;
			CamWorld(1) = (int16_t)0xFC18;
			CamWorld(2) = (int16_t)(Base() + 0x7D0);
		}
		else if (c == 0x64)
		{
			CamLookAt(0) = (int16_t)0xFE57;
			CamLookAt(1) = (int16_t)0xFCDD;
			CamWorld(0) = 0x0A91;
			CamLookAt(2) = (int16_t)(Base() - 0x77);
			CamWorld(1) = (int16_t)0xFC32;
			CamWorld(2) = (int16_t)(Base() + 0x2B);
		}
		else if (c >= 0x65 && c < 0x96)
		{
			// orbit about the look-at point by 8 per tick; from 121 also dolly in
			Mat4x3 rot;
			RotY(8, &rot);
			int16_t dx = (int16_t)(CamWorld(0) - CamLookAt(0));
			int16_t dy = (int16_t)(CamWorld(1) - CamLookAt(1));
			int16_t dz = (int16_t)(CamWorld(2) - CamLookAt(2));
			CamWorld(0) = dx;
			CamWorld(1) = dy;
			CamWorld(2) = dz;
			if (cn->counter >= 0x79)
			{
				int32_t d2 = (int32_t)((uint32_t)mul32(dz, dz) + (uint32_t)mul32(dx, dx) + (uint32_t)mul32(dy, dy));
				int32_t len = ISqrt(d2);
				int32_t k = 2 * (int32_t)cn->counter - 0xF2;
				CamWorld(0) = (int16_t)(CamWorld(0) - (int16_t)(mul32(CamWorld(0), k) / len));
				CamWorld(1) = (int16_t)(CamWorld(1) - (int16_t)(mul32(CamWorld(1), k) / len));
				int16_t z = (int16_t)(CamWorld(2) - (int16_t)(mul32(CamWorld(2), k) / len));
				CamLookAt(2) += 10;
				CamWorld(2) = z;
			}
			MatMulVec(&rot, (void *)0xB8B7F0, (void *)0xB8B7F0);
			CamWorld(0) += CamLookAt(0);
			CamWorld(1) += CamLookAt(1);
			CamWorld(2) += CamLookAt(2);
		}
		else if (c == 0x96)
		{
			CamLookAt(0) = 0x2E;
			CamLookAt(1) = (int16_t)0xFC15;
			CamWorld(0) = 0x112;
			CamLookAt(2) = (int16_t)(Base() - 0xB7);
			CamWorld(1) = (int16_t)0xFCB5;
			CamWorld(2) = (int16_t)(Base() - 0x2FD);
		}
		else if (c >= 0xC2)
		{
			int32_t f = (int32_t)c - 0xC2;
			int32_t a = shl32(f, 11) / 60;
			if (f == 0) CamWorld(1) = (int16_t)0xFEA4;
			Mat4x3 &root = Root(cn);
			root.m[2][2] = 0x3000;
			root.m[1][1] = 0x3000;
			root.m[0][0] = 0x3000;
			CamLookAt(0) = 0;
			int32_t s = ComputeSin(a + 0x80);
			CamWorld(0) = 0;
			CamLookAt(1) = (int16_t)(-0x44C - (s >> 2));
			CamLookAt(2) = (int16_t)(Base() - 0x1399);
			int32_t co = ComputeCos(a >> 1);
			// trunc(co / -190) through the compiler's magic-number division
			int32_t q = (int32_t)(((int64_t)co * (int32_t)0x53896E7B) >> 32) - co;
			q >>= 7;
			q += (int32_t)((uint32_t)q >> 31);
			CamWorld(1) += (int16_t)q;
			CamWorld(2) = (int16_t)(TargetsAvg() - 4 * f - 0x1398);
			if (cn->counter == 0xEB) StreamWaitDone(stream, &DoneFlag());
		}

		c = cn->counter;
		if (c == 0xF5)
		{
			uint8_t *action = *(uint8_t **)(CastCtx() + 4);
			ApplyActionResultToTargets(*(void **)(action + 8), action[0x10]);
		}
		else if (c == 0xFB)
			ReleaseVoiceSlot(cn->voice);

		cn->counter++;
		if (cn->counter < 0xFF) return 0;
		for (int k = 0; k < cn->ntargets; k++)
		{
			uint8_t *ent = Target(cn, k);
			*(uint16_t *)ent &= 0xF7FF;
			*(uint32_t *)(ent + 0x28) = *SavedColor(cn, k);
		}
		Flash() = 0;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Embers (0x6A7BF0): a prim model (0x11D4B1C) rising 25 per tick and spinning about Y,
	// fading to black (GTE depth cue factor age << 8) over 16 ticks
	// ------------------------------------------------------------------
	static void EmberDraw(const int16_t pos[3], int16_t angle, int32_t fade)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		Mat4x3 m;
		RotY(angle, &m);
		m.t[0] = pos[0];
		m.t[1] = pos[1];
		m.t[2] = pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = 0x11D4B1C;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(int32_t *)(h + 0xC) = fade;
		*(uint32_t *)(h + 0x1C) = 0xF0;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	struct EmberMemo { int16_t pos[3], angle, age; };
	static NodeMemo<EmberMemo, 16> g_ember_memo;

	static uint32_t __cdecl EmberTask(TaskNode *n)
	{
		EmberNode *p = (EmberNode *)n;
		EmberDraw(p->pos, p->angle, (int32_t)p->age << 8);
		if (Pause()) return 0;
		if (EmberMemo *m = g_ember_memo.put(n))
		{
			memcpy(m->pos, p->pos, sizeof(m->pos));
			m->angle = p->angle;
			m->age = p->age;
		}
		int16_t spin = p->spin;
		p->pos[1] -= 25;
		p->angle += spin;
		p->age++;
		return p->age < 16 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Sparks (0x6A7D90): flipbook 0x11D495C (frame = age) shot sideways with braking,
	// rising 75 per tick; 16 ticks
	// ------------------------------------------------------------------
	static void SparkDraw(const int16_t *pos, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, 0x400, -0x100);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = 0x11D495C;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	struct SparkMemo { int16_t pos[4], age; };
	static NodeMemo<SparkMemo, 128> g_spark_memo;

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *p = (SparkNode *)n;
		SparkDraw(p->pos, p->age); // (reads 8 bytes at pos: x, y, z, age)
		if (Pause()) return 0;
		if (SparkMemo *m = g_spark_memo.put(n))
		{
			memcpy(m->pos, p->pos, 8);
			m->age = p->age;
		}
		int16_t ax = p->acc[0];
		p->vel[0] += ax;
		int16_t vx = p->vel[0];
		int16_t az = p->acc[1];
		p->vel[1] += az;
		int16_t vz = p->vel[1];
		p->pos[1] -= 0x4B;
		p->pos[0] += (int16_t)(vx >> 4);
		p->pos[2] += (int16_t)(vz >> 4);
		if (vx > 0) p->acc[0] = (int16_t)(ax - 0x100);
		else if (vx < 0) p->acc[0] = (int16_t)(ax + 0x100);
		if (vz > 0) p->acc[1] = (int16_t)(az - 0x100);
		else if (vz < 0) p->acc[1] = (int16_t)(az + 0x100);
		p->age++;
		return p->age < 16 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Every stage is a function of the creature counter: the held frame
	// draws the stage the real tick drew (counter memo) at the in-between of its parameters and
	// the parameters of the next tick (the counter the creature holds now); what jumps
	// (cuts, stage changes, flipbook frames, texture scroll of the flame meshes) keeps the
	// 15 Hz steps. Nothing here changes game state: no RNG, no spawn, no camera write.
	// ------------------------------------------------------------------
	static void HeldTile(bool (*spec)(int32_t, uint8_t *), int32_t c, int32_t cn, int num, int den)
	{
		uint8_t a[3], b[3];
		if (!spec(c, a)) return; // not drawn on the real tick
		b[0] = a[0]; b[1] = a[1]; b[2] = a[2];
		if (cn != c && !spec(cn, b)) b[0] = b[1] = b[2] = 0; // gone next tick: fades out
		uint8_t rgb[3];
		for (int k = 0; k < 3; k++) rgb[k] = (uint8_t)lerp_i(a[k], b[k], num, den);
		PacketCursor() = TileEmit(rgb, PacketCursor());
	}

	// next-tick value if the next tick stays in the same stage (d = counter step 1..2), else hold
	static int32_t Toward(int32_t now, int32_t next, int32_t c, int32_t cnext, int32_t lo, int32_t hi, int num, int den)
	{
		if (cnext <= c || cnext > c + 2 || cnext < lo || cnext >= hi) return now;
		return lerp_i(now, next, num, den);
	}

	static void CreatureHeld(CreatureNode *cn, int num, int den)
	{
		const CreatureMemo &m = g_creature;
		if (m.tick != g_real_tick || m.node != cn) return;
		int32_t c0 = m.c0, c1 = m.c1, cx = cn->counter;

		// fire trail: path position advances by the counter step (2 per tick below 60)
		uint32_t v4 = (uint32_t)(c0 - 1);
		if (v4 < 100)
		{
			int32_t dv = (cx - 1) - (int32_t)v4;
			if (dv < 1 || dv > 2) dv = 0;
			FireTrail(v4, dv, num, den, false);
		}

		HeldTile(TileSpec93, c1, cx, num, den);

		// model: midpoint pose, root half way to the next draw's root
		if (m.model)
		{
			uint8_t *e = E(cn);
			Mat4x3 save = Root(cn), next = Root(cn), r = m.root;
			next.t[1] = RootRise(cx);
			if ((uint32_t)(cx - 101) < 0x88 && cx > c1)
			{
				for (int i = 0; i < 3; i++)
					for (int j = 0; j < 3; j++) r.m[i][j] = (int16_t)lerp_i(m.root.m[i][j], next.m[i][j], num, den);
				for (int i = 0; i < 3; i++) r.t[i] = lerp_i(m.root.t[i], next.t[i], num, den);
			}
			Root(cn) = r;
			pose_midpoint(e + 0x60, e + 0x6C, num, den);
			if (m.fade) DrawModelFade(e, 0, ModelBuffer() + 0x878);
			else FrameCursor() = DrawModel(e, ModelBuffer() + 0x878, FrameCursor(), var<uint32_t>(0x1D969A8));
			Root(cn) = save;
		}

		// prim stages: the shared player draws the in-between records; texture scroll in between
		uint32_t e1 = (uint32_t)(c1 - 101);
		if (e1 < 0x32)
		{
			PrimCtx ctx;
			CtxStage101(ctx, (int16_t)Toward((int32_t)(e1 << 4), (cx - 101) << 4, c1, cx, 101, 151, num, den));
			prim::play_held((prim::Layout *)cn->prim_a, PrimObject, (int)&ctx, num, den);
		}
		e1 = (uint32_t)(c1 - 151);
		if (e1 < 0x2C)
		{
			PrimCtx ctx;
			CtxIdentity(ctx, -0x400, (int16_t)Toward((int32_t)(e1 << 3), (cx - 151) << 3, c1, cx, 151, 195, num, den));
			prim::play_held((prim::Layout *)cn->prim_a, PrimObject, (int)&ctx, num, den);
			ctx.shade = 0x20;
			ctx.m.t[0] = 0x100;
			ctx.m.t[1] = 0x200;
			prim::play_held((prim::Layout *)cn->prim_b, PrimObject, (int)&ctx, num, den);
		}
		e1 = (uint32_t)(c1 - 195);
		if (e1 < 0x3C)
		{
			HeldTile(TileSpec195a, c1, cx, num, den);
			HeldTile(TileSpec195b, c1, cx, num, den);
			PrimCtx ctx;
			int32_t sc = (int32_t)e1 * 2 - (int32_t)(e1 << 4), scn = (cx - 195) * -14;
			CtxIdentity(ctx, -0x400, (int16_t)Toward(sc, scn, c1, cx, 195, 255, num, den));
			prim::play_held((prim::Layout *)cn->prim_a, PrimObject, (int)&ctx, num, den);
			if (e1 >= 0x28)
			{
				uint32_t en = (uint32_t)(cx - 195);
				int32_t amp = HazeAmp(e1), phase = (int32_t)(e1 << 7);
				if (en > e1 && en < 0x3C)
				{
					amp = lerp_i(amp, HazeAmp(en), num, den);
					phase = lerp_i(phase, (int32_t)(en << 7), num, den);
				}
				Haze(amp, phase, 0x11);
			}
		}
	}

	static void EmberHeld(EmberNode *p, int num, int den)
	{
		const EmberMemo *m = g_ember_memo.get(p);
		if (!m) return;
		int16_t pos[3];
		for (int k = 0; k < 3; k++) pos[k] = (int16_t)lerp_i(m->pos[k], p->pos[k], num, den);
		EmberDraw(pos, lerp_angle(m->angle, p->angle, num, den), lerp_i((int32_t)m->age << 8, (int32_t)p->age << 8, num, den));
	}

	static void SparkHeld(SparkNode *p, int num, int den)
	{
		const SparkMemo *m = g_spark_memo.get(p);
		if (!m) return;
		int16_t pos[4];
		for (int k = 0; k < 3; k++) pos[k] = (int16_t)lerp_i(m->pos[k], p->pos[k], num, den);
		pos[3] = m->age;
		SparkDraw(pos, m->age); // flipbook frame keeps its 15 Hz step
	}

	static uint8_t g_held_packets[0x60000];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static MasterNode *Master()
	{
		for (TaskNode *t = QueueCreature().second.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_SequenceTask) return (MasterNode *)t;
		return nullptr;
	}

	// mirrors the master's queue order; packets go to a private buffer (the module cursor and
	// the frame arena the model draw uses are redirected and put back); the effect-camera
	// matrix is rebuilt from the held-frame camera and put back
	static void HeldFrame(int num, int den)
	{
		MasterNode *master = Master();
		if (!master || !master->spawned) return;
		uint32_t cursor = PacketCursor(), frame_cursor = FrameCursor();
		Mat4x3 effect_camera = EffectCamera();
		PacketCursor() = (uint32_t)g_held_packets;
		FrameCursor() = (uint32_t)g_held_packets + 0x40000;
		EffectCameraMatrix(&Camera(), &EffectCamera());
		for (TaskNode *t = QueueCreature().first.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_CreatureTask) CreatureHeld((CreatureNode *)t, num, den);
		for (TaskNode *t = QueueParticles().second.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_SparkTask) SparkHeld((SparkNode *)t, num, den);
		for (TaskNode *t = QueueParticles().first.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_EmberTask) EmberHeld((EmberNode *)t, num, den);
		EffectCamera() = effect_camera;
		PacketCursor() = cursor;
		FrameCursor() = frame_cursor;
	}

	// ------------------------------------------------------------------
	// Held-frame camera (for the integrator's camera API; never writes the camera).
	// Camera = Battle_Camera_world (0xB8B7F0 x,y,z) / LookAt (0xB8B7F8 x,y,z). The next tick's
	// camera is predicted from the current one by the creature's camera code for the counter
	// it holds now; cuts (absolute writes) hold, everything else is lerped.
	// ------------------------------------------------------------------
	struct Cam { int16_t w[3], l[3]; };

	// the creature tick's camera writes for counter c (no pause, streams ready), on a copy
	static bool PredictCamera(int16_t c, Cam &k)
	{
		bool cut = false;
		int32_t base = Base();
		uint32_t v4 = (uint32_t)(c - 1);
		if (v4 < 100)
		{
			if (v4 == 20 || v4 == 40) // flipbook 0's position (mirrored at 40)
			{
				int32_t v = (int32_t)(86 * v4), idx = v / 96, f = shl32(v % 96, 12) / 96;
				const uint8_t *path = (const uint8_t *)(0x11D04BC + 8 * idx);
				int16_t P[4] = { 0, 0, 0, 0 };
				VecLerp(path, path + 8, 0x1000 - f, f, P);
				k.l[0] = v4 == 40 ? (int16_t)-P[0] : P[0];
				k.l[1] = P[1];
				k.l[2] = (int16_t)(base - P[2]);
				k.w[2] = (int16_t)(base + 0x7D0);
				cut = true;
			}
			if (v4 < 60)
			{
				if (v4 >= 20) { k.w[2] += 200; k.l[2] += 100; }
				c++;
			}
			else if (v4 == 60)
			{
				c = 0x51;
				k.l[0] = 0; k.l[1] = 0; k.l[2] = (int16_t)base;
				k.w[0] = 0; k.w[1] = (int16_t)0xF830; k.w[2] = (int16_t)(base + 0x7D0);
				cut = true;
			}
			else if (v4 >= 80)
			{
				uint32_t t = ((v4 - 80) << 12) / 20;
				int16_t a[4] = { 0, 0, (int16_t)base, 0 }, b[4] = { 0, 0x125, (int16_t)(base + 0xD1), 0 };
				int16_t o[4];
				VecLerp(a, b, 0x1000 - (int32_t)t, (int32_t)t, o);
				memcpy(k.l, o, 6);
				a[1] = (int16_t)0xF830; a[2] = (int16_t)(base + 0x7D0);
				b[1] = (int16_t)0xF448; b[2] = (int16_t)(base + 0x1194);
				VecLerp(a, b, 0x1000 - (int32_t)t, (int32_t)t, o);
				memcpy(k.w, o, 6);
			}
		}
		uint32_t e = (uint32_t)(c - 151);
		if (e < 0x2C && e > 0x1E) { k.w[0] += 0x30; k.w[1] += 0x30; k.w[2] -= 0x60; }
		if (c == 0 || c == 0x64 || c == 0x96 || c == 0xC2) cut = true;
		else if (c >= 0x65 && c < 0x96)
		{
			Mat4x3 rot;
			RotY(8, &rot);
			int16_t d[4] = { (int16_t)(k.w[0] - k.l[0]), (int16_t)(k.w[1] - k.l[1]), (int16_t)(k.w[2] - k.l[2]), 0 };
			if (c >= 0x79)
			{
				int32_t len = ISqrt((int32_t)((uint32_t)mul32(d[2], d[2]) + (uint32_t)mul32(d[0], d[0]) + (uint32_t)mul32(d[1], d[1])));
				int32_t kk = 2 * (int32_t)c - 0xF2;
				for (int i = 0; i < 3; i++) d[i] = (int16_t)(d[i] - (int16_t)(mul32(d[i], kk) / len));
				k.l[2] += 10;
			}
			MatMulVec(&rot, d, d);
			for (int i = 0; i < 3; i++) k.w[i] = (int16_t)(d[i] + k.l[i]);
		}
		else if (c > 0xC2)
		{
			int32_t f = (int32_t)c - 0xC2, a = shl32(f, 11) / 60;
			k.l[0] = 0;
			k.w[0] = 0;
			k.l[1] = (int16_t)(-0x44C - (ComputeSin(a + 0x80) >> 2));
			k.l[2] = (int16_t)(base - 0x1399);
			int32_t co = ComputeCos(a >> 1);
			int32_t q = (int32_t)(((int64_t)co * (int32_t)0x53896E7B) >> 32) - co;
			q >>= 7;
			q += (int32_t)((uint32_t)q >> 31);
			k.w[1] += (int16_t)q;
			k.w[2] = (int16_t)(TargetsAvg() - 4 * f - 0x1398);
		}
		return cut;
	}
}

	// held-frame camera of effect 140 at tick + num / den: false = the module does not drive
	// the camera now (creature not running / not ported this tick)
	bool mag140_held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		using namespace p140;
		const CreatureMemo &m = g_creature;
		if (m.tick != g_real_tick || !HeldReady()) return false;
		CreatureNode *cn = nullptr;
		for (TaskNode *t = QueueCreature().first.head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_CreatureTask && t == m.node) cn = (CreatureNode *)t;
		if (!cn) return false;
		Cam now, next;
		for (int i = 0; i < 3; i++) { now.w[i] = CamWorld(i); now.l[i] = CamLookAt(i); }
		next = now;
		bool cut = PredictCamera(cn->counter, next);
		for (int i = 0; i < 3; i++)
		{
			world[i] = cut ? now.w[i] : (int16_t)lerp_i(now.w[i], next.w[i], num, den);
			lookat[i] = cut ? now.l[i] : (int16_t)lerp_i(now.l[i], next.l[i], num, den);
		}
		return true;
	}

	void register_mag140_phoenix()
	{
		register_port(p140::ORIG_SequenceTask, (void *)p140::SequenceTask, "P140 SequenceTask", 140);
		register_port(p140::ORIG_CreatureTask, (void *)p140::CreatureTask, "P140 CreatureTask", 140, true);
		register_port(p140::ORIG_EmberTask, (void *)p140::EmberTask, "P140 EmberTask", 140, true);
		register_port(p140::ORIG_SparkTask, (void *)p140::SparkTask, "P140 SparkTask", 140, true);
		register_module_held(140, p140::HeldReady, p140::HeldFrame);
	}
}
