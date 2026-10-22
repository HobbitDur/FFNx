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

// Effect 112: Meteor Barret (Zell's Duel finisher; MAG_112_METEOR_BARRET_*). The Duel (effect 274) hands over to it when the
// player enters the finisher's input (BattleActionSequence_Tick_Duel 0x50BDC0 ends 274 with submode = the finisher, then
// casts this effect with Zell as the caster).
//
// Structure (setup 0x6CBBE0 -> _Init 0x6CBC10 (not ported: the setup: TIM upload, magic buffer 0x25218E8, caster, root queue,
// the column model's height span 0x25218F0 / 0x25218F2), file loader 0x6CBBF0 (the module file 0x12DB750)):
//   RootTask (0x6CBCF0) - node 0x1C (pool 0x25218F8): every tick the packet cursor alternates between the two arenas magic
//     buffer + 0x1E88 / + 0x11E88; on counter 1 (the counter runs 0..9 and wraps) the first time sets up the four queues
//     (director 1 x 0xF48 at the magic buffer, smoke 0x20 x 0x1C at + 0x1488, sparks 0x10 x 0x24 at + 0x1248, debris 0x10 x
//     0x30 at + 0xF48) and, while the next action index <= action 0's +0x11 (the last action), starts one DirectorTask on that
//     action's first target: Zell's flags +1 bit 0x10, his animation set bound (BS_BindEntityModelFromComData 0x5022C0, com
//     data 0x12C3618) and chain 1 queued (QueueChainTransformation 0x505C00), his facing turned to the target
//     (CartesianToGameAngle), the lift = target size - 400 (>= 0), his position saved and set to (0, ., 1000), his colours
//     +0x28 / +0x2C and +7 saved, the camera blob 0x12DAFA8 made current (0x5099A0: the battle's camera sequence plays it);
//     then runs the four queues and ends when they are all empty.
//   DirectorTask (0x6CBF90) - node 0xF48, counters 0..0x98 (UPDATE AND DRAW in one call per tick, the counter = the tick):
//     sounds at 2 / 0x49; screen flash every tick (0 -> 0x300 on 0..3, 0x400, 0x300 -> 0 on 0x95..0x98); Zell steps back at 7;
//     10..25: the ground point (his spawn point 0xF1 at 10) and the ground layout (0x12C7420, 11..25), Zell drifting forward
//     by the lift (11..17), sparks (8 a tick on 10 / 11), smoke (12 on 10), the rising column (prim model 0x12C5928 bent by
//     0x6CD7F0 into the magic buffer + 0x1808, spinning 0x40 a tick, stretched to the ground point's distance, fading from
//     17), debris (2 a tick on 12..17); 17..52: two layouts at the midpoint of his bones 0x15 / 0x16 (0x6CD8B0), screen tiles;
//     48..97: the fist layout (0x12CB40C) at bone 0x15, screen tiles, the target hidden (flags bit 8) at 52; 45..49: Zell
//     jumps up and onto the target; 69..133: Zell placed at the target (facing halved), the impact layouts (0x12D8D28 /
//     0x12D9DC0 at the impact point, 0x12D4684 a camera-space billboard at the target, 0x12D19F4 at the target from 107),
//     16 debris on 71, sparks on 110..113, a full-screen grey flat quad on 107, screen tiles; the target shown again and the
//     damage at 114 (target size >= 0xC00) / 119; 137..151: Zell's colours faded to black and back (0x56CC00 with the module's
//     zero colour 0x2521918), flags; 0x8F / 0x91: Zell hidden / put back at his saved position; 0x98: his colours and flags
//     restored, the end. The texture restore task (0x6CD870) starts at 0x89.
//   SmokeTask (0x6CD5C0, node 0x1C), SparkTask (0x6CD750, node 0x24): sprite sequences (DRAW THEN UPDATE; sparks move);
//   DebrisTask (0x6CD640, node 0x30): prim model 0x12C7230 spinning, falling and bouncing (DRAW THEN UPDATE; = mag117's
//     0x6C3440); TexRestoreTask (0x6CD870): Battle_RestoreDefaultEffectTexture, ends when it is done.
//   The prim-player callback 0x6CD280 draws one layout object with the per-layout flag table (bit 0x20000000: the block's
//   frame; 0x10000000: the offset through the view only; 0x40000000: pulled toward the camera by the block's radial word).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Writes to battle entities: Zell (caster): flags +0 (bit 0x1000 while the effect runs, 0x0C hidden on 0x8F..0x90, 0x800
//   cleared at the end), +1 bit 8 (colour override 137..151), position +0x1C / +0x1E / +0x20 (set at the start, stepped at 7,
//   drifted 11..17, jumped 45..49, placed at the target 69 (its +0x22 = the target anchor's 4th word), restored at 0x91),
//   facing +0x0E (start, halved at 69), colours +0x28..+0x2F / +7 (137..151, restored at 0x98), his animation set / chain
//   (start); the target: flags bit 8 (hidden 52..113/118); damage 0x506690 (one per action).
// Camera: no camera word is written; the camera blob 0x12DAFA8 (0x5099A0) is played by the battle's camera sequence
//   (BS_UpdateCameraSequence 0x509610 -> BS_GetCameraAnimationPointer 0x503520 in updateBattleCamera 0x504060). Draws read
//   the battle view 0x1D97778 themselves.
// Stack residue: one never-written local word lands in memory (the column delta's 4th word, copied into the debris nodes +0x16 on
//   12..17, never read): modelled as the task stack's zero fill (see GroundPhase).
// Module globals: 0x25218E0..0x2521970 (caster 0x25218E0, module file 0x25218E4, magic buffer 0x25218E8, context 0x25218EC,
//   column height top / span 0x25218F0 / 0x25218F2, root pool 0x25218F8, packet cursor 0x2521914, zero colour 0x2521918,
//   queues 0x2521920 debris / 0x2521930 sparks / 0x2521940 smoke / 0x2521950 director / 0x2521960 root).

#include "mag_common.h"

