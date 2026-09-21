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

#include "kernel_ability.h"
#include "kernel_magic.h"

#include "../ff8.h"
#include "../patch.h"
#include "../globals.h"
#include "../common.h"
#include "../log.h"
#include "../utils.h"

#include <initializer_list>
#include <stdint.h>
#include <string.h>

// -------------------------------------------------------------------------
// AddMoreAbility - lets kernel.bin hold more than the vanilla 9 GF abilities.
//
// Kernel sections 12..18 (junction, command, stat %, character, party, GF and
// menu abilities) are contiguous 8-byte entries, and the exe reads them as ONE
// array based at the junction ability section: getAbilityName(id) indexes
// K_JUNCTION_ABILITY[id] for every id 0..115 and only uses the group to pick
// which text section the name offset belongs to. The group boundaries
// (20/39/58/78/83/92) are inlined constants, and a table at the end of .rdata
// holds {section offset, first id, entry size} per group.
//
// Growing the GF ability section therefore does two things: it shifts the menu
// ability ids, and it pushes every kernel section behind the ability block.
// Rather than relocate those sections (110 operands read them by absolute
// address), we keep the array FFNx-side and repoint the 26 operands that read
// it - the same trick AddMoreMagic uses for the magic table - then patch the
// handful of constants that describe where the GF group ends.
//
// Modder contract: kernel.bin section 17 lists the vanilla 9 GF abilities
// first, then the new ones; every other data section keeps its vanilla size;
// names and descriptions of the new entries go in the GF ability text section
// (section 47). Total ability entries must stay <= 128, the width of the
// savemap's per-GF learned mask.
// -------------------------------------------------------------------------

#define ABILITY_ENTRY_SIZE         8
// The savemap's per-GF learned-ability bitfield is 16 bytes wide.
#define MAX_ABILITY_COUNT          128
#define VANILLA_ABILITY_COUNT      116
#define VANILLA_GF_ABILITY_COUNT   9
#define VANILLA_FIRST_GF_ID        83
#define VANILLA_FIRST_MENU_ID      92
// Ability block start, relative to the kernel.bin buffer.
#define ABILITY_BLOCK_OFFSET       0x40E0
// Data sections, 0-based as the kernel.bin header lists them.
#define KERNEL_JUNCTION_ABIL_SEC   11
#define KERNEL_GF_ABIL_SEC         16
#define KERNEL_MENU_ABIL_SEC       17
#define KERNEL_AFTER_ABIL_SEC      18
// How far past an instruction's first byte its absolute operand may sit.
#define OPERAND_SCAN_WINDOW        8

// Vanilla offsets of the ability sections and of the section right behind them.
static const uint32_t vanilla_ability_offsets[] = {
	0x40E0, 0x4180, 0x4218, 0x42B0, 0x4350, 0x4378, 0x43C0, 0x4480,
};

// Every address this file needs, resolved from anchors ff8_externals already
// holds, or from an anchor this file resolves first.
static struct
{
	uint32_t k_ability;             // kernel buffer + 0x40E0, the whole array
	uint32_t kernel_buffer;         // KERNEL_HEADER
	uint32_t group_table;           // {u16 section offset, u8 first id, u8 entry size}[7]

	uint32_t fn_get_name;           // getAbilityName(int id)
	uint32_t fn_get_desc;           // getAbilityDescription(int id)
	uint32_t fn_group_from_id;      // getAbilityGroupFromId(int id)
	uint32_t fn_build_list;         // BuildGFAbilityList(gf, out, include_learnable)
	uint32_t fn_menu_mask;          // RebuildLearnedMenuAbilityMask()
	uint32_t fn_learned_popup;      // the "Learned X!" window
	uint32_t fn_draw_learn_status;  // AP progress line in the GF menu
	uint32_t fn_gf_battle_stats;    // computeGFBattleStats(gf)
	uint32_t fn_reset_parse_chara;  // ResetAndParseBattleAndFieldCharacter(slot)
	uint32_t fn_stat_percent_bonus; // GetCharaStatPercentBonus(slot, stat)
	uint32_t fn_add_ap;             // GF_AddApToLearningAbility(gf, ap)
	uint32_t fn_junction_menu;      // StatusJunctionMenuHandler
	uint32_t fn_gf_summary;         // Menu_BuildGFJunctionSummary
	uint32_t fn_draw_list_row;      // Menu_DrawAbilityListRow
} ability_ext;

