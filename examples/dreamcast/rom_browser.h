/*
 * PocketDC Dreamcast frontend — ROM and save file I/O.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#ifndef DC_ROM_BROWSER_H
#define DC_ROM_BROWSER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "cover.h"
#include "dc_priv.h"

struct dc_settings;

#define DC_BROWSER_MAX_ENTRIES 128
#define DC_BROWSER_LIST_LINES    9
#define DC_BROWSER_GRID_COLS     5
#define DC_BROWSER_GRID_ROWS     3

enum dc_browser_view
{
	DC_BROWSER_VIEW_LIST = 0,
	DC_BROWSER_VIEW_GRID
};

enum dc_browser_filter
{
	DC_BROWSER_FILTER_ALL = 0,
	DC_BROWSER_FILTER_DMG,
	DC_BROWSER_FILTER_GBC,
	DC_BROWSER_FILTER_FAV
};

struct dc_browser_entry
{
	char path[256];
	char name[96];
	char title[17];
	char cart_name[16];
	char rom_size[8];
	bool is_cgb;
	bool has_save;
	bool is_zip;
	bool cover_ready;
	bool cover_from_file;
	uint16_t cover[DC_COVER_HEIGHT][DC_COVER_WIDTH];
};

struct dc_browser
{
	char root_path[64];
	char covers_path[128];
	struct dc_browser_entry entries[DC_BROWSER_MAX_ENTRIES];
	int count;
	int selected;
	int scroll;
	int root_index;
	enum dc_browser_view view;
	enum dc_browser_filter filter;
	int display_map[DC_BROWSER_MAX_ENTRIES];
	bool display_recent[DC_BROWSER_MAX_ENTRIES];
	int display_count;
};

struct dc_browser_input
{
	bool up;
	bool down;
	bool select;
	bool next_device;
	bool refresh;
	bool page_up;
	bool page_down;
	bool toggle_view;
	bool cycle_filter;
	bool toggle_favorite;
	bool jump_letter_next;
	bool jump_letter_prev;
	bool exit;
};

int dc_rom_load(struct dc_priv *priv, const char *rom_path);
void dc_rom_unload(struct dc_priv *priv);
int dc_save_path_from_rom(const char *rom_path, char *save_path, size_t save_path_len);
int dc_rtc_path_from_save(const char *save_path, char *rtc_path, size_t rtc_path_len);
int dc_state_path_from_save(const char *save_path, char *state_path,
			    size_t state_path_len);
int dc_cart_ram_read_file(const char *save_path, uint8_t **dest, size_t len);
int dc_cart_ram_reload_file(const char *save_path, uint8_t *dest, size_t len);
int dc_cart_ram_write_file(const char *save_path, const uint8_t *data, size_t len);
int dc_rtc_read_file(const char *rtc_path, uint8_t real[5], uint8_t latched[5],
		     uint32_t *unix_sec);
int dc_rtc_write_file(const char *rtc_path, const uint8_t real[5],
		      const uint8_t latched[5], uint32_t unix_sec);
bool dc_save_file_exists(const char *save_path);

void dc_browser_init(struct dc_browser *browser);
void dc_browser_apply_persisted(struct dc_browser *browser,
				const struct dc_settings *settings);
void dc_browser_export_persisted(const struct dc_browser *browser,
				 struct dc_settings *settings);
int dc_browser_scan(struct dc_browser *browser);
const char *dc_browser_device_label(const struct dc_browser *browser);
bool dc_browser_run(struct dc_browser *browser, struct dc_settings *settings,
		    char *selected_path, size_t selected_len);
void dc_browser_show_loading(const char *rom_name);

#endif /* DC_ROM_BROWSER_H */
