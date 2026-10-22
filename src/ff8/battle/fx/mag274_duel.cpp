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

// Effect 274: Duel (Zell's limit break, kernel.bin Duel +4; MAG_274_DUEL_*): the effect of the whole Duel and of its six
// regular moves (Punch Rush, Booya, Heel Drop, Mach Kick, Dolphin Blow, Meteor Strike all use effect 274; the finishers are
// their own effects 212 / 112 / 216 / 215).
//
// How the battle drives it: BattleActionSequence_Tick_Duel (0x50BDC0) runs the effect's setup again for every Duel action,
// with g_MagicCastContext (0x1D99A78) +8 sequence_mode = move + 2 (moves 0..5), 8 for the end (+0x0A submode: 0 = time-out,
// 1..4 = a finisher takes over), +9 done cleared, and waits for done = 1. The first run (mode 0, the magic cast) starts the
// root task; a later run only posts the mode (0x25084D9) and a "new move" flag (0x25084D8) and starts no task (both bytes are
// the root node's +0x10 / +0x11). The input window itself is UI code (ZellDuel_ActionStep 0x4852B0).
//
// Structure (setup 0x6897E0 -> _Init 0x6898B0 (not ported: the setup), file loader 0x6897F0 = the texture file 0x110E5D0 and
// the stream callback 0x689810 (the engine's 0x534150 copy, called by the file loader, not ported)):
//   RootTask (0x689990) - node 0x18 (the root pool 0x25084C8): every tick the packet cursor = the arena pointer kept at magic
//     buffer + 0x92C / + 0x930 (alternating; written by the move start); on counter 1 sets up the move pool (1 x 0x74 at the
//     magic buffer) and starts one MoveTask on action 0's first target (its lowest chain entity, model height 0x6DFD90); runs
//     the move queue; ends with it.
//   MoveTask (0x689AC0) - every tick restores flag bit 4 of the target's chain entities from the flags saved by the setup;
//     mode 0: done at once (the menu opens). After a move: 3 ticks, then the Duel pause (0x68D600 entity bit 8, screen flash
//     0x800, letterbox bars 0x68D4B0 sliding in up to level 0x40); a new move fades them out (0x10 a tick) first. A move start:
//     the first one saves the caster's position / angles and the target's position, takes the target's default effect
//     position (+ height corrections for com ids 0x8C / 0x54; 0x54 also lifts the party by 1000), binds the module's
//     animation set to Zell (BattleEntity_BindExternalAnimSet 0x5022C0, com data 0x10E2998) and streams files 0x2CB /
//     0x2CC; every move places the two packet arenas, zeroes his facing, puts the target back, restores flag bits
//     0x1004, queues his chain = mode (QueueChainTransformation 0x505C00), starts the move's camera animation
//     (BS_GetCameraAnimationPointer 0x503520: set mode - 2, animation = the target's +0x91) and takes the move's length
//     (0x110E3E0); then the move's tick runs on counters 0..length: Punch Rush 0x68A050 (45), Booya 0x68B2F0 (45), Heel Drop
//     0x68B700 (42), Mach Kick 0x68B950 (42), Dolphin Blow 0x68C7B0 (49), Meteor Strike 0x68CA60 (80); after the length the
//     tick keeps running at counter = length; done = 1 at the end (the first move waits for the stream loader).
//     Mode 8 (6 ticks): positions / angles / flags put back (the party lift too), then with a finisher hand-off (submode) done
//     and the end at once, else the action result (no damage: time-out), the texture restore task, the end when it is done.
//   The moves (UPDATE AND DRAW in one call per tick, the counter = the move's tick):
//     every move places Zell in front of the target (0x68AE90: the target's default effect position minus the distance
//       along his facing + 0x800) on counter 0, turns the target to face him (+0x0E), records his effect bones for the
//       afterimages (0x68ADF0) on some counters and draws the afterimages (0x68A7B0: up to 2 ghosts of the recorded bone
//       matrices of his model, through 0x68A990 / 0x68AAE0, tinted by his colour);
//     prim-model layouts through the shared player (0x701970) with the module callback 0x68A400 (three renderers: 0x68BD50,
//       0x68C320 / 0x68C350 (scrolling texture window) and the engine's 0x7043B0);
//     sparks 0x68B140 (sprite sequences 0x110E3B8, 5 launch kinds 0x68AEE0, radial bursts 0x68B5E0) and hit flashes
//       0x68B270 (sprite 0x10E7868) in two queues;
//     Punch Rush: the target bounces (+0x20 -50 / +50 / -200), punch layouts at his fist (bone 0x16, slot 0's matrix);
//     Booya: the target follows his fist (bone 0x16) on counters 10..31, a layout at bone 9;
//     Heel Drop: two layouts at his foot (bone 0x14) with the battle camera's orthonormal copy (0x2508440) for 0x1000 parts;
//     Mach Kick: a wave model (0x10E9530, exe data) bent in place (0x68BCD0) from a backup on counters 28..31, put back at
//       37, drawn on 29..36 along his foot's motion;
//     Dolphin Blow: two layouts (one with the texture-scroll renderer), sparks;
//     Meteor Strike: the target is thrown along a table (0x110E3F0, counters 0..59: position / world matrix / flags +1 bit
//       0x10), Zell jumps to it (28) and rises (58..67), rocks 0x68D3B0, the target redrawn by the module (0x68D1D0: shadow,
//       bone world matrices with its own rotation / scale, RenderGeometry, pose matrices rebuilt; flags 0x1004 hide it from
//       the engine's draw), a layout at bone 0x13.
//   Damage: ApplyActionResultToTarget at 32 / 33 / 70 (each move once).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Writes to battle entities: Zell (caster): position +0x1C / +0x20 (every move start, Meteor Strike's jump / rise; put back
//   by mode 8), facing +0x0E (move start 0, put back by mode 8), the module's animation set (first move) / chain (every move start);
//   the target and its chain: flags bit 4 every tick (from the setup's copy), bits 0x1004 at move starts / mode 8, 0x1004 and
//   +1 bit 0x10 in Meteor Strike, position +0x1C..+0x20 (move starts, Punch Rush bounces, Booya, Meteor Strike, put back by
//   mode 8), world translation +0x54..+0x5C and bone matrices (Meteor Strike), facing +0x0E (every move tick); every entity:
//   flags bit 8 (Duel pause); the three party slots' +0x20 (+1000, com id 0x54). Damage: 0x506690.
// Camera: no camera word is written; every move starts an engine camera animation (0x503520); draws read the battle view
//   0x1D97778 themselves (Heel Drop / Dolphin Blow keep an orthonormal copy 0x2508440 per tick).
// Exe data written: the wave model's vertices 0x10E9538 (0x268 bytes, Mach Kick, restored at 37).
// Module globals: 0x2508410..0x25084E6 (texture file, saved entity flags 0x2508414 (7 x 4), magic buffer base 0x2508430,
//   allocation cursor 0x2508434, packet cursor 0x2508438, camera copy 0x2508440, context 0x250846C, caster 0x2508470, queues
//   0x2508478 rocks / 0x2508488 hit flashes / 0x2508498 sparks / 0x25084A8 move / 0x25084B8 root, root node 0x25084C8 (its
//   +0x10 / +0x11 = new-move flag / mode), saved party heights 0x25084E0).

#include "mag_common.h"

namespace ff8fx
{
namespace duel274
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t *SavedFlags() { return (uint32_t *)0x2508414; }           // the 7 entities' flag words (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2508430); }          // Magic_TextureOFF (magic buffer)
	inline uint32_t &Alloc() { return var<uint32_t>(0x2508434); }            // magic buffer allocation cursor
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2508438); }
	inline Mat4x3 *CamCopy() { return (Mat4x3 *)0x2508440; }                // orthonormal copy of the battle view
	inline CastContext *&Ctx() { return var<CastContext *>(0x250846C); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2508470); }
	inline TaskQueue *QRocks() { return (TaskQueue *)0x2508478; }           // Meteor Strike rocks (0x2C)
	inline TaskQueue *QFlashes() { return (TaskQueue *)0x2508488; }         // hit flashes (0x14)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2508498; }          // sparks (0x24)
	inline TaskQueue *QMoves() { return (TaskQueue *)0x25084A8; }           // the move task (1 x 0x74)
	inline uint8_t &NewMove() { return var<uint8_t>(0x25084D8); }            // root node +0x10: a move was posted
	inline uint8_t &PostedMode() { return var<uint8_t>(0x25084D9); }         // root node +0x11: the posted mode
	inline int16_t *SavedPartyY() { return (int16_t *)0x25084E0; }            // party +0x20 before the 0x54 lift
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }
	inline uint32_t &FramePackets() { return var<uint32_t>(0x1D8E054); }     // the frame's own packet cursor

	static const uint32_t ORIG_RootTask = 0x689990;
	static const uint32_t ORIG_MoveTask = 0x689AC0;
	static const uint32_t ORIG_SparkTask = 0x68B140;
	static const uint32_t ORIG_FlashTask = 0x68B270;
	static const uint32_t ORIG_RockTask = 0x68D3B0;

	static const uint32_t COM_Anim = 0x10E2998;          // com data of the module's animation set (bound to Zell)
	static const uint32_t SEQ_Flash = 0x10E7868;         // hit flash sprite sequence (8 frames)
	static const uint32_t MODEL_Wave = 0x10E7E18;        // Meteor Strike's rising wave (prim model)
	static const uint32_t MODEL_Rock = 0x10E9360;
	static const uint32_t MODEL_Kick = 0x10E9530;        // Mach Kick's wave (vertices at +8 rewritten)
	static const uint32_t LAYOUT_Punch = 0x10EA030, LAYOUT_PunchB = 0x10EC7D4, LAYOUT_Booya = 0x10ED700;
	static const uint32_t LAYOUT_Kick = 0x10F10F8, LAYOUT_Heel = 0x10F3808, LAYOUT_HeelB = 0x10F6BE0;
	static const uint32_t LAYOUT_Meteor = 0x10F73B0, LAYOUT_MeteorB = 0x10F89F4, LAYOUT_Dolphin = 0x10FE310, LAYOUT_DolphinB = 0x110198C;
	static const uint32_t CAMERA_Data = 0x110BE70;      // the move camera animations (+[0x110BE74] & 0xFFFF)
	static const int16_t *const BEND_Table = (const int16_t *)0x110D3B0;
	static const uint8_t *const SPARK_Table = (const uint8_t *)0x110E3B8; // 8 bytes: sequence, frames, ?, scale
	static const uint8_t *const LENGTH_Table = (const uint8_t *)0x110E3E0; // move length by mode
	static const int16_t *const THROW_Table = (const int16_t *)0x110E3F0;  // Meteor Strike: the target's path (4 words a tick)
	static const void *const SOUND_Punch = (const void *)0x110BE58;
	static const void *const SOUND_Booya = (const void *)0x110BE5C;
	static const void *const SOUND_Heel = (const void *)0x110BE60;
	static const void *const SOUND_Kick = (const void *)0x110BE64;
	static const void *const SOUND_Dolphin = (const void *)0x110BE68;
	static const void *const SOUND_Meteor = (const void *)0x110BE6C;

	// ---- engine functions not in fx_port.h / mag_common.h
	inline int32_t Rand() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }  // Effect_SharedLcg_Rand
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline uint32_t BindExternalAnimSet(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); }
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline void CameraAnimation(uint32_t data, uint32_t id) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x503520)(data, id); } // BS_GetCameraAnimationPointer
	inline uint8_t *EntityChainLowest(uint8_t *e) { return fn<uint8_t *(__cdecl *)(uint8_t *)>(0x501FF0)(e); }
	inline void MeasureModelHeight(uint8_t *e, int16_t *out) { fn<void (__cdecl *)(uint8_t *, int16_t *)>(0x6DFD90)(e, out); } // MAG_070_ShockwavePulsar_MeasureModelHeight
	inline void StreamQueueLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamIsBusy() { return fn<int32_t (__cdecl *)()>(0x534270)(); }
	inline void DecodeModelPrimLayout(uint32_t data, uint32_t layout, int32_t size) { fn<void (__cdecl *)(uint32_t, uint32_t, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationY(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(a, out); }
	inline void RotationZ(int32_t a, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x7012C0)(a, out); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline void MultiplyPSX(const void *a, const void *b, Mat4x3 *out) { fn<void (__cdecl *)(const void *, const void *, Mat4x3 *)>(0x56C090)(a, b, out); } // Matrix3x3_MultiplyPSX
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatrixMultiply3x3(Mat4x3 *a, const Mat4x3 *b) { fn<void (__cdecl *)(Mat4x3 *, const Mat4x3 *)>(0x56C220)(a, b); }
	inline void Apply3DTransform(const Mat4x3 *m, const int16_t *in, int32_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int32_t *)>(0x56C3D0)(m, in, out); }
	inline void BuildOrthonormalBasis(const int32_t *dir, Mat4x3 *out) { fn<void (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(dir, out); }
	inline void CopyDwords(uint32_t dst, uint32_t src, int32_t n) { fn<void (__cdecl *)(uint32_t, uint32_t, int32_t)>(0x6D67B0)(dst, src, n); } // MAG_104_CopyDwords
	inline uint32_t RenderPrimModel070(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_RenderPrimModel
	inline uint32_t DrawShadow(uint8_t *e, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(e, ot, a, cursor); } // BattleEntity_DrawShadow
	inline uint32_t RenderGeometry(uint32_t model, void *ctx, uint32_t ot, int32_t a, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, ctx, ot, a, cursor); }
	inline void ScreenFadeTask(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x5712C0)(a, b, c, d); }
	// GTE (software PSX GTE) helpers not in fx_port.h
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }      // IR = MAC = R * V0
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }      // IR = R * IR
	inline void GteLoadIRFromColumn(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E180)(p); }  // IR1..3 = p[0], p[3], p[6] (a matrix column)
	inline void GteStoreIRToColumn(void *p) { fn<void (__cdecl *)(void *)>(0x45E470)(p); }
	inline void GteLoadV0FromDwords(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E060)(p); }
	inline void GteLoadIR123FromBytes(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(p); }
	inline void GteStoreIR123LowBytes(void *p) { fn<void (__cdecl *)(void *)>(0x45E410)(p); }
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(int32_t *p) { fn<void (__cdecl *)(int32_t *)>(0x45E3C0)(p); }
	inline void GteSetSZ123(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45E1D0)(a, b, c, d); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(int32_t *p) { fn<void (__cdecl *)(int32_t *)>(0x45E3D0)(p); }   // GTE_ReadOTZ (a dword)
	inline void GteReadOTZdiv4(void *p) { fn<void (__cdecl *)(void *)>(0x45E250)(p); }      // GTE_ReadOTZ_SZ3div4
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(uint32_t *p) { fn<void (__cdecl *)(uint32_t *)>(0x45E210)(p); }
	inline void GteReadSXY012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteLoadRGBC(const void *p) { fn<void (__cdecl *)(const void *)>(0x45E110)(p); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *p) { fn<void (__cdecl *)(void *)>(0x45E360)(p); }
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); }    // GTE_LoadV0_u16
	inline void GteStoreIR123s(void *p) { fn<void (__cdecl *)(void *)>(0x45E3E0)(p); }            // GTE_StoreIR123 (3 x s16)

