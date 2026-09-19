/*
 * PocketDC Dreamcast frontend — ROM browser and file I/O.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <kos.h>
#include <dc/maple/controller.h>

#include "input.h"
#include "rom_browser.h"
#include "settings.h"
#include "toast.h"
#include "ui.h"
#include "video.h"

#define DC_BROWSER_LINE_HEIGHT 22
#define DC_BROWSER_LIST_TOP    68
#define DC_BROWSER_LIST_LEFT   8
#define DC_BROWSER_LIST_WIDTH  268
#define DC_BROWSER_PREVIEW_X   288
#define DC_BROWSER_GRID_TOP    68
#define DC_BROWSER_HELP_Y      56
#define DC_BROWSER_GRID_CELL_W 120
#define DC_BROWSER_GRID_CELL_H 108

struct dc_browser_root
{
	const char *path;
	const char *label;
};

static char dc_browser_rom_hint[DC_SETTINGS_LAST_ROM_LEN];
static char dc_browser_recent[DC_SETTINGS_RECENT_MAX][DC_SETTINGS_LAST_ROM_LEN];

static const struct dc_browser_root dc_browser_roots[] = {
	{ "/cd/roms", "GD-ROM" },
	{ "/cd",      "GD-ROM (root)" },
	{ "/sd/roms", "SD Card" },
	{ "/sd",      "SD Card (root)" },
	{ "/ide/roms","IDE / GDEMU" },
	{ "/ide",     "IDE / GDEMU (root)" },
	{ "/pc",      "PC (dcload)" },
	{ NULL,       NULL }
};

static int dc_has_rom_extension(const char *name)
{
	size_t len;

	if (!name)
		return 0;

	len = strlen(name);
	if (len >= 3 && strcasecmp(name + len - 3, ".gb") == 0)
		return 1;
	if (len >= 4 && strcasecmp(name + len - 4, ".gbc") == 0)
		return 1;

	return 0;
}

static int dc_browser_entry_compare(const void *a, const void *b)
{
	const struct dc_browser_entry *ea = a;
	const struct dc_browser_entry *eb = b;

	return strcasecmp(ea->name, eb->name);
}

static void dc_browser_set_covers_path(struct dc_browser *browser)
{
	const char *root = browser->root_path;
	const size_t root_len = strlen(root);

	if (root_len >= 5 && strcmp(root + root_len - 5, "/roms") == 0) {
		snprintf(browser->covers_path, sizeof(browser->covers_path),
			 "%.*s/covers", (int)(root_len - 5), root);
		return;
	}

	snprintf(browser->covers_path, sizeof(browser->covers_path), "%s/covers", root);
}

static void dc_browser_entry_prepare_cover(struct dc_browser *browser,
					   struct dc_browser_entry *entry)
{
	if (!browser || !entry || entry->cover_ready)
		return;

	if (dc_cover_load_for_rom(entry->path, browser->covers_path, entry->is_cgb,
				  entry->cover)) {
		entry->cover_from_file = true;
	} else {
		dc_cover_make_placeholder(entry->title, entry->name, entry->is_cgb,
					  entry->cover);
		entry->cover_from_file = false;
	}

	entry->cover_ready = true;
}

static int dc_browser_root_count(void)
{
	int count = 0;

	while (dc_browser_roots[count].path != NULL)
		count++;

	return count;
}

void dc_browser_init(struct dc_browser *browser)
{
	memset(browser, 0, sizeof(*browser));
	browser->root_index = 0;
	browser->view = DC_BROWSER_VIEW_LIST;
	browser->filter = DC_BROWSER_FILTER_ALL;
	strncpy(browser->root_path, dc_browser_roots[0].path,
		sizeof(browser->root_path) - 1);
	dc_browser_set_covers_path(browser);
}

void dc_browser_apply_persisted(struct dc_browser *browser,
				const struct dc_settings *settings)
{
	const int root_count = dc_browser_root_count();

	if (!browser || !settings)
		return;

	if (settings->browser_root_index >= 0 &&
	    settings->browser_root_index < root_count) {
		browser->root_index = settings->browser_root_index;
		strncpy(browser->root_path,
			dc_browser_roots[browser->root_index].path,
			sizeof(browser->root_path) - 1);
		browser->root_path[sizeof(browser->root_path) - 1] = '\0';
		dc_browser_set_covers_path(browser);
	}

	if (settings->browser_view == DC_BROWSER_VIEW_GRID)
		browser->view = DC_BROWSER_VIEW_GRID;
	else
		browser->view = DC_BROWSER_VIEW_LIST;

	if (settings->last_rom_path[0] != '\0') {
		strncpy(dc_browser_rom_hint, settings->last_rom_path,
			sizeof(dc_browser_rom_hint) - 1);
		dc_browser_rom_hint[sizeof(dc_browser_rom_hint) - 1] = '\0';
	} else {
		dc_browser_rom_hint[0] = '\0';
	}

	memcpy(dc_browser_recent, settings->recent_roms, sizeof(dc_browser_recent));

	if (settings->browser_filter <= DC_BROWSER_FILTER_GBC)
		browser->filter = (enum dc_browser_filter)settings->browser_filter;
	else
		browser->filter = DC_BROWSER_FILTER_ALL;
}

void dc_browser_export_persisted(const struct dc_browser *browser,
				 struct dc_settings *settings)
{
	if (!browser || !settings)
		return;

	settings->browser_root_index = browser->root_index;
	settings->browser_view = (uint8_t)browser->view;
	settings->browser_filter = (uint8_t)browser->filter;
}

const char *dc_browser_device_label(const struct dc_browser *browser)
{
	if (!browser)
		return "";

	return dc_browser_roots[browser->root_index].label;
}

static void dc_browser_update_scroll(struct dc_browser *browser);
static void dc_browser_move_vertical(struct dc_browser *browser, int direction);
static void dc_browser_move_horizontal(struct dc_browser *browser, int direction);
static void dc_browser_rebuild_display(struct dc_browser *browser, int focus_entry);

static const char *dc_browser_filter_label(enum dc_browser_filter filter)
{
	switch (filter) {
	case DC_BROWSER_FILTER_DMG:
		return "DMG";
	case DC_BROWSER_FILTER_GBC:
		return "GBC";
	case DC_BROWSER_FILTER_ALL:
	default:
		return "All";
	}
}

static bool dc_browser_entry_passes_filter(const struct dc_browser_entry *entry,
					   enum dc_browser_filter filter)
{
	if (!entry)
		return false;

	switch (filter) {
	case DC_BROWSER_FILTER_DMG:
		return !entry->is_cgb;
	case DC_BROWSER_FILTER_GBC:
		return entry->is_cgb;
	case DC_BROWSER_FILTER_ALL:
	default:
		return true;
	}
}

static int dc_browser_find_entry_by_path(const struct dc_browser *browser,
					 const char *path)
{
	int i;

	if (!browser || !path || path[0] == '\0')
		return -1;

	for (i = 0; i < browser->count; i++) {
		if (strcmp(browser->entries[i].path, path) == 0)
			return i;
	}

	return -1;
}

static bool dc_browser_display_contains(const struct dc_browser *browser,
					int entry_index)
{
	int i;

	for (i = 0; i < browser->display_count; i++) {
		if (browser->display_map[i] == entry_index)
			return true;
	}

	return false;
}

static const struct dc_browser_entry *dc_browser_selected_entry(
	const struct dc_browser *browser)
{
	if (!browser || browser->display_count <= 0 ||
	    browser->selected < 0 || browser->selected >= browser->display_count)
		return NULL;

	return &browser->entries[browser->display_map[browser->selected]];
}

static void dc_browser_rebuild_display(struct dc_browser *browser, int focus_entry)
{
	int i;
	int r;

	if (!browser)
		return;

	browser->display_count = 0;

	for (r = 0; r < DC_SETTINGS_RECENT_MAX; r++) {
		const int entry_index =
			dc_browser_find_entry_by_path(browser,
						      dc_browser_recent[r]);

		if (entry_index < 0)
			continue;
		if (!dc_browser_entry_passes_filter(
			    &browser->entries[entry_index], browser->filter))
			continue;
		if (dc_browser_display_contains(browser, entry_index))
			continue;

		browser->display_map[browser->display_count] = entry_index;
		browser->display_recent[browser->display_count] = true;
		browser->display_count++;
	}

	for (i = 0; i < browser->count; i++) {
		if (!dc_browser_entry_passes_filter(&browser->entries[i],
						    browser->filter))
			continue;
		if (dc_browser_display_contains(browser, i))
			continue;

		browser->display_map[browser->display_count] = i;
		browser->display_recent[browser->display_count] = false;
		browser->display_count++;
	}

	browser->selected = 0;
	for (i = 0; i < browser->display_count; i++) {
		if (browser->display_map[i] == focus_entry) {
			browser->selected = i;
			break;
		}
	}
}

static void dc_browser_clamp_selected(struct dc_browser *browser)
{
	if (browser->display_count <= 0) {
		browser->selected = 0;
		browser->scroll = 0;
		return;
	}

	if (browser->selected < 0)
		browser->selected = 0;
	if (browser->selected >= browser->display_count)
		browser->selected = browser->display_count - 1;
}

static void dc_browser_restore_selection(struct dc_browser *browser,
					 const char *previous_path,
					 const char *previous_name,
					 int previous_selected)
{
	int i;
	bool found = false;

	if (previous_path[0] != '\0') {
		for (i = 0; i < browser->count; i++) {
			if (strcmp(browser->entries[i].path, previous_path) == 0) {
				browser->selected = i;
				found = true;
				break;
			}
		}
	}

	if (!found && previous_name[0] != '\0') {
		for (i = 0; i < browser->count; i++) {
			if (strcasecmp(browser->entries[i].name, previous_name) == 0) {
				browser->selected = i;
				found = true;
				break;
			}
		}
	}

	if (!found && previous_selected >= 0 && previous_selected < browser->count)
		browser->selected = previous_selected;
}

static void dc_browser_select_rom_hint(struct dc_browser *browser)
{
	int i;

	if (!browser || dc_browser_rom_hint[0] == '\0')
		return;

	for (i = 0; i < browser->count; i++) {
		if (strcmp(browser->entries[i].path, dc_browser_rom_hint) == 0) {
			browser->selected = i;
			return;
		}
	}
}

int dc_browser_scan(struct dc_browser *browser)
{
	DIR *dir;
	struct dirent *entry;
	char previous_path[sizeof(browser->entries[0].path)];
	char previous_name[sizeof(browser->entries[0].name)];
	int count = 0;
	int previous_entry = -1;
	bool truncated = false;

	previous_path[0] = '\0';
	previous_name[0] = '\0';
	if (browser->display_count > 0 && browser->selected >= 0 &&
	    browser->selected < browser->display_count)
		previous_entry = browser->display_map[browser->selected];
	else if (browser->selected >= 0 && browser->selected < browser->count)
		previous_entry = browser->selected;

	if (previous_entry >= 0 && previous_entry < browser->count) {
		strncpy(previous_path, browser->entries[previous_entry].path,
			sizeof(previous_path) - 1);
		previous_path[sizeof(previous_path) - 1] = '\0';
		strncpy(previous_name, browser->entries[previous_entry].name,
			sizeof(previous_name) - 1);
		previous_name[sizeof(previous_name) - 1] = '\0';
	}

	browser->count = 0;
	browser->selected = 0;
	browser->scroll = 0;

	dir = opendir(browser->root_path);
	if (!dir) {
		char line[48];

		snprintf(line, sizeof(line), "No media: %s",
			 dc_browser_device_label(browser));
		dc_toast_show(line, 1800);
		return -1;
	}

	while ((entry = readdir(dir)) != NULL) {
		struct dc_browser_entry *slot;
		const char *name = entry->d_name;

		if (name[0] == '.')
			continue;
		if (!dc_has_rom_extension(name))
			continue;

		if (count >= DC_BROWSER_MAX_ENTRIES) {
			truncated = true;
			continue;
		}

		slot = &browser->entries[count];
		{
			char save_path[256];

			snprintf(slot->path, sizeof(slot->path), "%s/%s",
				 browser->root_path, name);
			strncpy(slot->name, name, sizeof(slot->name) - 1);
			slot->name[sizeof(slot->name) - 1] = '\0';
			slot->cover_ready = false;
			slot->cover_from_file = false;
			slot->cart_name[0] = '\0';
			slot->rom_size[0] = '\0';
			{
				uint8_t cart_type = 0;
				uint8_t rom_size_code = 0;

				if (dc_rom_read_header(slot->path, slot->title,
						       sizeof(slot->title),
						       &slot->is_cgb, &cart_type,
						       &rom_size_code)) {
					dc_rom_format_cart_info(cart_type, rom_size_code,
								slot->cart_name,
								sizeof(slot->cart_name),
								slot->rom_size,
								sizeof(slot->rom_size));
				} else {
					strncpy(slot->title, "Unknown",
						sizeof(slot->title) - 1);
					slot->title[sizeof(slot->title) - 1] = '\0';
					snprintf(slot->cart_name, sizeof(slot->cart_name),
						 "?");
					snprintf(slot->rom_size, sizeof(slot->rom_size),
						 "?");
				}
			}
			slot->has_save = false;
			if (dc_save_path_from_rom(slot->path, save_path,
						  sizeof(save_path)) == 0) {
				FILE *save_file = fopen(save_path, "rb");

				if (save_file) {
					slot->has_save = true;
					fclose(save_file);
				}
			}
		}
		count++;
	}

	closedir(dir);
	dc_browser_set_covers_path(browser);
	browser->count = count;
	qsort(browser->entries, (size_t)browser->count, sizeof(browser->entries[0]),
	      dc_browser_entry_compare);

	{
		int focus_entry = previous_entry;

		dc_browser_restore_selection(browser, previous_path, previous_name,
					     previous_entry);
		dc_browser_select_rom_hint(browser);
		if (browser->selected >= 0 && browser->selected < browser->count)
			focus_entry = browser->selected;
		dc_browser_rebuild_display(browser, focus_entry);
	}

	dc_browser_clamp_selected(browser);
	dc_browser_update_scroll(browser);

	{
		char line[48];

		if (truncated)
			dc_toast_show("ROM list truncated (128 max)", 2000);
		else if (count == 0) {
			snprintf(line, sizeof(line), "%s: no ROMs",
				 dc_browser_device_label(browser));
			dc_toast_show(line, 1600);
		} else {
			snprintf(line, sizeof(line), "%s: %d ROM%s",
				 dc_browser_device_label(browser), count,
				 count == 1 ? "" : "s");
			dc_toast_show(line, 1400);
		}
	}

	return count;
}

static void dc_browser_draw_header(const struct dc_browser *browser,
				   uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH])
{
	char title[32];
	char subtitle[96];

	snprintf(title, sizeof(title), "ROM Library (%d)", browser->display_count);
	snprintf(subtitle, sizeof(subtitle), "%s  %s  %s  %s",
		 dc_browser_device_label(browser),
		 dc_browser_filter_label(browser->filter),
		 browser->view == DC_BROWSER_VIEW_GRID ? "Grid" : "List",
		 browser->root_path);
	subtitle[sizeof(subtitle) - 1] = '\0';
	dc_ui_draw_header(screen, title, subtitle);
	dc_ui_draw_text_clipped(screen, DC_UI_MARGIN_X, DC_BROWSER_HELP_Y,
				DC_SCREEN_WIDTH - DC_UI_MARGIN_X * 2,
				"A:Load  B:Device  Y:View  L/R:Page  L+R:Filter  Start:Refresh  X:Back",
				DC_UI_COLOR_DIM, DC_UI_COLOR_BG);
}

static void dc_browser_draw_preview_panel(struct dc_browser *browser,
					uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH])
{
	struct dc_browser_entry *entry;
	char line[80];
	const int cover_x = DC_BROWSER_PREVIEW_X + 70;
	const int cover_y = 72;

	entry = (struct dc_browser_entry *)dc_browser_selected_entry(browser);
	if (!entry)
		return;
	dc_browser_entry_prepare_cover(browser, entry);

	dc_ui_draw_panel(screen, DC_BROWSER_PREVIEW_X, DC_BROWSER_LIST_TOP,
			 DC_SCREEN_WIDTH - DC_BROWSER_PREVIEW_X,
			 DC_UI_FOOTER_Y - DC_BROWSER_LIST_TOP - 8, DC_UI_COLOR_PANEL);
	dc_cover_draw(screen, cover_x, cover_y, 160, 160, entry->cover);
	dc_ui_draw_text(screen, DC_BROWSER_PREVIEW_X + 8, 248, entry->title,
			DC_UI_COLOR_TITLE, DC_UI_COLOR_PANEL);
	dc_ui_draw_text_ellipsis(screen, DC_BROWSER_PREVIEW_X + 8, 272,
				 DC_SCREEN_WIDTH - DC_BROWSER_PREVIEW_X - 16,
				 entry->name, DC_UI_COLOR_FG, DC_UI_COLOR_PANEL);
	snprintf(line, sizeof(line), "%s  %s  %s%s",
		 entry->is_cgb ? "GBC" : "DMG",
		 entry->cart_name[0] ? entry->cart_name : "?",
		 entry->rom_size[0] ? entry->rom_size : "?",
		 entry->has_save ? "  [SAV]" : "");
	dc_ui_draw_text(screen, DC_BROWSER_PREVIEW_X + 8, 296, line,
			DC_UI_COLOR_DIM, DC_UI_COLOR_PANEL);
	dc_ui_draw_text(screen, DC_BROWSER_PREVIEW_X + 8, 316,
			entry->cover_from_file ? "Box art" : "Placeholder",
			DC_UI_COLOR_DIM, DC_UI_COLOR_PANEL);
}

static void dc_browser_draw_list(const struct dc_browser *browser,
				 uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH])
{
	char line[80];
	int i;

	for (i = 0; i < DC_BROWSER_LIST_LINES; i++) {
		const int index = browser->scroll + i;
		const int y = DC_BROWSER_LIST_TOP + i * DC_BROWSER_LINE_HEIGHT;
		uint16_t fg = DC_UI_COLOR_FG;
		uint16_t bg = DC_UI_COLOR_BG;

		if (index >= browser->display_count)
			break;

		{
			const int entry_index = browser->display_map[index];
			const struct dc_browser_entry *entry =
				&browser->entries[entry_index];

			if (index == browser->selected) {
				dc_ui_fill_rect(screen, DC_BROWSER_LIST_LEFT, y - 2,
						DC_BROWSER_LIST_WIDTH,
						DC_BROWSER_LINE_HEIGHT, DC_UI_COLOR_SELECT);
				bg = DC_UI_COLOR_SELECT;
			}

			snprintf(line, sizeof(line), "%c%s %s%s",
				 index == browser->selected ? '>' : ' ',
				 browser->display_recent[index] ? "*" : " ",
				 entry->title, entry->has_save ? " [SAV]" : "");
			dc_ui_draw_text_ellipsis(screen, DC_BROWSER_LIST_LEFT + 8, y,
						DC_BROWSER_LIST_WIDTH - 16, line, fg, bg);
		}
	}

	if (browser->scroll > 0)
		dc_ui_draw_text(screen, DC_BROWSER_LIST_LEFT + DC_BROWSER_LIST_WIDTH - 12,
				DC_BROWSER_LIST_TOP - 12, "^", DC_UI_COLOR_ACCENT,
				DC_UI_COLOR_BG);
	if (browser->scroll + DC_BROWSER_LIST_LINES < browser->display_count)
		dc_ui_draw_text(screen, DC_BROWSER_LIST_LEFT + DC_BROWSER_LIST_WIDTH - 12,
				DC_UI_FOOTER_Y - 16, "v", DC_UI_COLOR_ACCENT,
				DC_UI_COLOR_BG);

	dc_browser_draw_preview_panel((struct dc_browser *)browser, screen);
}

static void dc_browser_draw_grid(const struct dc_browser *browser,
				 uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH])
{
	int row;
	int col;

	for (row = 0; row < DC_BROWSER_GRID_ROWS; row++) {
		for (col = 0; col < DC_BROWSER_GRID_COLS; col++) {
			const int index = browser->scroll + row * DC_BROWSER_GRID_COLS + col;
			const int x = 8 + col * DC_BROWSER_GRID_CELL_W;
			const int y = DC_BROWSER_GRID_TOP + row * DC_BROWSER_GRID_CELL_H;
			struct dc_browser_entry *entry;

			if (index >= browser->display_count)
				break;

			entry = &browser->entries[browser->display_map[index]];
			dc_browser_entry_prepare_cover((struct dc_browser *)browser, entry);

			if (index == browser->selected)
				dc_ui_fill_rect(screen, x - 2, y - 2,
						DC_BROWSER_GRID_CELL_W - 4,
						DC_BROWSER_GRID_CELL_H - 4,
						DC_UI_COLOR_SELECT);

			dc_cover_draw(screen, x + 10, y + 4, 80, 80, entry->cover);
			if (browser->display_recent[index])
				dc_ui_fill_rect(screen, x + 4, y + 4, 8, 8,
						DC_UI_COLOR_ACCENT);
			if (entry->has_save)
				dc_ui_fill_rect(screen, x + 86, y + 4, 8, 8,
						DC_UI_COLOR_SAVE);
			dc_ui_draw_text_ellipsis(screen, x + 4, y + 88,
						DC_BROWSER_GRID_CELL_W - 8,
						entry->title, DC_UI_COLOR_FG, DC_UI_COLOR_BG);
		}
	}
}

static void dc_browser_draw(const struct dc_browser *browser,
			    uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH])
{
	char line[80];

	dc_ui_clear(screen, DC_UI_COLOR_BG);
	dc_browser_draw_header(browser, screen);

	if (browser->count == 0) {
		dc_ui_draw_panel(screen, 72, 108, 496, 168, DC_UI_COLOR_PANEL);
		dc_ui_draw_text(screen, 96, 132, "No ROM files found.",
				DC_UI_COLOR_TITLE, DC_UI_COLOR_PANEL);
		dc_ui_draw_text(screen, 96, 160,
				"Add .gb/.gbc files to this device path,",
				DC_UI_COLOR_FG, DC_UI_COLOR_PANEL);
		dc_ui_draw_text(screen, 96, 184,
				"then press Start to refresh the list.",
				DC_UI_COLOR_FG, DC_UI_COLOR_PANEL);
		dc_ui_draw_text(screen, 96, 220,
				"Box art: covers/boxart/GB|GBC/ROMNAME.w555",
				DC_UI_COLOR_DIM, DC_UI_COLOR_PANEL);
		dc_ui_draw_footer(screen, "B:Next device  Start:Refresh  X:Back");
		dc_toast_draw(screen);
		return;
	}

	if (browser->display_count == 0) {
		dc_ui_draw_panel(screen, 72, 108, 496, 140, DC_UI_COLOR_PANEL);
		dc_ui_draw_text(screen, 96, 132, "No ROMs match this filter.",
				DC_UI_COLOR_TITLE, DC_UI_COLOR_PANEL);
		dc_ui_draw_text(screen, 96, 160,
				"Press L+R to cycle All / DMG / GBC.",
				DC_UI_COLOR_FG, DC_UI_COLOR_PANEL);
		dc_ui_draw_footer(screen, "L+R:Filter  B:Device  X:Back");
		dc_toast_draw(screen);
		return;
	}

	if (browser->view == DC_BROWSER_VIEW_GRID)
		dc_browser_draw_grid(browser, screen);
	else
		dc_browser_draw_list(browser, screen);

	snprintf(line, sizeof(line), "%d / %d  %s", browser->selected + 1,
		 browser->display_count, dc_browser_filter_label(browser->filter));
	dc_ui_draw_footer(screen, line);
	dc_toast_draw(screen);
}

void dc_browser_show_loading(const char *rom_name)
{
	uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH];
	char subtitle[64];
	int frame;

	snprintf(subtitle, sizeof(subtitle), "Loading %s",
		 rom_name ? rom_name : "ROM");
	subtitle[sizeof(subtitle) - 1] = '\0';

	for (frame = 0; frame <= 12; frame++) {
		const int progress = frame * 100 / 12;

		dc_ui_draw_loading(screen, "Starting Game", subtitle, progress);
		dc_video_present_screen(screen);
		timer_spin(DC_INPUT_FRAME_MS);
	}
}

static uint32_t dc_browser_previous_buttons = 0xFFFF;
static int dc_browser_t_up;
static int dc_browser_t_down;
static int dc_browser_t_left;
static int dc_browser_t_right;

static void dc_browser_flush_input(void)
{
	dc_input_flush_edges();
	dc_browser_previous_buttons = 0xFFFF;
	dc_browser_t_up = dc_browser_t_down = 0;
	dc_browser_t_left = dc_browser_t_right = 0;
}

static void dc_browser_poll_input(struct dc_browser *browser,
				  struct dc_browser_input *input)
{
	maple_device_t *controller = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
	uint32_t previous_buttons = dc_browser_previous_buttons;
	int t_up = dc_browser_t_up;
	int t_down = dc_browser_t_down;
	int t_left = dc_browser_t_left;
	int t_right = dc_browser_t_right;
	cont_state_t *pad;
	uint32_t buttons;
	uint32_t changed;
	int vert;
	int horiz;

	(void)browser;
	memset(input, 0, sizeof(*input));
	if (!controller)
		goto release;

	pad = (cont_state_t *)maple_dev_status(controller);
	if (!pad)
		goto release;

	buttons = pad->buttons;
	if (dc_input_quit_combo(buttons))
		arch_exit();
	changed = buttons ^ previous_buttons;

	vert = dc_input_axis((buttons & CONT_DPAD_UP) != 0,
			     (buttons & CONT_DPAD_DOWN) != 0, pad->joyy,
			     DC_INPUT_ANALOG_THRESHOLD);
	horiz = dc_input_axis((buttons & CONT_DPAD_LEFT) != 0,
			      (buttons & CONT_DPAD_RIGHT) != 0, pad->joyx,
			      DC_INPUT_ANALOG_THRESHOLD);

	input->up = dc_input_repeat(vert < 0, &t_up);
	input->down = dc_input_repeat(vert > 0, &t_down);
	input->page_up = dc_input_repeat(horiz < 0, &t_left);
	input->page_down = dc_input_repeat(horiz > 0, &t_right);

	/* Action buttons stay edge-triggered. */
	if ((buttons & CONT_A) && (changed & CONT_A))
		input->select = true;
	if ((buttons & CONT_B) && (changed & CONT_B))
		input->next_device = true;
	if ((buttons & CONT_START) && (changed & CONT_START))
		input->refresh = true;
	if ((buttons & CONT_X) && (changed & CONT_X))
		input->exit = true;
	if ((buttons & CONT_Y) && (changed & CONT_Y))
		input->toggle_view = true;
	if ((buttons & CONT_LTRIGGER) && (buttons & CONT_RTRIGGER) &&
	    (changed & (CONT_LTRIGGER | CONT_RTRIGGER)))
		input->cycle_filter = true;

	dc_browser_previous_buttons = buttons;
	dc_browser_t_up = t_up;
	dc_browser_t_down = t_down;
	dc_browser_t_left = t_left;
	dc_browser_t_right = t_right;
	return;

