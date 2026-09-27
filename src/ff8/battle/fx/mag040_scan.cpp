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

// Effect 40: Scan (spell, MAG_040_SCAN_*).
//
// Structure (module code 0x84D010..0x8513B0: file loader MAG_040_SCAN_FL 0x84D010, setup
// MAG_040_SCAN 0x84D030 sets up the queues and the root node, queues the TIM and takes the two packet
// arenas: texture buffer + 0 (0x269CFC4) / + 0x4000 (0x269CFC0)). Every node starts with the shared
// effect-library header (Effect_AddTaskAndInitFromCtx 0x8DC540); every task runs its current phase
// function (node +0x29) from a small table and ends when it is flagged done (+0x26 bit0) and has no
// live children. No task tests the draw-only flags (battle_to_update_flags 0x201).
//   Root (0x84D110) - packet arena by tick parity (cursor 0x269A1B8), follows the caster's bones;
//     phase 1 spawns the emitter of the current action, arms the camera return (0x50A730) and hides
//     the battle menu (0x4A8480(0)); phase 2 moves to the next action; phase 3 waits for the four
//     queues to be empty, shows the menu again (0x4A8480(1)) and ends. Queues run: emitters,
//     controllers (0x78 nodes), sprites (0x58), models (0x6C).
//   Emitter (0x84D230): sound at its first tick; phase 0 finds the scanned entities (the target and
//     its linked parts, flags 0x269A1BC[7]; main entity 0x269CE30 = 0x501FF0(target)) and spawns the
//     scanner: 0x84D4B0, or 0x850B40 when the target record +2 bit 1 is set (a target that shows no
//     model: short sequence). Phase 1 applies the action result once the scanner is gone.
//   Scanner (0x84D4B0): the target entity is marked (entity +1 |= 0x78), the camera task (0x84E810
//     mode 0) and the reticle; the screen dimmer (0x84ED90), the scan bars (0x84EFF0), the camera
//     (mode 1), then the viewer (0x84F860) and two HUD models; when they are gone the camera
//     returns (0x850A30 for the last action, else mode 3) and the entities are restored (0x8507F0).
//   Reticle (0x84D5B0): a screen-space fan of green lines (0x84D6D0) plus four lines to the screen
//     corners (0x84E1F0) moving along the table 0x15D1898 (one point per tick until 0x7FFF), then
//     shrinking; spawns four corner brackets (0x84DDD0), four sliding brackets (0x84E010), the
//     rotating morph model (0x84DAA0) and the model pair (0x84D810), then the ring (0x84E3D0).
//   Camera (0x84E810): blends the battle camera (eye / look-at 0xB8B7F0.., projection 0x1D8E038)
//     from the current one to a view of the target defined by the target's info record
//     (0x15D0B40 + class * 16, class = 0x84D410), in steps of +0x32 over the camera state
//     0x269A168 (pointer 0x15CDDE8).
//   Viewer (0x84F860): the target's name (layer 2) and scan text (layer 0, 0xB687C0), the text pager
//     (0x84FAF0, layer 1, pad 0x20 = next line, 0x40 = close), the backdrop model (0x84F9E0); every
//     tick the target is turned / zoomed with the pad (0x8501F0: D-pad or analog, 0x49F0A0) around
//     its centre (0x84FD90); on confirm the view is put back (0x8506F0) and the text layers cleared.
//   HUD models are drawn in screen space (GTE set to the node's own matrix: 0x84D8B0 / 0x850CE0,
//     sprites 0x84E580); lines go to the render list + 0x14, the scan bars (POLY_FT4, 0x84F0B0) to
//     + 0x4474. Two models morph their vertices (0x84DB40, GTE GPF / GPL) into their exe data.
// Module globals: 0x269A160..0x269D93C (texture file 0x269A160, camera state 0x269A168, packet
// cursor 0x269A1B8, scanned-entity flags 0x269A1BC, saved entity words 0x269A1D8, model pool / queue
// 0x269A1F8 / 0x269AA68 (20 x 0x6C), name buffer 0x269AA78, HUD active 0x269AA90, pad / analog /
// view input words 0x269AAA8..0x269AAD7, root pool / queue 0x269AAE0 / 0x269ABA8 (2 x 0x64), sprite
// pool / queue 0x269ABB8 / 0x269CE18 (100 x 0x58), closing 0x269CE2C, main entity 0x269CE30, saved
// entity colours 0x269CE34, emitter queue / pool 0x269CE50 / 0x269CE60 (4 x 0x58), arenas
// 0x269CFC0 / 0x269CFC4, controller queue / pool 0x269CFC8 / 0x269CFD8 (20 x 0x78), confirm
// 0x269D938, text done 0x269D93A); the morphed vertex blocks in the exe data 0x15CDDF4 (0x30
// vertices) and 0x15CE1DC (0xA0 vertices); battle entities (flags, colours, positions, matrices,
// angles, +0x30 scale pointer), stage wobble words 0x1D98991 + k * 0x2C, the battle camera and the
// text layers.

#include "mag_common.h"

