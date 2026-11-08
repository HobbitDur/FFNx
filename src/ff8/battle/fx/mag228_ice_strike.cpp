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

// Effect 228: Ice Strike (Edea's limit break while she is a party member, disc 1; kernel.bin
// section 18 "temporary character limit breaks"; no enemy attack uses effect 228), cinematic-engine
// module MAG_228 0xA85590..0xA8F860, script data magic/mag227_b.00/.01 (loaded by MAG_228_UNKNOWN_FL
// 0xA85590; setup MAG_228_UNKNOWN 0xA855C0 = effect table index 227).
//
// The module is one more compile of the GF cinematic engine (gfc_engine.cpp) with the standard
// table layout (draw table 0x186B758; its descriptor block 0x186B438 sits 0x320 below the draw
// table: the engine reads it through c.desc). Instruction-level comparison against the ported
// clones (module-relative addresses normalised, table regions symbolic, jump tables and callee
// trees hashed; work\gfcB\tools\cmpall.py / best.py / fdiff.py):
//   - SetupSummon 0xA85620, BindContext 0xA85670, the section pointers 0xA85710, InitSequenceState
//     0xA8EE70, SequenceTick 0xA85C30 (the unlit BuildMatricesAndDraw) and the initial frame limit
//     0xA8F840 are byte clones of Sand Shake 256's (0x979460 / 0x9794B0 / 0x979550 / 0x981C30 /
//     0x979A70 / 0x982600); lit_dispatcher false;
//   - every table handler is a byte clone of Massive Anchor 263's slot (the generic (Ifrit) handler
//     or a `ret` stub; draw 70 is a stub here), except
//       VM 0x006 LoadBattleFile 0xA8B100: same code, own load callback 0xA8B230 (c.load_cb),
//       VM 0x0AB 0xA86690 BindLimitAnimSet: Massive Anchor's 0x9468E0 / Desperado's 0x95ABB0 with
//         this module's animation set (com data 0x18821A0): static copy below,
//       VM 0x0AD 0xA8EA50 PosFromPosedEntityJoint = Homing Laser 229's 0xA84780 / Twin Homing Laser
//         253's 0x9A1A30 (static there: same code below), with its 30 fps prediction at the
//         entity's next pose,
//       VM 0x039 0xA8ADF0 CameraAndSound: the engine's 0xB2B750 (gfc_engine.cpp op_039) but for sub 5
//         (case 4 of the jump table 0xA8B01C, 16 instructions at 0xA8AF1D): instead of
//         sub_504270(1) it sets the effect light block 0x1D99AB0 (the 0x20-byte matrix Moomba / Doom
//         also use): CopyMatrix (0xB656A0) of the billboard matrix 0 (0x2797968), then its
//         translation (+0x14 / +0x18 / +0x1C, s32) = the bone's outPos. Own copy below (sub 6 / 7
//         still call sub_504270(1) / (0)).
// The effect's queue (0x2796B38, pool 0x2796B50) holds one task, the engine tick 0xA85C30.
// Writes outside the engine state: battle entity (the caster): its animation set / chain (VM 0x0AB),
// its bone matrices (VM 0x0AD: those the battle renderer builds anyway); the effect light block
// 0x1D99AB0 (VM 0x039 sub 5); camera: VM 0x039 (engine words, HeldCamera).

#include "gfc_engine.h"

namespace ff8fx
{
namespace gfc
{
namespace icestrike228
{
	static Clone g_c = {};
}
}
}

#ifdef FF8_FX_HELD
#include "mag228_ice_strike_held.h"
#endif

