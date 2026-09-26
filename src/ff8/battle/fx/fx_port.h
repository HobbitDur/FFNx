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

// Native re-implementation of FF8's battle effect code (the "aoy" magic/GF modules).
//
// Every ported function is a C++ twin of an original FF8_EN.exe function, bit-exact: same
// integer widths and wrap-around, same order of random draws, engine calls and memory
// writes. With the ports enabled the game behaves exactly like vanilla.
//
// Ported task functions are dispatched by FFNx's replacement of the task queue executor
// (ExecuteTaskQueue 0x508420, fx_dispatch.cpp): a node keeps its ORIGINAL function address
// and the executor looks the address up here. Ported code calls engine functions it has not
// ported through their original addresses (namespace eng).
//
// fx_verify.cpp can run every tick of a ported effect twice from one memory snapshot (the
// original code, then the ports) and compare all state: any difference keeps the original
// result and is logged, so the game always continues on vanilla behaviour.
//
// FX_HELD(...) marks the few statements a frame-interpolation layer needs on real ticks
// (memos of what was drawn). It expands to nothing unless FF8_FX_HELD is defined (the
// True30FPS branch), so this code is the plain vanilla logic.

#pragma once

#include <stdint.h>
#include <string.h>

#ifdef FF8_FX_HELD
#define FX_HELD(...) __VA_ARGS__
#else
#define FX_HELD(...)
#endif

namespace ff8fx
{
#pragma pack(push, 1)
	struct TaskNode // TaskNodeHeader
	{
		uint16_t flags;    // bit0 = node in use (BD_LINK_NODE_USABILITY_NODE_USED)
		uint16_t sequence; // sequence_id
		TaskNode *next;
		uint32_t (__cdecl *func)(TaskNode *);
	};
	struct TaskQueue // TaskQueueHeader
	{
		TaskNode *head, *tail;
		void *pool;
		int16_t node_size, capacity;
	};
	struct TaskQueuePair // TaskQueueExample: two independent queues 16 bytes apart
	{
		TaskQueue first;  // IDA ".header"
		TaskQueue second; // IDA ".data" (data..data3)
	};
	struct Mat4x3 // FF8_PSX_Matrix4x3 (PSX MATRIX): 3x3 s16 rotation (4.12) + int32 translation
	{
		int16_t m[3][3];
		int16_t pad;
		int32_t t[3];
	};
#pragma pack(pop)
	static_assert(sizeof(TaskNode) == 12, "TaskNodeHeader is 12 bytes");
	static_assert(sizeof(TaskQueue) == 16, "TaskQueueHeader is 16 bytes");
	static_assert(sizeof(TaskQueuePair) == 32, "TaskQueueExample is 32 bytes");
	static_assert(sizeof(Mat4x3) == 32, "FF8_PSX_Matrix4x3 is 32 bytes");

	typedef uint32_t (__cdecl *TaskFn)(TaskNode *);

	// task return value bit: node finished, the executor unlinks it
	static const uint32_t TASK_END = 2;

	// Engine functions and globals that ported code uses (original addresses, FF8_EN 1.2 US)
	namespace eng
	{
		template<typename T> inline T &var(uint32_t addr) { return *(T *)addr; }
		template<typename F> inline F fn(uint32_t addr) { return (F)addr; }

