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

// The heal-ring module's code, once for every copy of it: Cure (effect 1, mag001_cure.cpp) and its
// relocated copy Potion (effect 50, mag050_potion.cpp) are the same instructions over their own
// memory and exe data. A copy is a Layout (where its original functions, data and memory are);
// HealRing<layout> is its port, built from the parts (fx_parts.h: inputs, behaviours, renderers)
// and the projector overlay (fx_overlay.h), with its parameters (mag_heal_ring.h).
//
// Structure: file loader (the texture file) and setup (the queues, the root node, the packet
// arenas inside the texture file's buffer, the camera track and the TIM upload), both called from
// the game's dispatch slots (fx_dispatch.cpp points them at the ports). Every node starts with the
// shared effect-library header (parts::FxNode); every task runs its current phase and ends when it
// is flagged done and has no live children.
//   Root - copies the camera into the shared snapshot 0x2793E58, alternates the packet arena
//     (texture file + 0 / + 0x10000), follows the caster's bones; phases: 0..2 wait for the
//     children, 3 spawns the emitter of the current action (marks the root busy), 4 waits while
//     busy, then moves to the next action (back to 3) until the last one, 6 waits until the three
//     queues (emitters, particles, glints) are empty, 9 done. Runs the three queues every tick.
//   Emitter - on the action's target: target anchor and model bounds every tick; spawns the ring
//     (after ring.start.delay), the sound at sound_tick, the heal (ApplyActionResultToTarget
//     0x506690) from heal_tick, then done.
//   Ring - a light rising in a helix around the target (parts::HelixPlace, per_tick steps a tick);
//     while spawning (flag 2) a sparkle per step, motes and glints per tick; while flag 8 it
//     redraws the target with a projected light texture (the overlay: the target is hidden, entity
//     flag 4); draws its flipbook and, for glow.brightness.ticks, an additive ground glow fading
//     out. Phase 1 switches its flipbook at ring.ending.tick, phase 2 runs it to its end, then
//     unhides the target, clears the root's busy flag and ends.
//   Mote / Sparkle - flipbooks moving at a random constant velocity (motes rise, sparkles fly out)
//     until their last frame.
//   Glints - the shared effect-library task Effect_Glint_Tick 0x8DCC20 (fx_glint.cpp).
// No task tests the draw-only flags (battle_to_update_flags 0x201): everything updates always.
//
// The 30 fps layer of the True30FPS branch notes what was drawn (FX_HELD lines below); its
// per-module memos (mag001_cure_held.inc, mag050_potion_held.inc there) follow this template.

#pragma once

#include <string>

#include "mag_common.h"
#include "mag_heal_ring.h"
#include "fx_parts.h"
#include "fx_overlay.h"

namespace ff8fx
{
namespace healring
{
	using namespace eng;
	using namespace magc;
	using namespace parts;
	using overlay::OverlayCtx;

