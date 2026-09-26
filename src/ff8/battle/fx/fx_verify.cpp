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

// Differential verifier of the native effect code (FFNx.toml: ff8_battle_fx_verify).
//
// The battle effect tick (BdLink @0x50093A: ExecuteTaskQueue(C3_28_GF_data_pointer)) is
// hooked. While the running effect has a module entry (fx_modules.cpp) and at least one
// ported function, every real tick runs twice from one memory snapshot:
//   A = the original code (ports off), every external engine call (fx_modules ext_sites:
//       sound, loads, damage, engine task spawns, off-screen renders...) RECORDED: its
//       arguments, return value, the x87 control word it left and the bytes it wrote
//       inside the tracked memory;
//   B = the same tick with the ports dispatched, the external calls REPLAYED from A (not
//       executed again: sounds, loads and damage happen exactly once).
// Tracked memory = the engine regions (magic buffer with the effect packet arenas, camera,
// GTE registers, entity arrays...), the module's globals, extra cell and stream buffers,
// the ordering-table span, the SSIGPU execution arena (OT nodes), the free part of the
// frame packet arena, the Field_Alloc scratch, the CRT rand seed, and the header + node
// pool of every task queue the tick runs, spawns into or initialises (a queue first seen
// mid-tick is saved before it runs / before the task is added).
// After B everything is compared: queue return value, external call sequence (sites and
// verified arguments), then every tracked byte. Identical: B's result stays. Any difference:
// the post-A state is put back, so the game always continues exactly as vanilla, and the
// first difference is logged (at most MISMATCH_LOG_MAX lines per summon). B runs under SEH;
// after FAULTS_MAX faults in one summon only the original code runs for the rest of it.
// The hit-effect queue (0x500923) is not verified (no ports there).

#include "fx_port.h"
#include "fx_modules.h"
#include "../../../patch.h"
#include "../../../log.h"

#include <windows.h>
#include <stdio.h>
#include <utility>

namespace ff8fx
{
namespace
{
	const uint32_t MISMATCH_LOG_MAX = 60;
	const uint32_t FAULTS_MAX = 3;

	int (__cdecl *g_tick_orig)(TaskQueue *) = nullptr;

	// ------------------------------------------------------------------ tracked memory
	// Every range a tick may modify, snapshotted once (g_snap), saved after the original run
	// (g_post) and copied before each recorded external call (g_scan, for its write log).
	// All three buffers use the same offsets.
	enum Kind : uint8_t { K_REGION, K_OT, K_ARENA, K_QHEADER, K_QPOOL };
	enum Phase : uint8_t { PH_PRE, PH_A, PH_B }; // when the range became tracked
	struct Entry
	{
		uint32_t addr, size, off;
		Kind kind;
		Phase phase;
		const char *name;
		TaskQueue *q;       // K_QHEADER / K_QPOOL
		uint32_t node_size; // K_QPOOL
	};
	const int ENTRIES_MAX = 160;
	// engine regions ~0x101400 + module globals 0x20000 + streams 0x80000 + OT span 0x44CC +
	// SSIGPU arena 0x60000 + frame packet window 0x40000 + scratch 0x1000 + pools 0x80000
	const uint32_t BUF_SIZE = 0x300000;
	const uint32_t POOL_MAX = 0x10000;          // one task queue's node storage
	const uint32_t POOLS_TOTAL_MAX = 0x80000;   // all of them
	const uint32_t FRAME_WINDOW_MAX = 0x40000;  // free part of the frame packet arena (0x1C000 / 0x24000)
	Entry g_ent[ENTRIES_MAX];
	int g_nent = 0;
	uint32_t g_used = 0, g_pools_used = 0;
	uint8_t g_snap[BUF_SIZE], g_post[BUF_SIZE], g_scan[BUF_SIZE];
	bool g_incomplete = false;       // the tick touched something that could not be tracked
	char g_incomplete_msg[192];

	Phase g_phase = PH_PRE;
	bool g_tracking = false;         // A or B running: queues seen / spawned into get tracked

