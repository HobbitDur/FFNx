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

// Effect 185: Shiva - Diamond Dust

#include "fx_port.h"

namespace ff8fx
{
namespace s185
{
	using namespace eng;

	namespace x
	{
		inline int32_t CrtRand() { return fn<int32_t (__cdecl *)()>(us(0x55CBD2))(); }
		inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(us(0x56D100))(a); }
		inline void ComposeZYXRotationMatrix(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(us(0x56CE30))(angles, out); }
		inline void Scale3DMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(us(0x56BEF0))(m, v); }
		inline int32_t NormalizeVectorToFixedPoint(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(us(0x56BC50))(in, out); }
		inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(us(0x56BEC0))(v); }
		inline uint32_t MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { return fn<uint32_t (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(us(0x56C4F0))(m, in, out); }
		inline void TransformVectorBy3x3Matrix(const Mat4x3 *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int32_t *, int32_t *)>(us(0x56C600))(m, in, out); }
		inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(us(0x502170))(entity, bone, frac, out); }
		inline int32_t PreReadAnimation(void *header, void *cmd, int32_t id) { return fn<int32_t (__cdecl *)(void *, void *, int32_t)>(us(0x509440))(header, cmd, id); }
		inline int32_t ReadAnimation(void *header, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(us(0x508F90))(header, cmd); }
		inline uint32_t RenderGeometry(void *geom, void *hdr, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(us(0x5099D0))(geom, hdr, ot, shift, cursor); }
		inline void BdPlaySE(uint32_t se, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(us(0x501330))(se, a, b); }
		inline void BdPlaySummonStream(int32_t a, int32_t b, int32_t c) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x5018C0))(a, b, c); }
		inline void BdTransSummonStream(uint32_t src, uint32_t state) { fn<void (__cdecl *)(uint32_t, uint32_t)>(us(0x501860))(src, state); }
		inline int32_t VoiceSlotBusy(uint32_t slot) { return fn<int32_t (__cdecl *)(uint32_t)>(us(0x4A2900))(slot); }
		inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(us(0x4A2940))(slot); }
		inline void CameraScriptStart(uint32_t script, const void *frame, void *entity, TaskQueue *q, int32_t n) { fn<void (__cdecl *)(uint32_t, const void *, void *, TaskQueue *, int32_t)>(us(0x63E960))(script, frame, entity, q, n); }
		inline void CameraShake(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(us(0x5712C0))(a, b, c, d); }
		inline void ApplyActionResultToTargets(uint32_t list, uint32_t count) { fn<void (__cdecl *)(uint32_t, uint32_t)>(us(0x506BA0))(list, count); }
		inline void WaitAnimSeq(void *buffer, void *flag) { fn<void (__cdecl *)(void *, void *)>(us(0x508630))(buffer, flag); }
		inline int32_t LoadState() { return fn<int32_t (__cdecl *)()>(us(0x508500))(); }
		inline void CharacterLoad(int32_t id, uint32_t dst) { fn<void (__cdecl *)(int32_t, uint32_t)>(us(0x508480))(id, dst); }
		inline void QueueTIMUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(us(0x505E30))(tim); }
		inline uint32_t SummonData() { return fn<uint32_t (__cdecl *)()>(us(0x571B70))(); }
		inline void Mag151Draw(void *h, uint32_t ot, int32_t mode, uint32_t *cursor) { *cursor = fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x64E080))(h, ot, mode, *cursor); }
		inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(us(0x45DE50))(m); }
		inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(us(0x64DDA0))(m); }
		inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(us(0x4607E0))(); }
		inline void GteSetRotDiag(int32_t s) { fn<void (__cdecl *)(int32_t)>(us(0x64DDD0))(s); }
		inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(us(0x64DE00))(s); }
		inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(us(0x64DD70))(v); }
		inline uint32_t RingRender(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x5C1B50))(h, ot, mode, cursor); }
		inline void ShatterSpawn() { fn<void (__cdecl *)()>(us(0x5C1E80))(); }
		inline uint32_t ShatterStatic(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x5C2390))(h, ot, mode, cursor); }
		inline uint32_t ShatterFly(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x5C26A0))(h, ot, mode, cursor); }
		inline uint32_t MountRender(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x5C3170))(h, ot, mode, cursor); }
		inline void MountShardInit(void *h) { fn<void (__cdecl *)(void *)>(us(0x5C3B10))(h); }
		inline uint32_t ShardFly(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x5C3D10))(h, ot, mode, cursor); }
		inline void IcicleSpawn() { fn<void (__cdecl *)()>(us(0x5C4820))(); }
		inline void TextureBlit(void *e, int32_t a, int32_t b) { fn<void (__cdecl *)(void *, int32_t, int32_t)>(us(0x5C4F80))(e, a, b); }
		inline uint32_t ShellRender(void *model, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(us(0x5C50D0))(model, h, ot, mode, cursor); }
		inline void IceBlockSpawn() { fn<void (__cdecl *)()>(us(0x5C7570))(); }
		inline uint32_t IceBlockRender(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x5C7AB0))(h, ot, mode, cursor); }
	}
	using namespace x;

	inline bool Paused() { return (var<uint32_t>(us(0x1D96A9C)) & 0x201) != 0; }
	inline Mat4x3 &Frame() { return var<Mat4x3>(us(0x22BC130)); }
	inline Mat4x3 &RootMatrix() { return var<Mat4x3>(us(0x22BD0C0)); }
	inline TaskQueue &SubQueue() { return var<TaskQueue>(us(0x22BC198)); }
	inline uint8_t *CastCtx() { return var<uint8_t *>(us(0x22BCFB8)); }
	inline uint32_t &VoiceSlot() { return var<uint32_t>(us(0x22BD0E0)); }
	inline uint32_t &DoneFlags() { return var<uint32_t>(us(0x22BD010)); }
	inline uint8_t *Creature() { return (uint8_t *)us(0x22BD018); }
	inline uint32_t MB() { return var<uint32_t>(us(0x22BD0B4)); }
	inline uint32_t &Arena() { return var<uint32_t>(us(0x22BD0B8)); }
	inline uint32_t &Cursor() { return var<uint32_t>(us(0x1D8E054)); }
	inline uint32_t OT() { return var<uint32_t>(us(0x1D8E04C)) + 0x44; }
	inline uint8_t *PoolA0() { return var<uint8_t *>(us(0xD324A0)); }
	inline uint8_t *PoolA4() { return var<uint8_t *>(us(0xD324A4)); }
	inline uint32_t Shards() { return var<uint32_t>(us(0xD324A8)); }
	inline uint8_t *RingVerts() { return var<uint8_t *>(us(0xD3249C)); }
	inline int32_t RootT(int k) { return var<int32_t>(us(0x22BD0D4) + 4 * k); }
	inline int16_t *Mid() { return (int16_t *)us(0x22BCFC8); }
	inline int16_t *MidWorld() { return (int16_t *)us(0x22BCFC0); }
	static const uint32_t SPRITE_MAT = 0x22BD0E8;

	static const uint32_t ORIG_SequenceTick = 0x5C7F50;
	static const uint32_t ORIG_TimelineTask = 0x5C0F30;
	static const uint32_t ORIG_EndTask = 0x5C4A90;
	static const uint32_t ORIG_RingTimerTask = 0x5C1390;
	static const uint32_t ORIG_RingTask = 0x5C16B0;
	static const uint32_t ORIG_ShatterTask = 0x5C2260;
	static const uint32_t ORIG_MountSpawnerTask = 0x5C2DE0;
	static const uint32_t ORIG_MountTask = 0x5C2E90;
	static const uint32_t ORIG_GroundRingTask = 0x5C4140;
	static const uint32_t ORIG_PuffTask = 0x5C42A0;
	static const uint32_t ORIG_MistTask = 0x5C4550;
	static const uint32_t ORIG_IcicleTask = 0x5C4900;
	static const uint32_t ORIG_CreatureTask = 0x5C4AD0;
	static const uint32_t ORIG_SparkleTask = 0x5C5950;
	static const uint32_t ORIG_HandGlowTask = 0x5C5BE0;
	static const uint32_t ORIG_SpikeRingTask = 0x5C5D90;
	static const uint32_t ORIG_ShardBurstTask = 0x5C6040;
	static const uint32_t ORIG_OrbitSnowTask = 0x5C62F0;
	static const uint32_t ORIG_GlowSpawnerTask = 0x5C6690;
	static const uint32_t ORIG_GlowRingTask = 0x5C6870;
	static const uint32_t ORIG_GlowIcicleTask = 0x5C69E0;
	static const uint32_t ORIG_SnowfallTask = 0x5C6BB0;
	static const uint32_t ORIG_HaloTask = 0x5C6FF0;
	static const uint32_t ORIG_BlizzardTask = 0x5C71B0;
	static const uint32_t ORIG_IceBlockTask = 0x5C77C0;
	static const uint32_t ORIG_DustTask = 0x5C7CA0;

#pragma pack(push, 1)
	struct Node24
	{
		TaskNode hdr;
		int16_t c;
		int16_t e;
		int16_t f10, f12, f14, f16;
		int16_t f18, f1A;
		int16_t f1C, f1E;
		int16_t f20, f22;
	};
	struct Rec
	{
		uint32_t mask;
		int16_t age;
		int16_t size;
		int16_t x, y, z;
		int16_t w0E;
		int16_t vx, vy, vz;
		int16_t w16;
	};
#pragma pack(pop)
	static_assert(sizeof(Node24) == 0x24, "sub-queue node is 0x24 bytes");
	static_assert(sizeof(Rec) == 0x18, "sprite record is 0x18 bytes");
}
}

#ifdef FF8_FX_HELD
#include "mag185_shiva_held.h"
#endif

namespace ff8fx
{
namespace s185
{
	using namespace eng;

	inline Rec *RecA0(int i) { return (Rec *)(PoolA0() + 0x18 * i); }
	inline Rec *RecA4(int i) { return (Rec *)(PoolA4() + 0x18 * i); }

