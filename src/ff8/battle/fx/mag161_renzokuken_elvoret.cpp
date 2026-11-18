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

// Effect 161: Renzokuken vs Elvoret / Elnoyle

#include "mag_common.h"

namespace ff8fx
{
namespace renzo161
{
	using namespace eng;
	using namespace magc;

	inline int32_t &CasterSlot() { return var<int32_t>(us(0x23D33FC)); }
	inline int32_t &TargetSlot() { return var<int32_t>(us(0x23D0E10)); }
	inline int32_t &Length() { return var<int32_t>(us(0x23D3494)); }
	inline int32_t &Finisher() { return var<int32_t>(us(0x23D3498)); }
	inline uint32_t &TexBase() { return var<uint32_t>(us(0x23D349C)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(us(0x23D34A0)); }
	inline int16_t &CameraShake() { return var<int16_t>(us(0x1D97712)); }
	inline TaskQueue *QMain() { return (TaskQueue *)us(0x23D25D8); }
	inline TaskQueue *QHit() { return (TaskQueue *)us(0x209FAA8); }
	static const uint32_t POOL_A = 0x23D0E18, POOL_A_END = 0x23D0FF8;
	static const uint32_t POOL_B = 0x23D0FF8, POOL_B_SPAWN_END = 0x23D1278, POOL_B_END = 0x23D13B8;
	static const uint32_t POOL_C = 0x23D20A0, POOL_C_END = 0x23D23C0;
	static const uint32_t POOL_D = 0x23D1B28, POOL_D_END = 0x23D1E48;
	static const uint32_t POOL_E = 0x23D1E48, POOL_E_END = 0x23D20A0;
	static const int BURST_COUNT = 28;
	static const uint32_t HIT_REC = 0x23D13B8;
	static const uint32_t PTS_A = 0x23D0D90, PTS_B = 0x23D0DD0;
	static const uint32_t SPL_A = 0x23D23C0, SPL_B = 0x23D24C0;
	static const uint32_t HIT_POS = 0x23D25C0;
	static const uint32_t HIT_DIR = 0x23D25C8;
	static const uint32_t RING = 0x23D3408, RING_END = 0x23D3494;
	inline uint8_t StageCount() { return var<uint8_t>(us(0x1D98214)); }
	inline uint8_t StageLen(int i) { return var<uint8_t>(us(0x1D98215) + i); }

	static const uint32_t ORIG_Controller = 0x5FD230, ORIG_HitSpark = 0x5FD810, ORIG_Sparkle = 0x5FDAA0,
		ORIG_Trail = 0x5FDDD0, ORIG_SparkSpawn = 0x5FE310, ORIG_Hit = 0x5FE440, ORIG_Slash = 0x5FE790,
		ORIG_Final = 0x5FEA70, ORIG_Master = 0x5FEC10;

	static const void *SOUND() { return (const void *)us(0xE9AD24); }
	static const uint32_t MODEL_SPARKLE = 0xE9AD28, SPARKLE_FADE = 0xE9AEB0, SPARKLE_DEPTH = 0xE9AEA0;
	static const uint32_t ANIM_SETS = 0xE9B314, BIND_OUT = 0x24FEB98;
	static const uint32_t MODEL_SLASH = 0xDA9474, MODEL_STREAK = 0xDA9E5C, MODEL_RING = 0xDAA12C;
	static const uint32_t SEQ_FLASH = 0xDA8ED4, SEQ_DEBRIS = 0xDA91A4, SEQ_STREAM = 0xDA8D28, SEQ_SPARK = 0xDA9080, SPARK_FRAMES = 0xDAA668;
	static const int SHARED_SEQ_BURST = 3;
	static const uint32_t SLASH_REF = 0xDAA6C8;
	static const uint32_t HIT_LEAD = 0xDAA6D8;

	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(us(0x56BC50))(in, out); }
	inline int32_t SqrtInt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(us(0x56BEC0))(v); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(us(0x56D100))(angle); }
	inline void GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { fn<void (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(us(0x502170))(entity, bone, frac, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(us(0x56C270))(a, b); }
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(us(0x56C4F0))(m, in, out); }
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(us(0x571620))(n, points, work); }
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(us(0x571690))(n, work, out, t); }
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x45DD60))(r, g, b); }
	inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x45DDA0))(r, g, b); }
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(us(0x45E110))(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(us(0x45F270))(); }
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E360))(dst); }
	inline void GteLoadV012(const void *v0, const void *v1, const void *v2) { fn<void (__cdecl *)(const void *, const void *, const void *)>(us(0x45DFE0))(v0, v1, v2); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(us(0x45FE10))(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E210))(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(us(0x45E270))(a, b, c); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(us(0x45E610))(); }
	inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E3D0))(dst); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(us(0x460850))(); }
	inline uint32_t BindEntityModelFromComData(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(us(0x5022C0))(entity, out, data); }
	inline void QueueChainTransformation(uint8_t *entity, int32_t seq) { fn<void (__cdecl *)(uint8_t *, int32_t)>(us(0x505C00))(entity, seq); }
	inline void AddAnimSeqTask(uint8_t *entity, int32_t type) { fn<void (__cdecl *)(uint8_t *, int32_t)>(us(0x571840))(entity, type); }

