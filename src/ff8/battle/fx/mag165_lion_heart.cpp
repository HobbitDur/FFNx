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
// Effect 165: Lion Heart (Squall's Renzokuken finisher, MAG_165_LION_HEART). It follows Renzokuken (160) like the other
// finishers (163 Rough Divide, 162 Fated Circle, 164 Blasting Zone).
//
// Structure (file loader 0x5E9450 = the texture file named at 0xD88A94 into 0x23C6874; setup MAG_165 0x5E9470, not a task:
// the texture base 0x23C6944 (Magic_TextureOFF_ToEAX1, also the base of pool C 0x23C5680), the packet arena 0x23C694C
// (sub_571B70), the cast context 0x23C6860, the caster slot 0x23C6870, the first target 0x23C32EC, the node id counter
// 0x23C6940 = 1, the hit result index 0x23C6920 = 0, the root task in its 1-node pool 0x23C34A0 (queue 0x23C34B0), the
// director in the main pool 0x23C5920 (100 x 0x24, queue 0x23C5910), pools A / B free, the TIM upload, the caster's rotation
// words (+0x0C / +0x10 dwords) kept in 0x23C58B0 (never read), the caster's effect bone 0xF1 (0x23C6928), the target centre
// (0x23C6910), the effect frame 0x23C58E0 = the rotation from 0xD886E8 to the horizontal direction caster -> centre at (bone x,
// 0, bone z), and the caster's yaw (entity +0x0E) = that angle (negated when the axis points down): Squall faces the targets):
//   RootTask (0x5F01F0) - packet arena parity (0x23C694C, + 0x8000 on odd ticks; cursor 0x23C6948), the main queue; ends
//     with it.
//   DirectorTask (0x5E9650) - every tick, before the draw-only test: 0x23C6878 = camera x effect frame (never read). When the
//     tick is draw-only it stops there (with only flag 0x200 set it asks the pause query 0x508500 and goes on if >= 0).
//     The choreography (counter):
//     0     sound channels stopped (sub_46B450(-1, 0)), the camera script 0xD88930 on the effect frame (shared camera-script
//           task 0x63E9C0 in the main queue), camera return armed, the animation set 0xD780C8 bound to the caster (out block
//           0x23C5688), the first target's / the caster's / all 7 entities' based positions (+0x1C / +0x20 dwords) kept
//           (0x23C6938 / 0x23C6868 / 0x23C6744 + 0x28 i), the target's anchor 0x23C32F0, his sequence 1
//     1     Trail; 2 character file 0x21C into the texture base; 8 GroundRing; 0xA sound 0xD886D4, RockBurst + Debris +
//           BoneSparks; 0xE texture upload (pause query first); 0xF character file 0x21D; 0x14 TipFlash; 0x18 TipShards; 0x1A
//           TipSmoke; 0x43 screen fade task (0, 1, 8, 0xFF); 0x44 2 TipRings
//     0x4E  sequence 2, BladeSparks; 0x5A the effect frame's translation = the caster's anchor
//     0x5B  HitDirector (15 hits); 0x64 FlatRing at the target
//     0x67  the caster shown (flags &= ~8); 0x6F FlatRing at the caster (bone 0xF0); 0x70..0x74 the caster's y -= 800;
//           0x77 sound 0xD886D8
//     0x8D  Flash (screen flash ramp); 0xA1 WaveSpawner; 0xA2 texture upload (pause query first)
//     0xC9  character file 0x21E, TipFlash2; 0xCA sound 0xD886DC; 0xCF TipRocks; 0xD3 texture upload (pause query first);
//           0xF1 screen fade task (0, 4, 8, 0xFF); 0xF2 the effect frame's x / z = the target centre's
//     0xF3  sequence 3, the caster's and the target's anchors (0x23C58C0 / 0x23C57A8); z step = (2 dz + 0x1130) / 16 when the
//           target is more than 2200 in front (dz < -2200), else 0; 0xF3..0x102 the caster's z += step; 0xF5 Streak;
//           0x120 Final (the last hit); 0x14C the effect frame's translation = the kept caster position
//     0x14D sequence 4, the caster's based position put back (+0x1C / +0x20 from 0x23C6868), screen flash off, stage
//           groups hidden (0x1D98991 + 0x2C k |= 2); 0x14F TargetSink + texture restore task; 0x157 sound 0xD886E0
//     0x173 ends: stage groups hidden, screen flash off.
//   Trail (0x5E9E00, 0x12D ticks) - Renzokuken's blade trail: a 6-entry ring 0x23C6898 (x 0x14) of the weapon point (bone 1)
//     and the tip 666 along the blade; the newest min(6, 0x12E - counter) entries (0x23C3278 / 0x23C32B0) splined by the
//     module's own float cubic spline (0x5EA210: natural spline 0x5EA4B0, segment sampling 0x5EA630, x87 at the game's
//     precision control) into 32 points (0x23C56A8 / 0x23C57B0); the first tip (0x23C58B8), the unit direction tip 1 -> tip 0
//     (0x23C58C8), the first point (0x23C58D8), the unit blade direction (0x23C5900, the hit sparks' normal); 30 gouraud quad
//     pairs fading toward black (Renzokuken's draw, cursor + 0x24 left unused).
//   HitDirector (0x5EC140, draws nothing, 0x6E ticks) - at the counters of table 0xD88718 (8, 0x1C, ..., 0x61) the hit i: its
//     point = caster matrix x offset 0xD88758 + 8 i + the first tip (averaged with the previous hit's from the 5th), kept with
//     the trail direction at 0x23C32F8 + 28 i; HitSparkA, HitSparkB, HitSlashA, HitSlashB, HitShards; before the last hit
//     the first target and its linked entities (+0x8C chain, flag +0 bit 1) get flag 0x1000 and a TargetBounce (duration =
//     the gap to the next hit, 8 for the first; height table 0xD887D0 - 4000); ApplyActionResultToTarget per hit.
//   TargetBounce (0x5ED1A0, draws nothing): the entity's y (+0x1E) = its y at the start + (going up: -70 counter; else
//     sin(counter * 0x400 / duration) x height >> 12).
//   Final (0x5EDE90, called at 0x120): FinalRing, FinalSparkA, FinalShards, FinalFlash, FinalRingSparks; pool C cleared;
//     flag 0x20 set on the caster, the target and its linked entities (each one's old bit kept at 0x23C6738 + 0x28 slot);
//     ApplyActionResultToTarget.
//   TargetSink (0x5EECD0, one per target entity, 16 ticks): the entity hidden (flags |= 4) and drawn by the effect upside
//     down (rotation z word +0x10 = 0x800) through the clipping renderer 0x5EF0C0 (only above the floor plane y = 0: the
//     light matrix row = the plane, its back colour = the distance), its shadow first; y = its anchor y - counter x y / 8
//     (counters 0..8); counter 9: Shake, TargetSparks and TargetRing at the kept target position, flag 0x20 put back; counter
//     16: the based position and rotation put back, flag 0x1000 cleared, the result applied (first target); then shown.
//   Single models (prim-model renderer 0x572200 unless noted): GroundRing (0x5EA8E0, model 0xD879EC through MAG_066's scroll
//     renderer 0x650720, v scroll), TipFlash / TipFlash2 (0x5EB790 / 0x5ED760, models 0xD7F960 / 0xD813B0, camera-facing),
//     TipRing (0x5EBFC0, model 0xD8392C in the effect frame, two), HitSlashA / HitSlashB (0x5ECB40 / 0x5ECCD0, models
//     0xD823E4 / 0xD82B5C turned onto the hit direction), FlatRing (0x5ED300, model 0xD84BE4 on the floor), ScreenWave
//     (0x5ED590, model 0xD7B668 through 0x650720 in view space, OT + 0x4068, colour table 0xD88808), Streak (0x5EDC10, model
//     0xD81B34 in screen space from the tip toward its point), FinalRing (0x5EE0F0, 0xD7D9A0), FinalFlash (0x5EE850, 0xD83E3C,
//     camera-facing, its size gives the radius word 0x23C6936), TargetRing (0x5EFFF0, 0xD83244).
//   Pool A (0x23C34C0, 200 x 0x18, entries tagged with their task's node id +0x0E, 0x23C6940 counting): RockBurst (0x5EAAF0,
//     model 0xD83244), Debris (0x5EAD50, model 0xD85D74), BoneSparks / BladeSparks / TargetSparks (0x5EB070 / 0x5EB410 /
//     0x5EFCE0, sequence 0xD7B1C8), TipShards (0x5EB8D0, model 0xD818A0), TipSmoke (0x5EBBD0, sequence 0xD7B4D4), HitSparkA /
//     FinalSparkA (0x5EC550 / 0x5EE240, sequence 0xD7AFE0), HitSparkB (0x5EC7D0, sequence 0xD7B340); pool B (0x23C4780, 120 x
//     0x20): HitShards (0x5ECE60, model 0xD81620), TipRocks (0x5ED8E0, model 0xD80230), FinalShards (0x5EE4C0, model 0xD81620:
//     its draw loop walks 200 entries, past the pool into the module globals); pool C (at the texture base, 500 x 0x18):
//     FinalRingSparks (0x5EE980, sequence 0xD7B340).
//   Flash (0x5E9D50): screen flash 0x800 + counter x 0x80 for 16 ticks, then the stage groups shown. WaveSpawner (0x5ED4E0):
//     a ScreenWave every 12 ticks. Shake (0x5F0130, 12 ticks): the camera shake word 0x1D97712 = +-(sin(counter * 0x400 / 12 +
//     0x400) x 200 >> 12). RestoreTask (0x5F01B0) - Battle_RestoreDefaultEffectTexture on its tick 1.
// Every task but the root / restore tests the draw-only flags (battle_to_update_flags 0x1D96A9C & 0x201).
// ENTITY WRITES: the caster (entity 0x1D972C0 + 0x9C slot): yaw +0x0E (setup), BS_BindEntityModelFromComData (counter 0), the
// AnimSeq chain starts 1 / 2 / 3 / 4 (counters 0 / 0x4E / 0xF3 / 0x14D), flags +0 &= ~8 (0x67), y +0x1E -= 800 (0x70..0x74),
// z +0x20 += step (0xF3..0x102), +0x1C / +0x20 dwords put back (0x14D), flags |= 0x20 (Final) / bit 0x20 put back
// (TargetSink start); the targets (first target and its linked entities): flags |= 0x1000 (hits), y +0x1E (TargetBounce,
// TargetSink), flags |= 0x20 (Final), flags |= 4 / rotation +0x10 = 0x800 (TargetSink), +0x0C / +0x10 / +0x1C / +0x20 put
// back, flags &= ~0x1000, ~4 (TargetSink end). Stage groups 0x1D98991 (+0x2C x 4) bit 1: cleared by Flash's end, set at
// 0x14D and at the end. Squall's own motion is his sequences (AnimSeq VM, the bound set's 4 sequences, section 1 at +0x2894):
// 1 = animation 0 then animation 1 looping; 2 = animation 2, then animation 3 without pausing while the dash runs (the
// forward offset E5 0F = var2 x frame / frames of animation 3 each frame until var 7F reaches 0, var2 = 3100 - var 0x11:
// Rough Divide's dash), then animation 4, animation 5 looping; 3 = animation 6, animation 7 looping; 4 = animation 8, E5 0F
// = 0, AC 00 (the base model back, chain to the automatic sequence).
// CAMERA: the camera script 0xD88930 (task 0x63E9C0, eye / look-at 0xB8B7F0 / 0xB8B7F8) relative to the effect frame (its
// translation moves at 0x5A, 0xF2, 0x14C); the shake word 0x1D97712 (Shake); the module reads the camera matrix 0x1D97778 and
// the projection word 0x1D8E038.
// Module globals: 0x23C3278..0x23C6950 (see the addresses above; hit records 0x23C32F8 (15 x 28), entity keep blocks
// 0x23C6738 (7 x 0x28: +0 flag bit 0x20, +4 rotation, +0xC based position), the tip point 0x23C6930 (+6 = FinalRingSparks'
// radius), the previous hit point 0x23C6730, the last hit's record copy 0x23C6850, the renderer's camera pointer 0x23C32E8).

#include "mag_common.h"
#include <intrin.h>

