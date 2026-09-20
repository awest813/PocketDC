/*
 * PocketDC Dreamcast frontend — PVR hardware sprite presentation.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kos.h>
#include <dc/pvr.h>
#include <dc/video.h>

#include "toast.h"
#include "ui.h"
#include "video.h"

#define DC_PVR_PACK_UV(u, v) \
	((uint32_t)(((int)((u) * 1024.0f)) & 0x3ff) | \
	 ((((int)((v) * 1024.0f)) & 0x3ff) << 10))

/* pvr_txr_load() copies bytes; count must be a multiple of 32. */
#define DC_VIDEO_TEX_BYTES(pixels) ((size_t)(pixels) * sizeof(uint16_t))

static pvr_ptr_t tex;
static pvr_ptr_t ui_tex;
static bool video_ready;
static pvr_sprite_cxt_t game_sprite_cxt;
static pvr_sprite_hdr_t game_sprite_hdr;
static pvr_sprite_cxt_t ui_sprite_cxt;
static pvr_sprite_hdr_t ui_sprite_hdr;
static uint16_t upload_buf[DC_FB_TEX_WIDTH * DC_FB_TEX_HEIGHT];
static uint16_t ui_upload_buf[DC_UI_TEX_WIDTH * DC_UI_TEX_HEIGHT];
static enum dc_scale_mode scale_mode = DC_SCALE_3X;

static const char *const dc_scale_mode_names[DC_SCALE_COUNT] = {
	"3x Integer",
	"Widescreen",
	"4x Integer",
	"Full Screen"
};

static void dc_video_init_sprite_cxt(pvr_sprite_cxt_t *cxt, pvr_sprite_hdr_t *hdr,
				     pvr_list_t list, int tex_fmt,
				     int tex_w, int tex_h, pvr_ptr_t tex_addr,
				     int filter)
{
	pvr_sprite_cxt_txr(cxt, list, tex_fmt, tex_w, tex_h, tex_addr, filter);
	cxt->gen.culling = PVR_CULLING_NONE;
	cxt->depth.write = false;
	pvr_sprite_compile(hdr, cxt);
	hdr->argb = 0xFFFFFFFF;
	hdr->oargb = 0;
}

static void dc_video_draw_sprite_uv(const pvr_sprite_hdr_t *hdr,
				    int x, int y, int w, int h,
				    float u0, float v0, float u1, float v1)
{
	pvr_sprite_txr_t vert;

	vert.flags = PVR_CMD_VERTEX;
	vert.ax = (float)x;
	vert.ay = (float)y;
	vert.az = 1.0f;
	vert.bx = (float)(x + w);
	vert.by = (float)y;
	vert.bz = 1.0f;
	vert.cx = (float)x;
	vert.cy = (float)(y + h);
	vert.cz = 1.0f;
	vert.dx = (float)(x + w);
	vert.dy = (float)(y + h);
	vert.dummy = 0;
	vert.auv = DC_PVR_PACK_UV(u0, v0);
	vert.buv = DC_PVR_PACK_UV(u1, v0);
	vert.cuv = DC_PVR_PACK_UV(u0, v1);

	pvr_prim(hdr, sizeof(*hdr));
	pvr_prim(&vert, sizeof(vert));
}

static void dc_video_draw_sprite(const pvr_sprite_hdr_t *hdr,
				 int x, int y, int w, int h,
				 float u1, float v1)
{
	dc_video_draw_sprite_uv(hdr, x, y, w, h, 0.0f, 0.0f, u1, v1);
}

static void dc_video_draw_game_sprite(int x, int y, int w, int h)
{
	const float u1 = (float)LCD_WIDTH / (float)DC_FB_TEX_WIDTH;
	const float v1 = (float)LCD_HEIGHT / (float)DC_FB_TEX_HEIGHT;

	dc_video_draw_sprite(&game_sprite_hdr, x, y, w, h, u1, v1);
}

static int dc_video_game_filter(void)
{
	switch (scale_mode) {
	case DC_SCALE_WIDE:
	case DC_SCALE_FULL:
		return PVR_FILTER_BILINEAR;
	default:
		return PVR_FILTER_NONE;
	}
}

static void dc_video_update_game_sprite_cxt(void)
{
	const int tex_fmt = PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED;

	dc_video_init_sprite_cxt(&game_sprite_cxt, &game_sprite_hdr, PVR_LIST_OP_POLY,
				 tex_fmt, DC_FB_TEX_WIDTH, DC_FB_TEX_HEIGHT, tex,
				 dc_video_game_filter());
}

