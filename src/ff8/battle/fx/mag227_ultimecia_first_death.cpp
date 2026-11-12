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

// Effect 227: Ultimecia First Death (enemy attack 339 of kernel.bin; Ultimecia's first phase c0m121, the end
// of her first fight: her last words and the fall; MAG_227_*). No other kernel.bin entry uses effect 227.
//
// Structure (setup MAG_227 0x693790 -> _Init 0x6937C0, file loader 0x6937A0 = the TIM named at 0x1132020 into
// 0x2508700; the setup keeps the CASTER's entity and clears its flag bit 2 (+0x00 word &= ~4), starts camera
// animation 0x112D4C0 and queues the TIM):
//   MasterTask (0x693840) - alternates the packet arena (magic buffer + 0xE24 / + 0x10E24; the other cursor
//     0x250870C = + 0x10E24 / + 0x20E24 is never read); on counter 1 sets up the pools once (the 0x80 sparks at
//     + 0x61C freed, 0x693960; main 1 x 0x49C at + 0x180, feathers 0x10 x 0x18 at + 0) and the main task (voice
//     slot 0x112D1C8 claimed into its +0x10). Then every tick the caster point 0x2508758 (0x693990: the point
//     0x1132018 {x, y, z, bone} of her model, her bone matrices rebuilt from her current pose), the main and
//     feather queues; ends when the main queue is empty (feathers still alive are dropped).
//   MainTask (0x693A10) - node 0x49C, 0x11C ticks, the three prim-model layouts through PrimCallback at the
//     callback block's position (depth -0x80, blended frames at magic buffer + 0xE1C):
//       layout A 0x112DCF8 (decoded at 0xD4 into +0x1C): 0xD4..0x105 at the caster point 200 higher (rising by
//         40 a tick from 0x102), 0x106..0x10D from the caster point taken at 0x106 (+0x14) to the point 0x2508760
//         = (caster x, 0, caster z + 1000) set then (t = 0x100 + 0x200 a tick, 0x56CB90);
//       layout B 0x112FE80 (decoded at 0x104 into +0x358): 0x104..0x115 at the caster's position + (0x2C,
//         -0x8EE, 0x19A);
//       layout C 0x1130368 (decoded at 0x10C into +0x418): 0x10C..0x11B at the point 0x2508760, depth 0.
//     Then the sparks drawn (0x694110, Shockwave Pulsar 70's 0x6DF9A0 on this pool) and moved (0x694240 = 70's
//     0x692DC0 text: life--, position += 4 x velocity). Sounds 0x112DAA0 at 0, 0x112DAA4 at 0xD4, the voice slot
//     released at 0x118. Sparks spawned (0x694290 / allocator 0x694300) at random points of her model: counters
//     0..0x5E two from each of the tables 0x1131660 / 0x1131960 (0x60 points each) twice, 0xB4..0xC9 two from
//     0x1131C60 (0x77 points); sequence 0x112D2E0, size 0x200, velocity ((rand & 3) - 2, (rand & 3) + 3,
//     (rand & 3) - 2), life 0x12. Counters 0xDE..0xF4: a feather task a tick (random angles, y spin +-0x10, z
//     spin -0x25 - (rand & 0x1F)). Her lines (0x4876F0: battle text of the caster's slot): line 0 at 0x26 (0x2A
//     ticks), 1 at 0x55, 2 at 0x73 (0x1E), 3 at 0xA5 (0xF). At 0x11C the caster's flag bit 2 is set back and the
//     task ends.
//   FeatherTask (0x694360) - node 0x18, 0x11 ticks, DRAWS THEN UPDATES: prim model 0x112DAA8 at the caster point
//     200 higher (followed every tick), rotated about z by its +0xE then about y by its +0x10, its x scaled by
//     1.5 x (1 + sin(counter x 0x40)) and y by 2 x sin(counter x 0x40) (0x701270 / 0x56C220 / 0x7012C0 /
//     0x56C270), faded in over the first 3 ticks and out from 0xB (header +0xC), drawn by 0x7043B0 at depth
//     -0x80; then its angles turn by their spins.
//   PrimCallback (0x693E20, Boomerang Sword 304's 0x66A440 / Reflect Beam's 0x6AF610 on this module's cursor)
//     draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: the main task plays its layouts (each play draws then advances), draws the sparks then moves them;
// the feathers draw then update.
// ENTITY / CAMERA: the caster's flag bit 2 (cleared by the setup, set at the main task's end); no task writes an
// entity position or the camera words (the camera animation is the engine's). Battle text 0x4876F0 (4 lines).
// Module globals: 0x2508700..0x250876C (texture file 0x2508700, spark cursor 0x2508704, packet cursors 0x2508708 /
// 0x250870C, context 0x2508710, caster entity 0x2508714, queues 0x2508718 feathers / 0x2508728 main / 0x2508738
// master, master pool 0x2508748, caster point 0x2508758, layout point 0x2508760 (its 4th word 0x2508766 is never
// written), magic buffer base 0x2508768).
// NEVER-WRITTEN STACK WORDS: the callback block's 4th position word (MainTask entry esp - 0x16 + 0xE; 0x56CB90
// stores 3 words) only reaches the GTE V0 pad (flag 0x200 objects) - modelled as 0 (CallbackPad); the feather's
// scale matrix pad / translation and the matrices' pads are read by nothing that draws.

// Fujin's described Ultimecia's first death (effect_227.json) runs this code: its memory through own::R() (fx_port.h),
// its spawns through own::Fn(), the setup and the file loader ported below; its parameters (UltimeciaParams: the sounds,
// the timeline, her four lines, the three layouts' ticks and places, the sparks and their point tables, the feathers,
// the feather mesh, the spark flipbook, the texture name and the camera track). FX_PART tags its parts in the lab.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_layouts.h"
#include <stddef.h>