	// where one copy of the module is in FF8_EN.exe
	struct Layout
	{
		// its original functions (the ports replace them)
		uint32_t file_loader, setup, root, emitter, ring, mote, sparkle;
		// its data: the texture file's name, the camera track, the sound block, the flipbooks
		uint32_t texture_name, camera, sound, seq_ring, seq_ring_end, seq_glow, seq_sparkle, seq_mote;
		// its memory
		uint32_t texture_file, packet_cursor, packet_limit;
		uint32_t arena_a, limit_a, arena_b, limit_b, arena_top;          // packet arenas (texture file + 0 / 0x10000 / 0x20000)
		uint32_t init_flag_a, init_flag_b, cam_ptr_a, cam_ptr_b;         // cleared by the setup; the camera snapshot's address
		uint32_t root_queue, root_pool;                                  // 2 x 0x64
		uint32_t emitter_queue, emitter_pool;                            // 4 x 0x58
		uint32_t particle_queue, particle_pool;                          // 300 x 0x6C (ring, motes, sparkles)
		uint32_t glint_queue, glint_pool;                                // 100 x 0x5C
		uint32_t overlay_ctx, overlay_a00, overlay_sprite, overlay_verts; // the projector overlay's context, descriptor, vertex buffer
	};

#pragma pack(push, 1)
	struct RootNode // pool of 2 nodes of 0x64 bytes
	{
		FxNode h;
		uint8_t bones[0x28];   // +0x30 caster bone anchor (Effect_UpdateTargetPosFromBones)
		int16_t last_action;   // +0x58
		int16_t target_count;  // +0x5A
		int16_t tick;          // +0x5C arena parity
		int16_t alive;         // +0x5E sum of the queue results
		uint8_t pad60[3];
		uint8_t busy;          // +0x63 a ring is running
	};
	struct EmitterNode // pool of 4 nodes of 0x58 bytes
	{
		FxNode h;
		uint8_t anchor[0x18];  // +0x30 target bone anchor (Emitter_UpdatePos)
		int16_t bmin[4];       // +0x48 model bounds (Emitter_ComputeModelBounds)
		int16_t bmax[4];       // +0x50
	};
	struct Particle // ring / mote / sparkle: pool of 300 nodes of 0x6C bytes
	{
		FxNode h;
		uint8_t pad30[0x12];
		int16_t angle;         // +0x42 ring: angle of the last step
		uint8_t pad44[8];
		uint32_t seq;          // +0x4C flipbook
		int16_t frame;         // +0x50
		int16_t last;          // +0x52 last frame
		int16_t shade;         // +0x54 TransformCameraByShadowRotation offset (0)
		int16_t pad56;
		int16_t vel[3];        // +0x58 mote / sparkle velocity; ring: vel[1] = climb per step
		int16_t pad5E;
		int16_t center[4];     // +0x60 ring: centre (x, top of the target's bounds, z)
		int16_t radius;        // +0x68 ring radius
		int16_t pad6A;
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x64 && sizeof(EmitterNode) == 0x58 && sizeof(Particle) == 0x6C, "heal ring nodes");

	static const uint32_t CAM_SNAPSHOT = 0x2793E58;       // Effect_SharedCamMatrixSnapshot

	template<const Layout &L>
	struct HealRing
	{
		// the game's values (with this copy's own flipbooks and sound) and the live ones the code reads
		inline static const HealRingParams GAME = game_values({ L.seq_ring, L.seq_ring_end, L.seq_glow, L.seq_sparkle, L.seq_mote, L.sound });
		inline static HealRingParams live = game_values({ L.seq_ring, L.seq_ring_end, L.seq_glow, L.seq_sparkle, L.seq_mote, L.sound });
		static const HealRingParams &P() { return live; }

		static uint32_t &PacketCursor() { return var<uint32_t>(L.packet_cursor); }
		static uint32_t &PacketLimit() { return var<uint32_t>(L.packet_limit); }
		static TaskQueue *QRoot() { return (TaskQueue *)L.root_queue; }
		static TaskQueue *QEmitters() { return (TaskQueue *)L.emitter_queue; }
		static TaskQueue *QParticles() { return (TaskQueue *)L.particle_queue; }
		static TaskQueue *QGlints() { return (TaskQueue *)L.glint_queue; }
		static OverlayCtx &Overlay() { return var<OverlayCtx>(L.overlay_ctx); }

		// ------------------------------------------------------------------
		// Drawing (the parts' renderers on this module's packet cursor)
		// ------------------------------------------------------------------
		static void DrawSprite(const Particle *p, const fxp::Colour &colour, int16_t size)
		{
			parts::DrawSprite(p->h.pos, p->seq, p->frame, p->shade, (p->h.flags & 4) != 0, colour, size, PacketCursor());
		}

		// ------------------------------------------------------------------
		// Particles
		// ------------------------------------------------------------------
		// mote / sparkle phase 1: position += velocity, done at the last frame
		static void Integrate(Particle *p)
		{
			Drift(p->h.pos, p->vel);
			if (FlipbookExpired(p->frame, p->last, p->h.flags))
			{
				*(uint8_t *)&p->h.flags |= 1;
				p->h.phase++;
			}
		}

		// the ring's place at step `step` (per_tick x counter + s)
		static void RingPlace(Particle *r, uint32_t step)
		{
			HelixPlace(r->h.pos, r->angle, r->center, r->vel[1], r->radius, step, P().ring.steps.angle);
		}

