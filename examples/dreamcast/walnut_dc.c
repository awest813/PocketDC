/*
 * PocketDC Dreamcast frontend.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <kos.h>
#include <dc/video.h>

uint8_t audio_read(uint16_t addr);
void audio_write(uint16_t addr, uint8_t val);

#define WALNUT_GB_IS_LITTLE_ENDIAN 1
#define WALNUT_GB_32BIT_DMA 1
#define WALNUT_GB_16BIT_DMA 0
#define WALNUT_GB_32BIT_ALIGNED 1
#define WALNUT_GB_USE_INTRINSICS 1
#define ENABLE_LCD 1
#define ENABLE_SOUND 1
#define WALNUT_GB_12_COLOUR 1

#include "../../walnut_cgb.h"

#include "audio.h"
#include "display.h"
#include "dc_priv.h"
#include "input.h"
#include "menu.h"
#include "palette.h"
#include "rom_browser.h"
#include "settings.h"
#include "toast.h"
#include "video.h"

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);

static struct dc_priv priv;
static struct dc_settings app_settings;
static unsigned int palette_selection = 3;

static int dc_autosave_interval_frames(void)
{
	if (!app_settings.autosave_enabled)
		return 0;

	return (int)(VERTICAL_SYNC * (double)app_settings.autosave_interval_sec);
}

static void dc_apply_av_settings(void)
{
	dc_display_init(app_settings.video_output);
	dc_video_set_scale_mode(app_settings.scale_mode);
#if ENABLE_SOUND
	dc_audio_configure(app_settings.volume, app_settings.muted,
			   app_settings.audio_buffer);
#endif
}

static void dc_on_settings_changed(struct dc_settings *settings)
{
	(void)settings;
	dc_apply_av_settings();
}

static void dc_apply_settings_to_game(struct gb_s *gb, struct dc_priv *p)
{
	gb->direct.frame_skip = app_settings.frameskip;
	palette_selection = app_settings.palette_index;
	dc_manual_assign_palette(p, (uint8_t)palette_selection);
	dc_apply_av_settings();
}

static void dc_build_status_bar_text(const char *rom_title, char *line, size_t len)
{
	if (!line || len == 0)
		return;

	if (app_settings.muted) {
		snprintf(line, len, "%s | %s | Mute",
			 rom_title ? rom_title : "Game",
			 dc_video_scale_mode_name(app_settings.scale_mode));
		return;
	}

	snprintf(line, len, "%s | %s | Vol %u%%",
		 rom_title ? rom_title : "Game",
		 dc_video_scale_mode_name(app_settings.scale_mode),
		 app_settings.volume);
}

uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr)
{
	const struct dc_priv *p = gb->direct.priv;

	if (!p->rom || addr >= p->rom_size)
		return 0xFF;

	return p->rom[addr];
}

uint16_t gb_rom_read_16bit(struct gb_s *gb, const uint_fast32_t addr)
{
	const struct dc_priv *p = gb->direct.priv;

	if (!p->rom || addr >= p->rom_size)
		return 0xFFFF;
	if (addr + 1 >= p->rom_size)
		return (uint16_t)p->rom[addr];

	{
		const uint8_t *src = &p->rom[addr];

		if ((uintptr_t)src & 1)
			return (uint16_t)src[0] | ((uint16_t)src[1] << 8);

		return *(const uint16_t *)src;
	}
}

uint32_t gb_rom_read_32bit(struct gb_s *gb, const uint_fast32_t addr)
{
	const struct dc_priv *p = gb->direct.priv;

	if (!p->rom || addr >= p->rom_size)
		return 0xFFFFFFFF;
	if (addr + 3 >= p->rom_size) {
		const uint8_t *src = &p->rom[addr];
		uint32_t value = 0;
		size_t i;

		for (i = 0; i < 4 && addr + i < p->rom_size; i++)
			value |= (uint32_t)src[i] << (i * 8);

		return value;
	}

	{
		const uint8_t *src = &p->rom[addr];

		if ((uintptr_t)src & 3) {
			return (uint32_t)src[0] |
			       ((uint32_t)src[1] << 8) |
			       ((uint32_t)src[2] << 16) |
			       ((uint32_t)src[3] << 24);
		}

		return *(const uint32_t *)src;
	}
}

uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr)
{
	const struct dc_priv *p = gb->direct.priv;

	if (!p->cart_ram || addr >= p->save_size)
		return 0xFF;

	return p->cart_ram[addr];
}

void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val)
{
	struct dc_priv *p = gb->direct.priv;

	if (!p->cart_ram || addr >= p->save_size)
		return;

	p->cart_ram[addr] = val;
}

uint8_t gb_bootrom_read(struct gb_s *gb, const uint_fast16_t addr)
{
	const struct dc_priv *p = gb->direct.priv;

	if (!p->bootrom || addr >= DC_DMG_BOOTROM_SIZE)
		return 0xFF;

	return p->bootrom[addr];
}

static inline uint16_t rgb565_to_rgb555(uint16_t c565)
{
	const uint16_t r5 = (c565 >> 11) & 0x1F;
	const uint16_t g6 = (c565 >> 5) & 0x3F;
	const uint16_t b5 = c565 & 0x1F;
	const uint16_t g5 = (uint16_t)((g6 * 31 + 31 / 2) / 63);

	return (uint16_t)((r5 << 10) | (g5 << 5) | b5);
}

void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[160], const uint_fast8_t line)
{
	struct dc_priv *p = gb->direct.priv;
	uint16_t(*fb565)[LCD_WIDTH] = p->fb;

	if (gb->cgb.cgbMode) {
		unsigned int x;

		for (x = 0; x < LCD_WIDTH; x++)
			fb565[line][x] = rgb565_to_rgb555(gb->cgb.fixPalette[pixels[x]]);
	}
#if WALNUT_GB_12_COLOUR
	else {
		unsigned int x;

		for (x = 0; x < LCD_WIDTH; x++)
			fb565[line][x] =
				p->selected_palette[((pixels[x] & 18) >> 1) | (pixels[x] & 3)];
	}
#else
	else {
		unsigned int x;

		for (x = 0; x < LCD_WIDTH; x++)
			fb565[line][x] = p->selected_palette[pixels[x] & 3];
	}
#endif
}

#if ENABLE_SOUND
uint8_t audio_read(uint16_t addr)
{
	return dc_audio_read(addr);
}

void audio_write(uint16_t addr, uint8_t val)
{
	dc_audio_write(addr, val);
}
#endif

void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t addr)
{
	struct dc_priv *p = gb->direct.priv;
	const char *gb_err_str[GB_INVALID_MAX] = {
		"UNKNOWN",
		"INVALID OPCODE",
		"INVALID READ",
		"INVALID WRITE",
		""
	};

	(void)addr;
	if (p->save_size > 0 && p->cart_ram) {
		dc_cart_ram_write_file("recovery.sav", p->cart_ram, p->save_size);
		if (p->save_path[0] != '\0')
			dc_cart_ram_write_file(p->save_path, p->cart_ram, p->save_size);
	}

	printf("pocketdc: emulator error %s\n",
	       (unsigned int)gb_err < GB_INVALID_MAX ? gb_err_str[gb_err] :
						      gb_err_str[0]);
	arch_exit();
}

static int dc_load_bootrom(struct dc_priv *p)
{
	FILE *f;
	long size;

	f = fopen("dmg_boot.bin", "rb");
	if (!f)
		return -1;

	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}

	size = ftell(f);
	if (size != (long)DC_DMG_BOOTROM_SIZE) {
		fclose(f);
		return -1;
	}

	rewind(f);
	p->bootrom = (uint8_t *)malloc((size_t)size);
	if (!p->bootrom) {
		fclose(f);
		return -1;
	}

	if (fread(p->bootrom, 1, (size_t)size, f) != (size_t)size) {
		free(p->bootrom);
		p->bootrom = NULL;
		fclose(f);
		return -1;
	}

	fclose(f);
	return 0;
}

static int dc_init_emulator(struct gb_s *gb, struct dc_priv *p)
{
	enum gb_init_error_e gb_ret;

	gb_ret = gb_init(gb, &gb_rom_read, &gb_rom_read_16bit, &gb_rom_read_32bit,
			 &gb_cart_ram_read, &gb_cart_ram_write, &gb_error, p);
	if (gb_ret != GB_INIT_NO_ERROR)
		return (int)gb_ret;

	if (dc_load_bootrom(p) == 0) {
		gb_set_bootrom(gb, gb_bootrom_read);
		gb_reset(gb);
	}

	if (gb_get_save_size_s(gb, &p->save_size) < 0) {
		printf("pocketdc: unable to determine save size\n");
		return -1;
	}

	if (p->save_size > 0 && dc_cart_ram_read_file(p->save_path, &p->cart_ram,
						      p->save_size) != 0) {
		printf("pocketdc: unable to allocate cart RAM\n");
		return DC_INIT_ERR_SAVE_ALLOC;
	}

	{
		time_t rawtime;
		struct tm timeinfo;

		time(&rawtime);
		localtime_r(&rawtime, &timeinfo);
		gb_set_rtc(gb, &timeinfo);
	}

	gb_init_lcd(gb, &lcd_draw_line);
	dc_auto_assign_palette(p, gb_colour_hash(gb));
	dc_manual_assign_palette(p, (uint8_t)palette_selection);
	return 0;
}

static bool dc_write_save(struct dc_priv *p)
{
	if (p->save_size == 0 || !p->cart_ram || p->save_path[0] == '\0')
		return true;

	return dc_cart_ram_write_file(p->save_path, p->cart_ram, p->save_size) == 0;
}

/*
 * Pause menu result: 1 = resume, 0 = return to main menu, -1 = exit app.
 */
