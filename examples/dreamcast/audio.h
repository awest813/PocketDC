/*
 * PocketDC Dreamcast frontend — AICA audio via snd_stream.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#ifndef DC_AUDIO_H
#define DC_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

int dc_audio_init(void);
void dc_audio_shutdown(void);
bool dc_audio_ready(void);
void dc_audio_frame(void);
void dc_audio_configure(uint8_t volume, bool muted,
			enum dc_audio_buffer_mode buffer_mode);
void dc_audio_set_menu_music(bool enabled);
void dc_audio_enter_menu(void);
void dc_audio_enter_game(void);
void dc_audio_cdda_hold(void);
void dc_audio_cdda_release(void);

uint8_t dc_audio_read(uint16_t addr);
void dc_audio_write(uint16_t addr, uint8_t val);

#endif /* DC_AUDIO_H */
