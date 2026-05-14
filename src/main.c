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
uint8_t reveals_remaining = MAX_REVEAL_PAIRS_WITHOUT_MATCH;

/* Convert grid coordinates back into the linear [0..CARD_COUNT-1] slot index. */
static uint8_t slot_from_row_col(uint8_t row, uint8_t col)
{
    return (uint8_t)(row * CARD_COLS + col);
}

/* Move selection by one step while clamping to the board bounds. */
static void move_selection(int8_t dx, int8_t dy)
{
    uint8_t row = (uint8_t)(selected_slot / CARD_COLS);
    uint8_t col = (uint8_t)(selected_slot % CARD_COLS);

    if (dx < 0 && col > 0U) {
        col--;
    } else if (dx > 0 && col < (CARD_COLS - 1U)) {
        col++;
    }

    if (dy < 0 && row > 0U) {
        row--;
    } else if (dy > 0 && row < (CARD_ROWS - 1U)) {
        row++;
    }

    selected_slot = slot_from_row_col(row, col);
    needs_redraw = 1;
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
    open_count = 0;
    reveals_remaining = MAX_REVEAL_PAIRS_WITHOUT_MATCH;
    banner_text[0] = '\0';
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
    str_cpy(banner_text, "GAME OVER");
}

/* End-state helper used when all pairs are removed. */
static void set_win_state(void)
{
    state = STATE_WIN;
    str_cpy(banner_text, "YOU WIN!");
}

/*
 * Resolve the currently open pair before starting a new reveal.
 * Returns true when gameplay may continue revealing another card this ENTER.
 */
static uint8_t resolve_open_pair_before_next_reveal(void)
{
    uint8_t left;
    uint8_t right;
    uint8_t left_rank;
    uint8_t right_rank;

    if (open_count < 2U) {
        return true;
    }

    left = open_slots[0];
    right = open_slots[1];
    left_rank = card_rank_for_match(cards[left].card);
    right_rank = card_rank_for_match(cards[right].card);

    if (left_rank == right_rank) {
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

    if (state != STATE_PLAYING) {
        return;
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

/*
 * Game-over flow:
 * - return to the splash screen
 * - after splash Enter/Space, start a fresh shuffled board
 */
static void restart_from_splash_after_game_over(void)
{
    splash_run_blocking();
    seed_rng_from_entropy();
    shuffle_deck();
    redeal_table_from_deck();
    render_table();
    start_game_music();
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

    if (!rng_seeded && (ev.up || ev.down || ev.left || ev.right || ev.enter || ev.space)) {
        seed_rng_from_entropy();
        shuffle_deck();
        redeal_table_from_deck();
    }

    if (state != STATE_PLAYING) {
        if (ev.quit) {
            deinit();
            exit(0);
        }

        if (state == STATE_GAME_OVER) {
            restart_from_splash_after_game_over();
        }
        return;
    }

    if (ev.toggle_music) {
        audio_toggle_game_audio_mode();
    }

    if (ev.left) {
        move_selection(-1, 0);
    } else if (ev.right) {
        move_selection(1, 0);
    } else if (ev.up) {
        move_selection(0, -1);
    } else if (ev.down) {
        move_selection(0, 1);
    }

    if (ev.enter || ev.space) {
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