		// Ring_SpawnSparkle (Cure 0x8D8080): one per step while the counter is in the window
		static void Ring_SpawnSparkle(Particle *r)
		{
			if (P().sparkle.window.has(r->h.counter)) AddEffectTask(QParticles(), L.sparkle, 0x6C, r);
		}

		// Ring_SpawnMotes (Cure 0x8D7E20): count.at(counter) motes on a circle of radius x the
		// target's radius at a random angle, at the ring's height lowered by a random share of the
		// ring's height above the target's top
		static void Ring_SpawnMotes(Particle *r)
		{
			const HealRingParams::Mote &mp = P().mote;
			const int16_t c = r->h.counter;
			if (!mp.window.has(c)) return;
			for (int32_t n = mp.count.at(c); n > 0; n--)
			{
				const int32_t a = mp.angle.roll(CrtRand());
				Particle *m = (Particle *)AddEffectTask(QParticles(), L.mote, 0x6C, r);
				CopyAnchorXZFromSource(m, m->h.pos);
				m->h.pos[1] = r->h.pos[1];
				int32_t d = (int32_t)r->h.pos[1] - (int32_t)(int16_t)BoundsMaxWorldY(r);
				d = mul32(d, mp.drop.roll(CrtRand()));
				// d / -48 (compiled 0xD5555555, >> 3: truncated toward zero as C does)
				m->h.pos[1] = (int16_t)(m->h.pos[1] + d / -mp.drop.den);
				m->radius = mp.radius.of((int16_t)BoundsRadius(r));
				FX_NOTE("mote circle radius", m->radius);
				FX_NOTE("mote drop", d / -mp.drop.den);
				OnCircle(m->h.pos, m->radius, a);
				memcpy(m->vel, r->vel, 8);
			}
		}

		// Ring_SpawnGlints (Cure 0x8D7D50): count.at(counter) glints drawing into the module cursor
		static void Ring_SpawnGlints(Particle *r)
		{
			const HealRingParams::Glint &gp = P().glint;
			const int16_t c = r->h.counter;
			if (!gp.window.has(c)) return;
			for (int32_t n = gp.count.at(c); n > 0; n--) SpawnGlint(QGlints(), r, L.packet_cursor, gp.style);
		}

		// ring phase 0 (Ring_State0_Init, Cure 0x8D81B0): spawning on; centre on the line's anchor
		// at the target's feet minus start.above; radius of the target's radius; climb of the
		// target's height per step; the overlay on the target, which the normal renderer then skips
		// (entity flag 4)
		static void Ring_State0_Init(Particle *r)
		{
			*(uint8_t *)&r->h.flags |= 2;
			r->seq = fxp::sequence(P().ring.seq, L.seq_ring);
			uint8_t *entity = Entity(r->h.slot);
			CopyAnchorXZFromSource(r, r->center);
			r->center[1] = (int16_t)((int16_t)BoundsMaxWorldY(r) - P().ring.start.above);     // y down
			const HealRingParams::Ring &rp = P().ring;
			r->radius = rp.radius.of(BoundsRadius(r));
			FX_NOTE("target radius", BoundsRadius(r));
			FX_NOTE("target height", (int16_t)BoundsHeight(r));
			FX_NOTE("ring radius", r->radius);
			const int32_t hv = (int32_t)(int16_t)BoundsHeight(r) + rp.climb.above;
			// hv / -20 (compiled 0x99999999, >> 3: truncated toward zero), per place
			const int32_t q = hv / -rp.climb.ticks;
			r->vel[1] = (int16_t)((int32_t)(int16_t)q / rp.steps.per_tick);
			FX_NOTE("ring rise per step", -r->vel[1]);
			FX_NOTE("ring start above feet", rp.start.above);
			overlay::SetupTargetOverlay(&Overlay(), L.overlay_a00, (uint8_t *)L.overlay_sprite, L.overlay_verts, entity, r->h.slot,
			                            &PacketCursor(), P().overlay.rgb, P().overlay.light_dist);
			*(uint8_t *)&r->h.flags |= 8;
			entity[0] |= 4;
			r->h.phase++;
		}