static int dc_handle_pause_menu(struct gb_s *gb, bool menu_mode)
{
	char rom_title[17];
	enum dc_pause_menu_action action;

	gb_get_rom_name(gb, rom_title);
	rom_title[sizeof(rom_title) - 1] = '\0';

	dc_input_flush_edges();

	while (1) {
		const bool can_save = priv.save_size > 0 && priv.cart_ram != NULL;
		const bool can_load = can_save &&
				      dc_save_file_exists(priv.save_path);

		action = dc_pause_menu_run(rom_title, can_save, can_load, menu_mode);

		switch (action) {
		case DC_PAUSE_MENU_RESUME:
			return 1;
		case DC_PAUSE_MENU_SAVE:
			if (dc_write_save(&priv))
				dc_menu_show_message("Save Game", "Game saved.", 1200);
			else
				dc_menu_show_message("Save Game",
						     "Unable to write save file.",
						     1500);
			break;
		case DC_PAUSE_MENU_LOAD:
			if (can_save) {
				if (dc_cart_ram_reload_file(priv.save_path, priv.cart_ram,
							    priv.save_size) == 0) {
					gb_reset(gb);
					dc_menu_show_message("Load Game",
							     "Save loaded.", 1200);
				} else {
					dc_menu_show_message("Load Game",
							     "No save file found.",
							     1500);
				}
			}
			break;
		case DC_PAUSE_MENU_SETTINGS:
			dc_settings_menu_run(&app_settings);
			dc_apply_settings_to_game(gb, &priv);
			break;
		case DC_PAUSE_MENU_MAIN_MENU:
			dc_write_save(&priv);
			return 0;
		case DC_PAUSE_MENU_EXIT:
			dc_write_save(&priv);
			return -1;
		default:
			return 1;
		}
	}
}