	static Node24 *LinkTask(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&SubQueue(), task_fn);
		n->c = 0;
		n->e = 0;
		return n;
	}

	static void ResetRotation(void *m)
	{
		uint32_t *d = (uint32_t *)m;
		d[3] = 0;
		d[2] = 0;
		d[1] = 0;
		d[0] = 0;
		*(uint16_t *)((uint8_t *)m + 0x10) = 0x1000;
		*(uint16_t *)((uint8_t *)m + 8) = 0x1000;
		*(uint16_t *)((uint8_t *)m + 0) = 0x1000;
	}

	static void PlaneDistance(const int16_t *a, int16_t *out)
	{
		int32_t d = mul32(a[2], out[2]) + mul32(a[1], out[1]);
		d = d + mul32(a[0], out[0]);
		out[3] = (int16_t)-(d >> 12);
	}

	static void NodeMatrix(Mat4x3 *m, const int16_t *angles, bool zyx, int32_t tx, int32_t ty, int32_t tz, int32_t sx, int32_t sy, int32_t sz)
	{
		if (zyx) ComposeZYXRotationMatrix(angles, m);
		else BuildRotationMatrixFromAngles(angles, m);
		m->t[0] = tx;
		m->t[1] = ty;
		m->t[2] = tz;
		int32_t s[3] = { sx, sy, sz };
		Scale3DMatrix(m, s);
	}

