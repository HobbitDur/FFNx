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

// Effect 76: Ultimecia's final form appears (enemy attack 355 of kernel.bin, no name; Ultimecia
// c0m126; MAG_076_ULTIMECIA_FINAL_FORM_SPAWN).
//
// Structure (setup 0x61F9A0, file loader 0x61F980 = texture 0xDE4224 "magic/mag075.tim"; the setup
// keeps the texture base 0x24C66A0, the cast context 0x24C6650, the CASTER slot 0x24C6658, the
// first target slot 0x24C3278 and the caster entity's matrix (+0x40) 0x24C6680, starts the root
// task (queue 0x24C3290, 1 x 0x10) and the intro (queue 0x24C5830, pool 0x24C5840, 0x64 x 0x24),
// frees both particle pools and queues the TIM upload):
//   RootTask (0x6225A0) - module packet cursor 0x24C66A4 = texture base (+0xC000 on odd ticks),
//     runs the master queue, ends when it is empty.
//   Intro (0x61FA80, 0x28 ticks) - tick 0: the caster's x / z kept (0x24C5828 / 0x24C582C), white
//     screen flash, caster y = 5000 (entity +0x1E) and hidden (flags | 4), sub_4A8480(0), the
//     fanfare sequence 0xEAA7EC (BS_R0win_QueueVictoryFanfare, done byte 0x24C5820: the intro
//     waits for it from tick 1); 0xA battle model load (loadBS_36Or37); 0x26 model swap (waits
//     while a file load is pending) + ambient colour to every entity with flag 2 (+0x2C); 0x27 wav
//     stream (Music_WavStream_Play); then Wait (0x61FC90, 0x32 ticks), then the Master.
//   Master (0x61FCD0) - keeps 0x24C6660 = camera x caster matrix; a 0x29A-tick timeline:
//       0      camera script 0xDE4160 (shared camera-script task 0x63E9C0 in the master queue),
//              camera return armed, voice slot (0xDE3FF0)
//       0x28   chain 0xF, caster shown (flags & ~4), Burst (0x6201A0); 0x32 Spin (0x620530);
//              0x4A..0x59 white flash down
//       0x5A.. caster y -= 0x37 every tick (90 ticks); 0x5B Rise (0x620820); 0x96 Halo (0x620BD0);
//              0xA0 chain 0x10
//       0xB4   caster y back (0x24C582A)
//       0xF0.. battle messages 1 / 3 / 4 (0x10E, 0x15E, 0x1AE; music volume at 0x10E), Flash1
//              (0x620DD0) at 0x1DF, Flash2 (0x620F60) at 0x1E3, screen fade task at 0x1F6
//       0x1FE  chain 0x11; 0x1FF sound 0xDE4148; 0x203 white feedback task (0x620140, 0x5F ticks) +
//              texture restore task (0x622560); 0x204 Debris (0x6215E0); 0x206 Ring (0x621110) +
//              Burst2 (0x621250)
//       0x25D  damage (target record of action 0); 0x296 voice released; 0x29A flash off, camera
//              save words (0xB8B800 = 0xB8B7F0, 0x1D977A0 = 0x1D8E038), sub_4A8480(1), update
//              flags & ~0x400, ends.
//   Particle pools: pool 1 0x24C32A0 (100 x 0x20: Burst bit 0, Spin bit 1, Burst2 bit 2), pool 2
//   0x24C3F20 (200 x 0x20: Rise bit 0, Debris bit 0). Every particle task spawns, then draws and
//   updates every particle of its kind; particle tasks end once their pool kind is empty.
//   Model draws: Effect_RenderPrimModel (0x572200; Burst / Spin into the frame packets 0x1D8E054,
//   the others into the module packets) and the module's prim renderer (0x6219D0, Debris; a copy of
//   Gastric Juice's 0x5E0450).
// Every task but RootTask and the texture restore tests the draw-only flags (battle_to_update_flags
// & 0x201; the intro only bit 0, or bit 9 with a file load pending).
// The effect WRITES the caster entity (position y +0x1E, flags +0x00 bit 2), every flag-2 entity's
// +0x2C (ambient colour), the update flags (& ~0x400), the camera save words 0xB8B800..0xB8B80F and
// 0x1D977A0 and, through the camera-script task, the battle camera.
// Module globals: 0x24C3270..0x24C66AC.

#include "mag_common.h"
#include <string.h>

namespace ff8fx
{
namespace uff076
{
	using namespace eng;
	using namespace magc;

	// raw memory access (the tasks follow the listing offset by offset)
	template<typename T> inline T &AT(uint32_t p, int32_t o) { return *(T *)(p + o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline uint32_t P(const void *p) { return (uint32_t)p; }

	// --- module globals ---
	static const uint32_t SPAWN_POS = 0x24C3270;                      // 4 x int16 (Flash1 / Flash2 anchor)
	inline TaskQueue *QRoot() { return (TaskQueue *)0x24C3290; }      // pool 0x24C3280, 1 x 0x10
	static const uint32_t POOL1 = 0x24C32A0, POOL1_END = 0x24C3F20;   // 100 x 0x20
	static const uint32_t POOL2 = 0x24C3F20, POOL2_END = 0x24C5820;   // 200 x 0x20
	inline uint8_t &FanfareDone() { return var<uint8_t>(0x24C5820); }
	static const uint32_t SAVED_XY = 0x24C5828, SAVED_Z = 0x24C582C;  // caster +0x1C / +0x20 dwords
	inline int16_t SavedX() { return var<int16_t>(0x24C5828); }
	inline int16_t SavedY() { return var<int16_t>(0x24C582A); }
	inline int16_t SavedZ() { return var<int16_t>(0x24C582C); }
	inline TaskQueue *QMaster() { return (TaskQueue *)0x24C5830; }    // pool 0x24C5840, 0x64 x 0x24
	inline CastContext *&Ctx() { return var<CastContext *>(0x24C6650); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x24C6658); }
	static const uint32_t CASTER_VIEW = 0x24C6660;                    // camera x CASTER_FRAME
	static const uint32_t CASTER_FRAME = 0x24C6680;                   // caster entity +0x40 (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x24C66A0); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x24C66A4); }
	inline uint32_t &Voice() { return var<uint32_t>(0x24C66A8); }
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }
	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }

	static const uint32_t ORIG_RootTask = 0x6225A0;
	static const uint32_t ORIG_Intro = 0x61FA80;
	static const uint32_t ORIG_Wait = 0x61FC90;
	static const uint32_t ORIG_Master = 0x61FCD0;
	static const uint32_t ORIG_FeedbackA = 0x6200F0;
	static const uint32_t ORIG_FeedbackB = 0x620140;
	static const uint32_t ORIG_Burst = 0x6201A0;
	static const uint32_t ORIG_Spin = 0x620530;
	static const uint32_t ORIG_Rise = 0x620820;
	static const uint32_t ORIG_Halo = 0x620BD0;
	static const uint32_t ORIG_Flash1 = 0x620DD0;
	static const uint32_t ORIG_Flash2 = 0x620F60;
	static const uint32_t ORIG_Ring = 0x621110;
	static const uint32_t ORIG_Burst2 = 0x621250;
	static const uint32_t ORIG_Debris = 0x6215E0;
	static const uint32_t ORIG_TexRestore = 0x622560;

