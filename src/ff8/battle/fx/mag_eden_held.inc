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

// 30 fps part of mag_eden.cpp: held frames of the clone and its held-frame notes (included at the end
// of mag_eden.cpp)
//
// Held-frame behaviour of the Eden handlers (the engine draws the held frames), per handler
// below; the others:
//   Draw 57 MeshRotScaledOrder3, Draw 61 MeshRadialLightWave, Draw 62 MeshRadialDarken, Draw 63
//     MorphWireframeLines: idempotent (no boneSkipFlag gate in vanilla, nothing advanced, no
//     external side effect; they read the in-between pose and write only ws/GTE/arena scratch
//     and packets at the held cursor) -> run unchanged on held frames.
//   Draw 58 RequestScreenFeedback, Draw 64 PlaceModelLerpBulge, Draw 65
//     BattleModelSyncTransform: nothing on held frames (external side effect / E is a battle
//     entity outside the saved set; it keeps the real tick's transform).
//   Prim 6: pure draw code.
// Prediction: VM 0x0B6 / 0x0B8 blit queue (xl_QueueBlitCommand) and VM 0x0B7
// (xl_SdStreamingVolumeTranslation) are skipped; the blit ring 0x27978E8 / index 0x2798BF4 are in
// the engine block (restored after a prediction).

#include <vector>

namespace ff8fx
{
namespace gfc
{
namespace part_eden_a
{
	// ------------------------------------------------------------------------------------
	// 0xAE5AD0 Draw 59 flat grid, held frame: steps 1 and 2 are skipped (the counters are the
	// ones the real tick drew; the arena block is not modified), active counters below 16 are
	// drawn at F + num/den. Draw 59 rolled mode is a pure function of the in-between pose.
	// ------------------------------------------------------------------------------------
	// colour of a fading line at the exact in-between of the real tick's counter F and the next
	// tick's min(F + 1, 16): the 0xB66A20 computation with IR0 = 16 * t * (F + num/den). Only
	// used on held frames (never on a real tick).
	static uint32_t HeldColourScale(uint32_t col, int32_t ir0)
	{
		x::GteWriteData(ir0, 8);
		x::GteWriteData((int32_t)(col & 0xFF), 9);
		x::GteWriteData((int32_t)((col >> 8) & 0xFF), 10);
		x::GteWriteData((int32_t)((col >> 16) & 0xFF), 11);
		eng::GteGPF();
		return (MEM<uint32_t>(0x1CA8A34) & 0xFF) | ((uint32_t)MEM<uint8_t>(0x1CA8A38) << 8)
			| ((uint32_t)MEM<uint8_t>(0x1CA8A3C) << 16) | (col & 0xFF000000u);
	}

	// FadeColour: frac (the factor really is t * F of a fade counter): on a held frame an active
	// counter below 16 is drawn at F + num/den (returns true, colour in *out)
	static bool held_fade_colour(uint32_t col, int32_t t, int32_t fv, bool frac, uint32_t *out)
	{
		if (!(frac && g_held.active && g_held.den > 0 && fv >= 0 && fv < 0x10)) return false;
		*out = HeldColourScale(col, (int32_t)((int64_t)16 * t * ((int64_t)fv * g_held.den + g_held.num) / g_held.den));
		return true;
	}

	// ------------------------------------------------------------------------------------
	// 0xAE6CA0 (Draw 60 PlaceModelOnCurlPath), held: runs normally (the bone outPos/accumPos it
	// writes live in the bone array, saved by HeldFrame), but the battle-model instance E
	// (outside the snapshot) is put back to its real-tick values before returning.
	// ------------------------------------------------------------------------------------
	static uint8_t *g_e60 = nullptr;
	static uint8_t g_sv1 = 0, g_sv94[6] = {}, g_svC[2] = {}, g_sv1C[6] = {};

	static void held_entity60_save(uint8_t *E)
	{
		g_e60 = nullptr;
		if (!g_held.active) return;
		g_e60 = E;
		g_sv1 = U8(E, 1);
		memcpy(g_sv94, E + 0x94, 6);
		memcpy(g_svC, E + 0xC, 2);
		memcpy(g_sv1C, E + 0x1C, 6);
	}

