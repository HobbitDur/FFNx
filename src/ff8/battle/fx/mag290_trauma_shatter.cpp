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

// Effect 290: Trauma's body shattering (enemy attack 291 of kernel.bin, no name; Trauma c0m109; MAG_290_*).
//
// Structure (setup 0x675280 -> _Init 0x6752B0, file loader 0x675290 = mag289.tim (name at 0x106E894) into 0x2507B38; _Init
// queues the TIM, keeps the context (0x2507B3C) and the CASTER's entity (0x2507B40), clears bits 2 / 3 of its word +0 then
// sets bit 3, sets up the root queue 0x2507B98 (pool 0x2507BA8, 1 x 0x14) and starts camera animation 0x106A2DC):
//   RootTask (0x675340) - alternates the packet arena (magic buffer + 0x8608 / + 0x14608); on counter 1 (once): music volume
//     command 0x47E3C0(0xF0), the pools (master 1 x 0x68E4 at + 0x1CDC, layouts 3 x 0x124 at + 0x1970, sprites B 16 x 0x128 at
//     + 0x6F0, sprites A 32 x 0x18 at + 0x3F0, bone scripts 28 x 0x24 at the magic buffer), the master (voice slot 0x106A3A0,
//     the caster's colour +0x2C kept, layouts 0x1067CB8 / 0x106971C / 0x10688C4), 20 bone scripts (0x106E648..0x106E884),
//     the caster's bit 3 cleared, the stream state (+ 0x85C0). Then the master, script, layout, sprite B and sprite A queues;
//     counter 80: file 0x168 streamed into + 0x20608; the stream pumped every tick once started; when the master queue is
//     empty and the stream is done (0x5342C0): the caster's bit 3 cleared, the end.
//   MasterTask (0x6767B0) - counters 0..89. Counters 0..15: layout A under the caster (its spawn position, y - 1000, depth
//     -0x200) and sprite 0x106784C (frame = counter) through the shadow-rotation camera (0x4000, -0x400); counters 20..41:
//     layouts B and C at the caster's spawn position 1 / 0x400, y = 0, turned 0x800; counters 0..19: the caster's colour
//     +0x2C blended from its kept colour to 0x2507BC4 (weight counter x 0x1000 / 20, byte +0x2F = 0x32); every tick the
//     caster drawn by the module with its pose offsets (0x676B40 -> 0x676C80 -> 0x676E00 / 0x669330: shadow, then every bone
//     group whose pose entry's visibility word is below 2, into the frame arena) and its bit 2 set; from counter 30 its bit 3
//     set; the fragments and the marks drawn; sound 0x106A39C at counter 0, the voice slot released at counter 86; counters
//     0..24: a sprite B (CRT rand velocity) and 2 sprites A (CRT rand offset / velocity) at the spawn position; then the
//     fragments move.
//   ScriptTask (0x6756A0) - a byte-code program (0x1000..0x1100) moving one of the 28 pose entries (translation, rotation, their
//     velocities, bounce on the ground, copies of other entries), shattering bone groups (0x1090 -> 0x675AA0), spawning a layout
//     at a bone (0x10A0 -> 0x676340), counted loops (0x10F0: the counter is stored in the program data), end (0x1100); any other
//     word yields until the next tick.
//   Shatter (0x675AA0 -> 0x675C50 -> 0x687AE0 / 0x675DB0): the listed bones' pose entries hidden (visibility 2), their vertex
//     groups through the posed bone matrices (world space), then every polygon of those objects becomes part of a fragment
//     (8 polygons each, a fragment starts with the shared-LCG chance given by the script; a fragment's velocity is the polygon's
//     normal (GTE outer product, scaled to the script's speed), its spin / prim model from the module LCG 0x2507BC0, a mark at its
//     first vertex); up to the script's count of fragments.
//   LayoutTask (0x676400) - a layout (0x1067CB8) at a bone's spawn position + its pose entry's translation, scaled by +0x12;
//     ends with the layout.
//   SpriteTaskA (0x677550) / SpriteTaskB (0x6775F0) - sprite sequence 0x10676E0 / 0x1067B58 through the shadow-rotation camera
//     (0x800 / 0x1770, -0x400), frame = its counter, then moves by its velocity bytes; 12 / 13 ticks.
//   Fragments (master +0x1D8, 192 x 0x7C: 0x676EF0 draws, 0x6774C0 moves): a prim model (one of 4) turned by RotX(+0x10) x
//     RotY(+0x12) scaled by +0x0E, its polygons' texture words from the shattered polygons, faded over its last 8 ticks
//     (0x677180 -> 0x6771E0); moves by its velocity, falls (+0x14 a tick), bounces at the ground (y velocity -v / 2, spin
//     reversed). Marks (master +0x5ED8, 128 x 0xC: 0x677690 -> 0x677720): flipbook 0x10679F8 (frame 17 - life) at their point,
//     moving by their velocity bytes x 4; 12 ticks.
//   PrimCallback (0x6764C0) - one layout object drawn by 0x7043B0 (header 0x58): vertex frame (or blend), rotation (record flag
//     0x40000: YXZ order), the record's offset (and the object) scaled by the block's scale when the block says so, through the
//     block's matrix (record flag 0x200: as is), the record's scale (flag 0x100: as a diagonal matrix), fade.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws in the same tick it updates (draw THEN update); the master moves the fragments after drawing them.
// ENTITY WRITES: the caster's word +0 bits 2 / 3, its colour +0x2C / +0x2F (counters 0..19), its bone matrices (posed with the
//   pose entries' rotations, through its world matrix, then rebuilt from its pose 0x508C90) on every draw / shatter / layout
//   spawn. No position word is written.
// CAMERA: the setup's camera animation 0x106A2DC (engine); no camera word is written by the tasks.
// Module globals: 0x2507B38..0x2507BCC (texture file 0x2507B38, context 0x2507B3C, caster 0x2507B40, the queues 0x2507B48
// scripts / 0x2507B58 sprites A / 0x2507B68 sprites B / 0x2507B78 layouts / 0x2507B88 master / 0x2507B98 root, root pool
// 0x2507BA8, packet cursor 0x2507BBC, module LCG 0x2507BC0, colour target 0x2507BC4, magic buffer base 0x2507BC8).
// Exe data the module writes: the loop counters inside the bone scripts (0x106E648..0x106E894; never put back: a second cast
// runs the counted loops once).
// MAGIC BUFFER: + 0x2060C = the layouts' blended vertex frames (inside the streamed file's area + 0x20608).
// SHARED CODE (same code as 308, merge later: PrimCallback, ModLcg, CasterDraw, DrawModel, DrawPolys): 0x6764C0 / 0x676080 / 0x676B40 / 0x676C80 and 0x669330 (called) are the same code as effect 308's 0x668680 /
// 0x668450 / 0x669070 / 0x6691B0 / 0x669330 (and 298's 0x671B50 / 0x6716A0 / 0x672330 / 0x672470); 0x675C50, 0x677030 /
// 0x6770C0 / 0x677130 / 0x677180 are the same code as 298's 0x671280, 0x672730 / 0x6727C0 / 0x672830 / 0x672880; 0x676E00 is
// called by effect 64 too; 0x687AE0 is effect 276's (ported there too); 0x672DB0 (called) is effect 298's.
// STACK WORDS: the 4th words of the vectors loaded into GTE V0 (spawn positions, mark points) reach the GTE's VZ0 high half
// (0x1CA8A16) only. The layout block's matrix pad word (+0x12) is never written (not read by its users).
// FUJIN: Fujin's described Trauma shatter (effect_290.json) runs this code: its memory through own::R() (fx_port.h),
// its spawns through own::Fn(), the file loader (0x675290) and the setup (0x675280 -> _Init 0x6752B0) ported below; its
// parameters (ShatterParams: the sound, the voice and the ticks, the layouts under and round the caster, his tint, the
// sprites, the fragments and the marks, the 3 layouts, 4 flipbooks, 4 fragment meshes, the bone scripts, the texture
// name and the camera track). FX_PART tags its parts.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace shatter290
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x2507B40)); }        // caster entity (setup)
	inline TaskQueue *QScript() { return (TaskQueue *)own::R(0x2507B48); }         // pool: magic buffer, 28 x 0x24
	inline TaskQueue *QSpriteA() { return (TaskQueue *)own::R(0x2507B58); }        // pool: + 0x3F0, 32 x 0x18
	inline TaskQueue *QSpriteB() { return (TaskQueue *)own::R(0x2507B68); }        // pool: + 0x6F0, 16 x 0x128
	inline TaskQueue *QLayout() { return (TaskQueue *)own::R(0x2507B78); }         // pool: + 0x1970, 3 x 0x124
	inline TaskQueue *QMaster() { return (TaskQueue *)own::R(0x2507B88); }         // pool: + 0x1CDC, 1 x 0x68E4
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x2507BBC)); }
	inline uint32_t &LcgSeed() { return var<uint32_t>(own::R(0x2507BC0)); }        // module LCG
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2507BC8)); }        // Magic_TextureOFF (magic buffer)
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }    // battle_texture_data_ptr (frame arena)
	inline uint32_t &BoneWorkspace() { return var<uint32_t>(0x1D98B3C); }  // projected / world vertices
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x675340;
	static const uint32_t ORIG_MasterTask = 0x6767B0;
	static const uint32_t ORIG_ScriptTask = 0x6756A0;
	static const uint32_t ORIG_LayoutTask = 0x676400;
	static const uint32_t ORIG_SpriteTaskA = 0x677550;
	static const uint32_t ORIG_SpriteTaskB = 0x6775F0;
	static const uint32_t ORIG_PrimCallback = 0x6764C0;  // not a task: the layouts' callback
	static const uint32_t POOL_SpriteA = 0x3F0, POOL_SpriteB = 0x6F0, POOL_Layout = 0x1970, POOL_Master = 0x1CDC;
	static const uint32_t POSE = 0x1CF4;                 // magic buffer: the master's pose entries (master + 0x18)
	static const uint32_t MORPH = 0x2060C;               // magic buffer: blended vertex frames
	static const uint32_t STREAM_State = 0x85C0, STREAM_Dest = 0x20608, STREAM_File = 0x168;
	static const uint32_t ARENA_A = 0x8608, ARENA_B = 0x14608; // magic buffer: packet arenas
	static const uint32_t MODEL_LayoutA = 0x1067CB8, MODEL_LayoutB = 0x106971C, MODEL_LayoutC = 0x10688C4; // prim-model layouts
	static const uint32_t SEQ_Under = 0x106784C, SEQ_A = 0x10676E0, SEQ_B = 0x1067B58, SEQ_Mark = 0x10679F8; // sprite sequences
	static const uint32_t FRAG_Models[4] = { 0x1067400, 0x10674B8, 0x1067570, 0x1067628 };
	static const uint32_t COLOUR_Target = 0x2507BC4;     // 3 bytes (module data)
	static const uint32_t SIN_COS = 0x106A4C8;           // 4096 x (2 x s16)
	static const uint32_t SQRT_Table = 0x106E448;        // u16 (GTE-style square root table)
	static const void *const SOUND_Voice = (const void *)0x106A3A0;
	static const void *const SOUND_Start = (const void *)0x106A39C;

	static const uint32_t ORIG_FileLoader = 0x675290, ORIG_Setup = 0x675280;
	static const uint32_t TEXTURE_NAME = 0x106E894, CAMERA_ANIM = 0x106A2DC, SCRIPT_TABLE = 0x106E648;
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x2507B38)); }     // the effect's file (its TIM)

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_UNDER = 1, PART_RINGS = 2, PART_BODY = 3, PART_FRAGMENTS = 4, PART_MARKS = 5, PART_SPRITES = 6, PART_BURSTS = 7 };
	struct ShatterParams
	{
		struct { uint32_t start, voice; } sounds;    // the start sound, the voice (its slot claimed at the start)
		// the master's ticks: the start sound, the voice released, its end
		struct { int16_t start, release, end; } timing;
		// layout A and the flipbook under the caster: its first ticks, so far below his spawn point, depth, the sprite's size
		struct { int16_t ticks, drop, depth, size; } under;
		// layouts B and C at the caster's spawn point (bone, fraction) on the ground: from start for ticks, their turn, depth
		struct { int16_t start, ticks, bone, frac, turn, depth; } rings;
		// the caster's colour blended to black over ticks (its byte +0x2F set to alpha); drawn faded from hide_from
		struct { int16_t ticks, alpha, hide_from; } tint;
		// the sprites at the caster's spawn point on the first ticks: A (count a tick, scatter, size, life), B (count, size,
		// life)
		struct { int16_t ticks, count_a, spread, size_a, life_a, count_b, size_b, life_b; } sprites;
		// the fragments: life (+ a random below 16), fall speed's growth a tick, fade over their last ticks, spin range
		struct { int16_t life, gravity, fade, spin; } fragments;
		struct { int16_t life, speed; } marks;      // the marks: life (their flipbook's frame 12 - life), speed (x their velocity bytes)
		struct { uint32_t a, b, c; } layouts;       // the prim-model layouts played (A also by the bone scripts' layout op)
		struct { uint32_t under, a, b, mark; } seqs; // the sprite sequences drawn (flipbooks)
		struct { uint32_t frag[4]; } models;        // the fragments' meshes (one of four at random)
		struct { uint32_t scripts; } tables;        // the 20 bone scripts (byte code: their loop counters are written)
		struct { uint32_t texture_name, camera; } res;   // its resources (data): texture file name, camera track
	};
	static const ShatterParams GAME = { { 0x106A39C, 0x106A3A0 }, { 0, 0x56, 0x5A }, { 0x10, 0x3E8, -0x200, 0x4000 },
		{ 0x14, 0x16, 1, 0x400, 0x800, 0 }, { 0x14, 0x32, 0x1E }, { 0x19, 2, 0x100, 0x800, 0xC, 1, 0x1770, 0xD },
		{ 0x10, 0x14, 8, 0x200 }, { 0xC, 4 }, { 0x1067CB8, 0x106971C, 0x10688C4 }, { 0x106784C, 0x10676E0, 0x1067B58, 0x10679F8 },
		{ { 0x1067400, 0x10674B8, 0x1067570, 0x1067628 } }, { SCRIPT_TABLE }, { TEXTURE_NAME, CAMERA_ANIM } };
	static ShatterParams live = GAME;
	static const ShatterParams &PG() { return live; }
	static const uint32_t SCRIPT_TABLE_SIZE = 0x24C;     // 0x106E648..0x106E894
	// the 20 bone scripts started at counter 1 (first word: pose entry)
	static const uint32_t SCRIPTS[20] = {
		0x106E648, 0x106E664, 0x106E6A4, 0x106E6E4, 0x106E74C, 0x106E75C, 0x106E76C, 0x106E77C, 0x106E78C, 0x106E79C,
		0x106E7AC, 0x106E7BC, 0x106E824, 0x106E834, 0x106E844, 0x106E854, 0x106E864, 0x106E874, 0x106E884, 0 };

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }                // Effect_RotationYMatrix
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }  // MAG_017_sub_701310
	inline void RotationFromAnglesYXZ(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply3x3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }       // a.rot = a.rot x b
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b.rot = a.rot x b.rot
	inline void Matrix3x3MultiplyPSX(const void *a, const void *b, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x56C090)(a, b, out); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrimModel70(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_RenderPrimModel
	inline void PoseBones(uint8_t *anim_header, const void *pose) { fn<void (__cdecl *)(uint8_t *, const void *)>(0x662C00)(anim_header, pose); } // Battle_BuildBoneMatricesWithPoseOffsets
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, shift, cursor); } // BattleEntity_DrawShadow
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
	inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); } // ColourLerp
	inline int32_t SharedLcg() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }   // Effect_SharedLcg_Rand
	inline void MusicVolume(int32_t v) { fn<void (__cdecl *)(int32_t)>(0x47E3C0)(v); }
	// battle file streaming (0x534110 state, 0x5341D0 load a file, 0x534210 pump, 0x5342C0 done)
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t n) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, n); }
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamDone(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); }
	// software GTE
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // V0 = 3 x u16
	inline void GteStoreIR123Minus8(void *end) { fn<void (__cdecl *)(void *)>(0x703EF0)(end); } // IR1..3 -> end - 8
	inline void GteSetSXY012(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E160)(a, b, c); }
	inline void GteSetSZ123(uint32_t a, uint32_t b, uint32_t c) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E1B0)(a, b, c); }
	inline void GteSetSZ0123(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45E1D0)(a, b, c, d); }
	inline void GteNCLIP() { fn<void (__cdecl *)()>(0x45EE10)(); }
	inline void GteReadMAC0(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3C0)(dst); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZWord(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }    // GTE_ReadOTZ (dword)
	inline void GteLoadIRColumn(const void *m) { fn<void (__cdecl *)(const void *)>(0x45E180)(m); } // GTE_LoadIRFromMatrixColumn
	inline void GteStoreIRColumn(void *m) { fn<void (__cdecl *)(void *)>(0x45E470)(m); }           // GTE_StoreIRToMatrixColumn
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }
	inline void GteLoadV0Dwords(const void *v) { fn<void (__cdecl *)(const void *)>(0x45E060)(v); } // GTE_LoadV0FromDwords
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteLoadIRBytes(const void *src) { fn<void (__cdecl *)(const void *)>(0x45E0E0)(src); }   // IR1..3 = 3 x u8
	inline void GteStoreIRBytes(void *dst) { fn<void (__cdecl *)(void *)>(0x45E410)(dst); }        // IR1..3 low bytes
	inline void GteSetRotDiag32(const void *v3) { fn<void (__cdecl *)(const void *)>(0x45DBC0)(v3); }  // GTE_SetRotDiagonal_v32
	inline void GteLoadIR32_45DBF0(const void *v3) { fn<void (__cdecl *)(const void *)>(0x45DBF0)(v3); } // (IDA: GTE_LoadIR123_32b)
	inline void GteLoadIR32_45E090(const void *v3) { fn<void (__cdecl *)(const void *)>(0x45E090)(v3); } // (IDA: GTE_LoadIR123_32)
	inline void GteOP() { fn<void (__cdecl *)()>(0x45E670)(); }
	inline void GteSQR() { fn<void (__cdecl *)()>(0x45F930)(); }
	inline void GteSumMAC123(void *dst) { fn<void (__cdecl *)(void *)>(0x703FE0)(dst); }
	inline void GteSetLZCS(int32_t v) { fn<void (__cdecl *)(int32_t)>(0x45DCA0)(v); }
	inline void GteReadLZCR(void *dst) { fn<void (__cdecl *)(void *)>(0x45E570)(dst); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct PoseEntry // 16 bytes, one per bone
	{
		int16_t pos[3];    // +0x00 translation added to the bone (after the entity's world matrix)
		int16_t vis;       // +0x06 >= 2: the bone's vertices are not drawn; low byte = vertex key
		int16_t rot[3];    // +0x08 rotation added to the animation pose (0x662C00)
		int16_t pad;       // +0x0E (the shatter's extra rotation when not 0)
	};
	struct Fragment // 0x7C bytes (master + 0x1D8, 192 of them)
	{
		int16_t pos[3];    // +0x00
		int16_t life;      // +0x06 ticks left (0 = free)
		int16_t vel[3];    // +0x08
		int16_t scale;     // +0x0E
		int16_t angle[2];  // +0x10 x / y
		int16_t spin[2];   // +0x14
		uint32_t model;    // +0x18 prim model (one of FRAG_Models)
		uint8_t uv[8][0xC]; // +0x1C the shattered polygons' texture words (uv0 + clut, uv1 + tpage, uv2)
	};
	struct Mark // 0xC bytes (master + 0x5ED8, 128 of them)
	{
		int16_t pos[3];    // +0x00
		int16_t life;      // +0x06 12 .. 1, 0 = free
		int8_t vel[3];     // +0x08 (x 4 a tick)
		uint8_t pad0B;
	};
	struct RootNode // pool of 1 node of 0x14 bytes (0x2507BA8)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MasterNode // pool of 1 node of 0x68E4 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..89
		int16_t pad0E;
		uint32_t voice;    // +0x10 voice slot
		uint32_t colour;   // +0x14 the caster's colour +0x2C as it was
		PoseEntry pose[28]; // +0x18
		Fragment frags[0xC0]; // +0x1D8
		Mark marks[0x80];  // +0x5ED8
		Fragment *next_frag; // +0x64D8 next fragment slot to try
		Mark *next_mark;   // +0x64DC next mark slot to try
		uint8_t layoutA[0x110]; // +0x64E0
		uint8_t layoutB[0x22C]; // +0x65F0
		uint8_t layoutC[0xC8];  // +0x681C
	};
	struct ScriptNode // pool of 28 nodes of 0x24 bytes
	{
		TaskNode hdr;
		PoseEntry *cur;    // +0x0C the entry this script moves
		const int16_t *ip; // +0x10
		int16_t vel[3];    // +0x14
		int16_t pad1A;
		int16_t avel[3];   // +0x1C
		int16_t pad22;
	};
	struct LayoutNode // pool of 3 nodes of 0x124 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t scale;     // +0x12
		uint8_t layout[0x110]; // +0x14
	};
	struct SpriteNode // pools of 0x18 / 0x128-byte nodes (the first 0x18 bytes used)
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t counter;   // +0x12 flipbook frame
		int8_t vel[3];     // +0x14
		uint8_t pad17;
	};
	// the prim-player callback's parameter block (a stack block of the master / layout task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 the layout's frame (camera composed in)
		int32_t scale[3];  // +0x20 the record offsets and the object scaled by it (4.12)
		int32_t scaled;    // +0x2C
		int32_t depth;     // +0x30 the header's +0x18
		uint32_t morph;    // +0x34 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(PoseEntry) == 0x10 && sizeof(Fragment) == 0x7C && sizeof(Mark) == 0xC, "Trauma shatter records");
	static_assert(sizeof(RootNode) == 0x14 && sizeof(MasterNode) == 0x68E4 && sizeof(ScriptNode) == 0x24, "Trauma shatter nodes");
	static_assert(sizeof(LayoutNode) == 0x124 && sizeof(SpriteNode) == 0x18 && sizeof(PrimArg) == 0x38, "Trauma shatter nodes");
	static_assert(offsetof(MasterNode, frags) == 0x1D8 && offsetof(MasterNode, marks) == 0x5ED8 && offsetof(MasterNode, layoutA) == 0x64E0, "Trauma shatter master");

	template<typename T> static inline T &F(uint8_t *s, uint32_t off) { return *(T *)(s + off); }
}
}