release:
	dc_browser_t_up = dc_browser_t_down = 0;
	dc_browser_t_left = dc_browser_t_right = 0;
	dc_browser_previous_buttons = 0xFFFF;
}

static int dc_browser_visible_slots(const struct dc_browser *browser)
{
	if (browser->view == DC_BROWSER_VIEW_GRID)
		return DC_BROWSER_GRID_COLS * DC_BROWSER_GRID_ROWS;

	return DC_BROWSER_LIST_LINES;
}

static void dc_browser_move_vertical(struct dc_browser *browser, int direction)
{
	if (browser->display_count <= 0)
		return;

	if (browser->view == DC_BROWSER_VIEW_LIST) {
		browser->selected += direction;
		if (browser->selected < 0)
			browser->selected = browser->display_count - 1;
		else if (browser->selected >= browser->display_count)
			browser->selected = 0;
		return;
	}

	{
		const int cols = DC_BROWSER_GRID_COLS;
		const int col = browser->selected % cols;
		int row = browser->selected / cols;
		const int rows = (browser->display_count + cols - 1) / cols;

		row += direction;
		if (row < 0)
			row = rows - 1;
		else if (row >= rows)
			row = 0;

		browser->selected = row * cols + col;
		if (browser->selected >= browser->display_count)
			browser->selected = browser->display_count - 1;
	}
}

