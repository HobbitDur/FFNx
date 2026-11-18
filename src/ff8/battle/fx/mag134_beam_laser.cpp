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

// Effect 134: Beam Laser

#include "mag_common.h"

namespace ff8fx
{
namespace beam134
{
	using namespace eng;
	using namespace magc;

	inline uint8_t *&Caster() { return var<uint8_t *>(us(0x2520E68)); }
	inline uint32_t &TexBase() { return var<uint32_t>(us(0x2520E70)); }
	inline uint32_t &CasterFlags() { return var<uint32_t>(us(0x2520E74)); }
	inline CastContext *&Ctx() { return var<CastContext *>(us(0x2520E78)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(us(0x2520E94)); }
	inline TaskQueue *QHits() { return (TaskQueue *)us(0x2520E98); }
	inline TaskQueue *QSparks() { return (TaskQueue *)us(0x2520EA8); }
	inline TaskQueue *QGlows() { return (TaskQueue *)us(0x2520EB8); }
	inline TaskQueue *QBeam() { return (TaskQueue *)us(0x2520EC8); }

	static const uint32_t ORIG_MasterTask = 0x6B0130;
	static const uint32_t ORIG_BeamTask = 0x6B03B0;
	static const uint32_t ORIG_HitTask = 0x6B0D30;
	static const uint32_t ORIG_SparkTask = 0x6B0D90;
	static const uint32_t ORIG_GlowTask = 0x6B0E30;
	static const uint32_t MORPH = 0xC60;
	static const uint32_t ARENA_A = 0xDE8;
	static const uint32_t ARENA_B = 0x10DE8;
	static const uint32_t POOL_Beam = 0x720;
	static const uint32_t MODEL_Charge = 0x12038DC;
	static const uint32_t MODEL_Muzzle = 0x1205270;
	static const uint32_t MODEL_Ray = 0x12065F0;
	static const uint32_t MODEL_Impact = 0x1204004;
	static const uint32_t MODEL_Hit = 0x1207150;
	static const uint32_t PRIM_Beam = 0x1202214;
	static const uint32_t SEQ_Spark = 0x1202068;
	static const uint32_t SEQ_Glow = 0x1201EBC;
	static const void *SOUND_Beam() { return (const void *)us(0x120767C); }

	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(us(0x56D100))(angle); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(us(0x502170))(entity, bone, flag, out); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(us(0x7016B0))(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(us(0x701310))(angles, out); }
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(us(0x701390))(model, f0, f1, t, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(us(0x56C270))(a, b); }
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(us(0x56C220))(a, b); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(us(0x460850))(); }
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(us(0x7043B0))(h, ot, mode, cursor); }
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(us(0x50CBA0))(v, m); }