// ---- state --------------------------------------------------------------
static uint8_t ff8_ability_table[MAX_ABILITY_COUNT][ABILITY_ENTRY_SIZE];
static int ff8_ability_count = VANILLA_ABILITY_COUNT;
static int ff8_gf_ability_count = VANILLA_GF_ABILITY_COUNT;
static int ff8_first_menu_id = VANILLA_FIRST_MENU_ID;
static bool ff8_ability_armed = false;
static bool ff8_ability_supported = false;

// ---- instruction sites --------------------------------------------------
// Offsets, inside the function that owns them, of the instructions whose
// absolute operand points into the ability array. The operand itself is found
// by scanning the instruction, so only the instruction address matters.
struct ability_site { uint32_t *owner; uint32_t offset; const char *what; };

static const ability_site array_sites[] = {
	{ &ability_ext.fn_get_name, 0x009, "getAbilityName junction" },
	{ &ability_ext.fn_get_name, 0x035, "getAbilityName command" },
	{ &ability_ext.fn_get_name, 0x061, "getAbilityName stat%" },
	{ &ability_ext.fn_get_name, 0x08D, "getAbilityName character" },
	{ &ability_ext.fn_get_name, 0x0B9, "getAbilityName party" },
	{ &ability_ext.fn_get_name, 0x0E3, "getAbilityName GF/menu" },
	{ &ability_ext.fn_get_desc, 0x009, "getAbilityDescription junction" },
	{ &ability_ext.fn_get_desc, 0x035, "getAbilityDescription command" },
	{ &ability_ext.fn_get_desc, 0x061, "getAbilityDescription stat%" },
	{ &ability_ext.fn_get_desc, 0x08D, "getAbilityDescription character" },
	{ &ability_ext.fn_get_desc, 0x0B9, "getAbilityDescription party" },
	{ &ability_ext.fn_get_desc, 0x0E3, "getAbilityDescription GF/menu" },
	{ &ability_ext.fn_reset_parse_chara, 0x0D0, "junction flags byte 2" },
	{ &ability_ext.fn_reset_parse_chara, 0x0D7, "junction flags byte 0" },
	{ &ability_ext.fn_reset_parse_chara, 0x0DE, "junction flags byte 1" },
	{ &ability_ext.fn_reset_parse_chara, 0x223, "character ability flags" },
	{ &ability_ext.fn_stat_percent_bonus, 0x02D, "stat percent stat id" },
	{ &ability_ext.fn_stat_percent_bonus, 0x03A, "stat percent value" },
	{ &ability_ext.fn_add_ap, 0x064, "AP required" },
	{ &ability_ext.fn_junction_menu, 0x4EE, "junction menu flags" },
	{ &ability_ext.fn_junction_menu, 0x22C4, "junction menu flags" },
	{ &ability_ext.fn_junction_menu, 0x251E, "junction menu flags" },
	{ &ability_ext.fn_junction_menu, 0x270F, "junction menu flags" },
	{ &ability_ext.fn_gf_summary, 0x079, "granted junction flags" },
	{ &ability_ext.fn_draw_list_row, 0x078, "ability row availability" },
	{ &ability_ext.fn_gf_battle_stats, 0x050, "effect loop start (id 64)" },
};

// The GF group's two range bounds in computeGFBattleStats, patched to the new
// boundaries rather than shifted like the sites above.
#define SITE_GF_RANGE_LOW   0x075
#define SITE_GF_RANGE_HIGH  0x07C

// The inlined "first menu ability" constant, and the ability count one past it.
static const ability_site boundary_sites[] = {
	{ &ability_ext.fn_get_name, 0x0E0, "getAbilityName" },
	{ &ability_ext.fn_get_desc, 0x0E0, "getAbilityDescription" },
	{ &ability_ext.fn_learned_popup, 0x0A6, "learned popup icon" },
	{ &ability_ext.fn_group_from_id, 0x03A, "getAbilityGroupFromId" },
	{ &ability_ext.fn_build_list, 0x266, "BuildGFAbilityList" },
	{ &ability_ext.fn_menu_mask, 0x040, "menu ability mask" },
};
#define SITE_ABILITY_COUNT_CMP  0x045   // inside fn_menu_mask: cmp id, 116