static void dc_browser_move_horizontal(struct dc_browser *browser, int direction)
{
	if (browser->display_count <= 0)
		return;

	if (browser->view == DC_BROWSER_VIEW_LIST) {
		browser->selected += direction * dc_browser_visible_slots(browser);
		if (browser->selected < 0)
			browser->selected = 0;
		else if (browser->selected >= browser->display_count)
			browser->selected = browser->display_count - 1;
		return;
	}

	browser->selected += direction;
	if (browser->selected < 0)
		browser->selected = browser->display_count - 1;
	else if (browser->selected >= browser->display_count)
		browser->selected = 0;
}

static void dc_browser_cycle_filter(struct dc_browser *browser)
{
	int focus_entry = -1;

	if (!browser)
		return;

	if (browser->display_count > 0 && browser->selected >= 0 &&
	    browser->selected < browser->display_count)
		focus_entry = browser->display_map[browser->selected];

	browser->filter = (enum dc_browser_filter)((browser->filter + 1) %
						   (DC_BROWSER_FILTER_GBC + 1));
	dc_browser_rebuild_display(browser, focus_entry);
	dc_browser_clamp_selected(browser);
	dc_browser_update_scroll(browser);
}

static void dc_browser_update_scroll(struct dc_browser *browser)
{
	const int visible = dc_browser_visible_slots(browser);

	if (browser->display_count <= 0) {
		browser->scroll = 0;
		return;
	}

	if (browser->view == DC_BROWSER_VIEW_GRID) {
		const int selected_row = browser->selected / DC_BROWSER_GRID_COLS;
		const int scroll_row = browser->scroll / DC_BROWSER_GRID_COLS;

		if (selected_row < scroll_row)
			browser->scroll = selected_row * DC_BROWSER_GRID_COLS;
		if (selected_row >= scroll_row + DC_BROWSER_GRID_ROWS)
			browser->scroll =
				(selected_row - DC_BROWSER_GRID_ROWS + 1) * DC_BROWSER_GRID_COLS;
		return;
	}

	if (browser->selected < browser->scroll)
		browser->scroll = browser->selected;
	if (browser->selected >= browser->scroll + visible)
		browser->scroll = browser->selected - visible + 1;
}

