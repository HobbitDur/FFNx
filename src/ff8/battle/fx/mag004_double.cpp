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

// Effect 4: Double (spell, MAG_004_DOUBLE_*).
//
// Structure (module code 0x8D5060..0x8D69E0: file loader MAG_004_DOUBLE_FL 0x8D5060 = the texture
// file named at 0x16417B8 (pointer 0x276FB00); setup MAG_004_DOUBLE_Init 0x8D5080 sets up the queues
// and the root node, starts camera animation 0x1641188, queues the TIM and takes the two packet
// arenas: texture buffer + 0 (0x277A344) / + 0x6000 (0x277A340)). Every node starts with the shared
// effect-library header (Effect_AddTaskAndInitFromCtx 0x8DC540, as in Cure); every task runs its
// current phase function (node +0x29) from a small table and ends when it is flagged done (+0x26
// bit0) and has no live children. Every task updates THEN draws; none tests the draw-only flags
// (battle_to_update_flags 0x201).
//   Root (0x8D5180) - packet arena by tick parity (cursor 0x276FB04), follows the caster's bones;
//     phases: 0 advance, 1 spawns the emitter of the current action (marks the root busy), 2 waits
//     while busy, then the next action (back to 1) or on, 3 waits until the five queues are empty.
//     Queues run: emitters, sequencers, sprites, models, flashes.
//   Emitter (0x8D52A0), one per action: target anchor and model bounds (effect library 0x8DC610 /
//     0x8DC870); phase 0 spawns the rune sequencer, phase 1 at tick 19 releases the root and spawns
//     the rising flash, phase 2 applies the action result. Sound at its first tick. Its word +0x58
//     is a handshake with the last rune: set to 1 by the rune, the emitter then starts a screen fade
//     (au_re_BdLinkTask_6) and the big rune and sets 2 (every rune and glow reacts in that tick),
//     then 0.
//   Sequencer (0x8D5B40) - one rune per tick at angles 0, 0x333, 0x666, 0x999, 0xCCC (the last
//     one flagged), then done.
//   Rune (0x8D5C80, prim model 0x163F014) - on a circle of radius 0x400 around the target, spinning;
//     fades in (0x1000 -> 0 in 4 ticks), then shrinks while fading out and ends hidden. While
//     spawning (its first ticks) a column (tick 3) and a light mote per tick (ticks 0..6).
//   Column (0x8D5E60, prim model 0x163EB94, flash model 0x163EDF4) - grows in x/z, then in y, then
//     settles; the flagged rune's column starts the handshake; flashes (the flash model drawn
//     over it) once when it settles and once on the emitter's signal, then fades out.
//   Glow (0x8D6200, sprite 0x163EB6C lying on the ground under the rune) - grey ramp up to 0x20,
//     a 0x40 pulse on the emitter's signal, then out.
//   Mote (0x8D6490, flipbook 0x163E930) - spirals in (radius -0x60, angle +0x60 per tick) and
//     brightens, until its radius is gone.
//   Big rune (0x8D53C0, prim model 0x163F224) - fades in then out; while it fades in, five sprites
//     (0x8D58A0) per tick for ticks 2..4 and a morphing ring (0x8D5750) at ticks 0 and 2.
//   Morphing ring (0x8D5750, prim model 0x163FFD0) - its vertices are blended between the vertex
//     sets 0x163FB00 and 0x163FD68 by its counter ramp (GTE GPF / GPL, written into the model in
//     the exe data) before each draw; fades while it opens.
//   Sprite (0x8D58A0, flipbook 0x163EA90) - random flipbook frame and X angle every tick, a grey
//     ramp 0x18 / 0x30 / 0x40 / 0x40 / 0x30 / 0x18, then done.
//   Flash (0x8D6790) - a screen-space textured quad (tpage 0x240/0x120, CLUT 0x140/0xF4, 0x28 x 0x10
//     texels) on the target's top anchor that jumps up and falls back, drawn straight into the
//     render list (no +0x44), for 15 ticks. Not spawned when the target record's +3 bit 2 is set.
// Module globals: 0x276FB00..0x277AEA8 (texture file pointer 0x276FB00, packet cursor 0x276FB04,
// pools / queues: models 0x276FB08 / 0x2774F68 (200 x 0x6C), root 0x2774F90 / 0x2775058 (2 x 0x64),
// sprites 0x2775068 / 0x277A1A8 (200 x 0x68), emitters 0x277A1D0 / 0x277A1C0 (4 x 0x5C), arenas
// 0x277A340 / 0x277A344, sequencers 0x277A358 / 0x277A348 (40 x 0x40), flashes 0x277AD58 /
// 0x277AE98 (4 x 0x50)); the morphing ring's vertex block in the exe data 0x163FFD8..0x1640238;
// the TransformCameraByShadowRotation scratch 0x21DFED0.
// Built from the parts library (fx_runes.h: the prim models, the turned flipbook frames, the vertex
// blend, the places on a circle, the column's flash, the glow's pulse, the flash quad; fx_parts.h:
// the camera-facing sprite, the ground flipbook). Parameters (DoubleParams, fx_params.h): the numbers
// above, defaults = the game's; the parts are tagged (FX_PART).

#include "mag_common.h"
#include "fx_params.h"
#include "fx_runes.h"

