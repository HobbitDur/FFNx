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

// Effect 308: Mobile Type 8 and its two probes taken apart by bone scripts (enemy attack 286 of kernel.bin, no name; Mobile
// Type 8 c0m105, whose next two entity slots hold its probes; no other kernel.bin entry uses effect 308; MAG_308_*).
//
// Structure (setup 0x667670 -> _Init 0x6676A0, file loader 0x667680 = mag307.tim (name at 0x102E940) into 0x25074D0; _Init
// queues that TIM, keeps the context (0x25074D4) and the CASTER's entity (0x25074D8), clears its entity flags 0x0C, sets up
// the root queue 0x2507520 (pool 0x2507530, 1 x 0x14) and starts camera animation 0x10257E8):
//   RootTask (0x667720) - alternates the module packet arena (magic buffer + 0x2A18 / + 0x12A18); on counter 1 (once): music
//     volume fade 0x47E3C0(0xF0), the pools (main 1 x 0xE94 at + 0x1B3C, layouts 8 x 0x190 at + 0xEBC, sprites 0x40 x 0x1C
//     at + 0x7BC, bone scripts 0x2D x 0x2C at the magic buffer), the main task (voice slot 0x102590C, the three entities'
//     colour words +0x2C kept, particle cursor), one bone script per entity (0x102E5A0 / 0x102E734 / 0x102E834 on the main's
//     three pose arrays) and the stream state (+ 0x29D0). Then each tick: main, scripts, layouts, sprites; the stream pump;
//     when the main queue is empty and the streams are done: screen flash 0, end.
//   MainTask (0x668DE0) - counters 0..0x6D: screen flash ramps 0..7 / 0x67..0x6D (2300 / 8 a step), 2300 between;
//     counters 0..0x13 the three colour words lerped toward 0x250754C (0, never written), byte +0x2F = 0x32; counters
//     0..0x55 each entity drawn with its pose offsets (0x669070) and its flag 4; counters 0x46..0x55 a white tile rising
//     (0x7040B0), on 0x55 the script / sprite / layout queues dropped; counters 0x56..0x65 the tile fading; before 0x56 the
//     particles (0x669620: draw THEN move); sound 0x1025908 at 0, voice released at 0x6A, stream loads 0x168 / 0x169 (+ 0x2A18)
//     at 0x67; at 0x6E flags 8 off, colour words back, end.
//   BoneScriptTask (0x667990) - node 0x2C: byte code moving one pose entry (set / add translation, velocity, rotation, angular
//     velocity, a step with a floor at y 0, copy entries, sparks from the model (0x1090: 0x667DB0), a layout at a bone
//     (0x10A0 / 0x10A1), one sprite (0x10A8) or a random burst (0x10A9) at a bone, a flat full-screen quad (0x10B0),
//     counted loops (0x10F0), end (0x1100); any other word yields until the next tick.
//   LayoutTask (0x6685E0) - a prim-model layout (table 0x102A400) at its position turned by +0x14, scaled by +0x12, with the
//     callback 0x668680 (vertex frames blended into magic buffer + 0x22A18); ends with the layout.
//   SpriteTask (0x668A50) - sequence 0x10262F8[+0x15] at its position, draws THEN moves (4 x velocity, velocity += acceleration,
//     y twice); ends after the last frame.
// Sparks (0x667DB0): the listed bones of the entity are hidden (pose entry +6 = 2) and marked (bone matrix pad word = 1); the
// marked bones' polygons throw particles along their normals (module LCG 0x2507548, shared LCG 0x7059E0) until the budget
// is spent.
// Draw-only flags (battle_to_update_flags 0x201) are not tested: everything updates always.
// ENTITY WRITES: the three entities - flags words +0x00 (bit 2 while drawn, bit 3 cleared at the end), colour words +0x2C /
// +0x2F, their bone matrices (and their pad words) every tick.
// CAMERA: the setup's camera animation 0x10257E8 (engine); no camera word is written by the tasks.
// Module globals: 0x25074D0..0x2507558 (texture file 0x25074D0, context 0x25074D4, caster 0x25074D8, script queue 0x25074E0,
// sprite queue 0x25074F0, layout queue 0x2507500, main queue 0x2507510, root queue 0x2507520, root pool 0x2507530, packet
// cursor 0x2507544, module LCG 0x2507548, colour lerp target 0x250754C, magic buffer base 0x2507550).
// CODE OF OTHER MODULES: 314's 0x662C00 (pose offsets), 276's 0x687AE0 (bone group in world space) and 290's 0x676E00 (bone
// group projection) are called at their addresses (as Griever's 064 does); 298's sprite renderer 0x672DB0 is ported in
// fx_model_sweep.h. 0x6684C0 / 0x668970 / 0x669070 / 0x6691B0 / 0x6696B0 are the same code as 298's 0x671990 / 0x671E40 /
// 0x672330 / 0x672470 / 0x672CD0 (and 290's 0x676B40 / 0x676C80).
// NEVER-WRITTEN STACK WORDS: none read.

#include "mag_common.h"
#include "fx_params.h"
#include "fx_model_sweep.h"

namespace ff8fx
{
namespace mobile308
{
	using namespace eng;
	using namespace magc;
	using namespace msweep;

