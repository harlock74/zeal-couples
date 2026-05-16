#include <stdlib.h>
#include <stdint.h>

#include <zos_sys.h>
#include <zos_video.h>
#include <zvb_gfx.h>
#include <zgdk.h>
#include <core.h>

#include "main.h"
#include "app_state.h"
#include "assets.h"
#include "gameplay.h"
#include "render.h"
#include "splash.h"
#include "audio.h"
#include "input.h"

/*
 * Main runtime module responsibilities:
 * - input polling and key-event interpretation
 * - turn-state transitions (reveal, match, mismatch reset)
 * - entropy collection and one-time RNG seeding before first deal
 * - high-level redraw scheduling
 */

gfx_context vctx;

/* Core game state shared with render/gameplay modules through app_state.h. */
SolitaireCard cards[CARD_COUNT];
uint8_t deck[DECK_SIZE];
uint8_t deck_pos = 0;
GameState state = STATE_PLAYING;
uint8_t selected_slot = 0;
uint8_t hand_selector_visible = 1;

/* Mixed from player timing/input before first RNG seed. */
uint16_t entropy = 1;

char banner_text[36] = "";

uint8_t scratch_tile_grid[SRC_CARD_H][SRC_CARD_W];

/* Runtime-only control flags and temporary turn state. */
static uint8_t needs_redraw = 1;
static uint8_t rng_seeded = 0;
static uint16_t entropy_tick = 0;
static uint8_t open_slots[2];
static uint8_t open_count = 0;
static uint8_t couple_found_banner_active = 0;
static uint8_t mouse_cursor_visible = 0;
static uint8_t mouse_cursor_dirty = 0;
static int16_t mouse_pixel_x = 0;
static int16_t mouse_pixel_y = 0;
uint8_t reveals_remaining = MAX_REVEAL_PAIRS_WITHOUT_MATCH;

/* Convert grid coordinates back into the linear [0..CARD_COUNT-1] slot index. */
static uint8_t slot_from_row_col(uint8_t row, uint8_t col)
{
    return (uint8_t)(row * CARD_COLS + col);
}

static uint8_t slot_row(uint8_t slot)
{
    return (uint8_t)(slot / CARD_COLS);
}

static uint8_t slot_col(uint8_t slot)
{
    return (uint8_t)(slot % CARD_COLS);
}

static uint8_t slot_tile_x(uint8_t slot)
{
    return (uint8_t)(1U + (slot_col(slot) * 4U));
}

static uint8_t slot_tile_y(uint8_t slot)
{
    return (uint8_t)(1U + (slot_row(slot) * 6U));
}

static uint8_t slot_is_available(uint8_t slot)
{
    return (slot < CARD_COUNT && !cards[slot].removed) ? true : false;
}

static uint8_t abs_u8_diff(uint8_t a, uint8_t b)
{
    return (a > b) ? (uint8_t)(a - b) : (uint8_t)(b - a);
}

static void sync_mouse_cursor_to_selected_slot(void)
{
    mouse_pixel_x = (int16_t)slot_tile_x(selected_slot) * 16;
    mouse_pixel_y = (int16_t)slot_tile_y(selected_slot) * 16;
    mouse_cursor_dirty = 1;
}

static void show_keyboard_hand_selector(void)
{
    mouse_cursor_visible = 0;
    if (!hand_selector_visible) {
        needs_redraw = 1;
    }
    hand_selector_visible = 1;
    render_mouse_cursor_hide();
}

static uint8_t select_nearest_available_slot(uint8_t preferred_slot)
{
    uint8_t found = false;
    uint8_t best_slot = selected_slot;
    uint8_t preferred_row = slot_row(preferred_slot);
    uint8_t preferred_col = slot_col(preferred_slot);
    uint8_t best_distance = 255;

    for (uint8_t i = 0; i < CARD_COUNT; i++) {
        if (slot_is_available(i)) {
            uint8_t distance = (uint8_t)(
                abs_u8_diff(slot_row(i), preferred_row) +
                abs_u8_diff(slot_col(i), preferred_col));
            if (!found || distance < best_distance) {
                found = true;
                best_slot = i;
                best_distance = distance;
            }
        }
    }

    if (!found) {
        return false;
    }

    selected_slot = best_slot;
    needs_redraw = 1;
    return true;
}

