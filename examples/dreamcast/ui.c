/*
 * PocketDC Dreamcast frontend — simple 640x480 UI drawing.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <string.h>

#include "font8x8.h"
#include "ui.h"

void dc_ui_clear(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH], uint16_t color)
{
	unsigned int x;
	unsigned int y;

	/*
	 * memset fills bytes, so it only works when both halves of the
	 * 16-bit colour are equal. Fill the first row pixel-by-pixel, then
	 * replicate it to the rest of the framebuffer.
	 */
	for (x = 0; x < DC_SCREEN_WIDTH; x++)
		fb[0][x] = color;

	for (y = 1; y < DC_SCREEN_HEIGHT; y++)
		memcpy(fb[y], fb[0], DC_SCREEN_WIDTH * sizeof(uint16_t));
}

void dc_ui_fill_rect(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
		     int x, int y, int w, int h, uint16_t color)
{
	int row;
	int col;

	for (row = y; row < y + h; row++) {
		if (row < 0 || row >= DC_SCREEN_HEIGHT)
			continue;

		for (col = x; col < x + w; col++) {
			if (col < 0 || col >= DC_SCREEN_WIDTH)
				continue;

			fb[row][col] = color;
		}
	}
}

static void dc_ui_draw_glyph(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
			     int x, int y, unsigned char c,
			     uint16_t fg, uint16_t bg)
{
	const uint8_t *glyph;
	int row;
	int col;

	if (c > 0x7F)
		c = '?';

	glyph = dc_font8x8_basic[c];

	for (row = 0; row < 8; row++) {
		uint8_t bits = glyph[row];

		for (col = 0; col < 8; col++) {
			const int px = x + col;
			const int py = y + row;

			if (px < 0 || px >= DC_SCREEN_WIDTH ||
			    py < 0 || py >= DC_SCREEN_HEIGHT)
				continue;

			fb[py][px] = (bits & (1U << (7 - col))) ? fg : bg;
		}
	}
}

void dc_ui_draw_text(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
		     int x, int y, const char *text,
		     uint16_t fg, uint16_t bg)
{
	int cursor_x = x;

	while (text && *text) {
		dc_ui_draw_glyph(fb, cursor_x, y, (unsigned char)*text, fg, bg);
		cursor_x += 8;
		text++;
	}
}

void dc_ui_draw_text_clipped(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
			     int x, int y, int max_width, const char *text,
			     uint16_t fg, uint16_t bg)
{
	int cursor_x = x;
	const int max_chars = max_width / 8;

	if (max_chars <= 0 || !text)
		return;

	{
		int i;

		for (i = 0; text[i] != '\0' && i < max_chars; i++) {
			dc_ui_draw_glyph(fb, cursor_x, y, (unsigned char)text[i], fg, bg);
			cursor_x += 8;
		}
	}
}

void dc_ui_draw_text_ellipsis(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
			      int x, int y, int max_width, const char *text,
			      uint16_t fg, uint16_t bg)
{
	char clipped[80];
	size_t len;
	size_t max_chars;
	size_t i;

	if (!text || max_width < 24)
		return;

	max_chars = (size_t)(max_width / 8);
	if (max_chars >= sizeof(clipped))
		max_chars = sizeof(clipped) - 1;

	len = strlen(text);
	if (len <= max_chars) {
		dc_ui_draw_text(fb, x, y, text, fg, bg);
		return;
	}

	for (i = 0; i < max_chars - 3; i++)
		clipped[i] = text[i];
	clipped[i++] = '.';
	clipped[i++] = '.';
	clipped[i++] = '.';
	clipped[i] = '\0';

	dc_ui_draw_text(fb, x, y, clipped, fg, bg);
}

void dc_ui_draw_toast_bar(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
			  const char *message)
{
	const int bar_h = 28;

	dc_ui_fill_rect(fb, 0, DC_UI_FOOTER_Y, DC_SCREEN_WIDTH, bar_h,
			DC_UI_COLOR_TOAST_BG);
	dc_ui_draw_text(fb, DC_UI_MARGIN_X, DC_UI_FOOTER_Y + 10,
			message ? message : "", DC_UI_COLOR_TOAST_FG,
			DC_UI_COLOR_TOAST_BG);
}

void dc_ui_draw_header(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
		       const char *title, const char *subtitle)
{
	dc_ui_fill_rect(fb, 0, 0, DC_SCREEN_WIDTH, DC_UI_HEADER_HEIGHT,
			DC_UI_COLOR_HEADER);
	dc_ui_draw_text(fb, DC_UI_MARGIN_X, 12, title ? title : "",
			DC_UI_COLOR_BG, DC_UI_COLOR_HEADER);

	if (subtitle && subtitle[0] != '\0')
		dc_ui_draw_text_clipped(fb, DC_UI_MARGIN_X, 28,
					DC_SCREEN_WIDTH - DC_UI_MARGIN_X * 2,
					subtitle, DC_UI_COLOR_BG, DC_UI_COLOR_HEADER);
}

void dc_ui_draw_footer(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
		       const char *text)
{
	if (!text || text[0] == '\0')
		return;

	dc_ui_draw_text_clipped(fb, DC_UI_MARGIN_X, DC_UI_FOOTER_Y,
				DC_SCREEN_WIDTH - DC_UI_MARGIN_X * 2,
				text, DC_UI_COLOR_DIM, DC_UI_COLOR_BG);
}

void dc_ui_draw_panel(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
		      int x, int y, int w, int h, uint16_t fill)
{
	dc_ui_fill_rect(fb, x, y, w, h, fill);
	dc_ui_fill_rect(fb, x, y, w, 1, DC_UI_COLOR_BORDER);
	dc_ui_fill_rect(fb, x, y + h - 1, w, 1, DC_UI_COLOR_BORDER);
	dc_ui_fill_rect(fb, x, y, 1, h, DC_UI_COLOR_BORDER);
	dc_ui_fill_rect(fb, x + w - 1, y, 1, h, DC_UI_COLOR_BORDER);
}

void dc_ui_draw_loading(uint16_t fb[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH],
			const char *title, const char *subtitle, int progress_pct)
{
	const int bar_x = 200;
	const int bar_y = 220;
	const int bar_w = 240;
	const int bar_h = 8;
	int fill_w;

	if (progress_pct < 0)
		progress_pct = 0;
	if (progress_pct > 100)
		progress_pct = 100;

	fill_w = bar_w * progress_pct / 100;
	if (fill_w < 8 && progress_pct > 0)
		fill_w = 8;

	dc_ui_clear(fb, DC_UI_COLOR_BG);
	dc_ui_draw_header(fb, title ? title : "Loading", NULL);
	dc_ui_fill_rect(fb, bar_x, bar_y, bar_w, bar_h, DC_UI_COLOR_TRACK);
	if (fill_w > 0)
		dc_ui_fill_rect(fb, bar_x, bar_y, fill_w, bar_h, DC_UI_COLOR_ACCENT);
	dc_ui_draw_text(fb, bar_x, 248, subtitle ? subtitle : "Please wait...",
			DC_UI_COLOR_FG, DC_UI_COLOR_BG);
}