	// --- module globals ---
	inline uint32_t &TexFile() { return var<uint32_t>(0x25074D0); }        // the effect's file (its TIM)
	inline CastContext *&Ctx() { return var<CastContext *>(0x25074D4); }
	inline uint8_t *&Caster() { return var<uint8_t *>(0x25074D8); }        // caster entity (setup); probes at + 0x9C / + 0x138
	inline TaskQueue *QScripts() { return (TaskQueue *)0x25074E0; }        // pool: magic buffer, 0x2D x 0x2C
	inline TaskQueue *QSprites() { return (TaskQueue *)0x25074F0; }        // pool: + 0x7BC, 0x40 x 0x1C
	inline TaskQueue *QLayouts() { return (TaskQueue *)0x2507500; }        // pool: + 0xEBC, 8 x 0x190
	inline TaskQueue *QMain() { return (TaskQueue *)0x2507510; }           // pool: + 0x1B3C, 1 x 0xE94
	inline TaskQueue *QRoot() { return (TaskQueue *)0x2507520; }           // pool 0x2507530, 1 x 0x14
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2507544); }
	inline uint32_t &Lcg() { return var<uint32_t>(0x2507548); }
	inline uint32_t &TexBase() { return var<uint32_t>(0x2507550); }        // Magic_TextureOFF (magic buffer)

	static const uint32_t ORIG_Setup = 0x667670, ORIG_FileLoader = 0x667680;
	static const uint32_t ORIG_RootTask = 0x667720;
	static const uint32_t ORIG_MainTask = 0x668DE0;
	static const uint32_t ORIG_ScriptTask = 0x667990;
	static const uint32_t ORIG_LayoutTask = 0x6685E0;
	static const uint32_t ORIG_SpriteTask = 0x668A50;
	static const uint32_t ORIG_PartCallback = 0x668680;
	static const uint32_t TEXTURE_NAME = 0x102E940;    // "mag307.tim"
	static const uint32_t CAMERA = 0x10257E8;
	static const uint32_t LAYOUTS = 0x102A400;         // 8-byte entries: data, size
	static const uint32_t SPRITES = 0x10262F8;         // sprite sequence pointers
	static const uint32_t PART_SEQ0 = 0x1026038, PART_SEQ1 = 0x1025D20; // particle sequences (by +0x0B)
	static const uint32_t SQRT_TABLE = 0x102E3A0;
	static const uint32_t COLOUR_Target = 0x250754C;   // colour lerp target (never written: 0)
	static const void *const SOUND_Voice = (const void *)0x102590C;
	static const void *const SOUND_Start = (const void *)0x1025908;
	static const uint32_t SCRIPTS[3] = { 0x102E5A0, 0x102E734, 0x102E834 };
	static const uint32_t MAIN = 0x1B3C;               // magic buffer: the main node
	static const uint32_t MORPH = 0x22A18;             // magic buffer: blended vertex frames
	static const uint32_t STREAM_STATE = 0x29D0, STREAM_DST = 0x2A18;
	static const uint32_t ARENA_A = 0x2A18, ARENA_B = 0x12A18;

	// engine functions not in fx_port.h / mag_common.h / fx_model_sweep.h
	inline void StreamLoad(int32_t id, uint32_t dst, int32_t kind) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, kind); } // Stream_QueueLoad
	inline void StreamPump() { fn<void (__cdecl *)()>(0x534210)(); }
	inline int32_t StreamPending(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x5342C0)(a); }                    // au_re_BattleFile_preLoad_0
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, void *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, void *)>(0x502170)(entity, bone, flag, out); }
	inline void InsertPrimAltViewport(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(bucket, packet); }
	inline uint32_t Tile(int32_t r, int32_t g, int32_t b, int32_t level, uint32_t cursor) { return fn<uint32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t, uint32_t)>(0x7040B0)(r, g, b, level, cursor); } // Effect_FullScreenTile
	inline void PoseBones(uint8_t *anim, const void *pose) { fn<void (__cdecl *)(uint8_t *, const void *)>(0x662C00)(anim, pose); }        // 314's Battle_BuildBoneMatricesWithPoseOffsets
	inline void WorldGroup(const uint8_t **data, uint8_t **out, uint8_t *h) { fn<void (__cdecl *)(const uint8_t **, uint8_t **, uint8_t *)>(0x687AE0)(data, out, h); } // MAG_276_TransformBoneGroupWorld
	inline void ProjectGroup(const uint8_t **data, uint8_t **out, uint8_t *h, int32_t key) { fn<void (__cdecl *)(const uint8_t **, uint8_t **, uint8_t *, int32_t)>(0x676E00)(data, out, h, key); } // Battle_ProjectBoneGroupVertices
	inline int32_t SharedRand() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }                                      // Effect_SharedLcg_Rand
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }        // Effect_RotationYMatrix
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAnglesYXZ(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); } // MAG_063_RotationFromAnglesYXZ
	inline void BlendVertexFrames(uint32_t obj, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(obj, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }        // GTE_MatrixMultiply: b = a x b
	inline void MatrixMultiply3x3(Mat4x3 *a, const void *b) { fn<void (__cdecl *)(Mat4x3 *, const void *)>(0x56C220)(a, b); }        // a = a x b (3x3)
	inline void Matrix3x3Multiply(const void *a, const void *b, void *out) { fn<void (__cdecl *)(const void *, const void *, void *)>(0x56C090)(a, b, out); } // Matrix3x3_MultiplyPSX
	inline uint32_t RenderPrimModel2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_RenderPrimModel
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }                                                // IR = R * V0

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes (0x2507530)
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
		int16_t vis;       // +0x06 >= 2 hidden; low byte = colour key
		int16_t rot[3];    // +0x08 rotation added to the animation pose (0x662C00)
		int16_t spin;      // +0x0E sparks: non-zero = the bone's world matrix turned by rot again
	};
	struct Particle // 12 bytes
	{
		int16_t pos[3];    // +0x00
		int16_t life;      // +0x06 ticks left (0 = free); frame = frame count - life
		int8_t vel[3];     // +0x08 x 4 a tick
		uint8_t seq;       // +0x0B 0: 0x1026038, else 0x1025D20
	};
	struct MainNode // pool of 1 node of 0xE94 bytes (magic buffer + 0x1B3C)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
		uint32_t voice;    // +0x10 voice slot (0x4A29A0)
		uint32_t colour[3]; // +0x14 the entities' colour words +0x2C at the start
		PoseEntry pose[3][0x2D]; // +0x20 / +0x2F0 / +0x5C0
		Particle parts[0x80];    // +0x890
		Particle *next;    // +0xE90
	};
	struct ScriptNode // pool of 0x2D nodes of 0x2C bytes
	{
		TaskNode hdr;
		uint8_t *entity;   // +0x0C
		PoseEntry *pose;   // +0x10 the entity's pose array
		PoseEntry *cur;    // +0x14 the entry this script moves
		const int16_t *ip; // +0x18
		int16_t vel[3];    // +0x1C
		int16_t pad22;
		int16_t avel[3];   // +0x24
		int16_t pad2A;
	};
	struct LayoutNode // pool of 8 nodes of 0x190 bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t scale;     // +0x12
		int16_t turn;      // +0x14 y rotation
		int16_t depth;     // +0x16
		uint8_t layout[0x178]; // +0x18 prim-model layout
	};
	struct SpriteNode // pool of 0x40 nodes of 0x1C bytes
	{
		TaskNode hdr;
		int16_t pos[3];    // +0x0C
		int16_t size;      // +0x12 (TransformCameraByShadowRotation)
		uint8_t frame;     // +0x14
		uint8_t seq;       // +0x15 sequence index (0x10262F8)
		int8_t vel[3];     // +0x16 (x 4 a tick)
		int8_t acc[3];     // +0x19 x, y (y twice), - (not read)
	};
	// the prim-player callback's parameter block (a stack block of the layout task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 anchor matrix (camera * turn, at the node's position)
		int32_t scale[3];  // +0x20 the node's scale, x / y / z
		int32_t scaled;    // +0x2C 1
		int32_t depth;     // +0x30 prim header +0x18
		uint32_t morph;    // +0x34 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(PoseEntry) == 0x10 && sizeof(Particle) == 0xC && sizeof(MainNode) == 0xE94, "308 nodes");
	static_assert(sizeof(ScriptNode) == 0x2C && sizeof(LayoutNode) == 0x190 && sizeof(SpriteNode) == 0x1C && sizeof(PrimArg) == 0x38, "308 nodes");
}
}

#ifdef FF8_FX_HELD
#include "mag308_mobile_type8_held.h"
#endif

namespace ff8fx
{
namespace mobile308
{
	using namespace eng;
	using namespace magc;
	using namespace msweep;

	// ------------------------------------------------------------------
	// Module LCG (0x668450): seed = (seed * 125 + 14) & 0x7FFF
	// ------------------------------------------------------------------
	static int32_t Rand15()
	{
		Lcg() = (Lcg() * 125 + 0xE) & 0x7FFF;
		return (int32_t)Lcg();
	}

	// particle slot (0x668470): the slot the cursor names when free, else the first free one (slots 0..0x7F; none: null);
	// the cursor goes to the slot after it (wrapping)
	static Particle *AllocParticle(MainNode *m)
	{
		Particle *s = m->next;
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
		m->next = s < &m->parts[0x7F] ? s + 1 : &m->parts[0];
		return s;
	}

