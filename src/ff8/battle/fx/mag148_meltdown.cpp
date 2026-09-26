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

// Effect 148: Meltdown (spell, MAG_148_*).
//
// Structure (setup MAG_148_MELTDOWN 0x6120D0, file loader 0x6120B0; root queue 0x2465778 with one
// 0x10-byte node, effect queue 0x24657E8 with 0x64 nodes of 0x24 bytes):
//   Root (0x614230) - alternates the packet arena (0x2466638 / 0x246E638), runs the effect queue,
//     ends when it is empty.
//   Director (0x612190), one per action: every tick (also in the draw-only mode) rebuilds the
//     action's frame 0x2465788 + 0x20 * action = camera * (rotation turning -z towards the target,
//     scale 0x600, origin 900/4096 of the way from the caster); at tick 0 takes the target and
//     caster positions (the target not above the caster when the caster is a party member), 1
//     spawns the disc, the two acid particle systems and plays the sound, 0x23 the caster glow and
//     a white screen fade, 0x28 the melting target, 0x50 damage, 0x55 the next action's director;
//     ends after tick 0x5A.
//   Melting target (0x612540): hides the target (entity flags |= 0xC) and draws its battle model
//     itself for 0x22 ticks: every vertex near the target is pushed along the plane normal facing
//     the caster, more and more then back (+0x3C), flattened at the ground (y > 0 -> 0); the model's
//     second part is drawn normally. The target's flags are restored at the end.
//   Disc (0x612D90) - prim model 0xDCDB4C in the action frame, growing, UV scrolling, fading in
//     and out (own prim renderer 0x612EE0 with four primitive lists and UV scroll).
//   Caster glow (0x613A50) - prim models 0xDCADB4 (flat, z scale 0x4000) and 0xDCCDA4, decelerating
//     growth, fade in / out.
//   Acid particles A (0x613C40) - pool 0x2464BB0 (60 x 0x14 bytes), 3 spawned per tick 1..0x1C
//     from the frame origin outwards, drifting back; 17-frame flipbook 0xDCABEC, then gone.
//   Acid particles B (0x613EF0) - pool 0x2465060, 3 per tick 1..0x12, looping flipbook 0xDCAA24;
//     they drift out until tick 0x23 then fly back decelerating (velocity x16 at 0x23).
// Module globals: 0x2464BA8..0x2476660 (cast context 0x2466620, target / caster positions
// 0x2466608 / 0x2466630, frames, pools, queues, packet arenas, cursor 0x2476638, scale matrix
// 0x2476640).

#include "mag_common.h"

namespace ff8fx
{
namespace melt148
{
	using namespace eng;
	using namespace magc;

	// raw memory access (the port follows the listing offset by offset)
	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline uint32_t P(const void *p) { return (uint32_t)p; }

	// --- module globals ---
	inline TaskQueue *EffectQueue() { return (TaskQueue *)0x24657E8; }
	inline CastContext *Ctx() { return var<CastContext *>(0x2466620); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2476638); }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }
	inline uint32_t Frame(int32_t action) { return 0x2465788 + (uint32_t)shl32(action, 5); }   // Mat4x3 per action
	inline uint32_t TargetPos(int32_t action) { return 0x2466608 + (uint32_t)shl32(action, 3); } // x, y, z, h
	static const uint32_t CASTER_POS = 0x2466630;
	static const uint32_t SCALE_MATRIX = 0x2476640;   // diagonal 0x2476640 / 48 / 50, translation 54 / 58 / 5C
	static const uint32_t POOL_A = 0x2464BB0, POOL_A_END = 0x2465060;
	static const uint32_t POOL_B = 0x2465060, POOL_B_END = 0x2465510;
	static const uint32_t ARENA_EVEN = 0x2466638, ARENA_ODD = 0x246E638;

	static const uint32_t ORIG_Root = 0x614230;
	static const uint32_t ORIG_Director = 0x612190;
	static const uint32_t ORIG_Melt = 0x612540;
	static const uint32_t ORIG_Disc = 0x612D90;
	static const uint32_t ORIG_CasterGlow = 0x613A50;
	static const uint32_t ORIG_ParticlesA = 0x613C40;
	static const uint32_t ORIG_ParticlesB = 0x613EF0;
	static const uint32_t MODEL_Disc = 0xDCDB4C, MODEL_GlowFlat = 0xDCADB4, MODEL_Glow = 0xDCCDA4;
	static const uint32_t SEQ_AcidA = 0xDCABEC, SEQ_AcidB = 0xDCAA24;
	static const void *const SOUND_Meltdown = (const void *)0xDCE74C;

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// ---- engine functions (original addresses)
	inline void GteWriteCtrl(uint32_t value, uint32_t reg) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45D7F0)(value, reg); }
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void GteReadSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
	inline void GteLoadV0u(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x45DF80)(v); }
	inline void GteReadSXY2u(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E260)(dst); }
	inline void GteLoadRGBC(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(c); }        // set_unk_1CA8A28
	inline void GteLoadRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E120)(a, b, c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(c); }       // set_param_with_dword_1CA8A68
	inline void GteStoreRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E370)(a, b, c); }
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void GteSetLightMatrix(uint32_t m) { fn<void (__cdecl *)(uint32_t)>(0x45DE50)(m); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }
	inline void GteMVMVA_LightIR() { fn<void (__cdecl *)()>(0x460810)(); }
	inline void GteMVMVA_RotV0Tr2() { fn<void (__cdecl *)()>(0x460830)(); }
	inline void InsertPrim(uint32_t bucket, uint32_t pkt) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x45C7A0)(bucket, pkt); }
	// GTE helpers of the neighbouring modules
	inline void GteLoadV0Words(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x64DEE0)(v); }    // MAG_146_sub_64DEE0: V0 = 3 words
	inline void GteStoreIRVertex(uint32_t out) { fn<void (__cdecl *)(uint32_t)>(0x64DF10)(out); } // MAG_146_sub_64DF10: IR1..3 -> 4 words (4th 0)
	inline void GteReadMAC1x4096(uint32_t out) { fn<void (__cdecl *)(uint32_t)>(0x64DF60)(out); } // MAG_148_sub_64DF60: MAC1 << 12
	inline void GteSetBkFromTranslation(uint32_t m) { fn<void (__cdecl *)(uint32_t)>(0x64DDA0)(m); } // MAG_148_sub_64DDA0: BK = matrix translation
	inline void GteSetRotDiagonal16(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DDD0)(s); } // MAG_148_sub_64DDD0: rotation = s * identity
	inline void GteSetTransFromIR() { fn<void (__cdecl *)()>(0x64DE90)(); }                    // MAG_148_sub_64DE90: TR = IR1..3
	// battle model helpers
	inline uint32_t DrawShadow(uint32_t entity, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	inline uint32_t RenderGeometry(uint32_t model, uint32_t hdr, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(0x5099D0)(model, hdr, ot, mode, cursor); }
	inline void NormalizeSVector(uint32_t in, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x56BDE0)(in, out); } // sub_56BDE0 (x87)
}
}

#ifdef FF8_FX_HELD
#include "mag148_meltdown_held.h"
#endif

