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

#include "fx_port.h"

namespace ff8fx
{
namespace g327
{
	using namespace eng;

	namespace x
	{
		inline int32_t CrtRand() { return fn<int32_t (__cdecl *)()>(us(0x55CBD2))(); }
		inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(us(0x56D100))(a); }
		inline void ComposeZYXRotationMatrix(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(us(0x56CE30))(angles, out); }
		inline void Scale3DMatrix(Mat4x3 *m, const int32_t *v) { fn<void (__cdecl *)(Mat4x3 *, const int32_t *)>(us(0x56BEF0))(m, v); }
		inline int32_t NormalizeVectorToFixedPoint(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(us(0x56BC50))(in, out); }
		inline uint32_t MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { return fn<uint32_t (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(us(0x56C4F0))(m, in, out); }
		inline int32_t GetRotationBetweenVectors(const void *ref, const int32_t *v, int32_t *axis) { return fn<int32_t (__cdecl *)(const void *, const int32_t *, int32_t *)>(us(0x571480))(ref, v, axis); }
		inline void BuildAxisAngleRotationMatrix(int32_t angle, Mat4x3 *out, const int32_t *axis) { fn<void (__cdecl *)(int32_t, Mat4x3 *, const int32_t *)>(us(0x5714F0))(angle, out, axis); }
		inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(us(0x502170))(entity, bone, frac, out); }
		inline void GetDefaultEffectPosition(void *entity, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(us(0x571400))(entity, out); }
		inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(us(0x571620))(n, points, work); }
		inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(us(0x571690))(n, work, out, t); }
		inline void SetProjection(int32_t h) { fn<void (__cdecl *)(int32_t)>(us(0x56CD00))(h); }
		inline int32_t PreReadAnimation(void *header, void *cmd, int32_t id) { return fn<int32_t (__cdecl *)(void *, void *, int32_t)>(us(0x509440))(header, cmd, id); }
		inline int32_t ReadAnimation(void *header, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(us(0x508F90))(header, cmd); }
		inline uint32_t RenderGeometry(void *geom, void *hdr, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(us(0x5099D0))(geom, hdr, ot, shift, cursor); }
		inline uint32_t DrawShadow(void *entity, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x5088A0))(entity, ot, shift, cursor); }
		inline void BdPlaySE(uint32_t se, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(us(0x501330))(se, a, b); }
		inline void BdPlaySummonStream(int32_t a, int32_t b, int32_t c) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x5018C0))(a, b, c); }
		inline void BdTransSummonStream(uint32_t src, uint32_t state) { fn<void (__cdecl *)(uint32_t, uint32_t)>(us(0x501860))(src, state); }
		inline int32_t VoiceSlotBusy(uint32_t slot) { return fn<int32_t (__cdecl *)(uint32_t)>(us(0x4A2900))(slot); }
		inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(us(0x4A2940))(slot); }
		inline void CameraScriptStart(uint32_t script, const void *frame, void *entity, TaskQueue *q, int32_t n) { fn<void (__cdecl *)(uint32_t, const void *, void *, TaskQueue *, int32_t)>(us(0x63E960))(script, frame, entity, q, n); }
		inline void CameraShake(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(us(0x5712C0))(a, b, c, d); }
		inline void ApplyActionResultToTargets(uint32_t list, uint32_t count) { fn<void (__cdecl *)(uint32_t, uint32_t)>(us(0x506BA0))(list, count); }
		inline void ApplyActionResultToTarget(void *rec) { fn<void (__cdecl *)(void *)>(us(0x506690))(rec); }
		inline void WaitAnimSeq(uint32_t buffer, void *flag) { fn<void (__cdecl *)(uint32_t, void *)>(us(0x508630))(buffer, flag); }
		inline int32_t LoadState() { return fn<int32_t (__cdecl *)()>(us(0x508500))(); }
		inline void CharacterLoad(int32_t id, uint32_t dst) { fn<void (__cdecl *)(int32_t, uint32_t)>(us(0x508480))(id, dst); }
		inline void QueueTIMUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(us(0x505E30))(tim); }
		inline uint32_t SummonData() { return fn<uint32_t (__cdecl *)()>(us(0x571B70))(); }
		inline void AnimSeqText() { fn<void (__cdecl *)()>(us(0x50A750))(); }
		inline void RequestScreenFeedback(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t, int32_t)>(us(0x47CF50))(a, b, c, d, e); }
		inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(us(0x45DE50))(m); }
		inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(us(0x64DDA0))(m); }
		inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(us(0x4607E0))(); }
		inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(us(0x64DE00))(s); }
		inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(us(0x64DD70))(v); }
		inline void GteTransFromMAC() { fn<void (__cdecl *)()>(us(0x64DE40))(); }
		inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(us(0x45DFE0))(a, b, c); }
		inline void GteRTPT() { fn<void (__cdecl *)()>(us(0x45FE10))(); }
		inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E210))(dst); }
		inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(us(0x45E270))(a, b, c); }
		inline void GteAVSZ4() { fn<void (__cdecl *)()>(us(0x45E610))(); }
		inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E3D0))(dst); }
		inline void GteReadSZ3(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E220))(dst); }
		inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(us(0x45E110))(c); }
		inline void GteDPCS() { fn<void (__cdecl *)()>(us(0x45F270))(); }
		inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(us(0x45E360))(dst); }
		inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x45DD60))(r, g, b); }
		inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x45DDA0))(r, g, b); }
		inline void BoneMid(void *entity, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(us(0x58E260))(entity, out); }
		inline uint32_t SliceRender(void *geom, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(us(0x58E880))(geom, h, ot, mode, cursor); }
		inline uint32_t PartyRender(void *anim, void *h, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(us(0x58FB50))(anim, h, ot, shift, cursor); }
		inline uint32_t ClipRender(void *geom, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(us(0x595250))(geom, h, ot, mode, cursor); }
		inline uint32_t MaterialRender(void *geom, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(us(0x5960A0))(geom, h, ot, mode, cursor); }
	}
	using namespace x;

	inline bool Paused() { return (var<uint32_t>(us(0x1D96A9C)) & 0x201) != 0; }
	inline Mat4x3 &RootMatrix() { return var<Mat4x3>(us(0x21FF3A0)); }
	inline Mat4x3 &Frame() { return var<Mat4x3>(us(0x21FF488)); }
	inline TaskQueue &Q1() { return var<TaskQueue>(us(0x21FFD58)); }
	inline TaskQueue &Q2() { return var<TaskQueue>(us(0x21FF458)); }
	inline uint32_t &VoiceSlot() { return var<uint32_t>(us(0x21FF2A8)); }
	inline int32_t Variant() { return var<int32_t>(us(0x21FFA70)); }
	inline uint8_t *CastCtx() { return var<uint8_t *>(us(0x2200DF0)); }
	inline uint32_t MB() { return var<uint32_t>(us(0x2200FB8)); }
	inline uint32_t &Cursor() { return var<uint32_t>(us(0x1D8E054)); }
	inline uint32_t RenderList() { return var<uint32_t>(us(0x1D8E04C)); }
	inline uint32_t OT() { return RenderList() + 0x44; }
	inline uint8_t *PoolB() { return var<uint8_t *>(us(0xCD0398)); }
	inline uint8_t *PoolA() { return var<uint8_t *>(us(0xCD039C)); }
	inline uint32_t VertexScratch() { return var<uint32_t>(us(0xCD03A0)); }
	inline uint8_t *Targets() { return *(uint8_t **)(CastCtx() + 4); }
	inline uint32_t TargetCount() { return Targets()[0x10]; }
	inline uint8_t *TargetRecs() { return *(uint8_t **)(Targets() + 8); }
	inline uint8_t *Entity(uint32_t slot) { return (uint8_t *)(us(0x1D972C0) + 156 * slot); }
	inline uint8_t *Sword(int i) { return (uint8_t *)(us(0x21FF4F0) + 0x9C * i); }
	inline uint8_t *Arm(int j) { return (uint8_t *)(us(0x21FF800) + 0x9C * j); }
	static uint8_t *GIL1() { return (uint8_t *)us(0x21FF760); }
	static uint8_t *GIL2() { return (uint8_t *)us(0x2200F00); }
	static uint8_t *GIL3() { return (uint8_t *)us(0x2200FC0); }
	static const uint32_t STREAM_STATE = 0x21FF398;

	static const uint32_t ORIG_SequenceTick = 0x596B70;
	static const uint32_t ORIG_TimelineTask = 0x592300;
	static const uint32_t ORIG_FeedbackTask = 0x592A00;
	static const uint32_t ORIG_Feedback2Task = 0x592A50;
	static const uint32_t ORIG_FlashInTask = 0x592AE0;
	static const uint32_t ORIG_FlashOutTask = 0x592B60;
	static const uint32_t ORIG_FadeTileTask = 0x592BC0;
	static const uint32_t ORIG_RingTask = 0x592E90;
	static const uint32_t ORIG_RingSpinTask = 0x592F90;
	static const uint32_t ORIG_RingSpin2Task = 0x5930C0;
	static const uint32_t ORIG_FlareTask = 0x593250;
	static const uint32_t ORIG_RiserTask = 0x593600;
	static const uint32_t ORIG_PuffTask = 0x593770;
	static const uint32_t ORIG_SparkTask = 0x5939C0;
	static const uint32_t ORIG_RisingRingSpawnerTask = 0x593C70;
	static const uint32_t ORIG_RisingRingTask = 0x593D20;
	static const uint32_t ORIG_DotTask = 0x593E80;
	static const uint32_t ORIG_SpinnerTask = 0x594210;
	static const uint32_t ORIG_TrailTask = 0x5943D0;
	static const uint32_t ORIG_SwordShardTask = 0x5948A0;
	static const uint32_t ORIG_SwordPuffTask = 0x594CA0;
	static const uint32_t ORIG_SwordClipTask = 0x594FD0;
	static const uint32_t ORIG_SwordDrawTask = 0x595D50;
	static const uint32_t ORIG_Gil1Task = 0x595EE0;
	static const uint32_t ORIG_Gil1DrawTask = 0x5966B0;
	static const uint32_t ORIG_Gil2Task = 0x596710;
	static const uint32_t ORIG_Gil2DrawTask = 0x596800;
	static const uint32_t ORIG_Gil3Task = 0x596970;
	static const uint32_t ORIG_Gil3DrawTask = 0x596A60;
	static const uint32_t ORIG_EndTask = 0x596B30;
	static const uint32_t ORIG_Zan_Task = 0x58DED0;
	static const uint32_t ORIG_Zan_ScreenPrimTask = 0x58E120;
	static const uint32_t ORIG_Zan_CutTask = 0x58E3E0;
	static const uint32_t ORIG_Zan_SliceTask = 0x58E570;
	static const uint32_t ORIG_Zan_PartyClipTask = 0x58F940;
	static const uint32_t ORIG_Masa_Task = 0x590510;
	static const uint32_t ORIG_Masa_ScreenPrimTask = 0x590790;
	static const uint32_t ORIG_Masa_PrimATask = 0x5908D0;
	static const uint32_t ORIG_Masa_PrimBTask = 0x590A10;
	static const uint32_t ORIG_Masa_SparkTask = 0x590B60;
	static const uint32_t ORIG_Masa_DustTask = 0x590F10;
	static const uint32_t ORIG_Exca_Task = 0x591250;
	static const uint32_t ORIG_Poor_Task = 0x5920E0;
	static const uint32_t ORIG_Exca_ScreenPrimTask = 0x591470;
	static const uint32_t ORIG_Exca_BurstTask = 0x5915C0;
	static const uint32_t ORIG_Exca_SmokeTask = 0x591850;
	static const uint32_t ORIG_Exca_StreakTask = 0x591B80;
	static const uint32_t ORIG_Exca_PillarTask = 0x591F50;

#pragma pack(push, 1)
	struct Node24
	{
		TaskNode hdr;
		int16_t c;
		int16_t e;
		int16_t p10, p12, p14, p16;
		int16_t a18, a1A;
		int16_t s1C, s1E;
		int16_t s20, s22;
	};
	struct RecA
	{
		uint32_t mask;
		int16_t age, size;
		int16_t x, y, z, w0E;
		int16_t vx, vy, vz, w16;
		int16_t w18, w1A, w1C, w1E;
	};
	struct RecB
	{
		uint32_t mask;
		int16_t age, size;
		int16_t x, y, z, w0E;
		int16_t vx, vy, vz, w16;
	};
#pragma pack(pop)
	static_assert(sizeof(Node24) == 0x24, "queue node is 0x24 bytes");
	static_assert(sizeof(RecA) == 0x20, "pool A record is 0x20 bytes");
	static_assert(sizeof(RecB) == 0x18, "pool B record is 0x18 bytes");

}
}

#ifdef FF8_FX_HELD
#include "mag327_gilgamesh_held.h"
#endif

namespace ff8fx
{
namespace g327
{

	static const int POOL_N = 200;
	inline RecA *RA(int i) { return (RecA *)(PoolA() + 0x20 * i); }
	inline RecB *RB(int i) { return (RecB *)(PoolB() + 0x18 * i); }

	static int FreeRecord(uint8_t *pool, int stride)
	{
		for (int i = 0; i < POOL_N; i++)
			if (*(uint32_t *)(pool + stride * i) == 0) return i;
		return -1;
	}