	// queues run by the current summon: tracked from the start of every later tick
	TaskQueue *g_known[32];
	int g_nknown = 0;

	void set_incomplete(const char *fmt, uint32_t a, uint32_t b)
	{
		if (g_incomplete) return;
		g_incomplete = true;
		_snprintf_s(g_incomplete_msg, sizeof(g_incomplete_msg), _TRUNCATE, fmt, a, b);
	}

	bool add_entry(uint32_t addr, uint32_t size, Kind kind, const char *name, TaskQueue *q = nullptr, uint32_t node_size = 0)
	{
		if (!size) return true;
		if (g_nent >= ENTRIES_MAX || g_used + size > BUF_SIZE)
		{
			set_incomplete("tracked memory full (range %08X, %X bytes)", addr, size);
			return false;
		}
		Entry &e = g_ent[g_nent++];
		e = { addr, size, g_used, kind, g_phase, name, q, node_size };
		memcpy(g_snap + g_used, (const void *)addr, size);
		g_used += size;
		return true;
	}

	bool has_entry(uint32_t addr, Kind kind)
	{
		for (int i = 0; i < g_nent; i++)
			if (g_ent[i].addr == addr && g_ent[i].kind == kind) return true;
		return false;
	}

	void note_known(TaskQueue *q)
	{
		if (g_phase == PH_B) return; // only what the original code runs
		for (int i = 0; i < g_nknown; i++) if (g_known[i] == q) return;
		if (g_nknown < 32) g_known[g_nknown++] = q;
	}

	inline uint32_t pool_size(const TaskQueue *q) { return (uint32_t)(uint16_t)q->node_size * (uint16_t)q->capacity; }

	// header + node storage of a task queue, before the tick changes them. pool == nullptr:
	// the pool the header names (InitTaskQueuePool passes the new one, the header is not set yet)
	void track_queue(TaskQueue *q, void *pool = nullptr, uint32_t size = 0, uint32_t node_size = 0)
	{
		if (!q) return;
		if (!has_entry((uint32_t)q, K_QHEADER))
		{
			if (IsBadReadPtr(q, sizeof(TaskQueue))) { set_incomplete("task queue %08X unreadable%.0u", (uint32_t)q, 0); return; }
			add_entry((uint32_t)q, sizeof(TaskQueue), K_QHEADER, "task queue header", q);
		}
		note_known(q);
		if (!pool) { pool = q->pool; size = pool_size(q); node_size = (uint16_t)q->node_size; }
		if (!pool || !size || has_entry((uint32_t)pool, K_QPOOL)) return;
		if (size > POOL_MAX || g_pools_used + size > POOLS_TOTAL_MAX || IsBadReadPtr(pool, size))
		{
			set_incomplete("task pool %08X (%X bytes) not trackable", (uint32_t)pool, size);
			return;
		}
		if (add_entry((uint32_t)pool, size, K_QPOOL, "task pool", q, node_size)) g_pools_used += size;
	}

	uint32_t *crt_seed() { return (uint32_t *)(((uint8_t *(__cdecl *)())mod::CRT_TLS_FN)() + 0x14); }