static void dc_show_fast_mode_toast(unsigned int fast_mode)
{
	char line[48];

	if (fast_mode <= 1)
		return;

	snprintf(line, sizeof(line), "Fast-forward %ux", fast_mode);
	dc_toast_show(line, 800);
}

static bool dc_run_game(const char *rom_path, const char *save_path, bool menu_mode)
{
	struct gb_s gb;
	struct dc_input_state input;
	unsigned int fast_mode = 1;
	unsigned int prev_fast_mode = 1;
	unsigned int fast_mode_timer = 1;
	int save_timer;
	uint64_t target_ticks;
	bool running = true;
	bool paused = false;
	bool return_to_main_menu = false;
	bool exit_app = false;
	char rom_title[17];

	memset(&priv, 0, sizeof(priv));
	if (dc_rom_load(&priv, rom_path) != 0) {
		dc_menu_show_message("Load Failed",
				     "Unable to open or validate ROM file.",
				     1500);
		return menu_mode;
	}

	if (save_path && save_path[0] != '\0') {
		strncpy(priv.save_path, save_path, sizeof(priv.save_path) - 1);
		priv.save_path[sizeof(priv.save_path) - 1] = '\0';
	}

	{
		const int init_err = dc_init_emulator(&gb, &priv);

		if (init_err != 0) {
			const char *message = "Emulator init failed.";

			if (init_err == GB_INIT_INVALID_CHECKSUM)
				message = "Invalid ROM checksum.";
			else if (init_err == GB_INIT_CARTRIDGE_UNSUPPORTED)
				message = "Unsupported cartridge type.";
			else if (init_err == DC_INIT_ERR_SAVE_ALLOC)
				message = "Out of memory for save data.";

			dc_menu_show_message("Load Failed", message, 1500);
			dc_rom_unload(&priv);
			return menu_mode;
		}
	}

	dc_apply_settings_to_game(&gb, &priv);
	save_timer = dc_autosave_interval_frames();

	gb_get_rom_name(&gb, rom_title);
	rom_title[sizeof(rom_title) - 1] = '\0';
	printf("PocketDC: %s\n", rom_title);

	target_ticks = (uint64_t)(1000.0 / VERTICAL_SYNC);

	while (running) {
		uint64_t now = timer_ms_gettime64();

		dc_input_poll(&input, &gb);

		if (input.reset_game)
			gb_reset(&gb);
		if (input.cycle_palette) {
			palette_selection = (palette_selection + 1) % DC_PALETTE_COUNT;
			dc_manual_assign_palette(&priv, (uint8_t)palette_selection);
			app_settings.palette_index = (uint8_t)palette_selection;
			dc_toast_show(dc_palette_name((uint8_t)palette_selection),
				      DC_TOAST_DURATION_MS);
			dc_settings_save(&app_settings);
		}
		if (input.toggle_frameskip) {
			gb.direct.frame_skip = !gb.direct.frame_skip;
			app_settings.frameskip = gb.direct.frame_skip;
			dc_toast_show(gb.direct.frame_skip ? "Frameskip on" : "Frameskip off",
				      DC_TOAST_DURATION_MS);
			dc_settings_save(&app_settings);
		}
		if (input.cycle_scale) {
			app_settings.scale_mode =
				(enum dc_scale_mode)((app_settings.scale_mode + 1) %
						     DC_SCALE_COUNT);
			dc_video_set_scale_mode(app_settings.scale_mode);
			dc_toast_show(dc_video_scale_mode_name(app_settings.scale_mode),
				      DC_TOAST_DURATION_MS);
			dc_settings_save(&app_settings);
		}
		if (input.pause_requested) {
			paused = true;
			gb.direct.joypad = 0xFF;
			dc_input_flush_edges();
		}
		if (input.exit_requested) {
			running = false;
			break;
		}

		if (paused) {
			const int pause_result = dc_handle_pause_menu(&gb, menu_mode);

			if (pause_result < 0) {
				exit_app = true;
				running = false;
				break;
			}
			if (pause_result == 0) {
				return_to_main_menu = true;
				running = false;
				break;
			}

			paused = false;
			save_timer = dc_autosave_interval_frames();
			continue;
		}

		fast_mode = input.fast_mode;
		if (fast_mode != prev_fast_mode) {
			dc_show_fast_mode_toast(fast_mode);
			prev_fast_mode = fast_mode;
		}
		gb_run_frame_dualfetch(&gb);

#if ENABLE_SOUND
		if (dc_audio_ready())
			dc_audio_frame();
#endif

		if (fast_mode_timer > 1) {
			fast_mode_timer--;
			continue;
		}

		fast_mode_timer = fast_mode;
		{
			char status_line[64];
			const char *status = NULL;

			if (app_settings.status_bar) {
				dc_build_status_bar_text(rom_title, status_line,
							 sizeof(status_line));
				status = status_line;
			}
			dc_video_present(&priv, status);
		}

		if (save_timer > 0 && priv.save_size > 0 && --save_timer <= 0) {
			if (dc_write_save(&priv))
				dc_toast_show("Autosaved", 1200);
			else
				dc_toast_show("Autosave failed", 1500);
			save_timer = dc_autosave_interval_frames();
		}

		{
			const uint64_t elapsed = timer_ms_gettime64() - now;

			if (elapsed < target_ticks)
				timer_spin((int)(target_ticks - elapsed));
		}
	}

	dc_write_save(&priv);
	dc_rom_unload(&priv);

	if (exit_app)
		return false;

	if (return_to_main_menu)
		return true;

	if (menu_mode && input.exit_requested)
		return true;

	return !input.exit_requested;
}