#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x18 bytes at 0x25084C8
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 1..10
		uint8_t action;    // +0x0E next action whose first target gets the move task
		uint8_t started;   // +0x0F move pool set up
		uint8_t new_move;  // +0x10 (0x25084D8)
		uint8_t mode;      // +0x11 (0x25084D9)
		uint16_t pad12;
		uint32_t arena;    // +0x14 packet arena parity
	};
	struct MoveNode // pool of 1 node of 0x74 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C the move's tick
		uint8_t action;    // +0x0E action index
		uint8_t mode;      // +0x0F the posted mode (0 = none yet, 2..7 moves, 8 end)
		uint8_t first;     // +0x10 first move not started yet / its streams pending
		uint8_t length;    // +0x11 the move's length (0x110E3E0)
		uint8_t wait;      // +0x12 0 = in a move, 1..3 = after it, 4 = Duel pause
		uint8_t level;     // +0x13 pause bars level (0..0x40)
		uint8_t *target;   // +0x14 the target's lowest chain entity
		uint32_t caster_pos[2]; // +0x18 Zell +0x1C..+0x23 (first move)
		uint32_t caster_rot[2]; // +0x20 Zell +0x0C..+0x13
		int16_t anchor[4]; // +0x28 the target's default effect position (Zell's place)
		uint32_t target_pos[2]; // +0x30 the target's +0x1C..+0x23
		uint8_t bind[0x10]; // +0x38 Zell's bind block (BattleEntity_BindExternalAnimSet)
		uint32_t layout_a; // +0x48 prim-model layouts (magic buffer)
		uint32_t layout_b; // +0x4C
		uint32_t ghosts;   // +0x50 afterimage block (0x488 bytes)
		int16_t p54[4];    // +0x54 a move's anchor (Booya / Heel Drop / Mach Kick: a bone; Dolphin Blow: Zell's place)
		int16_t height[2]; // +0x5C the target's model top / bottom (0x6DFD90)
		uint8_t w60[0x14]; // +0x60 move words (see the moves)
	};
	// the prim-player callback's parameter block (a stack block of the move ticks)
	struct PlayArg
	{
		Mat4x3 m;          // +0x00 the layout's frame (camera composed)
		int32_t depth;     // +0x20 engine renderer: header +0x18
		int16_t mode;      // +0x24 renderer: 1 = 0x68BD50, 2 = 0x68C320 (object 0), else 0x7043B0
		int16_t scroll;    // +0x26 renderer 2: texture v offset
		uint32_t morph;    // +0x28 blended vertex frames
	};
	// the afterimage draw context (0x68A7B0: the Field_Alloc block of 0x74 bytes from +0x20)
	struct GhostCtx
	{
		uint32_t polys;    // +0x00 polygon cursor
		uint32_t verts;    // +0x04 screen vertex buffer ([0x1D98B3C])
		int16_t ntri, nquad; // +0x08
		int16_t clip[4];   // +0x0C x0, y0, x1, y1 (x 8)
		uint32_t colour;   // +0x14
		uint32_t visible;  // +0x18 object mask (entity +0x7C)
		uint32_t v[8];     // +0x1C .. +0x3B vertex words (sxy, sz | flags) x 4
		int32_t mac0;      // +0x3C
		uint32_t pad40;
		int32_t otz;       // +0x44
		uint8_t pad48[4];
		uint8_t *ghosts;   // +0x4C afterimage block
		uint32_t slot;     // +0x50 the ghost's recorded matrices
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x18 && sizeof(MoveNode) == 0x74, "Duel nodes");
	static_assert(sizeof(PlayArg) == 0x2C && sizeof(GhostCtx) == 0x54, "Duel blocks");
	#pragma pack(push, 1)
	struct SparkNode
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t frame;     // +0x12
		int16_t vel[3];    // +0x14
		int16_t seq;       // +0x1A spark table entry (0x110E3B8)
		int16_t acc[3];    // +0x1C
		int16_t pad22;
	};
	struct FlashNode
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t frame;     // +0x12
	};
	struct RockNode
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t life;      // +0x12
		int16_t vel[3];    // +0x14
		int16_t pad1a;
		int16_t ang[3];    // +0x1C
		int16_t pad22;
		int16_t spin[3];   // +0x24
		int16_t pad2a;
	};
#pragma pack(pop)
	static_assert(sizeof(SparkNode) == 0x24 && sizeof(FlashNode) == 0x14 && sizeof(RockNode) == 0x2C, "Duel particle nodes");

}
}

#ifdef FF8_FX_HELD
#include "mag274_duel_held.h"
#endif

namespace ff8fx
{
namespace duel274
{
	using namespace eng;
	using namespace magc;

	inline int16_t &W16(void *p, int off) { return *(int16_t *)((uint8_t *)p + off); }
	inline uint16_t &U16(void *p, int off) { return *(uint16_t *)((uint8_t *)p + off); }
	inline int32_t &W32(void *p, int off) { return *(int32_t *)((uint8_t *)p + off); }
	inline uint32_t &U32(void *p, int off) { return *(uint32_t *)((uint8_t *)p + off); }
	inline uint8_t &B8(void *p, int off) { return *((uint8_t *)p + off); }

	// battle entity slot of an entity pointer
	inline int SlotOf(const uint8_t *e) { return (int)((e - (const uint8_t *)0x1D972C0) / 0x9C); }
	// the target data of action `i` (ApplyActionResultToTarget)
	inline uint8_t *ActionTargets(int i) { return Ctx()->actions[i].targets; }

	// ------------------------------------------------------------------
	// Afterimages (Zell's ghosts)
	// ------------------------------------------------------------------
	// MAG_210_sub_6A1230 (in 212's range, also called by Burning Rave): afterimage block init: no ghost yet, `n` recorded
	// bones (the 128-entry bone -> record map, 0xFF = not recorded)
	static void GhostInit(uint8_t *g, int32_t n, const int32_t *bones)
	{
		g[3] = 0;
		g[2] = 0;
		g[1] = 0;
		g[0] = 0;
		g[4] = (uint8_t)n;
		memset(g + 5, 0xFF, 0x80);
		for (int32_t i = 0; i < n; i++) g[5 + bones[i]] = (uint8_t)i;
	}

	// 0x68ADF0 (= Burning Rave's own copy region, Bloodfest 0x6F8200): the recorded bones' matrices of Zell's model into the
	// next ghost slot
	static void GhostRecord(uint8_t *entity, uint8_t *g)
	{
		g[0] = 1;
		const uint8_t s = g[2];
		g[3] = s;
		uint8_t *dst = g + 0x88 + (uint32_t)s * 0x200;
		g[2] = (uint8_t)(s + 1);
		if ((uint8_t)(s + 1) >= 2) g[2] = 0;
		g[1] = (uint8_t)(g[1] + 1);
		if (g[1] > 2) g[1] = 2;
		uint8_t *skel = **(uint8_t ***)(entity + 0x64);
		for (uint32_t b = 0; b < skel[0]; b++)
		{
			const int32_t k = (int8_t)g[5 + b];
			if (k >= 0) memcpy(dst + k * 0x20, skel + 0x20 + b * 0x30, 0x20);
		}
	}

	// MAG_210_sub_6A2A90 (in 212's range): a vertex group of an object through the current GTE matrix into the screen
	// vertex buffer (sxy, sz / 4, clip flags: 1 / 2 left / right, 4 / 8 top / bottom of the context's rectangle x 8;
	// behind the eye: flags 0x10 only)
	static void GhostVertices(uint8_t **cursor, uint8_t **out, GhostCtx *c)
	{
		uint8_t *src = *cursor;
		int32_t count = *(const int16_t *)src;
		uint8_t *o = *out;
		src += 2;
		for (; count != 0; count--)
		{
			GteLoadV0u(src);
			GteRTPS();
			GteReadSXY2(&c->v[0]);
			GteReadOTZdiv4(&c->v[1]);
			src += 6;
			if (U16(c, 0x20) == 0)
			{
				U16(o, 6) = 0x10;
			}
			else
			{
				U16(c, 0x22) = 0;
				const int32_t x = W16(c, 0x1C);
				if (x < (int32_t)c->clip[0] * 8) U16(c, 0x22) = 1;
				else if (x >= (int32_t)c->clip[2] * 8) U16(c, 0x22) = 2;
				const int32_t y = W16(c, 0x1E);
				if (y < (int32_t)c->clip[1] * 8) B8(c, 0x22) |= 4;
				else if (y >= (int32_t)c->clip[3] * 8) B8(c, 0x22) |= 8;
				U32(o, 0) = c->v[0];
				U32(o, 4) = c->v[1];
			}
			o += 8;
		}
		*cursor = src;
		*out = o;
	}