		// ring phase 2 (Cure 0x8D83A0): at the flipbook's end: the root is free again, the target
		// shown, spawning and overlay off, done
		static void Ring_State2_EndAndUnblockDirector(Particle *r)
		{
			if (!FlipbookExpired(r->frame, r->last, r->h.flags)) return;
			((RootNode *)r->h.root)->busy = 0;
			*(uint16_t *)Entity(r->h.slot) &= 0xFFFB;
			uint16_t f = r->h.flags;
			f = (uint16_t)((f & 0xFF00) | (uint8_t)(((uint8_t)f & 0xF5) | 1));
			r->h.flags = f;
			r->h.phase++;
		}

		// ------------------------------------------------------------------
		// Sparkle / Mote
		// ------------------------------------------------------------------
		static uint32_t __cdecl SparkleTask(TaskNode *n)
		{
			Particle *p = (Particle *)n;
			switch (p->h.phase)
			{
			case 0: // random velocity, flying out and up
				p->seq = fxp::sequence(P().sparkle.seq, L.seq_sparkle);
				p->last = fxp::last_frame(p->seq, P().sparkle.frames);
				RollVelocity(p->vel, P().sparkle.speed);
				p->h.phase++;
				break;
			case 1: Integrate(p); break;
			default: break; // phase 2: idle
			}
			FX_HELD(held_note_particle(p);)
			FX_PART(PART_SPARKLES);
			DrawSprite(p, P().sparkle.colour, P().sparkle.size);
			p->h.counter++;
			return EndWhenDone(&p->h);
		}

		static uint32_t __cdecl MoteTask(TaskNode *n)
		{
			Particle *p = (Particle *)n;
			switch (p->h.phase)
			{
			case 0: // rising at a random speed
				p->seq = fxp::sequence(P().mote.seq, L.seq_mote);
				p->last = fxp::last_frame(p->seq, P().mote.frames);
				p->vel[1] = (int16_t)-P().mote.rise.roll(CrtRand());
				p->vel[0] = 0;
				p->vel[2] = 0;
				p->h.phase++;
				break;
			case 1: Integrate(p); break;
			default: break; // phase 2: idle
			}
			FX_HELD(held_note_particle(p);)
			FX_PART(PART_MOTES);
			DrawSprite(p, P().mote.colour, P().mote.size);
			p->h.counter++;
			return EndWhenDone(&p->h);
		}

		// ------------------------------------------------------------------
		// Ring
		// ------------------------------------------------------------------
		static uint32_t __cdecl RingTask(TaskNode *n)
		{
			Particle *r = (Particle *)n;
			switch (r->h.phase)
			{
			case 0: Ring_State0_Init(r); break;
			case 1: // at ending.tick the ending flipbook
				if (r->h.counter >= P().ring.ending.tick)
				{
					r->seq = fxp::sequence(P().ring.ending.seq, L.seq_ring_end);
					r->last = fxp::last_frame(r->seq, P().ring.ending.frames);
					r->h.phase++;
				}
				break;
			case 2: Ring_State2_EndAndUnblockDirector(r); break;
			default: break; // phase 3: idle
			}
			const int32_t substeps = P().ring.steps.per_tick;
			for (uint32_t s = 0; s < (uint32_t)substeps; s++)
			{
				RingPlace(r, (uint32_t)(uint16_t)(r->h.counter * substeps) + s);
				if (r->h.flags & 2) Ring_SpawnSparkle(r);
			}
			if (r->h.flags & 2)
			{
				Ring_SpawnMotes(r);
				Ring_SpawnGlints(r);
			}
			FX_HELD(held_note_ring(r);)
			if (r->h.flags & 8)
			{
				const HealRingParams::Overlay &op = P().overlay;
				FX_PART(PART_OVERLAY);
				Overlay().offset[1] = r->h.pos[1];
				overlay::SetOverlayTexturePage(&Overlay(), r->h.pos, op.image.tex_x, op.image.tex_y, op.image.clut_x, op.image.clut_y, op.image.width, op.image.height);
				overlay::DrawTargetModelWithOverlay(&Overlay(), op.skip_back_faces != 0);
			}
			if (r->h.counter < P().glow.brightness.ticks)
			{
				// the 4th byte is the high byte of the task argument (the node address) the colour is
				// written over on the original's stack
				const uint32_t colour = FadingGrey(P().glow.brightness.start, P().glow.brightness.fade, r->h.counter, (uint32_t)r & 0xFF000000u);
				FX_PART(PART_RING);
				DrawSprite(r, P().ring.colour, P().ring.size);
				FX_PART(PART_GLOW);
				const HealRingParams::Glow::Placement &gp = P().glow.placement;
				DrawGroundFlipbook(r->h.pos, (r->h.flags & 4) != 0, fxp::sequence(P().glow.seq, L.seq_glow), gp.tilt, gp.height, gp.frame,
				                   colour, PacketCursor());
			}
			else
			{
				FX_PART(PART_RING);
				DrawSprite(r, P().ring.colour, P().ring.size);
			}
			r->h.counter++;
			return EndWhenDone(&r->h);
		}

