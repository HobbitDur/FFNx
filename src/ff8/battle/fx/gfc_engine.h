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

// GF "cinematic engine" (g_GfCinematic_*): native port of the byte-code VM + draw engine that
// Ifrit (201), Leviathan (006), Bahamut (202), Cerberus (203), Alexander (204), Brothers (205)
// and Eden (206) run. The engine is ONE source library compiled seven times into FF8_EN.exe
// (see gf_study/cinematic_clones.md): the same code at seven addresses, each copy reading its
// own module tables (VM opcode table, bone/draw handler tables, attribute tables) and using the
// SHARED engine state block 0x2796E00..0x2798C40 (only one GF runs at a time).
//
// The port is written once (namespace ff8fx::gfc). A clone is a `Clone` descriptor: the
// addresses of its module tables plus port tables built from its ORIGINAL tables - each slot
// holds the generic port of that handler when the clone's handler is a byte clone of the
// generic one, otherwise the clone's original function address (called as is). Porting another
// clone = a new descriptor (table addresses + list of its unique handlers).
//
// Every table-dispatched handler is `void __cdecl fn(void)` in the original (state is passed
// through the engine globals); a port slot may therefore hold either a port or an original
// address.

#pragma once

#include "fx_port.h"

namespace ff8fx
{
namespace gfc
{
	using namespace ff8fx::eng;