static uint8_t select_nearest_available_for_tile(uint8_t tile_x, uint8_t tile_y)
{
    uint8_t found = false;
    uint8_t best_slot = selected_slot;
    uint8_t best_distance = 255;

    for (uint8_t i = 0; i < CARD_COUNT; i++) {
        if (slot_is_available(i)) {
            uint8_t center_x = (uint8_t)(slot_tile_x(i) + 1U);
            uint8_t center_y = (uint8_t)(slot_tile_y(i) + 2U);
            uint8_t distance = (uint8_t)(abs_u8_diff(tile_x, center_x) + abs_u8_diff(tile_y, center_y));
            if (!found || distance < best_distance) {
                found = true;
                best_slot = i;
                best_distance = distance;
            }
        }
    }

    if (!found) {
        return false;
    }

    /*
     * Mouse movement uses the hardware sprite for visible feedback. Keep the
     * logical selected slot in sync for clicks, but do not redraw the full
     * 36-card board just because the hidden snap target changed.
     */
    selected_slot = best_slot;
    return true;
}

static uint8_t find_row_slot_near_col(uint8_t row, uint8_t preferred_col, uint8_t* out_slot)
{
    uint8_t found = false;
    uint8_t best_slot = 0;
    uint8_t best_distance = 255;

    for (uint8_t col = 0; col < CARD_COLS; col++) {
        uint8_t slot = slot_from_row_col(row, col);
        if (slot_is_available(slot)) {
            uint8_t distance = abs_u8_diff(col, preferred_col);
            if (!found || distance < best_distance) {
                found = true;
                best_slot = slot;
                best_distance = distance;
            }
        }
    }

    if (!found) {
        return false;
    }

    *out_slot = best_slot;
    return true;
}

/* Move selection while skipping removed slots. */
static void move_selection(int8_t dx, int8_t dy)
{
    uint8_t row = slot_row(selected_slot);
    uint8_t col = slot_col(selected_slot);
    uint8_t target_slot = selected_slot;

    if (!slot_is_available(selected_slot)) {
        if (select_nearest_available_slot(selected_slot)) {
            sync_mouse_cursor_to_selected_slot();
        }
        return;
    }

    if (dx < 0) {
        while (col > 0U) {
            col--;
            target_slot = slot_from_row_col(row, col);
            if (slot_is_available(target_slot)) {
                selected_slot = target_slot;
                needs_redraw = 1;
                sync_mouse_cursor_to_selected_slot();
                return;
            }
        }
    } else if (dx > 0) {
        while (col < (CARD_COLS - 1U)) {
            col++;
            target_slot = slot_from_row_col(row, col);
            if (slot_is_available(target_slot)) {
                selected_slot = target_slot;
                needs_redraw = 1;
                sync_mouse_cursor_to_selected_slot();
                return;
            }
        }
    } else if (dy < 0) {
        while (row > 0U) {
            row--;
            if (find_row_slot_near_col(row, col, &target_slot)) {
                selected_slot = target_slot;
                needs_redraw = 1;
                sync_mouse_cursor_to_selected_slot();
                return;
            }
        }
    } else if (dy > 0) {
        while (row < (CARD_ROWS - 1U)) {
            row++;
            if (find_row_slot_near_col(row, col, &target_slot)) {
                selected_slot = target_slot;
                needs_redraw = 1;
                sync_mouse_cursor_to_selected_slot();
                return;
            }
        }
    }
}

static void print_error_u16(const char* prefix, uint16_t value)
{
    put_s(prefix);
    put_u16(value);
    put_c('\n');
}

static void load_ui_font_tiles(void)
{
    /*
     * Runtime text rendering for bottom banner:
     * - uppercase letters from yellow alphabet strip
     * - space mapped to transparent clear tile
     * - exclamation used by splash prompt text
     */
    ascii_map(' ', 1, EMPTY_TILE);
    ascii_map('A', 26, 80);
    ascii_map('a', 26, 80);
    ascii_map('!', 1, 78);
    ascii_map(',', 1, EMPTY_TILE);
}

