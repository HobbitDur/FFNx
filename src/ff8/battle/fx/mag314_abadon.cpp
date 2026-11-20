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

// Effect 314: the caster's upper body breaking into falling bones and shards (enemy attack 191 of kernel.bin, no name;
// Abadon c0m103, whose second entity slot holds its lower half; no other kernel.bin entry uses effect 314; MAG_314_*).
//
// Structure (setup 0x662890 -> _Init 0x6628C0, file loader 0x6628A0 = mag313.tim (name at 0x101AF1C) into 0x2505578; _Init
// queues that TIM, keeps the context (0x250557C) and the CASTER's entity (0x2505580), clears entity flags 0x0C on the
// caster and the next entity slot (its lower half), sets up the root queue 0x25055A8 (pool 0x25055B8, 1 x 0x14), hides the
// three party entities' normal draw (flag 4) and starts camera animation 0x1016090):
//   RootTask (0x662970) - alternates the module packet arena (magic buffer + 0x56F0 / + 0x156F0); on counter 1 (once): music
//     volume fade 0x47E3C0(0xF0), the main pool (1 x 0x4C28 at + 0xA80), the bone-script pool (0x38 x 0x30 at the magic
//     buffer), the main task (voice slot 0x10161A0, both halves' colour words +0x2C and the lower half's +0x28 kept, the
//     particle / shard cursors), the stream state (+ 0x56A8) and 64 random shard spin matrices (CRT rand: angles +-0x100,
//     scale 0xF00 - 0..0x1FF). Until counter 0xE4: the main queue, the caster's bones from its animation pose plus the
//     main's pose offsets (0x662C00), the bone scripts, the caster's bones from its pose (0x508C90); the screen flash level;
//     on 0xE4 two stream loads (0x168, 0x169) into + 0x56F0, the stream pump every tick from counter 1; after 0xE4 it ends
//     when the streams are done (0x5342C0).
//   MainTask (0x662F40) - counters 0..0xE1: flash ramps 0..7 / 0xDB..0xE1, 1000 between; counters 0x10..0x41: sweep A
//     (316's code on this module's cursor: level 0x200 - 84 a tick); 0x51..0x60: both halves' colour words lerped toward
//     0x25055D8 (0, never written), byte +0x2F = 0x32; 0x42: 29 bone scripts (0x101AA50..); from 0x42 the caster drawn with
//     the pose offsets (0x663850); white tiles 0x659A10 on 0..4, 0xF..0x13, 0x13..0x17; the particles (0x665160, draw THEN
//     move), the shards (0x664020 draw, 0x6642F0 move / spin / age); sound 0x101619C at 1, voice released at 0xDE;
//     counters 0..8 flag 8 of both halves off (odd) / on (even); 0..0x10 up to 8 particles at random caster bones (CRT
//     rand), the lower half's +0x28 colour lerped, its byte +0x2B = 2 and flags bit 11; at 0x10 the lower half's flag 4;
//     counters 0x64..0xD2 the shard sweep (0x664420: polygons above a rising level leave the model as spinning shards,
//     module LCG 0x25055D0, some throw a particle); at 0xE2: flags 8 off, colour words back, every polygon shown again
//     on both halves, the party's flag 4 cleared, end.
//   BoneScriptTask (0x664D20) - node 0x30: byte code moving one pose entry (set / add translation, velocity, rotation,
//     angular velocity, step with a ground bounce that throws 16 dust particles once, ground level, hide, a flat
//     full-screen quad 0x6650E0, counted loops 0x10F0, end 0x1100); any other word yields until the next tick.
// Draw-only flags (battle_to_update_flags 0x201) are not tested: everything updates always.
// ENTITY WRITES: the caster (both halves) - flags words +0x00 (bits 2 / 3, lower half bit 11), colour words +0x2C / +0x2F,
// the lower half's +0x28 / +0x2B, the caster's bone matrices every tick, its model's polygon hidden bits (+0x0F bit 7,
// shards; cleared at the end); the party's flags bit 2 (setup / end).
// CAMERA: the setup's camera animation 0x1016090 (engine); no camera word is written by the tasks.
// Module globals: 0x2505578..0x25055E0 (texture file 0x2505578, context 0x250557C, caster 0x2505580, script queue 0x2505588,
// main queue 0x2505598, root queue 0x25055A8, root pool 0x25055B8, packet cursor 0x25055CC, module LCG 0x25055D0, flash level
// 0x25055D4, colour lerp target 0x25055D8, magic buffer base 0x25055DC).
// CODE OF OTHER MODULES: sweep A's polygons 0x661720 (316's range) and the particles' sprite renderer 0x672DB0 (298's range)
// are ported in fx_model_sweep.h; this module's 0x663760 is used by 316 (also there); 0x662C00 is called by Griever's 064 too.
// NEVER-WRITTEN STACK WORDS: none read (0x662C00's second path reads its stack arrays only at entries written for a parent
// bone, which precedes its children in the skeletons).
// FUJIN: Fujin's described Abadon death (effect_314.json) runs this code: its memory through own::R() (fx_port.h; the
// colour target 0x25055D8, never written, too), its spawns through own::Fn(), the file loader (0x6628A0) and the setup
// (0x662890 -> _Init 0x6628C0) ported below; its parameters (AbadonParams: the sounds and the main's ticks, the screen
// flash, the light sweep, the colour fade, the bone scripts' tick, the white tiles, the sparks, the dust, the shards, the
// particles; the particles' 4 flipbooks, the 29 bone scripts' byte code (one table), the texture name and the camera
// track). FX_PART tags its parts.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_model_sweep.h"

namespace ff8fx
{
namespace abadon314
{
	using namespace eng;
	using namespace magc;
	using namespace msweep;

