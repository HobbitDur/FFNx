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

// Effect 221: Apocalypse (the magic Apocalypse; enemy attack 373 of kernel.bin, Apocalypse of Ultimecia
// (final form) c0m126), cinematic-engine module MAG_221 0xAA5530..0xAB32F0, script data
// magic/mag220_b.00 / .01 (loaded by MAG_221_APOCALYPSE_FL 0xAA5530: IO_GetFile_MAGIC of the names
// 0x186DCD4 / 0x186DCE0; setup MAG_221_APOCALYPSE 0xAA5560 = effect table index 220).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with a LONGER handler
// block: 0x80 bytes of module data (a byte table of grid V coordinates 0x186D4F0.. and the 24 colour
// words 0x186D510.. of draw 50's pieces) sit between the prim sizes and the sprite table, so the sprite
// / particle tables and the draw table are 0x80 further from the bone table than in the standard
// layout (bone table 0x186D3A4 = draw table 0x186D610 - 0x26C; the descriptor block 0x186D234 sits
// 0x3DC below the draw table). Instruction-level comparison against the ported clones with that layout
// (module-relative addresses normalised, table regions symbolic, jump tables and callee trees hashed;
// work\gfcR\tools\cmpall221.py / best221.py / fd221.py):
//   - SetupSummon 0xAA55C0, BindContext 0xAA5610, the section pointers 0xAA56B0, InitSequenceState
//     0xAB2900, SequenceTick 0xAA5BD0 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0xAB32D0 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of Mega Flare 257's slot (the generic (Ifrit) handler, a
//     `ret` stub or prim 58 below), except
//       VM 0x006 LoadBattleFile 0xAAE910: same code, own load callback 0xAAEA40 (c.load_cb),
//       VM 0x05C 0xAB2460 = Brothers 0xAFEBF0 / Leviathan 0xB64090, prim 16 0xAAA2C0 = Cerberus 0xB105B0
//         / Eden 0xAEC030 TintedF4 (apply_shared_misc, only those slots taken),
//       draw 37 0xAA7890 RibbonTrail (apply_shared_ribbon, only that slot taken),
//       draw 61 0xAA6910 = Eden 0xAE71D0 MeshRadialLightWave (apply_eden_b, only that slot taken),
//       prim 58 0xAAAE60 TintedFT4Abr = Mega Flare 257's 0x9725F0 / Breath of Death 232's 0xA5F610
//         (static there: same code below),
//       bone 11 0xAA6270 PolarXY, draw 50 0xAABB70 ObjectShatter (+ 0xAABFF0 / 0xAAC130 / 0xAAC5A0),
//       draw 51 0xAAB290 / draw 68 0xAAB6D0 FoggedGridSheet variants (Terra Break 219's draw 67 0xAC2230
//         with other UV rules), draw 69 0xAA6770 EntityTint: Apocalypse's own handlers, ported here.
// The effect's queue (0x2796BE0) holds one task, the engine tick 0xAA5BD0.
// Battle entity writes: draw 69 writes the flags word +0 and the colour word +0x28 of the party or
// enemy entities (SceneHeader+0x60..); the battle camera words through the engine's camera ops (VM 0x039).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace apocalypse221
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag221_apocalypse_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
	void apply_eden_b(Clone &c);  // mag_eden.cpp

