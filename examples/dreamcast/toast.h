/*
 * PocketDC Dreamcast frontend — transient on-screen messages.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#ifndef DC_TOAST_H
#define DC_TOAST_H

#include <stdbool.h>
#include <stdint.h>

#include "ui.h"

#define DC_TOAST_DURATION_MS 1800

void dc_toast_show(const char *message, int duration_ms);
bool dc_toast_active(void);
void dc_toast_draw(uint16_t screen[DC_SCREEN_HEIGHT][DC_SCREEN_WIDTH]);
const char *dc_toast_message(void);

#endif /* DC_TOAST_H */