namespace ff8fx
{
namespace double004
{
	using namespace eng;
	using namespace magc;
	using namespace layouts;

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
		FxNode *emitter;       // +0x14 first node under the root on this node's line
		FxNode *parent;        // +0x18 (its +0x28 counts the live children)
		int16_t pos[4];        // +0x1C x, y, z (copied from the parent)
		int16_t counter;       // +0x24
		uint16_t flags;        // +0x26 bit0 done, bit1 spawning, bit2 hidden
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
		uint8_t bones[0x28];   // +0x30 caster bone anchor (Effect_UpdateTargetPosFromBones)
		int16_t last_action;   // +0x58
		int16_t target_count;  // +0x5A
		int16_t tick;          // +0x5C arena parity
		int16_t alive;         // +0x5E sum of the queue results
		uint8_t pad60[3];
		uint8_t busy;          // +0x63 an action is running
	};
	struct EmitterNode // pool of 4 nodes of 0x5C bytes
	{
		FxNode h;
		uint8_t anchor[0x18];  // +0x30 target anchors: +0x30 base, +0x38 middle, +0x40 top
		int16_t bmin[4];       // +0x48 model bounds
		int16_t bmax[4];       // +0x50
		int16_t signal;        // +0x58 handshake with the last rune (1 -> 2 -> 0)
		int16_t pad5A;
	};
	struct SeqNode // pool of 40 nodes of 0x40 bytes
	{
		FxNode h;
		uint8_t pad30[0x10];
	};
	struct ModelNode // rune / column / big rune / morphing ring: pool of 200 nodes of 0x6C bytes
	{
		FxNode h;
		int32_t scale[3];      // +0x30
		uint32_t pad3C;
		uint8_t rgb[4];        // +0x40 prim-model colour (dword)
		int16_t angle[3];      // +0x44 rotation (y spins)
		int16_t pad4A;
		uint32_t model;        // +0x4C prim model
		int16_t fade;          // +0x50 prim-model fade
		int16_t pad52;
		int16_t base[4];       // +0x54 rune / column: circle centre
		int16_t orbit;         // +0x5C rune / column: angle on the circle
		int16_t flagged;       // +0x5E the last rune (column: copied)
		int16_t flash;         // +0x60 column: flash state (1 / 3 start, 2 / 4 fade)
		int16_t radius;        // +0x62 column: circle radius (4.12 of 0x400)
		int16_t flash_on;      // +0x64 column: flash model drawn
		int16_t orbit_step;    // +0x66 column: angle step (never written: 0; the column's orbit speed instead)
		int16_t morph;         // +0x68 morphing ring: blend
		int16_t arg;           // +0x6A rune spawn argument (0)
	};
	struct SpriteNode // sprite / glow / mote: pool of 200 nodes of 0x68 bytes
	{
		FxNode h;
		int32_t scale[3];      // +0x30
		uint32_t pad3C;
		int16_t ax;            // +0x40 sprite: X angle
		int16_t ay;            // +0x42 sprite: Y angle; mote: spiral angle
		uint32_t pad44;
		uint8_t rgb[4];        // +0x48 colour (dword)
		uint32_t seq;          // +0x4C flipbook
		int16_t frame;         // +0x50
		int16_t last;          // +0x52 last frame
		int16_t shade;         // +0x54 TransformCameraByShadowRotation offset (0)
		int16_t pad56;
		int16_t base[4];       // +0x58 glow / mote: centre
		int16_t radius;        // +0x60 mote: spiral radius
		int16_t angle;         // +0x62 glow: angle on the circle
		int16_t pulse;         // +0x64 glow: pulse state (1 / 3 start, 2 / 4 fade)
		int16_t pad66;
	};
	struct FlashNode // pool of 4 nodes of 0x50 bytes
	{
		FxNode h;
		int16_t pad30;
		int16_t vel;           // +0x32 y velocity
		uint32_t pad34;
		int16_t target[4];     // +0x38 rest position (the target's top anchor)
		int16_t tx, ty;        // +0x40 texture page (VRAM x, y)
		int16_t cx, cy;        // +0x44 CLUT (VRAM x, y)
		int16_t u, v;          // +0x48 texel origin
		int16_t tw, th;        // +0x4C size
	};
#pragma pack(pop)
	static_assert(sizeof(FxNode) == 0x30 && sizeof(RootNode) == 0x64 && sizeof(EmitterNode) == 0x5C, "Double nodes");
	static_assert(sizeof(SeqNode) == 0x40 && sizeof(ModelNode) == 0x6C && sizeof(SpriteNode) == 0x68, "Double nodes");
	static_assert(sizeof(FlashNode) == 0x50, "Double nodes");

	// ------------------------------------------------------------------
	// Module globals
	// ------------------------------------------------------------------
	inline uint32_t &PacketCursor() { return var<uint32_t>(0x276FB04); }
	inline TaskQueue *QEmitters() { return (TaskQueue *)0x277A1C0; }  // 4 x 0x5C
	inline TaskQueue *QSequencers() { return (TaskQueue *)0x277A348; }// 40 x 0x40
	inline TaskQueue *QSprites() { return (TaskQueue *)0x277A1A8; }   // 200 x 0x68
	inline TaskQueue *QModels() { return (TaskQueue *)0x2774F68; }    // 200 x 0x6C
	inline TaskQueue *QFlashes() { return (TaskQueue *)0x277AE98; }   // 4 x 0x50

	static const uint32_t ORIG_Root = 0x8D5180;
	static const uint32_t ORIG_Emitter = 0x8D52A0;
	static const uint32_t ORIG_BigRune = 0x8D53C0;
	static const uint32_t ORIG_Morph = 0x8D5750;
	static const uint32_t ORIG_Sprite = 0x8D58A0;
	static const uint32_t ORIG_Sequencer = 0x8D5B40;
	static const uint32_t ORIG_Rune = 0x8D5C80;
	static const uint32_t ORIG_Column = 0x8D5E60;
	static const uint32_t ORIG_Glow = 0x8D6200;
	static const uint32_t ORIG_Mote = 0x8D6490;
	static const uint32_t ORIG_Flash = 0x8D6790;

	static const void *const SOUND_Double = (const void *)0x163E92C;
	static const uint32_t MODEL_BigRune = 0x163F224, MODEL_Rune = 0x163F014, MODEL_Column = 0x163EB94;
	static const uint32_t MODEL_ColumnFlash = 0x163EDF4, MODEL_Morph = 0x163FFD0;
	static const uint32_t MORPH_A = 0x163FB00, MORPH_B = 0x163FD68;
	static const uint32_t SEQ_Sprite = 0x163EA90, SEQ_Glow = 0x163EB6C, SEQ_Mote = 0x163E930;
	static const uint32_t ARENA_A = 0x277A344, ARENA_B = 0x277A340;

