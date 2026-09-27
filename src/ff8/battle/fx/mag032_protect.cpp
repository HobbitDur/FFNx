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

// Effect 32: Protect (spell, MAG_032_*): a copy of the actor/heal effect library (see act_engine.h).
//
// Setup MAG_032_PROTECT 0x86B9A0 (runs once, not ported; file loader 0x86B980 = the effect's data
// file 0x26C3954, its two packet arenas at file + 0xB800 / + 0x17000; camera animation 0x15F8278)
// creates the root queue 0x26C3A40 with the master and five task pools: 0x26CD978 (4 x 0x58:
// emitters), 0x26C3890 (3 x 0x3C: screen flashes), 0x26C3A50 (200 x 0x88: particle spawner and
// particles), 0x26CD6D0 (10 x 0x504: shield prim-model players), 0x26CD968 (10 x 0x40: stage
// wobble).
//   Master (0x86BB00) - camera copy 0x2793E58, packet arena by tick parity (cursor 0x26C3958), bone
//     follow, 11-state table: 0x86BC70 starts the stage wobble in (0x86BCA0: amplitude 0x1D98992 +
//     k * 0x2C up to 0x400), 0x86BD90 one emitter per action (loop 0x86C820 once the emitter
//     releases the master's +0x63 hold), 0x86C860 after 7 more ticks the wobble out (0x86C890),
//     then waits for the queues to empty. Queues run: emitters, flashes, particles, shields, wobble.
//   Emitter (0x86BDC0), one per action: bone follow + model bounds (CURE_Emitter helpers), sound at
//     its first tick, a screen flash at tick 22 (0x86BE70, colour script 0x15F3B14); state 0
//     (0x86BF70) spawns the shield player (0x86C000, model 0x15F3B24, animation 0x470) and the
//     particle spawner (0x86C3A0), state 1 releases the master at tick 18, state 2 applies the
//     action result (tick >= 18).
//   Shield player (0x86C000, engine task body a_73A380): state 0 (0x86C060) decodes the model's prim
//     layout and takes the target's effect bone 0xF1 as position (height clamped to <= -0x200),
//     state 1 (0x86C0C0) plays the model with the shared player 0x701970 and the draw callback
//     0x86C160 (parameter block = camera * target facing * position) until it ends.
//   Particle spawner (0x86C3A0): state 0 (0x86C400) bone position + a facing matrix at +0x58, from
//     tick 20 (0x86C480) state 2 (0x86C490) spawns table[tick] (0x15F8D94) particles per tick until
//     tick 24: each one turns the spawner matrix by a random angle, starts at (0, -0x280, -0x280) of
//     it and flies along the matrix's third column at a random speed.
//   Particle (0x86C5C0): state 0 (0x86C690) flipbook 0x15F8C64 (12 frames), state 1 (0x86C6B0)
//     moves by its velocity, velocity -= velocity / 5 every tick, until the flipbook ends; drawn
//     after its update (0x86C620: sprite sequence through TransformCameraByShadowRotation).
//   Screen flash (0x86BE70): a full-screen flat quad (+ draw mode) coloured by its script.
// No task tests the draw-only flags (battle_to_update_flags 0x201).
// Module globals: 0x26C3890..0x26CDCC8 (pools, queues, file pointer 0x26C3954, packet cursor
// 0x26C3958, arenas, prim-model blend buffer 0x26CDAF8), camera copy 0x2793E58 (0x20 bytes), the
// stage wobble words 0x1D98992 + k * 0x2C, the shadow-camera scratch 0x21DFED0 (0x20 bytes).

#include "act_engine.h"

namespace ff8fx
{
namespace act
{
namespace protect
{
	// ---- generated (gendesc.py): the module descriptor of this copy of the library ----
	static const Mod MOD_032 = { "protect", 32, 0x86B980, 0x86C9C0,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x26CD978, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x86BDC0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

	// --- module globals ---
	static const uint32_t PACKET_CURSOR = 0x26C3958;   // module packet cursor (every draw's arena)
	static const uint32_t Q_EMITTER = 0x26CD978;       // emitters (0x58-byte nodes)
	static const uint32_t Q_FLASH = 0x26C3890;         // screen flashes (0x3C)
	static const uint32_t Q_PARTICLE = 0x26C3A50;      // particle spawner + particles (0x88)
	static const uint32_t Q_SHIELD = 0x26CD6D0;        // shield prim-model players (0x504)
	static const uint32_t Q_STAGE = 0x26CD968;         // stage wobble (0x40)
	static const uint32_t BLEND_BUFFER = 0x26CDAF8;    // vertex frames blended by MAG_017_sub_701390

	static const uint32_t ORIG_Master = 0x86BB00;
	static const uint32_t ORIG_Emitter = 0x86BDC0;
	static const uint32_t ORIG_Flash = 0x86BE70;
	static const uint32_t ORIG_ShieldPlayer = 0x86C000;
	static const uint32_t ORIG_ShieldDraw = 0x86C160;
	static const uint32_t ORIG_Spawner = 0x86C3A0;
	static const uint32_t ORIG_Particle = 0x86C5C0;
	static const void *const SOUND_Protect = (const void *)0x15F3B20;

	// battle entity of a node's target slot (+0x2D)
	static inline uint32_t TargetEntity(uint32_t node)
	{
		return (uint32_t)U8(node, 0x2D) * 0x9C + 0x1D972C0;
	}

	// common task tail (frame counter already incremented): end (release the linked task, return 2)
	// when finished (status read before the increment) and no children are alive
	static inline uint32_t task_end(uint32_t node, uint8_t status)
	{
		if ((status & 1) != 0 && U8(node, 0x28) == 0)
		{
			x::Effect_ReleaseLinkedTask(node);
			return 2;
		}
		return 0;
	}
}
}
}

#ifdef FF8_FX_HELD
#include "mag032_protect_held.h"
#endif

namespace ff8fx
{
namespace act
{
namespace protect
{
	// ====================================================================================
	// master, stage wobble, emitter
	// ====================================================================================

