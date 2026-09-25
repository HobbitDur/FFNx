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

#include "kernel_command.h"
#include "kernel_magic.h"

#include "../ff8.h"
#include "../patch.h"
#include "../globals.h"
#include "../common.h"
#include "../log.h"

#include <stdint.h>
#include <string.h>

// -------------------------------------------------------------------------
// AddMoreCommand - lets kernel.bin hold more battle commands than the vanilla
// 39, and more command ability data entries than the vanilla 12.
//
// A command ability (kernel section 12) names a battle command (section 0) by
// id. The battle command carries its name, menu flags, target info and an
// abilityDataID into the command ability data (section 10), which holds the
// special action, hit count, attack type and power, element and statuses.
// computeCommandAction switches on the command id; ten ids (Defend, Mad Rush,
// Treatment, Recover, Revive, Doom, Kamikaze, Trance, LV Down, LV Up) share one
// case that takes everything from that data entry. A new command id has no case
// of its own, so it is run as one of those ten: its battle command entry stands
// in for theirs for the length of the call.
//
// Both sections are read at a handful of places, always through their fixed
// address. We keep them FFNx-side and repoint every operand, found by scanning
// the functions that own them and checked against the count measured on the US
// 1.2 build - a build where any count differs leaves the feature off.
//
// Modder contract: each section lists its vanilla entries first, then the new
// ones. A new battle command sets abilityDataID to one of the data entries, and
// its last byte (unused in vanilla) to the family command it behaves like, or 0
// for Mad Rush. Its name and description go in the battle command text section.
// A command ability then points at the new command id.
// -------------------------------------------------------------------------

#define KERNEL_COMMAND_SECTION        0
#define KERNEL_COMMAND_DATA_SECTION   10
#define COMMAND_ENTRY_SIZE            8
#define COMMAND_DATA_ENTRY_SIZE       16
#define VANILLA_COMMAND_COUNT         39
#define VANILLA_COMMAND_DATA_COUNT    12
// Command ids from 0xB0 up are the engine's own (Ultimecia, Renzokuken finisher,
// G-Force, Doom countdown...).
#define MAX_COMMAND_COUNT             0xB0
// abilityDataID is a byte and 0xFF means "no data".
#define NO_COMMAND_DATA               0xFF
#define MAX_COMMAND_DATA_COUNT        0xFF
// Battle command entry fields.
#define COMMAND_ABILITY_DATA_OFF      4
#define COMMAND_BEHAVES_LIKE_OFF      7
// Kernel buffer offsets of the two sections in the vanilla layout.
#define COMMAND_BLOCK_OFFSET          0xE4
#define COMMAND_DATA_BLOCK_OFFSET     0x4020

#define COMMAND_MAD_RUSH              0x18

// The data-driven family, the only case a new command can run through.
static const uint8_t data_driven_commands[] = { 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1E, 0x1F, 0x20, 0x21, 0x22 };

// ---- state --------------------------------------------------------------
static uint8_t ff8_command_table[MAX_COMMAND_COUNT][COMMAND_ENTRY_SIZE];
static uint8_t ff8_command_data_table[MAX_COMMAND_DATA_COUNT][COMMAND_DATA_ENTRY_SIZE];
static uint8_t ff8_command_behaves_like[MAX_COMMAND_COUNT];
static int ff8_command_count = VANILLA_COMMAND_COUNT;
static int ff8_command_data_count = VANILLA_COMMAND_DATA_COUNT;
static bool ff8_command_armed = false;
static bool ff8_command_checked = false;
static bool ff8_command_supported = false;

// ---- operand sites ------------------------------------------------------
// Each function that reads a table, how far to scan it, and how many operands
// into each table it holds. The scan length is the function's size on US 1.2.
struct command_reader
{
	uint32_t start;
	uint32_t length;
	int command_reads;
	int data_reads;
	const char *what;
};

enum command_reader_id
{
	READER_ABILITY_NAME,
	READER_COMMAND_DESCRIPTION,
	READER_SEVERAL_HIT,
	READER_LINKED_STOCK,
	READER_LIMIT_MENU,
	READER_DISPATCHER,
	READER_APPLY_DAMAGE,
	READER_RESET_PARSE_CHARA,
	READER_COUNT
};

static command_reader readers[READER_COUNT] = {
	{ 0, 0x02B, 1, 0, "getAddressAbilityName" },
	{ 0, 0x02B, 1, 0, "getBattleCommandDescription" },
	{ 0, 0x076, 1, 0, "manageSeveralHitData" },
	{ 0, 0x0D0, 2, 0, "linkedStockFieldCharData" },
	{ 0, 0x258, 1, 0, "BuildLimitCommandMenu" },
	{ 0, 0x11F7, 3, 6, "computeCommandAction" },
	{ 0, 0x13ED, 0, 8, "Battle_applyDamage" },
	{ 0, 0x3A8, 7, 1, "ResetAndParseBattleAndFieldCharacter" },
};

static uint32_t k_command, k_command_end, k_command_data, k_command_data_end;

