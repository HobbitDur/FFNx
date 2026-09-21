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
	uint32_t ticks, held, interp_held, prims, match, far_, nosig, unparsed, welded, orphans, maxcol, vram_xfer, badcmd;
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

int __cdecl ff8_bgate_bdlink_hook()
{
	if (ff8_bgate_left_battle)
	{
		ff8_bgate_left_battle = false;
		ff8_bgate_battle_reset();
	}
	ff8_bgate_phase = (ff8_bgate_phase + 1) % ff8_bgate_n;
	ff8_bgate_frame_no++;
	{
		static bool f6_down = false;
		bool d6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
		if (d6 && !f6_down)
		{
			ff8_bgate_vq_replay_all = !ff8_bgate_vq_replay_all;
			ffnx_info("30fps fx: F6 -> held-frame VRAM command replay %s\n", ff8_bgate_vq_replay_all ? "ON (all types)" : "off");
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
				ff8_bgate_fx_mode == 0 ? "plain replay" : ff8_bgate_fx_mode == 1 ? "vertices" :
				ff8_bgate_fx_mode == 2 ? "vertices + colors" : ff8_bgate_fx_mode == 3 ? "no replay (held frames draw nothing)" :
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

int __cdecl ff8_bgate_preread_hook(void *anim_header, void *anim_cmd, int animID)
{
	if (ff8_bgate_inside_queue_anim)
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
		ff8_bgate_pose_held(header, anim_cmd, ff8_bgate_phase % div, div);
		return 0; // "frame processed, not complete" -> animation continues
	}
	unreplace_function(ff8_bgate_readanim_ri);
	int r = ff8_bgate_readanim_guarded(header, anim_cmd);
	rereplace_function(ff8_bgate_readanim_ri);
	if (div > 1)
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
		if (complete && dc && !dc->was_complete)
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

static void ff8_bgate_cam_restore()
{
	if (!ff8_bgate_cam_nudged) return;
	ff8_bgate_cam_nudged = false;
	int32_t *g[4] = { (int32_t *)0xB8B7F0, (int32_t *)0xB8B7F4, (int32_t *)0xB8B7F8, (int32_t *)0xB8B7FC };
	for (int i = 0; i < 4; i++)
		if (*g[i] == ff8_bgate_cam_written[i]) // untouched since we wrote it
			*g[i] = ff8_bgate_cam_true[i];
}

int __cdecl ff8_bgate_updatecam_hook()
{
	int32_t *wxz = (int32_t *)0xB8B7F0, *wy = (int32_t *)0xB8B7F4;
	int32_t *lxz = (int32_t *)0xB8B7F8, *ly = (int32_t *)0xB8B7FC;
	int32_t before[4] = { *wxz, *wy, *lxz, *ly };

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

// One real tick's worth of effect draws, copied
struct ff8_bgate_fx_prim
{
	int32_t k[4];
	uint16_t bucket; // index in the battle OT
	uint16_t msk;
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

// dst (a copy of cur) := cur + (cur - prev) * num / den, on vertices and vertex colors
static int ff8_bgate_pkt_extrapolate(uint32_t *dst, const uint32_t *cur, const uint32_t *prev, const ff8_bgate_pkt_info &pi, int num, int den, bool colors)
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
			int v = cc + ff8_bgate_scale_round(cc - pc, num, den);
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
static void ff8_bgate_fx_replay_unsafe()
{
	const ff8_bgate_fx_snap &cur = ff8_bgate_fx_snaps[ff8_bgate_fx_cur];
	const ff8_bgate_fx_snap &prev = ff8_bgate_fx_snaps[ff8_bgate_fx_cur ^ 1];
	// only interpolate against the directly preceding real tick of the same effect
	bool interp = (ff8_bgate_fx_mode == 1 || ff8_bgate_fx_mode == 2) && prev.valid && prev.ctx == cur.ctx && cur.frame - prev.frame == (uint32_t)ff8_bgate_n;
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
	for (int i = 0; i < cur.n; i++)
	{
		const ff8_bgate_fx_prim &e = cur.prim[i];
		const uint32_t *src = &cur.arena[e.off];
		uint32_t *dst = &ff8_bgate_fx_out[out_used];
		memcpy(dst, src, e.words * 4);
		out_used += e.words;

		bool parsed = interp && ff8_bgate_pkt_parse(src, e.words, ci);
		if (interp && !parsed) { st_unparsed++; st_badcmd = ci.last_cmd; ff8_bgate_fx_sum.badcmd = ci.last_cmd; }
		if (parsed && (ci.nxy > 0 || ci.ncol > 0))
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
				for (int c = prev.head_w[ci.sig & (FF8_BGATE_FX_HASH - 1)]; c >= 0; c = prev.prim[c].next_w)
				{
					const ff8_bgate_fx_prim &q = prev.prim[c];
					if (q.sig != ci.sig || q.words != e.words || ff8_bgate_fx_used[c] == ff8_bgate_weld_gen) continue;
					int d = ff8_bgate_pkt_step(src, &prev.arena[q.off], ci);
					if (d < best_d) { best = c; best_d = d; }
				}
			}
			if (best >= 0)
			{
				const uint32_t *qp = &prev.arena[prev.prim[best].off];
				if (best_d <= limit && ff8_bgate_pkt_same_shape(src, qp, ci))
				{
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
	ffnx_info("30fps fx summary (%s %s): effect_id=%d ticks=%u held=%u interpolated=%u | prims=%u paired=%u%% rejected=%u%% new=%u%% unparsed=%u(cmd %02X) vram_xfer_skipped=%u orphans=%u welded_vtx=%u maxcol=%u mode=%d%s\n",
		ff8_bgate_R->name, why, ff8_bgate_R == &ff8_bgate_rec_fx ? *(int *)0x1D99A68 + 1 : -1, ff8_bgate_fx_sum.ticks, ff8_bgate_fx_sum.held, ff8_bgate_fx_sum.interp_held,
		ff8_bgate_fx_sum.prims, (ff8_bgate_fx_sum.match * 100) / pr, (ff8_bgate_fx_sum.far_ * 100) / pr,
		(ff8_bgate_fx_sum.nosig * 100) / pr, ff8_bgate_fx_sum.unparsed, ff8_bgate_fx_sum.badcmd, ff8_bgate_fx_sum.vram_xfer, ff8_bgate_fx_sum.orphans,
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
		int r = orig(ctx);
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
		// long effects (Eden runs ~1250 ticks): report progress so the diagnostics do not
		// depend on the effect reaching its end
		if (ff8_bgate_fx_sum.ticks == 40)
			ff8_bgate_vq_trace = 12; // ~6 real frames of tracing
		if (ff8_bgate_vq_trace)
		{
			ffnx_info("30fps frame: f=%u ph=%d parity=%u arena=%08X rlist=%08X nodes=%d\n",
				ff8_bgate_frame_no, ff8_bgate_phase, *(uint8_t *)0x1D96A80, *(uint32_t *)0x1D8E054,
				*(uint32_t *)0x1D8E04C, ff8_bgate_fx_snaps[ff8_bgate_fx_cur].n);
			ff8_bgate_vq_trace--;
		}
		if (ff8_bgate_fx_sum.ticks % 300 == 0)
			ff8_bgate_fx_summary("in progress");
		if (r == 0)
		{
			// queue empty = effect finished: never ghost-draw past the end, never pair
			// with the next one
			ff8_bgate_fx_summary("finished");
			ff8_bgate_fx_replay_ok = false;
			ff8_bgate_fx_otclear = false;
			ff8_bgate_fx_snaps[0].valid = ff8_bgate_fx_snaps[1].valid = false;
		}
		else if (ff8_bgate_fx_replay_ok)
		{
			ff8_bgate_fx_capture(ctx);
			if (ff8_bgate_R == &ff8_bgate_rec_fx)
				ff8_bgate_fx_recapture_pending = true;
		}
		return r;
	}
	if (ff8_bgate_vq_replay_all)
		ff8_bgate_vq_replay();
	// held frame: no advance; draw the in-between pose of the last real frame's primitives
	// (no VRAM command is re-queued: Eden's type-3 copies turned out to be small texture
	// animations inside the texture area, not screen snapshots, and repeating its type-0
	// streaming uploads would only re-upload the same rows)
	if (ff8_bgate_R == &ff8_bgate_rec_fx) ff8_bgate_diff_frame(false);
	if (ff8_bgate_R == &ff8_bgate_rec_fx && ff8_bgate_fx_otclear && ff8_bgate_fx_replay_ok)
		((void (__cdecl *)(void *, int))0x45D530)((void *)FF8_BGATE_CUR_OT(), 4096); // SSIGPU_ClearOrderingTable, as the effect's tick does
	if (ff8_bgate_fx_replay_ok && ff8_bgate_fx_mode != 3)
		ff8_bgate_fx_replay(ctx);
	return held_ret;
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
DWORD __cdecl ff8_bgate_t84_hook(uint8_t *n) { return ff8_bgate_phase ? 0 : ff8_bgate_t84_call(n); }
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
FF8_BGATE_TASK_HOOK(ta6, 0x50F2E0)
DWORD __cdecl ff8_bgate_ta6_hook(uint8_t *n)
{
	if (ff8_bgate_phase == 0)
		return ff8_bgate_ta6_call(n);
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

	ffnx_info("battle %dfps: gates installed (n=%d -> UI %d ticks/frame, input latch %d, camera gated+extrapolated; entity anims SLOW-interpolated, effects native-tick + draw replay)\n",
		15 * ff8_bgate_n, ff8_bgate_n, 4 / ff8_bgate_n, 4 / ff8_bgate_n);
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