bool dc_browser_run(struct dc_browser *browser, char *selected_path,
		    size_t selected_len)
{
	uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH];
	bool dirty = true;
	bool toast_visible = false;

	if (!browser || !selected_path || selected_len == 0)
		return false;

	if (dc_browser_scan(browser) < 0)
		printf("pocketdc: unable to scan '%s'\n", browser->root_path);

	dc_browser_flush_input();

	while (1) {
		const uint64_t frame_start = timer_ms_gettime64();
		struct dc_browser_input input;
		const bool toast_active = dc_toast_active();
		uint64_t elapsed;

		if (dirty || toast_active || toast_visible) {
			dc_browser_draw(browser, screen);
			dc_video_present_screen(screen);
			dirty = false;
			toast_visible = toast_active;
		}

		dc_browser_poll_input(browser, &input);

		if (input.exit)
			return false;

		if (input.next_device) {
			browser->root_index++;
			if (!dc_browser_roots[browser->root_index].path)
				browser->root_index = 0;
			strncpy(browser->root_path, dc_browser_roots[browser->root_index].path,
				sizeof(browser->root_path) - 1);
			browser->root_path[sizeof(browser->root_path) - 1] = '\0';
			dc_browser_set_covers_path(browser);
			dc_browser_scan(browser);
			dirty = true;
		}

		if (input.refresh) {
			dc_browser_scan(browser);
			dirty = true;
		}

		if (input.toggle_view) {
			int focus_entry = -1;

			if (browser->display_count > 0 && browser->selected >= 0 &&
			    browser->selected < browser->display_count)
				focus_entry = browser->display_map[browser->selected];

			browser->view = browser->view == DC_BROWSER_VIEW_GRID ?
					DC_BROWSER_VIEW_LIST : DC_BROWSER_VIEW_GRID;
			dc_browser_rebuild_display(browser, focus_entry);
			dc_browser_clamp_selected(browser);
			dc_browser_update_scroll(browser);
			dc_toast_show(browser->view == DC_BROWSER_VIEW_GRID ?
					      "Grid view" : "List view",
				      1000);
			dirty = true;
		}

		if (input.cycle_filter) {
			dc_browser_cycle_filter(browser);
			dc_toast_show(dc_browser_filter_label(browser->filter), 1000);
			dirty = true;
		}

		if (input.up && browser->display_count > 0) {
			dc_browser_move_vertical(browser, -1);
			dirty = true;
		} else if (input.down && browser->display_count > 0) {
			dc_browser_move_vertical(browser, 1);
			dirty = true;
		}

		if (input.page_up && browser->display_count > 0) {
			dc_browser_move_horizontal(browser, -1);
			dirty = true;
		} else if (input.page_down && browser->display_count > 0) {
			dc_browser_move_horizontal(browser, 1);
			dirty = true;
		}

		if (input.select && browser->display_count > 0) {
			const struct dc_browser_entry *entry =
				dc_browser_selected_entry(browser);

			if (!entry)
				continue;

			strncpy(selected_path, entry->path, selected_len - 1);
			selected_path[selected_len - 1] = '\0';
			dc_browser_show_loading(entry->title);
			return true;
		}

		if (dirty)
			dc_browser_update_scroll(browser);

		/* Hold the loop near 60 Hz so auto-repeat timing is stable. */
		elapsed = timer_ms_gettime64() - frame_start;
		if (elapsed < DC_INPUT_FRAME_MS)
			timer_spin((int)(DC_INPUT_FRAME_MS - elapsed));
	}
}

