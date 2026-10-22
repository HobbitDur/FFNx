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

// Effect 317: Acid (Quistis' Blue Magic, kernel.bin blue magic +4; MAG_317_*): Gastric Juice 128 (mag128_gastric_juice.cpp,
// the Gayla attack) adapted, cast by Quistis. Map: work\fx317\w317\pair_317_128.txt / amap_317.txt: setup / loader / _Init,
// PartCallback 0x65FF90, MeltModel 0x65FB90, MeltModelDraw 0x6603D0, the stream spawn 0x660970, drip 0x6608D0, splash 0x660D40, the
// ribbon 0x660DC0 / 0x660F20 identical to 128's up to addresses; 128's own MeltGroup 0x6B80D0, WorldVertices 0x6B8AE0 and
// MeltVertices 0x6B89E0 are CALLED by this module (it has no copy of them; 128 in turn calls 0x660540, the battle-model polygons
// function that lies in this module's range); and:
//   root 0x65F8F0: the magic buffer layout moved by 0x20 (the caster's bind block at + 0x14E8: blended vertex frames + 0x1508,
//     stream points + 0x1708, melt amounts + 0x3F08, packet arenas + 0x4708 / + 0x14708); its counter does not wrap at 10 (one
//     target spawn, at counter 1); at counter 1 it binds the module's animation set (com data 0xFFE6B0) to the caster
//     (BattleEntity_BindExternalAnimSet 0x5022C0, block magic buffer + 0x14E8) and queues her chain 3 (QueueChainTransformation)
//   target 0x65FC30 (128: 0x6B8140): every counter 4 later (layout from 0x19, streams at 0x12, spray at 0x12..0x13, melt 0x21..,
//     damage 0x39, end 0x49); the streams and the spray start at a point of the CASTER's model (0x660240: the point 0x1005AD0
//     {x, y, z, bone} through her bone matrix, rebuilt from her pose; 128: effect bone 3 / 0x14), the streams at half speed
//     (progress 0xA0 per step), the spray thrown 0x20 higher
//   spray 0x660830 (128: 0x6B8B20): sequence 0x10016A8, no roll, falling (y velocity + 0x10 per tick after the move)
//   stream 0x660AD0: its points fade twice as fast (-0x100 above 0x400, else -0x80; 128: -0x80 / -0x40)
//   new 0x660240: the caster point above
//
// Structure (setup MAG_317 0x65F840 -> _Init 0x65F870, file loader 0x65F850 = the texture file named at 0x1005AD8 (mag127.tim,
// 128's); the setup keeps the CASTER's entity, starts camera animation 0x10017A0 and queues the TIM):
//   RootTask (0x65F8F0) - alternates the packet arena (magic buffer + 0x4708 / + 0x14708); on counter 1 sets up the pools
//     (target 1 x 0x268, streams 8 x 0x50, particles 0x80 x 0x20, the 0x100 stream points freed), starts the target task on the
//     action's first target and binds Quistis' cast animation; then the effect-relative camera (0x67AAE0 into 0x25054B8) and
//     the target, stream and particle queues; ends when all three are empty.
//   Target (0x65FC30) - node 0x268, 74 ticks, DRAWS THEN UPDATES:
//     counters 25..: the prim-model layout 0x1000224 at the target's default effect position (taken at the spawn), callback
//       PartCallback (0x65FF90);
//     counters 33..72: the target hidden (entity +0x00 bit 2) and redrawn "melting" (0x6602C0): every vertex lowered by its
//       own random amount (bytes made at the spawn by 0x65FB20) x sin((counter - 33) * 2048 / 40) >> 10; the bit is cleared on
//       every other counter;
//     counter 10: sound 0x1001AC8; counter 18: three acid streams (0x660970) from the caster point to random bones of the
//       target; counters 18, 19: eight spray particles (0x660830) from the caster point around the caster->target direction;
//       even counters 33..51: a drip particle (0x6608D0) at a random bone of the target; counter 57: damage; ends after 73.
//   Stream (0x660AD0) - node 0x50, UPDATES THEN DRAWS: two steps per tick: the progress t (+0xA0) moves along the
//     caster->target line (0x56CB90), lifted by sin(t / 2) / 4 and swirled around it, a new point (0x660A70) drifting by 1/8 of
//     the swirl; at the end a splash particle (0x660D40); every point fades (0x800, -0x100 above 0x400, else -0x80). Then the
//     chain projected and drawn as a textured ribbon (0x660DC0 / 0x660F20), the points drift, faded tail points are freed; ends
//     when fewer than 2 are left.
//   Particles - sprite sequences (frame = counter) through the shadow-rotation camera: spray (0x660830, 0x10016A8, 9 frames,
//     falling, no roll), drip (0x6608D0, 0x100139C, 13 frames, falling), splash (0x660D40, 0x10014FC, 16 frames, fixed). DRAW
//     THEN UPDATE.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// Writes to battle entities: the caster's animation set / chain (0x5022C0 / 0x505C00, root counter 1) and her bone matrices
// (0x660240 rebuilds them from her current pose, target counters 18 / 19); the target entity's flags (+0x00 bit 2) and its bone
// matrices (the melting draw); damage (0x506690). Camera: no camera word is written (the camera animation is the engine's).
// Module globals: 0x2505490..0x2505520 (caster entity, texture file, magic buffer base, context, root pool, packet cursor
// 0x25054B4, effect-relative camera 0x25054B8, a never-written vector 0x25054D8, particle / stream / target / root queues). In the
// magic buffer: the particle pool, the stream pool (+ 0x1000), the target pool (+ 0x1280), the caster's bind block (+ 0x14E8),
// blended vertex frames (+ 0x1508), the stream points (+ 0x1708, 0x100 x 0x28), the melt amounts (+ 0x3F08, one byte per vertex)
// and the packet arenas. The melting target draws into the frame arena (battle_texture_data_ptr 0x1D8E054) with the battle's
// bone workspace as its vertex buffer.

#include "mag_common.h"

