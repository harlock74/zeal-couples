#pragma once

#include <stdint.h>

#include "main.h"

/* Place a prepared 3x4 tile block on the base layer. */
void place_tile_grid_at(uint8_t x0, uint8_t y0, const uint8_t grid[SRC_CARD_H][SRC_CARD_W]);

/* Draw static map layers once at startup. */
void render_table(void);

/* Draw dynamic card states, keyboard/SNES hand selector, and banner each redraw. */
void render_cards(void);

/* Sprite-backed opened hand cursor used by SNES mouse input. */
void render_mouse_cursor_init(void);
void render_mouse_cursor_show(uint16_t pixel_x, uint16_t pixel_y);
void render_mouse_cursor_hide(void);
