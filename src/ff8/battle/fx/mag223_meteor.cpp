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
// Effect 223: Meteor (cinematic-engine spell, module MAG_223 0xA8F860..0xA9B1D0, script data
// magic/mag222_b.00/.01, loaded by MAG_223_METEOR_FL 0xA8F860).
//
// Like Water (mag222_water.cpp), Meteor is one more compile of the GF cinematic engine
// (gfc_engine.cpp): SetupSummon 0xA8F890, SequenceTick 0xA8FF00, BindContext 0xA8F940,
// AnimChannelsNeg 0xA9AC00, AnimIntegrator 0xA90220, AnimChannelsPos 0xA9B020 are byte clones of
// Ifrit's, BuildMatricesAndDraw 0xA95CD0 is the unlit variant (= Brothers 0xAF9ED0). Every table
// handler is the generic one except
//   VM 0x006 LoadBattleFile 0xA969C0: same code, own load callback 0xA96AF0 (c.load_cb),
//   VM 0x096 0xA99660 CaptureScreenStrips, draw 16 0xA907B0 ScrollTextureU, draw 26 0xA91B90
//     ScreenCaptureStrips: the shared ports (apply_shared_misc),
//   prim 6 0xA93160 TintedF3 (= Eden 0xAEB5B0, mag_eden.cpp apply_eden_d),
// and the handlers only Meteor has, ported here:
//   VM 0x035 0xA996A0, VM 0x0BB 0xA997A0, VM 0x0BD 0xA99730, draw 25 0xA90AA0, draw 28 0xA91F20.
// The effect's queue (0x2796B70) holds one task, the engine tick 0xA8FF00.

#include "gfc_engine.h"

#ifdef FF8_FX_HELD
#include "mag223_meteor_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
	void apply_eden_d(Clone &c);  // mag_eden.cpp

namespace meteor
{
	static Clone g_meteor = {};

