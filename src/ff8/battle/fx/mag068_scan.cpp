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

// Effect 68: Scan (enemy attack 135 "Scan", Tri-Point c0m030; MAG_068_SCAN_*).
//
// A copy of the spell Scan's library (effect 40, mag040_scan.cpp) reduced to its short sequence: the
// emitter always spawns the short scanner (the spell's choice by the target record +2 bit 1 is gone)
// and the viewer, the long scanner, the screen dimmer / restorer, the scan bars, the spinner, the
// board and the backdrop are not in this copy. Every function left is the spell's code at its own
// addresses except: the line draws (0x7DC810, 0x7DD3D0) insert with SSIGPU_InsertPrimAutoDepth
// 0x45C7A0 (the spell: 0x45C8E0), the text pager calls manageScanText 0xB67EF0 / ScanText_GetLine
// 0xB68370 (the spell: their copies 0xB68390 / 0xB68810).
//
// Structure (module code 0x7DC180..0x7DEA00: file loader MAG_068_UNK3_FL 0x7DC180, setup
// MAG_068_SCAN 0x7DC1A0 sets up the queues and the root node, queues the TIM and takes the two packet
// arenas: texture buffer + 0 (0x25FFC3C) / + 0x4000 (0x25FFC38)). Every node starts with the shared
// effect-library header (Effect_AddTaskAndInitFromCtx 0x8DC540); every task runs its current phase
// function (node +0x29) from a small table and ends when it is flagged done (+0x26 bit0) and has no
// live children. No task tests the draw-only flags (battle_to_update_flags 0x201).
//   Root (0x7DC280) - packet arena by tick parity (cursor 0x25FCE30), follows the caster's bones;
//     phase 1 spawns the emitter of the current action, arms the camera return (0x50A730) and hides
//     the battle menu (0x4A8480(0)); phase 2 moves to the next action; phase 3 waits for the four
//     queues to be empty, shows the menu again (0x4A8480(1)) and ends. Queues run: emitters,
//     controllers (0x78 nodes), sprites (0x58), models (0x6C).
//   Emitter (0x7DC3A0): sound at its first tick; phase 0 (0x7DC430) finds the scanned entities (the
//     target and its linked parts, flags 0x25FCE34[7]; main entity 0x25FFAA8 = 0x501FF0(target)) and
//     spawns the scanner 0x7DC600. Phase 1 applies the action result once the scanner is gone.
//   Scanner (0x7DC600): the target entity is marked (entity +1 |= 0x78), the camera task (0x7DD950
//     mode 2) and the reticle; then the spinner model (0x7DDED0), the text pager (0x7DE540, layer 1,
//     pad 0x20 = next line, 0x40 = close) and the stat board (0x7DE110, stat bars 0x7DE200); the
//     camera circles the target (it writes the battle camera) until the text is closed; then the
//     camera returns (0x7DE830 for the last action, else mode 3).
//   Reticle (0x7DC6F0): a screen-space fan of green lines (0x7DC810) plus four lines to the screen
//     corners (0x7DD330) moving along the table 0x15956F0 (one point per tick until 0x7FFF), then
//     shrinking; spawns four corner brackets (0x7DCF10), four sliding brackets (0x7DD150), the
//     rotating morph model (0x7DCBE0) and the model pair (0x7DC950), then the ring (0x7DD510).
//   Camera (0x7DD950): blends the battle camera (eye / look-at 0xB8B7F0.., projection 0x1D8E038)
//     from the current one to a view of the target defined by the target's info record
//     (0x1594998 + class * 16, class = 0x7DC560), in steps of +0x32 over the camera state
//     0x25FCDE0 (pointer 0x1591C40).
//   HUD models are drawn in screen space (GTE set to the node's own matrix: 0x7DC9F0 / 0x7DDF40,
//     sprites 0x7DD6C0); lines go to the render list + 0x14. Two models morph their vertices
//     (0x7DCC80, GTE GPF / GPL) into their exe data.
// No task writes a battle entity's position (+0x1C..+0x20); the entities' flags (+0, +1) are written.
// Module globals: 0x25FCDD8..0x26005B4 (texture file 0x25FCDD8, camera state 0x25FCDE0, packet
// cursor 0x25FCE30, scanned-entity flags 0x25FCE34, model pool / queue 0x25FCE70 / 0x25FD6E0
// (20 x 0x6C), HUD active 0x25FD708, root pool / queue 0x25FD758 / 0x25FD820 (2 x 0x64), sprite
// pool / queue 0x25FD830 / 0x25FFA90 (100 x 0x58), closing 0x25FFAA4, main entity 0x25FFAA8,
// emitter queue / pool 0x25FFAC8 / 0x25FFAD8 (4 x 0x58), arenas 0x25FFC38 / 0x25FFC3C, controller
// queue / pool 0x25FFC40 / 0x25FFC50 (20 x 0x78), confirm 0x26005B0, text done 0x26005B2); the
// morphed vertex blocks in the exe data 0x1591C4C (0x30 vertices) and 0x1592034 (0xA0 vertices);
// battle entities (flags, matrices), the battle camera and the text layers.

#include "mag_common.h"

namespace ff8fx
{
namespace scan068
{
	using namespace eng;
	using namespace magc;

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	// shared effect-library node header (0x30 bytes, Effect_AddTaskAndInitFromCtx 0x8DC540)
	struct FxNode
	{
		TaskNode hdr;          // +0x00
		CastContext *ctx;      // +0x0C
		FxNode *root;          // +0x10
		FxNode *emitter;       // +0x14
		FxNode *parent;        // +0x18 (its +0x28 counts the live children)
		int16_t pos[4];        // +0x1C x, y, z (copied from the parent); HUD nodes: screen / view space
		int16_t counter;       // +0x24
		uint16_t flags;        // +0x26 bit0 done, bit2 hidden
		uint8_t children;      // +0x28
		int8_t phase;          // +0x29
		int8_t action;         // +0x2A
		int8_t target;         // +0x2B
		uint8_t attacker;      // +0x2C
		uint8_t slot;          // +0x2D target entity slot
		uint8_t line;          // +0x2E
		uint8_t last;          // +0x2F
	};
	struct RootNode // pool of 2 nodes of 0x64 bytes
	{
		FxNode h;
		uint8_t bones[0x28];   // +0x30
		int16_t last_action;   // +0x58
		int16_t target_count;  // +0x5A
		int16_t tick;          // +0x5C arena parity
		int16_t alive;         // +0x5E
		uint8_t pad60[3];
		uint8_t busy;          // +0x63
	};
	struct EmitterNode // pool of 4 nodes of 0x58 bytes
	{
		FxNode h;
		uint8_t anchor[0x18];  // +0x30
		int16_t bmin[4];       // +0x48
		int16_t bmax[4];       // +0x50
	};
	struct CtlNode // controllers / screen-space line parts: pool of 20 nodes of 0x78 bytes
	{
		FxNode h;
		int32_t scale[3];      // +0x30 (the spell's viewer: zoom of the target)
		uint32_t pad3C;
		int16_t rot[3];        // +0x40 (the spell's viewer)
		int16_t pad46;
		int16_t dx, dy;        // +0x48 sliding bracket: drift per tick
		uint32_t pad4C;
		uint32_t shape;        // +0x50 bracket line list (8 words per line, 0xFF ends)
		int16_t offx;          // +0x54 (the spell's viewer)
		int16_t pad56;
		int16_t offz;          // +0x58 (the spell's viewer)
		int16_t pad5A;
		uint32_t pad;          // +0x5C text: pad bits
		uint32_t prev;         // +0x60 text: previous pad bits
		int16_t size;          // +0x64 bracket scale
		int16_t timer;         // +0x66
		int16_t mode;          // +0x68 camera mode
		int16_t lines;         // +0x6A text: line count
		int16_t line;          // +0x6C text: line shown
		int16_t fade;          // +0x6E scan bars: brightness
		int16_t moved;         // +0x70 (the spell's viewer)
		int16_t idle;          // +0x72 (the spell's viewer)
		int16_t index;         // +0x74 bracket corner
		int16_t pad76;
	};
	struct ModelNode // HUD models: pool of 20 nodes of 0x6C bytes
	{
		FxNode h;
		int32_t scale[3];      // +0x30
		uint32_t pad3C;
		uint8_t rgb[4];        // +0x40
		int16_t angle[3];      // +0x44 x, y, z
		int16_t pad4A;
		uint32_t model;        // +0x4C prim model
		int16_t fade;          // +0x50
		int16_t pad52;
		uint32_t morph_a;      // +0x54 morph model: vertex sets
		uint32_t morph_b;      // +0x58
		int16_t fade1;         // +0x5C model pair: fades
		int16_t fade2;         // +0x5E
		int16_t table;         // +0x60 stat bar: table 0 / 1
		int16_t which;         // +0x62 stat bar: index
		int16_t max;           // +0x64 stat bar: full width
		int16_t timer;         // +0x66
		int16_t wave;          // +0x68 pulse angle
		int16_t blend;         // +0x6A morph model: blend
	};
	struct SpriteNode // pool of 100 nodes of 0x58 bytes
	{
		FxNode h;
		uint8_t pad30[0x1C];
		uint32_t seq;          // +0x4C flipbook
		int16_t frame;         // +0x50
		int16_t last;          // +0x52
		uint32_t pad54;
	};
	// camera state (0x25FCDE0, pointer at 0x1591C40)
	struct CamState
	{
		int16_t eye[4];        // +0x00 (+0x06 copied with it)
		int16_t look[4];       // +0x08 current look-at (+0x0E copied with it)
		int16_t look0[4];      // +0x10 start
		int16_t look1[4];      // +0x18 end
		uint32_t angles[2];    // +0x20 the main entity's angles (saved)
		uint32_t place[2];     // +0x28 the main entity's position (saved)
		int16_t t;             // +0x30 blend 0..0x1000
		int16_t step;          // +0x32
		int16_t yaw, yaw0, yaw1;     // +0x34
		int16_t dist, dist0, dist1;  // +0x3A
		int16_t h, h0, h1;           // +0x40
		int16_t proj, proj0, proj1;  // +0x46
		int16_t cls;           // +0x4C target class
	};
#pragma pack(pop)
	static_assert(sizeof(FxNode) == 0x30 && sizeof(RootNode) == 0x64 && sizeof(EmitterNode) == 0x58, "Scan nodes");
	static_assert(sizeof(CtlNode) == 0x78 && sizeof(ModelNode) == 0x6C && sizeof(SpriteNode) == 0x58, "Scan nodes");
	static_assert(sizeof(CamState) == 0x4E, "Scan camera state");