namespace ff8fx
{
namespace melt148
{
	// ------------------------------------------------------------------
	// Root task (0x614230): node +0x0C counter
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		uint32_t node = P(n);
		// 30 fps layer: see mag148_meltdown_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = ARENA_ODD;
		if (!(U8(node, 0xC) & 1)) PacketCursor() = ARENA_EVEN;
		int alive = ExecuteTaskQueue(EffectQueue());
		U16(node, 0xC) = (uint16_t)(U16(node, 0xC) + 1);
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Director (0x612190): node +0x0C counter, +0x0E action, +0x20 target slot, +0x22 caster slot
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		uint32_t node = P(n);
		// the action frame: rotation turning (0, 0, -1) towards the target, scale 0x600, origin
		// 900/4096 of the way from the caster to the target, composed with the camera
		{
			const int32_t action = S16(node, 0xE);
			const uint32_t tp = TargetPos(action);
			int32_t dir[3], from[3], scale[3], axis[3];
			from[2] = -0x1000;
			dir[0] = S16(tp, 0) - S16(CASTER_POS, 0);
			from[0] = 0;
			from[1] = 0;
			dir[1] = S16(tp, 2) - S16(CASTER_POS, 2);
			dir[2] = S16(tp, 4) - S16(CASTER_POS, 4);
			NormalizeVector(dir, dir);
			int32_t angle = RotationBetweenVectors(from, dir, axis);
			Mat4x3 *m = (Mat4x3 *)Frame(S16(node, 0xE));
			BuildAxisAngleRotationMatrix(angle, m, axis);
			scale[2] = 0x600;
			scale[1] = 0x600;
			scale[0] = 0x600;
			m->t[0] = (mul32(dir[0], 900) >> 12) + S16(CASTER_POS, 0);
			m->t[1] = (mul32(dir[1], 900) >> 12) + S16(CASTER_POS, 2);
			m->t[2] = (mul32(dir[2], 900) >> 12) + S16(CASTER_POS, 4);
			Scale3DMatrix(m, scale);
			ComposeAffineTransform(&Camera(), (Mat4x3 *)Frame(S16(node, 0xE)), (Mat4x3 *)Frame(S16(node, 0xE)));
		}
		if (DrawOnly()) return 0;
		if (S16(node, 0xC) == 0)
		{
			GetDefaultEffectPosition(Entity(S16(node, 0x20)), (int16_t *)TargetPos(S16(node, 0xE)));
			GetDefaultEffectPosition(Entity(S16(node, 0x22)), (int16_t *)CASTER_POS);
			const uint32_t tp = TargetPos(S16(node, 0xE));
			const int16_t cy = S16(CASTER_POS, 2);
			if (S16(tp, 2) > cy && S16(node, 0x22) < 3) S16(tp, 2) = cy;
		}
		if (S16(node, 0xC) == 0x28)
		{
			uint32_t t = P(AddTaskToQueue(EffectQueue(), ORIG_Melt));
			U16(t, 0xE) = U16(node, 0xE);
			const uint16_t slot = U16(node, 0x20);
			U16(t, 0x18) = slot;
			U16(t, 0xC) = 0;
			U16(t, 0x1A) = (uint16_t)(Entity((int16_t)slot)[0] & 0xC);
		}
		if (S16(node, 0xC) == 1)
		{
			uint32_t t = P(AddTaskToQueue(EffectQueue(), ORIG_Disc));
			const uint16_t a = U16(node, 0xE);
			U16(t, 0xC) = 0;
			U16(t, 0xE) = a;
			U16(t, 0x1C) = 0xC00;
			U16(t, 0x1E) = 0x80;
			t = P(AddTaskToQueue(EffectQueue(), ORIG_ParticlesA));
			U16(t, 0xC) = 0;
			U16(t, 0xE) = U16(node, 0xE);
			for (uint32_t p = POOL_A; p < POOL_A_END; p += 0x14) U16(p, 0) = 0xFFFF;
			t = P(AddTaskToQueue(EffectQueue(), ORIG_ParticlesB));
			U16(t, 0xC) = 0;
			U16(t, 0xE) = U16(node, 0xE);
			for (uint32_t p = POOL_B; p < POOL_B_END; p += 0x14) U16(p, 0) = 0xFFFF;
		}
		if (S16(node, 0xC) == 0x23)
		{
			uint32_t t = P(AddTaskToQueue(EffectQueue(), ORIG_CasterGlow));
			U16(t, 0xC) = 0;
			U16(t, 0xE) = U16(node, 0xE);
			U16(t, 0x1C) = 0x300;
			U16(t, 0x1E) = 0x600;
			if (S16(node, 0xC) == 0x23) ScreenFadeTask(0, 1, 8, 0xFF);
		}
		if (S16(node, 0xC) == 0x50)
			ApplyActionResultToTarget(Ctx()->actions[S16(node, 0xE)].targets);
		if (S16(node, 0xC) == 0x55)
		{
			const int32_t next = S16(node, 0xE) + 1;
			if (next <= (int32_t)Ctx()->actions[0].last_action)
			{
				uint32_t t = P(AddTaskToQueue(EffectQueue(), ORIG_Director));
				U16(t, 0xC) = 0;
				ActionData *ad = &Ctx()->actions[next];
				U16(t, 0xE) = (uint16_t)next;
				U16(t, 0x20) = ad->targets[0];
				U16(t, 0x22) = ad->attacker;
			}
		}
		if (S16(node, 0xC) == 1) BdPlaySE(SOUND_Meltdown, 0, 0x80);
		U16(node, 0xC) = (uint16_t)(U16(node, 0xC) + 1);
		return S16(node, 0xC) > 0x5A ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Melting target model
	// Draw header (0xA4 bytes, Field_Alloc): +0x00 primitive cursor, +0x04 projected / melted
	// vertex buffer (8 bytes each), +0x08.. list counts, +0x10 packet end, +0x14..+0x1A screen box,
	// +0x1C colour, +0x20 object mask, +0x28.. depth cue colour, +0x2C plane distance of the vertex
	// (x 4096), +0x30 plane distance of the target position, +0x34 normal y (0 -> 1), +0x38 push
	// of the vertex, +0x3C melt strength, +0x40 squared radius (0x5F5E10), +0x44 NCLIP, +0x4C OTZ,
	// +0x50 GTE flags, +0x54..+0x58 vertex - target, +0x74..+0x78 plane normal (4.12, towards the
	// caster), +0x7A plane distance, +0x7C..+0x82 target position, +0x84 camera * entity matrix.
	// ------------------------------------------------------------------