	// 0x86BB00 (MAG_032_PROTECT_Tick): MASTER task - camera copy, packet arena by tick parity, bone
	// follow, 11-state table, the five queues (live task count -> node +0x5E)
	static uint32_t __cdecl Master(uint32_t a1)
	{
		g_mod = &MOD_032;
		// 30 fps layer: see mag032_protect_held.inc
		FX_HELD(held_note_master();)

		uint32_t states[11];
		memcpy((void *)0x2793E58, (const void *)0x1D97778, 8 * 4); // rep movsd: camera matrix copy
		const uint32_t node = a1;
		MEM<uint32_t>(0x26CDCBC) = 0x2793E58;
		MEM<uint32_t>(0x26C3974) = 0x2793E58;
		states[0] = 0x86BC50;
		const uint8_t parity = U8(node, 0x5C);
		states[1] = 0x86BC60;
		states[2] = 0x86BC70;
		states[3] = 0x86BD90;
		states[4] = 0x86C820;
		states[5] = 0x86C860;
		states[6] = 0x86C960;
		states[7] = 0x86C970;
		states[8] = 0x86C980;
		states[9] = 0x86C990;
		states[10] = 0x86C9B0; // nullsub (ret)
		if ((parity & 1) != 0)
		{
			const uint32_t v1 = MEM<uint32_t>(0x26CDAEC);
			const uint32_t v2 = MEM<uint32_t>(0x26CD6E4);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26C3970) = v2;
		}
		else
		{
			const uint32_t v1 = MEM<uint32_t>(0x26CDAE8);
			const uint32_t v2 = MEM<uint32_t>(0x26CD6E0);
			MEM<uint32_t>(PACKET_CURSOR) = v1;
			MEM<uint32_t>(0x26C3970) = v2;
		}
		x::Effect_UpdateTargetPosFromBones(node);
		callp(states[S8(node, 0x29)], node);
		U16(node, 0x5E) = 0;
		static const uint32_t queues[5] = { Q_EMITTER, Q_FLASH, Q_PARTICLE, Q_SHIELD, Q_STAGE };
		for (int i = 0; i < 5; i++)
			U16(node, 0x5E) = (uint16_t)(U16(node, 0x5E) + (uint16_t)x::ExecuteTaskQueue(queues[i]));
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x5C) = (uint16_t)(U16(node, 0x5C) + 1);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x86BC70 (MAG_032_sub_86BC70): master state - once no child is alive: the stage wobble in
	// (0x86BCA0, 0x40 bytes, stage queue), next state
	static uint32_t __cdecl StartWobbleIn(uint32_t a1)
	{
		if (U8(a1, 0x28) != 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x86BCA0, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x86BD40 (MAG_032_sub_86BD40, = Drain 0x8517B0): stage wobble in, state 1 - amplitude +0x1C up
	// by 0x100 per tick to 0x400 (then finished, next state), written to the four stage wobble words
	// 0x1D98992 + k * 0x2C
	static uint32_t __cdecl WobbleIn(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0x100);
		if (S16(a1, 0x1C) >= 0x400)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0x400;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	// 0x86BDC0 (MAG_032_sub_86BDC0): EMITTER task (one per action) - bone-follow anchor and model
	// bounds, state {0x86BF70 shield + spawner, 0x86C7B0 release the master, 0x86C7D0 action
	// result, ret}, sound at its first tick, the screen flash at tick 22
	static uint32_t __cdecl Emitter(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x86BF70;
		states[1] = 0x86C7B0;
		states[2] = 0x86C7D0;
		states[3] = 0x86C810; // nullsub (ret)
		x::MAG_001_CURE_Emitter_UpdatePos(node);
		x::MAG_001_CURE_Emitter_ComputeModelBounds(node);
		callp(states[S8(node, 0x29)], node);
		if (U16(node, 0x24) == 0)
			x::BdPlaySE(P(SOUND_Protect), 0, 0x80);
		if (U16(node, 0x24) == 0x16)
		{
			const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_FLASH, ORIG_Flash, 0x3C, node);
			U32(t, 0x30) = 0x15F3B14;
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// the screen flash's full-screen flat quad (320 x 216, code 0x2A, colour `colour`) and its draw
	// mode (0x45C690 / 0x45BFC0) into the module arena, OT bucket base + 0x1C
	static void FlashDraw(uint32_t colour)
	{
		const uint32_t bucket = MEM<uint32_t>(0x1D8E04C) + 0x1C;
		uint32_t pkt = MEM<uint32_t>(PACKET_CURSOR);
		U32(pkt, 4) = colour;
		U32(pkt, 0) = 0x5000000;
		U8(pkt, 7) = 0x2A;
		U32(pkt, 8) = 0;
		U32(pkt, 0xC) = 0x140;
		U32(pkt, 0x10) = 0xD80000;
		U32(pkt, 0x14) = 0xD80140;
		x::SSIGPU_InsertPrimAltViewport(bucket, pkt);
		pkt += 0x18;
		const uint32_t tpage = x::sub_45C690(0, 1, 0x280, 0) & 0xFFFF;
		x::sub_45BFC0(pkt, 0, 0, tpage, 0);
		x::SSIGPU_InsertPrimAutoDepth(bucket, pkt);
		MEM<uint32_t>(PACKET_CURSOR) = pkt + 0xC;
	}

	// 0x86BE70 (MAG_032_sub_86BE70, = Drain 0x851900): SCREEN FLASH task - colour script at +0x30
	// (dwords: colour, or 0xFE000000 | t: wait until tick t then take the next entry, 0xFF......:
	// finished), current colour +0x34, script index +0x38; draws the full-screen quad (FlashDraw).
	// Protect's script 0x15F3B14: 0x3F3F3F, 0x0F0F0F, end (two drawn ticks).
	static uint32_t __cdecl Flash(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint16_t idx = U16(node, 0x38);
		const uint32_t script = U32(node, 0x30);
		const uint32_t entry = U32(script, (int32_t)(int16_t)idx * 4);
		const uint16_t hi = (uint16_t)(entry >> 16);
		if ((hi >> 8) == 0xFF)
			U8(node, 0x26) |= 1;
		else
		{
			if ((hi >> 8) == 0xFE)
			{
				if (S16(node, 0x24) >= (int16_t)(hi & 0xFF))
				{
					const uint16_t next = (uint16_t)(idx + 1);
					U16(node, 0x38) = next;
					U32(node, 0x34) = U32(script, (int32_t)(int16_t)next * 4);
				}
			}
			else
			{
				U32(node, 0x34) = entry;
				U16(node, 0x38) = (uint16_t)(idx + 1);
			}
			// 30 fps layer: see mag032_protect_held.inc
			FX_HELD(held_note_flash(node);)
			FlashDraw(U32(node, 0x34));
		}
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x86BFB0 (MAG_032_sub_86BFB0): spawns a prim-model player task `fn` (0x504 bytes, shield queue)
	// under `parent`: +0x74 model data, +0x78 animation word (sign-extended), +0x80 / +0x82 modes
	static uint32_t SpawnPlayer(uint32_t parent, uint32_t fn_addr, uint32_t model, uint32_t anim, uint32_t w80, uint32_t w82)
	{
		const uint32_t t = x::Effect_AddTaskAndInitFromCtx(Q_SHIELD, fn_addr, 0x504, parent);
		U16(t, 0x80) = (uint16_t)w80;
		U32(t, 0x74) = model;
		S32(t, 0x78) = (int16_t)anim;
		U16(t, 0x82) = (uint16_t)w82;
		return t;
	}

	// 0x86BF70 (MAG_032_sub_86BF70): emitter state 0 - the shield player (0x86C000, model 0x15F3B24,
	// animation 0x470) and the particle spawner (0x86C3A0, 0x88 bytes, particle queue), next state
	static uint32_t __cdecl SpawnShieldAndParticles(uint32_t a1)
	{
		SpawnPlayer(a1, ORIG_ShieldPlayer, 0x15F3B24, 0x470, 0, 0);
		x::Effect_AddTaskAndInitFromCtx(Q_PARTICLE, ORIG_Spawner, 0x88, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// ====================================================================================
	// shield (prim-model player)
	// ====================================================================================

	// 0x86C060 (sub_86C060): shield player state 0 - decode the model's prim layout into +0x94,
	// position = the target's effect bone 0xF1 with the height clamped to <= -0x200, next state
	static uint32_t __cdecl ShieldInit(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t anim = U32(node, 0x78);
		const uint32_t entity = TargetEntity(node);
		x::Effect_DecodeModelPrimLayout(U32(node, 0x74), node + 0x94, anim);
		x::GetEffectSpawnPosition(entity, 0xF1, 0, node + 0x1C);
		if (S16(node, 0x1E) > -0x200)
			S16(node, 0x1E) = -0x200;
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// the shield's placement matrix (first 0x20 bytes of its parameter block): identity turned by
	// the target's facing (entity +0xE), translated to the node position, composed with the camera
	static void ShieldMatrix(uint32_t node, uint32_t blk)
	{
		const uint32_t entity = TargetEntity(node);
		x::MAG_022_sub_8DD770(blk);
		S32(blk, 0x14) = S16(node, 0x1C);
		S32(blk, 0x18) = S16(node, 0x1E);
		S32(blk, 0x1C) = S16(node, 0x20);
		x::MAG_022_sub_8DD8A0(blk, (uint32_t)(int32_t)S16(entity, 0xE));
		x::ComposeAffineTransform(0x1D97778, blk, blk);
	}

	// 0x86C0C0 (MAG_032_PROTECT_RenderShield): shield player state 1 - plays the model (shared player
	// 0x701970, callback 0x86C160) with a parameter block on the stack: +0x00 placement matrix
	// (ShieldMatrix), +0x48 blend buffer. When the model has ended: finished, next state.
	static uint32_t __cdecl RenderShield(uint32_t a1)
	{
		const uint32_t node = a1;
		// stack frame 0x5C: the callback reads the matrix +0x00..+0x1F and +0x48 only
		alignas(4) uint8_t fr[0x5C] = {};
		const uint32_t L = P(fr);
		ShieldMatrix(node, L);
		U32(L, 0x48) = BLEND_BUFFER;
		// 30 fps layer: see mag032_protect_held.inc
		FX_HELD(held_note_shield(node);)
		if (prim_play(node + 0x94, ORIG_ShieldDraw, L, 0) == 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x86C160 (sub_86C160): prim-model player draw callback (layout a1, record a2, block a3 of
	// RenderShield) - picks the object's vertex frame (blended by MAG_017_sub_701390 between two
	// frames), builds its matrix (record rotation, record offset through the block matrix, object
	// rotation composed with the block matrix unless flag 0x8000, offset first rotated by the object
	// when flag 0x800; scale), sets the fade and draws it into the module arena with
	// Effect_RenderPrimModel
	static uint32_t __cdecl ShieldDraw(uint32_t a1, uint32_t a2, uint32_t a3)
	{
		// 30 fps layer: see mag032_protect_held.inc
		FX_HELD(held_note_callback();)
		// stack (0x38 bytes, offsets as in the original frame after its pushes): +0x00 SVECTOR
		// offset, +0x08 scale (3 x int32), +0x18 Mat4x3 object matrix (+0x2C translation)
		alignas(4) uint8_t loc[0x38] = {};
		const uint32_t L = P(loc);
		const uint32_t M = L + 0x18;
		const uint32_t rec = a2;
		if (((uint32_t)(int32_t)S16(rec, 0x1C) | U32(rec, 0x18)) == 0)
			return 0; // void (zero scale: nothing drawn)
		if (S16(rec, 0x24) >= 0x1000 && U32(rec, 0x20) == 0)
			return 0; // void (full fade to black: nothing drawn)

		const uint32_t hdr = x::Field_Alloc(0x58);
		// 30 fps layer: see mag032_protect_held.inc
		FX_HELD(held_header(hdr, 0x58);)
		const int32_t model = S16(rec, 2);
		const uint32_t data = U32(a1, 0);
		const uint32_t blk = a3;
		const uint32_t base = data + U32(data, model * 4 + 8);
		const int16_t f1 = S16(rec, 0x2A);   // second vertex frame
		const int16_t f0 = S16(rec, 0x28);   // first vertex frame
		U32(hdr, 0) = base;
		// vertex frame f: base + 0xC + f * vertex_count * 8
		auto frame_ptr = [base](int16_t f) -> uint32_t {
			if (f == 0)
				return base + 0xC;
			const int32_t n = mul32(S32(base, 4), (int32_t)f);
			return base + (uint32_t)shl32(n, 3) + 0xC;
		};
		if (f0 == f1)
			U32(hdr, 4) = frame_ptr(f0);
		else
		{
			const int16_t t = S16(rec, 0x26);   // blend factor 0..0x1000
			if (t == 0)
				U32(hdr, 4) = frame_ptr(f0);
			else if (t == 0x1000)
				U32(hdr, 4) = frame_ptr(f1);
			else
			{
				x::MAG_017_sub_701390(base, (uint32_t)(int32_t)f0, (uint32_t)(int32_t)f1, (uint32_t)(int32_t)t, U32(blk, 0x48));
				U32(hdr, 4) = U32(blk, 0x48);
			}
		}

		x::MAG_017_sub_701310(rec + 0x10, M);
		U16(L, 2) = U16(rec, 0xA);
		const uint32_t flags = U32(rec, 4);
		U16(L, 0) = U16(rec, 8);
		U16(L, 4) = U16(rec, 0xC);
		if ((flags & 0x800) != 0)
			x::matrixMultiplyVector(M, L, L); // offset turned by the object's rotation
		x::GTE_SetRotMatrix(blk);
		x::GTE_LoadV0(L);
		x::GTE_MVMVA_RotV0();
		x::GTE_ReadMAC123(M + 0x14);
		if ((flags & 0x8000) == 0)
			x::GTE_MatrixMultiply(blk, M);
		S32(M, 0x14) = add32(S32(M, 0x14), S32(blk, 0x14));
		S32(M, 0x18) = add32(S32(M, 0x18), S32(blk, 0x18));
		S32(M, 0x1C) = add32(S32(M, 0x1C), S32(blk, 0x1C));
		if (U32(rec, 0x18) != 0x10001000 || U16(rec, 0x1C) != 0x1000)
		{
			S32(L, 0x8) = S16(rec, 0x18);
			S32(L, 0xC) = S16(rec, 0x1A);
			S32(L, 0x10) = S16(rec, 0x1C);
			x::scale3DMatrix(M, L + 0x8);
		}
		x::GTE_SetRotMatrix_W(M);
		x::GTE_SetTransVector_W(M);
		const int32_t fade = S16(rec, 0x24);
		U32(hdr, 0x1C) = 0x2030;
		U32(hdr, 0xC) = (uint32_t)fade;
		if (fade != 0)
		{
			U32(hdr, 0x1C) = 0x20F0;
			U32(hdr, 8) = U32(rec, 0x20);   // fade colour
		}
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		MEM<uint32_t>(PACKET_CURSOR) = x::Effect_RenderPrimModel(hdr, ot, 2, MEM<uint32_t>(PACKET_CURSOR));
		x::Field_Free(0x58);
		return 0; // void (prim player callback)
	}

	// ====================================================================================
	// particle spawner and particles
	// ====================================================================================

	// 0x86C3A0 (MAG_032_sub_86C3A0, Siren 0x73F250 pattern): PARTICLE SPAWNER task - state table
	// {0x86C400 init, 0x86C480 wait, 0x86C490 spawn, 0x86C7A0 ret}
	static uint32_t __cdecl Spawner(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[4];
		states[0] = 0x86C400;
		states[1] = 0x86C480;
		states[2] = 0x86C490;
		states[3] = 0x86C7A0; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x86C400 (sub_86C400): spawner state 0 - position = the target's effect bone 0xF1 (height
	// clamped to <= -0x200), matrix +0x58 = the target's facing (entity +0xE) with that position as
	// translation (+0x6C..+0x74), next state
	static uint32_t __cdecl SpawnerInit(uint32_t a1)
	{
		const uint32_t node = a1;
		const uint32_t entity = TargetEntity(node);
		x::GetEffectSpawnPosition(entity, 0xF1, 0, node + 0x1C);
		if (S16(node, 0x1E) > -0x200)
			S16(node, 0x1E) = -0x200;
		x::MAG_022_sub_8DD770(node + 0x58);
		x::MAG_022_sub_8DD8A0(node + 0x58, (uint32_t)(int32_t)S16(entity, 0xE));
		S32(node, 0x6C) = S16(node, 0x1C);
		S32(node, 0x70) = S16(node, 0x1E);
		S32(node, 0x74) = S16(node, 0x20);
		U8(node, 0x29) = (uint8_t)(U8(node, 0x29) + 1);
		return 0; // void
	}

	// 0x86C480 (sub_86C480): spawner state 1 - next state at tick 20
	static uint32_t __cdecl SpawnerWait(uint32_t a1)
	{
		if (S16(a1, 0x24) >= 0x14)
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return a1;
	}

	// 0x86C490 (sub_86C490): spawner state 2 - table[tick] (bytes 0x15F8D94) particles this tick:
	// each one turns the spawner matrix +0x58 by a random angle (rand & 0xFFF, 0x8DD960), starts at
	// (0, -0x280, -0x280) of it, and gets the velocity third column * (0x10 + rand & 0x1FF) / 4096 of
	// that matrix turned by 0x100 (0x8DD7E0); finished after tick 24
	static uint32_t __cdecl SpawnerEmit(uint32_t a1)
	{
		const uint32_t node = a1;
		alignas(4) uint8_t loc[0x20] = {};
		const uint32_t H = P(loc);
		int32_t count = (int16_t)(uint16_t)MEM<uint8_t>(0x15F8D94 + S16(node, 0x24));
		if (count > 0)
		{
			const uint32_t mat = node + 0x58;
			do
			{
				const uint32_t p = x::Effect_AddTaskAndInitFromCtx(Q_PARTICLE, ORIG_Particle, 0x88, node);
				x::MAG_022_sub_8DD770(H);
				S32(H, 0x14) = 0;
				S32(H, 0x18) = -0x280;
				S32(H, 0x1C) = -0x280;
				const int32_t angle = (int16_t)(x::CrtRand() & 0xFFF);
				x::sub_8DD960(mat, (uint32_t)angle);
				x::ComposeAffineTransform(mat, H, H);
				U16(p, 0x1C) = U16(H, 0x14);
				U16(p, 0x1E) = U16(H, 0x18);
				U16(p, 0x20) = U16(H, 0x1C);
				x::sub_8DD7E0(H, 0x100);
				const int32_t speed = (int16_t)((x::CrtRand() & 0x1FF) + 0x10);
				U16(p, 0x78) = (uint16_t)((int32_t)S16(H, 0x4) * speed / 4096);
				U16(p, 0x7A) = (uint16_t)((int32_t)S16(H, 0xA) * speed / 4096);
				U16(p, 0x7C) = (uint16_t)((int32_t)S16(H, 0x10) * speed / 4096);
			} while (--count != 0);
		}
		if (S16(node, 0x24) > 0x18)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// 0x86C620 (sub_86C620): particle draw (unless hidden, +0x26 bit2) - sprite sequence header on
	// the scratch stack (flipbook +0x4C, frame +0x50), camera turned toward the position (shadow
	// rotation +0x54), emitted into the module arena, OT base + 0x44
	static void ParticleDraw(uint32_t node)
	{
		if ((U8(node, 0x26) & 4) != 0)
			return;
		// 30 fps layer: see mag032_protect_held.inc
		FX_HELD(held_note_particle(node);)
		const uint32_t hdr = x::Field_Alloc(0xB4);
		// 30 fps layer: see mag032_protect_held.inc
		FX_HELD(held_header(hdr, 0xB4);)
		TransformCameraByShadowRotation((const void *)(node + 0x1C), 0x1000, S16(node, 0x54));
		U32(hdr, 0) = U32(node, 0x4C);
		const uint32_t cursor = MEM<uint32_t>(PACKET_CURSOR);
		const uint32_t ot = MEM<uint32_t>(0x1D8E04C) + 0x44;
		U16(hdr, 4) = U16(node, 0x50);
		U16(hdr, 0x24) = 0;
		MEM<uint32_t>(PACKET_CURSOR) = InitEffectSequenceFromData((void *)hdr, ot, 2, cursor);
		x::Field_Free(0xB4);
	}

	// 0x86C5C0 (sub_86C5C0, Tonberry 0x767290 pattern): PARTICLE task - state {0x86C690 init,
	// 0x86C6B0 move, 0x86C790 ret}, then its draw (0x86C620)
	static uint32_t __cdecl Particle(uint32_t a1)
	{
		const uint32_t node = a1;
		uint32_t states[3];
		states[0] = 0x86C690;
		states[1] = 0x86C6B0;
		states[2] = 0x86C790; // nullsub (ret)
		callp(states[S8(node, 0x29)], node);
		ParticleDraw(node);
		const uint8_t status = U8(node, 0x26);
		U16(node, 0x24) = (uint16_t)(U16(node, 0x24) + 1);
		return task_end(node, status);
	}

	// 0x86C690 (sub_86C690): particle state 0 - flipbook 0x15F8C64, last frame 11, next state
	static uint32_t __cdecl ParticleInit(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U32(a1, 0x4C) = 0x15F8C64;
		U16(a1, 0x52) = 0xB;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// v / -5 truncated toward zero, as compiled (imul 0x99999999; sar edx, 1; + sign bit)
	static inline int32_t DivMinus5(int32_t v)
	{
		int32_t hi = (int32_t)(((int64_t)v * (int64_t)(int32_t)0x99999999) >> 32);
		hi >>= 1;
		return hi + (int32_t)((uint32_t)hi >> 31);
	}

	// one particle motion step (int16 arithmetic): velocity v += v / -5 (the step -v/5 is kept at
	// +0x80..+0x84), position += the new velocity
	static void ParticleStep(uint32_t node)
	{
		const int32_t dx = DivMinus5(S16(node, 0x78));
		const uint16_t vx = (uint16_t)(U16(node, 0x78) + (uint16_t)dx);
		const int32_t dy = DivMinus5(S16(node, 0x7A));
		const uint16_t vy = (uint16_t)(U16(node, 0x7A) + (uint16_t)dy);
		U16(node, 0x1C) = (uint16_t)(U16(node, 0x1C) + vx);
		U16(node, 0x1E) = (uint16_t)(U16(node, 0x1E) + vy);
		U16(node, 0x80) = (uint16_t)dx;
		const int32_t dz = DivMinus5(S16(node, 0x7C));
		const uint16_t vz = (uint16_t)(U16(node, 0x7C) + (uint16_t)dz);
		U16(node, 0x82) = (uint16_t)dy;
		U16(node, 0x20) = (uint16_t)(U16(node, 0x20) + vz);
		U16(node, 0x84) = (uint16_t)dz;
		U16(node, 0x78) = vx;
		U16(node, 0x7A) = vy;
		U16(node, 0x7C) = vz;
	}

	// 0x86C6B0 (sub_86C6B0): particle state 1 - motion step (ParticleStep), flipbook step
	// (0x86C760 = a_743C20); at its end the particle is hidden and finished, next state
	static uint32_t __cdecl ParticleMove(uint32_t a1)
	{
		const uint32_t node = a1;
		ParticleStep(node);
		if (a_743C20(node) != 0)
		{
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x26) |= 1;
			U8(node, 0x29) = (uint8_t)(st + 1);
		}
		return 0; // void
	}

	// ====================================================================================
	// emitter states 1-2, master action loop, stage wobble out
	// ====================================================================================

	// 0x86C7B0 (MAG_032_sub_86C7B0): emitter state 1 - at tick 18 releases the master (root +0x63 =
	// 0: its action loop may spawn the next emitter), next state
	static uint32_t __cdecl ReleaseMaster(uint32_t a1)
	{
		if (U16(a1, 0x24) == 0x12)
		{
			U8(U32(a1, 0x10), 0x63) = 0;
			U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		}
		return a1;
	}

	// 0x86C7D0 (MAG_032_sub_86C7D0): emitter state 2 - from tick 18: action result of its target,
	// finished, next state
	static uint32_t __cdecl ActionResult(uint32_t a1)
	{
		const uint32_t node = a1;
		if (S16(node, 0x24) < 0x12)
			return 0; // void
		const int32_t action = S8(node, 0x2A);
		const int32_t target = S8(node, 0x2B);
		const uint32_t acts = U32(U32(node, 0xC), 4);
		const uint32_t targets = U32(acts + (uint32_t)(action * 20), 8);
		x::ApplyActionResultToTarget(targets + (uint32_t)(target * 24));
		const uint8_t st = U8(node, 0x29);
		U8(node, 0x26) |= 1;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x86C820 (MAG_032_sub_86C820): master state - action loop: while the master is not held
	// (+0x63) and actions remain (+0x2A < +0x58), the next action (+0x2A / +0x2E up, state back to
	// the emitter spawn); after the last one waits 7 ticks (+0x60) and goes on
	static uint32_t __cdecl ActionLoop(uint32_t a1)
	{
		const uint32_t node = a1;
		if (U8(node, 0x63) != 0)
			return 0; // void
		const uint8_t idx = U8(node, 0x2A);
		if ((int16_t)(int8_t)idx < S16(node, 0x58))
		{
			const uint8_t loops = U8(node, 0x2E);
			U8(node, 0x2A) = (uint8_t)(idx + 1);
			const uint8_t st = U8(node, 0x29);
			U8(node, 0x2E) = (uint8_t)(loops + 1);
			U8(node, 0x29) = (uint8_t)(st - 1);
			return 0; // void
		}
		const uint8_t st = U8(node, 0x29);
		U16(node, 0x60) = 7;
		U8(node, 0x29) = (uint8_t)(st + 1);
		return 0; // void
	}

	// 0x86C860 (MAG_032_sub_86C860, = Drain 0x856870): master state - counts +0x60 down; at 0 starts
	// the stage wobble out (0x86C890, 0x40 bytes, stage queue), next state
	static uint32_t __cdecl StartWobbleOut(uint32_t a1)
	{
		U16(a1, 0x60) = (uint16_t)(U16(a1, 0x60) - 1);
		if (S16(a1, 0x60) > 0)
			return 0; // void
		x::Effect_AddTaskAndInitFromCtx(Q_STAGE, 0x86C890, 0x40, a1);
		U8(a1, 0x29) = (uint8_t)(U8(a1, 0x29) + 1);
		return 0; // void
	}

	// 0x86C8F0 (MAG_032_sub_86C8F0, = Drain 0x856900): stage wobble out state 0 - amplitude +0x1C =
	// 0x400, next state
	static uint32_t __cdecl WobbleOutInit(uint32_t a1)
	{
		const uint8_t st = U8(a1, 0x29);
		U16(a1, 0x1C) = 0x400;
		U8(a1, 0x29) = (uint8_t)(st + 1);
		return a1;
	}

	// 0x86C910 (MAG_032_sub_86C910, = Drain 0x856920): stage wobble out state 1 - amplitude +0x1C
	// down by 0x100 per tick to 0 (then finished, next state), written to the four stage wobble
	// words 0x1D98992 + k * 0x2C
	static uint32_t __cdecl WobbleOut(uint32_t a1)
	{
		U16(a1, 0x1C) = (uint16_t)(U16(a1, 0x1C) + 0xFF00);
		if (S16(a1, 0x1C) <= 0)
		{
			const uint8_t st = U8(a1, 0x29);
			U8(a1, 0x26) |= 1;
			U16(a1, 0x1C) = 0;
			U8(a1, 0x29) = (uint8_t)(st + 1);
		}
		const uint16_t level = U16(a1, 0x1C);
		uint32_t p = 0x1D98992;
		for (int i = 4; i != 0; i--, p += 0x2C)
			MEM<uint16_t>(p) = level;
		return 0; // void
	}

	struct ModPort { uint32_t addr; void *port; const char *name; };
	// ---- generated (gendesc.py): the module's functions served by the engine's ports (identical
	// code up to module addresses) ----
	static const ModPort ENGINE_PORTS[] = {
		{ 0x86BC50, (void *)a_73A0D0, "032 MAG_032_sub_86BC50" },
		{ 0x86BC60, (void *)a_73A0D0, "032 MAG_032_sub_86BC60" },
		{ 0x86BCA0, (void *)a_73A380, "032 MAG_032_sub_86BCA0" },
		{ 0x86BD00, (void *)a_7335D0, "032 MAG_032_sub_86BD00" },
		{ 0x86BD80, (void *)a_73A6D0, "032 nullsub_1662" },
		{ 0x86BD90, (void *)a_73A170, "032 MAG_032_sub_86BD90" },
		{ 0x86C000, (void *)a_73A380, "032 MAG_032_sub_86C000" },
		{ 0x86C390, (void *)a_73A6D0, "032 nullsub_1665" },
		{ 0x86C760, (void *)a_743C20, "032 sub_86C760" },
		{ 0x86C790, (void *)a_73A6D0, "032 nullsub_1666" },
		{ 0x86C7A0, (void *)a_73A6D0, "032 nullsub_1667" },
		{ 0x86C810, (void *)a_73A6D0, "032 nullsub_1663" },
		{ 0x86C890, (void *)a_73A380, "032 MAG_032_sub_86C890" },
		{ 0x86C950, (void *)a_73A6D0, "032 nullsub_1668" },
		{ 0x86C960, (void *)a_747550, "032 MAG_032_sub_86C960" },
		{ 0x86C970, (void *)a_73A0D0, "032 MAG_032_sub_86C970" },
		{ 0x86C980, (void *)a_73A0D0, "032 MAG_032_sub_86C980" },
		{ 0x86C990, (void *)a_7475A0, "032 MAG_032_sub_86C990" },
		{ 0x86C9B0, (void *)a_73A6D0, "032 nullsub_1664" },
		{ 0, nullptr, nullptr }
	};
	// the module's own ports
	static const ModPort PORTS[] = {
		{ ORIG_Master, (void *)Master, "032 Protect Master" },
		{ 0x86BC70, (void *)StartWobbleIn, "032 StartWobbleIn" },
		{ 0x86BD40, (void *)WobbleIn, "032 WobbleIn" },
		{ ORIG_Emitter, (void *)Emitter, "032 Emitter" },
		{ ORIG_Flash, (void *)Flash, "032 Flash" },
		{ 0x86BF70, (void *)SpawnShieldAndParticles, "032 SpawnShieldAndParticles" },
		{ 0x86C060, (void *)ShieldInit, "032 ShieldInit" },
		{ 0x86C0C0, (void *)RenderShield, "032 RenderShield" },
		{ ORIG_ShieldDraw, (void *)ShieldDraw, "032 ShieldDraw" },
		{ ORIG_Spawner, (void *)Spawner, "032 Spawner" },
		{ 0x86C400, (void *)SpawnerInit, "032 SpawnerInit" },
		{ 0x86C480, (void *)SpawnerWait, "032 SpawnerWait" },
		{ 0x86C490, (void *)SpawnerEmit, "032 SpawnerEmit" },
		{ ORIG_Particle, (void *)Particle, "032 Particle" },
		{ 0x86C690, (void *)ParticleInit, "032 ParticleInit" },
		{ 0x86C6B0, (void *)ParticleMove, "032 ParticleMove" },
		{ 0x86C7B0, (void *)ReleaseMaster, "032 ReleaseMaster" },
		{ 0x86C7D0, (void *)ActionResult, "032 ActionResult" },
		{ 0x86C820, (void *)ActionLoop, "032 ActionLoop" },
		{ 0x86C860, (void *)StartWobbleOut, "032 StartWobbleOut" },
		{ 0x86C8F0, (void *)WobbleOutInit, "032 WobbleOutInit" },
		{ 0x86C910, (void *)WobbleOut, "032 WobbleOut" },
		{ 0, nullptr, nullptr }
	};
}
}

	void register_mag032_protect()
	{
		act::register_module(32);
		for (const act::protect::ModPort *p = act::protect::ENGINE_PORTS; p->addr; p++)
			act::register_module_port(32, p->addr, p->port, p->name);
		for (const act::protect::ModPort *p = act::protect::PORTS; p->addr; p++)
			act::register_module_port(32, p->addr, p->port, p->name);
		// 30 fps layer: see mag032_protect_held.inc
		FX_HELD(register_mag032_held();)
	}
}

#ifdef FF8_FX_HELD
#include "mag032_protect_held.inc"
#endif