	// the ranges known before the tick runs
	bool build_entries(const mod::Module &m)
	{
		g_nent = 0; g_used = 0; g_pools_used = 0;
		g_incomplete = false; g_incomplete_msg[0] = 0;
		g_phase = PH_PRE;
		for (int i = 0; i < mod::engine_region_count; i++)
			add_entry(mod::engine_regions[i].addr, mod::engine_regions[i].size, K_REGION, mod::engine_regions[i].name);
		add_entry(m.data_lo, m.data_hi - m.data_lo, K_REGION, "module globals");
		if (m.extra) add_entry(m.extra, m.extra_size, K_REGION, "module extra cell");
		for (int i = 0; i < m.nstreams; i++) add_entry(m.streams[i].addr, m.streams[i].size, K_REGION, "module stream/cell");
		// ordering table span (render list header buckets + OT) and the OT nodes
		uint32_t ot = *(uint32_t *)mod::RENDER_LIST_BASE_PTR;
		if (ot) add_entry(ot, mod::OT_SPAN_WORDS * 4, K_OT, "ordering table");
		add_entry(mod::SSIGPU_ARENA, mod::SSIGPU_ARENA_SIZE, K_ARENA, "SSIGPU execution arena");
		// packets some effects allocate in the frame packet arena (cursor .. its limit)
		uint32_t fcur = *(uint32_t *)mod::FRAME_PACKET_CUR_PTR, fend = *(uint32_t *)mod::FRAME_PACKET_END_PTR;
		if (fcur && fend > fcur && fend - fcur <= FRAME_WINDOW_MAX && !IsBadReadPtr((void *)fcur, fend - fcur))
			add_entry(fcur, fend - fcur, K_REGION, "frame packet arena");
		// Field_Alloc scratch: effects allocate draw headers there and some engine calls read
		// fields they never set, so the memory above the pointer is part of what a tick sees
		uint32_t scratch = *(uint32_t *)mod::FIELD_ALLOC_PTR;
		if (scratch && !IsBadReadPtr((void *)scratch, mod::FIELD_ALLOC_SCRATCH))
			add_entry(scratch, mod::FIELD_ALLOC_SCRATCH, K_REGION, "Field_Alloc scratch");
		add_entry((uint32_t)crt_seed(), 4, K_REGION, "CRT rand seed");
		for (int i = 0; i < g_nknown; i++) track_queue(g_known[i]);
		return !g_incomplete;
	}

	// memory back to the snapshot (latest entries first: a range tracked mid-run inside an
	// earlier one gets the earlier, pre-tick content)
	void restore_snapshot()
	{
		for (int i = g_nent - 1; i >= 0; i--) memcpy((void *)g_ent[i].addr, g_snap + g_ent[i].off, g_ent[i].size);
	}

	void save_post()
	{
		for (int i = 0; i < g_nent; i++) memcpy(g_post + g_ent[i].off, (const void *)g_ent[i].addr, g_ent[i].size);
	}

	// what an entry must hold after B: its post-A content, or for a range only B touched,
	// its pre-B content
	inline const uint8_t *reference(const Entry &e) { return (e.phase == PH_B ? g_snap : g_post) + e.off; }

	void put_back_A()
	{
		for (int i = 0; i < g_nent; i++) if (g_ent[i].phase == PH_B) memcpy((void *)g_ent[i].addr, g_snap + g_ent[i].off, g_ent[i].size);
		for (int i = 0; i < g_nent; i++) if (g_ent[i].phase != PH_B) memcpy((void *)g_ent[i].addr, g_post + g_ent[i].off, g_ent[i].size);
	}

	// ------------------------------------------------------------------ x87 / stack
	inline uint16_t x87_cw()
	{
		uint16_t cw;
		__asm fnstcw cw
		return cw;
	}
	inline void x87_set_cw(uint16_t cw)
	{
		__asm fldcw cw
	}
	inline void x87_reset()
	{
		__asm fninit
	}

	// Original code reads some locals it never initialised (padding of vectors, unused 4th
	// words...). Both runs must see the same stack bytes, or those reads make them diverge
	// without any port being wrong: the stack below the harness is zeroed before each run.
	__declspec(noinline) void scrub_stack()
	{
		volatile uint8_t buf[0x10000];
		SecureZeroMemory((void *)buf, sizeof(buf));
	}

	// ------------------------------------------------------------------ external call sites
	enum { EXT_OFF = 0, EXT_RECORD, EXT_REPLAY };
	enum { OBS_NONE = 0, OBS_ADD_TASK, OBS_INIT_POOL };
	struct Site { uint32_t addr; int arity; const char *name; int observe; void *stub; uint8_t saved[5]; bool patched; };
	struct Call { int site; uint32_t args[6]; uint32_t ret; uint32_t w_off, w_len; uint16_t cw_after; };
	const int SITES_MAX = 64;
	const int CALLS_MAX = 256;
	const uint32_t WLOG_SIZE = 0x100000;
	Site g_sites[SITES_MAX];
	int g_nsites = 0;
	int g_mode = EXT_OFF, g_depth = 0;
	Call g_calls[CALLS_MAX];
	int g_ncalls = 0, g_replay_i = 0;
	bool g_calls_overflow = false;
	uint8_t g_wlog[WLOG_SIZE];
	uint32_t g_wlog_used = 0;
	bool g_wlog_overflow = false;
	char g_call_msg[256];

