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

// Effect 139: Sonic Wave (enemy attack 182 of kernel.bin, Oilboyle c0m099; MAG_139_*). No other
// kernel.bin entry uses effect 139.
//
// Structure (setup MAG_139_SONIC_WAVE 0x6A8830 -> _Init 0x6A8860, file loader 0x6A8840 = the
// texture file named at 0x11DF4D4; the _Init copies the module's data block 0x11D4E90 (0x9080
// bytes: the wave mesh, the wave frames, layouts, camera animation) into 0x2517B5C, keeps the
// CASTER's entity (0x2517B50) and the cast context (0x2520BDC), starts camera animation 0x11DD21C
// and queues the TIM):
//   MasterTask (0x6A88F0) - alternates the packet arena (magic buffer + 0x1EF0 / + 0x11EF0); on
//     counter 1 prepares the mesh copy 0x2517B5C (0x6A9DA0: normalised, its triangles turned to
//     start at their leftmost vertex and indexed for 12-byte vertices), normalises the wave frames
//     0x11D85A0 / 0x11D7F50 / 0x11D7900 (0x6A9C80: every vertex's packed word = its length and
//     direction; these static exe tables are rewritten on EVERY cast, with the same values; frame
//     0x11D72B0 is not), sets up the pools (targets 4 x 0x298 at + 0, main 1 x 0x4DC at + 0xA60)
//     and starts the main task (the targets of action 0, their average default effect position,
//     layout 0x11DAB70). Then the main and target queues; ends when the main queue is empty.
//   MainTask (0x6A8AE0) - counters 0..0x23: the layout 0x11DAB70 at the caster's bone point
//     0x11DD340 (0x6A9200 = Hypnotize's 0x6B18A0, bone 3; kept at counter 0 as the wave start),
//     RotY(-facing), composed with the camera, depth -0x80. Counters 0..0x1D: SCREEN WARP A - the
//     point t = counter * 4096 / 30 of the way from the start to the targets' average, projected:
//     a screen-space frame (scale 0x18000 / OTZ, at SXY >> 3, matrix 0x2520C28), the wave frames
//     blended (0x6A9BB0, frames k / k + 1, k = counter >> 3, weight (counter << 9) & 0xFFF, into
//     magic buffer + 0xF3C), drawn by 0x6A9260 at OT (OTZ - 0x200) / 4 + 0x11 with the wave
//     amplitude 2 sin(t / 2) and fade 0x1000 - sin(t / 2). Counters 0x1E..0x45: SCREEN WARP B -
//     the screen box of every target's head and feet (0x6A9AD0, the box helper 0x6662C0 copied
//     here), around the targets' average, scale = the box (at least 0x18000 / OTZ), the mesh
//     0x2517B64 itself, t = (counter - 0x1E) * 4096 / 40, amplitude sin(t / 2), at OT (the
//     nearest OTZ - 0x200) / 4 + 0x11. Sound at counter 0. Counters 0x1E, 0x22, 0x26, 0x2A: the
//     target task of target 0..3 (if there). From counter 0x46 it waits for the target tasks,
//     restores the battle textures (0x508630, arena A) and ends when that is done.
//   ScreenWarp (0x6A9260) - each vertex displaced along its direction by the wave table
//     0x11DD4C8[(length << 2) + t] x amplitude, both the displaced and the rest vertex through
//     the warp frame (GTE MVMVA) into magic buffer + 0x1584 (12 bytes: displaced sx, sy; rest
//     u, v clamped to the screen; out-of-screen flags of the displaced one, OR-ed into the word
//     the vertex had when outside); a draw mode packet, the triangles as semi-transparent Gouraud
//     triangles at the rest positions (0x6A97C0, colours depth-cued by the fade), a draw mode
//     packet, the triangles textured from the capture area (0x240, 0x100) at the displaced
//     positions (0x6A9640, skipping triangles all outside on one side), then the draw environment
//     of the other display buffer (into the warp's OT entry) and a clearing draw environment onto
//     the capture area (OT entry 0x1121): the scene behind the wave is drawn into the capture area
//     and redrawn warped.
//   TargetTask (0x6A98E0) - the layout 0x11D8BF0 at the target's default effect position through
//     the camera, depth 0. While it plays it tints the target ENTITY (+0x28 colour, flag 0x800 of
//     +0x00): its colour at the spawn blended with the table 0x11DF4C8 (3 steps), the weight
//     sin(u / 6), u = counter * 0x3000 / 26. When the layout ends: the colour put back, the flag
//     cleared, the damage of every target record of action 0 that is this entity (0x6A9A60, as
//     Eerie Sound Wave's 0x6B4B90).
//   PrimCallback (0x6A8FD0, Hypnotize's 0x6B1520 + the 0x40000 rotation variant, on this module's
//     cursor and parameter block) draws every layout object.
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
// DRAW ORDER: every task draws THEN updates (the counters after the draws, a layout in its play).
// ENTITY / CAMERA: TargetTask writes its target entity's colour word +0x28 and flag 0x800 (no
// position: +0x1C..+0x20 / +0x26 only read); no task writes the camera words (the camera
// animation is the engine's).
// EXE DATA WRITES: the wave frames 0x11D7900 / 0x11D7F50 / 0x11D85A0 (packed vertex words, every
// cast). The warp frame 0x2520C28 is a module global (only m[0][0], m[1][1], TX, TY written).
// Module globals: 0x2517B50..0x2520C48 (caster entity 0x2517B50, texture file 0x2517B54, magic
// buffer base 0x2517B58, the data copy 0x2517B5C..0x2520BDC, context 0x2520BDC, master pool
// 0x2520BE0, packet cursor 0x2520BF4, the queues 0x2520BF8 targets / 0x2520C08 main / 0x2520C18
// master, the warp frame 0x2520C28).
// NEVER-WRITTEN STACK WORDS (the game's executor 0x508420 calls every task of a queue at one esp;
// the master pushes its queue arguments without popping: main entry esp E, targets E - 4):
//   - MainTask's bone point word +6 (entry esp - 0x2A) is copied into the main node's start +0x26
//     on counter 0 (never read, but kept in the magic buffer). Its last writer is the master's
//     setup in the same tick: 0x502170's `push esi` (the output pointer of the last
//     GetDefaultEffectPosition of a target with bones = the master's local, master entry esp - 8):
//     the high half of that stack address; without such a target, 0x7016B0's push of the master
//     node 0x2520BE0: 0x0252 (StartResidue).
//   - The blend point's word +6 (the same stack word; GTE VZ0 high half 0x1CA8A16 only): on counter 0
//     StartResidue, later the engine residue between two ticks (taken as 0, as the oeall harness
//     clears the stack). The head points' words +6 (0x6A9AD0) and the callback's record offset word
//     +6 only reach the GTE VZ0 high half; the block matrices' pad
//     words are never read; the target task's blended colour byte +3 is never read (0x56CC00 reads
//     and writes 3 bytes).
//   - The textured triangles' CLUT word (packet + 0xE) is never written: 15-bit texture pages,
//     what the packet arena held there (the same packets two ticks before) is not used.

