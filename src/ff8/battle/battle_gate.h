/****************************************************************************/
//    Copyright (C) 2009 Aali132                                            //
//    Copyright (C) 2018 quantumpencil                                      //
//    Copyright (C) 2018 Maxime Bacoux                                      //
//    Copyright (C) 2020 myst6re                                            //
//    Copyright (C) 2020 Chris Rizzitello                                   //
//    Copyright (C) 2020 John Pritchard                                     //
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

// 30 fps battles: the battle subsystems paced back to native speed while the battle data plays every
// frame (see battle_gate.cpp). The host (FFNx, or the standalone FF8Battle30.dll) runs the battle at
// 30 fps, calls ff8_bgate_mid_frame_input half way through each battle frame's wait and raises
// ff8_bgate_left_battle on any frame spent outside battle.

extern bool ff8_bgate_left_battle;
extern bool ff8_bgate_active;
extern int ff8_bgate_n;

void ff8_bgate_mid_frame_input();
void ff8_bgate_install_hooks();
// Vanilla 15 fps battle: nothing gated, only the diagnostics, to compare against.
void ff8_bgate_install_reference();