// ---- init ---------------------------------------------------------------
static void ff8_kernel_command_find_externals()
{
	uint32_t kernel = uint32_t(ff8_externals.unk_1CF3E48);
	k_command = kernel + COMMAND_BLOCK_OFFSET;
	k_command_end = k_command + VANILLA_COMMAND_COUNT * COMMAND_ENTRY_SIZE;
	k_command_data = kernel + COMMAND_DATA_BLOCK_OFFSET;
	k_command_data_end = k_command_data + VANILLA_COMMAND_DATA_COUNT * COMMAND_DATA_ENTRY_SIZE;

	// The two text getters sit after getMagicText; the three battle helpers after
	// the draw list visibility function - the same neighbours AddMoreMagic uses.
	uint32_t magic_name_getter = ff8_kernel_magic_name_getter();
	uint32_t spell_visibility = ff8_kernel_magic_spell_visibility_fn();

	readers[READER_ABILITY_NAME].start = magic_name_getter ? magic_name_getter + 0x260 : 0;
	readers[READER_COMMAND_DESCRIPTION].start = magic_name_getter ? magic_name_getter + 0x290 : 0;
	readers[READER_SEVERAL_HIT].start = spell_visibility ? spell_visibility + 0x180 : 0;
	readers[READER_LINKED_STOCK].start = spell_visibility ? spell_visibility + 0x340 : 0;
	readers[READER_LIMIT_MENU].start = spell_visibility ? spell_visibility + 0x540 : 0;
	readers[READER_DISPATCHER].start = ff8_externals.battle_sub_48D200;
	readers[READER_APPLY_DAMAGE].start = ff8_externals.battle_sub_48FE20;
	readers[READER_RESET_PARSE_CHARA].start = get_relative_call(ff8_externals.sub_48B7E0, 0x94);
}

// Every dword inside the function that points into [lo, hi). Writes up to max
// operand addresses to out and returns how many it found.
static int scan_operands(const command_reader &reader, uint32_t lo, uint32_t hi, uint32_t *out, int max)
{
	int found = 0;

	for (uint32_t i = 0; i + 4 <= reader.length; ++i)
	{
		uint32_t value = *(uint32_t *)(reader.start + i);

		if (value >= lo && value < hi)
		{
			if (found < max) out[found] = reader.start + i;
			++found;
		}
	}

	return found;
}

// Check every reader before touching any of them: a bad anchor must disable the
// feature, never half-patch the exe.
static bool ff8_kernel_command_validate()
{
	bool ok = true;
	uint32_t sites[16];

	for (const command_reader &reader : readers)
	{
		if (!reader.start)
		{
			ffnx_warning("AddMoreCommand: could not resolve %s, extension disabled.\n", reader.what);
			return false;
		}

		int command_reads = scan_operands(reader, k_command, k_command_end, sites, 16);
		int data_reads = scan_operands(reader, k_command_data, k_command_data_end, sites, 16);

		if (command_reads != reader.command_reads || data_reads != reader.data_reads)
		{
			ffnx_warning("AddMoreCommand: %s at 0x%X reads the command tables %d/%d times, expected %d/%d.\n",
				reader.what, reader.start, command_reads, data_reads, reader.command_reads, reader.data_reads);
			ok = false;
		}
	}

	return ok;
}

// ---- patch application --------------------------------------------------
static void repoint(uint32_t lo, uint32_t hi, uint32_t table)
{
	uint32_t sites[16];

	for (const command_reader &reader : readers)
	{
		int count = scan_operands(reader, lo, hi, sites, 16);

		for (int i = 0; i < count && i < 16; ++i)
			patch_code_dword(sites[i], (DWORD)(table + (*(uint32_t *)sites[i] - lo)));
	}
}

static void ff8_kernel_command_arm()
{
	if (ff8_command_armed || !ff8_command_supported) return;
	ff8_command_armed = true;

	repoint(k_command, k_command_end, (uint32_t)&ff8_command_table[0][0]);
	repoint(k_command_data, k_command_data_end, (uint32_t)&ff8_command_data_table[0][0]);

	if (ff8_command_count > VANILLA_COMMAND_COUNT)
		ff8_kernel_magic_hook_dispatcher();
}

// ---- kernel.bin load ----------------------------------------------------
bool ff8_kernel_command_section_may_grow(int section)
{
	return section == KERNEL_COMMAND_SECTION || section == KERNEL_COMMAND_DATA_SECTION;
}

static bool is_data_driven(uint8_t command)
{
	for (uint8_t family : data_driven_commands)
		if (family == command) return true;

	return false;
}

