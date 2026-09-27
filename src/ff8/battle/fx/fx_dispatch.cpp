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

// Native task queue executor and the switches of the native effect code.
//
// ExecuteTaskQueue (0x508420) runs every node of a task queue once and unlinks the nodes whose
// function returned bit 1 (TASK_END). FFNx replaces it with a twin that calls the native port of
// a node's function when one is registered (a node keeps its original function address) and the
// optional observers (g_queue_seen, g_exec_observer). Options (FFNx.toml):
//   ff8_battle_fx_native  run the ported effect code (default true)
//   ff8_battle_fx_verify  run every tick of a ported effect with the original code too and
//                         keep the original result on any difference (fx_verify.cpp)
//
// The twin is stack-exact. Original effect code reads never-written stack words (a local whose slot
// still holds what an earlier call left there), so when the twin calls a task function the stack
// and the registers are exactly those of the original executor 0x508420..0x50846A: the same four
// saved registers and argument, the same return address (0x508437, the original's instruction after
// "call [esi+8]", patched with a jump back into the twin), nothing written below esp between the
// task calls, eax / ecx / edx / eflags and the x87 / SSE state as the original leaves them, and the
// same instructions for the unlinking. The port lookup and the observers run on a private stack:
// the bottom of the thread's own stack reservation (EXEC_PRIV_LO..EXEC_PRIV_HI above the TEB's
// DeallocationStack), committed page by page through the guard page on first use. Being inside the
// thread's stack range, it keeps structured exception handling working: an exception raised by an
// observer is dispatched to the game stack's handlers as usual. A call that cannot run stack-exact
// (an observer running a queue, i.e. already on the private stack, or a thread with a small stack)
// runs exec_plain, the same executor in plain C++ on the current stack.

#include "fx_port.h"
#include "../../../patch.h"
#include "../../../log.h"
#include "../../../cfg.h"
#include "../../../common.h"

namespace ff8fx
{
	void verify_install(bool hook_tick);       // fx_verify.cpp
	void (*g_queue_seen)(TaskQueue *q) = nullptr; // set by the verifier: queues run during a verified tick
	const ExecObserver *g_exec_observer = nullptr;
	static bool g_installed = false;
}

// ------------------------------------------------------------------ plain executor
// 0x508420 in C++, observers and lookup on the current stack
static int __cdecl exec_plain(ff8fx::TaskQueue *q)
{
	using namespace ff8fx;
	if (g_queue_seen) g_queue_seen(q);
	const ExecObserver *obs = g_exec_observer;
	if (obs && obs->queue_start) obs->queue_start(q);
	TaskNode *prev = nullptr;
	int kept = 0, index = 0;
	for (TaskNode *cur = q->head; cur; cur = cur->next, index++)
	{
		uint32_t cookie = 0;
		if (obs && obs->task_before && !obs->task_before(q, cur, index, &cookie)) break;
		TaskFn fn = (TaskFn)cur->func;
		if (g_active)
			if (void *port = lookup((uint32_t)fn)) fn = (TaskFn)port;
		uint32_t r = fn(cur);
		if (obs && obs->task_after) obs->task_after(q, cur, r, cookie);
		if (r & 2)
		{
			cur->flags = 0;
			if (prev) prev->next = cur->next;
			else q->head = cur->next;
		}
		else
		{
			prev = cur;
			kept++;
		}
	}
	q->tail = prev;
	return kept;
}

// ------------------------------------------------------------------ stack-exact executor
#define EXEC_PRIV_LO 0x10000           // private stack: [DeallocationStack + LO, DeallocationStack + HI)
#define EXEC_PRIV_HI 0x30000
#define EXEC_MIN_RESERVE 0x80000       // smaller thread stacks run exec_plain
#define TEB_STACK_BASE 0x4
#define TEB_STACK_LIMIT 0x8
#define TEB_DEALLOCATION_STACK 0xE0C
#define EXEC_ORIG_RESUME 0x508437      // original executor: the instruction after "call [esi+8]"
#define EXEC_STOP 1                    // exec_before: an observer stopped the queue

// Per-run state (observer, node index, task_before cookie), keyed by the run's frame: the game esp
// after its four pushes. A nested run (a task running a queue) has a lower esp than its parent;
// a frame left by a run that did not return (an exception) is dropped when a later run passes it.
struct ExecFrame { uint32_t esp; const ff8fx::ExecObserver *obs; int index; uint32_t cookie; };
static const int EXEC_FRAMES_MAX = 64;
static ExecFrame exec_frames[EXEC_FRAMES_MAX];
static int exec_nframes = 0;