	static void held_entity60_restore()
	{
		uint8_t *E = g_e60;
		if (!E) return;
		U8(E, 1) = g_sv1;
		memcpy(E + 0x94, g_sv94, 6);
		memcpy(E + 0xC, g_svC, 2);
		memcpy(E + 0x1C, g_sv1C, 6);
		g_e60 = nullptr;
	}
}

namespace part_eden_c
{
	// held frames: a range of the unallocated scratch above the arena top, saved and restored
	struct ScratchSave
	{
		uint8_t *p = nullptr;
		std::vector<uint8_t> b;
		void save(uint8_t *q, uint32_t n) { p = q; b.assign(q, q + n); }
		void restore() { if (p && !b.empty()) memcpy(p, b.data(), b.size()); p = nullptr; }
	};
	static ScratchSave g_ss;

	static void held_scratch_restore() { g_ss.restore(); }

	// ------------------------------------------------------------------------------------
	// 0xAED040 (Draw 53 PixelParticleEmitter), held frames: no allocation, no spawn (no rand),
	// the pool is not written; every particle alive for the next tick (life bit7 clear) is drawn
	// at pos + ((s16)(vel + acc) << 4) * num/den (the exact in-between of the deterministic
	// integration) with the real tick's colour (index life + 1). Particles that died on the real
	// tick are not redrawn; the next tick's spawns first appear on that tick.
	// ------------------------------------------------------------------------------------
	static void dh_53_PixelParticleEmitterHeld()
	{
		if (U32(CUR(), 0xBC) == 0) return;
		uint8_t *desc = PTR(CUR(), 0xB8);                                       // edi
		uint8_t *pool = PTR(CUR(), 0xBC);                                       // [ebp-8]
		uint8_t *ctrl = blob::GetBone(S16(desc, 6));                             // ecx
		int32_t ax = S16(ctrl, 0x8C);                                            // ebx
		int32_t az = S16(ctrl, 0x90);                                            // [ebp-4]
		int32_t ay = S16(ctrl, 0x8E);                                            // esi
		U16(WS(), 0xA0) = (uint16_t)div256(mul32(S16(desc, 0x0A), ax));
		U16(WS(), 0xA2) = (uint16_t)div256(mul32(S16(desc, 0x12), ay));
		U16(WS(), 0xA4) = (uint16_t)div256(mul32(S16(desc, 0x1A), az));
		U16(WS(), 0xA8) = (uint16_t)div256(mul32(S16(desc, 0x0C), ax));
		U16(WS(), 0xAA) = (uint16_t)div256(mul32(S16(desc, 0x14), ay));
		U16(WS(), 0xAC) = (uint16_t)div256(mul32(S16(desc, 0x1C), az));
		U16(WS(), 0xB0) = (uint16_t)div256(mul32(S16(desc, 0x0E), ax));
		U16(WS(), 0xB2) = (uint16_t)div256(mul32(S16(desc, 0x16), ay));
		U16(WS(), 0xB4) = (uint16_t)div256(mul32(S16(desc, 0x1E), az));
		U16(WS(), 0xB8) = (uint16_t)div256(mul32(S16(desc, 0x10), ax));
		U16(WS(), 0xBA) = (uint16_t)div256(mul32(S16(desc, 0x18), ay));
		U16(WS(), 0xBC) = (uint16_t)div256(mul32(S16(desc, 0x20), az));
		PTR(WS(), 0xE0) = pool + 0x10;
		// OT
		int32_t bias = S16(CUR(), 0xC0);
		if (bias == 0)
			S32(WS(), 0x64) = 0;
		else
			U32(WS(), 0x64) = ((uint32_t)bias & 0x3FFC) + U32(RT(), 0x4C);
		Mat4x3 *m = blob::GetParentMatrix(U16(CUR(), 0x9C));                     // ebx
		x::SetRotMatrix(m);
		x::SetTransVector(m);
		U32(WS(), 0xE4) = U32(pool, 4);
		uint8_t *p = PTR(WS(), 0xE0);                                            // esi
		int32_t n = U16(desc, 4);                                                // [ebp-0x10]
		uint8_t *pkt = PTR(CTX(), 0x7C);                                         // ebx / [ebp-0xc]
		if (n > 0)
		{
			do
			{
				if (!(U8(p, 0x18) & 0x80))
				{
					int32_t px, py, pz;       // position used for the draw
					int32_t cidx;             // colour index
					{
						// in-between of the next tick's integration, on locals
						int32_t d[3];
						for (int k = 0; k < 3; k++)
						{
							int16_t v = (int16_t)(U16(p, 0x0C + 4 * k) + U16(p, 0x0E + 4 * k));
							d[k] = g_held.den > 0 ? (int32_t)((int64_t)shl32(v, 4) * g_held.num / g_held.den) : 0;
						}
						px = add32(S32(p, 0), d[0]); py = add32(S32(p, 4), d[1]); pz = add32(S32(p, 8), d[2]);
						cidx = (int8_t)(uint8_t)(U8(p, 0x18) + 1);
					}
					uint8_t *v = WS() + 0xF0;                                   // [ebp-4]
					if (U16(desc, 2) == 0)
					{
						U16(v, 0) = (uint16_t)(px >> 8);
						U16(v, 2) = (uint16_t)(py >> 8);
						U16(v, 4) = (uint16_t)(pz >> 8);
					}
					if (U16(desc, 2) == 1)
					{
						int32_t rad = px >> 8;                                   // [ebp-8]
						U16(v, 2) = (uint16_t)(py >> 8);
						int32_t ang = (pz >> 4) & (int32_t)0xFFFFFFF0;           // ebx
						int32_t s = x::Sin(ang);
						U16(v, 0) = (uint16_t)((uint16_t)(mul32(s, rad) >> 12) + U16(CUR(), 0x94));
						int32_t c = x::Cos(ang);
						U16(v, 4) = (uint16_t)((uint16_t)(mul32(c, rad) >> 12) + U16(CUR(), 0x98));
					}
					// (other modes: ws+0xF0 is not written, the stale scratch is projected - vanilla)
					U8(pkt, 3) = 2;
					U32(pkt, 4) = U32(PTR(WS(), 0xE4), cidx * 4) | 0x6A000000;
					int32_t z = xl_TransformToScreen(WS() + 0xF0, pkt + 8, WS() + 0xFC, WS() + 0xFC);
					uint32_t ot = U32(WS(), 0x64);
					if (ot == 0) ot = U32(RT(), 0x4C) + ((uint32_t)z & ~3u);    // `and al, 0xfc`
					x::InsertPrimAltViewport(ot, pkt);
					pkt += 0xC;
				}
				p += 0x1C;
			} while (--n != 0);
		}
		PTR(CTX(), 0x7C) = pkt;
	}

