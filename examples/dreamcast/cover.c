/*
 * PocketDC Dreamcast frontend — ROM cover art.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>

#include "cover.h"
#include "font8x8.h"
#include "ui.h"

#define DC_COVER_MAGIC 0x35353557U /* "W555" little-endian */

static uint32_t dc_cover_hash_str(const char *text)
{
	uint32_t hash = 2166136261u;
	const unsigned char *p = (const unsigned char *)text;

	while (p && *p) {
		hash ^= *p++;
		hash *= 16777619u;
	}

	return hash;
}

static uint16_t dc_cover_hsv_to_rgb555(int hue)
{
	const int region = hue / 60;
	const int remainder = (hue % 60) * 255 / 60;
	int r = 0;
	int g = 0;
	int b = 0;

	switch (region) {
	case 0:
		r = 255;
		g = remainder;
		break;
	case 1:
		r = 255 - remainder;
		g = 255;
		break;
	case 2:
		g = 255;
		b = remainder;
		break;
	case 3:
		g = 255 - remainder;
		b = 255;
		break;
	case 4:
		r = remainder;
		b = 255;
		break;
	default:
		r = 255;
		b = 255 - remainder;
		break;
	}

	return (uint16_t)(((r * 31 / 255) << 10) | ((g * 31 / 255) << 5) |
			  (b * 31 / 255));
}

bool dc_rom_read_header(const char *rom_path, char *title, size_t title_len,
			bool *is_cgb, uint8_t *cart_type)
{
	FILE *file;
	uint8_t header[0x150];
	size_t i;

	if (!rom_path || !title || title_len == 0)
		return false;

	file = fopen(rom_path, "rb");
	if (!file)
		return false;

	if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
		fclose(file);
		return false;
	}

	fclose(file);

	title[0] = '\0';
	for (i = 0; i < 16 && title_len > i + 1; i++) {
		const uint8_t ch = header[0x134 + i];

		if (ch < ' ' || ch > '_')
			break;

		title[i] = (char)ch;
		title[i + 1] = '\0';
	}

	if (title[0] == '\0')
		strncpy(title, "Unknown", title_len);

	if (is_cgb)
		*is_cgb = (header[0x143] & 0x80) != 0;

	if (cart_type)
		*cart_type = header[0x147];

	return true;
}

static void dc_cover_rom_stem(const char *rom_path, char *stem, size_t stem_len)
{
	const char *basename;
	const char *slash;
	size_t len;

	if (!rom_path || !stem || stem_len == 0)
		return;

	basename = rom_path;
	slash = strrchr(rom_path, '/');
	if (slash)
		basename = slash + 1;

	strncpy(stem, basename, stem_len - 1);
	stem[stem_len - 1] = '\0';
	len = strlen(stem);
	if (len > 4 && strcasecmp(stem + len - 4, ".gbc") == 0)
		stem[len - 4] = '\0';
	else if (len > 3 && strcasecmp(stem + len - 3, ".gb") == 0)
		stem[len - 3] = '\0';
}

void dc_cover_path_for_rom(const char *rom_path, const char *covers_root,
			   char *cover_path, size_t cover_path_len)
{
	char stem[256];

	if (!rom_path || !covers_root || !cover_path || cover_path_len == 0)
		return;

	dc_cover_rom_stem(rom_path, stem, sizeof(stem));
	snprintf(cover_path, cover_path_len, "%s/%s.w555", covers_root, stem);
}

bool dc_cover_load_for_rom(const char *rom_path, const char *covers_root, bool is_cgb,
			   uint16_t pixels[DC_COVER_HEIGHT][DC_COVER_WIDTH])
{
	char stem[256];
	char path[512];

	if (!rom_path || !covers_root || !pixels)
		return false;

	dc_cover_rom_stem(rom_path, stem, sizeof(stem));

	snprintf(path, sizeof(path), "%s/%s.w555", covers_root, stem);
	if (dc_cover_load_file(path, pixels))
		return true;

	if (is_cgb) {
		snprintf(path, sizeof(path), "%s/boxart/GBC/%s.w555", covers_root, stem);
		if (dc_cover_load_file(path, pixels))
			return true;
		snprintf(path, sizeof(path), "%s/GBC/%s.w555", covers_root, stem);
		if (dc_cover_load_file(path, pixels))
			return true;
	}

	snprintf(path, sizeof(path), "%s/boxart/GB/%s.w555", covers_root, stem);
	if (dc_cover_load_file(path, pixels))
		return true;

	snprintf(path, sizeof(path), "%s/GB/%s.w555", covers_root, stem);
	if (dc_cover_load_file(path, pixels))
		return true;

	if (is_cgb) {
		snprintf(path, sizeof(path), "%s/boxart/GB/%s.w555", covers_root, stem);
		if (dc_cover_load_file(path, pixels))
			return true;
	}

	return false;
}

