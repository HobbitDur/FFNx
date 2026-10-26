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

// Effect 89: Wishing Star (kernel.bin Rinoa limit break part 2 {Combine} Wishing Star, section 25 entry 3, effect id +2 = 89;
// MAG_089_WISHING_STAR): Rinoa rides Angelo up into a starry sky, the screen shatters, Angelo dives through the targets eight
// times (eight damage waves) under a rain of sparkles, and both come back. A standalone hand-written module (not the act library,
// not a cinematic clone) 0x6F9CF0..0x6FE040 (41 functions in IDA, no missing / bogus start; the file loader is the shared null
// loader 0x5755E0). Ported: the six tasks and every helper; the setup (0x6F9CF0 -> 0x6F9D00) is not ported. Four helpers are
// copies of other modules' functions (0x6FA270 = 70 / 212 / 260's entity-flag save, 0x6FA2A0 = 65 / 70's flag 4 on the party,
// 0x6FBE00 / 0x6FBE40 = 65 / 212 / 260's flag-4 restore); the star dots 0x6FCD50 / 0x6FCF70 are 65's 0x6E4350 / 0x6E4570 on
// this module's tables. It calls 65's 0x6E2AC0 (screen feedback) and 0x6E2C10 (camera blend) and 65 / 70 / 260 call its
// 0x6FC9E0 (flipbook) and 0x6FC250 (camera keys) through their original addresses.
//
// Setup 0x6F9D00 (runs once, not ported): queues the TIMs 0x14AF700 / 0x14B7B20, keeps the magic buffer 0x2556600, the cast
// context 0x2556584 and the CASTER's entity 0x2556588 (Rinoa), starts the root task in the root queue 0x25565C8 (one 0x14-byte
// node at 0x25565D8).
//   RootTask (0x6F9D80) - alternates the module packet arena (0x25565EC = magic buffer + 0x1A88C / + 0x2A88C, end 0x25565F0);
//     on counter 2 saves the seven entities' flag words (0x6FA270), sets up the three pools (director 1 x 0x41C at + 0x5A40,
//     queue B 0x20 x 0x30 at + 0xC40, queue C 8 x 0x188 at the magic buffer) and starts the director: the targets' box
//     (centre x / y, max z) of the first wave's targets, Rinoa's position / angles kept, flag 4 on the party, Rinoa's external
//     animation set bound (0x5022C0, chain transformation 1), Angelo's model container bound (0x6EC060, at the director + 0x34),
//     Rinoa and Angelo put at the centre (entity +0x1C..+0x20), the particle / sprite / shard tables reset, the screen shards
//     built from model 0x1496550 (0x6FA3E0), the stream loader reset (+ 0x5E5C) and the 8 x 128 star dots seeded (+ 0x7C04,
//     groups 0x14AF490). Then each tick the director queue, queue B, queue C; ends when the director ends.
//   Director (0x6FA780) - node 0x41C: counter +0xC, targets +0x14, Angelo's container +0x34 (+0x40 matrix, +0x60 animation),
//     Rinoa's saved position +0xD0 / angles +0xE0, centre +0xD8, layouts +0x118 / +0x330. A 0x10F-tick timeline:
//       0..0x15    Rinoa (and Angelo) rise from the centre (entity +0x20, +0x5C), flag 4 on the enemies at 0; from 3 a flash
//                  and debris (0x6FD1B0, queue B) at Angelo's / Rinoa's bones every few ticks; camera keys 0..3, 4..0x14
//       0x16..0x31 they keep rising; queue B reset at 0x16 (enemy flags restored); 0x1E..0x21 Rinoa turns (+0x1E)
//       0x20..0x23 a layout at Angelo's bone 0x13; 0x24 the screen capture (0x6FBEE0) into + 0x14404, white fade, then the
//                  shards fall (0x6FD100 / 0x6FCFD0) until 0x4A; screen feedback (65's 0x6E2AC0) 0x24..0xE1
//       0x25..0xE1 the starry sky: layout +0x330 and the star dots (0x6FCD50, twinkling 0x6FCF70)
//       0x4B..0x5E a layout at the centre; 0x57..0x65 a growing prim model (0x1497140); 0x66..0x93 seven dives (queue C
//                  0x6FD580: Angelo's trail, sparks and particle models, a damage wave each at its tick 6)
//       0xA2..0xB0 the landing: three prim models at Angelo's bone 0x13; 0xB1..0xE2 the last dive (0x6FDB80 -> 0x6FDC20),
//                  the eighth damage wave at 0xB4, a layout at the centre
//       0xC5..0x102 white fade; 0xE3 Rinoa / the camera / the party flags put back, bones rebuilt; 0xEB..0x102 falling sparks
//       0x10F      Rinoa's angles back; ends when the stream loader is idle (else the counter goes to 0x11F and waits)
//     Stream loads (0x5341D0) of files 0x2F6..0x2FC / 0x168..0x16A into + 0xBC04 with waits at 0x24 / 0x65 / 0xAD (the
//     director returns without a step while the loader is busy), TIM uploads at 0x4A / 0xAD; sounds at 0 / 0x32 / 0xA2.
//     Camera: armed return at 0 (0x50A730), camera keys (0x6FC250) 0..3 / 0x15..0x22 / 0x31..0x49 / 0x5F..0x6D, Rinoa-relative
//     camera 4..0x14, eye turned about the look-at 0x65..0xA0 (0x6FC300), fixed at 0xA1, blended 0xB0..0xE1 (65's 0x6E2C10),
//     put back at 0xE3. Particles drawn every tick by flag (0x255658E: models 0x6FC500, 0x255658D: sprites 0x6FC8B0) and
//     updated (0x6FCCF0 / 0x6FC750).
//   Queue B: FlashTask (0x6FD340, flipbook 0x14A0538, 12 frames) and DebrisTask (0x6FD3C0, prim model 0x14A04D8, 15 ticks).
//   Queue C: DiveTask (0x6FD580) / LastDiveTask (0x6FDC20): a prim model and particles along Angelo's dive; the dive's layout.
// Prim-player callback 0x6FBB80 (blended vertex frames, flag 0x200 = no rotation of the offset, 0x4000 = blend mode 0).
// The effect WRITES ENTITY POSITIONS: Rinoa's +0x1C..+0x20 at the start and at 0xE3 (put back), +0x20 / +0x5C every tick of
// 0..0x31 and 0xA2..0xB0, +0x1E / +0x58 0x1E..0x21 and 0xA2; Rinoa's angles +0xC..+0x10 (+0xE = 0 at the start, put back at
// 0x10F); entity flags (bit 2 / 3 of Rinoa, bit 2 of the party / enemies, put back) and bit 2 of the enemies drawn by the
// capture. It WRITES THE BATTLE CAMERA: eye 0xB8B7F0 / look-at 0xB8B7F8, 0x1D977A2 and 0x1D8E038 (see above).
// Module globals: 0x2556584..0x2556624. Pools, arenas, particle tables in the magic buffer; static tables 0x14AF460..0x14AF700
// (the star groups 0x14AF490..0x14AF510 are WRITTEN: angles reset and turned).

#include "mag_common.h"
#include <intrin.h>

namespace ff8fx
{
namespace ws089
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Ctx() { return var<uint8_t *>(0x2556584); }               // cast context (g_MagicCastContext)
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2556588); }            // Rinoa's entity
	inline uint8_t &SpritesOn() { return var<uint8_t>(0x255658D); }            // sprites drawn (0x6FC8B0)
	inline uint8_t &ModelsOn() { return var<uint8_t>(0x255658E); }             // particle models drawn (0x6FC500)
	inline uint8_t *&SpriteCursor() { return var<uint8_t *>(0x2556590); }      // sprite alloc cursor
	inline uint8_t *&PartCursor() { return var<uint8_t *>(0x2556594); }        // particle model alloc cursor
	inline TaskQueue *QC() { return (TaskQueue *)0x2556598; }                  // dives: magic buffer, 8 x 0x188
	inline TaskQueue *QB() { return (TaskQueue *)0x25565A8; }                  // flashes / debris: + 0xC40, 0x20 x 0x30
	inline TaskQueue *QA() { return (TaskQueue *)0x25565B8; }                  // director: + 0x5A40, 1 x 0x41C
	inline uint32_t &Cursor() { return var<uint32_t>(0x25565EC); }             // module packet arena cursor
	inline uint32_t &CursorEnd() { return var<uint32_t>(0x25565F0); }          // its end
	inline int16_t *Centre() { return (int16_t *)0x25565F8; }                  // targets' centre x, y, (max) z
	inline uint32_t &TexBase() { return var<uint32_t>(0x2556600); }            // Magic_TextureOFF (magic buffer)
	inline uint32_t *SavedFlags() { return (uint32_t *)0x2556604; }            // 7 entities' flag words
	inline uint8_t *&TargetCursor() { return var<uint8_t *>(0x2556620); }      // next damage wave's target records
	inline uint32_t &FrameArena() { return var<uint32_t>(0x1D8E054); }         // battle_texture_data_ptr (frame packet arena)
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }

	static const uint32_t ORIG_RootTask = 0x6F9D80;
	static const uint32_t ORIG_Director = 0x6FA780;
	static const uint32_t ORIG_FlashTask = 0x6FD340;
	static const uint32_t ORIG_DebrisTask = 0x6FD3C0;
	static const uint32_t ORIG_DiveTask = 0x6FD580;
	static const uint32_t ORIG_LastDiveTask = 0x6FDC20;
	static const uint32_t ORIG_PartCallback = 0x6FBB80;

	static const uint32_t STAR_GROUPS = 0x14AF490;   // 8 x 16: colour, colour 2, angle steps (+8), angles (+0xC)
	static const uint32_t PART_KINDS = 0x14AF510;    // particle model kinds, 16 bytes: model, far colour, sprite at death, steps
	static const uint32_t SQRT_TABLE = 0x14AF4E0;    // SquareRoot0 table (index 0x40..0xFF)
	static const uint32_t CAPTURE_MODELS = 0x14AF6E0; // 4 x (OT index, mode) of the stage models in the capture
	static const uint32_t SEQ_Spark = 0x14A067C;     // falling sparks
	static const uint32_t SEQ_Sparkle = 0x14A07F4;   // dive / particle sparkles
	static const uint32_t SEQ_Flash = 0x14A0538;
	static const uint32_t MDL_Debris = 0x14A04D8;
	static const uint32_t MDL_Trail = 0x1497140;
	static const uint32_t MDL_Shards = 0x1496550;
	static const uint32_t LAYOUT_Dive = 0x14A3EB0;

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotY(int32_t angle, void *out) { fn<void (__cdecl *)(int32_t, void *)>(0x701270)(angle, out); }    // Effect_RotationYMatrix
	inline void RotX(int32_t angle, void *out) { fn<void (__cdecl *)(int32_t, void *)>(0x701220)(angle, out); }    // MAG_063_RotationXMatrix
	inline void RotZ(int32_t angle, void *out) { fn<void (__cdecl *)(int32_t, void *)>(0x7012C0)(angle, out); }    // MAG_065_RotationZMatrix
	inline void RotationFromAngles(const int16_t *angles, void *out) { fn<void (__cdecl *)(const int16_t *, void *)>(0x701310)(angles, out); }  // MAG_017_sub_701310
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline void MatrixMultiply(const void *a, void *b) { fn<void (__cdecl *)(const void *, void *)>(0x56C270)(a, b); }       // GTE_MatrixMultiply
	inline void MatMul3(const void *a, const void *b, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x56C090)(a, b, out); } // Matrix3x3_MultiplyPSX (pad not written)
	inline void MatVec(const void *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const void *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void TransformVec3(const void *m, const int32_t *in, int32_t *out) { fn<void (__cdecl *)(const void *, const int32_t *, int32_t *)>(0x56C600)(m, in, out); } // TransformVectorBy3x3Matrix
	inline int32_t ISqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void DirectionFromAngles(int32_t pitch, int32_t yaw, int16_t *out) { fn<void (__cdecl *)(int32_t, int32_t, int16_t *)>(0x6A6020)(pitch, yaw, out); } // Effect_DirectionFromPitchYaw
	inline uint32_t RenderPrimSet(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x701DD0)(h, ot, mode, cursor); } // MAG_063_RenderPrimSet
	inline uint32_t Tile(int32_t r, int32_t g, int32_t b, int32_t level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x7040B0)(r, g, b, level, cursor); } // Effect_FullScreenTile
	inline void QueueChainTransformation(uint8_t *entity, int32_t a) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, a); }
	inline uint32_t BindExternalAnimSet(uint8_t *entity, void *out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, void *, uint32_t)>(0x5022C0)(entity, out, data); } // BattleEntity_BindExternalAnimSet
	inline void BindModelContainer(void *c, void *sections, uint32_t data) { fn<void (__cdecl *)(void *, void *, uint32_t)>(0x6EC060)(c, sections, data); } // Effect_BindModelContainerSections
	inline void SetAnim(void *c, int32_t id) { fn<void (__cdecl *)(void *, int32_t)>(0x6574D0)(c, id); } // au_re_Battle_ReadAnimation_6
	inline uint32_t DrawCreatureModel(void *c, uint32_t buf, uint32_t cursor, uint32_t end) { return fn<uint32_t (__cdecl *)(void *, uint32_t, uint32_t, uint32_t)>(0x6A79B0)(c, buf, cursor, end); } // Effect_DrawCreatureModel
	inline void GetEffectSpawnPosition(void *c, int32_t bone, int32_t a, int16_t *out) { fn<void (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(c, bone, a, out); }
	inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }    // BS_Camera_ArmReturnAfterEffect
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline int32_t StreamBusy() { return fn<int32_t (__cdecl *)()>(0x534270)(); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamPending(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x534300)(a); } // au_re_BattleFile_preLoad_1
	inline void QueueTIMUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(0x505E30)(tim); } // Battle_QueueTIMUpload_GetEOF
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
	inline void ClearOrderingTable(uint32_t ot, int32_t n) { fn<void (__cdecl *)(uint32_t, int32_t)>(0x45D530)(ot, n); }
	inline void GetDrawEnv(void *env) { fn<void (__cdecl *)(void *)>(0x45B640)(env); }
	inline void SetDefDrawEnv(void *env, int32_t x, int32_t y, int32_t w, int32_t h) { fn<void (__cdecl *)(void *, int32_t, int32_t, int32_t, int32_t)>(0x45C2A0)(env, x, y, w, h); }
	inline void SetDrawEnv(void *packet, const void *env) { fn<void (__cdecl *)(void *, const void *)>(0x45C0F0)(packet, env); }
	inline void SetDrawMode(void *packet, int32_t dfe, int32_t dtd, int32_t tpage, const void *rect) { fn<void (__cdecl *)(void *, int32_t, int32_t, int32_t, const void *)>(0x45BFC0)(packet, dfe, dtd, tpage, rect); }
	inline uint32_t RenderGeometry(uint32_t group, void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(group, h, ot, mode, cursor); }
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	inline int32_t BattleReadAnimation(void *hdr, void *cmd) { return fn<int32_t (__cdecl *)(void *, void *)>(0x508F90)(hdr, cmd); }
	inline void PreBattleReadAnimation(void *hdr, void *cmd, uint32_t anim) { fn<void (__cdecl *)(void *, void *, uint32_t)>(0x509440)(hdr, cmd, anim); }
	inline void ScreenFeedback() { fn<void (__cdecl *)()>(0x6E2AC0)(); }     // MAG_065_GreatAttractor_ScreenFeedback
	// MAG_065_GreatAttractor_CameraBlend: look-at / eye between two (look-at, eye) keys by t (4.12), 0x1D977A2 from za to zb
	inline void CameraBlend(const int16_t *k0, const int16_t *k1, int32_t za, int32_t zb, int32_t t) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int32_t)>(0x6E2C10)(k0, k1, za, zb, t); }
	// software GTE
	inline void GteLoadIRBytes(const void *src) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(src); }   // IR1..3 = 3 x u8
	inline void GteStoreIRBytes(void *dst) { fn<void (__cdecl *)(void *)>(0x45E410)(dst); }        // IR1..3 low bytes
	inline void GteLoadIRColumn(const void *m) { fn<void (__cdecl *)(const void *)>(0x45E180)(m); }       // GTE_LoadIRFromMatrixColumn
	inline void GteStoreIRColumn(void *m) { fn<void (__cdecl *)(void *)>(0x45E470)(m); }           // GTE_StoreIRToMatrixColumn
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }
	inline void GteLoadV0Dwords(const void *v) { fn<void (__cdecl *)(const void *)>(0x45E060)(v); }       // GTE_LoadV0FromDwords
	inline void GteLoadV0u16(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); }          // GTE_LoadV0_u16
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }  // GTE_ReadSXY012_Split
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }       // GTE_ReadOTZ (dword)
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); }
	inline void GteLoadRGBC(const void *src) { fn<void (__cdecl *)(const void *)>(0x45E110)(src); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }

	// the compiler's magic-number divisions, step by step
	inline uint32_t mulhi_u(uint32_t a, uint32_t b) { return (uint32_t)(((uint64_t)a * b) >> 32); }
	inline int32_t mulhi_s(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 32); }
	// cdq; and edx, m; add; sar n (C signed division by 2^n)
	inline int32_t sdiv_pow2(int32_t v, int n) { return (int32_t)((uint32_t)v + ((uint32_t)(v >> 31) & ((1u << n) - 1))) >> n; }
	inline int32_t div6(int32_t v) { const int32_t h = mulhi_s(v, 0x2AAAAAAB); return h + (int32_t)((uint32_t)h >> 31); }    // v / 6 (trunc)
	inline int32_t div80(int32_t v) { const int32_t h = mulhi_s(v, 0x66666667) >> 5; return h + (int32_t)((uint32_t)h >> 31); } // v / 80 (trunc)
	inline int32_t div3(int32_t v) { const int32_t h = mulhi_s(v, 0x55555556); return h + (int32_t)((uint32_t)h >> 31); }     // v / 3 (trunc)
	template<typename T> static inline T &F(void *p, uint32_t off) { return *(T *)((uint8_t *)p + off); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x25565D8)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up, director started
		uint32_t parity;   // +0x10 packet arena parity
	};
	struct DirectorNode // pool of 1 node of 0x41C bytes (magic buffer + 0x5A40)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E[6];
		uint8_t targets[0x20];  // +0x14 the first wave's target entities, 8 bytes each (entity, unused)
		uint8_t angelo[0x9C];   // +0x34 Angelo's model container (+0x1C position, +0x40 matrix, +0x60 animation)
		uint32_t rinoa_xy; // +0xD0 Rinoa's entity +0x1C at the start
		uint32_t rinoa_z;  // +0xD4 Rinoa's entity +0x20
		int16_t centre[3]; // +0xD8 targets' centre x, y and max z
		int16_t padDE;
		uint32_t rinoa_ang0; // +0xE0 Rinoa's entity +0xC at the start
		uint32_t rinoa_ang1; // +0xE4 Rinoa's entity +0x10
		uint8_t sections[0x10]; // +0xE8 Angelo's sections (0x6EC060)
		uint8_t animset[0x20];  // +0xF8 Rinoa's external animation set (0x5022C0)
		uint8_t layout[0x218];  // +0x118 prim-model layout (0x68 / 0x70 / 0x218 bytes decoded)
		uint8_t layout2[0xEC];  // +0x330 the sky's layout
	};
	struct SmallNode // queue B pool: 0x20 nodes of 0x30 bytes (magic buffer + 0xC40)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t size;      // +0x12 (0x1000)
		int16_t vel[3];    // +0x14 debris velocity
		int8_t ang[2];     // +0x1A debris angles (x 16)
		int8_t gravity;    // +0x1C
		int8_t dang[2];    // +0x1D
		int8_t frame;      // +0x1F flash frame / debris life
		uint8_t pad20[0x10];
	};
	struct DiveNode // queue C pool: 8 nodes of 0x188 bytes (magic buffer)
	{
		TaskNode hdr;
		Mat4x3 m;          // +0x0C direction (from angles), translation +0x20 = position
		int16_t counter;   // +0x2C
		int16_t pad2E;
		uint8_t layout[0x158]; // +0x30
	};
	// the prim-player callback's parameter block (a stack block of the caller)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 anchor matrix
		uint32_t cursor;   // +0x20 packet cursor
		int16_t depth;     // +0x24 prim header +0x18
		int16_t pad26;
		uint32_t morph;    // +0x28 blended vertex frames (magic buffer + 0x1888C)
	};
	// particle tables in the magic buffer
	struct Sprite // + 0x1240, 0x200 x 0x18
	{
		uint32_t seq;      // +0x00 flipbook
		int16_t pos[3];    // +0x04
		int16_t life;      // +0x0A
		int16_t vel[3];    // +0x0C
		int16_t scale;     // +0x12
		int8_t acc[3];     // +0x14 (z: not accumulated)
		uint8_t pad17;
	};
	struct PModel // particle model, + 0x4240, 0x80 x 0x30
	{
		uint8_t life;      // +0x00
		uint8_t kind;      // +0x01 PART_KINDS index
		int16_t fade;      // +0x02 depth cue (IR0)
		int16_t scale[3];  // +0x04 diagonal scale
		int16_t vel[3];    // +0x0A
		int16_t m[9];      // +0x10 rotation
		int16_t pad22;
		int32_t pos[3];    // +0x24
	};
	struct Shard // + 0x5EA4, 0x78 x 0x34: a triangle of the captured screen
	{
		int16_t pos[3];    // +0x00 centroid (x, y on screen)
		int16_t life;      // +0x06 (0x10 / 0: gone)
		int16_t vel[3];    // +0x08
		int16_t len;       // +0x0E distance from the centre
		uint32_t rot;      // +0x10 its spin matrix (+ 0x7704 + 0x14 n)
		int16_t v[3][3];   // +0x14 vertices relative to the centroid
		uint8_t pad26[2];
		uint8_t uv0[2];    // +0x28
		uint16_t clut;     // +0x2A
		uint8_t uv1[2];    // +0x2C
		uint16_t tpage;    // +0x2E
		uint8_t uv2[2];    // +0x30
		uint8_t pad32[2];
	};
	// DRAWENV
	struct DrawEnv
	{
		int16_t clip[4];   // +0x00
		int16_t ofs[2];    // +0x08
		int16_t tw[4];     // +0x0C
		uint16_t tpage;    // +0x14
		uint8_t dtd, dfe;  // +0x16
		uint8_t isbg, r0, g0, b0; // +0x18
		uint8_t dr_env[0x40]; // +0x1C
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(DirectorNode) == 0x41C && sizeof(SmallNode) == 0x30 && sizeof(DiveNode) == 0x188, "Wishing Star 089 nodes");
	static_assert(sizeof(PrimArg) == 0x2C && sizeof(Sprite) == 0x18 && sizeof(PModel) == 0x30 && sizeof(Shard) == 0x34 && sizeof(DrawEnv) == 0x5C, "Wishing Star 089 blocks");
}
}