// The two reads of an entry's AP field through the group table; their operand
// is the kernel buffer + 4, not a pointer into the array.
static const ability_site ap_base_sites[] = {
	{ &ability_ext.fn_build_list, 0x2A2, "BuildGFAbilityList AP" },
	{ &ability_ext.fn_draw_learn_status, 0x137, "GF menu AP" },
};

// ---- helpers ------------------------------------------------------------
// Find the absolute operand of an instruction: the first dword inside it whose
// value falls in [lo, hi). Returns the operand's address, or 0.
static uint32_t find_operand(uint32_t insn, uint32_t lo, uint32_t hi)
{
	for (uint32_t i = 1; i <= OPERAND_SCAN_WINDOW; ++i)
	{
		uint32_t value = *(uint32_t *)(insn + i);

		if (value >= lo && value < hi) return insn + i;
	}

	return 0;
}

static uint32_t ability_array_end()
{
	return ability_ext.k_ability + VANILLA_ABILITY_COUNT * ABILITY_ENTRY_SIZE;
}

static uint32_t ability_table_base()
{
	return (uint32_t)&ff8_ability_table[0][0];
}

// ---- init ---------------------------------------------------------------
// Resolve every address this file patches. Anchors marked "delta" are measured
// on the US 1.2 build and only sanity-checked on the others - a wrong one makes
// the validation pass below fail, which disables the feature instead of
// patching the wrong instruction.
static void ff8_kernel_ability_find_externals()
{
	ability_ext.kernel_buffer = uint32_t(ff8_externals.unk_1CF3E48);
	ability_ext.k_ability = ability_ext.kernel_buffer + ABILITY_BLOCK_OFFSET;

	// Call chains.
	ability_ext.fn_menu_mask = get_relative_call(uint32_t(ff8_externals.sub_4F81F0), 0x3CA6);
	ability_ext.fn_build_list = ability_ext.fn_menu_mask ? get_relative_call(ability_ext.fn_menu_mask, 0x24) : 0;
	ability_ext.fn_reset_parse_chara = get_relative_call(ff8_externals.sub_48B7E0, 0x94);
	ability_ext.fn_stat_percent_bonus = ability_ext.fn_reset_parse_chara ? get_relative_call(ability_ext.fn_reset_parse_chara, 0x34C) : 0;

	// The ability text getters sit just before the magic one, in the same
	// family of kernel-text helpers.
	uint32_t magic_name_getter = ff8_kernel_magic_name_getter();
	ability_ext.fn_get_name = magic_name_getter ? magic_name_getter - 0x260 : 0;
	ability_ext.fn_get_desc = magic_name_getter ? magic_name_getter - 0x130 : 0;

	// Deltas.
	ability_ext.fn_group_from_id = ability_ext.fn_build_list ? ability_ext.fn_build_list - 0x50 : 0;
	ability_ext.fn_learned_popup = ability_ext.fn_build_list ? ability_ext.fn_build_list - 0x6890 : 0;
	ability_ext.fn_draw_learn_status = ability_ext.fn_build_list ? ability_ext.fn_build_list + 0x27A60 : 0;
	ability_ext.fn_gf_battle_stats = ff8_externals.compute_char_stats_sub_495960 - 0x1E0;
	ability_ext.fn_add_ap = ff8_externals.compute_char_stats_sub_495960 + 0x16B0;
	ability_ext.fn_junction_menu = ability_ext.fn_build_list ? ability_ext.fn_build_list + 0x2DE40 : 0;
	ability_ext.fn_gf_summary = ability_ext.fn_build_list ? ability_ext.fn_build_list + 0x360B0 : 0;
	ability_ext.fn_draw_list_row = ability_ext.fn_build_list ? ability_ext.fn_build_list + 0x3BCF0 : 0;

	// The group table's address is the operand of the lea that indexes it,
	// inside BuildGFAbilityList.
	ability_ext.group_table = ability_ext.fn_build_list ? *(uint32_t *)(ability_ext.fn_build_list + 0x28A) - 2 : 0;
}

