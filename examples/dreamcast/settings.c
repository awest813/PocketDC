/*
 * PocketDC Dreamcast frontend — persistent settings.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>

#include "../../extras/ini_kv/ini_kv.h"
#include "dc_priv.h"
#include "palette.h"
#include "settings.h"

static const char *const dc_audio_buffer_names[DC_AUDIO_BUFFER_COUNT] = {
	"Low latency",
	"Normal",
	"Stable"
};

static const char *dc_settings_read_paths[] = {
	"/pc/pocketdc.cfg",
	"/sd/pocketdc.cfg",
	"/ide/pocketdc.cfg",
	"/cd/pocketdc.cfg",
	"/pc/walnut-dc.cfg",
	"/sd/walnut-dc.cfg",
	"/ide/walnut-dc.cfg",
	"/cd/walnut-dc.cfg",
	NULL
};

static const char *dc_settings_write_paths[] = {
	"/pc/pocketdc.cfg",
	"/sd/pocketdc.cfg",
	"/ide/pocketdc.cfg",
	"/cd/pocketdc.cfg",
	NULL
};

static bool dc_settings_pending_migration;

void dc_settings_init_defaults(struct dc_settings *settings)
{
	memset(settings, 0, sizeof(*settings));
	settings->palette_index = 3;
	settings->video_output = DC_VIDEO_OUTPUT_AUTO;
	settings->scale_mode = DC_SCALE_3X;
	settings->status_bar = true;
	settings->frameskip = false;
	settings->autosave_enabled = true;
	settings->autosave_interval_sec = DC_SETTINGS_AUTOSAVE_DEFAULT_SEC;
	settings->volume = DC_SETTINGS_VOLUME_DEFAULT;
	settings->muted = false;
	settings->audio_buffer = DC_AUDIO_BUFFER_NORMAL;
	settings->browser_root_index = 0;
	settings->browser_view = 0;
	settings->browser_filter = 0;
}

const char *dc_audio_buffer_name(enum dc_audio_buffer_mode mode)
{
	if (mode >= DC_AUDIO_BUFFER_COUNT)
		return dc_audio_buffer_names[DC_AUDIO_BUFFER_NORMAL];

	return dc_audio_buffer_names[mode];
}

static void dc_settings_clamp(struct dc_settings *settings)
{
	if (settings->palette_index >= DC_PALETTE_COUNT)
		settings->palette_index = 3;

	if (settings->video_output >= DC_VIDEO_OUTPUT_COUNT)
		settings->video_output = DC_VIDEO_OUTPUT_AUTO;

	if (settings->scale_mode >= DC_SCALE_COUNT)
		settings->scale_mode = DC_SCALE_3X;

	if (settings->autosave_interval_sec < DC_SETTINGS_AUTOSAVE_MIN_SEC)
		settings->autosave_interval_sec = DC_SETTINGS_AUTOSAVE_MIN_SEC;
	if (settings->autosave_interval_sec > DC_SETTINGS_AUTOSAVE_MAX_SEC)
		settings->autosave_interval_sec = DC_SETTINGS_AUTOSAVE_MAX_SEC;

	if (settings->volume > DC_SETTINGS_VOLUME_MAX)
		settings->volume = DC_SETTINGS_VOLUME_DEFAULT;

	if (settings->audio_buffer >= DC_AUDIO_BUFFER_COUNT)
		settings->audio_buffer = DC_AUDIO_BUFFER_NORMAL;

	if (settings->browser_root_index < 0)
		settings->browser_root_index = 0;
	/* Seven scan roots (indices 0–6): /cd, /sd, /ide, /pc and their /roms. */
	if (settings->browser_root_index > 6)
		settings->browser_root_index = 0;

	if (settings->browser_view > 1)
		settings->browser_view = 0;

	if (settings->browser_filter > 2)
		settings->browser_filter = 0;
}