#ifdef FF8_FX_HELD
#include "mag089_wishing_star_held.h"
#endif

namespace ff8fx
{
namespace ws089
{
	using namespace eng;
	using namespace magc;

	inline Sprite *Sprites() { return (Sprite *)(TexBase() + 0x1240); }
	inline PModel *PModels() { return (PModel *)(TexBase() + 0x4240); }
	inline Shard *Shards() { return (Shard *)(TexBase() + 0x5EA4); }

	// ------------------------------------------------------------------
	// Entity flags
	// ------------------------------------------------------------------
	// 0x6FA270: the seven entities' flag words saved
	static void SaveEntityFlags()
	{
		uint32_t *dst = SavedFlags();
		for (uint32_t e = 0x1D972C0; e < 0x1D97704; e += 0x9C) *dst++ = *(const uint16_t *)e;
	}

	// 0x6FA2A0: flag 4 on the party entities with flag 2
	static void PartyFlag4On()
	{
		for (uint32_t e = 0x1D972C0; e < 0x1D97494; e += 0x9C)
		{
			const uint16_t f = *(const uint16_t *)e;
			if (f & 2) *(uint16_t *)e = (uint16_t)(f | 4);
		}
	}

	// 0x6FBDE0: flag 4 on the enemy entities (slots 3..6) with flag 2
	static void EnemyFlag4On()
	{
		for (uint32_t e = 0x1D97494; e < 0x1D97704; e += 0x9C)
		{
			const uint16_t f = *(const uint16_t *)e;
			if (f & 2) *(uint16_t *)e = (uint16_t)(f | 4);
		}
	}

	// flag 4 of the entities with flag 2 back to the saved value
	static void RestoreFlag4(uint32_t e, const uint32_t *src, const uint32_t *end)
	{
		for (; src < end; e += 0x9C, src++)
		{
			const uint16_t f = *(const uint16_t *)e;
			if (f & 2) *(uint16_t *)e = (uint16_t)((((uint8_t)*src ^ (uint32_t)f) & 4) ^ f);
		}
	}
	static void RestoreEnemyFlags() { RestoreFlag4(0x1D97494, SavedFlags() + 3, SavedFlags() + 7); } // 0x6FBE00
	static void RestorePartyFlags() { RestoreFlag4(0x1D972C0, SavedFlags(), SavedFlags() + 3); }     // 0x6FBE40

	// 0x6FBE80: the four stage models' bones rebuilt (bit 1 set on the shown ones), the enemies' flag 4 back
	static void StageModelsBack()
	{
		for (uint32_t m = 0x1D98991; m < 0x1D98A41; m += 0x2C)
		{
			ComputeBonesWorldMatrices((void *)(m + 0x13), &Camera());
			const uint8_t b = *(const uint8_t *)m;
			if (b & 1) *(uint8_t *)m = (uint8_t)(b | 2);
		}
		RestoreFlag4(0x1D97494, SavedFlags() + 3, SavedFlags() + 7);
	}

	// ------------------------------------------------------------------
	// Tables
	// ------------------------------------------------------------------
	// 0x6FA2C0: particle models free
	static void PModelsInit()
	{
		uint8_t *p = (uint8_t *)PModels();
		for (int n = 0x80; n != 0; n--, p += 0x30) *p = 0;
		PartCursor() = (uint8_t *)PModels();
	}

	// 0x6FA2F0: sprites free
	static void SpritesInit()
	{
		uint8_t *p = (uint8_t *)Sprites() + 0xA;
		for (int n = 0x200; n != 0; n--, p += 0x18) *(int16_t *)p = 0;
		SpriteCursor() = (uint8_t *)Sprites();
	}

	// 0x6FA320: shards gone; 64 random spin matrices (scaled 0xF00 - 0..0x1FF), 0x14 bytes apart, at + 0x7704
	static void ShardsReset()
	{
		uint8_t *p = (uint8_t *)Shards() + 6;
		for (int n = 0x78; n != 0; n--, p += 0x34) *(int16_t *)p = 0;
		uint8_t *m = (uint8_t *)(TexBase() + 0x7704);
		for (int n = 0x40; n != 0; n--, m += 0x14)
		{
			int16_t a[4];
			a[0] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
			a[1] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
			a[2] = (int16_t)((CrtRand() & 0x1FF) - 0x100);
			BuildRotationMatrixFromAngles(a, (Mat4x3 *)m);
			const int32_t s = 0xF00 - (CrtRand() & 0x1FF);
			int32_t v[3] = { s, s, s };
			Scale3DMatrix((Mat4x3 *)m, v);
		}
	}

	// 0x6FCC90: a free sprite (life 0) from the cursor on, or nullptr
	static Sprite *SpriteAlloc()
	{
		uint8_t *p = SpriteCursor();
		const uint32_t tex = TexBase();
		if (*(int16_t *)(p + 0xA) != 0)
		{
			p = (uint8_t *)(tex + 0x1240);
			if (*(int16_t *)(tex + 0x124A) != 0)
			{
				int n = 0x200;
				for (;;)
				{
					p += 0x18;
					if (--n == 0) return nullptr;
					if (*(int16_t *)(p + 0xA) == 0) break;
				}
			}
		}
		if ((uint32_t)p >= tex + 0x4228) SpriteCursor() = (uint8_t *)(tex + 0x1240);
		else SpriteCursor() = p + 0x18;
		return (Sprite *)p;
	}

	// 0x6FDB20: a free particle model (life 0) from the cursor on, or nullptr
	static PModel *PModelAlloc()
	{
		uint8_t *p = PartCursor();
		const uint32_t tex = TexBase();
		if (*p != 0)
		{
			p = (uint8_t *)(tex + 0x4240);
			if (*p != 0)
			{
				int n = 0x80;
				for (;;)
				{
					p += 0x30;
					if (--n == 0) return nullptr;
					if (*p == 0) break;
				}
			}
		}
		if ((uint32_t)p >= tex + 0x5A10) PartCursor() = (uint8_t *)(tex + 0x4240);
		else PartCursor() = p + 0x30;
		return (PModel *)p;
	}