namespace ff8fx
{
namespace ultimecia227
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(0x2508700)); }               // the effect's file (its TIM)
	inline uint8_t *&SparkCursor() { return var<uint8_t *>(own::R(0x2508704)); }          // next spark to try
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(0x2508708)); }
	inline uint32_t &PacketCursor2() { return var<uint32_t>(own::R(0x250870C)); }         // the other arena (never read)
	inline CastContext *&Ctx() { return var<CastContext *>(own::R(0x2508710)); }
	inline uint8_t *&Caster() { return var<uint8_t *>(own::R(0x2508714)); }               // caster entity (setup)
	inline TaskQueue *QFeathers() { return (TaskQueue *)own::R(0x2508718); }              // pool: + 0, 0x10 x 0x18
	inline TaskQueue *QMain() { return (TaskQueue *)own::R(0x2508728); }                  // pool: + 0x180, 1 x 0x49C
	inline int16_t *CasterPoint() { return (int16_t *)own::R(0x2508758); }                // the master's caster point (4 words)
	inline int16_t *LayoutPoint() { return (int16_t *)own::R(0x2508760); }                // layout A's end / layout C's point
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(0x2508768)); }               // Magic_TextureOFF (magic buffer)
	inline TaskQueue *QMaster() { return (TaskQueue *)own::R(0x2508738); }               // 1 x 0x10 (pool 0x2508748)

	static const uint32_t ORIG_MasterTask = 0x693840;
	static const uint32_t ORIG_MainTask = 0x693A10;
	static const uint32_t ORIG_FeatherTask = 0x694360;
	static const uint32_t ORIG_PrimCallback = 0x693E20;  // not a task: the layouts' prim callback
	static const uint32_t POOL_Main = 0x180;
	static const uint32_t SPARKS = 0x61C;                // magic buffer: 0x80 sparks x 0x10
	static const uint32_t SPARKS_LAST = 0xE0C;           // the last spark
	static const uint32_t MORPH = 0xE1C;                 // magic buffer: blended vertex frames
	static const uint32_t ARENA_A = 0xE24;               // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x10E24;
	static const uint32_t ARENA_C = 0x20E24;
	static const uint32_t LAYOUT_A = 0x112DCF8;          // prim-model layouts
	static const uint32_t LAYOUT_B = 0x112FE80;
	static const uint32_t LAYOUT_C = 0x1130368;
	static const uint32_t MODEL_Feather = 0x112DAA8;     // prim model
	static const uint32_t SEQ_Spark = 0x112D2E0;         // sprite sequence
	static const uint32_t POINT_Caster = 0x1132018;      // {x, y, z, bone} of the caster's model
	static const uint32_t POINTS_A = 0x1131660, POINTS_B = 0x1131960, POINTS_C = 0x1131C60; // spark points
	static const void *const SOUND_Start = (const void *)0x112DAA0;
	static const void *const SOUND_Fall = (const void *)0x112DAA4;
	static const void *const SOUND_Voice = (const void *)0x112D1C8;
	static const uint32_t ORIG_FileLoader = 0x6937A0, ORIG_Setup = 0x693790;
	static const uint32_t TEXTURE_NAME = 0x1132020, CAMERA_ANIM = 0x112D4C0;

	// ------------------------------------------------------------------
	// parameters (Fujin's effect lab: fx_params.h)
	// ------------------------------------------------------------------
	// the parts (FX_PART tags: what draws now, in the lab)
	enum : int { PART_RISE = 1, PART_BURST = 2, PART_GROUND = 3, PART_SPARKS = 4, PART_FEATHERS = 5 };
	struct UltimeciaParams
	{
		struct { uint32_t start, fall, voice; } sounds;  // the main task's tick 0, its fall tick; her voice (claimed at the start)
		struct { int16_t fall, voice_end, end; } timing; // the fall sound, the voice released, the main task's last tick
		// her lines (battle text of her slot: line 0..3): each one's tick and how long it shows
		struct { int16_t first, first_ticks, second, second_ticks, third, third_ticks, fourth, fourth_ticks; } lines;
		// layout A: at her point (lift higher) from its tick for hold ticks, rising the last rise_ticks by rise a tick, then 8
		// ticks to the ground point
		struct { int16_t tick, hold, lift, rise_ticks, rise; } rise;
		struct { int16_t tick, ticks, x, y, z; } burst;  // layout B: its ticks, at her position + (x, y, z)
		struct { int16_t tick, ticks, z; } ground;       // layout C at the ground point (her x, 0, her z + z): its ticks
		// sparks at random points of her model (the point tables): 2 x per a tick from tables A and B until first_end; per a
		// tick from table C for second_ticks from second; their size, life, rise
		struct { int16_t first_end, first_per, second, second_ticks, second_per, size, life, rise; } sparks;
		struct { int16_t tick, ticks, life, height, spin_y, spin_z, spin_z_rand; } feathers; // one a tick from tick, for ticks
		struct { uint32_t a, b, c; } layouts;            // the prim-model layouts played (their models' animation)
		struct { uint32_t feather; } models;             // the prim model drawn (mesh)
		struct { uint32_t spark; } seqs;                 // the sprite sequence drawn (flipbook)
		struct { uint32_t points_a, points_b, points_c, caster_point; } tables; // her model's points (x, y, z, bone)
		struct { uint32_t texture_name, camera; } res;   // its resources (data): texture file name, camera track
	};
	static const UltimeciaParams GAME = { { 0x112DAA0, 0x112DAA4, 0x112D1C8 }, { 0xD4, 0x118, 0x11C },
		{ 0x26, 0x2A, 0x55, 0x1E, 0x73, 0x1E, 0xA5, 0xF }, { 0xD4, 0x32, 0xC8, 4, 40 }, { 0x104, 0x12, 0x2C, -0x8EE, 0x19A },
		{ 0x10C, 0x10, 0x3E8 }, { 0x5F, 2, 0xB4, 0x16, 2, 0x200, 0x12, 3 }, { 0xDE, 0x17, 0x10, 0xC8, 0x10, 0x25, 0x20 },
		{ LAYOUT_A, LAYOUT_B, LAYOUT_C }, { MODEL_Feather }, { SEQ_Spark }, { POINTS_A, POINTS_B, POINTS_C, POINT_Caster },
		{ TEXTURE_NAME, CAMERA_ANIM } };
	static UltimeciaParams live = GAME;
	static const UltimeciaParams &PU() { return live; }

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesB(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_sub_7015B0
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }   // Effect_RotationYMatrix
	inline void RotZ(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x7012C0)(angle, out); }   // MAG_065_RotationZMatrix
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply: b = a x b
	inline void MulMatrix3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }             // MatrixMultiply3x3: a = a x b (3x3)
	inline void BlendVectors(const int16_t *a, const int16_t *b, int32_t wa, int32_t wb, int16_t *out) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int16_t *)>(0x56CB90)(a, b, wa, wb, out); } // (a wa + b wb) >> 12, 3 words
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_RenderPrimModel
	inline uint32_t Flipbook(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x6FC9E0)(h, ot, mode, cursor); } // MAG_065_RenderFlipbook
	inline void ReleaseVoiceSlot(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }                               // BdSound_ReleaseVoiceSlot
	inline void BattleText(int32_t slot, int32_t line, int32_t ticks) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x4876F0)(slot, line, ticks); } // printMonsterRelatedText
	inline uint32_t OT() { return var<uint32_t>(0x1D8E04C); }                    // g_Battle_FrameRenderListBase
	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x10 bytes (0x2508748)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t arena;     // +0x0E packet arena parity
		uint8_t started;   // +0x0F pools set up
	};
	struct MainNode // pool of 1 node of 0x49C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint32_t voice;    // +0x10 claimed voice slot
		int16_t from[4];   // +0x14 the caster point at 0x106 (layout A's start)
		uint8_t layout_a[0x33C]; // +0x1C
		uint8_t layout_b[0xC0];  // +0x358
		uint8_t layout_c[0x84];  // +0x418
	};
	struct FeatherNode // pool of 0x10 nodes of 0x18 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t rz;        // +0x0E angle about z
		int16_t ry;        // +0x10 angle about y
		int16_t dry;       // +0x12 y spin (+-0x10)
		int16_t drz;       // +0x14 z spin
		int16_t pad16;
	};
	struct Spark // magic buffer + 0x61C, 0x80 x 0x10
	{
		const uint8_t *seq;
		int16_t pos[3];    // +4
		int16_t scale;     // +0xA
		int8_t vel[3];     // +0xC (x 4 per tick)
		uint8_t life;      // +0xF
	};
	// the prim-player callback's parameter block (a stack block of the main task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 position (word +6 never written: CallbackPad)
		int32_t depth;     // +0x08 the prim header's +0x18
		uint32_t morph;    // +0x0C blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x10 && sizeof(MainNode) == 0x49C && sizeof(FeatherNode) == 0x18, "227 nodes");
	static_assert(sizeof(Spark) == 0x10 && sizeof(PrimArg) == 0x10, "227 blocks");

	// the callback block's 4th position word: the main task never writes it (stack residue in the original)
	static const int16_t CallbackPad = 0;

	// ------------------------------------------------------------------
	// Prim-player callback (0x693E20, Boomerang Sword's 0x66A440 / Reflect Beam's 0x6AF610): one object of a
	// layout. Vertex frame (or blend into the parameter block's morph buffer), rotation from the record's
	// angles (flag 0x40000: 0x7015B0); flag 0x200: the block's position through the camera rotation + the
	// record's offset as the translation (rotation as is); else the record's offset + the block's position
	// through the camera rotation, the camera composed with the rotation; + the camera translation; scale
	// (flag 0x100: 3x3 product, else scale3DMatrix), fade, drawn by 0x7043B0 at the block's depth.
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
		if (r->flags & 0x40000) RotationFromAnglesB(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		if (r->flags & 0x200)
		{
			// the block's position (its 4th word goes to the GTE V0 pad only)
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)(int32_t)r->pos[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)(int32_t)r->pos[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)(int32_t)r->pos[2]);
		}
		else
		{
			// the record's offset + the block's position (4th word never written: only loaded into
			// the GTE V0 pad)
			const int16_t off[4] = { (int16_t)(r->pos[0] + arg->pos[0]), (int16_t)(r->pos[1] + arg->pos[1]), (int16_t)(r->pos[2] + arg->pos[2]), 0 };
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&Camera(), &m);
		}
		m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
		m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
		m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			if (r->flags & 0x100)
			{
				Mat4x3 d = {};
				d.m[0][0] = r->scale[0];
				d.m[1][1] = r->scale[1];
				d.m[2][2] = r->scale[2];
				MulMatrix3(&m, &d);
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

	// ------------------------------------------------------------------
	// 0x693990 (Acid Quistis 317's 0x660240): the point v = {x, y, z, bone} of the caster's model in world space:
	// her bone matrices rebuilt from her current pose (0x508C90), bone v[3]'s matrix composed with her entity
	// matrix (+0x40), v through it (GTE, 3 x s16 out)
	// ------------------------------------------------------------------
	static void ModelPoint(const int16_t *v, int16_t *out)
	{
		uint8_t *caster = Caster();
		const uint8_t *bones = **(uint8_t *const *const *)(caster + 0x64) + 0x10;
		BuildBoneMatricesFromPose(caster + 0x60);
		Mat4x3 m;
		ComposeAffineTransform((const Mat4x3 *)(caster + 0x40), (const Mat4x3 *)(bones + (uint32_t)mul32(v[3], 0x30) + 0x10), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(v);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// ------------------------------------------------------------------
	// Sparks (Shockwave Pulsar 70's / 260's spark code on this module's pool)
	// ------------------------------------------------------------------
	// 0x693960: every spark freed, the cursor at the first
	static void ClearSparks()
	{
		uint8_t *p = (uint8_t *)TexBase() + SPARKS + 0xF;
		for (int k = 0x80; k != 0; k--, p += 0x10) *p = 0;
		SparkCursor() = (uint8_t *)TexBase() + SPARKS;
	}
	// 0x694300: a free spark (life 0) from the cursor on, else from the start
	static Spark *SparkAlloc()
	{
		uint8_t *p = SparkCursor();
		const uint32_t tex = TexBase();
		if (p[0xF] != 0)
		{
			p = (uint8_t *)tex + SPARKS;
			if (p[0xF] != 0)
			{
				int32_t n = 0x80;
				do
				{
					p += 0x10;
					if (--n == 0) return nullptr;
				} while (p[0xF] != 0);
			}
		}
		if ((uint32_t)p >= tex + SPARKS_LAST) SparkCursor() = (uint8_t *)tex + SPARKS;
		else SparkCursor() = p + 0x10;
		return (Spark *)p;
	}
	// 0x694290: a spark at a random point of the table (count points) on the caster's model
	static void SparkSpawn(const int16_t *points, int32_t count)
	{
		Spark *s = SparkAlloc();
		if (!s) return;
		const int32_t i = mul32(CrtRand(), count) / 0x8000;
		ModelPoint(points + i * 4, s->pos);
		s->seq = (const uint8_t *)fxp::sequence(PU().seqs.spark, SEQ_Spark);
		s->vel[0] = (int8_t)((CrtRand() & 3) - 2);
		s->vel[2] = (int8_t)((CrtRand() & 3) - 2);
		const int8_t vy = (int8_t)((CrtRand() & 3) + PU().sparks.rise);
		s->scale = PU().sparks.size;
		s->life = (uint8_t)PU().sparks.life;
		s->vel[1] = vy;
	}
	// 0x694240: life--, position += 4 x velocity
	static void SparksUpdate()
	{
		Spark *s = (Spark *)(TexBase() + SPARKS);
		for (int k = 0x80; k != 0; k--, s++)
		{
			if (s->life == 0) continue;
			s->life--;
			s->pos[0] = (int16_t)(s->pos[0] + (int16_t)((int16_t)s->vel[0] << 2));
			s->pos[1] = (int16_t)(s->pos[1] + (int16_t)((int16_t)s->vel[1] << 2));
			s->pos[2] = (int16_t)(s->pos[2] + (int16_t)((int16_t)s->vel[2] << 2));
		}
	}
	// 0x694110 (70's 0x6DF9A0): one flipbook frame each (frame = sequence counted down by life)
	static void SparksDraw(const Spark *s)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint16_t>(h, 0x60) = 0;
		F<uint16_t>(h, 0x58) = 0;
		F<uint16_t>(h, 0x50) = 0;
		F<uint16_t>(h, 0x48) = 0;
		for (int n = 0x80; n != 0; n--, s++)
		{
			if (s->life == 0) continue;
			GteSetRotMatrixCtrl(&Camera());
			GteSetTransVectorCtrl(&Camera());
			GteLoadV0(s->pos);
			GteMVMVA_RotV0Tr();
			GteReadMAC123((int32_t *)(h + 0x98));
			memset(h + 0x84, 0, 0x14);
			F<uint16_t>(h, 0x94) = (uint16_t)s->scale;
			F<uint16_t>(h, 0x8C) = (uint16_t)s->scale;
			F<uint16_t>(h, 0x84) = (uint16_t)s->scale;
			const uint8_t *seq = s->seq;
			*(const uint8_t **)h = seq;
			const int32_t frames = *(const int16_t *)(seq + 8);
			const int32_t k = frames - ((int32_t)(int8_t)s->life - 1) % frames;
			const uint8_t *frame = seq + *(const int16_t *)(seq + 8 + 2 * k);
			*(const uint8_t **)(h + 0x2C) = frame;
			const int32_t word = *(const int32_t *)frame;
			F<int32_t>(h, 0x30) = word;
			if (word < 0)
			{
				h[0x25] |= 1; // sticky in the scratch block, as the original
				F<uint32_t>(h, 0x30) = (uint32_t)word & 0x7FFFFFFF;
			}
			*(const uint8_t **)(h + 0x2C) = frame + 4;
			PacketCursor() = Flipbook(h, OT() + 0x44, 2, PacketCursor());
		}
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// Feather (0x694360) pieces: its fade, its scale matrix entries, its draw
	// ------------------------------------------------------------------
	static int32_t FeatherFade(int32_t c)
	{
		if (c > 0xA) return shl32(c - 0xA, 12) / 6;
		if (c < 3) return shl32(3 - c, 10);
		return 0;
	}
	// x scale (1.5 x (0x1000 + sin)) and y scale (2 x sin), 16-bit as the original computes them
	static void FeatherScale(int32_t c, int16_t &sx, int16_t &sy)
	{
		const int32_t s = ComputeSin((shl32(c, 12) / 16) >> 2);
		const int16_t a = (int16_t)(s + 0x1000);
		sx = (int16_t)(a + (int16_t)(a >> 1));
		sy = (int16_t)(s + s);
	}
	// the model at t (3 words) rotated about z by rz then about y by ry, scaled (sx, sy, 1), faded
	static void FeatherDraw(int32_t fade, int16_t sx, int16_t sy, int16_t ry, int16_t rz, const int32_t *t)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		Mat4x3 m, ym, d;
		m.t[0] = t[0];
		m.t[1] = t[1];
		m.t[2] = t[2];
		*(uint32_t *)(h + 0x1C) = 0xF3;
		*(uint32_t *)(h + 8) = 0;
		*(int32_t *)(h + 0xC) = fade;
		memset(&d, 0, 0x12);
		d.m[0][0] = sx;
		d.m[1][1] = sy;
		d.m[2][2] = 0x1000;
		RotY(ry, &ym);
		MulMatrix3(&ym, &d);
		RotZ(rz, &m);
		MatrixMultiply(&ym, &m);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = fxp::model(PU().models.feather, MODEL_Feather);
		*(int32_t *)(h + 0x18) = -0x80;
		PacketCursor() = RenderPrim2(h, OT() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
	}
	// the feather's translation: the caster point 200 higher
	static void FeatherAt(const int16_t *point, int32_t *t)
	{
		t[0] = point[0];
		t[1] = (int32_t)point[1] - PU().feathers.height;
		t[2] = point[2];
	}

	// layout A's position on counter c (0xD4..0x10D) from the caster point (0xD4..0x105) or the node's start
	// (0x106..: + the layout point); 3 words
	static void LayoutAPos(const MainNode *m, int32_t c, const int16_t *point, int16_t *pos)
	{
		const int32_t from = PU().rise.tick, hold = PU().rise.hold;
		if ((uint32_t)(c - from) < (uint32_t)hold)
		{
			const int32_t k = c - from;
			int32_t dy = -PU().rise.lift;
			if (k >= hold - PU().rise.rise_ticks) dy = (k - hold) * PU().rise.rise;
			pos[0] = point[0];
			pos[1] = (int16_t)(point[1] + dy);
			pos[2] = point[2];
		}
		else
		{
			const int32_t t = (int32_t)(((uint32_t)(c - (from + hold)) << 12) >> 3) + 0x100;
			BlendVectors(m->from, LayoutPoint(), 0x1000 - t, t, pos);
		}
	}
}
}

#ifdef FF8_FX_HELD
#include "mag227_ultimecia_first_death_held.h"
#endif

namespace ff8fx
{
namespace ultimecia227
{
	// ------------------------------------------------------------------
	// File loader (0x6937A0) and setup (0x693790 -> _Init 0x6937C0): the game runs its own; Fujin's programs these (its
	// memory may be a copy's): the TIM, the magic buffer, the cast, its caster's entity (its flag bit 2 cleared), the master
	// (it sets up the rest), the camera
	// ------------------------------------------------------------------
	static uint32_t __cdecl FileLoader()
	{
		TexFile() = IO_GetFile_MAGIC((const char *)fxp::name(PU().res.texture_name, TEXTURE_NAME));
		return TexFile();
	}

	static TaskQueue *__cdecl Setup(CastContext *ctx)
	{
		QueueTimUpload(TexFile());
		TexBase() = layouts::MagicBuffer();
		Ctx() = ctx;
		Caster() = Entity(ctx->attacker);
		*(uint16_t *)Caster() &= 0xFFFB;
		InitTaskQueuePool(QMaster(), (void *)own::R(0x2508748), 0x10, 1);
		Memset32((uint8_t *)AddTaskToQueue(QMaster(), own::Fn(ORIG_MasterTask)) + 0xC, 0, 1);
		PlayCameraAnimation((const void *)fxp::camera(PU().res.camera, CAMERA_ANIM));
		return QMaster();
	}

	// ------------------------------------------------------------------
	// Feather (0x694360)
	// ------------------------------------------------------------------
	static uint32_t __cdecl FeatherTask(TaskNode *n)
	{
		FeatherNode *f = (FeatherNode *)n;
		// 30 fps layer: see mag227_ultimecia_first_death_held.inc
		FX_HELD(held_note_feather(n);)
		FX_PART(PART_FEATHERS);
		int32_t t[3];
		FeatherAt(CasterPoint(), t);
		int16_t sx, sy;
		FeatherScale(f->counter, sx, sy);
		FeatherDraw(FeatherFade(f->counter), sx, sy, f->ry, f->rz, t);
		if (f->counter >= PU().feathers.life) return TASK_END;
		f->ry = (int16_t)(f->ry + f->dry);
		f->rz = (int16_t)(f->rz + f->drz);
		f->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Main (0x693A10)
	// ------------------------------------------------------------------
	static void Play(MainNode *m, int which, uint8_t *layout, const int16_t *pos, int32_t depth)
	{
		PrimArg arg;
		arg.pos[0] = pos[0];
		arg.pos[1] = pos[1];
		arg.pos[2] = pos[2];
		arg.pos[3] = CallbackPad;
		arg.depth = depth;
		arg.morph = TexBase() + MORPH;
		// 30 fps layer: see mag227_ultimecia_first_death_held.inc
		FX_HELD(held_note_play(m, which, arg.pos, depth);)
		prim::play((prim::Layout *)layout, PrimCallback, (int)&arg, 0);
	}

	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const int32_t c = m->counter;
		int16_t pos[3];
		// layout A: at the caster point (0xD4..0x105)
		const int32_t a0 = PU().rise.tick, a1 = PU().rise.tick + PU().rise.hold;
		FX_PART(PART_RISE);
		if ((uint32_t)(c - a0) < (uint32_t)PU().rise.hold)
		{
			if (c == a0) DecodeModelPrimLayout(fxp::layout(PU().layouts.a, LAYOUT_A, 0x33C), m->layout_a, 0x33C);
			LayoutAPos(m, c, CasterPoint(), pos);
			Play(m, 0, m->layout_a, pos, -0x80);
		}
		// layout A: to the layout point (0x106..0x10D)
		if ((uint32_t)(c - a1) < 8)
		{
			if (c == a1)
			{
				*(uint32_t *)&m->from[0] = *(const uint32_t *)&CasterPoint()[0];
				*(uint32_t *)&m->from[2] = *(const uint32_t *)&CasterPoint()[2];
				const uint8_t *e = Caster();
				LayoutPoint()[1] = 0;
				LayoutPoint()[0] = *(const int16_t *)(e + 0x1C);
				LayoutPoint()[2] = (int16_t)(*(const int16_t *)(e + 0x20) + PU().ground.z);
			}
			LayoutAPos(m, c, CasterPoint(), pos);
			Play(m, 0, m->layout_a, pos, -0x80);
		}
		// layout B: by the caster (0x104..0x115)
		FX_PART(PART_BURST);
		if ((uint32_t)(c - PU().burst.tick) < (uint32_t)PU().burst.ticks)
		{
			if (c == PU().burst.tick) DecodeModelPrimLayout(fxp::layout(PU().layouts.b, LAYOUT_B, 0xC0), m->layout_b, 0xC0);
			const uint8_t *e = Caster();
			pos[0] = (int16_t)(*(const int16_t *)(e + 0x1C) + PU().burst.x);
			pos[1] = (int16_t)(*(const int16_t *)(e + 0x1E) + PU().burst.y);
			pos[2] = (int16_t)(*(const int16_t *)(e + 0x20) + PU().burst.z);
			Play(m, 1, m->layout_b, pos, -0x80);
		}
		// layout C: at the layout point (0x10C..0x11B)
		FX_PART(PART_GROUND);
		if ((uint32_t)(c - PU().ground.tick) < (uint32_t)PU().ground.ticks)
		{
			if (c == PU().ground.tick) DecodeModelPrimLayout(fxp::layout(PU().layouts.c, LAYOUT_C, 0x84), m->layout_c, 0x84);
			PrimArg arg;
			memcpy(arg.pos, LayoutPoint(), 8);
			arg.depth = 0;
			arg.morph = TexBase() + MORPH;
			// 30 fps layer: see mag227_ultimecia_first_death_held.inc
			FX_HELD(held_note_play(m, 2, arg.pos, 0);)
			prim::play((prim::Layout *)m->layout_c, PrimCallback, (int)&arg, 0);
		}
		// 30 fps layer: see mag227_ultimecia_first_death_held.inc
		FX_HELD(held_note_sparks();)
		FX_PART(PART_SPARKS);
		SparksDraw((const Spark *)(TexBase() + SPARKS));
		SparksUpdate();
		FX_PART(0);
		if (m->counter == 0) BdPlaySE((const void *)fxp::sound(PU().sounds.start, (uint32_t)SOUND_Start), 0, 0x80);
		else if (m->counter == PU().timing.fall) BdPlaySE((const void *)fxp::sound(PU().sounds.fall, (uint32_t)SOUND_Fall), 0, 0x80);
		if (m->counter == PU().timing.voice_end) ReleaseVoiceSlot(m->voice);
		if ((uint16_t)m->counter < (uint16_t)PU().sparks.first_end)
		{
			const int16_t *a = (const int16_t *)fxp::data(PU().tables.points_a, POINTS_A, 0x60 * 8);
			const int16_t *b = (const int16_t *)fxp::data(PU().tables.points_b, POINTS_B, 0x60 * 8);
			for (int i = PU().sparks.first_per; i > 0; i--)
			{
				SparkSpawn(a, 0x60);
				SparkSpawn(b, 0x60);
			}
		}
		if ((uint32_t)(m->counter - PU().sparks.second) < (uint32_t)PU().sparks.second_ticks)
		{
			const int16_t *t = (const int16_t *)fxp::data(PU().tables.points_c, POINTS_C, 0x77 * 8);
			for (int i = PU().sparks.second_per; i > 0; i--) SparkSpawn(t, 0x77);
		}
		if ((uint32_t)(m->counter - PU().feathers.tick) < (uint32_t)PU().feathers.ticks)
		{
			FeatherNode *f = (FeatherNode *)AddTaskToQueue(QFeathers(), own::Fn(ORIG_FeatherTask));
			if (f)
			{
				Memset32(&f->counter, 0, 3);
				f->rz = (int16_t)CrtRand();
				f->ry = (int16_t)CrtRand();
				f->dry = PU().feathers.spin_y;
				if (CrtRand() & 1) f->dry = (int16_t)-f->dry;
				f->drz = (int16_t)(-PU().feathers.spin_z - CrtRand() % PU().feathers.spin_z_rand);
			}
		}
		const int32_t slot = (int32_t)((uint32_t)Caster() - 0x1D972C0u) / 0x9C;
		const auto &L = PU().lines;
		if (m->counter == L.first) BattleText(slot, 0, L.first_ticks);
		else if (m->counter == L.second) BattleText(slot, 1, L.second_ticks);
		else if (m->counter == L.third) BattleText(slot, 2, L.third_ticks);
		else if (m->counter == L.fourth) BattleText(slot, 3, L.fourth_ticks);
		if (m->counter >= PU().timing.end)
		{
			*Caster() |= 4;
			return TASK_END;
		}
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Master (0x693840)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *ms = (MasterNode *)n;
		// 30 fps layer: see mag227_ultimecia_first_death_held.inc
		FX_HELD(held_note_master();)
		if (ms->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			PacketCursor2() = TexBase() + ARENA_B;
			ms->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			PacketCursor2() = TexBase() + ARENA_C;
			ms->arena = 1;
		}
		if (ms->counter == 1 && !ms->started)
		{
			ms->started = 1;
			ClearSparks();
			InitTaskQueuePool(QMain(), (void *)(TexBase() + POOL_Main), 0x49C, 1);
			InitTaskQueuePool(QFeathers(), (void *)TexBase(), 0x18, 0x10);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), own::Fn(ORIG_MainTask));
			Memset32(&m->counter, 0, 0x124);
			m->voice = ClaimVoiceSlot((const void *)fxp::sound(PU().sounds.voice, (uint32_t)SOUND_Voice), 1, 0x80);
		}
		int main = (int)n;
		if (ms->started)
		{
			ModelPoint((const int16_t *)fxp::data(PU().tables.caster_point, POINT_Caster, 8), CasterPoint());
			main = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QFeathers());
		}
		if (ms->started && main == 0)
		{
			// 30 fps layer: see mag227_ultimecia_first_death_held.inc
			FX_HELD(held_note_end();)
			return TASK_END;
		}
		ms->counter++;
		return 0;
	}
	namespace table
	{
		using fxp::Field;
		static const fxp::Part PARTS[] = {
			{ 0, "director", "Director", "The main task's timeline: the sounds and her voice, her four lines, its end; her point, the texture and the camera" },
			{ PART_RISE, "rise", "Rising light", "Layout A at her point, rising, then down to the ground point" },
			{ PART_BURST, "burst", "Burst", "Layout B by her" },
			{ PART_GROUND, "ground", "Ground", "Layout C at the ground point" },
			{ PART_SPARKS, "sparks", "Sparks", "Sparks at random points of her model, rising" },
			{ PART_FEATHERS, "feathers", "Feathers", "Feathers at her point, spinning, stretching, fading" },
		};
		static const fxp::Group GROUPS[] = {
			{ "sounds", "Sounds", "The start, the fall, her voice" },
			{ "lines", "Her lines", "Each line's tick and how long it shows" },
		};
		static const Field FIELDS[] = {
			{ "director", "sounds.start", "Start", "At the main task's tick 0", (uint32_t)offsetof(UltimeciaParams, sounds.start), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "sounds.fall", "Fall", "At the main task's fall tick", (uint32_t)offsetof(UltimeciaParams, sounds.fall), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "sounds.voice", "Voice", "Her voice: a voice slot claimed with it when the main task starts, released at its tick", (uint32_t)offsetof(UltimeciaParams, sounds.voice), fxp::Sound, 0, 0x7FFFFFFF },
			{ "director", "timing.fall", "Fall sound tick", "The fall sound at this tick of the main task", (uint32_t)offsetof(UltimeciaParams, timing.fall), fxp::I16, 0, 400 },
			{ "director", "timing.voice_end", "Voice end tick", "Her voice slot released at this tick", (uint32_t)offsetof(UltimeciaParams, timing.voice_end), fxp::I16, 0, 400 },
			{ "director", "timing.end", "End tick", "The main task's last tick (her flag bit 2 set back; feathers still alive then are dropped)", (uint32_t)offsetof(UltimeciaParams, timing.end), fxp::I16, 1, 400 },
			{ "director", "lines.first", "Line 1 tick", "Her first line (battle text 0 of her slot) at this tick", (uint32_t)offsetof(UltimeciaParams, lines.first), fxp::I16, 0, 400 },
			{ "director", "lines.first_ticks", "Line 1 ticks", "How long it shows", (uint32_t)offsetof(UltimeciaParams, lines.first_ticks), fxp::I16, 0, 400 },
			{ "director", "lines.second", "Line 2 tick", "Her second line at this tick", (uint32_t)offsetof(UltimeciaParams, lines.second), fxp::I16, 0, 400 },
			{ "director", "lines.second_ticks", "Line 2 ticks", "How long it shows", (uint32_t)offsetof(UltimeciaParams, lines.second_ticks), fxp::I16, 0, 400 },
			{ "director", "lines.third", "Line 3 tick", "Her third line at this tick", (uint32_t)offsetof(UltimeciaParams, lines.third), fxp::I16, 0, 400 },
			{ "director", "lines.third_ticks", "Line 3 ticks", "How long it shows", (uint32_t)offsetof(UltimeciaParams, lines.third_ticks), fxp::I16, 0, 400 },
			{ "director", "lines.fourth", "Line 4 tick", "Her fourth line at this tick", (uint32_t)offsetof(UltimeciaParams, lines.fourth), fxp::I16, 0, 400 },
			{ "director", "lines.fourth_ticks", "Line 4 ticks", "How long it shows", (uint32_t)offsetof(UltimeciaParams, lines.fourth_ticks), fxp::I16, 0, 400 },
			{ "director", "tables.caster_point", "Her point", "The point of her model the layout A and the feathers follow: x, y, z and the bone it is on (8 bytes)", (uint32_t)offsetof(UltimeciaParams, tables.caster_point), fxp::Data, 0, 0x7FFFFFFF },
			{ "director", "res.texture_name", "Texture file", "The texture file its loader reads from the magic folder (a name of at most 31 letters)", (uint32_t)offsetof(UltimeciaParams, res.texture_name), fxp::Name, 0, 0x7FFFFFFF },
			{ "director", "res.camera", "Camera track", "The camera track its setup plays (any length: its offsets are its own)", (uint32_t)offsetof(UltimeciaParams, res.camera), fxp::Camera, 0, 0x7FFFFFFF },
			{ "rise", "rise.tick", "Tick", "Layout A starts at this tick, at her point", (uint32_t)offsetof(UltimeciaParams, rise.tick), fxp::I16, 0, 400 },
			{ "rise", "rise.hold", "Ticks", "It stays there this many ticks, then goes to the ground point in 8 ticks", (uint32_t)offsetof(UltimeciaParams, rise.hold), fxp::I16, 1, 400 },
			{ "rise", "rise.lift", "Lift", "This much above her point (y up)", (uint32_t)offsetof(UltimeciaParams, rise.lift), fxp::I16, -4000, 4000 },
			{ "rise", "rise.rise_ticks", "Rise ticks", "Its last ticks there it rises", (uint32_t)offsetof(UltimeciaParams, rise.rise_ticks), fxp::I16, 0, 400 },
			{ "rise", "rise.rise", "Rise", "By this much a tick (from the lift, ending this much x (its last tick - its end) above her point)", (uint32_t)offsetof(UltimeciaParams, rise.rise), fxp::I16, -400, 400 },
			{ "rise", "layouts.a", "Layout A", "The prim-model layout rising from her (its models' animation)", (uint32_t)offsetof(UltimeciaParams, layouts.a), fxp::Layout, 0, 0x7FFFFFFF },
			{ "burst", "burst.tick", "Tick", "Layout B starts at this tick", (uint32_t)offsetof(UltimeciaParams, burst.tick), fxp::I16, 0, 400 },
			{ "burst", "burst.ticks", "Ticks", "It plays this many ticks", (uint32_t)offsetof(UltimeciaParams, burst.ticks), fxp::I16, 0, 400 },
			{ "burst", "burst.x", "X", "At her position + this x", (uint32_t)offsetof(UltimeciaParams, burst.x), fxp::I16, -8000, 8000 },
			{ "burst", "burst.y", "Y", "+ this y (negative = up)", (uint32_t)offsetof(UltimeciaParams, burst.y), fxp::I16, -8000, 8000 },
			{ "burst", "burst.z", "Z", "+ this z", (uint32_t)offsetof(UltimeciaParams, burst.z), fxp::I16, -8000, 8000 },
			{ "burst", "layouts.b", "Layout B", "The prim-model layout by her (its models' animation)", (uint32_t)offsetof(UltimeciaParams, layouts.b), fxp::Layout, 0, 0x7FFFFFFF },
			{ "ground", "ground.tick", "Tick", "Layout C starts at this tick, at the ground point", (uint32_t)offsetof(UltimeciaParams, ground.tick), fxp::I16, 0, 400 },
			{ "ground", "ground.ticks", "Ticks", "It plays this many ticks", (uint32_t)offsetof(UltimeciaParams, ground.ticks), fxp::I16, 0, 400 },
			{ "ground", "ground.z", "Z", "The ground point: her x, the floor, her z + this (taken when layout A leaves her)", (uint32_t)offsetof(UltimeciaParams, ground.z), fxp::I16, -8000, 8000 },
			{ "ground", "layouts.c", "Layout C", "The prim-model layout on the ground (its models' animation)", (uint32_t)offsetof(UltimeciaParams, layouts.c), fxp::Layout, 0, 0x7FFFFFFF },
			{ "sparks", "sparks.first_end", "First until", "From tick 0 to this one, sparks at points of tables A and B", (uint32_t)offsetof(UltimeciaParams, sparks.first_end), fxp::I16, 0, 400 },
			{ "sparks", "sparks.first_per", "First a tick", "This many a tick from each of the two tables (128 sparks at most alive)", (uint32_t)offsetof(UltimeciaParams, sparks.first_per), fxp::I16, 0, 64 },
			{ "sparks", "sparks.second", "Second tick", "From this tick sparks at points of table C", (uint32_t)offsetof(UltimeciaParams, sparks.second), fxp::I16, 0, 400 },
			{ "sparks", "sparks.second_ticks", "Second ticks", "For this many ticks", (uint32_t)offsetof(UltimeciaParams, sparks.second_ticks), fxp::I16, 0, 400 },
			{ "sparks", "sparks.second_per", "Second a tick", "This many a tick", (uint32_t)offsetof(UltimeciaParams, sparks.second_per), fxp::I16, 0, 64 },
			{ "sparks", "sparks.size", "Size", "A spark's sprite size", (uint32_t)offsetof(UltimeciaParams, sparks.size), fxp::I16, 0, 0x7FFF },
			{ "sparks", "sparks.life", "Life", "Its ticks (its flipbook played backward over them)", (uint32_t)offsetof(UltimeciaParams, sparks.life), fxp::I16, 1, 255 },
			{ "sparks", "sparks.rise", "Rise", "It rises 4 x (this + a random 0..3) a tick (and drifts 4 x -2..1 in x and z)", (uint32_t)offsetof(UltimeciaParams, sparks.rise), fxp::I16, -100, 100 },
			{ "sparks", "seqs.spark", "Spark", "The sprite sequence spark (a flipbook)", (uint32_t)offsetof(UltimeciaParams, seqs.spark), fxp::Seq, 0, 0x7FFFFFFF },
			{ "sparks", "tables.points_a", "Points A", "The first table of points of her model (96 points: x, y, z, bone; an edited one keeps its length)", (uint32_t)offsetof(UltimeciaParams, tables.points_a), fxp::Data, 0, 0x7FFFFFFF },
			{ "sparks", "tables.points_b", "Points B", "The second table (96 points)", (uint32_t)offsetof(UltimeciaParams, tables.points_b), fxp::Data, 0, 0x7FFFFFFF },
			{ "sparks", "tables.points_c", "Points C", "The third table (119 points)", (uint32_t)offsetof(UltimeciaParams, tables.points_c), fxp::Data, 0, 0x7FFFFFFF },
			{ "feathers", "feathers.tick", "Tick", "A feather a tick from this tick", (uint32_t)offsetof(UltimeciaParams, feathers.tick), fxp::I16, 0, 400 },
			{ "feathers", "feathers.ticks", "Ticks", "For this many ticks (16 at most alive)", (uint32_t)offsetof(UltimeciaParams, feathers.ticks), fxp::I16, 0, 400 },
			{ "feathers", "feathers.life", "Life", "A feather's last tick (it fades in over 3 ticks, out from its tick 10)", (uint32_t)offsetof(UltimeciaParams, feathers.life), fxp::I16, 0, 400 },
			{ "feathers", "feathers.height", "Height", "This much above her point (y up), following it", (uint32_t)offsetof(UltimeciaParams, feathers.height), fxp::I16, -4000, 4000 },
			{ "feathers", "feathers.spin_y", "Y spin", "Its turn about y a tick, either way at random", (uint32_t)offsetof(UltimeciaParams, feathers.spin_y), fxp::I16, -2048, 0x800, fxp::Angle },
			{ "feathers", "feathers.spin_z", "Z spin", "Its turn about z a tick: minus this minus a random below the next", (uint32_t)offsetof(UltimeciaParams, feathers.spin_z), fxp::I16, -2048, 0x800, fxp::Angle },
			{ "feathers", "feathers.spin_z_rand", "Z spin random", "(at least 1)", (uint32_t)offsetof(UltimeciaParams, feathers.spin_z_rand), fxp::I16, 1, 0x800, fxp::Angle },
			{ "feathers", "models.feather", "Feather model", "The prim model of a feather (a mesh: an edited one keeps its vertices)", (uint32_t)offsetof(UltimeciaParams, models.feather), fxp::Model, 0, 0x7FFFFFFF },
		};
	}
}

	void register_mag227_ultimecia_first_death()
	{
		fxp::register_params({ 227, "Ultimecia's first death", &ultimecia227::live, &ultimecia227::GAME, sizeof(ultimecia227::UltimeciaParams),
		                       ultimecia227::table::PARTS, (int)(sizeof(ultimecia227::table::PARTS) / sizeof(ultimecia227::table::PARTS[0])),
		                       ultimecia227::table::FIELDS, (int)(sizeof(ultimecia227::table::FIELDS) / sizeof(ultimecia227::table::FIELDS[0])),
		                       ultimecia227::table::GROUPS, (int)(sizeof(ultimecia227::table::GROUPS) / sizeof(ultimecia227::table::GROUPS[0])) });
		own::note(ultimecia227::ORIG_FileLoader, (void *)ultimecia227::FileLoader);
		own::note(ultimecia227::ORIG_Setup, (void *)ultimecia227::Setup);
		own::note(ultimecia227::ORIG_MasterTask, (void *)ultimecia227::MasterTask);
		register_port(ultimecia227::ORIG_MasterTask, (void *)ultimecia227::MasterTask, "227 MasterTask", 227);
		own::note(ultimecia227::ORIG_MainTask, (void *)ultimecia227::MainTask);
		register_port(ultimecia227::ORIG_MainTask, (void *)ultimecia227::MainTask, "227 MainTask", 227);
		own::note(ultimecia227::ORIG_FeatherTask, (void *)ultimecia227::FeatherTask);
		register_port(ultimecia227::ORIG_FeatherTask, (void *)ultimecia227::FeatherTask, "227 FeatherTask", 227);
		// 30 fps layer: see mag227_ultimecia_first_death_held.inc
		FX_HELD(register_mag227_held();)
	}
	FX_LAB_MODULE(register_mag227_ultimecia_first_death);
}

#ifdef FF8_FX_HELD
#include "mag227_ultimecia_first_death_held.inc"
#endif