bool dc_cover_load_file(const char *cover_path,
			uint16_t pixels[DC_COVER_HEIGHT][DC_COVER_WIDTH])
{
	FILE *file;
	uint32_t magic;
	uint16_t width;
	uint16_t height;
	size_t expected;
	size_t read;

	if (!cover_path || !pixels)
		return false;

	file = fopen(cover_path, "rb");
	if (!file)
		return false;

	if (fread(&magic, sizeof(magic), 1, file) != 1 ||
	    fread(&width, sizeof(width), 1, file) != 1 ||
	    fread(&height, sizeof(height), 1, file) != 1) {
		fclose(file);
		return false;
	}

	if (magic != DC_COVER_MAGIC || width != DC_COVER_WIDTH ||
	    height != DC_COVER_HEIGHT) {
		fclose(file);
		return false;
	}

	expected = (size_t)DC_COVER_WIDTH * DC_COVER_HEIGHT * sizeof(uint16_t);
	read = fread(pixels, 1, expected, file);
	fclose(file);

	return read == expected;
}

static void dc_cover_draw_initials(uint16_t pixels[DC_COVER_HEIGHT][DC_COVER_WIDTH],
				   const char *title)
{
	char initials[4] = { '?', '\0', '\0', '\0' };
	const char *src = title ? title : "?";
	size_t count = 0;
	size_t i;

	for (i = 0; src[i] != '\0' && count < 3; i++) {
		if (src[i] == ' ')
			continue;
		initials[count++] =
			(char)((src[i] >= 'a' && src[i] <= 'z') ? src[i] - 32 : src[i]);
	}
	initials[count] = '\0';

	for (i = 0; initials[i] != '\0'; i++) {
		const int glyph_x = 24 + (int)i * 24;
		int row;
		int col;

		for (row = 0; row < 8; row++) {
			const uint8_t bits =
				dc_font8x8_basic[(unsigned char)initials[i]][row];

			for (col = 0; col < 8; col++) {
				const int px = glyph_x + col * 2;
				const int py = 40 + row * 2;

				if (bits & (1U << (7 - col))) {
					int yy;
					int xx;

					for (yy = 0; yy < 2; yy++) {
						for (xx = 0; xx < 2; xx++) {
							if (px + xx < DC_COVER_WIDTH &&
							    py + yy < DC_COVER_HEIGHT)
								pixels[py + yy][px + xx] = 0x7FFF;
						}
					}
				}
			}
		}
	}
}

void dc_cover_make_placeholder(const char *title, const char *filename,
			       bool is_cgb,
			       uint16_t pixels[DC_COVER_HEIGHT][DC_COVER_WIDTH])
{
	const uint32_t hash = dc_cover_hash_str(filename ? filename : title);
	const int hue = (int)(hash % 300);
	const uint16_t top = dc_cover_hsv_to_rgb555(hue);
	const uint16_t bottom = dc_cover_hsv_to_rgb555((hue + 40) % 360);
	int y;
	int x;

	for (y = 0; y < DC_COVER_HEIGHT; y++) {
		const uint16_t color =
			(y < DC_COVER_HEIGHT / 2) ? top : bottom;

		for (x = 0; x < DC_COVER_WIDTH; x++)
			pixels[y][x] = color;
	}

	{
		const uint16_t badge_bg = is_cgb ? 0x001F : 0x0320;
		const int badge_y = DC_COVER_HEIGHT - 18;

		for (y = badge_y; y < DC_COVER_HEIGHT; y++) {
			for (x = 0; x < DC_COVER_WIDTH; x++)
				pixels[y][x] = badge_bg;
		}
	}

	dc_cover_draw_initials(pixels, title);
}

void dc_cover_draw(uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
		   int x, int y, int w, int h,
		   const uint16_t pixels[DC_COVER_HEIGHT][DC_COVER_WIDTH])
{
	int row;
	int col;

	if (!screen || !pixels || w <= 0 || h <= 0)
		return;

	for (row = 0; row < h; row++) {
		const int src_y = row * DC_COVER_HEIGHT / h;
		const int dst_y = y + row;

		if (dst_y < 0 || dst_y >= DC_SCREEN_HEIGHT)
			continue;

		for (col = 0; col < w; col++) {
			const int src_x = col * DC_COVER_WIDTH / w;
			const int dst_x = x + col;

			if (dst_x < 0 || dst_x >= DC_SCREEN_WIDTH)
				continue;

			screen[dst_y][dst_x] = pixels[src_y][src_x];
		}
	}
}