	// ------------------------------------------------------------------
	// Screen shards
	// ------------------------------------------------------------------
	// 0x6FA3E0: one shard per triangle of `model` (screen x / y): vertices sorted by x (rotated), relative to the centroid,
	// uvs / tpage from the captured screen, velocity = (centroid - off) * 4 / |centroid - off| (SquareRoot0 inline); then each
	// shard alive (0x10) with a random spin matrix
	static void ShardsInit(uint32_t model, const int16_t *off)
	{
		uint8_t *rec = (uint8_t *)Shards();
		uint8_t *h = (uint8_t *)FieldAlloc(0x24);
		F<uint32_t>(h, 0) = *(const uint32_t *)model + model + 8;
		F<uint32_t>(h, 0x1C) = *(const uint32_t *)off;
		F<uint32_t>(h, 0x20) = *(const uint32_t *)(off + 2);
		uint32_t p = F<uint32_t>(h, 0);
		const int32_t count = *(const int32_t *)p;
		F<uint32_t>(h, 0) = p + 4;
		if (count > 0)
		{
			uint32_t t = p + 4 + 6;
			int16_t *v0 = &F<int16_t>(h, 4);
			for (int32_t left = count; left != 0; left--, t += 0x14)
			{
				uint32_t i0 = *(const uint16_t *)(t - 2), i1 = *(const uint16_t *)t, i2 = *(const uint16_t *)(t + 2);
				F<uint32_t>(h, 4) = *(const uint32_t *)(model + i0 * 4 + 8);
				F<uint32_t>(h, 8) = *(const uint32_t *)(model + i0 * 4 + 0xC);
				F<uint32_t>(h, 0xC) = *(const uint32_t *)(model + i1 * 4 + 8);
				F<uint32_t>(h, 0x10) = *(const uint32_t *)(model + i1 * 4 + 0xC);
				F<uint32_t>(h, 0x14) = *(const uint32_t *)(model + i2 * 4 + 8);
				F<uint32_t>(h, 0x18) = *(const uint32_t *)(model + i2 * 4 + 0xC);
				int32_t cx = div3(F<int16_t>(h, 0xC) + F<int16_t>(h, 0x14) + v0[0]);
				F<int16_t>(rec, 0) = (int16_t)cx;
				int32_t cy = div3(F<int16_t>(h, 0x16) + F<int16_t>(h, 0xE) + F<int16_t>(h, 6));
				F<int16_t>(rec, 2) = (int16_t)cy;
				int32_t cz = div3(F<int16_t>(h, 8) + F<int16_t>(h, 0x18) + F<int16_t>(h, 0x10));
				F<int16_t>(rec, 4) = (int16_t)cz;
				// sort by x (the smallest first, the order kept round)
				int32_t minx = v0[0];
				int flag = 0;
				if (F<int16_t>(h, 0xC) < minx) { minx = F<int16_t>(h, 0xC); flag = 1; }
				const int16_t ox = v0[0];
				if (F<int16_t>(h, 0x14) < minx)
				{
					const int16_t y2 = F<int16_t>(h, 0x16);
					v0[0] = F<int16_t>(h, 0x14);
					F<int16_t>(h, 0x14) = F<int16_t>(h, 0xC);
					F<int16_t>(h, 0xC) = ox;
					const int16_t oy = F<int16_t>(h, 6);
					F<int16_t>(h, 6) = y2;
					F<int16_t>(h, 0x16) = F<int16_t>(h, 0xE);
					const int16_t z2 = F<int16_t>(h, 0x18);
					F<int16_t>(h, 0xE) = oy;
					const int16_t oz = F<int16_t>(h, 8);
					F<int16_t>(h, 8) = z2;
					F<int16_t>(h, 0x18) = F<int16_t>(h, 0x10);
					F<int16_t>(h, 0x10) = oz;
				}
				else if (flag == 1)
				{
					const int16_t y1 = F<int16_t>(h, 0xE);
					v0[0] = F<int16_t>(h, 0xC);
					F<int16_t>(h, 0xC) = F<int16_t>(h, 0x14);
					F<int16_t>(h, 0x14) = ox;
					const int16_t oy = F<int16_t>(h, 6);
					F<int16_t>(h, 6) = y1;
					F<int16_t>(h, 0xE) = F<int16_t>(h, 0x16);
					const int16_t z1 = F<int16_t>(h, 0x10);
					F<int16_t>(h, 0x16) = oy;
					const int16_t oz = F<int16_t>(h, 8);
					F<int16_t>(h, 8) = z1;
					F<int16_t>(h, 0x10) = F<int16_t>(h, 0x18);
					F<int16_t>(h, 0x18) = oz;
				}
				F<int16_t>(rec, 0x14) = (int16_t)(v0[0] - cx);
				F<int16_t>(rec, 0x16) = (int16_t)(F<int16_t>(h, 6) - cy);
				F<int16_t>(rec, 0x18) = (int16_t)(F<int16_t>(h, 8) - cz);
				F<int16_t>(rec, 0x1A) = (int16_t)(F<int16_t>(h, 0xC) - cx);
				F<int16_t>(rec, 0x1C) = (int16_t)(F<int16_t>(h, 0xE) - cy);
				F<int16_t>(rec, 0x1E) = (int16_t)(F<int16_t>(h, 0x10) - cz);
				F<int16_t>(rec, 0x20) = (int16_t)(F<int16_t>(h, 0x14) - cx);
				F<int16_t>(rec, 0x22) = (int16_t)(F<int16_t>(h, 0x16) - cy);
				F<int16_t>(rec, 0x2A) = 0;
				F<int16_t>(rec, 0x24) = (int16_t)(F<int16_t>(h, 0x18) - cz);
				const int32_t x0 = v0[0];
				const int32_t base = (int32_t)(((uint32_t)x0 & 0xFFFFFF00u) | ((uint32_t)x0 & 0xC0u));
				F<uint16_t>(rec, 0x2E) = (uint16_t)((((base + 0x180) >> 6) & 0xF) | 0x130);
				F<uint8_t>(rec, 0x28) = (uint8_t)((uint8_t)x0 - (uint8_t)base);
				F<uint8_t>(rec, 0x29) = (uint8_t)F<int16_t>(h, 6);
				F<uint8_t>(rec, 0x2C) = (uint8_t)((uint8_t)F<int16_t>(h, 0xC) - (uint8_t)base);
				F<uint8_t>(rec, 0x2D) = (uint8_t)F<int16_t>(h, 0xE);
				F<uint8_t>(rec, 0x30) = (uint8_t)((uint8_t)F<int16_t>(h, 0x14) - (uint8_t)base);
				F<uint8_t>(rec, 0x31) = (uint8_t)F<int16_t>(h, 0x16);
				// direction from the offset point, its length by SquareRoot0
				v0[0] = (int16_t)(cx - F<int16_t>(h, 0x1C));
				F<int16_t>(h, 6) = (int16_t)(cy - F<int16_t>(h, 0x1E));
				F<int16_t>(h, 8) = (int16_t)(cz - F<int16_t>(h, 0x20));
				GteLoadIR123(v0);
				GteSQR();
				int32_t sq, lz;
				GteSumMAC123(&sq);
				GteSetLZCS(sq);
				GteReadLZCR(&lz);
				const int32_t z = lz & ~1;
				const int32_t sh = (0x1F - z) >> 1;
				const int32_t idx = (z - 0x18 > 0) ? shl32(sq, (z - 0x18) & 31) : (sq >> ((0x18 - z) & 31));
				int32_t len = shl32((int32_t)*(const uint16_t *)(SQRT_TABLE + (uint32_t)idx * 2), sh & 31) >> 12;
				F<int16_t>(rec, 0xE) = (int16_t)len;
				F<int16_t>(rec, 8) = (int16_t)(shl32(v0[0], 2) / len);
				F<int16_t>(rec, 0xA) = (int16_t)(shl32(F<int16_t>(h, 6), 2) / len);
				F<int16_t>(rec, 0xC) = (int16_t)(shl32(F<int16_t>(h, 8), 2) / len);
				rec += 0x34;
			}
		}
		FieldFree(0x24);
		if (count > 0)
		{
			uint8_t *s = (uint8_t *)Shards();
			for (int32_t left = count; left != 0; left--, s += 0x34)
			{
				F<int16_t>(s, 6) = 0x10;
				const int32_t r = CrtRand() & 0x3F;
				F<uint32_t>(s, 0x10) = TexBase() + (uint32_t)r * 0x14 + 0x7704;
			}
		}
	}

	// 0x6FD100: the shards with len < limit spin (their matrix) and move; a shard whose first vertex reaches 0 is gone
	static void ShardsUpdate(int32_t limit)
	{
		uint8_t *rec = (uint8_t *)Shards();
		for (int n = 0x78; n != 0; n--, rec += 0x34)
		{
			if (F<int16_t>(rec, 6) == 0) continue;
			if (F<int16_t>(rec, 0xE) >= limit) continue;
			GteSetRotMatrixCtrl((const Mat4x3 *)F<uint32_t>(rec, 0x10));
			GteLoadV0(rec + 0x14);
			GteMVMVA_RotV0();
			GteStoreIR123(rec + 0x14);
			GteLoadV0u16(rec + 0x1A);
			GteMVMVA_RotV0();
			if (F<int32_t>(rec, 0x14) == 0 && F<int16_t>(rec, 0x18) == 0)
			{
				F<int16_t>(rec, 6) = 0;
				continue;
			}
			GteStoreIR123(rec + 0x1A);
			GteLoadV0(rec + 0x20);
			GteMVMVA_RotV0();
			GteStoreIR123(rec + 0x20);
			F<int16_t>(rec, 0) = (int16_t)(F<int16_t>(rec, 0) + F<int16_t>(rec, 8));
			F<int16_t>(rec, 2) = (int16_t)(F<int16_t>(rec, 2) + F<int16_t>(rec, 0xA));
			F<int16_t>(rec, 4) = (int16_t)(F<int16_t>(rec, 4) + F<int16_t>(rec, 0xC));
		}
	}

	// 0x6FCFD0: the shards as textured triangles (0x24 808080) on the screen, into the frame arena, OT + 0x1000 (alt viewport);
	// a shard entirely off one side is not drawn
	static void ShardsDraw()
	{
		uint8_t *pk = (uint8_t *)FrameArena();
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		const uint8_t *rec = (const uint8_t *)Shards();
		for (int n = 0x78; n != 0; n--, rec += 0x34)
		{
			if (F<int16_t>((void *)rec, 6) == 0) continue;
			F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 0x28);
			F<uint32_t>(pk, 0x14) = *(const uint32_t *)(rec + 0x2C);
			F<uint16_t>(pk, 0x1C) = *(const uint16_t *)(rec + 0x30);
			const int16_t cx = *(const int16_t *)rec, cy = *(const int16_t *)(rec + 2);
			F<int16_t>(pk, 8) = (int16_t)(*(const int16_t *)(rec + 0x14) + cx);
			F<int16_t>(pk, 0xA) = (int16_t)(*(const int16_t *)(rec + 0x16) + cy);
			F<int16_t>(pk, 0x10) = (int16_t)(*(const int16_t *)(rec + 0x1A) + cx);
			F<int16_t>(pk, 0x12) = (int16_t)(*(const int16_t *)(rec + 0x1C) + cy);
			F<int16_t>(pk, 0x18) = (int16_t)(*(const int16_t *)(rec + 0x20) + cx);
			F<int16_t>(pk, 0x1A) = (int16_t)(*(const int16_t *)(rec + 0x22) + cy);
			uint32_t c = 0;
			if (F<uint16_t>(pk, 8) > 0xA00) c = 1;
			if (F<uint16_t>(pk, 0x10) > 0xA00) c |= 2;
			if (F<uint16_t>(pk, 0x18) > 0xA00) c |= 4;
			if (F<uint16_t>(pk, 0xA) > 0x6C0) c |= 0x10;
			if (F<uint16_t>(pk, 0x12) > 0x6C0) c |= 0x20;
			if (F<uint16_t>(pk, 0x1A) > 0x6C0) c |= 0x40;
			if ((c & 7) == 7 || (c & 0x70) == 0x70) continue;
			F<uint32_t>(pk, 0) = 0x7000000;
			F<uint32_t>(pk, 4) = 0x24808080;
			InsertPrimAltViewport(OTBase() + 0x1000, pk);
			pk += 0x20;
		}
		FrameArena() = (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// The screen capture
	// ------------------------------------------------------------------
	// 0x6FC0A0: the shown stage models (0x1D98991 + 0x2C n bit 0) and the shown enemies (with their shadows; flag 4 set) into `ot`
	static void CaptureModels(uint32_t ot)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		F<int16_t>(h, 0x34) = 0;
		F<int16_t>(h, 0x36) = 0;
		F<int16_t>(h, 0x38) = 0x140;
		F<int16_t>(h, 0x3A) = 0xD8;
		F<int32_t>(h, 0x40) = -1;
		F<uint32_t>(h, 0x24) = var<uint32_t>(0x1D98B3C);
		F<uint32_t>(h, 0x30) = var<uint32_t>(0x1D969A8);
		const uint32_t *tbl = (const uint32_t *)CAPTURE_MODELS;
		for (uint32_t m = 0x1D989A4; tbl < (const uint32_t *)0x14AF700; tbl += 2, m += 0x2C)
		{
			if (!(*(const uint8_t *)(m - 0x13) & 1)) continue;
			BuildBoneMatricesFromPose((void *)m);
			ComputeBonesWorldMatrices((void *)m, &Camera());
			F<uint32_t>(h, 0x3C) = *(const uint32_t *)(m + 0x14);
			F<uint16_t>(h, 0x44) = *(const uint16_t *)(m - 0x12);
			FrameArena() = RenderGeometry(m - 0x10, h + 0x20, ot + tbl[0] * 4, (int32_t)tbl[1], FrameArena());
			*(uint8_t *)(m - 0x13) &= 0xFD;
		}
		for (uint32_t e = 0x1D97494; e < 0x1D97704; e += 0x9C)
		{
			const uint16_t f = *(const uint16_t *)e;
			if (!(f & 2)) continue;
			*(uint16_t *)e = (uint16_t)(f | 4);
			ComposeAffineTransform(&Camera(), (const Mat4x3 *)(e + 0x40), (Mat4x3 *)h);
			FrameArena() = DrawShadow((uint8_t *)e, ot + 0x4040, 0x10, FrameArena());
			ComputeBonesWorldMatrices((void *)(e + 0x60), h);
			F<uint32_t>(h, 0x24) = var<uint32_t>(0x1D98B3C);
			F<int16_t>(h, 0x34) = 0;
			F<int16_t>(h, 0x36) = 0;
			F<int16_t>(h, 0x38) = 0x140;
			F<int16_t>(h, 0x3A) = 0xD8;
			F<uint32_t>(h, 0x3C) = *(const uint32_t *)(e + 0x28);
			F<uint32_t>(h, 0x48) = 0;
			F<uint32_t>(h, 0x40) = *(const uint32_t *)(e + 0x7C);
			FrameArena() = RenderGeometry(*(const uint32_t *)(e + 0x64), h + 0x20, ot + 0x44, 4, FrameArena());
			BuildBoneMatricesFromPose((void *)(e + 0x60));
			const uint32_t w = *(const uint32_t *)(e + 0x78);
			if (w == 0) continue;
			F<int32_t>(h, 0x40) = -1;
			ComputeBonesWorldMatrices((void *)w, h);
			FrameArena() = RenderGeometry(*(const uint32_t *)(w + 4), h + 0x20, ot + 0x44, 4, FrameArena());
			BuildBoneMatricesFromPose((void *)w);
		}
		FieldFree(0x4C);
	}

	// 0x6FBEE0: the screen capture into `ot` (0x1122 entries): a draw mode (texture window 0x100 x 0x100) and a draw
	// environment on the other display buffer, the stage models / enemies, then a draw environment at (0x180, 0x100)
	static int32_t Capture(uint32_t ot)
	{
		ClearOrderingTable(ot, 0x1122);
		DrawEnv d1;
		GetDrawEnv(&d1);
		int16_t rect[4];
		rect[3] = 0x100;
		rect[2] = 0x100;
		uint8_t *p = (uint8_t *)FrameArena();
		const int16_t other = d1.ofs[0] != 0 ? 0 : 0x140;
		FrameArena() += 0xC;
		const uint32_t tpage = *(const uint32_t *)&d1.tpage & 0xFFFF;
		d1.ofs[0] = other;
		const uint32_t dtd = d1.dtd, dfe = d1.dfe;
		rect[1] = 0;
		rect[0] = 0;
		SetDrawMode(p, (int32_t)dfe, (int32_t)dtd, (int32_t)tpage, rect);
		InsertPrimAutoDepth(ot + 4, p);
		DrawEnv d2;
		p = (uint8_t *)FrameArena();
		FrameArena() += 0x40;
		SetDefDrawEnv(&d2, d1.ofs[0], d1.ofs[1], 0x140, 0xD8);
		d2.tpage = d1.tpage;
		d2.dtd = 0;
		d2.dfe = d1.dfe;
		d2.isbg = 0;
		d2.b0 = 0;
		d2.g0 = 0;
		d2.r0 = 0;
		SetDrawEnv(p, &d2);
		InsertPrimAutoDepth(ot + 4, p);
		CaptureModels(ot);
		p = (uint8_t *)FrameArena();
		FrameArena() += 0x40;
		SetDefDrawEnv(&d2, 0x180, 0x100, 0x140, 0xD8);
		d2.tpage = d1.tpage;
		d2.dtd = d1.dtd;
		d2.dfe = d1.dfe;
		d2.isbg = 1;
		d2.b0 = 0;
		d2.g0 = 0;
		d2.r0 = 0;
		SetDrawEnv(p, &d2);
		InsertPrimAutoDepth(ot + 0x4484, p);
		return 0;
	}

	// ------------------------------------------------------------------
	// Small draws
	// ------------------------------------------------------------------
	// 0x6FC450: full-screen tile (r, g, b) at step t of a fade in over `in` ticks, held `hold`, out over `out`, into the
	// frame arena
	static int32_t FadeLevel(int32_t t, int32_t in, int32_t hold, int32_t out)
	{
		const int32_t din = in != 0 ? in : 1;
		const int32_t dout = out != 0 ? out : 1;
		if (t < in) return shl32(t + 1, 12) / din;
		if (t < in + hold) return 0x1000;
		return (shl32(in - t + hold, 12) - 0x1000) / dout + 0x1000;
	}
	static void FadeTile(int32_t t, int32_t r, int32_t g, int32_t b, int32_t in, int32_t hold, int32_t out)
	{
		const int32_t level = FadeLevel(t, in, hold, out);
		FX_HELD(held_note_tile(t, r, g, b, in, hold, out);)
		FrameArena() = Tile(r, g, b, level, FrameArena());
	}