#pragma pack(push, 1)
	struct MasterNode
	{
		TaskNode hdr;
		int16_t counter;
		uint8_t index;
		uint8_t started;
		uint32_t arena;
	};
	struct BeamNode
	{
		TaskNode hdr;
		int16_t counter;
		int16_t index;
		uint8_t *ent;
		int16_t spawn[4];
		uint8_t charge[2][0x58];
		uint8_t muzzle[0x220];
		uint8_t ray[0xA4];
		uint8_t impact[0x1B0];
	};
	struct HitNode
	{
		TaskNode hdr;
		Mat4x3 frame;
		uint8_t layout[0x58];
	};
	struct SparkNode
	{
		TaskNode hdr;
		int16_t pos[3];
		int16_t counter;
		int16_t vel[3];
		int16_t pad1A;
	};
	struct GlowNode
	{
		TaskNode hdr;
		int16_t pos[3];
		int16_t counter;
	};
	struct PrimArg
	{
		Mat4x3 m;
		int32_t depth;
		uint32_t morph;
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(BeamNode) == 0x540 && sizeof(HitNode) == 0x84, "Beam Laser nodes");
	static_assert(sizeof(SparkNode) == 0x1C && sizeof(GlowNode) == 0x14, "Beam Laser nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Beam Laser prim block");

	static void __cdecl PrimCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = AllocHeader(0x58);
		const int32_t object = (int16_t)r->flags_lo;
		uint8_t *model = l->data + *(const int32_t *)(l->data + object * 4 + 8);
		*(uint8_t **)h = model;
		const int16_t f0 = r->b0, f1 = r->b1;
		const int32_t nverts = *(const int32_t *)(model + 4);
		if (f0 == f1)
		{
			if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
			else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
		}
		else
		{
			const int16_t t = r->b;
			if (t == 0)
			{
				if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f0) * 8 + 0xC;
			}
			else if (t == 0x1000)
			{
				if (f1 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, f1) * 8 + 0xC;
			}
			else
			{
				BlendVertexFrames((uint32_t)model, f0, f1, t, arg->morph);
				*(uint32_t *)(h + 4) = arg->morph;
			}
		}
		Mat4x3 m = {};
		RotationFromAngles(r->rot, &m);
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		int32_t t[3];
		if (r->flags & 0x200)
		{
			t[0] = off[0];
			t[1] = off[1];
			t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
			t[0] = m.t[0];
			t[1] = m.t[1];
			t[2] = m.t[2];
		}
		m.t[0] = (int32_t)((uint32_t)t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				int16_t s[9] = { r->scale[0], 0, 0, 0, r->scale[1], 0, 0, 0, r->scale[2] };
				MulMatrix3(&m, s);
			}
			else
			{
				const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
				Scale3DMatrix(&m, v);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = 0x2030;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) = 0x20F0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		*(int32_t *)(h + 0x18) = arg->depth;
		PacketCursor() = RenderPrim2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void PointFrame(const Mat4x3 *M, const int16_t *P, int16_t *v, PrimArg *arg)
	{
		GteSetRotMatrixCtrl(M);
		GteLoadV0(v);
		GteMVMVA_RotV0();
		GteStoreIR123(v);
		v[0] = (int16_t)(v[0] + P[0]);
		v[1] = (int16_t)(v[1] + P[1]);
		v[2] = (int16_t)(v[2] + P[2]);
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(v);
		GteMVMVA_RotV0();
		int32_t mac[3];
		GteReadMAC123(mac);
		ff8fx::copy<(sizeof(arg->m.m))>(&arg->m.m, &Camera().m);
		arg->m.t[0] = (int32_t)((uint32_t)mac[0] + (uint32_t)Camera().t[0]);
		arg->m.t[1] = (int32_t)((uint32_t)mac[1] + (uint32_t)Camera().t[1]);
		arg->m.t[2] = (int32_t)((uint32_t)mac[2] + (uint32_t)Camera().t[2]);
		arg->depth = 0;
		arg->morph = TexBase() + MORPH;
	}

	struct BeamFrame
	{
		int16_t P[4];
		int16_t T[4];
		Mat4x3 M;
		Mat4x3 R;
	};
	static void BuildFrame(const BeamNode *b, BeamFrame *f)
	{
		GetEffectSpawnPosition(Caster(), 0xD, 0x800, f->P);
		GetDefaultEffectPosition(b->ent, f->T);
		const int32_t v[3] = { (int32_t)f->T[0] - f->P[0], 0, (int32_t)f->P[2] - f->T[2] };
		BuildOrthonormalBasis(v, &f->M);
		for (int k = 0; k < 3; k++)
		{
			f->R.m[0][k] = (int16_t)(0 - f->M.m[0][k]);
			f->R.m[1][k] = f->M.m[1][k];
			f->R.m[2][k] = (int16_t)(0 - f->M.m[2][k]);
		}
	}

	static int32_t ChargeAngle(int32_t counter) { return (int32_t)((uint32_t)(counter << 11) / 14u); }
	static void ChargeOffsets(int32_t a, int16_t *s, int16_t *c)
	{
		*s = (int16_t)((ComputeSin(a) * 500) >> 12);
		*c = (int16_t)((ComputeCos(a) * 500) >> 12);
	}

	struct Ease
	{
		int16_t lift;
		int32_t width;
		int32_t fade;
	};
	static Ease BeamEase(int32_t counter, int16_t size)
	{
		Ease r;
		const uint32_t e = (uint32_t)(counter - 14);
		if (e < 0xC)
		{
			r.width = 0x1000 - ComputeCos((int32_t)((e << 10) / 12u));
			r.lift = (int16_t)((int32_t)size * 4 - (mul32(size, r.width) >> 9));
			r.fade = 0;
		}
		else if (e < 0x14)
		{
			r.fade = (int32_t)((e - 0xC) << 9);
			r.lift = (int16_t)((int32_t)size * 2 - (shl32(mul32((int32_t)e + 0x34, size), 2) >> 6));
			r.width = 0x280 - (int32_t)(e << 5);
		}
		else
		{
			r.lift = (int16_t)(size * 2);
			r.fade = 0;
			r.width = 0;
		}
		return r;
	}

	static void FloorHit(const int16_t *P, int16_t *T, int16_t *hit)
	{
		const int32_t dy = (int32_t)T[1] - P[1];
		const int32_t ax = mul32((int32_t)P[0] - T[0], P[1]);
		const int32_t az = mul32((int32_t)P[2] - T[2], P[1]);
		const int32_t dh = dy - 0x200;
		hit[0] = (int16_t)(ax / dh + P[0]);
		hit[1] = 0;
		hit[2] = (int16_t)(az / dh + P[2]);
		T[0] = (int16_t)(ax / dy + P[0]);
		T[2] = (int16_t)(az / dy + P[2]);
		T[1] = 0;
	}

	struct BeamDraw
	{
		Mat4x3 M2;
		int32_t len;
		int kind;
	};
	static void BeamShape(const BeamNode *b, const int16_t *P, const int16_t *T, BeamDraw *d)
	{
		const int32_t v[3] = { (int32_t)T[0] - P[0], (int32_t)P[1] - T[1], (int32_t)P[2] - T[2] };
		int32_t len = BuildOrthonormalBasis(v, &d->M2);
		len += len >> 3;
		d->kind = 0;
		if (T[1] < *(const int16_t *)(b->ent + 0x92)) len <<= 1;
		else if (T[1] >= 0) d->kind = 1;
		else
		{
			const int32_t h = *(const int16_t *)(b->ent + 0x26);
			const int32_t up = -(int32_t)T[1];
			if (up >= (h >> 1) && up <= h) d->kind = 2;
		}
		d->len = len;
	}
	static int16_t BeamWidth(int32_t angle) { return (int16_t)(ComputeSin(angle >> 1) >> 3); }
	static void BeamRender(Mat4x3 *M2, int32_t len, int16_t w, int32_t fade)
	{
		Mat4x3 s;
		Memset32(&s, 0, 8);
		s.m[0][0] = w;
		s.m[1][1] = w;
		s.m[2][2] = (int16_t)len;
		MulMatrix3(M2, &s);
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = us(PRIM_Beam);
		if (fade == 0) *(uint32_t *)(h + 0x1C) = 0x30;
		else
		{
			h[0xA] = 0;
			h[9] = 0;
			h[8] = 0;
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) = 0xF0;
		}
		ComposeAffineTransform(&Camera(), M2, M2);
		GteSetRotMatrix(M2);
		GteSetTransVector(M2);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void SpriteDraw(const int16_t *pos, int16_t frame, uint32_t seq, int32_t a, int32_t b)
	{
		uint8_t *h = AllocHeader(0xB4);
		TransformCameraByShadowRotation(pos, a, b);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
}
}