	// 0x68AAE0: the object's flat textured triangles / quads (semi-transparent, the context's colour) from the screen
	// vertex buffer; a polygon with a vertex behind the eye (flags >= 0x10), outside on one side or back-facing is dropped
	static uint32_t GhostPolys(GhostCtx *c, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint8_t *vb = (uint8_t *)c->verts;
		uint8_t *p = (uint8_t *)c->polys;
		int16_t n = c->ntri;
		c->ntri = (int16_t)(n - 1);
		if (n != 0)
		{
			do
			{
				c->v[0] = U32(vb, (U16(p, 0) & 0xFFF) * 8);
				c->v[1] = U32(vb, (U16(p, 0) & 0xFFF) * 8 + 4);
				if (U16(c, 0x22) < 0x10)
				{
					c->v[2] = U32(vb, (U16(p, 2) & 0xFFF) * 8);
					c->v[3] = U32(vb, (U16(p, 2) & 0xFFF) * 8 + 4);
					if (U16(c, 0x2A) < 0x10)
					{
						c->v[4] = U32(vb, (U16(p, 4) & 0xFFF) * 8);
						c->v[5] = U32(vb, (U16(p, 4) & 0xFFF) * 8 + 4);
						if (U16(c, 0x32) < 0x10 && (B8(c, 0x22) & (uint8_t)(B8(c, 0x2A) & B8(c, 0x32))) == 0)
						{
							GteSetSXY012(c->v[0], c->v[2], c->v[4]);
							GteNCLIP();
							GteReadMAC0(&c->mac0);
							if (c->mac0 >= 0)
							{
								GteSetSZ123(U16(c, 0x20), U16(c, 0x28), U16(c, 0x30));
								GteAVSZ3();
								U32((void *)pk, 0) = 0x7000000;
								U32((void *)pk, 4) = c->colour | 0x26000000;
								U32((void *)pk, 8) = c->v[0];
								U32((void *)pk, 0x10) = c->v[2];
								U32((void *)pk, 0x18) = c->v[4];
								U32((void *)pk, 0xC) = U32(p, 8);
								U32((void *)pk, 0x14) = U32(p, 0xC);
								U16((void *)pk, 0x16) &= 0xFDFF;
								U16((void *)pk, 0x1C) = U16(p, 6);
								GteReadOTZ32(&c->otz);
								InsertPrimAutoDepth(ot + (uint32_t)(c->otz >> shift) * 4, (void *)pk);
								pk += 0x20;
							}
						}
					}
				}
				n = c->ntri;
				p += 0x10;
				c->ntri = (int16_t)(n - 1);
			} while (n != 0);
		}
		n = c->nquad;
		c->nquad = (int16_t)(n - 1);
		if (n != 0)
		{
			p += 4; // (once: the loop's cursor is 4 bytes into the quad)
			do
			{
				c->v[0] = U32(vb, (U16(p, -4) & 0xFFF) * 8);
				c->v[1] = U32(vb, (U16(p, -4) & 0xFFF) * 8 + 4);
				if (U16(c, 0x22) < 0x10)
				{
					c->v[2] = U32(vb, (U16(p, -2) & 0xFFF) * 8);
					c->v[3] = U32(vb, (U16(p, -2) & 0xFFF) * 8 + 4);
					if (U16(c, 0x2A) < 0x10)
					{
						c->v[4] = U32(vb, (U16(p, 0) & 0xFFF) * 8);
						c->v[5] = U32(vb, (U16(p, 0) & 0xFFF) * 8 + 4);
						if (U16(c, 0x32) < 0x10)
						{
							c->v[6] = U32(vb, (U16(p, 2) & 0xFFF) * 8);
							c->v[7] = U32(vb, (U16(p, 2) & 0xFFF) * 8 + 4);
							if (U16(c, 0x3A) < 0x10 && (B8(c, 0x3A) & (uint8_t)(B8(c, 0x22) & (uint8_t)(B8(c, 0x2A) & B8(c, 0x32)))) == 0)
							{
								GteSetSXY012(c->v[0], c->v[2], c->v[4]);
								GteNCLIP();
								GteReadMAC0(&c->mac0);
								if (c->mac0 >= 0)
								{
									GteSetSZ0123(U16(c, 0x20), U16(c, 0x28), U16(c, 0x30), U16(c, 0x38));
									GteAVSZ4();
									U32((void *)pk, 0) = 0x9000000;
									U32((void *)pk, 4) = c->colour | 0x2E000000;
									U32((void *)pk, 8) = c->v[0];
									U32((void *)pk, 0x10) = c->v[2];
									U32((void *)pk, 0x18) = c->v[4];
									U32((void *)pk, 0x20) = c->v[6];
									U32((void *)pk, 0xC) = U32(p, 4);
									U32((void *)pk, 0x14) = U32(p, 8);
									U16((void *)pk, 0x1C) = U16(p, 0xC);
									U16((void *)pk, 0x16) &= 0xFDFF;
									U16((void *)pk, 0x24) = U16(p, 0xE);
									GteReadOTZ32(&c->otz);
									InsertPrimAutoDepth(ot + (uint32_t)(c->otz >> shift) * 4, (void *)pk);
									pk += 0x28;
								}
							}
						}
					}
				}
				n = c->nquad;
				p += 0x14;
				c->nquad = (int16_t)(n - 1);
			} while (n != 0);
		}
		return pk;
	}

	// 0x68A990: the model's visible objects (entity +0x7C mask) with their recorded bones' (ghost) matrices: vertex groups of
	// a bone not recorded are marked behind the eye; then the polygons
	static void GhostModel(uint8_t *model, GhostCtx *c, uint32_t ot, int32_t shift)
	{
		const uint8_t *geom = *(uint8_t **)(model + 4);
		const int32_t nobj = *(const int32_t *)geom;
		const uint32_t *offs = (const uint32_t *)(geom + 4);
		for (int32_t i = 0; i < nobj; i++)
		{
			uint8_t *obj = (uint8_t *)(offs[i] + *(uint32_t *)(model + 4));
			if (!(c->visible & (1u << i))) continue;
			uint8_t *out = (uint8_t *)c->verts;
			uint8_t *cur = obj + 2;
			int32_t groups = *(const int16_t *)obj;
			for (; groups > 0; groups--)
			{
				const int32_t bone = *(const int16_t *)cur;
				cur += 2;
				const int32_t k = (int8_t)c->ghosts[bone + 5];
				if (k >= 0)
				{
					const Mat4x3 *m = (const Mat4x3 *)(c->slot + (uint32_t)(k + 8) * 0x20);
					GteSetRotMatrixCtrl(m);
					GteSetTransVectorCtrl(m);
					GhostVertices(&cur, &out, c);
				}
				else
				{
					const int32_t nv = *(const int16_t *)cur;
					cur += nv * 6 + 2;
					for (int32_t v = nv; v != 0; v--)
					{
						U16(out, 6) = 0x10;
						out += 8;
					}
				}
			}
			cur = (uint8_t *)(((uint32_t)cur + 3) & ~3u);
			c->ntri = *(const int16_t *)cur;
			cur += 2;
			c->nquad = *(const int16_t *)cur;
			cur += 0xA;
			c->polys = (uint32_t)cur;
			FramePackets() = GhostPolys(c, ot, shift, FramePackets());
		}
	}