	// ------------------------------------------------------------------
	// Engine / effect-library functions (original addresses)
	// ------------------------------------------------------------------
	// Effect_AddTaskAndInitFromCtx (0x8DC540): new node of `size` bytes in `q`, zeroed, header
	// inherited from `parent` (the parent's child count goes up)
	inline FxNode *AddEffectTask(TaskQueue *q, uint32_t task, int32_t size, void *parent) { return fn<FxNode *(__cdecl *)(TaskQueue *, uint32_t, int32_t, void *)>(0x8DC540)(q, task, size, parent); }
	inline void ReleaseLinkedTask(void *n) { fn<void (__cdecl *)(void *)>(0x8DC530)(n); }
	inline void EmitterUpdatePos(void *n) { fn<void (__cdecl *)(void *)>(0x8DC610)(n); }                  // MAG_001_CURE_Emitter_UpdatePos
	inline void ComputeModelBounds(void *n) { fn<void (__cdecl *)(void *)>(0x8DC870)(n); }                // MAG_001_CURE_Emitter_ComputeModelBounds
	inline void UpdateTargetPosFromBones(void *n) { fn<void (__cdecl *)(void *)>(0x8DC740)(n); }
	inline void CopyAnchorXZFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC6E0)(n, out); } // emitter +0x30
	inline void CopyAnchorTopFromSource(void *n, int16_t *out) { fn<void (__cdecl *)(void *, int16_t *)>(0x8DC720)(n, out); } // emitter +0x40

	// ------------------------------------------------------------------
	// Parameters (fx_params.h): the game's values by default
	// ------------------------------------------------------------------
	struct DoubleParams
	{
		struct Emitter
		{
			uint32_t sound;         // sound block (FF8_EN.exe data): BdPlaySE reads its world sound id
			int16_t sound_tick;     // the sound plays (emitter tick)...
			int16_t sound_pan;      // ...at this pan
			int16_t release_tick;   // from this emitter tick the next action may start; the flash then
			struct { int16_t a, b, c, d; } fade;   // the screen fade when the last rune's column has settled (ScreenFadeTask's four values)
			int16_t target_bit;     // no flash when the target record's +3 has these bits
		} emitter;
		struct Runes
		{
			int16_t last;           // runes 0 .. last, one a tick (the last one flagged: its column starts the big rune)
			int16_t angle_step;     // rune k at angle k x this on the circle
			int16_t lift;           // the circle this high over the target's feet
			int16_t radius;         // on a circle of 0x400 x this / 4096 around the target
			int16_t fade_in;        // its fade goes down by this a tick (from 4096: hidden), spawning...
			int16_t fade_out;       // ...then up by this a tick back to 4096 while it shrinks...
			int16_t shrink;         // ...by this a tick...
			int16_t end_scale;      // ...to this size, then it is hidden and done
			int16_t spin;           // it turns this much a tick
		} runes;
		struct Columns
		{
			int16_t tick;           // a rune's column at this rune tick
			int16_t fade_in;        // its fade goes down by this a tick, its width up by...
			int16_t grow;           // ...this, until it is seen (width then...
			int16_t width;          // ...this)
			int16_t rise;           // then its height up by this a tick to...
			int16_t peak;           // ...this...
			int16_t settle;         // ...then down by this a tick (its width up) back to 4096
			int16_t fade_out;       // at the end its fade up by this a tick, then done
			struct { int16_t first, second, step_first, step_second, grey; } flash;   // the flash model over it
			int16_t spin;           // it turns this much a tick...
			int16_t orbit_speed;    // ...and goes round the circle this much a tick
		} columns;
		struct Motes
		{
			int16_t from, last;     // a mote a rune tick over these (while the rune spawns)
			struct { int16_t base, mask; } radius;   // its spiral's radius: base + (rand & mask)
			int16_t spin;           // its angle goes round this much a tick...
			int16_t brighten;       // ...its grey up by this a tick while below...
			int16_t brightest;      // ...this
			int16_t shrink;         // its radius down by this a tick (done when it is gone)
			int16_t frames;         // its flipbook's last frame (it loops)
		} motes;
		struct Glows
		{
			int16_t up;             // a glow under each rune: its grey up by this a tick to...
			int16_t level;          // ...this
			struct { int16_t peak, step; } pulse;   // when the big rune starts: this grey, then down by step a tick back to the level
			int16_t down;           // then down by this a tick to 0, then hidden and done
		} glows;
		struct BigRune
		{
			int16_t lift;           // this high over the target's feet
			int16_t turn;           // turned this much about y
			int16_t fade;           // its fade from this down by...
			int16_t fade_in;        // ...this a tick to 0 (spawning: its sprites and rings)...
			int16_t fade_out;       // ...then up by this a tick to 4096, then hidden and done
			int16_t grey;           // its grey while it fades in (then black)
		} big;
		struct Sprites
		{
			int16_t from, last;     // over these big rune ticks...
			int16_t count;          // ...this many a tick, sprite k...
			int16_t angle_step;     // ...turned k x this about y...
			struct { int16_t mod, base; } x;   // ...at x = rand % mod + base...
			int16_t back;           // ...z = -this from the big rune
			struct { int16_t a, b, c; } grey;   // their grey: a, b, c, (held), b, a, then done
			int16_t hop, mask;      // their frame: (frame + 1 + rand % hop) & mask a tick
		} sprites;
		struct Morph
		{
			int16_t first, second;  // a morphing ring at these big rune ticks
			int16_t step;           // its blend and fade up by this a tick (done at 4096)
		} morph;
		struct Flash
		{
			int16_t jump;           // the flash's speed up (y down: negative)...
			int16_t gravity;        // ...slowing by this a tick until it is back at the target's top
			int16_t ticks;          // it ends at this tick
			struct { int16_t tx, ty, cx, cy, u, v, w, h; } tex;   // the texture page, CLUT and texels of its quad
		} flash;
	};
	static const DoubleParams DEFAULTS = {
		{ (uint32_t)SOUND_Double, 0, 0x80, 0x13, { 0, 1, 2, 0x80 }, 4 },
		{ 4, 0x333, 0x280, 0x1000, 0x200, 0x200, 0x200, 0x10, 0x140 },
		{ 3, 0x400, 0x200, 0x800, 0x400, 0x1800, 0x400, 0x200, { 0x400, 0x800, 0x100, 0x200, 0xFF }, 0x140, 0 },
		{ 0, 6, { 0x200, 0x1FF }, 0x60, 0x20, 0x80, 0x60, 0xC },
		{ 4, 0x20, { 0x40, 4 }, 4 },
		{ 0x280, 0x199, 0x800, 0x200, 0x300, 0xFF },
		{ 2, 4, 5, 0x333, { 0x300, -0x180 }, 0x2C0, { 0x18, 0x30, 0x40 }, 7, 7 },
		{ 0, 2, 0x200 },
		{ -0x90, 0x28, 0xF, { 0x240, 0x120, 0x140, 0xF4, 0, 0x20, 0x28, 0x10 } },
	};
	static DoubleParams g_params = DEFAULTS;
	static inline const DoubleParams &P() { return g_params; }
	enum PartId { PART_RUNES = 1, PART_COLUMNS = 2, PART_MOTES = 3, PART_GLOWS = 4, PART_BIG = 5, PART_SPRITES = 6, PART_MORPH = 7, PART_FLASH = 8 };

	// ------------------------------------------------------------------
	// Drawing helpers
	// ------------------------------------------------------------------
	static void DrawModel(const ModelNode *n)
	{
		EffectModelDraw(n->h.pos, (n->h.flags & 4) != 0, n->angle, n->scale, *(const uint32_t *)n->rgb, n->fade, n->model, PacketCursor());
	}

	static void SetGrey(uint8_t *rgb, uint8_t g) { rgb[0] = rgb[1] = rgb[2] = g; }
	static void AddGrey(uint8_t *rgb, uint8_t d)
	{
		rgb[1] = (uint8_t)(rgb[1] + d);
		rgb[0] = (uint8_t)(rgb[0] + d);
		rgb[2] = (uint8_t)(rgb[2] + d);
	}
	static void SetScale(int32_t *scale, int32_t s) { scale[0] = scale[1] = scale[2] = s; }

	// ------------------------------------------------------------------
	// Big rune (0x8D53C0) and what it spawns
	// ------------------------------------------------------------------
	// sub_8D5570: while fading in, `count` sprites a tick around the rune, sprite k turned k x step
	// (mode 1 of the original, never used, is not kept)
	static void SpawnSprites(ModelNode *n)
	{
		const DoubleParams::Sprites &sp = P().sprites;
		if (n->h.counter < sp.from || n->h.counter > sp.last) return;
		for (int32_t i = 0; i < sp.count; i++)
		{
			const int32_t k = ((int32_t)n->h.counter * sp.count + i) % sp.count;
			SpriteNode *p = (SpriteNode *)AddEffectTask(QSprites(), ORIG_Sprite, 0x68, n);
			p->seq = SEQ_Sprite;
			p->last = 7;
			SetScale(p->scale, 0x1000);
			int16_t v[4];
			const int32_t r = CrtRand() % sp.x.mod;
			v[1] = 0;
			v[2] = (int16_t)-sp.back;
			v[0] = (int16_t)(r + sp.x.base);
			p->ay = (int16_t)(k * sp.angle_step);
			TurnedOffset(p->h.pos, p->ay, v);
		}
	}

	// sub_8D5500: a morphing ring at two ticks (one at each: two when they are the same tick)
	static void SpawnMorphRing(ModelNode *n)
	{
		const DoubleParams &pp = P();
		ModelNode *m = (ModelNode *)AddEffectTask(QModels(), ORIG_Morph, 0x6C, n);
		CopyAnchorXZFromSource(m, m->h.pos);
		m->h.pos[1] = (int16_t)(m->h.pos[1] - pp.big.lift);
		m->angle[1] = pp.big.turn;
		m->model = MODEL_Morph;
		m->fade = 0;
		SetScale(m->scale, 0x1000);
		SetGrey(m->rgb, 0);
	}
	static void SpawnMorph(ModelNode *n)
	{
		if (n->h.counter == P().morph.first) SpawnMorphRing(n);
		if (n->h.counter == P().morph.second) SpawnMorphRing(n);
	}

	// MAG_004_sub_8D5360 (the emitter's signal): the big rune on the target
	static void SpawnBigRune(EmitterNode *e)
	{
		const DoubleParams::BigRune &b = P().big;
		ModelNode *n = (ModelNode *)AddEffectTask(QModels(), ORIG_BigRune, 0x6C, e);
		CopyAnchorXZFromSource(n, n->h.pos);
		n->h.pos[1] = (int16_t)(n->h.pos[1] - b.lift);
		SetScale(n->scale, 0x1000);
		n->angle[1] = b.turn;
		n->model = MODEL_BigRune;
		n->fade = b.fade;
		SetGrey(n->rgb, (uint8_t)b.grey);
	}

	// phases 0x8D56D0 (fade in, spawning), 0x8D5700 (fade out, then hidden and done), 0x8D5740
	static void BigRunePhase(ModelNode *n)
	{
		const DoubleParams::BigRune &b = P().big;
		switch (n->h.phase)
		{
		case 0:
			n->fade = (int16_t)(n->fade - b.fade_in);
			*(uint8_t *)&n->h.flags |= 2;
			if (n->fade <= 0)
			{
				n->fade = 0;
				SetGrey(n->rgb, 0);
				n->h.phase++;
			}
			break;
		case 1:
			n->fade = (int16_t)(n->fade + b.fade_out);
			if (n->fade >= 0x1000)
			{
				n->fade = 0x1000;
				n->h.flags = (uint16_t)((n->h.flags & 0xFFFD) | 5);
				n->h.phase++;
			}
			break;
		default: break;
		}
	}
}
}