namespace ff8fx
{
namespace scan040
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
		int32_t scale[3];      // +0x30 viewer: zoom of the target
		uint32_t pad3C;
		int16_t rot[3];        // +0x40 viewer: offset turned with the view (x, y, z)
		int16_t pad46;
		int16_t dx, dy;        // +0x48 sliding bracket: drift per tick
		uint32_t pad4C;
		uint32_t shape;        // +0x50 bracket line list (8 words per line, 0xFF ends)
		int16_t offx;          // +0x54 viewer: target x - its centre x
		int16_t pad56;
		int16_t offz;          // +0x58 viewer: target z - its centre z
		int16_t pad5A;
		uint32_t pad;          // +0x5C text: pad bits
		uint32_t prev;         // +0x60 text: previous pad bits
		int16_t size;          // +0x64 bracket scale
		int16_t timer;         // +0x66
		int16_t mode;          // +0x68 camera mode
		int16_t lines;         // +0x6A text: line count
		int16_t line;          // +0x6C text: line shown
		int16_t fade;          // +0x6E scan bars: brightness
		int16_t moved;         // +0x70 viewer: turned / zoomed by the pad
		int16_t idle;          // +0x72 viewer: ticks until the view is reset
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
	// camera state (0x269A168, pointer at 0x15CDDE8)
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
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x269A1B8); }
	inline TaskQueue *QEmitters() { return (TaskQueue *)0x269CE50; }  // 4 x 0x58
	inline TaskQueue *QControls() { return (TaskQueue *)0x269CFC8; }  // 20 x 0x78
	inline TaskQueue *QSprites() { return (TaskQueue *)0x269CE18; }   // 100 x 0x58
	inline TaskQueue *QModels() { return (TaskQueue *)0x269AA68; }    // 20 x 0x6C
	inline CamState &Cam() { return **(CamState **)0x15CDDE8; }
	inline int32_t &Scanned(int k) { return ((int32_t *)0x269A1BC)[k]; }   // entity k is scanned
	inline uint32_t &SavedWord(int k) { return ((uint32_t *)0x269A1D8)[k]; }
	inline uint32_t &SavedColour(int k) { return ((uint32_t *)0x269CE34)[k]; }
	inline int16_t &HudActive() { return var<int16_t>(0x269AA90); }
	inline int16_t &Closing() { return var<int16_t>(0x269CE2C); }
	inline int16_t &Confirmed() { return var<int16_t>(0x269D938); }
	inline int16_t &TextDone() { return var<int16_t>(0x269D93A); }
	inline uint8_t *MainEntity() { return var<uint8_t *>(0x269CE30); }
	inline int16_t &InW(uint32_t addr) { return var<int16_t>(addr); }       // pad / analog / view words 0x269AAxx
	inline uint32_t LinesOT() { return var<uint32_t>(0x1D8E04C) + 0x14; }

	static const uint32_t ORIG_Root = 0x84D110, ORIG_Emitter = 0x84D230, ORIG_Scanner = 0x84D4B0, ORIG_AltScanner = 0x850B40;
	static const uint32_t ORIG_Reticle = 0x84D5B0, ORIG_Pair = 0x84D810, ORIG_Morph = 0x84DAA0, ORIG_Corner = 0x84DDD0;
	static const uint32_t ORIG_Bracket = 0x84E010, ORIG_Ring = 0x84E3D0, ORIG_Sprite = 0x84E510, ORIG_Camera = 0x84E810;
	static const uint32_t ORIG_Dimmer = 0x84ED90, ORIG_Bars = 0x84EFF0, ORIG_Spinner = 0x84F300, ORIG_Board = 0x84F430;
	static const uint32_t ORIG_StatBar = 0x84F520, ORIG_Viewer = 0x84F860, ORIG_Backdrop = 0x84F9E0, ORIG_Text = 0x84FAF0;
	static const uint32_t ORIG_Restorer = 0x8507F0, ORIG_CamRestore = 0x850A30, ORIG_AltSpinner = 0x850C70;
	static const uint32_t ORIG_AltBoard = 0x850EB0, ORIG_AltText = 0x850FD0;

	static const void *const SOUND_Scan = (const void *)0x15D0B3C;
	static const uint32_t INFO_TABLE = 0x15D0B40;   // 16 bytes per entity class
	static const uint32_t RETICLE_PATH = 0x15D1898; // (x, y) per tick, 0x7FFF ends

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
	inline void InsertPrimAltViewport(uint32_t ot, void *prim) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(ot, prim); }
	inline void CameraArmReturn() { fn<void (__cdecl *)()>(0x50A730)(); }            // BS_Camera_ArmReturnAfterEffect
	inline void BattleMenuShow(int32_t on) { fn<void (__cdecl *)(int32_t)>(0x4A8480)(on); } // sub_4A8480
	// pad
	inline uint32_t ReadPadHeldRaw(int32_t a, int32_t b) { return fn<uint32_t (__cdecl *)(int32_t, int32_t)>(0x49ED30)(a, b); }
	inline uint32_t RemapPadInput(uint32_t raw) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x4A2D60)(raw); }
	inline int32_t ReadAnalog(int32_t port, int32_t axis, int32_t c) { return fn<int32_t (__cdecl *)(int32_t, int32_t, int32_t)>(0x49F0A0)(port, axis, c); }
	// text layers
	inline void TextSetLayerText(int32_t layer, const void *text) { fn<void (__cdecl *)(int32_t, const void *)>(0x4A0410)(layer, text); }
	inline void TextLayer4A0700(int32_t layer, int32_t a) { fn<void (__cdecl *)(int32_t, int32_t)>(0x4A0700)(layer, a); }
	inline void TextSetPosition(int32_t layer, const int16_t *rect) { fn<void (__cdecl *)(int32_t, const int16_t *)>(0x4A07A0)(layer, rect); }
	inline void TextLayer4A0640(int32_t layer) { fn<void (__cdecl *)(int32_t)>(0x4A0640)(layer); }
	inline void TextLayer4A0680(int32_t layer) { fn<void (__cdecl *)(int32_t)>(0x4A0680)(layer); }
	inline void TextLayer49FBC0(int32_t layer, int32_t a) { fn<void (__cdecl *)(int32_t, int32_t)>(0x49FBC0)(layer, a); }
	inline uint8_t ScanTextLines(uint32_t slot) { return (uint8_t)fn<uint32_t (__cdecl *)(uint32_t)>(0xB68390)(slot); }   // manageScanText_Dup
	inline const void *ScanTextLine(uint32_t slot, uint32_t line) { return fn<const void *(__cdecl *)(uint32_t, uint32_t)>(0xB68810)(slot, line); } // ScanText_GetLine_Dup
	inline const void *ScanTextInfo(uint32_t slot) { return fn<const void *(__cdecl *)(uint32_t)>(0xB687C0)(slot); }
	inline const void *CharacterName(uint32_t slot) { return fn<const void *(__cdecl *)(uint32_t)>(0x47EAF0)(slot); }
	inline const void *MonsterName(uint32_t slot) { return fn<const void *(__cdecl *)(uint32_t)>(0x495100)(slot); }

	template<typename T> static inline T &F(uint8_t *p, uint32_t off) { return *(T *)(p + off); }
	template<typename T> static inline const T &F(const uint8_t *p, uint32_t off) { return *(const T *)(p + off); }

	static inline int32_t Div4096(int32_t x) { return x / 4096; }
	// x / 6 by the compiler's reciprocal 0x2AAAAAAB, then / 16 (sar 4)
	static inline int16_t Div96(int32_t x) { int32_t hi = (int32_t)(((int64_t)x * 0x2AAAAAAB) >> 32); hi >>= 4; return (int16_t)(hi + (int32_t)((uint32_t)hi >> 31)); }

	// ------------------------------------------------------------------
	// Target helpers
	// ------------------------------------------------------------------
	// sub_84D410: the entity class used for the info record (entity +4, with special cases)
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

	// sub_84EAF0: the target's info record (class into the camera state)
	static const uint8_t *InfoOf(const FxNode *n)
	{
		const int16_t c = EntityClass((int16_t)(uint16_t)n->slot);
		Cam().cls = c;
		return (const uint8_t *)(INFO_TABLE + (int32_t)c * 16);
	}

	// MAG_040_sub_84D320: the main entity, and which entities are scanned (the target and its parts)
	static void FindScannedEntities(int16_t slot)
	{
		uint8_t *e = Entity(slot);
		var<uint8_t *>(0x269CE30) = MainPartOf(e);
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
	// a green LINE_F2 between two screen points into the render list + 0x14 (sub_84E290)
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
		InsertPrimAltViewport(LinesOT(), p);
		PacketCursor() = (uint32_t)(p + 0x10);
	}

	// sub_84D6D0: `count` green lines along the circle of radius r around the node's screen point
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
			InsertPrimAltViewport(ot, p);
			p += 0x10;
			x0 = x1;
			y0 = y1;
		}
		PacketCursor() = (uint32_t)p;
	}

	// sub_84E1F0: four lines from the node's screen point to the corners of the scan window
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

	// sub_84DF00: the bracket's line list (+0x50), scaled by +0x64, at the node's screen point
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

	// sub_84D8B0 (render 0x572200) / sub_850CE0 (render 0x8DE9D0): a HUD prim model at the node's
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

	// sub_84E580: a HUD sprite (flipbook frame) at the node's view-space position, scale s
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

	// sub_84DB40: vertex blend out = a * (1 - t) + b * t (4.12) of the vertex blocks (+4 = count,
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

	// sub_84F0B0: the scan bars: 5 rows x 3 textured quads (0x80 x 0x40, tpage 0xBA, CLUT 0x3C94) in
	// grey c, scrolling down with the tick; render list + 0x4474
	static void ScanBars(int16_t tick, uint8_t c)
	{
		const uint32_t ot = var<uint32_t>(0x1D8E04C) + 0x4474;
		uint8_t *p = (uint8_t *)PacketCursor();
		int16_t y = (int16_t)((int32_t)((uint8_t)tick & 0x3F) - 0x40);
		for (int row = 5; row > 0; row--)
		{
			const int16_t y1 = (int16_t)(y + 0x40);
			int16_t x = 0;
			for (int col = 3; col > 0; col--)
			{
				const int16_t x1 = (int16_t)(x + 0x80);
				F<int16_t>(p, 8) = x;
				F<int16_t>(p, 0x18) = x;
				F<uint32_t>(p, 0) = 0x9000000;
				p[7] = 0x2C;
				F<uint16_t>(p, 0x16) = 0xBA;
				F<uint16_t>(p, 0xE) = 0x3C94;
				p[0xC] = 0x80;
				p[0xD] = 0;
				p[0x14] = 0;
				p[0x15] = 0;
				p[0x1C] = 0x80;
				p[0x1D] = 0x40;
				p[0x24] = 0;
				p[0x25] = 0x40;
				F<int16_t>(p, 0xA) = y;
				F<int16_t>(p, 0x10) = x1;
				F<int16_t>(p, 0x12) = y;
				F<int16_t>(p, 0x1A) = y1;
				F<int16_t>(p, 0x20) = x1;
				F<int16_t>(p, 0x22) = y1;
				p[6] = c;
				p[5] = c;
				p[4] = c;
				InsertPrimAltViewport(ot, p);
				p += 0x28;
				x = x1;
			}
			y = y1;
		}
		PacketCursor() = (uint32_t)p;
	}

	// ------------------------------------------------------------------
	// Camera (0x84E810) helpers
	// ------------------------------------------------------------------
	// sub_84EB40: one blend step (t += step, capped at 0x1000: 1 = arrived)
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

	// sub_84EC80 (math part): eye = look-at + RotY(yaw) * (0, h, dist)
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

	// sub_84EC80: the eye, then the battle camera words and the projection
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

	// sub_84E870: the blend's start (current camera) and end (the view of the target for `mode`)
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
#include "mag040_scan_held.h"
#endif