static void dc_video_compute_layout(int *draw_x, int *draw_y, int *draw_w, int *draw_h)
{
	switch (scale_mode) {
	case DC_SCALE_WIDE:
		*draw_w = 640;
		*draw_h = LCD_HEIGHT * 3;
		break;
	case DC_SCALE_4X:
		*draw_w = LCD_WIDTH * 4;
		*draw_h = LCD_HEIGHT * 4;
		break;
	case DC_SCALE_FULL:
		*draw_w = 640;
		*draw_h = 480;
		break;
	case DC_SCALE_3X:
	default:
		*draw_w = LCD_WIDTH * 3;
		*draw_h = LCD_HEIGHT * 3;
		break;
	}

	*draw_x = (640 - *draw_w) / 2;
	*draw_y = (480 - *draw_h) / 2;
}

void dc_video_set_scale_mode(enum dc_scale_mode mode)
{
	if (mode >= DC_SCALE_COUNT)
		mode = DC_SCALE_3X;

	if (scale_mode == mode)
		return;

	scale_mode = mode;
	if (video_ready)
		dc_video_update_game_sprite_cxt();
}

enum dc_scale_mode dc_video_get_scale_mode(void)
{
	return scale_mode;
}

const char *dc_video_scale_mode_name(enum dc_scale_mode mode)
{
	if (mode >= DC_SCALE_COUNT)
		return dc_scale_mode_names[DC_SCALE_3X];

	return dc_scale_mode_names[mode];
}

int dc_video_init(void)
{
	const int tex_fmt = PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED;

	if (pvr_init_defaults() < 0)
		return -1;

	tex = pvr_mem_malloc(DC_FB_TEX_WIDTH * DC_FB_TEX_HEIGHT * 2);
	if (!tex)
		return -1;

	memset(upload_buf, 0, sizeof(upload_buf));
	pvr_txr_load(upload_buf, tex,
		     DC_VIDEO_TEX_BYTES(DC_FB_TEX_WIDTH * DC_FB_TEX_HEIGHT));
	dc_video_init_sprite_cxt(&game_sprite_cxt, &game_sprite_hdr, PVR_LIST_OP_POLY,
				 tex_fmt, DC_FB_TEX_WIDTH, DC_FB_TEX_HEIGHT, tex,
				 PVR_FILTER_NONE);

	ui_tex = pvr_mem_malloc(DC_UI_TEX_WIDTH * DC_UI_TEX_HEIGHT * 2);
	if (!ui_tex)
		return -1;

	memset(ui_upload_buf, 0, sizeof(ui_upload_buf));
	pvr_txr_load(ui_upload_buf, ui_tex,
		     DC_VIDEO_TEX_BYTES(DC_UI_TEX_WIDTH * DC_UI_TEX_HEIGHT));
	dc_video_init_sprite_cxt(&ui_sprite_cxt, &ui_sprite_hdr, PVR_LIST_OP_POLY,
				 tex_fmt, DC_UI_TEX_WIDTH, DC_UI_TEX_HEIGHT, ui_tex,
				 PVR_FILTER_NONE);

	video_ready = true;
	dc_video_update_game_sprite_cxt();
	return 0;
}

void dc_video_shutdown(void)
{
	video_ready = false;

	if (tex) {
		pvr_mem_free(tex);
		tex = NULL;
	}
	if (ui_tex) {
		pvr_mem_free(ui_tex);
		ui_tex = NULL;
	}
	pvr_shutdown();
}

static bool dc_video_upload_overlays(const char *status_text, bool *has_status,
				     bool *has_toast)
{
	const int status_h = 20;
	const int toast_h = 28;
	int tex_rows;
	unsigned int y;

	*has_status = status_text && status_text[0] != '\0';
	*has_toast = dc_toast_active();
	if (!*has_status && !*has_toast)
		return false;

	if (*has_status) {
		uint16_t strip[20][DC_SCREEN_WIDTH];

		memset(strip, 0, sizeof(strip));
		dc_ui_fill_rect((uint16_t (*)[DC_SCREEN_WIDTH])strip, 0, 0,
				DC_SCREEN_WIDTH, status_h, DC_UI_COLOR_TOAST_BG);
		dc_ui_draw_text_clipped((uint16_t (*)[DC_SCREEN_WIDTH])strip, 8, 6,
					DC_SCREEN_WIDTH - 16, status_text,
					DC_UI_COLOR_TOAST_FG, DC_UI_COLOR_TOAST_BG);

		for (y = 0; y < (unsigned int)status_h; y++)
			memcpy(&ui_upload_buf[y * DC_UI_TEX_WIDTH], strip[y],
			       DC_SCREEN_WIDTH * sizeof(uint16_t));
	}

	if (*has_toast) {
		uint16_t strip[28][DC_SCREEN_WIDTH];
		const int tex_row = *has_status ? status_h : 0;

		memset(strip, 0, sizeof(strip));
		dc_ui_fill_rect((uint16_t (*)[DC_SCREEN_WIDTH])strip, 0, 0,
				DC_SCREEN_WIDTH, toast_h, DC_UI_COLOR_TOAST_BG);
		dc_ui_draw_text((uint16_t (*)[DC_SCREEN_WIDTH])strip, 12, 10,
				dc_toast_message(), DC_UI_COLOR_TOAST_FG,
				DC_UI_COLOR_TOAST_BG);

		for (y = 0; y < (unsigned int)toast_h; y++)
			memcpy(&ui_upload_buf[(tex_row + y) * DC_UI_TEX_WIDTH], strip[y],
			       DC_SCREEN_WIDTH * sizeof(uint16_t));
	}

	tex_rows = (*has_status ? status_h : 0) + (*has_toast ? toast_h : 0);
	pvr_txr_load(ui_upload_buf, ui_tex,
		     DC_VIDEO_TEX_BYTES(DC_UI_TEX_WIDTH * tex_rows));
	return true;
}