#include "mag_common.h"
#include <intrin.h>

namespace ff8fx
{
namespace sonic139
{
	using namespace eng;
	using namespace magc;

	// --- module globals ---
	inline uint8_t *&Caster() { return var<uint8_t *>(0x2517B50); }        // caster entity (setup)
	inline uint32_t &TexBase() { return var<uint32_t>(0x2517B58); }        // Magic_TextureOFF (magic buffer)
	inline CastContext *&Ctx() { return var<CastContext *>(0x2520BDC); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x2520BF4); }
	inline TaskQueue *QTargets() { return (TaskQueue *)0x2520BF8; }        // pool: magic buffer, 4 x 0x298
	inline TaskQueue *QMain() { return (TaskQueue *)0x2520C08; }           // pool: + 0xA60, 1 x 0x4DC
	inline Mat4x3 &WarpFrame() { return var<Mat4x3>(0x2520C28); }          // m[0][0], m[1][1], TX, TY written
	inline uint32_t OTBase() { return var<uint32_t>(0x1D8E04C); }

	static const uint32_t ORIG_MasterTask = 0x6A88F0;
	static const uint32_t ORIG_MainTask = 0x6A8AE0;
	static const uint32_t ORIG_TargetTask = 0x6A98E0;
	static const uint32_t VERTS = 0xF3C;                 // magic buffer: blended wave frame / layout morph buffer (201 x 8)
	static const uint32_t VBUF = 0x1584;                 // magic buffer: warp vertices (201 x 12)
	static const uint32_t VBUF_BYTES = 0x1EF0 - VBUF;
	static const uint32_t VERTS_BYTES = VBUF - VERTS;
	static const uint32_t ARENA_A = 0x1EF0;              // magic buffer: packet arenas
	static const uint32_t ARENA_B = 0x11EF0;
	static uint8_t *const MESH = (uint8_t *)0x2517B5C;   // the wave mesh (module copy): size, count, vertices, triangles
	static const uint32_t FRAMES = 0x11D72B0;            // 4 wave frames of 0x650 bytes (same layout, no triangles)
	static const uint32_t FRAME_BYTES = 0x650;
	static const uint16_t *const SQRT_TABLE = (const uint16_t *)0x11DD2C8;
	static const int16_t *const WAVE_TABLE = (const int16_t *)0x11DD4C8;  // 0x1000 x s16 (also the 0x400-entry length table)
	static const int16_t *const BONE_ENTRY = (const int16_t *)0x11DD340; // x, y, z, bone 3
	static const uint32_t MODEL_Main = 0x11DAB70;        // prim-model layouts
	static const uint32_t MODEL_Target = 0x11D8BF0;
	static const uint8_t *const COLOURS = (const uint8_t *)0x11DF4C8;    // tint steps (RGBx)
	static const void *const SOUND_Wave = (const void *)0x11DD338;
	static const uint32_t OT_CAPTURE = 0x4484;           // OT entry 0x1121: the capture area's clearing environment

	// engine functions not in fx_port.h / mag_common.h
	inline void DecodeModelPrimLayout(uint32_t data, void *layout, int32_t size) { fn<void (__cdecl *)(uint32_t, void *, int32_t)>(0x7016B0)(data, layout, size); }
	inline void RotationFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x701310)(angles, out); }   // MAG_017_sub_701310
	inline void RotationFromAngles2(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x7015B0)(angles, out); }  // MAG_063_sub_7015B0
	inline void BlendVertexFrames(uint32_t model, int32_t f0, int32_t f1, int32_t t, uint32_t out) { fn<void (__cdecl *)(uint32_t, int32_t, int32_t, int32_t, uint32_t)>(0x701390)(model, f0, f1, t, out); } // MAG_017_sub_701390
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }   // IR = MAC = R * V0 (no translation)
	inline uint32_t RenderPrim2(void *h, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x7043B0)(h, ot, mode, cursor); } // MAG_070_sub_7043B0
	inline void RotY(int32_t angle, Mat4x3 *out) { fn<void (__cdecl *)(int32_t, Mat4x3 *)>(0x701270)(angle, out); }   // MAG_063_sub_701270
	inline void BlendVectors(const int16_t *a, const int16_t *b, int32_t wa, int32_t wb, int16_t *out) { fn<void (__cdecl *)(const int16_t *, const int16_t *, int32_t, int32_t, int16_t *)>(0x56CB90)(a, b, wa, wb, out); }
	inline void BlendColour(const void *a, const void *b, int32_t wa, int32_t wb, void *out) { fn<void (__cdecl *)(const void *, const void *, int32_t, int32_t, void *)>(0x56CC00)(a, b, wa, wb, out); }
	inline void GteWriteData(int32_t value, int reg) { fn<void (__cdecl *)(int32_t, int)>(0x45D7C0)(value, reg); }     // sub_45D7C0
	inline void GteWriteData2(int reg, uint32_t value) { fn<void (__cdecl *)(int, uint32_t)>(0x45DB90)(reg, value); }   // sub_45DB90
	inline void GteSetRGBC(const void *rgbc) { fn<void (__cdecl *)(const void *)>(0x45E110)(rgbc); }                     // set_unk_1CA8A28
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }                                                       // sub_45F270
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }                                // set_param_with_dword_1CA8A68
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); } // someCameraWork_45DD60
	inline void InsertPrimAltViewport(uint32_t ot, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(ot, packet); }       // SSIGPU_InsertPrimAltViewport

