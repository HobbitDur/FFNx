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

// Effect 105: Thundaga (spell, MAG_105_*, file mag104).
//
// Structure (setup MAG_105_THUNDAGA 0x6D3DB0 -> _Init 0x6D3DE0, file loader 0x6D3DC0 = mag104.tim):
//   RootTask (0x6D3E50) - alternates the packet arenas (magic buffer + 0x8C1C / + 0x20C1C and the
//     second arena + 0x20C1C / + 0x38C1C), computes the effect camera, at counter 1 (of a 46-tick
//     cycle) sets up the pools and spawns the bolt of the next action; updates and draws the
//     debris triangles (310 slots in the magic buffer), runs the bolt, shard and particle queues,
//     sets the screen flash; when everything is over queues the texture restore.
//   Bolt (0x6D43B0) - node 0x884: the lightning model, a prim-model layout (0x13009C0) played by
//     the shared player 0x701970 with the callback BoltPart (0x6D4910) that draws each object as
//     a prim model or as UV-scrolled textured quads (0x6D4BD0); flashes the target's colour to
//     white at frames 2, 30, 39; screen flash ramp; sound at 0, fades at 3, 30, 39. Frame 28: the
//     screen shatters: 14 shard meshes (exe data) are scaled, recentred and thrown, the scene is
//     rendered into a 128 x 128 texture (0x6D5620); frames 34..41: rings of 8 sparks; frames
//     45..56: flash sprites; frame 64: damage, end.
//   Shard (0x6D5AD0) - a mesh flying and spinning; when its life ends it breaks into debris
//     (one triangle per polygon, 0x6D5C00).
//   Spark (0x6D6100), Flash (0x6D61C0) - sprite sequences; WaitTexRestore (0x6D62B0).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Module globals: 0x2543CF0..0x2543DF0 (second arena, flash level, render-to-texture prims,
// texture file / base, effect camera, context, root pool, packet cursor 0x2543DAC, queues);
// pools, debris and the morph vertex buffer are in the magic buffer; the shard meshes are exe
// data (0x130E55C..0x131159C) rewritten at frame 28.

#include "mag_common.h"