	// ------------------------------------------------------------------------------------
	// 0xAEA420 (Draw 54 DrawEnvClipWindow), held frames: the packets are built in the private
	// packet memory (ctx+0x7C, advanced by 0x80) instead of the arena buffer (its contents must
	// not change on a held frame, and restoring them after the held draw would re-link the held
	// OT into the real tick's), the patched env is a local copy instead of the scratch above the
	// arena top; no allocation.
	// ------------------------------------------------------------------------------------
	alignas(4) static uint8_t g_local_env[0x1C];

	static uint8_t *held_local_env() { return g_local_env; }

	static uint8_t *held_packet_block()
	{
		uint8_t *pkt = PTR(CTX(), 0x7C);
		PTR(CTX(), 0x7C) = pkt + 0x80;
		return pkt;
	}

	// ------------------------------------------------------------------------------------
	// 0xAE4C70 (Draw 55 ScanlineJitterScreen), held frames: nothing allocated, the phases are
	// not written: the bars are drawn at phase + (s16)speed * num/den (the exact in-between); S is
	// saved and restored around the call (outside the saved set, and vanilla reads it stale).
	// ------------------------------------------------------------------------------------
	static void held_scratch55_save()
	{
		if (!g_held.active) return;
		int32_t v = S16(CUR(), 0x96);
		int32_t n = 0x6C - (mul32(v, 108) >> 8);
		int32_t lo = 0, hi = 0x3D8;
		if (n > 0)
		{
			if (0x2D8 - n < lo) lo = 0x2D8 - n;
			if (0x200 + n > hi) hi = 0x200 + n;
		}
		g_ss.save(PTR(CTX(), 0x74) + lo, (uint32_t)(hi - lo));
	}

