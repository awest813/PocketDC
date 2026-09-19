/*
 * PocketDC Dreamcast frontend — persistent settings.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#ifndef DC_SETTINGS_H
#define DC_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "display.h"
#include "video.h"

#define DC_SETTINGS_CONFIG_VERSION     3
#define DC_SETTINGS_RECENT_MAX         5
#define DC_SETTINGS_FAVORITE_MAX       16
#define DC_SETTINGS_AUTOSAVE_MIN_SEC   10
#define DC_SETTINGS_AUTOSAVE_MAX_SEC   300
#define DC_SETTINGS_AUTOSAVE_DEFAULT_SEC 60
#define DC_SETTINGS_VOLUME_MIN         0
#define DC_SETTINGS_VOLUME_MAX         100
#define DC_SETTINGS_VOLUME_DEFAULT     100
#define DC_SETTINGS_ROW_COUNT          9
#define DC_SETTINGS_LAST_ROM_LEN       256

enum dc_audio_buffer_mode
{
	DC_AUDIO_BUFFER_LOW = 0,
	DC_AUDIO_BUFFER_NORMAL,
	DC_AUDIO_BUFFER_HIGH,
	DC_AUDIO_BUFFER_COUNT
};

struct dc_settings
{
	uint8_t palette_index;
	enum dc_video_output video_output;
	enum dc_scale_mode scale_mode;
	bool status_bar;
	bool frameskip;
	bool autosave_enabled;
	int autosave_interval_sec;
	uint8_t volume;
	bool muted;
	enum dc_audio_buffer_mode audio_buffer;
	int browser_root_index;
	uint8_t browser_view;
	uint8_t browser_filter;
	char last_rom_path[DC_SETTINGS_LAST_ROM_LEN];
	char recent_roms[DC_SETTINGS_RECENT_MAX][DC_SETTINGS_LAST_ROM_LEN];
	char favorite_roms[DC_SETTINGS_FAVORITE_MAX][DC_SETTINGS_LAST_ROM_LEN];
};

void dc_settings_init_defaults(struct dc_settings *settings);
void dc_settings_load(struct dc_settings *settings);
int dc_settings_save(const struct dc_settings *settings);
void dc_settings_push_recent(struct dc_settings *settings, const char *path);
bool dc_settings_is_favorite(const struct dc_settings *settings, const char *path);
int dc_settings_toggle_favorite(struct dc_settings *settings, const char *path);
bool dc_settings_take_migration_notice(void);
bool dc_settings_can_continue(const struct dc_settings *settings);
void dc_settings_continue_label(const struct dc_settings *settings, char *out,
				size_t out_len);
const char *dc_audio_buffer_name(enum dc_audio_buffer_mode mode);

#endif /* DC_SETTINGS_H */