static void dc_settings_parse_line(struct dc_settings *settings, const char *line)
{
	int value;
	unsigned int slot;
	char key[24];

	if (ini_kv_get_int(line, "config_version", &value))
		return;

	if (ini_kv_get_int(line, "palette_index", &value)) {
		settings->palette_index = (uint8_t)value;
		return;
	}

	if (ini_kv_get_int(line, "video_output", &value)) {
		settings->video_output = (enum dc_video_output)value;
		return;
	}

	if (ini_kv_get_int(line, "scale_mode", &value)) {
		settings->scale_mode = (enum dc_scale_mode)value;
		return;
	}

	if (ini_kv_get_int(line, "status_bar", &value)) {
		settings->status_bar = value != 0;
		return;
	}

	if (ini_kv_get_int(line, "frameskip", &value)) {
		settings->frameskip = value != 0;
		return;
	}

	if (ini_kv_get_int(line, "autosave_enabled", &value)) {
		settings->autosave_enabled = value != 0;
		return;
	}

	if (ini_kv_get_int(line, "autosave_interval_sec", &value)) {
		settings->autosave_interval_sec = value;
		return;
	}

	if (ini_kv_get_int(line, "volume", &value)) {
		settings->volume = (uint8_t)value;
		return;
	}

	if (ini_kv_get_int(line, "muted", &value)) {
		settings->muted = value != 0;
		return;
	}

	if (ini_kv_get_int(line, "audio_buffer", &value)) {
		settings->audio_buffer = (enum dc_audio_buffer_mode)value;
		return;
	}

	if (ini_kv_get_int(line, "browser_root_index", &value)) {
		settings->browser_root_index = value;
		return;
	}

	if (ini_kv_get_int(line, "browser_view", &value)) {
		settings->browser_view = (uint8_t)value;
		return;
	}

	if (ini_kv_get_int(line, "browser_filter", &value)) {
		settings->browser_filter = (uint8_t)value;
		return;
	}

	if (ini_kv_get_string(line, "last_rom_path", settings->last_rom_path,
			      sizeof(settings->last_rom_path)))
		return;

	for (slot = 0; slot < DC_SETTINGS_RECENT_MAX; slot++) {
		snprintf(key, sizeof(key), "recent_rom_%u", slot);
		if (ini_kv_get_string(line, key, settings->recent_roms[slot],
				      sizeof(settings->recent_roms[slot])))
			return;
	}
}

void dc_settings_load(struct dc_settings *settings)
{
	FILE *f;
	char line[320];
	unsigned int i;

	dc_settings_pending_migration = false;
	dc_settings_init_defaults(settings);

	for (i = 0; dc_settings_read_paths[i] != NULL; i++) {
		f = fopen(dc_settings_read_paths[i], "r");
		if (!f)
			continue;

		if (strstr(dc_settings_read_paths[i], "walnut-dc.cfg") != NULL)
			dc_settings_pending_migration = true;

		while (fgets(line, sizeof(line), f) != NULL) {
			char *newline = strchr(line, '\n');

			if (newline)
				*newline = '\0';

			if (line[0] == '#' || line[0] == '\0' || line[0] == ';')
				continue;

			dc_settings_parse_line(settings, line);
		}

		fclose(f);
		break;
	}

	if (settings->last_rom_path[0] != '\0' &&
	    settings->recent_roms[0][0] == '\0') {
		strncpy(settings->recent_roms[0], settings->last_rom_path,
			sizeof(settings->recent_roms[0]) - 1);
	}

	dc_settings_clamp(settings);
}

static int dc_settings_write_file(const char *path, const struct dc_settings *settings)
{
	char tmp_path[280];
	FILE *f;
	unsigned int slot;

	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
	f = fopen(tmp_path, "w");
	if (!f)
		return -1;

	ini_kv_fprint_int(f, "config_version", DC_SETTINGS_CONFIG_VERSION);
	ini_kv_fprint_int(f, "palette_index", settings->palette_index);
	ini_kv_fprint_int(f, "video_output", (int)settings->video_output);
	ini_kv_fprint_int(f, "scale_mode", (int)settings->scale_mode);
	ini_kv_fprint_bool(f, "status_bar", settings->status_bar);
	ini_kv_fprint_bool(f, "frameskip", settings->frameskip);
	ini_kv_fprint_bool(f, "autosave_enabled", settings->autosave_enabled);
	ini_kv_fprint_int(f, "autosave_interval_sec",
			  settings->autosave_interval_sec);
	ini_kv_fprint_int(f, "volume", settings->volume);
	ini_kv_fprint_bool(f, "muted", settings->muted);
	ini_kv_fprint_int(f, "audio_buffer", (int)settings->audio_buffer);
	ini_kv_fprint_int(f, "browser_root_index", settings->browser_root_index);
	ini_kv_fprint_int(f, "browser_view", settings->browser_view);
	ini_kv_fprint_int(f, "browser_filter", settings->browser_filter);
	ini_kv_fprint_string(f, "last_rom_path", settings->last_rom_path);
	for (slot = 0; slot < DC_SETTINGS_RECENT_MAX; slot++) {
		char key[24];

		snprintf(key, sizeof(key), "recent_rom_%u", slot);
		ini_kv_fprint_string(f, key, settings->recent_roms[slot]);
	}

	if (fflush(f) != 0) {
		fclose(f);
		remove(tmp_path);
		return -1;
	}

	fclose(f);
	remove(path);
	if (rename(tmp_path, path) != 0) {
		remove(tmp_path);
		return -1;
	}

	return 0;
}