	static Node24 *LinkQ1(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), task_fn);
		n->c = 0;
		n->e = 0;
		return n;
	}
	static Node24 *LinkQ2(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q2(), task_fn);
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

	static void HideParty()
	{
		for (uint32_t a = us(0x1D98991); a < us(0x1D98A41); a += 0x2C) var<uint8_t>(a) |= 2;
	}
	static void ShowParty()
	{
		for (uint32_t a = us(0x1D98991); a < us(0x1D98A41); a += 0x2C) var<uint8_t>(a) &= 0xFD;
	}

	static void PlaneDistance(const int16_t *p, int16_t *nrm)
	{
		int32_t d = mul32(p[2], nrm[2]) + mul32(p[1], nrm[1]);
		d = d + mul32(p[0], nrm[0]);
		nrm[3] = (int16_t)-(d >> 12);
	}

	static uint32_t g_vertex_scratch_override = 0;
	static uint32_t ModelScratch() { return g_vertex_scratch_override ? g_vertex_scratch_override : VertexScratch(); }

	struct PrimArgs
	{
		int16_t ang[3];
		int32_t t[3];
		int32_t s[3];
		const Mat4x3 *compose;
		uint32_t model;
		int32_t mode;
		uint32_t h1C;
		bool setC;
		int32_t hC;
		bool screen;
	};

	static void PrimDraw(const PrimArgs &a)
	{
		int16_t ang[4] = { a.ang[0], a.ang[1], a.ang[2], 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = a.t[0];
		m.t[1] = a.t[1];
		m.t[2] = a.t[2];
		int32_t s[3] = { a.s[0], a.s[1], a.s[2] };
		Scale3DMatrix(&m, s);
		if (a.compose) ComposeAffineTransform(a.compose, &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = a.model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = a.h1C;
		if (a.setC) *(int32_t *)(h + 0xC) = a.hC;
		if (a.screen) SetProjection(0x120);
		Cursor() = RenderPrimModel(h, OT(), a.mode, Cursor());
		if (a.screen) SetProjection(var<int16_t>(us(0x1D8E038)));
		FieldFree(0x58);
	}

	static PrimArgs Prim(int16_t a0, int16_t a1, int16_t a2, int32_t tx, int32_t ty, int32_t tz, int32_t sx, int32_t sy, int32_t sz,
		const Mat4x3 *compose, uint32_t model)
	{
		PrimArgs a;
		a.ang[0] = a0; a.ang[1] = a1; a.ang[2] = a2;
		a.t[0] = tx; a.t[1] = ty; a.t[2] = tz;
		a.s[0] = sx; a.s[1] = sy; a.s[2] = sz;
		a.compose = compose;
		a.model = model;
		a.mode = 2;
		a.h1C = 0x33;
		a.setC = false;
		a.hC = 0;
		a.screen = false;
		return a;
	}

	static void SetFade(PrimArgs &a, bool fade, int32_t v)
	{
		if (!fade) return;
		a.h1C = 0xF3;
		a.setC = true;
		a.hC = v;
	}

	static uint32_t __cdecl SequenceTick(TaskNode *tn)
	{
		FX_HELD(held_note_master();)
		int a = ExecuteTaskQueue(&Q1());
		int b = ExecuteTaskQueue(&Q2());
		((Node24 *)tn)->c++;
		return (a | b) ? 0 : TASK_END;
	}

	static void LinkFeedback(int32_t flag, int16_t len)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), flag ? us(ORIG_Feedback2Task) : us(ORIG_FeedbackTask));
		n->c = 0;
		n->e = len;
	}

	static void LinkFlash(uint32_t task_fn)
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), task_fn);
		n->c = 0;
		n->e = 0x10;
		n->a18 = 0x800;
	}

	static void SpawnRings()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_RingTask));
		n->c = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xE4A8;
		n->p14 = 0;
		n->s1C = 0x2400;
		n->s20 = 0x3000;
		n = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_RingSpinTask));
		n->c = 0;
		n->e = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xE4A8;
		n->p14 = 0;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->a1A = 4;
		n->s1C = 0x2000;
		n->s20 = 0x3000;
		n->s22 = 0;
		n = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_RingSpinTask));
		n->c = 0;
		n->e = 1;
		n->p10 = 0;
		n->p12 = (int16_t)0xE4A8;
		n->p14 = 0;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->a1A = 8;
		n->s1C = 0x2000;
		n->s20 = 0x3000;
		n->s22 = 0x600;
		n = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_RingSpin2Task));
		n->c = 0;
		n->e = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xCD38;
		n->p14 = 0;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->a1A = 4;
		n->s1C = 0x3400;
		n->s22 = 0x800;
		n = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_RingSpin2Task));
		n->c = 0;
		n->e = 1;
		n->p10 = 0;
		n->p12 = (int16_t)0xC950;
		n->p14 = 0;
		n->a18 = (int16_t)(CrtRand() % 0x1000);
		n->a1A = -4;
		n->s1C = 0x3000;
		n->s22 = 0x800;
	}

	static void SpawnFlareRow()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_FlareTask));
		n->c = 0;
		n->p10 = 0;
		n->p12 = (int16_t)0xC950;
		n->p14 = 0;
		MatrixMultiplyVector(&RootMatrix(), &n->p10, &n->p10);
		n->p10 = (int16_t)(n->p10 + var<int16_t>(us(0x21FF3B4)));
		n->p12 = (int16_t)(n->p12 + var<int16_t>(us(0x21FF3B8)));
		n->p14 = (int16_t)(n->p14 + var<int16_t>(us(0x21FF3BC)));
		n->s22 = 0x320;
		n->s20 = 0x320;
	}

	static void SpawnRisers()
	{
		int32_t base = CrtRand() % 0x1000;
		for (int32_t k = 0; k < 0x4000; k += 0x1000)
		{
			int32_t a = k / 4 + base;
			Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_RiserTask));
			n->c = 0;
			int32_t s = ComputeSin(a);
			n->p12 = (int16_t)0xC568;
			n->p10 = (int16_t)(mul32(s, 1000) >> 12);
			int32_t co = ComputeCos(a);
			n->a18 = (int16_t)a;
			n->s1C = 0x400;
			n->s22 = 0x5A;
			n->p14 = (int16_t)(mul32(co, 1000) >> 12);
		}
	}

	static void SpawnSwords()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_SwordClipTask));
		n->c = 0;
		n->e = var<int16_t>(us(0x21FFA70));
		n = (Node24 *)AddTaskToQueue(&Q2(), us(ORIG_SwordDrawTask));
		n->c = 0;
		n->e = 0x69;
		for (int k = 0; k < 4; k++)
		{
			int16_t sword = *(const int16_t *)(us(0xCD0480) + 4 * k);
			Node24 *a = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_SwordPuffTask));
			a->c = 0;
			a->e = (int16_t)k;
			a->s1E = sword;
			a->s1C = (int16_t)(1 << k);
			Node24 *b = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_SwordShardTask));
			b->c = 0;
			b->s1C = (int16_t)(1 << k);
			b->s1E = *(const int16_t *)(us(0xCD0480) + 4 * k);
		}
	}

	static void SpawnSpinners()
	{
		for (uint32_t t = us(0xCD046A); t < us(0xCD0482); t += 8)
		{
			Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_SpinnerTask));
			n->c = 0;
			n->e = *(const int16_t *)(t - 2);
			n->p10 = 0;
			n->p12 = *(const int16_t *)t;
			n->p14 = 0;
			n->a18 = (int16_t)(CrtRand() % 0x1000);
			int32_t r = CrtRand() % 60;
			n->a1A = (int16_t)(r + 0x14);
			int32_t w = (int32_t)*(const int16_t *)(t + 2) / 8;
			n->s1E = (int16_t)w;
			n->s1C = (int16_t)(w + w);
			int32_t h = CrtRand() % 0x600;
			n->s20 = (int16_t)(h + *(const int16_t *)(t + 4));
		}
	}

	static void LinkTrail()
	{
		Node24 *n = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_TrailTask));
		n->c = 0;
		for (uint32_t a = us(0x2200DF8); a < us(0x2200EC0); a += 0x14) var<uint32_t>(a) = 0;
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
			CameraScriptStart(*(const uint32_t *)(us(0xCD0960) + 4 * Variant()), &RootMatrix(), GIL3(), &Q1(), 0);
			static const uint32_t streams[4] = { us(0xE9151C), us(0xE8D208), us(0xE88EF4), us(0xE84BE0) };
			uint32_t v = (uint32_t)Variant();
			if (v <= 3) BdTransSummonStream(streams[v], us(STREAM_STATE));
			VoiceSlot() = ClaimVoiceSlot(*(const void **)(us(0xCD04A0) + 4 * Variant()), 1, 0x80);
		}
		int32_t c = n->c;
		int32_t e;
		if (c < 0x10)
		{
			if (c == 1) BdPlaySE(us(0xCD0378), 0, 0x80);
			else if (c == 2) CharacterLoad(0x2E9, MB() + 0xE000);
			else if (c == 0) LinkQ1(us(ORIG_FadeTileTask));
			else if (c == 8)
			{
				SpawnRings();
				ShowParty();
			}
		}
		else if ((e = c - 0x10) < 0x3C)
		{
			if (e == 0)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0xE000);
				CharacterLoad(0x2EA, MB());
			}
			else if (e == 8)
			{
				if (LoadState() < 0) return 0;
				CharacterLoad(0x2EB, MB() + 0x8000);
			}
			else if (e == 2) SpawnFlareRow();
			else if (e == 0x1C) SpawnRisers();
		}
		else if ((e -= 0x3C) < 0x2D)
		{
			if (e == 3) BdPlaySummonStream(0x80, 0, 0x60);
			else if (e == 0)
			{
				HideParty();
				SpawnSwords();
				LinkQ2(us(ORIG_Gil1DrawTask));
				LinkQ1(us(ORIG_Gil1Task));
				LinkFlash(us(ORIG_FlashInTask));
			}
			else if (e == 0x18)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0x8000);
			}
			else if (e == 0x19) LinkQ1(us(ORIG_SparkTask));
			else if (e == 0x25) LinkQ1(us(ORIG_PuffTask));
		}
		else if ((e -= 0x2D) < 0x3C)
		{
			if (e == 0)
			{
				BdPlaySE(us(0xCD037C), 0, 0x80);
				SpawnSpinners();
			}
			else if (e == 1)
			{
				CharacterLoad(0x2EC, MB() + 0x8000);
				LinkQ1(us(ORIG_DotTask));
			}
			else if (e == 0xF)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0x8000);
				CharacterLoad(0x2ED, MB() + 0x22000);
			}
			else if (e == 0x1E)
			{
				if (LoadState() < 0) return 0;
				CharacterLoad(0x2EE, SummonData());
			}
			else if (e == 0x37)
			{
				if (LoadState() < 0) return 0;
				BdTransSummonStream(SummonData(), us(STREAM_STATE));
			}
			else if (e == 2) LinkQ1(us(ORIG_RisingRingSpawnerTask));
		}
		else if ((e -= 0x3C) < 0x2A)
		{
			if (e == 0)
			{
				BdPlaySE(us(0xCD0380), 0, 0x80);
				BdPlaySummonStream(0x80, 0, 0x60);
				CharacterLoad(0x2EF, MB());
				LinkQ2(us(ORIG_Gil2DrawTask));
				LinkQ1(us(ORIG_Gil2Task));
				LinkFeedback(1, 0x5C);
			}
			else if (e == 0xA)
			{
				if (LoadState() < 0) return 0;
				CharacterLoad(0x2F0, SummonData());
			}
			else if (e == 0x25)
			{
				if (LoadState() < 0) return 0;
				BdTransSummonStream(SummonData(), us(STREAM_STATE));
			}
		}
		else if ((e -= 0x2A) < 0x32)
		{
			if (e == 0x14) BdPlaySummonStream(0x80, 0, 0x60);
			else if (e == 0)
			{
				CharacterLoad(0x2F3, MB() + 0xE000);
				LinkQ2(us(ORIG_Gil3DrawTask));
				LinkQ1(us(ORIG_Gil3Task));
			}
			else if (e == 0xE)
			{
				if (LoadState() < 0) return 0;
				QueueTIMUpload(MB() + 0xE000);
			}
			else if (e == 0xF)
			{
				int32_t id = *(const int32_t *)(us(0xCD09A0) + 4 * Variant());
				if (id) CharacterLoad(id, MB() + 0xE000);
			}
			else if (e == 0x1E)
			{
				if (*(const int32_t *)(us(0xCD09A0) + 4 * Variant()) != 0 && LoadState() < 0) return 0;
				CharacterLoad(0x2F1, SummonData());
			}
			else if (e == 0x2D)
			{
				if (LoadState() < 0) return 0;
				BdTransSummonStream(SummonData(), us(STREAM_STATE));
			}
			else if (e == 0x28) LinkTrail();
			else if (e == 0xA) AnimSeqText();
		}
		else if ((e -= 0x32) < 0x32)
		{
			if (e == 0)
			{
				BdPlaySE(*(const uint32_t *)(us(0xCD04B0) + 4 * Variant()), 0, 0x80);
				BdPlaySummonStream(0x80, 0, 0x60);
				QueueTIMUpload(MB() + 0xE000);
				LinkQ1(*(const uint32_t *)(us(0xCD0990) + 4 * Variant()));
			}
			else if (e == 0x26) LinkFlash(us(ORIG_FlashOutTask));
			else if (e == 6)
			{
				CameraShake(2, 2, 0xC, 0xFF);
				LinkQ1(us(ORIG_EndTask));
			}
			else if (e == 7) LinkFeedback(1, 0x2B);
		}
		else if ((e -= 0x32) < 4)
		{
			if (e == 1 && VoiceSlotBusy(VoiceSlot())) ReleaseVoiceSlot(VoiceSlot());
		}
		n->c++;
		if (n->c <= 0x147) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	static uint32_t __cdecl FeedbackTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RequestScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F);
		if (Paused()) return 0;
		n->c++;
		return n->c >= n->e ? TASK_END : 0;
	}

	static uint32_t __cdecl Feedback2Task(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c > 0) RequestScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F);
		if (Paused()) return 0;
		n->c++;
		return n->c >= n->e ? TASK_END : 0;
	}

	static uint32_t __cdecl FlashInTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		SetScreenFlash((uint32_t)mul32((int32_t)n->a18 / (int32_t)n->e, n->c), 0);
		if (Paused()) return 0;
		n->c++;
		return n->c >= n->e ? TASK_END : 0;
	}

	static uint32_t __cdecl FlashOutTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t level = n->a18;
		SetScreenFlash((uint32_t)(level - mul32(level / (int32_t)n->e, n->c)), 0);
		if (Paused()) return 0;
		n->c++;
		if (n->c < n->e) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	static void Tile(int32_t r, int32_t g, int32_t b, int32_t otz)
	{
		uint32_t *p = (uint32_t *)Cursor();
		uint32_t col = ((((((uint32_t)b & 0xFF) | 0x6200u) << 8) | ((uint32_t)g & 0xFF)) << 8) | ((uint32_t)r & 0xFF);
		p[1] = col;
		((uint16_t *)p)[4] = 0;
		((uint16_t *)p)[5] = 0;
		p[0] = 0x3000000;
		((uint16_t *)p)[6] = 0xA00;
		((uint16_t *)p)[7] = 0x6C0;
		InsertPrimAutoDepth(RenderList() + 4 * (uint32_t)otz + 0x44, p);
		p += 4;
		p[0] = 0x1000000;
		p[1] = 0xE1000220;
		InsertPrimAutoDepth(RenderList() + 4 * (uint32_t)otz + 0x44, p);
		Cursor() = (uint32_t)(p + 2);
	}

	static int32_t FadeTileLevel(int32_t c)
	{
		if (c < 4) return (mul32(c, 255) / 4) & 0xFF;
		if (c >= 8) return 0xFF - ((mul32(c - 8, 255) / 16) & 0xFF);
		return 0xFF;
	}

	static uint32_t __cdecl FadeTileTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t v = FadeTileLevel(n->c);
		Tile(v, v, v, 0);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x18 ? TASK_END : 0;
	}

	static uint32_t __cdecl EndTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 1) WaitAnimSeq(MB() + 0xE000, &n->e);
		uint16_t done = (uint16_t)n->e;
		n->c++;
		return done ? TASK_END : 0;
	}

	static void LitSetupAt(uint8_t *s, int32_t x, int32_t y, int32_t z, const Mat4x3 *comp)
	{
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = x;
		*(int32_t *)(s + 0x20) = y;
		*(int32_t *)(s + 0x24) = z;
		ComposeAffineTransform(comp, (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static void LitSetupFrame(uint8_t *s)
	{
		ff8fx::copy<(sizeof(Mat4x3))>(s + 8, &Frame());
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static void LitSpriteMAC(uint8_t *h, uint8_t *s, const int16_t *pos, int16_t size, int16_t age, int shift, bool set8, int16_t w8)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int16_t *)(h + 4) = age;
		if (set8) *(int32_t *)(h + 8) = w8;
		int32_t *mac = (int32_t *)(s + 0x38), *u = (int32_t *)(s + 0x28);
		GteReadMAC123(mac);
		NormalizeVectorToFixedPoint(mac, u);
		int32_t k = -((int32_t)size >> shift);
		mac[0] = mac[0] + (mul32(u[0], k) >> 12);
		mac[1] = mac[1] + (mul32(u[1], k) >> 12);
		mac[2] = mac[2] + (mul32(u[2], k) >> 12);
		GteSetTransFromVec32(mac);
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static void LitSprite(uint8_t *h, const int16_t *pos, int16_t size, int16_t age)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(size);
		*(int16_t *)(h + 4) = age;
		GteTransFromMAC();
		Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
	}

	static uint32_t ScreenPrimModel(const Node24 *n, uint32_t orig)
	{
		if (orig == us(ORIG_Zan_ScreenPrimTask)) return us(0xCCDE04);
		if (orig == us(ORIG_Masa_ScreenPrimTask)) return us(0xCCF2DC);
		return n->e ? us(0xCCEC5C) : us(0xCCE5DC);
	}

	static void ScreenPrimDraw(const Node24 *n, uint32_t model, int16_t s1C)
	{
		PrimArgs a = Prim(0, 0, n->a18, n->p10, n->p12, 0x120, s1C, n->s20, n->s20, nullptr, model);
		a.screen = true;
		PrimDraw(a);
	}

	static uint32_t ScreenPrimTick(Node24 *n, uint32_t orig)
	{
		ScreenPrimDraw(n, ScreenPrimModel(n, orig), n->s1C);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c < 6)
		{
			int16_t v = n->s1E;
			n->s1C = (int16_t)(n->s1C + v);
			n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 16));
		}
		n->c = (int16_t)(c + 1);
		return n->c >= 8 ? TASK_END : 0;
	}

	static uint32_t __cdecl ZanScreenPrimTask(TaskNode *tn) { return ScreenPrimTick((Node24 *)tn, us(ORIG_Zan_ScreenPrimTask)); }
	static uint32_t __cdecl MasaScreenPrimTask(TaskNode *tn) { return ScreenPrimTick((Node24 *)tn, us(ORIG_Masa_ScreenPrimTask)); }
	static uint32_t __cdecl ExcaScreenPrimTask(TaskNode *tn) { return ScreenPrimTick((Node24 *)tn, us(ORIG_Exca_ScreenPrimTask)); }

	static uint32_t __cdecl ZanTask(TaskNode *tn)
	{
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			Node24 *p = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Zan_ScreenPrimTask));
			p->c = 0;
			p->p10 = (int16_t)0xFF56;
			p->p12 = (int16_t)0xFFE2;
			p->a18 = 0x860;
			p->s1E = 0x1C0;
			p->s1C = 0x1C0;
			p->s20 = 0x600;
		}
		if (n->c == 0x10)
		{
			int32_t spawned = 0;
			bool party = true;
			if (TargetCount() != 0)
			{
				uint32_t off = 0;
				int32_t idx = 0;
				do
				{
					uint8_t *rec = TargetRecs() + off;
					if (rec[3] & 4) ApplyActionResultToTarget(rec);
					else
					{
						uint32_t slot = rec[0];
						Node24 *cut = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Zan_CutTask));
						uint8_t *ent = Entity(slot);
						cut->c = 0;
						cut->e = (int16_t)slot;
						BoneMid(ent, &cut->p10);
						int32_t a = CrtRand() % 0x500 + 0xB00;
						cut->a18 = (int16_t)a;
						if (a & 1) cut->a18 = (int16_t)-a;
						int32_t r = CrtRand() % 0x12C;
						cut->s22 = (int16_t)(-150 - r);
						cut->s20 = (int16_t)(-150 - r);
						Node24 *sl = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Zan_SliceTask));
						uint32_t p0 = *(const uint32_t *)&cut->p10, p1 = *(const uint32_t *)&cut->p14;
						sl->e = (int16_t)slot;
						*(uint32_t *)&sl->p10 = p0;
						int32_t size = *(const int16_t *)(ent + 0x26);
						sl->c = 0;
						*(uint32_t *)&sl->p14 = p1;
						int32_t j = CrtRand() % 256;
						j = mul32(j + 0x80, size) >> 12;
						int32_t j2 = j + j;
						int32_t r1 = CrtRand() % j2;
						sl->p10 = (int16_t)(sl->p10 + (r1 - j));
						int32_t r2 = CrtRand() % j2;
						sl->p12 = (int16_t)(sl->p12 + (r2 - j));
						int32_t r3 = CrtRand() % j2;
						sl->a18 = 0x80;
						sl->a1A = (int16_t)idx;
						sl->p16 = (int16_t)idx;
						cut->p16 = (int16_t)idx;
						sl->p14 = (int16_t)(sl->p14 + (r3 - j));
						spawned++;
					}
					idx++;
					off += 0x18;
				} while ((uint32_t)idx < TargetCount());
				party = spawned == 0;
			}
			if (party)
			{
				Node24 *pc = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Zan_PartyClipTask));
				pc->c = 0;
				pc->e = 0x14;
				pc->s20 = 0;
				pc->s22 = 0x258;
			}
		}
		n->c++;
		return n->c >= 0x32 ? TASK_END : 0;
	}

	static void CutMatrix(const Node24 *n, int16_t a0, int16_t a1, int16_t a2, int32_t lift, Mat4x3 *out, int16_t *A)
	{
		A[0] = a0;
		A[1] = a1;
		A[2] = a2;
		Mat4x3 m;
		ComposeZYXRotationMatrix(A, &m);
		int16_t v[4] = { 0, 0, 0, 0 };
		MatrixMultiplyVector(&m, &n->p10, v);
		m.t[0] = (int32_t)n->p10 - v[0];
		m.t[1] = lift + n->p12 - v[1];
		m.t[2] = (int32_t)n->p14 - v[2];
		ComposeAffineTransform((const Mat4x3 *)(Entity((uint32_t)(int32_t)n->e) + 0x40), &m, out);
	}

	static int16_t CutAngle0(int32_t c, int16_t a) { return (int16_t)(mul32(c, a) / 40); }
	static int16_t CutAngle1(int32_t c, int16_t a) { return (int16_t)(mul32((int32_t)a / 2, c) / 40); }

	static uint32_t __cdecl ZanCutTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->p16;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->p16 = (int16_t)(d - 1);
			return 0;
		}
		int16_t c = n->c;
		int32_t slot = n->e;
		int16_t *A = (int16_t *)(us(0x2200B7C) + 60 * slot);
		if (c == 0)
		{
			A[2] = 0;
			A[1] = 0;
			A[0] = 0;
		}
		if (c <= 0x28)
		{
			FX_HELD(MemoNode(n);)
			CutMatrix(n, CutAngle0(c, n->a18), CutAngle1(c, n->a18), A[2], n->s20, (Mat4x3 *)(us(0x2200B94) + 60 * slot), A);
		}
		if (Paused()) return 0;
		int16_t v = n->s22;
		n->s20 = (int16_t)(n->s20 + v);
		n->s22 = (int16_t)(v - (int16_t)(v >> 3));
		n->c++;
		return n->c >= 0x28 ? TASK_END : 0;
	}

	static void TargetDraw(uint8_t *h, uint8_t *ent, const Mat4x3 *moved)
	{
		if (!(ent[0] & 0x20))
			Cursor() = DrawShadow(ent, RenderList() + 0x4064, 0x10, Cursor());
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(ent + 0x40), (Mat4x3 *)(h + 0x198));
		ComposeAffineTransform(&Camera(), moved, (Mat4x3 *)(h + 0x1B8));
		*(uint32_t *)(h + 4) = g_vertex_scratch_override ? g_vertex_scratch_override : var<uint32_t>(us(0x1D98B3C));
		*(uint32_t *)(h + 0x20) = *(uint32_t *)(ent + 0x28);
		*(uint32_t *)(h + 0x10) = *(uint32_t *)(ent + 0x7C);
		Cursor() = SliceRender(*(void **)(ent + 0x64), h, OT(), 2, Cursor());
		BuildBoneMatricesFromPose(ent + 0x60);
	}

	static void SliceDraw(uint8_t *h, const Node24 *n, uint8_t *ent, const Mat4x3 *moved, int32_t c, int32_t w_override)
	{
		int16_t ang[4] = { 0, 0, n->a18, 0 };
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		*(uint32_t *)(h + 0x48) = *(const uint32_t *)&n->p10;
		*(uint32_t *)(h + 0x4C) = *(const uint32_t *)&n->p14;
		int16_t *nrm = (int16_t *)(h + 0xA8);
		nrm[0] = 0;
		nrm[1] = (int16_t)0xF000;
		nrm[2] = 0;
		MatrixMultiplyVector(&m, nrm, nrm);
		PlaneDistance((const int16_t *)(h + 0x48), nrm);
		if (c >= 0x18)
		{
			int32_t t = ComputeSin(shl32(c - 0x18, 10) / 16);
			int32_t w = 0x1000 - t;
			if (w_override >= 0) w = w_override;
			uint32_t k = (uint32_t)(shl32(w, 7) >> 12);
			uint32_t col = ((((k | 0x3200u) << 8) | k) << 8) | k;
			*(uint32_t *)(ent + 0x2C) = col;
			GteSetFarColor(var<uint8_t>(us(0xB8B7D8)), var<uint8_t>(us(0xB8B7D9)), var<uint8_t>(us(0xB8B7DA)));
			uint32_t zero = 0;
			GteLoadRGBC(&zero);
			GteSetIR0(w);
			GteDPCS();
			GteStoreRGB2(ent + 0x28);
			ent[0x2B] = 2;
			ent[7] = 0;
		}
		TargetDraw(h, ent, moved);
	}

	static uint32_t __cdecl ZanSliceTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->p16;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->p16 = (int16_t)(d - 1);
			return 0;
		}
		if (n->c < 0x28)
		{
			uint8_t *ent = Entity((uint32_t)(int32_t)n->e);
			ent[0] |= 0xC;
			uint8_t *h = (uint8_t *)FieldAlloc(0x1D8);
			SliceDraw(h, n, ent, (const Mat4x3 *)(us(0x2200B94) + 60 * (int32_t)n->e), n->c, -1);
			FieldFree(0x1D8);
			FX_HELD(MemoNode(n);)
		}
		if (Paused()) return 0;
		n->c++;
		if (n->c <= 0x28) return 0;
		ApplyActionResultToTarget(TargetRecs() + 24 * (int32_t)n->a1A);
		return TASK_END;
	}

	static void PartyDraw(uint8_t *h, const Mat4x3 *m)
	{
		ff8fx::copy<(sizeof(Mat4x3))>(h + 0x198, &Camera());
		ComposeAffineTransform(&Camera(), m, (Mat4x3 *)(h + 0x1B8));
		*(uint32_t *)(h + 4) = g_vertex_scratch_override ? g_vertex_scratch_override : var<uint32_t>(us(0x1D98B3C));
		*(uint32_t *)(h + 0x10) = 0xFFFFFFFF;
		for (int i = 0; i < 4; i++)
		{
			uint8_t *e = (uint8_t *)(us(0x1D98991) + 0x2C * i);
			const int32_t *tb = (const int32_t *)(us(0xCD0970) + 8 * i);
			BuildBoneMatricesFromPose(e + 0x13);
			if (!(e[0] & 1)) continue;
			*(uint32_t *)(h + 0x14) = *(const uint32_t *)(e + 0x27);
			*(uint16_t *)(h + 0x18) = *(const uint16_t *)(e + 1);
			Cursor() = PartyRender(e + 3, h, RenderList() + 4 * (uint32_t)tb[0], tb[1], Cursor());
		}
	}

	static void PartyClipDraw(uint8_t *h, int32_t z)
	{
		int16_t ang[4] = { 0, 0, (int16_t)0xFFA0, 0 };
		int16_t *p = (int16_t *)(h + 0x48), *nrm = (int16_t *)(h + 0xA8);
		p[0] = 0;
		p[1] = (int16_t)0xF448;
		p[2] = 0;
		nrm[0] = 0;
		nrm[1] = (int16_t)0xF000;
		nrm[2] = 0;
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		nrm[0] = 0;
		nrm[1] = (int16_t)0xF000;
		nrm[2] = 0;
		MatrixMultiplyVector(&m, nrm, nrm);
		PlaneDistance(p, nrm);
		ResetRotation(&m);
		m.m[2][2] = 0x1000;
		m.m[1][1] = 0x1000;
		m.m[0][0] = 0x1000;
		m.t[1] = 0;
		m.t[0] = 0;
		m.t[2] = z;
		PartyDraw(h, &m);
	}

	static uint32_t __cdecl ZanPartyClipTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->e;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->e = (int16_t)(d - 1);
			return 0;
		}
		if (n->c < 0x41)
		{
			ShowParty();
			uint8_t *h = (uint8_t *)FieldAlloc(0x1D8);
			PartyClipDraw(h, n->s20);
			FieldFree(0x1D8);
			FX_HELD(MemoNode(n);)
		}
		if (Paused()) return 0;
		int16_t v = n->s22;
		n->s20 = (int16_t)(n->s20 - v);
		n->s22 = (int16_t)(v - (int16_t)(v >> 3));
		n->c++;
		if (n->c <= 0x41) return 0;
		HideParty();
		return TASK_END;
	}

	static uint32_t __cdecl MasaTask(TaskNode *tn)
	{
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c == 0 && TargetCount() != 0)
		{
			uint32_t off = 0;
			for (uint32_t k = 0; k < TargetCount(); k++, off += 0x18)
			{
				uint32_t slot = TargetRecs()[off];
				Node24 *p = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Masa_ScreenPrimTask));
				p->c = 0;
				GetDefaultEffectPosition(Entity(slot), &p->p10);
				GteSetRotMatrix(&Camera());
				GteSetTransVector(&Camera());
				GteLoadV0(&p->p10);
				GteRTPS();
				int16_t sxy[2];
				GteReadSXY2(sxy);
				sxy[0] = PsxPixels(sxy[0]);
				sxy[1] = PsxPixels(sxy[1]);
				p->p10 = (int16_t)(sxy[0] - 0x168);
				p->p12 = (int16_t)(sxy[1] - 0x134);
				p->a18 = 0xA00;
				p->s1E = 0x1C0;
				p->s1C = 0x1C0;
				p->s20 = 0x300;
			}
		}
		if (n->c == 0x10 && TargetCount() != 0)
		{
			uint32_t off = 0;
			for (uint32_t k = 0; k < TargetCount(); k++, off += 0x18)
			{
				uint32_t slot = TargetRecs()[off];
				Node24 *a = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Masa_PrimATask));
				a->c = 0;
				GetDefaultEffectPosition(Entity(slot), &a->p10);
				a->a18 = 0x200;
				int32_t r = CrtRand() % 0xA00 + 0xA00;
				int16_t v = (int16_t)(r / 3);
				a->s1E = v;
				a->s1C = v;
				Node24 *b = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Masa_PrimBTask));
				int16_t vb = (int16_t)(a->s1E + 0x600);
				uint32_t p0 = *(const uint32_t *)&a->p10, p1 = *(const uint32_t *)&a->p14;
				*(uint32_t *)&b->p10 = p0;
				b->c = 0;
				*(uint32_t *)&b->p14 = p1;
				b->a18 = 0x200;
				b->s1E = vb;
				b->s1C = vb;
				Node24 *s = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Masa_SparkTask));
				*(uint32_t *)&s->p10 = *(const uint32_t *)&a->p10;
				int16_t mask = (int16_t)(1 << k);
				*(uint32_t *)&s->p14 = *(const uint32_t *)&a->p14;
				s->c = 0;
				s->p16 = mask;
				Node24 *d = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Masa_DustTask));
				*(uint32_t *)&d->p10 = *(const uint32_t *)&a->p10;
				*(uint32_t *)&d->p14 = *(const uint32_t *)&a->p14;
				d->c = 0;
				d->e = 3;
				d->p16 = mask;
			}
		}
		if (n->c == 0x11) ApplyActionResultToTargets((uint32_t)TargetRecs(), TargetCount());
		n->c++;
		return n->c >= 0x32 ? TASK_END : 0;
	}

	static bool MasaFadeA(int32_t c, int32_t *v) { if (c < 8) return false; *v = shl32(c - 8, 9); return true; }
	static bool MasaFadeB(int32_t c, int32_t *v) { if (c < 4) return false; *v = mul32(c - 4, 341); return true; }

	static void MasaPrimDraw(const Node24 *n, bool b, int16_t s1C, bool fade, int32_t f)
	{
		PrimArgs a = Prim(0, 0, n->a18, n->p10, n->p12, n->p14, s1C, s1C, s1C, &Camera(), b ? us(0xCC877C) : us(0xCC72FC));
		SetFade(a, fade, f);
		PrimDraw(a);
	}

	static uint32_t MasaPrimTick(Node24 *n, bool b)
	{
		int32_t f = 0;
		bool fade = b ? MasaFadeB(n->c, &f) : MasaFadeA(n->c, &f);
		MasaPrimDraw(n, b, n->s1C, fade, f);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->c++;
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 4));
		return n->c >= 0x10 ? TASK_END : 0;
	}

	static uint32_t __cdecl MasaPrimATask(TaskNode *tn) { return MasaPrimTick((Node24 *)tn, false); }
	static uint32_t __cdecl MasaPrimBTask(TaskNode *tn) { return MasaPrimTick((Node24 *)tn, true); }

	static void MasaSparkSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = us(0xCC7120);
		*(uint16_t *)(h + 0x24) = 9;
		LitSetupAt(s, n->p10, n->p12, n->p14, &Camera());
	}

	static uint32_t __cdecl MasaSparkTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 2)
		{
			for (int k = 0; k < 8; k++)
			{
				int i = FreeRecord(pool, 0x20);
				if (i < 0) break;
				RecA *r = (RecA *)(pool + 0x20 * i);
				r->mask = (uint32_t)(int32_t)n->p16;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x800 + 0x600);
				int32_t *v = (int32_t *)(s + 0x28);
				v[0] = CrtRand() % 0x2000 - 0x1000;
				v[1] = CrtRand() % 0x2000 - 0x1000;
				v[2] = CrtRand() % 0x2000 - 0x1000;
				NormalizeVectorToFixedPoint(v, v);
				int32_t d = CrtRand() % 400 + 300;
				r->x = (int16_t)(mul32(v[0], d) >> 12);
				r->y = (int16_t)(mul32(v[1], d) >> 12);
				r->z = (int16_t)(mul32(v[2], d) >> 12);
				int32_t sp = CrtRand() % 350 + 120;
				r->vx = (int16_t)(mul32(v[0], sp) >> 12);
				r->vy = (int16_t)(mul32(v[1], sp) >> 12);
				r->vz = (int16_t)(mul32(v[2], sp) >> 12);
				r->w1C = (int16_t)(CrtRand() % 0x800);
				int32_t spin = CrtRand() % 90 + 30;
				r->w1E = (int16_t)spin;
				if (spin & 1) r->w1E = (int16_t)-spin;
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		MasaSparkSetup(h, s, n);
		int32_t mask = n->p16;
		for (int i = 0; i < POOL_N; i++)
		{
			RecA *r = (RecA *)(pool + 0x20 * i);
			if (!(mask & r->mask)) continue;
			FX_HELD(MemoA(i, n);)
			LitSpriteMAC(h, s, &r->x, r->size, r->age, 3, true, r->w1C);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->age++;
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			r->w1C = (int16_t)(r->w1C + r->w1E);
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	static void MasaDustSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = us(0xCC6DC8);
		*(uint16_t *)(h + 0x24) = 8;
		LitSetupAt(s, n->p10, 0, n->p14, &Camera());
	}

	static uint32_t __cdecl MasaDustTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->e;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->e = (int16_t)(d - 1);
			return 0;
		}
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 2)
		{
			for (int k = 0; k < 8; k++)
			{
				int i = FreeRecord(pool, 0x18);
				if (i < 0) break;
				RecB *r = (RecB *)(pool + 0x18 * i);
				r->mask = (uint32_t)(int32_t)n->p16;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x1000 + 0x500);
				int32_t a = CrtRand() % 0x1000;
				*(int32_t *)(s + 0x28) = ComputeSin(a);
				*(int32_t *)(s + 0x2C) = 0;
				*(int32_t *)(s + 0x30) = ComputeCos(a);
				int32_t rr = CrtRand() % 300 + 400;
				r->x = (int16_t)(mul32(rr, *(int32_t *)(s + 0x28)) >> 12);
				int32_t ry = CrtRand() % 50;
				r->y = (int16_t)-ry;
				r->z = (int16_t)(mul32(rr, *(int32_t *)(s + 0x30)) >> 12);
				int32_t sp = CrtRand() % 200 + 90;
				r->vx = (int16_t)(mul32(sp, *(int32_t *)(s + 0x28)) >> 12);
				r->vz = (int16_t)(mul32(sp, *(int32_t *)(s + 0x30)) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		MasaDustSetup(h, s, n);
		int32_t mask = n->p16;
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *r = (RecB *)(pool + 0x18 * i);
			if (!(mask & r->mask)) continue;
			FX_HELD(MemoB(i, n);)
			LitSpriteMAC(h, s, &r->x, r->size, r->age, 3, false, 0);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->age++;
			r->x = (int16_t)(r->x + r->vx);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	static uint32_t ExcaTick(Node24 *n, int16_t poor)
	{
		if (Paused()) return 0;
		if (n->c == 0)
		{
			Node24 *p = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Exca_ScreenPrimTask));
			p->c = 0;
			p->e = poor;
			p->p10 = (int16_t)0xFF56;
			p->p12 = (int16_t)0xFF9C;
			p->a18 = 0x968;
			p->s1E = 0x555;
			p->s1C = 0x555;
			p->s20 = 0x1000;
		}
		if (n->c == 0x12 && TargetCount() != 0)
		{
			uint32_t off = 0;
			for (uint32_t k = 0; k < TargetCount(); k++, off += 0x18)
			{
				uint32_t slot = TargetRecs()[off];
				Node24 *b = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Exca_BurstTask));
				b->c = 0;
				b->e = poor;
				GetDefaultEffectPosition(Entity(slot), &b->p10);
				int16_t bit = (int16_t)(1 << k);
				b->p16 = bit;
				Node24 *s = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Exca_SmokeTask));
				*(uint32_t *)&s->p10 = *(const uint32_t *)&b->p10;
				*(uint32_t *)&s->p14 = *(const uint32_t *)&b->p14;
				s->c = 0;
				s->e = poor;
				s->p16 = (int16_t)(0x100 << k);
				Node24 *t = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Exca_StreakTask));
				*(uint32_t *)&t->p10 = *(const uint32_t *)&b->p10;
				*(uint32_t *)&t->p14 = *(const uint32_t *)&b->p14;
				t->c = 0;
				t->p16 = bit;
				Node24 *q = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_Exca_PillarTask));
				*(uint32_t *)&q->p10 = *(const uint32_t *)&b->p10;
				q->c = 0;
				q->e = 1;
				*(uint32_t *)&q->p14 = *(const uint32_t *)&b->p14;
				q->a18 = 0x800;
				int32_t r1 = CrtRand() % 0x200 + 0x300;
				q->s1E = (int16_t)(r1 / 3);
				q->s1C = (int16_t)(r1 / 3);
				int32_t r2 = CrtRand() % 0x300 + 0x300;
				q->s22 = (int16_t)(r2 / 3);
				q->s20 = (int16_t)(r2 / 3);
			}
		}
		if (n->c == 0x13) ApplyActionResultToTargets((uint32_t)TargetRecs(), TargetCount());
		n->c++;
		return n->c >= 0x32 ? TASK_END : 0;
	}

	static uint32_t __cdecl ExcaTask(TaskNode *tn) { return ExcaTick((Node24 *)tn, 0); }
	static uint32_t __cdecl PoorTask(TaskNode *tn) { return ExcaTick((Node24 *)tn, 1); }

	static void ExcaBurstSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = n->e ? us(0xCC6954) : us(0xCC6730);
		*(uint16_t *)(h + 0x24) = 8;
		LitSetupAt(s, n->p10, n->p12, n->p14, &Camera());
	}

	static uint32_t __cdecl ExcaBurstTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 1)
		{
			for (int k = 0; k < 2; k++)
			{
				int i = FreeRecord(pool, 0x18);
				if (i < 0) break;
				RecB *r = (RecB *)(pool + 0x18 * i);
				r->mask = (uint32_t)(int32_t)n->p16;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x2000 + 0x600);
				r->x = (int16_t)(CrtRand() % 0x4B0 - 0x258);
				r->y = (int16_t)(CrtRand() % 0x4B0 - 0x258);
				r->z = (int16_t)(CrtRand() % 0x4B0 - 0x258);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		ExcaBurstSetup(h, s, n);
		int32_t mask = n->p16;
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *r = (RecB *)(pool + 0x18 * i);
			if (!(mask & r->mask)) continue;
			FX_HELD(MemoB(i, n);)
			LitSpriteMAC(h, s, &r->x, r->size, r->age, 3, false, 0);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->age++;
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	static void ExcaSmokeSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = us(0xCC6F74);
		*(uint16_t *)(h + 0x24) = 0xC;
		*(uint32_t *)(h + 0x1C) = n->e ? 0x404080u : 0x808080u;
		LitSetupAt(s, n->p10, n->p12, n->p14, &Camera());
	}

	static uint32_t __cdecl ExcaSmokeTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 2)
		{
			for (int k = 0; k < 6; k++)
			{
				int i = FreeRecord(pool, 0x18);
				if (i < 0) break;
				RecB *r = (RecB *)(pool + 0x18 * i);
				r->mask = (uint32_t)(int32_t)n->p16;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x600 + 0x300);
				int32_t *v = (int32_t *)(s + 0x28);
				v[0] = CrtRand() % 0x2000 - 0x1000;
				v[1] = CrtRand() % 0x2000 - 0x1000;
				v[2] = -0x800 - CrtRand() % 0x800;
				NormalizeVectorToFixedPoint(v, v);
				int32_t d = CrtRand() % 500 + 200;
				r->x = (int16_t)(mul32(v[0], d) >> 12);
				r->y = (int16_t)(mul32(v[1], d) >> 12);
				r->z = (int16_t)(mul32(v[2], d) >> 12);
				int32_t sp = CrtRand() % 300 + 100;
				r->vx = (int16_t)(mul32(v[0], sp) >> 12);
				r->vy = (int16_t)(mul32(v[1], sp) >> 12);
				r->vz = (int16_t)(mul32(v[2], sp) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		ExcaSmokeSetup(h, s, n);
		int32_t mask = n->p16;
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *r = (RecB *)(pool + 0x18 * i);
			if (!(r->mask & mask)) continue;
			FX_HELD(MemoB(i, n);)
			LitSprite(h, &r->x, r->size, r->age);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				r->mask = 0;
				continue;
			}
			r->age++;
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	static void ExcaStreakSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = us(0xCC6C0C);
		*(uint16_t *)(h + 0x24) = 8;
		*(uint16_t *)(h + 4) = 0;
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x1C) = n->p10;
		*(int32_t *)(s + 0x20) = n->p12;
		*(int32_t *)(s + 0x24) = n->p14;
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
		*(int32_t *)(s + 0x48) = 0;
		*(int32_t *)(s + 0x4C) = -0x1000;
		*(int32_t *)(s + 0x50) = 0;
		*(int32_t *)(s + 0x78) = 0xC00;
		*(int32_t *)(s + 0x80) = 0;
	}

	static void ExcaStreakOne(uint8_t *h, uint8_t *s, const int16_t *dir, const int16_t *pos, int16_t size)
	{
		int32_t *v = (int32_t *)(s + 0x58);
		v[0] = dir[0];
		v[1] = dir[1];
		v[2] = dir[2];
		int32_t angle = GetRotationBetweenVectors(s + 0x48, v, (int32_t *)(s + 0x68));
		Mat4x3 *m = (Mat4x3 *)(s + 0x28);
		BuildAxisAngleRotationMatrix(angle, m, (const int32_t *)(s + 0x68));
		m->t[0] = pos[0];
		m->t[1] = pos[1];
		m->t[2] = pos[2];
		*(int32_t *)(s + 0x7C) = size;
		Scale3DMatrix(m, (const int32_t *)(s + 0x78));
		ComposeAffineTransform((const Mat4x3 *)(s + 8), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
	}

	static int16_t StreakSize(int16_t age, int16_t size0) { return (int16_t)(mul32(0x1FFC - shl32(mul32(age, 341), 1), size0) >> 12); }

	static uint32_t __cdecl ExcaStreakTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x88);
		if (!Paused() && n->c <= 2)
		{
			for (int k = 0; k < 4; k++)
			{
				int i = FreeRecord(pool, 0x20);
				if (i < 0) break;
				RecA *r = (RecA *)(pool + 0x20 * i);
				r->mask = (uint32_t)(int32_t)n->p16;
				r->age = 0;
				r->size = (int16_t)(CrtRand() % 0x1000 + 0x600);
				int32_t *v = (int32_t *)(s + 0x58);
				v[0] = CrtRand() % 0x2000 - 0x1000;
				v[1] = CrtRand() % 0x2000 - 0x1000;
				v[2] = CrtRand() % 0x2000 - 0x1000;
				NormalizeVectorToFixedPoint(v, v);
				int32_t d = CrtRand() % 400 + 400;
				r->x = (int16_t)(mul32(v[0], d) >> 12);
				r->y = (int16_t)(mul32(v[1], d) >> 12);
				r->z = (int16_t)(mul32(v[2], d) >> 12);
				r->w0E = r->size;
				int32_t sp = CrtRand() % 380 + 140;
				r->vx = (int16_t)(mul32(v[0], sp) >> 12);
				r->vy = (int16_t)(mul32(v[1], sp) >> 12);
				r->vz = (int16_t)(mul32(v[2], sp) >> 12);
				r->w18 = (int16_t)v[0];
				r->w1A = (int16_t)v[1];
				r->w1C = (int16_t)v[2];
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		ExcaStreakSetup(h, s, n);
		int32_t mask = n->p16;
		for (int i = 0; i < POOL_N; i++)
		{
			RecA *r = (RecA *)(pool + 0x20 * i);
			if (!(mask & r->mask)) continue;
			FX_HELD(MemoA(i, n);)
			ExcaStreakOne(h, s, &r->w18, &r->x, r->size);
			if (r->age >= 6) r->size = StreakSize(r->age, r->w0E);
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
			if (Paused()) continue;
			if (r->age >= 0xC)
			{
				r->mask = 0;
				continue;
			}
			r->age++;
			r->x = (int16_t)(r->x + r->vx);
			r->y = (int16_t)(r->y + r->vy);
			r->z = (int16_t)(r->z + r->vz);
			r->vx = (int16_t)(r->vx - (int16_t)(r->vx >> 3));
			r->vy = (int16_t)(r->vy - (int16_t)(r->vy >> 3));
			r->vz = (int16_t)(r->vz - (int16_t)(r->vz >> 3));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x88);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	static void ExcaPillarDraw(const Node24 *n, int16_t a18, int16_t s1C, int16_t s20, bool fade, int32_t f)
	{
		PrimArgs a = Prim(0, a18, 0, n->p10, 0, n->p14, s1C, s20, s1C, &Camera(), us(0xCCFAB4));
		SetFade(a, fade, f);
		PrimDraw(a);
	}

	static uint32_t __cdecl ExcaPillarTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->e;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->e = (int16_t)(d - 1);
			return 0;
		}
		int32_t f = 0;
		bool fade = MasaFadeB(n->c, &f);
		ExcaPillarDraw(n, n->a18, n->s1C, n->s20, fade, f);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->s1C = (int16_t)(n->s1C + v);
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 4));
		int16_t w = n->s22;
		n->s20 = (int16_t)(n->s20 + w);
		n->c++;
		n->s22 = (int16_t)(w - (int16_t)((int32_t)w / 4));
		return n->c >= 0x10 ? TASK_END : 0;
	}

	static void RingDraw(const Node24 *n)
	{
		PrimArgs a = Prim(0, 0, 0, n->p10, n->p12, n->p14, n->s1C, n->s20, n->s1C, &Frame(), us(0xCC8F5C));
		a.mode = 1;
		a.h1C = 0;
		PrimDraw(a);
	}

	static uint32_t __cdecl RingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		RingDraw(n);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x44 ? TASK_END : 0;
	}

	static void RingSpinDraw(const Node24 *n, bool b, int16_t a18)
	{
		uint32_t model = b ? (n->e ? us(0xCCAD3C) : us(0xCCA14C)) : (n->e ? us(0xCCC434) : us(0xCCB92C));
		PrimArgs a = b ? Prim(0, a18, 0, n->p10, n->p12, n->p14, n->s1C, n->s1C, n->s1C, &Frame(), model)
			: Prim(0, a18, 0, n->p10, n->p12, n->p14, n->s1C, n->s20, n->s1C, &Frame(), model);
		a.h1C = 0xC3;
		a.setC = true;
		a.hC = n->s22;
		PrimDraw(a);
	}

	static uint32_t RingSpinTick(Node24 *n, bool b)
	{
		RingSpinDraw(n, b, n->a18);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		n->a18 = (int16_t)(n->a18 + n->a1A);
		n->c++;
		return n->c >= 0x44 ? TASK_END : 0;
	}

	static uint32_t __cdecl RingSpinTask(TaskNode *tn) { return RingSpinTick((Node24 *)tn, false); }
	static uint32_t __cdecl RingSpin2Task(TaskNode *tn) { return RingSpinTick((Node24 *)tn, true); }

	static void ScreenSpriteMatrix(const int16_t *v, int16_t size, int32_t zoff)
	{
		var<int16_t>(us(0x2201070)) = size;
		var<int16_t>(us(0x2201068)) = size;
		var<int16_t>(us(0x2201060)) = size;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(v);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy);
		int16_t sx = (int16_t)((int32_t)sxy[0] / 8);
		int16_t sy = (int16_t)((int32_t)sxy[1] / 8);
		var<int32_t>(us(0x2201074)) = (int32_t)sx - 0xA0;
		var<int32_t>(us(0x2201078)) = (int32_t)sy - 0x6C;
		var<int32_t>(us(0x220107C)) = (int32_t)var<int16_t>(us(0x1D8E038)) + zoff;
		GteSetRotMatrix((const Mat4x3 *)us(0x2201060));
		GteSetTransVector((const Mat4x3 *)us(0x2201060));
	}

	static uint32_t FlareColour(int32_t c)
	{
		int32_t v;
		if (c < 2) v = shl32(c, 6);
		else if (c >= 10) v = 0xE4 - c * 10;
		else v = 0x80;
		if (c & 1) v >>= 1;
		return (uint32_t)shl32(shl32(v, 8) | v, 8);
	}

	static void FlareDraw(const Node24 *n, int32_t spread, int32_t depth, uint32_t colour)
	{
		int32_t ref[3] = { 0, 0, -0x1000 };
		int32_t d[3];
		d[0] = (int32_t)var<int16_t>(us(0xB8B7F0)) - n->p10;
		d[1] = (int32_t)var<int16_t>(us(0xB8B7F2)) - n->p12;
		d[2] = (int32_t)var<int16_t>(us(0xB8B7F4)) - n->p14;
		NormalizeVectorToFixedPoint(d, d);
		int32_t axis[4];
		int32_t angle = GetRotationBetweenVectors(ref, d, axis);
		Mat4x3 m;
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = n->p10;
		m.t[1] = n->p12;
		m.t[2] = n->p14;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = us(0xCC6B64);
		*(uint16_t *)(h + 0x24) = 4;
		*(uint32_t *)(h + 0x1C) = colour;
		int32_t a = spread + 0x200;
		int32_t b = spread / 2 - 0x100;
		for (uint32_t t = us(0xCD03AA); t < us(0xCD03EA); t += 8)
		{
			int32_t k = mul32(*(const int16_t *)(t + 2), 2200) >> 12;
			int16_t v[4];
			v[0] = (int16_t)(mul32(k, a) >> 12);
			v[2] = (int16_t)(mul32(k, depth) >> 12);
			v[1] = (int16_t)(mul32(k, b) >> 12);
			v[3] = 0;
			MatrixMultiplyVector(&m, v, v);
			v[0] = (int16_t)(v[0] + (int16_t)m.t[0]);
			v[1] = (int16_t)(v[1] + (int16_t)m.t[1]);
			v[2] = (int16_t)(v[2] + (int16_t)m.t[2]);
			ScreenSpriteMatrix(v, *(const int16_t *)t, 0);
			*(int16_t *)(h + 4) = *(const int16_t *)(t - 2);
			Cursor() = InitEffectSequenceFromData(h, OT(), 2, Cursor());
		}
		FieldFree(0xB4);
	}

	static int32_t FlareDepth(int32_t c) { return mul32(c, 45) - 0xE00; }

	static uint32_t __cdecl FlareTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t c = n->c;
		FlareDraw(n, n->s20, FlareDepth(c), FlareColour(c));
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t v = n->s22;
		n->s20 = (int16_t)(n->s20 + v);
		n->c++;
		n->s22 = (int16_t)(v - (int16_t)(v >> 4));
		return n->c >= 0x16 ? TASK_END : 0;
	}

	static bool RiserFade(int32_t c, int32_t *v) { if (c >= 6) return false; *v = 0x1000 - shl32(mul32(c, 341), 1); return true; }

	static void RiserDraw(const Node24 *n, int16_t y, int16_t s1C, bool fade, int32_t f)
	{
		PrimArgs a = Prim(0, n->a18, 0, n->p10, y, n->p14, s1C, s1C, s1C, &Frame(), us(0xCC78EC));
		SetFade(a, fade, f);
		PrimDraw(a);
	}

	static uint32_t __cdecl RiserTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t f = 0;
		bool fade = RiserFade(n->c, &f);
		RiserDraw(n, n->p12, n->s1C, fade, f);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t s = n->s1C, v = n->s22;
		n->p12 = (int16_t)(n->p12 + v);
		n->s1C = (int16_t)((int16_t)(s >> 6) + s);
		n->c++;
		n->s22 = (int16_t)((int32_t)v / 12 + v);
		return n->c >= 0x1E ? TASK_END : 0;
	}

	static void PuffSetup(uint8_t *h, uint8_t *s)
	{
		*(uint32_t *)h = us(0xCC6DC8);
		ff8fx::copy<(sizeof(Mat4x3))>(s + 8, &Frame());
		*(uint16_t *)(h + 0x24) = 0;
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static uint32_t __cdecl PuffTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 0x3E)
		{
			int32_t r = mul32(100 - n->c, 27);
			for (int k = 0; k < 8; k++)
			{
				int i = FreeRecord(pool, 0x18);
				if (i < 0) break;
				RecB *p = (RecB *)(pool + 0x18 * i);
				p->mask = 1;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0x800 + 0x600);
				int32_t a = CrtRand() % 0x1000;
				p->x = (int16_t)(mul32(ComputeSin(a), r) >> 12);
				int32_t ry = CrtRand() % 50;
				p->y = (int16_t)-ry;
				p->z = (int16_t)(mul32(ComputeCos(a), r) >> 12);
				int32_t jx = CrtRand() % 40;
				p->x = (int16_t)(p->x + (jx - 0x14));
				int32_t jz = CrtRand() % 40;
				p->z = (int16_t)(p->z + (jz - 0x14));
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		PuffSetup(h, s);
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *p = (RecB *)(pool + 0x18 * i);
			if (!(p->mask & 1)) continue;
			FX_HELD(MemoB(i, n);)
			LitSprite(h, &p->x, p->size, p->age);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0) p->mask = 0;
			else p->age++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		if (n->c < 0x44) return 0;
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *p = (RecB *)(pool + 0x18 * i);
			if (p->mask & 1) p->mask = 0;
		}
		return TASK_END;
	}

	static uint32_t __cdecl SparkTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 8)
		{
			int32_t r = mul32(n->c, 0x91) + 0xC8;
			for (int k = 0; k < 12; k++)
			{
				int i = FreeRecord(pool, 0x20);
				if (i < 0) break;
				RecA *p = (RecA *)(pool + 0x20 * i);
				p->mask = 0x10;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0x800 + 0x600);
				int32_t a = CrtRand() % 0x1000;
				int32_t sn = ComputeSin(a);
				*(int32_t *)(s + 0x28) = sn;
				int32_t cs = ComputeCos(a);
				*(int32_t *)(s + 0x30) = cs;
				p->x = (int16_t)(mul32(sn, r) >> 12);
				int32_t ry = CrtRand() % 50;
				p->y = (int16_t)-ry;
				p->z = (int16_t)(mul32(cs, r) >> 12);
				int32_t jx = CrtRand() % 60;
				p->x = (int16_t)(p->x + (jx - 0x1E));
				int32_t jz = CrtRand() % 60;
				p->z = (int16_t)(p->z + (jz - 0x1E));
				int32_t sp = CrtRand() % 100 + 0x96;
				p->vx = (int16_t)(mul32(sn, sp) >> 12);
				p->vz = (int16_t)(mul32(cs, sp) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		PuffSetup(h, s);
		for (int i = 0; i < POOL_N; i++)
		{
			RecA *p = (RecA *)(pool + 0x20 * i);
			if (!(p->mask & 0x10)) continue;
			FX_HELD(MemoA(i, n);)
			LitSprite(h, &p->x, p->size, p->age);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				p->mask = 0;
				continue;
			}
			p->age++;
			p->x = (int16_t)(p->x + p->vx);
			p->z = (int16_t)(p->z + p->vz);
			p->vx = (int16_t)(p->vx - (int16_t)(p->vx >> 3));
			p->vz = (int16_t)(p->vz - (int16_t)(p->vz >> 3));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	static uint32_t __cdecl RisingRingSpawnerTask(TaskNode *tn)
	{
		if (Paused()) return 0;
		Node24 *n = (Node24 *)tn;
		int16_t c = n->c;
		if (c < 0x14 && (int32_t)c % 8 == 1)
		{
			Node24 *r = (Node24 *)AddTaskToQueue(&Q1(), us(ORIG_RisingRingTask));
			r->c = 0;
			r->p10 = 0;
			r->p12 = 0;
			r->p14 = 0;
			r->a18 = (int16_t)(CrtRand() % 0x1000);
			r->a1A = 0x32;
			r->s1C = 0x2400;
			r->s22 = 0xBA;
			r->s20 = 0xBA;
		}
		n->c++;
		return n->c >= 0x1E ? TASK_END : 0;
	}

	static bool RisingRingFade(int32_t c, int32_t *v) { if (c < 0x12) return false; *v = mul32(c - 0x12, 341); return true; }

	static void RisingRingDraw(const Node24 *n, int16_t a18, int16_t s20, bool fade, int32_t f)
	{
		PrimArgs a = Prim(0, a18, 0, n->p10, n->p12, n->p14, n->s1C, s20, n->s1C, &Frame(), us(0xCCCB1C));
		SetFade(a, fade, f);
		PrimDraw(a);
	}

	static uint32_t __cdecl RisingRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t f = 0;
		bool fade = RisingRingFade(n->c, &f);
		RisingRingDraw(n, n->a18, n->s20, fade, f);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t v = n->s22;
		n->a18 = (int16_t)(n->a18 + n->a1A);
		n->s20 = (int16_t)(n->s20 + v);
		n->c++;
		n->s22 = (int16_t)((int32_t)v / 12 + v);
		return n->c >= 0x1E ? TASK_END : 0;
	}

	static uint32_t DotEmit(uint8_t *s, uint32_t pk)
	{
		GteReadFLAG(s + 0x50);
		if (*(uint32_t *)(s + 0x50) & 0x60000) return pk;
		GteReadSXY2((void *)(pk + 8));
		GteReadSZ3(s + 0x54);
		uint32_t bucket = *(uint32_t *)(s + 0x48) + 4 * (uint32_t)(*(int32_t *)(s + 0x54) >> (*(uint32_t *)(s + 0x4C) & 31));
		InsertPrimAutoDepth(bucket, (void *)pk);
		return pk + 0xC;
	}

	static uint32_t __cdecl DotTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x58);
		if (!Paused() && n->c <= 0x28)
		{
			int32_t r = 0x76C - mul32(n->c, 35);
			for (int k = 0; k < 10; k++)
			{
				int i = FreeRecord(pool, 0x20);
				if (i < 0) break;
				RecA *p = (RecA *)(pool + 0x20 * i);
				p->mask = 1;
				p->age = 0;
				int32_t a = CrtRand() % 0x1000;
				int32_t sn = ComputeSin(a);
				*(int32_t *)(s + 0x28) = sn;
				int32_t cs = ComputeCos(a);
				*(int32_t *)(s + 0x30) = cs;
				p->x = (int16_t)(mul32(sn, r) >> 12);
				int32_t ry = CrtRand() % 90;
				p->y = (int16_t)-ry;
				p->z = (int16_t)(mul32(cs, r) >> 12);
				int32_t sp = CrtRand() % 240 + 0x5A;
				p->vx = (int16_t)(mul32(sn, sp) >> 12);
				int32_t up = CrtRand() % 50;
				p->vy = (int16_t)(-10 - up);
				p->vz = (int16_t)(mul32(cs, sp) >> 12);
			}
		}
		GteSetRotMatrix(&Frame());
		GteSetTransVector(&Frame());
		uint32_t pk = Cursor();
		int32_t count = 0;
		*(uint32_t *)(s + 0x48) = OT();
		*(uint32_t *)(s + 0x4C) = 4;
		for (int i = 0; i < POOL_N; i++)
		{
			RecA *p = (RecA *)(pool + 0x20 * i);
			if (!(p->mask & 1)) continue;
			FX_HELD(MemoA(i, n);)
			GteLoadV0(&p->x);
			GteRTPS();
			int16_t age = p->age;
			*(uint32_t *)pk = 0x2000000;
			*(uint32_t *)(pk + 4) = *(const uint32_t *)(us(0xCD0428) + 4 * age);
			if (!Paused())
			{
				age = (int16_t)(age + 1);
				p->age = age;
				if (age >= 0x10) p->mask = 0;
				else
				{
					p->x = (int16_t)(p->x + p->vx);
					p->y = (int16_t)(p->y + p->vy);
					p->z = (int16_t)(p->z + p->vz);
					p->vx = (int16_t)(p->vx - (int16_t)(p->vx >> 3));
					p->vy = (int16_t)((int16_t)(p->vy >> 4) + p->vy);
					p->vz = (int16_t)(p->vz - (int16_t)(p->vz >> 3));
					count++;
				}
			}
			pk = DotEmit(s, pk);
		}
		Cursor() = pk;
		FieldFree(0x58);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	static bool SpinnerFade(int32_t c, int32_t *v)
	{
		if (c < 4) { *v = shl32(4 - c, 10); return true; }
		if (c >= 6) { *v = shl32(c - 6, 9); return true; }
		return false;
	}

	static void SpinnerDraw(const Node24 *n, int16_t a18, int16_t s1C, bool fade, int32_t f)
	{
		PrimArgs a = Prim(0, a18, 0, n->p10, n->p12, n->p14, s1C, n->s20, s1C, &Frame(), us(0xCC7A94));
		SetFade(a, fade, f);
		PrimDraw(a);
	}

	static uint32_t __cdecl SpinnerTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->e;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->e = (int16_t)(d - 1);
			return 0;
		}
		int32_t f = 0;
		bool fade = SpinnerFade(n->c, &f);
		SpinnerDraw(n, n->a18, n->s1C, fade, f);
		FX_HELD(MemoNode(n);)
		if (Paused()) return 0;
		int16_t v = n->s1E;
		n->a18 = (int16_t)(n->a18 + n->a1A);
		n->s1C = (int16_t)(n->s1C + v);
		n->c++;
		n->s1E = (int16_t)(v - (int16_t)((int32_t)v / 12));
		return n->c >= 0xE ? TASK_END : 0;
	}

	static const uint32_t TRAIL_RING = 0x2200DF8;
	static const uint32_t TRAIL_PTS_A = 0x21FF2E8;
	static const uint32_t TRAIL_PTS_B = 0x21FF340;
	static const uint32_t TRAIL_SPL_A = 0x21FFA78;
	static const uint32_t TRAIL_SPL_B = 0x21FFB78;

	static void TrailPoints(uint8_t *s)
	{
		int16_t *p0 = (int16_t *)(s + 0x20), *p1 = (int16_t *)(s + 0x28);
		GetEffectSpawnPosition(Sword(Variant()), 1, 0, p0);
		GetEffectSpawnPosition(Sword(Variant()), 1, 0x1000, p1);
		int32_t *d = (int32_t *)(s + 0x30);
		d[0] = (int32_t)p1[0] - p0[0];
		d[1] = (int32_t)p1[1] - p0[1];
		d[2] = (int32_t)p1[2] - p0[2];
		NormalizeVectorToFixedPoint(d, d);
		p1[0] = (int16_t)((int16_t)(mul32(d[0], 1400) >> 12) + p0[0]);
		p1[1] = (int16_t)((int16_t)(mul32(d[1], 1400) >> 12) + p0[1]);
		p1[2] = (int16_t)((int16_t)(mul32(d[2], 1400) >> 12) + p0[2]);
	}

	static int32_t TrailGather(const uint8_t *ring, int32_t idx, uint8_t *pa, uint8_t *pb)
	{
		int32_t cnt = 0;
		do
		{
			const uint8_t *r = ring + 0x14 * idx;
			if (!*(const uint32_t *)r) break;
			idx--;
			*(uint32_t *)(pa + 8 * cnt) = *(const uint32_t *)(r + 4);
			*(uint32_t *)(pa + 8 * cnt + 4) = *(const uint32_t *)(r + 8);
			*(uint32_t *)(pb + 8 * cnt) = *(const uint32_t *)(r + 0xC);
			*(uint32_t *)(pb + 8 * cnt + 4) = *(const uint32_t *)(r + 0x10);
			if (idx < 0) idx = 9;
			cnt++;
		} while (cnt < 10);
		return cnt;
	}

	static void TrailSplines(int32_t cnt, const uint8_t *pa, const uint8_t *pb, uint8_t *sa, uint8_t *sb)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x190);
		SplineSetup(cnt, pa, w);
		for (int32_t k = 0, t = 0; k < 32; k++, t += 0x1000) SplineEval(cnt, w, sa + 8 * k, t / 31);
		SplineSetup(cnt, pb, w);
		for (int32_t k = 0, t = 0; k < 32; k++, t += 0x1000) SplineEval(cnt, w, sb + 8 * k, t / 31);
		FieldFree(0x190);
	}

	static void TrailDraw(const uint8_t *sa, const uint8_t *sb, uint8_t *s)
	{
		GteSetRotMatrix(&Frame());
		GteSetTransVector(&Frame());
		GteSetFarColor2(0, 0, 0);
		uint32_t colour = *(const uint32_t *)(us(0xCD0490) + 4 * Variant());
		*(uint32_t *)(s + 0x14) = colour;
		*(uint32_t *)(s + 0x1C) = colour;
		uint32_t *pk = (uint32_t *)Cursor();
		uint32_t *pk2 = pk + 9;
		int32_t tacc = 0;
		for (int32_t off = 0; off < 0xF0; off += 8, tacc += 0x1000)
		{
			*(uint32_t *)(s + 0x10) = *(uint32_t *)(s + 0x14);
			pk[0] = 0x8000000;
			GteLoadV012(sa + off, sa + off + 8, sb + off);
			GteRTPT();
			GteReadFLAG(s + 0xC);
			if (*(uint32_t *)(s + 0xC) & 0x60000) continue;
			GteReadSXY012Split(pk + 2, pk + 4, pk + 6);
			GteLoadV0(sb + off + 8);
			GteRTPS();
			GteReadSXY2(pk + 8);
			GteAVSZ4();
			GteReadOTZ32(s + 8);
			GteSetIR0(tacc / 30);
			GteLoadRGBC(s + 0x1C);
			GteDPCS();
			GteStoreRGB2(s + 0x14);
			pk[5] = pk[1] = *(uint32_t *)(s + 0x10);
			pk[7] = pk[3] = *(uint32_t *)(s + 0x14);
			InsertPrimAutoDepth(RenderList() + 4 * (uint32_t)(*(int32_t *)(s + 8) >> 2) + 0x44, pk);
			pk2[5] = *(uint32_t *)(s + 0x10);
			pk2[3] = pk2[1] = 0x3A000000;
			pk2[7] = *(uint32_t *)(s + 0x14);
			pk2[4] = pk[4];
			pk2[2] = pk[2];
			pk2[6] = pk[6];
			pk2[8] = pk[8];
			pk2[0] = 0x8000000;
			InsertPrimAutoDepth(RenderList() + 4 * (uint32_t)(*(int32_t *)(s + 8) >> 2) + 0x44, pk2);
			pk += 0x12;
			pk2 += 0x12;
		}
		Cursor() = (uint32_t)pk2;
	}

	static uint32_t __cdecl TrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *s = (uint8_t *)FieldAlloc(0x40);
		TrailPoints(s);
		int32_t idx = (int32_t)n->c % 10;
		uint8_t *e = (uint8_t *)(us(TRAIL_RING) + 0x14 * idx);
		*(uint32_t *)e = 1;
		*(uint32_t *)(e + 4) = *(const uint32_t *)(s + 0x20);
		*(uint32_t *)(e + 8) = *(const uint32_t *)(s + 0x24);
		*(uint32_t *)(e + 0xC) = *(const uint32_t *)(s + 0x28);
		*(uint32_t *)(e + 0x10) = *(const uint32_t *)(s + 0x2C);
		int32_t cnt = TrailGather((const uint8_t *)us(TRAIL_RING), idx, (uint8_t *)us(TRAIL_PTS_A), (uint8_t *)us(TRAIL_PTS_B));
		FX_HELD(held_note_trail(cnt);)
		if (cnt > 1)
		{
			TrailSplines(cnt, (const uint8_t *)us(TRAIL_PTS_A), (const uint8_t *)us(TRAIL_PTS_B), (uint8_t *)us(TRAIL_SPL_A), (uint8_t *)us(TRAIL_SPL_B));
			FX_HELD(held_note_trail_splines();)
			TrailDraw((const uint8_t *)us(TRAIL_SPL_A), (const uint8_t *)us(TRAIL_SPL_B), s);
		}
		FieldFree(0x40);
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0xA ? TASK_END : 0;
	}

	static void SwordTipFrame(uint8_t *s, const Node24 *n)
	{
		*(int16_t *)(s + 0) = 0;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, (Mat4x3 *)(s + 8));
		GetEffectSpawnPosition(Sword(n->s1E), 1, 0x1000, (int16_t *)s);
		*(int32_t *)(s + 0x1C) = *(const int16_t *)(s + 0);
		*(int32_t *)(s + 0x20) = 0;
		*(int32_t *)(s + 0x24) = *(const int16_t *)(s + 4);
		ComposeAffineTransform(&Frame(), (const Mat4x3 *)(s + 8), (Mat4x3 *)(s + 8));
	}

	static void ShardDrawOne(uint8_t *h, uint8_t *s, int16_t a0, int16_t a2, const int16_t *pos, int16_t size)
	{
		*(int16_t *)(s + 0) = a0;
		*(int16_t *)(s + 4) = a2;
		Mat4x3 *m = (Mat4x3 *)(s + 0x28);
		ComposeZYXRotationMatrix((const int16_t *)s, m);
		m->t[0] = pos[0];
		m->t[2] = pos[2];
		m->t[1] = pos[1];
		int32_t *sc = (int32_t *)(s + 0x48);
		sc[2] = size;
		sc[1] = size;
		sc[0] = size;
		Scale3DMatrix(m, sc);
		ComposeAffineTransform((const Mat4x3 *)(s + 8), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
	}

	static uint32_t __cdecl SwordShardTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		uint8_t *pool = PoolA();
		uint8_t *s = (uint8_t *)FieldAlloc(0x68);
		if (!Paused() && n->c <= 1)
		{
			for (int k = 0; k < 6; k++)
			{
				int i = FreeRecord(pool, 0x20);
				if (i < 0) break;
				RecA *p = (RecA *)(pool + 0x20 * i);
				p->mask = (uint32_t)(int32_t)n->s1C;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0x100 + 0x40);
				int32_t a = CrtRand() % 0x1000;
				int32_t sn = ComputeSin(a);
				*(int32_t *)(s + 0x48) = sn;
				int32_t cs = ComputeCos(a);
				*(int32_t *)(s + 0x50) = cs;
				int32_t r = CrtRand() % 80;
				p->x = (int16_t)(mul32(sn, r) >> 12);
				int32_t ry = CrtRand() % 40;
				p->y = (int16_t)-ry;
				p->z = (int16_t)(mul32(r, cs) >> 12);
				p->w0E = p->size;
				int32_t sp = (CrtRand() % 140 + 0x3C) >> 1;
				p->vx = (int16_t)(mul32(sn, sp) >> 12);
				int32_t up = CrtRand() % 140;
				p->vy = (int16_t)(-60 - up);
				p->vz = (int16_t)(mul32(sp, cs) >> 12);
				p->w18 = (int16_t)(CrtRand() % 0x1000);
				int32_t s1 = CrtRand() % 40 + 0x14;
				p->w1A = (int16_t)s1;
				if (s1 & 1) p->w1A = (int16_t)-s1;
				p->w1C = (int16_t)(CrtRand() % 0x800);
				int32_t s2 = CrtRand() % 40 + 0x14;
				p->w1E = (int16_t)s2;
				if (s2 & 1) p->w1E = (int16_t)-s2;
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0;
		int32_t count = 0;
		*(uint32_t *)h = us(0xCC8E64);
		SwordTipFrame(s, n);
		*(int16_t *)(s + 2) = 0;
		FX_HELD(held_note_tip(n, s);)
		int32_t mask = n->s1C;
		for (int i = 0; i < POOL_N; i++)
		{
			RecA *p = (RecA *)(pool + 0x20 * i);
			if (!(mask & p->mask)) continue;
			FX_HELD(MemoA(i, n);)
			ShardDrawOne(h, s, p->w18, p->w1C, &p->x, p->size);
			if (p->age >= 8) p->size = (int16_t)(shl32(mul32(0x10 - p->age, p->w0E), 9) >> 12);
			Cursor() = RenderPrimModel(h, OT(), 2, Cursor());
			if (Paused()) continue;
			if (p->age >= 0x10)
			{
				p->mask = 0;
				continue;
			}
			p->age++;
			p->x = (int16_t)(p->x + p->vx);
			p->y = (int16_t)(p->y + p->vy);
			p->z = (int16_t)(p->z + p->vz);
			p->vx = (int16_t)(p->vx - (int16_t)(p->vx >> 3));
			p->vy = (int16_t)(p->vy - (int16_t)(p->vy >> 3));
			p->vz = (int16_t)(p->vz - (int16_t)(p->vz >> 3));
			p->w18 = (int16_t)(p->w18 + p->w1A);
			p->w1C = (int16_t)(p->w1C + p->w1E);
			count++;
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
	}

	static void SwordPuffSetup(uint8_t *h, uint8_t *s, const Node24 *n)
	{
		*(uint32_t *)h = us(0xCC6C34);
		*(uint16_t *)(h + 0x24) = 8;
		SwordTipFrame(s, n);
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}

	static uint32_t __cdecl SwordPuffTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int16_t d = n->e;
		if (d > 0)
		{
			if (Paused()) return 0;
			n->e = (int16_t)(d - 1);
			return 0;
		}
		uint8_t *pool = PoolB();
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		if (!Paused() && n->c <= 1)
		{
			for (int k = 0; k < 4; k++)
			{
				int i = FreeRecord(pool, 0x18);
				if (i < 0) break;
				RecB *p = (RecB *)(pool + 0x18 * i);
				p->mask = (uint32_t)(int32_t)n->s1C;
				p->age = 0;
				p->size = (int16_t)(CrtRand() % 0x400 + 0x400);
				int32_t a = CrtRand() % 0x1000;
				int32_t sn = ComputeSin(a);
				*(int32_t *)(s + 0x28) = sn;
				int32_t cs = ComputeCos(a);
				*(int32_t *)(s + 0x30) = cs;
				int32_t r = CrtRand() % 100;
				p->x = (int16_t)(mul32(r, sn) >> 12);
				int32_t ry = CrtRand() % 40;
				p->y = (int16_t)-ry;
				p->z = (int16_t)(mul32(cs, r) >> 12);
				int32_t sp = CrtRand() % 10 + 10;
				p->vx = (int16_t)(mul32(sp, sn) >> 12);
				p->vz = (int16_t)(mul32(cs, sp) >> 12);
			}
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t count = 0;
		SwordPuffSetup(h, s, n);
		FX_HELD(held_note_tip(n, s);)
		int32_t mask = n->s1C;
		for (int i = 0; i < POOL_N; i++)
		{
			RecB *p = (RecB *)(pool + 0x18 * i);
			if (!(mask & p->mask)) continue;
			FX_HELD(MemoB(i, n);)
			LitSpriteMAC(h, s, &p->x, p->size, p->age, 2, false, 0);
			if (Paused()) continue;
			if (*(int16_t *)(h + 0x28) < 0)
			{
				p->mask = 0;
				continue;
			}
			p->age++;
			p->x = (int16_t)(p->x + p->vx);
			p->z = (int16_t)(p->z + p->vz);
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (Paused()) return 0;
		n->c++;
		return count ? 0 : TASK_END;
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

	static void SetAnim(uint8_t *E, int32_t id) { PreReadAnimation(E + 0x60, E + 0x6C, id); }

	static void ModelMatrixInit(uint8_t *M, int32_t tx, int32_t ty, int32_t tz)
	{
		ResetRotation(M);
		*(uint16_t *)(M + 0x10) = 0x1000;
		*(uint16_t *)(M + 8) = 0x1000;
		*(uint16_t *)(M + 0) = 0x1000;
		*(int32_t *)(M + 0x1C) = tz;
		*(int32_t *)(M + 0x18) = ty;
		*(int32_t *)(M + 0x14) = tx;
	}

	static void AdvanceModel(uint8_t *E)
	{
		if (ReadAnimation(E + 0x60, E + 0x6C) == 1 && !(E[0] & 1))
			PreReadAnimation(E + 0x60, E + 0x6C, E[0x6C]);
		FX_HELD(held_note_advanced(E);)
	}

	static void DrawModel(uint8_t *E, const Mat4x3 *frame, int32_t shift)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		ComposeAffineTransform(frame, (const Mat4x3 *)(E + 0x40), (Mat4x3 *)h);
		ComputeBonesWorldMatrices(E + 0x60, h);
		*(uint32_t *)(h + 0x30) = var<uint32_t>(us(0x1D969A8));
		*(uint32_t *)(h + 0x24) = ModelScratch();
		*(uint16_t *)(h + 0x34) = 0;
		*(uint16_t *)(h + 0x36) = 0;
		*(uint32_t *)(h + 0x3C) = *(uint32_t *)(E + 0x28);
		*(uint32_t *)(h + 0x48) = 0;
		*(uint16_t *)(h + 0x44) = 0;
		*(uint16_t *)(h + 0x38) = 0x140;
		*(uint16_t *)(h + 0x3A) = 0xD8;
		*(uint32_t *)(h + 0x40) = *(uint32_t *)(E + 0x7C);
		Cursor() = RenderGeometry(*(void **)(E + 0x64), h + 0x20, OT(), shift, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
		FieldFree(0x4C);
	}

	static void ClipSetup(uint8_t *h)
	{
		int16_t *p = (int16_t *)(h + 0x58), *nrm = (int16_t *)(h + 0xB8);
		p[0] = 0;
		p[1] = 0;
		p[2] = 0;
		nrm[0] = 0;
		nrm[1] = (int16_t)0xF000;
		nrm[2] = 0;
		PlaneDistance(p, nrm);
		*(uint32_t *)(h + 4) = ModelScratch();
	}

	static void ClipDraw(uint8_t *h, uint8_t *E, const Mat4x3 *frame)
	{
		ComputeBonesWorldMatrices(E + 0x60, E + 0x40);
		*(uint32_t *)(h + 0x10) = *(uint32_t *)(E + 0x28);
		*(uint32_t *)(h + 0x14) = *(uint32_t *)(E + 0x7C);
		var<uint32_t>(us(0x21FF3C0)) = (uint32_t)frame;
		*(uint16_t *)(h + 0x18) = 0;
		Cursor() = ClipRender(*(void **)(E + 0x64), h, OT(), 2, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
	}

	static void MaterialDraw(uint8_t *h, uint8_t *E)
	{
		ComputeBonesWorldMatrices(E + 0x60, E + 0x40);
		*(uint32_t *)(h + 4) = ModelScratch();
		*(uint16_t *)(h + 0x14) = 0;
		*(uint16_t *)(h + 0x16) = 0;
		*(uint32_t *)(h + 0x1C) = *(uint32_t *)(E + 0x28);
		*(uint32_t *)(h + 0x20) = *(uint32_t *)(E + 0x7C);
		*(uint32_t *)(h + 0x10) = var<uint32_t>(us(0x1D969A8));
		h[0x2A] = E[7];
		h[0x29] = E[7];
		h[0x28] = E[7];
		*(uint16_t *)(h + 0x18) = 0x140;
		*(uint16_t *)(h + 0x1A) = 0xD8;
		Cursor() = MaterialRender(*(void **)(E + 0x64), h, OT(), 2, Cursor());
		BuildBoneMatricesFromPose(E + 0x60);
	}

	static uint32_t __cdecl SwordClipTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			for (int i = 0; i < 4; i++)
			{
				uint8_t *E = Sword(i);
				ModelMatrixInit(E + 0x40, 0, 0, 0);
				ModelInit(E, (uint8_t *)(us(0x2200D20) + 0x34 * i), *(uint8_t **)(us(0xCD03E8) + 4 * (i + 4 * Variant())), 0x10);
				SetAnim(E, n->e);
			}
		}
		var<uint8_t>(us(0x21FF4F0)) |= 4;
		uint8_t *h = (uint8_t *)FieldAlloc(0x1A8);
		ClipSetup(h);
		for (int i = 0; i < 4; i++)
		{
			ClipDraw(h, Sword(i), &Frame());
			FX_HELD(ModelMemoTake(Sword(i), n, DK_CLIP, 0);)
		}
		FieldFree(0x1A8);
		if (Paused()) return 0;
		int16_t c = n->c;
		if (c >= 10)
		{
			int32_t e = c - 10;
			if (e < 15)
			{
				if (e == 0) var<uint8_t>(us(0x21FF4F0)) |= 8;
			}
			else if ((e -= 15) >= 20)
			{
				e -= 20;
				if (e < 60 && e == 0) var<uint16_t>(us(0x21FF4F0)) &= 0xFFF7;
			}
		}
		n->c = (int16_t)(c + 1);
		return n->c >= 0x69 ? TASK_END : 0;
	}

	static uint32_t __cdecl SwordDrawTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (!(var<uint8_t>(us(0x21FF4F0)) & 4))
		{
			for (int i = 0; i < 4; i++)
			{
				uint8_t *E = Sword(i);
				if (i != 0)
				{
					ff8fx::copy<(sizeof(Mat4x3))>(E + 0x40, (const void *)us(0x21FF530));
					*(uint32_t *)(E + 0x28) = var<uint32_t>(us(0x21FF518));
				}
				DrawModel(E, &Frame(), 4);
				FX_HELD(ModelMemoTake(E, n, DK_NORMAL4, 0);)
			}
		}
		if (Paused()) return 0;
		if (!(var<uint8_t>(us(0x21FF4F0)) & 8))
			for (int i = 0; i < 4; i++) AdvanceModel(Sword(i));
		n->c++;
		return n->c >= n->e ? TASK_END : 0;
	}

	static uint32_t __cdecl Gil1Task(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			ModelMatrixInit(GIL1() + 0x40, 0, 0, 0);
			ModelInit(GIL1(), (uint8_t *)us(0x21FF4B8), (uint8_t *)MB(), 0x20);
		}
		int32_t c = n->c;
		if (c < 10)
		{
			if (c == 0)
			{
				GIL1()[0] |= 4;
				SetAnim(GIL1(), 0);
			}
		}
		else if (c - 10 < 15)
		{
			if (c == 10) GIL1()[0] |= 8;
		}
		else if (c - 25 < 20)
		{
			uint32_t buf = MB() + 0x22000;
			if (c == 25)
				for (int k = 0; k < 0x200; k++) ((uint32_t *)buf)[k] = 0x40004000;
			uint8_t *h = (uint8_t *)FieldAlloc(0xB0);
			*(uint32_t *)(h + 0x2C) = buf;
			*(int32_t *)(h + 0x44) = c - 25;
			MaterialDraw(h, GIL1());
			FieldFree(0xB0);
			FX_HELD(ModelMemoTake(GIL1(), n, DK_MATERIAL, c - 25);)
		}
		else if (c - 45 < 60)
		{
			if (c == 45) *(uint16_t *)GIL1() &= 0xFFF3;
		}
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x3C ? TASK_END : 0;
	}

	static uint32_t __cdecl Gil1DrawTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (!(GIL1()[0] & 4))
		{
			DrawModel(GIL1(), &Frame(), 4);
			FX_HELD(ModelMemoTake(GIL1(), n, DK_NORMAL4, 0);)
		}
		if (Paused()) return 0;
		if (!(GIL1()[0] & 8)) AdvanceModel(GIL1());
		n->c++;
		return n->c >= 0x69 ? TASK_END : 0;
	}

	static uint32_t __cdecl Gil2Task(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			ModelMatrixInit(GIL2() + 0x40, 0, -1000, 0);
			ModelInit(GIL2(), (uint8_t *)us(0x21FF2B0), (uint8_t *)(MB() + 0x22000), 0x80);
			SetAnim(GIL2(), 0);
			uint8_t *buf = (uint8_t *)(MB() + 0x2379C);
			for (int j = 0; j < 4; j++, buf += 0x1860)
			{
				ModelInit(Arm(j), (uint8_t *)(us(0x21FFC78) + 0x34 * j), buf, 0x40);
				SetAnim(Arm(j), j);
			}
		}
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x2A ? TASK_END : 0;
	}

	static uint32_t __cdecl Gil2DrawTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (!(GIL2()[0] & 4))
		{
			DrawModel(GIL2(), &Frame(), 3);
			FX_HELD(ModelMemoTake(GIL2(), n, DK_NORMAL3, 0);)
			for (int j = 0; j < 4; j++)
			{
				uint8_t *A = Arm(j);
				*(uint16_t *)A = *(uint16_t *)GIL2();
				ff8fx::copy<(sizeof(Mat4x3))>(A + 0x40, (const void *)us(0x2200F40));
				*(int32_t *)(A + 0x5C) += 0x50;
				*(uint32_t *)(A + 0x28) = var<uint32_t>(us(0x2200F28));
				DrawModel(A, &Frame(), 4);
				FX_HELD(ModelMemoTake(A, n, DK_NORMAL4, 0);)
			}
		}
		if (Paused()) return 0;
		if (!(GIL2()[0] & 8))
		{
			AdvanceModel(GIL2());
			for (int j = 0; j < 4; j++) AdvanceModel(Arm(j));
		}
		n->c++;
		return n->c >= 0x2A ? TASK_END : 0;
	}

	static uint32_t __cdecl Gil3Task(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 0)
		{
			ModelMatrixInit(GIL3() + 0x40, 0, 0, 0);
			ModelInit(GIL3(), (uint8_t *)us(0x2200EC8), (uint8_t *)MB(), 0xF);
			SetAnim(GIL3(), 0);
			for (int i = 0; i < 4; i++)
			{
				ModelInit(Sword(i), (uint8_t *)(us(0x2200D20) + 0x34 * i), *(uint8_t **)(us(0xCD03E8) + 4 * (i + 4 * Variant())), 0x10);
				SetAnim(Sword(i), Variant() + 4);
			}
		}
		if (Paused()) return 0;
		n->c++;
		return n->c >= 0x32 ? TASK_END : 0;
	}

	static uint32_t __cdecl Gil3DrawTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (!(GIL3()[0] & 4))
		{
			DrawModel(GIL3(), &Frame(), 4);
			FX_HELD(ModelMemoTake(GIL3(), n, DK_NORMAL4, 0);)
			for (int i = 0; i < 4; i++)
			{
				uint8_t *E = Sword(i);
				*(uint16_t *)E = *(uint16_t *)GIL3();
				ff8fx::copy<(sizeof(Mat4x3))>(E + 0x40, (const void *)us(0x2201000));
				*(uint32_t *)(E + 0x28) = var<uint32_t>(us(0x2200FE8));
				DrawModel(E, &Frame(), 4);
				FX_HELD(ModelMemoTake(E, n, DK_NORMAL4, 0);)
			}
		}
		if (Paused()) return 0;
		if (!(GIL2()[0] & 8))
		{
			AdvanceModel(GIL3());
			for (int i = 0; i < 4; i++) AdvanceModel(Sword(i));
		}
		n->c++;
		return n->c >= 0x32 ? TASK_END : 0;
	}
}

	void register_mag327_gilgamesh()
	{
		using namespace g327;
		register_port(ORIG_SequenceTick, (void *)SequenceTick, "G327 SequenceTick", 327);
		register_port(ORIG_TimelineTask, (void *)TimelineTask, "G327 TimelineTask", 327);
		register_port(ORIG_FeedbackTask, (void *)FeedbackTask, "G327 FeedbackTask", 327);
		register_port(ORIG_Feedback2Task, (void *)Feedback2Task, "G327 Feedback2Task", 327);
		register_port(ORIG_FlashInTask, (void *)FlashInTask, "G327 FlashInTask", 327);
		register_port(ORIG_FlashOutTask, (void *)FlashOutTask, "G327 FlashOutTask", 327);
		register_port(ORIG_FadeTileTask, (void *)FadeTileTask, "G327 FadeTileTask", 327);
		register_port(ORIG_RingTask, (void *)RingTask, "G327 RingTask", 327);
		register_port(ORIG_RingSpinTask, (void *)RingSpinTask, "G327 RingSpinTask", 327);
		register_port(ORIG_RingSpin2Task, (void *)RingSpin2Task, "G327 RingSpin2Task", 327);
		register_port(ORIG_FlareTask, (void *)FlareTask, "G327 FlareTask", 327);
		register_port(ORIG_RiserTask, (void *)RiserTask, "G327 RiserTask", 327);
		register_port(ORIG_PuffTask, (void *)PuffTask, "G327 PuffTask", 327);
		register_port(ORIG_SparkTask, (void *)SparkTask, "G327 SparkTask", 327);
		register_port(ORIG_RisingRingSpawnerTask, (void *)RisingRingSpawnerTask, "G327 RisingRingSpawnerTask", 327);
		register_port(ORIG_RisingRingTask, (void *)RisingRingTask, "G327 RisingRingTask", 327);
		register_port(ORIG_DotTask, (void *)DotTask, "G327 DotTask", 327);
		register_port(ORIG_SpinnerTask, (void *)SpinnerTask, "G327 SpinnerTask", 327);
		register_port(ORIG_TrailTask, (void *)TrailTask, "G327 TrailTask", 327);
		register_port(ORIG_SwordShardTask, (void *)SwordShardTask, "G327 SwordShardTask", 327);
		register_port(ORIG_SwordPuffTask, (void *)SwordPuffTask, "G327 SwordPuffTask", 327);
		register_port(ORIG_SwordClipTask, (void *)SwordClipTask, "G327 SwordClipTask", 327);
		register_port(ORIG_SwordDrawTask, (void *)SwordDrawTask, "G327 SwordDrawTask", 327);
		register_port(ORIG_Gil1Task, (void *)Gil1Task, "G327 Gil1Task", 327);
		register_port(ORIG_Gil1DrawTask, (void *)Gil1DrawTask, "G327 Gil1DrawTask", 327);
		register_port(ORIG_Gil2Task, (void *)Gil2Task, "G327 Gil2Task", 327);
		register_port(ORIG_Gil2DrawTask, (void *)Gil2DrawTask, "G327 Gil2DrawTask", 327);
		register_port(ORIG_Gil3Task, (void *)Gil3Task, "G327 Gil3Task", 327);
		register_port(ORIG_Gil3DrawTask, (void *)Gil3DrawTask, "G327 Gil3DrawTask", 327);
		register_port(ORIG_EndTask, (void *)EndTask, "G327 EndTask", 327);
		register_port(ORIG_Zan_Task, (void *)ZanTask, "G327 ZanTask", 327);
		register_port(ORIG_Zan_ScreenPrimTask, (void *)ZanScreenPrimTask, "G327 ZanScreenPrimTask", 327);
		register_port(ORIG_Zan_CutTask, (void *)ZanCutTask, "G327 ZanCutTask", 327);
		register_port(ORIG_Zan_SliceTask, (void *)ZanSliceTask, "G327 ZanSliceTask", 327);
		register_port(ORIG_Zan_PartyClipTask, (void *)ZanPartyClipTask, "G327 ZanPartyClipTask", 327);
		register_port(ORIG_Masa_Task, (void *)MasaTask, "G327 MasaTask", 327);
		register_port(ORIG_Masa_ScreenPrimTask, (void *)MasaScreenPrimTask, "G327 MasaScreenPrimTask", 327);
		register_port(ORIG_Masa_PrimATask, (void *)MasaPrimATask, "G327 MasaPrimATask", 327);
		register_port(ORIG_Masa_PrimBTask, (void *)MasaPrimBTask, "G327 MasaPrimBTask", 327);
		register_port(ORIG_Masa_SparkTask, (void *)MasaSparkTask, "G327 MasaSparkTask", 327);
		register_port(ORIG_Masa_DustTask, (void *)MasaDustTask, "G327 MasaDustTask", 327);
		register_port(ORIG_Exca_Task, (void *)ExcaTask, "G327 ExcaTask", 327);
		register_port(ORIG_Poor_Task, (void *)PoorTask, "G327 PoorTask", 327);
		register_port(ORIG_Exca_ScreenPrimTask, (void *)ExcaScreenPrimTask, "G327 ExcaScreenPrimTask", 327);
		register_port(ORIG_Exca_BurstTask, (void *)ExcaBurstTask, "G327 ExcaBurstTask", 327);
		register_port(ORIG_Exca_SmokeTask, (void *)ExcaSmokeTask, "G327 ExcaSmokeTask", 327);
		register_port(ORIG_Exca_StreakTask, (void *)ExcaStreakTask, "G327 ExcaStreakTask", 327);
		register_port(ORIG_Exca_PillarTask, (void *)ExcaPillarTask, "G327 ExcaPillarTask", 327);
		FX_HELD(register_mag327_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag327_gilgamesh_held.inc"
#endif