/* Deal a fresh board from the already-shuffled deck and reset turn-scoped state. */
static void redeal_table_from_deck(void)
{
    for (uint8_t i = 0; i < CARD_COUNT; i++) {
        cards[i].card = pop_deck();
        cards[i].revealed = 0;
        cards[i].removed = 0;
    }

    state = STATE_PLAYING;
    selected_slot = 0;
    hand_selector_visible = 1;
    open_count = 0;
    reveals_remaining = MAX_REVEAL_PAIRS_WITHOUT_MATCH;
    couple_found_banner_active = 0;
    banner_text[0] = '\0';
    select_nearest_available_slot(selected_slot);
    sync_mouse_cursor_to_selected_slot();
    mouse_cursor_visible = 0;
    render_mouse_cursor_hide();
    needs_redraw = 1;
}

/* Slot can be revealed only if it exists and still holds a hidden card. */
static uint8_t slot_is_revealable(uint8_t slot)
{
    if (slot >= CARD_COUNT) {
        return false;
    }
    if (cards[slot].removed || cards[slot].revealed) {
        return false;
    }
    return true;
}

/* Hide the currently open pair (used for mismatch reset flow). */
static void hide_open_pair(void)
{
    if (open_count < 2U) {
        return;
    }

    cards[open_slots[0]].revealed = 0;
    cards[open_slots[1]].revealed = 0;
    open_count = 0;
}

/* Remove a matched pair from the board and clear the open-pair tracker. */
static void remove_open_pair(void)
{
    if (open_count < 2U) {
        return;
    }

    cards[open_slots[0]].revealed = 0;
    cards[open_slots[1]].revealed = 0;
    cards[open_slots[0]].removed = 1;
    cards[open_slots[1]].removed = 1;
    open_count = 0;

    if (!slot_is_available(selected_slot)) {
        select_nearest_available_slot(selected_slot);
    }
}

/* Win condition: all board slots must be marked removed. */
static uint8_t all_cards_removed(void)
{
    for (uint8_t i = 0; i < CARD_COUNT; i++) {
        if (!cards[i].removed) {
            return false;
        }
    }
    return true;
}

/* End-state helper used when REVEALS counter reaches zero. */
static void set_game_over_state(void)
{
    state = STATE_GAME_OVER;
    couple_found_banner_active = 0;
    str_cpy(banner_text, "YOU LOOSE, PRESS ENTER!");
    mouse_cursor_visible = 0;
    hand_selector_visible = 0;
    render_mouse_cursor_hide();
    needs_redraw = 1;
}

/* End-state helper used when all pairs are removed. */
static void set_win_state(void)
{
    state = STATE_WIN;
    couple_found_banner_active = 0;
    str_cpy(banner_text, "YOU WIN, PRESS ENTER!");
    mouse_cursor_visible = 0;
    hand_selector_visible = 0;
    render_mouse_cursor_hide();
    needs_redraw = 1;
}

static uint8_t open_pair_is_match(void)
{
    uint8_t left;
    uint8_t right;

    if (open_count < 2U) {
        return false;
    }

    left = open_slots[0];
    right = open_slots[1];
    return (card_rank_for_match(cards[left].card) == card_rank_for_match(cards[right].card)) ?
        true :
        false;
}

/*
 * Resolve the currently open pair before starting a new reveal.
 * Returns true when gameplay may continue revealing another card this ENTER.
 */
static uint8_t resolve_open_pair_before_next_reveal(void)
{
    if (open_count < 2U) {
        return true;
    }

    if (open_pair_is_match()) {
        remove_open_pair();
        reveals_remaining = MAX_REVEAL_PAIRS_WITHOUT_MATCH;

        if (all_cards_removed()) {
            set_win_state();
            return false;
        }

        return true;
    }

    hide_open_pair();
    if (reveals_remaining > 0U) {
        reveals_remaining--;
    }
    if (reveals_remaining == 0U) {
        set_game_over_state();
        return false;
    }

    return true;
}

/* Used to handle the edge case where the final pair is open and no third card exists. */
static uint8_t any_revealable_slots(void)
{
    for (uint8_t i = 0; i < CARD_COUNT; i++) {
        if (slot_is_revealable(i)) {
            return true;
        }
    }
    return false;
}

/*
 * ENTER action pipeline:
 * 1) if two cards are currently open, resolve that pair first
 * 2) reveal selected hidden card (starts/continues next pair cycle)
 */