	static void PrimModel(uint32_t model, bool fade, int32_t fade_value)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (fade)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade_value;
		}
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
		FieldFree(0x58);
	}

	static void SpriteAt(uint8_t *h, int16_t age, const int16_t *pos, int16_t size)
	{
		*(int16_t *)(h + 4) = age;
		TransformCameraByShadowRotation(pos, size, -((int32_t)size >> 4));
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static int FreeRecord(uint8_t *pool)
	{
		for (int i = 0; i < 150; i++)
			if (*(uint32_t *)(pool + 0x18 * i) == 0) return i;
		return -1;
	}

	static uint32_t __cdecl SequenceTick(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		FX_HELD(held_note_master();)
		if (*(uint8_t *)&n->c & 1) Arena() = MB() + 0x10000;
		else Arena() = MB() + 0x18000;
		int left = ExecuteTaskQueue(&SubQueue());
		n->c++;
		return left ? 0 : TASK_END;
	}

	static uint32_t __cdecl TimelineTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ComposeAffineTransform(&Camera(), &RootMatrix(), &Frame());
		uint32_t flags = var<uint32_t>(us(0x1D96A9C));
		if (flags & 0x201)
		{
			if (flags & 1) return 0;
			if (LoadState() < 0) return 0;
		}
		if (n->c == 0)
		{
			CameraScriptStart(us(0xD32604), &RootMatrix(), Creature(), &SubQueue(), 0x30);
			BdTransSummonStream(us(0xE9AD20), us(0x22BD0E4));
			uint32_t v = ClaimVoiceSlot((const void *)us(0xD32344), 1, 0x80);
			VoiceSlot() = v;
			LinkTask(us(ORIG_EndTask));
		}
		int32_t c = n->c;
		if (c < 30)
		{
			if (c == 1)
			{
				BdPlaySE(us(0xD32488), 0, 0x80);
				LinkTask(us(ORIG_RingTimerTask));
				CharacterLoad(0x21F, MB() + 0x10000);
			}
			else if (c == 15)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0x10000);
				CharacterLoad(0x220, MB());
			}
			else if (c == 19) LinkTask(us(ORIG_PuffTask));
			else if (c == 22) ShatterSpawn();
		}
		else if (c - 30 >= 8)
		{
			int32_t e = c - 38;
			if (e < 24)
			{
				if (e == 2) BdPlaySummonStream(0x80, 0, 0x60);
				else if (e == 5) LinkTask(us(ORIG_MistTask));
				else if (e == 4) IcicleSpawn();
				else if (e == 3)
				{
					if (LoadState() < 0) return 0;
					LinkTask(us(ORIG_CreatureTask));
					LinkTask(us(ORIG_MountSpawnerTask));
				}
			}
			else if ((e -= 24) < 30)
			{
				if (e == 0)
				{
					if (var<uint8_t>(us(0x22BD0E4))) CharacterLoad(0x221, SummonData());
				}
				else if (e == 10)
				{
					if (LoadState() < 0) return 0;
					BdTransSummonStream(SummonData(), us(0x22BD0E4));
				}
				else if (e == 15)
				{
					if (var<uint8_t>(us(0x22BD0E4))) CharacterLoad(0x222, SummonData());
				}
			}
			else if ((e -= 30) >= 9 && (e -= 9) >= 1)
			{
				e -= 1;
				if (e < 15)
				{
					if (e == 0)
					{
						BdPlaySE(us(0xD3248C), 0, 0x80);
						BdPlaySummonStream(0x80, 0, 0x60);
					}
				}
				else if ((e -= 15) < 30)
				{
					if (e == 15)
					{
						if (LoadState() < 0) return 0;
						BdTransSummonStream(SummonData(), us(0x22BD0E4));
					}
				}
				else if ((e -= 30) < 50)
				{
					if (e == 0)
					{
						BdPlaySE(us(0xD32490), 0, 0x80);
						CharacterLoad(0x223, MB() + 0x10000);
					}
					else if (e == 44) BdPlaySummonStream(0x80, 0, 0x60);
					else if (e == 10)
					{
						if (LoadState() < 0) return 0;
						QueueTIMUpload(MB() + 0x10000);
						CharacterLoad(0x224, SummonData());
					}
				}
				else if ((e -= 50) >= 14 && (e -= 14) >= 14 && (e -= 14) < 50)
				{
					if (e == 0)
					{
						if (LoadState() < 0) return 0;
						BdTransSummonStream(us(0xE9AD20), us(0x22BD0E4));
					}
					else if (e == 24)
					{
						BdPlaySE(0xD32494, 0, 0x80);
						BdPlaySummonStream(0x80, 0, 0x60);
					}
					else if (e == 47)
					{
						if (VoiceSlotBusy(VoiceSlot())) ReleaseVoiceSlot(VoiceSlot());
					}
				}
			}
		}
		int16_t cc = n->c;
		if (cc <= 0x10) SetScreenFlash((uint32_t)shl32(cc, 7), 0);
		else if (cc >= 0x103) SetScreenFlash((uint32_t)shl32(0x113 - (int32_t)cc, 7), 0);
		if (n->c == 0x109)
		{
			uint8_t *t = *(uint8_t **)(CastCtx() + 4);
			ApplyActionResultToTargets(*(uint32_t *)(t + 8), t[0x10]);
		}
		n->c++;
		if (n->c <= 0x113) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	static uint32_t __cdecl EndTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 1) WaitAnimSeq((void *)MB(), &n->e);
		if (DoneFlags() == 3) n->c++;
		return n->e ? TASK_END : 0;
	}

	static void RingGeometry(int32_t n, uint8_t *buf, uint8_t *g128, uint8_t *g190)
	{
		for (int k = 0; k < 0x4A; k++) *(int16_t *)(buf + 6 + 8 * k) = 0;
		int32_t k2 = shl32(0x24 - n, 1);
		const int16_t *A = (const int16_t *)us(0xD20FC4);
		const int16_t *B = (const int16_t *)(us(0xD20FC4) + 8 * k2);
		int16_t *Q = (int16_t *)(buf + 8 * k2);
		int32_t count = n + 1;
		for (int32_t i = 0; i < count; i++, A += 8, B += 8, Q += 8)
		{
			int32_t dx = (int32_t)A[4] - A[0], dy = (int32_t)A[5] - A[1], dz = (int32_t)A[6] - A[2];
			int32_t len2 = (int32_t)((uint32_t)mul32(dz, dz) + (uint32_t)mul32(dy, dy));
			len2 = (int32_t)((uint32_t)len2 + (uint32_t)mul32(dx, dx));
			int32_t half = Sqrt(len2);
			int16_t mx = (int16_t)(((int32_t)B[0] + B[4]) >> 1);
			Q[4] = mx;
			Q[0] = mx;
			int16_t my = (int16_t)(((int32_t)B[5] + B[1]) >> 1);
			Q[5] = my;
			Q[1] = my;
			int16_t mz = (int16_t)(((int32_t)B[6] + B[2]) >> 1);
			Q[6] = mz;
			Q[2] = mz;
			half >>= 1;
			if (i == 0)
			{
				ff8fx::copy<(8)>(g128, g190);
				ff8fx::copy<(8)>(g190, Q);
				int16_t lo = *(int16_t *)(g128 + 2), hi = *(int16_t *)(g190 + 2);
				if (hi < lo)
				{
					*(int16_t *)(g128 + 2) = hi;
					*(int16_t *)(g190 + 2) = lo;
				}
			}
			int32_t d[3] = { (int32_t)B[4] - B[0], (int32_t)B[5] - B[1], (int32_t)B[6] - B[2] };
			NormalizeVectorToFixedPoint(d, d);
			Q[0] = (int16_t)(Q[0] + (mul32(half, d[0]) >> 12));
			Q[1] = (int16_t)(Q[1] + (mul32(half, d[1]) >> 12));
			Q[2] = (int16_t)(Q[2] + (mul32(half, d[2]) >> 12));
			Q[7] = 1;
			Q[3] = 1;
			Q[4] = (int16_t)(Q[4] + ((int32_t)(0u - (uint32_t)mul32(half, d[0])) >> 12));
			Q[5] = (int16_t)(Q[5] + ((int32_t)(0u - (uint32_t)mul32(half, d[1])) >> 12));
			Q[6] = (int16_t)(Q[6] + ((int32_t)(0u - (uint32_t)mul32(half, d[2])) >> 12));
		}
		*(int16_t *)(g190 + 6) = (int16_t)k2;
	}

	static uint32_t __cdecl RingTimerTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t arg = n->e >= 0x24 ? 0x24 : (int32_t)n->e;
		RingGeometry(arg, RingVerts(), (uint8_t *)us(0x22BC128), (uint8_t *)us(0x22BC190));
		FX_HELD(held_note_ring_timer(arg);)
		if (n->c == 0)
		{
			int32_t r = CrtRand() % 0x1000;
			for (int i = 0; i < 4; i++)
			{
				Node24 *t = (Node24 *)AddTaskToQueue(&SubQueue(), us(ORIG_RingTask));
				t->c = 0;
				t->e = (int16_t)(1 << i);
				t->f12 = 0;
				t->f10 = 0;
				t->f14 = 0;
				t->f18 = (int16_t)(shl32(i, 10) + r);
				t->f1A = 0x3C;
				t->f1C = 0x1000;
				t->f20 = 0x1800;
				t->f22 = 0;
			}
			var<uint16_t>(us(0x22BC192)) = 0xD8F1;
		}
		if (Paused()) return 0;
		n->e += 2;
		n->c++;
		return n->c >= 0x3C ? TASK_END : 0;
	}

	static void RingDraw(uint32_t verts, int32_t h5c, int32_t h60, bool fade)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x68);
		*(uint32_t *)h = us(0xD215C4);
		*(uint32_t *)(h + 4) = verts;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0x5C) = h5c;
		*(int32_t *)(h + 0x60) = 0x24;
		if (fade)
		{
			*(int32_t *)(h + 0x60) = h60;
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(int32_t *)(h + 0xC) = 0x1000 - shl32(h60, 12) / 36;
		}
		Cursor() = RingRender(h, OT(), 2, Cursor());
		FieldFree(0x68);
	}

	static int32_t RingFadeSteps(int32_t c) { int32_t v = 0x48 - shl32(c, 1); return v < 0 ? 0 : v; }

	static uint32_t __cdecl RingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t count = 0;
		FX_HELD(held_note_node(n);)
		int16_t ang[4] = { 0, n->f18, 0, 0 };
		Mat4x3 m, w;
		NodeMatrix(&m, ang, true, n->f10, n->f12, n->f14, n->f1C, n->f20, n->f1C);
		ComposeAffineTransform(&Frame(), &m, &w);
		GteSetRotMatrix(&w);
		GteSetTransVector(&w);
		if (n->c < 0x3C)
		{
			int16_t c = n->c;
			RingDraw((uint32_t)RingVerts(), n->f22 >= 0x24 ? 0x24 : (int32_t)n->f22, RingFadeSteps(c), c >= 0x12);
			count = 1;
		}
		ComposeAffineTransform(&RootMatrix(), &m, &w);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = us(0xD209F4);
		*(uint16_t *)(h + 0x24) = 0;
		if (n->c < 0x1D)
		{
			int16_t v[4] = { 0, 0, 0, 0 };
			MatrixMultiplyVector(&w, (const int16_t *)us(0x22BC190), v);
			v[0] = (int16_t)(v[0] + (int16_t)w.t[0]);
			v[1] = (int16_t)(v[1] + (int16_t)w.t[1]);
			v[2] = (int16_t)(v[2] + (int16_t)w.t[2]);
			*(int16_t *)(h + 4) = (int16_t)(n->c & 7);
			TransformCameraByShadowRotation(v, 0x900, -0x90);
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
			count++;
		}
		*(uint32_t *)h = us(0xD20C7C);
		int32_t mask = n->e;
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecA0(i);
			if (!(r->mask & (uint32_t)mask)) continue;
			FX_HELD(held_note_rec_a0(i, n, r);)
			SpriteAt(h, r->age, &r->x, r->size);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			count++;
		}
		FieldFree(0xB4);
		if (Paused()) return 0;
		n->f18 = (int16_t)(n->f18 + n->f1A);
		n->f22 += 2;
		int16_t c = n->c;
		if (c >= 0 && c <= 0x1E)
		{
			int32_t base = var<int16_t>(us(0x22BC196));
			for (int k = 0; k < 3; k++)
			{
				int i = FreeRecord(PoolA0());
				if (i < 0) break;
				Rec *r = RecA0(i);
				r->mask = (uint32_t)(int32_t)n->e;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x300 + 0x400);
				int32_t rr = CrtRand();
				int32_t idx = rr % (0x4A - base) + base;
				MatrixMultiplyVector(&w, (const int16_t *)(RingVerts() + 8 * idx), &r->x);
				r->x = (int16_t)(r->x + (int16_t)w.t[0]);
				r->y = (int16_t)(r->y + (int16_t)w.t[1]);
				r->z = (int16_t)(r->z + (int16_t)w.t[2]);
				int32_t d[3];
				d[0] = CrtRand() % 0x1000 - 0x800;
				d[1] = CrtRand() % 0x1000 - 0x800;
				d[2] = CrtRand() % 0x1000 - 0x800;
				NormalizeVectorToFixedPoint(d, d);
				int32_t sp = CrtRand() % 40 + 30;
				r->vx = (int16_t)(mul32(sp, d[0]) >> 12);
				r->vy = (int16_t)(mul32(sp, d[1]) >> 12);
				r->vz = (int16_t)(mul32(sp, d[2]) >> 12);
			}
		}
		n->c++;
		if (n->c < 0x3C) return 0;
		return count ? 0 : TASK_END;
	}

	static uint32_t __cdecl PuffTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = us(0xD20AD0);
		*(uint16_t *)(h + 0x24) = 8;
		int32_t count = 0;
		for (int i = 0; i < 100; i++)
		{
			Rec *r = RecA4(i);
			if (!(r->mask & 0x100)) continue;
			FX_HELD(held_note_rec_a4(i, n, r);)
			SpriteAt(h, r->age, &r->x, r->size);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->x = (int16_t)(r->x + r->vx);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 2));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 2));
			count++;
		}
		FieldFree(0xB4);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 0 && c <= 8)
		{
			for (int k = 0; k < 12; k++)
			{
				int i = FreeRecord(PoolA4());
				if (i < 0) break;
				Rec *r = RecA4(i);
				r->mask = 0x100;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x600 + 0x800);
				r->x = (int16_t)(CrtRand() % 0x190 - 0xC8);
				r->y = (int16_t)-(CrtRand() % 0x64);
				r->z = (int16_t)(CrtRand() % 0x190 - 0xC8);
				int16_t *other = (int16_t *)(PoolA0() + 0x18 * i + 8);
				MatrixMultiplyVector(&RootMatrix(), other, other);
				r->x = (int16_t)(r->x + (int16_t)RootT(0));
				r->y = (int16_t)(r->y + (int16_t)RootT(1));
				r->z = (int16_t)(r->z + (int16_t)RootT(2));
				int32_t a = CrtRand() % 0x1000;
				int32_t sp = CrtRand() % 0x15E + 0x10E;
				r->vx = (int16_t)(mul32(ComputeCos(a), sp) >> 12);
				r->vz = (int16_t)(mul32(ComputeSin(a), sp) >> 12);
			}
		}
		n->c++;
		if (n->c < 4) return 0;
		return count ? 0 : TASK_END;
	}

	static uint32_t __cdecl MistTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = us(0xD20AD0);
		*(uint16_t *)(h + 0x24) = 8;
		int32_t count = 0;
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecA0(i);
			if (!(r->mask & 0x200)) continue;
			FX_HELD(held_note_rec_a0(i, n, r);)
			SpriteAt(h, r->age, &r->x, r->size);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->size = (int16_t)(r->size + (int16_t)(r->size >> 6));
			r->x = (int16_t)(r->x + r->vx);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			count++;
		}
		FieldFree(0xB4);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 0 && c <= 0x1E)
		{
			for (int k = 0; k < 6; k++)
			{
				int i = FreeRecord(PoolA0());
				if (i < 0) break;
				Rec *r = RecA0(i);
				r->mask = 0x200;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x480 + 0x380);
				int32_t a = CrtRand() % 0x1000;
				int32_t sp = CrtRand() % 0x1C2 + 0x190;
				r->x = (int16_t)(mul32(ComputeCos(a), sp) >> 12);
				int32_t up = CrtRand() % 0xC8;
				r->y = (int16_t)-up;
				r->z = (int16_t)(mul32(ComputeSin(a), sp) >> 12);
				MatrixMultiplyVector(&RootMatrix(), &r->x, &r->x);
				r->x = (int16_t)(r->x + (int16_t)RootT(0));
				r->y = (int16_t)(r->y + (int16_t)RootT(1));
				r->z = (int16_t)(r->z + (int16_t)RootT(2));
				int32_t sp2 = CrtRand() % 0x46 + 0x1E;
				r->vx = (int16_t)(mul32(ComputeCos(a), sp2) >> 12);
				r->vz = (int16_t)(mul32(ComputeSin(a), sp2) >> 12);
			}
		}
		n->c++;
		if (n->c < 4) return 0;
		return count ? 0 : TASK_END;
	}

	static void IcicleDraw(const Mat4x3 *frame, int16_t angle, int16_t x, int16_t y, int16_t z, int16_t s, int16_t s2, bool fade, int32_t fade_value)
	{
		int16_t ang[4] = { 0, angle, 0, 0 };
		Mat4x3 m;
		NodeMatrix(&m, ang, true, x, y, z, s, s2, s);
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimModel(us(0xD3138C), fade, fade_value);
	}

	static uint32_t __cdecl IcicleTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (Paused()) return 0;
			n->e--;
			return 0;
		}
		FX_HELD(held_note_node(n);)
		IcicleDraw(&Frame(), n->f18, n->f10, n->f12, n->f14, n->f1C, n->f20, n->c >= 8, mul32(n->c - 8, 682));
		if (Paused()) return 0;
		int16_t v = n->f1A;
		n->f18 = (int16_t)(n->f18 + v);
		n->f1A = (int16_t)(v - (int16_t)(v >> 3));
		int16_t g = n->f1E;
		n->f1C = (int16_t)(n->f1C + g);
		n->f1E = (int16_t)(g - (int32_t)g / (int32_t)n->f22);
		n->c++;
		return n->c >= 0xE ? TASK_END : 0;
	}

	static const uint32_t SHARD_BYTES = 0x2800;

	static void ShatterMatrix(const Node24 *n, Mat4x3 *m)
	{
		int16_t ang[4] = { 0, 0, 0, 0 };
		NodeMatrix(m, ang, true, n->f10, n->f12, n->f14, n->f1C, n->f20, n->f1C);
	}

	static uint32_t __cdecl ShatterTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		FX_HELD(held_note_node(n);)
		Mat4x3 m;
		ShatterMatrix(n, &m);
		uint8_t *h = (uint8_t *)FieldAlloc(0xBC);
		*(int32_t *)(h + 0x20) = n->c;
		ComposeAffineTransform(&Frame(), &m, (Mat4x3 *)(h + 0x7C));
		*(uint32_t *)h = us(0xD2F1C4);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		*(uint32_t *)(h + 0x28) = Shards();
		if (n->c >= 0x12)
		{
			*(int32_t *)(h + 0x20) -= 0x12;
			FX_HELD(held_note_shatter_recs();)
			Cursor() = ShatterFly(h, OT(), 2, Cursor());
		}
		else Cursor() = ShatterStatic(h, OT(), 2, Cursor());
		FieldFree(0xBC);
		if (Paused()) return 0;
		n->c++;
		if (n->c < 0x12) return 0;
		return *(uint32_t *)(h + 0x24) ? 0 : TASK_END;
	}

	static uint32_t __cdecl MountSpawnerTask(TaskNode *tn)
	{
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(&SubQueue(), us(ORIG_MountTask));
			t->c = 0;
			t->f10 = 0;
			t->f14 = 0x28;
			t->f12 = 0x724;
			t->f1C = 0x900;
			t->f20 = 0xB00;
			t->f22 = 0x16D;
		}
		if (n->c == 0xE || n->c == 0x1F)
		{
			Node24 *t = (Node24 *)AddTaskToQueue(&SubQueue(), us(ORIG_GroundRingTask));
			t->c = 0;
			t->f12 = 0;
			t->f10 = 0;
			t->f14 = 0x28;
			t->f1C = 0x900;
			t->f20 = 0xB00;
		}
		n->c++;
		return n->c >= 0x59 ? TASK_END : 0;
	}

	static void MountMatrix(const Node24 *n, int32_t c, int32_t y, Mat4x3 *m)
	{
		int16_t ang[4] = { 0, 0x40, 0, 0 };
		ComposeZYXRotationMatrix(ang, m);
		m->t[1] = y;
		m->t[0] = n->f10;
		m->t[2] = n->f14;
		if (c >= 0x34 && c < 0x3E) m->t[2] = (int32_t)n->f14 + 0x46;
		int32_t s[3] = { n->f1C, n->f20, n->f1C };
		Scale3DMatrix(m, s);
	}

	static void MountRise(int32_t plane_y)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x1A8);
		int16_t *p = (int16_t *)(h + 0x58);
		*(uint32_t *)(h + 0x1C) = us(0xD2BA3C);
		*(uint32_t *)(h + 0x24) = 0;
		p[0] = 0;
		p[2] = 0;
		*(int16_t *)(h + 0xB8) = 0;
		*(int16_t *)(h + 0xBA) = (int16_t)0xF000;
		*(int16_t *)(h + 0xBC) = 0;
		p[1] = (int16_t)plane_y;
		PlaneDistance(p, (int16_t *)(h + 0xB8));
		Cursor() = MountRender(h, OT(), 2, Cursor());
		FieldFree(0x1A8);
	}

	static void MountStand()
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = us(0xD2BA3C);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x30;
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
		FieldFree(0x58);
	}

	static int32_t MountShatter(const Mat4x3 *frame, const Mat4x3 *m, int32_t c, bool init, uint32_t records, bool memo)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xBC);
		if (init)
		{
			*(uint32_t *)h = us(0xD2BA3C);
			*(uint32_t *)(h + 0x28) = Shards();
			MountShardInit(h);
		}
		*(uint32_t *)(h + 0x1C) = 0x33;
		*(uint32_t *)(h + 0x14) = 0x33;
		*(int32_t *)(h + 0x20) = c + 0xE;
		ComposeAffineTransform(frame, m, (Mat4x3 *)(h + 0x7C));
		*(uint32_t *)h = us(0xD2BA3C);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x10) = 3;
		*(uint32_t *)(h + 0x18) = 6;
		*(uint32_t *)(h + 0x28) = records;
		FX_HELD(if (memo) held_note_mount_recs();)
		Cursor() = ShardFly(h, OT(), 2, Cursor());
		int32_t left = *(int32_t *)(h + 0x24);
		FieldFree(0xBC);
		return left;
	}

	static uint32_t __cdecl MountTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		FX_HELD(held_note_node(n);)
		Mat4x3 m;
		MountMatrix(n, n->c, n->f12, &m);
		int32_t left = 0;
		if (n->c < 0x40)
		{
			ComposeAffineTransform(&Frame(), &m, &m);
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
		}
		int16_t c = n->c;
		if (c < 5) MountRise((int16_t)((int32_t)mul32(c, 0xA64) / 5 - 0xA64));
		else if (c < 0x40) MountStand();
		else left = MountShatter(&Frame(), &m, c, c == 0x40, Shards(), true);
		if (Paused()) return 0;
		c = n->c;
		if (c < 5) n->f12 = (int16_t)(n->f12 - n->f22);
		else n->f12 = 0;
		n->c = (int16_t)(c + 1);
		if (n->c < 0x59) return 0;
		return left ? 0 : TASK_END;
	}

	static void GroundRingDraw(const Mat4x3 *frame, const Node24 *n, int32_t scroll, bool fade, int32_t fade_value)
	{
		int16_t ang[4] = { 0, 0x40, 0, 0 };
		Mat4x3 m;
		NodeMatrix(&m, ang, true, n->f10, n->f12, n->f14, n->f1C, n->f20, n->f1C);
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x7C);
		*(uint32_t *)(h + 8) = 0;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint32_t *)(h + 0x10) = 0;
		*(uint32_t *)h = us(0xD2D600);
		*(uint32_t *)(h + 0x20) = 0x30;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(int32_t *)(h + 0x14) = scroll;
		if (fade)
		{
			*(int32_t *)(h + 0xC) = fade_value;
			*(uint32_t *)(h + 0x20) = 0xF0;
		}
		Mag151Draw(h, OT(), 3, &Cursor());
		FieldFree(0x7C);
	}

	static bool GroundRingFade(int32_t c, int32_t *v)
	{
		if (c < 4) { *v = shl32(4 - c, 10); return true; }
		if (c >= 8) { *v = shl32(c - 8, 10); return true; }
		return false;
	}

	static uint32_t __cdecl GroundRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		FX_HELD(held_note_node(n);)
		int32_t fv = 0;
		bool fade = GroundRingFade(n->c, &fv);
		GroundRingDraw(&Frame(), n, shl32(n->c, 3), fade, fv);
		if (Paused()) return 0;
		n->c++;
		return n->c > 0xC ? TASK_END : 0;
	}

	static void ModelInit(uint8_t *E, uint8_t *data, uint8_t *buf, int32_t id)
	{
		*(uint16_t *)(E + 0) = 2;
		E[7] = 0;
		*(uint32_t *)(E + 0x28) = 0x808080;
		E[0x61] = 0;
		E[0x60] = 0;
		*(uint8_t **)(E + 0x68) = data + 0xC;
		*(uint8_t **)(E + 0x64) = data + 0xC;
		*(uint32_t *)(E + 0x78) = 0;
		*(uint32_t *)(E + 0x7C) = 0xFFFFFFFF;
		*(uint8_t **)(E + 0x84) = data;
		*(uint32_t *)(E + 0x8C) = 0;
		*(uint8_t **)(data + 0xC) = buf + *(uint32_t *)(buf + 4);
		*(uint8_t **)(data + 0x10) = buf + *(uint32_t *)(buf + 8);
		*(uint8_t **)(data + 0x14) = buf + *(uint32_t *)(buf + 0xC);
		*(uint16_t *)(data + 2) = (uint16_t)id;
		*(uint8_t **)(data + 0x30) = buf + *(uint32_t *)(buf + 0x10);
	}

	static void SetAnim(int32_t id) { PreReadAnimation(Creature() + 0x60, Creature() + 0x6C, id); }

	static void AdvanceModelAnimLoop(uint8_t *E)
	{
		if (ReadAnimation(E + 0x60, E + 0x6C) == 1 && !(E[0] & 1))
			PreReadAnimation(E + 0x60, E + 0x6C, E[0x6C]);
	}

	static void DrawModel(uint8_t *E, const Mat4x3 *frame, uint32_t scratch)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		ComposeAffineTransform(frame, (const Mat4x3 *)(E + 0x40), (Mat4x3 *)h);
		ComputeBonesWorldMatrices(E + 0x60, h);
		*(uint32_t *)(h + 0x30) = var<uint32_t>(us(0x1D969A8));
		*(uint32_t *)(h + 0x24) = scratch;
		*(uint16_t *)(h + 0x34) = 0;
		*(uint16_t *)(h + 0x36) = 0;
		*(uint32_t *)(h + 0x3C) = *(uint32_t *)(E + 0x28);
		*(uint32_t *)(h + 0x48) = 0;
		*(uint16_t *)(h + 0x44) = 0;
		*(uint16_t *)(h + 0x38) = 0x140;
		*(uint16_t *)(h + 0x3A) = 0xD8;
		*(uint32_t *)(h + 0x40) = *(uint32_t *)(E + 0x7C);
		Cursor() = RenderGeometry(*(void **)(E + 0x64), h + 0x20, OT(), 4, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
		FieldFree(0x4C);
	}

	static void ShellDraw(uint8_t *h, uint8_t *E, const Mat4x3 *frame)
	{
		ComputeBonesWorldMatrices(E + 0x60, E + 0x40);
		*(uint32_t *)(h + 0x10) = *(uint32_t *)(E + 0x28);
		*(uint32_t *)(h + 0x14) = *(uint32_t *)(E + 0x7C);
		var<uint32_t>(us(0x22BC154)) = (uint32_t)frame;
		*(uint16_t *)(h + 0x18) = 0;
		Cursor() = ShellRender(*(void **)(E + 0x64), h, OT(), 2, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
	}

	static void ShellPhase(uint8_t *E, const Mat4x3 *frame, uint32_t scratch, bool blit)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x1A8);
		*(int16_t *)(h + 0x58) = 0;
		*(int16_t *)(h + 0x5A) = 0;
		*(int16_t *)(h + 0x5C) = 0;
		*(int16_t *)(h + 0xB8) = 0;
		*(int16_t *)(h + 0xBA) = (int16_t)0xF000;
		*(int16_t *)(h + 0xBC) = 0;
		PlaneDistance((int16_t *)(h + 0x58), (int16_t *)(h + 0xB8));
		*(uint32_t *)(h + 4) = scratch;
		if (blit) TextureBlit(E, 1, 0);
		ShellDraw(h, E, frame);
		FieldFree(0x1A8);
	}

	static uint32_t __cdecl CreatureTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *E = Creature();
		if (n->c == 0)
		{
			ResetRotation((void *)us(0x22BD058));
			var<uint16_t>(us(0x22BD068)) = 0x1000;
			var<uint16_t>(us(0x22BD060)) = 0x1000;
			var<uint16_t>(us(0x22BD058)) = 0x1000;
			var<int32_t>(us(0x22BD070)) = 0;
			var<int32_t>(us(0x22BD06C)) = 0;
			var<int32_t>(us(0x22BD074)) = 0;
			ModelInit(E, (uint8_t *)us(0x22BCFD0), (uint8_t *)MB(), 3);
			n->f20 = 0x4E2;
			n->f22 = (int16_t)0xFF06;
		}
		int32_t c = n->c;
		if (c < 5)
		{
			if (!Paused() && c == 0) SetAnim(0);
			var<int32_t>(us(0x22BD070)) = n->f20;
			ShellPhase(E, &Frame(), var<uint32_t>(us(0xD324B0)), true);
			FX_HELD(held_note_creature(true);)
			if (!Paused())
			{
				n->f20 = (int16_t)(n->f20 + n->f22);
				FX_HELD(held_note_shell_next(c, n);)
			}
		}
		else if (c - 5 < 0x36)
		{
			var<int32_t>(us(0x22BD070)) = 0;
			if (c - 5 >= 0x30) TextureBlit(E, 1, var<int32_t>(us(0xD3244C) + 4 * (c - 5)));
			DrawModel(E, &Frame(), var<uint32_t>(us(0xD324B0)));
			FX_HELD(held_note_creature(false);)
		}
		else
		{
			bool draw = true, advance = true;
			int32_t e = c - 0x3B;
			if (e < 0x50)
			{
				if (!Paused())
				{
					switch (e)
					{
					case 0: SetAnim(1); LinkTask(us(ORIG_SparkleTask)); break;
					case 1: CameraShake(0, 1, 8, 0xFF); break;
					case 0x2C: TextureBlit(E, 0, 1); break;
					case 0x2E: TextureBlit(E, 0, 0); break;
					case 0x2B:
					{
						Node24 *t = (Node24 *)AddTaskToQueue(&SubQueue(), us(ORIG_HandGlowTask));
						t->c = 0;
						t->f1E = 0x23;
						t->f1C = 0x23;
						t->f22 = 0x4CC;
						break;
					}
					case 0x31: LinkTask(us(ORIG_SpikeRingTask)); break;
					case 0x3A: LinkTask(us(ORIG_ShardBurstTask)); break;
					case 0x32: LinkTask(us(ORIG_OrbitSnowTask)); break;
					default: break;
					}
				}
			}
			else if ((e -= 0x50) < 6) advance = false;
			else if ((e -= 6) < 5) { }
			else if ((e -= 5) < 0x31)
			{
				if (!Paused())
				{
					switch (e)
					{
					case 0: SetAnim(2); break;
					case 1: LinkTask(us(ORIG_GlowSpawnerTask)); break;
					case 0xE:
					{
						Node24 *t = (Node24 *)AddTaskToQueue(&SubQueue(), us(ORIG_HaloTask));
						t->c = 0;
						t->f10 = 0;
						t->f12 = 0;
						t->f1E = 0xAAA;
						t->f1C = 0xAAA;
						t->f14 = (int16_t)0xF63C;
						t->f22 = 0x755;
						t->f20 = 0x755;
						break;
					}
					case 0xD:
					{
						Node24 *t = (Node24 *)AddTaskToQueue(&SubQueue(), us(ORIG_BlizzardTask));
						t->c = 0;
						t->f10 = 0;
						t->f12 = 0;
						t->f14 = (int16_t)0xF63C;
						break;
					}
					case 0x12: IceBlockSpawn(); break;
					case 0x2E: LinkTask(us(ORIG_DustTask)); break;
					default: break;
					}
				}
			}
			else draw = false;
			if (draw)
			{
				DrawModel(E, &Frame(), var<uint32_t>(us(0xD324B0)));
				FX_HELD(held_note_creature(false);)
				if (advance && !Paused())
				{
					AdvanceModelAnimLoop(E);
					FX_HELD(held_note_creature_advanced();)
				}
			}
		}
		if (Paused()) return 0;
		n->c++;
		FX_HELD(held_note_creature_next(n);)
		if (n->c < 0xC7) return 0;
		DoneFlags() |= 1;
		return TASK_END;
	}

	static uint32_t __cdecl SparkleTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = us(0xD208B0);
		*(uint16_t *)(h + 0x24) = 0;
		int32_t count = 0;
		*(uint32_t *)(h + 0x14) = 0x1000;
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecA0(i);
			if (!(*(uint8_t *)&r->mask & 1)) continue;
			FX_HELD(held_note_rec_a0(i, n, r);)
			SpriteAt(h, r->age, &r->x, r->size);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->size = (int16_t)(r->size - (int16_t)(r->size >> 5));
			count++;
		}
		FieldFree(0xB4);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 0 && c <= 0x29)
		{
			int32_t bones = **(uint8_t **)var<uint8_t *>(us(0x22BD07C));
			for (int k = 0; k < 4; k++)
			{
				int i = FreeRecord(PoolA0());
				if (i < 0) break;
				Rec *r = RecA0(i);
				r->mask = 1;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x600 + 0x180);
				int32_t frac = CrtRand() % 0x1000;
				int32_t bone = CrtRand() % bones;
				GetEffectSpawnPosition(Creature(), bone, frac, &r->x);
				MatrixMultiplyVector(&RootMatrix(), &r->x, &r->x);
				r->x = (int16_t)(r->x + (int16_t)(CrtRand() % 0x76C + RootT(0) - 0x3B6));
				r->y = (int16_t)(r->y + (int16_t)(CrtRand() % 0x76C + RootT(1) - 0x3B6));
				r->z = (int16_t)(r->z + (int16_t)(CrtRand() % 0x76C + RootT(2) - 0x3B6));
			}
		}
		n->c++;
		if (n->c < 4) return 0;
		return count ? 0 : TASK_END;
	}

	static void HandGlowDraw(int16_t scale, bool fade, int32_t fade_value)
	{
		TransformCameraByShadowRotation(MidWorld(), scale, -((int32_t)scale >> 3));
		PrimModel(us(0xD31A74), fade, fade_value);
	}

	static uint32_t __cdecl HandGlowTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c < 0x2F)
		{
			int16_t a[4], b[4];
			GetEffectSpawnPosition(Creature(), 0x1A, 0x1800, a);
			GetEffectSpawnPosition(Creature(), 0x20, 0x1800, b);
			Mid()[0] = (int16_t)(((int32_t)b[0] + a[0]) / 2);
			Mid()[1] = (int16_t)(((int32_t)b[1] + a[1]) / 2);
			Mid()[2] = (int16_t)(((int32_t)b[2] + a[2]) / 2);
			MatrixMultiplyVector(&RootMatrix(), Mid(), MidWorld());
			MidWorld()[0] = (int16_t)(MidWorld()[0] + (int16_t)RootT(0));
			MidWorld()[1] = (int16_t)(MidWorld()[1] + (int16_t)RootT(1));
			MidWorld()[2] = (int16_t)(MidWorld()[2] + (int16_t)RootT(2));
		}
		FX_HELD(held_note_node(n);)
		HandGlowDraw(n->f1C, n->c >= 0x32, mul32(n->c - 0x32, 682));
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c < 0x24) n->f1C = (int16_t)(n->f1C + n->f1E);
		if (c >= 0x2E)
		{
			int16_t v = n->f22;
			n->f1C = (int16_t)(n->f1C + v);
			n->f22 = (int16_t)(v - (int32_t)v / 8);
		}
		n->c = (int16_t)(c + 1);
		return n->c >= 0x38 ? TASK_END : 0;
	}

	static int32_t SpikeFade(int32_t age) { return age < 6 ? 0x1000 - mul32(age, 682) : mul32(age - 6, 682); }

	static void SpikeRingSetup(uint8_t *s)
	{
		int32_t *v = (int32_t *)(s + 0x1C);
		v[0] = MidWorld()[0];
		v[1] = MidWorld()[1];
		v[2] = MidWorld()[2];
		TransformVectorBy3x3Matrix(&Camera(), v, v);
		v[0] += Camera().t[0];
		v[1] += Camera().t[1];
		v[2] += Camera().t[2];
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 0) = 0;
		GteSetTransVector((const Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x2C) = 0x500;
		*(int32_t *)(s + 0x30) = 0;
	}

	static void SpikeRingOne(uint8_t *h, uint8_t *s, int16_t angle, int16_t size, int32_t fade_value)
	{
		*(int16_t *)(s + 4) = angle;
		*(int32_t *)(s + 0x28) = size;
		BuildRotationMatrixFromAngles((const int16_t *)s, (Mat4x3 *)(s + 8));
		Scale3DMatrix((Mat4x3 *)(s + 8), (const int32_t *)(s + 0x28));
		GteSetRotMatrix((const Mat4x3 *)(s + 8));
		*(uint32_t *)(h + 0x1C) = 0x33;
		*(int32_t *)(h + 0xC) = fade_value;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
	}

	static uint32_t __cdecl SpikeRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		uint8_t *s = (uint8_t *)FieldAlloc(0x38);
		*(uint32_t *)h = us(0xD3048C);
		*(uint32_t *)(h + 8) = 0;
		int32_t count = 0;
		SpikeRingSetup(s);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecA0(i);
			if (!(*(uint8_t *)&r->mask & 2)) continue;
			FX_HELD(held_note_rec_a0(i, n, r);)
			SpikeRingOne(h, s, r->vz, r->size, SpikeFade(r->age));
			if (Paused()) continue;
			r->age++;
			if (r->age < 0xC)
			{
				r->vz = (int16_t)(r->vz + r->w16);
				count++;
			}
			else r->mask = 0;
		}
		FieldFree(0x38);
		FieldFree(0x58);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 0 && c <= 0x17)
		{
			for (int k = 0; k < 1; k++)
			{
				int i = FreeRecord(PoolA0());
				if (i < 0) break;
				Rec *r = RecA0(i);
				r->mask = 2;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x130 + 0x140);
				r->vz = (int16_t)(CrtRand() % 0x1000);
				int16_t spin = (int16_t)(CrtRand() % 0x37 + 0x14);
				r->w16 = spin;
				if (*(uint8_t *)&r->vz & 1) r->w16 = (int16_t)-spin;
			}
		}
		n->c++;
		if (n->c < 0xC) return 0;
		return count ? 0 : TASK_END;
	}

	static void ShardBurstOne(const Mat4x3 *frame, uint8_t *h, uint8_t *s, int16_t a1, int16_t a2, int16_t size, int16_t age)
	{
		*(int16_t *)(s + 2) = a1;
		*(int16_t *)(s + 4) = a2;
		int32_t *sc = (int32_t *)(s + 0x28);
		sc[2] = size;
		sc[1] = size;
		sc[0] = size;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		Scale3DMatrix((Mat4x3 *)(s + 8), sc);
		*(int32_t *)(s + 0x1C) = Mid()[0];
		*(int32_t *)(s + 0x20) = Mid()[1];
		*(int32_t *)(s + 0x24) = Mid()[2];
		ComposeAffineTransform(frame, (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetRotMatrix((const Mat4x3 *)(s + 8));
		GteSetTransVector((const Mat4x3 *)(s + 8));
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (age < 6)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = 0x1000 - mul32(age, 682);
		}
		Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
	}

	static uint32_t __cdecl ShardBurstTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		uint8_t *s = (uint8_t *)FieldAlloc(0x38);
		*(uint32_t *)h = us(0xD309A4);
		*(uint32_t *)(h + 8) = 0;
		int32_t count = 0;
		*(int16_t *)s = 0;
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecA0(i);
			if (!(*(uint8_t *)&r->mask & 4)) continue;
			FX_HELD(held_note_rec_a0(i, n, r);)
			ShardBurstOne(&Frame(), h, s, r->vy, r->vz, r->size, r->age);
			if (Paused()) continue;
			r->age++;
			if (r->age < 0xA)
			{
				int16_t d = r->w16;
				r->size = (int16_t)(r->size - d);
				r->w16 = (int16_t)(d - (int32_t)d / 6);
				count++;
			}
			else r->mask = 0;
		}
		FieldFree(0x38);
		FieldFree(0x58);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 0 && c <= 0x10 && (int32_t)c % 4 == 1)
		{
			for (int k = 0; k < 1; k++)
			{
				int i = FreeRecord(PoolA0());
				if (i < 0) break;
				Rec *r = RecA0(i);
				r->mask = 4;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0xA00 + 0x1800);
				r->vy = (int16_t)(CrtRand() % 0x1000);
				r->vz = (int16_t)(CrtRand() % 0x800);
				r->w16 = (int16_t)((int32_t)r->size / 5);
			}
		}
		n->c++;
		if (n->c < 0xA) return 0;
		return count ? 0 : TASK_END;
	}

	static void OrbitSetup(const Mat4x3 *frame, uint8_t *s)
	{
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = (int16_t)0xFF80;
		*(int16_t *)(s + 4) = 0x200;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 0x10));
		*(int32_t *)(s + 0x24) = Mid()[0];
		*(int32_t *)(s + 0x28) = Mid()[1];
		*(int32_t *)(s + 0x2C) = Mid()[2];
		ComposeAffineTransform(frame, (const Mat4x3 *)(s + 0x10), (Mat4x3 *)(s + 0x10));
		GteSetRotMatrixCtrl((const Mat4x3 *)us(SPRITE_MAT));
		GteSetLightMatrix(s + 0x10);
		GteSetBackColorFromTrans(s + 0x10);
	}

	static void SpriteMatTrans(int16_t size, int32_t *dir)
	{
		int32_t *t = (int32_t *)(us(SPRITE_MAT) + 0x14);
		GteReadMAC123(t);
		NormalizeVectorToFixedPoint(t, dir);
		int32_t k = -((int32_t)size >> 3);
		t[0] = t[0] + (mul32(dir[0], k) >> 12);
		t[1] = t[1] + (mul32(dir[1], k) >> 12);
		t[2] = t[2] + (mul32(dir[2], k) >> 12);
	}

	static void OrbitOne(uint8_t *h, uint8_t *s, int16_t radius, int16_t height, int16_t angle, int16_t size, int16_t age)
	{
		*(int16_t *)(s + 8) = (int16_t)(mul32(ComputeCos(angle), radius) >> 12);
		*(int16_t *)(s + 0xA) = height;
		*(int16_t *)(s + 0xC) = (int16_t)(mul32(ComputeSin(angle), radius) >> 12);
		GteLoadV0(s + 8);
		GteMVMVA_LightV0Bk();
		GteSetRotDiag(size);
		*(int16_t *)(h + 4) = age;
		SpriteMatTrans(size, (int32_t *)(s + 0x30));
		GteSetTransVectorCtrl((const Mat4x3 *)us(SPRITE_MAT));
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static uint32_t __cdecl OrbitSnowTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		*(uint32_t *)h = us(0xD20E10);
		*(uint16_t *)(h + 0x24) = 8;
		int32_t count = 0;
		OrbitSetup(&Frame(), s);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecA0(i);
			if (!(*(uint8_t *)&r->mask & 8)) continue;
			FX_HELD(held_note_rec_a0(i, n, r);)
			OrbitOne(h, s, r->x, r->y, r->z, r->size, r->age);
			if (Paused()) continue;
			int16_t age = (int16_t)(r->age + 1);
			r->age = age;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			if (age <= 8) r->x = (int16_t)(r->x - r->vx);
			r->z = (int16_t)(r->z + r->vz);
			r->vz = (int16_t)(r->vz + (int16_t)(r->vz >> 3));
			r->y = (int16_t)(r->y - r->vy);
			count++;
		}
		FieldFree(0x40);
		FieldFree(0xB4);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 0 && c <= 0x10)
		{
			for (int k = 0; k < 4; k++)
			{
				int i = FreeRecord(PoolA0());
				if (i < 0) break;
				Rec *r = RecA0(i);
				r->mask = 8;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x180 + 0x200);
				int32_t a = CrtRand() % 0x1000;
				int32_t rad = CrtRand() % 0x258 + 0x2BC;
				r->x = (int16_t)rad;
				r->z = (int16_t)a;
				int32_t h2 = CrtRand() % 0x320;
				r->y = (int16_t)(h2 - 0x190);
				r->vx = (int16_t)((rad - 0x258) / 8);
				r->vz = (int16_t)(CrtRand() % 0x5A + 0x28);
				int32_t d = CrtRand() % 10;
				r->vy = (int16_t)((int32_t)r->y / (d + 0xE));
			}
		}
		n->c++;
		if (n->c < 4) return 0;
		return count ? 0 : TASK_END;
	}

	static uint32_t __cdecl GlowSpawnerTask(TaskNode *tn)
	{
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			int16_t y = Mid()[1];
			int16_t x = Mid()[0];
			int16_t z = Mid()[2];
			y = (int16_t)(y - 0x96);
			n->f10 = x;
			n->f12 = y;
			n->f14 = z;
			n->f1C = 0x1000;
			n->f1E = 0x155;
			n->f20 = 0;
			n->f22 = 0x800;
			Node24 *t = (Node24 *)AddTaskToQueue(&SubQueue(), us(ORIG_GlowRingTask));
			ff8fx::copy<(4)>(&t->f10, &n->f10);
			t->c = 0;
			ff8fx::copy<(4)>(&t->f14, &n->f14);
			t->f18 = (int16_t)(CrtRand() % 0x1000);
			int32_t r = CrtRand() % 0x1E;
			t->f1C = n->f1C;
			t->f22 = n->f22;
			var<uint32_t>(us(0xD32508)) = (uint32_t)t;
			t->f1A = (int16_t)(-0x50 - r);
			t->f1E = n->f1E;
			t->f20 = n->f20;
			t = (Node24 *)AddTaskToQueue(&SubQueue(), us(ORIG_SnowfallTask));
			ff8fx::copy<(4)>(&t->f10, &n->f10);
			t->c = 0;
			ff8fx::copy<(4)>(&t->f14, &n->f14);
			t->f18 = (int16_t)(CrtRand() % 0x1000);
			t->f1A = (int16_t)(CrtRand() % 0x32 + 0x3C);
		}
		if (n->c == 1)
		{
			for (uint32_t tab = us(0xD324F8); tab < us(0xD32508); tab += 4)
			{
				Node24 *t = (Node24 *)AddTaskToQueue(&SubQueue(), us(ORIG_GlowIcicleTask));
				t->e = var<int16_t>(tab);
				t->c = 0;
				t->f10 = n->f10;
				t->f12 = n->f12;
				t->f14 = (int16_t)(var<int16_t>(tab + 2) + n->f14);
				t->f18 = (int16_t)(CrtRand() % 0x1000);
				t->f1A = (int16_t)(CrtRand() % 0xBE + 0x78);
				t->f20 = (int16_t)(CrtRand() % 0x1200 + 0x1E00);
			}
		}
		n->c++;
		return n->c >= 0x20 ? TASK_END : 0;
	}

	static void GlowRingDraw(const Mat4x3 *frame, const Node24 *n, int16_t angle, int16_t s, int16_t s2, bool fade, int32_t fade_value)
	{
		int16_t ang[4] = { 0, 0, angle, 0 };
		Mat4x3 m;
		NodeMatrix(&m, ang, true, n->f10, n->f12, n->f14, s, s, s2);
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimModel(us(0xD21B9C), fade, fade_value);
	}

	static uint32_t __cdecl GlowRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		FX_HELD(held_note_node(n);)
		GlowRingDraw(&Frame(), n, n->f18, n->f1C, n->f20, n->c >= 0x16, mul32(n->c - 0x16, 409));
		if (Paused()) return 0;
		n->f1C = (int16_t)(n->f1C + n->f1E);
		n->f18 = (int16_t)(n->f18 + n->f1A);
		if (n->f1C >= 0x7000) n->f1C = 0x7000;
		n->f20 = (int16_t)(n->f20 + n->f22);
		if (n->f20 >= 0x7000) n->f20 = 0x7000;
		n->c++;
		return n->c >= 0x20 ? TASK_END : 0;
	}

	static void GlowIcicleDraw(const Mat4x3 *frame, const Node24 *n, int16_t angle, int16_t s, bool fade, int32_t fade_value)
	{
		int16_t ang[4] = { 0x400, angle, 0, 0 };
		Mat4x3 m;
		NodeMatrix(&m, ang, false, n->f10, n->f12, n->f14, s, n->f20, s);
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		PrimModel(us(0xD3138C), fade, fade_value);
	}

	static uint32_t __cdecl GlowIcicleTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (Paused()) return 0;
			n->e--;
			return 0;
		}
		if (n->c == 0)
		{
			int32_t r = CrtRand() % 0x800;
			int32_t w = (r + (int32_t)*(int16_t *)(var<uint32_t>(us(0xD32508)) + 0x1C)) / 5;
			n->f1E = (int16_t)w;
			n->f1C = (int16_t)w;
		}
		FX_HELD(held_note_node(n);)
		GlowIcicleDraw(&Frame(), n, n->f18, n->f1C, n->c >= 0xC, mul32(n->c - 0xC, 682));
		if (Paused()) return 0;
		int16_t v = n->f1A;
		int16_t g = n->f1E;
		n->f18 = (int16_t)(n->f18 + v);
		n->f1A = (int16_t)(v - (int16_t)(v >> 3));
		n->f1C = (int16_t)(n->f1C + g);
		n->f1E = (int16_t)(g - (int32_t)g / 3);
		n->c++;
		return n->c >= 0x12 ? TASK_END : 0;
	}

	static void SnowfallSetup(const Mat4x3 *frame, const Node24 *n, int16_t angle, uint8_t *s)
	{
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = angle;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = n->f10;
		*(int32_t *)(s + 0x20) = n->f12;
		*(int32_t *)(s + 0x24) = n->f14;
		ComposeAffineTransform(frame, (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetRotMatrixCtrl((const Mat4x3 *)us(SPRITE_MAT));
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static void LitSprite(uint8_t *h, uint8_t *s, const int16_t *pos, int16_t size, int16_t frame, uint32_t dir_off)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotDiag(size);
		*(int16_t *)(h + 4) = frame;
		SpriteMatTrans(size, (int32_t *)(s + dir_off));
		GteSetTransVectorCtrl((const Mat4x3 *)us(SPRITE_MAT));
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static uint32_t __cdecl SnowfallTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x38);
		*(uint32_t *)h = us(0xD20E10);
		int32_t count = 0;
		*(uint16_t *)(h + 0x24) = 0x208;
		FX_HELD(held_note_node(n);)
		SnowfallSetup(&Frame(), n, n->f18, s);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecA0(i);
			if (!(*(uint8_t *)&r->mask & 1)) continue;
			FX_HELD(held_note_rec_a0(i, n, r);)
			LitSprite(h, s, &r->x, r->size, r->age < 8 ? 8 : r->age, 0x28);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 4));
			r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 4));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 4));
			count++;
		}
		FieldFree(0x38);
		FieldFree(0xB4);
		if (Paused()) return 0;
		int16_t v = n->f1A;
		n->f18 = (int16_t)(n->f18 - v);
		n->f1A = (int16_t)(v - (int16_t)(v >> 4));
		uint8_t *s2 = (uint8_t *)FieldAlloc(0x38);
		int16_t c = n->c;
		if (c >= 0 && c <= 0x16)
		{
			for (int k = 0; k < 3; k++)
			{
				int i = FreeRecord(PoolA0());
				if (i < 0) break;
				Rec *r = RecA0(i);
				r->mask = 1;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x360 + 0x180);
				r->x = (int16_t)(CrtRand() % 0xA0 - 0x50);
				r->y = (int16_t)(CrtRand() % 0xA0 - 0x50);
				r->z = (int16_t)-(CrtRand() % 0x50);
				int32_t *d = (int32_t *)(s2 + 0x28);
				d[0] = CrtRand() % 0x800 - 0x400;
				d[1] = CrtRand() % 0x800 - 0x400;
				d[2] = -0x600 - CrtRand() % 0xA00;
				NormalizeVectorToFixedPoint(d, d);
				int32_t sp = CrtRand() % 0x190 + 0xE6;
				r->vx = (int16_t)(mul32(d[0], sp) >> 12);
				r->vy = (int16_t)(mul32(d[1], sp) >> 12);
				r->vz = (int16_t)(mul32(d[2], sp) >> 12);
			}
		}
		FieldFree(0x38);
		n->c++;
		if (n->c < 4) return 0;
		return count ? 0 : TASK_END;
	}

	static void HaloDraw(const Mat4x3 *frame, const Node24 *n, int16_t s, int16_t s2, int32_t scroll, bool fade, int32_t fade_value)
	{
		int16_t ang[4] = { 0, 0x800, 0, 0 };
		Mat4x3 m;
		NodeMatrix(&m, ang, true, n->f10, n->f12, n->f14, s, s2, s);
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x7C);
		*(uint32_t *)(h + 8) = 0;
		*(uint16_t *)(h + 0x18) = 0x40;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint32_t *)(h + 0x10) = 0;
		*(uint32_t *)h = us(0xD2A50C);
		*(uint32_t *)(h + 0x20) = 0x33;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(int32_t *)(h + 0x14) = scroll;
		if (fade)
		{
			*(uint32_t *)(h + 0x20) = 0xF3;
			*(int32_t *)(h + 0xC) = fade_value;
		}
		Mag151Draw(h, OT(), 2, &Cursor());
		FieldFree(0x7C);
	}

	static uint32_t __cdecl HaloTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		FX_HELD(held_note_node(n);)
		HaloDraw(&Frame(), n, n->f1C, n->f20, shl32(n->c, 4), n->c >= 0x16, shl32(n->c - 0x16, 9));
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c < 6)
		{
			int16_t g = n->f1E;
			n->f1C = (int16_t)(n->f1C + g);
			n->f1E = (int16_t)(g - (int32_t)g / 3);
			int16_t g2 = n->f22;
			n->f20 = (int16_t)(n->f20 + g2);
			n->f22 = (int16_t)(g2 - (int32_t)g2 / 3);
		}
		n->c = (int16_t)(c + 1);
		return n->c >= 0x1E ? TASK_END : 0;
	}

	static void BlizzardSetup(const Mat4x3 *frame, const Node24 *n, uint8_t *s)
	{
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = n->f10;
		*(int32_t *)(s + 0x20) = n->f12;
		*(int32_t *)(s + 0x24) = n->f14;
		ComposeAffineTransform(frame, (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetRotMatrixCtrl((const Mat4x3 *)us(SPRITE_MAT));
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static uint32_t __cdecl BlizzardTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x38);
		*(uint32_t *)h = us(0xD20AD0);
		*(uint16_t *)(h + 0x24) = 8;
		int32_t count = 0;
		BlizzardSetup(&Frame(), n, s);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecA0(i);
			if (!(*(uint8_t *)&r->mask & 2)) continue;
			FX_HELD(held_note_rec_a0(i, n, r);)
			LitSprite(h, s, &r->x, r->size, r->age, 0x28);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 3));
			r->vz = (int16_t)(r->vz + (int16_t)(r->vz >> 4));
			count++;
		}
		FieldFree(0x38);
		FieldFree(0xB4);
		if (Paused()) return 0;
		uint8_t *s2 = (uint8_t *)FieldAlloc(0x38);
		int16_t c = n->c;
		if (c >= 0 && c <= 0x12)
		{
			for (int k = 0; k < 5; k++)
			{
				int i = FreeRecord(PoolA0());
				if (i < 0) break;
				Rec *r = RecA0(i);
				r->mask = 2;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0xE00 + 0x600);
				r->x = (int16_t)(CrtRand() % 0x190 - 0xC8);
				r->y = (int16_t)-(CrtRand() % 0x64);
				r->z = (int16_t)(CrtRand() % 0x190 - 0xC8);
				int32_t *d = (int32_t *)(s2 + 0x28);
				d[0] = CrtRand() % 0x2000 - 0x1000;
				d[1] = -(CrtRand() % 0x800);
				d[2] = -0x200 - CrtRand() % 0x400;
				NormalizeVectorToFixedPoint(d, d);
				int32_t sp = CrtRand() % 0x96 + 0x82;
				r->vx = (int16_t)(mul32(sp, d[0]) >> 12);
				r->vy = (int16_t)(mul32(sp, d[1]) >> 12);
				r->vz = (int16_t)(mul32(d[2], sp) >> 12);
			}
		}
		FieldFree(0x38);
		n->c++;
		if (n->c < 4) return 0;
		return count ? 0 : TASK_END;
	}

	static const uint32_t BLOCK_BYTES = 0x4000;

	static void IceBlockMatrix(const Node24 *n, Mat4x3 *m)
	{
		int16_t ang[4] = { 0, 0x800, 0, 0 };
		ComposeZYXRotationMatrix(ang, m);
		int32_t s[3] = { 0x1700, 0x1800, 0x1800 };
		m->t[1] = n->f12;
		m->t[0] = n->f10;
		m->t[2] = n->f14;
		Scale3DMatrix(m, s);
	}

	static void IceBlockBuild(const Mat4x3 *frame, const Mat4x3 *m, int32_t c, uint32_t rec0, uint32_t rec1, bool memo)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xBC);
		*(int32_t *)(h + 0x20) = c;
		ComposeAffineTransform(frame, m, (Mat4x3 *)(h + 0x7C));
		*(uint32_t *)h = us(0xD257CC);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint32_t *)(h + 0x28) = rec0;
		FX_HELD(if (memo) held_note_block0_recs();)
		Arena() = IceBlockRender(h, OT(), 2, Arena());
		*(uint32_t *)h = us(0xD27E6C);
		*(uint32_t *)(h + 0x1C) = 3;
		*(uint32_t *)(h + 0x28) = rec1;
		FX_HELD(if (memo) held_note_block1_recs();)
		Arena() = IceBlockRender(h, OT(), 3, Arena());
		FieldFree(0xBC);
	}

	static void IceBlockWhole(const Mat4x3 *frame, const Mat4x3 *local, int32_t c, int32_t flash_c2, bool flash_frac)
	{
		Mat4x3 m = *local;
		ComposeAffineTransform(frame, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = us(0xD257CC);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		Arena() = RenderPrimModel(h, OT(), 2, Arena());
		FieldFree(0x58);
		if (c > 0x1A) return;
		h = (uint8_t *)FieldAlloc(0x7C);
		*(uint32_t *)h = us(0xD27E6C);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x20) = 3;
		int32_t d2 = flash_c2 - 0x24;
		if (c >= 0x12)
		{
			*(uint32_t *)(h + 0x20) = 0xC3;
			*(int32_t *)(h + 0xC) = shl32(d2, 8);
		}
		*(uint16_t *)(h + 0x18) = 0x40;
		*(int32_t *)(h + 0x14) = flash_frac ? mul32(d2, 6) : shl32(mul32(c - 0x12, 3), 2);
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint32_t *)(h + 0x10) = 0;
		Mag151Draw(h, OT(), 3, &Arena());
		FieldFree(0x7C);
	}

	static void IceBlockShatter(const Mat4x3 *frame, const Mat4x3 *m, int32_t c, uint32_t rec0, bool memo)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xBC);
		*(int32_t *)(h + 0x20) = c - 0x20;
		ComposeAffineTransform(frame, m, (Mat4x3 *)(h + 0x7C));
		*(uint32_t *)h = us(0xD257CC);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		*(uint32_t *)(h + 0x10) = 5;
		*(uint32_t *)(h + 0x14) = 0x18;
		*(uint32_t *)(h + 0x18) = 8;
		*(uint32_t *)(h + 0x28) = rec0;
		FX_HELD(if (memo) held_note_block0_recs();)
		Cursor() = ShardFly(h, OT(), 2, Cursor());
		*(uint32_t *)(h + 0x24) = 0;
		FieldFree(0xBC);
	}

	static uint32_t __cdecl IceBlockTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		FX_HELD(held_note_node(n);)
		Mat4x3 m;
		IceBlockMatrix(n, &m);
		int16_t c = n->c;
		if (c < 0x10) IceBlockBuild(&Frame(), &m, c, MB() + 0x20000, MB() + 0x24000, true);
		else if (c < 0x20) IceBlockWhole(&Frame(), &m, c, shl32(c, 1), false);
		else
		{
			DoneFlags() |= 2;
			IceBlockShatter(&Frame(), &m, c, MB() + 0x20000, true);
		}
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x40 ? TASK_END : 0;
	}

	static void DustSetup(const Mat4x3 *frame, uint8_t *s)
	{
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = 0;
		*(int32_t *)(s + 0x20) = 0x2BC;
		*(int32_t *)(s + 0x24) = (int32_t)0xFFFFE4A8;
		ComposeAffineTransform(frame, (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static void DustOne(uint8_t *h, uint8_t *s, const int16_t *pos, int16_t size, int16_t age)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		int32_t *t = (int32_t *)(s + 0x38), *d = (int32_t *)(s + 0x28);
		*(int16_t *)(h + 4) = age;
		GteReadMAC123(t);
		NormalizeVectorToFixedPoint(t, d);
		int32_t k = -((int32_t)size >> 3);
		t[0] = t[0] + (mul32(d[0], k) >> 12);
		t[1] = t[1] + (mul32(d[1], k) >> 12);
		t[2] = t[2] + (mul32(d[2], k) >> 12);
		GteSetTransFromVec32(t);
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static uint32_t __cdecl DustTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		*(uint32_t *)h = us(0xD208B0);
		*(uint16_t *)(h + 0x24) = 0;
		int32_t count = 0;
		DustSetup(&Frame(), s);
		for (int i = 0; i < 150; i++)
		{
			Rec *r = RecA0(i);
			if (!(*(uint8_t *)&r->mask & 4)) continue;
			FX_HELD(held_note_rec_a0(i, n, r);)
			DustOne(h, s, &r->x, r->size, r->age);
			if (Paused()) continue;
			r->age++;
			if (*(int16_t *)(h + 0x28) < 0) r->mask = 0;
			else count++;
		}
		FieldFree(0x48);
		FieldFree(0xB4);
		if (Paused()) return 0;
		uint8_t *s2 = (uint8_t *)FieldAlloc(0x48);
		(void)s2;
		int16_t c = n->c;
		if (c >= 0 && c <= 0x1A)
		{
			for (int k = 0; k < 3; k++)
			{
				int i = FreeRecord(PoolA0());
				if (i < 0) break;
				Rec *r = RecA0(i);
				r->mask = 4;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0xE00 + 0x300);
				r->x = (int16_t)(CrtRand() % 0x1F40 - 0xFA0);
				r->y = (int16_t)(-0x12C - CrtRand() % 0xDAC);
				r->z = (int16_t)(CrtRand() % 0x1F40 - 0xFA0);
			}
		}
		FieldFree(0x48);
		n->c++;
		if (n->c < 4) return 0;
		return count ? 0 : TASK_END;
	}
}

	void register_mag185_shiva()
	{
		register_port(s185::ORIG_SequenceTick, (void *)s185::SequenceTick, "S185 SequenceTick", 185);
		register_port(s185::ORIG_TimelineTask, (void *)s185::TimelineTask, "S185 TimelineTask", 185);
		register_port(s185::ORIG_EndTask, (void *)s185::EndTask, "S185 EndTask", 185);
		register_port(s185::ORIG_RingTimerTask, (void *)s185::RingTimerTask, "S185 RingTimerTask", 185);
		register_port(s185::ORIG_RingTask, (void *)s185::RingTask, "S185 RingTask", 185);
		register_port(s185::ORIG_ShatterTask, (void *)s185::ShatterTask, "S185 ShatterTask", 185);
		register_port(s185::ORIG_MountSpawnerTask, (void *)s185::MountSpawnerTask, "S185 MountSpawnerTask", 185);
		register_port(s185::ORIG_MountTask, (void *)s185::MountTask, "S185 MountTask", 185);
		register_port(s185::ORIG_GroundRingTask, (void *)s185::GroundRingTask, "S185 GroundRingTask", 185);
		register_port(s185::ORIG_PuffTask, (void *)s185::PuffTask, "S185 PuffTask", 185);
		register_port(s185::ORIG_MistTask, (void *)s185::MistTask, "S185 MistTask", 185);
		register_port(s185::ORIG_IcicleTask, (void *)s185::IcicleTask, "S185 IcicleTask", 185);
		register_port(s185::ORIG_CreatureTask, (void *)s185::CreatureTask, "S185 CreatureTask", 185);
		register_port(s185::ORIG_SparkleTask, (void *)s185::SparkleTask, "S185 SparkleTask", 185);
		register_port(s185::ORIG_HandGlowTask, (void *)s185::HandGlowTask, "S185 HandGlowTask", 185);
		register_port(s185::ORIG_SpikeRingTask, (void *)s185::SpikeRingTask, "S185 SpikeRingTask", 185);
		register_port(s185::ORIG_ShardBurstTask, (void *)s185::ShardBurstTask, "S185 ShardBurstTask", 185);
		register_port(s185::ORIG_OrbitSnowTask, (void *)s185::OrbitSnowTask, "S185 OrbitSnowTask", 185);
		register_port(s185::ORIG_GlowSpawnerTask, (void *)s185::GlowSpawnerTask, "S185 GlowSpawnerTask", 185);
		register_port(s185::ORIG_GlowRingTask, (void *)s185::GlowRingTask, "S185 GlowRingTask", 185);
		register_port(s185::ORIG_GlowIcicleTask, (void *)s185::GlowIcicleTask, "S185 GlowIcicleTask", 185);
		register_port(s185::ORIG_SnowfallTask, (void *)s185::SnowfallTask, "S185 SnowfallTask", 185);
		register_port(s185::ORIG_HaloTask, (void *)s185::HaloTask, "S185 HaloTask", 185);
		register_port(s185::ORIG_BlizzardTask, (void *)s185::BlizzardTask, "S185 BlizzardTask", 185);
		register_port(s185::ORIG_IceBlockTask, (void *)s185::IceBlockTask, "S185 IceBlockTask", 185);
		register_port(s185::ORIG_DustTask, (void *)s185::DustTask, "S185 DustTask", 185);
		FX_HELD(register_mag185_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag185_shiva_held.inc"
#endif
