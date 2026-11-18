/****************************************************************************/
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

#include "mag_common.h"

namespace ff8fx
{
namespace fp315
{
	using namespace eng;
	using namespace magc;

	template<typename T> static inline T &F(uint32_t base, int32_t off) { return *(T *)(base + off); }
	static inline int16_t &S16(uint32_t p, int32_t o) { return F<int16_t>(p, o); }
	static inline uint16_t &U16(uint32_t p, int32_t o) { return F<uint16_t>(p, o); }
	static inline int32_t &S32(uint32_t p, int32_t o) { return F<int32_t>(p, o); }
	static inline uint32_t &U32(uint32_t p, int32_t o) { return F<uint32_t>(p, o); }
	static inline uint8_t &U8(uint32_t p, int32_t o) { return F<uint8_t>(p, o); }

	inline TaskQueue *RootQueue() { return (TaskQueue *)0x2239558; }
	inline TaskQueue *FxQueue() { return (TaskQueue *)0x223A828; }
	inline CastContext *&Ctx() { return var<CastContext *>(0x223B648); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x223B64C); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x223B654); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x223B658); }
	inline uint32_t &Voice() { return var<uint32_t>(0x223B65C); }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }
	inline uint32_t &VertexBuffer() { return var<uint32_t>(0x1D98B3C); }
	inline uint32_t OT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline uint32_t Ent(int32_t slot) { return 0x1D972C0 + (uint32_t)(slot * 0x9C); }

	static const uint32_t POOL = 0x2239568, POOL_END = 0x223A828;

	static const uint32_t ORIG_Master = 0x598E70;
	static const uint32_t ORIG_Drops = 0x599470;
	static const uint32_t ORIG_Bubbles = 0x599860;
	static const uint32_t ORIG_Bubbles2 = 0x599A90;
	static const uint32_t ORIG_Melt = 0x599CC0;
	static const uint32_t ORIG_Rise = 0x59A3E0;
	static const uint32_t ORIG_Splash = 0x59A500;
	static const uint32_t ORIG_Puddle = 0x59A670;
	static const uint32_t ORIG_Sparks = 0x59A7F0;
	static const uint32_t ORIG_Column = 0x59AAF0;
	static const uint32_t ORIG_Ring = 0x59AC70;
	static const uint32_t ORIG_CasterTint = 0x59ADB0;
	static const uint32_t ORIG_NextTint = 0x59AEF0;
	static const uint32_t ORIG_Root = 0x59B000;
	static const uint32_t ORIG_TexRestore = 0x59B070;

	static const uint32_t SEQ_Drop = 0xCD2C44, SEQ_Bubble = 0xCD2DF0, SEQ_Spark = 0xCD2F68;
	static const uint32_t MODEL_Splash = 0xCD3F08, MODEL_Puddle = 0xCD47B0, MODEL_Column = 0xCD3A48, MODEL_Ring = 0xCD3198;
	static const uint32_t DROP_OFFSETS = 0xCD58C8;
	static const uint32_t DROP_DIRS = 0xCD58E0;
	static const uint32_t TINT_Caster = 0xCD5910;
	static const uint32_t TINT_Next = 0xCD5940;
	static const void *const SOUND_Voice = (const void *)0xCD57A0;
	static const void *const SOUND_Start = (const void *)0xCD58BC;
	static const void *const SOUND_Hit = (const void *)0xCD58C0;

	inline int32_t GetEffectSpawnPosition(uint32_t entity, int32_t bone, int32_t flag, uint32_t out) { return fn<int32_t (__cdecl *)(uint32_t, int32_t, int32_t, uint32_t)>(0x502170)(entity, bone, flag, out); }
	inline uint32_t VoiceBusy(uint32_t v) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x4A2900)(v); }
	inline void VoiceRelease(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(v); }
	inline void SetMonsterPresence(int32_t slot) { fn<void (__cdecl *)(int32_t)>(0x487670)(slot); }
	inline void MatrixMulVector(uint32_t m, uint32_t in, uint32_t out) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56C4F0)(m, in, out); }
	inline int32_t NormalizeSVector(uint32_t in, uint32_t out) { return fn<int32_t (__cdecl *)(uint32_t, uint32_t)>(0x56BDE0)(in, out); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline uint32_t DrawShadow(uint32_t entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	inline uint32_t PuddleRender(uint32_t h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, int32_t, uint32_t)>(0x64E080)(h, ot, mode, cursor); }
	inline uint32_t ColumnRender(uint32_t h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, int32_t, uint32_t)>(0x64F0A0)(h, ot, mode, cursor); }
	inline void GteSetLightMatrix(uint32_t m) { fn<void (__cdecl *)(uint32_t)>(0x45DE50)(m); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }
	inline void GteMVMVA_RotV0Tr2() { fn<void (__cdecl *)()>(0x460830)(); }
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void GteReadSXY012Split(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteStoreOTZ(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
	inline void GteSetFarColour(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); }
	inline void GteLoadRGBC(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(c); }
	inline void GteSetBkFromTranslation(uint32_t m) { fn<void (__cdecl *)(uint32_t)>(0x64DDA0)(m); }
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }
	inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }
	inline void GteSetTransFromVec32(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x64DD70)(v); }
	inline void GteLoadV0Words(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x64DEE0)(v); }
	inline void GteStoreIRVertex(uint32_t out) { fn<void (__cdecl *)(uint32_t)>(0x64DF10)(out); }

	struct Cue { uint32_t flags; int32_t fade; };

	static uint32_t PoolFree()
	{
		uint32_t e = POOL;
		int32_t i = 0;
		while (S32(e, 0) != 0)
		{
			e += 0x18;
			i++;
			if (e >= POOL_END) return 0;
		}
		return i < 200 ? e : 0;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag315_fake_president_death_held.h"
#endif

namespace ff8fx
{
namespace fp315
{
	static void DropDraw(uint32_t w, uint32_t h, uint32_t e)
	{
		GteLoadV0((const void *)(e + 8));
		GteMVMVA_LightV0Bk();
		GteSetRotScale(S16(e, 6));
		S16(h, 4) = S16(e, 4);
		GteReadMAC123((int32_t *)(w + 0x38));
		NormalizeVector((const int32_t *)(w + 0x38), (int32_t *)(w + 0x28));
		const int32_t push = -(S16(e, 6) >> 4);
		S32(w, 0x38) = S32(w, 0x38) + (mul32(S32(w, 0x28), push) >> 12);
		S32(w, 0x3C) = S32(w, 0x3C) + (mul32(S32(w, 0x2C), push) >> 12);
		S32(w, 0x40) = S32(w, 0x40) + (mul32(S32(w, 0x30), push) >> 12);
		GteSetTransFromVec32(w + 0x38);
		PacketCursor() = InitEffectSequenceFromData((void *)h, OT44(), 2, PacketCursor());
	}

	static void DropMove(uint32_t e)
	{
		S16(e, 4) = (int16_t)(S16(e, 4) + 1);
		const int16_t s = S16(e, 6);
		S16(e, 6) = (int16_t)(s + (int16_t)(s >> 4));
		S16(e, 8) = (int16_t)(S16(e, 8) + S16(e, 0x10));
		S16(e, 0xA) = (int16_t)(S16(e, 0xA) + (int16_t)(S16(e, 0x16) + S16(e, 0x12)));
		S16(e, 0xC) = (int16_t)(S16(e, 0xC) + S16(e, 0x14));
		for (int k = 0x10; k <= 0x14; k += 2)
		{
			const int16_t v = S16(e, k);
			S16(e, k) = (int16_t)(v - (int16_t)(v >> 3));
		}
	}

	static void BubbleDraw(uint32_t h, uint32_t e)
	{
		GteLoadV0((const void *)(e + 8));
		GteMVMVA_LightV0Bk();
		GteSetRotScale(S16(e, 6));
		S16(h, 4) = S16(e, 4);
		GteTransFromMAC();
		PacketCursor() = InitEffectSequenceFromData((void *)h, OT44(), 2, PacketCursor());
	}

	static void SparkDraw(uint32_t w, uint32_t h, uint32_t e)
	{
		const int32_t angle = S16(e, 8);
		const int32_t radius = S16(e, 0xC);
		const int32_t s = mul32(ComputeSin(angle), radius);
		S16(w, 2) = S16(e, 0xA);
		S16(w, 0) = (int16_t)((int16_t)(s >> 12) + S16(w, 8));
		const int32_t c = mul32(ComputeCos(angle), radius);
		S16(w, 4) = (int16_t)((int16_t)(c >> 12) + S16(w, 0xC));
		GteLoadV0((const void *)w);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(S16(e, 6));
		S16(h, 4) = S16(e, 4);
		GteReadMAC123((int32_t *)(w + 0x40));
		NormalizeVector((const int32_t *)(w + 0x40), (int32_t *)(w + 0x30));
		const int32_t push = -(S16(e, 6) >> 3);
		S32(w, 0x40) = S32(w, 0x40) + (mul32(S32(w, 0x30), push) >> 12);
		S32(w, 0x44) = S32(w, 0x44) + (mul32(S32(w, 0x34), push) >> 12);
		S32(w, 0x48) = S32(w, 0x48) + (mul32(S32(w, 0x38), push) >> 12);
		GteSetTransFromVec32(w + 0x40);
		PacketCursor() = InitEffectSequenceFromData((void *)h, OT44(), 2, PacketCursor());
	}

	static void DropSpawn(uint32_t n, uint32_t w)
	{
		const int32_t bone = S16(n, 0x18);
		U32(w, 0) = U32(DROP_OFFSETS + (uint32_t)(bone * 8), 0);
		U32(w, 4) = U32(DROP_OFFSETS + (uint32_t)(bone * 8), 4);
		MatrixMulVector(Ent(CasterSlot()) + 0x40, w, w);
		S16(w, 2) = (int16_t)(S16(w, 2) + S16(n, 0x12));
		S16(w, 0) = (int16_t)(S16(w, 0) + S16(n, 0x10));
		S16(w, 4) = (int16_t)(S16(w, 4) + S16(n, 0x14));
		ff8fx::copy<(16)>((void *)(w + 0x28), (const void *)(DROP_DIRS + (uint32_t)(bone * 16)));
		TransformVectorBy3x3Matrix((const Mat4x3 *)(Ent(CasterSlot()) + 0x40), (const int32_t *)(w + 0x28), (int32_t *)(w + 0x28));
		for (int k = 0; k < 2; k++)
		{
			const uint32_t e = PoolFree();
			if (!e) break;
			S32(e, 0) = S16(n, 0x1A);
			S16(e, 4) = 0;
			S16(e, 6) = (int16_t)(CrtRand() % 0x280 + 0x300);
			U32(e, 8) = U32(w, 0);
			U32(e, 0xC) = U32(w, 4);
			S16(e, 8) = (int16_t)(S16(e, 8) + (int16_t)(CrtRand() % 16 - 8));
			S16(e, 0xA) = (int16_t)(S16(e, 0xA) + (int16_t)(CrtRand() % 16 - 8));
			S16(e, 0xC) = (int16_t)(S16(e, 0xC) + (int16_t)(CrtRand() % 16 - 8));
			const int32_t speed = CrtRand() % 0x1C2 + 0x8C;
			S16(e, 0x10) = (int16_t)(mul32(S32(w, 0x28), speed) >> 12);
			S16(e, 0x12) = (int16_t)(mul32(S32(w, 0x2C), speed) >> 12);
			S16(e, 0x14) = (int16_t)(mul32(S32(w, 0x30), speed) >> 12);
			S16(e, 0x16) = (int16_t)(CrtRand() % 8 + 0xC);
		}
	}

	static uint32_t __cdecl DropsTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const int16_t delay = S16(n, 0xE);
		if (delay > 0)
		{
			if (DrawOnly()) return 0;
			S16(n, 0xE) = (int16_t)(delay - 1);
			return 0;
		}
		const uint32_t w = (uint32_t)FieldAlloc(0x48);
		if (!DrawOnly() && S16(n, 0xC) <= 0x14) DropSpawn(n, w);
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		ff8fx::copy<(0x20)>((void *)(w + 8), (const void *)&Camera());
		U32(h, 0) = SEQ_Drop;
		S16(h, 0x24) = 0xA;
		S32(h, 0xC) = 0x1000;
		S32(h, 0x10) = 0xA00;
		GteSetLightMatrix(w + 8);
		GteSetBkFromTranslation(w + 8);
		int32_t count = 0;
		const int32_t mask = S16(n, 0x1A);
		FX_HELD(held_note_parts(ORIG_Drops, n, w, h, mask);)
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
		{
			if (!(S32(e, 0) & mask)) continue;
			DropDraw(w, h, e);
			if (DrawOnly()) continue;
			if (S16(h, 0x28) < 0)
			{
				S32(e, 0) = 0;
				continue;
			}
			DropMove(e);
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return count ? 0 : TASK_END;
	}

	static void BubbleSpawn(uint32_t n)
	{
		for (int k = 0; k < 2; k++)
		{
			const uint32_t e = PoolFree();
			if (!e) break;
			S32(e, 0) = 0x10;
			S16(e, 4) = 0;
			S16(e, 6) = (int16_t)(CrtRand() % 0x480 + 0x200);
			S16(e, 8) = (int16_t)(CrtRand() % 0x320 + S16(n, 0x10) - 0x190);
			S16(e, 0xA) = (int16_t)(-0x96 - CrtRand() % 0x3B6);
			S16(e, 0xC) = (int16_t)(CrtRand() % 0x320 + S16(n, 0x14) - 0x190);
			S16(e, 0x12) = (int16_t)(-6 - CrtRand() % 0x12);
		}
	}

	static void Bubble2Spawn(uint32_t n)
	{
		for (int k = 0; k < 3; k++)
		{
			const uint32_t e = PoolFree();
			if (!e) break;
			S32(e, 0) = 0x40;
			S16(e, 4) = 0;
			const int32_t r = CrtRand() % 0x500;
			S16(e, 6) = (int16_t)(r + (int16_t)(S16(n, 0xC) + 0x30) * 8);
			S16(e, 8) = (int16_t)(CrtRand() % 0x3E8 + S16(n, 0x10) - 0x1F4);
			S16(e, 0xA) = (int16_t)(0x96 - CrtRand() % 0x12C);
			S16(e, 0xC) = (int16_t)(CrtRand() % 0x3E8 + S16(n, 0x14) - 0x1F4);
			S16(e, 0x12) = (int16_t)(-5 - CrtRand() % 0x19);
		}
	}

	static uint32_t BubbleTask(uint32_t fn, uint32_t n, int32_t owner, int32_t last, void (*spawn)(uint32_t))
	{
		const uint32_t w = (uint32_t)FieldAlloc(0x48);
		if (!DrawOnly() && S16(n, 0xC) <= last) spawn(n);
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		S16(h, 0x24) = 0;
		ff8fx::copy<(0x20)>((void *)(w + 8), (const void *)&Camera());
		U32(h, 0) = SEQ_Bubble;
		GteSetLightMatrix(w + 8);
		GteSetBkFromTranslation(w + 8);
		int32_t count = 0;
		FX_HELD(held_note_parts(fn, n, w, h, owner);)
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
		{
			if (!(U8(e, 0) & owner)) continue;
			BubbleDraw(h, e);
			if (DrawOnly()) continue;
			if (S16(h, 0x28) < 0)
			{
				S32(e, 0) = 0;
				continue;
			}
			S16(e, 4) = (int16_t)(S16(e, 4) + 1);
			S16(e, 0xA) = (int16_t)(S16(e, 0xA) + S16(e, 0x12));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return count ? 0 : TASK_END;
	}

	static uint32_t __cdecl BubblesTask(TaskNode *nd) { return BubbleTask(ORIG_Bubbles, (uint32_t)nd, 0x10, 0x28, BubbleSpawn); }
	static uint32_t __cdecl Bubbles2Task(TaskNode *nd) { return BubbleTask(ORIG_Bubbles2, (uint32_t)nd, 0x40, 0x5F, Bubble2Spawn); }

	static void SparkSpawn()
	{
		for (int k = 0; k < 4; k++)
		{
			const uint32_t e = PoolFree();
			if (!e) break;
			S32(e, 0) = 0x20;
			S16(e, 4) = 0;
			S16(e, 6) = (int16_t)(CrtRand() % 0x300 + 0x200);
			S16(e, 8) = (int16_t)(CrtRand() % 0x1000);
			S16(e, 0xA) = (int16_t)(-0x32 - CrtRand() % 0x96);
			S16(e, 0xC) = (int16_t)(CrtRand() % 0xFA + 0x64);
			S16(e, 0x10) = (int16_t)(CrtRand() % 0x41 + 0xF);
			S16(e, 0x12) = (int16_t)(-5 - CrtRand() % 0x32);
			S16(e, 0x14) = (int16_t)(CrtRand() % 0x14 + 5);
		}
	}

	static uint32_t __cdecl SparksTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const uint32_t w = (uint32_t)FieldAlloc(0x50);
		if (!DrawOnly() && S16(n, 0xC) <= 0x3C) SparkSpawn();
		const uint32_t h = (uint32_t)FieldAlloc(0xB4);
		S16(h, 0x24) = 0;
		ff8fx::copy<(0x20)>((void *)(w + 0x10), (const void *)&Camera());
		U32(h, 0) = SEQ_Spark;
		GteSetLightMatrix(w + 0x10);
		GteSetBkFromTranslation(w + 0x10);
		U32(w, 8) = U32(n, 0x10);
		U32(w, 0xC) = U32(n, 0x14);
		int32_t count = 0;
		FX_HELD(held_note_parts(ORIG_Sparks, n, w, h, 0x20);)
		for (uint32_t e = POOL; e < POOL_END; e += 0x18)
		{
			if (!(U8(e, 0) & 0x20)) continue;
			SparkDraw(w, h, e);
			if (DrawOnly()) continue;
			if (S16(h, 0x28) < 0)
			{
				S32(e, 0) = 0;
				continue;
			}
			S16(e, 4) = (int16_t)(S16(e, 4) + 1);
			S16(e, 8) = (int16_t)(S16(e, 8) + S16(e, 0x10));
			S16(e, 0xA) = (int16_t)(S16(e, 0xA) + S16(e, 0x12));
			S16(e, 0xC) = (int16_t)(S16(e, 0xC) + S16(e, 0x14));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x50);
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return count ? 0 : TASK_END;
	}

	static uint32_t MeltPrims(uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint32_t pkt = cursor;
		const uint32_t verts = U32(hdr, 4);
		uint32_t rec = U32(hdr, 0);
		for (uint32_t i = 0; i < U16(hdr, 8); i++, rec += 0x10)
		{
			const uint32_t v0 = verts + (U16(rec, 0) & 0xFFF) * 8, v1 = verts + (U16(rec, 2) & 0xFFF) * 8, v2 = verts + (U16(rec, 4) & 0xFFF) * 8;
			U32(hdr, 0x54) = U32(v0, 0);
			U32(hdr, 0x58) = U32(v0, 4);
			U32(hdr, 0x5C) = U32(v1, 0);
			U32(hdr, 0x60) = U32(v1, 4);
			U32(hdr, 0x64) = U32(v2, 0);
			U32(hdr, 0x68) = U32(v2, 4);
			GteLoadV012(hdr + 0x54, hdr + 0x5C, hdr + 0x64);
			GteRTPT();
			const uint32_t w = U32(rec, 0xC);
			uint32_t code = (w & 0x2000000) | U32(hdr, 0x1C) | 0x24000000;
			uint32_t tpage = w & 0x1FFFFFF;
			U32(pkt, 0) = 0x7000000;
			U32(pkt, 4) = code;
			U32(pkt, 0xC) = U32(rec, 8);
			U32(pkt, 0x14) = tpage;
			if (S16(hdr, 0x56) == 0 || S16(hdr, 0x5E) == 0 || S16(hdr, 0x66) == 0)
			{
				U32(pkt, 4) = code | 0x2000000;
				U32(pkt, 0x14) = tpage + 0x200000;
			}
			GteReadFLAG(hdr + 0x50);
			if (U32(hdr, 0x50) & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(hdr + 0x44);
			if (S32(hdr, 0x44) <= 0) continue;
			GteReadSXY012Split(pkt + 8, pkt + 0x10, pkt + 0x18);
			GteAVSZ3();
			S32(pkt, 0x1C) = S32(rec, 4) >> 16;
			GteStoreOTZ(hdr + 0x4C);
			InsertPrimAutoDepth(ot + (uint32_t)shl32(S32(hdr, 0x4C) >> (shift & 31), 2), (void *)pkt);
			pkt += 0x20;
		}
		if (U16(hdr, 0xA) != 0)
		{
			rec += 4;
			for (uint32_t i = 0; i < U16(hdr, 0xA); i++, rec += 0x14)
			{
				const uint32_t v0 = verts + (U16(rec, -4) & 0xFFF) * 8, v1 = verts + (U16(rec, -2) & 0xFFF) * 8, v2 = verts + (U16(rec, 0) & 0xFFF) * 8;
				U32(hdr, 0x54) = U32(v0, 0);
				U32(hdr, 0x58) = U32(v0, 4);
				U32(hdr, 0x5C) = U32(v1, 0);
				U32(hdr, 0x60) = U32(v1, 4);
				U32(hdr, 0x64) = U32(v2, 0);
				U32(hdr, 0x68) = U32(v2, 4);
				GteLoadV012(hdr + 0x54, hdr + 0x5C, hdr + 0x64);
				GteRTPT();
				const uint32_t v3 = verts + (U16(rec, 2) & 0xFFF) * 8;
				U32(pkt, 0) = 0x9000000;
				U32(hdr, 0x6C) = U32(v3, 0);
				U32(hdr, 0x70) = U32(v3, 4);
				const uint32_t w = U32(rec, 8);
				uint32_t tpage = w & 0x1FFFFFF;
				U32(pkt, 0x14) = tpage;
				uint32_t code = (w & 0x2000000) | U32(hdr, 0x1C) | 0x2C000000;
				U32(pkt, 4) = code;
				U32(pkt, 0xC) = U32(rec, 4);
				if (S16(hdr, 0x56) == 0 || S16(hdr, 0x5E) == 0 || S16(hdr, 0x66) == 0 || S16(hdr, 0x6E) == 0)
				{
					U32(pkt, 4) = code | 0x2000000;
					U32(pkt, 0x14) = tpage + 0x200000;
				}
				GteReadFLAG(hdr + 0x50);
				if (U32(hdr, 0x50) & 0x60000) continue;
				GteNCLIP();
				GteReadMAC0(hdr + 0x44);
				if (S32(hdr, 0x44) <= 0) continue;
				GteReadSXY012Split(pkt + 8, pkt + 0x10, pkt + 0x18);
				GteLoadV0((const void *)(hdr + 0x6C));
				GteRTPS();
				const uint32_t w2 = U32(rec, 0xC);
				U32(pkt, 0x1C) = w2;
				S32(pkt, 0x24) = (int32_t)w2 >> 16;
				GteReadSXY2((void *)(pkt + 0x20));
				GteAVSZ4();
				GteStoreOTZ(hdr + 0x4C);
				InsertPrimAutoDepth(ot + (uint32_t)shl32(S32(hdr, 0x4C) >> (shift & 31), 2), (void *)pkt);
				pkt += 0x28;
			}
		}
		return pkt;
	}

	static uint32_t MeltModel(uint32_t model, uint32_t hdr, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t bones = U32(model, 0) + 0x10;
		uint32_t table = U32(model, 4);
		const int32_t count = S32(table, 0);
		table += 4;
		for (int32_t i = 0; i < count; i++)
		{
			uint32_t obj = U32(model, 4) + U32(table, 0);
			table += 4;
			if (!(U32(hdr, 0x20) & (1u << (i & 31)))) continue;
			int32_t groups = S16(obj, 0);
			uint32_t out = U32(hdr, 4);
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
						const int16_t y = S16(out, 2);
						const int16_t floor = S16(hdr, 0x76);
						if (y <= floor)
						{
							S16(out, 2) = (int16_t)(y - floor);
							continue;
						}
						S16(hdr, 0x56) = 0;
						S16(hdr, 0x54) = (int16_t)(S16(out, 0) - S16(hdr, 0x74));
						S16(hdr, 0x58) = (int16_t)(S16(out, 4) - S16(hdr, 0x78));
						const int32_t d = Sqrt(NormalizeSVector(hdr + 0x54, hdr + 0x54));
						const int32_t radius = S32(hdr, 0x34);
						if (d < radius)
						{
							const int32_t yy = S16(out, 2);
							S16(out, 2) = 0;
							int32_t push = (S32(hdr, 0x30) - yy) / S32(hdr, 0x3C);
							push = mul32(mul32(push, S32(hdr, 0x38)), S32(hdr, 0x2C)) >> 12;
							S16(out, 0) = (int16_t)(S16(out, 0) + (int16_t)(mul32(S16(hdr, 0x54), push) >> 12));
							S16(out, 4) = (int16_t)(S16(out, 4) + (int16_t)(mul32(S16(hdr, 0x58), push) >> 12));
						}
						else
						{
							S16(out, 0) = (int16_t)((int16_t)(mul32(S16(hdr, 0x54), radius) >> 12) + S16(hdr, 0x74));
							S16(out, 2) = 0;
							S16(out, 4) = (int16_t)((int16_t)(mul32(S16(hdr, 0x58), radius) >> 12) + S16(hdr, 0x78));
						}
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
			cursor = MeltPrims(hdr, ot, shift, cursor);
		}
		return cursor;
	}

	static void MeltEntity(uint32_t hdr, uint32_t entity)
	{
		const uint32_t anim = entity + 0x60;
		if (!(U8(entity, 0) & 0x20))
			FrameCursor() = DrawShadow(entity, var<uint32_t>(0x1D8E04C) + 0x4064, 0x10, FrameCursor());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)(hdr + 0x7C));
		ComputeBonesWorldMatrices((void *)anim, (void *)(entity + 0x40));
		U32(hdr, 4) = VertexBuffer();
		U32(hdr, 0x1C) = U32(entity, 0x28);
		U16(hdr, 0x14) = 0;
		U16(hdr, 0x16) = 0;
		U32(hdr, 0x20) = U32(entity, 0x7C);
		const uint8_t cue = U8(entity, 7);
		U8(hdr, 0x2A) = cue;
		U8(hdr, 0x29) = cue;
		U8(hdr, 0x28) = cue;
		U32(hdr, 0x10) = var<uint32_t>(0x1D969A8);
		U16(hdr, 0x18) = 0x140;
		U16(hdr, 0x1A) = 0xD8;
		FrameCursor() = MeltModel(U32(anim, 4), hdr, OT44(), 2, FrameCursor());
		BuildBoneMatricesFromPose((void *)anim);
		const uint32_t second = U32(entity, 0x78);
		if (second && !(U8(entity, 1) & 2))
		{
			U32(hdr, 0x20) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices((void *)second, (void *)(entity + 0x40));
			FrameCursor() = MeltModel(U32(second, 4), hdr, OT44(), 2, FrameCursor());
			BuildBoneMatricesFromPose((void *)second);
		}
	}

	static void MeltDraw(uint32_t n, int32_t strength, int32_t radius, int32_t factor)
	{
		const uint32_t entity = Ent(S16(n, 0xE));
		const uint32_t hdr = (uint32_t)FieldAlloc(0x9C);
		S32(hdr, 0x2C) = strength;
		S32(hdr, 0x30) = S16(n, 0x20);
		S32(hdr, 0x3C) = S16(n, 0x22);
		U32(hdr, 0x74) = U32(n, 0x10);
		U32(hdr, 0x78) = U32(n, 0x14);
		S32(hdr, 0x34) = radius;
		S32(hdr, 0x38) = factor;
		U8(entity, 0) |= 4;
		MeltEntity(hdr, entity);
		FieldFree(0x9C);
	}

	static int32_t CasterStrength(int32_t c) { return c < 0x46 ? shl32(c, 12) / 70 : 0x1000; }

	static uint32_t __cdecl MeltTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const int32_t slot = S16(n, 0xE);
		if (S16(n, 0xC) < 0x41)
		{
			FX_HELD(held_note_draw(ORIG_Melt, n);)
			MeltDraw(n, CasterStrength(S16(n, 0xC)), 0x2BC, 0xA);
		}
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c < 0x46) S16(n, 0x12) = (int16_t)(S16(n, 0x12) + S16(n, 0x22));
		S16(n, 0xC) = (int16_t)(c + 1);
		if (S16(n, 0xC) <= 0x41) return 0;
		U8(Ent(slot), 0) |= 4;
		return TASK_END;
	}

	static int32_t RiseStrength(int32_t c) { return c < 0x1E ? 0x1000 : shl32(0x1E - c, 12) / 80 + 0x1000; }

	static uint32_t __cdecl RiseTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const int32_t slot = S16(n, 0xE);
		if (S16(n, 0xC) < 0x6E)
		{
			const uint32_t entity = Ent(slot);
			S16(entity, 0x1C) = S16(n, 0x10);
			S16(entity, 0x20) = S16(n, 0x14);
			FX_HELD(held_note_draw(ORIG_Rise, n);)
			MeltDraw(n, RiseStrength(S16(n, 0xC)), 0x384, 0xB);
		}
		if (DrawOnly()) return 0;
		const int16_t c = S16(n, 0xC);
		if (c >= 0x1E) S16(n, 0x12) = (int16_t)(S16(n, 0x12) + S16(n, 0x22));
		S16(n, 0xC) = (int16_t)(c + 1);
		if (S16(n, 0xC) <= 0x6E) return 0;
		U16(Ent(slot), 0) &= 0xFFFB;
		return TASK_END;
	}

	static void PrimMatrix(uint32_t n, int32_t sx, int32_t sy, int32_t sz)
	{
		int16_t angles[4];
		angles[0] = 0;
		angles[1] = S16(n, 0x18);
		angles[2] = 0;
		Mat4x3 m;
		ComposeZYXRotationMatrix(angles, &m);
		m.t[0] = S16(n, 0x10);
		m.t[1] = S16(n, 0x12);
		int32_t v[3];
		v[2] = sz;
		v[0] = sx;
		m.t[2] = S16(n, 0x14);
		v[1] = sy;
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	static void ApplyCue(uint32_t h, Cue cue, int32_t off_flags)
	{
		U32(h, off_flags) = cue.flags;
		if (cue.flags == 0xF3) S32(h, 0xC) = cue.fade;
	}

	static Cue SplashCue(int32_t c)
	{
		if (c == 0) return { 0xF3u, 0x800 };
		if (c < 4) return { 0x33u, 0 };
		return { 0xF3u, (c - 4) * 341 };
	}

	static void SplashDraw(uint32_t n, Cue cue)
	{
		PrimMatrix(n, S16(n, 0x1C), S16(n, 0x20), S16(n, 0x1C));
		const uint32_t h = (uint32_t)FieldAlloc(0x58);
		U32(h, 0) = MODEL_Splash;
		U32(h, 8) = 0;
		ApplyCue(h, cue, 0x1C);
		PacketCursor() = RenderPrimModel((void *)h, OT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl SplashTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		FX_HELD(held_note_draw(ORIG_Splash, n);)
		SplashDraw(n, SplashCue(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		S16(n, 0x18) = (int16_t)(S16(n, 0x18) + S16(n, 0x1A));
		const int16_t d = S16(n, 0x1E);
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + d);
		S16(n, 0x1E) = (int16_t)(d - d / 24);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x10 ? TASK_END : 0;
	}

	static Cue PuddleCue(int32_t c)
	{
		if (c < 0x10) return { 0xF3u, shl32(0x10 - c, 8) };
		if (c < 0x44) return { 0x33u, 0 };
		return { 0xF3u, (c - 0x44) * 341 };
	}

	static void PuddleDraw(uint32_t n, int32_t uscroll, Cue cue)
	{
		PrimMatrix(n, S16(n, 0x1C), S16(n, 0x20), S16(n, 0x1C));
		const uint32_t h = (uint32_t)FieldAlloc(0x7C);
		U32(h, 0) = MODEL_Puddle;
		U32(h, 8) = 0;
		U32(h, 0x20) = 0x33;
		U16(h, 0x18) = 0;
		U16(h, 0x1A) = 0xC0;
		U16(h, 0x1C) = 0x80;
		U16(h, 0x1E) = 0x40;
		S32(h, 0x10) = uscroll;
		U32(h, 0x14) = 0;
		ApplyCue(h, cue, 0x20);
		PacketCursor() = PuddleRender(h, OT44(), 3, PacketCursor());
		FieldFree(0x7C);
	}

	static uint32_t __cdecl PuddleTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const int32_t c = S16(n, 0xC);
		FX_HELD(held_note_draw(ORIG_Puddle, n);)
		PuddleDraw(n, shl32(c, 2), PuddleCue(c));
		if (DrawOnly()) return 0;
		S16(n, 0x1C) = (int16_t)(S16(n, 0x1C) + S16(n, 0x1E));
		S16(n, 0x20) = (int16_t)(S16(n, 0x20) - S16(n, 0x22));
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x50 ? TASK_END : 0;
	}

	static Cue ColumnCue(int32_t c)
	{
		if (c < 8) return { 0x33u, 0 };
		return { 0xF3u, (c - 8) * 341 };
	}

	static void ColumnDraw(uint32_t n, Cue cue)
	{
		PrimMatrix(n, S16(n, 0x1C), S16(n, 0x20), S16(n, 0x1C));
		const uint32_t h = (uint32_t)FieldAlloc(0x60);
		U32(h, 0x58) = 0xE1000220;
		U32(h, 0x5C) = 0xE1000220;
		U32(h, 0) = MODEL_Column;
		U32(h, 8) = 0;
		ApplyCue(h, cue, 0x1C);
		PacketCursor() = ColumnRender(h, OT44(), 2, PacketCursor());
		FieldFree(0x60);
	}

	static uint32_t __cdecl ColumnTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const int16_t delay = S16(n, 0xE);
		if (delay > 0)
		{
			if (DrawOnly()) return 0;
			S16(n, 0xE) = (int16_t)(delay - 1);
			return 0;
		}
		FX_HELD(held_note_draw(ORIG_Column, n);)
		ColumnDraw(n, ColumnCue(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		const int16_t d = S16(n, 0x22);
		S16(n, 0x20) = (int16_t)(S16(n, 0x20) + d);
		S16(n, 0x22) = (int16_t)(d - d / 6);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x14 ? TASK_END : 0;
	}

	static int32_t RingFade(int32_t c)
	{
		if (c < 0x10) return shl32(0x20 - c, 7);
		if (c < 0x48) return 0x800;
		return shl32(c - 0x38, 7);
	}

	static void RingDraw(uint32_t n, int32_t fade)
	{
		PrimMatrix(n, S16(n, 0x1C), S16(n, 0x1C), S16(n, 0x1C));
		const uint32_t h = (uint32_t)FieldAlloc(0x58);
		U32(h, 0) = MODEL_Ring;
		S32(h, 0xC) = fade;
		U32(h, 8) = 0;
		U32(h, 0x1C) = 0xF3;
		PacketCursor() = RenderPrimModel((void *)h, OT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl RingTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		FX_HELD(held_note_draw(ORIG_Ring, n);)
		RingDraw(n, RingFade(S16(n, 0xC)));
		if (DrawOnly()) return 0;
		S16(n, 0x18) = (int16_t)(S16(n, 0x18) + S16(n, 0x1A));
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return S16(n, 0xC) >= 0x58 ? TASK_END : 0;
	}

	static void TintCue(uint32_t entity, uint32_t r, uint32_t g, uint32_t b, uint32_t colour, int32_t level)
	{
		GteSetFarColour(r, g, b);
		GteLoadRGBC(colour);
		GteSetIR0(level);
		GteDPCS();
		GteStoreRGB2(entity + 0x28);
	}

	static uint32_t __cdecl CasterTintTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const uint32_t entity = Ent(S16(n, 0xE));
		const int32_t c = S16(n, 0xC);
		if (c < 4)
			TintCue(entity, U8(0xB8B7D8, 0), U8(0xB8B7D8, 1), U8(0xB8B7DA, 0), TINT_Caster, shl32(4 - c, 10));
		else if (c >= 0x69)
			TintCue(entity, 0, 0, 0, TINT_Caster + (uint32_t)(S16(n, 0x1C) * 4), (c - 0x69) * 341);
		else
		{
			const int32_t k = (c - 4) % 12;
			S16(n, 0x1C) = (int16_t)k;
			U32(entity, 0x28) = U32(TINT_Caster + (uint32_t)(k * 4), 0);
		}
		U8(entity, 1) |= 8;
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) < 0x75) return 0;
		U16(entity, 0) &= 0xF7FF;
		return TASK_END;
	}

	static uint32_t __cdecl NextTintTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		const uint32_t entity = Ent(S16(n, 0xE));
		const int32_t c = S16(n, 0xC);
		if (c < 8)
			TintCue(entity, 0, 0, 0, TINT_Next, shl32(8 - c, 9));
		else if (c >= 0x6E)
			TintCue(entity, U8(0xB8B7D8, 0), U8(0xB8B7D8, 1), U8(0xB8B7DA, 0), TINT_Next + (uint32_t)(S16(n, 0x1C) * 4), shl32(c - 0x6E, 9));
		else
		{
			const int32_t k = c % 24;
			S16(n, 0x1C) = (int16_t)k;
			U32(entity, 0x28) = U32(TINT_Next + (uint32_t)(k * 4), 0);
		}
		U8(entity, 1) |= 8;
		if (DrawOnly()) return 0;
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) < 0x76) return 0;
		U16(entity, 0) &= 0xF7FF;
		return TASK_END;
	}

	static uint32_t Spawn(uint32_t task)
	{
		const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), task);
		S16(t, 0xC) = 0;
		return t;
	}

	static uint32_t __cdecl MasterTask(TaskNode *nd)
	{
		if (DrawOnly()) return 0;
		const uint32_t n = (uint32_t)nd;
		const int32_t slot = CasterSlot();
		if (S16(n, 0xC) == 0)
		{
			GetEffectSpawnPosition(Ent(slot), 0xF1, 0x800, n + 0x10);
			const uint32_t e = Ent(slot);
			U32(e, 8) |= 0x8000;
			const uint32_t model = U32(e, 0x74);
			U16(e, 0) &= 0xFFFB;
			U8(e, 0x9C) |= 4;
			S16(n, 0xE) = (int16_t)U8(model, 2);
			U8(model, 2) = U8(model, 0);
			S16(e, 0xB0) = S16(n, 0x10);
			S16(e, 0xB4) = S16(n, 0x14);
			Voice() = ClaimVoiceSlot(SOUND_Voice, 1, 0x80);
		}
		if (S16(n, 0xC) == 1)
		{
			const uint32_t t = Spawn(ORIG_CasterTint);
			S16(t, 0xE) = (int16_t)CasterSlot();
		}
		if (S16(n, 0xC) == 0x52)
		{
			const uint32_t t = Spawn(ORIG_NextTint);
			S16(t, 0xE) = (int16_t)(CasterSlot() + 1);
		}
		if (S16(n, 0xC) == 0xD)
		{
			for (int32_t i = 0; i < 3; i++)
			{
				const uint32_t t = Spawn(ORIG_Drops);
				const int32_t r = CrtRand();
				S16(t, 0x18) = (int16_t)i;
				S16(t, 0xE) = (int16_t)(r % 10 + i);
				U32(t, 0x10) = U32(n, 0x10);
				U32(t, 0x14) = U32(n, 0x14);
				S16(t, 0x1A) = (int16_t)(1 << i);
			}
		}
		if (S16(n, 0xC) == 0x1C)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Bubbles);
			U32(t, 0x10) = U32(n, 0x10);
			S16(t, 0xC) = 0;
			U32(t, 0x14) = U32(n, 0x14);
		}
		if (S16(n, 0xC) == 0x5F)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Bubbles2);
			U32(t, 0x10) = U32(n, 0x10);
			S16(t, 0xC) = 0;
			U32(t, 0x14) = U32(n, 0x14);
		}
		if (S16(n, 0xC) == 0x20)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Melt);
			const int16_t s = (int16_t)CasterSlot();
			S16(t, 0xE) = s;
			S16(t, 0x10) = S16(n, 0x10);
			S16(t, 0xC) = 0;
			S16(t, 0x14) = S16(n, 0x14);
			S16(t, 0x12) = 0;
			const int16_t start = (int16_t)(S16(Ent(s), 0x36) - 0x12C);
			S16(t, 0x20) = start;
			S16(t, 0x22) = (int16_t)(start / 70);
		}
		if (S16(n, 0xC) == 0x52)
		{
			const uint32_t t = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Rise);
			S16(t, 0xE) = (int16_t)(CasterSlot() + 1);
			S16(t, 0x10) = S16(n, 0x10);
			S16(t, 0xC) = 0;
			S16(t, 0x12) = -0x7D0;
			S16(t, 0x14) = S16(n, 0x14);
			S16(t, 0x20) = -0x7D0;
			S16(t, 0x22) = 0x19;
		}
		{
			const int16_t c = S16(n, 0xC);
			if (c >= 0x1E && c <= 0x52 && c % 4 == 1)
			{
				const uint32_t t = Spawn(ORIG_Splash);
				S16(t, 0x10) = S16(n, 0x10);
				S16(t, 0x12) = -100;
				S16(t, 0x14) = S16(n, 0x14);
				S16(t, 0x18) = (int16_t)(CrtRand() % 0x1000);
				const int32_t r = CrtRand();
				S16(t, 0x20) = 0x900;
				S16(t, 0x1E) = 0x180;
				S16(t, 0x1C) = 0x180;
				S16(t, 0x1A) = (int16_t)(r % 0x30 + 0x10);
			}
		}
		if (S16(n, 0xC) == 0x1E)
		{
			uint32_t t = Spawn(ORIG_Puddle);
			S16(t, 0x10) = S16(n, 0x10);
			S16(t, 0x12) = 0;
			S16(t, 0x14) = S16(n, 0x14);
			S16(t, 0x18) = (int16_t)(CrtRand() % 0x1000);
			S16(t, 0x1C) = 0x1000;
			S16(t, 0x1E) = 0x4F;
			S16(t, 0x20) = 0x2000;
			S16(t, 0x22) = 0x94;
			t = Spawn(ORIG_Sparks);
			S16(t, 0x10) = S16(n, 0x10);
			S16(t, 0x12) = 0;
			S16(t, 0x14) = S16(n, 0x14);
		}
		{
			const int16_t c = S16(n, 0xC);
			if (c >= 0x70 && c <= 0xB6 && c % 3 == 1)
			{
				const uint32_t t = Spawn(ORIG_Column);
				S16(t, 0xE) = (int16_t)(CrtRand() % 4);
				S16(t, 0x12) = 0;
				S16(t, 0x10) = (int16_t)(CrtRand() % 0x6A4 + S16(n, 0x10) - 0x352);
				S16(t, 0x14) = (int16_t)(CrtRand() % 0x6A4 + S16(n, 0x14) - 0x352);
				S16(t, 0x18) = (int16_t)(CrtRand() % 0x1000);
				const int32_t r = CrtRand();
				S16(t, 0x22) = 0xA00;
				S16(t, 0x20) = 0xA00;
				S16(t, 0x1C) = (int16_t)(r % 0xA00 + 0x800);
			}
		}
		if (S16(n, 0xC) == 0x7E)
		{
			const uint32_t a = Spawn(ORIG_Ring);
			S16(a, 0x10) = S16(n, 0x10);
			S16(a, 0x12) = 0;
			S16(a, 0x14) = S16(n, 0x14);
			S16(a, 0x18) = (int16_t)(CrtRand() % 0x1000);
			int32_t r = CrtRand();
			S16(a, 0x1C) = 0x1A00;
			S16(a, 0x1A) = (int16_t)(-20 - r % 0x2D);
			const uint32_t b = (uint32_t)AddTaskToQueue(FxQueue(), ORIG_Ring);
			U32(b, 0x10) = U32(a, 0x10);
			S16(b, 0xC) = 0;
			U32(b, 0x14) = U32(a, 0x14);
			S16(b, 0x18) = (int16_t)(S16(a, 0x18) + 0x200);
			r = CrtRand();
			S16(b, 0x1C) = 0x1A00;
			S16(b, 0x1A) = (int16_t)(-20 - r % 0x2D);
		}
		if (S16(n, 0xC) == 0x70) ApplyActionResultToTarget(Ctx()->actions->targets);
		if (S16(n, 0xC) == 1) BdPlaySE3D(SOUND_Start, 0, (const int16_t *)(n + 0x10));
		if (S16(n, 0xC) == 0x70) BdPlaySE3D(SOUND_Hit, 0, (const int16_t *)(n + 0x10));
		if (S16(n, 0xC) == 0x9D && VoiceBusy(Voice())) VoiceRelease(Voice());
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		if (S16(n, 0xC) <= 0xA0) return 0;
		const int32_t s = CasterSlot();
		const uint32_t e = Ent(s);
		U32(e, 8) &= 0xFFFF7FFF;
		U8(U32(e, 0x74), 2) = (uint8_t)S16(n, 0xE);
		SetMonsterPresence(s + 1);
		return TASK_END;
	}

	static uint32_t __cdecl RootTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		FX_HELD(held_note_root();)
		if (U8(n, 0xC) & 1) PacketCursor() = TexBase() + 0xC000;
		else PacketCursor() = TexBase();
		const int running = ExecuteTaskQueue(FxQueue());
		if (!running)
		{
			const uint32_t w = (uint32_t)AddTaskToQueue(RootQueue(), ORIG_TexRestore);
			S16(w, 0xC) = 0;
			S16(w, 0xE) = 0;
		}
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return running ? 0 : TASK_END;
	}

	static uint32_t __cdecl TexRestoreTask(TaskNode *nd)
	{
		const uint32_t n = (uint32_t)nd;
		if (S16(n, 0xC) == 1) TextureRestoreTask((void *)TexBase(), (void *)(n + 0xE));
		const int16_t done = S16(n, 0xE);
		S16(n, 0xC) = (int16_t)(S16(n, 0xC) + 1);
		return done ? TASK_END : 0;
	}
}

	void register_mag315_fake_president_death()
	{
		register_port(fp315::ORIG_Root, (void *)fp315::RootTask, "315 RootTask", 315);
		register_port(fp315::ORIG_TexRestore, (void *)fp315::TexRestoreTask, "315 TexRestore", 315);
		register_port(fp315::ORIG_Master, (void *)fp315::MasterTask, "315 Master", 315);
		register_port(fp315::ORIG_Drops, (void *)fp315::DropsTask, "315 Drops", 315);
		register_port(fp315::ORIG_Bubbles, (void *)fp315::BubblesTask, "315 Bubbles", 315);
		register_port(fp315::ORIG_Bubbles2, (void *)fp315::Bubbles2Task, "315 Bubbles2", 315);
		register_port(fp315::ORIG_Sparks, (void *)fp315::SparksTask, "315 Sparks", 315);
		register_port(fp315::ORIG_Melt, (void *)fp315::MeltTask, "315 Melt", 315);
		register_port(fp315::ORIG_Rise, (void *)fp315::RiseTask, "315 Rise", 315);
		register_port(fp315::ORIG_Splash, (void *)fp315::SplashTask, "315 Splash", 315);
		register_port(fp315::ORIG_Puddle, (void *)fp315::PuddleTask, "315 Puddle", 315);
		register_port(fp315::ORIG_Column, (void *)fp315::ColumnTask, "315 Column", 315);
		register_port(fp315::ORIG_Ring, (void *)fp315::RingTask, "315 Ring", 315);
		register_port(fp315::ORIG_CasterTint, (void *)fp315::CasterTintTask, "315 CasterTint", 315);
		register_port(fp315::ORIG_NextTint, (void *)fp315::NextTintTask, "315 NextTint", 315);
		FX_HELD(register_mag315_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag315_fake_president_death_held.inc"
#endif