namespace ff8fx
{
namespace meteor112
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x25218E0); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x25218E8); }          // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25218EC); }
	inline int16_t &ColumnTop() { return var<int16_t>(0x25218F0); }          // highest vertex y of the column model (setup)
	inline int16_t &ColumnSpan() { return var<int16_t>(0x25218F2); }         // its height span (setup)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2521914); }
	inline const void *Black() { return (const void *)0x2521918; }           // never written: the zero colour
	inline TaskQueue *QDebris() { return (TaskQueue *)0x2521920; }          // debris + the texture restore (0x30)
	inline TaskQueue *QSparks() { return (TaskQueue *)0x2521930; }          // sparks (0x24)
	inline TaskQueue *QSmoke() { return (TaskQueue *)0x2521940; }           // smoke (0x1C)
	inline TaskQueue *QDirector() { return (TaskQueue *)0x2521950; }        // the director (1 x 0xF48)
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }

	static const uint32_t ORIG_RootTask = 0x6CBCF0;
	static const uint32_t ORIG_DirectorTask = 0x6CBF90;
	static const uint32_t ORIG_SmokeTask = 0x6CD5C0;
	static const uint32_t ORIG_DebrisTask = 0x6CD640;
	static const uint32_t ORIG_SparkTask = 0x6CD750;
	static const uint32_t ORIG_TexRestoreTask = 0x6CD870;

	static const uint32_t COM_Anim = 0x12C3618;         // com data of the module's animation set (bound to Zell)
	static const uint32_t SEQ_Smoke = 0x12C5650;        // sprite sequence (smoke, the first sparks)
	static const uint32_t SEQ_Spark = 0x12C5794;        // sprite sequence (the impact sparks)
	static const uint32_t MODEL_Column = 0x12C5928;     // the rising column (vertices bent into the magic buffer + 0x1808)
	static const uint32_t MODEL_Debris = 0x12C7230;
	static const uint32_t LAYOUT_Ground = 0x12C7420, LAYOUT_HandA = 0x12C8020, LAYOUT_HandB = 0x12CA7B4, LAYOUT_Fist = 0x12CB40C;
	static const uint32_t LAYOUT_Target = 0x12D19F4, LAYOUT_Billboard = 0x12D4684, LAYOUT_ImpactA = 0x12D8D28, LAYOUT_ImpactB = 0x12D9DC0;
	// the callback's per-object flag tables (one per layout; the two impact layouts share one)
	static const uint32_t FLAGS_Ground = 0x12DB65C, FLAGS_HandA = 0x12DB664, FLAGS_HandB = 0x12DB67C, FLAGS_Fist = 0x12DB6AC;
	static const uint32_t FLAGS_Impact = 0x12DB6D8, FLAGS_Billboard = 0x12DB6E0, FLAGS_Target = 0x12DB720;
	static const uint32_t CAMERA_Blob = 0x12DAFA8;      // camera blob (0x5099A0)
	static const int16_t *const SPARK_Dirs = (const int16_t *)0x12DB730; // 4 SVECTORs (the impact sparks, last first)
	static const void *const SOUND_Start = (const void *)0x12DB654;
	static const void *const SOUND_Hit = (const void *)0x12DB658;

	// layout sites (director node offsets of the decoded layouts)
	enum : int { SITE_GROUND = 0xB4, SITE_HAND_A = 0x174, SITE_HAND_B = 0x284, SITE_FIST = 0x5BC, SITE_TARGET = 0x90C,
		SITE_BILLBOARD = 0xA18, SITE_IMPACT_A = 0xD98, SITE_IMPACT_B = 0xE78 };

	// ---- engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t CartesianToGameAngle(int32_t x, int32_t z) { return fn<int32_t (__cdecl *)(int32_t, int32_t)>(0x56D160)(x, z); }
	inline int32_t SqrtInt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); } // Math_SqrtInt
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline uint32_t BindExternalAnimSet(uint8_t *entity, void *out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, void *, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline void PlayCameraBlob(uint32_t blob) { fn<void (__cdecl *)(uint32_t)>(0x5099A0)(blob); } // Battle_PlayCameraAnimation (BS_Camera_SetCurrentBlob)
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void MorphVertices(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x6D5580)(model, f0, f1, t, out); } // Effect_MorphVertices
	inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); } // ColourLerp
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }      // IR = MAC = R * V0 (no translation)
	inline void ScreenFadeTask(int32_t a, int32_t b, int32_t c, int32_t d) { fn<void (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0x5712C0)(a, b, c, d); } // Effect_SpawnScreenFadeTask