int dc_save_path_from_rom(const char *rom_path, char *save_path, size_t save_path_len)
{
	const char *dot;
	size_t base_len;

	if (!rom_path || !save_path || save_path_len == 0)
		return -1;

	dot = strrchr(rom_path, '.');
	if (!dot || dot == rom_path)
		base_len = strlen(rom_path);
	else
		base_len = (size_t)(dot - rom_path);

	if (base_len + 4 + 1 > save_path_len)
		return -1;

	memcpy(save_path, rom_path, base_len);
	memcpy(save_path + base_len, ".sav", 5);
	return 0;
}

int dc_rom_load(struct dc_priv *priv, const char *rom_path)
{
	FILE *f;
	long size;

	if (!priv || !rom_path)
		return -1;

	f = fopen(rom_path, "rb");
	if (!f) {
		printf("pocketdc: unable to open ROM '%s'\n", rom_path);
		return -1;
	}

	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}

	size = ftell(f);
	if (size < (long)DC_ROM_HEADER_SIZE || size > (8 * 1024 * 1024)) {
		fclose(f);
		printf("pocketdc: invalid ROM size (%ld)\n", size);
		return -1;
	}

	rewind(f);
	priv->rom = (uint8_t *)malloc((size_t)size);
	if (!priv->rom) {
		fclose(f);
		return -1;
	}

	if (fread(priv->rom, 1, (size_t)size, f) != (size_t)size) {
		free(priv->rom);
		priv->rom = NULL;
		fclose(f);
		return -1;
	}

	fclose(f);
	priv->rom_size = (size_t)size;
	strncpy(priv->rom_path, rom_path, sizeof(priv->rom_path) - 1);
	priv->rom_path[sizeof(priv->rom_path) - 1] = '\0';

	if (dc_save_path_from_rom(rom_path, priv->save_path, sizeof(priv->save_path)) != 0)
		priv->save_path[0] = '\0';

	return 0;
}