// Check every site before touching any of them: a bad anchor must disable the
// feature, never half-patch the exe.
static bool ff8_kernel_ability_validate()
{
	uint32_t lo = ability_ext.k_ability, hi = ability_array_end();
	bool ok = true;

	if (!ability_ext.fn_build_list || !ability_ext.fn_get_name || !ability_ext.fn_menu_mask)
	{
		ffnx_warning("AddMoreAbility: could not resolve the ability functions, extension disabled.\n");
		return false;
	}

	for (const ability_site &site : array_sites)
	{
		if (!find_operand(*site.owner + site.offset, lo, hi))
		{
			ffnx_warning("AddMoreAbility: %s at 0x%X does not read the ability array.\n", site.what, *site.owner + site.offset);
			ok = false;
		}
	}

	for (uint32_t offset : { (uint32_t)SITE_GF_RANGE_LOW, (uint32_t)SITE_GF_RANGE_HIGH })
	{
		if (!find_operand(ability_ext.fn_gf_battle_stats + offset, lo, hi))
		{
			ffnx_warning("AddMoreAbility: GF ability range bound at 0x%X is not an ability pointer.\n", ability_ext.fn_gf_battle_stats + offset);
			ok = false;
		}
	}

	for (const ability_site &site : boundary_sites)
	{
		if (*(uint8_t *)(*site.owner + site.offset + 2) != VANILLA_FIRST_MENU_ID)
		{
			ffnx_warning("AddMoreAbility: %s boundary at 0x%X is not %d.\n", site.what, *site.owner + site.offset, VANILLA_FIRST_MENU_ID);
			ok = false;
		}
	}

	if (*(uint8_t *)(ability_ext.fn_menu_mask + SITE_ABILITY_COUNT_CMP + 2) != VANILLA_ABILITY_COUNT)
	{
		ffnx_warning("AddMoreAbility: ability count constant at 0x%X is not %d.\n", ability_ext.fn_menu_mask + SITE_ABILITY_COUNT_CMP, VANILLA_ABILITY_COUNT);
		ok = false;
	}

	for (const ability_site &site : ap_base_sites)
	{
		if (!find_operand(*site.owner + site.offset, ability_ext.kernel_buffer + 4, ability_ext.kernel_buffer + 5))
		{
			ffnx_warning("AddMoreAbility: %s base at 0x%X is not the kernel buffer.\n", site.what, *site.owner + site.offset);
			ok = false;
		}
	}

	// The group table must still describe the vanilla layout.
	for (int group = 0; group < 7; ++group)
	{
		const uint8_t *row = (const uint8_t *)(ability_ext.group_table + 4 * group);

		if (*(const uint16_t *)row != vanilla_ability_offsets[group] || row[3] != ABILITY_ENTRY_SIZE)
		{
			ffnx_warning("AddMoreAbility: ability group table row %d at 0x%X is not vanilla.\n", group, ability_ext.group_table + 4 * group);
			ok = false;
		}
	}

	return ok;
}