	// 0x6FC250 (Effect_CameraKeysLerp): battle camera between two (look-at, eye) keys, step i of n, 0x1D977A2 from za to zb
	static void CameraKeys(const int16_t *k0, const int16_t *k1, int32_t za, int32_t zb, int32_t i, int32_t n)
	{
		const int32_t d = n - 1;
		var<int16_t>(0x1D977A2) = (int16_t)(mul32(zb - za, i) / d + za);
		const int32_t t = shl32(i, 12) / d;
		const int32_t u = 0x1000 - t;
		GteSetIR0(u);
		GteLoadIR123(k0);
		GteGPF();
		GteSetIR0(t);
		GteLoadIR123(k1);
		GteGPL();
		GteStoreIR123((void *)0xB8B7F8);
		GteSetIR0(u);
		GteLoadIR123(k0 + 3);
		GteGPF();
		GteSetIR0(t);
		GteLoadIR123(k1 + 3);
		GteGPL();
		GteStoreIR123((void *)0xB8B7F0);
	}

	// 0x6FC300: the eye turned about the look-at by Y `yaw` (then X `pitch`), its distance x s (4.12); the eye height first
	// pulled by s
	static void CameraOrbit(int32_t pitch, int32_t yaw, int32_t s)
	{
		uint8_t frame[0x48];
		int16_t *v = (int16_t *)frame;
		Mat4x3 *m = (Mat4x3 *)(frame + 8);
		RotY(yaw, m);
		if (pitch != 0)
		{
			Mat4x3 *rx = (Mat4x3 *)(frame + 0x28);
			RotX(pitch, rx);
			MatMul3(rx, m, m);
		}
		const int16_t ly = var<int16_t>(0xB8B7FA);
		var<int16_t>(0xB8B7F2) = (int16_t)((mul32(var<int16_t>(0xB8B7F2) - ly, s) >> 12) + ly);
		v[1] = (int16_t)(mul32(var<int16_t>(0xB8B7F2) - ly, s) >> 12);
		const int32_t lz = var<int16_t>(0xB8B7FC), lx = var<int16_t>(0xB8B7F8);
		v[2] = (int16_t)(mul32(var<int16_t>(0xB8B7F4) - lz, s) >> 12);
		v[0] = (int16_t)(mul32(var<int16_t>(0xB8B7F0) - lx, s) >> 12);
		MatVec(m, v, v);
		var<int16_t>(0xB8B7F0) = (int16_t)(var<int16_t>(0xB8B7F8) + v[0]);
		var<int16_t>(0xB8B7F2) = (int16_t)(var<int16_t>(0xB8B7FA) + v[1]);
		var<int16_t>(0xB8B7F4) = (int16_t)(var<int16_t>(0xB8B7FC) + v[2]);
	}

	// 0x6FC410: one damage wave: the next (target count / 8) target records
	static void DamageWave()
	{
		const uint32_t n = (uint32_t)((ActionData *)F<uint8_t *>(Ctx(), 4))->target_count >> 3;
		for (uint32_t k = n; k != 0; k--)
		{
			ApplyActionResultToTarget(TargetCursor());
			TargetCursor() += 0x18;
		}
	}