	// ------------------------------------------------------------------
	// A spark from a polygon (0x668230): along the normal of its first three vertices (GTE outer product), length
	// +0x1C (normalised with the square-root table 0x102E3A0); 3 in 4 (module LCG > 0x2EE0): sequence 0, life 0xC, a
	// quarter of the speed; else sequence 1, life 0xB
	// ------------------------------------------------------------------
	static void Spark(uint8_t *h)
	{
		const int32_t x0 = F<int16_t>(h, 0x2C), y0 = F<int16_t>(h, 0x2E), z0 = F<int16_t>(h, 0x30);
		F<int32_t>(h, 0x5C) = x0 - F<int16_t>(h, 0x34);
		F<int32_t>(h, 0x60) = y0 - F<int16_t>(h, 0x36);
		F<int32_t>(h, 0x64) = z0 - F<int16_t>(h, 0x38);
		F<int32_t>(h, 0x6C) = F<int16_t>(h, 0x3C) - x0;
		F<int32_t>(h, 0x70) = F<int16_t>(h, 0x3E) - y0;
		F<int32_t>(h, 0x74) = F<int16_t>(h, 0x40) - z0;
		GteSetRotDiagonal((const int32_t *)(h + 0x5C));
		GteLoadIR123_32((const int32_t *)(h + 0x6C));
		GteOP();
		int32_t *n = (int32_t *)(h + 0x7C);
		GteReadMAC123(n);
		int32_t big = n[0] < 0 ? -n[0] : n[0];
		const int32_t ay = n[1] < 0 ? -n[1] : n[1];
		if (ay > big) big = ay;
		const int32_t az = n[2] < 0 ? -n[2] : n[2];
		if (az > big) big = az;
		GteSetLZCS(big);
		int32_t lz;
		GteReadLZCR(&lz);
		if (lz < 0x12)
		{
			const int32_t sh = 0x12 - lz;
			n[0] >>= sh;
			n[1] >>= sh;
			n[2] >>= sh;
		}
		GteLoadIR123_32b(n);
		GteSQR();
		int32_t len2;
		GteSumMAC123(&len2);
		GteSetLZCS(len2);
		GteReadLZCR(&lz);
		const int32_t e = lz & ~1;
		const int32_t up = (0x1F - e) >> 1;
		const int32_t idx = e - 0x18 > 0 ? shl32(len2, e - 0x18) : len2 >> (0x18 - e);
		const int32_t len = shl32((int32_t)var<uint16_t>(SQRT_TABLE + idx * 2), up) >> 0xC;
		if (len <= 0) return;
		const int32_t speed = F<int32_t>(h, 0x1C);
		const int16_t vx = (int16_t)(mul32(n[0], speed) / len);
		const int16_t vz = (int16_t)(mul32(speed, n[2]) / len);
		const int16_t vy = (int16_t)(mul32(n[1], speed) / len);
		Particle *p = AllocParticle(F<MainNode *>(h, 0x28));
		if (!p) return;
		*(uint32_t *)&p->pos[0] = F<uint32_t>(h, 0x2C);
		*(uint32_t *)&p->pos[2] = F<uint32_t>(h, 0x30);
		if (Rand15() > 0x2EE0)
		{
			p->seq = 0;
			p->life = 0xC;
			p->vel[0] = (int8_t)(vx >> 2);
			p->vel[1] = (int8_t)(vy >> 2);
			p->vel[2] = (int8_t)(vz >> 2);
			return;
		}
		p->seq = 1;
		p->life = 0xB;
		p->vel[0] = (int8_t)vx;
		p->vel[1] = (int8_t)vy;
		p->vel[2] = (int8_t)vz;
	}

	// ------------------------------------------------------------------
	// Spark polygons (0x6680A0): every polygon of the object whose vertices all come from marked bones: after a cool-down of
	// 8 polygons, one passing the shared LCG (<= +0x24) throws a spark while the budget (+0x14) lasts (spent: the walk ends)
	// ------------------------------------------------------------------
	static void SparkPolys(uint8_t *h)
	{
		const uint8_t *verts = F<const uint8_t *>(h, 4);
		const uint8_t *p = F<const uint8_t *>(h, 0);
		int16_t c = F<int16_t>(h, 8);
		F<int16_t>(h, 8) = (int16_t)(c - 1);
		while (c != 0)
		{
			F<uint32_t>(h, 0x2C) = *(const uint32_t *)(verts + (*(const uint16_t *)p & 0xFFF) * 8);
			F<uint32_t>(h, 0x30) = *(const uint32_t *)(verts + (*(const uint16_t *)p & 0xFFF) * 8 + 4);
			F<uint32_t>(h, 0x34) = *(const uint32_t *)(verts + (*(const uint16_t *)(p + 2) & 0xFFF) * 8);
			F<uint32_t>(h, 0x38) = *(const uint32_t *)(verts + (*(const uint16_t *)(p + 2) & 0xFFF) * 8 + 4);
			F<uint32_t>(h, 0x3C) = *(const uint32_t *)(verts + (*(const uint16_t *)(p + 4) & 0xFFF) * 8);
			F<uint32_t>(h, 0x40) = *(const uint32_t *)(verts + (*(const uint16_t *)(p + 4) & 0xFFF) * 8 + 4);
			if ((uint8_t)(h[0x3A] | h[0x42] | h[0x32]) == 0)
			{
				if (F<int32_t>(h, 0x18) == 0)
				{
					if (SharedRand() <= F<int32_t>(h, 0x24))
					{
						const int32_t b = F<int32_t>(h, 0x14);
						F<int32_t>(h, 0x14) = b - 1;
						if (b <= 0) return;
						F<int32_t>(h, 0x18) = 8;
						Spark(h);
						F<int32_t>(h, 0x18)--;
					}
				}
				else F<int32_t>(h, 0x18)--;
			}
			p += 0x10;
			c = F<int16_t>(h, 8);
			F<int16_t>(h, 8) = (int16_t)(c - 1);
		}
		c = F<int16_t>(h, 0xA);
		F<int16_t>(h, 0xA) = (int16_t)(c - 1);
		while (c != 0)
		{
			F<uint32_t>(h, 0x2C) = *(const uint32_t *)(verts + (*(const uint16_t *)p & 0xFFF) * 8);
			F<uint32_t>(h, 0x30) = *(const uint32_t *)(verts + (*(const uint16_t *)p & 0xFFF) * 8 + 4);
			F<uint32_t>(h, 0x34) = *(const uint32_t *)(verts + (*(const uint16_t *)(p + 2) & 0xFFF) * 8);
			F<uint32_t>(h, 0x38) = *(const uint32_t *)(verts + (*(const uint16_t *)(p + 2) & 0xFFF) * 8 + 4);
			F<uint32_t>(h, 0x3C) = *(const uint32_t *)(verts + (*(const uint16_t *)(p + 4) & 0xFFF) * 8);
			F<uint32_t>(h, 0x40) = *(const uint32_t *)(verts + (*(const uint16_t *)(p + 4) & 0xFFF) * 8 + 4);
			F<uint32_t>(h, 0x44) = *(const uint32_t *)(verts + (*(const uint16_t *)(p + 6) & 0xFFF) * 8);
			F<uint32_t>(h, 0x48) = *(const uint32_t *)(verts + (*(const uint16_t *)(p + 6) & 0xFFF) * 8 + 4);
			if ((uint8_t)(h[0x3A] | h[0x42] | h[0x32] | h[0x4A]) == 0)
			{
				if (F<int32_t>(h, 0x18) == 0)
				{
					if (SharedRand() <= F<int32_t>(h, 0x24))
					{
						const int32_t b = F<int32_t>(h, 0x14);
						F<int32_t>(h, 0x14) = b - 1;
						if (b <= 0) return;
						F<int32_t>(h, 0x18) = 8;
						Spark(h);
						F<int32_t>(h, 0x18)--;
					}
				}
				else F<int32_t>(h, 0x18)--;
			}
			p += 0x14;
			c = F<int16_t>(h, 0xA);
			F<int16_t>(h, 0xA) = (int16_t)(c - 1);
		}
	}

