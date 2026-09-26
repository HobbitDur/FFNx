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

// Effect 278: Carbuncle - Ruby Light (timeline-A GF family, MAG_278_*).
//
// Structure (see gf_study/gf_inventory_timeline.md 3.1):
//   SequenceTask (master, 0x680DF0) - flips the two packet cursors, spawns the creature at
//     tick 2 (seven pools carved out of the model buffer), runs the queues in the order
//     creature 0x2508188, sparkles 0x2508178, shards 0x2508168, rubies 0x2508158, orbits
//     0x2508138, rings 0x2508148, then the beam list 0x2508128 through its own executor
//     (0x6811E0 / 0x681270, shared flipbook header), sets the screen flash, and at the end
//     puts the lifted entities back down.
//   CreatureTask (0x681630) - 283-tick timeline: Carbuncle model (36..145, 215..235), the
//     ruby dome prim models (1..263 minus 145..230) with the floor mirror pass, two keyframed
//     prim-model stages (110..145, 145..176), sparkles, one ruby per target, shards, camera
//     orbit (every tick), party hide/lift, sounds, streams.
//   RubyTask (0x6822F0) - per target: prim model, orbit stones, rings, the reflect wall
//     model, damage at its tick 50.
//   SparkleTask 0x683E80, ShardTask 0x683DC0 (flipbooks), OrbitTask 0x683760, RingTask
//     0x6831D0 (prim models), BeamTask 0x683330 (flipbook; see the note at SpawnBeam: in
//     vanilla it never runs).
//   Prim-model callback 0x681FD0 is used through its original address (pure draw).
// Module globals: 0x2508110..0x25081FC (gf_study/gf_global_ranges.md).

#include "fx_port.h"

namespace ff8fx
{
namespace c278
{
	using namespace eng;

	// --- module globals ---
	inline uint32_t &Pause() { return var<uint32_t>(0x2508114); }           // debug pause, never set by the game
	inline uint8_t *&BeamHeader() { return var<uint8_t *>(0x2508118); }     // flipbook header of the beam list (per tick)
	inline uint8_t *&CastCtx() { return var<uint8_t *>(0x2508120); }
	inline TaskQueue &QueueBeams() { return var<TaskQueue>(0x2508128); }    // single list, custom executor
	inline TaskQueuePair &QueueOrbits() { return var<TaskQueuePair>(0x2508138); }   // .first orbits, .second rings
	inline TaskQueuePair &QueueRubies() { return var<TaskQueuePair>(0x2508158); }   // .first rubies, .second shards
	inline TaskQueuePair &QueueCreature() { return var<TaskQueuePair>(0x2508178); } // .first sparkles, .second creature
	inline TaskQueue &QueueRoot() { return var<TaskQueue>(0x2508198); }     // master (pool 0x25081A8)
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25081BC); }
	inline uint32_t &PacketCursor2() { return var<uint32_t>(0x25081C0); }   // the other arena: scratch of the mirror pass
	inline int32_t &Centroid() { return var<int32_t>(0x25081C4); }          // mean "+0x20" (height) of the targets
	inline int32_t &Base() { return var<int32_t>(0x25081CC); }              // centroid - 0x7D0 (floor reference)
	inline int32_t &Lift() { return var<int32_t>(0x25081D0); }              // added to the enemies' height at tick 2
	inline uint32_t &Flash() { return var<uint32_t>(0x25081D4); }
	inline uint8_t &DoneFlag() { return var<uint8_t>(0x25081D8); }
	inline uint8_t *&ModelBuffer() { return var<uint8_t *>(0x25081F8); }    // = Magic_TextureOFF_ToEAX1()
	static const uint32_t BBOX = 0x25081E0;   // s16 min x, y, z, max x, y, z (MAG_278_sub_6D6070)
	static const uint32_t MIRROR = 0x25081F0; // s16 0, 0, base - 200: floor plane of the mirror pass
	// engine
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }     // battle_texture_data_ptr_1D8E054
	inline int16_t &CamWorld(int k) { return var<int16_t>(0xB8B7F0 + 2 * k); }
	inline int16_t &CamLookAt(int k) { return var<int16_t>(0xB8B7F8 + 2 * k); }

	static const uint32_t ORIG_SequenceTask = 0x680DF0;
	static const uint32_t ORIG_CreatureTask = 0x681630;
	static const uint32_t ORIG_RubyTask = 0x6822F0;
	static const uint32_t ORIG_RingTask = 0x6831D0;
	static const uint32_t ORIG_BeamTask = 0x683330;
	static const uint32_t ORIG_OrbitTask = 0x683760;
	static const uint32_t ORIG_ShardTask = 0x683DC0;
	static const uint32_t ORIG_SparkleTask = 0x683E80;
	static const uint32_t CB_PrimObject = 0x681FD0; // prim-model callback (pure draw)

	// --- engine / module helpers called through their original addresses ---
	namespace cw
	{
		inline int32_t SharedRand() { return fn<int32_t (__cdecl *)()>(0x7059E0)(); }  // MAG_106_sub_7059E0: (125 s + 14) & 0x7FFF
		inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
		inline void RotX(int32_t a, void *m) { fn<void (__cdecl *)(int32_t, void *)>(0x701220)(a, m); }   // MAG_063_sub_701220 (3x3 + pad)
		inline void RotY(int32_t a, void *m) { fn<void (__cdecl *)(int32_t, void *)>(0x701270)(a, m); }   // MAG_063_sub_701270
		inline void RotZ(int32_t a, void *m) { fn<void (__cdecl *)(int32_t, void *)>(0x7012C0)(a, m); }   // MAG_065_sub_7012C0
		inline void RotYScaled(int32_t a, void *m, int32_t s) { fn<void (__cdecl *)(int32_t, void *, int32_t)>(0x6DBCC0)(a, m, s); } // MAG_103_sub_6DBCC0
		inline void MatMul56C220(void *a, const void *b) { fn<void (__cdecl *)(void *, const void *)>(0x56C220)(a, b); }
		inline void GteMatrixMultiply(const void *a, void *b) { fn<void (__cdecl *)(const void *, void *)>(0x56C270)(a, b); }
		inline void Decode(uint32_t src, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(src, layout, size); }
		inline void BindModelSections(void *e, void *sections, uint32_t data) { fn<void (__cdecl *)(void *, void *, uint32_t)>(0x6EC060)(e, sections, data); }
		inline void ReadAnimationStart(void *e, int32_t id) { fn<void (__cdecl *)(void *, int32_t)>(0x6574D0)(e, id); }
		inline void AdvanceModelAnim(void *e) { fn<void (__cdecl *)(void *)>(0x6FBDB0)(e); }
		inline uint32_t DrawModel(void *e, void *buf, uint32_t cursor, uint32_t mode) { return fn<uint32_t (__cdecl *)(void *, void *, uint32_t, uint32_t)>(0x6A79B0)(e, buf, cursor, mode); }
		inline void ModelBBox(uint32_t model, uint32_t bbox) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x6D6070)(model, bbox); }       // MAG_278_sub_6D6070
		inline void MirrorPass(uint32_t plane, uint32_t bbox) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x6812E0)(plane, bbox); }      // MAG_278_sub_6812E0
		inline void VertexPos(void *e, int32_t a, int32_t b, void *out) { fn<void (__cdecl *)(void *, int32_t, int32_t, void *)>(0x682270)(e, a, b, out); } // MAG_278_sub_682270
		inline uint32_t DrawWall(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x682760)(h, ot, mode, cursor); }
		inline uint32_t DrawBeamSprites(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x683490)(h, ot, mode, cursor); }
		inline void Rot2D(const void *src, void *dst, int32_t a) { fn<void (__cdecl *)(const void *, void *, int32_t)>(0x6836F0)(src, dst, a); }
		inline void DamageTarget(void *slot) { fn<void (__cdecl *)(void *)>(0x683920)(slot); }                                        // MAG_278_sub_683920
		inline uint32_t DrawDome(void *h, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x6839B0)(h, ot, mode, cursor); }
		inline void MorphDome(int32_t t) { fn<void (__cdecl *)(int32_t)>(0x683CA0)(t); }                                                  // MAG_278_sub_683CA0
		inline void SpawnPosition(void *slot, int32_t bone, int32_t a, void *out) { fn<void (__cdecl *)(void *, int32_t, int32_t, void *)>(0x502170)(slot, bone, a, out); }
		inline void DefaultPosition(void *slot, void *out) { fn<void (__cdecl *)(void *, void *)>(0x571400)(slot, out); }
		inline void StreamLoad(int32_t id, uint32_t dst, int32_t n) { fn<void (__cdecl *)(int32_t, uint32_t, int32_t)>(0x5341D0)(id, dst, n); }
		inline int32_t StreamBusy() { return fn<int32_t (__cdecl *)()>(0x534270)(); }
		inline void BattlePreload() { fn<void (__cdecl *)()>(0x534210)(); }
		inline void StreamWaitDone(uint32_t buf, uint8_t *flag) { fn<void (__cdecl *)(uint32_t, uint8_t *)>(0x508630)(buf, flag); }
		inline void PlaySE(uint32_t se, int32_t a, int32_t b) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t)>(0x501330)(se, a, b); }
		inline void ReleaseVoice(uint32_t slot) { fn<void (__cdecl *)(uint32_t)>(0x4A2940)(slot); }
		inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }                                                                  // BS_Camera_ArmReturnAfterEffect
	}
	using namespace cw;

