/*
 * PocketDC Dreamcast frontend — Maple controller input.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#ifndef DC_INPUT_H
#define DC_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "../../walnut_cgb.h"

#define DC_INPUT_ANALOG_THRESHOLD 64
#define DC_INPUT_REPEAT_DELAY     18
#define DC_INPUT_REPEAT_RATE      4
#define DC_INPUT_FRAME_MS         16

struct dc_input_state
{
	uint8_t joypad;
	bool reset_game;
	bool exit_requested;
	bool pause_requested;
	bool cycle_palette;
	bool toggle_frameskip;
	bool cycle_scale;
	bool system_exit;
	unsigned int fast_mode;
};

void dc_input_init(void);
/** Clear held-button edge state after context switches (game ↔ menu). */
void dc_input_flush_edges(void);
void dc_input_poll(struct dc_input_state *state, struct gb_s *gb);

/**
 * Resolve one navigation axis to -1, 0, or +1.
 * Digital d-pad wins over analog so mixed input cannot fire both directions.
 */
int dc_input_axis(bool dpad_negative, bool dpad_positive, int16_t analog,
		  int threshold);

/** Hold-to-repeat helper for menu and browser navigation. */
bool dc_input_repeat(bool pressed, int *timer);

/**
 * Repeat on a resolved axis; resets when the axis direction changes.
 * Returns the active axis (-1, 0, or +1) when a repeat event fires.
 */
int dc_input_repeat_axis(int axis, int *timer, int *last_axis);

/** Fire once when an axis leaves neutral; resets when the axis returns to 0. */
int dc_input_axis_edge(int axis, int *last_axis);

/** True when A+B+X+Y+Start are held (Dreamcast soft-reset convention). */
bool dc_input_quit_combo(uint32_t buttons);

#endif /* DC_INPUT_H */
