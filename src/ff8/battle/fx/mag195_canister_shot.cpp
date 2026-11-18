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

// Effect 195: Canister Shot

#include "mag_common.h"

namespace ff8fx
{
namespace canister195
{
	using namespace eng;
	using namespace magc;

	inline int32_t &EndTick() { return var<int32_t>(us(0x226A970)); }
	inline int32_t &ModuleTick() { return var<int32_t>(us(0x226A974)); }
	inline uint32_t &Flags() { return var<uint32_t>(us(0x226EDB0)); }
	inline CastContext *&Ctx() { return var<CastContext *>(us(0x226FBD8)); }
	inline int32_t &CasterSlot() { return var<int32_t>(us(0x22711C0)); }
	inline int32_t &PartCount() { return var<int32_t>(us(0x2271460)); }
	inline uint32_t &TexBase() { return var<uint32_t>(us(0x2271464)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(us(0x2271468)); }
	inline TaskQueue *QRoot() { return (TaskQueue *)us(0x226B7D0); }
	inline TaskQueue *QShots() { return (TaskQueue *)us(0x226B7E0); }
	inline TaskQueue *QAux() { return (TaskQueue *)us(0x226EDB8); }
	static const uint32_t PARTS_A = 0x226B7F0, PARTS_B = 0x226DF20;
	static const int32_t PARTS_N = 150;
	static const uint32_t DEBRIS_ODD = 0x226C600, DEBRIS_EVEN = 0x226D280;
	static const int32_t DEBRIS_N = 100;
	static const uint32_t SHOTS = 0x226FBE0;
	static const uint32_t BIND_OUT = 0x226DF00;
	static const uint32_t BONES_NOW = 0x226ED30;
	static const uint32_t GHOSTS = 0x22711C8, GHOSTS_END = 0x227145C;
	static const uint32_t ARENA_ODD = 0xE000;

	static const uint32_t ORIG_RootTask = 0x5B4BC0;
	static const uint32_t ORIG_StartTask = 0x5B4790;
	static const uint32_t ORIG_ShotTask = 0x5B2BF0;
	static const uint32_t ORIG_EndTask = 0x5B4670;
	static const uint32_t ORIG_FadeOutTask = 0x5B4730;
	static const uint32_t ORIG_FadeInTask = 0x5B4880;
	static const uint32_t ORIG_AfterimageTask = 0x5B48D0;
	static const uint32_t ORIG_SmokeTask = 0x5B32A0;
	static const uint32_t ORIG_GroundFlashTask = 0x5B3340;
	static const uint32_t ORIG_MuzzleConeTask = 0x5B3490;
	static const uint32_t ORIG_MuzzleRingTask = 0x5B3620;
	static const uint32_t ORIG_SteamTask = 0x5B37D0;
	static const uint32_t ORIG_BoneSmokeTask = 0x5B3A80;
	static const uint32_t ORIG_ImpactTask = 0x5B3DC0;
	static const uint32_t ORIG_ImpactSmokeTask = 0x5B3F60;
	static const uint32_t ORIG_ImpactFlashTask = 0x5B3FF0;
	static const uint32_t ORIG_DebrisTask = 0x5B4140;
	static const uint32_t ORIG_ImpactConeTask = 0x5B44C0;

	static const uint32_t ANIM_DATA = 0xD07400;
	static const uint32_t MODEL_GroundFlash = 0xD0A978, MODEL_Cone = 0xD0AFE0, MODEL_Ring = 0xD0B488, MODEL_ImpactFlash = 0xD09FB8, MODEL_Debris = 0xD0AD68;
	static const uint32_t SEQ_Smoke = 0xD099F0, SEQ_Steam = 0xD09AB4, SEQ_BoneSmoke = 0xD09C60, SEQ_Impact = 0xD09E0C, SEQ_ImpactSmoke = 0xD097CC;
	static const void *SOUND_Gun() { return (const void *)us(0xD0BD30); }
	static const int32_t *MUZZLE_REF() { return (const int32_t *)us(0xD0BD38); }
	static const int32_t *RING_REF() { return (const int32_t *)us(0xD0BD48); }
	static const uint32_t *GHOST_COLOURS() { return (const uint32_t *)us(0xD0BD58); }

	inline uint32_t BindExternalAnimSet(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(us(0x5022C0))(entity, out, data); }
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(us(0x505C00))(entity, id); }
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(us(0x56BC50))(in, out); }
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(us(0x56C4F0))(m, in, out); }
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(us(0x56C270))(a, b); }
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(us(0x5099D0))(model, ctx, ot, a, cursor); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t angle, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(us(0x502170))(entity, bone, angle, out); }
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(us(0x45DE50))(m); }
	inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(us(0x64DDA0))(m); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(us(0x4607E0))(); }
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(us(0x64DE00))(s); }
	inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(us(0x64DD70))(v); }
	inline uint32_t &FramePackets() { return var<uint32_t>(us(0x1D8E054)); }