#pragma pack(push, 1)
	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
	struct MasterNode // root pool 0x25081A8, 0x14 bytes
	{
		TaskNode hdr;
		uint16_t counter; // +0x0C
		uint8_t pad0E;
		uint8_t spawned;  // +0x0F
		uint32_t parity;  // +0x10 packet arenas
	};

	struct CreatureNode // the single node of queue 0x2508188 = ModelBuffer + 0x28A4, 0x5E0 bytes
	{
		TaskNode hdr;
		int16_t counter;         // +0x0C timeline tick
		int16_t ntargets;        // +0x0E
		uint32_t voice;          // +0x10 BdSound_ClaimVoiceSlot
		uint8_t *targets[3];     // +0x14 BattleEntitySlotData of each target (written without bound)
		uint8_t model[0x9C];     // +0x20 E: standard effect model block (E+0x40 root Mat4x3 = +0x60,
		                         //   E+0x60 BattleAnimHeader = +0x80, E+0x64 comFileData = +0x84, E+0x6C BattleAnimCmd = +0x8C)
		uint8_t sections[0x10];  // +0xBC model section pointers
		uint8_t prim[0x514];     // +0xCC prim-model player layout (stages 110 / 145)
	};

	struct PrimCtx // callback context of 0x681FD0 (arg of the prim player)
	{
		Mat4x3 m;       // +0x00 object frame
		int32_t shade;  // +0x20 -> render header +0x18
		uint8_t *morph; // +0x24 morph output buffer (ModelBuffer + 0x2E84)
	};

	struct RubyNode // queue 0x2508158 (3 x 0x10C)
	{
		TaskNode hdr;
		int16_t counter;    // +0x0C
		int16_t pad0E;
		uint8_t *target;    // +0x10 BattleEntitySlotData
		Mat4x3 model;       // +0x14 Y rotation x target scale; translation = the target's default effect position
		Mat4x3 view;        // +0x34 camera x model
		int16_t center[3];  // +0x54 centre of the orbit stones (x = 0, y/z along the path table 0x10BD490)
		int16_t pad5A;
		int16_t pos[3];     // +0x5C default effect position (y cleared)
		int16_t pad62;
		uint8_t prim[0xA8]; // +0x64 prim-model player layout (0x10C1CD8)
	};

	struct OrbitNode // queue 0x2508138 (24 x 0x1C): stone circling the ruby
	{
		TaskNode hdr;
		int16_t counter; // +0x0C
		int16_t tilt;    // +0x0E X rotation (negated)
		int16_t angle;   // +0x10 position angle around the centre + Z rotation
		int16_t dangle;  // +0x12
		int16_t dtilt;   // +0x14
		int16_t pad16;
		RubyNode *parent; // +0x18
	};

	struct RingNode // queue 0x2508148 (9 x 0x1C): expanding ring
	{
		TaskNode hdr;
		int16_t counter; // +0x0C
		int16_t height;  // +0x0E
		int16_t scale;   // +0x10
		int16_t speed;   // +0x12
		int16_t accel;   // +0x14
		int16_t pad16;
		RubyNode *parent; // +0x18
	};

	struct BeamNode // list 0x2508128 (256 x 0x1C)
	{
		TaskNode hdr;
		int16_t counter; // +0x0C flipbook frame
		int16_t angle;   // +0x0E
		int16_t scale;   // +0x10
		int16_t y;       // +0x12
		int16_t vy;      // +0x14
		int16_t pad16;   // (never written)
		RubyNode *parent; // +0x18
	};

	struct SparkleNode // queue 0x2508178 (64 x 0x14)
	{
		TaskNode hdr;
		int16_t pos[3]; // +0x0C
		int16_t frame;  // +0x12
	};

	struct ShardNode // queue 0x2508168 (8 x 0x1C)
	{
		TaskNode hdr;
		int16_t pos[3];  // +0x0C
		int16_t frame;   // +0x12
		int16_t vel[3];  // +0x14
		int16_t variant; // +0x1A flipbook 0x10BD2FC (1) / 0x10BD170 (0)
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14, "master node is 0x14 bytes");
	static_assert(sizeof(CreatureNode) == 0x5E0, "creature node is 0x5E0 bytes");
	static_assert(sizeof(PrimCtx) == 0x28, "prim context is 0x28 bytes");
	static_assert(sizeof(RubyNode) == 0x10C, "ruby node is 0x10C bytes");
	static_assert(sizeof(OrbitNode) == 0x1C, "orbit node is 0x1C bytes");
	static_assert(sizeof(RingNode) == 0x1C, "ring node is 0x1C bytes");
	static_assert(sizeof(BeamNode) == 0x1C, "beam node is 0x1C bytes");
	static_assert(sizeof(SparkleNode) == 0x14, "sparkle node is 0x14 bytes");
	static_assert(sizeof(ShardNode) == 0x1C, "shard node is 0x1C bytes");

	inline uint8_t *E(CreatureNode *c) { return c->model; }
	inline Mat4x3 &Root(CreatureNode *c) { return *(Mat4x3 *)(c->model + 0x40); }
	inline uint8_t *Scratch() { return ModelBuffer() + 0x2E84; } // model draw / morph / dome vertex scratch

	// nbones of a battle slot / effect model: **(comFileData) (the skeleton's first byte)
	inline uint8_t BoneCount(const uint8_t *slot) { return ***(uint8_t ***)(slot + 0x64); }
}
}

