#pragma once

#include <stdint.h>
#include <zvb_gfx.h>

#include "main.h"

/* Shared graphics context owned by main.c. */
extern gfx_context vctx;

/* Shared game-state arrays/flags owned by main.c. */
extern SolitaireCard cards[CARD_COUNT];
extern uint8_t deck[DECK_SIZE];
extern uint8_t deck_pos;
extern GameState state;
extern uint16_t entropy;
extern uint8_t selected_slot;
extern uint8_t hand_selector_visible;
extern uint8_t reveals_remaining;

/* Bottom status banner text rendered by render.c. */
extern char banner_text[36];

/* Shared temporary buffer used by renderer to compose card tiles. */
extern uint8_t scratch_tile_grid[SRC_CARD_H][SRC_CARD_W];