	uint32_t site_call(int site, uint32_t *args);
	template<int I> uint32_t __cdecl site_stub(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
	{
		uint32_t a[6] = { a0, a1, a2, a3, a4, a5 };
		return site_call(I, a);
	}
	template<int... I> void fill_stubs(void **out, std::integer_sequence<int, I...>)
	{
		((out[I] = (void *)&site_stub<I>), ...);
	}

	void patch_site(Site &s, bool on)
	{
		if (s.patched == on) return;
		uint8_t *code = (uint8_t *)s.addr;
		DWORD old;
		if (!VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &old)) return;
		if (on)
		{
			memcpy(s.saved, code, 5);
			code[0] = 0xE9;
			*(int32_t *)(code + 1) = (int32_t)((uint8_t *)s.stub - (code + 5));
		}
		else memcpy(code, s.saved, 5);
		VirtualProtect(code, 5, old, &old);
		FlushInstructionCache(GetCurrentProcess(), code, 5);
		s.patched = on;
	}

	void patch_all(bool on)
	{
		for (int i = 0; i < g_nsites; i++) patch_site(g_sites[i], on);
	}

	uint32_t call_through(int site, const uint32_t *a)
	{
		Site &s = g_sites[site];
		patch_site(s, false);
		uint32_t r = ((uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))s.addr)(a[0], a[1], a[2], a[3], a[4], a[5]);
		patch_site(s, true);
		return r;
	}

	void scan_copy()
	{
		for (int i = 0; i < g_nent; i++) memcpy(g_scan + g_ent[i].off, (const void *)g_ent[i].addr, g_ent[i].size);
	}

	// tracked bytes that differ from the pre-call copy -> write log (addr, len, bytes)
	uint32_t diff_to_wlog()
	{
		uint32_t start = g_wlog_used;
		for (int r = 0; r < g_nent; r++)
		{
			const uint8_t *cur = (const uint8_t *)g_ent[r].addr, *pre = g_scan + g_ent[r].off;
			uint32_t size = g_ent[r].size;
			for (uint32_t i = 0; i < size;)
			{
				uint32_t blk = size - i < 64 ? size - i : 64;
				if (memcmp(cur + i, pre + i, blk) == 0) { i += blk; continue; }
				uint32_t a = i;
				while (a < size && cur[a] == pre[a]) a++;
				uint32_t b = a;
				while (b < size && (cur[b] != pre[b] || (b + 1 < size && cur[b + 1] != pre[b + 1]))) b++;
				if (g_wlog_used + 8 + (b - a) > WLOG_SIZE) { g_wlog_overflow = true; return g_wlog_used - start; }
				*(uint32_t *)(g_wlog + g_wlog_used) = g_ent[r].addr + a;
				*(uint32_t *)(g_wlog + g_wlog_used + 4) = b - a;
				memcpy(g_wlog + g_wlog_used + 8, cur + a, b - a);
				g_wlog_used += 8 + (b - a);
				i = b;
			}
		}
		return g_wlog_used - start;
	}