namespace ff8fx
{
namespace scan040
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
	// Camera (0x84E810) and camera return (0x850A30)
	// ------------------------------------------------------------------
	static uint32_t __cdecl CameraTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0:
			CameraSetup(n);
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note_camera(n);)
			break;
		case 1: // 0x84EB20: blend, done when arrived
			if (CameraBlend(Cam()) == 1)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			CameraApply();
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note_camera(n);)
			break;
		default: break; // 0x84ED40
		}
		return TaskEnd(&n->h);
	}

	static uint32_t __cdecl CamRestoreTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x850A90: the battle's saved camera back
			var<uint32_t>(0xB8B7F8) = var<uint32_t>(0xB8B808);
			var<uint32_t>(0xB8B7FC) = var<uint32_t>(0xB8B80C);
			var<uint32_t>(0xB8B7F4) = var<uint32_t>(0xB8B804);
			var<int16_t>(0x1D8E038) = var<int16_t>(0x1D977A0);
			var<uint32_t>(0xB8B7F0) = var<uint32_t>(0xB8B800);
			n->timer = 1;
			n->h.phase++;
			break;
		case 1: // 0x850AE0
			n->timer--;
			if (n->timer <= 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x850B00
		}
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// Screen-space line parts
	// ------------------------------------------------------------------
	// Corner bracket (0x84DDD0): at a fixed corner of the scan window
	static uint32_t __cdecl CornerTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x84DE30
			switch (n->index)
			{
			case 0: n->shape = 0x15D1718; n->h.pos[0] = (int16_t)-0x90; n->h.pos[1] = (int16_t)-0x60; break;
			case 1: n->shape = 0x15D1748; n->h.pos[0] = 0x90; n->h.pos[1] = (int16_t)-0x60; break;
			case 2: n->shape = 0x15D1778; n->h.pos[0] = (int16_t)-0x90; n->h.pos[1] = 0x60; break;
			case 3: n->shape = 0x15D17A8; n->h.pos[0] = 0x90; n->h.pos[1] = 0x60; break;
			default: break;
			}
			n->size = 0x1000;
			n->h.phase++;
			break;
		case 1: // 0x84DED0
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_BRACKET, n);)
			BracketLines(n);
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x84E000
		}
		return TaskEnd(&n->h);
	}

	// Sliding bracket (0x84E010): on the reticle, then sliding out to its corner while growing
	static uint32_t __cdecl BracketTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x84E080
			memcpy(n->h.pos, n->h.parent->pos, 8);
			switch (n->index)
			{
			case 0: n->shape = 0x15D17D8; n->h.pos[0] = 0; n->h.pos[1] = 0; n->dx = -1; n->dy = -1; break;
			case 1: n->shape = 0x15D1808; n->h.pos[0] = 0; n->h.pos[1] = 0; n->dx = 1; n->dy = -1; break;
			case 2: n->shape = 0x15D1838; n->h.pos[0] = 0; n->h.pos[1] = 0; n->dx = -1; n->dy = 1; break;
			case 3: n->shape = 0x15D1868; n->h.pos[0] = 0; n->h.pos[1] = 0; n->dx = 1; n->dy = 1; break;
			default: break;
			}
			n->size = 0x1000;
			n->timer = 0x1E;
			n->h.phase++;
			break;
		case 1: // 0x84E140: follows the reticle
			memcpy(n->h.pos, n->h.parent->pos, 8);
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_BRACKET_FOLLOW, n);)
			BracketLines(n);
			n->timer--;
			if (n->timer < 0)
			{
				n->timer = 4;
				n->h.phase++;
			}
			break;
		case 2: // 0x84E180: slides out and grows
			n->h.pos[0] = (int16_t)(n->h.pos[0] + n->dx);
			n->h.pos[1] = (int16_t)(n->h.pos[1] + n->dy);
			n->size = (int16_t)(n->size + 0x400);
			n->timer--;
			if (n->timer < 0) n->h.phase++;
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_BRACKET, n);)
			BracketLines(n);
			break;
		case 3: // 0x84E1B0
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_BRACKET, n);)
			BracketLines(n);
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x84E1E0
		}
		return TaskEnd(&n->h);
	}

	// Ring (0x84E3D0): a fan growing around the right of the window, a pointer line and a sprite
	static uint32_t __cdecl RingTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		int16_t out[2];
		switch (n->h.phase)
		{
		case 0: // 0x84E440
			n->timer = 0;
			n->h.phase++;
			break;
		case 1: // 0x84E460
			n->timer = (int16_t)(n->timer + 4);
			if (n->timer >= 0x18)
			{
				n->timer = 0x18;
				n->h.phase++;
			}
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_RING1, n);)
			Fan(&n->h, 0x30, 0x600, n->timer, out);
			break;
		case 2: // 0x84E4A0
		{
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_RING2, n);)
			Fan(&n->h, 0x30, 0x600, n->timer, out);
			const int16_t p[2] = { (int16_t)(out[0] + 0x18), (int16_t)(out[1] + 0x18) };
			Line(out, p);
			AddEffectTask(QSprites(), ORIG_Sprite, 0x58, n);
			n->h.phase++;
			break;
		}
		case 3: // 0x84E740
		{
			// 30 fps layer: see mag040_scan_held.inc
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
		default: break; // 0x84E7D0
		}
		return TaskEnd(&n->h);
	}

	// Reticle (0x84D5B0)
	static uint32_t __cdecl ReticleTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		int16_t out[2];
		switch (n->h.phase)
		{
		case 0: // 0x84D620
		{
			const int16_t *path = (const int16_t *)RETICLE_PATH + 2 * n->h.counter;
			n->h.pos[0] = path[0];
			n->h.pos[1] = path[1];
			for (int16_t i = 0; i < 4; i++) ((CtlNode *)AddEffectTask(QControls(), ORIG_Bracket, 0x78, n))->index = i;
			for (int16_t i = 0; i < 4; i++) ((CtlNode *)AddEffectTask(QControls(), ORIG_Corner, 0x78, n))->index = i;
			AddEffectTask(QModels(), ORIG_Morph, 0x6C, n);
			AddEffectTask(QModels(), ORIG_Pair, 0x6C, n);
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_RETICLE, n);)
			Fan(&n->h, 8, 0, 0x20, out);
			CornerLines(&n->h);
			n->h.phase++;
			break;
		}
		case 1: // 0x84E300: along the path
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
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_RETICLE, n);)
			Fan(&n->h, 8, 0, 0x20, out);
			CornerLines(&n->h);
			n->timer = 8;
			break;
		}
		case 2: // 0x84E360: the circle shrinks
			n->timer = (int16_t)(n->timer - 2);
			if (n->timer > 0)
			{
				// 30 fps layer: see mag040_scan_held.inc
				FX_HELD(held_note(REC_RETICLE_CLOSE, n);)
				Fan(&n->h, n->timer, 0, 0x20, out);
				CornerLines(&n->h);
			}
			else n->h.phase++;
			break;
		case 3: // 0x84E3A0
			AddEffectTask(QControls(), ORIG_Ring, 0x78, n);
			n->h.phase++;
			break;
		case 4: // 0x84E7E0
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x84E800
		}
		return TaskEnd(&n->h);
	}

	// Scan bars (0x84EFF0)
	static uint32_t __cdecl BarsTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: n->h.phase++; break; // 0x84F060
		case 1: // 0x84F070: fade in
			n->fade = (int16_t)(n->fade + 8);
			if (n->fade >= 0x80)
			{
				n->fade = 0x80;
				n->h.phase++;
			}
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_BARS, n);)
			ScanBars(n->h.counter, (uint8_t)n->fade);
			break;
		case 2: // 0x84F1C0: until confirmed
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_BARS, n);)
			ScanBars(n->h.counter, (uint8_t)n->fade);
			if (Confirmed() == 1) n->h.phase++;
			break;
		case 3: // 0x84F1F0: fade out
			n->fade = (int16_t)(n->fade - 8);
			if (n->fade <= 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->fade = 0;
				n->h.phase++;
			}
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note(REC_BARS, n);)
			ScanBars(n->h.counter, (uint8_t)n->fade);
			break;
		default: break; // 0x84F230
		}
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// Dimmer (0x84ED90) / restorer (0x8507F0): the other entities fade to the battle's shade colour
	// ------------------------------------------------------------------
	// the stage wobble words and the shade of every entity that is not scanned (level +0x1C)
	static void DimEntities(const CtlNode *n)
	{
		const int16_t v = n->h.pos[0];
		for (int k = 0; k < 4; k++) var<int16_t>(0x1D98992 + 0x2C * k) = v;
		const uint32_t b = var<uint32_t>(0xB8B7D8);
		for (int k = 0; k < 7; k++)
		{
			if (Scanned(k) != 0) continue;
			uint8_t *e = Entity(k);
			const int32_t s = v;
			e[0x28] = (uint8_t)((uint8_t)b - (uint8_t)Div4096((int32_t)(b & 0xFF) * s));
			e[0x29] = (uint8_t)((uint8_t)(b >> 8) - (uint8_t)Div4096((int32_t)((b >> 8) & 0xFF) * s));
			const uint8_t z = var<uint8_t>(0xB8B7DA);
			e[0x2A] = (uint8_t)(z - (uint8_t)Div4096((int32_t)z * s));
		}
	}

	static uint32_t __cdecl DimmerTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x84EDF0: save the entities, hide the others' shadows
			n->h.pos[0] = 0;
			for (int k = 0; k < 4; k++)
			{
				var<int16_t>(0x1D98992 + 0x2C * k) = 0;
				var<uint8_t>(0x1D989BA + 0x2C * k) = 0;
				var<uint8_t>(0x1D989B9 + 0x2C * k) = 0;
				var<uint8_t>(0x1D989B8 + 0x2C * k) = 0;
			}
			for (int k = 0; k < 7; k++)
			{
				uint8_t *e = Entity(k);
				const uint16_t w = F<uint16_t>(e, 0);
				SavedWord(k) = w;
				SavedColour(k) = F<uint32_t>(e, 0x28);
				if (Scanned(k) == 0) F<uint16_t>(e, 0) = (uint16_t)(w | 0x800);
			}
			n->h.phase++;
			break;
		case 1: // 0x84EE70
			n->h.pos[0] = (int16_t)(n->h.pos[0] + 0x100);
			if (n->h.pos[0] >= 0x1000)
			{
				n->h.pos[0] = 0x1000;
				n->h.phase++;
			}
			DimEntities(n);
			break;
		case 2: // 0x84EF40
			for (int k = 0; k < 4; k++) var<uint8_t>(0x1D98991 + 0x2C * k) &= 0xFD;
			for (int k = 0; k < 7; k++)
			{
				uint8_t *e = Entity(k);
				if (Scanned(k) == 1)
				{
					e[0x2A] = 0x80;
					e[0x29] = 0x80;
					e[0x28] = 0x80;
					e[0x2B] = 0;
					e[0] |= 0x20;
				}
				else e[0] |= 0x24;
			}
			*(uint8_t *)&n->h.flags |= 1;
			n->h.phase++;
			break;
		default: break; // 0x84EFA0
		}
		return TaskEnd(&n->h);
	}

	static uint32_t __cdecl RestorerTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x850850
			n->h.pos[0] = 0x1000;
			for (int k = 0; k < 4; k++)
			{
				const uint8_t a = var<uint8_t>(0x1D98991 + 0x2C * k);
				var<uint8_t>(0x1D98991 + 0x2C * k) = (uint8_t)(((a >> 1) & 2) | (a & 0xFD));
			}
			for (int k = 0; k < 7; k++)
			{
				uint8_t *e = Entity(k);
				if (Scanned(k) == 0) F<uint16_t>(e, 0) ^= (uint16_t)((uint8_t)(e[0] ^ (uint8_t)SavedWord(k)) & 0x24);
				else
				{
					F<uint16_t>(e, 0) ^= (uint16_t)((uint8_t)((uint8_t)SavedWord(k) ^ e[0]) & 0x20);
					F<uint32_t>(e, 0x28) = SavedColour(k);
				}
			}
			n->h.phase++;
			break;
		case 1: // 0x8508E0
			n->h.pos[0] = (int16_t)(n->h.pos[0] - 0x100);
			if (n->h.pos[0] <= 0)
			{
				n->h.pos[0] = 0;
				n->h.phase++;
			}
			DimEntities(n);
			break;
		case 2: // 0x8509B0
		{
			const uint32_t b = var<uint32_t>(0xB8B7D8);
			for (int k = 0; k < 7; k++)
			{
				uint8_t *e = Entity(k);
				if (Scanned(k) == 0)
				{
					F<uint16_t>(e, 0) ^= (uint16_t)(((uint16_t)SavedWord(k) ^ F<uint16_t>(e, 0)) & 0x800);
					e[0x28] = (uint8_t)b;
					e[0x29] = (uint8_t)(b >> 8);
					e[0x2A] = var<uint8_t>(0xB8B7DA);
				}
				else F<uint16_t>(e, 0) ^= (uint16_t)((uint8_t)((uint8_t)SavedWord(k) ^ e[0]) & 0x20);
			}
			*(uint8_t *)&n->h.flags |= 1;
			n->h.phase++;
			break;
		}
		default: break; // 0x850A20
		}
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// HUD models
	// ------------------------------------------------------------------
	// Model pair (0x84D810): two models along the tables 0x15D15E8 / 0x15D1630 / 0x15D15A0 /
	// 0x15D15C4 for ticks 3..17, then the second one pulses
	static void PairPhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0: // 0x84D990
			*(uint8_t *)&n->h.flags |= 4;
			n->h.pos[2] = 0x480;
			n->scale[2] = 0x1000;
			n->angle[2] = 0xA00;
			n->h.phase++;
			break;
		case 1: // 0x84D9C0
			if (n->h.counter >= 3) n->h.phase++;
			break;
		case 2: // 0x84D9D0
		{
			const int16_t k = n->h.counter;
			*(uint8_t *)&n->h.flags &= 0xFB;
			n->h.pos[0] = (int16_t)(((const int16_t *)0x15D15E8)[2 * k] - 0x3C);
			n->h.pos[1] = (int16_t)(((const int16_t *)0x15D15E8)[2 * k + 1] + 0x3C);
			n->scale[0] = ((const int16_t *)0x15D1630)[2 * k];
			n->scale[1] = ((const int16_t *)0x15D1630)[2 * k + 1];
			n->fade1 = ((const int16_t *)0x15D15A0)[k];
			n->fade2 = ((const int16_t *)0x15D15C4)[k];
			if (k >= 0x11) n->h.phase++;
			break;
		}
		case 3: // 0x84DA40
			n->wave = (int16_t)((n->wave + 0x200) & 0xFFF);
			n->fade2 = (int16_t)((int16_t)(ComputeSin(n->wave) + 0x1000) / 4);
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x84DA90
		}
	}

	static void PairDraw(ModelNode *n)
	{
		n->fade = n->fade1;
		n->model = 0x15CE93C;
		DrawHudModel(n, false);
		n->fade = n->fade2;
		n->model = 0x15CE9B8;
		DrawHudModel(n, false);
	}

	static uint32_t __cdecl PairTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		PairPhase(n);
		// 30 fps layer: see mag040_scan_held.inc
		FX_HELD(held_note(REC_PAIR, n);)
		PairDraw(n);
		return TaskEnd(&n->h);
	}

	// Morph model (0x84DAA0): spins in while shrinking, then morphs twice
	static void MorphPhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0: // 0x84DBE0
			n->h.pos[0] = 0;
			n->h.pos[1] = 0;
			n->h.pos[2] = 0x480;
			n->scale[2] = 0x3FF8;
			n->scale[1] = 0x3FF8;
			n->scale[0] = 0x3FF8;
			n->angle[0] = 0x5DC;
			n->angle[1] = 0x2A0;
			n->angle[2] = (int16_t)0xF808;
			n->model = 0x15CDDEC;
			n->morph_a = 0x15CFAB4;
			n->morph_b = 0x15CFC3C;
			n->fade = 0x1000;
			n->h.phase++;
			break;
		case 1: // 0x84DC40
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
		case 2: // 0x84DCD0
			n->angle[2] = (int16_t)((n->angle[2] + 0xAA) & 0xFFF);
			n->timer--;
			if (n->timer <= 0) n->h.phase++;
			break;
		case 3: // 0x84DD00
			n->blend = (int16_t)(n->blend + 0x200);
			n->angle[2] = (int16_t)((n->angle[2] + 0xAA) & 0xFFF);
			if (n->blend >= 0x1000)
			{
				n->blend = 0;
				n->model = 0x15CE1D4;
				n->morph_a = 0x15CFDC4;
				n->morph_b = 0x15D02CC;
				n->h.phase++;
			}
			break;
		case 4: // 0x84DD50
			n->blend = (int16_t)(n->blend + 0x400);
			if (n->blend >= 0x1000)
			{
				n->blend = 0x1000;
				n->h.phase++;
			}
			break;
		case 5: // 0x84DD80
			if ((int8_t)((uint8_t)n->h.counter & 7) < 4) n->angle[2] = (int16_t)((n->angle[2] + 0x2A) & 0xFFF);
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x84DDC0
		}
	}

	static uint32_t __cdecl MorphTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		MorphPhase(n);
		// 30 fps layer: see mag040_scan_held.inc
		FX_HELD(held_note(REC_MORPH, n);)
		MorphVertices((const uint8_t *)n->morph_a, (const uint8_t *)n->morph_b, (uint8_t *)n->model, n->blend);
		DrawHudModel(n, false);
		return TaskEnd(&n->h);
	}

	// Spinner (0x84F300): a spinning model fading in, until closing
	static void SpinnerPhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0: // 0x84F390
			n->scale[0] = 0x1000;
			n->scale[1] = 0x1000;
			n->scale[2] = 0x1000;
			n->fade = 0x1000;
			n->h.pos[0] = (int16_t)-0x80;
			n->h.pos[1] = (int16_t)-0xA0;
			n->h.pos[2] = 0x480;
			n->model = 0x15CF37C;
			n->h.phase++;
			break;
		case 1: // 0x84F3D0
			n->fade = (int16_t)(n->fade - 0x100);
			if (n->fade <= 0)
			{
				n->fade = 0;
				n->h.phase++;
			}
			break;
		case 2: // 0x84F400
			if (Closing() == 1)
			{
				n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x84F420
		}
	}

	static void SpinnerTurn(ModelNode *n)
	{
		n->angle[1] = (int16_t)((n->angle[1] + 0xC0) & 0xFFF);
		n->angle[0] = (int16_t)((n->angle[0] + 0x60) & 0xFFF);
	}

	static uint32_t __cdecl SpinnerTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		SpinnerPhase(n);
		SpinnerTurn(n);
		// 30 fps layer: see mag040_scan_held.inc
		FX_HELD(held_note(REC_SPINNER, n);)
		DrawHudModel(n, false);
		return TaskEnd(&n->h);
	}

	// Stat bar (0x84F520): one bar of the target's stats (HP, then the bytes +0xA5..+0xA9, +0xAB of
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
		case 0: // 0x84F5C0
		{
			const int16_t k = n->which;
			const FxNode *parent = n->h.parent;
			const int16_t *tbl = (const int16_t *)(n->table == 0 ? 0x15D1678 : 0x15D16C8) + 5 * k;
			*(uint8_t *)&n->h.flags |= 4;
			if ((uint32_t)(int32_t)k <= 7)
			{
				static const uint32_t MODELS[8] = { 0x15CF6FC, 0x15CF6FC, 0x15CF784, 0x15CF80C, 0x15CF894, 0x15CF91C, 0x15CF9A4, 0x15CFA2C };
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
		case 1: // 0x84F7C0: shown after its delay
			n->timer--;
			if (n->timer < 0)
			{
				*(uint8_t *)&n->h.flags &= 0xFB;
				n->h.phase++;
			}
			break;
		case 2: // 0x84F7E0: grows to its width
			n->scale[0] += 0x300;
			if (n->scale[0] > n->max) n->scale[0] = n->max;
			if (Closing() == 1)
			{
				n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x84F820
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
		// 30 fps layer: see mag040_scan_held.inc
		FX_HELD(held_note(REC_STATBAR, n);)
		DrawHudModel(n, false);
		return TaskEnd(&n->h);
	}

	// Board (0x84F430 / 0x850EB0): the stat board and its bars
	static uint32_t __cdecl BoardTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x84F4A0
			n->scale[0] = 0x1000;
			n->scale[1] = 0x1000;
			n->scale[2] = 0x1000;
			n->h.pos[0] = (int16_t)-0x40;
			n->h.pos[1] = (int16_t)-0x58;
			n->h.pos[2] = 0x240;
			n->model = 0x15CF5B4;
			n->h.phase++;
			break;
		case 1: // 0x84F4E0
			for (int16_t i = 0; i < 8; i++)
			{
				ModelNode *b = (ModelNode *)AddEffectTask(QModels(), ORIG_StatBar, 0x6C, n);
				b->which = i;
				b->table = 0;
			}
			n->h.phase++;
			break;
		case 2: // 0x84F830
			if (Closing() == 1)
			{
				n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x84F850
		}
		// 30 fps layer: see mag040_scan_held.inc
		FX_HELD(held_note(REC_STATIC, n);)
		DrawHudModel(n, false);
		return TaskEnd(&n->h);
	}

	static uint32_t __cdecl AltBoardTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x850F20
			n->scale[0] = 0x1000;
			n->scale[1] = 0x1000;
			n->scale[2] = 0x1000;
			n->h.pos[0] = (int16_t)-0x88;
			n->h.pos[1] = (int16_t)-0x3C;
			n->h.pos[2] = 0x240;
			n->model = 0x15CF614;
			n->h.phase++;
			break;
		case 1: // 0x850F60: bars 1..7 of table 1
			for (int16_t i = 1; i < 8; i++)
			{
				ModelNode *b = (ModelNode *)AddEffectTask(QModels(), ORIG_StatBar, 0x6C, n);
				b->which = i;
				b->table = 1;
			}
			n->h.phase++;
			break;
		case 2: // 0x850FA0
			if (Closing() == 1)
			{
				n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x850FC0
		}
		// 30 fps layer: see mag040_scan_held.inc
		FX_HELD(held_note(REC_STATIC, n);)
		DrawHudModel(n, false);
		return TaskEnd(&n->h);
	}

	// Backdrop (0x84F9E0): the window's backdrop model fading in, until confirmed
	static void BackdropPhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0: // 0x84FA50
			n->scale[0] = 0x1200;
			n->scale[1] = 0x1200;
			n->scale[2] = 0x1200;
			n->h.pos[0] = (int16_t)0xF080;
			n->h.pos[1] = 0xF00;
			n->h.pos[2] = 0x7FFF;
			n->model = 0x15CEA34;
			n->fade = 0x1000;
			n->h.phase++;
			break;
		case 1: // 0x84FA90
			n->fade = (int16_t)(n->fade - 0x200);
			if (n->fade <= 0)
			{
				n->fade = 0;
				n->h.phase++;
			}
			break;
		case 2: // 0x84FAC0
			if (Confirmed() == 1)
			{
				n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x84FAE0
		}
	}

	static uint32_t __cdecl BackdropTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		BackdropPhase(n);
		// 30 fps layer: see mag040_scan_held.inc
		FX_HELD(held_note(REC_BACKDROP, n);)
		DrawHudModel(n, false);
		return TaskEnd(&n->h);
	}

	// Alt spinner (0x850C70): a model turning in (y 0x400 -> 0), until closing (render 0x8DE9D0)
	static void AltSpinnerPhase(ModelNode *n)
	{
		switch (n->h.phase)
		{
		case 0: // 0x850DC0
			n->scale[0] = 0x1000;
			n->scale[1] = 0x1000;
			n->scale[2] = 0x1000;
			n->h.pos[0] = (int16_t)-0x200;
			n->h.pos[1] = 0;
			n->h.pos[2] = 0x6C0;
			n->model = 0x15CF674;
			n->angle[1] = 0x400;
			n->h.phase++;
			break;
		case 1: // 0x850E00
			n->angle[1] = (int16_t)(n->angle[1] - 0x80);
			if (n->angle[1] <= 0)
			{
				n->angle[1] = 0;
				n->h.phase++;
			}
			break;
		case 2: // 0x850E30
			if (Closing() == 1)
			{
				n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x850E50
		}
	}

	static uint32_t __cdecl AltSpinnerTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		AltSpinnerPhase(n);
		// 30 fps layer: see mag040_scan_held.inc
		FX_HELD(held_note(REC_ALTSPINNER, n);)
		DrawHudModel(n, true);
		return TaskEnd(&n->h);
	}

	// Sprite (0x84E510): an opening flipbook, then a looping one
	static uint32_t __cdecl SpriteTask(TaskNode *t)
	{
		SpriteNode *n = (SpriteNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x84E640
			n->h.pos[0] = 0x190;
			n->h.pos[1] = 0xC0;
			n->h.pos[2] = 0x900;
			n->seq = 0x15D07D4;
			n->last = 0x1F;
			n->h.phase++;
			break;
		case 1: // 0x84E670
			n->frame++;
			if (n->frame > n->last)
			{
				*(uint8_t *)&n->h.flags |= 4;
				n->frame = n->last;
				*(uint8_t *)&n->h.flags &= 0xFB;
				n->seq = 0x15D0AF8;
				n->frame = 0;
				n->last = 3;
				n->h.phase++;
			}
			break;
		case 2: // 0x84E6E0
			n->frame++;
			if (n->frame > n->last) n->frame = 0;
			if (HudActive() == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x84E730
		}
		// 30 fps layer: see mag040_scan_held.inc
		FX_HELD(held_note(REC_SPRITE, n);)
		DrawHudSprite(n, 0x400);
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// Text (0x84FAF0 / 0x850FD0)
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

	static uint32_t __cdecl TextTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		n->prev = n->pad;
		n->pad = RemapPadInput(ReadPadHeldRaw(0, 0)) & 0xFFFF;
		switch (n->h.phase)
		{
		case 0: TextOpen(n); break;       // 0x84FB80
		case 1: TextShowLine(n); break;   // 0x84FBB0
		case 2: // 0x84FC30: 0x40 closes, 0x20 or the timer shows the next line (looping)
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
			if (n->line >= n->lines) n->line = 0;
			n->h.phase--;
			break;
		case 3: TextClose(n); break;      // 0x84FC90
		default: break;                   // 0x84FCC0
		}
		return TaskEnd(&n->h);
	}

	static uint32_t __cdecl AltTextTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		n->prev = n->pad;
		n->pad = RemapPadInput(ReadPadHeldRaw(0, 0)) & 0xFFFF;
		switch (n->h.phase)
		{
		case 0: TextOpen(n); break;       // 0x851060
		case 1: TextShowLine(n); break;   // 0x851090
		case 2: // 0x851110: 0x40 closes, 0x20 or the timer shows the next line; past the last one closes
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
		case 3: TextClose(n); break;      // 0x851160
		default: break;                   // 0x851190
		}
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// Viewer (0x84F860): the target turned / zoomed by the pad
	// ------------------------------------------------------------------
	// sub_8501F0: pad bits (0x269AAA8), analog axes (0x269AAAC..) and the view inputs: turn about y
	// (0x269AAC8 / AACA), x (AACC / AACE), zoom (AAC4 / AAC6), offset x (AAD0 / AAD2), y (AAD4 / AAD6);
	// L / R modifiers (bits 0-1 / 2-3) move them to the offsets / zoom
	static void ReadViewInput()
	{
		static const uint32_t CLEAR[] = { 0x269AABA, 0x269AAB8, 0x269AAB6, 0x269AAB4, 0x269AAC2, 0x269AAC0, 0x269AABE, 0x269AABC,
			0x269AACE, 0x269AACC, 0x269AACA, 0x269AAC8, 0x269AAD6, 0x269AAD4, 0x269AAD2, 0x269AAD0, 0x269AAC6, 0x269AAC4 };
		for (uint32_t a : CLEAR) InW(a) = 0;
		const uint32_t pad = RemapPadInput(ReadPadHeldRaw(0, 0)) & 0xFFFF;
		var<uint32_t>(0x269AAA8) = pad;
		if (pad & 0xF000)
		{
			// D-pad: 8 per tick
			const uint8_t m = (uint8_t)pad;
			if (m & 3)
			{
				if (pad & 0x1000) InW(0x269AAD4) = 8;
				else if (pad & 0x4000) InW(0x269AAD6) = 8;
				if (pad & 0x8000) InW(0x269AAD0) = 8;
				else if (pad & 0x2000) InW(0x269AAD2) = 8;
			}
			else if (m & 0xC)
			{
				if (pad & 0x1000) InW(0x269AAC6) = 8;
				else if (pad & 0x4000) InW(0x269AAC4) = 8;
				if (pad & 0x8000) InW(0x269AAD0) = 8;
				else if (pad & 0x2000) InW(0x269AAD2) = 8;
			}
			else
			{
				if (pad & 0x1000) InW(0x269AACC) = 8;
				else if (pad & 0x4000) InW(0x269AACE) = 8;
				if (pad & 0x8000) InW(0x269AAC8) = 8;
				else if (pad & 0x2000) InW(0x269AACA) = 8;
			}
			return;
		}
		// analog: outside the dead zone 0x61..0x9E, (distance * 16) / 96
		InW(0x269AAAC) = (int16_t)ReadAnalog(0, 2, 0);
		if (InW(0x269AAAC) < 0)
		{
			InW(0x269AAB2) = 0x80;
			InW(0x269AAB0) = 0x80;
			InW(0x269AAAE) = 0x80;
			InW(0x269AAAC) = 0x80;
		}
		else
		{
			InW(0x269AAAE) = (int16_t)ReadAnalog(0, 3, 0);
			InW(0x269AAB0) = (int16_t)ReadAnalog(0, 0, 0);
			InW(0x269AAB2) = (int16_t)ReadAnalog(0, 1, 0);
			auto axis = [](uint32_t in, uint32_t lo, uint32_t hi)
			{
				const int16_t v = InW(in);
				if (v <= 0x60) InW(lo) = Div96((int32_t)(int16_t)(0x60 - v) << 4);
				else if (v >= 0x9F) InW(hi) = Div96((int32_t)(int16_t)(v - 0x9F) << 4);
			};
			axis(0x269AAAC, 0x269AAB4, 0x269AAB6);
			axis(0x269AAAE, 0x269AAB8, 0x269AABA);
			axis(0x269AAB0, 0x269AABC, 0x269AABE);
			axis(0x269AAB2, 0x269AAC0, 0x269AAC2);
		}
		const uint8_t m = var<uint8_t>(0x269AAA8);
		if (m & 3)
		{
			InW(0x269AAD0) = InW(0x269AAB4);
			InW(0x269AAD2) = InW(0x269AAB6);
			InW(0x269AAD4) = InW(0x269AAB8);
			InW(0x269AAD6) = InW(0x269AABA);
		}
		else if (m & 0xC)
		{
			InW(0x269AAC6) = InW(0x269AAB8);
			InW(0x269AAC4) = InW(0x269AABA);
			InW(0x269AAD0) = InW(0x269AAB4);
			InW(0x269AAD2) = InW(0x269AAB6);
		}
		else
		{
			InW(0x269AAC8) = InW(0x269AAB4);
			InW(0x269AACA) = InW(0x269AAB6);
			InW(0x269AACE) = InW(0x269AABA);
			InW(0x269AACC) = InW(0x269AAB8);
		}
	}

	// sub_850600: the view back to the start
	static void ResetView(CtlNode *n)
	{
		uint8_t *ent = MainEntity();
		CamState &c = Cam();
		F<uint32_t>(ent, 0xC) = c.angles[0];
		F<uint32_t>(ent, 0x10) = c.angles[1];
		n->moved = 0;
		n->rot[0] = 0;
		n->rot[1] = 0;
		n->rot[2] = 0;
		n->scale[0] = 0x1000;
		n->scale[1] = 0x1000;
		n->scale[2] = 0x1000;
	}

	// sub_84FCD0: saves the main entity's angles / position, gives it the viewer's zoom (+0x30 of
	// the entity points to the node's scale) and keeps its offset from its centre
	static void ViewerSetup(CtlNode *n)
	{
		uint8_t *ent = MainEntity();
		CamState &c = Cam();
		c.angles[0] = F<uint32_t>(ent, 0xC);
		c.angles[1] = F<uint32_t>(ent, 0x10);
		c.place[0] = F<uint32_t>(ent, 0x1C);
		c.place[1] = F<uint32_t>(ent, 0x20);
		F<int32_t *>(ent, 0x30) = n->scale;
		n->scale[0] = 0x1000;
		int16_t centre[4];
		n->scale[2] = 0x1000;
		n->scale[1] = 0x1000;
		GetEffectSpawnPosition(ent, 0xF1, 0, centre);
		const uint8_t *info = InfoOf(&n->h);
		ent = MainEntity();
		if (F<int16_t>(info, 8) == 0)
		{
			memcpy(&centre[0], ent + 0x1C, 4);
			memcpy(&centre[2], ent + 0x20, 4);
			centre[1] = 0;
		}
		n->offx = (int16_t)(F<int16_t>(ent, 0x1C) - centre[0]);
		n->offz = (int16_t)(F<int16_t>(ent, 0x20) - centre[2]);
	}

	// sub_84FD90: one tick of the view: pad input, turn / zoom the main entity about its centre, the
	// scanned parts follow
	static void ViewerTick(CtlNode *n)
	{
		const uint8_t *info = InfoOf(&n->h);
		ReadViewInput();
		const uint8_t m = var<uint8_t>(0x269AAA8);
		if ((m & 4) && (m & 1)) ResetView(n);
		uint8_t *ent;
		if (n->moved == 0)
		{
			n->idle = 0x4B;
			ent = MainEntity();
			if (info[0xD] == 1) F<int16_t>(ent, 0xE) = (int16_t)(F<int16_t>(ent, 0xE) + 0x40);
		}
		else
		{
			n->idle--;
			if (n->idle <= 0) ResetView(n);
			ent = MainEntity();
		}
		int16_t v = InW(0x269AAC8);
		if (v > 0)
		{
			n->moved = 1;
			n->idle = 0x4B;
			if (info[0xD] == 1) F<int16_t>(ent, 0xE) = (int16_t)(F<int16_t>(ent, 0xE) + (int16_t)(v << 4));
		}
		v = InW(0x269AACA);
		if (v > 0)
		{
			n->moved = 1;
			n->idle = 0x4B;
			if (info[0xD] == 1) F<int16_t>(ent, 0xE) = (int16_t)(F<int16_t>(ent, 0xE) + (int16_t)(v * -0x10));
		}
		v = InW(0x269AACC);
		if (v > 0)
		{
			n->moved = 1;
			n->idle = 0x4B;
			if (info[0xC] == 1) F<int16_t>(ent, 0xC) = (int16_t)(F<int16_t>(ent, 0xC) + (int16_t)shl32(-v, 4));
		}
		v = InW(0x269AACE);
		if (v > 0)
		{
			n->moved = 1;
			n->idle = 0x4B;
			if (info[0xC] == 1) F<int16_t>(ent, 0xC) = (int16_t)(F<int16_t>(ent, 0xC) + (int16_t)(v << 4));
		}
		v = InW(0x269AAC4);
		if (v > 0)
		{
			n->moved = 1;
			n->scale[0] += (int32_t)v << 4;
			n->idle = 0x4B;
			if (n->scale[0] > 0x2000) n->scale[0] = 0x2000;
			n->scale[2] = n->scale[0];
			n->scale[1] = n->scale[0];
		}
		else
		{
			v = InW(0x269AAC6);
			if (v > 0)
			{
				n->moved = 1;
				n->scale[0] += -((int32_t)v << 4);
				n->idle = 0x4B;
				if (n->scale[0] < 0x800) n->scale[0] = 0x800;
				n->scale[2] = n->scale[0];
				n->scale[1] = n->scale[0];
			}
		}
		v = InW(0x269AAD0);
		if (v > 0)
		{
			n->rot[0] = (int16_t)(n->rot[0] + (int16_t)shl32(-v, 3));
			n->moved = 1;
			n->idle = 0x4B;
		}
		v = InW(0x269AAD2);
		if (v > 0)
		{
			n->rot[0] = (int16_t)(n->rot[0] + (int16_t)(v << 3));
			n->moved = 1;
			n->idle = 0x4B;
		}
		v = InW(0x269AAD4);
		if (v > 0)
		{
			n->rot[1] = (int16_t)(n->rot[1] + (int16_t)shl32(-v, 3));
			n->moved = 1;
			n->idle = 0x4B;
		}
		v = InW(0x269AAD6);
		if (v > 0)
		{
			n->rot[1] = (int16_t)(n->rot[1] + (int16_t)(v << 3));
			n->moved = 1;
			n->idle = 0x4B;
		}
		F<uint16_t>(ent, 0xC) &= 0xFFF;
		F<uint16_t>(ent, 0xE) &= 0xFFF;
		F<uint16_t>(ent, 0x10) &= 0xFFF;
		CamState &c = Cam();
		F<uint32_t>(ent, 0x1C) = c.place[0];
		F<uint32_t>(ent, 0x20) = c.place[1];
		// about its centre: the entity's rotation * zoom turns (0, centre height, 0)
		Mat4x3 m1;
		ComposeZYXRotationMatrix((const int16_t *)(ent + 0xC), &m1);
		Scale3DMatrix(&m1, n->scale);
		int16_t up[4];
		int16_t out[4];
		up[1] = F<int16_t>(info, 6);
		up[0] = 0;
		up[2] = 0;
		MatrixMulVector(&m1, up, out);
		ent = MainEntity();
		F<int16_t>(ent, 0x1C) = (int16_t)(F<int16_t>(ent, 0x1C) + out[0]);
		F<int16_t>(ent, 0x1E) = (int16_t)(F<int16_t>(ent, 0x1E) + out[1]);
		F<int16_t>(ent, 0x20) = (int16_t)(F<int16_t>(ent, 0x20) + out[2]);
		F<int16_t>(ent, 0x1E) = (int16_t)(F<int16_t>(ent, 0x1E) - F<int16_t>(info, 6));
		if (n->moved == 1 && F<int16_t>(info, 8) == 0)
		{
			// moved (and no info offset flag): its centre on the look-at point, moved by the info offset
			F<int32_t>(ent, 0x54) = F<int16_t>(ent, 0x1C);
			F<int32_t>(ent, 0x58) = F<int16_t>(ent, 0x1E);
			F<int32_t>(ent, 0x5C) = F<int16_t>(ent, 0x20);
			int16_t p[4];
			GetEffectSpawnPosition(ent, 0xF1, 0, p);
			ent = MainEntity();
			F<int16_t>(ent, 0x1C) = (int16_t)(F<int16_t>(ent, 0x1C) + (int16_t)(var<int16_t>(0xB8B7F8) - p[0]));
			F<int16_t>(ent, 0x1E) = (int16_t)(F<int16_t>(ent, 0x1E) + (int16_t)(var<int16_t>(0xB8B7FA) - p[1]));
			F<int16_t>(ent, 0x20) = (int16_t)(F<int16_t>(ent, 0x20) + (int16_t)(var<int16_t>(0xB8B7FC) - p[2]));
			F<int32_t>(ent, 0x54) = F<int16_t>(ent, 0x1C);
			F<int32_t>(ent, 0x58) = F<int16_t>(ent, 0x1E);
			F<int32_t>(ent, 0x5C) = F<int16_t>(ent, 0x20);
			Mat4x3 m3;
			ComposeZYXRotationMatrix((const int16_t *)c.angles, &m3);
			int16_t w[4];
			w[0] = (int16_t)-F<int16_t>(info, 2);
			w[1] = (int16_t)-F<int16_t>(info, 0xA);
			w[2] = 0;
			MatrixMulVector(&m3, w, out);
			ent = MainEntity();
			F<int16_t>(ent, 0x1C) = (int16_t)(F<int16_t>(ent, 0x1C) + out[0]);
			F<int16_t>(ent, 0x1E) = (int16_t)(F<int16_t>(ent, 0x1E) + out[1]);
			F<int16_t>(ent, 0x20) = (int16_t)(F<int16_t>(ent, 0x20) + out[2]);
			F<int32_t>(ent, 0x54) = F<int16_t>(ent, 0x1C);
			F<int32_t>(ent, 0x58) = F<int16_t>(ent, 0x1E);
			F<int32_t>(ent, 0x5C) = F<int16_t>(ent, 0x20);
			ComposeZYXRotationMatrix((const int16_t *)c.angles, &m3);
			MatrixMulVector(&m3, n->rot, out);
		}
		else
		{
			Mat4x3 m3;
			ComposeZYXRotationMatrix((const int16_t *)c.angles, &m3);
			MatrixMulVector(&m3, n->rot, out);
		}
		ent = MainEntity();
		F<int16_t>(ent, 0x1C) = (int16_t)(F<int16_t>(ent, 0x1C) + out[0]);
		F<int16_t>(ent, 0x1E) = (int16_t)(F<int16_t>(ent, 0x1E) + out[1]);
		F<int16_t>(ent, 0x20) = (int16_t)(F<int16_t>(ent, 0x20) + out[2]);
		F<int32_t>(ent, 0x54) = F<int16_t>(ent, 0x1C);
		F<int32_t>(ent, 0x58) = F<int16_t>(ent, 0x1E);
		F<int32_t>(ent, 0x5C) = F<int16_t>(ent, 0x20);
		for (int k = 0; k < 7; k++)
		{
			if (Scanned(k) != 1) continue;
			uint8_t *e = Entity(k);
			F<uint32_t>(e, 0x1C) = F<uint32_t>(ent, 0x1C);
			F<uint32_t>(e, 0x20) = F<uint32_t>(ent, 0x20);
			memcpy(e + 0x40, ent + 0x40, 0x20);
		}
	}

	// sub_8506F0: the scanned entities' angles and positions back
	static void RestoreScanned()
	{
		const CamState &c = Cam();
		for (int k = 0; k < 7; k++)
		{
			if (Scanned(k) != 1) continue;
			uint8_t *e = Entity(k);
			F<uint32_t>(e, 0xC) = c.angles[0];
			F<uint32_t>(e, 0x10) = c.angles[1];
			F<uint32_t>(e, 0x1C) = c.place[0];
			F<uint32_t>(e, 0x20) = c.place[1];
			F<uint32_t>(e, 0x30) = 0;
		}
	}

	static uint32_t __cdecl ViewerTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x84F8D0: name and info text, the pager, the backdrop
		{
			const void *name = n->h.slot <= 2 ? CharacterName(n->h.slot) : MonsterName(n->h.slot);
			memcpy((void *)0x269AA78, name, 0x18);
			int16_t rect[4] = { 0xAA, 0x1E, 0x88, 0x1C };
			TextSetLayerText(2, (const void *)0x269AA78);
			TextLayer4A0700(2, 0);
			TextSetPosition(2, rect);
			TextLayer4A0640(2);
			const void *info = ScanTextInfo(n->h.slot);
			rect[0] = 0xAA; rect[1] = 0x34; rect[2] = 0x88; rect[3] = 0x80;
			TextSetLayerText(0, info);
			TextLayer4A0700(0, 0);
			TextSetPosition(0, rect);
			TextLayer4A0640(0);
			((CtlNode *)AddEffectTask(QControls(), ORIG_Text, 0x78, n))->timer = 0;
			AddEffectTask(QModels(), ORIG_Backdrop, 0x6C, n);
			ViewerSetup(n);
			n->h.phase++;
			break;
		}
		case 1: // 0x84FD70: until the text is closed
			ViewerTick(n);
			if (TextDone() == 1) n->h.phase++;
			break;
		case 2: // 0x850650: until confirmed
			ViewerTick(n);
			if ((uint8_t)RemapPadInput(ReadPadHeldRaw(0, 0)) & 0x40)
			{
				n->timer = 0x12;
				Confirmed() = 1;
				Closing() = 1;
				n->h.phase++;
			}
			break;
		case 3: // 0x850690
			ViewerTick(n);
			n->timer--;
			if (n->timer <= 0) n->h.phase++;
			break;
		case 4: // 0x8506B0
			RestoreScanned();
			TextLayer4A0680(2);
			TextSetLayerText(2, nullptr);
			TextLayer4A0680(0);
			TextSetLayerText(0, nullptr);
			*(uint8_t *)&n->h.flags |= 1;
			n->h.phase++;
			break;
		default: break; // 0x850740
		}
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// Scanners (0x84D4B0 / 0x850B40)
	// ------------------------------------------------------------------
	static void MarkTarget(CtlNode *n)
	{
		Entity(n->h.slot)[1] |= 0x78;
	}

	static uint32_t __cdecl ScannerTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x84D540
			MarkTarget(n);
			((CtlNode *)AddEffectTask(QControls(), ORIG_Camera, 0x78, n))->mode = 0;
			HudActive() = 1;
			AddEffectTask(QControls(), ORIG_Reticle, 0x78, n);
			n->timer = 0xF;
			n->h.phase++;
			break;
		case 1: // 0x84ED50
			n->timer--;
			if (n->timer < 0)
			{
				AddEffectTask(QControls(), ORIG_Dimmer, 0x78, n);
				n->timer = 0x10;
				n->h.phase++;
			}
			break;
		case 2: // 0x84EFB0
			n->timer--;
			if (n->timer < 0)
			{
				AddEffectTask(QControls(), ORIG_Bars, 0x78, n);
				n->timer = 0x20;
				n->h.phase++;
			}
			break;
		case 3: // 0x84F240
			n->timer--;
			if (n->timer < 0)
			{
				HudActive() = 0;
				n->h.phase++;
			}
			break;
		case 4: // 0x84F270
			((CtlNode *)AddEffectTask(QControls(), ORIG_Camera, 0x78, n))->mode = 1;
			n->timer = 8;
			n->h.phase++;
			break;
		case 5: // 0x84F2A0
			n->timer--;
			if (n->timer < 0)
			{
				AddEffectTask(QControls(), ORIG_Viewer, 0x78, n);
				AddEffectTask(QModels(), ORIG_Spinner, 0x6C, n);
				AddEffectTask(QModels(), ORIG_Board, 0x6C, n);
				n->h.phase++;
			}
			break;
		case 6: // 0x850750: the camera back and the entities restored
			if (n->h.children == 0)
			{
				uint8_t *e = Entity(n->h.slot);
				if ((int16_t)n->h.action == ((RootNode *)n->h.root)->last_action) AddEffectTask(QControls(), ORIG_CamRestore, 0x78, n);
				else if (Cam().dist <= (int16_t)0xE000) ((CtlNode *)AddEffectTask(QControls(), ORIG_Camera, 0x78, n))->mode = 3;
				AddEffectTask(QControls(), ORIG_Restorer, 0x78, n);
				F<uint16_t>(e, 0) &= 0x87FF;
				n->h.phase++;
			}
			break;
		case 7: // 0x850B10
			if (n->h.children == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x850B30
		}
		return TaskEnd(&n->h);
	}

	static uint32_t __cdecl AltScannerTask(TaskNode *t)
	{
		CtlNode *n = (CtlNode *)t;
		switch (n->h.phase)
		{
		case 0: // 0x850BC0
			MarkTarget(n);
			((CtlNode *)AddEffectTask(QControls(), ORIG_Camera, 0x78, n))->mode = 2;
			HudActive() = 1;
			AddEffectTask(QControls(), ORIG_Reticle, 0x78, n);
			n->timer = 0x2D;
			n->h.phase++;
			break;
		case 1: // 0x850C30
			n->timer--;
			if (n->timer < 0)
			{
				AddEffectTask(QModels(), ORIG_AltSpinner, 0x6C, n);
				n->timer = 8;
				n->h.phase++;
			}
			break;
		case 2: // 0x850E60
			n->timer--;
			if (n->timer < 0)
			{
				AddEffectTask(QControls(), ORIG_AltText, 0x78, n);
				AddEffectTask(QModels(), ORIG_AltBoard, 0x6C, n);
				n->h.phase++;
			}
			break;
		case 3: // 0x8511A0: the camera circles the target until the text is closed
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
			// 30 fps layer: see mag040_scan_held.inc
			FX_HELD(held_note_camera(n);)
			break;
		}
		case 4: // 0x851230
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
		case 5: // 0x8512C0
			if (n->h.children == 0)
			{
				*(uint8_t *)&n->h.flags |= 1;
				n->h.phase++;
			}
			break;
		default: break; // 0x8512E0
		}
		return TaskEnd(&n->h);
	}

	// ------------------------------------------------------------------
	// Emitter (0x84D230) and root (0x84D110)
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
		case 0: // 0x84D2C0: the scanned entities, the scanner
		{
			Confirmed() = 0;
			Closing() = 0;
			FindScannedEntities((int16_t)(uint16_t)e->h.slot);
			const uint8_t *rec = e->h.ctx->actions[0].targets + 24 * (int32_t)e->h.action;
			AddEffectTask(QControls(), (rec[2] & 2) ? ORIG_AltScanner : ORIG_Scanner, 0x78, e);
			e->h.phase++;
			break;
		}
		case 1: // 0x8512F0: the action result once the scanner is gone
			if (e->h.children == 0)
			{
				((RootNode *)e->h.root)->busy = 0;
				ApplyActionResultToTarget(e->h.ctx->actions[e->h.action].targets);
				*(uint8_t *)&e->h.flags |= 1;
				e->h.phase++;
			}
			break;
		default: break; // 0x851330
		}
		return TaskEnd(&e->h);
	}

	static void RootPhase(RootNode *r)
	{
		switch (r->h.phase)
		{
		case 0: // 0x84D1E0
			r->h.phase++;
			break;
		case 1: // 0x84D1F0
			r->busy = 1;
			AddEffectTask(QEmitters(), ORIG_Emitter, 0x58, r);
			CameraArmReturn();
			BattleMenuShow(0);
			r->h.phase++;
			break;
		case 2: // 0x851340
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
		case 3: // 0x851370
			if (r->alive == 0)
			{
				BattleMenuShow(1);
				*(uint8_t *)&r->h.flags |= 1;
				r->h.phase++;
			}
			break;
		default: break; // 0x8513A0
		}
	}

	static uint32_t __cdecl RootTask(TaskNode *t)
	{
		RootNode *r = (RootNode *)t;
		if (r->tick & 1) PacketCursor() = var<uint32_t>(0x269CFC4);
		else PacketCursor() = var<uint32_t>(0x269CFC0);
		// 30 fps layer: see mag040_scan_held.inc
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

	void register_mag040_scan()
	{
		using namespace scan040;
		register_port(ORIG_Root, (void *)RootTask, "040 Root", 40);
		register_port(ORIG_Emitter, (void *)EmitterTask, "040 Emitter", 40);
		register_port(ORIG_Scanner, (void *)ScannerTask, "040 Scanner", 40);
		register_port(ORIG_AltScanner, (void *)AltScannerTask, "040 AltScanner", 40);
		register_port(ORIG_Reticle, (void *)ReticleTask, "040 Reticle", 40);
		register_port(ORIG_Pair, (void *)PairTask, "040 Pair", 40);
		register_port(ORIG_Morph, (void *)MorphTask, "040 Morph", 40);
		register_port(ORIG_Corner, (void *)CornerTask, "040 Corner", 40);
		register_port(ORIG_Bracket, (void *)BracketTask, "040 Bracket", 40);
		register_port(ORIG_Ring, (void *)RingTask, "040 Ring", 40);
		register_port(ORIG_Sprite, (void *)SpriteTask, "040 Sprite", 40);
		register_port(ORIG_Camera, (void *)CameraTask, "040 Camera", 40);
		register_port(ORIG_Dimmer, (void *)DimmerTask, "040 Dimmer", 40);
		register_port(ORIG_Bars, (void *)BarsTask, "040 Bars", 40);
		register_port(ORIG_Spinner, (void *)SpinnerTask, "040 Spinner", 40);
		register_port(ORIG_Board, (void *)BoardTask, "040 Board", 40);
		register_port(ORIG_StatBar, (void *)StatBarTask, "040 StatBar", 40);
		register_port(ORIG_Viewer, (void *)ViewerTask, "040 Viewer", 40);
		register_port(ORIG_Backdrop, (void *)BackdropTask, "040 Backdrop", 40);
		register_port(ORIG_Text, (void *)TextTask, "040 Text", 40);
		register_port(ORIG_Restorer, (void *)RestorerTask, "040 Restorer", 40);
		register_port(ORIG_CamRestore, (void *)CamRestoreTask, "040 CamRestore", 40);
		register_port(ORIG_AltSpinner, (void *)AltSpinnerTask, "040 AltSpinner", 40);
		register_port(ORIG_AltBoard, (void *)AltBoardTask, "040 AltBoard", 40);
		register_port(ORIG_AltText, (void *)AltTextTask, "040 AltText", 40);
		// 30 fps layer: see mag040_scan_held.inc
		FX_HELD(register_mag040_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag040_scan_held.inc"
#endif