	// 0x612B40: projects the melted vertices of one object and draws its flat-coloured textured
	// triangles (list 1, 0x10-byte records) and quads (list 2, 0x14-byte records)
	static uint32_t MeltDrawPrims(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t verts = U32(hdr, 4);
		uint32_t pkt = cursor;
		uint32_t rec = U32(hdr, 0);
		for (uint32_t i = 0; i < U16(hdr, 8); i++, rec += 0x10)
		{
			GteLoadV012(verts + (U16(rec, 0) & 0xFFF) * 8, verts + (U16(rec, 2) & 0xFFF) * 8, verts + (U16(rec, 4) & 0xFFF) * 8);
			GteRTPT();
			const uint32_t w = U32(rec, 0xC);
			U32(pkt, 0x14) = w & 0x1FFFFFF;
			U32(pkt, 0) = 0x7000000;
			U32(pkt, 4) = (w & 0x2000000) | U32(hdr, 0x1C) | 0x24000000;
			U32(pkt, 0xC) = U32(rec, 8);
			GteReadFLAG(hdr + 0x50);
			if (U32(hdr, 0x50) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(hdr + 0x44);
			if (S32(hdr, 0x44) <= 0) continue;
			GteReadSXY012(pkt + 8, pkt + 0x10, pkt + 0x18);
			GteAVSZ3();
			S32(pkt, 0x1C) = S32(rec, 4) >> 16;
			GteReadOTZ32(hdr + 0x4C);
			InsertPrim(ot + (uint32_t)shl32(S32(hdr, 0x4C) >> (shift & 31), 2), pkt);
			pkt += 0x20;
		}
		if (U16(hdr, 0xA) != 0)
		{
			rec += 2;
			for (uint32_t i = 0; i < U16(hdr, 0xA); i++, rec += 0x14)
			{
				GteLoadV012(verts + (U16(rec, -2) & 0xFFF) * 8, verts + (U16(rec, 0) & 0xFFF) * 8, verts + (U16(rec, 2) & 0xFFF) * 8);
				GteRTPT();
				const uint32_t w = U32(rec, 0xA);
				U32(pkt, 0x14) = w & 0x1FFFFFF;
				U32(pkt, 0) = 0x9000000;
				U32(pkt, 4) = (w & 0x2000000) | U32(hdr, 0x1C) | 0x2C000000;
				U32(pkt, 0xC) = U32(rec, 6);
				GteReadFLAG(hdr + 0x50);
				if (U32(hdr, 0x50) & 0x60000) continue;
				GteNCLIP();
				GteReadMAC0(hdr + 0x44);
				if (S32(hdr, 0x44) <= 0) continue;
				GteReadSXY012(pkt + 8, pkt + 0x10, pkt + 0x18);
				GteLoadV0u(verts + (U16(rec, 4) & 0xFFF) * 8);
				GteRTPS();
				const uint32_t w2 = U32(rec, 0xE);
				U32(pkt, 0x1C) = w2;
				S32(pkt, 0x24) = (int32_t)w2 >> 16;
				GteReadSXY2u(pkt + 0x20);
				GteAVSZ4();
				GteReadOTZ32(hdr + 0x4C);
				InsertPrim(ot + (uint32_t)shl32(S32(hdr, 0x4C) >> (shift & 31), 2), pkt);
				pkt += 0x28;
			}
		}
		return pkt;
	}

	// 0x6128D0: every visible object of the model: its vertices through their bone matrices
	// (world space) into the header's vertex buffer, each vertex within the squared radius +0x40 of
	// the target pushed along the plane normal (inverse-distance weighted, at most 15000, then
	// ground-clamped: y > 0 -> 0), then drawn through the camera (MeltDrawPrims)
	static uint32_t MeltModel(uint32_t model, uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		// light matrix row 1 = plane normal (x, y / z), background colour red = plane distance
		GteWriteCtrl(U32(hdr, 0x74), 8);
		const uint32_t w78 = U32(hdr, 0x78);
		GteWriteCtrl(w78, 9);
		GteWriteCtrl((uint32_t)((int32_t)w78 >> 16), 0xD);
		const uint32_t bones = U32(model, 0) + 0x10;
		uint32_t table = U32(model, 4);
		const int32_t count = S32(table, 0);
		table += 4;
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t obj = U32(model, 4) + U32(table, 0);
			table += 4;
			if (!(U32(hdr, 0x20) & (1u << (i & 31)))) continue;
			uint32_t out = U32(hdr, 4);
			int32_t groups = S16(obj, 0);
			obj += 2;
			if (groups > 0)
			{
				for (; groups != 0; groups--)
				{
					const int32_t bone = S16(obj, 0);
					obj += 2;
					const Mat4x3 *m = (const Mat4x3 *)(bones + (uint32_t)(bone * 0x30) + 0x10);
					GteSetRotMatrixCtrl(m);
					GteSetTransVectorCtrl(m);
					int32_t nv = S16(obj, 0);
					obj += 2;
					if (nv <= 0) continue;
					for (; nv != 0; nv--, out += 8)
					{
						GteLoadV0Words(obj);
						obj += 6;
						GteMVMVA_RotV0Tr2();
						GteStoreIRVertex(out);
						GteMVMVA_LightIR();
						const int16_t dy = (int16_t)(S16(out, 2) - S16(hdr, 0x7E));
						const int16_t dx = (int16_t)(S16(out, 0) - S16(hdr, 0x7C));
						const int16_t dz = (int16_t)(S16(out, 4) - S16(hdr, 0x80));
						S16(hdr, 0x56) = dy;
						S16(hdr, 0x54) = dx;
						S16(hdr, 0x58) = dz;
						int32_t d2 = (int32_t)((uint32_t)mul32(dx, dx) + (uint32_t)mul32(dy, dy) + (uint32_t)mul32(dz, dz));
						const int32_t r2 = S32(hdr, 0x40);
						if (d2 >= r2) continue;
						if (d2 < 0x9C400) d2 = 0x9C400;
						const int32_t weight = mul32(shl32(d2, 12) / r2, S32(hdr, 0x3C)) >> 9;
						GteReadMAC1x4096(hdr + 0x2C);
						int32_t push = mul32((int32_t)((uint32_t)S32(hdr, 0x30) - (uint32_t)S32(hdr, 0x2C)) / S32(hdr, 0x34), weight) >> 12;
						S32(hdr, 0x38) = push;
						if (push < 0) S32(hdr, 0x38) = -push;
						if (S32(hdr, 0x38) > 15000) S32(hdr, 0x38) = 15000;
						const int32_t v = S32(hdr, 0x38);
						S16(out, 0) = (int16_t)(S16(out, 0) - (int16_t)(mul32(S16(hdr, 0x74), v) >> 12));
						S16(out, 2) = (int16_t)(S16(out, 2) - (int16_t)(mul32(S16(hdr, 0x76), v) >> 12));
						S16(out, 4) = (int16_t)(S16(out, 4) - (int16_t)(mul32(S16(hdr, 0x78), v) >> 12));
						if (S16(out, 2) > 0) S16(out, 2) = 0;
					}
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
			cursor = MeltDrawPrims(hdr, ot, shift, cursor);
		}
		return cursor;
	}

	// 0x6127C0 (MAG_148_MELTDOWN_RenderMeltedTarget): the entity's shadow, its model melted, its
	// second model (+0x78) drawn normally
	static void RenderMeltedTarget(uint32_t hdr, uint32_t entity)
	{
		const uint32_t anim = entity + 0x60;
		if (!(U8(entity, 0) & 0x20))
			FrameCursor() = DrawShadow(entity, var<uint32_t>(0x1D8E04C) + 0x4064, 0x10, FrameCursor());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)(hdr + 0x84));
		ComputeBonesWorldMatrices((void *)anim, (void *)(entity + 0x40));
		U32(hdr, 0x1C) = U32(entity, 0x28);
		U32(hdr, 0x20) = U32(entity, 0x7C);
		const uint8_t cue = U8(entity, 7);
		U8(hdr, 0x2A) = cue;
		U8(hdr, 0x29) = cue;
		U8(hdr, 0x28) = cue;
		U32(hdr, 4) = var<uint32_t>(0x1D98B3C);
		U32(hdr, 0x10) = var<uint32_t>(0x1D969A8);
		U16(hdr, 0x14) = 0;
		U16(hdr, 0x16) = 0;
		U16(hdr, 0x18) = 0x140;
		U16(hdr, 0x1A) = 0xD8;
		FrameCursor() = MeltModel(U32(anim, 4), hdr, RenderOT44(), 2, FrameCursor());
		BuildBoneMatricesFromPose((void *)anim);
		const uint32_t second = U32(entity, 0x78);
		if (second && !(U8(entity, 1) & 2))
		{
			U32(hdr, 0x20) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices((void *)second, (void *)(hdr + 0x84));
			FrameCursor() = RenderGeometry(U32(second, 4), hdr, RenderOT44(), 4, FrameCursor());
			BuildBoneMatricesFromPose((void *)second);
		}
	}