namespace ff8fx
{
namespace acid_quistis
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2505490); }         // the caster's entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2505498); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x250549C); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25054B4); }
	inline Mat4x3 &EffectCamera() { return var<Mat4x3>(0x25054B8); }       // camera x effect frame (every tick)
	inline TaskQueue *QParticles() { return (TaskQueue *)0x25054E0; }       // pool: magic buffer, 0x80 x 0x20
	inline TaskQueue *QStreams() { return (TaskQueue *)0x25054F0; }         // pool: + 0x1000, 8 x 0x50
	inline TaskQueue *QTarget() { return (TaskQueue *)0x2505500; }          // pool: + 0x1280, 1 x 0x268
	inline uint32_t &FrameCursor() { return var<uint32_t>(0x1D8E054); }    // battle_texture_data_ptr (frame arena)
	inline uint32_t &BoneWorkspace() { return var<uint32_t>(0x1D98B3C); }  // melting target's vertex buffer
	inline uint32_t RenderOT(uint32_t off) { return var<uint32_t>(0x1D8E04C) + off; }

	static const uint32_t ORIG_RootTask = 0x65F8F0;
	static const uint32_t ORIG_TargetTask = 0x65FC30;
	static const uint32_t ORIG_SprayTask = 0x660830;
	static const uint32_t ORIG_DripTask = 0x6608D0;
	static const uint32_t ORIG_StreamTask = 0x660AD0;
	static const uint32_t ORIG_SplashTask = 0x660D40;
	static const uint32_t MODEL_Layout = 0x1000224;      // prim-model layout data (0x248-byte layout)
	static const uint32_t SEQ_Spray = 0x10016A8;
	static const uint32_t SEQ_Drip = 0x100139C;
	static const uint32_t SEQ_Splash = 0x10014FC;
	static const uint32_t ZERO_Vector = 0x25054D8;       // module global, never written (8 bytes)
	static const void *const SOUND_Juice = (const void *)0x1001AC8;
	static const uint8_t *const DIST_TABLE = (const uint8_t *)0x1001ACC; // [|dy|][|dx|] -> length, 128 x 128
	static const uint32_t POINTS = 0x1708;               // magic buffer: stream points (128: + 0x16E8)
	static const uint32_t MELT = 0x3F08;                 // magic buffer: melt amounts (128: + 0x3EE8)
	static const uint32_t MORPH = 0x1508;                // magic buffer: blended vertex frames (128: + 0x14E8)
	static const uint32_t BIND = 0x14E8;                 // magic buffer: the caster's bind block (0x20 bytes, BattleEntity_BindExternalAnimSet)
	static const uint32_t COM_Anim = 0xFFE6B0;           // the module's animation set (com data, Quistis)
	static const uint32_t CASTER_Point = 0x1005AD0;      // {x, y, z, bone}: the streams' and the spray's start on the caster's model

	// engine functions not in fx_port.h / mag_common.h
	// GetEffectSpawnPosition (0x502170): position of an entity's effect anchor `bone` (x, y, z)
	inline int32_t GetEffectSpawnPosition(uint8_t *entity, int32_t bone, int32_t flag, int16_t *out) { return fn<int32_t (__cdecl *)(uint8_t *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, flag, out); }
	inline uint32_t BindExternalAnimSet(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); } // BattleEntity_BindExternalAnimSet
	inline uint32_t QueueChainTransformation(uint8_t *entity, uint32_t id) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t)>(0x505C00)(entity, id); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t CartesianToGameAngle(int32_t dx, int32_t dz) { return fn<int32_t (__cdecl *)(int32_t, int32_t)>(0x56D160)(dx, dz); }
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	// MAG_017_sub_701390: vertex frames f0 and f1 of a model blended by t (4.12) into out
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline void GteLoadV0u(const void *v) { fn<void (__cdecl *)(const void *)>(0x703FB0)(v); } // MAG_069_sub_703FB0: V0 = 3 x u16
	// sub_56CB90: out = (a * wa + b * wb) >> 12 (int16 x3)
	inline void BlendVectors(const int16_t *a, const int16_t *b, int32_t wa, int32_t wb, int16_t *out) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int16_t *)>(0x56CB90)(a, b, wa, wb, out); }
	inline void MatrixMultiplyVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	// BuildOrthonormalBasis (0x50CBA0): 3x3 basis of the direction v (int32 x3), returns |v|
	inline int32_t BuildOrthonormalBasis(const int32_t *v, Mat4x3 *m) { return fn<int32_t (__cdecl *)(const int32_t *, Mat4x3 *)>(0x50CBA0)(v, m); }
	// sub_5088A0: an entity's ground shadow
	inline uint32_t DrawShadow(uint8_t *entity, uint32_t ot, int32_t mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x5088A0)(entity, ot, mode, cursor); }
	// sub_660540: the engine's battle-model polygons of one object (work block +0x00 polygons,
	// +0x04 projected vertices, +0x14 counts, +0x28 colour)
	inline uint32_t ModelPolygons(uint8_t *s, uint32_t ot, int32_t shift, uint32_t cursor) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, int32_t, uint32_t)>(0x660540)(s, ot, shift, cursor); }

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // pool of 1 node of 0x14 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..9, a target spawn at 1
		uint8_t action;    // +0x0E next action
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct TargetNode // pool of 1 node of 0x268 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C 0..69
		int16_t action;    // +0x0E action index
		uint8_t *entity;   // +0x10 the action's first target
		int16_t angle;     // +0x14 caster -> target direction (game angle)
		int16_t pos[4];    // +0x16 the target's default effect position (x, y, z, height)
		int16_t pad1E;
		uint8_t layout[0x248]; // +0x20 prim-model layout (prim::Layout: data, frame, state)
	};
	struct ParticleNode // pool of 0x80 nodes of 0x20 bytes (spray, drip, splash)
	{
		TaskNode hdr;
		int32_t counter;   // +0x0C sequence frame
		int16_t pos[3];    // +0x10
		int16_t size;      // +0x16
		int16_t vel[3];    // +0x18
		int16_t pad1E;
	};
	struct Point // 0x28 bytes, 0x100 in the magic buffer (+ 0x16E8)
	{
		int16_t pos[3];    // +0x00 world position (GTE V0)
		int16_t width;     // +0x06 ribbon half-width at depth 0; -1 = free
		int16_t vel[3];    // +0x08 drift per tick
		int16_t hold;      // +0x0E 0xA (not read)
		int16_t sx, sy;    // +0x10 projected position, then one edge of the ribbon
		int16_t otz;       // +0x14
		int16_t pad16;
		int16_t sx2, sy2;  // +0x18 other edge
		int16_t pad1C[2];
		int16_t fade;      // +0x20 brightness (0x800 down to 0)
		int16_t pad22;
		Point *next;       // +0x24
	};
	struct StreamNode // pool of 8 nodes of 0x50 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C always 0
		int16_t count;     // +0x0E points in the chain
		Point *head, *tail; // +0x10 oldest, +0x14 newest
		int16_t from[3];   // +0x18 caster's effect bone 3
		int16_t t;         // +0x1E progress 0..0x1000
		int16_t to[3];     // +0x20 a random bone of the target
		int16_t dt;        // +0x26 progress per step (0x100)
		Mat4x3 basis;      // +0x28 basis of the from -> to direction
		int16_t slot;      // +0x48 target slot (>= 0: a splash at the end)
		int16_t angle;     // +0x4A swirl angle (+0x100 per step)
		int16_t width;     // +0x4C ribbon width
		int16_t pad4E;
	};
	// the prim-player callback's parameter block (a stack block of the target task)
	struct PrimArg
	{
		int16_t pos[4];    // +0x00 the target's default effect position (x, y, z, height)
		uint32_t morph;    // +0x08 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x14 && sizeof(TargetNode) == 0x268 && sizeof(ParticleNode) == 0x20, "Acid nodes");
	static_assert(sizeof(Point) == 0x28 && sizeof(StreamNode) == 0x50 && sizeof(PrimArg) == 0xC, "Acid nodes");

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }
	inline Point *Points() { return (Point *)(TexBase() + POINTS); }
	inline uint8_t BoneCount(const uint8_t *entity) { return **(uint8_t *const *)*(uint8_t *const *)(entity + 0x64); }

	// ------------------------------------------------------------------
	// Prim-player callback (0x65FF90): one object of the layout. Record flag 0x1000 = the anchor
	// through the camera, the record's offset through the effect-relative camera (0x25054B8) and
	// the rotation composed with it; else anchor + offset through the camera, rotation composed
	// with the camera. Scale by scale3DMatrix.
	// ------------------------------------------------------------------
	static void __cdecl PartCallback(prim::Layout *l, prim::Record *r, int arg_)
	{
		const PrimArg *arg = (const PrimArg *)arg_;
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
		Mat4x3 m = {};
		RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		if (r->flags & 0x1000)
		{
			// the anchor through the camera, the offset through the effect-relative camera
			GteSetRotMatrixCtrl(&Camera());
			GteLoadV0(arg->pos);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			GteSetRotMatrixCtrl(&EffectCamera());
			GteLoadV0(off);
			GteMVMVA_RotV0();
			int32_t w[3];
			GteReadMAC123(w);
			m.t[0] = (int32_t)((uint32_t)m.t[0] + (uint32_t)w[0]);
			m.t[1] = (int32_t)((uint32_t)m.t[1] + (uint32_t)w[1]);
			m.t[2] = (int32_t)((uint32_t)m.t[2] + (uint32_t)w[2]);
			MatrixMultiply(&EffectCamera(), &m);
		}
		else
		{
			// anchor + offset through the camera, rotation composed with the camera
			off[0] = (int16_t)(off[0] + arg->pos[0]);
			off[1] = (int16_t)(off[1] + arg->pos[1]);
			off[2] = (int16_t)(off[2] + arg->pos[2]);
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
			const int32_t v[3] = { r->scale[0], r->scale[1], r->scale[2] };
			Scale3DMatrix(&m, v);
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
		// header +0x18 (depth offset) is not set: the scratch's value
		PacketCursor() = RenderPrimModel(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0x58);
	}

	// ------------------------------------------------------------------
	// MAG_317_sub_660240: the point v = {x, y, z, bone} of the caster's model in world space: her bone matrices rebuilt from
	// her current pose (0x508C90), bone v[3]'s matrix composed with her entity matrix (+0x40), v through it (GTE, 3 x s16 out)
	// ------------------------------------------------------------------
	static void CasterPoint(const int16_t *v, int16_t *out)
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
	// Melt amounts (0x65FB20 -> 0x65FB90 -> 0x6B80D0): one byte per vertex of the target's models,
	// -((rand & 0x1F) + 0x10) * (the vertex's world y) >> 10, in model order
	// ------------------------------------------------------------------
	// MAG_128_sub_6B80D0 (128's own function, called by this module: it has no copy of it): one bone group (header +0x04 vertex cursor, +0x08 byte cursor)
	static void MeltGroup(uint8_t *h)
	{
		const uint8_t *v = F<const uint8_t *>(h, 4);
		uint8_t *out = F<uint8_t *>(h, 8);
		const int32_t n = *(const int16_t *)v;
		v += 2;
		if (n != 0)
		{
			uint32_t left = (uint32_t)n;
			do
			{
				GteLoadV0u(v);
				GteMVMVA_RotV0Tr();
				v += 6;
				int32_t y;
				GteReadDataRegTo(&y, 10); // IR2
				const int32_t r = CrtRand();
				*out = (uint8_t)(0 - (uint8_t)(mul32((r & 0x1F) + 0x10, y) >> 10));
				out++;
			} while (--left != 0);
		}
		F<uint8_t *>(h, 8) = out;
		F<const uint8_t *>(h, 4) = v;
	}

	// MAG_317_sub_65FB90: every object's bone groups of a model (no object mask)
	static void MeltModel(uint8_t *h, uint8_t *anim)
	{
		const uint8_t *model = *(const uint8_t *const *)(anim + 4);
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const uint32_t *offs = (const uint32_t *)(*(const uint8_t *const *)(model + 4));
		int32_t count = *(const int32_t *)offs;
		offs++;
		if (count <= 0) return;
		do
		{
			const uint8_t *obj = *(const uint8_t *const *)(model + 4) + *offs++;
			const int32_t groups = *(const int16_t *)obj;
			F<const uint8_t *>(h, 4) = obj + 2;
			if (groups != 0)
			{
				uint32_t left = (uint32_t)groups;
				do
				{
					const uint8_t *p = F<const uint8_t *>(h, 4);
					const int32_t bone = *(const int16_t *)p;
					F<const uint8_t *>(h, 4) = p + 2;
					const Mat4x3 *m = (const Mat4x3 *)(bones + (uint32_t)mul32(bone, 0x30) + 0x10);
					GteSetRotMatrix(m);
					GteSetTransVector(m);
					MeltGroup(h);
				} while (--left != 0);
			}
		} while (--count != 0);
	}

	// MAG_317_sub_65FB20: the target's melt amounts (body +0x60, then the second model +0x78)
	static void InitMelt(uint8_t *entity)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0xC);
		F<uint32_t>(h, 8) = TexBase() + MELT;
		ComputeBonesWorldMatrices(entity + 0x60, entity + 0x40);
		MeltModel(h, entity + 0x60);
		BuildBoneMatricesFromPose(entity + 0x60);
		uint8_t *second = *(uint8_t *const *)(entity + 0x78);
		if (second)
		{
			ComputeBonesWorldMatrices(second, entity + 0x40);
			MeltModel(h, second);
			BuildBoneMatricesFromPose(second);
		}
		FieldFree(0xC);
	}

	// ------------------------------------------------------------------
	// Melting target (0x6602C0 -> 0x6603D0 -> 0x6B8AE0 / 0x6B89E0 / engine 0x660540)
	// ------------------------------------------------------------------
	// Work block (Field_Alloc 0x84), S = +0x20 (the engine's model-draw block for 0x660540):
	//   S+0x00 polygon cursor, +0x04 vertex buffer (bone workspace), +0x08 vertex cursor,
	//   +0x0C melt-amount cursor, +0x10 object cursor, +0x14 polygon counts (6 words),
	//   +0x20 clip rectangle (x0, y0, x1, y1), +0x28 colour (entity +0x28), +0x2C object mask
	//   (entity +0x7C, the second model -1), +0x30 screen xy, +0x34 OTZ / clip flags (+0x36),
	//   +0x60 the melt amplitude (sin).
	// sub_6B8AE0 (128's own function, called by this module): one bone group of model vertices to world space (into the vertex cursor; the
	// cursors are left for 0x6B89E0)
	static void WorldVertices(uint8_t *s)
	{
		const uint8_t *v = F<const uint8_t *>(s, 0x10);
		uint8_t *out = F<uint8_t *>(s, 8);
		const int32_t n = *(const int16_t *)v;
		v += 2;
		if (n == 0) return;
		uint32_t left = (uint32_t)n;
		do
		{
			GteLoadV0u(v);
			GteMVMVA_RotV0Tr();
			v += 6;
			GteStoreIR123(out);
			out += 8;
		} while (--left != 0);
	}

	// sub_6B89E0 (128's own function, called by this module): the same vertices lowered by their melt amounts, projected with the camera,
	// clip-flagged against the rectangle (x 8)
	static void MeltVertices(uint8_t *s)
	{
		const uint8_t *v = F<const uint8_t *>(s, 0x10);
		uint8_t *out = F<uint8_t *>(s, 8);
		const int32_t n = *(const int16_t *)v;
		v += 2;
		if (n == 0)
		{
			F<uint8_t *>(s, 8) = out;
			F<const uint8_t *>(s, 0x10) = v;
			return;
		}
		const uint8_t *end = v + (uint32_t)mul32(n, 6);
		uint32_t left = (uint32_t)n;
		do
		{
			const uint8_t *melt = F<const uint8_t *>(s, 0xC);
			*(int16_t *)(out + 2) = (int16_t)(*(const int16_t *)(out + 2) + (int16_t)(mul32(*melt, F<int16_t>(s, 0x60)) >> 10));
			F<const uint8_t *>(s, 0xC) = melt + 1;
			GteLoadV0(out);
			GteRTPS();
			GteReadSXY2(s + 0x30);
			GteReadOTZ(s + 0x34);
			if (F<uint16_t>(s, 0x34) == 0)
				*(uint16_t *)(out + 6) = 0x10;
			else
			{
				const int32_t sx = F<int16_t>(s, 0x30);
				if (sx < shl32(F<int16_t>(s, 0x20), 3)) s[0x36] |= 1;
				else if (sx >= shl32(F<int16_t>(s, 0x24), 3)) s[0x36] |= 2;
				const int32_t sy = F<int16_t>(s, 0x32);
				if (sy < shl32(F<int16_t>(s, 0x22), 3)) s[0x36] |= 4;
				else if (sy >= shl32(F<int16_t>(s, 0x26), 3)) s[0x36] |= 8;
				*(uint32_t *)out = F<uint32_t>(s, 0x30);
				*(uint32_t *)(out + 4) = F<uint32_t>(s, 0x34);
			}
			out += 8;
		} while (--left != 0);
		F<uint8_t *>(s, 8) = out;
		F<const uint8_t *>(s, 0x10) = end;
	}

	// MAG_317_sub_6603D0: every object of a model the mask shows (bone groups of vertices, then
	// the engine's polygons)
	static uint32_t MeltModelDraw(const uint8_t *model, uint8_t *s, uint32_t ot, int32_t shift, uint32_t cursor)
	{
		const uint8_t *bones = *(const uint8_t *const *)model + 0x10;
		const uint32_t *offs = (const uint32_t *)(*(const uint8_t *const *)(model + 4));
		const int32_t count = *(const int32_t *)offs;
		offs++;
		if (count <= 0) return cursor;
		for (int32_t k = 0; k < count; k++)
		{
			F<const uint8_t *>(s, 0x10) = *(const uint8_t *const *)(model + 4) + offs[k];
			if (!(F<uint32_t>(s, 0x2C) & (1u << (k & 31)))) continue;
			F<uint32_t>(s, 8) = F<uint32_t>(s, 4);
			const uint8_t *obj = F<const uint8_t *>(s, 0x10);
			int32_t groups = *(const int16_t *)obj;
			F<const uint8_t *>(s, 0x10) = obj + 2;
			if (groups > 0)
			{
				do
				{
					const uint8_t *p = F<const uint8_t *>(s, 0x10);
					const int32_t bone = *(const int16_t *)p;
					F<const uint8_t *>(s, 0x10) = p + 2;
					const Mat4x3 *m = (const Mat4x3 *)(bones + (uint32_t)mul32(bone, 0x30) + 0x10);
					GteSetRotMatrixCtrl(m);
					GteSetTransVectorCtrl(m);
					WorldVertices(s);
					GteSetRotMatrixCtrl(&Camera());
					GteSetTransVectorCtrl(&Camera());
					MeltVertices(s);
				} while (--groups != 0);
			}
			const uint8_t *p = (const uint8_t *)(((uint32_t)F<const uint8_t *>(s, 0x10) + 3) & ~3u);
			for (int w = 0; w < 6; w++)
			{
				F<uint16_t>(s, 0x14 + 2 * w) = *(const uint16_t *)p;
				p += 2;
			}
			F<const uint8_t *>(s, 0x10) = p;
			F<const uint8_t *>(s, 0) = p;
			cursor = ModelPolygons(s, ot, shift, cursor);
		}
		return cursor;
	}

	// MAG_317_sub_6602C0: shadow, then the body (+0x64) and the second model (+0x78) melted by sinv
	static void DrawMeltingTarget(uint8_t *entity, int32_t sinv)
	{
		uint8_t *h = (uint8_t *)FieldAlloc(0x84);
		F<int16_t>(h, 0x80) = (int16_t)sinv;
		F<uint32_t>(h, 0x2C) = TexBase() + MELT;
		FrameCursor() = DrawShadow(entity, RenderOT(0x4040), 0x10, FrameCursor());
		ComputeBonesWorldMatrices(entity + 0x60, entity + 0x40);
		F<int16_t>(h, 0x44) = 0x140;
		F<uint32_t>(h, 0x24) = BoneWorkspace();
		F<int16_t>(h, 0x40) = 0;
		F<int16_t>(h, 0x42) = 0;
		F<int16_t>(h, 0x46) = 0xD8;
		F<uint32_t>(h, 0x48) = *(const uint32_t *)(entity + 0x28);
		F<uint32_t>(h, 0x4C) = *(const uint32_t *)(entity + 0x7C);
		FrameCursor() = MeltModelDraw(*(const uint8_t *const *)(entity + 0x64), h + 0x20, RenderOT(0x44), 0, FrameCursor());
		BuildBoneMatricesFromPose(entity + 0x60);
		uint8_t *second = *(uint8_t *const *)(entity + 0x78);
		if (second)
		{
			F<uint32_t>(h, 0x4C) = 0xFFFFFFFF;
			ComputeBonesWorldMatrices(second, entity + 0x40);
			FrameCursor() = MeltModelDraw(*(const uint8_t *const *)(second + 4), h + 0x20, RenderOT(0x44), 0, FrameCursor());
			BuildBoneMatricesFromPose(second);
		}
		FieldFree(0x84);
	}

	// the melt amplitude at step e (0..39): sin(e * 2048 / 40)
	static int32_t MeltSin(int32_t e)
	{
		return ComputeSin((int32_t)(uint32_t)(((uint64_t)((uint32_t)e << 11) * 0xCCCCCCCDu) >> 37));
	}

	// ------------------------------------------------------------------
	// Particles (0x660830 spray, 0x6608D0 drip, 0x660D40 splash): a sprite sequence through the
	// shadow-rotation camera (scale = size, roll = -size / 4), frame = counter
	// ------------------------------------------------------------------
	static void ParticleDraw(const ParticleNode *p, uint32_t seq)
	{
		// (the spray 0x660830: no roll; 128's spray: -size / 4 as the others)
		TransformCameraByShadowRotation(p->pos, p->size, seq == SEQ_Spray ? 0 : -((int32_t)p->size >> 2));
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		F<uint32_t>(h, 0) = seq;
		F<int16_t>(h, 4) = (int16_t)p->counter;
		F<int16_t>(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(0x44), 2, PacketCursor());
		FieldFree(0xB4);
	}
	// spray: falling (y velocity + 0x10 per tick, after the move; 128: linear)
	static void SprayStep(ParticleNode *p)
	{
		p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
		p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
		p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
		p->vel[1] = (int16_t)(p->vel[1] + 0x10);
	}
	// drip: falling (y velocity + 1 per tick, applied first)
	static void DripStep(ParticleNode *p)
	{
		p->pos[0] = (int16_t)(p->pos[0] + p->vel[0]);
		p->pos[2] = (int16_t)(p->pos[2] + p->vel[2]);
		p->vel[1]++;
		p->pos[1] = (int16_t)(p->pos[1] + p->vel[1]);
	}

	// ------------------------------------------------------------------
	// Streams
	// ------------------------------------------------------------------
	// MAG_317_sub_660A70: a free point (+0x06 == -1), at pos (8 bytes copied), fade 0x800
	static Point *AllocPoint(const int16_t *pos, int16_t width)
	{
		Point *p = Points();
		int n = 0x100;
		while ((uint16_t)p->width != 0xFFFF)
		{
			p++;
			if (--n == 0) return nullptr;
		}
		memcpy(p->pos, pos, 8);
		p->width = width;
		p->hold = 0xA;
		p->fade = 0x800;
		p->next = nullptr;
		return p;
	}

	// MAG_317_sub_660970: a stream task from `from` to `to` with its first point (the 4th words
	// of both records are replaced: t, dt)
	static Point *SpawnStream(const int16_t *from, const int16_t *to, int32_t slot, int32_t t, int32_t dt, int32_t width, int32_t angle)
	{
		Point *p = AllocPoint(from, (int16_t)width);
		if (!p) return nullptr;
		StreamNode *s = (StreamNode *)AddTaskToQueue(QStreams(), ORIG_StreamTask);
		if (!s)
		{
			p->width = -1;
			return nullptr;
		}
		Memset32(&s->counter, 0, 0x11);
		s->counter = 0;
		memcpy(s->from, from, 8);
		memcpy(s->to, to, 8);
		s->t = (int16_t)t;
		s->dt = (int16_t)dt;
		s->angle = (int16_t)angle;
		s->tail = p;
		s->head = p;
		s->count = 1;
		s->slot = (int16_t)slot;
		s->width = (int16_t)width;
		const int32_t d[3] = { (int32_t)to[0] - from[0], (int32_t)to[1] - from[1], (int32_t)to[2] - from[2] };
		BuildOrthonormalBasis(d, &s->basis);
		return p;
	}

	// sub_660F20 (Quezacotl's 0x6C8620 on its own length table): the two edge vertices of point p
	// from the screen direction towards n (averaged with the previous one: mitred joints),
	// half width = n's width * 32 / (p's depth + 0x200); a point without n has no width
	static void RibbonEdges(Point *p, const Point *n, const int16_t *prev, int16_t *dir)
	{
		if (p->otz < 0) return;
		int32_t ox = 0, oy = 0;
		if (n)
		{
			if (n->otz < 0) return;
			const int32_t dx = n->sx - p->sx;
			const int32_t ndy = p->sy - n->sy;
			int32_t adx = dx < 0 ? -dx : dx, ady = ndy < 0 ? -ndy : ndy;
			if (adx >= 0x80) adx = 0x7F;
			if (ady >= 0x80) ady = 0x7F;
			const int32_t len = DIST_TABLE[ady * 128 + adx];
			int32_t px, py; // unit normal (4.12)
			if (prev)
			{
				if (len == 0)
				{
					dir[0] = prev[0];
					dir[1] = prev[1];
					px = prev[0];
					py = prev[1];
				}
				else
				{
					const int32_t cx = shl32(ndy, 12) / len, cy = shl32(dx, 12) / len;
					int32_t sx = cx + prev[0], sy = cy + prev[1];
					dir[0] = (int16_t)cx;
					dir[1] = (int16_t)cy;
					if (sx == 0 && sy == 0)
					{
						sx = cx * 2;
						sy = cy * 2;
					}
					const int32_t sq = (int32_t)((uint32_t)mul32(sx, sx) + (uint32_t)mul32(sy, sy)); // wraps, as the original
					const int32_t k = 0x4000 - (sq >> 13); // one Newton step of 1/|v|
					px = mul32(k, sx) >> 13;
					py = mul32(k, sy) >> 13;
				}
			}
			else
			{
				if (len == 0)
				{
					p->otz = -1;
					return;
				}
				px = shl32(ndy, 12) / len;
				py = shl32(dx, 12) / len;
				dir[0] = (int16_t)px;
				dir[1] = (int16_t)py;
			}
			const int32_t w = shl32(n->width, 5) / (p->otz + 0x200);
			ox = mul32(w, px) >> 12;
			oy = mul32(w, py) >> 12;
		}
		const int16_t x = p->sx, y = p->sy;
		p->sx = (int16_t)(x - ox);
		p->sx2 = (int16_t)(x + ox);
		p->sy2 = (int16_t)(y + oy);
		p->sy = (int16_t)(y - oy);
	}

	// sub_660DC0: the edges of the chain, then one GP0 0x3E quad (gouraud, textured,
	// semi-transparent) per pair of visible points, grey level = fade / 16.
	// UNINITIALISED READ 0x660DC3: the joint direction is a stack local the function never
	// initialises; when the first point is hidden (or its segment has no screen length) the second
	// point reads it. Its slot (stream task entry esp - 0x38) is the high dword of the last double
	// GTE_RTPS (0x45FAA0, fstp 0x45FB89) stored in its frame during the stream's projection loop,
	// which always runs right before: 0x43300000 (the 2^52 + 2^31 rounding bias).
	static const uint32_t RIBBON_DIR0 = 0x43300000;
	static void DrawRibbon(Point *head, int32_t count)
	{
		int16_t dir[2] = { (int16_t)(RIBBON_DIR0 & 0xFFFF), (int16_t)(RIBBON_DIR0 >> 16) };
		RibbonEdges(head, head->next, nullptr, dir);
		Point *p = head->next;
		for (int32_t i = count - 2; i != 0; i--)
		{
			RibbonEdges(p, p->next, dir, dir);
			p = p->next;
		}
		RibbonEdges(p, nullptr, dir, nullptr);
		Point *a = head;
		for (int32_t i = count - 1; i != 0; i--)
		{
			Point *b = a->next;
			if (a->otz > 0 && b->otz > 0)
			{
				uint32_t *pk = (uint32_t *)PacketCursor();
				PacketCursor() += 0x34;
				pk[2] = *(const uint32_t *)&a->sx;
				pk[5] = *(const uint32_t *)&a->sx2;
				pk[3] = 0x3D14E048;  // uv0 + CLUT
				pk[6] = 0x00BAE078;  // uv1 + texture page
				*(uint16_t *)((uint8_t *)pk + 0x24) = 0xE048; // uv2
				*(uint16_t *)((uint8_t *)pk + 0x30) = 0xE078; // uv3
				pk[8] = *(const uint32_t *)&b->sx;
				pk[11] = *(const uint32_t *)&b->sx2;
				uint32_t ca = (uint32_t)(int32_t)(int16_t)(a->fade >> 4);
				ca = (((ca << 8) | ca) << 8) | ca;
				pk[4] = ca;
				pk[1] = ca;
				uint32_t cb = (uint32_t)(int32_t)(int16_t)(b->fade >> 4);
				cb = (((cb << 8) | cb) << 8) | cb;
				pk[0] = 0x0C000000;
				((uint8_t *)pk)[7] = 0x3E;
				pk[10] = cb;
				pk[7] = cb;
				InsertPrimAutoDepth(RenderOT(0x44) + 4 * ((a->otz + b->otz) >> 3), pk);
			}
			a = b;
		}
	}

	// the chain's projection (camera), then its ribbon (2 points or more)
	static void StreamDraw(Point *head, int32_t count)
	{
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		Point *p = head;
		do
		{
			GteLoadV0(p);
			GteRTPS();
			GteReadSXY2(&p->sx);
			GteReadOTZ(&p->otz);
			p = p->next;
		} while (p);
		if (count >= 2) DrawRibbon(head, count);
	}

	// one fade step of a point (two per tick; 128: -0x80 / -0x40)
	static inline int16_t FadeStep(int16_t f)
	{
		if (f <= 0) return f;
		f = (int16_t)(f + (f > 0x400 ? -0x100 : -0x80));
		return f > 0 ? f : 0;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag317_acid_quistis_held.h"
#endif

namespace ff8fx
{
namespace acid_quistis
{
	// ------------------------------------------------------------------
	// Particle tasks
	// ------------------------------------------------------------------
	static uint32_t __cdecl SprayTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		// 30 fps layer: see mag317_acid_quistis_held.inc
		FX_HELD(held_note_particle(ORIG_SprayTask, p);)
		ParticleDraw(p, SEQ_Spray);
		SprayStep(p);
		p->counter++;
		return p->counter < 9 ? 0 : TASK_END;
	}

	static uint32_t __cdecl DripTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		// 30 fps layer: see mag317_acid_quistis_held.inc
		FX_HELD(held_note_particle(ORIG_DripTask, p);)
		ParticleDraw(p, SEQ_Drip);
		DripStep(p);
		p->counter++;
		return p->counter < 0xD ? 0 : TASK_END;
	}

	static uint32_t __cdecl SplashTask(TaskNode *n)
	{
		ParticleNode *p = (ParticleNode *)n;
		// 30 fps layer: see mag317_acid_quistis_held.inc
		FX_HELD(held_note_particle(ORIG_SplashTask, p);)
		ParticleDraw(p, SEQ_Splash);
		p->counter++;
		return p->counter < 0x10 ? 0 : TASK_END;
	}

	// ------------------------------------------------------------------
	// Stream (0x660AD0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl StreamTask(TaskNode *n)
	{
		StreamNode *s = (StreamNode *)n;
		for (int iter = 2; iter != 0; iter--)
		{
			if (s->t < 0x1000)
			{
				s->t = (int16_t)(s->dt + s->t);
				// on the line, lifted by sin(t / 2) / 4 (4th word never written: copied into the
				// point's width / the splash's size, both overwritten)
				int16_t at[4] = { 0, 0, 0, 0 };
				BlendVectors(s->from, s->to, 0x1000 - s->t, s->t, at);
				const int32_t sn = ComputeSin(s->t >> 1);
				at[1] = (int16_t)(at[1] - (int16_t)((int16_t)sn >> 2));
				// swirl around the line, radius sin / 16
				const int32_t radius = (int16_t)((int16_t)sn >> 4);
				int16_t v[4];
				v[0] = (int16_t)(mul32(ComputeSin(s->angle), radius) >> 12);
				v[1] = (int16_t)(mul32(ComputeCos(s->angle), radius) >> 12);
				v[2] = 0;
				v[3] = 0;
				MatrixMultiplyVector(&s->basis, v, v);
				s->angle = (int16_t)(s->angle + 0x100);
				at[0] = (int16_t)(at[0] + v[0]);
				at[1] = (int16_t)(at[1] + v[1]);
				at[2] = (int16_t)(at[2] + v[2]);
				if (s->t >= 0x1000 && s->slot >= 0)
				{
					ParticleNode *q = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_SplashTask);
					if (q)
					{
						q->counter = 0;
						memcpy(q->pos, at, 8);
						q->size = (int16_t)(s->width << 4);
					}
				}
				Point *p = AllocPoint(at, s->width);
				if (!p) s->t = 0x1000;
				else
				{
					s->count++;
					s->tail->next = p;
					s->tail = p;
					p->vel[0] = (int16_t)(v[0] >> 3);
					p->vel[1] = (int16_t)(v[1] >> 3);
					p->vel[2] = (int16_t)(v[2] >> 3);
				}
			}
			Point *q = s->head;
			do
			{
				if (q->fade > 0) q->fade = FadeStep(q->fade);
				q = q->next;
			} while (q);
		}
		// 30 fps layer: see mag317_acid_quistis_held.inc
		FX_HELD(held_note_stream(s);)
		StreamDraw(s->head, s->count);
		Point *q = s->head;
		do
		{
			q->pos[0] = (int16_t)(q->pos[0] + q->vel[0]);
			q->pos[1] = (int16_t)(q->pos[1] + q->vel[1]);
			q->pos[2] = (int16_t)(q->pos[2] + q->vel[2]);
			q = q->next;
		} while (q);
		// faded tail pairs are freed (the last point of an ended chain stays allocated)
		Point *h = s->head;
		if (h->fade != 0) return 0;
		for (;;)
		{
			Point *nx = h->next;
			if (nx->fade != 0) return 0;
			h->width = -1;
			s->count--;
			s->head = nx;
			if (s->count < 2) return TASK_END;
			h = nx;
			if (h->fade != 0) return 0;
		}
	}

	// ------------------------------------------------------------------
	// Target (0x65FC30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *t = (TargetNode *)n;
		if (t->counter >= 0x19)
		{
			PrimArg arg;
			memcpy(arg.pos, t->pos, 8);
			arg.morph = TexBase() + MORPH;
			// 30 fps layer: see mag317_acid_quistis_held.inc
			FX_HELD(held_note_play(t, &arg);)
			prim::play((prim::Layout *)t->layout, PartCallback, (int)&arg, 0);
		}
		const uint32_t e = (uint32_t)((int32_t)t->counter - 0x21);
		if (e < 0x28)
		{
			t->entity[0] |= 4;
			const int32_t sinv = MeltSin((int32_t)e);
			// 30 fps layer: see mag317_acid_quistis_held.inc
			FX_HELD(held_note_melt(t->entity, (int32_t)e);)
			DrawMeltingTarget(t->entity, sinv);
		}
		else *(uint16_t *)t->entity &= 0xFFFB;
		if (t->counter == 0xA) BdPlaySE(SOUND_Juice, 0, 0x80);
		if (t->counter == 0x12)
		{
			// three streams from the caster point 0x1005AD0 (0x660240) to random bones of the target
			for (int32_t w = 0xFFF; w != 0; w -= 0x555)
			{
				int16_t from[4], anchor[4], to[4];
				CasterPoint((const int16_t *)CASTER_Point, from);
				GetDefaultEffectPosition(Caster(), anchor);
				uint8_t *entity = t->entity;
				const int32_t r = CrtRand();
				GetEffectSpawnPosition(entity, mul32(r, BoneCount(entity)) >> 15, 0, to);
				const int32_t slot = (int32_t)((uint32_t)t->entity - 0x1D972C0u) / 0x9C;
				Point *p = SpawnStream(from, to, slot, 0, 0xA0, 0x96, w);
				if (p)
				{
					*(uint32_t *)p->vel = var<uint32_t>(ZERO_Vector);
					*(uint32_t *)&p->vel[2] = var<uint32_t>(ZERO_Vector + 4);
				}
			}
		}
		if ((uint32_t)((int32_t)t->counter - 0x12) < 2)
		{
			// eight spray particles from the caster point 0x1005AD0 (0x660240), +-1/8 turn around the direction
			for (int k = 8; k != 0; k--)
			{
				ParticleNode *q = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_SprayTask);
				if (!q) continue;
				CasterPoint((const int16_t *)CASTER_Point, q->pos);
				q->counter = 0;
				q->size = (int16_t)((CrtRand() & 0x3FF) + 0x400);
				const int16_t a = (int16_t)((uint32_t)(CrtRand() & 0x3FF) + (uint16_t)t->angle - 0x200);
				const int32_t speed = (int16_t)((CrtRand() & 0x3F) + 0x40);
				q->vel[0] = (int16_t)(mul32(ComputeSin(a), speed) >> 12);
				q->vel[2] = (int16_t)(mul32(ComputeCos(a), speed) >> 12);
				q->vel[1] = (int16_t)(-0x20 - (CrtRand() & 0x3F));
			}
		}
		const uint32_t d = (uint32_t)((int32_t)t->counter - 0x21);
		if (d < 0x14 && !(d & 1))
		{
			// a drip at a random bone of the target (smaller from the 10th)
			ParticleNode *q = (ParticleNode *)AddTaskToQueue(QParticles(), ORIG_DripTask);
			if (q)
			{
				uint8_t *entity = t->entity;
				const int32_t r = CrtRand();
				GetEffectSpawnPosition(entity, mul32(r, BoneCount(entity)) >> 15, 0, q->pos);
				q->counter = 0;
				q->size = (int16_t)((CrtRand() & 0x7FF) + 0x800);
				if (d >= 0xA) q->size = (int16_t)(uint32_t)(((uint64_t)(uint32_t)mul32(0x1E - (int32_t)d, q->size) * 0xCCCCCCCDu) >> 36);
				const int16_t a = (int16_t)((uint32_t)(CrtRand() & 0x3FF) + (uint16_t)t->angle - 0x200);
				const int32_t speed = (int16_t)(CrtRand() & 0x1F);
				q->vel[0] = (int16_t)(mul32(ComputeSin(a), speed) >> 12);
				q->vel[2] = (int16_t)(mul32(ComputeCos(a), speed) >> 12);
				q->vel[1] = (int16_t)(-0x10 - (CrtRand() & 0x1F));
			}
		}
		if (t->counter == 0x39) ApplyActionResultToTarget(Ctx()->actions[t->action].targets);
		if (t->counter >= 0x49) return TASK_END;
		t->counter++;
		return 0;
	}

	// ------------------------------------------------------------------
	// Root task (0x65F8F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		// 30 fps layer: see mag317_acid_quistis_held.inc
		FX_HELD(held_note_root();)
		if (r->arena)
		{
			PacketCursor() = TexBase() + 0x4708;
			r->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + 0x14708;
			r->arena = 1;
		}
		if (r->counter == 1)
		{
			if (!r->started)
			{
				r->started = 1;
				InitTaskQueuePool(QTarget(), (void *)(TexBase() + 0x1280), 0x268, 1);
				InitTaskQueuePool(QStreams(), (void *)(TexBase() + 0x1000), 0x50, 8);
				InitTaskQueuePool(QParticles(), (void *)TexBase(), 0x20, 0x80);
				Point *p = Points();
				for (int i = 0; i < 0x100; i++) p[i].width = -1;
			}
			if (r->action <= Ctx()->actions[0].last_action)
			{
				// the pool's one node (one action: the counter never comes back to 1)
				TargetNode *t = (TargetNode *)AddTaskToQueue(QTarget(), ORIG_TargetTask);
				Memset32(&t->counter, 0, 0x97);
				t->action = (int16_t)(uint16_t)r->action;
				uint8_t *entity = Entity(Ctx()->actions[t->action].targets[0]);
				t->entity = entity;
				GetDefaultEffectPosition(entity, t->pos);
				int16_t caster[4];
				GetDefaultEffectPosition(Caster(), caster);
				t->angle = (int16_t)CartesianToGameAngle((int32_t)t->pos[0] - caster[0], (int32_t)t->pos[2] - caster[2]);
				DecodeModelPrimLayout(MODEL_Layout, t->layout, 0x248);
				r->action++;
				InitMelt(t->entity);
			}
			// Quistis' cast animation: the module's animation set bound to her (block magic buffer + 0x14E8), chain 3
			BindExternalAnimSet(Caster(), TexBase() + BIND, COM_Anim);
			QueueChainTransformation(Caster(), 3);
		}
		int a, b, c;
		if (r->started)
		{
			EffectCameraMatrix(&Camera(), &EffectCamera());
			a = ExecuteTaskQueue(QTarget());
			b = ExecuteTaskQueue(QStreams());
			c = ExecuteTaskQueue(QParticles());
		}
		else a = b = c = (int)n;
		if (r->started && a == 0 && b == 0 && c == 0) return TASK_END;
		r->counter++; // (no 10-tick cycle: 128's root wraps it at 10)
		return 0;
	}
}

	void register_mag317_acid_quistis()
	{
		register_port(acid_quistis::ORIG_RootTask, (void *)acid_quistis::RootTask, "317 RootTask", 317);
		register_port(acid_quistis::ORIG_TargetTask, (void *)acid_quistis::TargetTask, "317 TargetTask", 317);
		register_port(acid_quistis::ORIG_StreamTask, (void *)acid_quistis::StreamTask, "317 StreamTask", 317);
		register_port(acid_quistis::ORIG_SprayTask, (void *)acid_quistis::SprayTask, "317 SprayTask", 317);
		register_port(acid_quistis::ORIG_DripTask, (void *)acid_quistis::DripTask, "317 DripTask", 317);
		register_port(acid_quistis::ORIG_SplashTask, (void *)acid_quistis::SplashTask, "317 SplashTask", 317);
		// 30 fps layer: see mag317_acid_quistis_held.inc
		FX_HELD(register_mag317_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag317_acid_quistis_held.inc"
#endif
