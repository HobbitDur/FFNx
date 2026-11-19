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

// Effect 163: Rough Divide

#include "mag_common.h"

namespace ff8fx
{
namespace rough163
{
	using namespace eng;
	using namespace magc;

	inline int32_t &CasterSlot() { return var<int32_t>(us(0x23CE274)); }
	inline int32_t &TargetSlot() { return var<int32_t>(us(0x23CA1A0)); }
	inline CastContext *&Context() { return var<CastContext *>(us(0x23CE270)); }
	inline uint32_t &TexBase() { return var<uint32_t>(us(0x23CE580)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(us(0x23CE584)); }
	inline TaskQueue *QMaster() { return (TaskQueue *)us(0x23CA1C8); }
	inline TaskQueue *QMain() { return (TaskQueue *)us(0x23CD348); }
	static const uint32_t POOL_A = 0x23CA1D8, POOL_A_END = 0x23CAB38;
	static const uint32_t POOL_B = 0x23CAB38, POOL_B_END = 0x23CBDF8;
	static const uint32_t POOL_C = 0x23CBDF8, POOL_C_END = 0x23CD0B8;
	static const uint32_t BIND_OUT = 0x23CD0B8, CASTER_MAT = 0x23CD0D8, CAM_CASTER = 0x23CD2F8;
	static const uint32_t PTS_A = 0x23CA120, PTS_B = 0x23CA160;
	static const uint32_t SPL_A = 0x23CD0F8, SPL_B = 0x23CD1F8;
	static const uint32_t HIT_POS = 0x23CD318;
	static const uint32_t HIT_DIR = 0x23CD320;
	static const uint32_t BASE_POS = 0x23CD330, BASE_DIR = 0x23CD338;
	static const uint32_t STREAK_VTX = 0x23CE168;
	static const uint32_t PILLAR_POS = 0x23CE280;
	static const uint32_t RING = 0x23CE288, RING_END = 0x23CE314;
	static const uint32_t STREAK_REC = 0x23CE318;
	static const int32_t TRAIL_LENGTH = 0x2A;

	static const uint32_t ORIG_Director = 0x5F5C80, ORIG_Trail = 0x5F5FE0, ORIG_Glow = 0x5F65E0, ORIG_Sparkle = 0x5F67A0,
		ORIG_HitSpark = 0x5F6A30, ORIG_Shard = 0x5F6CA0, ORIG_Chip = 0x5F7000, ORIG_Streak = 0x5F7390, ORIG_Flash = 0x5F78D0,
		ORIG_Burst = 0x5F7A80, ORIG_Pillar = 0x5F7E00, ORIG_Blade = 0x5F81D0, ORIG_Ring = 0x5F8540, ORIG_Shock = 0x5F8770,
		ORIG_Flare = 0x5F8960, ORIG_Wave = 0x5F8B80, ORIG_Debris = 0x5F8D10, ORIG_Master = 0x5F9020, ORIG_Restore = 0x5F9090;

	static const void *SOUND() { return (const void *)us(0xDA00A8); }
	static const uint32_t ANIM_DATA = 0xD99DC8;
	static const uint32_t SEQ_SPARK = 0xD9B458, SEQ_PILLAR = 0xD9B2AC;
	static const uint32_t MODEL_GLOW = 0xD9E2D4, MODEL_SHARD = 0xD9D314, MODEL_CHIP = 0xD9E1DC, MODEL_FLASH = 0xD9EC8C,
		MODEL_PILLAR = 0xD9CE04, MODEL_BLADE = 0xD9FE54, MODEL_RING = 0xD9B5D0, MODEL_SHOCK = 0xD9D594, MODEL_FLARE = 0xD9DD34,
		MODEL_WAVE = 0xD9CB38;
	static const uint32_t STREAK_REF = 0xDA00B0;
	static const uint32_t GLOW_REF = 0xDA00C0, SHARD_REF = 0xDA00D0, BLADE_REF = 0xDA00E0;

	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(us(0x56BC50))(in, out); }
	inline int32_t SqrtInt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(us(0x56BEC0))(v); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(us(0x56D100))(a); }
	inline void GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { fn<void (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(us(0x502170))(entity, bone, frac, out); }
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(us(0x56C4F0))(m, in, out); }
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(us(0x571620))(n, points, work); }
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(us(0x571690))(n, work, out, t); }
	inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x45DDA0))(r, g, b); }
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(us(0x45E110))(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(us(0x45F270))(); }
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E360))(dst); }
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(us(0x45E120))(a, b, c); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(us(0x45F4C0))(); }
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(us(0x45E370))(a, b, c); }
	inline void GteLoadV012(const void *v0, const void *v1, const void *v2) { fn<void (__cdecl *)(const void *, const void *, const void *)>(us(0x45DFE0))(v0, v1, v2); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(us(0x45FE10))(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E210))(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(us(0x45E270))(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(us(0x45E5C0))(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(us(0x45E610))(); }
	inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E3D0))(dst); }
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(us(0x45DE50))(m); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(us(0x4607E0))(); }
	inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(us(0x64DDA0))(m); }
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(us(0x64DE00))(s); }
	inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(us(0x64DD70))(v); }
	inline uint32_t Mag151Render(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x64E080))(h, ot, mode, cursor); }
	inline void BindEntityModelFromComData(uint8_t *entity, uint32_t out, uint32_t data) { fn<void (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(us(0x5022C0))(entity, out, data); }
	inline void QueueChainTransformation(uint8_t *entity, int32_t seq) { fn<void (__cdecl *)(uint8_t *, int32_t)>(us(0x505C00))(entity, seq); }

#pragma pack(push, 1)
	struct RootNode
	{
		TaskNode hdr;
		int16_t counter;
		int16_t done;
	};
	struct Node24
	{
		TaskNode hdr;
		int16_t c;
		int16_t e;
		int16_t w10, w12, w14, w16;
		int16_t w18;
		int16_t w1A;
		int16_t w1C;
		int16_t w1E;
		int16_t w20;
		int16_t w22;
	};
	struct SpriteP
	{
		uint32_t flags;
		int16_t life;
		int16_t scale;
		int16_t pos[4];
		int16_t vel[3];
		int16_t pad16;
	};
	struct ModelP
	{
		uint32_t flags;
		int16_t life;
		int16_t scale;
		int16_t pos[3];
		int16_t w0E;
		int16_t w10, w12, w14, w16;
		int16_t w18, w1A, w1C, w1E;
	};
	struct StreakRec
	{
		int16_t pos[4];
		int32_t dir[4];
		Mat4x3 mat;
	};
	struct RingEntry
	{
		int32_t alive;
		uint32_t base[2];
		uint32_t tip[2];
	};
	struct TrailWork
	{
		uint8_t pad00[8];
		int32_t otz;
		uint32_t flag;
		uint32_t col0;
		uint32_t col1;
		uint32_t pad18;
		uint32_t base_col;
		int16_t base[4];
		int16_t tip[4];
		int32_t dir[4];
	};
	struct SpriteWork
	{
		int16_t spawn[4];
		Mat4x3 cam;
		int32_t dir[4];
		int32_t mac[3];
		int32_t pad44;
	};
	struct ModelWork
	{
		int16_t ang[4];
		Mat4x3 cam;
		Mat4x3 m;
		int32_t v[4];
		int32_t axis[4];
	};
	struct PillarWork
	{
		int16_t ang[4];
		Mat4x3 cam;
		Mat4x3 m;
		int32_t dir[4];
		int32_t mac[4];
	};
	struct StreakWork
	{
		uint8_t pad00[8];
		int32_t otz;
		uint32_t flag;
		uint32_t col[4];
		int16_t cur[4];
		int16_t prev[4];
		int16_t width[4];
		int16_t off[4];
		int32_t ref[4];
		int32_t d[4];
		int32_t axis[4];
		Mat4x3 m;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node24) == 0x24, "Rough Divide nodes");
	static_assert(sizeof(SpriteP) == 0x18 && sizeof(ModelP) == 0x20 && sizeof(StreakRec) == 0x38, "Rough Divide pools");
	static_assert(sizeof(RingEntry) == 0x14 && sizeof(TrailWork) == 0x40 && sizeof(SpriteWork) == 0x48, "Rough Divide blocks");
	static_assert(sizeof(ModelWork) == 0x68 && sizeof(PillarWork) == 0x68 && sizeof(StreakWork) == 0x90, "Rough Divide blocks");

	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline StreakRec *Rec(int32_t i) { return (StreakRec *)(us(STREAK_REC) + 0x38 * i); }

	static void BoneFrame(uint8_t *anim_header, int32_t bone, int32_t *dir, int16_t *pos)
	{
		const uint8_t *sk = *(uint8_t **)*(uint8_t **)(anim_header + 4);
		if (bone >= 0xF0) bone = sk[bone - 0xEC];
		const uint8_t *b = sk + 0x10 + bone * 0x30;
		dir[1] = 0;
		dir[0] = 0;
		dir[2] = -0x1000;
		TransformVectorBy3x3Matrix((const Mat4x3 *)(b + 0x10), dir, dir);
		Normalize(dir, dir);
		pos[0] = *(const int16_t *)(b + 0x24);
		pos[1] = *(const int16_t *)(b + 0x28);
		pos[2] = *(const int16_t *)(b + 0x2C);
	}

	static void WeaponBone(uint8_t *entity, int32_t bone, int32_t *dir, int16_t *pos)
	{
		uint8_t *w = *(uint8_t **)(entity + 0x78);
		if (!w) return;
		BoneFrame(w, bone, dir, pos);
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(entity + 0x40), (const Mat4x3 *)(w + 0x24), &m);
		TransformVectorBy3x3Matrix(&m, dir, dir);
		Normalize(dir, dir);
		MatVec(&m, pos, pos);
		pos[0] = (int16_t)(pos[0] + (int16_t)m.t[0]);
		pos[1] = (int16_t)(pos[1] + (int16_t)m.t[1]);
		pos[2] = (int16_t)(pos[2] + (int16_t)m.t[2]);
	}

	static int32_t FreeSlot(uint32_t pool, uint32_t end, uint32_t stride, int32_t max)
	{
		int32_t i = 0;
		for (uint32_t a = pool; *(const uint32_t *)a != 0; i++)
		{
			a += stride;
			if (a >= end) return -1;
		}
		return i >= max ? -1 : i;
	}
}
}