namespace ff8fx
{
namespace gfc
{
namespace icestrike228
{
	static void describe(Clone &c)
	{
		c.name = "228 Ice Strike";
		c.effect_id = 228;
		c.vm_table = 0x186B8FC;      // MAG_228 VmOpcodeTable (draw_table + 0x1A4)
		c.bone_table = 0x186B56C;    // MAG_228 BoneHandlerTable (draw_table - 0x1EC)
		c.prim_table = 0x186B5B4;    // bone_table + 0x48
		c.prim_size = 0x186B6A4;     // bone_table + 0x138
		c.spr_table = 0x186B6B8;     // bone_table + 0x14C
		c.ptype_table = 0x186B6E4;   // bone_table + 0x178
		c.pop_table = 0x186B6F8;     // bone_table + 0x18C
		c.draw_table = 0x186B758;    // MAG_228 DrawHandlerTable
		c.attr_16A = 0x186B8C2;
		c.attr_184 = 0x186B8DC;
		c.attr_186 = 0x186B8DE;
		c.attr_194 = 0x186B8EC;
		c.attr_19C = 0x186B8F4;
		c.desc = 0x186B438;          // draw_table - 0x320 (read by 0xA85710)
		c.ptr_block = 0x186B544;     // draw_table - 0x214 (read by 0xA85670)
		c.queue = 0x2796B38;
		c.file00 = 0x2796B4C;
		c.file01 = 0x2796B48;
		c.tick = 0xA85C30;
		c.load_cb = 0xA8B230;        // passed by VM 0x006 0xA8B100
		c.lit_dispatcher = false;
		c.bone_count = 18;
	}

	static const uint32_t COMDATA = 0x18821A0;   // the module's animation set (VM 0x0AB)
	static const uint32_t EFFECT_LIGHT = 0x1D99AB0;   // effect light block (matrix, 0x20 bytes)

	template<typename F> static inline F fx(uint32_t a) { return (F)a; }
	// battle entity side effects (30 fps layer: see gfc_engine_held.h)
	static inline void xl_BindEntityModelFromComData(void *ent, void *out, uint32_t data) { FX_HELD(if (held_predicting()) return;) fx<void (__cdecl *)(void *, void *, uint32_t)>(0x5022C0)(ent, out, data); }
	static inline void xl_QueueChainTransformation(void *ent, int32_t id) { FX_HELD(if (held_predicting()) return;) fx<void (__cdecl *)(void *, int32_t)>(0x505C00)(ent, id); }
	// 0xB66B80 (blob): matrix of joint `joint` of the model whose BattleAnimHeader is `hdr`,
	// composed with m; joint origin -> ws+0xF0..0xF8. Returns ws+0xD0. Writes only ws + GTE.
	static inline uint8_t *xl_JointMatrix_B66B80(void *hdr, int32_t joint, int32_t scale, void *m)
	{
		return x::f<uint8_t *(__cdecl *)(void *, int32_t, int32_t, void *)>(0xB66B80)(hdr, joint, scale, m);
	}

	static inline uint8_t *SlotEntity() { return PTR(SCENE(), 0x60 + 4 * (int32_t)U8(CUR(), 0x1B)); }
	static inline uint32_t &CamWord32(int i) { return MEM<uint32_t>(0xB8B7F0 + 4 * i); } // battle camera block 0xB8B7F0..0xB8B80F as dwords
	static inline uint16_t &Word1D977A0() { return MEM<uint16_t>(0x1D977A0); }
	static inline uint16_t &Word1D977A2() { return MEM<uint16_t>(0x1D977A2); }
	static inline uint16_t &Word1D9771C() { return MEM<uint16_t>(0x1D9771C); }