#ifdef FF8_FX_HELD
#include "mag134_beam_laser_held.h"
#endif

namespace ff8fx
{
namespace beam134
{
	static uint32_t __cdecl GlowTask(TaskNode *n)
	{
		GlowNode *g = (GlowNode *)n;
		FX_HELD(held_note_sprite(SPR_GLOW, n, g->pos, g->counter);)
		SpriteDraw(g->pos, g->counter, us(SEQ_Glow), 0x1000, -0x400);
		g->counter++;
		return g->counter < 0x10 ? 0 : TASK_END;
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		FX_HELD(held_note_sprite(SPR_SPARK, n, s->pos, s->counter);)
		SpriteDraw(s->pos, s->counter, us(SEQ_Spark), 0xC00, -0x300);
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->counter++;
		return s->counter < 0x10 ? 0 : TASK_END;
	}

	static uint32_t __cdecl HitTask(TaskNode *n)
	{
		HitNode *h = (HitNode *)n;
		PrimArg arg;
		ComposeAffineTransform(&Camera(), &h->frame, &arg.m);
		arg.depth = 0;
		arg.morph = TexBase() + MORPH;
		FX_HELD(held_note_play(PLAY_HIT, n, (prim::Layout *)h->layout, nullptr);)
		return prim::play((prim::Layout *)h->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	static uint32_t __cdecl BeamTask(TaskNode *n)
	{
		BeamNode *b = (BeamNode *)n;
		BeamFrame f;
		BuildFrame(b, &f);
		const int32_t c = b->counter;
		int16_t v[4] = {};
		PrimArg arg;
		if ((uint32_t)c < 0xE)
		{
			int16_t s, co;
			ChargeOffsets(ChargeAngle(c), &s, &co);
			for (int k = 0; k < 2; k++)
			{
				v[0] = 0;
				v[1] = k ? (int16_t)(0 - s) : s;
				v[2] = co;
				PointFrame(&f.M, f.P, v, &arg);
				FX_HELD(held_note_play(PLAY_CHARGE0 + k, n, (prim::Layout *)b->charge[k], &f);)
				prim::play((prim::Layout *)b->charge[k], PrimCallback, (int)&arg, 0);
			}
		}
		if ((uint32_t)(c - 9) < 0x19)
		{
			v[0] = 0;
			v[1] = 0;
			v[2] = -500;
			PointFrame(&f.M, f.P, v, &arg);
			FX_HELD(held_note_play(PLAY_MUZZLE, n, (prim::Layout *)b->muzzle, &f);)
			prim::play((prim::Layout *)b->muzzle, PrimCallback, (int)&arg, 0);
			const Ease ez = BeamEase(c, *(const int16_t *)(b->ent + 0x26));
			int16_t T[4] = { f.T[0], (int16_t)(f.T[1] + ez.lift), f.T[2], f.T[3] };
			BeamDraw d;
			{
				const int32_t u[3] = { (int32_t)T[0] - f.P[0], (int32_t)f.P[1] - T[1], (int32_t)f.P[2] - T[2] };
				BuildOrthonormalBasis(u, &d.M2);
			}
			d.M2.t[0] = v[0];
			d.M2.t[1] = v[1];
			d.M2.t[2] = v[2];
			PrimArg ray;
			ComposeAffineTransform(&Camera(), &d.M2, &ray.m);
			ray.depth = 0;
			ray.morph = TexBase() + MORPH;
			FX_HELD(held_note_play(PLAY_RAY, n, (prim::Layout *)b->ray, &f);)
			prim::play((prim::Layout *)b->ray, PrimCallback, (int)&ray, 0);
			if ((uint32_t)(c - 14) < 0x14)
			{
				int16_t hit[3] = {};
				if (T[1] >= 0) FloorHit(f.P, T, hit);
				BeamShape(b, f.P, T, &d);
				if (d.kind == 1)
				{
					HitNode *h = (HitNode *)AddTaskToQueue(QHits(), us(ORIG_HitTask));
					if (h)
					{
						ff8fx::copy<(sizeof(h->frame.m))>(&h->frame.m, &f.R.m);
						h->frame.t[0] = hit[0];
						h->frame.t[1] = hit[1];
						h->frame.t[2] = hit[2];
						DecodeModelPrimLayout(us(MODEL_Hit), h->layout, 0x58);
					}
				}
				else if (d.kind == 2)
				{
					GlowNode *g = (GlowNode *)AddTaskToQueue(QGlows(), us(ORIG_GlowTask));
					if (g)
					{
						g->pos[0] = T[0];
						g->pos[1] = T[1];
						g->pos[2] = T[2];
						g->counter = 0;
					}
				}
				FX_HELD(held_note_beam(n);)
				BeamRender(&d.M2, d.len, BeamWidth(ez.width), ez.fade);
			}
		}
		if ((uint32_t)(c - 0x16) < 0x18)
		{
			if ((uint32_t)(c - 0x16) < 4)
			{
				for (int k = 0; k < 4; k++)
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), us(ORIG_SparkTask));
					if (!s) continue;
					ff8fx::copy<(4)>(s->pos, b->spawn);
					*(uint32_t *)&s->pos[2] = *(const uint32_t *)&b->spawn[2];
					s->counter = 0;
					s->vel[0] = (int16_t)((CrtRand() & 0xFF) - 0x80);
					s->vel[1] = (int16_t)((CrtRand() & 0xFF) - 0x80);
					s->vel[2] = (int16_t)((CrtRand() & 0xFF) - 0x80);
				}
			}
			f.R.t[0] = b->spawn[0];
			f.R.t[1] = b->spawn[1];
			f.R.t[2] = b->spawn[2];
			ComposeAffineTransform(&Camera(), &f.R, &arg.m);
			arg.depth = -0x80;
			arg.morph = TexBase() + MORPH;
			FX_HELD(held_note_play(PLAY_IMPACT, n, (prim::Layout *)b->impact, &f);)
			prim::play((prim::Layout *)b->impact, PrimCallback, (int)&arg, 0);
		}
		if (b->counter == 2) BdPlaySE(SOUND_Beam(), 0, 0x80);
		if (b->counter == 0x1A) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		if (b->counter >= 0x2E) return TASK_END;
		b->counter++;
		return 0;
	}

	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		FX_HELD(held_note_master();)
		if (m->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			m->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			m->arena = 1;
		}
		*Caster() |= 8;
		if (m->counter == 1)
		{
			if (!m->started)
			{
				m->started = 1;
				InitTaskQueuePool(QBeam(), (void *)(TexBase() + POOL_Beam), 0x540, 1);
				InitTaskQueuePool(QGlows(), (void *)(TexBase() + 0x5E0), 0x14, 0x10);
				InitTaskQueuePool(QSparks(), (void *)(TexBase() + 0x420), 0x1C, 0x10);
				InitTaskQueuePool(QHits(), (void *)TexBase(), 0x84, 8);
			}
			if (m->index <= Ctx()->actions[0].last_action)
			{
				uint8_t *ent = Entity(Ctx()->actions[m->index].targets[0]);
				const BeamNode *live = (const BeamNode *)(TexBase() + POOL_Beam);
				if ((live->hdr.flags & 1) && live->ent == ent) m->counter = 0;
				else
				{
					BeamNode *b = (BeamNode *)AddTaskToQueue(QBeam(), us(ORIG_BeamTask));
					if (b)
					{
						Memset32(&b->counter, 0, 0x14D);
						b->index = m->index;
						b->ent = ent;
						GetEffectSpawnPosition(ent, 0xF1, 0, b->spawn);
						m->index++;
						DecodeModelPrimLayout(us(MODEL_Charge), b->charge[0], 0x58);
						DecodeModelPrimLayout(us(MODEL_Charge), b->charge[1], 0x58);
						DecodeModelPrimLayout(us(MODEL_Muzzle), b->muzzle, 0x220);
						DecodeModelPrimLayout(us(MODEL_Ray), b->ray, 0xA4);
						DecodeModelPrimLayout(us(MODEL_Impact), b->impact, 0x1B0);
					}
				}
			}
		}
		int a0, a1, a2, a3;
		if (m->started)
		{
			a0 = ExecuteTaskQueue(QBeam());
			a1 = ExecuteTaskQueue(QGlows());
			a2 = ExecuteTaskQueue(QSparks());
			a3 = ExecuteTaskQueue(QHits());
		}
		else a0 = a1 = a2 = a3 = (int)n;
		if (m->started && a0 == 0 && a1 == 0 && a2 == 0 && a3 == 0)
		{
			uint16_t &w = *(uint16_t *)Caster();
			w = (uint16_t)(w ^ ((w ^ CasterFlags()) & 8));
			return TASK_END;
		}
		m->counter++;
		return 0;
	}
}

	void register_mag134_beam_laser()
	{
		register_port(beam134::ORIG_MasterTask, (void *)beam134::MasterTask, "134 MasterTask", 134);
		register_port(beam134::ORIG_BeamTask, (void *)beam134::BeamTask, "134 BeamTask", 134);
		register_port(beam134::ORIG_HitTask, (void *)beam134::HitTask, "134 HitTask", 134);
		register_port(beam134::ORIG_SparkTask, (void *)beam134::SparkTask, "134 SparkTask", 134);
		register_port(beam134::ORIG_GlowTask, (void *)beam134::GlowTask, "134 GlowTask", 134);
		FX_HELD(register_mag134_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag134_beam_laser_held.inc"
#endif
