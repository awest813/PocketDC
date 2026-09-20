/*
 * PocketDC Dreamcast frontend — start, main, pause, and settings menus.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#ifndef DC_MENU_H
#define DC_MENU_H

#include <stdbool.h>

#include "settings.h"

enum dc_main_menu_action
{
	DC_MAIN_MENU_NONE = 0,
	DC_MAIN_MENU_CONTINUE,
	DC_MAIN_MENU_ROM_LIBRARY,
	DC_MAIN_MENU_SETTINGS,
	DC_MAIN_MENU_CONTROLS,
	DC_MAIN_MENU_ABOUT,
	DC_MAIN_MENU_EXIT
};

enum dc_pause_menu_action
{
	DC_PAUSE_MENU_NONE = 0,
	DC_PAUSE_MENU_RESUME,
	DC_PAUSE_MENU_SAVE,
	DC_PAUSE_MENU_LOAD,
	DC_PAUSE_MENU_ERASE,
	DC_PAUSE_MENU_SETTINGS,
	DC_PAUSE_MENU_MAIN_MENU,
	DC_PAUSE_MENU_EXIT
};

bool dc_start_menu_run(void);
enum dc_main_menu_action dc_main_menu_run(const struct dc_settings *settings);
enum dc_pause_menu_action dc_pause_menu_run(const char *rom_title, bool can_save,
					    bool can_load, bool menu_mode);
typedef void (*dc_settings_apply_cb)(struct dc_settings *settings);

bool dc_settings_menu_run(struct dc_settings *settings);
void dc_menu_set_settings_apply_callback(dc_settings_apply_cb callback);
void dc_controls_menu_run(void);
void dc_about_menu_run(void);
void dc_menu_show_message(const char *title, const char *message, int duration_ms);

#endif /* DC_MENU_H */