	static const uint32_t MODEL_Burst = 0xDE2DE0, MODEL_Spin = 0xDE2EB8, MODEL_Rise = 0xDE25E8, MODEL_Halo = 0xDE27F0;
	static const uint32_t MODEL_Flash2 = 0xDE37A8, MODEL_Ring = 0xDE1748, MODEL_Debris = 0xDE2470;
	static const int32_t *const UP = (const int32_t *)0xDE4150;      // reference direction of the pool particles
	static const uint32_t CAMERA_SCRIPT = 0xDE4160;
	static const void *const SOUND_Voice = (const void *)0xDE3FF0;
	static const void *const SOUND_Hit = (const void *)0xDE4148;
	static const uint32_t FANFARE = 0xEAA7EC;
	static const uint32_t WAV_DIR = 0x1A77F88, WAV_NAME = 0xDE4230;   // directory string, 10 name bytes

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t PauseQuery() { return fn<int32_t (__cdecl *)()>(0x508500)(); }                                           // sub_508500
	inline void CameraScriptStart(uint32_t script, uint32_t frame, void *entity, TaskQueue *q, int32_t n) { fn<void (__cdecl *)(uint32_t, uint32_t, void *, TaskQueue *, int32_t)>(0x63E960)(script, frame, entity, q, n); } // MAG_066_sub_63E960
	inline void ArmCameraReturn() { fn<void (__cdecl *)()>(0x50A730)(); }                                                   // BS_Camera_ArmReturnAfterEffect
	inline void Sub4A8480(int32_t a) { fn<void (__cdecl *)(int32_t)>(0x4A8480)(a); }
	inline int32_t VoiceBusy(uint32_t v) { return fn<int32_t (__cdecl *)(uint32_t)>(0x4A2900)(v); }                         // sub_4A2900
	inline void VoiceRelease(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(v); }                                  // sub_4A2940
	inline void ChainTransformation(void *entity, int32_t id) { fn<void (__cdecl *)(void *, int32_t)>(0x505C00)(entity, id); } // QueueChainTransformation
	inline void BattleMessage(int32_t slot, int32_t a, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x4876F0)(slot, a, b); } // printMonsterRelatedText
	inline void ScreenFeedback(int32_t a, int32_t r, int32_t g, int32_t b, int32_t c) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t, int32_t)>(0x47CF50)(a, r, g, b, c); } // Battle_RequestScreenFeedback
	inline void QueueFanfare(uint32_t seq, uint8_t *done) { fn<void (__cdecl *)(uint32_t, uint8_t *)>(0x501A20)(seq, done); } // BS_R0win_QueueVictoryFanfare
	inline void MusicChannelVolume(int32_t channel, int32_t volume) { fn<void (__cdecl *)(int32_t, int32_t)>(0x501B10)(channel, volume); } // BS_Music_SetChannelVolumeCommand
	inline void WavStreamPlay(int32_t a, const char *name, int32_t b, int32_t c, int32_t d, int32_t e) { fn<void (__cdecl *)(int32_t, const char *, int32_t, int32_t, int32_t, int32_t)>(0x46CBB0)(a, name, b, c, d, e); } // Music_WavStream_Play
	inline void LoadBattleModel(int32_t a, uint32_t dst) { fn<void (__cdecl *)(int32_t, uint32_t)>(0x512AA0)(a, dst); }   // loadBS_36Or37
	inline void SwapBattleModel(int32_t a, uint32_t src) { fn<void (__cdecl *)(int32_t, uint32_t)>(0x512AC0)(a, src); }   // sub_512AC0
	inline int32_t GetEffectSpawnPosition(void *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline void NormalizeSVector(const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const int16_t *, int16_t *)>(0x56BDE0)(in, out); } // sub_56BDE0
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); } // GTE_MatrixMultiply

	// GTE (module prim renderer)
	inline void GteLoadV012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a, b, c); }
	inline void GteLoadV0u(uint32_t v) { fn<void (__cdecl *)(uint32_t)>(0x45DF80)(v); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3C0)(dst); }
	inline void GteReadFLAG(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E210)(dst); }
	inline void GteReadSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a, b, c); }
	inline void GteReadSXY2u(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E260)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(uint32_t dst) { fn<void (__cdecl *)(uint32_t)>(0x45E3D0)(dst); }
	inline void GteSetFarColor(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void GteLoadRGBC(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E110)(c); }        // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(uint32_t c) { fn<void (__cdecl *)(uint32_t)>(0x45E360)(c); }       // set_param_with_dword_1CA8A68
	inline void GteLoadRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E120)(a, b, c); }
	inline void GteStoreRGB012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E370)(a, b, c); }
	inline void InsertPrim(uint32_t bucket, uint32_t pkt) { InsertPrimAutoDepth(bucket, (void *)pkt); }

	inline Mat4x3 *M(uint32_t p) { return (Mat4x3 *)p; }
	inline int32_t *V(uint32_t p) { return (int32_t *)p; }

	// ------------------------------------------------------------------
	// Node layouts (master queue: pool of 0x64 nodes of 0x24 bytes)
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode { TaskNode hdr; int16_t counter; uint16_t pad; };
	struct Node
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t e;         // +0x0E feedback: tick count; texture restore: done flag
		int16_t pos[4];    // +0x10 Flash2 / Ring anchor
		int16_t angle;     // +0x18 Ring z angle
		int16_t spin;      // +0x1A Ring spin
		int16_t scale;     // +0x1C Halo / Flash / Ring scale
		int16_t step;      // +0x1E scale step
		int16_t sy;        // +0x20 Flash1 y scale
		uint16_t pad22;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10, "root node is 0x10 bytes");
	static_assert(sizeof(Node) == 0x24, "master queue nodes are 0x24 bytes");
	// pool particles (0x20 bytes): +0 kind bits (dword, 0 = free), +4 age, +6 size / delay,
	// +8 position, +0x10.. velocity / scale / angle words, +0x18.. direction / angles (per task)
}
}

#ifdef FF8_FX_HELD
#include "mag076_ultimecia_final_form_spawn_held.h"
#endif

namespace ff8fx
{
namespace uff076
{
	// ==================================================================
	// Module prim renderer (0x6219D0, a copy of Gastric Juice's 0x5E0450; header 0x5C bytes on the
	// scratch: +0x00 model, +0x04 vertices, +0x08..+0x0A far colour, +0x0C fade, +0x10 / +0x14
	// texture page / clut words, +0x18 texture offset, +0x1C flags, +0x20 section cursor, +0x24
	// NCLIP, +0x2C OTZ, +0x30 GTE FLAG, +0x58 draw mode word)
	// ==================================================================

	// clip bits of one corner: x outside 0..0xA00 -> xbit, y outside 0..0x6C0 -> ybit
	static uint32_t ClipX(uint32_t xy, uint32_t bit) { const int16_t x = S16(xy, 0); return (x < 0 || x > 0xA00) ? bit : 0; }
	static uint32_t ClipY(uint32_t xy, uint32_t bit) { const int16_t y = S16(xy, 2); return (y < 0 || y > 0x6C0) ? bit : 0; }

	// the texture page / clut words of a packet (flags 0x100 / 0x400, 0x200 / 0x800)
	static void PageWords(uint32_t h, uint32_t pk, int32_t page_off)
	{
		const uint32_t f = U32(h, 0x1C);
		if (f & 0x400) U16(pk, page_off) = (uint16_t)(U16(pk, page_off) + U16(h, 0x10));
		else if (f & 0x100) U16(pk, page_off) = U16(h, 0x10);
		if (f & 0x800) U16(pk, 0xE) = (uint16_t)(U16(pk, 0xE) + U16(h, 0x14));
		else if (f & 0x200) U16(pk, 0xE) = U16(h, 0x14);
	}

	// the face's semi-transparency bit (flags bit 0 / 1 sets it, bit 2 / 3 clears it)
	static void SemiTrans(uint32_t h, uint32_t pk, uint32_t set, uint32_t clear)
	{
		const uint32_t f = U32(h, 0x1C);
		if (f & set) U32(pk, 4) = U32(pk, 4) | 0x2000000;
		if (f & clear) U32(pk, 4) &= 0xFDFFFFFF;
	}