bool ff8_kernel_command_read(const char *stash, const uint32_t *offsets, int size)
{
	uint32_t command_bytes = offsets[KERNEL_COMMAND_SECTION + 1] - offsets[KERNEL_COMMAND_SECTION];
	uint32_t data_bytes = offsets[KERNEL_COMMAND_DATA_SECTION + 1] - offsets[KERNEL_COMMAND_DATA_SECTION];
	int commands = (int)(command_bytes / COMMAND_ENTRY_SIZE);
	int data = (int)(data_bytes / COMMAND_DATA_ENTRY_SIZE);

	// A stock kernel.bin leaves here, before anything is resolved or patched.
	if (commands == VANILLA_COMMAND_COUNT && data == VANILLA_COMMAND_DATA_COUNT) return false;

	if (command_bytes % COMMAND_ENTRY_SIZE || data_bytes % COMMAND_DATA_ENTRY_SIZE
		|| commands < VANILLA_COMMAND_COUNT || commands > MAX_COMMAND_COUNT
		|| data < VANILLA_COMMAND_DATA_COUNT || data > MAX_COMMAND_DATA_COUNT)
	{
		ffnx_warning("AddMoreCommand: kernel.bin has %d battle commands (%d-%d allowed) and %d command ability data entries (%d-%d allowed) - ignoring the extension.\n",
			commands, VANILLA_COMMAND_COUNT, MAX_COMMAND_COUNT, data, VANILLA_COMMAND_DATA_COUNT, MAX_COMMAND_DATA_COUNT);
		return false;
	}

	const uint8_t *command_section = (const uint8_t *)stash + offsets[KERNEL_COMMAND_SECTION];
	uint8_t behaves_like[MAX_COMMAND_COUNT] = {};

	for (int id = 0; id < commands; ++id)
	{
		const uint8_t *entry = command_section + COMMAND_ENTRY_SIZE * id;
		uint8_t data_id = entry[COMMAND_ABILITY_DATA_OFF];

		if (data_id != NO_COMMAND_DATA && data_id >= data)
		{
			ffnx_warning("AddMoreCommand: battle command %d uses command ability data %d, but there are only %d - ignoring the extension.\n", id, data_id, data);
			return false;
		}

		if (id < VANILLA_COMMAND_COUNT) continue;

		if (data_id == NO_COMMAND_DATA)
		{
			ffnx_warning("AddMoreCommand: battle command %d has no command ability data, which a new command needs - ignoring the extension.\n", id);
			return false;
		}

		uint8_t family = entry[COMMAND_BEHAVES_LIKE_OFF] ? entry[COMMAND_BEHAVES_LIKE_OFF] : COMMAND_MAD_RUSH;

		if (!is_data_driven(family))
		{
			ffnx_warning("AddMoreCommand: battle command %d behaves like command %d, which does not take its effect from command ability data - ignoring the extension.\n", id, family);
			return false;
		}

		behaves_like[id] = family;
	}

	// First extended file seen: only now is it worth resolving and checking.
	if (!ff8_command_checked)
	{
		ff8_command_checked = true;
		ff8_kernel_command_find_externals();
		ff8_command_supported = ff8_kernel_command_validate();

		if (!ff8_command_supported)
			ffnx_warning("AddMoreCommand: this build is not supported, extension disabled.\n");
	}

	if (!ff8_command_supported) return false;

	if (ff8_command_armed && (commands != ff8_command_count || data != ff8_command_data_count))
	{
		ffnx_warning("AddMoreCommand: kernel.bin command layout changed after the patches were applied - ignoring the new one.\n");
		return true;
	}

	memcpy(ff8_command_table, command_section, commands * COMMAND_ENTRY_SIZE);
	memcpy(ff8_command_data_table, stash + offsets[KERNEL_COMMAND_DATA_SECTION], data * COMMAND_DATA_ENTRY_SIZE);
	memcpy(ff8_command_behaves_like, behaves_like, sizeof(behaves_like));
	ff8_command_count = commands;
	ff8_command_data_count = data;

	ff8_kernel_command_arm();

	return true;
}

// ---- battle -------------------------------------------------------------
bool ff8_kernel_command_owns(uint8_t command)
{
	return ff8_command_armed && command >= VANILLA_COMMAND_COUNT && command < ff8_command_count;
}

typedef int(__cdecl *compute_command_action_t)(int, int, int, int, int, int, int);

int ff8_kernel_command_execute(int attacker_slot, int command, int id, int variant, int target_slot, int target_mask, int linked)
{
	// The dispatcher reads the command as a byte, and its callers leave junk in
	// the rest of the register; keep that junk, change only the byte.
	uint8_t command_id = (uint8_t)command;
	uint8_t family = ff8_command_behaves_like[command_id];

	// The family's case reads the battle command entry for the data id and the
	// name, so the new command's entry stands in for the family's own.
	uint8_t saved[COMMAND_ENTRY_SIZE];
	memcpy(saved, ff8_command_table[family], COMMAND_ENTRY_SIZE);
	memcpy(ff8_command_table[family], ff8_command_table[command_id], COMMAND_ENTRY_SIZE);

	int ret = ((compute_command_action_t)ff8_externals.battle_sub_48D200)(attacker_slot, (command & ~0xFF) | family, id, variant, target_slot, target_mask, linked);

	memcpy(ff8_command_table[family], saved, COMMAND_ENTRY_SIZE);

	return ret;
}