// ---- patch application --------------------------------------------------
void ff8_kernel_ability_arm()
{
	if (ff8_ability_armed || !ff8_ability_supported) return;
	ff8_ability_armed = true;

	uint32_t k_ability = ability_ext.k_ability;
	uint32_t table = ability_table_base();

	// Every read of the array moves to the FFNx-side table, keeping whatever
	// field offset the instruction had.
	for (const ability_site &site : array_sites)
	{
		uint32_t operand = find_operand(*site.owner + site.offset, k_ability, ability_array_end());

		if (operand) patch_code_dword(operand, (DWORD)(table + (*(uint32_t *)operand - k_ability)));
	}

	// The GF group's bounds are ids, not a fixed delta: low stays at the first
	// GF id, high follows the new first menu id. Both point at the entry's
	// stat_to_increase byte (+6).
	uint32_t low = find_operand(ability_ext.fn_gf_battle_stats + SITE_GF_RANGE_LOW, k_ability, ability_array_end());
	uint32_t high = find_operand(ability_ext.fn_gf_battle_stats + SITE_GF_RANGE_HIGH, k_ability, ability_array_end());

	if (low) patch_code_dword(low, (DWORD)(table + VANILLA_FIRST_GF_ID * ABILITY_ENTRY_SIZE + 6));
	if (high) patch_code_dword(high, (DWORD)(table + ff8_first_menu_id * ABILITY_ENTRY_SIZE + 6));

	// The inlined GF/menu boundary, and the total ability count.
	for (const ability_site &site : boundary_sites)
		patch_code_byte(*site.owner + site.offset + 2, (unsigned char)ff8_first_menu_id);

	patch_code_byte(ability_ext.fn_menu_mask + SITE_ABILITY_COUNT_CMP + 2, (unsigned char)ff8_ability_count);

	// The AP reads go through the group table: base + row offset + size * (id -
	// first id). With row offset == block offset + 8 * first id, rebasing the
	// table on ours keeps that arithmetic exact for every group.
	for (const ability_site &site : ap_base_sites)
	{
		uint32_t operand = find_operand(*site.owner + site.offset, ability_ext.kernel_buffer + 4, ability_ext.kernel_buffer + 5);

		if (operand) patch_code_dword(operand, (DWORD)(table + 4 - ABILITY_BLOCK_OFFSET));
	}

	// Menu abilities moved: their row of the group table follows.
	patch_code_word(ability_ext.group_table + 4 * 6, (WORD)(ABILITY_BLOCK_OFFSET + ABILITY_ENTRY_SIZE * ff8_first_menu_id));
	patch_code_byte(ability_ext.group_table + 4 * 6 + 2, (unsigned char)ff8_first_menu_id);

	if (trace_all) ffnx_trace("AddMoreAbility: armed with %d GF abilities (ids %d-%d), menu abilities %d-%d, %d abilities total.\n",
		ff8_gf_ability_count, VANILLA_FIRST_GF_ID, ff8_first_menu_id - 1, ff8_first_menu_id, ff8_ability_count - 1, ff8_ability_count);
}

// ---- kernel.bin load ----------------------------------------------------
bool ff8_kernel_ability_section_may_grow(int section)
{
	return section == KERNEL_GF_ABIL_SEC;
}

bool ff8_kernel_ability_read(const char *stash, const uint32_t *offsets, int size)
{
	if (!ff8_ability_supported) return false;

	int gf_entries = (int)((offsets[KERNEL_MENU_ABIL_SEC] - offsets[KERNEL_GF_ABIL_SEC]) / ABILITY_ENTRY_SIZE);
	int total = (int)((offsets[KERNEL_AFTER_ABIL_SEC] - offsets[KERNEL_JUNCTION_ABIL_SEC]) / ABILITY_ENTRY_SIZE);

	if (gf_entries == VANILLA_GF_ABILITY_COUNT && total == VANILLA_ABILITY_COUNT) return false;

	if (total > MAX_ABILITY_COUNT)
	{
		ffnx_warning("AddMoreAbility: kernel.bin has %d ability entries, the savemap holds %d - ignoring the extension.\n", total, MAX_ABILITY_COUNT);
		return false;
	}

	if (total - VANILLA_ABILITY_COUNT != gf_entries - VANILLA_GF_ABILITY_COUNT)
	{
		ffnx_warning("AddMoreAbility: only the GF ability section may grow, but the ability block grew by %d entries and the GF section by %d - ignoring the extension.\n",
			total - VANILLA_ABILITY_COUNT, gf_entries - VANILLA_GF_ABILITY_COUNT);
		return false;
	}

	if (ff8_ability_armed && (total != ff8_ability_count || gf_entries != ff8_gf_ability_count))
	{
		ffnx_warning("AddMoreAbility: kernel.bin changed size after the patches were applied - ignoring the new one.\n");
		return true;
	}

	memcpy(ff8_ability_table, stash + offsets[KERNEL_JUNCTION_ABIL_SEC], total * ABILITY_ENTRY_SIZE);
	ff8_ability_count = total;
	ff8_gf_ability_count = gf_entries;
	ff8_first_menu_id = VANILLA_FIRST_MENU_ID + (gf_entries - VANILLA_GF_ABILITY_COUNT);

	if (trace_all) ffnx_trace("AddMoreAbility: extended kernel.bin detected (%d GF abilities, %d abilities total).\n", gf_entries, total);

	ff8_kernel_ability_arm();

	return true;
}

void ff8_kernel_ability_init()
{
	ff8_kernel_ability_find_externals();

	ff8_ability_supported = ff8_kernel_ability_validate();

	if (!ff8_ability_supported && trace_all)
		ffnx_trace("AddMoreAbility: unsupported game version, extension disabled.\n");
}
