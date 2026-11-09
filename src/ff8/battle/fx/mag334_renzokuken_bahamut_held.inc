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

// 30 fps layer of Renzokuken vs Bahamut (effect 334), included at the end of mag334_renzokuken_bahamut.cpp: the common part's
// layer (renzo_boss_held.inc: trail, slashes, particles, the bone emitter's particles, the node models AngleModel /
// CasterModel) and its own task:
//   RingPool - its draw replayed from the SLOTS records it drew (alive & 1, a snapshot at its draw) through the held frame's
//     camera; each ring's scale half way to its post-update value and its fade half way from life 4 on (mode switch
//     before: held) when it was updated (life + 1, still alive & 1); births on the real tick; a ring freed on the tick
//     holds its last picture.

namespace ff8fx
{
namespace renzo334
{
#define RENZO_HAS_NODEMODELS
#include "renzo_boss_held.inc"
#undef RENZO_HAS_NODEMODELS

	enum { REC_RINGPOOL = REC_MODULE };
	static uint8_t g_pool[SLOTS_END - SLOTS]; // SLOTS as the ring pool drew it

	static void held_note_ringpool(const Node24 *n)
	{
		if (NoteRec(REC_RINGPOOL, n, true)) memcpy(g_pool, (const void *)SLOTS, sizeof(g_pool));
	}

	static void RingPoolHeld(const Rec &r, int num, int den)
	{
		const bool move = num != 0 && !g_tick_paused;
		Replay(r, true);
		PoolWork *w = (PoolWork *)FieldAlloc(0x68);
		uint8_t *h = AllocHeader(0x58);
		memcpy(&w->cam, &Camera(), sizeof(Mat4x3));
		*(uint32_t *)h = MODEL_POOL_RING;
		*(uint32_t *)(h + 8) = 0;
		w->v[0] = 0;
		w->v[2] = 0x300;
		for (uint32_t o = 0; o < sizeof(g_pool); o += 0x20)
		{
			const SlotRec *p = (const SlotRec *)(g_pool + o);
			if (!(p->alive & 1)) continue;
			const SlotRec *now = (const SlotRec *)(SLOTS + o);
			int32_t scale = p->w12;
			int32_t fade = PoolRingFade(p->life);
			if (move && (now->alive & 1) && now->life == p->life + 1)
			{
				scale = Lerp16(p->w12, now->w12, num, den);
				if (p->life >= 4) fade = lerp_i(fade, PoolRingFade(now->life), num, den);
			}
			PoolRingDraw(w, h, p, scale, fade);
		}
		FieldFree(0x58);
		FieldFree(0x68);
	}

	static void ModuleHeld(const Rec &r, int num, int den)
	{
		if (r.kind == REC_RINGPOOL) RingPoolHeld(r, num, den);
	}
}

	static void register_mag334_held()
	{
		renzo334::MarkHeldCommon();
		mark_held(renzo334::ORIG_RingPool);
		mark_held(renzo334::ORIG_AngleModel);
		mark_held(renzo334::ORIG_BoneEmitter);
		mark_held(renzo334::ORIG_CasterModel);
		register_module_held(334, renzo334::HeldReady, renzo334::HeldFrame);
	}
}