#ifdef FF8_FX_HELD
#include "mag290_trauma_shatter_held.h"
#endif

namespace ff8fx
{
namespace shatter290
{
	// sub_676080: module LCG (15 bits)
	static int32_t ModLcg()
	{
		const uint32_t v = (LcgSeed() * 125 + 0xE) & 0x7FFF;
		LcgSeed() = v;
		return (int32_t)v;
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6764C0; same code as 308's 0x668680 and 298's 0x671B50): one object of a layout
	// ------------------------------------------------------------------
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
		if (r->flags & 0x40000) RotationFromAnglesYXZ(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4];
		if (arg->scaled != 0)
		{
			off[0] = (int16_t)(mul32(r->pos[0], arg->scale[0]) >> 12);
			off[1] = (int16_t)(mul32(r->pos[1], arg->scale[1]) >> 12);
			off[2] = (int16_t)(mul32(r->pos[2], arg->scale[2]) >> 12);
		}
		else
		{
			off[0] = r->pos[0];
			off[1] = r->pos[1];
			off[2] = r->pos[2];
		}
		off[3] = 0;
		if (arg->scaled != 0) Scale3DMatrix(&m, arg->scale);
		int32_t t0, t1, t2;
		if (r->flags & 0x200)
		{
			t0 = off[0];
			t1 = off[1];
			t2 = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
			t0 = m.t[0];
			t1 = m.t[1];
			t2 = m.t[2];
		}
		m.t[0] = (int32_t)((uint32_t)t0 + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)t1 + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)t2 + (uint32_t)arg->m.t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				const int16_t s[9] = { r->scale[0], 0, 0, 0, r->scale[1], 0, 0, 0, r->scale[2] };
				MatrixMultiply3x3(&m, s);
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
		PacketCursor() = RenderPrimModel70(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// The caster's body with the pose entries (0x676B40 -> 0x676C80 -> 0x676E00 / 0x669330; same code as 308's 0x669070 /
	// 0x6691B0 / 0x669330)
	// ------------------------------------------------------------------
	// Work block (Field_Alloc 0x98): +0x00 polygon cursor, +0x04 vertex buffer (0x1D98B3C), +0x08 / +0x0A triangle / quad
	// counts, +0x0C clip rectangle (x 8 in the test), +0x14 colour word, +0x18 object mask, +0x1C..+0x3B four vertices
	// (sxy, sz, clip flags, key), +0x3C NCLIP, +0x44 OTZ, +0x4C the pose entries.

	// sub_676E00: one bone group's vertices projected into 8-byte entries (sxy, sz, clip flags, key); both cursors advanced
	static void ProjectGroup(const uint8_t **pp, uint8_t **outp, uint8_t *h, uint8_t key)
	{
		const uint8_t *p = *pp;
		uint8_t *out = *outp;
		const int32_t n = *(const int16_t *)p;
		p += 2;
		if (n != 0)
		{
			uint32_t left = (uint32_t)n;
			do
			{
				GteLoadV0u(p);
				GteRTPS();
				p += 6;
				GteReadSXY2(h + 0x1C);
				GteReadOTZ(h + 0x20);
				if (F<int16_t>(h, 0x20) <= 0)
					out[6] = 0x10;
				else
				{
					h[0x22] = 0;
					h[0x23] = key;
					const int32_t vx = F<int16_t>(h, 0x1C);
					if (vx < shl32(F<int16_t>(h, 0xC), 3)) h[0x22] = 1;
					else if (vx >= shl32(F<int16_t>(h, 0x10), 3)) h[0x22] = 2;
					const int32_t vy = F<int16_t>(h, 0x1E);
					if (vy < shl32(F<int16_t>(h, 0xE), 3)) h[0x22] |= 4;
					else if (vy >= shl32(F<int16_t>(h, 0x12), 3)) h[0x22] |= 8;
					*(uint32_t *)out = F<uint32_t>(h, 0x1C);
					*(uint32_t *)(out + 4) = F<uint32_t>(h, 0x20);
				}
				out += 8;
			} while (--left != 0);
		}
		*pp = p;
		*outp = out;
	}

	// sub_669330 (effect 308's code): the triangles then the quads of one object from the projected vertices; a polygon is
	// drawn when none of its vertices is clipped, they are not all outside one screen edge and it faces the camera
	static uint32_t DrawPolys(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *verts = *(const uint8_t *const *)(h + 4);
		const uint8_t *p = *(const uint8_t *const *)h;
		int16_t c = *(int16_t *)(h + 8);
		*(int16_t *)(h + 8) = (int16_t)(c - 1);
		while (c != 0)
		{
			const uint32_t i0 = *(const uint16_t *)p & 0xFFF, i1 = *(const uint16_t *)(p + 2) & 0xFFF, i2 = *(const uint16_t *)(p + 4) & 0xFFF;
			*(uint32_t *)(h + 0x1C) = *(const uint32_t *)(verts + i0 * 8);
			*(uint32_t *)(h + 0x20) = *(const uint32_t *)(verts + i0 * 8 + 4);
			*(uint32_t *)(h + 0x24) = *(const uint32_t *)(verts + i1 * 8);
			*(uint32_t *)(h + 0x28) = *(const uint32_t *)(verts + i1 * 8 + 4);
			*(uint32_t *)(h + 0x2C) = *(const uint32_t *)(verts + i2 * 8);
			*(uint32_t *)(h + 0x30) = *(const uint32_t *)(verts + i2 * 8 + 4);
			const uint8_t f0 = h[0x22], f1 = h[0x2A], f2 = h[0x32];
			if ((uint8_t)(f0 | f1 | f2) < 0x10 && !(f2 & (uint8_t)(f0 & f1)))
			{
				GteSetSXY012(*(const uint32_t *)(h + 0x1C), *(const uint32_t *)(h + 0x24), *(const uint32_t *)(h + 0x2C));
				GteNCLIP();
				GteReadMAC0(h + 0x3C);
				if (*(const int32_t *)(h + 0x3C) >= 0)
				{
					GteSetSZ123(*(const int16_t *)(h + 0x20), *(const int16_t *)(h + 0x28), *(const int16_t *)(h + 0x30));
					GteAVSZ3();
					*(uint32_t *)pk = 0x7000000;
					*(uint32_t *)(pk + 4) = ((uint32_t)((*(const uint16_t *)(p + 0xE) & 0x200) | 0x2400) << 16) | *(const uint32_t *)(h + 0x14);
					*(uint32_t *)(pk + 8) = *(const uint32_t *)(h + 0x1C);
					*(uint32_t *)(pk + 0x10) = *(const uint32_t *)(h + 0x24);
					*(uint32_t *)(pk + 0x18) = *(const uint32_t *)(h + 0x2C);
					*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 8);
					*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 0xC);
					*(uint16_t *)(pk + 0x16) &= 0xFDFF;
					*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 6);
					GteReadOTZWord(h + 0x44);
					InsertPrimAutoDepth(ot + (uint32_t)(*(const int32_t *)(h + 0x44) >> (shift & 31)) * 4, pk);
					pk += 0x20;
				}
			}
			p += 0x10;
			c = *(int16_t *)(h + 8);
			*(int16_t *)(h + 8) = (int16_t)(c - 1);
		}
		c = *(int16_t *)(h + 0xA);
		*(int16_t *)(h + 0xA) = (int16_t)(c - 1);
		while (c != 0)
		{
			const uint32_t i0 = *(const uint16_t *)p & 0xFFF, i1 = *(const uint16_t *)(p + 2) & 0xFFF;
			const uint32_t i2 = *(const uint16_t *)(p + 4) & 0xFFF, i3 = *(const uint16_t *)(p + 6) & 0xFFF;
			*(uint32_t *)(h + 0x1C) = *(const uint32_t *)(verts + i0 * 8);
			*(uint32_t *)(h + 0x20) = *(const uint32_t *)(verts + i0 * 8 + 4);
			*(uint32_t *)(h + 0x24) = *(const uint32_t *)(verts + i1 * 8);
			*(uint32_t *)(h + 0x28) = *(const uint32_t *)(verts + i1 * 8 + 4);
			*(uint32_t *)(h + 0x2C) = *(const uint32_t *)(verts + i2 * 8);
			*(uint32_t *)(h + 0x30) = *(const uint32_t *)(verts + i2 * 8 + 4);
			*(uint32_t *)(h + 0x34) = *(const uint32_t *)(verts + i3 * 8);
			*(uint32_t *)(h + 0x38) = *(const uint32_t *)(verts + i3 * 8 + 4);
			const uint8_t f0 = h[0x22], f1 = h[0x2A], f2 = h[0x32], f3 = h[0x3A];
			if ((uint8_t)(f3 | f0 | f1 | f2) < 0x10 && !(f2 & (uint8_t)(f3 & f0 & f1)))
			{
				GteSetSXY012(*(const uint32_t *)(h + 0x1C), *(const uint32_t *)(h + 0x24), *(const uint32_t *)(h + 0x2C));
				GteNCLIP();
				GteReadMAC0(h + 0x3C);
				if (*(const int32_t *)(h + 0x3C) >= 0)
				{
					GteSetSZ0123(*(const int16_t *)(h + 0x20), *(const int16_t *)(h + 0x28), *(const int16_t *)(h + 0x30), *(const int16_t *)(h + 0x38));
					GteAVSZ4();
					*(uint32_t *)pk = 0x9000000;
					*(uint32_t *)(pk + 4) = ((uint32_t)((*(const uint16_t *)(p + 0xE) & 0x200) | 0x2C00) << 16) | *(const uint32_t *)(h + 0x14);
					*(uint32_t *)(pk + 8) = *(const uint32_t *)(h + 0x1C);
					*(uint32_t *)(pk + 0x10) = *(const uint32_t *)(h + 0x24);
					*(uint32_t *)(pk + 0x18) = *(const uint32_t *)(h + 0x2C);
					*(uint32_t *)(pk + 0x20) = *(const uint32_t *)(h + 0x34);
					*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(p + 8);
					*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(p + 0xC);
					*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(p + 0x10);
					*(uint16_t *)(pk + 0x16) &= 0xFDFF;
					*(uint16_t *)(pk + 0x24) = *(const uint16_t *)(p + 0x12);
					GteReadOTZWord(h + 0x44);
					InsertPrimAutoDepth(ot + (uint32_t)(*(const int32_t *)(h + 0x44) >> (shift & 31)) * 4, pk);
					pk += 0x28;
				}
			}
			p += 0x14;
			c = *(int16_t *)(h + 0xA);
			*(int16_t *)(h + 0xA) = (int16_t)(c - 1);
		}
		return (uint32_t)pk;
	}

	// sub_676C80: every object of the model in the mask: each bone group projected through its bone matrix (key = the pose
	// entry's visibility byte), or marked clipped when the pose entry hides the bone (visibility >= 2); then its polygons
	static uint32_t DrawModel(const uint8_t *model, uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t *objs = *(const uint32_t *const *)(model + 4);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const int32_t count = (int32_t)objs[0];
		if (count <= 0) return cursor;
		for (int32_t i = 0; i < count; i++)
		{
			const uint8_t *p = *(const uint8_t *const *)(model + 4) + objs[1 + i];
			if (!(*(const uint32_t *)(h + 0x18) & (1u << (i & 31)))) continue;
			uint8_t *out = *(uint8_t **)(h + 4);
			const int32_t groups = *(const int16_t *)p;
			p += 2;
			if (groups > 0)
			{
				for (int32_t g = groups; g != 0; g--)
				{
					const int32_t bone = *(const int16_t *)p;
					p += 2;
					const PoseEntry *pe = *(const PoseEntry *const *)(h + 0x4C) + bone;
					if (pe->vis < 2)
					{
						const Mat4x3 *bm = (const Mat4x3 *)(bones + mul32(bone, 0x30) + 0x10);
						GteSetRotMatrixCtrl(bm);
						GteSetTransVectorCtrl(bm);
						ProjectGroup(&p, &out, h, *(const uint8_t *)&pe->vis);
					}
					else
					{
						const int32_t n = *(const int16_t *)p;
						p = p + n * 6 + 2;
						for (int32_t k = n; k != 0; k--)
						{
							out[6] = 0x10;
							out += 8;
						}
					}
				}
			}
			p = (const uint8_t *)(((uint32_t)p + 3) & ~3u);
			*(uint16_t *)(h + 8) = *(const uint16_t *)p;
			*(uint16_t *)(h + 0xA) = *(const uint16_t *)(p + 2);
			*(const uint8_t **)h = p + 0xC;
			cursor = DrawPolys(h, ot, shift, cursor);
		}
		return cursor;
	}

	// sub_676B40: the entity's shadow, then its body with the pose entries: bone matrices from the animation pose plus the
	// rotation offsets, through the entity's world matrix, plus the translation offsets, through the camera; then the bone
	// matrices rebuilt from the pose
	static void CasterDraw(uint8_t *entity, PoseEntry *pose)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x98);
		uint8_t *anim = entity + 0x60;
		FrameCursor() = DrawShadow(entity, RenderOT(0x4040), 0x10, FrameCursor());
		PoseBones(anim, pose);
		uint8_t *skel = **(uint8_t ***)(entity + 0x64);
		if (skel[0] != 0)
		{
			for (int32_t i = 0; i < skel[0]; i++)
			{
				Mat4x3 *bm = (Mat4x3 *)(skel + 0x20 + i * 0x30);
				ComposeAffineTransform((const Mat4x3 *)(entity + 0x40), bm, bm);
				bm->t[0] = (int32_t)((uint32_t)bm->t[0] + (uint32_t)(int32_t)pose[i].pos[0]);
				bm->t[1] = (int32_t)((uint32_t)bm->t[1] + (uint32_t)(int32_t)pose[i].pos[1]);
				bm->t[2] = (int32_t)((uint32_t)bm->t[2] + (uint32_t)(int32_t)pose[i].pos[2]);
				ComposeAffineTransform(&Camera(), bm, bm);
			}
		}
		*(PoseEntry **)(h + 0x4C) = pose;
		*(uint32_t *)(h + 4) = BoneWorkspace();
		*(uint16_t *)(h + 0xC) = 0;
		*(uint16_t *)(h + 0xE) = 0;
		*(uint16_t *)(h + 0x10) = 0x140;
		*(uint16_t *)(h + 0x12) = 0xD8;
		*(uint32_t *)(h + 0x14) = *(const uint32_t *)(entity + 0x28);
		*(uint32_t *)(h + 0x18) = *(const uint32_t *)(entity + 0x7C);
		FrameCursor() = DrawModel(*(const uint8_t *const *)(anim + 4), h, RenderOT(0x44), 0, FrameCursor());
		BuildBoneMatricesFromPose(anim);
		FieldFree(0x98);
	}

	// ------------------------------------------------------------------
	// Shatter (0x675AA0 -> 0x675C50 -> 0x687AE0 / 0x675DB0)
	// ------------------------------------------------------------------
	// Work block (Field_Alloc 0xB4): +0x00 rotation scratch; S = +0x20: S+0x00 polygon cursor, +0x04 vertex buffer
	// (0x1D98B3C), +0x08 / +0x0A triangle / quad counts, +0x0C colour word, +0x10 object mask, +0x14 fragments left, +0x18
	// polygons left in the current fragment, +0x1C speed, +0x20 scale, +0x24 chance (shared LCG <= it), +0x28 current fragment,
	// +0x2C its texture-word cursor, +0x30 the master node, +0x34..+0x53 four vertices (x, y, z, flag), +0x64 / +0x74 two edges,
	// +0x84 their cross product.

	// sub_687AE0 (effect 276's code): one bone group's vertices through the bone matrix (world space, +6 = 0)
	static void WorldGroup(const uint8_t **pp, uint8_t **outp)
	{
		const uint8_t *p = *pp;
		uint8_t *out = *outp;
		const int32_t n = *(const int16_t *)p;
		p += 2;
		if (n != 0)
		{
			uint32_t left = (uint32_t)n;
			do
			{
				GteLoadV0u(p);
				GteMVMVA_RotV0Tr();
				*(uint16_t *)(out + 6) = 0;
				out += 8;
				p += 6;
				GteStoreIR123Minus8(out);
			} while (--left != 0);
		}
		*pp = p;
		*outp = out;
	}

	// sub_675FD0: the next free fragment (from the cursor, then from the first slot), nullptr when all 192 are in use; its
	// prim model from the module LCG
	static Fragment *NewFragment(MasterNode *m)
	{
		Fragment *f = m->next_frag;
		if (f->life != 0)
		{
			f = m->frags;
			if (m->frags[0].life != 0)
			{
				int32_t n = 0xC0;
				do
				{
					f++;
					if (--n == 0) return nullptr;
				} while (f->life != 0);
			}
		}
		m->next_frag = (uintptr_t)f >= (uintptr_t)m + 0x5E5C ? m->frags : f + 1;
		const uint32_t k = (uint32_t)ModLcg() >> 0xD;
		if (k <= 3) f->model = fxp::model(PG().models.frag[k], FRAG_Models[k]);
		return f;
	}

	// sub_6762F0: the next free mark, nullptr when all 128 are in use
	static Mark *NewMark(MasterNode *m)
	{
		Mark *k = m->next_mark;
		if (k->life != 0)
		{
			k = m->marks;
			if (m->marks[0].life != 0)
			{
				int32_t n = 0x80;
				do
				{
					k++;
					if (--n == 0) return nullptr;
				} while (k->life != 0);
			}
		}
		m->next_mark = (uintptr_t)k >= (uintptr_t)m + 0x64CC ? m->marks : k + 1;
		return k;
	}

	// sub_6760A0: a fragment from the polygon in S: its first vertex, life 16 + LCG / 4096, velocity = the polygon's normal
	// (edge cross product, normalised by the square root table) x speed, a mark at its first vertex moving the same way, then
	// y velocity - 2 x speed, its spin / angles from the module LCG, its scale from the shared LCG
	static void FragmentInit(uint8_t *s, Fragment *f)
	{
		*(uint32_t *)f->pos = F<uint32_t>(s, 0x34);
		*(uint32_t *)&f->pos[2] = F<uint32_t>(s, 0x38);
		f->life = (int16_t)((SharedLcg() >> 0xC) + PG().fragments.life);
		const int32_t x0 = F<int16_t>(s, 0x34), y0 = F<int16_t>(s, 0x36), z0 = F<int16_t>(s, 0x38);
		F<int32_t>(s, 0x64) = x0 - F<int16_t>(s, 0x3C);
		F<int32_t>(s, 0x68) = y0 - F<int16_t>(s, 0x3E);
		F<int32_t>(s, 0x6C) = z0 - F<int16_t>(s, 0x40);
		F<int32_t>(s, 0x74) = F<int16_t>(s, 0x44) - x0;
		F<int32_t>(s, 0x78) = F<int16_t>(s, 0x46) - y0;
		F<int32_t>(s, 0x7C) = F<int16_t>(s, 0x48) - F<int16_t>(s, 0x38);
		GteSetRotDiag32(s + 0x64);
		GteLoadIR32_45DBF0(s + 0x74);
		GteOP();
		GteReadMAC123((int32_t *)(s + 0x84));
		int32_t big = F<int32_t>(s, 0x84);
		if (big < 0) big = -big;
		int32_t a = F<int32_t>(s, 0x88);
		if (a < 0) a = -a;
		if (a > big) big = a;
		a = F<int32_t>(s, 0x8C);
		if (a < 0) a = -a;
		if (a > big) big = a;
		GteSetLZCS(big);
		f->angle[0] = (int16_t)ModLcg();
		f->angle[1] = (int16_t)ModLcg();
		f->spin[0] = (int16_t)((ModLcg() >> 5) - PG().fragments.spin);
		f->spin[1] = (int16_t)((ModLcg() >> 5) - PG().fragments.spin);
		int32_t lz;
		GteReadLZCR(&lz);
		if (lz < 0x12)
		{
			const int32_t sh = 0x12 - lz;
			F<int32_t>(s, 0x84) >>= sh;
			F<int32_t>(s, 0x88) >>= sh;
			F<int32_t>(s, 0x8C) >>= sh;
		}
		GteLoadIR32_45E090(s + 0x84);
		GteSQR();
		int32_t sum;
		GteSumMAC123(&sum);
		GteSetLZCS(sum);
		int32_t lz2;
		GteReadLZCR(&lz2);
		lz2 &= ~1;
		const int32_t shift = (0x1F - lz2) >> 1;
		const int32_t e = lz2 - 0x18;
		const int32_t idx = e > 0 ? shl32(sum, e) : sum >> ((0x18 - lz2) & 31);
		const int32_t norm = (int32_t)((uint32_t)*(const uint16_t *)(SQRT_Table + idx * 2) << (shift & 31)) >> 0xC;
		const int32_t speed = F<int32_t>(s, 0x1C);
		f->vel[0] = (int16_t)(mul32(speed, F<int32_t>(s, 0x84)) / norm);
		f->vel[2] = (int16_t)(mul32(speed, F<int32_t>(s, 0x8C)) / norm);
		f->vel[1] = (int16_t)(mul32(F<int32_t>(s, 0x88), speed) / norm);
		Mark *k = NewMark(F<MasterNode *>(s, 0x30));
		if (k)
		{
			*(uint32_t *)k->pos = F<uint32_t>(s, 0x34);
			*(uint32_t *)&k->pos[2] = F<uint32_t>(s, 0x38);
			k->life = PG().marks.life;
			k->vel[0] = (int8_t)((uint16_t)f->vel[0] >> 2);
			k->vel[1] = (int8_t)((uint16_t)f->vel[1] >> 2);
			k->vel[2] = (int8_t)((uint16_t)f->vel[2] >> 2);
		}
		f->vel[1] = (int16_t)(f->vel[1] - (int16_t)(F<uint16_t>(s, 0x1C) << 1));
		f->scale = (int16_t)((mul32(SharedLcg(), F<int32_t>(s, 0x20)) >> 0xF) + F<int16_t>(s, 0x20));
	}

	// sub_675DB0 (part): one polygon of the shattered groups: a new fragment when the current one is full (shared-LCG chance;
	// the fragment budget), then the polygon's texture words into it
	enum { POLY_NEXT = 0, POLY_STOP = 1 };
	static int ShatterPoly(uint8_t *s, const uint8_t *uv0, const uint8_t *uv1, const uint8_t *word)
	{
		if (F<int32_t>(s, 0x18) == 0)
		{
			if (SharedLcg() > F<int32_t>(s, 0x24)) return POLY_NEXT;
			const int32_t left = F<int32_t>(s, 0x14);
			F<int32_t>(s, 0x14) = left - 1;
			if (left <= 0) return POLY_STOP;
			F<int32_t>(s, 0x18) = 8;
			Fragment *f = NewFragment(F<MasterNode *>(s, 0x30));
			F<Fragment *>(s, 0x28) = f;
			if (!f)
			{
				F<int32_t>(s, 0x14) = 0;
				return POLY_STOP;
			}
			F<uint8_t *>(s, 0x2C) = f->uv[0];
			FragmentInit(s, f);
		}
		uint8_t *w = F<uint8_t *>(s, 0x2C);
		F<int32_t>(s, 0x18)--;
		F<uint8_t *>(s, 0x2C) = w + 0xC;
		*(uint32_t *)w = *(const uint32_t *)uv0;
		*(uint32_t *)(w + 4) = *(const uint32_t *)uv1;
		*(uint16_t *)(w + 8) = *(const uint16_t *)word;
		return POLY_NEXT;
	}

	static inline void CopyWorldVertex(uint8_t *s, uint32_t off, const uint8_t *verts, uint16_t index)
	{
		const uint8_t *v = verts + (uint32_t)(index & 0xFFF) * 8;
		F<uint32_t>(s, off) = *(const uint32_t *)v;
		F<uint32_t>(s, off + 4) = *(const uint32_t *)(v + 4);
	}

	// sub_675DB0: the object's triangles and quads whose vertices were all transformed (flag 0)
	static void ShatterPolys(uint8_t *s)
	{
		const uint8_t *verts = F<uint8_t *>(s, 4);
		const uint8_t *p = F<uint8_t *>(s, 0);
		uint16_t k = F<uint16_t>(s, 8);
		F<uint16_t>(s, 8) = (uint16_t)(k - 1);
		if (k != 0)
		{
			do
			{
				CopyWorldVertex(s, 0x34, verts, *(const uint16_t *)p);
				CopyWorldVertex(s, 0x3C, verts, *(const uint16_t *)(p + 2));
				CopyWorldVertex(s, 0x44, verts, *(const uint16_t *)(p + 4));
				if ((uint8_t)(s[0x42] | s[0x4A] | s[0x3A]) == 0)
					if (ShatterPoly(s, p + 8, p + 0xC, p + 6) == POLY_STOP) return;
				k = F<uint16_t>(s, 8);
				p += 0x10;
				F<uint16_t>(s, 8) = (uint16_t)(k - 1);
			} while (k != 0);
		}
		k = F<uint16_t>(s, 0xA);
		F<uint16_t>(s, 0xA) = (uint16_t)(k - 1);
		if (k == 0) return;
		p += 4;
		do
		{
			CopyWorldVertex(s, 0x34, verts, *(const uint16_t *)(p - 4));
			CopyWorldVertex(s, 0x3C, verts, *(const uint16_t *)(p - 2));
			CopyWorldVertex(s, 0x44, verts, *(const uint16_t *)p);
			CopyWorldVertex(s, 0x4C, verts, *(const uint16_t *)(p + 2));
			if ((uint8_t)(s[0x42] | s[0x4A] | s[0x3A] | s[0x52]) == 0)
				if (ShatterPoly(s, p + 4, p + 8, p + 0xC) == POLY_STOP) return;
			k = F<uint16_t>(s, 0xA);
			p += 0x14;
			F<uint16_t>(s, 0xA) = (uint16_t)(k - 1);
		} while (k != 0);
	}

	// sub_675C50 (same code as 298's 0x671280): every object of the model in the mask: its marked bone groups (skeleton
	// bone +0x22 != 0) through their bone matrices into world vertices (others flagged 1); an object with a marked group is
	// shattered. While fragments are left, the walk goes on from the object's polygon list as if it were the next group list
	// (as the game does).
	static void ShatterModel(const uint8_t *model, uint8_t *s)
	{
		const uint32_t *table = *(const uint32_t *const *)(model + 4);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const int32_t count = (int32_t)table[0];
		const uint32_t *offs = table + 1;
		if (count <= 0) return;
		for (int32_t i = 0; i < count; i++)
		{
			const uint8_t *p = *(const uint8_t *const *)(model + 4) + *offs++;
			if (!(F<uint32_t>(s, 0x10) & (1u << (i & 31)))) continue;
			for (;;)
			{
				uint8_t *out = F<uint8_t *>(s, 4);
				int32_t any = 0;
				const int32_t groups = *(const int16_t *)p;
				p += 2;
				if (groups <= 0) break;
				for (int32_t g = groups; g != 0; g--)
				{
					const int32_t bone = *(const int16_t *)p;
					p += 2;
					const uint8_t *b = bones + bone * 0x30;
					if (*(const int16_t *)(b + 0x22) != 0)
					{
						const Mat4x3 *bm = (const Mat4x3 *)(b + 0x10);
						GteSetRotMatrixCtrl(bm);
						GteSetTransVectorCtrl(bm);
						WorldGroup(&p, &out);
						any = 1;
					}
					else
					{
						const int32_t n = *(const int16_t *)p;
						p = p + n * 6 + 2;
						for (int32_t k = n; k != 0; k--)
						{
							*(uint16_t *)(out + 6) = 1;
							out += 8;
						}
					}
				}
				if (!any) break;
				p = (const uint8_t *)(((uint32_t)p + 3) & ~3u);
				F<uint16_t>(s, 8) = *(const uint16_t *)p;
				F<uint16_t>(s, 0xA) = *(const uint16_t *)(p + 2);
				p += 0xC;
				F<const uint8_t *>(s, 0) = p;
				ShatterPolys(s);
				if (F<int32_t>(s, 0x14) <= 0) return;
			}
		}
	}

	// sub_675AA0: script op 0x1090 (bone ids up to -1, then fragment count, speed, scale, chance): the entity posed (rotation
	// offsets, world matrix, translation offsets, the entries' extra rotation +0x0E), the listed bones marked and their pose
	// entries hidden, the marked groups shattered; the bone matrices rebuilt from the pose; an unfinished fragment is dropped
	static const int16_t *Shatter(const int16_t *ip, uint8_t *entity, PoseEntry *pose)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<int32_t>(h, 0x38) = 0;
		F<uint32_t>(h, 0x50) = TexBase() + POOL_Master;
		uint8_t *anim = entity + 0x60;
		F<uint32_t>(h, 0x24) = BoneWorkspace();
		F<uint32_t>(h, 0x2C) = *(const uint32_t *)(entity + 0x28);
		F<uint32_t>(h, 0x30) = *(const uint32_t *)(entity + 0x7C);
		PoseBones(anim, pose);
		uint8_t *skel = **(uint8_t ***)(anim + 4);
		if (skel[0] != 0)
		{
			for (int32_t i = 0; i < skel[0]; i++)
			{
				Mat4x3 *bm = (Mat4x3 *)(skel + 0x20 + i * 0x30);
				ComposeAffineTransform((const Mat4x3 *)(entity + 0x40), bm, bm);
				if (pose[i].pad != 0)
				{
					BuildRotationMatrixFromAngles(pose[i].rot, (Mat4x3 *)h);
					Matrix3x3MultiplyPSX(h, bm, bm);
				}
				bm->t[0] = (int32_t)((uint32_t)bm->t[0] + (uint32_t)(int32_t)pose[i].pos[0]);
				bm->t[1] = (int32_t)((uint32_t)bm->t[1] + (uint32_t)(int32_t)pose[i].pos[1]);
				bm->t[2] = (int32_t)((uint32_t)bm->t[2] + (uint32_t)(int32_t)pose[i].pos[2]);
			}
		}
		skel = **(uint8_t ***)(anim + 4);
		for (uint32_t n = skel[0], k = 0; n != 0; n--, k++) *(int16_t *)(skel + 0x32 + k * 0x30) = 0;
		int32_t id = *ip++;
		while (id != -1)
		{
			*(int16_t *)(skel + id * 0x30 + 0x32) = 1;
			ip++;
			pose[id].vis = 2;
			id = ip[-1];
		}
		F<int32_t>(h, 0x34) = ip[0];
		F<int32_t>(h, 0x3C) = ip[1];
		F<int32_t>(h, 0x40) = ip[2];
		F<int32_t>(h, 0x44) = ip[3];
		ip += 4;
		ShatterModel(*(const uint8_t *const *)(anim + 4), h + 0x20);
		BuildBoneMatricesFromPose(anim);
		if (F<int32_t>(h, 0x38) > 0)
		{
			F<Fragment *>(h, 0x48)->life = 0;
			F<MasterNode *>(h, 0x50)->next_frag = F<Fragment *>(h, 0x48);
		}
		FieldFree(0xB4);
		return ip;
	}

	// ------------------------------------------------------------------
	// Fragments (0x676EF0 draw -> 0x677030 / 0x677180 -> 0x6771E0; 0x6774C0 move)
	// ------------------------------------------------------------------
	// sub_6770C0: RotX of angle a scaled by s (table 0x106A4C8)
	static void RotXScaled(int32_t a, int32_t s, uint8_t *out)
	{
		*(uint32_t *)out = 0;
		*(uint32_t *)(out + 4) = 0;
		*(uint32_t *)(out + 8) = 0;
		*(uint32_t *)(out + 0xC) = 0;
		*(uint32_t *)(out + 0x10) = 0;
		const int16_t *e = (const int16_t *)(SIN_COS + (a & 0xFFF) * 4);
		const int32_t s16 = (int16_t)s;
		const int32_t c = mul32(e[0], s16) >> 0xC;
		const int32_t d = mul32(e[1], s16) >> 0xC;
		*(int16_t *)out = (int16_t)s;
		*(int16_t *)(out + 0xE) = (int16_t)-c;
		*(int16_t *)(out + 8) = (int16_t)d;
		*(int16_t *)(out + 0xA) = (int16_t)c;
		*(int16_t *)(out + 0x10) = (int16_t)d;
	}

	// sub_677130: RotY of angle a (table 0x106A4C8)
	static void RotYTable(int32_t a, uint8_t *out)
	{
		*(uint32_t *)out = 0;
		*(uint32_t *)(out + 4) = 0;
		*(uint32_t *)(out + 8) = 0;
		*(uint32_t *)(out + 0xC) = 0;
		*(uint32_t *)(out + 0x10) = 0;
		const int16_t *e = (const int16_t *)(SIN_COS + (a & 0xFFF) * 4);
		const int16_t c = e[0], d = e[1];
		*(int16_t *)(out + 0xC) = c;
		*(int16_t *)out = d;
		*(int16_t *)(out + 8) = 0x1000;
		*(int16_t *)(out + 4) = (int16_t)-c;
		*(int16_t *)(out + 0x10) = d;
	}

	// sub_677030: out = RotX(ax) x s, times RotY(ay), through the GTE (columns)
	static void FragRotation(int32_t ax, int32_t ay, int32_t s, uint8_t *out)
	{
		uint8_t ry[0x20], rx[0x20];
		RotXScaled(ax, s, rx);
		RotYTable(ay, ry);
		GteSetRotMatrixCtrl((const Mat4x3 *)rx);
		GteLoadIRColumn(ry);
		GteMVMVA_RotIR();
		GteStoreIRColumn(out);
		GteLoadIRColumn(ry + 2);
		GteMVMVA_RotIR();
		GteStoreIRColumn(out + 2);
		GteLoadIRColumn(ry + 4);
		GteMVMVA_RotIR();
		GteStoreIRColumn(out + 4);
	}

	// sub_6771E0: the fragment model's triangles (3 vertex offsets per 0xC-byte record) projected; one that is not clipped, faces
	// the camera and is not all off one screen edge is drawn with the next texture words; faded (the colour below 0x80): semi-
	// transparent, followed by a flat subtractive copy and a draw-mode packet in the same bucket
	static uint32_t FragPolys(uint8_t *s, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *p = F<uint8_t *>(s, 0xC);
		const uint8_t *uv = F<uint8_t *>(s, 0x44);
		const int32_t count = *(const int32_t *)p;
		p += 4;
		const uint8_t *verts = F<uint8_t *>(s, 4);
		F<const uint8_t *>(s, 0xC) = p;
		if (count > 0)
		{
			int32_t left = count;
			do
			{
				GteLoadV012(verts + *(const uint16_t *)(p + 4) * 4, verts + *(const uint16_t *)(p + 6) * 4, verts + *(const uint16_t *)(p + 8) * 4);
				GteRTPT();
				*(uint32_t *)(pk + 4) = F<uint32_t>(s, 0x40);
				GteReadFLAG(s + 0x1C);
				if (!(F<uint32_t>(s, 0x1C) & 0x60000))
				{
					GteNCLIP();
					const uint8_t *w = F<uint8_t *>(s, 0x44);
					*(uint32_t *)(pk + 0xC) = *(const uint32_t *)w;
					*(uint32_t *)(pk + 0x14) = *(const uint32_t *)(w + 4);
					*(uint16_t *)(pk + 0x1C) = *(const uint16_t *)(w + 8);
					GteReadMAC0(s + 0x10);
					if (F<int32_t>(s, 0x10) >= 0)
					{
						GteReadSXY012Split(pk + 8, pk + 0x10, pk + 0x18);
						GteAVSZ3();
						uint32_t clip = 0;
						int16_t v = *(const int16_t *)(pk + 8);
						if (v < 0 || v > 0xA00) clip = 1;
						v = *(const int16_t *)(pk + 0x10);
						if (v < 0 || v > 0xA00) clip |= 2;
						v = *(const int16_t *)(pk + 0x18);
						if (v < 0 || v > 0xA00) clip |= 4;
						v = *(const int16_t *)(pk + 0xA);
						if (v < 0 || v > 0x6C0) clip |= 0x10;
						v = *(const int16_t *)(pk + 0x12);
						if (v < 0 || v > 0x6C0) clip |= 0x20;
						v = *(const int16_t *)(pk + 0x1A);
						if (v < 0 || v > 0x6C0) clip |= 0x40;
						if ((clip & 7) != 7 && (clip & 0x70) != 0x70)
						{
							GteReadOTZWord(s + 0x18);
							*(uint32_t *)pk = 0x7000000;
							const uint32_t bucket = ot + (uint32_t)(F<int32_t>(s, 0x18) >> (mode & 31)) * 4;
							if (F<int32_t>(s, 8) == 0x1000)
							{
								pk[7] = 0x24;
								InsertPrimAutoDepth(bucket, pk);
								pk += 0x20;
							}
							else
							{
								pk[7] = 0x26;
								InsertPrimAutoDepth(bucket, pk);
								uint8_t *flat = pk + 0x20;
								GteReadSXY012Split(flat + 8, flat + 0xC, flat + 0x10);
								*(uint32_t *)(flat + 4) = F<uint32_t>(s, 0x40);
								*(uint32_t *)flat = 0x4000000;
								flat[7] = 0x22;
								InsertPrimAutoDepth(bucket, flat);
								uint8_t *dm = pk + 0x34;
								*(uint32_t *)dm = 0x1000000;
								*(uint32_t *)(dm + 4) = 0xE1000240;
								InsertPrimAutoDepth(bucket, dm);
								pk += 0x3C;
							}
						}
					}
				}
				p += 0xC;
				F<uint8_t *>(s, 0x44) += 0xC;
			} while (--left != 0);
		}
		F<const uint8_t *>(s, 0xC) = p;
		F<const uint8_t *>(s, 0x44) = uv;
		return (uint32_t)pk;
	}

	// sub_677180: the fragment's prim model (vertices at +8, triangles at + its first word) in grey 0x80 faded by +0x08
	static uint32_t FragDraw(uint8_t *s, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		const uint8_t *model = F<uint8_t *>(s, 0);
		F<const uint8_t *>(s, 4) = model + 8;
		F<const uint8_t *>(s, 0xC) = model + *(const uint32_t *)model;
		s[0x42] = 0x80;
		s[0x41] = 0x80;
		s[0x40] = 0x80;
		GteSetIR0(F<int32_t>(s, 8));
		GteLoadIRBytes(s + 0x40);
		GteGPF();
		GteStoreIRBytes(s + 0x40);
		return FragPolys(s, ot, mode, cursor);
	}

	// sub_676EF0 (fade): over the fragment's last 8 ticks
	static int32_t FragFade(int16_t life) { return life < PG().fragments.fade ? shl32(life, 12) / PG().fragments.fade : 0x1000; }

	// sub_676EF0 (one fragment): its frame (rotation x scale through the camera, its point through the camera), faded (work
	// block 0x68: +0x00 matrix, S = +0x20)
	static void FragmentDraw(uint8_t *h, const Fragment *f, int32_t fade)
	{
		FragRotation(f->angle[0], f->angle[1], f->scale, h);
		F<int32_t>(h, 0x14) = f->pos[0];
		F<int32_t>(h, 0x18) = f->pos[1];
		F<int32_t>(h, 0x1C) = f->pos[2];
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
		F<uint32_t>(h, 0x20) = f->model;
		F<int32_t>(h, 0x28) = fade;
		F<const uint8_t *>(h, 0x64) = f->uv[0];
		PacketCursor() = FragDraw(h + 0x20, RenderOT(0x44), 2, PacketCursor());
	}

	// sub_676EF0: every live fragment
	static void DrawFragments(MasterNode *m)
	{
		// 30 fps layer: see mag290_trauma_shatter_held.inc
		FX_HELD(held_note_frags(m);)
		FX_PART(PART_FRAGMENTS);
		uint8_t *h = (uint8_t *)FieldAlloc(0x68);
		const Fragment *f = m->frags;
		for (int32_t n = 0xC0; n != 0; n--, f++)
			if (f->life != 0) FragmentDraw(h, f, FragFade(f->life));
		FieldFree(0x68);
	}

	// sub_6774C0: every live fragment one tick older (a freed one becomes the next slot to try), moved, falling, bouncing
	static void MoveFragments(MasterNode *m)
	{
		Fragment *f = m->frags;
		for (int32_t n = 0xC0; n != 0; n--, f++)
		{
			if (f->life == 0) continue;
			f->life--;
			if (f->life == 0)
			{
				m->next_frag = f;
				continue;
			}
			f->angle[0] = (int16_t)(f->angle[0] + f->spin[0]);
			f->angle[1] = (int16_t)(f->angle[1] + f->spin[1]);
			f->pos[0] = (int16_t)(f->pos[0] + f->vel[0]);
			f->pos[2] = (int16_t)(f->pos[2] + f->vel[2]);
			f->pos[1] = (int16_t)(f->pos[1] + f->vel[1]);
			f->vel[1] = (int16_t)(f->vel[1] + PG().fragments.gravity);
			if (f->pos[1] > 0)
			{
				f->vel[1] = (int16_t)(-(int32_t)f->vel[1] >> 1);
				f->spin[0] = (int16_t)-f->spin[0];
				f->spin[1] = (int16_t)-f->spin[1];
			}
		}
	}

	// ------------------------------------------------------------------
	// Marks (0x677690 -> 0x677720 -> 0x672DB0)
	// ------------------------------------------------------------------
	// sub_672DB0 (effect 298's code): the frame's sprites (0x14-byte records, see 301's 0x66E090) as quads around the work
	// block's point (+0x84 matrix, +0x98 translation), into bucket `slot`
	static uint32_t MarkSprites(uint8_t *h, uint32_t ot, int32_t slot, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const int32_t count = F<int32_t>(h, 0x30);
		const uint8_t *r = F<uint8_t *>(h, 0x2C);
		if (count > 0)
		{
			uint32_t left = (uint32_t)count;
			do
			{
				F<uint16_t>(h, 0x72) = 0;
				F<uint16_t>(h, 0x70) = 0;
				F<uint16_t>(h, 0x6E) = 0;
				F<uint16_t>(h, 0x6A) = 0;
				F<uint16_t>(h, 0x68) = 0;
				F<uint16_t>(h, 0x66) = 0;
				F<uint16_t>(h, 0x74) = 0x1000;
				F<uint16_t>(h, 0x64) = *(const uint16_t *)(r + 0x10);
				F<uint16_t>(h, 0x6A) = 0;
				F<uint16_t>(h, 0x66) = 0;
				F<uint16_t>(h, 0x6C) = *(const uint16_t *)(r + 0x12);
				const int32_t tpage = *(const int16_t *)(r + 0xE);
				const uint32_t w = r[0], hh = r[2];
				const int32_t w8 = (int32_t)(w << 3), h8 = (int32_t)(hh << 3);
				F<int16_t>(h, 0x54) = (int16_t)-w8;
				F<int16_t>(h, 0x44) = (int16_t)-w8;
				F<int16_t>(h, 0x5C) = (int16_t)w8;
				F<int16_t>(h, 0x4C) = (int16_t)w8;
				F<int16_t>(h, 0x4E) = (int16_t)-h8;
				F<int16_t>(h, 0x46) = (int16_t)-h8;
				F<int16_t>(h, 0x5E) = (int16_t)h8;
				F<int16_t>(h, 0x56) = (int16_t)h8;
				F<int32_t>(h, 0x78) = shl32(*(const int16_t *)(r + 8), 4) + w8;
				F<int32_t>(h, 0x7C) = shl32(*(const int16_t *)(r + 0xA), 4) + h8;
				F<int32_t>(h, 0x80) = 0;
				GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x84));
				GteLoadIRColumn(h + 0x64);
				GteMVMVA_RotIR();
				*(uint32_t *)pk = 0x9000000;
				*(uint32_t *)(pk + 4) = *(const uint32_t *)r;
				GteStoreIRColumn(h + 0x64);
				GteLoadIRColumn(h + 0x66);
				GteMVMVA_RotIR();
				*(uint32_t *)(pk + 0xC) = *(const uint32_t *)(r + 4);
				GteStoreIRColumn(h + 0x66);
				GteLoadIRColumn(h + 0x68);
				GteMVMVA_RotIR();
				pk[0x1C] = r[4];
				GteStoreIRColumn(h + 0x68);
				GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x84));
				GteLoadV0Dwords(h + 0x78);
				GteMVMVA_RotV0Tr();
				pk[0x15] = r[5];
				GteReadMAC123((int32_t *)(h + 0x78));
				GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x64));
				GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x64));
				GteLoadV012(h + 0x44, h + 0x4C, h + 0x54);
				GteRTPT();
				uint32_t u = r[4] + w;
				if ((int32_t)u >= 0x100) u = 0xFF;
				pk[0x24] = (uint8_t)u;
				pk[0x14] = (uint8_t)u;
				GteReadFLAG(h + 0x40);
				if (!(F<uint32_t>(h, 0x40) & 0x60000))
				{
					*(uint16_t *)(pk + 0x16) = (uint16_t)(tpage & 0x1FF);
					GteReadSXY012Split(pk + 8, pk + 0x10, pk + 0x18);
					GteLoadV0(h + 0x5C);
					GteRTPS();
					uint32_t v = r[5] + hh;
					if ((int32_t)v >= 0x100) v = 0xFF;
					pk[0x25] = (uint8_t)v;
					pk[0x1D] = (uint8_t)v;
					GteReadSXY2(pk + 0x20);
					InsertPrimAutoDepth(ot + (uint32_t)slot * 4, pk);
					pk += 0x28;
				}
				r += 0x14;
			} while (--left != 0);
		}
		return (uint32_t)pk;
	}

	// sub_677720 (draw): the mark's point through the camera, flipbook frame 17 - life
	static void MarkSprite(uint8_t *h, const Mark *k)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		GteLoadV0(k->pos);
		GteMVMVA_RotV0Tr();
		GteReadMAC123((int32_t *)(h + 0x98));
		F<uint16_t>(h, 0x94) = 0x1000;
		F<uint16_t>(h, 0x8C) = 0x1000;
		F<uint16_t>(h, 0x84) = 0x1000;
		const uint32_t seq = F<uint32_t>(h, 0);
		const uint32_t frame = seq + *(const uint16_t *)(seq + (0x11 - k->life) * 2);
		F<uint32_t>(h, 0x2C) = frame;
		F<uint32_t>(h, 0x30) = *(const uint32_t *)frame;
		F<uint32_t>(h, 0x2C) = frame + 4;
		PacketCursor() = MarkSprites(h, RenderOT(0x44), 2, PacketCursor());
	}

	// sub_677720: draw, then one tick less and moved by its velocity bytes x 4
	static void MarkDraw(uint8_t *h, Mark *k)
	{
		MarkSprite(h, k);
		k->life--;
		k->pos[0] = (int16_t)(k->pos[0] + (int16_t)(k->vel[0] * PG().marks.speed));
		k->pos[1] = (int16_t)(k->pos[1] + (int16_t)(k->vel[1] * PG().marks.speed));
		k->pos[2] = (int16_t)(k->pos[2] + (int16_t)(k->vel[2] * PG().marks.speed));
	}

	// sub_677690: every live mark (work block 0xB4: sequence 0x10679F8, the identity rotation and the sprite vertices' z words
	// zeroed)
	static void DrawMarks(MasterNode *m)
	{
		// 30 fps layer: see mag290_trauma_shatter_held.inc
		FX_HELD(held_note_marks(m);)
		FX_PART(PART_MARKS);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint32_t>(h, 0) = fxp::sequence(PG().seqs.mark, SEQ_Mark);
		F<uint16_t>(h, 0x92) = 0;
		F<uint16_t>(h, 0x90) = 0;
		F<uint16_t>(h, 0x8E) = 0;
		F<uint16_t>(h, 0x8A) = 0;
		F<uint16_t>(h, 0x88) = 0;
		F<uint16_t>(h, 0x86) = 0;
		F<uint16_t>(h, 0x60) = 0;
		F<uint16_t>(h, 0x58) = 0;
		F<uint16_t>(h, 0x50) = 0;
		F<uint16_t>(h, 0x48) = 0;
		Mark *k = m->marks;
		for (int32_t n = 0x80; n != 0; n--, k++)
			if (k->life != 0) MarkDraw(h, k);
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// Sprites (0x677550 A, 0x6775F0 B)
	// ------------------------------------------------------------------
	static void SpriteDraw(const int16_t *pos, int16_t frame, uint32_t seq, int32_t size)
	{
		TransformCameraByShadowRotation(pos, size, -0x400);
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = seq;
		*(int16_t *)(h + 4) = frame;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t SpriteStep(SpriteNode *s, int16_t life)
	{
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
		s->counter++;
		return s->counter < life ? 0 : TASK_END;
	}

	static uint32_t __cdecl SpriteTaskA(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag290_trauma_shatter_held.inc
		FX_HELD(held_note_sprite(s, SPRITE_A);)
		FX_PART(PART_SPRITES);
		SpriteDraw(s->pos, s->counter, fxp::sequence(PG().seqs.a, SEQ_A), PG().sprites.size_a);
		return SpriteStep(s, PG().sprites.life_a);
	}

	static uint32_t __cdecl SpriteTaskB(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		// 30 fps layer: see mag290_trauma_shatter_held.inc
		FX_HELD(held_note_sprite(s, SPRITE_B);)
		FX_PART(PART_SPRITES);
		SpriteDraw(s->pos, s->counter, fxp::sequence(PG().seqs.b, SEQ_B), PG().sprites.size_b);
		return SpriteStep(s, PG().sprites.life_b);
	}

	// ------------------------------------------------------------------
	// Layouts (0x676340 spawn, 0x676400 task)
	// ------------------------------------------------------------------
	static uint32_t __cdecl LayoutTask(TaskNode *n)
	{
		LayoutNode *L = (LayoutNode *)n;
		PrimArg arg;
		arg.morph = TexBase() + MORPH;
		arg.m.m[0][0] = 0x1000;
		arg.m.m[1][1] = 0x1000;
		arg.m.m[2][2] = 0x1000;
		arg.m.t[0] = L->pos[0];
		arg.m.t[1] = L->pos[1];
		arg.m.m[2][1] = 0;
		arg.m.m[2][0] = 0;
		arg.m.m[1][2] = 0;
		arg.m.m[1][0] = 0;
		arg.m.m[0][2] = 0;
		arg.m.m[0][1] = 0;
		arg.m.t[2] = L->pos[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.scale[2] = L->scale;
		arg.scale[1] = L->scale;
		arg.scale[0] = L->scale;
		arg.scaled = 1;
		arg.depth = -0x80;
		// 30 fps layer: see mag290_trauma_shatter_held.inc
		FX_HELD(held_note_layout(L, &arg);)
		FX_PART(PART_BURSTS);
		return prim::play((prim::Layout *)L->layout, PrimCallback, (int)&arg, 0) ? 0 : TASK_END;
	}

	// sub_676340: script op 0x10A0 (bone, flag, -, scale): a layout task at the bone's spawn position (the entity posed with the
	// pose entries' rotations) + the bone's pose translation
	static const int16_t *SpawnLayout(const int16_t *ip, uint8_t *entity, PoseEntry *pose)
	{
		const int32_t bone = ip[0];
		const int32_t flag = ip[1];
		const int16_t scale = ip[3];
		ip += 4;
		LayoutNode *L = (LayoutNode *)AddTaskToQueue(QLayout(), own::Fn(ORIG_LayoutTask));
		if (!L) return ip;
		uint8_t *anim = entity + 0x60;
		PoseBones(anim, pose);
		GetEffectSpawnPosition(entity, bone, flag, L->pos);
		const PoseEntry &pe = pose[bone];
		L->pos[0] = (int16_t)(L->pos[0] + pe.pos[0]);
		L->pos[1] = (int16_t)(L->pos[1] + pe.pos[1]);
		L->pos[2] = (int16_t)(L->pos[2] + pe.pos[2]);
		L->scale = scale;
		BuildBoneMatricesFromPose(anim);
		DecodeModelPrimLayout(fxp::layout(PG().layouts.a, MODEL_LayoutA, 0x110), L->layout, 0x110);
		return ip;
	}

	// ------------------------------------------------------------------
	// Bone script (0x6756A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ScriptTask(TaskNode *n)
	{
		ScriptNode *s = (ScriptNode *)n;
		const int16_t *ip = s->ip;
		PoseEntry *e = s->cur;
		for (;;)
		{
			const int32_t op = *ip++;
			switch (op)
			{
			case 0x1000: e->pos[0] = ip[0]; e->pos[1] = ip[1]; e->pos[2] = ip[2]; ip += 3; break;
			case 0x1001:
				e->pos[0] = (int16_t)(e->pos[0] + ip[0]);
				e->pos[1] = (int16_t)(e->pos[1] + ip[1]);
				e->pos[2] = (int16_t)(e->pos[2] + ip[2]);
				ip += 3;
				break;
			case 0x1002: s->vel[0] = ip[0]; s->vel[1] = ip[1]; s->vel[2] = ip[2]; ip += 3; break;
			case 0x1003:
				s->vel[0] = (int16_t)(s->vel[0] + ip[0]);
				s->vel[1] = (int16_t)(s->vel[1] + ip[1]);
				s->vel[2] = (int16_t)(s->vel[2] + ip[2]);
				ip += 3;
				break;
			case 0x1004:
				e->pos[0] = (int16_t)(e->pos[0] + s->vel[0]);
				e->pos[1] = (int16_t)(e->pos[1] + s->vel[1]);
				e->pos[2] = (int16_t)(e->pos[2] + s->vel[2]);
				if (e->pos[1] > 0)
				{
					e->pos[1] = 0;
					s->vel[1] = (int16_t)((int16_t)(s->vel[1] >> 2) - s->vel[1]);
				}
				break;
			case 0x1010: e->rot[0] = ip[0]; e->rot[1] = ip[1]; e->rot[2] = ip[2]; ip += 3; break;
			case 0x1011:
				e->rot[0] = (int16_t)(e->rot[0] + ip[0]);
				e->rot[1] = (int16_t)(e->rot[1] + ip[1]);
				e->rot[2] = (int16_t)(e->rot[2] + ip[2]);
				ip += 3;
				break;
			case 0x1012: s->avel[0] = ip[0]; s->avel[1] = ip[1]; s->avel[2] = ip[2]; ip += 3; break;
			case 0x1013:
				s->avel[0] = (int16_t)(s->avel[0] + ip[0]);
				s->avel[1] = (int16_t)(s->avel[1] + ip[1]);
				s->avel[2] = (int16_t)(s->avel[2] + ip[2]);
				ip += 3;
				break;
			case 0x1014:
				e->rot[0] = (int16_t)(e->rot[0] + s->avel[0]);
				e->rot[1] = (int16_t)(e->rot[1] + s->avel[1]);
				e->rot[2] = (int16_t)(e->rot[2] + s->avel[2]);
				break;
			case 0x1080:
				*e = *(const PoseEntry *)(TexBase() + POSE + shl32(ip[0], 4));
				ip++;
				break;
			case 0x1081:
			{
				const uint32_t *src = (const uint32_t *)(TexBase() + POSE + shl32(ip[0], 4));
				ip++;
				((uint32_t *)e)[0] = src[0];
				((uint32_t *)e)[1] = src[1];
				break;
			}
			case 0x1082:
			{
				const uint32_t *src = (const uint32_t *)(TexBase() + POSE + 8 + shl32(ip[0], 4));
				ip++;
				((uint32_t *)e)[2] = src[0];
				((uint32_t *)e)[3] = src[1];
				break;
			}
			case 0x1090: ip = Shatter(ip, Caster(), (PoseEntry *)(TexBase() + POSE)); break;
			case 0x10A0: ip = SpawnLayout(ip, Caster(), (PoseEntry *)(TexBase() + POSE)); break;
			case 0x10F0:
			{
				int16_t *c = (int16_t *)ip;
				if (*c != 0)
				{
					*c = (int16_t)(*c - 1);
					ip++;
					ip = ip + *ip - 2;
				}
				else ip += 2;
				break;
			}
			case 0x1100: return TASK_END;
			default:
				s->ip = ip;
				return 0;
			}
		}
	}

	// sub_675650: a bone script on the pose entry of its first word
	static void StartScript(uint32_t script)
	{
		ScriptNode *s = (ScriptNode *)AddTaskToQueue(QScript(), own::Fn(ORIG_ScriptTask));
		Memset32(&s->cur, 0, 6);
		const int16_t *p = (const int16_t *)(fxp::data(PG().tables.scripts, SCRIPT_TABLE, SCRIPT_TABLE_SIZE, 4) + (script - SCRIPT_TABLE));
		s->ip = p + 1;
		s->cur = (PoseEntry *)(TexBase() + POSE + shl32(p[0], 4));
	}

	// ------------------------------------------------------------------
	// Master (0x6767B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		PrimArg arg;
		arg.morph = TexBase() + MORPH;
		arg.scaled = 0;
		const ShatterParams &g = PG();
		if (m->counter >= g.timing.end) return TASK_END;
		int16_t pos[4] = {}; // the 4th word: never written by 0x502170 (stack residue in the game: GTE VZ0 pad only)
		if (m->counter < g.under.ticks)
		{
			GetEffectSpawnPosition(Caster(), 0, 0, pos);
			RotY(0, &arg.m);
			arg.m.t[0] = pos[0];
			arg.m.t[1] = pos[1] - g.under.drop;
			arg.m.t[2] = pos[2];
			// 30 fps layer: see mag290_trauma_shatter_held.inc
			FX_HELD(held_note_frame(&arg.m);)
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			arg.depth = g.under.depth;
			// 30 fps layer: see mag290_trauma_shatter_held.inc
			FX_HELD(held_note_play(m, PLAY_A, &arg);)
			FX_PART(PART_UNDER);
			prim::play((prim::Layout *)m->layoutA, PrimCallback, (int)&arg, 0);
			// 30 fps layer: see mag290_trauma_shatter_held.inc
			FX_HELD(held_note_under(m, pos);)
			SpriteDraw(pos, m->counter, fxp::sequence(g.seqs.under, SEQ_Under), g.under.size);
		}
		if ((uint32_t)(m->counter - g.rings.start) < (uint32_t)g.rings.ticks)
		{
			GetEffectSpawnPosition(Caster(), g.rings.bone, g.rings.frac, pos);
			RotY(g.rings.turn, &arg.m);
			arg.m.t[0] = pos[0];
			arg.m.t[1] = 0;
			arg.m.t[2] = pos[2];
			// 30 fps layer: see mag290_trauma_shatter_held.inc
			FX_HELD(held_note_frame(&arg.m);)
			ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
			arg.depth = g.rings.depth;
			// 30 fps layer: see mag290_trauma_shatter_held.inc
			FX_HELD(held_note_play(m, PLAY_B, &arg);)
			FX_PART(PART_RINGS);
			prim::play((prim::Layout *)m->layoutB, PrimCallback, (int)&arg, 0);
			// 30 fps layer: see mag290_trauma_shatter_held.inc
			FX_HELD(held_note_play(m, PLAY_C, &arg);)
			prim::play((prim::Layout *)m->layoutC, PrimCallback, (int)&arg, 0);
		}
		if (m->counter < g.tint.ticks)
		{
			const int32_t w = shl32(m->counter, 12) / g.tint.ticks;
			BlendColour(&m->colour, (const void *)own::R(COLOUR_Target), 0x1000 - w, w, Caster() + 0x2C);
			Caster()[0x2F] = (uint8_t)g.tint.alpha;
		}
		if (m->counter >= 0)
		{
			// 30 fps layer: see mag290_trauma_shatter_held.inc
			FX_HELD(held_note_body(m);)
			FX_PART(PART_BODY);
			CasterDraw(Caster(), m->pose);
			*(uint8_t *)Caster() |= 4;
		}
		if (m->counter >= g.tint.hide_from) *(uint8_t *)Caster() |= 8;
		DrawFragments(m);
		DrawMarks(m);
		if (m->counter == g.timing.start) BdPlaySE((const void *)fxp::sound(g.sounds.start, (uint32_t)SOUND_Start), 0, 0x80);
		else if (m->counter == g.timing.release) ReleaseVoiceSlot(m->voice);
		if (m->counter < g.sprites.ticks)
		{
			for (int32_t kb = g.sprites.count_b; kb > 0; kb--)
			{
			SpriteNode *b = (SpriteNode *)AddTaskToQueue(QSpriteB(), own::Fn(ORIG_SpriteTaskB));
			GetEffectSpawnPosition(Caster(), 0, 0, pos);
			if (b)
			{
				*(uint32_t *)b->pos = *(const uint32_t *)pos;
				*(uint32_t *)&b->pos[2] = *(const uint32_t *)&pos[2];
				b->counter = 0;
				b->vel[0] = (int8_t)((CrtRand() & 0xF) - 0x10);
				b->vel[1] = (int8_t)(0xE0 - (CrtRand() & 0xF));
				b->vel[2] = (int8_t)(0xF0 - (CrtRand() & 0xF));
			}
			}
			if (g.sprites.count_b <= 0) GetEffectSpawnPosition(Caster(), 0, 0, pos);
			for (int32_t k = g.sprites.count_a; k > 0; k--)
			{
				SpriteNode *a = (SpriteNode *)AddTaskToQueue(QSpriteA(), own::Fn(ORIG_SpriteTaskA));
				if (!a) continue;
				a->pos[0] = (int16_t)((CrtRand() & 0x1FF) + pos[0] - g.sprites.spread);
				a->pos[1] = (int16_t)((CrtRand() & 0x1FF) + pos[1] - g.sprites.spread);
				const int32_t rz = CrtRand() & 0x1FF;
				a->counter = 0;
				a->pos[2] = (int16_t)(rz + pos[2] - g.sprites.spread);
				a->vel[0] = (int8_t)((CrtRand() & 0xF) - 0x10);
				a->vel[1] = (int8_t)(0xC1 - (CrtRand() & 0x3F));
				a->vel[2] = (int8_t)(0xF0 - (CrtRand() & 0xF));
			}
		}
		MoveFragments(m);
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root (0x675340)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *rn = (RootNode *)n;
		// 30 fps layer: see mag290_trauma_shatter_held.inc
		FX_HELD(held_note_root();)
		if (rn->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			rn->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			rn->arena = 1;
		}
		if (rn->counter == 1 && !rn->started)
		{
			rn->started = 1;
			MusicVolume(0xF0);
			InitTaskQueuePool(QMaster(), (void *)(TexBase() + POOL_Master), 0x68E4, 1);
			InitTaskQueuePool(QLayout(), (void *)(TexBase() + POOL_Layout), 0x124, 3);
			InitTaskQueuePool(QSpriteB(), (void *)(TexBase() + POOL_SpriteB), 0x128, 0x10);
			InitTaskQueuePool(QSpriteA(), (void *)(TexBase() + POOL_SpriteA), 0x18, 0x20);
			InitTaskQueuePool(QScript(), (void *)TexBase(), 0x24, 0x1C);
			MasterNode *m = (MasterNode *)AddTaskToQueue(QMaster(), own::Fn(ORIG_MasterTask));
			Memset32(&m->counter, 0, 0x1A36);
			m->voice = ClaimVoiceSlot((const void *)fxp::sound(PG().sounds.voice, (uint32_t)SOUND_Voice), 1, 0x80);
			m->colour = *(const uint32_t *)(Caster() + 0x2C);
			m->next_frag = m->frags;
			m->next_mark = m->marks;
			DecodeModelPrimLayout(fxp::layout(PG().layouts.a, MODEL_LayoutA, 0x110), m->layoutA, 0x110);
			DecodeModelPrimLayout(fxp::layout(PG().layouts.b, MODEL_LayoutB, 0x22C), m->layoutB, 0x22C);
			DecodeModelPrimLayout(fxp::layout(PG().layouts.c, MODEL_LayoutC, 0xC8), m->layoutC, 0xC8);
			for (int i = 0; SCRIPTS[i]; i++) StartScript(SCRIPTS[i]);
			*(uint16_t *)Caster() &= 0xFFF7;
			StreamStateInit((void *)(TexBase() + STREAM_State));
		}
		int a0;
		if (rn->started)
		{
			a0 = ExecuteTaskQueue(QMaster());
			ExecuteTaskQueue(QScript());
			ExecuteTaskQueue(QLayout());
			ExecuteTaskQueue(QSpriteB());
			ExecuteTaskQueue(QSpriteA());
		}
		else a0 = (int)n;
		if (rn->counter == 0x50) StreamLoad(STREAM_File, TexBase() + STREAM_Dest, 0);
		if (rn->started)
		{
			StreamPump();
			if (a0 == 0)
			{
				if (!StreamDone(0)) return 0;
				*(uint16_t *)Caster() &= 0xFFF7;
				return TASK_END;
			}
		}
		rn->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// File loader (0x675290) and setup (0x675280 -> _Init 0x6752B0): the game runs its own; Fujin's programs these (its
	// memory may be a copy's): the TIM, the magic buffer, the context, the caster (its word +0 bits 2 / 3: only 3 set), the
	// root, the camera
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(PG().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(TexFile());
		TexBase() = (uint32_t)layouts::MagicBuffer();
		var<CastContext *>(own::R(0x2507B3C)) = ctx;
		Caster() = Entity(ctx->attacker);
		*(uint16_t *)Caster() &= 0xFFF3;
		*(uint16_t *)Caster() |= 8;
		TaskQueue *root = (TaskQueue *)own::R(0x2507B98);
		InitTaskQueuePool(root, (void *)own::R(0x2507BA8), 0x14, 1);
		Memset32((uint8_t *)AddTaskToQueue(root, own::Fn(ORIG_RootTask)) + 0xC, 0, 2);
		PlayCameraAnimation((const void *)fxp::camera(PG().res.camera, CAMERA_ANIM));
		return root;
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "master", "Master", "The master's ticks, the start sound and the voice (claimed at the start, released near the end); the texture and the camera" },
			{ 0, "scripts", "Bone scripts", "The 20 bone scripts: each moves one of the caster's 28 pose entries (offsets, turns, their speeds, a bounce on the ground), shatters bone groups into fragments (bones, count, speed, size, chance), spawns a bone layout, loops" },
			{ PART_UNDER, "under", "Under", "Layout A and a flipbook under the caster on the first ticks" },
			{ PART_RINGS, "rings", "Rings", "Layouts B and C on the ground at the caster" },
			{ PART_BODY, "body", "Body", "The caster drawn by the module with the pose entries (his shadow, then the bone groups not hidden), his colour blended to black" },
			{ PART_FRAGMENTS, "fragments", "Fragments", "The shattered polygons: fragments of up to 8 of them on a small mesh, flying off along their normal, spinning, falling, bouncing, fading out" },
			{ PART_MARKS, "marks", "Marks", "A flipbook where each fragment started, drifting the same way" },
			{ PART_SPRITES, "sprites", "Sprites", "Flipbook sprites rising from the caster on the first ticks" },
			{ PART_BURSTS, "bursts", "Bone layouts", "Layout A at a bone, spawned by the bone scripts" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing", "Ticks", "The master's ticks" },
		};
#define MPF(member) (uint32_t)offsetof(ShatterParams, member)
		static const Field FIELDS[] = {
			{ "master", "sounds.start", "Sound", "At the start sound's tick", MPF(sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "sounds.voice", "Voice", "The sound block its voice slot is claimed with (at the start)", MPF(sounds.voice), fxp::Sound, 0, 0x7FFFFFFF },
			{ "master", "timing.start", "Sound", "The master's tick of the start sound", MPF(timing.start), fxp::I16, 0, 400 },
			{ "master", "timing.release", "Voice released", "Its tick the voice slot is released", MPF(timing.release), fxp::I16, 0, 400 },
			{ "master", "timing.end", "End", "Its last tick (the effect ends once the battle file it streams is in)", MPF(timing.end), fxp::I16, 0, 400 },
			{ "master", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", MPF(res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "master", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", MPF(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "scripts", "tables.scripts", "Bone scripts", "The 20 bone scripts' byte code (0x24C bytes, each script at its own offset: edit their numbers, keep their shape)", MPF(tables.scripts), fxp::Data, 0, 0x7FFFFFFF },
			{ "under", "under.ticks", "Ticks", "On the master's first ticks (the flipbook's frame = the tick: 16 at most)", MPF(under.ticks), fxp::I16, 0, 16 },
			{ "under", "under.drop", "Drop", "So far below the caster's spawn point", MPF(under.drop), fxp::I16, -0x4000, 0x4000 },
			{ "under", "under.depth", "Depth", "The layout's depth offset", MPF(under.depth), fxp::I16, -0x1000, 0x1000 },
			{ "under", "under.size", "Sprite size", "The flipbook's size", MPF(under.size), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "under", "layouts.a", "Layout A", "Layout A (the bone scripts' layout too)", MPF(layouts.a), fxp::Layout, 0, 0x7FFFFFFF },
			{ "under", "seqs.under", "Flipbook", "The flipbook under the caster", MPF(seqs.under), fxp::Seq, 0, 0x7FFFFFFF },
			{ "rings", "rings.start", "Start", "The master's tick they start", MPF(rings.start), fxp::I16, 0, 400 },
			{ "rings", "rings.ticks", "Ticks", "Their ticks", MPF(rings.ticks), fxp::I16, 0, 400 },
			{ "rings", "rings.bone", "Bone", "At the caster's spawn point of this bone (on the ground)", MPF(rings.bone), fxp::I16, 0, 0xFF },
			{ "rings", "rings.frac", "Fraction", "The spawn point's fraction", MPF(rings.frac), fxp::I16, 0, 0x1000 },
			{ "rings", "rings.turn", "Turn", "Their turn", MPF(rings.turn), fxp::I16, -0x1000, 0x1000, fxp::Angle },
			{ "rings", "rings.depth", "Depth", "Their depth offset", MPF(rings.depth), fxp::I16, -0x1000, 0x1000 },
			{ "rings", "layouts.b", "Layout B", "Layout B", MPF(layouts.b), fxp::Layout, 0, 0x7FFFFFFF },
			{ "rings", "layouts.c", "Layout C", "Layout C", MPF(layouts.c), fxp::Layout, 0, 0x7FFFFFFF },
			{ "body", "tint.ticks", "Tint ticks", "His colour blended to black over the first ticks (at least 1)", MPF(tint.ticks), fxp::I16, 1, 400 },
			{ "body", "tint.alpha", "Tint byte", "His colour's 4th byte set meanwhile", MPF(tint.alpha), fxp::I16, 0, 255 },
			{ "body", "tint.hide_from", "Hidden from", "From this tick his own draw is off (bit 3)", MPF(tint.hide_from), fxp::I16, 0, 400 },
			{ "sprites", "sprites.ticks", "Ticks", "On the master's first ticks", MPF(sprites.ticks), fxp::I16, 0, 400 },
			{ "sprites", "sprites.count_a", "A count", "Sprites A a tick", MPF(sprites.count_a), fxp::I16, 0, 8 },
			{ "sprites", "sprites.spread", "A scatter", "Around the spawn point: a random below 0x200 - this on each axis", MPF(sprites.spread), fxp::I16, -0x1000, 0x1000 },
			{ "sprites", "sprites.size_a", "A size", "Their size", MPF(sprites.size_a), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "sprites", "sprites.life_a", "A life", "Their ticks (their flipbook's frames: 12 at most)", MPF(sprites.life_a), fxp::I16, 1, 12 },
			{ "sprites", "sprites.count_b", "B count", "Sprites B a tick", MPF(sprites.count_b), fxp::I16, 0, 8 },
			{ "sprites", "sprites.size_b", "B size", "Their size", MPF(sprites.size_b), fxp::I16, 0, 0x7FFF, fxp::Scale },
			{ "sprites", "sprites.life_b", "B life", "Their ticks (their flipbook's frames: 13 at most)", MPF(sprites.life_b), fxp::I16, 1, 13 },
			{ "sprites", "seqs.a", "A flipbook", "Sprites A's flipbook", MPF(seqs.a), fxp::Seq, 0, 0x7FFFFFFF },
			{ "sprites", "seqs.b", "B flipbook", "Sprites B's flipbook", MPF(seqs.b), fxp::Seq, 0, 0x7FFFFFFF },
			{ "fragments", "fragments.life", "Life", "A fragment's ticks: this + a random below 16", MPF(fragments.life), fxp::I16, 1, 400 },
			{ "fragments", "fragments.gravity", "Gravity", "Its fall speed grows by this a tick (it bounces on the ground at half its speed)", MPF(fragments.gravity), fxp::I16, -0x400, 0x400 },
			{ "fragments", "fragments.fade", "Fade", "It fades out over its last ticks (at least 1)", MPF(fragments.fade), fxp::I16, 1, 400 },
			{ "fragments", "fragments.spin", "Spin", "Its spin on each axis: a random below 0x400 - this a tick", MPF(fragments.spin), fxp::I16, -0x800, 0x800, fxp::Angle },
			{ "fragments", "models.frag[0]", "Mesh 1", "A fragment's mesh (one of four at random)", MPF(models.frag[0]), fxp::Model, 0, 0x7FFFFFFF },
			{ "fragments", "models.frag[1]", "Mesh 2", "A fragment's mesh", MPF(models.frag[1]), fxp::Model, 0, 0x7FFFFFFF },
			{ "fragments", "models.frag[2]", "Mesh 3", "A fragment's mesh", MPF(models.frag[2]), fxp::Model, 0, 0x7FFFFFFF },
			{ "fragments", "models.frag[3]", "Mesh 4", "A fragment's mesh", MPF(models.frag[3]), fxp::Model, 0, 0x7FFFFFFF },
			{ "marks", "marks.life", "Life", "A mark's ticks (its flipbook's frame 12 - its life left: its 12 frames at most)", MPF(marks.life), fxp::I16, 1, 12 },
			{ "marks", "marks.speed", "Speed", "It moves by its velocity bytes (its fragment's / 4) x this a tick", MPF(marks.speed), fxp::I16, -64, 64 },
			{ "marks", "seqs.mark", "Flipbook", "The marks' flipbook", MPF(seqs.mark), fxp::Seq, 0, 0x7FFFFFFF },
		};
#undef MPF
	}
}

	void register_mag290_trauma_shatter()
	{
		fxp::register_params({ 290, "Trauma shatter", &shatter290::live, &shatter290::GAME, sizeof(shatter290::ShatterParams),
		                       shatter290::table::PARTS, (int)(sizeof(shatter290::table::PARTS) / sizeof(shatter290::table::PARTS[0])),
		                       shatter290::table::FIELDS, (int)(sizeof(shatter290::table::FIELDS) / sizeof(shatter290::table::FIELDS[0])),
		                       shatter290::table::GROUPS, (int)(sizeof(shatter290::table::GROUPS) / sizeof(shatter290::table::GROUPS[0])) });
		own::note(shatter290::ORIG_FileLoader, (void *)shatter290::FileLoader);
		own::note(shatter290::ORIG_Setup, (void *)shatter290::Setup);
		own::note(shatter290::ORIG_RootTask, (void *)shatter290::RootTask);
		register_port(shatter290::ORIG_RootTask, (void *)shatter290::RootTask, "290 RootTask", 290);
		own::note(shatter290::ORIG_MasterTask, (void *)shatter290::MasterTask);
		register_port(shatter290::ORIG_MasterTask, (void *)shatter290::MasterTask, "290 MasterTask", 290);
		own::note(shatter290::ORIG_ScriptTask, (void *)shatter290::ScriptTask);
		register_port(shatter290::ORIG_ScriptTask, (void *)shatter290::ScriptTask, "290 ScriptTask", 290);
		own::note(shatter290::ORIG_LayoutTask, (void *)shatter290::LayoutTask);
		register_port(shatter290::ORIG_LayoutTask, (void *)shatter290::LayoutTask, "290 LayoutTask", 290);
		own::note(shatter290::ORIG_SpriteTaskA, (void *)shatter290::SpriteTaskA);
		register_port(shatter290::ORIG_SpriteTaskA, (void *)shatter290::SpriteTaskA, "290 SpriteTaskA", 290);
		own::note(shatter290::ORIG_SpriteTaskB, (void *)shatter290::SpriteTaskB);
		register_port(shatter290::ORIG_SpriteTaskB, (void *)shatter290::SpriteTaskB, "290 SpriteTaskB", 290);
		// 30 fps layer: see mag290_trauma_shatter_held.inc
		FX_HELD(register_mag290_held();)
	}
	FX_LAB_MODULE(register_mag290_trauma_shatter);
}

#ifdef FF8_FX_HELD
#include "mag290_trauma_shatter_held.inc"
#endif
