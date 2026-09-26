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

// Effect 291: Pandemona - Tornado Zone (timeline-family GF, MAG_291_*).
//
// Structure (see gf_study/gf_inventory_timeline.md):
//   SequenceTask (master, 0x6ED350) - flips the packet arena, spawns the creature timeline at
//     tick 2 (records the targets, sorts them by x), then runs five single queues in order:
//     creature 0x25562A8, flipbooks 0x2556298, trails 0x2556288, falling-target prims
//     0x2556278, landed-target prims 0x2556268.
//   TimelineTask (0x6ED900) - 480-tick timeline: tornado layers, prim models, particles, the
//     Pandemona model (drawn 100..376, animated 130..376), the targets sucked up, spun, carried
//     round and dropped (their battle entities are hidden and drawn by the module itself while
//     airborne), camera, streamed loads, damage when each target lands.
//   particle tasks: SwirlFlipbook 0x6F0ED0, RiseFlipbook 0x6F0FB0, Trail 0x6F1060,
//     FallPrim 0x6F0E30, LandPrim 0x6F0D90 (prim player callback 0x6F06F0, called by address).
// Module globals: 0x2556258..0x25562F8 (gf_study/gf_global_ranges.md) plus the stream state
// dword 0x13BBBE8 (static data).
//
// Engine and module helpers that are not task functions are called through their original
// addresses (namespace w); only the seven task functions are ported.

#include "fx_port.h"

namespace ff8fx
{
namespace p291
{
	using namespace eng;