#pragma pack(push, 1)
	// PSX DRAWENV (0x5C bytes)
	struct DrawEnv
	{
		int16_t clip[4];   // +0x00
		int16_t ofs[2];    // +0x08
		int16_t tw[4];     // +0x0C
		uint16_t tpage;    // +0x14
		uint8_t dtd, dfe;  // +0x16
		uint8_t isbg, r0, g0, b0; // +0x18
		uint8_t dr_env[0x40]; // +0x1C
	};
#pragma pack(pop)
	static_assert(sizeof(DrawEnv) == 0x5C, "DRAWENV");
	inline void GetDrawEnv(DrawEnv *env) { fn<void (__cdecl *)(DrawEnv *)>(0x45B640)(env); }                                   // sub_45B640
	inline void SetDefDrawEnv(DrawEnv *env, int32_t x, int32_t y, int32_t w, int32_t h) { fn<void (__cdecl *)(DrawEnv *, int32_t, int32_t, int32_t, int32_t)>(0x45C2A0)(env, x, y, w, h); } // BS_Debug_memset
	inline void SetDrawEnv(void *packet, const DrawEnv *env) { fn<void (__cdecl *)(void *, const DrawEnv *)>(0x45C0F0)(packet, env); }       // sub_45C0F0
	inline void SetDrawMode(void *packet, int32_t dfe, int32_t dtd, int32_t tpage, const void *rect) { fn<void (__cdecl *)(void *, int32_t, int32_t, int32_t, const void *)>(0x45BFC0)(packet, dfe, dtd, tpage, rect); } // sub_45BFC0

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct MasterNode // pool of 1 node of 0x14 bytes (0x2520BE0)
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		uint8_t pad0E;
		uint8_t started;   // +0x0F pools set up
		uint32_t arena;    // +0x10 packet arena parity
	};
	struct MainNode // pool of 1 node of 0x4DC bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t count;     // +0x0E targets of action 0
		uint8_t index[3];  // +0x10 their record indices
		uint8_t restored;  // +0x13 texture restore done (0x508630)
		uint8_t *ents[3];  // +0x14 target entities
		int16_t start[4];  // +0x20 the caster's bone point at counter 0 (+0x26: StartResidue)
		int16_t avg[4];    // +0x28 the targets' average default effect position (+0x2E: 0)
		uint8_t layout[0x4AC]; // +0x30 prim-model layout 0x11DAB70
	};
	struct TargetNode // pool of 4 nodes of 0x298 bytes
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t index;     // +0x0E target record index (not read)
		uint8_t *entity;   // +0x10
		uint32_t colour;   // +0x14 the entity's colour at the spawn
		int16_t pos[4];    // +0x18 its default effect position
		uint8_t layout[0x278]; // +0x20 prim-model layout 0x11D8BF0
	};
	// the prim-player callback's parameter block (a stack block of the drawing task)
	struct PrimArg
	{
		Mat4x3 m;          // +0x00 object frame
		int32_t depth;     // +0x20 the prim header's +0x18
		uint32_t morph;    // +0x24 blended vertex frames
	};
#pragma pack(pop)
	static_assert(sizeof(MasterNode) == 0x14 && sizeof(MainNode) == 0x4DC && sizeof(TargetNode) == 0x298, "Sonic Wave nodes");
	static_assert(sizeof(PrimArg) == 0x28, "Sonic Wave prim block");

	enum PlayKind { PLAY_MAIN = 1, PLAY_TARGET = 2, WARP_A = 3, WARP_B = 4 };
}
}

#ifdef FF8_FX_HELD
#include "mag139_sonic_wave_held.h"
#endif