	// melt strength of tick c: rising 81 / tick until 0x1C, then falling 364 / tick
	static int32_t MeltStrength(int32_t c)
	{
		if (c < 0x1C) return c * 81;
		return 0x305B - c * 364;
	}

	// the melting target's draw at melt strength `melt` (node +0x0E action, +0x18 target slot):
	// the melt plane (through the target, facing the caster, its y 500 above the target but not
	// below 0) and the model draw
	static void MeltDraw(uint32_t node, int32_t melt, bool draw_model)
	{
		const int32_t slot = S16(node, 0x18);
		const int32_t action = S16(node, 0xE);
		uint8_t *entity = Entity(slot);
		const uint32_t tw0 = U32(TargetPos(action), 0);
		const uint32_t tw1 = U32(TargetPos(action), 4);
		entity[0] |= 0xC;
		const uint32_t cw0 = U32(CASTER_POS, 0);
		const uint32_t cw1 = U32(CASTER_POS, 4);
		int16_t by = (int16_t)((uint16_t)(tw0 >> 16) + 0x1F4);
		if (by > 0) by = 0;
		const int32_t tx = (int16_t)tw0;
		const int32_t tz = (int16_t)tw1;
		int32_t flat[3], from[3], axis[3], n[3];
		flat[0] = (int16_t)cw0 - tx;
		from[0] = 0;
		from[1] = 0;
		flat[1] = 0;
		flat[2] = (int16_t)cw1 - tz;
		from[2] = -0x1000;
		NormalizeVector(flat, flat);
		int32_t angle = RotationBetweenVectors(from, flat, axis);
		Mat4x3 rot = {};
		BuildAxisAngleRotationMatrix(angle, &rot, axis); // (its result is not used)
		n[0] = (int16_t)cw0 - tx;
		n[1] = (int16_t)(cw0 >> 16) - by;
		n[2] = (int16_t)cw1 - tz;
		NormalizeVector(n, n);
		const uint32_t hdr = P(AllocHeader(0xA4));
		U32(hdr, 0x40) = 0x5F5E10;
		U32(hdr, 0x7C) = U32(TargetPos(S16(node, 0xE)), 0);
		U32(hdr, 0x80) = U32(TargetPos(S16(node, 0xE)), 4);
		U16(hdr, 0x74) = (uint16_t)n[0];
		U16(hdr, 0x76) = (uint16_t)n[1];
		U16(hdr, 0x78) = (uint16_t)n[2];
		int32_t d = (int32_t)((uint32_t)mul32(by, n[1]) + (uint32_t)mul32(tz, n[2]));
		d = (int32_t)((uint32_t)d + (uint32_t)mul32(tx, n[0]));
		U16(hdr, 0x7A) = (uint16_t)(-(d >> 12));
		const int32_t ny = S16(hdr, 0x76);
		int32_t p = (int32_t)((uint32_t)mul32(S16(hdr, 0x78), tz) + (uint32_t)mul32(S16(hdr, 0x74), tx));
		p = (int32_t)((uint32_t)p + (uint32_t)mul32(ny, by));
		const int32_t e = shl32(-ny, 12) >> 12;
		S32(hdr, 0x30) = p;
		S32(hdr, 0x34) = e;
		if (e == 0) S32(hdr, 0x34) = 1;
		S32(hdr, 0x3C) = melt;
		if (draw_model) RenderMeltedTarget(hdr, P(entity));
		FieldFree(0xA4);
	}

