#pragma once

#include <stdint.h>
#include <zvb_gfx.h>

#define true 1
#define false 0

#define SCREEN_TILE_W 40
#define SCREEN_TILE_H 30
#define SCREEN_PIXEL_W 640
#define SCREEN_PIXEL_H 480

/* Board dimensions: 9 columns x 4 rows = 36 slots/cards. */
#define CARD_COLS 9
#define CARD_ROWS 4
#define CARD_COUNT 36
#define DECK_SIZE 36

/*
 * Query_02 rule:
 * REVEALS counter tracks failed pairs (not single cards), so max is 05.
 * It decreases when a mismatched open pair is closed back to red backs.
 */
#define MAX_REVEAL_PAIRS_WITHOUT_MATCH 5

/*
 * Tile IDs/coordinates used by the REVEALS:NN overlay on layer 1.
 * Exposed here for easy future visual retuning without touching logic code.
 */
#define REVEALS_COUNTER_X_TENS 10
#define REVEALS_COUNTER_X_UNITS 11
#define REVEALS_COUNTER_Y 26
#define REVEALS_DIGIT_TILE_ZERO 115

#define OPEN_HAND_CURSOR_TILE_ID 125
/* Keyboard/SNES uses the same opened-hand art as the SNES mouse cursor. */
#define HAND_SELECTOR_TILE_ID OPEN_HAND_CURSOR_TILE_ID
#define MOUSE_CURSOR_SPRITE_INDEX 0

/*
 * Tile used to fill removed-card slots (3x4 green background block).
 * Exposed as a constant for easy visual tuning later.
 */
#define REMOVED_CARD_BACKGROUND_TILE_ID 32

#define SRC_CARD_W 3
#define SRC_CARD_H 4
#define SHARED_SCRATCH_BUF_SIZE 1024

/*
 * Runtime-reserved transparent tile.
 * Added explicitly in assets.c after tileset load.
 */
#define EMPTY_TILE 255

/* Tilemap layers used by renderer. */
#define TILEMAP_LAYER 0
#define UI_LAYER 1
#define CARD_LAYER 1

/* High-level game mode used by update() flow. */
typedef enum {
    STATE_PLAYING = 0,
    STATE_GAME_OVER,
    STATE_WIN
} GameState;

/* Per-slot runtime flags and compact card id. */
typedef struct {
    uint8_t card;
    uint8_t revealed;
    uint8_t removed;
} SolitaireCard;

void init(void);
void deinit(void);
void update(void);
void draw(void);