#pragma pack(push, 1)
	struct RootNode
	{
		TaskNode hdr;
		int16_t counter;
		int16_t pad0E;
	};
	struct Node24
	{
		TaskNode hdr;
		int16_t c;
		int16_t e;
		int16_t w10, w12, w14;
		int16_t w16;
		int16_t w18;
		int16_t w1A;
		int16_t w1C;
		int16_t w1E;
		int16_t w20;
		int16_t w22;
	};
	struct FlashP
	{
		int16_t life, scale;
		int16_t pos[4];
	};
	struct SparkP
	{
		int16_t life, scale;
		int16_t pos[3];
		int16_t vy;
		int16_t frame;
		int16_t pad0E;
	};
	struct DebrisP
	{
		int16_t life, scale;
		int16_t pos[4];
		int16_t vel[3];
		int16_t pad12;
	};
	struct StreamP
	{
		int16_t life, scale;
		int16_t pos[4];
		int16_t vel[3];
		int16_t fall;
	};
	struct BurstP
	{
		int16_t life, scale;
		int16_t pos[4];
		int16_t vx, vy_unused, vz;
		int16_t pad12;
	};
	struct HitRec
	{
		int16_t pos[4];
		int32_t dir[4];
		int32_t count;
		int16_t angle[16];
		int16_t width[16];
		int16_t off[16];
		int16_t speed[16];
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
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node24) == 0x24, "Renzokuken nodes");
	static_assert(sizeof(FlashP) == 0xC && sizeof(SparkP) == 0x10 && sizeof(DebrisP) == 0x14, "Renzokuken pools");
	static_assert(sizeof(StreamP) == 0x14 && sizeof(BurstP) == 0x14, "Renzokuken 161 pools");
	static_assert(sizeof(HitRec) == 0x9C && sizeof(RingEntry) == 0x14 && sizeof(TrailWork) == 0x40, "Renzokuken records");

	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline uint8_t *Target() { return Entity(TargetSlot()); }

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
}
}

#ifdef FF8_FX_HELD
#include "mag161_renzokuken_elvoret_held.h"
#endif

namespace ff8fx
{
namespace renzo161
{
	static void BindBoth(uint8_t *caster, uint8_t *boss, uint32_t out, uint32_t sets, int32_t seq)
	{
		const uint32_t next = BindEntityModelFromComData(caster, out, sets + *(const uint32_t *)(sets + 4));
		BindEntityModelFromComData(boss, next, sets + *(const uint32_t *)(sets + 8));
		QueueChainTransformation(caster, seq);
		QueueChainTransformation(boss, 1);
	}

	static int32_t FreeSlot(uint32_t pool, uint32_t end, uint32_t stride)
	{
		int32_t i = 0;
		for (uint32_t a = pool; *(const int16_t *)a >= 0; i++)
		{
			a += stride;
			if (a >= end) return -1;
		}
		return i >= 0x28 ? -1 : i;
	}