static ExecFrame *exec_frame(uint32_t esp)
{
	while (exec_nframes > 0 && exec_frames[exec_nframes - 1].esp < esp) exec_nframes--;
	return exec_nframes > 0 && exec_frames[exec_nframes - 1].esp == esp ? &exec_frames[exec_nframes - 1] : nullptr;
}

// the hooks below run on the private stack
static void __cdecl exec_start(uint32_t esp, ff8fx::TaskQueue *q)
{
	using namespace ff8fx;
	while (exec_nframes > 0 && exec_frames[exec_nframes - 1].esp <= esp) exec_nframes--;
	if (g_queue_seen) g_queue_seen(q);
	const ExecObserver *obs = g_exec_observer;
	if (exec_nframes < EXEC_FRAMES_MAX) exec_frames[exec_nframes++] = { esp, obs, 0, 0 };
	if (obs && obs->queue_start) obs->queue_start(q);
}

// the function to call for node n (its port when one is active), or EXEC_STOP
static uint32_t __cdecl exec_before(uint32_t esp, ff8fx::TaskQueue *q, ff8fx::TaskNode *n)
{
	using namespace ff8fx;
	if (ExecFrame *f = exec_frame(esp))
	{
		f->cookie = 0;
		if (f->obs && f->obs->task_before && !f->obs->task_before(q, n, f->index, &f->cookie)) return EXEC_STOP;
	}
	uint32_t fn = (uint32_t)n->func;
	if (g_active)
		if (void *port = lookup(fn)) fn = (uint32_t)port;
	return fn;
}

static void __cdecl exec_after(uint32_t esp, ff8fx::TaskQueue *q, ff8fx::TaskNode *n, uint32_t r)
{
	ExecFrame *f = exec_frame(esp);
	if (!f) return;
	if (f->obs && f->obs->task_after) f->obs->task_after(q, n, r, f->cookie);
	f->index++;
}

static void __cdecl exec_end(uint32_t esp)
{
	while (exec_nframes > 0 && exec_frames[exec_nframes - 1].esp <= esp) exec_nframes--;
}

// scratch of the switches (one game thread; each value lives for a few instructions)
static uint32_t exec_t_esp, exec_t_eax, exec_t_ecx, exec_t_fn;

// esp -> top of the private stack, which then holds the game esp, eflags, eax, ecx, edx and the
// x87 / SSE state (fxsave, 16-byte aligned: the top is 64 KB aligned); esp = top - 0x220.
// Nothing is written to the game stack.
#define EXEC_TO_PRIVATE_STACK 	__asm mov exec_t_esp, esp 	__asm mov exec_t_eax, eax 	__asm mov eax, dword ptr fs:[TEB_DEALLOCATION_STACK] 	__asm lea esp, [eax + EXEC_PRIV_HI] 	__asm mov eax, exec_t_eax 	__asm push exec_t_esp 	__asm pushfd 	__asm push eax 	__asm push ecx 	__asm push edx 	__asm sub esp, 0x20C 	__asm fxsave [esp]
// back to the game stack, every register, flag and the x87 / SSE state as before
// EXEC_TO_PRIVATE_STACK
#define EXEC_TO_GAME_STACK 	__asm fxrstor [esp] 	__asm add esp, 0x20C 	__asm pop edx 	__asm pop ecx 	__asm pop eax 	__asm popfd 	__asm mov esp, [esp]
// offset of the saved game esp from esp right after EXEC_TO_PRIVATE_STACK
#define EXEC_SAVED_ESP 0x21C

static void exec_task();
static void exec_done();