	uint32_t site_call(int site, uint32_t *args)
	{
		const Site &s = g_sites[site];
		if (s.observe)
		{
			// task queue bookkeeping: track the queue before it changes (not inside a recorded
			// external call: what that call does is its own, replayed as a whole)
			if (g_tracking && !(g_mode == EXT_RECORD && g_depth > 0))
			{
				TaskQueue *q = (TaskQueue *)args[0];
				if (s.observe == OBS_ADD_TASK) track_queue(q);
				else track_queue(q, (void *)args[1], args[2] * args[3], args[2]);
			}
			return call_through(site, args);
		}
		if (g_mode == EXT_RECORD)
		{
			if (g_depth > 0) return call_through(site, args); // nested inside a recorded call: part of it
			if (g_ncalls >= CALLS_MAX) { g_calls_overflow = true; return call_through(site, args); }
			g_depth++;
			scan_copy();
			uint32_t r = call_through(site, args);
			Call &c = g_calls[g_ncalls++];
			c.site = site;
			memcpy(c.args, args, sizeof(c.args));
			c.ret = r;
			c.w_off = g_wlog_used;
			c.w_len = diff_to_wlog();
			c.cw_after = x87_cw();
			g_depth--;
			return r;
		}
		if (g_mode == EXT_REPLAY)
		{
			if (g_replay_i >= g_ncalls)
			{
				if (!g_call_msg[0])
					_snprintf_s(g_call_msg, sizeof(g_call_msg), _TRUNCATE, "extra call %s(%08X, %08X, %08X) #%d", s.name, args[0], args[1], args[2], g_replay_i);
				g_replay_i++;
				return 0;
			}
			const Call &c = g_calls[g_replay_i++];
			bool same = c.site == site;
			for (int i = 0; same && i < s.arity; i++) same = c.args[i] == args[i];
			if (!same && !g_call_msg[0])
				_snprintf_s(g_call_msg, sizeof(g_call_msg), _TRUNCATE, "call #%d: original %s(%08X, %08X, %08X), port %s(%08X, %08X, %08X)",
					g_replay_i - 1, g_sites[c.site].name, c.args[0], c.args[1], c.args[2], s.name, args[0], args[1], args[2]);
			x87_set_cw(c.cw_after); // the FPU mode the real call left
			for (uint32_t o = c.w_off; o < c.w_off + c.w_len;)
			{
				uint32_t addr = *(uint32_t *)(g_wlog + o), len = *(uint32_t *)(g_wlog + o + 4);
				memcpy((void *)addr, g_wlog + o + 8, len);
				o += 8 + len;
			}
			return c.ret;
		}
		return call_through(site, args);
	}

	// ------------------------------------------------------------------ queue callback
	void on_queue_seen(TaskQueue *q)
	{
		if (!g_tracking || (g_mode == EXT_RECORD && g_depth > 0)) return;
		track_queue(q);
	}

	// ------------------------------------------------------------------ comparison
	// first differing byte of the tracked memory (port run vs reference); 0 if none
	int describe_memory_diff(char *msg, size_t n)
	{
		int first = -1, ndiff_entries = 0;
		uint32_t i = 0;
		for (int r = 0; r < g_nent; r++)
		{
			const Entry &e = g_ent[r];
			const uint8_t *cur = (const uint8_t *)e.addr, *ref = reference(e);
			if (memcmp(cur, ref, e.size) == 0) continue;
			ndiff_entries++;
			if (first >= 0) continue;
			first = r;
			while (cur[i] == ref[i]) i++;
		}
		if (first < 0) return 0;
		const Entry &e = g_ent[first];
		const uint8_t *cur = (const uint8_t *)e.addr, *ref = reference(e);
		uint32_t ndiff = 0;
		for (uint32_t k = i; k < e.size; k++) if (cur[k] != ref[k]) ndiff++;
		uint32_t w0 = i & ~3u, addr = e.addr + i;
		bool word = w0 + 4 <= e.size;
		int o = _snprintf_s(msg, n, _TRUNCATE, "%s %08X+%X = %08X: original %08X, port %08X (%u bytes differ in it, %d ranges differ)",
			e.name, e.addr, i, addr, word ? *(const uint32_t *)(ref + w0) : ref[i], word ? *(const uint32_t *)(cur + w0) : cur[i], ndiff, ndiff_entries);
		if (o < 0) return (int)strlen(msg);
		int k = 0;
		if (e.kind == K_OT)
			k = _snprintf_s(msg + o, n - o, _TRUNCATE, " [bucket %d]", (int)(i / 4) - 17);
		else if (e.kind == K_ARENA)
			k = _snprintf_s(msg + o, n - o, _TRUNCATE, " [OT node %u +%u]", i / 24, i % 24);
		else if (e.kind == K_QHEADER)
			k = _snprintf_s(msg + o, n - o, _TRUNCATE, " [queue %p: original head %08X tail %08X, port head %08X tail %08X]", e.q,
				*(const uint32_t *)ref, *(const uint32_t *)(ref + 4), *(const uint32_t *)cur, *(const uint32_t *)(cur + 4));
		if (k < 0) return (int)strlen(msg);
		o += k;
		// inside a task pool (often part of the magic buffer): name the node and its task
		for (int r = 0; r < g_nent; r++)
		{
			const Entry &p = g_ent[r];
			if (p.kind != K_QPOOL || !p.node_size || addr < p.addr || addr >= p.addr + p.size) continue;
			uint32_t node = (addr - p.addr) / p.node_size, noff = (addr - p.addr) % p.node_size;
			uint32_t fn = p.node_size >= 12 ? *(const uint32_t *)(p.addr + node * p.node_size + 8) : 0;
			const char *pn = port_name(fn);
			k = _snprintf_s(msg + o, n - o, _TRUNCATE, " [queue %p node %u +%X, task %08X%s%s]", p.q, node, noff, fn, pn ? " = " : "", pn ? pn : "");
			if (k > 0) o += k;
			break;
		}
		return o;
	}