void dc_rom_unload(struct dc_priv *priv)
{
	if (!priv)
		return;

	free(priv->rom);
	priv->rom = NULL;
	priv->rom_size = 0;
	free(priv->cart_ram);
	priv->cart_ram = NULL;
	free(priv->bootrom);
	priv->bootrom = NULL;
	priv->save_size = 0;
	priv->rom_path[0] = '\0';
	priv->save_path[0] = '\0';
}

int dc_cart_ram_read_file(const char *save_path, uint8_t **dest, size_t len)
{
	FILE *f;

	if (!dest)
		return -1;

	if (len == 0) {
		*dest = NULL;
		return 0;
	}

	*dest = (uint8_t *)calloc(1, len);
	if (!*dest)
		return -1;

	if (!save_path || save_path[0] == '\0')
		return 0;

	f = fopen(save_path, "rb");
	if (!f)
		return 0;

	if (fread(*dest, 1, len, f) != len)
		memset(*dest, 0, len);

	fclose(f);
	return 0;
}

int dc_cart_ram_reload_file(const char *save_path, uint8_t *dest, size_t len)
{
	FILE *f;

	if (!save_path || !dest || len == 0)
		return -1;

	f = fopen(save_path, "rb");
	if (!f)
		return -1;

	if (fread(dest, 1, len, f) != len) {
		fclose(f);
		return -1;
	}

	fclose(f);
	return 0;
}

bool dc_save_file_exists(const char *save_path)
{
	FILE *f;

	if (!save_path || save_path[0] == '\0')
		return false;

	f = fopen(save_path, "rb");
	if (!f)
		return false;

	fclose(f);
	return true;
}

int dc_cart_ram_write_file(const char *save_path, const uint8_t *data, size_t len)
{
	char tmp_path[272];
	FILE *f;

	if (!save_path || !data || len == 0)
		return 0;

	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", save_path);

	f = fopen(tmp_path, "wb");
	if (!f) {
		printf("pocketdc: unable to write save '%s'\n", save_path);
		return -1;
	}

	if (fwrite(data, 1, len, f) != len) {
		fclose(f);
		remove(tmp_path);
		return -1;
	}

	if (fflush(f) != 0) {
		fclose(f);
		remove(tmp_path);
		return -1;
	}

	fclose(f);

	remove(save_path);
	if (rename(tmp_path, save_path) != 0) {
		printf("pocketdc: unable to finalize save '%s'\n", save_path);
		remove(tmp_path);
		return -1;
	}

	return 0;
}