#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x1C bytes at 0x25218F8
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..9 (wraps)
		uint8_t action;    // +0x0E next action whose director starts
		uint8_t started;   // +0x0F the queues are set up
		uint32_t arena;    // +0x10 packet arena parity
		uint32_t pad14[2];
	};
	struct DirectorNode // pool of 1 node of 0xF48 bytes (magic buffer)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C the tick (0..0x98)
		int16_t action;    // +0x0E the action index (the target / its damage)
		uint8_t bind[0x20];// +0x10 Zell's bind block (BS_BindEntityModelFromComData; 0x10 bytes without a weapon model)
		uint32_t colour_a; // +0x30 Zell's +0x28..+0x2B (start)
		uint32_t colour_b; // +0x34 Zell's +0x2C..+0x2E, its byte +0x37 = Zell's +7
		uint32_t pad38;
		uint32_t saved_pos[2]; // +0x3C Zell's +0x1C..+0x23 (start)
		int16_t land[4];   // +0x44 his landing point (45)
		int16_t ground[4]; // +0x4C his spawn point 0xF1 (10)
		int16_t fist[4];   // +0x54 his bone 0x15 (48..97)
		int16_t anchor[4]; // +0x5C the target's default effect position (69)
		int16_t impact[4]; // +0x64 the impact point (69)
		Mat4x3 fist_m;     // +0x6C the fist layout's frame (rotation at 48, translation = bone 0x15 every tick)
		Mat4x3 impact_m;   // +0x8C the impact frame (yaw = his halved facing at 69)
		int32_t lift;      // +0xAC the target's size - 400 (>= 0)
		uint8_t *target;   // +0xB0
		uint8_t ground_l[0xC0];   // +0xB4  layout 0x12C7420
		uint8_t hand_a[0x110];    // +0x174 layout 0x12C8020
		uint8_t hand_b[0x338];    // +0x284 layout 0x12CA7B4
		uint8_t fist_l[0x350];    // +0x5BC layout 0x12CB40C
		uint8_t target_l[0x10C];  // +0x90C layout 0x12D19F4
		uint8_t billboard[0x380]; // +0xA18 layout 0x12D4684
		uint8_t impact_a[0xE0];   // +0xD98 layout 0x12D8D28
		uint8_t impact_b[0xD0];   // +0xE78 layout 0x12D9DC0
	};
	// the prim-player callback's parameter block (part of the director's stack frame)
	struct PlayArg
	{
		int16_t pos[3];    // +0x00 offset added to the object's (tables without 0x20000000)
		int16_t radial;    // +0x06 pull toward the camera (tables with 0x40000000)
		const Mat4x3 *m;   // +0x08 the frame (tables with 0x20000000)
		const uint32_t *flags; // +0x0C the per-object flag table
		uint32_t morph;    // +0x10 blended vertex frames
	};
	// the director's stack locals (0x44 bytes from its frame base; offsets as the original's esp + 0x10 after its four pushes)
	struct DirLocals
	{
		int16_t delta[4];  // +0x00 the column's spawn point minus the ground point (+0x06 never written: debris +0x16); a
		                   //        dword: the spark direction cursor (110..113) / |spread| (71)
		int16_t spawn[4];  // +0x08 his spawn point 0xF1 (10..25) / the rand angles (110..113) / a loop counter (71)
		PlayArg arg;       // +0x10 (+0x14..+0x19 also the fist layout's angles on 48)
		Mat4x3 m;          // +0x24 .. +0x43 (the frame / matrix local)
	};
	struct SmokeNode // 0x1C
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C frame
		int16_t end;       // +0x0E
		int16_t pos[3];    // +0x10
		int16_t size;      // +0x16
		uint32_t seq;      // +0x18
	};
	struct SparkNode // 0x24
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C frame
		int16_t end;       // +0x0E
		uint32_t seq;      // +0x10
		int16_t pos[3];    // +0x14
		int16_t size;      // +0x1A
		int16_t vel[3];    // +0x1C
		int16_t depth;     // +0x22 (the shadow rotation's push: -depth / 4)
	};
	struct DebrisNode // 0x30
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C life (0x1E)
		int16_t pad0E;
		int16_t pos[3];    // +0x10
		int16_t pad16;     // +0x16 (the spawn's never-written stack word on 12..17)
		int16_t vel[3];    // +0x18
		int16_t pad1E;
		int16_t rot[3];    // +0x20
		int16_t pad26;
		int16_t spin[3];   // +0x28
		int16_t pad2E;
	};
	struct TexRestoreNode // 0x30 (in the debris queue)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t done;      // +0x0E set by the restore task
		uint8_t pad[0x20];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x1C && sizeof(DirectorNode) == 0xF48, "Meteor Barret nodes");
	static_assert(sizeof(PlayArg) == 0x14 && sizeof(DirLocals) == 0x44, "Meteor Barret blocks");
	static_assert(sizeof(SmokeNode) == 0x1C && sizeof(SparkNode) == 0x24 && sizeof(DebrisNode) == 0x30 && sizeof(TexRestoreNode) == 0x30, "Meteor Barret particle nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag112_meteor_barret_held.h"
#endif

namespace ff8fx
{
namespace meteor112
{
	using namespace eng;
	using namespace magc;

	inline int16_t &W16(void *p, int off) { return *(int16_t *)((uint8_t *)p + off); }
	inline uint16_t &U16(void *p, int off) { return *(uint16_t *)((uint8_t *)p + off); }
	inline int32_t &W32(void *p, int off) { return *(int32_t *)((uint8_t *)p + off); }
	inline uint32_t &U32(void *p, int off) { return *(uint32_t *)((uint8_t *)p + off); }
	inline uint8_t &B8(void *p, int off) { return *((uint8_t *)p + off); }

	// the first target record of action `i` (its +0 = the target slot; ApplyActionResultToTarget)
	inline uint8_t *ActionTargets(int i) { return Ctx()->actions[i].targets; }

	// ------------------------------------------------------------------
	// Small helpers
	// ------------------------------------------------------------------
	// 0x6D9510 (Effect_BuildYRotationMatrix, a pure helper of MAG_104's range): the yaw rotation of the int16 angle
	static void YawMatrix(int32_t angle, Mat4x3 *m)
	{
		const int32_t a = (int16_t)angle;
		const int32_t s = ComputeSin(a);
		const int32_t c = ComputeCos(a);
		m->m[0][2] = (int16_t)s;
		m->m[2][0] = (int16_t)-s;
		m->m[0][0] = (int16_t)c;
		m->m[0][1] = 0;
		m->m[1][0] = 0;
		m->m[1][1] = 0x1000;
		m->m[1][2] = 0;
		m->m[2][1] = 0;
		m->m[2][2] = (int16_t)c;
	}

	// 0x6A83B0 (MAG_140_ScreenTile, a pure helper of Phoenix's range): full-screen flat semi-transparent tile (GP0 0x2A, OT
	// bucket 8 through the alternate-viewport insert), colour (r, g, b) * (d - n) / d; nothing when n >= d
	static uint32_t ScreenTile(int32_t r, int32_t g, int32_t b, int32_t n, int32_t d, uint32_t cursor)
	{
		const int32_t k = d - n;
		if (k <= 0) return cursor;
		// 30 fps layer: see mag112_meteor_barret_held.inc
		FX_HELD(held_note_tile(r, g, b, n, d);)
		uint8_t *p = (uint8_t *)cursor;
		W16(p, 8) = 0;
		W16(p, 0x10) = 0;
		W16(p, 0xE) = 0;
		W16(p, 0xA) = 0;
		W16(p, 0x14) = 0x140;
		W16(p, 0x16) = 0xD8;
		W16(p, 0x12) = 0xD8;
		W16(p, 0xC) = 0x140;
		U32(p, 0) = 0x05000000;
		p[7] = 0x2A;
		p[4] = (uint8_t)(mul32(r, k) / d);
		p[5] = (uint8_t)(mul32(g, k) / d);
		p[6] = (uint8_t)(mul32(b, k) / d);
		InsertPrimAltViewport(OTBase() + 0x20, p);
		return cursor + 0x18;
	}

	// 0x6CD7F0: the column's vertices through the GTE rotation / translation into out, each z pulled by
	// len * (1 + sin(sin(((top - y) * 1024 / span) - 1024) / 4)) / 4096 (the 4th words of out kept)
	static void ColumnBend(uint32_t model, int16_t *out, int32_t len)
	{
		int32_t count = *(const int32_t *)(model + 4);
		const uint8_t *v = (const uint8_t *)(model + 8);
		const int32_t l = (int16_t)len;
		do
		{
			GteLoadV0(v);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(out);
			const int32_t a = shl32((int32_t)ColumnTop() - (int32_t)out[1], 10) / (int32_t)ColumnSpan() + (int32_t)0xFFFFFC00;
			const int32_t s = ComputeSin(ComputeSin(a) >> 2) + 0x1000;
			out[2] = (int16_t)(out[2] - (int16_t)(mul32(s, l) >> 12));
			v += 8;
			out += 4;
		} while (--count != 0);
	}

	// 0x6CD8B0: the midpoint of an entity's spawn points 0x15 and 0x16 (out: x, y, z; the local's 4th word unused)
	static void HandsMidpoint(uint8_t *e, int16_t *out)
	{
		int16_t a[4];
		GetEffectSpawnPosition(e, 0x15, 0, a);
		GetEffectSpawnPosition(e, 0x16, 0, out);
		out[0] = (int16_t)(((int32_t)out[0] + (int32_t)a[0]) >> 1);
		out[1] = (int16_t)(((int32_t)out[1] + (int32_t)a[1]) >> 1);
		out[2] = (int16_t)(((int32_t)out[2] + (int32_t)a[2]) >> 1);
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x6CD280): one object of a layout
	// ------------------------------------------------------------------
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PlayArg *arg = (const PlayArg *)arg_;
		const int32_t idx = (int16_t)r->index;
		if (arg->flags[idx] & 0x80000000u) return;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		const int32_t object = (int16_t)r->flags_lo;
		uint8_t *model = l->data + *(const int32_t *)(l->data + object * 4 + 8);
		*(uint8_t **)h = model;
		const int16_t f0 = r->b0, f1 = r->b1;
		if (f0 == f1)
		{
			if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
			else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(*(const int32_t *)(model + 4), f0) * 8 + 0xC;
		}
		else
		{
			const int16_t t = r->b;
			if (t == 0)
			{
				if (f0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(*(const int32_t *)(model + 4), f0) * 8 + 0xC;
			}
			else if (t == 0x1000)
			{
				if (f1 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(*(const int32_t *)(model + 4), f1) * 8 + 0xC;
			}
			else
			{
				MorphVertices((uint32_t)model, f0, f1, t, arg->morph);
				*(uint32_t *)(h + 4) = arg->morph;
			}
		}
		Mat4x3 m;
		int32_t n[3];
		ComposeZYXRotationMatrix(r->rot, &m);
		if (arg->flags[idx] & 0x20000000u)
		{
			// the block's frame
			m.t[0] = r->pos[0];
			m.t[1] = r->pos[1];
			m.t[2] = r->pos[2];
			ComposeAffineTransform(arg->m, &m, &m);
		}
		else
		{
			m.t[0] = (int32_t)r->pos[0] + (int32_t)arg->pos[0];
			m.t[1] = (int32_t)r->pos[1] + (int32_t)arg->pos[1];
			m.t[2] = (int32_t)r->pos[2] + (int32_t)arg->pos[2];
			if (arg->flags[idx] & 0x10000000u)
			{
				// the offset through the view, the rotation not composed with it
				TransformVectorBy3x3Matrix(&Camera(), m.t, m.t);
				m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)Camera().t[0]);
				m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)Camera().t[1]);
				m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)Camera().t[2]);
			}
			else ComposeAffineTransform(&Camera(), &m, &m);
		}
		if (arg->flags[idx] & 0x40000000u)
		{
			// pulled along the view direction by the block's radial word (4.13)
			NormalizeVector(m.t, n);
			const int32_t rad = arg->radial;
			m.t[0] = m.t[0] - (mul32(rad, n[0]) >> 13);
			m.t[1] = m.t[1] - (mul32(rad, n[1]) >> 13);
			m.t[2] = m.t[2] - (mul32(rad, n[2]) >> 13);
		}
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			n[0] = r->scale[0];
			n[1] = r->scale[1];
			n[2] = r->scale[2];
			Scale3DMatrix(&m, n);
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)(h + 0x1C) = (arg->flags[idx] & 0xDFFF) | 0x2000;
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			B8(h, 0x1C) |= 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Smoke (0x6CD5C0), sparks (0x6CD750): sprite sequences; debris (0x6CD640): prim model. DRAW THEN UPDATE
	// ------------------------------------------------------------------
	static void SmokeDraw(const SmokeNode *s)
	{
		// 30 fps layer: see mag112_meteor_barret_held.inc
		FX_HELD(held_note_smoke(s);)
		TransformCameraByShadowRotation(s->pos, s->size, -((int32_t)s->size >> 2));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint32_t *)h = s->seq;
		W16(h, 4) = s->counter;
		W16(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SmokeTask(TaskNode *n)
	{
		SmokeNode *s = (SmokeNode *)n;
		SmokeDraw(s);
		s->counter++;
		return s->counter < s->end ? 0 : TASK_END;
	}

	static void SparkDraw(const SparkNode *s)
	{
		// 30 fps layer: see mag112_meteor_barret_held.inc
		FX_HELD(held_note_spark(s);)
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(s->pos, s->size, -((int32_t)s->depth >> 2));
		*(uint32_t *)h = s->seq;
		W16(h, 4) = s->counter;
		W16(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0xB4);
	}

	static void SparkMove(SparkNode *s)
	{
		s->pos[0] = (int16_t)(s->pos[0] + s->vel[0]);
		s->pos[1] = (int16_t)(s->pos[1] + s->vel[1]);
		s->pos[2] = (int16_t)(s->pos[2] + s->vel[2]);
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SparkNode *s = (SparkNode *)n;
		SparkDraw(s);
		SparkMove(s);
		s->counter++;
		return s->counter < s->end ? 0 : TASK_END;
	}

	static void DebrisDraw(const DebrisNode *d)
	{
		// 30 fps layer: see mag112_meteor_barret_held.inc
		FX_HELD(held_note_debris(d);)
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		Mat4x3 m;
		BuildRotationMatrixFromAngles(d->rot, &m);
		m.t[0] = d->pos[0];
		m.t[1] = d->pos[1];
		m.t[2] = d->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		*(uint32_t *)h = MODEL_Debris;
		*(uint32_t *)(h + 0x1C) = 0;
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
	}

	// spin, gravity 10, motion; a bounce on the ground (y > 0) with a quarter of the speed
	static void DebrisMove(DebrisNode *d)
	{
		d->rot[0] = (int16_t)(d->rot[0] + d->spin[0]);
		d->rot[2] = (int16_t)(d->rot[2] + d->spin[2]);
		d->vel[1] = (int16_t)(d->vel[1] + 0xA);
		d->rot[1] = (int16_t)(d->rot[1] + d->spin[1]);
		d->pos[0] = (int16_t)(d->pos[0] + d->vel[0]);
		d->pos[1] = (int16_t)(d->pos[1] + d->vel[1]);
		d->pos[2] = (int16_t)(d->pos[2] + d->vel[2]);
		if (d->pos[1] > 0)
		{
			const int16_t v = d->vel[1];
			d->pos[1] = 0;
			d->vel[1] = (int16_t)((int16_t)(v >> 2) - v);
		}
	}

	static uint32_t __cdecl DebrisTask(TaskNode *n)
	{
		DebrisNode *d = (DebrisNode *)n;
		DebrisDraw(d);
		DebrisMove(d);
		d->counter++;
		return d->counter < 0x1E ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Texture restore (0x6CD870, a node of the debris queue)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TexRestoreTask(TaskNode *n)
	{
		TexRestoreNode *w = (TexRestoreNode *)n;
		if (w->counter == 0) TextureRestoreTask((void *)(TexBase() + 0x1E88), &w->done);
		const int16_t done = w->done;
		w->counter++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// The director (0x6CBF90): the whole finisher, one call per tick (counter = the tick)
	// ------------------------------------------------------------------
	static void PlayLayout(DirectorNode *d, int site, PlayArg *arg)
	{
		// 30 fps layer: see mag112_meteor_barret_held.inc
		FX_HELD(held_note_layout(d, site, arg);)
		prim::play((prim::Layout *)((uint8_t *)d + site), PartCallback, (int)arg, 0);
	}

	// the 4th word of the column delta local (entry esp - 0x3E) is never written by the director: it is copied into the debris
	// nodes +0x16 on 12..17 and never read. No code writes that stack word during a tick before the director (breakpoint reads
	// under the game executor: the task stack's tick-start fill), so the port writes the zero fill
	static const int16_t PAD_DEBRIS = 0;

	// 0x6CC199..0x6CC5A2 (10..25): sparks, smoke, the column, debris
	static void GroundPhase(DirectorNode *d, DirLocals &L, int32_t k)
	{
		uint8_t *caster = Caster();
		GetEffectSpawnPosition(caster, 0xF1, 0, L.spawn);
		if ((uint32_t)k < 2)
		{
			int32_t angle = CrtRand();
			for (int32_t i = 8; i != 0; i--, angle += 0x200)
			{
				SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
				if (!s) continue;
				Memset32(&s->counter, 0, 6);
				s->seq = SEQ_Smoke;
				s->end = 0xC;
				s->depth = 0x1000;
				s->size = 0x1000;
				s->pos[0] = W16(caster, 0x1C);
				s->pos[1] = W16(caster, 0x24);
				s->pos[2] = W16(caster, 0x20);
				s->vel[0] = (int16_t)(ComputeCos(angle) >> 3);
				s->vel[2] = (int16_t)(ComputeSin(angle) >> 3);
				const int32_t r = CrtRand() & 0xFFF;
				const int32_t f = r + (0x400 - (r >> 2));
				s->vel[0] = (int16_t)(mul32(s->vel[0], f) >> 14);
				s->vel[2] = (int16_t)(mul32(s->vel[2], f) >> 14);
			}
		}
		if (k == 0)
		{
			for (int32_t i = 0xC; i != 0; i--)
			{
				SmokeNode *s = (SmokeNode *)AddTaskToQueue(QSmoke(), ORIG_SmokeTask);
				if (!s) continue;
				Memset32(&s->counter, 0, 4);
				memcpy(s->pos, L.spawn, 8); // (x, y, z and the local's 4th word, overwritten by the size)
				s->pos[0] = (int16_t)(s->pos[0] + (int16_t)((CrtRand() & 0x1FF) - 0x100));
				s->pos[2] = (int16_t)(s->pos[2] + (int16_t)((CrtRand() & 0x1FF) - 0x100));
				const int32_t r = CrtRand() & 0x7FF;
				s->seq = SEQ_Smoke;
				s->end = 0xC;
				s->size = (int16_t)(r + 0x1000);
			}
			return;
		}
		// the column: its vertices turned by k * 0x40 and bent by the ground point's distance, drawn upright at the ground point
		L.delta[0] = (int16_t)(L.spawn[0] - d->ground[0]);
		L.delta[1] = (int16_t)(L.spawn[1] - d->ground[1]);
		L.delta[2] = (int16_t)(L.spawn[2] - d->ground[2]);
		// 30 fps layer: see mag112_meteor_barret_held.inc
		FX_HELD(held_note_column(d, k, L.spawn);)
		YawMatrix(k << 6, &L.m);
		GteSetRotMatrix(&L.m);
		L.m.t[2] = 0;
		L.m.t[1] = 0;
		L.m.t[0] = 0;
		GteSetTransVector(&L.m);
		int32_t len = SqrtInt(mul32(L.delta[0], L.delta[0]) + mul32(L.delta[2], L.delta[2]));
		if (L.delta[2] > 0) len = -len;
		ColumnBend(MODEL_Column, (int16_t *)(TexBase() + 0x1808), len);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		YawMatrix(W16(caster, 0xE), &L.m);
		L.m.m[1][1] = (int16_t)shl32(-*(const int32_t *)&L.delta[1], 2);
		L.m.t[0] = d->ground[0];
		L.m.t[1] = W16(caster, 0x24);
		L.m.t[2] = d->ground[2];
		ComposeAffineTransform(&Camera(), &L.m, &L.m);
		GteSetRotMatrix(&L.m);
		GteSetTransVector(&L.m);
		*(uint32_t *)h = MODEL_Column;
		*(uint32_t *)(h + 4) = TexBase() + 0x1808;
		if ((uint32_t)k >= 7)
		{
			B8(h, 0xA) = 0;
			B8(h, 9) = 0;
			B8(h, 8) = 0;
			*(uint32_t *)(h + 0xC) = (uint32_t)(shl32(k, 9) - 0xC01) & 0x1FFFFE00u;
			*(uint32_t *)(h + 0x1C) = 0x20F3;
		}
		else *(uint32_t *)(h + 0x1C) = 0x2033;
		PacketCursor() = RenderPrimModel(h, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x58);
		// debris (12..17) at the column delta (its 4th word: the never-written local word)
		if ((uint32_t)k < 2 || (uint32_t)k > 7) return;
		L.delta[3] = PAD_DEBRIS;
		const uint32_t dxy = *(const uint32_t *)&L.delta[0], dz = *(const uint32_t *)&L.delta[2];
		for (int32_t i = 2; i != 0; i--)
		{
			DebrisNode *p = (DebrisNode *)AddTaskToQueue(QDebris(), ORIG_DebrisTask);
			if (!p) continue;
			Memset32(&p->counter, 0, 9);
			*(uint32_t *)&p->pos[0] = dxy;
			*(uint32_t *)&p->pos[2] = dz;
			p->vel[0] = (int16_t)((CrtRand() & 0x1F) + 0x20);
			if (CrtRand() & 1) p->vel[0] = (int16_t)-p->vel[0];
			p->vel[2] = (int16_t)((CrtRand() & 0x1F) + 0x20);
			if (CrtRand() & 1) p->vel[2] = (int16_t)-p->vel[2];
			p->rot[0] = (int16_t)CrtRand();
			p->rot[1] = (int16_t)CrtRand();
			p->rot[2] = (int16_t)CrtRand();
			p->spin[0] = (int16_t)((CrtRand() & 0x7F) - 0x40);
			p->spin[1] = (int16_t)((CrtRand() & 0x7F) - 0x40);
			p->spin[2] = (int16_t)((CrtRand() & 0x7F) - 0x40);
		}
	}

	// 0x6CC81B..0x6CCF83 (69..133): the impact
	static void ImpactPhase(DirectorNode *d, DirLocals &L, int32_t e)
	{
		uint8_t *caster = Caster();
		if (e == 0x26)
		{
			// a full-screen grey flat quad (GP0 0x2A, 0x808080) between additive / subtractive draw modes, OT bucket 8
			uint8_t *p = (uint8_t *)PacketCursor();
			PacketCursor() += 0x18;
			W16(p, 0x14) = 0x140;
			W16(p, 0xC) = 0x140;
			W16(p, 8) = 0;
			W16(p, 0x10) = 0;
			W16(p, 0xE) = 0;
			W16(p, 0xA) = 0;
			W16(p, 0x16) = 0xD8;
			W16(p, 0x12) = 0xD8;
			p[6] = 0x80;
			p[5] = 0x80;
			p[4] = 0x80;
			// 30 fps layer: see mag112_meteor_barret_held.inc
			FX_HELD(held_note_flat();)
			uint32_t *q = (uint32_t *)PacketCursor();
			PacketCursor() += 8;
			q[0] = 0x01000000;
			q[1] = 0xE1000220;
			InsertPrimAutoDepth(OTBase() + 0x20, q);
			U32(p, 0) = 0x05000000;
			p[7] = 0x2A;
			InsertPrimAltViewport(OTBase() + 0x20, p);
			q = (uint32_t *)PacketCursor();
			PacketCursor() += 8;
			q[0] = 0x01000000;
			q[1] = 0xE1000240;
			InsertPrimAutoDepth(OTBase() + 0x20, q);
		}
		else if (e == 0x27) ScreenFadeTask(0, 1, 1, 0xC0);
		if ((uint32_t)e >= 0x10) PacketCursor() = ScreenTile(0x28, 0x37, 0x32, e - 0x10, 3, PacketCursor());
		if ((uint32_t)e >= 0x20) PacketCursor() = ScreenTile(0x28, 0x37, 0x32, e - 0x20, 3, PacketCursor());
		if (e == 0)
		{
			DecodeModelPrimLayout(LAYOUT_Target, d->target_l, 0x10C);
			DecodeModelPrimLayout(LAYOUT_Billboard, d->billboard, 0x380);
			DecodeModelPrimLayout(LAYOUT_ImpactA, d->impact_a, 0xE0);
			DecodeModelPrimLayout(LAYOUT_ImpactB, d->impact_b, 0xD0);
			// Zell at the target: facing halved, his position = the target's anchor minus (size + 5000) / 4 along it
			int32_t f = W16(caster, 0xE);
			if (f > 0x800) f -= 0x1000;
			W16(caster, 0xE) = (int16_t)(f >> 1);
			YawMatrix(W16(caster, 0xE), &d->impact_m);
			GetDefaultEffectPosition(d->target, d->anchor);
			U32(caster, 0x1C) = *(const uint32_t *)&d->anchor[0];
			U32(caster, 0x20) = *(const uint32_t *)&d->anchor[2];
			W16(caster, 0x1C) = (int16_t)(W16(caster, 0x1C) - (int16_t)(mul32(ComputeSin(W16(caster, 0xE)), (int32_t)W16(d->target, 0x26) + 0x1388) >> 14));
			W16(caster, 0x20) = (int16_t)(W16(caster, 0x20) - (int16_t)(mul32(ComputeCos(W16(caster, 0xE)), (int32_t)W16(d->target, 0x26) + 0x1388) >> 14));
			W16(caster, 0x1E) = 0;
			d->impact[0] = (int16_t)(W16(caster, 0x1C) - (int16_t)(ComputeSin(W16(caster, 0xE)) >> 2));
			const int32_t c = ComputeCos(W16(caster, 0xE));
			d->impact[1] = 0;
			d->impact[2] = (int16_t)(W16(caster, 0x20) - (int16_t)(c >> 2));
		}
		d->impact_m.t[0] = d->impact[0];
		d->impact_m.t[1] = d->impact[1];
		d->impact_m.t[2] = d->impact[2];
		ComposeAffineTransform(&Camera(), &d->impact_m, &L.m);
		L.arg.m = &L.m;
		L.arg.flags = (const uint32_t *)FLAGS_Impact;
		L.arg.morph = TexBase() + 0x1808;
		PlayLayout(d, SITE_IMPACT_A, &L.arg);
		if ((uint32_t)e >= 0x29) PlayLayout(d, SITE_IMPACT_B, &L.arg);
		// the billboard: the target's anchor through the view, unrotated (the frame's last rotation element kept)
		GteSetRotMatrixCtrl(&Camera());
		L.m.m[1][1] = 0x1000;
		L.m.m[0][0] = 0x1000;
		L.m.m[2][1] = 0;
		L.m.m[2][0] = 0;
		L.m.m[1][2] = 0;
		L.m.m[1][0] = 0;
		L.m.m[0][2] = 0;
		L.m.m[0][1] = 0;
		GteLoadV0(d->anchor);
		GteMVMVA_RotV0();
		GteReadMAC123(L.m.t);
		L.m.t[0] = (int32_t)((uint32_t)L.m.t[0] + (uint32_t)Camera().t[0]);
		L.m.t[1] = (int32_t)((uint32_t)L.m.t[1] + (uint32_t)Camera().t[1]);
		L.m.t[2] = (int32_t)((uint32_t)L.m.t[2] + (uint32_t)Camera().t[2]);
		L.arg.radial = (int16_t)(d->lift + 0xC8);
		L.arg.m = &L.m;
		L.arg.flags = (const uint32_t *)FLAGS_Billboard;
		L.arg.morph = TexBase() + 0x1808;
		PlayLayout(d, SITE_BILLBOARD, &L.arg);
		if (e == 2)
		{
			// 16 debris thrown back from the impact point
			for (*(int32_t *)&L.spawn[0] = 0x10; *(int32_t *)&L.spawn[0] != 0; (*(int32_t *)&L.spawn[0])--)
			{
				DebrisNode *p = (DebrisNode *)AddTaskToQueue(QDebris(), ORIG_DebrisTask);
				if (!p) continue;
				int32_t spread = CrtRand();
				spread -= CrtRand();
				spread -= CrtRand();
				spread += CrtRand();
				Memset32(&p->counter, 0, 9);
				spread >>= 1;
				*(int32_t *)&L.delta[0] = spread;
				if (spread < 0) *(int32_t *)&L.delta[0] = -spread;
				const int32_t speed = 0x9C40 - *(const int32_t *)&L.delta[0];
				const int32_t a = spread + W16(caster, 0xE);
				const int32_t s = -ComputeSin(a);
				p->vel[0] = (int16_t)s;
				p->pos[0] = (int16_t)s;
				const int32_t c = -ComputeCos(a);
				p->vel[2] = (int16_t)c;
				p->pos[2] = (int16_t)c;
				p->pos[0] = (int16_t)((int16_t)(p->pos[0] >> 4) + d->impact[0]);
				p->pos[2] = (int16_t)((int16_t)((int16_t)c >> 4) + d->impact[2]);
				p->pos[1] = 0;
				p->vel[0] = (int16_t)(mul32(p->vel[0], speed) >> 0x14);
				p->vel[2] = (int16_t)(mul32((int16_t)c, speed) >> 0x14);
				p->vel[1] = (int16_t)(-mul32(CrtRand(), speed >> 2) >> 0x14);
				p->rot[0] = (int16_t)CrtRand();
				p->rot[1] = (int16_t)CrtRand();
				p->rot[2] = (int16_t)CrtRand();
				p->spin[0] = (int16_t)((CrtRand() & 0x7F) - 0x40);
				p->spin[1] = (int16_t)((CrtRand() & 0x7F) - 0x40);
				p->spin[2] = (int16_t)((CrtRand() & 0x7F) - 0x40);
			}
		}
		else
		{
			if ((uint32_t)e >= 0x26)
			{
				d->impact_m.t[0] = d->anchor[0];
				d->impact_m.t[1] = d->anchor[1];
				d->impact_m.t[2] = d->anchor[2];
				ComposeAffineTransform(&Camera(), &d->impact_m, &L.m);
				L.arg.radial = (int16_t)(d->lift + 0xC8);
				L.arg.m = &L.m;
				L.arg.flags = (const uint32_t *)FLAGS_Target;
				L.arg.morph = TexBase() + 0x1808;
				PlayLayout(d, SITE_TARGET, &L.arg);
			}
			if ((uint32_t)e >= 0x29 && (uint32_t)e < 0x2D)
			{
				// 4 sparks from the target's anchor along the rand-turned direction table
				L.spawn[0] = (int16_t)CrtRand();
				L.spawn[1] = (int16_t)CrtRand();
				L.spawn[2] = (int16_t)CrtRand();
				BuildRotationMatrixFromAngles(L.spawn, &L.m);
				L.m.t[2] = 0;
				L.m.t[1] = 0;
				L.m.t[0] = 0;
				GteSetRotMatrix(&L.m);
				GteSetTransVector(&L.m);
				for (*(uint32_t *)&L.delta[0] = (uint32_t)(SPARK_Dirs + 12); ; )
				{
					SparkNode *s = (SparkNode *)AddTaskToQueue(QSparks(), ORIG_SparkTask);
					if (s)
					{
						Memset32(&s->counter, 0, 6);
						s->seq = SEQ_Spark;
						s->end = 0xF;
						GteLoadV0((const void *)*(const uint32_t *)&L.delta[0]);
						GteMVMVA_RotV0Tr();
						GteStoreIR123(s->pos);
						const int16_t ir[3] = { s->pos[0], s->pos[1], s->pos[2] };
						*(uint32_t *)&s->vel[0] = *(const uint32_t *)&s->pos[0];
						*(uint32_t *)&s->vel[2] = *(const uint32_t *)&s->pos[2];
						s->pos[0] = (int16_t)((int16_t)(ir[0] >> 3) + d->anchor[0]);
						s->pos[1] = (int16_t)((int16_t)(ir[1] >> 3) + d->anchor[1]);
						s->pos[2] = (int16_t)((int16_t)(ir[2] >> 3) + d->anchor[2]);
						s->vel[0] = (int16_t)(s->vel[0] >> 7);
						s->vel[1] = (int16_t)(s->vel[1] >> 7);
						s->vel[2] = (int16_t)(s->vel[2] >> 7);
						s->size = 0x1000;
						s->depth = (int16_t)(d->lift + 0x1000);
					}
					const uint32_t at = *(const uint32_t *)&L.delta[0];
					*(uint32_t *)&L.delta[0] = at - 8;
					if (at == (uint32_t)SPARK_Dirs) break;
				}
			}
		}
		// the target shown again and the damage (a big target at 114, else at 119)
		if (W16(d->target, 0x26) >= 0xC00)
		{
			if (e != 0x2D) return;
		}
		else if (e != 0x32) return;
		U16(d->target, 0) &= 0xFFF7;
		ApplyActionResultToTarget(ActionTargets(d->action));
	}

	// 0x6CBF90
	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		DirectorNode *d = (DirectorNode *)n;
		uint8_t *caster = Caster();
		DirLocals L;
		if (d->counter == 2) BdPlaySE(SOUND_Start, 0, 0x80);
		if (d->counter == 0x49) BdPlaySE(SOUND_Hit, 0, 0x80);
		// the screen flash: rising on 0..3, 0x400, falling on 0x95..0x98
		int32_t flash = (int32_t)0xFFFFD8F1;
		const int16_t c0 = d->counter;
		if (c0 < 4) { const int32_t v = shl32(c0, 8); if (v > flash) flash = v; }
		else if (c0 > 0x94) { const int32_t v = shl32(0x98 - c0, 8); if (v > flash) flash = v; }
		else flash = 0x400;
		SetScreenFlash((uint32_t)flash, 0);
		if (d->counter == 7)
		{
			// Zell steps back (950 along his facing)
			W16(caster, 0x1C) = (int16_t)(W16(caster, 0x1C) - (int16_t)(mul32(ComputeSin(W16(caster, 0xE)), 0x3B6) >> 12));
			W16(caster, 0x20) = (int16_t)(W16(caster, 0x20) - (int16_t)(mul32(ComputeCos(W16(caster, 0xE)), 0x3B6) >> 12));
		}
		// 10..25: the ground point / layout, his drift, sparks, smoke, the column, debris
		const int32_t k = (int32_t)d->counter - 0xA;
		if ((uint32_t)k <= 0xF)
		{
			if (k == 0)
			{
				DecodeModelPrimLayout(LAYOUT_Ground, d->ground_l, 0xC0);
				GetEffectSpawnPosition(caster, 0xF1, 0, d->ground);
			}
			else
			{
				L.arg.pos[0] = d->ground[0];
				L.arg.pos[1] = W16(caster, 0x24);
				L.arg.pos[2] = d->ground[2];
				L.arg.flags = (const uint32_t *)FLAGS_Ground;
				L.arg.morph = TexBase() + 0x1808;
				PlayLayout(d, SITE_GROUND, &L.arg);
				if ((uint32_t)(k - 1) <= 6)
				{
					// 11..17: Zell drifts forward by lift / 400 / 160 of his facing a tick
					const int32_t step = d->lift / 400;
					W16(caster, 0x1C) = (int16_t)(W16(caster, 0x1C) + (int16_t)(mul32(ComputeSin(W16(caster, 0xE)), step) / 160));
					W16(caster, 0x20) = (int16_t)(W16(caster, 0x20) + (int16_t)(mul32(ComputeCos(W16(caster, 0xE)), step) / 160));
				}
			}
			GroundPhase(d, L, k);
		}
		// 17..52: the two layouts at the midpoint of his hands (bones 0x15 / 0x16), screen tiles
		const int32_t h = (int32_t)d->counter - 0x11;
		if ((uint32_t)h <= 0x23)
		{
			if (h == 0)
			{
				DecodeModelPrimLayout(LAYOUT_HandA, d->hand_a, 0x110);
				DecodeModelPrimLayout(LAYOUT_HandB, d->hand_b, 0x338);
			}
			HandsMidpoint(caster, L.arg.pos);
			L.arg.radial = (uint32_t)h < 0x16 ? 0 : (int16_t)-0x400;
			L.arg.flags = (const uint32_t *)FLAGS_HandA;
			L.arg.morph = TexBase() + 0x1808;
			PlayLayout(d, SITE_HAND_A, &L.arg);
			if ((uint32_t)h > 4)
			{
				L.arg.flags = (const uint32_t *)FLAGS_HandB;
				L.arg.morph = TexBase() + 0x1808;
				PlayLayout(d, SITE_HAND_B, &L.arg);
			}
			if ((uint32_t)h >= 8) PacketCursor() = ScreenTile(0x32, 0x50, 0x50, h - 8, 3, PacketCursor());
			if (h == 0x15) ScreenFadeTask(0, 1, 1, 0x60);
			else if ((uint32_t)h >= 0x19) PacketCursor() = ScreenTile(0x28, 0x3C, 0x3C, h - 0x19, 5, PacketCursor());
		}
		if (d->counter == 0x22) ScreenFadeTask(0, 1, 1, 0x60);
		// 48..97: the fist layout at his bone 0x15, screen tiles, the target hidden at 52
		const int32_t f = (int32_t)d->counter - 0x30;
		if ((uint32_t)f < 0x32)
		{
			if (f == 0)
			{
				DecodeModelPrimLayout(LAYOUT_Fist, d->fist_l, 0x350);
				int16_t *ang = &L.arg.pos[2]; // (the angles local overlaps the parameter block)
				ang[0] = 0x12C;
				ang[1] = W16(caster, 0xE);
				ang[2] = (int16_t)f;
				ComposeZYXRotationMatrix(ang, &d->fist_m);
			}
			GetEffectSpawnPosition(caster, 0x15, 0, d->fist);
			d->fist_m.t[0] = d->fist[0];
			d->fist_m.t[1] = d->fist[1];
			d->fist_m.t[2] = d->fist[2];
			ComposeAffineTransform(&Camera(), &d->fist_m, &L.m);
			L.arg.m = &L.m;
			L.arg.flags = (const uint32_t *)FLAGS_Fist;
			L.arg.morph = TexBase() + 0x1808;
			PlayLayout(d, SITE_FIST, &L.arg);
			if ((uint32_t)f >= 4)
			{
				PacketCursor() = ScreenTile(0x50, 0x3C, 0x1E, f - 4, 4, PacketCursor());
				if (f == 4) B8(d->target, 0) |= 8;
			}
		}
		if (d->counter == 0x43) ScreenFadeTask(0, 1, 6, 0xFF);
		// 69..133: the impact
		const int32_t e = (int32_t)d->counter - 0x45;
		if ((uint32_t)e < 0x41) ImpactPhase(d, L, e);
		// 45..49: Zell jumps up and onto the target
		const int32_t j = (int32_t)d->counter - 0x2D;
		if ((uint32_t)j <= 4)
		{
			if (j == 0)
			{
				GetDefaultEffectPosition(d->target, d->land);
				const int32_t sz = (int32_t)W16(d->target, 0x26) + 0x1388;
				d->land[0] = (int16_t)(d->land[0] + (int16_t)(mul32(ComputeSin(W16(caster, 0xE)), sz) / 0x3000));
				const int32_t sz2 = (int32_t)W16(d->target, 0x26) + 0x1388;
				d->land[2] = (int16_t)(d->land[2] + (int16_t)(mul32(ComputeCos(W16(caster, 0xE)), sz2) / 0x3000));
				d->land[1] = (int16_t)(d->land[1] - (int16_t)(((int32_t)W16(d->target, 0x26) + 0x2710) >> 5));
			}
			const int32_t up = shl32(4 - j, 0xB);
			W16(caster, 0x1C) = (int16_t)((int16_t)(mul32(ComputeSin(W16(caster, 0xE)), up) >> 12) + d->land[0]);
			W16(caster, 0x20) = (int16_t)((int16_t)(mul32(ComputeCos(W16(caster, 0xE)), up) >> 12) + d->land[2]);
			W16(caster, 0x1E) = (int16_t)(d->land[1] - (int16_t)up);
		}
		// 137..143: his colours fade to the zero colour; 145..151: back
		const int32_t a = (int32_t)d->counter - 0x89;
		if ((uint32_t)a <= 6)
		{
			const int32_t t = (uint32_t)a < 4 ? (int32_t)(((uint32_t)a & 0xFFFFF) << 10) : 0x1000;
			BlendColour(&d->colour_a, Black(), 0x1000 - t, t, caster + 0x28);
			BlendColour(&d->colour_b, Black(), 0x1000 - t, t, caster + 0x2C);
			B8(caster, 1) |= 8;
			B8(caster, 0x2B) = 2;
			B8(caster, 0x2F) = 0x32;
			B8(caster, 7) = 0;
		}
		const int32_t b = (int32_t)d->counter - 0x91;
		if ((uint32_t)b <= 6)
		{
			const int32_t t = (int32_t)(((uint64_t)((uint32_t)b << 12) * 0xAAAAAAABull >> 32) >> 2);
			BlendColour(Black(), &d->colour_a, 0x1000 - t, t, caster + 0x28);
			BlendColour(Black(), &d->colour_b, 0x1000 - t, t, caster + 0x2C);
			B8(caster, 1) |= 8;
			B8(caster, 0x2B) = 2;
			B8(caster, 0x2F) = 0x32;
			B8(caster, 7) = 0;
		}
		if (d->counter == 0x8F) B8(caster, 0) |= 0xC;
		else if (d->counter == 0x91)
		{
			U32(caster, 0x1C) = d->saved_pos[0];
			U32(caster, 0x20) = d->saved_pos[1];
			U16(caster, 0) &= 0xFFF3;
		}
		else if (d->counter == 0x98)
		{
			U32(caster, 0x28) = d->colour_a;
			B8(caster, 0x2B) = 0;
			U32(caster, 0x2C) = d->colour_b;
			B8(caster, 0x2F) = 0x32;
			B8(caster, 7) = B8(d, 0x37);
			U16(caster, 0) &= 0xF7FF;
		}
		if (d->counter == 0x89)
		{
			TaskNode *w = AddTaskToQueue(QDebris(), ORIG_TexRestoreTask);
			if (w) Memset32((uint8_t *)w + 0xC, 0, 9);
		}
		if (d->counter >= 0x98)
		{
			U16(caster, 0) &= 0xEFFF;
			return TASK_END;
		}
		d->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// RootTask (0x6CBCF0, node 0x1C at 0x25218F8)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag112_meteor_barret_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x1E88;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x11E88;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QDirector(), (void *)TexBase(), 0xF48, 1);
				InitTaskQueuePool(QSmoke(), (void *)(TexBase() + 0x1488), 0x1C, 0x20);
				InitTaskQueuePool(QSparks(), (void *)(TexBase() + 0x1248), 0x24, 0x10);
				InitTaskQueuePool(QDebris(), (void *)(TexBase() + 0xF48), 0x30, 0x10);
			}
			if (r->action <= Ctx()->actions[0].last_action)
			{
				DirectorNode *d = (DirectorNode *)AddTaskToQueue(QDirector(), ORIG_DirectorTask);
				if (d)
				{
					uint8_t *caster = Caster();
					Memset32(&d->counter, 0, 0x3CF);
					B8(caster, 1) |= 0x10;
					d->action = r->action;
					d->target = Entity(ActionTargets(r->action)[0]);
					BindExternalAnimSet(caster, d->bind, COM_Anim);
					QueueChainTransformation(caster, 1);
					W16(caster, 0xE) = (int16_t)CartesianToGameAngle((int32_t)W16(caster, 0x1C) - (int32_t)W16(d->target, 0x1C), (int32_t)W16(caster, 0x20) - (int32_t)W16(d->target, 0x20));
					d->lift = (int32_t)W16(d->target, 0x26) - 0x190;
					if (d->lift < 0) d->lift = 0;
					d->saved_pos[0] = U32(caster, 0x1C);
					d->saved_pos[1] = U32(caster, 0x20);
					W16(caster, 0x1C) = 0;
					W16(caster, 0x20) = 0x3E8;
					d->colour_a = U32(caster, 0x28);
					d->colour_b = U32(caster, 0x2C);
					B8(d, 0x37) = B8(caster, 7);
					r->action++;
					PlayCameraBlob(CAMERA_Blob);
				}
			}
		}
		int queues = 1;
		if (r->started)
		{
			const int a = ExecuteTaskQueue(QDirector());
			const int b = ExecuteTaskQueue(QSmoke());
			const int c = ExecuteTaskQueue(QSparks());
			const int dd = ExecuteTaskQueue(QDebris());
			queues = a | b | c | dd;
		}
		if (r->started && queues == 0) return TASK_END;
		r->counter++;
		if (r->counter >= 10) r->counter = 0;
		return 0;
	}
}

	void register_mag112_meteor_barret()
	{
		register_port(meteor112::ORIG_RootTask, (void *)meteor112::RootTask, "112 RootTask", 112);
		register_port(meteor112::ORIG_DirectorTask, (void *)meteor112::DirectorTask, "112 DirectorTask", 112);
		register_port(meteor112::ORIG_SmokeTask, (void *)meteor112::SmokeTask, "112 SmokeTask", 112);
		register_port(meteor112::ORIG_DebrisTask, (void *)meteor112::DebrisTask, "112 DebrisTask", 112);
		register_port(meteor112::ORIG_SparkTask, (void *)meteor112::SparkTask, "112 SparkTask", 112);
		register_port(meteor112::ORIG_TexRestoreTask, (void *)meteor112::TexRestoreTask, "112 TexRestoreTask", 112);
		// 30 fps layer: see mag112_meteor_barret_held.inc
		FX_HELD(register_mag112_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag112_meteor_barret_held.inc"
#endif