#pragma pack(push, 1)
	struct RootNode
	{
		TaskNode hdr;
		int16_t counter;
		int16_t pad0E;
	};
	struct Node
	{
		TaskNode hdr;
		int16_t counter;
		int16_t f0E;
		int16_t pos[4];
		int16_t f18;
		int16_t f1A;
		int16_t f1C;
		int16_t f1E;
		int16_t f20;
		int16_t f22;
	};
	struct ShotRec
	{
		int16_t pos[4];
		int32_t dir[3];
		uint8_t pad14[4];
		Mat4x3 m;
	};
	struct Particle
	{
		int32_t id;
		int16_t frame;
		int16_t size;
		int16_t pos[4];
		int16_t vel[3];
		int16_t pad;
	};
	struct Debris
	{
		int32_t id;
		int16_t counter;
		int16_t size;
		int16_t pos[4];
		int16_t vel[3];
		int16_t pad16;
		int16_t ax, sx;
		int16_t az, sz;
	};
	struct Ghost
	{
		int32_t used;
		uint8_t bones[0x80];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node) == 0x24, "Canister Shot nodes");
	static_assert(sizeof(ShotRec) == 0x38 && sizeof(Particle) == 0x18 && sizeof(Debris) == 0x20 && sizeof(Ghost) == 0x84, "Canister Shot records");

	inline ShotRec *Shot(int32_t i) { return (ShotRec *)(us(SHOTS) + 0x38 * i); }
	inline Ghost *GhostSlot(int32_t i) { return (Ghost *)(us(GHOSTS) + 0x84 * i); }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline Node *AddTask(TaskQueue *q, uint32_t fn) { return (Node *)AddTaskToQueue(q, fn); }
	inline Particle *PartArray(int16_t k) { return (Particle *)((k & 1) ? us(PARTS_B) : us(PARTS_A)); }
	inline Debris *DebrisArray(int16_t k) { return (Debris *)((k & 1) ? us(DEBRIS_ODD) : us(DEBRIS_EVEN)); }
	inline int32_t FindFree(const Particle *arr) { for (int32_t i = 0; i < PARTS_N; i++) if (arr[i].id == 0) return i; return -1; }
}
}

#ifdef FF8_FX_HELD
#include "mag195_canister_shot_held.h"
#endif

namespace ff8fx
{
namespace canister195
{
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

	static void BoneSnapshot(uint8_t *w, uint8_t *dst)
	{
		const uint8_t *sk = *(const uint8_t **)*(uint8_t **)(w + 4);
		const int32_t n = sk[0];
		const uint8_t *src = sk + 0x20;
		for (int32_t k = 0; k < n; k++, src += 0x30, dst += 0x20) ff8fx::copy<(0x20)>(dst, src);
	}
	static void BoneRestore(uint8_t *w, const uint8_t *src)
	{
		uint8_t *sk = *(uint8_t **)*(uint8_t **)(w + 4);
		uint8_t *dst = sk + 0x20;
		for (int32_t k = 0; k < (int32_t)sk[0]; k++, src += 0x20, dst += 0x30) ff8fx::copy<(0x20)>(dst, src);
	}