#ifdef FF8_FX_HELD
#include "mag278_carbuncle_held.h"
#endif

namespace ff8fx
{
namespace c278
{

	// MAG_278_sub_683990 / MAG_278_sub_6812C0: hide / show the three party models (flag bit 2)
	static void PartyHide()
	{
		var<uint16_t>(0x1D972C0) |= 4;
		var<uint16_t>(0x1D9735C) |= 4;
		var<uint16_t>(0x1D973F8) |= 4;
	}
	static void PartyShow()
	{
		var<uint16_t>(0x1D972C0) &= 0xFFFB;
		var<uint16_t>(0x1D9735C) &= 0xFFFB;
		var<uint16_t>(0x1D973F8) &= 0xFFFB;
	}

	// ------------------------------------------------------------------
	// Camera: MAG_278_sub_683D10 - eye rotated about the look-at in the ground plane (x/z) by
	// `angle` and its offset scaled by `scale` (4.12), height offset scaled only. Works on
	// any eye / look-at word triple.
	// ------------------------------------------------------------------
	static void CamRotate(int16_t *w, const int16_t *l, int32_t angle, int32_t scale)
	{
		int16_t l1 = l[1];
		w[1] = (int16_t)((int16_t)(mul32((int32_t)w[1] - l1, scale) >> 12) + l1);
		int32_t dx = mul32((int32_t)w[0] - (int32_t)l[0], scale) >> 12;
		int32_t dz = mul32((int32_t)w[2] - (int32_t)l[2], scale) >> 12;
		int32_t sn = ComputeSin(angle);
		int32_t cs = ComputeCos(angle);
		int32_t x = (int32_t)((uint32_t)mul32(cs, dx) - (uint32_t)mul32(sn, dz)) >> 12;
		int32_t z = (int32_t)((uint32_t)mul32(cs, dz) + (uint32_t)mul32(sn, dx)) >> 12;
		w[0] = (int16_t)(x + l[0]);
		w[2] = (int16_t)(z + l[2]);
	}
	inline int16_t *CamW() { return (int16_t *)0xB8B7F0; }
	inline int16_t *CamL() { return (int16_t *)0xB8B7F8; }

	// ------------------------------------------------------------------
	// Draw helpers
	// ------------------------------------------------------------------
	static int32_t FlashAt(int32_t c) // creature tick c
	{
		if (c < 8) return shl32(c, 11) >> 3;
		if (c > 0x113) return shl32(0x11B - c, 11) >> 3;
		return 0x800;
	}

	// ruby dome drawn at creature tick c (1..263 minus 145..230)
	static bool DomeDrawn(int32_t c) { return (uint32_t)(c - 1) < 0x107 && (c < 0x91 || c >= 0xE7); }

	// dome morph (0 = shell 0x10C0D10, 0x1000 = shell 0x10C1940)
	static int32_t DomeMorph(int32_t c)
	{
		int32_t v = c - 1;
		if ((uint32_t)v < 0xF) return 0;
		if ((uint32_t)v < 0x23) return (int32_t)((uint32_t)shl32(v + 0xFFFF1, 12) / 20u);
		if (c < 0xF7) return 0x1000;
		int32_t s = 0x1000 - ((shl32(c, 12) - 0xF6FF9) >> 3);
		return s < 0 ? 0 : s;
	}

	// 0x681700..0x681946: the dome (and the inner shell model 0x10BFB08 once it opens), with
	// the floor mirror pass when party slot 0's model is 44..46. flash = the tick's screen
	// flash (dome brightness), s = morph (stages 16..35 and 247..263)
	static void DomeDraw(int32_t c, int32_t flash, int32_t s)
	{
		int32_t v = c - 1;
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		uint8_t id = var<uint8_t>(0x1D98990);
		if (id == 0x2C || id == 0x2D || id == 0x2E) MirrorPass(MIRROR, BBOX);
		Mat4x3 M;
		RotY(0, &M);
		M.t[1] = 0;
		M.t[0] = 0;
		M.t[2] = Base() - 200;
		ComposeAffineTransform(&Camera(), &M, &M);
		GteSetTransVector(&M);
		GteSetRotMatrix(&M);
		*(uint32_t *)h = 0x10C0D08;
		*(uint32_t *)(h + 0x1C) = 0x2030;
		int32_t a = shl32(0x1000 - flash, 7) >> 12;
		h[0xA] = (uint8_t)a;
		h[9] = (uint8_t)a;
		h[8] = (uint8_t)a;
		if ((uint32_t)v < 0xF)
		{
			*(uint32_t *)(h + 4) = 0x10C0D10;
			FrameCursor() = DrawDome(h, RenderOT(), 2, FrameCursor());
		}
		else if ((uint32_t)v >= 0x23 && c < 0xF7)
		{
			*(uint32_t *)(h + 4) = 0x10C1940;
			FrameCursor() = DrawDome(h, RenderOT(), 2, FrameCursor());
			*(uint32_t *)h = 0x10BFB08;
			*(uint32_t *)(h + 4) = 0x10BFB10;
			*(uint32_t *)(h + 0x1C) = 0x2000;
			FrameCursor() = RenderPrimModel(h, RenderOT(), 2, FrameCursor());
		}
		else
		{
			MorphDome(s);
			*(uint32_t *)(h + 4) = (uint32_t)Scratch();
			FrameCursor() = DrawDome(h, RenderOT(), 2, FrameCursor());
			// same stack matrix (its pad keeps what ComposeAffineTransform left there)
			M.m[2][2] = (int16_t)s;
			M.m[1][1] = (int16_t)s;
			M.m[0][0] = (int16_t)s;
			M.m[2][1] = 0; M.m[2][0] = 0; M.m[1][2] = 0; M.m[1][0] = 0; M.m[0][2] = 0; M.m[0][1] = 0;
			M.t[1] = 0;
			M.t[0] = 0;
			M.t[2] = Base() - 200;
			ComposeAffineTransform(&Camera(), &M, &M);
			GteSetTransVector(&M);
			GteSetRotMatrix(&M);
			*(uint32_t *)h = 0x10BFB08;
			*(uint32_t *)(h + 4) = 0x10BFB10;
			*(uint32_t *)(h + 0x1C) = 0x2000;
			FrameCursor() = RenderPrimModel(h, RenderOT(), 2, FrameCursor());
		}
		FieldFree(0x58);
	}