		// ------------------------------------------------------------------
		// Emitter
		// ------------------------------------------------------------------
		static uint32_t __cdecl EmitterTask(TaskNode *n)
		{
			EmitterNode *e = (EmitterNode *)n;
			Emitter_UpdatePos(e);
			Emitter_ComputeModelBounds(e);
			switch (e->h.phase)
			{
			case 0: // the ring, its start delayed
				if (e->h.counter < P().ring.start.delay) break;
				AddEffectTask(QParticles(), L.ring, 0x6C, e);
				e->h.phase++;
				break;
			case 1: e->h.phase++; break;
			case 2: // the heal
				if (e->h.counter >= P().emitter.heal_tick)
				{
					ApplyActionResultToTarget(e->h.ctx->actions[e->h.action].targets + e->h.target * TARGET_STRIDE);
					*(uint8_t *)&e->h.flags |= 1;
					e->h.phase++;
				}
				break;
			default: break; // phase 3: idle
			}
			if (e->h.counter == P().emitter.sound_tick) BdPlaySE((const void *)fxp::sound(P().emitter.sound, L.sound), 0, P().emitter.sound_pan);
			e->h.counter++;
			return EndWhenDone(&e->h);
		}

		// ------------------------------------------------------------------
		// Root
		// ------------------------------------------------------------------
		static void RootPhase(RootNode *r)
		{
			switch (r->h.phase)
			{
			case 0:
			case 1:
				r->h.phase++;
				break;
			case 2: // children gone
				if (r->h.children == 0) r->h.phase++;
				break;
			case 3: // the emitter of the current action
				r->busy = 1;
				AddEffectTask(QEmitters(), L.emitter, 0x58, r);
				r->h.phase++;
				break;
			case 4: // when the ring is over, the next action (back to 3) or on
				if (r->busy == 0)
				{
					const int8_t a = r->h.action;
					if ((int16_t)a < r->last_action)
					{
						r->h.action = (int8_t)(a + 1);
						r->h.line++;
						r->h.phase--;
					}
					else r->h.phase++;
				}
				break;
			case 5:
				r->h.phase++;
				break;
			case 6: // every queue empty
				if (r->alive == 0) r->h.phase++;
				break;
			case 7:
			case 8:
				r->h.phase++;
				break;
			case 9: // done
				*(uint8_t *)&r->h.flags |= 1;
				r->h.phase++;
				break;
			default: break; // phase 10: idle
			}
		}