namespace ff8fx
{
namespace rough163
{
	static Node24 *AddMain(uint32_t fn) { return (Node24 *)AddTaskToQueue(QMain(), fn); }

	static void AddCleared(uint32_t fn)
	{
		Node24 *t = AddMain(fn);
		t->c = 0;
		t->e = 0;
	}

	static void AddTrail()
	{
		AddMain(us(ORIG_Trail))->c = 0;
		for (uint32_t a = us(RING); a < us(RING_END); a += 0x14) var<int32_t>(a) = 0;
	}

	static void AddGlow()
	{
		Node24 *t = AddMain(us(ORIG_Glow));
		t->c = 0;
		GetEffectSpawnPosition(Caster(), 0xF1, 0, &t->w10);
		t->w12 = 0;
		t->w1C = 0x2800;
		t->w20 = 0x2800;
		t->w22 = 0x355;
	}

	static void AddStreak()
	{
		Node24 *t = AddMain(us(ORIG_Streak));
		t->c = 0;
		t->w1C = 0x14;
	}

	static void AddFlash()
	{
		Node24 *t = AddMain(us(ORIG_Flash));
		t->c = 0;
		GetEffectSpawnPosition(Caster(), 0xF1, 0, &t->w10);
		t->w12 = 0;
		t->w18 = (int16_t)(CrtRand() % 0x1000);
		t->w1C = 0xC00;
		t->w1A = (int16_t)(-0x50 - CrtRand() % 0x28);
		t->w22 = 0x1AA;
		t->w20 = 0x1AA;
	}

	static void AddBurst()
	{
		Node24 *t = AddMain(us(ORIG_Burst));
		t->c = 0;
		GetEffectSpawnPosition(Caster(), 0xF1, 0, &t->w10);
		t->w12 = 0;
	}

	static void AddPillar()
	{
		Node24 *t = AddMain(us(ORIG_Pillar));
		t->c = 0;
		t->w12 = 0;
		t->w14 = var<int16_t>(us(HIT_POS) + 4);
		t->e = 1;
		t->w10 = *(const int16_t *)(Entity(TargetSlot()) + 0x1C);
		var<uint32_t>(us(PILLAR_POS)) = *(const uint32_t *)&t->w10;
		var<uint32_t>(us(PILLAR_POS) + 4) = *(const uint32_t *)&t->w14;
	}

	static void AddRing()
	{
		Node24 *t = AddMain(us(ORIG_Ring));
		*(uint32_t *)&t->w10 = var<uint32_t>(us(PILLAR_POS));
		t->c = 0;
		*(uint32_t *)&t->w14 = var<uint32_t>(us(PILLAR_POS) + 4);
		t->w18 = (int16_t)(CrtRand() % 0x1000);
		t->w1C = (int16_t)(CrtRand() % 0x200 + 0xE00);
		const int16_t s = (int16_t)((CrtRand() % 0x400 + 0x1400) / 3);
		t->w22 = s;
		t->w20 = s;
	}

	static void AddShocks()
	{
		int32_t i = 1;
		int32_t base = 0x1000;
		do
		{
			Node24 *t = AddMain(us(ORIG_Shock));
			t->c = 0;
			t->e = (int16_t)i;
			t->w10 = var<int16_t>(us(PILLAR_POS));
			const int32_t r = CrtRand() % 0x190;
			t->w14 = var<int16_t>(us(PILLAR_POS) + 4);
			t->w12 = (int16_t)(-1000 * i - r + 0xC8);
			t->w18 = (int16_t)(CrtRand() % 0x1000);
			t->w1A = (int16_t)(CrtRand() % 0x64 + 0x1E);
			const int16_t s = (int16_t)((CrtRand() % 0x300 + base) / 4);
			t->w1E = s;
			t->w1C = s;
			const int32_t h = CrtRand() % 0x500;
			base -= 0x200;
			i++;
			t->w20 = (int16_t)(h + 0xE00);
		} while (base >= 0xC00);
	}

	static void AddFlare()
	{
		Node24 *t = AddMain(us(ORIG_Flare));
		*(uint32_t *)&t->w10 = var<uint32_t>(us(PILLAR_POS));
		t->c = 0;
		*(uint32_t *)&t->w14 = var<uint32_t>(us(PILLAR_POS) + 4);
		t->w18 = (int16_t)(CrtRand() % 0x1000);
		const int16_t s = (int16_t)((CrtRand() % 0x300 + 0x1400) / 3);
		t->w1E = s;
		t->w1C = s;
	}

	static void AddWaves()
	{
		for (int32_t i = 0; i < 0xC; i++)
		{
			Node24 *t = AddMain(us(ORIG_Wave));
			t->c = 0;
			t->e = (int16_t)(CrtRand() % 4 + i);
			*(uint32_t *)&t->w10 = var<uint32_t>(us(PILLAR_POS));
			*(uint32_t *)&t->w14 = var<uint32_t>(us(PILLAR_POS) + 4);
			t->w10 = (int16_t)(t->w10 + (int16_t)(CrtRand() % 0xFA0 - 0x7D0));
			t->w14 = (int16_t)(t->w14 + (int16_t)(CrtRand() % 0xFA0 - 0x7D0));
			t->w18 = (int16_t)(CrtRand() % 0x1000);
			t->w1C = (int16_t)(CrtRand() % 0xC00 + 0x900);
			t->w20 = (int16_t)(CrtRand() % 0x600 + 0xE00);
			t->w22 = (int16_t)(CrtRand() % 0xA + 6);
		}
	}

