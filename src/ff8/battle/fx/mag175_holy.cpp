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

// Effect 175: Holy (spell, MAG_175_*, file mag174).
//
// Structure (setup MAG_175_HOLY 0x5D1AD0, file loader MAG_175_HOLY_FL 0x5D1AB0 = mag174.tim):
//   RootTask (0x5D44C0) - alternates the packet arena (magic buffer + 0 / + 0xC000), runs the
//     effect queue; when it is empty spawns the texture restore task (unless cast flag bit 1).
//   Director (0x5D1BD0, one per action, frame 0..0x6F) - frame 0: anchor on the target;
//     1: rising sparkle emitter + sound; 3: builds the screen grid (15 x 21 jittered vertices, 14 x 20
//     cell centres, 4 textured triangles per cell); 4: the triangles' shatter parameters;
//     5: the shatter task; 6: two orbit controllers; 0x10: screen motes; 0x1E: texture upload,
//     triangles go flat; 0x1F: spinning ring + two scrolling waves; 0x1F..0x49 every 6: an
//     expanding ring; 0x2D: 12 light pillars around the target; 0x42: beam of 8 sprites toward
//     the camera; 0x4E: two spinning disks; 0x5A: flash; 0x67..0x6F: screen flash ramp, stage groups
//     hidden, the side's entities fade back in; 0x6A: damage; 0x6F: the next action's director.
//   Shatter (0x5D2910) - frames 0..9: renders the battle stage into VRAM (576, 256) (the stage
//     groups' bone matrices rebuilt); every frame while any triangle lives: the grid drawn with that
//     capture, triangles past their delay fly apart and shrink (update inside the draw);
//     frame 6: the entities of the target's opposite side fade out.
//   Fade out / in (0x5D3030 / 0x5D27C0) - an entity's colour (entity +0x28 / +0x2C) toward black /
//     back, through the GTE depth cue.
//   Sparkles (0x5D3180) - pool of 100 sparkles (0x2374D90) spiralling inwards, 2 spawned per frame.
//   Orbit controller (0x5D3420) - rotation matrix (0x23745B0 + 0x20 * index) that tilts over time;
//     frame 0 spawns 8 orbiting sparks (0x5D3590) on it.
//   Motes (0x5D37B0) - pool of 80 screen-space motes (0x2374610) rising and brightening.
//   Ring (0x5D3A50), waves (0x5D3B80, via MAG_066_sub_650720), expanding rings (0x5D3CE0), light
//     pillars (0x5D3E20), beam (0x5D3FB0), disks (0x5D42B0), flash (0x5D43B0) - prim models /
//     sprite sequences with frame-driven fades.
// Every task except the root tests the draw-only flags (battle_to_update_flags & 0x201): it then
// only draws (the director, shatter and fades do nothing that moves).
// Module globals: 0x2374578..0x2394208 (fade side, root pool / queue, orbit matrices, mote pool,
// sparkle pool, grid cell centres, effect queue + pool, fade flags, triangles, triangle packets,
// grid vertices, texture file, texture base 0x23941BC, packet cursor 0x23941C0, sprite matrix
// 0x23941E8).

#include "mag_common.h"

