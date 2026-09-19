/*
 * PocketDC Dreamcast frontend — DMG palette helpers.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#ifndef DC_PALETTE_H
#define DC_PALETTE_H

#include <stdint.h>

#include "dc_priv.h"

#define DC_PALETTE_COUNT 12

void dc_auto_assign_palette(struct dc_priv *priv, uint8_t game_checksum);
void dc_manual_assign_palette(struct dc_priv *priv, uint8_t selection);
const char *dc_palette_name(uint8_t selection);

#endif /* DC_PALETTE_H */
