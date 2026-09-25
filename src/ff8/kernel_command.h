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

#pragma once

#include <stdint.h>

// AddMoreCommand: support kernel.bin files whose battle command section (0)
// holds more than the vanilla 39 commands, and whose command ability data
// section (10) holds more than the vanilla 12 entries. A new battle command
// runs through the same code as the data-driven family (Defend, Mad Rush,
// Treatment, Recover, Revive, Doom, Kamikaze, Trance, LV Down, LV Up): what it
// does comes from its data entry. Completely inert while both sections keep
// their vanilla size.
//
// Shares AddMoreMagic's kernel.bin load hook and its dispatcher call-site hook.

// Called by the kernel.bin load hook with the freshly read file. Returns true
// when either section is grown and this feature wants the vanilla-layout image
// to be built.
bool ff8_kernel_command_read(const char *stash, const uint32_t *offsets, int size);

// True for the data sections this feature allows to have a non-vanilla size.
bool ff8_kernel_command_section_may_grow(int section);

// True for a battle command id this feature runs itself.
bool ff8_kernel_command_owns(uint8_t command);

// Runs an added battle command through the dispatcher, standing in for the
// family command it behaves like.
int ff8_kernel_command_execute(int attacker_slot, int command, int id, int variant, int target_slot, int target_mask, int linked);
