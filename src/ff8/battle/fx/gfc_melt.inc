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

// Draw 52 EntityMelt + draw 3 AnimatedModelWithShadow of the GF cinematic engine clones that carry
// them: effect 265 (module MAG_265 0x92CBA0: draw 52 0x92E430, draw 3 0x92E170) and Final Sorceress
// Death 208 (MAG_208 0xAD78D0: draw 52 0xAD9160, draw 3 0xAD8EA0). The two trees are the same code
// byte for byte (module-relative addresses normalised; same offsets from the module base: draw 52 at
// +0x1890, draw 3 at +0x15D0). Included INSIDE the clone's namespace by the module file, after its
// describe(); the module's _held.h declares the held functions below (MeltNoteFlags,
// dh_52_EntityMeltHeld, MeltPoseHeld, held_note_mid03, held_shadow03) and its _held.inc includes
// gfc_melt_held.inc (inside the namespace too). The function names / comments carry effect 265's
// addresses (MAG_265 0x92CBA0 + offset; 208: + 0x1AAD30).
//
// Draw 52 (no other ported module has it): the battle entity of the bone's slot is hidden (flag +0
// |= 4) and a copy of its model (Elvoret's death 18's entity model copy helpers, 0x92E780 / 0x92E7E0
// / 0x92E6E0 = 0xB4C480 / 0xB4C4E0 / 0xB4C090) is drawn in the entity's pose, with every vertex below
// the height bone+0x94 (outPos x of the draw bone, model space, y down) pushed out radially (angle of
// its x / z around the entity) by an amplitude that grows with depth (a table rebuilt on every draw
// from the bone desc+2: amplitude +0x8C << 12 growing by +0x8E << 4 growing by +0x90, phase +0x94
// stepping by +0x96, sine-shaped), vertices below the floor (y > 0) flattened onto it: the model
// melts into a puddle. Polygons with a vertex below bone+0x98 start a fade through the colour table
// desc + s16 desc+0 (per-polygon life bytes in the arena, 0xFF = not started, life desc+4) and
// disappear when it runs out; then the entity's shadow (blob 0xB67040, grey level outAngleX).
//
// Draw 3 is Brothers 205's 0xAF5810 (mag_brothers.cpp part_brothers dh_03_AnimatedModelWithShadow,
// copied with its held seams): the generic draw 3 plus the embedded model's shadow.

	// ------------------------------------------------------------------------------------
	// engine calls not wrapped by gfc_engine.h
	// ------------------------------------------------------------------------------------
	// 0xB67040 (blob) model shadow: root != 0 -> BuildBoneMatricesFromPose(hdr) +
	// ComputeBonesWorldMatrices(hdr, root); ellipse (8 flat semi-transparent triangles, code
	// 0x32, grey level `level`) of the skeleton's XZ extent at height y, projected; OT = -1 ->
	// FRAMERL+0x4064 else RT+0x4C + (ot & 0x3FFC); packets at ctx+0x7C; root != 0 ->
	// BuildBoneMatricesFromPose(hdr) again. Writes the model skeleton, ws, GTE, packets/OT only.
	static inline void xl_ModelShadow_B67040(void *hdr, void *root, int32_t ot, int32_t y, int32_t level)
	{
		x::f<int32_t (__cdecl *)(void *, void *, int32_t, int32_t, int32_t)>(0xB67040)(hdr, root, ot, y, level);
	}
	// 0x56CB30 (NormalClip): SXY0..2 = a, b, c; GTE NCLIP; returns MAC0
	static inline int32_t xl_NormalClip(int32_t a, int32_t b, int32_t c) { return x::f<int32_t (__cdecl *)(int32_t, int32_t, int32_t)>(0x56CB30)(a, b, c); }
	// 0x45C7A0 SSIGPU_InsertPrimAutoDepth(ot, prim): links a packet (and bumps the depth-key
	// cursor 0x1CA8828).
	static inline void xl_InsertPrimAutoDepth(uint32_t ot, void *prim) { FX_HELD(if (held_predicting()) return;) x::f<void (__cdecl *)(uint32_t, void *)>(0x45C7A0)(ot, prim); }
	// x / 3 as compiled (imul 0x55555556, + sign bit of the high half)
	static inline int32_t melt_div3(int32_t v)
	{
		int32_t hi = (int32_t)(((int64_t)v * 0x55555556LL) >> 32);
		return hi + (int32_t)((uint32_t)hi >> 31);
	}
	// the battle entity of the current bone's slot (SceneHeader+0x60 + 4 * bone+0x1B)
	static inline uint8_t *MeltSlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }

	// ------------------------------------------------------------------------------------
	// 0x92E7E0 SyncEntityModelCopy(blk) (208: 0xAD9510; = Elvoret's death 18 0xB4C4E0 / Brothers
	// 0xAF6310): blk+0x10..0x1B = entity+0x60..0x6B (BattleAnimHeader: +0x14 = the entity's model
	// pointer P), blk+0x30..0x3F = the 16 bytes at P (entity = blk+0xC).
	// ------------------------------------------------------------------------------------
	static void SyncEntityModelCopy_92E7E0(uint8_t *blk)
	{
		uint8_t *src = PTR(blk, 0xC) + 0x60;
		uint8_t *d = blk + 0x10;
		uint8_t *d2 = blk + 0x30;
		U32(d, 0) = U32(src, 0);
		U32(d, 4) = U32(src, 4);
		U32(d, 8) = U32(src, 8);
		uint8_t *m = PTR(src, 4);
		U32(d2, 0) = U32(m, 0);
		U32(d2, 4) = U32(m, 4);
		U32(d2, 8) = U32(m, 8);
		U32(d2, 0xC) = U32(m, 0xC);
	}

	// ------------------------------------------------------------------------------------
	// 0x92E780 CreateEntityModelCopy(ent) (208: 0xAD94B0; = Elvoret's death 18 0xB4C480 / Brothers
	// 0xAF62B0): 0x80-byte block from the arena: +0xC = ent, sync, render descriptor at +0x40 (+0x50
	// = -1, +0x54 = 0, +0x58 = 0xD00140, +0x5C = +0x78 = ent+0x28, +0x60 = ent+0x7C, +0x64 = 0,
	// +0x68 = 0), +0 / +2 = 0, +0x21 = 0.
	// ------------------------------------------------------------------------------------
	static uint8_t *CreateEntityModelCopy_92E780(uint8_t *ent)
	{
		uint8_t *blk = blob::ArenaAlloc(0x80);
		PTR(blk, 0xC) = ent;
		SyncEntityModelCopy_92E7E0(blk);
		U32(blk, 0x54) = 0;
		U32(blk, 0x58) = 0xD00140;
		uint32_t t = U32(ent, 0x28);
		U32(blk, 0x78) = t;
		U32(blk, 0x5C) = t;
		t = U32(ent, 0x7C);
		U16(blk, 0x64) = 0;
		U32(blk, 0x60) = t;
		U32(blk, 0x68) = 0;
		U32(blk, 0x50) = 0xFFFFFFFF;
		U16(blk, 0) = 0;
		U16(blk, 2) = 0;
		U8(blk, 0x21) = 0;
		return blk;
	}

	// ------------------------------------------------------------------------------------
	// 0x92E6E0 CountModelPolygons(mdl = copy+0x30) (208: 0xAD9410; = Elvoret's death 18 0xB4C090):
	// ws+0xFC / ws+0xF8 = the G3 / G4 counts of every object (s16 pair after the vertex groups,
	// 4-aligned). Returns the vertex count.
	// ------------------------------------------------------------------------------------
	static int32_t CountModelPolygons_92E6E0(uint8_t *mdl)
	{
		S32(WS(), 0xFC) = 0;
		S32(WS(), 0xF8) = 0;
		int32_t nvert = 0;
		uint8_t *tab = PTR(mdl, 4);                                           // esi
		int32_t n = S32(tab, 0);
		tab += 4;
		for (; n > 0; n--)
		{
			uint8_t *o = PTR(mdl, 4) + U32(tab, 0);                           // ecx
			tab += 4;
			int32_t ng = S16(o, 0);
			o += 2;
			for (; ng > 0; ng--)
			{
				int32_t nv = S16(o, 2);
				o += 2;
				nvert += nv;
				o += nv * 6 + 2;
			}
			o = (uint8_t *)(((uint32_t)o + 3) & ~3u);
			S32(WS(), 0xFC) = add32(S32(WS(), 0xFC), S16(o, 0));
			S32(WS(), 0xF8) = add32(S32(WS(), 0xF8), S16(o, 2));
		}
		return nvert;
	}

	// ------------------------------------------------------------------------------------
	// 0x92EB10 MeltPolygons(s, ntri, nquad) (208: 0xAD9840): the object's G3 (0x10-byte records)
	// then G4 (0x14-byte records, the four vertex indices first) of the model copy, vertices from
	// the projected buffer ws+0x64 (8 bytes: SXY, depth, +6 = "below the fade height"). One life
	// byte per polygon (ws+0x8C, running over every object): 0 = gone (not drawn); bit 7 (0xFF, not
	// started) = plain grey 0x808080 (FT3 0x24 / FT4 0x2C) until one of its vertices is below the
	// fade height, then colour table[desc+4] and life = desc+4 - 1 (low byte); 1..0x7F = colour
	// table[life] (ws+0x6C, semi-transparent 0x26 / 0x2E, tpage | 0x20), life - 1. Drawn when NCLIP
	// >= 0, depth = (average of the vertex depths) / 4, OT RT+0x4C, packets at ctx+0x7C.
	// ------------------------------------------------------------------------------------
	static void MeltPolygons_92EB10(uint8_t *s, int32_t ntri, int32_t nquad)
	{
		S32(WS(), 0xF0) = S16(PTR(WS(), 0x80), 4);
		uint8_t *vb = PTR(WS(), 0x64);                                        // [ebp-8]
		uint8_t *fl = PTR(WS(), 0x8C);                                        // [ebp-4]
		uint8_t *pk = PTR(CTX(), 0x7C);                                       // esi
		for (; ntri > 0; ntri--, s += 0x10, fl++)
		{
			const int32_t f = S8(fl, 0);
			if (f == 0) continue;
			const uint8_t *a = vb + 8 * (U16(s, 0) & 0xFFF);                  // ebx
			const uint8_t *b = vb + 8 * (U16(s, 2) & 0xFFF);                  // [ebp+8]
			const uint8_t *c = vb + 8 * (U16(s, 4) & 0xFFF);                  // [ebp+0xc]
			int32_t idx = f;
			bool fade = true;
			if (f & 0x80)
			{
				if ((U8(c, 6) | U8(b, 6) | U8(a, 6)) == 0) fade = false;
				else idx = S32(WS(), 0xF0);
			}
			if (fade)
			{
				U32(pk, 4) = U32(PTR(WS(), 0x6C), 4 * idx) | 0x26000000;
				U16(pk, 0x16) = U16(s, 0xE) | 0x20;
				U8(fl, 0) = (uint8_t)(idx - 1);
			}
			else
			{
				U32(pk, 4) = 0x24808080;
				U16(pk, 0x16) = U16(s, 0xE);
			}
			U32(pk, 8) = U32(a, 0);
			U32(pk, 0x10) = U32(b, 0);
			U32(pk, 0x18) = U32(c, 0);
			if (xl_NormalClip(S32(a, 0), S32(b, 0), S32(c, 0)) < 0) continue;
			U8(pk, 3) = 7;
			U16(pk, 0xE) = U16(s, 0xA);
			U16(pk, 0xC) = U16(s, 8);
			U16(pk, 0x14) = U16(s, 0xC);
			U16(pk, 0x1C) = U16(s, 6);
			const int32_t z = (int32_t)U16(c, 4) + (int32_t)U16(b, 4) + (int32_t)U16(a, 4);
			xl_InsertPrimAutoDepth(U32(RT(), 0x4C) + (uint32_t)((melt_div3(z) >> 2) & ~3), pk);
			pk += 0x20;
		}
		if (nquad > 0)
		{
			s += 4;                                                           // edi = the record + 4
			for (; nquad > 0; nquad--, s += 0x14, fl++)
			{
				const int32_t f = S8(fl, 0);
				if (f == 0) continue;
				const uint8_t *v0 = vb + 8 * (U16(s, -4) & 0xFFF);            // [ebp-0xc]
				const uint8_t *v1 = vb + 8 * (U16(s, -2) & 0xFFF);            // [ebp+8]
				const uint8_t *v2 = vb + 8 * (U16(s, 0) & 0xFFF);             // [ebp+0xc]
				const uint8_t *v3 = vb + 8 * (U16(s, 2) & 0xFFF);             // ebx
				int32_t idx = f;
				bool fade = true;
				if (f & 0x80)
				{
					if ((U8(v3, 6) | U8(v2, 6) | U8(v1, 6) | U8(v0, 6)) == 0) fade = false;
					else idx = S32(WS(), 0xF0);
				}
				if (fade)
				{
					U32(pk, 4) = U32(PTR(WS(), 0x6C), 4 * idx) | 0x2E000000;
					U16(pk, 0x16) = U16(s, 0xA) | 0x20;
					U8(fl, 0) = (uint8_t)(idx - 1);
				}
				else
				{
					U32(pk, 4) = 0x2C808080;
					U16(pk, 0x16) = U16(s, 0xA);
				}
				U32(pk, 8) = U32(v0, 0);
				U32(pk, 0x10) = U32(v1, 0);
				U32(pk, 0x18) = U32(v2, 0);
				if (xl_NormalClip(S32(v0, 0), S32(v1, 0), S32(v2, 0)) < 0) continue;
				U32(pk, 0x20) = U32(v3, 0);
				const int32_t z = (int32_t)U16(v3, 4) + (int32_t)U16(v2, 4) + (int32_t)U16(v1, 4) + (int32_t)U16(v0, 4);
				U8(pk, 3) = 9;
				U16(pk, 0xE) = U16(s, 6);
				U16(pk, 0xC) = U16(s, 4);
				U16(pk, 0x14) = U16(s, 8);
				U16(pk, 0x1C) = U16(s, 0xC);
				U16(pk, 0x24) = U16(s, 0xE);
				xl_InsertPrimAutoDepth(U32(RT(), 0x4C) + (uint32_t)(((z / 4) >> 2) & ~3), pk);
				pk += 0x28;
			}
		}
		PTR(CTX(), 0x7C) = pk;
	}

	// ------------------------------------------------------------------------------------
	// 0x92E830 MeltVertices(tbl, out) (208: 0xAD9560; returns 0, unused; tbl unused: the table is
	// read through ws+0x60): the model copy's bone matrices from the entity's pose, world matrices
	// from the entity matrix (entity+0x40) with translation (0, ws+0x74, 0); copy+0x44 = out. Per
	// object: every vertex group through its bone matrix (MVMVA, IR -> out, 8 bytes per vertex,
	// restarting at out for every object), then with the camera rotation (node 0) and translation
	// = the entity position (entity+0x1C) in camera space: +6 = y > ws+0x78; y > 0 -> y = 0; when
	// y > ws+0x70 (y = 0 included) x / z += (sin / cos of the blob 0xB66B30 angle of (x, z)) x
	// table[y - ws+0x70] (>> 4 each, >> 16); RTPS -> SXY, SZ3 (+4). Then the object's polygons
	// (0x92EB10).
	// ------------------------------------------------------------------------------------
	static int32_t MeltVertices_92E830(uint8_t *, uint8_t *out)
	{
		uint8_t *blk = PTR(WS(), 0x84);                                       // ebx, [ebp-0x10]
		uint8_t *hdr = blk + 0x10;                                            // edi
		// 30 fps layer: the entity's in-between pose on held frames (gfc_melt_held.inc)
		FX_HELD(if (held_active()) MeltPoseHeld(hdr); else)
		x::BuildBoneMatricesFromPose(hdr);
		uint8_t *ent = MeltSlotEntity();                                      // [ebp-0x18]
		uint8_t *m = WS() + 0xE0;                                             // esi
		blob::CopyMatrix(m, ent + 0x40);
		U32(m, 0x1C) = 0;
		U32(m, 0x14) = 0;
		U32(m, 0x18) = U32(WS(), 0x74);
		x::ComputeBonesWorldMatrices(hdr, WS() + 0xE0);
		PTR(blk, 0x44) = out;
		uint8_t *bones = PTR(blk, 0x30) + 0x10;                               // [ebp-0x14]
		uint8_t *tab = PTR(blk, 0x34);                                        // edi
		int32_t nobj = S32(tab, 0);                                           // [ebp-0xc]
		tab += 4;
		for (; nobj > 0; nobj--)
		{
			uint8_t *s = PTR(blk, 0x34) + U32(tab, 0);                        // ebx
			tab += 4;
			int32_t ng = S16(s, 0);                                           // [ebp-8]
			s += 2;
			uint8_t *o = out;                                                 // esi
			for (; ng > 0; ng--)
			{
				uint8_t *mm = bones + S16(s, 0) * 48 + 0x10;
				x::SetRotMatrix(mm);
				x::SetTransVector(mm);
				const int32_t nv = S16(s, 2);
				s += 4;
				S32(WS(), 0xF0) = nv;
				for (int32_t k = nv; k > 0; k--)
				{
					x::GteWriteData((int32_t)(((uint32_t)U16(s, 2) << 16) | U16(s, 0)), 0);
					x::GteWriteData((int32_t)U16(s, 4), 1);
					x::GteMVMVA_RotV0Tr();
					U16(o, 0) = MEM<uint16_t>(0x1CA8A74);
					U16(o, 2) = (uint16_t)MEM<uint32_t>(0x1CA8A78);
					U16(o, 4) = (uint16_t)MEM<uint32_t>(0x1CA8A7C);
					s += 6;
					o += 8;
				}
				o += mul32(-S32(WS(), 0xF0), 8);
				x::SetRotMatrix(NODEMAT());
				x::SetTransVector(NODEMAT());
				x::TransformToCamera(ent + 0x1C, WS() + 0xF4, WS() + 0xE0);
				x::SetTransVector(WS() + 0xE0);
				const int32_t lo = S32(WS(), 0x70);                           // [ebp-0x1c]
				uint8_t *tb = PTR(WS(), 0x60);                                // [ebp-0x20]
				for (int32_t k = S32(WS(), 0xF0); k > 0; k--)
				{
					int32_t y = S16(o, 2);
					U8(o, 6) = y > S32(WS(), 0x78) ? 1 : 0;
					bool wave;
					if (y > 0)
					{
						U16(o, 2) = 0;
						y = 0;
						wave = true;
					}
					else wave = y > lo;
					if (wave)
					{
						const int32_t amp = S32(tb, 4 * (y - lo)) >> 4;       // edi
						const int32_t a = blob::Blob_B66B30(0, 0, S16(o, 0), S16(o, 4));
						U16(o, 0) = (uint16_t)(U16(o, 0) + (uint16_t)(mul32(x::Sin(a) >> 4, amp) >> 16));
						U16(o, 4) = (uint16_t)(U16(o, 4) + (uint16_t)(mul32(x::Cos(a) >> 4, amp) >> 16));
					}
					x::GteWriteData((int32_t)(((uint32_t)U16(o, 2) << 16) | U16(o, 0)), 0);
					x::GteWriteData((int32_t)U16(o, 4), 1);
					x::GteRTPS();
					x::GteReadSXY2(o);
					U16(o, 4) = MEM<uint16_t>(0x1CA8A5C);                    // SZ3
					o += 8;
				}
			}
			s = (uint8_t *)(((uint32_t)s + 3) & ~3u);
			const int32_t ntri = S16(s, 0);
			const int32_t nquad = S16(s, 2);
			s += 0xC;
			MeltPolygons_92EB10(s, ntri, nquad);
		}
		return 0;
	}

	// ------------------------------------------------------------------------------------
	// 0x92E430 (Draw 52 EntityMelt) (208: 0xAD9160). bone+0xB8 = descriptor {s16 colour table
	// offset, s16 wave bone, s16 fade life}. First draw: bone+0xBC = model copy of the slot entity
	// (0x92E780), bone+0xC0 = 0x10-byte block {u16 G3 + G4 count, u16 vertex count, colour table
	// pointer}, bone+0xC4 = the life bytes ((count + 4) & 0xFFC, all 0xFF), entity flag +0 |= 4.
	// Every draw (MeltFrame below): ws+0x80..0x8C = descriptor / copy / block / life bytes, the copy
	// synced, the amplitude table at the arena top (ws+0x60, 1 - outPos x entries: amplitude (s32)
	// and its two increments, sin of the phase stepping), ws+0x64 = the vertex buffer after it,
	// ws+0x6C = colour table, ws+0x70 / 0x74 / 0x78 = outPos x / y / z; 0x92E830; then the entity's
	// shadow (blob 0xB67040 root = entity matrix, OT -1, y 0, level outAngleX).
	// ------------------------------------------------------------------------------------
	// 0x92E513..0x92E6A4: the draw after the first-draw block, up to and with the call to 0x92E830;
	// returns the model copy ([ebp-0x18], the shadow's model)
	static uint8_t *MeltFrame_92E430()
	{
		PTR(WS(), 0x80) = PTR(CUR(), 0xB8);
		PTR(WS(), 0x84) = PTR(CUR(), 0xBC);
		uint8_t *blk = PTR(WS(), 0x84);                                       // [ebp-0x18]
		PTR(WS(), 0x88) = PTR(CUR(), 0xC0);
		uint8_t *st = PTR(WS(), 0x88);                                        // [ebp-0x14]
		PTR(WS(), 0x8C) = PTR(CUR(), 0xC4);
		uint8_t *wb = blob::GetBone((int32_t)S16(PTR(WS(), 0x80), 2));        // esi
		SyncEntityModelCopy_92E7E0(blk);
		PTR(WS(), 0x60) = PTR(CTX(), 0x74);
		uint8_t *tbl = PTR(WS(), 0x60);                                       // [ebp-8]
		S32(WS(), 0x78) = S16(CUR(), 0x98);
		S32(WS(), 0x74) = S16(CUR(), 0x96);
		S32(WS(), 0x70) = S16(CUR(), 0x94);
		int32_t amp = shl32(S16(wb, 0x8C), 12);                               // ebx
		int32_t vel = shl32(S16(wb, 0x8E), 4);                                // edi
		int32_t ang = S16(wb, 0x94);                                          // [ebp-4]
		const int32_t dang = S16(wb, 0x96);                                   // [ebp-0x10]
		const int32_t acc = S16(wb, 0x90);                                    // esi
		for (int32_t n = 1 - S16(CUR(), 0x94); n > 0; n--)
		{
			S32(tbl, 0) = mul32(x::Sin(ang) >> 4, amp) >> 8;
			tbl += 4;
			vel = add32(vel, acc);
			ang = add32(ang, dang);
			amp = add32(amp, vel);
		}
		PTR(WS(), 0x6C) = PTR(st, 4);
		PTR(WS(), 0x64) = tbl;
		MeltVertices_92E830(PTR(WS(), 0x60), PTR(WS(), 0x64));
		return blk;
	}

	static void __cdecl dh_52_EntityMelt()
	{
		// 30 fps layer: see gfc_melt_held.inc
		FX_HELD(if (held_active()) { dh_52_EntityMeltHeld(); return; })
		if (U32(CUR(), 0xBC) == 0)
		{
			uint8_t *desc = PTR(CUR(), 0xB8);                                 // [ebp-8]
			uint8_t *ent = MeltSlotEntity();                                  // edi
			PTR(CUR(), 0xBC) = CreateEntityModelCopy_92E780(ent);
			uint8_t *blk = PTR(CUR(), 0xBC);                                  // esi
			PTR(CUR(), 0xC0) = blob::ArenaAlloc(0x10);
			uint8_t *st = PTR(CUR(), 0xC0);                                   // ebx
			U16(st, 2) = (uint16_t)CountModelPolygons_92E6E0(blk + 0x30);
			const uint16_t np = (uint16_t)(U16(WS(), 0xFC) + U16(WS(), 0xF8));
			U16(st, 0) = np;
			const int32_t n = (int32_t)(((uint32_t)np + 4) & 0xFFC);
			PTR(CUR(), 0xC4) = blob::ArenaAlloc(n);
			if (n > 0) memset(PTR(CUR(), 0xC4), 0xFF, ((uint32_t)(n + 3) >> 2) * 4);
			PTR(st, 4) = desc + S16(desc, 0);
			U8(ent, 0) |= 4;
		}
		// 30 fps layer: the life bytes before this draw (gfc_melt_held.inc)
		FX_HELD(MeltNoteFlags();)
		uint8_t *blk = MeltFrame_92E430();
		xl_ModelShadow_B67040(blk + 0x10, MeltSlotEntity() + 0x40, -1, 0, S16(CUR(), 0x8C));
	}

	// ------------------------------------------------------------------------------------
	// 0x92E170 (Draw 3 AnimatedModelWithShadow) (208: 0xAD8EA0) = Brothers 205's 0xAF5810: generic
	// draw 3 (0x92DFD0 == Ifrit 0xB269E0 SetupParentXformScaled, 0x92E0C0 == Ifrit 0xB26AD0
	// StepEmbeddedModelAnim, RenderGeometry mode 4, block+0x44 = arena scratch), then, when the model
	// block flag byte +2 has bit 2: the model's projected shadow through blob 0xB67040 with root
	// matrix ws+0x80 = rotation of the parent node (bone+0x9C) in order 0 scaled by 16*bone scale
	// (+0x8C/8E/90), translation = parent outPos; OT spec / plane height = the two s16 at
	// *(block+0x7C); grey level 0x80.
	// ------------------------------------------------------------------------------------
	static void __cdecl dh_03_AnimatedModelWithShadow()
	{
		h_B269E0();
		// 30 fps layer: see gfc_melt_held.inc
		FX_HELD(held_note_mid03();)
		h_B26AD0(PTR(CUR(), 0xBC));
		uint8_t *blk = PTR(CUR(), 0xBC);                                  // esi
		U32(blk, 0x44) = U32(CTX(), 0x74);
		uint32_t cursor = U32(CTX(), 0x7C);
		uint32_t ot = U32(RT(), 0x4C);
		U32(CTX(), 0x7C) = x::RenderGeometry(blk + 0x30, blk + 0x40, ot, 4, cursor);
		if (!(U8(blk, 2) & 4)) return;
		uint8_t *par = blob::GetBone((int32_t)U16(CUR(), 0x9C));         // edi
		blob::RotMatrixOrder(par + 0x8C, 0);                              // -> ws+0xE0
		blob::CopyMatrix(WS() + 0x80, WS() + 0xE0);
		S32(WS(), 0x60) = shl32(S16(CUR(), 0x8C), 4);
		S32(WS(), 0x64) = shl32(S16(CUR(), 0x8E), 4);
		S32(WS(), 0x68) = shl32(S16(CUR(), 0x90), 4);
		x::ScaleMatrix(WS() + 0x80, WS() + 0x60);
		uint8_t *hdr = blk + 0x10;                                        // esi += 0x10
		S32(WS(), 0x94) = S16(par, 0x94);
		S32(WS(), 0x98) = S16(par, 0x96);
		S32(WS(), 0x9C) = S16(par, 0x98);
		uint8_t *sp = PTR(hdr, 0x6C);                                     // block+0x7C
		int32_t y = S16(sp, 2);
		int32_t otspec = S16(sp, 0);
		// 30 fps layer: see gfc_melt_held.inc
		FX_HELD(if (held_shadow03(hdr, blk, otspec, y)) return;)
		xl_ModelShadow_B67040(hdr, WS() + 0x80, otspec, y, 0x80);
	}