	static __declspec(noinline) uint32_t ControllerBody(Node24 *n, int16_t entry_w)
	{
		if (DrawOnly()) return 0;
		if (n->c == 0)
		{
			uint16_t *flags = (uint16_t *)Caster();
			const uint16_t f = *flags;
			n->e = (int16_t)(f & 0x800);
			*flags = (uint16_t)(f | 0x800);
		}
		if (n->c == 1) BindBoth(Caster(), Target(), us(BIND_OUT), us(ANIM_SETS), (Finisher() != 0) + 1);
		if (n->c == 2)
		{
			const int32_t last = (int32_t)StageCount() + 1;
			Length() = 0;
			for (int32_t i = 1; i <= last; i++) Length() = Length() + var<uint8_t>(us(0x1D98214) + i);
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), us(ORIG_Trail));
			t->c = 0;
			for (uint32_t a = us(RING); a < us(RING_END); a += 0x14) var<int32_t>(a) = 0;
			t = (Node24 *)AddTaskToQueue(QMain(), us(ORIG_SparkSpawn));
			t->c = 0;
			t->e = (int16_t)(CrtRand() % 0x18);
			t = (Node24 *)AddTaskToQueue(QMain(), us(ORIG_Hit));
			t->c = 0;
		}
		if (Finisher() != 0 && (int32_t)n->c == Length())
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QHit(), us(ORIG_HitSpark));
			t->c = 0;
			t->w1A = (int16_t)CasterSlot();
		}
		if (n->c >= 0x1C && n->c <= 0x50)
		{
			int16_t a[4], b[4];
			GetEffectSpawnPosition(Target(), 0xF0, 0, a);
			GetEffectSpawnPosition(Target(), 0xF0, 0x1000, b);
			b[3] = entry_w;
			int32_t d[3] = { (int32_t)b[0] - a[0], (int32_t)b[1] - a[1], (int32_t)b[2] - a[2] };
			Normalize(d, d);
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeSlot(us(POOL_D), us(POOL_D_END), 0x14);
				if (i < 0) break;
				StreamP *p = (StreamP *)(us(POOL_D) + 0x14 * i);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x700 + 0x600);
				ff8fx::copy<(8)>(p->pos, b);
				const int32_t sp = CrtRand() % 0x78 + 0x64;
				const int32_t r0 = CrtRand() % 0x14;
				p->vel[0] = (int16_t)(r0 + (mul32(sp, d[0]) >> 12) - 0xA);
				const int32_t r1 = CrtRand() % 0x14;
				p->vel[1] = (int16_t)(r1 + (mul32(sp, d[1]) >> 12) - 0xA);
				const int32_t r2 = CrtRand() % 0x14;
				p->vel[2] = (int16_t)(r2 + (mul32(sp, d[2]) >> 12) - 0xA);
				p->fall = (int16_t)(CrtRand() % 0x12 + 0xA);
			}
		}
		if (n->c == 0x2E)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(QMain(), us(ORIG_Final));
			t->c = 0;
			GetDefaultEffectPosition(Target(), &t->w10);
			const int16_t h = t->w16;
			t->w16 = t->w12;
			t->w12 = h;
			t->w1E = 0x1800;
			t->w1C = 0x1800;
			int16_t p4[4];
			GetDefaultEffectPosition(Target(), p4);
			for (int32_t k = 0; k < BURST_COUNT; k++)
			{
				BurstP *p = (BurstP *)(us(POOL_E) + 0x14 * k);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x600 + 0x1400);
				p->pos[0] = p4[0];
				p->pos[1] = p4[3];
				p->pos[2] = p4[2];
				const int32_t ang = CrtRand() % 0x1000;
				const int32_t sp = CrtRand() % 0xC8 + 0xDC;
				p->vx = (int16_t)(mul32(ComputeCos(ang), sp) >> 12);
				p->vz = (int16_t)(mul32(ComputeSin(ang), sp) >> 12);
			}
		}
		if (n->c >= 0x2E && n->c <= 0x32) CameraShake() = (int16_t)(CameraShake() + ((n->c & 1) ? -0x20 : 0x20));
		if (n->c <= 0x10)
			SetScreenFlash((uint32_t)shl32(n->c, 7), 0);
		else if ((int32_t)n->c >= Length() - 8 && n->c > 4 && Finisher() == 0)
			SetScreenFlash((uint32_t)shl32(Length() - n->c, 8), 0);
		bool tint = false;
		int32_t k = 0;
		if (n->c <= 8)
		{
			GteSetFarColor(0xF0, 0x60, 0x40);
			k = n->c;
			tint = true;
		}
		else if ((int32_t)n->c >= Length() - 8 && n->c > 4)
		{
			GteSetFarColor(0xF0, 0x60, 0x40);
			k = Length() - n->c;
			tint = true;
		}
		if (tint)
		{
			uint32_t colour = var<uint32_t>(us(0xB8B7D8));
			GteLoadRGBC(&colour);
			GteSetIR0(shl32(k, 9));
			GteDPCS();
			GteStoreRGB2(Caster() + 0x28);
		}
		if (n->c == 1) BdPlaySE(SOUND(), 0x8000, 0x80);
		n->c++;
		if ((int32_t)n->c <= Length()) return 0;
		if (Finisher() == 0) SetScreenFlash(0, 0);
		*(uint32_t *)(Caster() + 0x28) = var<uint32_t>(us(0xB8B7D8));
		uint16_t *flags = (uint16_t *)Caster();
		*flags = (uint16_t)((*flags & 0xF7FF) | (uint16_t)n->e);
		return TASK_END;
	}

	static uint32_t __cdecl ControllerTask(TaskNode *tn)
	{
		return ControllerBody((Node24 *)tn, 0x1A);
	}

	static uint32_t __cdecl HitSparkTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c >= 4 && n->c <= 8)
		{
			int16_t p[4];
			GetDefaultEffectPosition(Entity(n->w1A), p);
			const int16_t h = p[3];
			p[3] = p[1];
			p[1] = h;
			for (int32_t i = 0; i < 6; i++)
			{
				Node24 *s = (Node24 *)AddTaskToQueue(QHit(), us(ORIG_Sparkle));
				s->c = 0;
				s->e = (int16_t)(CrtRand() % 6 + i);
				const uint8_t *sk = **(uint8_t ***)(Entity(n->w1A) + 0x64);
				int32_t bone = sk[0];
				if (bone != 0) bone = CrtRand() % bone;
				int16_t q[4];
				GetEffectSpawnPosition(Entity(n->w1A), bone, 0, q);
				q[0] = (int16_t)(q[0] + (int16_t)(CrtRand() % 0xAF - 0x57));
				q[1] = (int16_t)(q[1] + (int16_t)(CrtRand() % 0xAF - 0x57));
				q[2] = (int16_t)(q[2] + (int16_t)(CrtRand() % 0xAF - 0x57));
				int16_t y = q[1];
				if (y > -600)
				{
					y = (int16_t)(y - 600);
					q[1] = y;
				}
				int32_t v[3] = { (int32_t)q[0] - p[0], (int32_t)y - p[1], (int32_t)q[2] - p[2] };
				Normalize(v, v);
				v[0] = shl32(v[0], 2);
				v[2] = shl32(v[2], 2);
				Normalize(v, v);
				const int32_t len = CrtRand() % 0x258 + 0x15E;
				s->w10 = (int16_t)((mul32(len, v[0]) >> 12) + q[0]);
				s->w12 = (int16_t)((mul32(len, v[1]) >> 12) + q[1]);
				s->w18 = (int16_t)bone;
				s->w14 = (int16_t)((mul32(len, v[2]) >> 12) + q[2]);
				s->w1A = n->w1A;
				s->w1C = (int16_t)(CrtRand() % 8);
				s->w20 = (int16_t)(CrtRand() % 0x680 + 0x80);
			}
		}
		if (n->c == 1) AddAnimSeqTask(Entity(n->w1A), 3);
		n->c++;
		return n->c >= 0x1E ? TASK_END : 0;
	}

	static uint32_t __cdecl SparkleTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (DrawOnly()) return 0;
			n->e--;
			return 0;
		}
		int16_t q[4];
		GetEffectSpawnPosition(Entity(n->w1A), n->w18, shl32(n->c, 9), q);
		int32_t d[3] = { (int32_t)var<int16_t>(us(0xB8B7F0)) - q[0], 0, (int32_t)var<int16_t>(us(0xB8B7F4)) - q[2] };
		int32_t u[3] = { 0, 0, -0x1000 };
		Normalize(d, d);
		int32_t axis[3];
		int32_t angle = RotationBetweenVectors(u, d, axis);
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m1;
		BuildRotationMatrixFromAngles(ang, &m1);
		Mat4x3 m2;
		m1.t[0] = 0xF0;
		m1.t[1] = 0;
		m1.t[2] = 0;
		BuildAxisAngleRotationMatrix(angle, &m2, axis);
		MatrixMultiply(&m2, &m1);
		int32_t spin = -shl32(n->c, 6);
		if (axis[1] < 0) spin = -spin;
		BuildAxisAngleRotationMatrix(spin, &m2, d);
		MatrixMultiply(&m2, &m1);
		int32_t e[3] = { (int32_t)n->w10 - q[0], (int32_t)n->w12 - q[1], (int32_t)n->w14 - q[2] };
		u[0] = 0;
		u[1] = -0x1000;
		u[2] = 0;
		const int32_t len = SqrtInt(Normalize(e, e));
		angle = RotationBetweenVectors(u, e, axis);
		BuildAxisAngleRotationMatrix(angle, &m2, axis);
		m2.t[0] = q[0];
		m2.t[2] = q[2];
		int32_t sc[3];
		sc[2] = n->w20;
		sc[0] = n->w20;
		m2.t[1] = q[1];
		sc[1] = shl32(len, 12) / 2000;
		Scale3DMatrix(&m2, sc);
		ComposeAffineTransform(&Camera(), &m2, &m2);
		ComposeAffineTransform(&m2, &m1, &m1);
		GteSetRotMatrix(&m1);
		GteSetTransVector(&m1);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = us(MODEL_SPARKLE);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0xC) = var<uint16_t>(us(SPARKLE_FADE) + 2 * n->c);
		*(uint32_t *)(h + 0x18) = var<uint16_t>(us(SPARKLE_DEPTH) + 2 * n->w1C);
		*(uint32_t *)(h + 0x1C) = 0x10F3;
		var<uint32_t>(us(0x1D8E054)) = RenderPrimModel(h, RenderOT(), 2, var<uint32_t>(us(0x1D8E054)));
		FieldFree(0x58);
		if (DrawOnly()) return 0;
		n->w1C++;
		if (n->w1C >= 8) n->w1C = 0;
		n->c++;
		return n->c >= 8 ? TASK_END : 0;
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

	static void TrailHitPoint(const uint8_t *sb, uint8_t *pos, int32_t *dir)
	{
		*(uint32_t *)pos = *(const uint32_t *)sb;
		*(uint32_t *)(pos + 4) = *(const uint32_t *)(sb + 4);
		dir[0] = (int32_t)*(const int16_t *)sb - *(const int16_t *)(sb + 8);
		dir[1] = (int32_t)*(const int16_t *)(sb + 2) - *(const int16_t *)(sb + 0xA);
		dir[2] = (int32_t)*(const int16_t *)(sb + 4) - *(const int16_t *)(sb + 0xC);
		Normalize(dir, dir);
	}

	static void TrailDraw(const uint8_t *sa, const uint8_t *sb, TrailWork *w)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteSetFarColor2(0, 0, 0);
		uint32_t *pk = (uint32_t *)PacketCursor();
		uint32_t *pk2 = pk + 9;
		w->col1 = 0x3A903030;
		w->base_col = 0x3A903030;
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
			pk2[4] = pk[4];
			pk2[2] = pk[2];
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
		const int32_t want = c < Length() - 7 ? 7 : Length() - c;
		const int32_t cnt = TrailGather((const uint8_t *)us(RING), idx, want, (uint8_t *)us(PTS_A), (uint8_t *)us(PTS_B));
		if (cnt > 1)
		{
			TrailSplines(cnt, (const uint8_t *)us(PTS_A), (const uint8_t *)us(PTS_B), (uint8_t *)us(SPL_A), (uint8_t *)us(SPL_B));
			TrailHitPoint((const uint8_t *)us(SPL_B), (uint8_t *)us(HIT_POS), (int32_t *)us(HIT_DIR));
			FX_HELD(held_note_trail(n);)
			TrailDraw((const uint8_t *)us(SPL_A), (const uint8_t *)us(SPL_B), w);
		}
		FieldFree(0x40);
		if (DrawOnly()) return 0;
		n->c++;
		return (int32_t)n->c >= Length() - 1 ? TASK_END : 0;
	}

	static uint32_t __cdecl SparkSpawnTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		if ((int32_t)n->c < Length() - 8)
		{
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeSlot(us(POOL_B), us(POOL_B_SPAWN_END), 0x10);
				if (i < 0) break;
				SparkP *p = (SparkP *)(us(POOL_B) + 0x10 * i);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x300 + 0x400);
				const uint8_t *sk = **(uint8_t ***)(Caster() + 0x64);
				int32_t bone = sk[0];
				if (bone != 0) bone = CrtRand() % bone;
				GetEffectSpawnPosition(Caster(), bone, 0, p->pos);
				p->vy = (int16_t)(CrtRand() % 10 + 2);
				p->frame = (int16_t)((int32_t)n->e % 0x18);
			}
		}
		n->c++;
		n->e++;
		return (int32_t)n->c >= Length() - 1 ? TASK_END : 0;
	}

	static uint32_t __cdecl HitTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		int32_t sum = 0;
		for (int32_t b = 0; b < (int32_t)StageCount(); b++)
		{
			const int32_t lead = var<int32_t>(us(HIT_LEAD) + 4 * b);
			sum += StageLen(b);
			if ((int32_t)n->c != sum - lead) continue;
			Node24 *s = (Node24 *)AddTaskToQueue(QMain(), us(ORIG_Slash));
			s->c = 0;
			s->e = (int16_t)b;
			const int16_t sc = (int16_t)(CrtRand() % 0x300 + 0x500);
			s->w1E = sc;
			s->w1C = sc;
			HitRec *r = (HitRec *)(us(HIT_REC) + 0x9C * b);
			ff8fx::copy<(8)>(r->pos, (const void *)us(HIT_POS));
			ff8fx::copy<(16)>(r->dir, (const void *)us(HIT_DIR));
			r->count = 0x10;
			for (int32_t k = 0; k < r->count; k++)
			{
				r->angle[k] = (int16_t)(CrtRand() % 0x1000);
				const int32_t wv = CrtRand() % 0xC00 + 0xC00;
				r->width[k] = (int16_t)wv;
				r->off[k] = (int16_t)(mul32(wv, 1000) >> 12);
				r->speed[k] = (int16_t)(CrtRand() % 200 + 0x78);
			}
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeSlot(us(POOL_A), us(POOL_A_END), 0xC);
				if (i < 0) break;
				FlashP *p = (FlashP *)(us(POOL_A) + 0xC * i);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x800 + 0xE00);
				ff8fx::copy<(8)>(p->pos, (const void *)us(HIT_POS));
				p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(CrtRand() % 0x2BC - 0x15E));
				p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(CrtRand() % 0x2BC - 0x15E));
				p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(CrtRand() % 0x2BC - 0x15E));
			}
			for (int32_t j = 0; j < 0xC; j++)
			{
				const int32_t i = FreeSlot(us(POOL_C), us(POOL_C_END), 0x14);
				if (i < 0) break;
				DebrisP *p = (DebrisP *)(us(POOL_C) + 0x14 * i);
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x1000 + 0x1800);
				ff8fx::copy<(8)>(p->pos, (const void *)us(HIT_POS));
				const int32_t sp = CrtRand() % 200 + 0x8C;
				const int32_t *d = (const int32_t *)us(HIT_DIR);
				const int32_t r0 = CrtRand() % 200;
				p->vel[0] = (int16_t)(r0 + (mul32(d[0], sp) >> 12) - 0x64);
				const int32_t r1 = CrtRand() % 200;
				p->vel[1] = (int16_t)(r1 + (mul32(d[1], sp) >> 12) - 0x64);
				const int32_t r2 = CrtRand() % 200;
				p->vel[2] = (int16_t)(r2 + (mul32(d[2], sp) >> 12) - 0x64);
			}
		}
		n->c++;
		return (int32_t)n->c >= Length() - 1 ? TASK_END : 0;
	}

	static uint32_t __cdecl SlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		HitRec *r = (HitRec *)(us(HIT_REC) + 0x9C * n->e);
		FX_HELD(held_note_slash(n, r);)
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors((const int32_t *)us(SLASH_REF), r->dir, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = r->pos[0];
		m.t[1] = r->pos[1];
		m.t[2] = r->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m3;
		BuildRotationMatrixFromAngles(ang, &m3);
		int32_t sv[3];
		m3.t[0] = 0;
		m3.t[1] = 0;
		m3.t[2] = 0;
		sv[1] = n->w1C;
		sv[0] = n->w1C;
		sv[2] = 0x1000;
		Scale3DMatrix(&m3, sv);
		ComposeAffineTransform(&m, &m3, &m3);
		GteSetRotMatrix(&m3);
		GteSetTransVector(&m3);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = us(MODEL_SLASH);
		*(uint32_t *)(h + 0x1C) = 0x30;
		if (n->c >= 2)
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0xF0;
			*(int32_t *)(h + 0xC) = mul32(n->c - 2, 682);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		*(uint32_t *)h = us(MODEL_STREAK);
		*(uint32_t *)(h + 0x1C) |= 3;
		for (int32_t k = 0; k < r->count; k++)
		{
			ang[2] = r->angle[k];
			BuildRotationMatrixFromAngles(ang, &m3);
			int16_t v[4];
			v[3] = 0;
			v[2] = 0;
			v[0] = 0;
			v[1] = r->off[k];
			GteLoadV0(v);
			GteSetRotMatrix(&m3);
			GteMVMVA_RotV0();
			GteReadMAC123(m3.t);
			const int32_t wd = r->width[k];
			sv[1] = wd;
			sv[0] = (wd >> 1) + wd;
			Scale3DMatrix(&m3, sv);
			ComposeAffineTransform(&m, &m3, &m3);
			GteSetRotMatrix(&m3);
			GteSetTransVector(&m3);
			PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
			if (!DrawOnly())
			{
				r->off[k] = (int16_t)(r->off[k] + r->speed[k]);
				r->speed[k] = (int16_t)(r->speed[k] - (int16_t)(r->speed[k] >> 4));
			}
		}
		FieldFree(0x58);
		if (DrawOnly()) return 0;
		const int16_t rate = n->w1E;
		n->w1C = (int16_t)(n->w1C + rate);
		n->w1E = (int16_t)(rate - rate / 6);
		n->c++;
		return n->c >= 8 ? TASK_END : 0;
	}

	static uint32_t __cdecl FinalTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		FX_HELD(held_note_final(n);)
		int16_t a[3] = { 0, 0, 0 };
		Mat4x3 m;
		BuildRotationMatrixFromAngles(a, &m);
		int32_t sv[3];
		sv[1] = 0x1000;
		m.t[1] = n->w12;
		m.t[0] = n->w10;
		sv[2] = n->w1C;
		sv[0] = n->w1C;
		m.t[2] = n->w14;
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = us(MODEL_RING);
		*(uint32_t *)(h + 0x1C) = 3;
		if (n->c >= 4)
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(uint32_t *)(h + 0x1C) = 0xC3;
			*(int32_t *)(h + 0xC) = mul32(n->c - 4, 682);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
		if (DrawOnly()) return 0;
		const int16_t rate = n->w1E;
		n->w1C = (int16_t)(n->w1C + rate);
		n->w1E = (int16_t)(rate - rate / 3);
		n->c++;
		return n->c >= 0xA ? TASK_END : 0;
	}

	static void ParticleDraws()
	{
		FX_HELD(held_note_particles();)
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = us(SEQ_FLASH);
		*(int16_t *)(h + 0x24) = 0;
		for (uint32_t a = us(POOL_A); a < us(POOL_A_END); a += 0xC)
		{
			FlashP *p = (FlashP *)a;
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0) p->life = -1;
		}
		*(uint32_t *)h = us(SEQ_DEBRIS);
		for (uint32_t a = us(POOL_C); a < us(POOL_C_END); a += 0x14)
		{
			DebrisP *p = (DebrisP *)a;
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->life = -1;
				continue;
			}
			p->scale = (int16_t)(p->scale - (int16_t)(p->scale >> 5));
			p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
			p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
			p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
			p->vel[0] = (int16_t)(p->vel[0] - (int16_t)(p->vel[0] >> 3));
			p->vel[1] = (int16_t)(p->vel[1] - (int16_t)(p->vel[1] >> 3));
			p->vel[2] = (int16_t)(p->vel[2] - (int16_t)(p->vel[2] >> 3));
		}
		*(uint32_t *)h = us(SEQ_STREAM);
		for (uint32_t i = 0; i < (us(POOL_D_END) - us(POOL_D)) / 0x14; i++)
		{
			StreamP *p = (StreamP *)(us(POOL_D) + 0x14 * i);
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->life = -1;
				continue;
			}
			p->scale = (int16_t)(p->scale + (int16_t)(((const DebrisP *)(us(POOL_C) + 0x14 * i))->scale >> 5));
			p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
			p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(p->fall + p->vel[1]));
			p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
			p->vel[0] = (int16_t)(p->vel[0] - (int16_t)(p->vel[0] >> 4));
			p->vel[1] = (int16_t)(p->vel[1] - (int16_t)(p->vel[1] >> 4));
			p->vel[2] = (int16_t)(p->vel[2] - (int16_t)(p->vel[2] >> 4));
			if (p->pos[1] > 0) p->life = -1;
		}
		*(void **)h = EffectSpriteSharedSequence(SHARED_SEQ_BURST);
		for (uint32_t a = us(POOL_E); a < us(POOL_E_END); a += 0x14)
		{
			BurstP *p = (BurstP *)a;
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->life = -1;
				continue;
			}
			p->scale = (int16_t)(p->scale + (int16_t)(p->scale >> 7));
			p->pos[0] = (int16_t)(p->pos[0] + p->vx);
			p->pos[2] = (int16_t)(p->pos[2] + p->vz);
			p->vx = (int16_t)(p->vx - (int16_t)(p->vx >> 3));
			p->vz = (int16_t)(p->vz - (int16_t)(p->vz >> 3));
		}
		*(uint32_t *)h = us(SEQ_SPARK);
		*(int16_t *)(h + 0x24) = 4;
		for (uint32_t a = us(POOL_B); a < us(POOL_B_END); a += 0x10)
		{
			SparkP *p = (SparkP *)a;
			if (p->life < 0) continue;
			*(int16_t *)(h + 4) = p->life;
			*(uint32_t *)(h + 0x1C) = var<uint32_t>(us(SPARK_FRAMES) + 4 * p->frame);
			TransformCameraByShadowRotation(p->pos, p->scale, -(p->scale >> 4));
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (UpdateFlags() & 0x201) continue;
			p->life++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->life = -1;
				continue;
			}
			p->pos[1] = (int16_t)(p->pos[1] + p->vy);
			p->frame++;
			if (p->frame >= 0x18) p->frame = 0;
		}
		FieldFree(0xB4);
	}

	static uint32_t __cdecl MasterTask(TaskNode *tn)
	{
		RootNode *r = (RootNode *)tn;
		if (r->counter & 1) PacketCursor() = TexBase() + 0x8000;
		else PacketCursor() = TexBase();
		FX_HELD(held_note_master();)
		const int alive = ExecuteTaskQueue(QMain());
		ParticleDraws();
		r->counter++;
		return alive ? 0 : TASK_END;
	}
}

	void register_mag161_renzokuken_elvoret()
	{
		register_port(renzo161::ORIG_Master, (void *)renzo161::MasterTask, "161 Master", 161);
		register_port(renzo161::ORIG_Controller, (void *)renzo161::ControllerTask, "161 Controller", 161);
		register_port(renzo161::ORIG_Trail, (void *)renzo161::TrailTask, "161 Trail", 161);
		register_port(renzo161::ORIG_SparkSpawn, (void *)renzo161::SparkSpawnTask, "161 SparkSpawn", 161);
		register_port(renzo161::ORIG_Hit, (void *)renzo161::HitTask, "161 Hit", 161);
		register_port(renzo161::ORIG_Slash, (void *)renzo161::SlashTask, "161 Slash", 161);
		register_port(renzo161::ORIG_Final, (void *)renzo161::FinalTask, "161 Final", 161);
		register_port(renzo161::ORIG_HitSpark, (void *)renzo161::HitSparkTask, "161 HitSpark", 161);
		register_port(renzo161::ORIG_Sparkle, (void *)renzo161::SparkleTask, "161 Sparkle", 161);
		FX_HELD(register_mag161_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag161_renzokuken_elvoret_held.inc"
#endif