	static uint16_t held_phase55(uint8_t *pool, uint8_t *desc, int k)
	{
		int32_t sp = (int16_t)U16(desc, 6 + 2 * k);
		int32_t d = g_held.den > 0 ? sp * g_held.num / g_held.den : 0;
		return (uint16_t)(U16(pool, 2 * k) + (uint16_t)d);
	}

	// ------------------------------------------------------------------------------------
	// 0xAE52C0 (Draw 56 LineRingTunnel), held frames: no allocation; the work area above the
	// arena top is saved and restored; an in-between outAngleY of 0 (the divisor; the real tick's
	// is non-zero) skips the draw.
	// ------------------------------------------------------------------------------------
	static bool held_skip56() { return g_held.active && S32(WS(), 0x70) == 0; }

	static void held_scratch56_save(uint8_t *desc)
	{
		if (!g_held.active) return;
		uint32_t nr = (uint32_t)U16(desc, 6) + 1;
		uint8_t *s0 = PTR(WS(), 0x60);
		uint32_t pv0 = ((uint32_t)s0 + nr * 17 + 4) & ~3u;
		uint32_t end = pv0 + nr * 0x88 + nr * 4;
		g_ss.save(s0, end - (uint32_t)s0);
	}
}

namespace part_eden_d
{
	// ------------------------------------------------------------------------------------
	// 0xAE44E0 (Draw 72 BattleModelPivotAttach), held frames: runs on the in-between pose (so the
	// shadow follows it), but E bytes 0..0x99 (every field written: +0/+1, +0xC..+0x11,
	// +0x1C..+0x21, the anim header +0x60.., +0x94..+0x99) and E's skeleton (0x10 + 0x30 * bones,
	// rebuilt by BuildBoneMatricesFromPose / ComputeBonesWorldMatrices) are saved before and
	// restored after: no battle-entity write persists. The one-shot init writes only the bone
	// (saved by HeldFrame) and E+1 (restored). Never runs while predicting (draw handler).
	// ------------------------------------------------------------------------------------
	static void dh_72_Held()
	{
		static uint8_t saved_e[0x9A];
		static uint8_t saved_sk[0x10 + 0x30 * 256];
		uint8_t *E = PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B));
		uint8_t *sk = PTR(E, 0x64) ? PTR(PTR(E, 0x64), 0) : nullptr;
		uint32_t sk_n = sk ? 0x10 + 0x30 * (uint32_t)U8(sk, 0) : 0;
		memcpy(saved_e, E, sizeof(saved_e));
		if (sk_n) memcpy(saved_sk, sk, sk_n);
		BattleModelPivotAttach();
		if (sk_n) memcpy(sk, saved_sk, sk_n);
		memcpy(E, saved_e, sizeof(saved_e));
	}
}
}
}

namespace ff8fx
{
namespace gfc
{
namespace eden
{
	// held frames of the clone: the engine's (gfc_engine_held.inc)
	static bool Ready() { return HeldReady(g_c); }
	static void Frame(int num, int den) { HeldFrame(g_c, num, den); }
	static bool Camera(int num, int den, int16_t world[3], int16_t lookat[3]) { return HeldCamera(g_c, num, den, world, lookat); }
}
}

	static void register_gfc_eden_held()
	{
		using namespace gfc;
		mark_held(0xAE3470);
		register_module_held(206, eden::Ready, eden::Frame);
		register_module_camera(206, eden::Camera);
	}
}