	// --- module globals ---
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x2505578)); }        // the effect's file (its TIM)
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x250557C)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x2505580)); }        // caster entity (setup); lower half at + 0x9C
	inline TaskQueue *QScripts() { return (TaskQueue *)own::R(0x2505588); }        // pool: magic buffer, 0x38 x 0x30
	inline TaskQueue *QMain() { return (TaskQueue *)own::R(0x2505598); }           // pool: + 0xA80, 1 x 0x4C28
	inline TaskQueue *QRoot() { return (TaskQueue *)own::R(0x25055A8); }           // pool 0x25055B8, 1 x 0x14
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x25055CC)); }
	inline uint32_t &Lcg() { return var<uint32_t>(own::R(0x25055D0)); }
	inline uint32_t &Flash() { return var<uint32_t>(own::R(0x25055D4)); }
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x25055DC)); }        // Magic_TextureOFF (magic buffer)

	static const uint32_t ORIG_Setup = 0x662890, ORIG_FileLoader = 0x6628A0;
	static const uint32_t ORIG_RootTask = 0x662970;
	static const uint32_t ORIG_MainTask = 0x662F40;
	static const uint32_t ORIG_ScriptTask = 0x664D20;
	static const uint32_t TEXTURE_NAME = 0x101AF1C;    // "mag313.tim"
	static const uint32_t CAMERA = 0x1016090;
	static const uint32_t SEQUENCES = 0x10168C0;       // particle sprite sequences (by the particle's +0x0F)
	static const uint32_t COLOUR_Target = 0x25055D8;   // colour lerp target (never written: 0)
	static const void *const SOUND_Voice = (const void *)0x10161A0;
	static const void *const SOUND_Start = (const void *)0x101619C;
	static const uint32_t MAIN = 0xA80;                // magic buffer: the main node
	static const uint32_t STREAM_STATE = 0x56A8, STREAM_DST = 0x56F0;
	static const uint32_t ARENA_A = 0x56F0, ARENA_B = 0x156F0;
	static const int16_t PAGE_MOVE = 0xD, CLUT_MOVE = 0x314;
	static const uint32_t SCRIPTS[29] = {
		0x101AA50, 0x101AAB0, 0x101AA7C, 0x101AADC, 0x101AB08, 0x101AB34, 0x101AB64, 0x101AB94, 0x101ABC0, 0x101ABEC,
		0x101AC18, 0x101AC38, 0x101AC64, 0x101AC90, 0x101ACB0, 0x101ACDC, 0x101AD08, 0x101AD28, 0x101AD58, 0x101AD88,
		0x101ADB4, 0x101ADE0, 0x101AE0C, 0x101AE2C, 0x101AE58, 0x101AE84, 0x101AEA4, 0x101AED0, 0x101AEFC };

	static const uint32_t SCRIPT_TABLE = 0x101AA50, SCRIPT_TABLE_SIZE = 0x4CC;   // the 29 bone scripts (0x101AA50..0x101AF1C)

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_SWEEP = 1, PART_BODY = 2, PART_TILES = 3, PART_PARTICLES = 4, PART_SHARDS = 5 };
	struct AbadonParams
	{
		struct { uint32_t start, voice; } sounds;      // the sound, the voice (its slot claimed at the start)
		struct { int16_t sound, release, end; } timing; // the main's ticks: the sound, the voice released, its end
		struct { int16_t ramp, level; } flash;        // the screen flash: up / down over `ramp` ticks, its level between
		// the light sweep: its ticks, the level falling from `top` by range over them; the texture page / CLUT moves (of
		// the swept, the scripted body's and the shards' polygons)
		struct { int16_t start, ticks, top, range, page, clut; } sweep;
		struct { int16_t start, ticks, alpha; } fade;  // both halves' colours faded to black: its ticks, their byte +0x2F
		struct { int16_t start; } scripts;             // the bone scripts started (and the body drawn from then on)
		// the white tiles: three windows of `ticks` ticks, colour, the tile's ramp in / out
		struct { int16_t start[3], ticks, r, g, b, in, out; } tiles;
		// the sparks at random bones on the first ticks: count a tick, their ticks, size (+ a random below 0x800)
		struct { int16_t ticks, count, life, size; } sparks;
		// the dust where a scripted bone first bounces: count, ticks, size (+ a random below 0x800), scatter
		struct { int16_t count, life, size, spread; } dust;
		// the shard sweep: its ticks, the level's base (rising), the velocity mask, life (+ a random below 8), the chance
		// (of 0x8000) a shard throws a particle, the 64 spin matrices' angles (a random below 2 x spin - spin) and scale
		// (this - a random below 0x200)
		struct { int16_t start, ticks, base, spread, life, chance, spin, scale; } shards;
		struct { int16_t shard_life, speed; } particles; // the shards' particles' ticks; every particle's velocity scale
		struct { uint32_t s[4]; } seqs;                 // the particles' flipbooks: dust, shard particles, sparks A / B
		struct { uint32_t scripts; } tables;            // the 29 bone scripts (byte code: their loop counters are written)
		struct { uint32_t texture_name, camera; } res; // its resources (data): texture file name, camera track
	};
	static const AbadonParams GAME = { { 0x101619C, 0x10161A0 }, { 1, 0xDE, 0xE2 }, { 8, 1000 },
		{ 0x10, 0x32, 0x200, 4200, PAGE_MOVE, CLUT_MOVE }, { 0x51, 0x10, 0x32 }, { 0x42 },
		{ { 0, 0xF, 0x13 }, 5, 0xFF, 0x80, 0x80, 1, 4 }, { 0x10, 8, 0xF, 0x200 }, { 0x10, 0xC, 0xC00, 0x400 },
		{ 0x64, 0x6F, 0x400, 0x7F, 4, 0x2000, 0x100, 0xF00 }, { 0xB, 4 },
		{ { 0x10162C4, 0x1016424, 0x1016568, 0x1016714 } }, { SCRIPT_TABLE }, { TEXTURE_NAME, CAMERA } };
	static AbadonParams live = GAME;
	static const AbadonParams &PG() { return live; }

	// engine functions not in fx_port.h / mag_common.h / fx_model_sweep.h
	inline uint32_t MagicTextureOff() { return fn<uint32_t (__cdecl *)()>(0x571B50)(); }                                // Magic_TextureOFF_ToEAX1
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); } // Stream_QueueLoad
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamPending(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); }                    // au_re_BattleFile_preLoad_0
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, void *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, void *)>(0x502170)(entity, bone, flag, out); }
	inline uint32_t FlashTile(int32_t t, int32_t r, int32_t g, int32_t b, int32_t in, int32_t out, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x659A10)(t, r, g, b, in, out, cursor); } // Effect_FlashTileUpDown
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
	inline void Apply2DTransform(const void *a, const void *b, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x508C30)(a, b, out); }   // apply2DTransform
	inline void Apply3DTransform(const void *m, const void *v, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x56C3D0)(m, v, out); } // apply3DTransform
	inline void Matrix3x3Multiply(const void *a, const void *b, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x56C090)(a, b, out); } // Matrix3x3_MultiplyPSX
	inline void GteLoadV1(const void *v) { fn<void (__cdecl *)(const void *)>(0x45DFA0)(v); }                           // sub_45DFA0
	inline void GteLoadV2(const void *v) { fn<void (__cdecl *)(const void *)>(0x45DFC0)(v); }                           // sub_45DFC0
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }                                                // IR = R * V0

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x25055B8)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up, main task started
		uint32_t parity;   // +0x10 packet arena parity
	};
	struct PoseEntry // 16 bytes, one per bone
	{
		int16_t pos[3];    // +0x00 translation added to the bone after the entity matrix
		int16_t vis;       // +0x06 0 shown, 1 shown with the colour key bone | 0x80, >= 2 hidden
		int16_t rot[3];    // +0x08 rotation added to the animation pose (0x662C00)
		int16_t pad;
	};
	struct Particle // 16 bytes
	{
		int16_t pos[3];    // +0x00
		int16_t life;      // +0x06 ticks left (0 = free); frame = frame count - life
		int16_t size;      // +0x08 sprite scale (4096 = as drawn)
		int16_t pad0A;
		int8_t vel[3];     // +0x0C x 4 a tick
		int8_t seq;        // +0x0F sequence (0x10168C0)
	};
	struct Shard // 0x38 bytes
	{
		int16_t pos[3];    // +0x00
		int16_t kind;      // +0x06 0 free, 1 triangle, 2 quad
		int16_t vel[3];    // +0x08
		int16_t life;      // +0x0E
		uint8_t *spin;     // +0x10 its spin matrix (0x14-byte 3x3 of the main node's 64)
		int16_t c[4][3];   // +0x14 corners from pos
		uint32_t uv0;      // +0x2C u0 v0 CLUT
		uint32_t uv1;      // +0x30 u1 v1 page
		uint16_t uv2;      // +0x34
		uint16_t uv3;      // +0x36
	};
	struct MainNode // pool of 1 node of 0x4C28 bytes (magic buffer + 0xA80)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint32_t voice;    // +0x10 voice slot (0x4A29A0)
		uint32_t colour[2]; // +0x14 the halves' colour words +0x2C at the start
		uint32_t tint1;    // +0x1C the lower half's +0x28 at the start
		PoseEntry pose[0x70]; // +0x20
		Particle parts[0x80]; // +0x720
		Particle *next_part;  // +0xF20
		Shard shards[0x100];  // +0xF24
		Shard *next_shard;    // +0x4724
		uint8_t spins[0x40][0x14]; // +0x4728
	};
	struct ScriptNode // pool of 0x38 nodes of 0x30 bytes
	{
		TaskNode hdr;
		uint8_t *entity;   // +0x0C
		PoseEntry *pose;   // +0x10 the main's pose array
		PoseEntry *cur;    // +0x14 the entry this script moves
		const int16_t *ip; // +0x18
		int16_t vel[3];    // +0x1C
		int16_t pad22;
		int16_t avel[3];   // +0x24
		int16_t pad2A;
		int16_t ground;    // +0x2C bounce level (y down)
		uint8_t bounced;   // +0x2E dust thrown
		uint8_t pad2F;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(PoseEntry) == 0x10 && sizeof(Particle) == 0x10 && sizeof(Shard) == 0x38, "314 nodes");
	static_assert(sizeof(MainNode) == 0x4C28 && sizeof(ScriptNode) == 0x30, "314 nodes");

	// Draw headers (Field_Alloc blocks):
	//   caster (0x9C): +0 polygon cursor, +4 vertex buffer, +8 / +0xA triangle / quad counts, +0xC..+0x12 screen rect 0, 0,
	//     0x140, 0xD8 (x 8), +0x14 model colour (entity +0x28), +0x18 / +0x1A page / CLUT moves, +0x1C object mask, +0x20..
	//     +0x3F the polygon's vertices, +0x40 NCLIP, +0x48 OTZ, +0x50 pose array.
	//   shard sweep (0x58): +0 polygon cursor, +4 vertex buffer, +8 / +0xA counts, +0xC model colour, +0x10 object mask,
	//     +0x14 level, +0x16 spread 0x7F, +0x18 / +0x1A page / CLUT moves, +0x1C main node, +0x20 pose array, +0x24..+0x43
	//     the polygon's vertices (bone-space x / y / z + key word), +0x54 the spin matrices.
}
}

#ifdef FF8_FX_HELD
#include "mag314_abadon_held.h"
#endif

namespace ff8fx
{
namespace abadon314
{
	using namespace eng;
	using namespace magc;
	using namespace msweep;

	// ------------------------------------------------------------------
	// Module LCG (0x664B70): seed = (seed * 125 + 14) & 0x7FFF
	// ------------------------------------------------------------------
	static int32_t Rand15()
	{
		Lcg() = (Lcg() * 125 + 0xE) & 0x7FFF;
		return (int32_t)Lcg();
	}