	// ------------------------------------------------------------------
	// Melting target (0x612540): node +0x0C counter, +0x0E action, +0x18 target slot, +0x1A the
	// target's flags 0xC bits before the effect
	// ------------------------------------------------------------------
	static uint32_t __cdecl MeltTask(TaskNode *n)
	{
		uint32_t node = P(n);
		const int16_t c = S16(node, 0xC);
		// 30 fps layer: see mag148_meltdown_held.inc
		FX_HELD(if (c < 0x22) held_note_draw(ORIG_Melt, node);)
		MeltDraw(node, MeltStrength(c), c < 0x22);
		if (DrawOnly()) return 0;
		U16(node, 0xC) = (uint16_t)(U16(node, 0xC) + 1);
		if (S16(node, 0xC) <= 0x22) return 0;
		uint16_t *flags = (uint16_t *)Entity(S16(node, 0x18));
		*flags = (uint16_t)((*flags & 0xFFF3) | U16(node, 0x1A));
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Prim-model renderer of the module (0x612EE0 and its four list renderers)
	// Header (0x7C bytes): +0x00 model, +0x04 its vertices (model + 8), +0x08..+0x0A far colour,
	// +0x0C depth cue (GTE IR0), +0x10 / +0x14 U / V scroll, +0x18 / +0x1A / +0x1C / +0x1E
	// colour words (+0x1C / +0x1E also the U / V wrap of the scroll), +0x20 flags (0x10 / 0x20
	// double-sided, 0x40 / 0x80 depth cue), +0x24 list cursor, +0x28 NCLIP, +0x30 OTZ, +0x34 GTE
	// flags, +0x5C..+0x68 scrolled coordinates, +0x6C / +0x70 U / V scroll (mod wrap), +0x74
	// draw mode word, +0x78 texture window word (0xE2000000).
	// ------------------------------------------------------------------

	// scroll the texture coordinate bytes at pkt + offs[k] by `delta`, wrapped by `wrap` when a
	// coordinate leaves 0..0xFF (all of them move together); the sums go through hdr +0x5C..
	static void ScrollUV(uint32_t hdr, uint32_t pkt, const int *offs, int count, int32_t delta, int32_t wrap)
	{
		int32_t v[4];
		bool over = false, under = false;
		for (int k = 0; k < count; k++)
		{
			v[k] = U8(pkt, offs[k]) + delta;
			S32(hdr, 0x5C + 4 * k) = v[k];
			if (v[k] >= 0x100) over = true;
		}
		if (!over)
			for (int k = 0; k < count; k++) under |= v[k] < 0;
		if (over || under)
			for (int k = 0; k < count; k++) S32(hdr, 0x5C + 4 * k) = over ? v[k] - wrap : v[k] + wrap;
		for (int k = 0; k < count; k++) U8(pkt, offs[k]) = U8(hdr, 0x5C + 4 * k);
	}

	// 0x613020: list 1 - textured triangles (0x14-byte records: +0 colour + code, +4 / +6 / +8
	// vertex offsets / 4, +0x0C uv0 + clut, +0x10 uv1 + tpage, +0x0A uv2) as 0x28-byte packets
	static uint32_t PrimList1(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t pkt = cursor;
		uint32_t rec = U32(hdr, 0x24);
		const uint32_t verts = U32(hdr, 4);
		const int32_t wrap_u = S16(hdr, 0x1C), wrap_v = S16(hdr, 0x1E);
		int32_t count = S32(rec, 0);
		rec += 4;
		U32(hdr, 0x24) = rec;
		if (count > 0)
		{
			for (; count != 0; count--, rec += 0x14)
			{
				GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
				GteRTPT();
				U32(pkt, 8) = U32(rec, 0);
				U32(pkt, 0) = 0x9000000;
				U32(pkt, 0x20) = U32(rec, 8) >> 16;
				U32(pkt, 0x10) = U32(rec, 0xC);
				U32(pkt, 0x18) = U32(rec, 0x10);
				GteReadFLAG(hdr + 0x34);
				if (U32(hdr, 0x34) & 0x60000) continue;
				GteNCLIP();
				if (S32(hdr, 0x6C) != 0)
				{
					static const int u[3] = { 0x10, 0x18, 0x20 };
					ScrollUV(hdr, pkt, u, 3, S32(hdr, 0x6C), wrap_u);
				}
				GteReadMAC0(hdr + 0x28);
				if (S32(hdr, 0x28) < 0 && !(U8(hdr, 0x20) & 0x10)) continue;
				GteReadSXY012(pkt + 0xC, pkt + 0x14, pkt + 0x1C);
				GteAVSZ3();
				if (S32(hdr, 0x70) != 0)
				{
					static const int v[3] = { 0x11, 0x19, 0x21 };
					ScrollUV(hdr, pkt, v, 3, S32(hdr, 0x70), wrap_v);
				}
				GteReadOTZ32(hdr + 0x30);
				if (U8(hdr, 0x20) & 0x40)
				{
					GteLoadRGBC(pkt + 8);
					GteSetIR0(S32(hdr, 0xC));
					GteDPCS();
					GteStoreRGB2(pkt + 8);
				}
				U32(pkt, 4) = U32(hdr, 0x74);
				U32(pkt, 0x24) = U32(hdr, 0x78);
				InsertPrim(ot + (uint32_t)shl32(S32(hdr, 0x30) >> (shift & 31), 2), pkt);
				pkt += 0x28;
			}
		}
		U32(hdr, 0x24) = rec;
		return pkt;
	}

	// 0x613280: list 2 - textured quads (0x18-byte records: + vertex 4 at +0x0A, uv2 / uv3 at
	// +0x14) as 0x30-byte packets
	static uint32_t PrimList2(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t pkt = cursor;
		uint32_t rec = U32(hdr, 0x24);
		const uint32_t verts = U32(hdr, 4);
		const int32_t wrap_u = S16(hdr, 0x1C), wrap_v = S16(hdr, 0x1E);
		int32_t count = S32(rec, 0);
		rec += 4;
		U32(hdr, 0x24) = rec;
		if (count > 0)
		{
			for (; count != 0; count--, rec += 0x18)
			{
				GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
				GteRTPT();
				U32(pkt, 8) = U32(rec, 0);
				const uint32_t w14 = U32(rec, 0x14);
				U32(pkt, 0x20) = w14;
				U32(pkt, 0x28) = w14 >> 16;
				U32(pkt, 0) = 0xB000000;
				U32(pkt, 0x10) = U32(rec, 0xC);
				U32(pkt, 0x18) = U32(rec, 0x10);
				GteReadFLAG(hdr + 0x34);
				if (U32(hdr, 0x34) & 0x60000) continue;
				GteNCLIP();
				if (S32(hdr, 0x6C) != 0)
				{
					static const int u[4] = { 0x10, 0x18, 0x20, 0x28 };
					ScrollUV(hdr, pkt, u, 4, S32(hdr, 0x6C), wrap_u);
				}
				GteReadMAC0(hdr + 0x28);
				if (S32(hdr, 0x28) < 0 && !(U8(hdr, 0x20) & 0x10)) continue;
				GteReadSXY012(pkt + 0xC, pkt + 0x14, pkt + 0x1C);
				GteLoadV0u(verts + U16(rec, 0xA) * 4);
				GteRTPS();
				if (S32(hdr, 0x70) != 0)
				{
					static const int v[4] = { 0x11, 0x19, 0x21, 0x29 };
					ScrollUV(hdr, pkt, v, 4, S32(hdr, 0x70), wrap_v);
				}
				GteReadSXY2u(pkt + 0x24);
				GteAVSZ4();
				GteReadOTZ32(hdr + 0x30);
				if (U8(hdr, 0x20) & 0x40)
				{
					GteLoadRGBC(pkt + 8);
					GteSetIR0(S32(hdr, 0xC));
					GteDPCS();
					GteStoreRGB2(pkt + 8);
				}
				U32(pkt, 0x2C) = U32(hdr, 0x78);
				U32(pkt, 4) = U32(hdr, 0x74);
				InsertPrim(ot + (uint32_t)shl32(S32(hdr, 0x30) >> (shift & 31), 2), pkt);
				pkt += 0x30;
			}
		}
		U32(hdr, 0x24) = rec;
		return pkt;
	}

	// 0x613570: list 3 - gouraud quads (0x18-byte records: +0 colour 0 + code, +4..+0x0A vertex
	// offsets / 4, +0x0C / +0x10 / +0x14 colours 1..3) as 0x24-byte packets
	static uint32_t PrimList3(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t rec = U32(hdr, 0x24);
		const uint32_t verts = U32(hdr, 4);
		int32_t count = S32(rec, 0);
		rec += 4;
		uint32_t pkt = cursor;
		U32(hdr, 0x24) = rec;
		if (count <= 0) return cursor;
		for (; count != 0; count--, rec += 0x18)
		{
			GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPT();
			U32(pkt, 4) = U32(rec, 0);
			U32(pkt, 0) = 0x8000000;
			GteReadFLAG(hdr + 0x34);
			if (U32(hdr, 0x34) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(hdr + 0x28);
			if (S32(hdr, 0x28) < 0 && !(U8(hdr, 0x20) & 0x20)) continue;
			GteReadSXY012(pkt + 8, pkt + 0x10, pkt + 0x18);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			GteReadSXY2u(pkt + 0x20);
			GteAVSZ4();
			GteReadOTZ32(hdr + 0x30);
			if (U8(hdr, 0x20) & 0x80)
			{
				GteLoadRGB012(rec + 0xC, rec + 0x10, rec + 0x14);
				GteSetIR0(S32(hdr, 0xC));
				GteDPCT();
				GteStoreRGB012(pkt + 0xC, pkt + 0x14, pkt + 0x1C);
				GteLoadRGBC(pkt + 4);
				GteDPCS();
				GteStoreRGB2(pkt + 4);
			}
			else
			{
				U32(pkt, 0xC) = U32(rec, 0xC);
				U32(pkt, 0x14) = U32(rec, 0x10);
				U32(pkt, 0x1C) = U32(rec, 0x14);
			}
			InsertPrim(ot + (uint32_t)shl32(S32(hdr, 0x30) >> (shift & 31), 2), pkt);
			pkt += 0x24;
		}
		U32(hdr, 0x24) = rec;
		return pkt;
	}

	// 0x613710: list 4 - gouraud textured quads (0x24-byte records: list 2's + colours 1..3 at
	// +0x18 / +0x1C / +0x20) as 0x3C-byte packets
	static uint32_t PrimList4(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t pkt = cursor;
		const int32_t wrap_u = S16(hdr, 0x1C), wrap_v = S16(hdr, 0x1E);
		uint32_t rec = U32(hdr, 0x24);
		int32_t count = S32(rec, 0);
		rec += 4;
		const uint32_t verts = U32(hdr, 4);
		U32(hdr, 0x24) = rec;
		if (count > 0)
		{
			for (; count != 0; count--, rec += 0x24)
			{
				GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
				GteRTPT();
				U32(pkt, 8) = U32(rec, 0);
				const uint32_t w14 = U32(rec, 0x14);
				U32(pkt, 0x28) = w14;
				U32(pkt, 0) = 0xE000000;
				U32(pkt, 0x10) = U32(rec, 0xC);
				U32(pkt, 0x1C) = U32(rec, 0x10);
				U32(pkt, 0x34) = w14 >> 16;
				GteReadFLAG(hdr + 0x34);
				if (U32(hdr, 0x34) & 0x60000) continue;
				GteNCLIP();
				if (S32(hdr, 0x6C) != 0)
				{
					static const int u[4] = { 0x10, 0x1C, 0x28, 0x34 };
					ScrollUV(hdr, pkt, u, 4, S32(hdr, 0x6C), wrap_u);
				}
				GteReadMAC0(hdr + 0x28);
				if (S32(hdr, 0x28) < 0 && !(U8(hdr, 0x20) & 0x20)) continue;
				GteReadSXY012(pkt + 0xC, pkt + 0x18, pkt + 0x24);
				GteLoadV0u(verts + U16(rec, 0xA) * 4);
				GteRTPS();
				if (S32(hdr, 0x70) != 0)
				{
					static const int v[4] = { 0x11, 0x1D, 0x29, 0x35 };
					ScrollUV(hdr, pkt, v, 4, S32(hdr, 0x70), wrap_v);
				}
				GteReadSXY2u(pkt + 0x30);
				GteAVSZ4();
				GteReadOTZ32(hdr + 0x30);
				if (U8(hdr, 0x20) & 0x80)
				{
					GteLoadRGB012(rec + 0x18, rec + 0x1C, rec + 0x20);
					GteSetIR0(S32(hdr, 0xC));
					GteDPCT();
					GteStoreRGB012(pkt + 0x14, pkt + 0x20, pkt + 0x2C);
					GteLoadRGBC(pkt + 8);
					GteDPCS();
					GteStoreRGB2(pkt + 8);
				}
				else
				{
					U32(pkt, 0x14) = U32(rec, 0x18);
					U32(pkt, 0x20) = U32(rec, 0x1C);
					U32(pkt, 0x2C) = U32(rec, 0x20);
				}
				U32(pkt, 4) = U32(hdr, 0x74);
				U32(pkt, 0x38) = U32(hdr, 0x78);
				InsertPrim(ot + (uint32_t)shl32(S32(hdr, 0x30) >> (shift & 31), 2), pkt);
				pkt += 0x3C;
			}
		}
		U32(hdr, 0x24) = rec;
		return pkt;
	}

	// 0x612EE0: the model's four primitive lists (a list with no entry is skipped over)
	static uint32_t PrimDraw(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t model = U32(hdr, 0);
		U32(hdr, 4) = model + 8;
		U32(hdr, 0x24) = model + U32(model, 0);
		// draw mode word: the colour words +0x1A / +0x18 / +0x1E / +0x1C as a texture window
		// (the original tests the header address for null: always taken)
		uint32_t mode = ((uint32_t)(U8(hdr, 0x1A) & 0xF8) | 0xFFFE2000u) << 5;
		mode |= U8(hdr, 0x18) & 0xF8;
		mode <<= 5;
		mode |= (0u - U16(hdr, 0x1E)) & 0xF8;
		mode <<= 2;
		mode |= ((0u - U16(hdr, 0x1C)) >> 3) & 0x1F;
		U32(hdr, 0x74) = mode;
		U32(hdr, 0x78) = 0xE2000000;
		S32(hdr, 0x6C) = S32(hdr, 0x10) % (int32_t)S16(hdr, 0x1C);
		S32(hdr, 0x70) = S32(hdr, 0x14) % (int32_t)S16(hdr, 0x1E);
		GteSetFarColor(U8(hdr, 8), U8(hdr, 9), U8(hdr, 0xA));
		uint32_t list = U32(hdr, 0x24) + 8;
		U32(hdr, 0x24) = list;
		if (U32(list, 0) != 0) cursor = PrimList1(hdr, ot, shift, cursor);
		else U32(hdr, 0x24) = list + 4;
		list = U32(hdr, 0x24);
		if (U32(list, 0) != 0) cursor = PrimList2(hdr, ot, shift, cursor);
		else U32(hdr, 0x24) = list + 4;
		list = U32(hdr, 0x24) + 4;
		U32(hdr, 0x24) = list;
		if (U32(list, 0) != 0) cursor = PrimList3(hdr, ot, shift, cursor);
		else U32(hdr, 0x24) = list + 4;
		U32(hdr, 0x24) = U32(hdr, 0x24) + 4;
		return PrimList4(hdr, ot, shift, cursor);
	}

	// the scale matrix 0x2476640 (x = y = s, z = sz, no translation) composed with the action frame
	// into the GTE
	static void SetScaledFrame(int32_t action, uint16_t s, uint16_t sz, bool set_z_first)
	{
		if (set_z_first) U16(SCALE_MATRIX, 0x10) = sz;
		U32(SCALE_MATRIX, 0x14) = 0;
		U16(SCALE_MATRIX, 0) = s;
		U16(SCALE_MATRIX, 8) = s;
		if (!set_z_first) U16(SCALE_MATRIX, 0x10) = sz;
		U32(SCALE_MATRIX, 0x18) = 0;
		U32(SCALE_MATRIX, 0x1C) = 0;
		Mat4x3 m = {};
		ComposeAffineTransform((const Mat4x3 *)Frame(action), (const Mat4x3 *)SCALE_MATRIX, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	// ------------------------------------------------------------------
	// Disc (0x612D90): node +0x0C counter, +0x0E action, +0x1C size (0xC00, +0x80 / tick)
	// ------------------------------------------------------------------
	static void DiscDraw(uint32_t node)
	{
		const uint16_t s = U16(node, 0x1C);
		SetScaledFrame(S16(node, 0xE), s, s, false);
		const uint32_t hdr = P(AllocHeader(0x7C));
		const int16_t c16 = S16(node, 0xC);
		const int32_t c = c16;
		U32(hdr, 0) = MODEL_Disc;
		U32(hdr, 0x20) = 0x30;
		U32(hdr, 0x10) = 0;
		U16(hdr, 0x18) = 0;
		U32(hdr, 0x14) = (uint32_t)shl32(-(c * 3), 2);
		U16(hdr, 0x1A) = 0x80;
		U16(hdr, 0x1C) = 0x80;
		U16(hdr, 0x1E) = 0x80;
		U8(hdr, 0xA) = 0;
		U8(hdr, 9) = 0;
		U8(hdr, 8) = 0;
		if (c16 < 8)
		{
			U32(hdr, 0xC) = (uint32_t)shl32(8 - c, 9);
			U32(hdr, 0x20) = 0xF0;
		}
		else if (c16 >= 0x41)
		{
			U32(hdr, 0xC) = (uint32_t)shl32(c - 0x41, 9);
			U32(hdr, 0x20) = 0xF0;
		}
		PacketCursor() = PrimDraw(hdr, RenderOT44(), 2, PacketCursor());
		FieldFree(0x7C);
	}

	static uint32_t __cdecl DiscTask(TaskNode *n)
	{
		uint32_t node = P(n);
		// 30 fps layer: see mag148_meltdown_held.inc
		FX_HELD(held_note_draw(ORIG_Disc, node);)
		DiscDraw(node);
		if (DrawOnly()) return 0;
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + U16(node, 0x1E));
		U16(node, 0xC) = (uint16_t)(U16(node, 0xC) + 1);
		return S16(node, 0xC) >= 0x49 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Caster glow (0x613A50): node +0x0C counter, +0x0E action, +0x1C size (0x300), +0x1E its step
	// (0x600, 7/8 of itself each tick)
	// ------------------------------------------------------------------
	static void CasterGlowDraw(uint32_t node)
	{
		U16(SCALE_MATRIX, 0x10) = 0x4000;
		const uint16_t s = U16(node, 0x1C);
		SetScaledFrame(S16(node, 0xE), s, 0x4000, false);
		const uint32_t hdr = P(AllocHeader(0x7C));
		const int16_t c16 = S16(node, 0xC);
		const int32_t c = c16;
		U32(hdr, 0) = MODEL_GlowFlat;
		U32(hdr, 0x20) = 0x30;
		U32(hdr, 0x10) = (uint32_t)shl32(c * 3, 3);
		U32(hdr, 0x14) = 0;
		U16(hdr, 0x18) = 0;
		U16(hdr, 0x1A) = 0;
		U16(hdr, 0x1C) = 0x80;
		U16(hdr, 0x1E) = 0x80;
		U8(hdr, 0xA) = 0;
		U8(hdr, 9) = 0;
		U8(hdr, 8) = 0;
		if (c16 < 6)
		{
			U32(hdr, 0xC) = (uint32_t)(0x1000 - shl32(c * 341, 1));
			U32(hdr, 0x20) = 0xF0;
		}
		else if (c16 >= 0x20)
		{
			U32(hdr, 0xC) = (uint32_t)shl32(c - 0x20, 9);
			U32(hdr, 0x20) = 0xF0;
		}
		PacketCursor() = PrimDraw(hdr, RenderOT44(), 2, PacketCursor());
		const uint16_t sz = U16(node, 0x1C);
		SetScaledFrame(S16(node, 0xE), s, sz, true);
		U16(hdr, 0x18) = 0x40;
		U16(hdr, 0x1C) = 0x40;
		U32(hdr, 0) = MODEL_Glow;
		U32(hdr, 0x10) = 0;
		U32(hdr, 0x14) = (uint32_t)shl32(S16(node, 0xC), 3);
		U16(hdr, 0x1A) = 0;
		U16(hdr, 0x1E) = 0x80;
		PacketCursor() = PrimDraw(hdr, RenderOT44(), 2, PacketCursor());
		FieldFree(0x7C);
	}

	static uint32_t __cdecl CasterGlowTask(TaskNode *n)
	{
		uint32_t node = P(n);
		// 30 fps layer: see mag148_meltdown_held.inc
		FX_HELD(held_note_draw(ORIG_CasterGlow, node);)
		CasterGlowDraw(node);
		if (DrawOnly()) return 0;
		const int16_t c = S16(node, 0xC);
		if (c >= 0)
		{
			const int16_t step = S16(node, 0x1E);
			U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + (uint16_t)step);
			S16(node, 0x1E) = (int16_t)(step - (int16_t)(step >> 3));
		}
		S16(node, 0xC) = (int16_t)(c + 1);
		return (int16_t)(c + 1) >= 0x28 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Acid particles: pools of 60 entries of 0x14 bytes: +0 flipbook frame (< 0: free), +2 size,
	// +4 position (in the action frame), +0x0C velocity. Drawn with the GTE light matrix = the
	// action frame's rotation and BK = its translation (world position), rotation = size, then
	// the shared sprite sequence player.
	// ------------------------------------------------------------------
	static void ParticleDraw(uint32_t h, uint32_t e)
	{
		GteLoadV0u(e + 4);
		GteMVMVA_LightV0Bk();
		U16(h, 4) = U16(e, 0);
		GteSetRotDiagonal16(S16(e, 2));
		GteSetTransFromIR();
		PacketCursor() = InitEffectSequenceFromData((void *)h, RenderOT44(), 2, PacketCursor());
	}

	// sprite header + GTE setup of both particle tasks
	static uint32_t ParticleSetup(uint32_t node, uint32_t seq)
	{
		const uint32_t h = P(AllocHeader(0xB4));
		U32(h, 0) = seq;
		U16(h, 0x24) = 0;
		GteSetRotMatrixCtrl((const Mat4x3 *)SCALE_MATRIX);
		GteSetLightMatrix(Frame(S16(node, 0xE)));
		GteSetBkFromTranslation(Frame(S16(node, 0xE)));
		return h;
	}

	// first free entry of a pool (frame < 0), or 0
	static uint32_t FreeEntry(uint32_t pool, uint32_t end)
	{
		for (uint32_t e = pool; e < end; e += 0x14)
			if (S16(e, 0) < 0) return e;
		return 0;
	}

	// A: moves by its velocity, 17 frames then free
	static void ParticleAUpdate(uint32_t e)
	{
		S16(e, 0) = (int16_t)(S16(e, 0) + 1);
		if (S16(e, 0) >= 0x11)
		{
			U16(e, 0) = 0xFFFF;
			return;
		}
		U16(e, 4) = (uint16_t)(U16(e, 4) + U16(e, 0xC));
		U16(e, 6) = (uint16_t)(U16(e, 6) + U16(e, 0xE));
		U16(e, 8) = (uint16_t)(U16(e, 8) + U16(e, 0x10));
	}

	// 0x613C40 (MAG_148_sub_613C40): pool A, node +0x0C counter, +0x0E action
	static uint32_t __cdecl ParticlesATask(TaskNode *n)
	{
		uint32_t node = P(n);
		const uint32_t h = ParticleSetup(node, SEQ_AcidA);
		for (uint32_t e = POOL_A; e < POOL_A_END; e += 0x14)
		{
			if (S16(e, 0) < 0) continue;
			// 30 fps layer: see mag148_meltdown_held.inc
			FX_HELD(held_note_particle(ORIG_ParticlesA, e, node, h);)
			ParticleDraw(h, e);
			if (DrawOnly()) continue;
			ParticleAUpdate(e);
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(node, 0xC);
		if (c >= 1 && c <= 0x1C)
		{
			for (int k = 0; k < 3; k++)
			{
				const uint32_t e = FreeEntry(POOL_A, POOL_A_END);
				if (!e) break;
				U16(e, 0) = 0;
				S16(e, 2) = (int16_t)(CrtRand() % 1024 + 0x400);
				int16_t v[4];
				v[0] = (int16_t)(CrtRand() % 4096 - 0x800);
				v[1] = (int16_t)(CrtRand() % 4096 - 0x800);
				v[2] = (int16_t)(CrtRand() % 2048);
				NormalizeSVector(P(v), P(v));
				const int32_t speed = CrtRand() % 0xAF0 + 0x1068;
				const int32_t x = v[0], y = v[1], z = v[2];
				S16(e, 4) = (int16_t)(mul32(x, speed) >> 12);
				S16(e, 6) = (int16_t)(mul32(y, speed) >> 12);
				S16(e, 8) = (int16_t)((mul32(z, speed) >> 12) - 0x1F4);
				const int32_t back = speed / 17;
				S16(e, 0xC) = (int16_t)(-mul32(x, back) >> 12);
				S16(e, 0xE) = (int16_t)(-mul32(y, back) >> 12);
				S16(e, 0x10) = (int16_t)(-mul32(z, back) >> 12);
			}
		}
		U16(node, 0xC) = (uint16_t)(U16(node, 0xC) + 1);
		return S16(node, 0xC) >= 0x3C ? TASK_END : 0;
	}

	// B: looping flipbook; moves by its velocity until tick 0x23 of its task, then back (the
	// velocity x16 at 0x23), decelerating by 1/8 each tick
	static void ParticleBUpdate(uint32_t e, int16_t c)
	{
		S16(e, 0) = (int16_t)(S16(e, 0) + 1);
		if (S16(e, 0) >= 0x11) U16(e, 0) = 1;
		if (c < 0x23)
		{
			U16(e, 4) = (uint16_t)(U16(e, 4) + U16(e, 0xC));
			U16(e, 6) = (uint16_t)(U16(e, 6) + U16(e, 0xE));
			U16(e, 8) = (uint16_t)(U16(e, 8) + U16(e, 0x10));
			return;
		}
		if (c == 0x23)
		{
			U16(e, 0xC) = (uint16_t)(U16(e, 0xC) << 4);
			U16(e, 0xE) = (uint16_t)(U16(e, 0xE) << 4);
			U16(e, 0x10) = (uint16_t)(U16(e, 0x10) << 4);
		}
		U16(e, 4) = (uint16_t)(U16(e, 4) - U16(e, 0xC));
		U16(e, 6) = (uint16_t)(U16(e, 6) - U16(e, 0xE));
		U16(e, 8) = (uint16_t)(U16(e, 8) - U16(e, 0x10));
		for (int k = 0; k < 3; k++)
		{
			const int16_t v = S16(e, 0xC + 2 * k);
			S16(e, 0xC + 2 * k) = (int16_t)(v - (int16_t)(v >> 3));
		}
	}

	// 0x613EF0 (MAG_148_sub_613EF0): pool B, node +0x0C counter, +0x0E action
	static uint32_t __cdecl ParticlesBTask(TaskNode *n)
	{
		uint32_t node = P(n);
		const uint32_t h = ParticleSetup(node, SEQ_AcidB);
		for (uint32_t e = POOL_B; e < POOL_B_END; e += 0x14)
		{
			if (S16(e, 0) < 0) continue;
			// 30 fps layer: see mag148_meltdown_held.inc
			FX_HELD(held_note_particle(ORIG_ParticlesB, e, node, h);)
			ParticleDraw(h, e);
			if (DrawOnly()) continue;
			ParticleBUpdate(e, S16(node, 0xC));
		}
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		const int16_t c = S16(node, 0xC);
		if (c >= 1 && c <= 0x12)
		{
			for (int k = 0; k < 3; k++)
			{
				const uint32_t e = FreeEntry(POOL_B, POOL_B_END);
				if (!e) break;
				U16(e, 0) = 0;
				S16(e, 2) = (int16_t)(CrtRand() % 512 + 0x200);
				int16_t v[4];
				v[0] = (int16_t)(CrtRand() % 4096 - 0x800);
				v[1] = (int16_t)(CrtRand() % 4096 - 0x800);
				v[2] = (int16_t)(CrtRand() % 1024 - 0x200);
				NormalizeSVector(P(v), P(v));
				const int32_t reach = CrtRand() % 500 + 0x2BC;
				const int32_t r24 = CrtRand() % 24;
				const int32_t t = k + r24 + 2 * S16(node, 0xC) - 6;
				S16(e, 4) = (int16_t)(mul32(v[0], reach) >> 12);
				S16(e, 6) = (int16_t)(mul32(v[1], reach) >> 12);
				S16(e, 8) = (int16_t)((mul32(v[2], reach) >> 12) - t * 550);
				int32_t s = CrtRand() % 40 + 0xF;
				if (s & 1) s = -s;
				S16(e, 0xC) = (int16_t)(mul32(v[0], s) >> 12);
				S16(e, 0xE) = (int16_t)(mul32(v[1], s) >> 12);
				S16(e, 0x10) = (int16_t)(mul32(v[2], s) >> 12);
			}
		}
		U16(node, 0xC) = (uint16_t)(U16(node, 0xC) + 1);
		return S16(node, 0xC) >= 0x3C ? TASK_END : 0;
	}
}

	void register_mag148_meltdown()
	{
		register_port(melt148::ORIG_Root, (void *)melt148::RootTask, "M148 RootTask", 148);
		register_port(melt148::ORIG_Director, (void *)melt148::DirectorTask, "M148 DirectorTask", 148);
		register_port(melt148::ORIG_Melt, (void *)melt148::MeltTask, "M148 MeltTask", 148);
		register_port(melt148::ORIG_Disc, (void *)melt148::DiscTask, "M148 DiscTask", 148);
		register_port(melt148::ORIG_CasterGlow, (void *)melt148::CasterGlowTask, "M148 CasterGlowTask", 148);
		register_port(melt148::ORIG_ParticlesA, (void *)melt148::ParticlesATask, "M148 ParticlesATask", 148);
		register_port(melt148::ORIG_ParticlesB, (void *)melt148::ParticlesBTask, "M148 ParticlesBTask", 148);
		// 30 fps layer: see mag148_meltdown_held.inc
		FX_HELD(register_mag148_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag148_meltdown_held.inc"
#endif