static void reveal_selected_card(void)
{
    uint8_t selected_revealable;
    uint8_t clear_couple_banner = couple_found_banner_active;

    if (state != STATE_PLAYING) {
        return;
    }

    if (clear_couple_banner) {
        couple_found_banner_active = 0;
        banner_text[0] = '\0';
        needs_redraw = 1;
    }

    selected_revealable = slot_is_revealable(selected_slot);

    /*
     * New requested flow:
     * two open cards are resolved only when player presses ENTER again for the
     * next reveal. We also allow resolution when no revealable slot exists
     * anymore (final pair edge case).
     */
    if (open_count == 2U) {
        if (!selected_revealable && any_revealable_slots()) {
            return;
        }

        if (!resolve_open_pair_before_next_reveal()) {
            needs_redraw = 1;
            return;
        }

        selected_revealable = slot_is_revealable(selected_slot);
    }

    if (!selected_revealable) {
        return;
    }

    cards[selected_slot].revealed = 1;
    open_slots[open_count++] = selected_slot;
    if (open_count == 2U && open_pair_is_match()) {
        str_cpy(banner_text, "COUPLE FOUND!");
        couple_found_banner_active = 1;
    }
    needs_redraw = 1;
}

/* Lightweight mixer used to fold unpredictable key/timing signals into entropy. */
static void mix_entropy_u16(uint16_t value)
{
    entropy ^= value;
    entropy = (uint16_t)(entropy * 109U + 89U);
    entropy ^= (uint16_t)(entropy >> 7);
    entropy ^= (uint16_t)(entropy << 9);

    if (entropy == 0U) {
        entropy = 1U;
    }
}

/* Record one user event into entropy with a small event-type tag. */
static void entropy_event(uint8_t tag)
{
    uint16_t sample = entropy_tick;
    sample ^= (uint16_t)((uint16_t)tag << 8);
    sample ^= (uint16_t)state;
    mix_entropy_u16(sample);
}

/* Seed rand8 once from the mixed entropy pool before the first shuffle. */
static void seed_rng_from_entropy(void)
{
    uint16_t seed = entropy;

    seed ^= (uint16_t)(entropy_tick << 1);

    if (seed == 0U) {
        seed = 1U;
    } else if ((seed & 1U) == 0U) {
        seed++;
    }

    rand8_seed(seed);
    rng_seeded = 1;
}

