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
// AddMoreAbility - lets kernel.bin hold more abilities than the vanilla 116.
//
// Kernel sections 12..18 (junction, command, stat %, character, party, GF and
// menu abilities) are contiguous 8-byte entries, and the exe reads them as ONE
// array based at the junction ability section: getAbilityName(id) indexes
// K_JUNCTION_ABILITY[id] for every id 0..115 and only uses the group to pick
// which text section the name offset belongs to. Which group an id belongs to
// is decided by an inlined chain of constants (20/39/58/78/83/92) repeated in
// five functions, by a few standalone range checks, and by a table holding
// {section offset, first id, entry size} per group.
//
// Growing any of those sections shifts the ids of every group behind it and
// pushes every kernel section behind the ability block. Rather than relocate
// those sections (110 operands address them absolutely), we keep the array
// FFNx-side and repoint the 26 operands that read it - the same trick
// AddMoreMagic uses for the magic table - then rewrite every constant that
// says where a group starts, from the section sizes the file itself carries.
//
// Modder contract: each ability section lists its vanilla entries first, then
// the new ones; the total must stay <= 128, the width of the savemap's per-GF
// learned mask; the GF ability group must still start at id 64 or above; and a
// new entry's name and description go in its own group's text section.
// Everything that stores an ability id has to be renumbered to match - above
// all the 21-slot learn lists in section 3.
// -------------------------------------------------------------------------

#define ABILITY_ENTRY_SIZE         8
// The savemap's per-GF learned-ability bitfield is 16 bytes wide.
#define MAX_ABILITY_COUNT          128
#define VANILLA_ABILITY_COUNT      116
#define ABILITY_GROUP_COUNT        7
// computeGFBattleStats only walks learned bits 64..127, so the GF ability group
// may move up but never below that.
#define GF_EFFECT_FIRST_BIT        64
// Ability block start, relative to the kernel.bin buffer.
#define ABILITY_BLOCK_OFFSET       0x40E0
// Data sections, 0-based as the kernel.bin header lists them.
#define KERNEL_FIRST_ABIL_SEC      11
#define KERNEL_AFTER_ABIL_SEC      18
// How far past an instruction's first byte its absolute operand may sit.
#define OPERAND_SCAN_WINDOW        8
// The junction menu builds its candidate lists into two fixed buffers that sit
// immediately before the GF summary table - room for 20 command entries and 48
// equippable-passive ones, against 19 and 44 in vanilla. Both move FFNx-side so
// the groups they list can use the whole id space.
#define VANILLA_COMMAND_CANDIDATES 20
#define CANDIDATE_ENTRY_SIZE       2      // {u8 ability id, u8 group}
// A learned menu ability is one bit of a single dword (RebuildLearnedMenuAbilityMask).
#define MAX_MENU_ABILITIES         32
// Groups whose first id the standalone checks below care about.
#define GROUP_COMMAND              1
#define GROUP_STAT_PERCENT         2
#define GROUP_GF                   5
#define GROUP_MENU                 6

// Vanilla first id of each group.
static const uint8_t vanilla_group_first[ABILITY_GROUP_COUNT] = { 0, 20, 39, 58, 78, 83, 92 };

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
	uint32_t fn_validate_passives;  // Menu_ValidateCharaPassiveAbilities
	uint32_t fn_chara_ability_lists;// Menu_BuildCharaAbilityMaskAndLists
	uint32_t fn_junction_ability_page; // the junction menu's ability page

	uint32_t command_candidates;    // {id, group}[20] the junction menu builds
	uint32_t passive_candidates;    // {id, group}[48], right behind it
} ability_ext;