namespace apocalypse221
{
	static void describe(Clone &c)
	{
		c.name = "221 Apocalypse";
		c.effect_id = 221;
		c.vm_table = 0x186D7B4;      // MAG_221 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x186D3A4;    // MAG_221 BoneHandlerTable (draw_table - 0x26C)
		c.prim_table = 0x186D3EC;    // bone_table + 0x48
		c.prim_size = 0x186D4DC;     // bone_table + 0x138
		c.spr_table = 0x186D570;     // bone_table + 0x1CC (after the 0x80 bytes of module data 0x186D4F0)
		c.ptype_table = 0x186D59C;   // spr_table + 0x2C
		c.pop_table = 0x186D5B0;     // spr_table + 0x40
		c.draw_table = 0x186D610;    // MAG_221 DrawHandlerTable
		c.attr_16A = 0x186D77A;
		c.attr_184 = 0x186D794;
		c.attr_186 = 0x186D796;
		c.attr_194 = 0x186D7A4;
		c.attr_19C = 0x186D7AC;
		c.desc = 0x186D234;          // draw_table - 0x3DC (read by 0xAA56B0)
		c.ptr_block = 0x186D37C;     // bone_table - 0x28 (read by 0xAA5610)
		c.queue = 0x2796BE0;
		c.file00 = 0x2796C10;
		c.file01 = 0x2796C14;
		c.tick = 0xAA5BD0;
		c.load_cb = 0xAAEA40;        // passed by VM 0x006 0xAAE910
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	// module data (FF8_EN.exe .rdata, read only): draw 68's V byte per grid row, draw 50's piece colours
	static inline uint8_t RowV_186D4FC(int32_t i) { return MEM<uint8_t>(0x186D4FC + i); }
	static inline uint32_t PieceColour_186D510(int32_t i) { return MEM<uint32_t>(0x186D510 + 4 * i); }

	// ------------------------------------------------------------------------------------
	// engine calls not wrapped by gfc_engine.h
	// ------------------------------------------------------------------------------------
	// GTE data registers (0x1CA8A10 + 4 * reg)
	inline uint32_t &GteData(int reg) { return MEM<uint32_t>(0x1CA8A10 + 4 * reg); }
	// 0x56CC50 GTE_SetFogNearFar(near, far, h): fog globals 0x209AB64 / 0xC78BF0 + GTE DQA / DQB
	static inline void xl_SetFogNearFar_56CC50(int32_t a, int32_t b, int32_t h) { x::f<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x56CC50)(a, b, h); }
	// 0x56CAA0 GTE_RotTransPersNormalColor(v, n, colour, sxy, rgb, flag): RTPS of v -> sxy, NCDS-style lit colour -> rgb, returns OTZ
	static inline int32_t xl_RotTransPersNormalColor_56CAA0(const void *v, const void *n, const void *c, void *sxy, void *rgb, void *flag)
	{
		return x::f<int32_t (__cdecl *)(const void *, const void *, const void *, void *, void *, void *)>(0x56CAA0)(v, n, c, sxy, rgb, flag);
	}
	// 0x56CA30 colour of a normal: lit colour of (normal n, colour col) -> out
	static inline void xl_NormalColor_56CA30(const void *n, const void *col, void *out) { x::f<void (__cdecl *)(const void *, const void *, void *)>(0x56CA30)(n, col, out); }
	// 0x56C850 RotTrans: LoadV0(v); MVMVA(R*V0+TR); IR1..3 -> out (3 x s16); FLAG -> *flag
	static inline void xl_RotTrans(const void *v, void *out, void *flag) { x::f<void (__cdecl *)(const void *, void *, void *)>(0x56C850)(v, out, flag); }
	// 0x45C7A0 SSIGPU_InsertPrimAutoDepth(ot, prim): links a packet (and bumps the depth-key cursor 0x1CA8828)
	static inline void xl_InsertPrimAutoDepth(uint32_t ot, void *prim) { FX_HELD(if (held_predicting()) return;) x::f<void (__cdecl *)(uint32_t, void *)>(0x45C7A0)(ot, prim); }
	// x / 3 as compiled (imul 0x55555556, + sign bit of the high half)
	static inline int32_t div3(int32_t v)
	{
		int32_t hi = (int32_t)(((int64_t)v * 0x55555556LL) >> 32);
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// ------------------------------------------------------------------------------------
	// 0xAA6270 (bone 11 PolarXY): polar around the bone S16(bone+0xB0) in the XY plane: radius =
	// accumPosX >> 8, angle = HIWORD(accumPosY), z = HIWORD(accumPosZ) (bone 3 Polar's axes turned)
	// ------------------------------------------------------------------------------------
	static void __cdecl bh_11_PolarXY()
	{
		uint8_t *o = blob::GetBone(S16(CUR(), 0xB0));                         // edi
		const int32_t ang = S32(CUR(), 0x60) >> 16;                           // esi
		const int32_t c = x::Cos(ang);
		S16(CUR(), 0x94) = (int16_t)((uint16_t)(mul32(S32(CUR(), 0x5C) >> 8, c >> 4) >> 16) + U16(o, 0x94));
		const int32_t s = x::Sin(ang);
		S16(CUR(), 0x96) = (int16_t)((uint16_t)(mul32(S32(CUR(), 0x5C) >> 8, s >> 4) >> 16) + U16(o, 0x96));
		S16(CUR(), 0x98) = (int16_t)((uint16_t)(S32(CUR(), 0x64) >> 16) + U16(o, 0x98));
	}

	// ------------------------------------------------------------------------------------
	// 0xAA6770 (draw 69 EntityTint): the battle entities of SceneHeader+0x60 + 4 * (s + k): the enemies
	// (s = 3, 4 slots) when SceneHeader+0x48 <= 2, else the party (s = 0, 3 slots). outPos x | y | z
	// negative: each entity's flags word +0 = the saved word SceneHeader+0x4E + 2 * (s + k); all zero:
	// flags +0 |= 0xC; else colour word +0x28 = 0x02000000 | z << 16 | y << 8 | x (low bytes) and flags
	// = the saved word | 0x800.
	// ------------------------------------------------------------------------------------
	static void EntityTint_AA6770()
	{
		uint8_t *sc = SCENE();                                                // ebx
		int32_t s, n;                                                         // edi, eax
		if (S8(sc, 0x48) > 2) { s = 0; n = 3; }
		else { s = 3; n = 4; }
		uint8_t *cur = CUR();                                                 // esi
		const int32_t v = (int32_t)S16(cur, 0x98) | (int32_t)S16(cur, 0x96) | (int32_t)S16(cur, 0x94);
		if (v & 0x8000)
		{
			for (int32_t k = 0; k < n; k++)
			{
				uint8_t *e = PTR(SCENE(), 0x60 + 4 * (s + k));
				if (e) U16(e, 0) = U16(SCENE(), 0x4E + 2 * (s + k));
			}
			if (v < 0) return;
		}
		if (v == 0)
		{
			for (int32_t k = 0; k < n; k++)
			{
				uint8_t *e = PTR(SCENE(), 0x60 + 4 * (s + k));
				if (e) U8(e, 0) |= 0xC;
			}
			return;
		}
		if (v <= 0) return;
		const uint32_t col = (((uint32_t)U8(CUR(), 0x98) | 0x200) << 16) | ((uint32_t)U8(cur, 0x96) << 8) | U8(CUR(), 0x94);
		for (int32_t k = 0; k < n; k++)
		{
			uint8_t *e = PTR(SCENE(), 0x60 + 4 * (s + k));
			if (!e) continue;
			U32(e, 0x28) = col;
			U16(e, 0) = (uint16_t)(U16(SCENE(), 0x4E + 2 * (s + k)) | 0x800);
		}
	}

	static void __cdecl dh_69_EntityTint()
	{
		EntityTint_AA6770();
		// 30 fps layer: the entity words this draw left (mag221_apocalypse_held.inc)
		FX_HELD(TintNote();)
	}

	// ------------------------------------------------------------------------------------
	// 0xAAB290 (draw 51 FoggedGridSheet) / 0xAAB6D0 (draw 68 FoggedGridSheet, row V table): Terra
	// Break 219's draw 67 0xAC2230 (mag219_terra_break.cpp dh_67_FoggedGridSheet) with other UV rules:
	// bone+0xB8 = {s16 reference bone, s16 texture id, s16 W, s16 H, s16 fog near, s16 fog far}; a
	// (W+1) x (H+1) vertex grid 0x200 apart in the bone's XZ plane (Y = outPos Y << 4, offset by
	// outPos X / Z & 0x1FF), each vertex projected and lit (0x56CAA0, colour 0x3C808080), drawn as W x H
	// Gouraud textured quads (GT4, 0x34 bytes) in the alternate viewport, depth slot bone+0xC0 when
	// bone+0xDE bit 0x20 else the cell's mean depth.
	//   draw 51: U from -(W/2 << 5) - (outPos Z >> 4 & 0x1E0) stepping 0x20 per cell (bytes, the right
	//            edge 0xFF when the next U is a multiple of 0x100: (u + 0x20) & 0xF0 == 0), V from
	//            -(H/2 << 5) - (outPos X >> 4 & 0x1E0) stepping 0x20 per row (bottom edge 0xFF likewise);
	//   draw 68: U as draw 51 but from (outPos Z >> 4 & 0x1E0); V = the module bytes 0x186D4FC[r] /
	//            [r + 1], r = ((-(H/2 << 5) - (outPos X >> 4 & 0x3E0)) >> 5) & 0xF counting on mod 16.
	// ------------------------------------------------------------------------------------
	static void GridSheet(bool rowtab)
	{
		const uint8_t *d = PTR(CUR(), 0xB8);                                  // esi
		S32(WS(), 0x60) = S16(d, 4);
		S32(WS(), 0x64) = S16(d, 6);
		const int32_t tex = S16(d, 2);                                        // edi
		blob::TexInfo_B665C0(tex);
		U32(WS(), 0x68) = U32(WS(), 0xFC);
		S32(WS(), 0x6C) = blob::ClutWord_B66640(tex);
		uint8_t *ws = WS();                                                   // ecx
		{
			const int32_t a = shl32(S32(ws, 0x60) / 2, 5);
			const int32_t e = ((int32_t)U16(CUR(), 0x98) >> 4) & 0x1E0;
			S32(ws, 0x70) = add32(sub32((int32_t)~(uint32_t)a, e), 1);
		}
		ws = WS();
		{
			const int32_t a = shl32(S32(ws, 0x64) / 2, 5);
			if (!rowtab)
			{
				const int32_t e = ((int32_t)U16(CUR(), 0x94) >> 4) & 0x1E0;
				S32(ws, 0x74) = add32(sub32((int32_t)~(uint32_t)a, e), 1);
			}
			else
			{
				const int32_t e = ((int32_t)U16(CUR(), 0x94) >> 4) & 0x3E0;
				S32(ws, 0x74) = add32(sub32((int32_t)~(uint32_t)a, e), 1) >> 5;
			}
		}
		ws = WS();
		S32(ws, 0x78) = mul32(S32(ws, 0x60) + 1, 12);
		uint8_t *ref = blob::GetBone(S16(d, 0));                              // ebx
		if (U8(CUR(), 0xDE) & 0x20) U32(WS(), 0x7C) = U32(CUR(), 0xC0);
		else U32(WS(), 0x7C) = 0xFFFFFFFFu;
		const int16_t angy = S16(CUR(), 0x8E);                                // edi
		S16(CUR(), 0x8E) = 0x1000;
		h_B27130();
		S16(CUR(), 0x8E) = angy;
		h_B29450((int32_t)U32(CUR(), 0xCC));
		xl_SetFogNearFar_56CC50(S16(d, 8), S16(d, 0xA), 0x200);
		const uint32_t rc = U32(ref, 0xCC);
		x::GteSetBackColor3((int32_t)(rc & 0xFF), (int32_t)((rc >> 8) & 0xFF), (int32_t)((rc >> 16) & 0xFF));
		uint8_t *v = PTR(CTX(), 0x74);                                        // ebx
		ws = WS();
		S16(ws, 0xC0) = (int16_t)((U16(CUR(), 0x94) & 0x1FF) - shl32(S32(ws, 0x64) / 2, 9));
		ws = WS();
		const int32_t z0 = (int32_t)(U16(CUR(), 0x98) & 0x1FF) - shl32(S32(ws, 0x60) / 2, 9);   // [ebp-8]
		S16(ws, 0xC2) = 0;
		S16(WS(), 0xC8) = (int16_t)(U16(CUR(), 0x96) << 4);
		S16(WS(), 0xCA) = 0;
		S16(WS(), 0xCC) = 0;
		U32(WS(), 0xE0) = 0x3C808080;
		const uint32_t code = (U32(CUR(), 0xCC) & 0x2000000) | 0x3C000000;   // edi
		ws = WS();
		if (S32(ws, 0x64) >= 0)
		{
			for (int32_t r = S32(ws, 0x64) + 1; r > 0; r--)
			{
				S16(ws, 0xC4) = (int16_t)z0;
				ws = WS();
				if (S32(ws, 0x60) >= 0)
				{
					for (int32_t c = S32(ws, 0x60) + 1; c > 0; c--)
					{
						U16(v, 4) = (uint16_t)xl_RotTransPersNormalColor_56CAA0(ws + 0xC0, ws + 0xC8, ws + 0xE0, v, v + 8, ws + 0xFC);
						U32(v, 8) |= code;
						v += 0xC;
						S16(WS(), 0xC4) = (int16_t)(S16(WS(), 0xC4) + 0x200);
						ws = WS();
					}
				}
				S16(ws, 0xC0) = (int16_t)(S16(ws, 0xC0) + 0x200);
				ws = WS();
			}
		}
		uint8_t *row0 = PTR(CTX(), 0x74);                                     // ebx
		uint8_t *row1 = row0 + S32(ws, 0x78);                                 // edi
		uint8_t *pk = PTR(CTX(), 0x7C);                                       // esi
		int32_t vv = rowtab ? (S32(ws, 0x74) & 0xF) : S32(ws, 0x74);          // [ebp-4]
		for (int32_t r = S32(ws, 0x64); r > 0; r--)
		{
			int32_t u = S32(ws, 0x70);
			const int32_t w = S32(ws, 0x60);
			if (w > 0)
			{
				const int32_t v2 = vv + 0x20;                                     // [ebp-0x14] (draw 51)
				for (int32_t c = w; c > 0; c--)
				{
					U8(pk, 3) = 0xC;
					U32(pk, 4) = U32(row0, 8);
					U32(pk, 0x10) = U32(row0, 0x14);
					U32(pk, 0x1C) = U32(row1, 8);
					U32(pk, 0x28) = U32(row1, 0x14);
					U16(pk, 0x1A) = U16(WS(), 0x68);
					U16(pk, 0xE) = U16(WS(), 0x6C);
					U8(pk, 0x24) = (uint8_t)u;
					U8(pk, 0xC) = (uint8_t)u;
					u += 0x20;
					const uint8_t u2 = (u & 0xF0) ? (uint8_t)u : 0xFF;
					U8(pk, 0x30) = u2;
					U8(pk, 0x18) = u2;
					if (!rowtab)
					{
						U8(pk, 0x19) = (uint8_t)vv;
						U8(pk, 0xD) = (uint8_t)vv;
						const uint8_t vb = (v2 & 0xF0) ? (uint8_t)v2 : 0xFF;
						U8(pk, 0x31) = vb;
						U8(pk, 0x25) = vb;
					}
					else
					{
						const uint8_t va = RowV_186D4FC(vv);
						U8(pk, 0x19) = va;
						U8(pk, 0xD) = va;
						const uint8_t vb = RowV_186D4FC(vv + 1);
						U8(pk, 0x31) = vb;
						U8(pk, 0x25) = vb;
					}
					U32(pk, 8) = U32(row0, 0);
					U32(pk, 0x14) = U32(row0, 0xC);
					U32(pk, 0x20) = U32(row1, 0);
					U32(pk, 0x2C) = U32(row1, 0xC);
					int32_t z = S32(WS(), 0x7C);
					if (z < 0) z = (S16(row1, 0x10) + S16(row1, 4) + S16(row0, 0x10) + S16(row0, 4)) / 4;
					x::InsertPrimAltViewport(((uint32_t)z & 0xFFFFFFFCu) + U32(RT(), 0x4C), pk);
					row0 += 0xC;
					row1 += 0xC;
					pk += 0x34;
				}
				ws = WS();
			}
			vv = rowtab ? ((vv + 1) & 0xF) : vv + 0x20;
			row0 += 0xC;
			row1 += 0xC;
		}
		PTR(CTX(), 0x7C) = pk;
	}
	static void __cdecl dh_51_FoggedGridSheet() { GridSheet(false); }
	static void __cdecl dh_68_FoggedGridSheetRows() { GridSheet(true); }

	// ------------------------------------------------------------------------------------
	// draw 50 ObjectShatter: bone+0xBC = block {+0 s16 desc+6, +2 0, +4 s16 desc+4, +6 s16 the
	// reference bone desc+0, +0xA piece count, +0xC object, +0x10 / +0x12 the reference bone's
	// outPos y / outAngle y at the first draw} followed by one 0x24-byte piece per triangle of the object
	// desc+2 {+0 state (0 on the object, 1 loose, | 2 thrown), +2 age, +4..+8 centroid, +0xA thrown
	// age, +0xC radius, +0xE / +0x10 radial speed / acceleration, +0x12 angle around the reference
	// bone, +0x14 / +0x16 angular speed / acceleration, +0x18..+0x1A spin bytes, +0x1B..+0x1D their
	// speeds, +0x20 / +0x22 scale / its speed}; bone+0xC0 = the object's vertices transformed once.
	// ------------------------------------------------------------------------------------
	// 0xAAC5A0 PieceStep(p): nothing when RT+0x45 (boneSkipFlag); age + 1; thrown: radial motion while
	// the thrown age < ws+0x80 (age + 1); loose: radial motion while age < ws+0x7C, the scale growing from
	// age ws+0x90; then the angle and the spin bytes always step.
	static void PieceStep_AAC5A0(uint8_t *p)
	{
		uint8_t *ws = WS();                                                   // edx
		if (U8(RT(), 0x45) != 0) return;
		const uint8_t st = U8(p, 0);                                          // bl
		U16(p, 2) = (uint16_t)(U16(p, 2) + 1);
		const int32_t age = S16(p, 2);
		if (st & 2)
		{
			const uint16_t ta = U16(p, 0xA);
			if ((int32_t)(int16_t)ta < S32(ws, 0x80))
			{
				U16(p, 0xE) = (uint16_t)(U16(p, 0xE) + U16(p, 0x10));
				U16(p, 0xC) = (uint16_t)(U16(p, 0xC) + U16(p, 0xE));
				U16(p, 0x14) = (uint16_t)(U16(p, 0x14) + U16(p, 0x16));
				U16(p, 0xA) = (uint16_t)(ta + 1);
			}
		}
		else if (age < S32(ws, 0x7C))
		{
			U16(p, 0xE) = (uint16_t)(U16(p, 0xE) + U16(p, 0x10));
			U16(p, 0xC) = (uint16_t)(U16(p, 0xC) + U16(p, 0xE));
			U16(p, 0x14) = (uint16_t)(U16(p, 0x14) + U16(p, 0x16));
			if (age >= S32(ws, 0x90)) U16(p, 0x20) = (uint16_t)(U16(p, 0x20) + U16(p, 0x22));
		}
		U16(p, 0x12) = (uint16_t)(U16(p, 0x12) + U16(p, 0x14));
		U8(p, 0x18) = (uint8_t)(U8(p, 0x18) + U8(p, 0x1B));
		U8(p, 0x19) = (uint8_t)(U8(p, 0x19) + U8(p, 0x1C));
		U8(p, 0x1A) = (uint8_t)(U8(p, 0x1A) + U8(p, 0x1D));
	}

	// 0xAABFF0 PieceRelease(blk, h): pieces still on the object whose radius is below h come loose
	// (state dword = 1: the age restarts); when the bone's +0x4A bit 1 is set, loose pieces whose angle
	// (& 0xFFF) lies within 0x400 / 0xC00 +- the reference bone's outAngle z are thrown: state | 2,
	// thrown age 0, radial acceleration 0, radial speed = (Rand73(ref outAngle x) + ref outPos x -
	// radius) / ws+0x80. VANILLA QUIRK 0xAAC0E3..0xAAC0FD: the throw reuses the registers holding the
	// first window's bounds, so after a throw the first window is [the random target, target - radius]
	// for the rest of the loop (the second window is kept in memory).
	static void PieceRelease_AABFF0(uint8_t *blk, int32_t h)
	{
		uint8_t *ws = WS();                                                   // ebx
		uint8_t *p0 = blk + 0x14;                                             // esi
		uint8_t *ref = PTR(ws, 0x88);                                         // edi, [ebp-0xc]
		for (int32_t n = S16(blk, 0xA), k = 0; k < n; k++)
		{
			uint8_t *p = p0 + 0x24 * k;
			if (U16(p, 0) == 0 && S16(p, 0xC) < h) U32(p, 0) = 1;
		}
		if (!(U8(CUR(), 0x4A) & 2)) return;
		const int32_t m = S32(ws, 0x80);
		S32(ws, 0xF0) = shl32(mul32(m + 1, m), 4) / 2;
		const int32_t a = S16(ref, 0x90);
		int32_t lo1 = 0x400 - a;                                              // ecx
		int32_t hi1 = a + 0x400;                                              // edi
		const int32_t lo2 = 0xC00 - a;                                        // [ebp+0xc]
		const int32_t hi2 = a + 0xC00;                                        // [ebp-8]
		uint8_t *p = p0;
		for (int32_t n = S16(blk, 0xA); n > 0; n--, p += 0x24)
		{
			const uint16_t sv = U16(p, 0);
			if (sv & 2) continue;
			const int32_t ang = U16(p, 0x12) & 0xFFF;
			if (!((ang >= lo1 && ang <= hi1) || (ang >= lo2 && ang <= hi2))) continue;
			U16(p, 0) = (uint16_t)(sv | 2);
			U16(p, 0xA) = 0;
			const int32_t r = S16(p, 0xC);
			const int32_t t = add32(blob::Rand73(S16(ref, 0x8C)), S16(ref, 0x94));
			lo1 = t;
			hi1 = sub32(t, r);
			U16(p, 0x10) = 0;
			U16(p, 0xE) = (uint16_t)(hi1 / S32(ws, 0x80));
		}
	}

	// 0xAAC130 DrawPieces(blk, verts): the parent transform (0xAA8E50 == Ifrit 0xB269E0) copied to
	// ws+0x40 (ws+0x78 -> it); the object's vertices (verts, 8 bytes each) projected into the arena
	// scratch; per piece (one per triangle record, 0x14 bytes, UV / CLUT / tpage at +4..+0x12, vertex
	// byte offsets +0xA / +0xC / +0xE): on the object (state 0) the triangle at its projected vertices,
	// lit (0x56CA30) code 0x26; else the colour 0x186D510[age % 24], the piece stepped (0xAAC5A0) and
	// drawn about its centroid: rotation of the spin bytes << 4, scaled by +0x20, translated by
	// (sin, 0, cos)(angle) x radius >> 12 after the parent transform, RTPT; depth SZ3 / 4. FT3 (0x20
	// bytes), OT RT+0x4C, no culling.
	static void DrawPieces_AAC130(uint8_t *blk, uint8_t *verts)
	{
		uint8_t *ws = WS();                                                   // esi
		uint8_t *obj = PTR(blk, 0xC);                                         // ebx, [ebp-0x28]
		uint8_t *p = blk + 0x14;                                              // edi
		h_B269E0();
		blob::CopyMatrix(WS() + 0x40, WS() + 0xE0);
		PTR(ws, 0x78) = WS() + 0x40;
		uint8_t *o = PTR(CTX(), 0x74);                                        // [ebp-4]
		const uint8_t *v = verts;                                             // ebx - 2
		for (int32_t n = S32(obj, 0x18); n > 0; n--, v += 8, o += 8)
		{
			x::GteWriteData((int32_t)(((uint32_t)U16(v, 2) << 16) | U16(v, 0)), 0);
			x::GteWriteData((int32_t)U16(v, 4), 1);
			x::GteRTPS();
			x::GteReadSXY2(o);
			U16(o, 4) = MEM<uint16_t>(0x1CA8A5C);
		}
		h_B29450((int32_t)U32(CUR(), 0xCC));
		const uint8_t *tri = obj + U32(obj, 8) + 4;                           // [ebp-0x20]
		uint8_t *pk = PTR(CTX(), 0x7C);                                       // [ebp-8]
		uint8_t *vb = PTR(CTX(), 0x74);                                       // [ebp-0x30]
		for (int32_t n = S16(blk, 0xA); n > 0; n--, pk += 0x20, tri += 0x14, p += 0x24)
		{
			U8(pk, 3) = 7;
			U16(pk, 0x16) = (uint16_t)(U16(tri, 0x12) | 0x20);
			U16(pk, 0xE) = U16(tri, 0x10);
			U16(pk, 0xC) = U16(tri, 4);
			U16(pk, 0x14) = U16(tri, 6);
			U16(pk, 0x1C) = U16(tri, 8);
			if (U16(p, 0) == 0)
			{
				const uint8_t *a = vb + U16(tri, 0xA);                         // ebx
				const uint8_t *b = vb + U16(tri, 0xC);                         // [ebp-4]
				const uint8_t *c = vb + U16(tri, 0xE);                         // [ebp-0x24]
				U32(pk, 8) = U32(a, 0);
				U32(pk, 0x10) = U32(b, 0);
				U32(pk, 0x18) = U32(c, 0);
				xl_NormalColor_56CA30(WS() + 0xE0, tri, pk + 4);
				U8(pk, 7) = 0x26;
				const int32_t z = (int32_t)U16(c, 4) + (int32_t)U16(b, 4) + (int32_t)U16(a, 4);
				xl_InsertPrimAutoDepth(U32(RT(), 0x4C) + (uint32_t)((div3(z) >> 2) & ~3), pk);
				continue;
			}
			// 30 fps layer: the colour of the real tick before the full step (mag221_apocalypse_held.inc)
			FX_HELD(if (held_active()) U32(pk, 4) = PieceColour_186D510(PieceHeldAge(p) % 0x18); else)
			U32(pk, 4) = PieceColour_186D510(S16(p, 2) % 0x18);
			uint8_t *q = p;
			// 30 fps layer: the piece at the in-between of its step (mag221_apocalypse_held.inc)
			FX_HELD(uint8_t held_q[0x24]; if (held_active()) q = PieceHeld(p, held_q); else)
			PieceStep_AAC5A0(q);
			uint8_t *m = ws + 0xC0;                                           // ebx
			U16(ws, 0xB0) = (uint16_t)((uint32_t)U8(q, 0x18) << 4);
			U16(ws, 0xB2) = (uint16_t)((uint32_t)U8(q, 0x19) << 4);
			U16(ws, 0xB4) = (uint16_t)((uint32_t)U8(q, 0x1A) << 4);
			x::RotMatrixFromAngles(ws + 0xB0, m);
			const uint32_t sc = U16(q, 0x20);
			U32(ws, 0xB8) = sc;
			U32(ws, 0xB4) = sc;
			U32(ws, 0xB0) = sc;
			x::ScaleMatrix(m, ws + 0xB0);
			S32(ws, 0xD4) = mul32(x::Sin(S16(q, 0x12)), S16(q, 0xC)) >> 12;
			S32(ws, 0xD8) = 0;
			S32(ws, 0xDC) = mul32(x::Cos(S16(q, 0x12)), S16(q, 0xC)) >> 12;
			x::ComposeAffine(PTR(ws, 0x78), m, m);
			x::SetRotMatrix(m);
			x::SetTransVector(m);
			const uint8_t *a = verts + U16(tri, 0xA);                         // eax
			const uint8_t *b = verts + U16(tri, 0xC);                         // [ebp-4]
			const uint8_t *c = verts + U16(tri, 0xE);                         // [ebp-0x24]
			const uint16_t cx = U16(q, 4), cy = U16(q, 6), cz = U16(q, 8);
			U16(m, 0) = (uint16_t)(U16(a, 0) - cx);
			U16(ws, 0xC2) = (uint16_t)(U16(a, 2) - cy);
			U16(ws, 0xC4) = (uint16_t)(U16(a, 4) - cz);
			U16(ws, 0xC8) = (uint16_t)(U16(b, 0) - cx);
			U16(ws, 0xCA) = (uint16_t)(U16(b, 2) - cy);
			U16(ws, 0xCC) = (uint16_t)(U16(b, 4) - cz);
			U16(ws, 0xD0) = (uint16_t)(U16(c, 0) - cx);
			U16(ws, 0xD2) = (uint16_t)(U16(c, 2) - cy);
			U16(ws, 0xD4) = (uint16_t)(U16(c, 4) - cz);
			x::GteLoadV012(m, ws + 0xC8, ws + 0xD0);
			x::GteRTPT();
			x::GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			const uint16_t sz = MEM<uint16_t>(0x1CA8A5C);
			U16(m, 0) = sz;
			xl_InsertPrimAutoDepth(U32(RT(), 0x4C) + (((uint32_t)sz >> 2) & ~3u), pk);
		}
		PTR(CTX(), 0x7C) = pk;
	}

	// 0xAABB70 (draw 50 ObjectShatter). First draw: the block (0x24 per triangle + 0x14) and the
	// transformed vertices (billboard matrix 0 scaled by outAngle x 16, translation outPos), every piece
	// on the object: centroid, angle (blob 0xB66B30 around the block's (+0, +4) point) and radius
	// (ws+0xFC), radial speed / acceleration reaching ref outPos x + Rand73(ref outAngle x) in desc+8
	// steps, angular acceleration desc+0xE + Rand73(desc+0x10), spin speeds Rand73(desc+0xC), scale
	// 0x1000 growing by (ref outPos y - 0x1000) / (desc+8 - ref outAngle y). Every draw: ws+0x88 = the
	// reference bone, ws+0x7C / 0x80 = desc+8 / desc+0xA, ws+0x90 = block+0x12; 0xAABFF0 (h = ref
	// outPos z) and 0xAAC130.
	static void __cdecl dh_50_ObjectShatter()
	{
		uint8_t *w = WS();                                                    // edi
		// 30 fps layer: the first draw (rand, allocations) on a real tick only
		FX_HELD(if (held_active() && U32(CUR(), 0xBC) == 0) return;)
		if (U32(CUR(), 0xBC) == 0)
		{
			const uint8_t *desc = PTR(CUR(), 0xB8);                           // ebx
			uint8_t *ref = blob::GetBone(S16(desc, 0));                       // [ebp-0x10]
			uint8_t *obj = blob::ObjectPtr(S16(desc, 2));                     // [ebp-0x14]
			const int32_t n = S16(obj + U32(obj, 8), 2);                      // [ebp-8]
			PTR(CUR(), 0xBC) = blob::ArenaAlloc(n * 0x24 + 0x14);
			uint8_t *blk = PTR(CUR(), 0xBC);                                  // esi
			U16(blk, 0xA) = (uint16_t)n;
			PTR(blk, 0xC) = obj;
			U16(blk, 6) = U16(desc, 0);
			U16(blk, 0) = U16(desc, 6);
			U16(blk, 2) = 0;
			U16(blk, 4) = U16(desc, 4);
			U16(blk, 0x10) = U16(ref, 0x96);
			U16(blk, 0x12) = U16(ref, 0x8E);
			blob::CopyMatrix(w + 0xC0, BBMAT());
			S32(w, 0xE0) = shl32(S16(CUR(), 0x8C), 4);
			S32(w, 0xE4) = shl32(S16(CUR(), 0x8E), 4);
			S32(w, 0xE8) = shl32(S16(CUR(), 0x90), 4);
			x::ScaleMatrix(w + 0xC0, w + 0xE0);
			x::SetRotMatrix(w + 0xC0);
			const int32_t tx = S16(CUR(), 0x94), ty = S16(CUR(), 0x96), tz = S16(CUR(), 0x98);
			x::GteWriteCtrl(tx, 5);
			x::GteWriteCtrl(ty, 6);
			x::GteWriteCtrl(tz, 7);
			const uint8_t *src = obj + U32(obj, 0x14);                        // [ebp-0xc]
			PTR(CUR(), 0xC0) = blob::ArenaAlloc(n * 8);
			uint8_t *dst = PTR(CUR(), 0xC0);                                  // [ebp-4]
			for (int32_t k = n; k > 0; k--, src += 8, dst += 8) xl_RotTrans(src, dst, w + 0xFC);
			const int32_t steps = S16(desc, 8);
			S32(w, 0xF0) = shl32(mul32(steps + 1, steps), 4) / 2;
			PTR(w, 0xF4) = PTR(CUR(), 0xC0);
			S32(w, 0xF8) = ((int32_t)S16(ref, 0x96) - 0x1000) / ((int32_t)S16(desc, 8) - (int32_t)S16(ref, 0x8E));
			S32(w, 0x70) = S16(ref, 0x94);
			S32(w, 0x74) = S16(ref, 0x8C);
			const uint8_t *rec = obj + U32(obj, 8) + 4 + 0xC;                 // [ebp-0xc]: triangle record + 0xC
			const int32_t ox = S16(blk, 0), oz = S16(blk, 4);                 // [ebp-0x20] / [ebp-0x1c]
			uint8_t *p = blk + 0x14;                                          // esi
			for (int32_t k = n; k > 0; k--, p += 0x24, rec += 0x14)
			{
				U32(p, 0) = 0;
				U32(p, 0x18) = 0;
				U32(p, 0x1C) = 0;
				const uint8_t *a = PTR(w, 0xF4) + U16(rec, -2);                // ecx
				const uint8_t *b = PTR(w, 0xF4) + U16(rec, 0);                 // [ebp-4]
				const uint8_t *c = PTR(w, 0xF4) + U16(rec, 2);                 // [ebp-0x18]
				const int16_t cx = (int16_t)div3(S16(a, 0) + S16(c, 0) + S16(b, 0));
				U16(p, 4) = (uint16_t)cx;
				const int16_t cy = (int16_t)div3(S16(c, 2) + S16(b, 2) + S16(a, 2));
				U16(p, 6) = (uint16_t)cy;
				const int16_t cz = (int16_t)div3(S16(c, 4) + S16(b, 4) + S16(a, 4));
				U16(p, 0x14) = 0;
				U16(p, 8) = (uint16_t)cz;
				U16(p, 0x16) = (uint16_t)(blob::Rand73(S16(desc, 0x10)) + U16(desc, 0xE));
				U16(p, 0x12) = (uint16_t)blob::Blob_B66B30(ox, oz, cx, cz);
				const int16_t r = S16(w, 0xFC);
				U16(p, 0xC) = (uint16_t)r;
				const int32_t t = add32(blob::Rand73(S32(w, 0x74)), S32(w, 0x70) - r);   // [ebp-0x14]
				const int16_t v = (int16_t)((t / S16(desc, 8)) * 2);
				U16(p, 0xE) = (uint16_t)v;
				U16(p, 0x10) = (uint16_t)(shl32(sub32(t, mul32(S16(desc, 8), v)), 4) / S32(w, 0xF0));
				const int32_t spin = S16(desc, 0xC);
				U8(p, 0x1B) = (uint8_t)blob::Rand73(spin);
				U8(p, 0x1C) = (uint8_t)blob::Rand73(spin);
				U8(p, 0x1D) = (uint8_t)blob::Rand73(spin);
				U16(p, 0x20) = 0x1000;
				U16(p, 0x22) = U16(w, 0xF8);
			}
		}
		uint8_t *blk = PTR(CUR(), 0xBC);                                      // esi
		const uint8_t *desc = PTR(CUR(), 0xB8);                               // ebx
		uint8_t *ref = blob::GetBone(S16(blk, 6));
		PTR(w, 0x88) = ref;
		S32(w, 0x7C) = S16(desc, 8);
		S32(w, 0x80) = S16(desc, 0xA);
		S32(w, 0x90) = S16(blk, 0x12);
		// 30 fps layer: nothing comes loose / is thrown on a held frame (rand; mag221_apocalypse_held.inc)
		FX_HELD(if (!held_active()))
		PieceRelease_AABFF0(blk, S16(ref, 0x98));
		DrawPieces_AAC130(blk, PTR(CUR(), 0xC0));
	}

	// ------------------------------------------------------------------------------------
	// prim 58 0xAAAE60 TintedFT4Abr = Mega Flare 257's 0x9725F0 (mag257_mega_flare.cpp, same code)
	// ------------------------------------------------------------------------------------
	// fild qword [u16 DEPTHARR[off >> 3]]; fmul dword [0x1877DA8]; call __ftol (low dword kept;
	// u16 x float = at most 40 significant bits, exact in a double)
	static inline int32_t DepthKey(uint32_t off)
	{
		uint32_t v = DEPTHARR()[(int32_t)off / 8] & 0xFFFF;
		return (int32_t)(int64_t)((double)v * (double)FLT_1877DA8());
	}

	static void __cdecl pr_58_TintedFT4Abr()
	{
		h_B29450((int32_t)U32(WS(), 0x84));                                 // 0xAAB140 (= the engine's 0xB29450)
		uint8_t *ws = WS();                                                 // [ebp-0x10]
		uint8_t *pkt = PTR(ws, 0x60);                                       // esi
		uint8_t *rec = PTR(ws, 0x6C);                                       // edi
		int32_t count = S32(ws, 0x70);                                      // [ebp-0x1c]
		uint32_t ot = U32(ws, 0x5C);                                        // [ebp-0x24]
		U32(ws, 0xF0) = 0;
		uint32_t nocull = U32(ws, 0x90) & 0x10;                             // [ebp-0x20]
		do
		{
			uint32_t o0 = U16(rec, 0xC);                                    // ebx
			uint32_t o1 = U16(rec, 0xE);                                    // [ebp-0x14]
			uint32_t o2 = U16(rec, 0x10);                                   // [ebp-4]
			uint32_t o3 = U16(rec, 0x12);                                   // [ebp-8]
			int32_t z0 = DepthKey(o0);
			int32_t z1 = DepthKey(o1);
			int32_t z2 = DepthKey(o2);
			int32_t z3 = DepthKey(o3);
			uint8_t *vb = PTR(ws, 0x7C);
			uint8_t *p0 = vb + o0;
			uint8_t *p1 = vb + o1;
			uint8_t *p2 = vb + o2;
			uint8_t *p3 = vb + o3;
			uint32_t s0 = U32(p0, 0);
			uint32_t s1 = U32(p1, 0);
			uint32_t s2 = U32(p2, 0);
			uint32_t s3 = U32(p3, 0);
			U32(pkt, 8) = s0;
			U32(pkt, 0x18) = s2;
			U32(pkt, 0x20) = s3;
			U32(pkt, 0x10) = s1;
			if (nocull == 0)
			{
				GteData(12) = s0;
				GteData(13) = s1;
				GteData(14) = s2;
				x::GteNCLIP();
				if ((int32_t)GteData(24) < 0) goto next;
			}
			{
				uint32_t a0 = U32(p0, 4), a2 = U32(p2, 4), a1 = U32(p1, 4), a3 = U32(p3, 4);
				if ((a3 | a2 | a1 | a0) & 0x460000) goto next;
				U8(pkt, 3) = 0xA;
				U32(pkt, 0x28) = 0xE1000220;
				uint32_t slot = (((a3 + a2 + a1 + a0) >> 2) & 0x3FFC) + ot;
				U16(pkt, 0x16) = (uint16_t)(U32(ws, 0x50) | U16(rec, 0x16));
				U16(pkt, 0xE) = (uint16_t)(U16(rec, 0x14) + (uint16_t)U32(ws, 0x54));
				uint32_t uvofs = U16(ws, 0x98);
				uint32_t uv0 = U16(rec, 4) + uvofs;
				uint32_t uv1 = U16(rec, 6) + uvofs;
				uint32_t uv2 = U16(rec, 8) + uvofs;
				uint32_t uv3 = U16(rec, 0xA) + uvofs;
				U16(pkt, 0xC) = (uint16_t)uv0;
				U16(pkt, 0x24) = (uint16_t)uv3;
				U16(pkt, 0x14) = (uint16_t)uv1;
				U16(pkt, 0x1C) = (uint16_t)uv2;
				GteData(6) = U32(ws, 0x8C) | U32(rec, 0) | 0x2C000000;
				x::Gte_4601B0();
				x::InsertPrimDepthKeys(slot, pkt, z0, z1, z2, z3);
				U32(pkt, 4) = GteData(22);
				pkt += 0x2C;
			}
		next:
			rec += 0x18;
		} while (--count > 0);
		PTR(ws, 0x60) = pkt;
	}

	// ------------------------------------------------------------------------------------
	// handler set: the shared handlers this module carries (VM 0x05C / prim 16 of apply_shared_misc,
	// draw 37 of apply_shared_ribbon, draw 61 of apply_eden_b: only those slots taken), then its own
	// ------------------------------------------------------------------------------------
	static VoidFn g_save_vm[0x200], g_save_big[256], g_save_draw[256];

	static void apply_slots(Clone &c)
	{
		memcpy(g_save_vm, c.vm, sizeof(g_save_vm));
		memcpy(g_save_big, c.bigtab, sizeof(g_save_big));
		memcpy(g_save_draw, c.draw, sizeof(g_save_draw));
		apply_shared_misc(c);
		apply_shared_ribbon(c);
		apply_eden_b(c);
		const VoidFn v5c = c.vm[0x05C];       // 0xAB2460 = Brothers 0xAFEBF0 / Leviathan 0xB64090
		const VoidFn p16 = c.prim[16];        // 0xAAA2C0 = Cerberus 0xB105B0 / Eden 0xAEC030 TintedF4
		const VoidFn d37 = c.draw[37];        // 0xAA7890 RibbonTrail
		const VoidFn d61 = c.draw[61];        // 0xAA6910 = Eden 0xAE71D0 MeshRadialLightWave
		memcpy(c.vm, g_save_vm, sizeof(g_save_vm));
		memcpy(c.bigtab, g_save_big, sizeof(g_save_big));
		memcpy(c.draw, g_save_draw, sizeof(g_save_draw));
		c.vm[0x05C] = v5c;
		c.prim[16] = p16;
		c.draw[37] = d37;
		c.draw[61] = d61;
		c.bone[11] = bh_11_PolarXY;                     // 0xAA6270
		c.prim[58] = pr_58_TintedFT4Abr;                // 0xAAAE60
		c.draw[50] = dh_50_ObjectShatter;               // 0xAABB70
		c.draw[51] = dh_51_FoggedGridSheet;             // 0xAAB290
		c.draw[68] = dh_68_FoggedGridSheetRows;         // 0xAAB6D0
		c.draw[69] = dh_69_EntityTint;                  // 0xAA6770
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		// 30 fps layer: the entity words of the real tick back where a held frame drew (mag221_apocalypse_held.inc)
		FX_HELD(TintRestore();)
		return SequenceTick(g_c);
	}
}
}

	void register_mag221_apocalypse()
	{
		using namespace gfc;
		apocalypse221::describe(apocalypse221::g_c);
		init_clone(apocalypse221::g_c, nullptr, 0);
		apocalypse221::apply_slots(apocalypse221::g_c);
		register_port(0xAA5BD0, (void *)apocalypse221::SequenceTask, "MAG221 Apocalypse SequenceTick", 221);
		// 30 fps layer: see mag221_apocalypse_held.inc
		FX_HELD(register_mag221_apocalypse_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag221_apocalypse_held.inc"
#endif
