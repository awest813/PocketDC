/*
 * PocketDC Dreamcast frontend — start, main, pause, and settings menus.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>
#ifdef _arch_dreamcast
#include <malloc.h>
#endif

#include <kos.h>
#include <dc/maple/controller.h>

#include "display.h"
#include "input.h"
#include "menu.h"
#include "palette.h"
#include "settings.h"
#include "toast.h"
#include "ui.h"
#include "video.h"
#include "../../version.all"

#define DC_SETTINGS_LINE_HEIGHT 22
#define DC_SETTINGS_LIST_TOP    72

#define DC_MENU_LINE_HEIGHT    28
#define DC_MENU_LIST_TOP       120

#define DC_CONTROLS_LINE_HEIGHT 20
#define DC_MENU_SPLASH_MS       2500

struct dc_menu_input
{
	bool up;
	bool down;
	bool select;
	bool back;
};

struct dc_menu_list
{
	const char *title;
	const char *subtitle;
	const char *const *items;
	int count;
	int selected;
};

static dc_settings_apply_cb settings_apply_cb;
static uint32_t dc_menu_previous_buttons = 0xFFFF;
static int dc_menu_t_up;
static int dc_menu_t_down;

static void dc_menu_flush_input(void)
{
	dc_input_flush_edges();
	dc_menu_previous_buttons = 0xFFFF;
	dc_menu_t_up = dc_menu_t_down = 0;
}

static void dc_menu_poll_input(struct dc_menu_input *input)
{
	maple_device_t *controller = (maple_device_t *)dc_input_controller();
	uint32_t previous_buttons = dc_menu_previous_buttons;
	int t_up = dc_menu_t_up;
	int t_down = dc_menu_t_down;
	cont_state_t *pad;
	uint32_t buttons;
	uint32_t changed;
	int vert;

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

	input->up = dc_input_repeat(vert < 0, &t_up);
	input->down = dc_input_repeat(vert > 0, &t_down);

	if ((buttons & CONT_A) && (changed & CONT_A))
		input->select = true;
	if ((buttons & CONT_B) && (changed & CONT_B))
		input->back = true;
	if ((buttons & CONT_START) && (changed & CONT_START))
		input->select = true;
	if ((buttons & CONT_X) && (changed & CONT_X))
		input->back = true;

	dc_menu_previous_buttons = buttons;
	dc_menu_t_up = t_up;
	dc_menu_t_down = t_down;
	return;

release:
	dc_menu_t_up = dc_menu_t_down = 0;
	dc_menu_previous_buttons = 0xFFFF;
}

static void dc_menu_draw_list(const struct dc_menu_list *menu,
			      uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
			      const char *footer)
{
	char line[80];
	int i;

	dc_ui_clear(screen, DC_UI_COLOR_BG);
	dc_ui_draw_header(screen, menu->title, menu->subtitle);

	for (i = 0; i < menu->count; i++) {
		const int y = DC_MENU_LIST_TOP + i * DC_MENU_LINE_HEIGHT;
		uint16_t fg = DC_UI_COLOR_FG;
		uint16_t bg = DC_UI_COLOR_BG;

		if (i == menu->selected) {
			dc_ui_fill_rect(screen, 24, y - 2, DC_SCREEN_WIDTH - 48,
					DC_MENU_LINE_HEIGHT, DC_UI_COLOR_SELECT);
			fg = DC_UI_COLOR_FG;
			bg = DC_UI_COLOR_SELECT;
		}

		snprintf(line, sizeof(line), "%c %s",
			 i == menu->selected ? '>' : ' ', menu->items[i]);
		dc_ui_draw_text(screen, 32, y, line, fg, bg);
	}

	if (footer)
		dc_ui_draw_footer(screen, footer);

	dc_toast_draw(screen);
}

static int dc_menu_run_list(struct dc_menu_list *menu, const char *footer)
{
	uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH];
	bool dirty = true;
	bool toast_visible = false;

	dc_menu_flush_input();

	while (1) {
		const uint64_t frame_start = timer_ms_gettime64();
		struct dc_menu_input input;
		const bool toast_active = dc_toast_active();
		uint64_t elapsed;

		if (dirty || toast_active || toast_visible) {
			dc_menu_draw_list(menu, screen, footer);
			dc_video_present_screen(screen);
			dirty = false;
			toast_visible = toast_active;
		}

		dc_menu_poll_input(&input);

		if (input.back)
			return -1;

		if (input.up) {
			menu->selected--;
			if (menu->selected < 0)
				menu->selected = menu->count - 1;
			dirty = true;
		}

		if (input.down) {
			menu->selected++;
			if (menu->selected >= menu->count)
				menu->selected = 0;
			dirty = true;
		}

		if (input.select)
			return menu->selected;

		elapsed = timer_ms_gettime64() - frame_start;
		if (elapsed < DC_INPUT_FRAME_MS)
			timer_spin((int)(DC_INPUT_FRAME_MS - elapsed));
	}
}

static void dc_menu_draw_splash(uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
				const char *prompt)
{
	char version[24];
	char ram_line[32];

	snprintf(version, sizeof(version), "v%d.%d.%d",
		 WALNUTGB_VERSION_MAJOR, WALNUTGB_VERSION_MINOR,
		 WALNUTGB_VERSION_PATCH);
#ifdef _arch_dreamcast
	{
		struct mallinfo mi = mallinfo();

		snprintf(ram_line, sizeof(ram_line), "Heap %dK used",
			 (int)(mi.uordblks / 1024));
	}
#else
	snprintf(ram_line, sizeof(ram_line), "16 MB SH-4");
#endif

	dc_ui_clear(screen, DC_UI_COLOR_BG);
	dc_ui_fill_rect(screen, 0, 168, DC_SCREEN_WIDTH, 4, DC_UI_COLOR_ACCENT);
	dc_ui_draw_text(screen, 176, 188, "PocketDC", DC_UI_COLOR_TITLE, DC_UI_COLOR_BG);
	dc_ui_draw_text(screen, 120, 220, "Game Boy / Game Boy Color",
			DC_UI_COLOR_FG, DC_UI_COLOR_BG);
	dc_ui_draw_text(screen, 168, 248, "Dreamcast Edition", DC_UI_COLOR_DIM,
			DC_UI_COLOR_BG);
	dc_ui_draw_text(screen, 232, 272, version, DC_UI_COLOR_DIM, DC_UI_COLOR_BG);
	dc_ui_draw_text(screen, 200, 296, ram_line, DC_UI_COLOR_DIM, DC_UI_COLOR_BG);
	dc_ui_fill_rect(screen, 0, 324, DC_SCREEN_WIDTH, 4, DC_UI_COLOR_ACCENT);
	dc_ui_draw_text(screen, 200, 360, prompt, DC_UI_COLOR_FG, DC_UI_COLOR_BG);
}

bool dc_start_menu_run(void)
{
	uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH];
	const uint64_t splash_end = timer_ms_gettime64() + DC_MENU_SPLASH_MS;
	bool dirty = true;

	while (1) {
		const uint64_t frame_start = timer_ms_gettime64();
		struct dc_menu_input input;
		uint64_t elapsed;

		if (dirty) {
			dc_menu_draw_splash(screen, "Press A or Start");
			dc_video_present_screen(screen);
			dirty = false;
		}

		dc_menu_poll_input(&input);
		if (input.select)
			return true;

		if (timer_ms_gettime64() >= splash_end)
			return true;

		elapsed = timer_ms_gettime64() - frame_start;
		if (elapsed < DC_INPUT_FRAME_MS)
			timer_spin((int)(DC_INPUT_FRAME_MS - elapsed));
	}
}

void dc_menu_show_message(const char *title, const char *message, int duration_ms)
{
	uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH];
	const uint64_t end_time = timer_ms_gettime64() + (uint64_t)duration_ms;

	dc_menu_flush_input();
	dc_ui_clear(screen, DC_UI_COLOR_BG);
	dc_ui_draw_header(screen, title ? title : "PocketDC", NULL);
	dc_ui_draw_text(screen, 120, 220, message ? message : "", DC_UI_COLOR_FG,
			DC_UI_COLOR_BG);
	dc_ui_draw_footer(screen, "A/B:Continue");

	while (timer_ms_gettime64() < end_time) {
		struct dc_menu_input input;

		dc_video_present_screen(screen);
		dc_menu_poll_input(&input);
		if (input.select || input.back)
			break;
		timer_spin(DC_INPUT_FRAME_MS);
	}
}

static bool dc_menu_confirm(const char *title, const char *message)
{
	const char *items[2];
	struct dc_menu_list menu = {
		.title = title ? title : "Confirm",
		.subtitle = message,
		.items = items,
		.count = 2,
		.selected = 1
	};
	int choice;

	items[0] = "Yes";
	items[1] = "No";
	choice = dc_menu_run_list(&menu, "A:Select  B:No");
	return choice == 0;
}

enum dc_main_menu_action dc_main_menu_run(const struct dc_settings *settings)
{
	char continue_label[DC_SETTINGS_LAST_ROM_LEN + 16];
	const char *items[6];
	struct dc_menu_list menu = {
		.title = "PocketDC",
		.subtitle = "Main Menu",
		.items = items,
		.count = 0,
		.selected = 0
	};

	while (1) {
		const bool show_continue = settings && dc_settings_can_continue(settings);
		int choice;
		enum dc_main_menu_action action;

		menu.count = 0;
		if (show_continue) {
			dc_settings_continue_label(settings, continue_label,
						   sizeof(continue_label));
			items[menu.count++] = continue_label;
		}

		items[menu.count++] = "ROM Library";
		items[menu.count++] = "Settings";
		items[menu.count++] = "Controls";
		items[menu.count++] = "About";
		items[menu.count++] = "Exit";

		choice = dc_menu_run_list(&menu, "A:Select  B:Exit");
		if (choice < 0)
			action = DC_MAIN_MENU_EXIT;
		else if (show_continue) {
			switch (choice) {
			case 0:
				action = DC_MAIN_MENU_CONTINUE;
				break;
			case 1:
				action = DC_MAIN_MENU_ROM_LIBRARY;
				break;
			case 2:
				action = DC_MAIN_MENU_SETTINGS;
				break;
			case 3:
				action = DC_MAIN_MENU_CONTROLS;
				break;
			case 4:
				action = DC_MAIN_MENU_ABOUT;
				break;
			default:
				action = DC_MAIN_MENU_EXIT;
				break;
			}
		} else {
			switch (choice) {
			case 0:
				action = DC_MAIN_MENU_ROM_LIBRARY;
				break;
			case 1:
				action = DC_MAIN_MENU_SETTINGS;
				break;
			case 2:
				action = DC_MAIN_MENU_CONTROLS;
				break;
			case 3:
				action = DC_MAIN_MENU_ABOUT;
				break;
			default:
				action = DC_MAIN_MENU_EXIT;
				break;
			}
		}

		if (action == DC_MAIN_MENU_EXIT) {
			if (dc_menu_confirm("Exit PocketDC", "Return to Dreamcast?"))
				return DC_MAIN_MENU_EXIT;
			continue;
		}

		return action;
	}
}

enum dc_pause_menu_action dc_pause_menu_run(const char *rom_title, bool can_save,
					    bool can_load, bool can_save_state,
					    bool can_load_state, bool menu_mode)
{
	char subtitle[80];
	char paused_line[80];
	const char *items[12];
	struct dc_menu_list menu;
	int choice;
	int save_idx = -1;
	int load_idx = -1;
	int erase_idx = -1;
	int save_state_idx = -1;
	int load_state_idx = -1;
	int settings_idx;
	int main_menu_idx = -1;
	int exit_idx;
	int i = 0;

	snprintf(paused_line, sizeof(paused_line), "Paused: %s",
		 rom_title ? rom_title : "Game");
	paused_line[sizeof(paused_line) - 1] = '\0';
	snprintf(subtitle, sizeof(subtitle), "%.68s", paused_line);
	subtitle[sizeof(subtitle) - 1] = '\0';

	items[i++] = "Resume";
	if (can_save) {
		save_idx = i;
		items[i++] = "Save Game";
	}
	if (can_load) {
		load_idx = i;
		items[i++] = "Load Game";
		erase_idx = i;
		items[i++] = "Erase Save";
	}
	if (can_save_state) {
		save_state_idx = i;
		items[i++] = "Save State";
	}
	if (can_load_state) {
		load_state_idx = i;
		items[i++] = "Load State";
	}
	settings_idx = i;
	items[i++] = "Settings";
	if (menu_mode) {
		main_menu_idx = i;
		items[i++] = "Main Menu";
	}
	exit_idx = i;
	items[i++] = "Exit PocketDC";

	menu.title = "Pause Menu";
	menu.subtitle = subtitle;
	menu.items = items;
	menu.count = i;
	menu.selected = 0;

	choice = dc_menu_run_list(&menu, "A:Select  B:Resume");
	if (choice < 0)
		return DC_PAUSE_MENU_RESUME;

	if (choice == 0)
		return DC_PAUSE_MENU_RESUME;
	if (choice == save_idx)
		return DC_PAUSE_MENU_SAVE;
	if (choice == load_idx)
		return DC_PAUSE_MENU_LOAD;
	if (choice == erase_idx) {
		if (!dc_menu_confirm("Erase Save", "Delete SRAM and clock data?"))
			return DC_PAUSE_MENU_NONE;
		return DC_PAUSE_MENU_ERASE;
	}
	if (choice == save_state_idx)
		return DC_PAUSE_MENU_SAVE_STATE;
	if (choice == load_state_idx)
		return DC_PAUSE_MENU_LOAD_STATE;
	if (choice == settings_idx)
		return DC_PAUSE_MENU_SETTINGS;
	if (choice == main_menu_idx)
		return DC_PAUSE_MENU_MAIN_MENU;
	if (choice == exit_idx)
		return DC_PAUSE_MENU_EXIT;

	return DC_PAUSE_MENU_RESUME;
}

void dc_menu_set_settings_apply_callback(dc_settings_apply_cb callback)
{
	settings_apply_cb = callback;
}

static int dc_controls_count_lines(const char *const *lines, unsigned int count)
{
	unsigned int i;
	int visible = 0;

	for (i = 0; i < count; i++) {
		if (lines[i][0] != '\0')
			visible++;
	}

	return visible;
}

static bool dc_controls_is_section_title(const char *const *lines, unsigned int index)
{
	if (!lines[index][0])
		return false;
	if (strchr(lines[index], '=') != NULL)
		return false;

	return index == 0 || lines[index - 1][0] == '\0';
}

static void dc_controls_draw(uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
			     const char *const *lines, unsigned int count,
			     int scroll)
{
	unsigned int i;
	int line_index = 0;
	int y = DC_UI_CONTENT_TOP;

	dc_ui_clear(screen, DC_UI_COLOR_BG);
	dc_ui_draw_header(screen, "Controls", "Button reference");
	dc_ui_draw_footer(screen, "Up/Dn:Scroll  B:Back");

	for (i = 0; i < count; i++) {
		uint16_t color = DC_UI_COLOR_FG;

		if (lines[i][0] == '\0')
			continue;

		if (line_index < scroll) {
			line_index++;
			continue;
		}

		if (y + 8 > DC_UI_FOOTER_Y)
			break;

		if (dc_controls_is_section_title(lines, i))
			color = DC_UI_COLOR_TITLE;

		dc_ui_draw_text(screen, 24, y, lines[i], color, DC_UI_COLOR_BG);
		y += DC_CONTROLS_LINE_HEIGHT;
		line_index++;
	}
}

void dc_controls_menu_run(void)
{
	static const char *lines[] = {
		"In Game",
		"A/B/Start/X = GB buttons",
		"D-Pad / Analog = GB D-Pad",
		"Start+Y = Pause menu (save/load)",
		"Start+A = Reset game",
		"Start+B = Exit (menu) or quit (CLI)",
		"Start+X = Toggle frameskip",
		"Start+L = Cycle scale mode",
		"Y = Cycle palette",
		"L/R trigger = Fast-forward (2x, both = 4x)",
		"",
		"Pause Menu",
		"Save Game = write .sav alongside ROM",
		"Load Game = reload .sav and reset",
		"Erase Save = delete .sav/.rtc (confirm)",
		"Save State = write .ss0 snapshot (MIT serialize)",
		"Load State = restore .ss0 (same ROM only)",
		"Main Menu = leave game (menu launch)",
		"Exit PocketDC = quit the emulator",
		"Autosave interval in Settings (default 60s)",
		"",
		"Main Menu",
		"Continue = last played ROM (when available)",
		"About = credits and conventions",
		"A+B+X+Y+Start = quit to Dreamcast",
		"",
		"Settings (main or pause menu)",
		"Video output, scale, status bar, audio",
		"Menu music = CDDA tracks on the disc",
		"Autosave on/off and interval",
		"Changes apply immediately",
		"",
		"ROM Library",
		"A = Load  B = Next device  Y = Grid/List",
		"Start+Y = Pin/unpin favorite  L/R = Jump letter",
		"Left/Right = Page  L+R = Filter  Start = Scan  X = Back",
		"* = Recent  + / yellow mark = Favorite",
		".zip ROMs (first .gb/.gbc inside) are listed"
	};
	const unsigned int line_count = sizeof(lines) / sizeof(lines[0]);
	const int visible_lines =
		(DC_UI_FOOTER_Y - DC_UI_CONTENT_TOP) / DC_CONTROLS_LINE_HEIGHT;
	const int total_lines = dc_controls_count_lines(lines, line_count);
	int scroll = 0;
	uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH];
	bool dirty = true;

	dc_menu_flush_input();

	while (1) {
		const uint64_t frame_start = timer_ms_gettime64();
		struct dc_menu_input input;
		uint64_t elapsed;
		int max_scroll;

		max_scroll = total_lines - visible_lines;
		if (max_scroll < 0)
			max_scroll = 0;
		if (scroll > max_scroll)
			scroll = max_scroll;

		if (dirty) {
			dc_controls_draw(screen, lines, line_count, scroll);
			dc_video_present_screen(screen);
			dirty = false;
		}

		dc_menu_poll_input(&input);
		if (input.back)
			return;

		if (input.up) {
			scroll--;
			if (scroll < 0)
				scroll = 0;
			dirty = true;
		}

		if (input.down) {
			scroll++;
			if (scroll > max_scroll)
				scroll = max_scroll;
			dirty = true;
		}

		elapsed = timer_ms_gettime64() - frame_start;
		if (elapsed < DC_INPUT_FRAME_MS)
			timer_spin((int)(DC_INPUT_FRAME_MS - elapsed));
	}
}

void dc_about_menu_run(void)
{
	static const char *lines[] = {
		"PocketDC",
		"Dreamcast Game Boy / GBC emulator",
		"",
		"Core",
		"Walnut-CGB (Peanut-GB lineage)",
		"MiniGB APU for sound",
		"",
		"Frontend",
		"KallistiOS  PVR  AICA  Maple",
		"audio_processor  audio_ring  ini_kv  zip_rom",
		"gb_serialize savestates (.ss0, MIT)",
		"",
		"Conventions",
		"A+B+X+Y+Start = quit to loader",
		"B in menus = back",
		"Start+Y in library = favorite",
		"Disc CDDA plays in menus when present",
		"MBC3 clock stored as .rtc beside .sav",
		"",
		"MIT License. Do not distribute ROMs."
	};
	const unsigned int line_count = sizeof(lines) / sizeof(lines[0]);
	const int visible_lines =
		(DC_UI_FOOTER_Y - DC_UI_CONTENT_TOP) / DC_CONTROLS_LINE_HEIGHT;
	const int total_lines = dc_controls_count_lines(lines, line_count);
	int scroll = 0;
	uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH];
	bool dirty = true;

	dc_menu_flush_input();

	while (1) {
		const uint64_t frame_start = timer_ms_gettime64();
		struct dc_menu_input input;
		uint64_t elapsed;
		int max_scroll;

		max_scroll = total_lines - visible_lines;
		if (max_scroll < 0)
			max_scroll = 0;
		if (scroll > max_scroll)
			scroll = max_scroll;

		if (dirty) {
			unsigned int i;
			int line_index = 0;
			int y = DC_UI_CONTENT_TOP;

			dc_ui_clear(screen, DC_UI_COLOR_BG);
			dc_ui_draw_header(screen, "About", "Credits");
			dc_ui_draw_footer(screen, "Up/Dn:Scroll  B:Back");
			for (i = 0; i < line_count; i++) {
				uint16_t color = DC_UI_COLOR_FG;

				if (lines[i][0] == '\0')
					continue;
				if (line_index < scroll) {
					line_index++;
					continue;
				}
				if (y + 8 > DC_UI_FOOTER_Y)
					break;
				if (dc_controls_is_section_title(lines, i))
					color = DC_UI_COLOR_TITLE;
				dc_ui_draw_text(screen, 24, y, lines[i], color,
						DC_UI_COLOR_BG);
				y += DC_CONTROLS_LINE_HEIGHT;
				line_index++;
			}
			dc_video_present_screen(screen);
			dirty = false;
		}

		dc_menu_poll_input(&input);
		if (input.back)
			return;

		if (input.up) {
			scroll--;
			if (scroll < 0)
				scroll = 0;
			dirty = true;
		}

		if (input.down) {
			scroll++;
			if (scroll > max_scroll)
				scroll = max_scroll;
			dirty = true;
		}

		elapsed = timer_ms_gettime64() - frame_start;
		if (elapsed < DC_INPUT_FRAME_MS)
			timer_spin((int)(DC_INPUT_FRAME_MS - elapsed));
	}
}

static int dc_settings_advance_row(int row, int delta, bool autosave_on)
{
	int next = row;
	int guard = 0;

	do {
		next += delta;
		if (next < 0)
			next = DC_SETTINGS_ROW_COUNT - 1;
		if (next >= DC_SETTINGS_ROW_COUNT)
			next = 0;
		guard++;
	} while (!autosave_on && next == 6 && guard < DC_SETTINGS_ROW_COUNT);

	return next;
}

static void dc_settings_format_row(const struct dc_settings *settings, int row,
				   char *line, size_t line_len, bool selected)
{
	const char *labels[DC_SETTINGS_ROW_COUNT] = {
		"Palette",
		"Video output",
		"Scale mode",
		"Status bar",
		"Frameskip",
		"Autosave",
		"Autosave interval",
		"Volume",
		"Menu music",
		"Audio buffer"
	};
	const char marker = selected ? '>' : ' ';

	switch (row) {
	case 0:
		snprintf(line, line_len, "%c %s: %s", marker, labels[row],
			 dc_palette_name(settings->palette_index));
		break;
	case 1:
		snprintf(line, line_len, "%c %s: %s", marker, labels[row],
			 dc_video_output_name(settings->video_output));
		break;
	case 2:
		snprintf(line, line_len, "%c %s: %s", marker, labels[row],
			 dc_video_scale_mode_name(settings->scale_mode));
		break;
	case 3:
		snprintf(line, line_len, "%c %s: %s", marker, labels[row],
			 settings->status_bar ? "On" : "Off");
		break;
	case 4:
		snprintf(line, line_len, "%c %s: %s", marker, labels[row],
			 settings->frameskip ? "On" : "Off");
		break;
	case 5:
		snprintf(line, line_len, "%c %s: %s", marker, labels[row],
			 settings->autosave_enabled ? "On" : "Off");
		break;
	case 6:
		if (!settings->autosave_enabled) {
			snprintf(line, line_len, "%c %s: n/a", marker, labels[row]);
			break;
		}
		snprintf(line, line_len, "%c %s: %d sec", marker, labels[row],
			 settings->autosave_interval_sec);
		break;
	case 7:
		snprintf(line, line_len, "%c %s: %s%u%%", marker, labels[row],
			 settings->muted ? "Mute " : "",
			 settings->muted ? 0U : settings->volume);
		break;
	case 8:
		snprintf(line, line_len, "%c %s: %s", marker, labels[row],
			 settings->menu_music ? "On" : "Off");
		break;
	default:
		snprintf(line, line_len, "%c %s: %s", marker, labels[row],
			 dc_audio_buffer_name(settings->audio_buffer));
		break;
	}
}

static void dc_settings_draw_value_screen(const struct dc_settings *settings,
					    int selected_row,
					    uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH])
{
	char line[80];
	int row;

	dc_ui_clear(screen, DC_UI_COLOR_BG);
	dc_ui_draw_header(screen, "Settings", "Video, status bar, and audio options.");

	for (row = 0; row < DC_SETTINGS_ROW_COUNT; row++) {
		const int y = DC_SETTINGS_LIST_TOP + row * DC_SETTINGS_LINE_HEIGHT;
		uint16_t fg = DC_UI_COLOR_DIM;
		uint16_t bg = DC_UI_COLOR_BG;

		if (row == 6 && !settings->autosave_enabled)
			fg = DC_UI_COLOR_DIM;
		else if (row == selected_row)
			fg = DC_UI_COLOR_FG;
		else
			fg = DC_UI_COLOR_DIM;

		if (row == selected_row) {
			dc_ui_fill_rect(screen, 24, y - 2, DC_SCREEN_WIDTH - 48,
					DC_SETTINGS_LINE_HEIGHT, DC_UI_COLOR_SELECT);
			bg = DC_UI_COLOR_SELECT;
			fg = DC_UI_COLOR_FG;
		}

		dc_settings_format_row(settings, row, line, sizeof(line),
				       row == selected_row);
		dc_ui_draw_text_clipped(screen, 32, y, DC_SCREEN_WIDTH - 48, line, fg, bg);
	}

	dc_ui_draw_footer(screen, "Up/Dn:Row  L/R:Change  A:Toggle  B:Back");
	dc_toast_draw(screen);
}

static bool dc_settings_row_is_toggle(int row)
{
	return row == 3 || row == 4 || row == 5 || row == 8;
}

static void dc_settings_nudge_row(struct dc_settings *settings, int *selected_row,
				  int inc)
{
	char toast_line[48];

	switch (*selected_row) {
	case 0:
		settings->palette_index =
			(uint8_t)((settings->palette_index + inc + DC_PALETTE_COUNT) %
				  DC_PALETTE_COUNT);
		dc_toast_show(dc_palette_name(settings->palette_index), 1000);
		break;
	case 1:
		settings->video_output =
			(enum dc_video_output)((settings->video_output + inc +
						DC_VIDEO_OUTPUT_COUNT) %
					       DC_VIDEO_OUTPUT_COUNT);
		dc_toast_show(dc_video_output_name(settings->video_output), 1000);
		break;
	case 2:
		settings->scale_mode =
			(enum dc_scale_mode)((settings->scale_mode + inc + DC_SCALE_COUNT) %
					     DC_SCALE_COUNT);
		dc_toast_show(dc_video_scale_mode_name(settings->scale_mode), 1000);
		break;
	case 3:
		settings->status_bar = !settings->status_bar;
		dc_toast_show(settings->status_bar ? "Status bar on" :
						       "Status bar off",
			      1000);
		break;
	case 4:
		settings->frameskip = !settings->frameskip;
		dc_toast_show(settings->frameskip ? "Frameskip on" : "Frameskip off",
			      1000);
		break;
	case 5:
		settings->autosave_enabled = !settings->autosave_enabled;
		if (!settings->autosave_enabled && *selected_row == 6)
			*selected_row = 5;
		dc_toast_show(settings->autosave_enabled ? "Autosave on" :
							   "Autosave off",
			      1000);
		break;
	case 6:
		if (!settings->autosave_enabled)
			break;
		settings->autosave_interval_sec += inc * 10;
		if (settings->autosave_interval_sec < DC_SETTINGS_AUTOSAVE_MIN_SEC)
			settings->autosave_interval_sec = DC_SETTINGS_AUTOSAVE_MIN_SEC;
		if (settings->autosave_interval_sec > DC_SETTINGS_AUTOSAVE_MAX_SEC)
			settings->autosave_interval_sec = DC_SETTINGS_AUTOSAVE_MAX_SEC;
		snprintf(toast_line, sizeof(toast_line), "Autosave: %d sec",
			 settings->autosave_interval_sec);
		dc_toast_show(toast_line, 1000);
		break;
	case 7:
		if (settings->muted) {
			settings->muted = false;
			dc_toast_show("Audio unmuted", 1000);
			break;
		}
		{
			int vol = (int)settings->volume + inc * 10;

			if (vol < DC_SETTINGS_VOLUME_MIN)
				vol = DC_SETTINGS_VOLUME_MIN;
			if (vol > DC_SETTINGS_VOLUME_MAX)
				vol = DC_SETTINGS_VOLUME_MAX;
			settings->volume = (uint8_t)vol;
		}
		snprintf(toast_line, sizeof(toast_line), "Volume: %u%%",
			 settings->volume);
		dc_toast_show(toast_line, 1000);
		break;
	case 8:
		settings->menu_music = !settings->menu_music;
		dc_toast_show(settings->menu_music ? "Menu music on" :
						     "Menu music off",
			      1000);
		break;
	default:
		settings->audio_buffer =
			(enum dc_audio_buffer_mode)((settings->audio_buffer + inc +
						     DC_AUDIO_BUFFER_COUNT) %
						    DC_AUDIO_BUFFER_COUNT);
		dc_toast_show(dc_audio_buffer_name(settings->audio_buffer), 1000);
		break;
	}

	if (settings_apply_cb)
		settings_apply_cb(settings);
}

static uint32_t dc_settings_previous_buttons = 0xFFFF;
static int dc_settings_t_up;
static int dc_settings_t_down;
static int dc_settings_t_horiz;
static int dc_settings_last_horiz;
static int dc_settings_horiz_edge_latch;

static void dc_settings_flush_input(void)
{
	dc_menu_flush_input();
	dc_settings_previous_buttons = 0xFFFF;
	dc_settings_t_up = dc_settings_t_down = 0;
	dc_settings_t_horiz = dc_settings_last_horiz = dc_settings_horiz_edge_latch = 0;
}

static bool dc_settings_poll_input(struct dc_settings *settings, int *selected_row,
				   bool *done)
{
	maple_device_t *controller = (maple_device_t *)dc_input_controller();
	uint32_t previous_buttons = dc_settings_previous_buttons;
	int t_up = dc_settings_t_up;
	int t_down = dc_settings_t_down;
	int t_horiz = dc_settings_t_horiz;
	int last_horiz = dc_settings_last_horiz;
	int horiz_edge_latch = dc_settings_horiz_edge_latch;
	cont_state_t *pad;
	uint32_t buttons;
	uint32_t changed;
	int vert;
	int horiz;
	int axis_step;
	int horiz_edge;
	bool changed_value = false;

	*done = false;
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

	if (dc_input_repeat(vert < 0, &t_up)) {
		*selected_row = dc_settings_advance_row(*selected_row, -1,
						      settings->autosave_enabled);
		changed_value = true;
	} else if (dc_input_repeat(vert > 0, &t_down)) {
		*selected_row = dc_settings_advance_row(*selected_row, 1,
						      settings->autosave_enabled);
		changed_value = true;
	}

	horiz_edge = dc_input_axis_edge(horiz, &horiz_edge_latch);
	axis_step = dc_settings_row_is_toggle(*selected_row) ?
		    horiz_edge :
		    dc_input_repeat_axis(horiz, &t_horiz, &last_horiz);
	if (axis_step != 0) {
		dc_settings_nudge_row(settings, selected_row, axis_step);
		changed_value = true;
	}

	if ((buttons & CONT_A) && (changed & CONT_A)) {
		if (*selected_row == 7) {
			settings->muted = !settings->muted;
			dc_toast_show(settings->muted ? "Audio muted" : "Audio unmuted",
				      1000);
			if (settings_apply_cb)
				settings_apply_cb(settings);
		} else {
			dc_settings_nudge_row(settings, selected_row, 1);
		}
		changed_value = true;
	}

	if (((buttons & CONT_B) && (changed & CONT_B)) ||
	    ((buttons & CONT_X) && (changed & CONT_X)))
		*done = true;

	dc_settings_previous_buttons = buttons;
	dc_settings_t_up = t_up;
	dc_settings_t_down = t_down;
	dc_settings_t_horiz = t_horiz;
	dc_settings_last_horiz = last_horiz;
	dc_settings_horiz_edge_latch = horiz_edge_latch;
	return changed_value;

release:
	dc_settings_t_up = dc_settings_t_down = 0;
	dc_settings_t_horiz = dc_settings_last_horiz = dc_settings_horiz_edge_latch = 0;
	dc_settings_previous_buttons = 0xFFFF;
	return false;
}

bool dc_settings_menu_run(struct dc_settings *settings)
{
	uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH];
	int selected_row = 0;
	bool dirty = true;
	bool toast_visible = false;

	if (!settings)
		return false;

	dc_settings_flush_input();

	while (1) {
		const uint64_t frame_start = timer_ms_gettime64();
		bool done = false;
		bool changed;
		const bool toast_active = dc_toast_active();
		uint64_t elapsed;

		if (dirty || toast_active || toast_visible) {
			dc_settings_draw_value_screen(settings, selected_row, screen);
			dc_video_present_screen(screen);
			dirty = false;
			toast_visible = toast_active;
		}

		changed = dc_settings_poll_input(settings, &selected_row, &done);
		if (done) {
			dc_settings_save(settings);
			return true;
		}

		if (changed)
			dirty = true;

		elapsed = timer_ms_gettime64() - frame_start;
		if (elapsed < DC_INPUT_FRAME_MS)
			timer_spin((int)(DC_INPUT_FRAME_MS - elapsed));
	}
}