// ---- state --------------------------------------------------------------
static uint8_t ff8_ability_table[MAX_ABILITY_COUNT][ABILITY_ENTRY_SIZE];
static int ff8_ability_count = VANILLA_ABILITY_COUNT;
static uint8_t ff8_group_first[ABILITY_GROUP_COUNT];
// The two candidate lists, FFNx-side and wide enough for the whole id space.
static uint8_t ff8_command_candidates[MAX_ABILITY_COUNT][CANDIDATE_ENTRY_SIZE];
static uint8_t ff8_passive_candidates[MAX_ABILITY_COUNT][CANDIDATE_ENTRY_SIZE];
static bool ff8_ability_armed = false;
static bool ff8_ability_checked = false;
static bool ff8_ability_supported = false;

// ---- instruction sites --------------------------------------------------
// Offsets, inside the function that owns them, of the instructions whose
// absolute operand points into the ability array. The operand itself is found
// by scanning the instruction, so only the instruction address matters.
struct ability_site { uint32_t *owner; uint32_t offset; const char *what; };

static const ability_site array_sites[] = {
	{ &ability_ext.fn_get_name, 0x009, "getAbilityName junction" },
	{ &ability_ext.fn_get_name, 0x035, "getAbilityName command" },
	{ &ability_ext.fn_get_name, 0x061, "getAbilityName stat percent" },
	{ &ability_ext.fn_get_name, 0x08D, "getAbilityName character" },
	{ &ability_ext.fn_get_name, 0x0B9, "getAbilityName party" },
	{ &ability_ext.fn_get_name, 0x0E3, "getAbilityName GF/menu" },
	{ &ability_ext.fn_get_desc, 0x009, "getAbilityDescription junction" },
	{ &ability_ext.fn_get_desc, 0x035, "getAbilityDescription command" },
	{ &ability_ext.fn_get_desc, 0x061, "getAbilityDescription stat percent" },
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

// The GF group's two range bounds in computeGFBattleStats: pointers at the
// stat_to_increase byte of the first GF and of the first menu ability.
#define SITE_GF_RANGE_LOW   0x075
#define SITE_GF_RANGE_HIGH  0x07C

// The inlined group chain: six "is the id past the start of group N" compares,
// in id order, repeated in five functions. The immediate sits at insn + 2.
struct chain_sites { uint32_t *owner; uint32_t offset[ABILITY_GROUP_COUNT - 1]; const char *what; };

static const chain_sites group_chains[] = {
	{ &ability_ext.fn_get_name,      { 0x004, 0x030, 0x05C, 0x088, 0x0B4, 0x0E0 }, "getAbilityName" },
	{ &ability_ext.fn_get_desc,      { 0x004, 0x030, 0x05C, 0x088, 0x0B4, 0x0E0 }, "getAbilityDescription" },
	{ &ability_ext.fn_group_from_id, { 0x004, 0x00C, 0x017, 0x022, 0x02D, 0x03A }, "getAbilityGroupFromId" },
	{ &ability_ext.fn_build_list,    { 0x21F, 0x234, 0x240, 0x24C, 0x258, 0x266 }, "BuildGFAbilityList" },
	{ &ability_ext.fn_learned_popup, { 0x059, 0x068, 0x077, 0x086, 0x095, 0x0A6 }, "learned popup icon" },
};

// Standalone constants: one group's first id, checked on its own.
struct group_bound_site { uint32_t *owner; uint32_t offset; int group; const char *what; };

static const group_bound_site group_bound_sites[] = {
	{ &ability_ext.fn_menu_mask,           0x040, GROUP_MENU,         "menu ability mask start" },
	{ &ability_ext.fn_validate_passives,   0x04F, GROUP_STAT_PERCENT, "equippable passives start" },
	{ &ability_ext.fn_validate_passives,   0x054, GROUP_GF,           "equippable passives end" },
	{ &ability_ext.fn_chara_ability_lists, 0x0B8, GROUP_STAT_PERCENT, "passive candidate list start" },
	{ &ability_ext.fn_chara_ability_lists, 0x114, GROUP_GF,           "passive candidate list end" },
	{ &ability_ext.fn_gf_summary,          0x074, GROUP_COMMAND,      "junction ability check" },
};

// One past the last ability, inside RebuildLearnedMenuAbilityMask.
#define SITE_ABILITY_COUNT_CMP  0x045

// Every operand pointing into one of the two candidate lists. Some address the
// list itself, some its second byte, so each is moved by whatever it pointed at.
struct candidate_site { uint32_t *owner; uint32_t offset; bool passive; const char *what; };

static const candidate_site candidate_sites[] = {
	{ &ability_ext.fn_junction_menu,         0x04E5, false, "junction menu command list" },
	{ &ability_ext.fn_junction_menu,         0x2185, false, "junction menu command list" },
	{ &ability_ext.fn_junction_menu,         0x2515, false, "junction menu command list" },
	{ &ability_ext.fn_junction_menu,         0x2706, false, "junction menu command list" },
	{ &ability_ext.fn_junction_menu,         0x28D8, false, "junction menu command list" },
	{ &ability_ext.fn_chara_ability_lists,   0x0078, false, "command list build" },
	{ &ability_ext.fn_junction_ability_page, 0x009F, false, "ability page command list" },
	{ &ability_ext.fn_junction_menu,         0x04BF, true,  "junction menu passive list" },
	{ &ability_ext.fn_junction_menu,         0x21E3, true,  "junction menu passive list" },
	{ &ability_ext.fn_junction_menu,         0x24F3, true,  "junction menu passive list" },
	{ &ability_ext.fn_junction_menu,         0x26E4, true,  "junction menu passive list" },
	{ &ability_ext.fn_junction_menu,         0x28CD, true,  "junction menu passive list" },
	{ &ability_ext.fn_chara_ability_lists,   0x00D4, true,  "passive list build" },
	{ &ability_ext.fn_junction_ability_page, 0x0080, true,  "ability page passive list" },
};

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

// Where a vanilla candidate list starts, and where it moves to.
static void candidate_range(bool passive, uint32_t *from, uint32_t *to)
{
	*from = passive ? ability_ext.passive_candidates : ability_ext.command_candidates;
	*to = (uint32_t)(passive ? &ff8_passive_candidates[0][0] : &ff8_command_candidates[0][0]);
}

// One past the last byte an operand into that list may hold. The command list
// ends where the passive one starts; the passive one runs to the GF summary
// table, which no operand of either list ever reaches.
static uint32_t candidate_end(bool passive)
{
	return passive
		? ability_ext.passive_candidates + MAX_ABILITY_COUNT * CANDIDATE_ENTRY_SIZE
		: ability_ext.passive_candidates;
}

static uint32_t ability_table_base()
{
	return (uint32_t)&ff8_ability_table[0][0];
}

// ---- init ---------------------------------------------------------------
// Resolve every address this file patches. The ones marked as deltas are
// measured on the US 1.2 build; a delta that does not hold on some other build
// makes the validation pass below fail, which disables the feature instead of
// patching the wrong instruction.
static void ff8_kernel_ability_find_externals()
{
	ability_ext.kernel_buffer = uint32_t(ff8_externals.unk_1CF3E48);
	ability_ext.k_ability = ability_ext.kernel_buffer + ABILITY_BLOCK_OFFSET;

	// Call chains.
	ability_ext.fn_menu_mask = get_relative_call(uint32_t(ff8_externals.menu_use_items_sub_4F81F0), 0x3CA6);
	ability_ext.fn_build_list = ability_ext.fn_menu_mask ? get_relative_call(ability_ext.fn_menu_mask, 0x24) : 0;
	ability_ext.fn_reset_parse_chara = get_relative_call(ff8_externals.sub_48B7E0, 0x94);
	ability_ext.fn_stat_percent_bonus = ability_ext.fn_reset_parse_chara ? get_relative_call(ability_ext.fn_reset_parse_chara, 0x34C) : 0;

	// The ability text getters sit just before the magic one, in the same
	// family of kernel-text helpers.
	uint32_t magic_name_getter = ff8_kernel_magic_name_getter();
	ability_ext.fn_get_name = magic_name_getter ? magic_name_getter - 0x260 : 0;
	ability_ext.fn_get_desc = magic_name_getter ? magic_name_getter - 0x130 : 0;

	// Deltas from BuildGFAbilityList.
	uint32_t list = ability_ext.fn_build_list;

	ability_ext.fn_group_from_id = list ? list - 0x50 : 0;
	ability_ext.fn_learned_popup = list ? list - 0x6890 : 0;
	ability_ext.fn_draw_learn_status = list ? list + 0x27A60 : 0;
	ability_ext.fn_validate_passives = list ? list + 0x2DAF0 : 0;
	ability_ext.fn_junction_menu = list ? list + 0x2DE40 : 0;
	ability_ext.fn_chara_ability_lists = list ? list + 0x335A0 : 0;
	ability_ext.fn_gf_summary = list ? list + 0x360B0 : 0;
	ability_ext.fn_draw_list_row = list ? list + 0x3BCF0 : 0;

	ability_ext.fn_junction_ability_page = list ? list + 0x359D0 : 0;

	// Both lists are named by a "mov reg, offset list+1" inside the function that
	// fills them, so read the bases from there rather than hardcoding them.
	if (ability_ext.fn_chara_ability_lists)
	{
		ability_ext.command_candidates = *(uint32_t *)(ability_ext.fn_chara_ability_lists + 0x78 + 1) - 1;
		ability_ext.passive_candidates = *(uint32_t *)(ability_ext.fn_chara_ability_lists + 0xD4 + 1) - 1;
	}

	// Deltas from the character stat computation.
	ability_ext.fn_gf_battle_stats = ff8_externals.compute_char_stats_sub_495960 + 0x420;
	ability_ext.fn_add_ap = ff8_externals.compute_char_stats_sub_495960 + 0x16B0;

	// The group table's address is the operand of the lea that indexes it,
	// inside BuildGFAbilityList; that operand points at the row's first-id
	// byte, two into the row.
	ability_ext.group_table = list ? *(uint32_t *)(list + 0x28A) - 2 : 0;
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

	// Each chain must still read as the vanilla 20/39/58/78/83/92.
	for (const chain_sites &chain : group_chains)
	{
		for (int group = 1; group < ABILITY_GROUP_COUNT; ++group)
		{
			uint32_t site = *chain.owner + chain.offset[group - 1];

			if (*(uint8_t *)(site + 2) != vanilla_group_first[group])
			{
				ffnx_warning("AddMoreAbility: %s boundary %d at 0x%X is not %d.\n", chain.what, group, site, vanilla_group_first[group]);
				ok = false;
			}
		}
	}

	for (const group_bound_site &site : group_bound_sites)
	{
		if (*(uint8_t *)(*site.owner + site.offset + 2) != vanilla_group_first[site.group])
		{
			ffnx_warning("AddMoreAbility: %s at 0x%X is not %d.\n", site.what, *site.owner + site.offset, vanilla_group_first[site.group]);
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

	// The command list holds 20 entries and the passive one starts right behind it.
	if (ability_ext.passive_candidates - ability_ext.command_candidates
			!= VANILLA_COMMAND_CANDIDATES * CANDIDATE_ENTRY_SIZE)
	{
		ffnx_warning("AddMoreAbility: the junction menu candidate lists sit at 0x%X and 0x%X, which is not the vanilla layout.\n",
			ability_ext.command_candidates, ability_ext.passive_candidates);
		ok = false;
	}

	for (const candidate_site &site : candidate_sites)
	{
		uint32_t from, to;

		candidate_range(site.passive, &from, &to);

		if (!find_operand(*site.owner + site.offset, from, candidate_end(site.passive)))
		{
			ffnx_warning("AddMoreAbility: %s at 0x%X does not point into that list.\n", site.what, *site.owner + site.offset);
			ok = false;
		}
	}

	// The group table must still describe the vanilla layout.
	for (int group = 0; group < ABILITY_GROUP_COUNT; ++group)
	{
		const uint8_t *row = (const uint8_t *)(ability_ext.group_table + 4 * group);
		uint16_t vanilla_offset = (uint16_t)(ABILITY_BLOCK_OFFSET + ABILITY_ENTRY_SIZE * vanilla_group_first[group]);

		if (*(const uint16_t *)row != vanilla_offset || row[2] != vanilla_group_first[group] || row[3] != ABILITY_ENTRY_SIZE)
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

	// The GF group's bounds are ids, not a fixed delta: both point at the
	// stat_to_increase byte (+6) of a group's first entry.
	uint32_t low = find_operand(ability_ext.fn_gf_battle_stats + SITE_GF_RANGE_LOW, k_ability, ability_array_end());
	uint32_t high = find_operand(ability_ext.fn_gf_battle_stats + SITE_GF_RANGE_HIGH, k_ability, ability_array_end());

	if (low) patch_code_dword(low, (DWORD)(table + ff8_group_first[GROUP_GF] * ABILITY_ENTRY_SIZE + 6));
	if (high) patch_code_dword(high, (DWORD)(table + ff8_group_first[GROUP_MENU] * ABILITY_ENTRY_SIZE + 6));

	// Where each group now starts: the inlined chains first, then the
	// standalone checks.
	for (const chain_sites &chain : group_chains)
		for (int group = 1; group < ABILITY_GROUP_COUNT; ++group)
			patch_code_byte(*chain.owner + chain.offset[group - 1] + 2, ff8_group_first[group]);

	for (const group_bound_site &site : group_bound_sites)
		patch_code_byte(*site.owner + site.offset + 2, ff8_group_first[site.group]);

	// One past the last ability. At 128 this writes 0x80, which the cmp sign
	// extends to 0xFFFFFF80 - harmless, because the test is unsigned and no
	// ability id can reach 128 anyway, so every id still passes it.
	patch_code_byte(ability_ext.fn_menu_mask + SITE_ABILITY_COUNT_CMP + 2, (unsigned char)ff8_ability_count);

	// The AP reads go through the group table: base + row offset + size * (id -
	// first id). With row offset == block offset + 8 * first id, rebasing the
	// table on ours keeps that arithmetic exact for every group.
	for (const ability_site &site : ap_base_sites)
	{
		uint32_t operand = find_operand(*site.owner + site.offset, ability_ext.kernel_buffer + 4, ability_ext.kernel_buffer + 5);

		if (operand) patch_code_dword(operand, (DWORD)(table + 4 - ABILITY_BLOCK_OFFSET));
	}

	// Both candidate lists move to buffers wide enough for the whole id space, so
	// the groups they list are no longer capped by what fits in front of the GF
	// summary table behind them.
	for (const candidate_site &site : candidate_sites)
	{
		uint32_t from, to;

		candidate_range(site.passive, &from, &to);

		uint32_t operand = find_operand(*site.owner + site.offset, from, candidate_end(site.passive));

		if (operand) patch_code_dword(operand, (DWORD)(to + (*(uint32_t *)operand - from)));
	}

	// Every group table row follows its group.
	for (int group = 0; group < ABILITY_GROUP_COUNT; ++group)
	{
		patch_code_word(ability_ext.group_table + 4 * group, (WORD)(ABILITY_BLOCK_OFFSET + ABILITY_ENTRY_SIZE * ff8_group_first[group]));
		patch_code_byte(ability_ext.group_table + 4 * group + 2, ff8_group_first[group]);
	}

	if (trace_all) ffnx_trace("AddMoreAbility: armed with %d abilities - junction %d, command %d, stat%% %d, character %d, party %d, GF %d, menu %d.\n",
		ff8_ability_count,
		ff8_group_first[1] - ff8_group_first[0], ff8_group_first[2] - ff8_group_first[1],
		ff8_group_first[3] - ff8_group_first[2], ff8_group_first[4] - ff8_group_first[3],
		ff8_group_first[5] - ff8_group_first[4], ff8_group_first[6] - ff8_group_first[5],
		ff8_ability_count - ff8_group_first[6]);
}

// ---- kernel.bin load ----------------------------------------------------
bool ff8_kernel_ability_section_may_grow(int section)
{
	return section >= KERNEL_FIRST_ABIL_SEC && section < KERNEL_AFTER_ABIL_SEC;
}

bool ff8_kernel_ability_read(const char *stash, const uint32_t *offsets, int size)
{
	uint8_t first[ABILITY_GROUP_COUNT];
	int total = 0;

	for (int group = 0; group < ABILITY_GROUP_COUNT; ++group)
	{
		int section = KERNEL_FIRST_ABIL_SEC + group;
		int entries = (int)((offsets[section + 1] - offsets[section]) / ABILITY_ENTRY_SIZE);

		if (entries < 0 || total + entries > MAX_ABILITY_COUNT)
		{
			ffnx_warning("AddMoreAbility: kernel.bin ability sections hold more than the %d ids the savemap can learn - ignoring the extension.\n", MAX_ABILITY_COUNT);
			return false;
		}

		first[group] = (uint8_t)total;
		total += entries;
	}

	// A stock kernel.bin leaves here, before anything is resolved, checked or
	// patched: on vanilla this feature costs one comparison and nothing else.
	if (total == VANILLA_ABILITY_COUNT && !memcmp(first, vanilla_group_first, sizeof(first))) return false;

	// First extended file seen: only now is it worth resolving the addresses and
	// checking every site, so a build this does not support stays quiet for anyone
	// who never mods abilities.
	if (!ff8_ability_checked)
	{
		ff8_ability_checked = true;
		ff8_kernel_ability_find_externals();
		ff8_ability_supported = ff8_kernel_ability_validate();

		if (!ff8_ability_supported)
			ffnx_warning("AddMoreAbility: this build is not supported, extension disabled.\n");
	}

	if (!ff8_ability_supported) return false;

	// computeGFBattleStats only walks learned bits 64..127; GF abilities below
	// that would silently stop working.
	if (first[GROUP_GF] < GF_EFFECT_FIRST_BIT)
	{
		ffnx_warning("AddMoreAbility: the GF ability group starts at id %d, but its effects are only read from id %d up - ignoring the extension.\n", first[GROUP_GF], GF_EFFECT_FIRST_BIT);
		return false;
	}

	// A learned menu ability is one bit of a single dword.
	int menu_abilities = total - first[GROUP_MENU];

	if (menu_abilities > MAX_MENU_ABILITIES)
	{
		ffnx_warning("AddMoreAbility: %d menu abilities, but only %d of them can be remembered as learned - ignoring the extension.\n",
			menu_abilities, MAX_MENU_ABILITIES);
		return false;
	}

	if (ff8_ability_armed && (total != ff8_ability_count || memcmp(first, ff8_group_first, sizeof(first))))
	{
		ffnx_warning("AddMoreAbility: kernel.bin ability layout changed after the patches were applied - ignoring the new one.\n");
		return true;
	}

	memcpy(ff8_ability_table, stash + offsets[KERNEL_FIRST_ABIL_SEC], total * ABILITY_ENTRY_SIZE);
	memcpy(ff8_group_first, first, sizeof(first));
	ff8_ability_count = total;

	if (trace_all) ffnx_trace("AddMoreAbility: extended kernel.bin detected (%d abilities, %d more than vanilla).\n", total, total - VANILLA_ABILITY_COUNT);

	ff8_kernel_ability_arm();

	return true;
}

void ff8_kernel_ability_init()
{
	// Nothing is resolved or patched here. The feature wakes up in
	// ff8_kernel_ability_read(), and only for a kernel.bin whose ability sections
	// are not the vanilla 116; AddMoreMagic's load hook is what calls it.
	memcpy(ff8_group_first, vanilla_group_first, sizeof(ff8_group_first));
}
