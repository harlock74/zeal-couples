#include <stdint.h>

#include <zvb_gfx.h>
#include <zgdk.h>
#include <core.h>

#include "main.h"
#include "app_state.h"
#include "assets.h"
#include "render.h"

/*
 * Rendering model:
 * - TILEMAP_LAYER (0): base board and card visuals (back/face/removed-green)
 * - CARD_LAYER/UI_LAYER (1): persistent TMX overlay + moving selector overlay
 *
 * Selector rendering is always temporary. We restore layer-1 tiles from TMX data
 * whenever selector moves away from a slot.
 */

static uint8_t slot_x(uint8_t slot)
{
    /* Matches the 9-column placeholders in assets/solitaire.tmx. */
    return (uint8_t)(1U + ((slot % CARD_COLS) * 4U));
}

static uint8_t slot_y(uint8_t slot)
{
    /* Matches the 4 row anchors in assets/solitaire.tmx. */
    return (uint8_t)(1U + ((slot / CARD_COLS) * 6U));
}

static void place_tile_grid_at_layer(
    uint8_t x0,
    uint8_t y0,
    uint8_t layer,
    const uint8_t grid[SRC_CARD_H][SRC_CARD_W])
{
    /* Generic helper used by both card art and selector overlays. */
    for (uint8_t row = 0; row < SRC_CARD_H; row++) {
        for (uint8_t col = 0; col < SRC_CARD_W; col++) {
            gfx_tilemap_place(&vctx, grid[row][col], layer, (uint8_t)(x0 + col), (uint8_t)(y0 + row));
        }
    }
}

static void clear_selector_overlay_slot(uint8_t slot)
{
    uint8_t x0 = slot_x(slot);
    uint8_t y0 = slot_y(slot);

    /*
     * Important: do not clear with EMPTY_TILE here.
     * Layer 1 contains real background overlay from TMX (transparent decorations),
     * so we must restore those exact tiles cell-by-cell.
     */
    for (uint8_t row = 0; row < SRC_CARD_H; row++) {
        for (uint8_t col = 0; col < SRC_CARD_W; col++) {
            uint8_t x = (uint8_t)(x0 + col);
            uint8_t y = (uint8_t)(y0 + row);
            gfx_tilemap_place(&vctx, assets_get_layout_overlay_tile(x, y), CARD_LAYER, x, y);
        }
    }
}

static void draw_bottom_banner(void)
{
    static const char kClearRow[] = "                         ";
    uint8_t len;
    uint8_t x;

    /*
     * Keep x=2..11 reserved for layer-1 "REVEALS:NN".
     * Clear/draw message only on x=13..37 (25 tiles).
     */
    nprint_string(&vctx, kClearRow, 25, 13, 26);

    if (banner_text[0] == '\0') {
        return;
    }

    len = (uint8_t)str_len(banner_text);
    if (len > 25U) {
        len = 25U;
    }
    x = (uint8_t)(13U + ((25U - len) / 2U));
    nprint_string(&vctx, banner_text, len, x, 26);
}

static uint8_t reveals_digit_tile(uint8_t digit)
{
    if (digit > 9U) {
        digit = 9U;
    }
    return (uint8_t)(REVEALS_DIGIT_TILE_ZERO + digit);
}

static void draw_reveals_counter(void)
{
    /*
     * Query_02:
     * Layer-1 map contains static "REVEALS:" text.
     * We overwrite only the two numeric slots with current gameplay value.
     */
    uint8_t tens = (uint8_t)(reveals_remaining / 10U);
    uint8_t units = (uint8_t)(reveals_remaining % 10U);

    gfx_tilemap_place(&vctx, reveals_digit_tile(tens), UI_LAYER, REVEALS_COUNTER_X_TENS, REVEALS_COUNTER_Y);
    gfx_tilemap_place(&vctx, reveals_digit_tile(units), UI_LAYER, REVEALS_COUNTER_X_UNITS, REVEALS_COUNTER_Y);
}

void place_tile_grid_at(uint8_t x0, uint8_t y0, const uint8_t grid[SRC_CARD_H][SRC_CARD_W])
{
    /* Backward-compatible wrapper kept for callers that target base layer only. */
    place_tile_grid_at_layer(x0, y0, TILEMAP_LAYER, grid);
}

void render_table(void)
{
    /* Full-screen redraw of both static TMX layers. Called during init. */
    for (uint8_t y = 0; y < SCREEN_TILE_H; y++) {
        for (uint8_t x = 0; x < SCREEN_TILE_W; x++) {
            gfx_tilemap_place(&vctx, assets_get_layout_tile(x, y), TILEMAP_LAYER, x, y);
            gfx_tilemap_place(&vctx, assets_get_layout_overlay_tile(x, y), UI_LAYER, x, y);
        }
    }
}

void render_cards(void)
{
    /*
     * Pass 1:
     * - draw each slot state on layer 0
     * - restore layer-1 background under selector footprint for every slot
     */
    for (uint8_t i = 0; i < CARD_COUNT; i++) {
        if (cards[i].removed) {
            assets_build_removed_tile_grid(scratch_tile_grid);
        } else if (cards[i].revealed) {
            assets_build_card_tile_grid(scratch_tile_grid, cards[i].card);
        } else {
            assets_build_back_tile_grid(scratch_tile_grid);
        }

        /* Base card/removed visuals live on layer 0. */
        place_tile_grid_at_layer(slot_x(i), slot_y(i), TILEMAP_LAYER, scratch_tile_grid);
        /* Selector overlay layer is cleared per slot before re-drawing selection. */
        clear_selector_overlay_slot(i);
    }

    /*
     * Pass 2:
     * Selection overlay lives on layer 1 only for selectable-hidden visuals:
     * - hidden card slots
     * - removed slots (as requested for navigation visibility)
     * Revealed face cards must not receive the overlay.
     */
    if (selected_slot < CARD_COUNT &&
        (cards[selected_slot].removed || !cards[selected_slot].revealed))
    {
        assets_build_money_back_tile_grid(scratch_tile_grid);
        place_tile_grid_at_layer(slot_x(selected_slot), slot_y(selected_slot), CARD_LAYER, scratch_tile_grid);
    }

    draw_bottom_banner();
    draw_reveals_counter();
}