namespace ff8fx
{
namespace sonic139
{
	// ------------------------------------------------------------------
	// Prim-player callback (0x6A8FD0, Hypnotize's 0x6B1520 with the 0x40000 rotation variant, on
	// this module's cursor): one object of a layout. Vertex frame (or blend into the parameter
	// block's morph buffer), rotation from the record's angles, the record's offset through the
	// parameter matrix (flag 0x200: as is), the matrix composed with it, scale unless 1.0, fade,
	// drawn by 0x7043B0.
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
		if (r->flags & 0x40000) RotationFromAngles2(r->rot, &m);
		else RotationFromAngles(r->rot, &m);
		// the record's offset (4th word never written: only loaded into the GTE V0 pad)
		int16_t off[4] = { r->pos[0], r->pos[1], r->pos[2], 0 };
		int32_t t[3];
		if (r->flags & 0x200)
		{
			t[0] = off[0];
			t[1] = off[1];
			t[2] = off[2];
		}
		else
		{
			GteSetRotMatrixCtrl(&arg->m);
			GteLoadV0(off);
			GteMVMVA_RotV0();
			GteReadMAC123(m.t);
			MatrixMultiply(&arg->m, &m);
			t[0] = m.t[0];
			t[1] = m.t[1];
			t[2] = m.t[2];
		}
		m.t[0] = (int32_t)((uint32_t)t[0] + (uint32_t)arg->m.t[0]);
		m.t[1] = (int32_t)((uint32_t)t[1] + (uint32_t)arg->m.t[1]);
		m.t[2] = (int32_t)((uint32_t)t[2] + (uint32_t)arg->m.t[2]);
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
		*(int32_t *)(h + 0x18) = arg->depth;
		PacketCursor() = RenderPrim2(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// 0x6A9200 (= Hypnotize's 0x6B18A0): a point through the caster's bone (word +6) composed with
	// its matrix; writes 3 words
	static void BonePoint(const int16_t *entry, int16_t *out)
	{
		uint8_t *c = Caster();
		Mat4x3 m = {};
		ComposeAffineTransform((const Mat4x3 *)(c + 0x40), (const Mat4x3 *)(**(uint8_t ***)(c + 0x64) + 0x20 + (int32_t)entry[3] * 0x30), &m);
		GteSetRotMatrixCtrl(&m);
		GteSetTransVectorCtrl(&m);
		GteLoadV0(entry);
		GteMVMVA_RotV0Tr();
		GteStoreIR123(out);
	}

	// 0x6A9A60 (= Eerie Sound Wave's 0x6B4B90): the damage of every target record of action 0 that
	// is this entity
	static void DamageEntity(uint8_t *entity)
	{
		const int32_t slot = (int32_t)((uint32_t)entity - 0x1D972C0u) / 0x9C;
		for (uint32_t i = 0; i < Ctx()->actions[0].target_count; i++)
		{
			uint8_t *t = Ctx()->actions[0].targets + i * TARGET_STRIDE;
			if ((int32_t)t[0] == slot) ApplyActionResultToTarget(t);
		}
	}

	// 0x6662C0 (another module's helper, copied): grows the screen box (x0, y0, x1, y1) by the
	// point (x, y, z) when z > 0, keeping the smallest z; 1 when it did
	static int32_t ExpandBox(int16_t *box, const int16_t *p, int32_t *minz)
	{
		if (p[2] <= 0) return 0;
		if ((int32_t)p[2] < *minz) *minz = p[2];
		if (box[0] > p[0]) box[0] = p[0];
		if (box[2] < p[0]) box[2] = p[0];
		if (box[1] > p[1]) box[1] = p[1];
		if (box[3] < p[1]) box[3] = p[1];
		return 1;
	}

	// 0x6A9AD0: the box of an entity's head (y - (size + 0x100)) and feet points, each widened by
	// its screen size 54 (size + 0x100) / OTZ (taken at the head) both ways (camera already in the GTE)
	static int32_t HeadBox(int16_t *box, const uint8_t *e, int32_t *minz)
	{
		const int16_t x = *(const int16_t *)(e + 0x1C), y = *(const int16_t *)(e + 0x1E), z = *(const int16_t *)(e + 0x20);
		const int16_t size = (int16_t)(*(const int16_t *)(e + 0x26) + 0x100);
		// [0] feet, [1] head; the 4th words are never written (GTE V0 pad only)
		int16_t pts[2][4] = { { x, y, z, 0 }, { x, (int16_t)(y - size), z, 0 } };
		int32_t any = 0, w = size;
		for (int i = 1; i >= 0; i--)
		{
			GteLoadV0(pts[i]);
			GteRTPS();
			int16_t q[4];
			GteReadSXY2(q);
			int32_t otz;
			GteReadOTZ(&otz);
			q[2] = (int16_t)otz;
			q[0] = (int16_t)(q[0] >> 3);
			q[1] = (int16_t)(q[1] >> 3);
			if (i == 1) w = mul32((int32_t)size, 54) / (int32_t)q[2];
			q[0] = (int16_t)(q[0] - (int16_t)w);
			any |= ExpandBox(box, q, minz);
			q[0] = (int16_t)(q[0] + (int16_t)(w + w));
			any |= ExpandBox(box, q, minz);
		}
		return any;
	}

	// 0x6A9BB0: the wave frames a and b blended by t (GTE IR0 / GPF on the x / y differences) into
	// out; every out vertex takes the packed word of a's NEXT vertex (as the original reads it)
	static void BlendFrames(const uint8_t *a, const uint8_t *b, uint8_t *out, int32_t t)
	{
		uint32_t n = *(const uint32_t *)(a + 4);
		GteSetIR0(t);
		const uint8_t *pa = a + 8, *pb = b + 8;
		uint8_t *o = out;
		do
		{
			const uint32_t ax = *(const uint16_t *)pa, bx = *(const uint16_t *)pb;
			GteWriteData((int32_t)(bx - ax), 9);
			const uint32_t ay = *(const uint16_t *)(pa + 2), by = *(const uint16_t *)(pb + 2);
			GteWriteData((int32_t)(by - ay), 10);
			GteGPF();
			int32_t ir;
			GteReadDataRegTo(&ir, 9);
			*(uint16_t *)o = (uint16_t)((uint32_t)ir + ax);
			GteReadDataRegTo(&ir, 10);
			*(uint16_t *)(o + 2) = (uint16_t)((uint32_t)ir + ay);
			pa += 8;
			*(uint32_t *)(o + 4) = *(const uint32_t *)(pa + 4);
			pb += 8;
			o += 8;
		} while (--n);
	}

	// 0x6A9C80: every vertex's packed word = its length (bits 20..31, even) and its direction
	// scaled by the length table (bits 0..9 x, 10..19 y)
	static void NormaliseMesh(uint8_t *mesh)
	{
		uint32_t n = *(const uint32_t *)(mesh + 4);
		uint8_t *v = mesh + 8;
		do
		{
			const int32_t x = *(const int16_t *)v, y = *(const int16_t *)(v + 2);
			int32_t sq = (int32_t)((uint32_t)mul32(x, x) + (uint32_t)mul32(y, y));
			if (sq == 0) *(uint32_t *)(v + 4) = 0;
			else
			{
				GteSetLZCS(sq);
				int32_t lz;
				GteReadLZCR(&lz);
				const int32_t e = lz & ~1;
				const int32_t sh = (0x1F - e) >> 1;
				const int32_t c = e - 0x18;
				uint32_t idx;
				if (c > 0) idx = (uint32_t)sq << (c & 31);
				else idx = (uint32_t)(sq >> ((0x18 - e) & 31));
				int32_t len = (int32_t)((uint32_t)SQRT_TABLE[idx] << (sh & 31)) >> 12;
				*(uint32_t *)(v + 4) = ((uint32_t)(len & ~1) << 19) | (*(const uint32_t *)(v + 4) & 0xFFFFF);
				int32_t a = 0x400 - (len >> 2);
				if (a < 0) a = 0;
				else if (a > 0x3FF) a = 0x3FF;
				const int32_t s = WAVE_TABLE[a] >> 2;
				if (len == 0) len = 1;
				const int32_t dx = mul32(x, s) / len;
				const int32_t dy = mul32(y, s) / len;
				const uint32_t p = *(const uint32_t *)(v + 4);
				*(uint32_t *)(v + 4) = (((p & ~0x3FFu) | ((uint32_t)dx & 0x3FF)) & 0xFFF003FFu) | (((uint32_t)dy & 0x3FF) << 10);
			}
			v += 8;
		} while (--n);
	}

	// 0x6A9DA0 / 0x6A9DD0 / 0x6A9DF0: the mesh normalised, then every triangle turned to start at
	// its leftmost vertex (indices and colours) and its indices made 12-byte vertex indices; the
	// walk state lives in a Field_Alloc header (0x58)
	static void PrepareMesh(uint8_t *mesh)
	{
		NormaliseMesh(mesh);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		*(uint8_t **)h = mesh;
		*(uint8_t **)(h + 4) = mesh + 8;
		uint8_t *p = mesh + *(const uint32_t *)mesh + 0x10;
		uint32_t n = *(const uint32_t *)p;
		p += 4;
		*(uint8_t **)(h + 0x20) = p;
		if (n)
		{
			do
			{
				const uint32_t *verts = *(const uint32_t *const *)(h + 4);
				const uint16_t i0 = *(const uint16_t *)(p + 4), i1 = *(const uint16_t *)(p + 6), i2 = *(const uint16_t *)(p + 8);
				*(uint32_t *)(h + 0x34) = verts[i0];
				*(uint32_t *)(h + 0x38) = verts[i0 + 1];
				*(uint32_t *)(h + 0x3C) = verts[i1];
				*(uint32_t *)(h + 0x40) = verts[i1 + 1];
				*(uint32_t *)(h + 0x44) = verts[i2];
				*(uint32_t *)(h + 0x48) = verts[i2 + 1];
				int32_t lo = *(const int16_t *)(h + 0x34);
				bool second = false;
				if ((int32_t)*(const int16_t *)(h + 0x3C) < lo)
				{
					lo = *(const int16_t *)(h + 0x3C);
					second = true;
				}
				uint16_t *w = (uint16_t *)(p + 4);
				uint32_t *c = (uint32_t *)p;
				if ((int32_t)*(const int16_t *)(h + 0x44) < lo)
				{
					const uint16_t a = w[0], b = w[1], d = w[2];
					w[0] = d; w[1] = a; w[2] = b;
					const uint32_t c0 = c[0], c1 = c[3], c2 = c[4];
					c[0] = c2; c[4] = c1; c[3] = c0;
				}
				else if (second)
				{
					const uint16_t a = w[0], b = w[1], d = w[2];
					w[0] = b; w[2] = a; w[1] = d;
					const uint32_t c0 = c[0], c1 = c[3], c2 = c[4];
					c[0] = c1; c[3] = c2; c[4] = c0;
				}
				for (int k = 0; k < 3; k++) w[k] = (uint16_t)(w[k] + (uint16_t)(w[k] >> 1));
				p += 0x14;
			} while (--n);
			*(uint8_t **)(h + 0x20) = p;
		}
		FieldFree(0x58);
	}

	// 0x6A97C0: the triangles as semi-transparent Gouraud triangles at the rest positions, every
	// vertex colour depth-cued (GTE DPCS with the warp's IR0 / far colour 0)
	static void DrawRestTriangles(uint8_t *h)
	{
		uint8_t *t = *(uint8_t **)(h + 0x14);
		uint32_t n = *(const uint32_t *)t;
		t += 4;
		*(uint8_t **)(h + 0x14) = t;
		uint8_t *p = (uint8_t *)PacketCursor();
		if (n)
		{
			do
			{
				const uint8_t *v = *(const uint8_t **)(h + 4);
				memcpy(h + 0x1C, v + *(const uint16_t *)(t + 4) * 4, 0xC);
				memcpy(h + 0x28, v + *(const uint16_t *)(t + 6) * 4, 0xC);
				memcpy(h + 0x34, v + *(const uint16_t *)(t + 8) * 4, 0xC);
				*(uint32_t *)(p + 8) = *(const uint32_t *)(h + 0x20);
				*(uint32_t *)(p + 0x10) = *(const uint32_t *)(h + 0x2C);
				*(uint32_t *)(p + 0x18) = *(const uint32_t *)(h + 0x38);
				GteSetRGBC(t);
				GteDPCS();
				GteStoreRGB2(p + 4);
				GteSetRGBC(t + 0xC);
				GteDPCS();
				GteStoreRGB2(p + 0xC);
				GteSetRGBC(t + 0x10);
				GteDPCS();
				GteStoreRGB2(p + 0x14);
				*(uint32_t *)p = 0x6000000;
				p[7] = 0x32;
				InsertPrimAltViewport(*(const uint32_t *)(h + 0x18), p);
				p += 0x1C;
				t += 0x14;
			} while (--n);
			*(uint8_t **)(h + 0x14) = t;
		}
		PacketCursor() = (uint32_t)p;
	}

	// 0x6A9640: the triangles textured from the capture area at the displaced positions, unless all
	// three vertices are outside on one side (the CLUT word is not written: 15-bit pages)
	static void DrawWarpTriangles(uint8_t *h)
	{
		uint8_t *t = *(uint8_t **)(h + 0x14);
		uint32_t n = *(const uint32_t *)t;
		t += 4;
		*(uint8_t **)(h + 0x14) = t;
		uint8_t *p = (uint8_t *)PacketCursor();
		if (n)
		{
			do
			{
				const uint8_t *v = *(const uint8_t **)(h + 4);
				memcpy(h + 0x1C, v + *(const uint16_t *)(t + 4) * 4, 0xC);
				memcpy(h + 0x28, v + *(const uint16_t *)(t + 6) * 4, 0xC);
				memcpy(h + 0x34, v + *(const uint16_t *)(t + 8) * 4, 0xC);
				if (!(h[0x24] & h[0x30] & h[0x3C]))
				{
					const uint16_t u0 = *(const uint16_t *)(h + 0x20);
					const uint32_t ty = *(const uint32_t *)(h + 0x10);
					const uint32_t tp = (((*(const uint32_t *)(h + 0xC) + u0) >> 6) & 0xF) | (((ty & 0x200) | 0x40) << 2) | (((int32_t)ty >> 4) & 0x10);
					*(uint16_t *)(p + 0x16) = (uint16_t)tp;
					const uint8_t base = (uint8_t)(u0 & 0xC0);
					p[0xC] = (uint8_t)(h[0x20] - base);
					p[0xD] = h[0x22];
					p[0x14] = (uint8_t)(h[0x2C] - base);
					p[0x15] = h[0x2E];
					p[0x1C] = (uint8_t)(h[0x38] - base);
					p[0x1D] = h[0x3A];
					*(uint32_t *)(p + 8) = *(const uint32_t *)(h + 0x1C);
					*(uint32_t *)(p + 0x10) = *(const uint32_t *)(h + 0x28);
					*(uint32_t *)(p + 4) = 0x808080;
					*(uint32_t *)(p + 0x18) = *(const uint32_t *)(h + 0x34);
					*(uint32_t *)p = 0x7000000;
					p[7] = 0x26;
					InsertPrimAltViewport(*(const uint32_t *)(h + 0x18), p);
					p += 0x20;
				}
				t += 0x14;
			} while (--n);
			*(uint8_t **)(h + 0x14) = t;
		}
		PacketCursor() = (uint32_t)p;
	}

	// 0x6A9260: the screen warp of the mesh `mesh` with the vertices `verts` (8 bytes: x, y, packed
	// length / direction) through the warp frame already in the GTE, at OT entry `ot_index`;
	// `fade` = GTE IR0 of the Gouraud colours, `amp` / `phase` = the wave; `vbuf` = the warp
	// vertex buffer (magic buffer + 0x1584; the out-of-screen flags keep their bits when outside)
	static void ScreenWarp(uint8_t *mesh, const uint8_t *verts, int32_t ot_index, int32_t fade, int32_t amp, int32_t phase, uint8_t *vbuf)
	{
		const uint32_t ot = OTBase() + (uint32_t)ot_index * 4;
		DrawEnv env;
		GetDrawEnv(&env);
		env.ofs[0] = env.ofs[0] != 0 ? 0 : 0x140; // the other display buffer
		const int16_t rect[4] = { 0, 0, 0x100, 0x100 };
		uint8_t *p = (uint8_t *)PacketCursor();
		PacketCursor() += 0xC;
		SetDrawMode(p, env.dfe, env.dtd, env.tpage, rect);
		InsertPrimAutoDepth(ot, p);
		uint32_t n = *(const uint32_t *)(mesh + 4);
		uint8_t *o = vbuf;
		const uint8_t *v = verts;
		do
		{
			const uint32_t pk = *(const uint32_t *)(v + 4);
			const int32_t idx = ((((int32_t)pk >> 18) & ~3) + phase) & 0xFFF;
			const int32_t a = mul32(WAVE_TABLE[idx], amp) >> 12;
			const int32_t dx = (int32_t)(pk << 22) >> 22, dy = (int32_t)(pk << 12) >> 22;
			const uint16_t vx = (uint16_t)((mul32(dx, a) >> 12) + *(const int16_t *)v);
			const uint16_t vy = (uint16_t)((mul32(dy, a) >> 12) + *(const int16_t *)(v + 2));
			GteWriteData2(0, (uint32_t)vx | ((uint32_t)vy << 16));
			GteMVMVA_RotV0Tr();
			int32_t ir1, ir2;
			GteReadDataRegTo(&ir1, 9);
			GteReadDataRegTo(&ir2, 10);
			*(int16_t *)o = (int16_t)ir1;
			*(int16_t *)(o + 2) = (int16_t)ir2;
			GteWriteData2(0, *(const uint32_t *)v);
			GteMVMVA_RotV0Tr();
			const int16_t sx = *(const int16_t *)o, sy = *(const int16_t *)(o + 2);
			if (sx < 0) o[8] |= 1;
			else if (sx >= 0x140) o[8] |= 2;
			else *(uint16_t *)(o + 8) = 0;
			if (sy < 0) o[8] |= 4;
			else if (sy >= 0xD8) o[8] |= 8;
			GteReadDataRegTo(&ir1, 9);
			GteReadDataRegTo(&ir2, 10);
			int16_t &u = *(int16_t *)(o + 4), &w = *(int16_t *)(o + 6);
			u = (int16_t)ir1;
			w = (int16_t)ir2;
			if (u < 0) u = 0;
			else if (u >= 0x140) u = 0x13F;
			else if (w < 0) w = 0;
			else if (w >= 0xD8) w = 0xD7;
			o += 0xC;
			v += 8;
		} while (--n);
		uint8_t *h = (uint8_t *)FieldAlloc(0x4C);
		*(uint8_t **)h = mesh;
		*(uint8_t **)(h + 4) = vbuf;
		*(uint32_t *)(h + 0xC) = 0x240;
		*(uint32_t *)(h + 0x10) = 0x100;
		*(uint32_t *)(h + 0x18) = OTBase() + (uint32_t)ot_index * 4;
		GteSetIR0(fade);
		GteSetFarColor(0, 0, 0);
		*(uint8_t **)(h + 0x14) = mesh + *(const uint32_t *)mesh + 0x10;
		DrawRestTriangles(h);
		p = (uint8_t *)PacketCursor();
		PacketCursor() += 8;
		*(uint32_t *)p = 0x1000000;
		*(uint32_t *)(p + 4) = 0xE1000220;
		InsertPrimAutoDepth(*(const uint32_t *)(h + 0x18), p);
		*(uint8_t **)(h + 0x14) = mesh + *(const uint32_t *)mesh + 0x10;
		DrawWarpTriangles(h);
		FieldFree(0x4C);
		// back to the other display buffer (runs first in the warp's OT entry)
		p = (uint8_t *)PacketCursor();
		PacketCursor() += 0x40;
		DrawEnv e;
		SetDefDrawEnv(&e, env.ofs[0], env.ofs[1], 0x140, 0xD8);
		e.tpage = env.tpage;
		e.dtd = 0;
		e.dfe = env.dfe;
		e.isbg = 0;
		e.b0 = e.g0 = e.r0 = 0;
		SetDrawEnv(p, &e);
		InsertPrimAutoDepth(ot, p);
		// the capture area, cleared (OT entry 0x1121: before everything else)
		p = (uint8_t *)PacketCursor();
		PacketCursor() += 0x40;
		SetDefDrawEnv(&e, 0x240, 0x100, 0x140, 0xD8);
		e.tpage = env.tpage;
		e.dtd = env.dtd;
		e.dfe = env.dfe;
		e.isbg = 1;
		e.b0 = e.g0 = e.r0 = 0;
		SetDrawEnv(p, &e);
		InsertPrimAutoDepth(OTBase() + OT_CAPTURE, p);
	}

	// warp A's frame (0x6A8BA0..0x6A8C5B): the start -> average point at t projected; false when
	// behind (OTZ <= 0). Writes the warp frame (or `frame`) and loads it into the GTE.
	static bool WarpAFrame(const MainNode *w, int32_t t, int16_t pad, Mat4x3 &frame, int16_t *otz_out)
	{
		int16_t q[4];
		BlendVectors(w->start, w->avg, 0x1000 - t, t, q);
		q[3] = pad; // (the bone point's word +6 below: GTE V0 pad only)
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		GteLoadV0(q);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy);
		int32_t otz;
		GteReadOTZ(&otz);
		sxy[0] = (int16_t)(sxy[0] >> 3);
		sxy[1] = (int16_t)(sxy[1] >> 3);
		const int16_t z = (int16_t)otz;
		if (z <= 0) return false;
		const int16_t s = (int16_t)(0x18000 / (int32_t)z);
		frame.t[0] = sxy[0];
		frame.m[1][1] = s;
		frame.m[0][0] = s;
		frame.t[1] = sxy[1];
		GteSetRotMatrix(&frame);
		GteSetTransVector(&frame);
		*otz_out = z;
		return true;
	}