// 0x508420
static __declspec(naked) int __cdecl exec_entry(ff8fx::TaskQueue *)
{
	// stack-exact on this thread's stack above the private stack, with a large enough reservation
	__asm mov exec_t_eax, eax
	__asm mov eax, dword ptr fs:[TEB_DEALLOCATION_STACK]
	__asm test eax, eax
	__asm jz plain
	__asm mov eax, dword ptr fs:[TEB_STACK_BASE]
	__asm sub eax, dword ptr fs:[TEB_DEALLOCATION_STACK]
	__asm cmp eax, EXEC_MIN_RESERVE
	__asm jb plain
	__asm cmp esp, dword ptr fs:[TEB_STACK_BASE]
	__asm jae plain
	__asm mov eax, dword ptr fs:[TEB_DEALLOCATION_STACK]
	__asm add eax, EXEC_PRIV_HI
	__asm cmp esp, eax
	__asm jbe plain
	__asm sub eax, EXEC_PRIV_HI - EXEC_PRIV_LO
	__asm cmp dword ptr fs:[TEB_STACK_LIMIT], eax
	__asm ja commit
exact:
	__asm mov eax, exec_t_eax
	__asm push ebx
	__asm push ebp
	__asm mov ebp, [esp + 0xC]
	__asm push esi
	__asm push edi
	EXEC_TO_PRIVATE_STACK
	__asm push ebp
	__asm push dword ptr [esp + EXEC_SAVED_ESP + 4]
	__asm call exec_start
	__asm add esp, 8
	EXEC_TO_GAME_STACK
	__asm xor edi, edi
	__asm mov esi, [ebp]
	__asm xor ebx, ebx
	__asm test esi, esi
	__asm jne first_task
	__asm jmp exec_done
first_task:
	__asm jmp exec_task
plain:
	__asm mov eax, exec_t_eax
	__asm jmp exec_plain
commit:
	// first use on this thread: commit the private stack by reading every page from the stack
	// limit down (each read moves the guard page one page down)
	__asm mov exec_t_ecx, ecx
	__asm mov ecx, dword ptr fs:[TEB_STACK_LIMIT]
commit_page:
	__asm sub ecx, 0x1000
	__asm test dword ptr [ecx], eax
	__asm cmp ecx, eax
	__asm ja commit_page
	__asm mov ecx, exec_t_ecx
	__asm jmp exact
}

// 0x508433: call the function of node esi
static __declspec(naked) void exec_task()
{
	EXEC_TO_PRIVATE_STACK
	__asm push esi
	__asm push ebp
	__asm push dword ptr [esp + EXEC_SAVED_ESP + 8]
	__asm call exec_before
	__asm add esp, 12
	__asm mov exec_t_fn, eax
	__asm cmp eax, EXEC_STOP
	__asm jne call_task
	EXEC_TO_GAME_STACK
	__asm jmp exec_done
call_task:
	EXEC_TO_GAME_STACK
	__asm push esi
	__asm push EXEC_ORIG_RESUME
	__asm jmp dword ptr [exec_t_fn]
}

// 0x508437 (patched to jump here): the task function returned
static __declspec(naked) void exec_resume()
{
	__asm add esp, 4
	EXEC_TO_PRIVATE_STACK
	__asm push eax
	__asm push esi
	__asm push ebp
	__asm push dword ptr [esp + EXEC_SAVED_ESP + 12]
	__asm call exec_after
	__asm add esp, 16
	EXEC_TO_GAME_STACK
	__asm test al, 2
	__asm je keep
	__asm test edi, edi
	__asm mov word ptr [esi], 0
	__asm je unlink_head
	__asm mov eax, [esi + 4]
	__asm mov [edi + 4], eax
	__asm jmp next
unlink_head:
	__asm mov ecx, [esi + 4]
	__asm mov [ebp], ecx
	__asm jmp next
keep:
	__asm mov edi, esi
	__asm inc ebx
next:
	__asm mov esi, [esi + 4]
	__asm test esi, esi
	__asm je last_task
	__asm jmp exec_task
last_task:
	__asm jmp exec_done
}

// 0x508461
static __declspec(naked) void exec_done()
{
	__asm mov [ebp + 4], edi
	EXEC_TO_PRIVATE_STACK
	__asm push dword ptr [esp + EXEC_SAVED_ESP]
	__asm call exec_end
	__asm add esp, 4
	EXEC_TO_GAME_STACK
	__asm pop edi
	__asm pop esi
	__asm mov eax, ebx
	__asm pop ebp
	__asm pop ebx
	__asm ret
}

namespace ff8fx
{
	static void install_executor()
	{
		replace_function(0x508420, (void *)exec_entry);
		replace_function(EXEC_ORIG_RESUME, (void *)exec_resume);
	}

	void install()
	{
		if (g_installed || !FF8_US_VERSION || !ff8_battle_fx_native) return;
		g_installed = true;
		register_all();
		install_executor();
		g_active = true;
		ffnx_info("FF8 battle fx: native effect code on%s\n", ff8_battle_fx_verify ? ", verified against the original" : "");
		if (ff8_battle_fx_verify)
		{
			prim::install_verify();
			verify_install(true);
		}
	}

	void install_hosted()
	{
		if (g_installed || !FF8_US_VERSION) return;
		g_installed = true;
		register_all();
		// the executor is installed even with the ports off: the host may observe it
		install_executor();
		g_active = false;
		if (!ff8_battle_fx_native) return;
		ffnx_info("FF8 battle fx: native effect code on (hosted)%s\n", ff8_battle_fx_verify ? ", verified against the original" : "");
		if (ff8_battle_fx_verify)
		{
			prim::install_verify();
			verify_install(false);
		}
	}
}