	static void GhostDraw(uint8_t *entity)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		uint8_t *w = *(uint8_t **)(entity + 0x78);
		if (w && !(entity[1] & 2))
		{
			Mat4x3 *m = (Mat4x3 *)h;
			ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), m);
			*(uint16_t *)(h + 0x34) = 0;
			*(uint16_t *)(h + 0x36) = 0;
			*(uint32_t *)(h + 0x24) = var<uint32_t>(us(0x1D98B3C));
			*(uint32_t *)(h + 0x3C) = *(const uint32_t *)(entity + 0x28);
			*(uint16_t *)(h + 0x38) = 0x140;
			*(uint16_t *)(h + 0x3A) = 0xD8;
			*(uint32_t *)(h + 0x30) = var<uint32_t>(us(0x1D969A8));
			*(uint32_t *)(h + 0x40) = 0xFFFFFFFF;
			const uint8_t c = entity[7];
			h[0x4A] = c;
			h[0x49] = c;
			h[0x48] = c;
			ComposeAffineTransform(m, (const Mat4x3 *)(w + 0x24), m);
			ComputeBonesWorldMatrices(w, m);
			FramePackets() = RenderGeometry(*(const uint32_t *)(w + 4), h + 0x20, RenderOT(), 4, FramePackets());
		}
		FieldFree(0x4C);
	}

	static void ModelDraw(uint32_t model, bool faded, int32_t fade)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (faded)
		{
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) = 0xF3;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
	static bool GroundFlashFaded(int32_t c) { return c == 0 || c >= 2; }
	static int32_t GroundFlashFade(int32_t c) { return c == 0 ? 0x800 : mul32(c - 2, 0x333); }
	static int32_t ConeFade(int32_t c) { return (int32_t)((uint32_t)(c - 2) << 9); }
	static int32_t RingFade(int32_t c) { return (int32_t)((uint32_t)(c - 6) << 9); }
	static int32_t ImpactFlashFade(int32_t c) { return (int32_t)((uint32_t)(c - 4) << 9); }
	static int32_t ImpactConeFade(int32_t c) { return mul32(c, 0x199); }
	static int32_t DebrisFade(int32_t c) { return (int32_t)((uint32_t)(c - 6) << 9); }

	static void GroundFlashDraw(const Node *n, bool faded, int32_t fade)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = n->f18;
		ang[2] = 0;
		ang[3] = 0;
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = s->pos[0];
		m.t[1] = 0;
		m.t[2] = s->pos[2];
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = us(MODEL_GroundFlash);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (faded)
		{
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) = 0xF3;
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void ConeDraw(const Node *n, bool faded, int32_t fade)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = n->f18;
		ang[3] = 0;
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		GteMatrixMultiply(&s->m, &m);
		m.t[0] = s->pos[0];
		m.t[1] = s->pos[1];
		m.t[2] = s->pos[2];
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		Scale3DMatrix(&m, v);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		ModelDraw(us(MODEL_Cone), faded, fade);
	}

	static void RingDraw(const Node *n, bool faded, int32_t fade)
	{
		const ShotRec *s = Shot(n->f0E);
		int16_t ang[4];
		ang[2] = 0;
		ang[0] = n->f18;
		ang[1] = n->f20;
		ang[3] = 0;
		Mat4x3 a, b;
		ComposeZYXRotationMatrix(ang, &a);
		int32_t axis[3];
		const int32_t angle = RotationBetweenVectors(RING_REF(), s->dir, axis);
		BuildAxisAngleRotationMatrix(angle, &b, axis);
		GteMatrixMultiply(&b, &a);
		a.t[0] = s->pos[0];
		a.t[1] = s->pos[1];
		a.t[2] = s->pos[2];
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		Scale3DMatrix(&a, v);
		ComposeAffineTransform(&Camera(), &a, &a);
		GteSetRotMatrix(&a);
		GteSetTransVector(&a);
		ModelDraw(us(MODEL_Ring), faded, fade);
	}

	static void ImpactModelDraw(const Node *n, int16_t roll, uint32_t model, bool faded, int32_t fade)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = roll;
		ang[3] = 0;
		Mat4x3 m;
		ComposeZYXRotationMatrix(ang, &m);
		GteMatrixMultiply(&Shot(n->f0E)->m, &m);
		const int32_t sc = n->f1C;
		const int32_t v[3] = { sc, sc, sc };
		Scale3DMatrix(&m, v);
		TransformCameraByShadowRotation(n->pos, n->f1C, -(int32_t)n->pos[3]);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		ModelDraw(model, faded, fade);
	}

	static int16_t SeqDraw(const int16_t *pt, int16_t size, int32_t depth, uint32_t seq, int16_t frame, int16_t f24)
	{
		TransformCameraByShadowRotation(pt, size, depth);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(int16_t *)(h + 0x24) = f24;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
		return *(const int16_t *)(h + 0x28);
	}
	static int16_t SmokeDraw(const Node *n) { return SeqDraw(Shot(n->f0E)->pos, n->f1C, -((int32_t)n->f1C >> 3), us(SEQ_Smoke), n->counter, 0); }
	static int16_t ImpactDraw(const Node *n) { return SeqDraw(n->pos, n->f1C, -(int32_t)n->pos[3], us(SEQ_Impact), n->counter, 0); }
	static int16_t ImpactSmokeDraw(const Node *n) { return SeqDraw(n->pos, n->f1C, -(int32_t)n->pos[3], us(SEQ_ImpactSmoke), n->counter, 8); }

	static void PartLight(uint8_t *s)
	{
		ff8fx::copy<(0x20)>(s + 8, &Camera());
		GteSetLightMatrix(s + 8);
		GteSetBackColorFromTrans(s + 8);
	}
	static void PartDraw(const Particle *e, uint8_t *s, uint8_t *h)
	{
		GteLoadV0(e->pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(e->size);
		*(int16_t *)(h + 4) = e->frame;
		int32_t *v = (int32_t *)(s + 0x38), *d = (int32_t *)(s + 0x28);
		GteReadMAC123(v);
		Normalize(v, d);
		const int32_t k = -((int32_t)e->size >> 4);
		v[0] += mul32(d[0], k) >> 12;
		v[1] += mul32(d[1], k) >> 12;
		v[2] += mul32(d[2], k) >> 12;
		GteSetTransFromVec32(v);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	static void DebrisDraw(const Debris *e, uint8_t *s, uint8_t *h, bool faded, int32_t fade)
	{
		*(int16_t *)s = e->ax;
		*(int16_t *)(s + 2) = 0;
		*(int16_t *)(s + 4) = e->az;
		Mat4x3 *m = (Mat4x3 *)(s + 0x28);
		BuildRotationMatrixFromAngles((const int16_t *)s, m);
		m->t[0] = e->pos[0];
		m->t[1] = e->pos[1];
		m->t[2] = e->pos[2];
		int32_t *v = (int32_t *)(s + 0x48);
		v[2] = e->size;
		v[1] = e->size;
		v[0] = e->size;
		Scale3DMatrix(m, v);
		ComposeAffineTransform((const Mat4x3 *)(s + 8), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		if (faded)
		{
			*(uint32_t *)(h + 0x1C) = 0xC3;
			*(int32_t *)(h + 0xC) = fade;
		}
		else *(uint32_t *)(h + 0x1C) = 0xC;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
	}
	static void DebrisBegin(uint8_t *s, uint8_t *h)
	{
		ff8fx::copy<(0x20)>(s + 8, &Camera());
		*(uint32_t *)h = us(MODEL_Debris);
		*(uint32_t *)(h + 8) = 0;
	}

	static uint32_t __cdecl SmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		FX_HELD(held_note_node(us(ORIG_SmokeTask), n);)
		const int16_t next = SmokeDraw(n);
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}

	static uint32_t __cdecl GroundFlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		FX_HELD(held_note_node(us(ORIG_GroundFlashTask), n);)
		GroundFlashDraw(n, GroundFlashFaded(n->counter), GroundFlashFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= 7 ? TASK_END : 0;
	}

	static uint32_t __cdecl MuzzleConeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		FX_HELD(held_note_node(us(ORIG_MuzzleConeTask), n);)
		ConeDraw(n, n->counter >= 2, ConeFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t spin = n->f1A;
		n->f18 = (int16_t)(n->f18 + spin);
		n->f1A = (int16_t)(spin - (int16_t)(spin / 10));
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 5));
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	static uint32_t __cdecl MuzzleRingTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		FX_HELD(held_note_node(us(ORIG_MuzzleRingTask), n);)
		RingDraw(n, n->counter >= 6, RingFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t spin = n->f1A;
		n->f18 = (int16_t)(n->f18 + spin);
		n->f1A = (int16_t)(spin - (int16_t)(spin / 10));
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 8));
		n->counter++;
		return n->counter >= 14 ? TASK_END : 0;
	}

	static uint32_t __cdecl SteamTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		const ShotRec *rec = Shot(n->f0E);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		const int32_t id = n->pos[3] + 1;
		if (!(UpdateFlags() & 0x201) && n->counter <= 3)
		{
			for (int32_t j = 0; j < 2; j++)
			{
				const int32_t i = FindFree(arr);
				if (i < 0) break;
				Particle *e = &arr[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x280 + 0x580);
				ff8fx::copy<(8)>(e->pos, rec->pos);
				e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 0xF0 - 0x78));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 0xF0 - 0x78));
				e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 0xF0 - 0x78));
				e->vel[1] = (int16_t)(-5 - CrtRand() % 10);
			}
		}
		FX_HELD(held_note_node(us(ORIG_SteamTask), n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t alive = 0;
		*(int16_t *)(h + 0x24) = 0;
		PartLight(s);
		*(uint32_t *)h = us(SEQ_Steam);
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			FX_HELD(held_note_part(e);)
			PartDraw(e, s, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
			alive++;
		}
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	static uint32_t __cdecl BoneSmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Particle *arr = PartArray(n->f0E);
		uint8_t *s = (uint8_t *)FieldAlloc(0x48);
		const int32_t id = n->pos[3] + 1;
		if (!(UpdateFlags() & 0x201) && n->counter <= 3)
		{
			for (int32_t j = 0; j < 2; j++)
			{
				const int32_t i = FindFree(arr);
				if (i < 0) break;
				Particle *e = &arr[i];
				e->id = id;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x280 + 0x320);
				GetEffectSpawnPosition(Caster(), 0x1D, 0xC00, e->pos);
				e->pos[0] = (int16_t)(e->pos[0] + (CrtRand() % 0x32 - 0x19));
				e->pos[1] = (int16_t)(e->pos[1] + (CrtRand() % 0x32 - 0x19));
				e->pos[2] = (int16_t)(e->pos[2] + (CrtRand() % 0x32 - 0x19));
				Particle *e2 = (Particle *)((uint32_t)arr + 0x18 * (uint32_t)(i + 1));
				e2->id = id;
				e2->frame = 0;
				e2->size = (int16_t)(CrtRand() % 0x280 + 0x320);
				GetEffectSpawnPosition(Caster(), 0x1E, 0xC00, e2->pos);
				e2->pos[0] = (int16_t)(e2->pos[0] + (CrtRand() % 0x32 - 0x19));
				e2->pos[1] = (int16_t)(e2->pos[1] + (CrtRand() % 0x32 - 0x19));
				e2->pos[2] = (int16_t)(e2->pos[2] + (CrtRand() % 0x32 - 0x19));
			}
		}
		FX_HELD(held_note_node(us(ORIG_BoneSmokeTask), n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(int16_t *)(h + 0x24) = 8;
		PartLight(s);
		*(uint32_t *)h = us(SEQ_BoneSmoke);
		int32_t alive = 0;
		for (Particle *e = arr; e < arr + PARTS_N; e++)
		{
			if (e->id != id) continue;
			FX_HELD(held_note_part(e);)
			PartDraw(e, s, h);
			if (UpdateFlags() & 0x201) continue;
			e->frame++;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				e->id = 0;
				FX_HELD(held_note_part_next(e);)
				continue;
			}
			e->size = (int16_t)(e->size + (int16_t)(e->size >> 4));
			alive++;
		}
		FX_HELD(held_note_alive(alive);)
		FieldFree(0xB4);
		FieldFree(0x48);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	static uint32_t __cdecl ImpactTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (n->f22 > 0)
		{
			if (UpdateFlags() & 0x201) return 0;
			n->f22--;
			return 0;
		}
		FX_HELD(held_note_node(us(ORIG_ImpactTask), n);)
		const int16_t next = ImpactDraw(n);
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 15)
		{
			Node *k = AddTask(QAux(), us(ORIG_ImpactSmokeTask));
			ff8fx::copy<(8)>(k->pos, n->pos);
			k->counter = 0;
			k->f1C = (int16_t)(CrtRand() % 0x800 + 0xD00);
			k->f1A = n->f1A;
			Node *f = AddTask(QAux(), us(ORIG_ImpactFlashTask));
			ff8fx::copy<(8)>(f->pos, n->pos);
			f->counter = 0;
			f->f0E = n->f0E;
			const int16_t third = (int16_t)((CrtRand() % 0xA80 + 0x1480) / 3);
			f->f1E = third;
			f->f1C = third;
			Node *d = AddTask(QAux(), us(ORIG_DebrisTask));
			d->f0E = n->f0E;
			ff8fx::copy<(8)>(d->pos, n->pos);
			d->f20 = (int16_t)PartCount();
			PartCount()++;
			d->counter = 0;
		}
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}

	static uint32_t __cdecl ImpactSmokeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		FX_HELD(held_note_node(us(ORIG_ImpactSmokeTask), n);)
		const int16_t next = ImpactSmokeDraw(n);
		FX_HELD(held_note_last(next < 0);)
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return next < 0 ? TASK_END : 0;
	}

	static uint32_t __cdecl ImpactFlashTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		FX_HELD(held_note_node(us(ORIG_ImpactFlashTask), n);)
		ImpactModelDraw(n, 0, us(MODEL_ImpactFlash), n->counter >= 4, ImpactFlashFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 4));
		n->counter++;
		return n->counter >= 12 ? TASK_END : 0;
	}

	static uint32_t __cdecl DebrisTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		Debris *arr = DebrisArray(n->f0E);
		uint8_t *s = (uint8_t *)FieldAlloc(0x68);
		const int32_t id = n->f20;
		if (!(UpdateFlags() & 0x201) && n->counter <= 2)
		{
			for (int32_t j = 0; j < 4; j++)
			{
				int32_t i = 0;
				while (i < DEBRIS_N && arr[i].id != 0) i++;
				if (i >= DEBRIS_N) break;
				Debris *e = &arr[i];
				e->id = id;
				e->counter = 0;
				e->size = (int16_t)(CrtRand() % 0x280 + 0x80);
				ff8fx::copy<(8)>(e->pos, n->pos);
				int32_t *d = (int32_t *)(s + 0x48);
				d[0] = CrtRand() % 0x1000 - 0x800;
				d[1] = CrtRand() % 0x1000 - 0x800;
				d[2] = CrtRand() % 0x1000 - 0x800;
				Normalize(d, d);
				const int32_t sp = CrtRand() % 0x190 + 0x8C;
				e->vel[0] = (int16_t)(mul32(d[0], sp) >> 12);
				e->vel[1] = (int16_t)(mul32(d[1], sp) >> 12);
				e->vel[2] = (int16_t)(mul32(d[2], sp) >> 12);
				const int16_t a = (int16_t)(CrtRand() % 0x1000);
				e->ax = a;
				e->az = (int16_t)(a >> 1);
				e->sx = (int16_t)(CrtRand() % 0x186 + 0x5A);
				if (i & 1) e->sx = (int16_t)-e->sx;
				e->sz = (int16_t)(CrtRand() % 0x140 + 0x50);
			}
		}
		FX_HELD(held_note_node(us(ORIG_DebrisTask), n);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		DebrisBegin(s, h);
		int32_t alive = 0;
		for (Debris *e = arr; e < arr + DEBRIS_N; e++)
		{
			if (e->id != id) continue;
			FX_HELD(held_note_debris(e);)
			DebrisDraw(e, s, h, e->counter >= 6, DebrisFade(e->counter));
			if (UpdateFlags() & 0x201) continue;
			e->counter++;
			if (e->counter >= 14)
			{
				e->id = 0;
				FX_HELD(held_note_debris_next(e);)
				continue;
			}
			e->pos[0] = (int16_t)(e->pos[0] + e->vel[0]);
			e->pos[1] = (int16_t)(e->pos[1] + e->vel[1]);
			e->pos[2] = (int16_t)(e->pos[2] + e->vel[2]);
			e->vel[0] = (int16_t)(e->vel[0] - (int16_t)(e->vel[0] >> 3));
			e->vel[1] = (int16_t)(e->vel[1] - (int16_t)(e->vel[1] >> 3));
			e->vel[2] = (int16_t)(e->vel[2] - (int16_t)(e->vel[2] >> 3));
			e->ax = (int16_t)(e->ax + e->sx);
			e->az = (int16_t)(e->az + e->sz);
			alive++;
		}
		FX_HELD(held_note_alive(alive);)
		FieldFree(0x58);
		FieldFree(0x68);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	static uint32_t __cdecl ImpactConeTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (n->f22 > 0)
		{
			if (UpdateFlags() & 0x201) return 0;
			n->f22--;
			return 0;
		}
		FX_HELD(held_note_node(us(ORIG_ImpactConeTask), n);)
		ImpactModelDraw(n, n->f18, us(MODEL_Cone), n->counter >= 0, ImpactConeFade(n->counter));
		if (UpdateFlags() & 0x201) return 0;
		const int16_t spin = n->f1A;
		n->f18 = (int16_t)(n->f18 + spin);
		n->f1A = (int16_t)(spin - (int16_t)(spin / 10));
		const int16_t step = n->f1E;
		n->f1C = (int16_t)(n->f1C + step);
		n->f1E = (int16_t)(step - (int16_t)(step / 5));
		n->counter++;
		return n->counter >= 10 ? TASK_END : 0;
	}

	static uint32_t __cdecl ShotTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (UpdateFlags() & 0x201) return 0;
		if (!(Flags() & 1)) return 0;
		if (n->counter == 0)
		{
			if (Flags() & 4) return 0;
			QueueChainTransformation(Caster(), n->f18);
			Flags() |= 4;
		}
		if (n->counter == 22)
		{
			Ctx()->done = 1;
			Flags() &= ~4u;
		}
		if (n->counter == 0)
		{
			ShotRec *s = Shot(n->f0E);
			int16_t pos[4];
			WeaponBone(Caster(), 2, s->dir, pos);
			s->pos[0] = (int16_t)((mul32(s->dir[0], 350) >> 12) + pos[0]);
			s->pos[1] = (int16_t)((mul32(s->dir[1], 350) >> 12) + pos[1]);
			s->pos[2] = (int16_t)((mul32(s->dir[2], 350) >> 12) + pos[2]);
			int32_t axis[3];
			const int32_t angle = RotationBetweenVectors(MUZZLE_REF(), s->dir, axis);
			BuildAxisAngleRotationMatrix(angle, &s->m, axis);
			Node *k = AddTask(QAux(), us(ORIG_SmokeTask));
			k->counter = 0;
			k->f0E = n->f0E;
			k->f1C = (int16_t)(CrtRand() % 0x280 + 0x600);
			Node *g = AddTask(QAux(), us(ORIG_GroundFlashTask));
			g->counter = 0;
			g->f0E = n->f0E;
			g->f18 = (int16_t)(CrtRand() % 0x1000);
			g->f1C = (int16_t)(CrtRand() % 0x280 + 0x800);
			Node *st = AddTask(QAux(), us(ORIG_SteamTask));
			st->pos[3] = (int16_t)PartCount();
			st->counter = 0;
			st->f0E = n->f0E;
			PartCount()++;
			Node *bs = AddTask(QAux(), us(ORIG_BoneSmokeTask));
			bs->f0E = n->f0E;
			bs->pos[3] = (int16_t)PartCount();
			bs->counter = 0;
			PartCount()++;
			n->f1C = (int16_t)(CrtRand() % 0x280 + 0x780);
		}
		if (n->counter == 1)
		{
			Node *r = AddTask(QAux(), us(ORIG_MuzzleRingTask));
			r->counter = 0;
			r->f0E = n->f0E;
			r->f18 = (int16_t)(CrtRand() % 0x1000);
			r->f1A = (int16_t)(CrtRand() % 0x96 + 0x6E);
			if (r->f18 & 1) r->f1A = (int16_t)-r->f1A;
			r->f20 = 0;
			const int16_t quarter = (int16_t)((CrtRand() % 0x280 + 0x300) / 4);
			r->f1E = quarter;
			r->f1C = quarter;
			Node *r2 = AddTask(QAux(), us(ORIG_MuzzleRingTask));
			r2->counter = 0;
			r2->f0E = n->f0E;
			r2->f18 = (int16_t)(CrtRand() % 0x1000);
			r2->f1A = (int16_t)(CrtRand() % 0x96 + 0x6E);
			r2->f20 = 0x800;
			const int16_t quarter2 = (int16_t)((CrtRand() % 0x280 + 0x300) / 4);
			r2->f1E = quarter2;
			r2->f1C = quarter2;
		}
		if (n->counter >= 0 && n->counter <= 4 && !(n->counter & 1))
		{
			Node *c = AddTask(QAux(), us(ORIG_MuzzleConeTask));
			c->counter = 0;
			c->f0E = n->f0E;
			c->f18 = (int16_t)(CrtRand() % 0x1000);
			c->f1A = (int16_t)(CrtRand() % 0x96 + 0x6E);
			const int16_t third = (int16_t)(n->f1C / 3);
			c->f1E = third;
			c->f1C = third;
		}
		if (n->counter == 2)
		{
			*(volatile int32_t *)&n_ = 0;
			const int32_t anchors = ***(const uint8_t ***)(Entity(n->f1A) + 0x64);
			for (int32_t j = 0; j < 4; j++)
			{
				const int32_t bone = anchors ? CrtRand() % anchors : 0;
				Node *im = AddTask(QAux(), us(ORIG_ImpactTask));
				im->counter = 0;
				im->f0E = n->f0E;
				GetEffectSpawnPosition(Entity(n->f1A), bone, CrtRand() % 0x1000, im->pos);
				im->pos[3] = (int16_t)(mul32(*(const int16_t *)(Entity(n->f1A) + 0x26), 2000) >> 12);
				im->f1A = (int16_t)*(volatile int32_t *)&n_;
				im->f1C = (int16_t)(CrtRand() % 0x280 + 0x600);
				im->f22 = 0;
				Node *c = AddTask(QAux(), us(ORIG_ImpactConeTask));
				ff8fx::copy<(8)>(c->pos, im->pos);
				c->counter = 0;
				c->f0E = n->f0E;
				c->f18 = (int16_t)(CrtRand() % 0x1000);
				c->f1A = (int16_t)(CrtRand() % 0xBE + 0xB9);
				if (c->f18 & 1) c->f1A = (int16_t)-c->f1A;
				const int16_t fifth = (int16_t)(im->f1C / 5);
				c->f1E = fifth;
				c->f1C = fifth;
				c->f22 = im->f22;
				*(volatile int32_t *)&n_ = j + 1;
			}
		}
		if (n->counter == 0x12) ApplyActionResultToTarget(Ctx()->actions->targets);
		if (n->counter == 1) BdPlaySE(SOUND_Gun(), (n->f0E & 1) ? 1 : 2, 0x80);
		n->counter++;
		return n->counter >= 25 ? TASK_END : 0;
	}

	static uint32_t __cdecl FadeOutTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 - n->f18 / n->f0E * n->counter, 0);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		if (n->counter < n->f0E) return 0;
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	static uint32_t __cdecl FadeInTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		SetScreenFlash(n->f18 / n->f0E * n->counter, 0);
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= n->f0E ? TASK_END : 0;
	}

	static uint32_t __cdecl AfterimageTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		const int32_t c = n->counter;
		Ghost *g = GhostSlot(c % 5);
		g->used = 1;
		uint8_t *e = Caster();
		BoneSnapshot(*(uint8_t **)(e + 0x78), g->bones);
		BoneSnapshot(*(uint8_t **)(Caster() + 0x78), (uint8_t *)us(BONES_NOW));
		const uint32_t colour = *(const uint32_t *)(Caster() + 0x28);
		int32_t slot = c - 1 >= 0 ? (c - 1) % 5 : 0;
		const int32_t end = EndTick();
		const int32_t count = c >= end ? end - c + 5 : 5;
		*(volatile int32_t *)&n_ = count;
		FX_HELD(held_note_ghosts(c, slot, count, colour);)
		for (int32_t k = 0; k < count; k++)
		{
			const Ghost *s = GhostSlot(slot);
			if (!s->used) break;
			BoneRestore(*(uint8_t **)(Caster() + 0x78), s->bones);
			*(uint32_t *)(Caster() + 0x28) = GHOST_COLOURS()[k];
			GhostDraw(Caster());
			if (--slot < 0) slot = 4;
		}
		BoneRestore(*(uint8_t **)(Caster() + 0x78), (const uint8_t *)us(BONES_NOW));
		*(uint32_t *)(Caster() + 0x28) = colour;
		if (UpdateFlags() & 0x201) return 0;
		n->counter++;
		return n->counter >= EndTick() + 5 ? TASK_END : 0;
	}

	static uint32_t __cdecl StartTask(TaskNode *n_)
	{
		Node *n = (Node *)n_;
		if (UpdateFlags() & 0x201) return 0;
		if (n->counter == 0)
		{
			BindExternalAnimSet(Caster(), us(BIND_OUT), us(ANIM_DATA));
			QueueChainTransformation(Caster(), 1);
			Node *f = AddTask(QAux(), us(ORIG_FadeInTask));
			f->counter = 0;
			f->f0E = 8;
			f->f18 = 0x800;
			AddTask(QAux(), us(ORIG_AfterimageTask))->counter = 0;
			EndTick() = 999999;
			for (uint32_t a = us(GHOSTS); a < us(GHOSTS_END); a += 0x84) *(int32_t *)a = 0;
		}
		n->counter++;
		if (n->counter < 7) return 0;
		Flags() |= 1;
		Ctx()->done = 1;
		return TASK_END;
	}

	static uint32_t __cdecl EndTask(TaskNode *n_)
	{
		if (UpdateFlags() & 0x201) return 0;
		if (Flags() & 4) return 0;
		Node *n = (Node *)n_;
		if (n->counter == 0)
		{
			QueueChainTransformation(Caster(), 2);
			Node *f = AddTask(QAux(), us(ORIG_FadeOutTask));
			f->counter = 0;
			f->f0E = 8;
			f->f18 = 0x800;
			EndTick() = ModuleTick();
		}
		if (n->counter == 2) ApplyActionResultToTargets(Ctx()->actions[0].targets, Ctx()->actions[0].target_count);
		n->counter++;
		if (n->counter < 0xE) return 0;
		Flags() |= 2;
		Ctx()->done = 1;
		return TASK_END;
	}

	static uint32_t __cdecl RootTask(TaskNode *n_)
	{
		RootNode *r = (RootNode *)n_;
		FX_HELD(held_note_root();)
		PacketCursor() = (*(const uint8_t *)&r->counter & 1) ? TexBase() + ARENA_ODD : TexBase();
		const int shots = ExecuteTaskQueue(QShots());
		const int aux = ExecuteTaskQueue(QAux());
		if (!(UpdateFlags() & 0x201)) ModuleTick()++;
		r->counter++;
		return (aux == 0 && shots == 0 && (Flags() & 2)) ? TASK_END : 0;
	}
}

	void register_mag195_canister_shot()
	{
		register_port(canister195::ORIG_RootTask, (void *)canister195::RootTask, "195 RootTask", 195);
		register_port(canister195::ORIG_StartTask, (void *)canister195::StartTask, "195 StartTask", 195);
		register_port(canister195::ORIG_ShotTask, (void *)canister195::ShotTask, "195 ShotTask", 195);
		register_port(canister195::ORIG_EndTask, (void *)canister195::EndTask, "195 EndTask", 195);
		register_port(canister195::ORIG_FadeOutTask, (void *)canister195::FadeOutTask, "195 FadeOutTask", 195);
		register_port(canister195::ORIG_FadeInTask, (void *)canister195::FadeInTask, "195 FadeInTask", 195);
		register_port(canister195::ORIG_AfterimageTask, (void *)canister195::AfterimageTask, "195 AfterimageTask", 195);
		register_port(canister195::ORIG_SmokeTask, (void *)canister195::SmokeTask, "195 SmokeTask", 195);
		register_port(canister195::ORIG_GroundFlashTask, (void *)canister195::GroundFlashTask, "195 GroundFlashTask", 195);
		register_port(canister195::ORIG_MuzzleConeTask, (void *)canister195::MuzzleConeTask, "195 MuzzleConeTask", 195);
		register_port(canister195::ORIG_MuzzleRingTask, (void *)canister195::MuzzleRingTask, "195 MuzzleRingTask", 195);
		register_port(canister195::ORIG_SteamTask, (void *)canister195::SteamTask, "195 SteamTask", 195);
		register_port(canister195::ORIG_BoneSmokeTask, (void *)canister195::BoneSmokeTask, "195 BoneSmokeTask", 195);
		register_port(canister195::ORIG_ImpactTask, (void *)canister195::ImpactTask, "195 ImpactTask", 195);
		register_port(canister195::ORIG_ImpactSmokeTask, (void *)canister195::ImpactSmokeTask, "195 ImpactSmokeTask", 195);
		register_port(canister195::ORIG_ImpactFlashTask, (void *)canister195::ImpactFlashTask, "195 ImpactFlashTask", 195);
		register_port(canister195::ORIG_DebrisTask, (void *)canister195::DebrisTask, "195 DebrisTask", 195);
		register_port(canister195::ORIG_ImpactConeTask, (void *)canister195::ImpactConeTask, "195 ImpactConeTask", 195);
		FX_HELD(register_mag195_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag195_canister_shot_held.inc"
#endif