	// ------------------------------------------------------------------
	// Bone matrices from the animation pose plus per-bone rotation offsets (0x662C00, Battle_BuildBoneMatricesWithPoseOffsets;
	// Griever's 064 calls it too): each bone's angles + the pose entry's (+8) -> rotation; a root bone (parent -1) is scaled
	// by the skeleton's scale and placed at its root position x scale >> 8; a child is its parent's matrix x (0, 0, parent
	// length). Skeleton flag +1 bit 0: the per-bone scale path (local rotation x scale chain on the stack)
	// ------------------------------------------------------------------
	static void PoseBones(uint8_t *anim, const PoseEntry *pose)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x60);
		uint8_t *skel = **(uint8_t ***)(anim + 4);
		const uint8_t count = skel[0];
		if (!(skel[1] & 1))
		{
			for (uint32_t i = 0; i < count; i++)
			{
				uint8_t *rec = skel + 0x10 + i * 0x30;
				const int16_t *pr = pose[i].rot;
				F<int16_t>(h, 0x58) = (int16_t)(F<int16_t>(rec, 4) + pr[0]);
				F<int16_t>(h, 0x5A) = (int16_t)(F<int16_t>(rec, 6) + pr[1]);
				F<int16_t>(h, 0x5C) = (int16_t)(F<int16_t>(rec, 8) + pr[2]);
				ComposeZYXRotationMatrix((const int16_t *)(h + 0x58), (Mat4x3 *)h);
				if (F<int16_t>(rec, 0) == -1)
				{
					const int32_t s = F<int16_t>(skel, 2);
					F<int32_t>(h, 0x48) = s;
					F<int32_t>(h, 0x44) = s;
					F<int32_t>(h, 0x40) = s;
					Scale3DMatrix((Mat4x3 *)h, (const int32_t *)(h + 0x40));
					memcpy(rec + 0x10, h, 0x20);
					F<int32_t>(rec, 0x24) = mul32(F<int16_t>(skel, 8), F<int16_t>(skel, 2)) >> 8;
					F<int32_t>(rec, 0x28) = mul32(F<int16_t>(skel, 0xA), F<int16_t>(skel, 2)) >> 8;
					F<int32_t>(rec, 0x2C) = mul32(F<int16_t>(skel, 0xC), F<int16_t>(skel, 2)) >> 8;
				}
				else
				{
					const uint8_t *prec = skel + 0x10 + mul32(F<int16_t>(rec, 0), 0x30);
					F<int32_t>(h, 0x14) = 0;
					F<int32_t>(h, 0x18) = 0;
					F<int32_t>(h, 0x1C) = F<int16_t>(prec, 2);
					ComposeAffineTransform((const Mat4x3 *)(prec + 0x10), (const Mat4x3 *)h, (Mat4x3 *)(rec + 0x10));
				}
			}
		}
		else if (count != 0)
		{
			static uint8_t rots[0x50][0x20];   // the original's stack arrays (0x50 bones): local rotations, local scales
			static int32_t scales[0x50][4];
			for (uint32_t i = 0; i < count; i++)
			{
				uint8_t *rec = skel + 0x10 + i * 0x30;
				const int16_t *pr = pose[i].rot;
				F<int16_t>(h, 0x58) = (int16_t)(F<int16_t>(rec, 4) + pr[0]);
				F<int16_t>(h, 0x5A) = (int16_t)(F<int16_t>(rec, 6) + pr[1]);
				F<int16_t>(h, 0x5C) = (int16_t)(F<int16_t>(rec, 8) + pr[2]);
				ComposeZYXRotationMatrix((const int16_t *)(h + 0x58), (Mat4x3 *)h);
				const int32_t parent = F<int16_t>(rec, 0);
				if (parent == -1)
				{
					const int32_t s = F<int16_t>(skel, 2);
					F<int32_t>(h, 0x48) = s;
					F<int32_t>(h, 0x44) = s;
					F<int32_t>(h, 0x40) = s;
					Apply2DTransform(h, h + 0x40, rots[i]);
					scales[i][0] = shl32(F<int16_t>(rec, 0xA), 2);
					scales[i][1] = shl32(F<int16_t>(rec, 0xC), 2);
					scales[i][2] = shl32(F<int16_t>(rec, 0xE), 2);
					Apply2DTransform(rots[i], scales[i], rec + 0x10);
					F<int32_t>(rec, 0x24) = mul32(F<int16_t>(skel, 8), F<int16_t>(skel, 2)) >> 8;
					F<int32_t>(rec, 0x28) = mul32(F<int16_t>(skel, 0xA), F<int16_t>(skel, 2)) >> 8;
					F<int32_t>(rec, 0x2C) = mul32(F<int16_t>(skel, 0xC), F<int16_t>(skel, 2)) >> 8;
				}
				else
				{
					const uint8_t *prec = skel + 0x10 + mul32(parent, 0x30);
					F<int16_t>(h, 0x52) = 0;
					F<int16_t>(h, 0x50) = 0;
					F<int16_t>(h, 0x54) = F<int16_t>(prec, 2);
					Apply3DTransform(prec + 0x10, h + 0x50, h + 0x40);
					F<int32_t>(rec, 0x24) = F<int32_t>(prec, 0x24) + F<int32_t>(h, 0x40);
					F<int32_t>(rec, 0x28) = F<int32_t>(prec, 0x28) + F<int32_t>(h, 0x44);
					F<int32_t>(rec, 0x2C) = F<int32_t>(prec, 0x2C) + F<int32_t>(h, 0x48);
					Matrix3x3Multiply(rots[parent], h, rots[i]);
					scales[i][0] = mul32(scales[parent][0], F<int16_t>(rec, 0xA)) >> 0xA;
					scales[i][1] = mul32(scales[parent][1], F<int16_t>(rec, 0xC)) >> 0xA;
					scales[i][2] = mul32(scales[parent][2], F<int16_t>(rec, 0xE)) >> 0xA;
					Apply2DTransform(rots[i], scales[i], rec + 0x10);
				}
			}
		}
		FieldFree(0x60);
	}

	// ------------------------------------------------------------------
	// Caster vertices (0x663EA0): one bone group through the current matrix (bone x camera) into 8-byte entries (sxy, sz,
	// clip flags: x outside the screen rect left 1 / right 2, y top 4 / bottom 8, behind the camera 0x10; colour key)
	// ------------------------------------------------------------------
	static void ProjectCaster(const uint8_t **data, uint8_t **out, uint8_t *h, int32_t key)
	{
		const uint8_t *d = *data;
		uint8_t *o = *out;
		const int32_t n = *(const int16_t *)d;
		d += 2;
		for (int32_t k = n; k != 0; k--)
		{
			GteLoadV0u16(d);
			GteRTPS();
			GteReadSXY2(h + 0x20);
			d += 6;
			GteReadOTZ(h + 0x24);
			if (F<int16_t>(h, 0x24) <= 0) o[6] = 0x10;
			else
			{
				h[0x26] = 0;
				h[0x27] = (uint8_t)key;
				const int32_t x = F<int16_t>(h, 0x20);
				if (x < shl32(F<int16_t>(h, 0xC), 3)) h[0x26] = 1;
				else if (x >= shl32(F<int16_t>(h, 0x10), 3)) h[0x26] = 2;
				const int32_t y = F<int16_t>(h, 0x22);
				if (y < shl32(F<int16_t>(h, 0xE), 3)) h[0x26] |= 4;
				else if (y >= shl32(F<int16_t>(h, 0x12), 3)) h[0x26] |= 8;
				*(uint32_t *)o = F<uint32_t>(h, 0x20);
				*(uint32_t *)(o + 4) = F<uint32_t>(h, 0x24);
			}
			o += 8;
		}
		*data = d;
		*out = o;
	}

	// ------------------------------------------------------------------
	// Caster polygons (0x663B40): as 064's 0x6E5960 with the texture page / CLUT words moved by +0x18 / +0x1A
	// ------------------------------------------------------------------
	static uint32_t CasterPolys(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *verts = F<const uint8_t *>(h, 4);
		const uint8_t *p = F<const uint8_t *>(h, 0);
		int16_t c = F<int16_t>(h, 8);
		F<int16_t>(h, 8) = (int16_t)(c - 1);
		while (c != 0)
		{
			if (!(p[0xF] & 0x80))
			{
				const uint32_t i0 = *(const uint16_t *)p & 0xFFF, i1 = *(const uint16_t *)(p + 2) & 0xFFF, i2 = *(const uint16_t *)(p + 4) & 0xFFF;
				F<uint32_t>(h, 0x20) = *(const uint32_t *)(verts + i0 * 8);
				F<uint32_t>(h, 0x24) = *(const uint32_t *)(verts + i0 * 8 + 4);
				F<uint32_t>(h, 0x28) = *(const uint32_t *)(verts + i1 * 8);
				F<uint32_t>(h, 0x2C) = *(const uint32_t *)(verts + i1 * 8 + 4);
				F<uint32_t>(h, 0x30) = *(const uint32_t *)(verts + i2 * 8);
				F<uint32_t>(h, 0x34) = *(const uint32_t *)(verts + i2 * 8 + 4);
				const uint8_t f0 = h[0x26], f1 = h[0x2E], f2 = h[0x36];
				if ((uint8_t)(f0 | f1 | f2) < 0x10 && !(f2 & (uint8_t)(f0 & f1)) && h[0x27] == h[0x2F] && h[0x27] == h[0x37])
				{
					GteSetSXY012(F<uint32_t>(h, 0x20), F<uint32_t>(h, 0x28), F<uint32_t>(h, 0x30));
					GteNCLIP();
					GteReadMAC0(h + 0x40);
					if (F<int32_t>(h, 0x40) >= 0)
					{
						GteSetSZ123(F<int16_t>(h, 0x24), F<int16_t>(h, 0x2C), F<int16_t>(h, 0x34));
						GteAVSZ3();
						F<uint32_t>(pk, 0) = 0x7000000;
						F<uint32_t>(pk, 4) = ((uint32_t)((*(const uint16_t *)(p + 0xE) & 0x200) | 0x2400) << 16) | F<uint32_t>(h, 0x14);
						F<uint32_t>(pk, 8) = F<uint32_t>(h, 0x20);
						F<uint32_t>(pk, 0x10) = F<uint32_t>(h, 0x28);
						F<uint32_t>(pk, 0x18) = F<uint32_t>(h, 0x30);
						F<uint32_t>(pk, 0xC) = *(const uint32_t *)(p + 8);
						F<uint32_t>(pk, 0x14) = *(const uint32_t *)(p + 0xC);
						const uint16_t page = (uint16_t)(F<uint16_t>(pk, 0x16) & 0xFDFF);
						F<uint16_t>(pk, 0x1C) = *(const uint16_t *)(p + 6);
						F<uint16_t>(pk, 0x16) = (uint16_t)(page + F<uint16_t>(h, 0x18));
						F<uint16_t>(pk, 0xE) = (uint16_t)(F<uint16_t>(pk, 0xE) + F<uint16_t>(h, 0x1A));
						GteReadOTZWord(h + 0x48);
						InsertPrimAutoDepth(ot + (uint32_t)(F<int32_t>(h, 0x48) >> (shift & 31)) * 4, pk);
						pk += 0x20;
					}
				}
			}
			p += 0x10;
			c = F<int16_t>(h, 8);
			F<int16_t>(h, 8) = (int16_t)(c - 1);
		}
		c = F<int16_t>(h, 0xA);
		F<int16_t>(h, 0xA) = (int16_t)(c - 1);
		while (c != 0)
		{
			if (!(p[0xF] & 0x80))
			{
				const uint32_t i0 = *(const uint16_t *)p & 0xFFF, i1 = *(const uint16_t *)(p + 2) & 0xFFF;
				const uint32_t i2 = *(const uint16_t *)(p + 4) & 0xFFF, i3 = *(const uint16_t *)(p + 6) & 0xFFF;
				F<uint32_t>(h, 0x20) = *(const uint32_t *)(verts + i0 * 8);
				F<uint32_t>(h, 0x24) = *(const uint32_t *)(verts + i0 * 8 + 4);
				F<uint32_t>(h, 0x28) = *(const uint32_t *)(verts + i1 * 8);
				F<uint32_t>(h, 0x2C) = *(const uint32_t *)(verts + i1 * 8 + 4);
				F<uint32_t>(h, 0x30) = *(const uint32_t *)(verts + i2 * 8);
				F<uint32_t>(h, 0x34) = *(const uint32_t *)(verts + i2 * 8 + 4);
				F<uint32_t>(h, 0x38) = *(const uint32_t *)(verts + i3 * 8);
				F<uint32_t>(h, 0x3C) = *(const uint32_t *)(verts + i3 * 8 + 4);
				const uint8_t f0 = h[0x26], f1 = h[0x2E], f2 = h[0x36], f3 = h[0x3E];
				if ((uint8_t)(f3 | f0 | f1 | f2) < 0x10 && !(f2 & (uint8_t)(f3 & f0 & f1)) && h[0x27] == h[0x2F] && h[0x27] == h[0x37] && h[0x27] == h[0x3F])
				{
					GteSetSXY012(F<uint32_t>(h, 0x20), F<uint32_t>(h, 0x28), F<uint32_t>(h, 0x30));
					GteNCLIP();
					GteReadMAC0(h + 0x40);
					if (F<int32_t>(h, 0x40) >= 0)
					{
						GteSetSZ0123(F<int16_t>(h, 0x24), F<int16_t>(h, 0x2C), F<int16_t>(h, 0x34), F<int16_t>(h, 0x3C));
						GteAVSZ4();
						F<uint32_t>(pk, 0) = 0x9000000;
						F<uint32_t>(pk, 4) = ((uint32_t)((*(const uint16_t *)(p + 0xE) & 0x200) | 0x2C00) << 16) | F<uint32_t>(h, 0x14);
						F<uint32_t>(pk, 8) = F<uint32_t>(h, 0x20);
						F<uint32_t>(pk, 0x10) = F<uint32_t>(h, 0x28);
						F<uint32_t>(pk, 0x18) = F<uint32_t>(h, 0x30);
						F<uint32_t>(pk, 0x20) = F<uint32_t>(h, 0x38);
						F<uint32_t>(pk, 0xC) = *(const uint32_t *)(p + 8);
						F<uint32_t>(pk, 0x14) = *(const uint32_t *)(p + 0xC);
						F<uint16_t>(pk, 0x1C) = *(const uint16_t *)(p + 0x10);
						const uint16_t page = (uint16_t)(F<uint16_t>(pk, 0x16) & 0xFDFF);
						F<uint16_t>(pk, 0x24) = *(const uint16_t *)(p + 0x12);
						F<uint16_t>(pk, 0x16) = (uint16_t)(page + F<uint16_t>(h, 0x18));
						F<uint16_t>(pk, 0xE) = (uint16_t)(F<uint16_t>(pk, 0xE) + F<uint16_t>(h, 0x1A));
						GteReadOTZWord(h + 0x48);
						InsertPrimAutoDepth(ot + (uint32_t)(F<int32_t>(h, 0x48) >> (shift & 31)) * 4, pk);
						pk += 0x28;
					}
				}
			}
			p += 0x14;
			c = F<int16_t>(h, 0xA);
			F<int16_t>(h, 0xA) = (int16_t)(c - 1);
		}
		return (uint32_t)pk;
	}

	// ------------------------------------------------------------------
	// Caster model (0x6639A0): every object in the mask (+0x1C); a bone whose pose entry is hidden (>= 2) marks its
	// vertices clipped, else they are projected (colour key bone | 0x80 when the entry is 1)
	// ------------------------------------------------------------------
	static uint32_t CasterModel(const uint8_t *model, uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t *objs = *(const uint32_t *const *)(model + 4);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const int32_t count = (int32_t)objs[0];
		if (count <= 0) return cursor;
		for (int32_t i = 0; i < count; i++)
		{
			const uint8_t *p = *(const uint8_t *const *)(model + 4) + objs[1 + i];
			if (!(F<uint32_t>(h, 0x1C) & (1u << (i & 31)))) continue;
			uint8_t *out = F<uint8_t *>(h, 4);
			const int32_t groups = *(const int16_t *)p;
			p += 2;
			if (groups > 0)
			{
				for (int32_t g = groups; g != 0; g--)
				{
					const int32_t bone = *(const int16_t *)p;
					p += 2;
					const PoseEntry *pe = F<const PoseEntry *>(h, 0x50) + bone;
					if (pe->vis < 2)
					{
						const Mat4x3 *bm = (const Mat4x3 *)(bones + mul32(bone, 0x30) + 0x10);
						GteSetRotMatrixCtrl(bm);
						GteSetTransVectorCtrl(bm);
						ProjectCaster(&p, &out, h, pe->vis == 1 ? (bone | 0x80) : 0);
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
			F<uint16_t>(h, 8) = *(const uint16_t *)p;
			F<uint16_t>(h, 0xA) = *(const uint16_t *)(p + 2);
			F<const uint8_t *>(h, 0) = p + 0xC;
			cursor = CasterPolys(h, ot, shift, cursor);
		}
		return cursor;
	}

	// ------------------------------------------------------------------
	// Caster draw (0x663850): its shadow, its bones from the pose plus the offsets, through its matrix, plus the offsets'
	// translations, through the camera; its model into the frame arena; its bones from the pose plus the offsets again
	// ------------------------------------------------------------------
	static void CasterDraw(uint8_t *ent, const PoseEntry *pose, int16_t page, int16_t clut)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x9C);
		uint8_t *anim = ent + 0x60;
		FrameArena() = DrawShadow(ent, OT(0x4040), 0x10, FrameArena());
		PoseBones(anim, pose);
		uint8_t *skel = **(uint8_t ***)(anim + 4);
		for (uint32_t i = 0; i < skel[0]; i++)
		{
			Mat4x3 *bm = (Mat4x3 *)(skel + 0x20 + i * 0x30);
			ComposeAffineTransform((const Mat4x3 *)(ent + 0x40), bm, bm);
			bm->t[0] = (int32_t)((uint32_t)bm->t[0] + (uint32_t)(int32_t)pose[i].pos[0]);
			bm->t[1] = (int32_t)((uint32_t)bm->t[1] + (uint32_t)(int32_t)pose[i].pos[1]);
			bm->t[2] = (int32_t)((uint32_t)bm->t[2] + (uint32_t)(int32_t)pose[i].pos[2]);
			ComposeAffineTransform(&Camera(), bm, bm);
		}
		F<int16_t>(h, 0x18) = page;
		F<int16_t>(h, 0x1A) = clut;
		F<const PoseEntry *>(h, 0x50) = pose;
		F<uint32_t>(h, 4) = var<uint32_t>(0x1D98B3C);
		F<int16_t>(h, 0xC) = 0;
		F<int16_t>(h, 0xE) = 0;
		F<int16_t>(h, 0x10) = 0x140;
		F<int16_t>(h, 0x12) = 0xD8;
		F<uint32_t>(h, 0x14) = F<uint32_t>(ent, 0x28);
		F<uint32_t>(h, 0x1C) = F<uint32_t>(ent, 0x7C);
		FrameArena() = CasterModel(F<const uint8_t *>(anim, 4), h, OT(0x44), 0, FrameArena());
		PoseBones(anim, pose);
		FieldFree(0x9C);
	}

	// ------------------------------------------------------------------
	// Particle slot (0x6643D0): the slot the cursor names when free, else the first free one (slots 0..0x7F; none: null);
	// the cursor goes to the slot after it (wrapping)
	// ------------------------------------------------------------------
	static Particle *AllocParticle(MainNode *m)
	{
		Particle *s = m->next_part;
		if (s->life != 0)
		{
			s = &m->parts[0];
			if (s->life != 0)
			{
				int k = 0x80;
				do
				{
					s++;
					if (--k == 0) return nullptr;
				} while (s->life != 0);
			}
		}
		m->next_part = s < &m->parts[0x7F] ? s + 1 : &m->parts[0];
		return s;
	}

	// shard slot (0x664B90): as the particles' over the 0x100 shards
	static Shard *AllocShard(MainNode *m)
	{
		Shard *s = m->next_shard;
		if (s->kind != 0)
		{
			s = &m->shards[0];
			if (s->kind != 0)
			{
				int k = 0x100;
				do
				{
					s++;
					if (--k == 0) return nullptr;
				} while (s->kind != 0);
			}
		}
		m->next_shard = s < &m->shards[0xFF] ? s + 1 : &m->shards[0];
		return s;
	}

	// a shard's particle (0x664BE0): at the shard, sequence 1, life 0xB, rising
	static void ShardParticle(MainNode *m, const Shard *s)
	{
		Particle *p = AllocParticle(m);
		if (!p) return;
		*(uint32_t *)&p->pos[0] = *(const uint32_t *)&s->pos[0];
		*(uint32_t *)&p->pos[2] = *(const uint32_t *)&s->pos[2];
		*(uint32_t *)&p->vel[0] = 0;
		p->life = PG().particles.shard_life;
		p->size = 0x1000;
		p->vel[0] = 0;
		p->vel[1] = (int8_t)-(int8_t)(Rand15() & 7);
		const uint8_t vz = (uint8_t)(0xC0 - (uint8_t)(Rand15() & 0x1F));
		p->seq = 1;
		p->vel[2] = (int8_t)vz;
	}

	// ------------------------------------------------------------------
	// Shard sweep vertices (0x664C40): one bone group through the bone matrix (entity space, no camera) into 8-byte
	// entries (x, y, z, key word); then every entry at or above the level (y <= +0x14) gets key | 1
	// ------------------------------------------------------------------
	static void ProjectShards(const uint8_t **data, uint8_t **out, uint8_t *h, int32_t key)
	{
		const uint8_t *d = *data;
		uint8_t *o = *out;
		const int32_t n = *(const int16_t *)d;
		d += 2;
		for (int32_t k = n; k != 0; k--)
		{
			GteLoadV0u16(d);
			GteMVMVA_RotV0Tr();
			d += 6;
			*(int16_t *)(o + 6) = (int16_t)key;
			o += 8;
			GteStoreIR123Minus8(o);
		}
		uint8_t *e = *out;
		for (int32_t k = n; k != 0; k--)
		{
			if (*(const int16_t *)(e + 2) <= F<int16_t>(h, 0x14)) *(int16_t *)(e + 6) = (int16_t)(key | 1);
			e += 8;
		}
		*data = d;
		*out = e;
	}

	// ------------------------------------------------------------------
	// Shard sweep polygons (0x6646A0): every shown polygon whose vertices are all above the level with one key leaves the
	// model (hidden bit) as a shard at its centre (corners from the centre, its texture words moved, a random spin matrix,
	// a random velocity from the spread +0x16, life 4..0xB); 1 in 4 throws a particle. No free shard: the walk stops
	// ------------------------------------------------------------------
	static void ShardPolys(uint8_t *h)
	{
		uint8_t *p = F<uint8_t *>(h, 0);
		const uint8_t *verts = F<const uint8_t *>(h, 4);
		MainNode *m = F<MainNode *>(h, 0x1C);
		int16_t c = F<int16_t>(h, 8);
		F<int16_t>(h, 8) = (int16_t)(c - 1);
		while (c != 0)
		{
			if (!(p[0xF] & 0x80))
			{
				const uint32_t i0 = *(const uint16_t *)p & 0xFFF, i1 = *(const uint16_t *)(p + 2) & 0xFFF, i2 = *(const uint16_t *)(p + 4) & 0xFFF;
				F<uint32_t>(h, 0x24) = *(const uint32_t *)(verts + i0 * 8);
				F<uint32_t>(h, 0x28) = *(const uint32_t *)(verts + i0 * 8 + 4);
				F<uint32_t>(h, 0x2C) = *(const uint32_t *)(verts + i1 * 8);
				F<uint32_t>(h, 0x30) = *(const uint32_t *)(verts + i1 * 8 + 4);
				F<uint32_t>(h, 0x34) = *(const uint32_t *)(verts + i2 * 8);
				F<uint32_t>(h, 0x38) = *(const uint32_t *)(verts + i2 * 8 + 4);
				if ((h[0x32] & (uint8_t)(h[0x3A] & h[0x2A])) != 0 && F<int16_t>(h, 0x2A) == F<int16_t>(h, 0x32) && F<int16_t>(h, 0x2A) == F<int16_t>(h, 0x3A))
				{
					Shard *s = AllocShard(m);
					if (!s) return;
					const int16_t cx = (int16_t)(((int32_t)F<int16_t>(h, 0x24) + F<int16_t>(h, 0x2C) + F<int16_t>(h, 0x34)) / 3);
					s->pos[0] = cx;
					const int16_t cy = (int16_t)(((int32_t)F<int16_t>(h, 0x26) + F<int16_t>(h, 0x36) + F<int16_t>(h, 0x2E)) / 3);
					s->pos[1] = cy;
					const int16_t cz = (int16_t)(((int32_t)F<int16_t>(h, 0x28) + F<int16_t>(h, 0x38) + F<int16_t>(h, 0x30)) / 3);
					s->kind = 1;
					s->pos[2] = cz;
					for (int k = 0; k < 3; k++)
					{
						s->c[k][0] = (int16_t)(F<int16_t>(h, 0x24 + 8 * k) - cx);
						s->c[k][1] = (int16_t)(F<int16_t>(h, 0x26 + 8 * k) - cy);
						s->c[k][2] = (int16_t)(F<int16_t>(h, 0x28 + 8 * k) - cz);
					}
					const int32_t sp = F<int16_t>(h, 0x16);
					s->vel[0] = (int16_t)(((Rand15() & sp) - (sp >> 1)) >> 2);
					const int32_t r1 = Rand15() & sp;
					s->vel[2] = (int16_t)-(r1 + (int16_t)((int16_t)F<int16_t>(h, 0x16) >> 1));
					s->vel[1] = (int16_t)(-(Rand15() & (int32_t)F<int16_t>(h, 0x16)) >> 1);
					s->life = (int16_t)((Rand15() & 7) + PG().shards.life);
					s->uv0 = *(const uint32_t *)(p + 8);
					s->uv1 = *(const uint32_t *)(p + 0xC);
					s->uv2 = *(const uint16_t *)(p + 6);
					*((uint16_t *)&s->uv1 + 1) = (uint16_t)(*((uint16_t *)&s->uv1 + 1) + F<uint16_t>(h, 0x18));
					*((uint16_t *)&s->uv0 + 1) = (uint16_t)(*((uint16_t *)&s->uv0 + 1) + F<uint16_t>(h, 0x1A));
					s->spin = F<uint8_t *>(h, 0x54) + (Rand15() & 0x3F) * 0x14;
					p[0xF] |= 0x80;
					if (Rand15() < PG().shards.chance) ShardParticle(m, s);
				}
			}
			p += 0x10;
			c = F<int16_t>(h, 8);
			F<int16_t>(h, 8) = (int16_t)(c - 1);
		}
		c = F<int16_t>(h, 0xA);
		F<int16_t>(h, 0xA) = (int16_t)(c - 1);
		while (c != 0)
		{
			if (!(p[0xF] & 0x80))
			{
				const uint32_t i0 = *(const uint16_t *)p & 0xFFF, i1 = *(const uint16_t *)(p + 2) & 0xFFF;
				const uint32_t i2 = *(const uint16_t *)(p + 4) & 0xFFF, i3 = *(const uint16_t *)(p + 6) & 0xFFF;
				F<uint32_t>(h, 0x24) = *(const uint32_t *)(verts + i0 * 8);
				F<uint32_t>(h, 0x28) = *(const uint32_t *)(verts + i0 * 8 + 4);
				F<uint32_t>(h, 0x2C) = *(const uint32_t *)(verts + i1 * 8);
				F<uint32_t>(h, 0x30) = *(const uint32_t *)(verts + i1 * 8 + 4);
				F<uint32_t>(h, 0x34) = *(const uint32_t *)(verts + i2 * 8);
				F<uint32_t>(h, 0x38) = *(const uint32_t *)(verts + i2 * 8 + 4);
				F<uint32_t>(h, 0x3C) = *(const uint32_t *)(verts + i3 * 8);
				F<uint32_t>(h, 0x40) = *(const uint32_t *)(verts + i3 * 8 + 4);
				if ((h[0x32] & (uint8_t)(h[0x3A] & h[0x2A] & h[0x42])) != 0 && F<int16_t>(h, 0x2A) == F<int16_t>(h, 0x32)
					&& F<int16_t>(h, 0x2A) == F<int16_t>(h, 0x3A) && F<int16_t>(h, 0x2A) == F<int16_t>(h, 0x42))
				{
					Shard *s = AllocShard(m);
					if (!s) return;
					const int16_t cx = (int16_t)(((int32_t)F<int16_t>(h, 0x24) + F<int16_t>(h, 0x2C) + F<int16_t>(h, 0x34) + F<int16_t>(h, 0x3C)) / 4);
					s->pos[0] = cx;
					const int16_t cy = (int16_t)(((int32_t)F<int16_t>(h, 0x26) + F<int16_t>(h, 0x3E) + F<int16_t>(h, 0x36) + F<int16_t>(h, 0x2E)) / 4);
					s->pos[1] = cy;
					const int16_t cz = (int16_t)(((int32_t)F<int16_t>(h, 0x28) + F<int16_t>(h, 0x40) + F<int16_t>(h, 0x38) + F<int16_t>(h, 0x30)) / 4);
					s->kind = 2;
					s->pos[2] = cz;
					for (int k = 0; k < 4; k++)
					{
						s->c[k][0] = (int16_t)(F<int16_t>(h, 0x24 + 8 * k) - cx);
						s->c[k][1] = (int16_t)(F<int16_t>(h, 0x26 + 8 * k) - cy);
						s->c[k][2] = (int16_t)(F<int16_t>(h, 0x28 + 8 * k) - cz);
					}
					const int16_t sp = F<int16_t>(h, 0x16);
					s->vel[0] = (int16_t)((Rand15() & (uint16_t)sp) - (int16_t)(sp >> 1));
					s->vel[2] = (int16_t)-(int32_t)((Rand15() & (uint16_t)sp) + (int16_t)(sp >> 1));
					s->vel[1] = (int16_t)-(int16_t)(Rand15() & (uint16_t)F<int16_t>(h, 0x16));
					s->life = (int16_t)((Rand15() & 7) + PG().shards.life);
					s->uv0 = *(const uint32_t *)(p + 8);
					s->uv1 = *(const uint32_t *)(p + 0xC);
					s->uv2 = *(const uint16_t *)(p + 0x10);
					s->uv3 = *(const uint16_t *)(p + 0x12);
					*((uint16_t *)&s->uv1 + 1) = (uint16_t)(*((uint16_t *)&s->uv1 + 1) + F<uint16_t>(h, 0x18));
					*((uint16_t *)&s->uv0 + 1) = (uint16_t)(*((uint16_t *)&s->uv0 + 1) + F<uint16_t>(h, 0x1A));
					s->spin = F<uint8_t *>(h, 0x54) + (Rand15() & 0x3F) * 0x14;
					p[0xF] |= 0x80;
					if (Rand15() < PG().shards.chance) ShardParticle(m, s);
				}
			}
			p += 0x14;
			c = F<int16_t>(h, 0xA);
			F<int16_t>(h, 0xA) = (int16_t)(c - 1);
		}
	}

	// shard sweep model walk (0x664520): as the caster model's with the shard projection (hidden entries: key word 0)
	static void ShardModel(const uint8_t *model, uint8_t *h)
	{
		const uint32_t *objs = *(const uint32_t *const *)(model + 4);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const int32_t count = (int32_t)objs[0];
		for (int32_t i = 0; i < count; i++)
		{
			const uint8_t *p = *(const uint8_t *const *)(model + 4) + objs[1 + i];
			if (!(F<uint32_t>(h, 0x10) & (1u << (i & 31)))) continue;
			uint8_t *out = F<uint8_t *>(h, 4);
			const int32_t groups = *(const int16_t *)p;
			p += 2;
			if (groups > 0)
			{
				for (int32_t g = groups; g != 0; g--)
				{
					const int32_t bone = *(const int16_t *)p;
					p += 2;
					const PoseEntry *pe = F<const PoseEntry *>(h, 0x20) + bone;
					if (pe->vis < 2)
					{
						const Mat4x3 *bm = (const Mat4x3 *)(bones + mul32(bone, 0x30) + 0x10);
						GteSetRotMatrixCtrl(bm);
						GteSetTransVectorCtrl(bm);
						ProjectShards(&p, &out, h, pe->vis == 1 ? shl32(bone, 8) : 0);
					}
					else
					{
						const int32_t n = *(const int16_t *)p;
						p = p + n * 6 + 2;
						for (int32_t k = n; k != 0; k--)
						{
							*(int16_t *)(out + 6) = 0;
							out += 8;
						}
					}
				}
			}
			p = (const uint8_t *)(((uint32_t)p + 3) & ~3u);
			F<uint16_t>(h, 8) = *(const uint16_t *)p;
			F<uint16_t>(h, 0xA) = *(const uint16_t *)(p + 2);
			F<const uint8_t *>(h, 0) = p + 0xC;
			ShardPolys(h);
		}
	}

	// ------------------------------------------------------------------
	// Shard sweep (0x664420): the caster's bones from the pose plus the offsets, through its matrix, plus the offsets'
	// translations; its model's polygons above `level` become shards
	// ------------------------------------------------------------------
	static void ShardSweep(uint8_t *ent, const PoseEntry *pose, uint8_t *spins, int16_t level, int16_t spread, int16_t page, int16_t clut)
	{
		uint8_t *anim = ent + 0x60;
		PoseBones(anim, pose);
		uint8_t *skel = **(uint8_t ***)(anim + 4);
		for (uint32_t i = 0; i < skel[0]; i++)
		{
			Mat4x3 *bm = (Mat4x3 *)(skel + 0x20 + i * 0x30);
			ComposeAffineTransform((const Mat4x3 *)(ent + 0x40), bm, bm);
			bm->t[0] = (int32_t)((uint32_t)bm->t[0] + (uint32_t)(int32_t)pose[i].pos[0]);
			bm->t[1] = (int32_t)((uint32_t)bm->t[1] + (uint32_t)(int32_t)pose[i].pos[1]);
			bm->t[2] = (int32_t)((uint32_t)bm->t[2] + (uint32_t)(int32_t)pose[i].pos[2]);
		}
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		F<int16_t>(h, 0x16) = spread;
		F<uint32_t>(h, 0x1C) = TexBase() + MAIN;
		F<int16_t>(h, 0x14) = level;
		F<uint8_t *>(h, 0x54) = spins;
		F<const PoseEntry *>(h, 0x20) = pose;
		F<int16_t>(h, 0x18) = page;
		F<int16_t>(h, 0x1A) = clut;
		F<uint32_t>(h, 4) = var<uint32_t>(0x1D98B3C);
		F<uint32_t>(h, 0xC) = F<uint32_t>(ent, 0x28);
		F<uint32_t>(h, 0x10) = F<uint32_t>(ent, 0x7C);
		ShardModel(F<const uint8_t *>(anim, 4), h);
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Shards (0x664020): every live shard's corners around its position through the camera; a quad (kind 2) or a
	// triangle, textured grey 0xF0, unless all its corners are off one side of the screen (x 0..0xA00, y 0..0x6C0) or it
	// is behind the camera; OT bucket OTZ / 4 at the frame OT
	// ------------------------------------------------------------------
	static void ShardsDraw(MainNode *m)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		int16_t *v = (int16_t *)FieldAlloc(8);
		for (int k = 0; k < 0x100; k++)
		{
			const Shard *s = &m->shards[k];
			uint8_t *pk = (uint8_t *)PacketCursor();
			if (s->kind == 0) continue;
			for (int a = 0; a < 3; a++) v[a] = (int16_t)(s->c[0][a] + s->pos[a]);
			GteLoadV0(v);
			for (int a = 0; a < 3; a++) v[a] = (int16_t)(s->c[1][a] + s->pos[a]);
			GteLoadV1(v);
			for (int a = 0; a < 3; a++) v[a] = (int16_t)(s->c[2][a] + s->pos[a]);
			GteLoadV2(v);
			GteRTPT();
			F<uint32_t>(pk, 0xC) = s->uv0;
			F<uint32_t>(pk, 0x14) = s->uv1;
			F<uint16_t>(pk, 0x1C) = s->uv2;
			GteReadSXY012Split(pk + 8, pk + 0x10, pk + 0x18);
			uint32_t off = 0;
			if (F<int16_t>(pk, 8) < 0 || F<int16_t>(pk, 8) > 0xA00) off = 1;
			if (F<int16_t>(pk, 0x10) < 0 || F<int16_t>(pk, 0x10) > 0xA00) off |= 2;
			if (F<int16_t>(pk, 0x18) < 0 || F<int16_t>(pk, 0x18) > 0xA00) off |= 4;
			if (F<int16_t>(pk, 0xA) < 0 || F<int16_t>(pk, 0xA) > 0x6C0) off |= 0x10;
			if (F<int16_t>(pk, 0x12) < 0 || F<int16_t>(pk, 0x12) > 0x6C0) off |= 0x20;
			if (F<int16_t>(pk, 0x1A) < 0 || F<int16_t>(pk, 0x1A) > 0x6C0) off |= 0x40;
			uint32_t size;
			if (s->kind == 2)
			{
				for (int a = 0; a < 3; a++) v[a] = (int16_t)(s->c[3][a] + s->pos[a]);
				GteLoadV0(v);
				GteRTPS();
				F<uint16_t>(pk, 0x24) = s->uv3;
				GteReadSXY2(pk + 0x20);
				GteAVSZ4();
				if (F<int16_t>(pk, 0x20) < 0 || F<int16_t>(pk, 0x20) > 0xA00) off |= 8;
				if (F<int16_t>(pk, 0x22) < 0 || F<int16_t>(pk, 0x22) > 0x6C0) off |= 0x80;
				if ((off & 0xF) == 0xF || (off & 0xF0) == 0xF0) continue;
				GteReadOTZWord(v);
				if (v[0] <= 0) continue;
				F<uint32_t>(pk, 0) = 0x9000000;
				F<uint32_t>(pk, 4) = 0x2CF0F0F0;
				size = 0x28;
			}
			else
			{
				GteAVSZ3();
				if ((off & 7) == 7 || (off & 0x70) == 0x70) continue;
				F<uint32_t>(pk, 0) = 0x7000000;
				F<uint32_t>(pk, 4) = 0x24F0F0F0;
				GteReadOTZWord(v);
				if (v[0] <= 0) continue;
				size = 0x20;
			}
			PacketCursor() += size;
			InsertPrimAutoDepth(var<uint32_t>(0x1D8E04C) + (uint32_t)(v[0] >> 2) * 4, pk);
		}
		FieldFree(8);
	}

	// ------------------------------------------------------------------
	// Shards' update (0x6642F0): age (dead at 0: kind 0, the shard cursor to it); corners spun by the shard's matrix
	// (a first corner spun to the origin also kills it); position += velocity
	// ------------------------------------------------------------------
	static void ShardsUpdate(MainNode *m)
	{
		for (int k = 0; k < 0x100; k++)
		{
			Shard *s = &m->shards[k];
			if (s->kind == 0) continue;
			s->life--;
			if (s->life != 0)
			{
				GteSetRotMatrixCtrl((const Mat4x3 *)s->spin);
				GteLoadV0(s->c[0]);
				GteMVMVA_RotV0();
				GteStoreIR123(s->c[0]);
				GteLoadV0u16(s->c[1]);
				GteMVMVA_RotV0();
				if (*(const uint32_t *)&s->c[0][0] != 0 || s->c[0][2] != 0)
				{
					GteStoreIR123(s->c[1]);
					GteLoadV0(s->c[2]);
					GteMVMVA_RotV0();
					GteStoreIR123(s->c[2]);
					GteLoadV0u16(s->c[3]);
					GteMVMVA_RotV0();
					GteStoreIR123(s->c[3]);
					s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
					s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
					s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
					continue;
				}
			}
			s->kind = 0;
			m->next_shard = s;
		}
	}

	// ------------------------------------------------------------------
	// Particles (0x665160 -> 0x6651F0): drawn THEN moved, as 316's with the particle's own sequence (0x10168C0[+0x0F])
	// and 298's sprite renderer
	// ------------------------------------------------------------------
	static void ParticleDraw(uint8_t *h, Particle *p)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		GteLoadV0(p);
		GteMVMVA_RotV0Tr();
		GteReadMAC123((int32_t *)(h + 0x98));
		const int16_t size = p->size;
		F<int16_t>(h, 0x94) = size;
		F<int16_t>(h, 0x8C) = size;
		F<int16_t>(h, 0x84) = size;
		const uint32_t game_seq = var<uint32_t>(SEQUENCES + (int32_t)p->seq * 4);
		const uint32_t seq = (uint32_t)p->seq < 4 ? fxp::sequence(PG().seqs.s[p->seq], game_seq) : game_seq;
		F<uint32_t>(h, 0) = seq;
		const uint32_t idx = (uint32_t)var<uint16_t>(seq + 8) - (uint32_t)(int32_t)p->life;
		const uint32_t rec = seq + var<uint16_t>(seq + idx * 2 + 8);
		F<uint32_t>(h, 0x2C) = rec;
		F<uint32_t>(h, 0x30) = var<uint32_t>(rec);
		F<uint32_t>(h, 0x2C) = rec + 4;
		PacketCursor() = DrawSprites(h, OT(0x44), 2, PacketCursor(), true);
		p->life--;
		const int32_t speed = PG().particles.speed;
		p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(p->vel[0] * speed));
		p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(p->vel[1] * speed));
		p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(p->vel[2] * speed));
	}

	static void Particles(MainNode *m)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<int16_t>(h, 0x92) = 0;
		F<int16_t>(h, 0x90) = 0;
		F<int16_t>(h, 0x8E) = 0;
		F<int16_t>(h, 0x8A) = 0;
		F<int16_t>(h, 0x88) = 0;
		F<int16_t>(h, 0x86) = 0;
		F<int16_t>(h, 0x60) = 0;
		F<int16_t>(h, 0x58) = 0;
		F<int16_t>(h, 0x50) = 0;
		F<int16_t>(h, 0x48) = 0;
		for (int k = 0; k < 0x80; k++)
			if (m->parts[k].life != 0) ParticleDraw(h, &m->parts[k]);
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// A flat semi-transparent full-screen quad (0x6650E0) of colour (ip[0], ip[1], ip[2]) (low bytes) at the frame OT
	// + 0x20 (alternate viewport); returns the program after it
	// ------------------------------------------------------------------
	static const int16_t *ScriptQuad(const int16_t *ip)
	{
		uint8_t *q = (uint8_t *)PacketCursor();
		PacketCursor() += 0x18;
		F<int16_t>(q, 8) = 0;
		F<int16_t>(q, 0x10) = 0;
		F<int16_t>(q, 0xE) = 0;
		F<int16_t>(q, 0xA) = 0;
		F<int16_t>(q, 0x14) = 0x140;
		F<int16_t>(q, 0xC) = 0x140;
		F<int16_t>(q, 0x16) = 0xD8;
		F<int16_t>(q, 0x12) = 0xD8;
		q[4] = *(const uint8_t *)ip;
		q[5] = *(const uint8_t *)(ip + 1);
		q[6] = *(const uint8_t *)(ip + 2);
		F<uint32_t>(q, 0) = 0x5000000;
		q[7] = 0x2A;
		InsertPrimAltViewport(var<uint32_t>(0x1D8E04C) + 0x20, q);
		return ip + 3;
	}

	// ------------------------------------------------------------------
	// Bone script (0x664D20)
	// ------------------------------------------------------------------
	static uint32_t __cdecl ScriptTask(TaskNode *n)
	{
		ScriptNode *s = (ScriptNode *)n;
		PoseEntry *e = s->cur;
		const int16_t *ip = s->ip;
		for (;;)
		{
			const int32_t op = *ip++;
			switch (op)
			{
			case 0x1000: e->pos[0] = ip[0]; e->pos[1] = ip[1]; e->pos[2] = ip[2]; ip += 3; continue;
			case 0x1001:
				e->pos[0] = (int16_t)(e->pos[0] + ip[0]); e->pos[1] = (int16_t)(e->pos[1] + ip[1]); e->pos[2] = (int16_t)(e->pos[2] + ip[2]);
				ip += 3; continue;
			case 0x1002: s->vel[0] = ip[0]; s->vel[1] = ip[1]; s->vel[2] = ip[2]; ip += 3; continue;
			case 0x1003:
				s->vel[0] = (int16_t)(s->vel[0] + ip[0]); s->vel[1] = (int16_t)(s->vel[1] + ip[1]); s->vel[2] = (int16_t)(s->vel[2] + ip[2]);
				ip += 3; continue;
			case 0x1004:
			{
				e->pos[0] = (int16_t)(e->pos[0] + s->vel[0]);
				e->pos[1] = (int16_t)(e->pos[1] + s->vel[1]);
				e->pos[2] = (int16_t)(e->pos[2] + s->vel[2]);
				const int32_t bone = (int32_t)((uint8_t *)s->cur - (uint8_t *)s->pose) >> 4;
				int32_t w[3];
				TransformVectorBy3x3Matrix((const Mat4x3 *)(s->entity + 0x40), (const int32_t *)(**(uint8_t ***)(s->entity + 0x64) + mul32(bone, 0x30) + 0x34), w);
				const int32_t y = w[1] + e->pos[1];
				if (y <= s->ground) continue;
				s->vel[1] = (int16_t)((int16_t)(s->vel[1] >> 2) - s->vel[1]);
				if (s->bounced) continue;
				s->bounced = 1;
				MainNode *m = (MainNode *)(TexBase() + MAIN);
				const AbadonParams &g = PG();
				const int32_t sp = g.dust.spread;
				Particle *p = g.dust.count > 0 ? AllocParticle(m) : nullptr;
				for (int left = g.dust.count; p; )
				{
					GetEffectSpawnPosition(Caster(), (int32_t)((uint8_t *)s->cur - (uint8_t *)s->pose) >> 4, CrtRand() & 0xFFF, p);
					p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(CrtRand() % (2 * sp) + e->pos[0] - sp));
					p->pos[1] = (int16_t)(p->pos[1] + e->pos[1]);
					const int16_t dz = (int16_t)(CrtRand() % (2 * sp) + e->pos[2]);
					p->life = g.dust.life;
					p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(dz - sp));
					p->size = (int16_t)((CrtRand() & 0x7FF) + g.dust.size);
					p->vel[0] = (int8_t)((CrtRand() & 7) - 4);
					p->vel[2] = (int8_t)((CrtRand() & 7) - 4);
					const uint8_t vy = (uint8_t)(0xF8 - (uint8_t)(CrtRand() & 7));
					p->vel[1] = (int8_t)vy;
					p->seq = 0;
					if (--left == 0) break;
					p = AllocParticle(m);
				}
				continue;
			}
			case 0x1008: s->ground = *ip++; continue;
			case 0x100A: e->vis = 1; continue;
			case 0x1010: e->rot[0] = ip[0]; e->rot[1] = ip[1]; e->rot[2] = ip[2]; ip += 3; continue;
			case 0x1011:
				e->rot[0] = (int16_t)(e->rot[0] + ip[0]); e->rot[1] = (int16_t)(e->rot[1] + ip[1]); e->rot[2] = (int16_t)(e->rot[2] + ip[2]);
				ip += 3; continue;
			case 0x1012: s->avel[0] = ip[0]; s->avel[1] = ip[1]; s->avel[2] = ip[2]; ip += 3; continue;
			case 0x1013:
				s->avel[0] = (int16_t)(s->avel[0] + ip[0]); s->avel[1] = (int16_t)(s->avel[1] + ip[1]); s->avel[2] = (int16_t)(s->avel[2] + ip[2]);
				ip += 3; continue;
			case 0x1014:
				e->rot[0] = (int16_t)(e->rot[0] + s->avel[0]); e->rot[1] = (int16_t)(e->rot[1] + s->avel[1]); e->rot[2] = (int16_t)(e->rot[2] + s->avel[2]);
				continue;
			case 0x10B0: ip = ScriptQuad(ip); continue;
			case 0x10F0:
			{
				int16_t *loop = (int16_t *)ip;
				if (loop[0] != 0)
				{
					loop[0] = (int16_t)(loop[0] - 1);
					ip = (const int16_t *)((const uint8_t *)(ip + 1) + mul32(ip[2], 2) - 4);
				}
				else
				{
					loop[0] = loop[1];
					ip += 3;
				}
				continue;
			}
			case 0x1100: return TASK_END;
			default:
				s->ip = ip;
				return 0;
			}
		}
	}

	// 0x664CD0: a bone script moving pose entry script[0]
	static void StartScript(uint8_t *ent, PoseEntry *pose, const int16_t *script)
	{
		ScriptNode *s = (ScriptNode *)AddTaskToQueue(QScripts(), own::Fn(ORIG_ScriptTask));
		Memset32(&s->cur, 0, 9);
		s->entity = ent;
		s->pose = pose;
		s->cur = pose + script[0];
		s->ip = script + 1;
	}

	// sweep A level at counter c (0x10..0x41: the parameters' window)
	static int16_t LevelA(int32_t c)
	{
		const AbadonParams &g = PG();
		return (int16_t)(g.sweep.top - (int32_t)((uint32_t)(c - g.sweep.start) * (uint32_t)(int32_t)g.sweep.range / (uint32_t)g.sweep.ticks));
	}
	// shard sweep level at counter c (0x64..0xD2: the parameters' window)
	static int16_t LevelShards(int32_t c) { return (int16_t)(mul32(3 * (c - PG().shards.start) - 0x14A, 1000) / 110 + PG().shards.base); }

	// ------------------------------------------------------------------
	// Main task (0x662F40)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const AbadonParams &g = PG();
		const int16_t c = m->counter;
		if (c >= g.timing.end)
		{
			*(uint16_t *)Caster() &= 0xFFF7;
			*(uint16_t *)(Caster() + 0x9C) &= 0xFFF7;
			*(uint32_t *)(Caster() + 0x2C) = m->colour[0];
			*(uint32_t *)(Caster() + 0xC8) = m->colour[1];
			ShowAllPolys(Caster());
			ShowAllPolys(Caster() + 0x9C);
			*(uint16_t *)Entity(0) &= 0xFFFB;
			*(uint16_t *)Entity(1) &= 0xFFFB;
			*(uint16_t *)Entity(2) &= 0xFFFB;
			return TASK_END;
		}
		if (c < g.flash.ramp) Flash() = (uint32_t)(mul32(c, g.flash.level) / g.flash.ramp);
		else if (c > g.timing.end - g.flash.ramp) Flash() = (uint32_t)(mul32(g.timing.end - c, g.flash.level) / g.flash.ramp);
		else Flash() = (uint32_t)(int32_t)g.flash.level;
		// 30 fps layer: see mag314_abadon_held.inc
		FX_HELD(held_note_main(m);)
		if ((uint32_t)(m->counter - g.sweep.start) < (uint32_t)g.sweep.ticks)
		{
			FX_PART(PART_SWEEP);
			SweepA(Caster(), LevelA(m->counter), g.sweep.page, g.sweep.clut, PacketCursor());
			*Caster() |= 4;
		}
		const uint32_t t = (uint32_t)(m->counter - g.fade.start);
		if (t < (uint32_t)g.fade.ticks)
		{
			const int32_t w = (int32_t)((t << 12) / (uint32_t)g.fade.ticks);
			for (int k = 0; k < 2; k++)
			{
				ColourLerp(&m->colour[k], own::R(COLOUR_Target), 0x1000 - w, w, Caster() + k * 0x9C + 0x2C);
				Caster()[k * 0x9C + 0x2F] = (uint8_t)g.fade.alpha;
			}
		}
		if (m->counter == g.scripts.start)
		{
			const uint32_t table = fxp::data(g.tables.scripts, SCRIPT_TABLE, SCRIPT_TABLE_SIZE);
			for (int k = 0; k < 29; k++) StartScript(Caster(), m->pose, (const int16_t *)(table + (SCRIPTS[k] - SCRIPT_TABLE)));
		}
		if (m->counter >= g.scripts.start)
		{
			// 30 fps layer: see mag314_abadon_held.inc
			FX_HELD(held_note_caster(m);)
			FX_PART(PART_BODY);
			CasterDraw(Caster(), m->pose, g.sweep.page, g.sweep.clut);
			*Caster() |= 4;
		}
		// 30 fps layer: see mag314_abadon_held.inc
		FX_HELD(held_note_tiles(m->counter);)
		FX_PART(PART_TILES);
		for (int k = 0; k < 3; k++)
			if ((uint32_t)(m->counter - g.tiles.start[k]) < (uint32_t)g.tiles.ticks)
				PacketCursor() = FlashTile(m->counter - g.tiles.start[k], g.tiles.r, g.tiles.g, g.tiles.b, g.tiles.in, g.tiles.out, PacketCursor());
		// 30 fps layer: see mag314_abadon_held.inc
		FX_HELD(held_note_parts(m);)
		FX_PART(PART_PARTICLES);
		Particles(m);
		FX_PART(PART_SHARDS);
		ShardsDraw(m);
		if (m->counter == g.timing.sound) BdPlaySE((const void *)fxp::sound(g.sounds.start, (uint32_t)SOUND_Start), 0, 0x80);
		else if (m->counter == g.timing.release) ReleaseVoice(m->voice);
		ShardsUpdate(m);
		if ((uint32_t)m->counter <= 8)
		{
			for (int k = 0; k < 2; k++)
			{
				if (m->counter & 1) *(uint16_t *)(Caster() + k * 0x9C) &= 0xFFF7;
				else Caster()[k * 0x9C] |= 8;
			}
		}
		const int32_t c2 = m->counter;
		if ((uint32_t)c2 <= (uint32_t)g.sparks.ticks)
		{
			Particle *p = g.sparks.count > 0 ? AllocParticle(m) : nullptr;
			for (int left = g.sparks.count; p; )
			{
				uint8_t *ent = Caster();
				const int32_t bone = CrtRand() % (int32_t)(uint32_t)***(uint8_t ***)(ent + 0x64);
				GetEffectSpawnPosition(ent, bone, 0, p);
				p->life = g.sparks.life;
				const int32_t r = CrtRand();
				*(uint32_t *)&p->vel[0] = 0;
				p->size = (int16_t)((r & 0x7FF) + g.sparks.size);
				p->seq = (int8_t)((CrtRand() & 1) + 2);
				if (--left == 0) break;
				p = AllocParticle(m);
			}
		}
		if ((uint32_t)c2 <= 0x10)
		{
			const int32_t w = (int32_t)(((uint32_t)c2 << 12) >> 4) + 0x80;
			ColourLerp(&m->tint1, own::R(COLOUR_Target), 0x1000 - w, w, Caster() + 0xC4);
			Caster()[0xC7] = 2;
			Caster()[0x9D] |= 8;
		}
		if (c2 == 0x10) Caster()[0x9C] |= 4;
		if ((uint32_t)(m->counter - g.shards.start) < (uint32_t)g.shards.ticks)
			ShardSweep(Caster(), m->pose, &m->spins[0][0], LevelShards(m->counter), g.shards.spread, g.sweep.page, g.sweep.clut);
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x662970)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		if (r->parity != 0)
		{
			PacketCursor() = TexBase() + ARENA_A;
			r->parity = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			r->parity = 1;
		}
		Flash() = 0;
		// 30 fps layer: see mag314_abadon_held.inc
		FX_HELD(held_note_root();)
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			MusicVolumeFade(0xF0);
			InitTaskQueuePool(QMain(), (void *)(TexBase() + MAIN), 0x4C28, 1);
			InitTaskQueuePool(QScripts(), (void *)TexBase(), 0x30, 0x38);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), own::Fn(ORIG_MainTask));
			Memset32(&m->counter, 0, 0x1307);
			m->voice = ClaimVoiceSlot((const void *)fxp::sound(PG().sounds.voice, (uint32_t)SOUND_Voice), 1, 0x80);
			m->colour[0] = *(const uint32_t *)(Caster() + 0x2C);
			m->colour[1] = *(const uint32_t *)(Caster() + 0xC8);
			m->tint1 = *(const uint32_t *)(Caster() + 0xC4);
			m->next_part = &m->parts[0];
			m->next_shard = &m->shards[0];
			StreamStateInit((void *)(TexBase() + STREAM_STATE));
			for (int k = 0; k < 0x40; k++)
			{
				int16_t ang[4];
				const int32_t spin = PG().shards.spin;
				ang[0] = (int16_t)(CrtRand() % (2 * spin) - spin);
				ang[1] = (int16_t)(CrtRand() % (2 * spin) - spin);
				ang[2] = (int16_t)(CrtRand() % (2 * spin) - spin);
				BuildRotationMatrixFromAngles(ang, (Mat4x3 *)m->spins[k]);
				const int32_t s = PG().shards.scale - (CrtRand() & 0x1FF);
				const int32_t sc[3] = { s, s, s };
				Scale3DMatrix((Mat4x3 *)m->spins[k], sc);
			}
		}
		const int32_t stop = PG().timing.end + 2;   // (0xE4: the main's end + 2)
		if (r->started && r->counter < stop)
		{
			ExecuteTaskQueue(QMain());
			PoseBones(Caster() + 0x60, (const PoseEntry *)(TexBase() + MAIN + 0x20));
			ExecuteTaskQueue(QScripts());
			BuildBoneMatricesFromPose(Caster() + 0x60);
		}
		SetScreenFlash(Flash(), 0);
		if (r->counter == stop)
		{
			StreamLoad(0x168, TexBase() + STREAM_DST, 1);
			StreamLoad(0x169, TexBase() + STREAM_DST, 0);
		}
		if (r->started) StreamPump();
		if (r->counter > stop) return StreamPending(1) ? TASK_END : 0;
		r->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// File loader (0x6628A0) and setup (0x662890 -> _Init 0x6628C0): the game runs its own; Fujin's programs these (its
	// memory may be a copy's): the TIM, the magic buffer, the context, the caster (both halves' flags 0x0C cleared), the
	// root, the party's normal draw hidden, the camera
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(PG().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(TexFile());
		TexBase() = MagicTextureOff();
		Ctx() = ctx;
		Caster() = Entity(ctx->attacker);
		*(uint16_t *)Caster() &= 0xFFF3;
		*(uint16_t *)(Caster() + 0x9C) &= 0xFFF3;
		InitTaskQueuePool(QRoot(), (void *)own::R(0x25055B8), 0x14, 1);
		Memset32((uint8_t *)AddTaskToQueue(QRoot(), own::Fn(ORIG_RootTask)) + 0xC, 0, 2);
		*(uint16_t *)Entity(0) |= 4;
		*(uint16_t *)Entity(1) |= 4;
		*(uint16_t *)Entity(2) |= 4;
		PlayCameraAnimation((const void *)fxp::camera(PG().res.camera, CAMERA));
		return QRoot();
	}

	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "main", "Main", "The main's ticks, the sound and the voice (claimed at the start, released near the end), the screen flash, the music faded; the texture and the camera" },
			{ PART_SWEEP, "sweep", "Light sweep", "The caster drawn lit by a light level falling down its body (its vertices grey by their height)" },
			{ 0, "fade", "Colour fade", "Both halves' colours faded to black" },
			{ 0, "scripts", "Bone scripts", "The 29 bone scripts: each moves a pose entry of the caster's upper body (offsets, turns, their speeds, a bounce on the ground throwing dust once, hiding it), loops" },
			{ PART_BODY, "body", "Body", "The caster's upper body drawn with the scripts' pose entries" },
			{ PART_TILES, "tiles", "White tiles", "Full-screen tiles flashing in three windows" },
			{ PART_PARTICLES, "sparks", "Sparks", "Sparks at random bones of the caster on the first ticks (flipbooks C and D)" },
			{ 0, "dust", "Dust", "Dust where a scripted bone first bounces on the ground (flipbook A)" },
			{ PART_SHARDS, "shards", "Shards", "The body's polygons above a rising level leaving it as spinning shards, some throwing a particle" },
			{ 0, "particles", "Particles", "Every particle (sparks, dust, the shards'): its flipbooks, its speed; the shards' particles' life" },
		};
		static const fxp::Group GROUPS[] = {
			{ "timing", "Ticks", "The main's ticks" },
		};