	// Words the original fills with stack garbage and nothing ever reads: the port cannot know
	// them, so the port run takes the original's value before the comparison.
	// GTE control register 4 high half (R33 pad): ComposeAffineTransform (0x56C2F0) leaves the
	// pad word of its result matrix uninitialised and GteSetRotMatrix copies it here.
	void adopt_unread_garbage()
	{
		static const uint32_t words[] = { 0x1CA928E };
		for (uint32_t a : words)
			for (int i = 0; i < g_nent; i++)
			{
				const Entry &e = g_ent[i];
				if (a >= e.addr && a + 2 <= e.addr + e.size)
				{
					*(uint16_t *)a = *(const uint16_t *)(reference(e) + (a - e.addr));
					break;
				}
			}
	}

	const char *compare(int ra, int rb)
	{
		static char msg[1024];
		if (ra != rb) { _snprintf_s(msg, sizeof(msg), _TRUNCATE, "queue return: original %d, port %d", ra, rb); return msg; }
		if (g_call_msg[0]) { _snprintf_s(msg, sizeof(msg), _TRUNCATE, "external %s", g_call_msg); return msg; }
		if (g_replay_i != g_ncalls)
		{
			_snprintf_s(msg, sizeof(msg), _TRUNCATE, "external calls: original made %d, port %d (next expected %s)", g_ncalls, g_replay_i,
				g_replay_i < g_ncalls ? g_sites[g_calls[g_replay_i].site].name : "-");
			return msg;
		}
		if (g_incomplete) { _snprintf_s(msg, sizeof(msg), _TRUNCATE, "port run touched untracked memory: %s", g_incomplete_msg); return msg; }
		adopt_unread_garbage();
		return describe_memory_diff(msg, sizeof(msg)) > 0 ? msg : nullptr;
	}

	// ------------------------------------------------------------------ guarded port run
	uint32_t g_fault_code = 0, g_fault_addr = 0;
	int fault_filter(EXCEPTION_POINTERS *ep)
	{
		g_fault_code = ep->ExceptionRecord->ExceptionCode;
		g_fault_addr = (uint32_t)ep->ExceptionRecord->ExceptionAddress;
		return EXCEPTION_EXECUTE_HANDLER;
	}
	bool run_guarded(TaskQueue *q, int *r)
	{
		__try { *r = g_tick_orig(q); }
		__except (fault_filter(GetExceptionInformation())) { return false; }
		return true;
	}