	static void describe(Clone &c)
	{
		c.name = "223 Meteor";
		c.effect_id = 223;
		c.vm_table = 0x186C314;     // GF_223Meteor_VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x186BF84;   // GF_223Meteor_BoneHandlerTable
		c.prim_table = 0x186BFCC;   // bone_table + 0x48
		c.prim_size = 0x186C0BC;    // bone_table + 0x138
		c.spr_table = 0x186C0D0;    // bone_table + 0x14C
		c.ptype_table = 0x186C0FC;  // bone_table + 0x178
		c.pop_table = 0x186C110;    // bone_table + 0x18C
		c.draw_table = 0x186C170;   // GF_223Meteor_DrawHandlerTable
		c.attr_16A = 0x186C2DA;
		c.attr_184 = 0x186C2F4;
		c.attr_186 = 0x186C2F6;
		c.attr_194 = 0x186C304;
		c.attr_19C = 0x186C30C;
		c.desc = 0x186BE34;         // draw_table - 0x33C
		c.ptr_block = 0x186BF5C;    // draw_table - 0x214
		c.queue = 0x2796B70;
		c.file00 = 0x2796BA4;
		c.file01 = 0x2796BA0;
		c.tick = 0xA8FF00;
		c.load_cb = 0xA96AF0;
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// prim 6 (TintedF3) is the only Eden handler Meteor carries: take that one slot from
	// apply_eden_d and keep every other slot as init_clone built it
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_prim6(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_eden_d(c);
		VoidFn p6 = c.prim[6];
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.prim[6] = p6;
	}

	// ------------------------------------------------------------------------------------
	// helpers and wrappers
	// ------------------------------------------------------------------------------------
	static const uint32_t RECTRING = 0x27978E8;      // g_GfCinematic_BlitRectRing[16] (engine block)
	static const uint32_t RECTRING_IDX = 0x2798BF4;  // its u8 index (engine block)

	// 0x505E70 Battle_QueueVramReadback_Type2(rect, dst): queues a VRAM -> memory copy
	static inline void xl_QueueVramReadback(const void *rect, uint32_t dst) { FX_HELD(if (held_predicting()) return;) x::f<void (__cdecl *)(const void *, uint32_t)>(0x505E70)(rect, dst); }
	// 0xB66670 (shared blob): Magic_ReadAlternativeTexture(id) + ws+0xF8 = its texture-window
	// command (0xE2......); only writes the workspace
	static inline void xl_TexWindow_B66670(int32_t id) { x::f<int32_t (__cdecl *)(int32_t)>(0xB66670)(id); }
	// 0x56C8C0: RTPS of v -> SXY (>> 3 each) at sxy, returns OTZ (SZ3 / 4)
	static inline int32_t xl_RotTransPers_56C8C0(const void *v, void *sxy, void *p, void *flag) { return x::f<int32_t (__cdecl *)(const void *, void *, void *, void *)>(0x56C8C0)(v, sxy, p, flag); }

	// 0xB66F90 (shared blob, ported here): copies the 8-byte RECT src into the ring slot
	// RECTRING + 8 * (u8 [RECTRING_IDX] & 0xF), Battle_QueueVramReadback_Type2(slot, dst),
	// ++u8 [RECTRING_IDX]
	static void StoreRect_B66F90(const uint8_t *src, uint32_t dst)
	{
		uint8_t *slot = (uint8_t *)(RECTRING + 8u * (MEM<uint8_t>(RECTRING_IDX) & 0xFu));
		U16(slot, 0) = U16(src, 0);
		U16(slot, 2) = U16(src, 2);
		U16(slot, 4) = U16(src, 4);
		U16(slot, 6) = U16(src, 6);
		xl_QueueVramReadback(slot, dst);
		MEM<uint8_t>(RECTRING_IDX) = (uint8_t)(MEM<uint8_t>(RECTRING_IDX) + 1);
	}

	// 0xB66F40 (shared blob, ported here): same ring, Battle_QueueVramUpload_Type0_RectData(slot, data)
	static void LoadRect_B66F40(const uint8_t *src, uint32_t data)
	{
		uint8_t *slot = (uint8_t *)(RECTRING + 8u * (MEM<uint8_t>(RECTRING_IDX) & 0xFu));
		U16(slot, 0) = U16(src, 0);
		U16(slot, 2) = U16(src, 2);
		U16(slot, 4) = U16(src, 4);
		U16(slot, 6) = U16(src, 6);
		x::QueueVramUpload(slot, (const void *)data);
		MEM<uint8_t>(RECTRING_IDX) = (uint8_t)(MEM<uint8_t>(RECTRING_IDX) + 1);
	}

	static inline uint32_t MagicTexBuf() { return MEM<uint32_t>(0x1D99A88); } // MAGIC_TEXTURE_BUFFER_PTR

	// ------------------------------------------------------------------------------------
	// VM opcodes
	// ------------------------------------------------------------------------------------
	// 0xA996A0 (VM 0x035 VramReadback): opcode bit 0x8000 (RT+0x4B & 0x80): operands {s16 rect
	// offset from the cursor, s16 slot}: RECT at cursor + off read back into the magic texture
	// buffer + 16 * slot; cursor += 6. Else operands {RECT (8 bytes)}: bone+0xC4 = arena block of
	// w * h * 2 bytes (RECT +4 * +6), the RECT at cursor + 2 read back into it; cursor += 10.
	// Held: VM ops only run on real ticks and in the prediction (readback skipped there).
	static void __cdecl op_035_VramReadback()
	{
		uint8_t *s = STREAM();                                           // ecx
		if (U8(RT(), 0x4B) & 0x80)
		{
			int32_t slot = S16(s, 4);
			uint8_t *src = s + S16(s, 2);
			uint32_t dst = MagicTexBuf() + ((uint32_t)slot << 4);
			STREAM() = s + 6;
			StoreRect_B66F90(src, dst);
			return;
		}
		uint8_t *src = s + 2;                                            // esi
		int32_t n = shl32(mul32(S16(s, 8), S16(s, 6)), 1);
		uint8_t *blk = blob::ArenaAlloc(n);
		FX_HELD(guard(CUR() + 0xC4, 4);)
		PTR(CUR(), 0xC4) = blk;
		uint32_t dst = U32(CUR(), 0xC4);
		STREAM() = STREAM() + 10;
		StoreRect_B66F90(src, dst);
	}

	// 0xA99730 (VM 0x0BD VramUpload): opcode bit 0x8000: operands {s16 rect offset from the
	// cursor, s16 slot}: RECT at cursor + off uploaded from the magic texture buffer + 16 * slot;
	// cursor += 6. Without the bit, vanilla passes an uninitialised local (the entry ecx, saved by
	// `push ecx`) as both arguments and does not advance the cursor: that path calls the original
	// handler (Meteor's script always sets the bit).
	static void __cdecl op_0BD_VramUpload()
	{
		if (!(U8(RT(), 0x4B) & 0x80))
		{
			FX_HELD(if (held_predicting()) return;)
			x::f<VoidFn>(0xA99730)();
			return;
		}
		uint8_t *s = STREAM();                                           // ecx
		uint32_t data = MagicTexBuf() + ((uint32_t)S16(s, 4) << 4);
		uint8_t *rect = s + S16(s, 2);
		STREAM() = s + 6;
		LoadRect_B66F40(rect, data);
	}

	// 0xA997A0 (VM 0x0BB CopyAltTextureToBuffer): operands {s16 texture id, s16 slot}:
	// Magic_ReadAlternativeTexture(id) (ws+0xF0 = its RECT, ws+0xFC = its pixels); n = w * h * 2
	// bytes copied as dwords from the pixels to the magic texture buffer + 16 * slot, memmove
	// style: backwards when src < dst (vanilla quirk: offsets n-4 down to 4, the first dword is
	// never copied, nothing at all when n <= 4), forwards otherwise ((n + 3) / 4 dwords);
	// cursor += 6. The copy is skipped while predicting (texture data only, uploaded by VM 0x0BD,
	// never read by the prediction).
	static void __cdecl op_0BB_CopyAltTextureToBuffer()
	{
		blob::ReadAltTexture(S16(STREAM(), 2));
		uint8_t *w = WS();
		uint32_t dst = MagicTexBuf() + ((uint32_t)S16(STREAM(), 4) << 4); // ecx
		uint32_t src = U32(w, 0xFC);                                     // esi
		uint8_t *r = PTR(w, 0xF0);
		int32_t n = shl32(mul32(S16(r, 6), S16(r, 4)), 1);               // eax
		bool copy = true;
		FX_HELD(if (held_predicting()) copy = false;)
		if (copy)
		{
			if ((int32_t)src < (int32_t)dst)
			{
				n -= 4;
				if (n > 0)
				{
					uint32_t d = (uint32_t)n + dst;                      // edx
					uint32_t k = ((uint32_t)n + 3) >> 2;
					uint32_t off = src - dst;
					do
					{
						MEM<uint32_t>(d) = MEM<uint32_t>(off + d);
						d -= 4;
					} while (--k != 0);
				}
			}
			else if (n > 0)
			{
				uint32_t k = ((uint32_t)n + 3) >> 2;
				uint32_t off = src - dst;
				uint32_t d = dst;
				do
				{
					MEM<uint32_t>(d) = MEM<uint32_t>(d + off);
					d += 4;
				} while (--k != 0);
			}
		}
		STREAM() = STREAM() + 6;
	}

	// ------------------------------------------------------------------------------------
	// draw 28
	// ------------------------------------------------------------------------------------
	// 0xA91F20 (draw 28 SpreadActionResults): first call (bone+0xBC == 0): bone+0xB8 = s16 at
	// the parameter pointer (duration), bone+0xC0 = 0 (results applied), bone+0xC4 = action
	// target count (ctx+0xCC action +0x10). Every call while i = bone+0xC0 < n = bone+0xC4:
	// when (i + 1) * duration / (n + 1) <= bone+0xBC, ApplyActionResultToTarget(target record
	// i (24 bytes each, action +8)), ++i; ++bone+0xBC. Not gated by boneSkipFlag.
	// Held: skipped (a held frame must not apply results nor advance the counters).
	static void __cdecl dh_28_SpreadActionResults()
	{
		FX_HELD(if (g_held.active) return;)
		uint8_t *b = CUR();                                              // ecx
		if (U32(b, 0xBC) == 0)
		{
			U32(b, 0xB8) = (uint32_t)(int32_t)S16(PTR(b, 0xB8), 0);
			U32(CUR(), 0xC0) = 0;
			uint8_t *act = PTR(CTX(), 0xCC);
			U32(CUR(), 0xC4) = U8(act, 0x10);
			b = CUR();
		}
		int32_t n = S32(b, 0xC4);                                        // edx
		int32_t i = S32(b, 0xC0);                                        // esi
		if (i >= n) return;
		int32_t q = mul32(i + 1, S32(b, 0xB8)) / (n + 1);
		if (q <= S32(b, 0xBC))
		{
			uint8_t *act = PTR(CTX(), 0xCC);
			x::ApplyActionResultToTarget((int32_t)(U32(act, 8) + (uint32_t)(i * 3) * 8u));
			U32(CUR(), 0xC0) = U32(CUR(), 0xC0) + 1;
			b = CUR();
		}
		U32(b, 0xBC) = U32(b, 0xBC) + 1;
	}

	// ------------------------------------------------------------------------------------
	// draw 25
	// ------------------------------------------------------------------------------------
	// 0xA91790: GTE rotation / translation = parent matrix (cur bone +0x9C), then
	// TR = R * outPos(bone ref) + TR (GTE_MVMVA_RotV0_Tr + sub_45E580)
	static void SetupRefXform_A91790(int32_t ref)
	{
		uint8_t *b = blob::GetBone(ref);                                  // edi
		Mat4x3 *m = blob::GetParentMatrix((int32_t)U16(CUR(), 0x9C));    // esi
		x::GteSetRotMatrix(m);
		x::GteSetTransVector(m);
		x::GteLoadV0(b + 0x94);
		x::GteMVMVA_RotV0Tr();
		x::Gte_45E580();
	}

	static inline int32_t divz256(int32_t v) { return (v + ((v >> 31) & 0xFF)) >> 8; }

	// 0xA917F0: vertex colour of the current grid vertex: ws+0xBC = ws+0x58 (colour | code);
	// fade-in / fade-out by the vertex counter ws+0x5C against the thresholds ws+0xB4 / ws+0xB0
	// with the factors ws+0xBA / ws+0xB8 (8.8, stepped by ws+0xB6 / ws+0xB2 per call), top byte kept
	static uint32_t scale_rgb(uint32_t c, int32_t f)
	{
		uint32_t r = (uint32_t)shl32(divz256(mul32((int32_t)((c >> 16) & 0xFF), f)), 8);
		r |= (uint32_t)divz256(mul32((int32_t)((c >> 8) & 0xFF), f));
		r <<= 8;
		r |= (uint32_t)divz256(mul32((int32_t)(c & 0xFF), f));
		return r | (c & 0xFF000000u);
	}

	static void VertexColour_A917F0()
	{
		uint8_t *w = WS();
		U32(w, 0xBC) = U32(w, 0x58);
		uint32_t c = U32(w, 0x58);                                        // ebx
		uint32_t hi = c & 0xFF000000u;                                    // esi
		if (U16(w, 0xB6) != 0 && (int32_t)S16(w, 0xB4) <= S32(w, 0x5C))
		{
			int16_t f = S16(w, 0xBA);
			if (f <= 0) U32(w, 0xBC) = hi;
			else U32(w, 0xBC) = scale_rgb(c, f);
			U16(w, 0xBA) = (uint16_t)(U16(w, 0xBA) + U16(w, 0xB6));
			return;
		}
		if (U16(w, 0xB2) == 0) return;
		if ((int32_t)S16(w, 0xB0) >= S32(w, 0x5C))
		{
			U32(w, 0xBC) = hi;
			return;
		}
		int16_t f = S16(w, 0xB8);
		if (f >= 0x100) U32(w, 0xBC) = c;
		else U32(w, 0xBC) = scale_rgb(c, f);
		U16(w, 0xB8) = (uint16_t)(U16(w, 0xB8) + U16(w, 0xB2));
	}

	// first row vertex into ws+0xC0 (x, z from angle a and radius v4; y = vC >> 16, plus a sine
	// wave when the wave bone exists; the y is kept per column at ws+0x40), y / z swapped on flag
	// 0x10
	static void GridVertexFirstRow(int32_t a, int32_t v4, int32_t vC, bool wave)
	{
		uint8_t *w = WS();
		U16(w, 0xC0) = (uint16_t)(mul32(x::Cos(a), v4) >> 16);
		U16(WS(), 0xC2) = (uint16_t)(mul32(x::Sin(a), v4) >> 16);
		w = WS();
		if (!wave)
		{
			U16(w, 0xC4) = (uint16_t)(vC >> 16);
			S32(PTR(w, 0x40), 0) = S16(w, 0xC4);
		}
		else
		{
			int32_t s = x::Sin(shl32(S16(WS(), 0xA6), 4));
			w = WS();
			U16(w, 0xC4) = (uint16_t)(add32(vC, mul32(s, S16(w, 0xA0)) >> 4) >> 16);
			S32(PTR(w, 0x40), 0) = S16(w, 0xC4);
			U16(w, 0xA2) = (uint16_t)(U16(w, 0xA2) + U16(w, 0xA4));
			U16(w, 0xA0) = (uint16_t)(U16(w, 0xA0) + U16(w, 0xA2));
			U16(w, 0xA8) = (uint16_t)(U16(w, 0xA8) + U16(w, 0xAA));
			U16(w, 0xA6) = (uint16_t)(U16(w, 0xA6) + U16(w, 0xA8));
		}
		if (U8(w, 0xAC) & 0x10)
		{
			uint16_t d = U16(w, 0xC4);
			int16_t cc = S16(w, 0xC2);
			U16(w, 0xC2) = d;
			U16(w, 0xC4) = (uint16_t)cc;
		}
		U32(w, 0x40) = U32(w, 0x40) + 4;
	}

	// the following rows: y from the per-column array ws+0x40
	static void GridVertexNextRow(int32_t a, int32_t v4)
	{
		uint8_t *w = WS();
		U16(w, 0xC0) = (uint16_t)(mul32(x::Cos(a), v4) >> 16);
		U16(WS(), 0xC2) = (uint16_t)(mul32(x::Sin(a), v4) >> 16);
		w = WS();
		U16(w, 0xC4) = U16(PTR(w, 0x40), 0);
		if (U8(w, 0xAC) & 0x10)
		{
			uint16_t d = U16(w, 0xC4);
			int16_t cc = S16(w, 0xC2);
			U16(w, 0xC2) = d;
			U16(w, 0xC4) = (uint16_t)cc;
		}
		U32(w, 0x40) = U32(w, 0x40) + 4;
	}

	// 0xA90AA0 (draw 25 TexturedRibbonGrid): a textured, gouraud grid of rows x columns quads
	// (packets 0x44: tag, texture window 0xE2 + 0, GT4, 0xE2000000 + 0) swept around the
	// reference bone (param +6): per row the radius (ws+0x7C.., bone outAngleZ / outPosX), angle
	// (ws+0x68.., reference outAngleZ), height (ws+0x8C.., reference outPosZ / outAngleX /
	// outAngleY) and texture u / v advance; optional sine wave from the wave bone (param +8);
	// vertex colour fades (0xA917F0, 4 x u16 at bone+0xBC, default 0x27971E4 = zeros).
	// Parameter block bone+0xB8: +0 rows, +2 v step, +4 columns, +6 ref bone, +8 wave bone,
	// +0xA texture id, +0xC flags (1 = height centred, 0x10 = swap y / z). Depth rows in the arena
	// scratch ctx+0x74 / +0x200, per-column y at ctx+0x74 + 0x400.
	// Vanilla quirks reproduced: flag 1 clear -> the depth buffer choice tests an uninitialised
	// local ([ebp-0x30], 0 here; unreached by Meteor's script); rows <= 0 -> ctx+0x7C =
	// [ebp-0x20] + 0x44 with [ebp-0x20] possibly never written (0 here); division by the row /
	// column count (0 faults as in vanilla).
	// Held: idempotent (every per-call value is rebuilt from the bones; bone+0xC4 / +0xBC are only
	// set on the first call): the held frame draws the in-between bone state with the same code.
	static void __cdecl dh_25_TexturedRibbonGrid()
	{
		uint8_t *b = CUR();
		uint32_t v20 = 0;                                  // [ebp-0x20] (uninitialised in vanilla)
		int32_t v30 = 0;                                   // [ebp-0x30] (uninitialised in vanilla)
		if (U32(b, 0xC4) == 0)
		{
			U32(b, 0xC4) = 0xFFFFFFFF;
			b = CUR();
			if (U32(b, 0xBC) == 0)
			{
				U32(b, 0xBC) = 0x27971E4;
				b = CUR();
			}
		}
		uint8_t *prm = PTR(b, 0xB8);                       // esi
		uint8_t *fade = PTR(b, 0xBC);                      // edi
		SetupRefXform_A91790(S16(prm, 6));
		uint8_t *ref = blob::GetBone(S16(prm, 6));         // [ebp-0x1c]
		S32(WS(), 0xF0) = S16(prm, 8);
		uint8_t *ref2 = nullptr;                           // [ebp-0x24]
		if (S32(WS(), 0xF0) != 0) ref2 = blob::GetBone(S32(WS(), 0xF0));
		U32(WS(), 0xE0) = U32(CTX(), 0x74) + 0x400;
		xl_TexWindow_B66670(S16(prm, 0xA));
		uint8_t *w = WS();
		U32(w, 0xE4) = U32(w, 0xF8);
		{
			uint8_t *r = PTR(w, 0xF0);
			int32_t tw = S16(r, 4);
			if (U8(w, 0xF4) & 0x80) tw += tw;
			S32(w, 0x30) = tw;
			S32(WS(), 0x34) = S16(r, 6);
		}
		w = WS();
		S32(w, 0x50) = S16(CUR(), 0x92);
		S32(w, 0x54) = S16(CUR(), 0x9A);
		U32(w, 0x58) = U32(CUR(), 0xCC) | 0x3C000000u;
		U32(w, 0x5C) = 0;
		U32(w, 0x60) = U32(CTX(), 0x7C);
		U32(w, 0x98) = 0;
		U32(w, 0x9C) = 0;
		S32(w, 0x64) = S16(prm, 0);
		S32(w, 0x68) = shl32(S16(ref, 0x90), 16);
		S32(w, 0x6C) = 0x10000000 / S32(w, 0x64);
		S32(w, 0x70) = S16(prm, 4);
		S32(w, 0x74) = S16(CUR(), 0x8E);
		S32(w, 0x7C) = shl32(S16(CUR(), 0x90), 8);
		S32(w, 0x78) = sub32(shl32(S16(CUR(), 0x94), 8), S32(w, 0x7C)) / S32(w, 0x70);
		S32(w, 0x20) = shl32(S16(CUR(), 0x8C), 16);
		S32(w, 0x24) = shl32(S32(w, 0x30), 16) / S32(w, 0x70);
		S32(w, 0x28) = 0;
		S32(w, 0x2C) = S16(prm, 2);
		S32(w, 0x84) = shl32(S16(CUR(), 0x96), 8);
		S32(w, 0x88) = shl32(S16(CUR(), 0x98), 8);
		U16(w, 0xAC) = U16(prm, 0xC);
		U16(w, 0xB0) = U16(fade, 0);
		U16(w, 0xB2) = U16(fade, 2);
		U16(w, 0xB4) = U16(fade, 4);
		U16(w, 0xB6) = U16(fade, 6);
		U16(w, 0xB8) = 0;
		U16(w, 0xBA) = 0x100;
		if (ref2 == nullptr)
		{
			U32(w, 0xA0) = 0;
			U32(w, 0xA4) = 0;
			U32(w, 0xA8) = 0;
		}
		else
		{
			U16(w, 0xA0) = (uint16_t)((uint32_t)U8(ref2, 0x94) << 8);
			U16(w, 0xA2) = U16(ref2, 0x96);
			U16(w, 0xA4) = U16(ref2, 0x98);
			U16(w, 0xA6) = U16(ref2, 0x8C);
			U16(w, 0xA8) = U16(ref2, 0x8E);
			U16(w, 0xAA) = U16(ref2, 0x90);
		}
		S32(w, 0x90) = shl32(S16(ref, 0x8C), 16);
		S32(w, 0x94) = shl32(S16(ref, 0x8E), 12);
		int32_t flag;                                      // ecx at 0xA90EB3
		if (!(U8(w, 0xAC) & 1))
		{
			S32(w, 0x8C) = shl32(S16(ref, 0x98), 16);
			flag = v30;
		}
		else
		{
			flag = S32(w, 0x70);
			int32_t st = S32(w, 0x94);                     // esi
			int32_t e = S32(w, 0x90);                      // edx
			int32_t sum = 0;                               // ebx
			v20 = (uint32_t)st;
			if (flag > 1)
			{
				int32_t k = flag - 1;
				flag = 1;
				do
				{
					e = add32(e, st);
					sum = add32(sum, e);
				} while (--k != 0);
			}
			S32(w, 0x8C) = sub32(shl32(S16(ref, 0x98), 16), sum);
		}
		U32(w, 0x40) = U32(w, 0xE0);
		int32_t v1c = S32(w, 0x84);
		int32_t v18 = S32(w, 0x78);
		int32_t v10 = S32(w, 0x7C);
		uint8_t *pk = PTR(w, 0x60);                        // edi
		int32_t vC = S32(w, 0x8C);
		int32_t v28 = S32(w, 0x90);
		int32_t v14 = S32(w, 0x68);
		int32_t ebx = S32(w, 0x20);
		uint8_t *v8 = flag == 0 ? PTR(CTX(), 0x74) : PTR(CTX(), 0x74) + 0x200;
		int32_t v2c = 0;
		GridVertexFirstRow(v14 >> 16, v10 >> 4, vC, ref2 != nullptr);
		w = WS();
		U16(v8, 0) = (uint16_t)xl_RotTransPers_56C8C0(w + 0xC0, pk + 0x10, w + 0xFC, w + 0xFC);
		w = WS();
		{
			int32_t r = (ebx >> 16) % S32(w, 0x30);
			uint8_t u = (uint8_t)add32(r, S32(w, 0x98));
			U8(pk, 0x2C) = u;
			U8(pk, 0x14) = u;
			uint8_t v = (uint8_t)(U8(w, 0x9C) + U8(w, 0x28));
			U8(pk, 0x21) = v;
			U8(pk, 0x15) = v;
		}
		VertexColour_A917F0();
		U32(pk, 0x24) = U32(WS(), 0xBC);
		U32(pk, 0x0C) = U32(pk, 0x24);
		S32(WS(), 0x5C) = S32(WS(), 0x5C) + 1;
		if (S32(WS(), 0x70) > 0)
		{
			uint8_t *nx = pk + 0x54;                       // esi
			v30 = S32(WS(), 0x70);
			for (;;)
			{
				w = WS();
				v18 = add32(v18, S32(w, 0x74));
				v10 = add32(v10, v18);
				v1c = add32(v1c, S32(w, 0x88));
				v14 = add32(v14, v1c);
				v28 = add32(v28, S32(w, 0x94));
				vC = add32(vC, v28);
				ebx = add32(ebx, S32(w, 0x24));
				v2c = ebx;
				GridVertexFirstRow(v14 >> 16, v10 >> 4, vC, ref2 != nullptr);
				w = WS();
				U16(v8, 0) = (uint16_t)xl_RotTransPers_56C8C0(w + 0xC0, nx, w + 0xFC, w + 0xFC);
				U32(pk, 0x1C) = U32(nx, 0);
				VertexColour_A917F0();
				U32(pk, 0x30) = U32(WS(), 0xBC);
				U32(pk, 0x18) = U32(pk, 0x30);
				U32(nx, 0x14) = U32(WS(), 0xBC);
				U32(nx, -4) = U32(nx, 0x14);
				w = WS();
				int32_t r = (v2c >> 16) % S32(w, 0x30);
				if (r != 0)
				{
					int32_t e = add32(r, S32(w, 0x98));
					int32_t a0 = U8(pk, 0x14);
					v20 = (uint32_t)a0;
					if (a0 < e)
					{
						U8(pk, 0x38) = (uint8_t)e;
						U8(pk, 0x20) = (uint8_t)e;
						U8(nx, 0x1C) = (uint8_t)e;
						U8(nx, 4) = (uint8_t)e;
					}
					else
					{
						U8(nx, 0x1C) = (uint8_t)e;
						U8(nx, 4) = (uint8_t)e;
						uint8_t s = (uint8_t)(U8(WS(), 0x30) + (uint8_t)e);
						U8(pk, 0x38) = s;
						U8(pk, 0x20) = s;
					}
				}
				else
				{
					U8(nx, 0x1C) = U8(w, 0x98);
					U8(nx, 4) = U8(w, 0x98);
					uint8_t s = (uint8_t)(U8(WS(), 0x98) + U8(WS(), 0x30));
					U8(pk, 0x38) = s;
					U8(pk, 0x20) = s;
				}
				uint8_t v15 = U8(pk, 0x15);
				pk += 0x44;
				U8(nx, 0x11) = v15;
				U8(nx, 5) = v15;
				nx += 0x44;
				S32(WS(), 0x5C) = S32(WS(), 0x5C) + 1;
				if (--v30 == 0) break;
			}
		}
		w = WS();
		if (S32(w, 0x64) > 0)
		{
			do
			{
				w = WS();
				U32(w, 0x40) = U32(w, 0xE0);
				uint8_t *prev = pk - (uint32_t)S32(w, 0x70) * 0x44u;        // esi
				S32(w, 0x28) = add32(S32(w, 0x28), S32(w, 0x2C));
				v18 = S32(w, 0x78);
				v10 = S32(w, 0x7C);
				S32(w, 0x68) = add32(S32(w, 0x68), S32(w, 0x6C));
				v1c = S32(w, 0x84);
				v14 = S32(w, 0x68);
				uint8_t *base = PTR(CTX(), 0x74);
				uint8_t *dA, *dB;                          // [ebp-8], [ebp-0xc]
				if (!(U8(w, 0x64) & 1)) { dA = base + 0x200; dB = base; }
				else { dA = base; dB = base + 0x200; }
				GridVertexNextRow(v14 >> 16, v10 >> 4);
				uint8_t *sx = pk + 0x10;                   // ebx
				w = WS();
				U16(dA, 0) = (uint16_t)xl_RotTransPers_56C8C0(w + 0xC0, sx, w + 0xFC, w + 0xFC);
				{
					uint32_t s0 = U32(sx, 0);
					uint32_t c0 = U32(prev, 0xC);
					U32(prev, 0x28) = s0;
					U32(pk, 0x24) = c0;
					U32(pk, 0x0C) = c0;
					uint32_t c1 = U32(prev, 0x18);
					U32(pk, 0x30) = c1;
					U32(pk, 0x18) = c1;
					uint8_t u = U8(prev, 0x14);
					U8(pk, 0x2C) = u;
					U8(pk, 0x14) = u;
					u = U8(prev, 0x20);
					U8(pk, 0x38) = u;
					U8(pk, 0x20) = u;
				}
				w = WS();
				int32_t r = S32(w, 0x28) % S32(w, 0x34);
				if (r != 0)
				{
					int32_t e = add32(r, S32(w, 0x9C));
					U8(prev, 0x39) = (uint8_t)e;
					U8(prev, 0x2D) = (uint8_t)e;
					U8(pk, 0x21) = (uint8_t)e;
					U8(pk, 0x15) = (uint8_t)e;
					S32(WS(), 0x3C) = e;
					S32(WS(), 0x38) = e;
				}
				else
				{
					U8(pk, 0x21) = U8(w, 0x9C);
					U8(pk, 0x15) = U8(w, 0x9C);
					uint8_t s = (uint8_t)(U8(WS(), 0x9C) + U8(WS(), 0x34));
					U8(prev, 0x39) = s;
					U8(prev, 0x2D) = s;
					S32(WS(), 0x38) = S32(WS(), 0x9C);
					S32(WS(), 0x3C) = add32(S32(WS(), 0x9C), S32(WS(), 0x34));
				}
				w = WS();
				if (S32(w, 0x70) > 0)
				{
					v30 = S32(w, 0x70);
					do
					{
						U8(prev, 3) = 0x10;
						U32(prev, 8) = 0;
						U32(prev, 0x3C) = 0xE2000000u;
						U32(prev, 0x40) = 0;
						sx += 0x44;
						U16(prev, 0x22) = U16(WS(), 0x50);
						U16(prev, 0x16) = U16(WS(), 0x54);
						U32(prev, 4) = U32(WS(), 0xE4);
						w = WS();
						v18 = add32(v18, S32(w, 0x74));
						v10 = add32(v10, v18);
						v1c = add32(v1c, S32(w, 0x88));
						v14 = add32(v14, v1c);
						GridVertexNextRow(v14 >> 16, v10 >> 4);
						w = WS();
						int32_t z = xl_RotTransPers_56C8C0(w + 0xC0, sx, w + 0xFC, w + 0xFC);
						int32_t sum = (int32_t)S16(dB, 0) + (int32_t)S16(dA, 0);
						v20 = (uint32_t)prev;
						dA += 2;
						prev += 0x44;
						dB += 2;
						pk += 0x44;
						U16(dA, 0) = (uint16_t)z;
						int32_t t = add32((int32_t)S16(dB, 0) + sum, z);
						uint32_t s0 = U32(sx, 0);
						U32(pk, -0x28) = s0;
						U32(prev, -0x10) = s0;
						t = (t + ((t >> 31) & 3)) >> 2;
						uint32_t m = U32(prev, -0x38);
						U32(pk, -0x20) = m;
						U32(pk, -0x38) = m;
						m = U32(prev, -0x2C);
						U32(pk, -0x14) = m;
						U32(pk, -0x2C) = m;
						t >>= 2;
						uint8_t d = U8(WS(), 0x38);
						U8(sx, 0x11) = d;
						U8(sx, 5) = d;
						d = U8(WS(), 0x3C);
						U8(prev, -0xB) = d;
						U8(prev, -0x17) = d;
						d = U8(prev, 0x14);
						U8(sx, 0x1C) = d;
						U8(sx, 4) = d;
						U32(prev, 0x28) = s0;
						d = U8(prev, 0x20);
						U8(sx, 0x28) = d;
						U8(sx, 0x10) = d;
						if (t > 0 && t < 0xFFF) x::InsertPrimAltViewport(U32(RT(), 0x4C) + (uint32_t)t * 4u, (void *)v20);
					} while (--v30 != 0);
				}
				w = WS();
				S32(w, 0x64) = S32(w, 0x64) - 1;
			} while (S32(WS(), 0x64) > 0);
		}
		U32(CTX(), 0x7C) = v20 + 0x44;
	}

	static void apply_meteor_ports(Clone &c)
	{
		c.vm[0x035] = op_035_VramReadback;              // 0xA996A0
		c.vm[0x0BB] = op_0BB_CopyAltTextureToBuffer;    // 0xA997A0
		c.vm[0x0BD] = op_0BD_VramUpload;                // 0xA99730
		c.draw[25] = dh_25_TexturedRibbonGrid;          // 0xA90AA0
		c.draw[28] = dh_28_SpreadActionResults;         // 0xA91F20
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_meteor);
	}
}
}

	void register_mag223_meteor()
	{
		using namespace gfc;
		meteor::describe(meteor::g_meteor);
		init_clone(meteor::g_meteor, nullptr, 0);
		apply_shared_misc(meteor::g_meteor);    // VM 0x096, draw 16, draw 26
		meteor::apply_prim6(meteor::g_meteor);
		meteor::apply_meteor_ports(meteor::g_meteor);
		register_port(0xA8FF00, (void *)meteor::SequenceTask, "MAG223 Meteor SequenceTick", 223);
		// 30 fps layer: see mag223_meteor_held.inc
		FX_HELD(register_mag223_meteor_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag223_meteor_held.inc"
#endif
