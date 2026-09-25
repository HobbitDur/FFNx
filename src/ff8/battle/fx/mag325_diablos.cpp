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

// Effect 325: Diablos - Dark Messenger (timeline-A GF family, MAG_325_*).
//
// Structure (see gf_study/gf_inventory_timeline.md 3.1):
//   SetupSummon (0x654210, not a task) - root queue 0x25051B0 (pool 0x25051C0, 1 x 0x10) with
//     the master; targets' mean position into 0x25051D4 (z) / 0x25051D8 (x).
//   SequenceTask (master, 0x654350) - flips the packet arena, spawns the timeline at tick 2,
//     runs the timeline queue (0x25051A0) then, when it has nodes, the prim-model particle
//     queue (0x2505190); screen flash (hides the battle entities at full flash).
//   TimelineTask (0x6545F0) - 530-tick timeline: two battle models (Diablos E1 at node+0x10C,
//     second model E2 at node+0x1A8), three prim-model players, prim models, target squash,
//     two particle systems in the model buffer (bolts: 0x6583E0 advance / 0x657F00 draw,
//     puffs: 0x658970 draw + 0x658C80/0x658CE0/0x658D30 moves), camera, streams, sounds,
//     damage (499), voice release (526).
//   RisingTask (0x659960) - prim-model player rising 0x18 per tick (spawned by 0x659900).
//   BurstTask (0x659860) - scaled prim-model player at a bone of Diablos (ticks 466..473).
//   prim-model callback 0x656B70 (drawn through the shared player, fx_primplayer.cpp).
// Module globals: 0x250517C..0x2505230 (gf_study/gf_global_ranges.md).

#include "fx_port.h"

namespace ff8fx
{
namespace d325
{
	using namespace eng;

	// --- module globals ---
	inline uint32_t &Pause() { return var<uint32_t>(0x2505180); }            // debug pause, zeroed by the setup, never set
	inline uint8_t *&CastCtx() { return var<uint8_t *>(0x2505184); }
	inline TaskQueuePair &QueueMain() { return var<TaskQueuePair>(0x2505190); } // .first = prim particles, .second = timeline
	inline TaskQueue &QueueParticles() { return QueueMain().first; }
	inline TaskQueue &QueueTimeline() { return QueueMain().second; }            // 0x25051A0
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25051D0); }
	inline int32_t &CenterZ() { return var<int32_t>(0x25051D4); }             // targets' mean BattleEntitySlotData +0x20
	inline int32_t &CenterX() { return var<int32_t>(0x25051D8); }             // targets' mean +0x1C
	inline int32_t &FarZ() { return var<int32_t>(0x25051DC); }                // CenterZ + 0xFA0
	inline int32_t &MidZ() { return var<int32_t>(0x25051E0); }                // CenterZ + 0x7D0
	inline uint32_t &Flash() { return var<uint32_t>(0x25051E8); }
	inline uint32_t &FlashPrev() { return var<uint32_t>(0x25051EC); }
	inline uint8_t &Mode() { return var<uint8_t>(0x25051F0); }                // particle system: 2 = bolts, 3 = puffs
	inline uint8_t *&ModelBuffer() { return var<uint8_t *>(0x2505208); }      // = Magic_TextureOFF_ToEAX1()
	inline Mat4x3 &EffectCamera() { return var<Mat4x3>(0x2505210); }
	// engine
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }       // battle_texture_data_ptr_1D8E054
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }             // g_Battle_FrameRenderListBase
	inline uint32_t ModelDrawMode() { return var<uint32_t>(0x1D969A8); }
	inline int16_t &CamWorld(int k) { return var<int16_t>(0xB8B7F0 + 2 * k); }  // Battle_Camera_world x, y, z
	inline int16_t &CamLookAt(int k) { return var<int16_t>(0xB8B7F8 + 2 * k); } // Battle_Camera_LookAt x, y, z

	static const uint32_t ORIG_SequenceTask = 0x654350;
	static const uint32_t ORIG_TimelineTask = 0x6545F0;
	static const uint32_t ORIG_BurstTask = 0x659860;
	static const uint32_t ORIG_RisingTask = 0x659960;
	static const uint32_t CB_PrimObject = 0x656B70; // prim-model object callback (pure draw)
	static uint32_t g_ported_tick = 0xFFFFFFFF;     // real tick on which the ported master last ran

	// --- engine and module functions called through their original addresses ---
	namespace x
	{
		inline int32_t Rand() { return fn<int32_t (__cdecl *)()>(0x55CBD2)(); }
		inline int32_t Cos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
		// MAG_063_sub_701DD0: prim-model set draw (header 0x6C bytes)
		inline uint32_t RenderPrimSet(void *hdr, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x701DD0)(hdr, ot, mode, cursor); }
		// MAG_063_sub_701220: rotation matrix about X (3x3 + pad)
		inline void RotX(int32_t angle, void *out) { fn<void (__cdecl *)(int32_t, void *)>(0x701220)(angle, out); }
		inline void Decode(uint32_t src, void *layout, int size) { fn<void (__cdecl *)(uint32_t, void *, int)>(0x7016B0)(src, layout, size); }
		// MAG_325_sub_658FC0: two morphed prim models (keyframe tables a / b, t 4.12) at (x, y, z)
		inline void MorphPair(int x, int y, int z, uint32_t hdr, void *tab_a, int na, int ta, void *tab_b, int nb, int tb)
		{ fn<void (__cdecl *)(int, int, int, uint32_t, void *, int, int, void *, int, int)>(0x658FC0)(x, y, z, hdr, tab_a, na, ta, tab_b, nb, tb); }
		// MAG_325_sub_659680: prim model at (x, y, z) relative to the camera (pure draw)
		inline void DrawPrimAt(int x, int y, int z, uint32_t model) { fn<void (__cdecl *)(int, int, int, uint32_t)>(0x659680)(x, y, z, model); }
		// MAG_325_sub_659900: new RisingTask
		inline void SpawnRising(int x, int y, int z, int shade, uint32_t model) { fn<void (__cdecl *)(int, int, int, int, uint32_t)>(0x659900)(x, y, z, shade, model); }
		// MAG_325_sub_657CB0: a battle entity (and its weapon) drawn with a scale vector
		inline void DrawEntityScaled(void *entity, const int32_t *scale) { fn<void (__cdecl *)(void *, const int32_t *)>(0x657CB0)(entity, scale); }
		// MAG_325_sub_659730: three prim sets (gravity well) at (x, y, z), texture scroll
		inline void DrawWell(int x, int y, int z, int scroll) { fn<void (__cdecl *)(int, int, int, int)>(0x659730)(x, y, z, scroll); }
		// MAG_314_sub_659A10: white-out tile, i-th tick of up + down ticks
		inline uint32_t FlashTile(int i, int r, int g, int b, int up, int down, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int, int, int, int, int, int, uint32_t)>(0x659A10)(i, r, g, b, up, down, cursor); }
		// MAG_063_sub_7040B0: full-screen tile, colour * level (4.12)
		inline uint32_t Tile(int r, int g, int b, int level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int, int, int, int, uint32_t)>(0x7040B0)(r, g, b, level, cursor); }
		// models
		inline void SetAnim(void *e, int id) { fn<void (__cdecl *)(void *, int)>(0x6574D0)(e, id); }
		inline void AdvanceAnim(void *e) { fn<void (__cdecl *)(void *)>(0x6FBDB0)(e); }
		inline void BindModel(void *e, void *sections, uint32_t data) { fn<void (__cdecl *)(void *, void *, uint32_t)>(0x6EC060)(e, sections, data); }
		// MAG_325_sub_6574F0: stylised model draw (private rasteriser, root not camera-composed)
		inline void DrawStylised(void *e, int y, int a, int mode, void *buf) { fn<void (__cdecl *)(void *, int, int, int, void *)>(0x6574F0)(e, y, a, mode, buf); }
		// MAG_089_sub_6A79B0: standard model draw
		inline uint32_t DrawModel(void *e, void *buf, uint32_t cursor, uint32_t mode) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, uint32_t)>(0x6A79B0)(e, buf, cursor, mode); }
		// particle systems in the model buffer
		inline void ResetPuffs() { fn<void (__cdecl *)()>(0x6545B0)(); }
		inline void BoltAdvance() { fn<void (__cdecl *)()>(0x6583E0)(); }
		inline void BoltDraw() { fn<void (__cdecl *)()>(0x657F00)(); }
		inline void PuffDraw() { fn<void (__cdecl *)()>(0x658970)(); }
		inline uint8_t *PuffAlloc() { return fn<uint8_t *(__cdecl *)()>(0x658B20)(); }
		inline void PuffMove() { fn<void (__cdecl *)()>(0x658CE0)(); }
		inline void PuffMoveRise() { fn<void (__cdecl *)()>(0x658D30)(); }
		inline void PuffMoveShrink() { fn<void (__cdecl *)()>(0x658C80)(); }
		inline void PuffSpawn(int n, int y, int flag) { fn<void (__cdecl *)(int, int, int)>(0x658B90)(n, y, flag); }
		inline void PuffsFromModel(void *e) { fn<void (__cdecl *)(void *)>(0x658D90)(e); }
		inline void BoltParams(int a, int b, int c) { fn<void (__cdecl *)(int, int, int)>(0x657E70)(a, b, c); }
		inline void BoltSpawn(void *s) { fn<void (__cdecl *)(void *)>(0x658890)(s); }
		// MAG_070_sub_657DF0: look-at = a[0..2] -> b[0..2], eye = a[3..5] -> b[3..5] at t (GTE)
		inline void CamLerp(const void *a, const void *b, int t) { fn<void (__cdecl *)(const void *, const void *, int)>(0x657DF0)(a, b, t); }
		inline void TargetHeights(void *entity, void *out) { fn<void (__cdecl *)(void *, void *)>(0x6DFD90)(entity, out); }
		inline void VertexPos(void *e, uint32_t vertex, void *out) { fn<void (__cdecl *)(void *, uint32_t, void *)>(0x6F0CC0)(e, vertex, out); }
		inline void SpawnPosition(void *e, int bone, int a, void *out) { fn<void (__cdecl *)(void *, int, int, void *)>(0x502170)(e, bone, a, out); }
		// streams / sound / battle
		inline void Load(int id, uint32_t dst, int n) { fn<void (__cdecl *)(int, uint32_t, int)>(0x5341D0)(id, dst, n); }
		inline int LoadBusy() { return fn<int (__cdecl *)()>(0x534270)(); }
		inline int PreLoad0(int a) { return fn<int (__cdecl *)(int)>(0x5342C0)(a); }
		inline void PreLoad() { fn<void (__cdecl *)()>(0x534210)(); }
		inline int PreLoad1(int a) { return fn<int (__cdecl *)(int)>(0x534300)(a); }
		inline void PlayStream(uint32_t a, int b, uint32_t c) { fn<void (__cdecl *)(uint32_t, int, uint32_t)>(0x5018C0)(a, b, c); }
		inline void TransStream(uint32_t dst, void *flag) { fn<void (__cdecl *)(uint32_t, void *)>(0x501860)(dst, flag); }
		inline void PlaySE(uint32_t se, int a, int b) { fn<void (__cdecl *)(uint32_t, int, int)>(0x501330)(se, a, b); }
		inline void TimUpload(uint32_t tim) { fn<void (__cdecl *)(uint32_t)>(0x505E30)(tim); }
		inline void ReleaseVoice(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
		inline void ApplyResult(void *targets, int n) { fn<void (__cdecl *)(void *, int)>(0x506BA0)(targets, n); }
	}

