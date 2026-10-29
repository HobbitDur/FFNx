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

#include "battle_memory.h"

#include "../../patch.h"
#include "../../log.h"

#include <stdint.h>
#include <string.h>

// The battle memory regions are fixed addresses held ONLY in the table 0xB6D070-0xB6D0A4 (no code or
// data holds them otherwise, no loader checks a size), so repointing the table moves them:
//   0xB6D078/7C/80  monster model arena: stage + every distinct monster   204,800 B
//   0xB6D084        party model regions: body + weapon per slot, 3 slots   65,536 B each
//   0xB6D08C        staging buffer: every battle file is read into it whole 294,912 B
//   0xB6D094        GF packet double buffer, the staging buffer in vanilla too (time shared)
// Converted to 30 fps (section 3 x1.69 on average), every character pair overflows its slot, 5 monsters
// the staging buffer and 58 encounters the arena; an overflow runs silently into the next region.
// The other entries (packet buffer, field/kernel/scene.out buffers, extra-anim slots) stay vanilla.
namespace
{
	constexpr uint32_t ARENA_SIZE = 0x80000;
	constexpr uint32_t PARTY_SLOT_SHIFT = 17;            // vanilla 16: one slot = 0x10000
	constexpr uint32_t PARTY_SLOT_SIZE = 1u << PARTY_SLOT_SHIFT;
	constexpr uint32_t STAGING_SIZE = 0x80000;

	alignas(16) uint8_t battle_memory[ARENA_SIZE + 3 * PARTY_SLOT_SIZE + STAGING_SIZE];

	struct CodeSite
	{
		uint32_t address;
		uint8_t size;            // instruction length
		uint8_t expected[6];     // vanilla bytes
		uint8_t operand;         // offset of the operand to patch
		uint32_t value;          // new operand (byte for a shift count, dword otherwise)
	};

	// The party slot stride: slot << 16 when a party model is loaded, and the GF cinematic, which
	// backs a party slot up into slot 2's region (base + 0x20000) and back, 0x10000 bytes each way.
	const CodeSite party_stride_sites[] = {
		{ 0x507850, 3, { 0xC1, 0xE6, 0x10 }, 2, PARTY_SLOT_SHIFT },                    // shl esi, 10h (Battle_isLoadSquallEtc)
		{ 0x507AE6, 3, { 0xC1, 0xE6, 0x10 }, 2, PARTY_SLOT_SHIFT },                    // shl esi, 10h (Edea load)
		{ 0x50B483, 5, { 0x68, 0x00, 0x00, 0x01, 0x00 }, 1, PARTY_SLOT_SIZE },         // push 10000h (GF cinematic)
		{ 0x50B488, 6, { 0x8D, 0x91, 0x00, 0x00, 0x02, 0x00 }, 2, 2 * PARTY_SLOT_SIZE }, // lea edx, [ecx+20000h]
		{ 0x50B493, 3, { 0xC1, 0xE2, 0x10 }, 2, PARTY_SLOT_SHIFT },                    // shl edx, 10h
		{ 0x50B50A, 5, { 0x68, 0x00, 0x00, 0x01, 0x00 }, 1, PARTY_SLOT_SIZE },         // push 10000h
		{ 0x50B516, 3, { 0xC1, 0xE1, 0x10 }, 2, PARTY_SLOT_SHIFT },                    // shl ecx, 10h
		{ 0x50B51B, 5, { 0x05, 0x00, 0x00, 0x02, 0x00 }, 1, 2 * PARTY_SLOT_SIZE },     // add eax, 20000h
	};
}

void ff8_battle_memory_enlarge()
{
	for (const CodeSite &site : party_stride_sites)
	{
		if (memcmp((const void *)site.address, site.expected, site.size) != 0)
		{
			ffnx_error("battle memory: unexpected code at 0x%X, the vanilla battle memory is kept\n", site.address);
			return;
		}
	}
	for (const CodeSite &site : party_stride_sites)
	{
		if (site.size == 3) patch_code_byte(site.address + site.operand, uint8_t(site.value));
		else patch_code_dword(site.address + site.operand, site.value);
	}

	const uint32_t arena = uint32_t(battle_memory);
	const uint32_t party = arena + ARENA_SIZE;
	const uint32_t staging = party + 3 * PARTY_SLOT_SIZE;
	patch_code_dword(0xB6D078, arena);              // arena start
	patch_code_dword(0xB6D07C, arena);              // stage load address = arena start
	patch_code_dword(0xB6D080, party);              // arena end
	patch_code_dword(0xB6D084, party);              // party model regions
	patch_code_dword(0xB6D088, party + PARTY_SLOT_SIZE);
	patch_code_dword(0xB6D08C, staging);            // staging buffer
	patch_code_dword(0xB6D094, staging);            // GF packet double buffer (aliases the staging buffer, as in vanilla)
	ffnx_info("battle memory: arena %u KB, party slots %u KB, staging buffer %u KB\n",
		ARENA_SIZE / 1024, PARTY_SLOT_SIZE / 1024, STAGING_SIZE / 1024);
}
