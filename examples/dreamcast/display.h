/*
 * PocketDC Dreamcast frontend — display output (VGA / TV).
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#ifndef DC_DISPLAY_H
#define DC_DISPLAY_H

#include <stdint.h>

enum dc_video_output
{
	DC_VIDEO_OUTPUT_AUTO = 0,
	DC_VIDEO_OUTPUT_VGA,
	DC_VIDEO_OUTPUT_TV,
	DC_VIDEO_OUTPUT_COUNT
};

int dc_display_init(enum dc_video_output mode);
int8_t dc_display_cable_type(void);
const char *dc_video_output_name(enum dc_video_output mode);
const char *dc_display_cable_name(int8_t cable);

#endif /* DC_DISPLAY_H */
