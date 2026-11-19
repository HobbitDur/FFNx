/****************************************************************************/
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

#include "mag_common.h"

namespace ff8fx
{
namespace griever344
{
	using namespace eng;
	using namespace magc;

	inline CastContext *&Ctx() { return var<CastContext *>(us(0x24E0BC0)); }
	inline uint32_t &CasterSlot() { return var<uint32_t>(us(0x24E0BC4)); }
	inline uint8_t &FanfareDone() { return var<uint8_t>(us(0x24DFD98)); }
	inline uint32_t &PacketCursor() { return var<uint32_t>(us(0x24F0BC8)); }
	inline TaskQueue *QCue() { return (TaskQueue *)us(0x24DFDA0); }

	static const uint32_t ORIG_RootTask = 0x62A7A0;
	static const uint32_t ORIG_CueTask = 0x62A6D0;
	static const uint32_t ARENA_A = 0x24E8BC8;
	static const uint32_t ARENA_B = 0x24E0BC8;
	static const uint32_t FANFARE_DATA = 0xDEE388;
	static const uint32_t CAMERA_ANIM = 0xE106D8;

	inline void Music_SetVolumeTrans(uint32_t a1, uint32_t a2, uint32_t a3) { fn<void (__cdecl *)(uint32_t, uint32_t, uint32_t)>(us(0x46BBC0))(a1, a2, a3); }
	inline void QueueChainTransformation(uint8_t *entity, uint32_t chain) { fn<void (__cdecl *)(uint8_t *, uint32_t)>(us(0x505C00))(entity, chain); }
	inline void BS_R0win_QueueVictoryFanfare(uint32_t data, uint8_t *done) { fn<void (__cdecl *)(uint32_t, uint8_t *)>(us(0x501A20))(data, done); }
	inline void BS_Music_SetChannelVolumeCommand(uint32_t a1, uint32_t a2) { fn<void (__cdecl *)(uint32_t, uint32_t)>(us(0x501B10))(a1, a2); }

#pragma pack(push, 1)
	struct RootNode
	{
		TaskNode hdr;
		int16_t counter;
		uint16_t pad0E;
	};
	struct CueNode
	{
		TaskNode hdr;
		int16_t counter;
		uint8_t pad0E[0x16];
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(CueNode) == 0x24, "344 nodes");
}
}

namespace ff8fx
{
namespace griever344
{
	static uint32_t __cdecl CueTask(TaskNode *n)
	{
		CueNode *c = (CueNode *)n;
		if (UpdateFlags() & 0x201) return 0;
		if (c->counter == 0) Music_SetVolumeTrans(0, 0x78, 0);
		if (c->counter == 0x1E) BS_R0win_QueueVictoryFanfare(us(FANFARE_DATA), &FanfareDone());
		if (FanfareDone() == 0 && c->counter == 0x1F) return 0;
		if (c->counter == 0x20)
		{
			fn<void (__cdecl *)(const void *)>(us(0x5099A0))((const void *)us(CAMERA_ANIM));
			QueueChainTransformation(Entity(CasterSlot()), 0x14);
		}
		if (c->counter == 0x22)
		{
			BS_Music_SetChannelVolumeCommand(0, 0x7F);
			if (c->counter == 0x22) ApplyActionResultToTarget(Ctx()->actions->targets);
		}
		c->counter++;
		return c->counter > 0x23 ? TASK_END : 0;
	}

	static uint32_t __cdecl RootTask(TaskNode *n)
	{
		RootNode *r = (RootNode *)n;
		PacketCursor() = us(ARENA_A);
		if (!(r->counter & 1)) PacketCursor() = us(ARENA_B);
		const int left = ExecuteTaskQueue(QCue());
		r->counter++;
		return left ? 0 : TASK_END;
	}
}

	void register_mag344_griever_368()
	{
		register_port(griever344::ORIG_RootTask, (void *)griever344::RootTask, "344 RootTask", 344);
		register_port(griever344::ORIG_CueTask, (void *)griever344::CueTask, "344 CueTask", 344);
	}
}