	// warp B's frame (0x6A8D17..0x6A8E8F): the box of the targets' heads and feet around the
	// average point (at least its scale); false when no point is in front or the nearest OTZ is
	// 0x200 or less. *z_out = the nearest OTZ - 0x200.
	static bool WarpBFrame(const MainNode *w, Mat4x3 &frame, int32_t *z_out)
	{
		int16_t box[4] = { 0x7FFF, 0x7FFF, (int16_t)0x8001, (int16_t)0x8001 };
		int32_t any = 0, minz = 0x10000;
		GteSetRotMatrix(&Camera());
		GteSetTransVector(&Camera());
		for (int32_t i = 0; i < w->count; i++) any |= HeadBox(box, w->ents[i], &minz);
		const int32_t z = minz - 0x200;
		if (!any || z <= 0) return false;
		int16_t bw = (int16_t)(box[2] - box[0]);
		const int16_t cx = (int16_t)(box[0] + (int16_t)((int32_t)bw / 2));
		int16_t bh = (int16_t)(box[3] - box[1]);
		const int16_t cy = (int16_t)(box[1] + (int16_t)((int32_t)bh / 2));
		GteLoadV0(w->avg);
		GteRTPS();
		int16_t sxy[2];
		GteReadSXY2(sxy);
		int32_t otz;
		GteReadOTZ(&otz);
		const int16_t s = (int16_t)(0x18000 / (int32_t)(int16_t)otz);
		if (bw < s) bw = s;
		if (bh < s) bh = s;
		frame.m[1][1] = bh;
		frame.m[0][0] = bw;
		frame.t[0] = cx;
		frame.t[1] = cy;
		GteSetRotMatrix(&frame);
		GteSetTransVector(&frame);
		*z_out = z;
		return true;
	}