	// 0x621AC0: flat textured triangles (0x14-byte records -> 0x24-byte packets)
	static uint32_t PrimFT3(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x14)
		{
			GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPT();
			U32(pk, 0) = 0x8000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 1, 4);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			U32(pk, 0x14) = U32(rec, 0x10) + d;
			U32(pk, 0x1C) = (U32(rec, 8) >> 16) + d;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			PageWords(h, pk, 0x16);
			GteReadMAC0(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteAVSZ3();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x10, 2) | ClipX(pk + 0x18, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x10, 0x20) | ClipY(pk + 0x18, 0x40);
			if ((m & 7) == 7 || (m & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(S32(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			U32(pk, 0x20) = U32(h, 0x58);
			InsertPrim(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), pk);
			pk += 0x24;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x621D20: flat textured quads (0x18-byte records -> 0x2C-byte packets)
	static uint32_t PrimFT4(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x18)
		{
			GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPT();
			U32(pk, 0) = 0xA000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 1, 4);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			const uint32_t uv3 = (d << 16) + d + U32(rec, 0x14);
			U32(pk, 0x1C) = uv3;
			U32(pk, 0x14) = U32(rec, 0x10) + d;
			U32(pk, 0x24) = uv3 >> 16;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			PageWords(h, pk, 0x16);
			GteReadMAC0(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x10)) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x10, 2) | ClipX(pk + 0x18, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x10, 0x20) | ClipY(pk + 0x18, 0x40);
			GteReadSXY2u(pk + 0x20);
			GteAVSZ4();
			m |= ClipX(pk + 0x20, 8) | ClipY(pk + 0x20, 0x80);
			if ((m & 0xF) == 0xF || (m & 0xF0) == 0xF0) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x40)
			{
				GteLoadRGBC(pk + 4);
				GteSetIR0(S32(h, 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			U32(pk, 0x28) = U32(h, 0x58);
			InsertPrim(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), pk);
			pk += 0x2C;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x621FF0: Gouraud textured triangles (0x1C-byte records -> 0x2C-byte packets)
	static uint32_t PrimGT3(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x1C)
		{
			GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPT();
			U32(pk, 0) = 0xA000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 2, 8);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			U32(pk, 0x18) = U32(rec, 0x10) + d;
			U32(pk, 0x24) = (U32(rec, 8) >> 16) + d;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			PageWords(h, pk, 0x1A);
			GteReadMAC0(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			GteAVSZ3();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x14, 2) | ClipX(pk + 0x20, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x14, 0x20) | ClipY(pk + 0x20, 0x40);
			if ((m & 7) == 7 || (m & 0x70) == 0x70) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0x14, rec + 0x18, pk + 4);
				GteSetIR0(S32(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 4);
			}
			else
			{
				U32(pk, 0x10) = U32(rec, 0x14);
				U32(pk, 0x1C) = U32(rec, 0x18);
			}
			U32(pk, 0x28) = U32(h, 0x58);
			InsertPrim(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), pk);
			pk += 0x2C;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x622250: Gouraud textured quads (0x24-byte records -> 0x38-byte packets)
	static uint32_t PrimGT4(uint32_t h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		const uint32_t verts = U32(h, 4);
		int32_t count = S32(U32(h, 0x20), 0);
		uint32_t rec = U32(h, 0x20) + 4;
		U32(h, 0x20) = rec;
		for (; count > 0; count--, rec += 0x24)
		{
			GteLoadV012(verts + U16(rec, 4) * 4, verts + U16(rec, 6) * 4, verts + U16(rec, 8) * 4);
			GteRTPT();
			U32(pk, 0) = 0xD000000;
			U32(pk, 4) = U32(rec, 0);
			SemiTrans(h, pk, 2, 8);
			const uint32_t d = U32(h, 0x18);
			U32(pk, 0xC) = U32(rec, 0xC) + d;
			const uint32_t uv3 = (d << 16) + d + U32(rec, 0x14);
			U32(pk, 0x24) = uv3;
			U32(pk, 0x18) = U32(rec, 0x10) + d;
			U32(pk, 0x30) = uv3 >> 16;
			GteReadFLAG(h + 0x30);
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			PageWords(h, pk, 0x1A);
			GteReadMAC0(h + 0x24);
			const int32_t nc = S32(h, 0x24);
			if (nc == 0) continue;
			if (nc < 0 && !(U8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012(pk + 8, pk + 0x14, pk + 0x20);
			GteLoadV0u(verts + U16(rec, 0xA) * 4);
			GteRTPS();
			uint32_t m = ClipX(pk + 8, 1) | ClipX(pk + 0x14, 2) | ClipX(pk + 0x20, 4);
			m |= ClipY(pk + 8, 0x10) | ClipY(pk + 0x14, 0x20) | ClipY(pk + 0x20, 0x40);
			GteReadSXY2u(pk + 0x2C);
			GteAVSZ4();
			m |= ClipX(pk + 0x2C, 8) | ClipY(pk + 0x2C, 0x80);
			if ((m & 0xF) == 0xF || (m & 0xF0) == 0xF0) continue;
			GteReadOTZ32(h + 0x2C);
			if (U8(h, 0x1C) & 0x80)
			{
				GteLoadRGB012(rec + 0x18, rec + 0x1C, rec + 0x20);
				GteSetIR0(S32(h, 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 0x28);
				GteLoadRGBC(pk + 4);
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			else
			{
				U32(pk, 0x10) = U32(rec, 0x18);
				U32(pk, 0x1C) = U32(rec, 0x1C);
				U32(pk, 0x28) = U32(rec, 0x20);
			}
			U32(pk, 0x34) = U32(h, 0x58);
			InsertPrim(ot + (uint32_t)shl32(S32(h, 0x2C) >> (shift & 31), 2), pk);
			pk += 0x38;
		}
		U32(h, 0x20) = rec;
		return pk;
	}

	// 0x6219D0: the model's sections (two skipped words, FT3, FT4, two skipped words, GT3, GT4; an
	// empty section is one zero word); vertices = model + 8 unless flag 0x2000, texture offset 0
	// unless flag 0x1000
	static uint32_t PrimRender(uint32_t h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t f = U32(h, 0x1C);
		if (!(f & 0x2000)) U32(h, 4) = U32(h, 0) + 8;
		U32(h, 0x20) = U32(U32(h, 0), 0) + U32(h, 0);
		if (!(f & 0x1000)) U32(h, 0x18) = 0;
		U32(h, 0x58) = 0xE1000220;
		GteSetFarColor(U8(h, 8), U8(h, 9), U8(h, 0xA));
		U32(h, 0x20) += 8;
		if (U32(U32(h, 0x20), 0)) cursor = PrimFT3(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		if (U32(U32(h, 0x20), 0)) cursor = PrimFT4(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		U32(h, 0x20) += 8;
		if (U32(U32(h, 0x20), 0)) cursor = PrimGT3(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		if (U32(U32(h, 0x20), 0)) cursor = PrimGT4(h, ot, shift, cursor);
		else U32(h, 0x20) += 4;
		return cursor;
	}

	// ------------------------------------------------------------------
	// helpers
	// ------------------------------------------------------------------

	// first free particle of a pool (kind dword 0), or 0
	static uint32_t PoolFree(uint32_t pool, uint32_t end)
	{
		for (uint32_t e = pool; e < end; e += 0x20)
			if (U32(e, 0) == 0) return e;
		return 0;
	}

	// au_re_BdLinkTask_59 (0x61FC40): a master-queue task, counter and +0xE cleared
	static Node *AddMasterTask(uint32_t fn)
	{
		Node *n = (Node *)AddTaskToQueue(QMaster(), fn);
		n->counter = 0;
		n->e = 0;
		return n;
	}

	// the particle tasks' base matrix (scratch + 8): camera x translation (caster x, y, caster z)
	static void BaseMatrix(uint32_t s, int32_t y)
	{
		S16(s, 0) = 0;
		S16(s, 2) = 0;
		S16(s, 4) = 0;
		ComposeZYXRotationMatrix((const int16_t *)s, M(s + 8));
		S32(s, 0x1C) = SavedX();
		S32(s, 0x20) = y;
		S32(s, 0x24) = SavedZ();
		ComposeAffineTransform(&Camera(), M(s + 8), M(s + 8));
	}

	// the pool particles' rotation: the reference direction UP turned onto the particle's
	// direction (+0x18..+0x1C), plus `extra` about the same axis
	static void TurnTo(uint32_t s, uint32_t e, int32_t vec, int32_t axis, uint32_t out, int32_t extra, bool add)
	{
		S32(s, vec) = S16(e, 0x18);
		S32(s, vec + 4) = S16(e, 0x1A);
		S32(s, vec + 8) = S16(e, 0x1C);
		int32_t a = RotationBetweenVectors(UP, V(s + vec), V(s + axis));
		if (add) a += extra;
		BuildAxisAngleRotationMatrix(a, M(out), V(s + axis));
	}

	// the particle / part matrix (scratch + 0x28) composed with the base matrix, into the GTE
	static void LoadPartMatrix(uint32_t s)
	{
		ComposeAffineTransform(M(s + 8), M(s + 0x28), M(s + 0x28));
		GteSetRotMatrix(M(s + 0x28));
		GteSetTransVector(M(s + 0x28));
	}

	// header fade of a draw: flags 0xF3 (fade +0x0C toward the far colour) or 0x33 (+0x0C not written)
	static const uint32_t FADE = 0xF3, NOFADE = 0x33;
	static void SetFade(uint32_t h, uint32_t flags, int32_t fade)
	{
		U32(h, 0x1C) = flags;
		if (flags == FADE) S32(h, 0xC) = fade;
	}

	// ------------------------------------------------------------------
	// Burst (0x6201A0): pool 1 kind 1, counter 0..0x32 spawns particles
	// (counter / 20 + 1 a tick: size, upward direction, speed); each draws flat (scale size / 2, size, 0) turned to its
	// direction, accelerates by 1/8 a tick, fades out from age 0x10, dies at 0x18
	// ------------------------------------------------------------------
	static uint32_t BurstFade(int32_t age, int32_t *fade)
	{
		if (age < 0x10) return NOFADE;
		*fade = (age - 0x10) << 9;
		return FADE;
	}

	static void BurstDraw(uint32_t s, uint32_t h, uint32_t e)
	{
		TurnTo(s, e, 0x48, 0x58, s + 0x28, 0, false);
		S32(s, 0x3C) = S16(e, 8);
		S32(s, 0x40) = S16(e, 0xA);
		S32(s, 0x44) = S16(e, 0xC);
		const int32_t size = S16(e, 6);
		S32(s, 0x6C) = size;
		S32(s, 0x68) = size >> 1;
		Scale3DMatrix(M(s + 0x28), V(s + 0x68));
		LoadPartMatrix(s);
		int32_t fade = 0;
		const uint32_t flags = BurstFade(S16(e, 4), &fade);
		SetFade(h, flags, fade);
		FX_HELD(held_fade(h);)
		FrameCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, FrameCursor());
	}

	static void BurstMove(uint32_t e)
	{
		S16(e, 4) = (int16_t)(S16(e, 4) + 1);
		S16(e, 8) = (int16_t)(S16(e, 8) + S16(e, 0x10));
		S16(e, 0xA) = (int16_t)(S16(e, 0xA) + S16(e, 0x12));
		S16(e, 0xC) = (int16_t)(S16(e, 0xC) + S16(e, 0x14));
		S16(e, 0x10) = (int16_t)(S16(e, 0x10) + (S16(e, 0x10) >> 3));
		S16(e, 0x12) = (int16_t)(S16(e, 0x12) + (S16(e, 0x12) >> 3));
		S16(e, 0x14) = (int16_t)(S16(e, 0x14) + (S16(e, 0x14) >> 3));
	}

	static uint32_t __cdecl BurstTask(TaskNode *node)
	{
		Node *n = (Node *)node;
		const uint32_t s = P(FieldAlloc(0x78));
		if (!DrawOnly() && n->counter <= 0x32)
		{
			const int32_t count = n->counter / 20 + 1;
			for (int32_t k = 0; k < count; k++)
			{
				const uint32_t e = PoolFree(POOL1, POOL1_END);
				if (!e) break;
				U32(e, 0) = 1;
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % 0x680 + 0x200);
				S32(s, 0x48) = CrtRand() % 0x1000 - 0x800;
				S32(s, 0x4C) = -0xA00 - CrtRand() % 0x600;
				S32(s, 0x50) = CrtRand() % 0x1000 - 0x800;
				NormalizeVector(V(s + 0x48), V(s + 0x48));
				const int32_t v = CrtRand() % 1000 + 0x9C4;
				S16(e, 8) = (int16_t)(mul32(S32(s, 0x48), v) >> 12);
				S16(e, 0xA) = (int16_t)(mul32(S32(s, 0x4C), v) >> 12);
				S16(e, 0xC) = (int16_t)(mul32(S32(s, 0x50), v) >> 12);
				const int32_t w = CrtRand() % 200 + 0x14;
				S16(e, 0x10) = (int16_t)(mul32(S32(s, 0x48), w) >> 12);
				S16(e, 0x12) = (int16_t)(mul32(S32(s, 0x4C), w) >> 12);
				S16(e, 0x14) = (int16_t)(mul32(S32(s, 0x50), w) >> 12);
				S16(e, 0x18) = (int16_t)S32(s, 0x48);
				S16(e, 0x1A) = (int16_t)S32(s, 0x4C);
				S16(e, 0x1C) = (int16_t)S32(s, 0x50);
			}
		}
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(held_note_task(ORIG_Burst, node, s, nullptr);)
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = MODEL_Burst;
		U32(h, 8) = 0;
		int32_t alive = 0;
		BaseMatrix(s, 0xFA0);
		S32(s, 0x70) = 0;
		for (uint32_t e = POOL1; e < POOL1_END; e += 0x20)
		{
			if (!(U8(e, 0) & 1)) continue;
			BurstDraw(s, h, e);
			if (DrawOnly()) continue;
			if (S16(e, 4) >= 0x18)
			{
				U32(e, 0) = 0;
				continue;
			}
			BurstMove(e);
			alive++;
		}
		FieldFree(0x58);
		FieldFree(0x78);
		if (DrawOnly()) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Spin (0x620530): pool 1 kind 2, every 4th tick of counters 0..0x32 one particle (size,
	// direction, spin 2..5 either way); drawn at the base (scale size, 0x6000, size) turned to its
	// direction and spun about it; fades in for 8 ticks, out from age 0x10, dies at 0x18
	// ------------------------------------------------------------------
	static uint32_t SpinFade(int32_t age, int32_t *fade)
	{
		if (age < 8) *fade = (8 - age) << 9;
		else if (age >= 0x10) *fade = (age - 0x10) << 9;
		else return NOFADE;
		return FADE;
	}

	static void SpinDraw(uint32_t s, uint32_t h, uint32_t e)
	{
		TurnTo(s, e, 0x48, 0x58, s + 0x28, S16(e, 0x14), true);
		S32(s, 0x44) = 0;
		S32(s, 0x40) = 0;
		S32(s, 0x3C) = 0;
		const int32_t size = S16(e, 6);
		S32(s, 0x70) = size;
		S32(s, 0x68) = size;
		Scale3DMatrix(M(s + 0x28), V(s + 0x68));
		LoadPartMatrix(s);
		int32_t fade = 0;
		const uint32_t flags = SpinFade(S16(e, 4), &fade);
		SetFade(h, flags, fade);
		FX_HELD(held_fade(h);)
		FrameCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, FrameCursor());
	}

	static void SpinMove(uint32_t e)
	{
		S16(e, 4) = (int16_t)(S16(e, 4) + 1);
		S16(e, 0x14) = (int16_t)(S16(e, 0x14) + S16(e, 0x16));
	}

	static uint32_t __cdecl SpinTask(TaskNode *node)
	{
		Node *n = (Node *)node;
		const uint32_t s = P(FieldAlloc(0x78));
		if (!DrawOnly() && n->counter <= 0x32 && n->counter % 4 == 0)
		{
			const uint32_t e = PoolFree(POOL1, POOL1_END);
			if (e)
			{
				U32(e, 0) = 2;
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % 0x1000 + 0x400);
				S16(e, 0x14) = 0;
				const int32_t spin = CrtRand() % 4 + 2;
				S16(e, 0x16) = (int16_t)spin;
				if (spin & 1) S16(e, 0x16) = (int16_t)-spin;
				S16(s, 0) = (int16_t)(CrtRand() % 0x1000 - 0x800);
				S16(s, 2) = (int16_t)(-0xA00 - CrtRand() % 0x600);
				S16(s, 4) = (int16_t)(CrtRand() % 0x1000 - 0x800);
				NormalizeSVector((const int16_t *)s, (int16_t *)(e + 0x18));
			}
		}
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(held_note_task(ORIG_Spin, node, s, nullptr);)
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = MODEL_Spin;
		U32(h, 8) = 0;
		int32_t alive = 0;
		BaseMatrix(s, 0xFA0);
		S32(s, 0x6C) = 0x6000;
		for (uint32_t e = POOL1; e < POOL1_END; e += 0x20)
		{
			if (!(U8(e, 0) & 2)) continue;
			SpinDraw(s, h, e);
			if (DrawOnly()) continue;
			if (S16(e, 4) >= 0x18)
			{
				U32(e, 0) = 0;
				continue;
			}
			SpinMove(e);
			alive++;
		}
		FieldFree(0x58);
		FieldFree(0x78);
		if (DrawOnly()) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Rise (0x620820): pool 2 kind 1, every 3rd tick of counters 0..0x5A one particle (position on
	// an upward cone, a y angle 0 / 0x800, scales x / y and a y-scale step); drawn turned about y
	// then to its direction, scale (x, y, 0) growing by the step (1/8 less each tick); fades from
	// age 0xC, dies at 0x14; module packets
	// ------------------------------------------------------------------
	static uint32_t RiseFade(int32_t age, int32_t *fade)
	{
		if (age < 0xC) return NOFADE;
		*fade = (age - 0xC) << 9;
		return FADE;
	}

	static void RiseDraw(uint32_t s, uint32_t h, uint32_t e)
	{
		S16(s, 2) = S16(e, 0x14);
		ComposeZYXRotationMatrix((const int16_t *)s, M(s + 0x28));
		TurnTo(s, e, 0x68, 0x78, s + 0x48, 0, false);
		MatrixMultiply(M(s + 0x48), M(s + 0x28));
		S32(s, 0x3C) = S16(e, 8);
		S32(s, 0x40) = S16(e, 0xA);
		S32(s, 0x44) = S16(e, 0xC);
		S32(s, 0x88) = S16(e, 0x10);
		S32(s, 0x8C) = S16(e, 0x12);
		Scale3DMatrix(M(s + 0x28), V(s + 0x88));
		LoadPartMatrix(s);
		int32_t fade = 0;
		const uint32_t flags = RiseFade(S16(e, 4), &fade);
		SetFade(h, flags, fade);
		FX_HELD(held_fade(h);)
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
	}

	static void RiseMove(uint32_t e)
	{
		S16(e, 4) = (int16_t)(S16(e, 4) + 1);
		const int16_t step = S16(e, 0x16);
		S16(e, 0x12) = (int16_t)(S16(e, 0x12) + step);
		S16(e, 0x16) = (int16_t)(step - step / 8);
	}

	static uint32_t __cdecl RiseTask(TaskNode *node)
	{
		Node *n = (Node *)node;
		const uint32_t s = P(FieldAlloc(0x98));
		if (!DrawOnly() && n->counter <= 0x5A && n->counter % 3 == 0)
		{
			const uint32_t e = PoolFree(POOL2, POOL2_END);
			if (e)
			{
				U32(e, 0) = 1;
				S16(e, 4) = 0;
				S32(s, 0x68) = CrtRand() % 0x1200 - 0x900;
				S32(s, 0x6C) = -0xA00 - CrtRand() % 0x600;
				S32(s, 0x70) = CrtRand() % 0x1200 - 0x900;
				NormalizeVector(V(s + 0x68), V(s + 0x68));
				const int32_t d = CrtRand() % 500 + 0x9C4;
				S16(e, 8) = (int16_t)(mul32(S32(s, 0x68), d) >> 12);
				S16(e, 0xA) = (int16_t)(mul32(S32(s, 0x6C), d) >> 12);
				S16(e, 0xC) = (int16_t)(mul32(S32(s, 0x70), d) >> 12);
				const int32_t sx = CrtRand() % 0x1000 + 0x600;
				S16(e, 0x10) = (int16_t)sx;
				S16(e, 0x14) = (int16_t)((sx & 1) << 11);
				const int32_t sy = (CrtRand() % 0x800 + 0x1000) / 6;
				S16(e, 0x16) = (int16_t)sy;
				S16(e, 0x12) = (int16_t)sy;
				S16(e, 0x18) = (int16_t)S32(s, 0x68);
				S16(e, 0x1A) = (int16_t)S32(s, 0x6C);
				S16(e, 0x1C) = (int16_t)S32(s, 0x70);
			}
		}
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(held_note_task(ORIG_Rise, node, s, nullptr);)
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = MODEL_Rise;
		U32(h, 8) = 0;
		int32_t alive = 0;
		BaseMatrix(s, 0xBB8);
		S16(s, 4) = 0;
		S16(s, 0) = 0;
		S32(s, 0x90) = 0;
		for (uint32_t e = POOL2; e < POOL2_END; e += 0x20)
		{
			if (!(U8(e, 0) & 1)) continue;
			RiseDraw(s, h, e);
			if (DrawOnly()) continue;
			if (S16(e, 4) >= 0x14)
			{
				U32(e, 0) = 0;
				continue;
			}
			RiseMove(e);
			alive++;
		}
		FieldFree(0x58);
		FieldFree(0x98);
		if (DrawOnly()) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Burst2 (0x621250): pool 1 kind 4, ten particles at counter 0 (start delay 0..12 ticks, a
	// position on a horizontal ring, scales, spin); after its delay each draws turned to its
	// direction (+ its angle), scale (x, y, x): y grows for 8 ticks, the angle turns from age 0x10,
	// fades from age 0x36, dies at 0x46; the task ends at counter 0x20 once the kind is empty;
	// module packets
	// ------------------------------------------------------------------
	static uint32_t Burst2Fade(int32_t age, int32_t *fade)
	{
		if (age < 0x36) return NOFADE;
		*fade = (age - 0x36) << 8;
		return FADE;
	}

	static void Burst2Draw(uint32_t s, uint32_t h, uint32_t e)
	{
		TurnTo(s, e, 0x48, 0x58, s + 0x28, S16(e, 0x14), true);
		S32(s, 0x3C) = S16(e, 8);
		S32(s, 0x40) = S16(e, 0xA);
		S32(s, 0x70) = S16(e, 0x10);
		S32(s, 0x68) = S16(e, 0x10);
		S32(s, 0x44) = S16(e, 0xC);
		S32(s, 0x6C) = S16(e, 0x12);
		Scale3DMatrix(M(s + 0x28), V(s + 0x68));
		LoadPartMatrix(s);
		int32_t fade = 0;
		const uint32_t flags = Burst2Fade(S16(e, 4), &fade);
		SetFade(h, flags, fade);
		FX_HELD(held_fade(h);)
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
	}

	// one tick of a particle that is not dying (its delay first)
	static void Burst2Move(uint32_t e)
	{
		if (S16(e, 6) > 0)
		{
			S16(e, 6) = (int16_t)(S16(e, 6) - 1);
			return;
		}
		const int16_t age = (int16_t)(S16(e, 4) + 1);
		S16(e, 4) = age;
		if (age < 8)
		{
			const int16_t step = S16(e, 0x16);
			S16(e, 0x12) = (int16_t)(S16(e, 0x12) + step);
			S16(e, 0x16) = (int16_t)(step - step / 6);
		}
		if (S16(e, 4) >= 0x10) S16(e, 0x14) = (int16_t)(S16(e, 0x14) + S16(e, 0xE));
	}

	static uint32_t __cdecl Burst2Task(TaskNode *node)
	{
		Node *n = (Node *)node;
		const uint32_t s = P(FieldAlloc(0x78));
		if (!DrawOnly() && n->counter == 0)
		{
			for (int32_t k = 0; k < 10; k++)
			{
				const uint32_t e = PoolFree(POOL1, POOL1_END);
				if (!e) break;
				U32(e, 0) = 4;
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % 4 + k);
				const int32_t a = CrtRand() % 0xD48 + 0x15C;
				S32(s, 0x48) = ComputeSin(a);
				S32(s, 0x4C) = 0;
				S32(s, 0x50) = ComputeCos(a);
				const int32_t r = CrtRand() % 100 + 0x578;
				S16(e, 8) = (int16_t)(mul32(S32(s, 0x48), r) >> 12);
				S16(e, 0xA) = 0;
				S16(e, 0xC) = (int16_t)(mul32(S32(s, 0x50), r) >> 12);
				S16(e, 0x10) = (int16_t)(CrtRand() % 0x1000 + 0x800);
				S16(e, 0x14) = 0;
				S16(e, 0xE) = (int16_t)(CrtRand() % 16 + 8);
				S16(e, 0x16) = 0xA00;
				S16(e, 0x12) = 0xA00;
				const int32_t q = CrtRand() % 0x400 + 0x100;
				S16(s, 0) = (int16_t)(mul32(S32(s, 0x48), q) >> 12);
				const int32_t up = CrtRand() % 0x600;
				S16(s, 4) = (int16_t)(mul32(S32(s, 0x50), q) >> 12);
				S16(s, 2) = (int16_t)(-0xA00 - up);
				NormalizeSVector((const int16_t *)s, (int16_t *)(e + 0x18));
			}
		}
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(held_note_task(ORIG_Burst2, node, s, nullptr);)
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = MODEL_Spin;
		U32(h, 8) = 0;
		int32_t alive = 0;
		BaseMatrix(s, 0);
		for (uint32_t e = POOL1; e < POOL1_END; e += 0x20)
		{
			if (!(U8(e, 0) & 4)) continue;
			if (S16(e, 6) == 0) Burst2Draw(s, h, e);
			if (DrawOnly()) continue;
			if (S16(e, 4) >= 0x46)
			{
				U32(e, 0) = 0;
				continue;
			}
			Burst2Move(e);
			alive++;
		}
		FieldFree(0x58);
		FieldFree(0x78);
		if (DrawOnly()) return 0;
		n->counter++;
		if (n->counter >= 0x20 && alive == 0) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Debris (0x6215E0): pool 2 kind 1, ten particles a tick for counters 0..0x2A (size, outward
	// and upward speed, two spinning angles); drawn with angles (x, y, y), scale size, through the
	// module prim renderer; the speed slows 1/8 (x, z) / grows 1/16 (y), the spins grow 1/16; fades
	// in for 4 ticks, out from age 0x18, dies at 0x20; frame packets
	// ------------------------------------------------------------------
	static uint32_t DebrisFade(int32_t age, int32_t *fade)
	{
		if (age < 4) *fade = (4 - age) << 10;
		else if (age >= 0x18) *fade = (age - 0x18) << 9;
		else return NOFADE;
		return FADE;
	}

	static void DebrisDraw(uint32_t s, uint32_t h, uint32_t e)
	{
		S16(s, 0) = S16(e, 0x18);
		S16(s, 2) = S16(e, 0x1C);
		S16(s, 4) = S16(e, 0x1C);
		ComposeZYXRotationMatrix((const int16_t *)s, M(s + 0x28));
		S32(s, 0x40) = S16(e, 0xA);
		S32(s, 0x44) = S16(e, 0xC);
		S32(s, 0x3C) = S16(e, 8);
		const int32_t size = S16(e, 6);
		S32(s, 0x50) = size;
		S32(s, 0x4C) = size;
		S32(s, 0x48) = size;
		Scale3DMatrix(M(s + 0x28), V(s + 0x48));
		LoadPartMatrix(s);
		int32_t fade = 0;
		const uint32_t flags = DebrisFade(S16(e, 4), &fade);
		SetFade(h, flags, fade);
		FX_HELD(held_fade(h);)
		FrameCursor() = PrimRender(h, RenderOT44(), 2, FrameCursor());
	}

	static void DebrisMove(uint32_t e)
	{
		S16(e, 4) = (int16_t)(S16(e, 4) + 1);
		S16(e, 8) = (int16_t)(S16(e, 8) + S16(e, 0x10));
		S16(e, 0xA) = (int16_t)(S16(e, 0xA) + S16(e, 0x12));
		S16(e, 0xC) = (int16_t)(S16(e, 0xC) + S16(e, 0x14));
		S16(e, 0x10) = (int16_t)(S16(e, 0x10) - (S16(e, 0x10) >> 3));
		S16(e, 0x12) = (int16_t)(S16(e, 0x12) + (S16(e, 0x12) >> 4));
		S16(e, 0x14) = (int16_t)(S16(e, 0x14) - (S16(e, 0x14) >> 3));
		S16(e, 0x18) = (int16_t)(S16(e, 0x18) + S16(e, 0x1A));
		S16(e, 0x1A) = (int16_t)(S16(e, 0x1A) + (S16(e, 0x1A) >> 4));
		S16(e, 0x1C) = (int16_t)(S16(e, 0x1C) + S16(e, 0x1E));
		S16(e, 0x1E) = (int16_t)(S16(e, 0x1E) + (S16(e, 0x1E) >> 4));
	}

	static uint32_t __cdecl DebrisTask(TaskNode *node)
	{
		Node *n = (Node *)node;
		const uint32_t s = P(FieldAlloc(0x68));
		if (!DrawOnly() && n->counter <= 0x2A)
		{
			for (int32_t k = 0; k < 10; k++)
			{
				const uint32_t e = PoolFree(POOL2, POOL2_END);
				if (!e) break;
				U32(e, 0) = 1;
				S16(e, 4) = 0;
				S16(e, 6) = (int16_t)(CrtRand() % 0x800 + 0x100);
				const int32_t a = CrtRand() % 0x1000;
				S32(s, 0x48) = ComputeSin(a);
				S32(s, 0x50) = ComputeCos(a);
				const int32_t r = CrtRand() % 1000 + 0x320;
				S16(e, 8) = (int16_t)(mul32(S32(s, 0x48), r) >> 12);
				const int32_t up = CrtRand() % 300;
				S16(e, 0xA) = (int16_t)(-0x64 - up);
				S16(e, 0xC) = (int16_t)(mul32(r, S32(s, 0x50)) >> 12);
				const int32_t v = CrtRand() % 0x186 + 0x50;
				S16(e, 0x10) = (int16_t)(mul32(S32(s, 0x48), v) >> 12);
				const int32_t vy = CrtRand() % 0x60;
				S16(e, 0x12) = (int16_t)(-4 - vy);
				S16(e, 0x14) = (int16_t)(mul32(v, S32(s, 0x50)) >> 12);
				S16(e, 0x18) = (int16_t)(CrtRand() % 0x1000 - 0x800);
				const int32_t sx = CrtRand() % 0x18 + 0x10;
				S16(e, 0x1A) = (int16_t)sx;
				if (sx & 1) S16(e, 0x1A) = (int16_t)-sx;
				S16(e, 0x1C) = (int16_t)(CrtRand() % 0x1000 - 0x800);
				const int32_t sy = CrtRand() % 0x20 + 0xC;
				S16(e, 0x1E) = (int16_t)sy;
				if (sy & 1) S16(e, 0x1E) = (int16_t)-sy;
			}
		}
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(held_note_task(ORIG_Debris, node, s, nullptr);)
		const uint32_t h = P(FieldAlloc(0x5C));
		U32(h, 0) = MODEL_Debris;
		U32(h, 8) = 0;
		int32_t alive = 0;
		BaseMatrix(s, 0);
		for (uint32_t e = POOL2; e < POOL2_END; e += 0x20)
		{
			if (!(U8(e, 0) & 1)) continue;
			DebrisDraw(s, h, e);
			if (DrawOnly()) continue;
			if (S16(e, 4) >= 0x20)
			{
				U32(e, 0) = 0;
				continue;
			}
			DebrisMove(e);
			alive++;
		}
		FieldFree(0x5C);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		n->counter++;
		return alive ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// single-node parts: a uniform / (s, sy, s) scale about an anchor projected through
	// TransformCameraByShadowRotation; the header fade from the counter; module packets
	// ------------------------------------------------------------------

	// Halo (0x620BD0), 0x168 ticks: the caster's effect bone 0xF0 every tick, scale 0x800 (+ step
	// 0x1800 shrinking by 1/4 from counter 0x158); fades in over 6 ticks, then flickers (0x100 /
	// 0x800 on even / odd counters), full from 0x158; drawn twice
	static int32_t HaloFade(int32_t c)
	{
		if (c < 6) return 0x1000 - c * 0x2AA;
		if (c >= 0x158) return 0;
		return (c & 1) ? 0x800 : 0x100;
	}

	static void HaloDraw(Node *n, const int16_t *pos)
	{
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m = {};
		ComposeZYXRotationMatrix(ang, &m);
		const int32_t sc[3] = { n->scale, n->scale, n->scale };
		Scale3DMatrix(&m, sc);
		TransformCameraByShadowRotation(pos, n->scale, -300);
		GteSetRotMatrix(&m);
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = MODEL_Halo;
		U32(h, 8) = 0;
		SetFade(h, FADE, HaloFade(n->counter));
		FX_HELD(held_fade(h);)
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void HaloMove(Node *n)
	{
		if (n->counter >= 0x158)
		{
			const int16_t step = n->step;
			n->scale = (int16_t)(n->scale + step);
			n->step = (int16_t)(step - step / 4);
		}
		n->counter++;
	}

	static uint32_t __cdecl HaloTask(TaskNode *node)
	{
		Node *n = (Node *)node;
		// the anchor's 4th word is never written (only loaded into the GTE VZ0 pad)
		int16_t pos[4] = { 0, 0, 0, 0 };
		GetEffectSpawnPosition(Caster(), 0xF0, 0x800, pos);
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(held_note_task(ORIG_Halo, node, 0, pos);)
		HaloDraw(n, pos);
		if (DrawOnly()) return 0;
		HaloMove(n);
		return n->counter >= 0x168 ? TASK_END : 0;
	}

	// Flash1 (0x620DD0), 0x1A ticks: the caster's effect bone 0xF0 every tick, scale (s, 0xA0, s),
	// s from 0xC00 growing by a step shrinking by 1/6; fades from counter 0x12
	static uint32_t Flash1Fade(int32_t c, int32_t *fade)
	{
		if (c < 0x12) return NOFADE;
		*fade = (c - 0x12) << 9;
		return FADE;
	}

	static void Flash1Draw(Node *n, const int16_t *pos)
	{
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m = {};
		ComposeZYXRotationMatrix(ang, &m);
		const int32_t sc[3] = { n->scale, n->sy, n->scale };
		Scale3DMatrix(&m, sc);
		TransformCameraByShadowRotation(pos, n->scale, -1000);
		GteSetRotMatrix(&m);
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = MODEL_Halo;
		U32(h, 8) = 0;
		int32_t fade = 0;
		const uint32_t flags = Flash1Fade(n->counter, &fade);
		SetFade(h, flags, fade);
		FX_HELD(held_fade(h);)
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void Flash1Move(Node *n)
	{
		const int16_t step = n->step;
		n->scale = (int16_t)(n->scale + step);
		n->step = (int16_t)(step - step / 6);
		n->counter++;
	}

	static uint32_t __cdecl Flash1Task(TaskNode *node)
	{
		Node *n = (Node *)node;
		int16_t pos[4] = { 0, 0, 0, 0 }; // 4th word never written (GTE VZ0 pad)
		GetEffectSpawnPosition(Caster(), 0xF0, 0x800, pos);
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(held_note_task(ORIG_Flash1, node, 0, pos);)
		Flash1Draw(n, pos);
		if (DrawOnly()) return 0;
		Flash1Move(n);
		return n->counter >= 0x1A ? TASK_END : 0;
	}

	// Flash2 (0x620F60), 0x14 ticks: at the anchor kept when it started (0x24C3270), scale s from
	// 0x255 growing by a step shrinking by 1/5; fades in over 2 ticks, out from counter 0xE
	static uint32_t Flash2Fade(int32_t c, int32_t *fade)
	{
		if (c < 2) *fade = (2 - c) << 11;
		else if (c >= 0xE) *fade = (c - 0xE) * 0x2AA;
		else return NOFADE;
		return FADE;
	}

	static void Flash2Draw(Node *n)
	{
		int16_t ang[3] = { 0, 0, 0 };
		Mat4x3 m = {};
		ComposeZYXRotationMatrix(ang, &m);
		const int32_t sc[3] = { n->scale, n->scale, n->scale };
		Scale3DMatrix(&m, sc);
		TransformCameraByShadowRotation(n->pos, n->scale, -1000);
		GteSetRotMatrix(&m);
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = MODEL_Flash2;
		U32(h, 8) = 0;
		int32_t fade = 0;
		const uint32_t flags = Flash2Fade(n->counter, &fade);
		SetFade(h, flags, fade);
		FX_HELD(held_fade(h);)
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void Flash2Move(Node *n)
	{
		const int16_t step = n->step;
		n->scale = (int16_t)(n->scale + step);
		n->step = (int16_t)(step - step / 5);
		n->counter++;
	}

	static uint32_t __cdecl Flash2Task(TaskNode *node)
	{
		Node *n = (Node *)node;
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(held_note_task(ORIG_Flash2, node, 0, nullptr);)
		Flash2Draw(n);
		if (DrawOnly()) return 0;
		Flash2Move(n);
		return n->counter >= 0x14 ? TASK_END : 0;
	}

	// Ring (0x621110), 0x4F ticks: at the caster's effect bone 0xF1 (taken when it started), z angle
	// turning 0x10 a tick, scale from 0x680 growing by a step shrinking by 1/6 for 16 ticks, then
	// by step / 64; fades from counter 0x3F
	static uint32_t RingFade(int32_t c, int32_t *fade)
	{
		if (c < 0x3F) return NOFADE;
		*fade = (c - 0x3F) << 8;
		return FADE;
	}

	static void RingDraw(Node *n)
	{
		int16_t ang[3] = { 0, 0, n->angle };
		Mat4x3 m = {};
		ComposeZYXRotationMatrix(ang, &m);
		const int32_t sc[3] = { n->scale, n->scale, n->scale };
		Scale3DMatrix(&m, sc);
		TransformCameraByShadowRotation(n->pos, 0, 0x7D0);
		GteSetRotMatrix(&m);
		const uint32_t h = P(FieldAlloc(0x58));
		U32(h, 0) = MODEL_Ring;
		U32(h, 8) = 0;
		int32_t fade = 0;
		const uint32_t flags = RingFade(n->counter, &fade);
		SetFade(h, flags, fade);
		FX_HELD(held_fade(h);)
		PacketCursor() = RenderPrimModel((void *)h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void RingMove(Node *n)
	{
		n->angle = (int16_t)(n->angle + n->spin);
		const int16_t step = n->step;
		if (n->counter < 0x10)
		{
			n->scale = (int16_t)(n->scale + step);
			n->step = (int16_t)(step - step / 6);
		}
		else n->scale = (int16_t)(n->scale + (step >> 6));
		n->counter++;
	}

	static uint32_t __cdecl RingTask(TaskNode *node)
	{
		Node *n = (Node *)node;
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(held_note_task(ORIG_Ring, node, 0, nullptr);)
		RingDraw(n);
		if (DrawOnly()) return 0;
		RingMove(n);
		return n->counter >= 0x4F ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// small tasks
	// ------------------------------------------------------------------

	// FeedbackA (0x6200F0): white screen feedback every tick (draw-only ticks too) for e ticks
	static uint32_t __cdecl FeedbackA(TaskNode *node)
	{
		Node *n = (Node *)node;
		ScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F);
		if (DrawOnly()) return 0;
		n->counter++;
		return n->counter >= n->e ? TASK_END : 0;
	}

	// FeedbackB (0x620140): the same from its second tick
	static uint32_t __cdecl FeedbackB(TaskNode *node)
	{
		Node *n = (Node *)node;
		if (n->counter > 0) ScreenFeedback(0, 0xFF, 0xFF, 0xFF, 0x3F);
		if (DrawOnly()) return 0;
		n->counter++;
		return n->counter >= n->e ? TASK_END : 0;
	}

	// MAG_076_sub_6200A0: a feedback task for `ticks` ticks (kind 0: FeedbackA, else FeedbackB)
	static void AddFeedbackTask(int32_t kind, int16_t ticks)
	{
		Node *n = (Node *)AddTaskToQueue(QMaster(), kind ? ORIG_FeedbackB : ORIG_FeedbackA);
		n->counter = 0;
		n->e = ticks;
	}

	// TexRestore (0x622560): the battle textures restored (anim-seq task 0x508630 at counter 1, done
	// flag +0xE); ends when it is done
	static uint32_t __cdecl TexRestoreTask(TaskNode *node)
	{
		Node *n = (Node *)node;
		if (n->counter == 1) TextureRestoreTask((void *)(TexBase() + 0x18000), &n->e);
		const int16_t done = n->e;
		n->counter++;
		return done ? TASK_END : 0;
	}

	// Wait (0x61FC90): 0x32 ticks, then the Master
	static uint32_t __cdecl WaitTask(TaskNode *node)
	{
		Node *n = (Node *)node;
		if (DrawOnly()) return 0;
		n->counter++;
		if (n->counter < 0x32) return 0;
		AddMasterTask(ORIG_Master);
		return TASK_END;
	}

	// MAG_076_sub_61FC60: the ambient colour word 0xB8B9A8 = c, and every entity with flag 2 gets it
	static void SetAmbient(uint32_t c)
	{
		var<uint32_t>(0xB8B9A8) = c;
		for (int slot = 0; slot < 7; slot++)
			if (*Entity(slot) & 2) *(uint32_t *)(Entity(slot) + 0x2C) = c;
	}

	// ------------------------------------------------------------------
	// Intro (0x61FA80)
	// ------------------------------------------------------------------
	static uint32_t __cdecl IntroTask(TaskNode *node)
	{
		Node *n = (Node *)node;
		if (UpdateFlags() & 0x201)
		{
			if (UpdateFlags() & 1) return 0;
			if (PauseQuery() < 0) return 0;
		}
		if (n->counter == 0)
		{
			uint8_t *c = Caster();
			var<uint32_t>(SAVED_XY) = *(const uint32_t *)(c + 0x1C);
			var<uint32_t>(SAVED_Z) = *(const uint32_t *)(c + 0x20);
			SetScreenFlash(0x1000, 0);
			*(int16_t *)(Caster() + 0x1E) = 0x1388;
			*Caster() |= 4;
			Sub4A8480(0);
			QueueFanfare(FANFARE, &FanfareDone());
		}
		if (FanfareDone() == 0 && n->counter != 0) return 0;
		if (n->counter == 0xA) LoadBattleModel(1, TexBase());
		if (n->counter == 0x26)
		{
			if (PauseQuery() < 0) return 0;
			SwapBattleModel(1, TexBase());
			SetAmbient(var<uint32_t>(0xB8B9A8));
		}
		if (n->counter == 0x27)
		{
			char name[0x100];
			const char *dir = (const char *)WAV_DIR;
			const size_t len = strlen(dir);
			memcpy(name, dir, len + 1);
			memcpy(name + len, (const void *)WAV_NAME, 10);
			WavStreamPlay(0, name, 0x7F, 0x40, 0x64, 0);
		}
		n->counter++;
		if (n->counter < 0x28) return 0;
		AddMasterTask(ORIG_Wait);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Master (0x61FCD0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *node)
	{
		Node *n = (Node *)node;
		ComposeAffineTransform(&Camera(), M(CASTER_FRAME), M(CASTER_VIEW));
		if (DrawOnly()) return 0;
		if (n->counter == 0)
		{
			CameraScriptStart(CAMERA_SCRIPT, CASTER_FRAME, Caster(), QMaster(), 0);
			ArmCameraReturn();
			Voice() = ClaimVoiceSlot(SOUND_Voice, 1, 0x80);
		}
		int32_t c = n->counter;
		if (c < 0x28)
		{
		}
		else if ((c -= 0x28) < 0x32)
		{
			if (c == 0)
			{
				ChainTransformation(Caster(), 0xF);
				*(uint16_t *)Caster() &= 0xFFFB;
				AddMasterTask(ORIG_Burst);
			}
			else
			{
				if (c >= 0x22) SetScreenFlash((uint32_t)(shl32(0x22 - c, 12) / 16 + 0x1000), 0);
				if (c == 0xA) AddMasterTask(ORIG_Spin);
			}
		}
		else if ((c -= 0x32) < 0x5A)
		{
			if (c == 0x46) ChainTransformation(Caster(), 0x10);
			*(int16_t *)(Caster() + 0x1E) = (int16_t)(*(int16_t *)(Caster() + 0x1E) - 0x37);
			if (c == 1) AddMasterTask(ORIG_Rise);
			else if (c == 0x3C)
			{
				Node *h = (Node *)AddTaskToQueue(QMaster(), ORIG_Halo); // au_re_BdLinkTask_60
				h->counter = 0;
				h->scale = 0x800;
				h->step = 0x1800;
			}
		}
		else if ((c -= 0x5A) < 0x3C)
		{
			if (c == 0) *(int16_t *)(Caster() + 0x1E) = SavedY();
		}
		else if ((c -= 0x3C) < 0x10E)
		{
			if (c == 0x1E)
			{
				MusicChannelVolume(0, 0x7F);
				BattleMessage(CasterSlot(), 1, 0x3C);
			}
			else if (c == 0x6E) BattleMessage(CasterSlot(), 3, 0x3C);
			else if (c == 0xBE) BattleMessage(CasterSlot(), 4, 0x3C);
			else if (c == 0xEF)
			{
				// MAG_076_sub_620D60
				GetEffectSpawnPosition(Caster(), 0xF0, 0x800, (int16_t *)SPAWN_POS);
				Node *f = (Node *)AddTaskToQueue(QMaster(), ORIG_Flash1);
				memcpy(f->pos, (const void *)SPAWN_POS, 8);
				f->counter = 0;
				f->step = 0xC00;
				f->scale = 0xC00;
				f->sy = 0xA0;
			}
			else if (c == 0xF3)
			{
				// au_re_BdLinkTask_61
				Node *f = (Node *)AddTaskToQueue(QMaster(), ORIG_Flash2);
				memcpy(f->pos, (const void *)SPAWN_POS, 8);
				f->counter = 0;
				f->step = 0x255;
				f->scale = 0x255;
			}
			else if (c == 0x106) ScreenFadeTask(7, 6, 0xC, 0xFF);
		}
		else if ((c -= 0x10E) < 0x5F)
		{
			if (c == 1) BdPlaySE(SOUND_Hit, 0, 0x80);
			else if (c == 0) ChainTransformation(Caster(), 0x11);
			else if (c == 8)
			{
				// MAG_076_sub_6210A0
				Node *r = (Node *)AddTaskToQueue(QMaster(), ORIG_Ring);
				r->counter = 0;
				GetEffectSpawnPosition(Caster(), 0xF1, 0, r->pos);
				r->angle = (int16_t)(CrtRand() % 0x800);
				r->spin = 0x10;
				r->step = 0x680;
				r->scale = 0x680;
				AddMasterTask(ORIG_Burst2);
			}
			else if (c == 6) AddMasterTask(ORIG_Debris);
			else if (c == 5)
			{
				AddFeedbackTask(1, 0x5F);
				AddMasterTask(ORIG_TexRestore);
			}
		}
		else if ((c -= 0x5F) < 0x3C)
		{
			if (c == 0) ApplyActionResultToTarget(Ctx()->actions[0].targets);
		}
		if (n->counter == 0x296 && VoiceBusy(Voice())) VoiceRelease(Voice());
		n->counter++;
		if (n->counter <= 0x299) return 0;
		SetScreenFlash(0, 0);
		memcpy((void *)0xB8B800, (const void *)0xB8B7F0, 0x10);
		var<int16_t>(0x1D977A0) = var<int16_t>(0x1D8E038);
		Sub4A8480(1);
		var<uint32_t>(0x1D96A9C) &= ~0x400u;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Root task (0x6225A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *node)
	{
		RootNode *r = (RootNode *)node;
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(held_note_root();)
		PacketCursor() = (r->counter & 1) ? TexBase() + 0xC000 : TexBase();
		const int a = ExecuteTaskQueue(QMaster());
		r->counter++;
		return a ? 0 : TASK_END;
	}
}

	void register_mag076_ultimecia_final_form_spawn()
	{
		register_port(uff076::ORIG_RootTask, (void *)uff076::RootTask, "U076 RootTask", 76);
		register_port(uff076::ORIG_Intro, (void *)uff076::IntroTask, "U076 Intro", 76);
		register_port(uff076::ORIG_Wait, (void *)uff076::WaitTask, "U076 Wait", 76);
		register_port(uff076::ORIG_Master, (void *)uff076::MasterTask, "U076 Master", 76);
		register_port(uff076::ORIG_FeedbackA, (void *)uff076::FeedbackA, "U076 FeedbackA", 76);
		register_port(uff076::ORIG_FeedbackB, (void *)uff076::FeedbackB, "U076 FeedbackB", 76);
		register_port(uff076::ORIG_Burst, (void *)uff076::BurstTask, "U076 Burst", 76);
		register_port(uff076::ORIG_Spin, (void *)uff076::SpinTask, "U076 Spin", 76);
		register_port(uff076::ORIG_Rise, (void *)uff076::RiseTask, "U076 Rise", 76);
		register_port(uff076::ORIG_Halo, (void *)uff076::HaloTask, "U076 Halo", 76);
		register_port(uff076::ORIG_Flash1, (void *)uff076::Flash1Task, "U076 Flash1", 76);
		register_port(uff076::ORIG_Flash2, (void *)uff076::Flash2Task, "U076 Flash2", 76);
		register_port(uff076::ORIG_Ring, (void *)uff076::RingTask, "U076 Ring", 76);
		register_port(uff076::ORIG_Burst2, (void *)uff076::Burst2Task, "U076 Burst2", 76);
		register_port(uff076::ORIG_Debris, (void *)uff076::DebrisTask, "U076 Debris", 76);
		register_port(uff076::ORIG_TexRestore, (void *)uff076::TexRestoreTask, "U076 TexRestore", 76);
		// 30 fps layer: see mag076_ultimecia_final_form_spawn_held.inc
		FX_HELD(register_mag076_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag076_ultimecia_final_form_spawn_held.inc"
#endif