namespace ff8fx
{
namespace holy175
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint32_t &TexFile() { return var<uint32_t>(0x2392CE0); }            // mag174.tim
	inline uint32_t &TexBase() { return var<uint32_t>(0x23941BC); }            // Magic_TextureOFF (magic buffer)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x23941C0); }
	inline CastContext *&Ctx() { return var<CastContext *>(0x2377744); }
	inline int32_t &FadeSide() { return var<int32_t>(0x2374578); }             // target slot: fades act on the other side
	inline int32_t &GridTextured() { return var<int32_t>(0x2377740); }         // 1 until director frame 0x1E
	inline int32_t &GridActive() { return var<int32_t>(0x2376880); }           // cleared when no triangle is left
	inline TaskQueuePair &RootQueues() { return var<TaskQueuePair>(0x23745A0); } // .first = root / texture restore
	inline TaskQueue &Tasks() { return var<TaskQueue>(0x2376870); }            // every effect task (100 nodes of 0x24)
	inline uint32_t &FadeFlag(int slot) { return var<uint32_t>(0x2377698 + 0x18 * slot); } // entity faded out
	inline Mat4x3 *OrbitMatrix(int index) { return (Mat4x3 *)(0x23745B0 + 0x20 * index); }
	inline Mat4x3 *SpriteMatrix() { return (Mat4x3 *)0x23941E8; }
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }              // g_Battle_FrameRenderListBase
	inline uint32_t &FrameArena() { return var<uint32_t>(0x1D8E054); }         // battle_texture_data_ptr: frame packet arena
	inline int16_t ProjH() { return var<int16_t>(0x1D8E038); }                 // GTE projection distance
	inline uint8_t DisplayBuffer() { return var<uint8_t>(0x1D96A80); }

	static const uint32_t ORIG_RootTask = 0x5D44C0;
	static const uint32_t ORIG_RestoreTask = 0x5D4540;
	static const uint32_t ORIG_DirectorTask = 0x5D1BD0;
	static const uint32_t ORIG_ShatterTask = 0x5D2910;
	static const uint32_t ORIG_FadeInTask = 0x5D27C0;
	static const uint32_t ORIG_FadeOutTask = 0x5D3030;
	static const uint32_t ORIG_SparkleTask = 0x5D3180;
	static const uint32_t ORIG_OrbitCtrlTask = 0x5D3420;
	static const uint32_t ORIG_OrbitTask = 0x5D3590;
	static const uint32_t ORIG_MoteTask = 0x5D37B0;
	static const uint32_t ORIG_RingTask = 0x5D3A50;
	static const uint32_t ORIG_WaveTask = 0x5D3B80;
	static const uint32_t ORIG_ExpandTask = 0x5D3CE0;
	static const uint32_t ORIG_PillarTask = 0x5D3E20;
	static const uint32_t ORIG_BeamTask = 0x5D3FB0;
	static const uint32_t ORIG_DiskTask = 0x5D42B0;
	static const uint32_t ORIG_FlashTask = 0x5D43B0;

	static const void *const SOUND_Holy = (const void *)0xD59180;
	static const void *const TIM_Upload = (const void *)0xD45EAC;  // texture uploaded at director frame 0x1E
	static const uint32_t PILLAR_Delays = 0xD59188;  // 12 x (word delay / 3, word unused)
	static const uint32_t BEAM_Sprites = 0xD591D0;   // 8 x (word frame, word size, word distance, word)
	static const uint32_t WAVE_Textures = 0xD59210;  // 16 dwords
	static const uint32_t STAGE_Buckets = 0xD59250;  // 4 x (dword OT index, dword mode)
	static const uint32_t STAGE_EnvRestore = 0xD592C0; // 2 x 0x18: draw environment of each buffer
	static const uint32_t DRAWENV = 0x1D969C8;         // 2 x 0x5C
	// sprite sequences / prim models
	static const uint32_t SEQ_Sparkle = 0xD524C4;
	static const uint32_t SEQ_OrbitSpark = 0xD523E8;
	static const uint32_t SEQ_Mote = 0xD52374;
	static const uint32_t SEQ_Beam = 0xD522CC;
	static const uint32_t MODEL_Ring = 0xD54588;
	static const uint32_t MODEL_Wave = 0xD56C58;
	static const uint32_t MODEL_Expand = 0xD53BA0;
	static const uint32_t MODEL_Pillar = 0xD53688;
	static const uint32_t MODEL_Disk = 0xD52DD8;
	static const uint32_t MODEL_Flash = 0xD527F8;

	// grid: 15 x 21 vertices, 14 x 20 cell centres, 14 x 20 x 4 triangles, their packets (2 per buffer)
	static const uint32_t GRID_Vertices = 0x2392CE8, GRID_VerticesEnd = 0x2394098;
	static const uint32_t GRID_Centres = 0x23756F0, GRID_CentresEnd = 0x2376870;
	static const uint32_t GRID_Triangles = 0x2377748;
	static const uint32_t GRID_Packets = 0x23814E0;
	static const uint32_t GRID_CellPointers = 0x23814C8; // 5 pointers of the last cell built
	static const int GRID_TriangleCount = 14 * 20 * 4;
	static const uint32_t SPARKLES = 0x2374D90, SPARKLES_End = 0x23756F0; // 100 x 0x18
	static const uint32_t MOTES = 0x2374610, MOTES_End = 0x2374D90;       // 80 x 0x18
	static const uint32_t STAGE_AnimHeaders = 0x1D989A4;                   // 4 stage groups, 0x2C apart

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline void QueueTimUpload(const void *tim) { fn<void (__cdecl *)(const void *)>(0x505E30)(tim); } // Battle_QueueTIMUpload_GetEOF
	inline void MatrixMultiplyVector(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); } // b = a * b (3x3)
	inline Mat4x3 *ShadowRotation(const void *pos, int32_t a, int32_t b) { return fn<Mat4x3 *(__cdecl *)(const void *, int32_t, int32_t)>(0x571BC0)(pos, a, b); }
	inline uint32_t RenderWaveModel(void *header, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x650720)(header, ot, mode, cursor); } // MAG_066_sub_650720
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
	inline uint32_t RenderGeometry(void *group, void *header, uint32_t ot, uint32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, uint32_t, uint32_t)>(0x5099D0)(group, header, ot, mode, cursor); }
	// packet builders (SSIGPU): draw environment, draw area, draw offset
	inline void BuildDrawEnvPacket(void *packet, uint32_t env) { fn<void (__cdecl *)(void *, uint32_t)>(0x45C0F0)(packet, env); }
	inline void BuildDrawAreaPacket(void *packet, const int16_t *rect) { fn<void (__cdecl *)(void *, const int16_t *)>(0x45C940)(packet, rect); }
	inline void BuildDrawOffsetPacket(void *packet, const void *xy) { fn<void (__cdecl *)(void *, const void *)>(0x45C9B0)(packet, xy); }
	// GTE screen offset (OFX / OFY) read / write (sub_56CD10 / Call_Bs_ParseCamera)
	inline void GetScreenOffset(int32_t *x, int32_t *y) { fn<void (__cdecl *)(int32_t *, int32_t *)>(0x56CD10)(x, y); }
	inline void SetScreenOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x56CCE0)(x, y); }
	// GTE: far colour, RGB, depth cue, RGB FIFO, three vertices
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); }
	inline void GteSetRGB(const void *rgb) { fn<void (__cdecl *)(const void *)>(0x45E110)(rgb); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteReadRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
	inline void GteLoadV012(const void *v0, const void *v1, const void *v2) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(v0, v1, v2); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadSXY012(void *s0, void *s1, void *s2) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(s0, s1, s2); }

	// ------------------------------------------------------------------
	// Node layouts (effect queue: pool of 100 nodes of 0x24 bytes; root queue: 2 of 0x10)
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C bit 0 = packet arena
		int16_t pad0E;
	};
	struct RestoreNode
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		uint16_t done;    // +0x0E set by the texture restore task
	};
	struct DirectorNode
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C 0..0x6F
		int16_t action;   // +0x0E
		int16_t pos[4];   // +0x10 target effect anchor (x, y, z, height)
		uint8_t pad18[8];
		int16_t target;   // +0x20 target slot
		int16_t pad22;
	};
	struct ShatterNode
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C 0..0x32
		uint8_t pad0E[0x16];
	};
	struct FadeNode
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		int16_t slot;     // +0x0E entity
		uint8_t pad10[8];
		int16_t duration; // +0x18
		uint16_t saved;   // +0x1A entity flags & 0x800 before the fade
		uint8_t pad1C[8];
	};
	struct SparkleNode
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		int16_t pad0E;
		int16_t pos[4];   // +0x10
		uint8_t pad18[0xC];
	};
	struct OrbitCtrlNode
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		int16_t index;    // +0x0E matrix 0 / 1
		int16_t pos[4];   // +0x10
		int16_t pad18;
		int16_t target;   // +0x1A
		int16_t ax;       // +0x1C angle about X
		int16_t dax;      // +0x1E
		int16_t az;       // +0x20 angle about Z
		int16_t daz;      // +0x22
	};
	struct OrbitNode
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		int16_t index;    // +0x0E controller matrix
		uint8_t pad10[6];
		int16_t base;     // +0x16 radius from the target's size
		int16_t angle;    // +0x18
		int16_t phase;    // +0x1A
		int16_t size;     // +0x1C
		int16_t k;        // +0x1E spark number
		int16_t radius;   // +0x20
		int16_t dradius;  // +0x22
	};
	struct EmitterNode // motes
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		uint8_t pad0E[0x16];
	};
	struct RingNode // ring, expanding ring, light pillar, disk, flash, wave, beam
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C
		int16_t delay;    // +0x0E pillar: frames before it appears
		int16_t pos[4];   // +0x10
		int16_t angle;    // +0x18 spin angle (pillar: about X, its yaw is pos[3] = +0x16)
		int16_t spin;     // +0x1A
		int16_t scale;    // +0x1C
		int16_t dscale;   // +0x1E
		int16_t w20;      // +0x20 wave: texture scroll step
		int16_t w22;      // +0x22 wave: texture frame
	};
	// sparkle (pool of 100, 0x18 bytes)
	struct Sparkle
	{
		uint32_t active;  // +0x00
		uint16_t frame;   // +0x04 flipbook frame
		int16_t size;     // +0x06
		int16_t radius;   // +0x08
		int16_t y;        // +0x0A
		int16_t angle;    // +0x0C
		int16_t pad0E;
		int16_t dradius;  // +0x10
		int16_t dy;       // +0x12
		int16_t dangle;   // +0x14
		int16_t pad16;
	};
	// screen mote (pool of 80, 0x18 bytes)
	struct Mote
	{
		uint32_t active;  // +0x00
		uint16_t frame;   // +0x04
		int16_t scale;    // +0x06
		int16_t x, y, z;  // +0x08
		int16_t age;      // +0x0E
		int16_t pad10;
		int16_t speed;    // +0x12
		int16_t pad14;
		int16_t life;     // +0x16
	};
	// grid vertex / cell centre (0x10 bytes)
	struct Vertex
	{
		int16_t sxy[2];   // +0x00 projected screen position
		int16_t x, y, z;  // +0x04 screen-space position (z = 0)
		uint16_t flags;   // +0x0A bit0 = top / bottom row, bit1 = left / right column
		int16_t u, v;     // +0x0C texture coordinates in the capture
	};
	// grid triangle (0x24 bytes)
	struct Triangle
	{
		Vertex *v[3];     // +0x00
		int16_t cx, cy;   // +0x0C centroid
		int16_t zero10;
		int16_t delay;    // +0x12 frames before it moves (-1 = gone)
		int16_t offx, offy; // +0x14 displacement
		int16_t zero18;
		int16_t scale;    // +0x1A 0x1000 .. 0x180 (flat white at 0x180)
		int16_t vx, vy;   // +0x1C
		int16_t ay;       // +0x20 vy += ay / 2
		int16_t dscale;   // +0x22
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(RestoreNode) == 0x10, "Holy root nodes");
	static_assert(sizeof(DirectorNode) == 0x24 && sizeof(ShatterNode) == 0x24 && sizeof(FadeNode) == 0x24 && sizeof(SparkleNode) == 0x24, "Holy nodes");
	static_assert(sizeof(OrbitCtrlNode) == 0x24 && sizeof(OrbitNode) == 0x24 && sizeof(EmitterNode) == 0x24 && sizeof(RingNode) == 0x24, "Holy nodes");
	static_assert(sizeof(Sparkle) == 0x18 && sizeof(Mote) == 0x18 && sizeof(Vertex) == 0x10 && sizeof(Triangle) == 0x24, "Holy pools");

	inline TaskNode *AddTask(uint32_t fn) { return AddTaskToQueue(&Tasks(), fn); }
	inline uint16_t &EntityFlags(int slot) { return *(uint16_t *)Entity(slot); }

	// x / 3, x / 6, x / 20 exactly as the magic-number divisions of the original
	static int32_t Div3(int32_t x) { int32_t hi = (int32_t)(((int64_t)x * 0x55555556) >> 32); return hi + (int32_t)((uint32_t)hi >> 31); }
	static int32_t Div6(int32_t x) { int32_t hi = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32); return hi + (int32_t)((uint32_t)hi >> 31); }
	static int32_t Div20(int32_t x) { int32_t hi = (int32_t)(((int64_t)x * 0x66666667) >> 32) >> 3; return hi + (int32_t)((uint32_t)hi >> 31); }
	static int32_t Div350(int32_t x) { int32_t hi = (int32_t)(((int64_t)x * 0x5D9F7391) >> 32) >> 7; return hi + (int32_t)((uint32_t)hi >> 31); }
	static int32_t Div12(int32_t x) { int32_t hi = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32) >> 1; return hi + (int32_t)((uint32_t)hi >> 31); }
}
}

#ifdef FF8_FX_HELD
#include "mag175_holy_held.h"
#endif

namespace ff8fx
{
namespace holy175
{
	// ------------------------------------------------------------------
	// Root (0x5D44C0) and texture restore (0x5D4540)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(held_note_root();)
		if (r->counter & 1) PacketCursor() = TexBase() + 0xC000;
		else PacketCursor() = TexBase();
		int left = ExecuteTaskQueue(&Tasks());
		if (!left && !(Ctx()->flags & 2))
		{
			RestoreNode *t = (RestoreNode *)AddTaskToQueue(&RootQueues().first, ORIG_RestoreTask);
			t->counter = (int16_t)left;
			t->done = (uint16_t)left;
		}
		r->counter++;
		return left ? 0 : TASK_END;
	}