		static uint32_t __cdecl RootTask(TaskNode *n)
		{
			RootNode *r = (RootNode *)n;
			memcpy((void *)CAM_SNAPSHOT, &Camera(), 0x20);
			var<uint32_t>(L.cam_ptr_a) = CAM_SNAPSHOT;
			var<uint32_t>(L.cam_ptr_b) = CAM_SNAPSHOT;
			if (r->tick & 1)
			{
				PacketCursor() = var<uint32_t>(L.arena_a);
				PacketLimit() = var<uint32_t>(L.limit_a);
			}
			else
			{
				PacketCursor() = var<uint32_t>(L.arena_b);
				PacketLimit() = var<uint32_t>(L.limit_b);
			}
			FX_HELD(held_note_root(r);)
			UpdateTargetPosFromBones(r);
			RootPhase(r);
			r->alive = 0;
			r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QEmitters()));
			r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QParticles()));
			FX_PART(PART_GLINTS);                 // the shared glint task draws them
			r->alive = (int16_t)(r->alive + ExecuteTaskQueue(QGlints()));
			FX_PART(0);
			r->tick++;
			r->h.counter++;
			return EndWhenDone(&r->h);
		}

		// ------------------------------------------------------------------
		// File loader and setup
		// ------------------------------------------------------------------
		// the texture file (files.texture, else the game's name) read into the module's buffer
		static uint32_t __cdecl FileLoader()
		{
			const uint32_t file = IO_GetFile_MAGIC(fxp::file(P().files.texture, (const char *)L.texture_name));
			var<uint32_t>(L.texture_file) = file;
			return file;
		}

		// the root queue with its director (the target loop: the last action is the larger of target
		// count - 1 and action count - 1), the camera track and the texture upload (unless the cast
		// asks for neither), the packet arenas inside the texture file's buffer (two of 0x10000
		// bytes, the root alternates them), the three other queues
		static TaskQueue *__cdecl Setup(CastContext *ctx)
		{
			var<uint32_t>(L.init_flag_a) = 0;
			var<uint32_t>(L.init_flag_b) = 0;
			InitTaskQueuePool(QRoot(), (void *)L.root_pool, 0x64, 2);
			RootNode *r = (RootNode *)AddEffectTask(QRoot(), L.root, 0x64, nullptr);
			const ActionData *a = ctx->actions + r->h.action;
			r->h.ctx = ctx;
			r->h.attacker = a->attacker;
			r->h.phase = 0;
			r->h.slot = a->targets[TARGET_STRIDE * r->h.target];
			r->target_count = ctx->actions[0].target_count;
			r->last_action = ctx->actions[0].last_action;
			r->h.last = (uint8_t)(r->target_count - 1);
			if ((int16_t)r->h.last < r->last_action) r->h.last = (uint8_t)r->last_action;
			if (!(ctx->flags & 1))
			{
				PlayCameraAnimation((const void *)L.camera);
				QueueTimUpload(var<uint32_t>(L.texture_file));
			}
			const uint32_t file = var<uint32_t>(L.texture_file);
			var<uint32_t>(L.arena_a) = file;
			var<uint32_t>(L.limit_a) = file + 0x10000;
			var<uint32_t>(L.arena_b) = file + 0x10000;
			var<uint32_t>(L.arena_top) = file + 0x20000;
			var<uint32_t>(L.limit_b) = file + 0x20000;
			InitTaskQueuePool(QEmitters(), (void *)L.emitter_pool, 0x58, 4);
			InitTaskQueuePool(QParticles(), (void *)L.particle_pool, 0x6C, 300);
			InitTaskQueuePool(QGlints(), (void *)L.glint_pool, 0x5C, 100);
			return QRoot();
		}

		// its parameters and its ports under `effect_id`, the ports named "<tag> Root"...
		static void register_module(int effect_id, const char *name, const char *tag)
		{
			static std::string names[7];
			const char *what[7] = { "File loader", "Setup", "Root", "Emitter", "Ring", "Mote", "Sparkle" };
			for (int k = 0; k < 7; k++) names[k] = std::string(tag) + " " + what[k];
			register_params(effect_id, name, &live, &GAME);
			register_port(L.file_loader, (void *)FileLoader, names[0].c_str(), effect_id);
			register_port(L.setup, (void *)Setup, names[1].c_str(), effect_id);
			register_port(L.root, (void *)RootTask, names[2].c_str(), effect_id);
			register_port(L.emitter, (void *)EmitterTask, names[3].c_str(), effect_id);
			register_port(L.ring, (void *)RingTask, names[4].c_str(), effect_id);
			register_port(L.mote, (void *)MoteTask, names[5].c_str(), effect_id);
			register_port(L.sparkle, (void *)SparkleTask, names[6].c_str(), effect_id);
		}
	};
}
}