	// 0x6FD2D0: a point (x, y, z, bone) of a model (container or battle entity) in world space (bones rebuilt from the pose)
	static void BonePoint(uint8_t *c, const int16_t *pt, int16_t *out)
	{
		Mat4x3 m;
		uint8_t *skel = **(uint8_t ***)(c + 0x64);
		BuildBoneMatricesFromPose(c + 0x60);
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(skel + 0x20 + mul32(pt[3], 0x30)), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(pt);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// 0x6FD1B0: a flash (queue B, 0x6FD340) and n debris (0x6FD3C0) at a bone point of `c`
	static void FlashAt(uint8_t *c, const int16_t *pt, int32_t n)
	{
		int16_t v[4];
		BonePoint(c, pt, v);
		SmallNode *t = (SmallNode *)AddTaskToQueue(QB(), ORIG_FlashTask);
		if (t)
		{
			t->frame = 0;
			t->pos[0] = v[0];
			t->pos[1] = v[1];
			t->pos[2] = v[2];
			t->size = 0x1000;
		}
		while (n-- != 0)
		{
			t = (SmallNode *)AddTaskToQueue(QB(), ORIG_DebrisTask);
			if (!t) break;
			t->frame = 0xF;
			t->pos[0] = v[0];
			t->pos[1] = v[1];
			t->pos[2] = v[2];
			t->size = 0x1000;
			t->vel[0] = (int16_t)((CrtRand() & 0x7F) - 0x40);
			t->vel[1] = (int16_t)(0x80 - (CrtRand() & 0x7F));
			t->vel[2] = (int16_t)((CrtRand() & 0x7F) + 0x7F);
			t->ang[0] = (int8_t)CrtRand();
			t->ang[1] = (int8_t)CrtRand();
			t->dang[0] = (int8_t)((CrtRand() & 0x1F) - 0x10);
			t->dang[1] = (int8_t)((CrtRand() & 0x1F) - 0x10);
			t->gravity = (int8_t)0xEC;
		}
	}

	// 0x6FD4D0 / 0x6FDB80: a dive (queue C) facing the angles (a, b), placed k (6 / 2) x its z axis from the centre
	static DiveNode *DiveSpawn(uint32_t task, int32_t a, int32_t b, int32_t k)
	{
		DiveNode *t = (DiveNode *)AddTaskToQueue(QC(), task);
		if (!t) return nullptr;
		DecodeModelPrimLayout(LAYOUT_Dive, t->layout, 0x158);
		int16_t ang[4];
		ang[0] = (int16_t)a;
		ang[1] = (int16_t)b;
		ang[2] = 0;
		BuildRotationMatrixFromAngles(ang, &t->m);
		t->m.t[0] = Centre()[0] + mul32(t->m.m[0][2], k);
		t->m.t[1] = Centre()[1] + mul32(t->m.m[1][2], k);
		t->counter = 0;
		t->m.t[2] = Centre()[2] + mul32(t->m.m[2][2], k);
		return t;
	}

	// ------------------------------------------------------------------
	// Prim player callback
	// ------------------------------------------------------------------
	// 0x6FBB80: one object: frames of its model blended by channel B (0x701390 into the block's buffer), rotation from its
	// angles, offset rotated by the anchor matrix (flag 0x200: not), scale, colour (channel A) and blend (flag 0x4000)
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		PrimArg *blk = (PrimArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t frame[0x38];
		int16_t *v = (int16_t *)frame;
		int32_t *s = (int32_t *)(frame + 8);
		Mat4x3 *m = (Mat4x3 *)(frame + 0x18);
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
		const uint32_t data = (uint32_t)l->data;
		const uint32_t mdl = data + *(const uint32_t *)(data + (int32_t)(int16_t)r->flags_lo * 4 + 8);
		F<uint32_t>(h, 0) = mdl;
		const int16_t b0 = r->b0, b1 = r->b1;
		const uint32_t nv = *(const uint32_t *)(mdl + 4);
		if (b0 == b1 || r->b == 0)
		{
			if (b0 == 0) F<uint32_t>(h, 4) = mdl + 0xC;
			else F<uint32_t>(h, 4) = mdl + (uint32_t)mul32((int32_t)nv, b0) * 8 + 0xC;
		}
		else if (r->b == 0x1000)
		{
			if (b1 == 0) F<uint32_t>(h, 4) = mdl + 0xC;
			else F<uint32_t>(h, 4) = mdl + (uint32_t)mul32((int32_t)nv, b1) * 8 + 0xC;
		}
		else
		{
			BlendVertexFrames(mdl, b0, b1, r->b, blk->morph);
			F<uint32_t>(h, 4) = blk->morph;
		}
		RotationFromAngles(r->rot, m);
		const uint32_t flags = r->flags;
		v[0] = r->pos[0];
		v[1] = r->pos[1];
		v[2] = r->pos[2];
		int32_t x, y, z;
		if (flags & 0x200)
		{
			x = v[0];
			y = v[1];
			z = v[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&blk->m);
			GteLoadV0(v);
			GteMVMVA_RotV0();
			GteReadMAC123(m->t);
			MatrixMultiply(&blk->m, m);
			x = m->t[0];
			y = m->t[1];
			z = m->t[2];
		}
		m->t[0] = x + blk->m.t[0];
		m->t[1] = y + blk->m.t[1];
		m->t[2] = z + blk->m.t[2];
		if (*(const uint32_t *)r->scale != 0x10001000 || r->scale[2] != 0x1000)
		{
			s[0] = r->scale[0];
			s[1] = r->scale[1];
			s[2] = r->scale[2];
			Scale3DMatrix(m, s);
		}
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		F<uint32_t>(h, 0x1C) = (r->flags & 0x4000) ? 0x2000 : 0x2030;
		F<int32_t>(h, 0xC) = r->a;
		if (r->a != 0)
		{
			F<uint32_t>(h, 0x1C) |= 0xC0;
			F<uint32_t>(h, 8) = *(const uint32_t *)r->rgb;
		}
		F<int32_t>(h, 0x18) = blk->depth;
		F<uint32_t>(h, 0x20) = 0;
		blk->cursor = RenderPrimSet(h, OTBase() + 0x44, 2, blk->cursor);
		FieldFree(0x6C);
	}

	// a layout through the anchor (x, y, z) (Y angle 0) and the camera, the morph buffer + 0x1888C, at `depth`
	static int Play(uint8_t *layout, int32_t x, int32_t y, int32_t z, int16_t depth, uint32_t *cursor)
	{
		PrimArg a;
		RotY(0, &a.m);
		a.m.t[0] = x;
		a.m.t[1] = y;
		a.m.t[2] = z;
		ComposeAffineTransform(&Camera(), &a.m, &a.m);
		a.morph = TexBase() + 0x1888C;
		a.depth = depth;
		a.cursor = *cursor;
		FX_HELD(held_note_play(layout, x, y, z, depth, cursor == &FrameArena());)
		const int left = prim::play((prim::Layout *)layout, PartCallback, (int)&a, 0);
		*cursor = a.cursor;
		return left;
	}

	// ------------------------------------------------------------------
	// Particles
	// ------------------------------------------------------------------
	// 0x6FCCF0: sprites move (x / y by their velocity, which then accelerates; z by velocity + 4 x acceleration), age
	static void SpritesUpdate()
	{
		uint8_t *p = (uint8_t *)Sprites() + 0xA;
		for (int n = 0x200; n != 0; n--, p += 0x18)
		{
			if (*(int16_t *)p == 0) continue;
			const int32_t ax = (int8_t)p[0xA];
			const int16_t vx = *(int16_t *)(p + 2), vy = *(int16_t *)(p + 4);
			*(int16_t *)(p - 6) = (int16_t)(*(int16_t *)(p - 6) + vx);
			*(int16_t *)(p - 4) = (int16_t)(*(int16_t *)(p - 4) + vy);
			(*(int16_t *)p)--;
			*(int16_t *)(p + 2) = (int16_t)(vx + ax * 4);
			const int32_t ay = (int8_t)p[0xB];
			const int16_t dz = (int16_t)((int16_t)((int8_t)p[0xC] * 4) + *(int16_t *)(p + 6));
			*(int16_t *)(p + 4) = (int16_t)(vy + ay * 4);
			*(int16_t *)(p - 2) = (int16_t)(*(int16_t *)(p - 2) + dz);
		}
	}

	// 0x6FC9E0 (MAG_065_RenderFlipbook): the frame's sprite pieces (0x14-byte records +0x2C, count +0x30) as quads scaled by the
	// header's matrix +0x84 / position +0x98 (+0x25 bit 0: a draw-mode packet after each piece)
	static uint32_t Flipbook(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		int32_t count = F<int32_t>(h, 0x30);
		const uint8_t *rec = F<const uint8_t *>(h, 0x2C);
		if (count <= 0) return (uint32_t)pk;
		for (; count != 0; count--, rec += 0x14)
		{
			F<int16_t>(h, 0x72) = 0;
			F<int16_t>(h, 0x70) = 0;
			F<int16_t>(h, 0x6E) = 0;
			F<int16_t>(h, 0x6A) = 0;
			F<int16_t>(h, 0x68) = 0;
			F<int16_t>(h, 0x66) = 0;
			F<int16_t>(h, 0x74) = 0x1000;
			const int32_t tp = *(const int16_t *)(rec + 0xE);
			F<int16_t>(h, 0x64) = *(const int16_t *)(rec + 0x10);
			F<int16_t>(h, 0x6A) = 0;
			F<int16_t>(h, 0x66) = 0;
			F<int16_t>(h, 0x6C) = *(const int16_t *)(rec + 0x12);
			const uint32_t w = rec[0], hh = rec[2];
			const int32_t w8 = (int32_t)(w << 3), h8 = (int32_t)(hh << 3);
			F<int16_t>(h, 0x54) = (int16_t)-w8;
			F<int16_t>(h, 0x44) = (int16_t)-w8;
			F<int16_t>(h, 0x5C) = (int16_t)w8;
			F<int16_t>(h, 0x4C) = (int16_t)w8;
			F<int16_t>(h, 0x4E) = (int16_t)-h8;
			F<int16_t>(h, 0x46) = (int16_t)-h8;
			F<int16_t>(h, 0x5E) = (int16_t)h8;
			F<int16_t>(h, 0x56) = (int16_t)h8;
			F<int32_t>(h, 0x78) = shl32(*(const int16_t *)(rec + 8), 4) + w8;
			F<int32_t>(h, 0x80) = 0;
			F<int32_t>(h, 0x7C) = shl32(*(const int16_t *)(rec + 0xA), 4) + h8;
			GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x84));
			GteLoadIRColumn(h + 0x64);
			GteMVMVA_RotIR();
			F<uint32_t>(pk, 0) = 0x9000000;
			pk[4] = rec[1];
			pk[5] = rec[1];
			pk[6] = rec[1];
			pk[7] = rec[3];
			GteStoreIRColumn(h + 0x64);
			GteLoadIRColumn(h + 0x66);
			GteMVMVA_RotIR();
			F<uint32_t>(pk, 0xC) = *(const uint32_t *)(rec + 4);
			GteStoreIRColumn(h + 0x66);
			GteLoadIRColumn(h + 0x68);
			GteMVMVA_RotIR();
			pk[0x1C] = rec[4];
			GteStoreIRColumn(h + 0x68);
			GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x84));
			GteLoadV0Dwords(h + 0x78);
			GteMVMVA_RotV0Tr();
			pk[0x15] = rec[5];
			GteReadMAC123((int32_t *)(h + 0x78));
			GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x64));
			GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x64));
			GteLoadV012(h + 0x44, h + 0x4C, h + 0x54);
			GteRTPT();
			uint32_t u = rec[4] + w;
			if (u >= 0x100) u = 0xFF;
			pk[0x24] = (uint8_t)u;
			pk[0x14] = (uint8_t)u;
			GteReadFLAG(h + 0x40);
			if (F<uint32_t>(h, 0x40) & 0x60000) continue;
			F<uint16_t>(pk, 0x16) = (uint16_t)(tp & 0x1FF);
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0(h + 0x5C);
			GteRTPS();
			uint32_t vv = rec[5] + hh;
			if (vv >= 0x100) vv = 0xFF;
			pk[0x25] = (uint8_t)vv;
			pk[0x1D] = (uint8_t)vv;
			GteReadSXY2(pk + 0x20);
			if (h[0x25] & 1)
			{
				uint8_t *p2 = pk + 0x28;
				const uint32_t slot = ot + (uint32_t)mode * 4;
				F<uint32_t>(p2, 0) = 0x1000000;
				F<uint32_t>(p2, 4) = 0xE1000220;
				InsertPrimAutoDepth(slot, p2);
				InsertPrimAutoDepth(slot, pk);
				pk = p2 + 8;
			}
			else
			{
				InsertPrimAutoDepth(ot + (uint32_t)mode * 4, pk);
				pk += 0x28;
			}
		}
		return (uint32_t)pk;
	}

	// 0x6FC8B0: the sprites, one flipbook frame each (frame by life), scaled by +0x12, through the camera, into the module arena
	static void SpritesDraw()
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<int16_t>(h, 0x60) = 0;
		F<int16_t>(h, 0x58) = 0;
		F<int16_t>(h, 0x50) = 0;
		F<int16_t>(h, 0x48) = 0;
		uint8_t *s = (uint8_t *)Sprites();
		for (int n = 0x200; n != 0; n--, s += 0x18)
		{
			if (F<int16_t>(s, 0xA) == 0) continue;
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			GteLoadV0(s + 4);
			GteMVMVA_RotV0Tr();
			GteReadMAC123((int32_t *)(h + 0x98));
			F<uint32_t>(h, 0x84) = 0;
			F<uint32_t>(h, 0x88) = 0;
			F<uint32_t>(h, 0x8C) = 0;
			F<uint32_t>(h, 0x90) = 0;
			F<uint32_t>(h, 0x94) = 0;
			const int16_t sc = F<int16_t>(s, 0x12);
			F<int16_t>(h, 0x94) = sc;
			F<int16_t>(h, 0x8C) = sc;
			F<int16_t>(h, 0x84) = sc;
			const uint32_t seq = F<uint32_t>(s, 0);
			F<uint32_t>(h, 0) = seq;
			const int32_t cnt = *(const int16_t *)(seq + 8);
			const int32_t rem = (F<int16_t>(s, 0xA) - 1) % cnt;
			const uint32_t fr = seq + (int32_t)*(const int16_t *)(seq + (uint32_t)(cnt - rem) * 2 + 8);
			F<uint32_t>(h, 0x2C) = fr;
			int32_t c = *(const int32_t *)fr;
			F<int32_t>(h, 0x30) = c;
			if (c < 0)
			{
				c &= 0x7FFFFFFF;
				h[0x25] |= 1;
				F<int32_t>(h, 0x30) = c;
			}
			F<uint32_t>(h, 0x2C) += 4;
			Cursor() = Flipbook(h, OTBase() + 0x44, 2, Cursor());
		}
		FieldFree(0xB4);
	}

	// 0x6FC750: particle models age, spin / scale / fade by their kind's steps and move (velocity pulled by their y axis / 80);
	// at death a sparkle (kind +7) at its position
	static void PModelsUpdate()
	{
		uint8_t *p = (uint8_t *)PModels();
		for (int n = 0x80; n != 0; n--, p += 0x30)
		{
			if (p[0] == 0) continue;
			const uint8_t *k = (const uint8_t *)(PART_KINDS + ((uint32_t)p[1] << 4));
			if (--p[0] == 0)
			{
				if (k[7] == 0) continue;
				Sprite *s = SpriteAlloc();
				if (!s) continue;
				s->pos[0] = F<int16_t>(p, 0x24);
				s->pos[1] = F<int16_t>(p, 0x28);
				s->pos[2] = F<int16_t>(p, 0x2C);
				s->scale = 0x800;
				s->life = 0x10;
				s->seq = SEQ_Sparkle;
				s->vel[0] = (int16_t)(F<int16_t>(p, 0x12) >> 7);
				s->vel[1] = (int16_t)(F<int16_t>(p, 0x18) >> 7);
				s->vel[2] = (int16_t)(F<int16_t>(p, 0x1E) >> 7);
				F<uint32_t>(s, 0x14) = 0;
				continue;
			}
			const int16_t vy = F<int16_t>(p, 0xC);
			F<int16_t>(p, 2) = (int16_t)(F<int16_t>(p, 2) + *(const int16_t *)(k + 8));
			F<int16_t>(p, 4) = (int16_t)(F<int16_t>(p, 4) + *(const int16_t *)(k + 0xA));
			F<int16_t>(p, 6) = (int16_t)(F<int16_t>(p, 6) + *(const int16_t *)(k + 0xC));
			const int16_t vx = F<int16_t>(p, 0xA);
			F<int16_t>(p, 8) = (int16_t)(F<int16_t>(p, 8) + *(const int16_t *)(k + 0xE));
			F<int32_t>(p, 0x24) += vx;
			F<int32_t>(p, 0x28) += vy;
			const int16_t vz = F<int16_t>(p, 0xE);
			F<int32_t>(p, 0x2C) += vz;
			F<int16_t>(p, 0xA) = (int16_t)(vx - div80(F<int16_t>(p, 0x12)));
			F<int16_t>(p, 0xC) = (int16_t)(vy - div80(F<int16_t>(p, 0x18)));
			F<int16_t>(p, 0xE) = (int16_t)(vz - div80(F<int16_t>(p, 0x1E)));
		}
	}

	// 0x6FC500: the particle models: their kind's model (one quad), rotated / scaled, through the camera, far colour by kind,
	// depth cue by +2, into the module arena
	static void PModelsDraw()
	{
		const uint32_t ot = OTBase() + 0x44;
		uint8_t *pk = (uint8_t *)Cursor();
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		RotY(0, h + 0x20);
		uint8_t *p = (uint8_t *)PModels();
		for (int n = 0x80; n != 0; n--, p += 0x30)
		{
			if (p[0] == 0) continue;
			const uint8_t *k = (const uint8_t *)(PART_KINDS + ((uint32_t)p[1] << 4));
			const uint32_t model = *(const uint32_t *)k;
			F<int16_t>(h, 0x20) = F<int16_t>(p, 4);
			const uint8_t *prim = (const uint8_t *)(model + *(const uint32_t *)model + 0x10);
			F<int16_t>(h, 0x28) = F<int16_t>(p, 6);
			F<int16_t>(h, 0x30) = F<int16_t>(p, 8);
			MatMul3(p + 0x10, h + 0x20, h);
			F<int32_t>(h, 0x14) = F<int32_t>(p, 0x24);
			F<int32_t>(h, 0x18) = F<int32_t>(p, 0x28);
			F<int32_t>(h, 0x1C) = F<int32_t>(p, 0x2C);
			GteSetRotMatrixCtrl(&Camera());
			GteLoadIRColumn(h);
			GteMVMVA_RotIR();
			GteStoreIRColumn(h);
			GteLoadIRColumn(h + 2);
			GteMVMVA_RotIR();
			GteStoreIRColumn(h + 2);
			GteLoadIRColumn(h + 4);
			GteMVMVA_RotIR();
			GteStoreIRColumn(h + 4);
			GteSetTransVectorCtrl(&Camera());
			GteLoadV0Dwords(h + 0x14);
			GteMVMVA_RotV0Tr();
			GteReadMAC123((int32_t *)(h + 0x14));
			GteSetRotMatrixCtrl((const Mat4x3 *)h);
			GteSetTransVectorCtrl((const Mat4x3 *)h);
			GteSetFarColor(k[4], k[5], k[6]);
			GteLoadV012((const void *)(model + (uint32_t)*(const uint16_t *)(prim + 4) * 4 + 8), (const void *)(model + (uint32_t)*(const uint16_t *)(prim + 6) * 4 + 8),
				(const void *)(model + (uint32_t)*(const uint16_t *)(prim + 8) * 4 + 8));
			GteRTPT();
			F<uint32_t>(pk, 0) = 0x9000000;
			F<uint32_t>(pk, 4) = *(const uint32_t *)prim;
			F<uint32_t>(pk, 0xC) = *(const uint32_t *)(prim + 0xC);
			F<uint32_t>(pk, 0x14) = *(const uint32_t *)(prim + 0x10);
			const int32_t uv3 = *(const int32_t *)(prim + 0x14);
			F<int32_t>(pk, 0x1C) = uv3;
			F<int32_t>(pk, 0x24) = uv3 >> 16;
			GteReadFLAG(h + 0x40);
			if (F<uint32_t>(h, 0x40) & 0x60000) continue;
			GteReadSXY012(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0((const void *)(model + (uint32_t)*(const uint16_t *)(prim + 0xA) * 4 + 8));
			GteRTPS();
			GteReadSXY2(pk + 0x20);
			GteAVSZ4();
			GteReadOTZWord(h + 0x44);
			GteLoadRGBC(pk + 4);
			const int32_t fade = F<int16_t>(p, 2);
			F<int32_t>(h, 0x48) = fade;
			GteSetIR0(fade);
			GteDPCS();
			GteStoreRGB2(pk + 4);
			InsertPrimAutoDepth(ot + (uint32_t)(F<int32_t>(h, 0x44) >> 2) * 4, pk);
			pk += 0x28;
		}
		FieldFree(0x4C);
		Cursor() = (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// The star dots (65's 0x6E4350 / 0x6E4570 on this module's tables)
	// ------------------------------------------------------------------
	// 0x6FCD50: the star dots (8 groups x 128, each group turned by its angles 0x14AF49C / 0x14AF49E) at (x, y, z) through
	// the camera; dots with distance < limit; twinkling colour (level x 16)
	static uint32_t Stars(int32_t depth, int32_t x, int32_t y, int32_t z, int32_t limit, uint32_t cursor)
	{
		const uint8_t *st = (const uint8_t *)(TexBase() + 0x7C04);
		uint8_t *pk = (uint8_t *)cursor;
		uint8_t *h = (uint8_t *)FieldAlloc(0x64);
		const uint32_t *cam = (const uint32_t *)&Camera();
		F<uint32_t>(h, 0) = cam[0];
		F<uint32_t>(h, 4) = cam[1];
		F<uint32_t>(h, 8) = cam[2];
		F<uint32_t>(h, 0xC) = cam[3];
		F<uint16_t>(h, 0x10) = *(const uint16_t *)&cam[4];
		Mat4x3 *v = (Mat4x3 *)h;
		v->t[0] = x;
		v->t[1] = y;
		v->t[2] = z;
		TransformVec3(v, v->t, v->t);
		v->t[0] = (int32_t)((uint32_t)v->t[0] + (uint32_t)Camera().t[0]);
		v->t[1] = (int32_t)((uint32_t)v->t[1] + (uint32_t)Camera().t[1]);
		v->t[2] = (int32_t)((uint32_t)v->t[2] + (uint32_t)Camera().t[2]);
		for (uint32_t g = STAR_GROUPS + 0xE; g < 0x14AF51E; g += 0x10)
		{
			RotX(*(const int16_t *)(g - 2), h + 0x40);
			RotY(*(const int16_t *)g, h + 0x20);
			MatMul3(h + 0x20, h + 0x40, h + 0x40);
			MatMul3(h, h + 0x40, h + 0x40);
			GteSetRotMatrix((const Mat4x3 *)(h + 0x40));
			GteSetTransVector(v);
			for (int n = 0x80; n != 0; n--, st += 0x10)
			{
				if (*(const int16_t *)(st + 6) >= limit) continue;
				GteLoadV0(st);
				GteRTPS();
				GteReadSXY2(pk + 8);
				GteReadOTZ(h + 0x60);
				if (F<int32_t>(h, 0x60) <= 0) continue;
				if (st[0xB] != 0)
				{
					const int32_t l = (uint16_t)((uint16_t)st[0xB] << 4);
					GteSetIR0(l);
					GteLoadIRBytes(st + 8);
					GteGPF();
					GteSetIR0(0x1000 - l);
					GteLoadIRBytes(st + 0xC);
					GteGPL();
					GteStoreIRBytes(pk + 4);
				}
				else F<uint32_t>(pk, 4) = *(const uint32_t *)(st + 8);
				F<uint32_t>(pk, 0) = 0x2000000;
				pk[7] = 0x68;
				int32_t d = (F<int32_t>(h, 0x60) >> 2) + depth;
				F<int32_t>(h, 0x60) = d;
				if (d < 0) F<int32_t>(h, 0x60) = 0;
				InsertPrimAutoDepth(OTBase() + (uint32_t)F<int32_t>(h, 0x60) * 4 + 0x44, pk);
				pk += 0xC;
			}
		}
		FieldFree(0x64);
		return (uint32_t)pk;
	}

	// 0x6FCF70: the dots twinkle (level += step, bouncing at 0 / 255); the groups turn
	static void StarsTwinkle()
	{
		uint8_t *p = (uint8_t *)(TexBase() + 0x7C0F);
		for (int n = 0x400; n != 0; n--, p += 0x10)
		{
			const uint16_t v = (uint16_t)((int16_t)(int8_t)p[4] + (uint16_t)p[0]);
			if (v > 0xFF)
			{
				p[0] = (uint8_t)~(uint8_t)v;
				p[4] = (uint8_t)-(int8_t)p[4];
			}
			else p[0] = (uint8_t)v;
		}
		for (uint32_t g = STAR_GROUPS + 0xC; g < 0x14AF51C; g += 0x10)
		{
			*(int16_t *)g = (int16_t)(*(int16_t *)g + *(const int16_t *)(g - 4));
			*(int16_t *)(g + 2) = (int16_t)(*(int16_t *)(g + 2) + *(const int16_t *)(g - 2));
		}
	}

	// ------------------------------------------------------------------
	// Queue B tasks
	// ------------------------------------------------------------------
	// the flash's draw: frame +0x1F of flipbook 0x14A0538 at its position (shadow-rotation camera, size +0x12)
	static void FlashDraw(const SmallNode *t)
	{
		TransformCameraByShadowRotation(t->pos, t->size, -(t->size >> 2));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint32_t>(h, 0) = SEQ_Flash;
		F<int16_t>(h, 4) = (int16_t)t->frame;
		F<int16_t>(h, 0x24) = 0;
		Cursor() = InitEffectSequenceFromData(h, OTBase() + 0x44, 2, Cursor());
		FieldFree(0xB4);
	}

	// 0x6FD340: flash: frame +0x1F of flipbook 0x14A0538 at its position (shadow-rotation camera, size +0x12), 12 frames
	static uint32_t __cdecl FlashTask(TaskNode *n)
	{
		SmallNode *t = (SmallNode *)n;
		FX_HELD(held_note_flash(t);)
		FlashDraw(t);
		t->frame = (int8_t)(t->frame + 1);
		return t->frame < 0xC ? 0 : TASK_END;
	}

	// the debris' draw: prim model 0x14A04D8 at pos turned (ax, ay, 0)
	static void DebrisDraw(const int16_t *pos, int16_t ax, int16_t ay)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x80);
		F<int16_t>(h, 0) = ax;
		Mat4x3 *m = (Mat4x3 *)(h + 8);
		F<int16_t>(h, 2) = ay;
		F<int16_t>(h, 4) = 0;
		BuildRotationMatrixFromAngles((const int16_t *)h, m);
		m->t[0] = pos[0];
		m->t[1] = pos[1];
		m->t[2] = pos[2];
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		F<uint32_t>(h, 0x44) = 0;
		F<uint32_t>(h, 0x28) = MDL_Debris;
		Cursor() = RenderPrimModel(h + 0x28, OTBase() + 0x44, 2, Cursor());
		FieldFree(0x80);
	}

	// 0x6FD3C0: debris: prim model 0x14A04D8 at its position, angles x 16, falling (gravity +0x1C, bouncing at y >= 0), 15 ticks
	static uint32_t __cdecl DebrisTask(TaskNode *n)
	{
		SmallNode *t = (SmallNode *)n;
		FX_HELD(held_note_debris(t);)
		DebrisDraw(t->pos, (int16_t)((int16_t)t->ang[0] << 4), (int16_t)((int16_t)t->ang[1] << 4));
		if (--t->frame == 0) return TASK_END;
		const int16_t vx = t->vel[0], vy = t->vel[1];
		t->pos[0] = (int16_t)(t->pos[0] + vx);
		t->pos[1] = (int16_t)(t->pos[1] + vy);
		if (t->pos[1] >= 0) t->vel[1] = (int16_t)-vy;
		else t->vel[1] = (int16_t)((int16_t)t->gravity + vy);
		t->ang[0] = (int8_t)(t->ang[0] + t->dang[0]);
		t->pos[2] = (int16_t)(t->pos[2] + t->vel[2]);
		t->ang[1] = (int8_t)(t->ang[1] + t->dang[1]);
		return 0;
	}

	// ------------------------------------------------------------------
	// Queue C tasks
	// ------------------------------------------------------------------
	// The pad of a particle model's matrix (+0x22) is what Matrix3x3_MultiplyPSX 0x56C090 copies from its own stack (never
	// written there; its frame is 8-aligned): the word earlier calls left at that depth. The draws of the random turns therefore
	// run with the ORIGINAL's stack layout (RandRotZMul / RandTiltMul: the same calls, esp at the rand call = the original's mod
	// 8), so the word is the same: computeCosine's double temp of RotZ (aligned case), what rand left (other case), a word of
	// the first product's result (tilted pair). The original's alignment: its root runs queue C with the executor entered at
	// the root's entry esp - 0x3C; the port's root records the difference to its own call of the executor (g_qc_delta, ExecQC),
	// added to a dive's entry esp (the dives' rand calls are at entry esp - 0x88 / - 0x78: the same alignment). In the
	// unaligned case (esp = 0 mod 8 at the rand call) the RotZ pad word is below every frame of rand / RotZ: the original reads
	// the high word of the return address its trail draw's Field_Free call pushed there (0x006FD5F2 / 0x006FDC8B: 0x006F; the
	// trail is drawn earlier in the same call on every tick with a burst below 0xC), modelled as that word; on the last dive's
	// tick 0xC (no trail) it is what the code before the task left at its entry esp - 0xA6 (read at the port's entry). On that
	// tick (director counter 0xBD) the ORIGINAL director's camera blend (65's 0x6E2C10, counters 0xB0..0xE1) is the last code
	// to write there: its call of GTE_StoreIR123 0x45E3E0 pushes the return address 0x006E2CA6 at the tasks' entry esp - 0xA8
	// (the director and the dives are run by the executor at the same entry esp). The port's director leaves other words
	// there, so the root writes that return address at the last dive's entry esp - 0xA8 (learnt on its first tick) before
	// running queue C on a tick whose director blended the camera: the last dive then reads what the original reads.
	static uint32_t g_root_entry = 0;   // the root task's entry esp (its return address' slot)
	static uint8_t *g_lastdive_entry = nullptr; // the last dive's entry esp (its return address' slot), as the executor calls it
	static bool g_blend_tick = false;   // the director blended the camera (0x6E2C10) on this tick
	static int32_t g_qc_delta = 0;      // the original's executor entry esp for queue C minus the port's
	static __declspec(naked) int __cdecl ExecQC(TaskQueue *)
	{
		__asm
		{
			mov eax, g_root_entry
			sub eax, 0x3C
			sub eax, esp
			mov g_qc_delta, eax
			mov eax, 0x508420
			jmp eax
		}
	}
	// the original's esp mod 8 at a dive's rand calls (0 / 4), from the dive's entry esp (its return address' slot)
	inline uint32_t OrigAlign(const void *ret_addr) { return ((uint32_t)ret_addr + (uint32_t)g_qc_delta) & 7; }

	// rand(); RotZ(r, m); Matrix3x3_MultiplyPSX(dir, m, m) laid out as the original (0x6FD80B / 0x6FDD3F)
	static __declspec(naked) void __cdecl RandRotZMul(int16_t *m, const void *dir, uint32_t al)
	{
		__asm
		{
			push ebx
			push edi
			push esi
			mov ebx, [esp + 0x10]
			mov edi, [esp + 0x14]
			mov ecx, [esp + 0x18]
			mov esi, esp
			sub esp, 0x10
			and esp, 0xFFFFFFF8
			sub esp, ecx
			mov eax, 0x55CBD2
			call eax
			push ebx
			push eax
			mov eax, 0x7012C0
			call eax
			push ebx
			push ebx
			push edi
			mov eax, 0x56C090
			call eax
			mov esp, esi
			pop esi
			pop edi
			pop ebx
			ret
		}
	}

	// rand(); RotZ(r, RZ); RotX(-0x200, RX); Matrix3x3_MultiplyPSX(RZ, RX, RX); Matrix3x3_MultiplyPSX(dir, RX, out) laid out as
	// the original (0x6FD8D7 / 0x6FDE60)
	static __declspec(naked) void __cdecl RandTiltMul(int16_t *out, const void *dir, uint32_t al)
	{
		__asm
		{
			push ebx
			push edi
			push esi
			mov ebx, [esp + 0x10]
			mov edi, [esp + 0x14]
			mov ecx, [esp + 0x18]
			mov esi, esp
			sub esp, 0x70
			and esp, 0xFFFFFFF8
			sub esp, ecx
			mov eax, 0x55CBD2
			call eax
			lea ecx, [esp + 0x3C]
			push ecx
			push eax
			mov eax, 0x7012C0
			call eax
			lea edx, [esp + 0x24]
			push edx
			push 0xFFFFFE00
			mov eax, 0x701220
			call eax
			lea eax, [esp + 0x2C]
			lea ecx, [esp + 0x2C]
			push eax
			lea edx, [esp + 0x50]
			push ecx
			push edx
			mov eax, 0x56C090
			call eax
			lea ecx, [esp + 0x38]
			push ebx
			push ecx
			push edi
			mov eax, 0x56C090
			call eax
			mov esp, esi
			pop esi
			pop edi
			pop ebx
			ret
		}
	}

	// a dive's trail: prim model 0x1497140 at its frame `m`
	static void TrailDraw(const Mat4x3 *m, uint32_t *cursor)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x78);
		ComposeAffineTransform(&Camera(), m, (Mat4x3 *)h);
		GteSetRotMatrix((const Mat4x3 *)h);
		GteSetTransVector((const Mat4x3 *)h);
		F<uint32_t>(h, 0x3C) = 0x30;
		F<uint32_t>(h, 0x20) = MDL_Trail;
		*cursor = RenderPrimModel(h + 0x20, OTBase() + 0x44, 2, *cursor);
		FieldFree(0x78);
	}

	// `count` particle models along the frame `dir` turned by random Z (tilt: and X -0x200) (kind `kind`), at p
	static void PModelBurst(const Mat4x3 *dir, const int32_t *p, int kind, bool tilt, int count, uint32_t al)
	{
		for (int left = count;;)
		{
			PModel *s = PModelAlloc();
			if (!s) return;
			if (!tilt)
			{
				RandRotZMul(s->m, dir, al);
				if (al == 0) s->pad22 = 0x006F;
			}
			else RandTiltMul(s->m, dir, al);
			s->pos[0] = p[0];
			s->pos[1] = p[1];
			s->pos[2] = p[2];
			s->vel[0] = (int16_t)div6(s->m[1]);
			s->vel[1] = (int16_t)div6(s->m[4]);
			s->kind = (uint8_t)kind;
			s->vel[2] = (int16_t)div6(s->m[7]);
			s->life = 0xD;
			s->fade = 0;
			s->scale[2] = 0x800;
			s->scale[0] = 0x800;
			s->scale[1] = 0x2000;
			if (--left == 0) return;
		}
	}

	// `count` sparkles at p flying along the frame `m` in random directions of its xy plane (z `z`), speed 0x80..0xFF / 0x1000
	static void SparkleBurst(const void *m, const int32_t *p, int32_t count, int16_t z)
	{
		for (Sprite *s = SpriteAlloc(); s;)
		{
			s->pos[0] = (int16_t)p[0];
			s->pos[1] = (int16_t)p[1];
			s->pos[2] = (int16_t)p[2];
			s->scale = 0x800;
			s->life = 0x10;
			s->seq = SEQ_Sparkle;
			const int32_t r = CrtRand();
			s->vel[0] = (int16_t)ComputeCos(r);
			s->vel[1] = (int16_t)ComputeSin(r);
			s->vel[2] = z;
			MatVec(m, s->vel, s->vel);
			const int32_t sp = (CrtRand() & 0x7F) + 0x80;
			F<uint32_t>(s, 0x14) = 0;
			s->vel[0] = (int16_t)(mul32(s->vel[0], sp) >> 12);
			s->vel[1] = (int16_t)(mul32(s->vel[1], sp) >> 12);
			s->vel[2] = (int16_t)(mul32(s->vel[2], sp) >> 12);
			if (--count == 0) break;
			s = SpriteAlloc();
		}
	}

	// 0x6FD580: a dive: the trail model along its direction and sparks thrown sideways while it closes in (ticks 4..7),
	// moving -z each tick; from 6 the dive's layout at the centre + z / 4 with particle bursts (6 / 8), sparkles and the damage
	// wave at 6; ends with the layout
	static uint32_t __cdecl DiveTask(TaskNode *n)
	{
		DiveNode *d = (DiveNode *)n;
		const uint32_t al = OrigAlign(_AddressOfReturnAddress());
		if (d->counter < 0xC)
		{
			FX_HELD(held_note_trail(d);)
			TrailDraw(&d->m, &FrameArena());
			const int16_t c = d->counter;
			if (c > 3 && c < 8)
			{
				int left = 8;
				for (Sprite *s = SpriteAlloc(); s;)
				{
					s->pos[0] = (int16_t)d->m.t[0];
					s->pos[1] = (int16_t)d->m.t[1];
					s->pos[2] = (int16_t)d->m.t[2];
					s->scale = 0x800;
					s->life = 0x10;
					s->seq = SEQ_Sparkle;
					const int32_t r = CrtRand();
					s->vel[0] = (int16_t)ComputeCos(r);
					s->vel[1] = (int16_t)ComputeSin(r);
					s->vel[2] = 0;
					MatVec(&d->m, s->vel, s->vel);
					const int32_t sp = (CrtRand() & 0x200) + 0x200;
					const int32_t fw = CrtRand() & 0xFFF;
					const int16_t vx = s->vel[0];
					s->pos[0] = (int16_t)(s->pos[0] + (int16_t)((mul32(vx, sp) >> 12) + (mul32(d->m.m[0][2], fw) >> 12)));
					const int16_t vy = s->vel[1];
					s->pos[1] = (int16_t)(s->pos[1] + (int16_t)((mul32(d->m.m[1][2], fw) >> 12) + (mul32(vy, sp) >> 12)));
					const int16_t vz = s->vel[2];
					const int32_t dz = (mul32(vz, sp) >> 12) + (mul32(d->m.m[2][2], fw) >> 12);
					F<uint32_t>(s, 0x14) = 0;
					s->vel[0] = (int16_t)(vx >> 6);
					s->vel[1] = (int16_t)(vy >> 6);
					s->pos[2] = (int16_t)(s->pos[2] + (int16_t)dz);
					s->vel[2] = (int16_t)(vz >> 6);
					if (--left == 0) break;
					s = SpriteAlloc();
				}
			}
			d->m.t[1] -= d->m.m[1][2];
			d->m.t[0] -= d->m.m[0][2];
			d->m.t[2] -= d->m.m[2][2];
		}
		const int16_t c = d->counter;
		if (c < 6)
		{
			d->counter++;
			return 0;
		}
		PrimArg a;
		F<uint32_t>(&a, 0) = F<uint32_t>(d, 0xC);
		F<uint32_t>(&a, 8) = F<uint32_t>(d, 0x14);
		F<uint32_t>(&a, 0xC) = F<uint32_t>(d, 0x18);
		F<uint32_t>(&a, 4) = F<uint32_t>(d, 0x10);
		F<uint16_t>(&a, 0x10) = F<uint16_t>(d, 0x1C);
		a.m.t[0] = sdiv_pow2(d->m.m[0][2], 2) + Centre()[0];
		a.m.t[1] = sdiv_pow2(d->m.m[1][2], 2) + Centre()[1];
		a.m.t[2] = sdiv_pow2(d->m.m[2][2], 2) + Centre()[2];
		if (c <= 8 && !(c & 1))
		{
			PModelBurst(&d->m, a.m.t, 0, false, 4, al);
			PModelBurst(&d->m, a.m.t, 1, true, 4, al);
		}
		if (d->counter == 6)
		{
			SparkleBurst(&d->m, a.m.t, 0x18, 0x2000);
			DamageWave();
		}
		FX_HELD(held_note_dive_play(d, &a.m);)
		ComposeAffineTransform(&Camera(), &a.m, &a.m);
		a.morph = TexBase() + 0x1888C;
		a.depth = 0;
		a.cursor = FrameArena();
		const int left = prim::play((prim::Layout *)d->layout, PartCallback, (int)&a, 0);
		FrameArena() = a.cursor;
		if (left == 0) return TASK_END;
		d->counter++;
		return 0;
	}

	// 0x6FDC20: the last dive: the trail model (module arena) closing in by half its z axis a tick; on ticks 3 / 7 / 11
	// particle bursts at the centre spread along the dive's x / y (kinds 2..4), on 3 a tilted burst and sparkles; the frame
	// turns X 0x400 on 3; 13 ticks
	static uint32_t __cdecl LastDiveTask(TaskNode *n)
	{
		g_lastdive_entry = (uint8_t *)_AddressOfReturnAddress();
		const uint16_t entry_word = *(const uint16_t *)(g_lastdive_entry - 0xA6);
		DiveNode *d = (DiveNode *)n;
		const uint32_t al = OrigAlign(_AddressOfReturnAddress());
		if (d->counter < 0xC)
		{
			FX_HELD(held_note_trail(d);)
			TrailDraw(&d->m, &Cursor());
			d->m.t[0] += -((d->m.m[0][2] - (d->m.m[0][2] >> 15)) >> 1);
			d->m.t[1] += -((d->m.m[1][2] - (d->m.m[1][2] >> 15)) >> 1);
			d->m.t[2] += -((d->m.m[2][2] - (d->m.m[2][2] >> 15)) >> 1);
		}
		if (d->counter >= 3)
		{
			Mat4x3 l;
			RotY(0, &l);
			l.t[2] = Centre()[2];
			const int16_t c = d->counter;
			const uint32_t ph = (uint32_t)((int32_t)c - 3);
			l.t[0] = Centre()[0];
			l.t[1] = Centre()[1];
			if (ph <= 9 && !(c & 3))
			{
				const int kind = (int)((ph >> 2) + 2);
				for (int left = 0x30;;)
				{
					PModel *s = PModelAlloc();
					if (!s) break;
					RandRotZMul(s->m, &l, al);
					if (al == 0) s->pad22 = (int16_t)(c < 0xC ? 0x006F : entry_word);
					const int32_t spread = (CrtRand() & 0x7FF) - 0x400;
					const int32_t mx = s->m[1];
					s->pos[0] = sdiv_pow2(mul32(mx, spread), 12) + l.t[0];
					const int32_t my = s->m[4];
					s->pos[1] = sdiv_pow2(mul32(my, spread), 12) + l.t[1];
					const int32_t mz = s->m[7];
					s->life = 0xD;
					s->fade = 0;
					s->pos[2] = sdiv_pow2(mul32(mz, spread), 12) + l.t[2];
					s->kind = (uint8_t)kind;
					s->scale[1] = 0x2000;
					s->vel[0] = (int16_t)div6(mx);
					s->vel[1] = (int16_t)div6(my);
					s->scale[2] = 0x800;
					s->vel[2] = (int16_t)div6(mz);
					s->scale[0] = 0x800;
					if (--left == 0) break;
				}
			}
			if (d->counter == 3)
			{
				PModelBurst(&l, l.t, 1, true, 0x10, al);
				SparkleBurst(&l, l.t, 0x10, 0x2000);
				RotX(0x400, &d->m);
			}
		}
		const int16_t c = d->counter;
		d->counter = (int16_t)(c + 1);
		return c < 0xC ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// The director
	// ------------------------------------------------------------------
	static void SetKeys(int16_t *k, int32_t a0, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5)
	{
		k[0] = (int16_t)a0; k[1] = (int16_t)a1; k[2] = (int16_t)a2; k[3] = (int16_t)a3; k[4] = (int16_t)a4; k[5] = (int16_t)a5;
	}

	// 0x6FBDB0: Angelo's animation steps (the next one started at its end)
	static void AnimStep(uint8_t *c)
	{
		if (BattleReadAnimation(c + 0x60, c + 0x6C) == 1)
		{
			FX_HELD(held_note_restart();)
			PreBattleReadAnimation(c + 0x60, c + 0x6C, c[0x6C]);
		}
	}

	// the rise / fall: Rinoa's height = max z - v + 0x1770 / 0x3A98 (+0x20, +0x5C), kept in the director +0x90
	static void RinoaHeight(DirectorNode *d, int32_t v, int32_t base)
	{
		*(int16_t *)(Caster() + 0x20) = (int16_t)(d->centre[2] - (int16_t)v + base);
		*(int32_t *)(Caster() + 0x5C) = *(const int16_t *)(Caster() + 0x20);
		F<int32_t>(d, 0x90) = *(const int32_t *)(Caster() + 0x5C);
	}

	// 0x6FAAA8: prim model 0x1497140 at the centre scaled by g (4.12), sinking as it grows (+ 0x3A98 z)
	static void GrowDraw(const DirectorNode *d, int32_t g)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x9C);
		Mat4x3 *m = (Mat4x3 *)(h + 0x10);
		RotY(0, m);
		F<int32_t>(h, 8) = g;
		F<int32_t>(h, 4) = g;
		F<int32_t>(h, 0) = g;
		Scale3DMatrix(m, (const int32_t *)h);
		m->t[0] = d->centre[0];
		m->t[1] = d->centre[1];
		m->t[2] = d->centre[2] - sdiv_pow2(g * 13500, 12) + 0x3A98;
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		F<uint32_t>(h, 0x50) = 0;
		F<uint32_t>(h, 0x48) = 0;
		F<uint32_t>(h, 0x30) = MDL_Trail;
		F<uint32_t>(h, 0x4C) = 0;
		Cursor() = RenderPrimSet(h + 0x30, OTBase() + 0x44, 2, Cursor());
		FieldFree(0x9C);
	}

	// 0x6FAD82: the landing's three prim models at p: 0x149D0F8 (uv step (lt & 3) x 0x20; lt 0xB..0xE: blended, fade `fade`),
	// 0x1499FA8 turned Z `angle`, 0x149C730 blended with fade g
	static void LandingDraw(uint32_t lt, const int16_t *p, int32_t angle, int32_t fade, int32_t g)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x8C);
		Mat4x3 *m = (Mat4x3 *)h;
		RotY(0, m);
		m->t[0] = p[0];
		m->t[1] = p[1];
		m->t[2] = p[2];
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		uint8_t *pr = h + 0x20;
		F<int16_t>(h, 0x4A) = 0x100;
		F<int16_t>(h, 0x48) = 0x100;
		F<int16_t>(h, 0x50) = 0x100;
		F<int16_t>(h, 0x46) = 0;
		F<int16_t>(h, 0x44) = 0;
		F<int16_t>(h, 0x4C) = 0;
		F<int16_t>(h, 0x40) = 0;
		F<uint32_t>(h, 0x28) = 0;
		F<uint32_t>(pr, 0) = 0x149D0F8;
		F<int16_t>(h, 0x4E) = 0x80;
		F<int16_t>(h, 0x52) = 0x80;
		F<int16_t>(h, 0x42) = (int16_t)((lt & 3) << 5);
		F<uint32_t>(h, 0x3C) = 0x30;
		if (lt - 0xB < 4)
		{
			F<uint32_t>(h, 0x3C) = 0xF0;
			F<int32_t>(h, 0x2C) = fade;
		}
		F<int32_t>(h, 0x38) = -0x40;
		Cursor() = RenderPrimSet(pr, OTBase() + 0x44, 2, Cursor());
		RotZ(angle, m);
		m->t[0] = p[0];
		m->t[1] = p[1];
		m->t[2] = p[2];
		ComposeAffineTransform(&Camera(), m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		F<uint32_t>(h, 0x40) = 0;
		F<uint32_t>(pr, 0) = 0x1499FA8;
		F<int32_t>(h, 0x38) = -0x40;
		F<uint32_t>(h, 0x3C) = 0x30;
		Cursor() = RenderPrimSet(pr, OTBase() + 0x44, 2, Cursor());
		F<uint32_t>(h, 0x3C) = 0xF0;
		F<int32_t>(h, 0x2C) = g;
		F<uint32_t>(pr, 0) = 0x149C730;
		F<int32_t>(h, 0x38) = -0x40;
		Cursor() = RenderPrimSet(pr, OTBase() + 0x44, 2, Cursor());
		FieldFree(0x8C);
	}

	// the director's camera on counter `counter` (Rinoa-relative from her entity position as the tick set it)
	static void DirectorCamera(int32_t counter)
	{
		const int16_t *ctr = Centre();
		const uint32_t cs = (uint32_t)counter;
		int16_t ka[6], kb[6];
		if (cs < 4)
		{
			if (cs == 0)
			{
				CameraArmReturn();
				var<int16_t>(0x1D8E038) = 0x120;
				var<int16_t>(0x1D977A2) = 0;
			}
			SetKeys(ka, ctr[0] - 0x169, -0x2C1, ctr[2] + 0x12C5, ctr[0] + 0x45A, -0x5AF, ctr[2] - 0x34F);
			SetKeys(kb, ctr[0] - 0xA3A, 0x4F1, ctr[2] + 0xBB5, ctr[0] + 0x45A, -0x5AF, ctr[2] - 0x34F);
			CameraKeys(ka, kb, 0, 0, (int32_t)cs, 0x16);
		}
		else if (cs < 0x15)
		{
			const uint8_t *e = Caster();
			var<uint32_t>(0xB8B7F8) = *(const uint32_t *)(e + 0x1C);
			const uint32_t z = *(const uint32_t *)(e + 0x20);
			var<uint32_t>(0xB8B7FC) = z;
			var<uint32_t>(0xB8B7F4) = z;
			var<int16_t>(0xB8B7FA) = (int16_t)(var<int16_t>(0xB8B7FA) - 0x2A3);
			var<int16_t>(0xB8B7FC) = (int16_t)(var<int16_t>(0xB8B7FC) - 0x1F4);
			var<int16_t>(0xB8B7F4) = (int16_t)(var<int16_t>(0xB8B7F4) - 0x1F4);
			var<uint32_t>(0xB8B7F0) = *(const uint32_t *)(e + 0x1C);
			const uint32_t q = mulhi_u((cs - 4) << 12, 0xBA2E8BA3) >> 4;
			var<int16_t>(0xB8B7F2) = (int16_t)(var<int16_t>(0xB8B7F2) - 0x20B);
			var<int16_t>(0xB8B7F0) = (int16_t)(var<int16_t>(0xB8B7F0) + sdiv_pow2((int32_t)q, 2) + 1000);
		}
		uint32_t cc = (uint32_t)(counter - 0x15);
		if (cc < 0xE)
		{
			SetKeys(ka, ctr[0] - 0xBD, -0x6F8, ctr[2] + 0x187, ctr[0] - 0x54, -0xC4, ctr[2] + 0x17B5);
			SetKeys(kb, ctr[0] - 0x86, -0x295, ctr[2] + 0x276, ctr[0] + 0xE2, -0x155, ctr[2] + 0xC40);
			CameraKeys(ka, kb, 0, 0, (int32_t)cc, 0xE);
		}
		cc = (uint32_t)(counter - 0x31);
		if (cc < 0x19)
		{
			SetKeys(ka, ctr[0] - 0x317, ctr[1] - 0x1E0, ctr[2] - 0x6C3, ctr[0] - 0x96C, ctr[1] - 0xA27, ctr[2] - 0x1569);
			SetKeys(kb, ctr[0] + 0xCC, ctr[1] - 0x491, ctr[2] - 0xFC4, ctr[0] + 0x11F, ctr[1] - 0x77F, ctr[2] - 0x182D);
			CameraKeys(ka, kb, 0, 0, (int32_t)cc, 0x19);
		}
		cc = (uint32_t)(counter - 0x5F);
		if (cc < 0xF)
		{
			SetKeys(ka, ctr[0] + 0xCC, ctr[1] - 0x491, ctr[2] - 0xFC4, ctr[0] + 0x11F, ctr[1] - 0x77F, ctr[2] - 0x182D);
			SetKeys(kb, ctr[0] - 0x529, ctr[1] - 0xB7, ctr[2] - 0x51E, ctr[0] + 0x1AB4, ctr[1] - 0x1AE, ctr[2] + 0x767);
			CameraKeys(ka, kb, 0, 0, (int32_t)cc, 0xF);
		}
		cc = (uint32_t)(counter - 0x65);
		if (cc < 0x3C)
		{
			const int32_t t = (int32_t)(mulhi_u(cc << 12, 0x88888889) >> 5);
			const int32_t sn = ComputeSin(t);
			CameraOrbit(mul32(sn, -40) >> 12, -0x30, 0x1000);
		}
		if (counter == 0xA1)
		{
			var<int16_t>(0xB8B7F8) = (int16_t)(ctr[0] - 0x158);
			var<int16_t>(0xB8B7FA) = (int16_t)(ctr[1] - 0x5D);
			var<int16_t>(0xB8B7FC) = (int16_t)(ctr[2] + 0xCF1);
			var<int16_t>(0xB8B7F0) = (int16_t)(ctr[0] - 0x196);
			var<int16_t>(0xB8B7F2) = (int16_t)(ctr[1] - 0x57);
			var<int16_t>(0xB8B7F4) = (int16_t)(ctr[2] + 0x51C);
		}
		cc = (uint32_t)(counter - 0xB0);
		if (cc < 0x32)
		{
			const uint32_t e = cc - 6;
			const int32_t t = e > 0x3E8 ? 0 : (int32_t)(mulhi_u(e << 10, 0xBA2E8BA3) >> 5);
			SetKeys(ka, ctr[0] + 0x3F1, ctr[1] - 0x2B9, ctr[2] + 0x8EF, ctr[0] + 0x188F, ctr[1] - 0x498, ctr[2] + 0x1B1B);
			SetKeys(kb, ctr[0] - 0x10A8, ctr[1] - 0xE1B, ctr[2] - 0xC50, ctr[0] + 0x1C7B, ctr[1] + 0x128C, ctr[2] + 0x167D);
			CameraBlend(ka, kb, 0, 0, ComputeSin(ComputeSin(t) >> 2));
			g_blend_tick = true;
		}
	}

	static uint32_t __cdecl Director(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		uint8_t *angelo = d->angelo;
		FX_HELD(held_note_director(d);)
		int16_t c = d->counter;
		if (c == 0x1E) QueueChainTransformation(Caster(), 2);
		else if (c == 0x20) QueueChainTransformation(Caster(), 3);
		else if (c == 0xDE) QueueChainTransformation(Caster(), 4);
		c = d->counter;
		if (c == 7) SetAnim(angelo, 1);
		else if (c == 0x1E) SetAnim(angelo, 2);
		else if (c == 0x20) SetAnim(angelo, 3);
		// 0..0x15: the rise, flashes / debris from 3
		uint32_t s = (uint32_t)(int32_t)d->counter;
		if (s < 0x16)
		{
			const uint32_t q = mulhi_u(s << 12, 0xBA2E8BA3) >> 4;
			if (s == 0) EnemyFlag4On();
			RinoaHeight(d, sdiv_pow2((int32_t)(q * 5000), 12), 0x1770);
			if (s >= 3)
			{
				switch (s % 6)
				{
				case 0: FlashAt(angelo, (const int16_t *)0x14AF460, 0); break;
				case 1: FlashAt(angelo, (const int16_t *)0x14AF468, 0); break;
				case 3: FlashAt(angelo, (const int16_t *)0x14AF470, 4); break;
				case 4: FlashAt(angelo, (const int16_t *)0x14AF478, 4); break;
				default: break;
				}
				if (s % 7 == 0) FlashAt(Caster(), (const int16_t *)(0x14AF480 + ((s / 7) & 1) * 8), 0);
			}
		}
		// 0x1E..0x21: Rinoa turns
		s = (uint32_t)((int32_t)d->counter - 0x1E);
		if (s <= 3)
		{
			*(int16_t *)(Caster() + 0x1E) = (int16_t)mul32((int32_t)s + 1, -275);
			*(int32_t *)(Caster() + 0x58) = *(const int16_t *)(Caster() + 0x1E);
			F<int32_t>(d, 0x8C) = *(const int32_t *)(Caster() + 0x58);
		}
		// 0x16..0x31: the rise goes on
		s = (uint32_t)((int32_t)d->counter - 0x16);
		if (s < 0x1C)
		{
			const uint32_t q = (s << 12) / 28;
			if (s == 0)
			{
				InitTaskQueuePool(QB(), (void *)(TexBase() + 0xC40), 0x30, 0x20);
				RestoreEnemyFlags();
			}
			RinoaHeight(d, sdiv_pow2((int32_t)(q * 12000), 12), 0x1770);
		}
		// 0x4B..0x5E: a layout at the centre
		s = (uint32_t)((int32_t)d->counter - 0x4B);
		if (s < 0x14)
		{
			if (s == 0) DecodeModelPrimLayout(0x14A6370, d->layout, 0x68);
			Play(d->layout, d->centre[0], d->centre[1], d->centre[2] + 0x3A98, 0, &Cursor());
		}
		// 0x57..0x65: a growing prim model
		s = (uint32_t)((int32_t)d->counter - 0x57);
		if (s < 0xF)
		{
			const int32_t g = (int32_t)(mulhi_u(s << 12, 0x88888889) >> 3);
			FX_HELD(held_note_grow(g);)
			GrowDraw(d, g);
		}
		// 0x66..0x93: seven dives
		s = (uint32_t)((int32_t)d->counter - 0x66);
		if (s < 0x3C && s <= 0x2D)
		{
			switch (*(const uint8_t *)(0x6FBB50 + s))
			{
			case 0: ModelsOn() = 1; SpritesOn() = 1; DiveSpawn(ORIG_DiveTask, 0, 0, 6); break;
			case 1: DiveSpawn(ORIG_DiveTask, 0x600, 0x600, 6); break;
			case 2: DiveSpawn(ORIG_DiveTask, 0xE00, 0xE00, 6); break;
			case 3: DiveSpawn(ORIG_DiveTask, 0xE00, 0x600, 6); break;
			case 4: DiveSpawn(ORIG_DiveTask, 0x200, 0xA00, 6); break;
			case 5: DiveSpawn(ORIG_DiveTask, 0xA00, 0, 6); break;
			default: break;
			}
		}
		// 0x20..0x23: a layout at Angelo's bone 0x13
		s = (uint32_t)((int32_t)d->counter - 0x20);
		if (s < 4)
		{
			if (s == 0) DecodeModelPrimLayout(0x14A09A0, d->layout, 0x70);
			int16_t p[4];
			GetEffectSpawnPosition(angelo, 0x13, 0, p);
			Play(d->layout, p[0], p[1], p[2], -0x200, &Cursor());
		}
		// 0xA2..0xB0: the landing, three prim models at Angelo's bone 0x13
		const uint32_t lt = (uint32_t)((int32_t)d->counter - 0xA2);
		if (lt < 0xF)
		{
			const int32_t g = (int32_t)(mulhi_u(lt << 12, 0x88888889) >> 3);
			if (lt == 0)
			{
				*(int16_t *)(Caster() + 0x1E) = d->centre[1];
				*(int32_t *)(Caster() + 0x58) = *(const int16_t *)(Caster() + 0x1E);
				F<int32_t>(d, 0x8C) = *(const int32_t *)(Caster() + 0x58);
				*(uint16_t *)Caster() &= 0xFFF3;
				*(uint16_t *)angelo &= 0xFFF3;
				ModelsOn() = 0;
				SpritesOn() = 0;
			}
			RinoaHeight(d, sdiv_pow2(g * 13500, 12), 0x3A98);
			int16_t p[4];
			GetEffectSpawnPosition(angelo, 0x13, 0, p);
			FX_HELD(held_note_landing(lt, p, g);)
			LandingDraw(lt, p, (int32_t)((lt & 0xFF) << 8), shl32((int32_t)(lt - 0xB) + 1, 10), g);
		}
		// 0xB1..0xE2: the last dive, the eighth wave, a layout at the centre
		s = (uint32_t)((int32_t)d->counter - 0xB1);
		if (s < 0x32)
		{
			if (s == 0)
			{
				*(uint8_t *)Caster() |= 0xC;
				angelo[0] |= 0xC;
				DecodeModelPrimLayout(0x14A6600, d->layout, 0x218);
				SpritesInit();
				ModelsOn() = 1;
				SpritesOn() = 1;
				DiveSpawn(ORIG_LastDiveTask, 0, 0, 2);
			}
			else if (s >= 3)
			{
				if (s == 3) DamageWave();
				Play(d->layout, d->centre[0], d->centre[1], d->centre[2], 0, &Cursor());
			}
		}
		// 0x25..0xE1: the sky and the stars
		s = (uint32_t)((int32_t)d->counter - 0x25);
		if (s < 0xBD)
		{
			if (s == 0) DecodeModelPrimLayout(0x14A9FF0, d->layout2, 0xEC);
			Play(d->layout2, d->centre[0], 0, d->centre[2], 0, &FrameArena());
			FX_HELD(held_note_stars();)
			FrameArena() = Stars(0, Centre()[0], 0, Centre()[2], 0x10000, FrameArena());
		}
		// 0x24..0x4A: the capture, the white flash, the shards
		s = (uint32_t)((int32_t)d->counter - 0x24);
		if (s < 0x27)
		{
			if (s == 0)
			{
				Capture(TexBase() + 0x14404);
				FadeTile(0, 0xFF, 0xFF, 0xFF, 0, 1, 7);
			}
			else if (s < 8) FadeTile((int32_t)s, 0xFF, 0xFF, 0xFF, 0, 1, 7);
			FX_HELD(held_note_shards();)
			ShardsDraw();
		}
		if ((uint32_t)((int32_t)d->counter - 0x24) < 0xBE) ScreenFeedback();
		// Angelo
		if ((uint16_t)d->counter < 0x10F)
		{
			if (!(angelo[0] & 4))
			{
				FX_HELD(held_note_angelo(d);)
				Cursor() = DrawCreatureModel(angelo, TexBase() + 0x1888C, Cursor(), CursorEnd());
			}
			if (!(angelo[0] & 8)) AnimStep(angelo);
		}
		// 0xC5..0x102: white fade; 0xE3 put back
		s = (uint32_t)((int32_t)d->counter - 0xC5);
		if (s < 0x3E)
		{
			FadeTile((int32_t)s, 0xFF, 0xFF, 0xFF, 0x1E, 1, 0x1F);
			if (s == 0x1E)
			{
				var<uint16_t>(0x1D8E038) = var<uint16_t>(0x1D977A0);
				var<uint32_t>(0xB8B7F8) = var<uint32_t>(0xB8B808);
				var<uint32_t>(0xB8B7F0) = var<uint32_t>(0xB8B800);
				var<uint32_t>(0xB8B7FC) = var<uint32_t>(0xB8B80C);
				var<uint32_t>(0xB8B7F4) = var<uint32_t>(0xB8B804);
				*(uint32_t *)(Caster() + 0x1C) = d->rinoa_xy;
				*(uint32_t *)(Caster() + 0x20) = d->rinoa_z;
				*(uint16_t *)Caster() &= 0xFFF3;
				RestorePartyFlags();
				StageModelsBack();
			}
		}
		// 0xEB..0x102: falling sparks (128 / 64 / ... per tick, 4 at the end)
		s = (uint32_t)((int32_t)d->counter - 0xEB);
		if (s < 0x18)
		{
			uint32_t cnt;
			if (s == 0)
			{
				SpritesInit();
				cnt = 1u << 7;
			}
			else if (s >= 0x16) cnt = 4;
			else if (s < 0x10) cnt = 1u << ((7 - (s >> 1)) & 31);
			else cnt = 0;
			while (cnt-- != 0)
			{
				Sprite *p = SpriteAlloc();
				if (!p) break;
				p->pos[0] = (int16_t)(sdiv_pow2(mul32(CrtRand(), 8000), 15) - 0xFA0);
				p->pos[1] = (int16_t)sdiv_pow2(mul32(CrtRand(), -4000), 15);
				const int32_t r = CrtRand();
				p->scale = 0x400;
				p->life = 0xE;
				p->seq = SEQ_Spark;
				p->pos[2] = (int16_t)(sdiv_pow2(mul32(r, 10000), 15) - 0x1388);
				p->vel[0] = (int16_t)((CrtRand() & 0x3F) - 0x20);
				p->vel[1] = (int16_t)((CrtRand() & 0x3F) - 0x20);
				const int32_t vz = (CrtRand() & 0x3F) - 0x20;
				p->vel[2] = (int16_t)vz;
				p->acc[2] = (int8_t)(-vz >> 6);
				p->acc[0] = (int8_t)(-(int32_t)p->vel[0] >> 6);
				p->acc[1] = (int8_t)(-(int32_t)p->vel[1] >> 6);
			}
		}
		if (ModelsOn())
		{
			FX_HELD(held_note_particles(0);)
			PModelsDraw();
		}
		if (SpritesOn())
		{
			FX_HELD(held_note_particles(1);)
			SpritesDraw();
		}
		SpritesUpdate();
		PModelsUpdate();
		// stream loads (the director waits for the loader at 0x24 / 0x65 / 0xAD)
		const uint32_t buf = TexBase() + 0xBC04;
		c = d->counter;
		if (c == 0)
		{
			StreamLoad(0x2F6, buf, 1);
			StreamLoad(0x2F7, TexBase() + 0xBC04, 1);
			StreamLoad(0x2F8, TexBase() + 0xBC04, 5);
		}
		else if (c == 0x24)
		{
			if (StreamBusy()) return 0;
		}
		else if (c == 0x4A)
		{
			QueueTIMUpload(buf);
			QueueTIMUpload(TexBase() + 0xDE24);
			QueueTIMUpload(TexBase() + 0x10044);
			QueueTIMUpload(TexBase() + 0x11264);
		}
		else if (c == 0x4C)
		{
			StreamLoad(0x2F9, buf, 1);
			StreamLoad(0x2FA, TexBase() + 0xBC04, 1);
			StreamLoad(0x2FB, TexBase() + 0xBC04, 1);
		}
		else if (c == 0x65)
		{
			if (StreamBusy()) return 0;
			StreamLoad(0x2FC, buf, 5);
		}
		else if (c == 0xAD)
		{
			if (StreamBusy()) return 0;
			QueueTIMUpload(buf);
			QueueTIMUpload(TexBase() + 0x10024);
		}
		else if (c == 0xE3)
		{
			StreamLoad(0x168, buf, 1);
			StreamLoad(0x169, TexBase() + 0xBC04, 1);
			StreamLoad(0x16A, TexBase() + 0xBC04, 1);
		}
		StreamPump();
		// sounds
		c = d->counter;
		if (c == 0) BdPlaySE((const void *)0x14913F0, 1, 0x80);
		else if (c == 0x32) BdPlaySE((const void *)0x14913F4, 1, 0x80);
		else if (c == 0xA2) BdPlaySE((const void *)0x14913F8, 1, 0x80);
		if (d->counter == 0x32)
		{
			RestoreEnemyFlags();
			*(uint8_t *)Caster() |= 0xC;
			angelo[0] |= 0xC;
		}
		// the shards fall
		s = (uint32_t)((int32_t)d->counter - 0x24);
		if (s < 0x27) ShardsUpdate(s < 0xA ? (int32_t)(mulhi_u((s * 5) << 9, 0xCCCCCCCD) >> 3) : 0xA00);
		if ((uint32_t)((int32_t)d->counter - 0x25) < 0xBD) StarsTwinkle();
		// camera
		DirectorCamera(d->counter);
		// next tick; the end waits for the loader
		d->counter++;
		if (d->counter >= 0x10F)
		{
			*(uint32_t *)(Caster() + 0xC) = d->rinoa_ang0;
			*(uint32_t *)(Caster() + 0x10) = d->rinoa_ang1;
			if (StreamPending(2)) return TASK_END;
			d->counter = 0x11F;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// The root task
	// ------------------------------------------------------------------
	static void DirectorInit(DirectorNode *d)
	{
		Memset32(&d->counter, 0, 0x104);
		const uint32_t tex = TexBase();
		int16_t maxx = -0x7FFF, maxy = -0x7FFF, maxz = -0x7FFF, minx = 0x7FFF, miny = 0x7FFF, minz = 0x7FFF;
		ActionData *act = F<ActionData *>(Ctx(), 4);
		const uint32_t count = (uint32_t)act->target_count >> 3;
		uint8_t **tg = (uint8_t **)d->targets;
		for (uint32_t k = 0, off = 0; k < count; k++, off += 0x18, tg += 2)
		{
			act = F<ActionData *>(Ctx(), 4);
			uint8_t *e = Entity(act->targets[off]);
			*tg = e;
			int16_t p[4];
			GetDefaultEffectPosition(e, p);
			if (p[0] < minx) minx = p[0];
			if (p[0] > maxx) maxx = p[0];
			if (p[1] < miny) miny = p[1];
			if (p[1] > maxy) maxy = p[1];
			if (p[2] < minz) minz = p[2];
			if (p[2] > maxz) maxz = p[2];
		}
		act = F<ActionData *>(Ctx(), 4);
		TargetCursor() = act->targets;
		d->centre[0] = (int16_t)((maxx + minx) / 2);
		d->centre[1] = (int16_t)((miny + maxy) / 2);
		d->centre[2] = maxz;
		uint8_t *e = Caster();
		d->rinoa_xy = *(const uint32_t *)(e + 0x1C);
		d->rinoa_z = *(const uint32_t *)(e + 0x20);
		d->rinoa_ang0 = *(const uint32_t *)(Caster() + 0xC);
		d->rinoa_ang1 = *(const uint32_t *)(Caster() + 0x10);
		*(int16_t *)(Caster() + 0xE) = 0;
		Centre()[0] = d->centre[0];
		Centre()[1] = d->centre[1];
		Centre()[2] = d->centre[2];
		PartyFlag4On();
		*(uint16_t *)Caster() &= 0xFFFB;
		BindExternalAnimSet(Caster(), d->animset, 0x1495530);
		QueueChainTransformation(Caster(), 1);
		uint8_t *angelo = d->angelo;
		BindModelContainer(angelo, d->sections, 0x14913FC);
		SetAnim(angelo, 0);
		RotY(0, angelo + 0x40);
		const int32_t cx = Centre()[0];
		F<int32_t>(d, 0x88) = cx;
		F<int16_t>(d, 0x50) = (int16_t)cx;
		*(int16_t *)(Caster() + 0x1C) = (int16_t)cx;
		F<int32_t>(d, 0x8C) = 0;
		F<int16_t>(d, 0x52) = 0;
		*(int16_t *)(Caster() + 0x1E) = 0;
		const int32_t cz = Centre()[2];
		F<int32_t>(d, 0x90) = cz;
		F<int16_t>(d, 0x54) = (int16_t)cz;
		*(int16_t *)(Caster() + 0x20) = (int16_t)cz;
		PModelsInit();
		SpritesInit();
		ShardsReset();
		int16_t off[4];
		off[0] = 0xA0;
		off[1] = 0x6C;
		off[2] = 0;
		ShardsInit(MDL_Shards, off);
		StreamStateInit((void *)(TexBase() + 0x5E5C));
		// the star dots: 8 groups (colours 0x14AF490 / 0x14AF494, their angles reset) x 128 dots at random directions
		uint8_t *st = (uint8_t *)(TexBase() + 0x7C04);
		for (uint32_t g = STAR_GROUPS + 8; g < 0x14AF518; g += 0x10)
		{
			*(int16_t *)(g + 2) = 0;
			*(int16_t *)g = 0;
			for (int k = 0x80; k != 0; k--, st += 0x10)
			{
				const int32_t pitch = sdiv_pow2(mul32(CrtRand(), 1600), 15) - 0x320;
				const int32_t yaw = (CrtRand() & 0xFFF) - 0x800;
				DirectionFromAngles(pitch, yaw, (int16_t *)st);
				F<int16_t>(st, 6) = (int16_t)ISqrt(mul32(yaw, yaw) + mul32(pitch, pitch));
				const int32_t rr = CrtRand();
				const int32_t dd = ((rr - (rr >> 31)) >> 1) + 0x2EE0;
				F<int16_t>(st, 0) = (int16_t)(mul32(F<int16_t>(st, 0), dd) >> 12);
				F<int16_t>(st, 2) = (int16_t)(mul32(F<int16_t>(st, 2), dd) >> 12);
				F<int16_t>(st, 4) = (int16_t)(mul32(F<int16_t>(st, 4), dd) >> 12);
				F<uint32_t>(st, 8) = *(const uint32_t *)(g - 8);
				st[0xB] = (uint8_t)CrtRand();
				F<uint32_t>(st, 0xC) = *(const uint32_t *)(g - 4);
				const uint8_t sp = st[0xF];
				st[0xF] = (uint8_t)((mul32(CrtRand(), sp) >> 17) + sp);
			}
		}
		(void)tex;
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		FX_HELD(held_note_root();)
		g_root_entry = (uint32_t)_AddressOfReturnAddress();
		const uint32_t tex = TexBase();
		if (r->parity != 0)
		{
			Cursor() = tex + 0x1A88C;
			CursorEnd() = tex + 0x2A88C;
			r->parity = 0;
		}
		else
		{
			Cursor() = tex + 0x2A88C;
			CursorEnd() = tex + 0x3A88C;
			r->parity = 1;
		}
		if (r->counter == 2)
		{
			SaveEntityFlags();
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QA(), (void *)(TexBase() + 0x5A40), 0x41C, 1);
				InitTaskQueuePool(QB(), (void *)(TexBase() + 0xC40), 0x30, 0x20);
				InitTaskQueuePool(QC(), (void *)TexBase(), 0x188, 8);
			}
			DirectorNode *d = (DirectorNode *)AddTaskToQueue(QA(), ORIG_Director);
			if (d) DirectorInit(d);
		}
		int a = (int)n;
		if (r->started)
		{
			g_blend_tick = false;
			a = ExecuteTaskQueue(QA());
			ExecuteTaskQueue(QB());
			// the original director's residue at the last dive's entry esp - 0xA8 (free stack: the executor calls the dives below
			// this frame, at that entry esp)
			volatile uint8_t here = 0;
			if (g_blend_tick && g_lastdive_entry && g_lastdive_entry < (const uint8_t *)&here) *(volatile uint32_t *)(g_lastdive_entry - 0xA8) = 0x006E2CA6;
			ExecQC(QC());
		}
		if (r->started && a == 0) return TASK_END;
		r->counter++;
		return 0;
	}
}

	void register_mag089_wishing_star()
	{
		register_port(ws089::ORIG_RootTask, (void *)ws089::RootTask, "089 RootTask", 89);
		register_port(ws089::ORIG_Director, (void *)ws089::Director, "089 Director", 89);
		register_port(ws089::ORIG_FlashTask, (void *)ws089::FlashTask, "089 FlashTask", 89);
		register_port(ws089::ORIG_DebrisTask, (void *)ws089::DebrisTask, "089 DebrisTask", 89);
		register_port(ws089::ORIG_DiveTask, (void *)ws089::DiveTask, "089 DiveTask", 89);
		register_port(ws089::ORIG_LastDiveTask, (void *)ws089::LastDiveTask, "089 LastDiveTask", 89);
		// 30 fps layer: see mag089_wishing_star_held.inc
		FX_HELD(register_mag089_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag089_wishing_star_held.inc"
#endif