	// ------------------------------------------------------------------
	// Module globals
	// ------------------------------------------------------------------
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x25FCE30); }
	inline TaskQueue *QEmitters() { return (TaskQueue *)0x25FFAC8; }  // 4 x 0x58
	inline TaskQueue *QControls() { return (TaskQueue *)0x25FFC40; }  // 20 x 0x78
	inline TaskQueue *QSprites() { return (TaskQueue *)0x25FFA90; }   // 100 x 0x58
	inline TaskQueue *QModels() { return (TaskQueue *)0x25FD6E0; }    // 20 x 0x6C
	inline CamState &Cam() { return **(CamState **)0x1591C40; }
	inline int32_t &Scanned(int k) { return ((int32_t *)0x25FCE34)[k]; }   // entity k is scanned
	inline int16_t &HudActive() { return var<int16_t>(0x25FD708); }
	inline int16_t &Closing() { return var<int16_t>(0x25FFAA4); }
	inline int16_t &Confirmed() { return var<int16_t>(0x26005B0); }
	inline int16_t &TextDone() { return var<int16_t>(0x26005B2); }
	inline uint8_t *MainEntity() { return var<uint8_t *>(0x25FFAA8); }
	inline uint32_t LinesOT() { return var<uint32_t>(0x1D8E04C) + 0x14; }

	static const uint32_t ORIG_Root = 0x7DC280, ORIG_Emitter = 0x7DC3A0, ORIG_AltScanner = 0x7DC600;
	static const uint32_t ORIG_Reticle = 0x7DC6F0, ORIG_Pair = 0x7DC950, ORIG_Morph = 0x7DCBE0, ORIG_Corner = 0x7DCF10;
	static const uint32_t ORIG_Bracket = 0x7DD150, ORIG_Ring = 0x7DD510, ORIG_Sprite = 0x7DD650, ORIG_Camera = 0x7DD950;
	static const uint32_t ORIG_StatBar = 0x7DE200, ORIG_CamRestore = 0x7DE830, ORIG_AltSpinner = 0x7DDED0;
	static const uint32_t ORIG_AltBoard = 0x7DE110, ORIG_AltText = 0x7DE540;

	static const void *const SOUND_Scan = (const void *)0x1594994;
	static const uint32_t INFO_TABLE = 0x1594998;   // 16 bytes per entity class
	static const uint32_t RETICLE_PATH = 0x15956F0; // (x, y) per tick, 0x7FFF ends

	// ------------------------------------------------------------------
	// Engine / effect-library functions (original addresses)
	// ------------------------------------------------------------------
	inline FxNode *AddEffectTask(TaskQueue *q, uint32_t task, int32_t size, void *parent) { return fn<FxNode *(__cdecl *)(TaskQueue *, uint32_t, int32_t, void *)>(0x8DC540)(q, task, size, parent); }
	inline void ReleaseLinkedTask(void *n) { fn<void (__cdecl *)(void *)>(0x8DC530)(n); }
	inline void EmitterUpdatePos(void *n) { fn<void (__cdecl *)(void *)>(0x8DC610)(n); }
	inline void ComputeModelBounds(void *n) { fn<void (__cdecl *)(void *)>(0x8DC870)(n); }
	inline void UpdateTargetPosFromBones(void *n) { fn<void (__cdecl *)(void *)>(0x8DC740)(n); }
	inline void CopyMidpointFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC700)(n, out); }
	inline void IdentityMatrix(Mat4x3 *m) { fn<void (__cdecl *)(Mat4x3 *)>(0x8DD770)(m); }
	inline void RotateX(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD7E0)(m, a); }
	inline void RotateY(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD8A0)(m, a); }
	inline void RotateZ(Mat4x3 *m, int32_t a) { fn<void (__cdecl *)(Mat4x3 *, int32_t)>(0x8DD960)(m, a); }
	inline void MatrixMulVector(const Mat4x3 *m, const int16_t *v, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, v, out); }
	inline int32_t ComputeCos(int32_t a) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
	inline int32_t Sqrt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }
	inline int32_t CartesianToGameAngle(int32_t a, int32_t b) { return fn<int32_t (__cdecl *)(int32_t, int32_t)>(0x56D160)(a, b); }
	inline void GetEffectSpawnPosition(void *entity, int32_t bone, int32_t a, int16_t *out) { fn<void (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, a, out); }
	inline uint8_t *MainPartOf(uint8_t *entity) { return fn<uint8_t *(__cdecl *)(uint8_t *)>(0x501FF0)(entity); } // compare_com_127_501FF0
	inline uint32_t RenderPrimModelB(void *header, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x8DE9D0)(header, ot, mode, cursor); }
	inline void InsertPrimAutoDepth(uint32_t ot, void *prim) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C7A0)(ot, prim); } // SSIGPU_InsertPrimAutoDepth
	inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }            // BS_Camera_ArmReturnAfterEffect
	inline void BattleMenuShow(int32_t on) { fn<void (__cdecl *)(int32_t)>(0x4A8480)(on); } // sub_4A8480
	// pad
	inline uint32_t ReadPadHeldRaw(int32_t a, int32_t b) { return fn<uint32_t (__cdecl *)(int32_t, int32_t)>(0x49ED30)(a, b); }
	inline uint32_t RemapPadInput(uint32_t raw) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x4A2D60)(raw); }
	// text layers
	inline void TextSetLayerText(int32_t layer, const void *text) { fn<void (__cdecl *)(int32_t, const void *)>(0x4A0410)(layer, text); }
	inline void TextLayer4A0700(int32_t layer, int32_t a) { fn<void (__cdecl *)(int32_t, int32_t)>(0x4A0700)(layer, a); }
	inline void TextSetPosition(int32_t layer, const int16_t *rect) { fn<void (__cdecl *)(int32_t, const int16_t *)>(0x4A07A0)(layer, rect); }
	inline void TextLayer4A0640(int32_t layer) { fn<void (__cdecl *)(int32_t)>(0x4A0640)(layer); }
	inline void TextLayer4A0680(int32_t layer) { fn<void (__cdecl *)(int32_t)>(0x4A0680)(layer); }
	inline void TextLayer49FBC0(int32_t layer, int32_t a) { fn<void (__cdecl *)(int32_t, int32_t)>(0x49FBC0)(layer, a); }
	inline uint8_t ScanTextLines(uint32_t slot) { return (uint8_t)fn<uint32_t (__cdecl *)(uint32_t)>(0xB67EF0)(slot); }   // manageScanText
	inline const void *ScanTextLine(uint32_t slot, uint32_t line) { return fn<const void *(__cdecl *)(uint32_t, uint32_t)>(0xB68370)(slot, line); } // ScanText_GetLine

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }
	template<typename T> static inline const T &F(const uint8_t *p, uint32_t off) { return *(const T *)(p + off); }

	static inline int32_t Div4096(int32_t x) { return x / 4096; }

	// ------------------------------------------------------------------
	// Target helpers
	// ------------------------------------------------------------------
	// sub_7DC560: the entity class used for the info record (entity +4, with special cases)
	static int16_t EntityClass(int16_t slot)
	{
		uint8_t *e = Entity(slot);
		const int16_t t = (int16_t)e[4];
		if (t == 0x24 || t == 0x25) return F<int32_t>(e, 0x7C) == -1 ? (int16_t)0xA0 : t;
		if (t == 0x2B) return *F<uint8_t *>(e, 0x74) == 1 ? t : (int16_t)0xA1;
		if (t == 0x77) return *F<uint8_t *>(e, 0x74) == 1 ? t : (int16_t)0xA2;
		if (t == 0x3B)
		{
			const int16_t w = F<int16_t>(e, 0x9A);
			if (w == 0x1800) return (int16_t)0xA3;
			if (w == 0x2000) return (int16_t)0xA4;
			return t;
		}
		if (t == 0x4B) return *F<uint8_t *>(e, 0x74) == 1 ? t : (int16_t)0xA5;
		return t;
	}

	// sub_7DDC30: the target's info record (class into the camera state)
	static const uint8_t *InfoOf(const FxNode *n)
	{
		const int16_t c = EntityClass((int16_t)(uint16_t)n->slot);
		Cam().cls = c;
		return (const uint8_t *)(INFO_TABLE + (int32_t)c * 16);
	}

	// MAG_068_sub_7DC470: the main entity, and which entities are scanned (the target and its parts)
	static void FindScannedEntities(int16_t slot)
	{
		uint8_t *e = Entity(slot);
		var<uint8_t *>(0x25FFAA8) = MainPartOf(e);
		uint8_t *list[8];
		if (EntityClass(slot) == 0x54)
		{
			int16_t n = 0;
			for (int k = 0; k < 7; k++)
			{
				const uint8_t b = Entity(k)[4];
				if (b == 0x54 || b == 0x60 || b == 0x61) list[n++] = Entity(k);
			}
			list[n] = nullptr;
		}
		else
		{
			int16_t n = 0;
			uint8_t *p = e;
			do
			{
				if (p[0] & 2) list[n++] = p;
				p = F<uint8_t *>(p, 0x8C);
			} while (p != e && p != nullptr);
			list[n] = nullptr;
		}
		for (int k = 0; k < 7; k++)
		{
			Scanned(k) = 0;
			if (!list[0]) continue;
			for (int j = 0; list[j]; j++)
				if (Entity(k) == list[j]) Scanned(k) = 1;
		}
	}

	// ------------------------------------------------------------------
	// Drawing helpers
	// ------------------------------------------------------------------
	// a green LINE_F2 between two screen points into the render list + 0x14 (sub_7DD3D0)
	static void Line(const int16_t *a, const int16_t *b)
	{
		uint8_t *p = (uint8_t *)PacketCursor();
		p[4] = 0;
		p[6] = 0;
		F<uint32_t>(p, 0) = 0x3000000;
		p[7] = 0x40;
		F<int16_t>(p, 0xA) = a[1];
		F<int16_t>(p, 8) = a[0];
		p[5] = 0xFF;
		F<int16_t>(p, 0xC) = b[0];
		F<int16_t>(p, 0xE) = b[1];
		InsertPrimAutoDepth(LinesOT(), p);
		PacketCursor() = (uint32_t)(p + 0x10);
	}

	// sub_7DC810: `count` green lines along the circle of radius r around the node's screen point
	// (+0xA0, +0x6C) from angle a0, 0x80 per line; `out` = the last point
	static void Fan(const FxNode *n, int16_t r, int16_t a0, int16_t count, int16_t *out)
	{
		const uint32_t ot = LinesOT();
		uint8_t *p = (uint8_t *)PacketCursor();
		int32_t a = a0;
		int32_t x0 = Div4096(mul32(ComputeCos(a), r));
		int32_t y0 = Div4096(mul32(ComputeSin(a), r));
		for (int32_t c = count; c > 0; c--)
		{
			a = (a + 0x80) & 0xFFF;
			const int32_t x1 = Div4096(mul32(ComputeCos(a), r));
			const int32_t y1 = Div4096(mul32(ComputeSin(a), r));
			F<uint32_t>(p, 0) = 0x3000000;
			p[7] = 0x40;
			F<int16_t>(p, 8) = (int16_t)(n->pos[0] + x0 + 0xA0);
			F<int16_t>(p, 0xC) = (int16_t)(n->pos[0] + x1 + 0xA0);
			p[4] = 0;
			F<int16_t>(p, 0xA) = (int16_t)(n->pos[1] + y0 + 0x6C);
			p[5] = 0xFF;
			p[6] = 0;
			out[0] = (int16_t)(n->pos[0] + x1 + 0xA0);
			F<int16_t>(p, 0xE) = (int16_t)(n->pos[1] + y1 + 0x6C);
			out[1] = (int16_t)(n->pos[1] + y1 + 0x6C);
			InsertPrimAutoDepth(ot, p);
			p += 0x10;
			x0 = x1;
			y0 = y1;
		}
		PacketCursor() = (uint32_t)p;
	}

	// sub_7DD330: four lines from the node's screen point to the corners of the scan window
	static void CornerLines(const FxNode *n)
	{
		int16_t o[2] = { (int16_t)(n->pos[0] + 0xA0), (int16_t)(n->pos[1] + 0x6C) };
		int16_t c[2] = { 0x10, 0xC };
		Line(o, c);
		c[0] = 0x130; c[1] = 0xC;
		Line(o, c);
		c[0] = 0x10; c[1] = 0xCC;
		Line(o, c);
		c[0] = 0x130; c[1] = 0xCC;
		Line(o, c);
	}

	// sub_7DD040: the bracket's line list (+0x50), scaled by +0x64, at the node's screen point
	static void BracketLines(const CtlNode *n)
	{
		const uint32_t ot = LinesOT();
		uint8_t *p = (uint8_t *)PacketCursor();
		const uint8_t *s = (const uint8_t *)n->shape;
		while (F<int16_t>(s, 0) != 0xFF)
		{
			p[4] = s[0xC];
			p[6] = s[0xE];
			const int32_t k = n->size;
			p[5] = s[0xD];
			F<uint32_t>(p, 0) = 0x3000000;
			p[7] = 0x40;
			F<int16_t>(p, 8) = (int16_t)(Div4096(F<int16_t>(s, 4) * k) + n->h.pos[0] + 0xA0);
			F<int16_t>(p, 0xA) = (int16_t)(Div4096(F<int16_t>(s, 6) * k) + n->h.pos[1] + 0x6C);
			F<int16_t>(p, 0xC) = (int16_t)(Div4096(F<int16_t>(s, 8) * k) + n->h.pos[0] + 0xA0);
			F<int16_t>(p, 0xE) = (int16_t)(Div4096(F<int16_t>(s, 0xA) * k) + n->h.pos[1] + 0x6C);
			InsertPrimAutoDepth(ot, p);
			s += 0x10;
			p += 0x10;
		}
		PacketCursor() = (uint32_t)p;
	}

	// sub_7DC9F0 (render 0x572200) / sub_7DDF40 (render 0x8DE9D0): a HUD prim model at the node's
	// view-space position, turned Y then X then Z, scaled (the node's matrix is the GTE's)
	static void DrawHudModel(const ModelNode *n, bool alt)
	{
		if (n->h.flags & 4) return;
		Mat4x3 m;
		IdentityMatrix(&m);
		RotateY(&m, n->angle[1]);
		RotateX(&m, n->angle[0]);
		RotateZ(&m, n->angle[2]);
		m.t[0] = n->h.pos[0];
		m.t[1] = n->h.pos[1];
		m.t[2] = n->h.pos[2];
		Scale3DMatrix(&m, n->scale);
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		uint8_t *h = (uint8_t *)FieldAlloc(0x58);
		F<uint32_t>(h, 0) = n->model;
		F<uint32_t>(h, 8) = *(const uint32_t *)n->rgb;
		F<int32_t>(h, 0xC) = n->fade;
		F<uint32_t>(h, 0x1C) = 0xF0;
		if (alt) PacketCursor() = RenderPrimModelB(h, RenderOT(), 2, PacketCursor());
		else PacketCursor() = RenderPrimModel(h, RenderOT(), 2, PacketCursor());
		FieldFree(0x58);
	}

	// sub_7DD6C0: a HUD sprite (flipbook frame) at the node's view-space position, scale s
	static void DrawHudSprite(const SpriteNode *n, int16_t s)
	{
		if (n->h.flags & 4) return;
		uint8_t *h = (uint8_t *)FieldAlloc(0xB4);
		int32_t sv[3] = { s, s, s };
		Mat4x3 m;
		IdentityMatrix(&m);
		Scale3DMatrix(&m, sv);
		m.t[0] = n->h.pos[0];
		m.t[1] = n->h.pos[1];
		m.t[2] = n->h.pos[2];
		GteSetRotMatrix(&m);
		GteSetTransVector(&m);
		F<uint32_t>(h, 0) = n->seq;
		F<int16_t>(h, 4) = n->frame;
		F<int16_t>(h, 0x24) = 0;
		PacketCursor() = InitEffectSequenceFromData(h, RenderOT(), 2, PacketCursor());
		FieldFree(0xB4);
	}

	// sub_7DCC80: vertex blend out = a * (1 - t) + b * t (4.12) of the vertex blocks (+4 = count,
	// 8 bytes per vertex from +8), through the GTE (GPF then GPL)
	static void MorphVertices(const uint8_t *a, const uint8_t *b, uint8_t *out, int16_t t)
	{
		const uint8_t *pa = a + 8;
		const uint8_t *pb = b + 8;
		uint8_t *po = out + 8;
		int32_t count = *(const int32_t *)(a + 4);
		int32_t *w = (int32_t *)FieldAlloc(8);
		w[1] = t;
		w[0] = 0x1000 - t;
		for (; count > 0; count--)
		{
			GteSetIR0(w[0]);
			GteLoadIR123(pa);
			GteGPF();
			GteSetIR0(w[1]);
			GteLoadIR123(pb);
			GteGPL();
			GteStoreIR123(po);
			pa += 8;
			pb += 8;
			po += 8;
		}
		FieldFree(8);
	}

	// ------------------------------------------------------------------
	// Camera (0x7DD950) helpers
	// ------------------------------------------------------------------
	// sub_7DDC80: one blend step (t += step, capped at 0x1000: 1 = arrived)
	static int32_t CameraBlend(CamState &c)
	{
		int32_t arrived = 0;
		c.t = (int16_t)(c.t + c.step);
		if (c.t >= 0x1000)
		{
			c.t = 0x1000;
			arrived = 1;
		}
		auto lerp = [&](int16_t a, int16_t b) { return (int16_t)(Div4096(((int32_t)b - (int32_t)a) * c.t) + a); };
		c.yaw = lerp(c.yaw0, c.yaw1);
		c.dist = lerp(c.dist0, c.dist1);
		c.h = lerp(c.h0, c.h1);
		c.proj = lerp(c.proj0, c.proj1);
		c.look[0] = lerp(c.look0[0], c.look1[0]);
		c.look[1] = lerp(c.look0[1], c.look1[1]);
		c.look[2] = lerp(c.look0[2], c.look1[2]);
		return arrived;
	}

	// sub_7DDDC0 (math part): eye = look-at + RotY(yaw) * (0, h, dist)
	static void CameraEye(CamState &c)
	{
		Mat4x3 m;
		IdentityMatrix(&m);
		RotateY(&m, c.yaw);
		int16_t v[4];
		int16_t out[4];
		v[0] = 0;
		v[1] = c.h;
		v[2] = c.dist;
		MatrixMulVector(&m, v, out);
		c.eye[0] = (int16_t)(c.look[0] + out[0]);
		c.eye[1] = (int16_t)(c.look[1] + out[1]);
		c.eye[2] = (int16_t)(c.look[2] + out[2]);
	}

	// sub_7DDDC0: the eye, then the battle camera words and the projection
	static void CameraApply()
	{
		CamState &c = Cam();
		CameraEye(c);
		var<uint32_t>(0xB8B7F8) = *(const uint32_t *)&c.look[0];
		var<uint32_t>(0xB8B7FC) = *(const uint32_t *)&c.look[2];
		var<uint32_t>(0xB8B7F0) = *(const uint32_t *)&c.eye[0];
		var<uint32_t>(0xB8B7F4) = *(const uint32_t *)&c.eye[2];
		var<int16_t>(0x1D8E038) = c.proj;
	}

	// sub_7DD9B0: the blend's start (current camera) and end (the view of the target for `mode`)
	static void CameraSetup(CtlNode *n)
	{
		const uint8_t *info = InfoOf(&n->h);
		uint8_t *ent = Entity(n->h.slot);
		CamState &c = Cam();
		c.t = 0;
		switch (n->mode)
		{
		case 0:
		case 2:
			c.dist1 = (int16_t)((int32_t)F<int16_t>(info, 0) * 3 / 4);
			c.h1 = (int16_t)(c.dist1 / 2);
			c.step = 0x100;
			break;
		case 1:
			c.dist1 = F<int16_t>(info, 0);
			c.h1 = 0;
			c.step = 0x2AA;
			break;
		case 3:
			c.dist1 = (int16_t)((int32_t)F<int16_t>(info, 0) * 3 / 4);
			c.h1 = (int16_t)(c.dist1 / 2);
			c.step = 0x200;
			break;
		default: break;
		}
		c.proj0 = var<int16_t>(0x1D8E038);
		c.proj1 = 0x240;
		const int32_t ex = (int32_t)var<int16_t>(0xB8B7F0) - var<int16_t>(0xB8B7F8);
		const int32_t ez = (int32_t)var<int16_t>(0xB8B7F4) - var<int16_t>(0xB8B7FC);
		const int32_t d = Sqrt((int32_t)((uint32_t)mul32(ez, ez) + (uint32_t)mul32(ex, ex)));
		c.dist0 = (int16_t)-d;
		c.h0 = (int16_t)(var<int16_t>(0xB8B7F2) - var<int16_t>(0xB8B7FA));
		c.yaw1 = F<int16_t>(MainEntity(), 0xE);
		c.yaw0 = (int16_t)((CartesianToGameAngle(var<int16_t>(0x1D97788), var<int16_t>(0x1D9777C)) - 0x400) & 0xFFF);
		if ((uint32_t)(int32_t)n->mode <= 3)
		{
			int16_t v[4];
			if (n->mode == 1)
			{
				GetEffectSpawnPosition(MainEntity(), 0xF1, 0, c.look1);
				if (F<int16_t>(info, 8) == 0)
				{
					memcpy(&c.look1[0], MainEntity() + 0x1C, 4);
					memcpy(&c.look1[2], MainEntity() + 0x20, 4);
				}
				const int16_t x = F<int16_t>(info, 2);
				c.look1[1] = 0;
				v[0] = x;
			}
			else
			{
				GetEffectSpawnPosition(ent, 0xF1, 0, c.look1);
				if (F<int16_t>(info, 8) == 0)
				{
					memcpy(&c.look1[0], ent + 0x1C, 4);
					memcpy(&c.look1[2], ent + 0x20, 4);
				}
				c.look1[1] = 0;
				v[0] = 0;
			}
			v[1] = F<int16_t>(info, 4);
			v[2] = 0;
			Mat4x3 m;
			IdentityMatrix(&m);
			RotateY(&m, F<int16_t>(MainEntity(), 0xE));
			int16_t out[4];
			MatrixMulVector(&m, v, out);
			c.look1[0] = (int16_t)(c.look1[0] + out[0]);
			c.look1[1] = (int16_t)(c.look1[1] + out[1]);
			c.look1[2] = (int16_t)(c.look1[2] + out[2]);
		}
		memcpy(&c.look0[0], (const void *)0xB8B7F8, 4);
		memcpy(&c.look0[2], (const void *)0xB8B7FC, 4);
		n->h.phase++;
	}
}
}