	// ------------------------------------------------------------------ summon statistics
	struct Summon
	{
		const mod::Module *m;
		TaskQueue *q;
		int effect_id;
		uint32_t ticks, verified, matched, mismatched, first_bad, skipped, faults, logged, original_only;
		bool refused;
	};
	Summon g_s = {};

	void summon_end()
	{
		if (g_s.m && g_s.ticks)
			ffnx_info("FF8 battle fx: %s (effect %d) ended: ticks=%u verified=%u matched=%u mismatched=%u (first at tick %u) skipped=%u faults=%u original-only=%u\n",
				g_s.m->name, g_s.effect_id, g_s.ticks, g_s.verified, g_s.matched, g_s.mismatched, g_s.first_bad, g_s.skipped, g_s.faults, g_s.original_only);
		g_s = {};
		g_nknown = 0;
	}

	void summon_begin(const mod::Module *m, TaskQueue *q, int effect_id)
	{
		g_s = {};
		g_s.m = m; g_s.q = q; g_s.effect_id = effect_id;
		g_nknown = 0;
		g_s.refused = !mod::fits(*m);
		if (g_s.refused)
			ffnx_error("FF8 battle fx: %s (effect %d) does not fit the snapshot (globals %X bytes, streams %X bytes): original code only\n",
				m->name, effect_id, m->data_hi - m->data_lo, mod::streams_bytes(*m));
		else
			ffnx_info("FF8 battle fx: verifying %s (effect %d), queue %p\n", m->name, effect_id, q);
	}

	int run_original_only(TaskQueue *q)
	{
		bool act = g_active;
		g_active = false;
		int r = g_tick_orig(q);
		g_active = act;
		return r;
	}

	// ------------------------------------------------------------------ one verified tick
	int verify_tick(TaskQueue *q, const mod::Module &m)
	{
		g_s.verified++;
		uint32_t tick = g_s.ticks + 1;
		if (!build_entries(m))
		{
			g_s.skipped++;
			if (g_s.logged < MISMATCH_LOG_MAX) { g_s.logged++; ffnx_error("FF8 battle fx: %s tick %u not verifiable: %s\n", m.name, tick, g_incomplete_msg); }
			return run_original_only(q);
		}

		// --- A: original code, external calls recorded ---
		g_ncalls = 0; g_calls_overflow = false;
		g_wlog_used = 0; g_wlog_overflow = false;
		g_phase = PH_A;
		g_tracking = true;
		g_queue_seen = on_queue_seen;
		g_mode = EXT_RECORD;
		g_depth = 0;
		bool act = g_active;
		g_active = false;
		patch_all(true);
		scrub_stack();
		uint16_t cw_a0 = x87_cw();
		int ra = g_tick_orig(q);
		uint16_t cw_a1 = x87_cw();
		patch_all(false);
		g_mode = EXT_OFF;
		g_queue_seen = nullptr;
		g_tracking = false;
		g_active = act;
		save_post();
		if (g_wlog_overflow || g_calls_overflow || g_incomplete)
		{
			// this tick cannot be replayed/compared faithfully: A stays as it is
			g_s.skipped++;
			if (g_s.logged < MISMATCH_LOG_MAX)
			{
				g_s.logged++;
				ffnx_error("FF8 battle fx: %s tick %u not verifiable: %s\n", m.name, tick,
					g_incomplete ? g_incomplete_msg : g_wlog_overflow ? "external call write log full" : "too many external calls");
			}
			g_phase = PH_PRE;
			return ra;
		}

		// --- B: ports, external calls replayed ---
		restore_snapshot();
		g_phase = PH_B;
		g_replay_i = 0;
		g_call_msg[0] = 0;
		g_tracking = true;
		g_queue_seen = on_queue_seen;
		g_mode = EXT_REPLAY;
		g_active = true;
		patch_all(true);
		scrub_stack();
		// the FPU control word is part of the state: the GTE emulation rounds with x87 maths,
		// and the original run's real engine calls (sound, loader) can change it mid-tick
		x87_set_cw(cw_a0);
		int rb = 0;
		bool ok = run_guarded(q, &rb);
		if (!ok) x87_reset(); // a fault can leave values on the x87 stack
		uint16_t cw_b1 = x87_cw();
		x87_set_cw(cw_a1); // leave the FPU as the original run left it
		patch_all(false);
		g_mode = EXT_OFF;
		g_queue_seen = nullptr;
		g_tracking = false;
		g_active = act;
		g_phase = PH_PRE;

		const char *diff = ok ? compare(ra, rb) : "port FAULTED";
		if (!ok)
		{
			g_s.faults++;
			ffnx_error("FF8 battle fx: %s tick %u: port fault %08X at %08X (fault %u)%s\n", m.name, tick, g_fault_code, g_fault_addr, g_s.faults,
				g_s.faults >= FAULTS_MAX ? " - original code only for the rest of this summon" : "");
		}
		if (!diff)
		{
			g_s.matched++;
			return rb;
		}
		g_s.mismatched++;
		if (!g_s.first_bad) g_s.first_bad = tick;
		if (ok && g_s.logged < MISMATCH_LOG_MAX)
		{
			g_s.logged++;
			char calls[200];
			int co = 0;
			calls[0] = 0;
			for (int i = 0; i < g_ncalls && co >= 0 && co < 180; i++)
				co += _snprintf_s(calls + co, sizeof(calls) - co, _TRUNCATE, " %s", g_sites[g_calls[i].site].name);
			ffnx_error("FF8 battle fx: %s tick %u MISMATCH: %s | return %d/%d, x87 cw A %04X->%04X B end %04X, calls:%s\n",
				m.name, tick, diff, ra, rb, cw_a0, cw_a1, cw_b1, calls[0] ? calls : " none");
		}
		put_back_A();
		return ra;
	}