#ifdef FF8_FX_HELD
#include "mag004_double_held.h"
#endif

namespace ff8fx
{
namespace double004
{
	static uint32_t __cdecl BigRuneTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		BigRunePhase(n);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_BIG, n);)
		FX_PART(PART_BIG);
		DrawModel(n);
		FX_PART(0);
		if (n->h.flags & 2)
		{
			SpawnSprites(n);
			SpawnMorph(n);
		}
		n->h.counter++;
		if ((n->h.flags & 1) && n->h.children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Morphing ring (0x8D5750)
	// ------------------------------------------------------------------
	// phases 0x8D5860 (blend and fade up a tick, done at 0x1000), 0x8D5890
	static void MorphPhase(ModelNode *n)
	{
		if (n->h.phase != 0) return;
		n->morph = (int16_t)(n->morph + P().morph.step);
		n->fade = (int16_t)(n->fade + P().morph.step);
		if (n->fade >= 0x1000)
		{
			*(uint8_t *)&n->h.flags |= 1;
			n->fade = 0x1000;
			n->h.phase++;
		}
	}

	static uint32_t __cdecl MorphTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		MorphPhase(n);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_MORPH, n);)
		MorphVertices((const uint8_t *)MORPH_A, (const uint8_t *)MORPH_B, (uint8_t *)MODEL_Morph, n->morph);
		FX_PART(PART_MORPH);
		DrawModel(n);
		FX_PART(0);
		n->h.counter++;
		if ((n->h.flags & 1) && n->h.children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Sprite (0x8D58A0)
	// ------------------------------------------------------------------
	// phases 0x8D5A30.. 0x8D5AE0: grey a, b, c, (hold), b, a, then 0 and done
	static void SpritePhase(SpriteNode *p)
	{
		const DoubleParams::Sprites &sp = P().sprites;
		int16_t g;
		switch (p->h.phase)
		{
		case 0: g = sp.grey.a; break;
		case 1: g = sp.grey.b; break;
		case 2: g = sp.grey.c; break;
		case 3: p->h.phase++; return;
		case 4: g = sp.grey.b; break;
		case 5: g = sp.grey.a; break;
		case 6:
			*(uint8_t *)&p->h.flags |= 1;
			SetGrey(p->rgb, 0);
			return;
		default: return;
		}
		SetGrey(p->rgb, (uint8_t)g);
		p->h.phase++;
	}

	static uint32_t __cdecl SpriteTask(TaskNode *t)
	{
		SpriteNode *p = (SpriteNode *)t;
		const DoubleParams::Sprites &sp = P().sprites;
		SpritePhase(p);
		p->ax = (int16_t)(CrtRand() & 0xFFF);
		const int32_t r = CrtRand() % sp.hop;
		p->frame = (int16_t)((uint8_t)(r + (uint8_t)p->frame + 1) & sp.mask);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_SPRITE, p);)
		FX_PART(PART_SPRITES);
		EffectSpriteTurned(p->h.pos, (p->h.flags & 4) != 0, p->ax, p->ay, p->scale, p->seq, p->frame, *(const uint32_t *)p->rgb, PacketCursor());
		FX_PART(0);
		p->h.counter++;
		if ((p->h.flags & 1) && p->h.children == 0)
		{
			ReleaseLinkedTask(p);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Column (0x8D5E60)
	// ------------------------------------------------------------------
	// phases 0x8D6000 (fade in, x / z grow), 0x8D6040 / 0x8D6060 (y grows to the peak), 0x8D6090
	// (settles at 0x1000: flash, or the handshake for the flagged rune's column), 0x8D6100 (the
	// emitter's signal: second flash), 0x8D6120 (fade out, done), 0x8D6150
	static void ColumnPhase(ModelNode *n)
	{
		const DoubleParams::Columns &c = P().columns;
		switch (n->h.phase)
		{
		case 0:
			n->fade = (int16_t)(n->fade - c.fade_in);
			n->scale[0] += c.grow;
			n->scale[2] += c.grow;
			if (n->fade <= 0)
			{
				n->fade = 0;
				n->scale[0] = c.width;
				n->scale[2] = c.width;
				n->h.phase++;
			}
			break;
		case 1:
			n->scale[1] += c.rise;
			if (n->scale[1] >= 0x1000) n->h.phase++;
			break;
		case 2:
			n->scale[1] += c.rise;
			if (n->scale[1] >= c.peak)
			{
				n->scale[1] = c.peak;
				n->h.phase++;
			}
			break;
		case 3:
			n->scale[0] += c.settle;
			n->scale[1] -= c.settle;
			n->scale[2] += c.settle;
			if (n->scale[1] <= 0x1000)
			{
				SetScale(n->scale, 0x1000);
				if (n->flagged == 0) n->flash = 1;
				else ((EmitterNode *)n->h.emitter)->signal = 1;
				n->h.phase++;
			}
			break;
		case 4:
			if (((EmitterNode *)n->h.emitter)->signal == 2)
			{
				n->flash = 3;
				n->h.phase++;
			}
			break;
		case 5:
			if (n->flash == 0)
			{
				n->fade = (int16_t)(n->fade + c.fade_out);
				if (n->fade >= 0x1000)
				{
					*(uint8_t *)&n->h.flags |= 1;
					n->fade = 0x1000;
					n->h.phase++;
				}
			}
			break;
		default: break;
		}
	}

	static uint32_t __cdecl ColumnTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		const DoubleParams &pp = P();
		const DoubleParams::Columns &c = pp.columns;
		ColumnPhase(n);
		const FadePulse pulse = { c.flash.first, c.flash.second, c.flash.step_first, c.flash.step_second, c.flash.grey };
		FadePulseStep(&n->flash, &n->flash_on, &n->fade, n->rgb, pulse);
		// spin, and the place on the circle (radius * (sin, cos) / 0x4000 of the orbit angle)
		const int16_t orbit = (int16_t)((c.orbit_speed + n->orbit) & 0xFFF);
		n->angle[1] = (int16_t)((n->angle[1] + c.spin) & 0xFFF);
		n->orbit = orbit;
		CirclePlace(n->h.pos, n->base, n->orbit, n->radius, 0x4000);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_COLUMN, n);)
		// the flash model over it while the flash is on, then the column
		FX_PART(PART_COLUMNS);
		if (n->flash_on == 1)
			EffectModelDraw(n->h.pos, (n->h.flags & 4) != 0, n->angle, n->scale, *(const uint32_t *)n->rgb, n->fade, MODEL_ColumnFlash, PacketCursor());
		DrawModel(n);
		FX_PART(0);
		n->h.counter++;
		if ((n->h.flags & 1) && n->h.children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Glow (0x8D6200)
	// ------------------------------------------------------------------
	// phases 0x8D63F0 (grey up to the level), 0x8D6420 (the emitter's signal: pulse), 0x8D6440
	// (grey down once the pulse is over, then hidden and done), 0x8D6480
	static void GlowPhase(SpriteNode *p)
	{
		const DoubleParams::Glows &g = P().glows;
		switch (p->h.phase)
		{
		case 0:
			AddGrey(p->rgb, (uint8_t)g.up);
			if (p->rgb[0] >= g.level) p->h.phase++;
			break;
		case 1:
			if (((EmitterNode *)p->h.emitter)->signal == 2)
			{
				p->pulse = 3;
				p->h.phase++;
			}
			break;
		case 2:
			if (p->pulse == 0)
			{
				AddGrey(p->rgb, (uint8_t)-g.down);
				if (p->rgb[0] == 0)
				{
					*(uint8_t *)&p->h.flags |= 5;
					p->h.phase++;
				}
			}
			break;
		default: break;
		}
	}

	static uint32_t __cdecl GlowTask(TaskNode *t)
	{
		SpriteNode *p = (SpriteNode *)t;
		const DoubleParams &pp = P();
		GlowPhase(p);
		GreyPulseStep(&p->pulse, p->rgb, pp.glows.pulse.peak, pp.glows.pulse.step, pp.glows.level);
		// on the rune's circle, on the ground
		CirclePlace(p->h.pos, p->base, p->angle, pp.runes.radius, 0x4000);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_GLOW, p);)
		FX_PART(PART_GLOWS);
		parts::DrawGroundFlipbook(p->h.pos, (p->h.flags & 4) != 0, SEQ_Glow, 0x400, 0, 0, *(const uint32_t *)p->rgb, PacketCursor());
		FX_PART(0);
		p->h.counter++;
		if ((p->h.flags & 1) && p->h.children == 0)
		{
			ReleaseLinkedTask(p);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Mote (0x8D6490)
	// ------------------------------------------------------------------
	static uint32_t __cdecl MoteTask(TaskNode *t)
	{
		SpriteNode *p = (SpriteNode *)t;
		const DoubleParams::Motes &m = P().motes;
		// spiral in: angle, grey up to the brightest, radius down (done when it is gone)
		p->ay = (int16_t)((p->ay + m.spin) & 0xFFF);
		if (p->rgb[0] < m.brightest) AddGrey(p->rgb, (uint8_t)m.brighten);
		p->radius = (int16_t)(p->radius - m.shrink);
		if (p->radius <= 0)
		{
			*(uint8_t *)&p->h.flags |= 1;
			p->h.phase++;
		}
		CirclePlace(p->h.pos, p->base, p->ay, p->radius, 4096);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_MOTE, p);)
		FX_PART(PART_MOTES);
		parts::DrawSprite(p->h.pos, p->seq, p->frame, p->shade, (p->h.flags & 4) != 0, fxp::Colour{ 0x80, 0x80, 0x80, 0 }, 0x1000, PacketCursor());
		FX_PART(0);
		// sub_8D65F0: next flipbook frame (looping)
		p->frame++;
		if (p->frame > p->last) p->frame = 0;
		p->h.counter++;
		if ((p->h.flags & 1) && p->h.children == 0)
		{
			ReleaseLinkedTask(p);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Rune (0x8D5C80)
	// ------------------------------------------------------------------
	// sub_8D5D50: the rune's column
	static void SpawnColumn(ModelNode *r)
	{
		const DoubleParams &pp = P();
		ModelNode *c = (ModelNode *)AddEffectTask(QModels(), ORIG_Column, 0x6C, r);
		c->radius = pp.runes.radius;
		c->flash_on = 0;
		c->flagged = r->flagged;
		c->arg = r->arg;
		CopyAnchorXZFromSource(c, c->h.pos);
		c->h.pos[1] = (int16_t)(c->h.pos[1] - pp.runes.lift);
		memcpy(c->base, c->h.pos, 8);
		c->orbit = r->orbit;
		c->angle[1] = r->orbit;
		c->model = MODEL_Column;
		c->fade = 0x1000;
		SetScale(c->scale, 0x10);
	}

	// sub_8D5DE0: a mote a tick over the motes' ticks
	static void SpawnMote(ModelNode *r)
	{
		const DoubleParams::Motes &m = P().motes;
		if (r->h.counter < m.from || r->h.counter > m.last) return;
		SpriteNode *p = (SpriteNode *)AddEffectTask(QSprites(), ORIG_Mote, 0x68, r);
		memcpy(p->base, p->h.pos, 8);
		p->seq = SEQ_Mote;
		p->radius = (int16_t)((CrtRand() & m.radius.mask) + m.radius.base);
		p->last = m.frames;
		p->ay = (int16_t)(CrtRand() & 0xFFF);
		SetScale(p->scale, 0x1000);
	}

	// phases 0x8D6160 (fade in, spawning), 0x8D6190 (shrink and fade out, then hidden and done), 0x8D61F0
	static void RunePhase(ModelNode *n)
	{
		const DoubleParams::Runes &r = P().runes;
		switch (n->h.phase)
		{
		case 0:
			n->fade = (int16_t)(n->fade - r.fade_in);
			*(uint8_t *)&n->h.flags |= 2;
			if (n->fade <= 0)
			{
				n->fade = 0;
				n->h.phase++;
			}
			break;
		case 1:
			n->fade = (int16_t)(n->fade + r.fade_out);
			n->scale[0] -= r.shrink;
			n->scale[1] -= r.shrink;
			n->scale[2] -= r.shrink;
			if (n->fade >= 0x1000)
			{
				n->fade = 0x1000;
				SetScale(n->scale, r.end_scale);
				n->h.flags = (uint16_t)((n->h.flags & 0xFFFD) | 5);
				n->h.phase++;
			}
			break;
		default: break;
		}
	}

	static uint32_t __cdecl RuneTask(TaskNode *t)
	{
		ModelNode *n = (ModelNode *)t;
		const DoubleParams &pp = P();
		RunePhase(n);
		// spin, and the place on the circle
		n->angle[1] = (int16_t)((n->angle[1] + pp.runes.spin) & 0xFFF);
		CirclePlace(n->h.pos, n->base, n->orbit, pp.runes.radius, 0x4000);
		if (n->h.flags & 2)
		{
			if (n->h.counter == pp.columns.tick) SpawnColumn(n);
			SpawnMote(n);
		}
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_RUNE, n);)
		FX_PART(PART_RUNES);
		DrawModel(n);
		FX_PART(0);
		n->h.counter++;
		if ((n->h.flags & 1) && n->h.children == 0)
		{
			ReleaseLinkedTask(n);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Sequencer (0x8D5B40)
	// ------------------------------------------------------------------
	// sub_8D5BD0: a rune at `angle` on the circle around the target and its glow
	static void SpawnRune(SeqNode *s, int32_t angle, int16_t flagged)
	{
		const DoubleParams &pp = P();
		ModelNode *r = (ModelNode *)AddEffectTask(QModels(), ORIG_Rune, 0x6C, s);
		CopyAnchorXZFromSource(r, r->h.pos);
		r->h.pos[1] = (int16_t)(r->h.pos[1] - pp.runes.lift);
		memcpy(r->base, r->h.pos, 8);
		const int16_t a = (int16_t)(angle & 0xFFF);
		r->arg = 0;
		r->flagged = flagged;
		r->orbit = a;
		r->model = MODEL_Rune;
		r->fade = 0x1000;
		SetScale(r->scale, 0x1000);
		SpriteNode *g = (SpriteNode *)AddEffectTask(QSprites(), ORIG_Glow, 0x68, s);
		CopyAnchorXZFromSource(g, g->h.pos);
		memcpy(g->base, g->h.pos, 8);
		g->angle = a;
	}

	// phases 0x8D5BB0, 0x8D6610, 0x8D6640, 0x8D6670, 0x8D66A0 (one rune each, the last one flagged,
	// then done), 0x8D66D0
	static uint32_t __cdecl SequencerTask(TaskNode *t)
	{
		SeqNode *s = (SeqNode *)t;
		const DoubleParams::Runes &r = P().runes;
		if (s->h.phase <= r.last)
		{
			SpawnRune(s, s->h.phase * r.angle_step, s->h.phase == r.last);
			if (s->h.phase == r.last) *(uint8_t *)&s->h.flags |= 1;
			s->h.phase++;
		}
		s->h.counter++;
		if ((s->h.flags & 1) && s->h.children == 0)
		{
			ReleaseLinkedTask(s);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Flash (0x8D6790)
	// ------------------------------------------------------------------
	// MAG_004_sub_8D6710: on the target's top anchor, unless the target record's +3 has the bits
	static void SpawnFlash(EmitterNode *e)
	{
		const DoubleParams::Flash &fp = P().flash;
		const uint8_t *targets = e->h.ctx->actions[e->h.action].targets;
		if (targets[3] & P().emitter.target_bit) return;
		FlashNode *f = (FlashNode *)AddEffectTask(QFlashes(), ORIG_Flash, 0x50, e);
		CopyAnchorTopFromSource(f, f->h.pos);
		memcpy(f->target, f->h.pos, 8);
		f->tx = fp.tex.tx;
		f->ty = fp.tex.ty;
		f->cx = fp.tex.cx;
		f->cy = fp.tex.cy;
		f->u = fp.tex.u;
		f->v = fp.tex.v;
		f->tw = fp.tex.w;
		f->th = fp.tex.h;
	}

	// 0: jump; 1: falls back to its rest height; 2: done at its last tick
	static void FlashPhase(FlashNode *f)
	{
		const DoubleParams::Flash &fp = P().flash;
		switch (f->h.phase)
		{
		case 0:
			f->vel = fp.jump;
			f->h.phase++;
			break;
		case 1:
			f->vel = (int16_t)(f->vel + fp.gravity);
			f->h.pos[1] = (int16_t)(f->h.pos[1] + f->vel);
			if (f->h.pos[1] >= f->target[1])
			{
				f->h.pos[1] = f->target[1];
				f->h.phase++;
			}
			break;
		case 2:
			if (f->h.counter >= fp.ticks)
			{
				*(uint8_t *)&f->h.flags |= 1;
				f->h.phase++;
			}
			break;
		default: break;
		}
	}

	static uint32_t __cdecl FlashTask(TaskNode *t)
	{
		FlashNode *f = (FlashNode *)t;
		FlashPhase(f);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note(REC_FLASH, f);)
		FX_PART(PART_FLASH);
		const TexQuad q = { f->tx, f->ty, f->cx, f->cy, f->u, f->v, f->tw, f->th };
		PacketCursor() = ScreenTexQuad(f->h.pos, q, PacketCursor());
		FX_PART(0);
		f->h.counter++;
		if ((f->h.flags & 1) && f->h.children == 0)
		{
			ReleaseLinkedTask(f);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Emitter (0x8D52A0)
	// ------------------------------------------------------------------
	static uint32_t __cdecl EmitterTask(TaskNode *t)
	{
		EmitterNode *e = (EmitterNode *)t;
		const DoubleParams::Emitter &ep = P().emitter;
		EmitterUpdatePos(e);
		ComputeModelBounds(e);
		switch (e->h.phase)
		{
		case 0: // 0x8D5B10: the rune sequencer
			AddEffectTask(QSequencers(), ORIG_Sequencer, 0x40, e);
			e->h.phase++;
			break;
		case 1: // 0x8D66E0: the root goes on, the flash
			if (e->h.counter >= ep.release_tick)
			{
				((RootNode *)e->h.root)->busy = 0;
				SpawnFlash(e);
				e->h.phase++;
			}
			break;
		case 2: // 0x8D6940: the action result, done
			ApplyActionResultToTarget(e->h.ctx->actions[e->h.action].targets);
			*(uint8_t *)&e->h.flags |= 1;
			e->h.phase++;
			break;
		default: break; // 0x8D6970
		}
		if (e->h.counter == ep.sound_tick) BdPlaySE((const void *)fxp::sound(ep.sound, (uint32_t)SOUND_Double), 0, ep.sound_pan);
		switch (e->signal)
		{
		case 1: // the last rune's column settled: screen fade and the big rune
			ScreenFadeTask(ep.fade.a, ep.fade.b, ep.fade.c, ep.fade.d);
			SpawnBigRune(e);
			e->signal++;
			break;
		case 2:
			e->signal = 0;
			break;
		default: break;
		}
		e->h.counter++;
		if ((e->h.flags & 1) && e->h.children == 0)
		{
			ReleaseLinkedTask(e);
			return TASK_END;
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Root (0x8D5180)
	// ------------------------------------------------------------------
	static void RootPhase(RootNode *r)
	{
		switch (r->h.phase)
		{
		case 0: // 0x8D5260
			r->h.phase++;
			break;
		case 1: // 0x8D5270: the emitter of the current action
			r->busy = 1;
			AddEffectTask(QEmitters(), ORIG_Emitter, 0x5C, r);
			r->h.phase++;
			break;
		case 2: // 0x8D6980: when the action is over, the next one (back to 1) or on
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
		case 3: // 0x8D69B0: every queue empty, done
			if (r->alive == 0)
			{
				*(uint8_t *)&r->h.flags |= 1;
				r->h.phase++;
			}
			break;
		default: break; // 0x8D69D0
		}
	}

	static uint32_t __cdecl RootTask(TaskNode *t)
	{
		RootNode *r = (RootNode *)t;
		if (r->tick & 1) PacketCursor() = var<uint32_t>(ARENA_A);
		else PacketCursor() = var<uint32_t>(ARENA_B);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(held_note_root(r);)
		UpdateTargetPosFromBones(r);
		RootPhase(r);
		r->alive = (int16_t)ExecuteTaskQueue(QEmitters());
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QSequencers()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QSprites()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QModels()));
		r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QFlashes()));
		r->tick++;
		r->h.counter++;
		if ((r->h.flags & 1) && r->h.children == 0)
		{
			ReleaseLinkedTask(r);
			return TASK_END;
		}
		return 0;
	}
}

	// ------------------------------------------------------------------
	// Parameter table (fx_params.h): what an editor shows of DoubleParams
	// ------------------------------------------------------------------
	namespace double004
	{
		using fxp::Field;
#define DOUBLE_FIELD(part, member, label, doc, type, lo, hi) \
		{ part, #member, label, doc, (uint32_t)offsetof(DoubleParams, member), fxp::type, lo, hi }
#define DOUBLE_I16(part, member, label, doc, lo, hi) DOUBLE_FIELD(part, member, label, doc, I16, lo, hi)
		static const fxp::Part PARTS[] = {
			{ 0, "emitter", "Emitter", "One per action, on its target: starts the rune sequencer, plays the sound, lets the next action start, gives the result; when the last rune's column settles, a screen fade and the big rune (draws nothing)" },
			{ PART_RUNES, "runes", "Runes", "Prim models on a circle around the target, one a tick, each fading in and spinning, then shrinking out" },
			{ PART_COLUMNS, "columns", "Columns", "A column on each rune: it grows, settles and flashes (the last one's starts the big rune), flashes again then fades out" },
			{ PART_MOTES, "motes", "Motes", "Flipbooks spiralling in towards each rune while it fades in" },
			{ PART_GLOWS, "glows", "Glows", "A flipbook on the ground under each rune, brightening, pulsing when the big rune starts, then fading" },
			{ PART_BIG, "big", "Big rune", "A prim model over the target, fading in (its sprites and rings) then out" },
			{ PART_SPRITES, "sprites", "Sprites", "Flipbooks around the big rune while it fades in, their frames and angles at random, brightening then dimming" },
			{ PART_MORPH, "morph", "Morphing rings", "Prim models whose vertices blend from one shape to another while they fade" },
			{ PART_FLASH, "flash", "Flash", "A textured quad on the target's top that jumps up and falls back" },
		};
		static const fxp::Group GROUPS[] = {
			{ "emitter.fade", "Screen fade", "ScreenFadeTask's four values" },
			{ "columns.flash", "Flash", "Its first flash at fade First (it settled), its second at fade Second (the big rune started), each down by its step a tick, in grey Grey" },
			{ "motes.radius", "Spiral radius", "Base + (rand & Mask)" },
			{ "glows.pulse", "Pulse", "When the big rune starts: grey Peak, then down by Step a tick back to the level" },
			{ "sprites.x", "Across", "x = rand % Mod + Base (then turned with the sprite)" },
			{ "sprites.grey", "Grey", "A sprite's grey tick by tick: A, B, C, C, B, A, then done" },
			{ "flash.tex", "Texture", "The quad's texture page (VRAM x, y), CLUT (VRAM x, y), texel origin and size" },
		};
		static const Field FIELDS[] = {
			DOUBLE_FIELD("emitter", emitter.sound, "Sound", "The sound block played (its world sound id: an audio.dat entry)", Sound, 0, 0x7FFFFFFF),
			DOUBLE_I16("emitter", emitter.sound_tick, "Sound tick", "The sound plays when the emitter tick == this", 0, 200),
			DOUBLE_I16("emitter", emitter.sound_pan, "Sound pan", "Where it plays between the speakers: 128 the middle (BdPlaySE's pan)", 0, 255),
			DOUBLE_I16("emitter", emitter.release_tick, "Next action tick", "From this emitter tick the next action may start; the flash starts then", 0, 200),
			DOUBLE_I16("emitter", emitter.fade.a, "A", "", -0x8000, 0x7FFF),
			DOUBLE_I16("emitter", emitter.fade.b, "B", "", -0x8000, 0x7FFF),
			DOUBLE_I16("emitter", emitter.fade.c, "C", "", -0x8000, 0x7FFF),
			DOUBLE_I16("emitter", emitter.fade.d, "D", "", -0x8000, 0x7FFF),
			DOUBLE_I16("emitter", emitter.target_bit, "No flash bits", "No flash when the target record's +3 has these bits", 0, 255),
			DOUBLE_I16("runes", runes.last, "Last rune", "Runes 0 .. this, one a tick; the last one's column starts the big rune", 0, 20),
			DOUBLE_I16("runes", runes.angle_step, "Angle step", "Rune k at angle k x this on the circle (4096: a full turn)", -0x1000, 0x1000),
			DOUBLE_I16("runes", runes.lift, "Height", "The circle this high over the target's feet (the columns' and the big rune's too)", -0x4000, 0x4000),
			DOUBLE_I16("runes", runes.radius, "Radius", "The circle's radius: 1024 x this / 4096 (the glows' and columns' too)", -0x4000, 0x4000),
			DOUBLE_I16("runes", runes.fade_in, "Fade in", "Its fade goes down by this a tick from 4096 (hidden) to 0 while it spawns", 1, 0x1000),
			DOUBLE_I16("runes", runes.fade_out, "Fade out", "Then up by this a tick back to 4096...", 1, 0x1000),
			DOUBLE_I16("runes", runes.shrink, "Shrink", "...while its size goes down by this a tick...", -0x1000, 0x1000),
			DOUBLE_I16("runes", runes.end_scale, "End size", "...to this, then it is hidden and done", 0, 0x7FFF),
			DOUBLE_I16("runes", runes.spin, "Spin", "It turns this much a tick about y", -0x1000, 0x1000),
			DOUBLE_I16("columns", columns.tick, "Rune tick", "A rune's column starts at this rune tick (while it spawns)", 0, 200),
			DOUBLE_I16("columns", columns.fade_in, "Fade in", "Its fade goes down by this a tick (from 4096) while...", 1, 0x1000),
			DOUBLE_I16("columns", columns.grow, "Widen", "...its width goes up by this a tick; seen, its width is...", -0x1000, 0x1000),
			DOUBLE_I16("columns", columns.width, "Width", "...this", 0, 0x7FFF),
			DOUBLE_I16("columns", columns.rise, "Rise", "Then its height up by this a tick...", 1, 0x1000),
			DOUBLE_I16("columns", columns.peak, "Peak", "...to this...", 0x1000, 0x7FFF),
			DOUBLE_I16("columns", columns.settle, "Settle", "...then down by this a tick (its width up by it) back to 4096: it flashes", 1, 0x1000),
			DOUBLE_I16("columns", columns.fade_out, "Fade out", "At the end its fade up by this a tick to 4096, then done", 1, 0x1000),
			DOUBLE_I16("columns", columns.flash.first, "First", "", 0, 0x1000),
			DOUBLE_I16("columns", columns.flash.second, "Second", "", 0, 0x1000),
			DOUBLE_I16("columns", columns.flash.step_first, "First step", "", 1, 0x1000),
			DOUBLE_I16("columns", columns.flash.step_second, "Second step", "", 1, 0x1000),
			DOUBLE_I16("columns", columns.flash.grey, "Grey", "", 0, 255),
			DOUBLE_I16("columns", columns.spin, "Spin", "It turns this much a tick about y", -0x1000, 0x1000),
			DOUBLE_I16("columns", columns.orbit_speed, "Orbit speed", "It goes round the circle this much a tick (the game's: 0)", -0x1000, 0x1000),
			DOUBLE_I16("motes", motes.from, "From tick", "A mote a rune tick from this rune tick...", 0, 200),
			DOUBLE_I16("motes", motes.last, "Last tick", "...to this one (while the rune spawns)", 0, 200),
			DOUBLE_I16("motes", motes.radius.base, "Base", "", 0, 0x4000),
			DOUBLE_I16("motes", motes.radius.mask, "Mask", "", 0, 0x7FFF),
			DOUBLE_I16("motes", motes.spin, "Spin", "Its angle on the spiral goes round this much a tick", -0x1000, 0x1000),
			DOUBLE_I16("motes", motes.brighten, "Brighten", "Its grey up by this a tick while below...", 0, 255),
			DOUBLE_I16("motes", motes.brightest, "Brightest", "...this", 0, 255),
			DOUBLE_I16("motes", motes.shrink, "Shrink", "Its radius down by this a tick: done when it is gone", 1, 0x1000),
			DOUBLE_I16("motes", motes.frames, "Last frame", "Its flipbook's last frame (it loops)", 0, 100),
			DOUBLE_I16("glows", glows.up, "Brighten", "Its grey up by this a tick to...", 1, 255),
			DOUBLE_I16("glows", glows.level, "Level", "...this", 0, 255),
			DOUBLE_I16("glows", glows.pulse.peak, "Peak", "", 0, 255),
			DOUBLE_I16("glows", glows.pulse.step, "Step", "", 1, 255),
			DOUBLE_I16("glows", glows.down, "Dim", "At the end its grey down by this a tick to 0, then hidden and done", 1, 255),
			DOUBLE_I16("big", big.lift, "Height", "This high over the target's feet (the morphing rings' too)", -0x4000, 0x4000),
			DOUBLE_I16("big", big.turn, "Turn", "Turned this much about y (the morphing rings' too)", -0x1000, 0x1000),
			DOUBLE_I16("big", big.fade, "Fade", "Its fade starts at this...", 0, 0x1000),
			DOUBLE_I16("big", big.fade_in, "Fade in", "...goes down by this a tick to 0 (it spawns its sprites and rings)...", 1, 0x1000),
			DOUBLE_I16("big", big.fade_out, "Fade out", "...then up by this a tick to 4096: hidden and done", 1, 0x1000),
			DOUBLE_I16("big", big.grey, "Grey", "Its grey while it fades in (then black)", 0, 255),
			DOUBLE_I16("sprites", sprites.from, "From tick", "Sprites over big rune ticks from this...", 0, 200),
			DOUBLE_I16("sprites", sprites.last, "Last tick", "...to this one...", 0, 200),
			DOUBLE_I16("sprites", sprites.count, "Count", "...this many a tick", 1, 50),
			DOUBLE_I16("sprites", sprites.angle_step, "Angle step", "Sprite k turned k x this about y", -0x1000, 0x1000),
			DOUBLE_I16("sprites", sprites.x.mod, "Mod", "", 1, 0x4000),
			DOUBLE_I16("sprites", sprites.x.base, "Base", "", -0x4000, 0x4000),
			DOUBLE_I16("sprites", sprites.back, "Out", "z = -this from the big rune (then turned)", -0x4000, 0x4000),
			DOUBLE_I16("sprites", sprites.grey.a, "A", "", 0, 255),
			DOUBLE_I16("sprites", sprites.grey.b, "B", "", 0, 255),
			DOUBLE_I16("sprites", sprites.grey.c, "C", "", 0, 255),
			DOUBLE_I16("sprites", sprites.hop, "Frame hop", "Its frame a tick: (frame + 1 + rand % this) & Frame mask", 1, 100),
			DOUBLE_I16("sprites", sprites.mask, "Frame mask", "", 0, 255),
			DOUBLE_I16("morph", morph.first, "First tick", "A morphing ring at this big rune tick...", 0, 200),
			DOUBLE_I16("morph", morph.second, "Second tick", "...and this one (while it spawns)", 0, 200),
			DOUBLE_I16("morph", morph.step, "Step", "Its blend and its fade up by this a tick: done at 4096", 1, 0x1000),
			DOUBLE_I16("flash", flash.jump, "Jump", "Its speed up at the start (y goes down: negative)...", -0x4000, 0x4000),
			DOUBLE_I16("flash", flash.gravity, "Gravity", "...slowing by this a tick until it is back at the target's top", -0x4000, 0x4000),
			DOUBLE_I16("flash", flash.ticks, "End tick", "It ends at this tick", 0, 200),
			DOUBLE_I16("flash", flash.tex.tx, "Page x", "", 0, 1023),
			DOUBLE_I16("flash", flash.tex.ty, "Page y", "", 0, 511),
			DOUBLE_I16("flash", flash.tex.cx, "CLUT x", "", 0, 1023),
			DOUBLE_I16("flash", flash.tex.cy, "CLUT y", "", 0, 511),
			DOUBLE_I16("flash", flash.tex.u, "U", "", 0, 255),
			DOUBLE_I16("flash", flash.tex.v, "V", "", 0, 255),
			DOUBLE_I16("flash", flash.tex.w, "Width", "", 0, 255),
			DOUBLE_I16("flash", flash.tex.h, "Height", "", 0, 255),
		};
#undef DOUBLE_I16
#undef DOUBLE_FIELD
	}

	void register_mag004_double()
	{
		fxp::register_params({ 4, "Double", &double004::g_params, &double004::DEFAULTS, sizeof(double004::DoubleParams),
		                       double004::PARTS, (int)(sizeof(double004::PARTS) / sizeof(double004::PARTS[0])),
		                       double004::FIELDS, (int)(sizeof(double004::FIELDS) / sizeof(double004::FIELDS[0])),
		                       double004::GROUPS, (int)(sizeof(double004::GROUPS) / sizeof(double004::GROUPS[0])) });
		register_port(double004::ORIG_Root, (void *)double004::RootTask, "004 Root", 4);
		register_port(double004::ORIG_Emitter, (void *)double004::EmitterTask, "004 Emitter", 4);
		register_port(double004::ORIG_Sequencer, (void *)double004::SequencerTask, "004 Sequencer", 4);
		register_port(double004::ORIG_Rune, (void *)double004::RuneTask, "004 Rune", 4);
		register_port(double004::ORIG_Column, (void *)double004::ColumnTask, "004 Column", 4);
		register_port(double004::ORIG_Glow, (void *)double004::GlowTask, "004 Glow", 4);
		register_port(double004::ORIG_Mote, (void *)double004::MoteTask, "004 Mote", 4);
		register_port(double004::ORIG_BigRune, (void *)double004::BigRuneTask, "004 BigRune", 4);
		register_port(double004::ORIG_Morph, (void *)double004::MorphTask, "004 Morph", 4);
		register_port(double004::ORIG_Sprite, (void *)double004::SpriteTask, "004 Sprite", 4);
		register_port(double004::ORIG_Flash, (void *)double004::FlashTask, "004 Flash", 4);
		// 30 fps layer: see mag004_double_held.inc
		FX_HELD(register_mag004_held();)
	}
	FX_LAB_MODULE(register_mag004_double);
}

#ifdef FF8_FX_HELD
#include "mag004_double_held.inc"
#endif
