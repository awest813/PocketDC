/*
 * PocketDC Dreamcast frontend — display output (VGA / TV).
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <stdio.h>

#include <kos.h>
#include <dc/video.h>

#include "display.h"

static const char *const dc_video_output_names[DC_VIDEO_OUTPUT_COUNT] = {
	"Auto",
	"VGA 640x480",
	"TV 640x480"
};

int8_t dc_display_cable_type(void)
{
	return vid_check_cable();
}

const char *dc_display_cable_name(int8_t cable)
{
	switch (cable) {
	case CT_VGA:
		return "VGA";
	case CT_RGB:
		return "RGB/SCART";
	case CT_COMPOSITE:
		return "Composite";
	case CT_NONE:
	default:
		return "Unknown";
	}
}

const char *dc_video_output_name(enum dc_video_output mode)
{
	if (mode >= DC_VIDEO_OUTPUT_COUNT)
		return dc_video_output_names[DC_VIDEO_OUTPUT_AUTO];

	return dc_video_output_names[mode];
}

static int dc_display_mode_for_output(enum dc_video_output mode, int8_t cable)
{
	switch (mode) {
	case DC_VIDEO_OUTPUT_VGA:
		return DM_640x480_VGA;
	case DC_VIDEO_OUTPUT_TV:
		return DM_640x480_NTSC_IL;
	case DC_VIDEO_OUTPUT_AUTO:
	default:
		if (cable == CT_VGA)
			return DM_640x480_VGA;
		return DM_640x480_NTSC_IL;
	}
}

static enum dc_video_output dc_display_current_mode = DC_VIDEO_OUTPUT_COUNT;

int dc_display_init(enum dc_video_output mode)
{
	int8_t cable;
	int dm;

	if (mode >= DC_VIDEO_OUTPUT_COUNT)
		mode = DC_VIDEO_OUTPUT_AUTO;

	if (mode == dc_display_current_mode)
		return 0;

	cable = vid_check_cable();
	dm = dc_display_mode_for_output(mode, cable);

	vid_set_mode(dm, PM_RGB555);
	dc_display_current_mode = mode;
	printf("pocketdc: display %s on %s cable\n",
	       dc_video_output_name(mode), dc_display_cable_name(cable));
	return 0;
}