#pragma pack(push, 1)
	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
	struct MasterNode // root pool 0x25051C0, 0x10 bytes
	{
		TaskNode hdr;
		uint16_t counter; // +0x0C
		uint8_t parity;   // +0x0E packet arena
		uint8_t spawned;  // +0x0F timeline spawned
	};

	struct Target // 12 bytes, node+0x94 (one per target)
	{
		uint8_t *entity;  // BattleEntitySlotData (0x1D972C0 + 0x9C * id)
		int16_t h0, h1;   // +4 model height range (MAG_070_sub_6DFD90 at tick 368)
		int16_t z0, one;  // +8 = 0, 0x1000 (set by the master, unused)
	};

	struct TimelineNode // the single node of the timeline queue = ModelBuffer + 0, 0x994 bytes
	{
		TaskNode hdr;
		int16_t counter;          // +0x0C timeline tick
		uint8_t pad0E;
		uint8_t stream_ready;     // +0x0F BdTransSummonStream done flag
		uint32_t voice;           // +0x10 BdSound_ClaimVoiceSlot
		uint32_t flip[16];        // +0x14 prim model flipbook / morph table A (13 used)
		uint32_t morph_b[16];     // +0x54 morph table B (3 used)
		Target targets[10];       // +0x94
		uint8_t e1[0x9C];         // +0x10C Diablos: E+0x28 colour, E+0x40 root Mat4x3, E+0x60 BattleAnimHeader,
		                          //   E+0x64 sections, E+0x6C BattleAnimCmd, E+0x7C object mask
		uint8_t e2[0x9C];         // +0x1A8 second model (ticks 298..367)
		uint8_t sections1[0x10];  // +0x244
		uint8_t sections2[0x10];  // +0x254
		uint8_t prim_a[0xE0];     // +0x264 prim-model player (ticks 16..520)
		uint8_t prim_b[0x468];    // +0x344 player (298..327, 406..455)
		uint8_t prim_c[0x1E8];    // +0x7AC player (268..327, restarted when done)
	};

	struct PrimCtx // callback context of 0x656B70 (arg of the player), 0x30 bytes
	{
		int16_t pos[3];           // +0x00 placement (GTE V0)
		int16_t pad06;
		int32_t scale[3];         // +0x08 read only when use_scale
		int32_t use_scale;        // +0x14
		uint32_t cursor;          // +0x18 frame arena cursor in / out
		uint32_t scroll_sel;      // +0x1C char per object: '1' = texture scroll
		uint32_t draw_sel;        // +0x20 char per object: '0' = MAG_063_sub_701DD0, else MAG_325_sub_657030
		int16_t shade;            // +0x24
		int16_t scroll;           // +0x26 (low byte & 0x7F)
		int16_t fade;             // +0x28 non-zero: overrides the record's channel A
		int16_t pad2A;
		uint8_t *morph;           // +0x2C morph output buffer (ModelBuffer + 0xD94)
	};

	struct RisingNode // queue 0x2505190 (16 x 0xE8)
	{
		TaskNode hdr;
		int16_t pos[3];           // +0x0C
		int16_t shade;            // +0x12
		int32_t unused14;
		uint8_t layout[0xD0];     // +0x18 prim-model player
	};

	struct BurstNode // queue 0x2505190 (16 x 0xE8)
	{
		TaskNode hdr;
		int16_t pos[3];           // +0x0C GetEffectSpawnPosition
		int16_t shade;            // +0x12 = -0x100
		int32_t scale;            // +0x14 0x800..0xFFF, all three axes
		uint8_t layout[0xD0];     // +0x18 prim-model player (0xB8 decoded)
	};

	struct BoltSpawnRec // argument of MAG_325_sub_658890 (the timeline's stack local), 0x30 bytes
	{
		uint8_t pad0[8];
		int16_t pos[3];           // +0x08 leader start
		int16_t width;            // +0x0E
		int16_t target[3];        // +0x10
		int16_t t;                // +0x16
		int16_t vel[3];           // +0x18
		int16_t dt;               // +0x1E
		int16_t shape[3];         // +0x20
		int16_t budget;           // +0x26
		int16_t intensity;        // +0x28
		int16_t decay;            // +0x2A
		int16_t unused2C;
		int16_t fade;             // +0x2E
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x10, "master node is 0x10 bytes");
	static_assert(sizeof(Target) == 12, "target record is 12 bytes");
	static_assert(sizeof(TimelineNode) == 0x994, "timeline node is 0x994 bytes");
	static_assert(sizeof(PrimCtx) == 0x30, "prim context is 0x30 bytes");
	static_assert(sizeof(RisingNode) == 0xE8, "rising node is 0xE8 bytes");
	static_assert(sizeof(BurstNode) == 0xE8, "burst node is 0xE8 bytes");
	static_assert(sizeof(BoltSpawnRec) == 0x30, "bolt spawn record is 0x30 bytes");

	// --- model buffer regions ---
	static const uint32_t MB_MORPH = 0xD94;       // morph / skinning scratch (prim callback, model draws)
	static const uint32_t MB_STREAM_STATE = 0x4C14;
	static const uint32_t MB_PARTICLES = 0x4C5C;  // puffs: 160 x 0x10 / bolts: 0x200 vertices x 0x18
	static const uint32_t MB_PUFFS_END = 0x565C;
	static const uint32_t MB_CHAINS = 0x7C5C;     // bolts: 64 chains x 0x30
	static const uint32_t MB_CHAINS_END = 0x882C;
	static const uint32_t MB_ARENA_A = 0x885C, MB_ARENA_B = 0xF85C; // packet arenas (0x7000 each)
	static const uint32_t MB_STREAM = 0x1685C;

	inline uint8_t *E1(TimelineNode *t) { return t->e1; }
	inline uint8_t *E2(TimelineNode *t) { return t->e2; }
	inline Mat4x3 &Root(uint8_t *e) { return *(Mat4x3 *)(e + 0x40); }
	inline uint8_t *Morph() { return ModelBuffer() + MB_MORPH; }
	inline uint8_t *ActionData() { return *(uint8_t **)(CastCtx() + 4); }
	inline int TargetCount() { return ActionData()[0x10]; }

	// ------------------------------------------------------------------
	// Master task (0x654350)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SequenceTask(TaskNode *n)
	{
		MasterNode *node = (MasterNode *)n;
		g_ported_tick = g_real_tick;
		if (node->parity)
		{
			PacketCursor() = (uint32_t)(ModelBuffer() + MB_ARENA_A);
			node->parity = 0;
		}
		else
		{
			PacketCursor() = (uint32_t)(ModelBuffer() + MB_ARENA_B);
			node->parity = 1;
		}
		Flash() = 0;

		if (node->counter == 2 && !Pause() && !node->spawned)
		{
			node->spawned = 1;
			x::ResetPuffs();
			InitTaskQueuePool(&QueueTimeline(), ModelBuffer(), 0x994, 1);
			TimelineNode *t = (TimelineNode *)AddTaskToQueue(&QueueTimeline(), ORIG_TimelineTask);
			Memset32(&t->counter, 0, 0x262);
			t->voice = ClaimVoiceSlot((const void *)0xFD6658, 1, 0x80);
			x::Decode(0xFD8584, t->prim_a, 0xE0);
			for (int k = 0; k < TargetCount(); k++)
			{
				uint32_t id = (*(uint8_t **)(ActionData() + 8))[0x18 * k];
				t->targets[k].z0 = 0;
				t->targets[k].one = 0x1000;
				t->targets[k].entity = (uint8_t *)(0x1D972C0 + 0x9C * id);
			}
			StreamStateInit(ModelBuffer() + MB_STREAM_STATE);
			// sin / cos table (256 x {s16 sin, s16 cos}) for the puffs' rotation (0x658AB0)
			int16_t *tab = (int16_t *)(ModelBuffer() + 0x994);
			for (int32_t a = 0; a < 0x1000; a += 0x10)
			{
				*tab++ = (int16_t)ComputeSin(a);
				*tab++ = (int16_t)x::Cos(a);
			}
		}

		uint32_t left = (uint32_t)n; // the original keeps the node pointer here: non-zero
		if (node->spawned)
		{
			left = (uint32_t)ExecuteTaskQueue(&QueueTimeline());
			if (QueueParticles().head)
			{
				EffectCameraMatrix(&Camera(), &EffectCamera());
				ExecuteTaskQueue(&QueueParticles());
			}
		}

		// screen flash; at full flash the battle entities (stru_1D9898C[0..3].currentBsId bit 1) show
		uint32_t f = Flash();
		if (f != FlashPrev())
		{
			SetScreenFlash(f, 0);
			f = Flash();
			if ((int32_t)f >= 0x1000)
			{
				for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) &= 0xFD;
			}
			else if ((int32_t)FlashPrev() >= 0x1000)
			{
				for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C) var<uint8_t>(a) |= 2;
			}
			FlashPrev() = f;
		}

		if (Pause()) return 0;
		if (node->spawned && left == 0)
		{
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		node->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Timeline draw pieces (shared by the real tick and the held frame)
	// ------------------------------------------------------------------

	// screen flash level / prim player A fade for counter c (v = c - 1 < 0x211)
	static int16_t FadeA(uint32_t v)
	{
		if (v < 0x10) return (int16_t)(((0x10000u - (v << 12)) >> 3) + 0x100);
		if (v >= 0x201) return (int16_t)((((v + 0xFFDFFu) << 12) >> 3) + 0x100);
		return 0;
	}

	static void CtxA(PrimCtx &c, int16_t fade)
	{
		memset(&c, 0, sizeof(c));
		c.pos[2] = (int16_t)MidZ();
		c.morph = Morph();
		c.scroll_sel = 0xFE0FC0; // "0000"
		c.draw_sel = 0xFE0FB8;   // "0010"
		c.shade = 0x200;
		c.fade = fade;
	}

	static void CtxC(PrimCtx &c)
	{
		memset(&c, 0, sizeof(c));
		c.pos[0] = 0x5A;
		c.pos[1] = (int16_t)0xDC7C;
		c.pos[2] = (int16_t)(FarZ() - 0xA0);
		c.morph = Morph();
		c.scroll_sel = c.draw_sel = 0xFE0FB4; // "0"
		c.shade = -0x40;
	}

	static void CtxB298(PrimCtx &c)
	{
		memset(&c, 0, sizeof(c));
		c.pos[0] = 0x4A;
		c.pos[1] = (int16_t)0xD736;
		c.pos[2] = (int16_t)(FarZ() - 0xF2);
		c.morph = Morph();
		c.scroll_sel = 0xFE0FAC; // "00000"
		c.draw_sel = 0xFE0FA4;   // "01000"
	}

	static void CtxB406(PrimCtx &c, int16_t scroll)
	{
		memset(&c, 0, sizeof(c));
		c.pos[0] = (int16_t)CenterX();
		c.pos[2] = (int16_t)CenterZ();
		c.morph = Morph();
		c.scroll = scroll;
		c.scroll_sel = 0xFE0F94; // "0010000000000"
		c.draw_sel = 0xFE0F84;   // "1000000100000"
	}

	static int Play(void *layout, PrimCtx &c)
	{
		c.cursor = FrameCursor();
		int r = prim::play((prim::Layout *)layout, (prim::Callback)CB_PrimObject, (int)&c, (int)Pause());
		FrameCursor() = c.cursor;
		return r;
	}

	static void PlayHeld(void *layout, PrimCtx &c, int num, int den)
	{
		c.cursor = FrameCursor();
		prim::play_held((prim::Layout *)layout, (prim::Callback)CB_PrimObject, (int)&c, num, den);
		FrameCursor() = c.cursor;
	}

	// 0x65473A: growing prim model (0xFD6B6C) at the targets, uniform scale diag (ticks 31..90)
	static void DrawGrowing(int32_t diag, int32_t ty)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x8C);
		*(int16_t *)(w + 0xE) = 0;
		*(int16_t *)(w + 0xC) = 0;
		*(int16_t *)(w + 0xA) = 0;
		*(int16_t *)(w + 0x10) = (int16_t)diag;
		*(int16_t *)(w + 8) = (int16_t)diag;
		*(int16_t *)(w + 0) = (int16_t)diag;
		*(int16_t *)(w + 6) = 0;
		*(int16_t *)(w + 4) = 0;
		*(int16_t *)(w + 2) = 0;
		*(int32_t *)(w + 0x14) = 0;
		*(int32_t *)(w + 0x18) = ty;
		*(int32_t *)(w + 0x1C) = CenterZ();
		ComposeAffineTransform(&Camera(), (Mat4x3 *)w, (Mat4x3 *)w);
		GteSetRotMatrix((Mat4x3 *)w);
		GteSetTransVector((Mat4x3 *)w);
		*(int32_t *)(w + 0x3C) = 0;
		*(int32_t *)(w + 0x40) = 0;
		*(int32_t *)(w + 0x38) = 0;
		*(uint32_t *)(w + 0x20) = 0xFD6B6C;
		PacketCursor() = x::RenderPrimSet(w + 0x20, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x8C);
	}
	static int32_t GrowDiag(uint32_t e) { return (int32_t)((e << 12) / 60u) + 0x22; }

	// 0x654DDC: sinking prim model (ModelBuffer + 0x35F7C), tilted about X (ticks 360..367);
	// k = 0..7, sub = in-between fraction of the next step (x 0x200 / den)
	static void DrawSinking(int32_t k, int32_t sub)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x78);
		x::RotX(0x200, w);
		*(int32_t *)(w + 0x14) = 0;
		*(int32_t *)(w + 0x18) = shl32(k - 0xE, 9) + sub;
		*(int32_t *)(w + 0x1C) = FarZ() - shl32(k + 4, 9) - sub;
		ComposeAffineTransform(&Camera(), (Mat4x3 *)w, (Mat4x3 *)w);
		GteSetRotMatrix((Mat4x3 *)w);
		GteSetTransVector((Mat4x3 *)w);
		*(int32_t *)(w + 0x3C) = 0x30;
		*(uint32_t *)(w + 0x20) = (uint32_t)(ModelBuffer() + 0x35F7C);
		PacketCursor() = RenderPrimModel(w + 0x20, OTBase() + 0x44, 2, PacketCursor());
		FieldFree(0x78);
	}

	// ease curve of the gravity well (0x654E7E.., 0x655DD0.., 0x656899..): x = 0..0x1000
	static int32_t Ease(int32_t x)
	{
		if (x >= 0x555) return ((x - 0x555) * 426 >> 11) + 0xE56;
		int32_t s = ComputeSin(x >> 2);
		s = ComputeSin(s >> 2);
		return ComputeSin(s >> 2);
	}
	// target squash depth of well tick e (0..0x25)
	static int32_t WellDepth(uint32_t e) { return Ease((int32_t)((e << 12) / 38u)) * 4 - 0x4100; }

	// 0x654F4D: a target squashed towards the ground: y scale and root y so that its height
	// (h1 - h0 seen at its distance from the well) stays above depth
	static void SquashTarget(Target &t, int32_t depth, int32_t scale[3])
	{
		uint8_t *ent = t.entity;
		int32_t d = (*(int32_t *)(ent + 0x54) - CenterX()) >> 2;
		if (d < 0) d = -d;
		if (d > 0x400) d = 0x400;
		int32_t co = x::Cos(d);
		int32_t h0 = t.h0;
		int32_t h = mul32((int32_t)t.h1 - h0, co) >> 12;
		if (h < depth)
		{
			int32_t q = shl32(depth, 12) / h;
			scale[1] = q;
			*(int32_t *)(ent + 0x58) = (int32_t)*(int16_t *)(ent + 0x1E) - mul32(q, h0) / 4096;
		}
		else
		{
			scale[1] = 0x1000;
			*(int32_t *)(ent + 0x58) = (int32_t)*(int16_t *)(ent + 0x1E) - h0;
		}
		scale[2] = 0x1000;
		scale[0] = 0x1000;
	}

	static int32_t FlashTileLevel(int32_t i) { return i < 4 ? shl32(i + 1, 12) / 4 : shl32(4 - i - 1, 12) / 8 + 0x1000; }

	// ------------------------------------------------------------------
	// Camera (0x656208..0x656AE0): cuts and GTE lerps between fixed key cameras
	// (a / b = look-at xyz then eye xyz). real = the timeline tick (also runs the puff
	// operations interleaved with the camera code); else only the camera words are written.
	// Returns: bit 0 = the camera was written, bit 1 = it was a cut.
	// ------------------------------------------------------------------
	static void SetCam(int16_t lx, int16_t ly, int16_t lz, int16_t wx, int16_t wy, int16_t wz)
	{
		CamLookAt(0) = lx; CamLookAt(1) = ly; CamLookAt(2) = lz;
		CamWorld(0) = wx; CamWorld(1) = wy; CamWorld(2) = wz;
	}

	static void Lerp6(int16_t a0, int16_t a1, int16_t a2, int16_t a3, int16_t a4, int16_t a5,
		int16_t b0, int16_t b1, int16_t b2, int16_t b3, int16_t b4, int16_t b5, int32_t t)
	{
		int16_t a[6] = { a0, a1, a2, a3, a4, a5 }, b[6] = { b0, b1, b2, b3, b4, b5 };
		x::CamLerp(a, b, t);
	}

	static int CameraTick(int32_t c, bool real)
	{
		int r = 0;
		const int32_t D4 = CenterZ(), D8 = CenterX();
		if ((uint32_t)c < 0x1E)
		{
			if (c == 0)
			{
				if (real)
				{
					x::ResetPuffs();
					var<int16_t>(0x1D8E038) = 0x120;
					var<int16_t>(0x1D977A2) = 0; // g_BattleCam_Roll
				}
				SetCam(0, (int16_t)0xF3A2, (int16_t)(D4 - 0x4BF), 0, (int16_t)0xF8FC, (int16_t)(D4 + 0x1E57));
				r = 3;
			}
			else if ((uint32_t)c > 8 && real)
			{
				x::PuffSpawn(4, -0xC00, 0);
				x::PuffMoveRise();
			}
		}
		if ((uint32_t)(c - 0x1E) < 0x1E)
		{
			if (c == 0x1E)
			{
				if (real) x::ResetPuffs();
				SetCam(0, (int16_t)0xC000, (int16_t)D4, 0, (int16_t)0xB8C0, (int16_t)(D4 + 0x13E8));
				r = 3;
				if (real)
					for (int k = 0x13; k; k--)
					{
						x::PuffSpawn(4, -0x4000, 1);
						x::PuffMoveShrink();
					}
			}
			if (real)
			{
				x::PuffSpawn(4, -0x4000, 1);
				x::PuffMoveShrink();
			}
		}
		{
			uint32_t e = (uint32_t)(c - 0x3C);
			if (e < 0x1E)
			{
				int32_t t = (int32_t)((e << 12) / 30u) + 0x44;
				if (real)
				{
					if (e == 0)
					{
						uint8_t *p = ModelBuffer() + MB_PARTICLES + 6;
						for (int k = 0xA0; k; k--, p += 0x10)
							if (p[9]) *(int16_t *)p += 0x2000;
					}
					x::PuffMoveShrink();
				}
				Lerp6(0x8FE, (int16_t)0xF9B0, (int16_t)(D4 - 0x837), (int16_t)0xFE53, (int16_t)0xD59D, (int16_t)(D4 + 0x6B6),
					(int16_t)0xFAA0, (int16_t)0xF78B, (int16_t)(D4 - 0xCAE), 0x6EB, (int16_t)0xD563, (int16_t)(D4 + 0x4EF), t);
				r = e == 0 ? 3 : 1;
			}
		}
		int32_t DC = FarZ();
		if (c == 0x5A)
		{
			if (real) x::ResetPuffs();
			DC = FarZ();
			SetCam(0, (int16_t)0xDFED, (int16_t)(DC - 0xD6), 0, (int16_t)0xE105, (int16_t)(DC - 0x62C));
			r = 3;
		}
		{
			uint32_t e = (uint32_t)(c - 0x7B);
			if (e < 0x1E)
			{
				int32_t t = (int32_t)((e << 12) / 30u) + 0x44;
				Lerp6(0, (int16_t)0xDFED, (int16_t)(DC - 0xD6), 0, (int16_t)0xE105, (int16_t)(DC - 0x62C),
					0, (int16_t)0xE3B4, (int16_t)(DC + 0x12C), 0, (int16_t)0xE9D2, (int16_t)(DC - 0xC52), t);
				DC = FarZ();
				r |= 1;
			}
		}
		if (c == 0x99)
		{
			SetCam(0, (int16_t)0xE532, (int16_t)(DC + 0xD9), 0, (int16_t)0xE565, (int16_t)(DC - 0x4AA));
			r = 3;
		}
		{
			uint32_t e = (uint32_t)(c - 0xAF);
			if (e < 0xF)
			{
				int32_t t = ComputeSin((int32_t)((e << 12) / 15u + 0x88) >> 2);
				DC = FarZ();
				Lerp6(0, (int16_t)0xE532, (int16_t)(DC + 0xD9), 0, (int16_t)0xE565, (int16_t)(DC - 0x4AA),
					0, (int16_t)0xE2FD, (int16_t)(DC + 0xD1), 0, (int16_t)0xE41A, (int16_t)(DC - 0xCE8), t);
				DC = FarZ();
				r |= 1;
			}
		}
		if (c == 0xCA)
		{
			if (real) x::ResetPuffs();
			DC = FarZ();
			SetCam((int16_t)0xFF51, (int16_t)0xE051, (int16_t)(DC - 0xF0), 0x177, (int16_t)0xE016, (int16_t)(DC - 0x121));
			r = 3;
		}
		{
			uint32_t e = (uint32_t)(c - 0xD9);
			if (e < 0x14)
			{
				int32_t co = x::Cos((int32_t)((e << 12) / 20u + 0x66) / 2);
				int32_t t = 0x800 - co / 2;
				DC = FarZ();
				Lerp6((int16_t)0xFF50, (int16_t)0xE053, (int16_t)(DC - 0xF1), (int16_t)0xFE29, (int16_t)0xD9DB, (int16_t)(DC + 0xB8),
					(int16_t)0xFF50, (int16_t)0xE053, (int16_t)(DC - 0xF1), 0x1CB, (int16_t)0xDA33, (int16_t)(DC + 0x7C), t);
				DC = FarZ();
				r = e == 0 ? 3 : (r | 1);
			}
		}
		if (c == 0xED)
		{
			SetCam(0x27, (int16_t)0xDED0, (int16_t)(DC - 0xEB), (int16_t)0xFCBA, (int16_t)0xDF3B, (int16_t)(DC - 0x48C));
			r = 3;
		}
		{
			uint32_t e = (uint32_t)(c - 0x104);
			if (e <= 0x10)
			{
				int32_t t = (int32_t)((e << 12) >> 4);
				Lerp6(0x27, (int16_t)0xDED0, (int16_t)(DC - 0xEB), (int16_t)0xFCBA, (int16_t)0xDF3B, (int16_t)(DC - 0x48C),
					0, (int16_t)0xDD59, (int16_t)(DC - 0x115), (int16_t)0xFEE7, (int16_t)0xDF0B, (int16_t)(DC - 0x228), t);
				DC = FarZ();
				r |= 1;
			}
		}
		if (c == 0x129)
		{
			if (real) var<int16_t>(0x1D977A2) = (int16_t)0xFF00;
			SetCam((int16_t)0xFFF9, (int16_t)0xDD51, (int16_t)(DC - 0x109), (int16_t)0xFF0D, (int16_t)0xE5DC, (int16_t)(DC - 0xEDF));
			r = 3;
		}
		{
			uint32_t e = (uint32_t)(c - 0x149);
			if (e < 0x28)
			{
				if (real) var<int16_t>(0x1D977A2) = 0;
				int32_t t = (int32_t)((e << 12) / 40u) + 0x66;
				Lerp6(0x3D, (int16_t)0xDE1E, (int16_t)(DC - 0x97), 0x1ED, (int16_t)0xDE91, (int16_t)(DC - 0x70),
					0xC9, (int16_t)0xE35F, (int16_t)(DC - 0x6C7), 0x651, (int16_t)0xE711, (int16_t)(DC + 0x48C), t);
				DC = FarZ();
				r = e == 0 ? 3 : (r | 1);
			}
		}
		{
			uint32_t e = (uint32_t)(c - 0x16F);
			if (e < 0x26)
			{
				int32_t t = (int32_t)((e << 12) / 38u) + 0x35;
				int32_t s = Ease(t);
				DC = FarZ();
				int16_t x8 = (int16_t)D8;
				Lerp6(x8, (int16_t)(s * 4 - 0x4100), (int16_t)(D4 + 0xEE4), x8, (int16_t)0xFDF2, (int16_t)(D4 + 0x2BBB),
					x8, (int16_t)0xFBE3, (int16_t)(DC - 0x1A9), x8, (int16_t)0xFCCF, (int16_t)(DC + 0xA30), t);
				DC = FarZ();
				r = e == 0 ? 3 : (r | 1);
			}
		}
		if (c == 0x195)
		{
			SetCam((int16_t)(D8 - 0x3EC), (int16_t)0xF22F, (int16_t)(D4 + 0x27B), (int16_t)(D8 - 0x1A91), (int16_t)0xC008, (int16_t)(D4 + 0x2287));
			r = 3;
		}
		if (c == 0x1C7)
		{
			SetCam(0, (int16_t)0xE16E, (int16_t)DC, 0xEB6, (int16_t)0xE1D0, (int16_t)DC);
			r = 3;
		}
		{
			uint32_t e = (uint32_t)(c - 0x1D4);
			if (e < 0x1E)
			{
				int32_t co = x::Cos((int32_t)((e << 12) / 30u + 0x44) / 2);
				int32_t t = 0x800 - co / 2;
				DC = FarZ();
				Lerp6(0, (int16_t)0xE16E, (int16_t)DC, 0xEB6, (int16_t)0xE1D0, (int16_t)DC,
					0, (int16_t)0xE16D, (int16_t)(DC - 1), 0x24C4, (int16_t)0xE7DF, (int16_t)(DC - 0x18), t);
				r |= 1;
			}
		}
		if (c == 0x1F2)
		{
			SetCam((int16_t)D8, (int16_t)0xFCF4, (int16_t)(D4 - 0x470), (int16_t)D8, (int16_t)0xFA2F, (int16_t)(D4 + 0x2178));
			r = 3;
		}
		return r;
	}

	// ------------------------------------------------------------------
	// Held-frame memo of what the real timeline tick drew
	// ------------------------------------------------------------------
	static const uint32_t SKEL_MAX = 16 + 48 * 256;
	struct PoseMemo { bool ok; uint32_t size; uint8_t pose[SKEL_MAX]; };
	struct TimelineMemo
	{
		uint32_t tick;
		const void *node;
		int16_t c;                  // counter the tick ran with
		bool play_a; PrimCtx ctx_a;
		bool grow; uint32_t grow_e;
		bool morph; uint32_t morph_e;
		int nflip; struct { int32_t x, y, z; uint32_t model; } flip[2];
		bool play_c; PrimCtx ctx_c; bool restart_c; uint8_t layout_c[0x1E8];
		bool play_b; PrimCtx ctx_b; bool b406;
		bool sink; int32_t sink_k;
		bool well; uint32_t well_e;
		bool tile; int32_t tile_e;
		int e1_draw;                // 0 none, 1 stylised (-0x2000, mode 2), 2 stylised (-0x22BC, mode 0), 3 standard
		bool e1_blend;              // E1 was drawn on the previous real tick too (the drawn pose is the memo)
		Mat4x3 e1_root; uint32_t e1_colour;
		bool e2_draw, e2_blend;
		PoseMemo p1, p2;
		int pmode;                  // particle system drawn: 2 bolts, 3 puffs
		uint8_t pool[MB_CHAINS_END - MB_PARTICLES];
	};
	static TimelineMemo g_tm = { 0xFFFFFFFF };
	static uint32_t g_e1_drawn_tick = 0xFFFFFFF0, g_e2_drawn_tick = 0xFFFFFFF0;

	static uint8_t *Skeleton(uint8_t *e, uint32_t *size)
	{
		uint8_t *com = *(uint8_t **)(e + 0x64); // BattleAnimHeader.comFileData
		uint8_t *sk = com ? *(uint8_t **)com : nullptr;
		if (!sk || sk[0] == 0) return nullptr;
		*size = 16 + 48 * (uint32_t)sk[0];
		return *size <= SKEL_MAX ? sk : nullptr;
	}

	static void PoseSave(uint8_t *e, PoseMemo &m)
	{
		uint32_t size = 0;
		uint8_t *sk = Skeleton(e, &size);
		m.ok = sk != nullptr;
		if (m.ok) { memcpy(m.pose, sk, size); m.size = size; }
	}

	// ------------------------------------------------------------------
	// Timeline task (0x6545F0): one node at ModelBuffer + 0, 530 ticks (0x212), then waits for
	// the last stream (counter parked at 0x222) and ends.
	// ------------------------------------------------------------------
	static uint32_t __cdecl TimelineTask(TaskNode *n)
	{
		TimelineNode *t = (TimelineNode *)n;
		uint8_t *MB = ModelBuffer();
		uint8_t *e1 = E1(t), *e2 = E2(t);

		TimelineMemo &M = g_tm;
		M.tick = g_real_tick;
		M.node = n;
		M.c = t->counter;
		M.play_a = M.grow = M.morph = M.play_c = M.restart_c = M.play_b = M.sink = M.well = M.tile = false;
		M.nflip = 0;
		M.e1_draw = 0;
		M.e2_draw = false;
		M.pmode = 0;
		// the pose each model draws this tick is the one the tick starts with (its draw builds the
		// world matrices from the bone matrices of the previous draw, before this tick's anim read)
		PoseSave(e1, M.p1);
		PoseSave(e2, M.p2);

		int32_t c = t->counter;

		// screen flash; prim player A (ticks 9..520)
		{
			uint32_t v = (uint32_t)(c - 1);
			if (v < 0x211)
			{
				if (v < 8) Flash() = ((v << 12) >> 3) + 0x100;
				else if (v >= 0x209) Flash() = ((0x210000u - (v << 12)) >> 3) + 0x100;
				else
				{
					Flash() = 0x1000;
					PrimCtx ctx;
					CtxA(ctx, FadeA(v));
					M.play_a = true;
					M.ctx_a = ctx;
					Play(t->prim_a, ctx);
				}
			}
		}

		// 31..90: growing prim model
		{
			uint32_t e = (uint32_t)(c - 0x1F);
			if (e < 0x3C)
			{
				M.grow = true;
				M.grow_e = e;
				DrawGrowing(GrowDiag(e), e < 0x1E ? -0x4000 : -0x2000);
			}
		}

		// 91..102: morphing prim models
		{
			uint32_t e = (uint32_t)(c - 0x5B);
			if (e < 0xC)
			{
				int32_t tt = (int32_t)((e << 12) / 12u);
				if (e == 0)
				{
					t->flip[0] = (uint32_t)(MB + 0x3505C);
					t->flip[1] = (uint32_t)(MB + 0x3704C);
					t->flip[2] = (uint32_t)(MB + 0x3505C);
					t->morph_b[0] = (uint32_t)(MB + 0x3505C);
					t->morph_b[1] = (uint32_t)(MB + 0x3704C);
					t->morph_b[2] = (uint32_t)(MB + 0x3505C);
				}
				M.morph = true;
				M.morph_e = e;
				x::MorphPair(0, -0x2000, (int16_t)FarZ(), (uint32_t)(ModelBuffer() + 0x4C244), t->flip, 2, tt, t->morph_b, 2, tt);
			}
		}

		// 103..202: prim-model flipbook (and the five rising prims at 103)
		{
			uint32_t e = (uint32_t)(c - 0x67);
			if (e < 0x64)
			{
				uint32_t model;
				if (e == 0)
				{
					if (!Pause())
					{
						InitTaskQueuePool(&QueueParticles(), ModelBuffer() + 0x3505C, 0xE8, 0x10);
						x::SpawnRising(-500, -0x222C, FarZ(), -0x80, (uint32_t)(ModelBuffer() + 0x4AFAC));
						x::SpawnRising(-200, -0x222C, FarZ(), -0x80, (uint32_t)(ModelBuffer() + 0x4BBC4));
						x::SpawnRising(0, -0x222C, FarZ(), -0x80, (uint32_t)(ModelBuffer() + 0x4B558));
						x::SpawnRising(200, -0x222C, FarZ(), -0x80, (uint32_t)(ModelBuffer() + 0x4BBC4));
						x::SpawnRising(500, -0x222C, FarZ(), -0x80, (uint32_t)(ModelBuffer() + 0x4AFAC));
						for (int k = 0; k < 9; k++) t->flip[k] = (uint32_t)(ModelBuffer() + 0x3903C + 0x1FF0 * k);
					}
					model = t->flip[e % 9];
				}
				else if (e < 0x46) model = t->flip[e % 9];
				else model = t->flip[0];
				M.flip[M.nflip++] = { 0, -0x2000, (int16_t)FarZ(), model };
				x::DrawPrimAt(0, -0x2000, (int16_t)FarZ(), model);
			}
		}

		// 173..202: anims 1, 2
		{
			uint32_t e = (uint32_t)(c - 0xAD);
			if (e < 0x1E)
			{
				if (e == 0) x::SetAnim(e1, 1);
				else if (e == 0x1C) x::SetAnim(e1, 2);
			}
		}
		if (t->counter == 0xCB && !Pause()) Root(e1).t[1] = -0x1880;

		// 238..297: anims 3, 4; second flipbook
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0xEE);
			if (e < 0x3C)
			{
				if (e == 0)
				{
					if (!Pause())
					{
						x::SetAnim(e1, 3);
						for (int k = 0; k < 13; k++) t->flip[k] = (uint32_t)(ModelBuffer() + 0x3505C + 0x1FF0 * k);
					}
				}
				else if (e == 8 && !Pause()) x::SetAnim(e1, 4);
				uint32_t model = t->flip[e % 13];
				M.flip[M.nflip++] = { 0x5A, -0x2258, FarZ() - 0xA0, model };
				x::DrawPrimAt(0x5A, -0x2258, FarZ() - 0xA0, model);
			}
		}

		// 268..327: Diablos sinks (268..297); prim player C, restarted when done
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x10C);
			if (e < 0x1E && !Pause()) Root(e1).t[1] += -10;
			if (e < 0x3C)
			{
				if (e == 0) x::Decode((uint32_t)(ModelBuffer() + 0x51D40), t->prim_c, 0x1E8);
				PrimCtx ctx;
				CtxC(ctx);
				M.play_c = true;
				M.ctx_c = ctx;
				int r = Play(t->prim_c, ctx);
				if (r == 0)
				{
					// the held frame needs the player as it was drawn, not the restarted one
					M.restart_c = true;
					memcpy(M.layout_c, t->prim_c, sizeof(M.layout_c));
					x::Decode((uint32_t)(ModelBuffer() + 0x51D40), t->prim_c, 0x1E8);
				}
			}
		}

		// 298..327: prim player B
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x12A);
			if (e < 0x1E)
			{
				if (e == 0) x::Decode((uint32_t)(ModelBuffer() + 0x4EF8C), t->prim_b, 0x3C8);
				PrimCtx ctx;
				CtxB298(ctx);
				M.play_b = true;
				M.b406 = false;
				M.ctx_b = ctx;
				Play(t->prim_b, ctx);
			}
		}

		// 328..367: anims 5 / 1; sinking prim model (360..367)
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x148);
			if (e < 0x28)
			{
				if (e == 0)
				{
					x::SetAnim(e1, 5);
					x::SetAnim(e2, 1);
				}
				else if (e >= 0x20)
				{
					M.sink = true;
					M.sink_k = (int32_t)e - 0x20;
					DrawSinking((int32_t)e - 0x20, 0);
				}
			}
		}

		// 368..405: gravity well, the targets are squashed
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x170);
			if (e < 0x26)
			{
				int32_t depth = WellDepth(e);
				if (e == 0)
					for (int k = 0; k < TargetCount(); k++)
					{
						uint8_t *ent = t->targets[k].entity;
						x::TargetHeights(ent, &t->targets[k].h0);
						ent[0] |= 8;
					}
				for (int k = 0; k < TargetCount(); k++)
				{
					int32_t scale[3];
					SquashTarget(t->targets[k], depth, scale);
					x::DrawEntityScaled(t->targets[k].entity, scale);
					t->targets[k].entity[0] |= 4;
				}
				M.well = true;
				M.well_e = e;
				x::DrawWell(CenterX(), depth - 0x1000, CenterZ(), (int32_t)(e << 4));
			}
		}

		// 402..413: white-out tile
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x192);
			if (e < 0xC)
			{
				M.tile = true;
				M.tile_e = (int32_t)e;
				PacketCursor() = x::FlashTile((int32_t)e, 0xFF, 0xFF, 0xFF, 4, 8, PacketCursor());
			}
		}

		// 406..455: prim player B again (scrolling)
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x196);
			if (e < 0x32)
			{
				if (e == 0) x::Decode((uint32_t)(ModelBuffer() + 0x3B4E4), t->prim_b, 0x468);
				PrimCtx ctx;
				CtxB406(ctx, (int16_t)(e * 4));
				M.play_b = true;
				M.b406 = true;
				M.ctx_b = ctx;
				Play(t->prim_b, ctx);
			}
		}
		if (t->counter == 0x1C8) x::SetAnim(e1, 6);

		// 92..621: Diablos (E1)
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x5C);
			if (e < 0x212)
			{
				if (e == 0)
				{
					Mat4x3 &r = Root(e1);
					r.m[2][1] = 0;
					r.m[2][2] = 0x1000;
					r.m[1][1] = 0x1000;
					r.m[0][0] = 0x1000;
					r.m[2][0] = 0;
					r.m[1][2] = 0;
					r.m[1][0] = 0;
					r.m[0][2] = 0;
					r.m[0][1] = 0;
					r.t[0] = 0;
					r.t[1] = -0x2000;
					r.t[2] = FarZ();
					x::BindModel(e1, t->sections1, (uint32_t)(ModelBuffer() + 0x2185C));
					x::SetAnim(e1, 0);
					*(uint32_t *)(e1 + 0x28) = 0x808080;
				}
				if (!Pause()) x::AdvanceAnim(e1);
				int kind = 0;
				if (e < 0x4B) kind = 1;
				else if (e >= 0xB0 && e < 0xEC) kind = 2;
				else if (e <= 0x114 || (e >= 0x16C && e < 0x17A)) kind = 3;
				M.e1_draw = kind;
				if (kind)
				{
					M.e1_blend = g_e1_drawn_tick + 1 == g_real_tick;
					g_e1_drawn_tick = g_real_tick;
					M.e1_root = Root(e1);
					M.e1_colour = *(uint32_t *)(e1 + 0x28);
				}
				if (kind == 1) x::DrawStylised(e1, -0x2000, 0x80, 2, ModelBuffer() + MB_MORPH);
				else if (kind == 2) x::DrawStylised(e1, -0x22BC, 0x80, 0, ModelBuffer() + MB_MORPH);
				else if (kind == 3) FrameCursor() = x::DrawModel(e1, ModelBuffer() + MB_MORPH, FrameCursor(), ModelDrawMode());
			}
		}

		// 298..367: second model (E2), same root as Diablos
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x12A);
			if (e < 0x46)
			{
				if (e == 0)
				{
					memcpy(e2 + 0x40, e1 + 0x40, 0x20);
					x::BindModel(e2, t->sections2, (uint32_t)(ModelBuffer() + 0x31B5C));
					x::SetAnim(e2, 0);
				}
				if (!Pause()) x::AdvanceAnim(e2);
				M.e2_draw = true;
				M.e2_blend = g_e2_drawn_tick + 1 == g_real_tick;
				g_e2_drawn_tick = g_real_tick;
				FrameCursor() = x::DrawModel(e2, ModelBuffer() + MB_MORPH, FrameCursor(), ModelDrawMode());
			}
		}

		// particle systems: bolts advance then draw, puffs draw (their moves come later)
		{
			uint8_t m;
			bool bolts = false;
			if (!Pause())
			{
				m = Mode();
				if (m == 2)
				{
					x::BoltAdvance();
					m = Mode();
					bolts = m == 2;
				}
			}
			else
			{
				m = Mode();
				bolts = m == 2;
			}
			if (bolts)
			{
				x::BoltDraw();
				M.pmode = 2;
				memcpy(M.pool, ModelBuffer() + MB_PARTICLES, sizeof(M.pool));
			}
			else if (m == 3)
			{
				M.pmode = 3;
				memcpy(M.pool, ModelBuffer() + MB_PARTICLES, MB_PUFFS_END - MB_PARTICLES);
				x::PuffDraw();
			}
		}

		// streams: the timeline waits (repeats the tick) while a load is busy
		{
			switch ((uint16_t)t->counter)
			{
			case 0:
				x::Load(0x2D7, (uint32_t)(ModelBuffer() + MB_STREAM), 3);
				x::Load(0x2D8, (uint32_t)(ModelBuffer() + MB_STREAM), 2);
				x::Load(0x2D9, (uint32_t)(ModelBuffer() + 0x2185C), 5);
				x::Load(0x2DA, (uint32_t)(ModelBuffer() + 0x3505C), 5);
				x::Load(0x2DB, (uint32_t)(ModelBuffer() + MB_STREAM), 5);
				break;
			case 0x20:
				if (!x::PreLoad0(0)) return 0;
				x::PlayStream(0x80, 1, 0x7F);
				break;
			case 0x57:
			case 0xA9:
			case 0x1D1:
				if (x::LoadBusy()) return 0;
				t->stream_ready = 0;
				x::TransStream((uint32_t)(ModelBuffer() + MB_STREAM), &t->stream_ready);
				break;
			case 0xD6:
				if (!x::PreLoad0(0)) return 0;
				t->stream_ready = 0;
				x::TransStream((uint32_t)(ModelBuffer() + MB_STREAM), &t->stream_ready);
				break;
			case 0x5B:
				if (!t->stream_ready) return 0;
				x::PlayStream(0x80, 1, 0x7F);
				x::Load(0x2DC, (uint32_t)(ModelBuffer() + MB_STREAM), 5);
				break;
			case 0xAD:
				if (!t->stream_ready) return 0;
				x::PlayStream(0x80, 1, 0x7F);
				x::Load(0x2DD, (uint32_t)(ModelBuffer() + MB_STREAM), 5);
				break;
			case 0xCB:
				QueueParticles().tail = nullptr;
				QueueParticles().head = nullptr;
				x::Load(0x2DE, (uint32_t)(ModelBuffer() + 0x3505C), 5);
				break;
			case 0xDA:
			case 0x1D5:
				if (!t->stream_ready) return 0;
				x::PlayStream(0x80, 1, 0x7F);
				break;
			case 0xED:
				if (x::LoadBusy()) return 0;
				break;
			case 0x147:
				x::Load(0x2DF, (uint32_t)(ModelBuffer() + 0x3505C), 5);
				break;
			case 0x167:
				if (x::LoadBusy()) return 0;
				x::TimUpload((uint32_t)(ModelBuffer() + 0x42E54));
				x::Load(0x2E0, (uint32_t)(ModelBuffer() + MB_STREAM), 3);
				break;
			case 0x1B4:
				if (x::LoadBusy()) return 0;
				x::PlayStream(0x80, 1, 0x7F);
				x::Load(0x2D7, (uint32_t)(ModelBuffer() + MB_STREAM), 5);
				break;
			case 0x1F4:
				x::Load(0x168, (uint32_t)(ModelBuffer() + MB_STREAM), 1);
				x::Load(0x169, (uint32_t)(ModelBuffer() + MB_STREAM), 1);
				x::Load(0x16A, (uint32_t)(ModelBuffer() + MB_STREAM), 0);
				break;
			default:
				break;
			}
		}
		x::PreLoad();

		// sound effects
		switch ((uint16_t)t->counter)
		{
		case 0: x::PlaySE(0xFD6640, 1, 0x80); break;
		case 0x5B: x::PlaySE(0xFD6644, 1, 0x80); break;
		case 0xEE: x::PlaySE(0xFD6648, 1, 0x80); break;
		case 0x14D: x::PlaySE(0xFD664C, 1, 0x80); break;
		case 0x192: x::PlaySE(0xFD6650, 1, 0x80); break;
		case 0x1C8: x::PlaySE(0xFD6654, 1, 0x80); break;
		default: break;
		}

		// 92..166: Diablos rises 0x20 per tick; every 8th tick a rising prim at a random vertex
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x5C);
			if (e < 0x4B && !Pause())
			{
				if ((e & 7) == 7)
				{
					int16_t v[4];
					int32_t r = x::Rand();
					x::VertexPos(e1, 0xFDBDA8 + 8 * ((uint32_t)(r * 10) >> 15), v);
					r = x::Rand();
					int32_t k = (r * 3) >> 15;
					uint32_t model;
					if (k == 0) model = (uint32_t)(ModelBuffer() + 0x4AFAC);
					else if (k == 1) model = (uint32_t)(ModelBuffer() + 0x4B558);
					else if (k == 2) model = (uint32_t)(ModelBuffer() + 0x4BBC4);
					else model = *(uint32_t *)v;
					if (e != 7) x::SpawnRising(v[0], v[1], v[2], -0x80, model);
				}
				Root(e1).t[1] += 0x20;
			}
		}

		// 173..202: burst of 32 puffs (177, 178), puffs move
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0xAD);
			if (e < 0x1E)
			{
				if (e >= 4 && e < 6)
				{
					for (int32_t cnt = 0x1F;; cnt--)
					{
						uint8_t *p = x::PuffAlloc();
						if (!p) break;
						p[0xF] = 0x10;
						*(uint32_t *)p = (cnt & 1) ? 0xFD69C0 : 0xFD6814;
						int32_t ang = x::Rand() & 0x7FF;
						int32_t rad = (x::Rand() & 0x1FF) + 0x200;
						int32_t cx = mul32(x::Cos(ang), rad) >> 12;
						*(int16_t *)(p + 4) = (int16_t)cx;
						int32_t sn = mul32(ComputeSin(ang), rad);
						int32_t q = rad / 4;
						p[0xC] = (uint8_t)(*(int16_t *)(p + 4) / 64);
						sn >>= 12;
						int32_t sy = (int16_t)sn;
						*(int16_t *)(p + 6) = (int16_t)(sn - 0x2000);
						p[0xD] = (uint8_t)(sy / 64);
						q = -q;
						*(int16_t *)(p + 8) = (int16_t)q;
						p[0xE] = (uint8_t)((int16_t)q / 16);
						*(int16_t *)(p + 8) = (int16_t)(q + FarZ());
						p[0xB] = (uint8_t)((x::Rand() & 0x3F) + 0x10);
						p[0xA] = (uint8_t)(ang / 16);
						if (cnt == 0) break;
					}
				}
				x::PuffMove();
			}
		}

		// 238..297: bolts from Diablos' bone 0x21 down to the ground
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0xEE);
			if (e < 0x3C)
			{
				if (e == 0) x::BoltParams(5, 0, -0x20);
				else if (e >= 8)
				{
					int16_t P[4];
					x::SpawnPosition(e1, 0x21, 0, P);
					BoltSpawnRec S;
					int32_t rad = (x::Rand() & 0xFFF) + 0x400;
					int32_t ang = x::Rand();
					int32_t cx = mul32(x::Cos(ang), rad) >> 12;
					int32_t sn = mul32(ComputeSin(ang), rad) >> 12;
					int32_t h7 = cx >> 7;
					S.pos[0] = (int16_t)(P[0] + h7);
					int32_t r = x::Rand();
					S.pos[1] = (int16_t)((r & 0xFF) + P[1] - 0x80);
					int32_t s7 = sn >> 7;
					S.pos[2] = (int16_t)(P[2] + s7);
					S.vel[1] = -100;
					S.target[0] = (int16_t)(P[0] + (cx >> 3));
					S.target[1] = (int16_t)(P[1] - rad / 8);
					S.shape[2] = 0;
					S.shape[1] = 0;
					S.target[2] = (int16_t)((sn >> 3) + P[2]);
					S.shape[0] = 0;
					S.vel[0] = (int16_t)-h7;
					S.vel[2] = (int16_t)-s7;
					S.width = 0x5DC;
					S.t = 0;
					S.dt = 0x100;
					S.budget = 0;
					S.intensity = 0x3000;
					S.decay = -0x200;
					S.shape[0] = (int16_t)(((x::Rand() * 100) >> 15) - 0x32);
					S.shape[1] = (int16_t)(((x::Rand() * 100) >> 15) - 0x32);
					S.fade = 0x1000;
					S.shape[2] = (int16_t)(((x::Rand() * 100) >> 15) - 0x32);
					x::BoltSpawn(&S);
				}
			}
		}

		// 298..317: bolts rising around Diablos
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x12A);
			if (e < 0x14)
			{
				if (e == 0)
				{
					var<int16_t>(0x2505200) = 9;
					var<int16_t>(0x2505202) = 0x20;
					var<int16_t>(0x2505204) = 0x800;
					var<int16_t>(0x2505206) = -4;
				}
				int16_t P[4];
				x::SpawnPosition(e1, 0x21, 0, P);
				int32_t rad = (x::Rand() & 0x3FF) + 0xBB8;
				int32_t half = rad / 2;
				for (int k = 2; k; k--)
				{
					BoltSpawnRec S;
					int32_t ang = x::Rand();
					int32_t cx = mul32(x::Cos(ang), rad) >> 12;
					int32_t sn = mul32(ComputeSin(ang), rad) >> 12;
					S.vel[1] = 200;
					S.pos[0] = (int16_t)((cx >> 8) + P[0]);
					S.pos[2] = (int16_t)((sn >> 8) + P[2]);
					S.target[0] = (int16_t)((cx >> 4) + P[0] + cx);
					S.pos[1] = P[1];
					S.target[1] = (int16_t)(P[1] - half);
					S.shape[2] = 0;
					S.shape[1] = 0;
					S.shape[0] = 0;
					S.target[2] = (int16_t)((sn >> 4) + P[2] + sn);
					S.vel[0] = (int16_t)(cx >> 6);
					S.vel[2] = (int16_t)(sn >> 6);
					S.width = 0xBB8;
					S.t = 0;
					S.dt = 0x200;
					S.budget = 2;
					S.intensity = 0x3000;
					S.decay = -0x200;
					S.shape[0] = (int16_t)(((x::Rand() * 300) >> 15) - 0x96);
					S.shape[1] = (int16_t)(((x::Rand() * 300) >> 15) - 0x96);
					S.fade = 0x1000;
					S.shape[2] = (int16_t)(((x::Rand() * 300) >> 15) - 0x96);
					x::BoltSpawn(&S);
				}
			}
		}

		// 376..405: bolts shooting out of the well
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x178);
			if (e < 0x1E)
			{
				int32_t a4 = (int32_t)(((e + 8) << 12) / 38u);
				if (e == 0) x::BoltParams(7, 0x1000, -0x100);
				for (int k = 2; k; k--)
				{
					BoltSpawnRec S;
					int32_t ang = x::Rand();
					int32_t rad = (x::Rand() & 0x3FF) + 0x400;
					int32_t cx = mul32(x::Cos(ang), rad) / 4096;
					int32_t sn = mul32(ComputeSin(ang), rad) / 4096;
					S.pos[1] = 0;
					S.pos[0] = (int16_t)(CenterX() + cx);
					S.pos[2] = (int16_t)(sn + CenterZ());
					S.target[0] = (int16_t)((cx >> 1) + cx + CenterX());
					int32_t s = Ease(a4);
					int32_t w = ComputeSin(rad / 4);
					S.target[1] = (int16_t)(s * 4 - 0x4200 - w);
					S.target[2] = (int16_t)((sn >> 1) + sn + CenterZ());
					int32_t kk = mul32(x::Rand(), a4) >> 15;
					S.vel[0] = (int16_t)(mul32(cx, kk) >> 12);
					S.vel[1] = 0;
					S.vel[2] = (int16_t)(mul32(sn, kk) >> 12);
					S.shape[2] = 0;
					S.shape[1] = 0;
					S.shape[0] = 0;
					S.width = 0x1770;
					S.t = 0;
					S.dt = 0x200;
					S.budget = 2;
					S.intensity = 0x3000;
					S.decay = -0x200;
					S.shape[0] = (int16_t)((x::Rand() & 0xFF) - 0x80);
					S.shape[1] = (int16_t)((x::Rand() & 0xFF) - 0x80);
					S.fade = 0x1000;
					S.shape[2] = (int16_t)((x::Rand() & 0xFF) - 0x80);
					x::BoltSpawn(&S);
				}
			}
		}

		// 406..421: bolts rising in a widening ring
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x196);
			if (e < 0x10)
			{
				int32_t q = (int32_t)((e << 12) / 30u);
				if (e == 0) x::BoltParams(7, 0x1000, 0);
				int32_t rad = q / 4 + 0xBB8;
				int16_t ty = (int16_t)shl32(-rad, 2);
				for (int k = 2; k; k--)
				{
					BoltSpawnRec S;
					int32_t ang = x::Rand();
					int32_t cx = mul32(x::Cos(ang), rad) >> 12;
					int32_t sn = mul32(ComputeSin(ang), rad) >> 12;
					S.pos[1] = 0;
					S.pos[0] = (int16_t)(cx + CenterX());
					S.pos[2] = (int16_t)(CenterZ() + sn);
					S.target[0] = (int16_t)((cx >> 1) + cx + CenterX());
					S.target[1] = ty;
					S.target[2] = (int16_t)((sn >> 1) + sn + CenterZ());
					S.vel[0] = (int16_t)(mul32(x::Rand(), cx) >> 22);
					S.vel[1] = 0;
					S.vel[2] = (int16_t)(mul32(x::Rand(), sn) >> 22);
					S.shape[2] = 0;
					S.shape[1] = 0;
					S.shape[0] = 0;
					S.width = 0x1770;
					S.t = 0;
					S.dt = 0x200;
					S.budget = 2;
					S.intensity = 0x3000;
					S.decay = -0x200;
					S.shape[0] = (int16_t)((x::Rand() & 0xFF) - 0x80);
					S.shape[1] = (int16_t)((x::Rand() & 0xFF) - 0x80);
					S.fade = 0x1000;
					S.shape[2] = (int16_t)((x::Rand() & 0xFF) - 0x80);
					x::BoltSpawn(&S);
				}
			}
		}

		// 466..473: bursts at random bones, Diablos darkens
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x1D2);
			if (e < 8)
			{
				if (e == 0) InitTaskQueuePool(&QueueParticles(), ModelBuffer() + 0x3505C, 0xE8, 0x10);
				for (int k = 2; k; k--)
				{
					BurstNode *b = (BurstNode *)AddTaskToQueue(&QueueParticles(), ORIG_BurstTask);
					if (!b) continue;
					int32_t r = x::Rand();
					uint32_t nb = **(uint8_t **)(e1 + 0x64);
					x::SpawnPosition(e1, (r * (int32_t)nb) >> 15, 0, b->pos);
					b->shade = -0x100;
					b->scale = (x::Rand() & 0x7FF) + 0x800;
					x::Decode((uint32_t)(ModelBuffer() + 0x42620), b->layout, 0xB8);
				}
				uint8_t g = (uint8_t)(0x70 - (uint8_t)((uint8_t)e << 4));
				e1[0x2A] = g;
				e1[0x29] = g;
				e1[0x28] = g;
			}
		}

		// 469..498: Diablos dissolves into puffs
		{
			uint32_t e = (uint32_t)((int32_t)t->counter - 0x1D5);
			if (e < 0x1E)
			{
				if (e == 0)
				{
					x::ResetPuffs();
					x::PuffsFromModel(e1);
				}
				x::PuffMove();
			}
		}

		// 499: damage, the targets' effect flags are cleared
		if (t->counter == 0x1F3)
		{
			for (int k = 0; k < TargetCount(); k++) *(uint16_t *)t->targets[k].entity &= 0xFFF3;
			Mode() = 0;
			uint8_t *a = ActionData();
			x::ApplyResult(*(void **)(a + 8), a[0x10]);
		}

		CameraTick(t->counter, true);

		if (t->counter == 0x20E) x::ReleaseVoice(t->voice);
		t->counter++;
		if (t->counter < 0x212) return 0;
		if (t->counter == 0x212)
			for (int k = 0; k < TargetCount(); k++) *(uint16_t *)t->targets[k].entity &= 0xFFF3;
		if (x::PreLoad1(2)) return TASK_END;
		t->counter = 0x222;
		return 0;
	}

	// ------------------------------------------------------------------
	// Prim particles
	// ------------------------------------------------------------------
	static void CtxRising(PrimCtx &c, const int16_t pos[3], int16_t shade)
	{
		memset(&c, 0, sizeof(c));
		c.pos[0] = pos[0];
		c.pos[1] = pos[1];
		c.pos[2] = pos[2];
		c.pad06 = shade; // the dword copy of +0x10 (z, shade); never read
		c.morph = Morph();
		c.shade = shade;
		c.scroll_sel = 0xFE0FB4; // "0"
		c.draw_sel = 0xFE0FC8;   // "1"
	}

	struct RisingMemo { int16_t pos[3]; };
	static NodeMemo<RisingMemo, 64> g_rising_memo;

	// 0x659960: prim-model player that rises 0x18 per tick, ends with its animation
	static uint32_t __cdecl RisingTask(TaskNode *n)
	{
		RisingNode *p = (RisingNode *)n;
		PrimCtx c;
		CtxRising(c, p->pos, p->shade);
		if (RisingMemo *m = g_rising_memo.put(n)) memcpy(m->pos, p->pos, sizeof(m->pos));
		if (!Play(p->layout, c)) return TASK_END;
		if (!Pause()) p->pos[1] += 0x18;
		return 0;
	}

	static void CtxBurst(PrimCtx &c, const BurstNode *p)
	{
		memset(&c, 0, sizeof(c));
		c.pos[0] = p->pos[0];
		c.pos[1] = p->pos[1];
		c.pos[2] = p->pos[2];
		c.pad06 = p->shade;
		c.scale[0] = c.scale[1] = c.scale[2] = p->scale;
		c.use_scale = 1;
		c.morph = Morph();
		c.shade = p->shade;
		c.scroll_sel = c.draw_sel = 0xFE0FB4; // "0"
	}

	// 0x659860: scaled prim-model player at a bone of Diablos, ends with its animation
	static uint32_t __cdecl BurstTask(TaskNode *n)
	{
		BurstNode *p = (BurstNode *)n;
		PrimCtx c;
		CtxBurst(c, p);
		return Play(p->layout, c) ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Held frames (30 fps). Exact vanilla shapes: everything that moves is drawn between the
	// state drawn on the last real tick and the state of the next one; what jumps (cuts, stage
	// changes, flipbook frames, particle growth / spawns) keeps its 15 Hz steps. Nothing here
	// changes game state: no RNG, no spawn, no camera write; the model-buffer scratch and the
	// particle pools the held draw rewrites are put back.
	// ------------------------------------------------------------------
	static void PoseBlend(uint8_t *sk, const uint8_t *from, int num, int den)
	{
		// sk holds the pose after the tick (the one the next tick draws), from the pose drawn
		// this tick: write the midpoint (angles the short way round) into sk's pose fields
		int nb = sk[0];
		bool scaled = (sk[1] & 1) != 0;
		for (int a = 0; a < 3; a++)
		{
			int16_t *p = (int16_t *)(sk + 8) + a;
			int16_t f = ((const int16_t *)(from + 8))[a];
			*p = (int16_t)(f + ((int32_t)*p - f) * num / den);
		}
		for (int b = 0; b < nb; b++)
		{
			int16_t *p = (int16_t *)(sk + 16 + 48 * b + 4);
			const int16_t *f = (const int16_t *)(from + 16 + 48 * b + 4);
			for (int a = 0; a < 3; a++)
			{
				int32_t d = (((int32_t)p[a] - f[a] + 2048) & 4095) - 2048;
				p[a] = (int16_t)(f[a] + d * num / den);
			}
			if (scaled)
				for (int a = 3; a < 6; a++) p[a] = (int16_t)(f[a] + ((int32_t)p[a] - f[a]) * num / den);
		}
	}

	static uint8_t g_skel_save[SKEL_MAX];

	// a model drawn between its memo pose and its current pose; draw() is the tick's draw call
	template<typename Draw>
	static void ModelHeld(uint8_t *e, const PoseMemo &pm, bool blend, int num, int den, Draw draw)
	{
		uint32_t size = 0;
		uint8_t *sk = Skeleton(e, &size);
		if (!sk) return;
		memcpy(g_skel_save, sk, size);
		if (blend && pm.ok && pm.size == size)
		{
			PoseBlend(sk, pm.pose, num, den);
			BuildBoneMatricesFromPose(e + 0x60);
		}
		draw();
		memcpy(sk, g_skel_save, size);
	}

	// next-tick value if the timeline advances by one tick inside [lo, hi), else hold
	static int32_t Toward(int32_t now, int32_t next, int32_t c, int32_t cn, int32_t lo, int32_t hi, int num, int den)
	{
		if (cn != c + 1 || cn < lo || cn >= hi) return now;
		return lerp_i(now, next, num, den);
	}

	static uint8_t g_layout_save[0x1E8];

	static void BoltsHeld(int num, int den)
	{
		// the chains as drawn, drifted towards the next tick: vertices by their velocity,
		// brightness by one decay step, chain fade by one step (growth keeps the 15 Hz batches)
		uint8_t *base = ModelBuffer() + MB_PARTICLES;
		memcpy(base, g_tm.pool, sizeof(g_tm.pool));
		if (!Pause())
		{
			uint8_t *ch = ModelBuffer() + MB_CHAINS;
			for (int k = 0; k < 64; k++, ch += 0x30)
			{
				uint8_t *v = *(uint8_t **)ch;
				if (!v) continue;
				int16_t &fade = *(int16_t *)(ch + 0x2E);
				int32_t fn = fade - 0x100;
				if (fn < 0) fn = 0;
				fade = (int16_t)lerp_i(fade, fn, num, den);
				for (int guard = 0; v && guard < 0x200; guard++)
				{
					if (v < base || v >= ModelBuffer() + MB_CHAINS) break;
					int16_t *pos = (int16_t *)v, *vel = (int16_t *)(v + 8);
					for (int a = 0; a < 3; a++) pos[a] = (int16_t)(pos[a] + vel[a] * num / den);
					int16_t &in = *(int16_t *)(v + 0x10);
					int32_t i1 = (int16_t)(in + *(int16_t *)(v + 0x12));
					if (i1 < 0) i1 = 0;
					in = (int16_t)lerp_i(in, i1, num, den);
					v = *(uint8_t **)(v + 0x14);
				}
			}
		}
		x::BoltDraw();
	}

	static void PuffsHeld(int32_t c, int num, int den)
	{
		// puffs as drawn, moved towards their state after this tick's moves; the pool resets
		// (0, 30, 90, 202, 469) and stage switches keep the drawn state
		static uint8_t cur[MB_PUFFS_END - MB_PARTICLES];
		uint8_t *pool = ModelBuffer() + MB_PARTICLES;
		memcpy(cur, pool, sizeof(cur));
		bool move = Mode() == 3 && c != 0 && c != 0x1E && c != 0x5A && c != 0xCA && c != 0x1D5;
		memcpy(pool, g_tm.pool, sizeof(cur));
		if (move)
			for (int i = 0; i < 0xA0; i++)
			{
				uint8_t *p = pool + 0x10 * i;
				const uint8_t *q = cur + 0x10 * i;
				if (!p[0xF] || *(const uint32_t *)q != *(const uint32_t *)p) continue;
				int16_t *pp = (int16_t *)(p + 4);
				const int16_t *qp = (const int16_t *)(q + 4);
				bool jump = false;
				for (int a = 0; a < 3; a++)
				{
					int32_t d = (int32_t)qp[a] - pp[a];
					if (d > 0x1000 || d < -0x1000) jump = true;
				}
				if (jump) continue;
				for (int a = 0; a < 3; a++) pp[a] = (int16_t)lerp_i(pp[a], qp[a], num, den);
				if (q[0xB] <= p[0xB]) p[0xB] = (uint8_t)lerp_i(p[0xB], q[0xB], num, den);
			}
		x::PuffDraw();
	}

	static void TimelineHeld(TimelineNode *t, int num, int den)
	{
		const TimelineMemo &M = g_tm;
		if (M.tick != g_real_tick || M.node != t) return;
		int32_t c = M.c, cn = t->counter;
		uint8_t *e1 = E1(t), *e2 = E2(t);

		if (M.play_a)
		{
			PrimCtx ctx = M.ctx_a;
			if (cn == c + 1 && (uint32_t)(cn - 1) < 0x211 && (uint32_t)(cn - 1) >= 8 && (uint32_t)(cn - 1) < 0x209)
				ctx.fade = (int16_t)lerp_i(M.ctx_a.fade, FadeA((uint32_t)(cn - 1)), num, den);
			PlayHeld(t->prim_a, ctx, num, den);
		}
		if (M.grow)
		{
			int32_t d = GrowDiag(M.grow_e);
			d = Toward(d, GrowDiag(M.grow_e + 1), c, cn, 0x1F, 0x5B, num, den);
			DrawGrowing(d, M.grow_e < 0x1E ? -0x4000 : -0x2000);
		}
		if (M.morph)
		{
			int32_t tt = (int32_t)((M.morph_e << 12) / 12u);
			tt = Toward(tt, (int32_t)(((M.morph_e + 1) << 12) / 12u), c, cn, 0x5B, 0x67, num, den);
			x::MorphPair(0, -0x2000, (int16_t)FarZ(), (uint32_t)(ModelBuffer() + 0x4C244), t->flip, 2, tt, t->morph_b, 2, tt);
		}
		for (int i = 0; i < M.nflip; i++) x::DrawPrimAt(M.flip[i].x, M.flip[i].y, M.flip[i].z, M.flip[i].model);
		if (M.play_c)
		{
			PrimCtx ctx = M.ctx_c;
			if (M.restart_c)
			{
				memcpy(g_layout_save, t->prim_c, sizeof(g_layout_save));
				memcpy(t->prim_c, M.layout_c, sizeof(g_layout_save));
				PlayHeld(t->prim_c, ctx, num, den);
				memcpy(t->prim_c, g_layout_save, sizeof(g_layout_save));
			}
			else PlayHeld(t->prim_c, ctx, num, den);
		}
		if (M.play_b && !M.b406)
		{
			PrimCtx ctx = M.ctx_b;
			PlayHeld(t->prim_b, ctx, num, den);
		}
		if (M.sink)
			DrawSinking(M.sink_k, (cn == c + 1 && M.sink_k < 7) ? 0x200 * num / den : 0);
		if (M.well)
		{
			uint32_t e = M.well_e;
			bool next = cn == c + 1 && e + 1 < 0x26;
			int32_t depth = WellDepth(e);
			if (next) depth = lerp_i(depth, WellDepth(e + 1), num, den);
			int32_t scroll = (int32_t)(e << 4) + (next ? 16 * num / den : 0);
			for (int k = 0; k < TargetCount() && k < 10; k++)
			{
				Target &tg = t->targets[k];
				int32_t save58 = *(int32_t *)(tg.entity + 0x58);
				int32_t scale[3];
				SquashTarget(tg, depth, scale);
				x::DrawEntityScaled(tg.entity, scale);
				*(int32_t *)(tg.entity + 0x58) = save58;
			}
			x::DrawWell(CenterX(), depth - 0x1000, CenterZ(), scroll);
		}
		if (M.tile)
		{
			int32_t lv = FlashTileLevel(M.tile_e);
			if (cn == c + 1 && M.tile_e + 1 < 0xC) lv = lerp_i(lv, FlashTileLevel(M.tile_e + 1), num, den);
			PacketCursor() = x::Tile(0xFF, 0xFF, 0xFF, lv, PacketCursor());
		}
		if (M.play_b && M.b406)
		{
			PrimCtx ctx = M.ctx_b;
			if (cn == c + 1 && (uint32_t)(cn - 0x196) < 0x32) ctx.scroll = (int16_t)(ctx.scroll + 4 * num / den);
			PlayHeld(t->prim_b, ctx, num, den);
		}

		if (M.e1_draw)
		{
			Mat4x3 root = Root(e1);
			uint32_t colour = *(uint32_t *)(e1 + 0x28);
			// root y: this tick's rise (92..166) happened after the draw; the next tick sinks by 10
			// before its draw (268..297); the jump at 203 holds
			Mat4x3 r = M.e1_root;
			if (cn == c + 1 && cn != 0xCB && !Pause())
			{
				int32_t next = root.t[1] + ((uint32_t)(cn - 0x10C) < 0x1E ? -10 : 0);
				r.t[1] = lerp_i(M.e1_root.t[1], next, num, den);
			}
			Root(e1) = r;
			uint8_t *col = e1 + 0x28;
			const uint8_t *c0 = (const uint8_t *)&M.e1_colour, *c1 = (const uint8_t *)&colour;
			for (int k = 0; k < 3; k++) col[k] = (uint8_t)lerp_i(c0[k], c1[k], num, den);
			int kind = M.e1_draw;
			ModelHeld(e1, M.p1, M.e1_blend, num, den, [&]() {
				if (kind == 1) x::DrawStylised(e1, -0x2000, 0x80, 2, ModelBuffer() + MB_MORPH);
				else if (kind == 2) x::DrawStylised(e1, -0x22BC, 0x80, 0, ModelBuffer() + MB_MORPH);
				else FrameCursor() = x::DrawModel(e1, ModelBuffer() + MB_MORPH, FrameCursor(), ModelDrawMode());
			});
			Root(e1) = root;
			*(uint32_t *)(e1 + 0x28) = colour;
		}
		if (M.e2_draw)
			ModelHeld(e2, M.p2, M.e2_blend, num, den, [&]() {
				FrameCursor() = x::DrawModel(e2, ModelBuffer() + MB_MORPH, FrameCursor(), ModelDrawMode());
			});

		if (M.pmode == 2) BoltsHeld(num, den);
		else if (M.pmode == 3) PuffsHeld(c, num, den);
	}

	static void RisingHeld(RisingNode *p, int num, int den)
	{
		const RisingMemo *m = g_rising_memo.get(p);
		if (!m) return;
		int16_t pos[3];
		for (int k = 0; k < 3; k++) pos[k] = (int16_t)lerp_i(m->pos[k], p->pos[k], num, den);
		PrimCtx c;
		CtxRising(c, pos, p->shade);
		PlayHeld(p->layout, c, num, den);
	}

	static void BurstHeld(BurstNode *p, int num, int den)
	{
		PrimCtx c;
		CtxBurst(c, p);
		PlayHeld(p->layout, c, num, den);
	}

	static uint8_t g_held_packets[0x80000]; // module arena 0x20000, then the frame arena (model draws)
	static uint8_t g_scratch_save[MB_STREAM_STATE - MB_MORPH];
	static uint8_t g_pool_save[MB_CHAINS_END - MB_PARTICLES];

	static bool HeldReady() { return g_ported_tick == g_real_tick; }

	static TimelineNode *Timeline()
	{
		for (TaskNode *t = QueueTimeline().head; t; t = t->next)
			if ((uint32_t)t->func == ORIG_TimelineTask) return (TimelineNode *)t;
		return nullptr;
	}

	// mirrors the master's queue order; packets go to a private buffer (the module cursor and the
	// frame arena are redirected and put back); the morph scratch and the particle pools the held
	// draw rewrites are saved and put back, and so is the effect-camera matrix
	static void HeldFrame(int num, int den)
	{
		uint8_t *MB = ModelBuffer();
		if (!MB) return;
		uint32_t cursor = PacketCursor(), frame_cursor = FrameCursor();
		Mat4x3 effect_camera = EffectCamera();
		memcpy(g_scratch_save, MB + MB_MORPH, sizeof(g_scratch_save));
		memcpy(g_pool_save, MB + MB_PARTICLES, sizeof(g_pool_save));
		PacketCursor() = (uint32_t)g_held_packets;
		FrameCursor() = (uint32_t)g_held_packets + 0x20000;

		if (TimelineNode *t = Timeline()) TimelineHeld(t, num, den);
		if (QueueParticles().head)
		{
			EffectCameraMatrix(&Camera(), &EffectCamera());
			for (TaskNode *t = QueueParticles().head; t; t = t->next)
			{
				if ((uint32_t)t->func == ORIG_RisingTask) RisingHeld((RisingNode *)t, num, den);
				else if ((uint32_t)t->func == ORIG_BurstTask) BurstHeld((BurstNode *)t, num, den);
			}
		}

		memcpy(MB + MB_MORPH, g_scratch_save, sizeof(g_scratch_save));
		memcpy(MB + MB_PARTICLES, g_pool_save, sizeof(g_pool_save));
		EffectCamera() = effect_camera;
		PacketCursor() = cursor;
		FrameCursor() = frame_cursor;
	}
}

	// held-frame camera of effect 325 at tick + num / den: the timeline's camera code for the
	// counter it holds now (the next tick) is run on the real camera words and put back (it only
	// writes those words and the GTE data registers), then the in-between camera; cuts and the
	// first tick of a camera move that starts elsewhere hold. false = no timeline this tick.
	bool mag325_held_camera(int num, int den, int16_t world[3], int16_t lookat[3])
	{
		using namespace d325;
		if (!HeldReady() || g_tm.tick != g_real_tick) return false;
		TimelineNode *t = Timeline();
		if (!t || t != g_tm.node) return false;
		int16_t now_w[3], now_l[3];
		for (int i = 0; i < 3; i++) { now_w[i] = CamWorld(i); now_l[i] = CamLookAt(i); }
		uint8_t save[16];
		memcpy(save, (void *)0xB8B7F0, sizeof(save));
		int r = CameraTick(t->counter, false);
		int16_t next_w[3], next_l[3];
		for (int i = 0; i < 3; i++) { next_w[i] = CamWorld(i); next_l[i] = CamLookAt(i); }
		memcpy((void *)0xB8B7F0, save, sizeof(save));
		bool move = (r & 1) && !(r & 2);
		for (int i = 0; i < 3; i++)
		{
			world[i] = move ? (int16_t)lerp_i(now_w[i], next_w[i], num, den) : now_w[i];
			lookat[i] = move ? (int16_t)lerp_i(now_l[i], next_l[i], num, den) : now_l[i];
		}
		return true;
	}

	void register_mag325_diablos()
	{
		register_port(d325::ORIG_SequenceTask, (void *)d325::SequenceTask, "D325 SequenceTask", 325);
		register_port(d325::ORIG_TimelineTask, (void *)d325::TimelineTask, "D325 TimelineTask", 325, true);
		register_port(d325::ORIG_RisingTask, (void *)d325::RisingTask, "D325 RisingTask", 325, true);
		register_port(d325::ORIG_BurstTask, (void *)d325::BurstTask, "D325 BurstTask", 325, true);
		register_module_held(325, d325::HeldReady, d325::HeldFrame);
		register_module_camera(325, mag325_held_camera);
	}
}