	// spark model walk (0x667F40): every object in the mask whose groups include a marked bone (pad word +0x22 of its
	// record) - the marked groups in world space (276's 0x687AE0), the others' entries keyed 1 - then its polygons; an
	// object is walked again while the budget lasts
	static void SparkModel(const uint8_t *model, uint8_t *h)
	{
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const uint32_t *objs = *(const uint32_t *const *)(model + 4);
		const int32_t count = (int32_t)objs[0];
		for (int32_t i = 0; i < count; i++)
		{
			const uint8_t *obj = *(const uint8_t *const *)(model + 4) + objs[1 + i];
			if (!(F<uint32_t>(h, 0x10) & (1u << (i & 31)))) continue;
			for (;;)
			{
				uint8_t *out = F<uint8_t *>(h, 4);
				bool marked = false;
				const uint8_t *p = obj;
				const int32_t groups = *(const int16_t *)p;
				p += 2;
				if (groups <= 0) break;
				for (int32_t g = groups; g != 0; g--)
				{
					const int32_t bone = *(const int16_t *)p;
					p += 2;
					const uint8_t *rec = bones + mul32(bone, 0x30);
					if (F<int16_t>(rec, 0x22) != 0)
					{
						GteSetRotMatrixCtrl((const Mat4x3 *)(rec + 0x10));
						GteSetTransVectorCtrl((const Mat4x3 *)(rec + 0x10));
						WorldGroup(&p, &out, h);
						marked = true;
					}
					else
					{
						const int32_t n = *(const int16_t *)p;
						p = p + n * 6 + 2;
						for (int32_t k = n; k != 0; k--)
						{
							*(int16_t *)(out + 6) = 1;
							out += 8;
						}
					}
				}
				if (!marked) break;
				p = (const uint8_t *)(((uint32_t)p + 3) & ~3u);
				F<uint16_t>(h, 8) = *(const uint16_t *)p;
				F<uint16_t>(h, 0xA) = *(const uint16_t *)(p + 2);
				F<const uint8_t *>(h, 0) = p + 0xC;
				SparkPolys(h);
				if (F<int32_t>(h, 0x14) <= 0) return;
			}
		}
	}

	// ------------------------------------------------------------------
	// Sparks (0x667DB0, opcode 0x1090): bones from the pose plus the offsets, through the entity matrix (an entry with +0xE
	// set turned by its rotation again), plus the translations; the listed bones (until -1) hidden and marked, then budget,
	// speed, cool-down unused, probability, range; the marked polygons throw sparks; the bones from the pose again
	// ------------------------------------------------------------------
	static const int16_t *Sparks(const int16_t *ip, uint8_t *ent, PoseEntry *pose)
	{
		uint8_t *b = (uint8_t *)FieldAlloc(0xAC);
		F<int32_t>(b, 0x38) = 0;
		F<uint32_t>(b, 0x48) = TexBase() + MAIN;
		uint8_t *anim = ent + 0x60;
		F<uint32_t>(b, 0x24) = var<uint32_t>(0x1D98B3C);
		F<uint32_t>(b, 0x2C) = F<uint32_t>(ent, 0x28);
		F<uint32_t>(b, 0x30) = F<uint32_t>(ent, 0x7C);
		PoseBones(anim, pose);
		uint8_t *skel = **(uint8_t ***)(anim + 4);
		for (uint32_t i = 0; i < skel[0]; i++)
		{
			Mat4x3 *bm = (Mat4x3 *)(skel + 0x20 + i * 0x30);
			ComposeAffineTransform((const Mat4x3 *)(ent + 0x40), bm, bm);
			if (pose[i].spin != 0)
			{
				BuildRotationMatrixFromAngles(pose[i].rot, (Mat4x3 *)b);
				Matrix3x3Multiply(b, bm, bm);
			}
			bm->t[0] = (int32_t)((uint32_t)bm->t[0] + (uint32_t)(int32_t)pose[i].pos[0]);
			bm->t[1] = (int32_t)((uint32_t)bm->t[1] + (uint32_t)(int32_t)pose[i].pos[1]);
			bm->t[2] = (int32_t)((uint32_t)bm->t[2] + (uint32_t)(int32_t)pose[i].pos[2]);
		}
		for (uint32_t i = 0; i < skel[0]; i++) F<int16_t>(skel, 0x32 + i * 0x30) = 0;
		int32_t bone = *ip++;
		while (bone != -1)
		{
			F<int16_t>(skel, 0x32 + mul32(bone, 0x30)) = 1;
			pose[bone].vis = 2;
			bone = *ip++;
		}
		F<int32_t>(b, 0x34) = ip[0];
		uint8_t *h = b + 0x20;
		F<int32_t>(h, 0x1C) = ip[1];
		F<int32_t>(h, 0x20) = ip[2];
		F<int32_t>(h, 0x24) = ip[3];
		SparkModel(F<const uint8_t *>(anim, 4), h);
		BuildBoneMatricesFromPose(anim);
		FieldFree(0xAC);
		return ip + 4;
	}

