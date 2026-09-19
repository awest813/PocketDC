/*
 * PocketDC Dreamcast frontend — ROM cover art.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#ifndef DC_COVER_H
#define DC_COVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui.h"

#define DC_COVER_WIDTH  96
#define DC_COVER_HEIGHT 96

bool dc_rom_read_header(const char *rom_path, char *title, size_t title_len,
			bool *is_cgb, uint8_t *cart_type);
void dc_cover_path_for_rom(const char *rom_path, const char *covers_root,
			   char *cover_path, size_t cover_path_len);
bool dc_cover_load_file(const char *cover_path,
			uint16_t pixels[DC_COVER_HEIGHT][DC_COVER_WIDTH]);
bool dc_cover_load_for_rom(const char *rom_path, const char *covers_root, bool is_cgb,
			   uint16_t pixels[DC_COVER_HEIGHT][DC_COVER_WIDTH]);
void dc_cover_make_placeholder(const char *title, const char *filename,
			       bool is_cgb,
			       uint16_t pixels[DC_COVER_HEIGHT][DC_COVER_WIDTH]);
void dc_cover_draw(uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
		   int x, int y, int w, int h,
		   const uint16_t pixels[DC_COVER_HEIGHT][DC_COVER_WIDTH]);

#endif /* DC_COVER_H */