	// ------------------------------------------------------------------------------------
	// raw memory access (the ports follow the listings offset by offset)
	// ------------------------------------------------------------------------------------
	template<typename T> inline T &MEM(uint32_t addr) { return *(T *)addr; }
	template<typename T> inline T &AT(const void *p, int32_t off) { return *(T *)((uint8_t *)p + off); }
	inline int8_t &S8(const void *p, int32_t o) { return AT<int8_t>(p, o); }
	inline uint8_t &U8(const void *p, int32_t o) { return AT<uint8_t>(p, o); }
	inline int16_t &S16(const void *p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(const void *p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(const void *p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(const void *p, int32_t o) { return AT<uint32_t>(p, o); }
	inline uint8_t *&PTR(const void *p, int32_t o) { return AT<uint8_t *>(p, o); }
	inline int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
	inline int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

	typedef void (__cdecl *VoidFn)();

	// ------------------------------------------------------------------------------------
	// shared engine globals (identical addresses in all seven clones)
	// ------------------------------------------------------------------------------------
	inline uint8_t *&RT() { return MEM<uint8_t *>(0x27973B8); }       // g_GfCinematic_RuntimeSlotPtr (0x50 B, scratch stack +0x100)
	inline uint8_t *&RCTX() { return MEM<uint8_t *>(0x27973BC); }     // RenderCtx (0x27975D8, 0x30 B)
	inline uint8_t *&SEQ() { return MEM<uint8_t *>(0x27973C0); }      // SequenceState (0x2797628)
	inline uint8_t *&PTR3() { return MEM<uint8_t *>(0x27973C4); }     // PSX leftover (0x803FF000), never dereferenced by Ifrit
	inline uint8_t *&CUR() { return MEM<uint8_t *>(0x27973E8); }      // g_GfCinematic_CurBonePtr (GfCinematicBone, 0x100 B)
	inline uint8_t *&CTX() { return MEM<uint8_t *>(0x27973EC); }      // g_GfCinematic_SequenceCtxPtr (0x2797690, 0xDC B)
	inline uint8_t *&SCENE() { return MEM<uint8_t *>(0x279744C); }    // SceneHeader (0x2797068)
	inline uint8_t *&STREAM() { return MEM<uint8_t *>(0x2797450); }   // g_GfCinematic_StreamCursor (VM instruction pointer, scratch)
	inline uint8_t *&WS() { return MEM<uint8_t *>(0x2797624); }       // g_GfCinematic_WorkspacePtr (0x100 B scratch, stack)
	inline uint8_t *ORDER() { return (uint8_t *)0x2797454; }           // g_GfCinematic_BoneOrderList, u8[256], 0xFF end, bit7 = immune to freeze
	inline uint8_t *DRAWORDER() { return (uint8_t *)0x2797554; }       // g_GfCinematic_DrawOrderList, u8[0xD0], 0xFF end
	inline uint16_t *NODEKEYS() { return (uint16_t *)0x2797204; }      // matrix-node key table u16[64] (bone id16 per slot)
	inline Mat4x3 *NODEMAT() { return (Mat4x3 *)0x27979E8; }           // matrix nodes [64]; [0] = g_GfCinematic_CamMatrixMain
	inline Mat4x3 *CAMALT() { return (Mat4x3 *)0x2796F90; }            // g_GfCinematic_CamMatrixAlt
	inline uint8_t *LIGHTS() { return (uint8_t *)0x27977A4; }          // g_GfCinematic_BoneMatrixTable: light sets, 0x50 B each
	inline Mat4x3 *BBMAT() { return (Mat4x3 *)0x2797968; }             // 4 billboard matrices
	inline uint8_t **RESFILE() { return (uint8_t **)0x2798A68; }       // resource file table: [0] = magNNN_b.00, [1] = .01, streamed parts after
	inline uint32_t *DEPTHARR() { return (uint32_t *)0x2798C18; }      // dword_2798C18: per-vertex depth scratch of the mesh renderer
	inline float &FLT_1877DA8() { return MEM<float>(0x1877DA8); }       // depth scale used by the prim renderers
	// battle globals
	inline uint32_t &PKTCUR() { return MEM<uint32_t>(0x1D8E054); }     // battle_texture_data_ptr_1D8E054 (main packet cursor)
	inline int16_t &PROJH() { return MEM<int16_t>(0x1D8E038); }        // word_1D8E038 projection distance H
	inline uint32_t &FRAMERL() { return MEM<uint32_t>(0x1D8E04C); }    // g_Battle_FrameRenderListBase
	inline uint32_t &UPDFLAGS() { return MEM<uint32_t>(0x1D96A9C); }   // battle_to_update_flags_dword_1D96A9C
	inline Mat4x3 &BATTLEVIEW() { return MEM<Mat4x3>(0x1D97778); }     // BD_LINK_TASK_HEADER_CAMERA.data (+g_BattleCam_View*)
	// battle camera words written by VM opcode 0x39 (eye 0xB8B7F0, look-at 0xB8B7F8)
	inline int16_t *CAMEYE() { return (int16_t *)0xB8B7F0; }
	inline int16_t *CAMAT() { return (int16_t *)0xB8B7F8; }

	// engine state block snapshot bounds (ctx, scene, rctx, seqstate, tables, lists, nodes...)
	static const uint32_t STATE_LO = 0x2796E00, STATE_HI = 0x2798C18;

	// ------------------------------------------------------------------------------------
	// modes
	// ------------------------------------------------------------------------------------
	// g_predict: the VM runs one tick ahead on saved state for a held frame; every engine call
	// with a side effect outside the snapshot (sound, music, streams, file loads, VRAM uploads,
	// battle damage, camera animation, battle entity/model writes) is skipped by its wrapper.
	extern bool g_predict;
	// g_held: a held-frame (30 fps) draw is running; num/den = position between the last real
	// tick (0) and the next one (den). The dispatcher forces rt->boneSkipFlag while it is set.
	// real_skip = the value rt->boneSkipFlag had on the real tick (before the held draw forced it).
	struct Held { bool active; int num, den; uint8_t real_skip; };
	extern Held g_held;
	// Writes the VM does OUTSIDE the snapshot set (engine block STATE_LO..STATE_HI, the bone
	// array, the arena [ctx+0x70, ctx+0x74), the RT/WS scratch block) must be announced with
	// guard(addr, size) right before the write: while predicting, the original bytes are
	// journaled and put back when the prediction ends. (No effect on real ticks.)
	void guard_record(const void *p, int n);
	inline void guard(const void *p, int n) { if (g_predict) guard_record(p, n); }

	// ------------------------------------------------------------------------------------
	// clone descriptor
	// ------------------------------------------------------------------------------------
	struct Clone
	{
		const char *name;
		int effect_id;
		// module tables (original addresses)
		uint32_t vm_table;      // VmOpcodeTable, 0x147 entries (opcode & 0x1FF)
		uint32_t bone_table;    // BoneHandlerTable (index = bone+0x18)
		uint32_t prim_table;    // prim renderers = bone_table + 0x48, 60 = 20 types x 3 variants
		uint32_t prim_size;     // u8 prim record size per type = bone_table + 0x138
		uint32_t spr_table;     // sprite orientation handlers = bone_table + 0x14C (bone+0x1E), 11
		uint32_t ptype_table;   // particle type handlers = bone_table + 0x178 (particle+0x37), 5
		uint32_t pop_table;     // particle VM opcodes = bone_table + 0x18C, 24
		uint32_t draw_table;    // DrawHandlerTable, 73
		uint32_t attr_16A;      // byte/word attribute tables after the draw table (draw_table + 0x16A,
		uint32_t attr_184;      //   +0x184, +0x186, +0x194, +0x19C); used by the generic write opcode
		uint32_t attr_186;      //   and op 0x76
		uint32_t attr_194;
		uint32_t attr_19C;
		uint32_t desc;          // per-GF resource descriptor (Ifrit 0x1874894): int offsets from desc
		uint32_t ptr_block;     // static pointer block read by BindContext (ctx, ptr3, scene, rctx, seq)
		uint32_t queue;         // effect root task queue pair
		uint32_t file00, file01;// statics holding the .00/.01 file pointers
		uint32_t tick;          // original SequenceTick address (the only task of the queue)
		uint32_t load_cb;       // the clone's file-load completion callback (Ifrit 0xB2BB40, clears
		                        // 0x2798219), passed to pre_LoadBattleFile by VM op 0x006
		bool lit_dispatcher;    // BuildMatricesAndDraw loads the light sets (Ifrit, Leviathan)
		int bone_count;         // BoneHandlerTable entries (Ifrit 18 = 13 handlers + data bytes; Eden 32)
		uint8_t model_draw[4];  // draw handler ids besides 3 whose bone+0xBC block is an embedded
		                        // battle model block (+0x14 -> skeleton), saved around held draws
		// port tables (filled by init_clone). The five handler tables are sub-ranges of one
		// memory block (bone_table .. draw_table) exactly like the original, so an index past
		// one sub-table reads the next one as vanilla does; entries that are not code keep the
		// raw value.
		VoidFn vm[0x200];
		VoidFn bigtab[256];
		VoidFn *bone, *prim, *spr, *ptype, *pop;
		VoidFn draw[256];       // 73 handlers, then whatever follows the table (u8 index)
	};
	extern Clone *g_clone;
	inline Clone &C() { return *g_clone; }

	// generic ports by table index (nullptr = not ported / compiled out); filled by the
	// fill_* functions of every part of the engine
	struct Generic
	{
		VoidFn vm[0x200];
		VoidFn bone[32];
		VoidFn prim[60];
		VoidFn spr[11];
		VoidFn ptype[5];
		VoidFn pop[24];
		VoidFn draw[73];
	};
	extern Generic g_generic;
	void fill_generic(); // calls every fill_* below once

	// a clone's handler that is NOT a byte clone of the generic one: keep the original
	struct Exception { int table; int index; }; // table: 0 vm, 1 bone, 2 prim, 3 spr, 4 ptype, 5 pop, 6 draw
	// builds the port tables of c from its original tables: stub (C3) -> no-op, index listed in
	// exceptions or without generic port -> original address, else generic port
	void init_clone(Clone &c, const Exception *ex, int nex);
	// the clones initialised so far (init_clone records them), by effect id; nullptr if none
	Clone *find_clone(int effect_id);

	// dispatch helpers (identical to the original indirect calls)
#ifdef GFC_DEBUG_HOOK
	// offline harness only: taint tracking of vanilla stack-garbage words (src == nullptr: dst
	// receives an uninitialised stack word in vanilla; else dst is a copy of src)
	extern void (*g_dbg_taint)(void *dst, const void *src, int n);
#define GFC_TAINT(d, s, n) do { if (g_dbg_taint) g_dbg_taint((void *)(d), (const void *)(s), (n)); } while (0)
#else
#define GFC_TAINT(d, s, n) do { } while (0)
#endif
#ifdef GFC_DEBUG_HOOK
	// offline harness only: called after every VM opcode / draw handler of a real tick
	extern void (*g_dbg_hook)(int kind, uint32_t id);
	inline void call_vm(uint32_t op) { C().vm[op & 0x1FF](); if (g_dbg_hook) g_dbg_hook(0, op & 0x1FF); }
#else
	inline void call_vm(uint32_t op) { C().vm[op & 0x1FF](); }
#endif

	// ------------------------------------------------------------------------------------
	// engine entry points (gfc_engine.cpp)
	// ------------------------------------------------------------------------------------
	uint32_t SequenceTick(Clone &c);     // the task function (0xB25DF0 for Ifrit)
	void BindContext();                  // 0xB25830: scratch alloc RT/WS + pointer block
	void ReleaseContext();               // 0xB258C0
	void AnimChannelsNeg();              // 0xB2FCF0
	void AnimChannelsPos();              // 0xB30110
	void AnimIntegrator();               // 0xB26110
	void BuildMatricesAndDraw();         // 0xB2ABE0 (lit variant) / Eden variant
	// held frames
	bool HeldReady(Clone &c);
	void HeldFrame(Clone &c, int num, int den);
	bool HeldCamera(Clone &c, int num, int den, int16_t world[3], int16_t lookat[3]);

	// ------------------------------------------------------------------------------------
	// engine (non-module) functions, called at their original addresses. x:: wrappers with a
	// side effect outside the engine state are skipped while g_predict is set.
	// ------------------------------------------------------------------------------------
	namespace x
	{
		template<typename F> inline F f(uint32_t a) { return (F)a; }
		// --- software GTE ---
		inline void GteWriteData(int32_t value, int reg) { f<void (__cdecl *)(int32_t, int)>(0x45D7C0)(value, reg); }
		inline void GteReadData(void *dst, int reg) { f<void (__cdecl *)(void *, int)>(0x45D7A0)(dst, reg); }
		inline void GteReadCtrl(void *dst, int reg) { f<void (__cdecl *)(void *, int)>(0x45D7D0)(dst, reg); }
		inline void GteWriteCtrl(int32_t value, int reg) { f<void (__cdecl *)(int32_t, int)>(0x45D7F0)(value, reg); } // GTE_WriteControlReg
		inline void GteWriteData2(int reg, int32_t value) { f<void (__cdecl *)(int, int32_t)>(0x45DB90)(reg, value); } // sub_45DB90
		inline void GteReadData2(int reg, void *dst) { f<void (__cdecl *)(int, void *)>(0x45DBA0)(reg, dst); }       // sub_45DBA0
		inline void GteSetBackColor3(int32_t r, int32_t g, int32_t b) { f<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DDA0)(r, g, b); } // pre_someCameraWork_45DD60
		inline void GteSetRotMatrix(const void *m) { f<void (__cdecl *)(const void *)>(0x45DE00)(m); }
		inline void GteSetTransVector(const void *m) { f<void (__cdecl *)(const void *)>(0x45DEF0)(m); }
		inline void GteLoadV0(const void *v) { f<void (__cdecl *)(const void *)>(0x45DF80)(v); }
		inline void GteLoadV012(const void *v0, const void *v1, const void *v2) { f<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(v0, v1, v2); }
		inline void GteReadOTZdiv4(void *dst) { f<void (__cdecl *)(void *)>(0x45E250)(dst); }
		inline void GteReadSXY2(void *dst) { f<void (__cdecl *)(void *)>(0x45E260)(dst); }
		inline void GteReadSXY012(void *a, void *b, void *c) { f<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
		inline void GteReadOTZ(void *dst) { f<void (__cdecl *)(void *)>(0x45E3D0)(dst); }
		inline void Gte_45E580() { f<void (__cdecl *)()>(0x45E580)(); }   // sub_45E580 (stores MVMVA result as TR)
		inline void GteAVSZ3() { f<void (__cdecl *)()>(0x45E5C0)(); }
		inline void GteNCLIP() { f<void (__cdecl *)()>(0x45EE10)(); }
		inline void GteRTPS() { f<void (__cdecl *)()>(0x45FAA0)(); }
		inline void GteRTPT() { f<void (__cdecl *)()>(0x45FE10)(); }
		inline void Gte_4601B0() { f<void (__cdecl *)()>(0x4601B0)(); }   // sub_4601B0 (lighting op used by the prim renderers)
		inline void GteMVMVA_RotV0Tr() { f<void (__cdecl *)()>(0x460840)(); }
		// --- matrices / trig ---
		inline void SetRotMatrix(const void *m) { f<void (__cdecl *)(const void *)>(0x56BAE0)(m); }       // GTE_SetRotMatrix_W
		inline void SetLightMatrix(const void *m) { f<void (__cdecl *)(const void *)>(0x56BB00)(m); }     // sub_56BB00
		inline void SetColorMatrix(const void *m) { f<void (__cdecl *)(const void *)>(0x56BB20)(m); }     // sub_56BB20
		inline void SetTransVector(const void *m) { f<void (__cdecl *)(const void *)>(0x56BB30)(m); }     // GTE_SetTransVector_W
		inline void ScaleMatrix(void *m, const void *v3) { f<void (__cdecl *)(void *, const void *)>(0x56BEF0)(m, v3); } // scale3DMatrix
		inline void *MulMatrix3(const void *a, const void *b, void *out) { return f<void *(__cdecl *)(const void *, const void *, void *)>(0x56C090)(a, b, out); } // Matrix3x3_MultiplyPSX
		inline void *MulMatrixInPlace(void *m, const void *n) { return f<void *(__cdecl *)(void *, const void *)>(0x56C220)(m, n); } // sub_56C220: m = m * n (rotation)
		inline void *MulMatrixGte(const void *a, void *b) { return f<void *(__cdecl *)(const void *, void *)>(0x56C270)(a, b); } // GTE_MatrixMultiply: b = a * b (rotation part)
		inline void Matrix_56C2C0(const void *m) { f<void (__cdecl *)(const void *)>(0x56C2C0)(m); }
		inline void *ComposeAffine(const void *a, const void *b, void *out) { return f<void *(__cdecl *)(const void *, const void *, void *)>(0x56C2F0)(a, b, out); }
		// TransformCoordinateToCameraSpace: MAC123 = R * v + TR (current GTE matrix) -> mac (3 x s32), GTE FLAG -> flag
		inline void TransformToCamera(const void *v, void *mac, void *flag) { f<void (__cdecl *)(const void *, void *, void *)>(0x56C820)(v, mac, flag); }
		inline void SetBackColor(int32_t r, int32_t g, int32_t b) { f<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x56CC30)(r, g, b); } // sub_56CC30
		inline void Fog_56CCA0(int32_t a, int32_t b) { f<void (__cdecl *)(int32_t, int32_t)>(0x56CCA0)(a, b); }
		inline void Fog_56CCC0(int32_t a, int32_t b) { f<void (__cdecl *)(int32_t, int32_t)>(0x56CCC0)(a, b); }
		inline void ParseCamera2(int32_t h) { f<void (__cdecl *)(int32_t)>(0x56CD00)(h); }                // Call_Bs_parseCamera2 (GTE H)
		inline void *RotMatrixFromAngles(const void *angles, void *out) { return f<void *(__cdecl *)(const void *, void *)>(0x56CD50)(angles, out); }
		inline int32_t Cos(int32_t a) { return f<int32_t (__cdecl *)(int32_t)>(0x56D100)(a); }
		inline int32_t Sin(int32_t a) { return f<int32_t (__cdecl *)(int32_t)>(0x56D130)(a); }
		// --- packets / OT ---
		inline void SetDrawMode(void *p, int32_t dfe, int32_t dtd, int32_t tpage, const void *rect) { f<void (__cdecl *)(void *, int32_t, int32_t, int32_t, const void *)>(0x45BFC0)(p, dfe, dtd, tpage, rect); }
		inline void InsertPrimDepthKeys(uint32_t ot, void *prim, int32_t a, int32_t b, int32_t c, int32_t d) { f<void (__cdecl *)(uint32_t, void *, int32_t, int32_t, int32_t, int32_t)>(0x45C870)(ot, prim, a, b, c, d); }
		inline void InsertPrimAltViewport(uint32_t ot, void *prim) { f<void (__cdecl *)(uint32_t, void *)>(0x45C8E0)(ot, prim); }
		inline void ClearOT(uint32_t ot, int32_t n) { f<void (__cdecl *)(uint32_t, int32_t)>(0x45D530)(ot, n); }
		// --- battle models (the embedded GF model) ---
		// (these write the model's skeleton / pose buffers, outside the snapshot: skipped while
		// predicting - the embedded model is not part of the prediction, its held pose comes from
		// pose_midpoint)
		inline void BuildBoneMatricesFromPose(void *hdr) { if (!g_predict) f<void (__cdecl *)(void *)>(0x508C90)(hdr); }
		inline int32_t ReadAnimation(void *hdr, void *cmd) { return g_predict ? 0 : f<int32_t (__cdecl *)(void *, void *)>(0x508F90)(hdr, cmd); }
		inline int32_t PreReadAnimation(void *hdr, void *cmd, int32_t anim) { return g_predict ? 0 : f<int32_t (__cdecl *)(void *, void *, int32_t)>(0x509440)(hdr, cmd, anim); }
		inline void ComputeBonesWorldMatrices(void *hdr, void *root) { if (!g_predict) f<void (__cdecl *)(void *, void *)>(0x5095B0)(hdr, root); }
		inline uint32_t RenderGeometry(void *a, void *b, uint32_t ot, int32_t mode, uint32_t cursor) { return g_predict ? cursor : f<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(0x5099D0)(a, b, ot, mode, cursor); }
		inline uint32_t Render_5088A0(void *a, uint32_t ot, int32_t mode, uint32_t cursor) { return g_predict ? cursor : f<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(0x5088A0)(a, ot, mode, cursor); }
		// --- CRT rand (per-thread _holdrand) ---
		inline int32_t Rand() { return f<int32_t (__cdecl *)()>(0x55CBD2)(); }
		// --- scratch stack ---
		inline void *FieldAlloc(int32_t n) { return f<void *(__cdecl *)(int32_t)>(0x5082B0)(n); }
		inline void FieldFree(int32_t n) { f<void (__cdecl *)(int32_t)>(0x5082D0)(n); }

		// --- side effects outside the engine state (skipped while predicting) ---
		inline void PlaySE(void *snd, int32_t a, int32_t b) { if (!g_predict) f<void (__cdecl *)(void *, int32_t, int32_t)>(0x501330)(snd, a, b); }        // BdPlaySE
		inline void TransSummonStream(void *a, void *b) { if (!g_predict) f<void (__cdecl *)(void *, void *)>(0x501860)(a, b); }                  // BdTransSummonStream
		inline void PlaySummonStream(int32_t a, int32_t b, int32_t c) { if (!g_predict) f<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x5018C0)(a, b, c); } // BdPlaySummonStream
		inline void Snd_46B3A0(int32_t a) { if (!g_predict) f<void (__cdecl *)(int32_t)>(0x46B3A0)(a); }
		inline void Snd_46B3E0(int32_t a) { if (!g_predict) f<void (__cdecl *)(int32_t)>(0x46B3E0)(a); }
		inline void Snd_46B450(int32_t a, int32_t b) { if (!g_predict) f<void (__cdecl *)(int32_t, int32_t)>(0x46B450)(a, b); }
		inline void MusicSetVolumeTrans(int32_t a, int32_t b, int32_t c) { if (!g_predict) f<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x46BBC0)(a, b, c); }
		inline void Music_47E3C0(int32_t a) { if (!g_predict) f<void (__cdecl *)(int32_t)>(0x47E3C0)(a); }
		inline void PreLoadBattleFile(int32_t id, void *dst, int32_t a, uint32_t cb) { if (!g_predict) f<void (__cdecl *)(int32_t, void *, int32_t, uint32_t)>(0x48D0A0)(id, dst, a, cb); }
		inline void Snd_4A2940(int32_t a) { if (!g_predict) f<void (__cdecl *)(int32_t)>(0x4A2940)(a); }
		inline uint32_t ClaimVoiceSlot(const void *snd, int32_t a, int32_t b) { return g_predict ? 0 : f<uint32_t (__cdecl *)(const void *, int32_t, int32_t)>(0x4A29A0)(snd, a, b); }
		inline void Battle_4A8480(int32_t a) { if (!g_predict) f<void (__cdecl *)(int32_t)>(0x4A8480)(a); }
		inline void Battle_501E40(int32_t a) { if (!g_predict) f<void (__cdecl *)(int32_t)>(0x501E40)(a); }
		inline void Battle_501F30(int32_t a) { if (!g_predict) f<void (__cdecl *)(int32_t)>(0x501F30)(a); }
		inline void Camera_504270(int32_t a) { if (!g_predict) f<void (__cdecl *)(int32_t)>(0x504270)(a); }
		inline void QueueVramUpload(const void *rect, const void *data) { if (!g_predict) f<void (__cdecl *)(const void *, const void *)>(0x505DF0)(rect, data); } // Battle_QueueVramUpload_Type0_RectData
		inline void ApplyActionResultToTarget(int32_t a) { if (!g_predict) f<void (__cdecl *)(int32_t)>(0x506690)(a); }
		inline void ApplyActionResultToTargets(int32_t a, int32_t b) { if (!g_predict) f<void (__cdecl *)(int32_t, int32_t)>(0x506BA0)(a, b); }
		inline void Battle_508630(void *a, void *b) { if (!g_predict) f<void (__cdecl *)(void *, void *)>(0x508630)(a, b); }
		inline void PlayCameraAnimation(void *a) { if (!g_predict) f<void (__cdecl *)(void *)>(0x5099A0)(a); }  // Battle_PlayCameraAnimation
		inline void CameraArmReturn() { if (!g_predict) f<void (__cdecl *)()>(0x50A730)(); }                    // BS_Camera_ArmReturnAfterEffect
	}