static void dc_video_draw_overlay_sprites(bool has_status, bool has_toast)
{
	const int status_h = 20;
	const int toast_h = 28;
	const int toast_y = DC_UI_FOOTER_Y;
	const float u1 = (float)DC_SCREEN_WIDTH / (float)DC_UI_TEX_WIDTH;

	if (has_status) {
		const float v1 = (float)status_h / (float)DC_UI_TEX_HEIGHT;

		dc_video_draw_sprite_uv(&ui_sprite_hdr, 0, 0, DC_SCREEN_WIDTH, status_h,
					0.0f, 0.0f, u1, v1);
	}

	if (has_toast) {
		const float v0 = (float)(has_status ? status_h : 0) /
				 (float)DC_UI_TEX_HEIGHT;
		const float v1 = (float)((has_status ? status_h : 0) + toast_h) /
				 (float)DC_UI_TEX_HEIGHT;

		dc_video_draw_sprite_uv(&ui_sprite_hdr, 0, toast_y, DC_SCREEN_WIDTH, toast_h,
					0.0f, v0, u1, v1);
	}
}

void dc_video_present(const struct dc_priv *priv, const char *status_text)
{
	unsigned int y;
	bool has_status = false;
	bool has_toast = false;
	bool overlays;

	for (y = 0; y < LCD_HEIGHT; y++)
		memcpy(&upload_buf[y * DC_FB_TEX_WIDTH], priv->fb[y],
		       LCD_WIDTH * sizeof(uint16_t));

	pvr_txr_load(upload_buf, tex,
		     DC_VIDEO_TEX_BYTES(DC_FB_TEX_WIDTH * DC_FB_TEX_HEIGHT));
	overlays = dc_video_upload_overlays(status_text, &has_status, &has_toast);

	pvr_wait_ready();
	pvr_scene_begin();
	pvr_list_begin(PVR_LIST_OP_POLY);

	{
		int draw_x;
		int draw_y;
		int draw_w;
		int draw_h;

		dc_video_compute_layout(&draw_x, &draw_y, &draw_w, &draw_h);
		dc_video_draw_game_sprite(draw_x, draw_y, draw_w, draw_h);
	}

	if (overlays)
		dc_video_draw_overlay_sprites(has_status, has_toast);

	pvr_list_finish();
	pvr_scene_finish();
}

void dc_video_present_screen(const uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH])
{
	unsigned int y;

	for (y = 0; y < DC_SCREEN_HEIGHT; y++)
		memcpy(&ui_upload_buf[y * DC_UI_TEX_WIDTH], screen[y],
		       DC_SCREEN_WIDTH * sizeof(uint16_t));

	pvr_txr_load(ui_upload_buf, ui_tex,
		     DC_VIDEO_TEX_BYTES(DC_UI_TEX_WIDTH * DC_UI_TEX_HEIGHT));

	pvr_wait_ready();
	pvr_scene_begin();
	pvr_list_begin(PVR_LIST_OP_POLY);
	dc_video_draw_sprite(&ui_sprite_hdr, 0, 0, DC_SCREEN_WIDTH, DC_SCREEN_HEIGHT,
			     (float)DC_SCREEN_WIDTH / (float)DC_UI_TEX_WIDTH,
			     (float)DC_SCREEN_HEIGHT / (float)DC_UI_TEX_HEIGHT);
	pvr_list_finish();
	pvr_scene_finish();
}
