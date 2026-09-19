/*
 * PocketDC Dreamcast frontend — DMG palette helpers.
 * Ported from examples/sdl2/walnut_sdl.c
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <stdio.h>
#include <string.h>

#include "../../extras/sgb.h"
#include "palette.h"

#define DC_NUMBER_OF_PALETTES DC_PALETTE_COUNT

static const char *const dc_palette_names[DC_PALETTE_COUNT] = {
	"Classic Green",
	"Yellow",
	"Blue",
	"Inverted",
	"Grayscale",
	"Brown",
	"Mixed Blue",
	"Kirby",
	"Pastel",
	"Vivid",
	"Sunset",
	"Neon"
};

static inline uint16_t rgb888_to_rgb555(uint32_t rgb)
{
	uint8_t r8 = (rgb >> 16) & 0xFF;
	uint8_t g8 = (rgb >> 8) & 0xFF;
	uint8_t b8 = rgb & 0xFF;
	uint16_t r5 = (r8 * 31 + 127) / 255;
	uint16_t g5 = (g8 * 31 + 127) / 255;
	uint16_t b5 = (b8 * 31 + 127) / 255;

	return (uint16_t)((r5 << 10) | (g5 << 5) | b5);
}

static void sgb_palette_to_rgb555(uint32_t palette_index, uint16_t *dst_rgb555)
{
	const uint32_t *src = &default32sgb_palettes[palette_index * 12];

	for (unsigned i = 0; i < 12; i++)
		dst_rgb555[i] = rgb888_to_rgb555(src[i]);
}

void dc_auto_assign_palette(struct dc_priv *priv, uint8_t game_checksum)
{
	const size_t palette_bytes = 3 * 4 * sizeof(uint16_t);

	switch (game_checksum) {
	case 0x71:
	case 0xFF:
		sgb_palette_to_rgb555(0, priv->selected_palette);
		break;

	case 0x15:
	case 0xDB:
	case 0x95:
		sgb_palette_to_rgb555(16, priv->selected_palette);
		break;

	case 0x19: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 },
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 },
			{ 0x7FFF, 0x7E60, 0x7C00, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	case 0x61:
	case 0x45:
	case 0xD8:
		sgb_palette_to_rgb555(SGB_PALETTE.POKEMON_BLUE, priv->selected_palette);
		break;

	case 0x14:
		sgb_palette_to_rgb555(SGB_PALETTE.POKEMON_RED, priv->selected_palette);
		break;

	case 0x8B:
		sgb_palette_to_rgb555(7, priv->selected_palette);
		break;

	case 0x27:
	case 0x49:
	case 0x5C:
	case 0xB3:
		sgb_palette_to_rgb555(2, priv->selected_palette);
		break;

	case 0x18:
	case 0x6A:
	case 0x4B:
	case 0x6B:
	case 0x86:
		sgb_palette_to_rgb555(SGB_PALETTE.DONKEY_KONG_LAND_1, priv->selected_palette);
		break;

	case 0x70:
		sgb_palette_to_rgb555(SGB_PALETTE.ZELDA_LINKS_AWAKENING, priv->selected_palette);
		break;

	case 0x01:
	case 0x10:
	case 0x29:
	case 0x52:
	case 0x5D:
	case 0x68:
	case 0x6D:
	case 0xF6:
		sgb_palette_to_rgb555(36, priv->selected_palette);
		break;

	default: {
		const uint16_t palette[3][4] = {
			{ 0x3E02, 0x2DE8, 0x1D69, 0x1507 },
			{ 0x3E02, 0x2DE8, 0x1D69, 0x1507 },
			{ 0x3E02, 0x2DE8, 0x1D69, 0x1507 }
		};
		printf("pocketdc: no palette found for checksum 0x%02X\n", game_checksum);
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	}
}

void dc_manual_assign_palette(struct dc_priv *priv, uint8_t selection)
{
	const size_t palette_bytes = 3 * 4 * sizeof(uint16_t);

	switch (selection % DC_NUMBER_OF_PALETTES) {
	case 0: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x2BE0, 0x7D00, 0x0000 },
			{ 0x7FFF, 0x2BE0, 0x7D00, 0x0000 },
			{ 0x7FFF, 0x2BE0, 0x7D00, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 1: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x7FE0, 0x7C00, 0x0000 },
			{ 0x7FFF, 0x7FE0, 0x7C00, 0x0000 },
			{ 0x7FFF, 0x7FE0, 0x7C00, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 2: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 },
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 },
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 3: {
		const uint16_t palette[3][4] = {
			{ 0x0000, 0x0210, 0x7F60, 0x7FFF },
			{ 0x0000, 0x0210, 0x7F60, 0x7FFF },
			{ 0x0000, 0x0210, 0x7F60, 0x7FFF }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 4:
	default: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x5294, 0x294A, 0x0000 },
			{ 0x7FFF, 0x5294, 0x294A, 0x0000 },
			{ 0x7FFF, 0x5294, 0x294A, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 5: {
		const uint16_t palette[3][4] = {
			{ 0x7FF4, 0x7E52, 0x4A5F, 0x0000 },
			{ 0x7FF4, 0x7E52, 0x4A5F, 0x0000 },
			{ 0x7FF4, 0x7E52, 0x4A5F, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 6: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 },
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 },
			{ 0x7F98, 0x6670, 0x41A5, 0x2CC1 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 7: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 },
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 },
			{ 0x7FFF, 0x3FE6, 0x0198, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 8: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 },
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 },
			{ 0x7FFF, 0x463B, 0x2951, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 9: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x3FE6, 0x0200, 0x0000 },
			{ 0x7FFF, 0x329F, 0x001F, 0x0000 },
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 10: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 },
			{ 0x7FFF, 0x3FE6, 0x0200, 0x0000 },
			{ 0x7FFF, 0x329F, 0x001F, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	case 11: {
		const uint16_t palette[3][4] = {
			{ 0x7FFF, 0x329F, 0x001F, 0x0000 },
			{ 0x7FFF, 0x3FE6, 0x0200, 0x0000 },
			{ 0x7FFF, 0x7FE0, 0x3D20, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	}
}

const char *dc_palette_name(uint8_t selection)
{
	return dc_palette_names[selection % DC_PALETTE_COUNT];
}