	// prim stage 1 (creature 110..145): relative to vertex 0xB0 of the model, tilted 0xA00
	static void CtxStage110(PrimCtx &ctx, CreatureNode *cn)
	{
		RotX(0xA00, &ctx.m);
		int16_t v[4];
		VertexPos(E(cn), 0, 0xB0, v);
		ctx.m.t[0] = v[0];
		ctx.m.t[1] = v[1];
		ctx.m.t[2] = v[2];
		ComposeAffineTransform(&Camera(), &ctx.m, &ctx.m);
		ctx.morph = Scratch();
		ctx.shade = -256;
	}
	// prim stage 2 (creature 145..176): absolute frame at depth 0x1000
	static void CtxStage145(PrimCtx &ctx)
	{
		RotX(0, &ctx.m);
		ctx.morph = Scratch();
		ctx.m.t[0] = 0;
		ctx.m.t[1] = 0;
		ctx.m.t[2] = 0x1000;
		ctx.shade = -512;
	}
	// ruby prim (ruby ticks 0..20): at the target, camera-relative
	static void CtxRuby(PrimCtx &ctx, const RubyNode *r)
	{
		RotY(0, &ctx.m);
		ctx.m.t[0] = r->pos[0];
		ctx.m.t[1] = r->pos[1];
		ctx.m.t[2] = r->pos[2];
		ComposeAffineTransform(&Camera(), &ctx.m, &ctx.m);
		ctx.morph = Scratch();
		ctx.shade = 0;
	}

	// ruby model / view matrices (0x6823B1..0x6823EB): the target's Y rotation (+0x0E) scaled
	// by min(target+0x26 << 2, 0x1000). model: the node's matrix (or a copy)
	static void RubyMatrices(const RubyNode *r, Mat4x3 *model, Mat4x3 *view)
	{
		int32_t s = (int32_t)*(int16_t *)(r->target + 0x26) << 2;
		if (s > 0x1000) s = 0x1000;
		// the angle goes in AX over the scale (the callee reads the low word)
		RotYScaled((int32_t)(((uint32_t)s & 0xFFFF0000u) | *(uint16_t *)(r->target + 0xE)), model, s);
		ComposeAffineTransform(&Camera(), model, view);
	}

	// orbit centre for ruby tick c (15..34): path table 0x10BD490 (8 bytes a key, y +2, z +4)
	static void RubyCenter(int32_t c, int16_t out[3])
	{
		uint32_t k = (uint32_t)(c - 0xF);
		uint32_t t = (k * 15u << 12) / 20u;
		int32_t idx = (int32_t)t >> 12;
		int32_t f = (int32_t)(t & 0xFFF);
		int16_t a = var<int16_t>(0x10BD492 + 8 * idx);
		out[1] = (int16_t)((int16_t)(mul32((int32_t)var<int16_t>(0x10BD49A + 8 * idx) - a, f) >> 12) + a);
		out[0] = 0;
		a = var<int16_t>(0x10BD494 + 8 * idx);
		out[2] = (int16_t)((int16_t)(mul32((int32_t)var<int16_t>(0x10BD49C + 8 * idx) - a, f) >> 12) + a);
	}

	// reflect wall parameters for ruby tick c (20..54): height (+0x5E) and fade (+0x08)
	static int32_t WallHeight(int32_t e)
	{
		const uint8_t *mb = ModelBuffer();
		int16_t base = *(const int16_t *)mb, span = *(const int16_t *)(mb + 2);
		if ((uint32_t)e <= 0x14) return (int16_t)((int16_t)((uint32_t)mul32(span, e + 1) / 20u) + base);
		return (int16_t)(span + base);
	}
	static int32_t WallFade(int32_t e)
	{
		if ((uint32_t)e <= 0x14) return 0;
		return (int32_t)((uint32_t)shl32(e + 0xFFFEC, 12) / 15u);
	}