	static uint32_t __cdecl RestoreTask(TaskNode *n)
	{
		RestoreNode *p = (RestoreNode *)n;
		if (p->counter == 1) TextureRestoreTask((void *)TexBase(), &p->done);
		uint16_t done = p->done;
		p->counter++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Grid construction (director frames 3 and 4)
	// ------------------------------------------------------------------
	// MAG_175_HOLY_BuildGatherLightGrid (0x5D2210): screen grid 15 x 21 vertices (16 units apart,
	// inner ones jittered, texture coordinates = grid position + the jitter), 14 x 20 cell centres
	// (jittered by -3..2), then per cell 4 triangles (top, left, right, bottom around the centre)
	// and their textured packets (two per buffer)
	static uint32_t CellUV(const Vertex *v, uint8_t du, uint16_t tpage)
	{
		uint8_t u = (uint8_t)((uint8_t)v->u + du);
		uint8_t w = (uint8_t)v->v;
		return (uint32_t)u | ((uint32_t)w << 8) | ((uint32_t)tpage << 16);
	}

	static void BuildGrid()
	{
		Vertex *e = (Vertex *)GRID_Vertices;
		int32_t y = -0x78;
		for (int row = 0; row < 15; row++, y += 0x10)
		{
			int32_t x = -0xA8;
			for (int col = 0; col < 21; col++, x += 0x10, e++)
			{
				e->x = (int16_t)x;
				e->y = (int16_t)y;
				e->z = 0;
				e->flags = 0;
				if (row == 0 || row == 14)
				{
					e->v = 0;
					e->flags |= 1;
				}
				else
				{
					int16_t r = (int16_t)(CrtRand() % 12 - 6);
					e->v = r;
					e->y = (int16_t)(e->y + r);
				}
				if (col == 0 || col == 20)
				{
					e->u = 0;
					e->flags |= 2;
				}
				else
				{
					int16_t r = (int16_t)(CrtRand() % 14 - 7);
					e->u = r;
					e->x = (int16_t)(e->x + r);
				}
				e->u = (int16_t)(e->u + (int16_t)(x + 0xA8));
				e->v = (int16_t)(e->v + (int16_t)(y + 0x78));
			}
		}
		Vertex *c = (Vertex *)GRID_Centres;
		int32_t yb = -0x78;
		for (int row = 0; row < 14; row++, yb += 0x10)
		{
			int32_t xb = -0xA8;
			for (int col = 0; col < 20; col++, xb += 0x10, c++)
			{
				c->x = (int16_t)(xb + 8);
				c->y = (int16_t)(yb + 8);
				c->z = 0;
				c->flags = 0;
				int16_t r = (int16_t)(CrtRand() % 6 - 3);
				c->v = r;
				c->y = (int16_t)(c->y + r);
				r = (int16_t)(CrtRand() % 6 - 3);
				c->u = r;
				c->x = (int16_t)(c->x + r);
				c->u = (int16_t)(c->u + (int16_t)(xb + 0xB0));
				c->v = (int16_t)(c->v + (int16_t)(yb + 0x80));
			}
		}
		Vertex *a = (Vertex *)GRID_Vertices;
		Vertex *b = (Vertex *)GRID_Centres;
		uint8_t *pk = (uint8_t *)GRID_Packets;
		Triangle *t = (Triangle *)GRID_Triangles;
		Vertex **cell = (Vertex **)GRID_CellPointers;
		for (int row = 0; row < 14; row++, a++)
		{
			for (int col = 0; col < 20; col++, a++, b++, pk += 0x100, t += 4)
			{
				Vertex *tl = a, *tr = a + 1, *bl = a + 21, *br = a + 22, *cc = b;
				cell[2] = bl;
				cell[0] = tl;
				cell[1] = tr;
				cell[3] = br;
				cell[4] = cc;
				uint16_t tpage;
				uint8_t du;
				if (tl->u > 0xA0)
				{
					tpage = 0x1B | 0x120;
					du = 0x80;
				}
				else
				{
					tpage = 0x19 | 0x120;
					du = 0;
				}
				uint32_t uvTL = CellUV(tl, du, tpage), uvTR = CellUV(tr, du, tpage), uvBL = CellUV(bl, du, tpage);
				uint32_t uvBR = CellUV(br, du, tpage), uvC = CellUV(cc, du, tpage);
				for (int j = 7; j >= 0; j--) *(uint32_t *)(pk + 0x20 * j) = 0x7000000;
				for (int j = 7; j >= 0; j--) *(uint32_t *)(pk + 0x20 * j + 4) = 0x26808080;
				// top: TL TR C / left: TL C BL / right: TR BR C / bottom: C BR BL
				t[0].v[0] = tl; t[0].v[1] = tr; t[0].v[2] = cc;
				t[1].v[0] = tl; t[1].v[1] = cc; t[1].v[2] = bl;
				t[2].v[0] = tr; t[2].v[1] = br; t[2].v[2] = cc;
				t[3].v[0] = cc; t[3].v[1] = br; t[3].v[2] = bl;
				const uint32_t uv[4][3] = { { uvTL, uvTR, uvC }, { uvTL, uvC, uvBL }, { uvTR, uvBR, uvC }, { uvC, uvBR, uvBL } };
				for (int k = 0; k < 4; k++)
					for (int buf = 0; buf < 2; buf++)
						for (int q = 0; q < 3; q++) *(uint32_t *)(pk + 0x40 * k + 0x20 * buf + 0xC + 8 * q) = uv[k][q];
			}
		}
	}

	// MAG_175_HOLY_InitLightPillarTable (0x5D25D0): per triangle its centroid, the delay before it
	// moves (distance of its first vertex's texture position from the corner / 35), velocity,
	// fall and shrink speed
	static void InitTriangles()
	{
		Triangle *t = (Triangle *)GRID_Triangles;
		for (int i = 0; i < GRID_TriangleCount; i++, t++)
		{
			const Vertex *v0 = t->v[0], *v1 = t->v[1], *v2 = t->v[2];
			int32_t cy = Div3(v0->y + v1->y + v2->y);
			int32_t cx = Div3(v0->x + v1->x + v2->x);
			t->cx = (int16_t)cx;
			t->cy = (int16_t)cy;
			t->zero10 = 0;
			const Vertex *w = t->v[0];
			int32_t u = w->u, v = w->v;
			int32_t d = Sqrt(u * u + v * v);
			t->delay = (int16_t)Div350(mul32(d, 10));
			t->zero18 = 0;
			t->offy = 0;
			t->offx = 0;
			t->scale = 0x1000;
			t->vx = (int16_t)(CrtRand() % 6 + 1);
			t->vy = (int16_t)(CrtRand() % 25 + 4);
			t->ay = (int16_t)(-2 - CrtRand() % 8);
			t->dscale = (int16_t)(CrtRand() % 0x160 + 0x140);
		}
	}

	// ------------------------------------------------------------------
	// Fades of the other side's entities
	// ------------------------------------------------------------------
	static void FadeSlots(int32_t &lo, int32_t &hi)
	{
		if (FadeSide() >= 3)
		{
			lo = 0;
			hi = 3;
		}
		else
		{
			lo = 3;
			hi = 7;
		}
	}

	// au_re_BdLinkTask_33 (0x5D2F80): fade out every visible entity of the other side
	static void SpawnFadeOuts(int16_t duration)
	{
		int32_t lo, hi;
		FadeSlots(lo, hi);
		for (int32_t s = lo; s < hi; s++)
		{
			uint16_t f = EntityFlags(s);
			if ((f & 2) && !(f & 4))
			{
				FadeFlag(s) = 1;
				FadeNode *t = (FadeNode *)AddTask(ORIG_FadeOutTask);
				t->counter = 0;
				t->slot = (int16_t)s;
				t->duration = duration;
				t->saved = (uint16_t)(EntityFlags(s) & 0x800);
			}
			else FadeFlag(s) = 0;
		}
	}

	// au_re_BdLinkTask_32 (0x5D2720): fade back in the entities faded out
	static void SpawnFadeIns(int16_t duration)
	{
		int32_t lo, hi;
		FadeSlots(lo, hi);
		for (int32_t s = lo; s < hi; s++)
		{
			if (!FadeFlag(s)) continue;
			FadeNode *t = (FadeNode *)AddTask(ORIG_FadeInTask);
			t->counter = 0;
			t->slot = (int16_t)s;
			t->duration = duration;
			t->saved = (uint16_t)(EntityFlags(s) & 0x800);
		}
	}

	// entity colours (+0x2C, +0x28) = GTE depth cue of black toward the far colour by ir0
	static void FadeColours(uint8_t *e, int32_t ir0)
	{
		uint32_t rgb = 0x32000000;
		GteSetFarColor(var<uint8_t>(0xB8B9A8), var<uint8_t>(0xB8B9A9), var<uint8_t>(0xB8B9AA));
		GteSetRGB(&rgb);
		GteSetIR0(ir0);
		GteDPCS();
		GteReadRGB2(e + 0x2C);
		GteSetFarColor(var<uint8_t>(0xB8B7D8) >> 1, var<uint8_t>(0xB8B7D9) >> 1, var<uint8_t>(0xB8B7DA) >> 1);
		rgb = 0;
		GteSetRGB(&rgb);
		GteDPCS();
		GteReadRGB2(e + 0x28);
	}

	static uint32_t __cdecl FadeInTask(TaskNode *n)
	{
		FadeNode *p = (FadeNode *)n;
		int32_t t = shl32(p->counter, 12) / p->duration;
		uint8_t *e = Entity(p->slot);
		FadeColours(e, t);
		e[0x2B] = 2;
		if (DrawOnly()) return 0;
		if (p->counter == 0) EntityFlags(p->slot) = (uint16_t)((EntityFlags(p->slot) & 0xFFFB) | 0x800);
		p->counter++;
		if (p->counter < p->duration) return 0;
		uint32_t colour = var<uint32_t>(0xB8B7D8);
		e[7] = 0;
		*(uint32_t *)(e + 0x28) = colour;
		EntityFlags(p->slot) = (uint16_t)((EntityFlags(p->slot) & 0xF7FF) | p->saved);
		return TASK_END;
	}

	static uint32_t __cdecl FadeOutTask(TaskNode *n)
	{
		FadeNode *p = (FadeNode *)n;
		int32_t t = shl32(p->counter, 12) / p->duration;
		uint8_t *e = Entity(p->slot);
		e[7] = 0;
		FadeColours(e, 0x1000 - t);
		e[0x2B] = 2;
		if (DrawOnly()) return 0;
		if (p->counter == 0) e[1] |= 8;
		p->counter++;
		if (p->counter < p->duration) return 0;
		EntityFlags(p->slot) = (uint16_t)((EntityFlags(p->slot) & 0xF7FF) | p->saved | 4);
		return TASK_END;
	}

	// ------------------------------------------------------------------
	// Shatter (0x5D2910)
	// ------------------------------------------------------------------
	// MAG_175_sub_5D21F0 / MAG_175_sub_5D29C0: stage groups' flag bit 1 set / cleared
	static void StageGroupsFlag(bool set)
	{
		for (uint32_t a = 0x1D98991; a < 0x1D98A41; a += 0x2C)
		{
			if (set) *(uint8_t *)a |= 2;
			else *(uint8_t *)a &= 0xFD;
		}
	}

	// MAG_175_sub_5D2F50: identity rotation (translation untouched)
	static void IdentityRotation(Mat4x3 *m)
	{
		uint32_t *d = (uint32_t *)m;
		d[3] = 0;
		d[2] = 0;
		d[1] = 0;
		d[0] = 0;
		m->m[2][2] = 0x1000;
		m->m[1][1] = 0x1000;
		m->m[0][0] = 0x1000;
	}

	// MAG_175_sub_5D29E0: the battle stage rendered into VRAM (576, 256, 320 x 232) with the other
	// buffer's draw environment and a 160 / 116 screen offset (bone matrices rebuilt from the pose),
	// then the draw environment of this buffer back
	static void StageCapture()
	{
		uint32_t other = DisplayBuffer() ^ 1u;
		uint8_t *pk = (uint8_t *)FrameArena();
		int16_t rect[4] = { 0x240, 0x100, 0x140, 0xE8 };
		BuildDrawEnvPacket(pk, DRAWENV + 0x5C * other);
		InsertPrimAutoDepth(OTBase() + 0x406C, pk);
		pk += 0x40;
		FrameArena() = (uint32_t)pk;
		int32_t ofy, ofx;
		GetScreenOffset(&ofx, &ofy);
		SetScreenOffset(rect[2] / 2, rect[3] / 2);
		uint8_t *h = (uint8_t *)FieldAlloc(0x54);
		*(uint16_t *)(h + 0x3C) = 0;
		*(uint16_t *)(h + 0x3E) = 0;
		*(uint16_t *)(h + 0x40) = 0x140;
		*(uint16_t *)(h + 0x42) = 0xD8;
		*(uint32_t *)(h + 0x2C) = var<uint32_t>(0x1D98B3C);
		*(uint32_t *)(h + 0x38) = var<uint32_t>(0x1D969A8);
		*(uint32_t *)(h + 0x48) = 0xFFFFFFFF;
		for (int k = 0; k < 4; k++)
		{
			uint8_t *anim = (uint8_t *)(STAGE_AnimHeaders + 0x2C * k);
			const uint32_t *bucket = (const uint32_t *)(STAGE_Buckets + 8 * k);
			BuildBoneMatricesFromPose(anim);
			ComputeBonesWorldMatrices(anim, &Camera());
			if (anim[-0x13] & 1)
			{
				*(uint32_t *)(h + 0x44) = *(const uint32_t *)(anim + 0x14);
				*(uint16_t *)(h + 0x4C) = *(const uint16_t *)(anim - 0x12);
				FrameArena() = RenderGeometry(anim - 0x10, h + 0x28, OTBase() + bucket[0] * 4, bucket[1], FrameArena());
			}
		}
		FieldFree(0x54);
		SetScreenOffset(ofx, ofy);
		uint8_t *env = (uint8_t *)(STAGE_EnvRestore + 0x18 * DisplayBuffer());
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(env = held_env_packet(env);)
		InsertPrimAltViewport(OTBase() + 0x4484, env);
		pk = (uint8_t *)FrameArena();
		BuildDrawAreaPacket(pk, rect);
		InsertPrimAutoDepth(OTBase() + 0x4484, pk);
		pk += 0xC;
		var<int16_t>(0x23941B8) = rect[0];
		var<int16_t>(0x23941BA) = rect[1];
		BuildDrawOffsetPacket(pk, (const void *)0x23941B8);
		InsertPrimAutoDepth(OTBase() + 0x4484, pk);
		pk += 0xC;
		FrameArena() = (uint32_t)pk;
	}

	// grid vertices and cell centres projected 1:1 (identity + (8, 4, H)) to screen positions
	static void ProjectGrid(uint8_t *h)
	{
		Mat4x3 *m = (Mat4x3 *)(h + 0x84);
		IdentityRotation(m);
		m->t[0] = 8;
		m->t[1] = 4;
		m->t[2] = ProjH();
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		for (Vertex *v = (Vertex *)GRID_Vertices; v < (Vertex *)GRID_VerticesEnd; v++)
		{
			GteLoadV0(&v->x);
			GteRTPS();
			GteReadSXY2(v->sxy);
		}
		for (Vertex *v = (Vertex *)GRID_Centres; v < (Vertex *)GRID_CentresEnd; v++)
		{
			GteLoadV0(&v->x);
			GteRTPS();
			GteReadSXY2(v->sxy);
		}
	}

	// a triangle still in the grid: its projected vertices, colour (0x24 = semi-transparent POLY_FT3)
	static void DrawStaticTriangle(const Triangle *t, uint8_t *pkt, uint32_t colour)
	{
		*(uint32_t *)(pkt + 4) = colour | 0x24000000;
		*(uint32_t *)(pkt + 8) = *(const uint32_t *)t->v[0]->sxy;
		*(uint32_t *)(pkt + 0x10) = *(const uint32_t *)t->v[1]->sxy;
		*(uint32_t *)(pkt + 0x18) = *(const uint32_t *)t->v[2]->sxy;
		InsertPrimAutoDepth(OTBase() + 0x4068, pkt);
	}

	static void UpdateTriangle(Triangle *t)
	{
		int16_t ox = t->offx;
		t->offx = (int16_t)(t->vx + ox);
		int16_t vx = t->vx;
		t->vx = (int16_t)(vx + (int16_t)(vx >> 2));
		t->offy = (int16_t)(t->offy + t->vy);
		t->vy = (int16_t)(t->vy + (int16_t)(t->ay >> 1));
		t->scale = (int16_t)(t->scale - t->dscale);
		if (t->scale < 0x180) t->scale = 0x180;
	}

	// the triangle's vertices around its centroid, scaled by its scale in X / Y, through the GTE
	// (translation h+0x78: centroid + displacement + (8, 4, H), set by the caller)
	static void ProjectTriangle(uint8_t *h, const Triangle *t)
	{
		const uint8_t *v0 = (const uint8_t *)t->v[0];
		*(uint32_t *)(h + 0x44) = *(const uint32_t *)(v0 + 4);
		*(uint32_t *)(h + 0x48) = *(const uint32_t *)(v0 + 8);
		*(int16_t *)(h + 0x46) = (int16_t)(*(int16_t *)(h + 0x46) - t->cy);
		*(int16_t *)(h + 0x44) = (int16_t)(*(int16_t *)(h + 0x44) - t->cx);
		const uint8_t *v1 = (const uint8_t *)t->v[1];
		*(uint32_t *)(h + 0x4C) = *(const uint32_t *)(v1 + 4);
		*(uint32_t *)(h + 0x50) = *(const uint32_t *)(v1 + 8);
		*(int16_t *)(h + 0x4C) = (int16_t)(*(int16_t *)(h + 0x4C) - t->cx);
		*(int16_t *)(h + 0x4E) = (int16_t)(*(int16_t *)(h + 0x4E) - t->cy);
		const uint8_t *v2 = (const uint8_t *)t->v[2];
		*(uint32_t *)(h + 0x54) = *(const uint32_t *)(v2 + 4);
		*(uint32_t *)(h + 0x58) = *(const uint32_t *)(v2 + 8);
		*(int16_t *)(h + 0x54) = (int16_t)(*(int16_t *)(h + 0x54) - t->cx);
		*(int16_t *)(h + 0x56) = (int16_t)(*(int16_t *)(h + 0x56) - t->cy);
		Mat4x3 *m = (Mat4x3 *)(h + 0x64);
		IdentityRotation(m);
		m->m[1][1] = t->scale;
		m->m[0][0] = t->scale;
		GteSetRotMatrixCtrl(m);
		GteSetTransVectorCtrl(m);
		GteLoadV012(h + 0x44, h + 0x4C, h + 0x54);
		GteRTPT();
	}

	// flat white triangle (semi-transparent POLY_F3) at the projected vertices
	static void FlatTriangle(uint8_t *pkt)
	{
		*(uint32_t *)pkt = 0x4000000;
		*(uint32_t *)(pkt + 4) = 0x20FFFFFF;
		GteReadSXY012(pkt + 8, pkt + 0xC, pkt + 0x10);
		InsertPrimAutoDepth(OTBase() + 0x4064, pkt);
	}

	// a moving triangle in its packet: textured, flat white once shrunk to 0x180
	static void EmitMovingTriangle(const Triangle *t, uint8_t *pkt, uint32_t colour)
	{
		if (t->scale == 0x180)
		{
			FlatTriangle(pkt);
			return;
		}
		*(uint32_t *)(pkt + 4) = colour | 0x26000000;
		GteReadSXY012(pkt + 8, pkt + 0x10, pkt + 0x18);
		InsertPrimAutoDepth(OTBase() + 0x4064, pkt);
	}

	// MAG_175_sub_5D2BC0: the grid at shatter frame `counter`, buffer `buf`: triangles whose delay
	// has not passed are drawn in place; the others move (displacement / velocity / shrink, not in
	// the draw-only mode) and are drawn around their centroid; a triangle leaving the screen
	// (x > 168 or y < -116) is gone. No triangle drawn: the grid is finished.
	static void GridDraw(int32_t counter, int32_t buf, uint32_t colour)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		ProjectGrid(h);
		*(int32_t *)(h + 0x30) = ProjH();
		int32_t drawn = 0;
		uint8_t *pk = (uint8_t *)GRID_Packets;
		Triangle *t = (Triangle *)GRID_Triangles;
		for (int i = 0; i < GRID_TriangleCount; i++, t++, pk += 0x40)
		{
			if (t->delay < 0) continue;
			if (t->delay >= counter)
			{
				DrawStaticTriangle(t, pk + buf * 0x20, colour);
				drawn++;
				continue;
			}
			int32_t x = t->offx + t->cx;
			int32_t y = t->offy + t->cy;
			*(int32_t *)(h + 0x78) = x;
			*(int32_t *)(h + 0x7C) = y;
			if (x > 0xA8 || y < -0x74)
			{
				t->delay = -1;
				continue;
			}
			*(int32_t *)(h + 0x78) = x + 8;
			*(int32_t *)(h + 0x80) = *(int32_t *)(h + 0x30);
			*(int32_t *)(h + 0x7C) = y + 4;
			if (!DrawOnly()) UpdateTriangle(t);
			ProjectTriangle(h, t);
			if (GridTextured()) EmitMovingTriangle(t, pk + buf * 0x20, colour);
			else if (t->scale == 0x180)
			{
				FlatTriangle((uint8_t *)PacketCursor());
				PacketCursor() += 0x20;
			}
			drawn++;
		}
		if (!drawn) GridActive() = 0;
		FieldFree(0xB4);
	}