	// the main layout's block (0x6A8AF6..0x6A8B89): the bone point, RotY(-facing), with the camera
	static void MainBlock(const int16_t *pos, PrimArg *arg)
	{
		RotY((int16_t)-*(const int16_t *)(Caster() + 0xE), &arg->m);
		arg->m.t[0] = pos[0];
		arg->m.t[1] = pos[1];
		arg->m.t[2] = pos[2];
		ComposeAffineTransform(&Camera(), &arg->m, &arg->m);
	}

	// the target layout's block (0x6A98E3..0x6A998D): the camera rows, the position through the camera
	static void TargetBlock(const int16_t *pos, PrimArg *arg)
	{
		memcpy(&arg->m.m, &Camera().m, sizeof(arg->m.m));
		GteSetRotMatrixCtrl(&Camera());
		GteLoadV0(pos);
		GteMVMVA_RotV0();
		GteReadMAC123(arg->m.t);
		arg->m.t[0] = (int32_t)((uint32_t)arg->m.t[0] + (uint32_t)Camera().t[0]);
		arg->m.t[1] = (int32_t)((uint32_t)arg->m.t[1] + (uint32_t)Camera().t[1]);
		arg->m.t[2] = (int32_t)((uint32_t)arg->m.t[2] + (uint32_t)Camera().t[2]);
	}