static int16_t clamp_mouse_pixel(int16_t value, int16_t max_value)
{
    if (value < 0) {
        return 0;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void apply_mouse_motion(const KeyEvents* ev)
{
    uint8_t tile_x;
    uint8_t tile_y;

    if (!ev->mouse_present || (ev->mouse_dx == 0 && ev->mouse_dy == 0)) {
        return;
    }

    mouse_pixel_x = clamp_mouse_pixel(
        (int16_t)(mouse_pixel_x + ev->mouse_dx),
        (int16_t)(SCREEN_PIXEL_W - 16));
    mouse_pixel_y = clamp_mouse_pixel(
        (int16_t)(mouse_pixel_y + ev->mouse_dy),
        (int16_t)(SCREEN_PIXEL_H - 16));

    mouse_cursor_visible = 1;
    mouse_cursor_dirty = 1;
    if (hand_selector_visible) {
        hand_selector_visible = 0;
        needs_redraw = 1;
    }

    tile_x = (uint8_t)((mouse_pixel_x + 8) / 16);
    tile_y = (uint8_t)((mouse_pixel_y + 8) / 16);
    select_nearest_available_for_tile(tile_x, tile_y);
}

/*
 * Game-over flow:
 * - return to the splash screen
 * - after splash Enter/Space, start a fresh shuffled board
 */
static void restart_from_splash_after_game_over(void)
{
    mouse_cursor_visible = 0;
    hand_selector_visible = 0;
    render_mouse_cursor_hide();
    splash_run_blocking();
    seed_rng_from_entropy();
    shuffle_deck();
    redeal_table_from_deck();
    render_table();
    start_game_music();
    mouse_cursor_visible = 0;
    hand_selector_visible = 1;
    render_mouse_cursor_hide();
    needs_redraw = 1;
}

/* Initialize input/video/assets, run splash, then draw gameplay board. */
void init(void)
{
    zos_err_t err = input_events_init();

    if (err != ERR_SUCCESS) {
        print_error_u16("Input init failed: ", err);
        exit(1);
    }

    gfx_enable_screen(0);

    err = gfx_initialize(ZVB_CTRL_VID_MODE_GFX_640_8BIT, &vctx);
    if (err != ERR_SUCCESS) {
        print_error_u16("GFX init failed: ", err);
        exit(1);
    }

    err = load_cards_palette(&vctx);
    if (err != GFX_SUCCESS) {
        print_error_u16("Palette load failed: ", err);
        exit(1);
    }

    err = load_cards_tileset(&vctx);
    if (err != GFX_SUCCESS) {
        print_error_u16("Tileset load failed: ", err);
        exit(1);
    }
    render_mouse_cursor_init();

    audio_init();
    load_ui_font_tiles();

    /* Deterministic fill before first entropy-driven shuffle. */
    for (uint8_t i = 0; i < CARD_COUNT; i++) {
        cards[i].card = i;
        cards[i].revealed = 0;
        cards[i].removed = 0;
    }

    gfx_enable_screen(1);
    splash_run_blocking();

    hand_selector_visible = 1;
    sync_mouse_cursor_to_selected_slot();
    render_table();
    start_game_music();
    needs_redraw = 1;
}

void deinit(void)
{
    audio_deinit();
    /* Restore text console when leaving the game. */
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
}

/* Advance one simulation tick: input, entropy, state transitions, quit path. */
void update(void)
{
    KeyEvents ev;

    tick_current_music();
    entropy_tick++;
    input_poll_events(&ev);

    if (ev.up) { entropy_event(1U); }
    if (ev.down) { entropy_event(2U); }
    if (ev.left) { entropy_event(3U); }
    if (ev.right) { entropy_event(4U); }
    if (ev.enter) { entropy_event(5U); }
    if (ev.space) { entropy_event(6U); }
    if (ev.toggle_music) { entropy_event(7U); }
    if (ev.quit) { entropy_event(8U); }
    if (ev.mouse_dx || ev.mouse_dy) { entropy_event(9U); }
    if (ev.mouse_left || ev.mouse_right) { entropy_event(10U); }

    if (!rng_seeded &&
        (ev.up || ev.down || ev.left || ev.right || ev.enter || ev.space || ev.mouse_left))
    {
        seed_rng_from_entropy();
        shuffle_deck();
        redeal_table_from_deck();
    }

    if (state != STATE_PLAYING) {
        if (ev.quit) {
            deinit();
            exit(0);
        }

        if (ev.enter || ev.space || ev.mouse_left) {
            restart_from_splash_after_game_over();
        }
        return;
    }

    if (ev.toggle_music) {
        audio_toggle_game_audio_mode();
    }

    apply_mouse_motion(&ev);

    if (ev.left) {
        show_keyboard_hand_selector();
        move_selection(-1, 0);
    } else if (ev.right) {
        show_keyboard_hand_selector();
        move_selection(1, 0);
    } else if (ev.up) {
        show_keyboard_hand_selector();
        move_selection(0, -1);
    } else if (ev.down) {
        show_keyboard_hand_selector();
        move_selection(0, 1);
    }

    if (ev.enter || ev.space || ev.mouse_left) {
        reveal_selected_card();
    }

    if (ev.quit) {
        deinit();
        exit(0);
    }
}

/* Draw only when state changed to keep frame work minimal on Z80. */
void draw(void)
{
    if (needs_redraw) {
        render_cards();
        needs_redraw = 0;
    }

    if (mouse_cursor_dirty) {
        if (mouse_cursor_visible && state == STATE_PLAYING) {
            render_mouse_cursor_show((uint16_t)mouse_pixel_x, (uint16_t)mouse_pixel_y);
        } else {
            render_mouse_cursor_hide();
        }
        mouse_cursor_dirty = 0;
    }
}

int main(void)
{
    init();

    /* Classic update->vblank->draw loop for stable tilemap updates. */
    while (1) {
        update();

        gfx_wait_vblank(&vctx);
        draw();
        gfx_wait_end_vblank(&vctx);
    }
}
