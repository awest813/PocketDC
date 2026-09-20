/*
 * PocketDC Dreamcast frontend — transient on-screen messages.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <kos.h>
#include <string.h>

#include "toast.h"
#include "ui.h"

static char toast_message[56];
static uint64_t toast_expires;

void dc_toast_show(const char *message, int duration_ms)
{
	if (!message || message[0] == '\0')
		return;

	if (duration_ms < 0)
		duration_ms = 0;

	strncpy(toast_message, message, sizeof(toast_message) - 1);
	toast_message[sizeof(toast_message) - 1] = '\0';
	toast_expires = timer_ms_gettime64() + (uint64_t)duration_ms;
}

bool dc_toast_active(void)
{
	if (toast_message[0] == '\0')
		return false;

	if (timer_ms_gettime64() >= toast_expires) {
		toast_message[0] = '\0';
		return false;
	}

	return true;
}

void dc_toast_draw(uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH])
{
	if (!dc_toast_active())
		return;

	dc_ui_draw_toast_bar(screen, toast_message);
}

const char *dc_toast_message(void)
{
	return toast_message;
}