	// grid brightness at shatter frame c: 0x80 + 12 c, at most 0xFF
	static uint32_t GridColour(int32_t c)
	{
		int32_t level = c * 12 + 0x80;
		if (level > 0xFF) level = 0xFF;
		return (uint32_t)level | ((uint32_t)level << 8) | ((uint32_t)level << 16);
	}

	static uint32_t __cdecl ShatterTask(TaskNode *n)
	{
		ShatterNode *p = (ShatterNode *)n;
		if (p->counter < 0x32 && GridActive())
		{
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(held_note_shatter(p);)
			if (p->counter < 10) StageCapture();
			int32_t c = p->counter;
			GridDraw(c, DisplayBuffer(), GridColour(c));
		}
		if (DrawOnly()) return 0;
		if (p->counter == 0) StageGroupsFlag(false);
		if (p->counter == 6) SpawnFadeOuts(6);
		p->counter++;
		return p->counter > 0x32 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Sparkles (0x5D3180): a sprite at (anchor + radius * (cos, sin)(angle), y)
	// ------------------------------------------------------------------
	static void SparkleDraw(uint8_t *h, int16_t *s, const Sparkle *e)
	{
		int32_t a = e->angle;
		*(uint16_t *)(h + 4) = e->frame;
		int32_t r = e->radius;
		s[4] = (int16_t)((int16_t)(mul32(ComputeCos(a), r) >> 12) + s[0]);
		s[5] = e->y;
		s[6] = (int16_t)((int16_t)(mul32(ComputeSin(a), r) >> 12) + s[2]);
		ShadowRotation(s + 4, e->size, -(e->size >> 4));
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
	}

	static uint32_t __cdecl SparkleTask(TaskNode *n)
	{
		SparkleNode *p = (SparkleNode *)n;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int16_t *s = (int16_t *)FieldAlloc(0x10);
		*(uint32_t *)s = *(const uint32_t *)&p->pos[0];
		*(uint32_t *)h = SEQ_Sparkle;
		*(uint16_t *)(h + 0x24) = 0;
		*(uint32_t *)(s + 2) = *(const uint32_t *)&p->pos[2];
		int32_t alive = 0;
		for (Sparkle *e = (Sparkle *)SPARKLES; e < (Sparkle *)SPARKLES_End; e++)
		{
			if (!(e->active & 1)) continue;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(held_note_item(ORIG_SparkleTask, e, h);)
			SparkleDraw(h, s, e);
			if (DrawOnly()) continue;
			e->frame++;
			if (*(int16_t *)(h + 0x28) < 0) e->active = 0;
			else
			{
				e->radius = (int16_t)(e->radius - e->dradius);
				e->y = (int16_t)(e->y + e->dy);
				e->angle = (int16_t)(e->angle + e->dangle);
				alive++;
			}
		}
		FieldFree(0x10);
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (p->counter >= 0 && p->counter <= 0x42)
		{
			for (int j = 0; j < 2; j++)
			{
				Sparkle *e = (Sparkle *)SPARKLES;
				int idx = 0;
				while (e->active)
				{
					e++;
					idx++;
					if (e >= (Sparkle *)SPARKLES_End) goto spawned;
				}
				if (idx >= 100) goto spawned;
				e->active = 1;
				e->frame = 0;
				e->size = (int16_t)(CrtRand() % 0x300 + 0x180);
				e->radius = (int16_t)(CrtRand() % 0x1F40 + 0x258);
				e->y = (int16_t)(0xC8 - CrtRand() % 0xBB8);
				e->angle = (int16_t)(CrtRand() % 0x1000);
				int32_t d = CrtRand() % 14 + 0x1F;
				e->dradius = (int16_t)(e->radius / d);
				e->dy = (int16_t)((p->pos[1] - e->y) / d);
				e->dangle = (int16_t)(-10 - CrtRand() % 30);
			}
		}
	spawned:
		p->counter++;
		if (p->counter >= 0x1F && !alive) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Orbit controller (0x5D3420) and orbiting sparks (0x5D3590)
	// ------------------------------------------------------------------
	static void OrbitCtrlMatrix(const OrbitCtrlNode *p, Mat4x3 *m)
	{
		int16_t ang[3] = { p->ax, 0, p->az };
		BuildRotationMatrixFromAngles(ang, m);
		m->t[0] = p->pos[0];
		m->t[1] = p->pos[1];
		m->t[2] = p->pos[2];
	}

	static void OrbitCtrlUpdate(OrbitCtrlNode *p)
	{
		if (p->counter >= 6 && p->counter - 6 < 0x34)
		{
			p->ax = (int16_t)(p->ax + p->dax);
			p->az = (int16_t)(p->az + p->daz);
		}
	}

	static uint32_t __cdecl OrbitCtrlTask(TaskNode *n)
	{
		OrbitCtrlNode *p = (OrbitCtrlNode *)n;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(held_note_draw(ORIG_OrbitCtrlTask, p, sizeof(*p));)
		OrbitCtrlMatrix(p, OrbitMatrix(p->index));
		if (DrawOnly()) return 0;
		if (p->counter == 0)
		{
			int32_t r = CrtRand() % 0x1000;
			for (int k = 0; k < 8; k++)
			{
				OrbitNode *o = (OrbitNode *)AddTask(ORIG_OrbitTask);
				o->index = p->index;
				o->k = (int16_t)k;
				o->counter = 0;
				o->phase = 0x800;
				o->size = 0xA00;
				o->radius = 0xFA0;
				o->dradius = 0x258;
				int32_t v = (int16_t)((EntitySize(p->target) * 2200) >> 12);
				o->base = (int16_t)((v * 3) / 4);
				o->angle = (int16_t)((k * 0x200 + r) & 0xFFF);
			}
		}
		OrbitCtrlUpdate(p);
		p->counter++;
		return p->counter >= 0x4B ? TASK_END : 0;
	}

	static void OrbitDraw(const OrbitNode *p, const Mat4x3 *m)
	{
		int32_t r = p->radius + p->base;
		int16_t v[4];
		v[0] = (int16_t)(mul32(ComputeCos(p->angle), r) >> 12);
		v[1] = 0;
		v[2] = (int16_t)(mul32(ComputeSin(p->angle), r) >> 12);
		v[3] = 0;
		MatrixMultiplyVector(m, v, v);
		v[0] = (int16_t)(v[0] + (int16_t)m->t[0]);
		v[2] = (int16_t)(v[2] + (int16_t)m->t[2]);
		v[1] = (int16_t)(v[1] + (int16_t)m->t[1]);
		ShadowRotation(v, p->size, -(p->size >> 4));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		*(uint16_t *)(h + 4) = (uint16_t)(*(const uint8_t *)&p->counter & 7);
		*(uint32_t *)h = SEQ_OrbitSpark;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// frames 0..5 radius shrinks (step - step / 20); 6..57 bobbing radius (sin of the phase, 400),
	// angle + 0x3C; 58..73 radius swings out (sin, 3000) then in, angle + 0x1E; 74: radius -base
	static void OrbitUpdate(OrbitNode *p)
	{
		int32_t c = p->counter;
		if (c < 6)
		{
			int16_t d = p->dradius;
			p->radius = (int16_t)(p->radius - d);
			p->dradius = (int16_t)(d - Div20(d));
		}
		else if (c - 6 < 0x34)
		{
			if (c == 6) p->dradius = p->radius;
			p->phase = (int16_t)(p->phase + 0x78);
			int32_t s = ComputeSin(p->phase);
			p->angle = (int16_t)(p->angle + 0x3C);
			p->radius = (int16_t)((int16_t)((s * 400) >> 12) + p->dradius);
		}
		else if (c - 0x3A < 0x10)
		{
			int32_t d = c - 0x3A;
			if (d == 0)
			{
				p->dradius = p->radius;
				p->phase = (int16_t)((p->radius + p->base) / 8);
			}
			int32_t a = (d << 11) / 16;
			if (d >= 8) p->dradius = (int16_t)(p->dradius - p->phase);
			int32_t s = ComputeSin(a);
			p->radius = (int16_t)((int16_t)((s * 3000) >> 12) + p->dradius);
			p->angle = (int16_t)(p->angle + 0x1E);
		}
		else if (c - 0x4A < 1) p->radius = (int16_t)-p->base;
	}

	static uint32_t __cdecl OrbitTask(TaskNode *n)
	{
		OrbitNode *p = (OrbitNode *)n;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(held_note_draw(ORIG_OrbitTask, p, sizeof(*p));)
		OrbitDraw(p, OrbitMatrix(p->index));
		if (DrawOnly()) return 0;
		OrbitUpdate(p);
		p->counter++;
		return p->counter >= 0x4B ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Screen motes (0x5D37B0): sprites in a fixed screen-space frame (rotation 0x1C2 about Z,
	// depth H), brightness 21 x age
	// ------------------------------------------------------------------
	static void MoteDraw(uint8_t *h, uint8_t *m, const Mote *e)
	{
		int32_t level = e->age * 21;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(level = held_ramp(level, 21);)
		if (level > 0x80) level = 0x80;
		uint32_t colour = (((uint32_t)level << 8 | (uint32_t)level) << 8) | (uint32_t)level;
		*(int32_t *)(m + 0x34) = 0;
		*(int32_t *)(m + 0x30) = 0;
		*(int32_t *)(m + 0x2C) = 0;
		*(int32_t *)(m + 0x3C) = e->x;
		*(uint32_t *)(h + 0x1C) = colour;
		*(uint16_t *)(h + 4) = e->frame;
		*(int32_t *)(m + 0x28) = 0;
		*(int16_t *)(m + 0x38) = e->scale;
		*(int16_t *)(m + 0x30) = e->scale;
		*(int16_t *)(m + 0x28) = e->scale;
		*(int32_t *)(m + 0x40) = e->y;
		*(int32_t *)(m + 0x44) = e->z;
		Mat4x3 *w = (Mat4x3 *)(m + 0x28);
		ComposeAffineTransform((const Mat4x3 *)(m + 8), w, w);
		GteSetRotMatrix(w);
		GteSetTransVector(w);
		FrameArena() = InitEffectSequenceFromData(h, OTBase() + 0x4064, 0xE, FrameArena());
	}

	static uint32_t __cdecl MoteTask(TaskNode *n)
	{
		EmitterNode *p = (EmitterNode *)n;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		uint8_t *m = (uint8_t *)FieldAlloc(0x48);
		int32_t alive = 0;
		*(uint32_t *)h = SEQ_Mote;
		*(uint16_t *)(h + 0x24) = 0xC;
		int16_t *ang = (int16_t *)m;
		ang[0] = 0;
		ang[1] = 0;
		ang[2] = 0x1C2;
		BuildRotationMatrixFromAngles(ang, (Mat4x3 *)(m + 8));
		*(int32_t *)(m + 0x1C) = 0;
		*(int32_t *)(m + 0x20) = 0;
		*(int32_t *)(m + 0x24) = ProjH();
		for (Mote *e = (Mote *)MOTES; e < (Mote *)MOTES_End; e++)
		{
			if (!(e->active & 1)) continue;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(held_note_item(ORIG_MoteTask, e, h);)
			MoteDraw(h, m, e);
			if (DrawOnly()) continue;
			e->age++;
			if (e->age < e->life)
			{
				e->y = (int16_t)(e->y + e->speed);
				e->speed = (int16_t)(e->speed + (int16_t)(e->speed >> 3));
				alive++;
			}
			else e->active = 0;
		}
		FieldFree(0x48);
		FieldFree(0xB4);
		if (DrawOnly()) return 0;
		if (p->counter >= 0 && p->counter <= 0x3A)
		{
			for (int j = 0; j < 2; j++)
			{
				Mote *e = (Mote *)MOTES;
				int idx = 0;
				while (e->active)
				{
					e++;
					idx++;
					if (e >= (Mote *)MOTES_End) goto spawned;
				}
				if (idx >= 0x50) goto spawned;
				e->active = 1;
				e->frame = (uint16_t)(CrtRand() & 3);
				e->scale = (int16_t)(CrtRand() % 0x90 + 0x80);
				e->x = (int16_t)(CrtRand() % 0x154 - 0xAA);
				e->y = (int16_t)(CrtRand() % 0x15E - 0x32);
				e->z = 0;
				e->age = 0;
				e->speed = (int16_t)(-1 - CrtRand() % 10);
				e->life = (int16_t)(CrtRand() % 10 + 10);
			}
		}
	spawned:
		p->counter++;
		if (p->counter >= 4 && !alive) return TASK_END;
		return 0;
	}

	// ------------------------------------------------------------------
	// Prim models: GTE = camera * (translate pos, rotate, scale)
	// ------------------------------------------------------------------
	static void ModelGte(const int16_t ang[3], const int16_t *pos, int32_t scale)
	{
		Mat4x3 m = {};
		ComposeZYXRotationMatrix(ang, &m);
		m.t[0] = pos[0];
		m.t[1] = pos[1];
		m.t[2] = pos[2];
		int32_t s[3] = { scale, scale, scale };
		Scale3DMatrix(&m, s);
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
	}

	// prim-model header (0x58 bytes of scratch): model, +8 = 0, +0x1C = 0x33 (no fade)
	static uint8_t *ModelHeader(uint32_t model)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)h = model;
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x1C) = 0x33;
		return h;
	}

	// fade toward black (header +0x0C, flags 0xF3)
	static void ModelFade(uint8_t *h, int32_t fade)
	{
		*(uint32_t *)(h + 0x1C) = 0xF3;
		*(int32_t *)(h + 0xC) = fade;
	}

	// Ring (0x5D3A50): spinning about Y, fades in over frames 0..7
	static void RingDraw(const RingNode *p)
	{
		int16_t ang[3] = { 0, p->angle, 0 };
		ModelGte(ang, p->pos, p->scale);
		uint8_t *h = ModelHeader(MODEL_Ring);
		if (p->counter < 8)
		{
			int32_t fade = (8 - p->counter) << 9;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(fade = held_ramp(fade, -0x200);)
			ModelFade(h, fade);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *p = (RingNode *)n;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(held_note_draw(ORIG_RingTask, p, sizeof(*p));)
		RingDraw(p);
		if (DrawOnly()) return 0;
		p->angle = (int16_t)(p->angle + p->spin);
		p->counter++;
		return p->counter >= 0x3A ? TASK_END : 0;
	}

	// Wave (0x5D3B80, MAG_066_sub_650720 header of 0x90 bytes): texture of the frame (+0x22 & 15),
	// scroll step * frame, fades out over frames 0..0x37
	static void WaveDraw(const RingNode *p)
	{
		int16_t ang[3] = { 0, 0, 0 };
		ModelGte(ang, p->pos, p->scale);
		uint8_t *h = (uint8_t *)FieldAlloc(0x90);
		*(uint32_t *)(h + 8) = 0;
		*(uint32_t *)(h + 0x10) = 0;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint32_t *)(h + 0x24) = *(const uint32_t *)(WAVE_Textures + 4 * (*(const uint8_t *)&p->w22 & 0xF));
		int32_t c = p->counter;
		int32_t scroll = p->w20 * c;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(scroll = held_ramp(scroll, p->w20);)
		*(uint32_t *)h = MODEL_Wave;
		*(uint32_t *)(h + 0x20) = 0x33;
		*(int32_t *)(h + 0x14) = scroll;
		*(uint16_t *)(h + 0x1A) = 0x80;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint16_t *)(h + 0x1E) = 0x80;
		if (c < 0x38)
		{
			int32_t fade = 0x1000 - c * 73;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(fade = held_ramp(fade, -73);)
			*(uint32_t *)(h + 0x20) = 0xF3;
			*(int32_t *)(h + 0xC) = fade;
		}
		FrameArena() = RenderWaveModel(h, RenderOT(), 2, FrameArena());
		FieldFree(0x90);
	}

	static uint32_t __cdecl WaveTask(TaskNode *n)
	{
		RingNode *p = (RingNode *)n;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(held_note_draw(ORIG_WaveTask, p, sizeof(*p));)
		WaveDraw(p);
		if (DrawOnly()) return 0;
		p->w22++;
		p->counter++;
		return p->counter >= 0x3A ? TASK_END : 0;
	}

	// Expanding ring (0x5D3CE0): grows by a decelerating step, fades from frame 8
	static void ExpandDraw(const RingNode *p)
	{
		int16_t ang[3] = { 0, 0, 0 };
		ModelGte(ang, p->pos, p->scale);
		uint8_t *h = ModelHeader(MODEL_Expand);
		if (p->counter >= 8)
		{
			int32_t fade = (p->counter - 8) << 9;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(fade = held_ramp(fade, 0x200);)
			ModelFade(h, fade);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void ExpandUpdate(RingNode *p)
	{
		int16_t d = p->dscale;
		p->scale = (int16_t)(p->scale + d);
		p->dscale = (int16_t)(d - d / 32);
	}

	static uint32_t __cdecl ExpandTask(TaskNode *n)
	{
		RingNode *p = (RingNode *)n;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(held_note_draw(ORIG_ExpandTask, p, sizeof(*p));)
		ExpandDraw(p);
		if (DrawOnly()) return 0;
		ExpandUpdate(p);
		p->counter++;
		return p->counter >= 0x10 ? TASK_END : 0;
	}

	// Light pillar (0x5D3E20): rotation (spin +0x18 about X, yaw +0x16 about Y); fades in over
	// frames 0..7, out from frame 0x10; spins from frame 0xC, the spin speed growing by 1/16
	static void PillarDraw(const RingNode *p)
	{
		int16_t ang[3] = { p->angle, p->pos[3], 0 };
		ModelGte(ang, p->pos, p->scale);
		uint8_t *h = ModelHeader(MODEL_Pillar);
		int32_t c = p->counter;
		if (c < 8)
		{
			int32_t fade = (8 - c) << 9;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(fade = held_ramp(fade, -0x200);)
			ModelFade(h, fade);
		}
		else if (c >= 0x10)
		{
			int32_t fade = (c - 0x10) * 409;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(fade = held_ramp(fade, 409);)
			ModelFade(h, fade);
		}
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	static void PillarUpdate(RingNode *p)
	{
		if (p->counter < 0xC) return;
		int16_t v = p->spin;
		p->angle = (int16_t)(p->angle + v);
		p->spin = (int16_t)(v + (int16_t)(v >> 4));
	}

	static uint32_t __cdecl PillarTask(TaskNode *n)
	{
		RingNode *p = (RingNode *)n;
		if (p->delay > 0)
		{
			if (DrawOnly()) return 0;
			p->delay--;
			return 0;
		}
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(held_note_draw(ORIG_PillarTask, p, sizeof(*p));)
		PillarDraw(p);
		if (DrawOnly()) return 0;
		PillarUpdate(p);
		p->counter++;
		return p->counter >= 0x1A ? TASK_END : 0;
	}

	// MAG_175_sub_5D41F0: sprite matrix 0x23941E8 = scale `size`, translation = the screen position
	// of pos / 8 - (160, 108), depth H + depth (the GTE gets it)
	static Mat4x3 *ScreenSpriteMatrix(const int16_t *pos, int16_t size, int32_t depth)
	{
		Mat4x3 *m = SpriteMatrix();
		m->m[2][2] = size;
		m->m[1][1] = size;
		m->m[0][0] = size;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(pos);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy);
		int16_t sx = (int16_t)(sxy[0] / 8);
		int16_t sy = (int16_t)(sxy[1] / 8);
		m->t[0] = sx - 0xA0;
		m->t[1] = sy - 0x6C;
		m->t[2] = ProjH() + depth;
		GteSetRotMatrix(m);
		GteSetTransVector(m);
		return m;
	}

	// Beam (0x5D3FB0): 8 sprites (table 0xD591D0) along a direction that sweeps with the frame,
	// turned toward the camera eye; brightness ramps in over frames 0..1, out over 0x10..0x17
	static void BeamDraw(const RingNode *p)
	{
		int32_t a[3] = { 0, 0, -0x1000 };
		int32_t b[3];
		b[0] = var<int16_t>(0xB8B7F0) - p->pos[0];
		b[1] = var<int16_t>(0xB8B7F2) - p->pos[1];
		b[2] = var<int16_t>(0xB8B7F4) - p->pos[2];
		NormalizeVector(b, b);
		int32_t axis[3];
		int32_t angle = RotationBetweenVectors(a, b, axis);
		Mat4x3 r = {};
		BuildAxisAngleRotationMatrix(angle, &r, axis);
		r.t[0] = p->pos[0];
		r.t[1] = p->pos[1];
		r.t[2] = p->pos[2];
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t c = p->counter;
		*(uint32_t *)h = SEQ_Beam;
		*(uint16_t *)(h + 0x24) = 0;
		bool lit = false;
		int32_t level = 0;
		if (c < 2)
		{
			level = c << 6;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(level = held_ramp(level, 0x40);)
			lit = true;
		}
		else if (c >= 0x10)
		{
			level = (0x18 - c) << 4;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(level = held_ramp(level, -0x10);)
			lit = true;
		}
		if (lit)
		{
			*(uint16_t *)(h + 0x24) = 4;
			*(uint32_t *)(h + 0x1C) = (((uint32_t)level << 8) | (uint32_t)level) << 8;
		}
		int32_t d[3] = { -0x200 - c * 300, -0x100 - c * 30, c * 25 - 0xE00 };
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(d[0] = held_ramp(d[0], -300); d[1] = held_ramp(d[1], -30); d[2] = held_ramp(d[2], 25);)
		NormalizeVector(d, d);
		for (uint32_t k = 0; k < 8; k++)
		{
			const int16_t *e = (const int16_t *)(BEAM_Sprites + 8 * k);
			int32_t dist = (e[2] * 2000) >> 12;
			int16_t v[4];
			v[0] = (int16_t)(mul32(dist, d[0]) >> 12);
			v[1] = (int16_t)(mul32(dist, d[1]) >> 12);
			v[2] = (int16_t)(mul32(dist, d[2]) >> 12);
			v[3] = 0;
			MatrixMultiplyVector(&r, v, v);
			v[0] = (int16_t)(v[0] + (int16_t)r.t[0]);
			v[1] = (int16_t)(v[1] + (int16_t)r.t[1]);
			v[2] = (int16_t)(v[2] + (int16_t)r.t[2]);
			ScreenSpriteMatrix(v, e[1], 0);
			*(uint16_t *)(h + 4) = (uint16_t)e[0];
			PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		}
		FieldFree(0xB4);
	}

	static uint32_t __cdecl BeamTask(TaskNode *n)
	{
		RingNode *p = (RingNode *)n;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(held_note_draw(ORIG_BeamTask, p, sizeof(*p));)
		BeamDraw(p);
		if (DrawOnly()) return 0;
		p->counter++;
		return p->counter >= 0x18 ? TASK_END : 0;
	}

	// Disk (0x5D42B0): shadow-rotation frame of size `scale`, spinning about Z; the size grows by
	// an accelerating step (overflow -> 0x7FFF)
	static void DiskDraw(const RingNode *p)
	{
		Mat4x3 *s = ShadowRotation(p->pos, p->scale, 0);
		int16_t ang[3] = { 0, 0, p->angle };
		Mat4x3 m = {};
		ComposeZYXRotationMatrix(ang, &m);
		MatrixMultiply(s, &m);
		GteSetRotMatrix(&m);
		uint8_t *h = ModelHeader(MODEL_Disk);
		PacketCursor() = RenderPrimModel(h, RenderOT(), 0xD, PacketCursor());
		FieldFree(0x58);
	}

	static void DiskUpdate(RingNode *p)
	{
		int16_t spin = p->spin, d = p->dscale;
		p->angle = (int16_t)(p->angle + spin);
		p->scale = (int16_t)(p->scale + d);
		if (p->scale < 0) p->scale = 0x7FFF;
		p->dscale = (int16_t)(d / 8 + d);
	}

	static uint32_t __cdecl DiskTask(TaskNode *n)
	{
		RingNode *p = (RingNode *)n;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(held_note_draw(ORIG_DiskTask, p, sizeof(*p));)
		DiskDraw(p);
		if (DrawOnly()) return 0;
		DiskUpdate(p);
		p->counter++;
		return p->counter >= 0x18 ? TASK_END : 0;
	}

	// Flash (0x5D43B0): screen-space prim model (sprite matrix, X scale = size, Y 1), fades in over
	// frames 0..3, out from 0x12; shrinks from frame 8
	static void FlashDraw(const RingNode *p)
	{
		Mat4x3 *m = ScreenSpriteMatrix(p->pos, p->scale, 0);
		m->m[0][0] = p->scale;
		m->m[1][1] = 0x1000;
		GteSetRotMatrix(m);
		uint8_t *h = ModelHeader(MODEL_Flash);
		int32_t c = p->counter;
		if (c < 4)
		{
			int32_t fade = (4 - c) << 10;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(fade = held_ramp(fade, -0x400);)
			ModelFade(h, fade);
		}
		else if (c >= 0x12)
		{
			int32_t fade = (c - 0x12) * 682;
			// 30 fps layer: see mag175_holy_held.inc
			FX_HELD(fade = held_ramp(fade, 682);)
			ModelFade(h, fade);
		}
		FrameArena() = RenderPrimModel(h, RenderOT(), 0xE, FrameArena());
		FieldFree(0x58);
	}

	static void FlashUpdate(RingNode *p)
	{
		if (p->counter < 8) return;
		int16_t d = p->dscale;
		p->scale = (int16_t)(p->scale - d);
		p->dscale = (int16_t)(d - Div6(d));
	}

	static uint32_t __cdecl FlashTask(TaskNode *n)
	{
		RingNode *p = (RingNode *)n;
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(held_note_draw(ORIG_FlashTask, p, sizeof(*p));)
		FlashDraw(p);
		if (DrawOnly()) return 0;
		FlashUpdate(p);
		p->counter++;
		return p->counter > 0x18 ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Director (0x5D1BD0)
	// ------------------------------------------------------------------
	static void CopyAnchor(int16_t *dst, const DirectorNode *p)
	{
		*(uint32_t *)&dst[0] = *(const uint32_t *)&p->pos[0];
		*(uint32_t *)&dst[2] = *(const uint32_t *)&p->pos[2];
	}

	static uint32_t __cdecl DirectorTask(TaskNode *n)
	{
		if (DrawOnly()) return 0;
		DirectorNode *p = (DirectorNode *)n;
		if (p->counter == 0)
		{
			GetDefaultEffectPosition(Entity(p->target), p->pos);
			GridTextured() = 1;
			GridActive() = 1;
		}
		if (p->counter == 3) BuildGrid();
		if (p->counter == 4) InitTriangles();
		if (p->counter == 5)
		{
			ShatterNode *t = (ShatterNode *)AddTask(ORIG_ShatterTask);
			t->counter = 0;
			FadeSide() = p->target;
		}
		if (p->counter == 1)
		{
			SparkleNode *t = (SparkleNode *)AddTask(ORIG_SparkleTask);
			CopyAnchor(t->pos, p);
			t->counter = 0;
		}
		if (p->counter == 6)
		{
			OrbitCtrlNode *a = (OrbitCtrlNode *)AddTask(ORIG_OrbitCtrlTask);
			CopyAnchor(a->pos, p);
			a->counter = 0;
			a->index = 0;
			a->target = p->target;
			a->ax = 0;
			a->az = 0;
			a->dax = (int16_t)(CrtRand() % 50 + 0x3C);
			a->daz = (int16_t)(CrtRand() % 40 + 0x1E);
			OrbitCtrlNode *b = (OrbitCtrlNode *)AddTask(ORIG_OrbitCtrlTask);
			CopyAnchor(b->pos, p);
			b->target = p->target;
			b->counter = 0;
			b->index = 1;
			b->ax = 0;
			b->dax = (int16_t)-a->dax;
			b->az = 0;
			b->daz = (int16_t)-a->daz;
		}
		if (p->counter == 0x10)
		{
			EmitterNode *t = (EmitterNode *)AddTask(ORIG_MoteTask);
			t->counter = 0;
		}
		if (p->counter == 0x1E)
		{
			GridTextured() = 0;
			QueueTimUpload(TIM_Upload);
		}
		if (p->counter == 0x1F)
		{
			RingNode *r = (RingNode *)AddTask(ORIG_RingTask);
			CopyAnchor(r->pos, p);
			r->pos[1] = (int16_t)(r->pos[1] + 0x1F4);
			r->counter = 0;
			r->scale = 0x1000;
			r->angle = (int16_t)(CrtRand() % 0x1000);
			r->spin = (int16_t)(CrtRand() % 20 + 0x1E);
			RingNode *w = (RingNode *)AddTask(ORIG_WaveTask);
			CopyAnchor(w->pos, p);
			w->pos[1] = (int16_t)(w->pos[1] + 0x258);
			w->counter = 0;
			w->scale = 0x1000;
			w->w20 = 0xC;
			w->w22 = 0;
			w = (RingNode *)AddTask(ORIG_WaveTask);
			CopyAnchor(w->pos, p);
			w->pos[1] = (int16_t)(w->pos[1] + 0x258);
			w->counter = 0;
			w->scale = 0xF00;
			w->w20 = 4;
			w->w22 = 8;
		}
		if (p->counter >= 0x1F && p->counter <= 0x49 && p->counter % 6 == 1)
		{
			RingNode *e = (RingNode *)AddTask(ORIG_ExpandTask);
			CopyAnchor(e->pos, p);
			e->pos[1] = (int16_t)(e->pos[1] + 0x5DC);
			e->counter = 0;
			e->dscale = 0x2AA;
			e->scale = 0x2AA;
		}
		if (p->counter == 0x2D)
		{
			// 12 pillars on a circle of 2100 around the anchor, 1400 lower, turned toward the centre
			int32_t r = CrtRand() % 0x1000;
			int32_t acc = 0;
			for (int k = 0; k < 12; k++, acc += 0x1000)
			{
				RingNode *q = (RingNode *)AddTask(ORIG_PillarTask);
				q->delay = (int16_t)(*(const int16_t *)(PILLAR_Delays + 4 * k) * 3);
				q->counter = 0;
				int32_t ang = (Div12(acc) + r) & 0xFFF;
				int32_t cs = ComputeCos(ang);
				q->pos[1] = (int16_t)(p->pos[1] + 0x578);
				q->pos[0] = (int16_t)((int16_t)((cs * 2100) >> 12) + p->pos[0]);
				int32_t sn = ComputeSin(ang);
				q->angle = 0;
				q->pos[3] = (int16_t)(0xC00 - ang);
				q->pos[2] = (int16_t)((int16_t)((sn * 2100) >> 12) + p->pos[2]);
				q->spin = (int16_t)(CrtRand() % 30 + 0x1E);
				q->scale = 0x1000;
			}
		}
		if (p->counter == 0x42)
		{
			RingNode *b = (RingNode *)AddTask(ORIG_BeamTask);
			CopyAnchor(b->pos, p);
			b->counter = 0;
		}
		if (p->counter == 0x4E)
		{
			RingNode *d = (RingNode *)AddTask(ORIG_DiskTask);
			CopyAnchor(d->pos, p);
			d->counter = 0;
			d->angle = (int16_t)(CrtRand() % 0x1000);
			d->spin = 0x28;
			d->scale = 0;
			d->dscale = 0x140;
			d = (RingNode *)AddTask(ORIG_DiskTask);
			CopyAnchor(d->pos, p);
			d->counter = 0;
			d->angle = (int16_t)(CrtRand() % 0x1000);
			d->spin = -0x28;
			d->scale = 0;
			d->dscale = 0x280;
		}
		if (p->counter == 0x5A)
		{
			RingNode *f = (RingNode *)AddTask(ORIG_FlashTask);
			CopyAnchor(f->pos, p);
			f->counter = 0;
			f->scale = 0x3C00;
			f->dscale = 0xA00;
			f->w20 = 0x3C00;
		}
		if (p->counter >= 0x67)
		{
			if (p->counter == 0x67)
			{
				StageGroupsFlag(true);
				SpawnFadeIns(8);
			}
			SetScreenFlash((uint32_t)((0x6F - p->counter) << 9), 0);
		}
		if (p->counter == 0x6A) ApplyActionResultToTarget(Ctx()->actions[p->action].targets);
		if (p->counter == 0x6F)
		{
			int32_t next = p->action + 1;
			if (next <= Ctx()->actions[0].last_action)
			{
				DirectorNode *d = (DirectorNode *)AddTask(ORIG_DirectorTask);
				d->counter = 0;
				d->action = (int16_t)next;
				d->target = Ctx()->actions[next].targets[0];
			}
		}
		if (p->counter == 1)
		{
			int16_t pos[4];
			GetDefaultEffectPosition(Entity(p->target), pos);
			BdPlaySE3D(SOUND_Holy, 1, pos);
		}
		p->counter++;
		if (p->counter <= 0x6F) return 0;
		if ((uint16_t)p->action == (uint16_t)Ctx()->actions[0].last_action) SetScreenFlash(0, 0);
		return TASK_END;
	}
}

	void register_mag175_holy()
	{
		register_port(holy175::ORIG_RootTask, (void *)holy175::RootTask, "H175 RootTask", 175);
		register_port(holy175::ORIG_RestoreTask, (void *)holy175::RestoreTask, "H175 RestoreTask", 175);
		register_port(holy175::ORIG_DirectorTask, (void *)holy175::DirectorTask, "H175 DirectorTask", 175);
		register_port(holy175::ORIG_ShatterTask, (void *)holy175::ShatterTask, "H175 ShatterTask", 175);
		register_port(holy175::ORIG_FadeInTask, (void *)holy175::FadeInTask, "H175 FadeInTask", 175);
		register_port(holy175::ORIG_FadeOutTask, (void *)holy175::FadeOutTask, "H175 FadeOutTask", 175);
		register_port(holy175::ORIG_SparkleTask, (void *)holy175::SparkleTask, "H175 SparkleTask", 175);
		register_port(holy175::ORIG_OrbitCtrlTask, (void *)holy175::OrbitCtrlTask, "H175 OrbitCtrlTask", 175);
		register_port(holy175::ORIG_OrbitTask, (void *)holy175::OrbitTask, "H175 OrbitTask", 175);
		register_port(holy175::ORIG_MoteTask, (void *)holy175::MoteTask, "H175 MoteTask", 175);
		register_port(holy175::ORIG_RingTask, (void *)holy175::RingTask, "H175 RingTask", 175);
		register_port(holy175::ORIG_WaveTask, (void *)holy175::WaveTask, "H175 WaveTask", 175);
		register_port(holy175::ORIG_ExpandTask, (void *)holy175::ExpandTask, "H175 ExpandTask", 175);
		register_port(holy175::ORIG_PillarTask, (void *)holy175::PillarTask, "H175 PillarTask", 175);
		register_port(holy175::ORIG_BeamTask, (void *)holy175::BeamTask, "H175 BeamTask", 175);
		register_port(holy175::ORIG_DiskTask, (void *)holy175::DiskTask, "H175 DiskTask", 175);
		register_port(holy175::ORIG_FlashTask, (void *)holy175::FlashTask, "H175 FlashTask", 175);
		// 30 fps layer: see mag175_holy_held.inc
		FX_HELD(register_mag175_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag175_holy_held.inc"
#endif