	// 0x56C270 / 0x56C220 build their product in a stack temporary and copy 5 dwords of it to
	// the destination; the temporary's pad word (+0x12) is never written, so the destination's
	// +0x12 receives stack garbage. When the call directly follows 0x56CD50
	// BuildRotationMatrixFromAngles(angles, m) with both calls' arguments still on the stack
	// (the vanilla pattern: no `add esp` between them), that word is the high half of the
	// sign-extended angles[1] that 0x56CD50 pushed for ApplyYRotation 0x56D020. The C++ call
	// sequence has another stack layout, so the ports write the vanilla value explicitly.
	inline void pad_after_rot_mul(void *m, const void *angles) { U16(m, 0x12) = S16(angles, 2) < 0 ? 0xFFFF : 0; }

	// ------------------------------------------------------------------------------------
	// shared GF helper blob 0xB65150..0xB67400 (one copy for all seven clones). Called at the
	// original addresses; they only touch the engine state block, the bone array, the arena
	// (ctx+0x74) and the GTE, except where noted.
	// ------------------------------------------------------------------------------------
	namespace blob
	{
		template<typename F> inline F f(uint32_t a) { return (F)a; }
		inline void InitBone(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(0xB65160)(id); }              // clear bone id, defaults
		inline void OrderListRemove(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(0xB651E0)(id); }       // + frees its node key
		inline void DrawListRemove(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(0xB65270)(id); }
		inline void DrawListAddId(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(0xB652E0)(id); }
		inline void SetDrawHandler(int32_t h) { f<int32_t (__cdecl *)(int32_t)>(0xB65320)(h); }         // cur bone +0x1C = h, append cur bone to draw list
		inline uint8_t *GetBone(int32_t ref) { return f<uint8_t *(__cdecl *)(int32_t)>(0xB65370)(ref); } // GfCinematic_GetRotationVector: bone ref -> bone
		inline Mat4x3 *NodeForCurBone() { return f<Mat4x3 *(__cdecl *)()>(0xB65480)(); }                // find-or-allocate node slot of cur bone
		inline Mat4x3 *GetParentMatrix(int32_t ref) { return f<Mat4x3 *(__cdecl *)(int32_t)>(0xB65520)(ref); } // also sets GTE H
		inline Mat4x3 *RotMatrixOrder(const void *angles, int32_t order) { return f<Mat4x3 *(__cdecl *)(const void *, int32_t)>(0xB65590)(angles, order); } // into ws+0xE0
		inline void CopyMatrix(void *dst, const void *src) { f<void (__cdecl *)(void *, const void *)>(0xB656A0)(dst, src); }
		// bump ctx+0x74 (returns the old top); predicting: the memory handed out is journaled
		inline uint8_t *ArenaAlloc(int32_t n)
		{
			if (g_predict) guard(PTR(CTX(), 0x74), (n + 3) & ~3);
			return f<uint8_t *(__cdecl *)(int32_t)>(0xB656E0)(n);
		}
		inline void ArenaFree_B65710() { f<int32_t (__cdecl *)()>(0xB65710)(); }
		inline int32_t Rand73(int32_t range) { return f<int32_t (__cdecl *)(int32_t)>(0xB65740)(range); } // CRT rand, signed range
		inline int32_t Rand74(int32_t range) { return f<int32_t (__cdecl *)(int32_t)>(0xB657A0)(range); } // CRT rand, 0..range
		inline uint8_t *ObjectPtr(int32_t id) { return f<uint8_t *(__cdecl *)(int32_t)>(0xB657E0)(id); }  // object table entry
		inline uint8_t *ObjectPtrWs(int32_t id) { return f<uint8_t *(__cdecl *)(int32_t)>(0xB66230)(id); } // same, ws+0xFC = table base
		inline void SetSpriteAnim(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(0xB66270)(id); }
		inline uint8_t *VramRectRing() { return f<uint8_t *(__cdecl *)()>(0xB663A0)(); }               // SceneHeader+0x43 ring of 16 RECTs
		// VRAM side effects (skipped while predicting; the ws fields they set are restored anyway)
		// GF_AlternativeTexture_UNK: per-tick budget ctx+0xD2 (< 12 uploads), returns 1 when over
		// budget. Predicting: the budget logic without the upload.
		inline int32_t AltTextureUpload(const void *rect, const void *data)
		{
			if (!g_predict) return f<int32_t (__cdecl *)(const void *, const void *)>(0xB663C0)(rect, data);
			uint8_t &n = U8(CTX(), 0xD2);
			if (n >= 12) return 1;
			n++;
			return 0;
		}
		inline void ReadAltTexture(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(0xB664A0)(id); }       // only sets ws+0xF0..0xFC
		inline void ReadClut_B66560(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(0xB66560)(id); }      // only sets ws+0xF0/0xFC
		inline void TexInfo_B665C0(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(0xB665C0)(id); }       // only sets ws+0xF8/0xFC
		inline int32_t ClutWord_B66640(int32_t id) { return f<int32_t (__cdecl *)(int32_t)>(0xB66640)(id); }
		inline void ClutTint_B666F0(int32_t r, int32_t g, int32_t b) { if (!g_predict) f<int32_t (__cdecl *)(int32_t, int32_t, int32_t)>(0xB666F0)(r, g, b); } // tints + uploads a CLUT
		inline void Blob_B66A90() { f<int32_t (__cdecl *)()>(0xB66A90)(); }
		inline int32_t Blob_B66B30(int32_t a, int32_t b, int32_t c, int32_t d) { return f<int32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(0xB66B30)(a, b, c, d); }
		inline void Blob_B66B80(void *a, int32_t b, int32_t c, void *d) { f<int32_t (__cdecl *)(void *, int32_t, int32_t, void *)>(0xB66B80)(a, b, c, d); }
		inline void LookAt_B66E50(const void *a, const void *b, void *out) { f<int32_t (__cdecl *)(const void *, const void *, void *)>(0xB66E50)(a, b, out); }
	}