	// ------------------------------------------------------------------ the effect tick hook
	int __cdecl effect_tick(TaskQueue *q)
	{
		int id = *(int *)mod::EFFECT_ID_PTR + 1;
		const mod::Module *m = mod::find_module(id);
		if (!m || !module_ported(id))
		{
			if (g_s.m) summon_end();
			return g_tick_orig(q); // plain dispatch
		}
		if (g_s.m != m || g_s.q != q) { summon_end(); summon_begin(m, q, id); }
		int r;
		if (g_s.refused || g_s.faults >= FAULTS_MAX)
		{
			r = run_original_only(q);
			g_s.original_only++;
		}
		else r = verify_tick(q, *m);
		g_s.ticks++;
		if (r == 0) summon_end(); // queue empty: the effect is over
		return r;
	}
}

	void verify_install()
	{
		if (g_tick_orig) return;
		void *stubs[SITES_MAX];
		fill_stubs(stubs, std::make_integer_sequence<int, SITES_MAX>{});
		g_nsites = 0;
		// task queue bookkeeping (a queue is tracked before a task is added / its pool initialised)
		g_sites[g_nsites++] = { 0x508360, 0, "AddTaskToQueue", OBS_ADD_TASK };
		g_sites[g_nsites++] = { 0x508300, 0, "InitTaskQueuePool", OBS_INIT_POOL };
		for (int i = 0; i < mod::ext_site_count && g_nsites < SITES_MAX; i++)
			g_sites[g_nsites++] = { mod::ext_sites[i].addr, mod::ext_sites[i].arity, mod::ext_sites[i].name, OBS_NONE };
		if (g_nsites - 2 < mod::ext_site_count)
			ffnx_error("FF8 battle fx: the verifier has room for %d external sites, %d listed\n", SITES_MAX - 2, mod::ext_site_count);
		for (int i = 0; i < g_nsites; i++) g_sites[i].stub = stubs[i];

		g_tick_orig = (int (__cdecl *)(TaskQueue *))get_relative_call(0x50093A, 0);
		replace_call(0x50093A, (void *)effect_tick);
		ffnx_info("FF8 battle fx: differential verifier installed (%d modules, %d external sites)\n", mod::module_count, mod::ext_site_count);
	}
}
