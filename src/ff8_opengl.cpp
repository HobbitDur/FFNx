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

#include "ff8/battle/fx/fx_port.h"
#include <intrin.h>
#include "globals.h"
#include "common.h"
#include "ff8.h"
#include "fake_dd.h"
#include "patch.h"
#include "log.h"
#include "macro.h"
#include "movies.h"
#include "gl.h"
#include "gamepad.h"
#include "joystick.h"
#include "sdl_gamepad.h"
#include "gamehacks.h"
#include "utils.h"
#include "vibration.h"
#include "ff8/file.h"
#include "ff8/vram.h"
#include "ff8/save_data.h"
#include "metadata.h"
#include "achievement.h"
#include "widescreen.h"

unsigned char texture_reload_fix1[] = {0x5B, 0x5F, 0x5E, 0x5D, 0x81, 0xC4, 0x10, 0x01, 0x00, 0x00};
unsigned char texture_reload_fix2[] = {0x5F, 0x5E, 0x5D, 0x5B, 0x81, 0xC4, 0x8C, 0x00, 0x00, 0x00};
int left_stick_y = 0x80;
int left_stick_x = 0x80;
int right_stick_y = 0x80;
int right_stick_x = 0x80;

std::chrono::time_point<std::chrono::high_resolution_clock> intro_credits_music_start_time;
constexpr int intro_credits_fade_frames = 33;
constexpr int intro_credits_adjusted_frames = 438; // Instead of 374 in the Game
constexpr int intro_credits_frames_between_music_start_and_first_image = 180;

uint8_t *extended_memory = nullptr;
uint16_t *field_current_poly = nullptr;

int (*ff8_opcode_old_battle)(int);

void ff8gl_field_78(struct ff8_polygon_set *polygon_set, struct ff8_game_obj *game_object)
{
	struct matrix_set *matrix_set;
	struct p_hundred *hundred_data = 0;
	uint32_t group_counter;

	if(trace_all) ffnx_trace("dll_gfx: field_78\n");

	if(!game_object->in_scene) return;

	if(polygon_set == 0) return;

	if(polygon_set->field_0 == 0) return;

	matrix_set = polygon_set->matrix_set;

	hundred_data = 0;

	if(polygon_set->field_2C) hundred_data = polygon_set->hundred_data;

	group_counter = 0;

	while(group_counter < polygon_set->numgroups)
	{
		uint32_t defer = false;
		uint32_t zsort = false;

		if(polygon_set->per_group_hundreds) hundred_data = polygon_set->hundred_data_group_array[group_counter];

		if(hundred_data)
		{
			if(game_object->field_91C && hundred_data->zsort) zsort = true;
			else if(!game_object->field_928) defer = (hundred_data->options & (BIT(V_ALPHABLEND) | BIT(V_TMAPBLEND)));
		}

		if(!defer) common_setrenderstate(hundred_data, (struct game_obj *)game_object);

		if(matrix_set && matrix_set->matrix_projection) gl_set_d3dprojection_matrix(matrix_set->matrix_projection);

		if(hundred_data) hundred_data = &hundred_data[1];

		group_counter++;
	}
}

void ff8gl_field_54(struct texture_set *texture_set, struct game_obj *game_object)
{
	if (trace_all) ffnx_trace("field_54\n");
}

void ff8gl_field_58(struct texture_set *texture_set, struct game_obj *game_object)
{
	if (trace_all) ffnx_trace("field_58\n");
}

void ff8gl_field_5C(struct texture_set *texture_set, struct game_obj *game_object)
{
	if (trace_all) ffnx_trace("field_5C\n");
}

void ff8gl_field_60(struct palette *palette, struct texture_set *texture_set)
{
	if(trace_all) ffnx_trace("field_60\n");
}

void ff8gl_field_84(uint32_t unknown, struct game_obj *game_object)
{
	if (trace_all) ffnx_trace("field_84\n");
}

void ff8gl_field_88()
{
	if (trace_all) ffnx_trace("field_88\n");
}

void ff8_destroy_tex_header(struct ff8_tex_header *tex_header)
{
	if(!tex_header) return;

	if((uint32_t)tex_header->file.pc_name > 32) external_free(tex_header->file.pc_name);

	external_free(tex_header->old_palette_data);
	external_free(tex_header->palette_colorkey);
	external_free(tex_header->tex_format.palette_data);
	external_free(tex_header->image_data);

	external_free(tex_header);
}

struct ff8_tex_header *ff8_load_tex_file(struct ff8_file_context* file_context, char *filename)
{
	struct ff8_tex_header *ret = (struct ff8_tex_header *)common_externals.create_tex_header();
	struct ff8_file* file = ff8_open_file(file_context, filename);
	uint32_t i, len;

	if(!file) goto error;
	if(!ff8_read_file(sizeof(*ret), ret, file)) goto error;

	ret->image_data = 0;
	ret->old_palette_data = 0;
	ret->palette_colorkey = 0;
	ret->tex_format.palette_data = 0;

	if(ret->version != 2) goto error;
	else
	{
		if(ret->tex_format.use_palette)
		{
			ret->tex_format.palette_data = (uint32_t*)common_externals.alloc_read_file(4, ret->tex_format.palette_size, (struct file *)file);
			if(!ret->tex_format.palette_data) goto error;
		}

		ret->image_data = (unsigned char*)common_externals.alloc_read_file(ret->tex_format.bytesperpixel, ret->tex_format.width * ret->tex_format.height, (struct file *)file);
		if(!ret->image_data) goto error;

		if(ret->use_palette_colorkey)
		{
			ret->palette_colorkey = (char*)common_externals.alloc_read_file(1, ret->palettes, (struct file *)file);
			if(!ret->palette_colorkey) goto error;
		}
	}

	ret->file.pc_name = (char*)external_malloc(1024);

	len = _snprintf(ret->file.pc_name, 1024, "%s", &filename[7]);

	for(i = 0; i < len; i++)
	{
		if(ret->file.pc_name[i] == '.')
		{
			if(!_strnicmp(&ret->file.pc_name[i], ".TEX", 4)) ret->file.pc_name[i] = 0;
			else ret->file.pc_name[i] = '_';
		}
	}

	ff8_close_file(file);
	return ret;

error:
	ff8_destroy_tex_header(ret);
	ff8_close_file(file);
	return 0;
}

#define TEXRELOAD_BUFFER_SIZE 64

struct
{
	char *image_data;
	uint32_t size;
	struct ff8_texture_set *texture_set;
} reload_buffer[TEXRELOAD_BUFFER_SIZE] = {};
uint32_t reload_buffer_index = 0;

// this function is wedged into the middle of a function designed to reload a Direct3D texture
// when the image data changes
void texture_reload_hack(struct texture_page *texture_page, struct ff8_texture_set *texture_set)
{
	uint32_t i;
	uint32_t size;
	VOBJ(tex_header, tex_header, texture_set->tex_header);

	size = VREF(tex_header, tex_format.width) * VREF(tex_header, tex_format.height) * VREF(tex_header, tex_format.bytesperpixel);

	// a circular buffer holds the last TEXRELOAD_BUFFER_SIZE textures that went through here
	// and their respective image data so that we can see if anything actually changed and avoid
	// unnecessary texture reloads
	for(i = 0; i < TEXRELOAD_BUFFER_SIZE; i++)
	{
		if(reload_buffer[i].texture_set == texture_set && reload_buffer[i].size == size && memcmp(reload_buffer[i].image_data, VREF(tex_header, image_data), size) == 0)
		{
			return;
		}
	}

	TexturePacker::TiledTex tiledTex = texturePacker.getTiledTex(VREF(tex_header, image_data));
	Tim::Bpp texBpp = Tim::Bpp(texture_page->color_key);

	if (tiledTex.isValid() && texBpp != tiledTex.bpp()) {
		if(trace_all || trace_vram) ffnx_trace("%s: ignore reload because BPP does not match 0x%X (bpp vram=%d, bpp tex=%d, source bpp tex=%d) image_data=0x%X\n", __func__, texture_set, tiledTex.bpp(), VREF(tex_header, tex_format.bytesperpixel), texBpp, VREF(tex_header, image_data));

		return;
	}

	if (texBpp != Tim::Bpp16) {
		const std::list<TexturePacker::IdentifiedTexture> &textures = texturePacker.matchTextures(tiledTex, false, true);

		if (textures.empty()) {
			if(trace_all || trace_vram) ffnx_trace("%s: ignore reload because no animated texture matches the current texture set 0x%X (bpp vram=%d, bpp tex=%d, source bpp tex=%d) image_data=0x%X\n", __func__, texture_set, tiledTex.bpp(), VREF(tex_header, tex_format.bytesperpixel), texBpp, VREF(tex_header, image_data));

			return;
		}
	}

	common_unload_texture((struct texture_set *)texture_set);
	common_load_texture((struct texture_set *)texture_set, texture_set->tex_header, texture_set->texture_format);

	reload_buffer[reload_buffer_index].texture_set = texture_set;
	if (reload_buffer[reload_buffer_index].image_data != nullptr && reload_buffer[reload_buffer_index].size != size) {
		driver_free(reload_buffer[reload_buffer_index].image_data);
		reload_buffer[reload_buffer_index].image_data = nullptr;
	}
	if (reload_buffer[reload_buffer_index].image_data == nullptr) {
		reload_buffer[reload_buffer_index].image_data = (char*)driver_malloc(size);
	}
	memcpy(reload_buffer[reload_buffer_index].image_data, VREF(tex_header, image_data), size);
	reload_buffer[reload_buffer_index].size = size;
	reload_buffer_index = (reload_buffer_index + 1) % TEXRELOAD_BUFFER_SIZE;

	stats.texture_reloads++;

	if(trace_all || trace_vram) ffnx_trace("texture_reload_hack: 0x%X (bpp=%d, sourceBpp=%d) image_data=0x%X\n", texture_set, VREF(tex_header, tex_format.bytesperpixel), texBpp, VREF(tex_header, image_data));
}

void texture_reload_hack1(struct texture_page *texture_page, uint32_t unknown1, uint32_t unknown2)
{
	struct ff8_texture_set *texture_set = (struct ff8_texture_set *)texture_page->tri_gfxobj->hundred_data->texture_set;

	texture_reload_hack(texture_page, texture_set);
}

void texture_reload_hack2(struct texture_page *texture_page, uint32_t unknown1, uint32_t unknown2)
{
	struct ff8_texture_set *texture_set = (struct ff8_texture_set *)texture_page->sub_tri_gfxobj->hundred_data->texture_set;

	texture_reload_hack(texture_page, texture_set);
}

void ff8_unload_texture(struct ff8_texture_set *texture_set)
{
	uint32_t i;

	// remove any references to this texture
	for(i = 0; i < TEXRELOAD_BUFFER_SIZE; i++) if(reload_buffer[i].texture_set == texture_set) reload_buffer[i].texture_set = 0;
}

void swirl_sub_56D390(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	static struct tex_header *last_tex_header = 0;
	struct tex_header *tex_header = make_framebuffer_tex(256, 256, x, y, w, h, false);

	if(last_tex_header) ff8_destroy_tex_header((struct ff8_tex_header *)last_tex_header);

	if(trace_all) ffnx_trace("swirl_sub_56D390: (%i, %i) %ix%i 0x%x (0x%x)\n", x, y, w, h, *ff8_externals.swirl_texture1, tex_header);

	struct ff8_texture_set *texture_set = (struct ff8_texture_set *)(*ff8_externals.swirl_texture1)->hundred_data->texture_set;

	common_unload_texture((*ff8_externals.swirl_texture1)->hundred_data->texture_set);
	common_load_texture((*ff8_externals.swirl_texture1)->hundred_data->texture_set, tex_header, texture_set->texture_format);

	last_tex_header = tex_header;
}

void ff8_set_render_to_vram_current_screen_flag_before_battle()
{
	if(trace_all) ffnx_trace("%s\n", __func__);

	// Disable software frame rendering to VRAM (by not doing anything here) because it is not needed anymore
}

void ff8_wm_set_render_to_vram_current_screen_flag_before_battle()
{
	if(trace_all) ffnx_trace("%s\n", __func__);

	// There is currently an visual issue in the last worldmap frame before swirl if this flag is enabled
	// We lose the shadows, but keep the full battle transition effect
	*ff8_externals.sub_blending_capability = false;

	ff8_set_render_to_vram_current_screen_flag_before_battle();
}

void ff8_swirl_init(float a1)
{
	if(trace_all) ffnx_trace("%s\n", __func__);

	// Reenable the flag that was disabled in worldmap
	*ff8_externals.sub_blending_capability = true;

	((void(*)(float))ff8_externals.sub_460B60)(a1);
}

int ff8_init_gamepad()
{
	if (use_sdl_gamepad)
	{
		if (sdlgamepad.Refresh())
			return TRUE;
	}
	else if (xinput_connected)
	{
		if (gamepad.Refresh())
			return TRUE;
	}
	else
	{
		if (joystick.Refresh())
			return TRUE;
	}

	return FALSE;
}

int ff8_get_analog_value(int8_t port, int type, int8_t offset)
{
	if (type == 0) {
		return right_stick_x;
	}

	if (type == 1) {
		return right_stick_y;
	}

	if (type == 2) {
		return left_stick_x;
	}

	if (type == 3) {
		return left_stick_y;
	}

	return -1;
}

int ff8_get_analog_value_wm(int8_t port, int type, int8_t offset)
{
	if (left_stick_x != 0x80 || left_stick_y != 0x80) {
		int *keyscans = *(int **)(ff8_externals.worldmap_input_update_sub_559240 + (FF8_US_VERSION ? 0x64 : 0x61));
		int index = **(int **)(ff8_externals.worldmap_input_update_sub_559240 + (FF8_US_VERSION ? 0x9 : 0x6));
		keyscans[index] &= 0x0FFF; // Remove d-pad keys
	}

	return ff8_get_analog_value(port, type, offset);
}

LPDIJOYSTATE2 ff8_update_gamepad_status()
{
	ff8_externals.dinput_gamepad_state->rgdwPOV[0] = -1;
	ff8_externals.dinput_gamepad_state->lX = 0;
	ff8_externals.dinput_gamepad_state->lY = 0;
	ff8_externals.dinput_gamepad_state->lRx = 0;
	ff8_externals.dinput_gamepad_state->lRy = 0;

	nxVibrationEngine.rumbleUpdate();

	int lX = 0, lY = 0, rX = 0, rY = 0;

	if (use_sdl_gamepad)
	{
		if (!sdlgamepad.Refresh() || !gamehacks.canInputBeProcessed())
			return ff8_externals.dinput_gamepad_state;

		if ((sdlgamepad.leftStickY > 0.5f) || sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_DPAD_UP))
		{
			ff8_externals.dinput_gamepad_state->lY = 0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 0;
		}
		else if ((sdlgamepad.leftStickY < -0.5f) || sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_DPAD_DOWN))
		{
			ff8_externals.dinput_gamepad_state->lY = -0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 18000;
		}

		if ((sdlgamepad.leftStickX < -0.5f) || sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_DPAD_LEFT))
		{
			ff8_externals.dinput_gamepad_state->lX = 0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 27000;
		}
		else if ((sdlgamepad.leftStickX > 0.5f) || sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
		{
			ff8_externals.dinput_gamepad_state->lX = -0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 9000;
		}

		lY = int(sdlgamepad.leftStickY * 0x80);
		lX = int(sdlgamepad.leftStickX * 0x80);
		rY = int(sdlgamepad.rightStickY * 0x80);
		rX = int(sdlgamepad.rightStickX * 0x80);

		if (sdlgamepad.rightStickY > 0.5f)
			ff8_externals.dinput_gamepad_state->lRy = 0xFFFFFFFFFFFFFFFF;
		else if (sdlgamepad.rightStickY < -0.5f)
			ff8_externals.dinput_gamepad_state->lRy = -0xFFFFFFFFFFFFFFFF;

		if (sdlgamepad.rightStickX > 0.5f)
			ff8_externals.dinput_gamepad_state->lRx = -0xFFFFFFFFFFFFFFFF;
		else if (sdlgamepad.rightStickX < -0.5f)
			ff8_externals.dinput_gamepad_state->lRx = 0xFFFFFFFFFFFFFFFF;

		ff8_externals.dinput_gamepad_state->lZ = 0;
		ff8_externals.dinput_gamepad_state->lRz = 0;
		ff8_externals.dinput_gamepad_state->rglSlider[0] = 0;
		ff8_externals.dinput_gamepad_state->rglSlider[1] = 0;
		ff8_externals.dinput_gamepad_state->rgdwPOV[1] = -1;
		ff8_externals.dinput_gamepad_state->rgdwPOV[2] = -1;
		ff8_externals.dinput_gamepad_state->rgdwPOV[3] = -1;
		ff8_externals.dinput_gamepad_state->rgbButtons[0] = sdlgamepad.IsPressed(steam_stock_launcher ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_WEST) ? 0x80 : 0; // Cross (Steam)/Square
		ff8_externals.dinput_gamepad_state->rgbButtons[1] = sdlgamepad.IsPressed(steam_stock_launcher ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH) ? 0x80 : 0; // Circle (Steam)/Cross
		ff8_externals.dinput_gamepad_state->rgbButtons[2] = sdlgamepad.IsPressed(steam_stock_launcher ? SDL_GAMEPAD_BUTTON_WEST : SDL_GAMEPAD_BUTTON_EAST) ? 0x80 : 0; // Square (Steam)/Circle
		ff8_externals.dinput_gamepad_state->rgbButtons[3] = sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_NORTH) ? 0x80 : 0; // Triangle
		ff8_externals.dinput_gamepad_state->rgbButtons[4] = sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) ? 0x80 : 0; // L1
		ff8_externals.dinput_gamepad_state->rgbButtons[5] = sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ? 0x80 : 0; // R1
		ff8_externals.dinput_gamepad_state->rgbButtons[6] = (steam_stock_launcher ? sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_BACK) : sdlgamepad.leftTrigger > 0.85f) ? 0x80 : 0; // SELECT (Steam)/L2
		ff8_externals.dinput_gamepad_state->rgbButtons[7] = (steam_stock_launcher ? sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_START) : sdlgamepad.rightTrigger > 0.85f) ? 0x80 : 0; // START (Steam)/R2
		ff8_externals.dinput_gamepad_state->rgbButtons[8] = (steam_stock_launcher ? sdlgamepad.leftTrigger > 0.85f : sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_BACK)) ? 0x80 : 0; // L2 (Steam)/SELECT
		ff8_externals.dinput_gamepad_state->rgbButtons[9] = (steam_stock_launcher ? sdlgamepad.rightTrigger > 0.85f : sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_START)) ? 0x80 : 0; // R2 (Steam)/START
		ff8_externals.dinput_gamepad_state->rgbButtons[10] = sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_LEFT_STICK) ? 0x80 : 0; // L3
		ff8_externals.dinput_gamepad_state->rgbButtons[11] = sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_RIGHT_STICK) ? 0x80 : 0; // R3
		ff8_externals.dinput_gamepad_state->rgbButtons[12] = sdlgamepad.IsPressed(SDL_GAMEPAD_BUTTON_GUIDE) ? 0x80 : 0; // PS Button
	}
	else if (xinput_connected)
	{
		if (!gamepad.Refresh() || !gamehacks.canInputBeProcessed()) return 0;

		if ((gamepad.leftStickY > 0.5f) || gamepad.IsPressed(XINPUT_GAMEPAD_DPAD_UP))
		{
			ff8_externals.dinput_gamepad_state->lY = 0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 0;
		}
		else if ((gamepad.leftStickY < -0.5f) || gamepad.IsPressed(XINPUT_GAMEPAD_DPAD_DOWN))
		{
			ff8_externals.dinput_gamepad_state->lY = -0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 18000;
		}

		if ((gamepad.leftStickX < -0.5f) || gamepad.IsPressed(XINPUT_GAMEPAD_DPAD_LEFT))
		{
			ff8_externals.dinput_gamepad_state->lX = 0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 27000;
		}
		else if ((gamepad.leftStickX > 0.5f) || gamepad.IsPressed(XINPUT_GAMEPAD_DPAD_RIGHT))
		{
			ff8_externals.dinput_gamepad_state->lX = -0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 9000;
		}

		lY = int(gamepad.leftStickY * 0x80);
		lX = int(gamepad.leftStickX * 0x80);
		rY = int(gamepad.rightStickY * 0x80);
		rX = int(gamepad.rightStickX * 0x80);

		if (gamepad.rightStickY > 0.5f)
			ff8_externals.dinput_gamepad_state->lRy = 0xFFFFFFFFFFFFFFFF;
		else if (gamepad.rightStickY < -0.5f)
			ff8_externals.dinput_gamepad_state->lRy = -0xFFFFFFFFFFFFFFFF;

		if (gamepad.rightStickX > 0.5f)
			ff8_externals.dinput_gamepad_state->lRx = -0xFFFFFFFFFFFFFFFF;
		else if (gamepad.rightStickX < -0.5f)
			ff8_externals.dinput_gamepad_state->lRx = 0xFFFFFFFFFFFFFFFF;

		ff8_externals.dinput_gamepad_state->lZ = 0;
		ff8_externals.dinput_gamepad_state->lRz = 0;
		ff8_externals.dinput_gamepad_state->rglSlider[0] = 0;
		ff8_externals.dinput_gamepad_state->rglSlider[1] = 0;
		ff8_externals.dinput_gamepad_state->rgdwPOV[1] = -1;
		ff8_externals.dinput_gamepad_state->rgdwPOV[2] = -1;
		ff8_externals.dinput_gamepad_state->rgdwPOV[3] = -1;
		ff8_externals.dinput_gamepad_state->rgbButtons[0] = gamepad.IsPressed(steam_stock_launcher ? XINPUT_GAMEPAD_A : XINPUT_GAMEPAD_X) ? 0x80 : 0; // Cross (Steam)/Square
		ff8_externals.dinput_gamepad_state->rgbButtons[1] = gamepad.IsPressed(steam_stock_launcher ? XINPUT_GAMEPAD_B : XINPUT_GAMEPAD_A) ? 0x80 : 0; // Circle (Steam)/Cross
		ff8_externals.dinput_gamepad_state->rgbButtons[2] = gamepad.IsPressed(steam_stock_launcher ? XINPUT_GAMEPAD_X : XINPUT_GAMEPAD_B) ? 0x80 : 0; // Square (Steam)/Circle
		ff8_externals.dinput_gamepad_state->rgbButtons[3] = gamepad.IsPressed(XINPUT_GAMEPAD_Y) ? 0x80 : 0; // Triangle
		ff8_externals.dinput_gamepad_state->rgbButtons[4] = gamepad.IsPressed(XINPUT_GAMEPAD_LEFT_SHOULDER) ? 0x80 : 0; // L1
		ff8_externals.dinput_gamepad_state->rgbButtons[5] = gamepad.IsPressed(XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 0x80 : 0; // R1
		ff8_externals.dinput_gamepad_state->rgbButtons[6] = (steam_stock_launcher ? gamepad.IsPressed(XINPUT_GAMEPAD_BACK) : gamepad.leftTrigger > 0.85f) ? 0x80 : 0; // SELECT (Steam)/L2
		ff8_externals.dinput_gamepad_state->rgbButtons[7] = (steam_stock_launcher ? gamepad.IsPressed(XINPUT_GAMEPAD_START) : gamepad.rightTrigger > 0.85f) ? 0x80 : 0; // START (Steam)/R2
		ff8_externals.dinput_gamepad_state->rgbButtons[8] = (steam_stock_launcher ? gamepad.leftTrigger > 0.85f : gamepad.IsPressed(XINPUT_GAMEPAD_BACK)) ? 0x80 : 0; // L2 (Steam)/SELECT
		ff8_externals.dinput_gamepad_state->rgbButtons[9] = (steam_stock_launcher ? gamepad.rightTrigger > 0.85f : gamepad.IsPressed(XINPUT_GAMEPAD_START)) ? 0x80 : 0; // R2 (Steam)/START
		ff8_externals.dinput_gamepad_state->rgbButtons[10] = gamepad.IsPressed(XINPUT_GAMEPAD_LEFT_THUMB) ? 0x80 : 0; // L3
		ff8_externals.dinput_gamepad_state->rgbButtons[11] = gamepad.IsPressed(XINPUT_GAMEPAD_RIGHT_THUMB) ? 0x80 : 0; // R3
		ff8_externals.dinput_gamepad_state->rgbButtons[12] = gamepad.IsPressed(0x400) ? 0x80 : 0; // PS Button
	}
	else
	{
		if (!joystick.Refresh() || !gamehacks.canInputBeProcessed()) return 0;

		if ((joystick.GetState()->lY < joystick.GetDeadZone(-0.5f)) || joystick.GetState()->rgdwPOV[0] == 0)
		{
			ff8_externals.dinput_gamepad_state->lY = 0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 0;
		}
		else if ((joystick.GetState()->lY > joystick.GetDeadZone(0.5f)) || joystick.GetState()->rgdwPOV[0] == 18000)
		{
			ff8_externals.dinput_gamepad_state->lY = -0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 18000;
		}

		if ((joystick.GetState()->lX < joystick.GetDeadZone(-0.5f)) || joystick.GetState()->rgdwPOV[0] == 27000)
		{
			ff8_externals.dinput_gamepad_state->lX = 0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 27000;
		}
		else if ((joystick.GetState()->lX > joystick.GetDeadZone(0.5f)) || joystick.GetState()->rgdwPOV[0] == 9000)
		{
			ff8_externals.dinput_gamepad_state->lX = -0xFFFFFFFFFFFFFFFF;
			ff8_externals.dinput_gamepad_state->rgdwPOV[0] = 9000;
		}

		lY = -int(joystick.GetState()->lY * 0x80 / SHRT_MAX);
		lX = int(joystick.GetState()->lX * 0x80 / SHRT_MAX);
		rY = -int(joystick.GetState()->lRy * 0x80 / SHRT_MAX);
		rX = int(joystick.GetState()->lRx * 0x80 / SHRT_MAX);

		if (joystick.GetState()->lRy < joystick.GetDeadZone(-0.5f))
			ff8_externals.dinput_gamepad_state->lRy = 0xFFFFFFFFFFFFFFFF;
		else if (joystick.GetState()->lRy > joystick.GetDeadZone(0.5f))
			ff8_externals.dinput_gamepad_state->lRy = -0xFFFFFFFFFFFFFFFF;

		if (joystick.GetState()->lRx > joystick.GetDeadZone(0.5f))
			ff8_externals.dinput_gamepad_state->lRx = -0xFFFFFFFFFFFFFFFF;
		else if (joystick.GetState()->lRx < joystick.GetDeadZone(-0.5f))
			ff8_externals.dinput_gamepad_state->lRx = 0xFFFFFFFFFFFFFFFF;

		ff8_externals.dinput_gamepad_state->lZ = 0;
		ff8_externals.dinput_gamepad_state->lRz = 0;
		ff8_externals.dinput_gamepad_state->rglSlider[0] = 0;
		ff8_externals.dinput_gamepad_state->rglSlider[1] = 0;
		ff8_externals.dinput_gamepad_state->rgdwPOV[1] = -1;
		ff8_externals.dinput_gamepad_state->rgdwPOV[2] = -1;
		ff8_externals.dinput_gamepad_state->rgdwPOV[3] = -1;
		ff8_externals.dinput_gamepad_state->rgbButtons[0] = joystick.GetState()->rgbButtons[0] & 0x80 ? 0x80 : 0; // Square
		ff8_externals.dinput_gamepad_state->rgbButtons[1] = joystick.GetState()->rgbButtons[1] & 0x80 ? 0x80 : 0; // Cross
		ff8_externals.dinput_gamepad_state->rgbButtons[2] = joystick.GetState()->rgbButtons[2] & 0x80 ? 0x80 : 0; // Circle
		ff8_externals.dinput_gamepad_state->rgbButtons[3] = joystick.GetState()->rgbButtons[3] & 0x80 ? 0x80 : 0; // Triangle
		ff8_externals.dinput_gamepad_state->rgbButtons[4] = joystick.GetState()->rgbButtons[4] & 0x80 ? 0x80 : 0; // L1
		ff8_externals.dinput_gamepad_state->rgbButtons[5] = joystick.GetState()->rgbButtons[5] & 0x80 ? 0x80 : 0; // R1
		ff8_externals.dinput_gamepad_state->rgbButtons[6] = joystick.GetState()->rgbButtons[6] & 0x80 ? 0x80 : 0; // L2
		ff8_externals.dinput_gamepad_state->rgbButtons[7] = joystick.GetState()->rgbButtons[7] & 0x80 ? 0x80 : 0; // R2
		ff8_externals.dinput_gamepad_state->rgbButtons[8] = joystick.GetState()->rgbButtons[8] & 0x80 ? 0x80 : 0; // SELECT
		ff8_externals.dinput_gamepad_state->rgbButtons[9] = joystick.GetState()->rgbButtons[9] & 0x80 ? 0x80 : 0; // START
		ff8_externals.dinput_gamepad_state->rgbButtons[10] = joystick.GetState()->rgbButtons[10] & 0x80 ? 0x80 : 0; // L3
		ff8_externals.dinput_gamepad_state->rgbButtons[11] = joystick.GetState()->rgbButtons[11] & 0x80 ? 0x80 : 0; // R3
		ff8_externals.dinput_gamepad_state->rgbButtons[12] = joystick.GetState()->rgbButtons[12] & 0x80 ? 0x80 : 0; // PS Button
	}

	left_stick_y = -lY + 0x80;
	if (left_stick_y > 255) left_stick_y = 255;
	if (left_stick_y < 0) left_stick_y = 0;

	left_stick_x = lX + 0x80;
	if (left_stick_x > 255) left_stick_x = 255;
	if (left_stick_x < 0) left_stick_x = 0;

	int mul = (left_stick_x - 128) * (left_stick_x - 128) + (left_stick_y - 128) * (left_stick_y - 128);
	if (mul < 1600) {
		left_stick_y = 0x80;
		left_stick_x = 0x80;
	}

	right_stick_y = -rY + 0x80;
	if (right_stick_y > 255) right_stick_y = 255;
	if (right_stick_y < 0) right_stick_y = 0;

	right_stick_x = rX + 0x80;
	if (right_stick_x > 255) right_stick_x = 255;
	if (right_stick_x < 0) right_stick_x = 0;

	mul = (right_stick_x - 128) * (right_stick_x - 128) + (right_stick_y - 128) * (right_stick_y - 128);
	if (mul < 1600) {
		right_stick_y = 0x80;
		right_stick_x = 0x80;
	}

	return ff8_externals.dinput_gamepad_state;
}

int ff8_get_input_device_capabilities_number_of_buttons(int a1)
{
	return (use_sdl_gamepad || xinput_connected) ? 10 : std::min<DWORD>(joystick.GetCaps()->dwButtons, 10);
}

int ff8_draw_gamepad_icon_or_keyboard_key(int a1, ff8_draw_menu_sprite_texture_infos *draw_infos, int icon_id, uint16_t x, uint16_t y)
{
	// Keep the "keys" if it is a keyboard and not a gamepad
	if (icon_id >= 128 && icon_id < 140)
	{
		BYTE is_gamepad = *ff8_externals.engine_gamepad_button_pressed != 0;

		if (is_gamepad)
		{
			int val = ((int(*)(int,int,int))ff8_externals.get_command_key)(is_gamepad, icon_id - 128, 0);

			if (val == 0) {
				val = ((int(*)(int,int,int))ff8_externals.get_command_key)(!is_gamepad, icon_id - 128, 0);
			}

			int rgbButton = val - 224;

			switch (rgbButton)
			{
				case 0: // Cross (Steam)/Square
					return steam_stock_launcher ? 134 : 135;
				case 1: // Circle (Steam)/Cross
					return steam_stock_launcher ? 133 : 134;
				case 2: // Square (Steam)/Circle
					return steam_stock_launcher ? 135 : 133;
				case 3: // Triangle
					return 132;
				case 4: // L1
					return 130;
				case 5: // R1
					return 131;
				case 6: // SELECT (Steam)/L2
					return steam_stock_launcher ? 136 : 128;
				case 7: // START (Steam)/R2
					return steam_stock_launcher ? 139 : 129;
				case 8: // L2 (Steam)/SELECT
					return steam_stock_launcher ? 128 : 136;
				case 9: // R2 (Steam)/START
					return steam_stock_launcher ? 129 : 139;
			}
		}

		((void(*)(int, ff8_draw_menu_sprite_texture_infos*, int, uint16_t, uint16_t))ff8_externals.draw_controller_or_keyboard_icons)(a1, draw_infos, icon_id, x, y);

		return -1;
	}

	return icon_id;
}

unsigned int *ff8_draw_icon_get_icon_sp1_infos(int icon_id, int &states_count)
{
	int *icon_sp1_data = ((int*(*)())ff8_externals.get_icon_sp1_data)();

	if (icon_id >= icon_sp1_data[0])
	{
		states_count = 0;

		return nullptr;
	}

	states_count = HIWORD(icon_sp1_data[icon_id + 1]);

	return (unsigned int *)((char *)icon_sp1_data + uint16_t(icon_sp1_data[icon_id + 1]));
}

ff8_draw_menu_sprite_texture_infos *ff8_draw_icon_or_key(
	int a1, ff8_draw_menu_sprite_texture_infos *draw_infos,
	int icon_id, uint16_t x, uint16_t y, int a6, int field10_modifier = 0,
	bool no_a6_mask = false,
	bool override_field4_8_with_a6 = false,
	bool yfix = false
) {
	icon_id = ff8_draw_gamepad_icon_or_keyboard_key(a1, draw_infos, icon_id, x, y);
	if (icon_id < 0)
	{
		return draw_infos;
	}

	int states_count = 0;
	unsigned int *sp1_section_data = ff8_draw_icon_get_icon_sp1_infos(icon_id, states_count);

	if (sp1_section_data == nullptr)
	{
		return draw_infos;
	}

	for (int i = states_count; i > 0; --i)
	{
		draw_infos->field_0 = 0x5000000;
		draw_infos->field_10 = (sp1_section_data[0] & 0x7CFFFFF) + ((0x3810 + field10_modifier) << 16);
		if (override_field4_8_with_a6)
		{
			draw_infos->field_8 = ((a6 & 0xFFFFFF) | 0x64000000) | (((HIBYTE(a6) >> 1) & 2) << 24);
			draw_infos->field_4 = ((HIBYTE(a6) & 3) << 5) | 0xE100041E;
		}
		else
		{
			draw_infos->field_8 = no_a6_mask ? a6 | (((sp1_section_data[0] >> 26) & 2) << 24) : (a6 & 0x3FFFFFF) | (((sp1_section_data[0] >> 26) & 2 | 0x64) << 24);
			draw_infos->field_4 = (sp1_section_data[0] >> 25) & 0x60 | 0xE100041E;
		}
		draw_infos->field_14 = sp1_section_data[1] & 0xFF00FF;
		draw_infos->x_related = x + (int16_t(sp1_section_data[1]) >> 8);
		draw_infos->y_related = y + (int32_t(sp1_section_data[1]) >> 24);
		if (yfix && *ff8_externals.battle_boost_cross_icon_display_1D76604) {
			*((uint8_t *)draw_infos + 11) |= 2u;
		}
		((void(*)(int, ff8_draw_menu_sprite_texture_infos*))ff8_externals.sub_49BB30)(a1, draw_infos);
		if (!no_a6_mask) {
			draw_infos += 1;
		}
		sp1_section_data += 2;
	}

	return draw_infos;
}

ff8_draw_menu_sprite_texture_infos *ff8_draw_icon_or_key1(int a1, ff8_draw_menu_sprite_texture_infos *draw_infos, int icon_id, uint16_t x, uint16_t y, int a6)
{
	return ff8_draw_icon_or_key(a1, draw_infos, icon_id, x, y, a6);
}

ff8_draw_menu_sprite_texture_infos *ff8_draw_icon_or_key2(int a1, ff8_draw_menu_sprite_texture_infos *draw_infos, int *icon_sp1_data, int icon_id, uint16_t x, uint16_t y)
{
	return ff8_draw_icon_or_key(a1, draw_infos, icon_id, x, y, *ff8_externals.dword_1D2B808);
}

ff8_draw_menu_sprite_texture_infos *ff8_draw_icon_or_key3(int a1, ff8_draw_menu_sprite_texture_infos *draw_infos, int *icon_sp1_data, int icon_id, uint16_t x, uint16_t y, int a6)
{
	return ff8_draw_icon_or_key(a1, draw_infos, icon_id, x, y, a6, 0, true);
}

ff8_draw_menu_sprite_texture_infos *ff8_draw_icon_or_key4(int a1, ff8_draw_menu_sprite_texture_infos *draw_infos, int *icon_sp1_data, int icon_id, uint16_t x, uint16_t y, int a6, int a7)
{
	return ff8_draw_icon_or_key(a1, draw_infos, icon_id, x, y, a6, a7, false, true);
}

ff8_draw_menu_sprite_texture_infos *ff8_draw_icon_or_key5(int a1, ff8_draw_menu_sprite_texture_infos *draw_infos, int icon_id, uint16_t x, uint16_t y, int a6, int a7)
{
	return ff8_draw_icon_or_key(a1, draw_infos, icon_id, x, y, a6, a7, true, false, true);
}

ff8_draw_menu_sprite_texture_infos_short *ff8_draw_icon_or_key6(int a1, ff8_draw_menu_sprite_texture_infos_short *draw_infos, int icon_id, uint16_t x, uint16_t y, int a6, int a7) {
	// We should not cast like this, but that's what the game does
	icon_id = ff8_draw_gamepad_icon_or_keyboard_key(a1, reinterpret_cast<ff8_draw_menu_sprite_texture_infos *>(draw_infos), icon_id, x, y);
	if (icon_id < 0)
	{
		return draw_infos;
	}

	int states_count = 0;
	unsigned int *sp1_section_data = ff8_draw_icon_get_icon_sp1_infos(icon_id, states_count);

	if (sp1_section_data == nullptr)
	{
		return draw_infos;
	}

	for (int i = states_count; i > 0; --i)
	{
		draw_infos->field_0 = 0x4000000;
		draw_infos->field_C = (sp1_section_data[0] & 0x7CFFFFF) + ((0x3810 + a7) << 16);
		draw_infos->field_4 = a6 | (((sp1_section_data[0] >> 26) & 2) << 24);
		draw_infos->field_10 = sp1_section_data[1] & 0xFF00FF;
		draw_infos->x_related = x + (int16_t(sp1_section_data[1]) >> 8);
		draw_infos->y_related = y + (sp1_section_data[1] >> 24);
		((void(*)(int, ff8_draw_menu_sprite_texture_infos_short*))ff8_externals.sub_49FE60)(a1, draw_infos);
		draw_infos += 1;
		sp1_section_data += 2;
	}

	return draw_infos;
}

int ff8_get_key_state(WPARAM dinput_scan_code)
{
	int virt_key = 0;

	if (!*ff8_externals.keyboard_state || dinput_scan_code > 0xFF) {
		return 0; // What are you doing, William?
	}

	switch (dinput_scan_code) {
		// Force real Q key, instead of directinput's positional DIK_Q (which is A in Azerty keyboards)
		case DIK_Q:
			virt_key = 'Q';
			break;
		case DIK_R:
			virt_key = 'R';
			break;
		// Keep the old implementation to prevent performance issues with using GetKeyState on each frame
		default: // DIK_LCONTROL, DIK_RCONTROL, and everything else
			return (*ff8_externals.keyboard_state)[dinput_scan_code];
	}

	return (GetKeyState(virt_key) & 0x8000) != 0;
}

int is_q_pressed = 0;

int ff8_is_window_active()
{
	if (gameHwnd == GetActiveWindow() || ff8_always_capture_input)
	{
		ff8_externals.engine_eval_keyboard_gamepad_input();
		ff8_externals.has_keyboard_gamepad_input();

		if (simulate_OK_button)
		{
			// Flag the button OK as pressed
			ff8_externals.engine_input_confirmed_buttons[1] = ff8_externals.engine_input_valid_buttons[1] = 0x40;

			// End simulation right here before we press this button by mistake in other windows
			simulate_OK_button = false;
		}

		// Allow to quit the game anywhere
		if ((ff8_get_key_state(DIK_LCONTROL) || ff8_get_key_state(DIK_RCONTROL)) && ff8_get_key_state(DIK_Q))
		{
			if (is_q_pressed > 20)
			{
				((ff8_game_obj *)common_externals.get_game_object())->do_quit = 1;
				is_q_pressed = 0;
			}
			else
			{
				is_q_pressed += 1;
			}
		}
		else
		{
			is_q_pressed = 0;
		}
	}

	return 0;
}

bool ff8_skip_movies()
{
	uint32_t mode = getmode_cached()->driver_mode;

	if (ff8_externals.movie_object->movie_is_playing)
	{
		if (mode == MODE_FIELD)
		{
			// Prevent game acting weird or wrong if movie is skipped
			if (
				*common_externals.current_field_id == 339 // dosea_2
			)
			{
				return false;
			}

			// Force last frame for field scripts
			ff8_externals.movie_object->movie_current_frame = 0xFFFF;
			(*ff8_externals.savemap_field)->current_frame = 0xFFFF;
			ff8_externals.sub_5304B0();
		}
		else if (mode == MODE_CREDITS)
		{
			if (enable_ffmpeg_videos)
				ff8_stop_movie();
			else
				((void(*)())common_externals.stop_movie)();
		}

		return true;
	}
	else
	{
		if (mode == MODE_CREDITS)
		{
			*ff8_externals.credits_counter = 256;
			*ff8_externals.credits_loop_state = 18;

			return true;
		}
	}

	return false;
}

int ff8_opcode_battle(int unk)
{
	int ret = ff8_opcode_old_battle(unk);

	next_battle_scene_id = *ff8_externals.battle_encounter_id;
	next_music_is_battle = true;

	return ret;
}

int ff8_toggle_battle_field()
{
	int ret = 0;

	if (gamehacks.wantsBattle()) ret = ff8_externals.sub_47CA90();

	if (ret > 0)
	{
		next_battle_scene_id = *ff8_externals.battle_encounter_id;
		next_music_is_battle = true;
	}

	return ret;
}

int ff8_toggle_battle_worldmap(WORD* battle_id)
{
	int ret = 0;

	if (gamehacks.wantsBattle()) ret = ff8_externals.sub_541C80(battle_id);

	if (ret > 0)
	{
		next_battle_scene_id = *battle_id;
		next_music_is_battle = true;
	}

	return ret;
}

uint32_t ff8_retry_configured_drive(char* filename, uint8_t* data)
{
	int32_t res = ff8_externals.sm_pc_read(filename, data);

	if (!res) {
		char dataDrive[8];
		char modifiedFilename[MAX_PATH];

		ff8_externals.reg_get_data_drive(dataDrive, 4);
		dataDrive[7] = '\0'; // For safety

		if (trace_files || trace_all) ffnx_trace("%s: filename=%s, dataDrive=%s, diskDataPath=%s\n", __func__, filename, dataDrive, ff8_externals.disk_data_path);

		if (GetDriveTypeA(dataDrive) == DRIVE_CDROM) {
			strcpy(modifiedFilename, dataDrive);
			char* filenameNoDrive = strrchr(filename, ':');
			if (filenameNoDrive != nullptr) {
				strncat(modifiedFilename, filenameNoDrive + 1, MAX_PATH);

				if (strncmp(modifiedFilename, filename, MAX_PATH) != 0) {
					res = ff8_externals.sm_pc_read(modifiedFilename, data);

					if (res) {
						strncpy(ff8_externals.disk_data_path, dataDrive, 260);
						strncat(ff8_externals.disk_data_path, "\\", 260);

						if (trace_files || trace_all) ffnx_trace("%s: diskDataPath changed %s\n", __func__, ff8_externals.disk_data_path);
					}
				}
			}
		}
	}

	return res;
}

uint32_t ff8_credits_main_loop_gfx_begin_scene(uint32_t unknown, struct game_obj *game_object)
{
	if (drawFFNxLogoFrame(game_object)) {
		ff8_externals.input_fill_keystate();

		if (((ff8_externals.input_get_keyscan(0, 0) & ff8_externals.input_get_keyscan(1, 0)) & 0xF0) != 0) {
			stopDrawFFNxLogo();
		}

		return 0;
	}

	return common_begin_scene(unknown, game_object);
}

int credits_controller_music_play(void *data)
{
	int ret = ((int(*)(void*))ff8_externals.sdmusicplay)(data);

	intro_credits_music_start_time = highResolutionNow();

	return ret;
}

int credits_controller_input_call()
{
	if (*ff8_externals.credits_counter == 0) {
		int frameCountAdjusted =
			intro_credits_frames_between_music_start_and_first_image
			+ *ff8_externals.credits_current_step_image * intro_credits_adjusted_frames
			+ intro_credits_fade_frames;

		int realFramesEllapsed = int((60.0 / 1000.0) * (elapsedMicroseconds(intro_credits_music_start_time) / 1000.0));
		int waitFor = frameCountAdjusted - realFramesEllapsed;

		// Add frames on each image display
		if (waitFor > 0) {
			*ff8_externals.credits_current_image_global_counter_start += waitFor;
		}
	}

	return ((int(*)())ff8_externals.sub_52FE80)();
}

char new_game_text_cache[64] = "";
char load_game_text_cache[64] = "";

char *ff8_get_text_cached(int pool_id, int cat_id, int text_id, int a4, char *cache)
{
	if (*cache == '\0') {
		memcpy(cache, ((char*(*)(int,int,int,int))ff8_externals.get_text_data)(pool_id, cat_id, text_id, a4), sizeof(new_game_text_cache));
	}

	return cache;
}

char *ff8_get_text_cached_new_game(int pool_id, int cat_id, int text_id, int a4)
{
	return ff8_get_text_cached(pool_id, cat_id, text_id, a4, new_game_text_cache);
}

char *ff8_get_text_cached_load_game(int pool_id, int cat_id, int text_id, int a4)
{
	return ff8_get_text_cached(pool_id, cat_id, text_id, a4, load_game_text_cache);
}

int ff8_create_save_file(int slot, char* save)
{
	int ret = ((int(*)(int,char*))ff8_externals.create_save_file_sub_4C6E50)(slot, save);

	uint8_t savefile_slot = slot > 1 ? 2 : 1;
	uint8_t savefile_save = atoi(&save[strlen(save) - 2]) + 1;
	ffnx_trace("Save: user saved in slot%d_save%02i\n", savefile_slot, savefile_save);
	metadataPatcher.updateFF8(savefile_slot, savefile_save);

	return ret;
}

int ff8_create_save_file_chocobo_world(int unused, int data_source, int offset, size_t size)
{
	int ret = ((int(*)(int,int,int,size_t))ff8_externals.create_save_chocobo_world_file_sub_4C6620)(unused, data_source, offset, size);

	ffnx_trace("Save: user saved in slot:choco\n");
	if (ret > 0) metadataPatcher.updateFF8(3, 0);

	return ret;
}

int ff8_cardgame_postgame_func_534BC0()
{
	g_FF8SteamAchievements->unlockPlayTripleTriadAchievement();
	return ff8_externals.cardgame_func_534BC0();
}

void ff8_cardgame_enter_hook_sub_460B60(float a1)
{
	g_FF8SteamAchievements->initOwnedTripleTriadRareCards(ff8_externals.savemap->triple_triad);
	((void(*)(float))ff8_externals.sub_460B60)(a1);
}

void ff8_cardgame_exit_hook_sub_4972A0()
{
	g_FF8SteamAchievements->unlockLoserTripleTriadAchievement(ff8_externals.savemap->triple_triad);
	((void(*)())ff8_externals.sub_4972A0)();
}

int ff8_cardgame_add_card_to_squall_original(int card_idx)
{
	// update known cards
	if (card_idx >= 77)
		ff8_externals.savemap->triple_triad.cards_rare[(card_idx - 77) / 8] |= 1 << ((card_idx - 77) % 8);
	else
		ff8_externals.savemap->triple_triad.cards[card_idx] |= 0x80u;

	// add card to squall
	if (card_idx >= 77)
	{
		ff8_externals.savemap->triple_triad.card_locations[card_idx - 77] = 240; // SQUALL
		return 0;
	}
	else if ((ff8_externals.savemap->triple_triad.cards[card_idx] & 0x7Fu) >= 100)
	{
		return -1;
	}
	else
	{
		++ff8_externals.savemap->triple_triad.cards[card_idx];
		return 0;
	}
}

int ff8_cardgame_add_card_to_squall(int card_idx)
{
	int ret = ff8_cardgame_add_card_to_squall_original(card_idx);
	g_FF8SteamAchievements->unlockCollectorTripleTriadAchievement(ff8_externals.savemap->triple_triad);
	return ret;
}

int ff8_cardgame_update_card_with_location_original(int card_idx, int card_location)
{
	if ( card_idx >= 77 )
	{
		ff8_externals.savemap->triple_triad.card_locations[card_idx - 77] = card_location;
		return 0;
	}
	else
	{
		byte card_value = ff8_externals.savemap->triple_triad.cards[card_idx];
		if ( card_location == 240 ) // SQUALL location
		{
			if ((card_value & 0x7Fu) < 100)
			{
				ff8_externals.savemap->triple_triad.cards[card_idx] = card_value + 1;
				return 0;
			}
		}
		else if ((card_value & 0x7F) != 0)
		{
			ff8_externals.savemap->triple_triad.cards[card_idx] = card_value - 1;
			return 0;
		}
		return -1;
	}
}

int ff8_cardgame_update_card_with_location(int card_idx, int card_location)
{
	int ret = ff8_cardgame_update_card_with_location_original(card_idx, card_location);
	if (card_location == 240) // Squall location
	{
		g_FF8SteamAchievements->unlockCollectorTripleTriadAchievement(ff8_externals.savemap->triple_triad);
	}
	return ret;
}

int ff8_cardgame_sub_535D00(void* tt_data)
{
	uint16_t prev_card_wins = ff8_externals.savemap->triple_triad.victory_count;
	int ret = ff8_externals.cardgame_sub_535D00(tt_data);
	if (ff8_externals.savemap->triple_triad.victory_count > prev_card_wins)
	{
		g_FF8SteamAchievements->increaseCardWinsAndUnlockProfessionalAchievement();
	}
	return ret;
}

int ff8_field_opcode_CARDGAME(int field_data)
{
	int ret = ff8_externals.opcode_cardgame(field_data);
	if (ret == 2) // cardgame exited
	{
		uint8_t deck_id = *ff8_externals.cardgame_deck_id_1DCD7AD;
		int cardgame_result = *(int*)(field_data + 324);
		if (deck_id == 202 && cardgame_result == 0) // Won against quistis
		{
			g_FF8SteamAchievements->unlockCardClubMasterAchievement(ff8_externals.savemap->field);
		}
	}
	return ret;
}

void ff8_enable_gf_sub_47E480(int gf_idx)
{
	ff8_externals.savemap->gfs[gf_idx].exists |= 1u;
	// NOTE: This function for Diablos is called when starting his battle
	if (gf_idx != SteamAchievementsFF8::DIABLOS_GF_IDX) {
		g_FF8SteamAchievements->unlockGuardianForceAchievement(gf_idx);
	}
}

void ff8_update_seed_exp_4C30E0(int seed_lvl)
{
	ff8_externals.update_seed_exp_4C30E0(seed_lvl);
	g_FF8SteamAchievements->unlockTopSeedRankAchievement(ff8_externals.savemap->field_header.seedExp);
}

int ff8_field_opcode_POPM_W(void* field_data, int memory_offset)
{
	int ret = ff8_externals.opcode_popm_w(field_data, memory_offset);
	if (memory_offset == 16) // seed exp
	{
		g_FF8SteamAchievements->unlockTopSeedRankAchievement(ff8_externals.savemap->field_header.seedExp);
	}
	if (memory_offset == 256 && ff8_externals.savemap->field.game_moment == 3000) // Ragnarok found
	{
		g_FF8SteamAchievements->unlockRagnarokAchievement();
	}
	return ret;
}

int ff8_field_opcode_POPM_B(void* field_data, int memory_offset)
{
	int ret = ff8_externals.opcode_popm_b(field_data, memory_offset);
	if (memory_offset == 0x130 || memory_offset == 0x131) // timber maniacs offset
	{
		g_FF8SteamAchievements->unlockTimberManiacsAchievement(ff8_externals.savemap->field.timber_maniacs);
	}
	return ret;
}

int ff8_field_opcode_ADDSEEDLEVEL(void* field_data)
{
	int ret = ff8_externals.opcode_addseedlevel(field_data);
	g_FF8SteamAchievements->unlockTopSeedRankAchievement(ff8_externals.savemap->field_header.seedExp);
	return ret;
}

void ff8_field_update_seed_level()
{
	((void(*)())ff8_externals.field_update_seed_level_52B140)();
	g_FF8SteamAchievements->unlockTopSeedRankAchievement(ff8_externals.savemap->field_header.seedExp);
	g_FF8SteamAchievements->unlockMaxGilAchievement(ff8_externals.savemap->gil);
	g_FF8SteamAchievements->unlockFirstSalaryAchievement();
}

void ff8_worldmap_update_seed_level()
{
	((void(*)())ff8_externals.worldmap_update_seed_level_651C10)();
	g_FF8SteamAchievements->unlockTopSeedRankAchievement(ff8_externals.savemap->field_header.seedExp);
	g_FF8SteamAchievements->unlockMaxGilAchievement(ff8_externals.savemap->gil);
	g_FF8SteamAchievements->unlockFirstSalaryAchievement();
}

// Replacing a specific call that is called when player remodel weapon just before assigning the new
// weapon id to the character
int ff8_menu_junkshop_get_char_id_hook_4ABC40(int chars_available_bitmap, int char_idx)
{
	int char_id = ff8_externals.sub_4ABC40(chars_available_bitmap, char_idx);
	g_FF8SteamAchievements->initPreviousWeaponIdBeforeUpgrade(char_id, ff8_externals.savemap->chars[char_id].weapon_id);
	return char_id;
}

// Replacing a specific call that is called when player remodel weapon just after assigning the new
// weapon id to the character
int ff8_menu_junkshop_hook_4EA770(int a1, uint32_t a2)
{
	int ret = ff8_externals.sub_4EA770(a1, a2);
	g_FF8SteamAchievements->unlockUpgradeWeaponAchievement(*ff8_externals.savemap);
	return ret;
}

// Replacing a call done before computing max HP for a character in order to get the
// index "party_char_id"
void ff8_hook_sub_4954B0(int party_char_id)
{
	ff8_externals.sub_4954B0(party_char_id);
	g_FF8SteamAchievements->initStatCharIdUnderStatCompute(party_char_id);
}

int ff8_compute_char_max_hp_496310(int multiplier, int char_id)
{
	int max_hp_mul = ff8_externals.compute_char_max_hp_496310(multiplier, char_id);
	byte stat_char_id = g_FF8SteamAchievements->getStatCharIdUnderStatCompute();
	if (stat_char_id != 0xFFu) {
		int max_hp = ff8_externals.char_comp_stats_1CFF000[stat_char_id].unk3[14] * max_hp_mul / 100;
		g_FF8SteamAchievements->unlockMaxHpAchievement(max_hp);
	}
	return max_hp_mul;
}

int ff8_field_opcode_ADDGIL(void* field_data)
{
	int ret = ff8_externals.opcode_addgil(field_data);
	g_FF8SteamAchievements->unlockMaxGilAchievement(ff8_externals.savemap->gil);
	return ret;
}

void ff8_menu_shop_sub_4EBE40(byte* menu_data)
{
	uint16_t menu_op = *(uint16_t*)(menu_data + 16);
	bool is_menu_sell_buy = ((*ff8_externals.menu_data_1D76A9C) & 0x40) != 0;
	((void(*)(byte*))ff8_externals.menu_shop_sub_4EBE40)(menu_data);
	byte is_sell = *(byte*)(menu_data + 70);
	if (menu_op == 12 && is_menu_sell_buy && is_sell)
	{
		uint32_t gil = *(uint32_t*)(menu_data + 40);
		g_FF8SteamAchievements->unlockMaxGilAchievement(gil);
	}
}

int ff8_battle_menu_add_exp_and_bonus_496CB0(int party_char_id, uint16_t exp)
{
	byte char_id = *(ff8_externals.character_data_1CFE74C + party_char_id);
	int ret = ff8_externals.battle_menu_add_exp_and_stat_bonus_496CB0(party_char_id, exp);
	if (char_id != 0xFF) {
		int level = ff8_externals.get_char_level_4961D0(ff8_externals.savemap->chars[char_id].exp, char_id);
		g_FF8SteamAchievements->unlockTopLevelAchievement(level);
	}
	if (*ff8_externals.global_battle_encounter_id_1CFF6E0 == SteamAchievementsFF8::DIABLOS_ENCOUNTER_ID) {
		g_FF8SteamAchievements->unlockGuardianForceAchievement(SteamAchievementsFF8::DIABLOS_GF_IDX);
	}
	return ret;
}

// Replace a function that is called before increasing the kills of a character
void ff8_battle_after_enemy_kill_sub_494AF0(int party_char_id, int monster_id, int current_actor_second_byte, int a2)
{
	ff8_externals.battle_sub_494AF0(party_char_id, monster_id, current_actor_second_byte, a2);
	g_FF8SteamAchievements->increaseKillsAndTryUnlockAchievement();
}

int ff8_opcode_drawpoint_sub_4A0850(int a1, int draw_magic_count)
{
	int ret = ff8_externals.opcode_drawpoint_sub_4A0850(a1, draw_magic_count);
	g_FF8SteamAchievements->increaseMagicDrawsAndTryUnlockAchievement();
	return ret;
}

void ff8_set_drawpoint_state_52D190(uint8_t drawpoint_id, char value)
{
	ff8_externals.set_drawpoint_state_521D90(drawpoint_id, value);
	g_FF8SteamAchievements->increaseMagicDrawsAndTryUnlockAchievement();
}

int ff8_battle_get_magic_draw_amount_48FD20(int actor_idx, int monster_id, int magic_id)
{
	int ret = ff8_externals.battle_get_draw_magic_amount_48FD20(actor_idx, monster_id, magic_id);
	g_FF8SteamAchievements->increaseMagicStockAndTryUnlockAchievement();
	return ret;
}

char ff8_menu_use_item_sub_4F81F0(int menu_data_pointer)
{
	uint16_t mode = *(uint16_t*)(menu_data_pointer + 16);
	char ret = ff8_externals.menu_use_items_sub_4F81F0(menu_data_pointer);
	if (mode == 111) // Show quistis blue magic unlocked message
	{
		g_FF8SteamAchievements->unlockQuistisLimitBreaksAchievement(ff8_externals.savemap->lb.quistis_lb);
	}
	return ret;
}

int ff8_play_sfx_at_unlock_rinoa_limit_break(int a1, int a2, uint32_t a3, uint32_t a4)
{
	int ret = ((int(*)(int, int, uint32_t, uint32_t))ff8_externals.sfx_play_to_current_playing_channel)(a1, a2, a3, a4);
	g_FF8SteamAchievements->unlockRinoaLimitBreaksAchievement(ff8_externals.savemap->lb.angelo_completed_lb);
	return ret;
}

void ff8_obtain_proof_of_omega(int tut_info_id)
{
	ff8_externals.update_tutorial_info_4AD170(tut_info_id);
	g_FF8SteamAchievements->unlockOmegaDestroyedAchievement();
}

void ff8_battle_after_set_result_to_won_sub_494D40()
{
	ff8_externals.battle_sub_494D40();
	if (*ff8_externals.global_battle_encounter_id_1CFF6E0 == 750 && *ff8_externals.battle_result_state_1CFF6E7 == 4) { // Won Pupu encounter
		g_FF8SteamAchievements->unlockPupuQuestAchievement(ff8_externals.savemap->worldmap.pupu_quest);
	}
}

int ff8_menu_choco_add_item_to_player_47ED00(int item_id, char quantity)
{
	int ret = ff8_externals.add_item_to_player_sub_47ED00(item_id, quantity);
	g_FF8SteamAchievements->unlockChocoLootAchievement();
	return ret;
}

void ff8_menu_chocobo_sub_4FF8F0()
{
	ff8_externals.menu_chocobo_sub_4FF8F0();
	g_FF8SteamAchievements->unlockTopLevelBokoAchievement(ff8_externals.savemap->choco_world.level);
}

int ff8_world_sub_54D7E0(WORD* a1)
{
	bool obel_quest_was_finished = ff8_externals.savemap->worldmap.obel_quest[2] & 1;
	int ret = ((int(*)(WORD*))ff8_externals.sub_54D7E0)(a1);
	if (!obel_quest_was_finished && (ff8_externals.savemap->worldmap.obel_quest[2] & 1))
	{
		g_FF8SteamAchievements->unlockObelLakeQuestAchievement();
	}
	return ret;
}

// NOTE:Re-implementation of the function to add item to player items (sub_47ED00)
// because the original function in FF8 exe code has been completely replaced
int ff8_add_item_to_player(int item_id, char quantity)
{
	if (!item_id) {
		return 0;
	}

	savemap_ff8_item *items = ff8_externals.savemap->items.items;
	for (int i = 0; i < 198; ++i)
	{
		if (items[i].item_id == item_id )
		{
			items[i].item_quantity += quantity;
			if (items[i].item_quantity < 100) {
				return 0;
			} else {
				items[i].item_quantity = 100;
				return 1;
			}
		}
	}

	int open_slot = 0;
	for (open_slot = 0; open_slot < 198 && items[open_slot].item_id; open_slot++);
	if (open_slot >= 198) {
		return 1;
	}
	items[open_slot].item_id = item_id;
	items[open_slot].item_quantity += quantity;
	if (items[open_slot].item_quantity < 100) {
		return 0;
	} else {
		items[open_slot].item_quantity = 100;
		return 1;
	}
}

int ff8_add_item_to_player_wrapper(int item_id, char quantity)
{
	int ret = ff8_add_item_to_player(item_id, quantity);
	if (SteamAchievementsFF8::itemIsMagazine(item_id)) {
		g_FF8SteamAchievements->unlockMagazineAddictAchievement(ff8_externals.savemap->items);
	}
	return ret;
}

void ff8_menu_shop_update_gil_and_items(int gil)
{
	bool bought_magazine = false;
	savemap_ff8_item *items = ff8_externals.savemap->items.items;
	for (int i = 0; i < 198; ++i)
	{
		if (SteamAchievementsFF8::itemIsMagazine(items[i].item_id)
			&& ff8_externals.menu_shop_staged_items_1D8D058[items[i].item_id] > items[i].item_quantity)
		{
			bought_magazine = true;
			break;
		}
	}

	ff8_externals.menu_shop_update_gil_and_items_4EB9F0(gil);

	if (bought_magazine)
	{
		g_FF8SteamAchievements->unlockMagazineAddictAchievement(ff8_externals.savemap->items);
	}
}

// Armed on any frame spent outside battle; the first battle frame after it resets every
// piece of per-battle 30fps state (see ff8_bgate_battle_reset).
static bool ff8_bgate_left_battle = true;
static bool ff8_bgate_active = false; // (defined here: the frame limiter below needs it)
static int ff8_bgate_n = 2;

// 60 Hz pad sampling in battle at 30fps. The engine reads the hardware ONCE per rendered
// frame (IsWindowNOTActive -> Input_ProcessInput 0x467D10, between the hidden and visible UI
// ticks), while the battle UI's pad ring (Input_PadRing_AdvanceAndComputeEdges, once per UI
// tick) computes press edges against that last read - so at 30fps only 30 samples/s reached
// the UI: GF Boost mashing capped at ~15 presses/s (PSX: 60 Hz vsync pad reads, ~30/s),
// short taps merged/lost in Zell's Duel and the menus. Here a second hardware read is taken
// halfway through the frame wait: the next frame's first UI tick sees it, the second UI tick
// sees the regular read -> 2 evenly spaced samples per frame = 60 Hz, like the PlayStation.
// The engine's own auto-repeat (dword_1CD02F0 gate, field/menu repeat) is suspended during
// the extra read so it does not tick twice per frame; battle uses its per-tick ring repeat.
static uint32_t ff8_bgate_mid_reads = 0; // diagnostics

static void ff8_bgate_mid_frame_input()
{
	struct ff8_game_obj *game_object = (ff8_game_obj *)common_externals.get_game_object();
	if (game_object->hwnd != GetActiveWindow())
		return;
	uint32_t *autorepeat = (uint32_t *)0x1CD02F0;
	uint32_t saved = *autorepeat;
	*autorepeat = 0;
	((void *(*)())0x467D10)(); // Input_ProcessInput
	*autorepeat = saved;
	ff8_bgate_mid_reads++;
}

int ff8_limit_fps()
{
	static time_t last_gametime;
	time_t gametime;
	double framerate = 30.0f;

	struct ff8_game_obj *game_object = (ff8_game_obj *)common_externals.get_game_object();
	struct game_mode *mode = getmode_cached();

	// For cross music play (vanilla music only)
	qpc_get_time(&gametime);
	*ff8_externals.time_volume_change_related_1A78BE0 = (1000.0 / game_object->countspersecond) * qpc_diff_time(&gametime, &last_gametime, nullptr);

	if (mode->driver_mode != MODE_BATTLE)
		ff8_bgate_left_battle = true;

	if (ff8_fps_limiter < FPS_LIMITER_60FPS)
	{
		switch (mode->driver_mode)
		{
		case MODE_BATTLE:
			if (ff8_fps_limiter < FPS_LIMITER_30FPS) framerate = 15.0f;
			break;
		case MODE_CREDITS:
		case MODE_CARDGAME:
			framerate = 60.0f;
			break;
		}
	}
	else
	{
		switch (mode->driver_mode)
		{
		case MODE_FIELD:
		case MODE_WORLDMAP:
		case MODE_BATTLE:
		case MODE_SWIRL:
		case MODE_CREDITS:
			framerate = 60.0f;
			break;
		}
	}

	framerate *= gamehacks.getCurrentSpeedhack();
	double frame_time = game_object->countspersecond / framerate;

	bool mid_input = ff8_bgate_active && ff8_bgate_n == 2 && mode->driver_mode == MODE_BATTLE;
	do
	{
		qpc_get_time(&gametime);
		if (mid_input && gametime > last_gametime && qpc_diff_time(&gametime, &last_gametime, nullptr) >= frame_time / 2)
		{
			mid_input = false;
			ff8_bgate_mid_frame_input();
		}
	}
	while (gametime > last_gametime && qpc_diff_time(&gametime, &last_gametime, nullptr) < frame_time);

	last_gametime = gametime;

	return 0;
}

// ---------------------------------------------------------------------------
// High-frame-rate battle mode - active at ff8_fps_limiter >= FPS_LIMITER_30FPS
// ---------------------------------------------------------------------------
// Vanilla PC locks the whole battle module at 15fps and catches up by running the
// battle-UI tick pair (isBattle_HUDupdate 0x4A8E30 + isBattle_HUDdisplay 0x4A84E0)
// 4x per rendered frame (3 hidden + 1 visible = 60 ticks/s), with ONE DirectInput
// poll per frame -> 15 Hz input sampling (lost/merged presses in Boost, Zell Duel
// and menu navigation). See wiki: Battle UI Timing and Input Sampling.
//
// Raising the limiter runs the module loop faster (ff8_limit_fps: limiter 2 -> 30fps,
// limiter 3 -> 60fps), which is what fixes the menu/input rate - but it also makes
// every battle subsystem run proportionally too fast. This block gates them back to
// native speed.
//
// ONE knob drives all of it: ff8_bgate_n, the number of host frames per real
// (15fps-equivalent) battle tick. 30fps -> 2, 60fps -> 4. It is derived from the
// configured limiter at install time (ff8_bgate_install_hooks), and every rate-
// dependent quantity below is expressed as a function of it rather than hardcoded,
// so switching between 30 and 60 is a config change, not a code change:
//
//   - UI tick pair: vanilla runs 4/frame; native rate needs 4/n per frame, so keep
//     the last (4/n - 1) hidden catch-up ticks plus the visible one (n=2 -> 2 ticks
//     per frame; n=4 -> 1). This single gate keeps ATB, Zell Duel countdown, Boost
//     tick, cursor blink and text speed at native pace, while the pad ring advances
//     on ticks that actually saw a fresh input poll.
//   - fresh-input latch divisor (CONST_BattleUI_TicksPerFrame @0xB8A3E4): 4 -> 4/n,
//     so the ctx+33 "fresh input" cadence matches the host frame rate.
//   - game time (Savemap_TickGameTimeAndCountdown): called 4x per loop iteration
//     unconditionally -> gate 1-in-n while in battle to hold native 60 ticks/s.
//   - battle model animation + AnimSeq choreography VM: advance 1-in-n, geometry
//     still rebuilt every tick (no double-buffer flicker).
//   - battle camera: keyframe time step 16 -> 16/n per tick (imm8 @0x503A80).
//   - magic/GF/limit/Draw effects: the effect tree ticks only on real frames (native
//     pace, nothing rewound or run twice); its draws are read from the SSIGPU arena and
//     redrawn on held frames, extrapolated per vertex from the last two ticks (identity
//     pairing + median vertex welding) - smooth for every effect, no per-spell code.
//   - entity animations: engine-native SLOW half-step reading forced at animation start;
//     animations still read 1-in-n (real Slow, stage models) get extrapolated poses.
//   - hit-effect task queue: same record/replay as effects; camera shake held.
//   - AnimSeq-spawned tasks (84/9F/99/B1/96 skipped, AD/AE/81/A6 counters native).
//   - status timers + Gilgamesh/Angelo countdown + end fade: real frames only.
//   - all per-battle state is reset at the start of each battle.
//
// (All addresses are FF8 2000 US/EN 1.2 specific, same as the 60fps branch.)
//
// Host frames per real battle tick: 2 at 30fps, 4 at 60fps. Set in
// ff8_bgate_install_hooks() from ff8_fps_limiter; 2 is only a placeholder default.
// Must divide 4 (the vanilla UI ticks/frame) exactly, which both values do.
// (ff8_bgate_n is declared above the frame limiter)

// false = diagnostics-only mode (vanilla behavior, no gating) - used to capture reference
// traces at ff8_fps_limiter < 2 for comparing against the gated build
// (ff8_bgate_active is declared above the frame limiter)

static int ff8_bgate_phase = 0; // 0 = advance frame, otherwise held (bumped in BdLink hook)
static uint32_t ff8_bgate_frame_no = 0; // host battle frames (bumped in the BdLink hook)
// Held-frame draw mode, cycled with F9 during battle (test builds): 0 = plain replay of tick N
// (no interpolation), 1 = vertices extrapolated, 2 = vertices + vertex colors extrapolated,
// 3 = nothing drawn on held frames (diagnostic: shows what the engine itself puts on screen),
// 4 = replay everything EXCEPT 15-bit (direct colour) textured prims, 5 = replay ONLY those
// (bisection: a screen-sized 15bpp quad is how an effect re-draws a copy of the screen)
static int ff8_bgate_fx_mode = 2;
// F5 (test builds): bypass the EFFECT pacing entirely - the effect tree ticks every host
// frame like stock FFNx (2x speed) and nothing is replayed. Bisection aid: tells whether a
// visual artifact comes from pacing the effect at all, or from something else in the mod.
static bool ff8_bgate_fx_bypass = false;
// F6 (test builds): also re-issue on held frames the VRAM commands the effect queued during
// its real tick (texture streaming included). Eden scrolls its tunnel texture 2 rows per tick;
// with the effect paced, that upload happens every other frame only.
static bool ff8_bgate_vq_replay_all = false;
static bool ff8_bgate_fx_log_stats = false; // true = one FFNx.log line per held frame (verbose)
// Per-effect totals, logged once when the effect finishes (one line per spell/GF cast)
struct ff8_bgate_fx_sum_t
{
	uint32_t ticks, held, interp_held, prims, match, far_, nosig, unparsed, welded, orphans, maxcol, vram_xfer, badcmd, ambiguous;
	uint32_t tpages[8], tpage_hits[8]; int ntpages; // diagnostics: texture pages used by replayed prims
	uint32_t vq_types[4];                          // VRAM queue commands the effect queued, by type
};

// TEMPORARY run-lag diagnostics: per-frame trace of party slots + AnimSeq events.
// Remove once the end-of-run hitch is understood.
#define FF8_BGATE_DIAG 0
static uint32_t ff8_bgate_diag_frame = 0;
#define FF8_BGATE_SLOT_BASE 0x1D97494u
#define FF8_BGATE_SLOT_IDX(slot) ((int)(((uint32_t)(slot) - FF8_BGATE_SLOT_BASE) / 156))

#if FF8_BGATE_DIAG
// render probe: log every actual BS_RenderBattleEntity execution for party entities,
// with the render-list cursor before/after (to catch list overflow = dropped geometry).
// A frame with no R-line for an entity = the render was skipped upstream (caller
// visibility conditions / task ordering).
static char (__cdecl *ff8_bgate_diag_render_orig)(void *) = nullptr;
static uint32_t ff8_bgate_diag_render_ri = 0;

char __cdecl ff8_bgate_diag_render_hook(void *slot)
{
	uint32_t cur_before = *(uint32_t *)0x1D8E054; // battle_texture_data_ptr (list cursor)
	unreplace_function(ff8_bgate_diag_render_ri);
	char r = ff8_bgate_diag_render_orig(slot);
	rereplace_function(ff8_bgate_diag_render_ri);
	if ((uint32_t)slot < FF8_BGATE_SLOT_BASE) // party entities live below the monster array
	{
		uint8_t *s = (uint8_t *)slot;
		ffnx_info("D f=%u R e=%p list=+%X vis=%08X mpal=%08X spal=%08X com=%08X\n",
			ff8_bgate_diag_frame, slot,
			*(uint32_t *)0x1D8E054 - cur_before,
			*(uint32_t *)(s + 0x7C),  // some_flag_data: per-object visibility bitmask
			*(uint32_t *)(s + 0x28),  // model_palette (RGB + ABE bit; fades)
			*(uint32_t *)(s + 0x2C),  // shadow_palette (0x32000000 = shadow skipped)
			*(uint32_t *)(s + 0x64)); // anim_header.comFileData (model swap detection)
	}
	return r;
}
#endif

// --- battle frame phase: bumped once per battle logic frame (BdLink_GF 0x500900) ---
static int (__cdecl *ff8_bgate_bdlink_orig)() = nullptr;
static uint32_t ff8_bgate_bdlink_ri = 0;

static void ff8_bgate_battle_reset();
static void ff8_bgate_move_restore_all();
static void ff8_bgate_cam_restore();

// TEMP diagnostics (camera stuck after an effect): one line whenever the battle camera
// controller state changes. CURRENT_CAMERA_ANIMATION 0x1D97728, CAMERA_FLAG_RELATED
// 0x1D97704, cameraRelated_pointerAnimColl_flag/_2 0x1D97718/0x1D9771A,
// battle_to_update_flags 0x1D96A9C (bit 0x10 = magic effect owns the camera),
// C3_28_GF_data_pointer 0x1D96AAC (effect running).
static void ff8_bgate_camera_state_log()
{
	static uint32_t last[7] = {0xFFFFFFFF, 0, 0, 0, 0, 0, 0};
	static int32_t last_eye[3] = {0, 0, 0};
	uint32_t now[7] = {
		(uint32_t)*(uint16_t *)0x1D97728, (uint32_t)*(uint16_t *)0x1D97704,
		(uint32_t)*(uint8_t *)0x1D97718, (uint32_t)*(uint8_t *)0x1D9771A,
		*(uint32_t *)0x1D96A9C & 0x1F, (uint32_t)(*(uint32_t *)0x1D96AAC != 0),
		(uint32_t)*(uint16_t *)0x1D9771E }; // Battle_Camera_ReturnViewBlend
	int32_t eye[3] = { *(int16_t *)0xB8B7F0, *(int32_t *)0xB8B7F4, *(int16_t *)0xB8B7F2 };
	bool moved = abs(eye[0] - last_eye[0]) + abs(eye[1] - last_eye[1]) + abs(eye[2] - last_eye[2]) > 1500;
	if (memcmp(now, last, sizeof(now)) == 0 && !moved) return;
	memcpy(last, now, sizeof(now));
	memcpy(last_eye, eye, sizeof(eye));
	ffnx_info("30fps cam: f=%u ph=%d cur_anim=%04X cam_flag=%04X shot=%u coll2=%u upd_flags=%02X effect=%u retblend=%u eye=%d,%d,%d ret=%d,%d,%d%s\n",
		ff8_bgate_frame_no, ff8_bgate_phase, now[0], now[1], now[2], now[3], now[4], now[5], now[6],
		eye[0], eye[1], eye[2],
		(int)*(int16_t *)0xB8B800, (int)*(int32_t *)0xB8B804, (int)*(int16_t *)0xB8B802,
		moved ? " (moved)" : "");
}

// SCREEN FEEDBACK (Eden's / Ultima's... "ghost" of the whole screen, HUD included): an effect
// tick calls Battle_RequestScreenFeedback (0x47CF50, arg 0/1) which arms a one-frame request
// (0x1CFF6F4 = arg + 1). After the battle loop, battle_main_loop (0x47D1B3) consumes it: the
// screen rendered so far is captured into a texture and drawn back at 1/3 opacity (enlarged by
// 16px when the request is 2), then the request is cleared. Effects tick on real frames only, so
// on held frames nobody armed the request and the ghost vanished every other frame (the 15Hz
// "flash"). The request made during the last real frame is re-armed on the held frames.
static int ff8_bgate_feedback_req = 0;      // request armed during the current/last real frame
static bool ff8_bgate_feedback_rearm = true;
static bool ff8_bgate_gfc_f6 = true; // TEMP diagnostics: F6 = dedicated cinematic-GF redraw on/off
static uint32_t ff8_bgate_feedback_held = 0; // stats

int __cdecl ff8_bgate_feedback_request_hook(int mode)
{
	*(int *)0x1CFF6F4 = mode + 1;
	ff8_bgate_feedback_req = mode + 1;
	return mode + 1;
}

// TEMP diagnostics: F8 = save the next 60 presented frames as PNG (renderer.cpp burst capture)
extern int ffnx_cap_left, ffnx_cap_seq, ffnx_cap_div;
extern char ffnx_cap_label[96], ffnx_cap_dir[260];
static int ff8_bgate_cap_burst = 0;

int __cdecl ff8_bgate_bdlink_hook()
{
	if (ff8_bgate_left_battle)
	{
		ff8_bgate_left_battle = false;
		ff8_bgate_battle_reset();
	}
	ff8_bgate_phase = (ff8_bgate_phase + 1) % ff8_bgate_n;
	ff8_bgate_frame_no++;
	_snprintf_s(ffnx_cap_label, sizeof(ffnx_cap_label), _TRUNCATE, "f%u_%s", ff8_bgate_frame_no, ff8_bgate_phase == 0 ? "REAL" : "held");
	{
		static bool f8_down = false;
		// F8 = 60 full-resolution frames, F7 = 900 quarter-resolution frames (30 s, a whole summon)
		static bool f7_down = false;
		bool d7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
		bool long_burst = d7 && !f7_down;
		f7_down = d7;
		bool d8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
		if (((d8 && !f8_down) || long_burst) && ffnx_cap_left == 0)
		{
			_snprintf_s(ffnx_cap_dir, sizeof(ffnx_cap_dir), _TRUNCATE, "%s/capture30/burst%02d", basedir, ff8_bgate_cap_burst++);
			char cmd[300];
			_snprintf_s(cmd, sizeof(cmd), _TRUNCATE, "%s/capture30", basedir);
			CreateDirectoryA(cmd, NULL);
			CreateDirectoryA(ffnx_cap_dir, NULL);
			ffnx_cap_seq = 0;
			ffnx_cap_left = long_burst ? 900 : 60;
			ffnx_cap_div = long_burst ? 4 : 1;
			ffnx_info("capture: F7/F8 -> %d frames (1/%d res) to %s (n=%d bypass=%d mode=%d effect_id=%d)\n", ffnx_cap_left, ffnx_cap_div, ffnx_cap_dir, ff8_bgate_n, (int)ff8_bgate_fx_bypass, ff8_bgate_fx_mode, *(int *)0x1D99A68 + 1);
		}
		else if (long_burst && ffnx_cap_left > 1 && ffnx_cap_div > 1)
			ffnx_cap_left = 1; // F7 again = stop the long burst
		f8_down = d8;
		static bool f6_down = false;
		bool d6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
		if (d6 && !f6_down)
		{
			ff8_bgate_gfc_f6 = !ff8_bgate_gfc_f6;
			ffnx_info("30fps fx: F6 -> dedicated GF redraw on held frames %s\n", ff8_bgate_gfc_f6 ? "ON" : "OFF (generic 2D replay)");
		}
		f6_down = d6;
		static bool f5_down = false;
		bool d5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
		if (d5 && !f5_down)
		{
			ff8_bgate_fx_bypass = !ff8_bgate_fx_bypass;
			ffnx_info("30fps fx: F5 -> effect pacing %s\n", ff8_bgate_fx_bypass ? "BYPASSED (ticks every frame, 2x speed, no replay)" : "on (native pace + replay)");
		}
		f5_down = d5;
		static bool f9_down = false;
		bool down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
		if (down && !f9_down)
		{
			ff8_bgate_fx_mode = (ff8_bgate_fx_mode + 1) % 6;
			ffnx_info("30fps fx: F9 -> held-frame mode %d (%s)\n", ff8_bgate_fx_mode,
				ff8_bgate_fx_mode == 0 ? "plain replay / GF: camera only" : ff8_bgate_fx_mode == 1 ? "vertices / GF: + pure-op replay" :
				ff8_bgate_fx_mode == 2 ? "vertices + colors / GF: + midpoint state" : ff8_bgate_fx_mode == 3 ? "no replay (held frames draw nothing)" :
				ff8_bgate_fx_mode == 4 ? "replay without 15bpp prims" : "replay ONLY 15bpp prims");
		}
		f9_down = down;
	}
#if FF8_BGATE_DIAG
	ff8_bgate_diag_frame++;
#endif
	if (ff8_bgate_phase == 0)
	{
		ff8_bgate_move_restore_all();
		ff8_bgate_cam_restore();
		ff8_bgate_feedback_req = 0; // re-armed by the effect if it still wants it this tick
	}
	else if (ff8_bgate_feedback_req && !ff8_bgate_fx_bypass && ff8_bgate_feedback_rearm)
	{
		*(int *)0x1CFF6F4 = ff8_bgate_feedback_req;
		ff8_bgate_feedback_held++;
	}
	unreplace_function(ff8_bgate_bdlink_ri);
	int r = ff8_bgate_bdlink_orig();
	rereplace_function(ff8_bgate_bdlink_ri);
	ff8_bgate_camera_state_log();
	// NOTE: FADE_OUT_END_BATTLE_DURATION (0x1D27B0C) is decremented once per battleLoop
	// iteration (2x at 30fps), but compensating it alone desynced it from the VISUAL fade-out
	// (still host-rate): the screen went black, came back, then the battle unloaded. Left at
	// 2x until the visual fade driver is paced together with it.
#if FF8_BGATE_DIAG
	{
		// final camera position after updateBattleCamera ran (world XZ packed s16 pair + Y),
		// plus the task functions currently queued on the camera task queue
		int16_t *wxz = (int16_t *)0xB8B7F0;
		int32_t wy = *(int32_t *)0xB8B7F4;
		char tasks[128]; tasks[0] = 0;
		uint32_t node = *(uint32_t *)0x1D97768; // BD_LINK_TASK_HEADER_CAMERA.head
		int guard = 0;
		while (node > 0x10000 && node < 0x7F000000 && guard++ < 8)
		{
			char one[24];
			sprintf(one, " %08X", *(uint32_t *)(node + 8)); // TaskNodeHeader.task_func
			strcat(tasks, one);
			node = *(uint32_t *)(node + 4); // next
		}
		// C3_28_GF_data_pointer: non-zero while a magic/GF effect is running. Squall's cast
		// sequence waits on it (C3 28). If it never clears, the effect never completed.
		uint32_t c328 = *(uint32_t *)0x1D96AAC;
		uint32_t qhead = 0;
		if (c328 > 0x10000 && c328 < 0x7F000000) qhead = *(uint32_t *)c328; // queue head
		ffnx_info("D f=%u CAMPOS x=%d z=%d y=%d c328=%08X qhead=%08X tasks:%s\n",
			ff8_bgate_diag_frame, (int)wxz[0], (int)wxz[1], wy, c328, qhead, tasks);
	}
#endif
	return r;
}

// --- UI tick reduction: 4 ticks/frame -> 2 ticks/frame (= native 60 ticks/s) ---
// battle_cardgame_main_loop calls the pair 3x with menu rendering DISABLED
// (menu_rendering_enabled @0x1D6D4AC == 0, the hidden catch-up ticks) then 1x
// enabled (the visible tick). Skip hidden ticks #0 and #1, keep #2 + visible.
// The pair is called from the main loop only, always update-then-display, so the
// skip decision made in the update hook is consumed by the display hook.
#define FF8_BGATE_MENU_RENDERING_ENABLED (*(uint32_t *)0x1D6D4AC)

static int (__cdecl *ff8_bgate_hudupdate_orig)() = nullptr;
static int (__cdecl *ff8_bgate_huddisplay_orig)() = nullptr;
static uint32_t ff8_bgate_hudupdate_ri = 0, ff8_bgate_huddisplay_ri = 0;
static bool ff8_bgate_tick_skip = false;
static int ff8_bgate_hidden_idx = 0;

int __cdecl ff8_bgate_hudupdate_hook()
{
	if (FF8_BGATE_MENU_RENDERING_ENABLED == 0)
	{
		// Hidden catch-up tick. Native pace needs 4/n ticks per rendered frame, and the visible
		// tick already provides one, so keep the LAST (4/n - 1) of the 3 hidden ticks and drop
		// the rest (n=2 -> keep #2 only; n=4 -> keep none, the visible tick alone is enough).
		ff8_bgate_tick_skip = (ff8_bgate_hidden_idx++ < 3 - (4 / ff8_bgate_n - 1));
	}
	else
	{
		// visible tick: always runs, and resyncs the hidden counter
		ff8_bgate_hidden_idx = 0;
		ff8_bgate_tick_skip = false;
	}
	if (ff8_bgate_tick_skip)
		return 0;
	unreplace_function(ff8_bgate_hudupdate_ri);
	int r = ff8_bgate_hudupdate_orig();
	rereplace_function(ff8_bgate_hudupdate_ri);
	// HUD blink/pulse counter (BattleUI ctx+0x2C, ++ at 0x4A8E9B) advances only on the
	// fresh-input latch tick, i.e. once per rendered frame: 15/s vanilla, 30/s here - the
	// limit-break arrow, blinking command text, list page arrows, active character name /
	// ATB pulse and the countdown's red flash all ran 2x fast. Keep 1 advance in n.
	uint8_t *ctx = *(uint8_t **)0x1D6D490; // BattleUI_CtxPtr
	if (ctx && ctx[33]) // ctx+33 = the latch fired on this tick
	{
		static uint32_t latches = 0;
		if ((latches++ % ff8_bgate_n) != 0)
			ctx[0x2C]--;
	}
	return r;
}

int __cdecl ff8_bgate_huddisplay_hook()
{
	if (ff8_bgate_tick_skip)
		return 0;
	unreplace_function(ff8_bgate_huddisplay_ri);
	int r = ff8_bgate_huddisplay_orig();
	rereplace_function(ff8_bgate_huddisplay_ri);
	return r;
}

// --- GF Boost phases at native pacing ---
// computeGFBoost_ (0x56DD70, once per UI tick) counts its safe/danger phases and total window
// (word_209CEF6 / word_209CEF4, kernel gfBoostParams x15 units) down only on ticks where
// BattleUI ctx+33 ("fresh input frame") is set. That latch is armed once per BATTLE frame by
// BdLink - the same engine logic on the PSX, whose battle also ran ~15 frames/s - so the
// native pace is 15 units/s on both machines (only the pad SAMPLING differed: 60 Hz on PSX,
// restored here by the mid-frame read). At 30fps the latch fires 30/s, which made the phases
// and the whole Boost window 2x short: let only 1 latch in n reach the countdown.
// (A first attempt counted every UI tick = 60/s "to match the PSX" - 4x too fast: the
// "phases 4x longer than PSX" note it was based on was wrong.)
static void (__cdecl *ff8_bgate_boost_orig)() = nullptr;
static uint32_t ff8_bgate_boost_ri = 0;

// TEMP diagnostics: where do Boost's button-press edges come from? A press edge seen on the
// FIRST UI tick of a frame can only come from the mid-frame read (the regular read happens
// between the two ticks); edges on the second tick come from the regular read.
static struct { uint32_t first, second, calls, mid0; bool active; } ff8_bgate_boost_diag;

void __cdecl ff8_bgate_boost_hook()
{
	uint8_t *ctx = *(uint8_t **)0x1D6D490; // BattleUI_CtxPtr
	{
		auto &d = ff8_bgate_boost_diag;
		if (!d.active) { memset(&d, 0, sizeof(d)); d.active = true; d.mid0 = ff8_bgate_mid_reads; }
		d.calls++;
		int pressed = ((int(__cdecl *)(int))0x4A8420)(0); // read_pad_pressed_remapped
		if (pressed & 0xF0) // any face button edge (Boost uses Square = 0x80 after remap)
		{
			if (*(uint32_t *)0x1D6D4AC == 0) d.first++; // menu rendering off = hidden = 1st tick
			else d.second++;
		}
		if (*(uint8_t *)0x209CEFB == 6) // Boost state: finished
		{
			ffnx_info("30fps boost: face-button edges on 1st tick (mid-frame read)=%u, on 2nd tick (regular read)=%u | ui ticks=%u mid-frame reads=%u\n",
				d.first, d.second, d.calls, ff8_bgate_mid_reads - d.mid0);
			d.active = false;
		}
	}
	uint8_t saved = ctx ? ctx[33] : 0;
	if (ctx && ctx[33])
	{
		static uint32_t latches = 0;
		if ((latches++ % ff8_bgate_n) != 0)
			ctx[33] = 0; // this latch does not count for the phase timers
	}
	unreplace_function(ff8_bgate_boost_ri);
	ff8_bgate_boost_orig();
	rereplace_function(ff8_bgate_boost_ri);
	if (ctx) ctx[33] = saved;
}

// --- game time: called 4x per loop iteration unconditionally, so it scales with the host
// frame rate (120/s at 30fps, 240/s at 60fps) -> keep 1-in-n while in battle = native 60/s ---
static int (__cdecl *ff8_bgate_savemap_tick_orig)() = nullptr;
static uint32_t ff8_bgate_savemap_tick_ri = 0;

int __cdecl ff8_bgate_savemap_tick_hook()
{
	struct game_mode *mode = getmode_cached();
	if (mode->driver_mode == MODE_BATTLE)
	{
		static uint32_t n = 0;
		if ((n++ % ff8_bgate_n) != 0)
			return 0; // held: keep game time / battle countdown at native 60 ticks/s
	}
	unreplace_function(ff8_bgate_savemap_tick_ri);
	int r = ff8_bgate_savemap_tick_orig();
	rereplace_function(ff8_bgate_savemap_tick_ri);
	return r;
}

// --- battle model animation: engine-native SLOW interpolation (smooth models) ---
// Battle_ReadAnimation (0x508F90) consumes a bit-packed DELTA stream: frame 0 is the
// absolute base pose (bones zeroed at anim start), later frames are accumulative deltas -
// there is no closed form to re-sample, so a naive high-fps mod can only hold the pose.
// BUT the engine's Slow-status mechanism (BATTLE_ANIM_FLAG_SLOW, flags bit 0) is a complete
// half-step player: deltas halved, each frame consumed twice across 2 calls (stream offset
// only saved when the sub-frame counter, flags bits 0xC, is 0), and pre_Battle_ReadAnimation
// (0x509440) sets total_frames = 2*nbFrames-1 so completion stays exact. Repurposed at 2x
// call rate, forced SLOW = native speed + native duration + a genuine midpoint pose every
// host frame. Per-entity policy keeps the Slow/Haste statuses visually distinct:
//
//   status   vanilla (15 calls/s)   n=2 (30 calls/s)      n=4 (60 calls/s)
//   normal   plain    = 15 f/s      SLOW, no hold         SLOW, hold 1-in-2
//   Haste    FAST     = 30 f/s      plain, no hold        SLOW, no hold
//   Slow     SLOW     = 7.5 f/s     SLOW, hold 1-in-2     SLOW, hold 1-in-4
//
// Choreography stays exact: no AnimSeq opcode reads current_frame (audited - sequences wait
// on completion or B9 tick delays), and Renzokuken's AB stage buckets use the .dat frame
// counts, whose wall-clock spans are unchanged. See wiki: Battle Model Animation Timing.
#define FF8_BGATE_ANIM_FLAG_SLOW 0x01
#define FF8_BGATE_ANIM_FLAG_FAST 0x02

// Hold divisor per anim_cmd: readanim passes a call through only when phase % div == 0.
// Entries are (re)written at every entity animation start; anim_cmd addresses are stable
// (entity slot data / weapon headers). Unknown anim_cmds (stage models, effect models -
// they never start via Battle_QueueAnimation) default to div = n, the plain pose-hold.
static struct { void *cmd; int div; bool forced; } ff8_bgate_anim_policy[32];

static void ff8_bgate_anim_policy_set(void *cmd, int div, bool forced)
{
	int free_slot = -1;
	for (int i = 0; i < 32; i++)
	{
		if (ff8_bgate_anim_policy[i].cmd == cmd)
		{
			ff8_bgate_anim_policy[i].div = div;
			ff8_bgate_anim_policy[i].forced = forced;
			return;
		}
		if (!ff8_bgate_anim_policy[i].cmd && free_slot < 0) free_slot = i;
	}
	if (free_slot >= 0)
	{
		ff8_bgate_anim_policy[free_slot].cmd = cmd;
		ff8_bgate_anim_policy[free_slot].div = div;
		ff8_bgate_anim_policy[free_slot].forced = forced;
	}
}

static int ff8_bgate_anim_policy_get(void *cmd)
{
	for (int i = 0; i < 32; i++)
		if (ff8_bgate_anim_policy[i].cmd == cmd) return ff8_bgate_anim_policy[i].div;
	return ff8_bgate_n; // unknown anim_cmd: hold 1-in-n (previous behavior)
}

// true if WE set SLOW on this anim_cmd (vanilla would not have doubled its total_frames)
static bool ff8_bgate_anim_policy_forced(void *cmd)
{
	for (int i = 0; i < 32; i++)
		if (ff8_bgate_anim_policy[i].cmd == cmd) return ff8_bgate_anim_policy[i].forced;
	return false;
}

// --- AnimSeq C3 special vars 0x09/0x0A: report frame counts in NATIVE units ---
// Choreography sequences read the current animation's frame counts through
// AnimSeq_ReadSpecialVar_C3 (0x5044B0): var 0x0A = anim_cmd->total_frames, var 0x09 =
// anim_cmd->current_frame. A common construct is a counted wait-loop that holds a pose
// for exactly the animation's length: "C3 0A E5 7F" (counter = total frames) then
// "A0 xx ... C3 7F C5 FF E5 7F E7 xx" (decrement once per VM run, loop while > 0).
// The VM runs at native rate, but forced SLOW doubles total_frames (2T-1) - so those
// loops spun ~2x too long: +3 VM ticks of frozen pose at the end of every attack
// run-up and walk-back (measured against a vanilla 15fps reference trace; the
// diagnostic showed the interpreter parked on the E7 loop with delay 0). Rescale both
// vars back to native frame units when WE forced the SLOW flag; a real Slow status
// keeps the doubled values, exactly as vanilla reports them.
// CRITICAL - the signature must be int/int, NOT uint16_t: IDA types this function as
// returning WORD, but it actually returns SIGN-EXTENDED 32-bit values in EAX (movsx at
// 0x5044D0 / 0x5044ED / 0x50455E ...), and the interpreter consumes the full EAX
// (its c3Handler is int(*)(int)). A uint16_t-typed wrapper truncates and zero-extends,
// turning every negative variable into a huge positive one - e.g. Squall's settle
// sequence computes position.y = -1756 + (C3_02 + 1756) * frame / total with
// C3_02 = -1700, which became 63836 and flung the model off-screen for 3 of 4 frames
// (culled by RenderGeometry -> the model vanished). Sign extension is load-bearing here.
#define FF8_BGATE_ACTIVE_ANIM_CMD (*(uint8_t **)0x1D981F8)

static int (__cdecl *ff8_bgate_c3var_orig)(int) = nullptr;
static uint32_t ff8_bgate_c3var_ri = 0;

int __cdecl ff8_bgate_c3var_hook(int var)
{
	unreplace_function(ff8_bgate_c3var_ri);
	int r = ff8_bgate_c3var_orig(var);
	rereplace_function(ff8_bgate_c3var_ri);
	// 09/0A are zero-extended byte loads (xor eax,eax; mov al,[..]) - always positive,
	// so the rescale is safe on the full-width value.
	if ((var == 0x09 || var == 0x0A) && ff8_bgate_anim_policy_forced(FF8_BGATE_ACTIVE_ANIM_CMD))
		r = (r + 1) / 2; // total: 2T-1 -> T; current: call index -> data frame index
	return r;
}

// Battle_QueueAnimation (0x509520) is the ONLY entity/weapon animation starter (AnimSeq VM
// opcode < 0x80, opcode A0, idle restart) and the place where the engine applies the
// Slow/Haste status to the flags. Our policy must be applied to exactly the two
// pre_Battle_ReadAnimation calls it makes (weapon then entity) - hence this scope marker.
static bool ff8_bgate_inside_queue_anim = false;
static int (__cdecl *ff8_bgate_queueanim_orig)(void *, int) = nullptr;
static uint32_t ff8_bgate_queueanim_ri = 0;

int __cdecl ff8_bgate_queueanim_hook(void *entity_slot, int opcode)
{
#if FF8_BGATE_DIAG
	ffnx_info("D f=%u p=%d s=%d e=%p QUEUE anim=%02X\n",
		ff8_bgate_diag_frame, ff8_bgate_phase, FF8_BGATE_SLOT_IDX(entity_slot), entity_slot, opcode & 0xFFFF);
#endif
	ff8_bgate_inside_queue_anim = true;
	unreplace_function(ff8_bgate_queueanim_ri);
	int r = ff8_bgate_queueanim_orig(entity_slot, opcode);
	rereplace_function(ff8_bgate_queueanim_ri);
	ff8_bgate_inside_queue_anim = false;
	return r;
}

// pre_Battle_ReadAnimation resets the anim state and doubles total_frames if SLOW is set,
// so the policy has to be written into flags BEFORE the original body runs. When called
// from anywhere else (stage load, effect model animators) the flags are left untouched.
static int (__cdecl *ff8_bgate_preread_orig)(void *, void *, int) = nullptr;
static uint32_t ff8_bgate_preread_ri = 0;

// TRUE 30 FPS models (default): the animation logic stays exactly vanilla - every entity and
// weapon animation keeps the flags the engine gave it (Slow/Haste included) and is read once
// per logic tick, so frame counts, completions, sequence waits and the C3 frame variables are
// the vanilla ones. Held frames show the exact midpoint between the pose of this tick and the
// pose the next read will produce (ff8_bgate_pose_lookahead). 0 = the older forced-SLOW mode.
#define FF8_BGATE_ANIM_LOOKAHEAD 1

int __cdecl ff8_bgate_preread_hook(void *anim_header, void *anim_cmd, int animID)
{
	if (ff8_bgate_inside_queue_anim && FF8_BGATE_ANIM_LOOKAHEAD)
		ff8_bgate_anim_policy_set(anim_cmd, ff8_bgate_n, false);
	else if (ff8_bgate_inside_queue_anim)
	{
		uint8_t *flags = (uint8_t *)anim_cmd + 1;
		// speed multiplier x2: 2 = normal, 4 = Haste, 1 = Slow (the engine set at most one bit)
		int m2 = 2;
		if (*flags & FF8_BGATE_ANIM_FLAG_SLOW) m2 = 1;
		else if (*flags & FF8_BGATE_ANIM_FLAG_FAST) m2 = 4;
		// with SLOW the visible rate is 15n/(2*div) frames/s; we want 7.5*m2 -> div = n/m2
		int div = ff8_bgate_n / m2;
		if (div >= 1)
		{
			*flags = (*flags | FF8_BGATE_ANIM_FLAG_SLOW) & (uint8_t)~FF8_BGATE_ANIM_FLAG_FAST;
			// "forced" = vanilla would NOT have SLOW here (m2 > 1), so total_frames gets a
			// doubling the sequence scripts don't expect - the C3 var hook compensates.
			// Real Slow status (m2 == 1) is doubled in vanilla too: not forced.
			ff8_bgate_anim_policy_set(anim_cmd, div, m2 > 1);
		}
		else
		{
			// SLOW would undershoot even unheld (Haste at n=2): plain reading at 15n/div f/s
			*flags &= (uint8_t)~(FF8_BGATE_ANIM_FLAG_SLOW | FF8_BGATE_ANIM_FLAG_FAST);
			ff8_bgate_anim_policy_set(anim_cmd, (2 * ff8_bgate_n) / m2, false);
		}
	}
	unreplace_function(ff8_bgate_preread_ri);
	int r = ff8_bgate_preread_orig(anim_header, anim_cmd, animID);
	rereplace_function(ff8_bgate_preread_ri);
	return r;
}

static int (__cdecl *ff8_bgate_readanim_orig)(void *, void *) = nullptr;
static uint32_t ff8_bgate_readanim_ri = 0;

// Safety net: a fault while decoding an animation bitstream (stream read past its end)
// finishes that animation instead of taking the game down - the engine then does what it
// does at any normal completion (requeue/next animation). Logged with everything needed to
// find the cause. anim_cmd: +0 anim id, +1 flags, +6 current frame, +7 total frames.
static int ff8_bgate_readanim_guarded(void *header, void *anim_cmd)
{
	__try
	{
		return ff8_bgate_readanim_orig(header, anim_cmd);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		uint8_t *c = (uint8_t *)anim_cmd;
		ffnx_info("battle 30fps: animation read FAULTED (header=%p anim_cmd=%p anim=%u flags=%02X frame=%u/%u policy div=%d forced=%d) -> animation finished early\n",
			header, anim_cmd, (unsigned)c[0], (unsigned)c[1], (unsigned)c[6], (unsigned)c[7],
			ff8_bgate_anim_policy_get(anim_cmd), (int)ff8_bgate_anim_policy_forced(anim_cmd));
		c[6] = c[7]; // complete
		return 1;    // READ_ANIMATION_RETURN_ANIMATION_COMPLETE
	}
}

static inline int ff8_bgate_scale_round(int v, int num, int den);

// --- held-frame pose extrapolation for animations read 1-in-div (real Slow status,
// stage models, any anim_cmd with a hold divisor) ---
// A pose is the skeleton section of the model (ComFileSectionSkeleton, reached through
// BattleAnimHeader->comFileData->[0]): root position int16 x/y/z at +8/+10/+12, then
// nbBones (+0) bones of 48 bytes from +16 with rotX/Y/Z at +4/+6/+8 (4096 = full turn)
// and, when per_bone_scale_flag (+1) is set, scaleX/Y/Z at +10/+12/+14.
// ProcessFieldEntitiesTransformation (0x508C90) turns that pose into the bone matrices.
// After every real read the pose is recorded (prev/cur); a held frame writes
// cur + (cur - prev) * k/div into the skeleton (rotations the short way round), builds
// the matrices from it, then puts cur back - the stream reader works in deltas on top of
// the skeleton, so the next real read must find the exact real pose again.
#define FF8_BGATE_POSE_BONES 64
#define FF8_BGATE_POSE_SLOTS 64
struct ff8_bgate_pose_t
{
	int16_t root[3];
	int16_t v[FF8_BGATE_POSE_BONES][6]; // rot x/y/z, scale x/y/z
};
struct ff8_bgate_pose_hist_t
{
	void *cmd;
	uint8_t anim_id, frame; // anim id / current frame at the time of the cur capture
	uint32_t cur_at, prev_at; // host frame numbers of the captures
	bool prev_ok;
	int nb;
	bool scaled;
	ff8_bgate_pose_t prev, cur;
};
static ff8_bgate_pose_hist_t ff8_bgate_pose_hist[FF8_BGATE_POSE_SLOTS];

static uint8_t *ff8_bgate_skeleton(void *header)
{
	uint8_t *com = *(uint8_t **)((uint8_t *)header + 4); // BattleAnimHeader.comFileData
	return com ? *(uint8_t **)com : nullptr;            // -> comFileSkeletonSection1
}

static void ff8_bgate_pose_read(const uint8_t *sk, int nb, bool scaled, ff8_bgate_pose_t &p)
{
	memcpy(p.root, sk + 8, 6);
	for (int b = 0; b < nb; b++)
	{
		const uint8_t *bone = sk + 16 + 48 * b;
		memcpy(p.v[b], bone + 4, scaled ? 12 : 6);
	}
}

static void ff8_bgate_pose_write(uint8_t *sk, int nb, bool scaled, const ff8_bgate_pose_t &p)
{
	memcpy(sk + 8, p.root, 6);
	for (int b = 0; b < nb; b++)
	{
		uint8_t *bone = sk + 16 + 48 * b;
		memcpy(bone + 4, p.v[b], scaled ? 12 : 6);
	}
}

static ff8_bgate_pose_hist_t *ff8_bgate_pose_slot(void *cmd, bool create)
{
	ff8_bgate_pose_hist_t *free_slot = nullptr;
	for (int i = 0; i < FF8_BGATE_POSE_SLOTS; i++)
	{
		if (ff8_bgate_pose_hist[i].cmd == cmd) return &ff8_bgate_pose_hist[i];
		if (!ff8_bgate_pose_hist[i].cmd && !free_slot) free_slot = &ff8_bgate_pose_hist[i];
	}
	if (!create || !free_slot) return nullptr;
	memset(free_slot, 0, sizeof(*free_slot));
	free_slot->cmd = cmd;
	return free_slot;
}

// after a REAL read of a held (div > 1) animation
static void ff8_bgate_pose_capture(void *header, void *anim_cmd)
{
	uint8_t *sk = ff8_bgate_skeleton(header);
	if (!sk || sk[0] == 0 || sk[0] > FF8_BGATE_POSE_BONES) return;
	ff8_bgate_pose_hist_t *h = ff8_bgate_pose_slot(anim_cmd, true);
	if (!h) return;
	uint8_t id = ((uint8_t *)anim_cmd)[0], frame = ((uint8_t *)anim_cmd)[6];
	int nb = sk[0];
	bool scaled = (sk[1] & 1) != 0;
	// a new animation, a restart or a different model: nothing to extrapolate from yet
	bool continues = h->cur_at != 0 && h->anim_id == id && frame > h->frame && h->nb == nb && h->scaled == scaled;
	if (continues)
	{
		h->prev = h->cur;
		h->prev_at = h->cur_at;
	}
	h->prev_ok = continues;
	h->nb = nb;
	h->scaled = scaled;
	h->anim_id = id;
	h->frame = frame;
	h->cur_at = ff8_bgate_frame_no;
	ff8_bgate_pose_read(sk, nb, scaled, h->cur);
}

static inline int16_t ff8_bgate_extrap_angle(int16_t c, int16_t p, int k, int div)
{
	int d = ((c - p + 2048) & 4095) - 2048; // shortest way round
	return (int16_t)(c + ff8_bgate_scale_round(d, k, div));
}

// on a HELD frame (k = position inside the hold window, 1..div-1): build the bone
// matrices from the extrapolated pose, then restore the real one
static void ff8_bgate_pose_held(void *header, void *anim_cmd, int k, int div)
{
	uint8_t *sk = ff8_bgate_skeleton(header);
	ff8_bgate_pose_hist_t *h = sk ? ff8_bgate_pose_slot(anim_cmd, false) : nullptr;
	bool ok = h && h->prev_ok && h->nb == sk[0] && h->scaled == ((sk[1] & 1) != 0)
		&& h->anim_id == ((uint8_t *)anim_cmd)[0]
		&& ff8_bgate_frame_no - h->cur_at < (uint32_t)div        // cur is this window's read
		&& h->cur_at - h->prev_at == (uint32_t)div;             // prev is the one before it
	if (!ok)
	{
		((void(__cdecl *)(void *))0x508C90)(header); // no history: plain hold
		return;
	}
	static ff8_bgate_pose_t x;
	for (int a = 0; a < 3; a++)
		x.root[a] = (int16_t)(h->cur.root[a] + ff8_bgate_scale_round(h->cur.root[a] - h->prev.root[a], k, div));
	for (int b = 0; b < h->nb; b++)
	{
		for (int a = 0; a < 3; a++)
			x.v[b][a] = ff8_bgate_extrap_angle(h->cur.v[b][a], h->prev.v[b][a], k, div);
		if (h->scaled)
			for (int a = 3; a < 6; a++)
				x.v[b][a] = (int16_t)(h->cur.v[b][a] + ff8_bgate_scale_round(h->cur.v[b][a] - h->prev.v[b][a], k, div));
	}
	ff8_bgate_pose_write(sk, h->nb, h->scaled, x);
	((void(__cdecl *)(void *))0x508C90)(header); // ProcessFieldEntitiesTransformation
	ff8_bgate_pose_write(sk, h->nb, h->scaled, h->cur);
}

// Held frame of an animation read once per logic tick: run the engine's own reader one frame
// ahead on the real state, keep the pose it produces, put the animation command and the
// whole skeleton section (pose + bone matrices) back, then build the bone matrices from the
// exact midpoint pose (angles the short way round) and restore the real pose values. The
// reader is a pure function of the animation command and the skeleton (its bit cursor lives
// in a scratch block rebuilt from the command on every call), so the next real read finds
// exactly the vanilla state. A completed animation has no next frame in its own stream (the
// sequence script decides what plays next): it holds its last pose for that half tick, as
// vanilla shows it until the next tick.
static uint8_t ff8_bgate_la_skel_save[16 + 48 * FF8_BGATE_POSE_BONES];

static void ff8_bgate_pose_lookahead(void *header, void *anim_cmd, int k, int div)
{
	uint8_t *sk = ff8_bgate_skeleton(header);
	uint8_t *cmd = (uint8_t *)anim_cmd;
	if (!sk || sk[0] == 0 || sk[0] > FF8_BGATE_POSE_BONES || cmd[6] >= cmd[7])
	{
		((void(__cdecl *)(void *))0x508C90)(header); // plain hold
		return;
	}
	int nb = sk[0];
	bool scaled = (sk[1] & 1) != 0;
	uint32_t size = 16 + 48 * nb;
	uint8_t cmd_save[8];
	memcpy(cmd_save, cmd, 8);
	memcpy(ff8_bgate_la_skel_save, sk, size);
	static ff8_bgate_pose_t cur, next, mid;
	ff8_bgate_pose_read(sk, nb, scaled, cur);
	unreplace_function(ff8_bgate_readanim_ri);
	ff8_bgate_readanim_guarded(header, anim_cmd);
	rereplace_function(ff8_bgate_readanim_ri);
	ff8_bgate_pose_read(sk, nb, scaled, next);
	memcpy(sk, ff8_bgate_la_skel_save, size);
	memcpy(cmd, cmd_save, 8);
	for (int a = 0; a < 3; a++)
		mid.root[a] = (int16_t)(cur.root[a] + ff8_bgate_scale_round(next.root[a] - cur.root[a], k, div));
	for (int b = 0; b < nb; b++)
	{
		for (int a = 0; a < 3; a++)
		{
			int d = ((next.v[b][a] - cur.v[b][a] + 2048) & 4095) - 2048; // shortest way round
			mid.v[b][a] = (int16_t)(cur.v[b][a] + ff8_bgate_scale_round(d, k, div));
		}
		if (scaled)
			for (int a = 3; a < 6; a++)
				mid.v[b][a] = (int16_t)(cur.v[b][a] + ff8_bgate_scale_round(next.v[b][a] - cur.v[b][a], k, div));
	}
	ff8_bgate_pose_write(sk, nb, scaled, mid);
	((void(__cdecl *)(void *))0x508C90)(header); // BattleModel_BuildBoneMatricesFromPose
	ff8_bgate_pose_write(sk, nb, scaled, cur);
}

namespace ff8fx
{
	void pose_midpoint(void *anim_header, void *anim_cmd, int num, int den)
	{
		ff8_bgate_pose_lookahead(anim_header, anim_cmd, num, den);
	}
}

int __cdecl ff8_bgate_readanim_hook(void *header, void *anim_cmd)
{
	// COMPLETED animation: the original early-outs (return 1) WITHOUT rebuilding the
	// double-buffered geometry. Vanilla never shows that - its VM runs every tick and
	// requeues the loop in the same call - but with the VM gated to real frames a
	// completion surfacing on a held frame leaves the entity's geometry stale for one
	// frame -> 1-frame flicker once per animation loop (SLOW loops are 2T-1 calls, odd,
	// so the completion parity keeps landing on held frames). Rebuild before returning.
	// (Effect-model reads run only inside the real-frame effect tick now, where phase
	// is 0 - both branches below pass them straight through, so no special-casing.)
	if (*((uint8_t *)anim_cmd + 6) >= *((uint8_t *)anim_cmd + 7))
	{
		((void(__cdecl *)(void *))0x508C90)(header); // ProcessFieldEntitiesTransformation
		return 1; // READ_ANIMATION_RETURN_ANIMATION_COMPLETE (same as the early-out)
	}
	int div = ff8_bgate_anim_policy_get(anim_cmd);
	// frame 0 is the absolute base pose read right after the bones were zeroed - never
	// hold it, or the model shows a T-pose for a frame.
	if (div > 1 && *((uint8_t *)anim_cmd + 6) != 0 && (ff8_bgate_phase % div) != 0)
	{
		// held: show the in-between pose instead of repeating the last one
		if (FF8_BGATE_ANIM_LOOKAHEAD) ff8_bgate_pose_lookahead(header, anim_cmd, ff8_bgate_phase % div, div);
		else ff8_bgate_pose_held(header, anim_cmd, ff8_bgate_phase % div, div);
		return 0; // "frame processed, not complete" -> animation continues
	}
	unreplace_function(ff8_bgate_readanim_ri);
	int r = ff8_bgate_readanim_guarded(header, anim_cmd);
	rereplace_function(ff8_bgate_readanim_ri);
	if (div > 1 && !FF8_BGATE_ANIM_LOOKAHEAD)
		ff8_bgate_pose_capture(header, anim_cmd);
	return r;
}

// AnimSeq_UpdateEntityPerFrame (0x504290) advances the CHOREOGRAPHY (run-up
// movement, animation transitions, frame delays, sound triggers) and the skeletal
// frame. On held ticks skip the VM but still call
// AdvanceAnimationBy1AndCheckCompletion (0x5094F0) so the leaf gate above fires
// its geometry rebuild. The sole caller ignores the return value.
//
// Loop-boundary stall: under SLOW an animation consumes its stream in 2T-1 calls -
// an ODD number - and entity animations start on real frames (the VM only runs
// there), so the COMPLETE return always surfaces on a HELD frame. Vanilla handles a
// completion in the same tick it appears (requeue the looping run/idle cycle, bake
// transforms, continue the sequence); with the VM gated, the entity instead froze
// for one host frame at every animation loop - a visible hitch each run cycle.
// Fix: when a completion surfaces on a held frame, run the VM once for this entity
// right now - exactly what vanilla would do - EDGE-TRIGGERED per completion, so a
// sequence that stays in a completed-animation wait state (B9 delays etc.) still
// ticks its VM only on real frames and its timing is untouched.
struct ff8_bgate_done_cache_t { void *slot; bool was_complete; };
static ff8_bgate_done_cache_t ff8_bgate_done_cache[16];

static ff8_bgate_done_cache_t *ff8_bgate_done_cache_get(void *slot)
{
	int free_slot = -1;
	for (int i = 0; i < 16; i++)
	{
		if (ff8_bgate_done_cache[i].slot == slot) return &ff8_bgate_done_cache[i];
		if (!ff8_bgate_done_cache[i].slot && free_slot < 0) free_slot = i;
	}
	if (free_slot < 0) return nullptr;
	ff8_bgate_done_cache[free_slot].slot = slot;
	ff8_bgate_done_cache[free_slot].was_complete = false;
	return &ff8_bgate_done_cache[free_slot];
}

// FF8BattleEntitySlotData.anim_cmd is at +0x6C; current_frame/total_frames at +6/+7
#define FF8_BGATE_ANIM_COMPLETE(slot) \
	(*((uint8_t *)(slot) + 0x6C + 6) >= *((uint8_t *)(slot) + 0x6C + 7))

static int (__cdecl *ff8_bgate_animseq_upd_orig)(void *) = nullptr;
static uint32_t ff8_bgate_animseq_upd_ri = 0;

// --- held-frame extrapolation of SCRIPT-DRIVEN entity movement ---
// Choreography scripts that move an entity themselves (flying approaches, jumps, lunges:
// E5 0D/0E/0F write the state position offset from frame-count formulas) change it only
// when the VM runs - on real frames - so the body moved in 15 steps/s next to 30fps limbs
// ("teleporting" Bite Bugs). On held frames the rendered position is nudged forward by
// half the last real step, and the true value is put back at the start of the next real
// frame, before any logic reads it.
// Two lessons from the 16/07 attempt (which extrapolated based_position blindly):
//  - root motion is folded into based_position once per animation loop - a one-off spike
//    that is NOT movement: only STEADY motion is extrapolated (the last two real steps must
//    agree), so a lone handover spike never qualifies;
//  - some movement already runs every host frame (9E run-up task): if the position changed
//    since the last real update, something else is animating it smoothly - hands off.
// Positions handled: FF8BattleEntitySlotData.based_position (+0x1C, int16 z/y/x) and the
// state controller's position offset (*(slot+0x74) + 0x24, int16 x3).
#define FF8_BGATE_MOVE_SLOTS 16
struct ff8_bgate_move_t
{
	void *slot;
	bool have, have_d;
	int16_t last[6];     // positions at the end of the last real update
	int16_t d1[6], d2[6]; // last two real steps
	bool nudged;
	int16_t written[6];  // values we wrote on the held frame (validity check on restore)
};
static ff8_bgate_move_t ff8_bgate_move[FF8_BGATE_MOVE_SLOTS];

static int16_t *ff8_bgate_move_pos(void *slot, int i)
{
	uint8_t *s = (uint8_t *)slot;
	if (i < 3)
		return (int16_t *)(s + 0x1C) + i;
	uint8_t *st = *(uint8_t **)(s + 0x74);
	return st ? (int16_t *)(st + 0x24) + (i - 3) : nullptr;
}

static ff8_bgate_move_t *ff8_bgate_move_get(void *slot)
{
	ff8_bgate_move_t *free_slot = nullptr;
	for (int i = 0; i < FF8_BGATE_MOVE_SLOTS; i++)
	{
		if (ff8_bgate_move[i].slot == slot) return &ff8_bgate_move[i];
		if (!ff8_bgate_move[i].slot && !free_slot) free_slot = &ff8_bgate_move[i];
	}
	if (!free_slot) return nullptr;
	memset(free_slot, 0, sizeof(*free_slot));
	free_slot->slot = slot;
	return free_slot;
}

// start of a real frame (BdLink hook, before any battle logic): put the true positions back
static void ff8_bgate_move_restore_all()
{
	for (int m = 0; m < FF8_BGATE_MOVE_SLOTS; m++)
	{
		ff8_bgate_move_t &mv = ff8_bgate_move[m];
		if (!mv.slot || !mv.nudged) continue;
		mv.nudged = false;
		for (int i = 0; i < 6; i++)
		{
			int16_t *p = ff8_bgate_move_pos(mv.slot, i);
			if (p && *p == mv.written[i]) // untouched since we wrote it: restore
				*p = mv.last[i];
		}
	}
}

// end of a real update: record this native step
static void ff8_bgate_move_record(void *slot)
{
	ff8_bgate_move_t *mv = ff8_bgate_move_get(slot);
	if (!mv) return;
	for (int i = 0; i < 6; i++)
	{
		int16_t *p = ff8_bgate_move_pos(slot, i);
		int16_t v = p ? *p : 0;
		if (mv->have)
		{
			mv->d2[i] = mv->d1[i];
			mv->d1[i] = (int16_t)(v - mv->last[i]);
		}
		mv->last[i] = v;
	}
	mv->have_d = mv->have;
	mv->have = true;
}

// held frame: nudge steady movement half a step forward
static void ff8_bgate_move_nudge(void *slot)
{
	ff8_bgate_move_t *mv = ff8_bgate_move_get(slot);
	if (!mv || !mv->have_d || mv->nudged) return;
	for (int i = 0; i < 6; i++)
	{
		int16_t *p = ff8_bgate_move_pos(slot, i);
		if (p && *p != mv->last[i])
			return; // moved since the real update (9E task, teleport...): not ours to smooth
	}
	bool any = false;
	for (int i = 0; i < 6; i++)
	{
		int16_t *p = ff8_bgate_move_pos(slot, i);
		int d1 = mv->d1[i], d2 = mv->d2[i];
		int tol = abs(d1) / 2; if (tol < 4) tol = 4;
		bool steady = d1 != 0 && abs(d1) < 1024 && abs(d1 - d2) <= tol;
		mv->written[i] = p ? *p : 0;
		if (p && steady)
		{
			*p = (int16_t)(mv->last[i] + ff8_bgate_scale_round(d1, ff8_bgate_phase, ff8_bgate_n));
			mv->written[i] = *p;
			any = true;
		}
	}
	mv->nudged = any;
}

int __cdecl ff8_bgate_animseq_upd_hook(void *slot_data_struct)
{
	ff8_bgate_done_cache_t *dc = ff8_bgate_done_cache_get(slot_data_struct);

#if FF8_BGATE_DIAG
	// diagnostics-only mode: log and pass straight through (pure vanilla flow)
	if (!ff8_bgate_active)
	{
		uint8_t *sv = (uint8_t *)slot_data_struct;
		uint8_t *stv = (uint8_t *)*(uint32_t *)(sv + 0x74);
		ffnx_info("V f=%u e=%p com=%u anim=%02X cf=%u/%u pos=%d,%d,%d\n",
			ff8_bgate_diag_frame, slot_data_struct, sv[4], sv[0x6C], sv[0x72], sv[0x73],
			(int)*(int16_t *)(stv + 0x24), (int)*(int16_t *)(stv + 0x26),
			(int)*(int16_t *)(stv + 0x28));
		unreplace_function(ff8_bgate_animseq_upd_ri);
		int rv = ff8_bgate_animseq_upd_orig(slot_data_struct);
		rereplace_function(ff8_bgate_animseq_upd_ri);
		return rv;
	}
#endif
#if FF8_BGATE_DIAG
	{
		int idx = FF8_BGATE_SLOT_IDX(slot_data_struct);
		{ // log every entity - party slots turned out to live outside the 0x1D97494 array
			uint8_t *s = (uint8_t *)slot_data_struct;
			int rx = 0, rz = 0, b0 = 0;
			uint32_t comdata = *(uint32_t *)(s + 0x64); // anim_header.comFileData
			if (comdata > 0x10000 && comdata < 0x7F000000)
			{
				uint32_t skel = *(uint32_t *)comdata; // comFileSkeletonSection1
				if (skel > 0x10000 && skel < 0x7F000000)
				{
					rx = *(int16_t *)(skel + 8);   // root_pos_x
					rz = *(int16_t *)(skel + 12);  // root_pos_z
					b0 = *(int16_t *)(skel + 0x14); // bone0 rotX (pose probe)
				}
			}
			uint8_t *st = (uint8_t *)*(uint32_t *)(s + 0x74); // battle_state_controler_pointer
			ffnx_info("D f=%u p=%d e=%p com=%u anim=%02X cf=%u/%u root=%d,%d seq=%u pc=%u dly=%u flg=%04X pos=%d,%d,%d\n",
				ff8_bgate_diag_frame, ff8_bgate_phase, slot_data_struct, s[4],
				s[0x6C], s[0x72], s[0x73], rx, rz,
				st[0], (unsigned)*(uint16_t *)(st + 0x2E), st[1],
				(unsigned)*(uint16_t *)(st + 0x2C),
				(int)*(int16_t *)(st + 0x24), (int)*(int16_t *)(st + 0x26),
				(int)*(int16_t *)(st + 0x28)); // state->position (E5 0D/0E/0F offsets)
		}
	}
#endif

	if (ff8_bgate_phase != 0)
	{
		int complete = ((int(__cdecl *)(void *))0x5094F0)(slot_data_struct);
		// (forced-SLOW mode only: with vanilla-rate reads a completion only appears on a logic tick,
		// and vanilla handles it on the next one)
		if (complete && dc && !dc->was_complete && !FF8_BGATE_ANIM_LOOKAHEAD)
		{
			// fresh completion on a held frame: handle it now instead of stalling a frame
			dc->was_complete = true;
			unreplace_function(ff8_bgate_animseq_upd_ri);
			ff8_bgate_animseq_upd_orig(slot_data_struct);
			rereplace_function(ff8_bgate_animseq_upd_ri);
			// if the VM requeued a new animation, the state below re-arms automatically
			dc->was_complete = FF8_BGATE_ANIM_COMPLETE(slot_data_struct);
		}
		else if (dc)
		{
			dc->was_complete = (complete != 0);
		}
		ff8_bgate_move_nudge(slot_data_struct);
		return 0;
	}

	unreplace_function(ff8_bgate_animseq_upd_ri);
	int r = ff8_bgate_animseq_upd_orig(slot_data_struct);
	rereplace_function(ff8_bgate_animseq_upd_ri);
	if (dc)
		dc->was_complete = FF8_BGATE_ANIM_COMPLETE(slot_data_struct);
	ff8_bgate_move_record(slot_data_struct);
	return r;
}

// --- run-up movement task (AnimSeq opcode 9E): native duration, host-rate smooth ---
// moveEntityToTargetSmoothly (0x50F750) is a time-parameterized lerp: one counter++ per
// task tick, position = origin + (counter/movement_param) * (target - origin), done when
// counter reaches movement_param. AnimSeq task queues tick every HOST frame, so at n x
// call rate the lerp finished in 1/n of the intended wall time: the entity arrived early
// and stood waiting for the (correctly native-paced) choreography - one visible hitch at
// the end of each run segment, both toward the target and back. Scaling movement_param
// by n at task creation restores the intended duration with n x finer steps; the curve
// is a pure ratio (counter << 12) / movement_param, so it passes through exactly the
// same positions the native pacing would, plus genuine in-between ones every host frame.
static void *(__cdecl *ff8_bgate_anim9e_orig)(void *, void *, int) = nullptr;
static uint32_t ff8_bgate_anim9e_ri = 0;

void *__cdecl ff8_bgate_anim9e_hook(void *active_entity, void *param_entity, int movement_param)
{
	unreplace_function(ff8_bgate_anim9e_ri);
	void *task = ff8_bgate_anim9e_orig(active_entity, param_entity, movement_param);
	rereplace_function(ff8_bgate_anim9e_ri);
	if (task)
	{
		uint16_t *param = (uint16_t *)((uint8_t *)task + 0x14); // TaskNodeAnimSeq_9E.movement_param
#if FF8_BGATE_DIAG
		ffnx_info("D f=%u p=%d 9E s=%d->s=%d param=%u (scaled to %u)\n",
			ff8_bgate_diag_frame, ff8_bgate_phase, FF8_BGATE_SLOT_IDX(active_entity),
			FF8_BGATE_SLOT_IDX(param_entity), (unsigned)*param, (unsigned)(*param * ff8_bgate_n));
#endif
		if (*param > 0)
			*param = (uint16_t)(*param * ff8_bgate_n);
	}
	return task;
}

// --- battle camera: gate the keyframe player to real frames (see install-site comment
// for why the step-halving patch this replaced caused a hard engine hang) ---
static int (__cdecl *ff8_bgate_camanim_orig)(void *) = nullptr;
static uint32_t ff8_bgate_camanim_ri = 0;

int __cdecl ff8_bgate_camanim_hook(void *task)
{
	if (ff8_bgate_phase != 0)
		return 0; // held frame: camera_struct stays put, no advance (see updateBattleCamera hook below for the render-facing interpolation this enables safely)
	unreplace_function(ff8_bgate_camanim_ri);
	int r = ff8_bgate_camanim_orig(task);
	rereplace_function(ff8_bgate_camanim_ri);
	return r;
}

// --- battle camera: extrapolate the RENDER-FACING position on held frames ---
// The keyframe player's internal catch-up loop is where the hang lived (see above) -
// its internal state (camera_struct: currentTime, keyframe pointers, segment tables)
// is never touched on held frames. But the four globals updateBattleCamera (0x504060)
// writes every call - Battle_Camera_world_XZ_s16/Y and Battle_Camera_LookAt_XZ_s16/Y,
// the actual values the renderer reads for this frame's view - are a completely
// separate, simple output: just four numbers. This hook lets the original function run
// untouched (proven safe, produces the correct real-frame values, including the task
// no-op on held frames), then on held frames OVERWRITES those four globals with a
// linear extrapolation from the last two REAL frames' outputs:
//   extrapolated = prev1 + (prev1 - prev2) * phase / n
// prev1/prev2 are consecutive REAL camera outputs, so their delta is exactly one real
// tick's worth of camera movement regardless of n; scaling by phase/n places each held
// frame at its correct fractional position between them - same closed-form technique
// used for the Cure ring, just applied to the camera's already-computed output instead
// of re-deriving position from keyframe data. Nothing here can hang: it is pure
// arithmetic on saved snapshots, no loops, no engine state mutated except the two
// render-facing globals themselves.
//
// A large delta between the last two real frames (camera cut / stage transition) is
// clamped rather than extrapolated - overshooting a cut would fling the view somewhere
// absurd for one frame; holding the pre-cut position for that one frame is the safe
// fallback and self-corrects next real tick.
#define FF8_BGATE_CAM_JUMP_CLAMP 4096

struct ff8_bgate_cam_snap_t { int32_t wxz, wy, lxz, ly; bool valid; };
static ff8_bgate_cam_snap_t ff8_bgate_cam_prev1 = {0, 0, 0, 0, false};
static ff8_bgate_cam_snap_t ff8_bgate_cam_prev2 = {0, 0, 0, 0, false};

static int32_t ff8_bgate_extrap_s16pair(int32_t prev2, int32_t prev1, int num, int den)
{
	int16_t p2x = (int16_t)(prev2 & 0xFFFF), p2z = (int16_t)(prev2 >> 16);
	int16_t p1x = (int16_t)(prev1 & 0xFFFF), p1z = (int16_t)(prev1 >> 16);
	if (abs(p1x - p2x) > FF8_BGATE_CAM_JUMP_CLAMP || abs(p1z - p2z) > FF8_BGATE_CAM_JUMP_CLAMP)
		return prev1;
	int16_t rx = (int16_t)(p1x + ((p1x - p2x) * num) / den);
	int16_t rz = (int16_t)(p1z + ((p1z - p2z) * num) / den);
	return (uint16_t)rx | ((uint32_t)(uint16_t)rz << 16);
}

static int32_t ff8_bgate_extrap_i32(int32_t prev2, int32_t prev1, int num, int den)
{
	if (abs(prev1 - prev2) > FF8_BGATE_CAM_JUMP_CLAMP)
		return prev1;
	return prev1 + ((prev1 - prev2) * num) / den;
}

static int (__cdecl *ff8_bgate_updatecam_orig)() = nullptr;
static uint32_t ff8_bgate_updatecam_ri = 0;

// --- battle camera script VM at native rate ---
// updateBattleCamera runs every host frame and steps the camera script VM
// BS_UpdateCameraSequence (0x509610; section-6 camera scripts: which shot plays, when it
// ends) on each call -> 2x fast. That script ending too early broke the camera return
// after cinematic enemy abilities (Bite Bug effect 234): Magic_EffectEnd_ResetCamera...
// (0x50AED0) only arms the return to the saved view (CAMERA_WOBBLING_FACTOR = 4096 ->
// dword_B8B800 view restored) while the script's anim collection is still active
// (cameraRelated_pointerAnimColl_flag); it had already finished, so the camera stayed
// wherever the effect left it ("locked on the target"). Real frames only; the VM returns
// its new setting pointer, so held frames return the current one unchanged.
static void *(__cdecl *ff8_bgate_camseq_orig)() = nullptr;
static uint32_t ff8_bgate_camseq_ri = 0;

void *__cdecl ff8_bgate_camseq_hook()
{
	if (ff8_bgate_phase != 0)
		return *(void **)0x1D99A34; // CURRENT_CAMERA_SETTING_ADDR
	unreplace_function(ff8_bgate_camseq_ri);
	void *r = ff8_bgate_camseq_orig();
	rereplace_function(ff8_bgate_camseq_ri);
	return r;
}

// Held-frame extrapolation must never OVERRIDE a camera the engine set on purpose. The
// return-to-view after an effect is a ONE-SHOT snap (Magic_EffectEnd_ResetCameraAndEntityFlags
// arms Battle_Camera_ReturnViewBlend = 4096, updateBattleCamera copies the return view once);
// when it landed on a held frame, the extrapolation from the effect's last shots replaced it,
// and since the engine never rewrites a settled camera the view stayed "locked on the target"
// (Bite Bug effect 234, victory fanfare). So: if the original call changed the camera on a
// held frame, keep its value and restart the history there. And the extrapolated value is
// display-only: the true value is restored at the start of the next real frame (BdLink hook),
// so it can never linger in the camera globals.
static int32_t ff8_bgate_cam_true[4], ff8_bgate_cam_written[4];
static bool ff8_bgate_cam_nudged = false;

// field of view (word_1D8E038) and roll (g_BattleCam_Roll 0x1D977A2) are written by the
// keyframe player itself: their held-frame midpoints are restored the same way
static int16_t ff8_bgate_cam_fovroll_true[2], ff8_bgate_cam_fovroll_written[2];
static bool ff8_bgate_cam_fovroll_nudged = false;

static void ff8_bgate_cam_restore()
{
	if (ff8_bgate_cam_fovroll_nudged)
	{
		ff8_bgate_cam_fovroll_nudged = false;
		int16_t *fr[2] = { (int16_t *)0x1D8E038, (int16_t *)0x1D977A2 };
		for (int i = 0; i < 2; i++)
			if (*fr[i] == ff8_bgate_cam_fovroll_written[i]) *fr[i] = ff8_bgate_cam_fovroll_true[i];
	}
	if (!ff8_bgate_cam_nudged) return;
	ff8_bgate_cam_nudged = false;
	int32_t *g[4] = { (int32_t *)0xB8B7F0, (int32_t *)0xB8B7F4, (int32_t *)0xB8B7F8, (int32_t *)0xB8B7FC };
	for (int i = 0; i < 4; i++)
		if (*g[i] == ff8_bgate_cam_written[i]) // untouched since we wrote it
			*g[i] = ff8_bgate_cam_true[i];
}

// TRUE 30 FPS camera shots: the keyframe player (ProcessCameraAnimation 0x5035E0) is run one
// tick ahead with the vanilla time step on a saved copy of its whole state - the camera struct
// (BattleStageCameraMainData, 1316 bytes, task node +0x0C), field of view, roll and the
// shot mask - and everything is put back; the held frame then shows the exact midpoint of the
// camera (eye, target, fov, roll) between this tick and the next. A shot that ends on the next
// tick has no next camera of its own (the camera script picks what follows): that half tick
// holds. Cameras written directly by effects keep the extrapolation below.
#define FF8_BGATE_CAM_LOOKAHEAD 1
struct ff8_bgate_cam_next_t { bool valid; uint8_t *cs; int16_t v[8]; int16_t fov, roll; };

static void ff8_bgate_cam_lookahead(ff8_bgate_cam_next_t &nx)
{
	nx.valid = false;
	uint8_t *node = *(uint8_t **)0x1D97768; // BD_LINK_TASK_HEADER_CAMERA.head
	for (; node; node = *(uint8_t **)(node + 4))
		if (*(uint32_t *)(node + 8) == 0x5035E0) break;
	if (!node) return;
	uint8_t *cs = *(uint8_t **)(node + 0x0C);
	if (!cs || cs != *(uint8_t **)0x1D97798) return; // the struct the output reads (cameraStructPointer)
	static uint8_t cs_save[1316];
	memcpy(cs_save, cs, sizeof(cs_save));
	int16_t fov = *(int16_t *)0x1D8E038, roll = *(int16_t *)0x1D977A2;
	uint16_t shots = *(uint16_t *)0x1D97718;
	uint32_t pool = *(uint32_t *)0x1D999C4;
	// a segment switch can capture the return view (BS_Camera_CaptureReturnView 0x503300):
	// Battle_Camera_ReturnView_* (0xB8B800, 16 bytes), its fov (0x1D977A0) and roll (0x1D9771C)
	uint8_t ret_view[16];
	memcpy(ret_view, (void *)0xB8B800, sizeof(ret_view));
	int16_t ret_fov = *(int16_t *)0x1D977A0, ret_roll = *(int16_t *)0x1D9771C;
	uint32_t r = 2;
	__try
	{
		unreplace_function(ff8_bgate_camanim_ri);
		r = (uint32_t)ff8_bgate_camanim_orig(node);
		rereplace_function(ff8_bgate_camanim_ri);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		rereplace_function(ff8_bgate_camanim_ri);
		r = 2;
	}
	if (r != 2)
	{
		memcpy(nx.v, cs + 20, sizeof(nx.v)); // CurrentCameraPos Z/X/Y, pad, CurrentLookAt Z/X/Y, pad
		nx.fov = *(int16_t *)0x1D8E038;
		nx.roll = *(int16_t *)0x1D977A2;
		nx.cs = cs;
		nx.valid = true;
	}
	memcpy(cs, cs_save, sizeof(cs_save));
	*(int16_t *)0x1D8E038 = fov;
	*(int16_t *)0x1D977A2 = roll;
	*(uint16_t *)0x1D97718 = shots;
	*(uint32_t *)0x1D999C4 = pool;
	memcpy((void *)0xB8B800, ret_view, sizeof(ret_view));
	*(int16_t *)0x1D977A0 = ret_fov;
	*(int16_t *)0x1D9771C = ret_roll;
}

// held frame of a natively ported effect that drives the camera: its exact in-between camera
static bool ff8_bgate_fx_held_camera(int16_t world[3], int16_t lookat[3]);

int __cdecl ff8_bgate_updatecam_hook()
{
	int32_t *wxz = (int32_t *)0xB8B7F0, *wy = (int32_t *)0xB8B7F4;
	int32_t *lxz = (int32_t *)0xB8B7F8, *ly = (int32_t *)0xB8B7FC;
	int32_t before[4] = { *wxz, *wy, *lxz, *ly };
	static ff8_bgate_cam_next_t nx;
	nx.valid = false;
	if (ff8_bgate_phase != 0 && FF8_BGATE_CAM_LOOKAHEAD)
		ff8_bgate_cam_lookahead(nx);

	unreplace_function(ff8_bgate_updatecam_ri);
	int r = ff8_bgate_updatecam_orig();
	rereplace_function(ff8_bgate_updatecam_ri);

	int32_t after[4] = { *wxz, *wy, *lxz, *ly };
	if (ff8_bgate_phase == 0)
	{
		// real frame: snapshot the freshly-computed, fully-vanilla output
		ff8_bgate_cam_prev2 = ff8_bgate_cam_prev1;
		ff8_bgate_cam_prev1.wxz = after[0];
		ff8_bgate_cam_prev1.wy = after[1];
		ff8_bgate_cam_prev1.lxz = after[2];
		ff8_bgate_cam_prev1.ly = after[3];
		ff8_bgate_cam_prev1.valid = true;
	}
	else if (memcmp(before, after, sizeof(before)) != 0)
	{
		// the engine moved the camera itself on this held frame (return snap, blend, cut):
		// that value wins, and extrapolation restarts from it
		ff8_bgate_cam_prev1.wxz = after[0]; ff8_bgate_cam_prev1.wy = after[1];
		ff8_bgate_cam_prev1.lxz = after[2]; ff8_bgate_cam_prev1.ly = after[3];
		ff8_bgate_cam_prev1.valid = true;
		ff8_bgate_cam_prev2 = ff8_bgate_cam_prev1;
	}
	else if (int16_t fw[3], fl[3]; ff8_bgate_fx_held_camera(fw, fl))
	{
		// a ported effect writes the camera: it knows the next tick's camera exactly
		memcpy(ff8_bgate_cam_true, after, sizeof(after));
		int16_t *w16 = (int16_t *)0xB8B7F0, *l16 = (int16_t *)0xB8B7F8;
		for (int i = 0; i < 3; i++) { w16[i] = fw[i]; l16[i] = fl[i]; }
		ff8_bgate_cam_written[0] = *wxz; ff8_bgate_cam_written[1] = *wy;
		ff8_bgate_cam_written[2] = *lxz; ff8_bgate_cam_written[3] = *ly;
		ff8_bgate_cam_nudged = true;
	}
	else if (nx.valid && memcmp(after, nx.cs + 20, sizeof(after)) == 0)
	{
		// a camera shot is playing (the output is the keyframe player's): exact midpoint
		memcpy(ff8_bgate_cam_true, after, sizeof(after));
		int16_t cur[8], mid[8];
		memcpy(cur, nx.cs + 20, sizeof(cur));
		for (int i = 0; i < 8; i++)
			mid[i] = (int16_t)(cur[i] + ff8_bgate_scale_round(nx.v[i] - cur[i], ff8_bgate_phase, ff8_bgate_n));
		mid[3] = cur[3];
		mid[7] = cur[7];
		memcpy(wxz, mid, 16); // world XZ, world Y(+pad), look-at XZ, look-at Y(+pad): consecutive globals
		ff8_bgate_cam_written[0] = *wxz; ff8_bgate_cam_written[1] = *wy;
		ff8_bgate_cam_written[2] = *lxz; ff8_bgate_cam_written[3] = *ly;
		ff8_bgate_cam_nudged = true;
		int16_t *fov = (int16_t *)0x1D8E038, *roll = (int16_t *)0x1D977A2;
		ff8_bgate_cam_fovroll_true[0] = *fov;
		ff8_bgate_cam_fovroll_true[1] = *roll;
		*fov = (int16_t)(*fov + ff8_bgate_scale_round(nx.fov - *fov, ff8_bgate_phase, ff8_bgate_n));
		*roll = (int16_t)(*roll + ff8_bgate_scale_round(nx.roll - *roll, ff8_bgate_phase, ff8_bgate_n));
		ff8_bgate_cam_fovroll_written[0] = *fov;
		ff8_bgate_cam_fovroll_written[1] = *roll;
		ff8_bgate_cam_fovroll_nudged = true;
	}
	else if (ff8_bgate_cam_prev1.valid && ff8_bgate_cam_prev2.valid)
	{
		memcpy(ff8_bgate_cam_true, after, sizeof(after));
		*wxz = ff8_bgate_extrap_s16pair(ff8_bgate_cam_prev2.wxz, ff8_bgate_cam_prev1.wxz, ff8_bgate_phase, ff8_bgate_n);
		*wy = ff8_bgate_extrap_i32(ff8_bgate_cam_prev2.wy, ff8_bgate_cam_prev1.wy, ff8_bgate_phase, ff8_bgate_n);
		*lxz = ff8_bgate_extrap_s16pair(ff8_bgate_cam_prev2.lxz, ff8_bgate_cam_prev1.lxz, ff8_bgate_phase, ff8_bgate_n);
		*ly = ff8_bgate_extrap_i32(ff8_bgate_cam_prev2.ly, ff8_bgate_cam_prev1.ly, ff8_bgate_phase, ff8_bgate_n);
		ff8_bgate_cam_written[0] = *wxz; ff8_bgate_cam_written[1] = *wy;
		ff8_bgate_cam_written[2] = *lxz; ff8_bgate_cam_written[3] = *ly;
		ff8_bgate_cam_nudged = true;
	}
	return r;
}

// --- magic/GF effect pacing: native-rate tick + OT-insert record/replay ---
// The battle effect tick (call @0x50093A -> ExecuteTaskQueue(C3_28_GF_data_pointer))
// both ADVANCES and DRAWS the active magic/GF effect tree. Two failed designs first:
// skipping the tick on held frames = 15fps flicker (no draw); snapshot/restore around
// a held tick = SOFTLOCKS, because effect state is spread over regions that cannot be
// enumerated (per-effect BSS block, a packet scratch growing 71520 bytes/state, and a
// SHARED GROWING PARTICLE HEAP at ~0x020Fxxxx whose list header has no pool bounds -
// measured live on Esuna: its emitter's live-particle counts sat inside the restore
// window while the particles died outside it, so the counts never reached zero and the
// busy_lock completion chain hung the battle forever).
//
// This design never rewinds anything. The tick runs ONLY on real frames (phase 0), so
// every effect - magic, Draw/Stock, limits, GF summons - advances at native 15fps by
// construction. For the draw: all GPU packets enter the frame via the SSIGPU ordering
// table through exactly two primitives, SSIGPU_InsertPrimDepthKeys (0x45C870) and
// SSIGPU_InsertPrimAutoDepth (0x45C7A0). During the real tick we RECORD every insert
// (bucket, packet, depth keys); on held frames we RE-ISSUE the same inserts against the
// current frame's OT (rebased: the OT lives at g_Battle_FrameRenderListBase+68 and
// flips per frame). This is safe because:
//  - the insert allocates its linkage node from the per-frame ssigpu execution arena
//    and only refreshes the packet tag's link bits - re-inserting a packet is a
//    first-class engine operation;
//  - effect packet memory is double-buffered per TICK (activeVram/vramEven/vramOdd),
//    and at n=2 the frame-flat buffer parity of real frames is untouched on held
//    frames, so recorded packet pointers stay valid until the next real tick. (n=4
//    would need packet copies for flat-buffer packets - not handled yet.)
// No held tick also means no re-triggered SFX or damage numbers (the old suppression
// hooks are gone), and no per-effect whitelists: everything is paced uniformly.
#define FF8_BGATE_CUR_OT() (*(uint32_t *)0x1D8E04C + 68) // g_Battle_FrameRenderListBase + 68
// the whole ordering-table span effects can insert into: the render list's 17 header buckets
// (some draws use indices below 0, e.g. Quezacotl) plus the OT proper
#define FF8_BGATE_OT_SPAN_BASE() (*(uint32_t *)0x1D8E04C)
#define FF8_BGATE_OT_SPAN_WORDS (17 + 4386)

static int (__cdecl *ff8_bgate_effect_tick_orig)(void *effect_ctx) = nullptr;

// Held-frame interpolation of the effect draws (all magic, GF summons, limits, Draw...).
// Every effect part - particle, sprite, prim model, mesh, GF creature - reaches the screen
// as a PSX GPU packet whose vertices are already projected to screen space. So instead of
// per-spell work, the held frame is built from the last TWO real ticks: each primitive of
// tick N is paired with its counterpart in tick N-1 and its vertices (and vertex colors)
// are moved forward by (N - (N-1)) * phase / n. At n=2 that is exactly half a native step,
// i.e. the in-between pose, and it matches the camera, which is extrapolated the same way.
// Texture coordinates are never touched: flipbook sprite art keeps its native 15fps
// cadence, only the motion is smoothed. Anything that cannot be paired safely (new particle,
// changed primitive type/texture page, jump larger than FF8_BGATE_FX_MAX_STEP) is redrawn
// exactly as tick N drew it - the previous hold behaviour, never worse.
//
// Packets are COPIED at the end of each real tick (1) so the tick N-1 content survives
// the effects' own double-buffered packet arenas, and (2) so the held-frame inserts link
// our copies into the OT instead of re-linking game memory the effect may already be
// recycling (the cause of the Cure replay fault the blacklist below was guarding against).
// Recording: every primitive reaches the frame as a 24-byte node in the SSIGPU execution
// arena, linked into an ordering-table bucket. FOUR engine functions create nodes -
// 0x45C740 (plain), 0x45C7A0 (GTE auto depth), 0x45C870 (explicit depth keys) and 0x45C8E0
// (msk=1: the executor's alternate viewport mode) - and 0x45C860 edits the last node's msk
// afterwards. Hooking two of them missed draws (Leviathan flashed at 15Hz: part of it was
// drawn on real frames only) and replaying through one of them lost msk. So the tick's draws
// are read straight from the arena instead: the nodes created during the effect tick are
// exactly [cursor before, cursor after), each one with its final keys and msk, and its
// bucket is found by walking the OT (effect nodes sit on top of their bucket's chain, since
// nothing else inserts during the tick). No hooks at all on the shared insert primitives.
#define FF8_BGATE_EXEC_START 0x1C48828u                // ssigpu_execution_start
#define FF8_BGATE_EXEC_CUR (*(uint32_t *)0x1CA8828)     // ssigpu_execution_cur
#define FF8_BGATE_EXEC_LIMIT 0x60000u                   // arena size, as checked by the inserts
#define FF8_BGATE_OT_BUCKETS 4386
#define FF8_BGATE_RLIST_CUR (*(uint32_t *)0x1D8E054)    // battle model render-list cursor (diagnostics)
#define FF8_BGATE_FX_MAX_PRIMS 4096
#define FF8_BGATE_FX_ARENA_WORDS (256 * 1024)  // 1 MB of packet copies per tick
#define FF8_BGATE_FX_MAX_VERTS 64              // vertices tracked per packet
#define FF8_BGATE_FX_MAX_STEP 48               // px per native step; beyond = cut, don't extrapolate
#define FF8_BGATE_FX_MAX_STEP_INORDER 128      // same, for an exact-UV counterpart (meshes: unambiguous)

#pragma pack(push, 1)
struct ff8_bgate_exec_node // struc_34_ssgi_execution
{
	uint32_t *pkt;   // +0x00 packet (tag low 24 bits = previous bucket head, low part)
	int32_t k[4];    // +0x04 depth keys
	uint16_t msk;    // +0x14 bit0 = alternate viewport mode
	uint8_t link_hi; // +0x16 previous bucket head, high byte
	uint8_t pad;
};
#pragma pack(pop)
static_assert(sizeof(ff8_bgate_exec_node) == 24, "SSIGPU execution node is 24 bytes");

static int16_t ff8_bgate_fx_node_bucket[FF8_BGATE_FX_MAX_PRIMS];

// task node -> range of SSIGPU nodes it created (indices relative to the tick's first node)
struct ff8_bgate_task_rng { void *node; uint16_t a, b; };
#define FF8_BGATE_TASK_LOG 1024

// One real tick's worth of effect draws, copied
struct ff8_bgate_fx_prim
{
	int32_t k[4];
	uint16_t bucket; // index in the battle OT
	uint16_t msk;
	uint16_t ei;     // index of the node in the tick (task ranges refer to it)
	const uint32_t *src_pkt; // where the packet lived in the game's packet arena (re-read at display)
	uint32_t off;    // word offset of the packet copy in the arena
	uint32_t words;  // packet size in words, tag included
	uint32_t sig, sig_uv; // identities (0/0 + !parsed when the packet could not be parsed)
	int32_t next_uv, next_w; // hash chains (see ff8_bgate_fx_snap)
	bool parsed;
};
#define FF8_BGATE_FX_HASH 4096 // chain heads per identity table (power of two)
struct ff8_bgate_fx_snap
{
	ff8_bgate_fx_prim prim[FF8_BGATE_FX_MAX_PRIMS];
	int32_t head_uv[FF8_BGATE_FX_HASH]; // strong identity -> first primitive with it
	int32_t head_w[FF8_BGATE_FX_HASH];  // weak identity -> first primitive with it
	int n;
	int orphans;          // nodes whose bucket could not be found (not replayed)
	uint32_t rlist_delta; // bytes the effect tick pushed to the battle model render list
	uint32_t used;
	void *ctx;
	uint32_t frame;
	bool valid;
	uint32_t arena[FF8_BGATE_FX_ARENA_WORDS];
	// look-ahead pairing (ff8_bgate_la_*): which task drew each primitive
	ff8_bgate_task_rng tasks[FF8_BGATE_TASK_LOG];
	int ntasks;
	int16_t ei2prim[FF8_BGATE_FX_MAX_PRIMS]; // node index -> primitive index (-1: orphan)
	int16_t task_of[FF8_BGATE_FX_MAX_PRIMS]; // primitive -> innermost task entry (-1: none)
};
// One recorder per gated task queue that draws. Two exist: the magic/GF effect tree
// (C3_28_GF_data_pointer, call @0x50093A) and the hit-effect queue (call @0x500923:
// impact sparks, hit camera shakes, footstep dust, Renzokuken parts, MAG_332..343...),
// which vanilla also ticks once per host frame. The recorder code below works on the
// CURRENT recorder (ff8_bgate_R), selected by each gate before it records or replays.
struct ff8_bgate_rec_t
{
	const char *name;
	ff8_bgate_fx_snap snaps[2];
	int cur;              // snaps[cur] = tick N, snaps[cur ^ 1] = tick N-1
	bool replay_ok;
	uint32_t rec_begin;   // arena cursor before the real tick
	uint32_t rec_ot, rec_rlist;
	void *blacklist[16];
	int blacklist_n;
	int last_r;           // queue return value of the last real tick (reported on held frames)
	ff8_bgate_fx_sum_t sum;
	uint32_t out[FF8_BGATE_FX_ARENA_WORDS]; // held-frame packets (live until the frame is drawn)
	ff8_bgate_fx_snap la;                   // tick N+1 (look-ahead), when available
};
static ff8_bgate_rec_t ff8_bgate_rec_fx = { "effect" };
static ff8_bgate_rec_t ff8_bgate_rec_eq = { "hit-effect queue" };
static ff8_bgate_rec_t *ff8_bgate_R = &ff8_bgate_rec_fx;
#define ff8_bgate_fx_snaps (ff8_bgate_R->snaps)
#define ff8_bgate_fx_cur (ff8_bgate_R->cur)
#define ff8_bgate_fx_out (ff8_bgate_R->out)
#define ff8_bgate_fx_replay_ok (ff8_bgate_R->replay_ok)
#define ff8_bgate_fx_rec_begin (ff8_bgate_R->rec_begin)
#define ff8_bgate_fx_rec_ot (ff8_bgate_R->rec_ot)
#define ff8_bgate_fx_rec_rlist (ff8_bgate_R->rec_rlist)
#define ff8_bgate_fx_sum (ff8_bgate_R->sum)
#define ff8_bgate_fx_replay_blacklist (ff8_bgate_R->blacklist)
#define ff8_bgate_fx_replay_blacklist_n (ff8_bgate_R->blacklist_n)

// Per-task draw ranges for the look-ahead pairing (see ff8_bgate_la_* below): while
// recording, ExecuteTaskQueue 0x508420 is replaced by this instruction-for-instruction
// re-implementation that also notes which SSIGPU nodes each task created and which queues
// ran (their pools are part of the look-ahead snapshot).
struct ff8_bgate_task_node { uint16_t flags; uint16_t seq; ff8_bgate_task_node *next; int (__cdecl *func)(ff8_bgate_task_node *); };
struct ff8_bgate_task_queue { ff8_bgate_task_node *head, *tail; uint8_t *pool; uint16_t node_size, capacity; };
static int (__cdecl *ff8_bgate_etq_orig)(void *) = nullptr;
static uint32_t ff8_bgate_etq_ri = 0;
static bool ff8_bgate_etq_rec = false;
static ff8_bgate_task_rng ff8_bgate_etq_log[FF8_BGATE_TASK_LOG];
static int ff8_bgate_etq_log_n = 0;
static bool ff8_bgate_etq_log_overflow = false;
static bool ff8_bgate_etq_cycle = false;
static void *ff8_bgate_etq_queues[32];
static int ff8_bgate_etq_queues_n = 0;
static bool ff8_bgate_la_active = false;       // held frame being built from the look-ahead
static bool ff8_bgate_la_in_lookahead = false; // look-ahead pass running (new queues get saved on first sight)
static void ff8_bgate_la_pool_save(ff8_bgate_task_queue *q);

static int __cdecl ff8_bgate_etq_hook(void *qv)
{
	ff8_bgate_task_queue *q = (ff8_bgate_task_queue *)qv;
	int k = 0;
	if (ff8_bgate_etq_rec)
		while (k < ff8_bgate_etq_queues_n && ff8_bgate_etq_queues[k] != qv) k++;
	if (ff8_bgate_etq_rec && k == ff8_bgate_etq_queues_n)
	{
		if (k < 32) ff8_bgate_etq_queues[ff8_bgate_etq_queues_n++] = qv;
		if (ff8_bgate_la_in_lookahead) ff8_bgate_la_pool_save(q); // first seen inside the look-ahead: save before it runs
	}
	ff8_bgate_task_node *prev = nullptr, *cur = q->head;
	int kept = 0, guard = 0;
	for (; cur; cur = cur->next)
	{
		if (++guard > 65536)
		{
			ffnx_info("30fps la: task list of queue %p cycles (lookahead=%d) - aborted\n", qv, (int)ff8_bgate_la_in_lookahead);
			ff8_bgate_etq_cycle = true;
			break;
		}
		uint32_t before = FF8_BGATE_EXEC_CUR;
		int (__cdecl *fn)(ff8_bgate_task_node *) = cur->func;
		if (ff8fx::g_active)
		{
			void *port = ff8fx::lookup((uint32_t)fn); // native twin of this task function
			if (port) fn = (int (__cdecl *)(ff8_bgate_task_node *))port;
		}
		int r = fn(cur);
		uint32_t after = FF8_BGATE_EXEC_CUR;
		if (ff8_bgate_etq_rec && after > before && before >= ff8_bgate_fx_rec_begin)
		{
			if (ff8_bgate_etq_log_n < FF8_BGATE_TASK_LOG)
			{
				ff8_bgate_task_rng &e = ff8_bgate_etq_log[ff8_bgate_etq_log_n++];
				e.node = cur;
				e.a = (uint16_t)((before - ff8_bgate_fx_rec_begin) / sizeof(ff8_bgate_exec_node));
				e.b = (uint16_t)((after - ff8_bgate_fx_rec_begin) / sizeof(ff8_bgate_exec_node));
			}
			else ff8_bgate_etq_log_overflow = true;
		}
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


// Walks a PSX GPU packet (tag word + GP0 commands) and lists the word indexes holding
// screen vertices (int16 x | int16 y << 16) and vertex colors (0x00BBGGRR). The signature
// hashes everything that identifies "the same primitive" across ticks: size, command
// codes and texture page/CLUT - but not UVs (flipbook frames advance on the same sprite)
// nor positions/colors (those are what moves). Returns false on commands it does not know
// (VRAM transfers, fills...): such a packet is redrawn as-is.
struct ff8_bgate_pkt_info
{
	uint16_t xy[FF8_BGATE_FX_MAX_VERTS];
	uint16_t col[FF8_BGATE_FX_MAX_VERTS];
	int nxy, ncol;
	uint32_t sig;      // weak identity: packet shape + texture page/CLUT
	uint32_t sig_uv;   // strong identity: weak + every UV word (a mesh triangle keeps its UVs)
	uint32_t tpage;    // texture page word of the last textured primitive (diagnostics)
	uint32_t last_cmd; // command being decoded when parsing stopped (diagnostics)
};

static bool ff8_bgate_pkt_parse(const uint32_t *p, uint32_t words, ff8_bgate_pkt_info &pi)
{
	pi.nxy = pi.ncol = 0;
	pi.last_cmd = 0;
	pi.tpage = 0xFFFFFFFF;
	uint32_t sig = 2166136261u ^ words;
	uint32_t uv = 2166136261u;
	#define FF8_BGATE_UV(v) (uv = (uv ^ (uint32_t)(v)) * 16777619u)
	#define FF8_BGATE_SIG(v) (sig = (sig ^ (uint32_t)(v)) * 16777619u)
	#define FF8_BGATE_XY(w) do { if (pi.nxy >= FF8_BGATE_FX_MAX_VERTS) return false; pi.xy[pi.nxy++] = (uint16_t)(w); } while (0)
	#define FF8_BGATE_COL(w) do { if (pi.ncol >= FF8_BGATE_FX_MAX_VERTS) return false; pi.col[pi.ncol++] = (uint16_t)(w); } while (0)
	uint32_t w = 1;
	while (w < words)
	{
		uint32_t cmd = p[w] >> 24;
		pi.last_cmd = cmd;
		FF8_BGATE_SIG(cmd);
		if (cmd >= 0x20 && cmd < 0x40) // polygon: color, then per vertex [color] xy [uv]
		{
			int nv = (cmd & 0x08) ? 4 : 3;
			bool tex = (cmd & 0x04) != 0, gouraud = (cmd & 0x10) != 0;
			uint32_t need = 1 + nv + (tex ? nv : 0) + (gouraud ? nv - 1 : 0);
			if (w + need > words) return false;
			FF8_BGATE_COL(w++);
			for (int v = 0; v < nv; v++)
			{
				if (v > 0 && gouraud) FF8_BGATE_COL(w++);
				FF8_BGATE_XY(w++);
				if (tex)
				{
					if (v == 1) pi.tpage = p[w] >> 16;
					if (v < 2) FF8_BGATE_SIG(p[w] >> 16); // v0: CLUT, v1: texture page
					FF8_BGATE_UV(p[w]);
					w++;
				}
			}
		}
		else if (cmd >= 0x40 && cmd < 0x60) // line / polyline
		{
			bool poly = (cmd & 0x08) != 0, gouraud = (cmd & 0x10) != 0;
			if (w + 2 > words) return false;
			FF8_BGATE_COL(w++);
			FF8_BGATE_XY(w++);
			if (!poly)
			{
				if (gouraud) { if (w >= words) return false; FF8_BGATE_COL(w++); }
				if (w >= words) return false;
				FF8_BGATE_XY(w++);
			}
			else
			{
				while (w < words && (p[w] & 0xF000F000) != 0x50005000)
				{
					if (gouraud) FF8_BGATE_COL(w++);
					if (w >= words) return false;
					FF8_BGATE_XY(w++);
				}
				w++; // terminator
			}
		}
		else if (cmd >= 0x60 && cmd < 0x80) // rectangle / sprite: color, xy, [uv+clut], [w,h]
		{
			bool tex = (cmd & 0x04) != 0, var = (cmd & 0x18) == 0;
			uint32_t need = 2 + (tex ? 1 : 0) + (var ? 1 : 0);
			if (w + need > words) return false;
			FF8_BGATE_COL(w++);
			FF8_BGATE_XY(w++);
			if (tex) { FF8_BGATE_SIG(p[w] >> 16); FF8_BGATE_UV(p[w]); w++; }
			if (var) { FF8_BGATE_SIG(p[w]); w++; }
		}
		else if (cmd >= 0xE1 && cmd <= 0xE6) // draw mode / texture window / area / offset / mask
		{
			FF8_BGATE_SIG(p[w]);
			w++;
		}
		else if (cmd == 0x00 && p[w] == 0) // NOP padding
			w++;
		else
			return false;
	}
	pi.sig = sig;
	pi.sig_uv = (uv == 2166136261u) ? 0 : (sig ^ uv) * 16777619u; // 0 = untextured: no strong identity
	return true;
	#undef FF8_BGATE_SIG
	#undef FF8_BGATE_XY
	#undef FF8_BGATE_COL
	#undef FF8_BGATE_UV
}

// Largest per-vertex screen step between the two versions of a primitive
static int ff8_bgate_pkt_step(const uint32_t *a, const uint32_t *b, const ff8_bgate_pkt_info &pi)
{
	int d = 0;
	for (int i = 0; i < pi.nxy; i++)
	{
		uint32_t va = a[pi.xy[i]], vb = b[pi.xy[i]];
		int dx = abs((int)(int16_t)(va & 0xFFFF) - (int)(int16_t)(vb & 0xFFFF));
		int dy = abs((int)(int16_t)(va >> 16) - (int)(int16_t)(vb >> 16));
		if (dx > d) d = dx;
		if (dy > d) d = dy;
	}
	return d;
}

// round(v * num / den), symmetric around 0
static inline int ff8_bgate_scale_round(int v, int num, int den)
{
	int t = v * num;
	return (t >= 0) ? (t + den / 2) / den : -((-t + den / 2) / den);
}

#define FF8_BGATE_FX_MAX_COLOR_STEP 12
// dst (a copy of cur) := cur + (cur - prev) * num / den, on vertices and vertex colors
static int ff8_bgate_pkt_extrapolate(uint32_t *dst, const uint32_t *cur, const uint32_t *prev, const ff8_bgate_pkt_info &pi, int num, int den, bool colors, int cap = FF8_BGATE_FX_MAX_COLOR_STEP)
{
	int max_dc = 0;
	for (int i = 0; i < pi.nxy; i++)
	{
		uint32_t c = cur[pi.xy[i]], p = prev[pi.xy[i]];
		int cx = (int16_t)(c & 0xFFFF), cy = (int16_t)(c >> 16);
		int px = (int16_t)(p & 0xFFFF), py = (int16_t)(p >> 16);
		int x = cx + ff8_bgate_scale_round(cx - px, num, den);
		int y = cy + ff8_bgate_scale_round(cy - py, num, den);
		if (x < -32768) x = -32768; if (x > 32767) x = 32767;
		if (y < -32768) y = -32768; if (y > 32767) y = 32767;
		dst[pi.xy[i]] = (uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16);
	}
	if (!colors)
		return 0;
	for (int i = 0; i < pi.ncol; i++)
	{
		uint32_t c = cur[pi.col[i]], p = prev[pi.col[i]];
		uint32_t out = c & 0xFF000000; // command byte (first color word) stays
		for (int s = 0; s < 24; s += 8)
		{
			int cc = (c >> s) & 0xFF, pc = (p >> s) & 0xFF;
			int step = ff8_bgate_scale_round(cc - pc, num, den);
			// capped: a fade keeps its slope, a sudden brightness jump (flash onset) is not
			// amplified - overshooting additive primitives saturate to white for one frame
			if (step > cap) step = cap;
			if (step < -cap) step = -cap;
			int v = cc + step;
			if (v < 0) v = 0; if (v > 255) v = 255;
			if (abs(v - cc) > max_dc) max_dc = abs(v - cc);
			out |= (uint32_t)v << s;
		}
		dst[pi.col[i]] = out;
	}
	return max_dc;
}

// Indexes the tick's primitives by identity, so the next held frame can find each one's
// previous-tick counterpart without depending on the (culling-sensitive) emission order
static void ff8_bgate_fx_index(ff8_bgate_fx_snap &s)
{
	static ff8_bgate_pkt_info pi;
	for (int h = 0; h < FF8_BGATE_FX_HASH; h++)
		s.head_uv[h] = s.head_w[h] = -1;
	for (int i = s.n - 1; i >= 0; i--) // backwards so chains come out in emission order
	{
		ff8_bgate_fx_prim &e = s.prim[i];
		e.parsed = ff8_bgate_pkt_parse(&s.arena[e.off], e.words, pi) && pi.nxy > 0;
		e.sig = e.parsed ? pi.sig : 0;
		e.sig_uv = e.parsed ? pi.sig_uv : 0;
		e.next_uv = e.next_w = -1;
		if (!e.parsed)
			continue;
		uint32_t hu = e.sig_uv & (FF8_BGATE_FX_HASH - 1), hw = e.sig & (FF8_BGATE_FX_HASH - 1);
		if (e.sig_uv != 0) { e.next_uv = s.head_uv[hu]; s.head_uv[hu] = i; }
		e.next_w = s.head_w[hw]; s.head_w[hw] = i;
	}
}

// End of a real tick: copy every node the effect created, while the packets are fresh
static void ff8_bgate_fx_capture_unsafe(ff8_bgate_fx_snap &s)
{
	uint32_t begin = ff8_bgate_fx_rec_begin, end = FF8_BGATE_EXEC_CUR;
	if (end < begin || (end - begin) % sizeof(ff8_bgate_exec_node) != 0)
		return; // arena was flushed during the tick - nothing trustworthy to replay
	int n = (int)((end - begin) / sizeof(ff8_bgate_exec_node));
	if (n > FF8_BGATE_FX_MAX_PRIMS)
	{
		ffnx_info("30fps fx: %d draws in one effect tick (> %d), held-frame redraw skipped\n", n, FF8_BGATE_FX_MAX_PRIMS);
		return;
	}
	for (int i = 0; i < n; i++)
		ff8_bgate_fx_node_bucket[i] = -1;

	// bucket of each node: walk every bucket's chain while it stays inside the tick's nodes
	uint32_t *ot = (uint32_t *)ff8_bgate_fx_rec_ot;
	for (int b = 0; b < FF8_BGATE_OT_BUCKETS; b++)
	{
		uint32_t v = ot[b];
		while (v >= begin && v < end && (v - begin) % sizeof(ff8_bgate_exec_node) == 0)
		{
			int i = (int)((v - begin) / sizeof(ff8_bgate_exec_node));
			if (ff8_bgate_fx_node_bucket[i] >= 0)
				break;
			ff8_bgate_fx_node_bucket[i] = (int16_t)b;
			const ff8_bgate_exec_node *node = (const ff8_bgate_exec_node *)v;
			v = (node->pkt[0] & 0xFFFFFF) | ((uint32_t)node->link_hi << 24);
		}
	}

	for (int i = 0; i < n; i++)
	{
		const ff8_bgate_exec_node *node = (const ff8_bgate_exec_node *)(begin + i * sizeof(ff8_bgate_exec_node));
		if (ff8_bgate_fx_node_bucket[i] < 0)
		{
			s.orphans++;
			continue;
		}
		uint32_t words = (node->pkt[0] >> 24) + 1;
		if (s.used + words > FF8_BGATE_FX_ARENA_WORDS)
			return;
		ff8_bgate_fx_prim &d = s.prim[s.n++];
		memcpy(d.k, node->k, sizeof(d.k));
		d.bucket = (uint16_t)ff8_bgate_fx_node_bucket[i];
		d.msk = node->msk;
		d.ei = (uint16_t)i;
		d.off = s.used;
		d.words = words;
		d.src_pkt = node->pkt;
		memcpy(&s.arena[s.used], node->pkt, words * 4);
		s.used += words;
	}
	ff8_bgate_fx_index(s);
	s.valid = true;
}

// PACKET ALIASING (Eden's "warped screen", vanilla behaviour): a cinematic effect's tick saves
// the packet-arena cursor (battle_texture_data_ptr), draws - some bones allocate their packets
// IN THE FRAME ARENA - then rewinds the cursor. The stage/entity draws that follow in BdLink
// allocate from the same cursor and OVERWRITE those packets before the ordering table is
// executed (display @0x5006DF), so at display time the effect's OT nodes point at other draws'
// data: that aliased content is what the screen shows every frame in vanilla (stable). The
// copies taken at tick end held the effect's TRUE packets, so held frames showed something the
// real frames never did -> the ghost flashed at 15Hz. Re-reading every captured packet from its
// original address at display time reproduces exactly what vanilla displays; the number of
// packets that changed in between is reported (realiased) to confirm the mechanism.
static bool ff8_bgate_fx_recapture_pending = false;
// Cinematic effects call SSIGPU_ClearOrderingTable(OT, 4096) at the start of their tick,
// discarding everything inserted earlier in the frame (camera/AnimSeq/hit-effect queues).
// Detected on real ticks (a bucket that held nodes before the tick is back to its cleared
// link afterwards) and reproduced on held frames before the replay, so the same draws are
// discarded on both kinds of frame.
static bool ff8_bgate_fx_otclear = false;
static uint32_t ff8_bgate_fx_ot_probe[2]; // bucket 1 and 4095 heads before the tick
static uint32_t ff8_bgate_fx_realiased = 0;

static void ff8_bgate_fx_recapture_unsafe(ff8_bgate_fx_snap &s)
{
	for (int i = 0; i < s.n; i++)
	{
		ff8_bgate_fx_prim &d = s.prim[i];
		const uint32_t *pkt = d.src_pkt;
		if (!pkt) continue;
		uint32_t words = (pkt[0] >> 24) + 1;
		if (words != d.words) // size byte overwritten too: keep the copy that still fits
			words = (words < d.words) ? words : d.words;
		if (memcmp(&s.arena[d.off], pkt, words * 4) != 0)
		{
			memcpy(&s.arena[d.off], pkt, words * 4);
			ff8_bgate_fx_realiased++;
		}
	}
	ff8_bgate_fx_index(s); // identities may have changed with the content
}

static void ff8_bgate_fx_recapture_at_display()
{
	if (!ff8_bgate_fx_recapture_pending) return;
	ff8_bgate_fx_recapture_pending = false;
	ff8_bgate_R = &ff8_bgate_rec_fx;
	ff8_bgate_fx_snap &s = ff8_bgate_fx_snaps[ff8_bgate_fx_cur];
	if (!s.valid) return;
	uint32_t before = ff8_bgate_fx_realiased;
	__try
	{
		ff8_bgate_fx_recapture_unsafe(s);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ffnx_info("30fps: display-time packet re-read faulted (effect_ctx=%p)\n", s.ctx);
	}
	static uint32_t logged = 0;
	if (ff8_bgate_fx_sum.ticks <= 8) logged = 0;
	if (logged < 8)
	{
		logged++;
		ffnx_info("30fps alias: f=%u effect_id=%d prims=%d realiased_this_frame=%u otclear_seen=%d\n",
			ff8_bgate_frame_no, *(int *)0x1D99A68 + 1, s.n, ff8_bgate_fx_realiased - before, (int)ff8_bgate_fx_otclear);
	}
}

// display_texture_related_sub_45D610 (0x45D610): executes the battle OT (SSIGPU_DrawOrderingTableAndReset)
static int (__cdecl *ff8_bgate_display_orig)(unsigned int) = nullptr;
static uint32_t ff8_bgate_display_ri = 0;

int __cdecl ff8_bgate_display_hook(unsigned int arg)
{
	ff8_bgate_fx_recapture_at_display();
	unreplace_function(ff8_bgate_display_ri);
	int r = ff8_bgate_display_orig(arg);
	rereplace_function(ff8_bgate_display_ri);
	return r;
}

static void ff8_bgate_fx_capture(void *effect_ctx)
{
	ff8_bgate_fx_cur ^= 1;
	ff8_bgate_fx_snap &s = ff8_bgate_fx_snaps[ff8_bgate_fx_cur];
	s.n = 0;
	s.ntasks = 0; // no task tags unless attached for this tick
	s.orphans = 0;
	s.rlist_delta = FF8_BGATE_RLIST_CUR - ff8_bgate_fx_rec_rlist;
	s.used = 0;
	s.ctx = effect_ctx;
	s.frame = ff8_bgate_frame_no;
	s.valid = false;
	__try
	{
		ff8_bgate_fx_capture_unsafe(s);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		s.valid = false;
		ffnx_info("30fps: effect_ctx=%p packet copy faulted, held frame skipped\n", effect_ctx);
	}
}

// Blacklist of effect_ctx whose replay faulted once - never replayed again (that effect
// steps at native 15fps with no held-frame redraw, everything else keeps replaying).

static bool ff8_bgate_fx_replay_is_blacklisted(void *ctx)
{
	for (int i = 0; i < ff8_bgate_fx_replay_blacklist_n; i++)
		if (ff8_bgate_fx_replay_blacklist[i] == ctx) return true;
	return false;
}

static void ff8_bgate_fx_replay_blacklist_add(void *ctx)
{
	if (ff8_bgate_fx_replay_is_blacklisted(ctx)) return;
	if (ff8_bgate_fx_replay_blacklist_n < 16) ff8_bgate_fx_replay_blacklist[ff8_bgate_fx_replay_blacklist_n++] = ctx;
}

// Vertex weld table for one held frame: screen position of a tick-N vertex -> summed
// displacement of every paired primitive that has a vertex there. Generation-stamped so it
// never needs clearing (a stale entry from an older frame reads as empty).
#define FF8_BGATE_WELD_SLOTS 65536 // power of two, > 4 x FF8_BGATE_FX_MAX_PRIMS
#define FF8_BGATE_WELD_SAMPLES 8
struct ff8_bgate_weld_slot { uint32_t key, gen; int32_t n; int16_t dx[FF8_BGATE_WELD_SAMPLES], dy[FF8_BGATE_WELD_SAMPLES]; };
static ff8_bgate_weld_slot ff8_bgate_weld[FF8_BGATE_WELD_SLOTS];
static uint32_t ff8_bgate_fx_used[FF8_BGATE_FX_MAX_PRIMS]; // prev primitive already paired (== weld gen)
static uint32_t ff8_bgate_weld_gen = 0;

static ff8_bgate_weld_slot *ff8_bgate_weld_find(uint32_t key, bool create)
{
	uint32_t h = (key * 2654435761u) >> 16;
	for (int probe = 0; probe < 64; probe++)
	{
		ff8_bgate_weld_slot &s = ff8_bgate_weld[(h + probe) & (FF8_BGATE_WELD_SLOTS - 1)];
		if (s.gen != ff8_bgate_weld_gen)
		{
			if (!create) return nullptr;
			s.gen = ff8_bgate_weld_gen; s.key = key; s.n = 0;
			return &s;
		}
		if (s.key == key) return &s;
	}
	return nullptr; // table saturated around this key: vertex simply is not welded
}

static void ff8_bgate_weld_add(uint32_t key, int dx, int dy)
{
	ff8_bgate_weld_slot *s = ff8_bgate_weld_find(key, true);
	if (!s || s->n >= FF8_BGATE_WELD_SAMPLES) return;
	s->dx[s->n] = (int16_t)dx;
	s->dy[s->n] = (int16_t)dy;
	s->n++;
}

static int ff8_bgate_median(int16_t *v, int n)
{
	for (int i = 1; i < n; i++) // n <= 8: insertion sort
	{
		int16_t x = v[i];
		int j = i - 1;
		while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
		v[j + 1] = x;
	}
	return v[(n - 1) / 2]; // lower median: an even split leans to the smaller-index sample
}

// One displacement per vertex position, voted by every paired primitive touching it: the
// component-wise median, so a minority of wrong pairs (repeated texture tiles give several
// triangles identical UVs) is outvoted instead of tearing the mesh. Two disagreeing votes
// keep the smaller movement - the conservative choice when there is no majority.
static bool ff8_bgate_weld_get(uint32_t key, int &dx, int &dy)
{
	ff8_bgate_weld_slot *s = ff8_bgate_weld_find(key, false);
	if (!s || s->n == 0) return false;
	if (s->n == 2)
	{
		int a = abs(s->dx[0]) + abs(s->dy[0]), b = abs(s->dx[1]) + abs(s->dy[1]);
		int k = (a <= b) ? 0 : 1;
		dx = s->dx[k]; dy = s->dy[k];
		return true;
	}
	int16_t tx[FF8_BGATE_WELD_SAMPLES], ty[FF8_BGATE_WELD_SAMPLES];
	memcpy(tx, s->dx, s->n * sizeof(int16_t));
	memcpy(ty, s->dy, s->n * sizeof(int16_t));
	dx = ff8_bgate_median(tx, s->n);
	dy = ff8_bgate_median(ty, s->n);
	return true;
}

// A primitive and its true counterpart one native tick earlier have nearly the same shape;
// a wrong pair (a neighbouring tile with the same UVs, a different particle) usually does
// not. Rejects pairs whose edges changed length by more than 1.5x (edges under 4px ignored).
static bool ff8_bgate_pkt_same_shape(const uint32_t *a, const uint32_t *b, const ff8_bgate_pkt_info &pi)
{
	if (pi.nxy < 3)
		return true; // sprites/lines: position only, nothing to compare
	for (int i = 0; i < pi.nxy; i++)
	{
		int j = (i + 1) % pi.nxy;
		uint32_t a0 = a[pi.xy[i]], a1 = a[pi.xy[j]], b0 = b[pi.xy[i]], b1 = b[pi.xy[j]];
		int ax = (int16_t)(a1 & 0xFFFF) - (int16_t)(a0 & 0xFFFF), ay = (int16_t)(a1 >> 16) - (int16_t)(a0 >> 16);
		int bx = (int16_t)(b1 & 0xFFFF) - (int16_t)(b0 & 0xFFFF), by = (int16_t)(b1 >> 16) - (int16_t)(b0 >> 16);
		long long la = (long long)ax * ax + (long long)ay * ay, lb = (long long)bx * bx + (long long)by * by;
		if (la < 16 && lb < 16)
			continue;
		// (1.5)^2 = 2.25 -> 4 * la > 9 * lb, both ways
		if (4 * la > 9 * lb || 4 * lb > 9 * la)
			return false;
	}
	return true;
}

// Held frame: insert an extrapolated copy of every primitive of tick N into this frame's OT
// look-ahead pairing: the counterpart of tick-N primitive i inside tick N+1 (la), by task:
// same task node, same rank when the task drew the same number of primitives, else the
// nearest same primitive within that task (and only when clearly nearer than the runner-up)
static int ff8_bgate_la_pair(const ff8_bgate_fx_snap &cur, const ff8_bgate_fx_snap &la, int i, const ff8_bgate_pkt_info &ci, const uint32_t *src, int &d)
{
	const ff8_bgate_fx_prim &e = cur.prim[i];
	int t = cur.task_of[i];
	if (t < 0) return -1;
	static int cache_t = -1, cache_lt = -1;
	static uint32_t cache_frame = 0xFFFFFFFF;
	if (cache_t != t || cache_frame != ff8_bgate_frame_no)
	{
		cache_t = t; cache_frame = ff8_bgate_frame_no; cache_lt = -1;
		for (int k = 0; k < la.ntasks; k++)
			if (la.tasks[k].node == cur.tasks[t].node) { cache_lt = k; break; }
	}
	if (cache_lt < 0) return -1;
	const ff8_bgate_task_rng &ct = cur.tasks[t], &lt = la.tasks[cache_lt];
	if (ct.b - ct.a == lt.b - lt.a)
	{
		uint32_t ei = lt.a + (e.ei - ct.a);
		int j = ei < FF8_BGATE_FX_MAX_PRIMS ? la.ei2prim[ei] : -1;
		if (j >= 0 && la.prim[j].words == e.words && la.prim[j].sig == ci.sig && ff8_bgate_fx_used[j] != ff8_bgate_weld_gen)
		{
			d = ff8_bgate_pkt_step(src, &la.arena[la.prim[j].off], ci);
			return d <= FF8_BGATE_FX_MAX_STEP_INORDER ? j : -1;
		}
	}
	int best = -1, best_d = 0x7FFFFFFF, second_d = 0x7FFFFFFF;
	for (uint32_t ei = lt.a; ei < lt.b && ei < FF8_BGATE_FX_MAX_PRIMS; ei++)
	{
		int j = la.ei2prim[ei];
		if (j < 0 || la.prim[j].words != e.words || la.prim[j].sig != ci.sig || ff8_bgate_fx_used[j] == ff8_bgate_weld_gen) continue;
		int dd = ff8_bgate_pkt_step(src, &la.arena[la.prim[j].off], ci);
		if (dd < best_d) { second_d = best_d; best = j; best_d = dd; }
		else if (dd < second_d) second_d = dd;
	}
	if (best < 0 || best_d > FF8_BGATE_FX_MAX_STEP) return -1;
	if (second_d != 0x7FFFFFFF && best_d * 2 + 2 > second_d) return -1;
	d = best_d;
	return best;
}

static bool ff8_bgate_fx_dump_armed = true; // TEMP diagnostics: dump once per game run

static bool ff8_bgate_fx_skip_held = false; // replay without the tasks redrawn natively

static void ff8_bgate_fx_replay_unsafe()
{
	const ff8_bgate_fx_snap &cur = ff8_bgate_fx_snaps[ff8_bgate_fx_cur];
	const ff8_bgate_fx_snap &prev = ff8_bgate_fx_snaps[ff8_bgate_fx_cur ^ 1];
	const ff8_bgate_fx_snap &la = ff8_bgate_R->la;
	// look-ahead mode: interpolate toward the exact next tick (per-task pairing)
	bool la_mode = ff8_bgate_la_active && la.valid && la.ctx == cur.ctx && la.frame == cur.frame;
	// only interpolate against the directly preceding real tick of the same effect
	bool interp = la_mode || ((ff8_bgate_fx_mode == 1 || ff8_bgate_fx_mode == 2) && prev.valid && prev.ctx == cur.ctx && cur.frame - prev.frame == (uint32_t)ff8_bgate_n);
	int st_match = 0, st_nosig = 0, st_far = 0, st_unparsed = 0, st_maxcol = 0, st_welded = 0;
	uint32_t st_badcmd = 0;
	uint32_t cur_ot = FF8_BGATE_CUR_OT();
	uint32_t out_used = 0;
	static ff8_bgate_pkt_info ci;

	// Pass 1: pair primitives, extrapolate colors of the paired ones, and collect one screen
	// displacement per VERTEX POSITION (welding). Pass 2 applies those displacements to every
	// primitive touching the position, paired or not - so triangles sharing a corner always
	// move that corner identically and meshes never crack open along their edges (per-primitive
	// decisions left gaps wherever a neighbour was rejected: Leviathan showed its triangles).
	ff8_bgate_weld_gen++;
	// TEMP diagnostics: per-primitive dump (Eden 206 screen collapse, ticks 61..64)
	bool dump = ff8_bgate_R == &ff8_bgate_rec_fx && *(int *)0x1D99A68 + 1 == 206 && false && ff8_bgate_fx_dump_armed;
	static int dump_pair[2048], dump_d[2048]; static uint8_t dump_kind[2048];
	for (int i = 0; i < cur.n; i++)
	{
		const ff8_bgate_fx_prim &e = cur.prim[i];
		const uint32_t *src = &cur.arena[e.off];
		uint32_t *dst = &ff8_bgate_fx_out[out_used];
		memcpy(dst, src, e.words * 4);
		out_used += e.words;
		if (i < 2048) { dump_pair[i] = -1; dump_d[i] = -1; dump_kind[i] = 0; }

		bool parsed = interp && ff8_bgate_pkt_parse(src, e.words, ci);
		if (interp && !parsed) { st_unparsed++; st_badcmd = ci.last_cmd; ff8_bgate_fx_sum.badcmd = ci.last_cmd; }
		if (parsed && la_mode)
		{
			int d = 0;
			int j = ff8_bgate_la_pair(cur, la, i, ci, src, d);
			if (j >= 0)
			{
				const uint32_t *lp = &la.arena[la.prim[j].off];
				if (ff8_bgate_pkt_same_shape(src, lp, ci))
				{
					ff8_bgate_fx_used[j] = ff8_bgate_weld_gen;
					// dst := cur + (la - cur) * phase / n, colours uncapped (both ends are real)
					int dc = ff8_bgate_pkt_extrapolate(dst, src, lp, ci, -ff8_bgate_phase, ff8_bgate_n, ff8_bgate_fx_mode >= 2, 255);
					if (dc > st_maxcol) st_maxcol = dc;
					for (int v = 0; v < ci.nxy; v++)
					{
						uint32_t c = src[ci.xy[v]], l = lp[ci.xy[v]];
						ff8_bgate_weld_add(c,
							ff8_bgate_scale_round((int)(int16_t)(l & 0xFFFF) - (int)(int16_t)(c & 0xFFFF), ff8_bgate_phase, ff8_bgate_n),
							ff8_bgate_scale_round((int)(int16_t)(l >> 16) - (int)(int16_t)(c >> 16), ff8_bgate_phase, ff8_bgate_n));
					}
					st_match++;
				}
				else st_far++;
			}
			else st_nosig++;
		}
		else if (parsed && (ci.nxy > 0 || ci.ncol > 0))
		{
			// counterpart = the closest not-yet-used primitive of tick N-1 with the same
			// STRONG identity (same UVs: a mesh triangle, or a particle on the same sprite
			// frame); failing that, the same WEAK identity within FF8_BGATE_FX_MAX_STEP
			// (flipbook sprites whose UVs advanced). Independent of emission order, which
			// shifts whenever a few triangles get culled or particles spawn/die.
			int best = -1, best_d = 0x7FFFFFFF, limit = FF8_BGATE_FX_MAX_STEP_INORDER;
			for (int c = ci.sig_uv ? prev.head_uv[ci.sig_uv & (FF8_BGATE_FX_HASH - 1)] : -1; c >= 0; c = prev.prim[c].next_uv)
			{
				const ff8_bgate_fx_prim &q = prev.prim[c];
				if (q.sig_uv != ci.sig_uv || q.words != e.words || ff8_bgate_fx_used[c] == ff8_bgate_weld_gen) continue;
				int d = ff8_bgate_pkt_step(src, &prev.arena[q.off], ci);
				if (d < best_d) { best = c; best_d = d; }
			}
			if (best < 0 || best_d > FF8_BGATE_FX_MAX_STEP_INORDER)
			{
				best = -1; best_d = 0x7FFFFFFF; limit = FF8_BGATE_FX_MAX_STEP;
				int second_d = 0x7FFFFFFF;
				for (int c = prev.head_w[ci.sig & (FF8_BGATE_FX_HASH - 1)]; c >= 0; c = prev.prim[c].next_w)
				{
					const ff8_bgate_fx_prim &q = prev.prim[c];
					if (q.sig != ci.sig || q.words != e.words || ff8_bgate_fx_used[c] == ff8_bgate_weld_gen) continue;
					int d = ff8_bgate_pkt_step(src, &prev.arena[q.off], ci);
					if (d < best_d) { second_d = best_d; best = c; best_d = d; }
					else if (d < second_d) second_d = d;
				}
				// The weak identity is shared by every primitive of a batch (all rows of a
				// screen-warp, all particles of a cloud): "the nearest one" is only meaningful
				// when it is clearly nearer than the runner-up. Eden's TV-collapse draws the
				// screen as ~300 one-pixel rows whose UVs change every tick; each row paired
				// with whatever row sat at its new height and the held frame was torn apart.
				if (best >= 0 && second_d != 0x7FFFFFFF && best_d * 2 + 2 > second_d)
				{
					best = -1;
					ff8_bgate_fx_sum.ambiguous++;
				}
			}
			if (best >= 0 && i < 2048) { dump_pair[i] = best; dump_d[i] = best_d; dump_kind[i] = (limit == FF8_BGATE_FX_MAX_STEP_INORDER) ? 1 : 2; }
			if (best >= 0)
			{
				const uint32_t *qp = &prev.arena[prev.prim[best].off];
				if (best_d <= limit && ff8_bgate_pkt_same_shape(src, qp, ci))
				{
					if (i < 2048) dump_kind[i] |= 0x10; // accepted
					ff8_bgate_fx_used[best] = ff8_bgate_weld_gen;
					int dc = ff8_bgate_pkt_extrapolate(dst, src, qp, ci, ff8_bgate_phase, ff8_bgate_n, ff8_bgate_fx_mode >= 2);
					if (dc > st_maxcol) st_maxcol = dc;
					for (int v = 0; v < ci.nxy; v++)
					{
						uint32_t c = src[ci.xy[v]], p = qp[ci.xy[v]];
						ff8_bgate_weld_add(c,
							ff8_bgate_scale_round((int)(int16_t)(c & 0xFFFF) - (int)(int16_t)(p & 0xFFFF), ff8_bgate_phase, ff8_bgate_n),
							ff8_bgate_scale_round((int)(int16_t)(c >> 16) - (int)(int16_t)(p >> 16), ff8_bgate_phase, ff8_bgate_n));
					}
					st_match++;
				}
				else st_far++;
			}
			else st_nosig++;
		}
	}

	// Pass 2: welded vertex positions, then link every primitive into this frame's OT
	out_used = 0;
	for (int i = 0; i < cur.n; i++)
	{
		const ff8_bgate_fx_prim &e = cur.prim[i];
		const uint32_t *src = &cur.arena[e.off];
		uint32_t *dst = &ff8_bgate_fx_out[out_used];
		out_used += e.words;

		if (interp && ff8_bgate_pkt_parse(src, e.words, ci))
		{
			if (ci.tpage != 0xFFFFFFFF) // diagnostics: which texture pages this effect draws from
			{
				auto &s = ff8_bgate_fx_sum;
				int k = 0;
				while (k < s.ntpages && s.tpages[k] != ci.tpage) k++;
				if (k < 8)
				{
					if (k == s.ntpages) { s.tpages[k] = ci.tpage; s.tpage_hits[k] = 0; s.ntpages++; }
					s.tpage_hits[k]++;
				}
			}
			for (int v = 0; v < ci.nxy; v++)
			{
				uint32_t c = src[ci.xy[v]];
				int dx, dy;
				if (!ff8_bgate_weld_get(c, dx, dy))
					continue; // no paired primitive touches this position: stays as tick N drew it
				// the voted displacement overrides each primitive's own, so shared corners agree
				int x = (int)(int16_t)(c & 0xFFFF) + dx, y = (int)(int16_t)(c >> 16) + dy;
				if (x < -32768) x = -32768; if (x > 32767) x = 32767;
				if (y < -32768) y = -32768; if (y > 32767) y = 32767;
				dst[ci.xy[v]] = (uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16);
				st_welded++;
			}
		}

		if (ff8_bgate_fx_skip_held && cur.ntasks > 0 && cur.task_of[i] >= 0
			&& ff8fx::held_redraws((uint32_t)((ff8_bgate_task_node *)cur.tasks[cur.task_of[i]].node)->func))
			continue; // drawn in between by the native port (ff8fx held frame)

		if (ff8_bgate_fx_mode == 4 || ff8_bgate_fx_mode == 5)
		{
			static ff8_bgate_pkt_info bi;
			bool is15 = ff8_bgate_pkt_parse(src, e.words, bi) && bi.tpage != 0xFFFFFFFF && ((bi.tpage >> 7) & 3) == 2;
			if ((ff8_bgate_fx_mode == 4) == is15)
				continue; // mode 4 skips the 15bpp prims, mode 5 keeps only them
		}

		// VRAM transfer commands (GP0 0x80-0x9F VRAM->VRAM copy, 0xA0 CPU->VRAM, 0xC0 VRAM->CPU)
		// are never replayed: effects such as Eden (206) copy the framebuffer into a texture to
		// build mirrors/distortions; repeating that copy on a held frame grabs whatever the
		// screen holds at that moment (other buffer, 30fps UI) -> duplicated, flashing UI.
		// The texture keeps the last real frame's copy, exactly like vanilla at 15fps.
		{
			static ff8_bgate_pkt_info xi;
			if (!ff8_bgate_pkt_parse(src, e.words, xi) && xi.last_cmd >= 0x80 && xi.last_cmd <= 0xDF)
			{
				ff8_bgate_fx_sum.vram_xfer++;
				ff8_bgate_fx_sum.badcmd = xi.last_cmd;
				continue;
			}
		}

		if (dump && i < 2048)
		{
			char l[500]; int o = 0;
			o += _snprintf_s(l + o, sizeof(l) - o, _TRUNCATE, "30fps dump: t=%u i=%d bucket=%u words=%u pair=%d d=%d kind=%02X |", ff8_bgate_fx_sum.ticks, i, e.bucket, e.words, dump_pair[i], dump_d[i], dump_kind[i]);
			for (uint32_t k = 1; k < e.words && k < 10 && o > 0 && o < 400; k++)
				o += _snprintf_s(l + o, sizeof(l) - o, _TRUNCATE, " %08X", src[k]);
			if (o > 0 && o < 400) o += _snprintf_s(l + o, sizeof(l) - o, _TRUNCATE, " | out");
			for (uint32_t k = 1; k < e.words && k < 10 && o > 0 && o < 480; k++)
				if (dst[k] != src[k]) o += _snprintf_s(l + o, sizeof(l) - o, _TRUNCATE, " [%u]=%08X", k, dst[k]);
			if (dump_pair[i] >= 0 && o > 0 && o < 440)
			{
				const uint32_t *qp = &prev.arena[prev.prim[dump_pair[i]].off];
				o += _snprintf_s(l + o, sizeof(l) - o, _TRUNCATE, " | prev %08X %08X %08X", qp[1], e.words > 2 ? qp[2] : 0, e.words > 3 ? qp[3] : 0);
			}
			ffnx_info("%s\n", l);
		}

		// link it exactly like the engine's inserts do (SSIGPU_InsertPrimDepthKeys + msk)
		if (FF8_BGATE_EXEC_CUR - FF8_BGATE_EXEC_START >= FF8_BGATE_EXEC_LIMIT)
			break; // draw list full - same limit the engine applies
		ff8_bgate_exec_node *node = (ff8_bgate_exec_node *)FF8_BGATE_EXEC_CUR;
		uint32_t *bucket = (uint32_t *)cur_ot + e.bucket;
		uint32_t old_head = *bucket;
		node->pkt = dst;
		memcpy(node->k, e.k, sizeof(node->k));
		node->msk = e.msk;
		node->link_hi = (uint8_t)(old_head >> 24);
		*bucket = (uint32_t)node;
		dst[0] = (dst[0] & 0xFF000000) | (old_head & 0xFFFFFF);
		FF8_BGATE_EXEC_CUR += sizeof(ff8_bgate_exec_node);
	}
	if (dump && ff8_bgate_fx_sum.ticks == 300) ff8_bgate_fx_dump_armed = false;
	ff8_bgate_fx_sum.held++;
	if (interp) ff8_bgate_fx_sum.interp_held++;
	ff8_bgate_fx_sum.prims += cur.n;
	ff8_bgate_fx_sum.match += st_match;
	ff8_bgate_fx_sum.far_ += st_far;
	ff8_bgate_fx_sum.nosig += st_nosig;
	ff8_bgate_fx_sum.unparsed += st_unparsed;
	ff8_bgate_fx_sum.welded += st_welded;
	ff8_bgate_fx_sum.orphans += cur.orphans;
	if ((uint32_t)st_maxcol > ff8_bgate_fx_sum.maxcol) ff8_bgate_fx_sum.maxcol = st_maxcol;
	if (ff8_bgate_fx_log_stats)
		ffnx_info("30fps fx: f=%u ctx=%p mode=%d interp=%d prims=%d prev=%d match=%d nosig=%d far=%d unparsed=%d(last cmd %02X) orphans=%d welded=%d maxcol=%d words=%u\n",
			ff8_bgate_frame_no, cur.ctx, ff8_bgate_fx_mode, (int)interp, cur.n, prev.n, st_match, st_nosig, st_far,
			st_unparsed, st_badcmd, cur.orphans, st_welded, st_maxcol, out_used);
}

static void ff8_bgate_fx_replay(void *effect_ctx)
{
	if (ff8_bgate_fx_replay_is_blacklisted(effect_ctx) || !ff8_bgate_fx_snaps[ff8_bgate_fx_cur].valid)
		return;
	__try
	{
		ff8_bgate_fx_replay_unsafe();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ff8_bgate_fx_replay_blacklist_add(effect_ctx);
		ffnx_info("30fps: effect_ctx=%p faulted during held-frame replay -> blacklisted (native 15fps, no redraw)\n", effect_ctx);
	}
}

// One line per finished effect: which effect (MAGIC_EFFECT_INDEX @0x1D99A68 = effect id - 1,
// same numbering as Fujin), how many held frames were interpolated and how well pairing
// went. match% low or unparsed/orphans > 0 point at an effect worth a closer look.
// diagnostics: texture pages the replayed primitives sampled, and the battle's two draw
// environments (PSX DRAWENV: clip rect x/y/w/h then draw offset) - a primitive whose texture
// page points inside a draw buffer is sampling the SCREEN (Eden's mirror/warp).
static const char *ff8_bgate_fx_tpages_str()
{
	static char buf[1024];
	int o = 0;
	auto &s = ff8_bgate_fx_sum;
	#define FF8_BGATE_APPEND(...) do { if (o < (int)sizeof(buf) - 1) o += _snprintf_s(buf + o, sizeof(buf) - o, _TRUNCATE, __VA_ARGS__); if (o < 0 || o > (int)sizeof(buf) - 1) o = (int)sizeof(buf) - 1; } while (0)
	FF8_BGATE_APPEND(" | tpages:");
	for (int i = 0; i < s.ntpages && o < 150; i++)
	{
		uint32_t tp = s.tpages[i];
		// PSX tpage word: bits 0-3 = X base / 64, bit 4 = Y base / 256, bits 5-6 = blend, 7-8 = bpp
		FF8_BGATE_APPEND(" %04X(x=%u,y=%u,abr=%u,bpp=%u)x%u", tp, (tp & 0xF) * 64, ((tp >> 4) & 1) * 256,
			(tp >> 5) & 3, (tp >> 7) & 3, s.tpage_hits[i]);
	}
	int16_t *d0 = (int16_t *)0x1D969C8, *d1 = (int16_t *)0x1D96A24;
	int16_t *p0 = (int16_t *)0x1D96980, *p1 = (int16_t *)0x1D96994; // DISPENV per parity
	FF8_BGATE_APPEND(" | parity=%u dispenv0=%d,%d %dx%d dispenv1=%d,%d %dx%d rlistbase=%08X pktarena=%08X",
		*(uint8_t *)0x1D96A80, p0[0], p0[1], p0[2], p0[3], p1[0], p1[1], p1[2], p1[3],
		*(uint32_t *)0x1D8E04C, *(uint32_t *)0x1D8E054);
	FF8_BGATE_APPEND(" | realiased=%u", ff8_bgate_fx_realiased);
	ff8_bgate_fx_realiased = 0;
	FF8_BGATE_APPEND(" | vramq t0=%u t1=%u t2=%u t3=%u", s.vq_types[0], s.vq_types[1], s.vq_types[2], s.vq_types[3]);
	FF8_BGATE_APPEND(" | drawenv0=%d,%d %dx%d drawenv1=%d,%d %dx%d",
		d0[0], d0[1], d0[2], d0[3], d1[0], d1[1], d1[2], d1[3]);
	#undef FF8_BGATE_APPEND
	return buf;
}

static void ff8_bgate_fx_summary(const char *why)
{
	if (ff8_bgate_fx_sum.ticks == 0 || ff8_bgate_fx_sum.prims == 0)
	{
		// nothing was ever redrawn (empty queue polled, one-tick effect): not worth a line
		memset(&ff8_bgate_fx_sum, 0, sizeof(ff8_bgate_fx_sum));
		return;
	}
	uint32_t pr = ff8_bgate_fx_sum.prims ? ff8_bgate_fx_sum.prims : 1;
	ffnx_info("30fps fx summary (%s %s): effect_id=%d ticks=%u held=%u interpolated=%u | prims=%u paired=%u%% rejected=%u%% new=%u%% unparsed=%u(cmd %02X) vram_xfer_skipped=%u ambiguous=%u orphans=%u welded_vtx=%u maxcol=%u mode=%d%s\n",
		ff8_bgate_R->name, why, ff8_bgate_R == &ff8_bgate_rec_fx ? *(int *)0x1D99A68 + 1 : -1, ff8_bgate_fx_sum.ticks, ff8_bgate_fx_sum.held, ff8_bgate_fx_sum.interp_held,
		ff8_bgate_fx_sum.prims, (ff8_bgate_fx_sum.match * 100) / pr, (ff8_bgate_fx_sum.far_ * 100) / pr,
		(ff8_bgate_fx_sum.nosig * 100) / pr, ff8_bgate_fx_sum.unparsed, ff8_bgate_fx_sum.badcmd, ff8_bgate_fx_sum.vram_xfer, ff8_bgate_fx_sum.ambiguous, ff8_bgate_fx_sum.orphans,
		ff8_bgate_fx_sum.welded, ff8_bgate_fx_sum.maxcol, ff8_bgate_fx_mode, ff8_bgate_fx_tpages_str());
	memset(&ff8_bgate_fx_sum, 0, sizeof(ff8_bgate_fx_sum));
}

// Battle VRAM command queue (32 slots of 16 bytes at 0x1D98220, counter g_next_command_slot
// at 0x1D98420, applied once per frame by Battle_FlushVramCommandQueue): effects queue their
// texture work there. Types: 0 = raw rect upload, 1 = TIM upload, 2 = VRAM read-back,
// 3 = VRAM->VRAM blit. Eden (206) refreshes the screen SNAPSHOT it warps (UI included - that
// ghost is vanilla behaviour) with such a copy every tick; gated to real frames it refreshed
// at 15/s while the screen is redrawn at 30/s, so one frame in two showed a stale snapshot:
// the ghost flashed. Re-issue only the COPY types (2, 3) on held frames - uploads of fixed
// texture data (0, 1) would just cost time.
#define FF8_BGATE_VQ_SLOT(i) ((uint8_t *)(0x1D98220 + 16 * (i)))
#define FF8_BGATE_VQ_COUNT (*(int *)0x1D98420)
static uint8_t ff8_bgate_vq_copy[8][16];
static int ff8_bgate_vq_copies = 0;

static uint32_t ff8_bgate_vq_trace = 0; // frames left to trace

static void ff8_bgate_vq_dump(const char *what, uint8_t *c)
{
	int16_t *r = (int16_t *)(c + 4);
	uint32_t d = *(uint32_t *)(c + 12);
	ffnx_info("30fps vq: f=%u ph=%d parity=%u arena=%08X rlist=%08X %s type=%u src=(%d,%d %dx%d) dst=(%u,%u) raw=%08X %08X %08X %08X\n",
		ff8_bgate_frame_no, ff8_bgate_phase, *(uint8_t *)0x1D96A80, *(uint32_t *)0x1D8E054, *(uint32_t *)0x1D8E04C,
		what, c[0], r[0], r[1], r[2], r[3],
		d & 0xFFFF, d >> 16, *(uint32_t *)c, *(uint32_t *)(c + 4), *(uint32_t *)(c + 8), d);
}

static void ff8_bgate_vq_record(int from, int to)
{
	ff8_bgate_vq_copies = 0;
	for (int i = from; i < to && i < 32; i++)
	{
		uint8_t *c = FF8_BGATE_VQ_SLOT(i);
		if (ff8_bgate_vq_trace) ff8_bgate_vq_dump("queued-by-effect", c);
		if ((ff8_bgate_vq_replay_all || c[0] == 2 || c[0] == 3) && ff8_bgate_vq_copies < 8)
			memcpy(ff8_bgate_vq_copy[ff8_bgate_vq_copies++], c, 16);
	}
}

static void ff8_bgate_vq_replay()
{
	for (int i = 0; i < ff8_bgate_vq_copies; i++)
	{
		int slot = FF8_BGATE_VQ_COUNT;
		if (slot >= 31) return; // the engine's own bound (slot 31 is never written)
		memcpy(FF8_BGATE_VQ_SLOT(slot), ff8_bgate_vq_copy[i], 16);
		FF8_BGATE_VQ_COUNT = slot + 1;
		if (ff8_bgate_vq_trace) ff8_bgate_vq_dump("re-queued-held", FF8_BGATE_VQ_SLOT(slot));
	}
}

// TEMP diagnostics: frame-state differ. Snapshots renderer / camera / battle / cinematic
// globals at the effect-tick point on every frame; on selected held frames logs the dwords
// that differ from the last real frame but did NOT differ between the last two real frames.
struct ff8_bgate_diff_region { uint32_t addr, dwords; };
static const ff8_bgate_diff_region ff8_bgate_diff_regions[] = {
	{ 0x1D8E000, 64 },  // battle render list header / packet arena ptrs / fov / screen offsets
	{ 0xB7CC00, 16 },   // SSIGPU pass state (B7CC00/04/1C/20/24)
	{ 0x1D97700, 48 },  // battle camera globals
	{ 0x1D96A80, 12 },  // parity, update flags, queues
	{ 0x2797300, 456 }, // cinematic engine runtime globals
	{ 0x1CA8828, 8 },   // ssigpu exec cursor etc.
};
#define FF8_BGATE_DIFF_TOTAL (64 + 16 + 48 + 12 + 456 + 8)
static uint32_t ff8_bgate_diff_real[2][FF8_BGATE_DIFF_TOTAL]; // [0] = last real, [1] = the one before
static bool ff8_bgate_diff_real_ok[2];

static void ff8_bgate_diff_snapshot(uint32_t *dst)
{
	int o = 0;
	for (auto &r : ff8_bgate_diff_regions)
	{
		memcpy(dst + o, (void *)r.addr, r.dwords * 4);
		o += r.dwords;
	}
}

static void ff8_bgate_diff_frame(bool real)
{
	static uint32_t held[FF8_BGATE_DIFF_TOTAL];
	if (real)
	{
		memcpy(ff8_bgate_diff_real[1], ff8_bgate_diff_real[0], sizeof(ff8_bgate_diff_real[0]));
		ff8_bgate_diff_real_ok[1] = ff8_bgate_diff_real_ok[0];
		ff8_bgate_diff_snapshot(ff8_bgate_diff_real[0]);
		ff8_bgate_diff_real_ok[0] = true;
		return;
	}
	uint32_t tk = ff8_bgate_fx_sum.ticks;
	if (!(tk >= 150 && tk <= 154) || !ff8_bgate_diff_real_ok[0] || !ff8_bgate_diff_real_ok[1])
		return;
	ff8_bgate_diff_snapshot(held);
	char line[900]; int o = 0, n = 0;
	int idx = 0;
	for (auto &r : ff8_bgate_diff_regions)
	{
		for (uint32_t i = 0; i < r.dwords; i++, idx++)
		{
			uint32_t h = held[idx], r0 = ff8_bgate_diff_real[0][idx], r1 = ff8_bgate_diff_real[1][idx];
			if (h != r0 && r0 == r1)
			{
				if (o < (int)sizeof(line) - 40)
					o += sprintf(line + o, " %08X:%08X->%08X", r.addr + i * 4, r0, h);
				n++;
			}
		}
	}
	line[o] = 0;
	ffnx_info("30fps diff: f=%u tick=%u held-vs-real (stable across real) n=%d%s\n", ff8_bgate_frame_no, tk, n, line);
}

// --- dedicated held-frame path for the "cinematic engine" GF summons ("true 30 fps") ---
// Ifrit 201, Leviathan 006, Bahamut 202, Cerberus 203, Alexander 204, Brothers 205 and Eden 206
// are seven compilations of one engine (study: gf_study/cinematic_*.md, true30_*.md). A tick is
//   SequenceTick: ++seqCounter, rand(), copy the battle view into node 0 (CamMatrixMain),
//                 if (!paused) { integrator (vel += acc; accum += vel; outputs), anim VM }
//                 BuildMatricesAndDraw                                    <- DrawHandlerTable[bone+0x1C]
// On PC the whole VM runs after the integrator; the integrator is the ONLY per-tick rate in the
// animation: everything else is script events separated by waits. So the exact in-between state
// of tick k-1 -> k is known before tick k runs: accum_{k-1} + vel_k / 2 with vel_k = vel + acc.
// A held frame (host frame between two ticks) therefore
//   - calls the effect queue with battle_to_update_flags bit 0 set: SequenceTick runs its preamble
//     (node 0 = the smoothed battle view, packet cursors) and skips the animation phases;
//     seqCounter and the CRT rand seed are put back afterwards;
//   - inside BuildMatricesAndDraw (hooked): snapshots the bones, node matrices, light sets, the
//     particle pools and the lazy handler pools, then writes the midpoint state: integrator half
//     step per bone of the bone order list (+ its bone handler), particles advanced by half a
//     step, and REPLAYS the pure output opcodes the VM executed during the last real tick (node
//     matrices, light sets, camera, colours, shadow) - pure functions of the bone outputs, node 0
//     and their operands, so they yield the exact in-between matrices with the new camera;
//   - draws with rt->boneSkipFlag = 0xFF (the engine's own draw-without-advancing switch for the
//     embedded model / sprite / particle handlers) and restores everything.
// Real ticks run unmodified, so every second host frame is bit-identical to vanilla.
// The generic 2D replay remains the fallback (F6, and anything not recognised).
struct ff8_bgate_gfc_module { int effect_id; uint32_t build_draw, draw_table, step_model, bone_table; const char *name; };
static const ff8_bgate_gfc_module ff8_bgate_gfc_modules[] = {
	{ 201, 0xB2ABE0, 0x1874D6C, 0xB26AD0, 0x1874B80, "Ifrit" },
	{ 6,   0xB5F5D0, 0x18776C0, 0xB5A480, 0x18774D4, "Leviathan" },
	{ 202, 0xB20270, 0x18741B8, 0xB19E90, 0x1873FCC, "Bahamut" },
	{ 203, 0xB135D0, 0x1873384, 0xB0D3F0, 0x1873198, "Cerberus" },
	{ 204, 0xB06E00, 0x187281C, 0xB00FF0, 0x1872630, "Alexander" },
	{ 205, 0xAF9ED0, 0x1871CA8, 0xAF5760, 0x1871ABC, "Brothers" },
	{ 206, 0xAEEC40, 0x187119C, 0xAE4150, 0x1870F78, "Eden" },
};
#define FF8_BGATE_GFC_MODULES 7
#define FF8_BGATE_GFC_VM_TABLE(dt)   ((dt) + 0x1A4) // VmOpcodeTable follows the draw table (328 slots)
// pure output opcodes recorded during the real tick and replayed on the held frame (true30_anim_side.md 4.4):
// matrix nodes / billboards, light set, camera (sub-op 0 only) + shake, colours from outputs,
// entity transform, entity shadow draw
static const int ff8_bgate_gfc_ops[] = { 0x65, 0x66, 0x67, 0x69, 0x6A, 0x7A, 0x84, 0xC4, 0x104, 0x105, 0x116, 0xCB,
	0x93, 0x39, 0x47, 0x40, 0x83, 0xD3, 0x48, 0x71 };
#define FF8_BGATE_GFC_OPS 20

#define FF8_BGATE_GFC_CTX      (*(uint8_t **)0x27973EC)  // g_GfCinematic_SequenceCtxPtr
#define FF8_BGATE_GFC_RT       (*(uint8_t **)0x27973B8)  // g_GfCinematic_RuntimeSlotPtr
#define FF8_BGATE_GFC_CURBONE  (*(uint8_t **)0x27973E8)  // g_GfCinematic_CurBonePtr
#define FF8_BGATE_GFC_CURSOR   (*(uint32_t *)0x2797450)  // g_GfCinematic_StreamCursor (VM instruction pointer)
#define FF8_BGATE_GFC_BONEORDER ((uint8_t *)0x2797454)   // g_GfCinematic_BoneOrderList (0xFF terminated, bit7 = runs while frozen)
#define FF8_BGATE_GFC_DRAWORDER ((uint8_t *)0x2797554)   // g_GfCinematic_DrawOrderList (0xFF terminated)
#define FF8_BGATE_GFC_NODES_BEGIN 0x27977A4              // light sets, billboard matrices, 64 node matrices
#define FF8_BGATE_GFC_NODES_END   0x27981E8
#define FF8_BGATE_GFC_MAX_BONES 128
#define FF8_BGATE_GFC_ARENA_MAX (1024 * 1024)
#define FF8_BGATE_GFC_POOL_MAX (256 * 1024)
#define FF8_BGATE_GFC_LOG_MAX 1024

#define ff8_bgate_gfc_enabled ff8_bgate_gfc_f6
static bool ff8_bgate_gfc_redraw = false;   // inside the held-frame second call
static bool ff8_bgate_gfc_logging = false;  // inside a real tick: pure opcodes are recorded
static uint32_t ff8_bgate_gfc_orig_op[FF8_BGATE_GFC_MODULES][FF8_BGATE_GFC_OPS];
static uint32_t ff8_bgate_gfc_build_ri[FF8_BGATE_GFC_MODULES];

struct ff8_bgate_gfc_logent { uint32_t fn; uint8_t *bone; uint32_t cursor; uint16_t opword; uint8_t op_idx; };
static ff8_bgate_gfc_logent ff8_bgate_gfc_log[FF8_BGATE_GFC_LOG_MAX];
static int ff8_bgate_gfc_log_n = 0;
static bool ff8_bgate_gfc_log_overflow = false;

static uint32_t ff8_bgate_gfc_tick = 0;      // real ticks seen for the running summon
static uint32_t ff8_bgate_gfc_faults = 0;    // held-frame faults of the running summon
static uint8_t *ff8_bgate_gfc_ctx = nullptr; // ctx of the running summon (seqCounter at +0x32)
static uint32_t ff8_bgate_gfc_arena_base = 0;
static uint8_t *ff8_bgate_gfc_save_bones = nullptr, *ff8_bgate_gfc_save_arena = nullptr, *ff8_bgate_gfc_save_pools = nullptr;
static uint8_t ff8_bgate_gfc_save_nodes[FF8_BGATE_GFC_NODES_END - FF8_BGATE_GFC_NODES_BEGIN];
static struct { uint32_t redraws, replayed_ops, bones_stepped, particles_stepped, models_stepped, model_faults, arena_bytes; uint8_t handlers[73]; } ff8_bgate_gfc_stats;

// Embedded battle models (draw handler 3): the creature itself. Its keyframes are read by the
// standard Battle_ReadAnimation, one frame per real tick, from the draw handler. The pose lives in
// the model's skeleton section (root at +8, bones of 48 bytes from +16 with rotations at +4/+6/+8,
// 4096 = one turn) and the reader's whole state is the 8-byte BattleAnimCmd at block+0x20, so the
// NEXT keyframe can be read ahead: snapshot cmd + skeleton, step once exactly as the next tick will
// (restart included), keep the resulting pose, put everything back, then write the midpoint pose
// and let the draw build its matrices from it (draw-only path of the step function).
#define FF8_BGATE_GFC_MODEL_SAVE (128 * 1024)
#define FF8_BGATE_GFC_MODEL_MAX 8
struct ff8_bgate_gfc_model_save { uint8_t *block; uint8_t *skel; uint32_t size; uint32_t off; uint8_t cmd[8]; uint16_t counter; };
static ff8_bgate_gfc_model_save ff8_bgate_gfc_models[FF8_BGATE_GFC_MODEL_MAX];
static int ff8_bgate_gfc_models_n = 0;
static uint8_t *ff8_bgate_gfc_model_buf = nullptr;

static bool ff8_bgate_gfc_step_model_guarded(uint32_t fn, uint8_t *block)
{
	__try
	{
		((void (__cdecl *)(uint8_t *))fn)(block);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

static inline int16_t ff8_bgate_gfc_mid_angle(int16_t cur, int16_t next)
{
	int d = ((int)next - (int)cur) & 0xFFF;
	if (d >= 2048) d -= 4096;
	return (int16_t)(cur + d / 2);
}

static void ff8_bgate_gfc_models_snapshot(uint8_t *bones, int n, int m, bool frozen)
{
	ff8_bgate_gfc_models_n = 0;
	if (!ff8_bgate_gfc_model_buf) ff8_bgate_gfc_model_buf = (uint8_t *)malloc(FF8_BGATE_GFC_MODEL_SAVE);
	if (!ff8_bgate_gfc_model_buf) return;
	uint32_t used = 0;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF && ff8_bgate_gfc_models_n < FF8_BGATE_GFC_MODEL_MAX; i++)
	{
		int id = FF8_BGATE_GFC_DRAWORDER[i] & 0x7F;
		if (id >= n) continue;
		uint8_t *b = bones + id * 0x100;
		if (b[0x1C] != 3) continue;
		uint8_t *block = *(uint8_t **)(b + 0xBC);
		if (!block) continue;
		uint8_t *skel = *(uint8_t **)(block + 0x30);
		if (!skel) continue;
		uint32_t size = 16 + 48 * skel[0];
		if (used + size > FF8_BGATE_GFC_MODEL_SAVE) break;
		ff8_bgate_gfc_model_save &s = ff8_bgate_gfc_models[ff8_bgate_gfc_models_n++];
		s.block = block; s.skel = skel; s.size = size; s.off = used;
		memcpy(s.cmd, block + 0x20, 8);
		s.counter = *(uint16_t *)block;
		memcpy(ff8_bgate_gfc_model_buf + used, skel, size);
		used += size;

		// the model advances next tick only if the step function will take its reading branch
		uint16_t flags = *(uint16_t *)(block + 2);
		if (frozen || (flags & 3) || s.counter == 0 || ff8_bgate_fx_mode < 2) continue;
		if (!ff8_bgate_gfc_step_model_guarded(ff8_bgate_gfc_modules[m].step_model, block))
		{
			ff8_bgate_gfc_stats.model_faults++;
			memcpy(skel, ff8_bgate_gfc_model_buf + s.off, size);
			memcpy(block + 0x20, s.cmd, 8);
			*(uint16_t *)block = s.counter;
			continue;
		}
		// next pose is in the skeleton now; current pose in the snapshot
		const uint8_t *cur = ff8_bgate_gfc_model_buf + s.off;
		int nb = skel[0];
		int16_t root[3], rot[64][3], scl[64][3];
		if (nb > 64) nb = 64;
		for (int k = 0; k < 3; k++) root[k] = (int16_t)(*(int16_t *)(cur + 8 + 2 * k) + (*(int16_t *)(skel + 8 + 2 * k) - *(int16_t *)(cur + 8 + 2 * k)) / 2);
		for (int j = 0; j < nb; j++)
			for (int k = 0; k < 3; k++)
			{
				rot[j][k] = ff8_bgate_gfc_mid_angle(*(int16_t *)(cur + 16 + 48 * j + 4 + 2 * k), *(int16_t *)(skel + 16 + 48 * j + 4 + 2 * k));
				int16_t sc = *(int16_t *)(cur + 16 + 48 * j + 10 + 2 * k), sn = *(int16_t *)(skel + 16 + 48 * j + 10 + 2 * k);
				scl[j][k] = (int16_t)(sc + (sn - sc) / 2);
			}
		bool scale = cur[1] != 0;
		// back to the current state, then the midpoint pose on top of it
		memcpy(skel, cur, size);
		memcpy(block + 0x20, s.cmd, 8);
		*(uint16_t *)block = s.counter;
		for (int k = 0; k < 3; k++) *(int16_t *)(skel + 8 + 2 * k) = root[k];
		for (int j = 0; j < nb; j++)
			for (int k = 0; k < 3; k++)
			{
				*(int16_t *)(skel + 16 + 48 * j + 4 + 2 * k) = rot[j][k];
				if (scale) *(int16_t *)(skel + 16 + 48 * j + 10 + 2 * k) = scl[j][k];
			}
		ff8_bgate_gfc_stats.models_stepped++;
	}
}

static void ff8_bgate_gfc_models_restore()
{
	for (int i = 0; i < ff8_bgate_gfc_models_n; i++)
	{
		ff8_bgate_gfc_model_save &s = ff8_bgate_gfc_models[i];
		memcpy(s.skel, ff8_bgate_gfc_model_buf + s.off, s.size);
		memcpy(s.block + 0x20, s.cmd, 8);
		*(uint16_t *)s.block = s.counter;
	}
	ff8_bgate_gfc_models_n = 0;
}

static int ff8_bgate_gfc_module_of_effect()
{
	int id = *(int *)0x1D99A68 + 1; // MAGIC_EFFECT_INDEX + 1
	for (int m = 0; m < FF8_BGATE_GFC_MODULES; m++)
		if (ff8_bgate_gfc_modules[m].effect_id == id) return m;
	return -1;
}

static void ff8_bgate_gfc_reset()
{
	ff8_bgate_gfc_tick = 0;
	ff8_bgate_gfc_ctx = nullptr;
	ff8_bgate_gfc_arena_base = 0;
	ff8_bgate_gfc_log_n = 0;
	ff8_bgate_gfc_faults = 0;
	ff8_bgate_gfc_redraw = ff8_bgate_gfc_logging = false;
}

template <int M, int J> static int __cdecl ff8_bgate_gfc_op_wrap()
{
	if (ff8_bgate_gfc_logging && !ff8_bgate_gfc_redraw)
	{
		if (ff8_bgate_gfc_log_n < FF8_BGATE_GFC_LOG_MAX)
		{
			ff8_bgate_gfc_logent &e = ff8_bgate_gfc_log[ff8_bgate_gfc_log_n++];
			e.fn = ff8_bgate_gfc_orig_op[M][J];
			e.bone = FF8_BGATE_GFC_CURBONE;
			e.cursor = FF8_BGATE_GFC_CURSOR;
			e.opword = *(uint16_t *)(FF8_BGATE_GFC_RT + 0x4A); // rt->curOpcode
			e.op_idx = (uint8_t)J;
		}
		else ff8_bgate_gfc_log_overflow = true;
	}
	return ((int (__cdecl *)())ff8_bgate_gfc_orig_op[M][J])();
}

static int ff8_bgate_gfc_bone_count()
{
	int n = 0;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_BONEORDER[i] != 0xFF; i++)
		if ((FF8_BGATE_GFC_BONEORDER[i] & 0x7F) + 1 > n) n = (FF8_BGATE_GFC_BONEORDER[i] & 0x7F) + 1;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF; i++)
		if ((FF8_BGATE_GFC_DRAWORDER[i] & 0x7F) + 1 > n) n = (FF8_BGATE_GFC_DRAWORDER[i] & 0x7F) + 1;
	return n > FF8_BGATE_GFC_MAX_BONES ? FF8_BGATE_GFC_MAX_BONES : n;
}

// Half of the integrator step the next tick will take (semi-implicit Euler, as the engine does it):
// vel' = vel + acc << 12 (when enabled), accum += vel' >> 1; then the outputs exactly as the
// integrator derives them (outAngle = HIWORD(accumRot), then the bone handler for outPos).
static void ff8_bgate_gfc_step_bones(uint8_t *bones, int m, bool frozen)
{
	uint32_t *handlers = (uint32_t *)ff8_bgate_gfc_modules[m].bone_table;
	uint8_t *rt = FF8_BGATE_GFC_RT;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_BONEORDER[i] != 0xFF; i++)
	{
		uint8_t e = FF8_BGATE_GFC_BONEORDER[i];
		if (frozen && !(e & 0x80)) continue; // same freeze test as the integrator
		int id = e & 0x7F;
		if (id >= FF8_BGATE_GFC_MAX_BONES) continue;
		uint8_t *b = bones + id * 0x100;
		uint8_t flags = b[0x1A];
		int32_t *accRot = (int32_t *)(b + 0x50), *accPos = (int32_t *)(b + 0x5C);
		int32_t *velRot = (int32_t *)(b + 0x68), *velPos = (int32_t *)(b + 0x74);
		int16_t *aRot = (int16_t *)(b + 0x80), *aPos = (int16_t *)(b + 0x86);
		for (int k = 0; k < 3; k++)
		{
			int32_t vr = velRot[k] + ((flags & 1) ? ((int32_t)aRot[k] << 12) : 0);
			int32_t vp = velPos[k] + ((flags & 8) ? ((int32_t)aPos[k] << 12) : 0);
			accRot[k] += vr >> 1;
			accPos[k] += vp >> 1;
		}
		for (int k = 0; k < 3; k++) *(int16_t *)(b + 0x8C + 2 * k) = (int16_t)(accRot[k] >> 16);
		uint32_t h = b[0x18];
		if (h < 12 && handlers[h])
		{
			rt[0x42] = (uint8_t)id;
			FF8_BGATE_GFC_CURBONE = b;
			((void (__cdecl *)())handlers[h])();
		}
		ff8_bgate_gfc_stats.bones_stepped++;
	}
}

// Particle pools (draw handler 6): live particles advanced by half of the handler's own step
// (vel += acc; pos += 16*vel; rgb += drgb) - display only, the pools are restored after the draw.
static uint32_t ff8_bgate_gfc_pools_save(uint8_t *bones, int n, bool restore)
{
	uint32_t used = 0;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF; i++)
	{
		int id = FF8_BGATE_GFC_DRAWORDER[i] & 0x7F;
		if (id >= n) continue;
		uint8_t *b = bones + id * 0x100;
		if (b[0x1C] != 6) continue;
		uint8_t *pool = *(uint8_t **)(b + 0xB8);
		if (!pool) continue;
		uint32_t cnt = *(uint16_t *)pool;
		uint32_t bytes = 16 + 80 * cnt;
		if (cnt == 0 || cnt > 2048 || used + bytes > FF8_BGATE_GFC_POOL_MAX) continue;
		if (restore) memcpy(pool, ff8_bgate_gfc_save_pools + used, bytes);
		else
		{
			memcpy(ff8_bgate_gfc_save_pools + used, pool, bytes);
			if (ff8_bgate_fx_mode >= 2)
				for (uint32_t p = 0; p < cnt; p++)
				{
					uint8_t *q = pool + 16 + 80 * p;
					if (!*(uint32_t *)(q + 4) || !*(uint32_t *)(q + 0x30)) continue; // inactive / invisible
					int16_t *vel = (int16_t *)(q + 0x20), *acc = (int16_t *)(q + 0x28);
					int32_t *ch = (int32_t *)(q + 0x10);
					for (int c = 0; c < 4; c++)
					{
						int32_t v = (int32_t)vel[c] + acc[c];
						ch[c] += 8 * v;
					}
					for (int c = 0; c < 3; c++)
					{
						int v = q[0x38 + c] + ((int8_t)q[0x3C + c]) / 2;
						q[0x38 + c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
					}
					ff8_bgate_gfc_stats.particles_stepped++;
				}
		}
		used += bytes;
	}
	return used;
}

static uint32_t ff8_bgate_gfc_fault_addr = 0, ff8_bgate_gfc_fault_code = 0;

// TEMP diagnostics: copies of the mesh headers (16 dwords) of every mesh bone, compared after
// each phase of the held frame to find who rewrites a mesh
#define FF8_BGATE_GFC_WATCH_MAX 96
static struct { int id; uint32_t *mesh; uint32_t hdr[16]; } ff8_bgate_gfc_watch[FF8_BGATE_GFC_WATCH_MAX];
static int ff8_bgate_gfc_watch_n = 0;

static void ff8_bgate_gfc_watch_take(uint8_t *bones, int n)
{
	ff8_bgate_gfc_watch_n = 0;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF && ff8_bgate_gfc_watch_n < FF8_BGATE_GFC_WATCH_MAX; i++)
	{
		int id = FF8_BGATE_GFC_DRAWORDER[i] & 0x7F;
		if (id >= n) continue;
		uint8_t *b = bones + id * 0x100;
		int h = b[0x1C];
		if (h != 1 && h != 2 && h != 7 && h != 9 && h != 27) continue;
		uint32_t *mesh = *(uint32_t **)(b + 0xD8);
		if (!mesh || IsBadReadPtr(mesh, 64)) continue;
		auto &w = ff8_bgate_gfc_watch[ff8_bgate_gfc_watch_n++];
		w.id = id; w.mesh = mesh;
		memcpy(w.hdr, mesh, 64);
	}
}

static void ff8_bgate_gfc_watch_check(const char *phase, int m)
{
	for (int i = 0; i < ff8_bgate_gfc_watch_n; i++)
	{
		auto &w = ff8_bgate_gfc_watch[i];
		if (IsBadReadPtr(w.mesh, 64) || memcmp(w.hdr, w.mesh, 64) == 0) continue;
		char l[400]; int o = 0;
		for (int k = 0; k < 16 && o < 360; k++)
			if (w.hdr[k] != w.mesh[k]) o += sprintf(l + o, " [%d]%08X->%08X", k, w.hdr[k], w.mesh[k]);
		l[o] = 0;
		uint8_t *cx = FF8_BGATE_GFC_CTX;
		ffnx_info("30fps gf: %s MESH HEADER CHANGED after %s (tick %u): bone %d mesh=%p%s | ctx+74=%08X +7C=%08X +D4=%08X +D8=%08X frame_pkt=%08X\n", ff8_bgate_gfc_modules[m].name, phase, ff8_bgate_gfc_tick, w.id, w.mesh, l,
			*(uint32_t *)(cx + 0x74), *(uint32_t *)(cx + 0x7C), *(uint32_t *)(cx + 0xD4), *(uint32_t *)(cx + 0xD8), *(uint32_t *)0x1D8E054);
		memcpy(w.hdr, w.mesh, 64);
	}
}


static int ff8_bgate_gfc_fault_filter(EXCEPTION_POINTERS *ep)
{
	ff8_bgate_gfc_fault_code = ep->ExceptionRecord->ExceptionCode;
	ff8_bgate_gfc_fault_addr = (uint32_t)ep->ExceptionRecord->ExceptionAddress;
	return EXCEPTION_EXECUTE_HANDLER;
}

// the state-changing part of a held frame (midpoint, replay, draw) - a fault here must not take the
// game down: everything is restored by the caller and the frame is simply incomplete
static bool ff8_bgate_gfc_held_body(int m, uint8_t *bones, int n, bool frozen, uint8_t *rt, int (__cdecl *orig)())
{
	__try
	{
		ff8_bgate_gfc_watch_take(bones, n);
		if (ff8_bgate_fx_mode >= 2) ff8_bgate_gfc_step_bones(bones, m, frozen);
		ff8_bgate_gfc_watch_check("bone half-step", m);
		ff8_bgate_gfc_pools_save(bones, n, false);
		ff8_bgate_gfc_watch_check("particle half-step", m);
		ff8_bgate_gfc_models_snapshot(bones, n, m, frozen);
		ff8_bgate_gfc_watch_check("model look-ahead", m);
		// what AdvanceAnimChannels_Neg does before the first opcode of a tick: the replayed
		// opcodes (0x71 links shadow packets through it) must not see a stale cursor
		*(uint32_t *)(rt + 0x4C) = *(uint32_t *)(rt + 0x38);
		*(uint16_t *)(rt + 0x3E) = 0;
		if (ff8_bgate_fx_mode >= 1 && !ff8_bgate_gfc_log_overflow)
			for (int i = 0; i < ff8_bgate_gfc_log_n; i++)
			{
				const ff8_bgate_gfc_logent &e = ff8_bgate_gfc_log[i];
				if (ff8_bgate_gfc_ops[e.op_idx] == 0x39 && (e.opword >> 12) != 0) continue; // only the camera sub-op is pure
				if (ff8_bgate_gfc_ops[e.op_idx] == 0x71) continue; // TEMP experiment: entity shadow not replayed (Bahamut corruption)
				FF8_BGATE_GFC_CURBONE = e.bone;
				FF8_BGATE_GFC_CURSOR = e.cursor;
				*(uint16_t *)(rt + 0x4A) = e.opword;
				((int (__cdecl *)())e.fn)();
				ff8_bgate_gfc_stats.replayed_ops++;
			}
		ff8_bgate_gfc_watch_check("pure-op replay", m);
		rt[0x45] = 0xFF; // boneSkipFlag: draw handlers 3/5/6/37... draw without advancing
		unreplace_function(ff8_bgate_gfc_build_ri[m]);
		orig();
		rereplace_function(ff8_bgate_gfc_build_ri[m]);
		ff8_bgate_gfc_watch_check("held draw", m);
		return true;
	}
	__except (ff8_bgate_gfc_fault_filter(GetExceptionInformation()))
	{
		rereplace_function(ff8_bgate_gfc_build_ri[m]);
		return false;
	}
}

static void ff8_bgate_gfc_fault_report(int m, uint8_t *bones, int n, uint8_t *rt)
{
	int id = rt[0x42];
	uint8_t *b = bones + (id < n ? id : 0) * 0x100;
	char ops[200]; int o = 0;
	for (int i = 0; i < ff8_bgate_gfc_log_n && o < 180; i++)
		if (ff8_bgate_gfc_log[i].bone == b) o += sprintf(ops + o, " %X", ff8_bgate_gfc_ops[ff8_bgate_gfc_log[i].op_idx]);
	ops[o] = 0;
	char dl[300]; int d = 0;
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF && d < 280; i++)
	{
		int bid = FF8_BGATE_GFC_DRAWORDER[i] & 0x7F;
		d += sprintf(dl + d, " %d:h%d", bid, bid < n ? bones[bid * 0x100 + 0x1C] : -1);
	}
	dl[d] = 0;
	ffnx_info("30fps gf: %s HELD-FRAME FAULT %08X at %08X (tick %u, fault %u): cur bone %d id=%u draw=%d bonehdl=%d parent=%u +B8=%08X +BC=%08X +D8=%08X flags4A=%04X | ops replayed for it:%s | draw list:%s\n",
		ff8_bgate_gfc_modules[m].name, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr, ff8_bgate_gfc_tick, ff8_bgate_gfc_faults, id,
		*(uint16_t *)(b + 0x12), b[0x1C], b[0x18], *(uint16_t *)(b + 0x9C), *(uint32_t *)(b + 0xB8), *(uint32_t *)(b + 0xBC), *(uint32_t *)(b + 0xD8), *(uint16_t *)(b + 0x4A), ops, dl);
}

static uint32_t ff8_bgate_gfc_last_arena_len = 0;

static bool ff8_bgate_gfc_real_body(int m, int (__cdecl *orig)(), int *r)
{
	__try
	{
		unreplace_function(ff8_bgate_gfc_build_ri[m]);
		*r = orig();
		rereplace_function(ff8_bgate_gfc_build_ri[m]);
		return true;
	}
	__except (ff8_bgate_gfc_fault_filter(GetExceptionInformation()))
	{
		rereplace_function(ff8_bgate_gfc_build_ri[m]);
		return false;
	}
}

// TEMP diagnostics: before a real tick draws, check every mesh bone's mesh pointer
static void ff8_bgate_gfc_validate(int m, uint8_t *bones, int n, uint8_t *ctx)
{
	for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF; i++)
	{
		int id = FF8_BGATE_GFC_DRAWORDER[i] & 0x7F;
		if (id >= n) continue;
		uint8_t *b = bones + id * 0x100;
		int h = b[0x1C];
		if (h != 1 && h != 2 && h != 7 && h != 9 && h != 27) continue;
		uint32_t *mesh = *(uint32_t **)(b + 0xD8);
		bool bad = !mesh || IsBadReadPtr(mesh, 0x24);
		uint32_t vofs = 0, vcnt = 0;
		if (!bad)
		{
			vofs = mesh[5]; vcnt = mesh[6];
			bad = vcnt > 4096 || IsBadReadPtr((uint8_t *)mesh + vofs, 8 * (vcnt ? vcnt : 1));
		}
		if (bad)
		{
			ffnx_info("30fps gf: %s BAD MESH before real tick %u: bone %d id=%u draw=%d parent=%u mesh=%p vofs=%u vcnt=%u +B8=%08X +BC=%08X flags4A=%04X drawFlags=%02X | ctx+74=%08X arena_base=%08X last_len=%u seq=%u\n",
				ff8_bgate_gfc_modules[m].name, ff8_bgate_gfc_tick, id, *(uint16_t *)(b + 0x12), h, *(uint16_t *)(b + 0x9C), mesh, vofs, vcnt,
				*(uint32_t *)(b + 0xB8), *(uint32_t *)(b + 0xBC), *(uint16_t *)(b + 0x4A), b[0x4C],
				*(uint32_t *)(ctx + 0x74), ff8_bgate_gfc_arena_base, ff8_bgate_gfc_last_arena_len, *(uint16_t *)(ctx + 0x32));
		}
	}
}

static int ff8_bgate_gfc_build_draw(int m)
{
	int (__cdecl *orig)() = (int (__cdecl *)())ff8_bgate_gfc_modules[m].build_draw;
	uint8_t *ctx = FF8_BGATE_GFC_CTX, *rt = FF8_BGATE_GFC_RT;
	uint8_t *bones = *(uint8_t **)(ctx + 0x90);
	int n = ff8_bgate_gfc_bone_count();
	int r;

	if (!ff8_bgate_gfc_redraw)
	{
		if (ff8_bgate_gfc_ctx != ctx || ff8_bgate_gfc_arena_base == 0)
		{
			// first draw of this summon: nothing lazy has been allocated from the bump arena yet
			ff8_bgate_gfc_reset();
			ff8_bgate_gfc_ctx = ctx;
			ff8_bgate_gfc_arena_base = *(uint32_t *)(ctx + 0x74);
			ffnx_info("30fps gf: %s cinematic summon, ctx=%p bones=%d arena=%08X\n", ff8_bgate_gfc_modules[m].name, ctx, n, ff8_bgate_gfc_arena_base);
		}
		ff8_bgate_gfc_watch_check("the real tick's VM", m);
		ff8_bgate_gfc_validate(m, bones, n, ctx);
		if (!ff8_bgate_gfc_real_body(m, orig, &r))
		{
			ff8_bgate_gfc_faults = 99; // this summon is off the dedicated path from now on
			ffnx_info("30fps gf: %s REAL-TICK FAULT after a held frame: see the bone below\n", ff8_bgate_gfc_modules[m].name);
			ff8_bgate_gfc_fault_report(m, bones, n, rt);
			r = 0;
		}
		ff8_bgate_gfc_tick++;
		for (int i = 0; i < 256 && FF8_BGATE_GFC_DRAWORDER[i] != 0xFF; i++)
		{
			int id = bones[(FF8_BGATE_GFC_DRAWORDER[i] & 0x7F) * 0x100 + 0x1C];
			if (id < 73) ff8_bgate_gfc_stats.handlers[id] = 1;
		}
		return r;
	}

	// ---- held frame: second draw, at the exact midpoint of the coming tick ----
	if (!ff8_bgate_gfc_save_bones) ff8_bgate_gfc_save_bones = (uint8_t *)malloc(FF8_BGATE_GFC_MAX_BONES * 0x100);
	if (!ff8_bgate_gfc_save_arena) ff8_bgate_gfc_save_arena = (uint8_t *)malloc(FF8_BGATE_GFC_ARENA_MAX);
	if (!ff8_bgate_gfc_save_pools) ff8_bgate_gfc_save_pools = (uint8_t *)malloc(FF8_BGATE_GFC_POOL_MAX);
	uint32_t arena_top = *(uint32_t *)(ctx + 0x74);
	uint32_t arena_len = (arena_top > ff8_bgate_gfc_arena_base && arena_top - ff8_bgate_gfc_arena_base <= FF8_BGATE_GFC_ARENA_MAX) ? arena_top - ff8_bgate_gfc_arena_base : 0;
	uint8_t rt_save[0x50];
	memcpy(rt_save, rt, sizeof(rt_save));
	uint8_t *curbone_save = FF8_BGATE_GFC_CURBONE;
	uint32_t cursor_save = FF8_BGATE_GFC_CURSOR;
	bool frozen = rt[0x45] != 0;
	memcpy(ff8_bgate_gfc_save_bones, bones, n * 0x100);
	memcpy(ff8_bgate_gfc_save_nodes, (void *)FF8_BGATE_GFC_NODES_BEGIN, sizeof(ff8_bgate_gfc_save_nodes));
	if (arena_len) memcpy(ff8_bgate_gfc_save_arena, (void *)ff8_bgate_gfc_arena_base, arena_len);
	ff8_bgate_gfc_stats.arena_bytes = arena_len;
	ff8_bgate_gfc_last_arena_len = arena_len;
	// node 0 (the camera) has just been refreshed by SequenceTick's preamble from the smoothed view

	// F9 sub-modes: 0 = camera only, 1 = + node/light/camera replay, >= 2 = + midpoint state
	r = 0;
	uint32_t pool_save = *(uint32_t *)0x1D999C4;
	if (!ff8_bgate_gfc_held_body(m, bones, n, frozen, rt, orig))
	{
		ff8_bgate_gfc_faults++;
		ff8_bgate_gfc_fault_report(m, bones, n, rt);
	}
	*(uint32_t *)0x1D999C4 = pool_save;

	ff8_bgate_gfc_models_restore();
	ff8_bgate_gfc_pools_save(bones, n, true);
	if (arena_len) memcpy((void *)ff8_bgate_gfc_arena_base, ff8_bgate_gfc_save_arena, arena_len);
	memcpy((void *)FF8_BGATE_GFC_NODES_BEGIN, ff8_bgate_gfc_save_nodes, sizeof(ff8_bgate_gfc_save_nodes));
	memcpy(bones, ff8_bgate_gfc_save_bones, n * 0x100);
	FF8_BGATE_GFC_CURSOR = cursor_save;
	FF8_BGATE_GFC_CURBONE = curbone_save;
	memcpy(rt, rt_save, sizeof(rt_save));
	ff8_bgate_gfc_watch_check("restores", m);
	if (ff8_bgate_gfc_tick >= 505 && ff8_bgate_gfc_tick <= 517)
		ffnx_info("30fps gf: held end tick %u: ctx+74=%08X +7C=%08X +D4=%08X +D8=%08X frame_pkt=%08X rt+38=%08X rt+4C=%08X\n", ff8_bgate_gfc_tick,
			*(uint32_t *)(ctx + 0x74), *(uint32_t *)(ctx + 0x7C), *(uint32_t *)(ctx + 0xD4), *(uint32_t *)(ctx + 0xD8), *(uint32_t *)0x1D8E054, *(uint32_t *)(rt + 0x38), *(uint32_t *)(rt + 0x4C));
	ff8_bgate_gfc_stats.redraws++;
	return r;
}

template <int M> static int __cdecl ff8_bgate_gfc_build_hook() { return ff8_bgate_gfc_build_draw(M); }

// held frame of the effect-tree gate: true = the summon drew itself, no 2D replay wanted
static bool ff8_bgate_gfc_held_frame(void *queue, int (__cdecl *orig)(void *))
{
	if (!ff8_bgate_gfc_enabled || ff8_bgate_gfc_module_of_effect() < 0) return false;
	if (!ff8_bgate_gfc_ctx || ff8_bgate_gfc_tick < 1 || ff8_bgate_gfc_faults >= 3) return false;
	uint32_t *flags = (uint32_t *)0x1D96A9C; // battle_to_update_flags: bit 0 = effects paused
	uint32_t flags_save = *flags;
	uint16_t seq_save = *(uint16_t *)(ff8_bgate_gfc_ctx + 0x32);
	uint32_t *seed = (uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14); // _getptd()->_holdrand
	uint32_t seed_save = *seed;
	*flags |= 1;
	ff8_bgate_gfc_redraw = true;
	orig(queue);
	ff8_bgate_gfc_redraw = false;
	*flags = (*flags & ~1u) | (flags_save & 1);
	*seed = seed_save;
	*(uint16_t *)(ff8_bgate_gfc_ctx + 0x32) = seq_save;
	return true;
}

template <int M> static void ff8_bgate_gfc_install_module()
{
	static int (__cdecl *const wraps[FF8_BGATE_GFC_OPS])() = {
		&ff8_bgate_gfc_op_wrap<M, 0>, &ff8_bgate_gfc_op_wrap<M, 1>, &ff8_bgate_gfc_op_wrap<M, 2>, &ff8_bgate_gfc_op_wrap<M, 3>, &ff8_bgate_gfc_op_wrap<M, 4>,
		&ff8_bgate_gfc_op_wrap<M, 5>, &ff8_bgate_gfc_op_wrap<M, 6>, &ff8_bgate_gfc_op_wrap<M, 7>, &ff8_bgate_gfc_op_wrap<M, 8>, &ff8_bgate_gfc_op_wrap<M, 9>,
		&ff8_bgate_gfc_op_wrap<M, 10>, &ff8_bgate_gfc_op_wrap<M, 11>, &ff8_bgate_gfc_op_wrap<M, 12>, &ff8_bgate_gfc_op_wrap<M, 13>, &ff8_bgate_gfc_op_wrap<M, 14>,
		&ff8_bgate_gfc_op_wrap<M, 15>, &ff8_bgate_gfc_op_wrap<M, 16>, &ff8_bgate_gfc_op_wrap<M, 17>, &ff8_bgate_gfc_op_wrap<M, 18>, &ff8_bgate_gfc_op_wrap<M, 19> };
	const ff8_bgate_gfc_module &g = ff8_bgate_gfc_modules[M];
	for (int j = 0; j < FF8_BGATE_GFC_OPS; j++)
	{
		uint32_t slot = FF8_BGATE_GFC_VM_TABLE(g.draw_table) + 4 * ff8_bgate_gfc_ops[j];
		ff8_bgate_gfc_orig_op[M][j] = *(uint32_t *)slot;
		if (ff8_bgate_gfc_orig_op[M][j]) patch_code_dword(slot, (DWORD)wraps[j]);
	}
	ff8_bgate_gfc_build_ri[M] = replace_function(g.build_draw, (void *)&ff8_bgate_gfc_build_hook<M>);
}

static void ff8_bgate_gfc_install()
{
	ff8_bgate_gfc_install_module<0>(); ff8_bgate_gfc_install_module<1>(); ff8_bgate_gfc_install_module<2>();
	ff8_bgate_gfc_install_module<3>(); ff8_bgate_gfc_install_module<4>(); ff8_bgate_gfc_install_module<5>();
	ff8_bgate_gfc_install_module<6>();
}

// --- dedicated held-frame path for the "timeline" GF summons (family B) ---
// Shiva 185, Cactuar 199, Odin 187 / 326, Doomtrain 191, Gilgamesh 327-330 (study:
// gf_study/gf_inventory_timeline.md). Their effect queue holds a tiny master task (runs a
// sub-queue, ++counter at node+12) and the sub-queue holds the timeline, the creature and the
// particle tasks. Every one of those tasks tests battle_to_update_flags & 0x201: it DRAWS from
// its current state, then returns before its events, spawns, rand(), sound and state advance
// when the bit is set. So a held frame = the effect queue called once more with bit 0 set:
// the whole summon is redrawn (with the smoothed battle camera) and nothing advances, except
// the master counters (restored) and the CRT seed (restored).
// The creature is a standard battle entity (BattleAnimHeader at E+96, BattleAnimCmd at E+108,
// keyframes read by Battle_ReadAnimation): its pose is half-stepped by look-ahead, exactly as
// the cinematic engine's embedded models.
struct ff8_bgate_tlb_module { int effect_id; uint32_t creature, advance_fn; const char *name; };
static const ff8_bgate_tlb_module ff8_bgate_tlb_modules[] = {
	{ 185, 0x22BD018, 0x5C4EA0, "Shiva" },
	{ 199, 0x225A838, 0x5A8FD0, "Cactuar" },
	{ 187, 0x24FD8D8, 0x64A260, "Odin" },
	{ 326, 0, 0, "Odin (Zantetsuken Reverse)" },
	{ 191, 0, 0, "Doomtrain" },
	{ 327, 0, 0, "Gilgamesh" }, { 328, 0, 0, "Gilgamesh" }, { 329, 0, 0, "Gilgamesh" }, { 330, 0, 0, "Gilgamesh" },
};
#define FF8_BGATE_TLB_MODULES 9
#define FF8_BGATE_TLB_SKEL_MAX (64 * 1024)

static int ff8_bgate_tlb_cur = -1;        // running summon (-1 = none)
static void *ff8_bgate_tlb_queue = nullptr;
static uint32_t ff8_bgate_tlb_tick = 0, ff8_bgate_tlb_redraws = 0, ff8_bgate_tlb_model_steps = 0, ff8_bgate_tlb_faults = 0;
static uint8_t *ff8_bgate_tlb_skel_save = nullptr;
static uint8_t *ff8_bgate_tlb_skel = nullptr; // skeleton section being held (null = none)
static uint32_t ff8_bgate_tlb_skel_size = 0;
static uint8_t ff8_bgate_tlb_cmd_save[8];

static int ff8_bgate_tlb_module_of_effect()
{
	int id = *(int *)0x1D99A68 + 1;
	for (int m = 0; m < FF8_BGATE_TLB_MODULES; m++)
		if (ff8_bgate_tlb_modules[m].effect_id == id) return m;
	return -1;
}

static void ff8_bgate_tlb_reset()
{
	ff8_bgate_tlb_cur = -1;
	ff8_bgate_tlb_queue = nullptr;
	ff8_bgate_tlb_tick = ff8_bgate_tlb_redraws = ff8_bgate_tlb_model_steps = ff8_bgate_tlb_faults = 0;
	ff8_bgate_tlb_skel = nullptr;
}

static bool ff8_bgate_tlb_call_guarded(uint32_t fn, void *arg)
{
	__try
	{
		((void (__cdecl *)(void *))fn)(arg);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

// Field_Alloc/Field_Free are a bump stack (GLOBAL_MEMORY_POOL 0x1D999C4); a fault inside game code
// leaves it pushed, and every later Field_Alloc then runs past the region: heap corruption. The
// pool pointer is restored after every held pass, fault or not.
static bool ff8_bgate_tlb_queue_guarded(int (__cdecl *orig)(void *), void *queue)
{
	uint32_t pool = *(uint32_t *)0x1D999C4;
	bool ok;
	__try
	{
		orig(queue);
		ok = true;
	}
	__except (ff8_bgate_gfc_fault_filter(GetExceptionInformation()))
	{
		ok = false;
	}
	*(uint32_t *)0x1D999C4 = pool;
	return ok;
}

// pose(skel) := midpoint of cur (snapshot) and skel (next), root + rotations + scales
static void ff8_bgate_pose_write_mid(uint8_t *skel, const uint8_t *cur)
{
	int nb = skel[0];
	bool scale = cur[1] != 0;
	for (int k = 0; k < 3; k++)
	{
		int16_t c = *(int16_t *)(cur + 8 + 2 * k), nx = *(int16_t *)(skel + 8 + 2 * k);
		*(int16_t *)(skel + 8 + 2 * k) = (int16_t)(c + (nx - c) / 2);
	}
	for (int j = 0; j < nb; j++)
		for (int k = 0; k < 3; k++)
		{
			uint8_t *rc = skel + 16 + 48 * j + 4 + 2 * k;
			*(int16_t *)rc = ff8_bgate_gfc_mid_angle(*(int16_t *)(cur + 16 + 48 * j + 4 + 2 * k), *(int16_t *)rc);
			if (scale)
			{
				uint8_t *sc = skel + 16 + 48 * j + 10 + 2 * k;
				int16_t c = *(int16_t *)(cur + 16 + 48 * j + 10 + 2 * k);
				*(int16_t *)sc = (int16_t)(c + (*(int16_t *)sc - c) / 2);
			}
		}
	// everything else of the record (matrices) is rebuilt by the draw from this pose
	for (int j = 0; j < nb; j++)
		memcpy(skel + 16 + 48 * j + 16, cur + 16 + 48 * j + 16, 32);
}

// creature half-step: snapshot, read the next keyframe as the next tick will, midpoint, restore later
static uint8_t *ff8_bgate_tlb_E = nullptr; // creature block being held

static void ff8_bgate_tlb_creature_halfstep(uint32_t creature, uint32_t advance_fn)
{
	ff8_bgate_tlb_skel = nullptr;
	if (!creature || !advance_fn || ff8_bgate_fx_mode < 2) return;
	uint8_t *E = (uint8_t *)creature;
	ff8_bgate_tlb_E = E;
	uint8_t *cmd = E + 108;
	static uint8_t *logged_for = nullptr;
	#define FF8_BGATE_TLB_SKIP(why) do { if (logged_for != E) { logged_for = E; ffnx_info("30fps tl: creature half-step skipped: %s (E=%p cmd=%02X %02X .. %02X %02X com=%p)\n", why, E, cmd[0], cmd[1], cmd[6], cmd[7], *(void **)(E + 100)); } return; } while (0)
	if (cmd[7] == 0) FF8_BGATE_TLB_SKIP("no animation set");
	uint8_t *com = *(uint8_t **)(E + 100);
	if (!com || IsBadReadPtr(com, 4)) FF8_BGATE_TLB_SKIP("bad model data pointer");
	uint8_t *skel = *(uint8_t **)com;
	if (!skel || IsBadReadPtr(skel, 16)) FF8_BGATE_TLB_SKIP("bad skeleton pointer");
	uint32_t size = 16 + 48 * skel[0];
	if (size > FF8_BGATE_TLB_SKEL_MAX || IsBadReadPtr(skel, size)) FF8_BGATE_TLB_SKIP("skeleton size");
	#undef FF8_BGATE_TLB_SKIP
	logged_for = nullptr;
	if (!ff8_bgate_tlb_skel_save) ff8_bgate_tlb_skel_save = (uint8_t *)malloc(FF8_BGATE_TLB_SKEL_MAX);
	if (!ff8_bgate_tlb_skel_save) return;
	memcpy(ff8_bgate_tlb_skel_save, skel, size);
	memcpy(ff8_bgate_tlb_cmd_save, cmd, 8);
	ff8_bgate_tlb_skel = skel;
	ff8_bgate_tlb_skel_size = size;
	if (!ff8_bgate_tlb_call_guarded(advance_fn, E))
	{
		ff8_bgate_tlb_faults++;
		memcpy(skel, ff8_bgate_tlb_skel_save, size);
		memcpy(cmd, ff8_bgate_tlb_cmd_save, 8);
		return;
	}
	// next pose is in the skeleton, current in the snapshot; the cmd goes back to current now
	memcpy(cmd, ff8_bgate_tlb_cmd_save, 8);
	ff8_bgate_pose_write_mid(skel, ff8_bgate_tlb_skel_save);
	ff8_bgate_tlb_model_steps++;
}

static void ff8_bgate_tlb_creature_restore()
{
	if (!ff8_bgate_tlb_skel) return;
	memcpy(ff8_bgate_tlb_skel, ff8_bgate_tlb_skel_save, ff8_bgate_tlb_skel_size);
	memcpy(ff8_bgate_tlb_E + 108, ff8_bgate_tlb_cmd_save, 8);
	ff8_bgate_tlb_skel = nullptr;
}

// real tick of the effect queue: track the running summon
static void ff8_bgate_tlb_real_tick(void *queue, int r)
{
	int m = ff8_bgate_tlb_module_of_effect();
	if (m < 0) return;
	if (ff8_bgate_tlb_cur != m || ff8_bgate_tlb_queue != queue)
	{
		ff8_bgate_tlb_reset();
		ff8_bgate_tlb_cur = m;
		ff8_bgate_tlb_queue = queue;
		ffnx_info("30fps tl: %s timeline summon, queue=%p\n", ff8_bgate_tlb_modules[m].name, queue);
	}
	ff8_bgate_tlb_tick++;
	if (r == 0)
	{
		ffnx_info("30fps tl: %s finished, real ticks=%u held redraws=%u creature half-steps=%u faults=%u\n",
			ff8_bgate_tlb_modules[m].name, ff8_bgate_tlb_tick, ff8_bgate_tlb_redraws, ff8_bgate_tlb_model_steps, ff8_bgate_tlb_faults);
		ff8_bgate_tlb_reset();
	}
}

// held frame: true = the summon drew itself, no 2D replay wanted
static bool ff8_bgate_tlb_held_frame(void *queue, int (__cdecl *orig)(void *))
{
	if (!ff8_bgate_gfc_enabled) return false;
	int m = ff8_bgate_tlb_cur;
	if (m < 0 || queue != ff8_bgate_tlb_queue || ff8_bgate_tlb_tick < 1 || ff8_bgate_tlb_faults >= 3) return false;
	const ff8_bgate_tlb_module &g = ff8_bgate_tlb_modules[m];
	uint32_t *flags = (uint32_t *)0x1D96A9C;
	uint32_t flags_save = *flags;
	uint32_t *seed = (uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14); // _getptd()->_holdrand
	uint32_t seed_save = *seed;
	// master task node = head of the effect queue; its counter (node+12) advances even when paused
	uint8_t *master = *(uint8_t **)queue;
	uint16_t counter_save = master ? *(uint16_t *)(master + 12) : 0;

	ff8_bgate_tlb_creature_halfstep(g.creature, g.advance_fn);
	*flags |= 1;
	bool ok = ff8_bgate_tlb_queue_guarded(orig, queue);
	*flags = (*flags & ~1u) | (flags_save & 1);
	*seed = seed_save;
	if (master) *(uint16_t *)(master + 12) = counter_save;
	ff8_bgate_tlb_creature_restore();
	if (!ok)
	{
		ff8_bgate_tlb_faults++;
		ffnx_info("30fps tl: %s HELD-FRAME FAULT %08X at %08X (tick %u, fault %u)\n", g.name, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr, ff8_bgate_tlb_tick, ff8_bgate_tlb_faults);
	}
	ff8_bgate_tlb_redraws++;
	return true;
}

// --- dedicated held-frame path for the "timeline" GF summons (family A) ---
// Quezacotl 116 (precedent), later Diablos 325, Carbuncle 278, Pandemona 291, Phoenix 140.
// Each has a debug PAUSE global the retail game never sets: when non-zero the master keeps its
// counter, does not spawn, the creature's keyframe advance and the relative camera moves are
// skipped and every particle task draws then returns. What the pause does NOT cover is the
// creature task itself: its counter still increments and its frame-keyed one-shots (sounds,
// streamed loads, pool re-inits, task spawns, damage application) fire again for the same
// frame. So a held pass sets the pause global AND a side-effect shield: during the pass those
// engine entry points are no-ops (AddTaskToQueue hands out a scratch node), then the creature
// counter, the master's arena toggle, the CRT seed and the effect's own LCG seed are restored.
// The creature (standard battle entity in the effect's model buffer) gets the look-ahead
// half-step of the timeline-B path.
struct ff8_bgate_tla_module {
	int effect_id; uint32_t pause_global, model_buffer_global; int creature_off, creature_counter_off;
	uint32_t advance_fn, seed_global; int master_toggle_off, master_toggle_size; const char *name;
};
static const ff8_bgate_tla_module ff8_bgate_tla_modules[] = {
	{ 116, 0x25216DC, 0x25217AC, 40, 12, 0x6FBDB0, 0x25217A0, 16, 4, "Quezacotl" },
};
#define FF8_BGATE_TLA_MODULES 1

static int ff8_bgate_tla_cur = -1;
static void *ff8_bgate_tla_queue = nullptr;
static uint32_t ff8_bgate_tla_tick = 0, ff8_bgate_tla_redraws = 0, ff8_bgate_tla_model_steps = 0, ff8_bgate_tla_faults = 0;

// side-effect shield: engine entry points that must not fire twice per tick. They are patched
// only for the duration of a held pass (5-byte jump to a stub, original bytes put back right
// after): no permanent hook, nothing forwarded, nothing left in FFNx's replacement table.
static uint8_t ff8_bgate_shield_node[8192];
static int __cdecl ff8_bgate_shield_zero() { return 0; }
static int __cdecl ff8_bgate_shield_one() { return 1; }
static void *__cdecl ff8_bgate_shield_scratch_node() { memset(ff8_bgate_shield_node, 0, sizeof(ff8_bgate_shield_node)); return ff8_bgate_shield_node; }
struct ff8_bgate_shield_site { uint32_t addr; void *stub; uint8_t saved[5]; };
static ff8_bgate_shield_site ff8_bgate_shield_sites[] = {
	{ 0x501330, (void *)ff8_bgate_shield_zero },         // BdPlaySE
	{ 0x5018C0, (void *)ff8_bgate_shield_zero },         // BdPlaySummonStream
	{ 0x501860, (void *)ff8_bgate_shield_zero },         // BdTransSummonStream
	{ 0x4A29A0, (void *)ff8_bgate_shield_zero },         // BdSound_ClaimVoiceSlot
	{ 0x4A2940, (void *)ff8_bgate_shield_zero },         // voice slot release
	{ 0x5341D0, (void *)ff8_bgate_shield_zero },         // streamed file load request
	{ 0x508480, (void *)ff8_bgate_shield_zero },         // BattleFile_CharacterLoad
	{ 0x508300, (void *)ff8_bgate_shield_zero },         // BdLink_InitTaskQueuePool (would wipe live particles)
	{ 0x508360, (void *)ff8_bgate_shield_scratch_node }, // AddTaskToQueue -> scratch node
	{ 0x506BA0, (void *)ff8_bgate_shield_zero },         // ApplyActionResultToTargets
	{ 0x506690, (void *)ff8_bgate_shield_zero },         // ApplyActionResultToTarget
	{ 0x505C00, (void *)ff8_bgate_shield_zero },         // QueueChainTransformation
	{ 0x534270, (void *)ff8_bgate_shield_one },          // loader poll (idle branch resets the queue): "busy" = the task waits as on a real load frame
	{ 0x534210, (void *)ff8_bgate_shield_zero },         // loader pump (pre_LoadBattleFile of the next queued file)
	{ 0x534110, (void *)ff8_bgate_shield_zero },         // loader state init (dword_1DCD6EC/E4) at creature spawn
};
#define FF8_BGATE_SHIELD_SITES 15
static bool ff8_bgate_shield = false;

static void ff8_bgate_shield_set(bool on)
{
	if (on == ff8_bgate_shield) return;
	ff8_bgate_shield = on;
	for (int i = 0; i < FF8_BGATE_SHIELD_SITES; i++)
	{
		ff8_bgate_shield_site &s = ff8_bgate_shield_sites[i];
		uint8_t *code = (uint8_t *)s.addr;
		DWORD old;
		if (!VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &old)) continue;
		if (on)
		{
			memcpy(s.saved, code, 5);
			code[0] = 0xE9;
			*(int32_t *)(code + 1) = (int32_t)((uint8_t *)s.stub - (code + 5));
		}
		else memcpy(code, s.saved, 5);
		VirtualProtect(code, 5, old, &old);
		FlushInstructionCache(GetCurrentProcess(), code, 5);
	}
}

static int ff8_bgate_tla_module_of_effect()
{
	int id = *(int *)0x1D99A68 + 1;
	for (int m = 0; m < FF8_BGATE_TLA_MODULES; m++)
		if (ff8_bgate_tla_modules[m].effect_id == id) return m;
	return -1;
}

static void ff8_bgate_tla_reset()
{
	ff8_bgate_tla_cur = -1;
	ff8_bgate_tla_queue = nullptr;
	ff8_bgate_tla_tick = ff8_bgate_tla_redraws = ff8_bgate_tla_model_steps = ff8_bgate_tla_faults = 0;
	ff8_bgate_shield_set(false);
}

static void ff8_bgate_tla_real_tick(void *queue, int r)
{
	int m = ff8_bgate_tla_module_of_effect();
	if (m < 0) return;
	if (ff8_bgate_tla_cur != m || ff8_bgate_tla_queue != queue)
	{
		ff8_bgate_tla_reset();
		ff8_bgate_tla_cur = m;
		ff8_bgate_tla_queue = queue;
		ffnx_info("30fps tl: %s timeline-A summon, queue=%p\n", ff8_bgate_tla_modules[m].name, queue);
	}
	ff8_bgate_tla_tick++;
	if (r == 0)
	{
		ffnx_info("30fps tl: %s finished, real ticks=%u held redraws=%u creature half-steps=%u faults=%u\n",
			ff8_bgate_tla_modules[m].name, ff8_bgate_tla_tick, ff8_bgate_tla_redraws, ff8_bgate_tla_model_steps, ff8_bgate_tla_faults);
		ff8_bgate_tla_reset();
	}
}

static bool ff8_bgate_tla_held_frame(void *queue, int (__cdecl *orig)(void *))
{
	if (!ff8_bgate_gfc_enabled) return false;
	int m = ff8_bgate_tla_cur;
	if (m < 0 || queue != ff8_bgate_tla_queue || ff8_bgate_tla_tick < 1 || ff8_bgate_tla_faults >= 3) return false;
	const ff8_bgate_tla_module &g = ff8_bgate_tla_modules[m];
	uint8_t *master = *(uint8_t **)queue;
	if (!master) return false;
	uint8_t *buf = *(uint8_t **)g.model_buffer_global;
	bool spawned = master[15] != 0 && buf != nullptr; // "creature spawned" byte of the master node
	uint32_t *seed = (uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14);
	uint32_t seed_save = *seed, gfseed_save = *(uint32_t *)g.seed_global;
	uint16_t master_counter_save = *(uint16_t *)(master + 12);
	uint32_t toggle_save = *(uint32_t *)(master + g.master_toggle_off);
	uint16_t creature_counter_save = spawned ? *(uint16_t *)(buf + g.creature_counter_off) : 0;

	if (spawned) ff8_bgate_tlb_creature_halfstep((uint32_t)(buf + g.creature_off), g.advance_fn);
	*(uint32_t *)g.pause_global = 1;
	ff8_bgate_shield_set(true);
	bool ok = ff8_bgate_tlb_queue_guarded(orig, queue);
	ff8_bgate_shield_set(false);
	*(uint32_t *)g.pause_global = 0;
	*seed = seed_save;
	*(uint32_t *)g.seed_global = gfseed_save;
	*(uint16_t *)(master + 12) = master_counter_save;
	if (g.master_toggle_size == 4) *(uint32_t *)(master + g.master_toggle_off) = toggle_save;
	else *(uint8_t *)(master + g.master_toggle_off) = (uint8_t)toggle_save;
	if (spawned) *(uint16_t *)(buf + g.creature_counter_off) = creature_counter_save;
	ff8_bgate_tlb_creature_restore();
	if (!ok)
	{
		ff8_bgate_tla_faults++;
		ffnx_info("30fps tl: %s HELD-FRAME FAULT %08X at %08X (tick %u, fault %u)\n", g.name, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr, ff8_bgate_tla_tick, ff8_bgate_tla_faults);
	}
	ff8_bgate_tla_redraws++;
	return true;
}

// --- look-ahead packet midpoint for the timeline GF families (ff8_bgate_la_*) ---
// Quezacotl's storm, Shiva's ice, Cactuar's needles...: hundreds of small particle tasks,
// each with its own private layout (position/velocity fields packed into the task node) and
// each drawing then advancing inside one function. Instead of teaching the mod every layout,
// the held frame is built from the EXACT next state: right after every real tick N the
// effect is run one more time (tick N+1) under a full state snapshot - the 1 MB magic
// buffer (creature, particle pools, packet arenas), the module's own globals, the task pools
// it executed, the camera, the GTE registers, the seeds, the ordering table and the draw-list
// cursors - with the side-effect shield up (no sound, no loads, no damage, no task spawn);
// its draws are copied, then everything is put back. The held frame draws every tick-N
// primitive half way to its tick-N+1 counterpart. Pairing is exact: the task queue executor
// is re-implemented while recording so every primitive is tagged with the task node that
// drew it, and a task's k-th primitive at N is paired with its k-th at N+1 (nearest same
// primitive within the task when the counts differ). The creature is included - its mesh
// triangles pair the same way - so no separate pose half-step is needed. When the look-ahead
// is unavailable (fault, overflow) the held frame falls back to the pause-mode paths
// (ff8_bgate_tla_* / ff8_bgate_tlb_*) and then to the generic replay.
// data_lo..data_hi = the module's own globals (FF8 was compiled per source file, so they are
// contiguous; ranges from gf_study/gf_global_ranges.md); extra = one more cell the module
// steps every tick outside that range (0 = none); lookahead = held frames use the look-ahead.
struct ff8_bgate_la_region { uint32_t addr, size; };
// streams = the module's .data buffers that its streamed files are loaded into (and restored
// from the engine's backup at the end): part of the state a tick reads
struct ff8_bgate_la_module { int effect_id; uint32_t data_lo, data_hi, extra, extra_size; bool lookahead; const char *name; const ff8_bgate_la_region *streams; int nstreams; };
static const ff8_bgate_la_region ff8_bgate_la_streams_q116[] = { { 0x1298C68, 0x109FC }, { 0x12A9664, 0x4D9C } };
// Cactuar: pool/table pointer cells 0xCF3564..0xCF3593 (written at the creature's first tick)
static const ff8_bgate_la_region ff8_bgate_la_streams_c199[] = { { 0xCF3564, 0x30 } };
// Shiva: glow-ring node cell, summon data (CharacterLoad 0x221/0x222/0x224 destination, read by
// BdTransSummonStream), TransformCameraByShadowRotation scratch
// Pandemona: texture load state machine cell (outside the module range)
static const ff8_bgate_la_region ff8_bgate_la_streams_p291[] = { { 0x13BBBE8, 4 } };
// Odin: ripple-texture destination in exe data, pool/scratch pointer cells, summon data (loads 0x227/0x229/0x22A)
static const ff8_bgate_la_region ff8_bgate_la_streams_o187[] = { { 0xFA94A8, 0x4000 }, { 0xE41E50, 0xC }, { 0xE41E98, 4 }, { 0x209FAB8, 0x40000 } };
// Doomtrain: pool pointer cells, summon data (loads 0x22E..0x231), shadow-camera scratch
static const ff8_bgate_la_region ff8_bgate_la_streams_d191[] = { { 0xE3C8C0, 0x18 }, { 0x209FAB8, 0x40000 }, { 0x21DFED0, 0x20 } };
static const ff8_bgate_la_region ff8_bgate_la_streams_s185[] = { { 0xD32508, 4 }, { 0x209FAB8, 0x40000 }, { 0x21DFED0, 0x20 } };
static const ff8_bgate_la_module ff8_bgate_la_modules[] = {
	// timeline-A (own pause flag, creature spawned by the master at counter 2)
	{ 116, 0x25216D8, 0x25217D0, 0, 0, true, "Quezacotl", ff8_bgate_la_streams_q116, 2 },
	{ 325, 0x250517C, 0x2505230, 0, 0, false, "Diablos" },
	{ 278, 0x2508110, 0x25081FC, 0, 0, false, "Carbuncle" },
	{ 291, 0x2556258, 0x25562F8, 0, 0, false, "Pandemona", ff8_bgate_la_streams_p291, 1 },
	{ 140, 0x2517AA0, 0x2517B50, 0, 0, false, "Phoenix" },
	{ 338, 0x25561C8, 0x2556254, 0, 0, false, "Moomba" },
	{ 69,  0x2556628, 0x2556F98, 0, 0, false, "Griever" },
	// timeline-B (draw-only mode on battle_to_update_flags bit0, creature spawned by the timeline)
	{ 185, 0x22BC128, 0x22BD108, 0, 0, false, "Shiva", ff8_bgate_la_streams_s185, 3 },
	{ 199, 0x2259950, 0x225A8E4, 0xCF3A68, 4, false, "Cactuar", ff8_bgate_la_streams_c199, 1 }, // + its private rand seed
	{ 187, 0x24FD458, 0x24FE910, 0, 0, false, "Odin", ff8_bgate_la_streams_o187, 4 },
	{ 326, 0x24F0BD0, 0x24F2308, 0, 0, false, "Odin (reverse)" },
	{ 191, 0x24FBD68, 0x24FD458, 0, 0, false, "Doomtrain", ff8_bgate_la_streams_d191, 3 },
	{ 327, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Zantetsuken)" },
	{ 328, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Masamune)" },
	{ 329, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Excalibur)" },
	{ 330, 0x21FF2A8, 0x2201080, 0, 0, false, "Gilgamesh (Excalipoor)" },
};
#define FF8_BGATE_LA_MODULES ((int)(sizeof(ff8_bgate_la_modules) / sizeof(ff8_bgate_la_modules[0])))

// engine state every timeline effect may touch in one tick (saved before the look-ahead,
// restored after it, whatever happened)
static const ff8_bgate_la_region ff8_bgate_la_regions[] = {
	{ 0x20DFAB8, 0x100000 }, // MAGIC_TEXTURE_BUFFER_BASE: creature, particle pools, packet arenas
	{ 0x21DFAB8, 4 },        // its fill cursor (zeroed with it by Magic_ClearMemoryForTex)
	{ 0x1D99A88, 4 },        // MAGIC_TEXTURE_BUFFER_PTR
	{ 0xB8B7F0, 0x20 },      // Battle_Camera_world/LookAt + ReturnView
	{ 0x1D97700, 0xB0 },     // camera shake, roll, blend, BD_LINK_TASK_HEADER_CAMERA, view matrix
	{ 0x1D8E038, 0x20 },     // word_1D8E038 .. battle_texture_data_ptr_1D8E054 (frame packet cursor)
	{ 0x1CA8A10, 0x70 },     // GTE data registers
	{ 0x1CA9230, 0xD0 },     // GTE control registers
	{ 0x1D9898C, 0xDC },     // battle entity array (screen flash, currentBsId visibility bits)
	{ 0x1D972C0, 0x440 },    // BattleEntitySlotData (entity_flags hide bits, Carbuncle's party lift, Odin/Doomtrain target edits)
	{ 0x1D99AB0, 0x20 },     // shared effect light/position struct (Moomba)
	{ 0x1DCD6E0, 0x10 },     // streamed-file loader globals (dword_1DCD6E4/E8/EC)
	{ 0x1D999C4, 4 },        // Field_Alloc bump pointer
	{ 0x2557098, 4 },        // shared effect LCG seed (MAG_106_sub_7059E0)
	{ 0x1CFF6F4, 4 },        // g_Battle_ScreenFeedbackRequest
	{ 0x1D98220, 0x204 },    // VRAM upload queue + count
	{ 0x1CA8828, 4 },        // ssigpu_execution_cur
};
#define FF8_BGATE_LA_REGIONS ((int)(sizeof(ff8_bgate_la_regions) / sizeof(ff8_bgate_la_regions[0])))
#define FF8_BGATE_LA_DATA_MAX 0x8000
#define FF8_BGATE_LA_POOLS_MAX 0x80000
static uint8_t ff8_bgate_la_save_regions[0x100000 + 0x1000];
static uint8_t ff8_bgate_la_save_data[FF8_BGATE_LA_DATA_MAX];
static uint8_t ff8_bgate_la_save_pools[FF8_BGATE_LA_POOLS_MAX];
static uint32_t ff8_bgate_la_save_ot[FF8_BGATE_OT_SPAN_WORDS];

// task pools of the queues the effect ran: header + node storage, saved before the look-ahead
// (a queue first seen inside the look-ahead is saved by the executor before it runs)
struct ff8_bgate_la_pool { ff8_bgate_task_queue *q; ff8_bgate_task_queue hdr; uint32_t off, size; };
static ff8_bgate_la_pool ff8_bgate_la_pools[32];
static int ff8_bgate_la_npools = 0;
static uint32_t ff8_bgate_la_pools_used = 0;
static bool ff8_bgate_la_pools_warned = false;

static void ff8_bgate_la_pool_save(ff8_bgate_task_queue *q)
{
	if (ff8_bgate_la_npools >= 32) return;
	ff8_bgate_la_pool &p = ff8_bgate_la_pools[ff8_bgate_la_npools++];
	p.q = q; p.hdr = *q; p.off = ff8_bgate_la_pools_used; p.size = 0;
	uint32_t size = (uint32_t)q->node_size * q->capacity;
	if (q->pool && size && size <= 0x10000 && ff8_bgate_la_pools_used + size <= FF8_BGATE_LA_POOLS_MAX && !IsBadReadPtr(q->pool, size))
	{
		memcpy(ff8_bgate_la_save_pools + p.off, q->pool, size);
		p.size = size;
		ff8_bgate_la_pools_used += size;
	}
	else if (q->pool && size && !ff8_bgate_la_pools_warned)
	{
		ff8_bgate_la_pools_warned = true;
		ffnx_info("30fps la: pool of queue %p not saved (%u bytes)\n", q, size);
	}
}

static int ff8_bgate_la_cur = -1;
static void *ff8_bgate_la_queue = nullptr;
static uint32_t ff8_bgate_la_ticks = 0, ff8_bgate_la_runs = 0, ff8_bgate_la_faults = 0, ff8_bgate_la_held = 0, ff8_bgate_la_fallback = 0;

static int ff8_bgate_la_module_of_effect()
{
	int id = *(int *)0x1D99A68 + 1;
	for (int m = 0; m < FF8_BGATE_LA_MODULES; m++)
		if (ff8_bgate_la_modules[m].effect_id == id) return m;
	return -1;
}

static bool ff8_bgate_fxv_live = false; // the running summon is being verified (ff8_bgate_fxv_*)

static void ff8_bgate_la_reset()
{
	ff8_bgate_la_cur = -1;
	ff8_bgate_la_queue = nullptr;
	ff8_bgate_la_ticks = ff8_bgate_la_runs = ff8_bgate_la_faults = ff8_bgate_la_held = ff8_bgate_la_fallback = 0;
	ff8_bgate_la_pools_warned = false;
	ff8_bgate_etq_queues_n = 0;
	ff8_bgate_rec_fx.la.valid = false;
	ff8_bgate_fxv_live = false;
}

// Called before the real tick's queue call: is this a look-ahead effect? (turns task recording on)
static bool ff8_bgate_la_real_tick_begin(void *queue)
{
	int m = ff8_bgate_la_module_of_effect();
	if (m < 0 || !ff8_bgate_gfc_enabled) { if (ff8_bgate_la_cur >= 0) ff8_bgate_la_reset(); return false; }
	if (ff8_bgate_la_cur != m || ff8_bgate_la_queue != queue)
	{
		ff8_bgate_la_reset();
		ff8_bgate_la_cur = m;
		ff8_bgate_la_queue = queue;
		ffnx_info("30fps la: %s look-ahead summon, queue=%p\n", ff8_bgate_la_modules[m].name, queue);
	}
	ff8_bgate_la_ticks++;
	ff8_bgate_etq_log_n = 0;
	ff8_bgate_etq_log_overflow = false;
	ff8_bgate_etq_rec = true;
	return true;
}

static void ff8_bgate_fxv_summary(const char *name);
static void ff8_bgate_la_finish()
{
	if (ff8_bgate_la_cur < 0) return;
	ffnx_info("30fps la: %s finished, real ticks=%u look-aheads=%u faults=%u held frames=%u (fallback %u) queues=%d\n",
		ff8_bgate_la_modules[ff8_bgate_la_cur].name, ff8_bgate_la_ticks, ff8_bgate_la_runs, ff8_bgate_la_faults, ff8_bgate_la_held, ff8_bgate_la_fallback, ff8_bgate_etq_queues_n);
	ff8_bgate_fxv_summary(ff8_bgate_la_modules[ff8_bgate_la_cur].name);
	ff8_bgate_la_reset();
}

// copies the recorded task ranges into the snapshot and tags every primitive with the
// innermost task that drew it (the log is in post-order: inner tasks before their master)
static void ff8_bgate_la_attach_tasks(ff8_bgate_fx_snap &s)
{
	s.ntasks = ff8_bgate_etq_log_n;
	memcpy(s.tasks, ff8_bgate_etq_log, sizeof(ff8_bgate_task_rng) * s.ntasks);
	for (int i = 0; i < FF8_BGATE_FX_MAX_PRIMS; i++) s.ei2prim[i] = -1;
	for (int i = 0; i < s.n; i++) { s.ei2prim[s.prim[i].ei] = (int16_t)i; s.task_of[i] = -1; }
	for (int t = 0; t < s.ntasks; t++)
		for (uint32_t ei = s.tasks[t].a; ei < s.tasks[t].b && ei < FF8_BGATE_FX_MAX_PRIMS; ei++)
		{
			int p = s.ei2prim[ei];
			if (p >= 0 && s.task_of[p] < 0) s.task_of[p] = (int16_t)t;
		}
}

// Snapshot of everything an effect tick may modify (regions, module globals, task pools of
// the queues seen so far, ordering table, CRT seed); shared by the look-ahead and the verifier.
static uint8_t ff8_bgate_snap_extra[16];
#define FF8_BGATE_SNAP_STREAMS_MAX 0x20000
static uint8_t ff8_bgate_snap_streams[FF8_BGATE_SNAP_STREAMS_MAX];
static uint32_t ff8_bgate_snap_seed = 0;
// Field_Alloc scratch (bump stack growing up from GLOBAL_MEMORY_POOL 0x1D999C4): effects
// allocate draw headers there and some engine calls read fields they never set, so the
// memory above the pointer is part of the state a tick sees
static uint8_t ff8_bgate_snap_scratch[0x1000];
static uint32_t ff8_bgate_snap_scratch_addr = 0;

static uint32_t *ff8_bgate_crt_seed() { return (uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14); }

static void ff8_bgate_snap_take(const ff8_bgate_la_module &g)
{
	uint32_t o = 0;
	for (int i = 0; i < FF8_BGATE_LA_REGIONS; i++) { memcpy(ff8_bgate_la_save_regions + o, (void *)ff8_bgate_la_regions[i].addr, ff8_bgate_la_regions[i].size); o += ff8_bgate_la_regions[i].size; }
	memcpy(ff8_bgate_la_save_data, (void *)g.data_lo, g.data_hi - g.data_lo);
	if (g.extra) memcpy(ff8_bgate_snap_extra, (void *)g.extra, g.extra_size);
	for (int i = 0, so = 0; i < g.nstreams; so += g.streams[i].size, i++)
		memcpy(ff8_bgate_snap_streams + so, (void *)g.streams[i].addr, g.streams[i].size);
	ff8_bgate_la_npools = 0;
	ff8_bgate_la_pools_used = 0;
	for (int i = 0; i < ff8_bgate_etq_queues_n; i++)
		ff8_bgate_la_pool_save((ff8_bgate_task_queue *)ff8_bgate_etq_queues[i]);
	memcpy(ff8_bgate_la_save_ot, (void *)FF8_BGATE_OT_SPAN_BASE(), sizeof(ff8_bgate_la_save_ot));
	ff8_bgate_snap_seed = *ff8_bgate_crt_seed();
	ff8_bgate_snap_scratch_addr = *(uint32_t *)0x1D999C4;
	memcpy(ff8_bgate_snap_scratch, (void *)ff8_bgate_snap_scratch_addr, sizeof(ff8_bgate_snap_scratch));
}

static void ff8_bgate_snap_restore(const ff8_bgate_la_module &g)
{
	memcpy((void *)FF8_BGATE_OT_SPAN_BASE(), ff8_bgate_la_save_ot, sizeof(ff8_bgate_la_save_ot));
	for (int i = ff8_bgate_la_npools - 1; i >= 0; i--)
	{
		ff8_bgate_la_pool &p = ff8_bgate_la_pools[i];
		if (p.size) memcpy(p.q->pool, ff8_bgate_la_save_pools + p.off, p.size);
		*p.q = p.hdr;
	}
	memcpy((void *)g.data_lo, ff8_bgate_la_save_data, g.data_hi - g.data_lo);
	if (g.extra) memcpy((void *)g.extra, ff8_bgate_snap_extra, g.extra_size);
	for (int i = 0, so = 0; i < g.nstreams; so += g.streams[i].size, i++)
		memcpy((void *)g.streams[i].addr, ff8_bgate_snap_streams + so, g.streams[i].size);
	uint32_t o = 0;
	for (int i = 0; i < FF8_BGATE_LA_REGIONS; i++) { memcpy((void *)ff8_bgate_la_regions[i].addr, ff8_bgate_la_save_regions + o, ff8_bgate_la_regions[i].size); o += ff8_bgate_la_regions[i].size; }
	*ff8_bgate_crt_seed() = ff8_bgate_snap_seed;
	memcpy((void *)ff8_bgate_snap_scratch_addr, ff8_bgate_snap_scratch, sizeof(ff8_bgate_snap_scratch));
}

// TEMP diagnostics: how well did the look-ahead of tick N predict the real tick N+1? Same task
// node, same rank: identical packet / same size but different (max vertex step) / count mismatch.
static void ff8_bgate_la_check(const ff8_bgate_fx_snap &cur)
{
	const ff8_bgate_fx_snap &la = ff8_bgate_rec_fx.la;
	if (ff8_bgate_la_cur < 0 || !la.valid || la.frame + (uint32_t)ff8_bgate_n != cur.frame) return;
	int tasks = 0, found = 0, identical = 0, differ = 0, mismatch = 0, maxd = 0, missing = 0;
	static ff8_bgate_pkt_info pi;
	for (int t = 0; t < cur.ntasks; t++)
	{
		const ff8_bgate_task_rng &ct = cur.tasks[t];
		if (ct.b <= ct.a) continue;
		tasks++;
		int lt = -1;
		for (int k = 0; k < la.ntasks; k++) if (la.tasks[k].node == ct.node) { lt = k; break; }
		if (lt < 0) { missing++; continue; }
		found++;
		const ff8_bgate_task_rng &l = la.tasks[lt];
		if (l.b - l.a != ct.b - ct.a) { mismatch++; continue; }
		for (uint32_t k = 0; k < (uint32_t)(ct.b - ct.a); k++)
		{
			uint32_t ec = ct.a + k, el = l.a + k;
			if (ec >= FF8_BGATE_FX_MAX_PRIMS || el >= FF8_BGATE_FX_MAX_PRIMS) continue;
			int pc = cur.ei2prim[ec], pl = la.ei2prim[el];
			if (pc < 0 || pl < 0) continue;
			const ff8_bgate_fx_prim &a = cur.prim[pc], &b = la.prim[pl];
			if (a.words == b.words && memcmp(&cur.arena[a.off + 1], &la.arena[b.off + 1], (a.words - 1) * 4) == 0) { identical++; continue; }
			differ++;
			if (a.words == b.words && ff8_bgate_pkt_parse(&cur.arena[a.off], a.words, pi))
			{
				int d = ff8_bgate_pkt_step(&cur.arena[a.off], &la.arena[b.off], pi);
				if (d > maxd) maxd = d;
			}
		}
	}
	if (ff8_bgate_la_ticks <= 400)
		ffnx_info("30fps la: check t=%u tasks=%d found=%d missing=%d count_mismatch=%d identical=%d differ=%d maxstep=%d\n",
			ff8_bgate_la_ticks, tasks, found, missing, mismatch, identical, differ, maxd);
}

// The look-ahead itself: run tick N+1 under snapshot, capture its draws, put everything back.
static void ff8_bgate_la_lookahead(void *queue, int (__cdecl *orig)(void *))
{
	int m = ff8_bgate_la_cur;
	ff8_bgate_fx_snap &la = ff8_bgate_rec_fx.la;
	la.valid = false;
	if (m < 0 || queue != ff8_bgate_la_queue || ff8_bgate_la_faults >= 3 || ff8_bgate_fx_mode < 2) return;
	const ff8_bgate_la_module &g = ff8_bgate_la_modules[m];
	if (!g.lookahead || g.data_hi - g.data_lo > FF8_BGATE_LA_DATA_MAX) return;

	// --- snapshot ---
	ff8_bgate_snap_take(g);
	uint32_t rec_begin_save = ff8_bgate_fx_rec_begin;

	// --- tick N+1, shielded ---
	if (ff8_bgate_la_ticks <= 400)
		ffnx_info("30fps la: t=%u begin (real n=%d tasks=%d overflow=%d queues=%d pools=%d/%u bytes)\n", ff8_bgate_la_ticks,
			ff8_bgate_rec_fx.snaps[ff8_bgate_rec_fx.cur].n, ff8_bgate_rec_fx.snaps[ff8_bgate_rec_fx.cur].ntasks, (int)ff8_bgate_etq_log_overflow,
			ff8_bgate_etq_queues_n, ff8_bgate_la_npools, ff8_bgate_la_pools_used);
	ff8_bgate_etq_cycle = false;
	ff8_bgate_fx_rec_begin = FF8_BGATE_EXEC_CUR;
	ff8_bgate_etq_log_n = 0;
	ff8_bgate_etq_log_overflow = false;
	ff8_bgate_etq_rec = true;
	ff8_bgate_la_in_lookahead = true;
	ff8_bgate_shield_set(true);
	bool ok = ff8_bgate_tlb_queue_guarded(orig, queue);
	ff8_bgate_shield_set(false);
	ff8_bgate_la_in_lookahead = false;
	ff8_bgate_etq_rec = false;
	ff8_bgate_la_runs++;

	// --- capture its draws (packets still live in the effect's arenas) ---
	if (ok)
	{
		la.n = 0; la.orphans = 0; la.used = 0; la.rlist_delta = 0;
		la.ctx = queue; la.frame = ff8_bgate_frame_no; la.valid = false;
		__try { ff8_bgate_fx_capture_unsafe(la); }
		__except (EXCEPTION_EXECUTE_HANDLER) { la.valid = false; }
		if (la.valid) ff8_bgate_la_attach_tasks(la);
	}

	// --- restore ---
	ff8_bgate_snap_restore(g);
	ff8_bgate_fx_rec_begin = rec_begin_save;
	if (ff8_bgate_etq_cycle) { la.valid = false; ff8_bgate_la_faults++; }
	if (!ok)
	{
		ff8_bgate_la_faults++;
		la.valid = false;
		ffnx_info("30fps la: %s LOOK-AHEAD FAULT %08X at %08X (tick %u, fault %u)\n", g.name, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr, ff8_bgate_la_ticks, ff8_bgate_la_faults);
	}
	if (ff8_bgate_la_ticks <= 400)
		ffnx_info("30fps la: t=%u done ok=%d valid=%d la n=%d tasks=%d overflow=%d pools=%d\n", ff8_bgate_la_ticks, (int)ok, (int)la.valid, la.n, la.ntasks, (int)ff8_bgate_etq_log_overflow, ff8_bgate_la_npools);
}

// Held frame: draw tick N half way to tick N+1 (exact per-task pairing in the replay)
static bool ff8_bgate_la_held_frame(void *queue)
{
	if (ff8_bgate_la_cur < 0 || queue != ff8_bgate_la_queue || !ff8_bgate_gfc_enabled) return false;
	const ff8_bgate_fx_snap &la = ff8_bgate_rec_fx.la, &cur = ff8_bgate_rec_fx.snaps[ff8_bgate_rec_fx.cur];
	if (!la.valid || !cur.valid || la.ctx != cur.ctx || la.frame != cur.frame) { ff8_bgate_la_fallback++; return false; }
	ff8_bgate_la_active = true;
	ff8_bgate_fx_replay(queue);
	ff8_bgate_la_active = false;
	ff8_bgate_la_held++;
	return true;
}

// --- differential verification of the native effect ports (ff8fx, src/ff8/battle/fx) ---
// For an effect with ported functions, every real tick runs twice from the same snapshot:
//   A = the original code, with every external engine call RECORDED (arguments, return
//       value, and the bytes it wrote inside the snapshot regions),
//   B = the same tick with the ports dispatched, external calls REPLAYED from A (not
//       executed again: sounds, loads, damage happen exactly once).
// Then everything is compared: return value, every snapshot region byte (magic buffer,
// module globals, camera, GTE, entity arrays...), task pools, ordering table, each emitted
// GPU primitive (bucket, depth keys, packet words) and the external call sequence. On a
// match B's result stays (it is identical); on any difference the post-A state is put back,
// so the game always continues on vanilla behaviour, and the first difference is logged
// with the task that produced it.
enum { FXV_EXT_OFF = 0, FXV_EXT_RECORD, FXV_EXT_REPLAY };
struct ff8_bgate_fxv_site { uint32_t addr; int arity; const char *name; void *stub; uint8_t saved[5]; bool patched; };
struct ff8_bgate_fxv_call { int site; uint32_t args[6]; uint32_t ret; uint32_t w_off, w_len; uint16_t cw_after; };
#define FXV_CALLS_MAX 256
#define FXV_WLOG_SIZE 0x100000
static int ff8_bgate_fxv_ext_mode = FXV_EXT_OFF, ff8_bgate_fxv_depth = 0;
static ff8_bgate_fxv_call ff8_bgate_fxv_calls[FXV_CALLS_MAX];
static int ff8_bgate_fxv_ncalls = 0, ff8_bgate_fxv_replay_i = 0;
static uint8_t ff8_bgate_fxv_wlog[FXV_WLOG_SIZE];
static uint32_t ff8_bgate_fxv_wlog_used = 0;
static bool ff8_bgate_fxv_wlog_overflow = false;
static char ff8_bgate_fxv_call_msg[256];
static int ff8_bgate_fxv_ext_call(int site, uint32_t *args);
template<int I> static uint32_t __cdecl ff8_bgate_fxv_stub(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	uint32_t a[6] = { a0, a1, a2, a3, a4, a5 };
	return (uint32_t)ff8_bgate_fxv_ext_call(I, a);
}
// arity 0 = not verified yet: only the call itself is compared, not its arguments
static ff8_bgate_fxv_site ff8_bgate_fxv_sites[] = {
	{ 0x501330, 3, "BdPlaySE", (void *)ff8_bgate_fxv_stub<0> },
	{ 0x5018C0, 3, "BdPlaySummonStream", (void *)ff8_bgate_fxv_stub<1> },
	{ 0x501860, 2, "BdTransSummonStream", (void *)ff8_bgate_fxv_stub<2> },
	{ 0x4A29A0, 3, "BdSound_ClaimVoiceSlot", (void *)ff8_bgate_fxv_stub<3> },
	{ 0x4A2940, 1, "BdSound_ReleaseVoiceSlot", (void *)ff8_bgate_fxv_stub<4> },
	{ 0x48D0A0, 4, "pre_LoadBattleFile", (void *)ff8_bgate_fxv_stub<5> },
	{ 0x508480, 0, "BattleFile_CharacterLoad", (void *)ff8_bgate_fxv_stub<6> },
	{ 0x506BA0, 2, "ApplyActionResultToTargets", (void *)ff8_bgate_fxv_stub<7> },
	{ 0x506690, 0, "ApplyActionResultToTarget", (void *)ff8_bgate_fxv_stub<8> },
	{ 0x505C00, 0, "QueueChainTransformation", (void *)ff8_bgate_fxv_stub<9> },
	{ 0x506C10, 1, "AddTaskToQueueAnimSeq", (void *)ff8_bgate_fxv_stub<10> },
	// battle-stage model swaps (Griever: attacker model 36/37 at tick 50, swap at 110)
	{ 0x512AA0, 2, "loadBS_36Or37", (void *)ff8_bgate_fxv_stub<11> },
	{ 0x512AC0, 2, "BS_SwapModel_512AC0", (void *)ff8_bgate_fxv_stub<12> },
};
#define FXV_SITES ((int)(sizeof(ff8_bgate_fxv_sites) / sizeof(ff8_bgate_fxv_sites[0])))

static void ff8_bgate_fxv_patch(ff8_bgate_fxv_site &s, bool on)
{
	if (s.patched == on) return;
	uint8_t *code = (uint8_t *)s.addr;
	DWORD old;
	if (!VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &old)) return;
	if (on)
	{
		memcpy(s.saved, code, 5);
		code[0] = 0xE9;
		*(int32_t *)(code + 1) = (int32_t)((uint8_t *)s.stub - (code + 5));
	}
	else memcpy(code, s.saved, 5);
	VirtualProtect(code, 5, old, &old);
	FlushInstructionCache(GetCurrentProcess(), code, 5);
	s.patched = on;
}

static void ff8_bgate_fxv_patch_all(bool on)
{
	for (int i = 0; i < FXV_SITES; i++) ff8_bgate_fxv_patch(ff8_bgate_fxv_sites[i], on);
}

// the regions every run is compared on: la regions + the module's globals (+ extra cell)
static ff8_bgate_la_region ff8_bgate_fxv_reg[FF8_BGATE_LA_REGIONS + 8];
static int ff8_bgate_fxv_nreg = 0;
static uint32_t ff8_bgate_fxv_reg_bytes = 0;
static uint8_t ff8_bgate_fxv_scan[0x100000 + 0x1000 + FF8_BGATE_LA_DATA_MAX + FF8_BGATE_SNAP_STREAMS_MAX + 64];

static void ff8_bgate_fxv_set_regions(const ff8_bgate_la_module &g)
{
	ff8_bgate_fxv_nreg = 0;
	for (int i = 0; i < FF8_BGATE_LA_REGIONS; i++) ff8_bgate_fxv_reg[ff8_bgate_fxv_nreg++] = ff8_bgate_la_regions[i];
	ff8_bgate_fxv_reg[ff8_bgate_fxv_nreg++] = { g.data_lo, g.data_hi - g.data_lo };
	if (g.extra) ff8_bgate_fxv_reg[ff8_bgate_fxv_nreg++] = { g.extra, g.extra_size };
	for (int i = 0; i < g.nstreams && i < 6; i++) ff8_bgate_fxv_reg[ff8_bgate_fxv_nreg++] = g.streams[i];
	ff8_bgate_fxv_reg_bytes = 0;
	for (int i = 0; i < ff8_bgate_fxv_nreg; i++) ff8_bgate_fxv_reg_bytes += ff8_bgate_fxv_reg[i].size;
}

static void ff8_bgate_fxv_regions_copy(uint8_t *dst)
{
	uint32_t o = 0;
	for (int i = 0; i < ff8_bgate_fxv_nreg; i++) { memcpy(dst + o, (void *)ff8_bgate_fxv_reg[i].addr, ff8_bgate_fxv_reg[i].size); o += ff8_bgate_fxv_reg[i].size; }
}

static void ff8_bgate_fxv_regions_put(const uint8_t *src)
{
	uint32_t o = 0;
	for (int i = 0; i < ff8_bgate_fxv_nreg; i++) { memcpy((void *)ff8_bgate_fxv_reg[i].addr, src + o, ff8_bgate_fxv_reg[i].size); o += ff8_bgate_fxv_reg[i].size; }
}

// bytes of the regions that differ from the pre-call copy -> write log (addr, len, bytes)
static uint32_t ff8_bgate_fxv_diff_to_wlog()
{
	uint32_t start = ff8_bgate_fxv_wlog_used, o = 0;
	for (int r = 0; r < ff8_bgate_fxv_nreg; r++)
	{
		const uint8_t *cur = (const uint8_t *)ff8_bgate_fxv_reg[r].addr, *pre = ff8_bgate_fxv_scan + o;
		uint32_t size = ff8_bgate_fxv_reg[r].size;
		for (uint32_t i = 0; i < size;)
		{
			uint32_t blk = size - i < 64 ? size - i : 64;
			if (memcmp(cur + i, pre + i, blk) == 0) { i += blk; continue; }
			uint32_t a = i;
			while (a < size && cur[a] == pre[a]) a++;
			uint32_t b = a;
			while (b < size && (cur[b] != pre[b] || (b + 1 < size && cur[b + 1] != pre[b + 1]))) b++;
			if (ff8_bgate_fxv_wlog_used + 8 + (b - a) > FXV_WLOG_SIZE) { ff8_bgate_fxv_wlog_overflow = true; return ff8_bgate_fxv_wlog_used - start; }
			*(uint32_t *)(ff8_bgate_fxv_wlog + ff8_bgate_fxv_wlog_used) = ff8_bgate_fxv_reg[r].addr + a;
			*(uint32_t *)(ff8_bgate_fxv_wlog + ff8_bgate_fxv_wlog_used + 4) = b - a;
			memcpy(ff8_bgate_fxv_wlog + ff8_bgate_fxv_wlog_used + 8, cur + a, b - a);
			ff8_bgate_fxv_wlog_used += 8 + (b - a);
			i = b;
		}
		o += size;
	}
	return ff8_bgate_fxv_wlog_used - start;
}

static uint32_t ff8_bgate_fxv_call_through(int site, const uint32_t *a)
{
	ff8_bgate_fxv_site &s = ff8_bgate_fxv_sites[site];
	ff8_bgate_fxv_patch(s, false);
	uint32_t r = ((uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))s.addr)(a[0], a[1], a[2], a[3], a[4], a[5]);
	ff8_bgate_fxv_patch(s, true);
	return r;
}

static int ff8_bgate_fxv_ext_call(int site, uint32_t *args)
{
	const ff8_bgate_fxv_site &s = ff8_bgate_fxv_sites[site];
	if (ff8_bgate_fxv_ext_mode == FXV_EXT_RECORD)
	{
		if (ff8_bgate_fxv_depth > 0 || ff8_bgate_fxv_ncalls >= FXV_CALLS_MAX)
			return (int)ff8_bgate_fxv_call_through(site, args); // nested inside a recorded call: part of it
		ff8_bgate_fxv_depth++;
		ff8_bgate_fxv_regions_copy(ff8_bgate_fxv_scan);
		uint32_t r = ff8_bgate_fxv_call_through(site, args);
		ff8_bgate_fxv_call &c = ff8_bgate_fxv_calls[ff8_bgate_fxv_ncalls++];
		c.site = site;
		memcpy(c.args, args, sizeof(c.args));
		c.ret = r;
		c.w_off = ff8_bgate_fxv_wlog_used;
		c.w_len = ff8_bgate_fxv_diff_to_wlog();
		uint16_t cw;
		__asm fnstcw cw
		c.cw_after = cw;
		ff8_bgate_fxv_depth--;
		return (int)r;
	}
	if (ff8_bgate_fxv_ext_mode == FXV_EXT_REPLAY)
	{
		if (ff8_bgate_fxv_replay_i >= ff8_bgate_fxv_ncalls)
		{
			if (!ff8_bgate_fxv_call_msg[0])
				_snprintf_s(ff8_bgate_fxv_call_msg, sizeof(ff8_bgate_fxv_call_msg), _TRUNCATE, "extra call %s(%08X, %08X, %08X) #%d", s.name, args[0], args[1], args[2], ff8_bgate_fxv_replay_i);
			ff8_bgate_fxv_replay_i++;
			return 0;
		}
		const ff8_bgate_fxv_call &c = ff8_bgate_fxv_calls[ff8_bgate_fxv_replay_i++];
		bool same = c.site == site;
		for (int i = 0; same && i < s.arity; i++) same = c.args[i] == args[i];
		if (!same && !ff8_bgate_fxv_call_msg[0])
			_snprintf_s(ff8_bgate_fxv_call_msg, sizeof(ff8_bgate_fxv_call_msg), _TRUNCATE, "call #%d: original %s(%08X, %08X, %08X), port %s(%08X, %08X, %08X)",
				ff8_bgate_fxv_replay_i - 1, ff8_bgate_fxv_sites[c.site].name, c.args[0], c.args[1], c.args[2], s.name, args[0], args[1], args[2]);
		{
			uint16_t cw = c.cw_after; // the FPU mode the real call left
			__asm fldcw cw
		}
		for (uint32_t o = c.w_off; o < c.w_off + c.w_len;)
		{
			uint32_t addr = *(uint32_t *)(ff8_bgate_fxv_wlog + o), len = *(uint32_t *)(ff8_bgate_fxv_wlog + o + 4);
			memcpy((void *)addr, ff8_bgate_fxv_wlog + o + 8, len);
			o += 8 + len;
		}
		return (int)c.ret;
	}
	return (int)ff8_bgate_fxv_call_through(site, args);
}

// post-A state (kept to compare B with, and put back on a mismatch)
static uint8_t ff8_bgate_fxv_post_regions[sizeof(ff8_bgate_fxv_scan)];
static uint8_t ff8_bgate_fxv_post_pools[FF8_BGATE_LA_POOLS_MAX];
static ff8_bgate_task_queue ff8_bgate_fxv_post_pool_hdr[32];
static uint32_t ff8_bgate_fxv_post_ot[FF8_BGATE_OT_SPAN_WORDS];
static uint8_t ff8_bgate_fxv_post_exec[FF8_BGATE_FX_MAX_PRIMS * sizeof(ff8_bgate_exec_node)];
static uint32_t ff8_bgate_fxv_post_exec_len = 0, ff8_bgate_fxv_post_seed = 0;
static ff8_bgate_fx_snap ff8_bgate_fxv_a, ff8_bgate_fxv_b;
static ff8_bgate_task_rng ff8_bgate_fxv_log_a[FF8_BGATE_TASK_LOG];
static int ff8_bgate_fxv_log_a_n = 0;
// statistics for the running summon
static uint32_t ff8_bgate_fxv_held_frames = 0;
static uint32_t ff8_bgate_fxv_ticks = 0, ff8_bgate_fxv_match = 0, ff8_bgate_fxv_mismatch = 0, ff8_bgate_fxv_faults = 0, ff8_bgate_fxv_first_bad = 0, ff8_bgate_fxv_logged = 0;
static bool ff8_bgate_fxv_enabled = true; // verify ported modules (else: original code only)

static bool ff8_bgate_fxv_wanted()
{
	return ff8_bgate_fxv_enabled && ff8_bgate_la_cur >= 0 && ff8_bgate_fxv_faults < 3
		&& ff8fx::module_ported(ff8_bgate_la_modules[ff8_bgate_la_cur].effect_id);
}

static void ff8_bgate_fxv_pools_save_post()
{
	uint32_t o = 0;
	for (int i = 0; i < ff8_bgate_la_npools && i < 32; i++)
	{
		ff8_bgate_la_pool &p = ff8_bgate_la_pools[i];
		ff8_bgate_fxv_post_pool_hdr[i] = *p.q;
		if (p.size) { memcpy(ff8_bgate_fxv_post_pools + o, p.q->pool, p.size); o += p.size; }
	}
}

static void ff8_bgate_fxv_pools_put_post()
{
	uint32_t o = 0;
	for (int i = 0; i < ff8_bgate_la_npools && i < 32; i++)
	{
		ff8_bgate_la_pool &p = ff8_bgate_la_pools[i];
		*p.q = ff8_bgate_fxv_post_pool_hdr[i];
		if (p.size) { memcpy(p.q->pool, ff8_bgate_fxv_post_pools + o, p.size); o += p.size; }
	}
}

static const char *ff8_bgate_fxv_task_name(const ff8_bgate_fx_snap &s, int prim, char *buf, size_t n)
{
	int t = prim >= 0 && prim < s.n ? s.task_of[prim] : -1;
	if (t < 0) { _snprintf_s(buf, n, _TRUNCATE, "no task"); return buf; }
	uint32_t fn = (uint32_t)((ff8_bgate_task_node *)s.tasks[t].node)->func;
	const char *pn = ff8fx::port_name(fn);
	_snprintf_s(buf, n, _TRUNCATE, "task %08X node %p%s%s", fn, s.tasks[t].node, pn ? " = " : "", pn ? pn : "");
	return buf;
}

// first differing byte of the snapshot regions (port run vs post-original), 0 if none
static int ff8_bgate_fxv_mem_diff(char *out, size_t n)
{
	uint32_t o = 0;
	for (int r = 0; r < ff8_bgate_fxv_nreg; r++)
	{
		const uint8_t *cur = (const uint8_t *)ff8_bgate_fxv_reg[r].addr, *post = ff8_bgate_fxv_post_regions + o;
		uint32_t size = ff8_bgate_fxv_reg[r].size;
		if (memcmp(cur, post, size) != 0)
		{
			uint32_t i = 0, ndiff = 0;
			while (cur[i] == post[i]) i++;
			for (uint32_t k = i; k < size; k++) if (cur[k] != post[k]) ndiff++;
			uint32_t w0 = i & ~3u;
			return _snprintf_s(out, n, _TRUNCATE, "memory %08X (region %08X+%X): original %08X, port %08X, %u bytes differ in the region",
				ff8_bgate_fxv_reg[r].addr + i, ff8_bgate_fxv_reg[r].addr, i,
				w0 + 4 <= size ? *(const uint32_t *)(post + w0) : post[i], w0 + 4 <= size ? *(const uint32_t *)(cur + w0) : cur[i], ndiff);
		}
		o += size;
	}
	return 0;
}

// first differing field of a task node (port run vs post-original), 0 if identical / not found
static int ff8_bgate_fxv_node_diff(const void *node, char *out, size_t n)
{
	uint32_t o = 0;
	for (int i = 0; i < ff8_bgate_la_npools && i < 32; i++)
	{
		ff8_bgate_la_pool &p = ff8_bgate_la_pools[i];
		const uint8_t *pool = (const uint8_t *)p.q->pool;
		if (p.size && (const uint8_t *)node >= pool && (const uint8_t *)node < pool + p.size)
		{
			uint32_t off = (uint32_t)((const uint8_t *)node - pool), size = (uint32_t)p.q->node_size;
			const uint8_t *cur = pool + off, *post = ff8_bgate_fxv_post_pools + o + off;
			for (uint32_t k = 0; k < size; k++)
				if (cur[k] != post[k])
				{
					uint32_t w = k & ~3u;
					return _snprintf_s(out, n, _TRUNCATE, "node +%X: original %08X, port %08X", w, *(const uint32_t *)(post + w), *(const uint32_t *)(cur + w));
				}
			return _snprintf_s(out, n, _TRUNCATE, "node identical");
		}
		o += p.size;
	}
	return 0;
}

// compare the current (B) state with post-A; returns a description of the first difference or null
static const char *ff8_bgate_fxv_compare(int ra, int rb)
{
	static char msg[1024];
	if (ra != rb) { _snprintf_s(msg, sizeof(msg), _TRUNCATE, "queue return: original %d, port %d", ra, rb); return msg; }
	if (ff8_bgate_fxv_call_msg[0]) { _snprintf_s(msg, sizeof(msg), _TRUNCATE, "external %s", ff8_bgate_fxv_call_msg); return msg; }
	if (ff8_bgate_fxv_replay_i != ff8_bgate_fxv_ncalls)
	{
		_snprintf_s(msg, sizeof(msg), _TRUNCATE, "external calls: original made %d, port %d (next expected %s)", ff8_bgate_fxv_ncalls, ff8_bgate_fxv_replay_i,
			ff8_bgate_fxv_replay_i < ff8_bgate_fxv_ncalls ? ff8_bgate_fxv_sites[ff8_bgate_fxv_calls[ff8_bgate_fxv_replay_i].site].name : "-");
		return msg;
	}
	// primitives first: their task attribution is the most useful clue
	const ff8_bgate_fx_snap &A = ff8_bgate_fxv_a, &B = ff8_bgate_fxv_b;
	char tn[160];
	if (A.valid != B.valid || A.n != B.n)
	{
		int o = _snprintf_s(msg, sizeof(msg), _TRUNCATE, "primitive count: original %d, port %d (valid %d/%d, tasks %d/%d, orphans %d/%d)",
			A.n, B.n, (int)A.valid, (int)B.valid, A.ntasks, B.ntasks, A.orphans, B.orphans);
		for (int ta = 0; ta < A.ntasks && o > 0; ta++)
		{
			int tb = 0;
			while (tb < B.ntasks && B.tasks[tb].node != A.tasks[ta].node) tb++;
			int na = A.tasks[ta].b - A.tasks[ta].a, nb = tb < B.ntasks ? B.tasks[tb].b - B.tasks[tb].a : -1;
			if (na != nb)
			{
				uint32_t fn = (uint32_t)((ff8_bgate_task_node *)A.tasks[ta].node)->func;
				const char *pn = ff8fx::port_name(fn);
				_snprintf_s(msg + o, sizeof(msg) - o, _TRUNCATE, "; first differing task %08X%s%s node %p drew %d, port %d", fn, pn ? " = " : "", pn ? pn : "", A.tasks[ta].node, na, nb);
				break;
			}
		}
		return msg;
	}
	for (int i = 0; i < A.n; i++)
	{
		const ff8_bgate_fx_prim &a = A.prim[i], &b = B.prim[i];
		if (a.words != b.words || a.bucket != b.bucket || a.msk != b.msk || memcmp(a.k, b.k, sizeof(a.k)) != 0)
		{
			_snprintf_s(msg, sizeof(msg), _TRUNCATE, "primitive %d header: words %u/%u bucket %u/%u msk %X/%X k0 %d/%d (%s)", i, a.words, b.words, a.bucket, b.bucket, a.msk, b.msk, a.k[0], b.k[0],
				ff8_bgate_fxv_task_name(A, i, tn, sizeof(tn)));
			return msg;
		}
		for (uint32_t w = 1; w < a.words; w++)
			if (A.arena[a.off + w] != B.arena[b.off + w])
			{
				int o = _snprintf_s(msg, sizeof(msg), _TRUNCATE, "primitive %d word %u: original %08X, port %08X (%s)", i, w, A.arena[a.off + w], B.arena[b.off + w],
					ff8_bgate_fxv_task_name(A, i, tn, sizeof(tn)));
				int ndiff = 0;
				for (int j = i; j < A.n; j++)
					if (A.prim[j].words != B.prim[j].words || memcmp(&A.arena[A.prim[j].off + 1], &B.arena[B.prim[j].off + 1], (A.prim[j].words - 1) * 4) != 0) ndiff++;
				if (o > 0) o += _snprintf_s(msg + o, sizeof(msg) - o, _TRUNCATE, " [%d primitives differ]", ndiff);
				int t = A.task_of[i];
				if (o > 0 && t >= 0) { o += _snprintf_s(msg + o, sizeof(msg) - o, _TRUNCATE, " | "); if (o > 0) o += ff8_bgate_fxv_node_diff(A.tasks[t].node, msg + o, sizeof(msg) - o); }
				if (o > 0) { o += _snprintf_s(msg + o, sizeof(msg) - o, _TRUNCATE, " | "); if (o > 0) ff8_bgate_fxv_mem_diff(msg + o, sizeof(msg) - o); }
				return msg;
			}
	}
	// memory regions
	uint32_t o = 0;
	for (int r = 0; r < ff8_bgate_fxv_nreg; r++)
	{
		const uint8_t *cur = (const uint8_t *)ff8_bgate_fxv_reg[r].addr, *post = ff8_bgate_fxv_post_regions + o;
		uint32_t size = ff8_bgate_fxv_reg[r].size;
		if (memcmp(cur, post, size) != 0)
		{
			uint32_t i = 0, ndiff = 0;
			while (cur[i] == post[i]) i++;
			for (uint32_t k = i; k < size; k++) if (cur[k] != post[k]) ndiff++;
			uint32_t w0 = i & ~3u;
			_snprintf_s(msg, sizeof(msg), _TRUNCATE, "memory %08X (region %08X+%X): original %08X, port %08X, %u bytes differ in the region",
				ff8_bgate_fxv_reg[r].addr + i, ff8_bgate_fxv_reg[r].addr, i,
				w0 + 4 <= size ? *(const uint32_t *)(post + w0) : post[i], w0 + 4 <= size ? *(const uint32_t *)(cur + w0) : cur[i], ndiff);
			return msg;
		}
		o += size;
	}
	// task pools
	o = 0;
	for (int i = 0; i < ff8_bgate_la_npools && i < 32; i++)
	{
		ff8_bgate_la_pool &p = ff8_bgate_la_pools[i];
		if (memcmp(p.q, &ff8_bgate_fxv_post_pool_hdr[i], sizeof(ff8_bgate_task_queue)) != 0)
		{
			_snprintf_s(msg, sizeof(msg), _TRUNCATE, "task queue %p header: original head %p tail %p, port head %p tail %p", p.q,
				ff8_bgate_fxv_post_pool_hdr[i].head, ff8_bgate_fxv_post_pool_hdr[i].tail, p.q->head, p.q->tail);
			return msg;
		}
		if (p.size && memcmp(p.q->pool, ff8_bgate_fxv_post_pools + o, p.size) != 0)
		{
			const uint8_t *cur = (const uint8_t *)p.q->pool, *post = ff8_bgate_fxv_post_pools + o;
			uint32_t k = 0;
			while (cur[k] == post[k]) k++;
			_snprintf_s(msg, sizeof(msg), _TRUNCATE, "task pool of queue %p: node %u (+%X) byte %02X original, %02X port", p.q,
				k / (uint32_t)p.q->node_size, k % (uint32_t)p.q->node_size, post[k], cur[k]);
			return msg;
		}
		o += p.size;
	}
	uint32_t *ot = (uint32_t *)FF8_BGATE_OT_SPAN_BASE();
	for (int b = 0; b < FF8_BGATE_OT_SPAN_WORDS; b++)
		if (ot[b] != ff8_bgate_fxv_post_ot[b])
		{
			_snprintf_s(msg, sizeof(msg), _TRUNCATE, "ordering table bucket %d: original %08X, port %08X", b - 17, ff8_bgate_fxv_post_ot[b], ot[b]);
			return msg;
		}
	uint32_t *seed = (uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14);
	if (*seed != ff8_bgate_fxv_post_seed) { _snprintf_s(msg, sizeof(msg), _TRUNCATE, "CRT rand seed: original %08X, port %08X", ff8_bgate_fxv_post_seed, *seed); return msg; }
	return nullptr;
}

static void ff8_bgate_fxv_put_back_A()
{
	// A's packets back where they were (the frame arena is outside the regions), then all state
	const ff8_bgate_fx_snap &A = ff8_bgate_fxv_a;
	for (int i = 0; i < A.n; i++)
		memcpy((void *)A.prim[i].src_pkt, &A.arena[A.prim[i].off], A.prim[i].words * 4);
	memcpy((void *)ff8_bgate_fx_rec_begin, ff8_bgate_fxv_post_exec, ff8_bgate_fxv_post_exec_len);
	ff8_bgate_fxv_pools_put_post();
	ff8_bgate_fxv_regions_put(ff8_bgate_fxv_post_regions);
	memcpy((void *)FF8_BGATE_OT_SPAN_BASE(), ff8_bgate_fxv_post_ot, sizeof(ff8_bgate_fxv_post_ot));
	*(uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14) = ff8_bgate_fxv_post_seed;
}

static void ff8_bgate_fxv_capture(ff8_bgate_fx_snap &s, void *queue)
{
	s.n = 0; s.orphans = 0; s.used = 0; s.rlist_delta = 0;
	s.ctx = queue; s.frame = ff8_bgate_frame_no; s.valid = false;
	__try { ff8_bgate_fx_capture_unsafe(s); }
	__except (EXCEPTION_EXECUTE_HANDLER) { s.valid = false; }
	if (s.valid) ff8_bgate_la_attach_tasks(s);
}

// Original code reads some locals it never initialised (padding of vectors, unused 4th
// words...). Both runs must see the same stack bytes, or those reads make them diverge
// without any port being wrong: the stack below the harness is zeroed before each run.
__declspec(noinline) static void ff8_bgate_fxv_scrub_stack()
{
	volatile uint8_t buf[0x10000];
	SecureZeroMemory((void *)buf, sizeof(buf));
}

static inline uint16_t ff8_bgate_x87_cw()
{
	uint16_t cw;
	__asm fnstcw cw
	return cw;
}
static inline void ff8_bgate_x87_set_cw(uint16_t cw)
{
	__asm fldcw cw
}

// One real tick of a ported effect: original (A) and ports (B) from the same snapshot.
static int ff8_bgate_fxv_tick(void *queue, int (__cdecl *orig)(void *))
{
	const ff8_bgate_la_module &g = ff8_bgate_la_modules[ff8_bgate_la_cur];
	ff8_bgate_fxv_ticks++;
	uint8_t loader_state[8] = {};
	if (*(uint8_t **)0x1DCD6EC) memcpy(loader_state, *(uint8_t **)0x1DCD6EC, 5);
	ff8_bgate_fxv_live = true;
	ff8_bgate_fxv_set_regions(g);
	ff8_bgate_snap_take(g);
	uint32_t *seed = (uint32_t *)(((uint8_t *(__cdecl *)())0x560578)() + 0x14);

	// --- A: original, external calls recorded ---
	ff8_bgate_fxv_ncalls = 0;
	ff8_bgate_fxv_wlog_used = 0;
	ff8_bgate_fxv_wlog_overflow = false;
	ff8_bgate_etq_log_n = 0;
	ff8_bgate_etq_log_overflow = false;
	ff8_bgate_etq_rec = true;
	ff8_bgate_la_in_lookahead = true; // queues first seen now get their pool saved before they run
	ff8_bgate_fxv_ext_mode = FXV_EXT_RECORD;
	ff8_bgate_fxv_patch_all(true);
	ff8_bgate_fxv_scrub_stack();
	uint16_t cw_a0 = ff8_bgate_x87_cw();
	int ra = orig(queue);
	uint16_t cw_a1 = ff8_bgate_x87_cw();
	ff8_bgate_fxv_patch_all(false);
	ff8_bgate_fxv_ext_mode = FXV_EXT_OFF;
	ff8_bgate_la_in_lookahead = false;
	ff8_bgate_fxv_capture(ff8_bgate_fxv_a, queue);
	ff8_bgate_fxv_regions_copy(ff8_bgate_fxv_post_regions);
	ff8_bgate_fxv_pools_save_post();
	memcpy(ff8_bgate_fxv_post_ot, (void *)FF8_BGATE_OT_SPAN_BASE(), sizeof(ff8_bgate_fxv_post_ot));
	ff8_bgate_fxv_post_exec_len = FF8_BGATE_EXEC_CUR - ff8_bgate_fx_rec_begin;
	if (ff8_bgate_fxv_post_exec_len > sizeof(ff8_bgate_fxv_post_exec)) ff8_bgate_fxv_post_exec_len = sizeof(ff8_bgate_fxv_post_exec);
	memcpy(ff8_bgate_fxv_post_exec, (void *)ff8_bgate_fx_rec_begin, ff8_bgate_fxv_post_exec_len);
	ff8_bgate_fxv_post_seed = *seed;
	ff8_bgate_fxv_log_a_n = ff8_bgate_etq_log_n;
	memcpy(ff8_bgate_fxv_log_a, ff8_bgate_etq_log, sizeof(ff8_bgate_task_rng) * ff8_bgate_etq_log_n);
	if (ff8_bgate_fxv_wlog_overflow || ff8_bgate_etq_log_overflow)
	{
		// cannot replay/compare this tick faithfully: keep A as it is
		ff8_bgate_etq_rec = false;
		return ra;
	}

	// --- B: ports, external calls replayed ---
	ff8_bgate_snap_restore(g);
	ff8_bgate_etq_log_n = 0;
	ff8_bgate_fxv_replay_i = 0;
	ff8_bgate_fxv_call_msg[0] = 0;
	ff8_bgate_fxv_ext_mode = FXV_EXT_REPLAY;
	ff8_bgate_fxv_patch_all(true);
	ff8fx::g_active = true;
	int rb = 0;
	bool ok = true;
	uint32_t pool = *(uint32_t *)0x1D999C4;
	ff8_bgate_fxv_scrub_stack();
	// the FPU control word is part of the state: the GTE emulation rounds with x87 maths, and
	// the original run's real engine calls (sound, loader) can change it mid-tick
	ff8_bgate_x87_set_cw(cw_a0);
	__try { rb = orig(queue); }
	__except (ff8_bgate_gfc_fault_filter(GetExceptionInformation())) { ok = false; }
	*(uint32_t *)0x1D999C4 = pool;
	ff8fx::g_active = false;
	uint16_t cw_b1 = ff8_bgate_x87_cw();
	ff8_bgate_x87_set_cw(cw_a1); // leave the FPU as the original run left it
	ff8_bgate_fxv_patch_all(false);
	ff8_bgate_fxv_ext_mode = FXV_EXT_OFF;
	ff8_bgate_etq_rec = false;
	ff8_bgate_fxv_capture(ff8_bgate_fxv_b, queue);

	const char *diff = ok ? ff8_bgate_fxv_compare(ra, rb) : "port FAULTED";
	if (false && ff8_bgate_fxv_ticks > 340 && g.effect_id == 116)
	{
		char calls[200]; int co = 0; calls[0] = 0;
		for (int i = 0; i < ff8_bgate_fxv_ncalls && co >= 0 && co < 180; i++)
			co += _snprintf_s(calls + co, sizeof(calls) - co, _TRUNCATE, " %s", ff8_bgate_fxv_sites[ff8_bgate_fxv_calls[i].site].name);
		ffnx_info("30fps fxv: t=%u ra=%d rb=%d %s done=%02X loadstate=%d creature=%p calls:%s\n", ff8_bgate_fxv_ticks, ra, rb, diff ? "MISMATCH" : "match",
			*(uint8_t *)0x25217A8, *(int32_t *)0x1D999C8, *(void **)0x2521738, calls);
	}
	if (!ok)
	{
		ff8_bgate_fxv_faults++;
		ffnx_info("30fps fxv: %s tick %u: port fault %08X at %08X (fault %u)\n", g.name, ff8_bgate_fxv_ticks, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr, ff8_bgate_fxv_faults);
	}
	if (!diff)
	{
		ff8_bgate_fxv_match++;
		return rb;
	}
	ff8_bgate_fxv_mismatch++;
	if (!ff8_bgate_fxv_first_bad) ff8_bgate_fxv_first_bad = ff8_bgate_fxv_ticks;
	if (ff8_bgate_fxv_logged < 60)
	{
		ff8_bgate_fxv_logged++;
		char calls[200]; int co = 0; calls[0] = 0;
		for (int i = 0; i < ff8_bgate_fxv_ncalls && co >= 0 && co < 180; i++)
			co += _snprintf_s(calls + co, sizeof(calls) - co, _TRUNCATE, " %s", ff8_bgate_fxv_sites[ff8_bgate_fxv_calls[i].site].name);
		ffnx_info("30fps fxv: %s tick %u MISMATCH: %s | ra=%d rb=%d loader=%02X %02X %02X %02X %02X x87 cw A %04X->%04X B end %04X calls:%s\n", g.name, ff8_bgate_fxv_ticks, diff, ra, rb,
			loader_state[0], loader_state[1], loader_state[2], loader_state[3], loader_state[4], cw_a0, cw_a1, cw_b1, calls);
		if (g.effect_id == 116) // TEMP: first differing lightning-branch segment (pool of 512 x 28 bytes at model buffer + 0x40200)
		{
			const uint8_t *mb = *(const uint8_t **)0x25217AC;
			uint32_t o = 0;
			for (int r = 0; r < ff8_bgate_fxv_nreg; r++)
			{
				if (ff8_bgate_fxv_reg[r].addr == (uint32_t)0x20DFAB8)
				{
					const uint8_t *post = ff8_bgate_fxv_post_regions + o + ((uint32_t)mb + 0x40200 - 0x20DFAB8);
					const uint8_t *cur = mb + 0x40200;
					for (int s = 0; s < 512; s++)
						if (memcmp(post + 28 * s, cur + 28 * s, 16) != 0)
						{
							const int16_t *a = (const int16_t *)(post + 28 * s), *b = (const int16_t *)(cur + 28 * s);
							const int16_t *v = (const int16_t *)(0x1298C70 + 8 * (int32_t)a[0]);
							ffnx_info("30fps fxv:   segment %d vertex %d (%d,%d,%d): original sxy %d,%d otz %d edge %d,%d | port sxy %d,%d otz %d edge %d,%d\n",
								s, a[0], v[0], v[1], v[2], a[2], a[3], a[4], a[6], a[7], b[2], b[3], b[4], b[6], b[7]);
							break;
						}
					break;
				}
				o += ff8_bgate_fxv_reg[r].size;
			}
		}
	}
	ff8_bgate_fxv_put_back_A();
	ff8_bgate_etq_log_n = ff8_bgate_fxv_log_a_n;
	memcpy(ff8_bgate_etq_log, ff8_bgate_fxv_log_a, sizeof(ff8_bgate_task_rng) * ff8_bgate_fxv_log_a_n);
	return ra;
}

static void ff8_bgate_fxv_summary(const char *name)
{
	if (!ff8_bgate_fxv_ticks) return;
	ffnx_info("30fps fxv: %s verified ticks=%u match=%u mismatch=%u (first at tick %u) port faults=%u native held frames=%u\n",
		name, ff8_bgate_fxv_ticks, ff8_bgate_fxv_match, ff8_bgate_fxv_mismatch, ff8_bgate_fxv_first_bad, ff8_bgate_fxv_faults, ff8_bgate_fxv_held_frames);
	ff8_bgate_fxv_held_frames = 0;
	ff8_bgate_fxv_ticks = ff8_bgate_fxv_match = ff8_bgate_fxv_mismatch = ff8_bgate_fxv_faults = ff8_bgate_fxv_first_bad = ff8_bgate_fxv_logged = 0;
}

// Shared gate body for a recorded queue (ff8_bgate_R already selected): real frame =
// tick at native rate and capture its draws; held frame = no tick, replay the draws
// extrapolated. held_ret is what the caller sees on held frames.
static int ff8_bgate_gate_tick(void *ctx, int (__cdecl *orig)(void *), int held_ret)
{
	if (ff8_bgate_fx_bypass)
		return orig(ctx); // F5: stock behaviour (every host frame), no recording, no replay
	if (ff8_bgate_phase == 0)
	{
		// real frame: advance + draw at native rate; the draws it makes are the arena nodes
		// created between these two cursor reads (see the recording note above)
		ff8_bgate_fx_rec_begin = FF8_BGATE_EXEC_CUR;
		ff8_bgate_fx_rec_ot = FF8_BGATE_CUR_OT();
		if (ff8_bgate_R == &ff8_bgate_rec_fx)
		{
			uint32_t *ot = (uint32_t *)ff8_bgate_fx_rec_ot;
			ff8_bgate_fx_ot_probe[0] = ot[1];
			ff8_bgate_fx_ot_probe[1] = ot[4095];
		}
		ff8_bgate_fx_rec_rlist = FF8_BGATE_RLIST_CUR;
		ff8_bgate_fx_replay_ok = true;
		int vq_before = FF8_BGATE_VQ_COUNT;
		if (ff8_bgate_R == &ff8_bgate_rec_fx)
		{
			ff8_bgate_gfc_logging = ff8_bgate_gfc_module_of_effect() >= 0;
			ff8_bgate_gfc_log_n = 0;
			ff8_bgate_gfc_log_overflow = false;
		}
		if (ff8_bgate_R == &ff8_bgate_rec_fx) ff8fx::g_real_tick++;
		bool la_tick = ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_la_real_tick_begin(ctx);
		bool fxv_tick = la_tick && ff8_bgate_fxv_wanted();
		int r = fxv_tick ? ff8_bgate_fxv_tick(ctx, orig) : orig(ctx);
		ff8_bgate_etq_rec = false;
		ff8_bgate_gfc_logging = false;
		ff8_bgate_vq_record(vq_before, FF8_BGATE_VQ_COUNT);
		for (int i = vq_before; i < FF8_BGATE_VQ_COUNT && i < 32; i++)
		{
			uint8_t ty = *FF8_BGATE_VQ_SLOT(i);
			if (ty < 4) ff8_bgate_fx_sum.vq_types[ty]++;
		}
		if (ff8_bgate_R == &ff8_bgate_rec_fx)
		{
			// cleared link = &bucket[i-1]; a bucket that held a node before and is back to its
			// cleared link now was wiped by the effect (its own draws go into other buckets
			// or were prepended after the clear - a still-linked node also means "not cleared")
			uint32_t *ot = (uint32_t *)ff8_bgate_fx_rec_ot;
			bool had1 = ff8_bgate_fx_ot_probe[0] != (uint32_t)&ot[0], had4095 = ff8_bgate_fx_ot_probe[1] != (uint32_t)&ot[4094];
			bool cleared = (had1 && ot[1] == (uint32_t)&ot[0]) || (had4095 && ot[4095] == (uint32_t)&ot[4094]);
			if (cleared) ff8_bgate_fx_otclear = true;
		}
		if (ff8_bgate_R == &ff8_bgate_rec_fx) ff8_bgate_diff_frame(true);
		ff8_bgate_R->last_r = r;
		ff8_bgate_fx_sum.ticks++;
		if (ff8_bgate_R == &ff8_bgate_rec_fx)
			_snprintf_s(ffnx_cap_label, sizeof(ffnx_cap_label), _TRUNCATE, "f%u_REAL_fx%d_t%u", ff8_bgate_frame_no, *(int *)0x1D99A68 + 1, ff8_bgate_fx_sum.ticks);
		// long effects (Eden runs ~1250 ticks): report progress so the diagnostics do not
		// depend on the effect reaching its end
		if (ff8_bgate_vq_trace)
		{
			ffnx_info("30fps frame: f=%u ph=%d parity=%u arena=%08X rlist=%08X nodes=%d\n",
				ff8_bgate_frame_no, ff8_bgate_phase, *(uint8_t *)0x1D96A80, *(uint32_t *)0x1D8E054,
				*(uint32_t *)0x1D8E04C, ff8_bgate_fx_snaps[ff8_bgate_fx_cur].n);
			ff8_bgate_vq_trace--;
		}
		if (ff8_bgate_fx_sum.ticks % 300 == 0)
			ff8_bgate_fx_summary("in progress");
		if (ff8_bgate_R == &ff8_bgate_rec_fx) { ff8_bgate_tlb_real_tick(ctx, r); ff8_bgate_tla_real_tick(ctx, r); }
		if (r == 0)
		{
			// queue empty = effect finished: never ghost-draw past the end, never pair
			// with the next one
			ff8_bgate_fx_summary("finished");
			if (ff8_bgate_R == &ff8_bgate_rec_fx) ff8_bgate_la_finish();
			if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_gfc_ctx)
			{
				char hl[256]; int ho = 0;
				for (int i = 0; i < 73; i++) if (ff8_bgate_gfc_stats.handlers[i] && ho < 240) ho += sprintf(hl + ho, " %d", i);
				ffnx_info("30fps gf: finished, mode=%d real ticks=%u held redraws=%u pure ops replayed=%u bone half-steps=%u particle half-steps=%u model half-steps=%u (faults %u) pool arena=%u bytes log_overflow=%d handlers:%s\n",
					ff8_bgate_fx_mode, ff8_bgate_gfc_tick, ff8_bgate_gfc_stats.redraws, ff8_bgate_gfc_stats.replayed_ops, ff8_bgate_gfc_stats.bones_stepped, ff8_bgate_gfc_stats.particles_stepped, ff8_bgate_gfc_stats.models_stepped, ff8_bgate_gfc_stats.model_faults, ff8_bgate_gfc_stats.arena_bytes, (int)ff8_bgate_gfc_log_overflow, hl);
				memset(&ff8_bgate_gfc_stats, 0, sizeof(ff8_bgate_gfc_stats));
				ff8_bgate_gfc_reset();
			}
			ff8_bgate_fx_replay_ok = false;
			ff8_bgate_fx_otclear = false;
			ff8_bgate_fx_snaps[0].valid = ff8_bgate_fx_snaps[1].valid = false;
		}
		else if (ff8_bgate_fx_replay_ok)
		{
			ff8_bgate_fx_capture(ctx);
			if (ff8_bgate_R == &ff8_bgate_rec_fx)
				ff8_bgate_fx_recapture_pending = true;
			if (la_tick && ff8_bgate_fx_snaps[ff8_bgate_fx_cur].valid)
				ff8_bgate_la_attach_tasks(ff8_bgate_fx_snaps[ff8_bgate_fx_cur]);
			if (la_tick && !fxv_tick && ff8_bgate_fx_snaps[ff8_bgate_fx_cur].valid)
			{
				ff8_bgate_la_check(ff8_bgate_fx_snaps[ff8_bgate_fx_cur]);
				ff8_bgate_la_lookahead(ctx, orig);
			}
		}
		return r;
	}
	if (ff8_bgate_vq_replay_all)
		ff8_bgate_vq_replay();
	// held frame: no advance; draw the in-between pose of the last real frame's primitives
	// (no VRAM command is re-queued: Eden's type-3 copies turned out to be small texture
	// animations inside the texture area, not screen snapshots, and repeating its type-0
	// streaming uploads would only re-upload the same rows)
	if (ff8_bgate_R == &ff8_bgate_rec_fx)
		_snprintf_s(ffnx_cap_label, sizeof(ffnx_cap_label), _TRUNCATE, "f%u_held_fx%d_t%u", ff8_bgate_frame_no, *(int *)0x1D99A68 + 1, ff8_bgate_fx_sum.ticks);
	if (ff8_bgate_R == &ff8_bgate_rec_fx) ff8_bgate_diff_frame(false);
	if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_otclear && ff8_bgate_fx_replay_ok)
		((void (__cdecl *)(void *, int))0x45D530)((void *)FF8_BGATE_CUR_OT(), 4096); // SSIGPU_ClearOrderingTable, as the effect's tick does
	if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_replay_ok && ff8_bgate_fxv_live && ff8_bgate_gfc_enabled && ff8_bgate_fx_mode >= 2)
	{
		int eid = *(int *)0x1D99A68 + 1;
		if (ff8fx::held_ready(eid))
		{
			ff8_bgate_fx_skip_held = true;
			ff8_bgate_fx_replay(ctx);
			ff8_bgate_fx_skip_held = false;
			// the native draw uses the GTE and the scratch stack like a real tick: both are put
			// back so the next real tick starts from exactly the state vanilla would have
			static uint8_t gte_data[0x90], gte_ctrl[0xD0];
			memcpy(gte_data, (void *)0x1CA8A10, sizeof(gte_data));
			memcpy(gte_ctrl, (void *)0x1CA9230, sizeof(gte_ctrl));
			uint32_t pool = *(uint32_t *)0x1D999C4;
			__try { ff8fx::held_draw(eid, ff8_bgate_phase, ff8_bgate_n); }
			__except (ff8_bgate_gfc_fault_filter(GetExceptionInformation()))
			{
				ffnx_info("30fps held: effect %d native held draw FAULT %08X at %08X\n", eid, ff8_bgate_gfc_fault_code, ff8_bgate_gfc_fault_addr);
			}
			*(uint32_t *)0x1D999C4 = pool;
			memcpy((void *)0x1CA8A10, gte_data, sizeof(gte_data));
			memcpy((void *)0x1CA9230, gte_ctrl, sizeof(gte_ctrl));
			ff8_bgate_fxv_held_frames++;
			return held_ret;
		}
	}
	if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_replay_ok && !ff8_bgate_fxv_live && ff8_bgate_la_held_frame(ctx))
		return held_ret;
	if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_replay_ok && !ff8_bgate_fxv_live && ff8_bgate_tla_held_frame(ctx, orig))
		return held_ret;
	if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_replay_ok && !ff8_bgate_fxv_live && ff8_bgate_tlb_held_frame(ctx, orig))
		return held_ret;
	if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_replay_ok && ff8_bgate_gfc_held_frame(ctx, orig))
		return held_ret;
	if (ff8_bgate_fx_replay_ok && ff8_bgate_fx_mode != 3)
		ff8_bgate_fx_replay(ctx);
	return held_ret;
}

static bool ff8_bgate_fx_held_camera(int16_t world[3], int16_t lookat[3])
{
	if (ff8_bgate_phase == 0 || !ff8_bgate_fxv_live || !ff8_bgate_gfc_enabled || ff8_bgate_fx_mode < 2 || !ff8_bgate_fx_replay_ok)
		return false;
	int eid = *(int *)0x1D99A68 + 1;
	if (!ff8fx::held_ready(eid)) return false;
	// predictors may run the module's own camera math through the GTE and the scratch stack:
	// both are put back so the frame and the next real tick see exactly vanilla state
	static uint8_t gte_data[0x90], gte_ctrl[0xD0];
	memcpy(gte_data, (void *)0x1CA8A10, sizeof(gte_data));
	memcpy(gte_ctrl, (void *)0x1CA9230, sizeof(gte_ctrl));
	uint32_t pool = *(uint32_t *)0x1D999C4;
	bool ok = false;
	__try { ok = ff8fx::held_camera(eid, ff8_bgate_phase, ff8_bgate_n, world, lookat); }
	__except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
	*(uint32_t *)0x1D999C4 = pool;
	memcpy((void *)0x1CA8A10, gte_data, sizeof(gte_data));
	memcpy((void *)0x1CA9230, gte_ctrl, sizeof(gte_ctrl));
	return ok;
}

int __cdecl ff8_bgate_effect_tick_gate(void *effect_ctx)
{
	ff8_bgate_R = &ff8_bgate_rec_fx;
	// 1 on held frames: still running (don't let the caller clear the effect pointer)
	int r = ff8_bgate_gate_tick(effect_ctx, ff8_bgate_effect_tick_orig, 1);
	return r;
}

// Hit-effect queue (ExecuteTaskQueue(dword_1D96AA0) @0x500923, TASK_QUEUE 0x209FAA8): fed by
// CreateCameraShakeTask, DispatchEffectCommand, CreateEffectTask* (weapon/monster impact
// effects), the 99/B1 footstep particles, Renzokuken (MAG_141/159/160/161), MAG_332..343.
// Vanilla ticked it once per host frame -> all of that ran 2x at 30fps. Same treatment as
// the magic effect tree: native ticks, extrapolated draws on held frames.
static int (__cdecl *ff8_bgate_eq_tick_orig)(void *) = nullptr;

int __cdecl ff8_bgate_eq_tick_gate(void *queue)
{
	ff8_bgate_R = &ff8_bgate_rec_eq;
	int r = ff8_bgate_gate_tick(queue, ff8_bgate_eq_tick_orig, ff8_bgate_rec_eq.last_r);
	ff8_bgate_R = &ff8_bgate_rec_fx;
	return r;
}

// --- camera shake offsets: hold the last real frame's values on held frames ---
// someUnknownBSCameraOperations (0x5033E0, from BdLink) adds the shake offsets
// (0x1D97710/12/14, int16 x/y/z) to the view translation and then ZEROES them - every
// producer (AnimSeq 96, hit shakes, magic effects) rewrites them each tick. With the
// producers gated to real frames the shake vanished on held frames (half amplitude,
// 30Hz flicker). If nothing wrote them this held frame, re-apply the last real values.
static int (__cdecl *ff8_bgate_camops_orig)() = nullptr;
static uint32_t ff8_bgate_camops_ri = 0;
static int16_t ff8_bgate_shake_last[3] = {0, 0, 0};

int __cdecl ff8_bgate_camops_hook()
{
	int16_t *s = (int16_t *)0x1D97710;
	if (ff8_bgate_phase == 0)
		memcpy(ff8_bgate_shake_last, s, sizeof(ff8_bgate_shake_last));
	else if (s[0] == 0 && s[1] == 0 && s[2] == 0)
		memcpy(s, ff8_bgate_shake_last, sizeof(ff8_bgate_shake_last));
	unreplace_function(ff8_bgate_camops_ri);
	int r = ff8_bgate_camops_orig();
	rereplace_function(ff8_bgate_camops_ri);
	return r;
}

// --- AnimSeq-spawned tasks (TASK_QUEUE_ANIM_SEQ 0x1D986B8, ticked every host frame by
// ExecuteTaskQueue(dword_1D96A8C) @0x500917). Tick fns are DWORD __cdecl f(node*),
// 0 = keep running, 2 = remove. Node task data starts at +0x0C. ---
#define FF8_BGATE_TASK_HOOK(name, addr) \
	static DWORD (__cdecl *ff8_bgate_##name##_orig)(uint8_t *) = (DWORD (__cdecl *)(uint8_t *))(addr); \
	static uint32_t ff8_bgate_##name##_ri = 0; \
	static DWORD ff8_bgate_##name##_call(uint8_t *node) \
	{ \
		unreplace_function(ff8_bgate_##name##_ri); \
		DWORD r = ff8_bgate_##name##_orig(node); \
		rereplace_function(ff8_bgate_##name##_ri); \
		return r; \
	}

// Pure counters with persistent effects: skipping held frames = native pace.
//   84 sine wobble of a stage group (0x501F90), 9F texture toggle timeline (0x5057D0),
//   99/B1 footstep dust + step sounds (0x50F830), 96 camera shake (0x50F6C0 - its offset
//   is held by the camera hook above).
FF8_BGATE_TASK_HOOK(t84, 0x501F90)
FF8_BGATE_TASK_HOOK(t9f, 0x5057D0)
FF8_BGATE_TASK_HOOK(tstep, 0x50F830)
FF8_BGATE_TASK_HOOK(t96, 0x50F6C0)
// 84 is a pure time ratio (sin((4096 - counter*4096/duration) / 4) into the 4 battle entity
// records +6, then counter++): the held frame writes the same formula at counter - 1 +
// phase/n, the next real tick writes the vanilla value again.
DWORD __cdecl ff8_bgate_t84_hook(uint8_t *n)
{
	if (!ff8_bgate_phase) return ff8_bgate_t84_call(n);
	int32_t c = *(int16_t *)(n + 0x0C), d = *(int16_t *)(n + 0x0E);
	if (d <= 0) return 0;
	int32_t q = (((c - 1) * ff8_bgate_n + ff8_bgate_phase) << 12) / (d * ff8_bgate_n);
	int16_t v = (int16_t)((int32_t(__cdecl *)(int32_t))0x56D130)((0x1000 - q) / 4);
	for (uint32_t a = 0x1D98992; a < 0x1D98A42; a += 0x2C) *(int16_t *)a = v;
	return 0;
}
DWORD __cdecl ff8_bgate_t9f_hook(uint8_t *n) { return ff8_bgate_phase ? 0 : ff8_bgate_t9f_call(n); }
DWORD __cdecl ff8_bgate_tstep_hook(uint8_t *n) { return ff8_bgate_phase ? 0 : ff8_bgate_tstep_call(n); }
DWORD __cdecl ff8_bgate_t96_hook(uint8_t *n) { return ff8_bgate_phase ? 0 : ff8_bgate_t96_call(n); }

// AD/AE drag target to attacker bone (0x50F500): snaps the target to the bone EVERY tick
// (so it keeps following the smoothly animated attacker) - only its frames-left dword
// (+0x18) must count at native rate: restore it after held-frame ticks, and never let a
// held tick be the one that ends the task.
FF8_BGATE_TASK_HOOK(tdrag, 0x50F500)
DWORD __cdecl ff8_bgate_tdrag_hook(uint8_t *n)
{
	if (ff8_bgate_phase == 0)
		return ff8_bgate_tdrag_call(n);
	int32_t left = *(int32_t *)(n + 0x18);
	if (left == 1)
		return 0; // this tick would finish it: leave that to the next real frame
	DWORD r = ff8_bgate_tdrag_call(n);
	*(int32_t *)(n + 0x18) = left;
	return r;
}

// 81 restore model part (0x50F0E0): draws the fading part INSIDE the tick, so it must run
// every frame; its fade/lifetime counter (+0x13, 15 ticks) only advances on real frames.
FF8_BGATE_TASK_HOOK(t81, 0x50F0E0)
DWORD __cdecl ff8_bgate_t81_hook(uint8_t *n)
{
	if (ff8_bgate_phase == 0)
		return ff8_bgate_t81_call(n);
	uint8_t state = n[0x12], counter = n[0x13];
	ff8_bgate_t81_call(n);
	n[0x13] = (state == 0) ? 0 : counter; // first tick: init only, count from the next real one
	return 0;
}

// A6 detached model part with physics (0x50F2E0): also draws inside the tick. Its whole
// simulation state is the node (+0x0C..+0x2B: state, 15-tick counter, velocities, spin)
// plus the global DETACHED_PART_SAVED_MATRIX (0x1D99BF8, 32 B incl. position) from which
// the bone matrices are rebuilt every tick - so a held tick is run and then fully undone.
// TRUE 30 FPS: the task draws, THEN integrates (rotate the saved matrix by the spin +0x20,
// position += velocity +0x14/16/18, drag, gravity). The real tick's pre-update state is kept;
// a held frame draws from it with half the spin and half the velocity applied (physics skipped
// through battle_to_update_flags bit 0 for that one call), then the real state is put back.
FF8_BGATE_TASK_HOOK(ta6, 0x50F2E0)
static struct { uint8_t *node; uint8_t state[0x20], mtx[0x20]; bool valid; } ff8_bgate_a6_memo;
DWORD __cdecl ff8_bgate_ta6_hook(uint8_t *n)
{
	if (ff8_bgate_phase == 0)
	{
		ff8_bgate_a6_memo.node = n;
		memcpy(ff8_bgate_a6_memo.state, n + 0x0C, 0x20);
		memcpy(ff8_bgate_a6_memo.mtx, (void *)0x1D99BF8, 0x20);
		ff8_bgate_a6_memo.valid = n[0x12] != 0; // initialised (the first tick copies the bone matrix)
		return ff8_bgate_ta6_call(n);
	}
	if (ff8_bgate_a6_memo.valid && ff8_bgate_a6_memo.node == n && n[0x12] != 0)
	{
		uint8_t node_cur[0x20], mtx_cur[0x20];
		memcpy(node_cur, n + 0x0C, sizeof(node_cur));
		memcpy(mtx_cur, (void *)0x1D99BF8, sizeof(mtx_cur));
		const uint8_t *s = ff8_bgate_a6_memo.state; // node +0x0C..
		memcpy((void *)0x1D99BF8, ff8_bgate_a6_memo.mtx, 0x20);
		int16_t spin = *(const int16_t *)(s + 0x14);
		((void (__cdecl *)(int32_t, void *))0x56CFB0)(spin * ff8_bgate_phase / ff8_bgate_n, (void *)0x1D99BF8); // ApplyXRotation
		int32_t *pos = (int32_t *)0x1D99C0C;
		for (int i = 0; i < 3; i++)
			pos[i] += *(const int16_t *)(s + 0x08 + 2 * i) * ff8_bgate_phase / ff8_bgate_n;
		memcpy(n + 0x0C, s, 0x20); // the counter (fade) and state the real tick drew with
		uint32_t flags = *(uint32_t *)0x1D96A9C;
		*(uint32_t *)0x1D96A9C = flags | 1;
		ff8_bgate_ta6_call(n);
		*(uint32_t *)0x1D96A9C = flags;
		memcpy(n + 0x0C, node_cur, sizeof(node_cur));
		memcpy((void *)0x1D99BF8, mtx_cur, sizeof(mtx_cur));
		return 0;
	}
	uint8_t node_save[0x20], mtx_save[0x20];
	memcpy(node_save, n + 0x0C, sizeof(node_save));
	memcpy(mtx_save, (void *)0x1D99BF8, sizeof(mtx_save));
	ff8_bgate_ta6_call(n);
	memcpy(n + 0x0C, node_save, sizeof(node_save));
	memcpy((void *)0x1D99BF8, mtx_save, sizeof(mtx_save));
	return 0;
}

// --- status-effect timers + Gilgamesh/Angelo countdown: native rate ---
// FFBattleDirector_battleLoop (0x47CCB0) calls computeTimerStatus (0x483470) and
// summonGilgaAngelStartFight (0x482F80) once per LOOP ITERATION = once per host frame:
//  - computeTimerStatus: *timer -= speed (1/2/3 by Slow/normal/Haste) for every status
//    timer (regen/poison/doom/petrify/stop/sleep/shell/protect/reflect...), fires the
//    Regen heal each 60/speed, handles expiry -> every timed status lasted HALF as long;
//  - summonGilgaAngelStartFight: --DEAD_TIMER_TO_SUMMON_GILGA, and every frame it sits
//    at 0 rolls Gilgamesh (12/255) and the Angelo auto-actions -> twice as frequent.
// Both are pure per-tick counters with no cross-frame handshake, so running them on
// real frames only reproduces vanilla exactly. (The old 60fps build disabled the timer
// gate after an Odin crash that was never isolated; the camera-VM gate active in the
// same build - since replaced by the safe camera task gate - is the likelier culprit.)
static void (__cdecl *ff8_bgate_timerstatus_orig)() = nullptr;
static uint32_t ff8_bgate_timerstatus_ri = 0;
static void (__cdecl *ff8_bgate_gilga_orig)() = nullptr;
static uint32_t ff8_bgate_gilga_ri = 0;

void __cdecl ff8_bgate_gilga_hook()
{
	if (ff8_bgate_phase != 0)
		return;
	unreplace_function(ff8_bgate_gilga_ri);
	ff8_bgate_gilga_orig();
	rereplace_function(ff8_bgate_gilga_ri);
}

void __cdecl ff8_bgate_timerstatus_hook()
{
	if (ff8_bgate_phase != 0)
		return;
	unreplace_function(ff8_bgate_timerstatus_ri);
	ff8_bgate_timerstatus_orig();
	rereplace_function(ff8_bgate_timerstatus_ri);
}

// Per-battle state is keyed by addresses (anim_cmd, entity slot, effect ctx) that the next
// battle reuses for DIFFERENT models and effects. Carrying it over crashed the second battle
// of a session: an anim_cmd kept battle 1's "SLOW forced by us" policy, so its frame-count
// variables were halved for an animation that was no longer SLOW (see the C3 var hook).
static void ff8_bgate_battle_reset()
{
	for (ff8_bgate_rec_t *rec : { &ff8_bgate_rec_fx, &ff8_bgate_rec_eq })
	{
		ff8_bgate_R = rec;
		ff8_bgate_fx_summary("battle ended");
		rec->snaps[0].valid = rec->snaps[1].valid = false;
		rec->replay_ok = false;
		rec->blacklist_n = 0;
		rec->last_r = 0;
	}
	ff8_bgate_R = &ff8_bgate_rec_fx;
	ff8_bgate_gfc_reset();
	ff8_bgate_tlb_reset();
	ff8_bgate_tla_reset();
	ff8_bgate_la_reset();
	ff8_bgate_feedback_req = 0;
	ff8_bgate_fx_otclear = false;
	memset(ff8_bgate_shake_last, 0, sizeof(ff8_bgate_shake_last));
	memset(ff8_bgate_anim_policy, 0, sizeof(ff8_bgate_anim_policy));
	memset(ff8_bgate_done_cache, 0, sizeof(ff8_bgate_done_cache));
	memset(ff8_bgate_pose_hist, 0, sizeof(ff8_bgate_pose_hist));
	memset(ff8_bgate_move, 0, sizeof(ff8_bgate_move));
	ff8_bgate_cam_prev1.valid = ff8_bgate_cam_prev2.valid = false;
	ff8_bgate_cam_nudged = false;
	ff8_bgate_inside_queue_anim = false;
	ff8_bgate_tick_skip = false;
	ff8_bgate_hidden_idx = 0;
	ff8_bgate_phase = 0;
	ffnx_info("battle 30fps: new battle, per-battle state reset\n");
}

static void ff8_bgate_install_hooks()
{
	// The one knob: host frames per real battle tick. ff8_limit_fps() drives the battle module
	// at 30fps for limiter 2 and 60fps for limiter 3, against a native 15fps tick.
	ff8_bgate_n = (ff8_fps_limiter >= FPS_LIMITER_60FPS) ? 4 : 2;

	// Battle frame phase driver
	ff8_bgate_bdlink_orig = (int(__cdecl *)())0x500900;
	ff8_bgate_bdlink_ri = replace_function(0x500900, (void *)ff8_bgate_bdlink_hook);

	// UI tick reduction: vanilla 4/frame -> 4/n per frame (native 60 ticks/s either way)
	ff8_bgate_hudupdate_orig = (int(__cdecl *)())0x4A8E30;
	ff8_bgate_huddisplay_orig = (int(__cdecl *)())0x4A84E0;
	ff8_bgate_hudupdate_ri = replace_function(0x4A8E30, (void *)ff8_bgate_hudupdate_hook);
	ff8_bgate_huddisplay_ri = replace_function(0x4A84E0, (void *)ff8_bgate_huddisplay_hook);

	// Fresh-input latch divisor: 4 -> 4/n UI ticks, so the ctx+33 "fresh input" cadence tracks
	// the host frame rate (30fps -> 30/s, 60fps -> 60/s; vanilla PC was 15/s). This is also what
	// restores GF Boost's phase pacing, which keys off that latch rather than the tick counter.
	patch_code_dword(0xB8A3E4, 4 / ff8_bgate_n); // CONST_BattleUI_TicksPerFrame

	// GF Boost phases/window at native pacing (1 latch in n counts)
	ff8_bgate_boost_orig = (void(__cdecl *)())0x56DD70;
	ff8_bgate_boost_ri = replace_function(0x56DD70, (void *)ff8_bgate_boost_hook);

	// Game time / battle countdown at native 60 ticks/s
	ff8_bgate_savemap_tick_orig = (int(__cdecl *)())0x4701B0;
	ff8_bgate_savemap_tick_ri = replace_function(0x4701B0, (void *)ff8_bgate_savemap_tick_hook);

	// Battle model animation: per-anim_cmd policy (forced SLOW interpolation for entities,
	// pose-hold for stage/effect models) + choreography VM 1-in-n
	ff8_bgate_queueanim_orig = (int(__cdecl *)(void *, int))0x509520;
	ff8_bgate_queueanim_ri = replace_function(0x509520, (void *)ff8_bgate_queueanim_hook);
	ff8_bgate_preread_orig = (int(__cdecl *)(void *, void *, int))0x509440;
	ff8_bgate_preread_ri = replace_function(0x509440, (void *)ff8_bgate_preread_hook);
	// choreography-visible frame counts back to native units under forced SLOW
	ff8_bgate_c3var_orig = (int(__cdecl *)(int))0x5044B0;
	ff8_bgate_c3var_ri = replace_function(0x5044B0, (void *)ff8_bgate_c3var_hook);
	ff8_bgate_readanim_orig = (int(__cdecl *)(void *, void *))0x508F90;
	ff8_bgate_readanim_ri = replace_function(0x508F90, (void *)ff8_bgate_readanim_hook);
	ff8_bgate_animseq_upd_orig = (int(__cdecl *)(void *))0x504290;
	ff8_bgate_animseq_upd_ri = replace_function(0x504290, (void *)ff8_bgate_animseq_upd_hook);
	// run-up movement task duration x n (ticks at host rate -> native wall time, smooth)
	ff8_bgate_anim9e_orig = (void *(__cdecl *)(void *, void *, int))0x50F720;
	ff8_bgate_anim9e_ri = replace_function(0x50F720, (void *)ff8_bgate_anim9e_hook);
#if FF8_BGATE_DIAG
	ff8_bgate_diag_render_orig = (char(__cdecl *)(void *))0x502D40;
	ff8_bgate_diag_render_ri = replace_function(0x502D40, (void *)ff8_bgate_diag_render_hook);
#endif

	// Battle camera: gate the whole keyframe player (0x5035E0) to real frames instead of
	// halving its internal time step (imm8 @0x503A80, formerly patched to 16/n).
	//
	// *** DO NOT re-attempt the step-halving patch *** - it caused a genuine engine hang,
	// reproduced twice, hard (window "Not Responding", no exception, no log after the
	// point of entry - confirmed by exhaustive per-line flushed tracing that BdLink_GF
	// itself never returns). ProcessCameraAnimation contains a catch-up loop: "while
	// current_time >= this segment's total_time: advance to the next keyframe segment"
	// (0x5035E9-0x503613). That loop assumes the vanilla step (16); halving it can make
	// current_time never clear a segment whose total_time is <= the halved step, so the
	// loop spins forever re-reading keyframes - a true infinite loop, invisible to SEH
	// (no bad memory access). Triggered reliably by Cure, which calls
	// Battle_PlayCameraAnimation(&MAG_Cure_cameraAnimation) at cast start - that track
	// apparently has a short segment. Any other camera track could hit the same trap.
	//
	// Gating the task instead is unconditionally safe: on real frames the function runs
	// completely unmodified (same step, same segment math as vanilla), so it can only
	// ever do what vanilla already proves safe; on held frames it doesn't run at all and
	// the camera holds its last real position. Costs camera smoothness (steps at native
	// 15fps like models did before the SLOW-interpolation work) in exchange for zero
	// hang risk - the correct trade until camera data can be proven trap-free.
	ff8_bgate_camanim_orig = (int(__cdecl *)(void *))0x5035E0;
	ff8_bgate_camanim_ri = replace_function(0x5035E0, (void *)ff8_bgate_camanim_hook);
	// Camera smoothness recovered separately: extrapolate the render-facing output only
	// (see ff8_bgate_updatecam_hook above) - never re-touches the risky keyframe state.
	ff8_bgate_camseq_orig = (void *(__cdecl *)())0x509610;
	ff8_bgate_camseq_ri = replace_function(0x509610, (void *)ff8_bgate_camseq_hook);
	ff8_bgate_updatecam_orig = (int(__cdecl *)())0x504060;
	ff8_bgate_updatecam_ri = replace_function(0x504060, (void *)ff8_bgate_updatecam_hook);
	// screen feedback request (see ff8_bgate_feedback_request_hook); the 11-byte original is fully replaced
	replace_function(0x47CF50, (void *)ff8_bgate_feedback_request_hook);
	// cinematic-engine GF summons: held-frame 3D redraw (see ff8_bgate_gfc_*)
	ff8_bgate_gfc_install();
	// timeline GF summons: task-tagged draws for the look-ahead pairing (see ff8_bgate_la_*)
	ff8_bgate_etq_orig = (int (__cdecl *)(void *))0x508420;
	ff8_bgate_etq_ri = replace_function(0x508420, (void *)ff8_bgate_etq_hook);
	// native effect ports (src/ff8/battle/fx), verified against the original code while they run
	ff8fx::register_all();
	// the native prim-model player is compared against the original on every call (all effects)
	ff8fx::prim::install_verify();

	// effect packets re-read at display time (packet aliasing, see ff8_bgate_fx_recapture_*)
	ff8_bgate_display_orig = (int(__cdecl *)(unsigned int))0x45D610;
	ff8_bgate_display_ri = replace_function(0x45D610, (void *)ff8_bgate_display_hook);

	// Magic/GF effect pacing: native-rate tick + OT-insert record/replay. This paces the
	// WHOLE effect tree (magic, Draw/Stock, limits, GF summons - Ifrit's dedicated gates
	// are gone, the tick gating covers its entire subsystem) and needs no per-effect
	// knowledge, whitelists or SFX suppression: nothing runs twice, nothing is rewound.
	ff8_bgate_effect_tick_orig = (int(__cdecl *)(void *))get_relative_call(0x50093A, 0);
	replace_call(0x50093A, (void *)ff8_bgate_effect_tick_gate);
	// hit-effect queue: same record/replay pacing
	ff8_bgate_eq_tick_orig = (int(__cdecl *)(void *))get_relative_call(0x500923, 0);
	replace_call(0x500923, (void *)ff8_bgate_eq_tick_gate);
	// camera shake offsets held on held frames
	ff8_bgate_camops_orig = (int(__cdecl *)())0x5033E0;
	ff8_bgate_camops_ri = replace_function(0x5033E0, (void *)ff8_bgate_camops_hook);
	// AnimSeq-spawned tasks at native pace
	ff8_bgate_t84_ri = replace_function(0x501F90, (void *)ff8_bgate_t84_hook);
	ff8_bgate_t9f_ri = replace_function(0x5057D0, (void *)ff8_bgate_t9f_hook);
	ff8_bgate_tstep_ri = replace_function(0x50F830, (void *)ff8_bgate_tstep_hook);
	ff8_bgate_t96_ri = replace_function(0x50F6C0, (void *)ff8_bgate_t96_hook);
	ff8_bgate_tdrag_ri = replace_function(0x50F500, (void *)ff8_bgate_tdrag_hook);
	ff8_bgate_t81_ri = replace_function(0x50F0E0, (void *)ff8_bgate_t81_hook);
	ff8_bgate_ta6_ri = replace_function(0x50F2E0, (void *)ff8_bgate_ta6_hook);

	// Status-effect timers + Gilgamesh/Angelo countdown at native rate
	ff8_bgate_timerstatus_orig = (void(__cdecl *)())0x483470;
	ff8_bgate_timerstatus_ri = replace_function(0x483470, (void *)ff8_bgate_timerstatus_hook);
	ff8_bgate_gilga_orig = (void(__cdecl *)())0x482F80;
	ff8_bgate_gilga_ri = replace_function(0x482F80, (void *)ff8_bgate_gilga_hook);

	ffnx_info("battle %dfps: gates installed (n=%d -> UI %d ticks/frame, input latch %d, camera gated+extrapolated; entity anims %s, effects native-tick + draw replay)\n",
		15 * ff8_bgate_n, ff8_bgate_n, 4 / ff8_bgate_n, 4 / ff8_bgate_n,
		FF8_BGATE_ANIM_LOOKAHEAD ? "vanilla-rate + look-ahead midpoint poses" : "SLOW-interpolated");
}

void* ff8_engine_set_wide_viewport(int x, int y, int w, int h)
{
	*ff8_externals.current_viewport_x_dword_1A7764C = wide_viewport_x;
	*ff8_externals.current_viewport_y_dword_1A77648 = wide_viewport_y;
	*ff8_externals.current_viewport_width_dword_1A77654 = wide_viewport_width;
	*ff8_externals.current_viewport_height_dword_1A77650 = wide_viewport_height;

	*ff8_externals.ssigpu_viewport_x_dword_1CA89D8 = wide_viewport_x;
	*ff8_externals.ssigpu_viewport_y_dword_1CA89DC = wide_viewport_y;
	*ff8_externals.ssigpu_viewport_width_dword_B7CBF8 = wide_viewport_width;
	*ff8_externals.ssigpu_viewport_height_dword_B7CBFC = wide_viewport_height;

	if ( w >= 540 || h >= 380 )
	{
		if ( *ff8_externals.dword_B7CE28 != -1 )
			*ff8_externals.flag_d3d_renderer_related_dword_1CCFD94 = *ff8_externals.dword_B7CE28;
	}
	else
	{
		int tmp = *ff8_externals.flag_d3d_renderer_related_dword_1CCFD94;
		*ff8_externals.flag_d3d_renderer_related_dword_1CCFD94 = 0;
		*ff8_externals.dword_B7CE28 = tmp;
	}

	return ff8_externals.engine_setviewport_sub_41E070(wide_viewport_x, wide_viewport_y, wide_viewport_width, wide_viewport_height, common_externals.get_game_object());
}

void ff8_widescreen_hook_init() {
	// Viewport fixes
	replace_function(ff8_externals.engine_setviewport_sub_45B4C0, ff8_engine_set_wide_viewport);

	// Menu
	ff8_externals.menu_viewport[2].scale_x = 2.0;
	ff8_externals.menu_viewport[2].offset_x = -64.0;
}

void ff8_field_3d_models_push_rects(int a1, uint16_t *a2, int a3, int a4)
{
	field_current_poly = a2;

	ff8_externals.field_push_mch_vertices_rect_sub_533A90(a1, a2, a3, a4);

	field_current_poly = nullptr;
}

void ff8_field_calc_triangle_condition()
{
	ff8_externals.calc_model_triangle_condition_sub_45EE10();

	// The current rect is refused, but this is a rect, with 4 vertices, and only one triangle was checked here (field_current_poly[0], field_current_poly[1], field_current_poly[2])
	if (uint32_t(*ff8_externals.calc_model_poly_condition_result_dword_1CA8A70) >= 1500000) {
		// Retry with another triangle (field_current_poly[1], field_current_poly[2], field_current_poly[3])
		ff8_externals.set_current_triangle_sub_45E160(ff8_externals.dword_1DC6314[field_current_poly[1]], ff8_externals.dword_1DC6314[field_current_poly[2]], ff8_externals.dword_1DC6314[field_current_poly[3]]);
		ff8_externals.calc_model_triangle_condition_sub_45EE10();

		// With this triangle, a negative result is OK, so we override the result for the game to accept it
		*ff8_externals.calc_model_poly_condition_result_dword_1CA8A70 = *ff8_externals.calc_model_poly_condition_result_dword_1CA8A70 > -1500000 && *ff8_externals.calc_model_poly_condition_result_dword_1CA8A70 <= 0 ? 1 : 1500001;
	}

	field_current_poly += 14;
}

void ff8_init_hooks(struct game_obj *_game_object)
{
	struct ff8_game_obj *game_object = (struct ff8_game_obj *)_game_object;

	game_object->dddevice = &_fake_dddevice;
	game_object->front_surface[0] = &_fake_dd_front_surface;
	game_object->front_surface[1] = &_fake_dd_back_surface;
	game_object->dd2interface = &_fake_dddevice;
	game_object->d3d2device = &_fake_d3d2device;

	if (ff8_ssigpu_debug)
		ff8_externals.show_vram_window();

	replace_function(ff8_externals.engine_eval_process_input, ff8_is_window_active);
	replace_function(ff8_externals.get_key_state, ff8_get_key_state);

	replace_function(ff8_externals.swirl_sub_56D390, swirl_sub_56D390);
	replace_call(ff8_externals.worldmap_with_fog_sub_53FAC0 + (FF8_US_VERSION ? 0xB3C: (JP_VERSION ? 0xB24 : 0xB2F)), ff8_wm_set_render_to_vram_current_screen_flag_before_battle);
	replace_function(ff8_externals.set_render_to_vram_current_screen_flag_before_battle, ff8_set_render_to_vram_current_screen_flag_before_battle);
	replace_call(ff8_externals.swirl_enter + 0x9, ff8_swirl_init);

	replace_function(common_externals.destroy_tex_header, ff8_destroy_tex_header);
	replace_function(common_externals.load_tex_file, ff8_load_tex_file);

	replace_function(common_externals.open_file, ff8_open_file);
	replace_call(uint32_t(ff8_externals.fs_archive_search_filename) + 0x10, ff8_fs_archive_search_filename2);
	replace_call(ff8_externals.moriya_filesystem_open + 0x126, ff8_fs_archive_sub_archive_get_filename);
	// Open temp.fs/temp.fl/temp.fi
	replace_call(ff8_externals.moriya_filesystem_open + 0x705, ff8_fs_archive_open_temp);
	// Search file in temp.fs archive (field)
	replace_call(ff8_externals.moriya_filesystem_open + 0x776, ff8_fs_archive_search_filename_sub_archive);
	// Search file in FS archive
	replace_call(ff8_externals.moriya_filesystem_open + 0x83C, ff8_fs_archive_search_filename_sub_archive);
	replace_function(ff8_externals._open, ff8_open);
	replace_function(ff8_externals.fopen, ff8_fopen);
	replace_call(ff8_externals.moriya_filesystem_close + 0x1F, ff8_fs_archive_free_file_container_sub_archive);

	ff8_read_file = (uint32_t(*)(uint32_t, void *, struct ff8_file *))common_externals.read_file;
	ff8_close_file = (void (*)(struct ff8_file *))common_externals.close_file;

	// #####################
	// Adding LZ4 support to FS archives
	// #####################

	// Insert a call to ff8_fs_archive_patch_compression to pass the compression type
	// 83 BD D4 FD FF FF 01|0F 84(addrCompre)|E9(addrUnkComp)
	// 51|E8 (addrPatch)|83 C4 04|E9(addrCompre) 90 90 90 90
	uint32_t read_or_uncompress_fs_data_jump_to_uncompress = *(uint32_t *)(ff8_externals.read_or_uncompress_fs_data + 0x54 + 2);
	memcpy_code(ff8_externals.read_or_uncompress_fs_data + 0x4D, "\x51\xE8\x00\x00\x00\x00\x83\xC4\x04\xE9\x00\x00\x00\x00\x90\x90\x90\x90", 18);
	replace_call(ff8_externals.read_or_uncompress_fs_data + 0x4D + 1, ff8_fs_archive_patch_compression);
	patch_code_dword(ff8_externals.read_or_uncompress_fs_data + 0x4D + 10, read_or_uncompress_fs_data_jump_to_uncompress - 1);
	// Obtain the compressed and the uncompressed sizes
	replace_call(ff8_externals.read_or_uncompress_fs_data + 0x153, ff8_fs_archive_malloc_source_data);
	replace_call(ff8_externals.read_or_uncompress_fs_data + 0x188, ff8_fs_archive_malloc_target_data);
	// Replace the LZS algorithm by LZ4 if compression type is 2
	replace_call(ff8_externals.read_or_uncompress_fs_data + 0x1E6, ff8_fs_archive_uncompress_data);

	memset_code(ff8_externals.movie_hack1, 0x90, 14);
	memset_code(ff8_externals.movie_hack2, 0x90, 8);

	ff8_externals.d3dcaps[0] = true; // Has DDSCAPS_OVERLAY capability in Hardware Emulation Layer + Enable alpha on textures
	ff8_externals.d3dcaps[1] = true; // Enable alpha on textures
	ff8_externals.d3dcaps[2] = false; // Emulate substractive blending via non-paletted texture if enabled
	ff8_externals.d3dcaps[3] = true; // Seems to divide by 2 one vertex if disabled
	ff8_externals.d3dcaps[4] = false;
	*ff8_externals.sub_blending_capability = true;
	patch_code_byte(ff8_externals.sub_45CDD0 + 0x12, 2); // Force comparison to current driver, to enable substractive blending in field fade in/out

	// Fix save format
	if (version == VERSION_FF8_12_FR_NV || version == VERSION_FF8_12_SP_NV || version == VERSION_FF8_12_IT_NV)
	{
		unsigned char ff8fr_savefix1[] = "\xC0\xEA\x03\x8A\x41\x6D\x80\xE2"
																		 "\x01\x24\xFE\x0A\xD0\x88\x51\x6D";
		unsigned char ff8fr_savefix2[] = "\x8A\x50\x6D\xC0\xE9\x03\x80\xE1"
																		 "\x01\x80\xE2\xFE\x0A\xCA\x88\x48"
																		 "\x6D";

		memcpy_code(ff8_externals.sub_53BB90 + 0x952, ff8fr_savefix1, sizeof(ff8fr_savefix1) - 1);
		memcpy_code(ff8_externals.sub_53C750 + 0x8B0, ff8fr_savefix1, sizeof(ff8fr_savefix1) - 1);

		memcpy_code(ff8_externals.sub_544630 + 0xE2, ff8fr_savefix2, sizeof(ff8fr_savefix2) - 1);

		patch_code_byte(ff8_externals.sub_544630 + 0x12F, 0x7D);

		patch_code_byte(ff8_externals.sub_548080 + 0x174, 0x6E);
		patch_code_byte(ff8_externals.sub_548080 + 0x1A3, 0x6E);
		patch_code_byte(ff8_externals.sub_548080 + 0x1C7, 0x6E);
		patch_code_byte(ff8_externals.sub_548080 + 0x1E5, 0x6E);

		patch_code_byte(ff8_externals.sub_549E80 + 0x1CE, 0x6E);

		patch_code_byte(ff8_externals.sub_546100 + 0x952, 0x6D);
		patch_code_byte(ff8_externals.sub_546100 + 0x9A6, 0x74);

		patch_code_byte(ff8_externals.sub_546100 + 0xA23, 0x7C);
		patch_code_byte(ff8_externals.sub_546100 + 0xA67, 0x7C);
		patch_code_byte(ff8_externals.sub_546100 + 0xA90, 0x7C);

		patch_code_byte(ff8_externals.sub_54A0D0 + 0x151, 0x6E);

		patch_code_byte(ff8_externals.sub_54D7E0 + 0xB6, 0x74);
		patch_code_byte(ff8_externals.sub_54D7E0 + 0xE0, 0x74);
		patch_code_byte(ff8_externals.sub_54D7E0 + 0x14A, 0x7C);

		patch_code_byte(ff8_externals.sub_54FDA0 + 0x51, 0x70);
		patch_code_byte(ff8_externals.sub_54FDA0 + 0xB6, 0x70);
		patch_code_byte(ff8_externals.sub_54FDA0 + 0x17F, 0x70);
		patch_code_byte(ff8_externals.sub_54FDA0 + 0x1C7, 0x70);
		patch_code_byte(ff8_externals.sub_54FDA0 + 0x1DB, 0x70);
	}

	// Update the metadata file when a save file is modified
	if (steam_edition)
	{
		replace_call(ff8_externals.main_menu_controller + (JP_VERSION ? 0x1004 : 0xF8D), ff8_create_save_file);
		replace_call(ff8_externals.menu_chocobo_world_controller + 0x9F6, ff8_create_save_file_chocobo_world);
		replace_call(ff8_externals.menu_chocobo_world_controller + (JP_VERSION ? 0xF8D : 0xFA3), ff8_create_save_file_chocobo_world);
		replace_call(ff8_externals.menu_chocobo_world_controller + (JP_VERSION ? 0x11A5 : 0x11BB), ff8_create_save_file_chocobo_world);
		replace_call(ff8_externals.menu_chocobo_world_controller + (JP_VERSION ? 0x13D6 : 0x13EC), ff8_create_save_file_chocobo_world);
	}

	// don't set system speaker config to stereo
	memset_code(common_externals.directsound_create + 0x6D, 0x90, 34);

	if (ff8_externals.nvidia_hack1)
		patch_code_double(ff8_externals.nvidia_hack1, 0.0);
	if (ff8_externals.nvidia_hack2)
		patch_code_float(ff8_externals.nvidia_hack2, 0.0f);

	memcpy_code(ff8_externals.sub_4653B0 + 0xA5, texture_reload_fix1, sizeof(texture_reload_fix1));
	replace_function(ff8_externals.sub_4653B0 + 0xA5 + sizeof(texture_reload_fix1), texture_reload_hack1);

	memcpy_code(ff8_externals.sub_465720 + 0xB3, texture_reload_fix2, sizeof(texture_reload_fix2));
	replace_function(ff8_externals.sub_465720 + 0xB3 + sizeof(texture_reload_fix2), texture_reload_hack2);

	// #####################
	// new timer calibration
	// #####################

	// replace time diff
	replace_function((uint32_t)common_externals.diff_time, qpc_diff_time);

	if (ff8_fps_limiter >= FPS_LIMITER_DEFAULT)
	{
		// replace rdtsc timing
		replace_function((uint32_t)common_externals.get_time, qpc_get_time);

		// override the timer calibration
		QueryPerformanceFrequency((LARGE_INTEGER *)&game_object->_countspersecond);
		game_object->countspersecond = (double)game_object->_countspersecond;

		replace_function(ff8_externals.fps_limiter, ff8_limit_fps);
	}

	// High-frame-rate battle mode: gate the battle subsystems back to native speed so only the
	// menu/input rate rises with the limiter (see the ff8_bgate_* block above). Handles both
	// 30fps (limiter 2, n=2) and 60fps (limiter 3, n=4). US/EN 1.2 addresses only.
	if (ff8_fps_limiter >= FPS_LIMITER_30FPS && FF8_US_VERSION)
	{
		ff8_bgate_active = true;
		ff8_bgate_install_hooks();
	}
#if !FF8_BGATE_DIAG
	else if (FF8_US_VERSION)
	{
		// REFERENCE mode (vanilla 15fps battle): nothing is gated (n = 1 -> every frame is a
		// real frame), only the BdLink hook runs, for its diagnostics log (camera state) - to
		// compare the 30fps build against the original timing.
		ff8_bgate_n = 1;
		ff8_bgate_bdlink_orig = (int(__cdecl *)())0x500900;
		ff8_bgate_bdlink_ri = replace_function(0x500900, (void *)ff8_bgate_bdlink_hook);
		ffnx_info("battle 30fps: REFERENCE mode (limiter < 30fps): no gating, diagnostics only\n");
	}
#endif
#if FF8_BGATE_DIAG
	else if (FF8_US_VERSION)
	{
		// diagnostics-only mode: vanilla behavior at 15fps, animation trace only - for
		// capturing a reference trace to compare choreography timing against the gated build
		ff8_bgate_bdlink_orig = (int(__cdecl *)())0x500900;
		ff8_bgate_bdlink_ri = replace_function(0x500900, (void *)ff8_bgate_bdlink_hook);
		ff8_bgate_queueanim_orig = (int(__cdecl *)(void *, int))0x509520;
		ff8_bgate_queueanim_ri = replace_function(0x509520, (void *)ff8_bgate_queueanim_hook);
		ff8_bgate_animseq_upd_orig = (int(__cdecl *)(void *))0x504290;
		ff8_bgate_animseq_upd_ri = replace_function(0x504290, (void *)ff8_bgate_animseq_upd_hook);
		ff8_bgate_diag_render_orig = (char(__cdecl *)(void *))0x502D40;
		ff8_bgate_diag_render_ri = replace_function(0x502D40, (void *)ff8_bgate_diag_render_hook);
		ffnx_info("battle DIAG-only mode: vanilla flow, animation trace enabled\n");
	}
#endif

	// Gamepad
	replace_function(ff8_externals.dinput_init_gamepad, ff8_init_gamepad);
	replace_function(ff8_externals.dinput_update_gamepad_status, ff8_update_gamepad_status);
	replace_function(ff8_externals.dinput_get_input_device_capabilities_number_of_buttons, ff8_get_input_device_capabilities_number_of_buttons);

	if (steam_stock_launcher)
	{
		// Create ff8input.cfg with the same default values than the FF8_Launcher

		// When the game starts without ff8input.cfg file
		patch_code_byte(ff8_externals.input_init + 0x29, 225); // 226 => 225
		patch_code_byte(ff8_externals.input_init + 0x3C, 224); // 225 => 224
		patch_code_byte(ff8_externals.input_init + 0x53, 226); // 224 => 226
		patch_code_byte(ff8_externals.input_init + 0xA7, 232); // 230 => 232
		patch_code_byte(ff8_externals.input_init + 0xBA, 233); // 231 => 233
		patch_code_byte(ff8_externals.input_init + 0xD1, 230); // 232 => 230
		patch_code_byte(ff8_externals.input_init + 0xE4, 231); // 233 => 231

		// When the player reset the controls in the game menu
		patch_code_byte(ff8_externals.ff8input_cfg_reset + 0xD8, 225); // 226 => 225
		patch_code_byte(ff8_externals.ff8input_cfg_reset + 0xEB, 224); // 225 => 224
		patch_code_byte(ff8_externals.ff8input_cfg_reset + 0x102, 226); // 224 => 226
		patch_code_byte(ff8_externals.ff8input_cfg_reset + 0x156, 232); // 230 => 232
		patch_code_byte(ff8_externals.ff8input_cfg_reset + 0x169, 233); // 231 => 233
		patch_code_byte(ff8_externals.ff8input_cfg_reset + 0x180, 230); // 232 => 230
		patch_code_byte(ff8_externals.ff8input_cfg_reset + 0x193, 231); // 233 => 231
	}

	// #####################
	// Analog 360 patch
	// #####################
	// Field
	replace_call(ff8_externals.sub_4789A0 + (JP_VERSION ? 0x320 : 0x336), ff8_get_analog_value); // Test if available
	replace_call(ff8_externals.sub_4789A0 + (JP_VERSION ? 0x331 : 0x347), ff8_get_analog_value); // lX
	replace_call(ff8_externals.sub_4789A0 + (JP_VERSION ? 0x345 : 0x35B), ff8_get_analog_value); // lY
	// Worldmap
	replace_call(ff8_externals.worldmap_input_update_sub_559240 + (FF8_US_VERSION ? 0xC2 : 0xBF), ff8_get_analog_value_wm); // lX
	replace_call(ff8_externals.worldmap_input_update_sub_559240 + (FF8_US_VERSION ? 0xD2 : 0xCF), ff8_get_analog_value); // lY
	replace_call(ff8_externals.worldmap_input_update_sub_559240 + (FF8_US_VERSION ? 0xE2 : 0xDF), ff8_get_analog_value); // rX
	replace_call(ff8_externals.worldmap_input_update_sub_559240 + (FF8_US_VERSION ? 0xF2 : 0xEF), ff8_get_analog_value); // rY

	// Do not alter worldmap texture UVs (Maki's patch) http://forums.qhimm.com/index.php?topic=16327.0
	if (FF8_US_VERSION)
	{
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x180, 0); // +-2 replaced by 0
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x18A, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x198, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1A2, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1B2, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1BC, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1CC, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1D6, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1E6, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1F0, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1F8, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x202, 0);
	}
	else
	{
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x19F, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1AA, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1BB, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1C6, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1D9, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1E4, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x1F7, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x202, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x215, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x220, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x229, 0);
		patch_code_byte(ff8_externals.worldmap_alter_uv_sub_553B40 + 0x234, 0);
	}

	// #####################
	// battle toggle
	// #####################
	replace_call_function(ff8_externals.sub_4789A0 + (JP_VERSION ? 0x674 : 0x68B), ff8_toggle_battle_field);
	replace_call_function(ff8_externals.battle_trigger_worldmap, ff8_toggle_battle_worldmap);

	// Allow squaresoft logo skip by pressing a button
	patch_code_byte(ff8_externals.load_credits_image + 0x5FD, 0); // if (intro_step >= 0) ...
	// Add FFNx Logo
	replace_call(ff8_externals.credits_main_loop + 0x6D, ff8_credits_main_loop_gfx_begin_scene);
	// Fix credits intro synchronization with the music
	replace_call(ff8_externals.load_credits_image + 0x164, credits_controller_music_play);
	replace_call(ff8_externals.load_credits_image + 0x305, credits_controller_input_call);

	if (!steam_edition) {
		// Look again with the DataDrive specified in the register
		replace_call(ff8_externals.get_disk_number + 0x6E, ff8_retry_configured_drive);
		replace_call(ff8_externals.cdcheck_sub_52F9E0 + 0x15E, ff8_retry_configured_drive);
	}

	// Force SFX IDs for Quezacotl
	patch_code_dword(int(ff8_externals.vibrate_data_summon_quezacotl) - 16, 240030); // 240030 - 240000 + 370 = ID 400
	patch_code_dword(int(ff8_externals.vibrate_data_summon_quezacotl) - 12, 240033); // 240033 - 240000 + 370 = ID 403
	patch_code_dword(int(ff8_externals.vibrate_data_summon_quezacotl) - 8, 240036); // 240036 - 240000 + 370 = ID 406
	patch_code_dword(int(ff8_externals.vibrate_data_summon_quezacotl) - 4, 240039); // 240039 - 240000 + 370 = ID 409

	if (!FF8_US_VERSION && !JP_VERSION) {
		// Fix "New Game" and "Load Game" texts converted to "Doomtrain" and "Alexander" when starting a new game
		replace_call(ff8_externals.main_menu_render_sub_4E5550 + 0x203, ff8_get_text_cached_new_game);
		replace_call(ff8_externals.main_menu_render_sub_4E5550 + 0x222, ff8_get_text_cached_load_game);
	}

	if (ff8_use_gamepad_icons) {
		// Replace the whole function to conditionnally show PlayStation icons or keyboard keys
		replace_function(ff8_externals.ff8_draw_icon_or_key1, ff8_draw_icon_or_key1);
		replace_function(ff8_externals.ff8_draw_icon_or_key2, ff8_draw_icon_or_key2);
		replace_function(ff8_externals.ff8_draw_icon_or_key3, ff8_draw_icon_or_key3);
		replace_function(ff8_externals.ff8_draw_icon_or_key4, ff8_draw_icon_or_key4);
		replace_function(ff8_externals.ff8_draw_icon_or_key5, ff8_draw_icon_or_key5);
		replace_function(ff8_externals.ff8_draw_icon_or_key6, ff8_draw_icon_or_key6);
	}

	// All possible message and ask windows
	ff8_opcode_old_battle = (int (*)(int))ff8_externals.opcode_battle;
	patch_code_dword((uint32_t)&common_externals.execute_opcode_table[0x69], (DWORD)&ff8_opcode_battle);

	//###############################
	// steam achievement unlock calls
	//###############################
	if(enable_steam_achievements)
	{
		// triple triad
		patch_code_dword((uint32_t)&ff8_externals.cardgame_funcs[4], (uint32_t)&ff8_cardgame_postgame_func_534BC0);
		replace_call(ff8_externals.sub_534640 + 0x8D, (void*)ff8_cardgame_enter_hook_sub_460B60);
		replace_call(ff8_externals.sub_534640 + 0x51, (void*)ff8_cardgame_exit_hook_sub_4972A0);
		replace_function(ff8_externals.cardgame_add_card_to_squall_534840, (void*)ff8_cardgame_add_card_to_squall);
		replace_function(ff8_externals.cardgame_update_card_with_location_5347F0, (void*)ff8_cardgame_update_card_with_location);
		patch_code_dword(ff8_externals.cargame_func_535C90 + 0x19, (uint32_t)&ff8_cardgame_sub_535D00);

		// cc master
		patch_code_dword((uint32_t)&common_externals.execute_opcode_table[0x13A], (uint32_t)&ff8_field_opcode_CARDGAME);

		// guardian forces
		replace_function(ff8_externals.enable_gf_sub_47E480, (void*)ff8_enable_gf_sub_47E480);

		// seed rank A (also max GIL)
		replace_call(ff8_externals.menu_sub_4D4D30 + (JP_VERSION ? 0x929 : 0x928), (void*)ff8_update_seed_exp_4C30E0);
		patch_code_dword((uint32_t)&common_externals.execute_opcode_table[0x0D], (uint32_t)&ff8_field_opcode_POPM_W);
		patch_code_dword((uint32_t)&common_externals.execute_opcode_table[0x153], (uint32_t)&ff8_field_opcode_ADDSEEDLEVEL);
		replace_call(common_externals.update_field_entities + 0x120, (void*)ff8_field_update_seed_level);
		replace_call(ff8_externals.worldmap_update_steps_sub_6519D0 + 0x152, (void*)ff8_worldmap_update_seed_level);

		// handyman: upgrade weapon
		replace_call(ff8_externals.menu_junkshop_sub_4EA890 + (JP_VERSION ? 0x5F0 : 0x5C1), (void*)ff8_menu_junkshop_get_char_id_hook_4ABC40);
		replace_call(ff8_externals.menu_junkshop_sub_4EA890 + (JP_VERSION ? 0x63A : 0x60B), (void*)ff8_menu_junkshop_hook_4EA770);

		// max HP
		replace_call(ff8_externals.compute_char_stats_sub_495960 + 0x68, (void*)ff8_hook_sub_4954B0);
		replace_call(ff8_externals.compute_char_stats_sub_495960 + 0x94, (void*)ff8_compute_char_max_hp_496310);

		// max GIL
		replace_call((uint32_t)ff8_externals.menu_callbacks[11].func + 0x1F0, (void*)ff8_menu_shop_sub_4EBE40);
		patch_code_dword((uint32_t)ff8_externals.menu_callbacks[11].func + 0x39, (uint32_t)ff8_menu_shop_sub_4EBE40);
		patch_code_dword((uint32_t)&common_externals.execute_opcode_table[0x151], (uint32_t)&ff8_field_opcode_ADDGIL);

		// max LEVEL
		replace_call(ff8_externals.battle_menu_sub_4A3EE0 + 0x581, (void*)ff8_battle_menu_add_exp_and_bonus_496CB0);

		// kills
		replace_call(ff8_externals.battle_sub_494410 + 0x525, (void*)ff8_battle_after_enemy_kill_sub_494AF0);

		// draw magic from draw points
		replace_call(ff8_externals.opcode_drawpoint + 0x6B7, (void*)ff8_opcode_drawpoint_sub_4A0850);
		replace_call(ff8_externals.sub_54E9B0 + (FF8_US_VERSION ? 0x845 : (FF8_SP_VERSION ? 0x89A : 0x85F)), (void*)ff8_set_drawpoint_state_52D190);

		// draw magic via stock in battle
		replace_call(ff8_externals.battle_sub_48D200 + (FF8_US_VERSION ? 0x354 : (JP_VERSION ? 0x36F : 0x355)), (void*)ff8_battle_get_magic_draw_amount_48FD20);

		// timber maniacs
		patch_code_dword((uint32_t)&common_externals.execute_opcode_table[0x0B], (uint32_t)&ff8_field_opcode_POPM_B);

		// quistis blue magics
		replace_call((uint32_t)ff8_externals.menu_callbacks[2].func + 0x152, (void*)ff8_menu_use_item_sub_4F81F0);
		patch_code_dword((uint32_t)ff8_externals.menu_callbacks[2].func + 0x8, (uint32_t)ff8_menu_use_item_sub_4F81F0);

		// dog trainer rinoa
		replace_call(ff8_externals.field_update_rinoa_limit_breaks_52B320 + 0x5D, (void*)ff8_play_sfx_at_unlock_rinoa_limit_break);
		replace_call(ff8_externals.worldmap_update_steps_sub_6519D0 + 0x225, (void*)ff8_play_sfx_at_unlock_rinoa_limit_break);

		// omega destroyed
		replace_call(ff8_externals.battle_ai_opcode_sub_487DF0 + (FF8_US_VERSION ? 0x216C : (JP_VERSION ? 0x2148 : (FF8_SP_VERSION ? 0x21A0 : 0x2176))), (void*)ff8_obtain_proof_of_omega);

		// pupu side quest
		replace_call(ff8_externals.battle_check_won_sub_486500 + 0x66, (void*)ff8_battle_after_set_result_to_won_sub_494D40);

		// chocobo world
		replace_call(ff8_externals.menu_chocobo_world_controller + (JP_VERSION ? 0x17FE : 0x1814), (void*)ff8_menu_choco_add_item_to_player_47ED00);
		replace_call(ff8_externals.menu_chocobo_world_controller + (JP_VERSION ? 0x13BA : 0x13D0), (void*)ff8_menu_chocobo_sub_4FF8F0);
		// chocobo achievement is implemented in aask opcode (voice section)

		// magazine addict
		replace_function((uint32_t)ff8_externals.add_item_to_player_sub_47ED00, (void*)ff8_add_item_to_player_wrapper);
		replace_call(ff8_externals.menu_shop_sub_4EBE40 + 0x11A7, (void*)ff8_menu_shop_update_gil_and_items);

		// obel lake quest
		replace_call(ff8_externals.worldmap_with_fog_sub_53FAC0 + (FF8_US_VERSION ? 0x3C2 : 0x3C4), (void*)ff8_world_sub_54D7E0);
	}

	// #####################
	// widescreen / uncrop
	// #####################
	if(widescreen_enabled)
		ff8_widescreen_hook_init();

	// #####################
	// 3D model extended memory
	// #####################
	extended_memory = (uint8_t *)driver_malloc(0x1000000); // 16 MB

	if (extended_memory) {
		uint32_t memory_offsets = JP_VERSION ? 0xD6DD60 : 0xB6D060;
		patch_code_dword(memory_offsets + 0xC, uint32_t(extended_memory) + 0x300000);
		patch_code_dword(memory_offsets + 0x18, uint32_t(extended_memory) + 0x80000);
		patch_code_dword(memory_offsets + 0x1C, uint32_t(extended_memory) + 0x80000);
		patch_code_dword(memory_offsets + 0x20, uint32_t(extended_memory) + 0x100000);
		patch_code_dword(memory_offsets + 0x24, uint32_t(extended_memory) + 0x100000);
		patch_code_dword(memory_offsets + 0x2C, uint32_t(extended_memory) + 0x180000);
		patch_code_dword(memory_offsets + 0x30, uint32_t(extended_memory) + 0x180000);
		patch_code_dword(memory_offsets + 0x34, uint32_t(extended_memory) + 0x180000);

		// Extend field data size
		patch_code_dword(ff8_externals.read_field_data + (JP_VERSION ? 0xF64 : 0xED1), uint32_t(extended_memory) + 0x5F0000);
		patch_code_dword(ff8_externals.read_field_data + (JP_VERSION ? 0xF6B : 0xED8), uint32_t(extended_memory) + 0x600000);
	} else {
		ffnx_error("%s: cannot allocate extended_memory\n", __func__);
	}

	// #####################
	// field 3D model holes fix
	// #####################
	replace_call(ff8_externals.sub_530C30 + 0x46A, ff8_field_3d_models_push_rects);
	replace_call(uint32_t(ff8_externals.field_push_mch_vertices_rect_sub_533A90) + 0x4D, ff8_field_calc_triangle_condition);

}

struct ff8_gfx_driver *ff8_load_driver(void* _game_object)
{
	struct ff8_gfx_driver *ret = (ff8_gfx_driver *)external_calloc(1, sizeof(*ret));

	ret->init = common_init;
	ret->cleanup = common_cleanup;
	ret->lock = common_lock;
	ret->unlock = common_unlock;
	ret->flip = common_flip;
	ret->clear = common_clear;
	ret->clear_all= common_clear_all;
	ret->setviewport = common_setviewport;
	ret->setbg = common_setbg;
	ret->prepare_polygon_set = common_prepare_polygon_set;
	ret->load_group = common_externals.generic_load_group;
	ret->setmatrix = common_setmatrix;
	ret->unload_texture = common_unload_texture;
	ret->load_texture = common_load_texture;
	ret->field_54 = ff8gl_field_54;
	ret->field_58 = ff8gl_field_58;
	ret->field_5C = ff8gl_field_5C;
	ret->field_60 = ff8gl_field_60;
	ret->palette_changed = common_palette_changed;
	ret->write_palette = common_write_palette;
	ret->blendmode = common_blendmode;
	ret->light_polygon_set = common_light_polygon_set;
	ret->field_64 = common_field_64;
	ret->setrenderstate = common_setrenderstate;
	ret->_setrenderstate = common_setrenderstate;
	ret->__setrenderstate = common_setrenderstate;
	ret->field__84 = ff8gl_field_84;
	ret->field_88 = ff8gl_field_88;
	ret->field_74 = common_field_74;
	ret->field_78 = common_field_78;
	ret->draw_deferred = common_draw_deferred;
	ret->field_80 = common_field_80;
	ret->field_84 = common_field_84;
	ret->begin_scene = common_begin_scene;
	ret->end_scene = common_end_scene;
	ret->field_90 = common_field_90;
	ret->setrenderstate_flat2D = common_setrenderstate_2D;
	ret->setrenderstate_smooth2D = common_setrenderstate_2D;
	ret->setrenderstate_textured2D = common_setrenderstate_2D;
	ret->setrenderstate_paletted2D = common_setrenderstate_2D;
	ret->_setrenderstate_paletted2D = common_setrenderstate_2D;
	ret->draw_flat2D = common_draw_2D;
	ret->draw_smooth2D = common_draw_2D;
	ret->draw_textured2D = common_draw_2D;
	ret->draw_paletted2D = common_draw_paletted2D;
	ret->setrenderstate_flat3D = common_setrenderstate_3D;
	ret->setrenderstate_smooth3D = common_setrenderstate_3D;
	ret->setrenderstate_textured3D = common_setrenderstate_3D;
	ret->setrenderstate_paletted3D = common_setrenderstate_3D;
	ret->_setrenderstate_paletted3D = common_setrenderstate_3D;
	ret->draw_flat3D = common_draw_3D;
	ret->draw_smooth3D = common_draw_3D;
	ret->draw_textured3D = common_draw_3D;
	ret->draw_paletted3D = common_draw_paletted3D;
	ret->setrenderstate_flatlines = common_setrenderstate_2D;
	ret->setrenderstate_smoothlines = common_setrenderstate_2D;
	ret->draw_flatlines = common_draw_lines;
	ret->draw_smoothlines = common_draw_lines;
	ret->field_EC = common_field_EC;

	return ret;
}