		// task queues
		inline int ExecuteTaskQueue(TaskQueue *q) { return fn<int (__cdecl *)(TaskQueue *)>(0x508420)(q); }
		inline void InitTaskQueuePool(TaskQueue *q, void *pool, uint32_t node_size, int count) { fn<void (__cdecl *)(TaskQueue *, void *, uint32_t, int)>(0x508300)(q, pool, node_size, count); }
		inline TaskNode *AddTaskToQueue(TaskQueue *q, uint32_t task_fn) { return fn<TaskNode *(__cdecl *)(TaskQueue *, uint32_t)>(0x508360)(q, task_fn); }
		// scratch stack
		inline void *FieldAlloc(int size) { return fn<void *(__cdecl *)(int)>(0x5082B0)(size); }
		inline void FieldFree(int size) { fn<void (__cdecl *)(int)>(0x5082D0)(size); }
		inline void Memset32(void *dst, uint32_t value, uint32_t dwords) { fn<void (__cdecl *)(void *, uint32_t, uint32_t)>(0x701200)(dst, value, dwords); }
		// GTE / matrices
		inline void GteSetRotMatrix(const Mat4x3 *m) { fn<void (__cdecl *)(const Mat4x3 *)>(0x56BAE0)(m); }
		inline void GteSetTransVector(const Mat4x3 *m) { fn<void (__cdecl *)(const Mat4x3 *)>(0x56BB30)(m); }
		inline void BuildRotationMatrixFromAngles(const int16_t *angles, Mat4x3 *out) { fn<void (__cdecl *)(const int16_t *, Mat4x3 *)>(0x56CD50)(angles, out); }
		inline void ComposeAffineTransform(const Mat4x3 *a, const Mat4x3 *b, Mat4x3 *out) { fn<void (__cdecl *)(const Mat4x3 *, const Mat4x3 *, Mat4x3 *)>(0x56C2F0)(a, b, out); }
		// MAG_069_sub_67AAE0: effect-relative camera matrix (camera composed with the effect's frame)
		inline void EffectCameraMatrix(const Mat4x3 *camera, Mat4x3 *out) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x67AAE0)(camera, out); }
		// software GTE (the PSX geometry coprocessor, emulated by the PC port)
		inline void GteLoadV0(const void *coord) { fn<void (__cdecl *)(const void *)>(0x45DF80)(coord); }
		inline void GteRTPS() { fn<void (__cdecl *)()>(0x45FAA0)(); }
		inline void GteReadSXY2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E260)(dst); }
		inline void GteReadOTZ(void *dst) { fn<void (__cdecl *)(void *)>(0x45E250)(dst); } // OTZ = SZ3 / 4
		inline void GteSetIR0(int32_t v) { fn<void (__cdecl *)(int32_t)>(0x45E150)(v); }
		inline void GteLoadIR123(const void *v3) { fn<void (__cdecl *)(const void *)>(0x45E0B0)(v3); } // 3 x u16, zero-extended
		inline void GteGPF() { fn<void (__cdecl *)()>(0x45E9D0)(); }  // MAC = IR0 * IR (interpolation)
		inline void GteGPL() { fn<void (__cdecl *)()>(0x45EBF0)(); }  // MAC += IR0 * IR
		inline void GteStoreIR123(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3E0)(dst); } // 3 x s16
		inline void GteSetRotMatrixCtrl(const Mat4x3 *m) { fn<void (__cdecl *)(const Mat4x3 *)>(0x45DE00)(m); }   // via control registers
		inline void GteSetTransVectorCtrl(const Mat4x3 *m) { fn<void (__cdecl *)(const Mat4x3 *)>(0x45DEF0)(m); }
		inline void GteMVMVA_RotV0Tr() { fn<void (__cdecl *)()>(0x460840)(); }        // IR = R * V0 + TR
		inline void GteReadDataReg(int reg, void *dst) { fn<void (__cdecl *)(int, void *)>(0x45DBA0)(reg, dst); }
		inline void GteReadDataRegTo(void *dst, int reg) { fn<void (__cdecl *)(void *, int)>(0x45D7A0)(dst, reg); }
		inline void GteSetRotDiagonal(const int32_t *v3) { fn<void (__cdecl *)(const int32_t *)>(0x45DBC0)(v3); } // R11, R22, R33 (outer product operand)
		inline void GteLoadIR123_32(const int32_t *v3) { fn<void (__cdecl *)(const int32_t *)>(0x45DBF0)(v3); }
		inline void GteLoadIR123_32b(const int32_t *v3) { fn<void (__cdecl *)(const int32_t *)>(0x45E090)(v3); }
		inline void GteOP() { fn<void (__cdecl *)()>(0x45E670)(); }                   // outer product
		inline void GteSQR() { fn<void (__cdecl *)()>(0x45F930)(); }                  // MAC = IR * IR
		inline void GteReadMAC123(int32_t *dst) { fn<void (__cdecl *)(int32_t *)>(0x45E450)(dst); }
		inline void GteSumMAC123(int32_t *dst) { fn<void (__cdecl *)(int32_t *)>(0x703FE0)(dst); }  // MAG_089_sub_703FE0: MAC1 + MAC2 + MAC3
		inline void GteSetLZCS(int32_t v) { fn<void (__cdecl *)(int32_t)>(0x45DCA0)(v); }
		inline void GteReadLZCR(int32_t *dst) { fn<void (__cdecl *)(int32_t *)>(0x45E570)(dst); }
		inline int32_t ComputeSin(int32_t angle) { return fn<int32_t (__cdecl *)(int32_t)>(0x56D130)(angle); }
		// drawing
		inline void InsertPrimAutoDepth(uint32_t bucket, void *packet) { fn<void (__cdecl *)(uint32_t, void *)>(0x45C7A0)(bucket, packet); }
		inline uint32_t RenderPrimModel(void *header, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x572200)(header, ot, mode, cursor); }
		inline void SetScreenFlash(uint32_t level, uint32_t mode) { fn<void (__cdecl *)(uint32_t, uint32_t)>(0x5713E0)(level, mode); }
		// effect sprite sequences (the "flipbook" player): header 0xB4 bytes on the scratch stack
		inline void TransformCameraByShadowRotation(const void *pos, int32_t a, int32_t b) { fn<void (__cdecl *)(const void *, int32_t, int32_t)>(0x571BC0)(pos, a, b); }
		inline uint32_t InitEffectSequenceFromData(void *header, uint32_t ot, int mode, uint32_t cursor) { return fn<uint32_t (__cdecl *)(void *, uint32_t, int, uint32_t)>(0x571C80)(header, ot, mode, cursor); }
		// battle models
		inline void ComputeBonesWorldMatrices(void *anim_header, void *root) { fn<void (__cdecl *)(void *, void *)>(0x5095B0)(anim_header, root); }
		inline void BuildBoneMatricesFromPose(void *anim_header) { fn<void (__cdecl *)(void *)>(0x508C90)(anim_header); }
		// sound / streaming
		inline uint32_t ClaimVoiceSlot(const void *sound, int a, int b) { return fn<uint32_t (__cdecl *)(const void *, int, int)>(0x4A29A0)(sound, a, b); }
		inline void StreamStateInit(void *state) { fn<void (__cdecl *)(void *)>(0x534110)(state); }

		// globals
		inline Mat4x3 &Camera() { return var<Mat4x3>(0x1D97778); }                     // BD_LINK_TASK_HEADER_CAMERA.data
		inline uint32_t RenderOT() { return var<uint32_t>(0x1D8E04C) + 68; }          // g_Battle_FrameRenderListBase + 68
	}

	// 32-bit two's-complement arithmetic exactly as the x86 code does it (C++ signed overflow
	// and left shifts of negative values are undefined; the original relies on wrapping)
	inline int32_t mul32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
	inline int32_t shl32(int32_t a, int n) { return (int32_t)((uint32_t)a << n); }

	// --- port registry ---
	void register_port(uint32_t orig, void *port, const char *name, int effect_id);
	void *lookup(uint32_t orig);        // ported twin, or nullptr (only while ports are active)
	bool module_ported(int effect_id);  // at least one port registered for this effect
	const char *port_name(uint32_t orig);
	extern bool g_active;               // ports dispatched (fx_dispatch.cpp / fx_verify.cpp)
	void register_all();                // every module's register function (fx_port.cpp), once
	void install();                     // FFNx hook installation (fx_dispatch.cpp), once
	extern void (*g_queue_seen)(TaskQueue *q); // verifier callback: a queue runs

	// --- hosting by another FFNx layer (fx_dispatch.cpp / fx_verify.cpp) ---
	// A layer that owns the effect tick call (0x50093A) itself calls install_hosted() instead
	// of letting install() run: the executor replacement (0x508420) and the ports are
	// installed, the verifier's external call sites too when verification is on, but the
	// tick call is not hooked and the ports stay off (g_active false) outside verify_tick().
	// install() does nothing afterwards.
	void install_hosted();
	// One effect tick (queue = C3_28_GF_data_pointer, orig = the original tick call target)
	// exactly as the verifier's own 0x50093A hook runs it: verified against the original when
	// the running effect's module is ported and fits the snapshot (ff8_battle_fx_verify), the
	// ports alone when verification is off, else the original code.
	int verify_tick(void *queue, int (__cdecl *orig)(void *));
	// Optional observer of the verifier's runs (nullptr = none)
	enum VerifyEvent { VERIFY_ORIGINAL_RUN, VERIFY_PORT_RUN, VERIFY_KEPT_ORIGINAL, VERIFY_KEPT_PORT };
	extern void (*g_verify_event)(VerifyEvent e);

	// Optional observer of the task queue executor (nullptr = none). queue_start: a queue is
	// about to run (after g_queue_seen). task_before: before a node's function, index = nodes
	// run so far in this call; returning false stops the queue there (as if the list ended).
	// task_after: after it, with its return value and the cookie task_before stored.
	struct ExecObserver
	{
		void (*queue_start)(TaskQueue *q);
		bool (*task_before)(TaskQueue *q, TaskNode *n, int index, uint32_t *cookie);
		void (*task_after)(TaskQueue *q, TaskNode *n, uint32_t ret, uint32_t cookie);
	};
	extern const ExecObserver *g_exec_observer;

	// Shared effect prim-model player (MAG_011_sub_701970, ~300 callers): see fx_primplayer.cpp
	namespace prim
	{
		struct Layout { uint8_t *data; int32_t frame; uint8_t state[4]; }; // state: per-object integrators
		struct Record // 44 bytes, handed to the callback for every object
		{
			uint16_t index, flags_lo; uint32_t flags;
			int16_t pos[3], pad0; int16_t rot[3], pad1; int16_t scale[3], pad2;
			uint8_t rgb[4];
			int16_t a;      // channel A value >> 16
			int16_t b;      // channel B value >> 16
			int16_t b0, b1; // channel B words
		};
		typedef void(__cdecl *Callback)(Layout *l, Record *r, int arg);
		// exact twin of 0x701970: returns the frames left (0 = finished, nothing drawn)
		int play(Layout *l, Callback cb, int arg, int paused);
		// live self-check of the native player against every original call in the game
		void install_verify();
	}

	// module register functions (one per ported effect module)
	void register_mag069_griever();
	void register_mag116_quezacotl();
	void register_mag140_phoenix();
	void register_mag185_shiva();
	void register_mag187_odin();
	void register_mag199_cactuar();
	void register_mag278_carbuncle();
	void register_mag291_pandemona();
	void register_mag325_diablos();
	void register_mag326_odin_reverse();
	void register_mag338_moomba();
	void register_mag095_siren();
	void register_mag096_minimog();
	void register_mag090_tonberry();
	void register_mag097_boko();
	void register_mag002_fire();
	void register_mag142_fira();
	void register_mag143_firaga();
	void register_mag003_thunder();
	void register_mag102_thundara();
	void register_mag105_thundaga();
	void register_mag144_blizzard();
	void register_mag103_blizzara();
	void register_mag104_blizzaga();
	void register_gfc_ifrit();
	void register_gfc_leviathan();
	void register_gfc_bahamut();
	void register_gfc_cerberus();
	void register_gfc_alexander();
	void register_gfc_brothers();
	void register_gfc_eden();
	void register_mag191_doomtrain();
	void register_mag327_gilgamesh();
}