namespace ff8fx
{
namespace lion165
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline int32_t &TargetSlot() { return var<int32_t>(0x23C32EC); }
	inline int32_t &CasterSlot() { return var<int32_t>(0x23C6870); }
	inline CastContext *&Context() { return var<CastContext *>(0x23C6860); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x23C6944); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x23C6948); }
	inline uint32_t &PacketArena() { return var<uint32_t>(0x23C694C); }
	inline uint32_t &PoolCBase() { return var<uint32_t>(0x23C5680); }
	inline int32_t &NextId() { return var<int32_t>(0x23C6940); }
	inline int32_t &ResultIndex() { return var<int32_t>(0x23C6920); }
	inline uint32_t &FrameArena() { return var<uint32_t>(0x1D8E054); }
	inline TaskQueue *QRoot() { return (TaskQueue *)0x23C34B0; }
	inline TaskQueue *QMain() { return (TaskQueue *)0x23C5910; }
	static const uint32_t POOL_A = 0x23C34C0, POOL_A_END = 0x23C4780;  // 200 x 0x18
	static const uint32_t POOL_B = 0x23C4780, POOL_B_END = 0x23C5680;  // 120 x 0x20
	static const uint32_t FINAL_SHARDS_END = 0x23C6080;                 // FinalShards' draw loop: 200 x 0x20
	static const uint32_t PTS_BASE = 0x23C3278, PTS_TIP = 0x23C32B0;   // trail spline input (6 x 8 each)
	static const uint32_t SPL_BASE = 0x23C56A8, SPL_TIP = 0x23C57B0;   // trail splines (32 x 8 each)
	static const uint32_t TIP0 = 0x23C58B8, TRAIL_DIR = 0x23C58C8, BASE0 = 0x23C58D8, BLADE_DIR = 0x23C5900;
	static const uint32_t RING = 0x23C6898;                             // trail ring (6 x 0x14)
	static const uint32_t FRAME = 0x23C58E0;                            // effect frame (setup; translation moved by the director)
	static const uint32_t CAM_FRAME = 0x23C6878;                        // camera x effect frame (director, every tick)
	static const uint32_t BIND_OUT = 0x23C5688;
	static const uint32_t HITS = 0x23C32F8;                             // hit records (15 x 28)
	static const uint32_t PREV_HIT = 0x23C6730, LAST_HIT = 0x23C6850;
	static const uint32_t KEEP = 0x23C6738;                             // entity keep blocks (7 x 0x28)
	static const uint32_t TIP = 0x23C6930;                              // blade tip point (x, y, z, radius)
	static const uint32_t BONE_F1 = 0x23C6918;                          // the caster's effect bone 0xF1 (GroundRing / RockBurst / Debris)
	static const uint32_t TARGET_KEPT = 0x23C6938, CASTER_KEPT = 0x23C6868;
	static const uint32_t TARGET_ANCHOR = 0x23C32F0, TARGET_CENTRE = 0x23C6910;
	static const uint32_t CASTER_POS = 0x23C58C0, TARGET_POS = 0x23C57A8; // anchors at counter 0xF3
	static const uint32_t CAM_PTR = 0x23C32E8;                          // the clipping renderer's camera pointer

	static const uint32_t ORIG_Root = 0x5F01F0, ORIG_Director = 0x5E9650, ORIG_Flash = 0x5E9D50, ORIG_Trail = 0x5E9E00,
		ORIG_GroundRing = 0x5EA8E0, ORIG_RockBurst = 0x5EAAF0, ORIG_Debris = 0x5EAD50, ORIG_BoneSparks = 0x5EB070,
		ORIG_BladeSparks = 0x5EB410, ORIG_TipFlash = 0x5EB790, ORIG_TipShards = 0x5EB8D0, ORIG_TipSmoke = 0x5EBBD0,
		ORIG_TipRing = 0x5EBFC0, ORIG_HitDirector = 0x5EC140, ORIG_HitSparkA = 0x5EC550, ORIG_HitSparkB = 0x5EC7D0,
		ORIG_HitSlashA = 0x5ECB40, ORIG_HitSlashB = 0x5ECCD0, ORIG_HitShards = 0x5ECE60, ORIG_TargetBounce = 0x5ED1A0,
		ORIG_FlatRing = 0x5ED300, ORIG_WaveSpawner = 0x5ED4E0, ORIG_ScreenWave = 0x5ED590, ORIG_TipFlash2 = 0x5ED760,
		ORIG_TipRocks = 0x5ED8E0, ORIG_Streak = 0x5EDC10, ORIG_FinalRing = 0x5EE0F0, ORIG_FinalSparkA = 0x5EE240,
		ORIG_FinalShards = 0x5EE4C0, ORIG_FinalFlash = 0x5EE850, ORIG_FinalRingSparks = 0x5EE980, ORIG_TargetSink = 0x5EECD0,
		ORIG_TargetSparks = 0x5EFCE0, ORIG_TargetRing = 0x5EFFF0, ORIG_Shake = 0x5F0130, ORIG_Restore = 0x5F01B0;

	// data (FF8_EN.exe)
	static const uint32_t ANIM_DATA = 0xD780C8, CAMERA_SCRIPT = 0xD88930;
	static const void *const SOUND_A = (const void *)0xD886D4, *const SOUND_B = (const void *)0xD886D8,
		*const SOUND_C = (const void *)0xD886DC, *const SOUND_D = (const void *)0xD886E0;
	static const uint32_t REF_BACK = 0xD886E8;   // (0, 0, -0x1000): the models' reference direction
	static const uint32_t REF_RIGHT = 0xD886F8;  // (0x1000, 0, 0): Streak's
	static const uint32_t REF_UP = 0xD88708;     // (0, -0x1000, 0): FinalShards'
	static const uint32_t HIT_TIMES = 0xD88718;  // 15 (+1) counters of the hits
	static const uint32_t HIT_OFFSETS = 0xD88758;// 15 x SVECTOR offsets in the caster's frame
	static const uint32_t HIT_HEIGHTS = 0xD887D0;// 14 bounce heights (+ 4000)
	static const uint32_t WAVE_COLOURS = 0xD88808;// 18 colour words of ScreenWave
	static const uint32_t CLIP_SLOTS = 0xD88850; // 7 x 8 vertex slots of the clipped triangle's cases
	static const uint32_t SEQ_SPARK = 0xD7B1C8, SEQ_SMOKE = 0xD7B4D4, SEQ_HITA = 0xD7AFE0, SEQ_HITB = 0xD7B340;
	static const uint32_t MODEL_GROUNDRING = 0xD879EC, MODEL_ROCK = 0xD83244, MODEL_DEBRIS = 0xD85D74, MODEL_TIPFLASH = 0xD7F960,
		MODEL_TIPSHARD = 0xD818A0, MODEL_TIPRING = 0xD8392C, MODEL_SLASHA = 0xD823E4, MODEL_SLASHB = 0xD82B5C,
		MODEL_SHARD = 0xD81620, MODEL_FLATRING = 0xD84BE4, MODEL_WAVE = 0xD7B668, MODEL_TIPFLASH2 = 0xD813B0,
		MODEL_TIPROCK = 0xD80230, MODEL_STREAK = 0xD81B34, MODEL_FINALRING = 0xD7D9A0, MODEL_FINALFLASH = 0xD83E3C;
	static const float SPLINE_THREE = 3.0f, SPLINE_SIX = 6.0f; // 0xB6995C / 0xB6B988

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t PauseQuery() { return fn<int32_t (__cdecl *)()>(0x508500)(); }                                          // sub_508500
	inline void StopSounds(int32_t a, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t)>(0x46B450)(a, b); }              // sub_46B450
	inline void CameraScriptStart(uint32_t script, uint32_t frame, void *entity, TaskQueue *q, int32_t n) { fn<void (__cdecl *)(uint32_t, uint32_t, void *, TaskQueue *, int32_t)>(0x63E960)(script, frame, entity, q, n); } // MAG066_CamScript_Start
	inline void ArmCameraReturn() { fn<void (__cdecl *)()>(0x50A730)(); }                                                  // BS_Camera_ArmReturnAfterEffect
	inline void BindEntityModelFromComData(uint8_t *entity, uint32_t out, uint32_t data) { fn<void (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); }
	inline void QueueChainTransformation(uint8_t *entity, int32_t seq) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, seq); }
	inline void CharacterLoad(int32_t id, uint32_t dst) { fn<void (__cdecl *)(int32_t, uint32_t)>(0x508480)(id, dst); }    // BattleFile_CharacterLoad
	inline void QueueTIMUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(0x505E30)(tim); }                           // Battle_QueueTIMUpload_GetEOF
	inline void GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t a, void *out) { fn<void (__cdecl *)(uint8_t *, int32_t, int32_t, void *)>(0x502170)(entity, bone, a, out); }
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }                                    // computeCosine
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void GteMatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }    // GTE_MatrixMultiply (b = a x b)
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DDA0)(r, g, b); } // pre_someCameraWork_45DD60
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
	inline void GteLoadV012(const void *v0, const void *v1, const void *v2) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(v0, v1, v2); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteStoreOTZ(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }   // GTE_ReadOTZ
	inline void GteSetLightMatrix(const void *m) { fn<void (__cdecl *)(const void *)>(0x45DE50)(m); }
	inline void GteMVMVA_LightV0Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }             // MAC = LLM * V0 + BK
	inline void GteMVMVA_LightV1Bk() { fn<void (__cdecl *)()>(0x4607F0)(); }
	inline void GteMVMVA_LightV2Bk() { fn<void (__cdecl *)()>(0x460800)(); }
	inline void GteMVMVA_RotV0Tr2() { fn<void (__cdecl *)()>(0x460830)(); }               // GTE_MVMVA_RotV0_Tr_2
	inline void GteSetBackColorFromTrans(const void *m) { fn<void (__cdecl *)(const void *)>(0x64DDA0)(m); } // BK = m.t
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }   // R = s * I
	inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); } // TR = v
	inline void GteTransFromMAC() { fn<void (__cdecl *)()>(0x64DE40)(); }                   // TR = MAC1..3
	inline void GteWriteControlReg(int32_t value, int32_t reg) { fn<void (__cdecl *)(int32_t, int32_t)>(0x45D7F0)(value, reg); }
	inline void GteWriteDataReg(uint32_t value, int32_t reg) { fn<void (__cdecl *)(uint32_t, int32_t)>(0x45D7C0)(value, reg); }
	inline uint32_t ScrollRender(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x650720)(h, ot, mode, cursor); } // MAG_066_RenderScrollPrimModel
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); } // BattleEntity_DrawShadow
	inline uint32_t OT0() { return var<uint32_t>(0x1D8E04C); }
	inline int16_t ProjectionH() { return var<int16_t>(0x1D8E038); }

	// ------------------------------------------------------------------
	// Node / record layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // root pool, 1 x 0x10 (0x23C34A0)
	{
		TaskNode hdr;
		int16_t c;         // +0x0C
		int16_t pad0E;
	};
	struct Node24 // main pool (0x23C5920), 0x24 bytes
	{
		TaskNode hdr;
		int16_t c;         // +0x0C counter
		int16_t e;         // +0x0E node id (pool tasks) / hit index / entity slot / delay / restore: done
		int16_t w10, w12, w14, w16; // +0x10 position
		int16_t w18;       // +0x18 angle
		int16_t w1A;       // +0x1A angle rate / duration / hit index
		int16_t w1C;       // +0x1C scale
		int16_t w1E;       // +0x1E scale rate
		int16_t w20;       // +0x20 y scale / y
		int16_t w22;       // +0x22 y scale rate / step
	};
	struct PoolE // pools A / C, 0x18 bytes
	{
		int32_t id;        // +0x00 the task's node id, 0 = free
		int16_t life;      // +0x04
		int16_t scale;     // +0x06
		int16_t a[8];      // +0x08..+0x16: per task (see the tasks)
	};
	struct PoolE2 // pool B, 0x20 bytes
	{
		int32_t id;        // +0x00
		int16_t life;      // +0x04
		int16_t scale;     // +0x06
		int16_t a[12];     // +0x08..+0x1E
	};
	struct RingEntry // trail ring, 0x14 bytes
	{
		uint32_t alive;
		uint32_t base[2];  // weapon point (x, y | z, 4th word)
		uint32_t tip[2];   // tip
	};
	struct HitRec // hit records, 28 bytes
	{
		int16_t pos[4];    // +0x00
		int32_t dir[4];    // +0x08 the trail direction at the hit (+0x14: 0x23C58D4)
		uint32_t pad18;
	};
	struct KeepBlock // entity keep blocks, 0x28 bytes
	{
		uint16_t flag20;   // +0x00 the flag bit 0x20 before Final
		uint16_t pad02;
		uint32_t rot[2];   // +0x04 rotation dwords (+0x0C / +0x10) before TargetSink
		uint32_t pos[2];   // +0x0C based position dwords (+0x1C / +0x20) at counter 0
		uint8_t pad14[0x14];
	};
	struct TrailWork   // Trail's Field_Alloc block (0x40)
	{
		uint8_t pad00[8];
		int32_t otz;       // +0x08
		uint32_t flag;     // +0x0C
		uint32_t col0;     // +0x10 the segment's leading colour
		uint32_t col1;     // +0x14 its trailing colour (DPCS)
		uint8_t pad18[4];
		uint32_t base_col; // +0x1C
		int16_t base[4];   // +0x20 the weapon point (4th word never written)
		int16_t tip[4];    // +0x28 the tip (4th word never written)
		int32_t dir[4];    // +0x30 the blade direction (4th word never written)
	};
	struct ModelWork   // the pool tasks' Field_Alloc blocks (0x48 .. 0x78)
	{
		int16_t ang[4];    // +0x00 angles (TipSmoke: +0x08 the sprite's position)
		Mat4x3 m1;         // +0x08 (camera copy / frame)
		Mat4x3 m2;         // +0x28 (sprites: +0x28 direction, +0x38 camera-space position)
		int32_t v[4];      // +0x48 scale vector / axis
		int32_t w[4];      // +0x58
		int32_t x[4];      // +0x68 (FinalShards' scale vector)
	};
	// the clipping renderer's header (TargetSink's 0x1A8-byte block)
	struct ClipHeader
	{
		uint32_t prims;    // +0x00 current primitive list
		uint32_t verts;    // +0x04 transformed vertex buffer (0x1D98B3C)
		uint16_t ntri;     // +0x08
		uint16_t nquad;    // +0x0A
		uint16_t w0C, w0E; // +0x0C
		uint32_t code;     // +0x10 the entity's +0x28 (code / colour word of the packets)
		uint32_t mask;     // +0x14 the parts drawn (entity +0x7C; weapon: all)
		uint8_t pad18[0x14];
		uint32_t clip;     // +0x2C the triangle's crossing edges (1 / 2 / 4)
		uint32_t col;      // +0x30 copy of +0x10
		uint32_t uv0;      // +0x34 the triangle's first uv / clut word
		uint32_t uv1;      // +0x38 its second uv / tpage word
		uint32_t ot;       // +0x3C
		uint32_t shift;    // +0x40
		uint32_t cursor;   // +0x44
		int32_t mac0;      // +0x48
		uint32_t pad4C;
		int32_t otz;       // +0x50
		uint32_t flag;     // +0x54
		uint8_t slot[6][0x10]; // +0x58 vertices v0 / v1 / v2, the edge points 01 / 12 / 20 (+0x0C of slots 3..5: +0x90 /
		                       //       +0xA0 / +0xB0 the crossing flags, +0x94 / +0xA4 / +0xB4 the vertices' plane distances)
		int16_t plane[4];  // +0xB8 the plane normal, +0xBE its distance
		uint8_t padC0[8];
		uint32_t ptr[7][8];// +0xC8 vertex slot pointers of the 7 cases (also the start of the entity matrix copy)
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node24) == 0x24 && sizeof(PoolE) == 0x18 && sizeof(PoolE2) == 0x20, "Lion Heart nodes");
	static_assert(sizeof(RingEntry) == 0x14 && sizeof(HitRec) == 28 && sizeof(KeepBlock) == 0x28, "Lion Heart records");
	static_assert(sizeof(TrailWork) == 0x40 && sizeof(ModelWork) == 0x78 && sizeof(ClipHeader) == 0x1A8, "Lion Heart blocks");

	template<typename T> static inline T &At(uint32_t base, int32_t off) { return *(T *)(base + off); }
	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline uint8_t *Target() { return Entity(TargetSlot()); }
	inline PoolE *EntryA(int32_t i) { return (PoolE *)(POOL_A + 0x18 * i); }
	inline PoolE2 *EntryB(int32_t i) { return (PoolE2 *)(POOL_B + 0x20 * i); }
	inline HitRec *Hit(int32_t i) { return (HitRec *)(HITS + 28 * i); }
	inline KeepBlock *Keep(int32_t slot) { return (KeepBlock *)(KEEP + 0x28 * slot); }

	// first free entry (id 0) of a pool, or -1 (none / index >= max)
	static int32_t FreeSlot(uint32_t pool, uint32_t end, uint32_t stride, int32_t max)
	{
		int32_t i = 0;
		for (uint32_t a = pool; *(const int32_t *)a != 0; i++)
		{
			a += stride;
			if (a >= end) return -1;
		}
		return i >= max ? -1 : i;
	}
	static int32_t FreeA() { return FreeSlot(POOL_A, POOL_A_END, 0x18, 0xC8); }
	static int32_t FreeB() { return FreeSlot(POOL_B, POOL_B_END, 0x20, 0x78); }

	// the entries of a node id freed (a pool task's end)
	static void ClearIdA(int32_t id)
	{
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0x18)
			if (*(const int32_t *)a == id) *(int32_t *)a = 0;
	}

	// node id of a new pool task (post-increment of 0x23C6940)
	static inline int16_t TakeId()
	{
		const int32_t id = NextId();
		NextId() = id + 1;
		return (int16_t)id;
	}

	// task end tests of the counters
	static inline uint32_t EndAt(int16_t c, int16_t limit) { return c >= limit ? TASK_END : 0; }
	static inline uint32_t EndIfNone(int32_t count) { return count != 0 ? 0 : TASK_END; }

	// the index of an entity in the 7 battle entity slots, or 7
	static int32_t EntityIndex(const uint8_t *e)
	{
		int32_t i = 0;
		for (uint32_t a = 0x1D972C0; a != (uint32_t)e; i++)
		{
			a += 0x9C;
			if (a >= 0x1D97704) return 7;
		}
		return i;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag165_lion_heart_held.h"
#endif

namespace ff8fx
{
namespace lion165
{
	// ------------------------------------------------------------------
	// Weapon bone (0x5EA7D0 / 0x5EA750: Renzokuken's 0x5FFF60 / 0x5FFEE0, Blasting Zone's 0x5F0A90 / 0x5F0A10)
	// ------------------------------------------------------------------
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
		Mat4x3 m; // (pad +0x12: ComposeAffineTransform's)
		ComposeAffineTransform((const Mat4x3 *)(entity + 0x40), (const Mat4x3 *)(w + 0x24), &m);
		TransformVectorBy3x3Matrix(&m, dir, dir);
		Normalize(dir, dir);
		MatVec(&m, pos, pos);
		pos[0] = (int16_t)(pos[0] + (int16_t)m.t[0]);
		pos[1] = (int16_t)(pos[1] + (int16_t)m.t[1]);
		pos[2] = (int16_t)(pos[2] + (int16_t)m.t[2]);
	}

	// 0x5EB710: the blade tip point 0x23C6930 = the weapon's bone 1 + 380 along the blade (= Blasting Zone's 0x5F0990)
	static void SetTipPoint()
	{
		int32_t dir[4]; // (4th word never written)
		WeaponBone(Caster(), 1, dir, (int16_t *)TIP);
		var<int16_t>(TIP) = (int16_t)(var<int16_t>(TIP) + (int16_t)(mul32(dir[0], 380) >> 12));
		var<int16_t>(TIP + 2) = (int16_t)(var<int16_t>(TIP + 2) + (int16_t)(mul32(dir[1], 380) >> 12));
		var<int16_t>(TIP + 4) = (int16_t)(var<int16_t>(TIP + 4) + (int16_t)(mul32(dir[2], 380) >> 12));
	}

	// 0x5E9D00 / 0x5E9DB0: the four stage groups' flag bit 1 set / cleared (= Blasting Zone's 0x5F0950 / 0x5F0970)
	static void StageGroupsHide()
	{
		for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) |= 2;
	}
	static void StageGroupsShow()
	{
		for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) &= 0xFD;
	}

	// ------------------------------------------------------------------
	// Trail spline (0x5EA210 / 0x5EA4B0 / 0x5EA630): float maths on the x87 FPU. The game runs with the precision control
	// its CRT sets (_controlfp(_PC_53): 53-bit significands, __setdefaultprecision 0x55F8AE), so each x87 operation rounds
	// to a double exactly like the same IEEE double operation; values kept in x87 registers are doubles here, values the
	// original stores (fstp dword) are floats. __ftol truncates (an out-of-range value gives the integer indefinite, low
	// word 0).
	// ------------------------------------------------------------------
	static inline int16_t Ftol16(double v)
	{
		if (!(v >= -9223372036854775808.0 && v < 9223372036854775808.0)) return 0;
		return (int16_t)(uint16_t)(uint64_t)(int64_t)v;
	}

	// 0x5EA4B0: natural cubic spline second derivatives M of (X, Y), n points (H, D: its 0x44-byte scratch block)
	static void SplineSolve(int32_t n, const float *X, const float *Y, float *M)
	{
		float *H = (float *)FieldAlloc(0x44);
		float *D = H + n;
		*(uint32_t *)&M[n - 1] = 0;
		*(uint32_t *)&M[0] = 0;
		for (int32_t i = 0; i < n - 1; i++)
		{
			const double h = (double)X[i + 1] - (double)X[i];
			H[i] = (float)h;
			D[i + 1] = (float)(((double)Y[i + 1] - (double)Y[i]) / h);
		}
		M[1] = (float)((double)D[2] - (double)D[1]);
		D[1] = (float)(((double)X[2] - (double)X[0]) * 2.0);
		for (int32_t k = 2; k <= n - 2; k++)
		{
			const double t = (double)H[k - 1] / (double)D[k - 1];
			M[k] = (float)(((double)D[k + 1] - (double)D[k]) - t * (double)M[k - 1]);
			D[k] = (float)(((double)X[k + 1] - (double)X[k - 1]) * 2.0 - t * (double)H[k - 1]);
		}
		M[n - 2] = (float)((double)M[n - 2] - (double)H[n - 2] * (double)M[n - 1]);
		for (int32_t j = n - 2; j > 0; j--)
			M[j] = (float)(((double)M[j] - (double)H[j] * (double)M[j + 1]) / (double)D[j]);
		FieldFree(0x44);
	}

	// 0x5EA630: the steps points of segment seg (forward differences of its cubic); out[0] = Y[seg], out[steps - 1] first Y[seg + 1]
	static void SplineEval(const float *X, const float *Y, const float *M, int32_t seg, int32_t steps, float *out)
	{
		const int32_t s = (int16_t)seg;
		const int32_t last = (int16_t)(steps - 1);
		const float yb = Y[s + 1];
		out[last] = yb;
		double y = Y[s];
		out[0] = (float)y;
		if ((int16_t)(steps - 1) <= 1) return;
		const double h = (double)X[s + 1] - (double)X[s];
		const double h2 = h * h;
		const float p = (float)(((double)Y[s] * 2.0 + (double)Y[s + 1]) - ((double)M[s] * 2.0 + (double)M[s + 1]) * h2);
		const double q = ((double)Y[s + 1] * 2.0 + (double)Y[s]) - ((double)M[s + 1] * 2.0 + (double)M[s]) * h2;
		const float nf = (float)last;
		const float a3 = (float)((((((double)p + (double)yb) - q) - y) / (double)nf) / (double)nf / (double)nf);
		const float y3 = (float)(y * (double)SPLINE_THREE);
		const float b2 = (float)(((q - (double)p * 2.0) + (double)y3) / (double)nf / (double)nf);
		double c = ((double)p - (double)y3) / (double)nf - ((double)b2 - (double)a3);
		const double a6 = (double)a3 * (double)SPLINE_SIX;
		const float a6f = (float)a6;
		double e = ((double)b2 - a6) + (double)b2;
		for (int32_t i = 1; i <= last; i++)
		{
			e += (double)a6f;
			c += e;
			y += c;
			out[i] = (float)y;
		}
	}

	// 0x5EA210: total points (stride 8, x / y / z words) of the spline through the n points src into dst
	static void SplineCurve(int32_t n, const int16_t *src, int32_t total, int16_t *dst)
	{
		uint8_t *a = (uint8_t *)FieldAlloc(0x190);
		float *X = (float *)a, *Y = (float *)(a + 0x44), *M = (float *)(a + 0x88), *S = (float *)(a + 0xCC);
		for (int32_t i = 0; i < n; i++)
		{
			X[i] = (float)i;
			Y[i] = (float)src[4 * i];
		}
		for (int32_t k = 0; k < 3; k++)
		{
			if (k)
				for (int32_t i = 0; i < n; i++) Y[i] = (float)src[4 * i + k];
			SplineSolve(n, X, Y, M);
			if (n - 1 > 0)
			{
				const int32_t q = total / (n - 1);
				for (int32_t seg = 0; seg < n - 1; seg++) SplineEval(X, Y, M, seg, q + 1, S + q * seg);
			}
			for (int32_t i = 0; i < total; i++) dst[4 * i + k] = Ftol16((double)S[i]);
		}
		FieldFree(0x190);
	}

	// ------------------------------------------------------------------
	// Clipping model renderer (0x5EF0C0 and its list / clip functions 0x5EF3C0 / 0x5EF910 / 0x5EF9B0 / 0x5EFAF0 / 0x5EFC10;
	// = Shiva's 0x5C50D0 family): a battle model's parts through their bone matrices into the vertex buffer, then the
	// textured triangles / quads above the plane (light matrix row 1 = the plane normal, RBK = its distance: MVMVA LV x BK =
	// the vertices' plane distances): wholly above drawn, crossing ones clipped at the plane.
	// ------------------------------------------------------------------
	// 0x5EFAF0: the point where the edge a -> b crosses the plane, its uv word interpolated
	static void ClipEdge(const int16_t *a, const int16_t *b, const int16_t *plane, int16_t *out)
	{
		const int32_t dx = (int32_t)b[0] - a[0];
		const int32_t dy = (int32_t)b[1] - a[1];
		const int32_t dz = (int32_t)b[2] - a[2];
		int32_t den = (mul32(plane[0], dx) + mul32(plane[1], dy) + mul32(plane[2], dz)) >> 12;
		if (den == 0) den = 1;
		const int32_t num = shl32(plane[3], 12) + mul32(plane[2], a[2]) + mul32(plane[1], a[1]) + mul32(plane[0], a[0]);
		const int32_t t = -(num / den);
		out[0] = (int16_t)((mul32(t, dx) >> 12) + a[0]);
		out[1] = (int16_t)((mul32(t, dy) >> 12) + a[1]);
		out[2] = (int16_t)((mul32(t, dz) >> 12) + a[2]);
		const int32_t au = (uint16_t)a[3] & 0xFF, bu = (uint16_t)b[3] & 0xFF;
		const int32_t av = (uint16_t)a[3] & 0xFF00, bv = (uint16_t)b[3] & 0xFF00;
		const int32_t u = (mul32(bu - au, t) >> 12) + au;
		const int32_t v = (mul32(bv - av, t) >> 12) + av;
		out[3] = (int16_t)((uint16_t)u | (uint16_t)(v & 0xFF00));
	}

	// 0x5EF910: a POLY_FT3 of three vertex slots (uv words from the slots' 4th words)
	static void ClipTriFT3(ClipHeader *h, const int16_t *a, const int16_t *b, const int16_t *c)
	{
		const uint32_t pkt = h->cursor;
		GteLoadV012(a, b, c);
		GteRTPT();
		At<uint32_t>(pkt, 4) = h->col | 0x24000000;
		At<uint32_t>(pkt, 0xC) = h->uv0;
		At<uint32_t>(pkt, 0x14) = h->uv1;
		At<uint32_t>(pkt, 0) = 0x7000000;
		GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x10), (void *)(pkt + 0x18));
		GteAVSZ3();
		At<int16_t>(pkt, 0x14) = b[3];
		At<int16_t>(pkt, 0xC) = a[3];
		At<int16_t>(pkt, 0x1C) = c[3];
		GteStoreOTZ(&h->otz);
		InsertPrimAutoDepth(h->ot + (uint32_t)(h->otz >> h->shift) * 4, (void *)pkt);
		h->cursor = pkt + 0x20;
	}

	// 0x5EFC10: a POLY_FT4 of four vertex slots
	static void ClipQuadFT4(ClipHeader *h, const int16_t *a, const int16_t *b, const int16_t *c, const int16_t *d)
	{
		const uint32_t pkt = h->cursor;
		GteLoadV012(a, b, c);
		GteRTPT();
		At<uint32_t>(pkt, 4) = h->col | 0x2C000000;
		At<uint32_t>(pkt, 0xC) = h->uv0;
		At<uint32_t>(pkt, 0x14) = h->uv1;
		At<uint32_t>(pkt, 0) = 0x9000000;
		GteReadSXY012Split((void *)(pkt + 8), (void *)(pkt + 0x10), (void *)(pkt + 0x18));
		GteLoadV0(d);
		GteRTPS();
		At<int16_t>(pkt, 0x14) = b[3];
		At<int16_t>(pkt, 0xC) = a[3];
		At<int16_t>(pkt, 0x1C) = c[3];
		At<int16_t>(pkt, 0x24) = d[3];
		GteReadSXY2((void *)(pkt + 0x20));
		GteAVSZ4();
		GteStoreOTZ(&h->otz);
		InsertPrimAutoDepth(h->ot + (uint32_t)(h->otz >> h->shift) * 4, (void *)pkt);
		h->cursor = pkt + 0x28;
	}

	static inline int16_t *Slot(ClipHeader *h, int32_t k) { return (int16_t *)h->slot[k]; }
	static inline int32_t &SlotWord(ClipHeader *h, int32_t k, int32_t off) { return *(int32_t *)(h->slot[k] + off); }
	static inline const int16_t *SlotPtr(const ClipHeader *h, int32_t f, int32_t i) { return (const int16_t *)h->ptr[f][i]; }

	// 0x5EF9B0: a triangle crossing the plane: the crossing edges' points, then the case's triangle or quad above it
	static void ClipTriangle(ClipHeader *h)
	{
		if (SlotWord(h, 3, 8)) ClipEdge(Slot(h, 0), Slot(h, 1), h->plane, Slot(h, 3));
		if (SlotWord(h, 4, 8)) ClipEdge(Slot(h, 1), Slot(h, 2), h->plane, Slot(h, 4));
		if (SlotWord(h, 5, 8)) ClipEdge(Slot(h, 2), Slot(h, 0), h->plane, Slot(h, 5));
		const uint32_t f = h->clip;
		switch (f)
		{
		case 1: case 2: case 4:
			if (*(const int32_t *)((const uint8_t *)SlotPtr(h, f, 3) + 0xC) > 0) ClipTriFT3(h, SlotPtr(h, f, 0), SlotPtr(h, f, 1), SlotPtr(h, f, 2));
			else ClipTriFT3(h, SlotPtr(h, f, 4), SlotPtr(h, f, 5), SlotPtr(h, f, 6));
			break;
		case 3: case 5: case 6:
			if (*(const int32_t *)((const uint8_t *)SlotPtr(h, f, 3) + 0xC) > 0) ClipTriFT3(h, SlotPtr(h, f, 0), SlotPtr(h, f, 1), SlotPtr(h, f, 2));
			else ClipQuadFT4(h, SlotPtr(h, f, 4), SlotPtr(h, f, 5), SlotPtr(h, f, 6), SlotPtr(h, f, 7));
			break;
		default:
			break;
		}
	}

	// the three loaded vertices' plane distances (+0x94 / +0xA4 / +0xB4) and crossing flags (+0x90 / +0xA0 / +0xB0); the
	// triangle drawn whole (all >= 0, not all 0), clipped (a sign change) or not at all
	static void ClipPlaneTest(ClipHeader *h)
	{
		GteMVMVA_LightV0Bk();
		SlotWord(h, 3, 8) = 0;
		GteReadDataReg(9, &SlotWord(h, 3, 0xC));
		GteMVMVA_LightV1Bk();
		SlotWord(h, 4, 8) = 0;
		GteReadDataReg(9, &SlotWord(h, 4, 0xC));
		GteMVMVA_LightV2Bk();
		SlotWord(h, 5, 8) = 0;
		GteReadDataReg(9, &SlotWord(h, 5, 0xC));
		const int32_t d0 = SlotWord(h, 3, 0xC), d1 = SlotWord(h, 4, 0xC), d2 = SlotWord(h, 5, 0xC);
		if (d0 != 0 && (d0 ^ d1) < 0) SlotWord(h, 3, 8) = 1;
		if (d1 != 0 && (d1 ^ d2) < 0) SlotWord(h, 4, 8) = 2;
		if (d2 != 0 && (d2 ^ d0) < 0) SlotWord(h, 5, 8) = 4;
		h->clip = (uint32_t)(SlotWord(h, 5, 8) | SlotWord(h, 3, 8) | SlotWord(h, 4, 8));
		if (h->clip) ClipTriangle(h);
		else if ((d2 | d1 | d0) > 0) ClipTriFT3(h, Slot(h, 0), Slot(h, 1), Slot(h, 2));
	}

	static inline void LoadSlot(ClipHeader *h, int32_t k, uint16_t index)
	{
		const uint32_t v = h->verts + (uint32_t)(index & 0xFFF) * 8;
		*(uint32_t *)h->slot[k] = At<uint32_t>(v, 0);
		*(uint32_t *)(h->slot[k] + 4) = At<uint32_t>(v, 4);
	}

	// 0x5EF3C0: the part's triangles (0x10-byte records) and quads (0x14-byte records, two triangles: the second one without
	// the screen flag / backface tests of the first)
	static uint32_t ClipRenderPrims(ClipHeader *h, uint32_t ot, uint32_t shift, uint32_t cursor)
	{
		h->col = h->code;
		h->cursor = cursor;
		uint32_t p = h->prims;
		h->ot = ot;
		h->shift = shift;
		for (uint32_t k = 0; k < h->ntri; k++, p += 0x10)
		{
			LoadSlot(h, 0, At<uint16_t>(p, 0));
			LoadSlot(h, 1, At<uint16_t>(p, 2));
			LoadSlot(h, 2, At<uint16_t>(p, 4));
			GteLoadV012(Slot(h, 0), Slot(h, 1), Slot(h, 2));
			GteRTPT();
			h->uv0 = At<uint32_t>(p, 8);
			h->uv1 = At<uint32_t>(p, 0xC);
			Slot(h, 0)[3] = (int16_t)At<uint16_t>(p, 8);
			Slot(h, 1)[3] = (int16_t)At<uint16_t>(p, 0xC);
			Slot(h, 2)[3] = (int16_t)At<uint16_t>(p, 6);
			GteReadFLAG(&h->flag);
			if (h->flag & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(&h->mac0);
			if (h->mac0 <= 0) continue;
			ClipPlaneTest(h);
		}
		for (uint32_t k = 0; k < h->nquad; k++, p += 0x14)
		{
			LoadSlot(h, 0, At<uint16_t>(p, 0));
			LoadSlot(h, 1, At<uint16_t>(p, 2));
			LoadSlot(h, 2, At<uint16_t>(p, 4));
			GteLoadV012(Slot(h, 0), Slot(h, 1), Slot(h, 2));
			GteRTPT();
			const uint32_t uv3 = At<uint32_t>(p, 0x10);
			h->uv0 = At<uint32_t>(p, 8);
			Slot(h, 2)[3] = (int16_t)uv3;
			h->uv1 = At<uint32_t>(p, 0xC);
			Slot(h, 0)[3] = (int16_t)At<uint16_t>(p, 8);
			Slot(h, 1)[3] = (int16_t)At<uint16_t>(p, 0xC);
			GteReadFLAG(&h->flag);
			if (h->flag & 0x60000) continue;
			GteNCLIP();
			GteReadMAC0(&h->mac0);
			if (h->mac0 <= 0) continue;
			ClipPlaneTest(h);
			// the second triangle (v3, v1, v2)
			LoadSlot(h, 0, At<uint16_t>(p, 6));
			LoadSlot(h, 1, At<uint16_t>(p, 2));
			LoadSlot(h, 2, At<uint16_t>(p, 4));
			GteLoadV012(Slot(h, 0), Slot(h, 1), Slot(h, 2));
			GteRTPT();
			Slot(h, 2)[3] = (int16_t)uv3;
			Slot(h, 1)[3] = (int16_t)h->uv1;
			Slot(h, 0)[3] = (int16_t)(uv3 >> 16);
			GteReadFLAG(&h->flag);
			ClipPlaneTest(h);
		}
		return h->cursor;
	}

	// 0x5EF0C0: the model's parts in the mask (+0x14) through their bones (vertex groups: bone, count, x / y / z words) into
	// the vertex buffer (+0x04), each part's primitives with the camera
	static uint32_t ClipRenderModel(uint32_t model, ClipHeader *h, uint32_t ot, uint32_t shift, uint32_t cursor)
	{
		GteWriteControlReg(*(const int32_t *)&h->plane[0], 8);
		const int32_t w = *(const int32_t *)&h->plane[2];
		GteWriteControlReg(w, 9);
		GteWriteControlReg(w >> 16, 0xD);
		for (int32_t i = 0; i < 0x38; i++) (&h->ptr[0][0])[i] = (uint32_t)h + 0x58 + var<uint32_t>(CLIP_SLOTS + 4 * i) * 0x10;
		const uint32_t bones = At<uint32_t>(model, 0) + 0x10;
		uint32_t offs = At<uint32_t>(model, 4);
		const int32_t nparts = At<int32_t>(offs, 0);
		offs += 4;
		for (int32_t i = 0; i < nparts; i++)
		{
			uint32_t s = At<uint32_t>(offs, 0) + At<uint32_t>(model, 4);
			offs += 4;
			if (!(h->mask & (uint32_t)shl32(1, i))) continue;
			int32_t groups = At<int16_t>(s, 0);
			uint32_t out = h->verts;
			s += 2;
			for (; groups > 0; groups--)
			{
				const uint32_t m = (uint32_t)At<int16_t>(s, 0) * 0x30 + bones + 0x10;
				s += 2;
				GteSetRotMatrixCtrl((const Mat4x3 *)m);
				GteSetTransVectorCtrl((const Mat4x3 *)m);
				int32_t n = At<int16_t>(s, 0);
				s += 2;
				for (; n != 0; n--)
				{
					GteWriteDataReg((uint32_t)At<uint16_t>(s, 0) | (uint32_t)At<uint16_t>(s, 2) << 16, 0);
					GteWriteDataReg(At<uint16_t>(s, 4), 1);
					s += 6;
					out += 8;
					GteMVMVA_RotV0Tr2();
					int32_t ir[3];
					GteReadDataRegTo(&ir[0], 9);
					GteReadDataRegTo(&ir[1], 10);
					GteReadDataRegTo(&ir[2], 11);
					At<uint32_t>(out, -8) = (uint32_t)ir[1] << 16 | ((uint32_t)ir[0] & 0xFFFF);
					At<int16_t>(out, -4) = (int16_t)ir[2];
				}
			}
			s = (s + 3) & ~3u;
			h->ntri = At<uint16_t>(s, 0);
			h->nquad = At<uint16_t>(s, 2);
			h->w0C = At<uint16_t>(s, 4);
			h->w0E = At<uint16_t>(s, 6);
			h->prims = s + 0xC;
			GteSetRotMatrixCtrl((const Mat4x3 *)var<uint32_t>(CAM_PTR));
			GteSetTransVectorCtrl((const Mat4x3 *)var<uint32_t>(CAM_PTR));
			cursor = ClipRenderPrims(h, ot, shift, cursor);
		}
		return cursor;
	}

	// 0x5EEF80: the plane's distance word (b + 6) = -(b . a) >> 12
	static void PlaneDistance(const int16_t *a, int16_t *b)
	{
		b[3] = (int16_t)-((mul32(a[2], b[2]) + mul32(a[1], b[1]) + mul32(a[0], b[0])) >> 12);
	}

	// 0x5EEFC0: an entity drawn through the clipping renderer: its shadow (unless flag 0x20), its body (parts of +0x7C) at its
	// pose, then its weapon (all parts; its matrix = the renderer's slot pointers composed with the weapon's: the 0xC8 block
	// was overwritten by the body's draw)
	static void DrawClipped(ClipHeader *h, uint8_t *ent)
	{
		uint8_t *b = (uint8_t *)h;
		if (!(*ent & 0x20)) FrameArena() = DrawShadow(ent, OT0() + 0x4064, 0x10, FrameArena());
		memcpy(b + 0xC8, ent + 0x40, 0x20);
		var<uint32_t>(CAM_PTR) = 0x1D97778;
		ComputeBonesWorldMatrices(ent + 0x60, b + 0xC8);
		h->verts = var<uint32_t>(0x1D98B3C);
		h->code = *(const uint32_t *)(ent + 0x28);
		h->mask = *(const uint32_t *)(ent + 0x7C);
		FrameArena() = ClipRenderModel(*(const uint32_t *)(ent + 0x64), h, RenderOT(), 2, FrameArena());
		BuildBoneMatricesFromPose(ent + 0x60);
		uint8_t *w = *(uint8_t **)(ent + 0x78);
		if (w && !(ent[1] & 2))
		{
			h->mask = 0xFFFFFFFF;
			ComposeAffineTransform((const Mat4x3 *)(b + 0xC8), (const Mat4x3 *)(w + 0x24), (Mat4x3 *)(b + 0xC8));
			ComputeBonesWorldMatrices(w, b + 0xC8);
			FrameArena() = ClipRenderModel(*(const uint32_t *)(w + 4), h, RenderOT(), 2, FrameArena());
			BuildBoneMatricesFromPose(w);
		}
	}

	// ------------------------------------------------------------------
	// Trail (0x5E9E00)
	// ------------------------------------------------------------------
	// the weapon point and the tip 666 along the blade into the work block
	static void TrailPoints(TrailWork *w)
	{
		WeaponBone(Caster(), 1, w->dir, w->base);
		w->tip[0] = (int16_t)((int16_t)(mul32(w->dir[0], 666) >> 12) + w->base[0]);
		w->tip[1] = (int16_t)((int16_t)(mul32(w->dir[1], 666) >> 12) + w->base[1]);
		w->tip[2] = (int16_t)((int16_t)(mul32(w->dir[2], 666) >> 12) + w->base[2]);
	}

	// newest-first gather of at most `want` ring entries from slot idx into the two point lists; returns the count
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
			if (idx < 0) idx = 5;
			cnt++;
		} while (cnt < want);
		return cnt;
	}

	// the trail's derived globals: the first tip, the unit direction tip 1 -> tip 0, the first point, the unit blade direction
	static void TrailDirections(const uint8_t *sa, const uint8_t *sb)
	{
		var<uint32_t>(TIP0) = *(const uint32_t *)sb;
		var<uint32_t>(TIP0 + 4) = *(const uint32_t *)(sb + 4);
		int32_t *d = (int32_t *)TRAIL_DIR;
		d[0] = (int32_t)*(const int16_t *)sb - *(const int16_t *)(sb + 8);
		d[1] = (int32_t)*(const int16_t *)(sb + 2) - *(const int16_t *)(sb + 0xA);
		d[2] = (int32_t)*(const int16_t *)(sb + 4) - *(const int16_t *)(sb + 0xC);
		Normalize(d, d);
		var<uint32_t>(BASE0) = *(const uint32_t *)sa;
		var<uint32_t>(BASE0 + 4) = *(const uint32_t *)(sa + 4);
		int32_t *b = (int32_t *)BLADE_DIR;
		b[0] = (int32_t)var<int16_t>(TIP0) - *(const int16_t *)sa;
		b[1] = (int32_t)var<int16_t>(TIP0 + 2) - *(const int16_t *)(sa + 2);
		b[2] = (int32_t)var<int16_t>(TIP0 + 4) - *(const int16_t *)(sa + 4);
		Normalize(b, b);
	}

	// 30 quad pairs between the two splines: gouraud from the base colour 0x3A903030 fading toward the far colour (black)
	// along the trail, and a copy whose leading edge is 0x3A000000 (Renzokuken's 0x5FFAA0 draw); the cursor ends at the
	// second packet (0x24 past the last pair)
	static void TrailDraw(const uint8_t *sa, const uint8_t *sb, TrailWork *w)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteSetFarColor(0, 0, 0);
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
			GteStoreOTZ(&w->otz);
			GteSetIR0(tacc / 30);
			GteLoadRGBC(&w->base_col);
			GteDPCS();
			GteStoreRGB2(&w->col1);
			pk[5] = pk[1] = w->col0;
			pk[7] = pk[3] = w->col1;
			InsertPrimAutoDepth(OT0() + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk);
			pk2[5] = w->col0;
			pk2[3] = pk2[1] = 0x3A000000;
			pk2[7] = w->col1;
			pk2[4] = pk[4];
			pk2[2] = pk[2];
			pk2[6] = pk[6];
			pk2[8] = pk[8];
			pk2[0] = 0x8000000;
			InsertPrimAutoDepth(OT0() + 4 * (uint32_t)(w->otz >> 2) + 0x44, pk2);
			pk += 0x12;
			pk2 += 0x12;
		}
		PacketCursor() = (uint32_t)pk2;
	}

	static int32_t TrailWant(int32_t c) { return c >= 0x128 ? 0x12E - c : 6; }

	// what the original Trail leaves at [entry esp - 4] (its colour accumulator local, 30 x 0x1000 after its draw): HitDirector's
	// never-written stack word (see HitDirector)
	static uint32_t g_trail_slot4 = 0;
	static bool g_trail_slot4_known = false;

	static uint32_t __cdecl TrailTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		TrailWork *w = (TrailWork *)FieldAlloc(0x40);
		TrailPoints(w);
		const int32_t c = n->c;
		const int32_t idx = c % 6;
		RingEntry *e = (RingEntry *)(RING + 0x14 * idx);
		e->alive = 1;
		e->base[0] = *(const uint32_t *)&w->base[0];
		e->base[1] = *(const uint32_t *)&w->base[2];
		e->tip[0] = *(const uint32_t *)&w->tip[0];
		e->tip[1] = *(const uint32_t *)&w->tip[2];
		const int32_t cnt = TrailGather((const uint8_t *)RING, idx, TrailWant(c), (uint8_t *)PTS_BASE, (uint8_t *)PTS_TIP);
		if (cnt > 1)
		{
			SplineCurve(cnt, (const int16_t *)PTS_BASE, 0x20, (int16_t *)SPL_BASE);
			SplineCurve(cnt, (const int16_t *)PTS_TIP, 0x20, (int16_t *)SPL_TIP);
			TrailDirections((const uint8_t *)SPL_BASE, (const uint8_t *)SPL_TIP);
			// 30 fps layer: see mag165_lion_heart_held.inc
			FX_HELD(held_note_trail(n);)
			TrailDraw((const uint8_t *)SPL_BASE, (const uint8_t *)SPL_TIP, w);
			g_trail_slot4 = 0x1E000;
			g_trail_slot4_known = true;
		}
		else g_trail_slot4_known = false;
		FieldFree(0x40);
		if (DrawOnly()) return 0;
		n->c++;
		return EndAt(n->c, 0x12D);
	}

	// ------------------------------------------------------------------
	// Spawners (called by the director)
	// ------------------------------------------------------------------
	static Node24 *AddMain(uint32_t fn) { return (Node24 *)AddTaskToQueue(QMain(), fn); }

	// 0x5E9CE0: a task with counter / word +0xE cleared
	static void AddCleared(uint32_t fn)
	{
		Node24 *t = AddMain(fn);
		t->c = 0;
		t->e = 0;
	}

	// 0x5E9D20: the screen flash ramp (16 ticks, 0x800 more, then the stage groups shown)
	static void AddFlash()
	{
		Node24 *t = AddMain(ORIG_Flash);
		t->c = 0;
		t->e = 0x10;
		t->w18 = 0x800;
		t->w1A = 1;
	}

	// 0x5E9DD0: the trail, its ring cleared
	static void AddTrail()
	{
		Node24 *t = AddMain(ORIG_Trail);
		t->c = 0;
		for (uint32_t a = RING; a < RING + 6 * 0x14; a += 0x14) *(uint32_t *)a = 0;
	}

	// 0x5EA850: the ground ring at the caster's effect bone 0xF1 (y 0)
	static void AddGroundRing()
	{
		GetEffectSpawnPosition(Caster(), 0xF1, 0, (void *)BONE_F1);
		Node24 *t = AddMain(ORIG_GroundRing);
		t->c = 0;
		t->w10 = var<int16_t>(BONE_F1);
		t->w12 = 0;
		t->w14 = var<int16_t>(BONE_F1 + 4);
		t->w18 = (int16_t)(CrtRand() % 0x1000);
		t->w1C = 0x1800;
		t->w22 = 0x100;
		t->w20 = 0x100;
	}

	// 0x5EAA50: RockBurst (at the bone, y 0), Debris (at the bone), BoneSparks, each with a new node id
	static void AddRockTrio()
	{
		Node24 *t = AddMain(ORIG_RockBurst);
		t->e = TakeId();
		t->c = 0;
		t->w10 = var<int16_t>(BONE_F1);
		t->w12 = 0;
		t->w14 = var<int16_t>(BONE_F1 + 4);
		t = AddMain(ORIG_Debris);
		t->e = TakeId();
		*(uint32_t *)&t->w10 = var<uint32_t>(BONE_F1);
		t->c = 0;
		*(uint32_t *)&t->w14 = var<uint32_t>(BONE_F1 + 4);
		t = AddMain(ORIG_BoneSparks);
		t->e = TakeId();
		t->c = 0;
	}

	// 0x5EB3E0
	static void AddBladeSparks()
	{
		Node24 *t = AddMain(ORIG_BladeSparks);
		t->e = TakeId();
		t->c = 0;
	}

	// the tip point copied into a new node
	static void AtTip(Node24 *t)
	{
		*(uint32_t *)&t->w10 = var<uint32_t>(TIP);
		*(uint32_t *)&t->w14 = var<uint32_t>(TIP + 4);
	}

	// 0x5EB6D0
	static void AddTipFlash()
	{
		SetTipPoint();
		Node24 *t = AddMain(ORIG_TipFlash);
		AtTip(t);
		t->c = 0;
		t->w1E = 0x95;
		t->w1C = 0x95;
	}

	// 0x5EB890 / 0x5EBB90 / 0x5ED8A0: a pool task at the tip with a new node id
	static void AddAtTipWithId(uint32_t fn)
	{
		Node24 *t = AddMain(fn);
		t->e = TakeId();
		t->c = 0;
		AtTip(t);
	}

	// 0x5EBEF0: two rings at the tip (+0xE = 0 / 1: the y angle -0x300 / 0x300)
	static void AddTipRings()
	{
		for (int16_t k = 0; k < 2; k++)
		{
			Node24 *t = AddMain(ORIG_TipRing);
			AtTip(t);
			t->c = 0;
			t->e = k;
			t->w18 = (int16_t)(CrtRand() % 0x1000);
			const int32_t r = CrtRand() % 0x3C;
			t->w1E = 0xC00;
			t->w1C = 0xC00;
			t->w1A = (int16_t)(r + 0x1E);
		}
	}

	// 0x5ED270: the flat ring at the first target's anchor
	static void AddTargetFlatRing()
	{
		Node24 *t = AddMain(ORIG_FlatRing);
		t->c = 0;
		GetDefaultEffectPosition(Target(), &t->w10);
		t->w18 = (int16_t)(CrtRand() % 0x1000);
		t->w1C = 0xE00;
		const int16_t s = (int16_t)((CrtRand() % 0x300 + 0x600) / 3);
		t->w22 = s;
		t->w20 = s;
	}

	// 0x5ED440: the flat ring at the caster's bone 0xF0
	static void AddCasterFlatRing()
	{
		Node24 *t = AddMain(ORIG_FlatRing);
		t->c = 0;
		GetEffectSpawnPosition(Caster(), 0xF0, 0, &t->w10);
		t->w18 = (int16_t)(CrtRand() % 0x1000);
		t->w1C = 0xC00;
		const int16_t s = (int16_t)((CrtRand() % 0x200 + 0x480) / 3);
		t->w22 = s;
		t->w20 = s;
	}

	// 0x5ED700
	static void AddTipFlash2()
	{
		SetTipPoint();
		Node24 *t = AddMain(ORIG_TipFlash2);
		AtTip(t);
		t->c = 0;
		t->w18 = (int16_t)(CrtRand() % 0x1000);
		t->w1A = 0x40;
		t->w1E = 0x95;
		t->w1C = 0x95;
	}

	// 0x5EDBC0: the streak from the tip, 2 ticks delayed
	static void AddStreak()
	{
		SetTipPoint();
		Node24 *t = AddMain(ORIG_Streak);
		AtTip(t);
		t->c = 0;
		t->e = 2;
		t->w1C = 0x1000;
		t->w20 = 0x2000;
	}

	// 0x5EDE90: the last hit at the first target's anchor: FinalRing, FinalSparkA, FinalShards, FinalFlash, FinalRingSparks;
	// pool C cleared; flag 0x20 on the caster, the target and its linked entities (old bits kept); the result
	static void AddFinal()
	{
		Node24 *a = AddMain(ORIG_FinalRing);
		a->c = 0;
		GetDefaultEffectPosition(Target(), &a->w10);
		a->w18 = (int16_t)(CrtRand() % 0x400 - 0x200);
		const int16_t s = (int16_t)((CrtRand() % 0x1200 + 0x3000) / 3);
		a->w1E = s;
		a->w1C = s;
		const uint32_t p0 = *(const uint32_t *)&a->w10, p1 = *(const uint32_t *)&a->w14;
		Node24 *t = AddMain(ORIG_FinalSparkA);
		t->e = TakeId();
		*(uint32_t *)&t->w10 = p0;
		t->c = 0;
		*(uint32_t *)&t->w14 = p1;
		t = AddMain(ORIG_FinalShards);
		t->e = TakeId();
		*(uint32_t *)&t->w10 = p0;
		t->c = 0;
		*(uint32_t *)&t->w14 = p1;
		t = AddMain(ORIG_FinalFlash);
		*(uint32_t *)&t->w10 = p0;
		t->c = 0;
		*(uint32_t *)&t->w14 = p1;
		const int16_t f = (int16_t)((CrtRand() % 0xC00 + 0x3000) / 4);
		t->w1E = f;
		t->w1C = f;
		t = AddMain(ORIG_FinalRingSparks);
		t->e = TakeId();
		*(uint32_t *)&t->w10 = p0;
		t->c = 0;
		*(uint32_t *)&t->w14 = p1;
		for (uint32_t k = 0, a3 = PoolCBase(); k < 500; k++, a3 += 0x18) *(uint32_t *)a3 = 0;
		uint8_t *cs = Caster();
		const uint16_t cf = *(const uint16_t *)cs;
		Keep(CasterSlot())->flag20 = (uint16_t)(cf & 0x20);
		uint8_t *tg = Target();
		const uint8_t tf = *tg;
		*(uint16_t *)cs = (uint16_t)(cf | 0x20);
		*tg |= 0x20;
		Keep(TargetSlot())->flag20 = (uint16_t)(tf & 0x20);
		uint8_t *e = *(uint8_t **)(tg + 0x8C);
		if (e && e != tg)
		{
			do
			{
				if (*e & 2)
				{
					const int32_t i = EntityIndex(e);
					if (i < 7)
					{
						uint8_t *le = Entity(i);
						const uint16_t lf = *(const uint16_t *)le;
						Keep(i)->flag20 = (uint16_t)(lf & 0x20);
						*(uint16_t *)le = (uint16_t)(lf | 0x20);
					}
				}
				e = *(uint8_t **)(e + 0x8C);
			} while (e != tg);
		}
		const int32_t k = ResultIndex();
		ResultIndex() = k + 1;
		ApplyActionResultToTarget(Context()->actions->targets + TARGET_STRIDE * k);
	}

	// 0x5EEC00: TargetSink for the first target and its linked entities; the caster's flag bit 0x20 put back
	static void AddTargetSink()
	{
		Node24 *t = AddMain(ORIG_TargetSink);
		t->c = 0;
		t->e = (int16_t)TargetSlot();
		uint8_t *tg = Target();
		uint8_t *e = *(uint8_t **)(tg + 0x8C);
		if (e && e != tg)
		{
			do
			{
				if (*e & 2)
				{
					const int32_t i = EntityIndex(e);
					if (i < 7)
					{
						t = AddMain(ORIG_TargetSink);
						t->c = 0;
						t->e = (int16_t)i;
					}
				}
				e = *(uint8_t **)(e + 0x8C);
			} while (e != tg);
		}
		uint8_t *cs = Caster();
		*(uint16_t *)cs = (uint16_t)(Keep(CasterSlot())->flag20 | (*(const uint16_t *)cs & 0xFFDF));
	}

	// ------------------------------------------------------------------
	// Director (0x5E9650)
	// ------------------------------------------------------------------
	static uint32_t __cdecl DirectorTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)FRAME, (Mat4x3 *)CAM_FRAME);
		if (UpdateFlags() & 0x201)
		{
			if (UpdateFlags() & 1) return 0;
			if (PauseQuery() < 0) return 0;
		}
		if (n->c == 0)
		{
			StopSounds(-1, 0);
			CameraScriptStart(CAMERA_SCRIPT, FRAME, Caster(), QMain(), 0);
			ArmCameraReturn();
			BindEntityModelFromComData(Caster(), BIND_OUT, ANIM_DATA);
			var<uint32_t>(TARGET_KEPT) = *(const uint32_t *)(Target() + 0x1C);
			var<uint32_t>(TARGET_KEPT + 4) = *(const uint32_t *)(Target() + 0x20);
			var<uint32_t>(CASTER_KEPT) = *(const uint32_t *)(Caster() + 0x1C);
			var<uint32_t>(CASTER_KEPT + 4) = *(const uint32_t *)(Caster() + 0x20);
			for (int32_t i = 0; i < 7; i++)
			{
				Keep(i)->pos[0] = *(const uint32_t *)(Entity(i) + 0x1C);
				Keep(i)->pos[1] = *(const uint32_t *)(Entity(i) + 0x20);
			}
			GetDefaultEffectPosition(Target(), (int16_t *)TARGET_ANCHOR);
		}
		int32_t c = n->c;
		if (c < 0x4E)
		{
			if (c == 0xA)
			{
				BdPlaySE(SOUND_A, 0, 0x80);
				AddRockTrio();
			}
			else if (c == 0) QueueChainTransformation(Caster(), 1);
			else if (c == 2) CharacterLoad(0x21C, TexBase());
			else if (c == 0xE)
			{
				if (PauseQuery() < 0) return 0;
				QueueTIMUpload(TexBase());
			}
			else if (c == 0xF) CharacterLoad(0x21D, TexBase());
			else if (c == 1) AddTrail();
			else if (c == 8) AddGroundRing();
			else if (c == 0x14) AddTipFlash();
			else if (c == 0x18) AddAtTipWithId(ORIG_TipShards);
			else if (c == 0x1A) AddAtTipWithId(ORIG_TipSmoke);
			else if (c == 0x43) ScreenFadeTask(0, 1, 8, 0xFF);
			else if (c == 0x44) AddTipRings();
		}
		else if ((c -= 0x4E) < 0xD)
		{
			if (c == 0)
			{
				QueueChainTransformation(Caster(), 2);
				AddBladeSparks();
			}
			else if (c == 0xC)
			{
				GetDefaultEffectPosition(Caster(), &n->w10);
				Mat4x3 *f = (Mat4x3 *)FRAME;
				f->t[0] = n->w10;
				f->t[1] = n->w12;
				f->t[2] = n->w14;
			}
		}
		else if ((c -= 0xD) < 0xC)
		{
			if (c == 0) AddCleared(ORIG_HitDirector);
			else if (c == 9) AddTargetFlatRing();
		}
		else if ((c -= 0xC) < 0x26)
		{
			if (c == 0x10) BdPlaySE(SOUND_B, 0, 0x80);
			else if (c == 0) *(uint16_t *)Caster() &= 0xFFF7;
			else
			{
				if (c >= 9 && c <= 0xD) *(int16_t *)(Caster() + 0x1E) = (int16_t)(*(const int16_t *)(Caster() + 0x1E) - 800);
				if (c == 8) AddCasterFlatRing();
			}
		}
		else if ((c -= 0x26) < 0x3C)
		{
			if (c == 0x15)
			{
				if (PauseQuery() < 0) return 0;
				QueueTIMUpload(TexBase());
			}
			else if (c == 0) AddFlash();
			else if (c == 0x14) AddCleared(ORIG_WaveSpawner);
		}
		else if ((c -= 0x3C) < 0x2A)
		{
			if (c == 1) BdPlaySE(SOUND_C, 0, 0x80);
			else if (c == 0)
			{
				CharacterLoad(0x21E, TexBase());
				AddTipFlash2();
			}
			else if (c == 0xA)
			{
				if (PauseQuery() < 0) return 0;
				QueueTIMUpload(TexBase());
			}
			else if (c == 6) AddAtTipWithId(ORIG_TipRocks);
			else if (c == 0x28) ScreenFadeTask(0, 4, 8, 0xFF);
			else if (c == 0x29)
			{
				Mat4x3 *f = (Mat4x3 *)FRAME;
				f->t[0] = var<int16_t>(TARGET_CENTRE);
				f->t[2] = var<int16_t>(TARGET_CENTRE + 4);
			}
		}
		else if ((c -= 0x2A) < 0x5A)
		{
			bool step = false;
			if (c == 0)
			{
				QueueChainTransformation(Caster(), 3);
				GetDefaultEffectPosition(Caster(), (int16_t *)CASTER_POS);
				GetDefaultEffectPosition(Target(), (int16_t *)TARGET_POS);
				const int32_t dz = (int32_t)var<int16_t>(TARGET_POS + 4) - var<int16_t>(CASTER_POS + 4);
				n->w22 = dz < -2200 ? (int16_t)((dz * 2 + 0x1130) / 16) : 0;
				step = true;
			}
			else if (c < 0x10) step = true;
			if (step) *(int16_t *)(Caster() + 0x20) = (int16_t)(*(const int16_t *)(Caster() + 0x20) + n->w22);
			if (c == 2) AddStreak();
			else if (c == 0x2D) AddFinal();
			else if (c == 0x59)
			{
				Mat4x3 *f = (Mat4x3 *)FRAME;
				f->t[0] = var<int16_t>(CASTER_KEPT);
				f->t[1] = var<int16_t>(CASTER_KEPT + 2);
				f->t[2] = var<int16_t>(CASTER_KEPT + 4);
			}
		}
		else if ((c -= 0x5A) < 0x25)
		{
			if (c == 0xA) BdPlaySE(SOUND_D, 0, 0x80);
			else if (c == 0)
			{
				QueueChainTransformation(Caster(), 4);
				*(uint32_t *)(Caster() + 0x1C) = var<uint32_t>(CASTER_KEPT);
				*(uint32_t *)(Caster() + 0x20) = var<uint32_t>(CASTER_KEPT + 4);
				SetScreenFlash(0, 0);
				StageGroupsHide();
			}
			else if (c == 2)
			{
				AddTargetSink();
				AddCleared(ORIG_Restore);
			}
		}
		n->c++;
		if (n->c <= 0x172) return 0;
		StageGroupsHide();
		SetScreenFlash(0, 0);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Single models
	// ------------------------------------------------------------------
	// prim-model header (0x58) of the single models: model, far colour 0, mode 0x33 / 0xF3 with the fade
	static uint8_t *ModelHeader(uint32_t model, bool fade_on, int32_t fade)
	{
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (fade_on)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		return h;
	}

	static void RenderToCursor(uint8_t *h) { PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor()); }
	static void RenderToArena(uint8_t *h) { FrameArena() = RenderPrimModel(h, RenderOT(), 2, FrameArena()); }

	// model matrix: angles, translation, scale, composed with `view`; GTE rotation / translation set
	static void WorldMatrix(Mat4x3 *m, int16_t ax, int16_t ay, int16_t az, int32_t tx, int32_t ty, int32_t tz, int32_t sx, int32_t sy, int32_t sz, const Mat4x3 *view)
	{
		int16_t ang[4]; // (4th word never written)
		ang[0] = ax;
		ang[1] = ay;
		ang[2] = az;
		ComposeZYXRotationMatrix(ang, m);
		m->t[0] = tx;
		m->t[1] = ty;
		m->t[2] = tz;
		int32_t sv[3];
		sv[0] = sx;
		sv[1] = sy;
		sv[2] = sz;
		Scale3DMatrix(m, sv);
		ComposeAffineTransform(view, m, m);
		GteSetRotMatrix(m);
		GteSetTransVector(m);
	}

	// camera-facing models: rotation (0, 0, roll) scaled by s, TransformCameraByShadowRotation at pos (scale, offset)
	static void FacingMatrix(Mat4x3 *m, int16_t roll, int32_t s, const int16_t *pos, int32_t scale, int32_t off)
	{
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = roll;
		ComposeZYXRotationMatrix(ang, m);
		int32_t sv[3];
		sv[0] = s;
		sv[1] = s;
		sv[2] = s;
		Scale3DMatrix(m, sv);
		TransformCameraByShadowRotation(pos, scale, off);
		GteSetRotMatrix(m);
	}

	// GroundRing (0x5EA8E0): turned about y, scaled (w1C, w20, w1C) at the bone (y 0), v scroll counter << 4, fading in up to 8
	static int32_t GroundRingFade(int32_t c) { return shl32(8 - c, 9); }
	static void GroundRingDraw(const Node24 *n, int16_t sy, int32_t scroll, int32_t c, int32_t fade)
	{
		Mat4x3 m = {}; // (pad +0x12 never written in the original: GTE R33 high half only)
		WorldMatrix(&m, 0, n->w18, 0, n->w10, n->w12, n->w14, n->w1C, sy, n->w1C, &Camera());
		uint8_t *h = AllocHeader(0x90);
		*(uint32_t *)(h + 8) = 0;
		*(int16_t *)(h + 0x18) = 0x40;
		*(int16_t *)(h + 0x1C) = 0x40;
		*(int16_t *)(h + 0x1A) = 0;
		*(uint32_t *)(h + 0x10) = 0;
		*(uint32_t *)h = MODEL_GROUNDRING;
		*(uint32_t *)(h + 0x20) = 0x33;
		*(int16_t *)(h + 0x1E) = 0x80;
		*(int32_t *)(h + 0x14) = scroll;
		if (c < 8)
		{
			*(uint32_t *)(h + 0x20) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		*(uint32_t *)(h + 0x24) = 0x808080;
		PacketCursor() = ScrollRender(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x90);
	}

	static uint32_t __cdecl GroundRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_GroundRing, n);)
		GroundRingDraw(n, n->w20, shl32(n->c, 4), n->c, GroundRingFade(n->c));
		if (DrawOnly()) return 0;
		const int16_t r = n->w22;
		n->w20 = (int16_t)(n->w20 + r);
		n->w22 = (int16_t)(r - r / 32);
		n->c++;
		return EndAt(n->c, 0x3C);
	}

	// TipFlash (0x5EB790): camera-facing at the tip, growing (up to counter 0x10)
	static void TipFlashDraw(const Node24 *n, int16_t s)
	{
		Mat4x3 m = {};
		FacingMatrix(&m, 0, s, &n->w10, 0, -(s >> 3));
		uint8_t *h = ModelHeader(MODEL_TIPFLASH, false, 0);
		RenderToCursor(h);
		FieldFree(0x58);
	}

	static uint32_t __cdecl TipFlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_TipFlash, n);)
		TipFlashDraw(n, n->w1C);
		if (DrawOnly()) return 0;
		const int16_t c = n->c;
		if (c < 0x10)
		{
			const int16_t r = n->w1E;
			n->w1C = (int16_t)(n->w1C + r);
			n->w1E = (int16_t)(r - r / 32);
		}
		n->c = (int16_t)(c + 1);
		return EndAt(n->c, 0x30);
	}

	// TipRing (0x5EBFC0): angles (0x200, -+0x300, w18) in the effect frame at the tip, scaled; fading from 4
	static void TipRingDraw(const Node24 *n, int16_t roll, int16_t s, int32_t c, int32_t fade)
	{
		Mat4x3 m = {};
		int16_t ang[4];
		ang[0] = 0x200;
		ang[1] = (int16_t)(n->e != 0 ? 0x300 : -0x300);
		ang[2] = roll;
		ComposeZYXRotationMatrix(ang, &m);
		GteMatrixMultiply((const Mat4x3 *)FRAME, &m);
		m.t[0] = n->w10;
		m.t[1] = n->w12;
		m.t[2] = n->w14;
		int32_t sv[3];
		sv[0] = s;
		sv[1] = s;
		sv[2] = s;
		Scale3DMatrix(&m, sv);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = ModelHeader(MODEL_TIPRING, c >= 4, fade);
		RenderToCursor(h);
		FieldFree(0x58);
	}

	static uint32_t __cdecl TipRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_TipRing, n);)
		TipRingDraw(n, n->w18, n->w1C, n->c, mul32(n->c - 4, 682));
		if (DrawOnly()) return 0;
		const int16_t r = n->w1E;
		n->w18 = (int16_t)(n->w18 + n->w1A);
		n->w1C = (int16_t)(n->w1C + r);
		n->w1E = (int16_t)(r - r / 7);
		n->c++;
		return EndAt(n->c, 0xA);
	}

	// HitSlashA / HitSlashB (0x5ECB40 / 0x5ECCD0): turned about z by w18, then onto the hit's trail direction, at the hit
	// point, scaled
	static void HitSlashDraw(const Node24 *n, uint32_t model, int16_t s, bool fade_on, int32_t fade)
	{
		const HitRec *r = Hit(n->e);
		Mat4x3 m1 = {}, m2 = {};
		int16_t ang[4];
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = n->w18;
		ComposeZYXRotationMatrix(ang, &m1);
		int32_t axis[4];
		const int32_t angle = RotationBetweenVectors((const int32_t *)REF_BACK, r->dir, axis);
		BuildAxisAngleRotationMatrix(angle, &m2, axis);
		GteMatrixMultiply(&m2, &m1);
		m1.t[0] = r->pos[0];
		m1.t[1] = r->pos[1];
		int32_t sv[3];
		sv[2] = s;
		sv[1] = s;
		sv[0] = s;
		m1.t[2] = r->pos[2];
		Scale3DMatrix(&m1, sv);
		ComposeAffineTransform(&Camera(), &m1, &m1);
		GteSetRotMatrix(&m1);
		GteSetTransVector(&m1);
		uint8_t *h = ModelHeader(model, fade_on, fade);
		RenderToCursor(h);
		FieldFree(0x58);
	}

	static uint32_t __cdecl HitSlashATask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_HitSlashA, n);)
		HitSlashDraw(n, MODEL_SLASHA, n->w1C, n->c >= 4, mul32(n->c - 4, 682));
		if (DrawOnly()) return 0;
		const int16_t r = n->w1E;
		n->w1C = (int16_t)(n->w1C + r);
		n->w1E = (int16_t)(r - r / 4);
		n->c++;
		return EndAt(n->c, 0xA);
	}

	static uint32_t __cdecl HitSlashBTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_HitSlashB, n);)
		HitSlashDraw(n, MODEL_SLASHB, n->w1C, n->c >= 2, shl32(n->c - 2, 9));
		if (DrawOnly()) return 0;
		const int16_t r = n->w1E;
		n->w1C = (int16_t)(n->w1C + r);
		n->w1E = (int16_t)(r - r / 24);
		n->c++;
		return EndAt(n->c, 0xA);
	}

	// FlatRing (0x5ED300): turned about y, scaled (w1C, w20, w1C) on the floor (y 0) into the frame arena; fading from 2
	static void FlatRingDraw(const Node24 *n, int16_t sy, int32_t c, int32_t fade)
	{
		Mat4x3 m = {};
		WorldMatrix(&m, 0, n->w18, 0, n->w10, 0, n->w14, n->w1C, sy, n->w1C, &Camera());
		uint8_t *h = ModelHeader(MODEL_FLATRING, c >= 2, fade);
		RenderToArena(h);
		FieldFree(0x58);
	}

	static uint32_t __cdecl FlatRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_FlatRing, n);)
		FlatRingDraw(n, n->w20, n->c, shl32(n->c - 2, 9));
		if (DrawOnly()) return 0;
		const int16_t r = n->w22;
		n->w20 = (int16_t)(n->w20 + r);
		n->w22 = (int16_t)(r - r / 4);
		n->c++;
		return EndAt(n->c, 0xA);
	}

	// WaveSpawner (0x5ED4E0, draws nothing): a ScreenWave on every 12th tick (counter % 12 == 1) up to 0x63
	static uint32_t __cdecl WaveSpawnerTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		Node24 *n = (Node24 *)tn;
		if (n->c % 12 == 1)
		{
			Node24 *t = AddMain(ORIG_ScreenWave);
			t->c = 0;
			t->w18 = (int16_t)shl32(CrtRand() % 8, 9);
			t->w1A = (int16_t)shl32(CrtRand() % 4, 9);
			t->w1C = 0x2000;
			t->w20 = (int16_t)(CrtRand() % 16 + 6);
		}
		n->c++;
		return EndAt(n->c, 0x63);
	}

	// ScreenWave (0x5ED590): view space (no camera) at (0, 0, projection distance), angles (w18, 0, w1A), scaled w1C; MAG_066's
	// scroll renderer into OT + 0x4068 (mode 6), v scroll w20 x counter, fade 0x1000 - 0x180 c (up to 8) / 0x400 / 0x180 c -
	// 0x1A00 (from 0x14), colour 0xD88808[counter % 18]
	static int32_t ScreenWaveFade(int32_t c) { return c < 8 ? 0x1000 - mul32(c, 0x180) : c >= 0x14 ? mul32(c, 0x180) - 0x1A00 : 0x400; }
	static uint32_t ScreenWaveColour(int32_t c) { return var<uint32_t>(WAVE_COLOURS + 4 * (c % 0x12)); }
	static void ScreenWaveDraw(const Node24 *n, int32_t scroll, int32_t fade, uint32_t colour)
	{
		Mat4x3 m = {};
		int16_t ang[4];
		ang[0] = n->w18;
		ang[1] = 0;
		ang[2] = n->w1A;
		ComposeZYXRotationMatrix(ang, &m);
		int32_t sv[3];
		sv[2] = n->w1C;
		sv[1] = n->w1C;
		sv[0] = n->w1C;
		m.t[0] = 0;
		m.t[1] = 0;
		m.t[2] = ProjectionH();
		Scale3DMatrix(&m, sv);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = AllocHeader(0x90);
		*(uint32_t *)(h + 8) = 0;
		*(int16_t *)(h + 0x18) = 0;
		*(int16_t *)(h + 0x1A) = 0;
		*(uint32_t *)(h + 0x10) = 0;
		*(uint32_t *)h = MODEL_WAVE;
		*(uint32_t *)(h + 0x20) = 0xF3;
		*(int16_t *)(h + 0x1C) = 0x40;
		*(int16_t *)(h + 0x1E) = 0x80;
		*(int32_t *)(h + 0x14) = scroll;
		*(int32_t *)(h + 0xC) = fade;
		*(uint32_t *)(h + 0x24) = colour;
		FrameArena() = ScrollRender(h, OT0() + 0x4068, 6, FrameArena());
		FieldFree(0x90);
	}

	static uint32_t __cdecl ScreenWaveTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_ScreenWave, n);)
		ScreenWaveDraw(n, mul32(n->w20, n->c), ScreenWaveFade(n->c), ScreenWaveColour(n->c));
		if (DrawOnly()) return 0;
		n->c++;
		return EndAt(n->c, 0x1C);
	}

	// TipFlash2 (0x5ED760): camera-facing at the tip (scale 0x1000), turned about z, growing; fading in up to 8, out from 0x20
	static int32_t TipFlash2Fade(int32_t c) { return c < 8 ? shl32(8 - c, 9) : shl32(c - 0x20, 9); }
	static void TipFlash2Draw(const Node24 *n, int16_t roll, int16_t s, int32_t c, int32_t fade)
	{
		Mat4x3 m = {};
		FacingMatrix(&m, roll, s, &n->w10, 0x1000, -(s >> 3));
		uint8_t *h = ModelHeader(MODEL_TIPFLASH2, c < 8 || c >= 0x20, fade);
		RenderToCursor(h);
		FieldFree(0x58);
	}

	static uint32_t __cdecl TipFlash2Task(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_TipFlash2, n);)
		TipFlash2Draw(n, n->w18, n->w1C, n->c, TipFlash2Fade(n->c));
		if (DrawOnly()) return 0;
		const int16_t r = n->w1E;
		n->w18 = (int16_t)(n->w18 + n->w1A);
		n->w1C = (int16_t)(n->w1C + r);
		n->w1E = (int16_t)(r - r / 32);
		n->c++;
		return EndAt(n->c, 0x28);
	}

	// Streak (0x5EDC10): 2 ticks delayed; on its first draw the tip point is taken again. In screen space: from the tip's
	// screen point toward the node point's (screen coordinates / 8, made a unit direction), model 0xD81B34 turned from
	// (0x1000, 0, 0) onto it at the tip's screen point - (0xA0, 0x6C), depth the projection distance, scaled (w1C, w20, w20);
	// fading from 0x20; w20 = 2 sin(c x 0x400 / 15 + 0x400) + 0x80 up to 0xF, then 0x80
	static void StreakDraw(const Node24 *n, const int16_t *tip, int16_t sy, int32_t c, int32_t fade)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(&n->w10);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy);
		int32_t v[3];
		const int16_t ax = (int16_t)(sxy[0] / 8), ay = (int16_t)(sxy[1] / 8);
		v[2] = 0;
		v[0] = ax;
		v[1] = ay;
		GteLoadV0(tip);
		GteRTPS();
		GteReadSXY2(sxy);
		const int16_t bx = (int16_t)(sxy[0] / 8), by = (int16_t)(sxy[1] / 8);
		v[0] = v[0] - bx;
		v[1] = v[1] - by;
		Normalize(v, v);
		int32_t axis[4];
		const int32_t angle = RotationBetweenVectors((const int32_t *)REF_RIGHT, v, axis);
		Mat4x3 m = {};
		BuildAxisAngleRotationMatrix(angle, &m, axis);
		m.t[0] = bx - 0xA0;
		m.t[1] = by - 0x6C;
		m.t[2] = ProjectionH();
		int32_t sv[3];
		sv[2] = sy;
		sv[1] = sy;
		sv[0] = n->w1C;
		Scale3DMatrix(&m, sv);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = ModelHeader(MODEL_STREAK, c >= 0x20, fade);
		RenderToCursor(h);
		FieldFree(0x58);
	}

	static int16_t StreakWidth(int32_t c) { return c <= 0xF ? (int16_t)((shl32(ComputeSin(shl32(c, 10) / 15 + 0x400), 13) >> 12) + 0x80) : 0x80; }

	static uint32_t __cdecl StreakTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->e > 0)
		{
			if (DrawOnly()) return 0;
			n->e--;
			return 0;
		}
		if (n->c == 0) SetTipPoint();
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_Streak, n);)
		StreakDraw(n, (const int16_t *)TIP, n->w20, n->c, shl32(n->c - 0x20, 9));
		if (DrawOnly()) return 0;
		n->w20 = StreakWidth(n->c);
		n->c++;
		return EndAt(n->c, 0x28);
	}

	// FinalRing (0x5EE0F0): turned about y at the target's anchor, scaled; fading from 0xC
	static void FinalRingDraw(const Node24 *n, int16_t s, int32_t c, int32_t fade)
	{
		Mat4x3 m = {};
		WorldMatrix(&m, 0, n->w18, 0, n->w10, n->w12, n->w14, s, s, s, &Camera());
		uint8_t *h = ModelHeader(MODEL_FINALRING, c >= 0xC, fade);
		RenderToCursor(h);
		FieldFree(0x58);
	}

	static uint32_t __cdecl FinalRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_FinalRing, n);)
		FinalRingDraw(n, n->w1C, n->c, mul32(n->c - 0xC, 682));
		if (DrawOnly()) return 0;
		const int16_t r = n->w1E;
		n->w1C = (int16_t)(n->w1C + r);
		n->w1E = (int16_t)(r - r / 4);
		n->c++;
		return EndAt(n->c, 0x12);
	}

	// FinalFlash (0x5EE850): camera-facing (shadow rotation scale 0x1000) at the target's anchor, angles (0x100, 0, 0x100),
	// scaled, mode 1 into the frame arena; fading from 0x10. Its size gives FinalRingSparks' radius (0x23C6936 = 850 w1C >> 12).
	static void FinalFlashDraw(const Node24 *n, int16_t s, int32_t c, int32_t fade)
	{
		Mat4x3 m = {};
		int16_t ang[4];
		ang[0] = 0x100;
		ang[2] = 0x100;
		ang[1] = 0;
		ComposeZYXRotationMatrix(ang, &m);
		int32_t sv[3];
		sv[2] = s;
		sv[1] = s;
		sv[0] = s;
		Scale3DMatrix(&m, sv);
		TransformCameraByShadowRotation(&n->w10, 0x1000, 0);
		GteSetRotMatrix(&m);
		uint8_t *h = ModelHeader(MODEL_FINALFLASH, c >= 0x10, fade);
		FrameArena() = RenderPrimModel(h, RenderOT(), 1, FrameArena());
		FieldFree(0x58);
	}

	static uint32_t __cdecl FinalFlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		var<int16_t>(TIP + 6) = (int16_t)(mul32(n->w1C, 850) >> 12);
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_FinalFlash, n);)
		FinalFlashDraw(n, n->w1C, n->c, shl32(n->c - 0x10, 9));
		if (DrawOnly()) return 0;
		const int16_t r = n->w1E;
		n->w1C = (int16_t)(n->w1C + r);
		n->w1E = (int16_t)(r - r / 8);
		n->c++;
		return EndAt(n->c, 0x18);
	}

	// TargetRing (0x5EFFF0): at the kept target position (y 0), scaled; fading from 6
	static void TargetRingDraw(const Node24 *n, int16_t s, int32_t c, int32_t fade)
	{
		Mat4x3 m = {};
		WorldMatrix(&m, 0, 0, 0, n->w10, 0, n->w14, s, s, s, &Camera());
		uint8_t *h = ModelHeader(MODEL_ROCK, c >= 6, fade);
		RenderToCursor(h);
		FieldFree(0x58);
	}

	static uint32_t __cdecl TargetRingTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_node(ORIG_TargetRing, n);)
		TargetRingDraw(n, n->w1C, n->c, mul32(n->c - 6, 409));
		if (DrawOnly()) return 0;
		const int16_t r = n->w1E;
		n->w1C = (int16_t)(n->w1C + r);
		n->w1E = (int16_t)(r - r / 6);
		n->c++;
		return EndAt(n->c, 0x10);
	}

	// ------------------------------------------------------------------
	// Pool A model tasks: RockBurst (0x5EAAF0), Debris (0x5EAD50), TipShards (0x5EB8D0); pool B: HitShards (0x5ECE60), TipRocks
	// (0x5ED8E0), FinalShards (0x5EE4C0)
	// ------------------------------------------------------------------
	// RockBurst entry: +0x08 position, +0x10 scale, +0x12 its rate (shrinking by 1/5); fading from life 4; freed at 0xE
	static int32_t RockFade(int32_t life) { return mul32(life - 4, 409); }
	static void RockDraw(ModelWork *w, uint8_t *h, const PoolE *p, int16_t s, int32_t life, int32_t fade)
	{
		ComposeZYXRotationMatrix(w->ang, &w->m2);
		w->m2.t[0] = p->a[0];
		w->m2.t[1] = p->a[1];
		w->m2.t[2] = p->a[2];
		w->v[2] = s;
		w->v[1] = s;
		w->v[0] = s;
		Scale3DMatrix(&w->m2, w->v);
		ComposeAffineTransform(&w->m1, &w->m2, &w->m2);
		GteSetRotMatrix(&w->m2);
		GteSetTransVector(&w->m2);
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (life >= 4)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		RenderToCursor(h);
	}

	static uint32_t __cdecl RockBurstTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x68);
		if (!DrawOnly() && n->c <= 0x30)
		{
			*(uint32_t *)&w->ang[0] = *(const uint32_t *)&n->w10;
			*(uint32_t *)&w->ang[2] = *(const uint32_t *)&n->w14;
			if (n->c % 8 == 1)
			{
				const int32_t i = FreeA();
				if (i >= 0)
				{
					PoolE *p = EntryA(i);
					p->id = n->e;
					p->life = 0;
					*(uint32_t *)&p->a[0] = *(const uint32_t *)&n->w10;
					*(uint32_t *)&p->a[2] = *(const uint32_t *)&n->w14;
					p->a[5] = 0x600;
					p->a[4] = 0x600;
				}
			}
		}
		uint8_t *h = AllocHeader(0x58);
		w->m1 = Camera();
		const int32_t id = n->e;
		*(uint32_t *)h = MODEL_ROCK;
		*(uint32_t *)(h + 8) = 0;
		w->ang[2] = 0;
		w->ang[1] = 0;
		w->ang[0] = 0;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_RockBurst, n, h);)
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0x18)
		{
			PoolE *p = (PoolE *)a;
			if (p->id != id) continue;
			RockDraw(w, h, p, p->a[4], p->life, RockFade(p->life));
			if (DrawOnly()) continue;
			if (p->life >= 0xE) p->id = 0;
			else
			{
				p->life++;
				p->a[4] = (int16_t)(p->a[4] + p->a[5]);
				const int16_t r = p->a[5];
				p->a[5] = (int16_t)(r - r / 5);
			}
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		n->c++;
		if (n->c < 0x3A) return 0;
		ClearIdA(id);
		return TASK_END;
	}

	// Debris entry: +0x08 position, +0x0E y angle, +0x10 x / z scale, +0x12 y scale, +0x14 scale rate (1/24), +0x16 angle rate;
	// fading in up to life 6, out from 8; freed at 0x12
	static int32_t DebrisFade(int32_t life) { return life < 6 ? 0x1000 - mul32(life, 682) : mul32(life - 8, 409); }
	static void DebrisDraw(ModelWork *w, uint8_t *h, const PoolE *p, int16_t yaw, int16_t s, int32_t life, int32_t fade)
	{
		w->ang[1] = yaw;
		ComposeZYXRotationMatrix(w->ang, &w->m2);
		w->m2.t[0] = p->a[0];
		w->m2.t[1] = p->a[1];
		w->m2.t[2] = p->a[2];
		w->v[2] = s;
		w->v[0] = s;
		w->v[1] = p->a[5];
		Scale3DMatrix(&w->m2, w->v);
		ComposeAffineTransform(&w->m1, &w->m2, &w->m2);
		GteSetRotMatrix(&w->m2);
		GteSetTransVector(&w->m2);
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (life < 6 || life >= 8)
		{
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) = 0xF3;
		}
		RenderToCursor(h);
	}

	static uint32_t __cdecl DebrisTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x68);
		if (!DrawOnly() && n->c <= 0x30)
		{
			*(uint32_t *)&w->ang[0] = *(const uint32_t *)&n->w10;
			*(uint32_t *)&w->ang[2] = *(const uint32_t *)&n->w14;
			if (n->c % 10 == 2)
			{
				const int32_t i = FreeA();
				if (i >= 0)
				{
					PoolE *p = EntryA(i);
					p->id = n->e;
					p->life = 0;
					*(uint32_t *)&p->a[0] = *(const uint32_t *)&n->w10;
					*(uint32_t *)&p->a[2] = *(const uint32_t *)&n->w14;
					p->a[1] = (int16_t)(p->a[1] + (int16_t)(CrtRand() % 0x4B0 - 0x258));
					const int16_t s = (int16_t)((CrtRand() % 0x600 + 0x800) / 8);
					p->a[6] = s;
					p->a[4] = s;
					p->a[5] = (int16_t)(CrtRand() % 0x600 + 0x300);
					p->a[3] = (int16_t)(CrtRand() % 0x800);
					p->a[7] = (int16_t)(CrtRand() % 0x3C + 0x14);
				}
			}
		}
		uint8_t *h = AllocHeader(0x58);
		w->m1 = Camera();
		const int32_t id = n->e;
		*(uint32_t *)h = MODEL_DEBRIS;
		*(uint32_t *)(h + 8) = 0;
		w->ang[2] = 0;
		w->ang[0] = 0;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_Debris, n, h);)
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0x18)
		{
			PoolE *p = (PoolE *)a;
			if (p->id != id) continue;
			DebrisDraw(w, h, p, p->a[3], p->a[4], p->life, DebrisFade(p->life));
			if (DrawOnly()) continue;
			if (p->life >= 0x12) p->id = 0;
			else
			{
				p->life++;
				p->a[4] = (int16_t)(p->a[4] + p->a[6]);
				const int16_t r = p->a[6];
				p->a[6] = (int16_t)(r - r / 24);
				p->a[3] = (int16_t)(p->a[3] + p->a[7]);
			}
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		n->c++;
		if (n->c < 0x3A) return 0;
		ClearIdA(id);
		return TASK_END;
	}

	// TipShards entry: +0x0C phase (x scale = sin(phase) x scale), +0x0E its rate 0x80, +0x14 z angle, +0x16 its rate; screen
	// facing (view rotation only) at the tip's view position, scale (x, 0x3000, 0); fading in up to life 4, out from 10; freed
	// at 0x10
	static int32_t TipShardFade(int32_t life) { return life < 4 ? shl32(4 - life, 10) : mul32(life - 10, 682); }
	static void TipShardDraw(ModelWork *w, uint8_t *h, int16_t scale, int16_t phase, int16_t roll, int32_t life, int32_t fade)
	{
		w->ang[2] = roll;
		ComposeZYXRotationMatrix(w->ang, &w->m2);
		w->v[0] = mul32(ComputeSin(phase), scale) >> 12;
		Scale3DMatrix(&w->m2, w->v);
		GteSetRotMatrix(&w->m2);
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (life < 4)
		{
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) = 0xF3;
		}
		else if (life >= 0xA)
		{
			*(int32_t *)(h + 0xC) = fade;
			*(uint32_t *)(h + 0x1C) = 0xF3;
		}
		RenderToArena(h);
	}

	// the shards' frame: camera x (identity at the node's tip position): its translation into the GTE
	static void TipShardFrame(ModelWork *w, const Node24 *n, const Mat4x3 *view)
	{
		w->ang[0] = 0;
		w->ang[1] = 0;
		w->ang[2] = 0;
		ComposeZYXRotationMatrix(w->ang, &w->m1);
		w->m1.t[0] = n->w10;
		w->m1.t[1] = n->w12;
		w->m1.t[2] = n->w14;
		ComposeAffineTransform(view, &w->m1, &w->m1);
		GteSetTransVector(&w->m1);
	}

	static uint32_t __cdecl TipShardsTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x68);
		if (!DrawOnly() && n->c <= 0x22 && n->c % 3 == 1)
		{
			const int32_t i = FreeA();
			if (i >= 0)
			{
				PoolE *p = EntryA(i);
				p->id = n->e;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x3000 + 0x1000);
				p->a[2] = 0;
				p->a[3] = 0x80;
				p->a[6] = (int16_t)(CrtRand() % 0x1000);
				const int16_t r = (int16_t)(CrtRand() % 0x28 + 0xA);
				p->a[7] = r;
				if (r & 1) p->a[7] = (int16_t)-r;
			}
		}
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_TIPSHARD;
		*(uint32_t *)(h + 8) = 0;
		TipShardFrame(w, n, &Camera());
		const int32_t id = n->e;
		w->ang[1] = 0;
		w->ang[0] = 0;
		w->v[1] = 0x3000;
		w->v[2] = 0;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_TipShards, n, h);)
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0x18)
		{
			PoolE *p = (PoolE *)a;
			if (p->id != id) continue;
			TipShardDraw(w, h, p->scale, p->a[2], p->a[6], p->life, TipShardFade(p->life));
			if (DrawOnly()) continue;
			if (p->life >= 0x10) p->id = 0;
			else
			{
				p->life++;
				p->a[2] = (int16_t)(p->a[2] + p->a[3]);
				p->a[6] = (int16_t)(p->a[6] + p->a[7]);
			}
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		n->c++;
		if (n->c < 0x2C) return 0;
		ClearIdA(id);
		return TASK_END;
	}

	// pool B shard entry (HitShards / FinalShards): +0x08 position, +0x10 velocity (x / y; FinalShards: + z), decaying by 1/8
	static void ShardMove(PoolE2 *p, int k)
	{
		for (int i = 0; i < k; i++) p->a[i] = (int16_t)(p->a[i] + p->a[4 + i]);
		for (int i = 0; i < k; i++)
		{
			const int16_t v = p->a[4 + i];
			p->a[4 + i] = (int16_t)(v - (int16_t)(v >> 3));
		}
	}

	// HitShards entry: +0x08 position in the hit's frame (z 0), +0x10 velocity, +0x14 z angle; scale (0xC00, scale, 0) through
	// the hit's frame (w18 about z, onto the hit direction, at the hit point); fading from life 4; freed at 0xC
	static void HitShardDraw(ModelWork *w, uint8_t *h, const PoolE2 *p, const int16_t *pos, int32_t life, int32_t fade)
	{
		w->ang[2] = p->a[6];
		ComposeZYXRotationMatrix(w->ang, &w->m2);
		w->m2.t[0] = pos[0];
		w->m2.t[2] = pos[2];
		w->m2.t[1] = pos[1];
		w->v[1] = p->scale;
		Scale3DMatrix(&w->m2, w->v);
		ComposeAffineTransform(&w->m1, &w->m2, &w->m2);
		GteSetRotMatrix(&w->m2);
		GteSetTransVector(&w->m2);
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (life >= 4)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		RenderToArena(h);
	}

	// the hit's frame: (0, 0, w18) turned onto the hit direction, at the hit point, through the camera
	static void HitShardFrame(ModelWork *w, const Node24 *n, const HitRec *r, const Mat4x3 *view)
	{
		w->ang[0] = 0;
		w->ang[1] = 0;
		w->ang[2] = n->w18;
		ComposeZYXRotationMatrix(w->ang, &w->m1);
		const int32_t angle = RotationBetweenVectors((const int32_t *)REF_BACK, r->dir, w->v);
		BuildAxisAngleRotationMatrix(angle, &w->m2, w->v);
		GteMatrixMultiply(&w->m2, &w->m1);
		w->m1.t[0] = r->pos[0];
		w->m1.t[1] = r->pos[1];
		w->m1.t[2] = r->pos[2];
		ComposeAffineTransform(view, &w->m1, &w->m1);
		w->v[0] = 0xC00;
		w->ang[1] = 0;
		w->ang[0] = 0;
		w->v[2] = 0;
	}

	static uint32_t __cdecl HitShardsTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		const HitRec *r = Hit(n->w1A);
		ModelWork *w = (ModelWork *)FieldAlloc(0x68);
		if (!DrawOnly() && n->c <= 0)
		{
			for (int32_t j = 0; j < 0xC; j++)
			{
				const int32_t i = FreeB();
				if (i < 0) break;
				PoolE2 *p = EntryB(i);
				p->id = n->e;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x1800 + 0xC00);
				const int32_t a = CrtRand() % 0x1000;
				w->v[0] = -ComputeSin(a);
				w->v[1] = ComputeCos(a);
				const int32_t s = CrtRand() % 0x12C + 0x1F4;
				p->a[0] = (int16_t)(mul32(w->v[0], s) >> 12);
				p->a[1] = (int16_t)(mul32(w->v[1], s) >> 12);
				p->a[2] = 0;
				const int32_t s2 = CrtRand() % 0x12C + 0x8C;
				p->a[4] = (int16_t)(mul32(w->v[0], s2) >> 12);
				p->a[5] = (int16_t)(mul32(w->v[1], s2) >> 12);
				p->a[6] = (int16_t)a;
			}
		}
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)(h + 8) = 0;
		int32_t count = 0;
		*(uint32_t *)h = MODEL_SHARD;
		HitShardFrame(w, n, r, &Camera());
		const int32_t id = n->e;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_HitShards, n, h);)
		for (uint32_t a = POOL_B; a < POOL_B_END; a += 0x20)
		{
			PoolE2 *p = (PoolE2 *)a;
			if (p->id != id) continue;
			HitShardDraw(w, h, p, &p->a[0], p->life, shl32(p->life - 4, 9));
			if (DrawOnly()) continue;
			if (p->life >= 0xC) p->id = 0;
			else
			{
				p->life++;
				ShardMove(p, 2);
				count++;
			}
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		n->c++;
		return EndIfNone(count);
	}

	// TipRocks entry: +0x06 scale, +0x0E its rate (shrinking 1/18), +0x10 x angle, +0x12 its rate, +0x14 z angle, +0x16 its
	// rate; at the tip; fading from life 0x10; freed at 0x18
	static void TipRockDraw(ModelWork *w, uint8_t *h, const PoolE2 *p, int16_t s, int16_t ax, int16_t az, int32_t life, int32_t fade)
	{
		w->ang[0] = ax;
		w->ang[2] = az;
		ComposeZYXRotationMatrix(w->ang, &w->m2);
		w->m2.t[0] = p->a[0];
		w->m2.t[1] = p->a[1];
		w->m2.t[2] = p->a[2];
		w->v[2] = s;
		w->v[1] = s;
		w->v[0] = s;
		Scale3DMatrix(&w->m2, w->v);
		ComposeAffineTransform(&w->m1, &w->m2, &w->m2);
		GteSetRotMatrix(&w->m2);
		GteSetTransVector(&w->m2);
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (life >= 0x10)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		RenderToArena(h);
	}

	static uint32_t __cdecl TipRocksTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x68);
		if (!DrawOnly() && n->c <= 0xC && n->c % 4 == 0)
		{
			const int32_t i = FreeB();
			if (i >= 0)
			{
				PoolE2 *p = EntryB(i);
				p->id = n->e;
				p->life = 0;
				*(uint32_t *)&p->a[0] = *(const uint32_t *)&n->w10;
				*(uint32_t *)&p->a[2] = *(const uint32_t *)&n->w14;
				const int16_t s = (int16_t)((CrtRand() % 0x800 + 0x1000) / 18);
				p->a[3] = s;
				p->scale = s;
				p->a[4] = (int16_t)(CrtRand() % 0x1000);
				int16_t r = (int16_t)(CrtRand() % 0x20 + 0x10);
				p->a[5] = r;
				if (r & 1) p->a[5] = (int16_t)-r;
				p->a[6] = (int16_t)(CrtRand() % 0x1000);
				r = (int16_t)(CrtRand() % 0x20 + 0x10);
				p->a[7] = r;
				if (r & 1) p->a[7] = (int16_t)-r;
			}
		}
		uint8_t *h = AllocHeader(0x58);
		int32_t count = 0;
		*(uint32_t *)(h + 8) = 0;
		w->m1 = Camera();
		*(uint32_t *)h = MODEL_TIPROCK;
		w->ang[1] = 0;
		const int32_t id = n->e;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_TipRocks, n, h);)
		for (uint32_t a = POOL_B; a < POOL_B_END; a += 0x20)
		{
			PoolE2 *p = (PoolE2 *)a;
			if (p->id != id) continue;
			TipRockDraw(w, h, p, p->scale, p->a[4], p->a[6], p->life, shl32(p->life - 0x10, 9));
			if (DrawOnly()) continue;
			if (p->life >= 0x18) p->id = 0;
			else
			{
				p->life++;
				p->scale = (int16_t)(p->scale + p->a[3]);
				const int16_t r = p->a[3];
				p->a[3] = (int16_t)(r - r / 18);
				p->a[4] = (int16_t)(p->a[4] + p->a[5]);
				p->a[6] = (int16_t)(p->a[6] + p->a[7]);
				count++;
			}
		}
		FieldFree(0x58);
		FieldFree(0x68);
		if (DrawOnly()) return 0;
		n->c++;
		return EndIfNone(count);
	}

	// FinalShards entry: +0x08 position, +0x10 velocity (decaying 1/8), +0x18 direction (the model's up (0, -0x1000, 0) turned
	// onto it); scale (0x2000, scale, 0) through the frame (camera x the target's anchor); fading from life 6; freed at 0xC.
	// The draw loop walks 200 entries of 0x20 from pool B (past its 120 into the module globals)
	static void FinalShardDraw(ModelWork *w, uint8_t *h, const PoolE2 *p, const int16_t *pos, int32_t life, int32_t fade)
	{
		w->v[0] = p->a[8];
		w->v[1] = p->a[9];
		w->v[2] = p->a[10];
		const int32_t angle = RotationBetweenVectors((const int32_t *)REF_UP, w->v, w->w);
		BuildAxisAngleRotationMatrix(angle, &w->m2, w->w);
		w->m2.t[0] = pos[0];
		w->m2.t[1] = pos[1];
		w->m2.t[2] = pos[2];
		w->x[1] = p->scale;
		Scale3DMatrix(&w->m2, w->x);
		ComposeAffineTransform(&w->m1, &w->m2, &w->m2);
		GteSetRotMatrix(&w->m2);
		GteSetTransVector(&w->m2);
		*(uint32_t *)(h + 0x1C) = 0x33;
		if (life >= 6)
		{
			*(uint32_t *)(h + 0x1C) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		RenderToArena(h);
	}

	static void FinalShardFrame(ModelWork *w, const Node24 *n, const Mat4x3 *view)
	{
		w->ang[0] = 0;
		w->ang[1] = 0;
		w->ang[2] = 0;
		ComposeZYXRotationMatrix(w->ang, &w->m1);
		w->m1.t[0] = n->w10;
		w->m1.t[1] = n->w12;
		w->m1.t[2] = n->w14;
		ComposeAffineTransform(view, &w->m1, &w->m1);
		w->x[0] = 0x2000;
		w->x[2] = 0;
	}

	static uint32_t __cdecl FinalShardsTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x78);
		if (!DrawOnly() && n->c <= 4)
		{
			for (int32_t j = 0; j < 8; j++)
			{
				const int32_t i = FreeB();
				if (i < 0) break;
				PoolE2 *p = EntryB(i);
				p->id = n->e;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x1800 + 0x1C00);
				w->v[0] = CrtRand() % 0x2000 - 0x1000;
				w->v[1] = CrtRand() % 0x2000 - 0x1000;
				w->v[2] = CrtRand() % 0x2000 - 0x1000;
				Normalize(w->v, w->v);
				const int32_t s = CrtRand() % 0x320 + 0x384;
				p->a[0] = (int16_t)(mul32(w->v[0], s) >> 12);
				p->a[1] = (int16_t)(mul32(w->v[1], s) >> 12);
				p->a[2] = (int16_t)(mul32(w->v[2], s) >> 12);
				const int32_t s2 = CrtRand() % 0x1C2 + 0xFA;
				p->a[4] = (int16_t)(mul32(w->v[0], s2) >> 12);
				p->a[5] = (int16_t)(mul32(w->v[1], s2) >> 12);
				p->a[6] = (int16_t)(mul32(w->v[2], s2) >> 12);
				p->a[8] = (int16_t)w->v[0];
				p->a[9] = (int16_t)w->v[1];
				p->a[10] = (int16_t)w->v[2];
			}
		}
		uint8_t *h = AllocHeader(0x58);
		*(uint32_t *)h = MODEL_SHARD;
		*(uint32_t *)(h + 8) = 0;
		int32_t count = 0;
		FinalShardFrame(w, n, &Camera());
		const int32_t id = n->e;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_FinalShards, n, h);)
		for (uint32_t a = POOL_B; a < FINAL_SHARDS_END; a += 0x20)
		{
			PoolE2 *p = (PoolE2 *)a;
			if (p->id != id) continue;
			FinalShardDraw(w, h, p, &p->a[0], p->life, mul32(p->life - 6, 682));
			if (DrawOnly()) continue;
			if (p->life >= 0xC) p->id = 0;
			else
			{
				p->life++;
				ShardMove(p, 3);
				count++;
			}
		}
		FieldFree(0x58);
		FieldFree(0x78);
		if (DrawOnly()) return 0;
		n->c++;
		return EndIfNone(count);
	}

	// ------------------------------------------------------------------
	// Pool A / C sprite tasks: BoneSparks (0x5EB070), BladeSparks (0x5EB410), TargetSparks (0x5EFCE0), TipSmoke (0x5EBBD0),
	// HitSparkA (0x5EC550), HitSparkB (0x5EC7D0), FinalSparkA (0x5EE240), FinalRingSparks (0x5EE980)
	// ------------------------------------------------------------------
	// the sprite tasks' light matrix: `m` (a camera copy or the frame), BK its translation
	static void SpriteLight(const Mat4x3 *m)
	{
		GteSetLightMatrix(m);
		GteSetBackColorFromTrans(m);
	}

	// the sprite tasks' view position / its unit direction words in their work block (+0x38 / +0x28)
	static inline int32_t *Mac(ModelWork *w) { return (int32_t *)((uint8_t *)w + 0x38); }
	static inline int32_t *Dir(ModelWork *w) { return (int32_t *)((uint8_t *)w + 0x28); }

	// a sprite at pos (through the light matrix), moved toward the camera by scale / 8 (mac: the view position, dir: its unit
	// direction): sequence frame = life
	static void SmokeSprite(uint8_t *h, int32_t *mac, int32_t *dir, const int16_t *pos, int16_t scale, int16_t life)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(scale);
		*(int16_t *)(h + 4) = life;
		GteReadMAC123(mac);
		Normalize(mac, dir);
		const int32_t k = -(scale >> 3);
		mac[0] += mul32(dir[0], k) >> 12;
		mac[1] += mul32(dir[1], k) >> 12;
		mac[2] += mul32(dir[2], k) >> 12;
		GteSetTransFromVec32(mac);
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	// a sprite at pos through the light matrix (no move toward the camera)
	static void PlainSprite(uint8_t *h, const int16_t *pos, int16_t scale, int16_t frame)
	{
		GteLoadV0(pos);
		GteMVMVA_LightV0Bk();
		GteSetRotScale(scale);
		*(int16_t *)(h + 4) = frame;
		GteTransFromMAC();
	}

	static uint8_t *SequenceHeader(uint32_t seq, int16_t mode)
	{
		uint8_t *h = AllocHeader(0xB4);
		*(int16_t *)(h + 0x24) = mode;
		*(uint32_t *)h = seq;
		return h;
	}

	// BoneSparks: two sparks every 6 ticks at the caster's bones 0x13 / 0x14 (+-0x50), scale 0x380..0x5C0, static
	static void BoneSpark(PoolE *p, int16_t id, int32_t bone)
	{
		p->id = id;
		p->life = 0;
		p->scale = (int16_t)(CrtRand() % 0x240 + 0x380);
		GetEffectSpawnPosition(Caster(), bone, 0x1000, &p->a[0]);
		p->a[0] = (int16_t)(p->a[0] + (int16_t)(CrtRand() % 0xA0 - 0x50));
		p->a[1] = (int16_t)(p->a[1] + (int16_t)(CrtRand() % 0xA0 - 0x50));
		p->a[2] = (int16_t)(p->a[2] + (int16_t)(CrtRand() % 0xA0 - 0x50));
	}

	static uint32_t __cdecl BoneSparksTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x48);
		if (!DrawOnly() && n->c <= 0x30 && n->c % 6 == 1)
		{
			int32_t i = FreeA();
			if (i >= 0)
			{
				BoneSpark(EntryA(i), n->e, 0x13);
				for (i++; i < 0xC8; i++)
					if (EntryA(i)->id == 0) break;
				if (i < 0xC8) BoneSpark(EntryA(i), n->e, 0x14);
			}
		}
		uint8_t *h = SequenceHeader(SEQ_SPARK, 0);
		w->m1 = Camera();
		SpriteLight(&w->m1);
		const int32_t id = n->e;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_BoneSparks, n, h);)
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0x18)
		{
			PoolE *p = (PoolE *)a;
			if (p->id != id) continue;
			SmokeSprite(h, Mac(w), Dir(w), &p->a[0], p->scale, p->life);
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0) p->id = 0;
			else p->life++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		n->c++;
		if (n->c < 0x3A) return 0;
		ClearIdA(id);
		return TASK_END;
	}

	// BladeSparks: one spark per tick up to 0x0D at the caster's bone 0x13 (length 0xE00) while it is above y -50
	static uint32_t __cdecl BladeSparksTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x50);
		if (!DrawOnly() && n->c <= 0xD)
		{
			const int32_t i = FreeA();
			if (i >= 0)
			{
				int16_t *q = w->ang;
				GetEffectSpawnPosition(Caster(), 0x13, 0xE00, q);
				GetEffectSpawnPosition(Caster(), 0x13, 0xE00, q + 4);
				if (q[1] > q[5])
				{
					*(uint32_t *)&q[0] = *(const uint32_t *)&q[4];
					*(uint32_t *)&q[2] = *(const uint32_t *)&q[6];
				}
				if (q[1] > -0x32)
				{
					PoolE *p = EntryA(i);
					p->id = n->e;
					p->life = 0;
					p->scale = (int16_t)(CrtRand() % 0x240 + 0x380);
					*(uint32_t *)&p->a[0] = *(const uint32_t *)&q[0];
					*(uint32_t *)&p->a[2] = *(const uint32_t *)&q[2];
					p->a[0] = (int16_t)(p->a[0] + (int16_t)(CrtRand() % 0xA0 - 0x50));
					p->a[1] = (int16_t)(p->a[1] + (int16_t)(CrtRand() % 0xA0 - 0x50));
					p->a[2] = (int16_t)(p->a[2] + (int16_t)(CrtRand() % 0xA0 - 0x50));
				}
			}
		}
		uint8_t *h = SequenceHeader(SEQ_SPARK, 0);
		Mat4x3 *cam = (Mat4x3 *)((uint8_t *)w + 0x10);
		*cam = Camera();
		SpriteLight(cam);
		const int32_t id = n->e;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_BladeSparks, n, h);)
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0x18)
		{
			PoolE *p = (PoolE *)a;
			if (p->id != id) continue;
			SmokeSprite(h, (int32_t *)((uint8_t *)w + 0x40), (int32_t *)((uint8_t *)w + 0x30), &p->a[0], p->scale, p->life);
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0) p->id = 0;
			else p->life++;
		}
		FieldFree(0xB4);
		FieldFree(0x50);
		if (DrawOnly()) return 0;
		n->c++;
		return n->c > 0xD ? TASK_END : 0;
	}

	// TipSmoke entry: +0x08 radius, +0x0A y, +0x0C angle, +0x10 radius step (until life 0x10), +0x12 y step, +0x14 angle rate
	// (growing by 1/8); the sprite at (cos x radius, y, sin x radius) through the frame (angles (0, -0x80, 0x200), at the tip,
	// camera x), sequence 0xD7B4D4 (mode 8) frame life / 2, into the frame arena
	static void TipSmokeSprite(ModelWork *w, uint8_t *h, int16_t r, int16_t y, int16_t ang, int16_t scale, int16_t life)
	{
		int16_t *q = (int16_t *)((uint8_t *)w + 8);
		q[0] = (int16_t)(mul32(ComputeCos(ang), r) >> 12);
		q[1] = y;
		q[2] = (int16_t)(mul32(ComputeSin(ang), r) >> 12);
		PlainSprite(h, q, scale, (int16_t)(life >> 1));
		FrameArena() = InitEffectSequenceFromData(h, RenderOT(), 2, FrameArena());
	}

	static void TipSmokeFrame(ModelWork *w, const Node24 *n, const Mat4x3 *view)
	{
		Mat4x3 *m = (Mat4x3 *)((uint8_t *)w + 0x10);
		w->ang[0] = 0;
		w->ang[1] = -0x80;
		w->ang[2] = 0x200;
		ComposeZYXRotationMatrix(w->ang, m);
		m->t[0] = n->w10;
		m->t[1] = n->w12;
		m->t[2] = n->w14;
		ComposeAffineTransform(view, m, m);
		SpriteLight(m);
	}

	static uint32_t __cdecl TipSmokeTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x50);
		if (!DrawOnly() && n->c <= 0x32)
		{
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeA();
				if (i < 0) break;
				PoolE *p = EntryA(i);
				p->id = n->e;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x180 + 0x200);
				const int32_t a = CrtRand() % 0x1000;
				const int32_t r = CrtRand() % 0x258 + 0x2BC;
				p->a[0] = (int16_t)r;
				p->a[2] = (int16_t)a;
				p->a[1] = (int16_t)(CrtRand() % 0x4B0 - 0x258);
				p->a[4] = (int16_t)((r - 0x12C) / 16);
				p->a[6] = (int16_t)(CrtRand() % 0x5A + 0x28);
				const int32_t t = CrtRand() % 10 + 10;
				p->a[5] = (int16_t)(p->a[1] / t);
			}
		}
		uint8_t *h = SequenceHeader(SEQ_SMOKE, 8);
		TipSmokeFrame(w, n, &Camera());
		const int32_t id = n->e;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_TipSmoke, n, h);)
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0x18)
		{
			PoolE *p = (PoolE *)a;
			if (p->id != id) continue;
			TipSmokeSprite(w, h, p->a[0], p->a[1], p->a[2], p->scale, p->life);
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->id = 0;
				continue;
			}
			p->life++;
			if (p->life <= 0x10) p->a[0] = (int16_t)(p->a[0] - p->a[4]);
			p->a[2] = (int16_t)(p->a[2] + p->a[6]);
			const int16_t v = p->a[6];
			p->a[6] = (int16_t)(v + (int16_t)(v >> 3));
			p->a[1] = (int16_t)(p->a[1] - p->a[5]);
		}
		FieldFree(0xB4);
		FieldFree(0x50);
		if (DrawOnly()) return 0;
		n->c++;
		if (n->c < 0x2C) return 0;
		ClearIdA(id);
		return TASK_END;
	}

	// HitSparkA / FinalSparkA: static sparks (sequence 0xD7AFE0, mode 8) moved toward the camera; the task ends when no spark
	// is left
	static uint32_t SparkAPass(Node24 *n, ModelWork *w, uint8_t *h, uint32_t fn)
	{
		const int32_t id = n->e;
		int32_t count = 0;
		(void)fn;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(fn, n, h);)
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0x18)
		{
			PoolE *p = (PoolE *)a;
			if (p->id != id) continue;
			SmokeSprite(h, Mac(w), Dir(w), &p->a[0], p->scale, p->life);
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0) p->id = 0;
			else
			{
				p->life++;
				count++;
			}
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		n->c++;
		return EndIfNone(count);
	}

	static uint32_t __cdecl HitSparkATask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x48);
		if (!DrawOnly() && n->c <= 0)
		{
			*(uint32_t *)&w->ang[0] = *(const uint32_t *)&n->w10;
			*(uint32_t *)&w->ang[2] = *(const uint32_t *)&n->w14;
			for (int32_t j = 0; j < 3; j++)
			{
				const int32_t i = FreeA();
				if (i < 0) break;
				PoolE *p = EntryA(i);
				p->id = n->e;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0xA00 + 0x600);
				*(uint32_t *)&p->a[0] = *(const uint32_t *)&w->ang[0];
				*(uint32_t *)&p->a[2] = *(const uint32_t *)&w->ang[2];
				p->a[0] = (int16_t)(p->a[0] + (int16_t)(CrtRand() % 0x258 - 0x12C));
				p->a[1] = (int16_t)(p->a[1] + (int16_t)(CrtRand() % 0x258 - 0x12C));
				p->a[2] = (int16_t)(p->a[2] + (int16_t)(CrtRand() % 0x258 - 0x12C));
			}
		}
		uint8_t *h = SequenceHeader(SEQ_HITA, 8);
		w->m1 = Camera();
		SpriteLight(&w->m1);
		return SparkAPass(n, w, h, ORIG_HitSparkA);
	}

	static uint32_t __cdecl FinalSparkATask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x48);
		if (!DrawOnly() && n->c <= 4)
		{
			*(uint32_t *)&w->ang[0] = *(const uint32_t *)&n->w10;
			*(uint32_t *)&w->ang[2] = *(const uint32_t *)&n->w14;
			for (int32_t j = 0; j < 6; j++)
			{
				const int32_t i = FreeA();
				if (i < 0) break;
				PoolE *p = EntryA(i);
				p->id = n->e;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x1000 + 0xE00);
				*(uint32_t *)&p->a[0] = *(const uint32_t *)&w->ang[0];
				*(uint32_t *)&p->a[2] = *(const uint32_t *)&w->ang[2];
				p->a[0] = (int16_t)(p->a[0] + (int16_t)(CrtRand() % 0x7D0 - 0x3E8));
				p->a[1] = (int16_t)(p->a[1] + (int16_t)(CrtRand() % 0x7D0 - 0x3E8));
				p->a[2] = (int16_t)(p->a[2] + (int16_t)(CrtRand() % 0x7D0 - 0x3E8));
			}
		}
		uint8_t *h = SequenceHeader(SEQ_HITA, 8);
		w->m1 = Camera();
		SpriteLight(&w->m1);
		return SparkAPass(n, w, h, ORIG_FinalSparkA);
	}

	// HitSparkB entry: +0x08 position, +0x10 velocity (decaying 1/8); 12 at the hit point (sequence 0xD7B340, mode 8)
	static uint32_t __cdecl HitSparkBTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x48);
		if (!DrawOnly() && n->c <= 0)
		{
			*(uint32_t *)&w->ang[0] = *(const uint32_t *)&n->w10;
			*(uint32_t *)&w->ang[2] = *(const uint32_t *)&n->w14;
			int32_t *v = (int32_t *)&w->m2;
			for (int32_t j = 0; j < 0xC; j++)
			{
				const int32_t i = FreeA();
				if (i < 0) break;
				PoolE *p = EntryA(i);
				p->id = n->e;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x680 + 0x380);
				*(uint32_t *)&p->a[0] = *(const uint32_t *)&w->ang[0];
				*(uint32_t *)&p->a[2] = *(const uint32_t *)&w->ang[2];
				v[0] = CrtRand() % 0x2000 - 0x1000;
				v[1] = CrtRand() % 0x2000 - 0x1000;
				v[2] = CrtRand() % 0x2000 - 0x1000;
				Normalize(v, v);
				const int32_t s = CrtRand() % 0x12C + 0x96;
				p->a[0] = (int16_t)(p->a[0] + (int16_t)(mul32(v[0], s) >> 12));
				p->a[1] = (int16_t)(p->a[1] + (int16_t)(mul32(v[1], s) >> 12));
				p->a[2] = (int16_t)(p->a[2] + (int16_t)(mul32(v[2], s) >> 12));
				const int32_t s2 = CrtRand() % 0x15E + 0x96;
				p->a[4] = (int16_t)(mul32(v[0], s2) >> 12);
				p->a[5] = (int16_t)(mul32(v[1], s2) >> 12);
				p->a[6] = (int16_t)(mul32(v[2], s2) >> 12);
			}
		}
		uint8_t *h = SequenceHeader(SEQ_HITB, 8);
		w->m1 = Camera();
		int32_t count = 0;
		SpriteLight(&w->m1);
		const int32_t id = n->e;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_HitSparkB, n, h);)
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0x18)
		{
			PoolE *p = (PoolE *)a;
			if (p->id != id) continue;
			SmokeSprite(h, Mac(w), Dir(w), &p->a[0], p->scale, p->life);
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->id = 0;
				continue;
			}
			p->life++;
			for (int k = 0; k < 3; k++) p->a[k] = (int16_t)(p->a[k] + p->a[4 + k]);
			for (int k = 0; k < 3; k++)
			{
				const int16_t v = p->a[4 + k];
				p->a[4 + k] = (int16_t)(v - (int16_t)(v >> 3));
			}
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		n->c++;
		return EndIfNone(count);
	}

	// TargetSparks entry: +0x08 x, +0x0A y, +0x0C z, +0x10 / +0x14 x / z velocity (decaying 1/8); 8 around the kept target
	// position (frame: camera x (x, 0, z)), sequence 0xD7B1C8 (mode 8), moved toward the camera
	static uint32_t __cdecl TargetSparksTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		ModelWork *w = (ModelWork *)FieldAlloc(0x48);
		if (!DrawOnly() && n->c <= 3)
		{
			for (int32_t j = 0; j < 8; j++)
			{
				const int32_t i = FreeA();
				if (i < 0) break;
				PoolE *p = EntryA(i);
				p->id = n->e;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0xA00 + 0x800);
				const int32_t a = CrtRand() % 0x1000;
				int32_t *sc = (int32_t *)&w->m2; // (+0x28 / +0x30: the sine / cosine)
				sc[0] = ComputeSin(a);
				sc[2] = ComputeCos(a);
				const int32_t r = CrtRand() % 0xC8 + 0x190;
				p->a[0] = (int16_t)(mul32(sc[0], r) >> 12);
				const int32_t y = CrtRand() % 0x32;
				p->a[1] = (int16_t)-y;
				p->a[2] = (int16_t)(mul32(r, sc[2]) >> 12);
				const int32_t s2 = CrtRand() % 0x118 + 0x78;
				p->a[4] = (int16_t)(mul32(sc[0], s2) >> 12);
				p->a[6] = (int16_t)(mul32(s2, sc[2]) >> 12);
			}
		}
		uint8_t *h = SequenceHeader(SEQ_SPARK, 8);
		int32_t count = 0;
		w->ang[0] = 0;
		w->ang[1] = 0;
		w->ang[2] = 0;
		ComposeZYXRotationMatrix(w->ang, &w->m1);
		w->m1.t[0] = n->w10;
		w->m1.t[1] = 0;
		w->m1.t[2] = n->w14;
		ComposeAffineTransform(&Camera(), &w->m1, &w->m1);
		SpriteLight(&w->m1);
		const int32_t id = n->e;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_TargetSparks, n, h);)
		for (uint32_t a = POOL_A; a < POOL_A_END; a += 0x18)
		{
			PoolE *p = (PoolE *)a;
			if (p->id != id) continue;
			SmokeSprite(h, Mac(w), Dir(w), &p->a[0], p->scale, p->life);
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0)
			{
				p->id = 0;
				continue;
			}
			p->life++;
			p->a[0] = (int16_t)(p->a[0] + p->a[4]);
			p->a[2] = (int16_t)(p->a[2] + p->a[6]);
			int16_t v = p->a[4];
			p->a[4] = (int16_t)(v - (int16_t)(v >> 3));
			v = p->a[6];
			p->a[6] = (int16_t)(v - (int16_t)(v >> 3));
			count++;
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		n->c++;
		return EndIfNone(count);
	}

	// FinalRingSparks (pool C at the texture base, 500 x 0x18): 20 per tick up to 0x18 on a ring of FinalFlash's radius around
	// the target's anchor (light matrix: angles (0x100, 0, 0x100), translation the anchor in view space), sequence 0xD7B340
	// (mode 8)
	static uint32_t __cdecl FinalRingSparksTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		const uint32_t base = PoolCBase();
		ModelWork *w = (ModelWork *)FieldAlloc(0x48);
		if (!DrawOnly() && n->c <= 0x18)
		{
			int32_t *sc = (int32_t *)&w->m2;
			for (int32_t j = 0; j < 0x14; j++)
			{
				int32_t i = 0;
				uint32_t a = base;
				for (; *(const int32_t *)a != 0; a += 0x18)
					if (++i >= 0x1F4) break;
				if (i >= 0x1F4) break;
				PoolE *p = (PoolE *)(base + 0x18 * i);
				p->id = n->e;
				p->life = 0;
				p->scale = (int16_t)(CrtRand() % 0x400 + 0x200);
				const int32_t ang = CrtRand() % 0x1000;
				sc[0] = ComputeSin(ang);
				sc[2] = ComputeCos(ang);
				const int32_t r = var<int16_t>(TIP + 6) - CrtRand() % 0x64;
				p->a[0] = (int16_t)(mul32(sc[0], r) >> 12);
				const int32_t y = CrtRand() % 0xC8;
				p->a[2] = (int16_t)(mul32(r, sc[2]) >> 12);
				p->a[1] = (int16_t)(y - 0x64);
			}
		}
		uint8_t *h = SequenceHeader(SEQ_HITB, 8);
		int32_t count = 0;
		w->ang[0] = 0x100;
		w->ang[1] = 0;
		w->ang[2] = 0x100;
		ComposeZYXRotationMatrix(w->ang, &w->m1);
		w->m1.t[0] = n->w10;
		w->m1.t[1] = n->w12;
		w->m1.t[2] = n->w14;
		TransformVectorBy3x3Matrix(&Camera(), w->m1.t, w->m1.t);
		w->m1.t[0] += Camera().t[0];
		w->m1.t[1] += Camera().t[1];
		w->m1.t[2] += Camera().t[2];
		SpriteLight(&w->m1);
		const int32_t id = n->e;
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_pool(ORIG_FinalRingSparks, n, h);)
		for (uint32_t a = base, k = 0; k < 0x1F4; k++, a += 0x18)
		{
			PoolE *p = (PoolE *)a;
			if (p->id != id) continue;
			PlainSprite(h, &p->a[0], p->scale, p->life);
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
			if (DrawOnly()) continue;
			if (*(const int16_t *)(h + 0x28) < 0) p->id = 0;
			else
			{
				p->life++;
				count++;
			}
		}
		FieldFree(0xB4);
		FieldFree(0x48);
		if (DrawOnly()) return 0;
		n->c++;
		return EndIfNone(count);
	}

	// ------------------------------------------------------------------
	// Hits: HitDirector (0x5EC140), TargetBounce (0x5ED1A0)
	// ------------------------------------------------------------------
	// THE NEVER-WRITTEN STACK WORD: the hit point is a 4-word local whose 4th word ([entry esp - 2]) matrixMultiplyVector
	// does not write; it goes with the point into the previous hit point 0x23C6736, the hit record (+6) and the hit spark
	// nodes (+0x16, then their sparks' +0x0E). The game's executor 0x508420 calls every task from one esp: the word is the
	// high half of what the task before HitDirector in the main queue left at [entry esp - 4]. That task is the Trail (alive
	// from tick 1 to 0x12E, HitDirector from 0x5B to 0xC9, everything else is queued after them): its colour accumulator
	// local, 30 x 0x1000 = 0x1E000 after its draw (word 0x0001). Modelled: the Trail port notes it when it drew; HitDirector
	// takes it when its predecessor in the queue is the Trail, else the stack word itself.

	static bool PredecessorIsTrail(const TaskNode *self)
	{
		const TaskNode *prev = nullptr;
		for (const TaskNode *t = QMain()->head; t && t != self; t = t->next) prev = t;
		return prev && (uint32_t)prev->func == ORIG_Trail;
	}

	static uint32_t HitDirectorBody(Node24 *n, uint32_t slot4)
	{
		for (int32_t i = 0; i < 0xF; i++)
		{
			const int32_t t = var<int32_t>(HIT_TIMES + 4 * i);
			if (n->c > t) continue;
			if (n->c < t) break;
			int16_t v[4];
			v[3] = (int16_t)(slot4 >> 16);
			MatVec((const Mat4x3 *)(Caster() + 0x40), (const int16_t *)(HIT_OFFSETS + 8 * i), v);
			v[0] = (int16_t)(v[0] + var<int16_t>(TIP0));
			v[1] = (int16_t)(v[1] + var<int16_t>(TIP0 + 2));
			v[2] = (int16_t)(v[2] + var<int16_t>(TIP0 + 4));
			if (i > 3)
			{
				v[0] = (int16_t)(((int32_t)var<int16_t>(PREV_HIT) + v[0]) >> 1);
				v[1] = (int16_t)(((int32_t)var<int16_t>(PREV_HIT + 2) + v[1]) >> 1);
				v[2] = (int16_t)(((int32_t)var<int16_t>(PREV_HIT + 4) + v[2]) >> 1);
			}
			const uint32_t p0 = *(const uint32_t *)&v[0], p1 = *(const uint32_t *)&v[2];
			var<uint32_t>(PREV_HIT) = p0;
			HitRec *r = Hit(i);
			*(uint32_t *)&r->pos[0] = p0;
			*(uint32_t *)&r->pos[2] = p1;
			var<uint32_t>(PREV_HIT + 4) = p1;
			for (int k = 0; k < 4; k++)
			{
				const uint32_t d = var<uint32_t>(TRAIL_DIR + 4 * k);
				var<uint32_t>(LAST_HIT + 4 * k) = d;
				((uint32_t *)r->dir)[k] = d;
			}
			Node24 *t1 = AddMain(ORIG_HitSparkA);
			t1->c = 0;
			t1->e = TakeId();
			*(uint32_t *)&t1->w10 = p0;
			*(uint32_t *)&t1->w14 = p1;
			Node24 *t2 = AddMain(ORIG_HitSparkB);
			t2->c = 0;
			t2->e = TakeId();
			*(uint32_t *)&t2->w10 = p0;
			*(uint32_t *)&t2->w14 = p1;
			Node24 *t3 = AddMain(ORIG_HitSlashA);
			t3->c = 0;
			t3->e = (int16_t)i;
			t3->w18 = (int16_t)(CrtRand() % 0x1000);
			const int16_t s3 = (int16_t)((CrtRand() % 0xA00 + 0xE00) / 3);
			t3->w1E = s3;
			t3->w1C = s3;
			Node24 *t4 = AddMain(ORIG_HitSlashB);
			t4->c = 0;
			t4->e = (int16_t)i;
			t4->w18 = (int16_t)(CrtRand() % 0x1000);
			const int16_t s4 = (int16_t)((CrtRand() % 0xC00 + 0xC00) / 3);
			t4->w1E = s4;
			t4->w1C = (int16_t)(s4 + 0x580);
			Node24 *t5 = AddMain(ORIG_HitShards);
			t5->e = TakeId();
			t5->c = 0;
			t5->w1A = (int16_t)i;
			if (i < 0xE)
			{
				const int32_t ts = TargetSlot();
				uint8_t *tg = Entity(ts);
				tg[1] |= 0x10;
				Node24 *b = AddMain(ORIG_TargetBounce);
				b->c = 0;
				b->e = (int16_t)ts;
				b->w1A = (int16_t)(i != 0 ? var<int32_t>(HIT_TIMES + 4 * (i + 1)) - var<int32_t>(HIT_TIMES + 4 * i) : 8);
				b->w1C = (int16_t)(var<int16_t>(HIT_HEIGHTS + 4 * i) - 0xFA0);
				uint8_t *e = *(uint8_t **)(tg + 0x8C);
				if (e && e != tg)
				{
					do
					{
						if (*e & 2)
						{
							const int32_t k = EntityIndex(e);
							if (k < 7)
							{
								Entity(k)[1] |= 0x10;
								Node24 *b2 = AddMain(ORIG_TargetBounce);
								b2->w1A = b->w1A;
								b2->w1C = b->w1C;
								b2->c = 0;
								b2->e = (int16_t)k;
							}
						}
						e = *(uint8_t **)(e + 0x8C);
					} while (e != tg);
				}
			}
			const int32_t k = ResultIndex();
			ResultIndex() = k + 1;
			ApplyActionResultToTarget(Context()->actions->targets + TARGET_STRIDE * k);
		}
		n->c++;
		return EndAt(n->c, 0x6E);
	}

	static uint32_t __cdecl HitDirectorTask(TaskNode *tn)
	{
		if (DrawOnly()) return 0;
		const uint32_t stack_word = *(const uint32_t *)((const uint8_t *)_AddressOfReturnAddress() - 4);
		const bool modelled = g_trail_slot4_known && PredecessorIsTrail(tn);
		return HitDirectorBody((Node24 *)tn, modelled ? g_trail_slot4 : stack_word);
	}

	// TargetBounce: the entity's y = its y at the start + (rising: -70 counter; else sin(counter x 0x400 / duration) x height
	// >> 12; height = the table's - 4000 - the anchor's y)
	static int32_t BounceOffset(const Node24 *n, int32_t c)
	{
		if (n->w22 > 0) return -70 * c;
		return mul32(ComputeSin(shl32(c, 10) / n->w1A), n->w22) >> 12;
	}

	static uint32_t __cdecl TargetBounceTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		const int32_t slot = n->e;
		if (DrawOnly()) return 0;
		uint8_t *ent = Entity(slot);
		if (n->c == 0)
		{
			GetDefaultEffectPosition(ent, &n->w10);
			n->w20 = *(const int16_t *)(ent + 0x1E);
			n->w22 = (int16_t)(n->w1C - n->w12);
		}
		const int32_t d = BounceOffset(n, n->c);
		*(int16_t *)(ent + 0x1E) = (int16_t)(n->w20 + d);
		n->c++;
		return n->c >= n->w1A ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// TargetSink (0x5EECD0)
	// ------------------------------------------------------------------
	// the clipping renderer's block: the floor plane (0, -0x1000, 0) through (0, 0, 0)
	static void SinkPlane(ClipHeader *h)
	{
		int16_t *p = Slot(h, 0);
		p[0] = 0;
		p[1] = 0;
		p[2] = 0;
		h->plane[0] = 0;
		h->plane[1] = (int16_t)0xF000;
		h->plane[2] = 0;
		PlaneDistance(p, h->plane);
	}

	static uint32_t __cdecl TargetSinkTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		const int32_t slot = n->e;
		uint8_t *ent = Entity(slot);
		if (n->c == 0)
		{
			GetDefaultEffectPosition(ent, &n->w10);
			const uint32_t r1 = *(const uint32_t *)(ent + 0x10);
			n->w20 = n->w12;
			n->w22 = (int16_t)(n->w12 / 8);
			Keep(slot)->rot[0] = *(const uint32_t *)(ent + 0xC);
			Keep(slot)->rot[1] = r1;
		}
		if (n->c < 0x10)
		{
			ClipHeader *h = (ClipHeader *)FieldAlloc(0x1A8);
			SinkPlane(h);
			*ent |= 4;
			// 30 fps layer: see mag165_lion_heart_held.inc
			FX_HELD(held_note_sink(n, (const uint8_t *)h);)
			DrawClipped(h, ent);
			FieldFree(0x1A8);
		}
		const int16_t c = n->c;
		if (c <= 8)
		{
			const int16_t y = (int16_t)(n->w20 + var<int16_t>(TARGET_ANCHOR + 2));
			*(int16_t *)(ent + 0x10) = 0x800;
			*(int16_t *)(ent + 0x1E) = y;
		}
		if (DrawOnly()) return 0;
		if (c <= 8) n->w20 = (int16_t)(n->w20 - n->w22);
		if (c == 9)
		{
			Node24 *t = AddMain(ORIG_Shake);
			t->c = 0;
			t = AddMain(ORIG_TargetSparks);
			t->e = TakeId();
			*(uint32_t *)&t->w10 = var<uint32_t>(TARGET_KEPT);
			t->c = 0;
			*(uint32_t *)&t->w14 = var<uint32_t>(TARGET_KEPT + 4);
			t = AddMain(ORIG_TargetRing);
			*(uint32_t *)&t->w10 = var<uint32_t>(TARGET_KEPT);
			t->c = 0;
			*(uint32_t *)&t->w14 = var<uint32_t>(TARGET_KEPT + 4);
			const int16_t s = (int16_t)((CrtRand() % 0x800 + 0x2000) / 4);
			t->w1E = s;
			t->w1C = s;
			*(uint16_t *)ent = (uint16_t)(Keep(slot)->flag20 | (*(const uint16_t *)ent & 0xFFDF));
		}
		n->c = (int16_t)(c + 1);
		if (n->c < 0x10) return 0;
		if (n->c == 0x10)
		{
			*(uint32_t *)(ent + 0x1C) = Keep(slot)->pos[0];
			*(uint32_t *)(ent + 0x20) = Keep(slot)->pos[1];
			*(uint32_t *)(ent + 0xC) = Keep(slot)->rot[0];
			*(uint32_t *)(ent + 0x10) = Keep(slot)->rot[1];
			*(uint16_t *)ent &= 0xEFFF;
			if (slot == TargetSlot())
			{
				const int32_t k = ResultIndex();
				ResultIndex() = k + 1;
				ApplyActionResultToTarget(Context()->actions->targets + TARGET_STRIDE * k);
			}
			return 0;
		}
		*(uint16_t *)ent &= 0xFFFB;
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Flash (0x5E9D50), Shake (0x5F0130), Restore (0x5F01B0), Root (0x5F01F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl FlashTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		SetScreenFlash((uint32_t)(mul32(n->w18 / n->e, n->c) + 0x800), 0);
		if (DrawOnly()) return 0;
		n->c++;
		if (n->c < n->e) return 0;
		if (n->w1A != 0) StageGroupsShow();
		return TASK_END;
	}

	static uint32_t __cdecl ShakeTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		int32_t v = mul32(ComputeSin(shl32(n->c, 10) / 12 + 0x400), 200) >> 12;
		if (!(n->c & 1)) v = -v;
		var<int16_t>(0x1D97712) = (int16_t)v;
		if (DrawOnly()) return 0;
		n->c++;
		return EndAt(n->c, 0xC);
	}

	static uint32_t __cdecl RestoreTask(TaskNode *tn)
	{
		Node24 *n = (Node24 *)tn;
		if (n->c == 1) TextureRestoreTask((void *)TexBase(), &n->e);
		const int16_t done = n->e;
		n->c++;
		return done != 0 ? TASK_END : 0;
	}

	static uint32_t __cdecl RootTask(TaskNode *tn)
	{
		RootNode *r = (RootNode *)tn;
		if (r->c & 1) PacketCursor() = PacketArena() + 0x8000;
		else PacketCursor() = PacketArena();
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(held_note_root();)
		const int alive = ExecuteTaskQueue(QMain());
		r->c++;
		return alive ? 0 : TASK_END;
	}
}

	void register_mag165_lion_heart()
	{
		using namespace lion165;
		register_port(ORIG_Root, (void *)RootTask, "165 Root", 165);
		register_port(ORIG_Director, (void *)DirectorTask, "165 Director", 165);
		register_port(ORIG_Flash, (void *)FlashTask, "165 Flash", 165);
		register_port(ORIG_Trail, (void *)TrailTask, "165 Trail", 165);
		register_port(ORIG_GroundRing, (void *)GroundRingTask, "165 GroundRing", 165);
		register_port(ORIG_RockBurst, (void *)RockBurstTask, "165 RockBurst", 165);
		register_port(ORIG_Debris, (void *)DebrisTask, "165 Debris", 165);
		register_port(ORIG_BoneSparks, (void *)BoneSparksTask, "165 BoneSparks", 165);
		register_port(ORIG_BladeSparks, (void *)BladeSparksTask, "165 BladeSparks", 165);
		register_port(ORIG_TipFlash, (void *)TipFlashTask, "165 TipFlash", 165);
		register_port(ORIG_TipShards, (void *)TipShardsTask, "165 TipShards", 165);
		register_port(ORIG_TipSmoke, (void *)TipSmokeTask, "165 TipSmoke", 165);
		register_port(ORIG_TipRing, (void *)TipRingTask, "165 TipRing", 165);
		register_port(ORIG_HitDirector, (void *)HitDirectorTask, "165 HitDirector", 165);
		register_port(ORIG_HitSparkA, (void *)HitSparkATask, "165 HitSparkA", 165);
		register_port(ORIG_HitSparkB, (void *)HitSparkBTask, "165 HitSparkB", 165);
		register_port(ORIG_HitSlashA, (void *)HitSlashATask, "165 HitSlashA", 165);
		register_port(ORIG_HitSlashB, (void *)HitSlashBTask, "165 HitSlashB", 165);
		register_port(ORIG_HitShards, (void *)HitShardsTask, "165 HitShards", 165);
		register_port(ORIG_TargetBounce, (void *)TargetBounceTask, "165 TargetBounce", 165);
		register_port(ORIG_FlatRing, (void *)FlatRingTask, "165 FlatRing", 165);
		register_port(ORIG_WaveSpawner, (void *)WaveSpawnerTask, "165 WaveSpawner", 165);
		register_port(ORIG_ScreenWave, (void *)ScreenWaveTask, "165 ScreenWave", 165);
		register_port(ORIG_TipFlash2, (void *)TipFlash2Task, "165 TipFlash2", 165);
		register_port(ORIG_TipRocks, (void *)TipRocksTask, "165 TipRocks", 165);
		register_port(ORIG_Streak, (void *)StreakTask, "165 Streak", 165);
		register_port(ORIG_FinalRing, (void *)FinalRingTask, "165 FinalRing", 165);
		register_port(ORIG_FinalSparkA, (void *)FinalSparkATask, "165 FinalSparkA", 165);
		register_port(ORIG_FinalShards, (void *)FinalShardsTask, "165 FinalShards", 165);
		register_port(ORIG_FinalFlash, (void *)FinalFlashTask, "165 FinalFlash", 165);
		register_port(ORIG_FinalRingSparks, (void *)FinalRingSparksTask, "165 FinalRingSparks", 165);
		register_port(ORIG_TargetSink, (void *)TargetSinkTask, "165 TargetSink", 165);
		register_port(ORIG_TargetSparks, (void *)TargetSparksTask, "165 TargetSparks", 165);
		register_port(ORIG_TargetRing, (void *)TargetRingTask, "165 TargetRing", 165);
		register_port(ORIG_Shake, (void *)ShakeTask, "165 Shake", 165);
		register_port(ORIG_Restore, (void *)RestoreTask, "165 Restore", 165);
		// 30 fps layer: see mag165_lion_heart_held.inc
		FX_HELD(register_mag165_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag165_lion_heart_held.inc"
#endif