namespace ff8fx
{
namespace thundaga105
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&TexBase() { return var<uint8_t *>(0x2543D4C); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2543D88); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2543DAC); }
	inline uint32_t &Arena2() { return var<uint32_t>(0x2543CF0); }          // second arena (offscreen model render)
	inline int32_t &FlashLevel() { return var<int32_t>(0x2543CF4); }        // screen flash, max of the bolts
	inline Mat4x3 &EffectCamera() { return var<Mat4x3>(0x2543D68); }
	inline TaskQueue &RootQueue() { return var<TaskQueue>(0x2543DE0); }
	inline TaskQueue &BoltQueue() { return var<TaskQueue>(0x2543DD0); }     // pool: magic buffer, 3 x 0x884
	inline TaskQueue &ShardQueue() { return var<TaskQueue>(0x2543DC0); }    // pool: + 0x198C, 0xE x 0x3C
	inline TaskQueue &SparkQueue() { return var<TaskQueue>(0x2543DB0); }    // pool: + 0x1CD4, 0x20 x 0x2C
	static uint8_t *const PRIM_Env = (uint8_t *)0x2543CF8;    // DR_ENV of the offscreen render
	static uint8_t *const PRIM_Offset = (uint8_t *)0x2543D40; // draw offset
	static int16_t *const RECT_Offset = (int16_t *)0x2543D50;
	static uint8_t *const PRIM_Area = (uint8_t *)0x2543D58;   // draw area

	static const uint32_t ORIG_RootTask = 0x6D3E50;
	static const uint32_t ORIG_BoltTask = 0x6D43B0;
	static const uint32_t ORIG_ShardTask = 0x6D5AD0;
	static const uint32_t ORIG_SparkTask = 0x6D6100;
	static const uint32_t ORIG_FlashTask = 0x6D61C0;
	static const uint32_t ORIG_WaitTexRestore = 0x6D62B0;
	static const uint32_t MODEL_Bolt = 0x13009C0;          // prim-model layout of the lightning
	static const uint8_t *const BOLT_QuadParts = (const uint8_t *)0x1312660; // [object] != 0: UV-scrolled quads
	static const void *const SOUND_Thundaga = (const void *)0x131265C;
	static const uint32_t SEQ_Spark = 0x13115DC;
	static const uint32_t SEQ_Flash = 0x131136C;
	static const int DEBRIS_COUNT = 0x136;
	// shard meshes: destination (rewritten) / source pairs
	static const uint32_t SHARD_MESH[14][2] = {
		{ 0x130E55C, 0x130E86C }, { 0x130E8E4, 0x130EC34 }, { 0x130ECB4, 0x130EE84 }, { 0x130EED4, 0x130F12C },
		{ 0x130F18C, 0x130F4DC }, { 0x130F55C, 0x130F7C4 }, { 0x130F824, 0x130FA7C }, { 0x130FADC, 0x130FD34 },
		{ 0x130FD94, 0x130FFCC }, { 0x131002C, 0x131048C }, { 0x131052C, 0x1310944 }, { 0x13109DC, 0x1310C74 },
		{ 0x1310CDC, 0x1310F2C }, { 0x1310F8C, 0x13112EC },
	};

	inline uint32_t RenderOT44() { return var<uint32_t>(0x1D8E04C) + 0x44; }

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline int32_t ComputeCos(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(angle); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }        // GTE_MatrixMultiply
	inline void MatrixMultiplyVector(const void *m, const void *in, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x56C4F0)(m, in, out); }
	inline uint32_t RenderGeometry(uint32_t model, void *header, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint32_t, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(model, header, ot, mode, cursor); }
	inline void GetScreenOffset(int32_t *x, int32_t *y) { fn<void (__cdecl *)(int32_t *, int32_t *)>(0x56CD10)(x, y); }
	inline void SetScreenOffset(int32_t x, int32_t y) { fn<void (__cdecl *)(int32_t, int32_t)>(0x56CCE0)(x, y); }  // Call_Bs_ParseCamera
	inline void SetDrawEnvPrim(void *prim, uint32_t env) { fn<void (__cdecl *)(void *, uint32_t)>(0x45C0F0)(prim, env); }
	inline void SetDrawAreaPrim(void *prim, const int16_t *rect) { fn<void (__cdecl *)(void *, const int16_t *)>(0x45C940)(prim, rect); }
	inline void SetDrawOffsetPrim(void *prim, const int16_t *xy) { fn<void (__cdecl *)(void *, const int16_t *)>(0x45C9B0)(prim, xy); }
	// software GTE
	inline void GteLoadV012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(a, b, c); }
	inline void GteLoadV0FromDwords(const void *v) { fn<void (__cdecl *)(const void *)>(0x45E060)(v); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = R * V0
	inline void GteMVMVA_RotIR() { fn<void (__cdecl *)()>(0x460820)(); }   // IR = R * IR
	inline void GteLoadIRFromMatrixColumn(const void *col) { fn<void (__cdecl *)(const void *)>(0x45E180)(col); }
	inline void GteStoreIRToMatrixColumn(void *col) { fn<void (__cdecl *)(void *)>(0x45E470)(col); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ3() { fn<void (__cdecl *)()>(0x45E5C0)(); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteStoreOTZ(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }  // GTE_ReadOTZ
	inline void GteSetFarColour(uint32_t r, uint32_t g, uint32_t b) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(r, g, b); }  // someCameraWork_45DD60
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }            // set_unk_1CA8A28
	inline void GteLoadRGB012(const void *a, const void *b, const void *c) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45E120)(a, b, c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteDPCT() { fn<void (__cdecl *)()>(0x45F4C0)(); }
	inline void GteStoreRGB2(void *c) { fn<void (__cdecl *)(void *)>(0x45E360)(c); }                     // set_param_with_dword_1CA8A68
	inline void GteStoreRGB012(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E370)(a, b, c); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x1C bytes
	{
		TaskNode hdr;
		int16_t counter;  // +0x0C 0..45, a bolt spawn at 1
		uint8_t action;   // +0x0E next action
		uint8_t pad0F;
		void *last_bolt;  // +0x10
		uint8_t state;    // +0x14 0 = not started, 1 = running, 2 = texture restore
		uint8_t pad15[3];
		uint32_t arena;   // +0x18 packet arena parity
	};
	struct BoltNode // pool of 3 nodes of 0x884 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C frame
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 target entity
		uint32_t colour;   // +0x14 the target's colour word (entity + 0x28) at the spawn
		int32_t scale;     // +0x18 ((target size / 2) + 0xE86) / 2
		uint8_t layout[0x868]; // +0x1C prim-model layout (prim::Layout: data, frame, state)
	};
	struct ShardNode // pool of 0xE nodes of 0x3C bytes
	{
		TaskNode hdr;
		int16_t life;      // +0x0C ticks left
		int16_t pad0E;
		int16_t pos[4];    // +0x10
		int16_t vel[4];    // +0x18 1/16 units per tick
		int16_t acc[4];    // +0x20 added to vel every tick
		int16_t ang[4];    // +0x28 rotation angles
		int16_t angvel[4]; // +0x30
		uint8_t *mesh;     // +0x38 prim model (recentred shard mesh)
	};
	struct SpriteNode // Spark and Flash: pool of 0x20 nodes of 0x2C bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C sequence frame
		int16_t end;       // +0x0E
		uint32_t seq;      // +0x10 sprite sequence
		int16_t pos[3];    // +0x14
		int16_t size;      // +0x1A
		int16_t vel[4];    // +0x1C (Spark)
		int16_t acc[4];    // +0x24 (Spark) added to vel every tick
	};
	struct WaitNode { TaskNode hdr; int16_t counter; int16_t done; uint8_t pad10[0x1C]; }; // done: set by the restore task
	// debris triangle (magic buffer + 0x2A24, 0x136 slots of 0x50 bytes; free when size is 0)
	struct Debris
	{
		int16_t pos[3];    // +0x00
		int16_t size;      // +0x06 0x1000 at the spawn, dies at 0x400 or less
		int16_t vel[3];    // +0x08 1/16 units per tick, y bounces at 0
		uint16_t clut;     // +0x0E
		int16_t acc[3];    // +0x10
		uint16_t tpage;    // +0x16
		int16_t v[3][4];   // +0x18 corners relative to pos, 4th word = the corner's UV
		uint32_t rgb[3];   // +0x30 (+0x33 = GPU code 0x34, gouraud textured triangle)
		int16_t rot[9];    // +0x3C spin applied to the corners every tick
		int16_t dsize;     // +0x4E
	};
	// argument of the bolt's part callback (the bolt task's stack)
	struct BoltArg
	{
		int16_t pos[4];    // +0x00 effect anchor x, target height, z
		uint8_t *morph;    // +0x08 vertex buffer of the morphing parts (magic buffer + 0x2254)
		int32_t scale[4];  // +0x0C
		const uint8_t *quad_parts; // +0x1C
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x1C && sizeof(BoltNode) == 0x884 && sizeof(ShardNode) == 0x3C && sizeof(SpriteNode) == 0x2C && sizeof(WaitNode) == 0x2C, "Thundaga nodes");
	static_assert(sizeof(Debris) == 0x50 && sizeof(BoltArg) == 0x20, "Thundaga structs");

	inline Debris *DebrisSlots() { return (Debris *)(TexBase() + 0x2A24); }
	inline int16_t Sar16(int16_t v, int n) { return (int16_t)(v >> n); }

	// Effect_Memset32_wrapper (0x701200): `count` dwords
	static void FillDwords(void *dst, uint32_t value, uint32_t count)
	{
		uint32_t *d = (uint32_t *)dst;
		for (uint32_t k = 0; k < count; k++) d[k] = value;
	}

	// x / -10 (0x99999999, >> 2)
	static int16_t DivNeg10(int16_t v)
	{
		int32_t x = v;
		int32_t hi = (int32_t)(((int64_t)x * (int32_t)0x99999999) >> 32);
		hi >>= 2;
		hi += (int32_t)((uint32_t)hi >> 31);
		return (int16_t)hi;
	}

	// x / 3 (0x55555556)
	static int32_t Div3(int32_t x)
	{
		int32_t hi = (int32_t)(((int64_t)x * 0x55555556) >> 32);
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// MAG_003_sub_6D5940: rotation about the X axis (translation and pad untouched)
	static void RotX(int16_t angle, Mat4x3 *m)
	{
		int32_t s = ComputeSin(angle);
		int32_t c = ComputeCos(angle);
		m->m[1][2] = (int16_t)-s;
		m->m[2][1] = (int16_t)s;
		m->m[0][0] = 0x1000;
		m->m[0][1] = 0;
		m->m[0][2] = 0;
		m->m[1][0] = 0;
		m->m[1][1] = (int16_t)c;
		m->m[2][0] = 0;
		m->m[2][2] = (int16_t)c;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag105_thundaga_held.h"
#endif

namespace ff8fx
{
namespace thundaga105
{
	// ------------------------------------------------------------------
	// Debris triangles
	// ------------------------------------------------------------------
	// size step, move (1/16 units), y bounce at 0 and acceleration; false = the debris dies
	static bool DebrisMove(Debris *e)
	{
		e->size = (int16_t)(e->size + e->dsize);
		if (e->size <= 0x400)
		{
			e->size = 0;
			return false;
		}
		int16_t vx = e->vel[0], vy = e->vel[1], vz = e->vel[2];
		e->pos[0] = (int16_t)(e->pos[0] + Sar16(vx, 4));
		e->pos[1] = (int16_t)(e->pos[1] + Sar16(vy, 4));
		int16_t y = e->pos[1];
		e->pos[2] = (int16_t)(e->pos[2] + Sar16(vz, 4));
		if (y >= 0) e->vel[1] = (int16_t)-e->vel[1];
		e->vel[2] = (int16_t)(e->vel[2] + e->acc[2]);
		e->vel[0] = (int16_t)(e->vel[0] + e->acc[0]);
		e->vel[1] = (int16_t)(e->vel[1] + e->acc[1]);
		return true;
	}

	// MAG_105_sub_6D40E0: spin, shrink and move every debris; returns how many are alive
	static int32_t DebrisUpdate()
	{
		int32_t alive = 0;
		Debris *e = DebrisSlots();
		for (int i = 0; i < DEBRIS_COUNT; i++, e++)
		{
			if (e->size == 0) continue;
			GteSetRotMatrixCtrl((const Mat4x3 *)e->rot);
			for (int k = 0; k < 3; k++)
			{
				GteLoadV0(e->v[k]);
				GteMVMVA_RotV0();
				GteStoreIR123(e->v[k]);
			}
			if (DebrisMove(e)) alive++;
		}
		return alive;
	}

	// MAG_105_sub_6D41E0: every debris of `slots` as a gouraud textured triangle scaled by its size
	static uint32_t DebrisDraw(const Debris *slots, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x64);
		// +0x00 scale matrix (+0x14 translation), +0x20 camera, +0x40 camera * scale, +0x60 OTZ
		*(int16_t *)(h + 0xE) = 0;
		*(int16_t *)(h + 0xC) = 0;
		*(int16_t *)(h + 0xA) = 0;
		*(int16_t *)(h + 6) = 0;
		*(int16_t *)(h + 4) = 0;
		*(int16_t *)(h + 2) = 0;
		memcpy(h + 0x20, &Camera(), 0x20);
		const Debris *e = slots;
		for (int i = 0; i < DEBRIS_COUNT; i++, e++)
		{
			int16_t size = e->size;
			if (size == 0) continue;
			*(int16_t *)(h + 0x10) = size;
			*(int16_t *)(h + 8) = size;
			*(int16_t *)h = size;
			GteSetRotMatrixCtrl((const Mat4x3 *)h);
			*(int32_t *)(h + 0x14) = e->pos[0];
			*(int32_t *)(h + 0x18) = e->pos[1];
			*(int32_t *)(h + 0x1C) = e->pos[2];
			GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x20));
			GteLoadIRFromMatrixColumn(h);
			GteMVMVA_RotIR();
			GteStoreIRToMatrixColumn(h + 0x40);
			GteLoadIRFromMatrixColumn(h + 2);
			GteMVMVA_RotIR();
			GteStoreIRToMatrixColumn(h + 0x42);
			GteLoadIRFromMatrixColumn(h + 4);
			GteMVMVA_RotIR();
			GteStoreIRToMatrixColumn(h + 0x44);
			GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x20));
			GteLoadV0FromDwords(h + 0x14);
			GteMVMVA_RotV0Tr();
			GteReadMAC123((int32_t *)(h + 0x54));
			GteLoadV012(e->v[0], e->v[1], e->v[2]);
			GteSetRotMatrixCtrl((const Mat4x3 *)(h + 0x40));
			GteSetTransVectorCtrl((const Mat4x3 *)(h + 0x40));
			GteRTPT();
			uint8_t *pk = (uint8_t *)cursor;
			*(uint32_t *)(pk + 4) = e->rgb[0];
			*(uint32_t *)(pk + 0x10) = e->rgb[1];
			*(uint32_t *)(pk + 0x1C) = e->rgb[2];
			*(uint32_t *)pk = 0x9000000;
			GteReadSXY012Split(pk + 8, pk + 0x14, pk + 0x20);
			GteAVSZ3();
			*(uint16_t *)(pk + 0xE) = e->clut;
			*(uint16_t *)(pk + 0x1A) = e->tpage;
			*(int16_t *)(pk + 0xC) = e->v[0][3];
			*(int16_t *)(pk + 0x18) = e->v[1][3];
			*(int16_t *)(pk + 0x24) = e->v[2][3];
			GteStoreOTZ(h + 0x60);
			int32_t otz = *(int32_t *)(h + 0x60);
			if (otz > 0)
			{
				InsertPrimAutoDepth(ot + (uint32_t)(otz >> mode) * 4, pk);
				cursor = (uint32_t)(pk + 0x28);
			}
		}
		FieldFree(0x64);
		return cursor;
	}

	// MAG_105_sub_6D5CF0: one debris triangle from corners i0 i1 i2 of the breaking shard (work
	// block w: +0 polygon cursor, +4 vertices, +8 position, +0x10 velocity, +0x18 acceleration,
	// +0x20 rotation, +0x40 rotated corners); nothing when every slot is taken
	static void DebrisSpawn(uint8_t *w, uint32_t i0, uint32_t i1, uint32_t i2, uint32_t c0, uint32_t c1, uint32_t c2,
		uint16_t a8, uint16_t a9, uint16_t a10, uint16_t a11, uint16_t a12)
	{
		(void)a8; // (passed, never read)
		Debris *e = DebrisSlots();
		if (e->size != 0)
		{
			int left = DEBRIS_COUNT;
			for (;;)
			{
				e++;
				if (--left == 0) return;
				if (e->size == 0) break;
			}
		}
		const uint8_t *verts = *(uint8_t **)(w + 4);
		int16_t *r0 = (int16_t *)(w + 0x40), *r1 = (int16_t *)(w + 0x48), *r2 = (int16_t *)(w + 0x50);
		MatrixMultiplyVector(w + 0x20, verts + i0 * 4, r0);
		MatrixMultiplyVector(w + 0x20, verts + i1 * 4, r1);
		MatrixMultiplyVector(w + 0x20, verts + i2 * 4, r2);
		int16_t cx = (int16_t)Div3((int32_t)r2[0] + r0[0] + r1[0]);
		int16_t cy = (int16_t)Div3((int32_t)r0[1] + r2[1] + r1[1]);
		int16_t cz = (int16_t)Div3((int32_t)r0[2] + r2[2] + r1[2]);
		const int16_t *wp = (const int16_t *)(w + 8), *wv = (const int16_t *)(w + 0x10), *wa = (const int16_t *)(w + 0x18);
		e->pos[0] = (int16_t)(wp[0] + cx);
		e->pos[1] = (int16_t)(wp[1] + cy);
		e->pos[2] = (int16_t)(wp[2] + cz);
		e->vel[0] = (int16_t)(Sar16(cx, 1) + wv[0]);
		e->vel[1] = (int16_t)(wv[1] + cy);
		e->vel[2] = (int16_t)(Sar16(cz, 1) + wv[2]);
		e->clut = a9;
		e->acc[2] = 0;
		e->acc[0] = 0;
		e->acc[1] = wa[1];
		e->tpage = a9;
		e->v[0][0] = (int16_t)(r0[0] - cx);
		e->v[0][1] = (int16_t)(r0[1] - cy);
		e->v[0][2] = (int16_t)(r0[2] - cz);
		e->v[0][3] = (int16_t)a10;
		e->v[1][0] = (int16_t)(r1[0] - cx);
		e->v[1][1] = (int16_t)(r1[1] - cy);
		e->v[1][2] = (int16_t)(r1[2] - cz);
		e->v[1][3] = (int16_t)a11;
		e->v[2][0] = (int16_t)(r2[0] - cx);
		e->v[2][1] = (int16_t)(r2[1] - cy);
		e->v[2][2] = (int16_t)(r2[2] - cz);
		e->v[2][3] = (int16_t)a12;
		e->rgb[0] = c0;
		((uint8_t *)e)[0x33] = 0x34;
		e->rgb[1] = c1;
		e->rgb[2] = c2;
		int16_t angles[4] = { 0, 0, 0, 0 };
		angles[0] = (int16_t)((CrtRand() - 0x4000) >> 7);
		angles[1] = (int16_t)((CrtRand() - 0x4000) >> 7);
		angles[2] = (int16_t)((CrtRand() - 0x4000) >> 7);
		BuildRotationMatrixFromAngles(angles, (Mat4x3 *)e->rot);
		e->size = 0x1000;
		e->dsize = (int16_t)(-100 - (CrtRand() & 0x7F));
	}

	// sub_6D5C80: the mesh's triangles (records of 0x14 bytes)
	static void BreakTriangles(uint8_t *w)
	{
		uint8_t *pl = *(uint8_t **)w;
		int32_t n = *(int32_t *)pl;
		pl += 4;
		*(uint8_t **)w = pl;
		if (n <= 0) return;
		for (uint32_t left = (uint32_t)n; left; left--)
		{
			uint32_t c = *(uint32_t *)pl;
			DebrisSpawn(w, *(uint16_t *)(pl + 4), *(uint16_t *)(pl + 6), *(uint16_t *)(pl + 8), c, c, c,
				*(uint16_t *)(pl + 0xE), *(uint16_t *)(pl + 0x12), *(uint16_t *)(pl + 0xC), *(uint16_t *)(pl + 0x10), *(uint16_t *)(pl + 0xA));
			pl += 0x14;
		}
		*(uint8_t **)w = pl;
	}

	// sub_6D5F30: the mesh's quads (records of 0x1C bytes): one triangle each (corners 0, 1, 2)
	static void BreakQuads(uint8_t *w)
	{
		uint8_t *pl = *(uint8_t **)w;
		int32_t n = *(int32_t *)pl;
		pl += 4;
		*(uint8_t **)w = pl;
		if (n <= 0) return;
		for (uint32_t left = (uint32_t)n; left; left--)
		{
			DebrisSpawn(w, *(uint16_t *)(pl + 4), *(uint16_t *)(pl + 6), *(uint16_t *)(pl + 8),
				*(uint32_t *)pl, *(uint32_t *)(pl + 0x14), *(uint32_t *)(pl + 0x18),
				*(uint16_t *)(pl + 0xE), *(uint16_t *)(pl + 0x12), *(uint16_t *)(pl + 0xC), *(uint16_t *)(pl + 0x10), *(uint16_t *)(pl + 0xA));
			pl += 0x1C;
		}
		*(uint8_t **)w = pl;
	}

	// sub_6D5C00: a shard breaks into debris triangles
	static void ShardBreak(const int16_t *pos, const int16_t *vel, const int16_t *acc, const int16_t *angles, uint8_t *mesh)
	{
		uint8_t *w = (uint8_t *)FieldAlloc(0x58);
		BuildRotationMatrixFromAngles(angles, (Mat4x3 *)(w + 0x20));
		memcpy(w + 8, pos, 8);
		memcpy(w + 0x10, vel, 8);
		memcpy(w + 0x18, acc, 8);
		*(uint8_t **)(w + 4) = mesh + 8;
		*(uint8_t **)w = mesh + *(int32_t *)mesh + 8;
		BreakTriangles(w);
		*(uint8_t **)w += 0xC;
		BreakQuads(w);
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Root task (0x6D3E50)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag105_thundaga_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = (uint32_t)(TexBase() + 0x8C1C);
			Arena2() = (uint32_t)(TexBase() + 0x20C1C);
			r->arena = 0;
		}
		else
		{
			PacketCursor() = (uint32_t)(TexBase() + 0x20C1C);
			Arena2() = (uint32_t)(TexBase() + 0x38C1C);
			r->arena = 1;
		}
		FlashLevel() = 0;
		EffectCameraMatrix(&Camera(), &EffectCamera());
		if (r->counter == 1)
		{
			if (r->state == 0)
			{
				r->state = 1;
				InitTaskQueuePool(&BoltQueue(), TexBase(), 0x884, 3);
				InitTaskQueuePool(&ShardQueue(), TexBase() + 0x198C, 0x3C, 0xE);
				InitTaskQueuePool(&SparkQueue(), TexBase() + 0x1CD4, 0x2C, 0x20);
				for (int i = 0; i < DEBRIS_COUNT; i++) DebrisSlots()[i].size = 0;
			}
			ActionData *acts = Ctx()->actions;
			if (r->action <= acts[0].last_action)
			{
				uint8_t *entity = Entity(acts[r->action].targets[0]);
				BoltNode *b = (BoltNode *)AddTaskToQueue(&BoltQueue(), ORIG_BoltTask);
				if (b)
				{
					FillDwords(&b->counter, 0, 0x21E);
					b->action = (int16_t)(uint16_t)r->action;
					b->entity = entity;
					b->colour = *(uint32_t *)(entity + 0x28);
					b->scale = (((int32_t)*(int16_t *)(entity + 0x26) >> 1) + 0xE86) >> 1;
					DecodeModelPrimLayout(MODEL_Bolt, b->layout, 0x868);
					r->last_bolt = b;
					r->action++;
				}
			}
		}
		int32_t debris;
		if (r->state == 1)
		{
			debris = DebrisUpdate();
			// 30 fps layer: see mag105_thundaga_held.inc
			FX_HELD(held_note_debris();)
			PacketCursor() = DebrisDraw(DebrisSlots(), RenderOT44(), 2, PacketCursor());
		}
		else debris = 1;
		int32_t bolts, shards, sparks;
		if (r->state)
		{
			bolts = ExecuteTaskQueue(&BoltQueue());
			shards = ExecuteTaskQueue(&ShardQueue());
			sparks = ExecuteTaskQueue(&SparkQueue());
		}
		else bolts = shards = sparks = debris;
		SetScreenFlash((uint32_t)FlashLevel(), 0);
		if (r->state == 1)
		{
			if (!bolts && !shards && !sparks && !debris && r->action > Ctx()->actions[0].last_action)
			{
				if (Ctx()->flags & 2) return TASK_END;
				WaitNode *w = (WaitNode *)AddTaskToQueue(&SparkQueue(), ORIG_WaitTexRestore);
				FillDwords(&w->counter, (uint32_t)shards, 8);
				r->state = 2;
			}
		}
		else if (r->state == 2)
		{
			if (!sparks) return TASK_END;
		}
		r->counter++;
		if (r->counter >= 0x2E) r->counter = 0;
		return 0;
	}

	// ------------------------------------------------------------------
	// Texture restore wait (0x6D62B0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl WaitTexRestore(TaskNode *n)
	{
		WaitNode *w = (WaitNode *)n;
		if (w->counter == 0) TextureRestoreTask(TexBase() + 0x8C1C, &w->done);
		int16_t done = w->done;
		w->counter++;
		return done ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Bolt parts
	// ------------------------------------------------------------------
	// MAG_011_sub_6D5580: vertices of keyframe k0 and k1 blended by w (4.12) into out
	static void MorphVertices(const uint8_t *model, int32_t k0, int32_t k1, int32_t w, uint8_t *out)
	{
		int32_t inv = 0x1000 - w;
		const uint8_t *a = k0 == 0 ? model + 0xC : model + (uint32_t)mul32(*(const int32_t *)(model + 4), k0) * 8 + 0xC;
		const uint8_t *b = k1 == 0 ? model + 0xC : model + (uint32_t)mul32(*(const int32_t *)(model + 4), k1) * 8 + 0xC;
		uint32_t n = *(const uint32_t *)(model + 4);
		if (n == 0) return;
		do
		{
			GteSetIR0(inv);
			GteLoadIR123(a);
			GteGPF();
			GteSetIR0(w);
			GteLoadIR123(b);
			GteGPL();
			GteStoreIR123(out);
			out += 8;
			a += 8;
			b += 8;
		} while (--n);
	}

	// GP0(E2) texture window word of the 4 halves at tw (+0 / +2 bytes, +4 / +6 words)
	static uint32_t TexWindow(const uint8_t *tw)
	{
		uint32_t e = (uint32_t)(tw[2] & 0xF8) | 0xFFFE2000u;
		e <<= 5;
		e |= (uint32_t)(tw[0] & 0xF8);
		e <<= 5;
		e |= ~((uint32_t)*(const uint16_t *)(tw + 6) - 1) & 0xF8;
		e <<= 2;
		e |= (uint32_t)((int32_t)~((uint32_t)*(const uint16_t *)(tw + 4) - 1) >> 3) & 0x1F;
		return e;
	}

	// screen coordinate out of range: x 0..0xA00, y 0..0x6C0
	static inline bool OutX(const uint8_t *xy) { int16_t v = *(const int16_t *)xy; return v < 0 || v > 0xA00; }
	static inline bool OutY(const uint8_t *xy) { int16_t v = *(const int16_t *)xy; return v < 0 || v > 0x6C0; }

	// the texture window prims around a scrolled quad: window (h + 0x5C) inserted first, then the
	// quad, then the window of h + 0x64
	static uint8_t *QuadWindows(const uint8_t *h, uint32_t bucket, uint8_t *pk, uint8_t *p2)
	{
		*(uint32_t *)p2 = 0x2000000;
		*(uint32_t *)(p2 + 4) = TexWindow(h + 0x5C);
		*(uint32_t *)(p2 + 8) = 0;
		InsertPrimAutoDepth(bucket, p2);
		InsertPrimAutoDepth(bucket, pk);
		uint8_t *p3 = p2 + 0xC;
		*(uint32_t *)p3 = 0x2000000;
		*(uint32_t *)(p3 + 4) = TexWindow(h + 0x64);
		*(uint32_t *)(p3 + 8) = 0;
		InsertPrimAutoDepth(bucket, p3);
		return p3 + 0xC;
	}

	// sub_6D4C10: textured quads (records of 0x18 bytes: code/colour, 4 vertex indices, UVs),
	// fog toward the far colour by h + 0x0C, V scrolled by h + 0x5A (wrapping by h + 0x6A)
	static uint32_t MeshFT4(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *pl = *(uint8_t **)(h + 0x20);
		int32_t count = *(int32_t *)pl;
		pl += 4;
		const uint8_t *vtx = *(uint8_t **)(h + 4);
		*(uint8_t **)(h + 0x20) = pl;
		uint8_t *pk = (uint8_t *)cursor;
		if (count <= 0) return (uint32_t)pk;
		for (uint32_t left = (uint32_t)count; left; left--, pl += 0x18)
		{
			GteLoadV012(vtx + *(uint16_t *)(pl + 4) * 4, vtx + *(uint16_t *)(pl + 6) * 4, vtx + *(uint16_t *)(pl + 8) * 4);
			GteRTPT();
			*(uint32_t *)pk = 0x9000000;
			*(uint32_t *)(pk + 4) = *(uint32_t *)pl;
			*(uint32_t *)(pk + 0xC) = *(uint32_t *)(pl + 0xC);
			*(uint32_t *)(pk + 0x14) = *(uint32_t *)(pl + 0x10);
			*(uint32_t *)(pk + 0x1C) = *(uint32_t *)(pl + 0x14);
			*(uint32_t *)(pk + 0x24) = *(uint32_t *)(pl + 0x14) >> 16;
			GteReadFLAG(h + 0x30);
			if (*(uint32_t *)(h + 0x30) & 0x60000) continue;
			uint32_t clip = 0;
			GteReadSXY012Split(pk + 8, pk + 0x10, pk + 0x18);
			GteLoadV0(vtx + *(uint16_t *)(pl + 0xA) * 4);
			GteRTPS();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x10)) clip |= 2;
			if (OutX(pk + 0x18)) clip |= 4;
			if (OutY(pk + 0xA)) clip |= 0x10;
			if (OutY(pk + 0x12)) clip |= 0x20;
			if (OutY(pk + 0x1A)) clip |= 0x40;
			GteReadSXY2(pk + 0x20);
			GteAVSZ4();
			if (OutX(pk + 0x20)) clip |= 8;
			if (OutY(pk + 0x22)) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteStoreOTZ(h + 0x2C);
			if (*(int32_t *)(h + 0xC))
			{
				GteSetFarColour(h[8], h[9], h[0xA]);
				GteLoadRGBC(pk + 4);
				GteSetIR0(*(int32_t *)(h + 0xC));
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			uint32_t scroll = *(uint16_t *)(h + 0x5A);
			uint32_t v2 = pk[0x1D] + scroll, v3 = pk[0x25] + scroll, v0 = pk[0xD] + scroll, v1 = pk[0x15] + scroll;
			if ((int32_t)(v3 | v2 | v1 | v0) > 0xFF)
			{
				uint8_t wrap = h[0x6A];
				pk[0xD] = (uint8_t)(v0 - wrap);
				pk[0x15] = (uint8_t)(v1 - wrap);
				pk[0x1D] = (uint8_t)(v2 - wrap);
				v3 = (uint8_t)(v3 - wrap);
			}
			else
			{
				pk[0xD] = (uint8_t)v0;
				pk[0x15] = (uint8_t)v1;
				pk[0x1D] = (uint8_t)v2;
			}
			pk[0x25] = (uint8_t)v3;
			uint32_t bucket = ot + (uint32_t)(*(int32_t *)(h + 0x2C) >> mode) * 4;
			pk = QuadWindows(h, bucket, pk, pk + 0x28);
		}
		*(uint8_t **)(h + 0x20) = pl;
		return (uint32_t)pk;
	}

	// sub_6D5070: gouraud textured quads (records of 0x24 bytes, colours at +0x18..+0x20), fog
	// toward the far colour, V scrolled by h + 0x5A (wrapping by h + 0x68)
	static uint32_t MeshGT4(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *pl = *(uint8_t **)(h + 0x20);
		int32_t count = *(int32_t *)pl;
		pl += 4;
		const uint8_t *vtx = *(uint8_t **)(h + 4);
		*(uint8_t **)(h + 0x20) = pl;
		uint8_t *pk = (uint8_t *)cursor;
		if (count <= 0) return (uint32_t)pk;
		for (uint32_t left = (uint32_t)count; left; left--, pl += 0x24)
		{
			GteLoadV012(vtx + *(uint16_t *)(pl + 4) * 4, vtx + *(uint16_t *)(pl + 6) * 4, vtx + *(uint16_t *)(pl + 8) * 4);
			GteRTPT();
			*(uint32_t *)pk = 0xC000000;
			*(uint32_t *)(pk + 4) = *(uint32_t *)pl;
			*(uint32_t *)(pk + 0xC) = *(uint32_t *)(pl + 0xC);
			*(uint32_t *)(pk + 0x18) = *(uint32_t *)(pl + 0x10);
			*(uint32_t *)(pk + 0x24) = *(uint32_t *)(pl + 0x14);
			*(uint32_t *)(pk + 0x30) = *(uint32_t *)(pl + 0x14) >> 16;
			GteReadFLAG(h + 0x30);
			if (*(uint32_t *)(h + 0x30) & 0x60000) continue;
			uint32_t clip = 0;
			GteReadSXY012Split(pk + 8, pk + 0x14, pk + 0x20);
			GteLoadV0(vtx + *(uint16_t *)(pl + 0xA) * 4);
			GteRTPS();
			if (OutX(pk + 8)) clip = 1;
			if (OutX(pk + 0x14)) clip |= 2;
			if (OutX(pk + 0x20)) clip |= 4;
			if (OutY(pk + 0xA)) clip |= 0x10;
			if (OutY(pk + 0x16)) clip |= 0x20;
			if (OutY(pk + 0x22)) clip |= 0x40;
			GteReadSXY2(pk + 0x2C);
			GteAVSZ4();
			if (OutX(pk + 0x2C)) clip |= 8;
			if (OutY(pk + 0x2E)) clip |= 0x80;
			if ((clip & 0xF) == 0xF || (clip & 0xF0) == 0xF0) continue;
			GteStoreOTZ(h + 0x2C);
			if (*(int32_t *)(h + 0xC))
			{
				GteSetFarColour(h[8], h[9], h[0xA]);
				GteLoadRGB012(pl + 0x18, pl + 0x1C, pl + 0x20);
				GteSetIR0(*(int32_t *)(h + 0xC));
				GteDPCT();
				GteStoreRGB012(pk + 0x10, pk + 0x1C, pk + 0x28);
				GteLoadRGBC(pk + 4);
				GteDPCS();
				GteStoreRGB2(pk + 4);
			}
			else
			{
				*(uint32_t *)(pk + 0x10) = *(uint32_t *)(pl + 0x18);
				*(uint32_t *)(pk + 0x1C) = *(uint32_t *)(pl + 0x1C);
				*(uint32_t *)(pk + 0x28) = *(uint32_t *)(pl + 0x20);
			}
			uint32_t scroll = *(uint16_t *)(h + 0x5A);
			uint32_t v2 = pk[0x25] + scroll, v3 = pk[0x31] + scroll, v0 = pk[0xD] + scroll, v1 = pk[0x19] + scroll;
			if ((int32_t)(v3 | v2 | v1 | v0) > 0xFF)
			{
				uint8_t wrap = h[0x68];
				pk[0xD] = (uint8_t)(v0 - wrap);
				pk[0x19] = (uint8_t)(v1 - wrap);
				pk[0x25] = (uint8_t)(v2 - wrap);
				v3 = (uint8_t)(v3 - wrap);
			}
			else
			{
				pk[0xD] = (uint8_t)v0;
				pk[0x19] = (uint8_t)v1;
				pk[0x25] = (uint8_t)v2;
			}
			pk[0x31] = (uint8_t)v3;
			uint32_t bucket = ot + (uint32_t)(*(int32_t *)(h + 0x2C) >> mode) * 4;
			pk = QuadWindows(h, bucket, pk, pk + 0x34);
		}
		*(uint8_t **)(h + 0x20) = pl;
		return (uint32_t)pk;
	}

	// MAG_105_sub_6D4BD0: the model's textured quads then its gouraud quads
	static uint32_t ScrolledMesh(uint8_t *h, uint32_t ot, int32_t mode, uint32_t cursor)
	{
		uint8_t *model = *(uint8_t **)h;
		*(uint8_t **)(h + 0x20) = model + *(int32_t *)model + 0xC;
		cursor = MeshFT4(h, ot, mode, cursor);
		*(uint8_t **)(h + 0x20) += 0xC;
		return MeshGT4(h, ot, mode, cursor);
	}

	// MAG_105_sub_6D4910: prim-player callback, one object of the lightning model
	static void __cdecl BoltPart(prim::Layout *l, prim::Record *r, int arg_)
	{
		const BoltArg *arg = (const BoltArg *)arg_;
		if (((int32_t)r->scale[2] | *(const int32_t *)r->scale) == 0) return;
		if (r->a >= 0x1000 && *(const uint32_t *)r->rgb == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x6C);
		uint8_t *model = l->data + *(int32_t *)(l->data + (int32_t)(int16_t)r->flags_lo * 4 + 8);
		*(uint8_t **)h = model;
		int16_t k0 = r->b0, k1 = r->b1;
		int32_t nverts = *(int32_t *)(model + 4);
		if (k0 == k1)
		{
			if (k0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
			else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, k0) * 8 + 0xC;
		}
		else
		{
			int16_t w = r->b;
			if (w == 0)
			{
				if (k0 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, k0) * 8 + 0xC;
			}
			else if (w == 0x1000)
			{
				if (k1 == 0) *(uint8_t **)(h + 4) = model + 0xC;
				else *(uint8_t **)(h + 4) = model + (uint32_t)mul32(nverts, k1) * 8 + 0xC;
			}
			else
			{
				MorphVertices(model, k0, k1, w, arg->morph);
				*(uint8_t **)(h + 4) = arg->morph;
			}
		}
		Mat4x3 m = {};
		ComposeZYXRotationMatrix(r->rot, &m);
		m.t[0] = (int32_t)r->pos[0] + arg->pos[0];
		m.t[1] = (int32_t)r->pos[1] + arg->pos[1];
		m.t[2] = (int32_t)r->pos[2] + arg->pos[2];
		const Mat4x3 *cam = (r->flags & 0x1000) ? &EffectCamera() : &Camera();
		TransformVectorBy3x3Matrix(&Camera(), m.t, m.t);
		MatrixMultiply(cam, &m);
		m.t[0] += Camera().t[0];
		m.t[1] += Camera().t[1];
		m.t[2] += Camera().t[2];
		if (!(*(const uint32_t *)r->scale == 0x10001000 && r->scale[2] == 0x1000))
		{
			int32_t v[4] = { r->scale[0], r->scale[1], r->scale[2], 0 };
			Scale3DMatrix(&m, v);
		}
		*(uint32_t *)(h + 0x1C) = 0x2030;
		Scale3DMatrix(&m, arg->scale);
		*(int32_t *)(h + 0xC) = r->a;
		if (r->a != 0)
		{
			*(uint32_t *)(h + 0x1C) |= 0xC0;
			*(uint32_t *)(h + 8) = *(const uint32_t *)r->rgb;
		}
		GteSetRotMatrix(&m);
		if (arg->quad_parts[(int16_t)r->index] == 0)
		{
			GteSetTransVector(&m);
			PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		}
		else
		{
			m.t[2] -= 0x100;
			GteSetTransVector(&m);
			*(int16_t *)(h + 0x5E) = 0;
			*(int16_t *)(h + 0x5C) = 0;
			*(int16_t *)(h + 0x62) = 0x100;
			*(int16_t *)(h + 0x60) = 0x100;
			*(int16_t *)(h + 0x64) = 0;
			*(int16_t *)(h + 0x66) = 0x80;
			*(int16_t *)(h + 0x68) = 0;
			*(int16_t *)(h + 0x6A) = 0x80;
			uint16_t frame = (uint16_t)l->frame;
			// 30 fps layer: see mag105_thundaga_held.inc
			FX_HELD(frame = held_part_frame(frame);)
			*(uint16_t *)(h + 0x5A) = frame;
			if (frame > 4) *(uint16_t *)(h + 0x5A) = 4;
			*(uint16_t *)(h + 0x5A) = (uint16_t)((h[0x5A] & 3) << 5);
			PacketCursor() = ScrolledMesh(h, RenderOT44(), 2, PacketCursor());
		}
		FieldFree(0x6C);
	}

	// ------------------------------------------------------------------
	// Screen shatter (frame 28)
	// ------------------------------------------------------------------
	// MAG_278_sub_6D6070: bounding box (min x y z, max x y z) of a mesh's vertices
	static void MeshBounds(const uint8_t *mesh, int16_t *box)
	{
		uint32_t left = *(const uint32_t *)(mesh + 4);
		const int16_t *v = (const int16_t *)(mesh + 8);
		left--;
		box[3] = box[0] = v[0];
		box[4] = box[1] = v[1];
		box[5] = box[2] = v[2];
		do
		{
			v += 4;
			for (int k = 0; k < 3; k++)
			{
				if (box[k] > v[k]) box[k] = v[k];
				else if (box[3 + k] < v[k]) box[3 + k] = v[k];
			}
		} while (--left);
	}

	// MAG_105_sub_6D5FA0: the mesh's box and centre (b[6..8]), grown into the shatter box, the
	// mesh recentred on its centre
	static void RecentreMesh(int16_t *b, uint8_t *mesh, int16_t *all)
	{
		MeshBounds(mesh, b);
		b[6] = (int16_t)(((int32_t)b[0] + b[3]) >> 1);
		b[7] = (int16_t)(((int32_t)b[4] + b[1]) >> 1);
		b[8] = (int16_t)(((int32_t)b[5] + b[2]) >> 1);
		for (int k = 0; k < 3; k++)
		{
			if (all[k] > b[k]) all[k] = b[k];
			else if (all[3 + k] < b[3 + k]) all[3 + k] = b[3 + k];
		}
		uint32_t left = *(uint32_t *)(mesh + 4);
		int16_t *v = (int16_t *)(mesh + 8);
		do
		{
			v[0] = (int16_t)(v[0] - b[6]);
			v[1] = (int16_t)(v[1] - b[7]);
			v[2] = (int16_t)(v[2] - b[8]);
			v += 4;
		} while (--left);
	}

	// MAG_105_sub_6D5990: shard mesh dst = GTE transform of src, recentred; the shard flies from
	// the anchor + its centre, falling
	static void ShardSpawn(const int16_t *anchor, uint8_t *dst, const uint8_t *src, int16_t *all)
	{
		ShardNode *s = (ShardNode *)AddTaskToQueue(&ShardQueue(), ORIG_ShardTask);
		if (!s) return;
		FillDwords(&s->life, 0, 0xC);
		uint32_t left = *(uint32_t *)(dst + 4);
		uint8_t *d = dst + 8;
		const uint8_t *v = src + 8;
		do
		{
			GteLoadV0(v);
			GteMVMVA_RotV0Tr();
			GteStoreIR123(d);
			d += 8;
			v += 8;
		} while (--left);
		int16_t b[10];
		RecentreMesh(b, dst, all);
		s->life = (int16_t)((CrtRand() & 0xF) + 8);
		s->pos[0] = (int16_t)(anchor[0] + b[6]);
		s->pos[1] = (int16_t)(anchor[1] + b[7]);
		s->pos[2] = (int16_t)(anchor[2] + b[8]);
		s->mesh = dst;
		s->acc[0] = Sar16(b[6], 5);
		s->acc[2] = Sar16(b[6], 5);
		s->acc[1] = (int16_t)((CrtRand() & 0x1F) + 0x1E);
		s->vel[1] = (int16_t)(-1000 - (CrtRand() & 0x3FF));
		s->ang[2] = 0;
		s->ang[1] = 0;
		s->ang[0] = 0;
		s->angvel[0] = (int16_t)((CrtRand() & 0x7F) - 0x40);
		s->angvel[1] = (int16_t)((CrtRand() & 0x7F) - 0x40);
		s->angvel[2] = (int16_t)((CrtRand() & 0x7F) - 0x40);
	}

	// where the render-to-texture of the shatter draws its prims
	struct CapturePrims { uint8_t *env, *area, *offset; int16_t *offset_xy; uint32_t arena; };

	// the scene of the shatter box rendered into the 128 x 128 texture at (0x2C0, 0x100): draw
	// environment of the other buffer, the model 0x1D989C0 (when shown) seen from above, then draw
	// area / offset back to the texture rectangle
	static void CaptureDraw(const int16_t *anchor, const int16_t *all, const CapturePrims &c)
	{
		int32_t maxx = all[3], minz = -all[2], minx = -all[0], maxz = all[5];
		int32_t width = maxx + minx, depth = maxz + minz;
		int16_t rect[4] = { 0x2C0, 0x100, 0x80, 0x80 };
		int32_t sx = width / 128;
		int32_t sz = depth / 128;
		if (sx % 128) sx++;
		if (sz % 128) sz++;
		Mat4x3 m = {};
		m.t[0] = (minx - maxx) / 2 / sx - anchor[0] / sx;
		m.t[1] = anchor[2] / sz - (minz - maxz) / 2 / sz;
		m.t[2] = var<int16_t>(0x1D8E038);
		RotX(0x400, &m);
		int32_t v[4] = { 0, 0, 0, 0 };
		v[0] = 0x1000000 / (shl32(width, 12) / rect[2]);
		v[2] = 0x1000000 / (shl32(depth, 12) / rect[3]);
		Scale3DMatrix(&m, v);
		SetDrawEnvPrim(c.env, 0x1D969C8 + 0x5C * (uint32_t)(var<uint8_t>(0x1D96A80) ^ 1));
		InsertPrimAutoDepth(RenderOT44(), c.env);
		int32_t ofx = 0, ofy = 0;
		GetScreenOffset(&ofx, &ofy);
		SetScreenOffset(rect[2] / 2, rect[3] / 2);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		if (var<uint8_t>(0x1D989BD) & 1)
		{
			uint8_t *h = (uint8_t *)FieldAlloc(0x2C);
			*(int16_t *)(h + 0x14) = 0;
			*(int16_t *)(h + 0x16) = 0;
			*(int16_t *)(h + 0x18) = 0x140;
			*(int16_t *)(h + 0x1A) = 0xD8;
			*(int16_t *)(h + 0x24) = var<int16_t>(0x1D989BE);
			*(uint32_t *)(h + 0x1C) = var<uint32_t>(0x1D989E4);
			*(uint32_t *)(h + 4) = var<uint32_t>(0x1D98B3C);
			*(int32_t *)(h + 0x20) = -1;
			*(uint32_t *)(h + 0x28) = 0;
			*(uint32_t *)(h + 0x10) = c.arena;
			BuildBoneMatricesFromPose((void *)0x1D989D0);
			ComputeBonesWorldMatrices((void *)0x1D989D0, &m);
			PacketCursor() = RenderGeometry(0x1D989C0, h, RenderOT44(), 0x10, PacketCursor());
			FieldFree(0x2C);
		}
		// a black 128 x 128 quad (written, never inserted)
		uint8_t *pk = (uint8_t *)PacketCursor();
		PacketCursor() += 0x18;
		*(int16_t *)(pk + 8) = 0;
		*(int16_t *)(pk + 0xA) = 0;
		*(int16_t *)(pk + 0xC) = 0x80;
		*(int16_t *)(pk + 0xE) = 0;
		*(int16_t *)(pk + 0x10) = 0;
		*(int16_t *)(pk + 0x12) = 0x80;
		*(int16_t *)(pk + 0x14) = 0x80;
		*(int16_t *)(pk + 0x16) = 0x80;
		pk[4] = 0;
		pk[5] = 0;
		pk[6] = 0;
		*(uint32_t *)pk = 0x5000000;
		pk[7] = 0x28;
		SetScreenOffset(ofx, ofy);
		SetDrawAreaPrim(c.area, rect);
		InsertPrimAutoDepth(RenderOT44(), c.area);
		c.offset_xy[0] = rect[0];
		c.offset_xy[1] = rect[1];
		SetDrawOffsetPrim(c.offset, c.offset_xy);
		InsertPrimAutoDepth(RenderOT44(), c.offset);
	}

	// ------------------------------------------------------------------
	// Bolt (0x6D43B0)
	// ------------------------------------------------------------------
	// MAG_105_sub_6D6240: target colour white at frame k, half way back at k + 1, restored at k + 2
	static void TargetFlash(uint8_t *entity, const uint8_t *colour, int32_t frame, int32_t k)
	{
		if (frame == k)
		{
			entity[0x2A] = 0xFF;
			entity[0x29] = 0xFF;
			entity[0x28] = 0xFF;
		}
		else if (frame == k + 1)
		{
			entity[0x28] = (uint8_t)((colour[0] + 0xFF) >> 1);
			entity[0x29] = (uint8_t)((colour[1] + 0xFF) >> 1);
			entity[0x2A] = (uint8_t)((colour[2] + 0xFF) >> 1);
		}
		else if (frame == k + 2) *(uint32_t *)(entity + 0x28) = *(const uint32_t *)colour;
	}

	static uint32_t __cdecl BoltTask(TaskNode *n)
	{
		BoltNode *p = (BoltNode *)n;
		BoltArg arg = {};
		GetEffectSpawnPosition(p->entity, 0xF1, 0, arg.pos);
		arg.pos[1] = *(int16_t *)(p->entity + 0x24);
		arg.scale[2] = p->scale;
		arg.scale[1] = p->scale;
		arg.scale[0] = p->scale;
		arg.morph = TexBase() + 0x2254;
		arg.quad_parts = BOLT_QuadParts;
		// 30 fps layer: see mag105_thundaga_held.inc
		FX_HELD(held_note_bolt(p, &arg);)
		prim::play((prim::Layout *)p->layout, BoltPart, (int)&arg, 0);
		TargetFlash(p->entity, (const uint8_t *)&p->colour, p->counter, 2);
		TargetFlash(p->entity, (const uint8_t *)&p->colour, p->counter, 0x1E);
		TargetFlash(p->entity, (const uint8_t *)&p->colour, p->counter, 0x27);
		// screen flash: ramps up over 0..3, holds 0x800, down from 61
		int16_t c = p->counter;
		if (c < 4 || c > 0x3C)
		{
			int32_t level = c < 4 ? c : 0x40 - c;
			level <<= 9;
			if (level > FlashLevel()) FlashLevel() = level;
		}
		else if (FlashLevel() < 0x800) FlashLevel() = 0x800;
		if (p->counter == 0)
		{
			int16_t q[4];
			GetDefaultEffectPosition(p->entity, q);
			BdPlaySE3D(SOUND_Thundaga, 0x101, q);
		}
		if (p->counter == 3) ScreenFadeTask(0, 1, 0, 0x80);
		else if (p->counter == 0x1E) ScreenFadeTask(1, 1, 6, 0x80);
		else if (p->counter == 0x27) ScreenFadeTask(0, 1, 0, 0x50);
		if (p->counter == 0x1C)
		{
			int16_t all[6] = { 0x7FFF, 0x7FFF, 0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF };
			int16_t s = (int16_t)p->scale;
			Mat4x3 m = {};
			m.m[2][2] = s;
			m.m[1][1] = s;
			m.m[0][0] = s;
			GteSetRotMatrix(&m);
			GteSetTransVector(&m);
			for (int k = 0; k < 14; k++) ShardSpawn(arg.pos, (uint8_t *)SHARD_MESH[k][0], (const uint8_t *)SHARD_MESH[k][1], all);
			SetScreenFlash(0, 0);
			// 30 fps layer: see mag105_thundaga_held.inc
			FX_HELD(held_note_capture(arg.pos, all);)
			CapturePrims prims = { PRIM_Env, PRIM_Area, PRIM_Offset, RECT_Offset, Arena2() };
			CaptureDraw(arg.pos, all, prims);
		}
		if ((uint32_t)(p->counter - 0x22) < 8 && !((p->counter - 0x22) & 1))
		{
			// a ring of 8 sparks at the target's height, thrown outwards, decelerating by 1/10
			int32_t a = CrtRand();
			for (int k = 8; k; k--, a += 0x200)
			{
				SpriteNode *s = (SpriteNode *)AddTaskToQueue(&SparkQueue(), ORIG_SparkTask);
				if (!s) continue;
				FillDwords(&s->counter, 0, 8);
				s->seq = SEQ_Spark;
				s->pos[0] = (int16_t)ComputeCos(a);
				int16_t sn = (int16_t)ComputeSin(a);
				s->pos[2] = sn;
				s->pos[1] = arg.pos[1];
				s->vel[0] = Sar16(s->pos[0], 8);
				s->vel[2] = Sar16(sn, 8);
				s->vel[1] = (int16_t)(-0x100 - (CrtRand() & 0x7F));
				s->acc[0] = DivNeg10(s->vel[0]);
				s->acc[1] = DivNeg10(s->vel[1]);
				s->acc[2] = DivNeg10(s->vel[2]);
				s->pos[0] = (int16_t)(Sar16(s->pos[0], 2) + arg.pos[0]);
				s->pos[2] = (int16_t)(Sar16(s->pos[2], 2) + arg.pos[2]);
				s->size = 0x1000;
				s->end = 0xC;
			}
		}
		if ((uint32_t)(p->counter - 0x2D) < 0xC && !((p->counter - 0x2D) & 1))
		{
			// a flash sprite somewhere in 0x800 around the target's centre
			int16_t q[4];
			GetDefaultEffectPosition(p->entity, q);
			SpriteNode *f = (SpriteNode *)AddTaskToQueue(&SparkQueue(), ORIG_FlashTask);
			if (f)
			{
				FillDwords(&f->counter, 0, 8);
				f->seq = SEQ_Flash;
				f->pos[0] = (int16_t)((CrtRand() & 0x7FF) + q[0] - 0x400);
				f->pos[1] = (int16_t)((CrtRand() & 0x7FF) + q[1] - 0x400);
				int32_t rz = CrtRand() & 0x7FF;
				f->size = 0x1000;
				f->end = 0xF;
				f->pos[2] = (int16_t)(rz + q[2] - 0x400);
			}
		}
		if (p->counter >= 0x40)
		{
			ApplyActionResultToTarget(Ctx()->actions[p->action].targets);
			return TASK_END;
		}
		p->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Shard (0x6D5AD0)
	// ------------------------------------------------------------------
	static void ShardDraw(const ShardNode *p)
	{
		Mat4x3 m = {};
		BuildRotationMatrixFromAngles(p->ang, &m);
		m.t[0] = p->pos[0];
		m.t[1] = p->pos[1];
		m.t[2] = p->pos[2];
		ComposeAffineTransform(&Camera(), &m, &m);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint32_t *)(h + 0x1C) = 0;
		*(uint8_t **)h = p->mesh;
		PacketCursor() = RenderPrimModel(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// position += velocity / 16, velocity += acceleration, angles += spin
	static void ShardUpdate(ShardNode *p)
	{
		int16_t vx = p->vel[0], vy = p->vel[1];
		p->pos[0] = (int16_t)(p->pos[0] + Sar16(vx, 4));
		p->pos[1] = (int16_t)(p->pos[1] + Sar16(vy, 4));
		int16_t vz = p->vel[2];
		p->pos[2] = (int16_t)(p->pos[2] + Sar16(vz, 4));
		int16_t nx = (int16_t)(p->acc[0] + vx);
		int16_t ny = (int16_t)(p->acc[1] + vy);
		int16_t nz = (int16_t)(p->acc[2] + vz);
		p->ang[0] = (int16_t)(p->ang[0] + p->angvel[0]);
		p->vel[1] = ny;
		p->vel[2] = nz;
		p->ang[1] = (int16_t)(p->ang[1] + p->angvel[1]);
		p->ang[2] = (int16_t)(p->ang[2] + p->angvel[2]);
		p->vel[0] = nx;
	}

	static uint32_t __cdecl ShardTask(TaskNode *n)
	{
		ShardNode *p = (ShardNode *)n;
		// 30 fps layer: see mag105_thundaga_held.inc
		FX_HELD(held_note_draw(ORIG_ShardTask, p, sizeof(ShardNode));)
		ShardDraw(p);
		p->life--;
		if (p->life == 0)
		{
			ShardBreak(p->pos, p->vel, p->acc, p->ang, p->mesh);
			return TASK_END;
		}
		ShardUpdate(p);
		return 0;
	}

	// ------------------------------------------------------------------
	// Spark (0x6D6100) and flash (0x6D61C0): sprite sequence `seq` at frame `counter`
	// ------------------------------------------------------------------
	static void SpriteDraw(const SpriteNode *p)
	{
		TransformCameraByShadowRotation(p->pos, p->size, -((int32_t)p->size >> 2));
		uint8_t *h = AllocHeader(0xB4);
		*(uint32_t *)h = p->seq;
		*(uint16_t *)(h + 4) = (uint16_t)p->counter;
		*(uint16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT44(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// position += velocity, velocity += acceleration (the counter steps in between)
	static void SparkUpdate(SpriteNode *p)
	{
		int16_t vx = p->vel[0], vy = p->vel[1];
		p->pos[0] = (int16_t)(p->pos[0] + vx);
		int16_t nx = (int16_t)(p->acc[0] + vx);
		p->pos[1] = (int16_t)(p->pos[1] + vy);
		int16_t vz = p->vel[2];
		p->pos[2] = (int16_t)(p->pos[2] + vz);
		int16_t ny = (int16_t)(p->acc[1] + vy);
		int16_t nz = (int16_t)(p->acc[2] + vz);
		p->counter++;
		p->vel[1] = ny;
		p->vel[0] = nx;
		p->vel[2] = nz;
	}

	static uint32_t __cdecl SparkTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag105_thundaga_held.inc
		FX_HELD(held_note_draw(ORIG_SparkTask, p, sizeof(SpriteNode));)
		SpriteDraw(p);
		SparkUpdate(p);
		return p->counter < p->end ? 0 : TASK_END;
	}

	static uint32_t __cdecl FlashTask(TaskNode *n)
	{
		SpriteNode *p = (SpriteNode *)n;
		// 30 fps layer: see mag105_thundaga_held.inc
		FX_HELD(held_note_draw(ORIG_FlashTask, p, sizeof(SpriteNode));)
		SpriteDraw(p);
		p->counter++;
		return p->counter < p->end ? 0 : TASK_END;
	}
}

	void register_mag105_thundaga()
	{
		register_port(thundaga105::ORIG_RootTask, (void *)thundaga105::RootTask, "T105 RootTask", 105);
		register_port(thundaga105::ORIG_WaitTexRestore, (void *)thundaga105::WaitTexRestore, "T105 WaitTexRestore", 105);
		register_port(thundaga105::ORIG_BoltTask, (void *)thundaga105::BoltTask, "T105 BoltTask", 105);
		register_port(thundaga105::ORIG_ShardTask, (void *)thundaga105::ShardTask, "T105 ShardTask", 105);
		register_port(thundaga105::ORIG_SparkTask, (void *)thundaga105::SparkTask, "T105 SparkTask", 105);
		register_port(thundaga105::ORIG_FlashTask, (void *)thundaga105::FlashTask, "T105 FlashTask", 105);
		// 30 fps layer: see mag105_thundaga_held.inc
		FX_HELD(register_mag105_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag105_thundaga_held.inc"
#endif
