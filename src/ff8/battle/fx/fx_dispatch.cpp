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
// function returned bit 1 (TASK_END). FFNx replaces it with this instruction-exact twin, which
// calls the native port of a node's function when one is registered (a node keeps its
// original function address). Options (FFNx.toml):
//   ff8_battle_fx_native  run the ported effect code (default true)
//   ff8_battle_fx_verify  run every tick of a ported effect with the original code too and
//                         keep the original result on any difference (fx_verify.cpp)

#include "fx_port.h"
#include "../../../patch.h"
#include "../../../log.h"
#include "../../../cfg.h"
#include "../../../common.h"

namespace ff8fx
{
	void verify_install();                     // fx_verify.cpp
	void (*g_queue_seen)(TaskQueue *q) = nullptr; // set by the verifier: queues run during a verified tick

	// 0x508420, instruction for instruction
	static int __cdecl ExecuteTaskQueue(TaskQueue *q)
	{
		if (g_queue_seen) g_queue_seen(q);
		TaskNode *prev = nullptr;
		int kept = 0;
		for (TaskNode *cur = q->head; cur; cur = cur->next)
		{
			TaskFn fn = (TaskFn)cur->func;
			if (g_active)
				if (void *port = lookup((uint32_t)fn)) fn = (TaskFn)port;
			uint32_t r = fn(cur);
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

	void install()
	{
		if (!FF8_US_VERSION || !ff8_battle_fx_native) return;
		register_all();
		replace_function(0x508420, (void *)ExecuteTaskQueue);
		g_active = true;
		ffnx_info("FF8 battle fx: native effect code on%s\n", ff8_battle_fx_verify ? ", verified against the original" : "");
		if (ff8_battle_fx_verify)
		{
			prim::install_verify();
			verify_install();
		}
	}
}