int main(int argc, char **argv)
{
	struct dc_browser browser;
	char selected_rom[256];
	bool show_start_menu = true;
	bool show_migration_toast = false;
	bool audio_init_failed = false;

	dc_settings_load(&app_settings);
	show_migration_toast = dc_settings_take_migration_notice();
	dc_menu_set_settings_apply_callback(dc_on_settings_changed);
	dc_apply_av_settings();
	vid_clear(0, 0, 0);

	if (dc_video_init() != 0) {
		printf("pocketdc: video init failed\n");
		return EXIT_FAILURE;
	}

#if ENABLE_SOUND
	if (dc_audio_init() != 0) {
		printf("pocketdc: audio init failed, continuing without sound\n");
		audio_init_failed = true;
	} else {
		dc_apply_av_settings();
	}
#endif

	dc_input_init();
	palette_selection = app_settings.palette_index;
	dc_browser_init(&browser);
	dc_browser_apply_persisted(&browser, &app_settings);

	if (show_migration_toast)
		dc_toast_show("Config upgraded to pocketdc.cfg", 2500);
	if (audio_init_failed)
		dc_toast_show("Audio init failed - no sound", 3000);

	if (argc >= 2) {
		const char *save_path = (argc >= 3) ? argv[2] : NULL;

		if (!dc_run_game(argv[1], save_path, false))
			goto shutdown;
	} else {
		while (1) {
			enum dc_main_menu_action action;

			if (show_start_menu) {
				dc_start_menu_run();
				show_start_menu = false;
			}

			action = dc_main_menu_run(&app_settings);
			if (action == DC_MAIN_MENU_EXIT)
				break;

			if (action == DC_MAIN_MENU_CONTINUE) {
				strncpy(selected_rom, app_settings.last_rom_path,
					sizeof(selected_rom) - 1);
				selected_rom[sizeof(selected_rom) - 1] = '\0';
				dc_settings_push_recent(&app_settings, selected_rom);
				dc_settings_save(&app_settings);

				if (!dc_run_game(selected_rom, NULL, true))
					goto shutdown;
				continue;
			}

			if (action == DC_MAIN_MENU_SETTINGS) {
				dc_settings_menu_run(&app_settings);
				palette_selection = app_settings.palette_index;
				dc_apply_av_settings();
				continue;
			}

			if (action == DC_MAIN_MENU_CONTROLS) {
				dc_controls_menu_run();
				continue;
			}

			while (action == DC_MAIN_MENU_ROM_LIBRARY) {
				dc_browser_apply_persisted(&browser, &app_settings);
				if (!dc_browser_run(&browser, selected_rom,
						    sizeof(selected_rom))) {
					dc_browser_export_persisted(&browser,
								    &app_settings);
					dc_settings_save(&app_settings);
					break;
				}

				dc_settings_push_recent(&app_settings, selected_rom);
				dc_browser_export_persisted(&browser, &app_settings);
				dc_settings_save(&app_settings);

				if (!dc_run_game(selected_rom, NULL, true))
					goto shutdown;
			}
		}
	}

	dc_settings_save(&app_settings);

shutdown:
	dc_video_shutdown();
#if ENABLE_SOUND
	dc_audio_shutdown();
#endif

	return EXIT_SUCCESS;
}
