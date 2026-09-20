/*
 * PocketDC Dreamcast frontend — video output.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#ifndef DC_VIDEO_H
#define DC_VIDEO_H

#include <stdbool.h>

#include "dc_priv.h"
#include "ui.h"

#define DC_UI_TEX_WIDTH  1024
#define DC_UI_TEX_HEIGHT 512

enum dc_scale_mode
{
	DC_SCALE_3X = 0,
	DC_SCALE_WIDE,
	DC_SCALE_4X,
	DC_SCALE_FULL,
	DC_SCALE_COUNT
};

int dc_video_init(void);
void dc_video_shutdown(void);
void dc_video_set_scale_mode(enum dc_scale_mode mode);
enum dc_scale_mode dc_video_get_scale_mode(void);
const char *dc_video_scale_mode_name(enum dc_scale_mode mode);
void dc_video_present(const struct dc_priv *priv, const char *status_text);
void dc_video_present_screen(const uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH]);

#endif /* DC_VIDEO_H */