	// ------------------------------------------------------------------
	// module globals
	// ------------------------------------------------------------------
	inline uint32_t &Pause() { return var<uint32_t>(0x2556258); }            // debug pause, never set by the game
	inline uint8_t *CastCtx() { return var<uint8_t *>(0x255625C); }
	inline TaskQueue &QLand() { return var<TaskQueue>(0x2556268); }          // stru_2556268.header: LandPrim
	inline TaskQueue &QFall() { return var<TaskQueue>(0x2556278); }          // stru_2556268.data: FallPrim
	inline TaskQueue &QTrail() { return var<TaskQueue>(0x2556288); }         // stru_2556288.header: Trail
	inline TaskQueue &QFlip() { return var<TaskQueue>(0x2556298); }          // stru_2556288.data: flipbooks
	inline TaskQueue &QCreature() { return var<TaskQueue>(0x25562A8); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25562DC); }
	inline int32_t &Ground() { return var<int32_t>(0x25562E0); }             // mean target z (entity +0x20)
	inline int32_t &Center() { return var<int32_t>(0x25562E8); }             // Ground() + 4000: tornado centre
	inline int32_t &Flash() { return var<int32_t>(0x25562F0); }
	inline uint8_t *MB() { return var<uint8_t *>(0x25562F4); }               // = MAGIC_TEXTURE_BUFFER_BASE
	inline int32_t &LoadState() { return var<int32_t>(0x13BBBE8); }          // static data, not a module global
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }      // battle_texture_data_ptr_1D8E054
	inline int16_t &CamW(int i) { return var<int16_t>(0xB8B7F0 + 2 * i); }   // eye x, z(*), y   (*) 683D10 keeps it
	inline int16_t &CamL(int i) { return var<int16_t>(0xB8B7F8 + 2 * i); }   // look-at
	inline int16_t &ShakeY() { return var<int16_t>(0x1D97712); }             // BATTLE_CAMERA_SHAKE_OFFSET_Y
	inline int16_t &Roll() { return var<int16_t>(0x1D977A2); }               // g_BattleCam_Roll
	inline int16_t &W1D8E038() { return var<int16_t>(0x1D8E038); }
	inline int16_t SinT(int i) { return *(const int16_t *)(0x13B7BB8 + 4 * i); } // 4096 (sin, cos) pairs
	inline int16_t CosT(int i) { return *(const int16_t *)(0x13B7BBA + 4 * i); }

	inline int Count() { return *(uint8_t *)(*(uint32_t *)(CastCtx() + 4) + 0x10); }
	inline uint8_t *TargetIds() { return *(uint8_t **)(*(uint32_t *)(CastCtx() + 4) + 8); } // 0x18 bytes per target

	template<typename T> inline T &At(void *p, int o) { return *(T *)((uint8_t *)p + o); }
	template<typename T> inline const T &At(const void *p, int o) { return *(const T *)((const uint8_t *)p + o); }

	static const uint32_t ORIG_SequenceTask = 0x6ED350;
	static const uint32_t ORIG_TimelineTask = 0x6ED900;
	static const uint32_t ORIG_LandPrimTask = 0x6F0D90;
	static const uint32_t ORIG_FallPrimTask = 0x6F0E30;
	static const uint32_t ORIG_SwirlFlipTask = 0x6F0ED0;
	static const uint32_t ORIG_RiseFlipTask = 0x6F0FB0;
	static const uint32_t ORIG_TrailTask = 0x6F1060;
	static const uint32_t CB_Prim = 0x6F06F0; // MAG_291_sub_6F06F0: prim player object -> prim set, rel. to ctx matrix

	static uint32_t g_ported_tick = 0xFFFFFFFF;

	// ------------------------------------------------------------------
	// engine / module helpers (original addresses)
	// ------------------------------------------------------------------
	namespace w
	{
		inline int32_t Rand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); }
		inline int32_t Cos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
		inline void Decode(uint32_t src, void *layout, int n) { fn<void (__cdecl *)(uint32_t, void *, int)>(0x7016B0)(src, layout, n); }
		inline void RotY(int32_t a, void *m) { fn<void (__cdecl *)(int32_t, void *)>(0x701270)(a, m); }           // MAG_063_sub_701270 (3x3 + pad)
		inline void RotX(int32_t a, void *m) { fn<void (__cdecl *)(int32_t, void *)>(0x701220)(a, m); }           // MAG_063_sub_701220
		inline void Scale3D(void *m, const void *v) { fn<void (__cdecl *)(void *, const void *)>(0x56BEF0)(m, v); }
		inline void Basis(const void *v, void *m) { fn<void (__cdecl *)(const void *, void *)>(0x50CBA0)(v, m); } // BuildOrthonormalBasis
		inline void Billboard(void *m) { fn<void (__cdecl *)(void *)>(0x673810)(m); }  // MAG_226_sub_673810: m = camera 3x3, t = camera * t
		// sub_56CB90: out = (a * wa + b * wb) >> 12 on 3 x s16
		inline void VecLerp(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CB90)(a, b, wa, wb, out); }
		inline uint32_t PrimSet(void *hdr, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x701DD0)(hdr, ot, mode, cursor); }
		inline void CamRotate(int32_t angle, int32_t scale) { fn<void (__cdecl *)(int32_t, int32_t)>(0x683D10)(angle, scale); } // MAG_278_sub_683D10
		inline void DefaultPos(void *ent, void *out) { fn<void (__cdecl *)(void *, void *)>(0x571400)(ent, out); }           // GetDefaultEffectPosition
		// streaming / sound / gameplay
		inline void Load(int id, uint32_t dst, int kind) { fn<void (__cdecl *)(int, uint32_t, int)>(0x5341D0)(id, dst, kind); }
		inline int LoadBusy() { return fn<int (__cdecl *)()>(0x534270)(); }
		inline void LoadPump() { fn<void (__cdecl *)()>(0x534210)(); }
		inline int PreLoad1(int a) { return fn<int (__cdecl *)(int)>(0x534300)(a); }
		inline void LoadFile(int id, uint32_t dst, int a, int b) { fn<void (__cdecl *)(int, uint32_t, int, int)>(0x48D0A0)(id, dst, a, b); }
		inline void TimUpload(uint32_t p) { fn<void (__cdecl *)(uint32_t)>(0x505E30)(p); }
		inline void PlaySE(uint32_t snd) { fn<void (__cdecl *)(uint32_t, int, int)>(0x501330)(snd, 1, 0x80); }
		inline void PlayStream() { fn<void (__cdecl *)(int, int, int)>(0x5018C0)(0x80, 1, 0x7F); }
		inline void ReleaseVoice(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
		inline void StreamStateInit(void *s) { fn<void (__cdecl *)(void *)>(0x534110)(s); }
		// creature model
		inline void ReadAnim6(void *e, int id) { fn<void (__cdecl *)(void *, int)>(0x6574D0)(e, id); }   // au_re_Battle_ReadAnimation_6
		inline void Advance(void *e) { fn<void (__cdecl *)(void *)>(0x6FBDB0)(e); }                      // MAG_089_sub_6FBDB0
		// module helpers
		inline void Pivot(void *ent, void *out) { fn<void (__cdecl *)(void *, void *)>(0x6ED770)(ent, out); }                 // bone y range of an entity
		inline void Bind(void *e, void *sec, uint32_t data) { fn<void (__cdecl *)(void *, void *, uint32_t)>(0x6F0A70)(e, sec, data); }
		inline uint32_t DrawCreature(void *e, int a2, uint32_t a3, uint32_t cursor, uint32_t a5) { return fn<uint32_t (__cdecl *)(void *, int, uint32_t, uint32_t, uint32_t)>(0x6F0AE0)(e, a2, a3, cursor, a5); }
		inline void BonePos(void *e, uint32_t data, void *out) { fn<void (__cdecl *)(void *, uint32_t, void *)>(0x6F0CC0)(e, data, out); }
		inline void HitTarget(void *ent) { fn<void (__cdecl *)(void *)>(0x6F0D20)(ent); }                                     // ApplyActionResultToTarget
		inline void Flatten(uint32_t mesh) { fn<void (__cdecl *)(uint32_t)>(0x6F2E40)(mesh); }
		inline void Flatten2(uint32_t mesh) { fn<void (__cdecl *)(uint32_t)>(0x6F2E60)(mesh); }
		inline void PartReset(void *nd) { fn<void (__cdecl *)(void *)>(0x6F1520)(nd); }
		inline int16_t *PartAlloc(void *nd) { return fn<int16_t *(__cdecl *)(void *)>(0x6F1550)(nd); }
		inline void PartFall(void *nd) { fn<void (__cdecl *)(void *)>(0x6F19C0)(nd); }                                          // drift, gravity -10
		inline void PartOrbit(void *nd, int32_t a, int32_t b) { fn<void (__cdecl *)(void *, int32_t, int32_t)>(0x6F1A30)(nd, a, b); } // pos = GTE(R v + T) + swirl
		inline void PartBounce(void *nd) { fn<void (__cdecl *)(void *)>(0x6F1B20)(nd); }                                        // gravity +20, bounce on y >= 0
		inline void PartDrift(void *nd) { fn<void (__cdecl *)(void *)>(0x6F1BA0)(nd); }
		inline void PartDraw(void *nd) { fn<void (__cdecl *)(void *)>(0x6F1650)(nd); }
		inline void Scale3(int32_t a, int32_t s, void *out) { fn<void (__cdecl *)(int32_t, int32_t, void *)>(0x6F15E0)(a, s, out); }
		inline void MatY(int32_t a, int32_t s, int32_t sy, void *m) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, void *)>(0x6F29D0)(a, s, sy, m); } // scaled rotation about y
		inline void Morph(uint32_t a, uint32_t b, int32_t t) { fn<void (__cdecl *)(uint32_t, uint32_t, int32_t)>(0x6F2D20)(a, b, t); }
		typedef void (__cdecl *Fn12)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
		inline void Tornado(const int32_t *a) { fn<Fn12>(0x6F2A30)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11]); }
		typedef void (__cdecl *Fn10)(void *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
		inline void Ribbon(void *m, const int32_t *a) { fn<Fn10>(0x6F1C10)(m, a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]); }
		inline void EntDraw(void *ent, void *ang, void *pivot, void *scale) { fn<void (__cdecl *)(void *, void *, void *, void *)>(0x68D1D0)(ent, ang, pivot, scale); } // MAG_274_sub_68D1D0
		inline int TrailPush(int max, int count, void *entries, void *a, void *b, int c0, int c1, int c2) { return fn<int (__cdecl *)(int, int, void *, void *, void *, int, int, int)>(0x6D99D0)(max, count, entries, a, b, c0, c1, c2); }
		inline uint32_t TrailDraw(int32_t *count, void *entries, int d, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t *, void *, int, uint32_t, int, uint32_t)>(0x6F1290)(count, entries, d, ot, mode, cursor); }
	}

	// ------------------------------------------------------------------
	// Master (0x6ED350). Node: +0x0C u16 counter, +0x0F u8 creature spawned, +0x10 arena parity.
	// Target record (0x20, creature node +0x14 + 0x20 i):
	//   +0x00 entity*       +0x04 s16 bone-y max (feet)  +0x06 s16 bone-y min (head)
	//   +0x08 s16 1/size     +0x0A s16 draw scale (4.12)  +0x0C s16 x, y  +0x10 s16 z (entity +0x1C..)
	//   +0x14 s16 angles x, y, z (entity +0x0C.., y + 0x800)  +0x1C u8 landed ticks
	//   +0x1D u8 drawn by the module  +0x1E u8 not linked to another target (draw the chain)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SequenceTask(TaskNode *n)
	{
		uint8_t *nd = (uint8_t *)n;
		g_ported_tick = g_real_tick;
		if (At<uint32_t>(nd, 0x10))
		{
			PacketCursor() = (uint32_t)(MB() + 0x71D4);
			At<uint32_t>(nd, 0x10) = 0;
		}
		else
		{
			PacketCursor() = (uint32_t)(MB() + 0x151D4);
			At<uint32_t>(nd, 0x10) = 1;
		}
		Flash() = 0;
		if (At<uint16_t>(nd, 0xC) == 2 && !Pause() && !nd[0xF])
		{
			nd[0xF] = 1;
			InitTaskQueuePool(&QCreature(), MB() + 0x21A4, 0x1030, 1);
			InitTaskQueuePool(&QFlip(), MB() + 0x1AA4, 0x1C, 0x40);
			InitTaskQueuePool(&QTrail(), MB() + 0xD90, 0x45C, 3);
			InitTaskQueuePool(&QFall(), MB() + 0x640, 0x9C, 0xC);
			InitTaskQueuePool(&QLand(), MB(), 0x190, 4);
			uint8_t *cr = (uint8_t *)AddTaskToQueue(&QCreature(), ORIG_TimelineTask);
			Memset32(cr + 0xC, 0, 0x409);
			At<uint32_t>(cr, 0x10) = ClaimVoiceSlot((const void *)0x13B7A5C, 1, 0x80);
			At<uint32_t>(cr, 0x102C) = (uint32_t)(cr + 0x42C);
			for (int i = 0; i < Count(); i++)
			{
				uint8_t *rec = cr + 0x14 + 0x20 * i;
				uint8_t *ent = (uint8_t *)(0x1D972C0 + 0x9C * (int32_t)TargetIds()[0x18 * i]);
				At<uint8_t *>(rec, 0) = ent;
				At<uint32_t>(rec, 0xC) = At<uint32_t>(ent, 0x1C);
				At<uint32_t>(rec, 0x10) = At<uint32_t>(ent, 0x20);
				At<uint32_t>(rec, 0x14) = At<uint32_t>(ent, 0xC);
				At<uint32_t>(rec, 0x18) = At<uint32_t>(ent, 0x10);
				At<int16_t>(rec, 0x16) += 0x800;
				rec[0x1C] = 0;
				rec[0x1D] = 1;
				At<int16_t>(rec, 0xA) = 0x1000;
				int16_t size = At<int16_t>(ent, 0x26);
				At<int16_t>(rec, 8) = size == 0 ? (int16_t)0x10 : (int16_t)(0x10000 / ((int32_t)size + 0x400));
				w::Pivot(ent, rec + 4);
			}
			// sort the records by entity x, largest first
			if (Count() - 1 > 0)
			{
				for (int p = 1; p - 1 < Count() - 1; p++)
				{
					for (int j = p; j < Count(); j++)
					{
						uint8_t *a = cr + 0x14 + 0x20 * (p - 1), *b = cr + 0x14 + 0x20 * j;
						if (At<int16_t>(At<uint8_t *>(a, 0), 0x1C) < At<int16_t>(At<uint8_t *>(b, 0), 0x1C))
						{
							uint8_t t[0x20];
							memcpy(t, a, 0x20);
							memcpy(a, b, 0x20);
							memcpy(b, t, 0x20);
						}
					}
				}
			}
			// +0x1E: the entity's part chain (+0x8C) holds no other target
			for (int i = 0; i < Count(); i++)
			{
				uint8_t *rec = cr + 0x14 + 0x20 * i;
				uint8_t *ent = At<uint8_t *>(rec, 0);
				rec[0x1E] = 0;
				uint8_t *e = At<uint8_t *>(ent, 0x8C);
				bool found = false;
				if (e && e != ent)
				{
					while (e)
					{
						if (e[0] & 2)
						{
							int k = 0, nt = Count();
							for (; k < nt; k++)
								if (At<uint8_t *>(cr + 0x14 + 0x20 * k, 0) == e) break;
							if (k < nt) { found = true; break; }
						}
						e = At<uint8_t *>(e, 0x8C);
						if (e == ent) break;
					}
				}
				if (!found) rec[0x1E] = 1;
			}
			w::Flatten(0x13A0E58);
			w::Flatten(0x13A4F88);
			w::Flatten(0x13A6508);
			w::Flatten2(0x13A8508);
			w::Flatten2(0x13AB5C0);
			w::Flatten2(0x13AC1B8);
			w::Flatten2(0x13ACDB0);
			w::StreamStateInit(MB() + 0x231D4);
		}
		uint32_t left = (uint32_t)nd; // the original keeps the node pointer here: non-zero
		if (nd[0xF])
		{
			left = (uint32_t)ExecuteTaskQueue(&QCreature());
			ExecuteTaskQueue(&QFlip());
			ExecuteTaskQueue(&QTrail());
			ExecuteTaskQueue(&QFall());
			ExecuteTaskQueue(&QLand());
		}
		SetScreenFlash((uint32_t)Flash(), 0);
		if (Pause()) return 0;
		if (nd[0xF] && left == 0) return TASK_END;
		At<uint16_t>(nd, 0xC)++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Timeline (0x6ED900). One node at modelBuffer + 0x21A4, 0x1030 bytes:
	//   +0x0C s16 tick, +0x10 voice slot, +0x14 target records (0x20 each),
	//   +0x154 creature model E (+0x40 root matrix, t at +0x54/+0x58/+0x5C; +0x60 BattleAnimHeader,
	//   +0x6C BattleAnimCmd), +0x1F0 prim player layout, +0x41C model section table,
	//   +0x42C particle pool (128 x 0x18: s16 pos[3], life, vel[3], scale, angle A, angle B,
	//   s8 spin A, spin B, u8 model; free hint at +0x102C).
	// The original's stack frame is emulated by fr (esp-relative offsets after the 4 register
	// pushes: locals 0x10..0xAF): ctx = fr + 0x44 (prim player context: matrix, +0x20 s16 depth
	// bias, +0x22 s16 uv scroll, +0x24 mode table, +0x28 vertex scratch = modelBuffer + 0x31D4).
	// ------------------------------------------------------------------
	static uint8_t g_fr[0xB0];
	inline int16_t &F16(uint8_t *fr, int o) { return *(int16_t *)(fr + o); }
	inline int32_t &F32(uint8_t *fr, int o) { return *(int32_t *)(fr + o); }

	// a draw pass: the real tick (held = false) or a held frame at c + num / den
	struct Pass
	{
		uint8_t *nd, *fr;
		int32_t c;
		int num, den;
		bool held;   // draw-only pass (held frame): prim players replay, nothing advances
		bool interp; // held and the next tick is c + 1: parameters lerp towards it
	};

	inline uint8_t *E(uint8_t *nd) { return nd + 0x154; }
	inline uint8_t *Rec(uint8_t *nd, int i) { return nd + 0x14 + 0x20 * i; }
	inline uint8_t *Ent(uint8_t *nd, int i) { return At<uint8_t *>(Rec(nd, i), 0); }

	static void Play(const Pass &p, uint8_t *ctx)
	{
		prim::Layout *l = (prim::Layout *)(p.nd + 0x1F0);
		if (p.held) prim::play_held(l, (prim::Callback)CB_Prim, (int)ctx, p.num, p.den);
		else prim::play(l, (prim::Callback)CB_Prim, (int)ctx, Pause());
	}

	// numeric draw parameters of a block at a tick; a held frame lerps two of them field by field
	template<typename T> static void LerpPar(T &a, const T &b, int num, int den)
	{
		static_assert(sizeof(T) % 4 == 0, "int32 fields only");
		int32_t *x = (int32_t *)&a;
		const int32_t *y = (const int32_t *)&b;
		for (size_t i = 0; i < sizeof(T) / 4; i++) x[i] = lerp_i(x[i], y[i], num, den);
	}

	// held parameters: P(v) lerped to P(v + 1) when tick c + 1 is in the same block and has the
	// same shape, else P(v)
	template<typename T, typename G, typename S> static T HeldPar(const Pass &p, int32_t v, bool next_in, G gen, S same)
	{
		T a, b;
		gen(v, a);
		if (!p.interp || !next_in || Pause()) return a;
		gen(v + 1, b);
		if (same(a, b)) LerpPar(a, b, p.num, p.den);
		return a;
	}

	// ---- 10..39: intro prim model (from 18) ----
	static void DrawA(const Pass &p, int32_t v)
	{
		uint8_t *fr = p.fr;
		int32_t scroll = (int32_t)shl32(v, 3);
		if (p.interp && (uint32_t)(v + 1) < 0x1E && !Pause()) scroll = lerp_i(scroll, (int32_t)shl32(v + 1, 3), p.num, p.den);
		F32(fr, 0x58) = 0;
		F32(fr, 0x5C) = 0;
		F32(fr, 0x60) = Center();
		w::RotY(0x800, fr + 0x44);
		ComposeAffineTransform(&Camera(), (Mat4x3 *)(fr + 0x44), (Mat4x3 *)(fr + 0x44));
		F16(fr, 0x66) = (int16_t)scroll;
		F32(fr, 0x6C) = (int32_t)(MB() + 0x31D4);
		F32(fr, 0x68) = 0x13EF440;
		F16(fr, 0x64) = 0;
		Play(p, fr + 0x44);
	}

	// ---- 24..39: three tornado layers growing ----
	struct ParB { int32_t t[3][12]; };
	static void GenB(int32_t v, ParB &r)
	{
		int32_t e = (int32_t)((uint32_t)shl32(v, 12) / 14u);
		int32_t y = Center() + 0x7D0;
		int32_t a[3][12] = {
			{ y, 0x13A0E58, 0x13A4280, shl32(v, 6), 0, 0, 0, e, e + 0x1000, 0, shl32(v, 5), 0x1000 - (e >> 1) },
			{ y, 0x13A4F88, 0x13A5EF0, shl32(v, 6), 0, 0, 0, e * 2, e + 0x1000, 1, shl32(v, 5), 0x1000 - (e >> 2) },
			{ y, 0x13A6508, 0x13A7BF0, shl32(v, 6), 0, 0, 0, e * 2, e + 0x1000, 1, v * 40, 0x1000 - (e >> 2) } };
		memcpy(r.t, a, sizeof(a));
	}
	static void DrawB(const Pass &p, int32_t v)
	{
		ParB r = HeldPar<ParB>(p, v, (uint32_t)(p.c + 1 - 24) < 0x10, GenB, [](const ParB &, const ParB &) { return true; });
		for (int k = 0; k < 3; k++) w::Tornado(r.t[k]);
	}

	// ---- 40..99: tornado layers, the ground prim sets ----
	struct ParC { int32_t t[3][12]; int32_t e, ang2, fade, third; };
	static void GenC(int32_t v, ParC &r)
	{
		int32_t e = (int32_t)((uint32_t)shl32(v, 12) / 60u);
		int32_t y = Center();
		int32_t a[3][12] = {
			{ y, 0x13A0E58, 0x13A4280, shl32(v, 6), e, shl32(v, 7), e >> 2, shl32(v + 0x20, 7), 0x2000, 0, shl32(v, 5), 0 },
			{ y, 0x13A4F88, 0x13A5EF0, shl32(v, 6), e, shl32(v + 2, 7), e >> 1, shl32(v + 0x10, 8), 0x2000, 1, shl32(v, 5), 0 },
			{ y, 0x13A6508, 0x13A7BF0, shl32(v, 6), e, shl32(v + 4, 7), e >> 1, shl32(v + 0x10, 8), 0x2000, 1, v * 40, 0 } };
		memcpy(r.t, a, sizeof(a));
		r.e = e;
		r.ang2 = v * 100;
		r.fade = shl32(0x3C - v, 8);
		r.third = (uint32_t)v >= 0x2C;
	}
	static void DrawC(const Pass &p, int32_t v)
	{
		ParC r = HeldPar<ParC>(p, v, (uint32_t)(p.c + 1 - 40) < 0x3C, GenC, [](const ParC &a, const ParC &b) { return a.third == b.third; });
		for (int k = 0; k < 3; k++) w::Tornado(r.t[k]);
		uint8_t *ws = (uint8_t *)FieldAlloc(0x8C);
		w::Morph(0x139FFE8, 0x13A0790, r.e);
		w::MatY(r.t[0][3], 0x2000, 0x1000, ws);
		At<int32_t>(ws, 0x14) = 0;
		At<int32_t>(ws, 0x18) = -0x2000;
		At<int32_t>(ws, 0x1C) = Center();
		ComposeAffineTransform(&Camera(), (Mat4x3 *)ws, (Mat4x3 *)ws);
		GteSetRotMatrix((Mat4x3 *)ws);
		GteSetTransVector((Mat4x3 *)ws);
		At<int32_t>(ws, 0x3C) = 0x2030;
		At<int16_t>(ws, 0x42) = 0;
		At<int16_t>(ws, 0x40) = 0;
		At<int32_t>(ws, 0x38) = 0x100;
		At<uint32_t>(ws, 0x24) = (uint32_t)(MB() + 0x31D4);
		At<uint32_t>(ws, 0x20) = 0x139FFE8;
		PacketCursor() = w::PrimSet(ws + 0x20, RenderOT(), 2, PacketCursor());
		w::MatY(r.ang2, 0x1388, 0x1000, ws);
		At<int32_t>(ws, 0x14) = 0;
		At<int32_t>(ws, 0x18) = -0x2000;
		At<int32_t>(ws, 0x1C) = Center();
		ComposeAffineTransform(&Camera(), (Mat4x3 *)ws, (Mat4x3 *)ws);
		GteSetRotMatrix((Mat4x3 *)ws);
		GteSetTransVector((Mat4x3 *)ws);
		At<int32_t>(ws, 0x38) = 0x80;
		At<uint32_t>(ws, 0x20) = 0x139FFE8;
		At<uint32_t>(ws, 0x24) = (uint32_t)(MB() + 0x31D4);
		PacketCursor() = w::PrimSet(ws + 0x20, RenderOT(), 2, PacketCursor());
		if (r.third)
		{
			At<int32_t>(ws, 0x14) = 0;
			At<int32_t>(ws, 0x18) = -0x2000;
			At<int32_t>(ws, 0x1C) = Center();
			w::Billboard(ws);
			GteSetRotMatrix((Mat4x3 *)ws);
			GteSetTransVector((Mat4x3 *)ws);
			At<int32_t>(ws, 0x3C) = 0xF0;
			At<int32_t>(ws, 0x28) = 0;
			At<int32_t>(ws, 0x2C) = r.fade;
			At<int32_t>(ws, 0x38) = 0;
			At<uint32_t>(ws, 0x20) = 0x13A0978;
			PacketCursor() = w::PrimSet(ws + 0x20, RenderOT(), 2, PacketCursor());
		}
		FieldFree(0x8C);
	}

	// ---- 100..153: tornado layers fading out, ground prim sets ----
	struct ParD { int32_t on, more; int32_t t[3][12]; int32_t v36, ang2; };
	static void GenD(int32_t v, ParD &r)
	{
		memset(&r, 0, sizeof(r));
		int32_t v35 = (uint32_t)v < 0x18 ? 0 : (int32_t)((uint32_t)shl32(v - 0x18, 12) / 20u);
		if (v35 > 0x1000) return;
		r.on = 1;
		int32_t y = Center();
		int32_t a0[12] = { y, 0x13A0E58, 0x13A4280, shl32(v, 6), 0x1000, 0, 0, 0x2E00, 0x2000, 0, shl32(v, 5), v35 };
		memcpy(r.t[0], a0, sizeof(a0));
		int32_t v36 = (uint32_t)v < 0x18 ? 0 : (int32_t)((uint32_t)shl32(v - 0x18, 12) / 30u);
		if (v36 > 0x1000) return;
		r.more = 1;
		int32_t a1[12] = { y, 0x13A4F88, 0x13A5EF0, shl32(v, 6), 0x1000, 0, 0, (int32_t)0xFFFF8800, 0x2000, 1, shl32(v, 5), v36 };
		int32_t a2[12] = { y, 0x13A6508, 0x13A7BF0, shl32(v, 6), 0x1000, 0, 0, (int32_t)0xFFFF8800, 0x2000, 1, v * 40, v36 };
		memcpy(r.t[1], a1, sizeof(a1));
		memcpy(r.t[2], a2, sizeof(a2));
		r.v36 = v36;
		r.ang2 = v * 100;
	}
	static void DrawD(const Pass &p, int32_t v)
	{
		ParD r = HeldPar<ParD>(p, v, (uint32_t)(p.c + 1 - 100) < 0x36, GenD,
			[](const ParD &a, const ParD &b) { return a.on == b.on && a.more == b.more && (a.v36 != 0) == (b.v36 != 0); });
		if (!r.on) return;
		w::Tornado(r.t[0]);
		if (!r.more) return;
		w::Tornado(r.t[1]);
		w::Tornado(r.t[2]);
		uint8_t *ws = (uint8_t *)FieldAlloc(0x8C);
		if (r.v36)
		{
			At<int32_t>(ws, 0x3C) = 0x20F0;
			At<int32_t>(ws, 0x2C) = r.v36;
			At<int32_t>(ws, 0x28) = 0;
		}
		else At<int32_t>(ws, 0x3C) = 0x2030;
		At<int16_t>(ws, 0x42) = 0;
		At<int16_t>(ws, 0x40) = 0;
		w::MatY(r.t[0][3], 0x2000, 0x1000, ws);
		At<int32_t>(ws, 0x14) = 0;
		At<int32_t>(ws, 0x18) = -0x2000;
		At<int32_t>(ws, 0x1C) = Center();
		ComposeAffineTransform(&Camera(), (Mat4x3 *)ws, (Mat4x3 *)ws);
		GteSetRotMatrix((Mat4x3 *)ws);
		GteSetTransVector((Mat4x3 *)ws);
		At<int32_t>(ws, 0x38) = 0x100;
		At<uint32_t>(ws, 0x24) = 0x13A0798;
		At<uint32_t>(ws, 0x20) = 0x139FFE8;
		PacketCursor() = w::PrimSet(ws + 0x20, RenderOT(), 2, PacketCursor());
		w::MatY(r.ang2, 0x1388, 0x1000, ws);
		At<int32_t>(ws, 0x14) = 0;
		At<int32_t>(ws, 0x18) = -0x2000;
		At<int32_t>(ws, 0x1C) = Center();
		ComposeAffineTransform(&Camera(), (Mat4x3 *)ws, (Mat4x3 *)ws);
		GteSetRotMatrix((Mat4x3 *)ws);
		GteSetTransVector((Mat4x3 *)ws);
		At<int32_t>(ws, 0x38) = 0x80;
		At<uint32_t>(ws, 0x24) = 0x13A0798;
		At<uint32_t>(ws, 0x20) = 0x139FFE8;
		PacketCursor() = w::PrimSet(ws + 0x20, RenderOT(), 2, PacketCursor());
		At<int32_t>(ws, 0x14) = 0;
		At<int32_t>(ws, 0x18) = -0x2000;
		At<int32_t>(ws, 0x1C) = Center();
		w::Billboard(ws);
		GteSetRotMatrix((Mat4x3 *)ws);
		GteSetTransVector((Mat4x3 *)ws);
		At<uint32_t>(ws, 0x3C) &= ~0x2000u;
		At<int32_t>(ws, 0x38) = 0;
		At<uint32_t>(ws, 0x20) = 0x13A0978;
		PacketCursor() = w::PrimSet(ws + 0x20, RenderOT(), 2, PacketCursor());
		FieldFree(0x8C);
	}

	// ---- 144..173: prim model round the centre ----
	static void DrawE(const Pass &p)
	{
		uint8_t *fr = p.fr;
		F32(fr, 0x58) = 0;
		F32(fr, 0x5C) = 0;
		F32(fr, 0x60) = Center();
		w::RotY(0x800, fr + 0x44);
		ComposeAffineTransform(&Camera(), (Mat4x3 *)(fr + 0x44), (Mat4x3 *)(fr + 0x44));
		F32(fr, 0x6C) = (int32_t)(MB() + 0x31D4);
		F32(fr, 0x68) = 0;
		F16(fr, 0x64) = -128;
		Play(p, fr + 0x44);
	}

	// prim player at a bone of the creature, facing the camera (184..223, 254..283, 284..319)
	static void DrawBonePrim(const Pass &p, int bo, int32_t scroll, int32_t scroll1, bool next_in, int16_t bias, uint32_t table)
	{
		uint8_t *fr = p.fr;
		w::BonePos(E(p.nd), 0x13B7BB0, fr + bo);
		F32(fr, 0x58) = F16(fr, bo);
		F32(fr, 0x5C) = F16(fr, bo + 2);
		F32(fr, 0x60) = F16(fr, bo + 4);
		w::Billboard(fr + 0x44);
		if (p.interp && next_in && !Pause()) scroll = lerp_i(scroll, scroll1, p.num, p.den);
		F16(fr, 0x66) = (int16_t)scroll;
		F32(fr, 0x6C) = (int32_t)(MB() + 0x31D4);
		F32(fr, 0x68) = (int32_t)table;
		F16(fr, 0x64) = bias;
		Play(p, fr + 0x44);
	}

	// ---- 224..253: prim model, stretched ----
	static void DrawG(const Pass &p, int32_t v)
	{
		uint8_t *fr = p.fr;
		F32(fr, 0x58) = 0;
		F32(fr, 0x5C) = 0;
		F32(fr, 0x60) = Ground() - 0x400;
		w::RotY(0x800, fr + 0x44);
		F32(fr, 0x20) = 0x1000;
		F32(fr, 0x1C) = 0x1000;
		F32(fr, 0x24) = 0x1400;
		w::Scale3D(fr + 0x44, fr + 0x1C);
		ComposeAffineTransform(&Camera(), (Mat4x3 *)(fr + 0x44), (Mat4x3 *)(fr + 0x44));
		int32_t s = shl32(v, 5);
		F32(fr, 0x2C) = s;
		if (p.interp && (uint32_t)(p.c + 1 - 224) < 0x1E && !Pause()) s = lerp_i(s, shl32(v + 1, 5), p.num, p.den);
		F16(fr, 0x66) = (int16_t)s;
		F32(fr, 0x68) = 0x13EF430;
		F16(fr, 0x64) = -128;
		F32(fr, 0x6C) = (int32_t)(MB() + 0x31D4);
		Play(p, fr + 0x44);
	}

	// the ring prim set 0x13AEBC0 at (-500, 0, ground) (254..283 always, 284..319 fading)
	static void DrawRing(int32_t scroll, bool fading, int32_t fade)
	{
		uint8_t *ws = (uint8_t *)FieldAlloc(0x8C);
		if (fading) At<int32_t>(ws, 0x2C) = fade;
		if (!fading || fade < 0x1000)
		{
			w::MatY(0x800, 0x1000, 0x1000, ws);
			At<int32_t>(ws, 0x14) = -500;
			At<int32_t>(ws, 0x18) = 0;
			At<int32_t>(ws, 0x1C) = Ground();
			ComposeAffineTransform(&Camera(), (Mat4x3 *)ws, (Mat4x3 *)ws);
			GteSetRotMatrix((Mat4x3 *)ws);
			GteSetTransVector((Mat4x3 *)ws);
			if (fading)
			{
				At<int32_t>(ws, 0x3C) = 0x20F0;
				At<int32_t>(ws, 0x28) = 0;
			}
			else At<int32_t>(ws, 0x3C) = 0x2030;
			At<int16_t>(ws, 0x46) = 0;
			At<int16_t>(ws, 0x44) = 0;
			At<int16_t>(ws, 0x4A) = 0x100;
			At<int16_t>(ws, 0x48) = 0x100;
			At<int16_t>(ws, 0x4C) = 0;
			At<int16_t>(ws, 0x4E) = 0x80;
			At<int16_t>(ws, 0x50) = 0x100;
			At<int16_t>(ws, 0x52) = 0x80;
			At<int16_t>(ws, 0x40) = 0;
			At<int16_t>(ws, 0x42) = (int16_t)(scroll & 0x7F);
			At<int32_t>(ws, 0x38) = 0;
			At<uint32_t>(ws, 0x24) = 0x13AEBC8;
			At<uint32_t>(ws, 0x20) = 0x13AEBC0;
			PacketCursor() = w::PrimSet(ws + 0x20, RenderOT(), 2, PacketCursor());
		}
		FieldFree(0x8C);
	}

	// ---- 335..376: prim model on the creature's hand, oriented up the tornado; four ribbons ----
	struct ParJ { int32_t r[4][10]; };
	static void GenJ(int32_t v, ParJ &p)
	{
		int32_t q = (int32_t)((uint32_t)shl32(v, 12) / 42u) >> 2;
		int32_t f = v * 372;
		if (f > 0x2000) f = 0x2000;
		int32_t g = v * 93 + 0x1000, s = shl32(v, 5), b = shl32(v + 4, 9);
		int32_t a[4][10] = {
			{ 0, 0x13A8508, shl32(v, 6), 0x1400, q, b, 0x2000, 2, s, f },
			{ 0, 0x13AB5C0, shl32(v, 6), shl32(v, 7), q, b, 0x2000, 0, s, g },
			{ 0, 0x13AC1B8, shl32(-v, 6), shl32(v, 7), q, b, 0x2000, 1, s, g },
			{ 0, 0x13ACDB0, shl32(v, 6), shl32(v, 7), q, b, 0x2000, 2, s, g } };
		memcpy(p.r, a, sizeof(a));
	}
	static void DrawJ(const Pass &p, int32_t v)
	{
		uint8_t *fr = p.fr;
		F32(fr, 0x94) = -(int32_t)F16(fr, 0x1E);
		F32(fr, 0x90) = 0;
		F32(fr, 0x98) = Center() - Ground();
		w::Basis(fr + 0x90, fr + 0x70);
		F32(fr, 0x84) = F16(fr, 0x1C);
		F32(fr, 0x88) = F16(fr, 0x1E);
		F32(fr, 0x8C) = F16(fr, 0x20);
		ComposeAffineTransform(&Camera(), (Mat4x3 *)(fr + 0x70), (Mat4x3 *)(fr + 0x44));
		F32(fr, 0x68) = 0;
		F16(fr, 0x64) = -128;
		F32(fr, 0x6C) = (int32_t)(MB() + 0x31D4);
		Play(p, fr + 0x44);
		ParJ r = HeldPar<ParJ>(p, v, (uint32_t)(p.c + 1 - 335) < 0x2A, GenJ, [](const ParJ &, const ParJ &) { return true; });
		for (int k = 0; k < 4; k++) w::Ribbon(fr + 0x70, r.r[k]);
	}

	// ---- 377..416: four ribbons round the ground, then 4 timed prim sets from the table
	// 0x13BBBC0 (12 bytes each: s16 base, rate, y scale, pad, start, end) ----
	struct ParK
	{
		int32_t r[4][10];
		struct Tb { int32_t vis, flags, fade, ang, scale, yscale; } tb[4];
	};
	static void GenK(int32_t v, ParK &p)
	{
		memset(&p, 0, sizeof(p));
		int32_t f = (uint32_t)v <= 0x18 ? 0x2000 : shl32(v - 8, 9);
		int32_t s = shl32(v, 5);
		int32_t a[4][10] = {
			{ 0, 0x13A8508, shl32(v, 6), shl32(v, 7), 0x400, 0x5800, 0x2000, 2, s, f },
			{ 0, 0x13AB5C0, shl32(v, 6), shl32(v, 7), 0x400, 0x5800, 0x2000, 2, s, f },
			{ 0, 0x13AC1B8, shl32(-v, 6), shl32(v, 7), 0x400, 0x5800, 0x2000, 2, s, f },
			{ 0, 0x13ACDB0, shl32(v, 6), shl32(v, 7), 0x400, 0x5800, 0x2000, 2, s, f } };
		memcpy(p.r, a, sizeof(a));
		for (int k = 0; k < 4; k++)
		{
			const int16_t *t = (const int16_t *)(0x13BBBE4 - 0xC * k);
			int32_t start = t[0], end = t[1];
			ParK::Tb &b = p.tb[k];
			if ((uint32_t)v <= (uint32_t)start || (uint32_t)v >= (uint32_t)(end + 8)) continue;
			b.vis = 1;
			if ((uint32_t)v < (uint32_t)(start + 8)) { b.flags = 0xF0; b.fade = shl32(start + 8, 9) - shl32(v, 9); }
			else if ((uint32_t)v > (uint32_t)end) { b.flags = 0xF0; b.fade = shl32(v - end, 9); }
			else b.flags = 0x30;
			b.ang = shl32(v, 6);
			b.scale = (int16_t)((int16_t)((int16_t)v - t[0]) * t[-3] + t[-4]);
			b.yscale = t[-2];
		}
	}
	static void DrawK(const Pass &p, int32_t v)
	{
		uint8_t *fr = p.fr;
		ParK r0, r1;
		GenK(v, r0);
		ParK r = r0;
		if (p.interp && (uint32_t)(p.c + 1 - 377) < 0x28 && !Pause())
		{
			GenK(v + 1, r1);
			for (int k = 0; k < 4; k++)
				for (int j = 0; j < 10; j++) r.r[k][j] = lerp_i(r0.r[k][j], r1.r[k][j], p.num, p.den);
			for (int k = 0; k < 4; k++)
			{
				ParK::Tb &a = r.tb[k];
				const ParK::Tb &b = r1.tb[k];
				if (!a.vis || !b.vis || a.flags != b.flags) continue;
				a.fade = lerp_i(a.fade, b.fade, p.num, p.den);
				a.ang = lerp_i(a.ang, b.ang, p.num, p.den);
				a.scale = lerp_angle((int16_t)a.scale, (int16_t)b.scale, p.num, p.den);
			}
		}
		w::RotX(0x400, fr + 0x90);
		F32(fr, 0xA4) = 0;
		F32(fr, 0xA8) = 0;
		F32(fr, 0xAC) = Ground();
		for (int k = 0; k < 4; k++) w::Ribbon(fr + 0x90, r.r[k]);
		uint8_t *ws = (uint8_t *)FieldAlloc(0x8C);
		int32_t y = -0x15C8;
		At<int16_t>(ws, 0x42) = 0;
		At<int16_t>(ws, 0x40) = 0;
		At<int32_t>(ws, 0x28) = 0;
		for (int k = 0; k < 4; k++, y += 0x898)
		{
			const ParK::Tb &b = r.tb[k];
			if (!b.vis) continue;
			At<int32_t>(ws, 0x3C) = b.flags;
			if (b.flags == 0xF0) At<int32_t>(ws, 0x2C) = b.fade;
			w::MatY(b.ang, b.scale, b.yscale, ws);
			At<int32_t>(ws, 0x14) = 0;
			At<int32_t>(ws, 0x18) = y;
			At<int32_t>(ws, 0x1C) = Ground();
			ComposeAffineTransform(&Camera(), (Mat4x3 *)ws, (Mat4x3 *)ws);
			GteSetRotMatrix((Mat4x3 *)ws);
			GteSetTransVector((Mat4x3 *)ws);
			At<int32_t>(ws, 0x38) = 0;
			At<uint32_t>(ws, 0x20) = 0x13AE0F8;
			PacketCursor() = w::PrimSet(ws + 0x20, RenderOT(), 2, PacketCursor());
		}
		FieldFree(0x8C);
	}

	// ---- 224+: the airborne targets, drawn by the module (their battle entities are hidden) ----
	static void DrawTargets(const Pass &p)
	{
		uint8_t *nd = p.nd, *fr = p.fr;
		F16(fr, 0x20) = 0;
		F16(fr, 0x1C) = 0;
		F32(fr, 0x3C) = 0;
		for (int i = 0; i < Count(); i++)
		{
			uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
			if (rec[0x1C]) continue;
			if (rec[0x1D] && (ent[0] & 2))
			{
				F16(fr, 0x1E) = (int16_t)(((int32_t)At<int16_t>(rec, 4) + At<int16_t>(rec, 6)) >> 1);
				int32_t s = At<int16_t>(rec, 0xA);
				F32(fr, 0x34) = s;
				F32(fr, 0x30) = s;
				F32(fr, 0x2C) = s;
				At<int32_t>(ent, 0x54) = At<int16_t>(ent, 0x1C);
				At<int32_t>(ent, 0x58) = At<int16_t>(ent, 0x1E);
				At<int32_t>(ent, 0x5C) = At<int16_t>(ent, 0x20);
				if (rec[0x1E])
				{
					uint8_t *e = ent;
					do
					{
						if (e[0] & 2) w::EntDraw(e, rec + 0x14, fr + 0x1C, fr + 0x2C);
						e = At<uint8_t *>(e, 0x8C);
					} while (e != ent && e);
				}
				else w::EntDraw(ent, rec + 0x14, fr + 0x1C, fr + 0x2C);
			}
			if (rec[0x1E])
			{
				uint8_t *e = ent;
				do
				{
					if (At<uint16_t>(e, 0) & 2) At<uint16_t>(e, 0) |= 4;
					e = At<uint8_t *>(e, 0x8C);
				} while (e != ent && e);
			}
			else ent[0] |= 4;
		}
	}

	static bool CreatureDrawn(int32_t c) { return c >= 100 && c < 377 && (c < 224 || c >= 254); }

	// ------------------------------------------------------------------
	// Target motion (shared by the real tick and the held-frame prediction). fr carries the
	// creature's hand position (BonePos) at +0x2C (254..283) / +0x1C (335..376).
	// ------------------------------------------------------------------
	static void MotionSink(uint8_t *nd, int32_t v) // 224..253: pulled towards the tornado
	{
		for (int i = 0; i < Count(); i++)
		{
			uint8_t *ent = Ent(nd, i);
			At<int16_t>(ent, 0x20) += (int16_t)v;
			if ((uint32_t)v >= (uint32_t)(15 + 2 * i))
			{
				At<int16_t>(ent, 0x20) += (int16_t)shl32(v, 5);
				At<int16_t>(ent, 0x1E) -= (int16_t)shl32(v, 3);
			}
		}
	}

	static void MotionLift(uint8_t *nd, uint8_t *fr, int32_t v) // 254..283: lifted to the hand, one after another
	{
		int32_t wgt = (int32_t)((uint32_t)shl32(v, 12) / 20u);
		for (int i = 0; i < Count(); i++)
		{
			uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
			wgt -= 0x100;
			if (wgt > 0x1100) { rec[0x1D] = 0; continue; }
			F16(fr, 0x3C) = At<int16_t>(rec, 0xC);
			F16(fr, 0x3E) = (int16_t)(At<int16_t>(rec, 0xE) - 0x3E8);
			F16(fr, 0x40) = At<int16_t>(rec, 0x10);
			F16(fr, 0x1C) = F16(fr, 0x2C);
			int32_t avg = ((int32_t)At<int16_t>(rec, 4) + At<int16_t>(rec, 6)) / 2;
			F16(fr, 0x1E) = (int16_t)(F32(fr, 0x2E) - avg);
			F16(fr, 0x20) = F16(fr, 0x30);
			if (i & 1)
			{
				At<int16_t>(rec, 0x18) += 0x200;
				At<int16_t>(rec, 0x16) += (int16_t)shl32(v, 3);
			}
			else
			{
				At<int16_t>(rec, 0x18) += (int16_t)0xFE00;
				At<int16_t>(rec, 0x16) += (int16_t)shl32(-v, 3);
			}
			w::VecLerp(fr + 0x3C, fr + 0x1C, 0x1000 - wgt, wgt, ent + 0x1C);
			wgt = 0x1000 - w::Cos(wgt >> 2);
			if (wgt < 0) wgt = 0;
			At<int16_t>(rec, 0xA) = (int16_t)(mul32(At<int16_t>(rec, 8), wgt) / 4096 - wgt + 0x1000);
		}
	}

	static void MotionSpin(uint8_t *nd, uint8_t *fr, int32_t v) // 335..376: spun up round the hand, then flung up
	{
		int32_t wgt = (int32_t)((uint32_t)shl32(v, 12) >> 5);
		for (int i = 0; i < Count(); i++)
		{
			uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
			wgt -= 0x100;
			if (wgt < 0) continue;
			int32_t d;
			if (i & 1) { At<int16_t>(rec, 0x18) += 0x200; d = shl32(v, 3); }
			else { At<int16_t>(rec, 0x18) += (int16_t)0xFE00; d = shl32(-v, 3); }
			At<int16_t>(rec, 0x16) += (int16_t)d;
			if (wgt >= 0xC00)
			{
				At<int16_t>(ent, 0x1E) += (int16_t)0xFE00;
				At<int16_t>(rec, 0xA) = 0x1000;
				continue;
			}
			F16(fr, 0x10) = (int16_t)(At<int16_t>(rec, 0xC) >> 1);
			F16(fr, 0x12) = At<int16_t>(rec, 0xE);
			F16(fr, 0x14) = At<int16_t>(rec, 0x10);
			F16(fr, 0x3C) = F16(fr, 0x1C);
			int32_t avg = ((int32_t)At<int16_t>(rec, 4) + At<int16_t>(rec, 6)) / 2;
			F16(fr, 0x3E) = (int16_t)(F32(fr, 0x1E) - avg);
			F16(fr, 0x40) = F16(fr, 0x20);
			w::VecLerp(fr + 0x10, fr + 0x3C, wgt, 0x1000 - wgt, ent + 0x1C);
			wgt = ComputeSin(wgt >> 2);
			rec[0x1D] = 1;
			At<int16_t>(rec, 0xA) = (int16_t)(mul32(At<int16_t>(rec, 8), 0x1000 - wgt) / 4096 + wgt);
		}
	}

	static void OrbitReset(uint8_t *nd) // 377: back to the recorded position, fresh angles
	{
		for (int i = 0; i < Count(); i++)
		{
			uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
			At<uint32_t>(ent, 0x1C) = At<uint32_t>(rec, 0xC);
			At<uint32_t>(ent, 0x20) = At<uint32_t>(rec, 0x10);
			At<uint32_t>(rec, 0x14) = At<uint32_t>(ent, 0xC);
			At<uint32_t>(rec, 0x18) = At<uint32_t>(ent, 0x10);
			At<int16_t>(rec, 0x18) = (int16_t)((i & 1) ? 0x100 : -0x100);
		}
	}

	static void MotionOrbit(uint8_t *nd, int32_t v) // 377..416: carried round the tornado
	{
		int32_t base = shl32(v, 7);
		for (int i = 0; i < Count(); i++)
		{
			uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
			At<int16_t>(rec, 0x16) += (int16_t)((i & 1) ? 0x100 : -0x100);
			int32_t a = (ent[4] == 0x8E ? mul32(v, -800) : mul32(v, -350)) - shl32(i, 11);
			At<int16_t>(ent, 0x1E) = (int16_t)a;
			int32_t a2 = (int16_t)(a << 1);
			int32_t b = (base + a2) & 0xFFF;
			int32_t r = (int16_t)(*(const int16_t *)(0x13B7BB8 + 2 * (a2 & 0xFFE)) >> 2);
			At<int16_t>(ent, 0x1C) = (int16_t)(mul32(SinT(b), r) >> 12);
			At<int16_t>(ent, 0x20) = (int16_t)((mul32(CosT(b), r) >> 12) + Ground());
		}
	}

	static void DropReset(uint8_t *nd) // 440: above the recorded position
	{
		for (int i = 0; i < Count(); i++)
		{
			uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
			int16_t x = At<int16_t>(rec, 0xC);
			At<int16_t>(rec, 0x18) += 0x800;
			At<int16_t>(ent, 0x1C) = x;
			At<int16_t>(ent, 0x20) = At<int16_t>(rec, 0x10);
			At<int16_t>(ent, 0x1E) -= At<int16_t>(rec, 4);
		}
	}

	// 440..479: the drop. real = the tick itself (prims, damage, unhide, shake); else prediction
	static void MotionDrop(uint8_t *nd, int32_t v, bool real)
	{
		for (int i = 0; i < Count(); i++)
		{
			uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
			uint8_t landed = rec[0x1C];
			if (landed) { rec[0x1C] = landed + 1; continue; }
			At<int16_t>(rec, 0x16) += (int16_t)((i & 1) ? 0x100 : -0x100);
			At<int16_t>(ent, 0x1E) += 0x400;
			if ((int32_t)At<int16_t>(ent, 0x1E) < -(int32_t)At<int16_t>(rec, 4))
			{
				if (real && (v & 3) == 0)
				{
					uint8_t *t = (uint8_t *)AddTaskToQueue(&QFall(), ORIG_FallPrimTask);
					if (t)
					{
						At<uint8_t *>(t, 0xC) = rec;
						w::Decode((uint32_t)(MB() + 0x30760), t + 0x10, 0x8C);
					}
				}
				continue;
			}
			if (real)
			{
				uint8_t *t = (uint8_t *)AddTaskToQueue(&QLand(), ORIG_LandPrimTask);
				if (t)
				{
					At<uint8_t *>(t, 0xC) = rec;
					w::Decode((uint32_t)(MB() + 0x2EAA0), t + 0x10, 0x180);
				}
			}
			At<int16_t>(ent, 0x1E) = (int16_t)-At<int16_t>(rec, 4);
			At<int16_t>(rec, 0x18) = At<int16_t>(ent, 0x10);
			At<int16_t>(rec, 0x16) = (int16_t)(At<int16_t>(ent, 0xE) + 0x800);
			if (real)
			{
				w::HitTarget(ent);
				if (rec[0x1E])
				{
					uint8_t *e = ent;
					do
					{
						if (At<uint16_t>(e, 0) & 2) At<uint16_t>(e, 0) &= 0xFFFB;
						e = At<uint8_t *>(e, 0x8C);
					} while (e != ent && e);
				}
				else At<uint16_t>(ent, 0) &= 0xFFFB;
			}
			rec[0x1C] = 1;
			if (real && i == 0) ShakeY() += 0x3E8;
		}
	}

	// four new particles at the tornado floor, outward drift (40..99, 100..124, 377..416)
	static void SpawnFloorParticles(uint8_t *nd)
	{
		int k = 4;
		for (int16_t *q = w::PartAlloc(nd); q; q = w::PartAlloc(nd))
		{
			int32_t r = w::Rand() & 0xFFF;
			q[4] = (int16_t)(SinT(r) >> 3);
			q[6] = (int16_t)(CosT(r) >> 3);
			q[5] = 0;
			q[3] = 0xC;
			q[7] = 0x1000;
			if (--k == 0) break;
		}
	}

	// particles out of the hand position (fr + bo: x, y, z), half = 184..223 (radius halved)
	static void SpawnHandParticles(uint8_t *nd, uint8_t *fr, int bo, int32_t n, bool half)
	{
		for (int32_t k = n; k != 0; k--)
		{
			int16_t *q = w::PartAlloc(nd);
			if (!q) break;
			int32_t r = w::Rand() & 0xFFF;
			int16_t sx = half ? (int16_t)(SinT(r) >> 1) : SinT(r);
			q[4] = sx;
			q[5] = half ? (int16_t)(CosT(r) >> 1) : CosT(r);
			int16_t cz = q[5];
			q[0] = (int16_t)(sx + F32(fr, bo));
			q[1] = (int16_t)(F32(fr, bo + 2) + cz);
			q[2] = (int16_t)(F32(fr, bo + 4) - 0x1000);
			q[6] = 0x200;
			q[4] = (int16_t)(-(int32_t)sx >> 3);
			q[5] = (int16_t)(-(int32_t)cz >> 3);
			q[3] = 8;
			q[7] = (int16_t)(half ? 0x800 : 0x1000);
		}
	}

	// the flipbook swirls round the tornado (24..39: radius from the tornado growth; 40..99)
	static void SpawnSwirls(int32_t n, int16_t r, int16_t y, int16_t va, int16_t vb, int16_t da)
	{
		for (int32_t k = n; k != 0; k--)
		{
			uint8_t *t = (uint8_t *)AddTaskToQueue(&QFlip(), ORIG_SwirlFlipTask);
			if (!t) break;
			At<int16_t>(t, 0xC) = 0;
			At<int16_t>(t, 0x16) = va;
			At<int16_t>(t, 0x18) = vb;
			At<int16_t>(t, 0x10) = (int16_t)w::Rand();
			At<int16_t>(t, 0x12) = da;
			At<int16_t>(t, 0x14) = y;
			At<int16_t>(t, 0xE) = r;
		}
	}

	// ------------------------------------------------------------------
	// Camera part of the timeline (0x6EFDE5..0x6F060E), run by the real tick (real = true:
	// sounds, the model set-up at 99 and the voice release) and by the held-frame prediction on
	// the real globals (real = false, pure math, put back by the caller). Returns true on a cut.
	// ------------------------------------------------------------------
	static bool CamStep(uint8_t *nd, int32_t c, bool real)
	{
		bool cut = false;
		int32_t y8 = Center();
		if ((uint32_t)c < 0x28)
		{
			if (c == 0)
			{
				cut = true;
				if (real) w::PlaySE(0x13B7A48);
				Roll() = 0;
				W1D8E038() = 0xF0;
				CamL(0) = 0;
				CamL(1) = (int16_t)0xFC7C;
				CamL(2) = (int16_t)(y8 - 0x7D0);
				CamW(0) = 0;
				CamW(1) = (int16_t)0xFB50;
				CamW(2) = (int16_t)(y8 - 200);
			}
			w::CamRotate(0, shl32(0x228 - c, 3));
			CamW(1) = (int16_t)0xFB50;
		}
		if (c == 0x27)
		{
			cut = true;
			W1D8E038() = 0x120;
			CamL(0) = 0;
			CamL(1) = (int16_t)0xF458;
			CamL(2) = (int16_t)(y8 - 0x840);
			CamW(0) = (int16_t)0xF000;
			CamW(1) = (int16_t)0xFB48;
			CamW(2) = (int16_t)(y8 - 0x2396);
		}
		if ((uint32_t)(c - 0x28) < 0x3C) w::CamRotate(0x10, 0x1000);
		if (c == 0x63)
		{
			cut = true;
			if (real)
			{
				uint8_t *e = E(nd);
				At<int16_t>(nd, 0x1A2) = 0;
				At<int16_t>(nd, 0x1A4) = 0x1800;
				At<int16_t>(nd, 0x19C) = 0x1800;
				At<int16_t>(nd, 0x194) = 0x1800;
				At<int16_t>(nd, 0x1A0) = 0;
				At<int16_t>(nd, 0x19E) = 0;
				At<int16_t>(nd, 0x19A) = 0;
				At<int16_t>(nd, 0x198) = 0;
				At<int16_t>(nd, 0x196) = 0;
				At<int32_t>(nd, 0x1A8) = 0;
				At<int16_t>(nd, 0x170) = 0;
				At<int32_t>(nd, 0x1AC) = -0x2000;
				At<int16_t>(nd, 0x172) = (int16_t)0xE000;
				At<int32_t>(nd, 0x1B0) = Center();
				At<int16_t>(nd, 0x174) = (int16_t)Center();
				w::Bind(e, nd + 0x41C, (uint32_t)(MB() + 0x31220));
				w::ReadAnim6(e, 0);
			}
			CamL(2) = (int16_t)Center();
			CamL(0) = 0;
			CamL(1) = (int16_t)0xEE61;
			CamW(0) = 0;
			CamW(1) = (int16_t)0xF976;
			CamW(2) = (int16_t)(Center() - 0x936);
			if (real) w::PlaySE(0x13B7A4C);
		}
		y8 = Center();
		if (c == 0x6B && real) w::PlayStream();
		if (c == 0x7B)
		{
			cut = true;
			CamL(0) = (int16_t)0xFB6E;
			CamL(1) = (int16_t)0xEA57;
			CamL(2) = (int16_t)(y8 - 0x484);
			CamW(0) = 0x5F9;
			CamW(1) = (int16_t)0xEEEA;
			CamW(2) = (int16_t)(y8 + 0xA08);
		}
		{
			uint32_t v = (uint32_t)(c - 0x7C);
			if (v < 0x1E)
			{
				w::CamRotate(0, 0x1054);
				CamL(1) += 0x82;
				uint32_t s = v - 0x14;
				if (s < 6)
				{
					int32_t k = (int32_t)(s * 0x155 + 0xAA);
					int16_t sh = (int16_t)(mul32(w::Cos(k), 100) >> 12);
					ShakeY() = sh;
					if (s & 1) ShakeY() = (int16_t)-sh;
				}
			}
		}
		if (c == 0x99)
		{
			cut = true;
			CamL(0) = (int16_t)0xFDEC;
			CamL(1) = (int16_t)0xF768;
			CamL(2) = (int16_t)(y8 - 0xEA);
			CamW(0) = 0x76B;
			CamW(1) = (int16_t)0xF7B7;
			CamW(2) = (int16_t)(y8 - 0x557);
		}
		if ((uint32_t)(c - 0x9A) < 0xA) w::CamRotate(-0x15, 0xFA0);
		else if (c - 0x9A == 0xA)
		{
			cut = true;
			CamL(0) = (int16_t)0xFE25;
			CamL(1) = (int16_t)0xF624;
			CamL(2) = (int16_t)(y8 - 0x171);
			CamW(0) = (int16_t)0xFE95;
			CamW(1) = (int16_t)0xFA8B;
			CamW(2) = (int16_t)(y8 - 0x1CCD);
		}
		{
			uint32_t v = (uint32_t)(c - 0xB7);
			if (v < 0x29)
			{
				int32_t s = (int32_t)(shl32(v, 12) / 41u);
				if (v == 0 && real) w::PlayStream();
				int32_t k1 = w::Cos(s >> 2);
				int32_t k2 = w::Cos((0x1000 - k1) >> 2);
				int32_t wb = 0x1000 - k2, wa = 0x1000 - wb;
				int16_t a[3] = { (int16_t)0xFE25, (int16_t)0xF624, (int16_t)(Center() - 0x171) };
				int16_t b[3] = { (int16_t)0xFE64, (int16_t)0xF31C, (int16_t)(Center() + 0x4E4) };
				w::VecLerp(a, b, wa, wb, &CamL(0));
				int16_t a2[3] = { (int16_t)0xFE95, (int16_t)0xFA8B, (int16_t)(Center() - 0x1CCD) };
				int16_t b2[3] = { (int16_t)0xFEA9, (int16_t)0xF769, (int16_t)(Center() - 0x25D) };
				w::VecLerp(a2, b2, wa, wb, &CamW(0));
			}
		}
		if (c == 0xDF)
		{
			cut = true;
			if (real) w::PlaySE(0x13B7A50);
			int32_t g = Ground();
			CamL(0) = 0x34F;
			CamL(1) = (int16_t)0xF9F4;
			CamL(2) = (int16_t)(g + 0x250);
			CamW(0) = 0x14E2;
			CamW(1) = (int16_t)0xFE1B;
			CamW(2) = (int16_t)(g + 0x83C);
		}
		if ((uint32_t)(c - 0x9A) < 0x1E) w::CamRotate(0, 0x1000);
		y8 = Center();
		if (c == 0xFD)
		{
			cut = true;
			CamL(0) = 0;
			CamL(1) = (int16_t)0xF4EE;
			CamL(2) = (int16_t)y8;
			CamW(0) = (int16_t)0xFAEA;
			CamW(1) = (int16_t)0xFEB1;
			CamW(2) = (int16_t)(y8 + 0x148C);
		}
		{
			uint32_t v = (uint32_t)(c - 0xFE);
			if (v < 0x1E)
			{
				int32_t a = (int32_t)((shl32(v, 12) / 30u) >> 1);
				w::CamRotate(ComputeSin(a) >> 8, 0xFA0);
				CamW(1) += (int16_t)0xFF38;
			}
		}
		if (c == 0x11B)
		{
			cut = true;
			int32_t g = Ground();
			CamL(0) = (int16_t)0xFEC5;
			CamL(1) = (int16_t)0xF904;
			CamW(0) = (int16_t)0xFA97;
			CamL(2) = (int16_t)(g + 0x1395);
			CamW(1) = (int16_t)0xFD39;
			CamW(2) = (int16_t)(g - 0xA62);
		}
		y8 = Center();
		if (c == 0x13F)
		{
			cut = true;
			CamL(0) = (int16_t)0xFF8D;
			CamL(1) = (int16_t)0xF556;
			CamL(2) = (int16_t)(y8 + 0x279);
			CamW(0) = 0x159;
			CamW(1) = (int16_t)0xF07F;
			CamW(2) = (int16_t)(y8 - 0x1EB);
		}
		{
			uint32_t v = (uint32_t)(c - 0x14E);
			if (v < 0x2B)
			{
				int32_t s = (int32_t)(shl32(v, 12) / 43u);
				if (v == 0 && real) w::PlayStream();
				int32_t k = ComputeSin(s >> 2);
				int16_t a[3] = { (int16_t)0xFF8D, (int16_t)0xF556, (int16_t)(Center() + 0x279) };
				int16_t b[3] = { (int16_t)0xFD15, (int16_t)0xF982, (int16_t)(Center() - 0x795) };
				w::VecLerp(a, b, 0x1000 - k, k, &CamL(0));
				int16_t a2[3] = { 0x159, (int16_t)0xF07F, (int16_t)(Center() - 0x1EB) };
				int16_t b2[3] = { 0xD7E, (int16_t)0xFEA4, (int16_t)(Center() - 0x1EAB) };
				w::VecLerp(a2, b2, 0x1000 - k, k, &CamW(0));
			}
		}
		if (c == 0x178)
		{
			cut = true;
			if (real) w::PlaySE(0x13B7A54);
			CamL(0) = 0;
			uint8_t *ent = Ent(nd, 0);
			int32_t g = Ground();
			CamL(2) = (int16_t)g;
			CamL(1) = (int16_t)((((int32_t)At<int16_t>(ent, 0x1E) + 0x1000) >> 2) - 0x1000);
			CamW(0) = (int16_t)0xFC88;
			CamW(1) = (int16_t)0xF1A7;
			CamW(2) = (int16_t)(g - 0x1DD7);
		}
		if ((uint32_t)(c - 0x179) < 0x28)
		{
			uint8_t *ent = Ent(nd, 0);
			CamL(1) = (int16_t)((((int32_t)At<int16_t>(ent, 0x1E) + 0x1000) >> 2) - 0x1000);
			w::CamRotate(0x20, 0x1000);
		}
		if ((uint32_t)(c - 0x1B8) < 0x28)
		{
			if (c - 0x1B8 == 8 && real) w::PlaySE(0x13B7A58);
			if (Rec(nd, 0)[0x1C] <= 2)
			{
				uint8_t *ent = Ent(nd, 0);
				w::DefaultPos(ent, &CamL(0));
				int16_t half = (int16_t)(At<int16_t>(ent, 0x26) >> 1);
				CamW(0) = (int16_t)(half + CamL(0) + 0xBB8);
				int16_t lz = CamL(1);
				CamW(2) = CamL(2);
				CamW(1) = (int16_t)((int16_t)(lz >> 2) - 0x3E8);
				CamL(1) = (int16_t)(lz >> 1);
			}
		}
		if (c == 0x1DC && real) w::ReleaseVoice(At<uint32_t>(nd, 0x10));
		return cut;
	}

	// ------------------------------------------------------------------
	// Held-frame memo of the real tick
	// ------------------------------------------------------------------
	struct CreatureMemo
	{
		uint32_t tick;
		uint8_t *nd;
		int32_t c;
		bool pose_ok;
		uint32_t pose_size;
		uint8_t pose[16 + 48 * 64];
	};
	static CreatureMemo g_cm;

	static uint8_t *CreatureSkeleton(uint8_t *nd, uint32_t *size)
	{
		uint8_t *com = At<uint8_t *>(nd, 0x1B8); // E + 0x60 BattleAnimHeader.comFileData
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0 || sk[0] > 64) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return sk;
	}

	static uint32_t __cdecl TimelineTask(TaskNode *n)
	{
		uint8_t *nd = (uint8_t *)n;
		uint8_t *fr = g_fr;
		int32_t c = At<int16_t>(nd, 0xC);

		// held memo: the skeleton as the tick starts is the pose this tick draws (the draw comes
		// before this tick's anim advance)
		g_cm.tick = g_real_tick;
		g_cm.nd = nd;
		g_cm.c = c;
		{
			uint32_t size = 0;
			uint8_t *sk = CreatureSkeleton(nd, &size);
			g_cm.pose_ok = sk && size <= sizeof(g_cm.pose);
			if (g_cm.pose_ok) { memcpy(g_cm.pose, sk, size); g_cm.pose_size = size; }
		}
		Pass p = { nd, fr, c, 0, 1, false, false };

		// screen flash
		if (c < 8) Flash() = (2800 * c) >> 3;
		else if (c <= 0x1D8) Flash() = 0xAF0;
		else Flash() = (2800 * (480 - c)) >> 3;

		// 10..39: intro prim model, falling particles, then (24..) tornado and swirls
		{
			uint32_t v = (uint32_t)(c - 10);
			if (v < 0x1E)
			{
				if (v == 0) w::Decode(0x13AFF48, nd + 0x1F0, 0x12C);
				else if (v >= 8) DrawA(p, (int32_t)v);
				if (!Pause())
				{
					int k = 4;
					for (int16_t *q = w::PartAlloc(nd); q; q = w::PartAlloc(nd))
					{
						int32_t r = w::Rand();
						q[1] = 0x3E8;
						q[0] = (int16_t)((r & 0xFFF) - 0x800);
						q[2] = (int16_t)(Center() + 0x3E8);
						q[3] = 0x14;
						q[4] = 0;
						q[5] = -100;
						q[6] = -400;
						q[7] = 0x1000;
						if (--k == 0) break;
					}
					w::PartFall(nd);
				}
				if (v > 0xE)
				{
					int32_t v6 = (int32_t)v - 0xE;
					DrawB(p, v6);
					if (!Pause())
					{
						int32_t e = (int32_t)((uint32_t)shl32(v6, 12) / 14u);
						SpawnSwirls(v6 >> 1, (int16_t)(e >> 2), (int16_t)(Center() + 0x7D0), 0x1000, 0x800, 0x80);
					}
				}
			}
		}

		// 40..99: tornado, swirls, orbiting floor particles, ground prim sets
		{
			uint32_t v = (uint32_t)(c - 0x28);
			if (v < 0x3C)
			{
				int32_t e = (int32_t)((uint32_t)shl32(v, 12) / 60u);
				if (v == 0) w::PartReset(nd);
				if (!Pause())
				{
					SpawnFloorParticles(nd);
					SpawnSwirls((int32_t)(v >> 1), (int16_t)shl32(v, 7), (int16_t)Center(), 0x2000, 0x1000, 0);
					w::MatY(shl32(v, 6), shl32(v + 0x40, 6), 0x2000, fr + 0x70);
					F32(fr, 0x88) = 0;
					F32(fr, 0x84) = 0;
					F32(fr, 0x8C) = Center();
					GteSetRotMatrix((Mat4x3 *)(fr + 0x70));
					GteSetTransVector((Mat4x3 *)(fr + 0x70));
					w::PartOrbit(nd, shl32(v, 7), e >> 1);
				}
				DrawC(p, (int32_t)v);
			}
		}

		// 100..153: the creature rises (root y), floor particles, trails at 114, tornado fades
		{
			uint32_t v = (uint32_t)(c - 0x64);
			if (v < 0x36)
			{
				if (!Pause())
				{
					if (v <= 0x18)
					{
						At<int32_t>(nd, 0x1AC) = (int32_t)(shl32(v, 13) / 24) - 0x2000;
						SpawnFloorParticles(nd);
						w::MatY(shl32(v, 6), shl32(v + 0x40, 6), 0x2000, fr + 0x70);
						F32(fr, 0x88) = 0;
						F32(fr, 0x84) = 0;
						F32(fr, 0x8C) = Center();
						GteSetRotMatrix((Mat4x3 *)(fr + 0x70));
						GteSetTransVector((Mat4x3 *)(fr + 0x70));
						w::PartOrbit(nd, 0x3C, 0x800);
						if (v == 0x18)
						{
							// the floor particles either die or are flung out, rising
							int16_t *q = (int16_t *)(nd + 0x42C);
							for (int k = 0; k < 0x80; k++, q += 12)
							{
								if (q[3] == 0) continue;
								if (q[5] < -0x400)
								{
									q[3] += 0x18;
									q[4] = (int16_t)((int32_t)q[0] / 256);
									q[6] = (int16_t)(((int32_t)q[2] - Center()) / 256);
									q[5] = (int16_t)((w::Rand() & 0x7F) + 0x40);
								}
								else
								{
									q[3] = 0;
									At<uint32_t>(nd, 0x102C) = (uint32_t)q;
								}
							}
						}
						else if (v == 0xE)
						{
							for (int32_t k = 2;; k--)
							{
								uint8_t *t = (uint8_t *)AddTaskToQueue(&QTrail(), ORIG_TrailTask); // (no null test)
								At<int16_t>(t, 0xC) = 0;
								At<int32_t>(t, 0x20) = 0;
								At<int16_t>(t, 0x10) = (int16_t)(shl32(k, 11) - 0x1770);
								At<int16_t>(t, 0x1A) = 0x1000;
								At<int16_t>(t, 0x1E) = 0;
								At<int16_t>(t, 0x1C) = 0;
								At<int16_t>(t, 0x14) = 0xD2;
								At<int16_t>(t, 0x18) = 0x40;
								At<int16_t>(t, 0x12) = (int16_t)(k * 0x555);
								At<int16_t>(t, 0x16) = (int16_t)(k * 0x555);
								if (k & 1)
								{
									At<int16_t>(t, 0x14) = (int16_t)0xFF2E;
									At<int16_t>(t, 0x18) = (int16_t)0xFFC0;
								}
								if (k == 0) break;
							}
						}
					}
					else w::PartBounce(nd);
				}
				DrawD(p, (int32_t)v);
			}
		}

		// 144..173: prim model round the centre
		{
			uint32_t v = (uint32_t)(c - 0x90);
			if (v < 0x1E)
			{
				if (v == 0) w::Decode(0x13B0C9C, nd + 0x1F0, 0xE0);
				DrawE(p);
			}
		}
		if (c == 0x9A) w::PartReset(nd);

		// 184..223: prim model on the hand, particles out of it
		{
			uint32_t v = (uint32_t)(c - 0xB8);
			if (v < 0x28)
			{
				if (v == 0) w::Decode((uint32_t)(MB() + 0x23220), nd + 0x1F0, 0x22C);
				DrawBonePrim(p, 0x1C, (int32_t)(v * v), (int32_t)((v + 1) * (v + 1)), (uint32_t)(c + 1 - 0xB8) < 0x28, -128, 0x13EF438);
				if (!Pause())
				{
					SpawnHandParticles(nd, fr, 0x1C, (int32_t)(v >> 3), true);
					w::PartDrift(nd);
				}
			}
		}

		// 224..253: stretched prim model, targets pulled in
		{
			uint32_t v = (uint32_t)(c - 0xE0);
			if (v < 0x1E)
			{
				if (v == 0)
				{
					w::PartReset(nd);
					w::Decode(0x13B17AC, nd + 0x1F0, 0x1D8);
				}
				DrawG(p, (int32_t)v);
				if (!Pause())
				{
					for (int32_t k = 3;; k--)
					{
						int16_t *q = w::PartAlloc(nd);
						if (!q) break;
						q[0] = (int16_t)((w::Rand() & 0xFFF) - 0x800);
						q[1] = (int16_t)-(w::Rand() & 0x7FF);
						q[2] = (int16_t)(Ground() - 0x1000);
						q[3] = 0xC;
						q[4] = 0;
						q[5] = 0;
						q[6] = 0x2AA;
						q[7] = 0x1000;
						if (k == 0) break;
					}
					w::PartDrift(nd);
					MotionSink(nd, (int32_t)v);
				}
			}
		}

		// 254..283: hand prim, ring, particles and rising flipbooks, the targets lifted
		{
			uint32_t v = (uint32_t)(c - 0xFE);
			if (v < 0x1E)
			{
				if (v == 0)
				{
					w::PartReset(nd);
					w::Decode((uint32_t)(MB() + 0x23220), nd + 0x1F0, 0x8C);
				}
				bool nx = (uint32_t)(c + 1 - 0xFE) < 0x1E;
				DrawBonePrim(p, 0x2C, shl32(v, 5), shl32(v + 1, 5), nx, 0x40, 0x13EF42C);
				DrawRing(shl32(v, 5), false, 0);
				if (!Pause())
				{
					SpawnHandParticles(nd, fr, 0x2C, (int32_t)(v >> 2), false);
					for (int32_t k = 3;; k--)
					{
						uint8_t *t = (uint8_t *)AddTaskToQueue(&QFlip(), ORIG_RiseFlipTask);
						if (!t) break;
						At<int16_t>(t, 0x12) = 0;
						At<int16_t>(t, 0x18) = 0x2000;
						At<int16_t>(t, 0x1A) = 0x1000;
						int32_t x = (w::Rand() & 0x1FFF) - 0x1000;
						int32_t lo = (int8_t)(uint8_t)x;
						At<int16_t>(t, 0xC) = (int16_t)x;
						int32_t q = (int32_t)(((int64_t)lo * (int32_t)0xD5555555) >> 32) >> 5;
						t[0x14] = (uint8_t)(q + (int32_t)((uint32_t)q >> 31));
						At<int16_t>(t, 0x10) = (int16_t)Ground();
						t[0x16] = 0x14;
						At<int16_t>(t, 0xE) = 0;
						t[0x15] = 0;
						if (k == 0) break;
					}
					w::PartDrift(nd);
					MotionLift(nd, fr, (int32_t)v);
				}
			}
		}

		// 284..319: hand prim, ring fading, particles slowed down
		{
			uint32_t v = (uint32_t)(c - 0x11C);
			if (v < 0x24)
			{
				if (v == 0)
				{
					int16_t *q = (int16_t *)(nd + 0x42C);
					for (int k = 0; k < 0x80; k++, q += 12)
					{
						if (q[3] == 0) continue;
						q[4] >>= 4;
						q[6] >>= 4;
						q[3] = 0x24;
						q[7] = 0x800;
					}
					w::Decode((uint32_t)(MB() + 0x2786C), nd + 0x1F0, 0x11C);
				}
				bool nx = (uint32_t)(c + 1 - 0x11C) < 0x24;
				DrawBonePrim(p, 0x1C, shl32(v, 5), shl32(v + 1, 5), nx, 0x80, 0x13EF42C);
				DrawRing(shl32(v, 5), true, (int32_t)((uint32_t)shl32(v, 12) >> 4));
				if (!Pause()) w::PartBounce(nd);
			}
		}

		// 335..376: particles fall, the targets spun round the hand, four ribbons
		{
			uint32_t v = (uint32_t)(c - 0x14F);
			if (v < 0x2A)
			{
				if (v == 0)
				{
					w::PartReset(nd);
					w::Decode((uint32_t)(MB() + 0x2C074), nd + 0x1F0, 0x1C0);
				}
				w::BonePos(E(nd), 0x13B7BB0, fr + 0x1C);
				if (!Pause())
				{
					int k = 4;
					for (int16_t *q = w::PartAlloc(nd); q; q = w::PartAlloc(nd))
					{
						q[0] = (int16_t)((w::Rand() & 0xFFF) - 0x800);
						q[1] = 0;
						q[3] = 0xC;
						q[2] = (int16_t)(Center() - 0x3E8);
						q[4] = 0;
						q[5] = (int16_t)(-0x20 - (w::Rand() & 0x40));
						q[6] = -400;
						q[7] = 0x1000;
						if (--k == 0) break;
					}
					w::PartFall(nd);
					MotionSpin(nd, fr, (int32_t)v);
				}
				DrawJ(p, (int32_t)v);
			}
		}

		// 377..416: the targets carried round, floor particles orbit, ribbons and timed rings
		{
			uint32_t v = (uint32_t)(c - 0x179);
			if (v < 0x28)
			{
				if (v == 0)
				{
					OrbitReset(nd);
					w::PartReset(nd);
				}
				if (!Pause())
				{
					SpawnFloorParticles(nd);
					w::MatY(shl32(v, 6), 0x5800, 0x2000, fr + 0x90);
					F32(fr, 0xA8) = 0;
					F32(fr, 0xA4) = 0;
					F32(fr, 0xAC) = Ground();
					GteSetRotMatrix((Mat4x3 *)(fr + 0x90));
					GteSetTransVector((Mat4x3 *)(fr + 0x90));
					w::PartOrbit(nd, 0x28, 0x800);
					MotionOrbit(nd, (int32_t)v);
				}
				DrawK(p, (int32_t)v);
			}
		}

		// 417..439: particles slowed, bouncing (the bounce update is not pause-guarded)
		{
			uint32_t v = (uint32_t)(c - 0x1A1);
			if (v < 0x17)
			{
				if (v == 0 && !Pause())
				{
					int16_t *q = (int16_t *)(nd + 0x42C);
					for (int k = 0; k < 0x80; k++, q += 12)
					{
						if (q[3] == 0) continue;
						q[3] += 0x28;
						q[4] >>= 2;
						q[5] >>= 2;
						q[6] >>= 2;
					}
				}
				w::PartBounce(nd);
			}
		}

		// 440..479: the drop
		uint32_t vdrop = (uint32_t)(c - 0x1B8);
		if (vdrop < 0x28)
		{
			if (vdrop == 0) DropReset(nd);
			if (!Pause())
			{
				MotionDrop(nd, (int32_t)vdrop, true);
				w::PartBounce(nd);
			}
		}
		if (vdrop < 0x5C8)
		{
			// landed targets stand on their feet (bone y range recomputed every tick)
			for (int i = 0; i < Count(); i++)
			{
				uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
				if (!rec[0x1C]) continue;
				if (*(uint8_t *)(At<uint32_t>(ent, 0x74) + 0x2C) & 0x40) continue;
				if (!(ent[0] & 2)) continue;
				w::Pivot(ent, rec + 4);
				int16_t y = (int16_t)-At<int16_t>(rec, 4);
				At<int16_t>(ent, 0x1E) = y;
				At<int32_t>(ent, 0x58) = y;
			}
		}

		w::PartDraw(nd);
		if (c >= 0xE0) DrawTargets(p);
		if (CreatureDrawn(c))
			FrameCursor() = w::DrawCreature(E(nd), 0x2000, (uint32_t)(MB() + 0x31D4), FrameCursor(), var<uint32_t>(0x1D969A8));

		// streamed loads (the timeline waits while the loader is busy)
		{
			uint32_t D = (uint32_t)(MB() + 0x2321C);
			switch ((uint16_t)c)
			{
			case 0:
				w::Load(0x2CF, D, 2);
				w::Load(0x2D0, (uint32_t)(MB() + 0x2321C), 1);
				w::Load(0x2D1, (uint32_t)(MB() + 0x2321C), 3);
				w::Load(0x2D2, (uint32_t)(MB() + 0x31220), 5);
				break;
			case 0x63:
				if (w::LoadBusy()) return 0;
				w::Load(0x2D3, D, 3);
				w::Load(0x2D4, (uint32_t)(MB() + 0x23220), 5);
				break;
			case 0xB7:
				if (w::LoadBusy()) return 0;
				break;
			case 0xE0:
				w::Load(0x2D5, D, 3);
				w::Load(0x2D6, (uint32_t)(MB() + 0x23220), 5);
				break;
			case 0xFD:
				if (w::LoadBusy()) return 0;
				break;
			case 0x1E0:
				if (QLand().head) return 0;
				w::Load(0x168, (uint32_t)(MB() + 0x31220), 1);
				w::Load(0x169, (uint32_t)(MB() + 0x31220), 1);
				w::Load(0x16A, (uint32_t)(MB() + 0x31220), 1);
				LoadState() = 0;
				break;
			default: break;
			}
		}
		w::LoadPump();
		switch (LoadState())
		{
		case 0:
			w::LoadFile(0x168, (uint32_t)(MB() + 0x31220), 0, 0);
			w::TimUpload((uint32_t)(MB() + 0x31220));
			LoadState() = 1;
			break;
		case 1:
			w::LoadFile(0x169, (uint32_t)(MB() + 0x31220), 0, 0);
			w::TimUpload((uint32_t)(MB() + 0x31220));
			LoadState() = 2;
			break;
		case 2:
			w::LoadFile(0x16A, (uint32_t)(MB() + 0x31220), 0, 0);
			w::TimUpload((uint32_t)(MB() + 0x31220));
			LoadState() = 3;
			break;
		default: break;
		}
		// the creature's animation (NOT pause-guarded), after its draw
		if (c >= 0x82 && c < 0x179 && (c < 0xE0 || c >= 0xFE)) w::Advance(E(nd));

		CamStep(nd, c, true);

		At<int16_t>(nd, 0xC) = (int16_t)(At<int16_t>(nd, 0xC) + 1);
		if (At<int16_t>(nd, 0xC) <= 0x1E0) return 0;
		Flash() = 0;
		if (At<int16_t>(nd, 0xC) < 0x1FE)
		{
			int cnt = Count();
			for (int i = 0; i < cnt; i++)
				if (!(*(uint8_t *)(At<uint32_t>(Ent(nd, i), 0x74) + 0x2C) & 0x40)) return 0;
		}
		if (!w::PreLoad1(2))
		{
			At<int16_t>(nd, 0xC) = 0x208;
			return 0;
		}
		for (int i = 0; i < Count(); i++)
		{
			uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
			At<int16_t>(ent, 0x1C) = At<int16_t>(rec, 0xC);
			At<int16_t>(ent, 0x1E) = At<int16_t>(rec, 0xE);
			At<int16_t>(ent, 0x20) = At<int16_t>(rec, 0x10);
		}
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Target prim players (0x6F0D90 landed, 0x6F0E30 falling; identical code, two queues).
	// Node: +0x0C record*, +0x10 prim player layout. Scaled by the entity size, at the entity
	// position (y + the record's foot offset).
	// ------------------------------------------------------------------
	static void TargetPrimCtx(uint8_t *ctx, uint8_t *rec, const int16_t *pos)
	{
		uint8_t *ent = At<uint8_t *>(rec, 0);
		At<int32_t>(ctx, 0x14) = pos[0];
		At<int32_t>(ctx, 0x18) = (int32_t)pos[1] + At<int16_t>(rec, 4);
		At<int32_t>(ctx, 0x1C) = pos[2];
		w::Scale3(0, (int16_t)(At<int16_t>(ent, 0x26) + 0x400), ctx);
		ComposeAffineTransform(&Camera(), (Mat4x3 *)ctx, (Mat4x3 *)ctx);
		At<int32_t>(ctx, 0x24) = 0;
		At<int16_t>(ctx, 0x20) = -128;
		At<uint32_t>(ctx, 0x28) = (uint32_t)(MB() + 0x31D4);
	}

	static uint32_t __cdecl TargetPrimTask(TaskNode *n)
	{
		uint8_t *nd = (uint8_t *)n;
		uint8_t *rec = At<uint8_t *>(nd, 0xC);
		uint8_t ctx[0x2C];
		TargetPrimCtx(ctx, rec, &At<int16_t>(At<uint8_t *>(rec, 0), 0x1C));
		int left = prim::play((prim::Layout *)(nd + 0x10), (prim::Callback)CB_Prim, (int)ctx, Pause());
		return left != 0 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Swirl flipbook (0x6F0ED0), 12 frames of 0x13AFCC0 circling the tornado. Node 0x1C:
	// +0x0C s16 frame, +0x0E s16 radius, +0x10 s16 angle, +0x12 s16 angle step,
	// +0x14 s16 z, +0x16 s16 size, +0x18 s16 camera push (x -1/4)
	// ------------------------------------------------------------------
	static void SwirlDraw(uint8_t *nd, int16_t angle, int16_t frame)
	{
		int32_t i = angle & 0xFFF;
		int32_t r = At<int16_t>(nd, 0xE);
		int16_t pos[3];
		pos[0] = (int16_t)(mul32(SinT(i), r) >> 12);
		pos[1] = 0;
		pos[2] = (int16_t)((int16_t)(mul32(CosT(i), r) >> 12) + At<int16_t>(nd, 0x14));
		TransformCameraByShadowRotation(pos, At<int16_t>(nd, 0x16), -(At<int16_t>(nd, 0x18) >> 2));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = 0x13AFCC0;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SwirlFlipTask(TaskNode *n)
	{
		uint8_t *nd = (uint8_t *)n;
		SwirlDraw(nd, At<int16_t>(nd, 0x10), At<int16_t>(nd, 0xC));
		if (Pause()) return 0;
		At<int16_t>(nd, 0xC)++;
		if (At<int16_t>(nd, 0xC) >= 0xC) return TASK_END;
		At<int16_t>(nd, 0x10) += At<int16_t>(nd, 0x12);
		return 0;
	}

	// ------------------------------------------------------------------
	// Rising flipbook (0x6F0FB0), 12 frames of 0x13AFE04. Node 0x1C: +0x0C s16 pos[3],
	// +0x12 s16 frame, +0x14 s8 vel[3] (x16 per tick), +0x18 s16 size, +0x1A s16 camera push
	// ------------------------------------------------------------------
	static void RiseDraw(uint8_t *nd, const int16_t *pos, int16_t frame)
	{
		TransformCameraByShadowRotation(pos, At<int16_t>(nd, 0x18), -(At<int16_t>(nd, 0x1A) >> 2));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = 0x13AFE04;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl RiseFlipTask(TaskNode *n)
	{
		uint8_t *nd = (uint8_t *)n;
		RiseDraw(nd, &At<int16_t>(nd, 0xC), At<int16_t>(nd, 0x12));
		if (Pause()) return 0;
		At<int16_t>(nd, 0x12)++;
		if (At<int16_t>(nd, 0x12) >= 0xC) return TASK_END;
		for (int k = 0; k < 3; k++) At<int16_t>(nd, 0xC + 2 * k) += (int16_t)shl32((int8_t)nd[0x14 + k], 4);
		return 0;
	}

	// ------------------------------------------------------------------
	// Trail (0x6F1060): a glowing ribbon wound up the tornado. Node 0x45C:
	//   +0x0C s16 age, +0x10 s16 height, +0x12 s16 phase, +0x14 s16 phase step, +0x16 s16 spin,
	//   +0x18 s16 spin step, +0x1A s16 scale, +0x1C s16 scale step, +0x1E s16 step change,
	//   +0x20 s32 entries, +0x24 30 entries x 0x24 (two 8-byte points, projected edges,
	//   +0x20 u8 brightness, +0x21 s8 brightness change (-4 per tick), +0x22 u8 colour).
	// For 21 ticks a new pair of points is pushed at the head; the ribbon fades by itself.
	// VANILLA UNINITIALISED READ: the 4th word of both 8-byte points (stack slots +0x0E / +0x16
	// of the frame, never written) is copied into every new entry; the port keeps what the
	// previous tick left there (g_trail_pts), vanilla holds whatever the stack held.
	// ------------------------------------------------------------------
	static int16_t g_trail_pts[8]; // frame +0x08: point b (x, y, z, ?), frame +0x10: point a

	struct TrailMemo { int16_t spin, scale; };
	static NodeMemo<TrailMemo, 16> g_trail_memo;

	static void TrailMatrix(uint8_t *m, int16_t spin, int16_t scale)
	{
		w::MatY(spin, scale, (int16_t)((scale >> 2) + 0xC00), m);
		At<int32_t>(m, 0x18) = 0;
		At<int32_t>(m, 0x14) = 0;
		At<int32_t>(m, 0x1C) = Center();
		ComposeAffineTransform(&Camera(), (Mat4x3 *)m, (Mat4x3 *)m);
		GteSetRotMatrix((Mat4x3 *)m);
		GteSetTransVector((Mat4x3 *)m);
	}

	static uint32_t __cdecl TrailTask(TaskNode *n)
	{
		uint8_t *nd = (uint8_t *)n;
		int16_t age = At<int16_t>(nd, 0xC);
		if (age <= 0x14 && !Pause())
		{
			int32_t t = (int32_t)(((int64_t)shl32(age, 12) * 0x66666667) >> 32) >> 3;
			t += (int32_t)((uint32_t)t >> 31);
			int32_t k = (ComputeSin(t + t) >> 3) + 0x1000;
			int32_t s = mul32(ComputeSin(At<int16_t>(nd, 0x12)), k) >> 14;
			g_trail_pts[0] = (int16_t)s;
			g_trail_pts[4] = (int16_t)s;
			int32_t cz = mul32(w::Cos(At<int16_t>(nd, 0x12)), k);
			At<int16_t>(nd, 0x12) += At<int16_t>(nd, 0x14);
			cz >>= 14;
			g_trail_pts[2] = (int16_t)cz;
			g_trail_pts[6] = (int16_t)cz;
			g_trail_pts[1] = (int16_t)(At<int16_t>(nd, 0x10) + (int16_t)t);
			int16_t y2;
			if (age < 8) y2 = (int16_t)(g_trail_pts[1] - (shl32(age, 9) >> 3) - 0x20);
			else if (age > 0xC) y2 = (int16_t)(g_trail_pts[1] - (shl32(0x14 - age, 9) >> 3) - 0x20);
			else y2 = (int16_t)(g_trail_pts[1] - 0x200);
			g_trail_pts[5] = y2;
			At<int32_t>(nd, 0x20) = w::TrailPush(0x1E, At<int32_t>(nd, 0x20), nd + 0x24, g_trail_pts + 4, g_trail_pts, 0x10, 0x58, (3 - (age & 3)) << 5);
		}
		if (TrailMemo *m = g_trail_memo.put(n))
		{
			m->spin = At<int16_t>(nd, 0x16);
			m->scale = At<int16_t>(nd, 0x1A);
		}
		uint8_t mat[0x20];
		TrailMatrix(mat, At<int16_t>(nd, 0x16), At<int16_t>(nd, 0x1A));
		PacketCursor() = w::TrailDraw(&At<int32_t>(nd, 0x20), nd + 0x24, -4, RenderOT(), 2, PacketCursor());
		if (Pause()) return 0;
		At<int16_t>(nd, 0x16) += At<int16_t>(nd, 0x18);
		int16_t a = At<int16_t>(nd, 0xC);
		if (a >= 0x14)
		{
			if (a == 0x14)
			{
				At<int16_t>(nd, 0x1A) = 0x1000;
				At<int16_t>(nd, 0x1C) = 0x400;
				At<int16_t>(nd, 0x1E) = (int16_t)0xFFC0;
			}
			int16_t d = At<int16_t>(nd, 0x1C);
			At<int16_t>(nd, 0x1A) += d;
			int16_t d2 = (int16_t)(At<int16_t>(nd, 0x1E) + d);
			At<int16_t>(nd, 0x1C) = d2;
			if (d2 < 0x40) At<int16_t>(nd, 0x1C) = 0x40;
		}
		int16_t na = (int16_t)(a + 1);
		At<int16_t>(nd, 0xC) = na;
		if (na > 8 && At<int32_t>(nd, 0x20) < 2) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). The logic stays vanilla; held frames only draw, on copies or on
	// state that is put back byte for byte afterwards (creature node, entity array, skeleton).
	// ------------------------------------------------------------------
	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static uint8_t *Creature()
	{
		TaskNode *t = QCreature().head;
		return (t && (uint32_t)t->func == ORIG_TimelineTask) ? (uint8_t *)t : nullptr;
	}

	static void PoseBlend(uint8_t *sk, const uint8_t *from, int num, int den)
	{
		// sk holds the pose after the tick (the one the next tick draws), from the pose drawn
		// this tick: write the midpoint (angles the short way round) into sk's pose fields
		int nb = sk[0];
		bool scaled = (sk[1] & 1) != 0;
		for (int a = 0; a < 3; a++)
		{
			int16_t *q = (int16_t *)(sk + 8) + a;
			int16_t f = ((const int16_t *)(from + 8))[a];
			*q = (int16_t)(f + ((int32_t)*q - f) * num / den);
		}
		for (int b = 0; b < nb; b++)
		{
			int16_t *q = (int16_t *)(sk + 16 + 48 * b + 4);
			const int16_t *f = (const int16_t *)(from + 16 + 48 * b + 4);
			for (int a = 0; a < 3; a++)
			{
				int32_t d = (((int32_t)q[a] - f[a] + 2048) & 4095) - 2048;
				q[a] = (int16_t)(f[a] + d * num / den);
			}
			if (scaled)
				for (int a = 3; a < 6; a++) q[a] = (int16_t)(f[a] + ((int32_t)q[a] - f[a]) * num / den);
		}
	}

	static const uint32_t ENT_BASE = 0x1D972C0, ENT_SIZE = 0x440; // BattleEntitySlotData[7]
	static uint8_t g_node_save[0x1030], g_ent_save[ENT_SIZE], g_skel_save[16 + 48 * 64];
	static uint8_t g_hfr[0xB0];

	// Next tick's particles and targets (c1 = the next tick), run on the real node / entities
	// (pure math: no generator, no spawn, no event) and put back by the caller. parts = also
	// the particle updates. hand = the hand position at the next tick's pose (null: unknown).
	// Returns false when the next tick changes the particles at random (tick 124).
	static bool PredictNext(uint8_t *nd, int32_t c1, const int16_t *hand, bool parts)
	{
		bool particles = parts;
		uint8_t *fr = g_hfr;
		if (Pause()) return particles;
		uint32_t v;
		if (parts && (c1 == 0x28 || c1 == 0x9A || c1 == 0xE0 || c1 == 0xFE || c1 == 0x14F || c1 == 0x179)) w::PartReset(nd);
		if (parts && (v = (uint32_t)(c1 - 10)) < 0x1E) w::PartFall(nd);
		if (parts && (v = (uint32_t)(c1 - 0x28)) < 0x3C)
		{
			int32_t e = (int32_t)((uint32_t)shl32(v, 12) / 60u);
			Mat4x3 m;
			w::MatY(shl32(v, 6), shl32(v + 0x40, 6), 0x2000, &m);
			m.t[0] = 0; m.t[1] = 0; m.t[2] = Center();
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			w::PartOrbit(nd, shl32(v, 7), e >> 1);
		}
		if ((v = (uint32_t)(c1 - 0x64)) < 0x36)
		{
			if (v <= 0x18)
			{
				At<int32_t>(nd, 0x1AC) = (int32_t)(shl32(v, 13) / 24) - 0x2000;
				if (parts)
				{
					Mat4x3 m;
					w::MatY(shl32(v, 6), shl32(v + 0x40, 6), 0x2000, &m);
					m.t[0] = 0; m.t[1] = 0; m.t[2] = Center();
					GteSetRotMatrix(&m);
					GteSetTransVector(&m);
					w::PartOrbit(nd, 0x3C, 0x800);
				}
				if (v == 0x18) particles = false;
			}
			else if (parts) w::PartBounce(nd);
		}
		if (parts && (v = (uint32_t)(c1 - 0xB8)) < 0x28) w::PartDrift(nd);
		if ((v = (uint32_t)(c1 - 0xE0)) < 0x1E)
		{
			if (parts) w::PartDrift(nd);
			MotionSink(nd, (int32_t)v);
		}
		if ((v = (uint32_t)(c1 - 0xFE)) < 0x1E)
		{
			if (parts) w::PartDrift(nd);
			if (hand)
			{
				F16(fr, 0x2C) = hand[0];
				F16(fr, 0x2E) = hand[1];
				F16(fr, 0x30) = hand[2];
				MotionLift(nd, fr, (int32_t)v);
			}
		}
		if (parts && (v = (uint32_t)(c1 - 0x11C)) < 0x24)
		{
			if (v == 0)
			{
				int16_t *q = (int16_t *)(nd + 0x42C);
				for (int k = 0; k < 0x80; k++, q += 12)
					if (q[3]) { q[4] >>= 4; q[6] >>= 4; q[3] = 0x24; q[7] = 0x800; }
			}
			w::PartBounce(nd);
		}
		if ((v = (uint32_t)(c1 - 0x14F)) < 0x2A)
		{
			if (parts) w::PartFall(nd);
			if (hand)
			{
				F16(fr, 0x1C) = hand[0];
				F16(fr, 0x1E) = hand[1];
				F16(fr, 0x20) = hand[2];
				MotionSpin(nd, fr, (int32_t)v);
			}
		}
		if ((v = (uint32_t)(c1 - 0x179)) < 0x28)
		{
			if (v == 0) OrbitReset(nd);
			if (parts)
			{
				Mat4x3 m;
				w::MatY(shl32(v, 6), 0x5800, 0x2000, &m);
				m.t[0] = 0; m.t[1] = 0; m.t[2] = Ground();
				GteSetRotMatrix(&m);
				GteSetTransVector(&m);
				w::PartOrbit(nd, 0x28, 0x800);
			}
			MotionOrbit(nd, (int32_t)v);
		}
		if (parts && (v = (uint32_t)(c1 - 0x1A1)) < 0x17)
		{
			if (v == 0)
			{
				int16_t *q = (int16_t *)(nd + 0x42C);
				for (int k = 0; k < 0x80; k++, q += 12)
					if (q[3]) { q[3] += 0x28; q[4] >>= 2; q[5] >>= 2; q[6] >>= 2; }
			}
			w::PartBounce(nd);
		}
		if ((v = (uint32_t)(c1 - 0x1B8)) < 0x28)
		{
			if (v == 0) DropReset(nd);
			MotionDrop(nd, (int32_t)v, false);
			if (parts) w::PartBounce(nd);
		}
		return particles;
	}

	// the hand position (bone vector 0x13B7BB0) at the next tick's pose: the skeleton's bone
	// matrices already hold it (the anim advanced after this tick's draw)
	static bool NextHand(uint8_t *nd, int16_t *hand)
	{
		uint32_t size = 0;
		if (!CreatureSkeleton(nd, &size)) return false;
		int16_t h[4];
		w::BonePos(E(nd), 0x13B7BB0, h);
		memcpy(hand, h, 6);
		return true;
	}

	inline bool Jump(int32_t a, int32_t b) { return a - b > 1500 || b - a > 1500; }

	static int16_t g_next_pool[0x80 * 12];
	static uint8_t g_next_recs[0x140];
	static int16_t g_next_pos[10][3];
	static int16_t g_held_pos[10][3]; // held target positions (also used by the target prims)
	static uint8_t *g_held_nd = nullptr;

	static int Targets() { int n = Count(); return n > 10 ? 10 : n; }

	// the timeline's draws of tick c (memo) at c + num / den, in the tick's order, on the real
	// node / entities / skeleton moved to the in-between state and put back byte for byte
	static void CreatureHeld(int num, int den)
	{
		uint8_t *nd = Creature();
		g_held_nd = nullptr;
		if (!nd || g_cm.tick != g_real_tick || g_cm.nd != nd) return;
		int32_t c = g_cm.c;
		int32_t c1 = At<int16_t>(nd, 0xC); // the next tick (== c when this tick waited on a load)
		bool interp = c1 == c + 1;
		int n = Targets();

		memcpy(g_node_save, nd, sizeof(g_node_save));
		memcpy(g_ent_save, (void *)ENT_BASE, ENT_SIZE);
		uint32_t size = 0;
		uint8_t *sk = CreatureSkeleton(nd, &size);
		if (sk) memcpy(g_skel_save, sk, size);

		if (interp)
		{
			int16_t hand[3];
			bool hand_ok = NextHand(nd, hand);
			bool next_particles = PredictNext(nd, c1, hand_ok ? hand : nullptr, true);
			memcpy(g_next_pool, nd + 0x42C, sizeof(g_next_pool));
			memcpy(g_next_recs, nd + 0x14, sizeof(g_next_recs));
			int32_t root1 = At<int32_t>(nd, 0x1AC);
			for (int i = 0; i < n; i++) memcpy(g_next_pos[i], Ent(nd, i) + 0x1C, 6);
			memcpy(nd, g_node_save, sizeof(g_node_save));
			memcpy((void *)ENT_BASE, g_ent_save, ENT_SIZE);

			// particles: the drawn state -> the next update
			if (next_particles)
			{
				int16_t *q = (int16_t *)(nd + 0x42C);
				const int16_t *r = g_next_pool;
				for (int k = 0; k < 0x80; k++, q += 12, r += 12)
				{
					if (q[3] == 0 || r[3] == 0) continue;
					for (int a = 0; a < 3; a++) q[a] = (int16_t)lerp_i(q[a], r[a], num, den);
					q[7] = (int16_t)lerp_i(q[7], r[7], num, den);
					q[8] = lerp_angle(q[8], r[8], num, den);
					q[9] = lerp_angle(q[9], r[9], num, den);
				}
			}
			// targets: position (held on jumps), angles, draw scale
			for (int i = 0; i < n; i++)
			{
				uint8_t *rec = Rec(nd, i), *ent = Ent(nd, i);
				const uint8_t *nr = g_next_recs + 0x20 * i;
				int16_t *pos = &At<int16_t>(ent, 0x1C);
				bool cut = false;
				for (int a = 0; a < 3; a++) cut |= Jump(pos[a], g_next_pos[i][a]);
				if (cut) continue;
				for (int a = 0; a < 3; a++) pos[a] = (int16_t)lerp_i(pos[a], g_next_pos[i][a], num, den);
				for (int a = 0; a < 3; a++)
					At<int16_t>(rec, 0x14 + 2 * a) = lerp_angle(At<int16_t>(rec, 0x14 + 2 * a), At<int16_t>(nr, 0x14 + 2 * a), num, den);
				At<int16_t>(rec, 0xA) = (int16_t)lerp_i(At<int16_t>(rec, 0xA), At<int16_t>(nr, 0xA), num, den);
			}
			// the creature's root height (rising 100..124)
			At<int32_t>(nd, 0x1AC) = lerp_i(At<int32_t>(nd, 0x1AC), root1, num, den);
		}
		for (int i = 0; i < n; i++) memcpy(g_held_pos[i], Ent(nd, i) + 0x1C, 6);
		g_held_nd = nd;

		// the pose drawn this tick (tick start) -> the pose after the advance
		if (sk && g_cm.pose_ok && size == g_cm.pose_size)
		{
			PoseBlend(sk, g_cm.pose, num, den);
			BuildBoneMatricesFromPose(nd + 0x1B4);
		}

		memcpy(g_hfr, g_fr, sizeof(g_hfr));
		Pass p = { nd, g_hfr, c, num, den, true, interp };
		uint32_t v;
		if ((v = (uint32_t)(c - 10)) < 0x1E)
		{
			if (v >= 8) DrawA(p, (int32_t)v);
			if (v > 0xE) DrawB(p, (int32_t)v - 0xE);
		}
		if ((v = (uint32_t)(c - 0x28)) < 0x3C) DrawC(p, (int32_t)v);
		if ((v = (uint32_t)(c - 0x64)) < 0x36) DrawD(p, (int32_t)v);
		if ((v = (uint32_t)(c - 0x90)) < 0x1E) DrawE(p);
		if ((v = (uint32_t)(c - 0xB8)) < 0x28)
			DrawBonePrim(p, 0x1C, (int32_t)(v * v), (int32_t)((v + 1) * (v + 1)), (uint32_t)(c + 1 - 0xB8) < 0x28, -128, 0x13EF438);
		if ((v = (uint32_t)(c - 0xE0)) < 0x1E) DrawG(p, (int32_t)v);
		if ((v = (uint32_t)(c - 0xFE)) < 0x1E)
		{
			bool nx = interp && (uint32_t)(c + 1 - 0xFE) < 0x1E && !Pause();
			int32_t s = shl32(v, 5);
			DrawBonePrim(p, 0x2C, s, shl32(v + 1, 5), nx, 0x40, 0x13EF42C);
			DrawRing(nx ? lerp_i(s, shl32(v + 1, 5), num, den) : s, false, 0);
		}
		if ((v = (uint32_t)(c - 0x11C)) < 0x24)
		{
			bool nx = interp && (uint32_t)(c + 1 - 0x11C) < 0x24 && !Pause();
			int32_t s = shl32(v, 5), f = (int32_t)((uint32_t)shl32(v, 12) >> 4);
			DrawBonePrim(p, 0x1C, s, shl32(v + 1, 5), nx, 0x80, 0x13EF42C);
			if (nx)
			{
				s = lerp_i(s, shl32(v + 1, 5), num, den);
				f = lerp_i(f, (int32_t)((uint32_t)shl32(v + 1, 12) >> 4), num, den);
			}
			DrawRing(s, true, f);
		}
		if ((v = (uint32_t)(c - 0x14F)) < 0x2A)
		{
			w::BonePos(E(nd), 0x13B7BB0, g_hfr + 0x1C);
			DrawJ(p, (int32_t)v);
		}
		if ((v = (uint32_t)(c - 0x179)) < 0x28) DrawK(p, (int32_t)v);
		w::PartDraw(nd);
		if (c >= 0xE0) DrawTargets(p);
		if (CreatureDrawn(c))
			FrameCursor() = w::DrawCreature(E(nd), 0x2000, (uint32_t)(MB() + 0x31D4), FrameCursor(), var<uint32_t>(0x1D969A8));

		if (sk) memcpy(sk, g_skel_save, size);
		memcpy(nd, g_node_save, sizeof(g_node_save));
		memcpy((void *)ENT_BASE, g_ent_save, ENT_SIZE);
	}

	// held target position of a record (falling / landed target prims)
	static const int16_t *HeldTargetPos(uint8_t *rec)
	{
		if (g_held_nd)
		{
			int32_t i = (int32_t)(rec - (g_held_nd + 0x14));
			if (i >= 0 && (i & 0x1F) == 0 && (i >> 5) < 10 && (i >> 5) < Count()) return g_held_pos[i >> 5];
		}
		return &At<int16_t>(At<uint8_t *>(rec, 0), 0x1C);
	}

	static void TargetPrimHeld(uint8_t *nd, int num, int den)
	{
		uint8_t *rec = At<uint8_t *>(nd, 0xC);
		static uint8_t ctx[0x2C];
		TargetPrimCtx(ctx, rec, HeldTargetPos(rec));
		prim::play_held((prim::Layout *)(nd + 0x10), (prim::Callback)CB_Prim, (int)ctx, num, den);
	}

	// swirl: drawn at (angle - step, frame - 1) this tick; the angle turns on in between
	static void SwirlHeld(uint8_t *nd, int num, int den)
	{
		int16_t f = At<int16_t>(nd, 0xC), a = At<int16_t>(nd, 0x10), d = At<int16_t>(nd, 0x12);
		if (Pause()) { SwirlDraw(nd, a, f); return; }
		if (f <= 0) return;
		SwirlDraw(nd, (int16_t)(a - d + d * num / den), (int16_t)(f - 1));
	}

	static void RiseHeld(uint8_t *nd, int num, int den)
	{
		int16_t f = At<int16_t>(nd, 0x12);
		const int16_t *pos = &At<int16_t>(nd, 0xC);
		if (Pause()) { RiseDraw(nd, pos, f); return; }
		if (f <= 0) return;
		int16_t h[3];
		for (int k = 0; k < 3; k++)
		{
			int32_t vk = shl32((int8_t)nd[0x14 + k], 4);
			h[k] = (int16_t)(pos[k] - vk + vk * num / den);
		}
		RiseDraw(nd, h, (int16_t)(f - 1));
	}

	// trail: spin/scale between the drawn and the current values, brightness half way to the
	// next fade step (drawn on a copy, with the pause flag up so the copy is not faded again)
	static uint8_t g_trail_copy[0x1E * 0x24];
	static void TrailHeld(uint8_t *nd, int num, int den)
	{
		const TrailMemo *m = g_trail_memo.get(nd);
		if (!m) return;
		int32_t cnt = At<int32_t>(nd, 0x20);
		if (cnt < 0 || cnt > 0x1E) return;
		memcpy(g_trail_copy, nd + 0x24, cnt * 0x24);
		if (!Pause())
			for (int32_t k = 0; k < cnt; k++)
			{
				uint8_t *e = g_trail_copy + 0x24 * k;
				int32_t b0 = e[0x20];
				int8_t d = (int8_t)(e[0x21] - 4);
				int32_t b1 = b0 + (d >> 3);
				if (b1 < 0) b1 = 0;
				e[0x20] = (uint8_t)lerp_i(b0, (uint8_t)b1, num, den);
			}
		int16_t spin = Pause() ? At<int16_t>(nd, 0x16) : lerp_angle(m->spin, At<int16_t>(nd, 0x16), num, den);
		int16_t scale = Pause() ? At<int16_t>(nd, 0x1A) : (int16_t)lerp_i(m->scale, At<int16_t>(nd, 0x1A), num, den);
		uint8_t mat[0x20];
		TrailMatrix(mat, spin, scale);
		uint32_t pause = Pause();
		Pause() = 1;
		int32_t c2 = cnt;
		PacketCursor() = w::TrailDraw(&c2, g_trail_copy, -4, RenderOT(), 2, PacketCursor());
		Pause() = pause;
	}

	static uint8_t g_held_packets[0x100000];
	static uint8_t g_scratch_save[0x4000];

	// mirrors the master's queue order; packets go to a private buffer, the vertex scratch at
	// modelBuffer + 0x31D4 is put back
	static void HeldFrame(int num, int den)
	{
		uint32_t cursor = PacketCursor(), frame_cursor = FrameCursor();
		memcpy(g_scratch_save, MB() + 0x31D4, sizeof(g_scratch_save));
		PacketCursor() = (uint32_t)g_held_packets;
		FrameCursor() = (uint32_t)g_held_packets + 0x40000;
		CreatureHeld(num, den);
		for (TaskNode *t = QFlip().head; t; t = t->next)
		{
			if ((uint32_t)t->func == ORIG_SwirlFlipTask) SwirlHeld((uint8_t *)t, num, den);
			else if ((uint32_t)t->func == ORIG_RiseFlipTask) RiseHeld((uint8_t *)t, num, den);
		}
		for (TaskNode *t = QTrail().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TrailTask) TrailHeld((uint8_t *)t, num, den);
		for (TaskNode *t = QFall().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_FallPrimTask) TargetPrimHeld((uint8_t *)t, num, den);
		for (TaskNode *t = QLand().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_LandPrimTask) TargetPrimHeld((uint8_t *)t, num, den);
		g_held_nd = nullptr;
		memcpy(MB() + 0x31D4, g_scratch_save, sizeof(g_scratch_save));
		PacketCursor() = cursor;
		FrameCursor() = frame_cursor;
	}
}

	// ------------------------------------------------------------------
	// Held-frame camera: the timeline's camera code for the next tick, run on the real globals
	// with the targets moved to their next positions (the look-at follows target 0 from 376
	// on), then everything is put back (camera, shake, roll, node, entities, GTE registers,
	// Field_Alloc pointer); cuts (absolute set-ups, jumps > 1500) hold.
	// ------------------------------------------------------------------
	bool mag291_held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		using namespace p291;
		uint8_t *nd = Creature();
		if (!nd || !HeldReady() || g_cm.tick != g_real_tick || g_cm.nd != nd) return false;
		int32_t c1 = At<int16_t>(nd, 0xC);
		int16_t e0[3], a0[3], e1[3], a1[3];
		for (int i = 0; i < 3; i++) { e0[i] = CamW(i); a0[i] = CamL(i); }
		bool cut = true;
		if (c1 == g_cm.c + 1)
		{
			static uint8_t cam_save[0x20], node_save[0x1030], ent_save[ENT_SIZE], gte_d[0x70], gte_c[0xD0];
			int16_t shake = ShakeY(), roll = Roll(), w38 = W1D8E038();
			uint32_t alloc = var<uint32_t>(0x1D999C4);
			memcpy(cam_save, (void *)0xB8B7F0, sizeof(cam_save));
			memcpy(node_save, nd, sizeof(node_save));
			memcpy(ent_save, (void *)ENT_BASE, ENT_SIZE);
			memcpy(gte_d, (void *)0x1CA8A10, sizeof(gte_d));
			memcpy(gte_c, (void *)0x1CA9230, sizeof(gte_c));
			int16_t hand[3];
			bool hand_ok = NextHand(nd, hand);
			PredictNext(nd, c1, hand_ok ? hand : nullptr, false);
			cut = CamStep(nd, c1, false);
			for (int i = 0; i < 3; i++) { e1[i] = CamW(i); a1[i] = CamL(i); }
			memcpy((void *)0xB8B7F0, cam_save, sizeof(cam_save));
			memcpy(nd, node_save, sizeof(node_save));
			memcpy((void *)ENT_BASE, ent_save, ENT_SIZE);
			memcpy((void *)0x1CA8A10, gte_d, sizeof(gte_d));
			memcpy((void *)0x1CA9230, gte_c, sizeof(gte_c));
			var<uint32_t>(0x1D999C4) = alloc;
			ShakeY() = shake;
			Roll() = roll;
			W1D8E038() = w38;
			for (int i = 0; i < 3; i++) cut |= Jump(e0[i], e1[i]) || Jump(a0[i], a1[i]);
		}
		for (int i = 0; i < 3; i++)
		{
			world[i] = cut ? e0[i] : (int16_t)lerp_i(e0[i], e1[i], num, den);
			lookat[i] = cut ? a0[i] : (int16_t)lerp_i(a0[i], a1[i], num, den);
		}
		return true;
	}

	void register_mag291_pandemona()
	{
		register_port(p291::ORIG_SequenceTask, (void *)p291::SequenceTask, "P291 SequenceTask", 291);
		register_port(p291::ORIG_TimelineTask, (void *)p291::TimelineTask, "P291 TimelineTask", 291, true);
		register_port(p291::ORIG_LandPrimTask, (void *)p291::TargetPrimTask, "P291 LandPrimTask", 291, true);
		register_port(p291::ORIG_FallPrimTask, (void *)p291::TargetPrimTask, "P291 FallPrimTask", 291, true);
		register_port(p291::ORIG_SwirlFlipTask, (void *)p291::SwirlFlipTask, "P291 SwirlFlipTask", 291, true);
		register_port(p291::ORIG_RiseFlipTask, (void *)p291::RiseFlipTask, "P291 RiseFlipTask", 291, true);
		register_port(p291::ORIG_TrailTask, (void *)p291::TrailTask, "P291 TrailTask", 291, true);
		register_module_held(291, p291::HeldReady, p291::HeldFrame);
		register_module_camera(291, mag291_held_camera);
	}
}