#ifdef FF8_FX_HELD
#include "mag068_scan_held.h"
#endif

namespace ff8fx
{
namespace scan068
{
	static inline uint32_t TaskEnd(FxNode *n)
	{
		n->counter++;
		if ((n->flags & 1) && n->children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Camera (0x7DD950) and camera return (0x7DE830)
	// ------------------------------------------------------------------
	static uint32_t __cdecl CameraTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0:
			CameraSetup(n);
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note_camera(n);)
			break;
		case 1: // 0x7DDC60: blend, done when arrived
			if (CameraBlend(Cam()) == 1)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			CameraApply();
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note_camera(n);)
			break;
		default: break; // 0x7DDE80
		}
		return TaskEnd(&n->h);
	}

	static uint32_t __cdecl CamRestoreTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x7DE890: the battle's saved camera back
			var<uint32_t>(0xB8B7F8) = var<uint32_t>(0xB8B808);
			var<uint32_t>(0xB8B7FC) = var<uint32_t>(0xB8B80C);
			var<uint32_t>(0xB8B7F4) = var<uint32_t>(0xB8B804);
			var<int16_t>(0x1D8E038) = var<int16_t>(0x1D977A0);
			var<uint32_t>(0xB8B7F0) = var<uint32_t>(0xB8B800);
			n->timer = 1;
			n->h.phase++;
			break;
		case 1: // 0x7DE8E0
			n->timer--;
			if (n->timer <= 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DE900
		}
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// Screen-space line parts
	// ------------------------------------------------------------------
	// Corner bracket (0x7DCF10): at a fixed corner of the scan window
	static uint32_t __cdecl CornerTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x7DCF70
			switch (n->index)
			{
			case 0: n->shape = 0x1595570; n->h.pos[0] = (int16_t)-0x90; n->h.pos[1] = (int16_t)-0x60; break;
			case 1: n->shape = 0x15955A0; n->h.pos[0] = 0x90; n->h.pos[1] = (int16_t)-0x60; break;
			case 2: n->shape = 0x15955D0; n->h.pos[0] = (int16_t)-0x90; n->h.pos[1] = 0x60; break;
			case 3: n->shape = 0x1595600; n->h.pos[0] = 0x90; n->h.pos[1] = 0x60; break;
			default: break;
			}
			n->size = 0x1000;
			n->h.phase++;
			break;
		case 1: // 0x7DD010
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note(REC_BRACKET, n);)
			BracketLines(n);
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DD140
		}
		return TaskEnd(&n->h);
	}

	// Sliding bracket (0x7DD150): on the reticle, then sliding out to its corner while growing
	static uint32_t __cdecl BracketTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x7DD1C0
			memcpy(n->h.pos, n->h.parent->pos, 8);
			switch (n->index)
			{
			case 0: n->shape = 0x1595630; n->h.pos[0] = 0; n->h.pos[1] = 0; n->dx = -1; n->dy = -1; break;
			case 1: n->shape = 0x1595660; n->h.pos[0] = 0; n->h.pos[1] = 0; n->dx = 1; n->dy = -1; break;
			case 2: n->shape = 0x1595690; n->h.pos[0] = 0; n->h.pos[1] = 0; n->dx = -1; n->dy = 1; break;
			case 3: n->shape = 0x15956C0; n->h.pos[0] = 0; n->h.pos[1] = 0; n->dx = 1; n->dy = 1; break;
			default: break;
			}
			n->size = 0x1000;
			n->timer = 0x1E;
			n->h.phase++;
			break;
		case 1: // 0x7DD280: follows the reticle
			memcpy(n->h.pos, n->h.parent->pos, 8);
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note(REC_BRACKET_FOLLOW, n);)
			BracketLines(n);
			n->timer--;
			if (n->timer < 0)
			{
				n->timer = 4;
				n->h.phase++;
			}
			break;
		case 2: // 0x7DD2C0: slides out and grows
			n->h.pos[0] = (int16_t)(n->h.pos[0] + n->dx);
			n->h.pos[1] = (int16_t)(n->h.pos[1] + n->dy);
			n->size = (int16_t)(n->size + 0x400);
			n->timer--;
			if (n->timer < 0) n->h.phase++;
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note(REC_BRACKET, n);)
			BracketLines(n);
			break;
		case 3: // 0x7DD2F0
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note(REC_BRACKET, n);)
			BracketLines(n);
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DD320
		}
		return TaskEnd(&n->h);
	}

	// Ring (0x7DD510): a fan growing around the right of the window, a pointer line and a sprite
	static uint32_t __cdecl RingTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		int16_t out[2];
		switch (n->h.phase)
		{
		case 0: // 0x7DD580
			n->timer = 0;
			n->h.phase++;
			break;
		case 1: // 0x7DD5A0
			n->timer = (int16_t)(n->timer + 4);
			if (n->timer >= 0x18)
			{
				n->timer = 0x18;
				n->h.phase++;
			}
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note(REC_RING1, n);)
			Fan(&n->h, 0x30, 0x600, n->timer, out);
			break;
		case 2: // 0x7DD5E0
		{
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note(REC_RING2, n);)
			Fan(&n->h, 0x30, 0x600, n->timer, out);
			const int16_t p[2] = { (int16_t)(out[0] + 0x18), (int16_t)(out[1] + 0x18) };
			Line(out, p);
			AddEffectTask(QSprites(), ORIG_Sprite, 0x58, n);
			n->h.phase++;
			break;
		}
		case 3: // 0x7DD880
		{
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note(REC_RING3, n);)
			Fan(&n->h, 0x30, 0x600, n->timer, out);
			const int16_t p[2] = { (int16_t)(out[0] + 0x18), (int16_t)(out[1] + 0x18) };
			Line(out, p);
			const int16_t q[2] = { (int16_t)(p[0] + 0x58), p[1] };
			Line(p, q);
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		}
		default: break; // 0x7DD910
		}
		return TaskEnd(&n->h);
	}

	// Reticle (0x7DC6F0)
	static uint32_t __cdecl ReticleTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		int16_t out[2];
		switch (n->h.phase)
		{
		case 0: // 0x7DC760
		{
			const int16_t *path = (const int16_t *)RETICLE_PATH + 2 * n->h.counter;
			n->h.pos[0] = path[0];
			n->h.pos[1] = path[1];
			for (int16_t i = 0; i < 4; i++) ((CtlNode *)AddEffectTask(QControls(), ORIG_Bracket, 0x78, n))->index = i;
			for (int16_t i = 0; i < 4; i++) ((CtlNode *)AddEffectTask(QControls(), ORIG_Corner, 0x78, n))->index = i;
			AddEffectTask(QModels(), ORIG_Morph, 0x6C, n);
			AddEffectTask(QModels(), ORIG_Pair, 0x6C, n);
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note(REC_RETICLE, n);)
			Fan(&n->h, 8, 0, 0x20, out);
			CornerLines(&n->h);
			n->h.phase++;
			break;
		}
		case 1: // 0x7DD440: along the path
		{
			const int16_t *path = (const int16_t *)RETICLE_PATH + 2 * n->h.counter;
			if (path[0] == 0x7FFF)
			{
				n->timer = 0;
				n->h.phase++;
			}
			else
			{
				n->h.pos[0] = path[0];
				n->h.pos[1] = path[1];
			}
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note(REC_RETICLE, n);)
			Fan(&n->h, 8, 0, 0x20, out);
			CornerLines(&n->h);
			n->timer = 8;
			break;
		}
		case 2: // 0x7DD4A0: the circle shrinks
			n->timer = (int16_t)(n->timer - 2);
			if (n->timer > 0)
			{
				// 30 fps layer: see mag068_scan_held.inc
				FX_HELD(held_note(REC_RETICLE_CLOSE, n);)
				Fan(&n->h, n->timer, 0, 0x20, out);
				CornerLines(&n->h);
			}
			else n->h.phase++;
			break;
		case 3: // 0x7DD4E0
			AddEffectTask(QControls(), ORIG_Ring, 0x78, n);
			n->h.phase++;
			break;
		case 4: // 0x7DD920
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DD940
		}
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// HUD models
	// ------------------------------------------------------------------
	// Model pair (0x7DC950): two models along the tables 0x1595440 / 0x1595488 / 0x15953F8 /
	// 0x159541C for ticks 3..17, then the second one pulses
	static void PairPhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0: // 0x7DCAD0
			*(uint8_t *)&n->h.flags |= 4;
			n->h.pos[2] = 0x480;
			n->scale[2] = 0x1000;
			n->angle[2] = 0xA00;
			n->h.phase++;
			break;
		case 1: // 0x7DCB00
			if (n->h.counter >= 3) n->h.phase++;
			break;
		case 2: // 0x7DCB10
		{
			const int16_t k = n->h.counter;
			*(uint8_t *)&n->h.flags &= 0xFB;
			n->h.pos[0] = (int16_t)(((const int16_t *)0x1595440)[2 * k] - 0x3C);
			n->h.pos[1] = (int16_t)(((const int16_t *)0x1595440)[2 * k + 1] + 0x3C);
			n->scale[0] = ((const int16_t *)0x1595488)[2 * k];
			n->scale[1] = ((const int16_t *)0x1595488)[2 * k + 1];
			n->fade1 = ((const int16_t *)0x15953F8)[k];
			n->fade2 = ((const int16_t *)0x159541C)[k];
			if (k >= 0x11) n->h.phase++;
			break;
		}
		case 3: // 0x7DCB80
			n->wave = (int16_t)((n->wave + 0x200) & 0xFFF);
			n->fade2 = (int16_t)((int16_t)(ComputeSin(n->wave) + 0x1000) / 4);
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DCBD0
		}
	}

	static void PairDraw(ModelNode *n)
	{
		n->fade = n->fade1;
		n->model = 0x1592794;
		DrawHudModel(n, false);
		n->fade = n->fade2;
		n->model = 0x1592810;
		DrawHudModel(n, false);
	}

	static uint32_t __cdecl PairTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		PairPhase(n);
		// 30 fps layer: see mag068_scan_held.inc
		FX_HELD(held_note(REC_PAIR, n);)
		PairDraw(n);
		return TaskEnd(&n->h);
	}

	// Morph model (0x7DCBE0): spins in while shrinking, then morphs twice
	static void MorphPhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0: // 0x7DCD20
			n->h.pos[0] = 0;
			n->h.pos[1] = 0;
			n->h.pos[2] = 0x480;
			n->scale[2] = 0x3FF8;
			n->scale[1] = 0x3FF8;
			n->scale[0] = 0x3FF8;
			n->angle[0] = 0x5DC;
			n->angle[1] = 0x2A0;
			n->angle[2] = (int16_t)0xF808;
			n->model = 0x1591C44;
			n->morph_a = 0x159390C;
			n->morph_b = 0x1593A94;
			n->fade = 0x1000;
			n->h.phase++;
			break;
		case 1: // 0x7DCD80
			n->fade = (int16_t)(n->fade - 0x155);
			if (n->fade < 0) n->fade = 0;
			n->angle[0] = (int16_t)((n->angle[0] - 0x7D) & 0xFFF);
			n->angle[1] = (int16_t)((n->angle[1] - 0x38) & 0xFFF);
			n->angle[2] = (int16_t)((n->angle[2] + 0xAA) & 0xFFF);
			n->scale[0] += -0x4AA;
			if (n->scale[0] <= 0x800)
			{
				n->scale[0] = 0x800;
				n->timer = 0xC;
				n->h.phase++;
			}
			n->scale[2] = n->scale[0];
			n->scale[1] = n->scale[0];
			break;
		case 2: // 0x7DCE10
			n->angle[2] = (int16_t)((n->angle[2] + 0xAA) & 0xFFF);
			n->timer--;
			if (n->timer <= 0) n->h.phase++;
			break;
		case 3: // 0x7DCE40
			n->blend = (int16_t)(n->blend + 0x200);
			n->angle[2] = (int16_t)((n->angle[2] + 0xAA) & 0xFFF);
			if (n->blend >= 0x1000)
			{
				n->blend = 0;
				n->model = 0x159202C;
				n->morph_a = 0x1593C1C;
				n->morph_b = 0x1594124;
				n->h.phase++;
			}
			break;
		case 4: // 0x7DCE90
			n->blend = (int16_t)(n->blend + 0x400);
			if (n->blend >= 0x1000)
			{
				n->blend = 0x1000;
				n->h.phase++;
			}
			break;
		case 5: // 0x7DCEC0
			if ((int8_t)((uint8_t)n->h.counter & 7) < 4) n->angle[2] = (int16_t)((n->angle[2] + 0x2A) & 0xFFF);
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DCF00
		}
	}

	static uint32_t __cdecl MorphTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		MorphPhase(n);
		// 30 fps layer: see mag068_scan_held.inc
		FX_HELD(held_note(REC_MORPH, n);)
		MorphVertices((const uint8_t *)n->morph_a, (const uint8_t *)n->morph_b, (uint8_t *)n->model, n->blend);
		DrawHudModel(n, false);
		return TaskEnd(&n->h);
	}

	// Stat bar (0x7DE200): one bar of the target's stats (HP, then the bytes +0xA5..+0xA9, +0xAB of
	// its battle stats 0x1D27B28 + slot * 0xD0), growing to its value, pulsing
	static int16_t BarWidth(uint8_t *stats, int which)
	{
		int32_t d;
		if (which <= 1)
		{
			int32_t v = F<int32_t>(stats, 0);
			if (v > 0x3E7) v = 0x3E7;
			const int32_t s = shl32(v, 0xE);
			d = (int32_t)(((int64_t)s * (int32_t)0x83340521) >> 32) + s;
			d >>= 9;
		}
		else if (which <= 6)
		{
			int32_t v = stats[0xA5 + (which - 2)];
			if (v > 0x7F) v = 0x7F;
			const int32_t s = shl32(v, 0xE);
			d = (int32_t)(((int64_t)s * (int32_t)0x81020409) >> 32) + s;
			d >>= 6;
		}
		else
		{
			int32_t v = stats[0xAB];
			if (v > 0x19) v = 0x19;
			const int32_t s = shl32(v, 0xE);
			d = (int32_t)(((int64_t)s * 0x51EB851F) >> 32);
			d >>= 3;
		}
		return (int16_t)(d + (int32_t)((uint32_t)d >> 31));
	}

	static void StatBarPhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0: // 0x7DE2A0
		{
			const int16_t k = n->which;
			const FxNode *parent = n->h.parent;
			const int16_t *tbl = (const int16_t *)(n->table == 0 ? 0x15954D0 : 0x1595520) + 5 * k;
			*(uint8_t *)&n->h.flags |= 4;
			if ((uint32_t)(int32_t)k <= 7)
			{
				static const uint32_t MODELS[8] = { 0x1593554, 0x1593554, 0x15935DC, 0x1593664, 0x15936EC, 0x1593774, 0x15937FC, 0x1593884 };
				n->model = MODELS[k];
				n->max = BarWidth((uint8_t *)0x1D27B28 + 0xD0 * n->h.slot, k);
			}
			n->scale[0] = 0;
			n->scale[1] = 0x1000;
			n->scale[2] = 0x1000;
			n->h.pos[0] = (int16_t)(tbl[0] / 2 + parent->pos[0]);
			n->h.pos[2] = parent->pos[2];
			n->h.pos[1] = (int16_t)(tbl[1] / 2 + parent->pos[1]);
			n->angle[2] = tbl[2];
			n->wave = tbl[4];
			n->timer = tbl[3];
			n->h.phase++;
			break;
		}
		case 1: // 0x7DE4A0: shown after its delay
			n->timer--;
			if (n->timer < 0)
			{
				*(uint8_t *)&n->h.flags &= 0xFB;
				n->h.phase++;
			}
			break;
		case 2: // 0x7DE4C0: grows to its width
			n->scale[0] += 0x300;
			if (n->scale[0] > n->max) n->scale[0] = n->max;
			if (Closing() == 1)
			{
				n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DE500
		}
	}

	// pulse: fade = (sin(wave) + 0x1000) / 6
	static void StatBarPulse(ModelNode *n)
	{
		n->wave = (int16_t)((n->wave + 0x100) & 0xFFF);
		const int32_t v = (int16_t)(ComputeSin(n->wave) + 0x1000);
		const int32_t hi = (int32_t)(((int64_t)v * 0x2AAAAAAB) >> 32);
		n->fade = (int16_t)(hi + (int32_t)((uint32_t)hi >> 31));
	}

	static uint32_t __cdecl StatBarTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		StatBarPhase(n);
		StatBarPulse(n);
		// 30 fps layer: see mag068_scan_held.inc
		FX_HELD(held_note(REC_STATBAR, n);)
		DrawHudModel(n, false);
		return TaskEnd(&n->h);
	}

	static uint32_t __cdecl AltBoardTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x7DE180
			n->scale[0] = 0x1000;
			n->scale[1] = 0x1000;
			n->scale[2] = 0x1000;
			n->h.pos[0] = (int16_t)-0x88;
			n->h.pos[1] = (int16_t)-0x3C;
			n->h.pos[2] = 0x240;
			n->model = 0x159346C;
			n->h.phase++;
			break;
		case 1: // 0x7DE1C0: bars 1..7 of table 1
			for (int16_t i = 1; i < 8; i++)
			{
				ModelNode *b = (ModelNode *)AddEffectTask(QModels(), ORIG_StatBar, 0x6C, n);
				b->which = i;
				b->table = 1;
			}
			n->h.phase++;
			break;
		case 2: // 0x7DE510
			if (Closing() == 1)
			{
				n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DE530
		}
		// 30 fps layer: see mag068_scan_held.inc
		FX_HELD(held_note(REC_STATIC, n);)
		DrawHudModel(n, false);
		return TaskEnd(&n->h);
	}

	// Alt spinner (0x7DDED0): a model turning in (y 0x400 -> 0), until closing (render 0x8DE9D0)
	static void AltSpinnerPhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0: // 0x7DE020
			n->scale[0] = 0x1000;
			n->scale[1] = 0x1000;
			n->scale[2] = 0x1000;
			n->h.pos[0] = (int16_t)-0x200;
			n->h.pos[1] = 0;
			n->h.pos[2] = 0x6C0;
			n->model = 0x15934CC;
			n->angle[1] = 0x400;
			n->h.phase++;
			break;
		case 1: // 0x7DE060
			n->angle[1] = (int16_t)(n->angle[1] - 0x80);
			if (n->angle[1] <= 0)
			{
				n->angle[1] = 0;
				n->h.phase++;
			}
			break;
		case 2: // 0x7DE090
			if (Closing() == 1)
			{
				n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DE0B0
		}
	}

	static uint32_t __cdecl AltSpinnerTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		AltSpinnerPhase(n);
		// 30 fps layer: see mag068_scan_held.inc
		FX_HELD(held_note(REC_ALTSPINNER, n);)
		DrawHudModel(n, true);
		return TaskEnd(&n->h);
	}

	// Sprite (0x7DD650): an opening flipbook, then a looping one
	static uint32_t __cdecl SpriteTask(TaskNode *t)
	{
		SpriteNode *n = (SpriteNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x7DD780
			n->h.pos[0] = 0x190;
			n->h.pos[1] = 0xC0;
			n->h.pos[2] = 0x900;
			n->seq = 0x159462C;
			n->last = 0x1F;
			n->h.phase++;
			break;
		case 1: // 0x7DD7B0 (frame step 0x7DD7F0 inlined)
			n->frame++;
			if (n->frame > n->last)
			{
				*(uint8_t *)&n->h.flags |= 4;
				n->frame = n->last;
				*(uint8_t *)&n->h.flags &= 0xFB;
				n->seq = 0x1594950;
				n->frame = 0;
				n->last = 3;
				n->h.phase++;
			}
			break;
		case 2: // 0x7DD820 (frame step 0x7DD850 inlined)
			n->frame++;
			if (n->frame > n->last) n->frame = 0;
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DD870
		}
		// 30 fps layer: see mag068_scan_held.inc
		FX_HELD(held_note(REC_SPRITE, n);)
		DrawHudSprite(n, 0x400);
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// Text pager (0x7DE540)
	// ------------------------------------------------------------------
	static void TextOpen(CtlNode *n)
	{
		TextDone() = 0;
		n->lines = (int16_t)(uint16_t)ScanTextLines(n->h.slot);
		n->h.phase++;
	}

	static void TextShowLine(CtlNode *n)
	{
		int16_t rect[4] = { 0x10, (int16_t)0xB4, 0x128, 0x1C };
		const void *s = ScanTextLine(n->h.slot, (uint8_t)n->line);
		TextSetLayerText(1, s);
		TextLayer4A0700(1, 0);
		TextSetPosition(1, rect);
		TextLayer49FBC0(1, 0);
		TextLayer4A0640(1);
		n->timer = 0x2D;
		n->h.phase++;
	}

	static void TextClose(CtlNode *n)
	{
		TextLayer4A0680(1);
		TextSetLayerText(1, nullptr);
		TextDone() = 1;
		*(uint8_t *)&n->h.flags |= 1;
		n->h.phase++;
	}

	static uint32_t __cdecl AltTextTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		n->prev = n->pad;
		n->pad = RemapPadInput(ReadPadHeldRaw(0, 0)) & 0xFFFF;
		switch (n->h.phase)
		{
		case 0: TextOpen(n); break;       // 0x7DE5D0
		case 1: TextShowLine(n); break;   // 0x7DE600
		case 2: // 0x7DE680: 0x40 closes, 0x20 or the timer shows the next line; past the last one closes
			if (n->pad & 0x40)
			{
				n->h.phase++;
				break;
			}
			if (!(n->prev & 0x20) && (n->pad & 0x20)) { }
			else
			{
				n->timer--;
				if (n->timer > 0) break;
			}
			n->line++;
			if (n->line < n->lines) n->h.phase--;
			else n->h.phase++;
			break;
		case 3: TextClose(n); break;      // 0x7DE6D0
		default: break;                   // 0x7DE700
		}
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// Scanner (0x7DC600)
	// ------------------------------------------------------------------
	static void MarkTarget(CtlNode *n)
	{
		Entity(n->h.slot)[1] |= 0x78;
	}

	static uint32_t __cdecl AltScannerTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x7DC680
			MarkTarget(n);
			((CtlNode *)AddEffectTask(QControls(), ORIG_Camera, 0x78, n))->mode = 2;
			HudActive() = 1;
			AddEffectTask(QControls(), ORIG_Reticle, 0x78, n);
			n->timer = 0x2D;
			n->h.phase++;
			break;
		case 1: // 0x7DDE90
			n->timer--;
			if (n->timer < 0)
			{
				AddEffectTask(QModels(), ORIG_AltSpinner, 0x6C, n);
				n->timer = 8;
				n->h.phase++;
			}
			break;
		case 2: // 0x7DE0C0
			n->timer--;
			if (n->timer < 0)
			{
				AddEffectTask(QControls(), ORIG_AltText, 0x78, n);
				AddEffectTask(QModels(), ORIG_AltBoard, 0x6C, n);
				n->h.phase++;
			}
			break;
		case 3: // 0x7DE710: the camera circles the target until the text is closed
		{
			const uint8_t *info = InfoOf(&n->h);
			CamState &c = Cam();
			if (info[0xE] == 1) c.yaw = (int16_t)((c.yaw + 0x40) & 0xFFF);
			if (F<int16_t>(info, 4) < (int16_t)0xFE80)
			{
				c.h = (int16_t)(c.h - c.h1 / 32);
				if (c.h > 0) c.h = 0;
			}
			CameraApply();
			if (TextDone() == 1)
			{
				HudActive() = 0;
				Closing() = 1;
				n->h.phase++;
			}
			// 30 fps layer: see mag068_scan_held.inc
			FX_HELD(held_note_camera(n);)
			break;
		}
		case 4: // 0x7DE7A0
			if (n->h.children == 0)
			{
				uint8_t *e = Entity(n->h.slot);
				if ((int16_t)n->h.action == ((RootNode *)n->h.root)->last_action)
				{
					AddEffectTask(QControls(), ORIG_CamRestore, 0x78, n);
					F<uint16_t>(e, 0) &= 0x87FF;
					n->h.phase++;
					break;
				}
				if (Cam().dist <= (int16_t)0xE000) ((CtlNode *)AddEffectTask(QControls(), ORIG_Camera, 0x78, n))->mode = 3;
				F<uint16_t>(e, 0) &= 0x87FF;
				n->h.phase++;
			}
			break;
		case 5: // 0x7DE910
			if (n->h.children == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x7DE930
		}
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// Emitter (0x7DC3A0) and root (0x7DC280)
	// ------------------------------------------------------------------
	static uint32_t __cdecl EmitterTask(TaskNode *t)
	{
		EmitterNode *e = (EmitterNode *)t;
		if (e->h.counter == 0)
		{
			int16_t mid[4]; // read and not used
			CopyMidpointFromSource(e, mid);
			BdPlaySE(SOUND_Scan, 0, 0x80);
		}
		EmitterUpdatePos(e);
		ComputeModelBounds(e);
		switch (e->h.phase)
		{
		case 0: // 0x7DC430: the scanned entities, the scanner (always the short sequence)
			Confirmed() = 0;
			Closing() = 0;
			FindScannedEntities((int16_t)(uint16_t)e->h.slot);
			AddEffectTask(QControls(), ORIG_AltScanner, 0x78, e);
			e->h.phase++;
			break;
		case 1: // 0x7DE940: the action result once the scanner is gone
			if (e->h.children == 0)
			{
				((RootNode *)e->h.root)->busy = 0;
				ApplyActionResultToTarget(e->h.ctx->actions[e->h.action].targets);
				*(uint8_t *)&e->h.flags |= 1;
				e->h.phase++;
			}
			break;
		default: break; // 0x7DE980
		}
		return TaskEnd(&e->h);
	}

	static void RootPhase(RootNode *r)
	{
		switch (r->h.phase)
		{
		case 0: // 0x7DC350
			r->h.phase++;
			break;
		case 1: // 0x7DC360
			r->busy = 1;
			AddEffectTask(QEmitters(), ORIG_Emitter, 0x58, r);
			CameraArmReturn();
			BattleMenuShow(0);
			r->h.phase++;
			break;
		case 2: // 0x7DE990
			if (r->busy == 0)
			{
				const int8_t a = r->h.action;
				if ((int16_t)a < r->last_action)
				{
					r->h.action = (int8_t)(a + 1);
					r->h.phase--;
				}
				else r->h.phase++;
			}
			break;
		case 3: // 0x7DE9C0
			if (r->alive == 0)
			{
				BattleMenuShow(1);
				*(uint8_t *)&r->h.flags |= 1;
				r->h.phase++;
			}
			break;
		default: break; // 0x7DE9F0
		}
	}

	static uint32_t __cdecl RootTask(TaskNode *t)
	{
		RootNode *r = (RootNode *)t;
		if (r->tick & 1) PacketCursor() = var<uint32_t>(0x25FFC3C);
		else PacketCursor() = var<uint32_t>(0x25FFC38);
		// 30 fps layer: see mag068_scan_held.inc
		FX_HELD(held_note_root(r);)
		UpdateTargetPosFromBones(r);
		RootPhase(r);
		r->alive = (int16_t)ExecuteTaskQueue(QEmitters());
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QControls()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QSprites()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QModels()));
		r->tick++;
		return TaskEnd(&r->h);
	}
}

	void register_mag068_scan()
	{
		using namespace scan068;
		register_port(ORIG_Root, (void *)RootTask, "068 Root", 68);
		register_port(ORIG_Emitter, (void *)EmitterTask, "068 Emitter", 68);
		register_port(ORIG_AltScanner, (void *)AltScannerTask, "068 AltScanner", 68);
		register_port(ORIG_Reticle, (void *)ReticleTask, "068 Reticle", 68);
		register_port(ORIG_Pair, (void *)PairTask, "068 Pair", 68);
		register_port(ORIG_Morph, (void *)MorphTask, "068 Morph", 68);
		register_port(ORIG_Corner, (void *)CornerTask, "068 Corner", 68);
		register_port(ORIG_Bracket, (void *)BracketTask, "068 Bracket", 68);
		register_port(ORIG_Ring, (void *)RingTask, "068 Ring", 68);
		register_port(ORIG_Sprite, (void *)SpriteTask, "068 Sprite", 68);
		register_port(ORIG_Camera, (void *)CameraTask, "068 Camera", 68);
		register_port(ORIG_StatBar, (void *)StatBarTask, "068 StatBar", 68);
		register_port(ORIG_CamRestore, (void *)CamRestoreTask, "068 CamRestore", 68);
		register_port(ORIG_AltSpinner, (void *)AltSpinnerTask, "068 AltSpinner", 68);
		register_port(ORIG_AltBoard, (void *)AltBoardTask, "068 AltBoard", 68);
		register_port(ORIG_AltText, (void *)AltTextTask, "068 AltText", 68);
		// 30 fps layer: see mag068_scan_held.inc
		FX_HELD(register_mag068_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag068_scan_held.inc"
#endif