int dc_settings_save(const struct dc_settings *settings)
{
	unsigned int i;

	dc_settings_pending_migration = false;

	for (i = 0; dc_settings_write_paths[i] != NULL; i++) {
		if (dc_settings_write_file(dc_settings_write_paths[i], settings) == 0)
			return 0;
	}

	return -1;
}

void dc_settings_push_recent(struct dc_settings *settings, const char *path)
{
	unsigned int i;
	unsigned int write;

	if (!settings || !path || path[0] == '\0')
		return;

	for (i = 0, write = 0; i < DC_SETTINGS_RECENT_MAX; i++) {
		if (settings->recent_roms[i][0] == '\0')
			continue;
		if (strcmp(settings->recent_roms[i], path) == 0)
			continue;
		if (write != i) {
			strncpy(settings->recent_roms[write], settings->recent_roms[i],
				sizeof(settings->recent_roms[write]) - 1);
			settings->recent_roms[write][sizeof(settings->recent_roms[write]) - 1] =
				'\0';
		}
		write++;
	}

	for (; write < DC_SETTINGS_RECENT_MAX; write++)
		settings->recent_roms[write][0] = '\0';

	for (i = DC_SETTINGS_RECENT_MAX - 1; i > 0; i--) {
		strncpy(settings->recent_roms[i], settings->recent_roms[i - 1],
			sizeof(settings->recent_roms[i]) - 1);
		settings->recent_roms[i][sizeof(settings->recent_roms[i]) - 1] = '\0';
	}

	strncpy(settings->recent_roms[0], path,
		sizeof(settings->recent_roms[0]) - 1);
	settings->recent_roms[0][sizeof(settings->recent_roms[0]) - 1] = '\0';
	strncpy(settings->last_rom_path, path, sizeof(settings->last_rom_path) - 1);
	settings->last_rom_path[sizeof(settings->last_rom_path) - 1] = '\0';
}

bool dc_settings_take_migration_notice(void)
{
	if (!dc_settings_pending_migration)
		return false;

	dc_settings_pending_migration = false;
	return true;
}

static bool dc_settings_rom_path_readable(const char *path, long *size_out)
{
	FILE *f;
	long size;

	if (!path || path[0] == '\0')
		return false;

	f = fopen(path, "rb");
	if (!f)
		return false;

	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return false;
	}

	size = ftell(f);
	fclose(f);

	if (size < (long)DC_ROM_HEADER_SIZE || size > (8 * 1024 * 1024))
		return false;

	if (size_out)
		*size_out = size;

	return true;
}

bool dc_settings_can_continue(const struct dc_settings *settings)
{
	if (!settings)
		return false;

	return dc_settings_rom_path_readable(settings->last_rom_path, NULL);
}

void dc_settings_continue_label(const struct dc_settings *settings, char *out,
				size_t out_len)
{
	const char *basename;
	const char *slash;

	if (!out || out_len == 0)
		return;

	out[0] = '\0';
	if (!settings || settings->last_rom_path[0] == '\0')
		return;

	basename = settings->last_rom_path;
	slash = strrchr(settings->last_rom_path, '/');
	if (slash && slash[1] != '\0')
		basename = slash + 1;

	snprintf(out, out_len, "Continue: %s", basename);
}
