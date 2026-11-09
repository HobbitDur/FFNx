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

// 30 fps part of gfc_palshift.inc (draw 47 PaletteHsvShift): included INSIDE the clone's namespace
// by the module's _held.inc.
//
// Draw 47 writes four colour words outside the engine state: 0xB8BA00 / 0xB8BA04 and the colour
// word +0x28 of battle entities 3 / 4 (model_palette, read when the entity task renders the model,
// after the effect tick in the same frame: 0x50093A effect tree, then 0x500971 entity queue). A
// held frame runs draw 47 on the in-between bone (outPos / outAngle interpolated by the engine's
// held frame), so the entities are drawn in the in-between colour on held frames. The next real
// tick starts by putting the real tick's words back (PalShiftRestore, called by the module's
// SequenceTask before the engine tick): each word the held frame left untouched since, so the
// real frame's code sees exactly what vanilla's tick left there (the tick's own draw 47 then writes
// its colour as vanilla does).

	static const uint32_t PS_WORDS[4] = { 0xB8BA00, 0xB8BA04, 0x1D974BC, 0x1D97558 };
	static struct { bool held; uint32_t real[4], drawn[4]; } g_ps = {};

	// end of every draw 47: real tick -> the words it leaves; held frame -> the words it drew
	static void PalShiftNote()
	{
		if (held_predicting()) return;
		uint32_t *dst = g_held.active ? g_ps.drawn : g_ps.real;
		for (int k = 0; k < 4; k++) dst[k] = MEM<uint32_t>(PS_WORDS[k]);
		g_ps.held = g_held.active;
	}

	// start of the next real tick: the real tick's words back where a held frame drew
	static void PalShiftRestore()
	{
		if (!g_ps.held) return;
		g_ps.held = false;
		for (int k = 0; k < 4; k++)
			if (MEM<uint32_t>(PS_WORDS[k]) == g_ps.drawn[k]) MEM<uint32_t>(PS_WORDS[k]) = g_ps.real[k];
	}