	// 0x68A7B0: the recorded ghosts (up to 2, the newer one first, brighter: his colour (+0x28) x 0x800 * 1.5, then
	// x 0x400 * 1.5) of Zell's model: every recorded bone matrix through the view x his world matrix (into the slot's second
	// half), the model drawn with them (0x68A990); the count drops by one on a tick without a recording
	static void GhostDraw(uint8_t *entity, uint8_t *g)
	{
		// 30 fps layer: see mag274_duel_held.inc
		FX_HELD(held_note_ghosts(entity, g);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x74);
		GhostCtx *c = (GhostCtx *)(h + 0x20);
		c->verts = var<uint32_t>(0x1D98B3C);
		c->clip[0] = 0;
		c->clip[1] = 0;
		c->clip[2] = 0x140;
		c->clip[3] = 0xD8;
		c->visible = *(const uint32_t *)(entity + 0x7C);
		ComposeAffineTransform(&Camera(), (const Mat4x3 *)(entity + 0x40), (Mat4x3 *)h);
		c->ghosts = g;
		int32_t w = 0x800;
		for (int32_t i = 0; i < g[1]; i++)
		{
			GteSetIR0((w >> 1) + w);
			GteLoadIR123FromBytes(entity + 0x28);
			GteGPF();
			GteStoreIR123LowBytes(&c->colour);
			B8(c, 0x17) = 0;
			c->slot = (uint32_t)g + 0x88 + (((uint32_t)g[3] - (uint32_t)i) & 1) * 0x200;
			for (int32_t b = 0; b < g[4]; b++)
			{
				const uint32_t r = c->slot + (uint32_t)b * 0x20;
				GteSetRotMatrixCtrl((const Mat4x3 *)h);
				GteLoadIRFromColumn((const void *)(r + 0));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x100));
				GteLoadIRFromColumn((const void *)(r + 2));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x102));
				GteLoadIRFromColumn((const void *)(r + 4));
				GteMVMVA_RotIR();
				GteStoreIRToColumn((void *)(r + 0x104));
				GteSetTransVectorCtrl((const Mat4x3 *)h);
				GteLoadV0FromDwords((const void *)(r + 0x14));
				GteMVMVA_RotV0Tr();
				GteReadMAC123((int32_t *)(r + 0x114));
			}
			GhostModel(*(uint8_t **)(entity + 0x64), c, OTBase() + 0x44, 0);
			w -= 0x400;
		}
		FieldFree(0x74);
		if (g[0] == 0 && g[1] != 0) g[1]--;
		g[0] = 0;
	}

	// ------------------------------------------------------------------
	// Module prim-model renderers (prim-player objects, header 0x58 on the Field_Alloc scratch)
	// ------------------------------------------------------------------
	// 0x68BDB0: textured triangles (0x1C packets between two draw-mode packets: 0xE1000220 / 0xE1000240)
	static uint32_t RenderTris(uint8_t *h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint8_t *list = *(uint8_t **)(h + 0x20);
		int32_t count = *(const int32_t *)list;
		uint8_t *p = list + 4;
		const uint32_t va = U32(h, 4);
		*(uint8_t **)(h + 0x20) = p;
		if (count <= 0)
		{
			*(uint8_t **)(h + 0x20) = p;
			return pk;
		}
		for (; count != 0; count--, p += 0x14)
		{
			GteLoadV012((const void *)(va + U16(p, 4) * 4), (const void *)(va + U16(p, 6) * 4), (const void *)(va + U16(p, 8) * 4));
			GteRTPT();
			U32((void *)pk, 0) = 0x6000000;
			U32((void *)pk, 4) = U32(p, 0);
			GteReadFLAG((uint32_t *)(h + 0x30));
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0((int32_t *)(h + 0x24));
			if (W32(h, 0x24) < 0 && !(B8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012((void *)(pk + 8), (void *)(pk + 0x10), (void *)(pk + 0x18));
			GteAVSZ3();
			int16_t x = W16((void *)pk, 8);
			if (x < 0 || x > 0xA00) clip = 1;
			x = W16((void *)pk, 0x10);
			if (x < 0 || x > 0xA00) clip |= 2;
			x = W16((void *)pk, 0x18);
			if (x < 0 || x > 0xA00) clip |= 4;
			x = W16((void *)pk, 0xA);
			if (x < 0 || x > 0x6C0) clip |= 0x10;
			x = W16((void *)pk, 0x12);
			if (x < 0 || x > 0x6C0) clip |= 0x20;
			x = W16((void *)pk, 0x1A);
			if (x < 0 || x > 0x6C0) clip |= 0x40;
			if ((clip & 7) == 7 || (clip & 0x70) == 0x70) continue;
			GteReadOTZ32((int32_t *)(h + 0x2C));
			U32((void *)pk, 0xC) = U32(p, 0xC);
			U32((void *)pk, 0x14) = U32(p, 0x10);
			const int32_t z = W32(h, 0x2C) + W32(h, 0xC);
			W32(h, 0x2C) = z;
			if (z < 0) continue;
			const uint32_t slot = ot + (uint32_t)(z >> shift) * 4;
			const uint32_t tri = pk;
			pk += 0x1C;
			U32((void *)pk, 0) = 0x1000000;
			U32((void *)pk, 4) = 0xE1000220;
			InsertPrimAutoDepth(slot, (void *)pk);
			pk += 8;
			InsertPrimAutoDepth(slot, (void *)tri);
			U32((void *)pk, 0) = 0x1000000;
			U32((void *)pk, 4) = 0xE1000240;
			InsertPrimAutoDepth(slot, (void *)pk);
			pk += 8;
		}
		*(uint8_t **)(h + 0x20) = p;
		return pk;
	}

	// 0x68C010: textured quads (0x24 packets between the two draw-mode packets)
	static uint32_t RenderQuads(uint8_t *h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint8_t *list = *(uint8_t **)(h + 0x20);
		int32_t count = *(const int32_t *)list;
		uint8_t *p = list + 4;
		const uint32_t va = U32(h, 4);
		*(uint8_t **)(h + 0x20) = p;
		if (count <= 0)
		{
			*(uint8_t **)(h + 0x20) = p;
			return pk;
		}
		for (; count != 0; count--, p += 0x18)
		{
			GteLoadV012((const void *)(va + U16(p, 4) * 4), (const void *)(va + U16(p, 6) * 4), (const void *)(va + U16(p, 8) * 4));
			GteRTPT();
			U32((void *)pk, 0) = 0x8000000;
			U32((void *)pk, 4) = U32(p, 0);
			GteReadFLAG((uint32_t *)(h + 0x30));
			if (U32(h, 0x30) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0((int32_t *)(h + 0x24));
			if (W32(h, 0x24) < 0 && !(B8(h, 0x1C) & 0x20)) continue;
			GteReadSXY012((void *)(pk + 8), (void *)(pk + 0x10), (void *)(pk + 0x18));
			GteLoadV0((const void *)(va + U16(p, 0xA) * 4));
			GteRTPS();
			int16_t x = W16((void *)pk, 8);
			if (x < 0 || x > 0xA00) clip = 1;
			x = W16((void *)pk, 0x10);
			if (x < 0 || x > 0xA00) clip |= 2;
			x = W16((void *)pk, 0x18);
			if (x < 0 || x > 0xA00) clip |= 4;
			x = W16((void *)pk, 0xA);
			if (x < 0 || x > 0x6C0) clip |= 0x10;
			x = W16((void *)pk, 0x12);
			if (x < 0 || x > 0x6C0) clip |= 0x20;
			x = W16((void *)pk, 0x1A);
			if (x < 0 || x > 0x6C0) clip |= 0x40;
			GteReadSXY2((void *)(pk + 0x20));
			GteAVSZ4();
			x = W16((void *)pk, 0x20);
			if (x < 0 || x > 0xA00) clip |= 8;
			x = W16((void *)pk, 0x22);
			if (x < 0 || x > 0x6C0) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteReadOTZ32((int32_t *)(h + 0x2C));
			U32((void *)pk, 0xC) = U32(p, 0xC);
			U32((void *)pk, 0x14) = U32(p, 0x10);
			U32((void *)pk, 0x1C) = U32(p, 0x14);
			const int32_t z = W32(h, 0x2C) + W32(h, 0xC);
			W32(h, 0x2C) = z;
			if (z < 0) continue;
			const uint32_t slot = ot + (uint32_t)(z >> shift) * 4;
			const uint32_t quad = pk;
			pk += 0x24;
			U32((void *)pk, 0) = 0x1000000;
			U32((void *)pk, 4) = 0xE1000220;
			InsertPrimAutoDepth(slot, (void *)pk);
			pk += 8;
			InsertPrimAutoDepth(slot, (void *)quad);
			U32((void *)pk, 0) = 0x1000000;
			U32((void *)pk, 4) = 0xE1000240;
			InsertPrimAutoDepth(slot, (void *)pk);
			pk += 8;
		}
		*(uint8_t **)(h + 0x20) = p;
		return pk;
	}

	// 0x68BD50 (= Demi's 0x6D23D0): the model's triangle list then its quad list (each skipped when empty)
	static uint32_t RenderModelA(uint8_t *h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint8_t *model = *(uint8_t **)h;
		uint8_t *list = model + *(const uint32_t *)model + 0x10;
		*(uint8_t **)(h + 0x20) = list;
		if (*(const uint32_t *)list != 0) pk = RenderTris(h, ot, shift, pk);
		else *(uint8_t **)(h + 0x20) = list + 4;
		uint8_t *q = *(uint8_t **)(h + 0x20);
		if (*(const uint32_t *)q != 0) return RenderQuads(h, ot, shift, pk);
		*(uint8_t **)(h + 0x20) = q + 4;
		return pk;
	}

	// texture-window GPU word (E2) of the scrolling renderer from the header bytes a, b and words w, hh
	static uint32_t TexWindow(uint8_t a, uint8_t b, uint16_t w, uint16_t hh)
	{
		uint32_t d = ((uint32_t)a & 0xF8) | 0xFFFE2000u;
		d <<= 5;
		d |= (uint32_t)b & 0xF8;
		d <<= 5;
		d |= ~((uint32_t)w - 1) & 0xF8;
		d <<= 2;
		return (((uint32_t)(~((uint32_t)hh - 1)) >> 3) & 0x1F) | d;
	}

	// 0x68C350: textured triangles with a scrolling texture (v offset +0x12, wrapped by +0x22 inside the texture window)
	// and an optional depth-cue fade (+0x0C: the colour faded toward the far colour, a grey overlay 0x40 - fade / 64)
	static uint32_t RenderScroll(uint8_t *h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint8_t *list = *(uint8_t **)(h + 0x24);
		int32_t count = *(const int32_t *)list;
		uint8_t *p = list + 4;
		const uint32_t va = U32(h, 4);
		*(uint8_t **)(h + 0x24) = p;
		if (count <= 0)
		{
			*(uint8_t **)(h + 0x24) = p;
			return pk;
		}
		for (; count != 0; count--, p += 0x14)
		{
			GteLoadV012((const void *)(va + U16(p, 4) * 4), (const void *)(va + U16(p, 6) * 4), (const void *)(va + U16(p, 8) * 4));
			GteRTPT();
			U32((void *)pk, 0) = 0x7000000;
			U32((void *)pk, 4) = U32(p, 0);
			U32((void *)pk, 0xC) = U32(p, 0xC);
			U32((void *)pk, 0x14) = U32(p, 0x10);
			U32((void *)pk, 0x1C) = U32(p, 8) >> 16;
			GteReadFLAG((uint32_t *)(h + 0x34));
			if (U32(h, 0x34) & 0x60000) continue;
			GteNCLIP();
			uint32_t clip = 0;
			GteReadMAC0((int32_t *)(h + 0x28));
			if (W32(h, 0x28) < 0) continue;
			GteReadSXY012((void *)(pk + 8), (void *)(pk + 0x10), (void *)(pk + 0x18));
			GteAVSZ3();
			int16_t x = W16((void *)pk, 8);
			if (x < 0 || x > 0xA00) clip = 1;
			x = W16((void *)pk, 0x10);
			if (x < 0 || x > 0xA00) clip |= 2;
			x = W16((void *)pk, 0x18);
			if (x < 0 || x > 0xA00) clip |= 4;
			x = W16((void *)pk, 0xA);
			if (x < 0 || x > 0x6C0) clip |= 0x10;
			x = W16((void *)pk, 0x12);
			if (x < 0 || x > 0x6C0) clip |= 0x20;
			x = W16((void *)pk, 0x1A);
			if (x < 0 || x > 0x6C0) clip |= 0x40;
			if ((clip & 7) == 7 || (clip & 0x70) == 0x70) continue;
			GteReadOTZ32((int32_t *)(h + 0x30));
			if (W32(h, 0xC) != 0)
			{
				GteLoadRGBC((const void *)(pk + 4));
				GteSetIR0(W32(h, 0xC));
				GteDPCS();
				GteStoreRGB2((void *)(pk + 4));
				B8((void *)pk, 7) = 0x26;
			}
			const uint32_t off = U16(h, 0x12);
			uint32_t v2 = (uint32_t)B8((void *)pk, 0x1D) + off;
			const uint32_t v0 = (uint32_t)B8((void *)pk, 0xD) + off;
			const uint32_t v1 = (uint32_t)B8((void *)pk, 0x15) + off;
			if ((int32_t)(v2 | v1 | v0) > 0xFF)
			{
				B8((void *)pk, 0xD) = (uint8_t)((uint8_t)v0 - B8(h, 0x22));
				B8((void *)pk, 0x15) = (uint8_t)((uint8_t)v1 - B8(h, 0x22));
				v2 = (uint8_t)((uint8_t)v2 - B8(h, 0x22));
			}
			else
			{
				B8((void *)pk, 0xD) = (uint8_t)v0;
				B8((void *)pk, 0x15) = (uint8_t)v1;
			}
			B8((void *)pk, 0x1D) = (uint8_t)v2;
			const uint32_t slot = ot + (uint32_t)(W32(h, 0x30) >> shift) * 4;
			const uint32_t tri = pk;
			pk += 0x20;
			U32((void *)pk, 0) = 0x2000000;
			U32((void *)pk, 4) = TexWindow(B8(h, 0x16), B8(h, 0x14), U16(h, 0x1A), U16(h, 0x18));
			U32((void *)pk, 8) = 0;
			InsertPrimAutoDepth(slot, (void *)pk);
			InsertPrimAutoDepth(slot, (void *)tri);
			pk += 0xC;
			U32((void *)pk, 0) = 0x2000000;
			U32((void *)pk, 4) = TexWindow(B8(h, 0x1E), B8(h, 0x1C), U16(h, 0x22), U16(h, 0x20));
			U32((void *)pk, 8) = 0;
			InsertPrimAutoDepth(slot, (void *)pk);
			pk += 0xC;
			if (W32(h, 0xC) == 0) continue;
			U32((void *)pk, 0) = 0x4000000;
			B8((void *)pk, 7) = 0x22;
			U32((void *)pk, 8) = U32((void *)tri, 8);
			U32((void *)pk, 0xC) = U32((void *)tri, 0x10);
			U32((void *)pk, 0x10) = U32((void *)tri, 0x18);
			const uint8_t grey = (uint8_t)(0x40 - (uint8_t)(W32(h, 0xC) >> 6));
			B8((void *)pk, 6) = grey;
			B8((void *)pk, 5) = grey;
			B8((void *)pk, 4) = grey;
			InsertPrimAutoDepth(slot, (void *)pk);
			pk += 0x14;
			U32((void *)pk, 0) = 0x1000000;
			U32((void *)pk, 4) = 0xE1000240;
			InsertPrimAutoDepth(slot, (void *)pk);
			pk += 8;
		}
		*(uint8_t **)(h + 0x24) = p;
		return pk;
	}

	// 0x68C320: the scrolling renderer on the model's triangle list
	static uint32_t RenderModelB(uint8_t *h, uint32_t ot, int32_t shift, uint32_t pk)
	{
		uint8_t *model = *(uint8_t **)h;
		*(uint8_t **)(h + 0x24) = model + *(const uint32_t *)model + 8;
		return RenderScroll(h, ot, shift, pk);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x68A400): one object of a move layout
	// ------------------------------------------------------------------
	// the record's offset is loaded into the GTE V0 with its 4th word never written (the stack's residue: the port loads 0)
	static int16_t g_off_pad = 0;
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PlayArg *arg = (const PlayArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
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
		Mat4x3 m;
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		const int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], g_off_pad };
		if (r->flags & 0x1000)
		{
			// the offset through the orthonormal view copy, the rotation composed with it (0x8000: not)
			GteSetRotMatrixCtrl(CamCopy());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t t[3];
			GteReadMAC123(t);
			m.t[0] = t[0];
			m.t[1] = t[1];
			m.t[2] = t[2];
			if (!(r->flags & 0x8000)) MatrixMultiply(CamCopy(), &m);
		}
		else if (r->flags & 0x200)
		{
			// the offset in the frame's own axes
			m.t[0] = off[0];
			m.t[1] = off[1];
			m.t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			if (!(r->flags & 0x8000)) MatrixMultiply(&arg->m, &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				// 3x3 scale matrix (diagonal x, y, z)
				Mat4x3 s;
				s.m[0][0] = r->scale[0];
				s.m[1][1] = r->scale[1];
				s.m[2][2] = r->scale[2];
				s.m[1][2] = 0;
				s.m[0][2] = 0;
				s.m[2][1] = 0;
				s.m[0][1] = 0;
				s.m[2][0] = 0;
				s.m[1][0] = 0;
				MatrixMultiply3x3(&m, &s);
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
		if (r->flags & 0x2000) *(uint32_t *)(h + 0x1C) = 0x203C;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			B8(h, 0x1C) |= 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		uint32_t pk;
		if (arg->mode == 1)
		{
			pk = RenderModelA(h, OTBase() + 0x44, 2, PacketCursor());
		}
		else if (arg->mode == 2 && r->flags_lo == 0)
		{
			U16(h, 0x16) = 0;
			U16(h, 0x14) = 0;
			U16(h, 0x1A) = 0x100;
			U16(h, 0x18) = 0x100;
			U16(h, 0x1E) = 0;
			U16(h, 0x1C) = 0;
			U16(h, 0x20) = 0x100;
			U16(h, 0x22) = 0x80;
			W16(h, 0x12) = arg->scroll;
			W32(h, 0xC) = 0;
			pk = RenderModelB(h, OTBase() + 0x44, 2, PacketCursor());
		}
		else
		{
			*(int32_t *)(h + 0x18) = arg->depth;
			pk = RenderPrimModel070(h, OTBase() + 0x44, 2, PacketCursor());
		}
		PacketCursor() = pk;
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Sparks (0x68B140, pool 0x24: position +0x0C, frame +0x12, velocity +0x14, sequence +0x1A, acceleration +0x1C)
	// DRAW THEN UPDATE
	// ------------------------------------------------------------------

	static void SparkDraw(SparkNode *s)
	{
		// 30 fps layer: see mag274_duel_held.inc
		FX_HELD(held_note_spark(s);)
		const uint8_t *e = SPARK_Table + s->seq * 8;
		const int16_t scale = *(const int16_t *)(e + 6);
		// (the scale is passed zero-extended: the high half of the register holds the index's)
		const uint32_t a = ((uint32_t)(int32_t)s->seq & 0xFFFF0000u) | (uint16_t)scale;
		TransformCameraByShadowRotation(s->pos, (int32_t)a, -((int32_t)scale >> 2));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = *(const uint32_t *)e;
		*(int16_t *)(h + 4) = s->frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0xB4);
	}
	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		SparkDraw(s);
		s->frame++;
		if (s->frame >= *(const int16_t *)(SPARK_Table + s->seq * 8 + 4)) return TASK_END;
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->vel[0] = (int16_t)(s->acc[0] + s->vel[0]);
		s->vel[1] = (int16_t)(s->acc[1] + s->vel[1]);
		s->vel[2] = (int16_t)(s->acc[2] + s->vel[2]);
		return 0;
	}

	// 0x68AEE0: n sparks at Zell's effect bone, launch kind 0..4, spark table entry seq (kind 0..2 decelerate by 1/8 of
	// their velocity a tick; 3 falls (+0x14); 4 spreads on the ground and rises (-0x14))
	static void SparkBurst(int32_t bone, int32_t n, int32_t kind, int32_t seq)
	{
		int16_t pos[4];
		GetEffectSpawnPosition(Caster(), bone, 0, pos);
		pos[3] = 0;
		SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
		while (s)
		{
			memcpy(s->pos, pos, 8); // (+0x12 frame = pos[3] = 0)
			bool decel = true;
			switch (kind)
			{
			case 0:
				s->vel[0] = (int16_t)((Rand() & 0xFF) - 0x80);
				s->vel[1] = (int16_t)((Rand() & 0xFF) - 0x80);
				s->vel[2] = (int16_t)(-0x80 - (Rand() & 0x7F));
				break;
			case 1:
				s->vel[0] = (int16_t)((Rand() & 0xFF) - 0x80);
				s->vel[1] = (int16_t)(-0x80 - (Rand() & 0x7F));
				s->vel[2] = (int16_t)((Rand() & 0xFF) - 0x80);
				break;
			case 2:
			{
				const int32_t a = Rand();
				const int32_t r = (Rand() & 0xFF) + 0x100;
				s->vel[0] = (int16_t)(mul32(ComputeSin(a), r) >> 12);
				s->vel[1] = (int16_t)(mul32(ComputeCos(a), r) >> 12);
				s->vel[2] = (int16_t)((Rand() & 0x3F) - 0x20);
				break;
			}
			case 3:
			{
				const int32_t a = Rand();
				const int32_t r = (Rand() & 0x7F) + 0x80;
				s->vel[0] = (int16_t)(mul32(ComputeSin(a), r) >> 12);
				s->vel[2] = (int16_t)(mul32(ComputeCos(a), r) >> 12);
				const int32_t v = -0x80 - (Rand() & 0x7F);
				s->acc[2] = 0;
				s->vel[1] = (int16_t)v;
				s->acc[0] = 0;
				s->acc[1] = 0x14;
				decel = false;
				break;
			}
			case 4:
			{
				const int32_t a = Rand();
				const int32_t r = Rand() & 0x3FF;
				s->pos[0] = (int16_t)(s->pos[0] + (int16_t)(mul32(ComputeSin(a), r) >> 11));
				s->pos[2] = (int16_t)(s->pos[2] + (int16_t)(mul32(ComputeCos(a), r) >> 12));
				const int32_t up = Rand() & 0x7FF;
				s->vel[1] = 0;
				s->pos[1] = (int16_t)(s->pos[1] - up);
				s->vel[2] = 0;
				s->vel[0] = 0;
				s->acc[1] = -0x14;
				s->acc[2] = 0;
				s->acc[0] = 0;
				decel = false;
				break;
			}
			default:
				break;
			}
			if (decel)
			{
				s->acc[0] = (int16_t)(-(int32_t)s->vel[0] >> 3);
				s->acc[1] = (int16_t)(-(int32_t)s->vel[1] >> 3);
				s->acc[2] = (int16_t)(-(int32_t)s->vel[2] >> 3);
			}
			s->seq = (int16_t)seq;
			if (--n == 0) break;
			s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
		}
	}

	// 0x68B5E0: 2 sparks (table entry 2, no velocity) on a sphere of radius (size + 0x1000) / 16 around an entity's bone
	static void SparkRing(uint8_t *entity, int32_t bone)
	{
		int16_t pos[4];
		GetEffectSpawnPosition(entity, bone, 0, pos);
		int32_t n = 2;
		SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
		while (s)
		{
			const int32_t r = ((int32_t)*(const int16_t *)(entity + 0x26) + 0x1000) >> 4;
			const int32_t a = Rand();
			const int32_t b = Rand();
			const int32_t c = mul32(ComputeCos(a), r) >> 12;
			s->pos[1] = (int16_t)(pos[1] - (int16_t)(mul32(ComputeSin(a), r) >> 12));
			s->pos[0] = (int16_t)(pos[0] - (int16_t)(mul32(ComputeSin(b), c) >> 12));
			s->frame = 0;
			s->pos[2] = (int16_t)((mul32(ComputeCos(b), c) >> 12) + pos[2]);
			s->seq = 2;
			s->acc[2] = 0;
			s->acc[1] = 0;
			s->acc[0] = 0;
			s->vel[2] = 0;
			s->vel[1] = 0;
			s->vel[0] = 0;
			if (--n == 0) break;
			s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
		}
	}

	// ------------------------------------------------------------------
	// Hit flash (0x68B270, pool 0x14): sprite 0x10E7868, 8 frames, fixed place. DRAW THEN UPDATE
	// ------------------------------------------------------------------
	static void FlashDraw(FlashNode *f)
	{
		// 30 fps layer: see mag274_duel_held.inc
		FX_HELD(held_note_flash(f);)
		TransformCameraByShadowRotation(f->pos, 0x800, -0x200);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = SEQ_Flash;
		*(int16_t *)(h + 4) = f->frame;
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0xB4);
	}
	static uint32_t __cdecl FlashTask(TaskNode *n)
	{
		FlashNode *f = (FlashNode *)n;
		FlashDraw(f);
		f->frame++;
		return f->frame < 8 ? 0 : TASK_END;
	}

	// 0x68B210: a hit flash at Zell's effect bone (near: 0x80 closer to the camera side when `far` is 0)
	static void SpawnFlash(int32_t bone, int32_t far_)
	{
		int16_t pos[4];
		GetEffectSpawnPosition(Caster(), bone, 0, pos);
		pos[3] = 0;
		FlashNode *f = (FlashNode *)AddTaskToQueue(QFlashes(), ORIG_FlashTask);
		if (!f) return;
		memcpy(f->pos, pos, 8);
		if (far_ == 0) f->pos[2] = (int16_t)(f->pos[2] - 0x80);
	}

	// ------------------------------------------------------------------
	// Rocks (0x68D3B0, pool 0x2C): prim model 0x10E9360 spinning, falling (gravity 0xF, bounce on y = 0), 20 ticks.
	// DRAW THEN UPDATE
	// ------------------------------------------------------------------
	static void RockDraw(RockNode *k)
	{
		// 30 fps layer: see mag274_duel_held.inc
		FX_HELD(held_note_rock(k);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		Mat4x3 m;
		BuildRotationMatrixFromAngles(k->ang, &m);
		m.t[0] = k->pos[0];
		m.t[1] = k->pos[1];
		m.t[2] = k->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = MODEL_Rock;
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
	}
	static uint32_t __cdecl RockTask(TaskNode *n)
	{
		RockNode *k = (RockNode *)n;
		RockDraw(k);
		k->ang[0] = (int16_t)(k->ang[0] + k->spin[0]);
		k->ang[2] = (int16_t)(k->ang[2] + k->spin[2]);
		k->vel[1] = (int16_t)(k->vel[1] + 0xF);
		k->ang[1] = (int16_t)(k->ang[1] + k->spin[1]);
		k->pos[0] = (int16_t)(k->pos[0] + k->vel[0]);
		k->pos[1] = (int16_t)(k->pos[1] + k->vel[1]);
		k->pos[2] = (int16_t)(k->pos[2] + k->vel[2]);
		if (k->pos[1] > 0)
		{
			k->pos[1] = 0;
			k->vel[1] = (int16_t)-k->vel[1];
		}
		k->life++;
		return k->life < 0x14 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------
	// 0x68AE90: out x / z = from x / z minus dist along the angle (Zell's place in front of the target)
	static void PlaceBefore(int32_t angle, int32_t dist, const int16_t *from, int16_t *out)
	{
		out[0] = (int16_t)(from[0] - (int16_t)(mul32(ComputeSin(angle), dist) >> 12));
		out[2] = (int16_t)(from[2] - (int16_t)(mul32(ComputeCos(angle), dist) >> 12));
	}

	// 0x68A790: an entity's bone matrix (its skeleton block + 0x20 + bone * 0x30)
	static const Mat4x3 *BoneMatrix(uint8_t *entity, int32_t bone)
	{
		return (const Mat4x3 *)(**(uint8_t ***)(entity + 0x64) + bone * 0x30 + 0x20);
	}

	// 0x68BCD0: Mach Kick's wave: the backup vertices rotated about z by `roll` into the model, each z scaled by
	// grow / 1024 and its x bent by the table at z / 2 (x grow / 8192)
	static void BendWave(uint32_t model, const uint8_t *src, int32_t grow, int32_t roll)
	{
		Mat4x3 m;
		RotationZ(roll, &m);
		GteSetRotMatrix(&m);
		int32_t count = *(const int32_t *)(model + 4);
		int16_t *v = (int16_t *)(model + 8);
		do
		{
			GteLoadV0(src);
			GteMVMVA_RotV0();
			src += 8;
			GteStoreIR123s(v);
			const int32_t z = v[2];
			const int32_t bend = mul32(BEND_Table[z >> 1], grow) >> 13;
			v[0] = (int16_t)(v[0] - (int16_t)bend);
			v[2] = (int16_t)(mul32(z, grow) >> 10);
			v += 4;
		} while (--count != 0);
	}

	// 0x68D340: out = the battle view with out's translation (int32 x3 at +0x14) taken through it
	static void ViewAt(Mat4x3 *out)
	{
		TransformVectorBy3x3Matrix(&Camera(), out->t, out->t);
		out->t[0] = (int32_t)((uint32_t)out->t[0] + (uint32_t)Camera().t[0]);
		out->t[1] = (int32_t)((uint32_t)out->t[1] + (uint32_t)Camera().t[1]);
		out->t[2] = (int32_t)((uint32_t)out->t[2] + (uint32_t)Camera().t[2]);
		memcpy(out, &Camera(), 0x12);
	}

	// 0x68D1D0: an entity drawn by the module (Meteor Strike's target): its shadow, its bones' world matrices from its
	// world matrix x (rotation `angles`, translation `off` minus the rotated `off`, then `scale`) under the view,
	// RenderGeometry, its pose matrices rebuilt; the same for its weapon (+0x78)
	static void DrawEntity(uint8_t *e, const int16_t *angles, const int16_t *off, const int32_t *scale)
	{
		// 30 fps layer: see mag274_duel_held.inc
		FX_HELD(held_note_entity(e, angles, off, scale);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x5C);
		Mat4x3 *m = (Mat4x3 *)h;
		FramePackets() = DrawShadow(e, OTBase() + 0x4040, 0x10, FramePackets());
		ComposeZYXRotationMatrix(angles, m);
		m->t[0] = off[0];
		m->t[1] = off[1];
		m->t[2] = off[2];
		ComposeAffineTransform((const Mat4x3 *)(e + 0x40), m, m);
		Scale3DMatrix(m, scale);
		ComposeAffineTransform(&Camera(), m, m);
		Apply3DTransform(m, off, (int32_t *)(h + 0x20));
		m->t[1] = (int32_t)((uint32_t)m->t[1] - U32(h, 0x24));
		m->t[0] = (int32_t)((uint32_t)m->t[0] - U32(h, 0x20));
		m->t[2] = (int32_t)((uint32_t)m->t[2] - U32(h, 0x28));
		ComputeBonesWorldMatrices(e + 0x60, m);
		U32(h, 0x34) = var<uint32_t>(0x1D98B3C);
		U16(h, 0x44) = 0;
		U16(h, 0x46) = 0;
		U16(h, 0x48) = 0x140;
		U16(h, 0x4A) = 0xD8;
		U32(h, 0x4C) = U32(e, 0x28);
		U32(h, 0x58) = 0;
		U32(h, 0x50) = U32(e, 0x7C);
		U32(h, 0x40) = var<uint32_t>(0x1D969A8);
		FramePackets() = RenderGeometry(U32(e, 0x64), h + 0x30, OTBase() + 0x44, 4, FramePackets());
		BuildBoneMatricesFromPose(e + 0x60);
		uint8_t *w = *(uint8_t **)(e + 0x78);
		if (w)
		{
			U32(h, 0x50) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(w, m);
			FramePackets() = RenderGeometry(U32(w, 4), h + 0x30, OTBase() + 0x44, 4, FramePackets());
			BuildBoneMatricesFromPose(w);
		}
		FieldFree(0x5C);
	}

	// 0x68D4B0: the Duel pause's letterbox: two grey gouraud bars (0x404040 to black) sliding in from the top and the
	// bottom by sin(level * 16) * 80 / 4096 lines, between draw-mode packets, at OT bucket 8
	static void DrawBars(int32_t level)
	{
		const int32_t s = mul32(ComputeSin(shl32(level, 4)) * 5, 16) >> 12;
		const uint32_t slot = OTBase() + 0x20;
		uint8_t *pk = (uint8_t *)PacketCursor();
		PacketCursor() += 8;
		U32(pk, 0) = 0x1000000;
		U32(pk, 4) = 0xE1000220;
		InsertPrimAutoDepth(slot, pk);
		pk = (uint8_t *)PacketCursor();
		PacketCursor() += 0x24;
		W16(pk, 0xA) = (int16_t)(s * 8 - 0x280);
		W16(pk, 0x12) = (int16_t)(s * 8 - 0x280);
		U32(pk, 4) = 0x404040;
		W16(pk, 8) = 0;
		W16(pk, 0x10) = 0xA00;
		W16(pk, 0x18) = 0;
		W16(pk, 0x1A) = (int16_t)(s * 8);
		W16(pk, 0x20) = 0xA00;
		W16(pk, 0x22) = (int16_t)(s * 8);
		U32(pk, 0xC) = 0x404040;
		U32(pk, 0x1C) = 0;
		U32(pk, 0x14) = 0;
		U32(pk, 0) = 0x8000000;
		B8(pk, 7) = 0x3A;
		InsertPrimAutoDepth(slot, pk);
		pk = (uint8_t *)PacketCursor();
		PacketCursor() += 0x24;
		W16(pk, 0xA) = (int16_t)((0xD8 - s) * 8);
		W16(pk, 0x12) = (int16_t)((0xD8 - s) * 8);
		U32(pk, 4) = 0;
		W16(pk, 8) = 0;
		W16(pk, 0x10) = 0xA00;
		W16(pk, 0x18) = 0;
		W16(pk, 0x1A) = (int16_t)((0x128 - s) * 8);
		W16(pk, 0x20) = 0xA00;
		W16(pk, 0x22) = (int16_t)((0x128 - s) * 8);
		U32(pk, 0xC) = 0;
		U32(pk, 0x1C) = 0x404040;
		U32(pk, 0x14) = 0x404040;
		U32(pk, 0) = 0x8000000;
		B8(pk, 7) = 0x3A;
		InsertPrimAutoDepth(slot, pk);
		pk = (uint8_t *)PacketCursor();
		PacketCursor() += 8;
		U32(pk, 0) = 0x1000000;
		U32(pk, 4) = 0xE1000240;
		InsertPrimAutoDepth(slot, pk);
	}

	// 0x68D600 / 0x68D620: bit 8 of every shown entity's flags set / cleared (the Duel pause)
	static void PauseEntities()
	{
		for (uint8_t *e = (uint8_t *)0x1D972C0; e < (uint8_t *)0x1D97704; e += 0x9C)
			if (e[0] & 2) U16(e, 0) |= 8;
	}
	static void ResumeEntities()
	{
		for (uint8_t *e = (uint8_t *)0x1D972C0; e < (uint8_t *)0x1D97704; e += 0x9C)
			if (e[0] & 2) U16(e, 0) &= 0xFFF7;
	}

	// the target chain's flag bits `mask` back to the setup's copy (entity slots by pointer)
	static void RestoreChainFlags(uint8_t *target, uint16_t mask)
	{
		uint8_t *e = target;
		do
		{
			const uint16_t f = U16(e, 0);
			if (f & 2) U16(e, 0) = (uint16_t)(f ^ ((f ^ (uint16_t)SavedFlags()[SlotOf(e)]) & mask));
			e = *(uint8_t **)(e + 0x8C);
		} while (e != target && e != nullptr);
	}

	// ------------------------------------------------------------------
	// The moves (called by MoveTask on counters 0..length with the move node)
	// ------------------------------------------------------------------
	// the move layouts (the site = which layout of which move; its frame matrix arg->m is built by the move WITHOUT the view)
	enum : int { SITE_PUNCH_B = 1, SITE_PUNCH_A, SITE_BOOYA, SITE_HEEL_A, SITE_HEEL_B, SITE_KICK, SITE_DOLPHIN_A, SITE_DOLPHIN_B, SITE_METEOR_A, SITE_METEOR_B };
	// a move layout: its frame under the battle view (Meteor Strike's wave layout: the view with the frame's translation taken
	// through it, 0x68D340), through the shared player with the module callback
	// the bone-anchored layout frames (without the view): Punch Rush's two at his fist (bone 0x16; the second turned by his
	// bone 0x12 matrix through the slot 0 entity's world matrix, whoever casts), Meteor Strike's wave at his hip (bone 0x13,
	// on the ground); pos = the spawn position read
	static void BoneFrame(int site, Mat4x3 *f, int16_t *pos)
	{
		uint8_t *c = Caster();
		if (site == SITE_METEOR_A)
		{
			GetEffectSpawnPosition(c, 0x13, 0, pos);
			f->t[0] = pos[0];
			f->t[1] = 0;
			f->t[2] = pos[2];
			return;
		}
		GetEffectSpawnPosition(c, 0x16, 0, pos);
		if (site == SITE_PUNCH_B)
		{
			MultiplyPSX((const void *)0x1D97300, BoneMatrix(c, 0x12), f);
			f->t[0] = pos[0];
			f->t[1] = pos[1];
			f->t[2] = pos[2];
		}
		else
		{
			RotationY(-0x400, f);
			f->t[0] = pos[0];
			f->t[1] = pos[1];
			f->t[2] = pos[2] - 0xC8;
		}
	}
	static int PlayLayout(MoveNode *m, int site, uint32_t layout, PlayArg *arg)
	{
		(void)m;
		// 30 fps layer: see mag274_duel_held.inc
		FX_HELD(held_note_layout(m, site, layout, arg);)
		if (site == SITE_METEOR_A) ViewAt(&arg->m);
		else ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
		return prim::play((prim::Layout *)layout, PartCallback, (int)arg, 0);
	}
	// the 4th words of two spawn-position locals that are copied into the move node +0x66 (Booya: entry esp - 0x2E, Meteor
	// Strike: - 0x4E; never written by the move: the stack's residue, never read). The tick-start residue is modelled as 0; on
	// the first move's start tick MoveTask's GetDefaultEffectPosition leaves, through GetEffectSpawnPosition's push of the
	// view address 0x1D97708 (its model path), the word 0x01D9 at Meteor Strike's slot (see MoveTask)
	static int16_t g_pad_booya = 0, g_pad_meteor = 0;
	// the action's damage
	static void Damage(const MoveNode *m)
	{
		ApplyActionResultToTarget(ActionTargets(m->action));
	}
	// Zell in front of the target (counter 0 of a move)
	static void PlaceCaster(MoveNode *m, int32_t dist)
	{
		uint8_t *c = Caster();
		PlaceBefore((int32_t)W16(c, 0xE) + 0x800, dist, m->anchor, (int16_t *)(c + 0x1C));
	}
	static int32_t Radius(uint8_t *e) { return W16(e, 0x26); }

	// 0x68A050 Punch Rush (45 ticks): punches at his fists, the target bouncing back on each (8 / 13 / 22), the last one
	// (32 damage, 33 fade + burst) pushes it back
	static void MovePunchRush(MoveNode *m)
	{
		uint8_t *c = Caster();
		if (m->counter == 0)
		{
			const int32_t r = Radius(c);
			PlaceCaster(m, (r >> 1) + (r + (Radius(m->target) >> 1)));
			InitTaskQueuePool(QSparks(), (void *)Alloc(), 0x24, 0x20);
			Alloc() += 0x480;
			InitTaskQueuePool(QFlashes(), (void *)Alloc(), 0x14, 2);
			Alloc() += 0x28;
			m->layout_a = Alloc();
			Alloc() += 0x14C;
			m->layout_b = Alloc();
			Alloc() += 0x64;
			m->ghosts = Alloc();
			Alloc() += 0x488;
			DecodeModelPrimLayout(LAYOUT_Punch, m->layout_a, 0x14C);
			DecodeModelPrimLayout(LAYOUT_PunchB, m->layout_b, 0x64);
			static const int32_t bones[6] = { 0xD, 0x11, 0x15, 0xE, 0x12, 0x16 };
			GhostInit((uint8_t *)m->ghosts, 6, bones);
		}
		W16(m->target, 0xE) = (int16_t)(W16(c, 0xE) + 0x800);
		ExecuteTaskQueue(QSparks());
		ExecuteTaskQueue(QFlashes());
		PlayArg arg;
		if ((uint32_t)(m->counter - 0x1B) < 0xF)
		{
			int16_t pos[4];
			BoneFrame(SITE_PUNCH_B, &arg.m, pos);
			arg.morph = TexBase() + 0x74;
			arg.depth = -0x80;
			arg.mode = 0;
			PlayLayout(m, SITE_PUNCH_B, m->layout_b, &arg);
		}
		if ((uint32_t)(m->counter - 0x21) < 0xE)
		{
			int16_t pos[4];
			BoneFrame(SITE_PUNCH_A, &arg.m, pos);
			arg.morph = TexBase() + 0x74;
			arg.depth = -0x100;
			arg.mode = 0;
			PlayLayout(m, SITE_PUNCH_A, m->layout_a, &arg);
		}
		GhostDraw(c, (uint8_t *)m->ghosts);
		if (m->counter <= 0x1A) GhostRecord(c, (uint8_t *)m->ghosts);
		switch (m->counter)
		{
		case 8:
			SparkBurst(0x15, 0x10, 0, 0);
			SpawnFlash(0x15, 0);
			W16(m->target, 0x20) -= 0x32;
			break;
		case 9:
		case 0xE:
		case 0x17:
			W16(m->target, 0x20) += 0x32;
			break;
		case 0xD:
			SparkBurst(0x16, 0x10, 0, 0);
			SpawnFlash(0x16, 0);
			W16(m->target, 0x20) -= 0x32;
			break;
		case 0x16:
			SparkBurst(0x15, 0x10, 1, 0);
			SpawnFlash(0x15, 1);
			W16(m->target, 0x20) -= 0x32;
			break;
		case 0x20:
		case 0x21:
			if (m->counter == 0x20) Damage(m);
			if (m->counter == 0x21)
			{
				ScreenFadeTask(0, 1, 1, 0x80);
				SparkBurst(0x16, 0x20, 0, 0);
			}
			W16(m->target, 0x20) -= 0xC8;
			break;
		default:
			break;
		}
		if (m->counter == 8) BdPlaySE(SOUND_Punch, 0, 0x80);
	}

	// 0x68B2F0 Booya (45 ticks): the target follows his fist (bone 0x16) on 10..31, put back at 32 (damage); a layout at
	// his elbow (bone 9) on 31..44; bursts at 31 and 35..38
	static void MoveBooya(MoveNode *m)
	{
		uint8_t *c = Caster();
		if (m->counter == 0)
		{
			PlaceCaster(m, (Radius(m->target) >> 1) + (Radius(c) >> 1) + 0x80);
			InitTaskQueuePool(QSparks(), (void *)Alloc(), 0x24, 0x40);
			Alloc() += 0x900;
			m->layout_a = Alloc();
			Alloc() += 0x5C8;
			m->ghosts = Alloc();
			Alloc() += 0x488;
			DecodeModelPrimLayout(LAYOUT_Booya, m->layout_a, 0x5C8);
			static const int32_t bones[1] = { 9 };
			GhostInit((uint8_t *)m->ghosts, 1, bones);
		}
		GhostDraw(c, (uint8_t *)m->ghosts);
		ExecuteTaskQueue(QSparks());
		W16(m->target, 0xE) = (int16_t)(W16(c, 0xE) + 0x800);
		if (m->counter >= 0x1F && m->counter < 0x2D)
		{
			if (m->counter == 0x1F) GetEffectSpawnPosition(c, 9, 0, m->p54);
			PlayArg arg;
			RotationY(-0x400, &arg.m);
			arg.m.t[0] = m->p54[0];
			arg.m.t[1] = m->p54[1];
			arg.m.t[2] = m->p54[2];
			arg.morph = TexBase() + 0x74;
			arg.depth = 0;
			arg.mode = 0;
			PlayLayout(m, SITE_BOOYA, m->layout_a, &arg);
		}
		int16_t *fist = (int16_t *)m->w60; // +0x60 his fist's last position
		if (m->counter == 9) GetEffectSpawnPosition(c, 0x16, 0, fist);
		if (m->counter >= 0xA && m->counter < 0x20)
		{
			int16_t pos[4];
			GetEffectSpawnPosition(c, 0x16, 0, pos);
			pos[3] = g_pad_booya; // (never written: the stack's residue, copied into the node)
			W16(m->target, 0x1C) += (int16_t)(pos[0] - fist[0]);
			W16(m->target, 0x1E) += (int16_t)(pos[1] - fist[1]);
			W16(m->target, 0x20) += (int16_t)(pos[2] - fist[2]);
			memcpy(fist, pos, 8);
		}
		if (m->counter >= 0x19 && m->counter <= 0x20) GhostRecord(c, (uint8_t *)m->ghosts);
		if (m->counter == 0x1F) SparkBurst(0x16, 0x20, 2, 1);
		if (m->counter == 0x20)
		{
			U32(m->target, 0x1C) = m->target_pos[0];
			U32(m->target, 0x20) = m->target_pos[1];
			Damage(m);
		}
		if (m->counter == 0x21) ScreenFadeTask(0, 1, 1, 0x80);
		if (m->counter >= 0x23 && m->counter <= 0x26)
		{
			SparkRing(c, 9);
			SparkRing(m->target, 0xF0);
		}
		if (m->counter == 0xD) BdPlaySE(SOUND_Booya, 0, 0x80);
	}

	// 0x68B700 Heel Drop (42 ticks): two layouts at his foot (bone 0x14, taken at 32) on 32..45 (the second one on the
	// ground), damage at 32
	static void MoveHeelDrop(MoveNode *m)
	{
		uint8_t *c = Caster();
		EffectCameraMatrix(&Camera(), CamCopy());
		if (m->counter == 0)
		{
			PlaceCaster(m, (Radius(m->target) >> 1) + Radius(c) + 0x100);
			m->layout_a = Alloc();
			Alloc() += 0x360;
			m->layout_b = Alloc();
			Alloc() += 0x64;
			m->ghosts = Alloc();
			Alloc() += 0x488;
			DecodeModelPrimLayout(LAYOUT_Heel, m->layout_a, 0x360);
			DecodeModelPrimLayout(LAYOUT_HeelB, m->layout_b, 0x64);
			static const int32_t bones[3] = { 0xC, 0x10, 0x14 };
			GhostInit((uint8_t *)m->ghosts, 3, bones);
		}
		GhostDraw(c, (uint8_t *)m->ghosts);
		if (m->counter >= 0x20 && m->counter < 0x2E)
		{
			if (m->counter == 0x20) GetEffectSpawnPosition(c, 0x14, 0, m->p54);
			PlayArg arg;
			RotationY(0, &arg.m);
			arg.m.t[0] = m->p54[0];
			arg.m.t[1] = m->p54[1];
			arg.m.t[2] = m->p54[2];
			arg.morph = TexBase() + 0x74;
			arg.depth = -0x100;
			arg.mode = 0;
			PlayLayout(m, SITE_HEEL_A, m->layout_a, &arg);
			RotationY(0, &arg.m);
			arg.m.t[0] = m->p54[0];
			arg.m.t[1] = 0;
			arg.m.t[2] = m->p54[2];
			PlayLayout(m, SITE_HEEL_B, m->layout_b, &arg);
		}
		if (m->counter >= 0x19 && m->counter <= 0x20) GhostRecord(c, (uint8_t *)m->ghosts);
		if (m->counter == 0x20) Damage(m);
		if (m->counter == 0x21) ScreenFadeTask(0, 1, 1, 0x80);
		if (m->counter == 0x1E) BdPlaySE(SOUND_Heel, 0, 0x80);
	}

	// 0x68B950 Mach Kick (42 ticks): the wave model (0x10E9530) backed up at 0, bent on 28..31 (grow / roll by the
	// tick), drawn on 29..36 along his foot's motion (bone 0x14: taken at 28 and on 29..32), put back at 37; a layout at
	// his foot on 32..45; damage at 32
	static void MoveMachKick(MoveNode *m)
	{
		uint8_t *c = Caster();
		uint32_t &backup = U32(m->w60, 0);           // +0x60
		int16_t *from = (int16_t *)(m->w60 + 4);     // +0x64 the foot at 28
		int16_t *to = (int16_t *)(m->w60 + 0xC);     // +0x6C the foot on 29..32
		if (m->counter == 0)
		{
			const int32_t r = Radius(c);
			PlaceCaster(m, (r >> 1) + (r + (Radius(m->target) >> 1)));
			backup = Alloc();
			Alloc() += 0x268;
			m->layout_a = Alloc();
			Alloc() += 0x23C;
			m->ghosts = Alloc();
			Alloc() += 0x488;
			CopyDwords(backup, MODEL_Kick + 8, 0x9A);
			DecodeModelPrimLayout(LAYOUT_Kick, m->layout_a, 0x23C);
			static const int32_t bones[3] = { 0xC, 0x10, 0x14 };
			GhostInit((uint8_t *)m->ghosts, 3, bones);
		}
		GhostDraw(c, (uint8_t *)m->ghosts);
		if (m->counter >= 0x20 && m->counter < 0x2E)
		{
			if (m->counter == 0x20) GetEffectSpawnPosition(c, 0x14, 0, m->p54);
			PlayArg arg;
			RotationY(0, &arg.m);
			arg.m.t[0] = m->p54[0];
			arg.m.t[1] = m->p54[1];
			arg.m.t[2] = m->p54[2];
			arg.morph = TexBase() + 0x74;
			arg.depth = -0x100;
			arg.mode = 0;
			PlayLayout(m, SITE_KICK, m->layout_a, &arg);
		}
		const uint32_t v = (uint32_t)(m->counter - 0x1D);
		if (v < 8)
		{
			if (v < 4) GetEffectSpawnPosition(c, 0x14, 0, to);
			int32_t d[3] = { to[0] - from[0], to[1] - from[1], from[2] - to[2] };
			Mat4x3 b;
			BuildOrthonormalBasis(d, &b);
			d[1] = 0x200;
			d[0] = 0x200;
			d[2] = 0x200;
			Scale3DMatrix(&b, d);
			b.t[0] = to[0];
			b.t[1] = to[1];
			b.t[2] = to[2];
			// 30 fps layer: see mag274_duel_held.inc
			FX_HELD(held_note_wave(m, v, &b);)
			ComposeAffineTransform(&Camera(), &b, &b);
			GteSetRotMatrix(&b);
			GteSetTransVector(&b);
			uint8_t *h = (uint8_t *)FieldAlloc(0x58);
			*(uint32_t *)h = MODEL_Kick;
			if (v >= 4)
			{
				*(uint32_t *)(h + 0x1C) = 0xF0;
				*(uint32_t *)(h + 8) = 0;
				*(uint32_t *)(h + 0xC) = (v - 4) << 10;
			}
			else
			{
				*(uint32_t *)(h + 0x1C) = 0x30;
			}
			PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
			FieldFree(0x58);
		}
		const uint32_t w = (uint32_t)(m->counter - 0x1C);
		if (w < 4)
		{
			if (m->counter == 0x1C) GetEffectSpawnPosition(c, 0x14, 0, from);
			BendWave(MODEL_Kick, (const uint8_t *)backup, (int32_t)(w << 10) + 0x100, (int32_t)(w << 10));
		}
		if (m->counter == 0x25) CopyDwords(MODEL_Kick + 8, backup, 0x9A);
		if (m->counter >= 0x19 && m->counter <= 0x20) GhostRecord(c, (uint8_t *)m->ghosts);
		if (m->counter == 0x20)
		{
			Damage(m);
			if (m->counter == 0x20) ScreenFadeTask(0, 1, 1, 0x80);
		}
		if (m->counter == 0x1B) BdPlaySE(SOUND_Kick, 0, 0x80);
	}

	// 0x68C7B0 Dolphin Blow (49 ticks): a layout under him on 0..15 (renderer 0x68BD50), the water spout layout on 20..69
	// (texture scroll 8 a tick, renderer 0x68C320), sparks at 30 / 32..39, damage at 33 (the target pushed down on 32 / 33)
	static void MoveDolphinBlow(MoveNode *m)
	{
		uint8_t *c = Caster();
		EffectCameraMatrix(&Camera(), CamCopy());
		if (m->counter == 0)
		{
			const int32_t r = Radius(c);
			PlaceCaster(m, (r >> 1) + (r + (Radius(m->target) >> 1)));
			memcpy(m->p54, c + 0x1C, 8);
			m->layout_a = Alloc();
			Alloc() += 0xE0;
			m->layout_b = Alloc();
			Alloc() += 0x388;
			InitTaskQueuePool(QSparks(), (void *)Alloc(), 0x24, 0x40);
			Alloc() += 0x900;
			DecodeModelPrimLayout(LAYOUT_Dolphin, m->layout_a, 0xE0);
			DecodeModelPrimLayout(LAYOUT_DolphinB, m->layout_b, 0x388);
		}
		if (m->counter < 0x10)
		{
			PlayArg arg;
			RotationY(0x800, &arg.m);
			arg.m.t[0] = m->p54[0];
			arg.m.t[1] = m->p54[1];
			arg.m.t[2] = m->p54[2] - 0x1F4;
			arg.morph = TexBase() + 0x74;
			arg.depth = -0x100;
			arg.mode = 1;
			PlayLayout(m, SITE_DOLPHIN_A, m->layout_a, &arg);
		}
		const uint32_t v = (uint32_t)(m->counter - 0x14);
		if (v < 0x32)
		{
			PlayArg arg;
			RotationY(0x800, &arg.m);
			arg.m.t[0] = m->p54[0];
			arg.m.t[1] = m->p54[1];
			arg.m.t[2] = m->p54[2] - 0x3E8;
			arg.morph = TexBase() + 0x74;
			arg.depth = 0;
			arg.mode = 2;
			arg.scroll = (int16_t)((v & 0xF) << 3);
			PlayLayout(m, SITE_DOLPHIN_B, m->layout_b, &arg);
		}
		ExecuteTaskQueue(QSparks());
		if (m->counter == 0x1E) SparkBurst(0xF1, 0x10, 3, 3);
		if (m->counter >= 0x20 && m->counter < 0x28) SparkBurst(0xF1, 8, 4, 4);
		if (m->counter == 0x1F) ScreenFadeTask(0, 1, 0, 0x80);
		if (m->counter >= 0x20 && m->counter <= 0x21)
		{
			if (m->counter == 0x21) Damage(m);
			W16(m->target, 0x20) -= 0xC8;
		}
		if (m->counter == 0x18) BdPlaySE(SOUND_Dolphin, 0, 0x80);
	}

	// Meteor Strike's rising wave (0x10E7E18): turned by w (RotationY by the counter), stretched by -2 * his hip's height,
	// at his hip on the ground; fading in by the counter from 44
	static void DrawPillar(Mat4x3 *w, const int16_t *pos, int16_t counter)
	{
		w->t[1] = 0;
		w->m[1][1] = (int16_t)(-2 * pos[1]);
		w->t[0] = pos[0];
		w->t[2] = pos[2];
		// 30 fps layer: see mag274_duel_held.inc
		FX_HELD(held_note_pillar(w, pos, counter);)
		ComposeAffineTransform(&Camera(), w, w);
		GteSetRotMatrix(w);
		GteSetTransVector(w);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = MODEL_Wave;
		if (counter < 0x2C)
		{
			*(uint32_t *)(h + 0x1C) = 0x30;
		}
		else
		{
			*(uint32_t *)(h + 8) = 0;
			*(uint32_t *)(h + 0x1C) = 0xF0;
			*(uint32_t *)(h + 0xC) = (uint32_t)(counter - 0x2C) << 9;
		}
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
	}

	// 0x68CA60 Meteor Strike (80 ticks): the target thrown along 0x110E3F0 on 0..59 (position, world translation, flags +1
	// bit 0x10), Zell jumps to it at 28 and rises on 58..67; the rising wave (0x10E7E18) and its layout at his hip (bone
	// 0x13) on 44..61 with rocks on 44..49; the impact layout at the target on 60..75 with rocks on 60..63; from 55 the
	// target is drawn by the module, tilted and squashed (0x68D1D0, flags 0x1004); damage at 70
	static void MoveMeteorStrike(MoveNode *m)
	{
		uint8_t *c = Caster();
		uint8_t *t = m->target;
		int16_t *hip = (int16_t *)m->w60;           // +0x60 his hip (bone 0x15)
		int32_t &tilt = W32(m->w60, 8);              // +0x68
		if (m->counter == 0)
		{
			U32(t, 0x1C) = U32(m->anchor, 0);
			U32(t, 0x20) = U32(m->anchor, 4);
			PlaceCaster(m, Radius(t) >> 1);
			GetEffectSpawnPosition(c, 0x15, 0, hip);
			MeasureModelHeight(t, m->height);
			tilt = 0;
			m->layout_a = Alloc();
			Alloc() += 0x128;
			m->layout_b = Alloc();
			Alloc() += 0x9E4;
			InitTaskQueuePool(QRocks(), (void *)Alloc(), 0x2C, 0x20);
			Alloc() += 0x580;
			DecodeModelPrimLayout(LAYOUT_Meteor, m->layout_a, 0x128);
			DecodeModelPrimLayout(LAYOUT_MeteorB, m->layout_b, 0x9E4);
		}
		int16_t pos[4];       // (the wave's place, then the module draw's offset)
		int16_t ang[4];       // (the hip, then the module draw's angles)
		if (m->counter >= 0x2C && m->counter < 0x3E)
		{
			PlayArg arg;
			BoneFrame(SITE_METEOR_A, &arg.m, pos);
			arg.morph = TexBase() + 0x74;
			arg.depth = 0;
			arg.mode = 0;
			const int left = PlayLayout(m, SITE_METEOR_A, m->layout_a, &arg);
			if (m->counter < 0x35)
			{
				Mat4x3 w;
				// (the angle register's high half is the player's return value's)
				const int32_t angle = (int32_t)shl32((int32_t)(((uint32_t)left & 0xFFFF0000u) | (uint16_t)m->counter), 7);
				// 30 fps layer: see mag274_duel_held.inc
				FX_HELD(held_pillar_angle(angle);)
				RotationY(angle, &w);
				GetEffectSpawnPosition(c, 0x13, 0, pos);
				DrawPillar(&w, pos, m->counter);
				if (m->counter < 0x32)
				{
					int32_t n = 2;
					RockNode *k = (RockNode *)AddTaskToQueue(QRocks(), ORIG_RockTask);
					while (k)
					{
						memcpy(k->pos, pos, 8);
						k->life = 0;
						k->pos[0] = (int16_t)(k->pos[0] + (Rand() & 0x1FF) - 0x100);
						k->pos[2] = (int16_t)(k->pos[2] + (Rand() & 0x1FF) - 0x100);
						k->pos[1] = (int16_t)(k->pos[1] + (Rand() & 0xFF));
						k->vel[0] = (int16_t)((Rand() & 0x7F) - 0x40);
						k->vel[2] = (int16_t)((Rand() & 0x7F) - 0x40);
						k->vel[1] = (int16_t)(Rand() & 0x1F);
						k->ang[0] = (int16_t)Rand();
						k->ang[1] = (int16_t)Rand();
						k->ang[2] = (int16_t)Rand();
						k->spin[0] = (int16_t)((Rand() & 0xFF) - 0x80);
						k->spin[1] = (int16_t)((Rand() & 0xFF) - 0x80);
						k->spin[2] = (int16_t)((Rand() & 0xFF) - 0x80);
						if (--n == 0) break;
						k = (RockNode *)AddTaskToQueue(QRocks(), ORIG_RockTask);
					}
				}
			}
		}
		const uint32_t u = (uint32_t)(m->counter - 0x3C);
		if (u < 0x10)
		{
			PlayArg arg;
			const int16_t s = (int16_t)(((int32_t)W16(t, 0x26) + 0x1000) >> 1);
			arg.m.m[2][2] = s;
			arg.m.m[2][1] = 0;
			arg.m.m[1][1] = s;
			arg.m.m[0][0] = s;
			arg.m.m[2][0] = 0;
			arg.m.m[1][2] = 0;
			arg.m.m[1][0] = 0;
			arg.m.m[0][2] = 0;
			arg.m.m[0][1] = 0;
			arg.m.t[0] = W16(t, 0x1C);
			arg.m.t[1] = 0;
			arg.m.t[2] = W16(t, 0x20);
			arg.morph = TexBase() + 0x74;
			arg.depth = ((int32_t)W16(t, 0x26) - 0x800) >> 3;
			arg.mode = 0;
			PlayLayout(m, SITE_METEOR_B, m->layout_b, &arg);
			if (u < 4)
			{
				int32_t n = 4;
				RockNode *k = (RockNode *)AddTaskToQueue(QRocks(), ORIG_RockTask);
				while (k)
				{
					U32(k->pos, 0) = U32(t, 0x1C);
					U32(k->pos, 4) = U32(t, 0x20);
					k->life = 0;
					k->pos[1] = 0;
					k->vel[0] = (int16_t)((Rand() & 0x1FF) - 0x100);
					k->vel[2] = (int16_t)((Rand() & 0x1FF) - 0x100);
					k->vel[1] = (int16_t)(-0x40 - (Rand() & 0x7F));
					k->ang[0] = (int16_t)Rand();
					k->ang[1] = (int16_t)Rand();
					k->ang[2] = (int16_t)Rand();
					k->spin[0] = (int16_t)((Rand() & 0xFF) - 0x80);
					k->spin[1] = (int16_t)((Rand() & 0xFF) - 0x80);
					k->spin[2] = (int16_t)((Rand() & 0xFF) - 0x80);
					if (--n == 0) break;
					k = (RockNode *)AddTaskToQueue(QRocks(), ORIG_RockTask);
				}
			}
		}
		if ((uint16_t)m->counter <= 0x3B)
		{
			GetEffectSpawnPosition(c, 0x15, 0, ang);
			ang[3] = g_pad_meteor; // (never written: the stack's residue, copied into the node)
			const int16_t *path = THROW_Table + m->counter * 4;
			int16_t x = (int16_t)(m->anchor[0] + path[0]);
			W16(t, 0x1C) = x;
			W32(t, 0x54) = x;
			x = (int16_t)((int16_t)(path[1] - m->height[0]) + m->anchor[1]);
			W16(t, 0x1E) = x;
			W32(t, 0x58) = x;
			x = (int16_t)(path[2] + m->anchor[2]);
			W16(t, 0x20) = x;
			W32(t, 0x5C) = x;
			memcpy(hip, ang, 8);
			B8(t, 1) |= 0x10;
			if (m->counter == 0x1C)
			{
				GetDefaultEffectPosition(t, pos);
				hip[0] = (int16_t)(hip[0] + (int16_t)(pos[0] - W16(c, 0x1C)));
				hip[2] = (int16_t)(hip[2] + (int16_t)(pos[2] - W16(c, 0x20)));
				W16(c, 0x1C) = pos[0];
				W16(c, 0x20) = pos[2];
			}
		}
		if (m->counter >= 0x3A && m->counter <= 0x43) W16(c, 0x20) += (int16_t)shl32(m->counter - 0x39, 7);
		if (m->counter == 0x3D) ScreenFadeTask(0, 1, 1, 0x80);
		if (m->counter == 0x46) Damage(m);
		const uint32_t k = (uint32_t)(m->counter - 0x37);
		if (k < 0x100)
		{
			pos[2] = 0;
			pos[0] = 0;
			if (k < 4)
			{
				pos[1] = (int16_t)((uint32_t)mul32((int32_t)m->height[1] - (int32_t)m->height[0], (int32_t)k) >> 3) + m->height[0];
				tilt -= 0x200;
				if (tilt < -0x800) tilt = -0x800;
			}
			else
			{
				MeasureModelHeight(t, m->height);
				pos[1] = (int16_t)(((int32_t)m->height[1] + (int32_t)m->height[0]) >> 1);
			}
			int32_t scale[3];
			scale[2] = 0x1000;
			scale[0] = 0x1000;
			if (k >= 4 && k < 0xC)
			{
				const int32_t s = 0x1000 - (ComputeSin((int32_t)((k - 4) << 8)) >> 1);
				scale[1] = s;
				pos[1] = (int16_t)(mul32(pos[1], s) >> 12);
			}
			else
			{
				scale[1] = 0x1000;
			}
			ang[2] = 0;
			ang[0] = (int16_t)tilt;
			ang[1] = 0;
			pos[2] = 0;
			pos[0] = 0;
			uint8_t *e = t;
			do
			{
				if (e[0] & 2)
				{
					DrawEntity(e, ang, pos, scale);
					U16(e, 0) |= 0x1004;
				}
				e = *(uint8_t **)(e + 0x8C);
			} while (e != t && e != nullptr);
		}
		ExecuteTaskQueue(QRocks());
		if (m->counter == 0x2B) BdPlaySE(SOUND_Meteor, 0, 0x80);
	}

	// ------------------------------------------------------------------
	// MoveTask (0x689AC0, pool 1 x 0x74)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MoveTask(TaskNode *n)
	{
		MoveNode *m = (MoveNode *)n;
		g_pad_meteor = 0;
		m->mode = PostedMode();
		RestoreChainFlags(m->target, 4);
		if (m->mode == 0)
		{
			if (m->counter == 0) Ctx()->done = 1;
			m->counter = 1;
			return 0;
		}
		if (m->wait != 0)
		{
			bool bars = true;
			if (m->wait < 4)
			{
				bars = false;
			}
			else if (NewMove())
			{
				// the next move: the bars slide out first
				m->level = (uint8_t)(m->level - 0x10);
				if (m->level > 0x80)
				{
					ResumeEntities();
					SetScreenFlash(0, 0);
					m->level = 0;
					m->wait = 0;
					goto start;
				}
			}
			else
			{
				PauseEntities();
				m->level = (uint8_t)(m->level + 0x10);
				if (m->level > 0x40) m->level = 0x40;
			}
			if (bars)
			{
				SetScreenFlash(0x800, 0);
				// 30 fps layer: see mag274_duel_held.inc
				FX_HELD(held_note_bars(m->level, NewMove() != 0);)
				DrawBars(m->level);
			}
			m->wait++;
			if (m->wait > 4) m->wait = 4;
			return 0;
		}
	start:
		if (NewMove())
		{
			Alloc() = TexBase() + 0x934;
			if (m->first)
			{
				uint8_t *c = Caster();
				m->caster_pos[0] = U32(c, 0x1C);
				m->caster_pos[1] = U32(c, 0x20);
				m->caster_rot[0] = U32(c, 0xC);
				m->caster_rot[1] = U32(c, 0x10);
				m->target_pos[0] = U32(m->target, 0x1C);
				m->target_pos[1] = U32(m->target, 0x20);
				GetDefaultEffectPosition(m->target, m->anchor);
				if (m->target[0] & 2) g_pad_meteor = 0x01D9; // (the high half of the pushed 0x1D97708 left at the move tick's slot)
				if (m->target[4] == 0x8C)
				{
					m->anchor[2] = (int16_t)(m->anchor[2] - 0xC80);
				}
				else if (m->target[4] == 0x54)
				{
					// (the party lifted by 1000)
					SavedPartyY()[0] = W16(Entity(0), 0x20);
					SavedPartyY()[1] = W16(Entity(1), 0x20);
					SavedPartyY()[2] = W16(Entity(2), 0x20);
					W16(Entity(0), 0x20) = (int16_t)(SavedPartyY()[0] + 1000);
					W16(Entity(1), 0x20) = (int16_t)(SavedPartyY()[1] + 1000);
					W16(Entity(2), 0x20) = (int16_t)(SavedPartyY()[2] + 1000);
					m->anchor[2] = (int16_t)(m->anchor[2] - 0x7D0);
				}
				m->anchor[1] = W16(m->target_pos, 2);
				BindExternalAnimSet(c, (uint32_t)m->bind, COM_Anim);
				StreamStateInit((void *)Alloc());
				Alloc() += 0x18;
				StreamQueueLoad(0x2CB, Alloc(), 1);
				StreamQueueLoad(0x2CC, Alloc(), 0x10);
				Alloc() += 0x8800;
			}
			U32((void *)TexBase(), 0x92C) = Alloc();
			const uint32_t arena = m->mode == 6 ? 0x14000 : 0xE000;
			Alloc() += arena;
			U32((void *)TexBase(), 0x930) = Alloc();
			Alloc() += arena;
			W16(Caster(), 0xE) = 0;
			U32(m->target, 0x1C) = m->target_pos[0];
			U32(m->target, 0x20) = m->target_pos[1];
			RestoreChainFlags(m->target, 0x1004);
			QueueChainTransformation(Caster(), m->mode);
			CameraAnimation(CAMERA_Data + var<uint16_t>(0x110BE74), (((uint32_t)m->mode - 2) << 4) + m->target[0x91]);
			m->length = LENGTH_Table[m->mode];
			NewMove() = 0;
			m->counter = 0;
		}
		if (m->first) StreamPump();
		if (m->counter <= (int16_t)m->length)
		{
			switch (m->mode)
			{
			case 2: MovePunchRush(m); break;
			case 3: MoveBooya(m); break;
			case 4: MoveHeelDrop(m); break;
			case 5: MoveMachKick(m); break;
			case 6: MoveDolphinBlow(m); break;
			case 7: MoveMeteorStrike(m); break;
			case 8:
			{
				// the end: everything put back; a finisher takes over at once, else the action result and the texture
				// restore task
				uint8_t *c = Caster();
				if (m->counter == 0)
				{
					U32(c, 0x1C) = m->caster_pos[0];
					U32(c, 0x20) = m->caster_pos[1];
					U32(c, 0xC) = m->caster_rot[0];
					U32(c, 0x10) = m->caster_rot[1];
					U32(m->target, 0x1C) = m->target_pos[0];
					U32(m->target, 0x20) = m->target_pos[1];
					RestoreChainFlags(m->target, 0x1004);
					if (m->target[4] == 0x54)
					{
						W16(Entity(0), 0x20) = SavedPartyY()[0];
						W16(Entity(1), 0x20) = SavedPartyY()[1];
						W16(Entity(2), 0x20) = SavedPartyY()[2];
					}
				}
				if (Ctx()->submode != 0)
				{
					Ctx()->done = 1;
					return TASK_END;
				}
				if (m->counter == 0)
				{
					Damage(m);
					m->w60[0] = 0;
					TextureRestoreTask((void *)U32((void *)TexBase(), 0x92C), &m->w60[0]);
				}
				if (m->w60[0] == 0 || m->counter < (int16_t)m->length)
				{
					m->counter++;
					return 0;
				}
				Ctx()->done = 1;
				return TASK_END;
			}
			default:
				break;
			}
		}
		m->counter++;
		if (m->counter == m->length)
		{
			if (m->first && StreamIsBusy())
			{
				m->counter--;
				return 0;
			}
			m->first = 0;
			m->wait = 1;
			m->level = 0;
			Ctx()->done = 1;
		}
		if (m->counter > (int16_t)m->length) m->counter = m->length;
		return 0;
	}

	// ------------------------------------------------------------------
	// RootTask (0x689990, node 0x18 at 0x25084C8)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag274_duel_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = U32((void *)TexBase(), 0x92C);
			r->arena = 0;
		}
		else
		{
			PacketCursor() = U32((void *)TexBase(), 0x930);
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QMoves(), (void *)TexBase(), 0x74, 1);
			}
			MoveNode *m = (MoveNode *)AddTaskToQueue(QMoves(), ORIG_MoveTask);
			if (m)
			{
				Memset32(&m->counter, 0, 0x1A);
				m->mode = 0;
				m->action = r->action;
				m->first = 1;
				m->target = Entity(ActionTargets(r->action)[0]);
				m->target = EntityChainLowest(m->target);
				MeasureModelHeight(m->target, m->height);
				r->action++;
			}
		}
		if (r->started && ExecuteTaskQueue(QMoves()) == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 10;
		return 0;
	}
}

	void register_mag274_duel()
	{
		register_port(duel274::ORIG_RootTask, (void *)duel274::RootTask, "274 RootTask", 274);
		register_port(duel274::ORIG_MoveTask, (void *)duel274::MoveTask, "274 MoveTask", 274);
		register_port(duel274::ORIG_SparkTask, (void *)duel274::SparkTask, "274 SparkTask", 274);
		register_port(duel274::ORIG_FlashTask, (void *)duel274::FlashTask, "274 FlashTask", 274);
		register_port(duel274::ORIG_RockTask, (void *)duel274::RockTask, "274 RockTask", 274);
		// 30 fps layer: see mag274_duel_held.inc
		FX_HELD(register_mag274_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag274_duel_held.inc"
#endif