	// 0x6825B5..0x68268D: the reflect wall (model 0x10BD488 through MAG_278_sub_682760)
	static uint8_t *WallDraw(int32_t c, int32_t height, int32_t fade, const Mat4x3 *view)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x60);
		*(uint32_t *)h = 0x10BD488;
		*(uint16_t *)(h + 0x12) = 0;
		*(uint16_t *)(h + 0x10) = 0;
		*(uint16_t *)(h + 0x16) = 0x100;
		*(uint16_t *)(h + 0x14) = 0x100;
		*(uint16_t *)(h + 0x1A) = 0;
		*(uint16_t *)(h + 0x18) = 0;
		*(uint16_t *)(h + 0x1C) = 0x40;
		*(uint16_t *)(h + 0x1E) = 0x80;
		*(uint16_t *)(h + 0x0C) = 0;
		*(uint16_t *)(h + 0x0E) = (uint16_t)((uint8_t)(0u - (uint8_t)(c << 3)) & 0x7F);
		*(int16_t *)(h + 0x5C) = *(int16_t *)ModelBuffer();
		h[6] = 0;
		h[5] = 0;
		h[4] = 0;
		*(int32_t *)(h + 8) = fade;
		*(int16_t *)(h + 0x5E) = (int16_t)height;
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		PacketCursor() = DrawWall(h, RenderOT(), 2, PacketCursor());
		return h; // freed by the caller (the ring spawn reads +0x5E)
	}

	// 0x683760: stone at `angle` around the centre, tilted, squashed in x as it fades
	static void OrbitDraw(const int16_t *center, int16_t angle, int16_t tilt, int32_t fade, int16_t sx, const Mat4x3 *view)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		int16_t p[4] = { 0, 0, 0, 0 }; // 4th word: vanilla stack garbage (GTE VZ0 high half only)
		Rot2D(center, p, angle);
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		GteLoadV0(p);
		GteMVMVA_RotV0Tr();
		Mat4x3 Q = {};
		GteReadMAC123(Q.t);
		*(uint32_t *)(h + 0x1C) = 0xF3;
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(int32_t *)(h + 0xC) = fade;
		int16_t S[9] = { sx, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000 };
		Mat4x3 R = {};
		RotZ((int32_t)(uint16_t)angle, &R);
		MatMul56C220(&R, S);
		GteMatrixMultiply(view, &R);
		// VANILLA: the angle goes in AX over a stack address (0x683880); the callee reads the low word
		RotX((int32_t)(uint16_t)(int16_t)-tilt, &Q);
		GteMatrixMultiply(&R, &Q);
		GteSetRotMatrix(&Q);
		GteSetTransVector(&Q);
		*(uint32_t *)h = 0x10BF3A8;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}
	// fade parameters of an orbit stone at its tick c (0x6837CC..0x683825)
	static void OrbitFade(int32_t c, int32_t &fade, int32_t &sx)
	{
		if (c > 0xA)
		{
			int32_t q = shl32(c - 0xA, 12) / 6;
			sx = (int16_t)(0x1000 - (int16_t)q);
			fade = (q >> 1) + 0x800;
		}
		else
		{
			sx = 0x1000;
			fade = 0x800;
		}
	}

	static int32_t RingFade(int32_t c) { return ComputeSin(shl32(c, 10) / 20); }

	// 0x6831D0: ring at height above the ruby, scaled, fade sin(c * 1024 / 20)
	static void RingDraw(int16_t height, int16_t scale, int32_t c, const Mat4x3 *view)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		int16_t V[4] = { 0, 0, height, 0 }; // 4th word: vanilla stack garbage
		GteSetRotMatrix(view);
		GteSetTransVector(view);
		GteLoadV0(V);
		GteMVMVA_RotV0Tr();
		Mat4x3 T = {};
		GteReadMAC123(T.t);
		GteSetTransVector(&T);
		Mat4x3 S = {};
		S.m[0][0] = scale;
		S.m[1][1] = scale;
		S.m[2][2] = scale;
		MatMul56C220(&S, view);
		GteSetRotMatrix(&S);
		*(uint32_t *)h = 0x10BF480;
		*(int32_t *)(h + 0xC) = RingFade(c);
		h[0xA] = 0;
		h[9] = 0;
		h[8] = 0;
		*(uint32_t *)(h + 0x1C) = 0xF3;
		PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// 0x6833D4..0x683454: beam sprites at p (world, relative to the ruby) through the shared header
	static void BeamDraw(uint8_t *h, const int16_t *p, int16_t scale, int32_t frame, const Mat4x3 *view)
	{
		GteSetRotMatrixCtrl(view);
		GteSetTransVectorCtrl(view);
		GteLoadV0(p);
		GteMVMVA_RotV0Tr();
		GteReadMAC123((int32_t *)(h + 0x98));
		*(int16_t *)(h + 0x94) = scale;
		*(int16_t *)(h + 0x8C) = scale;
		*(int16_t *)(h + 0x84) = scale;
		uint8_t *seq = *(uint8_t **)h;
		uint8_t *fr = seq + *(uint16_t *)(seq + 2 * frame + 0xA);
		*(uint8_t **)(h + 0x2C) = fr;
		*(uint32_t *)(h + 0x30) = *(uint32_t *)fr;
		*(uint8_t **)(h + 0x2C) = fr + 4;
		PacketCursor() = DrawBeamSprites(h, RenderOT(), 2, PacketCursor());
	}

	// flipbooks
	static void SparkleDraw(const int16_t *pos4)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(pos4, 0x400, -0x100);
		*(uint32_t *)h = 0x10BD044;
		*(int16_t *)(h + 4) = pos4[3];
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}
	static void ShardDraw(const int16_t *pos4, int16_t variant)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		TransformCameraByShadowRotation(pos4, 0x800, -0x200);
		*(uint32_t *)h = variant != 0 ? 0x10BD2FC : 0x10BD170;
		*(int16_t *)(h + 4) = pos4[3];
		*(int16_t *)(h + 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// ------------------------------------------------------------------
	// Sparkle (0x683E80): 11-frame flipbook 0x10BD044 at a fixed point
	// ------------------------------------------------------------------
	static uint32_t __cdecl SparkleTask(TaskNode *n)
	{
		SparkleNode *p = (SparkleNode *)n;
		// 30 fps layer: see mag278_carbuncle_held.inc
		FX_HELD(held_note_sparkle(p);)
		SparkleDraw(p->pos); // (reads 8 bytes: x, y, z, frame)
		if (Pause()) return 0;
		p->frame++;
		return p->frame < 0xB ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Shard (0x683DC0): 14-frame flipbook drifting by its velocity
	// ------------------------------------------------------------------
	static uint32_t __cdecl ShardTask(TaskNode *n)
	{
		ShardNode *p = (ShardNode *)n;
		// 30 fps layer: see mag278_carbuncle_held.inc
		FX_HELD(held_note_shard(p);)
		ShardDraw(p->pos, p->variant);
		if (Pause()) return 0;
		p->pos[0] += p->vel[0];
		p->pos[1] += p->vel[1];
		p->pos[2] += p->vel[2];
		p->frame++;
		return p->frame < 0xE ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Orbit stone (0x683760): 17 ticks
	// ------------------------------------------------------------------
	static uint32_t __cdecl OrbitTask(TaskNode *n)
	{
		OrbitNode *o = (OrbitNode *)n;
		// 30 fps layer: see mag278_carbuncle_held.inc
		FX_HELD(held_note_orbit(o);)
		int32_t fade, sx;
		OrbitFade(o->counter, fade, sx);
		OrbitDraw(o->parent->center, o->angle, o->tilt, fade, (int16_t)sx, &o->parent->view);
		if (Pause()) return 0;
		if (o->counter >= 0x10) return TASK_END;
		o->angle += o->dangle;
		o->tilt += o->dtilt;
		o->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Ring (0x6831D0): 21 ticks, the growth speed brakes to 0x10
	// ------------------------------------------------------------------
	static uint32_t __cdecl RingTask(TaskNode *n)
	{
		RingNode *r = (RingNode *)n;
		// 30 fps layer: see mag278_carbuncle_held.inc
		FX_HELD(held_note_ring(r);)
		RingDraw(r->height, r->scale, r->counter, &r->parent->view);
		if (Pause()) return 0;
		if (r->counter >= 0x14) return TASK_END;
		r->speed += r->accel;
		if (r->speed < 0x10) r->speed = 0x10;
		r->scale += r->speed;
		r->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Beam (0x683330): 16-frame sprite sequence 0x10BCE98 rising from the orbit centre.
	// y/vy integrate even when paused (no pause test there), the frame index only when not.
	// ------------------------------------------------------------------
	static uint32_t __cdecl BeamTask(TaskNode *n)
	{
		BeamNode *b = (BeamNode *)n;
		uint8_t *h = BeamHeader();
		RubyNode *parent = b->parent;
		int16_t out[4] = { 0, 0, 0, 0 }; // 4th word: vanilla stack garbage
		if (parent->counter >= 0x23)
		{
			if (b->vy == 0) b->vy = (int16_t)((SharedRand() & 0x3F) + 0x80);
			else
			{
				b->vy = (int16_t)(b->vy - 0x10);
				if (b->vy < 1) b->vy = 1;
			}
			int16_t P[4] = { parent->center[0], parent->center[1], parent->center[2], parent->pad5A };
			b->y += b->vy;
			P[2] = b->y;
			Rot2D(P, out, b->angle);
		}
		else
		{
			Rot2D(parent->center, out, b->angle);
			b->y = parent->center[2];
		}
		// 30 fps layer: see mag278_carbuncle_held.inc
		FX_HELD(held_note_beam(b, out);)
		BeamDraw(h, out, b->scale, b->counter, &parent->view);
		if (Pause()) return 0;
		if (b->counter >= 0xF) return TASK_END;
		b->counter++;
		return 0;
	}

	// MAG_106_sub_705A00: "append" a node to a queue passed BY VALUE. It links the new node
	// only behind the copy's tail and never writes the real header back.
	// VANILLA DEAD CODE: the beam list 0x2508128 is only ever filled through this function,
	// its head stays 0 (only InitTaskQueuePool and the executor 0x681270 write it) and its
	// tail is the executor's last survivor (0 on an empty list), so every beam node is
	// allocated but never linked: BeamTask never runs, and the 256-node pool fills up
	// (after which no more are made). Reproduced as is.
	static TaskNode *AppendByValue(TaskQueue q, uint32_t task_fn)
	{
		uint8_t *p = (uint8_t *)q.pool;
		for (int32_t i = 0; i < (int32_t)q.capacity; i++, p += q.node_size)
		{
			if (*p & 1) continue;
			TaskNode *t = (TaskNode *)p;
			*p |= 1;
			t->func = (TaskFn)task_fn;
			t->sequence = 0;
			t->next = nullptr;
			if (q.tail) q.tail->next = t;
			return t;
		}
		return nullptr;
	}

	// ------------------------------------------------------------------
	// Ruby (0x6822F0), one per target, 56 ticks: prim model (0..20), orbit stones (15),
	// beams (15..34), the reflect wall (20..54) with rings (20, 26, 32), damage at 50
	// ------------------------------------------------------------------
	static uint32_t __cdecl RubyTask(TaskNode *n)
	{
		RubyNode *r = (RubyNode *)n;
		// 30 fps layer: see mag278_carbuncle_held.inc
		FX_HELD(held_note_ruby(r);)
		if (r->counter < 0x15)
		{
			PrimCtx ctx;
			CtxRuby(ctx, r);
			prim::play((prim::Layout *)r->prim, (prim::Callback)CB_PrimObject, (int)&ctx, (int)Pause());
			SparkleNode *sp = (SparkleNode *)AddTaskToQueue(&QueueCreature().first, ORIG_SparkleTask);
			if (sp)
			{
				int32_t rnd = SharedRand();
				SpawnPosition(r->target, mul32(rnd, BoneCount(r->target)) >> 15, 0, sp->pos);
				sp->frame = 0;
			}
		}
		RubyMatrices(r, &r->model, &r->view);
		uint32_t k = (uint32_t)(r->counter - 0xF);
		if (k < 0x14)
		{
			if (k == 0)
			{
				int32_t rnd = SharedRand();
				for (int i = 8; i; i--)
				{
					OrbitNode *o = (OrbitNode *)AddTaskToQueue(&QueueOrbits().first, ORIG_OrbitTask);
					if (!o) continue;
					Memset32(&o->counter, 0, 4);
					o->parent = r;
					o->tilt = 0x400;
					o->angle = (int16_t)rnd;
					o->dangle = 0x10;
					if (SharedRand() & 1) o->dangle = (int16_t)-o->dangle;
					o->dtilt = (int16_t)(-0x25 - (SharedRand() & 0x1F));
					rnd += 0x200;
				}
			}
			RubyCenter(r->counter, r->center);
			int32_t ang = SharedRand();
			if (!Pause())
			{
				for (int i = 8; i; i--)
				{
					BeamNode *b = (BeamNode *)AppendByValue(QueueBeams(), ORIG_BeamTask);
					if (!b) continue;
					b->vy = 0;
					b->y = 0;
					b->counter = 0;
					b->parent = r;
					b->angle = (int16_t)ang;
					ang += 0x155;
					b->scale = (int16_t)((SharedRand() & 0x3FF) + 0x200);
				}
			}
		}
		int32_t e = r->counter - 0x14;
		if ((uint32_t)e < 0x23)
		{
			int32_t s = (int32_t)*(int16_t *)(r->target + 0x26) << 2;
			if (s > 0x1000) s = 0x1000;
			uint8_t *h = WallDraw(r->counter, WallHeight(e), WallFade(e), &r->view);
			if (!Pause() && (uint32_t)e < 0x12 && (uint32_t)e % 6 == 0)
			{
				RingNode *g = (RingNode *)AddTaskToQueue(&QueueOrbits().second, ORIG_RingTask);
				if (g)
				{
					Memset32(&g->counter, 0, 4);
					g->parent = r;
					g->height = (int16_t)(mul32(*(int16_t *)(h + 0x5E), s) >> 12);
					g->scale = 0x800;
					g->speed = 0x100;
					g->accel = -8;
				}
			}
			FieldFree(0x60);
		}
		if (Pause()) return 0;
		if (r->counter == 0x32) DamageTarget(r->target);
		if (r->counter >= 0x37) return TASK_END;
		r->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Creature timeline (0x681630)
	// ------------------------------------------------------------------
	static uint32_t __cdecl CreatureTask(TaskNode *n)
	{
		CreatureNode *cn = (CreatureNode *)n;
		// 30 fps layer: see mag278_carbuncle_held.inc
		FX_HELD(held_note_creature(cn);)
		uint8_t *MB = ModelBuffer();

		// screen flash: up over 0..7, down over 276..282
		Flash() = (uint32_t)FlashAt(cn->counter);

		// the model (36..145, 215..235)
		int32_t c = cn->counter;
		if ((uint32_t)(c - 0x24) < 0xC8 && (c <= 0x91 || c >= 0xD7))
		{
			if (!Pause()) AdvanceModelAnim(E(cn));
			// 30 fps layer: see mag278_carbuncle_held.inc
			FX_HELD(held_note_model(cn);)
			FrameCursor() = DrawModel(E(cn), Scratch(), FrameCursor(), var<uint32_t>(0x1D969A8));
		}

		// the ruby dome (1..263 minus 145..230)
		c = cn->counter;
		if (DomeDrawn(c))
		{
			// 30 fps layer: see mag278_carbuncle_held.inc
			FX_HELD(held_note_dome();)
			DomeDraw(c, (int32_t)Flash(), DomeMorph(c));
		}

		// prim stage 1 (110..145)
		int32_t e = cn->counter - 0x6E;
		if ((uint32_t)e < 0x24)
		{
			if (e == 0) Decode(0x10C2608, cn->prim, 0x514);
			PrimCtx ctx;
			CtxStage110(ctx, cn);
			// 30 fps layer: see mag278_carbuncle_held.inc
			FX_HELD(held_note_prim_stage(1);)
			prim::play((prim::Layout *)cn->prim, (prim::Callback)CB_PrimObject, (int)&ctx, (int)Pause());
		}
		// prim stage 2 (145..176)
		e = cn->counter - 0x91;
		if ((uint32_t)e < 0x20)
		{
			if (e == 0) Decode(0x10C5C8C, cn->prim, 0x3D8);
			PrimCtx ctx;
			CtxStage145(ctx);
			// 30 fps layer: see mag278_carbuncle_held.inc
			FX_HELD(held_note_prim_stage(2);)
			prim::play((prim::Layout *)cn->prim, (prim::Callback)CB_PrimObject, (int)&ctx, (int)Pause());
		}

		// streams (the timeline waits while a load is busy)
		if (cn->counter == 0x96)
		{
			if (StreamBusy()) return 0;
			StreamLoad(0x2CD, (uint32_t)(MB + 0x23ECC), 1);
			StreamLoad(0x2CE, (uint32_t)(ModelBuffer() + 0x23ECC), 0);
		}
		BattlePreload();
		switch (cn->counter)
		{
		case 1: PlaySE(0x10C74CC, 0, 0x80); break;
		case 0x56: PlaySE(0x10C74D0, 0, 0x80); break;
		case 0x91: PlaySE(0x10C74D4, 0, 0x80); break;
		case 0xD7: PlaySE(0x10C74D8, 0, 0x80); break;
		default: break;
		}

		// sparkles on the model's bones (86..109, 219..230), 4 a tick
		c = cn->counter;
		if ((uint32_t)(c - 0x56) < 0x18 || (c >= 0xDB && c <= 0xE6))
		{
			for (int k = 4; k; k--)
			{
				SparkleNode *sp = (SparkleNode *)AddTaskToQueue(&QueueCreature().first, ORIG_SparkleTask);
				if (!sp) continue;
				int32_t rnd = SharedRand();
				SpawnPosition(E(cn), mul32(rnd, BoneCount(E(cn))) >> 15, 0, sp->pos);
				sp->frame = 0;
			}
		}

		// one ruby per target, from the last (145, 149, 153)
		e = cn->counter - 0x91;
		if ((uint32_t)e < 0xC && (e & 3) == 0)
		{
			int32_t k = (int32_t)cn->ntargets - (int32_t)((uint32_t)e >> 2) - 1;
			if (k >= 0)
			{
				RubyNode *r = (RubyNode *)AddTaskToQueue(&QueueRubies().first, ORIG_RubyTask);
				if (r)
				{
					Memset32(&r->counter, 0, 0x40);
					uint8_t *t = *(uint8_t **)((uint8_t *)cn + 0x14 + 4 * k);
					r->target = t;
					DefaultPosition(t, r->pos);
					r->model.t[0] = r->pos[0];
					r->model.t[1] = r->pos[1];
					r->model.t[2] = r->pos[2];
					r->pos[1] = 0;
					Decode(0x10C1CD8, r->prim, 0xA8);
				}
			}
		}

		// shards (231..234; waits for the stream at 231)
		e = cn->counter - 0xE7;
		if ((uint32_t)e < 4)
		{
			if (e == 0 && StreamBusy()) return 0;
			ShardNode *s = (ShardNode *)AddTaskToQueue(&QueueRubies().second, ORIG_ShardTask);
			if (s)
			{
				s->pos[0] = (int16_t)((SharedRand() & 0x7F) - 0x40);
				int32_t r2 = SharedRand();
				int16_t x = s->pos[0];
				int16_t z = (int16_t)((r2 & 0x7F) - 0x40);
				s->pos[1] = 0;
				s->pos[2] = z;
				s->frame = 0;
				s->vel[0] = (int16_t)(x >> 2);
				s->vel[2] = (int16_t)(z >> 2);
				s->vel[1] = (int16_t)(-0x40 - (SharedRand() & 0x1F));
				s->variant = (int16_t)(SharedRand() & 1);
				s->pos[2] += (int16_t)Base();
			}
		}

		// animations
		e = cn->counter - 0x24;
		if ((uint32_t)e < 0xC8)
		{
			if (e == 0x4A) ReadAnimationStart(E(cn), 1);
			else if (e == 0x64) ReadAnimationStart(E(cn), 2);
		}
		if (cn->counter == 0x107) StreamWaitDone((uint32_t)(ModelBuffer() + 0x3ECC), &DoneFlag());

		// camera 1..36: cut, then the eye swings in (the enemies are lifted at 2)
		int32_t v = cn->counter - 1;
		if ((uint32_t)v < 0x24)
		{
			uint32_t s = ((uint32_t)v << 12) / 36u;
			if (v == 0)
			{
				CameraArmReturn();
				var<int16_t>(0x1D977A2) = 0; // g_BattleCam_Roll
				CamLookAt(2) = (int16_t)Base();
				var<int16_t>(0x1D8E038) = 0x120;
				CamLookAt(0) = 0;
				CamLookAt(1) = 0;
				CamWorld(0) = 0;
				CamWorld(1) = (int16_t)0xF060;
				CamWorld(2) = (int16_t)(Base() + 0xBB8);
			}
			else if (v == 1)
			{
				int16_t lift = (int16_t)Lift();
				for (uint32_t a = 0x1D974B4; a < 0x1D97724; a += 0x9C) // slots 3..6 (+0x20 height)
					if (var<uint8_t>(a - 0x20) & 2) var<int16_t>(a) += lift;
				PartyHide();
			}
			CamRotate(CamW(), CamL(), ComputeCos((int32_t)s) >> 6, 0xFAA);
			CamWorld(1) += 0x32;
		}
		// 86..109: look at the model's vertex 0xB0 (86..91), swing
		e = cn->counter - 0x56;
		if ((uint32_t)e < 0x18)
		{
			if ((uint32_t)e < 6) VertexPos(E(cn), 0, 0xB0, CamL());
			CamRotate(CamW(), CamL(), -0x20, 0xFA0);
			CamWorld(1) += -30;
		}
		// 122..145: widen, height kept
		e = cn->counter - 0x6E;
		if ((uint32_t)e < 0x24 && (uint32_t)e > 0xB)
		{
			int32_t t = shl32(0xB - e, 12) / 25 + 0x1000;
			int16_t y = CamWorld(1);
			CamRotate(CamW(), CamL(), t >> 5, t / 8 + 0x1000);
			CamWorld(1) = y;
		}
		if (cn->counter == 0x91)
		{
			PartyShow();
			CamLookAt(0) = 0x17E;
			CamLookAt(1) = (int16_t)0xFF64;
			CamWorld(0) = (int16_t)0xF557;
			CamLookAt(2) = (int16_t)(Centroid() - 0x227);
			CamWorld(1) = (int16_t)0xF67B;
			CamWorld(2) = (int16_t)(Centroid() - 0xB6C);
		}
		// 215..230: back on the model (root lowered to the floor at 215)
		e = cn->counter - 0xD7;
		if ((uint32_t)e < 0x10)
		{
			VertexPos(E(cn), 0, 0xB0, CamL());
			if (e == 0)
			{
				PartyHide();
				Root(cn).t[1] = 0;
				CamWorld(0) = 0;
				CamWorld(1) = CamLookAt(1);
				CamWorld(2) = (int16_t)(Base() + 0x7D0);
			}
			CamRotate(CamW(), CamL(), 0, 0xFA0);
		}
		if (cn->counter == 0xE7)
		{
			CamLookAt(0) = 0;
			CamLookAt(2) = (int16_t)Base();
			CamLookAt(1) = (int16_t)0xFED4;
			CamWorld(0) = 0;
			CamWorld(1) = (int16_t)0xFDA8;
			CamWorld(2) = (int16_t)(Base() + 0x7D0);
		}
		// 247..282: rising pull-out
		e = cn->counter - 0xF7;
		if ((uint32_t)e < 0x11B)
		{
			CamRotate(CamW(), CamL(), ComputeSin(shl32(e & 0x1FFFFF, 7)) >> 7, 0x1036);
			CamLookAt(1) += -0x32;
			CamWorld(1) += -30;
		}
		if (cn->counter == 0x117) ReleaseVoice(cn->voice);
		cn->counter++;
		if (cn->counter >= 0x11B)
		{
			Flash() = 0;
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Beam list executor (MAG_278_sub_6811E0 + MAG_278_sub_681270): a per-tick flipbook
	// header on the scratch stack (published in 0x2508118), then every node of the list;
	// finished nodes are freed and unlinked, the tail becomes the last survivor
	// ------------------------------------------------------------------
	static uint8_t *BeamHeaderInit()
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		BeamHeader() = h;
		*(uint32_t *)h = 0x10BCE98;
		const uint8_t *seq = *(const uint8_t **)h;
		*(uint16_t *)(h + 0x92) = 0;
		*(uint16_t *)(h + 0x90) = 0;
		*(uint16_t *)(h + 0x8E) = 0;
		*(uint16_t *)(h + 0x8A) = 0;
		*(uint16_t *)(h + 0x88) = 0;
		*(uint16_t *)(h + 0x86) = 0;
		*(uint16_t *)(h + 0x60) = 0;
		*(uint16_t *)(h + 0x58) = 0;
		*(uint16_t *)(h + 0x50) = 0;
		*(uint16_t *)(h + 0x48) = 0;
		h[0x20] = seq[4];
		h[0x21] = seq[5];
		h[0x22] = seq[6];
		h[0x23] = seq[7];
		return h;
	}

	static uint32_t RunBeams()
	{
		BeamHeaderInit();
		TaskQueue &q = QueueBeams();
		TaskNode *prev = nullptr;
		uint32_t alive = 0;
		for (TaskNode *t = q.head; t; t = t->next)
		{
			void *port = lookup((uint32_t)t->func);
			uint32_t r = (port ? (TaskFn)port : t->func)(t);
			if (r & 2)
			{
				t->flags = 0;
				if (prev) prev->next = t->next;
				else q.head = t->next;
			}
			else
			{
				prev = t;
				alive++;
			}
		}
		q.tail = prev;
		FieldFree(0xB4);
		return alive;
	}

	// ------------------------------------------------------------------
	// Master task (0x680DF0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl SequenceTask(TaskNode *n)
	{
		MasterNode *node = (MasterNode *)n;
		// 30 fps layer: see mag278_carbuncle_held.inc
		FX_HELD(held_note_master();)
		uint8_t *MB = ModelBuffer();
		if (node->parity)
		{
			PacketCursor() = (uint32_t)(MB + 0x3ECC);
			PacketCursor2() = (uint32_t)(MB + 0x13ECC);
			node->parity = 0;
		}
		else
		{
			PacketCursor() = (uint32_t)(MB + 0x13ECC);
			PacketCursor2() = (uint32_t)(MB + 0x23ECC);
			node->parity = 1;
		}
		Flash() = 0;

		if (node->counter == 2 && !Pause() && !node->spawned)
		{
			node->spawned = 1;
			InitTaskQueuePool(&QueueBeams(), ModelBuffer() + 4, 0x1C, 0x100);
			InitTaskQueuePool(&QueueOrbits().first, ModelBuffer() + 0x1C04, 0x1C, 0x18);
			InitTaskQueuePool(&QueueOrbits().second, ModelBuffer() + 0x1EA4, 0x1C, 9);
			InitTaskQueuePool(&QueueRubies().first, ModelBuffer() + 0x1FA0, 0x10C, 3);
			InitTaskQueuePool(&QueueRubies().second, ModelBuffer() + 0x22C4, 0x1C, 8);
			InitTaskQueuePool(&QueueCreature().first, ModelBuffer() + 0x23A4, 0x14, 0x40);
			InitTaskQueuePool(&QueueCreature().second, ModelBuffer() + 0x28A4, 0x5E0, 1);
			int16_t *mirror = (int16_t *)MIRROR, *bbox = (int16_t *)BBOX;
			int16_t floor = (int16_t)(Base() - 200);
			mirror[1] = 0;
			mirror[0] = 0;
			mirror[2] = floor;
			bbox[2] = 0x7FFF; bbox[1] = 0x7FFF; bbox[0] = 0x7FFF;
			bbox[5] = (int16_t)0x8001; bbox[4] = (int16_t)0x8001; bbox[3] = (int16_t)0x8001;
			ModelBBox(0x10C0D08, BBOX);
			MirrorPass(MIRROR, BBOX);
			CreatureNode *cn = (CreatureNode *)AddTaskToQueue(&QueueCreature().second, ORIG_CreatureTask);
			Memset32(&cn->counter, 0, 0x175);
			cn->voice = ClaimVoiceSlot((const void *)0x10C74DC, 1, 0x80);
			BindModelSections(E(cn), cn->sections, 0x10B6F94);
			ReadAnimationStart(E(cn), 0);
			RotY(0x800, &Root(cn));
			Root(cn).t[0] = 0;
			*(int16_t *)(E(cn) + 0x1C) = 0;
			Root(cn).t[1] = -20;
			*(int16_t *)(E(cn) + 0x1E) = -20;
			Root(cn).t[2] = Base();
			*(int16_t *)(E(cn) + 0x20) = (int16_t)Base();
			uint8_t *action = *(uint8_t **)(CastCtx() + 4);
			cn->ntargets = (int16_t)(uint16_t)action[0x10];
			if (cn->ntargets > 0)
			{
				for (int32_t k = 0; k < (int32_t)cn->ntargets; k++)
				{
					uint8_t *act = *(uint8_t **)(CastCtx() + 4);
					uint32_t id = (*(uint8_t **)(act + 8))[0x18 * k];
					*(uint8_t **)((uint8_t *)cn + 0x14 + 4 * k) = (uint8_t *)(0x1D972C0 + 0x9C * id);
				}
			}
			bbox[2] = 0x7FFF; bbox[1] = 0x7FFF; bbox[0] = 0x7FFF;
			bbox[5] = (int16_t)0x8001; bbox[4] = (int16_t)0x8001; bbox[3] = (int16_t)0x8001;
			ModelBBox(0x10BD488, BBOX);
			// reflect wall: bottom and height (+1/16) of its bounding box
			int16_t *mbw = (int16_t *)ModelBuffer();
			mbw[0] = bbox[2];
			mbw[1] = (int16_t)(bbox[5] - bbox[2]);
			mbw[1] = (int16_t)(mbw[1] + (int16_t)(mbw[1] >> 4));
			StreamStateInit(ModelBuffer() + 0x3E84);
		}

		uint32_t creature_left = (uint32_t)n; // the original keeps the node pointer: non-zero
		if (node->spawned)
		{
			creature_left = (uint32_t)ExecuteTaskQueue(&QueueCreature().second);
			ExecuteTaskQueue(&QueueCreature().first);
			ExecuteTaskQueue(&QueueRubies().second);
			ExecuteTaskQueue(&QueueRubies().first);
			ExecuteTaskQueue(&QueueOrbits().first);
			ExecuteTaskQueue(&QueueOrbits().second);
			RunBeams();
		}

		SetScreenFlash(Flash(), 0);
		if (Pause()) return 0;
		if (node->spawned && creature_left == 0 && DoneFlag())
		{
			PartyShow();
			int16_t lift = (int16_t)Lift();
			for (uint32_t a = 0x1D974B4; a < 0x1D97724; a += 0x9C) // the enemies back down
				if (var<uint8_t>(a - 0x20) & 2) var<int16_t>(a) -= lift;
			return TASK_END;
		}
		node->counter++;
		return 0;
	}
}
	void register_mag278_carbuncle()
	{
		register_port(c278::ORIG_SequenceTask, (void *)c278::SequenceTask, "C278 SequenceTask", 278);
		register_port(c278::ORIG_CreatureTask, (void *)c278::CreatureTask, "C278 CreatureTask", 278);
		register_port(c278::ORIG_RubyTask, (void *)c278::RubyTask, "C278 RubyTask", 278);
		register_port(c278::ORIG_SparkleTask, (void *)c278::SparkleTask, "C278 SparkleTask", 278);
		register_port(c278::ORIG_ShardTask, (void *)c278::ShardTask, "C278 ShardTask", 278);
		register_port(c278::ORIG_OrbitTask, (void *)c278::OrbitTask, "C278 OrbitTask", 278);
		register_port(c278::ORIG_RingTask, (void *)c278::RingTask, "C278 RingTask", 278);
		register_port(c278::ORIG_BeamTask, (void *)c278::BeamTask, "C278 BeamTask", 278);
		// 30 fps layer: see mag278_carbuncle_held.inc
		FX_HELD(register_mag278_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag278_carbuncle_held.inc"
#endif