#define APF(member) (uint32_t)offsetof(AbadonParams, member)
		static const Field FIELDS[] = {
			{ "main", "sounds.start", "Sound", "At the sound's tick", APF(sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "main", "sounds.voice", "Voice", "The sound block its voice slot is claimed with (at the start)", APF(sounds.voice), fxp::Sound, 0, 0x7FFFFFFF },
			{ "main", "timing.sound", "Sound", "The main's tick of the sound", APF(timing.sound), fxp::I16, 0, 400 },
			{ "main", "timing.release", "Voice released", "Its tick the voice slot is released", APF(timing.release), fxp::I16, 0, 400 },
			{ "main", "timing.end", "End", "Its last tick (the colours put back, every polygon shown again; the scripts stop 2 ticks later, then the battle files it streams)", APF(timing.end), fxp::I16, 1, 400 },
			{ "main", "flash.ramp", "Flash ramp", "The screen flash's ticks up at the start and down at the end", APF(flash.ramp), fxp::I16, 1, 400 },
			{ "main", "flash.level", "Flash level", "The screen flash's level between", APF(flash.level), fxp::I16, 0, 4000 },
			{ "main", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", APF(res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "main", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", APF(res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "sweep", "sweep.start", "Start", "The main's tick it starts", APF(sweep.start), fxp::I16, 0, 400 },
			{ "sweep", "sweep.ticks", "Ticks", "Its ticks", APF(sweep.ticks), fxp::I16, 1, 400 },
			{ "sweep", "sweep.top", "Top level", "The light level's start (y down)", APF(sweep.top), fxp::I16, -0x4000, 0x4000 },
			{ "sweep", "sweep.range", "Fall", "The level falls by this over its ticks", APF(sweep.range), fxp::I16, 0, 0x4000 },
			{ "sweep", "sweep.page", "Page move", "The swept, the scripted body's and the shards' texture page word moved by this", APF(sweep.page), fxp::I16, -0x400, 0x400 },
			{ "sweep", "sweep.clut", "CLUT move", "Their CLUT word moved by this", APF(sweep.clut), fxp::I16, -0x8000, 0x7FFF },
			{ "fade", "fade.start", "Start", "The main's tick it starts", APF(fade.start), fxp::I16, 0, 400 },
			{ "fade", "fade.ticks", "Ticks", "Its ticks", APF(fade.ticks), fxp::I16, 1, 400 },
			{ "fade", "fade.alpha", "Alpha byte", "Both halves' byte +0x2F while they fade", APF(fade.alpha), fxp::I16, 0, 0xFF },
			{ "scripts", "scripts.start", "Start", "The main's tick the scripts start (the body is drawn by the module from then on)", APF(scripts.start), fxp::I16, 0, 400 },
			{ "scripts", "tables.scripts", "Bone scripts", "The 29 bone scripts' byte code (0x4CC bytes, each script at its own offset: edit their numbers, keep their shape)", APF(tables.scripts), fxp::Data, 0, 0x7FFFFFFF },
			{ "tiles", "tiles.start[0]", "Window 1", "The main's tick the first window starts", APF(tiles.start[0]), fxp::I16, 0, 400 },
			{ "tiles", "tiles.start[1]", "Window 2", "The second's", APF(tiles.start[1]), fxp::I16, 0, 400 },
			{ "tiles", "tiles.start[2]", "Window 3", "The third's", APF(tiles.start[2]), fxp::I16, 0, 400 },
			{ "tiles", "tiles.ticks", "Ticks", "Each window's ticks", APF(tiles.ticks), fxp::I16, 0, 400 },
			{ "tiles", "tiles.r", "Red", "The tiles' red", APF(tiles.r), fxp::I16, 0, 0xFF },
			{ "tiles", "tiles.g", "Green", "Their green", APF(tiles.g), fxp::I16, 0, 0xFF },
			{ "tiles", "tiles.b", "Blue", "Their blue", APF(tiles.b), fxp::I16, 0, 0xFF },
			{ "tiles", "tiles.in", "Ramp in", "A tile's ticks fading in", APF(tiles.in), fxp::I16, 1, 400 },
			{ "tiles", "tiles.out", "Ramp out", "Its ticks fading out", APF(tiles.out), fxp::I16, 1, 400 },
			{ "sparks", "sparks.ticks", "Ticks", "Thrown on the main's ticks up to this", APF(sparks.ticks), fxp::I16, 0, 400 },
			{ "sparks", "sparks.count", "Count", "How many a tick (128 particles at most alive)", APF(sparks.count), fxp::I16, 0, 128 },
			{ "sparks", "sparks.life", "Life", "Their ticks (below their flipbooks' frame count: 15 at most)", APF(sparks.life), fxp::I16, 1, 15 },
			{ "sparks", "sparks.size", "Size", "Their size (+ a random below 0x800)", APF(sparks.size), fxp::I16, 0, 0x4000, fxp::Scale },
			{ "dust", "dust.count", "Count", "How many at a bounce", APF(dust.count), fxp::I16, 0, 128 },
			{ "dust", "dust.life", "Life", "Their ticks (below their flipbook's frame count: 12 at most)", APF(dust.life), fxp::I16, 1, 12 },
			{ "dust", "dust.size", "Size", "Their size (+ a random below 0x800)", APF(dust.size), fxp::I16, 0, 0x4000, fxp::Scale },
			{ "dust", "dust.spread", "Scatter", "Around the bone: a random below 2 x this - this on x / z", APF(dust.spread), fxp::I16, 1, 0x2000 },
			{ "shards", "shards.start", "Start", "The main's tick the shard sweep starts", APF(shards.start), fxp::I16, 0, 400 },
			{ "shards", "shards.ticks", "Ticks", "Its ticks", APF(shards.ticks), fxp::I16, 0, 400 },
			{ "shards", "shards.base", "Level", "The rising level's base (polygons above it leave the body)", APF(shards.base), fxp::I16, -0x4000, 0x4000 },
			{ "shards", "shards.spread", "Velocity mask", "A shard's velocity: a random masked by this (half of it off x, z backwards, y up)", APF(shards.spread), fxp::I16, 0, 0x7FFF },
			{ "shards", "shards.life", "Life", "Its ticks: this + a random below 8", APF(shards.life), fxp::I16, 1, 400 },
			{ "shards", "shards.chance", "Particle chance", "It throws a particle when a random below 0x8000 is below this", APF(shards.chance), fxp::I16, 0, 0x7FFF },
			{ "shards", "shards.spin", "Spin", "The 64 spin matrices' angles: a random below 2 x this - this on each axis, a tick", APF(shards.spin), fxp::I16, 1, 0x800, fxp::Angle },
			{ "shards", "shards.scale", "Shrink", "Their scale: this - a random below 0x200 (0x1000: the shards keep their size)", APF(shards.scale), fxp::I16, 0, 0x2000, fxp::Scale },
			{ "particles", "particles.shard_life", "Shard particles' life", "The shards' particles' ticks (below their flipbook's frame count: 11 at most)", APF(particles.shard_life), fxp::I16, 1, 11 },
			{ "particles", "particles.speed", "Speed", "Every particle's velocity scale a tick", APF(particles.speed), fxp::I16, -64, 64 },
			{ "particles", "seqs.s[0]", "Flipbook A", "The dust's flipbook", APF(seqs.s[0]), fxp::Seq, 0, 0x7FFFFFFF },
			{ "particles", "seqs.s[1]", "Flipbook B", "The shards' particles' flipbook", APF(seqs.s[1]), fxp::Seq, 0, 0x7FFFFFFF },
			{ "particles", "seqs.s[2]", "Flipbook C", "The sparks' first flipbook", APF(seqs.s[2]), fxp::Seq, 0, 0x7FFFFFFF },
			{ "particles", "seqs.s[3]", "Flipbook D", "The sparks' second flipbook", APF(seqs.s[3]), fxp::Seq, 0, 0x7FFFFFFF },
		};
#undef APF
	}
}

	void register_mag314_abadon()
	{
		fxp::register_params({ 314, "Abadon death", &abadon314::live, &abadon314::GAME, sizeof(abadon314::AbadonParams),
		                       abadon314::table::PARTS, (int)(sizeof(abadon314::table::PARTS) / sizeof(abadon314::table::PARTS[0])),
		                       abadon314::table::FIELDS, (int)(sizeof(abadon314::table::FIELDS) / sizeof(abadon314::table::FIELDS[0])),
		                       abadon314::table::GROUPS, (int)(sizeof(abadon314::table::GROUPS) / sizeof(abadon314::table::GROUPS[0])) });
		own::note(abadon314::ORIG_FileLoader, (void *)abadon314::FileLoader);
		own::note(abadon314::ORIG_Setup, (void *)abadon314::Setup);
		own::note(abadon314::ORIG_RootTask, (void *)abadon314::RootTask);
		register_port(abadon314::ORIG_RootTask, (void *)abadon314::RootTask, "314 RootTask", 314);
		own::note(abadon314::ORIG_MainTask, (void *)abadon314::MainTask);
		register_port(abadon314::ORIG_MainTask, (void *)abadon314::MainTask, "314 MainTask", 314);
		own::note(abadon314::ORIG_ScriptTask, (void *)abadon314::ScriptTask);
		register_port(abadon314::ORIG_ScriptTask, (void *)abadon314::ScriptTask, "314 ScriptTask", 314);
		// 30 fps layer: see mag314_abadon_held.inc
		FX_HELD(register_mag314_held();)
	}
	FX_LAB_MODULE(register_mag314_abadon);
}

#ifdef FF8_FX_HELD
#include "mag314_abadon_held.inc"
#endif