	// ------------------------------------------------------------------------------------
	// parts of the engine (one fill function per source part; each sets the g_generic slots
	// of the handlers it ports)
	// ------------------------------------------------------------------------------------
	// module helpers shared between parts (original Ifrit addresses in the names)
	void h_B269E0();                   // draw_mesh: SetupParentXformScaled
	int32_t h_B26AD0(uint8_t *blk);    // draw_mesh: StepEmbeddedModelAnim (model block)
	uint8_t *h_B26860(int32_t id);     // vm_a: create embedded model block for object id
	void h_B2C440();                   // vm_b: outAngle = HIWORD(accumRot)
	int32_t h_B2D460();                // vm_b: recompute bone+0x1A integrator flags
	int32_t h_B2F8F0();                // core: InitBones
	int32_t h_B2FC20();                // core: target/scene setup
	void h_B27000();                   // draw_mesh: SetupParentXform (GTE R/TR = parent, TR = parent * outPos)
	void h_B27130();                   // draw_mesh: SetupParentXformScaledAtOrigin
	void h_B27360(const void *pos, const void *angles, int32_t scale, int32_t order); // draw_mesh: SetupBillboardXform
	void h_B27440(const int16_t *clipArg); // draw_mesh: DrawMeshObject (the generic mesh renderer of CUR())
	void h_B29450(int32_t c);          // draw_prim: light/colour setup of the tinted prim renderers

	void fill_core(Generic &g);
	void fill_vm_a(Generic &g);
	void fill_vm_b(Generic &g);
	void fill_vm_c(Generic &g);
	void fill_vm_d(Generic &g);
	void fill_draw_mesh(Generic &g);
	void fill_draw_prim(Generic &g);
	void fill_draw_sprite(Generic &g);

	// clone-specific handler sets shared by several clones (gfc_engine.cpp): install the ports into
	// a clone's tables after init_clone
	void apply_shared_ribbon(Clone &c);   // draw 37 ribbon trail (Bahamut, Cerberus, Alexander, Eden)
	void apply_shared_misc(Clone &c);     // draw 16/26/39, prim 16/29, VM 0x022/0x042/0x05C/0x090/0x096/0x0CF
}
}