	// ------------------------------------------------------------------------------------
	// 0xA86690 (VM 0x0AB BindLimitAnimSet, = Massive Anchor 263's 0x9468E0 with COMDATA): the slot
	// entity's model bound to the module's animation set (0x5022C0, 0x20-byte arena block), its
	// chain transformation w0 queued (0x505C00). Length 4.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AB_BindLimitAnimSet()
	{
		uint8_t *ent = SlotEntity();                                          // esi
		uint8_t *blk = blob::ArenaAlloc(0x20);
		xl_BindEntityModelFromComData(ent, blk, COMDATA);
		xl_QueueChainTransformation(ent, S16(STREAM(), 2));
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// 0xA8EA50 (VM 0x0AD PosFromPosedEntityJoint, = Homing Laser 229's 0xA84780): the slot
	// entity's bone matrices rebuilt from its pose (header entity+0x60, or *(entity+0x78) when op
	// bit 15 is set), joint w0 of that model (blob 0xB66B80, composed with the entity matrix +0x40)
	// applied to outPos -> accumPos = the camera-space point << 16. Then the bone handler. Length 4.
	// 30 fps: while the held layer predicts the next tick, the joint comes from the entity's NEXT
	// pose (see mag228_ice_strike_held.inc).
	// ------------------------------------------------------------------------------------
	static void __cdecl op_0AD_PosFromPosedEntityJoint()
	{
		uint8_t *ent = SlotEntity();                                          // edi
		const bool weapon = (U8(RT(), 0x4B) & 0x80) != 0;
		uint8_t *hdr = weapon ? PTR(ent, 0x78) : ent + 0x60;                  // esi
		x::BuildBoneMatricesFromPose(hdr);
		// 30 fps layer: see mag228_ice_strike_held.inc
		FX_HELD(if (held_predicting()) held_next_pose(hdr, weapon ? hdr + 0xC : ent + 0x6C);)
		uint8_t *m = xl_JointMatrix_B66B80(hdr, S16(STREAM(), 2), 0x1000, ent + 0x40);
		x::SetRotMatrix(m);
		x::SetTransVector(m);
		uint8_t *w = WS();
		x::TransformToCamera(CUR() + 0x94, w + 0xF0, w + 0xFC);
		S32(CUR(), 0x5C) = shl32(S32(WS(), 0xF0), 16);
		S32(CUR(), 0x60) = shl32(S32(WS(), 0xF4), 16);
		S32(CUR(), 0x64) = shl32(S32(WS(), 0xF8), 16);
		C().bone[U8(CUR(), 0x18)]();
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// 0xA8ADF0 (VM 0x039 CameraAndSound): sub = op>>12 (jump table 0xA8B01C): as the engine's
	// 0xB2B750 (gfc_engine.cpp op_039_CameraAndSound) but sub 5: the effect light block 0x1D99AB0
	// = billboard matrix 0 (CopyMatrix 0xB656A0, 0x20 bytes), translation = outPos (s16 -> s32).
	// 1 Battle_PlayCameraAnimation(.00 + U32(.00,0x20)); 2 SceneHeader+0x168[lo w0] =
	// ClaimVoiceSlot(.00 + U32(.00,0x24), hi w0 + 1, 0x80); 3 sub_4A2940 on the 3 voice slots;
	// 4 save camera (0xB8B7F0..FF -> 0xB8B800..0F, H 0x1D8E038 -> 0x1D977A0, 0x1D977A2 -> 0x1D9771C);
	// 6 sub_504270(1); 7 sub_504270(0); 8 BS_Camera_ArmReturnAfterEffect; other (0, 9..15):
	// unless SequenceState+9 & 0x20, eye = bone outPos, H = bone+0x8C, 0x1D977A2 = bone+0x8E,
	// look-at = outPos of bone ref w0. cursor += 4.
	// ------------------------------------------------------------------------------------
	static void __cdecl op_039_CameraAndSound()
	{
		uint32_t sub = (uint32_t)U16(RT(), 0x4A) >> 12;
		switch (sub - 1)
		{
		case 0: // 0xA8AE25
		{
			uint8_t *f = RESFILE()[0];
			x::PlayCameraAnimation(f + U32(f, 0x20));
			break;
		}
		case 1: // 0xA8AE47
		{
			uint8_t *f = RESFILE()[0];
			uint8_t *s = STREAM();
			uint32_t slot = U8(s, 2);
			uint8_t *snd = f + U32(f, 0x24);
			int32_t n = (int32_t)U8(s, 3) + 1;
			uint32_t v = x::ClaimVoiceSlot(snd, n, 0x80);
			U32(SCENE(), slot * 4 + 0x168) = v;
			break;
		}
		case 2: // 0xA8AE91
			for (int32_t off = 0x168; off < 0x174; off += 4)
			{
				uint32_t v = U32(SCENE(), off);
				if (v) x::Snd_4A2940((int32_t)v);
			}
			break;
		case 3: // 0xA8AEC6
		{
			uint32_t e0 = CamWord32(0), e1 = CamWord32(1), a0 = CamWord32(2);
			FX_HELD(guard(&CamWord32(4), 4);)
			CamWord32(4) = e0;             // 0xB8B800
			uint32_t a1 = CamWord32(3);
			FX_HELD(guard(&CamWord32(5), 4);)
			CamWord32(5) = e1;             // 0xB8B804
			uint16_t h = (uint16_t)PROJH();
			FX_HELD(guard(&CamWord32(7), 4);)
			CamWord32(7) = a1;             // 0xB8B80C
			FX_HELD(guard(&CamWord32(6), 4);)
			CamWord32(6) = a0;             // 0xB8B808
			uint16_t w = Word1D977A2();
			FX_HELD(guard(&Word1D977A0(), 2);)
			Word1D977A0() = h;
			FX_HELD(guard(&Word1D9771C(), 2);)
			Word1D9771C() = w;
			break;
		}
		case 4: // 0xA8AF1D (this module only): the effect light block
		{
			// 30 fps layer: see mag228_ice_strike_held.inc (outside the snapshot: journaled while predicting)
			FX_HELD(guard((void *)EFFECT_LIGHT, 0x20);)
			blob::CopyMatrix((void *)EFFECT_LIGHT, BBMAT());
			uint8_t *b = CUR();
			MEM<int32_t>(EFFECT_LIGHT + 0x14) = S16(b, 0x94);
			MEM<int32_t>(EFFECT_LIGHT + 0x18) = S16(b, 0x96);
			MEM<int32_t>(EFFECT_LIGHT + 0x1C) = S16(b, 0x98);
			break;
		}
		case 5: // 0xA8AF69
			x::Camera_504270(1);
			break;
		case 6: // 0xA8AF82
			x::Camera_504270(0);
			break;
		case 7: // 0xA8AE11
			x::CameraArmReturn();
			break;
		default: // 0xA8AF9B
			if (!(U8(SEQ(), 9) & 0x20))
			{
				uint8_t *b = CUR();
				FX_HELD(guard(&CamWord32(0), 4);)
				CamWord32(0) = U32(b, 0x94);   // eye x, y
				FX_HELD(guard(CAMEYE() + 2, 2);)
				CAMEYE()[2] = S16(b, 0x98);    // eye z
				FX_HELD(guard(&PROJH(), 2);)
				PROJH() = S16(b, 0x8C);
				FX_HELD(guard(&Word1D977A2(), 2);)
				Word1D977A2() = U16(b, 0x8E);
				uint8_t *t = blob::GetBone(S16(STREAM(), 2));
				FX_HELD(guard(&CamWord32(2), 4);)
				CamWord32(2) = U32(t, 0x94);   // look-at x, y
				FX_HELD(guard(CAMAT() + 2, 2);)
				CAMAT()[2] = S16(t, 0x98);     // look-at z
			}
			break;
		}
		STREAM() += 4;
	}

	// ------------------------------------------------------------------------------------
	// the module's own handlers (no shared handler is live in this module)
	// ------------------------------------------------------------------------------------
	static void apply_slots(Clone &c)
	{
		c.vm[0x039] = op_039_CameraAndSound;            // 0xA8ADF0
		c.vm[0x0AB] = op_0AB_BindLimitAnimSet;          // 0xA86690
		c.vm[0x0AD] = op_0AD_PosFromPosedEntityJoint;   // 0xA8EA50
	}

	static uint32_t __cdecl SequenceTask(TaskNode *)
	{
		return SequenceTick(g_c);
	}
}
}

	void register_mag228_ice_strike()
	{
		using namespace gfc;
		icestrike228::describe(icestrike228::g_c);
		// (first: init_clone reads the exe, absent when the lab only lists the parameters)
		ProgramFiles<228>::Register("Ice Strike (Edea)", icestrike228::g_c, 0xA85590, 0x186BE1C, 0x186BE28,
		                           (void *)icestrike228::SequenceTask);
		init_clone(icestrike228::g_c, nullptr, 0);
		icestrike228::apply_slots(icestrike228::g_c);
		register_port(0xA85C30, (void *)icestrike228::SequenceTask, "MAG228 Ice Strike SequenceTick", 228);
		// 30 fps layer: see mag228_ice_strike_held.inc
		FX_HELD(register_mag228_ice_strike_held();)
	}
	FX_LAB_MODULE(register_mag228_ice_strike);
}

#ifdef FF8_FX_HELD
#include "mag228_ice_strike_held.inc"
#endif
