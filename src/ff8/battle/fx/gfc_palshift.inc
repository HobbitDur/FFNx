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

// Draw 47 PaletteHsvShift + VM 0x035 VramReadback of the GF cinematic engine clones that carry
// them (Adel's death 211 0xACE7B0 / 0xAD5E80, effect 269 (Gargantua) 0x8FB8B0 / 0x902F80: the
// same code byte for byte, module-relative addresses normalised; draw 47 reads only engine
// globals and fixed battle addresses). Included INSIDE the clone's namespace by the module file;
// the module's _held.h declares PalShiftNote / PalShiftRestore and its _held.inc includes
// gfc_palshift_held.inc (inside the namespace too).
//
// VM 0x035 is Meteor 223's 0xA996A0 (mag223_meteor.cpp op_035_VramReadback, copied with its blob
// helper 0xB66F90): the bone's palette block bone+0xC4 = a VRAM rectangle read back into the arena.
// Draw 47 turns that palette into HSV once (blob 0xB67710, into an arena block at bone+0xBC), then
// on every draw rebuilds the palette in place from the HSV copy shifted by the bone's outPos
// (hue +0x94, saturation +0x96, value +0x98) blended by outAngleX (+0x8C, 0..256) (blob 0xB67810),
// and gives the last colour of the palette's first row to the battle entities of slots 3 and 4:
// their colour word +0x28 (model_palette rgb, +0x2B = 0) and the two words 0xB8BA00 / 0xB8BA04
// ({r, g, b, slot}).

	// ------------------------------------------------------------------------------------
	// shared GF helper blob (one copy for every clone, called at its original addresses as the
	// blob functions of gfc_engine.h are; they write only their two buffers and read the workspace)
	// ------------------------------------------------------------------------------------
	// 0xB67710 (blob): n 15-bit colours at src -> n 8-byte HSV records at dst {s16 hue 0..0x2FF,
	// s16 saturation 0..256, s16 value 0..31 (max channel), u16 colour & 0x8000}
	static inline void xl_PaletteToHsv_B67710(const void *src, void *dst, int32_t n) { x::f<void (__cdecl *)(const void *, void *, int32_t)>(0xB67710)(src, dst, n); }
	// 0xB67810 (blob): n HSV records at hsv -> n 15-bit colours at dst: hue += ws+0xF0 (shortest
	// way, blended by ws+0xF4 clamped to 0..256), saturation + ws+0xF8 (0..256), value + ws+0xFC
	// (0..32), bit 15 kept
	static inline void xl_HsvToPalette_B67810(const void *hsv, void *dst, int32_t n) { x::f<void (__cdecl *)(const void *, void *, int32_t)>(0xB67810)(hsv, dst, n); }

	// 0x505E70 Battle_QueueVramReadback_Type2(rect, dst): queues a VRAM -> memory copy
	static inline void xl_QueueVramReadback(const void *rect, uint32_t dst) { FX_HELD(if (held_predicting()) return;) x::f<void (__cdecl *)(const void *, uint32_t)>(0x505E70)(rect, dst); }

	static const uint32_t PS_RECTRING = 0x27978E8;      // g_GfCinematic_BlitRectRing[16] (engine block)
	static const uint32_t PS_RECTRING_IDX = 0x2798BF4;  // its u8 index (engine block)

	// 0xB66F90 (shared blob, ported as in mag223_meteor.cpp): copies the 8-byte RECT src into the ring
	// slot PS_RECTRING + 8 * (u8 [PS_RECTRING_IDX] & 0xF), Battle_QueueVramReadback_Type2(slot, dst),
	// ++u8 [PS_RECTRING_IDX]
	static void StoreRect_B66F90(const uint8_t *src, uint32_t dst)
	{
		uint8_t *slot = (uint8_t *)(PS_RECTRING + 8u * (MEM<uint8_t>(PS_RECTRING_IDX) & 0xFu));
		U16(slot, 0) = U16(src, 0);
		U16(slot, 2) = U16(src, 2);
		U16(slot, 4) = U16(src, 4);
		U16(slot, 6) = U16(src, 6);
		xl_QueueVramReadback(slot, dst);
		MEM<uint8_t>(PS_RECTRING_IDX) = (uint8_t)(MEM<uint8_t>(PS_RECTRING_IDX) + 1);
	}

	// ------------------------------------------------------------------------------------
	// VM 0x035 VramReadback (Meteor 223's 0xA996A0; 211: 0xAD5E80, 269: 0x902F80): opcode bit
	// 0x8000 (RT+0x4B & 0x80): operands {s16 rect offset from the cursor, s16 slot}: RECT at
	// cursor + off read back into the magic texture buffer + 16 * slot; cursor += 6. Else operands
	// {RECT (8 bytes)}: bone+0xC4 = arena block of w * h * 2 bytes (RECT +4 * +6), the RECT at
	// cursor + 2 read back into it; cursor += 10.
	// Held: VM ops only run on real ticks and in the prediction (readback skipped there).
	// ------------------------------------------------------------------------------------
	static void __cdecl op_035_VramReadback()
	{
		uint8_t *s = STREAM();                                           // ecx
		if (U8(RT(), 0x4B) & 0x80)
		{
			int32_t slot = S16(s, 4);
			uint8_t *src = s + S16(s, 2);
			uint32_t dst = MEM<uint32_t>(0x1D99A88) + ((uint32_t)slot << 4); // MAGIC_TEXTURE_BUFFER_PTR
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

	// ------------------------------------------------------------------------------------
	// Draw 47 PaletteHsvShift (211: 0xACE7B0, 269: 0x8FB8B0). bone+0xB8 = the palette's RECT
	// (w +4, h +6), bone+0xC4 = the palette (VM 0x035), bone+0xBC = its HSV copy (made on the first
	// draw). Every draw: ws+0xF0 = hue shift (outPos x), ws+0xF8 = saturation add (outPos y),
	// ws+0xFC = value add (outPos z), ws+0xF4 = blend (outAngleX); the palette rebuilt in place from
	// the HSV copy; its colour w - 1 (5-bit channels << 3) -> {r, g, b, 3 + i} at 0xB8BA00 + 4 i and
	// the colour word +0x28 of battle entity slot 3 + i ({r, g, b, 0}), i = 0, 1.
	// The frozen path (rt+0x45) is the same: the draw does not test it.
	// Held (gfc_palshift_held.inc): the held draw runs this code on the in-between bone (outPos /
	// outAngle interpolated by the engine's held frame), so the palette (arena, put back by the held
	// frame) and the four colour words get the in-between colour; the real tick's colour words are
	// put back when the next real tick starts (PalShiftRestore).
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_47_PaletteHsvShift()
	{
		uint8_t *cur = CUR();                                            // ecx
		if (U32(cur, 0xBC) == 0)
		{
			const uint8_t *r = PTR(cur, 0xB8);
			const int32_t n = mul32(S16(r, 6), S16(r, 4));               // esi
			PTR(CUR(), 0xBC) = blob::ArenaAlloc(shl32(n, 3));
			xl_PaletteToHsv_B67710(PTR(CUR(), 0xC4), PTR(CUR(), 0xBC), n);
			cur = CUR();
		}
		const uint8_t *rect = PTR(cur, 0xB8);                            // esi
		const int32_t n = mul32(S16(rect, 6), S16(rect, 4));
		S32(WS(), 0xF0) = S16(cur, 0x94);
		S32(WS(), 0xF8) = S16(CUR(), 0x96);
		S32(WS(), 0xFC) = S16(CUR(), 0x98);
		S32(WS(), 0xF4) = S16(CUR(), 0x8C);
		xl_HsvToPalette_B67810(PTR(CUR(), 0xBC), PTR(CUR(), 0xC4), n);
		const uint32_t c = AT<uint16_t>(PTR(CUR(), 0xC4), 2 * (S16(rect, 4) - 1));
		const uint8_t red = (uint8_t)(c << 3), green = (uint8_t)((c >> 5) << 3), blue = (uint8_t)((c >> 10) << 3);
		for (uint32_t i = 0; i < 2; i++)
		{
			uint8_t *w = (uint8_t *)(0xB8BA00 + 4 * i);
			U8(w, 0) = red;
			U8(w, 1) = green;
			U8(w, 2) = blue;
			U8(w, 3) = (uint8_t)(3 + i);
			uint8_t *ent = (uint8_t *)(0x1D974BC + 0x9C * i);           // battle entity slot 3 + i, +0x28
			U32(ent, 0) = U32(w, 0);
			U8(ent, 3) = 0;
		}
		FX_HELD(PalShiftNote();)
	}