	static uint32_t __cdecl DirectorTask(TaskNode *tn)
	{
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)us(CASTER_MAT), (Mat4x3 *)us(CAM_CASTER));
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0) BindEntityModelFromComData(Caster(), us(BIND_OUT), us(ANIM_DATA));
		int32_t c = n->c;
		if (c < 7)
		{
			if (c == 0) QueueChainTransformation(Caster(), 1);
			else if (c == 1)
			{
				BdPlaySE(SOUND(), 0, 0x80);
				AddTrail();
				AddCleared(us(ORIG_Sparkle));
			}
			else if (c == 4) AddGlow();
		}
		else if ((c -= 7) < 6)
		{
			if (c == 0) QueueChainTransformation(Caster(), 2);
		}
		else if ((c -= 6) < 0x1B)
		{
			if (c == 0) QueueChainTransformation(Caster(), 3);
			else if (c == 7)
			{
				AddCleared(us(ORIG_HitSpark));
				AddCleared(us(ORIG_Shard));
				AddCleared(us(ORIG_Chip));
				AddStreak();
			}
			else if (c == 0x12)
			{
				AddFlash();
				AddPillar();
			}
			else if (c == 0x11)
			{
				AddBurst();
				QueueChainTransformation(Entity(TargetSlot()), 6);
			}
			else if (c == 0x19)
			{
				uint8_t *e = Caster();
				*(uint32_t *)&n->w10 = *(const uint32_t *)(e + 0x1C);
				*(uint32_t *)&n->w14 = *(const uint32_t *)(e + 0x20);
				*(int16_t *)(e + 0x1E) = (int16_t)0xD120;
			}
		}
		else if ((c -= 0x1B) < 5)
		{
			if (c == 0)
			{
				QueueChainTransformation(Caster(), 4);
				AddCleared(us(ORIG_Blade));
			}
		}
		else if ((c -= 5) < 0x22)
		{
			if (c == 6)
			{
				AddRing();
				AddWaves();
				AddCleared(us(ORIG_Debris));
			}
			else if (c == 7)
			{
				AddShocks();
				AddFlare();
			}
		}
		else if ((c -= 0x22) < 6)
		{
			if (c == 0)
			{
				QueueChainTransformation(Caster(), 5);
				*(int16_t *)(Caster() + 0x1E) = n->w12;
			}
			*(int16_t *)(Caster() + 0x20) = (int16_t)(n->w14 - (int16_t)(c * 1000 / 6) + 0x3E8);
		}
		if (n->c == 0x41) ApplyActionResultToTarget(Context()->actions->targets);
		if (n->c >= 0x4D) SetScreenFlash((uint32_t)shl32(0x55 - n->c, 8), 0);
		n->c++;
		if (n->c <= 0x55) return 0;
		SetScreenFlash(0, 0);
		uint8_t *e = Caster();
		*(uint32_t *)(e + 0x1C) = *(const uint32_t *)&n->w10;
		*(uint32_t *)(e + 0x20) = *(const uint32_t *)&n->w14;
		return TASK_END;
	}

	static void TrailPoints(TrailWork *w)
	{
		WeaponBone(Caster(), 1, w->dir, w->base);
		w->tip[0] = (int16_t)((int16_t)(mul32(w->dir[0], 666) >> 12) + w->base[0]);
		w->tip[1] = (int16_t)((int16_t)(mul32(w->dir[1], 666) >> 12) + w->base[1]);
		w->tip[2] = (int16_t)((int16_t)(mul32(w->dir[2], 666) >> 12) + w->base[2]);
	}

	static int32_t TrailGather(const uint8_t *ring, int32_t idx, int32_t want, uint8_t *pa, uint8_t *pb)
	{
		int32_t cnt = 0;
		if (want <= 0) return 0;
		do
		{
			const RingEntry *e = (const RingEntry *)(ring + 0x14 * idx);
			if (!e->alive) break;
			idx--;
			*(uint32_t *)(pa + 8 * cnt) = e->base[0];
			*(uint32_t *)(pa + 8 * cnt + 4) = e->base[1];
			*(uint32_t *)(pb + 8 * cnt) = e->tip[0];
			*(uint32_t *)(pb + 8 * cnt + 4) = e->tip[1];
			if (idx < 0) idx = 6;
			cnt++;
		} while (cnt < want);
		return cnt;
	}

	static void TrailSplines(int32_t cnt, const uint8_t *pa, const uint8_t *pb, uint8_t *sa, uint8_t *sb)
	{
		uint8_t *wk = (uint8_t *)FieldAlloc(0x190);
		SplineSetup(cnt, pa, wk);
		for (int32_t k = 0, t = 0; k < 32; k++, t += 0x1000) SplineEval(cnt, wk, sa + 8 * k, t / 31);
		SplineSetup(cnt, pb, wk);
		for (int32_t k = 0, t = 0; k < 32; k++, t += 0x1000) SplineEval(cnt, wk, sb + 8 * k, t / 31);
		FieldFree(0x190);
	}

	static void TrailHitPoint(const uint8_t *sa, const uint8_t *sb)
	{
		var<uint32_t>(us(HIT_POS)) = *(const uint32_t *)sb;
		var<uint32_t>(us(HIT_POS) + 4) = *(const uint32_t *)(sb + 4);
		int32_t *d = (int32_t *)us(HIT_DIR);
		d[0] = (int32_t)*(const int16_t *)sb - *(const int16_t *)(sb + 8);
		d[1] = (int32_t)*(const int16_t *)(sb + 2) - *(const int16_t *)(sb + 0xA);
		d[2] = (int32_t)*(const int16_t *)(sb + 4) - *(const int16_t *)(sb + 0xC);
		Normalize(d, d);
		var<uint32_t>(us(BASE_POS)) = *(const uint32_t *)sa;
		var<uint32_t>(us(BASE_POS) + 4) = *(const uint32_t *)(sa + 4);
		int32_t *b = (int32_t *)us(BASE_DIR);
		b[0] = (int32_t)var<int16_t>(us(HIT_POS)) - *(const int16_t *)sa;
		b[1] = (int32_t)var<int16_t>(us(HIT_POS) + 2) - *(const int16_t *)(sa + 2);
		b[2] = (int32_t)var<int16_t>(us(HIT_POS) + 4) - *(const int16_t *)(sa + 4);
		Normalize(b, b);
	}

	static void TrailDraw(const uint8_t *sa, const uint8_t *sb, TrailWork *w)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteSetFarColor2(0, 0, 0);
		uint32_t *pk = (uint32_t *)PacketCursor();
		uint32_t *pk2 = pk + 9;
		w->base_col = 0x3A903030;
		w->col1 = 0x3A903030;
		int32_t tacc = 0;
		for (int32_t off = 0; off < 0xF0; off += 8, tacc += 0x1000)
		{
			w->col0 = w->col1;
			pk[0] = 0x8000000;
			GteLoadV012(sa + off, sa + off + 8, sb + off);
			GteRTPT();
			GteReadFLAG(&w->flag);
			if (w->flag & 0x60000) continue;
			GteReadSXY012Split(pk + 2, pk + 4, pk + 6);
			GteLoadV0(sb + off + 8);
			GteRTPS();
			GteReadSXY2(pk + 8);
			GteAVSZ4();
			GteReadOTZ32(&w->otz);
			GteSetIR0(tacc / 30);
			GteLoadRGBC(&w->base_col);
			GteDPCS();
			GteStoreRGB2(&w->col1);
			pk[5] = pk[1] = w->col0;
			pk[7] = pk[3] = w->col1;
			InsertPrimAutoDepth(var<uint32_t>(us(0x1D8E04C)) + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk);
			pk2[5] = w->col0;
			pk2[3] = pk2[1] = 0x3A000000;
			pk2[7] = w->col1;
			pk2[2] = pk[2];
			pk2[4] = pk[4];
			pk2[6] = pk[6];
			pk2[8] = pk[8];
			pk2[0] = 0x8000000;
			InsertPrimAutoDepth(var<uint32_t>(us(0x1D8E04C)) + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk2);
			pk += 0x12;
			pk2 += 0x12;
		}
		PacketCursor() = (uint32_t)pk2;
	}

	static uint32_t __cdecl TrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		TrailWork *w = (TrailWork *)FieldAlloc(0x40);
		TrailPoints(w);
		const int32_t c = n->c;
		const int32_t idx = c % 7;
		RingEntry *e = (RingEntry *)(us(RING) + 0x14 * idx);
		e->alive = 1;
		e->base[0] = *(const uint32_t *)&w->base[0];
		e->base[1] = *(const uint32_t *)&w->base[2];
		e->tip[0] = *(const uint32_t *)&w->tip[0];
		e->tip[1] = *(const uint32_t *)&w->tip[2];
		const int32_t want = c >= TRAIL_LENGTH - 7 ? TRAIL_LENGTH - c : 7;
		const int32_t cnt = TrailGather((const uint8_t *)us(RING), idx, want, (uint8_t *)us(PTS_A), (uint8_t *)us(PTS_B));
		if (cnt > 1)
		{
			TrailSplines(cnt, (const uint8_t *)us(PTS_A), (const uint8_t *)us(PTS_B), (uint8_t *)us(SPL_A), (uint8_t *)us(SPL_B));
			TrailHitPoint((const uint8_t *)us(SPL_A), (const uint8_t *)us(SPL_B));
			TrailDraw((const uint8_t *)us(SPL_A), (const uint8_t *)us(SPL_B), w);
		}
		FieldFree(0x40);
		if (DrawOnly()) return 0;
		n->c++;
		return n->c >= TRAIL_LENGTH - 1 ? TASK_END : 0;
	}

	static void GlowDraw(const Node24 *n, int16_t sy, int32_t c, int32_t fade, const int16_t *q)
	{
		int32_t d[3];
		d[1] = 0;
		d[0] = (int32_t)n->w10 - q[0];
		d[2] = (int32_t)n->w14 - q[2];
		const int32_t len = SqrtInt(Normalize(d, d));
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors((const int32_t *)us(GLOW_REF), d, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = q[0];
		m.t[2] = q[2];
		int32_t sv[3];
		sv[1] = sy;
		sv[0] = n->w1C;
		sv[2] = shl32(len, 12) / 1800;
		m.t[1] = 0;
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = us(MODEL_GLOW);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (c >= 7)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl GlowTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t q[4];
		GetEffectSpawnPosition(Caster(), 0xF1, 0, q);
		GlowDraw(n, n->w20, n->c, shl32(n->c - 7, 9), q);
		if (DrawOnly()) return 0;
		n->w20 = (int16_t)(n->w20 + n->w22);
		n->c++;
		return n->c >= 0xF ? TASK_END : 0;
	}

	static void SpriteDraw(SpriteWork *w, uint8_t *h, const int16_t *pos, int16_t scale, int16_t life)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(scale);
		*(int16_t *)(h + 4) = life;
		GteReadMAC123(w->mac);
		Normalize(w->mac, w->dir);
		const int32_t k = -(scale >> 3);
		w->mac[0] += mul32(k, w->dir[0]) >> 12;
		w->mac[1] += mul32(w->dir[1], k) >> 12;
		w->mac[2] += mul32(w->dir[2], k) >> 12;
		GteSetTransFromVec32(w->mac);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	static int32_t SpritePool(SpriteWork *w, uint32_t type, bool moves)
	{
		uint8_t *h = AllocHeader(0xB4);
		w->cam = Camera();
		*(int16_t *)(h + 0x24) = 8;
		*(uint32_t *)h = us(SEQ_SPARK);
		GteSetLightMatrix(&w->cam);
		GteSetBackColorFromTrans(&w->cam);
		int32_t count = 0;
		for (uint32_t a = us(POOL_A); a < us(POOL_A_END); a += 0x18)
		{
			SpriteP *p = (SpriteP *)a;
			if (!(p->flags & type)) continue;
			SpriteDraw(w, h, p->pos, p->scale, p->life);
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->flags = 0;
				continue;
			}
			p->life++;
			if (moves)
			{
				p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
				p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
				p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
				p->vel[0] = (int16_t)(p->vel[0] - (int16_t)(p->vel[0] >> 3));
				p->vel[1] = (int16_t)(p->vel[1] - (int16_t)(p->vel[1] >> 3));
				p->vel[2] = (int16_t)(p->vel[2] - (int16_t)(p->vel[2] >> 3));
			}
			count++;
		}
		FieldFree(0xB4);
		return count;
	}

	static uint32_t SpritePoolEnd(TaskNode *tn, int32_t count)
	{
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		((Node24 *)tn)->c++;
		return count != 0 ? 0 : TASK_END;
	}

	static uint32_t __cdecl SparkleTask(TaskNode *tn)
	{
		SpriteWork *w = (SpriteWork *)FieldAlloc(0x48);
		if (!DrawOnly() && ((Node24 *)tn)->c <= 0x14)
		{
			for (int32_t j = 0; j < 2; j++)
			{
				GetEffectSpawnPosition(Caster(), j != 0 ? 0x13 : 0x14, 0x1000, w->spawn);
				if (w->spawn[1] <= -0x28) continue;
				const int32_t i = FreeSlot(us(POOL_A), us(POOL_A_END), 0x18, 0x64);
				if (i < 0) break;
				SpriteP *p = (SpriteP *)(us(POOL_A) + 0x18 * i);
				p->flags = 1;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x200 + 0x500);
				ff8fx::copy<(8)>(p->pos, w->spawn);
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(CrtRand() % 0x64 - 0x32));
				p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(CrtRand() % 0x64 - 0x32));
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(CrtRand() % 0x64 - 0x32));
			}
		}
		return SpritePoolEnd(tn, SpritePool(w, 1, false));
	}

	static uint32_t __cdecl HitSparkTask(TaskNode *tn)
	{
		SpriteWork *w = (SpriteWork *)FieldAlloc(0x48);
		if (!DrawOnly() && ((Node24 *)tn)->c <= 0xA)
		{
			ff8fx::copy<(8)>(w->spawn, (const void *)us(HIT_POS));
			for (int32_t j = 0; j < 2; j++)
			{
				const int32_t i = FreeSlot(us(POOL_A), us(POOL_A_END), 0x18, 0x64);
				if (i < 0) break;
				SpriteP *p = (SpriteP *)(us(POOL_A) + 0x18 * i);
				p->flags = 2;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x300 + 0x400);
				ff8fx::copy<(8)>(p->pos, (const void *)us(HIT_POS));
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(CrtRand() % 0x50 - 0x28));
				p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(CrtRand() % 0x50 - 0x28));
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(CrtRand() % 0x50 - 0x28));
			}
		}
		return SpritePoolEnd(tn, SpritePool(w, 2, false));
	}

	static uint32_t __cdecl BurstTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		SpriteWork *w = (SpriteWork *)FieldAlloc(0x48);
		if (!DrawOnly() && n->c <= 3)
		{
			ff8fx::copy<(8)>(w->spawn, &n->w10);
			for (int32_t j = 0; j < 8; j++)
			{
				const int32_t i = FreeSlot(us(POOL_A), us(POOL_A_END), 0x18, 0x64);
				if (i < 0) break;
				SpriteP *p = (SpriteP *)(us(POOL_A) + 0x18 * i);
				p->flags = 4;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x580 + 0x580);
				ff8fx::copy<(8)>(p->pos, w->spawn);
				const int32_t a = CrtRand() % 0x1000;
				int32_t r = CrtRand() % 0x64 + 0x12C;
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(mul32(ComputeSin(a), r) >> 12));
				const int32_t up = CrtRand() % 0x64;
				p->pos[1] = (int16_t)(p->pos[1] - (int16_t)up);
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(mul32(ComputeCos(a), r) >> 12));
				r = CrtRand() % 0x50 + 0x28;
				p->vel[0] = (int16_t)(mul32(ComputeSin(a), r) >> 12);
				p->vel[2] = (int16_t)(mul32(ComputeCos(a), r) >> 12);
			}
		}
		return SpritePoolEnd(tn, SpritePool(w, 4, true));
	}

	static inline void ModelFade(uint8_t *h, int32_t life, int32_t from, int32_t fade)
	{
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (life >= from)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
	}

	static void ShardDraw(ModelWork *w, uint8_t *h, const ModelP *p, int32_t fade)
	{
		w->v[0] = p->w18;
		w->v[1] = p->w1A;
		w->v[2] = p->w1C;
		const int32_t angle = RotationBetweenVectors((const int32_t *)us(SHARD_REF), w->v, w->axis);
		BuildAxisAngleRotationMatrix(angle, &w->m, w->axis);
		w->m.t[2] = p->pos[2];
		w->m.t[0] = p->pos[0];
		w->m.t[1] = p->pos[1];
		w->v[2] = p->scale;
		w->v[1] = p->scale;
		w->v[0] = p->scale;
		Scale3DMatrix(&w->m, w->v);
		ComposeAffineTransform(&w->cam, &w->m, &w->m);
		GteSetRotMatrix(&w->m);
		GteSetTransVector(&w->m);
		ModelFade(h, p->life, 1, fade);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
	}

	static uint32_t __cdecl ShardTask(TaskNode *tn)
	{
		ModelWork *w = (ModelWork *)FieldAlloc(0x68);
		if (!DrawOnly() && ((Node24 *)tn)->c <= 0xA)
		{
			ff8fx::copy<(8)>(w->ang, (const void *)us(HIT_POS));
			for (int32_t j = 0; j < 4; j++)
			{
				const int32_t i = FreeSlot(us(POOL_B), us(POOL_B_END), 0x20, 0x96);
				if (i < 0) break;
				ModelP *p = (ModelP *)(us(POOL_B) + 0x20 * i);
				p->flags = 1;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x280 + 0x280);
				ff8fx::copy<(8)>(p->pos, w->ang);
				int32_t *v = w->v;
				v[0] = CrtRand() % 0x2000 - 0x1000;
				v[1] = -(CrtRand() % 0x800);
				v[2] = CrtRand() % 0x2000 - 0x1000;
				Normalize(v, v);
				int32_t s = CrtRand() % 0x28 + 0x1E;
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(mul32(v[0], s) >> 12));
				p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(mul32(v[1], s) >> 12));
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(mul32(v[2], s) >> 12));
				s = CrtRand() % 0xF0 + 0xDC;
				p->w10 = (int16_t)(mul32(v[0], s) >> 12);
				p->w12 = (int16_t)(mul32(v[1], s) >> 12);
				p->w14 = (int16_t)(mul32(v[2], s) >> 12);
				p->w18 = (int16_t)v[0];
				p->w1A = (int16_t)v[1];
				p->w1C = (int16_t)v[2];
			}
		}
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)(h + 8) = 0;
		int32_t count = 0;
		w->cam = Camera();
		*(uint32_t *)h = us(MODEL_SHARD);
		for (uint32_t a = us(POOL_B); a < us(POOL_B_END); a += 0x20)
		{
			ModelP *p = (ModelP *)a;
			if (!(p->flags & 1)) continue;
			ShardDraw(w, h, p, mul32(p->life - 1, 682));
			if (DrawOnly()) continue;
			if (p->life >= 7)
			{
				p->flags = 0;
				continue;
			}
			p->life++;
			p->pos[0] = (int16_t)(p->pos[0] + p->w10);
			p->pos[1] = (int16_t)(p->pos[1] + p->w12);
			p->pos[2] = (int16_t)(p->pos[2] + p->w14);
			p->w10 = (int16_t)(p->w10 - (int16_t)(p->w10 >> 2));
			p->w12 = (int16_t)(p->w12 - (int16_t)(p->w12 >> 2));
			p->w14 = (int16_t)(p->w14 - (int16_t)(p->w14 >> 2));
			count++;
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		((Node24 *)tn)->c++;
		return count != 0 ? 0 : TASK_END;
	}

	static void ChipDraw(ModelWork *w, uint8_t *h, const ModelP *p, int16_t scale)
	{
		w->ang[0] = p->w18;
		w->ang[2] = p->w1C;
		ComposeZYXRotationMatrix(w->ang, &w->m);
		w->m.t[0] = p->pos[0];
		w->m.t[2] = p->pos[2];
		w->m.t[1] = p->pos[1];
		w->v[2] = scale;
		w->v[1] = scale;
		w->v[0] = scale;
		Scale3DMatrix(&w->m, w->v);
		ComposeAffineTransform(&w->cam, &w->m, &w->m);
		GteSetRotMatrix(&w->m);
		GteSetTransVector(&w->m);
	}

	static inline int16_t ChipNextScale(const ModelP *p) { return (int16_t)(mul32(0x1249 - mul32(p->life, 585), p->w0E) >> 12); }
	static inline int16_t DebrisNextScale(const ModelP *p) { return (int16_t)(mul32(0x2550 - mul32(p->life, 682), p->w0E) >> 12); }

	static uint32_t __cdecl ChipTask(TaskNode *tn)
	{
		ModelWork *w = (ModelWork *)FieldAlloc(0x68);
		if (!DrawOnly() && ((Node24 *)tn)->c <= 0xA)
		{
			ff8fx::copy<(8)>(w->ang, (const void *)us(HIT_POS));
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeSlot(us(POOL_B), us(POOL_B_END), 0x20, 0x96);
				if (i < 0) break;
				ModelP *p = (ModelP *)(us(POOL_B) + 0x20 * i);
				p->flags = 2;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x200 + 0x200);
				ff8fx::copy<(8)>(p->pos, w->ang);
				p->w0E = p->scale;
				int32_t *v = w->v;
				v[0] = CrtRand() % 0x1400 - 0xA00;
				v[1] = -(CrtRand() % 0x800);
				v[2] = CrtRand() % 0x1400 - 0xA00;
				Normalize(v, v);
				const int32_t s = CrtRand() % 0xDC + 0x28;
				p->w10 = (int16_t)(mul32(v[0], s) >> 12);
				p->w12 = (int16_t)(mul32(v[1], s) >> 12);
				p->w14 = (int16_t)(mul32(v[2], s) >> 12);
				p->w18 = (int16_t)(CrtRand() % 0x1000);
				const int32_t r = CrtRand() % 0x28 + 0x1E;
				p->w1A = (int16_t)r;
				if (r & 1) p->w1A = (int16_t)-r;
				p->w1C = (int16_t)(CrtRand() % 0x1000);
				p->w1E = (int16_t)(CrtRand() % 0x3C + 0x14);
			}
		}
		uint8_t *h = AllocHeader(0x58);
		w->cam = Camera();
		*(uint32_t *)h = us(MODEL_CHIP);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		int32_t count = 0;
		w->ang[1] = 0;
		for (uint32_t a = us(POOL_B); a < us(POOL_B_END); a += 0x20)
		{
			ModelP *p = (ModelP *)a;
			if (!(p->flags & 2)) continue;
			ChipDraw(w, h, p, p->scale);
			if (p->life >= 1) p->scale = ChipNextScale(p);
			PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
			if (DrawOnly()) continue;
			if (p->life >= 7)
			{
				p->flags = 0;
				continue;
			}
			p->life++;
			p->pos[0] = (int16_t)(p->pos[0] + p->w10);
			p->pos[1] = (int16_t)(p->pos[1] + p->w12);
			p->pos[2] = (int16_t)(p->pos[2] + p->w14);
			p->w10 = (int16_t)(p->w10 - (int16_t)(p->w10 >> 3));
			p->w12 = (int16_t)(p->w12 - (int16_t)(p->w12 >> 3));
			p->w14 = (int16_t)(p->w14 - (int16_t)(p->w14 >> 3));
			p->w18 = (int16_t)(p->w18 + p->w1A);
			p->w1C = (int16_t)(p->w1C + p->w1E);
			count++;
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		((Node24 *)tn)->c++;
		return count != 0 ? 0 : TASK_END;
	}

	static uint32_t __cdecl DebrisTask(TaskNode *tn)
	{
		ModelWork *w = (ModelWork *)FieldAlloc(0x68);
		if (!DrawOnly() && ((Node24 *)tn)->c <= 5)
		{
			ff8fx::copy<(8)>(w->ang, (const void *)us(PILLAR_POS));
			for (int32_t j = 0; j < 6; j++)
			{
				const int32_t i = FreeSlot(us(POOL_C), us(POOL_C_END), 0x20, 0x96);
				if (i < 0) break;
				ModelP *p = (ModelP *)(us(POOL_C) + 0x20 * i);
				p->flags = 4;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0xA00 + 0x100);
				ff8fx::copy<(8)>(p->pos, w->ang);
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(CrtRand() % 0xFA0 - 0x7D0));
				p->pos[1] = (int16_t)(-0x64 - CrtRand() % 0x12C);
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(CrtRand() % 0xFA0 - 0x7D0));
				p->w0E = p->scale;
				p->w12 = (int16_t)(-0x3C - CrtRand() % 0x1A4);
				p->w18 = (int16_t)(CrtRand() % 0x1000);
				const int32_t r = CrtRand() % 0x96 + 0x64;
				p->w1A = (int16_t)r;
				if (r & 1) p->w1A = (int16_t)-r;
				p->w1C = (int16_t)(CrtRand() % 0x1000);
				p->w1E = (int16_t)(CrtRand() % 0x82 + 0x3C);
			}
		}
		uint8_t *h = AllocHeader(0x58);
		int32_t count = 0;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		w->cam = Camera();
		*(uint32_t *)h = us(MODEL_CHIP);
		w->ang[1] = 0;
		for (uint32_t a = us(POOL_C); a < us(POOL_C_END); a += 0x20)
		{
			ModelP *p = (ModelP *)a;
			if (!(p->flags & 4)) continue;
			ChipDraw(w, h, p, p->scale);
			if (p->life >= 8) p->scale = DebrisNextScale(p);
			PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
			if (DrawOnly()) continue;
			if (p->life >= 0xE)
			{
				p->flags = 0;
				continue;
			}
			p->life++;
			p->pos[1] = (int16_t)(p->pos[1] + p->w12);
			p->w12 = (int16_t)(p->w12 - (int16_t)(p->w12 >> 4));
			p->w18 = (int16_t)(p->w18 + p->w1A);
			p->w1C = (int16_t)(p->w1C + p->w1E);
			count++;
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		((Node24 *)tn)->c++;
		return count != 0 ? 0 : TASK_END;
	}

	static void PillarDraw(PillarWork *w, uint8_t *hs, uint8_t *hm, const ModelP *p, int16_t scale, int16_t spin, int32_t fade)
	{
		GteLoadV0(p->pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(p->scale);
		*(int16_t *)(hs + 4) = p->life;
		GteReadMAC123(w->mac);
		Normalize(w->mac, w->dir);
		const int32_t k = -(p->scale >> 3);
		w->mac[0] += mul32(w->dir[0], k) >> 12;
		w->mac[1] += mul32(w->dir[1], k) >> 12;
		w->mac[2] += mul32(w->dir[2], k) >> 12;
		GteSetTransFromVec32(w->mac);
		PacketCursor() = InitEffectSequenceFromData(hs, RenderOT(), 2, PacketCursor());
		if (p->life < 6)
		{
			w->ang[2] = spin;
			BuildRotationMatrixFromAngles(w->ang, &w->m);
			w->dir[2] = scale;
			w->dir[1] = scale;
			w->dir[0] = scale;
			Scale3DMatrix(&w->m, w->dir);
			GteSetRotMatrix(&w->m);
			ModelFade(hm, p->life, 0, fade);
			PacketCursor() = RenderPrimModel(hm, RenderOT(), 2, PacketCursor());
		}
	}

	static uint32_t __cdecl PillarTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		PillarWork *w = (PillarWork *)FieldAlloc(0x68);
		if (!DrawOnly() && n->c <= 0xA)
		{
			ff8fx::copy<(8)>(w->ang, &n->w10);
			const int32_t k1 = n->c + 1;
			const int32_t amp = (mul32(ComputeSin((int32_t)shl32(n->c, 11) / 10), 1500) >> 12) + 100;
			for (int32_t j = 0; j < 2; j++)
			{
				const int32_t i = FreeSlot(us(POOL_B), us(POOL_B_END), 0x20, 0x96);
				if (i < 0) break;
				ModelP *p = (ModelP *)(us(POOL_B) + 0x20 * i);
				p->flags = 4;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0xA00 + 0x500);
				ff8fx::copy<(8)>(p->pos, w->ang);
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(CrtRand() % (amp + amp) - amp));
				p->pos[1] = (int16_t)(mul32(k1, -350) - CrtRand() % 0x320 + 0x190);
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(CrtRand() % (amp + amp) - amp));
				p->w10 = (int16_t)(p->scale >> 1);
				p->w12 = (int16_t)(p->scale / 3);
				p->w14 = (int16_t)(CrtRand() % 0x1000);
				p->w16 = (int16_t)(CrtRand() % 0x46 + 0x32);
			}
		}
		uint8_t *hs = AllocHeader(0xB4);
		*(uint32_t *)hs = us(SEQ_PILLAR);
		*(int16_t *)(hs + 0x24) = 8;
		int32_t count = 0;
		uint8_t *hm = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)(hm + 8) = 0;
		w->cam = Camera();
		*(uint32_t *)hm = us(MODEL_PILLAR);
		GteSetLightMatrix(&w->cam);
		GteSetBackColorFromTrans(&w->cam);
		w->ang[0] = 0x200;
		w->ang[1] = (int16_t)0xFE00;
		for (uint32_t a = us(POOL_B); a < us(POOL_B_END); a += 0x20)
		{
			ModelP *p = (ModelP *)a;
			if (!(p->flags & 4)) continue;
			PillarDraw(w, hs, hm, p, p->w10, p->w14, mul32(p->life, 682));
			if (DrawOnly()) continue;
			if (*(const int16_t *)(hs + 0x28) < 0)
			{
				p->flags = 0;
				continue;
			}
			p->life++;
			p->w10 = (int16_t)(p->w10 + p->w12);
			p->w12 = (int16_t)(p->w12 - (int16_t)(p->w12 / 3));
			p->w14 = (int16_t)(p->w14 + p->w16);
			count++;
		}
		FieldFree(0x58);
		FieldFree(0xB4);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		n->c++;
		return count != 0 ? 0 : TASK_END;
	}

	static void BladeDraw(ModelWork *w, uint8_t *h, const ModelP *p, int16_t sy, int32_t fade)
	{
		w->m = Rec(p->scale)->mat;
		w->m.t[2] = p->pos[2];
		w->m.t[0] = p->pos[0];
		w->v[0] = p->w10;
		w->m.t[1] = 0;
		w->v[1] = sy;
		Scale3DMatrix(&w->m, w->v);
		ComposeAffineTransform(&w->cam, &w->m, &w->m);
		GteSetRotMatrix(&w->m);
		GteSetTransVector(&w->m);
		ModelFade(h, p->life, 6, fade);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
	}

	static uint32_t __cdecl BladeTask(TaskNode *tn)
	{
		ModelWork *w = (ModelWork *)FieldAlloc(0x68);
		const int32_t c = ((Node24 *)tn)->c;
		if (!DrawOnly() && c < 0xA)
		{
			const int32_t i = FreeSlot(us(POOL_B), us(POOL_B_END), 0x20, 0x96);
			if (i >= 0)
			{
				ModelP *p = (ModelP *)(us(POOL_B) + 0x20 * i);
				const StreakRec *r = Rec(c);
				p->flags = 8;
				p->life = 0;
				p->scale = (int16_t)c;
				ff8fx::copy<(8)>(p->pos, r->pos);
				const int16_t s = (int16_t)(shl32(c + 2, 9) / 3);
				w->v[1] = 0;
				p->w16 = s;
				p->w12 = s;
				w->v[0] = (int32_t)Rec(c + 1)->pos[0] - r->pos[0];
				w->v[2] = (int32_t)Rec(c + 1)->pos[2] - r->pos[2];
				const int32_t len = SqrtInt(Normalize(w->v, w->v));
				const int32_t angle = RotationBetweenVectors((const int32_t *)us(BLADE_REF), w->v, w->axis);
				BuildAxisAngleRotationMatrix(angle, &Rec(c)->mat, w->axis);
				p->w10 = (int16_t)(shl32(len, 12) / 700);
			}
		}
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = us(MODEL_BLADE);
		*(uint32_t *)(h + 8) = 0;
		int32_t count = 0;
		w->v[2] = 0;
		w->cam = Camera();
		for (uint32_t a = us(POOL_B); a < us(POOL_B_END); a += 0x20)
		{
			ModelP *p = (ModelP *)a;
			if (!(p->flags & 8)) continue;
			BladeDraw(w, h, p, p->w12, mul32(p->life - 6, 682));
			if (DrawOnly()) continue;
			if (p->life >= 0xC)
			{
				p->flags = 0;
				continue;
			}
			p->life++;
			p->w12 = (int16_t)(p->w12 + p->w16);
			p->w16 = (int16_t)(p->w16 - (int16_t)(p->w16 / 5));
			count++;
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		((Node24 *)tn)->c++;
		return count != 0 ? 0 : TASK_END;
	}

	static void StreakBuild(StreakWork *w, int32_t count, int16_t width, uint8_t *vtx)
	{
		ff8fx::copy<(16)>(w->ref, (const void *)us(STREAK_REF));
		w->width[0] = width;
		w->width[1] = 0;
		w->width[2] = 0;
		for (int32_t i = 0; i < count; i++)
		{
			if (i != 0)
			{
				ff8fx::copy<(8)>(w->cur, Rec(i)->pos);
				ff8fx::copy<(8)>(w->prev, Rec(i - 1)->pos);
				w->d[0] = (int32_t)w->cur[0] - w->prev[0];
				w->d[2] = (int32_t)w->cur[2] - w->prev[2];
			}
			else
			{
				ff8fx::copy<(8)>(w->cur, Rec(0)->pos);
				ff8fx::copy<(8)>(w->prev, Rec(1)->pos);
				w->d[0] = (int32_t)w->prev[0] - w->cur[0];
				w->d[2] = (int32_t)w->prev[2] - w->cur[2];
			}
			w->d[1] = 0;
			Normalize(w->d, w->d);
			const int32_t angle = RotationBetweenVectors(w->ref, w->d, w->axis);
			BuildAxisAngleRotationMatrix(angle, &w->m, w->axis);
			MatVec(&w->m, w->width, w->off);
			int16_t *v = (int16_t *)(vtx + 0x18 * i);
			ff8fx::copy<(8)>(v, w->cur);
			v[4] = (int16_t)(w->off[0] + w->cur[0]);
			v[8] = (int16_t)(w->cur[0] - w->off[0]);
			v[6] = (int16_t)(w->off[2] + w->cur[2]);
			v[10] = (int16_t)(w->cur[2] - w->off[2]);
			v[9] = 0;
			v[5] = 0;
		}
	}

	static void StreakDraw(StreakWork *w, int32_t count, int32_t c, int32_t fade, const uint8_t *vtx)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteSetFarColor2(0, 0, 0);
		uint32_t *pk1 = (uint32_t *)PacketCursor();
		uint32_t *pk2 = pk1 + 9;
		w->col[0] = 0x3A202080;
		w->col[1] = 0x3A04040E;
		w->col[2] = 0x3A04040E;
		w->col[3] = 0x3A000000;
		if (c >= 0x20)
		{
			GteLoadRGB012(&w->col[0], &w->col[1], &w->col[2]);
			GteSetIR0(fade);
			GteDPCT();
			GteStoreRGB012(&w->col[0], &w->col[1], &w->col[2]);
			w->col[0] |= 0x3A000000;
			w->col[1] |= 0x3A000000;
		}
		const int32_t segs = count - 1;
		for (int32_t j = 0; j < segs; j++)
		{
			const uint8_t *v = vtx + 0x18 * j;
			pk2[0] = 0x8000000;
			pk1[0] = 0x8000000;
			GteLoadV012(v + 0x18, v + 8, v + 0x10);
			GteRTPT();
			GteReadFLAG(&w->flag);
			if (w->flag & 0x60000) continue;
			GteReadSXY012Split(pk1 + 4, pk1 + 6, pk2 + 8);
			pk2[2] = pk1[4];
			GteAVSZ3();
			GteReadOTZ32(&w->otz);
			GteLoadV012(v, v + 0x20, v + 0x28);
			GteRTPT();
			GteReadSXY012Split(pk1 + 8, pk1 + 2, pk2 + 4);
			pk2[6] = pk1[8];
			if (j == 0)
			{
				pk2[3] = pk1[1] = w->col[1];
				pk2[7] = pk1[5] = w->col[3];
				pk2[1] = pk1[3] = w->col[0];
				pk2[5] = pk1[7] = w->col[2];
			}
			else if (j == segs - 1)
			{
				pk2[3] = pk1[1] = w->col[3];
				pk2[7] = pk1[5] = w->col[1];
				pk2[1] = pk1[3] = w->col[2];
				pk2[5] = pk1[7] = w->col[0];
			}
			else
			{
				pk1[5] = pk1[1] = w->col[1];
				pk1[7] = pk1[3] = w->col[0];
				pk2[5] = pk2[1] = w->col[0];
				pk2[7] = pk2[3] = w->col[1];
			}
			InsertPrimAutoDepth(var<uint32_t>(us(0x1D8E04C)) + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk1);
			InsertPrimAutoDepth(var<uint32_t>(us(0x1D8E04C)) + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk2);
			pk1 += 0x12;
			pk2 += 0x12;
		}
		PacketCursor() = (uint32_t)pk2;
	}

	static uint32_t __cdecl StreakTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		StreakWork *w = (StreakWork *)FieldAlloc(0x90);
		int32_t count;
		if (n->c >= 0xB) count = 0xB;
		else
		{
			StreakRec *r = Rec(n->c);
			ff8fx::copy<(8)>(r->pos, (const void *)us(HIT_POS));
			ff8fx::copy<(16)>(r->dir, (const void *)us(HIT_DIR));
			count = n->c + 1;
		}
		if (count >= 2)
		{
			StreakBuild(w, count, n->w1C, (uint8_t *)us(STREAK_VTX));
			StreakDraw(w, count, n->c, shl32(n->c - 0x20, 9), (const uint8_t *)us(STREAK_VTX));
		}
		FieldFree(0x90);
		if (DrawOnly()) return 0;
		if (n->c < 0x14) n->w1C = (int16_t)(n->w1C + 5);
		n->c++;
		return n->c >= 0x28 ? TASK_END : 0;
	}

	static void ModelMatrix(const Node24 *n, int16_t yaw, int16_t sx, int16_t sy)
	{
		int16_t ang[3];
		ang[0] = 0;
		ang[1] = yaw;
		ang[2] = 0;
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		int32_t sv[3];
		m.t[0] = n->w10;
		m.t[1] = n->w12;
		sv[2] = sx;
		sv[0] = sx;
		m.t[2] = n->w14;
		sv[1] = sy;
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	static void ModelRender(uint32_t model, int32_t c, int32_t from, int32_t fade)
	{
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (c >= from)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl FlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelMatrix(n, n->w18, n->w1C, n->w20);
		ModelRender(us(MODEL_FLASH), n->c, 1, mul32(n->c - 1, 682));
		if (DrawOnly()) return 0;
		const int16_t rate = n->w22;
		n->w18 = (int16_t)(n->w18 + n->w1A);
		n->w20 = (int16_t)(n->w20 + rate);
		n->c++;
		n->w22 = (int16_t)(rate - rate / 7);
		return n->c >= 7 ? TASK_END : 0;
	}

	static uint32_t __cdecl RingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelMatrix(n, n->w18, n->w1C, n->w20);
		ModelRender(us(MODEL_RING), n->c, 0xC, shl32(n->c - 0xC, 9));
		if (DrawOnly()) return 0;
		const int16_t rate = n->w22;
		n->w20 = (int16_t)(n->w20 + rate);
		n->c++;
		n->w22 = (int16_t)(rate - rate / 4);
		return n->c >= 0x14 ? TASK_END : 0;
	}

	static uint32_t __cdecl ShockTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (DrawOnly()) return 0;
			n->e--;
			return 0;
		}
		ModelMatrix(n, n->w18, n->w1C, n->w20);
		ModelRender(us(MODEL_SHOCK), n->c, 8, shl32(n->c - 8, 9));
		if (DrawOnly()) return 0;
		const int16_t rate = n->w1E;
		n->w18 = (int16_t)(n->w18 + n->w1A);
		n->w1C = (int16_t)(n->w1C + rate);
		n->c++;
		n->w1E = (int16_t)(rate - rate / 6);
		return n->c >= 0x10 ? TASK_END : 0;
	}

	static uint32_t __cdecl FlareTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelMatrix(n, n->w18, n->w1C, n->w1C);
		ModelRender(us(MODEL_FLARE), n->c, 8, shl32(n->c - 8, 9));
		if (DrawOnly()) return 0;
		const int16_t rate = n->w1E;
		n->w1C = (int16_t)(n->w1C + rate);
		n->c++;
		n->w1E = (int16_t)(rate - rate / 4);
		return n->c >= 0x10 ? TASK_END : 0;
	}

	static void WaveRender(int32_t c, int32_t scroll, int32_t fade)
	{
		uint8_t *h = AllocHeader(0x7C);
		*(int32_t *)(h + 0x10) = scroll;
		*(uint32_t *)h = us(MODEL_WAVE);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x20) = 0x33;
		*(int16_t *)(h + 0x18) = 0;
		*(int16_t *)(h + 0x1A) = 0xC0;
		*(int16_t *)(h + 0x1C) = 0x80;
		*(int16_t *)(h + 0x1E) = 0x40;
		*(uint32_t *)(h + 0x14) = 0;
		if (c < 4)
		{
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x20) = 0xF3;
		}
		else if (c >= 0xA)
		{
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x20) = 0xF3;
		}
		PacketCursor() = Mag151Render(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x7C);
	}

	static uint32_t __cdecl WaveTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (DrawOnly()) return 0;
			n->e--;
			return 0;
		}
		ModelMatrix(n, n->w18, n->w1C, n->w20);
		WaveRender(n->c, mul32(n->w22, n->c), n->c < 4 ? shl32(4 - n->c, 0xA) : shl32(n->c - 0xA, 9));
		if (DrawOnly()) return 0;
		n->c++;
		return n->c >= 0x12 ? TASK_END : 0;
	}

	static uint32_t __cdecl MasterTask(TaskNode *tn)
	{
		RootNode *r = (RootNode *)tn;
		if (r->counter & 1) PacketCursor() = TexBase() + 0x10000;
		else PacketCursor() = TexBase();
		const int alive = ExecuteTaskQueue(QMain());
		if (!alive)
		{
			RootNode *t = (RootNode *)AddTaskToQueue(QMaster(), us(ORIG_Restore));
			t->counter = 0;
			t->done = 0;
		}
		r->counter++;
		return alive ? 0 : TASK_END;
	}

	static uint32_t __cdecl RestoreTask(TaskNode *tn)
	{
		RootNode *r = (RootNode *)tn;
		if (r->counter == 1) TextureRestoreTask((void *)TexBase(), &r->done);
		r->counter++;
		return r->done != 0 ? TASK_END : 0;
	}
}

	void register_mag163_rough_divide()
	{
		register_port(rough163::ORIG_Master, (void *)rough163::MasterTask, "163 Master", 163);
		register_port(rough163::ORIG_Restore, (void *)rough163::RestoreTask, "163 Restore", 163);
		register_port(rough163::ORIG_Director, (void *)rough163::DirectorTask, "163 Director", 163);
		register_port(rough163::ORIG_Trail, (void *)rough163::TrailTask, "163 Trail", 163);
		register_port(rough163::ORIG_Glow, (void *)rough163::GlowTask, "163 Glow", 163);
		register_port(rough163::ORIG_Sparkle, (void *)rough163::SparkleTask, "163 Sparkle", 163);
		register_port(rough163::ORIG_HitSpark, (void *)rough163::HitSparkTask, "163 HitSpark", 163);
		register_port(rough163::ORIG_Shard, (void *)rough163::ShardTask, "163 Shard", 163);
		register_port(rough163::ORIG_Chip, (void *)rough163::ChipTask, "163 Chip", 163);
		register_port(rough163::ORIG_Streak, (void *)rough163::StreakTask, "163 Streak", 163);
		register_port(rough163::ORIG_Flash, (void *)rough163::FlashTask, "163 Flash", 163);
		register_port(rough163::ORIG_Burst, (void *)rough163::BurstTask, "163 Burst", 163);
		register_port(rough163::ORIG_Pillar, (void *)rough163::PillarTask, "163 Pillar", 163);
		register_port(rough163::ORIG_Blade, (void *)rough163::BladeTask, "163 Blade", 163);
		register_port(rough163::ORIG_Ring, (void *)rough163::RingTask, "163 Ring", 163);
		register_port(rough163::ORIG_Shock, (void *)rough163::ShockTask, "163 Shock", 163);
		register_port(rough163::ORIG_Flare, (void *)rough163::FlareTask, "163 Flare", 163);
		register_port(rough163::ORIG_Wave, (void *)rough163::WaveTask, "163 Wave", 163);
		register_port(rough163::ORIG_Debris, (void *)rough163::DebrisTask, "163 Debris", 163);
	}
}