	// ------------------------------------------------------------------
	// Target (0x6A98E0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl TargetTask(TaskNode *n)
	{
		TargetNode *p = (TargetNode *)n;
		PrimArg arg;
		TargetBlock(p->pos, &arg);
		arg.depth = 0;
		arg.morph = TexBase() + VERTS;
		// 30 fps layer: see mag139_sonic_wave_held.inc
		FX_HELD(held_note_play(PLAY_TARGET, n, (prim::Layout *)p->layout);)
		if (!prim::play((prim::Layout *)p->layout, PrimCallback, (int)&arg, 0))
		{
			*(uint32_t *)(p->entity + 0x28) = p->colour;
			*(uint16_t *)p->entity &= 0xF7FF;
			DamageEntity(p->entity);
			return TASK_END;
		}
		p->entity[1] |= 8;
		const int32_t u = mul32((int32_t)p->counter * 3, 0x1000) / 26;
		const int32_t s = ComputeSin(u / 6);
		const int32_t f = u & 0xFFF;
		const int32_t k = u >> 12;
		uint8_t c[4]; // (byte 3 never written nor read)
		BlendColour(COLOURS + k * 4, COLOURS + k * 4 + 4, 0x1000 - f, f, c);
		BlendColour(&p->colour, c, 0x1000 - s, s, p->entity + 0x28);
		p->counter++;
		return 0;
	}

	// the word below the original's bone point (main entry esp - 0x2A) on counter 0, kept in the main
	// node's start +0x26 (StartResidue): the main task runs right after the master's setup (same
	// tick); its last writer there is the master's last GetDefaultEffectPosition of a target with
	// bones (entity +0x00 bit 1): 0x502170's `push esi` of the output pointer = the master's local
	// (master entry esp - 8): the high half of that stack address. Without such a target: the
	// master node address 0x2520BE0 pushed by 0x7016B0 (DecodeModelPrimLayout), high half 0x0252.
	// (DR0 write watches under the game executor; the port's master runs at the original's esp.)
	static int16_t g_start_residue = 0x0252;
	static int16_t StartResidue() { return g_start_residue; }