	// ------------------------------------------------------------------
	// Prim-player callback (0x668680): one object of a layout (vertex frames blended when the record's frames differ),
	// turned by the record's angles (flag 0x40000: YXZ), at its position (scaled by the block's scale) through the block's
	// matrix (flag 0x200: the block's translation only), scaled by the record's scale (flag 0x100: a diagonal matrix
	// product), drawn by 0x7043B0 (channel A: fade toward the record's colour)
	// ------------------------------------------------------------------
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		uint8_t *rr = (uint8_t *)r;
		PrimArg *arg = (PrimArg *)arg_;
		if (((int32_t)F<int16_t>(rr, 0x1C) | F<int32_t>(rr, 0x18)) == 0) return;
		if (F<int16_t>(rr, 0x24) >= 0x1000 && F<uint32_t>(rr, 0x20) == 0) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		uint8_t *obj = l->data + *(const uint32_t *)(l->data + (int32_t)F<int16_t>(rr, 2) * 4 + 8);
		F<uint8_t *>(h, 0) = obj;
		const int16_t f0 = F<int16_t>(rr, 0x28), f1 = F<int16_t>(rr, 0x2A);
		auto frame = [&](int16_t f) { return f == 0 ? obj + 0xC : obj + mul32(*(const int32_t *)(obj + 4), f) * 8 + 0xC; };
		if (f0 == f1) F<uint8_t *>(h, 4) = frame(f0);
		else
		{
			const int16_t t = F<int16_t>(rr, 0x26);
			if (t == 0) F<uint8_t *>(h, 4) = frame(f0);
			else if (t == 0x1000) F<uint8_t *>(h, 4) = frame(f1);
			else
			{
				BlendVertexFrames((uint32_t)obj, f0, f1, t, arg->morph);
				F<uint32_t>(h, 4) = arg->morph;
			}
		}
		Mat4x3 m;
		if (F<uint32_t>(rr, 4) & 0x40000) RotationFromAnglesYXZ((const int16_t *)(rr + 0x10), &m);
		else RotationFromAngles((const int16_t *)(rr + 0x10), &m);
		int16_t v[3];
		if (arg->scaled)
		{
			v[0] = (int16_t)(mul32(F<int16_t>(rr, 8), arg->scale[0]) >> 0xC);
			v[1] = (int16_t)(mul32(F<int16_t>(rr, 0xA), arg->scale[1]) >> 0xC);
			v[2] = (int16_t)(mul32(F<int16_t>(rr, 0xC), arg->scale[2]) >> 0xC);
			Scale3DMatrix(&m, arg->scale);
		}
		else
		{
			v[0] = F<int16_t>(rr, 8);
			v[1] = F<int16_t>(rr, 0xA);
			v[2] = F<int16_t>(rr, 0xC);
		}
		int32_t t[3];
		if (F<uint32_t>(rr, 4) & 0x200)
		{
			t[0] = v[0];
			t[1] = v[1];
			t[2] = v[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(v);
			GteMVMVA_RotV0();
			GteReadMAC123(t);
			MatrixMultiply(&arg->m, &m);
		}
		m.t[0] = t[0] + arg->m.t[0];
		m.t[1] = t[1] + arg->m.t[1];
		m.t[2] = t[2] + arg->m.t[2];
		if (F<int32_t>(rr, 0x18) != 0x10001000 || F<int16_t>(rr, 0x1C) != 0x1000)
		{
			if (F<uint32_t>(rr, 4) & 0x100)
			{
				int16_t d[9] = { F<int16_t>(rr, 0x18), 0, 0, 0, F<int16_t>(rr, 0x1A), 0, 0, 0, F<int16_t>(rr, 0x1C) };
				MatrixMultiply3x3(&m, d);
			}
			else
			{
				const int32_t s[3] = { F<int16_t>(rr, 0x18), F<int16_t>(rr, 0x1A), F<int16_t>(rr, 0x1C) };
				Scale3DMatrix(&m, s);
			}
		}
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		F<uint32_t>(h, 0x1C) = 0x2030;
		F<int32_t>(h, 0xC) = F<int16_t>(rr, 0x24);
		if (F<int16_t>(rr, 0x24) != 0)
		{
			F<uint32_t>(h, 0x1C) = 0x20F0;
			F<uint32_t>(h, 8) = F<uint32_t>(rr, 0x20);
		}
		F<int32_t>(h, 0x18) = arg->depth;
		PacketCursor() = RenderPrimModel2(h, OT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// Layout task (0x6685E0): the layout at the node (camera x turn about Y, at its position, scaled by +0x12)
	// ------------------------------------------------------------------
	static uint32_t __cdecl LayoutTask(TaskNode *n)
	{
		LayoutNode *L = (LayoutNode *)n;
		PrimArg arg;
		arg.morph = TexBase() + MORPH;
		RotationY(L->turn, &arg.m);
		arg.m.t[0] = L->pos[0];
		arg.m.t[1] = L->pos[1];
		arg.m.t[2] = L->pos[2];
		ComposeAffineTransform(&Camera(), &arg.m, &arg.m);
		arg.scale[2] = L->scale;
		arg.scale[1] = L->scale;
		arg.scale[0] = L->scale;
		arg.scaled = 1;
		arg.depth = L->depth;
		// 30 fps layer: see mag308_mobile_type8_held.inc
		FX_HELD(held_note_play(L, &arg);)
		return prim::play((prim::Layout *)L->layout, PartCallback, (int)&arg, 0) != 0 ? 0 : TASK_END;
	}

	// 0x6684C0 (opcodes 0x10A0 / 0x10A1): a layout at a bone: bone, spawn-position flag, layout index, scale, turn, depth;
	// ground = 1: y = 0 instead of the bone's y offset
	static const int16_t *SpawnLayout(const int16_t *ip, uint8_t *ent, PoseEntry *pose, int32_t ground)
	{
		const int32_t bone = ip[0], flag = ip[1], layout = ip[2], scale = ip[3], turn = ip[4], depth = ip[5];
		ip += 6;
		LayoutNode *L = (LayoutNode *)AddTaskToQueue(QLayouts(), ORIG_LayoutTask);
		if (!L) return ip;
		uint8_t *anim = ent + 0x60;
		PoseBones(anim, pose);
		GetEffectSpawnPosition(ent, bone, flag, L->pos);
		const PoseEntry *pe = pose + bone;
		L->pos[0] = (int16_t)(L->pos[0] + pe->pos[0]);
		if (ground != 0) L->pos[1] = 0;
		else L->pos[1] = (int16_t)(L->pos[1] + pe->pos[1]);
		L->pos[2] = (int16_t)(L->pos[2] + pe->pos[2]);
		L->scale = (int16_t)scale;
		L->turn = (int16_t)turn;
		L->depth = (int16_t)depth;
		BuildBoneMatricesFromPose(anim);
		DecodeModelPrimLayout(var<uint32_t>(LAYOUTS + layout * 8), L->layout, var<int32_t>(LAYOUTS + layout * 8 + 4));
		return ip;
	}

	// ------------------------------------------------------------------
	// Sprite task (0x668A50): draws THEN moves
	// ------------------------------------------------------------------
	static void SpriteDraw(const SpriteNode *s)
	{
		TransformCameraByShadowRotation(s->pos, s->size, -0x400);
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint32_t>(h, 0) = var<uint32_t>(SPRITES + s->seq * 4);
		F<uint16_t>(h, 4) = s->frame;
		F<uint16_t>(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, OT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}

	static uint32_t __cdecl SpriteTask(TaskNode *n)
	{
		SpriteNode *s = (SpriteNode *)n;
		const int32_t frames = *(const int16_t *)(var<uint32_t>(SPRITES + s->seq * 4) + 8);
		// 30 fps layer: see mag308_mobile_type8_held.inc
		FX_HELD(held_note_sprite(s);)
		SpriteDraw(s);
		const int8_t vx = s->vel[0], vy = s->vel[1];
		s->pos[0] = (int16_t)(s->pos[0] + (int16_t)(vx << 2));
		s->pos[1] = (int16_t)(s->pos[1] + (int16_t)(vy << 2));
		s->pos[2] = (int16_t)(s->pos[2] + (int16_t)(s->vel[2] << 2));
		s->vel[1] = (int8_t)(s->acc[1] * 2 + vy);
		s->vel[0] = (int8_t)(s->acc[0] + vx);
		s->frame++;
		return s->frame < frames ? 0 : TASK_END;
	}

	// 0x668970 (opcode 0x10A8): one sprite at a bone (mode, bone, spawn-position flag, sequence, size, velocity x / y / z,
	// acceleration x / y / -); mode != 0 writes the fields into the ENTITY instead of a new node
	static const int16_t *SpawnSprite(const int16_t *ip, uint8_t *ent, PoseEntry *pose)
	{
		const int32_t mode = *ip++;
		SpriteNode *s = mode == 0 ? (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask) : (SpriteNode *)ent;
		if (!s) return ip + 10;
		PoseBones(ent + 0x60, pose);
		const int32_t bone = ip[0];
		GetEffectSpawnPosition(ent, bone, ip[1], s->pos);
		const PoseEntry *pe = pose + bone;
		s->pos[0] = (int16_t)(s->pos[0] + pe->pos[0]);
		s->pos[1] = (int16_t)(s->pos[1] + pe->pos[1]);
		s->pos[2] = (int16_t)(s->pos[2] + pe->pos[2]);
		s->frame = 0;
		s->seq = (uint8_t)ip[2];
		s->size = ip[3];
		s->vel[0] = (int8_t)ip[4];
		s->vel[1] = (int8_t)ip[5];
		s->vel[2] = (int8_t)ip[6];
		s->acc[0] = (int8_t)ip[7];
		s->acc[1] = (int8_t)ip[8];
		s->acc[2] = (int8_t)ip[9];
		return ip + 10;
	}

	// 0x668D30: Rand15() * range >> 15 + base (range 0: base)
	static int32_t Spread(int32_t range, int32_t base)
	{
		if (range == 0) return base;
		return (mul32(Rand15(), range) >> 0xF) + base;
	}

	// 0x668B20 (opcode 0x10A9): `count` sprites at a bone with random spreads (mode, bone, flag, sequence, size, count, then
	// (range, base) pairs: x, y, z, velocity x / y / z, then a shift: acceleration = velocity >> shift (x, y, z))
	static const int16_t *SpawnSprites(const int16_t *ip, uint8_t *ent, PoseEntry *pose)
	{
		const int32_t mode = ip[0], bone = ip[1], flag = ip[2], seq = ip[3], size = ip[4];
		int32_t count = ip[5];
		int32_t r[12];
		for (int k = 0; k < 12; k++) r[k] = ip[6 + k];
		const int32_t shift = ip[18];
		ip += 19;
		PoseBones(ent + 0x60, pose);
		int16_t at[3];
		GetEffectSpawnPosition(ent, bone, flag, at);
		const PoseEntry *pe = pose + bone;
		at[0] = (int16_t)(at[0] + pe->pos[0]);
		at[1] = (int16_t)(at[1] + pe->pos[1]);
		at[2] = (int16_t)(at[2] + pe->pos[2]);
		while (count-- != 0)
		{
			SpriteNode *s = (SpriteNode *)ent;
			if (mode == 0) s = (SpriteNode *)AddTaskToQueue(QSprites(), ORIG_SpriteTask);
			if (!s) break;
			s->pos[0] = (int16_t)(Spread(r[0], r[1]) + at[0]);
			s->pos[1] = (int16_t)(Spread(r[2], r[3]) + at[1]);
			s->pos[2] = (int16_t)(Spread(r[4], r[5]) + at[2]);
			s->seq = (uint8_t)seq;
			s->size = (int16_t)size;
			s->frame = 0;
			s->vel[0] = (int8_t)Spread(r[6], r[7]);
			s->vel[1] = (int8_t)Spread(r[8], r[9]);
			s->vel[2] = (int8_t)Spread(r[10], r[11]);
			s->acc[0] = (int8_t)(s->vel[0] >> (shift & 31));
			s->acc[2] = (int8_t)(s->vel[2] >> (shift & 31));
			s->acc[1] = (int8_t)(s->vel[1] >> (shift & 31));
		}
		return ip;
	}

	// 0x668D60 (opcode 0x10B0): a flat semi-transparent full-screen quad of colour (ip[0], ip[1], ip[2]) (low bytes)
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
		// 30 fps layer: see mag308_mobile_type8_held.inc
		FX_HELD(held_note_quad(ip);)
		InsertPrimAltViewport(OT(0x20), q);
		return ip + 3;
	}

	// ------------------------------------------------------------------
	// Bone script (0x667990)
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
			case 0x1000: e->pos[0] = ip[0]; e->pos[1] = ip[1]; e->pos[2] = ip[2]; ip += 3; continue;
			case 0x1001:
				e->pos[0] = (int16_t)(e->pos[0] + ip[0]); e->pos[1] = (int16_t)(e->pos[1] + ip[1]); e->pos[2] = (int16_t)(e->pos[2] + ip[2]);
				ip += 3; continue;
			case 0x1002: s->vel[0] = ip[0]; s->vel[1] = ip[1]; s->vel[2] = ip[2]; ip += 3; continue;
			case 0x1003:
				s->vel[0] = (int16_t)(s->vel[0] + ip[0]); s->vel[1] = (int16_t)(s->vel[1] + ip[1]); s->vel[2] = (int16_t)(s->vel[2] + ip[2]);
				ip += 3; continue;
			case 0x1004:
				e->pos[0] = (int16_t)(e->pos[0] + s->vel[0]);
				e->pos[1] = (int16_t)(e->pos[1] + s->vel[1]);
				e->pos[2] = (int16_t)(e->pos[2] + s->vel[2]);
				if (e->pos[1] > 0)
				{
					e->pos[1] = 0;
					s->vel[1] = (int16_t)((int16_t)(s->vel[1] >> 2) - s->vel[1]);
				}
				continue;
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
			case 0x1080: memcpy(e, s->pose + *ip++, 0x10); continue;
			case 0x1081: memcpy(e, s->pose + *ip++, 8); continue;
			case 0x1082: memcpy(&e->rot[0], &s->pose[*ip++].rot[0], 8); continue;
			case 0x1090: ip = Sparks(ip, s->entity, s->pose); continue;
			case 0x10A0: ip = SpawnLayout(ip, s->entity, s->pose, 0); continue;
			case 0x10A1: ip = SpawnLayout(ip, s->entity, s->pose, 1); continue;
			case 0x10A8: ip = SpawnSprite(ip, s->entity, s->pose); continue;
			case 0x10A9: ip = SpawnSprites(ip, s->entity, s->pose); continue;
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

	// 0x667940: a bone script moving pose entry script[0] of `pose`
	static void StartScript(uint8_t *ent, PoseEntry *pose, const int16_t *script)
	{
		ScriptNode *s = (ScriptNode *)AddTaskToQueue(QScripts(), ORIG_ScriptTask);
		Memset32(&s->cur, 0, 8);
		s->entity = ent;
		s->pose = pose;
		s->cur = pose + script[0];
		s->ip = script + 1;
	}

	// ------------------------------------------------------------------
	// Entity polygons (0x669330): as 064's 0x6E5960 without its hidden-bit and colour-key tests
	// ------------------------------------------------------------------
	static uint32_t DrawPolys(uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		uint8_t *pk = (uint8_t *)cursor;
		const uint8_t *verts = F<const uint8_t *>(h, 4);
		const uint8_t *p = F<const uint8_t *>(h, 0);
		int16_t c = F<int16_t>(h, 8);
		F<int16_t>(h, 8) = (int16_t)(c - 1);
		while (c != 0)
		{
			const uint32_t i0 = *(const uint16_t *)p & 0xFFF, i1 = *(const uint16_t *)(p + 2) & 0xFFF, i2 = *(const uint16_t *)(p + 4) & 0xFFF;
			F<uint32_t>(h, 0x1C) = *(const uint32_t *)(verts + i0 * 8);
			F<uint32_t>(h, 0x20) = *(const uint32_t *)(verts + i0 * 8 + 4);
			F<uint32_t>(h, 0x24) = *(const uint32_t *)(verts + i1 * 8);
			F<uint32_t>(h, 0x28) = *(const uint32_t *)(verts + i1 * 8 + 4);
			F<uint32_t>(h, 0x2C) = *(const uint32_t *)(verts + i2 * 8);
			F<uint32_t>(h, 0x30) = *(const uint32_t *)(verts + i2 * 8 + 4);
			const uint8_t f0 = h[0x22], f1 = h[0x2A], f2 = h[0x32];
			if ((uint8_t)(f0 | f1 | f2) < 0x10 && !(f2 & (uint8_t)(f0 & f1)))
			{
				GteSetSXY012(F<uint32_t>(h, 0x1C), F<uint32_t>(h, 0x24), F<uint32_t>(h, 0x2C));
				GteNCLIP();
				GteReadMAC0(h + 0x3C);
				if (F<int32_t>(h, 0x3C) >= 0)
				{
					GteSetSZ123(F<int16_t>(h, 0x20), F<int16_t>(h, 0x28), F<int16_t>(h, 0x30));
					GteAVSZ3();
					F<uint32_t>(pk, 0) = 0x7000000;
					F<uint32_t>(pk, 4) = ((uint32_t)((*(const uint16_t *)(p + 0xE) & 0x200) | 0x2400) << 16) | F<uint32_t>(h, 0x14);
					F<uint32_t>(pk, 8) = F<uint32_t>(h, 0x1C);
					F<uint32_t>(pk, 0x10) = F<uint32_t>(h, 0x24);
					F<uint32_t>(pk, 0x18) = F<uint32_t>(h, 0x2C);
					F<uint32_t>(pk, 0xC) = *(const uint32_t *)(p + 8);
					F<uint32_t>(pk, 0x14) = *(const uint32_t *)(p + 0xC);
					F<uint16_t>(pk, 0x16) &= 0xFDFF;
					F<uint16_t>(pk, 0x1C) = *(const uint16_t *)(p + 6);
					GteReadOTZWord(h + 0x44);
					InsertPrimAutoDepth(ot + (uint32_t)(F<int32_t>(h, 0x44) >> (shift & 31)) * 4, pk);
					pk += 0x20;
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
			const uint32_t i0 = *(const uint16_t *)p & 0xFFF, i1 = *(const uint16_t *)(p + 2) & 0xFFF;
			const uint32_t i2 = *(const uint16_t *)(p + 4) & 0xFFF, i3 = *(const uint16_t *)(p + 6) & 0xFFF;
			F<uint32_t>(h, 0x1C) = *(const uint32_t *)(verts + i0 * 8);
			F<uint32_t>(h, 0x20) = *(const uint32_t *)(verts + i0 * 8 + 4);
			F<uint32_t>(h, 0x24) = *(const uint32_t *)(verts + i1 * 8);
			F<uint32_t>(h, 0x28) = *(const uint32_t *)(verts + i1 * 8 + 4);
			F<uint32_t>(h, 0x2C) = *(const uint32_t *)(verts + i2 * 8);
			F<uint32_t>(h, 0x30) = *(const uint32_t *)(verts + i2 * 8 + 4);
			F<uint32_t>(h, 0x34) = *(const uint32_t *)(verts + i3 * 8);
			F<uint32_t>(h, 0x38) = *(const uint32_t *)(verts + i3 * 8 + 4);
			const uint8_t f0 = h[0x22], f1 = h[0x2A], f2 = h[0x32], f3 = h[0x3A];
			if ((uint8_t)(f3 | f0 | f1 | f2) < 0x10 && !(f2 & (uint8_t)(f3 & f0 & f1)))
			{
				GteSetSXY012(F<uint32_t>(h, 0x1C), F<uint32_t>(h, 0x24), F<uint32_t>(h, 0x2C));
				GteNCLIP();
				GteReadMAC0(h + 0x3C);
				if (F<int32_t>(h, 0x3C) >= 0)
				{
					GteSetSZ0123(F<int16_t>(h, 0x20), F<int16_t>(h, 0x28), F<int16_t>(h, 0x30), F<int16_t>(h, 0x38));
					GteAVSZ4();
					F<uint32_t>(pk, 0) = 0x9000000;
					F<uint32_t>(pk, 4) = ((uint32_t)((*(const uint16_t *)(p + 0xE) & 0x200) | 0x2C00) << 16) | F<uint32_t>(h, 0x14);
					F<uint32_t>(pk, 8) = F<uint32_t>(h, 0x1C);
					F<uint32_t>(pk, 0x10) = F<uint32_t>(h, 0x24);
					F<uint32_t>(pk, 0x18) = F<uint32_t>(h, 0x2C);
					F<uint32_t>(pk, 0x20) = F<uint32_t>(h, 0x34);
					F<uint32_t>(pk, 0xC) = *(const uint32_t *)(p + 8);
					F<uint32_t>(pk, 0x14) = *(const uint32_t *)(p + 0xC);
					F<uint16_t>(pk, 0x1C) = *(const uint16_t *)(p + 0x10);
					F<uint16_t>(pk, 0x16) &= 0xFDFF;
					F<uint16_t>(pk, 0x24) = *(const uint16_t *)(p + 0x12);
					GteReadOTZWord(h + 0x44);
					InsertPrimAutoDepth(ot + (uint32_t)(F<int32_t>(h, 0x44) >> (shift & 31)) * 4, pk);
					pk += 0x28;
				}
			}
			p += 0x14;
			c = F<int16_t>(h, 0xA);
			F<int16_t>(h, 0xA) = (int16_t)(c - 1);
		}
		return (uint32_t)pk;
	}

	// entity model (0x6691B0): every object in the mask (+0x18); a bone whose pose entry is hidden (>= 2) marks its vertices
	// clipped, else they are projected by 0x676E00 (colour key = the entry's low byte)
	static uint32_t DrawModel(const uint8_t *model, uint8_t *h, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint32_t *objs = *(const uint32_t *const *)(model + 4);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const int32_t count = (int32_t)objs[0];
		if (count <= 0) return cursor;
		for (int32_t i = 0; i < count; i++)
		{
			const uint8_t *p = *(const uint8_t *const *)(model + 4) + objs[1 + i];
			if (!(F<uint32_t>(h, 0x18) & (1u << (i & 31)))) continue;
			uint8_t *out = F<uint8_t *>(h, 4);
			const int32_t groups = *(const int16_t *)p;
			p += 2;
			if (groups > 0)
			{
				for (int32_t g = groups; g != 0; g--)
				{
					const int32_t bone = *(const int16_t *)p;
					p += 2;
					const PoseEntry *pe = F<const PoseEntry *>(h, 0x4C) + bone;
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
			F<uint16_t>(h, 8) = *(const uint16_t *)p;
			F<uint16_t>(h, 0xA) = *(const uint16_t *)(p + 2);
			F<const uint8_t *>(h, 0) = p + 0xC;
			cursor = DrawPolys(h, ot, shift, cursor);
		}
		return cursor;
	}

	// ------------------------------------------------------------------
	// Entity draw (0x669070): its shadow, its bones from the pose plus the offsets, through its matrix, plus the offsets'
	// translations, through the camera; its model into the frame arena; its bones from the pose again
	// ------------------------------------------------------------------
	static void EntityDraw(uint8_t *ent, const PoseEntry *pose)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x98);
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
		F<const PoseEntry *>(h, 0x4C) = pose;
		F<uint32_t>(h, 4) = var<uint32_t>(0x1D98B3C);
		F<int16_t>(h, 0xC) = 0;
		F<int16_t>(h, 0xE) = 0;
		F<int16_t>(h, 0x10) = 0x140;
		F<int16_t>(h, 0x12) = 0xD8;
		F<uint32_t>(h, 0x14) = F<uint32_t>(ent, 0x28);
		F<uint32_t>(h, 0x18) = F<uint32_t>(ent, 0x7C);
		FrameArena() = DrawModel(F<const uint8_t *>(anim, 4), h, OT(0x44), 0, FrameArena());
		BuildBoneMatricesFromPose(anim);
		FieldFree(0x98);
	}

	// ------------------------------------------------------------------
	// Particles (0x669620 -> 0x6696B0): drawn THEN moved: the sequence frame (frame count - life) through the camera at
	// scale 0x1000 with 298's sprite renderer, into OT slot 2 of the module arena; then position += 4 x velocity, life - 1
	// ------------------------------------------------------------------
	static void ParticleDraw(uint8_t *h, Particle *p)
	{
		GteSetRotMatrixCtrl(&Camera());
		GteSetTransVectorCtrl(&Camera());
		GteLoadV0(p);
		GteMVMVA_RotV0Tr();
		GteReadMAC123((int32_t *)(h + 0x98));
		F<int16_t>(h, 0x94) = 0x1000;
		F<int16_t>(h, 0x8C) = 0x1000;
		F<int16_t>(h, 0x84) = 0x1000;
		const uint32_t seq = p->seq != 0 ? PART_SEQ1 : PART_SEQ0;
		F<uint32_t>(h, 0) = seq;
		const uint32_t idx = (uint32_t)var<uint16_t>(seq + 8) - (uint32_t)(int32_t)p->life;
		const uint32_t rec = seq + var<uint16_t>(seq + idx * 2 + 8);
		F<uint32_t>(h, 0x2C) = rec;
		F<uint32_t>(h, 0x30) = var<uint32_t>(rec);
		F<uint32_t>(h, 0x2C) = rec + 4;
		PacketCursor() = DrawSprites(h, OT(0x44), 2, PacketCursor(), true);
		p->life--;
		p->pos[0] = (int16_t)(p->pos[0] + (int16_t)(p->vel[0] << 2));
		p->pos[1] = (int16_t)(p->pos[1] + (int16_t)(p->vel[1] << 2));
		p->pos[2] = (int16_t)(p->pos[2] + (int16_t)(p->vel[2] << 2));
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
	// Main task (0x668DE0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *m = (MainNode *)n;
		const int16_t c = m->counter;
		int32_t level;
		if (c < 8) level = mul32(c, 0x8FC) >> 3;
		else if (c > 0x66) level = mul32(0x6E - c, 0x8FC) >> 3;
		else level = 0x8FC;
		SetScreenFlash((uint32_t)level, 0);
		if (m->counter >= 0x6E)
		{
			*(uint16_t *)Caster() &= 0xFFF7;
			*(uint16_t *)(Caster() + 0x9C) &= 0xFFF7;
			*(uint16_t *)(Caster() + 0x138) &= 0xFFF7;
			*(uint32_t *)(Caster() + 0x2C) = m->colour[0];
			*(uint32_t *)(Caster() + 0xC8) = m->colour[1];
			*(uint32_t *)(Caster() + 0x164) = m->colour[2];
			SetScreenFlash(0, 0);
			return TASK_END;
		}
		if (m->counter < 0x14)
		{
			const int32_t w = shl32(m->counter, 12) / 20;
			for (int k = 0; k < 3; k++)
			{
				ColourLerp(&m->colour[k], COLOUR_Target, 0x1000 - w, w, Caster() + k * 0x9C + 0x2C);
				Caster()[k * 0x9C + 0x2F] = 0x32;
			}
		}
		if ((uint16_t)m->counter < 0x56)
		{
			// 30 fps layer: see mag308_mobile_type8_held.inc
			FX_HELD(held_note_entities(m);)
			for (int k = 0; k < 3; k++)
			{
				EntityDraw(Caster() + k * 0x9C, m->pose[k]);
				Caster()[k * 0x9C] |= 4;
			}
		}
		const uint32_t t = (uint32_t)(m->counter - 0x46);
		if (t < 0x10)
		{
			// 30 fps layer: see mag308_mobile_type8_held.inc
			FX_HELD(held_note_tile((int32_t)((t & 0xFFFFF) << 8));)
			PacketCursor() = Tile(0xFF, 0xFF, 0xFF, (int32_t)((t & 0xFFFFF) << 8), PacketCursor());
			if (t == 0xF)
			{
				QScripts()->head = nullptr; QScripts()->tail = nullptr;
				QSprites()->head = nullptr; QSprites()->tail = nullptr;
				QLayouts()->head = nullptr; QLayouts()->tail = nullptr;
			}
		}
		const uint32_t t2 = (uint32_t)(m->counter - 0x56);
		if (t2 >= 0x3E8)
		{
			// 30 fps layer: see mag308_mobile_type8_held.inc
			FX_HELD(held_note_parts(m);)
			Particles(m);
		}
		else if (t2 < 0x10)
		{
			// 30 fps layer: see mag308_mobile_type8_held.inc
			FX_HELD(held_note_tile((int32_t)(0x1000 - ((t2 & 0xFFFFF) << 8)));)
			PacketCursor() = Tile(0xFF, 0xFF, 0xFF, (int32_t)(0x1000 - ((t2 & 0xFFFFF) << 8)), PacketCursor());
		}
		if (m->counter == 0)
		{
			BdPlaySE(SOUND_Start, 0, 0x80);
			m->counter++;
			return 0;
		}
		if (m->counter == 0x6A)
		{
			ReleaseVoice(m->voice);
			m->counter++;
			return 0;
		}
		if (m->counter == 0x67)
		{
			StreamLoad(0x168, TexBase() + STREAM_DST, 1);
			StreamLoad(0x169, TexBase() + STREAM_DST, 0);
		}
		m->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x667720)
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
		// 30 fps layer: see mag308_mobile_type8_held.inc
		FX_HELD(held_note_root();)
		if (r->counter == 1 && !r->started)
		{
			r->started = 1;
			MusicVolumeFade(0xF0);
			InitTaskQueuePool(QMain(), (void *)(TexBase() + MAIN), 0xE94, 1);
			InitTaskQueuePool(QLayouts(), (void *)(TexBase() + 0xEBC), 0x190, 8);
			InitTaskQueuePool(QSprites(), (void *)(TexBase() + 0x7BC), 0x1C, 0x40);
			InitTaskQueuePool(QScripts(), (void *)TexBase(), 0x2C, 0x2D);
			MainNode *m = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&m->counter, 0, 0x3A2);
			m->voice = ClaimVoiceSlot(SOUND_Voice, 1, 0x80);
			m->colour[0] = *(const uint32_t *)(Caster() + 0x2C);
			m->colour[1] = *(const uint32_t *)(Caster() + 0xC8);
			m->next = &m->parts[0];
			m->colour[2] = *(const uint32_t *)(Caster() + 0x164);
			for (int k = 0; k < 3; k++) StartScript(Caster() + k * 0x9C, m->pose[k], (const int16_t *)SCRIPTS[k]);
			StreamStateInit((void *)(TexBase() + STREAM_STATE));
		}
		int left;
		if (r->started)
		{
			left = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QScripts());
			ExecuteTaskQueue(QLayouts());
			ExecuteTaskQueue(QSprites());
		}
		else left = (int)n;
		if (r->started)
		{
			StreamPump();
			if (left == 0)
			{
				if (StreamPending(1) == 0) return 0;
				SetScreenFlash(0, 0);
				return TASK_END;
			}
		}
		r->counter++;
		return 0;
	}
}

	void register_mag308_mobile_type8()
	{
		register_port(mobile308::ORIG_RootTask, (void *)mobile308::RootTask, "308 RootTask", 308);
		register_port(mobile308::ORIG_MainTask, (void *)mobile308::MainTask, "308 MainTask", 308);
		register_port(mobile308::ORIG_ScriptTask, (void *)mobile308::ScriptTask, "308 ScriptTask", 308);
		register_port(mobile308::ORIG_LayoutTask, (void *)mobile308::LayoutTask, "308 LayoutTask", 308);
		register_port(mobile308::ORIG_SpriteTask, (void *)mobile308::SpriteTask, "308 SpriteTask", 308);
		// 30 fps layer: see mag308_mobile_type8_held.inc
		FX_HELD(register_mag308_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag308_mobile_type8_held.inc"
#endif