	// ------------------------------------------------------------------
	// Main (0x6A8AE0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MainTask(TaskNode *n)
	{
		MainNode *w = (MainNode *)n;
		if ((uint32_t)(int32_t)w->counter < 0x24)
		{
			int16_t pos[4];
			pos[3] = StartResidue();
			BonePoint(BONE_ENTRY, pos);
			if (w->counter == 0) memcpy(w->start, pos, 8);
			PrimArg arg;
			MainBlock(pos, &arg);
			arg.depth = -0x80;
			arg.morph = TexBase() + VERTS;
			// 30 fps layer: see mag139_sonic_wave_held.inc
			FX_HELD(held_note_play(PLAY_MAIN, n, (prim::Layout *)w->layout);)
			prim::play((prim::Layout *)w->layout, PrimCallback, (int)&arg, 0);
		}
		const int32_t c = w->counter;
		if ((uint32_t)c < 0x1E)
		{
			const int32_t t = (int32_t)((uint32_t)(c << 12) / 30);
			// 30 fps layer: see mag139_sonic_wave_held.inc
			FX_HELD(held_note_play(WARP_A, n, nullptr);)
			int16_t z;
			if (WarpAFrame(w, t, c == 0 ? StartResidue() : 0, WarpFrame(), &z))
			{
				const int32_t half = t >> 1;
				const int32_t s = ComputeSin(half);
				const uint32_t phase = (uint32_t)c << 9;
				const uint32_t k = (phase >> 12) & 3;
				BlendFrames((const uint8_t *)(FRAMES + k * FRAME_BYTES), (const uint8_t *)(FRAMES + ((k + 1) & 3) * FRAME_BYTES), (uint8_t *)(TexBase() + VERTS), (int32_t)(phase & 0xFFF));
				const int32_t s2 = ComputeSin(half);
				ScreenWarp(MESH, (const uint8_t *)(TexBase() + VERTS), (((int16_t)(z - 0x200)) >> 2) + 0x11, 0x1000 - s2, s + s, t, (uint8_t *)(TexBase() + VBUF));
			}
		}
		const uint32_t b = (uint32_t)(c - 0x1E);
		if (b < 0x28)
		{
			// 30 fps layer: see mag139_sonic_wave_held.inc
			FX_HELD(held_note_play(WARP_B, n, nullptr);)
			int32_t z;
			if (WarpBFrame(w, WarpFrame(), &z))
			{
				const int32_t t = (int32_t)((b << 12) / 40);
				const int32_t half = t >> 1;
				const int32_t s = ComputeSin(half);
				const int32_t s2 = ComputeSin(half);
				ScreenWarp(MESH, MESH + 8, (z >> 2) + 0x11, 0x1000 - s2, s, t, (uint8_t *)(TexBase() + VBUF));
			}
		}
		if (w->counter == 0) BdPlaySE(SOUND_Wave, 0, 0x80);
		const uint32_t e = (uint32_t)(w->counter - 0x1E);
		if (e < 0x10 && !(e & 3) && (e >> 2) < (uint32_t)(int32_t)w->count)
		{
			TargetNode *p = (TargetNode *)AddTaskToQueue(QTargets(), ORIG_TargetTask);
			if (p)
			{
				const uint32_t i = e >> 2;
				p->counter = 0;
				p->index = w->index[i];
				p->entity = w->ents[i];
				p->colour = *(const uint32_t *)(p->entity + 0x28);
				GetDefaultEffectPosition(p->entity, p->pos);
				DecodeModelPrimLayout(MODEL_Target, p->layout, 0x278);
			}
		}
		if (w->counter < 0x46)
		{
			w->counter++;
			return 0;
		}
		if (QTargets()->head) return 0;
		if (w->counter == 0x46)
		{
			TextureRestoreTask((void *)(TexBase() + ARENA_A), &w->restored);
			w->counter++;
		}
		return w->restored ? TASK_END : 0;
	}

	// ------------------------------------------------------------------
	// Master (0x6A88F0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MasterTask(TaskNode *n)
	{
		MasterNode *m = (MasterNode *)n;
		// 30 fps layer: see mag139_sonic_wave_held.inc
		FX_HELD(held_note_master();)
		if (m->arena)
		{
			PacketCursor() = TexBase() + ARENA_A;
			m->arena = 0;
		}
		else
		{
			PacketCursor() = TexBase() + ARENA_B;
			m->arena = 1;
		}
		if (m->counter == 1 && !m->started)
		{
			PrepareMesh(MESH);
			for (uint32_t f = FRAMES + 3 * FRAME_BYTES; f != FRAMES; f -= FRAME_BYTES) NormaliseMesh((uint8_t *)f);
			m->started = 1;
			InitTaskQueuePool(QMain(), (void *)(TexBase() + 0xA60), 0x4DC, 1);
			InitTaskQueuePool(QTargets(), (void *)TexBase(), 0x298, 4);
			MainNode *w = (MainNode *)AddTaskToQueue(QMain(), ORIG_MainTask);
			Memset32(&w->counter, 0, 0x134);
			DecodeModelPrimLayout(MODEL_Main, w->layout, 0x4AC);
			w->count = (int16_t)Ctx()->actions[0].target_count;
			w->avg[2] = 0;
			w->avg[1] = 0;
			w->avg[0] = 0;
			g_start_residue = 0x0252;
			if (w->count > 0)
			{
				for (int32_t i = 0; i < w->count; i++)
				{
					w->index[i] = (uint8_t)i;
					w->ents[i] = Entity(Ctx()->actions[0].targets[i * TARGET_STRIDE]);
					int16_t pos[4];
					GetDefaultEffectPosition(w->ents[i], pos);
					if (w->ents[i][0] & 2) g_start_residue = (int16_t)(((uint32_t)_AddressOfReturnAddress() - 8) >> 16);
					w->avg[0] = (int16_t)(w->avg[0] + pos[0]);
					w->avg[1] = (int16_t)(w->avg[1] + pos[1]);
					w->avg[2] = (int16_t)(w->avg[2] + pos[2]);
				}
			}
			w->avg[0] = (int16_t)((int32_t)w->avg[0] / (int32_t)w->count);
			w->avg[1] = (int16_t)((int32_t)w->avg[1] / (int32_t)w->count);
			w->avg[2] = (int16_t)((int32_t)w->avg[2] / (int32_t)w->count);
		}
		int a0;
		if (m->started)
		{
			a0 = ExecuteTaskQueue(QMain());
			ExecuteTaskQueue(QTargets());
		}
		else a0 = (int)n;
		if (m->started && a0 == 0) return TASK_END;
		m->counter++;
		return 0;
	}
}

	void register_mag139_sonic_wave()
	{
		register_port(sonic139::ORIG_MasterTask, (void *)sonic139::MasterTask, "139 MasterTask", 139);
		register_port(sonic139::ORIG_MainTask, (void *)sonic139::MainTask, "139 MainTask", 139);
		register_port(sonic139::ORIG_TargetTask, (void *)sonic139::TargetTask, "139 TargetTask", 139);
		// 30 fps layer: see mag139_sonic_wave_held.inc
		FX_HELD(register_mag139_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag139_sonic_wave_held.inc"
#endif
